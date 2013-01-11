#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__

#define BugCheckFileId                   (NTFS_BUG_CHECK_FILEINFO)

#define Dbg                              (DEBUG_TRACE_FILEINFO)
#define Dbg2                             (DEBUG_INFO_FILEINFO)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')

#define TRAVERSE_MATCH              (0x00000001)
#define EXACT_CASE_MATCH            (0x00000002)
#define ACTIVELY_REMOVE_SOURCE_LINK (0x00000004)
#define REMOVE_SOURCE_LINK          (0x00000008)
#define REMOVE_TARGET_LINK          (0x00000010)
#define ADD_TARGET_LINK             (0x00000020)
#define REMOVE_TRAVERSE_LINK        (0x00000040)
#define REUSE_TRAVERSE_LINK         (0x00000080)
#define MOVE_TO_NEW_DIR             (0x00000100)
#define ADD_PRIMARY_LINK            (0x00000200)
#define OVERWRITE_SOURCE_LINK       (0x00000400)


NTSTATUS
NdasNtfsSetBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NdasNtfsSetDispositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NdasNtfsSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN OUT PBOOLEAN VcbAcquired
    );

NTSTATUS
NdasNtfsSetPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb
    );

NTSTATUS
NdasNtfsSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NdasNtfsSetEndOfFileInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb OPTIONAL,
    IN BOOLEAN VcbAcquired
    );

NTSTATUS
NdasNtfsSetValidDataLengthInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NdasNtfsSetShortNameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    );


NTSTATUS
NdasNtfsSecondaryCommonSetInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for set file information called by both the
    fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    FILE_INFORMATION_CLASS FileInformationClass;
    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN ReleaseScbPaging = FALSE;
    BOOLEAN LazyWriterCallback = FALSE;
    ULONG WaitState;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );
    ASSERT( FlagOn( IrpContext->TopLevelIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL ));

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonSetInformation\n") );
    DebugTrace( 0, Dbg, ("IrpContext           = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp                  = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("Length               = %08lx\n", IrpSp->Parameters.SetFile.Length) );
    DebugTrace( 0, Dbg, ("FileInformationClass = %08lx\n", IrpSp->Parameters.SetFile.FileInformationClass) );
    DebugTrace( 0, Dbg, ("FileObject           = %08lx\n", IrpSp->Parameters.SetFile.FileObject) );
    DebugTrace( 0, Dbg, ("ReplaceIfExists      = %08lx\n", IrpSp->Parameters.SetFile.ReplaceIfExists) );
    DebugTrace( 0, Dbg, ("Buffer               = %08lx\n", Irp->AssociatedIrp.SystemBuffer) );

    //
    //  Reference our input parameters to make things easier
    //

    FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

#if 1

	if (FlagOn(Ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED) &&
		!(FileInformationClass == FileEndOfFileInformation && IrpSp->Parameters.SetFile.AdvanceOnly)) {
		
		ASSERT( FlagOn(Ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );

		NtfsCompleteRequest( IrpContext, Irp, STATUS_FILE_CORRUPT_ERROR );

		DebugTrace( -1, Dbg, ("NtfsCommonDirectoryControl -> STATUS_FILE_CORRUPT_ERROR\n") );

		return STATUS_FILE_CORRUPT_ERROR;
	}

#endif

    //
    //  We can reject volume opens immediately.
    //

    if (TypeOfOpen == UserVolumeOpen ||
        TypeOfOpen == UnopenedFileObject ||
        TypeOfOpen == UserViewIndexOpen ||
        ((TypeOfOpen != UserFileOpen) &&
         (FileInformationClass == FileValidDataLengthInformation))) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsCommonSetInformation -> STATUS_INVALID_PARAMETER\n") );
        return STATUS_INVALID_PARAMETER;
    }

    if (NtfsIsVolumeReadOnly( Vcb )) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_MEDIA_WRITE_PROTECTED );

        DebugTrace( -1, Dbg, ("NtfsCommonSetInformation -> STATUS_MEDIA_WRITE_PROTECTED\n") );
        return STATUS_MEDIA_WRITE_PROTECTED;
    }

    try {

        //
        //  The typical path here is for the lazy writer callback.  Go ahead and
        //  remember this first.
        //

        if (FileInformationClass == FileEndOfFileInformation) {

            LazyWriterCallback = IrpSp->Parameters.SetFile.AdvanceOnly;
        }

        //
        //  Perform the oplock check for changes to allocation or EOF if called
        //  by the user.
        //

        if (!LazyWriterCallback &&
            ((FileInformationClass == FileEndOfFileInformation) ||
             (FileInformationClass == FileAllocationInformation) ||
             (FileInformationClass == FileValidDataLengthInformation)) &&
            (TypeOfOpen == UserFileOpen) &&
            !FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

            //
            //  We check whether we can proceed based on the state of the file oplocks.
            //  This call might block this request.
            //

            Status = FsRtlCheckOplock( &Scb->ScbType.Data.Oplock,
                                       Irp,
                                       IrpContext,
                                       NULL,
                                       NULL );

            if (Status != STATUS_SUCCESS) {

                try_return( NOTHING );
            }

            //
            //  Update the FastIoField.
            //

            NtfsAcquireFsrtlHeader( Scb );
            Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
            NtfsReleaseFsrtlHeader( Scb );
        }

        //
        //  If this call is for EOF then we need to acquire the Vcb if we may
        //  have to perform an update duplicate call.  Don't block waiting for
        //  the Vcb in the Valid data callback case.
        //  We don't want to block the lazy write threads in the clean checkpoint
        //  case.
        //

        switch (FileInformationClass) {

        case FileEndOfFileInformation:

            //
            //  If this is not a system file then we will need to update duplicate info.
            //

            if (!FlagOn( Fcb->FcbState, FCB_STATE_SYSTEM_FILE )) {

                WaitState = FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT );
                ClearFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );

                //
                //  Only acquire the Vcb for the Lazy writer if we know the file size in the Fcb
                //  is out of date or can compare the Scb with that in the Fcb.  An unsafe comparison
                //  is OK because if they are changing then someone else can do the work.
                //  We also want to update the duplicate information if the total allocated
                //  has changed and there are no user handles remaining to perform the update.
                //

                if (LazyWriterCallback) {

                    if ((FlagOn( Fcb->InfoFlags, FCB_INFO_CHANGED_FILE_SIZE ) ||
                         ((Scb->Header.FileSize.QuadPart != Fcb->Info.FileSize) &&
                          FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA ))) ||
                        (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK ) &&
                         (Scb->CleanupCount == 0) &&
                         (Scb->ValidDataToDisk >= Scb->Header.ValidDataLength.QuadPart) &&
                         (FlagOn( Fcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE ) ||
                          (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA ) &&
                           (Scb->TotalAllocated != Fcb->Info.AllocatedLength))))) {

                        //
                        //  Go ahead and try to acquire the Vcb without waiting.
                        //

                        if (NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE )) {

                            VcbAcquired = TRUE;

                        } else {

                            SetFlag( IrpContext->State, WaitState );

                            //
                            //  If we could not get the Vcb for any reason then return.  Let's
                            //  not block an essential thread waiting for the Vcb.  Typically
                            //  we will only be blocked during a clean checkpoint.  The Lazy
                            //  Writer will periodically come back and retry this call.
                            //

                            try_return( Status = STATUS_FILE_LOCK_CONFLICT );
                        }
                    }

                //
                //  Otherwise we always want to wait for the Vcb except if we were called from
                //  MM extending a section.  We will try to get this without waiting and test
                //  if called from MM if unsuccessful.
                //

                } else {

                    if (NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE )) {

                        VcbAcquired = TRUE;

                    } else if ((Scb->Header.PagingIoResource == NULL) ||
                               !NtfsIsExclusiveScbPagingIo( Scb )) {

                        SetFlag( IrpContext->State, WaitState );

                        NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
                        VcbAcquired = TRUE;
                    }
                }

                SetFlag( IrpContext->State, WaitState );
            }

            break;
        //
        //  Acquire the Vcb shared for changes to allocation or basic
        //  information.
        //

        case FileAllocationInformation:
        case FileBasicInformation:
        case FileDispositionInformation:

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
            VcbAcquired = TRUE;

            break;

        //
        //  If this is a rename or link operation then we need to make sure
        //  we have the user's context and acquire the Vcb.
        //

        case FileRenameInformation:
        case FileLinkInformation:

            if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_ALLOC_SECURITY )) {

                IrpContext->Union.SubjectContext = NtfsAllocatePool( PagedPool,
                                                                      sizeof( SECURITY_SUBJECT_CONTEXT ));

                SetFlag( IrpContext->State, IRP_CONTEXT_STATE_ALLOC_SECURITY );

                SeCaptureSubjectContext( IrpContext->Union.SubjectContext );
            }

            //  Fall thru

        //
        //  For the two above plus the shortname we might need the Vcb exclusive for either directories
        //  or possible deadlocks.
        //

        case FileShortNameInformation:

            if (IsDirectory( &Fcb->Info )) {

                SetFlag( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX );
            }

            if (FlagOn( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX )) {

                NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

            } else {

                NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
            }

            VcbAcquired = TRUE;

            break;

        default:

            NOTHING;
        }

        //
        //  The Lazy Writer must still synchronize with Eof to keep the
        //  stream sizes from changing.  This will be cleaned up when we
        //  complete.
        //

        if (LazyWriterCallback) {

            //
            //  Acquire either the paging io resource shared to serialize with
            //  the flush case where the main resource is acquired before IoAtEOF
            //

            if (Scb->Header.PagingIoResource != NULL) {

                ExAcquireResourceSharedLite( Scb->Header.PagingIoResource, TRUE );
                ReleaseScbPaging = TRUE;
            }

            FsRtlLockFsRtlHeader( &Scb->Header );
            IrpContext->CleanupStructure = Scb;

        //
        //  Anyone potentially shrinking/deleting allocation must get the paging I/O
        //  resource first.  Special cases are the rename path and SetBasicInfo.  The
        //  rename path to lock the mapped page writer out of this file for deadlock
        //  prevention.  SetBasicInfo since we may call WriteFileSizes and we
        //  don't want to bump up the file size on disk from the value in the Scb
        //  if a write to EOF is underway.
        //

        } else if ((Scb->Header.PagingIoResource != NULL) &&
                   ((FileInformationClass == FileEndOfFileInformation) ||
                    (FileInformationClass == FileAllocationInformation) ||
                    (FileInformationClass == FileRenameInformation) ||
                    (FileInformationClass == FileBasicInformation) ||
                    (FileInformationClass == FileLinkInformation) ||
                    (FileInformationClass == FileValidDataLengthInformation))) {

            NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
        }

        //
        //  Acquire exclusive access to the Fcb,  We use exclusive
        //  because it is probable that one of the subroutines
        //  that we call will need to monkey with file allocation,
        //  create/delete extra fcbs.  So we're willing to pay the
        //  cost of exclusive Fcb access.
        //

        NtfsAcquireExclusiveFcb( IrpContext, Fcb, Scb, 0 );

        //
        //  Make sure the Scb state test we're about to do is properly synchronized.
        //  There's no point in testing the SCB_STATE_VOLUME_DISMOUNTED flag below
        //  if the volume can still get dismounted below us during this operation.
        //

        ASSERT( NtfsIsExclusiveScb( Scb ) || NtfsIsSharedScb( Scb ) );

        //
        //  The lazy writer callback is the only caller who can get this far if the
        //  volume has been dismounted.  We know that there are no user handles or
        //  writeable file objects or dirty pages.  Make one last check to see
        //  if this stream is on a dismounted or locked volume. Note the
        //  vcb tests are unsafe
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_VOLUME_DISMOUNTED ) ||
            FlagOn( Vcb->VcbState, VCB_STATE_LOCK_IN_PROGRESS ) ||
           !(FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED ))) {

            NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
        }

        //
        //  Based on the information class we'll do different
        //  actions.  We will perform checks, when appropriate
        //  to insure that the requested operation is allowed.
        //

        switch (FileInformationClass) {

        case FileBasicInformation:

            Status = NdasNtfsSetBasicInfo( IrpContext, FileObject, Irp, Scb, Ccb );
            break;

        case FileDispositionInformation:

            Status = NdasNtfsSetDispositionInfo( IrpContext, FileObject, Irp, Scb, Ccb );
            break;

        case FileRenameInformation:

            Status = NdasNtfsSetRenameInfo( IrpContext, FileObject, Irp, Vcb, Scb, Ccb, &VcbAcquired );
            break;

        case FilePositionInformation:

            Status = NdasNtfsSetPositionInfo( IrpContext, FileObject, Irp, Scb );
            break;

        case FileLinkInformation:

#if 0
            Status = NdasNtfsSetLinkInfo( IrpContext, Irp, Vcb, Scb, Ccb, &VcbAcquired );
#else			
			ASSERT( FALSE );
			Status = STATUS_INVALID_PARAMETER;
#endif
            break;

        case FileAllocationInformation:

            if (TypeOfOpen == UserDirectoryOpen ||
                TypeOfOpen == UserViewIndexOpen) {

                Status = STATUS_INVALID_PARAMETER;

            } else {

                Status = NdasNtfsSetAllocationInfo( IrpContext, FileObject, Irp, Scb, Ccb );
            }

            break;

        case FileEndOfFileInformation:

            if (TypeOfOpen == UserDirectoryOpen ||
                TypeOfOpen == UserViewIndexOpen) {

                Status = STATUS_INVALID_PARAMETER;

            } else {

                Status = NdasNtfsSetEndOfFileInfo( IrpContext, FileObject, Irp, Scb, Ccb, VcbAcquired );
            }

            break;

        case FileValidDataLengthInformation:

            Status = NdasNtfsSetValidDataLengthInfo( IrpContext, Irp, Scb, Ccb );
            break;

        case FileShortNameInformation:

            //
            //  Disallow setshortname on the root - its meaningless anyway
            //

            if (Scb->Header.NodeTypeCode == NTFS_NTC_SCB_ROOT_INDEX) {
                Status = STATUS_INVALID_PARAMETER;
            } else {
#if 0
                Status = NdasNtfsSetShortNameInfo( IrpContext, FileObject, Irp, Vcb, Scb, Ccb );
#else				
				ASSERT( FALSE );
				Status = STATUS_INVALID_PARAMETER;
#endif
            }
            break;

        default:

            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        //  Abort transaction on error by raising.
        //

        if (Status != STATUS_PENDING) {

            NtfsCleanupTransaction( IrpContext, Status, FALSE );
        }

    try_exit:  NOTHING;
    } finally {

#if __NDAS_NTFS_DBG__
		if (!(AbnormalTermination() && 
			IrpContext->ExceptionStatus != STATUS_CANT_WAIT) &&
			IrpContext->ExceptionStatus != STATUS_FILE_DELETED)
#endif
        DebugUnwind( NtfsCommonSetInformation );

        //
        //  Release the paging io resource if acquired shared.
        //

        if (ReleaseScbPaging) {

            ExReleaseResourceLite( Scb->Header.PagingIoResource );
        }

        if (VcbAcquired) {

            NtfsReleaseVcb( IrpContext, Vcb );
        }

        DebugTrace( -1, Dbg, ("NtfsCommonSetInformation -> %08lx\n", Status) );
    }

    //
    //  Complete the request unless it is being done in the oplock
    //  package.
    //

    if (Status != STATUS_PENDING) {
        NtfsCompleteRequest( IrpContext, Irp, Status );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set basic information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this operation

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb;
    ULONG UsnReason = 0;
    ULONG NewCcbFlags = 0;

    PFILE_BASIC_INFORMATION Buffer;
    ULONG PreviousFileAttributes = Scb->Fcb->Info.FileAttributes;

    BOOLEAN LeaveChangeTime = BooleanFlagOn( Ccb->Flags, CCB_FLAG_USER_SET_LAST_CHANGE_TIME );

    LONGLONG CurrentTime;

#if 1

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	ULONG originalBufferFileAttributes;

	struct SetFile				setFile;

#endif

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetBasicInfo...\n") );

#if 1
    if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

        Status = NtfsPostRequest( IrpContext, Irp );

        DebugTrace( -1, Dbg, ("NtfsSetBasicInfo:  Can't wait\n") );
        return Status;
    }
