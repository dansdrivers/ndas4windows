 directories\n", 0);
                    try_return( Status = STATUS_INVALID_PARAMETER );
                }

                SetFlag( Fcb->FcbState, FCB_STATE_TEMPORARY );

                SetFlag( IoGetCurrentIrpStackLocation(Irp)->FileObject->Flags,
                         FO_TEMPORARY_FILE );

            } else {

                ClearFlag( Fcb->FcbState, FCB_STATE_TEMPORARY );

                ClearFlag( IoGetCurrentIrpStackLocation(Irp)->FileObject->Flags,
                           FO_TEMPORARY_FILE );
            }

            //
            //  Set the new attributes byte, and mark the bcb dirty
            //

            Fcb->DirentFatFlags = Attributes;

            Dirent->Attributes = Attributes;

            NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
        }

        if ( ModifyCreation ) {

            //
            //  Set the new last write time in the dirent, and mark
            //  the bcb dirty
            //

            Fcb->CreationTime = LargeCreationTime;
            Dirent->CreationTime = CreationTime;
            Dirent->CreationMSec = CreationMSec;


            NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
            //
            //  Now we have to round the time in the Fcb up to the
            //  nearest tem msec.
            //

            Fcb->CreationTime.QuadPart =

                ((Fcb->CreationTime.QuadPart + AlmostTenMSec) /
                 TenMSec) * TenMSec;

            //
            //  Now because the user just set the creation time we
            //  better not set the creation time on close
            //

            SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_CREATION );
        }

        if ( ModifyLastAccess ) {

            //
            //  Set the new last write time in the dirent, and mark
            //  the bcb dirty
            //

            Fcb->LastAccessTime = LargeLastAccessTime;
            Dirent->LastAccessDate = LastAccessDate;

            NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;

            //
            //  Now we have to truncate the time in the Fcb down to the
            //  current day.  This has to be in LocalTime though, so first
            //  convert to local, trunacate, then set back to GMT.
            //

            ExSystemTimeToLocalTime( &Fcb->LastAccessTime,
                                     &Fcb->LastAccessTime );

            Fcb->LastAccessTime.QuadPart =

                (Fcb->LastAccessTime.QuadPart /
                 FatOneDay.QuadPart) * FatOneDay.QuadPart;

            ExLocalTimeToSystemTime( &Fcb->LastAccessTime,
                                     &Fcb->LastAccessTime );

            //
            //  Now because the user just set the last access time we
            //  better not set the last access time on close
            //

            SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_ACCESS );
        }

        if ( ModifyLastWrite ) {

            //
            //  Set the new last write time in the dirent, and mark
            //  the bcb dirty
            //

            Fcb->LastWriteTime = LargeLastWriteTime;
            Dirent->LastWriteTime = LastWriteTime;

            NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;

            //
            //  Now we have to round the time in the Fcb up to the
            //  nearest two seconds.
            //

            Fcb->LastWriteTime.QuadPart =

                ((Fcb->LastWriteTime.QuadPart + AlmostTwoSeconds) /
                 TwoSeconds) * TwoSeconds;

            //
            //  Now because the user just set the last write time we
            //  better not set the last write time on close
            //

            SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_WRITE );
        }

        //
        //  If we modified any of the values, we report this to the notify
        //  package.
        //
        //  We also take this opportunity to set the current file size and
        //  first cluster in the Dirent in order to support a server hack.
        //

        if (NotifyFilter != 0) {

            if (NodeType(Fcb) == FAT_NTC_FCB) {

                Dirent->FileSize = Fcb->Header.FileSize.LowPart;

                Dirent->FirstClusterOfFile = (USHORT)Fcb->FirstClusterOfFile;

                if (FatIsFat32(Fcb->Vcb)) {

                    Dirent->FirstClusterOfFileHi =
                            (USHORT)(Fcb->FirstClusterOfFile >> 16);
                }
            }

            FatNotifyReportChange( IrpContext,
                                   Fcb->Vcb,
                                   Fcb,
                                   NotifyFilter,
                                   FILE_ACTION_MODIFIED );

            FatSetDirtyBcb( IrpContext, DirentBcb, Fcb->Vcb, TRUE );
        }

    try_exit: NOTHING;
    } finally {

        DebugUnwind( FatSetBasicInfo );

        FatUnpinBcb( IrpContext, DirentBcb );

        DebugTrace(-1, Dbg, "FatSetBasicInfo -> %08lx\n", Status);
    }

    return Status;
}

//
//  Internal Support Routine
//

NTSTATUS
FatSetDispositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine performs the set disposition information for fat.  It either
    completes the request or enqueues it off to the fsp.

Arguments:

    Irp - Supplies the irp being processed

    FileObject - Supplies the file object being processed

    Fcb - Supplies the Fcb or Dcb being processed, already known not to
        be the root dcb

Return Value:

    NTSTATUS - The result of this operation if it completes without
               an exception.

--*/

{
    PFILE_DISPOSITION_INFORMATION Buffer;
    PBCB Bcb;
    PDIRENT Dirent;

    DebugTrace(+1, Dbg, "FatSetDispositionInfo...\n", 0);

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Check if the user wants to delete the file or not delete
    //  the file
    //

    if (Buffer->DeleteFile) {

        //
        //  Check if the file is marked read only
        //

        if (FlagOn(Fcb->DirentFatFlags, FAT_DIRENT_ATTR_READ_ONLY)) {

            DebugTrace(-1, Dbg, "Cannot delete readonly file\n", 0);

            return STATUS_CANNOT_DELETE;
        }

        //
        //  Make sure there is no process mapping this file as an image.
        //

        if (!MmFlushImageSection( &Fcb->NonPaged->SectionObjectPointers,
                                  MmFlushForDelete )) {

            DebugTrace(-1, Dbg, "Cannot delete user mapped image\n", 0);

            return STATUS_CANNOT_DELETE;
        }

        //
        //  Check if this is a dcb and if so then only allow
        //  the request if the directory is empty.
        //

        if (NodeType(Fcb) == FAT_NTC_ROOT_DCB) {

            DebugTrace(-1, Dbg, "Cannot delete root Directory\n", 0);

            return STATUS_CANNOT_DELETE;
        }

        if (NodeType(Fcb) == FAT_NTC_DCB) {

            DebugTrace(-1, Dbg, "User wants to delete a directory\n", 0);

            //
            //  Check if the directory is empty
            //

            if ( !FatIsDirectoryEmpty(IrpContext, Fcb) ) {

                DebugTrace(-1, Dbg, "Directory is not empty\n", 0);

                return STATUS_DIRECTORY_NOT_EMPTY;
            }
        }

        //
        //  If this is a floppy, touch the volume so to verify that it
        //  is not write protected.
        //

        if ( FlagOn(Fcb->Vcb->Vpb->RealDevice->Characteristics, FILE_FLOPPY_DISKETTE)) {

            PVCB Vcb;
            PBCB Bcb = NULL;
            UCHAR *Buffer;
            UCHAR TmpChar;
            ULONG BytesToMap;

            IO_STATUS_BLOCK Iosb;

            Vcb = Fcb->Vcb;

            BytesToMap = Vcb->AllocationSupport.FatIndexBitSize == 12 ?
                         FatReservedBytes(&Vcb->Bpb) +
                         FatBytesPerFat(&Vcb->Bpb):PAGE_SIZE;

            FatReadVolumeFile( IrpContext,
                               Vcb,
                               0,
                               BytesToMap,
                               &Bcb,
                               (PVOID *)&Buffer );

            try {

                if (!CcPinMappedData( Vcb->VirtualVolumeFile,
                                      &FatLargeZero,
                                      BytesToMap,
                                      BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                                      &Bcb )) {

                    //
                    // Could not pin the data without waiting (cache miss).
                    //

                    FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
                }

                //
                //  Make Mm, myself, and Cc think the byte is dirty, and then
                //  force a writethrough.
                //

                Buffer += FatReservedBytes(&Vcb->Bpb);

                TmpChar = Buffer[0];
                Buffer[0] = TmpChar;

                FatAddMcbEntry( Vcb, &Vcb->DirtyFatMcb,
                                FatReservedBytes( &Vcb->Bpb ),
                                FatReservedBytes( &Vcb->Bpb ),
                                Vcb->Bpb.BytesPerSector );

            } finally {

                if (AbnormalTermination() && (Bcb != NULL)) {

                    FatUnpinBcb( IrpContext, Bcb );
                }
            }

            CcRepinBcb( Bcb );
            CcSetDirtyPinnedData( Bcb, NULL );
            CcUnpinData( Bcb );
            DbgDoit( ASSERT( IrpContext->PinCount ));
            DbgDoit( IrpContext->PinCount -= 1 );
            CcUnpinRepinnedBcb( Bcb, TRUE, &Iosb );

            //
            //  If this was not successful, raise the status.
            //

            if ( !NT_SUCCESS(Iosb.Status) ) {

                FatNormalizeAndRaiseStatus( IrpContext, Iosb.Status );
            }

        } else {

            //
            //  Just set a Bcb dirty here.  The above code was only there to
            //  detect a write protected floppy, while the below code works
            //  for any write protected media and only takes a hit when the
            //  volume in clean.
            //

            FatGetDirentFromFcbOrDcb( IrpContext,
                                      Fcb,
                                      &Dirent,
                                      &Bcb );

            //
            //  This has to work for the usual reasons (we verified the Fcb within
            //  volume synch).
            //
            
            ASSERT( Bcb != NULL );

            try {

                FatSetDirtyBcb( IrpContext, Bcb, Fcb->Vcb, TRUE );

            } finally {

                FatUnpinBcb( IrpContext, Bcb );
            }
        }

        //
        //  At this point either we have a file or an empty directory
        //  so we know the delete can proceed.
        //

        SetFlag( Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE );
        FileObject->DeletePending = TRUE;

        //
        //  If this is a directory then report this delete pending to
        //  the dir notify package.
        //

        if (NodeType(Fcb) == FAT_NTC_DCB) {

            FsRtlNotifyFullChangeDirectory( Fcb->Vcb->NotifySync,
                                            &Fcb->Vcb->DirNotifyList,
                                            FileObject->FsContext,
                                            NULL,
                                            FALSE,
                                            FALSE,
                                            0,
                                            NULL,
                                            NULL,
                                            NULL );
        }
    } else {

        //
        //  The user doesn't want to delete the file so clear
        //  the delete on close bit
        //

        DebugTrace(0, Dbg, "User want to not delete file\n", 0);

        ClearFlag( Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE );
        FileObject->DeletePending = FALSE;
    }

    DebugTrace(-1, Dbg, "FatSetDispositionInfo -> STATUS_SUCCESS\n", 0);

    return STATUS_SUCCESS;
}


//
//  Internal Support Routine
//

NTSTATUS
FatSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set name information for fat.  It either
    completes the request or enqueues it off to the fsp.

Arguments:

    Irp - Supplies the irp being processed

    Vcb - Supplies the Vcb being processed

    Fcb - Supplies the Fcb or Dcb being processed, already known not to
        be the root dcb

    Ccb - Supplies the Ccb corresponding to the handle opening the source
        file

Return Value:

    NTSTATUS - The result of this operation if it completes without
               an exception.

--*/

