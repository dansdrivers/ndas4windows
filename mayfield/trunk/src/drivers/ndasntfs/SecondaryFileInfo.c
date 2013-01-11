#include "FatProcs.h"

#ifdef __ND_FAT_SECONDARY__

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)


NTSTATUS
NdFatSetBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb,
    IN PCCB Ccb
    );

NTSTATUS
NdFatSetDispositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PFCB Fcb
    );

NTSTATUS
NdFatSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN PCCB Ccb
    );

NTSTATUS
NdFatSetPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject
    );

NTSTATUS
NdFatSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb,
    IN PFILE_OBJECT FileObject
    );

NTSTATUS
NdFatSetEndOfFileInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PVCB Vcb,
    IN PFCB Fcb
    );


NTSTATUS
NdFatSecondaryCommonSetInformation (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp
	)
{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    FILE_INFORMATION_CLASS FileInformationClass;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN FcbAcquired = FALSE;

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatCommonSetInformation...\n", 0);
    DebugTrace( 0, Dbg, "Irp                    = %08lx\n", Irp);
    DebugTrace( 0, Dbg, "->Length               = %08lx\n", IrpSp->Parameters.SetFile.Length);
    DebugTrace( 0, Dbg, "->FileInformationClass = %08lx\n", IrpSp->Parameters.SetFile.FileInformationClass);
    DebugTrace( 0, Dbg, "->FileObject           = %08lx\n", IrpSp->Parameters.SetFile.FileObject);
    DebugTrace( 0, Dbg, "->ReplaceIfExists      = %08lx\n", IrpSp->Parameters.SetFile.ReplaceIfExists);
    DebugTrace( 0, Dbg, "->Buffer               = %08lx\n", Irp->AssociatedIrp.SystemBuffer);

#if 1

	if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

		return FatFsdPostRequest( IrpContext, Irp );
	}

#endif

    //
    //  Reference our input parameters to make things easier
    //

    FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;
    FileObject = IrpSp->FileObject;

    //
    //  Decode the file object
    //

    TypeOfOpen = FatDecodeFileObject( FileObject, &Vcb, &Fcb, &Ccb );