#endif

    Fcb = Scb->Fcb;

    //
    //  Reference the system buffer containing the user specified basic
    //  information record
    //

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Remember the source info flags in the Ccb.
    //

    IrpContext->SourceInfo = Ccb->UsnSourceInfo;

    //
    //  If the user is specifying -1 for a field, that means
    //  we should leave that field unchanged, even if we might
    //  have otherwise set it ourselves.  We'll set the
    //  Ccb flag saying the user set the field so that we
    //  don't do our default updating.
    //
    //  We set the field to 0 then so we know not to actually
    //  set the field to the user-specified (and in this case,
    //  illegal) value.
    //

    if (Buffer->ChangeTime.QuadPart == -1) {

        SetFlag( NewCcbFlags, CCB_FLAG_USER_SET_LAST_CHANGE_TIME );
        Buffer->ChangeTime.QuadPart = 0;

        //
        //  This timestamp is special -- sometimes even this very
        //  function wants to update the ChangeTime, but if the
        //  user is asking us not to, we shouldn't.
        //

        LeaveChangeTime = TRUE;
    }

    if (Buffer->LastAccessTime.QuadPart == -1) {

        SetFlag( NewCcbFlags, CCB_FLAG_USER_SET_LAST_ACCESS_TIME );
        Buffer->LastAccessTime.QuadPart = 0;
    }

    if (Buffer->LastWriteTime.QuadPart == -1) {

        SetFlag( NewCcbFlags, CCB_FLAG_USER_SET_LAST_MOD_TIME );
        Buffer->LastWriteTime.QuadPart = 0;
    }

    if (Buffer->CreationTime.QuadPart == -1) {

        //
        //  We only set the creation time at creation time anyway (how
        //  appropriate), so we don't need to set a Ccb flag in this
        //  case.  In fact, there isn't even a Ccb flag to signify
        //  that the user set the creation time.
        //

        Buffer->CreationTime.QuadPart = 0;
    }

    //
    //  Do a quick check to see there are any illegal time stamps being set.
    //  Ntfs supports all values of Nt time as long as the uppermost bit
    //  isn't set.
    //

    if (FlagOn( Buffer->ChangeTime.HighPart, 0x80000000 ) ||
        FlagOn( Buffer->CreationTime.HighPart, 0x80000000 ) ||
        FlagOn( Buffer->LastAccessTime.HighPart, 0x80000000 ) ||
        FlagOn( Buffer->LastWriteTime.HighPart, 0x80000000 )) {

        DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", STATUS_INVALID_PARAMETER) );

        return STATUS_INVALID_PARAMETER;
    }

    NtfsGetCurrentTime( IrpContext, CurrentTime );

    //
    //  Pick up any changes from the fast Io path now while we have the
    //  file exclusive.
    //

#if 0
    NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
#endif

    //
    //  If the user specified a non-zero file attributes field then
    //  we need to change the file attributes.  This code uses the
    //  I/O supplied system buffer to modify the file attributes field
    //  before changing its value on the disk.
    //

#if 1
	originalBufferFileAttributes = Buffer->FileAttributes;
#endif

    if (Buffer->FileAttributes != 0) {

        //
        //  Check for valid flags being passed in.  We fail if this is
        //  a directory and the TEMPORARY bit is used.  Also fail if this
        //  is a file and the DIRECTORY bit is used.
        //

        if (Scb->AttributeTypeCode == $DATA) {

            if (FlagOn( Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY )) {

                DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", STATUS_INVALID_PARAMETER) );

                return STATUS_INVALID_PARAMETER;
            }

        } else if (IsDirectory( &Fcb->Info )) {

            if (FlagOn( Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY )) {

                DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", STATUS_INVALID_PARAMETER) );

                return STATUS_INVALID_PARAMETER;
            }
        }

        //
        //  Clear out the normal bit and the directory bit as well as any unsupported
        //  bits.
        //

        ClearFlag( Buffer->FileAttributes,
                   ~FILE_ATTRIBUTE_VALID_SET_FLAGS | FILE_ATTRIBUTE_NORMAL );

        //
        //  Update the attributes in the Fcb if this is a change to the file.
        //  We want to keep the flags that the user can't set.
        //

        Fcb->Info.FileAttributes = (Fcb->Info.FileAttributes & ~FILE_ATTRIBUTE_VALID_SET_FLAGS) |
                                   Buffer->FileAttributes;

        ASSERTMSG( "conflict with flush",
                   NtfsIsSharedFcb( Fcb ) ||
                   (Fcb->PagingIoResource != NULL &&
                    NtfsIsSharedFcbPagingIo( Fcb )) );

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_FILE_ATTR );

        //
        //  If this is the root directory then keep the hidden and system flags.
        //

#if 1
		if( NtfsSegmentNumber( &Fcb->FileReference ) == ROOT_FILE_NAME_INDEX_NUMBER ) { 
#else
        if (Fcb == Fcb->Vcb->RootIndexScb->Fcb) {
#endif

            SetFlag( Fcb->Info.FileAttributes, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN );

        //
        //  Mark the file object temporary flag correctly.
        //

        } else if (FlagOn(Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY)) {

            SetFlag( Scb->ScbState, SCB_STATE_TEMPORARY );
            SetFlag( FileObject->Flags, FO_TEMPORARY_FILE );

        } else {

            ClearFlag( Scb->ScbState, SCB_STATE_TEMPORARY );
            ClearFlag( FileObject->Flags, FO_TEMPORARY_FILE );
        }

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }

        //
        //  Post a Usn change if the file attribute change.
        //

        if (PreviousFileAttributes != Fcb->Info.FileAttributes) {

            UsnReason = USN_REASON_BASIC_INFO_CHANGE;
        }
    }

    //
    //  Propagate the new Ccb flags to the Ccb now that we know we won't fail.
    //

    SetFlag( Ccb->Flags, NewCcbFlags );

    //
    //  If the user specified a non-zero change time then change
    //  the change time on the record.  Then do the exact same
    //  for the last acces time, last write time, and creation time
    //

    if (Buffer->ChangeTime.QuadPart != 0) {

        if (Fcb->Info.LastChangeTime != Buffer->ChangeTime.QuadPart) {
            UsnReason = USN_REASON_BASIC_INFO_CHANGE;
        }

        Fcb->Info.LastChangeTime = Buffer->ChangeTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_CHANGE_TIME );

        LeaveChangeTime = TRUE;
    }

    if (Buffer->CreationTime.QuadPart != 0) {

        if (Fcb->Info.CreationTime != Buffer->CreationTime.QuadPart) {
            UsnReason = USN_REASON_BASIC_INFO_CHANGE;
        }

        Fcb->Info.CreationTime = Buffer->CreationTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_CREATE );

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    if (Buffer->LastAccessTime.QuadPart != 0) {

        if (Fcb->CurrentLastAccess != Buffer->LastAccessTime.QuadPart) {
            UsnReason = USN_REASON_BASIC_INFO_CHANGE;
        }

        Fcb->CurrentLastAccess = Fcb->Info.LastAccessTime = Buffer->LastAccessTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_ACCESS );
        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_ACCESS_TIME );

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    if (Buffer->LastWriteTime.QuadPart != 0) {

        if (Fcb->Info.LastModificationTime != Buffer->LastWriteTime.QuadPart) {
            UsnReason = USN_REASON_BASIC_INFO_CHANGE;
        }

        Fcb->Info.LastModificationTime = Buffer->LastWriteTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_MOD );
        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_MOD_TIME );

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    //
    //  Now indicate that we should not be updating the standard information attribute anymore
    //  on cleanup.
    //

    if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

        //
        //  Check if the index bit changed.
        //

        if (FlagOn( PreviousFileAttributes ^ Fcb->Info.FileAttributes,
                    FILE_ATTRIBUTE_NOT_CONTENT_INDEXED )) {

            SetFlag( UsnReason, USN_REASON_INDEXABLE_CHANGE );
        }

        //
        //  Post the change to the Usn Journal
        //

        if (UsnReason != 0) {

            NtfsPostUsnChange( IrpContext, Scb, UsnReason );
        }

#if 0
        NtfsUpdateStandardInformation( IrpContext, Fcb  );
#endif

        if (FlagOn( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE )) {

#if 0
            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &Scb->Header.ValidDataLength.QuadPart,
                                FALSE,
                                TRUE,
                                FALSE );
#endif

            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        }

        ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

        if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

#if 0
            NtfsCheckpointCurrentTransaction( IrpContext );
            NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
#endif
        }
    }

    Status = STATUS_SUCCESS;

#if 1

	try {
	
		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  0 );

		if(secondaryRequest == NULL) {

			NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
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

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if(Status != STATUS_SUCCESS) {

			ASSERT( NDASNTFS_BUG );
			secondaryRequest = NULL;
			try_return( Status = STATUS_IO_DEVICE_ERROR );	
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

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

		Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		//ASSERT( ndfsWinxpReplytHeader->Information == 0);
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

try_exit:  NOTHING;

	} finally {
	
		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}
	}

#endif

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetDispositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set disposition information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this handle

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PLCB Lcb;
    BOOLEAN GenerateOnClose = FALSE;
    PIO_STACK_LOCATION IrpSp;
    HANDLE FileHandle = NULL;

    PFILE_DISPOSITION_INFORMATION Buffer;

#if 1

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	struct SetFile				setFile;

#endif

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetDispositionInfo...\n") );

    //
    // First pull the file handle out of the irp
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FileHandle = IrpSp->Parameters.SetFile.DeleteHandle;

    //
    //  We get the Lcb for this open.  If there is no link then we can't
    //  set any disposition information if this is a file.
    //

    Lcb = Ccb->Lcb;

    if ((Lcb == NULL) &&
        FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        DebugTrace( -1, Dbg, ("NtfsSetDispositionInfo:  Exit -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Reference the system buffer containing the user specified disposition
    //  information record
    //

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    try {

#if 1

		if (IsDirectory(&Scb->Fcb->Info)) {
		
			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->SessionResource, 
														 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
			}

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_SET_INFORMATION,
															  0 );

			if(secondaryRequest == NULL) {

				NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
											NDFS_COMMAND_EXECUTE, 
											volDo->Secondary, 
											IRP_MJ_SET_INFORMATION, 
											0 );

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


			ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;

			if(setFile.FileObject) {

				PCCB	setFileCcb  = (PCCB)(setFile.FileObject->FsContext2); 

				if(setFileCcb == NULL || !FlagOn(((PSCB)(setFile.FileObject->FsContext))->Fcb->NdasNtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY)) {

					ASSERT(NDASNTFS_BUG);
					try_return( Status = STATUS_IO_DEVICE_ERROR );
				}

				if(FlagOn(setFileCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

					ASSERT( FlagOn(setFileCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );
					try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );
				}
				ndfsWinxpRequestHeader->SetFile.FileHandle = setFileCcb->PrimaryFileHandle;

			} else
				ndfsWinxpRequestHeader->SetFile.FileHandle = 0;

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

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if(Status != STATUS_SUCCESS) {

				ASSERT( NDASNTFS_BUG );
				secondaryRequest = NULL;
				try_return( Status = STATUS_IO_DEVICE_ERROR );	
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
				DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
			//ASSERT( ndfsWinxpReplytHeader->Information == 0);
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

			if (!NT_SUCCESS(Status))
				try_return( Status );
		}
		
#endif

        if (Buffer->DeleteFile) {

            //
            //  Check if the file is marked read only
            //

            if (IsReadOnly( &Scb->Fcb->Info )) {

                DebugTrace( 0, Dbg, ("File fat flags indicates read only\n") );

#if 1
				if (IsDirectory(&Scb->Fcb->Info))
					NDASNTFS_ASSERT( FALSE );
#endif
                try_return( Status = STATUS_CANNOT_DELETE );
            }

            //
            //  Make sure there is no process mapping this file as an image
            //

            if (!MmFlushImageSection( &Scb->NonpagedScb->SegmentObject,
                                      MmFlushForDelete )) {

                DebugTrace( 0, Dbg, ("Failed to flush image section\n") );

#if 1
				if (IsDirectory(&Scb->Fcb->Info))
					NDASNTFS_ASSERT( FALSE );
#endif
                try_return( Status = STATUS_CANNOT_DELETE );
            }

            //
            //  Check that we are not trying to delete one of the special
            //  system files.
            //

            if (FlagOn( Scb->Fcb->FcbState, FCB_STATE_SYSTEM_FILE )) {

                DebugTrace( 0, Dbg, ("Scb is one of the special system files\n") );

#if 1
				if (IsDirectory(&Scb->Fcb->Info))
					NDASNTFS_ASSERT( FALSE );
#endif
                try_return( Status = STATUS_CANNOT_DELETE );
            }

            //
            //  Only do the auditing if we have a user handle.  We verify that the FileHandle
            //  is still valid and hasn't gone through close.  Note we first check the CCB state
            //  to see if the cleanup has been issued.  If the CCB state is valid then we are
            //  guaranteed the handle couldn't have been reused by the object manager even if
            //  the user close in another thread has gone through OB.  This is because this request
            //  is serialized with Ntfs cleanup.
            //

            if (FileHandle != NULL) {

                //
                //  Check for serialization with Ntfs cleanup first.
                //

                if (FlagOn( Ccb->Flags, CCB_FLAG_CLEANUP )) {

                    DebugTrace( 0, Dbg, ("This call issued after cleanup\n") );
                    try_return( Status = STATUS_INVALID_HANDLE );
                }

                Status = ObQueryObjectAuditingByHandle( FileHandle,
                                                        &GenerateOnClose );

                //
                //  Fail the request if the object manager doesn't recognize the handle.
                //

                if (!NT_SUCCESS( Status )) {

                    DebugTrace( 0, Dbg, ("Object manager fails to recognize handle\n") );
#if 1
					NDASNTFS_ASSERT( FALSE );
#endif
                    try_return( Status );
                }
            }

            //
            //  Now check that the file is really deleteable according to indexsup
            //

            if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

#if 0
                BOOLEAN LastLink;
#endif
                BOOLEAN NonEmptyIndex = FALSE;

                //
                //  If the link is not deleted, we check if it can be deleted.
                //

                if (!LcbLinkIsDeleted( Lcb )) {

#if 1
					if (1) {
#else
                    if (NtfsIsLinkDeleteable( IrpContext, Scb->Fcb, &NonEmptyIndex, &LastLink )) {
#endif

                        //
                        //  It is ok to get rid of this guy.  All we need to do is
                        //  mark this Lcb for delete and decrement the link count
                        //  in the Fcb.  If this is a primary link, then we
                        //  indicate that the primary link has been deleted.
                        //

                        SetFlag( Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                        ASSERTMSG( "Link count should not be 0\n", Scb->Fcb->LinkCount != 0 );
                        Scb->Fcb->LinkCount -= 1;

                        if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

                            SetFlag( Scb->Fcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED );
                        }

                        //
                        //  Call into the notify package to close any handles on
                        //  a directory being deleted.
                        //

#if 0
                        if (IsDirectory( &Scb->Fcb->Info )) {

                            FsRtlNotifyFilterChangeDirectory( Scb->Vcb->NotifySync,
                                                              &Scb->Vcb->DirNotifyList,
                                                              FileObject->FsContext,
                                                              NULL,
                                                              FALSE,
                                                              FALSE,
                                                              0,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL );
                        }
#endif

                    } else if (NonEmptyIndex) {

                        DebugTrace( 0, Dbg, ("Index attribute has entries\n") );
#if 1
						if (IsDirectory(&Scb->Fcb->Info))
							NDASNTFS_ASSERT( FALSE );
#endif
                        try_return( Status = STATUS_DIRECTORY_NOT_EMPTY );

                    } else {

                        DebugTrace( 0, Dbg, ("File is not deleteable\n") );
#if 1
						if (IsDirectory(&Scb->Fcb->Info))
							NDASNTFS_ASSERT( FALSE );
#endif
                        try_return( Status = STATUS_CANNOT_DELETE );
                    }
                }

            //
            //  Otherwise we are simply removing the attribute.
            //

            } else {

                SetFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
            }

            //
            //  Indicate in the file object that a delete is pending
            //

            FileObject->DeletePending = TRUE;

            //
            //  Now do the audit.
            //

            if ((FileHandle != NULL) && GenerateOnClose) {

                SeDeleteObjectAuditAlarm( FileObject, FileHandle );
            }

        } else {

            if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

                if (LcbLinkIsDeleted( Lcb )) {

                    //
                    //  The user doesn't want to delete the link so clear any delete bits
                    //  we have laying around
                    //

                    DebugTrace( 0, Dbg, ("File is being marked as do not delete on close\n") );

                    ClearFlag( Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                    Scb->Fcb->LinkCount += 1;
                    ASSERTMSG( "Link count should not be 0\n", Scb->Fcb->LinkCount != 0 );

                    if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

                        ClearFlag( Scb->Fcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED );
                    }
                }

            //
            //  Otherwise we are undeleting an attribute.
            //

            } else {

                ClearFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
            }

            FileObject->DeletePending = FALSE;
        }

        Status = STATUS_SUCCESS;

#if 1

		if (!IsDirectory(&Scb->Fcb->Info)) {
		
			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->SessionResource, 
														 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
			}

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_SET_INFORMATION,
															  0 );

			if(secondaryRequest == NULL) {

				NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, 
											NDFS_COMMAND_EXECUTE, 
											volDo->Secondary, 
											IRP_MJ_SET_INFORMATION, 
											0 );

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


			ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;

			if (setFile.FileObject) {

				PCCB	setFileCcb  = (PCCB)(setFile.FileObject->FsContext2); 

				if(setFileCcb == NULL || !FlagOn(((PSCB)(setFile.FileObject->FsContext))->Fcb->NdasNtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY)) {

					ASSERT(NDASNTFS_BUG);
					try_return( Status = STATUS_IO_DEVICE_ERROR );
				}

				if(FlagOn(setFileCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

					ASSERT( FlagOn(setFileCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );
					try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );
				}
				ndfsWinxpRequestHeader->SetFile.FileHandle = setFileCcb->PrimaryFileHandle;

			} else
				ndfsWinxpRequestHeader->SetFile.FileHandle = 0;

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

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if(Status != STATUS_SUCCESS) {

				ASSERT( NDASNTFS_BUG );
				secondaryRequest = NULL;
				try_return( Status = STATUS_IO_DEVICE_ERROR );	
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
				DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
			//ASSERT( ndfsWinxpReplytHeader->Information == 0);
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

			ASSERT( NT_SUCCESS(Status) );
		}

#endif

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsSetDispositionInfo );

#if 1

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}
#endif

        NOTHING;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsSetDispositionInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN OUT PBOOLEAN VcbAcquired
    )

