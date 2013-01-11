#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#define BugCheckFileId                   (FAT_BUG_CHECK_FLUSH)

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')



BOOLEAN
FatIsMediaWriteProtected (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject
    );


NTSTATUS
NdasFatPnpQueryRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine handles the PnP query remove operation.  The filesystem
    is responsible for answering whether there are any reasons it sees
    that the volume can not go away (and the device removed).  Initiation
    of the dismount begins when we answer yes to this question.
    
    Query will be followed by a Cancel or Remove.

Arguments:

    Irp - Supplies the Irp to process
    
    Vcb - Supplies the volume being queried.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    KEVENT Event;
    BOOLEAN VcbDeleted = FALSE;
#if 0
    BOOLEAN GlobalHeld = TRUE;
#else
	BOOLEAN GlobalHeld = FALSE;
#endif

    PAGED_CODE();

    //
    //  Having said yes to a QUERY, any communication with the
    //  underlying storage stack is undefined (and may block)
    //  until the bounding CANCEL or REMOVE is sent.
    //

    FatAcquireExclusiveVcb( IrpContext, Vcb );

#if 0
    FatReleaseGlobal( IrpContext);
    GlobalHeld = FALSE;
#endif

    try {
        
        Status = FatLockVolumeInternal( IrpContext, Vcb, NULL );

        //
        //  Drop an additional reference on the Vpb so that the volume cannot be
        //  torn down when we drop all the locks below.
        //
        
        FatPnpAdjustVpbRefCount( Vcb, 1);
        
        //
        //  Drop and reacquire the resources in the right order.
        //

        FatReleaseVcb( IrpContext, Vcb );
        FatAcquireExclusiveGlobal( IrpContext );
        GlobalHeld = TRUE;
        FatAcquireExclusiveVcb( IrpContext, Vcb );

        //
        //  Drop the reference we added above.
        //
        
        FatPnpAdjustVpbRefCount( Vcb, -1);

        if (NT_SUCCESS( Status )) {

            //
            //  With the volume held locked, note that we must finalize as much
            //  as possible right now.
            //

            FatFlushAndCleanVolume( IrpContext, Irp, Vcb, Flush );

#if 0

            //
            //  We need to pass this down before starting the dismount, which
            //  could disconnect us immediately from the stack.
            //

            //
            //  Get the next stack location, and copy over the stack location
            //

            IoCopyCurrentIrpStackLocationToNext( Irp );

            //
            //  Set up the completion routine
            //

            KeInitializeEvent( &Event, NotificationEvent, FALSE );
            IoSetCompletionRoutine( Irp,
                                    FatPnpCompletionRoutine,
                                    &Event,
                                    TRUE,
                                    TRUE,
                                    TRUE );

            //
            //  Send the request and wait.
            //

            Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);

            if (Status == STATUS_PENDING) {

                KeWaitForSingleObject( &Event,
                                       Executive,
                                       KernelMode,
                                       FALSE,
                                       NULL );

                Status = Irp->IoStatus.Status;
            }

            //
            //  Now if no one below us failed already, initiate the dismount
            //  on this volume, make it go away.  PnP needs to see our internal
            //  streams close and drop their references to the target device.
            //
            //  Since we were able to lock the volume, we are guaranteed to
            //  move this volume into dismount state and disconnect it from
            //  the underlying storage stack.  The force on our part is actually
            //  unnecesary, though complete.
            //
            //  What is not strictly guaranteed, though, is that the closes
            //  for the metadata streams take effect synchronously underneath
            //  of this call.  This would leave references on the target device
            //  even though we are disconnected!
            //

            if (NT_SUCCESS( Status )) {

                VcbDeleted = FatCheckForDismount( IrpContext, Vcb, TRUE );

                ASSERT( VcbDeleted || Vcb->VcbCondition == VcbBad );

            }

#endif

        }

    } finally {
        
        //
        //  Release the Vcb if it could still remain.
        //

        if (!VcbDeleted) {

            FatReleaseVcb( IrpContext, Vcb );
        }

        if (GlobalHeld) {
            
            FatReleaseGlobal( IrpContext );
        }
    }
    
#if 0

    //
    //  Cleanup our IrpContext and complete the IRP if neccesary.
    //

    FatCompleteRequest( IrpContext, Irp, Status );

#endif

    return Status;
}


VOID
NdasFatTearDownVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine tries to remove all internal opens from the volume.

Arguments:

    IrpContext - Supplies the context for the overall request.

    Vcb - Supplies the Vcb to be torn down.

Return Value:

    None

--*/