#if 1

	if (FlagOn(Ccb->NdFatFlags, ND_FAT_CCB_FLAG_UNOPENED) &&
		!(FileInformationClass == FileEndOfFileInformation && IrpSp->Parameters.SetFile.AdvanceOnly)) {
		
		ASSERT( FlagOn(Ccb->NdFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );

		FatCompleteRequest( IrpContext, Irp, STATUS_FILE_CORRUPT_ERROR );

		DebugTrace2( -1, Dbg, ("NtfsCommonDirectoryControl -> STATUS_FILE_CORRUPT_ERROR\n") );

		return STATUS_FILE_CORRUPT_ERROR;
	}

#endif

    try {

        //
        //  Case on the type of open we're dealing with
        //

        switch (TypeOfOpen) {

        case UserVolumeOpen:

            //
            //  We cannot query the user volume open.
            //

            try_return( Status = STATUS_INVALID_PARAMETER );
            break;

        case UserFileOpen:

            if (!FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE ) &&
                ((FileInformationClass == FileEndOfFileInformation) ||
                 (FileInformationClass == FileAllocationInformation))) {

                //
                //  We check whether we can proceed
                //  based on the state of the file oplocks.
                //

                Status = FsRtlCheckOplock( &Fcb->Specific.Fcb.Oplock,
                                           Irp,
                                           IrpContext,
                                           NULL,
                                           NULL );

                if (Status != STATUS_SUCCESS) {

                    try_return( Status );
                }

                //
                //  Set the flag indicating if Fast I/O is possible
                //

                Fcb->Header.IsFastIoPossible = FatIsFastIoPossible( Fcb );
            }
            break;

        case UserDirectoryOpen:

            break;

        default:

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  We can only do a set on a nonroot dcb, so we do the test
        //  and then fall through to the user file open code.
        //

        if (NodeType(Fcb) == FAT_NTC_ROOT_DCB) {

            if (FileInformationClass == FileDispositionInformation) {

                try_return( Status = STATUS_CANNOT_DELETE );
            }

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  In the following two cases, we cannot have creates occuring
        //  while we are here, so acquire the volume exclusive.
        //

        if ((FileInformationClass == FileDispositionInformation) ||
            (FileInformationClass == FileRenameInformation)) {

            if (!FatAcquireExclusiveVcb( IrpContext, Vcb )) {

                DebugTrace(0, Dbg, "Cannot acquire Vcb\n", 0);

                Status = FatFsdPostRequest( IrpContext, Irp );
                Irp = NULL;
                IrpContext = NULL;

                try_return( Status );
            }

            VcbAcquired = TRUE;
        }

        //
        //  We need to look here to check whether the oplock state
        //  will allow us to continue.  We may have to loop to prevent
        //  an oplock being granted between the time we check the oplock
        //  and obtain the Fcb.
        //

        //
        //  Acquire exclusive access to the Fcb,  We use exclusive
        //  because it is probable that one of the subroutines
        //  that we call will need to monkey with file allocation,
        //  create/delete extra fcbs.  So we're willing to pay the
        //  cost of exclusive Fcb access.
        //
        //  Note that we do not acquire the resource for paging file
        //  operations in order to avoid deadlock with Mm.
        //

        if (!FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

            if (!FatAcquireExclusiveFcb( IrpContext, Fcb )) {

                DebugTrace(0, Dbg, "Cannot acquire Fcb\n", 0);

                Status = FatFsdPostRequest( IrpContext, Irp );
                Irp = NULL;
                IrpContext = NULL;

                try_return( Status );
            }

            FcbAcquired = TRUE;
        }

        Status = STATUS_SUCCESS;

        //
        //  Make sure the Fcb is in a usable condition.  This
        //  will raise an error condition if the fcb is unusable
        //

        FatVerifyFcb( IrpContext, Fcb );

        //
        //  Based on the information class we'll do different
        //  actions.  Each of the procedures that we're calling will either
        //  complete the request of send the request off to the fsp
        //  to do the work.
        //

        switch (FileInformationClass) {

        case FileBasicInformation:

            Status = NdFatSetBasicInfo( IrpContext, Irp, Fcb, Ccb );
            break;

        case FileDispositionInformation:

            //
            //  If this is on deferred flush media, we have to be able to wait.
            //

            if ( FlagOn(Vcb->VcbState, VCB_STATE_FLAG_DEFERRED_FLUSH) &&
                 !FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ) {

                Status = FatFsdPostRequest( IrpContext, Irp );
                Irp = NULL;
                IrpContext = NULL;

            } else {

                Status = NdFatSetDispositionInfo( IrpContext, Irp, FileObject, Fcb );
            }

            break;

        case FileRenameInformation:

            //
            //  We proceed with this operation only if we can wait
            //

            if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

                Status = FatFsdPostRequest( IrpContext, Irp );
                Irp = NULL;
                IrpContext = NULL;

            } else {

                Status = NdFatSetRenameInfo( IrpContext, Irp, Vcb, Fcb, Ccb );

                //
                //  If STATUS_PENDING is returned it means the oplock
                //  package has the Irp.  Don't complete the request here.
                //

                if (Status == STATUS_PENDING) {
                    Irp = NULL;
                    IrpContext = NULL;
                }
            }

            break;

        case FilePositionInformation:

            Status = NdFatSetPositionInfo( IrpContext, Irp, FileObject );
            break;

        case FileLinkInformation:

            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;

        case FileAllocationInformation:

            Status = NdFatSetAllocationInfo( IrpContext, Irp, Fcb, FileObject );
            break;

        case FileEndOfFileInformation:

            Status = NdFatSetEndOfFileInfo( IrpContext, Irp, FileObject, Vcb, Fcb );
            break;

        default:

            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if ( IrpContext != NULL ) {

            FatUnpinRepinnedBcbs( IrpContext );
        }

    try_exit: NOTHING;
    } finally {

        DebugUnwind( FatCommonSetInformation );

        if (FcbAcquired) { FatReleaseFcb( IrpContext, Fcb ); }
        if (VcbAcquired) { FatReleaseVcb( IrpContext, Vcb ); }

        if (!AbnormalTermination()) {

            FatCompleteRequest( IrpContext, Irp, Status );
        }

        DebugTrace(-1, Dbg, "FatCommonSetInformation -> %08lx\n", Status);
    }

    return Status;
}

NTSTATUS
NdFatSetBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set basic information for fat.  It either
    completes the request or enqueues it off to the fsp.

Arguments:

    Irp - Supplies the irp being processed

    Fcb - Supplies the Fcb or Dcb being processed, already known not to
        be the root dcb

    Ccb - Supplies the flag bit that control updating the last modify
        time on cleanup.

Return Value:

    NTSTATUS - The result of this operation if it completes without
               an exception.

--*/

{
    NTSTATUS Status;

    PFILE_BASIC_INFORMATION Buffer;

    PDIRENT Dirent;
    PBCB DirentBcb;

    FAT_TIME_STAMP CreationTime;
    UCHAR CreationMSec;
    FAT_TIME_STAMP LastWriteTime;
    FAT_TIME_STAMP LastAccessTime;
    FAT_DATE LastAccessDate;
    UCHAR Attributes;

    BOOLEAN ModifyCreation = FALSE;
    BOOLEAN ModifyLastWrite = FALSE;
    BOOLEAN ModifyLastAccess = FALSE;

    LARGE_INTEGER LargeCreationTime;
    LARGE_INTEGER LargeLastWriteTime;
    LARGE_INTEGER LargeLastAccessTime;


    ULONG NotifyFilter = 0;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	ULONG						originalBufferFileAttributes;
	struct SetFile				setFile;


    DebugTrace(+1, Dbg, "FatSetBasicInfo...\n", 0);

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  If the user is specifying -1 for a field, that means
    //  we should leave that field unchanged, even if we might
    //  have otherwise set it ourselves.  We'll set the Ccb flag
    //  saying that the user set the field so that we
    //  don't do our default updating.
    //
    //  We set the field to 0 then so we know not to actually
    //  set the field to the user-specified (and in this case,
    //  illegal) value.
    //

    if (Buffer->LastWriteTime.QuadPart == -1) {

        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_WRITE );
        Buffer->LastWriteTime.QuadPart = 0;
    }

    if (Buffer->LastAccessTime.QuadPart == -1) {

        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_ACCESS );
        Buffer->LastAccessTime.QuadPart = 0;
    }

    if (Buffer->CreationTime.QuadPart == -1) {

        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_CREATION );
        Buffer->CreationTime.QuadPart = 0;
    }

    DirentBcb = NULL;

	originalBufferFileAttributes = Buffer->FileAttributes;

    Status = STATUS_SUCCESS;

    try {

        LARGE_INTEGER FatLocalDecThirtyOne1979;
        LARGE_INTEGER FatLocalJanOne1980;

        ExLocalTimeToSystemTime( &FatDecThirtyOne1979,
                                 &FatLocalDecThirtyOne1979 );

        ExLocalTimeToSystemTime( &FatJanOne1980,
                                 &FatLocalJanOne1980 );

        //
        //  Get a pointer to the dirent
        //

        ASSERT( Fcb->FcbCondition == FcbGood );
#if 0        
        FatGetDirentFromFcbOrDcb( IrpContext,
                                  Fcb,
                                  &Dirent,
                                  &DirentBcb );

		ASSERT( Dirent && DirentBcb );
#endif

        //
        //  Check if the user specified a non-zero creation time
        //

        if (FatData.ChicagoMode && (Buffer->CreationTime.QuadPart != 0)) {

            LargeCreationTime = Buffer->CreationTime;

            //
            //  Convert the Nt time to a Fat time
            //

            if ( !FatNtTimeToFatTime( IrpContext,
                                      &LargeCreationTime,
                                      FALSE,
                                      &CreationTime,
                                      &CreationMSec )) {

                //
                //  Special case the value 12/31/79 and treat this as 1/1/80.
                //  This '79 value can happen because of time zone issues.
                //

                if ((LargeCreationTime.QuadPart >= FatLocalDecThirtyOne1979.QuadPart) &&
                    (LargeCreationTime.QuadPart < FatLocalJanOne1980.QuadPart)) {

                    CreationTime = FatTimeJanOne1980;
                    LargeCreationTime = FatLocalJanOne1980;

                } else {

                    DebugTrace(0, Dbg, "Invalid CreationTime\n", 0);
                    try_return( Status = STATUS_INVALID_PARAMETER );
                }

                //
                //  Don't worry about CreationMSec
                //

                CreationMSec = 0;
            }

            ModifyCreation = TRUE;
        }

        //
        //  Check if the user specified a non-zero last access time
        //

        if (FatData.ChicagoMode && (Buffer->LastAccessTime.QuadPart != 0)) {

            LargeLastAccessTime = Buffer->LastAccessTime;

            //
            //  Convert the Nt time to a Fat time
            //

            if ( !FatNtTimeToFatTime( IrpContext,
                                      &LargeLastAccessTime,
                                      TRUE,
                                      &LastAccessTime,
                                      NULL )) {

                //
                //  Special case the value 12/31/79 and treat this as 1/1/80.
                //  This '79 value can happen because of time zone issues.
                //

                if ((LargeLastAccessTime.QuadPart >= FatLocalDecThirtyOne1979.QuadPart) &&
                    (LargeLastAccessTime.QuadPart < FatLocalJanOne1980.QuadPart)) {

                    LastAccessTime = FatTimeJanOne1980;
                    LargeLastAccessTime = FatLocalJanOne1980;

                } else {

                    DebugTrace(0, Dbg, "Invalid LastAccessTime\n", 0);
                    try_return( Status = STATUS_INVALID_PARAMETER );
                }
            }

            LastAccessDate = LastAccessTime.Date;
            ModifyLastAccess = TRUE;
        }

        //
        //  Check if the user specified a non-zero last write time
        //

        if (Buffer->LastWriteTime.QuadPart != 0) {

            //
            //  First do a quick check here if the this time is the same
            //  time as LastAccessTime.
            //

            if (ModifyLastAccess &&
                (Buffer->LastWriteTime.QuadPart == Buffer->LastAccessTime.QuadPart)) {

                ModifyLastWrite = TRUE;
                LastWriteTime = LastAccessTime;
                LargeLastWriteTime = LargeLastAccessTime;

            } else {

                LargeLastWriteTime = Buffer->LastWriteTime;

                //
                //  Convert the Nt time to a Fat time
                //

                if ( !FatNtTimeToFatTime( IrpContext,
                                          &LargeLastWriteTime,
                                          TRUE,
                                          &LastWriteTime,
                                          NULL )) {


                    //
                    //  Special case the value 12/31/79 and treat this as 1/1/80.
                    //  This '79 value can happen because of time zone issues.
                    //

                    if ((LargeLastWriteTime.QuadPart >= FatLocalDecThirtyOne1979.QuadPart) &&
                        (LargeLastWriteTime.QuadPart < FatLocalJanOne1980.QuadPart)) {

                        LastWriteTime = FatTimeJanOne1980;
                        LargeLastWriteTime = FatLocalJanOne1980;

                    } else {

                        DebugTrace(0, Dbg, "Invalid LastWriteTime\n", 0);
                        try_return( Status = STATUS_INVALID_PARAMETER );
                    }
                }

                ModifyLastWrite = TRUE;
            }
        }


        //
        //  Check if the user specified a non zero file attributes byte
        //

        if (Buffer->FileAttributes != 0) {

            //
            //  Only permit the attributes that FAT understands.  The rest are silently
            //  dropped on the floor.
            //

            Attributes = (UCHAR)(Buffer->FileAttributes & (FILE_ATTRIBUTE_READONLY |
                                                           FILE_ATTRIBUTE_HIDDEN |
                                                           FILE_ATTRIBUTE_SYSTEM |
                                                           FILE_ATTRIBUTE_DIRECTORY |
                                                           FILE_ATTRIBUTE_ARCHIVE));

            //
            //  Make sure that for a file the directory bit is not set
            //  and that for a directory the bit is set.
            //

            if (NodeType(Fcb) == FAT_NTC_FCB) {

                if (FlagOn(Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY)) {

                    DebugTrace(0, Dbg, "Attempt to set dir attribute on file\n", 0);
                    try_return( Status = STATUS_INVALID_PARAMETER );
                }

            } else {

                Attributes |= FAT_DIRENT_ATTR_DIRECTORY;
            }

            //
            //  Mark the FcbState temporary flag correctly.
            //

            if (FlagOn(Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY)) {

                //
                //  Don't allow the temporary bit to be set on directories.
                //

                if (NodeType(Fcb) == FAT_NTC_DCB) {

                    DebugTrace(0, Dbg, "No temporary directories\n", 0);
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
#if 0
            Dirent->Attributes = Attributes;
#endif
            NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
        }

        if ( ModifyCreation ) {

            //
            //  Set the new last write time in the dirent, and mark
            //  the bcb dirty
            //

            Fcb->CreationTime = LargeCreationTime;
#if 0
			Dirent->CreationTime = CreationTime;
            Dirent->CreationMSec = CreationMSec;
#endif

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
#if 0
			Dirent->LastAccessDate = LastAccessDate;
#endif
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
#if 0
			Dirent->LastWriteTime = LastWriteTime;
#endif
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
#if 0
                Dirent->FileSize = Fcb->Header.FileSize.LowPart;

                Dirent->FirstClusterOfFile = (USHORT)Fcb->FirstClusterOfFile;

                if (FatIsFat32(Fcb->Vcb)) {

                    Dirent->FirstClusterOfFileHi =
                            (USHORT)(Fcb->FirstClusterOfFile >> 16);
                }
#endif
            }

            FatNotifyReportChange( IrpContext,
                                   Fcb->Vcb,
                                   Fcb,
                                   NotifyFilter,
                                   FILE_ACTION_MODIFIED );
#if 0
            FatSetDirtyBcb( IrpContext, DirentBcb, Fcb->Vcb, TRUE );
#endif
        }

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->Secondary->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  0 );

		if(secondaryRequest == NULL) {

			FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
										NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_INFORMATION, 
										0 );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, Ccb->PrimaryFileHandle );

		RtlZeroMemory( &setFile, sizeof(setFile) );

		setFile.FileInformationClass	= irpSp->Parameters.SetFile.FileInformationClass;
		setFile.FileObject				= irpSp->Parameters.SetFile.FileObject;
		setFile.Length					= irpSp->Parameters.SetFile.Length;

		setFile.ReplaceIfExists			= irpSp->Parameters.SetFile.ReplaceIfExists;
		setFile.AdvanceOnly				= irpSp->Parameters.SetFile.AdvanceOnly;
		setFile.ClusterCount			= irpSp->Parameters.SetFile.ClusterCount;
		setFile.DeleteHandle			= irpSp->Parameters.SetFile.DeleteHandle;


		ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
		ndfsWinxpRequestHeader->SetFile.Length					= setFile.Length;
		ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;

		ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
		ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;