/*++

Routine Description:

    This routine performs the set rename function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Vcb - Supplies the Vcb for the Volume

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this file object

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PLCB Lcb = Ccb->Lcb;
    PFCB Fcb = Scb->Fcb;
    PSCB ParentScb;
    USHORT FcbLinkCountAdj = 0;

    BOOLEAN AcquiredParentScb = TRUE;
    BOOLEAN AcquiredObjectIdIndex = FALSE;
    BOOLEAN AcquiredReparsePointIndex = FALSE;

    PFCB TargetLinkFcb = NULL;
#if 0
    BOOLEAN ExistingTargetLinkFcb;
#endif
    BOOLEAN AcquiredTargetLinkFcb = FALSE;
    USHORT TargetLinkFcbCountAdj = 0;

    BOOLEAN AcquiredFcbTable = FALSE;
    PFCB FcbWithPagingToRelease = NULL;

    PFILE_OBJECT TargetFileObject;
#if 0
    PSCB TargetParentScb;
#endif

    UNICODE_STRING NewLinkName;
#if 0
    UNICODE_STRING NewFullLinkName;
#endif
    PWCHAR NewFullLinkNameBuffer = NULL;
#if 0
    UCHAR NewLinkNameFlags;
#endif

    PFILE_NAME FileNameAttr = NULL;
    USHORT FileNameAttrLength = 0;

#if 0
    UNICODE_STRING PrevLinkName;
#endif
    UNICODE_STRING PrevFullLinkName;
#if 0
    UCHAR PrevLinkNameFlags;
#endif

    UNICODE_STRING SourceFullLinkName;
#if 0
    USHORT SourceLinkLastNameOffset;
#endif

#if 0
    BOOLEAN FoundLink;
    PINDEX_ENTRY IndexEntry;
#endif
    PBCB IndexEntryBcb = NULL;
#if 0
    PWCHAR NextChar;
#endif

    BOOLEAN ReportDirNotify = FALSE;

    ULONG RenameFlags = ACTIVELY_REMOVE_SOURCE_LINK | REMOVE_SOURCE_LINK | ADD_TARGET_LINK;

    PLIST_ENTRY Links;
    PSCB ThisScb;

    PFCB_USN_RECORD SavedFcbUsnRecord = NULL;
    ULONG SavedUsnReason = 0;

    NAME_PAIR NamePair;
#if 0
    NTFS_TUNNELED_DATA TunneledData;
    ULONG TunneledDataSize;
#endif
    BOOLEAN HaveTunneledInformation = FALSE;
    PFCB LockedFcb = NULL;

#if 1
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
#endif


    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE ();

    DebugTrace( +1, Dbg, ("NtfsSetRenameInfo...\n") );

#if 1
	UNREFERENCED_PARAMETER( VcbAcquired );
#endif

    //
    //  See if we are doing a stream rename.  The allowed inputs are:
    //      No associated file object.
    //      Rename Name begins with a colon
    //  If so, perform the rename
    //

    TargetFileObject = IrpSp->Parameters.SetFile.FileObject;

    if (TargetFileObject == NULL) {
        PFILE_RENAME_INFORMATION FileRename;

#if 1
		NDASNTFS_ASSERT( FALSE );
#endif

        FileRename = IrpContext->OriginatingIrp->AssociatedIrp.SystemBuffer;

        if (FileRename->FileNameLength >= sizeof( WCHAR ) &&
            FileRename->FileName[0] == L':') {

            NewLinkName.Buffer = FileRename->FileName;
            NewLinkName.MaximumLength =
                NewLinkName.Length = (USHORT) FileRename->FileNameLength;

#if 0
            Status = NtfsStreamRename( IrpContext, FileObject, Fcb, Scb, Ccb, FileRename->ReplaceIfExists, &NewLinkName );
#endif
            DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit -> %08lx\n", Status) );
            return Status;
        }
    }

    //
    //  Do a quick check that the caller is allowed to do the rename.
    //  The opener must have opened the main data stream by name and this can't be
    //  a system file.
    //
#if 0

    if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE ) ||
        (Lcb == NULL) ||
        FlagOn(Fcb->FcbState, FCB_STATE_SYSTEM_FILE)) {

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  If this link has been deleted, then we don't allow this operation.
    //

    if (LcbLinkIsDeleted( Lcb )) {

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit -> %08lx\n", STATUS_ACCESS_DENIED) );
        return STATUS_ACCESS_DENIED;
    }
#endif

    //
    //  Verify that we can wait.
    //

    if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

        Status = NtfsPostRequest( IrpContext, Irp );

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Can't wait\n") );
        return Status;
    }

    //
    //  Remember the source info flags in the Ccb.
    //

    IrpContext->SourceInfo = Ccb->UsnSourceInfo;

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Initialize the local variables.
        //

        ParentScb = Lcb->Scb;
        NtfsInitializeNamePair( &NamePair );

        if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID ) &&
            (Vcb->NotifyCount != 0)) {

            ReportDirNotify = TRUE;
        }

        PrevFullLinkName.Buffer = NULL;
        SourceFullLinkName.Buffer = NULL;

#if 1

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		inputBufferLength = renameInformation->FileNameLength;

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  inputBufferLength );
		if(secondaryRequest == NULL) {

			NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
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

				ASSERT(NDASNTFS_BUG);
				Status = STATUS_INVALID_PARAMETER;
				leave;
			}

			if (FlagOn(setFileCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

				ASSERT( FlagOn(setFileCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );
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
		//ASSERT( ndfsWinxpRequestHeader->SetFile.DeleteHandle	== (_U32)setFile.DeleteHandle );

		DebugTrace( 0, Dbg2, ("FileRenameInformation: renameInformation->FileName = %ws\n", renameInformation->FileName) );
		PrintIrp( Dbg, NULL, NULL, Irp );

		ndfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists = renameInformation->ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength  = renameInformation->FileNameLength;

		if(renameInformation->RootDirectory == NULL) {

			DebugTrace( 0, Dbg, ("RedirectIrp: FileRenameInformation: No RootDirectory\n") );
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

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		if (Status != STATUS_SUCCESS) {
	
			ASSERT( NDASNTFS_BUG );
			secondaryRequest = NULL;
			Status = STATUS_IO_DEVICE_ERROR;	
			leave;
		}

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
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

				NtfsRaiseStatus( IrpContext, appendStatus, NULL, NULL );
			}

			if (IsDirectory(&Fcb->Info) && fullPathName.Length > 2) {

				if ((appendStatus = RtlAppendUnicodeToString(&fullPathName, L"\\")) != STATUS_SUCCESS) {

					NtfsRaiseStatus( IrpContext, appendStatus, NULL, NULL );
				}
			}

			DebugTrace( 0, Dbg2, ("NdasNtfsSetRenameInfo: fullpathName = %Z\n", &fullPathName) );
			Secondary_ChangeLcbFileName( IrpContext, Lcb, &fullPathName );
		}

		leave;

#endif

        //
        //  If this is a directory file, we need to examine its descendents.
        //  We may not remove a link which may be an ancestor path
        //  component of any open file.
        //

#if 0
        if (IsDirectory( &Fcb->Info )) {

            Status = NtfsCheckTreeForBatchOplocks( IrpContext, Irp, Scb );

            if (Status != STATUS_SUCCESS) { leave; }
        }
#endif

        //
        //  We now assemble the names and in memory-structures for both the
        //  source and target links and check if the target link currently
        //  exists.
        //

#if 0
        NtfsFindTargetElements( IrpContext,
                                TargetFileObject,
                                ParentScb,
                                &TargetParentScb,
                                &NewFullLinkName,
                                &NewLinkName );

        //
        //  Check that the new name is not invalid.
        //

        if ((NewLinkName.Length > (NTFS_MAX_FILE_NAME_LENGTH * sizeof( WCHAR ))) ||
            !NtfsIsFileNameValid( &NewLinkName, FALSE )) {

            Status = STATUS_OBJECT_NAME_INVALID;
            leave;
        }
#endif

        //
        //  Acquire the current parent in order to synchronize removing the current name.
        //

#if 0
        NtfsAcquireExclusiveScb( IrpContext, ParentScb );

        //
        //  If this Scb does not have a normalized name then provide it with one now.
        //

        if (ParentScb->ScbType.Index.NormalizedName.Length == 0) {

            NtfsBuildNormalizedName( IrpContext,
                                     ParentScb->Fcb,
                                     ParentScb,
                                     &ParentScb->ScbType.Index.NormalizedName );
        }

        //
        //  If this is a directory then make sure it has a normalized name.
        //

        if (IsDirectory( &Fcb->Info ) &&
            (Scb->ScbType.Index.NormalizedName.Length == 0)) {

            NtfsUpdateNormalizedName( IrpContext,
                                      ParentScb,
                                      Scb,
                                      NULL,
                                      FALSE );
        }
#endif

        //
        //  Check if we are renaming to the same directory with the exact same name.
        //
#if 0

        if (TargetParentScb == ParentScb) {

            if (NtfsAreNamesEqual( Vcb->UpcaseTable, &NewLinkName, &Lcb->ExactCaseLink.LinkName, FALSE )) {

                DebugTrace( 0, Dbg, ("Renaming to same name and directory\n") );
                leave;
            }

        //
        //  Otherwise we want to acquire the target directory.
        //

        } else {

            //
            //  We need to do the acquisition carefully since we may only have the Vcb shared.
            //

            if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX )) {

                if (!NtfsAcquireExclusiveFcb( IrpContext,
                                              TargetParentScb->Fcb,
                                              TargetParentScb,
                                              ACQUIRE_DONT_WAIT )) {

                    SetFlag( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX );
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                //
                //  Now snapshot the Scb.
                //

                if (FlagOn( TargetParentScb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

                    NtfsSnapshotScb( IrpContext, TargetParentScb );
                }

            } else {

                NtfsAcquireExclusiveScb( IrpContext, TargetParentScb );
            }

            SetFlag( RenameFlags, MOVE_TO_NEW_DIR );
        }
#endif

        //
        //  We also determine which type of link to
        //  create.  We create a hard link only unless the source link is
        //  a primary link and the user is an IgnoreCase guy.
        //

        if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS ) &&
            FlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE )) {

            SetFlag( RenameFlags, ADD_PRIMARY_LINK );
        }

        //
        //  Lookup the entry for this filename in the target directory.
        //  We look in the Ccb for the type of case match for the target
        //  name.
        //

#if 0
        FoundLink = NtfsLookupEntry( IrpContext,
                                     TargetParentScb,
                                     BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE ),
                                     &NewLinkName,
                                     &FileNameAttr,
                                     &FileNameAttrLength,
                                     NULL,
                                     &IndexEntry,
                                     &IndexEntryBcb,
                                     NULL );
#endif

        //
        //  This call to NtfsLookupEntry may decide to push the root index,
        //  in which case we might be holding the Mft now.  If there is a
        //  transaction, commit it now so we will be able to free the Mft to
        //  eliminate a potential deadlock with the ObjectId index when we
        //  look up the object id for the rename source to add it to the
        //  tunnel cache.
        //

#if 1
		ASSERT( IrpContext->TransactionId == 0 );
#else
        if (IrpContext->TransactionId != 0) {

            NtfsCheckpointCurrentTransaction( IrpContext );

            //
            //  Go through and free any Scb's in the queue of shared
            //  Scb's for transactions.
            //

            if (IrpContext->SharedScb != NULL) {

                NtfsReleaseSharedResources( IrpContext );
                ASSERT( IrpContext->SharedScb == NULL );
            }

            //
            //  Release the mft, if we acquired it in pushing the root index.
            //

            NtfsReleaseExclusiveScbIfOwned( IrpContext, Vcb->MftScb );
        }
#endif

        //
        //  If we found a matching link, we need to check how we want to operate
        //  on the source link and the target link.  This means whether we
        //  have any work to do, whether we need to remove the target link
        //  and whether we need to remove the source link.
        //

#if 0
        if (FoundLink) {

            PFILE_NAME IndexFileName;

            //
            //  Assume we will remove this link.
            //

            SetFlag( RenameFlags, REMOVE_TARGET_LINK );

            IndexFileName = (PFILE_NAME) NtfsFoundIndexEntry( IndexEntry );

            NtfsCheckLinkForRename( Fcb,
                                    Lcb,
                                    IndexFileName,
                                    IndexEntry->FileReference,
                                    &NewLinkName,
                                    BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE ),
                                    &RenameFlags );

            //
            //  Assume we will use the existing name flags on the link found.  This
            //  will be the case where the file was opened with the 8.3 name and
            //  the new name is exactly the long name for the same file.
            //

            PrevLinkNameFlags =
            NewLinkNameFlags = IndexFileName->Flags;

            //
            //  If we didn't have an exact match, then we need to check if we
            //  can remove the found link and then remove it from the disk.
            //

            if (FlagOn( RenameFlags, REMOVE_TARGET_LINK )) {

                //
                //  We need to check that the user wanted to remove that link.
                //

                if (!FlagOn( RenameFlags, TRAVERSE_MATCH ) &&
                    !IrpSp->Parameters.SetFile.ReplaceIfExists) {

                    Status = STATUS_OBJECT_NAME_COLLISION;
                    leave;
                }

                //
                //  We want to preserve the case and the flags of the matching
                //  link found.  We also want to preserve the case of the
                //  name being created.  The following variables currently contain
                //  the exact case for the target to remove and the new name to
                //  apply.
                //
                //      Link to remove - In 'IndexEntry'.
                //          The link's flags are also in 'IndexEntry'.  We copy
                //          these flags to 'PrevLinkNameFlags'
                //
                //      New Name - Exact case is stored in 'NewLinkName'
                //               - It is also in 'FileNameAttr
                //
                //  We modify this so that we can use the FileName attribute
                //  structure to create the new link.  We copy the linkname being
                //  removed into 'PrevLinkName'.   The following is the
                //  state after the switch.
                //
                //      'FileNameAttr' - contains the name for the link being
                //          created.
                //
                //      'PrevLinkFileName' - Contains the link name for the link being
                //          removed.
                //
                //      'PrevLinkFileNameFlags' - Contains the name flags for the link
                //          being removed.
                //

                //
                //  Allocate a buffer for the name being removed.  It should be
                //  large enough for the entire directory name.
                //

                PrevFullLinkName.MaximumLength = TargetParentScb->ScbType.Index.NormalizedName.Length +
                                                 sizeof( WCHAR ) +
                                                 (IndexFileName->FileNameLength * sizeof( WCHAR ));

                PrevFullLinkName.Buffer = NtfsAllocatePool( PagedPool,
                                                            PrevFullLinkName.MaximumLength );

                RtlCopyMemory( PrevFullLinkName.Buffer,
                               TargetParentScb->ScbType.Index.NormalizedName.Buffer,
                               TargetParentScb->ScbType.Index.NormalizedName.Length );

                NextChar = Add2Ptr( PrevFullLinkName.Buffer,
                                    TargetParentScb->ScbType.Index.NormalizedName.Length );

                if (TargetParentScb != Vcb->RootIndexScb) {

                    *NextChar = L'\\';
                    NextChar += 1;
                }

                RtlCopyMemory( NextChar,
                               IndexFileName->FileName,
                               IndexFileName->FileNameLength * sizeof( WCHAR ));

                //
                //  Copy the name found in the Index Entry to 'PrevLinkName'
                //

                PrevLinkName.Buffer = NextChar;
                PrevLinkName.MaximumLength =
                PrevLinkName.Length = IndexFileName->FileNameLength * sizeof( WCHAR );

                //
                //  Update the full name length with the final component.
                //

                PrevFullLinkName.Length = (USHORT) PtrOffset( PrevFullLinkName.Buffer, NextChar ) + PrevLinkName.Length;

                //
                //  We only need this check if the link is for a different file.
                //

                if (!FlagOn( RenameFlags, TRAVERSE_MATCH )) {

                    //
                    //  We check if there is an existing Fcb for the target link.
                    //  If there is, the unclean count better be 0.
                    //

                    NtfsAcquireFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = TRUE;

                    TargetLinkFcb = NtfsCreateFcb( IrpContext,
                                                   Vcb,
                                                   IndexEntry->FileReference,
                                                   FALSE,
                                                   BooleanFlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ),
                                                   &ExistingTargetLinkFcb );

                    //
                    //  Before we go on, make sure we aren't about to rename over a system file.
                    //

                    if (FlagOn( TargetLinkFcb->FcbState, FCB_STATE_SYSTEM_FILE )) {

                        Status = STATUS_ACCESS_DENIED;
                        leave;
                    }

                    //
                    //  Add a paging resource to the target - this is not supplied if its created
                    //  from scratch. We need this (acquired in the proper order) for the delete
                    //  to work correctly if there are any data streams. It's not going to harm a
                    //  directory del and because of the teardown in the finally clause its difficult
                    //  to retry again without looping.
                    //

                    NtfsLockFcb( IrpContext, TargetLinkFcb );
                    LockedFcb = TargetLinkFcb;
                    if (TargetLinkFcb->PagingIoResource == NULL) {
                        TargetLinkFcb->PagingIoResource = NtfsAllocateEresource();
                    }
                    NtfsUnlockFcb( IrpContext, LockedFcb );
                    LockedFcb = NULL;

                    //
                    //  We need to acquire this file carefully in the event that we don't hold
                    //  the Vcb exclusively.
                    //

                    if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX )) {

                        if (!ExAcquireResourceExclusiveLite( TargetLinkFcb->PagingIoResource, FALSE )) {

                            SetFlag( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX );
                            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                        }

                        FcbWithPagingToRelease = TargetLinkFcb;

                        if (!NtfsAcquireExclusiveFcb( IrpContext, TargetLinkFcb, NULL, ACQUIRE_DONT_WAIT )) {

                            SetFlag( IrpContext->State, IRP_CONTEXT_STATE_ACQUIRE_EX );
                            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                        }

                        NtfsReleaseFcbTable( IrpContext, Vcb );
                        AcquiredFcbTable = FALSE;

                    } else {

                        NtfsReleaseFcbTable( IrpContext, Vcb );
                        AcquiredFcbTable = FALSE;

                        //
                        //  Acquire the paging Io resource for this file before the main
                        //  resource in case we need to delete.
                        //

                        FcbWithPagingToRelease = TargetLinkFcb;
                        ExAcquireResourceExclusiveLite( FcbWithPagingToRelease->PagingIoResource, TRUE );

                        NtfsAcquireExclusiveFcb( IrpContext, TargetLinkFcb, NULL, 0 );
                    }

                    AcquiredTargetLinkFcb = TRUE;

                    //
                    //  If the Fcb Info field needs to be initialized, we do so now.
                    //  We read this information from the disk as the duplicate information
                    //  in the index entry is not guaranteed to be correct.
                    //

                    if (!FlagOn( TargetLinkFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

                        NtfsUpdateFcbInfoFromDisk( IrpContext,
                                                   TRUE,
                                                   TargetLinkFcb,
                                                   NULL );
                        NtfsConditionallyFixupQuota( IrpContext, TargetLinkFcb );

                        if (IrpContext->TransactionId != 0) {

                            NtfsCheckpointCurrentTransaction( IrpContext );
                            ASSERTMSG( "Ntfs: we should not own the mftscb\n", !NtfsIsSharedScb( Vcb->MftScb ) );
                        }
                    }

                    //
                    //  We are adding a link to the source file which already
                    //  exists as a link to a different file in the target directory.
                    //
                    //  We need to check whether we permitted to delete this
                    //  link.  If not then it is possible that the problem is
                    //  an existing batch oplock on the file.  In that case
                    //  we want to delete the batch oplock and try this again.
                    //

                    Status = NtfsCheckFileForDelete( IrpContext,
                                                     TargetParentScb,
                                                     TargetLinkFcb,
                                                     ExistingTargetLinkFcb,
                                                     IndexEntry );

                    if (!NT_SUCCESS( Status )) {

                        PSCB NextScb = NULL;

                        //
                        //  We are going to either fail this request or pass
                        //  this on to the oplock package.  Test if there is
                        //  a batch oplock on any streams on this file.
                        //

                        while ((NextScb = NtfsGetNextChildScb( TargetLinkFcb,
                                                               NextScb )) != NULL) {

                            if ((NextScb->AttributeTypeCode == $DATA) &&
                                (NextScb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA) &&
                                FsRtlCurrentBatchOplock( &NextScb->ScbType.Data.Oplock )) {

                                if (*VcbAcquired) {
                                    NtfsReleaseVcb( IrpContext, Vcb );
                                    *VcbAcquired = FALSE;
                                }

                                Status = FsRtlCheckOplock( &NextScb->ScbType.Data.Oplock,
                                                           Irp,
                                                           IrpContext,
                                                           NtfsOplockComplete,
                                                           NtfsPrePostIrp );
                                break;
                            }
                        }

                        leave;
                    }

                    NtfsCleanupLinkForRemoval( TargetLinkFcb, TargetParentScb, ExistingTargetLinkFcb );

                    //
                    //  DeleteFile might need to get the reparse index to remove a reparse
                    //  point.  We may need the object id index later to deal with the
                    //  tunnel cache.  Let's acquire them in the right order now.
                    //

                    if (HasReparsePoint( &TargetLinkFcb->Info ) &&
                        (Vcb->ReparsePointTableScb != NULL)) {

                        NtfsAcquireExclusiveScb( IrpContext, Vcb->ReparsePointTableScb );
                        AcquiredReparsePointIndex = TRUE;
                    }

                    if (Vcb->ObjectIdTableScb != NULL) {

                        NtfsAcquireExclusiveScb( IrpContext, Vcb->ObjectIdTableScb );
                        AcquiredObjectIdIndex = TRUE;
                    }

                    if (TargetLinkFcb->LinkCount == 1) {

                        PFCB TempFcb;

                        //
                        //  Fixup the IrpContext CleanupStructure so deletefile logs correctly
                        //

                        TempFcb = (PFCB) IrpContext->CleanupStructure;
                        IrpContext->CleanupStructure = FcbWithPagingToRelease;

                        ASSERT( (NULL == TempFcb) || (NTFS_NTC_FCB == SafeNodeType( TempFcb )) );

                        FcbWithPagingToRelease = TempFcb;

                        NtfsDeleteFile( IrpContext,
                                        TargetLinkFcb,
                                        TargetParentScb,
                                        &AcquiredParentScb,
                                        NULL,
                                        NULL );

                        FcbWithPagingToRelease = IrpContext->CleanupStructure;
                        IrpContext->CleanupStructure = TempFcb;

                        //
                        //  Make sure to force the close record out to disk.
                        //

                        TargetLinkFcbCountAdj += 1;

                    } else {
                        NtfsPostUsnChange( IrpContext, TargetLinkFcb, USN_REASON_HARD_LINK_CHANGE | USN_REASON_CLOSE );
                        NtfsRemoveLink( IrpContext,
                                        TargetLinkFcb,
                                        TargetParentScb,
                                        PrevLinkName,
                                        NULL,
                                        NULL );

                        ClearFlag( TargetLinkFcb->FcbState, FCB_STATE_VALID_USN_NAME );

                        TargetLinkFcbCountAdj += 1;
                        NtfsUpdateFcb( TargetLinkFcb, FCB_INFO_CHANGED_LAST_CHANGE );
                    }

                //
                //  The target link is for the same file as the source link.  No security
                //  checks need to be done.  Go ahead and remove it.
                //

                } else {

                    NtfsPostUsnChange( IrpContext, Scb, USN_REASON_RENAME_OLD_NAME );

                    TargetLinkFcb = Fcb;
                    NtfsRemoveLink( IrpContext,
                                    Fcb,
                                    TargetParentScb,
                                    PrevLinkName,
                                    NULL,
                                    NULL );

                    FcbLinkCountAdj += 1;
                }
            }
        }
#endif

        NtfsUnpinBcb( IrpContext, &IndexEntryBcb );

        //
        //  Post the Usn record for the old name.  Don't write it until after
        //  we check if we need to remove an object ID due to tunnelling.
        //  Otherwise we might deadlock between the journal/mft resources
        //  and the object id resources.
        //

#if 0
        NtfsPostUsnChange( IrpContext, Scb, USN_REASON_RENAME_OLD_NAME );
#endif

        //
        //  See if we need to remove the current link.
        //

#if 0
        if (FlagOn( RenameFlags, REMOVE_SOURCE_LINK )) {

            //
            //  Now we want to remove the source link from the file.  We need to
            //  remember if we deleted a two part primary link.
            //

            if (FlagOn( RenameFlags, ACTIVELY_REMOVE_SOURCE_LINK )) {

                TunneledData.HasObjectId = FALSE;
                NtfsRemoveLink( IrpContext,
                                Fcb,
                                ParentScb,
                                Lcb->ExactCaseLink.LinkName,
                                &NamePair,
                                &TunneledData );

                //
                //  Remember the full name for the original filename and some
                //  other information to pass to the dirnotify package.
                //

                if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

                    if (!IsDirectory( &Fcb->Info ) &&
                        !FlagOn( FileObject->Flags, FO_OPENED_CASE_SENSITIVE )) {

                        //
                        //  Tunnel property information for file links
                        //

                        NtfsGetTunneledData( IrpContext,
                                             Fcb,
                                             &TunneledData );

                        FsRtlAddToTunnelCache(  &Vcb->Tunnel,
                                                *(PULONGLONG)&ParentScb->Fcb->FileReference,
                                                &NamePair.Short,
                                                &NamePair.Long,
                                                BooleanFlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS ),
                                                sizeof( NTFS_TUNNELED_DATA ),
                                                &TunneledData );
                    }
                }

                FcbLinkCountAdj += 1;
            }

            if (ReportDirNotify) {

                SourceFullLinkName.Buffer = NtfsAllocatePool( PagedPool, Ccb->FullFileName.Length );

                RtlCopyMemory( SourceFullLinkName.Buffer,
                               Ccb->FullFileName.Buffer,
                               Ccb->FullFileName.Length );

                SourceFullLinkName.MaximumLength = SourceFullLinkName.Length = Ccb->FullFileName.Length;
                SourceLinkLastNameOffset = Ccb->LastFileNameOffset;
            }
        }
#endif

        //
        //  See if we need to add the target link.
        //

#if 0
        if (FlagOn( RenameFlags, ADD_TARGET_LINK )) {

            //
            //  Check that we have permission to add a file to this directory.
            //

            NtfsCheckIndexForAddOrDelete( IrpContext,
                                          TargetParentScb->Fcb,
                                          (IsDirectory( &Fcb->Info ) ?
                                           FILE_ADD_SUBDIRECTORY :
                                           FILE_ADD_FILE),
                                          Ccb->AccessFlags >> 2 );

            //
            //  Grunge the tunnel cache for property restoration
            //

            if (!IsDirectory( &Fcb->Info ) &&
                !FlagOn( FileObject->Flags, FO_OPENED_CASE_SENSITIVE )) {

                NtfsResetNamePair( &NamePair );
                TunneledDataSize = sizeof( NTFS_TUNNELED_DATA );

                if (FsRtlFindInTunnelCache( &Vcb->Tunnel,
                                            *(PULONGLONG)&TargetParentScb->Fcb->FileReference,
                                            &NewLinkName,
                                            &NamePair.Short,
                                            &NamePair.Long,
                                            &TunneledDataSize,
                                            &TunneledData)) {

                    ASSERT( TunneledDataSize == sizeof( NTFS_TUNNELED_DATA ));
                    HaveTunneledInformation = TRUE;
                }
            }

            //
            //  We now want to add the new link into the target directory.
            //  We create a hard link only if the source name was a hard link
            //  or this is a case-sensitive open.  This means that we can
            //  replace a primary link pair with a hard link only.
            //

            NtfsAddLink( IrpContext,
                         BooleanFlagOn( RenameFlags, ADD_PRIMARY_LINK ),
                         TargetParentScb,
                         Fcb,
                         FileNameAttr,
                         NULL,
                         &NewLinkNameFlags,
                         NULL,
                         HaveTunneledInformation ? &NamePair : NULL,
                         NULL );

            //
            //  Restore timestamps on tunneled files
            //

            if (HaveTunneledInformation) {

                NtfsSetTunneledData( IrpContext,
                                     Fcb,
                                     &TunneledData );

                SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_CREATE );
                SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                //
                //  If we have tunneled information then copy the correct case of the
                //  name into the new link pointer.
                //

                if (NewLinkNameFlags == FILE_NAME_DOS) {

                    RtlCopyMemory( NewLinkName.Buffer,
                                   NamePair.Short.Buffer,
                                   NewLinkName.Length );
                }
            }

            //
            //  Update the flags field in the target file name.  We will use this
            //  below if we are updating the normalized name.
            //

            FileNameAttr->Flags = NewLinkNameFlags;

            if (ParentScb != TargetParentScb) {

                NtfsUpdateFcb( TargetParentScb->Fcb,
                               (FCB_INFO_CHANGED_LAST_CHANGE |
                                FCB_INFO_CHANGED_LAST_MOD |
                                FCB_INFO_UPDATE_LAST_ACCESS) );
            }

            //
            //  If we need a full buffer for the new name for notify and don't already
            //  have one then construct the full name now.  This will only happen if
            //  we are renaming within the same directory.
            //

            if (ReportDirNotify &&
                (NewFullLinkName.Buffer == NULL)) {

                NewFullLinkName.MaximumLength = Ccb->LastFileNameOffset + NewLinkName.Length;

                NewFullLinkNameBuffer = NtfsAllocatePool( PagedPool,
                                                          NewFullLinkName.MaximumLength );

                RtlCopyMemory( NewFullLinkNameBuffer,
                               Ccb->FullFileName.Buffer,
                               Ccb->LastFileNameOffset );

                RtlCopyMemory( Add2Ptr( NewFullLinkNameBuffer, Ccb->LastFileNameOffset ),
                               NewLinkName.Buffer,
                               NewLinkName.Length );

                NewFullLinkName.Buffer = NewFullLinkNameBuffer;
                NewFullLinkName.Length = NewFullLinkName.MaximumLength;
            }

            FcbLinkCountAdj -= 1;
        }
#endif

        //
        //  Now write the Usn record for the old name if it exists.  Since this call
        //  needs to acquire the usn journal and/or mft, we need to do this after the
        //  NtfsSetTunneledData call, since that may acquire the object id index.
        //

#if 1
		ASSERT( IrpContext->Usn.CurrentUsnFcb == NULL );
#else
        if (IrpContext->Usn.CurrentUsnFcb != NULL) {
            NtfsWriteUsnJournalChanges( IrpContext );
        }
#endif

        //
        //  We need to update the names in the Lcb for this file as well as any subdirectories
        //  or files.  We will do this in two passes.  The first pass is just to reserve enough
        //  space in all of the file objects and Lcb's.  We update the names in the second pass.
        //

        if (FlagOn( RenameFlags, TRAVERSE_MATCH )) {

            if (FlagOn( RenameFlags, REMOVE_TARGET_LINK )) {

                SetFlag( RenameFlags, REMOVE_TRAVERSE_LINK );

            } else {

                SetFlag( RenameFlags, REUSE_TRAVERSE_LINK );
            }
        }

        //
        //  If this is a directory and we added a target link it means that the
        //  normalized name has changed.  Make sure the buffer in the Scb will hold
        //  the larger name.
        //

#if 0
        if (IsDirectory( &Fcb->Info ) && FlagOn( RenameFlags, ADD_TARGET_LINK )) {

            NtfsUpdateNormalizedName( IrpContext,
                                      TargetParentScb,
                                      Scb,
                                      FileNameAttr,
                                      TRUE );
        }
#endif

        //
        //  Now post a rename change on the Fcb.  We delete the old Usn record first,
        //  since it has the wrong name.  No need to get the mutex since we have the
        //  file exclusive.
        //

#if 1
		ASSERT( Fcb->FcbUsnRecord == NULL );
#else
        if (Fcb->FcbUsnRecord != NULL) {

            SavedFcbUsnRecord = Fcb->FcbUsnRecord;
            SavedUsnReason = SavedFcbUsnRecord->UsnRecord.Reason;
            if (SavedFcbUsnRecord->ModifiedOpenFilesLinks.Flink != NULL) {
                NtfsLockFcb( IrpContext, Vcb->UsnJournal->Fcb );
                RemoveEntryList( &SavedFcbUsnRecord->ModifiedOpenFilesLinks );

                if (SavedFcbUsnRecord->TimeOutLinks.Flink != NULL) {

                    RemoveEntryList( &SavedFcbUsnRecord->TimeOutLinks );
                    SavedFcbUsnRecord->ModifiedOpenFilesLinks.Flink = NULL;
                }

                NtfsUnlockFcb( IrpContext, Vcb->UsnJournal->Fcb );
            }
            Fcb->FcbUsnRecord = NULL;

            //
            //  Note - Fcb is unlocked immediately below in the finally clause.
            //
        }
#endif

        //
        //  Post the rename to the Usn Journal.  We wait until the end, in order to
        //  reduce resource contention on the UsnJournal, in the event that we already
        //  posted a change when we deleted the target file.
        //

#if 0
        NtfsPostUsnChange( IrpContext,
                           Scb,
                           (SavedUsnReason & ~USN_REASON_RENAME_OLD_NAME) | USN_REASON_RENAME_NEW_NAME );
#endif

        //
        //  Now, if anything at all is posted to the Usn Journal, we must write it now
        //  so that we do not get a log file full later.  But do not checkpoint until
        //  any failure cases are behind us.
        //

#if 1
		ASSERT( IrpContext->Usn.CurrentUsnFcb == NULL );
#else
        if (IrpContext->Usn.CurrentUsnFcb != NULL) {
            NtfsWriteUsnJournalChanges( IrpContext );
        }
#endif

        //
        //  We have now modified the on-disk structures.  We now need to
        //  modify the in-memory structures.  This includes the Fcb and Lcb's
        //  for any links we superseded, and the source Fcb and it's Lcb's.
        //
        //  We will do this in two passes.  The first pass will guarantee that all of the
        //  name buffers will be large enough for the names.  The second pass will store the
        //  names into the buffers.
        //

#if 0
        if (FlagOn( RenameFlags, MOVE_TO_NEW_DIR )) {

            NtfsMoveLinkToNewDir( IrpContext,
                                  &NewFullLinkName,
                                  &NewLinkName,
                                  NewLinkNameFlags,
                                  TargetParentScb,
                                  Fcb,
                                  Lcb,
                                  RenameFlags,
                                  &PrevLinkName,
                                  PrevLinkNameFlags );

        //
        //  Otherwise we will rename in the current directory.  We need to remember
        //  if we have merged with an existing link on this file.
        //

        } else {

            NtfsRenameLinkInDir( IrpContext,
                                 ParentScb,
                                 Fcb,
                                 Lcb,
                                 &NewLinkName,
                                 NewLinkNameFlags,
                                 RenameFlags,
                                 &PrevLinkName,
                                 PrevLinkNameFlags );
        }

        //
        //  Now, checkpoint the transaction to free resources if we are holding on
        //  to the Usn Journal.  No more failures can occur.
        //

        if (IrpContext->Usn.CurrentUsnFcb != NULL) {
            NtfsCheckpointCurrentTransaction( IrpContext );
        }
#endif

        //
        //  Nothing should fail from this point forward.
        //
        //  Now make the change to the normalized name.  The buffer should be
        //  large enough.
        //

#if 0
        if (IsDirectory( &Fcb->Info ) && FlagOn( RenameFlags, ADD_TARGET_LINK )) {

            NtfsUpdateNormalizedName( IrpContext,
                                      TargetParentScb,
                                      Scb,
                                      FileNameAttr,
                                      FALSE );
        }
#endif

        //
        //  Now look at the link we superseded.  If we deleted the file then go through and
        //  mark everything as deleted.
        //

        if (FlagOn( RenameFlags, REMOVE_TARGET_LINK | TRAVERSE_MATCH ) == REMOVE_TARGET_LINK) {

#if 0
            NtfsUpdateFcbFromLinkRemoval( IrpContext,
                                          TargetParentScb,
                                          TargetLinkFcb,
                                          PrevLinkName,
                                          PrevLinkNameFlags );

            //
            //  If the link count is going to 0, we need to perform the work of
            //  removing the file.
            //
#endif
#if 1
			if (1) {
#else
            if (TargetLinkFcb->LinkCount == 1) {
#endif
                SetFlag( TargetLinkFcb->FcbState, FCB_STATE_FILE_DELETED );

                //
                //  We need to mark all of the Scbs as gone.
                //

                for (Links = TargetLinkFcb->ScbQueue.Flink;
                     Links != &TargetLinkFcb->ScbQueue;
                     Links = Links->Flink) {

                    ThisScb = CONTAINING_RECORD( Links,
                                                 SCB,
                                                 FcbLinks );

                    SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                }
            }
        }

        //
        //  Change the time stamps in the parent if we modified the links in this directory.
        //

        if (FlagOn( RenameFlags, REMOVE_SOURCE_LINK )) {

#if 0
            NtfsUpdateFcb( ParentScb->Fcb,
                           (FCB_INFO_CHANGED_LAST_CHANGE |
                            FCB_INFO_CHANGED_LAST_MOD |
                            FCB_INFO_UPDATE_LAST_ACCESS) );
#endif
        }

        //
        //  We always set the last change time on the file we renamed unless
        //  the caller explicitly set this.
        //

        SetFlag( Ccb->Flags, CCB_FLAG_UPDATE_LAST_CHANGE );

        //
        //  Don't set the archive bit on a directory.  Otherwise we break existing
        //  apps that don't expect to see this flag.
        //

#if 1
        if (!IsDirectory( &Fcb->Info )) {

            SetFlag( Ccb->Flags, CCB_FLAG_SET_ARCHIVE );
        }
#endif

        //
        //  Report the changes to the affected directories.  We defer reporting
        //  until now so that all of the on disk changes have been made.
        //  We have already preserved the original file name for any changes
        //  associated with it.
        //
        //  Note that we may have to make a call to notify that we are removing
        //  a target if there is only a case change.  This could make for
        //  a third notify call.
        //
        //  Now that we have the new name we need to decide whether to report
        //  this as a change in the file or adding a file to a new directory.
        //

#if 0
        if (ReportDirNotify) {

            ULONG FilterMatch = 0;
            ULONG Action;

            //
            //  If we are deleting a target link in order to make a case change then
            //  report that.
            //

            if ((PrevFullLinkName.Buffer != NULL) &&
                FlagOn( RenameFlags,
                        OVERWRITE_SOURCE_LINK | REMOVE_TARGET_LINK | EXACT_CASE_MATCH ) == REMOVE_TARGET_LINK) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &PrevFullLinkName,
                                     PrevFullLinkName.Length - PrevLinkName.Length,
                                     NULL,
                                     ((TargetParentScb->ScbType.Index.NormalizedName.Length != 0) ?
                                      &TargetParentScb->ScbType.Index.NormalizedName :
                                      NULL),
                                     (IsDirectory( &TargetLinkFcb->Info ) ?
                                      FILE_NOTIFY_CHANGE_DIR_NAME :
                                      FILE_NOTIFY_CHANGE_FILE_NAME),
                                     FILE_ACTION_REMOVED,
                                     TargetParentScb->Fcb );
            }

            //
            //  If we stored the original name then we report the changes
            //  associated with it.
            //

            if (FlagOn( RenameFlags, REMOVE_SOURCE_LINK )) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &SourceFullLinkName,
                                     SourceLinkLastNameOffset,
                                     NULL,
                                     ((ParentScb->ScbType.Index.NormalizedName.Length != 0) ?
                                      &ParentScb->ScbType.Index.NormalizedName :
                                      NULL),
                                     (IsDirectory( &Fcb->Info ) ?
                                      FILE_NOTIFY_CHANGE_DIR_NAME :
                                      FILE_NOTIFY_CHANGE_FILE_NAME),
                                     ((FlagOn( RenameFlags, MOVE_TO_NEW_DIR ) ||
                                       !FlagOn( RenameFlags, ADD_TARGET_LINK ) ||
                                       (FlagOn( RenameFlags, REMOVE_TARGET_LINK | EXACT_CASE_MATCH ) == (REMOVE_TARGET_LINK | EXACT_CASE_MATCH))) ?
                                      FILE_ACTION_REMOVED :
                                      FILE_ACTION_RENAMED_OLD_NAME),
                                     ParentScb->Fcb );
            }

            //
            //  Check if a new name will appear in the directory.
            //

            if (!FoundLink ||
                (FlagOn( RenameFlags, OVERWRITE_SOURCE_LINK | EXACT_CASE_MATCH) == OVERWRITE_SOURCE_LINK) ||
                (FlagOn( RenameFlags, REMOVE_TARGET_LINK | EXACT_CASE_MATCH ) == REMOVE_TARGET_LINK)) {

                FilterMatch = IsDirectory( &Fcb->Info)
                              ? FILE_NOTIFY_CHANGE_DIR_NAME
                              : FILE_NOTIFY_CHANGE_FILE_NAME;

                //
                //  If we moved to a new directory, remember the
                //  action was a create operation.
                //

                if (FlagOn( RenameFlags, MOVE_TO_NEW_DIR )) {

                    Action = FILE_ACTION_ADDED;

                } else {

                    Action = FILE_ACTION_RENAMED_NEW_NAME;
                }

            //
            //  There was an entry with the same case.  If this isn't the
            //  same file then we report a change to all the file attributes.
            //

            } else if (FlagOn( RenameFlags, REMOVE_TARGET_LINK | TRAVERSE_MATCH ) == REMOVE_TARGET_LINK) {

                FilterMatch = (FILE_NOTIFY_CHANGE_ATTRIBUTES |
                               FILE_NOTIFY_CHANGE_SIZE |
                               FILE_NOTIFY_CHANGE_LAST_WRITE |
                               FILE_NOTIFY_CHANGE_LAST_ACCESS |
                               FILE_NOTIFY_CHANGE_CREATION |
                               FILE_NOTIFY_CHANGE_SECURITY |
                               FILE_NOTIFY_CHANGE_EA);

                //
                //  The file name isn't changing, only the properties of the
                //  file.
                //

                Action = FILE_ACTION_MODIFIED;
            }

            if (FilterMatch != 0) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &NewFullLinkName,
                                     NewFullLinkName.Length - NewLinkName.Length,
                                     NULL,
                                     ((TargetParentScb->ScbType.Index.NormalizedName.Length != 0) ?
                                      &TargetParentScb->ScbType.Index.NormalizedName :
                                      NULL),
                                     FilterMatch,
                                     Action,
                                     TargetParentScb->Fcb );
            }
        }
#endif

        //
        //  Now adjust the link counts on the different files.
        //

#if 0
        if (TargetLinkFcb != NULL) {

            TargetLinkFcb->LinkCount -= TargetLinkFcbCountAdj;
            TargetLinkFcb->TotalLinks -= TargetLinkFcbCountAdj;

            //
            //  Now go through and mark everything as deleted.
            //

            if (TargetLinkFcb->LinkCount == 0) {

                SetFlag( TargetLinkFcb->FcbState, FCB_STATE_FILE_DELETED );

                //
                //  We need to mark all of the Scbs as gone.
                //

                for (Links = TargetLinkFcb->ScbQueue.Flink;
                     Links != &TargetLinkFcb->ScbQueue;
                     Links = Links->Flink) {

                    ThisScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                    if (!FlagOn( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

                        NtfsSnapshotScb( IrpContext, ThisScb );

                        ThisScb->ValidDataToDisk =
                        ThisScb->Header.AllocationSize.QuadPart =
                        ThisScb->Header.FileSize.QuadPart =
                        ThisScb->Header.ValidDataLength.QuadPart = 0;
#if __NDAS_NTFS_DBG__
						strncpy(ThisScb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
						ThisScb->LastUpdateLine = __LINE__; 
#endif				

                        SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                    }
                }
            }
        }

        Fcb->TotalLinks -= FcbLinkCountAdj;
        Fcb->LinkCount -= FcbLinkCountAdj;
#endif

    } finally {

        DebugUnwind( NtfsSetRenameInfo );
#if 1
		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}
#endif
        if (LockedFcb != NULL) {
            NtfsUnlockFcb( IrpContext, LockedFcb );
        }

        //
        //  See if we have a SavedFcbUsnRecord.
        //

        if (SavedFcbUsnRecord != NULL) {

            //
            //  Conceivably we failed to reallcoate the record when we tried to post
            //  the rename.  If so, we will simply restore it here.  (Note the rename
            //  back to the old name will occur anyway.)
            //

            if (Fcb->FcbUsnRecord == NULL) {
                Fcb->FcbUsnRecord = SavedFcbUsnRecord;

            //
            //  Else just free the pool.
            //

            } else {
                NtfsFreePool( SavedFcbUsnRecord );
            }
        }

        //
        //  release objectid and reparse explicitly so we can call Teardown structures and wait to go up chain
        //

        if (AcquiredObjectIdIndex) { NtfsReleaseScb( IrpContext, Vcb->ObjectIdTableScb ); }
        if (AcquiredReparsePointIndex) { NtfsReleaseScb( IrpContext, Vcb->ReparsePointTableScb ); }
        if (AcquiredFcbTable) { NtfsReleaseFcbTable( IrpContext, Vcb ); }
        if (FcbWithPagingToRelease != NULL) { ExReleaseResourceLite( FcbWithPagingToRelease->PagingIoResource ); }
        NtfsUnpinBcb( IrpContext, &IndexEntryBcb );

        //
        //  If we allocated any buffers for the notify operations deallocate them now.
        //

        if (NewFullLinkNameBuffer != NULL) { NtfsFreePool( NewFullLinkNameBuffer ); }
        if (PrevFullLinkName.Buffer != NULL) { NtfsFreePool( PrevFullLinkName.Buffer ); }
        if (SourceFullLinkName.Buffer != NULL) {

            NtfsFreePool( SourceFullLinkName.Buffer );
        }

        //
        //  If we allocated a file name attribute, we deallocate it now.
        //

        if (FileNameAttr != NULL) { NtfsFreePool( FileNameAttr ); }

        //
        //  If we allocated a buffer for the tunneled names, deallocate them now.
        //

        if (NamePair.Long.Buffer != NamePair.LongBuffer) {

            NtfsFreePool( NamePair.Long.Buffer );
        }

        //
        //  Some cleanup only occurs if this request has not been posted to
        // the oplock package

        if (Status != STATUS_PENDING) {

            if (AcquiredTargetLinkFcb) {

#if 1
				NDASNTFS_ASSERT( FALSE );
#endif

                NtfsTeardownStructures( IrpContext,
                                        TargetLinkFcb,
                                        NULL,
                                        FALSE,
                                        0,
                                        NULL );
            }
        }

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine performs the set position information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;

    PFILE_POSITION_INFORMATION Buffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetPositionInfo...\n") );

    //
    //  Reference the system buffer containing the user specified position
    //  information record
    //

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    try {

        //
        //  Check if the file does not use intermediate buffering.  If it does
        //  not use intermediate buffering then the new position we're supplied
        //  must be aligned properly for the device
        //

        if (FlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING )) {

            PDEVICE_OBJECT DeviceObject;

            DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;

            if ((Buffer->CurrentByteOffset.LowPart & DeviceObject->AlignmentRequirement) != 0) {

                DebugTrace( 0, Dbg, ("Offset missaligned %08lx %08lx\n", Buffer->CurrentByteOffset.LowPart, Buffer->CurrentByteOffset.HighPart) );

                try_return( Status = STATUS_INVALID_PARAMETER );
            }
        }

        //
        //  Set the new current byte offset in the file object
        //

        FileObject->CurrentByteOffset = Buffer->CurrentByteOffset;

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsSetPositionInfo );

        NOTHING;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsSetPositionInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsPrepareToShrinkFileSize (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    LONGLONG NewFileSize
    )
/*++

Routine Description:

    Page in the last page of the file so we don't deadlock behind another thread
    trying to access it. (CcSetFileSizes will do a purge that will try to zero
    the cachemap directly when we shrink a file)

    Note: this requires droping and regaining the main resource to not deadlock
    and must be done before a transaction has started

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb for the file/directory being modified

    NewFileSize - The new size the file will shrink to

Return Value:

    NTSTATUS - The status of the operation

--*/
{
    IO_STATUS_BLOCK Iosb;
    ULONG Buffer;

    if (!MmCanFileBeTruncated( FileObject->SectionObjectPointer,
                               (PLARGE_INTEGER)&NewFileSize )) {

        return STATUS_USER_MAPPED_FILE;
    }

    if ((Scb->NonpagedScb->SegmentObject.DataSectionObject != NULL) &&
        ((NewFileSize % PAGE_SIZE) != 0)) {

#if 0
        if (NULL == Scb->FileObject) {
            NtfsCreateInternalAttributeStream( IrpContext,
                                               Scb,
                                               FALSE,
                                               &NtfsInternalUseFile[PREPARETOSHRINKFILESIZE_FILE_NUMBER] );
        }
#else
		if (!(CcIsFileCached(FileObject)) &&
			!FlagOn( Scb->Fcb->FcbState, FCB_STATE_PAGING_FILE ) &&
			IrpContext->OriginatingIrp && !FlagOn( IrpContext->OriginatingIrp->Flags, IRP_PAGING_IO )) {

			if (FileObject->PrivateCacheMap == NULL)
				CcInitializeCacheMap( FileObject,
									  (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
									  FALSE,
									  &NtfsData.CacheManagerCallbacks,
									  Scb );
		}

#endif

        ASSERT( NtfsIsExclusiveScb( Scb ) );
        NtfsReleaseScb( IrpContext,  Scb  );

        NewFileSize = NewFileSize & ~(PAGE_SIZE - 1);
        if (!CcCopyRead( Scb->FileObject,
                         (PLARGE_INTEGER)&NewFileSize,
                         1,
                         BooleanFlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT ),
                         &Buffer,
                         &Iosb )) {

            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
        }

        NtfsAcquireExclusiveScb( IrpContext, Scb );
    }

    return STATUS_SUCCESS;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set allocation information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - This is the Scb for the open operation.  May not be present if
        this is a Mm call.

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb = Scb->Fcb;
    BOOLEAN NonResidentPath = FALSE;
    BOOLEAN FileIsCached = FALSE;
    BOOLEAN ClearCheckSizeFlag = FALSE;

    LONGLONG NewAllocationSize;
    LONGLONG PrevAllocationSize;

    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    BOOLEAN CleanupAttrContext = FALSE;

#if 1
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

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetAllocationInfo...\n") );

    //
    //  Are we serialized correctly?  In NtfsCommonSetInformation above, we get
    //  paging shared for a lazy writer callback, but we should never end up in
    //  here from a lazy writer callback.
    //

    ASSERT( NtfsIsExclusiveScbPagingIo( Scb ) );

    //
    //  If this attribute has been 'deleted' then we we can return immediately
    //

    if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

        Status = STATUS_SUCCESS;

        DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo:  Attribute is already deleted\n") );

        return Status;
    }

    if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
    }

    if (Ccb != NULL) {

        //
        //  Remember the source info flags in the Ccb.
        //

        IrpContext->SourceInfo = Ccb->UsnSourceInfo;
    }

    //
    //  Save the current state of the Scb.
    //

