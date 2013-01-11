#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__


extern ATTRIBUTE_DEFINITION_COLUMNS NtfsAttributeDefinitions[];


#define BugCheckFileId                   (NTFS_BUG_CHECK_FLUSH)

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')



#if (DBG || defined( NTFS_FREE_ASSERTS ))
#define NtfsInsertFcbTableEntry(IC,V,F,FR) {								\
    FCB_TABLE_ELEMENT _Key;													\
    PFCB_TABLE_ELEMENT _NewKey;												\
    _Key.FileReference = (FR);												\
    _Key.Fcb = (F);															\
    if (FlagOn((F)->NdasNtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY))				\
		_NewKey = RtlInsertElementGenericTable( &(V)->SecondaryFcbTable,    \
			                                    &_Key,                      \
				                                sizeof(FCB_TABLE_ELEMENT),  \
					                            NULL );                     \
	else																	\
    	_NewKey = RtlInsertElementGenericTable( &(V)->FcbTable,             \
		                                        &_Key,                      \
			                                    sizeof(FCB_TABLE_ELEMENT),  \
				                                NULL );                     \
    ASSERT( _NewKey->Fcb == _Key.Fcb );										\
}
#else
#define NtfsInsertFcbTableEntry(IC,V,F,FR) {								\
    FCB_TABLE_ELEMENT _Key;													\
    _Key.FileReference = (FR);												\
    _Key.Fcb = (F);															\
	if (FlagOn((F)->NdasNtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY))				\
		(VOID) RtlInsertElementGenericTable( &(V)->SecondaryFcbTable,		\
			                                 &_Key,							\
				                             sizeof(FCB_TABLE_ELEMENT),		\
					                         NULL );						\
	else																	\
	    (VOID) RtlInsertElementGenericTable( &(V)->FcbTable,				\
			                                 &_Key,							\
				                             sizeof(FCB_TABLE_ELEMENT),		\
					                         NULL );						\
}
#endif

#if (DBG || defined( NTFS_FREE_ASSERTS ))
#define NtfsInsertFcbTableEntryFull(IC,V,F,FR,N,SR) {							\
    FCB_TABLE_ELEMENT _Key;														\
    PFCB_TABLE_ELEMENT _NewKey;													\
    _Key.FileReference = (FR);													\
    _Key.Fcb = (F);																\
	if (FlagOn((F)->NdasNtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY))				\
	    _NewKey = RtlInsertElementGenericTableFull( &(V)->SecondaryFcbTable,    \
		                                            &_Key,                      \
			                                        sizeof(FCB_TABLE_ELEMENT),  \
				                                    NULL,                       \
					                                (N),                        \
						                            (SR)                        \
							                        );                          \
	else																		\
		_NewKey = RtlInsertElementGenericTableFull( &(V)->FcbTable,				\
			                                        &_Key,                      \
				                                    sizeof(FCB_TABLE_ELEMENT),  \
					                                NULL,                       \
						                            (N),                        \
							                        (SR)                        \
								                    );                          \
    ASSERT( _NewKey->Fcb == _Key.Fcb );											\
}
#else
#define NtfsInsertFcbTableEntryFull(IC,V,F,FR,N,SR) {						\
    FCB_TABLE_ELEMENT _Key;													\
    _Key.FileReference = (FR);												\
    _Key.Fcb = (F);															\
	if (FlagOn((F)->NdasNtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY))			\
		(VOID) RtlInsertElementGenericTableFull( &(V)->SecondaryFcbTable,   \
			                                     &_Key,                     \
				                                 sizeof(FCB_TABLE_ELEMENT), \
					                             NULL,                      \
						                         (N),                       \
							                     (SR)                       \
								                 );                         \
	else																	\
		(VOID) RtlInsertElementGenericTableFull( &(V)->FcbTable,            \
			                                     &_Key,                     \
				                                 sizeof(FCB_TABLE_ELEMENT), \
					                             NULL,                      \
						                         (N),                       \
							                     (SR)                       \
								                 );                         \
}
#endif

NTSTATUS
SecondaryRecoverySession (
	IN  PSECONDARY		Secondary
	);

VOID
SecondaryRecoveryThreadProc (
	IN	PSECONDARY	Secondary
	)
{
	NTSTATUS	status;

	BOOLEAN		secondaryResourceAcquired = FALSE;
	BOOLEAN		secondaryRecoveryResourceAcquired = FALSE;


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	DebugTrace2( 0, Dbg2, ("SecondaryRecoveryThreadProc: Start Secondary = %p\n", Secondary) );
	
	Secondary_Reference( Secondary );
	FsRtlEnterFileSystem();

	KeSetEvent( &Secondary->RecoveryReadyEvent, IO_DISK_INCREMENT, FALSE );

	try {

		secondaryRecoveryResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( NULL, 
													 &Secondary->VolDo->RecoveryResource, 
													 TRUE );
								
		if (!FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			NDASNTFS_ASSERT( FALSE );

			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->RecoveryResource );
			secondaryRecoveryResourceAcquired = FALSE;

			leave;
		}

		secondaryResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( NULL, 
													 &Secondary->VolDo->Resource, 
													 TRUE );
		
		try {
								
			status = SecondaryRecoverySession( Secondary );
								
		} finally {

			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->Resource );
			secondaryResourceAcquired = FALSE;

			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->RecoveryResource );
			secondaryRecoveryResourceAcquired = FALSE;
		}
	
	} finally {

		if (secondaryResourceAcquired)
			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->Resource );

		if (secondaryRecoveryResourceAcquired)
			SecondaryReleaseResourceLite( NULL, &Secondary->VolDo->RecoveryResource );

		Secondary->RecoveryThreadHandle = NULL;

		FsRtlExitFileSystem();
		Secondary_Dereference( Secondary );

		DebugTrace2( 0, Dbg2, ("SecondaryRecoveryThreadProc: Terminated Secondary = %p\n", Secondary) );
	}

	PsTerminateSystemThread( STATUS_SUCCESS );
}


