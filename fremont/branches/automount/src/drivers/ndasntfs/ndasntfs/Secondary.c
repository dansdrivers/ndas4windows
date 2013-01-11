#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__


#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('sftN')

#if __NDAS_NTFS_DBG__

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#endif


PSECONDARY
Secondary_Create (
	IN  PIRP_CONTEXT			IrpContext,
	IN	PVOLUME_DEVICE_OBJECT	VolDo		 
	)
{
	NTSTATUS			status;
	PSECONDARY			secondary;

	OBJECT_ATTRIBUTES	objectAttributes;
	LARGE_INTEGER		timeOut;
	ULONG				tryQuery;
	BOOLEAN				isLocalAddress;
	

	UNREFERENCED_PARAMETER( IrpContext );

	secondary = ExAllocatePoolWithTag( NonPagedPool, sizeof(SECONDARY), NDASNTFS_ALLOC_TAG );
	
	if (secondary == NULL) {

		ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}
	
	RtlZeroMemory( secondary, sizeof(SECONDARY) );

#define MAX_TRY_QUERY 2

	for (tryQuery = 0; tryQuery < MAX_TRY_QUERY; tryQuery++) {

		status = ((PVOLUME_DEVICE_OBJECT) NdasNtfsFileSystemDeviceObject)->
			NdfsCallback.QueryPrimaryAddress( &VolDo->NetdiskPartitionInformation, &secondary->PrimaryAddress, &isLocalAddress );

		DebugTrace( 0, Dbg2, ("Secondary_Create: QueryPrimaryAddress %08x\n", status) );

		if (NT_SUCCESS(status)) {

			DebugTrace( 0, Dbg2, ("Secondary_Create: QueryPrimaryAddress: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
				secondary->PrimaryAddress.Node[0], secondary->PrimaryAddress.Node[1],
				secondary->PrimaryAddress.Node[2], secondary->PrimaryAddress.Node[3],
				secondary->PrimaryAddress.Node[4], secondary->PrimaryAddress.Node[5],
				NTOHS(secondary->PrimaryAddress.Port)) );
			break;
		}
	}

	if (status != STATUS_SUCCESS || isLocalAddress) {

		ExFreePoolWithTag( secondary, NDASNTFS_ALLOC_TAG );
		return NULL;
	}

	secondary->Flags = SECONDARY_FLAG_INITIALIZING;

	ExInitializeResourceLite( &VolDo->RecoveryResource );
	ExInitializeResourceLite( &VolDo->Resource );
	ExInitializeResourceLite( &VolDo->SessionResource );
	ExInitializeResourceLite( &VolDo->CreateResource );

	ExInitializeFastMutex( &secondary->FastMutex );

	secondary->ReferenceCount = 1;

	VolDo_Reference( VolDo );
	secondary->VolDo = VolDo;

	secondary->ThreadHandle = NULL;

	InitializeListHead( &secondary->RecoveryCcbQueue );
    ExInitializeFastMutex( &secondary->RecoveryCcbQMutex );

	InitializeListHead( &secondary->DeletedFcbQueue );

	KeQuerySystemTime( &secondary->TryCloseTime );

	secondary->TryCloseWorkItem = IoAllocateWorkItem( (PDEVICE_OBJECT)VolDo );

	KeInitializeEvent( &secondary->ReadyEvent, NotificationEvent, FALSE );
    
	InitializeListHead( &secondary->RequestQueue );
	KeInitializeSpinLock( &secondary->RequestQSpinLock );
	KeInitializeEvent( &secondary->RequestEvent, NotificationEvent, FALSE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	secondary->SessionId = 0;
	
	status = PsCreateSystemThread( &secondary->ThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   SecondaryThreadProc,
								   secondary );

	if (!NT_SUCCESS(status)) {

		ASSERT( NDASNTFS_UNEXPECTED );
		Secondary_Close( secondary );
		
		return NULL;
	}

	status = ObReferenceObjectByHandle( secondary->ThreadHandle,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&secondary->ThreadObject,
										NULL );

	if(!NT_SUCCESS(status)) {

		ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
		Secondary_Close( secondary );
		
		return NULL;
	}

	secondary->SessionId ++;

	timeOut.QuadPart = -NDASNTFS_TIME_OUT;
	status = KeWaitForSingleObject( &secondary->ReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		ASSERT( NDASNTFS_BUG );
		Secondary_Close( secondary );
		
		return NULL;
	}

	KeClearEvent( &secondary->ReadyEvent );

	ExAcquireFastMutex( &secondary->FastMutex );

	if (!FlagOn(secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) ||
		FlagOn(secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

		ExReleaseFastMutex( &secondary->FastMutex );

		if (secondary->Thread.SessionStatus != STATUS_DISK_CORRUPT_ERROR) {
	
			Secondary_Close( secondary );
			return NULL;
		}
	} 

	ASSERT( secondary->Thread.SessionContext.SessionSlotCount != 0 );

	ClearFlag( secondary->Flags, SECONDARY_FLAG_INITIALIZING );
	SetFlag( secondary->Flags, SECONDARY_FLAG_START );

	ExReleaseFastMutex( &secondary->FastMutex );

	DebugTrace( 0, Dbg2,
				("Secondary_Create: The client thread are ready secondary = %p\n", secondary) );

	return secondary;
}


VOID
Secondary_Close (
	IN  PSECONDARY	Secondary
	)
{
	NTSTATUS		status;
	LARGE_INTEGER	timeOut;

	PLIST_ENTRY			secondaryRequestEntry;
	PSECONDARY_REQUEST	secondaryRequest;


	DebugTrace( 0, Dbg2, ("Secondary close\n") );

	ExAcquireFastMutex( &Secondary->FastMutex );

	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

		ASSERT( FALSE );
		ExReleaseFastMutex( &Secondary->FastMutex );
		return;
	}

	SetFlag( Secondary->Flags, SECONDARY_FLAG_CLOSED );

	ExReleaseFastMutex( &Secondary->FastMutex );

	if(Secondary->ThreadHandle == NULL) {

		Secondary_Dereference(Secondary);
		return;
	}

	ASSERT( Secondary->ThreadObject != NULL );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DISCONNECT;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DOWN;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	timeOut.QuadPart = - NDASNTFS_TIME_OUT;

	status = KeWaitForSingleObject( Secondary->ThreadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status == STATUS_SUCCESS) {
	   
		DebugTrace( 0, Dbg, ("Secondary_Close: thread stoped Secondary = %p\n", Secondary));

		ObDereferenceObject( Secondary->ThreadObject );

		Secondary->ThreadHandle = NULL;
		Secondary->ThreadObject = NULL;
	
	} else {

		ASSERT( NDASNTFS_BUG );
		return;
	}

	ASSERT( Secondary->VolDo->Vcb.SecondaryCloseCount == 0 );

	ASSERT( IsListEmpty(&Secondary->RecoveryCcbQueue) );
	ASSERT( IsListEmpty(&Secondary->RequestQueue) );

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		PSECONDARY_REQUEST secondaryRequest;
			
		secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry,
											  SECONDARY_REQUEST,
											  ListEntry );
        
		secondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if(secondaryRequest->Synchronous == TRUE)
			KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( secondaryRequest );
	}
	
	Secondary_Dereference( Secondary );

	return;
}