#if 0
    NtfsSnapshotScb( IrpContext, Scb );
#endif

    //
    //  Get the new allocation size.
    //

    NewAllocationSize = ((PFILE_ALLOCATION_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->AllocationSize.QuadPart;
    PrevAllocationSize = Scb->Header.AllocationSize.QuadPart;

    //
    //  Check for a valid input value for the file size.
    //

    ASSERT( NewAllocationSize >= 0 );

    if ((ULONGLONG)NewAllocationSize > MAXFILESIZE) {

        Status = STATUS_INVALID_PARAMETER;
        DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo:  Invalid allocation size\n") );
        return Status;
    }

    //
    //  Do work to prepare for shrinking file if necc.
    //

    if (NewAllocationSize < Scb->Header.FileSize.QuadPart) {

        //
        //  Paging IO should never shrink the file.
        //

        ASSERT( !FlagOn( Irp->Flags, IRP_PAGING_IO ) );

        Status = NdasNtfsPrepareToShrinkFileSize( IrpContext, FileObject, Scb, NewAllocationSize );
        if (Status != STATUS_SUCCESS) {

            DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo -> %08lx\n", Status) );
            return Status;
        }
    }

#if 1
	if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

		Status = NtfsPostRequest( IrpContext, Irp );

		DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo:  Can't wait\n") );
		return Status;
	}
#endif

    //
    //  Use a try-finally so we can update the on disk time-stamps.
    //

    try {

#ifdef SYSCACHE
        //
        //  Let's remember this.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_SYSCACHE_FILE )) {

            PSYSCACHE_EVENT SyscacheEvent;

            SyscacheEvent = NtfsAllocatePool( PagedPool, sizeof( SYSCACHE_EVENT ) );

            SyscacheEvent->EventTypeCode = SYSCACHE_SET_ALLOCATION_SIZE;
            SyscacheEvent->Data1 = NewAllocationSize;
            SyscacheEvent->Data2 = 0L;

            InsertTailList( &Scb->ScbType.Data.SyscacheEventList, &SyscacheEvent->EventList );
        }
#endif

        //
        //  It is extremely expensive to make this call on a file that is not
        //  cached, and Ntfs has suffered stack overflows in addition to massive
        //  time and disk I/O expense (CcZero data on user mapped files!).  Therefore,
        //  if no one has the file cached, we cache it here to make this call cheaper.
        //
        //  Don't create the stream file if called from FsRtlSetFileSize (which sets
        //  IRP_PAGING_IO) because mm is in the process of creating a section.
        //


        if ((NewAllocationSize != Scb->Header.AllocationSize.QuadPart) &&
            (Scb->NonpagedScb->SegmentObject.DataSectionObject != NULL)) {

            FileIsCached = CcIsFileCached( FileObject );

            if (!FileIsCached &&
                !FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE ) &&
                !FlagOn( Irp->Flags, IRP_PAGING_IO )) {

#if 0
                NtfsCreateInternalAttributeStream( IrpContext,
                                                   Scb,
                                                   FALSE,
                                                   &NtfsInternalUseFile[SETALLOCATIONINFO_FILE_NUMBER] );
#else
				if (FileObject->PrivateCacheMap == NULL)
					CcInitializeCacheMap( FileObject,
										  (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
										  FALSE,
										  &NtfsData.CacheManagerCallbacks,
										  Scb );
#endif
                FileIsCached = TRUE;
            }
        }

        //
        //  If the caller is extending the allocation of resident attribute then
        //  we will force it to become non-resident.  This solves the problem of
        //  trying to keep the allocation and file sizes in sync with only one
        //  number to use in the attribute header.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            NtfsInitializeAttributeContext( &AttrContext );
            CleanupAttrContext = TRUE;

            NtfsLookupAttributeForScb( IrpContext,
                                       Scb,
                                       NULL,
                                       &AttrContext );

            //
            //  Convert if extending.
            //

            if (NewAllocationSize > Scb->Header.AllocationSize.QuadPart) {

                NtfsConvertToNonresident( IrpContext,
                                          Fcb,
                                          NtfsFoundAttribute( &AttrContext ),
                                          (BOOLEAN) (!FileIsCached),
                                          &AttrContext );

                NonResidentPath = TRUE;

            //
            //  Otherwise the allocation is shrinking or staying the same.
            //

            } else {

                NewAllocationSize = QuadAlign( (ULONG) NewAllocationSize );

                //
                //  If the allocation size doesn't change, we are done.
                //

                if ((ULONG) NewAllocationSize == Scb->Header.AllocationSize.LowPart) {

                    try_return( NOTHING );
                }

                //
                //  We are sometimes called by MM during a create section, so
                //  for right now the best way we have of detecting a create
                //  section is IRP_PAGING_IO being set, as in FsRtlSetFileSizes.
                //

                NtfsChangeAttributeValue( IrpContext,
                                          Fcb,
                                          (ULONG) NewAllocationSize,
                                          NULL,
                                          0,
                                          TRUE,
                                          FALSE,
                                          (BOOLEAN) (!FileIsCached),
                                          FALSE,
                                          &AttrContext );

                NtfsCleanupAttributeContext( IrpContext, &AttrContext );
                CleanupAttrContext = FALSE;

                //
                //  Post this to the Usn journal if we are shrinking the data.
                //

                if (NewAllocationSize < Scb->Header.FileSize.QuadPart) {
                    NtfsPostUsnChange( IrpContext, Scb, USN_REASON_DATA_TRUNCATION );
                }

                //
                //  Now update the sizes in the Scb.
                //

#if 0
                Scb->Header.AllocationSize.LowPart =
                Scb->Header.FileSize.LowPart =
                Scb->Header.ValidDataLength.LowPart = (ULONG) NewAllocationSize;

                Scb->TotalAllocated = NewAllocationSize;

#endif

#ifdef SYSCACHE_DEBUG
                if (ScbIsBeingLogged( Scb )) {
                    FsRtlLogSyscacheEvent( Scb, SCE_VDL_CHANGE, SCE_FLAG_SET_ALLOC, 0, 0, NewAllocationSize );
                }
#endif
            }

        } else {

            NonResidentPath = TRUE;
        }

        //
        //  We now test if we need to modify the non-resident allocation.  We will
        //  do this in two cases.  Either we're converting from resident in
        //  two steps or the attribute was initially non-resident.
        //

        if (NonResidentPath) {

            NewAllocationSize = LlClustersFromBytes( Scb->Vcb, NewAllocationSize );
            NewAllocationSize = LlBytesFromClusters( Scb->Vcb, NewAllocationSize );


            DebugTrace( 0, Dbg, ("NewAllocationSize -> %016I64x\n", NewAllocationSize) );

            //
            //  Now if the file allocation is being increased then we need to only add allocation
            //  to the attribute
            //

            if (Scb->Header.AllocationSize.QuadPart < NewAllocationSize) {

                //
                //  Add either the true disk allocation or add a hole for a sparse
                //  file.
                //

                if (!FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE )) {

                    //
                    //  If there is a compression unit then we could be in the process of
                    //  decompressing.  Allocate precisely in this case because we don't
                    //  want to leave any holes.  Specifically the user may have truncated
                    //  the file and is now regenerating it yet the clear compression operation
                    //  has already passed this point in the file (and dropped all resources).
                    //  No one will go back to cleanup the allocation if we leave a hole now.
                    //

                    if (!FlagOn( Scb->ScbState, SCB_STATE_WRITE_COMPRESSED ) &&
                        (Scb->CompressionUnit != 0)) {

                        ASSERT( FlagOn( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE ));
                        NewAllocationSize += Scb->CompressionUnit - 1;
                        ((PLARGE_INTEGER) &NewAllocationSize)->LowPart &= ~(Scb->CompressionUnit - 1);
                    }

#if 0
                    NtfsAddAllocation( IrpContext,
                                       FileObject,
                                       Scb,
                                       LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ),
                                       LlClustersFromBytes( Scb->Vcb, NewAllocationSize - Scb->Header.AllocationSize.QuadPart ),
                                       FALSE,
                                       NULL );
#endif

                } else {

#if 1
					NDASNTFS_ASSERT( FALSE );
#endif
                    NtfsAddSparseAllocation( IrpContext,
                                             FileObject,
                                             Scb,
                                             Scb->Header.AllocationSize.QuadPart,
                                             NewAllocationSize - Scb->Header.AllocationSize.QuadPart );
                }

                //
                //  Set the truncate on close flag.
                //

                SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );

            //
            //  Otherwise delete the allocation as requested.
            //

            } else if (Scb->Header.AllocationSize.QuadPart > NewAllocationSize) {

                //
                //  Check on possible cleanup if the file will shrink.
                //

                if (NewAllocationSize < Scb->Header.FileSize.QuadPart) {

                    //
                    //  If we will shrink FileSize, then write the UsnJournal.
                    //

#if 0
                    NtfsPostUsnChange( IrpContext, Scb, USN_REASON_DATA_TRUNCATION );
#endif

                    Scb->Header.FileSize.QuadPart = NewAllocationSize;

                    //
                    //  Do we need to shrink any of the valid data length values.
                    //

                    if (NewAllocationSize < Scb->Header.ValidDataLength.QuadPart) {

                        Scb->Header.ValidDataLength.QuadPart = NewAllocationSize;
#if __NDAS_NTFS_DBG__
						strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
						Scb->LastUpdateLine = __LINE__; 
#endif				
#ifdef SYSCACHE_DEBUG
                        if (ScbIsBeingLogged( Scb )) {
                            FsRtlLogSyscacheEvent( Scb, SCE_VDL_CHANGE, SCE_FLAG_SET_ALLOC, 0, 0, NewAllocationSize );
                        }
#endif
                    }

                    if (NewAllocationSize < Scb->ValidDataToDisk) {

                        Scb->ValidDataToDisk = NewAllocationSize;

#ifdef SYSCACHE_DEBUG
                        if (ScbIsBeingLogged( Scb )) {
                            FsRtlLogSyscacheEvent( Scb, SCE_VDD_CHANGE, SCE_FLAG_SET_ALLOC, 0, 0, NewAllocationSize );
                        }
#endif
                    }
                }

#if 0
                NtfsDeleteAllocation( IrpContext,
                                      FileObject,
                                      Scb,
                                      LlClustersFromBytes( Scb->Vcb, NewAllocationSize ),
                                      MAXLONGLONG,
                                      TRUE,
                                      TRUE );
#endif

            }

            //
            //  If this is the paging file then guarantee that the Mcb is fully loaded.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

                NtfsPreloadAllocation( IrpContext,
                                       Scb,
                                       0,
                                       LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ));
            }
        }

