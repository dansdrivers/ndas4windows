#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#define BugCheckFileId                   (FAT_BUG_CHECK_FLUSH)

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')


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

			NDAS_BUGON( FALSE );

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

	if (Secondary->RecoveryThreadHandle) {

		return STATUS_SUCCESS;
	}

	NDAS_BUGON( ExIsResourceAcquiredExclusiveLite(&Secondary->VolDo->RecoveryResource) && 
			    ExIsResourceAcquiredExclusiveLite(&Secondary->VolDo->Resource) );

	NDAS_BUGON( IrpContext != NULL );

	NDAS_BUGON( FatIsTopLevelRequest(IrpContext) /*|| 
				FatIsTopLevelFat( IrpContext) && FatGetTopLevelContext()->SavedTopLevelIrp == (PIRP)FSRTL_FSP_TOP_LEVEL_IRP ||
				FlagOn(IrpContext->State, IRP_CONTEXT_STATE_IN_FSP)*/ );
	
	if (IrpContext->OriginatingIrp) {

		PrintIrp( Dbg2, "SecondaryRecoverySessionStart", NULL, IrpContext->OriginatingIrp );
	}

	if (FlagOn(Secondary->VolDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_SHUTDOWN)) {

		//DebugTrace2( 0, Dbg2, ("SecondaryToPrimary ND_FAT_DEVICE_FLAG_SHUTDOWN\n") ); 
		//DbgPrint( "SecondaryToPrimary ND_FAT_DEVICE_FLAG_SHUTDOWN\n" ); 

		FatRaiseStatus( IrpContext, STATUS_TOO_LATE );
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

		NDAS_BUGON( FALSE );
		
		return status;
	}

	timeOut.QuadPart = -NDASFAT_TIME_OUT;		
	
	status = KeWaitForSingleObject( &Secondary->RecoveryReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		NDAS_BUGON( FALSE );

		return status;
	}

	KeClearEvent( &Secondary->RecoveryReadyEvent );

	if (IrpContext->OriginatingIrp)
		PrintIrp( Dbg2, "SecondaryRecoverySessionStart returned", NULL, IrpContext->OriginatingIrp );

	//status = SecondaryRecoverySession( Secondary, IrpContext );

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


	DebugTrace2( 0, Dbg2, ("SecondaryRecoverySession: Called Secondary = %p\n", Secondary) );

	SetFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

	ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ); 
	ASSERT( Secondary->ThreadHandle );

	ASSERT( IsListEmpty(&Secondary->RequestQueue) );

	for (slotIndex=0; slotIndex < Secondary->Thread.SessionContext.SessionSlotCount; slotIndex++) {

		ASSERT( Secondary->Thread.SessionSlot[slotIndex] == NULL );
	}

	if (Secondary->ThreadHandle) {

		ASSERT( Secondary->ThreadObject );
		
		timeOut.QuadPart = -NDASFAT_TIME_OUT;
		status = KeWaitForSingleObject( Secondary->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return status;
		}

		DebugTrace2( 0, Dbg2, ("Secondary_Stop: thread stoped\n") );

		ObDereferenceObject( Secondary->ThreadObject );

		Secondary->ThreadHandle = 0;
		Secondary->ThreadObject = 0;

		RtlZeroMemory( &Secondary->Thread.Flags, sizeof(SECONDARY) - FIELD_OFFSET(SECONDARY, Thread.Flags) );
	}

	for (status = STATUS_UNSUCCESSFUL, reconnectionTry = 0; reconnectionTry < MAX_RECONNECTION_TRY; reconnectionTry++) {

		if (FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_FLAG_LOCKED)) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_FLAG_SHUTDOWN)) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_FLAG_SHUTDOWN)) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (FlagOn(Secondary->VolDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_SHUTDOWN)) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );
			return STATUS_UNSUCCESSFUL;
		}

		if (Secondary->VolDo->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {
			
			status = STATUS_SUCCESS;
		
		} else {

			status = ((PVOLUME_DEVICE_OBJECT) FatData.DiskFileSystemDeviceObject)->
						NdfsCallback.SecondaryToPrimary( Secondary->VolDo->Vcb.Vpb->RealDevice, TRUE );

			if (status == STATUS_NO_SUCH_DEVICE) {

				NDAS_BUGON( FlagOn(Secondary->VolDo->Vcb.VcbState, VCB_STATE_FLAG_LOCKED) );
				ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

				return STATUS_UNSUCCESSFUL;
			}
		}

		//FatDebugTraceLevel = 0;

		DebugTrace2( 0, Dbg2, ("SecondaryToPrimary status = %x\n", status) ); 

#if 0
		if (queryResult == TRUE) {
			
			BOOLEAN	result0, result1;
		    IRP_CONTEXT IrpContext;
		
			ASSERT( Secondary->VolDo->Vcb.VirtualVolumeFile );
			result0 = CcPurgeCacheSection( &Secondary->VolDo->Vcb.SectionObjectPointers,
										   NULL,
										   0,
										   FALSE );
			ASSERT( Secondary->VolDo->Vcb.RootDcb->Specific.Dcb.DirectoryFile );
			result1 = CcPurgeCacheSection( &Secondary->VolDo->Vcb.RootDcb->NonPaged->SectionObjectPointers,
										   NULL,
										   0,
										   FALSE );
		
			ASSERT( result0 == TRUE );
			ASSERT( result1 == TRUE );

			ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
		    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );
            SetFlag(IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT);
			FatTearDownAllocationSupport ( &IrpContext, &Secondary->VolDo->Vcb );
   			FatSetupAllocationSupport( &IrpContext, &Secondary->VolDo->Vcb );
			FatCheckDirtyBit( &IrpContext, &Secondary->VolDo->Vcb );

	        ASSERT( IrpContext.Repinned.Bcb[0] == NULL );
	        FatUnpinRepinnedBcbs( &IrpContext );
    
			Secondary->VolDo->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY; 
			Secondary->VolDo->SecondaryState = CONNECT_TO_LOCAL_STATE;
		}