NTSTATUS
SecondaryRecoverySessionStart (
	IN  PSECONDARY		Secondary,
	IN  PIRP_CONTEXT	IrpContext
	)
{
	NTSTATUS			status;
	OBJECT_ATTRIBUTES	objectAttributes;
	LARGE_INTEGER		timeOut;

	if (Secondary->RecoveryThreadHandle)
		return STATUS_SUCCESS;

	ASSERT( ExIsResourceAcquiredExclusiveLite(&Secondary->VolDo->RecoveryResource) && 
			ExIsResourceAcquiredExclusiveLite(&Secondary->VolDo->Resource) );

	ASSERT( NtfsIsTopLevelRequest(IrpContext) || 
		NtfsIsTopLevelNtfs( IrpContext) && NtfsGetTopLevelContext()->SavedTopLevelIrp == (PIRP)FSRTL_FSP_TOP_LEVEL_IRP ||
			FlagOn(IrpContext->State, IRP_CONTEXT_STATE_IN_FSP) );

	if (IrpContext->OriginatingIrp)
		PrintIrp( Dbg2, "SecondaryRecoverySessionStart", NULL, IrpContext->OriginatingIrp );

	if (FlagOn(Secondary->VolDo->NdasNtfsFlags, NDAS_NTFS_DEVICE_FLAG_SHUTDOWN)) {

		//DebugTrace( 0, Dbg2, ("SecondaryToPrimary NDAS_NTFS_DEVICE_FLAG_SHUTDOWN\n") ); 
		//DbgPrint( "SecondaryToPrimary NDAS_NTFS_DEVICE_FLAG_SHUTDOWN\n" ); 

		NtfsRaiseStatus( IrpContext, STATUS_TOO_LATE, NULL, NULL );
	}

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	status = PsCreateSystemThread( &Secondary->RecoveryThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   SecondaryRecoveryThreadProc,
								   Secondary );

	if (!NT_SUCCESS(status)) {
		
		return status;
	}

	timeOut.QuadPart = -NDASNTFS_TIME_OUT;		
	
	status = KeWaitForSingleObject( &Secondary->RecoveryReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		return status;
	}

	KeClearEvent( &Secondary->RecoveryReadyEvent );

	if (IrpContext->OriginatingIrp)
		PrintIrp( Dbg2, "SecondaryRecoverySessionStart returned", NULL, IrpContext->OriginatingIrp );

	return status;
}