#if defined(_WIN64)
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U64)setFile.DeleteHandle;
#else
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U32)setFile.DeleteHandle;
#endif

		ndfsWinxpRequestHeader->SetFile.BasicInformation.CreationTime   = Buffer->CreationTime.QuadPart;
		ndfsWinxpRequestHeader->SetFile.BasicInformation.LastAccessTime = Buffer->LastAccessTime.QuadPart;
		ndfsWinxpRequestHeader->SetFile.BasicInformation.LastWriteTime  = Buffer->LastWriteTime.QuadPart;
		ndfsWinxpRequestHeader->SetFile.BasicInformation.ChangeTime     = Buffer->ChangeTime.QuadPart;
		ndfsWinxpRequestHeader->SetFile.BasicInformation.FileAttributes = originalBufferFileAttributes;

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDFAT_TIME_OUT;		
		Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if(Status != STATUS_SUCCESS) {

			ASSERT( NDFAT_BUG );
			secondaryRequest = NULL;
			try_return( Status = STATUS_IO_DEVICE_ERROR );	
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

#if 0
		if (NT_SUCCESS(ndfsWinxpReplytHeader->Status)) {

			ASSERT( (Fcb->Info.FileAttributes & ~FILE_ATTRIBUTE_ARCHIVE) 
					== (ndfsWinxpReplytHeader->DuplicatedInformation.FileAttributes & ~FILE_ATTRIBUTE_ARCHIVE) );

			Fcb->Info.AllocatedLength		= ndfsWinxpReplytHeader->DuplicatedInformation.AllocatedLength;
			Fcb->Info.CreationTime			= ndfsWinxpReplytHeader->DuplicatedInformation.CreationTime;
			Fcb->Info.FileAttributes		= ndfsWinxpReplytHeader->DuplicatedInformation.FileAttributes;
			Fcb->Info.FileSize				= ndfsWinxpReplytHeader->DuplicatedInformation.FileSize;
			Fcb->Info.LastAccessTime		= ndfsWinxpReplytHeader->DuplicatedInformation.LastAccessTime;
			Fcb->Info.LastChangeTime		= ndfsWinxpReplytHeader->DuplicatedInformation.LastChangeTime;
			Fcb->Info.LastModificationTime	= ndfsWinxpReplytHeader->DuplicatedInformation.LastModificationTime;
			Fcb->Info. ReparsePointTag		= ndfsWinxpReplytHeader->DuplicatedInformation.ReparsePointTag;
		}
#endif

		Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		//ASSERT( ndfsWinxpReplytHeader->Information == 0);
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;


    try_exit: NOTHING;
    } finally {

        DebugUnwind( FatSetBasicInfo );

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );		
		}

#if 0
        FatUnpinBcb( IrpContext, DirentBcb );
#endif
        DebugTrace(-1, Dbg, "FatSetBasicInfo -> %08lx\n", Status);
    }

    return Status;
}

