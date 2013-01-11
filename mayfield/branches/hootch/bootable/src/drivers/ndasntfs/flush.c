      //
        //  Synchronize with FatCheckForDismount(), which modifies the vpb.
        //

        (VOID)FatAcquireExclusiveGlobal( IrpContext );

        //
        //  Create a new volume device object.  This will have the Vcb
        //  hanging off of its end, and set its alignment requirement
        //  from the device we talk to.
        //

        if (!NT_SUCCESS(Status = IoCreateDevice( FatData.DriverObject,
                                                 sizeof(VOLUME_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
                                                 NULL,
                                                 FILE_DEVICE_DISK_FILE_SYSTEM,
                                                 0,
                                                 FALSE,
                                                 (PDEVICE_OBJECT *)&VolDo))) {

            try_return( Status );
        }

#ifdef __ND_FAT__

		RtlZeroMemory( (PUCHAR)VolDo+sizeof(DEVICE_OBJECT), 
						sizeof(VOLUME_DEVICE_OBJECT)-sizeof(DEVICE_OBJECT) );

		VolDo->ReferenceCount = 1;
		SetFlag( VolDo->NdFatFlags, ND_FAT_DEVICE_FLAG_INITIALIZING );
		KeInitializeEvent( &VolDo->ReferenceZeroEvent, NotificationEvent, FALSE );

		InitializeListHead( &VolDo->PrimarySessionQueue );
		KeInitializeSpinLock( &VolDo->PrimarySessionQSpinLock );

#endif

#if (defined(__ND_FAT_PRIMARY__) || defined(__ND_FAT_SECONDARY__))

		DebugTrace2( 0, Dbg2, ("NdFatMountVolume: VolDo = %p\n", VolDo) );		
		DebugTrace2( 0, Dbg2, ("NdFatMountVolume QueryNetdiskInformation \n") );

		if( ((PVOLUME_DEVICE_OBJECT)FatData.DiskFileSystemDeviceObject)->NdfsCallback.QueryPartitionInformation == NULL ) {

			ASSERT( FALSE );
			VolDo->NetdiskEnableMode = NETDISK_UNKNOWN_MODE;
			try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
		
		} else {

			NTSTATUS				queryStatus;
			NETDISK_INFORMATION		netdiskInformation;
			NETDISK_ENABLE_MODE		netdiskEnableMode;
			PARTITION_INFORMATION	partitionInformation;


			queryStatus = ((PVOLUME_DEVICE_OBJECT)FatData.DiskFileSystemDeviceObject)->
								NdfsCallback.QueryPartitionInformation( Vpb->RealDevice, 
																		&netdiskInformation, 
																		&partitionInformation );

			if (queryStatus != STATUS_SUCCESS) {

				DebugTrace2( 0, Dbg2, ("NdFatMountVolume %08lx QueryNetdiskInformation \n", queryStatus) );
				ASSERT( NDFAT_BUG );

				try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
			
			} else {

				if (netdiskInformation.DesiredAccess & GENERIC_WRITE) {

					if(netdiskInformation.GrantedAccess & GENERIC_WRITE)
						netdiskEnableMode = NETDISK_PRIMARY;
					else
						netdiskEnableMode = NETDISK_SECONDARY;
				
				} else
					netdiskEnableMode = NETDISK_READ_ONLY;

				if (netdiskEnableMode == NETDISK_READ_ONLY) {

					ASSERT( NDFAT_BUG );
					try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
				}

				VolDo->NetdiskEnableMode = netdiskEnableMode;
				VolDo->NetdiskInformation = netdiskInformation;
				VolDo->PartitionInformation = partitionInformation;

				DebugTrace2( 0, Dbg2, ("netdiskEnableMode = %d, NETDISK_SECONDARY = %d\n", netdiskEnableMode, NETDISK_SECONDARY) );
			}
		}
#endif

#ifdef _PNP_POWER_
        //
        // This driver doesn't talk directly to a device, and (at the moment)
        // isn't otherwise concerned about power management.
        //

        VolDo->DeviceObject.DeviceObjectExtension->PowerControlNeeded = FALSE;
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

#ifdef __ND_FAT_WIN2K_SUPPORT__

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

            ASSERT( !FlagOn( OldVcb->VcbState, VCB_STATE_FLAG_VPB_MUST_BE_FREED ) );

            OldVpb->RealDevice = Vpb->RealDevice;
            OldVpb->RealDevice->Vpb = OldVpb;
            
            OldVcb->VcbCondition = VcbGood;

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
            //  We do not want to touch this VPB again. It will get cleaned up when
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
   H>&            ∏c&     X>&            ∞µ     `>&            ∏µ     »>&            ¿c&     ÿ>&            ‘µ     ‡>&            Ëƒ     H?&            »c&     »?&            –c&     ÿ?&            Ïµ     ‡?&            ≈     H@&            ÿc&     X@&             ∂     `@&            	∂     »@&            ‡c&     ÿ@&            &∂     ‡@&            .∂     HA&            Ëc&     »A&            c&     HB&            ¯c&     XB&            ê¥     `B&            J∂     »B&             d&     ÿB&            b∂     ‡B&            @≈     HC&            d&     XC&            m∂     `C&            `≈     »C&            d&     ÿC&            É∂     ‡C&            ê≈     HD&            d&     XD&            ò∂     `D&            ¿≈     »D&             d&     ÿD&            ´∂     ‡D&            Ë≈     ËD&            `¶&     HE&            (d&     XE&            æ∂     `E&            ∆     »E&            0d&     ÿE&            ‘∂     ‡E&            @∆     ËE&            p¶&     XF&            Í∂     `F&            p∆     hF&            Ä¶&     »F&            @d&     ÿF&            ˇ∂     ‡F&            †∆     HG&            Pd&     XG&            ∑     `G&            –∆     hG&            †¶&     »G&            `d&     ÿG&            /∑     ‡G&             «     HH&            pd&     XH&            G∑     `H&            0«     hH&            ∞¶&     »H&            Äd&     ÿH&            _∑     ‡H&            `«     ËH&            ¿¶&     HI&            êd&     XI&            s∑     `I&            ê«     hI&            –¶&     »I&            ®d&     ÿI&            ƒ∂     ‡I&            ¿«     HJ&            ∞d&     XJ&            ⁄∂     `J&            Ë«     hJ&            ¶&     ÿJ&            å∑     ‡J&            »     ËJ&             ß&     HK&            »d&     XK&            ∑     `K&            @»     »K&            ‡d&     ÿK&            ∑     ‡K&            h»     ËK&            @ß&     HL&            e&     XL&            †∑     `L&            ê»     hL&            pß&     pL&            òÕ     »L&            0e&     ÿL&            Æ∑     ‡L&            ∏»     ËL&            êß&     HM&            @e&     XM&            ¿∑     `M&            ‡»     »M&            `e&     ÿM&            5∑     ‡M&            …     ËM&            †ß&     HN&            †e&     XN&            M∑     `N&            8…     hN&            ‡ß&     »N&            –e&     ÿN&            ÷∑     ‡N&            `…     HO&            ÿe&     XO&            Ë∑     `O&            à…     »O&            ‡e&     ÿO&            ˙∑     ‡O&            ∞…     ËO&             ®&     HP&             f&     XP&            ∏     `P&            ÿ…     hP&            `®&     »P&            Hf&     ÿP&            ∏     ‡P&                   HQ&            Pf&     XQ&            *∏     `Q&            (      ÿQ&            6∏     ‡Q&            P      ËQ&            †®&     HR&            Xf&     XR&            Áñ     `R&            x      »R&            `f&     ÿR&            B∏     ‡R&            J∏     ËR&            ‡®&     HS&            pf&     XS&            g∏     `S&            ò      hS&            ®&     »S&            xf&     ÿS&            ∏     ‡S&            »      HT&            Äf&     »T&            êf&     ÿT&            è∏     ‡T&                  ËT&             ©&     HU&            †f&     XU&            õ∏     `U&            À     hU&             ©&     »U&            ‡f&     ÿU&            ´∏     ‡U&            @À     ËU&            `©&     HV&             g&     XV&            B∏     `V&            ª∏     hV&            Ä©&     ÿV&            ÿ∏     ‡V&            hÀ     XW&            Â∏     `W&            êÀ     »W&            g&     ÿW&            Ï∏     ‡W&            ı∏     ËW&            ê©&     HX&            (g&     XX&            π     `X&            »À     ÿX&            !π     ‡X&            'π     XY&            Aπ     `Y&            À     ÿY&            Nπ     ‡Y&            Ã     XZ&            ]π     `Z&            HÃ     »Z&            0g&     ÿZ&            tπ     ‡Z&            ÄÃ     H[&            8g&     »[&            @g&     ÿ[&            p∏     ‡[&            ®Ã     Ë[&            ∞©&     H\&            Pg&     X\&            áπ     `\&            ëπ     »\&            Xg&     ÿ\&            Øπ     ‡\&            –Ã     H]&            `g&     »]&            pg&     ÿ]&            »π     ‡]&             Õ     Ë]&            ¿©&     H^&            Äg&     X^&            ÿπ     `^&            (Õ     »^&            êg&     ÿ^&            √—     ‡^&            ‰π     Ë^&            –©&     H_&            †g&     »_&            ∞g&     ÿ_&            ¸π     ‡_&            HÕ     Ë_&            ‡©&     H`&            ¿g&     X`&            
∫     ``&            ∫     h&            ™Õ      h&            µÕ     òh&            ö„     †h&            8‚     i&            £„      i&            h‚     òi&            ≠„     †i&            à‚     j&            8ó      j&            π„     òj&            ⁄è     †j&            “Õ     k&            Áñ      k&            ∞‚     òk&            €ñ     †k&            –‚     l&            ‘ñ      l&            ◊„     òl&            Àñ     †l&            Ò„     m&            ∏ñ      m&            ‰     òm&            +‰     †m&            ‚     n&            4‰      n&            :‰     òn&            ≤„     †n&            V‰     o&            •ñ      o&            s‰     òo&            è‰     †o&            „     p&            ò‰      p&            ù‰     òp&            0ò     †p&            ∏‰     q&            /ó      q&            ’‰     òq&            √ñ     †q&            Ò‰     r&            yñ      r&            Â     òr&            qñ     †r&            'Â     ®t&            P     ∞t&            ê/     »t&            $     –t&             8     Ët&            †     t&             2     u&            –     u&            ¿.     (u&            ê     0u&            ¿*     Hu&            P     Pu&            Ä1     hu&                  pu&             1     àu&            @     êu&            ‡-     ®u&            †     ∞u&            ‡/     Hv&            P     Pv&            ê*     hv&                 pv&            `.     8w&            Ì     @w&            ¿Ô     ∏w&             Ì     ¿w&            ¯Ô     8x&            ,Ì     @x&            (     ∏x&            9Ì     ¿x&            X     8y&            JÌ     @y&            ê     ∏y&            _Ì     ¿y&            »     8z&            oÌ     @z&             Ò     ∏z&            ÉÌ     ¿z&            8Ò     ({&            hÑ&     8{&            çÌ     @{&            hÒ     H{&             ≥&     ∏{&            õÌ     ¿{&            †Ò     8|&            ÆÌ     @|&            ÿÒ     ∏|&            √Ì     ¿|&            Ú     (}&            pÑ&     8}&            ÿÌ     @}&            XÚ     ®}&            xÑ&     8~&            ÊÌ     @~&            òÚ     H~&            @≥&     ∏~&            ÏÌ     ¿~&            »Ú     8&            ¸Ì     @&            Û     H&            `≥&     ∏&            Ó     ¿&            HÛ     »&            Ä≥&     8Ä&            5Ó     @Ä&            àÛ     HÄ&            ê≥&     ∏Ä&            SÓ     ¿Ä&            »Û     »Ä&            †≥&     8Å&            kÓ     @Å&             Ù     HÅ&            ∞≥&     ∏Å&            ÉÓ     ¿Å&            8Ù     »Å&            ¿≥&     8Ç&            óÓ     @Ç&            pÙ     HÇ&            –≥&     ®Ç&            ÄÑ&     ∏Ç&            ©Ó     ¿Ç&            ®Ù     »Ç&            Ë≥&     (É&            àÑ&     8É&            ¯â     @É&            ÿÙ     ®É&            êÑ&     ∏É&            ∏Ó     ¿É&             ı     (Ñ&            òÑ&     ÿÑ&            9     ‡Ñ&            ¿     XÖ&            N     `Ö&                  »Ö&            àã&     ÿÖ&            [     ‡Ö&            (     HÜ&            êã&     »Ü&            òã&     ÿÜ&            ^     ‡Ü&            P     Há&            †ã&     Xá&            e     `á&            x     »á&            ®ã&     Hà&            ∞ã&     Xà&            w     `à&            ®     hà&            P¥&     »à&            ¿ã&     ÿà&            Ü     ‡à&            ÿ     Hâ&            »ã&     »â&            –ã&     ÿâ&            ó     ‡â&                 Ëâ&            `¥&     Hä&            ‡ã&     Xä&            •     `ä&            8     ÿä&            Aπ     ‡ä&            `     Xã&            Ø     `ã&            ê     »ç&            0Ê     –ç&            ‡‰      é&            ¿Â     é&            –‰     Xé&             §     `é&            PÂ     hé&            ‰     àé&             §     Ëé&             ó&     ¯é&            ¿‰     (è&            Á     Äè&             û     †è&            †Ê      ó&             ó&     8ó&            ¡}     `ó&            h~     pó&            ∑&     xó&            v~     àó&            ∑&     ¿ó&            Åâ     »ó&            Ñâ     –ó&            åâ     ‡ó&            úâ     Ëó&            ¶â     ó&            :ñ     ¯ó&            Øâ      ò&            ïº     ò&            ≥â     ò&            `Ω      ò&            Åâ     (ò&            `Ω     0ò&            ïº     @ò&            Lì     Hò&            \ì     Pò&            lì     Xò&            Äì     `ò&            èì     hò&            úì     Äò&            ïº     àò&            ©ì     êò&            ó     òò&            ≤ì     †ò&            ºì     ®ò&            ¬ì     ∞ò&            Ãì     ¿ò&            ”ì     »ò&            €ì     –ò&            „ì     ÿò&            Ïì     ‡ò&            Ùì     Ëò&            ˛ì     ò&            î     ¯ò&            î      ô&            î     ô&            %î     ô&            -î     ô&            6î      ô&            =î     (ô&            Iî     0ô&            Vî     8ô&            `î     @ô&            jî     Hô&            sî     Pô&            {î     Xô&            yÈ     `ô&            Öî     hô&            éî     pô&            óî     xô&            °î     Äô&            ≠î     àô&            ∫î     êô&    