#endif

		if (status == STATUS_SUCCESS) {

			PVCB				vcb = &Secondary->VolDo->Vcb;
#if 0
			TOP_LEVEL_CONTEXT	topLevelContext;
			PTOP_LEVEL_CONTEXT	threadTopLevelContext;
#endif
			IRP_CONTEXT			tempIrpContext2;
			PIRP_CONTEXT		tempIrpContext = NULL;

			SetFlag( Secondary->Flags, SECONDARY_FLAG_CLEANUP_VOLUME );

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&FatData.Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&FatData.Resource) );	

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&vcb->SecondaryResource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&vcb->SecondaryResource) );	


			//FatReleaseAllResources( IrpContext );

			//ObReferenceObject( vcb->TargetDeviceObject );

			DebugTrace2( 0, Dbg2, ("Vcb->State = %X\n", vcb->VcbState) );
			DebugTrace2( 0, Dbg2, ("Vcb->Vpb->ReferenceCount = %X\n", vcb->Vpb->ReferenceCount) );
			DebugTrace2( 0, Dbg2, ("Vcb->TargetDeviceObject->ReferenceCount = %X\n", vcb->TargetDeviceObject->ReferenceCount) );

			tempIrpContext = NULL;

#if 0
			threadTopLevelContext = FatInitializeTopLevelIrp( &topLevelContext, TRUE, FALSE );
			ASSERT( threadTopLevelContext == &topLevelContext );

			FatInitializeIrpContext( NULL, TRUE, &tempIrpContext );
            FatUpdateIrpContextWithTopLevel( tempIrpContext, threadTopLevelContext );
			
			ASSERT( FlagOn(tempIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL) );