NTSTATUS
NdFatSetDispositionInfo (
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

#if 1
	NTSTATUS					Status = STATUS_SUCCESS;

	PCCB						Ccb = FileObject->FsContext2;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct SetFile				setFile;
#endif


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
#if 0
            if ( !FatIsDirectoryEmpty(IrpContext, Fcb) ) {

                DebugTrace(-1, Dbg, "Directory is not empty\n", 0);

                return STATUS_DIRECTORY_NOT_EMPTY;
            }
#endif
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
#if 0
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
#endif
        }

        //
        //  At this point either we have a file or an empty directory
        //  so we know the delete can proceed.
        //
#if 0
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

#endif

	} else {

        //
        //  The user doesn't want to delete the file so clear
        //  the delete on close bit
        //

        DebugTrace(0, Dbg, "User want to not delete file\n", 0);

#if 0
        ClearFlag( Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE );
        FileObject->DeletePending = FALSE;
#endif
    }

	try {
	
		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->Secondary->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		if (FlagOn(Ccb->NdFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(Ccb->NdFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			try_return( Status = STATUS_FILE_CORRUPT_ERROR );
		}

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  0 );

		if(secondaryRequest == NULL) {

			FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
										NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_INFORMATION, 
										0 );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, Ccb->PrimaryFileHandle );

		RtlZeroMemory( &setFile, sizeof(setFile) );

		setFile.FileInformationClass	= irpSp->Parameters.SetFile.FileInformationClass;
		setFile.FileObject				= irpSp->Parameters.SetFile.FileObject;
		setFile.Length					= irpSp->Parameters.SetFile.Length;

		setFile.ReplaceIfExists			= irpSp->Parameters.SetFile.ReplaceIfExists;
		setFile.AdvanceOnly				= irpSp->Parameters.SetFile.AdvanceOnly;
		setFile.ClusterCount			= irpSp->Parameters.SetFile.ClusterCount;
		setFile.DeleteHandle			= irpSp->Parameters.SetFile.DeleteHandle;


		ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;
		ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
		ndfsWinxpRequestHeader->SetFile.Length					= setFile.Length;

		ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
		ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;
#if defined(_WIN64)
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U64)setFile.DeleteHandle;
#else
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U32)setFile.DeleteHandle;
#endif

		ndfsWinxpRequestHeader->SetFile.DispositionInformation.DeleteFile = Buffer->DeleteFile;

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDFAT_TIME_OUT;		
		Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (Status != STATUS_SUCCESS) {
	
			ASSERT( NDFAT_BUG );
			secondaryRequest = NULL;
			try_return( Status = STATUS_IO_DEVICE_ERROR );	
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );

			DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		//ASSERT( ndfsWinxpReplytHeader->Information == 0);
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

		ASSERT( NT_SUCCESS(Status) || Status == STATUS_CANNOT_DELETE || Status == STATUS_DIRECTORY_NOT_EMPTY );

		if (NT_SUCCESS(Status)) {

			if (Buffer->DeleteFile) {
				
				SetFlag( Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE );
				FileObject->DeletePending = TRUE;

			} else {

				ClearFlag( Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE );
				FileObject->DeletePending = FALSE;
			}
		}


try_exit: NOTHING;

	} finally {

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );		
		}
	}

    DebugTrace(-1, Dbg, "FatSetDispositionInfo -> STATUS_SUCCESS\n", 0);

#if 0
    return STATUS_SUCCESS;
#else
	return Status;
#endif
}