VOID
Secondary_Stop (
	IN  PSECONDARY	Secondary
	)
{
	NTSTATUS		status;
	LARGE_INTEGER	timeOut;

	PLIST_ENTRY			secondaryRequestEntry;
	PSECONDARY_REQUEST	secondaryRequest;


	DebugTrace( 0, Dbg2, ("Secondary stop\n") );

	ExAcquireFastMutex( &Secondary->FastMutex );

	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

		ASSERT( FALSE );
		ExReleaseFastMutex( &Secondary->FastMutex );
		return;
	}

	SetFlag( Secondary->Flags, SECONDARY_FLAG_CLOSED );

	ExReleaseFastMutex( &Secondary->FastMutex );

	if(Secondary->ThreadHandle == NULL) {

		Secondary_Dereference(Secondary);
		return;
	}

	ASSERT( Secondary->ThreadObject != NULL );


	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DISCONNECT;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DOWN;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	timeOut.QuadPart = - NDASNTFS_TIME_OUT;

	status = KeWaitForSingleObject( Secondary->ThreadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status == STATUS_SUCCESS) {
	   
		DebugTrace( 0, Dbg, ("Secondary_Stop: thread stoped Secondary = %p\n", Secondary));

		ObDereferenceObject( Secondary->ThreadObject );

		Secondary->ThreadHandle = NULL;
		Secondary->ThreadObject = NULL;
	
	} else {

		ASSERT( NDASNTFS_BUG );
		return;
	}

	ASSERT( IsListEmpty(&Secondary->RecoveryCcbQueue) );
	ASSERT( IsListEmpty(&Secondary->RequestQueue) );

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		PSECONDARY_REQUEST secondaryRequest;
			
		secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry,
											  SECONDARY_REQUEST,
											  ListEntry );
        
		secondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if(secondaryRequest->Synchronous == TRUE)
			KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( secondaryRequest );
	}
	
	return;
}


VOID
Secondary_Reference (
	IN  PSECONDARY	Secondary
	)
{
    LONG result;
	
    result = InterlockedIncrement ( &Secondary->ReferenceCount );

    ASSERT (result >= 0);
}