#endif
			tempIrpContext = &tempIrpContext2;

			RtlZeroMemory( tempIrpContext, sizeof(IRP_CONTEXT) );
			SetFlag( tempIrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
			SetFlag( tempIrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
		
			tempIrpContext->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
			tempIrpContext->MinorFunction = IRP_MN_MOUNT_VOLUME;
			tempIrpContext->Vcb			  = vcb;

			ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			try {
			
				status = STATUS_UNSUCCESSFUL;
				status = CleanUpVcb( tempIrpContext, vcb );
			
			} finally {
                
				//FatCompleteRequest( tempIrpContext, NULL, 0 );
				//ASSERT( IoGetTopLevelIrp() != (PIRP) &topLevelContext );
				tempIrpContext = NULL;
			}

			ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			ASSERT( status == STATUS_SUCCESS );
			//ASSERT( FlagOn(vcb->VcbState, VCB_STATE_MOUNT_COMPLETED) );
			ASSERT( FlagOn(Secondary->VolDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_MOUNTED) );
			ASSERT( !ExIsResourceAcquiredExclusiveLite(&FatData.Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&FatData.Resource) );	

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&vcb->SecondaryResource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&vcb->SecondaryResource) );	

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_CLEANUP_VOLUME );


			DebugTrace2( 0, Dbg2, ("Vcb->TargetDeviceObject->ReferenceCount = %X\n", vcb->TargetDeviceObject->ReferenceCount) );
			DebugTrace2( 0, Dbg2, ("Vcb->Vpb->ReferenceCount = %X\n", vcb->Vpb->ReferenceCount) );
			DebugTrace2( 0, Dbg2, ("Vcb->CloseCount = %d\n", vcb->OpenFileCount) );

#if 0
			tempIrpContext = NULL;

			threadTopLevelContext = FatInitializeTopLevelIrp( &topLevelContext, TRUE, FALSE );
			ASSERT( threadTopLevelContext == &topLevelContext );

			FatInitializeIrpContext( NULL, TRUE, &tempIrpContext );
            FatUpdateIrpContextWithTopLevel( tempIrpContext, threadTopLevelContext );

			ASSERT( FlagOn(tempIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL) );
#endif

			Secondary->VolDo->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY; 

			tempIrpContext = &tempIrpContext2;

			RtlZeroMemory( tempIrpContext, sizeof(IRP_CONTEXT) );
			SetFlag( tempIrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
			SetFlag( tempIrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
			
			tempIrpContext->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
			tempIrpContext->MinorFunction = IRP_MN_MOUNT_VOLUME;
			tempIrpContext->Vcb			  = vcb;
			
			SetFlag( Secondary->Flags, SECONDARY_FLAG_REMOUNT_VOLUME );

			try {
			
				status = STATUS_UNSUCCESSFUL;

				status = NdasFatMountVolume( tempIrpContext, vcb->TargetDeviceObject, vcb->Vpb, NULL );
			
			} finally {
                
				//FatCompleteRequest( tempIrpContext, NULL, 0 );
				//ASSERT( IoGetTopLevelIrp() != (PIRP) &topLevelContext );
				tempIrpContext = NULL;
			}

			//ObDereferenceObject( vcb->TargetDeviceObject );

			//FatDebugTraceLevel = 0xFFFFFFFFFFFFFFFF;
			//FatDebugTraceLevel |= DEBUG_TRACE_CREATE;

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_REMOUNT_VOLUME );

			ASSERT( status == STATUS_SUCCESS );

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&FatData.Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&FatData.Resource) );	

			ASSERT( !ExIsResourceAcquiredExclusiveLite(&vcb->Resource) );			
			ASSERT( !ExIsResourceAcquiredSharedLite(&vcb->Resource) );	

			DebugTrace2( 0, Dbg2, ("Vcb->TargetDeviceObject->ReferenceCount = %X\n", vcb->TargetDeviceObject->ReferenceCount) );
			DebugTrace2( 0, Dbg2, ("Vcb->Vpb->ReferenceCount = %X\n", vcb->Vpb->ReferenceCount) );
			DebugTrace2( 0, Dbg2, ("Vcb->CloseCount = %d\n", vcb->OpenFileCount) );