NTSTATUS
NdFatSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN PCCB Ccb
    )
{
	NTSTATUS					Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION			IrpSp = IoGetCurrentIrpStackLocation( Irp );

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	struct SetFile				setFile;

	PFILE_RENAME_INFORMATION	renameInformation = IrpContext->OriginatingIrp->AssociatedIrp.SystemBuffer;
	ULONG						inputBufferLength;
	PCCB						rootDirectoryCcb;


	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->Secondary->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		inputBufferLength = renameInformation->FileNameLength;

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  inputBufferLength );
		if(secondaryRequest == NULL) {

			FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
										NDFS_COMMAND_EXECUTE, 
										volDo->Secondary, 
										IRP_MJ_SET_INFORMATION, 
										inputBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, IrpSp, Ccb->PrimaryFileHandle );

		RtlZeroMemory( &setFile, sizeof(setFile) );

		setFile.FileInformationClass	= IrpSp->Parameters.SetFile.FileInformationClass;
		setFile.FileObject				= IrpSp->Parameters.SetFile.FileObject;
		setFile.Length					= IrpSp->Parameters.SetFile.Length;

		setFile.ReplaceIfExists			= IrpSp->Parameters.SetFile.ReplaceIfExists;
		setFile.AdvanceOnly				= IrpSp->Parameters.SetFile.AdvanceOnly;
		setFile.ClusterCount			= IrpSp->Parameters.SetFile.ClusterCount;
		setFile.DeleteHandle			= IrpSp->Parameters.SetFile.DeleteHandle;


		ndfsWinxpRequestHeader->SetFile.FileInformationClass = setFile.FileInformationClass;

		if (setFile.FileObject == NULL) {

			ndfsWinxpRequestHeader->SetFile.FileHandle = 0;
		
		} else {

			PCCB	setFileCcb  = setFile.FileObject->FsContext2; 

			if (!IS_SECONDARY_FILEOBJECT(setFile.FileObject)) {

				ASSERT( NDFAT_BUG );
				Status = STATUS_INVALID_PARAMETER;
				leave;
			}

			if (FlagOn(setFileCcb->NdFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

				ASSERT( FlagOn(setFileCcb->NdFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
				Status = STATUS_OBJECT_PATH_NOT_FOUND;
				leave;
			}

			ndfsWinxpRequestHeader->SetFile.FileHandle = setFileCcb->PrimaryFileHandle;
		} 

		ndfsWinxpRequestHeader->SetFile.Length			= setFile.Length;
		
		ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
		ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;
#if defined(_WIN64)
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U64)setFile.DeleteHandle;
#else
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U32)setFile.DeleteHandle;
#endif

		ASSERT( sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + renameInformation->FileNameLength <= setFile.Length );

		ASSERT( ndfsWinxpRequestHeader->SetFile.ReplaceIfExists == setFile.ReplaceIfExists );
		ASSERT( ndfsWinxpRequestHeader->SetFile.AdvanceOnly		== setFile.AdvanceOnly );
		ASSERT( ndfsWinxpRequestHeader->SetFile.ClusterCount	== setFile.ClusterCount );
#if defined(_WIN64)
		ASSERT( ndfsWinxpRequestHeader->SetFile.DeleteHandle	== (_U64)setFile.DeleteHandle );
#else
		ASSERT( ndfsWinxpRequestHeader->SetFile.DeleteHandle	== (_U32)setFile.DeleteHandle );
#endif

		DebugTrace2( 0, Dbg2, ("FileRenameInformation: renameInformation->FileName = %ws\n", renameInformation->FileName) );
		PrintIrp( Dbg, NULL, NULL, Irp );

		ndfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists = renameInformation->ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength  = renameInformation->FileNameLength;

		if(renameInformation->RootDirectory == NULL) {

			DebugTrace2( 0, Dbg, ("RedirectIrp: FileRenameInformation: No RootDirectory\n") );
			ndfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle = 0;
		
		} else {

			PFILE_OBJECT	rootDirectoryFileObject;

			Status = ObReferenceObjectByHandle( renameInformation->RootDirectory,
												FILE_READ_DATA,
												0,
												KernelMode,
												&rootDirectoryFileObject,
												NULL );

			if(Status != STATUS_SUCCESS) {

				ASSERT( FALSE );
				leave;
			}
	
			ObDereferenceObject( rootDirectoryFileObject );

			if (!IS_SECONDARY_FILEOBJECT(rootDirectoryFileObject)) {

				ASSERT( FALSE );
				Status = STATUS_INVALID_PARAMETER;
				leave;
			}
		
			rootDirectoryCcb = rootDirectoryFileObject->FsContext2;;

			ndfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle = rootDirectoryCcb->PrimaryFileHandle;
		}

		if (renameInformation->FileNameLength) {

			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
			RtlCopyMemory( ndfsWinxpRequestData, renameInformation->FileName, renameInformation->FileNameLength );
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(volDo->Secondary, secondaryRequest);

		timeOut.QuadPart = -NDFAT_TIME_OUT;		
		Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if (Status != STATUS_SUCCESS) {
	
			ASSERT( NDFAT_BUG );
			secondaryRequest = NULL;
			Status = STATUS_IO_DEVICE_ERROR;	
			leave;
		}

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

		if (NT_SUCCESS(Status) && setFile.FileObject) {

			UNICODE_STRING	fullPathName;
			WCHAR			fullPathNameBuffer[NDFS_MAX_PATH];
			NTSTATUS		appendStatus;

			RtlInitEmptyUnicodeString( &fullPathName, fullPathNameBuffer, sizeof(fullPathNameBuffer) );

			if ((appendStatus = Secondary_MakeFullPathName( setFile.FileObject, &setFile.FileObject->FileName, &fullPathName)) != STATUS_SUCCESS) {

				FatRaiseStatus( IrpContext, appendStatus );
			}

			if (Fcb->Header.NodeTypeCode == FAT_NTC_DCB && fullPathName.Length > 2) {

				if ((appendStatus = RtlAppendUnicodeToString(&fullPathName, L"\\")) != STATUS_SUCCESS) {

					FatRaiseStatus( IrpContext, appendStatus );
				}
			}

			DebugTrace2( 0, Dbg2, ("NdNtfsSetRenameInfo: fullpathName = %Z\n", &fullPathName) );
			Secondary_ChangeFcbFileName( IrpContext, Fcb, &fullPathName );
		}
	
	} finally {

        DebugUnwind( FatSetRenameInfo );

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );		
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdFatSetPositionInfo (
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


NTSTATUS
NdFatSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb,
    IN PFILE_OBJECT FileObject
    )
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

#if 1
	PCCB						Ccb = FileObject->FsContext2;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct SetFile				setFile;
#endif


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

        if (NewAllocationSize > Fcb->Header.AllocationSize.LowPart) {
#if 0
            FatAddFileAllocation( IrpContext, Fcb, FileObject, NewAllocationSize);
#endif
        } else {

            //
            //  Check here if we will be decreasing file size and synchonize with
            //  paging IO.
            //

            if ( Fcb->Header.FileSize.LowPart > NewAllocationSize ) {

                //
                //  Before we actually truncate, check to see if the purge
                //  is going to fail.
                //

                if (!MmCanFileBeTruncated( FileObject->SectionObjectPointer,
                                           &Buffer->AllocationSize )) {

                    try_return( Status = STATUS_USER_MAPPED_FILE );
                }

#if 0

				FileSizeTruncated = TRUE;
#endif

				OriginalFileSize = Fcb->Header.FileSize.LowPart;
                OriginalValidDataLength = Fcb->Header.ValidDataLength.LowPart;
                OriginalValidDataToDisk = Fcb->ValidDataToDisk;

                (VOID)ExAcquireResourceExclusiveLite( Fcb->Header.PagingIoResource, TRUE );
                ResourceAcquired = TRUE;
#if 0
                Fcb->Header.FileSize.LowPart = NewAllocationSize;

                //
                //  If we reduced the file size to less than the ValidDataLength,
                //  adjust the VDL.  Likewise ValidDataToDisk.
                //

                if (Fcb->Header.ValidDataLength.LowPart > Fcb->Header.FileSize.LowPart) {

                    Fcb->Header.ValidDataLength.LowPart = Fcb->Header.FileSize.LowPart;
                }
                if (Fcb->ValidDataToDisk > Fcb->Header.FileSize.LowPart) {

                    Fcb->ValidDataToDisk = Fcb->Header.FileSize.LowPart;
                }
#endif
            }

			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->Secondary->SessionResource, 
														 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
			}

			if (FlagOn(Ccb->NdFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

				ASSERT( FlagOn(Ccb->NdFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
				try_return( Status = STATUS_FILE_CORRUPT_ERROR );
			}

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
															  IRP_MJ_SET_INFORMATION,
															  0 );

			if(secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
											NDFS_COMMAND_EXECUTE, 
											volDo->Secondary, 
											IRP_MJ_SET_INFORMATION, 
											0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
			INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, Ccb->PrimaryFileHandle );

			RtlZeroMemory( &setFile, sizeof(setFile) );

			setFile.FileInformationClass	= irpSp->Parameters.SetFile.FileInformationClass;
			setFile.FileObject				= irpSp->Parameters.SetFile.FileObject;
			setFile.Length					= irpSp->Parameters.SetFile.Length;

			setFile.ReplaceIfExists			= irpSp->Parameters.SetFile.ReplaceIfExists;
			setFile.AdvanceOnly				= irpSp->Parameters.SetFile.AdvanceOnly;
			setFile.ClusterCount			= irpSp->Parameters.SetFile.ClusterCount;
			setFile.DeleteHandle			= irpSp->Parameters.SetFile.DeleteHandle;


			ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;
			ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
			ndfsWinxpRequestHeader->SetFile.Length					= setFile.Length;

			ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
			ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
			ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;
#if defined(_WIN64)
			ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U64)setFile.DeleteHandle;
#else
			ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U32)setFile.DeleteHandle;
#endif

			ndfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize  = NewAllocationSize;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDFAT_TIME_OUT;		
			Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (Status != STATUS_SUCCESS) {
	
				ASSERT( NDFAT_BUG );
				secondaryRequest = NULL;
				try_return( Status = STATUS_IO_DEVICE_ERROR );	
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT);
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
			//ASSERT( ndfsWinxpReplytHeader->Information == 0);
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

			ASSERT( NT_SUCCESS(Status) );

			if (NT_SUCCESS(Status) && ndfsWinxpReplytHeader->AllocationSize != (UINT64)Fcb->Header.AllocationSize.QuadPart) {

				PNDFS_MCB_ENTRY	mcbEntry;
				ULONG			index;

				BOOLEAN			lookupResut;
				VBO				vcn;
				LBO				lcn;
				//LCN				startingLcn;
				ULONG			clusterCount;

				ASSERT( ExIsResourceAcquiredExclusiveLite(Fcb->Header.Resource) );

				mcbEntry = (PNDFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

				for (index=0, vcn=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++, mcbEntry++) {

					lookupResut = FatLookupMcbEntry( IrpContext->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
					
					if (lookupResut == TRUE && vcn < Fcb->Header.AllocationSize.QuadPart) {

						if (vcn < ndfsWinxpReplytHeader->AllocationSize) {
							
							//ASSERT( startingLcn == lcn );
							ASSERT( vcn == mcbEntry->Vcn );
							ASSERT( lcn == mcbEntry->Lcn );
							ASSERT( clusterCount <= mcbEntry->ClusterCount || 
									Fcb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize && (index+1) == ndfsWinxpReplytHeader->NumberOfMcbEntry );
							
							if (clusterCount < mcbEntry->ClusterCount) {

								FatAddMcbEntry ( IrpContext->Vcb, 
												 &Fcb->Mcb, 
												 (VBO)mcbEntry->Vcn, 
												 (LBO)mcbEntry->Lcn, 
												 (ULONG)mcbEntry->ClusterCount );
	
								lookupResut = FatLookupMcbEntry( IrpContext->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

								ASSERT( lookupResut == TRUE );
								//ASSERT( startingLcn == lcn );
								ASSERT( vcn == mcbEntry->Vcn );
								ASSERT( lcn == mcbEntry->Lcn );
								ASSERT( clusterCount == mcbEntry->ClusterCount );
							}
						}
					
					} else { 

						ASSERT( lookupResut == FALSE || lcn == 0 );

						FatAddMcbEntry ( IrpContext->Vcb, 
										 &Fcb->Mcb, 
										 (VBO)mcbEntry->Vcn, 
										 (LBO)mcbEntry->Lcn, 
										 (ULONG)mcbEntry->ClusterCount );
					}

					vcn += (ULONG)mcbEntry->ClusterCount;
				}

				ASSERT( vcn == ndfsWinxpReplytHeader->AllocationSize );

				if (Fcb->Header.AllocationSize.QuadPart < ndfsWinxpReplytHeader->AllocationSize) {

					ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClse );
					ASSERT( Fcb->Header.FileSize.QuadPart == ndfsWinxpReplytHeader->FileSize );

					Fcb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

					if (CcIsFileCached(FileObject)) {

						ASSERT( FileObject->SectionObjectPointer->SharedCacheMap != NULL );
						CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Fcb->Header.AllocationSize );
					}

					SetFlag( Fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );
				}

				if (Fcb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize) {

					Fcb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

					Fcb->Header.FileSize.QuadPart = ndfsWinxpReplytHeader->FileSize;

					DebugTrace2(0, Dbg, ("NtfsSetEndOfFileInfo scb->Header.FileSize.QuadPart = %I64x, scb->Header.ValidDataLength.QuadPart = %I64x\n", 
										 Fcb->Header.FileSize.QuadPart, Fcb->Header.ValidDataLength.QuadPart) );
			
					if (Fcb->Header.ValidDataLength.QuadPart > Fcb->Header.FileSize.QuadPart) {

						Fcb->Header.ValidDataLength.QuadPart = Fcb->Header.FileSize.QuadPart;

#ifdef __ND_FAT_DBG__
						//strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
						//Scb->LastUpdateLine = __LINE__; 
#endif				
					}

					if (Fcb->ValidDataToDisk > Fcb->Header.FileSize.LowPart) {

						Fcb->ValidDataToDisk = Fcb->Header.FileSize.LowPart;
					}

#if DBG
					{
						BOOLEAN			lookupResut;
						VBO				vcn;
						LBO				lcn;
						//LCN			startingLcn;
						ULONG			clusterCount;

						vcn = 0;

						while (1) {

							lookupResut = FatLookupMcbEntry( &volDo->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

							DebugTrace2( 0, Dbg, ("vcn = %x, lcn = %I64x, clusterCount = %x\n", vcn, lcn, clusterCount) );

							if (lookupResut == FALSE || lcn == 0)
								break;

							vcn += clusterCount;
						}

						//ASSERT( vcn == Fcb->Header.AllocationSize.QuadPart );
					}

#endif
					{
						VBO				vbo = Fcb->Header.AllocationSize.LowPart;
						LBO				lbo;
						ULONG			byteCount;

						while (FatLookupMcbEntry(&volDo->Vcb, &Fcb->Mcb, vbo, &lbo, &byteCount, NULL)) {

							FatRemoveMcbEntry( &volDo->Vcb, &Fcb->Mcb, vbo, byteCount );

							vbo += byteCount;

							if (vbo == 0) {

								ASSERT( FALSE );
								break;
							}
						}
					}

					//FatRemoveMcbEntry( &volDo->Vcb, &Fcb->Mcb, Fcb->Header.AllocationSize.LowPart, 0xFFFFFFFF );

					if (CcIsFileCached(FileObject)) {

						ASSERT( FileObject->SectionObjectPointer->SharedCacheMap != NULL );
						CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Fcb->Header.AllocationSize );
					}
				}
			}

#if DBG
			{
				BOOLEAN			lookupResut;
				VBO				vcn;
				LBO				lcn;
				//LCN			startingLcn;
				ULONG			clusterCount;

				vcn = 0;

				while (1) {

					lookupResut = FatLookupMcbEntry( &volDo->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
					if (lookupResut == FALSE || lcn == 0)
						break;

					vcn += clusterCount;
				}

				ASSERT( vcn == Fcb->Header.AllocationSize.QuadPart );
			}

#endif
            //
            //  Now that File Size is down, actually do the truncate.
            //
#if 0
            FatTruncateFileAllocation( IrpContext, Fcb, NewAllocationSize);
#endif
            //
            //  Now check if we needed to decrease the file size accordingly.
            //

            if ( FileSizeTruncated ) {

                //
                //  Tell the cache manager we reduced the file size.
                //  The call is unconditional, because MM always wants to know.
                //

#if DBG
                try {
#endif
                
                    CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Fcb->Header.AllocationSize );

#if DBG
                } except(FatBugCheckExceptionFilter( GetExceptionInformation() )) {

                      NOTHING;
                }
#endif

                ASSERT( FileObject->DeleteAccess || FileObject->WriteAccess );

                //
                //  There is no going back from this. If we run into problems updating
                //  the dirent we will have to live with the consequences. Not sending
                //  the notifies is likewise pretty benign compared to failing the entire
                //  operation and trying to back out everything, which could fail for the
                //  same reasons.
                //
                //  If you want a transacted filesystem, use NTFS ...
                //

                FileSizeTruncated = FALSE;
#if 0
                FatSetFileSizeInDirent( IrpContext, Fcb, NULL );
#endif
                //
                //  Report that we just reduced the file size.
                //

                FatNotifyReportChange( IrpContext,
                                       Fcb->Vcb,
                                       Fcb,
                                       FILE_NOTIFY_CHANGE_SIZE,
                                       FILE_ACTION_MODIFIED );
            }
        }

    try_exit: NOTHING;

    } finally {

        if ( AbnormalTermination() && FileSizeTruncated ) {

            Fcb->Header.FileSize.LowPart = OriginalFileSize;
            Fcb->Header.ValidDataLength.LowPart = OriginalValidDataLength;
            Fcb->ValidDataToDisk = OriginalValidDataToDisk;

            //
            //  Make sure Cc knows the right filesize.
            //

            if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                *CcGetFileSizePointer(FileObject) = Fcb->Header.FileSize;
            }

            ASSERT( Fcb->Header.FileSize.LowPart <= Fcb->Header.AllocationSize.LowPart );
        }

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );		
		}

        if (CacheMapInitialized) {

            CcUninitializeCacheMap( FileObject, NULL, NULL );
        }

        if (ResourceAcquired) {

            ExReleaseResourceLite( Fcb->Header.PagingIoResource );

        }
        
    }

    DebugTrace(-1, Dbg, "FatSetAllocationInfo -> %08lx\n", STATUS_SUCCESS);

    return Status;
}