#if 1
		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		if(FlagOn(Ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			ASSERT( FlagOn(Ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_CORRUPTED) );
			try_return( Status = STATUS_FILE_CORRUPT_ERROR );
		}

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
														  IRP_MJ_SET_INFORMATION,
														  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

		if(secondaryRequest == NULL) {

			NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
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

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		KeClearEvent( &secondaryRequest->CompleteEvent );

		if(Status != STATUS_SUCCESS) {
	
			ASSERT( NDASNTFS_BUG );
			secondaryRequest = NULL;
			try_return( Status = STATUS_IO_DEVICE_ERROR );	
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		//ASSERT( ndfsWinxpReplytHeader->Information == 0);
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

		ASSERT( NT_SUCCESS(Status) );

		if (NT_SUCCESS(Status) && ndfsWinxpReplytHeader->AllocationSize != (UINT64)Scb->Header.AllocationSize.QuadPart) {

			PNDFS_NTFS_MCB_ENTRY	mcbEntry;
			ULONG			index;

			BOOLEAN			lookupResut;
			VCN				vcn;
			LCN				lcn;
			LCN				startingLcn;
			LONGLONG		clusterCount;

			ASSERT( NtfsIsExclusiveScb(Scb) );

			mcbEntry = (PNDFS_NTFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

			for (index=0, vcn=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++, mcbEntry++) {

				ASSERT( mcbEntry->Vcn == vcn );

				lookupResut = NtfsLookupNtfsMcbEntry( &Scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
					
				if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, Scb->Header.AllocationSize.QuadPart)) {

					ASSERT( lookupResut == TRUE );

					if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, ndfsWinxpReplytHeader->AllocationSize)) {
							
						ASSERT( startingLcn == lcn );
						ASSERT( vcn == mcbEntry->Vcn );
						ASSERT( lcn == mcbEntry->Lcn );
						ASSERT( clusterCount <= mcbEntry->ClusterCount || 
								Scb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize && (index+1) == ndfsWinxpReplytHeader->NumberOfMcbEntry );
							
						if (clusterCount < mcbEntry->ClusterCount) {

							NtfsAddNtfsMcbEntry( &Scb->Mcb, 
												 mcbEntry->Vcn, 
												 mcbEntry->Lcn, 
												 (LONGLONG)mcbEntry->ClusterCount, 
												 FALSE );
	
							lookupResut = NtfsLookupNtfsMcbEntry( &Scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );

							ASSERT( lookupResut == TRUE );
							ASSERT( startingLcn == lcn );
							ASSERT( vcn == mcbEntry->Vcn );
							ASSERT( lcn == mcbEntry->Lcn );
							ASSERT( clusterCount == mcbEntry->ClusterCount );
						}
					}
					
				} else { 

					ASSERT( lookupResut == FALSE || lcn == UNUSED_LCN );

					NtfsAddNtfsMcbEntry( &Scb->Mcb, 
										 mcbEntry->Vcn, 
										 mcbEntry->Lcn, 
										 (LONGLONG)mcbEntry->ClusterCount, 
										 FALSE );
				}

				vcn += mcbEntry->ClusterCount;
			}
			
			ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == ndfsWinxpReplytHeader->AllocationSize );

			if (Scb->Header.AllocationSize.QuadPart < ndfsWinxpReplytHeader->AllocationSize) {

				ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose );
				ASSERT( Scb->Header.FileSize.QuadPart == ndfsWinxpReplytHeader->FileSize );

				Scb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;
#if 0
				if (FileObject->SectionObjectPointer->DataSectionObject != NULL && FileObject->PrivateCacheMap == NULL) {

					CcInitializeCacheMap( FileObject,
										  (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
										  FALSE,
										  &NtfsData.CacheManagerCallbacks,
										  Scb );
				}
#endif
				NtfsSetBothCacheSizes( FileObject,
									   (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
									   Scb );

				SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
			}

			if (Scb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize) {

				Scb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

				Scb->Header.FileSize.QuadPart = ndfsWinxpReplytHeader->FileSize;

				DebugTrace(0, Dbg, ("NtfsSetEndOfFileInfo scb->Header.FileSize.QuadPart = %I64x, scb->Header.ValidDataLength.QuadPart = %I64x\n", 
									 Scb->Header.FileSize.QuadPart, Scb->Header.ValidDataLength.QuadPart) );
			
				if (Scb->Header.ValidDataLength.QuadPart > Scb->Header.FileSize.QuadPart) {

					Scb->Header.ValidDataLength.QuadPart = Scb->Header.FileSize.QuadPart;

#if __NDAS_NTFS_DBG__
					strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
					Scb->LastUpdateLine = __LINE__; 
#endif				
				}

				if (Scb->ValidDataToDisk > Scb->Header.FileSize.QuadPart) {

					Scb->ValidDataToDisk = Scb->Header.FileSize.QuadPart;
				}

				NtfsRemoveNtfsMcbEntry( &Scb->Mcb, 
										LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, Scb->Header.AllocationSize.QuadPart), 
										0xFFFFFFFF );

				NtfsSetBothCacheSizes( FileObject,
									   (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
									   Scb );
			}
#if 0
				mcbEntry = (PNDFS_NTFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

				NtfsAcquireNtfsMcbMutex( &Scb->Mcb );

				if (Scb->Header.AllocationSize.QuadPart)
					NtfsRemoveNtfsMcbEntry( &Scb->Mcb, 0, 0xFFFFFFFF );

				for (index=0, testVcn=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++) {

					ASSERT( mcbEntry[index].Vcn == testVcn );
					testVcn += mcbEntry[index].ClusterCount;

					NtfsAddNtfsMcbEntry( &Scb->Mcb, 
										 mcbEntry[index].Vcn, 
										 mcbEntry[index].Lcn, 
										 (LONGLONG)mcbEntry[index].ClusterCount, 
										 TRUE );
				}

				NtfsReleaseNtfsMcbMutex( &Scb->Mcb );
	
				ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, testVcn) == ndfsWinxpReplytHeader->AllocationSize );

				Scb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

#endif
		}