VOID
Secondary_Dereference (
	IN  PSECONDARY	Secondary
	)
{
    LONG result;
	
    result = InterlockedDecrement(&Secondary->ReferenceCount);
    ASSERT ( result >= 0 );

    if (result == 0) {

		PVOLUME_DEVICE_OBJECT volDo = Secondary->VolDo;

#if __NDAS_NTFS_DBG__
		
		BOOLEAN acquired;

		acquired = SecondaryAcquireResourceExclusiveLite( NULL, &volDo->RecoveryResource, TRUE );
		if (acquired)
			SecondaryReleaseResourceLite( NULL, &volDo->RecoveryResource );
		else
			ASSERT( FALSE );

		acquired = SecondaryAcquireResourceExclusiveLite( NULL, &volDo->Resource, TRUE );
		if (acquired)
			SecondaryReleaseResourceLite( NULL, &volDo->Resource );
		else
			ASSERT( FALSE );

		acquired = SecondaryAcquireResourceExclusiveLite( NULL, &volDo->CreateResource, TRUE );
		if (acquired)
			SecondaryReleaseResourceLite( NULL, &volDo->CreateResource );
		else
			ASSERT( FALSE );

		acquired = SecondaryAcquireResourceExclusiveLite( NULL, &volDo->SessionResource, TRUE );
		if (acquired)
			SecondaryReleaseResourceLite( NULL, &volDo->SessionResource );
		else
			ASSERT( FALSE );

#endif
		ASSERT( Secondary->TryCloseActive == FALSE );

		if (Secondary->TryCloseWorkItem)
			IoFreeWorkItem( Secondary->TryCloseWorkItem );

		ExDeleteResourceLite( &volDo->RecoveryResource );
		ExDeleteResourceLite( &volDo->Resource );
		ExDeleteResourceLite( &volDo->CreateResource );
		ExDeleteResourceLite( &volDo->SessionResource );

		ExFreePoolWithTag( Secondary, NDASNTFS_ALLOC_TAG );

		DebugTrace( 0, Dbg2, ("Secondary_Dereference: Secondary is Freed Secondary  = %p\n", Secondary) );
		DebugTrace( 0, Dbg2, ("Secondary_Dereference: voldo->Reference = %d\n", volDo->ReferenceCount) );

		VolDo_Dereference( volDo );
	}
}


VOID
SecondaryTryCloseWorkItemRoutine (
	PDEVICE_OBJECT	VolumeDeviceObject,
	PSECONDARY		Secondary
	)
{
	UNREFERENCED_PARAMETER( VolumeDeviceObject );
	SecondaryTryClose( NULL, Secondary );
}


