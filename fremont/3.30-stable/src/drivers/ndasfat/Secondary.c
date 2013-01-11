#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('sftN')

#if __NDAS_FAT_DBG__

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#endif


#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable


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

	secondary = ExAllocatePoolWithTag( NonPagedPool, sizeof(SECONDARY), NDASFAT_ALLOC_TAG );
	
	if (secondary == NULL) {

		ASSERT( NDASFAT_INSUFFICIENT_RESOURCES );
		return NULL;
	}
	
	RtlZeroMemory( secondary, sizeof(SECONDARY) );

#define MAX_TRY_QUERY 2

	for (tryQuery = 0; tryQuery < MAX_TRY_QUERY; tryQuery++) {

		status = ((PVOLUME_DEVICE_OBJECT) FatData.DiskFileSystemDeviceObject)->
			NdfsCallback.QueryPrimaryAddress( &VolDo->NetdiskPartitionInformation, &secondary->PrimaryAddress, &isLocalAddress );

		DebugTrace2( 0, Dbg2, ("Secondary_Create: QueryPrimaryAddress %08x\n", status) );

		if (NT_SUCCESS(status)) {

			DebugTrace2( 0, Dbg2, ("Secondary_Create: QueryPrimaryAddress: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
				secondary->PrimaryAddress.Node[0], secondary->PrimaryAddress.Node[1],
				secondary->PrimaryAddress.Node[2], secondary->PrimaryAddress.Node[3],
				secondary->PrimaryAddress.Node[4], secondary->PrimaryAddress.Node[5],
				NTOHS(secondary->PrimaryAddress.Port)) );
			break;
		}
	}

	if (status != STATUS_SUCCESS || isLocalAddress) {

		ExFreePoolWithTag( secondary, NDASFAT_ALLOC_TAG );
		return NULL;
	}

	secondary->Flags = SECONDARY_FLAG_INITIALIZING;

#if 0
	ExInitializeResourceLite( &secondary->RecoveryResource );
	ExInitializeResourceLite( &secondary->Resource );
	ExInitializeResourceLite( &secondary->SessionResource );
	ExInitializeResourceLite( &secondary->CreateResource );