#if 0

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
#endif
		}


		status = ((PVOLUME_DEVICE_OBJECT) FatData.DiskFileSystemDeviceObject)->
						NdfsCallback.QueryPrimaryAddress( &Secondary->VolDo->NetdiskPartitionInformation, &Secondary->PrimaryAddress, &isLocalAddress );

		DebugTrace2( 0, Dbg2, ("RecoverSession: LfsTable_QueryPrimaryAddress status = %X\n", status) );
	
		if (status == STATUS_SUCCESS && !(Secondary->VolDo->NetdiskEnableMode == NETDISK_SECONDARY && isLocalAddress)) {

			DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
								  Secondary->PrimaryAddress.Node[0],
								  Secondary->PrimaryAddress.Node[1],
								  Secondary->PrimaryAddress.Node[2],
								  Secondary->PrimaryAddress.Node[3],
								  Secondary->PrimaryAddress.Node[4],
								  Secondary->PrimaryAddress.Node[5],
								  NTOHS(Secondary->PrimaryAddress.Port)) );
		
		} else {

			//ASSERT( FALSE );
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

			ASSERT( NDASFAT_UNEXPECTED );
			break;
		}

		status = ObReferenceObjectByHandle( Secondary->ThreadHandle,
											FILE_READ_DATA,
											NULL,
											KernelMode,
											&Secondary->ThreadObject,
											NULL );

		if (!NT_SUCCESS(status)) {

			ASSERT (NDASFAT_INSUFFICIENT_RESOURCES );
			break;
		}

		timeOut.QuadPart = -NDASFAT_TIME_OUT;
		status = KeWaitForSingleObject( &Secondary->ReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

		if (status != STATUS_SUCCESS) {

			ASSERT( NDASFAT_BUG );
			break;
		}

		KeClearEvent( &Secondary->ReadyEvent );

		InterlockedIncrement( &Secondary->SessionId );	

		ExAcquireFastMutex( &Secondary->FastMutex );

		if (!FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) || FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

			ExReleaseFastMutex( &Secondary->FastMutex );

			if (Secondary->Thread.SessionStatus == STATUS_DISK_CORRUPT_ERROR) {

				status = STATUS_SUCCESS;
				break;
			}

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject( Secondary->ThreadObject,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );

			if (status != STATUS_SUCCESS) {

				ASSERT( NDASFAT_BUG );
				return status;
			}

			DebugTrace2( 0, Dbg, ("Secondary_Stop: thread stoped\n") );

			ObDereferenceObject( Secondary->ThreadObject );

			Secondary->ThreadHandle = 0;
			Secondary->ThreadObject = 0;

			RtlZeroMemory( &Secondary->Thread.Flags, sizeof(SECONDARY) - FIELD_OFFSET(SECONDARY, Thread.Flags) );

			continue;
		} 

		ExReleaseFastMutex( &Secondary->FastMutex );

		status = STATUS_SUCCESS;

		DebugTrace2( 0, Dbg2, ("SecondaryRecoverySession Success Secondary = %p\n", Secondary) );

		break;
	}

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );
		return status;
	}
	
	//FatDebugTrace2Level = 0xFFFFFFFFFFFFFFFF;
	//FatDebugTrace2Level &= ~DEBUG_TRACE_WRITE;
	
    for (ccblistEntry = Secondary->RecoveryCcbQueue.Blink; 
		 ccblistEntry != &Secondary->RecoveryCcbQueue; 
		 ccblistEntry = ccblistEntry->Blink) {

		//PSCB						scb;
		PCCB						ccb;
		ULONG						disposition;
		
		ULONG						dataSize;
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		ccb = CONTAINING_RECORD( ccblistEntry, CCB, ListEntry );
		ccb->SessionId = Secondary->SessionId;
	
		if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

			continue;
		}

		if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_CLOSE)) {

			DebugTrace2( 0, Dbg2, ("RecoverSession: CCB_FLAG_CLOSE fcb = %p, ccb->Scb->FullPathName = %wZ \n", 
								  ccb->Fcb, &ccb->Fcb->FullFileName) );

			SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED );
			continue;
		}

#if 0

		scb = ccb->FileObject->FsContext;

		if (FlagOn(scb->Fcb->FcbState, FCB_STATE_FILE_DELETED)) {

			DebugTrace2( 0, Dbg2, ("RecoverSession: FCB_STATE_FILE_DELETED fcb = %p, ccb->Scb->FullPathName = %wZ\n", 
								  ccb->Fcb, &ccb->Fcb->FullFileName) );

			ASSERT( scb->Fcb->CleanupCount == 0 );
			SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED );
			
			continue;
		}

		if (FlagOn(scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED)) {

			DebugTrace2( 0, Dbg2, ("RecoverSession: SCB_STATE_ATTRIBUTE_DELETED fcb = %p, ccb->Scb->FullPathName = %wZ\n", 
								  ccb->Lcb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ASSERT( scb->CleanupCount == 0 );
			SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED );
			
			continue;
		}

		if (FlagOn(ccb->Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE)) {

			DebugTrace2( 0, Dbg, ("RecoverSession: LCB_STATE_DELETE_ON_CLOSE fcb = %p, ccb->Scb->FullPathName = %wZ\n", 
								  ccb->Lcb->Fcb, &ccb->Lcb->ExactCaseLink.LinkName) );

			ASSERT( scb->CleanupCount == 0 );
			SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED );
			
			continue;
		}