VOID
SecondaryTryClose (
	IN PIRP_CONTEXT	*IrpContext OPTIONAL,
	IN PSECONDARY	Secondary
	)
{
	PIRP_CONTEXT		localIrpContext = NULL;
	TOP_LEVEL_CONTEXT	topLevelContext;
	PTOP_LEVEL_CONTEXT	threadTopLevelContext;

	BOOLEAN				secondaryResourceAcquired = FALSE;
	BOOLEAN				acquiredVcb = FALSE;
	BOOLEAN				wait;

	PFCB				fcb;
	BOOLEAN				acquiredFcb = FALSE;
	BOOLEAN				removedFcb;
	PFCB				nextFcb = NULL;
	PSCB				scb;
	PVOID				restartKey;
	PLIST_ENTRY			deletedFcbListEntry;


	DebugTrace( 0, Dbg, ("Secondary_TryCloseFiles start\n") );

	//if (!ARGUMENT_PRESENT(IrpContext)) {

		IrpContext = &localIrpContext;
	
		threadTopLevelContext = NtfsInitializeTopLevelIrp( &topLevelContext, TRUE, FALSE );
		ASSERT( threadTopLevelContext == &topLevelContext );
	//}

	try {

		secondaryResourceAcquired = SecondaryAcquireResourceSharedLite( (*IrpContext), 
																		&Secondary->VolDo->Resource, 
																		FALSE );
		if (secondaryResourceAcquired == FALSE)
			leave;
		
		if (IrpContext == &localIrpContext) {

			NtfsInitializeIrpContext( NULL, FALSE, &(*IrpContext) );
			SetFlag( (*IrpContext)->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE );
			(*IrpContext)->Vcb = &Secondary->VolDo->Vcb;
			NtfsUpdateIrpContextWithTopLevel( (*IrpContext), threadTopLevelContext );
		}

		wait = BooleanFlagOn( (*IrpContext)->State, IRP_CONTEXT_STATE_WAIT );

		//ClearFlag( (*IrpContext)->State, IRP_CONTEXT_STATE_WAIT );

		acquiredVcb = NtfsAcquireExclusiveVcb( (*IrpContext), (*IrpContext)->Vcb, FALSE );

		//if (wait)
		//	SetFlag( (*IrpContext)->State, IRP_CONTEXT_STATE_WAIT );

		if (acquiredVcb == FALSE)
			leave;

		SetFlag( Secondary->VolDo->Vcb.NdNtfsFlags, ND_NTFS_VCB_FLAG_TRY_CLOSE_FILES );
		NtfsFspClose( &Secondary->VolDo->Vcb );
		ClearFlag( Secondary->VolDo->Vcb.NdNtfsFlags, ND_NTFS_VCB_FLAG_TRY_CLOSE_FILES );

		restartKey = NULL;
		deletedFcbListEntry = NULL;

		SetFlag( (*IrpContext)->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES );

		do {

			fcb = nextFcb;
			NtfsAcquireFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );

			if (deletedFcbListEntry == NULL) {
			
				nextFcb = NdNtfsGetNextFcbTableEntry( &Secondary->VolDo->Vcb, &restartKey );
			}

			//if (nextFcb == NULL) {

			//	deletedFcbListEntry = Secondary->DeletedFcbQueue.Flink;
			//}
            
			//if (deletedFcbListEntry && deletedFcbListEntry == &Secondary->DeletedFcbQueue) {

			//	nextFcb = NULL;
				
			//} else {

			//	nextFcb = CONTAINING_RECORD( deletedFcbListEntry, FCB, DeletedListEntry );
			//	ASSERT( &nextFcb->DeletedListEntry == deletedFcbListEntry );
			//	deletedFcbListEntry = deletedFcbListEntry->Flink;
			//	if (fcb && deletedFcbListEntry == &fcb->DeletedListEntry)
			//		deletedFcbListEntry = deletedListEntry->Flink;
			//}

			if ((fcb == NULL) && (nextFcb == NULL)  && IsListEmpty(&Secondary->DeletedFcbQueue)) {
			
				ASSERT( Secondary->VolDo->Vcb.SecondaryCloseCount == 0 );
				ASSERT( IsListEmpty(&Secondary->RecoveryCcbQueue) );
			}

			if (nextFcb != NULL) {

				nextFcb->ReferenceCount += 1;
			}

			if (fcb == NULL) {

				NtfsReleaseFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );
				continue;
			}

			ASSERT_FCB( fcb );
			ASSERT( FlagOn(fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY) );
			
			if (fcb->CloseCount == 0) {

				DebugTrace( 0, Dbg2, ("fcb = %p\n", fcb) );
				ASSERT( FALSE );
			}

			if (1) { //!FlagOn(fcb->FcbState, FCB_STATE_SYSTEM_FILE)) {
			
				try {
				
					acquiredFcb = NtfsAcquireExclusiveFcb( (*IrpContext), fcb,  NULL, ACQUIRE_DONT_WAIT );
				
				} except(EXCEPTION_EXECUTE_HANDLER) {

					ASSERT( FALSE );
				}

			} else
				acquiredFcb = FALSE;

		    fcb->ReferenceCount -= 1;
			NtfsReleaseFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );

			if( !acquiredFcb )
				continue;

			InterlockedIncrement( &fcb->CloseCount );

			scb = NULL;
        
			while (scb = NtfsGetNextChildScb( fcb, scb )) {

				ASSERT_SCB( scb );

				if (scb->AttributeTypeCode != $DATA)
					continue;

				if (scb->CleanupCount != 0)
					continue;
	
				ASSERT( scb->CloseCount );
				ASSERT( !IsListEmpty(&scb->CcbQueue) );

				//DebugTrace( 0, Dbg, ("Secondary_TryCloseFilExts: scb->FullPathName = %wZ\n", &scb->FullPathName));

				InterlockedIncrement( &scb->CloseCount );

				if (scb->CorruptedCcbCloseCount == (scb->CloseCount-1)) {

					CcPurgeCacheSection( &scb->NonpagedScb->SegmentObject, NULL, 0, FALSE );
				
				} else {
				
					try {
#if 0
						if (scb->NonpagedScb->SegmentObject.DataSectionObject) {

							BOOLEAN			result;

							result = MmFlushImageSection( &scb->NonpagedScb->SegmentObject, MmFlushForWrite );
						
							if (result == TRUE && scb->NonpagedScb->SegmentObject.ImageSectionObject) {

								CcPurgeCacheSection( &scb->NonpagedScb->SegmentObject, NULL, 0, FALSE );
							}
						}
#else
						IO_STATUS_BLOCK ioStatus;

						CcFlushCache( &scb->NonpagedScb->SegmentObject, NULL, 0, &ioStatus );

						if (ioStatus.Status == STATUS_SUCCESS) {
	
							CcPurgeCacheSection( &scb->NonpagedScb->SegmentObject, NULL, 0, FALSE );
						
						} else {
						
							LONG		ccbCount;
							PLIST_ENTRY	ccbListEntry;

							for (ccbCount = 0, ccbListEntry = scb->CcbQueue.Flink; 
								 ccbListEntry != &scb->CcbQueue; 
								 ccbListEntry = ccbListEntry->Flink, ccbCount++);

							DebugTrace( 0, Dbg2, ("SecondaryTryClose CcFlushCache = %X, ccbCount = %d, scb->CorruptedCcbCloseCount = %d scb->CloseCount = %d\n", 
												  ioStatus.Status, ccbCount, scb->CorruptedCcbCloseCount, scb->CloseCount) );
						}
#endif				
					} except(EXCEPTION_EXECUTE_HANDLER) {

						ASSERT( FALSE );
					}
				}

				InterlockedDecrement( &scb->CloseCount );
			}
			
			InterlockedDecrement( &fcb->CloseCount );

			if (fcb->CloseCount == 0) {
			
				ASSERT( fcb->ReferenceCount == 0 );

				NtfsAcquireFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );
				RemoveEntryList( &fcb->DeletedListEntry );
				NtfsReleaseFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );
					
				InitializeListHead( &fcb->DeletedListEntry );

				NtfsTeardownStructures( (*IrpContext),
										fcb,
										NULL,
										FALSE,
										0,
										&removedFcb );

				ASSERT( removedFcb );
			}
			else 
				removedFcb = FALSE;

			if (!removedFcb) {

				NtfsReleaseFcb( (*IrpContext), fcb );
			}

			acquiredFcb = FALSE;
	
		} while (nextFcb != NULL);


		NtfsAcquireFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );

		for (deletedFcbListEntry = Secondary->DeletedFcbQueue.Flink;
			 deletedFcbListEntry != &Secondary->DeletedFcbQueue;
			 deletedFcbListEntry = deletedFcbListEntry->Flink);

		deletedFcbListEntry = Secondary->DeletedFcbQueue.Flink;
		NtfsReleaseFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );

		while (deletedFcbListEntry != &Secondary->DeletedFcbQueue) {

			fcb = CONTAINING_RECORD( deletedFcbListEntry, FCB, DeletedListEntry );
			ASSERT( &fcb->DeletedListEntry == deletedFcbListEntry );
        
			NtfsAcquireFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );
			deletedFcbListEntry = deletedFcbListEntry->Flink;
			NtfsReleaseFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );

			ASSERT_FCB( fcb );
			ASSERT( FlagOn(fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY) );
			ASSERT( fcb->CloseCount );

			if (1) { //!FlagOn(fcb->FcbState, FCB_STATE_SYSTEM_FILE)) {
			
				try {
				
					acquiredFcb = NtfsAcquireExclusiveFcb( (*IrpContext), fcb,  NULL, ACQUIRE_DONT_WAIT|ACQUIRE_NO_DELETE_CHECK );
				
				} except(EXCEPTION_EXECUTE_HANDLER) {

					ASSERT( FALSE );
				}

			} else
				acquiredFcb = FALSE;

			if( !acquiredFcb )
				continue;

			InterlockedIncrement( &fcb->CloseCount );

			scb = NULL;
        
			while (scb = NtfsGetNextChildScb( fcb, scb )) {

				ASSERT_SCB( scb );

				if (scb->AttributeTypeCode != $DATA)
					continue;

				if (scb->CleanupCount != 0)
					continue;
	
				ASSERT( scb->CloseCount );
				ASSERT( !IsListEmpty(&scb->CcbQueue) );

				//DebugTrace( 0, Dbg, ("Secondary_TryCloseFilExts: scb->FullPathName = %wZ\n", &scb->FullPathName));

				InterlockedIncrement( &scb->CloseCount );

				if (scb->CorruptedCcbCloseCount == (scb->CloseCount-1)) {

					CcPurgeCacheSection( &scb->NonpagedScb->SegmentObject, NULL, 0, FALSE );
				
				} else {
				
					try {

						IO_STATUS_BLOCK IoStatus;
	
						CcFlushCache( &scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );

						if (IoStatus.Status == STATUS_SUCCESS) {
	
							CcPurgeCacheSection( &scb->NonpagedScb->SegmentObject, NULL, 0, FALSE );
						
						} else {
						
							LONG		ccbCount;
							PLIST_ENTRY	ccbListEntry;

							for (ccbCount = 0, ccbListEntry = scb->CcbQueue.Flink; 
								 ccbListEntry != &scb->CcbQueue; 
								 ccbListEntry = ccbListEntry->Flink, ccbCount++);

							DebugTrace( 0, Dbg2, ("SecondaryTryClose CcFlushCache = %X, ccbCount = %d, scb->CorruptedCcbCloseCount = %d scb->CloseCount = %d\n", 
												  IoStatus.Status, ccbCount, scb->CorruptedCcbCloseCount, scb->CloseCount) );
						}
				
					} except(EXCEPTION_EXECUTE_HANDLER) {

						DebugTrace( 0, Dbg2, ("Execption occured\n") );
						DbgBreakPoint();
					}
				}

				InterlockedDecrement( &scb->CloseCount );
			}
			
			InterlockedDecrement( &fcb->CloseCount );

			if (fcb->CloseCount == 0) {
			
				ASSERT( fcb->ReferenceCount == 0 );

				NtfsAcquireFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );
				ASSERT( deletedFcbListEntry != &fcb->DeletedListEntry );
				RemoveEntryList( &fcb->DeletedListEntry );
				NtfsReleaseFcbTable( (*IrpContext), &Secondary->VolDo->Vcb );
					
				InitializeListHead( &fcb->DeletedListEntry );

				NtfsTeardownStructures( (*IrpContext),
										fcb,
										NULL,
										FALSE,
										0,
										&removedFcb );

				ASSERT( removedFcb );
			}
			else 
				removedFcb = FALSE;

			if (!removedFcb) {

				NtfsReleaseFcb( (*IrpContext), fcb );
			}

			acquiredFcb = FALSE;
		}

	} finally {

		ASSERT( nextFcb == NULL );

		ClearFlag( (*IrpContext)->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES );

		if (acquiredVcb)
			NtfsReleaseVcb( (*IrpContext), (*IrpContext)->Vcb );
		
		if (IrpContext == &localIrpContext)
			NtfsCompleteRequest( (*IrpContext), NULL, 0 );

		DebugTrace( 0, Dbg, ("Secondary_TryCloseFiles exit\n") );

		ExAcquireFastMutex( &Secondary->FastMutex );		
		Secondary->TryCloseActive = FALSE;
		ExReleaseFastMutex( &Secondary->FastMutex );

		if (secondaryResourceAcquired)
			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->Resource );

		Secondary_Dereference( Secondary );
	}

	return;
}