NTSTATUS
SecondaryRecoverySession (
	IN  PSECONDARY		Secondary
	)
{
	NTSTATUS			status;
	LONG				slotIndex;

	LARGE_INTEGER		timeOut;
	OBJECT_ATTRIBUTES	objectAttributes;

	ULONG				reconnectionTry;
    PLIST_ENTRY			ccblistEntry;
	BOOLEAN				isLocalAddress;


	SetFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

	ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ); 
	ASSERT( Secondary->ThreadHandle );

	ASSERT( IsListEmpty(&Secondary->RequestQueue) );

	for (slotIndex=0; slotIndex < Secondary->Thread.SessionContext.SessionSlotCount; slotIndex++) {

		ASSERT( Secondary->Thread.SessionSlot[slotIndex] == NULL );
	}	

	if (Secondary->ThreadHandle) {

		ASSERT( Secondary->ThreadObject );
		
		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( Secondary->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

		if(status != STATUS_SUCCESS) {

			ASSERT( NDASNTFS_BUG );
			return status;
		}

		DebugTrace( 0, Dbg2, ("Secondary_Stop: thread stoped\n") );

		ObDereferenceObject( Secondary->ThreadObject );

		Secondary->ThreadHandle = 0;
		Secondary->ThreadObject = 0;

		RtlZeroMemory( &Secondary->Thread.Flags, sizeof(SECONDARY) - FIELD_OFFSET(SECONDARY, Thread.Flags) );
	}

	for (status = STATUS_UNSUCCESSFUL, reconnectionTry = 0; reconnectionTry < MAX_RECONNECTION_TRY; reconnectionTry++) {

		if (FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_TARGET_DEVICE_STOPPED)) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_FLAG_SHUTDOWN)) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (FlagOn(Secondary->VolDo->NdasNtfsFlags, NDAS_NTFS_DEVICE_FLAG_SHUTDOWN)) {

			//DebugTrace( 0, Dbg2, ("SecondaryToPrimary NDAS_NTFS_DEVICE_FLAG_SHUTDOWN\n") ); 
			//DbgPrint( "SecondaryToPrimary NDAS_NTFS_DEVICE_FLAG_SHUTDOWN\n" ); 

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (Secondary->VolDo->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

			status = STATUS_SUCCESS;

		} else {

			status = ((PVOLUME_DEVICE_OBJECT) NdasNtfsFileSystemDeviceObject)->
						NdfsCallback.SecondaryToPrimary( Secondary->VolDo->Vcb.Vpb->RealDevice, TRUE );

			if (status == STATUS_NO_SUCH_DEVICE) {

				NDASNTFS_ASSERT( FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_TARGET_DEVICE_STOPPED) );
				ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

				return STATUS_UNSUCCESSFUL;
			}
		}


		//NtfsDebugTraceLevel = 0;

		DebugTrace( 0, Dbg2, ("SecondaryToPrimary status = %x\n", status) ); 

		if (status == STATUS_SUCCESS) {

			PVCB				vcb = &Secondary->VolDo->Vcb;
			TOP_LEVEL_CONTEXT	topLevelContext;
			PTOP_LEVEL_CONTEXT	threadTopLevelContext;
			PIRP_CONTEXT		tempIrpContext = NULL;

			SetFlag( Secondary->Flags, SECONDARY_FLAG_CLEANUP_VOLUME );

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&NtfsData.Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&NtfsData.Resource) );	

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&vcb->Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&vcb->Resource) );	


			DebugTrace( 0, Dbg2, ("Vcb->State = %X\n", vcb->VcbState) );
			DebugTrace( 0, Dbg2, ("Vcb->Vpb->ReferenceCount = %X\n", vcb->Vpb->ReferenceCount) );
			DebugTrace( 0, Dbg2, ("Vcb->TargetDeviceObject->ReferenceCount = %X\n", vcb->TargetDeviceObject->ReferenceCount) );

			tempIrpContext = NULL;

			threadTopLevelContext = NtfsInitializeTopLevelIrp( &topLevelContext, TRUE, FALSE );
			ASSERT( threadTopLevelContext == &topLevelContext );

			NtfsInitializeIrpContext( NULL, TRUE, &tempIrpContext );
            NtfsUpdateIrpContextWithTopLevel( tempIrpContext, threadTopLevelContext );

			SetFlag( tempIrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
			ASSERT( FlagOn(tempIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL) );
						
			tempIrpContext->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
			tempIrpContext->MinorFunction = IRP_MN_MOUNT_VOLUME;
			tempIrpContext->Vcb			  = vcb;

			try {
			
				status = CleanUpVcb( tempIrpContext, vcb );
			
			} finally {
                
				NtfsCompleteRequest( tempIrpContext, NULL, 0 );
				ASSERT( IoGetTopLevelIrp() != (PIRP) &topLevelContext );
				tempIrpContext = NULL;
			}

			ASSERT( status == STATUS_SUCCESS );
			ASSERT( FlagOn(vcb->VcbState, VCB_STATE_MOUNT_COMPLETED) );

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&NtfsData.Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&NtfsData.Resource) );	

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&vcb->Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&vcb->Resource) );	

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_CLEANUP_VOLUME );


			DebugTrace( 0, Dbg2, ("Vcb->TargetDeviceObject->ReferenceCount = %X\n", vcb->TargetDeviceObject->ReferenceCount) );
			DebugTrace( 0, Dbg2, ("Vcb->Vpb->ReferenceCount = %X\n", vcb->Vpb->ReferenceCount) );
			DebugTrace( 0, Dbg2, ("Vcb->CloseCount = %d\n", vcb->CloseCount) );

			Secondary->VolDo->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY; 

			tempIrpContext = NULL;

			threadTopLevelContext = NtfsInitializeTopLevelIrp( &topLevelContext, TRUE, FALSE );
			ASSERT( threadTopLevelContext == &topLevelContext );

			NtfsInitializeIrpContext( NULL, TRUE, &tempIrpContext );
            NtfsUpdateIrpContextWithTopLevel( tempIrpContext, threadTopLevelContext );

			ASSERT( FlagOn(tempIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL) );
			
			//tempIrpContext->TopLevelIrpContext->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
			//tempIrpContext->TopLevelIrpContext->MinorFunction = IRP_MN_MOUNT_VOLUME;
			
			tempIrpContext->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
			tempIrpContext->MinorFunction = IRP_MN_MOUNT_VOLUME;
			tempIrpContext->Vcb			  = vcb;

			try {
			
				status = NdasNtfsMountVolume( tempIrpContext, vcb );
			
			} finally {
                
				NtfsCompleteRequest( tempIrpContext, NULL, 0 );
				ASSERT( IoGetTopLevelIrp() != (PIRP) &topLevelContext );
				tempIrpContext = NULL;
			}

			ASSERT( status == STATUS_SUCCESS );

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&NtfsData.Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&NtfsData.Resource) );	

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&vcb->Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&vcb->Resource) );	

			DebugTrace( 0, Dbg2, ("Vcb->TargetDeviceObject->ReferenceCount = %X\n", vcb->TargetDeviceObject->ReferenceCount) );
			DebugTrace( 0, Dbg2, ("Vcb->Vpb->ReferenceCount = %X\n", vcb->Vpb->ReferenceCount) );
			DebugTrace( 0, Dbg2, ("Vcb->CloseCount = %d\n", vcb->CloseCount) );

			if (vcb->MftScb) {
			
				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->MftScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->MftScb->Header.Resource) );	
			}

			if (vcb->Mft2Scb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->Mft2Scb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->Mft2Scb->Header.Resource) );	
			}

			if (vcb->LogFileScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->LogFileScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->LogFileScb->Header.Resource) );			
			}

			if (vcb->VolumeDasdScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->VolumeDasdScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->VolumeDasdScb->Header.Resource) );			
			}

			if (vcb->AttributeDefTableScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->AttributeDefTableScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->AttributeDefTableScb->Header.Resource) );			
			}

			if (vcb->UpcaseTableScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->UpcaseTableScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->UpcaseTableScb->Header.Resource) );			
			}

			if (vcb->RootIndexScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->RootIndexScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->RootIndexScb->Header.Resource) );			
			}

			if (vcb->BitmapScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->BitmapScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->BitmapScb->Header.Resource) );			
			}

			if (vcb->BadClusterFileScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->BadClusterFileScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->BadClusterFileScb->Header.Resource) );		
			}

			if (vcb->MftBitmapScb) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->MftBitmapScb->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->MftBitmapScb->Header.Resource) );			
			}

			if (vcb->SecurityDescriptorStream) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->SecurityDescriptorStream->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->SecurityDescriptorStream->Header.Resource) );			
			}

			if (vcb->UsnJournal) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->UsnJournal->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->UsnJournal->Header.Resource) );			
			}

			if (vcb->ExtendDirectory) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->ExtendDirectory->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->ExtendDirectory->Header.Resource) );			
			}

			if (vcb->SecurityDescriptorHashIndex) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->SecurityDescriptorHashIndex->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->SecurityDescriptorHashIndex->Header.Resource) );			
			}

			if (vcb->SecurityIdIndex) {

				ASSERT( !ExIsResourceAcquiredExclusiveLite(vcb->SecurityIdIndex->Header.Resource) );			
				ASSERT( !ExIsResourceAcquiredSharedLite(vcb->SecurityIdIndex->Header.Resource) );			
			}
		}

		status = ((PVOLUME_DEVICE_OBJECT) NdasNtfsFileSystemDeviceObject)->
						NdfsCallback.QueryPrimaryAddress( &Secondary->VolDo->NetdiskPartitionInformation, &Secondary->PrimaryAddress, &isLocalAddress );

		DebugTrace( 0, Dbg2, ("RecoverSession: LfsTable_QueryPrimaryAddress status = %X\n", status) );
	
		if (status == STATUS_SUCCESS && !(Secondary->VolDo->NetdiskEnableMode == NETDISK_SECONDARY && isLocalAddress)) {

			DebugTrace( 0, Dbg, ("SessionRecovery: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
								  Secondary->PrimaryAddress.Node[0],
								  Secondary->PrimaryAddress.Node[1],
								  Secondary->PrimaryAddress.Node[2],
								  Secondary->PrimaryAddress.Node[3],
								  Secondary->PrimaryAddress.Node[4],
								  Secondary->PrimaryAddress.Node[5],
								  NTOHS(Secondary->PrimaryAddress.Port)) );
		
		} else {

			continue;
		}	
		
		KeInitializeEvent( &Secondary->ReadyEvent, NotificationEvent, FALSE );
		KeInitializeEvent( &Secondary->RequestEvent, NotificationEvent, FALSE );

		InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

		status = PsCreateSystemThread( &Secondary->ThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   SecondaryThreadProc,
									   Secondary );

		if (!NT_SUCCESS(status)) {

			ASSERT( NDASNTFS_UNEXPECTED );
			break;
		}

		status = ObReferenceObjectByHandle( Secondary->ThreadHandle,
											FILE_READ_DATA,
											NULL,
											KernelMode,
											&Secondary->ThreadObject,
											NULL );

		if (!NT_SUCCESS(status)) {

			ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
			break;
		}

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &Secondary->ReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

		if(status != STATUS_SUCCESS) {

			ASSERT( NDASNTFS_BUG );
			break;
		}

		KeClearEvent( &Secondary->ReadyEvent );

		InterlockedIncrement( &Secondary->SessionId );	

		ExAcquireFastMutex( &Secondary->FastMutex );

		if (!FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) || FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

			ExReleaseFastMutex( &Secondary->FastMutex );

			if(Secondary->Thread.SessionStatus == STATUS_DISK_CORRUPT_ERROR) {

				status = STATUS_SUCCESS;
				break;
			}

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			status = KeWaitForSingleObject( Secondary->ThreadObject,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );

			if(status != STATUS_SUCCESS) {

				ASSERT( NDASNTFS_BUG );
				return status;
			}

			DebugTrace( 0, Dbg, ("Secondary_Stop: thread stoped\n") );

			ObDereferenceObject( Secondary->ThreadObject );

			Secondary->ThreadHandle = 0;
			Secondary->ThreadObject = 0;

			RtlZeroMemory( &Secondary->Thread.Flags, sizeof(SECONDARY) - FIELD_OFFSET(SECONDARY, Thread.Flags) );

			continue;
		} 

		ExReleaseFastMutex( &Secondary->FastMutex );

		status = STATUS_SUCCESS;

		DebugTrace( 0, Dbg2, ("SessionRecovery Success Secondary = %p\n", Secondary) );

		break;
	}

	if (status != STATUS_SUCCESS) {

		return status;
	}
	
	//NtfsDebugTraceLevel = 0xFFFFFFFFFFFFFFFF;
	//NtfsDebugTraceLevel &= ~DEBUG_TRACE_WRITE;
	
    for (ccblistEntry = Secondary->RecoveryCcbQueue.Blink; 
		 ccblistEntry != &Secondary->RecoveryCcbQueue; 
		 ccblistEntry = ccblistEntry->Blink) {

		PSCB						scb;
		PCCB						ccb;
		ULONG						disposition;
		
		ULONG						dataSize;
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		ccb = CONTAINING_RECORD( ccblistEntry, CCB, ListEntry );
		ccb->SessionId = Secondary->SessionId;
	
		if (FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			continue;
		}

		if (FlagOn(ccb->Flags, CCB_FLAG_CLOSE)) {

			DebugTrace( 0, Dbg2, ("RecoverSession: CCB_FLAG_CLOSE fcb = %p, ccb->Scb->FullPathName = %wZ \n", 
								  ccb->Lcb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED );
			continue;
		}

		scb = ccb->FileObject->FsContext;

		if (FlagOn(scb->Fcb->FcbState, FCB_STATE_FILE_DELETED)) {

			DebugTrace( 0, Dbg2, ("RecoverSession: FCB_STATE_FILE_DELETED fcb = %p, ccb->Scb->FullPathName = %wZ\n", 
								  ccb->Lcb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ASSERT( scb->Fcb->CleanupCount == 0 );
			SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED );
			
			continue;
		}

		if (FlagOn(scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED)) {

			DebugTrace( 0, Dbg2, ("RecoverSession: SCB_STATE_ATTRIBUTE_DELETED fcb = %p, ccb->Scb->FullPathName = %wZ\n", 
								  ccb->Lcb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ASSERT( scb->CleanupCount == 0 );
			SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED );
			
			continue;
		}

		if (FlagOn(ccb->Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE)) {

			DebugTrace( 0, Dbg, ("RecoverSession: LCB_STATE_DELETE_ON_CLOSE fcb = %p, ccb->Scb->FullPathName = %wZ\n", 
								  ccb->Lcb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ASSERT( scb->CleanupCount == 0 );
			SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED );
			
			continue;
		}

		if (ccb->CreateContext.RelatedFileHandle != 0) {

			ASSERT( FALSE );
			SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED );
			continue;
		}
				
		DebugTrace( 0, Dbg, ("SessionRecovery: ccb->Lcb->ExactCaseLink.LinkName = %wZ \n", &ccb->Lcb->ExactCaseLink.LinkName) );

		dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary,
														  IRP_MJ_CREATE,
														  (dataSize >= DEFAULT_NDAS_MAX_DATA_SIZE) ? dataSize : DEFAULT_NDAS_MAX_DATA_SIZE );

		if (secondaryRequest == NULL) {

			ASSERT( FALSE );
			status = STATUS_INSUFFICIENT_RESOURCES;	
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

		INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
										NDFS_COMMAND_EXECUTE,
										Secondary,
										IRP_MJ_CREATE,
										(ccb->BufferLength + ccb->Lcb->ExactCaseLink.LinkName.Length) );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

		//ndfsWinxpRequestHeader->IrpTag   = (_U32)ccb;
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle = 0;

		ndfsWinxpRequestHeader->IrpFlags   = ccb->IrpFlags;
		ndfsWinxpRequestHeader->IrpSpFlags = ccb->IrpSpFlags;

		ndfsWinxpRequestHeader->Create.FileNameLength 
			= (USHORT)(ccb->Lcb->ExactCaseLink.LinkName.Length + (ccb->BufferLength - ccb->CreateContext.EaLength));
		
		disposition = FILE_OPEN_IF;
		ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
		ndfsWinxpRequestHeader->Create.Options |= (disposition << 24);

		ndfsWinxpRequestHeader->Create.Options &= ~FILE_DELETE_ON_CLOSE;

		if (FlagOn(ccb->Flags, CCB_FLAG_DELETE_ON_CLOSE)) {

			DebugTrace( 0, Dbg2, ("RecoverSession: CCB_FLAG_DELETE_ON_CLOSE fcb = %p, ccb->Scb->FullPathName = %wZ \n", 
								   scb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ndfsWinxpRequestHeader->Create.Options |= FILE_DELETE_ON_CLOSE;
		}

		ndfsWinxpRequestHeader->Create.FileAttributes = ccb->CreateContext.FileAttributes;
		ndfsWinxpRequestHeader->Create.ShareAccess = ccb->CreateContext.ShareAccess;
		ndfsWinxpRequestHeader->Create.EaLength = 0; //ccb->CreateContext.EaLength;
		ndfsWinxpRequestHeader->Create.RelatedFileHandle = ccb->CreateContext.RelatedFileHandle;
		ndfsWinxpRequestHeader->Create.FileNameLength = ccb->CreateContext.FileNameLength;
		ndfsWinxpRequestHeader->Create.AllocationSize = 0;
		
		ndfsWinxpRequestHeader->Create.SecurityContext.DesiredAccess = ccb->CreateContext.SecurityContext.DesiredAccess;
		ndfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions = ccb->CreateContext.SecurityContext.FullCreateOptions;

		ndfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions &= ~FILE_DELETE_ON_CLOSE;

		if (FlagOn(ccb->Flags, CCB_FLAG_DELETE_ON_CLOSE)) {

			ndfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions |= FILE_DELETE_ON_CLOSE;
		}

		ndfsWinxpRequestHeader->Create.SecurityContext.AccessState.Flags = ccb->CreateContext.SecurityContext.AccessState.Flags;
		ndfsWinxpRequestHeader->Create.SecurityContext.AccessState.RemainingDesiredAccess = ccb->CreateContext.SecurityContext.AccessState.RemainingDesiredAccess;
		ndfsWinxpRequestHeader->Create.SecurityContext.AccessState.PreviouslyGrantedAccess = ccb->CreateContext.SecurityContext.AccessState.PreviouslyGrantedAccess;
		ndfsWinxpRequestHeader->Create.SecurityContext.AccessState.OriginalDesiredAccess = ccb->CreateContext.SecurityContext.AccessState.OriginalDesiredAccess;


		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

		RtlCopyMemory( ndfsWinxpRequestData,
					   ccb->Buffer,
					   ndfsWinxpRequestHeader->Create.EaLength );

		RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength,
					   ccb->Lcb->ExactCaseLink.LinkName.Buffer,
					   ccb->Lcb->ExactCaseLink.LinkName.Length );

		RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength + ccb->Lcb->ExactCaseLink.LinkName.Length,
					   ccb->Buffer + ccb->CreateContext.EaLength,
					   ccb->BufferLength - ccb->CreateContext.EaLength );

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(Secondary, secondaryRequest);
				
		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if (status != STATUS_SUCCESS) {
		
			ASSERT( NDASNTFS_BUG );

			secondaryRequest = NULL;
			break;
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			status = secondaryRequest->ExecuteStatus;
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
		DebugTrace( 0, Dbg, ("SessionRecovery: ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

		if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

			LONG		ccbCount;
			PLIST_ENTRY	ccbListEntry;

			for (ccbCount = 0, ccbListEntry = scb->CcbQueue.Flink; 
				ccbListEntry != &scb->CcbQueue; 
				ccbListEntry = ccbListEntry->Flink, ccbCount++);

			SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED | ND_NTFS_CCB_FLAG_CORRUPTED );
			scb->CorruptedCcbCloseCount ++;

			DebugTrace( 0, Dbg2, ("RecoverSession: ccb->Lcb->ExactCaseLink.LinkName = %wZ Corrupted Status = %x scb->CorruptedCcbCloseCount = %d, scb->CloseCount = %d\n",
								   &ccb->Lcb->ExactCaseLink.LinkName, ndfsWinxpReplytHeader->Status, scb->CorruptedCcbCloseCount, scb->CloseCount));

			if (scb->CloseCount == scb->CorruptedCcbCloseCount)
				SetFlag( scb->NdasNtfsFlags, ND_NTFS_SCB_FLAG_CORRUPTED );

			DereferenceSecondaryRequest( secondaryRequest );
			
			continue;
		}

		ccb->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
		scb->Handle = ndfsWinxpReplytHeader->Open.ScbHandle;

		ccb->Lcb->SecondaryScb = (PSCB)ndfsWinxpReplytHeader->Open.Lcb.Scb;

		if (scb->Fcb->FileReference.SegmentNumberHighPart != ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart ||
			scb->Fcb->FileReference.SegmentNumberLowPart  != ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart	||
			scb->Fcb->FileReference.SequenceNumber		  != ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber) {
		
			FILE_REFERENCE		fileReference;
			FCB_TABLE_ELEMENT	key;
			PVOID				nodeOrParent;
			TABLE_SEARCH_RESULT searchResult;
					
			DebugTrace( 0, Dbg2, ("FileReference is Different scb->Fcb = %p\n", scb->Fcb) );

			fileReference.SegmentNumberHighPart = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart;
			fileReference.SegmentNumberLowPart  = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart;
			fileReference.SequenceNumber		= ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber;
					
			key.FileReference = fileReference;

			if (RtlLookupElementGenericTableFull(&scb->Fcb->Vcb->SecondaryFcbTable, &key, &nodeOrParent, &searchResult) != NULL) {

				ASSERT( FALSE );
				
			} else {
					
				NtfsDeleteFcbTableEntry( scb->Fcb, scb->Fcb->Vcb, scb->Fcb->FileReference );
				scb->Fcb->FileReference = fileReference;
				NtfsInsertFcbTableEntryFull( IrpContext, scb->Fcb->Vcb, scb->Fcb, fileReference, nodeOrParent, searchResult );
			}
		}

		if (scb->Header.AllocationSize.QuadPart == ndfsWinxpReplytHeader->AllocationSize) {
		
			if (scb->Header.FileSize.QuadPart == ndfsWinxpReplytHeader->FileSize) {

				DereferenceSecondaryRequest( secondaryRequest );
				SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_REOPENED );
			
			} else {

				DereferenceSecondaryRequest( secondaryRequest );

				secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 
																  IRP_MJ_SET_INFORMATION, 
																  Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					status = STATUS_INSUFFICIENT_RESOURCES;	
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
												NDFS_COMMAND_EXECUTE, 
												Secondary, 
												IRP_MJ_SET_INFORMATION, 
												0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag   = (_U32)ccb;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

				ndfsWinxpRequestHeader->IrpFlags   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;
				ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
				ndfsWinxpRequestHeader->SetFile.Length					= 0;

				ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
				ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
				ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
				ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

				ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = scb->Header.FileSize.QuadPart;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASNTFS_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {
	
					ASSERT( NDASNTFS_BUG );
					secondaryRequest = NULL;
					break;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					status = STATUS_CANT_WAIT;	
					break;
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				ASSERT( NT_SUCCESS(ndfsWinxpReplytHeader->Status) );

				DereferenceSecondaryRequest( secondaryRequest );

				SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_REOPENED );
			}

		} else {

			BOOLEAN				lookupResut;
			VCN					vcn;
			LCN					lcn;
			LCN					startingLcn;
			LONGLONG			clusterCount;


			DereferenceSecondaryRequest( secondaryRequest );

			DebugTrace( 0, Dbg2, ("RecoverSession: scb->Header.AllocationSize.QuadPart != ndfsWinxpReplytHeader->AllocationSize\n") );

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_SET_INFORMATION, 0 );

			if (secondaryRequest == NULL) {

				status = STATUS_INSUFFICIENT_RESOURCES;	
				break;
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
											NDFS_COMMAND_EXECUTE, 
											Secondary, 
											IRP_MJ_SET_INFORMATION, 
											0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag   = (_U32)ccb;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

			ndfsWinxpRequestHeader->IrpFlags   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileAllocationInformation;
			ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
			ndfsWinxpRequestHeader->SetFile.Length					= 0;

			ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
			ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
			ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
			ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

			ndfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize  = 0;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			if(status != STATUS_SUCCESS) {
	
				ASSERT( NDASNTFS_BUG );
				secondaryRequest = NULL;
				break;
			}

			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				status = STATUS_CANT_WAIT;	
				break;
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			ASSERT( NT_SUCCESS(ndfsWinxpReplytHeader->Status) );

			DereferenceSecondaryRequest( secondaryRequest );

			vcn = 0;

			while (LlBytesFromClusters(&Secondary->VolDo->Vcb, vcn) < (ULONGLONG)scb->Header.AllocationSize.QuadPart) {

				lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );

				DebugTrace( 0, Dbg2, ("RecoverSession: vcn = %I64d, clusterCount = %I64d\n", vcn, clusterCount) );

				if (lookupResut == FALSE || !(LlBytesFromClusters(&Secondary->VolDo->Vcb, (vcn + clusterCount)) <= (ULONGLONG)scb->Header.AllocationSize.QuadPart)) {

					ASSERT( FALSE );
					break;
				}

				ASSERT( lcn == startingLcn );

				secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 
																  IRP_MJ_SET_INFORMATION, 
																  Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					status = STATUS_INSUFFICIENT_RESOURCES;	
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
												NDFS_COMMAND_EXECUTE, 
												Secondary, 
												IRP_MJ_SET_INFORMATION, 
												0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag   = (_U32)ccb;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

				ndfsWinxpRequestHeader->IrpFlags   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileAllocationInformation;
				ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
				ndfsWinxpRequestHeader->SetFile.Length					= 0;

				ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
				ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
				ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
				ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

				ndfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize = LlBytesFromClusters(&Secondary->VolDo->Vcb, (vcn + clusterCount));
				//ndfsWinxpRequestHeader->SetFile.AllocationInformation.Lcn			 = startingLcn;
				ndfsWinxpRequestHeader->SetFile.AllocationInformation.Lcn			 = lcn;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASNTFS_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {
	
					ASSERT( NDASNTFS_BUG );
					secondaryRequest = NULL;
					break;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					status = STATUS_CANT_WAIT;	
					break;
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

					ASSERT( FALSE );
					DereferenceSecondaryRequest( secondaryRequest );
					break;
				}
 
#if DBG
				if (LlBytesFromClusters(&Secondary->VolDo->Vcb, (vcn + clusterCount)) == scb->Header.AllocationSize.QuadPart) {

					PNDFS_NTFS_MCB_ENTRY	mcbEntry;
					ULONG			index;
					VBO				testVcn;

					BOOLEAN			lookupResut2;
					VCN				vcn2;
					LCN				lcn2;
					LCN				startingLcn2;
					LONGLONG		clusterCount2;

					ASSERT( ndfsWinxpReplytHeader->AllocationSize == scb->Header.AllocationSize.QuadPart );
					mcbEntry = (PNDFS_NTFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

					for (index=0, testVcn= 0, vcn2=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++, mcbEntry++) {

						ASSERT( mcbEntry->Vcn == testVcn );
						testVcn += mcbEntry->ClusterCount;

						lookupResut2 = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn2, &lcn2, &clusterCount2, &startingLcn2, NULL, NULL, NULL );
					
						ASSERT( lookupResut2 == TRUE );
						ASSERT( startingLcn2 == lcn2 );
						ASSERT( vcn2 == mcbEntry->Vcn );
						ASSERT( lcn2 == mcbEntry->Lcn );
						ASSERT( clusterCount2 == mcbEntry->ClusterCount );

						vcn2 += clusterCount2;
					}
				}

#endif
				DereferenceSecondaryRequest( secondaryRequest );

				vcn += clusterCount;
			} 

			if (LlBytesFromClusters(&Secondary->VolDo->Vcb, vcn) == scb->Header.AllocationSize.QuadPart) {

				secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 
																  IRP_MJ_SET_INFORMATION, 
																  Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					status = STATUS_INSUFFICIENT_RESOURCES;	
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
												NDFS_COMMAND_EXECUTE, 
												Secondary, 
												IRP_MJ_SET_INFORMATION, 
												0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag   = (_U32)ccb;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

				ndfsWinxpRequestHeader->IrpFlags   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;
				ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
				ndfsWinxpRequestHeader->SetFile.Length					= 0;

				ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
				ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
				ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
				ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

				ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = scb->Header.FileSize.QuadPart;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASNTFS_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {
	
					ASSERT( NDASNTFS_BUG );
					secondaryRequest = NULL;
					break;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					status = STATUS_CANT_WAIT;	
					break;
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				ASSERT( NT_SUCCESS(ndfsWinxpReplytHeader->Status) );

				DereferenceSecondaryRequest( secondaryRequest );
				
				SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_REOPENED );
			
			} else {

				LONG		ccbCount;
				PLIST_ENTRY	ccbListEntry;

				ClosePrimaryFile( Secondary, ccb->PrimaryFileHandle );

				for (ccbCount = 0, ccbListEntry = scb->CcbQueue.Flink; 
					 ccbListEntry != &scb->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink, ccbCount++);

				SetFlag( ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED | ND_NTFS_CCB_FLAG_CORRUPTED );
				scb->CorruptedCcbCloseCount ++;

				DebugTrace( 0, Dbg2, ("RecoverSession: ccb->Lcb->ExactCaseLink.LinkName = %wZ Corrupted Status = %x scb->CorruptedCcbCloseCount = %d, scb->CloseCount = %d\n",
									   &ccb->Lcb->ExactCaseLink.LinkName, ndfsWinxpReplytHeader->Status, scb->CorruptedCcbCloseCount, scb->CloseCount));

				if (scb->CloseCount == scb->CorruptedCcbCloseCount)
					SetFlag( scb->NdasNtfsFlags, ND_NTFS_SCB_FLAG_CORRUPTED );

				continue;
			}
		}
	}

    for (ccblistEntry = Secondary->RecoveryCcbQueue.Blink; 
		 ccblistEntry != &Secondary->RecoveryCcbQueue; 
		 ccblistEntry = ccblistEntry->Blink) {

		PSCB						scb;
		PCCB						ccb;
		
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		ccb = CONTAINING_RECORD( ccblistEntry, CCB, ListEntry );
	
		if (FlagOn(ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			continue;
		}

		scb = ccb->FileObject->FsContext;

		if (FlagOn(ccb->FileObject->Flags, FO_CLEANUP_COMPLETE)) {

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_CLEANUP, 0 );

			if (secondaryRequest == NULL) {

				status = STATUS_INSUFFICIENT_RESOURCES;	
				break;
			}

			DebugTrace( 0, Dbg, ("RecoverSession: FO_CLEANUP_COMPLETE fcb = %p, ccb->Scb->FullPathName = %wZ \n", 
								  scb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader, NDFS_COMMAND_EXECUTE, Secondary, IRP_MJ_CLEANUP, 0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

			//ndfsWinxpRequestHeader->IrpTag   = (_U32)ccb;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CLEANUP;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

			ndfsWinxpRequestHeader->IrpFlags   = 0; //ccb->IrpFlags;
			ndfsWinxpRequestHeader->IrpSpFlags = 0; //ccb->IrpSpFlags;

			ndfsWinxpRequestHeader->CleanUp.AllocationSize	= scb->Header.AllocationSize.QuadPart;
			ndfsWinxpRequestHeader->CleanUp.FileSize		= scb->Header.FileSize.QuadPart;
			ndfsWinxpRequestHeader->CleanUp.ValidDataLength = scb->Header.ValidDataLength.QuadPart;
			ndfsWinxpRequestHeader->CleanUp.VaildDataToDisk = scb->ValidDataToDisk;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			status = KeWaitForSingleObject(	&secondaryRequest->CompleteEvent,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );
	
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (status != STATUS_SUCCESS) {

				ASSERT( NDASNTFS_BUG );
				secondaryRequest = NULL;
				break;
			}
		
			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				status = secondaryRequest->ExecuteStatus;		
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				break;
			}
				
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;
		}
	}

	DebugTrace( 0, Dbg2, ("SessionRecovery: Completed. Secondary = %p, status = %x\n", Secondary, status) );

	ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

	//NtfsDebugTraceLevel = 0x00000009;

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	return status;
}


#endif