{
    BOOLEAN AllLowerComponent;
    BOOLEAN AllLowerExtension;
    BOOLEAN CaseOnlyRename;
    BOOLEAN ContinueWithRename;
    BOOLEAN CreateLfn;
    BOOLEAN DeleteSourceDirent;
    BOOLEAN DeleteTarget;
    BOOLEAN NewDirentFromPool;
    BOOLEAN RenamedAcrossDirectories;
    BOOLEAN ReplaceIfExists;

    CCB LocalCcb;
    PCCB SourceCcb;

    DIRENT SourceDirent;

    NTSTATUS Status;

    OEM_STRING OldOemName;
    OEM_STRING NewOemName;
    UCHAR OemNameBuffer[24*2];

    PBCB DotDotBcb;
    PBCB NewDirentBcb;
    PBCB OldDirentBcb;
    PBCB SecondPageBcb;
    PBCB TargetDirentBcb;

    PDCB TargetDcb;
    PDCB OldParentDcb;

    PDIRENT DotDotDirent;
    PDIRENT FirstPageDirent;
    PDIRENT NewDirent;
    PDIRENT OldDirent;
    PDIRENT SecondPageDirent;
    PDIRENT ShortDirent;
    PDIRENT TargetDirent;

    PFCB TempFcb;

    PFILE_OBJECT TargetFileObject;
    PFILE_OBJECT FileObject;

    PIO_STACK_LOCATION IrpSp;

    PLIST_ENTRY Links;

    ULONG BytesInFirstPage;
    ULONG DirentsInFirstPage;
    ULONG DirentsRequired;
    ULONG NewOffset;
    ULONG NotifyAction;
    ULONG SecondPageOffset;
    ULONG ShortDirentOffset;
    ULONG TargetDirentOffset;
    ULONG TargetLfnOffset;

    UNICODE_STRING NewName;
    UNICODE_STRING NewUpcasedName;
    UNICODE_STRING OldName;
    UNICODE_STRING OldUpcasedName;
    UNICODE_STRING TargetLfn;

    PWCHAR UnicodeBuffer;

    UNICODE_STRING UniTunneledShortName;
    WCHAR UniTunneledShortNameBuffer[12];
    UNICODE_STRING UniTunneledLongName;
    WCHAR UniTunneledLongNameBuffer[26];
    LARGE_INTEGER TunneledCreationTime;
    ULONG TunneledDataSize;
    BOOLEAN HaveTunneledInformation;
    BOOLEAN UsingTunneledLfn = FALSE;

    BOOLEAN InvalidateFcbOnRaise = FALSE;

    DebugTrace(+1, Dbg, "FatSetRenameInfo...\n", 0);

    //
    //  P H A S E  0: Initialize some variables.
    //

    CaseOnlyRename = FALSE;
    ContinueWithRename = FALSE;
    DeleteSourceDirent = FALSE;
    DeleteTarget = FALSE;
    NewDirentFromPool = FALSE;
    RenamedAcrossDirectories = FALSE;

    DotDotBcb = NULL;
    NewDirentBcb = NULL;
    OldDirentBcb = NULL;
    SecondPageBcb = NULL;
    TargetDirentBcb = NULL;

    NewOemName.Length = 0;
    NewOemName.MaximumLength = 24;
    NewOemName.Buffer = &OemNameBuffer[0];

    OldOemName.Length = 0;
    OldOemName.MaximumLength = 24;
    OldOemName.Buffer = &OemNameBuffer[24];

    UnicodeBuffer = FsRtlAllocatePoolWithTag( PagedPool,
                                              4 * MAX_LFN_CHARACTERS * sizeof(WCHAR),
                                              TAG_FILENAME_BUFFER );

    NewUpcasedName.Length = 0;
    NewUpcasedName.MaximumLength = MAX_LFN_CHARACTERS * sizeof(WCHAR);
    NewUpcasedName.Buffer = &UnicodeBuffer[0];

    OldName.Length = 0;
    OldName.MaximumLength = MAX_LFN_CHARACTERS * sizeof(WCHAR);
    OldName.Buffer = &UnicodeBuffer[MAX_LFN_CHARACTERS];

    OldUpcasedName.Length = 0;
    OldUpcasedName.MaximumLength = MAX_LFN_CHARACTERS * sizeof(WCHAR);
    OldUpcasedName.Buffer = &UnicodeBuffer[MAX_LFN_CHARACTERS * 2];

    TargetLfn.Length = 0;
    TargetLfn.MaximumLength = MAX_LFN_CHARACTERS * sizeof(WCHAR);
    TargetLfn.Buffer = &UnicodeBuffer[MAX_LFN_CHARACTERS * 3];

    UniTunneledShortName.Length = 0;
    UniTunneledShortName.MaximumLength = sizeof(UniTunneledShortNameBuffer);
    UniTunneledShortName.Buffer = &UniTunneledShortNameBuffer[0];

    UniTunneledLongName.Length = 0;
    UniTunneledLongName.MaximumLength = sizeof(UniTunneledLongNameBuffer);
    UniTunneledLongName.Buffer = &UniTunneledLongNameBuffer[0];

    //
    //  Remember the name in case we have to modify the name
    //  value in the ea.
    //

    RtlCopyMemory( OldOemName.Buffer,
                   Fcb->ShortName.Name.Oem.Buffer,
                   OldOemName.Length );

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Extract information from the Irp to make our life easier
    //

    FileObject = IrpSp->FileObject;
    SourceCcb = FileObject->FsContext2;
    TargetFileObject = IrpSp->Parameters.SetFile.FileObject;
    ReplaceIfExists = IrpSp->Parameters.SetFile.ReplaceIfExists;

    RtlZeroMemory( &LocalCcb, sizeof(CCB) );

    //
    //  P H A S E  1:
    //
    //  Test if rename is legal.  Only small side-effects are not undone.
    //

    try {

        //
        //  Can't rename the root directory
        //

        if ( NodeType(Fcb) == FAT_NTC_ROOT_DCB ) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Check that we were not given a dcb with open handles beneath
        //  it.  If there are only UncleanCount == 0 Fcbs beneath us, then
        //  remove them from the prefix table, and they will just close
        //  and go away naturally.
        //

        if (NodeType(Fcb) == FAT_NTC_DCB) {

            PFCB BatchOplockFcb;
            ULONG BatchOplockCount;

            //
            //  Loop until there are no batch oplocks in the subtree below
            //  this directory.
            //

            while (TRUE) {

                BatchOplockFcb = NULL;
                BatchOplockCount = 0;

                //
                //  First look for any UncleanCount != 0 Fcbs, and fail if we
                //  find any.
                //

                for ( TempFcb = FatGetNextFcbBottomUp(IrpContext, NULL, Fcb);
                      TempFcb != Fcb;
                      TempFcb = FatGetNextFcbBottomUp(IrpContext, TempFcb, Fcb) ) {

                     if ( TempFcb->UncleanCount != 0 ) {

                         //
                         // If there is a batch oplock on this file then
                         // increment our count and remember the Fcb if
                         // this is the first.
                         //

                         if ( (NodeType(TempFcb) == FAT_NTC_FCB) &&
                              FsRtlCurrentBatchOplock( &TempFcb->Specific.Fcb.Oplock ) ) {

                             BatchOplockCount += 1;
                             if ( BatchOplockFcb == NULL ) {

                                 BatchOplockFcb = TempFcb;
                             }

                         } else {

                            try_return( Status = STATUS_ACCESS_DENIED );
                         }
                     }
                }

                //
                //  If this is not the first pass for rename and the number
                //  of batch oplocks has not decreased then give up.
                //

                if ( BatchOplockFcb != NULL ) {

                    if ( (Irp->IoStatus.Information != 0) &&
                         (BatchOplockCount >= Irp->IoStatus.Information) ) {

                        try_return( Status = STATUS_ACCESS_DENIED );
                    }

                    //
                    //  Try to break this batch oplock.
                    //

                    Irp->IoStatus.Information = BatchOplockCount;
                    Status = FsRtlCheckOplock( &BatchOplockFcb->Specific.Fcb.Oplock,
                                               Irp,
                                               IrpContext,
                                               FatOplockComplete,
                                               NULL );

                    //
                    //  If the oplock was already broken then look for more
                    //  batch oplocks.
                    //

                    if (Status == STATUS_SUCCESS) {

                        continue;
                    }

                    //
                    //  Otherwise the oplock package will post or complete the
                    //  request.
                    //

                    try_return( Status = STATUS_PENDING );
                }

                break;
            }

            //
            //  Now try to get as many of these file object, and thus Fcbs
            //  to go away as possible, flushing first, of course.
            //

            FatPurgeReferencedFileObjects( IrpContext, Fcb, TRUE );

            //
            //  OK, so there are no UncleanCount != 0, Fcbs.  Infact, there
            //  shouldn't really be any Fcbs left at all, except obstinate
            //  ones from user mapped sections ....oh well, he shouldn't have
            //  closed his handle if he wanted the file to stick around.  So
            //  remove any Fcbs beneath us from the splay table and mark them
            //  DELETE_ON_CLOSE so that any future operations will fail.
            //

            for ( TempFcb = FatGetNextFcbBottomUp(IrpContext, NULL, Fcb);
                  TempFcb != Fcb;
                  TempFcb = FatGetNextFcbBottomUp(IrpContext, TempFcb, Fcb) ) {

                FatRemoveNames( IrpContext, TempFcb );

                SetFlag( TempFcb->FcbState, FCB_STATE_DELETE_ON_CLOSE );
            }
        }

        //
        //  Check if this is a simple rename or a fully-qualified rename
        //  In both cases we need to figure out what the TargetDcb, and
        //  NewName are.
        //

        if (TargetFileObject == NULL) {

            //
            //  In the case of a simple rename the target dcb is the
            //  same as the source file's parent dcb, and the new file name
            //  is taken from the system buffer
            //

            PFILE_RENAME_INFORMATION Buffer;

            Buffer = Irp->AssociatedIrp.SystemBuffer;

            TargetDcb = Fcb->ParentDcb;

            NewName.Length = (USHORT) Buffer->FileNameLength;
            NewName.Buffer = (PWSTR) &Buffer->FileName;

            //
            //  Make sure the name is of legal length.
            //

            if (NewName.Length >= 255*sizeof(WCHAR)) {

                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

        } else {

            //
            //  For a fully-qualified rename the target dcb is taken from
            //  the target file object, which must be on the same vcb as
            //  the source.
            //

            PVCB TargetVcb;
            PCCB TargetCcb;

            if ((FatDecodeFileObject( TargetFileObject,
                                      &TargetVcb,
                                      &TargetDcb,
                                      &TargetCcb ) != UserDirectoryOpen) ||
                (TargetVcb != Vcb)) {

                try_return( Status = STATUS_INVALID_PARAMETER );
            }

            //
            //  This name is by definition legal.
            //

            NewName = *((PUNICODE_STRING)&TargetFileObject->FileName);
        }

        //
        //  We will need an upcased version of the unicode name and the
        //  old name as well.
        //

        Status = RtlUpcaseUnicodeString( &NewUpcasedName, &NewName, FALSE );

        if (!NT_SUCCESS(Status)) {

            try_return( Status );
        }

        FatGetUnicodeNameFromFcb( IrpContext, Fcb, &OldName );

        Status = RtlUpcaseUnicodeString( &OldUpcasedName, &OldName, FALSE );

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Check if the current name and new name are equal, and the
        //  DCBs are equal.  If they are then our work is already done.
        //

        if (TargetDcb == Fcb->ParentDcb) {

            //
            //  OK, now if we found something then check if it was an exact
            //  match or just a case match.  If it was an exact match, then
            //  we can bail here.
            //

            if (FsRtlAreNamesEqual( &NewName,
                                    &OldName,
                                    FALSE,
                                    NULL )) {

                 try_return( Status = STATUS_SUCCESS );
            }

            //
            //  Check now for a case only rename.
            //


            if (FsRtlAreNamesEqual( &NewUpcasedName,
                                    &OldUpcasedName,
                                    FALSE,
                                    NULL )) {

                 CaseOnlyRename = TRUE;
            }

        } else {

            RenamedAcrossDirectories = TRUE;
        }

        //
        //  Upcase the name and convert it to the Oem code page.
        //
        //  If the new UNICODE name is already more than 12 characters,
        //  then we know the Oem name will not be valid
        //

        if (NewName.Length <= 12*sizeof(WCHAR)) {

            FatUnicodeToUpcaseOem( IrpContext, &NewOemName, &NewName );

            //
            //  If the name is not valid 8.3, zero the length.
            //

            if (FatSpaceInName( IrpContext, &NewName ) ||
                !FatIsNameShortOemValid( IrpContext, NewOemName, FALSE, FALSE, FALSE)) {

                NewOemName.Length = 0;
            }

        } else {

            NewOemName.Length = 0;
        }

        //
        //  Look in the tunnel cache for names and timestamps to restore
        //

        TunneledDataSize = sizeof(LARGE_INTEGER);
        HaveTunneledInformation = FsRtlFindInTunnelCache( &Vcb->Tunnel,
                                                          FatDirectoryKey(TargetDcb),
                                                          &NewName,
                                                          &UniTunneledShortName,
                                                          &UniTunneledLongName,
                                                          &TunneledDataSize,
                                                          &TunneledCreationTime );
        ASSERT(TunneledDataSize == sizeof(LARGE_INTEGER));

        //
        //  Now we need to determine how many dirents this new name will
        //  require.
        //

        if ((NewOemName.Length == 0) ||
            (FatEvaluateNameCase( IrpContext,
                                  &NewName,
                                  &AllLowerComponent,
                                  &AllLowerExtension,
                                  &CreateLfn ),
             CreateLfn)) {

            DirentsRequired = FAT_LFN_DIRENTS_NEEDED(&NewName) + 1;

        } else {

            //
            //  The user-given name is a short name, but we might still have
            //  a tunneled long name we want to use. See if we can.
            //

            if (UniTunneledLongName.Length && 
                !FatLfnDirentExists(IrpContext, TargetDcb, &UniTunneledLongName, &TargetLfn)) {

                UsingTunneledLfn = CreateLfn = TRUE;
                DirentsRequired = FAT_LFN_DIRENTS_NEEDED(&UniTunneledLongName) + 1;

            } else {

                //
                //  This really is a simple dirent.  Note that the two AllLower BOOLEANs
                //  are correctly set now.
                //

                DirentsRequired = 1;
            }
        }

        //
        //  Do some extra checks here if we are not in Chicago mode.
        //

        if (!FatData.ChicagoMode) {

            //
            //  If the name was not 8.3 valid, fail the rename.
            //

            if (NewOemName.Length == 0) {

                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

            //
            //  Don't use the magic bits.
            //

            AllLowerComponent = FALSE;
            AllLowerExtension = FALSE;
            CreateLfn = FALSE;
            UsingTunneledLfn = FALSE;
        }

        if (!CaseOnlyRename) {

            //
            //  Check if the new name already exists, wait is known to be
            //  true.
            //

            if (NewOemName.Length != 0) {

                FatStringTo8dot3( IrpContext,
                                  NewOemName,
                                  &LocalCcb.OemQueryTemplate.Constant );

            } else {

                SetFlag( LocalCcb.Flags, CCB_FLAG_SKIP_SHORT_NAME_COMPARE );
            }

            LocalCcb.UnicodeQueryTemplate = NewUpcasedName;
            LocalCcb.ContainsWildCards = FALSE;

            FatLocateDirent( IrpContext,
                             TargetDcb,
                             &LocalCcb,
                             0,
                             &TargetDirent,
                             &TargetDirentBcb,
                             &TargetDirentOffset,
                             NULL,
                             &TargetLfn);

            if (TargetDirent != NULL) {

                //
                //  The name already exists, check if the user wants
                //  to overwrite the name, and has access to do the overwrite
                //  We cannot overwrite a directory.
                //

                if ((!ReplaceIfExists) ||
                    (FlagOn(TargetDirent->Attributes, FAT_DIRENT_ATTR_DIRECTORY)) ||
                    (FlagOn(TargetDirent->Attributes, FAT_DIRENT_ATTR_READ_ONLY))) {

                    try_return( Status = STATUS_OBJECT_NAME_COLLISION );
                }

                //
                //  Check that the file has no open user handles, if it does
                //  then we will deny access.  We do the check by searching
                //  down the list of fcbs opened under our parent Dcb, and making
                //  sure none of the maching Fcbs have a non-zero unclean count or
                //  outstanding image sections.
                //

                for (Links = TargetDcb->Specific.Dcb.ParentDcbQueue.Flink;
                     Links != &TargetDcb->Specific.Dcb.ParentDcbQueue; ) {

                    TempFcb = CONTAINING_RECORD( Links, FCB, ParentDcbLinks );

                    //
                    //  Advance now.  The image section flush may cause the final
                    //  close, which will recursively happen underneath of us here.
                    //  It would be unfortunate if we looked through free memory.
                    //

                    Links = Links->Flink;

                    if ((TempFcb->DirentOffsetWithinDirectory == TargetDirentOffset) &&
                        ((TempFcb->UncleanCount != 0) ||
                         !MmFlushImageSection( &TempFcb->NonPaged->SectionObjectPointers,
                                               MmFlushForDelete))) {

                        //
                        //  If there are batch oplocks on this file then break the
                        //  oplocks before failing the rename.
                        //

                        Status = STATUS_ACCESS_DENIED;

                        if ((NodeType(TempFcb) == FAT_NTC_FCB) &&
                            FsRtlCurrentBatchOplock( &TempFcb->Specific.Fcb.Oplock )) {

                            //
                            //  Do all of our cleanup now since the IrpContext
                            //  could go away when this request is posted.
                            //

                            FatUnpinBcb( IrpContext, TargetDirentBcb );

                            Status = FsRtlCheckOplock( &TempFcb->Specific.Fcb.Oplock,
                                                       Irp,
                                                       IrpContext,
                                                       FatOplockComplete,
                                                       NULL );

                            if (Status != STATUS_PENDING) {

                                Status = STATUS_ACCESS_DENIED;
                            }
                        }

                        try_return( NOTHING );
                    }
                }

                //
                //  OK, this target is toast.  Remember the Lfn offset.
                //

                TargetLfnOffset = TargetDirentOffset -
                                  FAT_LFN_DIRENTS_NEEDED(&TargetLfn) *
                                  sizeof(DIRENT);

                DeleteTarget = TRUE;
            }
        }

        //
        //  If we will need more dirents than we have, allocate them now.
        //

        if ((TargetDcb != Fcb->ParentDcb) ||
            (DirentsRequired !=
             (Fcb->DirentOffsetWithinDirectory -
              Fcb->LfnOffsetWithinDirectory) / sizeof(DIRENT) + 1)) {

            //
            //  Get some new allocation
            //

            NewOffset = FatCreateNewDirent( IrpContext,
                                            TargetDcb,
                                            DirentsRequired );

            DeleteSourceDirent = TRUE;

        } else {

            NewOffset = Fcb->LfnOffsetWithinDirectory;
        }

        ContinueWithRename = TRUE;

    try_exit: NOTHING;

    } finally {

        if (!ContinueWithRename) {

            //
            //  Undo everything from above.
            //

            ExFreePool( UnicodeBuffer );
            FatUnpinBcb( IrpContext, TargetDirentBcb );
        }
    }

    //
    //  Now, if we are already done, return here.
    //

    if (!ContinueWithRename) {

        return Status;
    }

    //
    //  P H A S E  2: Actually perform the rename.
    //

    try {

        //
        //  Report the fact that we are going to remove this entry.
        //  If we renamed within the same directory and the new name for the
        //  file did not previously exist, we report this as a rename old
        //  name.  Otherwise this is a removed file.
        //

        if (!RenamedAcrossDirectories && !DeleteTarget) {

            NotifyAction = FILE_ACTION_RENAMED_OLD_NAME;

        } else {

            NotifyAction = FILE_ACTION_REMOVED;
        }

        FatNotifyReportChange( IrpContext,
                               Vcb,
                               Fcb,
                               ((NodeType( Fcb ) == FAT_NTC_FCB)
                                ? FILE_NOTIFY_CHANGE_FILE_NAME
                                : FILE_NOTIFY_CHANGE_DIR_NAME ),
                               NotifyAction );

        //
        //  Capture a copy of the source dirent.
        //

        FatGetDirentFromFcbOrDcb( IrpContext, Fcb, &OldDirent, &OldDirentBcb );
        SourceDirent = *OldDirent;

        try {

            //
            //  Tunnel the source Fcb - the names are disappearing regardless of
            //  whether the dirent allocation physically changed
            //

            FatTunnelFcbOrDcb( Fcb, SourceCcb );

            //
            //  From here until very nearly the end of the operation, if we raise there
            //  is no reasonable way to suppose we'd be able to undo the damage.  Not
            //  being a transactional filesystem, FAT is at the mercy of a lot of things
            //  (as the astute reader has no doubt realized by now).
            //

            InvalidateFcbOnRaise = TRUE;

            //
            //  Delete our current dirent(s) if we got a new one.
            //

            if (DeleteSourceDirent) {

                FatDeleteDirent( IrpContext, Fcb, NULL, FALSE );
            }

            //
            //  Delete a target conflict if we were meant to.
            //

            if (DeleteTarget) {

                FatDeleteFile( IrpContext,
                               TargetDcb,
                               TargetLfnOffset,
                               TargetDirentOffset,
                               TargetDirent,
                               &TargetLfn );
            }

            //
            //  We need to evaluate any short names required.  If there were any
            //  conflicts in existing short names, they would have been deleted above.
            //
            //  It isn't neccesary to worry about the UsingTunneledLfn case. Since we
            //  actually already know whether CreateLfn will be set either NewName is
            //  an Lfn and !UsingTunneledLfn is implied or NewName is a short name and
            //  we can handle that externally.
            //

            FatSelectNames( IrpContext,
                            TargetDcb,
                            &NewOemName,
                            &NewName,
                            &NewOemName,
                            (HaveTunneledInformation ? &UniTunneledShortName : NULL),
                            &AllLowerComponent,
                            &AllLowerExtension,
                            &CreateLfn );

            if (!CreateLfn && UsingTunneledLfn) {

                CreateLfn = TRUE;
                NewName = UniTunneledLongName;

                //
                //  Short names are always upcase if an LFN exists
                //

                AllLowerComponent = FALSE;
                AllLowerExtension = FALSE;
            }

            //
            //  OK, now setup the new dirent(s) for the new name.
            //

            FatPrepareWriteDirectoryFile( IrpContext,
                                          TargetDcb,
                                          NewOffset,
                                          sizeof(DIRENT),
                                          &NewDirentBcb,
                                          &NewDirent,
                                          FALSE,
                                          TRUE,
                                          &Status );

            ASSERT( NT_SUCCESS( Status ) );

            //
            //  Deal with the special case of an LFN + Dirent structure crossing
            //  a page boundry.
            //

            if ((NewOffset / PAGE_SIZE) !=
                ((NewOffset + (DirentsRequired - 1) * sizeof(DIRENT)) / PAGE_SIZE)) {

                SecondPageOffset = (NewOffset & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

                BytesInFirstPage = SecondPageOffset - NewOffset;

                DirentsInFirstPage = BytesInFirstPage / sizeof(DIRENT);

                FatPrepareWriteDirectoryFile( IrpContext,
                                              TargetDcb,
                                              SecondPageOffset,
                                              sizeof(DIRENT),
                                              &SecondPageBcb,
                                              &SecondPageDirent,
                                              FALSE,
                                              TRUE,
                                              &Status );

                ASSERT( NT_SUCCESS( Status ) );

                FirstPageDirent = NewDirent;

                NewDirent = FsRtlAllocatePoolWithTag( PagedPool,
                                                      DirentsRequired * sizeof(DIRENT),
                                                      TAG_DIRENT );

                NewDirentFromPool = TRUE;
            }

            //
            //  Bump up Dirent and DirentOffset
            //

            ShortDirent = NewDirent + DirentsRequired - 1;
            ShortDirentOffset = NewOffset + (DirentsRequired - 1) * sizeof(DIRENT);

            //
            //  Fill in the fields of the dirent.
            //

            *ShortDirent = SourceDirent;

            FatConstructDirent( IrpContext,
                                ShortDirent,
                                &NewOemName,
                                AllLowerComponent,
                                AllLowerExtension,
                                CreateLfn ? &NewName : NULL,
                                SourceDirent.Attributes,
                                FALSE,
                                (HaveTunneledInformation ? &TunneledCreationTime : NULL) );

            if (HaveTunneledInformation) {

                //
                //  Need to go in and fix the timestamps in the FCB. Note that we can't use
                //  the TunneledCreationTime since the conversions may have failed.
                //

                Fcb->CreationTime = FatFatTimeToNtTime(IrpContext, ShortDirent->CreationTime, ShortDirent->CreationMSec);
                Fcb->LastWriteTime = FatFatTimeToNtTime(IrpContext, ShortDirent->LastWriteTime, 0);
                Fcb->LastAccessTime = FatFatDateToNtTime(IrpContext, ShortDirent->LastAccessDate);
            }

            //
            //  If the dirent crossed pages, split the contents of the
            //  temporary pool between the two pages.
            //

            if (NewDirentFromPool) {

                RtlCopyMemory( FirstPageDirent, NewDirent, BytesInFirstPage );

                RtlCopyMemory( SecondPageDirent,
                               NewDirent + DirentsInFirstPage,
                               DirentsRequired*sizeof(DIRENT) - BytesInFirstPage );

                ShortDirent = SecondPageDirent +
                              (DirentsRequired - DirentsInFirstPage) - 1;
            }

        } finally {

            //
            //  Remove the entry from the splay table, and then remove the
            //  full file name and exact case lfn. It is important that we
            //  always remove the name from the prefix table regardless of
            //  other errors.
            //

            FatRemoveNames( IrpContext, Fcb );

            if (Fcb->FullFileName.Buffer != NULL) {

                ExFreePool( Fcb->FullFileName.Buffer );
                Fcb->FullFileName.Buffer = NULL;
            }

            if (Fcb->ExactCaseLongName.Buffer) {

                ExFreePool( Fcb->ExactCaseLongName.Buffer );
                Fcb->ExactCaseLongName.Buffer = NULL;
            }
        }

        //
        //  Now we need to update the location of the file's directory
        //  offset and move the fcb from its current parent dcb to
        //  the target dcb.
        //

        Fcb->LfnOffsetWithinDirectory = NewOffset;
        Fcb->DirentOffsetWithinDirectory = ShortDirentOffset;

        RemoveEntryList( &Fcb->ParentDcbLinks );

        //
        //  There is a deep reason we put files on the tail, others on the head,
        //  which is to allow us to easily enumerate all child directories before
        //  child files. This is important to let us maintain whole-volume lockorder
        //  via BottomUp enumeration.
        //

        if (NodeType(Fcb) == FAT_NTC_FCB) {

            InsertTailList( &TargetDcb->Specific.Dcb.ParentDcbQueue,
                            &Fcb->ParentDcbLinks );

        } else {

            InsertHeadList( &TargetDcb->Specific.Dcb.ParentDcbQueue,
                            &Fcb->ParentDcbLinks );
        }

        OldParentDcb = Fcb->ParentDcb;
        Fcb->ParentDcb = TargetDcb;

        //
        //  If we renamed across directories, some cleanup is now in order.
        //

        if (RenamedAcrossDirectories) {

            //
            //  See if we need to uninitialize the cachemap for the source directory.
            //  Do this now in case we get unlucky and raise trying to finalize the
            //  operation.
            //

            if (IsListEmpty(&OldParentDcb->Specific.Dcb.ParentDcbQueue) &&
                (OldParentDcb->OpenCount == 0) &&
                (OldParentDcb->Specific.Dcb.DirectoryFile != NULL)) {

                PFILE_OBJECT DirectoryFileObject;

                ASSERT( NodeType(OldParentDcb) == FAT_NTC_DCB );

                DirectoryFileObject = OldParentDcb->Specific.Dcb.DirectoryFile;

                DebugTrace(0, Dbg, "Uninitialize our parent Stream Cache Map\n", 0);

                CcUninitializeCacheMap( DirectoryFileObject, NULL, NULL );

                OldParentDcb->Specific.Dcb.DirectoryFile = NULL;

                ObDereferenceObject( DirectoryFileObject );
            }

            //
            //  If we move a directory across directories, we have to change
            //  the cluster number in its .. entry
            //

            if (NodeType(Fcb) == FAT_NTC_DCB) {

                FatPrepareWriteDirectoryFile( IrpContext,
                                              Fcb,
                                              sizeof(DIRENT),
                                              sizeof(DIRENT),
                                              &DotDotBcb,
                                              &DotDotDirent,
                                              FALSE,
                                              TRUE,
                                              &Status );

                ASSERT( NT_SUCCESS( Status ) );

                DotDotDirent->FirstClusterOfFile = (USHORT)
                    ( NodeType(TargetDcb) == FAT_NTC_ROOT_DCB ?
                      0 : TargetDcb->FirstClusterOfFile);

                if (FatIsFat32( Vcb )) {

                    DotDotDirent->FirstClusterOfFileHi = (USHORT)
                    ( NodeType( TargetDcb ) == FAT_NTC_ROOT_DCB ?
                      0 : (TargetDcb->FirstClusterOfFile >> 16));
                }
            }
        }

        //
        //  Now we need to setup the splay table and the name within
        //  the fcb.  Free the old short name at this point.
        //

        ExFreePool( Fcb->ShortName.Name.Oem.Buffer );
        Fcb->ShortName.Name.Oem.Buffer = NULL;

        FatConstructNamesInFcb( IrpContext,
                                Fcb,
                                ShortDirent,
                                CreateLfn ? &NewName : NULL );

        FatSetFullNameInFcb( IrpContext, Fcb, &NewName );

        //
        //  The rest of the actions taken are not related to correctness of
        //  the in-memory structures, so we shouldn't toast the Fcb if we
        //  raise from here to the end.
        //

        InvalidateFcbOnRaise = FALSE;

        //
        //  If a file, set the file as modified so that the archive bit
        //  is set.  We prevent this from adjusting the write time by
        //  indicating the user flag in the ccb.
        //

        if (Fcb->Header.NodeTypeCode == FAT_NTC_FCB) {

            SetFlag( FileObject->Flags, FO_FILE_MODIFIED );
            SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_WRITE );
        }

        //
        //  We have three cases to report.
        //
        //      1.  If we overwrote an existing file, we report this as
        //          a modified file.
        //
        //      2.  If we renamed to a new directory, then we added a file.
        //
        //      3.  If we renamed in the same directory, then we report the
        //          the renamednewname.
        //

        if (DeleteTarget) {

            FatNotifyReportChange( IrpContext,
                                   Vcb,
                                   Fcb,
                                   FILE_NOTIFY_CHANGE_ATTRIBUTES
                                   | FILE_NOTIFY_CHANGE_SIZE
                                   | FILE_NOTIFY_CHANGE_LAST_WRITE
                                   | FILE_NOTIFY_CHANGE_LAST_ACCESS
                                   | FILE_NOTIFY_CHANGE_CREATION
                                   | FILE_NOTIFY_CHANGE_EA,
                                   FILE_ACTION_MODIFIED );

        } else if (RenamedAcrossDirectories) {

            FatNotifyReportChange( IrpContext,
                                   Vcb,
                                   Fcb,
                                   ((NodeType( Fcb ) == FAT_NTC_FCB)
                                    ? FILE_NOTIFY_CHANGE_FILE_NAME
                                    : FILE_NOTIFY_CHANGE_DIR_NAME ),
                                   FILE_ACTION_ADDED );

        } else {

            FatNotifyReportChange( IrpContext,
                                   Vcb,
                                   Fcb,
                                   ((NodeType( Fcb ) == FAT_NTC_FCB)
                                    ? FILE_NOTIFY_CHANGE_FILE_NAME
                                    : FILE_NOTIFY_CHANGE_DIR_NAME ),
                                   FILE_ACTION_RENAMED_NEW_NAME );
        }

        //
        //  We need to update the file name in the dirent.  This value
        //  is never used elsewhere, so we don't concern ourselves
        //  with any error we may encounter.  We let chkdsk fix the
        //  disk at some later time.
        //

        if (!FatIsFat32(Vcb) &&
            ShortDirent->ExtendedAttributes != 0) {

            FatRenameEAs( IrpContext,
                          Fcb,
                          ShortDirent->ExtendedAttributes,
                          &OldOemName );
        }

        //
        //  Set our final status
        //

        Status = STATUS_SUCCESS;

    } finally {

        DebugUnwind( FatSetRenameInfo );

        ExFreePool( UnicodeBuffer );

        if (UniTunneledLongName.Buffer != UniTunneledLongNameBuffer) {

            //
            //  Free pool if the buffer was grown on tunneling lookup
            //

            ExFreePool(UniTunneledLongName.Buffer);
        }

        FatUnpinBcb( IrpContext, OldDirentBcb );
        FatUnpinBcb( IrpContext, TargetDirentBcb );
        FatUnpinBcb( IrpContext, NewDirentBcb );
        FatUnpinBcb( IrpContext, SecondPageBcb );
        FatUnpinBcb( IrpContext, DotDotBcb );


        //
        //  If this was an abnormal termination, then we are in trouble.
        //  Should the operation have been in a sensitive state there is
        //  nothing we can do but invalidate the Fcb.
        //

        if (AbnormalTermination() && InvalidateFcbOnRaise) {

            Fcb->FcbCondition = FcbBad;
        }

        DebugTrace(-1, Dbg, "FatSetRenameInfo -> %08lx\n", Status);
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
FatSetPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine performs the set position information for fat.  It either
    completes the request or enqueues it off to the fsp.

Arguments:

    Irp - Supplies the irp being processed

    FileObject - Supplies the file object being processed

Return Value:

    NTSTATUS - The result of this operation if it completes without
               an exception.

--*/

{
    PFILE_POSITION_INFORMATION Buffer;

    DebugTrace(+1, Dbg, "FatSetPositionInfo...\n", 0);

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Check if the file does not use intermediate buffering.  If it
    //  does not use intermediate buffering then the new position we're
    //  supplied must be aligned properly for the device
    //

    if (FlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING )) {

        PDEVICE_OBJECT DeviceObject;

        DeviceObject = IoGetCurrentIrpStackLocation( Irp )->DeviceObject;

        if ((Buffer->CurrentByteOffset.LowPart & DeviceObject->AlignmentRequirement) != 0) {

            DebugTrace(0, Dbg, "Cannot set position due to aligment conflict\n", 0);
            DebugTrace(-1, Dbg, "FatSetPositionInfo -> %08lx\n", STATUS_INVALID_PARAMETER);

            return STATUS_INVALID_PARAMETER;
        }
    }

    //
    //  The input parameter is fine so set the current byte offset and
    //  complete the request
    //

    DebugTrace(0, Dbg, "Set the new position to %08lx\n", Buffer->CurrentByteOffset);

    FileObject->CurrentByteOffset = Buffer->CurrentByteOffset;

    DebugTrace(-1, Dbg, "FatSetPositionInfo -> %08lx\n", STATUS_SUCCESS);

    UNREFERENCED_PARAMETER( IrpContext );

    return STATUS_SUCCESS;
}


//
//  Internal Support Routine
//

NTSTATUS
FatSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine performs the set Allocation information for fat.  It either
    completes the request or enqueues it off to the fsp.

Arguments:

    Irp - Supplies the irp being processed

    Fcb - Supplies the Fcb or Dcb being processed, already known not to
        be the root dcb

    FileObject - Supplies the FileObject being processed, already known not to
        be the root dcb

Return Value:

    NTSTATUS - The result of this operation if it completes without
               an exception.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PFILE_ALLOCATION_INFORMATION Buffer;
    ULONG NewAllocationSize;

    BOOLEAN FileSizeTruncated = FALSE;
    BOOLEAN CacheMapInitialized = FALSE;
    BOOLEAN ResourceAcquired = FALSE;
    ULONG OriginalFileSize;
    ULONG OriginalValidDataLength;
    ULONG OriginalValidDataToDisk;

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    NewAllocationSize = Buffer->AllocationSize.LowPart;

    DebugTrace(+1, Dbg, "FatSetAllocationInfo.. to %08lx\n", NewAllocationSize);

    //
    //  Allocation is only allowed on a file and not a directory
    //

    if (NodeType(Fcb) == FAT_NTC_DCB) {

        DebugTrace(-1, Dbg, "Cannot change allocation of a directory\n", 0);

        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    //  Check that the new file allocation is legal
    //

    if (!FatIsIoRangeValid( Fcb->Vcb, Buffer->AllocationSize, 0 )) {

        DebugTrace(-1, Dbg, "Illegal allocation size\n", 0);

        return STATUS_DISK_FULL;
    }

    //
    //  If we haven't yet looked up the correct AllocationSize, do so.
    //

    if (Fcb->Header.AllocationSize.QuadPart == FCB_LOOKUP_ALLOCATIONSIZE_HINT) {

        FatLookupFileAllocationSize( IrpContext, Fcb );
    }

    //
    //  This is kinda gross, but if the file is not cached, but there is
    //  a data section, we have to cache the file to avoid a bunch of
    //  extra work.
    //

    if ((FileObject->SectionObjectPointer->DataSectionObject != NULL) &&
        (FileObject->SectionObjectPointer->SharedCacheMap == NULL) &&
        !FlagOn(Irp->Flags, IRP_PAGING_IO)) {

        ASSERT( !FlagOn( FileObject->Flags, FO_CLEANUP_COMPLETE ) );

        //
        //  Now initialize the cache map.
        //

        CcInitializeCacheMap( FileObject,
                              (PCC_FILE_SIZES)&Fcb->Header.AllocationSize,
                              FALSE,
                              &FatData.CacheManagerCallbacks,
                              Fcb );

        CacheMapInitialized = TRUE;
    }

    //
    //  Now mark the fact that the file needs to be truncated on close
    //

    Fcb->FcbState |= FCB_STATE_TRUNCATE_ON_CLOSE;

    //
    //  Now mark that the time on the dirent needs to be updated on close.
    //

    SetFlag( FileObject->Flags, FO_FILE_MODIFIED );

    try {

        //
        //  Increase or decrease the allocation size.
        //

        if (NewAl               N_ERROR) ||
				ADAPTERINFO_ISSTATUS(pdoData->LanscsiAdapterPDO.AdapterStatus, ADAPTERINFO_STATUS_STOPPING) /*||
				ADAPTERINFO_ISSTATUSFLAG(pdoData->LanscsiAdapterPDO.AdapterStatus, ADAPTERINFO_STATUSFLAG_MEMBER_FAULT) */
				) {

				nodeAliveOut.bHasError = TRUE;
				Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
					("IOCTL_BUSENUM_QUERY_NODE_ALIVE Adapter has Error 0x%x\n", nodeAliveOut.bHasError));
			} else {
				nodeAliveOut.bHasError = FALSE;
			}

		}

		if(pdoData)
			ObDereferenceObject(pdoData->Self);

		RtlCopyMemory(
			Irp->AssociatedIrp.SystemBuffer,
			&nodeAliveOut,
			sizeof(BUSENUM_NODE_ALIVE_OUT)
			);
		
		Irp->IoStatus.Information = sizeof(BUSENUM_NODE_ALIVE_OUT);
		status = STATUS_SUCCESS;
		}
		break;

	//
	//	added by hootch 01172004
	//
	case IOCTL_LANSCSI_UPGRADETOWRITE:
		{
		PPDO_DEVICE_DATA				pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_UPGRADETOWRITE called\n"));
		// Check Parameter.
		if(inlen != sizeof(BUSENUM_UPGRADE_TO_WRITE)) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_UPGRADETOWRITE: Invalid input buffer length\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		pdoData = LookupPdoData(fdoData, ((PBUSENUM_UPGRADE_TO_WRITE)buffer)->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_UPGRADETOWRITE: No pdo for Slotno:%d\n", ((PBUSENUM_UPGRADE_TO_WRITE)buffer)->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_LANSCSI_ADD_TARGET,
				((PBUSENUM_UPGRADE_TO_WRITE)buffer)->SlotNo);
		} else {
			//
			//	redirect to the LanscsiMiniport Device
			//
			status = LSBus_IoctlToLSMPDevice(
					pdoData,
					LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE,
					buffer,
					inlen,
					buffer,
					outlen
				);

			ObDereferenceObject(pdoData->Self);
		}
		Irp->IoStatus.Information = 0;
	}
		break;

	case IOCTL_LANSCSI_REDIRECT_NDASSCSI:
		{
		PPDO_DEVICE_DATA				pdoData;
		PBUSENUM_REDIRECT_NDASSCSI		redirectIoctl;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_REDIRECT_NDASSCSI called\n"));
		// Check Parameter.
		if(inlen < sizeof(BUSENUM_REDIRECT_NDASSCSI)) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_REDIRECT_NDASSCSI: Invalid input buffer length\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		redirectIoctl = (PBUSENUM_REDIRECT_NDASSCSI)buffer;

		pdoData = LookupPdoData(fdoData, redirectIoctl->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_REDIRECT_NDASSCSI: No pdo for Slotno:%d\n", redirectIoctl->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
		} else {
			//
			//	redirect to the LanscsiMiniport Device
			//
			status = LSBus_IoctlToLSMPDevice(
					pdoData,
					redirectIoctl->IoctlCode,
					redirectIoctl->IoctlData,
					redirectIoctl->IoctlDataSize,
					redirectIoctl->IoctlData,
					redirectIoctl->IoctlDataSize
				);

			ObDereferenceObject(pdoData->Self);
		}
		Irp->IoStatus.Information = 0;
	}
		break;

	case IOCTL_LANSCSI_QUERY_LSMPINFORMATION:
		{
		PPDO_DEVICE_DATA				pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_QUERY_LSMPINFORMATION called\n"));
		// Check Parameter.
		if(inlen < FIELD_OFFSET(LSMPIOCTL_QUERYINFO, QueryData) ) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_QUERY_LSMPINFORMATION: Invalid input buffer length too small.\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}
		pdoData = LookupPdoData(fdoData, ((PLSMPIOCTL_QUERYINFO)buffer)->SlotNo);

		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_QUERY_LSMPINFORMATION No pdo\n"));
			status = STATUS_NO_SUCH_DEVICE;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_LANSCSI_ADD_TARGET,
				((PLSMPIOCTL_QUERYINFO)buffer)->SlotNo);
		} else {
			//
			// p'2 ˆÿÿ        šÀ  êÿÿ        Device
			//
			status = LSBus_IoctlToLSMPDevice(
					pdoData,
					LANSCSIMINIPORT_IOCTL_QUERYINFO_EX,
					buffer,
					inlen,
					buffer,
					outlen
				);

			ObDereferenceObject(pdoData->Self);
		}
        Irp->IoStatus.Information = outlen;
		}
		break;

	case IOCTL_BUSENUM_QUERY_INFORMATION:
		{

//		PPDO_DEVICE_DATA				pdoData;
		BUSENUM_QUERY_INFORMATION		Query;
		PBUSENUM_INFORMATION			Information;
		LONG							BufferLenNeeded;

		// Check Parameter.
		if(	inlen < sizeof(BUSENUM_QUERY_INFORMATION) /*|| 
			outlen < sizeof(BUSENUM_INFORMATION) */) {
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		RtlCopyMemory(&Query, buffer, sizeof(BUSENUM_QUERY_INFORMATION));
		Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_TRACE,
			("FDO: IOCTL_BUSENUM_QUERY_INFORMATION QueryType : %d  SlotNumber = %d\n",
			Query.InfoClass, Query.SlotNo));

		Information = (PBUSENUM_INFORMATION)buffer;
		ASSERT(Information);
		Information->InfoClass = Query.InfoClass;
		status = LSBus_QueryInformation(fdoData, IoIs32bitProcess(Irp), &Query, Information, outlen, &BufferLenNeeded);
		if(NT_SUCCESS(status)) {
			Information->Size = BufferLenNeeded;
			Irp->IoStatus.Information = BufferLenNeeded;
		} else {
			Irp->IoStatus.Information = BufferLenNeeded;
		}
		}
		break;

	case IOCTL_BUSENUM_PLUGIN_HARDWARE_EX2:
		{
			ULONG	structLen;		// Without variable length field
			ULONG	wholeStructLen; // With variable length field
			ULONG	inputWholeStructLen;

			//
			// Check 32 bit thunking request
            //
			if(IoIs32bitProcess(Irp)) {
				structLen = FIELD_OFFSET(BUSENUM_PLUGIN_HARDWARE_EX2_32, HardwareIDs);
				wholeStructLen = sizeof(BUSENUM_PLUGIN_HARDWARE_EX2_32);
				inputWholeStructLen = ((PBUSENUM_PLUGIN_HARDWARE_EX2_32) buffer)->Size;
			} else {
				structLen = FIELD_OFFSET(BUSENUM_PLUGIN_HARDWARE_EX2, HardwareIDs);
				wholeStructLen = sizeof(BUSENUM_PLUGIN_HARDWARE_EX2);
				inputWholeStructLen = ((PBUSENUM_PLUGIN_HARDWARE_EX2) buffer)->Size;
			}

			if ((inlen == outlen) &&
				//
				// Make sure it has at least two nulls and the size 
				// field is set to the declared size of the struct
				//
				((structLen + sizeof(UNICODE_NULL) * 2) <=
				inlen) &&

				//
				// The size field should be set to the sizeof the struct as declared
				// and *not* the size of the struct plus the multi_sz
				//
				(wholeStructLen == inputWholeStructLen)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("PlugIn called\n"));

				status= Bus_PlugInDeviceEx2((PBUSENUM_PLUGIN_HARDWARE_EX2)buffer,
											inlen,
											fdoData,
											IoIs32bitProcess(Irp),
											Irp->RequestorMode, FALSE);

				Irp->IoStatus.Information = outlen;

			}
		}
        break;

	case IOCTL_LANSCSI_GETVERSION:
		{
			if (outlen >= sizeof(BUSENUM_GET_VERSION)) {
				PBUSENUM_GET_VERSION version = (PBUSENUM_GET_VERSION)buffer;

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_GETVERSION: called\n"));

			try {
				version->VersionMajor = VER_FILEMAJORVERSION;
				version->VersionMinor = VER_FILEMINORVERSION;
				version->VersionBuild = VER_FILEBUILD;
				version->VersionPrivate = VER_FILEBUILD_QFE;

					Irp->IoStatus.Information = sizeof(BUSENUM_GET_VERSION);
					status = STATUS_SUCCESS;

				} except (EXCEPTION_EXECUTE_HANDLER) {

					status = GetExceptionCode();
					Irp->IoStatus.Information = 0;
				}

			}
		}			
		break;

    case IOCTL_BUSENUM_UNPLUG_HARDWARE:
		{
			if ((sizeof (BUSENUM_UNPLUG_HARDWARE) == inlen) &&
				(inlen == outlen) &&
				(((PBUSENUM_UNPLUG_HARDWARE)buffer)->Size == inlen)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UnPlug called\n"));

				status= Bus_UnPlugDevice(
						(PBUSENUM_UNPLUG_HARDWARE)buffer, fdoData);
				Irp->IoStatus.Information = outlen;

			}
		}
        break;

    case IOCTL_BUSENUM_EJECT_HARDWARE:
		{
			if ((sizeof (BUSENUM_EJECT_HARDWARE) == inlen) &&
				(inlen == outlen) &&
				(((PBUSENUM_EJECT P'2 ˆÿÿ       )->Size == inlen)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("Eject called\n"));

				status= Bus_EjectDevice((PBUSENUM_EJECT_HARDWARE)buffer, fdoData);

				Irp->IoStatus.Information = outlen;
			}
		}
		break;

	case IOCTL_DVD_GET_STATUS:
		{
			PPDO_DEVICE_DATA		pdoData;
			PBUSENUM_DVD_STATUS		pDvdStatusData;


			// Check Parameter.
			if((inlen != outlen)
				|| (sizeof(BUSENUM_DVD_STATUS) >  inlen))
			{
				status = STATUS_UNSUCCESSFUL ;
				break;
			}
			
			pDvdStatusData = (PBUSENUM_DVD_STATUS)Irp->AssociatedIrp.SystemBuffer;  
			
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("FDO: IOCTL_DVD_GET_STATUS SlotNumber = %d\n",
				pDvdStatusData->SlotNo));	

			pdoData = LookupPdoData(fdoData, pDvdStatusData->SlotNo);
			
			if(pdoData == NULL) {
				Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
					("IOCTL_DVD_GET_STATUS No pdo\n"));
				status = STATUS_UNSUCCESSFUL;
				NDBusIoctlLogError(	fdoData->Self,
					NDASBUS_IO_PDO_NOT_FOUND,
					IOCTL_LANSCSI_ADD_TARGET,
					pDvdStatusData->SlotNo);
				break;	
			} else {

				if(pdoData->LanscsiAdapterPDO.Flags & LSDEVDATA_FLAG_LURDESC) {
					//
					//	A LUR descriptor is set.
					//
					if(((PLURELATION_DESC)pdoData->LanscsiAdapterPDO.AddDevInfo)->DevType != NDASSCSI_TYPE_DVD)
				{
					Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
						("IOCTL_DVD_GET_STATUS  No DVD Device\n"));
					status = STATUS_UNSUCCESSFUL;
					break;
				}
				} else {
					//
					//	ADD_TARGET_DATA is set.
					//
					if(((PLANSCSI_ADD_TARGET_DATA)pdoData->LanscsiAdapterPDO.AddDevInfo)->ucTargetType != NDASSCSI_TYPE_DVD)
					{
						Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
							("IOCTL_DVD_GET_STATUS  No DVD Device\n"));
						status = STATUS_UNSUCCESSFUL;
						break;
					}
				}
				//
				//	redirect to the LanscsiMiniport Device
				//
				status = LSBus_IoctlToLSMPDevice(
						pdoData,
						LANSCSIMINIPORT_IOCTL_GET_DVD_STATUS,
						buffer,
						inlen,
						buffer,
						outlen
					);

				ObDereferenceObject(pdoData->Self);
				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = outlen;
			}					
		}
		break;
    default:
        break; // default status is STATUS_INVALID_PARAMETER
    }

	Irp->IoStatus.Status = status;
	if(Irp->UserIosb)
		*Irp->UserIosb = Irp->IoStatus;

	IoCompleteRequest (Irp, IO_NO_INCREMENT);
    Bus_DecIoCount (fdoData);
    return status;
}