NTSTATUS
NdFatSetEndOfFileInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PVCB Vcb,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine performs the set End of File information for fat.  It either
    completes the request or enqueues it off to the fsp.

Arguments:

    Irp - Supplies the irp being processed

    FileObject - Supplies the file object being processed

    Vcb - Supplies the Vcb being processed

    Fcb - Supplies the Fcb or Dcb being processed, already known not to
        be the root dcb

Return Value:

    NTSTATUS - The result of this operation if it completes without
               an exception.

--*/

{
    NTSTATUS Status;

    PFILE_END_OF_FILE_INFORMATION Buffer;

    ULONG NewFileSize;
    ULONG InitialFileSize;
    ULONG InitialValidDataLength;
    ULONG InitialValidDataToDisk;

    BOOLEAN CacheMapInitialized = FALSE;
    BOOLEAN UnwindFileSizes = FALSE;
    BOOLEAN ResourceAcquired = FALSE;

#if 1
	PCCB						Ccb = FileObject->FsContext2;
	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct SetFile				setFile;
#endif

    DebugTrace(+1, Dbg, "FatSetEndOfFileInfo...\n", 0);

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    try {

        //
        //  File Size changes are only allowed on a file and not a directory
        //

        if (NodeType(Fcb) != FAT_NTC_FCB) {

            DebugTrace(0, Dbg, "Cannot change size of a directory\n", 0);

            try_return( Status = STATUS_INVALID_DEVICE_REQUEST );
        }

        //
        //  Check that the new file size is legal
        //

        if (!FatIsIoRangeValid( Fcb->Vcb, Buffer->EndOfFile, 0 )) {

            DebugTrace(0, Dbg, "Illegal allocation size\n", 0);

            try_return( Status = STATUS_DISK_FULL );
        }

        NewFileSize = Buffer->EndOfFile.LowPart;

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

            if (FlagOn( FileObject->Flags, FO_CLEANUP_COMPLETE ))  {

                //
                //  This IRP has raced (and lost) with a close (=>cleanup)
                //  on the same fileobject.  We don't want to reinitialise the
                //  cachemap here now because we'll leak it (unless we do so &
                //  then tear it down again here,  which is too much of a change at
                //  this stage).   So we'll just say the file is closed - which
                //  is arguably the right thing to do anyway,  since a caller
                //  racing operations in this way is broken.  The only stumbling
                //  block is possibly filters - do they operate on cleaned
                //  up fileobjects?
                //

                FatRaiseStatus( IrpContext, STATUS_FILE_CLOSED);
            }

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
        //  Do a special case here for the lazy write of file sizes.
        //

        if (IoGetCurrentIrpStackLocation(Irp)->Parameters.SetFile.AdvanceOnly) {

            //
            //  Only attempt this if the file hasn't been "deleted on close" and
            //  this is a good FCB.
            //

            if (!IsFileDeleted( IrpContext, Fcb ) && (Fcb->FcbCondition == FcbGood)) {

                PDIRENT Dirent;
                PBCB DirentBcb;

                //
                //  Never have the dirent filesize larger than the fcb filesize
                //

                if (NewFileSize >= Fcb->Header.FileSize.LowPart) {

                    NewFileSize = Fcb->Header.FileSize.LowPart;
                }

                //
                //  Make sure we don't set anything higher than the alloc size.
                //

                ASSERT( NewFileSize <= Fcb->Header.AllocationSize.LowPart );

                //
                //  Only advance the file size, never reduce it with this call
                //
#if 0
                FatGetDirentFromFcbOrDcb( IrpContext,
                                          Fcb,
                                          &Dirent,
                                          &DirentBcb );

                ASSERT( Dirent && DirentBcb );

				try {

                    if ( NewFileSize > Dirent->FileSize ) {

                        Dirent->FileSize = NewFileSize;

                        FatSetDirtyBcb( IrpContext, DirentBcb, Fcb->Vcb, TRUE );

                        //
                        //  Report that we just changed the file size.
                        //

                        FatNotifyReportChange( IrpContext,
                                               Vcb,
                                               Fcb,
                                               FILE_NOTIFY_CHANGE_SIZE,
                                               FILE_ACTION_MODIFIED );
                    }

                } finally {

                    FatUnpinBcb( IrpContext, DirentBcb );
                }
#endif

            } else {

                DebugTrace(0, Dbg, "Cannot set size on deleted file.\n", 0);
            }

            try_return( Status = STATUS_SUCCESS );
        }

        //
        //  Check if the new file size is greater than the current
        //  allocation size.  If it is then we need to increase the
        //  allocation size.
        //

        if ( NewFileSize > Fcb->Header.AllocationSize.LowPart ) {

            //
            //  Change the file allocation
            //
#if 0
            FatAddFileAllocation( IrpContext, Fcb, FileObject, NewFileSize );
#endif
		}

        //
        //  At this point we have enough allocation for the file.
        //  So check if we are really changing the file size
        //

        if (Fcb->Header.FileSize.LowPart != NewFileSize) {

            if ( NewFileSize < Fcb->Header.FileSize.LowPart ) {

                //
                //  Before we actually truncate, check to see if the purge
                //  is going to fail.
                //

                if (!MmCanFileBeTruncated( FileObject->SectionObjectPointer,
                                           &Buffer->EndOfFile )) {

                    try_return( Status = STATUS_USER_MAPPED_FILE );
                }

                //
                //  This call is unconditional, because MM always wants to know.
                //  Also serialize here with paging io since we are truncating
                //  the file size.
                //

                ResourceAcquired =
                    ExAcquireResourceExclusiveLite( Fcb->Header.PagingIoResource, TRUE );
            }

            //
            //  Set the new file size
            //

            InitialFileSize = Fcb->Header.FileSize.LowPart;
            InitialValidDataLength = Fcb->Header.ValidDataLength.LowPart;
            InitialValidDataToDisk = Fcb->ValidDataToDisk;
            UnwindFileSizes = TRUE;

			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->Secondary->SessionResource, 
														 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {
	
				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
			}

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
															  IRP_MJ_SET_INFORMATION,
															  0 );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
											NDFS_COMMAND_EXECUTE, 
											volDo->Secondary, 
											IRP_MJ_SET_INFORMATION, 
											0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
			INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, Ccb->PrimaryFileHandle );

			RtlZeroMemory( &setFile, sizeof(setFile) );

			setFile.FileInformationClass	= irpSp->Parameters.SetFile.FileInformationClass;
			setFile.FileObject				= irpSp->Parameters.SetFile.FileObject;
			setFile.Length					= irpSp->Parameters.SetFile.Length;

			setFile.ReplaceIfExists			= irpSp->Parameters.SetFile.ReplaceIfExists;
			setFile.AdvanceOnly				= irpSp->Parameters.SetFile.AdvanceOnly;
			setFile.ClusterCount			= irpSp->Parameters.SetFile.ClusterCount;
			setFile.DeleteHandle			= irpSp->Parameters.SetFile.DeleteHandle;


			ASSERT( setFile.Length	== sizeof(FILE_END_OF_FILE_INFORMATION) );


			ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;
			ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
			ndfsWinxpRequestHeader->SetFile.Length					= setFile.Length;

			ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
			ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
			ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;
#if defined(_WIN64)
			ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U64)setFile.DeleteHandle;