#endif

		if (ccb->CreateContext.RelatedFileHandle != 0) {

			ASSERT( FALSE );
			SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED );
			continue;
		}
				
		DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: ccb->Lcb->ExactCaseLink.LinkName = %wZ \n", &ccb->Fcb->FullFileName) );

		dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary,
														  IRP_MJ_CREATE,
														  (dataSize >= DEFAULT_NDAS_MAX_DATA_SIZE) ? dataSize : DEFAULT_NDAS_MAX_DATA_SIZE );

		if (secondaryRequest == NULL) {

			NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
			status = STATUS_INSUFFICIENT_RESOURCES;	
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

		INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
										NDFS_COMMAND_EXECUTE,
										Secondary,
										IRP_MJ_CREATE,
										(ccb->BufferLength + ccb->Fcb->FullFileName.Length) );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

		//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)ccb;
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle8 = 0;

		ndfsWinxpRequestHeader->IrpFlags4   = HTONL(ccb->IrpFlags);
		ndfsWinxpRequestHeader->IrpSpFlags = ccb->IrpSpFlags;

		ndfsWinxpRequestHeader->Create.FileNameLength 
			= (USHORT)(ccb->Fcb->FullFileName.Length + (ccb->BufferLength - ccb->CreateContext.EaLength));
		
		disposition = FILE_OPEN_IF;
		ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
		ndfsWinxpRequestHeader->Create.Options |= (disposition << 24);

		ndfsWinxpRequestHeader->Create.Options &= ~FILE_DELETE_ON_CLOSE;

		if (FlagOn(ccb->Flags, CCB_FLAG_DELETE_ON_CLOSE)) {

			DebugTrace2( 0, Dbg2, ("RecoverSession: CCB_FLAG_DELETE_ON_CLOSE fcb = %p, ccb->Scb->FullPathName = %wZ \n", 
								   ccb->Fcb, &ccb->Fcb->FullFileName) );

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


		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

		RtlCopyMemory( ndfsWinxpRequestData,
					   ccb->Buffer,
					   ndfsWinxpRequestHeader->Create.EaLength );

		RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength,
					   ccb->Fcb->FullFileName.Buffer,
					   ccb->Fcb->FullFileName.Length );

		RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength + ccb->Fcb->FullFileName.Length,
					   ccb->Buffer + ccb->CreateContext.EaLength,
					   ccb->BufferLength - ccb->CreateContext.EaLength );

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(Secondary, secondaryRequest);
				
		timeOut.QuadPart = -NDASFAT_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if (status != STATUS_SUCCESS) {
		
			ASSERT( NDASFAT_BUG );

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
		
		DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

		if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

			LONG		ccbCount;
			PLIST_ENTRY	ccbListEntry;

			for (ccbCount = 0, ccbListEntry = ccb->Fcb->NonPaged->CcbQueue.Flink; 
				ccbListEntry != &ccb->Fcb->NonPaged->CcbQueue; 
				ccbListEntry = ccbListEntry->Flink, ccbCount++);

			SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED | ND_FAT_CCB_FLAG_CORRUPTED );
			ccb->Fcb->CorruptedCcbCloseCount ++;

			DebugTrace2( 0, Dbg2, ("RecoverSession: ccb->Lcb->ExactCaseLink.LinkName = %wZ Corrupted Status = %x scb->CorruptedCcbCloseCount = %d, scb->CloseCount = %d\n",
								   &ccb->Fcb->FullFileName, NTOHL(ndfsWinxpReplytHeader->Status4), ccb->Fcb->CorruptedCcbCloseCount, ccb->Fcb->OpenCount));

			if (ccb->Fcb->OpenCount == ccb->Fcb->CorruptedCcbCloseCount)
				SetFlag( ccb->Fcb->NdasFatFlags, ND_FAT_FCB_FLAG_CORRUPTED );

			DereferenceSecondaryRequest( secondaryRequest );
			
			continue;
		}

		ccb->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
		ccb->Fcb->Handle = ndfsWinxpReplytHeader->Open.FcbHandle;

		//ccb->Lcb->SecondaryScb = (PSCB)ndfsWinxpReplytHeader->Open.Lcb.Scb;