#endif

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

	InitializeListHead( &secondary->FcbQueue );
	ExInitializeFastMutex( &secondary->FcbQMutex );

	KeInitializeEvent( &secondary->RecoveryReadyEvent, NotificationEvent, FALSE );

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

		ASSERT( NDASFAT_UNEXPECTED );
		Secondary_Close( secondary );
		
		return NULL;
	}

	status = ObReferenceObjectByHandle( secondary->ThreadHandle,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&secondary->ThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		ASSERT( NDASFAT_INSUFFICIENT_RESOURCES );
		Secondary_Close( secondary );
		
		return NULL;
	}

	secondary->SessionId ++;

	timeOut.QuadPart = -NDASFAT_TIME_OUT;		
	status = KeWaitForSingleObject( &secondary->ReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		NDAS_ASSERT( FALSE );
		Secondary_Close( secondary );
		
		return NULL;
	}

	KeClearEvent( &secondary->ReadyEvent );

	ExAcquireFastMutex( &secondary->FastMutex );

	if (!FlagOn(secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) ||
		FlagOn(secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

		if (secondary->Thread.SessionStatus != STATUS_DISK_CORRUPT_ERROR &&
			secondary->Thread.SessionStatus != STATUS_UNRECOGNIZED_VOLUME) {
	
			ExReleaseFastMutex( &secondary->FastMutex );

			Secondary_Close( secondary );
			return NULL;
		}
	} 

	ASSERT( secondary->Thread.SessionContext.SessionSlotCount != 0 );

	ClearFlag( secondary->Flags, SECONDARY_FLAG_INITIALIZING );
	SetFlag( secondary->Flags, SECONDARY_FLAG_START );

	ExReleaseFastMutex( &secondary->FastMutex );

	DebugTrace2( 0, Dbg2,
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


	DebugTrace2( 0, Dbg2, 
				   ("Secondary close Secondary = %p\n", Secondary) );

	ExAcquireFastMutex( &Secondary->FastMutex );

	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

		//ASSERT( FALSE );
		ExReleaseFastMutex( &Secondary->FastMutex );
		return;
	}

	SetFlag( Secondary->Flags, SECONDARY_FLAG_CLOSED );

	ExReleaseFastMutex( &Secondary->FastMutex );

	if (Secondary->ThreadHandle == NULL) {

		Secondary_Dereference( Secondary );
		return;
	}

	ASSERT( Secondary->ThreadObject != NULL );

	DebugTrace2( 0, Dbg, ("Secondary close SECONDARY_REQ_DISCONNECT Secondary = %p\n", Secondary) );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DISCONNECT;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DOWN;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	DebugTrace2( 0, Dbg, ("Secondary close SECONDARY_REQ_DISCONNECT end Secondary = %p\n", Secondary) );

	timeOut.QuadPart = -NDASFAT_TIME_OUT;

	status = KeWaitForSingleObject( Secondary->ThreadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status == STATUS_SUCCESS) {
	   
		DebugTrace2( 0, Dbg, ("Secondary_Close: thread stoped Secondary = %p\n", Secondary));

		ObDereferenceObject( Secondary->ThreadObject );

		Secondary->ThreadHandle = NULL;
		Secondary->ThreadObject = NULL;
	
	} else {

		ASSERT( NDASFAT_BUG );
		return;
	}

	ASSERT( Secondary->VolDo->Vcb.SecondaryOpenFileCount == 0 );

	ASSERT( IsListEmpty(&Secondary->FcbQueue) );
	ASSERT( IsListEmpty(&Secondary->RecoveryCcbQueue) );
	ASSERT( IsListEmpty(&Secondary->RequestQueue) );

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		PSECONDARY_REQUEST secondaryRequest2;

		InitializeListHead( secondaryRequestEntry );
			
		secondaryRequest2 = CONTAINING_RECORD( secondaryRequestEntry,
											   SECONDARY_REQUEST,
											   ListEntry );
        
		secondaryRequest2->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if (secondaryRequest2->Synchronous == TRUE)
			KeSetEvent( &secondaryRequest2->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( secondaryRequest2 );
	}
	
	Secondary_Dereference( Secondary );

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
	
    result = InterlockedDecrement (&Secondary->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) {

		PVOLUME_DEVICE_OBJECT volDo = Secondary->VolDo;

		ASSERT( Secondary->TryCloseActive == FALSE );

		if (Secondary->TryCloseWorkItem)
			IoFreeWorkItem( Secondary->TryCloseWorkItem );

		NDAS_ASSERT( Secondary->ThreadHandle == NULL );

		ExFreePoolWithTag( Secondary, NDASFAT_ALLOC_TAG );

		DebugTrace2( 0, Dbg2, ("Secondary_Dereference: Secondary is Freed Secondary  = %p\n", Secondary) );
		DebugTrace2( 0, Dbg2, ("Secondary_Dereference: voldo->Reference = %d\n", volDo->ReferenceCount) );

		VolDo_Dereference( volDo );
	}
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

		ASSERT( NDASFAT_INSUFFICIENT_RESOURCES );
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


PSECONDARY_REQUEST
AllocateWinxpSecondaryRequest (
	IN PSECONDARY	Secondary,
	IN UINT8			IrpMajorFunction,
	IN UINT32		DataSize
	)
{
	if (IrpMajorFunction == IRP_MJ_CREATE ||
		IrpMajorFunction == IRP_MJ_WRITE  ||
		IrpMajorFunction == IRP_MJ_SET_INFORMATION) {

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
ReferenceSecondaryRequest (
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

	if (0 == result) {

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

		ExReleaseFastMutex( &Secondary->FastMutex );

		KeSetEvent( &Secondary->RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		ExReleaseFastMutex( &Secondary->FastMutex );

		status = STATUS_UNSUCCESSFUL;
	}

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
SecondaryTryCloseWorkItemRoutine (
	PDEVICE_OBJECT	VolumeDeviceObject,
	PSECONDARY		Secondary
	)
{
	UNREFERENCED_PARAMETER( VolumeDeviceObject );
	SecondaryTryClose( Secondary );
}


VOID
SecondaryTryClose (
	IN PSECONDARY	Secondary
	)
{
	IRP_CONTEXT			irpContext;

	BOOLEAN				secondaryResourceAcquired = FALSE;
	BOOLEAN				acquiredVcb = FALSE;
	BOOLEAN				wait;


	DebugTrace2( 0, Dbg, ("Secondary_TryCloseFiles start\n") );

	try {

		RtlZeroMemory( &irpContext, sizeof(IRP_CONTEXT) );
		SetFlag( irpContext.Flags, IRP_CONTEXT_FLAG_WAIT );
		SetFlag( irpContext.NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
		irpContext.Vcb = &Secondary->VolDo->Vcb;

		secondaryResourceAcquired = SecondaryAcquireResourceSharedLite( &irpContext, 
																		&Secondary->VolDo->Resource, 
																		FALSE );
		if (secondaryResourceAcquired == FALSE)
			leave;
		
		wait = BooleanFlagOn( irpContext.Flags, IRP_CONTEXT_FLAG_WAIT );

		acquiredVcb = FatAcquireExclusiveVcb( &irpContext, irpContext. Vcb );

		if (acquiredVcb == FALSE)
			leave;

		FsRtlEnterFileSystem();
		FatFspClose( &Secondary->VolDo->Vcb );
		FsRtlExitFileSystem();

		DebugTrace2( 0, Dbg, ("Secondary_TryCloseFiles FatFspClose, Secondary->VolDo->Vcb.SecondaryOpenCount = %d\n", 
							    Secondary->VolDo->Vcb.SecondaryOpenFileCount) );

		SetFlag( irpContext.NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES );

		Secondary_TryCloseFilExts( Secondary );

	} finally {

		ClearFlag( irpContext.NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES );

		if (acquiredVcb)
			FatReleaseVcb( &irpContext, irpContext.Vcb );
		
		DebugTrace2( 0, Dbg, ("Secondary_TryCloseFiles exit\n") );

		ExAcquireFastMutex( &Secondary->FastMutex );		
		Secondary->TryCloseActive = FALSE;
		ExReleaseFastMutex( &Secondary->FastMutex );

		if (secondaryResourceAcquired)
			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->Resource );

		Secondary_Dereference( Secondary );
	}

	return;
}


VOID
Secondary_TryCloseFilExts (
	PSECONDARY Secondary
	)
{
	PLIST_ENTRY		listEntry;


	Secondary_Reference( Secondary );

	if (ExTryToAcquireFastMutex(&Secondary->RecoveryCcbQMutex) == FALSE) {

		Secondary_Dereference(Secondary);
		return;
	}
	
	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {

		ExReleaseFastMutex( &Secondary->RecoveryCcbQMutex );
		Secondary_Dereference( Secondary );
		return;
	}

	listEntry = Secondary->RecoveryCcbQueue.Flink;

	while (listEntry != &Secondary->RecoveryCcbQueue) {

		PCCB						ccb;
		PFCB						fcb;

		PSECTION_OBJECT_POINTERS	section;
		BOOLEAN						dataSectionExists;
		BOOLEAN						imageSectionExists;
		IRP_CONTEXT					IrpContext;

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );
		listEntry = listEntry->Flink;

		if (ccb->Fcb == NULL) {

			NDAS_ASSERT( FALSE );
			continue;
		}	

		if (NodeType(ccb->Fcb) != FAT_NTC_FCB && NodeType(ccb->Fcb) != FAT_NTC_DCB && NodeType(ccb->Fcb) != FAT_NTC_ROOT_DCB) {

			continue;
		}

		fcb = ccb->Fcb;

		if (fcb->UncleanCount != 0 || fcb->NonCachedUncleanCount != 0) {

			DebugTrace2( 0, Dbg, ("Secondary_TryCloseFilExts: fcb->FullFileName = %wZ\n", &ccb->Fcb->FullFileName) );
			break;
		}

		DebugTrace2( 0, Dbg, ("Secondary_TryCloseFilExts: fcb->FullFileName = %wZ\n", &ccb->Fcb->FullFileName) );

		RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
		SetFlag( IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT );
		
		section = &fcb->NonPaged->SectionObjectPointers;			
		if (section == NULL)
			break;

		dataSectionExists = (BOOLEAN)(section->DataSectionObject != NULL);
		imageSectionExists = (BOOLEAN)(section->ImageSectionObject != NULL);

		if (imageSectionExists) {

			(VOID)MmFlushImageSection( section, MmFlushForWrite );
		}

		if (dataSectionExists) {

            CcPurgeCacheSection( section, NULL, 0, FALSE );
	    }
	}

	ExReleaseFastMutex( &Secondary->RecoveryCcbQMutex );
	Secondary_Dereference( Secondary );

	return;
}


VOID
INITIALIZE_NDFS_REQUEST_HEADER (
	PNDFS_REQUEST_HEADER	NdfsRequestHeader,
	UINT8						Command,
	PSECONDARY				Secondary,
	UINT8						IrpMajorFunction,
	UINT32					DataSize
	)
{
	ExAcquireFastMutex( &Secondary->FastMutex );		

	RtlCopyMemory(NdfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsRequestHeader->Protocol));
	NdfsRequestHeader->Command	= Command;				
	NdfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;							    
	NdfsRequestHeader->Uid2		= NTOHS(Secondary->Thread.SessionContext.Uid);									
	NdfsRequestHeader->Tid2		= NTOHS(Secondary->Thread.SessionContext.Tid);									
	NdfsRequestHeader->Mid2		= 0;																	    
	NdfsRequestHeader->MessageSize4																			
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
	
	NdfsRequestHeader->MessageSize4 = NTOHL(NdfsRequestHeader->MessageSize4);

	ExReleaseFastMutex( &Secondary->FastMutex );

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
	
	if (IrpContext && IrpContext->MajorFunction != IRP_MJ_PNP)
		ASSERT( Wait == TRUE );

	//if (ARGUMENT_PRESENT(IrpContext)) { 
		
	//	ASSERT( FatIsTopLevelFat(IrpContext) );
	//}

	result = ExAcquireResourceExclusiveLite( Resource, Wait );

	if (IrpContext && IrpContext->MajorFunction != IRP_MJ_PNP)
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

	UNREFERENCED_PARAMETER( IrpContext );

	//if (ARGUMENT_PRESENT(IrpContext)) { 
		
	//	ASSERT( FatIsTopLevelFat(IrpContext) );
	//}

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
		
	//if (ARGUMENT_PRESENT(IrpContext) && !FatIsTopLevelFat(IrpContext)) {
		
	//	ASSERT( IrpContext->MajorFunction == IRP_MJ_CLOSE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CLEANUP		||
	//			IrpContext->MajorFunction == IRP_MJ_CLOSE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CREATE			||
	//			IrpContext->MajorFunction == IRP_MJ_CLOSE && FlagOn(IrpContext->TopLevelIrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES) ||
	//			IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_WRITE			||
	//			IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CREATE			||
	//			IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_CLEANUP		||
	//			IrpContext->MajorFunction == IRP_MJ_WRITE && IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_FLUSH_BUFFERS	||
	//			IrpContext->MajorFunction == IRP_MJ_WRITE && FlagOn(IrpContext->TopLevelIrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_TRY_CLOSE_FILES) );
	//}

	result = ExAcquireSharedStarveExclusive( Resource, Wait );
	
	if (Wait)
		ASSERT( result == TRUE );

	//if (ARGUMENT_PRESENT(IrpContext) && !FatIsTopLevelFat(IrpContext))
	//	ASSERT( result == TRUE );

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


VOID
Secondary_ChangeFcbFileName (
	IN PIRP_CONTEXT		IrpContext,
	IN PFCB				Fcb,
	IN PUNICODE_STRING	FullFileName
	)
{
	UNREFERENCED_PARAMETER( IrpContext );

	RtlInitEmptyUnicodeString( &Fcb->FullFileName,
							   Fcb->FullFileNameBuffer,
							   sizeof(Fcb->FullFileNameBuffer) );

	RtlCopyUnicodeString( &Fcb->FullFileName, FullFileName );

    RtlInitEmptyUnicodeString( &Fcb->CaseInSensitiveFullFileName,
							   Fcb->CaseInSensitiveFullFileNameBuffer,
							   sizeof(Fcb->CaseInSensitiveFullFileNameBuffer) );

	RtlDowncaseUnicodeString( &Fcb->CaseInSensitiveFullFileName,
							  &Fcb->FullFileName,
							  FALSE );

	if (FullFileName->Length && FullFileName->Buffer[0] != L'\\')
		ASSERT( NDASFAT_BUG );

	return;
}


PFCB
Secondary_LookUpFcbByHandle (
	IN PSECONDARY	Secondary,
	IN UINT64			FcbHandle,
	IN BOOLEAN		ExceptDeleteOnClose,
	IN BOOLEAN		ExceptAlreadyDeleted
	)
{
	PFCB			fcb = NULL;
    PLIST_ENTRY		listEntry;


	UNREFERENCED_PARAMETER( ExceptAlreadyDeleted );

	ExAcquireFastMutex( &Secondary->FcbQMutex );

    for (listEntry = Secondary->FcbQueue.Flink;
         listEntry != &Secondary->FcbQueue;
         listEntry = listEntry->Flink) {

		fcb = CONTAINING_RECORD( listEntry, FCB, ListEntry );

#if 0
		if (ExceptAlreadyDeleted == TRUE && fcb->AlreadyDeleted) {

			fcb = NULL;
			continue;
		}
#endif
		if (ExceptDeleteOnClose == TRUE && BooleanFlagOn(fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE) && fcb->UncleanCount == 0) {

			fcb = NULL;
			continue;	
		}

		if (fcb->Handle == FcbHandle)
			break;

		fcb = NULL;
	}

	ExReleaseFastMutex( &Secondary->FcbQMutex );

	return fcb;
}

#endif