#else
			ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U32)setFile.DeleteHandle;
#endif

			ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = NewFileSize;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDFAT_TIME_OUT;		
			Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if(Status != STATUS_SUCCESS) {
	
				ASSERT( NDFAT_BUG );
				secondaryRequest = NULL;
				try_return( Status = STATUS_IO_DEVICE_ERROR );	
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );

				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
			//ASSERT( ndfsWinxpReplytHeader->Information == 0);
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

			ASSERT( NT_SUCCESS(Status) );

			if (!NT_SUCCESS(Status)) {

				try_return( Status );
			
			} else {
				
				if (ndfsWinxpReplytHeader->AllocationSize != (UINT64)Fcb->Header.AllocationSize.QuadPart) {

					PNDFS_MCB_ENTRY	mcbEntry;
					ULONG			index;

					BOOLEAN			lookupResut;
					VBO				vcn;
					LBO				lcn;
					//LCN				startingLcn;
					ULONG			clusterCount;

					ASSERT( ExIsResourceAcquiredExclusiveLite(Fcb->Header.Resource) );

					mcbEntry = (PNDFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

					for (index=0, vcn=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++, mcbEntry++) {

						lookupResut = FatLookupMcbEntry( IrpContext->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
					
						if (lookupResut == TRUE && vcn < Fcb->Header.AllocationSize.QuadPart) {

							if (vcn < ndfsWinxpReplytHeader->AllocationSize) {
							
								//ASSERT( startingLcn == lcn );
								ASSERT( vcn == mcbEntry->Vcn );
								ASSERT( lcn == mcbEntry->Lcn );
								ASSERT( clusterCount <= mcbEntry->ClusterCount || 
										Fcb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize && (index+1) == ndfsWinxpReplytHeader->NumberOfMcbEntry );
							
								if (clusterCount < mcbEntry->ClusterCount) {

									FatAddMcbEntry ( IrpContext->Vcb, 
													 &Fcb->Mcb, 
													 (VBO)mcbEntry->Vcn, 
													 (LBO)mcbEntry->Lcn, 
													 (ULONG)mcbEntry->ClusterCount );
	
									lookupResut = FatLookupMcbEntry( IrpContext->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

									ASSERT( lookupResut == TRUE );
									//ASSERT( startingLcn == lcn );
									ASSERT( vcn == mcbEntry->Vcn );
									ASSERT( lcn == mcbEntry->Lcn );
									ASSERT( clusterCount == mcbEntry->ClusterCount );
								}
							}
					
						} else { 

							ASSERT( lookupResut == FALSE || lcn == 0 );

							FatAddMcbEntry ( IrpContext->Vcb, 
											 &Fcb->Mcb, 
											 (VBO)mcbEntry->Vcn, 
											 (LBO)mcbEntry->Lcn, 
											 (ULONG)mcbEntry->ClusterCount );
						}

						vcn += (ULONG)mcbEntry->ClusterCount;
					}

					ASSERT( vcn == ndfsWinxpReplytHeader->AllocationSize );

					if (Fcb->Header.AllocationSize.QuadPart < ndfsWinxpReplytHeader->AllocationSize) {

						//ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClse );

						Fcb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;
					}

					if (Fcb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize) {

						Fcb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

						Fcb->Header.FileSize.QuadPart = ndfsWinxpReplytHeader->FileSize;

						DebugTrace2( 0, Dbg, ("NtfsSetEndOfFileInfo scb->Header.FileSize.QuadPart = %I64x, scb->Header.ValidDataLength.QuadPart = %I64x\n", 
											 Fcb->Header.FileSize.QuadPart, Fcb->Header.ValidDataLength.QuadPart) );
			
						{
							VBO				vbo = Fcb->Header.AllocationSize.LowPart;
							LBO				lbo;
							ULONG			byteCount;

							while (FatLookupMcbEntry(&volDo->Vcb, &Fcb->Mcb, vbo, &lbo, &byteCount, NULL)) {

								FatRemoveMcbEntry( &volDo->Vcb, &Fcb->Mcb, vbo, byteCount );

								vbo += byteCount;

								if (vbo == 0) {

									ASSERT( FALSE );
									break;
								}
							}
						}
					}
				}

				ASSERT( NewFileSize == ndfsWinxpReplytHeader->FileSize );
				NewFileSize = (ULONG)ndfsWinxpReplytHeader->FileSize;
			}

#if DBG
			{
				BOOLEAN			lookupResut;
				VBO				vcn;
				LBO				lcn;
				//LCN			startingLcn;
				ULONG			clusterCount;

				vcn = 0;

				while (1) {

					lookupResut = FatLookupMcbEntry( &volDo->Vcb, &Fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
					if (lookupResut == FALSE || lcn == 0)
						break;

					vcn += clusterCount;
				}

				ASSERT( vcn == Fcb->Header.AllocationSize.QuadPart );
			}

#endif

            Fcb->Header.FileSize.LowPart = NewFileSize;

            //
            //  If we reduced the file size to less than the ValidDataLength,
            //  adjust the VDL.  Likewise ValidDataToDisk.
            //

            if (Fcb->Header.ValidDataLength.LowPart > NewFileSize) {

                Fcb->Header.ValidDataLength.LowPart = NewFileSize;
            }

            if (Fcb->ValidDataToDisk > NewFileSize) {

                Fcb->ValidDataToDisk = NewFileSize;
            }

            DebugTrace(0, Dbg, "New file size is 0x%08lx.\n", NewFileSize);

            //
            //  We must now update the cache mapping (benign if not cached).
            //

            CcSetFileSizes( FileObject,
                            (PCC_FILE_SIZES)&Fcb->Header.AllocationSize );

#if 0

            FatSetFileSizeInDirent( IrpContext, Fcb, NULL );

            //
            //  Report that we just changed the file size.
            //

            FatNotifyReportChange( IrpContext,
                                   Vcb,
                                   Fcb,
                                   FILE_NOTIFY_CHANGE_SIZE,
                                   FILE_ACTION_MODIFIED );

#endif

            //
            //  Mark the fact that the file will need to checked for
            //  truncation on cleanup.
            //

            SetFlag( Fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );
        }

		//
        //  Set this handle as having modified the file
        //

        FileObject->Flags |= FO_FILE_MODIFIED;

        //
        //  Set our return status to success
        //

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;

        FatUnpinRepinnedBcbs( IrpContext );

    } finally {

        DebugUnwind( FatSetEndOfFileInfo );

        if (AbnormalTermination() && UnwindFileSizes) {

            Fcb->Header.FileSize.LowPart = InitialFileSize;
            Fcb->Header.ValidDataLength.LowPart = InitialValidDataLength;
            Fcb->ValidDataToDisk = InitialValidDataToDisk;

            if (FileObject->SectionObjectPointer->SharedCacheMap != NULL) {

                *CcGetFileSizePointer(FileObject) = Fcb->Header.FileSize;
            }
        }

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );		
		}

        if (CacheMapInitialized) {

            CcUninitializeCacheMap( FileObject, NULL, NULL );
        }

        if ( ResourceAcquired ) {

            ExReleaseResourceLite( Fcb->Header.PagingIoResource );
        }

        DebugTrace(-1, Dbg, "FatSetEndOfFileInfo -> %08lx\n", Status);
    }

    return Status;
}



#endif