#if 0
		if (scb->Fcb->FileReference.SegmentNumberHighPart != ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart ||
			scb->Fcb->FileReference.SegmentNumberLowPart  != ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart	||
			scb->Fcb->FileReference.SequenceNumber		  != ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber) {
		
			FILE_REFERENCE		fileReference;
			FCB_TABLE_ELEMENT	key;
			PVOID				nodeOrParent;
			TABLE_SEARCH_RESULT searchResult;
					
			DebugTrace2( 0, Dbg2, ("FileReference is Different scb->Fcb = %p\n", scb->Fcb) );

			fileReference.SegmentNumberHighPart = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart;
			fileReference.SegmentNumberLowPart  = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart;
			fileReference.SequenceNumber		= ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber;
					
			key.FileReference = fileReference;

			if (RtlLookupElementGenericTableFull(&scb->Fcb->Vcb->SecondaryFcbTable, &key, &nodeOrParent, &searchResult) != NULL) {

				ASSERT( FALSE );
				
			} else {
					
				FatDeleteFcbTableEntry( scb->Fcb, scb->Fcb->Vcb, scb->Fcb->FileReference );
				scb->Fcb->FileReference = fileReference;
				FatInsertFcbTableEntryFull( IrpContext, scb->Fcb->Vcb, scb->Fcb, fileReference, nodeOrParent, searchResult );
			}
		}