{
    PFILE_OBJECT DirectoryFileObject;


    PAGED_CODE();

    //
    //  Get rid of the virtual volume file, if we need to.
    //

    if (Vcb->VirtualVolumeFile != NULL) {

        //
        //  Uninitialize the cache
        //

        FatSyncUninitializeCacheMap( IrpContext, Vcb->VirtualVolumeFile );

        //
        //  Dereference the virtual volume file.  This will cause a close
        //  Irp to be processed, so we need to do this before we destory
        //  the Vcb
        //

#if 0
        FsRtlTeardownPerStreamContexts( &Vcb->VolumeFileHeader );
#else
		if (FatFsRtlTeardownPerStreamContexts)
			(*FatFsRtlTeardownPerStreamContexts)( &Vcb->VolumeFileHeader );
#endif

        ObDereferenceObject( Vcb->VirtualVolumeFile );

        Vcb->VirtualVolumeFile = NULL;
    }

    //
    //  Close down the EA file.
    //

    FatCloseEaFile( IrpContext, Vcb, FALSE );

    //
    //  Close down the root directory stream..
    //

    if (Vcb->RootDcb != NULL) {

        DirectoryFileObject = Vcb->RootDcb->Specific.Dcb.DirectoryFile;

        if (DirectoryFileObject != NULL) {

            //
            //  Tear down this directory file.
            //

            FatSyncUninitializeCacheMap( IrpContext,
                                         DirectoryFileObject );

            Vcb->RootDcb->Specific.Dcb.DirectoryFile = NULL;
            ObDereferenceObject( DirectoryFileObject );
        }
    }

    //
    //  The VCB can no longer be used.
    //

    FatSetVcbCondition( Vcb, VcbBad );
}


VOID
NdasFatDeleteVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine removes the Vcb record from Fat's in-memory data
    structures.  It also will remove all associated underlings
    (i.e., FCB records).

Arguments:

    Vcb - Supplies the Vcb to be removed

Return Value:

    None

--*/

{
    PFCB Fcb;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "FatDeleteVcb, Vcb = %08lx\n", Vcb);

    //
    //  If the IrpContext points to the VCB being deleted NULL out the stail
    //  pointer.
    //

    if (IrpContext->Vcb == Vcb) {

        IrpContext->Vcb = NULL;

    }

    //
    //  Chuck the backpocket Vpb we kept just in case.
    //

    if (Vcb->SwapVpb) {

        ExFreePool( Vcb->SwapVpb );

    }

    //
    //  Free the VPB, if we need to.
    //

    if (FlagOn( Vcb->VcbState, VCB_STATE_FLAG_VPB_MUST_BE_FREED )) {

        //
        //  We swapped the VPB, so we need to free the main one.
        //

        ExFreePool( Vcb->Vpb );
    }
    
    //
    //  Free the close context for the virtual volume file, if it is still
    //  present.  If this is the case, it means the last close that came
    //  through was for the virtual volume file, and we are processing that
    //  close right now.
    //

    if (Vcb->CloseContext != NULL) {

        ExFreePool( Vcb->CloseContext );
    }

    //
    //  Remove this record from the global list of all Vcb records.
    //  Note that the global lock must already be held when calling
    //  this function.
    //

    RemoveEntryList( &(Vcb->VcbLinks) );

    //
    //  Make sure the direct access open count is zero, and the open file count
    //  is also zero.
    //

    if ((Vcb->DirectAccessOpenCount != 0) || (Vcb->OpenFileCount != 0)) {

        FatBugCheck( 0, 0, 0 );
    }

    //
    //  Remove the EaFcb and dereference the Fcb for the Ea file if it
    //  exists.
    //

    if (Vcb->EaFcb != NULL) {

        Vcb->EaFcb->OpenCount = 0;
        FatDeleteFcb( IrpContext, &Vcb->EaFcb );
    }

    //
    //  Remove the Root Dcb
    //

    if (Vcb->RootDcb != NULL) {

        //
        //  Rundown stale child Fcbs that may be hanging around.  Yes, this
        //  can happen.  No, the create path isn't perfectly defensive about
        //  tearing down branches built up on creates that don't wind up
        //  succeeding.  Normal system operation usually winds up having
        //  cleaned them out through re-visiting, but ...
        //
        //  Just pick off Fcbs from the bottom of the tree until we run out.
        //  Then we delete the root Dcb.
        //

        while( (Fcb = FatGetNextFcbBottomUp( IrpContext, NULL, Vcb->RootDcb )) != Vcb->RootDcb ) {

            FatDeleteFcb( IrpContext, &Fcb );
        }

        FatDeleteFcb( IrpContext, &Vcb->RootDcb );
    }

    //
    //  Uninitialize the notify sychronization object.
    //

    FsRtlNotifyUninitializeSync( &Vcb->NotifySync );

    //
    //  Uninitialize the resource variable for the Vcb
    //

    FatDeleteResource( &Vcb->Resource );
    FatDeleteResource( &Vcb->ChangeBitMapResource );

    //
    //  If allocation support has been setup, free it.
    //

    if (Vcb->FreeClusterBitMap.Buffer != NULL) {

        FatTearDownAllocationSupport( IrpContext, Vcb );
    }

    //
    //  UnInitialize the Mcb structure that kept track of dirty fat sectors.
    //

    FsRtlUninitializeLargeMcb( &Vcb->DirtyFatMcb );

    //
    //  Free the pool for the stached copy of the boot sector
    //

    if ( Vcb->First0x24BytesOfBootSector ) {

        ExFreePool( Vcb->First0x24BytesOfBootSector );
        Vcb->First0x24BytesOfBootSector = NULL;
    }

    //
    //  Cancel the CleanVolume Timer and Dpc
    //

    (VOID)KeCancelTimer( &Vcb->CleanVolumeTimer );

    (VOID)KeRemoveQueueDpc( &Vcb->CleanVolumeDpc );

    //
    //  Free the performance counters memory
    //

    ExFreePool( Vcb->Statistics );

    //
    //  Clean out the tunneling cache
    //

    FsRtlDeleteTunnelCache(&Vcb->Tunnel);