VOID
Bus_DriverUnload (
    IN PDRIVER_OBJECT DriverObject
    )
/*++
Routine Description:
    Clean up everything we did in driver entry.

Arguments:

   DriverObject - pointer to this driverObject.


Return Value:

--*/
{
    PAGED_CODE ();

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Unload\n"));
    
    //
    // All the device objects should be gone.
    //

    ASSERT (NULL == DriverObject->DeviceObject);
	UNREFERENCED_PARAMETER(DriverObject);

    //
    // Here we free all the resources allocated in the DriverEntry
    //

    if(Globals.RegistryPath.Buffer)
        ExFreePool(Globals.RegistryPath.Buffer);   
        
    return;
}

VOID
Bus_IncIoCount (
    IN  PFDO_DEVICE_DATA   FdoData
    )   

/*++

Routine Description:

    This routine increments the number of requests the device receives
    

Arguments:

    FdoData - pointer to the FDO device extension.
    
Return Value:

    VOID

--*/

{

    LONG            result;


    result = InterlockedIncrement(&FdoData->OutstandingIO);

    ASSERT(result > 0);
    //
    // Need to clear StopEvent (when OutstandingIO bumps from 1 to 2) 
    //
    if (result == 2) {
        //
        // We need to clear the event
        //
        KeClearEvent(&FdoData->StopEvent);
    }

    return;
}