PFCB
NdNtfsGetNextFcbTableEntry (
    IN PVCB		Vcb,
    IN PVOID	*RestartKey
    )
{
    PFCB Fcb;

    PAGED_CODE();

    Fcb = (PFCB) RtlEnumerateGenericTableWithoutSplaying( &Vcb->SecondaryFcbTable, RestartKey );

    if (Fcb != NULL) {

        Fcb = ((PFCB_TABLE_ELEMENT)(Fcb))->Fcb;
    }

    return Fcb;
}


PSECONDARY_REQUEST
ALLOC_WINXP_SECONDARY_REQUEST (
	IN PSECONDARY	Secondary,
	IN _U8			IrpMajorFunction,
	IN UINT32		DataSize
	)
{

	if (IrpMajorFunction == IRP_MJ_CREATE				||
		IrpMajorFunction == IRP_MJ_WRITE				||
		IrpMajorFunction == IRP_MJ_SET_INFORMATION		||
		IrpMajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

		DataSize = (DataSize > Secondary->Thread.SessionContext.SecondaryMaxDataSize) ?
					DataSize : Secondary->Thread.SessionContext.SecondaryMaxDataSize;
	} 

	return AllocSecondaryRequest( Secondary,
								  (((sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER)) > (sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER))) ?
								  (sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER)) : (sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER)))
								  + DataSize,
								  TRUE );
}