#if DBG
		{
			BOOLEAN			lookupResut;
			VCN				vcn;
			LCN				lcn;
			LCN				startingLcn;
			LONGLONG		clusterCount;

			vcn = 0;
			while (1) {
						
				lookupResut = NtfsLookupNtfsMcbEntry( &Scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
				if (lookupResut == FALSE || lcn == UNUSED_LCN)
					break;

				vcn += clusterCount;
			}

			ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == Scb->Header.AllocationSize.QuadPart );

		}

#endif

#endif

try_exit:

        if (PrevAllocationSize != Scb->Header.AllocationSize.QuadPart) {

            //
            //  Mark this file object as modified and with a size change in order to capture
            //  all of the changes to the Fcb.
            //

            SetFlag( FileObject->Flags, FO_FILE_SIZE_CHANGED );
            ClearCheckSizeFlag = TRUE;
        }

        //
        //  Always set the file as modified to force a time stamp change.
        //

        if (ARGUMENT_PRESENT( Ccb )) {

            SetFlag( Ccb->Flags,
                     (CCB_FLAG_UPDATE_LAST_MODIFY |
                      CCB_FLAG_UPDATE_LAST_CHANGE |
                      CCB_FLAG_SET_ARCHIVE) );

        } else {

            SetFlag( FileObject->Flags, FO_FILE_MODIFIED );
        }

        //
        //  Now capture any file size changes in this file object back to the Fcb.
        //