VOID
Bus_DecIoCount(
    IN  PFDO_DEVICE_DATA  FdoData
    )

/*++

Routine Description:

    This routine decrements as it complete the request it receives

Arguments:

    FdoData - pointer to the FDO device extens               rn Value:

    VOID

--*/
{

    LONG            result;
    
    result = InterlockedDecrement(&FdoData->OutstandingIO);

    ASSERT(result >= 0);

    if (result == 1) {
        //
        // Set the stop event. Note that when this happens
        // (i.e. a transition from 2 to 1), the type of requests we 
        // want to be processed are already held instead of being 
        // passed away, so that we can't "miss" a request that
        // will appear between the decrement and the moment when
        // the value is actually used.
        //
 
        KeSetEvent (&FdoData->StopEvent, IO_NO_INCREMENT, FALSE);
        
    }
    
    if (result == 0) {

        //
        // The count is 1-biased, so it can be zero only if an 
        // extra decrement is done when a remove Irp is received 
        //

        ASSERT(FdoData->DevicePnPState == Deleted);

        //
        // Set the remove event, so the device object can be deleted
        //

        KeSetEvent (&FdoData->RemoveEvent, IO_NO_INCREMENT, FALSE);
        
    }

    return;
}


                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       €'2 ˆÿÿ       t (c) 1990-2000 Microsoft Corporation All Rights Reserved