VOID
INITIALIZE_NDFS_REQUEST_HEADER (
	PNDFS_REQUEST_HEADER	NdfsRequestHeader,
	_U8						Command,
	PSECONDARY				Secondary,
	_U8						IrpMajorFunction,
	_U32					DataSize
	)
{
	ExAcquireFastMutex( &Secondary->FastMutex );		

	RtlCopyMemory(NdfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsRequestHeader->Protocol));
	NdfsRequestHeader->Command	= Command;				
	NdfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;							    
	NdfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;									
	NdfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;									
	NdfsRequestHeader->Mid		= 0;																	    
	NdfsRequestHeader->MessageSize																			
		= sizeof(NDFS_REQUEST_HEADER)																			
			+ (																									
				(Secondary->Thread.SessionContext.MessageSecurity == 0)										
	 			 ? sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize											
				 : (																							
					 ((IrpMajorFunction == IRP_MJ_WRITE														
						&& Secondary->Thread.SessionContext.RwDataSecurity == 0								
						&& DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)				
					 ||																							
					 IrpMajorFunction == IRP_MJ_READ																
						&& Secondary->Thread.SessionContext.RwDataSecurity == 0								
						&& DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize)

					? ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)							
					: ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)				
				   )																							
				);																								
	
	ExReleaseFastMutex( &Secondary->FastMutex );

	return;
}