#if 0
        NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
#endif

        //
        //  Update the standard information if required.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

#if 0
            NtfsUpdateStandardInformation( IrpContext, Fcb );
#endif
        }

        ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

        //
        //  We know we wrote out any changes to the file size above so clear the
        //  flag in the Scb to check the attribute size.  This will save us from doing
        //  this unnecessarily at cleanup.
        //

        if (ClearCheckSizeFlag) {

            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        }

#if 0
        NtfsCheckpointCurrentTransaction( IrpContext );
#endif

        //
        //  Update duplicated information.
        //

#if 0
        NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
#endif

        //
        //  Update the cache manager if needed.
        //

        if (CcIsFileCached( FileObject )) {
            //
            //  We want to checkpoint the transaction if there is one active.
            //

#if 1
			ASSERT( IrpContext->TransactionId == 0 );
#else
            if (IrpContext->TransactionId != 0) {

                NtfsCheckpointCurrentTransaction( IrpContext );
            }
#endif

#ifdef SYSCACHE_DEBUG
            if (ScbIsBeingLogged( Scb )) {
                FsRtlLogSyscacheEvent( Scb, SCE_CC_SET_SIZE, SCE_FLAG_SET_ALLOC, 0, Scb->Header.ValidDataLength.QuadPart, Scb->Header.FileSize.QuadPart );
            }
#endif

            //
            //  Truncate either stream that is cached.
            //  Cachemap better exist or we will skip notifying cc and not potentially.
            //  purge the data section
            //

            ASSERT( FileObject->SectionObjectPointer->SharedCacheMap != NULL );
            NtfsSetBothCacheSizes( FileObject,
                                   (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
                                   Scb );

            //
            //  Clear out the write mask on truncates to zero.
            //

#ifdef SYSCACHE
            if ((Scb->Header.FileSize.QuadPart == 0) && FlagOn(Scb->ScbState, SCB_STATE_SYSCACHE_FILE) &&
                (Scb->ScbType.Data.WriteMask != NULL)) {
                RtlZeroMemory(Scb->ScbType.Data.WriteMask, (((0x2000000) / PAGE_SIZE) / 8));
            }
#endif

            //
            //  Now cleanup the stream we created if there are no more user
            //  handles.
            //

            if ((Scb->CleanupCount == 0) && (Scb->FileObject != NULL)) {
                NtfsDeleteInternalAttributeStream( Scb, FALSE, FALSE );
            }
        }

        Status = STATUS_SUCCESS;

    } finally {

        DebugUnwind( NtfsSetAllocation );
#if 1		
		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if( secondarySessionResourceAcquired == TRUE ) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}
#endif
        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( IrpContext, &AttrContext );
        }

        //
        //  And return to our caller
        //

        DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetEndOfFileInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb OPTIONAL,
    IN BOOLEAN VcbAcquired
    )

