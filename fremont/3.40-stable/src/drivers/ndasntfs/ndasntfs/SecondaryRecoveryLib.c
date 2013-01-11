#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__


extern ATTRIBUTE_DEFINITION_COLUMNS NtfsAttributeDefinitions[];


#define BugCheckFileId                   (NTFS_BUG_CHECK_FLUSH)

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')


BOOLEAN
NdasNtfsDeleteVcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB *Vcb
    )

/*++

Routine Description:

    This routine removes the Vcb record from Ntfs's in-memory data
    structures.

Arguments:

    Vcb - Supplies the Vcb to be removed

Return Value:

    BOOLEAN - TRUE if the Vcb was deleted, FALSE otherwise.

--*/

{
    PVOLUME_DEVICE_OBJECT VolDo;
    BOOLEAN AcquiredFcb;
    PSCB Scb;
    PFCB Fcb;
    BOOLEAN VcbDeleted = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( *Vcb );

    ASSERTMSG("Cannot delete Vcb ", !FlagOn((*Vcb)->VcbState, VCB_STATE_VOLUME_MOUNTED));

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeleteVcb, *Vcb = %08lx\n", *Vcb) );

    //
    //  Remember the volume device object.
    //

    VolDo = CONTAINING_RECORD( *Vcb, VOLUME_DEVICE_OBJECT, Vcb );

    //
    //  Make sure that we can really delete the vcb
    //

    ASSERT( (*Vcb)->CloseCount == 0 );

    NtOfsPurgeSecurityCache( *Vcb );

    //
    //  If the Vcb log file object is present then we need to
    //  dereference it and uninitialize it through the cache.
    //

    if (((*Vcb)->LogFileObject != NULL) &&
        !FlagOn( (*Vcb)->CheckpointFlags, VCB_DEREFERENCED_LOG_FILE )) {

        CcUninitializeCacheMap( (*Vcb)->LogFileObject,
                                &Li0,
                                NULL );

        //
        //  Set a flag indicating that we are dereferencing the LogFileObject.
        //

        SetFlag( (*Vcb)->CheckpointFlags, VCB_DEREFERENCED_LOG_FILE );
        ObDereferenceObject( (*Vcb)->LogFileObject );
    }

	ExAcquireFastMutex( &CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->FastMutex );

	if ((*Vcb)->LogFileObject != NULL) {

		NTSTATUS		status;
		LARGE_INTEGER	timeout;

		SetFlag( CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->NdasNtfsFlags, NDAS_NTFS_DEVICE_FLAG_WAIT_CLOSE_LOGFILE );

		ExReleaseFastMutex( &CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->FastMutex );

		timeout.QuadPart = -NDASNTFS_TIME_OUT;

		status = KeWaitForSingleObject( &CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->LogFileCloseEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeout );

		NDAS_ASSERT( status == STATUS_SUCCESS );
		NDAS_ASSERT( (*Vcb)->LogFileObject == NULL );

		KeClearEvent( &CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->LogFileCloseEvent );

		ClearFlag( CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->NdasNtfsFlags, NDAS_NTFS_DEVICE_FLAG_WAIT_CLOSE_LOGFILE );
	
	} else {

		ExReleaseFastMutex( &CONTAINING_RECORD((*Vcb), VOLUME_DEVICE_OBJECT, Vcb)->FastMutex );
	}

    //
    //  Only proceed if the log file object went away.  In the typical case the
    //  close will come in through a recursive call from the ObDereference call
    //  above.
    //

    if ((*Vcb)->LogFileObject == NULL) {

        //
        //  If the OnDiskOat is not the same as the embedded table then
        //  free the OnDisk table.
        //

        if (((*Vcb)->OnDiskOat != NULL) &&
            ((*Vcb)->OnDiskOat != &(*Vcb)->OpenAttributeTable)) {

            NtfsFreeRestartTable( (*Vcb)->OnDiskOat );
            NtfsFreePool( (*Vcb)->OnDiskOat );
            (*Vcb)->OnDiskOat = NULL;
        }

        //
        //  Uninitialize the Mcb's for the deallocated cluster Mcb's.
        //

        if ((*Vcb)->DeallocatedClusters1.Link.Flink == NULL) {
            FsRtlUninitializeLargeMcb( &(*Vcb)->DeallocatedClusters1.Mcb );
        }
        if ((*Vcb)->DeallocatedClusters2.Link.Flink == NULL) {
            FsRtlUninitializeLargeMcb( &(*Vcb)->DeallocatedClusters2.Mcb );
        }

        while (!IsListEmpty(&(*Vcb)->DeallocatedClusterListHead )) {

            PDEALLOCATED_CLUSTERS Clusters;

            Clusters = (PDEALLOCATED_CLUSTERS) RemoveHeadList( &(*Vcb)->DeallocatedClusterListHead );
            FsRtlUninitializeLargeMcb( &Clusters->Mcb );
            if ((Clusters != &((*Vcb)->DeallocatedClusters2)) &&
                (Clusters != &((*Vcb)->DeallocatedClusters1))) {

                NtfsFreePool( Clusters );
            }
        }

        //
        //  Clean up the Root Lcb if present.
        //

        if ((*Vcb)->RootLcb != NULL) {

            //
            //  Cleanup the Lcb so the DeleteLcb routine won't look at any
            //  other structures.
            //

            InitializeListHead( &(*Vcb)->RootLcb->ScbLinks );
            InitializeListHead( &(*Vcb)->RootLcb->FcbLinks );
            ClearFlag( (*Vcb)->RootLcb->LcbState,
                       LCB_STATE_EXACT_CASE_IN_TREE | LCB_STATE_IGNORE_CASE_IN_TREE );

            NtfsDeleteLcb( IrpContext, &(*Vcb)->RootLcb );
            (*Vcb)->RootLcb = NULL;
        }

        //
        //  Make sure the Fcb table is completely emptied.  It is possible that an occasional Fcb
        //  (along with its Scb) will not be deleted when the file object closes come in.
        //

        while (TRUE) {

            PVOID RestartKey;

            //
            //  Always reinitialize the search so we get the first element in the tree.
            //

            RestartKey = NULL;
            NtfsAcquireFcbTable( IrpContext, *Vcb );
            Fcb = NtfsGetNextFcbTableEntry( *Vcb, &RestartKey );
            NtfsReleaseFcbTable( IrpContext, *Vcb );

            if (Fcb == NULL) { break; }

            while ((Scb = NtfsGetNextChildScb( Fcb, NULL )) != NULL) {

                NtfsDeleteScb( IrpContext, &Scb );
            }

            NtfsAcquireFcbTable( IrpContext, *Vcb );
            NtfsDeleteFcb( IrpContext, &Fcb, &AcquiredFcb );
        }

        //
        //  Free the upcase table and attribute definitions.  The upcase
        //  table only gets freed if it is not the global table.
        //

        if (((*Vcb)->UpcaseTable != NULL) && ((*Vcb)->UpcaseTable != NtfsData.UpcaseTable)) {

            NtfsFreePool( (*Vcb)->UpcaseTable );
        }

        (*Vcb)->UpcaseTable = NULL;

        if (((*Vcb)->AttributeDefinitions != NULL) &&
            ((*Vcb)->AttributeDefinitions != NtfsAttributeDefinitions)) {

            NtfsFreePool( (*Vcb)->AttributeDefinitions );
            (*Vcb)->AttributeDefinitions = NULL;
        }

        //
        //  Free the device name string if present.
        //

        if ((*Vcb)->DeviceName.Buffer != NULL) {

            NtfsFreePool( (*Vcb)->DeviceName.Buffer );
            (*Vcb)->DeviceName.Buffer = NULL;
        }

        FsRtlNotifyUninitializeSync( &(*Vcb)->NotifySync );

        //
        //  We will free the structure allocated for the Lfs handle.
        //

        LfsDeleteLogHandle( (*Vcb)->LogHandle );
        (*Vcb)->LogHandle = NULL;

        //
        //  Delete the vcb resource and also free the restart tables
        //

        //
        //  Empty the list of OpenAttribute Data.
        //

        NtfsFreeAllOpenAttributeData( *Vcb );

        NtfsFreeRestartTable( &(*Vcb)->OpenAttributeTable );
        NtfsFreeRestartTable( &(*Vcb)->TransactionTable );

        //
        //  The Vpb in the Vcb may be a temporary Vpb and we should free it here.
        //

        if (FlagOn( (*Vcb)->VcbState, VCB_STATE_TEMP_VPB )) {

			ASSERT( FALSE );

            NtfsFreePool( (*Vcb)->Vpb );
            (*Vcb)->Vpb = NULL;
        }

        //
        //  Uninitialize the hash table.
        //

        NtfsUninitializeHashTable( &(*Vcb)->HashTable );

        ExDeleteResourceLite( &(*Vcb)->Resource );
        ExDeleteResourceLite( &(*Vcb)->MftFlushResource );

        //
        //  Delete the space used to store performance counters.
        //

        if ((*Vcb)->Statistics != NULL) {
            NtfsFreePool( (*Vcb)->Statistics );
            (*Vcb)->Statistics = NULL;
        }

        //
        //  Tear down the file property tunneling structure
        //

        FsRtlDeleteTunnelCache(&(*Vcb)->Tunnel);

#ifdef NTFS_CHECK_BITMAP
        if ((*Vcb)->BitmapCopy != NULL) {

            ULONG Count = 0;

            while (Count < (*Vcb)->BitmapPages) {

                if (((*Vcb)->BitmapCopy + Count)->Buffer != NULL) {

                    NtfsFreePool( ((*Vcb)->BitmapCopy + Count)->Buffer );
                }

                Count += 1;
            }

            NtfsFreePool( (*Vcb)->BitmapCopy );
            (*Vcb)->BitmapCopy = NULL;
        }
#endif

#if 0
        //
        // Drop the reference on the target device object
        //

        ObDereferenceObject( (*Vcb)->TargetDeviceObject );
#endif

        //
        //  Check that the Usn queues are empty.
        //

        ASSERT( IsListEmpty( &(*Vcb)->NotifyUsnDeleteIrps ));
        ASSERT( IsListEmpty( &(*Vcb)->ModifiedOpenFiles ));
        ASSERT( IsListEmpty( &(*Vcb)->TimeOutListA ));
        ASSERT( IsListEmpty( &(*Vcb)->TimeOutListB ));

        //
        //  Unnitialize the cached runs.
        //

        NtfsUninitializeCachedRuns( &(*Vcb)->CachedRuns );

        //
        //  Free any spare Vpb we might have stored in the Vcb.
        //

        if ((*Vcb)->SpareVpb != NULL) {

            NtfsFreePool( (*Vcb)->SpareVpb );
            (*Vcb)->SpareVpb = NULL;
        }

        //
        //  Return the Vcb (i.e., the VolumeDeviceObject) to pool and null out
        //  the input pointer to be safe
        //

#if 0
        IoDeleteDevice( (PDEVICE_OBJECT)VolDo );

        *Vcb = NULL;
#endif
        VcbDeleted = TRUE;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NdasNtfsDeleteVcb -> VOID\n") );

    return VcbDeleted;
}



NTSTATUS
CleanUpVcb( 
	IN PIRP_CONTEXT	IrpContext,
	IN PVCB			Vcb
	)
{
    NTSTATUS				Status;
    NTSTATUS				FlushStatus;
    //PIO_STACK_LOCATION	IrpSp;
    //PVOLUME_DEVICE_OBJECT	OurDeviceObject;

	//PVCB					Vcb;
    BOOLEAN					VcbAcquired = FALSE;
    BOOLEAN					CheckpointAcquired = FALSE;
    BOOLEAN					DecrementCloseCount = FALSE;
    
#ifdef SYSCACHE_DEBUG
    ULONG SystemHandleCount = 0;
#endif

	DebugTrace( 0, Dbg, ("RestoreVcb\n") );

	ASSERT_IRP_CONTEXT( IrpContext );
	//ASSERT_IRP( *Irp );

	ASSERT( FlagOn( IrpContext->TopLevelIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL ));


	//IrpSp = IoGetCurrentIrpStackLocation( *Irp );

    //OurDeviceObject = (PVOLUME_DEVICE_OBJECT) IrpSp->DeviceObject;
	//Vcb = &OurDeviceObject->Vcb;

	DebugTrace( 0, Dbg2, ("Vcb->State = %X\n", Vcb->VcbState) );
	ASSERT( Vcb->CloseCount == Vcb->SystemFileCloseCount );

	try {
	
		NtfsAcquireCheckpointSynchronization( IrpContext, Vcb );
	    CheckpointAcquired = TRUE;
        
		NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
	    VcbAcquired = TRUE;
    
		if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

			Status = STATUS_VOLUME_DISMOUNTED;
			leave;
		} 
      
#ifdef SYSCACHE_DEBUG
		if (Vcb->SyscacheScb != NULL) {
		
			SystemHandleCount = Vcb->SyscacheScb->CleanupCount;
		}
	    if ((Vcb->CleanupCount > SystemHandleCount) ||
#else
		if ((Vcb->CleanupCount > 0) ||
#endif
			FlagOn(Vcb->VcbState, VCB_STATE_DISALLOW_DISMOUNT)) {

	        DebugTrace( 0, Dbg2, ("RestoreVcb --> cleanup count still %x \n", Vcb->CleanupCount) );
            
			Status = STATUS_UNSUCCESSFUL;
                
		} else {

			FlushStatus = NtfsFlushVolume( IrpContext,
				                           Vcb,
					                       TRUE,
						                   TRUE,
							               TRUE,
								           FALSE );

			Vcb->CloseCount += 1;
		    DecrementCloseCount = TRUE;
                
			//NtfsReleaseVcb( IrpContext, Vcb );
	        //CcWaitForCurrentLazyWriterActivity();
		    //NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

			Vcb->CloseCount -= 1;
	        DecrementCloseCount = FALSE;

			if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

				Status = STATUS_VOLUME_DISMOUNTED;
				leave;
			} 

#ifdef SYSCACHE_DEBUG
			if (Vcb->SyscacheScb != NULL) {
				SystemHandleCount = Vcb->SyscacheScb->CleanupCount;
			}
                
	        if ((Vcb->CleanupCount > SystemHandleCount) ||
#else
		    if ((Vcb->CleanupCount > 0) ||
#endif
				FlagOn(Vcb->VcbState, VCB_STATE_DISALLOW_DISMOUNT)) {

				Status = STATUS_UNSUCCESSFUL;
				leave;
			}

            if ((Vcb->CloseCount - (Vcb->SystemFileCloseCount + Vcb->QueuedCloseCount)) > 0) {

				DebugTrace( 0, Dbg2, ("IRP_MN_QUERY_REMOVE_DEVICE --> %x user files still open \n", (Vcb->CloseCount - Vcb->SystemFileCloseCount)) );
                
				Status = STATUS_UNSUCCESSFUL;
                    
			} else {

				ULONG Retrying = 1;
                    
                DebugTrace( 0, Dbg, ("IRP_MN_QUERY_REMOVE_DEVICE --> No user files, Locking volume \n") );
                
                Status = NtfsLockVolumeInternal( IrpContext, 
                                                 Vcb, 
                                                 ((PFILE_OBJECT) 1),
                                                 &Retrying );

				if (NT_SUCCESS( Status )) {

					//NDAS_ASSERT( Vcb->CloseCount == 0 );
                    ASSERT_EXCLUSIVE_RESOURCE( &Vcb->Resource );
                    //SetFlag( Vcb->VcbState, VCB_STATE_TARGET_DEVICE_STOPPED );
                }
            }

			//ClearFlag( Vcb->VcbState, VCB_STATE_TARGET_DEVICE_STOPPED );

			if (FlagOn( Vcb->VcbState, VCB_STATE_EXPLICIT_LOCK )) {

				//NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE, NULL );

				ClearFlag( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_EXPLICIT_LOCK );
				Vcb->FileObjectWithVcbLocked = NULL;
			}

			if (NT_SUCCESS( Status )) {

				BOOLEAN VcbDeleted;

				NtfsAcquireExclusiveGlobal( IrpContext, TRUE );
				RemoveEntryList( &Vcb->VcbLinks );
				NtfsReleaseGlobal( IrpContext );

				VcbDeleted = NdasNtfsDeleteVcb( IrpContext, &Vcb );
				NDAS_ASSERT( VcbDeleted == TRUE );
			}
		}

	} finally {
            
		if (DecrementCloseCount) {

			if (!VcbAcquired) {

				NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
                VcbAcquired = TRUE;
            }
            
            Vcb->CloseCount -= 1;
        }

		if (VcbAcquired) {

			NtfsReleaseVcbCheckDelete( IrpContext, Vcb, IrpContext->MajorFunction, NULL );
		}

		if (CheckpointAcquired) {
            NtfsReleaseCheckpointSynchronization( IrpContext, Vcb );
        }
	}

	DebugTrace( 0, Dbg2, ("Vcb->State = %X\n", Vcb->VcbState) );
	//DbgBreakPoint();

	return Status;
}

RTL_GENERIC_COMPARE_RESULTS
NtfsFcbTableCompare (
    IN PRTL_GENERIC_TABLE FcbTable,
    IN PVOID FirstStruct,
    IN PVOID SecondStruct
    );

VOID
NdasNtfsInitializeVcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB Vcb,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb
    )