Module Name:

    BUSENUM.H

Abstract:

    This module contains the common private declarations 
    for the Toaster Bus enumerator.

Author:

    Eliyas Yakub Sep 10, 1998

Environment:

    kernel mode only

Notes:


Revision History:


--*/
#include "wmilib.h"

#ifndef BUSENUM_H
#define BUSENUM_H

#define BUSENUM_COMPATIBLE_IDS L"LanscsiBus\\LanscsiMiniport\0"

#define BUSENUM_COMPATIBLE_IDS_LENGTH sizeof(BUSENUM_COMPATIBLE_IDS)

#define BUSENUM_POOL_TAG (ULONG) 'sBSL'

//
//	Driver's registry
//

#define BUSENUM_DRVREG_DISABLE_PERSISTENTPDO	L"DisablePersistentPDO"

//
//	Bus device PDO's registry
//

#define BUSENUM_BUSPDOREG_DISABLE_PERSISTENTPDO	L"DisablePersistentPDO"


//
// Debugging Output Levels
//

#define BUS_DBG_ALWAYS                  0x00000000

#define BUS_DBG_STARTUP_SHUTDOWN_MASK   0x0000000F
#define BUS_DBG_SS_NOISE                0x00000001
#define BUS_DBG_SS_TRACE                0x00000002
#define BUS_DBG_SS_INFO                 0x00000004
#define BUS_DBG_SS_ERROR                0x00000008

#define BUS_DBG_PNP_MASK                0x000000F0
#define BUS_DBG_PNP_NOISE               0x00000010
#define BUS_DBG_PNP_TRACE               0x00000020
#define BUS_DBG_PNP_INFO                0x00000040
#define BUS_DBG_PNP_ERROR               0x00000080

#define BUS_DBG_IOCTL_MASK              0x00000F00
#define BUS_DBG_IOCTL_NOISE             0x00000100
#define BUS_DBG_IOCTL_TRACE             0x00000200
#define BUS_DBG_IOCTL_INFO              0x00000400
#define BUS_DBG_IOCTL_ERROR             0x00000800

#define BUS_DBG_POWER_MASK              0x0000F000
#define BUS_DBG_POWER_NOISE             0x00001000
#define BUS_DBG_POWER_TRACE             0x00002000
#define BUS_DBG_POWER_INFO              0x00004000
#define BUS_DBG_POWER_ERROR             0x00008000

#define BUS_DBG_WMI_MASK                0x000F0000
#define BUS_DBG_WMI_NOISE               0x00010000
#define BUS_DBG_WMI_TRACE               0x00020000
#define BUS_DBG_WMI_INFO                0x00040000
#define BUS_DBG_WMI_ERROR               0x00080000


#if DBG

#define BUS_DEFAULT_DEBUG_OUTPUT_LEVEL	0x0008FCFC

#define __MODULE__ __FILE__

#define Bus_KdPrint(_d_,_l_, _x_) do { if (!(_l_) || (_d_)->DebugLevel & (_l_)) { _KDebugPrintWithLocation(_KDebugPrint _x_ , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define Bus_KdPrint_Cont(_d_,_l_, _x_) do { if (!(_l_) || (_d_)->DebugLevel & (_l_)) { _KDebugPrintWithLocation(_KDebugPrint _x_ , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define Bus_KdPrint_Def(_l_, _x_) do { if (!(_l_) || BusEnumDebugLevel & (_l_)) { _KDebugPrintWithLocation(_KDebugPrint _x_ , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define DbgRaiseIrql(_x_,_y_) KeRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_) KeLowerIrql(_x_)

PCHAR
_KDebugPrint(
   IN PCCHAR	DebugMessage,
   ...
   ) ;

VOID
_KDebugPrintWithLocation(
   IN PCCHAR	DebugMessage,
   PCCHAR		ModuleName,
   UINT32		LineNumber,
   PCCHAR		FunctionName
   ) ;
#else

#define BUS_DEFAULT_DEBUG_OUTPUT_LEVEL 0x0
#define Bus_KdPrint(_d_, _l_, _x_)
#define Bus_KdPrint_Cont(_d_, _l_, _x_)
#define Bus_KdPrint_Def(_l_, _x_)
#define DbgRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_)

#endif

extern ULONG BusEnumDebugLevel;
typedef struct _FDO_DEVICE_DATA FDO_DEVICE_DATA, *PFDO_DEVICE_DATA;

//
// These are the states a PDO or FDO transition upon
// receiving a specific PnP Irp. Refer to the PnP Device States
// diagram in DDK documentation for better understanding.
//

typedef enum _DEVICE_PNP_STATE {

    NotStarted = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // D '2 ˆÿÿ       ed the SURPRISE_REMOVE IRP
    Deleted,                // Device has received the REMOVE_DEVICE IRP
    UnKnown                 // Unknown state

} DEVICE_PNP_STATE;


typedef struct _GLOBALS {
   
    // 
    // Path to the driver's Services Key in the registry
    //

    UNICODE_STRING RegistryPath;


	//
	//	OS versions
	//

	BOOLEAN		bCheckVersion;
	ULONG		MajorVersion;
    ULONG		MinorVersion;
    ULONG		BuildNumber;

	//
	//	Options
	//

	//	Automatically register plug-ined PDOs in the registry
	BOOLEAN		PersistentPdo;

	//	Set TRUE if LFS filter is installed.
	BOOLEAN		LfsFilterInstalled;

	//
	//	FDO for the purpose to receive the TDI PnP event.
	//
	PFDO_DEVICE_DATA	FdoDataTdiPnP;

	//
	//
	//
	FAST_MUTEX			Mutex;

} GLOBALS;


extern GLOBALS Globals;

//
// Structure for reporting data to WMI
//

typedef struct _TOASTER_BUS_WMI_STD_DATA {

    //
    // The error Count
    //
    UINT32   ErrorCount;

    //
    // Debug Print Level
    //

    UINT32  DebugPrintLevel;

} TOASTER_BUS_WMI_STD_DATA, * PTOASTER_BUS_WMI_STD_DATA;


//
// A common header for the device extensions of the PDOs and FDO
//

typedef struct _COMMON_DEVICE_DATA
{
    // A back pointer to the device object for which this is the extension

    PDEVICE_OBJECT  Self;

    // This flag helps distinguish between PDO and FDO

    BOOLEAN         IsFDO;

    // We track the state of the device with every PnP Irp
    // that affects the device through these two variables.
    
    DEVICE_PNP_STATE DevicePnPState;

    DEVICE_PNP_STATE PreviousPnPState;

    
    ULONG           DebugLevel;

    // Stores the current system power state
    
    SYSTEM_POWER_STATE  SystemPowerState;

    // Stores current device power state
    
    DEVICE_POWER_STATE  DevicePowerState;

    
} COMMON_DEVICE_DATA, *PCOMMON_DEVICE_DATA;

//
// The device extension for the PDOs.
// That's of the toaster device which this bus driver enumerates.
//