#endif

		if (!FlagOn(Secondary->VolDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_DIRECT_RW)) {

			goto next_step;
		}			

		if (ccb->Fcb->Header.AllocationSize.QuadPart == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {
		
			if (ccb->Fcb->Header.FileSize.LowPart == NTOHLL(ndfsWinxpReplytHeader->FileSize8)) {

				DereferenceSecondaryRequest( secondaryRequest );
				SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_REOPENED );
			
			} else {

				DereferenceSecondaryRequest( secondaryRequest );

				secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_SET_INFORMATION, 0 );

				if (secondaryRequest == NULL) {

					NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
					status = STATUS_INSUFFICIENT_RESOURCES;	
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
												NDFS_COMMAND_EXECUTE, 
												Secondary, 
												IRP_MJ_SET_INFORMATION, 
												Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)ccb;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

				ndfsWinxpRequestHeader->IrpFlags4   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;
				ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
				ndfsWinxpRequestHeader->SetFile.Length					= 0;

				ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
				ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
				ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
				ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

				ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = ccb->Fcb->Header.FileSize.LowPart;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASFAT_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {
	
					ASSERT( NDASFAT_BUG );
					secondaryRequest = NULL;
					break;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					DereferenceSecondaryRequest( secondaryRequest );
					status = STATUS_CANT_WAIT;	
					break;
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				ASSERT( NT_SUCCESS(NTOHL(ndfsWinxpReplytHeader->Status4)) );

				DereferenceSecondaryRequest( secondaryRequest );

				SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_REOPENED );
			}

		} else {

			BOOLEAN				lookupResut;
			VBO					vcn;
			LBO					lcn;
			//LCN				startingLcn;
			ULONG				clusterCount;


			DereferenceSecondaryRequest( secondaryRequest );

			DebugTrace2( 0, Dbg2, ("RecoverSession: fcb->Header.AllocationSize.QuadPart != ndfsWinxpReplytHeader->AllocationSize\n") );

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_SET_INFORMATION, 0 );

			if (secondaryRequest == NULL) {

				NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
				status = STATUS_INSUFFICIENT_RESOURCES;	
				break;
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
											NDFS_COMMAND_EXECUTE, 
											Secondary, 
											IRP_MJ_SET_INFORMATION, 
											Secondary->Thread.SessionContext.SecondaryMaxDataSize );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)ccb;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
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

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			if (status != STATUS_SUCCESS) {
	
				ASSERT( NDASFAT_BUG );
				secondaryRequest = NULL;
				break;
			}

			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				DereferenceSecondaryRequest( secondaryRequest );
				status = STATUS_CANT_WAIT;	
				break;
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			ASSERT( NT_SUCCESS(NTOHL(ndfsWinxpReplytHeader->Status4)) );

			DereferenceSecondaryRequest( secondaryRequest );

			vcn = 0;

			while (vcn < ccb->Fcb->Header.AllocationSize.QuadPart) {

				lookupResut = FatLookupMcbEntry( &Secondary->VolDo->Vcb, &ccb->Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

				DebugTrace2( 0, Dbg2, ("RecoverSession: vcn = %d, clusterCount = %d\n", vcn, clusterCount) );

				if (lookupResut == FALSE || !((vcn + clusterCount) <= ccb->Fcb->Header.AllocationSize.QuadPart)) {

					ASSERT( FALSE );
					break;
				}

				secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_SET_INFORMATION, 0 );

				if (secondaryRequest == NULL) {

					NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
					status = STATUS_INSUFFICIENT_RESOURCES;	
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
												NDFS_COMMAND_EXECUTE, 
												Secondary, 
												IRP_MJ_SET_INFORMATION, 
												Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)ccb;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

				ndfsWinxpRequestHeader->IrpFlags4   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileAllocationInformation;
				ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
				ndfsWinxpRequestHeader->SetFile.Length					= 0;

				ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
				ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
				ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
				ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

				ndfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize = vcn + clusterCount;
				ndfsWinxpRequestHeader->SetFile.AllocationInformation.Lcn			 = lcn;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASFAT_TIME_OUT;	
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {
	
					ASSERT( NDASFAT_BUG );
					secondaryRequest = NULL;
					break;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					DereferenceSecondaryRequest( secondaryRequest );
					status = STATUS_CANT_WAIT;	
					break;
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

					ASSERT( FALSE );
					DereferenceSecondaryRequest( secondaryRequest );
					break;
				}
 
#if DBG
				if (vcn + clusterCount == ccb->Fcb->Header.AllocationSize.QuadPart) {

					PNDFS_FAT_MCB_ENTRY	mcbEntry;
					ULONG			index;
					VBO				testVcn;

					BOOLEAN			lookupResut2;
					VBO				vcn2;
					LBO				lcn2;
					//LCN			startingLcn2;
					ULONG			clusterCount2;

					ASSERT( NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) == ccb->Fcb->Header.AllocationSize.QuadPart );
					mcbEntry = (PNDFS_FAT_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

					for (index=0, testVcn= 0, vcn2=0; index < NTOHL(ndfsWinxpReplytHeader->NumberOfMcbEntry4); index++, mcbEntry++) {

						ASSERT( mcbEntry->Vcn == testVcn );
						testVcn += (ULONG)mcbEntry->ClusterCount;

						lookupResut2 = FatLookupMcbEntry( &Secondary->VolDo->Vcb, &ccb->Fcb->Mcb, vcn2, &lcn2, &clusterCount2, NULL );
					
						ASSERT( lookupResut2 == TRUE );
						//ASSERT( startingLcn2 == lcn2 );
						ASSERT( vcn2 == mcbEntry->Vcn );
						ASSERT( lcn2 == (((LBO)mcbEntry->Lcn) << Secondary->VolDo->Vcb.AllocationSupport.LogOfBytesPerSector) );
						ASSERT( clusterCount2 == mcbEntry->ClusterCount );

						vcn2 += clusterCount2;
					}
				}

#endif
				DereferenceSecondaryRequest( secondaryRequest );

				vcn += clusterCount;
			} 

			if (vcn == ccb->Fcb->Header.AllocationSize.QuadPart) {

				secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_SET_INFORMATION, 0 );

				if (secondaryRequest == NULL) {

					NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
					status = STATUS_INSUFFICIENT_RESOURCES;	
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
												NDFS_COMMAND_EXECUTE, 
												Secondary, 
												IRP_MJ_SET_INFORMATION, 
												Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)ccb;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

				ndfsWinxpRequestHeader->IrpFlags4   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;
				ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
				ndfsWinxpRequestHeader->SetFile.Length					= 0;

				ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = 0;
				ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= 0;
				ndfsWinxpRequestHeader->SetFile.ClusterCount	= 0;
				ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0;

				ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = ccb->Fcb->Header.FileSize.LowPart;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASFAT_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {
	
					ASSERT( NDASFAT_BUG );
					secondaryRequest = NULL;
					break;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					DereferenceSecondaryRequest( secondaryRequest );
					status = STATUS_CANT_WAIT;	
					break;
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				ASSERT( NT_SUCCESS(NTOHL(ndfsWinxpReplytHeader->Status4)) );

				DereferenceSecondaryRequest( secondaryRequest );
				
				SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_REOPENED );
			
			} else {

				LONG		ccbCount;
				PLIST_ENTRY	ccbListEntry;

				ClosePrimaryFile( Secondary, ccb->PrimaryFileHandle );

				for (ccbCount = 0, ccbListEntry = ccb->Fcb->NonPaged->CcbQueue.Flink; 
					 ccbListEntry != &ccb->Fcb->NonPaged->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink, ccbCount++);

				SetFlag( ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED | ND_FAT_CCB_FLAG_CORRUPTED );
				ccb->Fcb->CorruptedCcbCloseCount ++;

				DebugTrace2( 0, Dbg2, ("RecoverSession: ccb->Lcb->ExactCaseLink.LinkName = %wZ Corrupted Status = %x scb->CorruptedCcbCloseCount = %d, scb->CloseCount = %d\n",
									   &ccb->Fcb->FullFileName, NTOHL(ndfsWinxpReplytHeader->Status4), ccb->Fcb->CorruptedCcbCloseCount, ccb->Fcb->OpenCount));

				if (ccb->Fcb->OpenCount == ccb->Fcb->CorruptedCcbCloseCount)
					SetFlag( ccb->Fcb->NdasFatFlags, ND_FAT_FCB_FLAG_CORRUPTED );

				continue;
			}
		}
	}

