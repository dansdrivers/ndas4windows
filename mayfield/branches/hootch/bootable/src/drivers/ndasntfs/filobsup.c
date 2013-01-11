return STATUS_INVALID_DEVICE_REQUEST;
		
		} else {

			ASSERT( FALSE );

			DebugTrace2( 0, Dbg2, ("IoControlCode != ND_FAT_SHUTDOWN\n", 0) );

			Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
			Irp->IoStatus.Information = 0;

			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			DebugTrace2( -1, Dbg, ("FatFsdFileSystemControl -> %08lx\n", STATUS_SUCCESS) );

			return STATUS_INVALID_DEVICE_REQUEST;
		}
	}

#endif

    DebugTrace(+1, Dbg,"FatFsdFileSystemControl\n", 0);

    //
    //  Call the common FileSystem Control routine, with blocking allowed if
    //  synchronous.  This opeation needs to special case the mount
    //  and verify suboperations because we know they are allowed to block.
    //  We identify these suboperations by looking at the file object field
    //  and seeing if its null.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->FileObject == NULL) {

        Wait = TRUE;

    } else {

        Wait = CanFsdWait( Irp );
    }

    FsRtlEnterFileSystem();

    TopLevel = FatIsIrpTopLevel( Irp );

#ifdef __ND_FAT__
    
	do {
	
		try {

	        PIO_STACK_LOCATION IrpSp;

		    IrpSp = IoGetCurrentIrpStackLocation( Irp );

			//
	        //  We need to made a special check here for the InvalidateVolumes
		    //  FSCTL as that comes in with a FileSystem device object instead
			//  of a volume device object.
	        //

		    if (FatDeviceIsFatFsdo( IrpSp->DeviceObject) &&
			    (IrpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
				(IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
	            (IrpSp->Parameters.FileSystemControl.FsControlCode ==
		         FSCTL_INVALIDATE_VOLUMES)) {

			    Status = FatInvalidateVolumes( Irp );

	        } else {

				if (IrpContext == NULL) { 

					IrpContext = FatCreateIrpContext( Irp, Wait );
					IrpContext->TopLevel = TopLevel;
				}

#ifdef __ND_FAT_SECONDARY__

				if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) {

					BOOLEAN	secondaryResourceAcquired = FALSE;
					BOOLEAN secondaryRecoveryResourceAcquired = FALSE;

					ASSERT( FatIsTopLevelRequest(IrpContext) );

#ifdef __ND_FAT_DBG__
					ASSERT( FlagOn(IrpContext->NdFatFlags, ND_FAT_IRP_CONTEXT_FLAG_SECONDARY_FILE) );
#endif				

					Status = STATUS_SUCCESS;

					while (TRUE) {
			
						ASSERT( secondaryRecoveryResourceAcquired == FALSE );
						ASSERT( secondaryResourceAcquired == FALSE );

						if (FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) || 
							FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {
		
							if(!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

								Status = FatFsdPostRequest( IrpContext, Irp );
								break;
							}
						}
					
						if (FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {
						
							secondaryRecoveryResourceAcquired 
								= SecondaryAcquireResourceExclusiveLite( IrpContext, 
																		 &VolumeDeviceObject->Secondary->RecoveryResource, 
																		 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
								
							if (!FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->Secondary->RecoveryResource );
								secondaryRecoveryResourceAcquired = FALSE;
								continue;
							}

							secondaryResourceAcquired 
								= SecondaryAcquireResourceExclusiveLite( IrpContext, 
																		 &VolumeDeviceObject->Secondary->Resource, 
																		 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
							try {
								
								SessionRecovery( VolumeDeviceObject->Secondary, IrpContext );
								
							} finally {

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->Secondary->Resource );
								secondaryResourceAcquired = FALSE;

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->Secondary->RecoveryResource );
								secondaryRecoveryResourceAcquired = FALSE;
							}

							continue;
						}

						secondaryResourceAcquired 
							= SecondaryAcquireResourceSharedLite( IrpContext, 
																  &VolumeDeviceObject->Secondary->Resource, 
																  BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

						if (secondaryResourceAcquired == FALSE) {

							ASSERT( FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ||
									FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );

							continue;
						}

						break;
					}

					if (Status == STATUS_SUCCESS) {
					
						try {

							Status = FatCommonFileSystemControl( IrpContext, Irp );
							
						} finally {

							ASSERT( ExIsResourceAcquiredSharedLite(&VolumeDeviceObject->Secondary->Resource) );
							SecondaryReleaseResourceLite( NULL, &VolumeDeviceObject->Secondary->Resource );
						}
					}

				} else
					Status = FatCommonFileSystemControl( IrpContext, Irp );
#else
			    Status = FatCommonFileSystemControl( IrpContext, Irp );
#endif
	        }

			break;

		} except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

			//
	        //  We had some trouble trying to perform the requested
		    //  operation, so we'll abort the I/O request with
			//  the error status that we get back from the
	        //  execption code
		    //

			Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
		}

	} while (Status == STATUS_CANT_WAIT);
	
#else

    try {

        PIO_STACK_LOCATION IrpSp;

        IrpSp = IoGetCurrentIrpStackLocation( Irp );

        //
        //  We need to made a special check here for the InvalidateVolumes
        //  FSCTL as that comes in with a FileSystem device object instead
        //  of a volume device object.
        //

        if (FatDeviceIsFatFsdo( IrpSp->DeviceObject) &&
            (IrpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
            (IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
            (IrpSp->Parameters.FileSystemControl.FsControlCode ==
             FSCTL_INVALIDATE_VOLUMES)) {

            Status = FatInvalidateVolumes( Irp );

        } else {

            IrpContext = FatCreateIrpContext( Irp, Wait );

            Status = FatCommonFileSystemControl( IrpContext, Irp );
        }

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

#endif

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdFileSystemControl -> %08lx\n", Status);

    return Status;
}


NTSTATUS
FatCommonFileSystemControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for doing FileSystem control operations called
    by both the fsd and fsp threads

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg,"FatCommonFileSystemControl\n", 0);
    DebugTrace( 0, Dbg,"Irp           = %08lx\n", Irp);
    DebugTrace( 0, Dbg,"MinorFunction = %08lx\n", IrpSp->MinorFunction);

#ifdef __ND_FAT_SECONDARY__

	if (IoGetCurrentIrpStackLocation(Irp)->FileObject == NULL) {

		DebugTrace2( 0, DEBUG_TRACE_ALL, ("FatCommonFileSystemControl: IrpSp->MinorFunction = %x, IrpSp->Parameters.FileSystemControl.FsControlCode = %d\n", 
			IrpSp->MinorFunction, (IrpSp->Parameters.FileSystemControl.FsControlCode & 0x00003FFC) >> 2) );
	}

	if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) {

		Status = NdFatSecondaryCommonFileSystemControl( IrpContext, Irp );
		return Status;
	}

#endif

    //
    //  We know this is a file system control so we'll case on the
    //  minor function, and call a internal worker routine to complete
    //  the irp.
    //

    switch (IrpSp->MinorFunction) {

    case IRP_MN_USER_FS_REQUEST:

        Status = FatUserFsCtrl( IrpContext, Irp );
        break;

    case IRP_MN_MOUNT_VOLUME:

        Status = FatMountVolume( IrpContext,
                                 IrpSp->Parameters.MountVolume.DeviceObject,
                                 IrpSp->Parameters.MountVolume.Vpb,
                                 IrpSp->DeviceObject );

        //
        //  Complete the request.
        //
        //  We do this here because FatMountVolume can be called recursively,
        //  but the Irp is only to be completed once.
        //
        //  NOTE: I don't think this is true anymore (danlo 3/15/1999).  Probably
        //  an artifact of the old doublespace attempt.
        //

        FatCompleteRequest( IrpContext, Irp, Status );
        break;

    case IRP_MN_VERIFY_VOLUME:

        Status = FatVerifyVolume( IrpContext, Irp );
        break;

    default:

        DebugTrace( 0, Dbg, "Invalid FS Control Minor Function %08lx\n", IrpSp->MinorFunction);

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    DebugTrace(-1, Dbg, "FatCommonFileSystemControl -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatMountVolume (
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
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( IrpContext->OriginatingIrp );
    NTSTATUS Status;

    PBCB BootBcb;
    PPACKED_BOOT_SECTOR BootSector;

    PBCB DirentBcb;
    PDIRENT Dirent;
    ULONG ByteOffset;

    BOOLEAN MountNewVolume = FALSE;
    BOOLEAN WeClearedVerifyRequiredBit = FALSE;
    BOOLEAN DoARemount = FALSE;

    PVCB OldVcb;
    PVPB OldVpb;

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

    DebugTrace(+1, Dbg, "FatMountVolume\n", 0);
    DebugTrace( 0, Dbg, "TargetDeviceObject = %08lx\n", TargetDeviceObject);
    DebugTrace( 0, Dbg, "Vpb                = %08lx\n", Vpb);

    ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
    ASSERT( FatDeviceIsFatFsdo( FsDeviceObject));

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

    //
    //  If this is a CD class device,  then check to see if there is a 
    //  'data track' or not.  This is to avoid issuing paging reads which will
    //  fail later in the mount process (e.g. CD-DA or blank CD media)
    //

    if ((TargetDeviceObject->DeviceType == FILE_DEVICE_CD_ROM) &&