typedef struct _PDO_DEVICE_DATA
{
    COMMON_DEVICE_DATA;

    // A back pointer to the bus

    PDEVICE_OBJECT  ParentFdo;

    // An array of (zero terminated wide character strings).
    // The array itself also null terminated

	ULONG		HardwareIDLen;
    PWCHAR      HardwareIDs;

    // Unique serail number of the device on the bus

    ULONG		SlotNo;

    // Link point to hold all the PDOs for a single bus together

    LIST_ENTRY  Link;
    
    //
    // Present is set to TRUE when the PDO is exposed via PlugIn IOCTL,
    // and set to FALSE when a UnPlug IOCTL is received. 
    // We will delete the PDO in IRP_MN_REMOVE only after we have reported 
    // to the Plug and Play manager that it's missing.
    //
    
    BOOLEAN     Present;
    BOOLEAN     ReportedMissing;
    UCHAR       Reserved[2]; // for 4 byte alignment

    //
    // Used to track the intefaces handed out to other drivers.
    // If this value is non-zero, we fail query-remove.
    //
    ULONG       ToasterInterfaceRefCount;
    
    //
    // In order to reduce the complexity of the driver, I chose not 
    // to use any queues for holding IRPs when the system tries to 
    // rebalance resources to accommodate a new device, and stops our 
    // device temporarily. But in a real world driver this is required. 
    // If you hold Irps then you should also support Cancel and 
    // Cleanup functions. The function driver demonstrates these techniques.
    //    
    // The queue where the incoming requests are held when
    // the device is stopped for resource rebalance.

    //LIST_ENTRY          PendingQueue;     

    // The spin lock that protects access to  the queue

    //KSPIN_LOCK          PendingQueueLock;     
    
	PDO_LANSCSI_DEVICE_DATA	LanscsiAdapterPDO;
	BOOLEAN					Persistent;

	// Added by jgahn.
	// The name returned from IoRegisterDeviceInterface,
	BOOLEAN				bRegisterInterface;
    UNICODE_STRING      InterfaceName;


} PDO_DEVICE_DATA,  '2 ˆÿÿ       TA;


//
// The device extension of the bus itself.  From whence the PDO's are born.
//

typedef struct _FDO_DEVICE_DATA
{
    COMMON_DEVICE_DATA;

    PDEVICE_OBJECT  UnderlyingPDO;
    
    // The underlying bus PDO and the actual device object to which our
    // FDO is attached

    PDEVICE_OBJECT  NextLowerDriver;

    // List of PDOs created so far
    
    LIST_ENTRY      ListOfPDOs;
    
    // The PDOs currently enumerated.

    ULONG           NumPDOs;

    // A synchronization for access to the device extension.

    FAST_MUTEX      Mutex;

	// A synchronization for access to the registrar and PersistentPdo.

	FAST_MUTEX		RegMutex;

    //
    // The number of IRPs sent from the bus to the underlying device object
    //
    
	LONG           OutstandingIO; // Biased to 1

    //
    // On remove device plug & play request we must wait until all outstanding
    // requests have been completed before we can actually delete the device
    // object. This event is when the Outstanding IO count goes to zero
    //

    KEVENT          RemoveEvent;

    //
    // This event is set when the Outstanding IO count goes to 1.
    //
    
    KEVENT          StopEvent;
    
    // The name returned from IoRegisterDeviceInterface,
    // which is used as a handle for IoSetDeviceInterfaceState.

    UNICODE_STRING      InterfaceName;

	//
	//	Automatically register plug-ined PDOs in the registry
	//

	BOOLEAN		PersistentPdo;

	BOOLEAN		StartStopRegistrarEnum;

	//
	//	Set if the LFS filter is installed.
	//

	BOOLEAN		LfsFilterInstalled;

	//
	//	TDI client handle
	//
	HANDLE		TdiClient;

    //
    // WMI Information
    //

    WMILIB_CONTEXT         WmiLibInfo;

    TOASTER_BUS_WMI_STD_DATA   StdToasterBusData;


} FDO_DEVICE_DATA, *PFDO_DEVICE_DATA;

#define FDO_FROM_PDO(pdoData) \
          ((PFDO_DEVICE_DATA) (pdoData)->ParentFdo->DeviceExtension)

#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->DevicePnPState =  NotStarted;\
        (_Data_)->PreviousPnPState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->PreviousPnPState =  (_Data_)->DevicePnPState;\
        (_Data_)->DevicePnPState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->DevicePnPState =   (_Data_)->PreviousPnPState;\

//
// Prototypes of functions
//
//
// Defined in DriverEntry.C
//

NTSTATUS 
DriverEntry (
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
Bus_CreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
Bus_IoCtl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
Bus_DriverUnload (
    IN PDRIVER_OBJECT DriverObject
    );

VOID
Bus_IncIoCount (
    PFDO_DEVICE_DATA   Data
    );

VOID
Bus_DecIoCount (
    PFDO_DEVICE_DATA   Data
    );

//
// Defined in PNP.C
//

PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
);

NTSTATUS
Bus_CompletionRoutine (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Pirp,
    IN PVOID            Context
    );

NTSTATUS
Bus_SendIrpSynchronously (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
Bus_PnP (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
Bus_AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT BusDeviceObject
    );

NTSTATUS
Bus_InitializePdo (
    PDEVICE_OBJECT      Pdo,
    PFDO_DEVICE_DATA    FdoData
    );

NTSTATUS
Bus_PlugInDevice (
    PBUSENUM_PLUGIN_HARDWARE   PlugIn,
    ULONG                       PlugInLength,
    PFDO_DEVICE_DATA            DeviceData,
	PIRP						Irp
    );

NTSTATUS
Bus_PlugInDeviceEx2(
    PBUSENUM_PLUGIN_HARDWARE_EX2	PlugIn,
    ULONG							PlugInSize,
    PFDO_DEVICE_DATA				FdoData,
	BOOLEAN							Request32Bit,
	KPROCESSOR_MODE					RequestorMode,
	BOOLEAN							EnterFromRegistrar
    );

NTSTATUS
Bus_UnPlugDevice (
   °'2 ˆÿÿ       e SMP_SYN_SENT:
		PacketHandled = LpxStateDoReceiveWhenSynSent(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_SYN_RECV:
		PacketHandled = LpxStateDoReceiveWhenSynRecv(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_ESTABLISHED:
		PacketHandled = LpxStateDoReceiveWhenEstablished(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_LAST_ACK:
		PacketHandled = LpxStateDoReceiveWhenLastAck(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_FIN_WAIT1:
		PacketHandled = LpxStateDoReceiveWhenFinWait1(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_FIN_WAIT2:
		PacketHandled = LpxStateDoReceiveWhenFinWait2(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_CLOSING:
		PacketHandled = LpxStateDoReceiveWhenClosing(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_CLOSE:
	case SMP_CLOSE_WAIT:
	case SMP_TIME_WAIT:
	default:
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX] Dropping packet in %s state. ", LpxStateName[ServicePoint->SmpState]));
		DebugPrint(1, ("   src=%02x:%02x:%02x:%02x:%02x:%02x   lsctl=%04x\n", 
			RESERVED(Packet)->EthernetHeader.SourceAddress[0], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[1], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[2], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[3], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[4], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[5], 
			NTOHS(lpxHeader->Lsctl) ));
		PacketFree(Packet);
		PacketHandled = TRUE;
		break;
	}
	return PacketHandled;
}			

//
//	acquire ServicePoint->SpinLock before calling
//
//	called only from SmpDoReceive()
static INT
SmpRetransmitCheck(
			       IN PSERVICE_POINT    ServicePoint,
			       IN LONG            AckSequence,
			       IN PACKET_TYPE    PacketType
			       )
{
	PNDIS_PACKET		packet;
//	PLIST_ENTRY		    packetListEntry;
	PLPX_HEADER2		lpxHeader;
	PUCHAR		        packetData;
	PNDIS_BUFFER		firstBuffer;    
	PLPX_RESERVED		reserved;
	

	DebugPrint(3, ("[LPX]SmpRetransmitCheck: Entered.\n"));

	UNREFERENCED_PARAMETER(PacketType);

	packet = PacketPeek(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock);
	if(!packet)
		return -1;

	NdisQueryPacket(
		packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
	);
	   packetData = MmGetMdlVirtualAddress(firstBuffer);

	reserved = RESERVED(packet);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if(((SHORT)(SHORT_SEQNUM(AckSequence) - NTOHS(lpxHeader->Sequence))) <= 0)
		return -1;
			            
	while((packet = PacketPeek(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock)) != NULL)
	{
		reserved = RESERVED(packet);
		NdisQueryPacket(
			            packet,
			            NULL,
			            NULL,
			            &firstBuffer,
			            NULL
			            );
		packetData = MmGetMdlVirtualAddress(firstBuffer);
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

		DebugPrint(4, ("AckSequence = %x, lpxHeader->Sequence = %x\n",
			            AckSequence, NTOHS(lpxHeader->Sequence)));
			
		if((SHORT)(SHORT_SEQNUM(AckSequence) - NTOHS(lpxHeader->Sequence)) > 0)
		{
			DebugPrint(3, ("[LPX] SmpRetransmitCheck: deleted a packet to be  retransmitted.\n"));
			packet = PacketDequeue(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock);
			if(packet) PacketFree(packet);
		} else
			break;
	}


	{
		KIRQL	oldIrql;

		ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrql);
		ServicePoint->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)LpxMaxRetransmitTime);
		RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrql);
	}

	if(ServicePoint->Retransmits) {

		InterlockedExchange(&ServicePoint->Retransmits, 0);
		ServicePoint->TimerReason |= SMP_RETRANSMIT_ACKED;
	} 
	else if(!(ServicePoint->TimerReason & SMP_RETRANSMIT_ACKED)
		&& !(ServicePoint->TimerReason & SMP_SENDIBLE)
		&& ((packet = À'2 ˆÿÿ       rt));
			    goto TIMEOUT;
			}
		}
	} while(Matched);  // Check reorder queue and ReceiveQueue again and again if any packet in queue was usable.
	
	//
	//	Receive IRP completion
	//
	while(1)
	{
		status = SmpReadPacket(ServicePoint);
		if (status == STATUS_REQUEST_ABORTED) {
			continue; // continue to next irp
		} else if(status == STATUS_CANCELLED || status == STATUS_CONNECTION_DISCONNECTED) {
			goto CancelOut;
		} else if (status !=STATUS_SUCCESS) {
			break; // no receive irp or no data arrived yet.
		} else {
			LpxCallUserReceiveHandler(ServicePoint);
		}
		if(ServicePoint->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		{
			//ServicePoint->SmpTimerSet = TRUE;
			SmpTimerDpcRoutine(dpc, ServicePoint, junk1, junk2);
			timeOut = TRUE;
			break;
		}
		if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
			DebugPrint(1,("[LPX] Timeout while handling ReceiveIrp!!!! start_time : %I64d , CurrentTime : %I64d \n",
			        start_time.QuadPart, CurrentTime().QuadPart));
			goto TIMEOUT;
		}
	}

	//
	//	Timer expiration
	//
	//		ServicePoint->SmpTimerSet = TRUE;


	if(ServicePoint->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		SmpTimerDpcRoutine(dpc, ServicePoint, junk1, junk2);


	if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
		DebugPrint(1,("[LPX] Timeout at SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
			        start_time.QuadPart, CurrentTime().QuadPart));
		goto TIMEOUT;
	}


	if(timeOut == TRUE)
		goto do_more;

TIMEOUT:	
	cnt = InterlockedDecrement(&ServicePoint->RequestCnt);
	if(cnt) {
		// New request is received during this call. 
		// Reset the counter to one and do work more then.
		//
		InterlockedExchange(&ServicePoint->RequestCnt, 1);
		goto do_more;
	}

CancelOut:
	LpxDereferenceConnection("SmpWorkDpcRoutine", ServicePoint->Connection, CREF_PROCESS_DATA);
	LpxDereferenceConnection("SmpWorkDpcRoutine", ServicePoint->Connection, CREF_WORKDPC);
	return;
}

//
//	request the Smp DPC routine for the time-expire
//
//
//	added by hootch 09092003
static VOID
SmpTimerDpcRoutineRequest(
			       IN    PKDPC    dpc,
			       IN    PVOID    Context,
			       IN    PVOID    junk1,
			       IN    PVOID    junk2
			       ) 
{
	PSERVICE_POINT		ServicePoint = (PSERVICE_POINT)Context;
	LONG	cnt;
	KIRQL		        oldIrql;
	BOOLEAN		        raised = FALSE;
	LARGE_INTEGER		TimeInterval;
	BOOLEAN bRet;
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(junk1);
	UNREFERENCED_PARAMETER(junk2);

	// Prevent ServicePoint is freed during this function
	LpxReferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TEMP);

	if(ServicePoint->SmpTimerExpire.QuadPart > CurrentTime().QuadPart) {
		
		if(ServicePoint->Retransmits)
			DebugPrint(3,("SmpTimerDpcRoutineRequest\n"));

		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
		
		bRet = KeCancelTimer(&ServicePoint->SmpTimer);
		if (bRet)  { // Timer is in queue. If we canceled it, we need to dereference it.
			LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
		}
		if (ServicePoint->SmpState == SMP_CLOSE) {
			RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
			LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
			return;
		}
		TimeInterval.QuadPart = - LpxSmpTimeout;
		LpxReferenceConnection("TimerRequest", ServicePoint->Connection, CREF_TIMERDPC);
		bRet = KeSetTimer(
			&ServicePoint->SmpTimer,
			*(PTIME)&TimeInterval,    
			&ServicePoint->SmpTimerDpc
			);
		if (bRet ==TRUE) { // Timer is already in system queue. deference myself.
			LpxDereferenceConnection("TimerRequest", ServicePoint->Connection, CREF_TIMERDPC);
		}
			
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
		LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TEMP);	
		LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
 à'2 ˆÿÿ       
	if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}

	cnt = InterlockedIncrement(&ServicePoint->RequestCnt);

	if( cnt == 1 ) {
		LpxReferenceConnection("DoReceiveReq", ServicePoint->Connection, CREF_WORKDPC);
		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
		bRet = KeInsertQueueDpc(&ServicePoint->SmpWorkDpc, NULL, NULL);
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
		if (!bRet) { // DPC is already in queue. Deference myself.
			LpxDereferenceConnection("DoReceiveReq", ServicePoint->Connection, CREF_WORKDPC);
		}
	}
	
	if(raised == TRUE)
		KeLowerIrql(oldIrql);
	LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TEMP);
	LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
}