#if 0

    //
    // Dereference the target device object.
    //

    ObDereferenceObject( Vcb->TargetDeviceObject );

    //
    //  We better have used all the close contexts we allocated. There could be
    //  one remaining if we're doing teardown due to a final close coming in on
    //  a directory file stream object.  It will be freed on the way back up.
    //

    ASSERT( Vcb->CloseContextCount <= 1);

    //
    //  And zero out the Vcb, this will help ensure that any stale data is
    //  wiped clean
    //

    RtlZeroMemory( Vcb, sizeof(VCB) );

#endif

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatDeleteVcb -> VOID\n", 0);

    return;
}


NTSTATUS
NdasFatMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb,
    IN PDEVICE_OBJECT FsDeviceObject
    )

/*++

Routine Description:

    This routine performs the mount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

    Its job is to verify that the volume denoted in the IRP is a Fat volume,
    and create the VCB and root DCB structures.  The algorithm it uses is
    essentially as follows:

    1. Create a new Vcb Structure, and initialize it enough to do cached
       volume file I/O.

    2. Read the disk and check if it is a Fat volume.

    3. If it is not a Fat volume then free the cached volume file, delete
       the VCB, and complete the IRP with STATUS_UNRECOGNIZED_VOLUME

    4. Check if the volume was previously mounted and if it was then do a
       remount operation.  This involves reinitializing the cached volume
       file, checking the dirty bit, resetting up the allocation support,
       deleting the VCB, hooking in the old VCB, and completing the IRP.

    5. Otherwise create a root DCB, create Fsp threads as necessary, and
       complete the IRP.

Arguments:

    TargetDeviceObject - This is where we send all of our requests.

    Vpb - This gives us additional information needed to complete the mount.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
#if 0
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( IrpContext->OriginatingIrp );
#endif
    NTSTATUS Status;

    PBCB BootBcb;
    PPACKED_BOOT_SECTOR BootSector;

    PBCB DirentBcb;
    PDIRENT Dirent;
    ULONG ByteOffset;

    BOOLEAN MountNewVolume = FALSE;
    BOOLEAN WeClearedVerifyRequiredBit = FALSE;
    BOOLEAN DoARemount = FALSE;

#if 0
    PVCB OldVcb;
    PVPB OldVpb;
#endif

    PDEVICE_OBJECT RealDevice;
    PVOLUME_DEVICE_OBJECT VolDo = NULL;
    PVCB Vcb = NULL;
    PFILE_OBJECT RootDirectoryFile = NULL;

    PLIST_ENTRY Links;

    IO_STATUS_BLOCK Iosb;
    ULONG ChangeCount = 0;

    DISK_GEOMETRY Geometry;

    PARTITION_INFORMATION_EX PartitionInformation;
    NTSTATUS StatusPartInfo;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "FatMountVolume\n", 0);
    DebugTrace( 0, Dbg, "TargetDeviceObject = %08lx\n", TargetDeviceObject);
    DebugTrace( 0, Dbg, "Vpb                = %08lx\n", Vpb);

    ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
#if 0
    ASSERT( FatDeviceIsFatFsdo( FsDeviceObject));
#endif

#if 0
    //
    //  Verify that there is a disk here and pick up the change count.
    //

    Status = FatPerformDevIoCtrl( IrpContext,
                                  IOCTL_DISK_CHECK_VERIFY,
                                  TargetDeviceObject,
                                  &ChangeCount,
                                  sizeof(ULONG),
                                  FALSE,
                                  TRUE,
                                  &Iosb );

    if (!NT_SUCCESS( Status )) {

        //
        //  If we will allow a raw mount then avoid sending the popup.
        //
        //  Only send this on "true" disk devices to handle the accidental
        //  legacy of FAT. No other FS will throw a harderror on empty
        //  drives.
        //
        //  Cmd should really handle this per 9x.
        //

        if (!FlagOn( IrpSp->Flags, SL_ALLOW_RAW_MOUNT ) &&
            Vpb->RealDevice->DeviceType == FILE_DEVICE_DISK) {

            FatNormalizeAndRaiseStatus( IrpContext, Status );
        }

        return Status;
    }

    if (Iosb.Information != sizeof(ULONG)) {

        //
        //  Be safe about the count in case the driver didn't fill it in
        //

        ChangeCount = 0;
    }

#endif

    //
    //  If this is a CD class device,  then check to see if there is a 
    //  'data track' or not.  This is to avoid issuing paging reads which will
    //  fail later in the mount process (e.g. CD-DA or blank CD media)
    //

    if ((TargetDeviceObject->DeviceType == FILE_DEVICE_CD_ROM) &&
        !FatScanForDataTrack( IrpContext, TargetDeviceObject))  {

        return STATUS_UNRECOGNIZED_VOLUME;
    }

    //
    //  Ping the volume with a partition query and pick up the partition
    //  type.  We'll check this later to avoid some scurrilous volumes.
    //

#if __NDAS_FAT_WIN2K_SUPPORT__

	if (!IS_WINDOWSXP_OR_LATER()) {

		StatusPartInfo = FatPerformDevIoCtrl( IrpContext,
			                                  IOCTL_DISK_GET_PARTITION_INFO,
				                              TargetDeviceObject,
					                          &PartitionInformation,
						                      sizeof(PARTITION_INFORMATION),
							                  FALSE,
								              TRUE,
									          &Iosb );
	} else {

		StatusPartInfo = FatPerformDevIoCtrl( IrpContext,
			                                  IOCTL_DISK_GET_PARTITION_INFO_EX,
				                              TargetDeviceObject,
					                          &PartitionInformation,
						                      sizeof(PARTITION_INFORMATION_EX),
							                  FALSE,
								              TRUE,
									          &Iosb );
	}

#else

    StatusPartInfo = FatPerformDevIoCtrl( IrpContext,
                                          IOCTL_DISK_GET_PARTITION_INFO_EX,
                                          TargetDeviceObject,
                                          &PartitionInformation,
                                          sizeof(PARTITION_INFORMATION_EX),
                                          FALSE,
                                          TRUE,
                                          &Iosb );

#endif

    //
    //  Make sure we can wait.
    //

    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);

    //
    //  Do a quick check to see if there any Vcb's which can be removed.
    //

#if 0
    FatScanForDismountedVcb( IrpContext );
#endif

    //
    //  Initialize the Bcbs and our final state so that the termination
    //  handlers will know what to free or unpin
    //

    BootBcb = NULL;
    DirentBcb = NULL;

    Vcb = NULL;
    VolDo = NULL;
    MountNewVolume = FALSE;

    try {

        //
        //  Synchronize with FatCheckForDismount(), which modifies the vpb.
        //

        (VOID)FatAcquireExclusiveGlobal( IrpContext );

        //
        //  Create a new volume device object.  This will have the Vcb
        //  hanging off of its end, and set its alignment requirement
        //  from the device we talk to.
        //

#if 0

        if (!NT_SUCCESS(Status = IoCreateDevice( FatData.DriverObject,
                                                 sizeof(VOLUME_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
                                                 NULL,
                                                 FILE_DEVICE_DISK_FILE_SYSTEM,
                                                 0,
                                                 FALSE,
                                                 (PDEVICE_OBJECT *)&VolDo))) {

            try_return( Status );
        }

#else

		Vcb = IrpContext->Vcb;
		VolDo = CONTAINING_RECORD( Vcb, VOLUME_DEVICE_OBJECT, Vcb );

#endif

        //
        //  Our alignment requirement is the larger of the processor alignment requirement
        //  already in the volume device object and that in the TargetDeviceObject
        //

        if (TargetDeviceObject->AlignmentRequirement > VolDo->DeviceObject.AlignmentRequirement) {

            VolDo->DeviceObject.AlignmentRequirement = TargetDeviceObject->AlignmentRequirement;
        }

        //
        //  Initialize the overflow queue for the volume
        //

        VolDo->OverflowQueueCount = 0;
        InitializeListHead( &VolDo->OverflowQueue );

        VolDo->PostedRequestCount = 0;
        KeInitializeSpinLock( &VolDo->OverflowQueueSpinLock );

        //
        //  We must initialize the stack size in our device object before
        //  the following reads, because the I/O system has not done it yet.
        //  This must be done before we clear the device initializing flag
        //  otherwise a filter could attach and copy the wrong stack size into
        //  it's device object.
        //

        VolDo->DeviceObject.StackSize = (CCHAR)(TargetDeviceObject->StackSize + 1);

        //
        //  We must also set the sector size correctly in our device object 
        //  before clearing the device initializing flag.
        //
        
        Status = FatPerformDevIoCtrl( IrpContext,
                                      IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                      TargetDeviceObject,
                                      &Geometry,
                                      sizeof( DISK_GEOMETRY ),
                                      FALSE,
                                      TRUE,
                                      NULL );

        VolDo->DeviceObject.SectorSize = (USHORT)Geometry.BytesPerSector;

        //
        //  Indicate that this device object is now completely initialized
        //

        ClearFlag(VolDo->DeviceObject.Flags, DO_DEVICE_INITIALIZING);

        //
        //  Now Before we can initialize the Vcb we need to set up the device
        //  object field in the Vpb to point to our new volume device object.
        //  This is needed when we create the virtual volume file's file object
        //  in initialize vcb.
        //

        Vpb->DeviceObject = (PDEVICE_OBJECT)VolDo;

        //
        //  If the real device needs verification, temporarily clear the
        //  field.
        //

        RealDevice = Vpb->RealDevice;

        if ( FlagOn(RealDevice->Flags, DO_VERIFY_VOLUME) ) {

            ClearFlag(RealDevice->Flags, DO_VERIFY_VOLUME);

            WeClearedVerifyRequiredBit = TRUE;
        }

        //
        //  Initialize the new vcb
        //

        FatInitializeVcb( IrpContext, 
                          &VolDo->Vcb, 
                          TargetDeviceObject, 
                          Vpb, 
                          FsDeviceObject);
        //
        //  Get a reference to the Vcb hanging off the end of the device object
        //

        Vcb = &VolDo->Vcb;

        //
        //  Read in the boot sector, and have the read be the minumum size
        //  needed.  We know we can wait.
        //

        //
        //  We need to commute errors on CD so that CDFS will get its crack.  Audio
        //  and even data media may not be universally readable on sector zero.        
        //
        
        try {
        
            FatReadVolumeFile( IrpContext,
                               Vcb,
                               0,                          // Starting Byte
                               sizeof(PACKED_BOOT_SECTOR),
                               &BootBcb,
                               (PVOID *)&BootSector );
        
        } except( Vpb->RealDevice->DeviceType == FILE_DEVICE_CD_ROM ?
                  EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {

              NOTHING;
        }

        //
        //  Call a routine to check the boot sector to see if it is fat
        //

        if (BootBcb == NULL || !FatIsBootSectorFat( BootSector)) {

            DebugTrace(0, Dbg, "Not a Fat Volume\n", 0);
        
            //
            //  Complete the request and return to our caller
            //

            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  Unpack the BPB.  We used to do some sanity checking of the FATs at
        //  this point, but authoring errors on third-party devices prevent
        //  us from continuing to safeguard ourselves.  We can only hope the
        //  boot sector check is good enough.
        //
        //  (read: digital cameras)
        //
        //  Win9x does the same.
        //

        FatUnpackBios( &Vcb->Bpb, &BootSector->PackedBpb );

        //
        //  Check if we have an OS/2 Boot Manager partition and treat it as an
        //  unknown file system.  We'll check the partition type in from the
        //  partition table and we ensure that it has less than 0x80 sectors,
        //  which is just a heuristic that will capture all real OS/2 BM partitions
        //  and avoid the chance we'll discover partitions which erroneously
        //  (but to this point, harmlessly) put down the OS/2 BM type.
        //
        //  Note that this is only conceivable on good old MBR media.
        //
        //  The OS/2 Boot Manager boot format mimics a FAT16 partition in sector
        //  zero but does is not a real FAT16 file system.  For example, the boot
        //  sector indicates it has 2 FATs but only really has one, with the boot
        //  manager code overlaying the second FAT.  If we then set clean bits in
        //  FAT[0] we'll corrupt that code.
        //

#if __NDAS_FAT_WIN2K_SUPPORT__

		if (!IS_WINDOWSXP_OR_LATER()) {

	        if (( NT_SUCCESS( StatusPartInfo )) &&
		        ( ((PARTITION_INFORMATION *)(&PartitionInformation))->PartitionType == PARTITION_OS2BOOTMGR ) &&
			    ( Vcb->Bpb.Sectors != 0 ) &&
	            ( Vcb->Bpb.Sectors < 0x80 )) {

		        DebugTrace( 0, Dbg, "OS/2 Boot Manager volume detected, volume not mounted. \n", 0 );
			    //
				//  Complete the request and return to our caller
				//
	            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
		    }
		
		} else {

	       if (NT_SUCCESS( StatusPartInfo ) &&
		        (PartitionInformation.PartitionStyle == PARTITION_STYLE_MBR &&
			     PartitionInformation.Mbr.PartitionType == PARTITION_OS2BOOTMGR) &&
				(Vcb->Bpb.Sectors != 0 &&
	             Vcb->Bpb.Sectors < 0x80)) {

		        DebugTrace( 0, Dbg, "OS/2 Boot Manager volume detected, volume not mounted. \n", 0 );
            
			    //
				//  Complete the request and return to our caller
	            //
            
		        try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
			}
		}

#else

        if (NT_SUCCESS( StatusPartInfo ) &&
            (PartitionInformation.PartitionStyle == PARTITION_STYLE_MBR &&
             PartitionInformation.Mbr.PartitionType == PARTITION_OS2BOOTMGR) &&
            (Vcb->Bpb.Sectors != 0 &&
             Vcb->Bpb.Sectors < 0x80)) {

            DebugTrace( 0, Dbg, "OS/2 Boot Manager volume detected, volume not mounted. \n", 0 );
            
            //
            //  Complete the request and return to our caller
            //
            
            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

#endif

        //
        //  Verify that the sector size recorded in the Bpb matches what the
        //  device currently reports it's sector size to be.
        //

        if ( !NT_SUCCESS( Status) || 
             (Geometry.BytesPerSector != Vcb->Bpb.BytesPerSector))  {

            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  This is a fat volume, so extract the bpb, serial number.  The
        //  label we'll get later after we've created the root dcb.
        //
        //  Note that the way data caching is done, we set neither the
        //  direct I/O or Buffered I/O bit in the device object flags.
        //

        if (Vcb->Bpb.Sectors != 0) { Vcb->Bpb.LargeSectors = 0; }

        if (IsBpbFat32(&BootSector->PackedBpb)) {

            CopyUchar4( &Vpb->SerialNumber, ((PPACKED_BOOT_SECTOR_EX)BootSector)->Id );

        } else  {

            CopyUchar4( &Vpb->SerialNumber, BootSector->Id );

            //
            //  Allocate space for the stashed boot sector chunk.  This only has meaning on
            //  FAT12/16 volumes since this only is kept for the FSCTL_QUERY_FAT_BPB and it and
            //  its users are a bit wierd, thinking that a BPB exists wholly in the first 0x24
            //  bytes.
            //

            Vcb->First0x24BytesOfBootSector =
                FsRtlAllocatePoolWithTag( PagedPool,
                                          0x24,
                                          TAG_STASHED_BPB );

            //
            //  Stash a copy of the first 0x24 bytes
            //

            RtlCopyMemory( Vcb->First0x24BytesOfBootSector,
                           BootSector,
                           0x24 );
        }

        //
        //  Now unpin the boot sector, so when we set up allocation eveything
        //  works.
        //

        FatUnpinBcb( IrpContext, BootBcb );

        //
        //  Compute a number of fields for Vcb.AllocationSupport
        //

        FatSetupAllocationSupport( IrpContext, Vcb );

        //
        //  Sanity check the FsInfo information for FAT32 volumes.  Silently deal
        //  with messed up information by effectively disabling FsInfo updates.
        //

        if (FatIsFat32( Vcb )) {

            if (Vcb->Bpb.FsInfoSector >= Vcb->Bpb.ReservedSectors) {

                Vcb->Bpb.FsInfoSector = 0;
            }
        }

        //
        //  Create a root Dcb so we can read in the volume label.  If this is FAT32, we can
        //  discover corruption in the FAT chain.
        //
        //  NOTE: this exception handler presumes that this is the only spot where we can
        //  discover corruption in the mount process.  If this ever changes, this handler
        //  MUST be expanded.  The reason we have this guy here is because we have to rip
        //  the structures down now (in the finally below) and can't wait for the outer
        //  exception handling to do it for us, at which point everything will have vanished.
        //

        try {

            FatCreateRootDcb( IrpContext, Vcb );

        } except (GetExceptionCode() == STATUS_FILE_CORRUPT_ERROR ? EXCEPTION_EXECUTE_HANDLER :
                                                                    EXCEPTION_CONTINUE_SEARCH) {

            //
            //  The volume needs to be dirtied, do it now.  Note that at this point we have built
            //  enough of the Vcb to pull this off.
            //

            FatMarkVolume( IrpContext, Vcb, VolumeDirty );

            //
            //  Now keep bailing out ...
            //

            FatRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR );
        }

        FatLocateVolumeLabel( IrpContext,
                              Vcb,
                              &Dirent,
                              &DirentBcb,
                              &ByteOffset );

        if (Dirent != NULL) {

            OEM_STRING OemString;
            UNICODE_STRING UnicodeString;

            //
            //  Compute the length of the volume name
            //

            OemString.Buffer = &Dirent->FileName[0];
            OemString.MaximumLength = 11;

            for ( OemString.Length = 11;
                  OemString.Length > 0;
                  OemString.Length -= 1) {

                if ( (Dirent->FileName[OemString.Length-1] != 0x00) &&
                     (Dirent->FileName[OemString.Length-1] != 0x20) ) { break; }
            }

            UnicodeString.MaximumLength = MAXIMUM_VOLUME_LABEL_LENGTH;
            UnicodeString.Buffer = &Vcb->Vpb->VolumeLabel[0];

            Status = RtlOemStringToCountedUnicodeString( &UnicodeString,
                                                         &OemString,
                                                         FALSE );

            if ( !NT_SUCCESS( Status ) ) {

                try_return( Status );
            }

            Vpb->VolumeLabelLength = UnicodeString.Length;

        } else {

            Vpb->VolumeLabelLength = 0;
        }

        //
        //  Use the change count we noted initially *before* doing any work.
        //  If something came along in the midst of this operation, we'll
        //  verify and discover the problem.
        //

        Vcb->ChangeCount = ChangeCount;

#if 0

        //
        //  Now scan the list of previously mounted volumes and compare
        //  serial numbers and volume labels off not currently mounted
        //  volumes to see if we have a match.
        //

        for (Links = FatData.VcbQueue.Flink;
             Links != &FatData.VcbQueue;
             Links = Links->Flink) {

            OldVcb = CONTAINING_RECORD( Links, VCB, VcbLinks );
            OldVpb = OldVcb->Vpb;

            //
            //  Skip over ourselves since we're already in the VcbQueue
            //

            if (OldVpb == Vpb) { continue; }

            //
            //  Check for a match:
            //
            //  Serial Number, VolumeLabel and Bpb must all be the same.
            //  Also the volume must have failed a verify before (ie.
            //  VolumeNotMounted), and it must be in the same physical
            //  drive than it was mounted in before.
            //

            if ( (OldVpb->SerialNumber == Vpb->SerialNumber) &&
                 (OldVcb->VcbCondition == VcbNotMounted) &&
                 (OldVpb->RealDevice == RealDevice) &&
                 (OldVpb->VolumeLabelLength == Vpb->VolumeLabelLength) &&
                 (RtlEqualMemory(&OldVpb->VolumeLabel[0],
                                 &Vpb->VolumeLabel[0],
                                 Vpb->VolumeLabelLength)) &&
                 (RtlEqualMemory(&OldVcb->Bpb,
                                 &Vcb->Bpb,
                                 IsBpbFat32(&Vcb->Bpb) ?
                                     sizeof(BIOS_PARAMETER_BLOCK) :
                                     FIELD_OFFSET(BIOS_PARAMETER_BLOCK,
                                                  LargeSectorsPerFat) ))) {

                DoARemount = TRUE;

                break;
            }
        }

        if ( DoARemount ) {

            PVPB *IrpVpb;

            DebugTrace(0, Dbg, "Doing a remount\n", 0);
            DebugTrace(0, Dbg, "Vcb = %08lx\n", Vcb);
            DebugTrace(0, Dbg, "Vpb = %08lx\n", Vpb);
            DebugTrace(0, Dbg, "OldVcb = %08lx\n", OldVcb);
            DebugTrace(0, Dbg, "OldVpb = %08lx\n", OldVpb);

            //
            //  Swap target device objects between the VCBs. That way
            //  the old VCB will start using the new target device object,
            //  and the new VCB will be torn down and deference the old
            //  target device object.
            //

            Vcb->TargetDeviceObject = OldVcb->TargetDeviceObject;
            OldVcb->TargetDeviceObject = TargetDeviceObject;

            //
            //  This is a remount, so link the old vpb in place
            //  of the new vpb.
            //

            ASSERT( !FlagOn( OldVcb->VcbState, VCB_STATE_FLAG_VPB_MUST_BE_FREED ) );

            FatSetVcbCondition( OldVcb, VcbGood);
            OldVpb->RealDevice = Vpb->RealDevice;
            ClearFlag( OldVcb->VcbState, VCB_STATE_VPB_NOT_ON_DEVICE);

            OldVpb->RealDevice->Vpb = OldVpb;

            //
            //  Use the new changecount.
            //

            OldVcb->ChangeCount = Vcb->ChangeCount;

            //
            //  If the new VPB is the VPB referenced in the original Irp, set
            //  that reference back to the old VPB.
            //

            IrpVpb = &IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp)->Parameters.MountVolume.Vpb;

            if (*IrpVpb == Vpb) {

                *IrpVpb = OldVpb;
            }

            //
            //  We do not want to touch this VPB again.  It will get cleaned up when
            //  the new VCB is cleaned up.
            //

            ASSERT( Vcb->Vpb == Vpb );

            Vpb = NULL;
            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_VPB_MUST_BE_FREED );
            FatSetVcbCondition( Vcb, VcbBad );

            //
            //  Reinitialize the volume file cache and allocation support.
            //

            {
                CC_FILE_SIZES FileSizes;

                FileSizes.AllocationSize.QuadPart =
                FileSizes.FileSize.QuadPart = ( 0x40000 + 0x1000 );
                FileSizes.ValidDataLength = FatMaxLarge;

                DebugTrace(0, Dbg, "Truncate and reinitialize the volume file\n", 0);

                CcInitializeCacheMap( OldVcb->VirtualVolumeFile,
                                      &FileSizes,
                                      TRUE,
                                      &FatData.CacheManagerNoOpCallbacks,
                                      Vcb );

                //
                //  Redo the allocation support
                //

                FatSetupAllocationSupport( IrpContext, OldVcb );

                //
                //  Get the state of the dirty bit.
                //

                FatCheckDirtyBit( IrpContext, OldVcb );

                //
                //  Check for write protected media.
                //

                if (FatIsMediaWriteProtected(IrpContext, TargetDeviceObject)) {

                    SetFlag( OldVcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );

                } else {

                    ClearFlag( OldVcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );
                }
            }

            //
            //  Complete the request and return to our caller
            //

            try_return( Status = STATUS_SUCCESS );
        }

        DebugTrace(0, Dbg, "Mount a new volume\n", 0);

#endif

        //
        //  This is a new mount
        //
        //  Create a blank ea data file fcb, just not for Fat32.
        //

        if (!FatIsFat32(Vcb)) {

            DIRENT TempDirent;
            PFCB EaFcb;

            RtlZeroMemory( &TempDirent, sizeof(DIRENT) );
            RtlCopyMemory( &TempDirent.FileName[0], "EA DATA  SF", 11 );

            EaFcb = FatCreateFcb( IrpContext,
                                  Vcb,
                                  Vcb->RootDcb,
                                  0,
                                  0,
                                  &TempDirent,
                                  NULL,
                                  FALSE,
                                  TRUE );

            //
            //  Deny anybody who trys to open the file.
            //

            SetFlag( EaFcb->FcbState, FCB_STATE_SYSTEM_FILE );

            Vcb->EaFcb = EaFcb;
        }

        //
        //  Get the state of the dirty bit.
        //

        FatCheckDirtyBit( IrpContext, Vcb );

        //
        //  Check for write protected media.
        //

        if (FatIsMediaWriteProtected(IrpContext, TargetDeviceObject)) {

            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );

        } else {

            ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );
        }

        //
        //  Lock volume in drive if we just mounted the boot drive.
        //

        if (FlagOn(RealDevice->Flags, DO_SYSTEM_BOOT_PARTITION)) {

            SetFlag(Vcb->VcbState, VCB_STATE_FLAG_BOOT_OR_PAGING_FILE);

            if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA)) {

                FatToggleMediaEjectDisable( IrpContext, Vcb, TRUE );
            }
        }

        //
        //  Indicate to our termination handler that we have mounted
        //  a new volume.
        //

        MountNewVolume = TRUE;

        //
        //  Complete the request
        //

        Status = STATUS_SUCCESS;

        //
        //  Ref the root dir stream object so we can send mount notification.
        //

        RootDirectoryFile = Vcb->RootDcb->Specific.Dcb.DirectoryFile;
        ObReferenceObject( RootDirectoryFile );

        //
        //  Remove the extra reference to this target DO made on behalf of us
        //  by the IO system.  In the remount case, we permit regular Vcb
        //  deletion to do this work.
        //

        ObDereferenceObject( TargetDeviceObject );


    try_exit: NOTHING;

    } finally {

        DebugUnwind( FatMountVolume );

        FatUnpinBcb( IrpContext, BootBcb );
        FatUnpinBcb( IrpContext, DirentBcb );

        //
        //  Check if a volume was mounted.  If not then we need to
        //  mark the Vpb not mounted again.
        //

        if ( !MountNewVolume ) {

            if ( Vcb != NULL ) {

                //
                //  A VCB was created and initialized.  We need to try to tear it down.
                //

                FatCheckForDismount( IrpContext,
                                     Vcb,
                                     TRUE );

                IrpContext->Vcb = NULL;

            } else if (VolDo != NULL) {

                //
                //  The VCB was never initialized, so we need to delete the
                //  device right here.
                //

                IoDeleteDevice( &VolDo->DeviceObject );
            }

#if 0

            //
            //  See if a remount failed.
            //

            if (DoARemount && AbnormalTermination()) {

                //
                //  The remount failed. Try to tear down the old VCB as well.
                //

                FatCheckForDismount( IrpContext,
                                     OldVcb,
                                     TRUE );
            }

#endif

        }

        if ( WeClearedVerifyRequiredBit == TRUE ) {

            SetFlag(RealDevice->Flags, DO_VERIFY_VOLUME);
        }

        FatReleaseGlobal( IrpContext );

        DebugTrace(-1, Dbg, "FatMountVolume -> %08lx\n", Status);
    }

    //
    //  Now send mount notification. Note that since this is outside of any
    //  synchronization since the synchronous delivery of this may go to
    //  folks that provoke re-entrance to the FS.
    //

    if (RootDirectoryFile != NULL) {

        FsRtlNotifyVolumeEvent( RootDirectoryFile, FSRTL_VOLUME_MOUNT );
        ObDereferenceObject( RootDirectoryFile );
    }

    return Status;
}

NTSTATUS
NdasFatPnpCancelRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine handles the PnP cancel remove operation.  This is our
    notification that a previously proposed remove (query) was eventually
    vetoed by a component.  The filesystem is responsible for cleaning up
    and getting ready for more IO.
    
Arguments:

    Irp - Supplies the Irp to process
    
    Vcb - Supplies the volume being removed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  CANCEL - a previous QUERY has been rescinded as a result
    //  of someone vetoing.  Since PnP cannot figure out who may
    //  have gotten the QUERY (think about it: stacked drivers),
    //  we must expect to deal with getting a CANCEL without having
    //  seen the QUERY.
    //
    //  For FAT, this is quite easy.  In fact, we can't get a
    //  CANCEL if the underlying drivers succeeded the QUERY since
    //  we disconnect the Vpb on our dismount initiation.  This is
    //  actually pretty important because if PnP could get to us
    //  after the disconnect we'd be thoroughly unsynchronized
    //  with respect to the Vcb getting torn apart - merely referencing
    //  the volume device object is insufficient to keep us intact.
    //
    
    FatAcquireExclusiveVcb( IrpContext, Vcb );

#if 0
    FatReleaseGlobal( IrpContext);
#endif
    
    //
    //  Unlock the volume.  This is benign if we never had seen
    //  a QUERY.
    //

    Status = FatUnlockVolumeInternal( IrpContext, Vcb, NULL );

#if 0

    try {
        
        //
        //  Send the request.  The underlying driver will complete the
        //  IRP.  Since we don't need to be in the way, simply ellide
        //  ourselves out of the IRP stack.
        //

        IoSkipCurrentIrpStackLocation( Irp );

        Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);
    } 
    finally {
        
        FatReleaseVcb( IrpContext, Vcb );
    }

    FatCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

#else

	FatReleaseVcb( IrpContext, Vcb );

#endif

    return Status;
}


NTSTATUS
CleanUpVcb ( 
	IN PIRP_CONTEXT	IrpContext,
	IN PVCB			Vcb
	)
{
	NTSTATUS	Status;
	BOOLEAN		VcbAcquired = FALSE;


	try {

		FatAcquireExclusiveVcb( IrpContext, Vcb );
		VcbAcquired = TRUE;

		Status = NdasFatPnpQueryRemove( IrpContext, NULL, IrpContext->Vcb );
		ASSERT( Status == STATUS_SUCCESS );
        
		if (NT_SUCCESS(Status)) {

			BOOLEAN VcbDeleted;

			Status = NdasFatPnpCancelRemove ( IrpContext, NULL, IrpContext->Vcb );
			ASSERT( Status == STATUS_SUCCESS );

			FatAcquireExclusiveGlobal( IrpContext );
			RemoveEntryList( &Vcb->VcbLinks );
			FatReleaseGlobal( IrpContext );
			
			NdasFatTearDownVcb( IrpContext, Vcb );
			NdasFatDeleteVcb( IrpContext, Vcb );
		}

	} finally {

		if (VcbAcquired) {

			FatReleaseVcb( IrpContext, Vcb );
		}
	}

	return Status;
}

#endif