PSECONDARY_REQUEST
AllocSecondaryRequest (
	IN	PSECONDARY	Secondary,
	IN 	UINT32		MessageSize,
	IN	BOOLEAN		Synchronous
	) 
{
	PSECONDARY_REQUEST	secondaryRequest;
	ULONG				allocationSize;
	
	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	allocationSize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + MessageSize;

	secondaryRequest = ExAllocatePoolWithTag( NonPagedPool,
											  allocationSize,
											  SECONDARY_MESSAGE_TAG );

	if (secondaryRequest == NULL) {

		ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( secondaryRequest, allocationSize );

	secondaryRequest->ReferenceCount = 1;
	InitializeListHead( &secondaryRequest->ListEntry );

	secondaryRequest->Synchronous = Synchronous;
	KeInitializeEvent( &secondaryRequest->CompleteEvent, NotificationEvent, FALSE );

	ExAcquireFastMutex( &Secondary->FastMutex );
	secondaryRequest->SessionId = Secondary->SessionId;
	ExReleaseFastMutex( &Secondary->FastMutex );

	secondaryRequest->NdfsMessageAllocationSize = MessageSize;

	return secondaryRequest;
}


VOID
ReferenceSencondaryRequest (
	IN	PSECONDARY_REQUEST	SecondaryRequest
	) 
{
	LONG	result;

	result = InterlockedIncrement( &SecondaryRequest->ReferenceCount );

	ASSERT( result > 0 );
}


VOID
DereferenceSecondaryRequest (
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	LONG	result;


	result = InterlockedDecrement(&SecondaryRequest->ReferenceCount);

	ASSERT( result >= 0 );

	if(0 == result) {

		ASSERT( SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink );
		ExFreePool( SecondaryRequest );
	}
}


FORCEINLINE
NTSTATUS
QueueingSecondaryRequest (
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	)
{
	NTSTATUS	status;


	ASSERT( SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink );

	ExAcquireFastMutex( &Secondary->FastMutex );

	if (FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) &&
		!FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

		ExInterlockedInsertTailList( &Secondary->RequestQueue,
									 &SecondaryRequest->ListEntry,
									 &Secondary->RequestQSpinLock );

		KeSetEvent( &Secondary->RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		status = STATUS_UNSUCCESSFUL;
	}

	ExReleaseFastMutex( &Secondary->FastMutex );

	if (status == STATUS_UNSUCCESSFUL) {
	
		SecondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;

		if (SecondaryRequest->Synchronous == TRUE)
			KeSetEvent( &SecondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( SecondaryRequest );
	}

	return status;
}


VOID
Secondary_ChangeLcbFileName (
	IN PIRP_CONTEXT		IrpContext,
	IN PLCB				Lcb,
	IN PUNICODE_STRING	FullPathName
	)
{
        UCHAR MaxNameLength;
		UCHAR FileNameFlags = Lcb->FileNameAttr->Flags;


		DebugTrace( 0, Dbg2, ("Secondary_ChangeLcbFileName: FullPathName =%Z\n", FullPathName) );

	 	NtfsUpcaseName( IrpContext->Vcb->UpcaseTable,
		                IrpContext->Vcb->UpcaseTableSize,
			            FullPathName );
       
		if (Lcb->FileNameAttr != (PFILE_NAME) &Lcb->ParentDirectory) {
			
            NtfsFreePool( Lcb->FileNameAttr );
        }

        if (FlagOn( Lcb->Fcb->FcbState, FCB_STATE_COMPOUND_DATA) &&
            ((PLCB)(((PFCB_DATA) Lcb->Fcb)->Lcb) == Lcb)) {

            MaxNameLength = MAX_DATA_FILE_NAME;

        } else if (FlagOn( Lcb->Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ) &&
            ((PLCB)(((PFCB_INDEX) Lcb->Fcb)->Lcb) == Lcb)) {

            MaxNameLength = MAX_INDEX_FILE_NAME;

        } else {

            MaxNameLength = 0;
        }

        if (MaxNameLength < (USHORT) (FullPathName->Length / sizeof( WCHAR ))) {

			Lcb->FileNameAttr = NtfsAllocatePool(PagedPool, FullPathName->Length +
												 NtfsFileNameSizeFromLength( FullPathName->Length ));

			MaxNameLength = (UCHAR)(FullPathName->Length / sizeof( WCHAR ));

        } else {

            Lcb->FileNameAttr = (PFILE_NAME) &Lcb->ParentDirectory;
        }

        Lcb->FileNameAttr->FileNameLength = (UCHAR) (FullPathName->Length / sizeof( WCHAR ));
        Lcb->FileNameAttr->Flags = FileNameFlags;

        Lcb->ExactCaseLink.LinkName.Buffer = (PWCHAR) &Lcb->FileNameAttr->FileName;

        Lcb->IgnoreCaseLink.LinkName.Buffer = Add2Ptr( Lcb->FileNameAttr,
                                                       NtfsFileNameSizeFromLength( MaxNameLength * sizeof( WCHAR )));

        Lcb->ExactCaseLink.LinkName.Length =
        Lcb->IgnoreCaseLink.LinkName.Length = FullPathName->Length;

        Lcb->ExactCaseLink.LinkName.MaximumLength =
        Lcb->IgnoreCaseLink.LinkName.MaximumLength = MaxNameLength * sizeof( WCHAR );

        RtlCopyMemory( Lcb->ExactCaseLink.LinkName.Buffer,
                       FullPathName->Buffer,
                       FullPathName->Length );

        RtlCopyMemory( Lcb->IgnoreCaseLink.LinkName.Buffer,
                       FullPathName->Buffer,
                       FullPathName->Length );

        NtfsUpcaseName( IrpContext->Vcb->UpcaseTable,
                        IrpContext->Vcb->UpcaseTableSize,
                        &Lcb->IgnoreCaseLink.LinkName );
	return;
}


BOOLEAN
SecondaryAcquireResourceExclusiveLite (
	IN PIRP_CONTEXT	IrpContext OPTIONAL,
	IN PERESOURCE	Resource,
    IN BOOLEAN		Wait
    )
{
	BOOLEAN result;
	
	if (!(IrpContext && IrpContext->MajorFunction == IRP_MJ_PNP && IrpContext->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE))
		ASSERT( Wait == TRUE );

	if (ARGUMENT_PRESENT(IrpContext)) { 
		
		ASSERT( NtfsIsTopLevelNtfs(IrpContext) );
	}

	result = ExAcquireResourceExclusiveLite( Resource, Wait );

	if (!(IrpContext && IrpContext->MajorFunction == IRP_MJ_PNP && IrpContext->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE))
		ASSERT( result == TRUE );

	return result;
}


BOOLEAN
SecondaryAcquireResourceSharedLite (
	IN PIRP_CONTEXT	IrpContext OPTIONAL,
	IN PERESOURCE	Resource,
    IN BOOLEAN		Wait
    )
{
	BOOLEAN result;


	if (ARGUMENT_PRESENT(IrpContext)) { 
		
		ASSERT( NtfsIsTopLevelNtfs(IrpContext) );
	}

	result = ExAcquireResourceSharedLite( Resource, Wait );
	
	if (Wait)
		ASSERT( result == TRUE );

	return result;
}


BOOLEAN
SecondaryAcquireResourceSharedStarveExclusiveLite (
	IN PIRP_CONTEXT	IrpContext OPTIONAL,
	IN PERESOURCE	Resource,
    IN BOOLEAN		Wait
    )
{
	BOOLEAN result;

	UNREFERENCED_PARAMETER( IrpContext );
		
	if (ARGUMENT_PRESENT(IrpContext) && !NtfsIsTopLevelNtfs(IrpContext)) {
		
		ASSERT( IrpContext->MajorFunction == IRP_MJ_CLOSE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CLEANUP		||
				IrpContext->MajorFunction == IRP_MJ_CLOSE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CREATE			||
				IrpContext->MajorFunction == IRP_MJ_CLOSE && FlagOn(IrpContext->TopLevelIrpContext->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES) ||
				IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_WRITE			||
				IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CREATE			||
				IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CLEANUP		||
				IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_FLUSH_BUFFERS	||
				IrpContext->MajorFunction == IRP_MJ_WRITE && FlagOn(IrpContext->TopLevelIrpContext->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES) );
	}

	result = ExAcquireSharedStarveExclusive( Resource, Wait );
	
	if (Wait)
		ASSERT( result == TRUE );

	if (ARGUMENT_PRESENT(IrpContext) && !NtfsIsTopLevelNtfs(IrpContext))
		ASSERT( result == TRUE );

	return result;
}


VOID
SecondaryReleaseResourceLite (
	IN PIRP_CONTEXT	IrpContext OPTIONAL,
    IN PERESOURCE	Resource
    )
{
	UNREFERENCED_PARAMETER( IrpContext );

	ExReleaseResourceLite( Resource );
}


#endif