/*++

Routine Description:

    This routine initializes and inserts a new Vcb record into the in-memory
    data structure.  The Vcb record "hangs" off the end of the Volume device
    object and must be allocated by our caller.

Arguments:

    Vcb - Supplies the address of the Vcb record being initialized.

    TargetDeviceObject - Supplies the address of the target device object to
        associate with the Vcb record.

    Vpb - Supplies the address of the Vpb to associate with the Vcb record.

Return Value:

    None.

--*/

{
    ULONG i;
    ULONG NumberProcessors;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeVcb, Vcb = %08lx\n", Vcb) );

    //
    //  First zero out the Vcb
    //

    RtlZeroMemory( Vcb, FIELD_OFFSET(VCB, NdasNtfsFlags) );

    //
    //  Set the node type code and size
    //

    Vcb->NodeTypeCode = NTFS_NTC_VCB;
    Vcb->NodeByteSize = sizeof(VCB);

    //
    //  Set the following Vcb flags before putting the Vcb in the
    //  Vcb queue.  This will lock out checkpoints until the
    //  volume is mounted.
    //

    SetFlag( Vcb->CheckpointFlags,
             VCB_CHECKPOINT_IN_PROGRESS |
             VCB_LAST_CHECKPOINT_CLEAN);

    //
    //  Insert this vcb record into the vcb queue off of the global data
    //  record
    //

    InsertTailList( &NtfsData.VcbQueue, &Vcb->VcbLinks );

    //
    //  Set the target device object and vpb fields
    //

    ObReferenceObject( TargetDeviceObject );
    Vcb->TargetDeviceObject = TargetDeviceObject;
    Vcb->Vpb = Vpb;

    //
    //  Set the state and condition fields.  The removable media flag
    //  is set based on the real device's characteristics.
    //

    if (FlagOn(Vpb->RealDevice->Characteristics, FILE_REMOVABLE_MEDIA)) {

        SetFlag( Vcb->VcbState, VCB_STATE_REMOVABLE_MEDIA );
    }

    SetFlag( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED );

    //
    //  Initialized the ModifiedOpenFilesListhead and the delete notify queue.
    //

    InitializeListHead( &Vcb->NotifyUsnDeleteIrps );
    InitializeListHead( &Vcb->ModifiedOpenFiles );
    InitializeListHead( &Vcb->TimeOutListA );
    InitializeListHead( &Vcb->TimeOutListB );

    Vcb->CurrentTimeOutFiles = &Vcb->TimeOutListA;
    Vcb->AgedTimeOutFiles = &Vcb->TimeOutListB;

    //
    //  Initialize list of OpenAttribute structures.
    //

    InitializeListHead( &Vcb->OpenAttributeData );

    //
    //  Initialize list of deallocated clusters
    //

    InitializeListHead( &Vcb->DeallocatedClusterListHead );

    //
    //  Initialize the synchronization objects in the Vcb.
    //

    ExInitializeResourceLite( &Vcb->Resource );
    ExInitializeResourceLite( &Vcb->MftFlushResource );

    ExInitializeFastMutex( &Vcb->FcbTableMutex );
    ExInitializeFastMutex( &Vcb->FcbSecurityMutex );
    ExInitializeFastMutex( &Vcb->ReservedClustersMutex );
    ExInitializeFastMutex( &Vcb->HashTableMutex );
    ExInitializeFastMutex( &Vcb->CheckpointMutex );

    KeInitializeEvent( &Vcb->CheckpointNotifyEvent, NotificationEvent, TRUE );

    //
    //  Initialize the Fcb Table
    //

    RtlInitializeGenericTable( &Vcb->FcbTable,
                               NtfsFcbTableCompare,
                               NtfsAllocateFcbTableEntry,
                               NtfsFreeFcbTableEntry,
                               NULL );

    //
    //  Initialize the property tunneling structure
    //

    FsRtlInitializeTunnelCache(&Vcb->Tunnel);


#ifdef BENL_DBG
    InitializeListHead( &(Vcb->RestartRedoHead) );
    InitializeListHead( &(Vcb->RestartUndoHead) );
#endif

    //
    //  Possible calls that might fail begins here
    //

    //
    //  Initialize the list head and mutex for the dir notify Irps.
    //  Also the rename resource.
    //

    InitializeListHead( &Vcb->DirNotifyList );
    InitializeListHead( &Vcb->ViewIndexNotifyList );
    FsRtlNotifyInitializeSync( &Vcb->NotifySync );

    //
    //  Allocate and initialize struct array for performance data.  This
    //  attempt to allocate could raise STATUS_INSUFFICIENT_RESOURCES.
    //

    NumberProcessors = KeNumberProcessors;
    Vcb->Statistics = NtfsAllocatePool( NonPagedPool,
                                         sizeof(FILE_SYSTEM_STATISTICS) * NumberProcessors );

    RtlZeroMemory( Vcb->Statistics, sizeof(FILE_SYSTEM_STATISTICS) * NumberProcessors );

    for (i = 0; i < NumberProcessors; i += 1) {
        Vcb->Statistics[i].Common.FileSystemType = FILESYSTEM_STATISTICS_TYPE_NTFS;
        Vcb->Statistics[i].Common.Version = 1;
        Vcb->Statistics[i].Common.SizeOfCompleteStructure =
            sizeof(FILE_SYSTEM_STATISTICS);
    }

    //
    //  Initialize the cached runs.
    //

    NtfsInitializeCachedRuns( &Vcb->CachedRuns );

    //
    //  Initialize the hash table.
    //

    NtfsInitializeHashTable( &Vcb->HashTable );

    //
    //  Allocate a spare Vpb for the dismount case.
    //

    Vcb->SpareVpb = NtfsAllocatePoolWithTag( NonPagedPool, sizeof( VPB ), 'VftN' );

    //
    //  Capture the current change count in the device we talk to.
    //

    if (FlagOn( Vcb->VcbState, VCB_STATE_REMOVABLE_MEDIA )) {

        ULONG ChangeCount = 0;

        NtfsDeviceIoControlAsync( IrpContext,
                                  Vcb->TargetDeviceObject,
                                  IOCTL_DISK_CHECK_VERIFY,
                                  (PVOID) &ChangeCount,
                                  sizeof( ChangeCount ));

        //
        //  Ignore any error for now.  We will see it later if there is
        //  one.
        //

        Vcb->DeviceChangeCount = ChangeCount;
    }

    //
    //  Set the dirty page table hint to its initial value
    //

    Vcb->DirtyPageTableSizeHint = INITIAL_DIRTY_TABLE_HINT;

    //
    //  Initialize the recently deallocated cluster mcbs and put the 1st one on the list.
    //

    FsRtlInitializeLargeMcb( &Vcb->DeallocatedClusters1.Mcb, PagedPool );
    FsRtlInitializeLargeMcb( &Vcb->DeallocatedClusters2.Mcb, PagedPool );

    Vcb->DeallocatedClusters1.Lsn.QuadPart = 0;
    InsertHeadList( &Vcb->DeallocatedClusterListHead, &Vcb->DeallocatedClusters1.Link );

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsInitializeVcb -> VOID\n") );

    return;
}



extern BOOLEAN NtfsDisable;
extern BOOLEAN NtfsDefragMftEnabled;
extern ULONG SkipNtOfs;
extern BOOLEAN NtfsForceUpgrade;


NTSTATUS
NtfsUpdateAttributeTable (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
NtfsVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsUserFsRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsLockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsUnlockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsIsVolumeMounted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsDirtyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );


BOOLEAN
NtfsGetDiskGeometry (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT RealDevice,
    IN PDISK_GEOMETRY DiskGeometry,
    IN PLONGLONG Length
    );

VOID
NtfsReadBootSector (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PSCB *BootScb,
    OUT PBCB *BootBcb,
    OUT PVOID *BootSector
    );

BOOLEAN
NtfsIsBootSectorNtfs (
    IN PPACKED_BOOT_SECTOR BootSector,
    IN PVCB Vcb
    );

VOID
NtfsGetVolumeInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PVPB Vpb OPTIONAL,
    IN PVCB Vcb,
    OUT PUSHORT VolumeFlags
    );

VOID
NtfsSetAndGetVolumeTimes (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN MarkDirty,
    IN BOOLEAN UpdateInTransaction
    );

VOID
NtfsOpenSystemFile (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB *Scb,
    IN PVCB Vcb,
    IN ULONG FileNumber,
    IN LONGLONG Size,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN BOOLEAN ModifiedNoWrite
    );

VOID
NtfsOpenRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
NtfsQueryRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

VOID
NtfsChangeAttributeCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PVCB Vcb,
    IN PCCB Ccb,
    IN USHORT CompressionState
    );

NTSTATUS
NtfsSetCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsMarkAsSystemHive (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetStatistics (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

LONG
NtfsWriteRawExceptionFilter (
    IN PIRP_CONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    );

#define NtfsMapPageInBitmap(A,B,C,D,E,F) NtfsMapOrPinPageInBitmap(A,B,C,D,E,F,FALSE)

#define NtfsPinPageInBitmap(A,B,C,D,E,F) NtfsMapOrPinPageInBitmap(A,B,C,D,E,F,TRUE)

VOID
NtfsMapOrPinPageInBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN Lcn,
    OUT PLCN StartingLcn,
    IN OUT PRTL_BITMAP Bitmap,
    OUT PBCB *BitmapBcb,
    IN BOOLEAN AlsoPinData
    );

#define BYTES_PER_PAGE (PAGE_SIZE)
#define BITS_PER_PAGE (BYTES_PER_PAGE * 8)

NTSTATUS
NtfsGetVolumeData (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetVolumeBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetMftRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsIsVolumeDirty (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsSetExtendedDasdIo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsCreateUsnJournal (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsReadFileRecordUsnData (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsReadFileUsnData (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsWriteUsnCloseRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsReadUsnWorker (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    );

NTSTATUS
NtfsBulkSecurityIdCheck (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

VOID
NtfsInitializeSecurityFile (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsUpgradeSecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsInitializeQuotaFile (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsInitializeObjectIdFile (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsInitializeReparseFile (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsInitializeUsnJournal (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG CreateIfNotExist,
    IN ULONG Restamp,
    IN PCREATE_USN_JOURNAL_DATA NewJournalData
    );

VOID
NtfsInitializeExtendDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
NtfsQueryAllocatedRanges (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsSetSparse (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsZeroRange (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsSetReparsePoint (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp );

NTSTATUS
NtfsGetReparsePoint (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp );

NTSTATUS
NtfsDeleteReparsePoint (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp );

NTSTATUS
NtfsEncryptionFsctl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsSetEncryption (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsReadRawEncrypted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsWriteRawEncrypted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsFindFilesOwnedBySid (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsFindBySidWorker (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    );

NTSTATUS
NtfsExtendVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsMarkHandle (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsPrefetchFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

LONG
NtfsFsctrlExceptionFilter (
    IN PIRP_CONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer,
    IN BOOLEAN AccessingUserData,
    IN OUT PNTSTATUS Status
    );

#ifdef BRIANDBG
LONG
NtfsDismountExceptionFilter (
    IN PEXCEPTION_POINTERS ExceptionPointer
    );
#endif

#ifdef SYSCACHE_DEBUG
VOID
NtfsInitializeSyscacheLogFile (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );
#endif


NTSTATUS
NdasNtfsMountVolume (
    IN PIRP_CONTEXT IrpContext,
	IN PVCB			Vcb
    )

/*++

Routine Description:

    This routine performs the mount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

    Its job is to verify that the volume denoted in the IRP is an NTFS volume,
    and create the VCB and root SCB/FCB structures.  The algorithm it uses is
    essentially as follows:

    1. Create a new Vcb Structure, and initialize it enough to do cached
       volume file I/O.

    2. Read the disk and check if it is an NTFS volume.

    3. If it is not an NTFS volume then free the cached volume file, delete
       the VCB, and complete the IRP with STATUS_UNRECOGNIZED_VOLUME

    4. Check if the volume was previously mounted and if it was then do a
       remount operation.  This involves freeing the cached volume file,
       delete the VCB, hook in the old VCB, and complete the IRP.

    5. Otherwise create a root SCB, recover the volume, create Fsp threads
       as necessary, and complete the IRP.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
#if 0
    PIO_STACK_LOCATION IrpSp;
#endif

    PATTRIBUTE_RECORD_HEADER Attribute;

    PDEVICE_OBJECT DeviceObjectWeTalkTo;
    PVPB Vpb;

    PVOLUME_DEVICE_OBJECT VolDo;
#if 0
    PVCB Vcb;
#endif

    PFILE_OBJECT RootDirFileObject = NULL;
    PBCB BootBcb = NULL;
    PPACKED_BOOT_SECTOR BootSector;
    PSCB BootScb = NULL;
    PSCB QuotaDataScb = NULL;

    POBJECT_NAME_INFORMATION DeviceObjectName = NULL;
    ULONG DeviceObjectNameLength;

    PBCB Bcbs[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    PMDL Mdls[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    ULONG FirstNonMirroredCluster;
    ULONG MirroredMftRange;

    PLIST_ENTRY MftLinks;
    PSCB AttributeListScb;

    ULONG i;

    IO_STATUS_BLOCK IoStatus;

    BOOLEAN UpdatesApplied;
    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN MountFailed = TRUE;
    BOOLEAN CloseAttributes = FALSE;
    BOOLEAN UpgradeVolume = FALSE;
    BOOLEAN WriteProtected;
    BOOLEAN CurrentVersion = FALSE;
    BOOLEAN UnrecognizedRestart;

    USHORT VolumeFlags = 0;

    LONGLONG LlTemp1;

    ASSERT_IRP_CONTEXT( IrpContext );
#if 0
    ASSERT_IRP( Irp );
#endif
    PAGED_CODE();

    //
    //**** The following code is only temporary and is used to disable NTFS
    //**** from mounting any volumes
    //

#if 0
    if (NtfsDisable) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_UNRECOGNIZED_VOLUME );
        return STATUS_UNRECOGNIZED_VOLUME;
    }
#endif

    //
    //  Reject floppies
    //

#if 0
    if (FlagOn( IoGetCurrentIrpStackLocation(Irp)->
                Parameters.MountVolume.Vpb->
                RealDevice->Characteristics, FILE_FLOPPY_DISKETTE ) ) {

        Irp->IoStatus.Information = 0;

        NtfsCompleteRequest( IrpContext, Irp, STATUS_UNRECOGNIZED_VOLUME );
        return STATUS_UNRECOGNIZED_VOLUME;
    }
#endif

    //
    //  Get the current Irp stack location
    //

#if 0
    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsMountVolume\n") );
#endif

    //
    //  Save some references to make our life a little easier
    //

#if 0
    DeviceObjectWeTalkTo = IrpSp->Parameters.MountVolume.DeviceObject;
    Vpb = IrpSp->Parameters.MountVolume.Vpb;
    ClearFlag( Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );
#else
	DeviceObjectWeTalkTo = Vcb->TargetDeviceObject;
	Vpb	= Vcb->Vpb;
#endif

    //
    //  TEMPCODE  Perform the following test for chkdsk testing.
    //

#if 0
    if (NtfsDisableDevice == IrpSp->Parameters.MountVolume.DeviceObject) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_UNRECOGNIZED_VOLUME );
        return STATUS_UNRECOGNIZED_VOLUME;
    }
#endif

    //
    //  Acquire exclusive global access
    //

    NtfsAcquireExclusiveGlobal( IrpContext, TRUE );

    //
    //  Now is a convenient time to look through the queue of Vcb's to see if there
    //  are any which can be deleted.
    //

#if 0
    try {

        PLIST_ENTRY Links;

        for (Links = NtfsData.VcbQueue.Flink;
             Links != &NtfsData.VcbQueue;
             Links = Links->Flink) {

            Vcb = CONTAINING_RECORD( Links, VCB, VcbLinks );

            if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED ) &&
                (Vcb->CloseCount == 0) &&
                FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT )) {

                //
                //  Now we can check to see if we should perform the teardown
                //  on this Vcb.  The release Vcb routine below can do all of
                //  the checks correctly.  Make this appear to from a close
                //  call since there is no special biasing for this case.
                //

                IrpContext->Vcb = Vcb;
                NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

                if (!FlagOn( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY )) {

                    NtfsReleaseGlobal( IrpContext );

                    NtfsReleaseVcbCheckDelete( IrpContext,
                                               Vcb,
                                               IRP_MJ_CLOSE,
                                               NULL );

                    //
                    //  Only do one since we have lost our place in the Vcb list.
                    //

                    NtfsAcquireExclusiveGlobal( IrpContext, TRUE );

                    break;

                } else {

                    NtfsReleaseVcb( IrpContext, Vcb );
                }
            }
        }

    } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  Make sure we own the global resource for mount.  We can only raise above
        //  in the DeleteVcb path when we don't hold the resource.
        //

        NtfsAcquireExclusiveGlobal( IrpContext, TRUE );
    }

    Vcb = NULL;

#endif

    try {

        PFILE_RECORD_SEGMENT_HEADER MftBuffer;
        PVOID Mft2Buffer;
        LONGLONG MftMirrorOverlap;

        //
        //  Create a new volume device object.  This will have the Vcb hanging
        //  off of its end, and set its alignment requirement from the device
        //  we talk to.
        //
#if 0

        if (!NT_SUCCESS(Status = IoCreateDevice( NtfsData.DriverObject,
                                                 sizeof(VOLUME_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
                                                 NULL,
                                                 FILE_DEVICE_DISK_FILE_SYSTEM,
                                                 0,
                                                 FALSE,
                                                 (PDEVICE_OBJECT *)&VolDo))) {

            try_return( Status );
        }

#else
		VolDo = CONTAINING_RECORD( Vcb, VOLUME_DEVICE_OBJECT, Vcb );

#endif

        //
        //  Our alignment requirement is the larger of the processor alignment requirement
        //  already in the volume device object and that in the DeviceObjectWeTalkTo
        //

        if (DeviceObjectWeTalkTo->AlignmentRequirement > VolDo->DeviceObject.AlignmentRequirement) {

            VolDo->DeviceObject.AlignmentRequirement = DeviceObjectWeTalkTo->AlignmentRequirement;
        }

        ClearFlag( VolDo->DeviceObject.Flags, DO_DEVICE_INITIALIZING );

        //
        //  Add one more to the stack size requirements for our device
        //

        VolDo->DeviceObject.StackSize = DeviceObjectWeTalkTo->StackSize + 1;

        //
        //  Initialize the overflow queue for the volume
        //

        VolDo->OverflowQueueCount = 0;
        InitializeListHead( &VolDo->OverflowQueue );
        KeInitializeEvent( &VolDo->OverflowQueueEvent, SynchronizationEvent, FALSE );

        //
        //  Get a reference to the Vcb hanging off the end of the volume device object
        //  we just created
        //

        IrpContext->Vcb = Vcb = &VolDo->Vcb;

        //
        //  Set the device object field in the vpb to point to our new volume device
        //  object
        //

        Vpb->DeviceObject = (PDEVICE_OBJECT)VolDo;

        //
        //  Initialize the Vcb.  Set checkpoint
        //  in progress (to prevent a real checkpoint from occuring until we
        //  are done).
        //

        NdasNtfsInitializeVcb( IrpContext, Vcb, DeviceObjectWeTalkTo, Vpb );
        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        VcbAcquired= TRUE;

        //
        //  Query the device we talk to for this geometry and setup enough of the
        //  vcb to read in the boot sectors.  This is a temporary setup until
        //  we've read in the actual boot sector and got the real cluster factor.
        //

        {
            DISK_GEOMETRY DiskGeometry;
            LONGLONG Length;
            ULONG BytesPerSector;

            WriteProtected = NtfsGetDiskGeometry( IrpContext,
                                                  DeviceObjectWeTalkTo,
                                                  &DiskGeometry,
                                                  &Length );

            //
            //  If the sector size is greater than the page size, it is probably
            //  a bogus return, but we cannot use the device.  We also verify that
            //  the sector size is a power of two.
            //

            BytesPerSector = DiskGeometry.BytesPerSector;

            if ((BytesPerSector > PAGE_SIZE) ||
                (BytesPerSector == 0)) {
                NtfsRaiseStatus( IrpContext, STATUS_BAD_DEVICE_TYPE, NULL, NULL );
            }

            while (TRUE) {

                if (FlagOn( BytesPerSector, 1 )) {

                    if (BytesPerSector != 1) {
                        NtfsRaiseStatus( IrpContext, STATUS_BAD_DEVICE_TYPE, NULL, NULL );
                    }
                    break;
                }

                BytesPerSector >>= 1;
            }

            Vcb->BytesPerSector = DiskGeometry.BytesPerSector;
            Vcb->BytesPerCluster = Vcb->BytesPerSector;
            Vcb->NumberSectors = Length / DiskGeometry.BytesPerSector;

            //
            //  Fail the mount if the number of sectors is less than 16.  Otherwise our mount logic
            //  won't work.
            //

            if (Vcb->NumberSectors <= 0x10) {

                try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
            }

            Vcb->ClusterMask = Vcb->BytesPerCluster - 1;
            Vcb->InverseClusterMask = ~Vcb->ClusterMask;
            for (Vcb->ClusterShift = 0, i = Vcb->BytesPerCluster; i > 1; i = i / 2) {
                Vcb->ClusterShift += 1;
            }
            Vcb->ClustersPerPage = PAGE_SIZE >> Vcb->ClusterShift;

            //
            //  Set the sector size in our device object.
            //

            VolDo->DeviceObject.SectorSize = (USHORT) Vcb->BytesPerSector;
        }

        //
        //  Read in the Boot sector, or spare boot sector, on exit of this try
        //  body we will have set bootbcb and bootsector.
        //

        NtfsReadBootSector( IrpContext, Vcb, &BootScb, &BootBcb, (PVOID *)&BootSector );

        //
        //  Check if this is an NTFS volume
        //

        if (!NtfsIsBootSectorNtfs( BootSector, Vcb )) {

            DebugTrace( 0, Dbg, ("Not an NTFS volume\n") );
            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  Media is write protected, so we should try to mount read-only.
        //

        if (WriteProtected) {

            SetFlag( Vcb->VcbState, VCB_STATE_MOUNT_READ_ONLY );
        }

        //
        //  Now that we have a real boot sector on a real NTFS volume we can
        //  really set the proper Vcb fields.
        //

        {
            BIOS_PARAMETER_BLOCK Bpb;

            NtfsUnpackBios( &Bpb, &BootSector->PackedBpb );

            Vcb->BytesPerSector = Bpb.BytesPerSector;
            Vcb->BytesPerCluster = Bpb.BytesPerSector * Bpb.SectorsPerCluster;
            Vcb->NumberSectors = BootSector->NumberSectors;
            Vcb->MftStartLcn = BootSector->MftStartLcn;
            Vcb->Mft2StartLcn = BootSector->Mft2StartLcn;

            Vcb->ClusterMask = Vcb->BytesPerCluster - 1;
            Vcb->InverseClusterMask = ~Vcb->ClusterMask;
            for (Vcb->ClusterShift = 0, i = Vcb->BytesPerCluster; i > 1; i = i / 2) {
                Vcb->ClusterShift += 1;
            }

            //
            //  If the cluster size is greater than the page size then set this value to 1.
            //

            Vcb->ClustersPerPage = PAGE_SIZE >> Vcb->ClusterShift;

            if (Vcb->ClustersPerPage == 0) {

                Vcb->ClustersPerPage = 1;
            }

            //
            //  File records can be smaller, equal or larger than the cluster size.  Initialize
            //  both ClustersPerFileRecordSegment and FileRecordsPerCluster.
            //
            //  If the value in the boot sector is positive then it signifies the
            //  clusters/structure.  If negative then it signifies the shift value
            //  to obtain the structure size.
            //

            if (BootSector->ClustersPerFileRecordSegment < 0) {

                Vcb->BytesPerFileRecordSegment = 1 << (-1 * BootSector->ClustersPerFileRecordSegment);

                //
                //  Initialize the other Mft/Cluster relationship numbers in the Vcb
                //  based on whether the clusters are larger or smaller than file
                //  records.
                //

                if (Vcb->BytesPerFileRecordSegment < Vcb->BytesPerCluster) {

                    Vcb->FileRecordsPerCluster = Vcb->BytesPerCluster / Vcb->BytesPerFileRecordSegment;

                } else {

                    Vcb->ClustersPerFileRecordSegment = Vcb->BytesPerFileRecordSegment / Vcb->BytesPerCluster;
                }

            } else {

                Vcb->BytesPerFileRecordSegment = BytesFromClusters( Vcb, BootSector->ClustersPerFileRecordSegment );
                Vcb->ClustersPerFileRecordSegment = BootSector->ClustersPerFileRecordSegment;
            }

            for (Vcb->MftShift = 0, i = Vcb->BytesPerFileRecordSegment; i > 1; i = i / 2) {
                Vcb->MftShift += 1;
            }

            //
            //  We want to shift between file records and clusters regardless of which is larger.
            //  Compute the shift value here.  Anyone using this value will have to know which
            //  way to shift.
            //

            Vcb->MftToClusterShift = Vcb->MftShift - Vcb->ClusterShift;

            if (Vcb->ClustersPerFileRecordSegment == 0) {

                Vcb->MftToClusterShift = Vcb->ClusterShift - Vcb->MftShift;
            }

            //
            //  Remember the clusters per view section and 4 gig.
            //

            Vcb->ClustersPer4Gig = (ULONG) LlClustersFromBytesTruncate( Vcb, 0x100000000 );

            //
            //  Compute the default index allocation buffer size.
            //

            if (BootSector->DefaultClustersPerIndexAllocationBuffer < 0) {

                Vcb->DefaultBytesPerIndexAllocationBuffer = 1 << (-1 * BootSector->DefaultClustersPerIndexAllocationBuffer);

                //
                //  Determine whether the index allocation buffer is larger/smaller
                //  than the cluster size to determine the block size.
                //

                if (Vcb->DefaultBytesPerIndexAllocationBuffer < Vcb->BytesPerCluster) {

                    Vcb->DefaultBlocksPerIndexAllocationBuffer = Vcb->DefaultBytesPerIndexAllocationBuffer / DEFAULT_INDEX_BLOCK_SIZE;

                } else {

                    Vcb->DefaultBlocksPerIndexAllocationBuffer = Vcb->DefaultBytesPerIndexAllocationBuffer / Vcb->BytesPerCluster;
                }

            } else {

                Vcb->DefaultBlocksPerIndexAllocationBuffer = BootSector->DefaultClustersPerIndexAllocationBuffer;
                Vcb->DefaultBytesPerIndexAllocationBuffer = BytesFromClusters( Vcb, Vcb->DefaultBlocksPerIndexAllocationBuffer );
            }

            //
            //  Now compute our volume specific constants that are stored in
            //  the Vcb.  The total number of clusters is:
            //
            //      (NumberSectors * BytesPerSector) / BytesPerCluster
            //

            Vcb->PreviousTotalClusters =
            Vcb->TotalClusters = LlClustersFromBytesTruncate( Vcb,
                                                              Vcb->NumberSectors * Vcb->BytesPerSector );

            //
            //  Compute the maximum clusters for a file.
            //

            Vcb->MaxClusterCount = LlClustersFromBytesTruncate( Vcb, MAXFILESIZE );

            //
            //  Compute the attribute flags mask for this volume for this volume.
            //

            Vcb->AttributeFlagsMask = 0xffff;

            if (Vcb->BytesPerCluster > 0x1000) {

                ClearFlag( Vcb->AttributeFlagsMask, ATTRIBUTE_FLAG_COMPRESSION_MASK );
            }

            //
            //  For now, an attribute is considered "moveable" if it is at
            //  least 5/16 of the file record.  This constant should only
            //  be changed i conjunction with the MAX_MOVEABLE_ATTRIBUTES
            //  constant.  (The product of the two should be a little less
            //  than or equal to 1.)
            //

            Vcb->BigEnoughToMove = Vcb->BytesPerFileRecordSegment * 5 / 16;

            //
            //  Set the serial number in the Vcb
            //

            Vcb->VolumeSerialNumber = BootSector->SerialNumber;
            Vpb->SerialNumber = ((ULONG)BootSector->SerialNumber);

            //
            //  Compute the sparse file values.
            //

            Vcb->SparseFileUnit = NTFS_SPARSE_FILE_UNIT;
            Vcb->SparseFileClusters = ClustersFromBytes( Vcb, Vcb->SparseFileUnit );

            //
            //  If this is the system boot partition, we need to remember to
            //  not allow this volume to be dismounted.
            //

            if (FlagOn( Vpb->RealDevice->Flags, DO_SYSTEM_BOOT_PARTITION )) {

                SetFlag( Vcb->VcbState, VCB_STATE_DISALLOW_DISMOUNT );
            }

            //
            //  We should never see the BOOT flag in the device we talk to unless it
            //  is in the real device.
            //

            ASSERT( !FlagOn( DeviceObjectWeTalkTo->Flags, DO_SYSTEM_BOOT_PARTITION ) ||
                    FlagOn( Vpb->RealDevice->Flags, DO_SYSTEM_BOOT_PARTITION ));
        }

        //
        //  Initialize recovery state.
        //

        NtfsInitializeRestartTable( sizeof( OPEN_ATTRIBUTE_ENTRY ),
                                    INITIAL_NUMBER_ATTRIBUTES,
                                    &Vcb->OpenAttributeTable );

        NtfsUpdateOatVersion( Vcb, NtfsDefaultRestartVersion );


        NtfsInitializeRestartTable( sizeof( TRANSACTION_ENTRY ),
                                    INITIAL_NUMBER_TRANSACTIONS,
                                    &Vcb->TransactionTable );

        //
        //  Now start preparing to restart the volume.
        //

        //
        //  Create the Mft and Log File Scbs and prepare to read them.
        //  The Mft and mirror length will be the first 4 file records or
        //  the first cluster.
        //

        FirstNonMirroredCluster = ClustersFromBytes( Vcb, 4 * Vcb->BytesPerFileRecordSegment );
        MirroredMftRange = 4 * Vcb->BytesPerFileRecordSegment;

        if (MirroredMftRange < Vcb->BytesPerCluster) {

            MirroredMftRange = Vcb->BytesPerCluster;
        }

        //
        //  Check the case where the boot sector has an invalid value for either the
        //  beginning of the Mft or the beginning of the Mft mirror.  Specifically
        //  check the they don't overlap.  Otherwise we can corrupt the valid one
        //  as we read and possibly try to correct the invalid one.
        //

        if (Vcb->MftStartLcn > Vcb->Mft2StartLcn) {

            MftMirrorOverlap = Vcb->MftStartLcn - Vcb->Mft2StartLcn;

        } else {

            MftMirrorOverlap = Vcb->Mft2StartLcn - Vcb->MftStartLcn;
        }

        MftMirrorOverlap = LlBytesFromClusters( Vcb, MftMirrorOverlap );

        //
        //  Don't raise corrupt since we don't want to attempt to write the
        //  disk in this state.  Someone who knows how will need to
        //  restore the correct boot sector.
        //

        if (MftMirrorOverlap < (LONGLONG) MirroredMftRange) {

            DebugTrace( 0, Dbg, ("Not an NTFS volume\n") );
            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->MftScb,
                            Vcb,
                            MASTER_FILE_TABLE_NUMBER,
                            MirroredMftRange,
                            $DATA,
                            TRUE );

        CcSetAdditionalCacheAttributes( Vcb->MftScb->FileObject, TRUE, TRUE );

        LlTemp1 = FirstNonMirroredCluster;

        (VOID)NtfsAddNtfsMcbEntry( &Vcb->MftScb->Mcb,
                                   (LONGLONG)0,
                                   Vcb->MftStartLcn,
                                   (LONGLONG)FirstNonMirroredCluster,
                                   FALSE );

        //
        //  Now the same for Mft2
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->Mft2Scb,
                            Vcb,
                            MASTER_FILE_TABLE2_NUMBER,
                            MirroredMftRange,
                            $DATA,
                            TRUE );

        CcSetAdditionalCacheAttributes( Vcb->Mft2Scb->FileObject, TRUE, TRUE );


        (VOID)NtfsAddNtfsMcbEntry( &Vcb->Mft2Scb->Mcb,
                                   (LONGLONG)0,
                                   Vcb->Mft2StartLcn,
                                   (LONGLONG)FirstNonMirroredCluster,
                                   FALSE );

        //
        //  Create the dasd system file, we do it here because we need to dummy
        //  up the mcb for it, and that way everything else in NTFS won't need
        //  to know that it is a special file.  We need to do this after
        //  cluster allocation initialization because that computes the total
        //  clusters on the volume.  Also for verification purposes we will
        //  set and get the times off of the volume.
        //
        //  Open it now before the Log File, because that is the first time
        //  anyone may want to mark the volume corrupt.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->VolumeDasdScb,
                            Vcb,
                            VOLUME_DASD_NUMBER,
                            LlBytesFromClusters( Vcb, Vcb->TotalClusters ),
                            $DATA,
                            FALSE );

        (VOID)NtfsAddNtfsMcbEntry( &Vcb->VolumeDasdScb->Mcb,
                                   (LONGLONG)0,
                                   (LONGLONG)0,
                                   Vcb->TotalClusters,
                                   FALSE );

        SetFlag( Vcb->VolumeDasdScb->Fcb->FcbState, FCB_STATE_DUP_INITIALIZED );

        Vcb->VolumeDasdScb->Fcb->LinkCount =
        Vcb->VolumeDasdScb->Fcb->TotalLinks = 1;

        //
        //  We want to read the first four record segments of each of these
        //  files.  We do this so that we don't have a cache miss when we
        //  look up the real allocation below.
        //

        for (i = 0; i < 4; i++) {

            FILE_REFERENCE FileReference;
            BOOLEAN ValidRecord;
            ULONG CorruptHint;

            NtfsSetSegmentNumber( &FileReference, 0, i );
            if (i > 0) {
                FileReference.SequenceNumber = (USHORT)i;
            } else {
                FileReference.SequenceNumber = 1;
            }

            NtfsReadMftRecord( IrpContext,
                               Vcb,
                               &FileReference,
                               FALSE,
                               &Bcbs[i*2],
                               &MftBuffer,
                               NULL );

            NtfsMapStream( IrpContext,
                           Vcb->Mft2Scb,
                           (LONGLONG)(i * Vcb->BytesPerFileRecordSegment),
                           Vcb->BytesPerFileRecordSegment,
                           &Bcbs[i*2 + 1],
                           &Mft2Buffer );

            //
            //  First validate the record and if its valid and record 0
            //  do an extra check for whether its the mft.
            //

            ValidRecord = NtfsCheckFileRecord( Vcb, MftBuffer, &FileReference, &CorruptHint );
            if (ValidRecord && (i == 0)) {

                ATTRIBUTE_ENUMERATION_CONTEXT Context;

                NtfsInitializeAttributeContext( &Context );

                try {

                    if (!NtfsLookupAttributeByCode( IrpContext, Vcb->MftScb->Fcb, &Vcb->MftScb->Fcb->FileReference, $ATTRIBUTE_LIST, &Context )) {

                        if (NtfsLookupAttributeByCode( IrpContext, Vcb->MftScb->Fcb, &Vcb->MftScb->Fcb->FileReference, $FILE_NAME, &Context )) {

                            PFILE_NAME FileName;

                            FileName = (PFILE_NAME) NtfsAttributeValue( NtfsFoundAttribute( &Context ) );
                            if ((FileName->FileNameLength != wcslen( L"MFT" )) ||
                                (!RtlEqualMemory( FileName->FileName, L"$MFT", FileName->FileNameLength * sizeof( WCHAR )))) {

                                ValidRecord = FALSE;
                            }
                        }
                    }

                } finally {
                    NtfsCleanupAttributeContext( IrpContext, &Context );
                }
            }

            //
            //  If any of these file records are bad then try the mirror
            //  (unless we are already looking at the mirror).  If we
            //  can't find a valid record then fail the mount.
            //

            if (!ValidRecord) {

                if ((MftBuffer != Mft2Buffer) &&
                    NtfsCheckFileRecord( Vcb, Mft2Buffer, &FileReference, &CorruptHint )) {

                    LlTemp1 = MAXLONGLONG;

                    //
                    //  Put a BaadSignature in this file record,
                    //  mark it dirty and then read it again.
                    //  The baad signature should force us to bring
                    //  in the mirror and we can correct the problem.
                    //

                    NtfsPinMappedData( IrpContext,
                                       Vcb->MftScb,
                                       i * Vcb->BytesPerFileRecordSegment,
                                       Vcb->BytesPerFileRecordSegment,
                                       &Bcbs[i*2] );

                    RtlCopyMemory( MftBuffer, Mft2Buffer, Vcb->BytesPerFileRecordSegment );

                    CcSetDirtyPinnedData( Bcbs[i*2], (PLARGE_INTEGER) &LlTemp1 );

                } else {

                    NtfsMarkVolumeDirty( IrpContext, Vcb, FALSE );
                    try_return( Status = STATUS_DISK_CORRUPT_ERROR );
                }
            }
        }

        //
        //  The last file record was the Volume Dasd, so check the version number.
        //

        Attribute = NtfsFirstAttribute(MftBuffer);

        while (TRUE) {

            Attribute = NtfsGetNextRecord(Attribute);

            if (Attribute->TypeCode == $VOLUME_INFORMATION) {

                PVOLUME_INFORMATION VolumeInformation;

                VolumeInformation = (PVOLUME_INFORMATION)NtfsAttributeValue(Attribute);
                VolumeFlags = VolumeInformation->VolumeFlags;

                //
                //  Upgrading the disk on NT 5.0 will use version number 3.0.  Version
                //  number 2.0 was used temporarily when the upgrade was automatic.
                //
                //  NOTE - We use the presence of the version number to indicate
                //  that the first four file records have been validated.  We won't
                //  flush the MftMirror if we can't verify these records.  Otherwise
                //  we might corrupt a valid mirror.
                //

                Vcb->MajorVersion = VolumeInformation->MajorVersion;
                Vcb->MinorVersion = VolumeInformation->MinorVersion;

                if ((Vcb->MajorVersion < 1) || (Vcb->MajorVersion > 3)) {
                    NtfsRaiseStatus( IrpContext, STATUS_WRONG_VOLUME, NULL, NULL );
                }

                if (Vcb->MajorVersion > 1) {

                    CurrentVersion = TRUE;

                    ASSERT( VolumeInformation->MajorVersion != 2 || !FlagOn( IrpContext->State, IRP_CONTEXT_STATE_VOL_UPGR_FAILED ) );

                    if (NtfsDefragMftEnabled) {
                        SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED );
                    }
                }

                break;
            }

            if (Attribute->TypeCode == $END) {
                NtfsRaiseStatus( IrpContext, STATUS_WRONG_VOLUME, NULL, NULL );
            }
        }

        //
        //  Create the log file Scb and really look up its size.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->LogFileScb,
                            Vcb,
                            LOG_FILE_NUMBER,
                            0,
                            $DATA,
                            TRUE );

        Vcb->LogFileObject = Vcb->LogFileScb->FileObject;

        CcSetAdditionalCacheAttributes( Vcb->LogFileScb->FileObject, TRUE, TRUE );

        //
        //  Lookup the log file mapping now, since we will not go to the
        //  disk for allocation information any more once we set restart
        //  in progress.
        //

        (VOID)NtfsPreloadAllocation( IrpContext, Vcb->LogFileScb, 0, MAXLONGLONG );

        //
        //  Now we have to unpin everything before restart, because it generally
        //  has to uninitialize everything.
        //

        NtfsUnpinBcb( IrpContext, &BootBcb );

        for (i = 0; i < 8; i++) {
            NtfsUnpinBcb( IrpContext, &Bcbs[i] );
        }

        NtfsPurgeFileRecordCache( IrpContext );

        //
        //  Purge the Mft, since we only read the first four file
        //  records, not necessarily an entire page!
        //

        CcPurgeCacheSection( &Vcb->MftScb->NonpagedScb->SegmentObject, NULL, 0, FALSE );

        //
        //  Now start up the log file and perform Restart.  This calls will
        //  unpin and remap the Mft Bcb's.  The MftBuffer variables above
        //  may no longer point to the correct range of bytes.  This is OK
        //  if they are never referenced.
        //
        //  Put a try-except around this to catch any restart failures.
        //  This is important in order to allow us to limp along until
        //  autochk gets a chance to run.
        //
        //  We set restart in progress first, to prevent us from looking up any
        //  more run information (now that we know where the log file is!)
        //

        SetFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

        try {

            Status = STATUS_SUCCESS;
            UnrecognizedRestart = FALSE;

#if __NDAS_NTFS_AVOID_LOG__

            NtfsStartLogFile( IrpContext,
							  Vcb->LogFileScb,
                              Vcb );

#else

            NtfsStartLogFile( Vcb->LogFileScb,
                              Vcb );

#endif
            //
            //  We call the cache manager again with the stream files for the Mft and
            //  Mft mirror as we didn't have a log handle for the first call.
            //

            CcSetLogHandleForFile( Vcb->MftScb->FileObject,
                                   Vcb->LogHandle,
                                   &LfsFlushToLsn );

            CcSetLogHandleForFile( Vcb->Mft2Scb->FileObject,
                                   Vcb->LogHandle,
                                   &LfsFlushToLsn );

            CloseAttributes = TRUE;

            if (!NtfsIsVolumeReadOnly( Vcb )) {

                UpdatesApplied = NtfsRestartVolume( IrpContext, Vcb, &UnrecognizedRestart );
            }

        //
        //  For right now, we will charge ahead with a dirty volume, no
        //  matter what the exception was.  Later we will have to be
        //  defensive and use a filter.
        //

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            Status = GetExceptionCode();

            //
            //  If the error is STATUS_LOG_FILE_FULL then it means that
            //  we couldn't complete the restart.  Mark the volume dirty in
            //  this case.  Don't return this error code.
            //

            if (Status == STATUS_LOG_FILE_FULL) {

                Status = STATUS_DISK_CORRUPT_ERROR;
                IrpContext->ExceptionStatus = STATUS_DISK_CORRUPT_ERROR;
            }
        }

        //
        //  If we hit an error trying to mount this as a readonly volume,
        //  fail the mount. We don't want to do any writes.
        //

        if (Status == STATUS_MEDIA_WRITE_PROTECTED) {

            ASSERT(FlagOn( Vcb->VcbState, VCB_STATE_MOUNT_READ_ONLY ));
            ClearFlag( Vcb->VcbState, VCB_STATE_MOUNT_READ_ONLY );
            try_return( Status );
        }

        //
        //  Mark the volume dirty if we hit an error during restart or if we didn't
        //  recognize the restart area.  In that case also mark the volume dirty but
        //  continue to run.
        //

        if (!NT_SUCCESS( Status ) || UnrecognizedRestart) {

            LONGLONG VolumeDasdOffset;

            NtfsSetAndGetVolumeTimes( IrpContext, Vcb, TRUE, FALSE );

            //
            //  Now flush it out, so chkdsk can see it with Dasd.
            //  Clear the error in the IrpContext so that this
            //  flush will succeed.  Otherwise CommonWrite will
            //  return FILE_LOCK_CONFLICT.
            //

            IrpContext->ExceptionStatus = STATUS_SUCCESS;

            VolumeDasdOffset = VOLUME_DASD_NUMBER << Vcb->MftShift;

            CcFlushCache( &Vcb->MftScb->NonpagedScb->SegmentObject,
                          (PLARGE_INTEGER)&VolumeDasdOffset,
                          Vcb->BytesPerFileRecordSegment,
                          NULL );

            if (!NT_SUCCESS( Status )) {

                try_return( Status );
            }
        }

        //
        //  Now flush the Mft copies, because we are going to shut the real
        //  one down and reopen it for real.
        //

        CcFlushCache( &Vcb->Mft2Scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );

        if (NT_SUCCESS( IoStatus.Status )) {
            CcFlushCache( &Vcb->MftScb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );
        }

        if (!NT_SUCCESS( IoStatus.Status )) {

            NtfsNormalizeAndRaiseStatus( IrpContext,
                                         IoStatus.Status,
                                         STATUS_UNEXPECTED_IO_ERROR );
        }

        //
        //  Show that the restart is complete, and it is safe to go to
        //  the disk for the Mft allocation.
        //

        ClearFlag( Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS );

        //
        //  Set the Mft sizes back down to the part which is guaranteed to
        //  be contiguous for now.  Important on large page size systems!
        //

        Vcb->MftScb->Header.AllocationSize.QuadPart =
        Vcb->MftScb->Header.FileSize.QuadPart =
        Vcb->MftScb->Header.ValidDataLength.QuadPart = FirstNonMirroredCluster << Vcb->ClusterShift;

        //
        //  Pin the first four file records.  We need to lock the pages to
        //  absolutely guarantee they stay in memory, otherwise we may
        //  generate a recursive page fault, forcing MM to block.
        //

        for (i = 0; i < 4; i++) {

            FILE_REFERENCE FileReference;
            ULONG CorruptHint;

            NtfsSetSegmentNumber( &FileReference, 0, i );
            if (i > 0) {
                FileReference.SequenceNumber = (USHORT)i;
            } else {
                FileReference.SequenceNumber = 1;
            }

            NtfsPinStream( IrpContext,
                           Vcb->MftScb,
                           (LONGLONG)(i << Vcb->MftShift),
                           Vcb->BytesPerFileRecordSegment,
                           &Bcbs[i*2],
                           (PVOID *)&MftBuffer );

            Mdls[i*2] = IoAllocateMdl( MftBuffer,
                                       Vcb->BytesPerFileRecordSegment,
                                       FALSE,
                                       FALSE,
                                       NULL );

            //
            //  Verify that we got an Mdl.
            //

            if (Mdls[i*2] == NULL) {

                NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
            }

            MmProbeAndLockPages( Mdls[i*2], KernelMode, IoReadAccess );

            NtfsPinStream( IrpContext,
                           Vcb->Mft2Scb,
                           (LONGLONG)(i << Vcb->MftShift),
                           Vcb->BytesPerFileRecordSegment,
                           &Bcbs[i*2 + 1],
                           &Mft2Buffer );

            Mdls[i*2 + 1] = IoAllocateMdl( Mft2Buffer,
                                           Vcb->BytesPerFileRecordSegment,
                                           FALSE,
                                           FALSE,
                                           NULL );

            //
            //  Verify that we got an Mdl.
            //

            if (Mdls[i*2 + 1] == NULL) {

                NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
            }

            MmProbeAndLockPages( Mdls[i*2 + 1], KernelMode, IoReadAccess );

            //
            //  If any of these file records are bad then try the mirror
            //  (unless we are already looking at the mirror).  If we
            //  can't find a valid record then fail the mount.
            //

            if (!NtfsCheckFileRecord( Vcb, MftBuffer, &FileReference, &CorruptHint )) {

                if ((MftBuffer != Mft2Buffer) &&
                    NtfsCheckFileRecord( Vcb, Mft2Buffer, &FileReference, &CorruptHint )) {

                    LlTemp1 = MAXLONGLONG;

                    //
                    //  Put a BaadSignature in this file record,
                    //  mark it dirty and then read it again.
                    //  The baad signature should force us to bring
                    //  in the mirror and we can correct the problem.
                    //

                    RtlCopyMemory( MftBuffer, Mft2Buffer, Vcb->BytesPerFileRecordSegment );
                    CcSetDirtyPinnedData( Bcbs[i*2], (PLARGE_INTEGER) &LlTemp1 );

                } else {

                    NtfsMarkVolumeDirty( IrpContext, Vcb, FALSE );
                    try_return( Status = STATUS_DISK_CORRUPT_ERROR );
                }
            }
        }

        //
        //  Now we need to uninitialize and purge the Mft and Mft2.  This is
        //  because we could have only a partially filled page at the end, and
        //  we need to do real reads of whole pages now.
        //

        //
        //  Uninitialize and reinitialize the large mcbs so that we can reload
        //  it from the File Record.
        //

        NtfsUnloadNtfsMcbRange( &Vcb->MftScb->Mcb, (LONGLONG) 0, MAXLONGLONG, TRUE, FALSE );
        NtfsUnloadNtfsMcbRange( &Vcb->Mft2Scb->Mcb, (LONGLONG) 0, MAXLONGLONG, TRUE, FALSE );

        //
        //  Mark both of them as uninitialized.
        //

        ClearFlag( Vcb->MftScb->ScbState, SCB_STATE_FILE_SIZE_LOADED );
        ClearFlag( Vcb->Mft2Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED );

        //
        //  We need to deal with a rare case where the Scb for a non-resident attribute
        //  list for the Mft has been created but the size is not correct.  This could
        //  happen if we logged part of the stream but not the whole stream.  In that
        //  case we really want to load the correct numbers into the Scb.  We will need the
        //  full attribute list if we are to look up the allocation for the Mft
        //  immediately after this.
        //

        MftLinks = Vcb->MftScb->Fcb->ScbQueue.Flink;

        while (MftLinks != &Vcb->MftScb->Fcb->ScbQueue) {

            AttributeListScb = CONTAINING_RECORD( MftLinks,
                                                  SCB,
                                                  FcbLinks );

            if (AttributeListScb->AttributeTypeCode == $ATTRIBUTE_LIST) {

                //
                //  Clear the flags so we can reload the information from disk.
                //  Also unload the allocation.  If we have a log record for a
                //  change to the attribute list for the Mft then the allocation
                //  may only be partially loaded.  Looking up the allocation for the
                //  Mft below could easily hit one of the holes.  This way we will
                //  reload all of the allocation.
                //

                NtfsUnloadNtfsMcbRange( &AttributeListScb->Mcb, 0, MAXLONGLONG, TRUE, FALSE );
                ClearFlag( AttributeListScb->ScbState, SCB_STATE_FILE_SIZE_LOADED | SCB_STATE_HEADER_INITIALIZED );
                NtfsUpdateScbFromAttribute( IrpContext, AttributeListScb, NULL );

                //
                //  Let the cache manager know the sizes if this is cached.
                //

                if (AttributeListScb->FileObject != NULL) {

                    CcSetFileSizes( AttributeListScb->FileObject,
                                    (PCC_FILE_SIZES) &AttributeListScb->Header.AllocationSize );
                }

                break;
            }

            MftLinks = MftLinks->Flink;
        }

        //
        //  Now load up the real allocation from just the first file record.
        //

        if (Vcb->FileRecordsPerCluster == 0) {

            NtfsPreloadAllocation( IrpContext,
                                   Vcb->MftScb,
                                   0,
                                   (FIRST_USER_FILE_NUMBER - 1) << Vcb->MftToClusterShift );

        } else {

            NtfsPreloadAllocation( IrpContext,
                                   Vcb->MftScb,
                                   0,
                                   (FIRST_USER_FILE_NUMBER - 1) >> Vcb->MftToClusterShift );
        }

        NtfsPreloadAllocation( IrpContext, Vcb->Mft2Scb, 0, MAXLONGLONG );

        //
        //  We update the Mft and the Mft mirror before we delete the current
        //  stream file for the Mft.  We know we can read the true attributes
        //  for the Mft and the Mirror because we initialized their sizes
        //  above through the first few records in the Mft.
        //

        NtfsUpdateScbFromAttribute( IrpContext, Vcb->MftScb, NULL );

        //
        //  We will attempt to upgrade the version only if this isn't already
        //  a version 2 or 3 volume, the upgrade bit is set, and we aren't
        //  retrying the mount because the upgrade failed last time.
        //  We will always upgrade a new volume
        //

        if ((Vcb->MajorVersion == 1) &&
            !FlagOn( IrpContext->State, IRP_CONTEXT_STATE_VOL_UPGR_FAILED ) &&
           (NtfsForceUpgrade ?
            (!FlagOn( NtfsData.Flags, NTFS_FLAGS_DISABLE_UPGRADE ) ||
             (Vcb->MftScb->Header.FileSize.QuadPart <= FIRST_USER_FILE_NUMBER * Vcb->BytesPerFileRecordSegment))
                                                                        :
            FlagOn( VolumeFlags, VOLUME_UPGRADE_ON_MOUNT ))) {

            //
            //  We can't upgrade R/O volumes, so we can't proceed either.
            //

            if (NtfsIsVolumeReadOnly( Vcb )) {

                Status = STATUS_MEDIA_WRITE_PROTECTED;
                try_return( Status );
            }

            UpgradeVolume = TRUE;
        }

        ClearFlag( Vcb->MftScb->ScbState, SCB_STATE_WRITE_COMPRESSED );
        ClearFlag( Vcb->MftScb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK );

        if (!FlagOn( Vcb->MftScb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE )) {

            Vcb->MftScb->CompressionUnit = 0;
            Vcb->MftScb->CompressionUnitShift = 0;
        }

        NtfsUpdateScbFromAttribute( IrpContext, Vcb->Mft2Scb, NULL );
        ClearFlag( Vcb->Mft2Scb->ScbState, SCB_STATE_WRITE_COMPRESSED );
        ClearFlag( Vcb->Mft2Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK );

        if (!FlagOn( Vcb->Mft2Scb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE )) {

            Vcb->Mft2Scb->CompressionUnit = 0;
            Vcb->Mft2Scb->CompressionUnitShift = 0;
        }

        //
        //  Unpin the Bcb's for the Mft files before uninitializing.
        //

        for (i = 0; i < 8; i++) {

            NtfsUnpinBcb( IrpContext, &Bcbs[i] );

            //
            //  Now we can get rid of these Mdls.
            //

            MmUnlockPages( Mdls[i] );
            IoFreeMdl( Mdls[i] );
            Mdls[i] = NULL;
        }

        //
        //  Before we call CcSetAdditionalCacheAttributes to disable write behind,
        //  we need to flush what we can now.
        //

        CcFlushCache( &Vcb->MftScb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );

        //
        //  Now close and purge the Mft, and recreate its stream so that
        //  the Mft is in a normal state, and we can close the rest of
        //  the attributes from restart.  We need to bump the close count
        //  to keep the scb around while we do this little bit of trickery
        //

        {
            Vcb->MftScb->CloseCount += 1;

            NtfsPurgeFileRecordCache( IrpContext );

            NtfsDeleteInternalAttributeStream( Vcb->MftScb, TRUE, FALSE );

            NtfsCreateInternalAttributeStream( IrpContext,
                                               Vcb->MftScb,
                                               FALSE,
                                               &NtfsSystemFiles[MASTER_FILE_TABLE_NUMBER] );

            //
            //  Tell the cache manager the file sizes for the MFT.  It is possible
            //  that the shared cache map did not go away on the DeleteInternalAttributeStream
            //  call above.  In that case the Cache Manager has the file sizes from
            //  restart.
            //

            CcSetFileSizes( Vcb->MftScb->FileObject,
                            (PCC_FILE_SIZES) &Vcb->MftScb->Header.AllocationSize );

            CcSetAdditionalCacheAttributes( Vcb->MftScb->FileObject, TRUE, FALSE );

            Vcb->MftScb->CloseCount -= 1;
        }

        //
        //  We want to read all of the file records for the Mft to put
        //  its complete mapping into the Mcb.
        //

        SetFlag( Vcb->VcbState, VCB_STATE_PRELOAD_MFT );
        NtfsPreloadAllocation( IrpContext, Vcb->MftScb, 0, MAXLONGLONG );
        ClearFlag( Vcb->VcbState, VCB_STATE_PRELOAD_MFT );

        //
        //  Close the boot file (get rid of it because we do not know its proper
        //  size, and the Scb may be inconsistent).
        //

        NtfsDeleteInternalAttributeStream( BootScb, TRUE, FALSE );
        BootScb = NULL;

        //
        //  Closing the attributes from restart has to occur here after
        //  the Mft is clean, because flushing these files will cause
        //  file size updates to occur, etc.
        //

        Status = NtfsCloseAttributesFromRestart( IrpContext, Vcb );
        CloseAttributes = FALSE;

        if (!NT_SUCCESS( Status )) {

            NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
        }

        //
        //  The CHECKPOINT flags function the same way whether the volume is mounted
        //  read-only or not. We just ignore the actual checkpointing process.
        //

        NtfsAcquireCheckpoint( IrpContext, Vcb );

        //
        //  Show that it is ok to checkpoint now.
        //

        ClearFlag( Vcb->CheckpointFlags, VCB_CHECKPOINT_SYNC_FLAGS | VCB_LAST_CHECKPOINT_CLEAN );

        //
        //  Clear the flag indicating that we won't defrag the volume.
        //

        ClearFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_ENABLED );

        NtfsSetCheckpointNotify( IrpContext, Vcb );
        NtfsReleaseCheckpoint( IrpContext, Vcb );

        //
        //  We always need to write a checkpoint record so that we have
        //  a checkpoint on the disk before we modify any files.
        //

        NtfsCheckpointVolume( IrpContext,
                              Vcb,
                              FALSE,
                              UpdatesApplied,
                              UpdatesApplied,
                              0,
                              Vcb->LastRestartArea );

        //
        //  Now set the defrag enabled flag.
        //

        NtfsAcquireCheckpoint( IrpContext, Vcb );
        SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_ENABLED );
        NtfsReleaseCheckpoint( IrpContext, Vcb );

/*      Format is using wrong attribute definitions

        //
        //  At this point we are ready to use the volume normally.  We could
        //  open the remaining system files by name, but for now we will go
        //  ahead and open them by file number.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->AttributeDefTableScb,
                            Vcb,
                            ATTRIBUTE_DEF_TABLE_NUMBER,
                            0,
                            $DATA,
                            FALSE );

        //
        //  Read in the attribute definitions.
        //

        {
            IO_STATUS_BLOCK IoStatus;
            PSCB Scb = Vcb->AttributeDefTableScb;

            if ((Scb->Header.FileSize.HighPart != 0) || (Scb->Header.FileSize.LowPart == 0)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            Vcb->AttributeDefinitions = NtfsAllocatePool(PagedPool, Scb->Header.FileSize.LowPart );

            CcCopyRead( Scb->FileObject,
                        &Li0,
                        Scb->Header.FileSize.LowPart,
                        TRUE,
                        Vcb->AttributeDefinitions,
                        &IoStatus );

            if (!NT_SUCCESS(IoStatus.Status)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }
        }
*/
        //
        //  Just point to our own attribute definitions for now.
        //

        Vcb->AttributeDefinitions = NtfsAttributeDefinitions;

        //
        //  Open the upcase table.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->UpcaseTableScb,
                            Vcb,
                            UPCASE_TABLE_NUMBER,
                            0,
                            $DATA,
                            FALSE );

        //
        //  Read in the upcase table.
        //

        {
            IO_STATUS_BLOCK IoStatus;
            PSCB Scb = Vcb->UpcaseTableScb;

            if ((Scb->Header.FileSize.HighPart != 0) || (Scb->Header.FileSize.LowPart < 512)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            Vcb->UpcaseTable = NtfsAllocatePool(PagedPool, Scb->Header.FileSize.LowPart );
            Vcb->UpcaseTableSize = Scb->Header.FileSize.LowPart / sizeof( WCHAR );

            CcCopyRead( Scb->FileObject,
                        &Li0,
                        Scb->Header.FileSize.LowPart,
                        TRUE,
                        Vcb->UpcaseTable,
                        &IoStatus );

            if (!NT_SUCCESS(IoStatus.Status)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  If we do not have a global upcase table yet then make this one the global one
            //

            if (NtfsData.UpcaseTable == NULL) {

                NtfsData.UpcaseTable = Vcb->UpcaseTable;
                NtfsData.UpcaseTableSize = Vcb->UpcaseTableSize;

            //
            //  Otherwise if this one perfectly matches the global upcase table then throw
            //  this one back and use the global one
            //

            } else if ((NtfsData.UpcaseTableSize == Vcb->UpcaseTableSize)

                            &&

                       (RtlCompareMemory( NtfsData.UpcaseTable,
                                          Vcb->UpcaseTable,
                                          Vcb->UpcaseTableSize) == Vcb->UpcaseTableSize)) {

                NtfsFreePool( Vcb->UpcaseTable );
                Vcb->UpcaseTable = NtfsData.UpcaseTable;
            }
        }

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->BitmapScb,
                            Vcb,
                            BIT_MAP_FILE_NUMBER,
                            0,
                            $DATA,
                            TRUE );

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->BadClusterFileScb,
                            Vcb,
                            BAD_CLUSTER_FILE_NUMBER,
                            0,
                            $DATA,
                            TRUE );

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->MftBitmapScb,
                            Vcb,
                            MASTER_FILE_TABLE_NUMBER,
                            0,
                            $BITMAP,
                            TRUE );

        //
        //  Initialize the bitmap support
        //

        NtfsInitializeClusterAllocation( IrpContext, Vcb );

        NtfsSetAndGetVolumeTimes( IrpContext, Vcb, FALSE, TRUE );

        //
        //  Initialize the Mft record allocation
        //

        {
            ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
            BOOLEAN FoundAttribute;
            ULONG ExtendGranularity;

            //
            //  Lookup the bitmap allocation for the Mft file.
            //

            NtfsInitializeAttributeContext( &AttrContext );

            //
            //  Use a try finally to cleanup the attribute context.
            //

            try {

                //
                //  CODENOTE    Is the Mft Fcb fully initialized at this point??
                //

                FoundAttribute = NtfsLookupAttributeByCode( IrpContext,
                                                            Vcb->MftScb->Fcb,
                                                            &Vcb->MftScb->Fcb->FileReference,
                                                            $BITMAP,
                                                            &AttrContext );
                //
                //  Error if we don't find the bitmap
                //

                if (!FoundAttribute) {

                    DebugTrace( 0, 0, ("Couldn't find bitmap attribute for Mft\n") );

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                //
                //  If there is no file object for the Mft Scb, we create it now.
                //

                if (Vcb->MftScb->FileObject == NULL) {

                    NtfsCreateInternalAttributeStream( IrpContext, Vcb->MftScb, TRUE, NULL );
                }

                //
                //  TEMPCODE    We need a better way to determine the optimal
                //              truncate and extend granularity.
                //

                ExtendGranularity = MFT_EXTEND_GRANULARITY;

                if ((ExtendGranularity * Vcb->BytesPerFileRecordSegment) < Vcb->BytesPerCluster) {

                    ExtendGranularity = Vcb->FileRecordsPerCluster;
                }

                NtfsInitializeRecordAllocation( IrpContext,
                                                Vcb->MftScb,
                                                &AttrContext,
                                                Vcb->BytesPerFileRecordSegment,
                                                ExtendGranularity,
                                                ExtendGranularity,
                                                &Vcb->MftScb->ScbType.Index.RecordAllocationContext );

            } finally {

                NtfsCleanupAttributeContext( IrpContext, &AttrContext );
            }
        }

        //
        //  Get the serial number and volume label for the volume
        //

        NtfsGetVolumeInformation( IrpContext, Vpb, Vcb, &VolumeFlags );

        //
        //  Get the Device Name for this volume.
        //

        Status = ObQueryNameString( Vpb->RealDevice,
                                    NULL,
                                    0,
                                    &DeviceObjectNameLength );

        ASSERT( Status != STATUS_SUCCESS );

        //
        //  Unlike the rest of the system, ObQueryNameString returns
        //  STATUS_INFO_LENGTH_MISMATCH instead of STATUS_BUFFER_TOO_SMALL when
        //  passed too small a buffer.
        //
        //  We expect to get this error here.  Anything else we can't handle.
        //

        if (Status == STATUS_INFO_LENGTH_MISMATCH) {

            DeviceObjectName = NtfsAllocatePool( PagedPool, DeviceObjectNameLength );

            Status = ObQueryNameString( Vpb->RealDevice,
                                        DeviceObjectName,
                                        DeviceObjectNameLength,
                                        &DeviceObjectNameLength );
        }

        if (!NT_SUCCESS( Status )) {

            try_return( NOTHING );
        }

        //
        //  Now that we are successfully mounting, let us see if we should
        //  enable balanced reads.
        //

        if (!FlagOn(Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED_DIRTY)) {

            FsRtlBalanceReads( DeviceObjectWeTalkTo );
        }

        ASSERT( DeviceObjectName->Name.Length != 0 );

        Vcb->DeviceName.MaximumLength =
        Vcb->DeviceName.Length = DeviceObjectName->Name.Length;

        Vcb->DeviceName.Buffer = NtfsAllocatePool( PagedPool, DeviceObjectName->Name.Length );

        RtlCopyMemory( Vcb->DeviceName.Buffer,
                       DeviceObjectName->Name.Buffer,
                       DeviceObjectName->Name.Length );

        //
        //  Now we want to initialize the remaining defrag status values.
        //

        Vcb->MftHoleGranularity = MFT_HOLE_GRANULARITY;
        Vcb->MftClustersPerHole = Vcb->MftHoleGranularity << Vcb->MftToClusterShift;

        if (MFT_HOLE_GRANULARITY < Vcb->FileRecordsPerCluster) {

            Vcb->MftHoleGranularity = Vcb->FileRecordsPerCluster;
            Vcb->MftClustersPerHole = 1;
        }

        Vcb->MftHoleMask = Vcb->MftHoleGranularity - 1;
        Vcb->MftHoleInverseMask = ~(Vcb->MftHoleMask);

        Vcb->MftHoleClusterMask = Vcb->MftClustersPerHole - 1;
        Vcb->MftHoleClusterInverseMask = ~(Vcb->MftHoleClusterMask);

        //
        //  Our maximum reserved Mft space is 0x140, we will try to
        //  get an extra 40 bytes if possible.
        //

        Vcb->MftReserved = Vcb->BytesPerFileRecordSegment / 8;

        if (Vcb->MftReserved > 0x140) {

            Vcb->MftReserved = 0x140;
        }

        Vcb->MftCushion = Vcb->MftReserved - 0x20;

        NtfsScanMftBitmap( IrpContext, Vcb );

#ifdef NTFS_CHECK_BITMAP
        {
            ULONG BitmapSize;
            ULONG Count;

            BitmapSize = Vcb->BitmapScb->Header.FileSize.LowPart;

            //
            //  Allocate a buffer for the bitmap copy and each individual bitmap.
            //

            Vcb->BitmapPages = (BitmapSize + PAGE_SIZE - 1) / PAGE_SIZE;

            Vcb->BitmapCopy = NtfsAllocatePool(PagedPool, Vcb->BitmapPages * sizeof( RTL_BITMAP ));
            RtlZeroMemory( Vcb->BitmapCopy, Vcb->BitmapPages * sizeof( RTL_BITMAP ));

            //
            //  Now get a buffer for each page.
            //

            for (Count = 0; Count < Vcb->BitmapPages; Count += 1) {

                (Vcb->BitmapCopy + Count)->Buffer = NtfsAllocatePool(PagedPool, PAGE_SIZE );
                RtlInitializeBitMap( Vcb->BitmapCopy + Count, (Vcb->BitmapCopy + Count)->Buffer, PAGE_SIZE * 8 );
            }

            if (NtfsCopyBitmap) {

                PUCHAR NextPage;
                PBCB BitmapBcb = NULL;
                ULONG BytesToCopy;
                LONGLONG FileOffset = 0;

                Count = 0;

                while (BitmapSize) {

                    BytesToCopy = PAGE_SIZE;

                    if (BytesToCopy > BitmapSize) {

                        BytesToCopy = BitmapSize;
                    }

                    NtfsUnpinBcb( IrpContext, &BitmapBcb );

                    NtfsMapStream( IrpContext, Vcb->BitmapScb, FileOffset, BytesToCopy, &BitmapBcb, &NextPage );

                    RtlCopyMemory( (Vcb->BitmapCopy + Count)->Buffer,
                                   NextPage,
                                   BytesToCopy );

                    BitmapSize -= BytesToCopy;
                    FileOffset += BytesToCopy;
                    Count += 1;
                }

                NtfsUnpinBcb( IrpContext, &BitmapBcb );

            //
            //  Otherwise we will want to scan the entire Mft and compare the mapping pairs
            //  with the current volume bitmap.
            //

            }
        }
#endif

        //
        //  Whether this was already an upgraded volume or we want it to
        //  be one now, we need to open all the new indices.
        //

        if ((CurrentVersion || UpgradeVolume) &&
            !SkipNtOfs) {

            BOOLEAN UpdatedVolumeVersion = FALSE;

            try {

                //
                //  Create/open the security file and initialize security on the volume.
                //

                NtfsInitializeSecurityFile( IrpContext, Vcb );

                //
                //  Open the Root Directory.
                //

                NtfsOpenRootDirectory( IrpContext, Vcb );

                //
                //  Create/open the $Extend directory.
                //

                NtfsInitializeExtendDirectory( IrpContext, Vcb );

                //
                //  Create/open the Quota File and initialize quotas.
                //

                NtfsInitializeQuotaFile( IrpContext, Vcb );

                //
                //  Create/open the Object Id File and initialize object ids.
                //

                NtfsInitializeObjectIdFile( IrpContext, Vcb );

                //
                //  Create/open the Mount Points File and initialize it.
                //

                NtfsInitializeReparseFile( IrpContext, Vcb );

                //
                //  Open the Usn Journal only if it is there.  If the volume was mounted
                //  on a 4.0 system then we want to restamp the journal.  Skip the
                //  initialization if the volume flags indicate that the journal
                //  delete has started.
                //  No USN journal if we're mounting Read Only.
                //

                if (FlagOn( VolumeFlags, VOLUME_DELETE_USN_UNDERWAY )) {

                    SetFlag( Vcb->VcbState, VCB_STATE_USN_DELETE );

                } else if (!NtfsIsVolumeReadOnly( Vcb )) {

                    NtfsInitializeUsnJournal( IrpContext,
                                              Vcb,
                                              FALSE,
                                              FlagOn( VolumeFlags, VOLUME_MOUNTED_ON_40 ),
                                              (PCREATE_USN_JOURNAL_DATA) &Vcb->UsnJournalInstance.MaximumSize );

                    if (FlagOn( VolumeFlags, VOLUME_MOUNTED_ON_40 )) {

                        NtfsSetVolumeInfoFlagState( IrpContext,
                                                    Vcb,
                                                    VOLUME_MOUNTED_ON_40,
                                                    FALSE,
                                                    TRUE );
                    }
                }

                //
                //  Upgrade all security information
                //

                NtfsUpgradeSecurity( IrpContext, Vcb );

                //
                //  If we haven't opened the root directory, do so
                //

                if (Vcb->RootIndexScb == NULL) {
                    NtfsOpenRootDirectory( IrpContext, Vcb );
                }

                NtfsCleanupTransaction( IrpContext, STATUS_SUCCESS, FALSE );

                //
                //  Update version numbers in volinfo
                //

                if (!NtfsIsVolumeReadOnly( Vcb )) {
                    UpdatedVolumeVersion = NtfsUpdateVolumeInfo( IrpContext, Vcb, NTFS_MAJOR_VERSION, NTFS_MINOR_VERSION );
                }

                //
                //  If we've gotten this far during the mount, it's safe to
                //  update the version number on disk if necessary.
                //

                if (UpgradeVolume) {

                    //
                    //  Now enable defragging.
                    //

                    if (NtfsDefragMftEnabled) {

                        NtfsAcquireCheckpoint( IrpContext, Vcb );
                        SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED );
                        NtfsReleaseCheckpoint( IrpContext, Vcb );
                    }

                    //
                    //  Update the on-disk attribute definition table to include the
                    //  new attributes for an upgraded volume.
                    //

                    NtfsUpdateAttributeTable( IrpContext, Vcb );
                }

            } finally {

                if (!NT_SUCCESS( IrpContext->ExceptionStatus ) && UpgradeVolume) {
                    SetFlag( IrpContext->State, IRP_CONTEXT_STATE_VOL_UPGR_FAILED );
                }
            }

            if (UpdatedVolumeVersion) {

                //
                //  If we've upgraded successfully, we should clear the upgrade
                //  bit now so we can use it again in the future.
                //

                NtfsSetVolumeInfoFlagState( IrpContext,
                                            Vcb,
                                            VOLUME_UPGRADE_ON_MOUNT,
                                            FALSE,
                                            TRUE );
            }

        } else {

            //
            //  If we haven't opened the root directory, do so
            //

            if (Vcb->RootIndexScb == NULL) {
                NtfsOpenRootDirectory( IrpContext, Vcb );
            }

            NtfsCleanupTransaction( IrpContext, STATUS_SUCCESS, FALSE );
        }

        //
        //  Start the usn journal delete operation if the vcb flag is specified.
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_USN_DELETE )) {

            NtfsPostSpecial( IrpContext, Vcb, NtfsDeleteUsnSpecial, &Vcb->DeleteUsnData );
        }

        //
        //  If the last mount was on a 4.0 volume then we need to clean up the quota
        //  and object id indices.
        //

        if ((Vcb->MajorVersion >= 3) &&
            FlagOn( VolumeFlags, VOLUME_MOUNTED_ON_40 )) {

            NtfsSetVolumeInfoFlagState( IrpContext,
                                        Vcb,
                                        VOLUME_REPAIR_OBJECT_ID,
                                        TRUE,
                                        TRUE );

            SetFlag( VolumeFlags, VOLUME_REPAIR_OBJECT_ID );

            //
            //  Fire off the quota cleanup if quotas are enabled.
            //

            if (FlagOn( Vcb->QuotaFlags, (QUOTA_FLAG_TRACKING_REQUESTED |
                                      QUOTA_FLAG_TRACKING_ENABLED |
                                      QUOTA_FLAG_ENFORCEMENT_ENABLED ))) {

                NtfsMarkQuotaCorrupt( IrpContext, Vcb );
            }
        }

        //
        //  Start the object ID cleanup if we were mounted on 4.0 or had started
        //  in a previous mount.
        //

        if (FlagOn( VolumeFlags, VOLUME_REPAIR_OBJECT_ID )) {

            NtfsPostSpecial( IrpContext, Vcb, NtfsRepairObjectId, NULL );
        }

        //
        //  Clear the MOUNTED_ON_40 and CHKDSK_MODIFIED flags if set.
        //

        if (FlagOn( VolumeFlags, VOLUME_MOUNTED_ON_40 | VOLUME_MODIFIED_BY_CHKDSK )) {

            NtfsSetVolumeInfoFlagState( IrpContext,
                                        Vcb,
                                        VOLUME_MOUNTED_ON_40 | VOLUME_MODIFIED_BY_CHKDSK,
                                        FALSE,
                                        TRUE );
        }

        //
        //  Looks like this mount will succeed.  Remember the root directory fileobject
        //  so we can use it for the notification later.
        //

        RootDirFileObject = Vcb->RootIndexScb->FileObject;

        //
        //  Dereference the root file object if present.  The absence of this doesn't
        //  indicate whether the volume was upgraded.  Older 4K Mft records can contain
        //  all of the new streams.
        //

        if (RootDirFileObject != NULL) {

            ObReferenceObject( RootDirFileObject );
        }

        //
        //
        //  Set our return status and say that the mount succeeded
        //

        Status = STATUS_SUCCESS;
        MountFailed = FALSE;
        SetFlag( Vcb->VcbState, VCB_STATE_MOUNT_COMPLETED );

#ifdef SYSCACHE_DEBUG
        if (!NtfsIsVolumeReadOnly( Vcb ) && !NtfsDisableSyscacheLogFile) {
            NtfsInitializeSyscacheLogFile( IrpContext, Vcb );
        }
#endif


    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsMountVolume );

        NtfsUnpinBcb( IrpContext, &BootBcb );

        if (DeviceObjectName != NULL) {

            NtfsFreePool( DeviceObjectName );
        }

        if (CloseAttributes) { NtfsCloseAttributesFromRestart( IrpContext, Vcb ); }

        for (i = 0; i < 8; i++) {

            NtfsUnpinBcb( IrpContext, &Bcbs[i] );

            //
            //  Get rid of the Mdls, if we haven't already.
            //

            if (Mdls[i] != NULL) {

                if (FlagOn(Mdls[i]->MdlFlags, MDL_PAGES_LOCKED )) {
                    MmUnlockPages( Mdls[i] );
                }
                IoFreeMdl( Mdls[i] );
                Mdls[i] = NULL;
            }
        }

        if (BootScb != NULL) {  NtfsDeleteInternalAttributeStream( BootScb, TRUE, FALSE ); }

        if (Vcb != NULL) {

            if (Vcb->MftScb != NULL)               { NtfsReleaseScb( IrpContext, Vcb->MftScb ); }
            if (Vcb->Mft2Scb != NULL)              { NtfsReleaseScb( IrpContext, Vcb->Mft2Scb ); }
            if (Vcb->LogFileScb != NULL)           { NtfsReleaseScb( IrpContext, Vcb->LogFileScb ); }
            if (Vcb->VolumeDasdScb != NULL)        { NtfsReleaseScb( IrpContext, Vcb->VolumeDasdScb ); }
            if (Vcb->AttributeDefTableScb != NULL) { NtfsReleaseScb( IrpContext, Vcb->AttributeDefTableScb );
                                                     NtfsDeleteInternalAttributeStream( Vcb->AttributeDefTableScb, TRUE, FALSE );
                                                     Vcb->AttributeDefTableScb = NULL;}
            if (Vcb->UpcaseTableScb != NULL)       { NtfsReleaseScb( IrpContext, Vcb->UpcaseTableScb );
                                                     NtfsDeleteInternalAttributeStream( Vcb->UpcaseTableScb, TRUE, FALSE );
                                                     Vcb->UpcaseTableScb = NULL;}
            if (Vcb->RootIndexScb != NULL)         { NtfsReleaseScb( IrpContext, Vcb->RootIndexScb ); }
            if (Vcb->BitmapScb != NULL)            { NtfsReleaseScb( IrpContext, Vcb->BitmapScb ); }
            if (Vcb->BadClusterFileScb != NULL)    { NtfsReleaseScb( IrpContext, Vcb->BadClusterFileScb ); }
            if (Vcb->MftBitmapScb != NULL)         { NtfsReleaseScb( IrpContext, Vcb->MftBitmapScb ); }

            //
            //  Drop the security  data
            //

            if (Vcb->SecurityDescriptorStream != NULL) { NtfsReleaseScb( IrpContext, Vcb->SecurityDescriptorStream ); }
            if (Vcb->UsnJournal != NULL) { NtfsReleaseScb( IrpContext, Vcb->UsnJournal ); }
            if (Vcb->ExtendDirectory != NULL) { NtfsReleaseScb( IrpContext, Vcb->ExtendDirectory ); }
            if (QuotaDataScb != NULL) {
                NtfsReleaseScb( IrpContext, QuotaDataScb );
                NtfsDeleteInternalAttributeStream( QuotaDataScb, TRUE, FALSE );
            }

#if 0
            if (MountFailed) {

                PVPB NewVpb;

                NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE, &NewVpb );

                //
                //  If the version upgrade failed, we will be coming back in here soon
                //  and we need to have the right vpb when we do.  This is true if the
                //  upgrade failed or if we are processing a log file full condition.
                //

                if ((FlagOn( IrpContext->State, IRP_CONTEXT_STATE_VOL_UPGR_FAILED ) ||
                     (IrpContext->TopLevelIrpContext->ExceptionStatus == STATUS_LOG_FILE_FULL) ||
                     (IrpContext->TopLevelIrpContext->ExceptionStatus == STATUS_CANT_WAIT)) &&

                    (NewVpb != NULL)) {

                    IrpSp->Parameters.MountVolume.Vpb = NewVpb;
                }

                //
                //  On abnormal termination, someone will try to abort a transaction on
                //  this Vcb if we do not clear these fields.
                //

                IrpContext->TransactionId = 0;
                IrpContext->Vcb = NULL;
            }
#endif

        }

        if (VcbAcquired) {

            NtfsReleaseVcbCheckDelete( IrpContext, Vcb, IRP_MJ_FILE_SYSTEM_CONTROL, NULL );
        }

        NtfsReleaseGlobal( IrpContext );
    }

#if 0
    NtfsCompleteRequest( IrpContext, Irp, Status );
#endif

    if (RootDirFileObject != NULL) {

        FsRtlNotifyVolumeEvent( RootDirFileObject, FSRTL_VOLUME_MOUNT );
        ObDereferenceObject( RootDirFileObject );
    }

    if (NT_SUCCESS( Status )) {

        //
        //  Remove the extra object reference to the target device object
        //  because I/O system has already made one for this mount.
        //

        ObDereferenceObject( Vcb->TargetDeviceObject );
    }

    DebugTrace( -1, Dbg, ("NtfsMountVolume -> %08lx\n", Status) );

    return Status;
}


#endif