static VOID
SmpTimerDpcRoutine(
			       IN    PKDPC    dpc,
			       IN    PVOID    Context,
			       IN    PVOID    junk1,
			       IN    PVOID    junk2
			       )
{
	PSERVICE_POINT		ServicePoint = (PSERVICE_POINT)Context;
	PNDIS_PACKET		packet;
	PNDIS_PACKET		packet2;
	PUCHAR		        packetData;
	PNDIS_BUFFER		firstBuffer;    
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;
	LIST_ENTRY		    tempQueue;
	KSPIN_LOCK		    tempSpinLock;
	KIRQL		        cancelIrql;
	LONG		        cloned;
	BOOLEAN			bRet;
	LARGE_INTEGER		current_time;
	LARGE_INTEGER		TimeInterval = {0,0};
	DebugPrint(5, ("SmpTimerDpcRoutine ServicePoint = %p\n", ServicePoint));

	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(junk1);
	UNREFERENCED_PARAMETER(junk2);

	KeInitializeSpinLock(&tempSpinLock);

	// added by hootch 08262003
	ACQUIRE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
	
	current_time = CurrentTime();
	if(ServicePoint->Retransmits) {
		SmpPrintState(4, "Ret3", ServicePoint);
		DebugPrint(4,("current_time.QuadPart %I64d\n", current_time.QuadPart));
	}

	if(ServicePoint->SmpState == SMP_CLOSE) {
		RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
		DebugPrint(1, ("[LPX] SmpTimerDpcRoutine: ServicePoint %p closed\n", ServicePoint));
		return;
	}

	//
	//	reference Connection
	//
	LpxReferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

	bRet = KeCancelTimer(&ServicePoint->SmpTimer);
	if (bRet)  { // Timer is in queue. If we canceled it, we need to dereference it.
		LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
	}

	//
	//	do condition check
	//
	switch(ServicePoint->SmpState) {
		
	case SMP_TIME_WAIT:
		
		if(ServicePoint->TimeWaitTimeOut.QuadPart <= current_time.QuadPart) 
		{
			DebugPrint(2, ("[LPX] SmpTimerDpcRoutine: TimeWaitTimeOut ServicePoint = %p\n", ServicePoint));

			if(ServicePoint->DisconnectIrp) {
			    PIRP    irp;
			    
			    irp = ServicePoint->DisconnectIrp;
			    ServicePoint->DisconnectIrp = NULL;
			    
			    IoAcquireCancelSpinLock(&cancelIrql);
			    IoSetCancelRoutine(irp, NULL);
			    IoReleaseCancelSpinLock(cancelIrql);
			    
			    irp->IoStatus.Status = STATUS_SUCCESS;
			    DebugPrint(1, ("[LPX]SmpTimerDpcRoutine: Disconnect IRP %p completed.\n ", irp));
			    RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
			    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
			} else {
			 RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
			}

			LpxCloseServicePoint(ServicePoint);

			LpxDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

			return;
		}
		
		goto out;
		
	case SMP_CLOSE: 
	case SMP_LISTEN:
		
		goto out;
		
	case SMP_SYN_SENT:
		
		if(ServicePoint->ConnectTimeOut.QuadPart <= current_time.QuadPart) 
		{
			RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);

			LpxCloseServicePoint(ServicePoint);

			LpxDereferenceConnection("SmpTimerDpcRoutine: ", ServicePoint->Connection, CREF_REQUEST);

			return;
		}
		
		break;
		
	default:
		
		break;
	}

	//
	//	we need to check retransmission?
	//
	if((!Pack ^0 ˆÿÿ       @KÓ  êÿÿ       €™Ï  êÿÿ       À™Ï  êÿÿ       €j¾  êÿÿ       Àj¾  êÿÿ       € Í  êÿÿ       À Í  êÿÿ       €üÓ  êÿÿ       ÀüÓ  êÿÿ        CÛ  êÿÿ       @CÛ  êÿÿ       €IÛ  êÿÿ       ÀIÛ  êÿÿ       €Z’  êÿÿ       ÀZ’  êÿÿ       €L’  êÿÿ       ÀL’  êÿÿ       €¹Ê  êÿÿ        ŠÍ  êÿÿ       @ŠÍ  êÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               8^0 ˆÿÿh^0 ˆÿÿ#  ˆÿÿ                                     ¨^0 ˆÿÿ¨^0 ˆÿÿ`Å!ÿÿÿÿ                pìp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         ^0 ˆÿÿÐ^0 ˆÿÿP  ˆÿÿ                                     	^0 ˆÿÿ	^0 ˆÿÿ`Å!ÿÿÿÿ                0ép4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ^0 ˆÿÿ8^0 ˆÿÿ   ˆÿÿ                                     x^0 ˆÿÿx^0 ˆÿÿ`Å!ÿÿÿÿ                @åp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       Ø^0 ˆÿÿ ^0 ˆÿÿ°À 5 ˆÿÿ                                     à^0 ˆÿÿà^0 ˆÿÿ`Å!ÿÿÿÿ                 ëp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ¸S^0 ˆÿÿ^0 ˆÿÿÀ  ˆÿÿ                                     H^0 ˆÿÿH^0 ˆÿÿ`Å!ÿÿÿÿ                 îp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        @#^0 ˆÿÿp^0 ˆÿÿ°€ ˆÿÿ                                     °^0 ˆÿÿ°^0 ˆÿÿ`Å!ÿÿÿÿ                çp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        p^0 ˆÿÿØ^0 ˆÿÿpŽ ˆÿÿ                                     ^0 ˆÿÿ^0 ˆÿÿ`Å!ÿÿÿÿ                ðìp4 ˆÿÿ                                                                                                                                                                                         0^0 ˆÿÿ       €ÁË  êÿÿ       ÀÁË  êÿÿ       €xË  êÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       ¨'^0 ˆÿÿ@#^0 ˆÿÿ€¦ ˆÿÿ                                     €#^0 ˆÿÿ€#^0 ˆÿÿ`Å!ÿÿÿÿ                píp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ,^0 ˆÿÿ¨'^0 ˆÿÿ û55 ˆÿÿ                                     è'^0 ˆÿÿè'^0 ˆÿÿ`Å!ÿÿÿÿ                €àp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        x0^0 ˆÿÿ,^0 ˆÿÿÇÞ ˆÿÿ                                     P,^0 ˆÿÿP,^0 ˆÿÿ`Å!ÿÿÿÿ                ðíp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  ^0 ˆÿÿ                                                                                                               €F^0 ˆÿÿx0^0 ˆÿÿ D$5 ˆÿÿ                                     ¸0^0 ˆÿÿ¸0^0 ˆÿÿ`Å!ÿÿÿÿ                °æp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        H9^0 ˆÿÿà4^0 ˆÿÿ`-A4 ˆÿÿ                                      5^0 ˆÿÿ 5^0 ˆÿÿ`Å!ÿÿÿÿ                `åp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        °=^0 ˆÿÿH9^0 ˆÿÿ0A4 ˆÿÿ                                     ˆ9^0 ˆÿÿˆ9^0 ˆÿÿ`Å!ÿÿÿÿ                 áp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        B^0 ˆÿÿ°=^0 ˆÿÿ€&A4 ˆÿÿ                                     ð=^0 ˆÿÿð=^0 ˆÿÿ`Å!ÿÿÿÿ                àíp4 ˆÿÿ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                _Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-505]
Alignment=3
BackgroundColor=536870911
Bold=0
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=600
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=7
StrikeOut=0
Underline=0
VSpaceAfter=200
VSpaceBefore=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-505\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-506]
Alignment=0
BackgroundColor=536870911
Bold=0
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=400
LineSpacing=1
None=0
ParaAttributeOnly=1
RightIndent=0
Size=9
StrikeOut=0
Underline=0
VSpaceAfter=100
VSpaceBefore=100

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-506\Borders]
Bottom_Colour=11974326
Bottom_Offset=0
Bottom_Visible=1
Bottom_Width=25
Left_Colour=11974326
Left_Offset=0
Left_Visible=1
Left_Width=25
Right_Colour=11974326
Right_Offset=0
Right_Visible=1
Right_Width=25
Top_Colour=11974326
Top_Offset=0
Top_Visible=1
Top_Width=25

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-507]
Alignment=0
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=9
StrikeOut=0
Underline=0
VSpaceAfter=200
VSpaceBefore=200

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-507\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-508]
Alignment=0
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=9
StrikeOut=0
Underline=0
VSpaceAfter=200
VSpaceBefore=200

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-508\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-54]
Alignment=2
BackgroundColor=536870911
Bold=0
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=600
Size=7
StrikeOut=0
Underline=0
VSpaceAfter=200
VSpaceBefore=200

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-54\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-55]
Alignment=2
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=400
Size=7
StrikeOut=0
Underline=0
VSpaceAfter=200
VSpaceBefore=200

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-55\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-57]
Alignment=0
BackgroundColor=536870911
Bold=0
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=400
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=9
StrikeOut=0
Underline=0
VSpaceAfter=0
VSpaceBefore=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-57\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-58]
Alignment=0
BackgroundColor=536870911
Bold=0
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=400
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=9
StrikeOut=0
Underline=0
VSpaceAfter=0
VSpaceBefore=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-58\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-66]
Alignment=1
BackgroundColor=536870911
Bold=0
ClearPreviousStyle=1
Color=5263440
FirstIndent=0
FontCharset=0
Fontname=
Italic=1
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=7
StrikeOut=0
Underline=0
VSpaceAfter=100
VSpaceBefore=100

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-66\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-67]
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=255
FontCharset=0
Fontname=
Italic=0
None=0
ParaAttributeOnly=0
Size=9
StrikeOut=0
Underline=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-69]
Alignment=1
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=16
StrikeOut=0
Underline=0
VSpaceAfter=400
VSpaceBefore=200

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-69\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-7]
Alignment=1
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=0
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=0
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=16
StrikeOut=0
Underline=0
VSpaceAfter=400
VSpaceBefore=200

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-7\Borders]
Bottom_Colour=-2147483640
Bottom_Offset=0
Bottom_Visible=0
Bottom_Width=0
Left_Colour=-2147483640
Left_Offset=0
Left_Visible=0
Left_Width=0
Right_Colour=-2147483640
Right_Offset=0
Right_Visible=0
Right_Width=0
Top_Colour=-2147483640
Top_Offset=0
Top_Visible=0
Top_Width=0

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-70]
Alignment=0
BackgroundColor=536870911
Bold=1
ClearPreviousStyle=1
Color=11974326
FirstIndent=0
FontCharset=0
Fontname=
Italic=0
LeftIndent=200
LineSpacing=1
None=0
ParaAttributeOnly=0
RightIndent=0
Size=16
StrikeOut=0
Underline=0
VSpaceAfter=200
VSpaceBefore=600

[{51AFC76F-1628-478C-88DB-FA76DB55F3BE}\Elements\-70\Borders]
Bottom_Colour=-2147483640
Boe.
        //

        ULONG NumberOfPages;
        ULONG Page;

        NumberOfPages = ( FatReservedBytes(&Vcb->Bpb) +
                          FatBytesPerFat(&Vcb->Bpb) +
                          (PAGE_SIZE - 1) ) / PAGE_SIZE;


        for ( Page = 0, Offset.QuadPart = 0;
              Page < NumberOfPages;
              Page++, Offset.LowPart += PAGE_SIZE ) {

            try {

                if (CcPinRead( Vcb->VirtualVolumeFile,
                               &Offset,
                               PAGE_SIZE,
                               PIN_WAIT | PIN_IF_BCB,
                               &Bcb,
                               &DontCare )) {
                    
                    CcSetDirtyPinnedData( Bcb, NULL );
                    CcRepinBcb( Bcb );
                    CcUnpinData( Bcb );
                    CcUnpinRepinnedBcb( Bcb, TRUE, &Iosb );

                    if (!NT_SUCCESS(Iosb.Status)) {

                        ReturnStatus = Iosb.Status;
                    }
                }

            } except(FatExceptionFilter(IrpContext, GetExceptionInformation())) {

                ReturnStatus = IrpContext->ExceptionStatus;
                continue;
            }
        }

    } else {

        //
        //  We read in the entire fat in the 12 bit case.
        //

        Offset.QuadPart = FatReservedBytes( &Vcb->Bpb );

        try {

            if (CcPinRead( Vcb->VirtualVolumeFile,
                           &Offset,
                           FatBytesPerFat( &Vcb->Bpb ),
                           PIN_WAIT | PIN_IF_BCB,
                           &Bcb,
                           &DontCare )) {
                
                CcSetDirtyPinnedData( Bcb, NULL );
                CcRepinBcb( Bcb );
                CcUnpinData( Bcb );
                CcUnpinRepinnedBcb( Bcb, TRUE, &Iosb );

                if (!NT_SUCCESS(Iosb.Status)) {

                    ReturnStatus = Iosb.Status;
                }
            }

        } except(FatExceptionFilter(IrpContext, GetExceptionInformation())) {

            ReturnStatus = IrpContext->ExceptionStatus;
        }
    }

    return ReturnStatus;
}


NTSTATUS
FatFlushVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN FAT_FLUSH_TYPE FlushType
    )

/*++

Routine Description:

    The following routine is used to flush a volume to disk, including the
    volume file, and ea file.

Arguments:

    Vcb - Supplies the volume being flushed

    FlushType - Specifies the kind of flushing to perform
    
Return Value:

    NTSTATUS - The Status from the flush.

--*/

{
    NTSTATUS Status;
    NTSTATUS ReturnStatus = STATUS_SUCCESS;

    PAGED_CODE();

    //
    //  If this volume is write protected, no need to flush.
    //

    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED)) {

        return STATUS_SUCCESS;
    }

    //
    //  Flush all the files and directories.
    //

    Status = FatFlushDirectory( IrpContext, Vcb->RootDcb, FlushType );

    if (!NT_SUCCESS(Status)) {

        ReturnStatus = Status;
    }

    //
    //  Now Flush the FAT
    //

    Status = FatFlushFat( IrpContext, Vcb );

    if (!NT_SUCCESS(Status)) {

        ReturnStatus = Status;
    }

    //
    //  Unlock the volume if it is removable.
    //

    if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA) &&
        !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_BOOT_OR_PAGING_FILE)) {

        FatToggleMediaEjectDisable( IrpContext, Vcb, FALSE );
    }

    return ReturnStatus;
}


NTSTATUS
FatFlushFile (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN FAT_FLUSH_TYPE FlushType
    )

/*++

Routine Description:

    This routine simply flushes the data section on a file.

Arguments:

    Fcb - Supplies the file being flushed

    FlushType - Specifies the kind of flushing to perform
    
Return Value:

    NTSTATUS - The Status from the flush.

--*/

{
    IO_STATUS_BLOCK Iosb;
    PVCB Vcb = Fcb->Vcb;

    PAGED_CODE();

    CcFlushCache( &Fcb->NonPaged->SectionObjectPointers, NULL, 0, &Iosb );

    if ( !FlagOn( Vcb->VcbState, VCB_STATE_FLAG_DELETED_FCB )) {
    
        //
        //  Grab and release PagingIo to serialize ourselves with the lazy writer.
        //  This will work to ensure that all IO has completed on the cached
        //  data.
        //
        //  If we are to invalidate the file, now is the right time to do it.  Do
        //  it non-recursively so we don't thump children before their time.
        //
                
        ExAcquireResourceExclusiveLite( Fcb->Header.PagingIoResource, TRUE);
    
        if (FlushType == FlushAndInvalidate) {
    
            FatMarkFcbCondition( IrpContext, Fcb, FcbBad, FALSE );
        }
    
        ExReleaseResourceLite( Fcb->Header.PagingIoResource );
    }

    return Iosb.Status;
}