/*++

Routine Description:

    This routine performs the set end of file information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this operation.  Will always be present if the
        Vcb is acquired.  Otherwise we must test for it.

    AcquiredVcb - Indicates if this request has acquired the Vcb, meaning
        do we have duplicate information to update.

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb = Scb->Fcb;
    BOOLEAN NonResidentPath = TRUE;
    BOOLEAN FileSizeChanged = FALSE;
    BOOLEAN FileIsCached = FALSE;

    LONGLONG NewFileSize;
    LONGLONG NewValidDataLength;

#if 1
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

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetEndOfFileInfo...\n") );

    if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
    }

    //
    //  Get the new file size and whether this is coming from the lazy writer.
    //

    NewFileSize = ((PFILE_END_OF_FILE_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->EndOfFile.QuadPart;

    //
    //  If this attribute has been 'deleted' then return immediately.
    //

    if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

        DebugTrace( -1, Dbg, ("NtfsEndOfFileInfo:  No work to do\n") );

        return STATUS_SUCCESS;
    }

    //
    //  Save the current state of the Scb.
    //

#if 0
    NtfsSnapshotScb( IrpContext, Scb );
#endif

    //
    //  If we are called from the cache manager then we want to update the valid data
    //  length if necessary and also perform an update duplicate call if the Vcb
    //  is held.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.SetFile.AdvanceOnly) {

#ifdef SYSCACHE_DEBUG
        if (ScbIsBeingLogged( Scb ) && (Scb->CleanupCount == 0)) {
            FsRtlLogSyscacheEvent( Scb, SCE_WRITE, SCE_FLAG_SET_EOF, Scb->Header.ValidDataLength.QuadPart, Scb->ValidDataToDisk, NewFileSize );
        }
#endif

        //
        //  We only have work to do if the file is nonresident.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            //
            //  Assume this is the lazy writer and set NewValidDataLength to
            //  NewFileSize (NtfsWriteFileSizes never goes beyond what's in the
            //  Fcb).
            //

            NewValidDataLength = NewFileSize;
            NewFileSize = Scb->Header.FileSize.QuadPart;

            //
            //  If this file has a compressed stream, then we have to possibly
            //  reduce NewValidDataLength according to dirty data in the opposite
            //  stream (compressed or uncompressed) from which we were called.
            //

#ifdef  COMPRESS_ON_WIRE
            if (Scb->NonpagedScb->SegmentObjectC.SharedCacheMap != NULL) {

                LARGE_INTEGER FlushedValidData;
                PSECTION_OBJECT_POINTERS SegmentObject = &Scb->NonpagedScb->SegmentObject;

                //
                //  Assume the other stream is not cached.
                //

                FlushedValidData.QuadPart = NewValidDataLength;

                //
                //  If we were called for the compressed stream, then get flushed number
                //  for the normal stream.
                //

                if (FileObject->SectionObjectPointer != SegmentObject) {
                    if (SegmentObject->SharedCacheMap != NULL) {
                        FlushedValidData = CcGetFlushedValidData( SegmentObject, FALSE );
                    }

                //
                //  Else if we were called for the normal stream, get the flushed number
                //  for the compressed stream.
                //

                } else {
                    FlushedValidData = CcGetFlushedValidData( &Scb->NonpagedScb->SegmentObjectC, FALSE );
                }

                if (NewValidDataLength > FlushedValidData.QuadPart) {
                    NewValidDataLength = FlushedValidData.QuadPart;
                }
            }
#endif
            //
            //  NtfsWriteFileSizes will trim the new vdl down to filesize if necc. for on disk updates
            //  so we only need to explicitly trim it ourselfs for cases when its really growing
            //  but cc thinks its gone farther than it really has
            //  E.g in the activevacb case when its replaced cc considers the whole page dirty and
            //  advances valid data goal to the end of the page
            //
            //  3 pts protect us here - cc always trims valid data goal when we shrink so any
            //  callbacks indicate real data from this size file
            //  We inform cc of the new vdl on all unbuffered writes so eventually he will
            //  call us back to update for new disk sizes
            //  if mm and cc are active in a file we will let mm
            //  flush all pages beyond vdl. For the boundary page
            //  cc can flush it but we will move vdl fwd at that time as well
            //

            if ((Scb->Header.ValidDataLength.QuadPart < NewFileSize) &&
                (NewValidDataLength > Scb->Header.ValidDataLength.QuadPart)) {

#ifdef SYSCACHE_DEBUG
                if (ScbIsBeingLogged( Scb )) {
                    FsRtlLogSyscacheEvent( Scb, SCE_VDL_CHANGE, SCE_FLAG_SET_EOF, NewValidDataLength, 0, Scb->Header.ValidDataLength.QuadPart );
                }
#endif

                NewValidDataLength = Scb->Header.ValidDataLength.QuadPart;
            } //  endif advancing VDL

            //
            //  Always call writefilesizes in case on disk VDL is less than the
            //  in memory one
            //

#if 0
            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &NewValidDataLength,
                                TRUE,
                                TRUE,
                                TRUE );
#endif
        }

        //
        //  If we acquired the Vcb then do the update duplicate if necessary.
        //

        if (VcbAcquired) {

            //
            //  Now capture any file size changes in this file object back to the Fcb.
            //

#if 0
            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );
#endif

            //
            //  Update the standard information if required.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

#if 0
                NtfsUpdateStandardInformation( IrpContext, Fcb );
#endif
                ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
            }

#if 0
            NtfsCheckpointCurrentTransaction( IrpContext );
#endif

            //
            //  Update duplicated information.
            //

#if 0
            NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
#endif
        }

        //
        //  We know the file size for this Scb is now correct on disk.
        //

        NtfsAcquireFsrtlHeader( Scb );
        ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        NtfsReleaseFsrtlHeader( Scb );

    } else {

#if 1
	    if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

		    Status = NtfsPostRequest( IrpContext, Irp );

			DebugTrace( -1, Dbg, ("NtfsSetEndOfFileInfo:  Can't wait\n") );
	        return Status;
		}
#endif

        if (Ccb != NULL) {

            //
            //  Remember the source info flags in the Ccb.
            //

            IrpContext->SourceInfo = Ccb->UsnSourceInfo;
        }

        //
        //  Check for a valid input value for the file size.
        //

        if ((ULONGLONG)NewFileSize > MAXFILESIZE) {

            Status = STATUS_INVALID_PARAMETER;
            DebugTrace( -1, Dbg, ("NtfsSetEndOfFileInfo: Invalid file size -> %08lx\n", Status) );
            return Status;
        }

        //
        //  Do work to prepare for shrinking file if necc.
        //

#if 0
        if (NewFileSize < Scb->Header.FileSize.QuadPart) {

            Status = NdasNtfsPrepareToShrinkFileSize( IrpContext, FileObject, Scb, NewFileSize );
            if (Status != STATUS_SUCCESS) {

                DebugTrace( -1, Dbg, ("NtfsSetEndOfFileInfo -> %08lx\n", Status) );
                return Status;
            }
        }
#endif

        //
        //  Check if we really are changing the file size.
        //

        if (Scb->Header.FileSize.QuadPart != NewFileSize) {

            FileSizeChanged = TRUE;

            //
            //  Post the FileSize change to the Usn Journal
            //

#if 0
            NtfsPostUsnChange( IrpContext,
                               Scb,
                               ((NewFileSize > Scb->Header.FileSize.QuadPart) ?
                                 USN_REASON_DATA_EXTEND :
                                 USN_REASON_DATA_TRUNCATION) );
#endif
        }

        //
        //  It is extremely expensive to make this call on a file that is not
        //  cached, and Ntfs has suffered stack overflows in addition to massive
        //  time and disk I/O expense (CcZero data on user mapped files!).  Therefore,
        //  if no one has the file cached, we cache it here to make this call cheaper.
        //
        //  Don't create the stream file if called from FsRtlSetFileSize (which sets
        //  IRP_PAGING_IO) because mm is in the process of creating a section.
        //

        if (FileSizeChanged &&
            (Scb->NonpagedScb->SegmentObject.DataSectionObject != NULL)) {

            FileIsCached = CcIsFileCached( FileObject );

            if (!FileIsCached &&
                !FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE ) &&
                !FlagOn( Irp->Flags, IRP_PAGING_IO )) {

#if 0
                NtfsCreateInternalAttributeStream( IrpContext,
                                                   Scb,
                                                   FALSE,
                                                   &NtfsInternalUseFile[SETENDOFFILEINFO_FILE_NUMBER] );
#else
				if (FileObject->PrivateCacheMap == NULL)
					CcInitializeCacheMap( FileObject,
										  (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
										  FALSE,
										  &NtfsData.CacheManagerCallbacks,
										  Scb );
#endif
                FileIsCached = TRUE;
            }
        }

        //
        //  If this is a resident attribute we will try to keep it resident.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
            PFILE_RECORD_SEGMENT_HEADER FileRecord;

            if (FileSizeChanged) {

                //
                //  If the new file size is larger than a file record then convert
                //  to non-resident and use the non-resident code below.  Otherwise
                //  call ChangeAttributeValue which may also convert to nonresident.
                //

                NtfsInitializeAttributeContext( &AttrContext );

                try {

                    NtfsLookupAttributeForScb( IrpContext,
                                               Scb,
                                               NULL,
                                               &AttrContext );

                    //
                    //  If we are growing out of the file record then force the non-resident
                    //  path.  We especially need this for sparse files to make sure it
                    //  stays either fully allocated or fully deallocated.  QuadAlign the new
                    //  size to handle the close boundary cases.
                    //

                    FileRecord = NtfsContainingFileRecord( &AttrContext );

                    ASSERT( FileRecord->FirstFreeByte > Scb->Header.FileSize.LowPart );

                    if ((FileRecord->FirstFreeByte - Scb->Header.FileSize.QuadPart + QuadAlign( NewFileSize )) >=
                        Scb->Vcb->BytesPerFileRecordSegment) {

                        NtfsConvertToNonresident( IrpContext,
                                                  Fcb,
                                                  NtfsFoundAttribute( &AttrContext ),
                                                  (BOOLEAN) (!FileIsCached),
                                                  &AttrContext );

                    } else {

                        ULONG AttributeOffset;

                        //
                        //  We are sometimes called by MM during a create section, so
                        //  for right now the best way we have of detecting a create
                        //  section is IRP_PAGING_IO being set, as in FsRtlSetFileSizes.
                        //

                        if ((ULONG) NewFileSize > Scb->Header.FileSize.LowPart) {

                            AttributeOffset = Scb->Header.ValidDataLength.LowPart;

                        } else {

                            AttributeOffset = (ULONG) NewFileSize;
                        }

                        NtfsChangeAttributeValue( IrpContext,
                                                  Fcb,
                                                  AttributeOffset,
                                                  NULL,
                                                  (ULONG) NewFileSize - AttributeOffset,
                                                  TRUE,
                                                  FALSE,
                                                  (BOOLEAN) (!FileIsCached),
                                                  FALSE,
                                                  &AttrContext );

                        Scb->Header.FileSize.QuadPart = NewFileSize;

                        //
                        //  If the file went non-resident, then the allocation size in
                        //  the Scb is correct.  Otherwise we quad-align the new file size.
                        //

                        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                            Scb->Header.AllocationSize.LowPart = QuadAlign( Scb->Header.FileSize.LowPart );
                            Scb->Header.ValidDataLength.QuadPart = NewFileSize;
                            Scb->TotalAllocated = Scb->Header.AllocationSize.QuadPart;
#if __NDAS_NTFS_DBG__
							strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
							Scb->LastUpdateLine = __LINE__; 
#endif				

#ifdef SYSCACHE_DEBUG
                            if (ScbIsBeingLogged( Scb )) {
                                FsRtlLogSyscacheEvent( Scb, SCE_VDL_CHANGE, SCE_FLAG_SET_EOF, 0, 0, NewFileSize );
                            }
#endif

                        }

                        NonResidentPath = FALSE;
                    }

                } finally {

                    NtfsCleanupAttributeContext( IrpContext, &AttrContext );
                }

            } else {

                NonResidentPath = FALSE;
            }
        }

        //
        //  We now test if we need to modify the non-resident Eof.  We will
        //  do this in two cases.  Either we're converting from resident in
        //  two steps or the attribute was initially non-resident.  We can ignore
        //  this step if not changing the file size.
        //

        if (NonResidentPath) {

            //
            //  Now determine where the new file size lines up with the
            //  current file layout.  The two cases we need to consider are
            //  where the new file size is less than the current file size and
            //  valid data length, in which case we need to shrink them.
            //  Or we new file size is greater than the current allocation,
            //  in which case we need to extend the allocation to match the
            //  new file size.
            //

            if (NewFileSize > Scb->Header.AllocationSize.QuadPart) {

                DebugTrace( 0, Dbg, ("Adding allocation to file\n") );

                //
                //  Add either the true disk allocation or add a hole for a sparse
                //  file.
                //

                if (!FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE )) {

                    LONGLONG NewAllocationSize = NewFileSize;

                    //
                    //  If there is a compression unit then we could be in the process of
                    //  decompressing.  Allocate precisely in this case because we don't
                    //  want to leave any holes.  Specifically the user may have truncated
                    //  the file and is now regenerating it yet the clear compression operation
                    //  has already passed this point in the file (and dropped all resources).
                    //  No one will go back to cleanup the allocation if we leave a hole now.
                    //

                    if (!FlagOn( Scb->ScbState, SCB_STATE_WRITE_COMPRESSED ) &&
                        (Scb->CompressionUnit != 0)) {

                        ASSERT( FlagOn( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE ));
                        NewAllocationSize += Scb->CompressionUnit - 1;
                        ((PLARGE_INTEGER) &NewAllocationSize)->LowPart &= ~(Scb->CompressionUnit - 1);
                    }

#if 0
                    NtfsAddAllocation( IrpContext,
                                       FileObject,
                                       Scb,
                                       LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ),
                                       LlClustersFromBytes(Scb->Vcb, (NewAllocationSize - Scb->Header.AllocationSize.QuadPart)),
                                       FALSE,
                                       NULL );
#endif

                } else {

#if 1
					NDASNTFS_ASSERT( FALSE );
#endif
                    NtfsAddSparseAllocation( IrpContext,
                                             FileObject,
                                             Scb,
                                             Scb->Header.AllocationSize.QuadPart,
                                             NewFileSize - Scb->Header.AllocationSize.QuadPart );
                }

            } else {

                LONGLONG DeletePoint;

                //
                //  If this is a sparse file we actually want to leave a hole between
                //  the end of the file and the allocation size.
                //

                if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE ) &&
                    (NewFileSize < Scb->Header.FileSize.QuadPart) &&
                    ((DeletePoint = NewFileSize + Scb->CompressionUnit - 1) < Scb->Header.AllocationSize.QuadPart)) {

                    ((PLARGE_INTEGER) &DeletePoint)->LowPart &= ~(Scb->CompressionUnit - 1);

                    ASSERT( DeletePoint < Scb->Header.AllocationSize.QuadPart );

#if 0
                    NtfsDeleteAllocation( IrpContext,
                                          FileObject,
                                          Scb,
                                          LlClustersFromBytesTruncate( Scb->Vcb, DeletePoint ),
                                          LlClustersFromBytesTruncate( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ) - 1,
                                          TRUE,
                                          TRUE );
#endif
                }

                SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
            }

            NewValidDataLength = Scb->Header.ValidDataLength.QuadPart;

            //
            //  If this is a paging file, let the whole thing be valid
            //  so that we don't end up zeroing pages!  Also, make sure
            //  we really write this into the file.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

                VCN AllocatedVcns;

                AllocatedVcns = Int64ShraMod32(Scb->Header.AllocationSize.QuadPart, Scb->Vcb->ClusterShift);

                Scb->Header.ValidDataLength.QuadPart =
                NewValidDataLength = NewFileSize;
#if __NDAS_NTFS_DBG__
				strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
				Scb->LastUpdateLine = __LINE__; 
#endif				

                //
                //  If this is the paging file then guarantee that the Mcb is fully loaded.
                //

                NtfsPreloadAllocation( IrpContext, Scb, 0, AllocatedVcns );
            }

            if (NewFileSize < NewValidDataLength) {

                Scb->Header.ValidDataLength.QuadPart =
                NewValidDataLength = NewFileSize;
#if __NDAS_NTFS_DBG__
				strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
				Scb->LastUpdateLine = __LINE__; 
#endif				

#ifdef SYSCACHE_DEBUG
                if (ScbIsBeingLogged( Scb )) {
                   FsRtlLogSyscacheEvent( Scb, SCE_VDL_CHANGE, SCE_FLAG_SET_EOF, 0, 0, NewFileSize );
                }
#endif
            }

            if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK ) &&
                (NewFileSize < Scb->ValidDataToDisk)) {

                Scb->ValidDataToDisk = NewFileSize;

#ifdef SYSCACHE_DEBUG
                if (ScbIsBeingLogged( Scb )) {
                    FsRtlLogSyscacheEvent( Scb, SCE_VDD_CHANGE, SCE_FLAG_SET_EOF, 0, 0, NewFileSize );
                }
#endif

            }

#if 0
            Scb->Header.FileSize.QuadPart = NewFileSize;
#endif

            //
            //  Call our common routine to modify the file sizes.  We are now
            //  done with NewFileSize and NewValidDataLength, and we have
            //  PagingIo + main exclusive (so no one can be working on this Scb).
            //  NtfsWriteFileSizes uses the sizes in the Scb, and this is the
            //  one place where in Ntfs where we wish to use a different value
            //  for ValidDataLength.  Therefore, we save the current ValidData
            //  and plug it with our desired value and restore on return.
            //

#if 0
            ASSERT( NewFileSize == Scb->Header.FileSize.QuadPart );
            ASSERT( NewValidDataLength == Scb->Header.ValidDataLength.QuadPart );
#endif
            NtfsVerifySizes( &Scb->Header );
#if 0
            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &Scb->Header.ValidDataLength.QuadPart,
                                BooleanFlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE ),
                                TRUE,
                                TRUE );
#endif
        }

#if 1
		try {
	
			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->SessionResource, 
														 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {
	
				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
			}

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_SET_INFORMATION,
															  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

			if (secondaryRequest == NULL) {

				NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
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

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if(Status != STATUS_SUCCESS) {
	
				ASSERT( NDASNTFS_BUG );
				secondaryRequest = NULL;
				try_return( Status = STATUS_IO_DEVICE_ERROR );	
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );

				DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			Status = Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
			//ASSERT( ndfsWinxpReplytHeader->Information == 0);
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

			ASSERT( NT_SUCCESS(Status) );

			if (NT_SUCCESS(Status)) {
				
				if (ndfsWinxpReplytHeader->AllocationSize != (UINT64)Scb->Header.AllocationSize.QuadPart) {

					PNDFS_NTFS_MCB_ENTRY	mcbEntry;
					ULONG			index;

					BOOLEAN			lookupResut;
					VCN				vcn;
					LCN				lcn;
					LCN				startingLcn;
					LONGLONG		clusterCount;

					ASSERT( NtfsIsExclusiveScb(Scb) );

					mcbEntry = (PNDFS_NTFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

					for (index=0, vcn=0; index < ndfsWinxpReplytHeader->NumberOfMcbEntry; index++, mcbEntry++) {

						ASSERT( mcbEntry->Vcn == vcn );

						lookupResut = NtfsLookupNtfsMcbEntry( &Scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
					
						if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, Scb->Header.AllocationSize.QuadPart)) {

							ASSERT( lookupResut == TRUE );

							if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, ndfsWinxpReplytHeader->AllocationSize)) {
							
								ASSERT( startingLcn == lcn );
								ASSERT( vcn == mcbEntry->Vcn );
								ASSERT( lcn == mcbEntry->Lcn );
								ASSERT( clusterCount <= mcbEntry->ClusterCount );
							
								if (clusterCount < mcbEntry->ClusterCount) {

									NtfsAddNtfsMcbEntry( &Scb->Mcb, 
														 mcbEntry->Vcn, 
														 mcbEntry->Lcn, 
														 (LONGLONG)mcbEntry->ClusterCount, 
														 FALSE );
	
									lookupResut = NtfsLookupNtfsMcbEntry( &Scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );

									ASSERT( lookupResut == TRUE );
									ASSERT( startingLcn == lcn );
									ASSERT( vcn == mcbEntry->Vcn );
									ASSERT( lcn == mcbEntry->Lcn );
									ASSERT( clusterCount == mcbEntry->ClusterCount );
								}
							}
					
						} else { 

							ASSERT( lookupResut == FALSE || lcn == UNUSED_LCN );

							NtfsAddNtfsMcbEntry( &Scb->Mcb, 
												 mcbEntry->Vcn, 
												 mcbEntry->Lcn, 
												 (LONGLONG)mcbEntry->ClusterCount, 
												 FALSE );
						}

						vcn += mcbEntry->ClusterCount;
					}
			
					ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == ndfsWinxpReplytHeader->AllocationSize );

					if (Scb->Header.AllocationSize.QuadPart < ndfsWinxpReplytHeader->AllocationSize) {

						ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose );

						Scb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

#if 0
						if (FileObject->SectionObjectPointer->DataSectionObject != NULL && FileObject->PrivateCacheMap == NULL) {

							CcInitializeCacheMap( FileObject,
												  (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
												  FALSE,
												  &NtfsData.CacheManagerCallbacks,
												  Scb );
						}
#endif

						NtfsSetBothCacheSizes( FileObject,
											   (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
											   Scb );

						SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
					}

					if (Scb->Header.AllocationSize.QuadPart > ndfsWinxpReplytHeader->AllocationSize) {

						Scb->Header.AllocationSize.QuadPart = ndfsWinxpReplytHeader->AllocationSize;

						Scb->Header.FileSize.QuadPart = ndfsWinxpReplytHeader->FileSize;

						DebugTrace(0, Dbg, ("NtfsSetEndOfFileInfo scb->Header.FileSize.QuadPart = %I64x, scb->Header.ValidDataLength.QuadPart = %I64x\n", 
											 Scb->Header.FileSize.QuadPart, Scb->Header.ValidDataLength.QuadPart) );
			
						NtfsRemoveNtfsMcbEntry( &Scb->Mcb, 
												LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, Scb->Header.AllocationSize.QuadPart), 
												0xFFFFFFFF );


						NtfsSetBothCacheSizes( FileObject,
											   (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
											   Scb );
					}
				}

				ASSERT( NewFileSize == ndfsWinxpReplytHeader->FileSize );
				Scb->Header.FileSize.QuadPart = ndfsWinxpReplytHeader->FileSize;

				if (Scb->Header.ValidDataLength.QuadPart > Scb->Header.FileSize.QuadPart) {

					Scb->Header.ValidDataLength.QuadPart = Scb->Header.FileSize.QuadPart;

#if __NDAS_NTFS_DBG__
					strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
					Scb->LastUpdateLine = __LINE__; 
#endif				
				}

				if (Scb->ValidDataToDisk > Scb->Header.FileSize.QuadPart) {

					Scb->ValidDataToDisk = Scb->Header.FileSize.QuadPart;
				}
			}

#if DBG
			{
				BOOLEAN			lookupResut;
				VCN				vcn;
				LCN				lcn;
				LCN				startingLcn;
				LONGLONG		clusterCount;

				vcn = 0;
				while (1) {
						
					lookupResut = NtfsLookupNtfsMcbEntry( &Scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
					if (lookupResut == FALSE || lcn == UNUSED_LCN)
						break;

					vcn += clusterCount;
				}

				ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == Scb->Header.AllocationSize.QuadPart );
			}

#endif

try_exit:  NOTHING;

		} finally {
	
			if(secondaryRequest)
				DereferenceSecondaryRequest( secondaryRequest );

			if( secondarySessionResourceAcquired == TRUE ) {

				SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
			}
		}

#endif

        //
        //  If the file size changed then mark this file object as having changed the size.
        //

        if (FileSizeChanged) {

            SetFlag( FileObject->Flags, FO_FILE_SIZE_CHANGED );
        }

        //
        //  Always mark the data stream as modified.
        //

        if (ARGUMENT_PRESENT( Ccb )) {

            SetFlag( Ccb->Flags,
                     (CCB_FLAG_UPDATE_LAST_MODIFY |
                      CCB_FLAG_UPDATE_LAST_CHANGE |
                      CCB_FLAG_SET_ARCHIVE) );

        } else {

            SetFlag( FileObject->Flags, FO_FILE_MODIFIED );
        }

        //
        //  Now capture any file size changes in this file object back to the Fcb.
        //

#if 0
        NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, VcbAcquired );
#endif

        //
        //  Update the standard information if required.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

#if 0
            NtfsUpdateStandardInformation( IrpContext, Fcb );
#endif
            ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        }

        //
        //  We know we wrote out any changes to the file size above so clear the
        //  flag in the Scb to check the attribute size.  This will save us from doing
        //  this unnecessarily at cleanup.
        //

        if (FileSizeChanged) {

            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        }

#if 0
        NtfsCheckpointCurrentTransaction( IrpContext );
#endif

        //
        //  Update duplicated information.
        //

        if (VcbAcquired) {

#if 0
            NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
#endif
        }

        if (CcIsFileCached( FileObject )) {

            //
            //  We want to checkpoint the transaction if there is one active.
            //

#if 1
			ASSERT( IrpContext->TransactionId == 0 );
#else
            if (IrpContext->TransactionId != 0) {

                NtfsCheckpointCurrentTransaction( IrpContext );
            }
#endif

#ifdef SYSCACHE_DEBUG
            if (ScbIsBeingLogged( Scb )) {
                FsRtlLogSyscacheEvent( Scb, SCE_CC_SET_SIZE, SCE_FLAG_SET_EOF, 0, Scb->Header.ValidDataLength.QuadPart, Scb->Header.FileSize.QuadPart );
            }
#endif

            //
            //  Cache map should still exist or we won't purge the data section
            //

            ASSERT( FileObject->SectionObjectPointer->SharedCacheMap != NULL );
            NtfsSetBothCacheSizes( FileObject,
                                   (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
                                   Scb );

            //
            //  Clear out the write mask on truncates to zero.
            //

#ifdef SYSCACHE
            if ((Scb->Header.FileSize.QuadPart == 0) && FlagOn(Scb->ScbState, SCB_STATE_SYSCACHE_FILE) &&
                (Scb->ScbType.Data.WriteMask != NULL)) {
                RtlZeroMemory(Scb->ScbType.Data.WriteMask, (((0x2000000) / PAGE_SIZE) / 8));
            }
#endif

            //
            //  Now cleanup the stream we created if there are no more user
            //  handles.
            //

            if ((Scb->CleanupCount == 0) && (Scb->FileObject != NULL)) {
                NtfsDeleteInternalAttributeStream( Scb, FALSE, FALSE );
            }
        }
    }

    Status = STATUS_SUCCESS;

    DebugTrace( -1, Dbg, ("NtfsSetEndOfFileInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NdasNtfsSetValidDataLengthInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set valid data length information function.
    Notes: we interact with CC but do not initiate caching ourselves. This is
    only possible if the file is not mapped so we can do purges on the section.

    Also the filetype check that restricts this to fileopens only is done in the
    CommonSetInformation call.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Ccb attached to the file. Contains cached privileges of opener


Return Value:

    NTSTATUS - The status of the operation

--*/

{
    LONGLONG NewValidDataLength;
    LONGLONG NewFileSize;
    PIO_STACK_LOCATION IrpSp;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  User must have manage volume privilege to explicitly tweak the VDL
    //

    if (!FlagOn( Ccb->AccessFlags, MANAGE_VOLUME_ACCESS)) {
        return STATUS_PRIVILEGE_NOT_HELD;
    }

    //
    //  We don't support this call for compressed or sparse files
    //

    if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK | ATTRIBUTE_FLAG_SPARSE)) {
        return STATUS_INVALID_PARAMETER;
    }

    NewValidDataLength = ((PFILE_VALID_DATA_LENGTH_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->ValidDataLength.QuadPart;
    NewFileSize = Scb->Header.FileSize.QuadPart;

    //
    //  VDL can only move forward
    //

    if ((NewValidDataLength < Scb->Header.ValidDataLength.QuadPart) ||
        (NewValidDataLength > NewFileSize) ||
        (NewValidDataLength < 0)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    //  We only have work to do if the file is nonresident.
    //

    if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

        //
        //  We can't change the VDL without being able to purge. This should stay
        //  constant since we own everything exclusive
        //

        if (!MmCanFileBeTruncated( &Scb->NonpagedScb->SegmentObject, &Li0 )) {
            return STATUS_USER_MAPPED_FILE;
        }

#if 0
        NtfsSnapshotScb( IrpContext, Scb );
#endif

        //
        //  Flush old data out and purge the cache so we can see new data
        //

        NtfsFlushAndPurgeScb( IrpContext, Scb, NULL );

        //
        //  update the scb
        //

        Scb->Header.ValidDataLength.QuadPart = NewValidDataLength;
#if __NDAS_NTFS_DBG__
		strncpy(Scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
		Scb->LastUpdateLine = __LINE__; 
#endif				
        if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {
            Scb->ValidDataToDisk = NewValidDataLength;
        }

#ifdef SYSCACHE_DEBUG
        if (ScbIsBeingLogged( Scb )) {
            FsRtlLogSyscacheEvent( Scb, SCE_VDL_CHANGE, SCE_FLAG_SET_VDL, 0, 0, NewValidDataLength );
        }
#endif

        ASSERT( IrpContext->CleanupStructure != NULL );
#if 0
        NtfsWriteFileSizes( IrpContext,
                            Scb,
                            &NewValidDataLength,
                            TRUE,
                            TRUE,
                            TRUE );
#endif

        //
        //  Now capture any file size changes in this file object back to the Fcb.
        //

#if 0
        NtfsUpdateScbFromFileObject( IrpContext, IrpSp->FileObject, Scb, FALSE );
#endif

        //
        //  Inform CC of the new values
        //

        NtfsSetBothCacheSizes( IrpSp->FileObject,
                               (PCC_FILE_SIZES)&Scb->Header.AllocationSize,
                               Scb );

        //
        //  We know the file size for this Scb is now correct on disk.
        //

        NtfsAcquireFsrtlHeader( Scb );
        ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        NtfsReleaseFsrtlHeader( Scb );

        //
        //  Post a usn record
        //

#if 0
        NtfsPostUsnChange( IrpContext, Scb, USN_REASON_DATA_OVERWRITE );
        NtfsWriteUsnJournalChanges( IrpContext );
#endif
    }

    return STATUS_SUCCESS;
}

#endif