next_step: NOTHING;

    for (ccblistEntry = Secondary->RecoveryCcbQueue.Blink; 
		 ccblistEntry != &Secondary->RecoveryCcbQueue; 
		 ccblistEntry = ccblistEntry->Blink) {

		//PSCB						scb;
		PCCB						ccb;
		
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		ccb = CONTAINING_RECORD( ccblistEntry, CCB, ListEntry );
	
		if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

			continue;
		}

		//scb = ccb->FileObject->FsContext;

		if (FlagOn(ccb->FileObject->Flags, FO_CLEANUP_COMPLETE)) {

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_CLEANUP, 0 );

			if (secondaryRequest == NULL) {

				NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
				status = STATUS_INSUFFICIENT_RESOURCES;	
				break;
			}

			DebugTrace2( 0, Dbg, ("RecoverSession: FO_CLEANUP_COMPLETE fcb = %p, ccb->Scb->FullPathName = %wZ \n", 
								  ccb->Fcb, &ccb->Fcb->FullFileName) );

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader, NDFS_COMMAND_EXECUTE, Secondary, IRP_MJ_CLEANUP, 0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)ccb;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CLEANUP;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

			ndfsWinxpRequestHeader->IrpFlags4   = 0; //ccb->IrpFlags;
			ndfsWinxpRequestHeader->IrpSpFlags = 0; //ccb->IrpSpFlags;

			ndfsWinxpRequestHeader->CleanUp.AllocationSize	= ccb->Fcb->Header.AllocationSize.QuadPart;
			ndfsWinxpRequestHeader->CleanUp.FileSize		= ccb->Fcb->Header.FileSize.LowPart;
			ndfsWinxpRequestHeader->CleanUp.ValidDataLength = ccb->Fcb->Header.ValidDataLength.QuadPart;
			ndfsWinxpRequestHeader->CleanUp.VaildDataToDisk = ccb->Fcb->ValidDataToDisk;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject(	&secondaryRequest->CompleteEvent,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );
	
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (status != STATUS_SUCCESS) {

				ASSERT( NDASFAT_BUG );
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
		}
	}

	DebugTrace2( 0, Dbg2, ("SecondaryRecoverySession: Completed. Secondary = %p, status = %x\n", Secondary, status) );

	ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

	//FatDebugTrace2Level = 0x00000009;

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	return status;
}

#endif