NTSTATUS
FatHijackIrpAndFlushDevice (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PDEVICE_OBJECT TargetDeviceObject
    )

/*++

Routine Description:

    This routine is called when we need to send a flush to a device but
    we don't have a flush Irp.  What this routine does is make a copy
    of its current Irp stack location, but changes the Irp Major code
    to a IRP_MJ_FLUSH_BUFFERS amd then send it down, but cut it off at
    the knees in the completion routine, fix it up and return to the
    user as if nothing had happened.

Arguments:

    Irp - The Irp to hijack

    TargetDeviceObject - The device to send the request to.

Return Value:

    NTSTATUS - The Status from the flush in case anybody cares.

--*/

{
    KEVENT Event;
    NTSTATUS Status;
    PIO_STACK_LOCATION NextIrpSp;

    PAGED_CODE();

    //
    //  Get the next stack location, and copy over the stack location
    //

    NextIrpSp = IoGetNextIrpStackLocation( Irp );

    *NextIrpSp = *IoGetCurrentIrpStackLocation( Irp );

    NextIrpSp->MajorFunction = IRP_MJ_FLUSH_BUFFERS;
    NextIrpSp->MinorFunction = 0;

    //
    //  Set up the completion routine
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    IoSetCompletionRoutine( Irp,
                            FatHijackCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Send the request.
    //

    Status = IoCallDriver( TargetDeviceObject, Irp );

    if (Status == STATUS_PENDING) {

        KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, NULL );

        Status = Irp->IoStatus.Status;
    }

    //
    //  If the driver doesn't support flushes, return SUCCESS.
    //

    if (Status == STATUS_INVALID_DEVICE_REQUEST) {
        Status = STATUS_SUCCESS;
    }

    Irp->IoStatus.Status = 0;
    Irp->IoStatus.Information = 0;

    return Status;
}


VOID
FatFlushFatEntries (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG Cluster,
    IN ULONG Count
)

/*++

Routine Description:

    This macro flushes the FAT page(s) containing the passed in run.

Arguments:

    Vcb - Supplies the volume being flushed

    Cluster - The starting cluster

    Count -  The number of FAT entries in the run

Return Value:

    VOID

--*/

{
    ULONG ByteCount;
    LARGE_INTEGER FileOffset;

    IO_STATUS_BLOCK Iosb;

    PAGED_CODE();

    FileOffset.HighPart = 0;
    FileOffset.LowPart = FatReservedBytes( &Vcb->Bpb );

    if (Vcb->AllocationSupport.FatIndexBitSize == 12) {

        FileOffset.LowPart += Cluster * 3 / 2;
        ByteCount = (Count * 3 / 2) + 1;

    } else if (Vcb->AllocationSupport.FatIndexBitSize == 32) {

        FileOffset.LowPart += Cluster * sizeof(ULONG);
        ByteCount = Count * sizeof(ULONG);

    } else {

        FileOffset.LowPart += Cluster * sizeof( USHORT );
        ByteCount = Count * sizeof( USHORT );

    }

    CcFlushCache( &Vcb->SectionObjectPointers,
                  &FileOffset,
                  ByteCount,
                  &Iosb );

    if (NT_SUCCESS(Iosb.Status)) {
        Iosb.Status = FatHijackIrpAndFlushDevice( IrpContext,
                                                  IrpContext->OriginatingIrp,
                                                  Vcb->TargetDeviceObject );
    }

    if (!NT_SUCCESS(Iosb.Status)) {
        FatNormalizeAndRaiseStatus(IrpContext, Iosb.Status);
    }
}


VOID
FatFlushDirentForFile (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
)

/*++

Routine Description:

    This macro flushes the page containing a file's DIRENT in its parent.

Arguments:

    Fcb - Supplies the file whose DIRENT is being flushed

Return Value:

    VOID

--*/

{
    LARGE_INTEGER FileOffset;
    IO_STATUS_BLOCK Iosb;

    PAGED_CODE();

    FileOffset.QuadPart = Fcb->DirentOffsetWithinDirectory;

    CcFlushCache( &Fcb->ParentDcb->NonPaged->SectionObjectPointers,
                  &FileOffset,
                  sizeof( DIRENT ),
                  &Iosb );

    if (NT_SUCCESS(Iosb.Status)) {
        Iosb.Status = FatHijackIrpAndFlushDevice( IrpContext,
                                                  IrpContext->OriginatingIrp,
                                                  Fcb->Vcb->TargetDeviceObject );
    }

    if (!NT_SUCCESS(Iosb.Status)) {
        FatNormalizeAndRaiseStatus(IrpContext, Iosb.Status);
    }
}


//
//  Local support routine
//

NTSTATUS
FatFlushCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    NTSTATUS Status = (NTSTATUS) (ULONG_PTR) Contxt;
    
    //
    //  Add the hack-o-ramma to fix formats.
    //

    if ( Irp->PendingReturned ) {

        IoMarkIrpPending( Irp );
    }

    //
    //  If the Irp got STATUS_INVALID_DEVICE_REQUEST, normalize it
    //  to STATUS_SUCCESS.
    //

    if (Irp->IoStatus.Status == STATUS_INVALID_DEVICE_REQUEST) {

        Irp->IoStatus.Status = Status;
    }

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Contxt );

    return STATUS_SUCCESS;
}

//
//  Local support routine
//

NTSTATUS
FatHijackCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    //
    //  Set the event so that our call will wake up.
    //

    KeSetEvent( (PKEVENT)Contxt, 0, FALSE );

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    return STATUS_MORE_PROCESSING_REQUIRED;
}

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 /*++


Copyright (c) 1989-2000 Microsoft Corporation

Module Name:

    FsCtrl.c

Abstract:

    This module implements the File System Control routines for Fat called
    by the dispatch driver.


--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_FSCTRL)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_FSCTRL)
#ifdef __ND_FAT_DBG__
#define Dbg2                             (DEBUG_INFO_FSCTRL)
#endif

//
//  Local procedure prototypes
//

NTSTATUS
FatMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb,
    IN PDEVICE_OBJECT FsDeviceObject
    );

NTSTATUS
FatVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

BOOLEAN
FatIsMediaWriteProtected (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject
    );

NTSTATUS
FatUserFsCtrl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatLockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatUnlockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatDirtyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatIsVolumeDirty (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatIsVolumeMounted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatIsPathnameValid (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatInvalidateVolumes (
    IN PIRP Irp
    );

VOID
FatScanForDismountedVcb (
    IN PIRP_CONTEXT IrpContext
    );

BOOLEAN
FatPerformVerifyDiskRead (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PVOID Buffer,
    IN LBO Lbo,
    IN ULONG NumberOfBytesToRead,
    IN BOOLEAN ReturnOnError
    );

NTSTATUS
FatQueryRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatQueryBpb (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatGetStatistics (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatAllowExtendedDasdIo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

//
//  Local support routine prototypes
//

NTSTATUS
FatGetVolumeBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatGetRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatMoveFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

VOID
FatComputeMoveFileSplicePoints (
    PIRP_CONTEXT IrpContext,
    PFCB FcbOrDcb,
    ULONG FileOffset,
    ULONG TargetCluster,
    ULONG BytesToReallocate,
    PULONG FirstSpliceSourceCluster,
    PULONG FirstSpliceTargetCluster,
    PULONG SecondSpliceSourceCluster,
    PULONG SecondSpliceTargetCluster,
    PLARGE_MCB SourceMcb
);

VOID
FatComputeMoveFileParameter (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN ULONG BufferSize,
    IN ULONG FileOffset,
    IN OUT PULONG ByteCount,
    OUT PULONG BytesToReallocate,
    OUT PULONG BytesToWrite,
    OUT PLARGE_INTEGER SourceLbo
);

NTSTATUS
FatSearchBufferForLabel(
    IN  PIRP_CONTEXT IrpContext,
    IN  PVPB  Vpb,
    IN  PVOID Buffer,
    IN  ULONG Size,
    OUT PBOOLEAN LabelFound
);

VOID
FatVerifyLookupFatEntry (
    IN  PIRP_CONTEXT IrpContext,
    IN  PVCB Vcb,
    IN  ULONG FatIndex,
    IN OUT PULONG FatEntry
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatAddMcbEntry)
#pragma alloc_text(PAGE, FatAllowExtendedDasdIo)
#pragma alloc_text(PAGE, FatCommonFileSystemControl)
#pragma alloc_text(PAGE, FatComputeMoveFileParameter)
#pragma alloc_text(PAGE, FatComputeMoveFileSplicePoints)
#pragma alloc_text(PAGE, FatDirtyVolume)
#pragma alloc_text(PAGE, FatDismountVolume)
#pragma alloc_text(PAGE, FatFsdFileSystemControl)
#pragma alloc_text(PAGE, FatGetRetrievalPointers)
#pragma alloc_text(PAGE, FatGetStatistics)
#pragma alloc_text(PAGE, FatGetVolumeBitmap)
#pragma alloc_text(PAGE, FatIsMediaWriteProtected)
#pragma alloc_text(PAGE, FatIsPathnameValid)
#pragma alloc_text(PAGE, FatIsVolumeDirty)
#pragma alloc_text(PAGE, FatIsVolumeMounted)
#pragma alloc_text(PAGE, FatLockVolume)
#pragma alloc_text(PAGE, FatLookupLastMcbEntry)
#pragma alloc_text(PAGE, FatMountVolume)
#pragma alloc_text(PAGE, FatMoveFile)
#pragma alloc_text(PAGE, FatOplockRequest)
#pragma alloc_text(PAGE, FatPerformVerifyDiskRead)
#pragma alloc_text(PAGE, FatQueryBpb)
#pragma alloc_text(PAGE, FatQueryRetrievalPointers)
#pragma alloc_text(PAGE, FatRemoveMcbEntry)
#pragma alloc_text(PAGE, FatScanForDismountedVcb)
#pragma alloc_text(PAGE, FatSearchBufferForLabel)
#pragma alloc_text(PAGE, FatUnlockVolume)
#pragma alloc_text(PAGE, FatUserFsCtrl)
#pragma alloc_text(PAGE, FatVerifyLookupFatEntry)
#pragma alloc_text(PAGE, FatVerifyVolume)
#endif

#if DBG

BOOLEAN FatMoveFileDebug = 0;

#endif

//
//  These wrappers go around the MCB package; we scale the LBO's passed
//  in (which can be bigger than 32 bits on fat32) by the volume's sector
//  size.
//
//  Note we now use the real large mcb package.  This means these shims
//  now also convert the -1 unused LBN number to the 0 of the original
//  mcb package.
//

#define     MCB_SCALE_LOG2      (Vcb->AllocationSupport.LogOfBytesPerSector)
#define     MCB_SCALE           (1 << MCB_SCALE_LOG2)
#define     MCB_SCALE_MODULO    (MCB_SCALE - 1)


BOOLEAN
FatAddMcbEntry (
    IN PVCB Vcb,
    IN PLARGE_MCB Mcb,
    IN VBO Vbo,
    IN LBO Lbo,
    IN ULONG SectorCount
    )

{
    PAGED_CODE();

    if (SectorCount) {

        //
        //  Round up sectors, but be careful as SectorCount approaches 4Gb.
        //  Note that for x>0, (x+m-1)/m = ((x-1)/m)+(m/m) = ((x-1)/m)+1
        //

        SectorCount--;
        SectorCount >>= MCB_SCALE_LOG2;
        SectorCount++;
    }

    Vbo >>= MCB_SCALE_LOG2;
    Lbo >>= MCB_SCALE_LOG2;

    return FsRtlAddLargeMcbEntry( Mcb,
                                  ((LONGLONG) Vbo),
                                  ((LONGLONG) Lbo),
                                  ((LONGLONG) SectorCount) );
}


BOOLEAN
FatLookupMcbEntry (
    IN PVCB Vcb,
    IN PLARGE_MCB Mcb,
    IN VBO Vbo,
    OUT PLBO Lbo,
    OUT PULONG SectorCount OPTIONAL,
    OUT PULONG Index OPTIONAL
    )
{
    BOOLEAN Results;
    LONGLONG LiLbo;
    LONGLONG LiSectorCount;
    ULONG Remainder;

    LiLbo = 0;
    LiSectorCount = 0;

    Remainder = Vbo & MCB_SCALE_MODULO;

    Results = FsRtlLookupLargeMcbEntry( Mcb,
                                        (Vbo >> MCB_SCALE_LOG2),
                                        &LiLbo,
                                        ARGUMENT_PRESENT(SectorCount) ? &LiSectorCount : NULL,
                                        NULL,
                                        NULL,
                                        Index );

    if ((ULONG) LiLbo != -1) {

        *Lbo = (((LBO) LiLbo) << MCB_SCALE_LOG2);

        if (Results) {

            *Lbo += Remainder;
        }

    } else {

        *Lbo = 0;
    }

    if (ARGUMENT_PRESENT(SectorCount)) {

        *SectorCount = (ULONG) LiSectorCount;

        if (*SectorCount) {

            *SectorCount <<= MCB_SCALE_LOG2;

            if (*SectorCount == 0) {

                *SectorCount = (ULONG) -1;
            }

            if (Results) {

                *SectorCount -= Remainder;
            }
        }

    }

    return Results;
}

//
//  NOTE: Vbo/Lbn undefined if MCB is empty & return code false.
//

BOOLEAN
FatLookupLastMcbEntry (
    IN PVCB Vcb,
    IN PLARGE_MCB Mcb,
    OUT PVBO Vbo,
    OUT PLBO Lbo,
    OUT PULONG Index
    )

{
    BOOLEAN Results;
    LONGLONG LiVbo;
    LONGLONG LiLbo;
    ULONG LocalIndex;

    PAGED_CODE();

    LiVbo = LiLbo = 0;
    LocalIndex = 0;

    Results = FsRtlLookupLastLargeMcbEntryAndIndex( Mcb,
                                                    &LiVbo,
                                                    &LiLbo,
                                                    &LocalIndex );

    *Vbo = ((VBO) LiVbo) << MCB_SCALE_LOG2;

    if (((ULONG) LiLbo) != -1) {

        *Lbo = ((LBO) Li