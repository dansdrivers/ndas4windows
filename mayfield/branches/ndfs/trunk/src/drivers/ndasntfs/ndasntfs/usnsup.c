/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    UsnSup.c

Abstract:

    This module implements the Usn Journal support routines for NtOfs

Author:

    Tom Miller      [TomM]          1-Dec-1996

Revision History:

--*/

#include "NtfsProc.h"
#include "lockorder.h"

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('UFtN')

#define GENERATE_CLOSE_RECORD_LIMIT     (200)

UNICODE_STRING $Max = CONSTANT_UNICODE_STRING( L"$Max" );

RTL_GENERIC_COMPARE_RESULTS
NtfsUsnTableCompare (
    IN PRTL_GENERIC_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    );

PVOID
NtfsUsnTableAllocate (
    IN PRTL_GENERIC_TABLE Table,
    CLONG ByteSize
    );

VOID
NtfsUsnTableFree (
    IN PRTL_GENERIC_TABLE Table,
    PVOID Buffer
    );

VOID
NtfsCancelReadUsnJournal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
NtfsCancelDeleteUsnJournal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NtfsDeleteUsnWorker (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    );

BOOLEAN
NtfsValidateUsnPage (
    IN PUSN_RECORD UsnRecord,
    IN USN PageUsn,
    IN USN *UserStartUsn OPTIONAL,
    IN LONGLONG UsnFileSize,
    OUT PBOOLEAN ValidUserStartUsn OPTIONAL,
    OUT USN *NextUsn
    );

//
//  VOID
//  NtfsAdvanceUsnJournal (
//  PVCB Vcb,
//  PUSN_JOURNAL_INSTANCE UsnJournalInstance,
//  LONGLONG OldSize,
//  PBOOLEAN NewMax
//  );
//

#define NtfsAdvanceUsnJournal(V,I,SZ,M)   {                                 \
    ULONG _TempUlong;                                                       \
    _TempUlong = USN_PAGE_BOUNDARY;                                         \
    if (USN_PAGE_BOUNDARY < (V)->BytesPerCluster) {                         \
        _TempUlong = (V)->BytesPerCluster;                                  \
    }                                                                       \
    (I)->LowestValidUsn = SZ + _TempUlong - 1;                              \
    ((PLARGE_INTEGER) &(I)->LowestValidUsn)->LowPart &= ~(_TempUlong - 1);  \
    KeQuerySystemTime( (PLARGE_INTEGER) &(I)->JournalId );                  \
    *(M) = TRUE;                                                            \
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsDeleteUsnJournal)
#pragma alloc_text(PAGE, NtfsDeleteUsnSpecial)
#pragma alloc_text(PAGE, NtfsDeleteUsnWorker)
#pragma alloc_text(PAGE, NtfsPostUsnChange)
#pragma alloc_text(PAGE, NtfsQueryUsnJournal)
#pragma alloc_text(PAGE, NtfsReadUsnJournal)
#pragma alloc_text(PAGE, NtfsSetupUsnJournal)
#pragma alloc_text(PAGE, NtfsTrimUsnJournal)
#pragma alloc_text(PAGE, NtfsUsnTableCompare)
#pragma alloc_text(PAGE, NtfsUsnTableAllocate)
#pragma alloc_text(PAGE, NtfsUsnTableFree)
#pragma alloc_text(PAGE, NtfsValidateUsnPage)
#pragma alloc_text(PAGE, NtfsWriteUsnJournalChanges)
#endif


NTSTATUS
NtfsReadUsnJournal (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN BOOLEAN ProbeInput
    )

/*++

Routine Description:

    This routine reads records filtered from the Usn journal.

Arguments:

    IrpContext - Only optional if we are being called to cancel an async
                 request.

    Irp - request being serviced

    ProbeInput - Indicates if we should probe the user input buffer.  We also
        call this routine internally and don't want to probe in that case.

Return Value:

    NTSTATUS - The return status for the operation.
    STATUS_PENDING - if asynch Irp queued for later completion.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PUSN_RECORD UsnRecord;
    USN_RECORD UNALIGNED *OutputUsnRecord;
    PVOID UserBuffer;
    LONGLONG ViewLength;
    ULONG RemainingUserBuffer, BytesUsed;
    MAP_HANDLE MapHandle;

    READ_USN_JOURNAL_DATA CapturedData;
    PSCB UsnJournal;
    ULONG JournalAcquired = FALSE;
    ULONG AccessingUserBuffer = FALSE;
    ULONG DecrementReferenceCount = FALSE;
    ULONG VcbAcquired = FALSE;
    ULONG Wait;
    ULONG OriginalWait;

    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PAGED_CODE();

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Extract and decode the file object and check for type of open.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

#ifdef __ND_NTFS_SECONDARY__
	if (!IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject))
#endif
	if ((Ccb == NULL) || !FlagOn( Ccb->AccessFlags, MANAGE_VOLUME_ACCESS)) {

        ASSERT( ProbeInput );
        NtfsCompleteRequest( IrpContext, Irp, STATUS_ACCESS_DENIED );
        return STATUS_ACCESS_DENIED;
    }

    //
    //  This request must be able to wait for resources.  Set WAIT to TRUE.
    //

    Wait = TRUE;
    if (ProbeInput && !FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

        Wait = FALSE;
    }

    NtOfsInitializeMapHandle( &MapHandle );
    UserBuffer = NtfsMapUserBuffer( Irp );

    try {

        //
        //  We always want to be able to wait for resources in this routine but need to be able
        //  to restore the original wait value in the Irp.  After this the original wait will
        //  have only the wait flag set and then only if it originally wasn't set.  In clean
        //  up we just need to clear the irp context flags using this mask.
        //

        OriginalWait = (IrpContext->State ^ IRP_CONTEXT_STATE_WAIT) & IRP_CONTEXT_STATE_WAIT;

        SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );

        //
        //  Detect if we fail while accessing the input buffer.
        //

        try {

            AccessingUserBuffer = TRUE;

            //
            //  Probe the input buffer if not in kernel mode and we haven't already done so.
            //

            if (Irp->RequestorMode != KernelMode) {

                if (ProbeInput) {

                    ProbeForRead( IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                                  IrpSp->Parameters.FileSystemControl.InputBufferLength,
                                  sizeof( ULONG ));
                }

                //
                //  Probe the output buffer if we haven't locked it down yet.
                //  Capture the JournalData from the unsafe user buffer.
                //

                if (Irp->MdlAddress == NULL) {

                    ProbeForWrite( UserBuffer, IrpSp->Parameters.FileSystemControl.OutputBufferLength, sizeof( ULONG ));
                }
            }

#ifdef __ND_NTFS_SECONDARY__

			if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) {
	
				IrpContext->InputBuffer	 = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
				IrpContext->outputBuffer = UserBuffer;
				Status = STATUS_SUCCESS;
				leave;
			}
#endif

            //
            //  Acquire the Vcb to serialize journal operations with delete journal and dismount.
            //  Only do this if are being called directly by the user.
            //

            if (ProbeInput) {

                VcbAcquired = NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

                if (!FlagOn( Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE )) {

                    UsnJournal = NULL;

                } else {

                    UsnJournal = Vcb->UsnJournal;
                }

            } else {

                UsnJournal = Vcb->UsnJournal;
            }

            //
            //  Make sure no one is deleting the journal.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_USN_DELETE )) {

                Status = STATUS_JOURNAL_DELETE_IN_PROGRESS;
                leave;
            }

            //
            //  Also check that the version is still active.
            //

            if (UsnJournal == NULL) {

                Status = STATUS_JOURNAL_NOT_ACTIVE;
                leave;
            }

            //
            //  Check that the buffer sizes meet our minimum needs.
            //

            if (IrpSp->Parameters.FileSystemControl.InputBufferLength < sizeof( READ_USN_JOURNAL_DATA )) {

                Status = STATUS_INVALID_USER_BUFFER;
                leave;

            } else {

                RtlCopyMemory( &CapturedData,
                               IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                               sizeof( READ_USN_JOURNAL_DATA ));

                //
                //  Check that the user is querying with the correct journal ID.
                //

                if (CapturedData.UsnJournalID != Vcb->UsnJournalInstance.JournalId) {

                    Status = STATUS_INVALID_PARAMETER;
                    leave;
                }
            }

            //
            //  Check that the output buffer can hold at least one USN.
            //

            if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < sizeof( USN )) {

                Status = STATUS_BUFFER_TOO_SMALL;
                leave;
            }

            AccessingUserBuffer = FALSE;

            //
            //  Set up for filling output records
            //

            RemainingUserBuffer = IrpSp->Parameters.FileSystemControl.OutputBufferLength - sizeof(USN);
            OutputUsnRecord = (PUSN_RECORD) Add2Ptr( UserBuffer, sizeof(USN) );
            BytesUsed = sizeof(USN);

            NtfsAcquireResourceShared( IrpContext, UsnJournal, TRUE );
            JournalAcquired = TRUE;

            if (VcbAcquired) {

                NtfsReleaseVcb( IrpContext, Vcb );
                VcbAcquired = FALSE;
            }

            //
            //  Verify the volume is mounted.
            //

            if (FlagOn( UsnJournal->ScbState, SCB_STATE_VOLUME_DISMOUNTED )) {
                Status = STATUS_VOLUME_DISMOUNTED;
                leave;
            }

            //
            //  If 0 was specified as the Usn, then translate that to the first record
            //  in the Usn journal.
            //

            if (CapturedData.StartUsn == 0) {
                CapturedData.StartUsn = Vcb->FirstValidUsn;
            }

            //
            //  Loop here until he gets some data, if that is what the caller wants.
            //

            do {

                //
                //  Make sure he is within the stream.
                //

                if (CapturedData.StartUsn < Vcb->FirstValidUsn) {
                    CapturedData.StartUsn = Vcb->FirstValidUsn;
                    Status = STATUS_JOURNAL_ENTRY_DELETED;
                    break;
                }

                //
                //  Make sure he is within the stream.
                //

                if (CapturedData.StartUsn >= UsnJournal->Header.FileSize.QuadPart) {

                    //
                    //  If he wants to wait for data, then wait here.
                    //
                    //  If an asynchronous request has
                    //  met its wakeup condition, then this Irp will not be the same as the
                    //  Originating Irp, and we do not want to give him a second chance since
                    //  this could cause us to loop in NtOfsPostNewLength.  (Basically the only
                    //  case where this could happen anyway is if he gave us a bogus StartUsn
                    //  which is too high.)
                    //

                    if (CapturedData.BytesToWaitFor != 0) {

                        //
                        //  Make sure the journal doesn't get deleted while
                        //  this Irp is outstanding.
                        //

                        InterlockedIncrement( &UsnJournal->CloseCount );
                        DecrementReferenceCount = TRUE;

                        //
                        //  If the caller does not want to wait, then just queue his
                        //  Irp to be completed when sufficient bytes come in.  If we were
                        //  called for another Irp, then do the same, since we know that
                        //  was another async Irp.
                        //

                        if (!Wait || (Irp != IrpContext->OriginatingIrp)) {

                            //
                            //  Now set up our wait block, capturing the user's parameters.
                            //  Update the Irp to say where the input parameters are now.
                            //

                            Status = NtfsHoldIrpForNewLength( IrpContext,
                                                              UsnJournal,
                                                              Irp,
                                                              CapturedData.StartUsn + CapturedData.BytesToWaitFor,
                                                              NtfsCancelReadUsnJournal,
                                                              &CapturedData,
                                                              &IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                                                              sizeof( READ_USN_JOURNAL_DATA ));

                            //
                            //  If pending then someone else will decrement the reference count.
                            //

                            if (Status == STATUS_PENDING) {

                                DecrementReferenceCount = FALSE;
                            }

                            leave;
                        }

                        //
                        //  We can safely release the resource.  Our reference on the Scb above
                        //  will keep it from being deleted.
                        //

                        NtfsReleaseResource( IrpContext, UsnJournal );
                        JournalAcquired = FALSE;

                        FsRtlExitFileSystem();

                        Status = NtOfsWaitForNewLength( UsnJournal,
                                                        CapturedData.StartUsn + CapturedData.BytesToWaitFor,
                                                        FALSE,
                                                        Irp,
                                                        NtfsCancelReadUsnJournal,
                                                        ((CapturedData.Timeout != 0) ?
                                                         (PLARGE_INTEGER) &CapturedData.Timeout :
                                                         NULL) );

                        FsRtlEnterFileSystem();

                        //
                        //  Get out in the error case.
                        //

                        if (Status != STATUS_SUCCESS) {

                            leave;
                        }

                        //
                        //  Acquire the resource to proceed with the request.
                        //

                        NtfsAcquireResourceShared( IrpContext, UsnJournal, TRUE );
                        JournalAcquired = TRUE;

                        //
                        //  Decrement our reference on the Scb.
                        //

                        InterlockedDecrement( &UsnJournal->CloseCount );
                        DecrementReferenceCount = FALSE;

                        //
                        //  The journal may have been deleted while we weren't holding
                        //  anything.
                        //

                        if (UsnJournal != UsnJournal->Vcb->UsnJournal) {

                            if (FlagOn( UsnJournal->Vcb->VcbState, VCB_STATE_USN_DELETE )) {
                                Status = STATUS_JOURNAL_DELETE_IN_PROGRESS;
                            } else {
                                Status = STATUS_JOURNAL_NOT_ACTIVE;
                            }
                            leave;
                        }

                        ASSERT( Status == STATUS_SUCCESS );

                        //
                        //  **** Get out if we are shutting down the volume.
                        //

                        //  if (ShuttingDown) {
                        //      Status = STATUS_TOO_LATE;
                        //      leave;
                        //  }

                    //
                    //  Otherwise, get out.  Note, we may have processed a number of records
                    //  that did not match his filter criteria, so we will return success, so
                    //  we can at least give him an updated Usn so we do not have to skip over
                    //  all those records again.
                    //

                    } else {

                        break;
                    }
                }

                //
                //  Loop through as many views as required to fill the output buffer.
                //

                while ((RemainingUserBuffer != 0) && (CapturedData.StartUsn < UsnJournal->Header.FileSize.QuadPart)) {

                    LONGLONG BiasedStartUsn;
                    BOOLEAN ValidUserStartUsn;
                    USN NextUsn;
                    ULONG RecordSize;

                    //
                    //  Calculate length to process in this view.
                    //

                    ViewLength = UsnJournal->Header.FileSize.QuadPart - CapturedData.StartUsn;
                    if (ViewLength > (VACB_MAPPING_GRANULARITY - (ULONG)(CapturedData.StartUsn & (VACB_MAPPING_GRANULARITY - 1)))) {
                        ViewLength = VACB_MAPPING_GRANULARITY - (ULONG)(CapturedData.StartUsn & (VACB_MAPPING_GRANULARITY - 1));
                    }

                    //
                    //  Map the view containing the desired Usn.
                    //

                    BiasedStartUsn = CapturedData.StartUsn - Vcb->UsnCacheBias;
                    NtOfsMapAttribute( IrpContext, UsnJournal, BiasedStartUsn, (ULONG)ViewLength, (PVOID *)&UsnRecord, &MapHandle );

                    //
                    //  For each page in the view we want to validate the page and return the records
                    //  within the page starting at the user's current usn.
                    //

                    do {

                        //
                        //  Validate the records on the entire page are valid.
                        //

                        if (!NtfsValidateUsnPage( (PUSN_RECORD) BlockAlignTruncate( ((ULONG_PTR) UsnRecord), USN_PAGE_SIZE ),
                                                  BlockAlignTruncate( CapturedData.StartUsn, USN_PAGE_SIZE ),
                                                  &CapturedData.StartUsn,
                                                  UsnJournal->Header.FileSize.QuadPart,
                                                  &ValidUserStartUsn,
                                                  &NextUsn )) {

                            //
                            //  Simply fail the request with bad data.
                            //

                            Status = STATUS_DATA_ERROR;
                            leave;
                        }

                        //
                        //  If the user gave us an incorrect Usn then fail the request.
                        //

                        if (!ValidUserStartUsn) {

                            Status = STATUS_INVALID_PARAMETER;
                            leave;
                        }

                        //
                        //  Now loop to process this page.  We know the Usn values which exist on the page and
                        //  there are no checks for valid data needed.
                        //

                        while (CapturedData.StartUsn < NextUsn) {

                            RecordSize = UsnRecord->RecordLength;

                            //
                            //  Only recognize version 2 records.
                            //

                            if (FlagOn( UsnRecord->Reason, CapturedData.ReasonMask ) &&
                                (!CapturedData.ReturnOnlyOnClose || FlagOn( UsnRecord->Reason, USN_REASON_CLOSE )) &&
                                (UsnRecord->MajorVersion == 2)) {

                                if (RecordSize > RemainingUserBuffer) {
                                    RemainingUserBuffer = 0;
                                    break;
                                }

                                //
                                //  Copy the data back to the unsafe user buffer.
                                //

                                AccessingUserBuffer = TRUE;

                                //
                                //  Copy directly if the version numbers match.
                                //

                                RtlCopyMemory( OutputUsnRecord, UsnRecord, RecordSize );

                                AccessingUserBuffer = FALSE;

                                RemainingUserBuffer -= RecordSize;
                                BytesUsed += RecordSize;
                                OutputUsnRecord = Add2Ptr( OutputUsnRecord, RecordSize );
                            }

                            CapturedData.StartUsn += RecordSize;
                            UsnRecord = Add2Ptr( UsnRecord, RecordSize );

                            //
                            //  The view length should already account for record size.
                            //

                            ASSERT( ViewLength >= RecordSize );
                            ViewLength -= RecordSize;
                        }

                        //
                        //  Break out if the users buffer is empty.
                        //

                        if (RemainingUserBuffer == 0) {

                            break;
                        }

                        //
                        //  We finished the current page.  Now move to the next page.
                        //  Figure out how many bytes remain on this page.
                        //  If the next offset is the start of the next page then make sure
                        //  to mask off the page size bits again.
                        //

                        RecordSize = BlockOffset( USN_PAGE_SIZE - BlockOffset( (ULONG) NextUsn, USN_PAGE_SIZE ),
                                                  USN_PAGE_SIZE );

                        if (RecordSize > ViewLength) {

                            RecordSize = (ULONG) ViewLength;
                        }

                        UsnRecord = Add2Ptr( UsnRecord, RecordSize );
                        CapturedData.StartUsn += RecordSize;
                        ViewLength -= RecordSize;

                    } while (ViewLength != 0);

                    NtOfsReleaseMap( IrpContext, &MapHandle );
                }

            } while ((RemainingUserBuffer != 0) && (BytesUsed == sizeof(USN)));

            Irp->IoStatus.Information = BytesUsed;

            //
            //  Set the returned Usn.  Move to the start of the next page if
            //  the next record won't fit on this page.
            //

            AccessingUserBuffer = TRUE;
            *(USN *)UserBuffer = CapturedData.StartUsn;
            AccessingUserBuffer = FALSE;

        } except(EXCEPTION_EXECUTE_HANDLER) {

            Status = GetExceptionCode();

            //
            //  Restore the original wait state back into the IrpContext.
            //

            ClearFlag( IrpContext->State, OriginalWait );

            if (FsRtlIsNtstatusExpected( Status )) {

                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );

            } else {

                ExRaiseStatus( AccessingUserBuffer ? STATUS_INVALID_USER_BUFFER : Status );
            }
        }

    } finally {

        NtOfsReleaseMap( IrpContext, &MapHandle );

        if (JournalAcquired) {
            NtfsReleaseResource( IrpContext, UsnJournal );
        }

        if (DecrementReferenceCount) {

            InterlockedDecrement( &UsnJournal->CloseCount );
        }

        if (VcbAcquired) {

            NtfsReleaseVcb( IrpContext, Vcb );
        }
    }

    //
    //  Complete the request, unless we've marked this Irp as pending and we plan to complete
    //  it later.  If the Irp is not the originating Irp then it belongs to another request
    //  and we don't want to complete it.
    //

    //
    //  Restore the original wait flag back into the IrpContext.
    //

    ClearFlag( IrpContext->State, OriginalWait );

    ASSERT( (Status == STATUS_PENDING) || (Irp->CancelRoutine == NULL) );

#ifdef __ND_NTFS_SECONDARY__
	if (!IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject))
#endif
	NtfsCompleteRequest( (Irp == IrpContext->OriginatingIrp) ? IrpContext : NULL,
                         (Status != STATUS_PENDING) ? Irp : NULL,
                         Status );

    return Status;
}


ULONG
NtfsPostUsnChange (
    IN PIRP_CONTEXT IrpContext,
    IN PVOID ScbOrFcb,
    IN ULONG Reason
    )

/*++

Routine Description:

    This routine is called to post a set of changes to a file.  A change is
    only posted if at least one reason in the Reason mask is not already set
    in either the Fcb or the IrpContext or if we are changing the source info
    reasons in the Fcb.

Arguments:

    ScbOrFcb - Supplies the file for which a change is being posted.  If reason contains
               USN_REASON_DATA_xxx reasons, then it must be an Scb, because we transform
               the code for named streams and do other special handling.

    Reason - Supplies a mask of reasons for which a change is being posted.

Return Value:

    Nonzero if changes are actually posted from this or a previous call

--*/

{
    PLCB Lcb;
    PFCB_USN_RECORD FcbUsnRecord;
    BOOLEAN Found;
    PFCB Fcb;
    PSCB Scb = NULL;
    ULONG NewReasons;
    ULONG RemovedSourceInfo;
    PUSN_FCB ThisUsn;
    BOOLEAN LockedFcb = FALSE;
    BOOLEAN AcquiredFcb = FALSE;

    //
    //  Assume we got an Fcb.
    //

    Fcb = (PFCB)ScbOrFcb;

#if (defined __ND_NTFS_DBG__ && defined __ND_NTFS_SECONDARY__) 
	if (FlagOn(Fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY))
		ASSERT( FALSE );
#endif

    ASSERT( !(Reason & (USN_REASON_DATA_OVERWRITE | USN_REASON_DATA_EXTEND | USN_REASON_DATA_TRUNCATION)) ||
            (NTFS_NTC_FCB != Fcb->NodeTypeCode) );

    //
    //  Switch if we got an Scb
    //

    if (Fcb->NodeTypeCode != NTFS_NTC_FCB) {

        ASSERT_SCB(Fcb);

        Scb = (PSCB)ScbOrFcb;
        Fcb = Scb->Fcb;
    }

    //
    //  We better be holding some resource.
    //

    ASSERT( !IsListEmpty( &IrpContext->ExclusiveFcbList ) ||
            ((Fcb->PagingIoResource != NULL) && NtfsIsSharedFcbPagingIo( Fcb )) ||
            NtfsIsSharedFcb( Fcb ) );

    //
    //  If there is a Usn Journal and its not a system file setup the memory structures
    //  to hold the usn reasons
    //

    ThisUsn = &IrpContext->Usn;

    if (FlagOn( Fcb->Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE ) &&
        !FlagOn( Fcb->FcbState, FCB_STATE_SYSTEM_FILE )) {

        //
        //  First see if we have a Usn record structure already for this file.  We might need
        //  the whole usn record or simply the name.  If this is the RENAME_NEW_NAME record
        //  then find then name again as well.
        //

        if ((Fcb->FcbUsnRecord == NULL) ||
            !FlagOn( Fcb->FcbState, FCB_STATE_VALID_USN_NAME ) ||
            FlagOn( Reason, USN_REASON_RENAME_NEW_NAME )) {

            ULONG SizeToAllocate;
            ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
            PFILE_NAME FileName = NULL;

            NtfsInitializeAttributeContext( &AttributeContext );

            try {

                //
                //  First we have to find the designated file name.  If we are lucky
                //  it is in an Lcb.  We cannot do this in the case of rename, because
                //  the in-memory stuff is not fixed up yet.
                //

                if (!FlagOn( Reason, USN_REASON_RENAME_NEW_NAME )) {

                    Lcb = (PLCB)CONTAINING_RECORD( Fcb->LcbQueue.Flink, LCB, FcbLinks );
                    while (&Lcb->FcbLinks.Flink != &Fcb->LcbQueue.Flink) {

                        //
                        //  If this is the designated file name, then we can get out pointing
                        //  to the FILE_NAME in the Lcb.
                        //

                        if (FlagOn( Lcb->LcbState, LCB_STATE_DESIGNATED_LINK )) {
                            FileName = (PFILE_NAME)&Lcb->ParentDirectory;
                            break;
                        }

                        //
                        //  Advance to next Lcb.
                        //

                        Lcb = (PLCB)CONTAINING_RECORD( Lcb->FcbLinks.Flink, LCB, FcbLinks );
                    }
                }

                //
                //  If we did not find the file name the easy way, then we have to go
                //  get it.
                //

                if (FileName == NULL) {

                    //
                    //  Acquire some synchronization against the filerecord
                    //  

                    NtfsAcquireResourceShared( IrpContext, Fcb, TRUE );
                    AcquiredFcb = TRUE;

                    //
                    //  Now scan for the filename attribute we need.
                    //

                    Found = NtfsLookupAttributeByCode( IrpContext,
                                                       Fcb,
                                                       &Fcb->FileReference,
                                                       $FILE_NAME,
                                                       &AttributeContext );

                    while (Found) {

                        FileName = (PFILE_NAME)NtfsAttributeValue( NtfsFoundAttribute(&AttributeContext) );

                        if (!FlagOn(FileName->Flags, FILE_NAME_DOS) || FlagOn(FileName->Flags, FILE_NAME_NTFS)) {
                            break;
                        }

                        Found = NtfsLookupNextAttributeByCode( IrpContext,
                                                               Fcb,
                                                               $FILE_NAME,
                                                               &AttributeContext );
                    }

                    //
                    //  If there is no file name, raise corrupt!
                    //

                    if (FileName == NULL) {
                        NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
                    }
                }

                //
                //  Lock the Fcb so the record can't go away.
                //

                NtfsLockFcb( IrpContext, Fcb );
                LockedFcb = TRUE;

                //
                //  Now test for the need for a new record and construct one
                //  if necc. Prev. test was unsafe for checking the Fcb->FcbUsnRecord
                //

                if ((Fcb->FcbUsnRecord == NULL) ||
                    !FlagOn( Fcb->FcbState, FCB_STATE_VALID_USN_NAME ) ||
                    FlagOn( Reason, USN_REASON_RENAME_NEW_NAME )) {

                    //
                    //  Calculate the size required for the record and allocate a new record.
                    //

                    SizeToAllocate = sizeof( FCB_USN_RECORD ) + (FileName->FileNameLength * sizeof(WCHAR));
                    FcbUsnRecord = NtfsAllocatePool( PagedPool, SizeToAllocate );

                    //
                    //  Zero and initialize the new usn record.
                    //

                    RtlZeroMemory( FcbUsnRecord, SizeToAllocate );

                    FcbUsnRecord->NodeTypeCode = NTFS_NTC_USN_RECORD;
                    FcbUsnRecord->NodeByteSize = (USHORT)QuadAlign(FIELD_OFFSET( FCB_USN_RECORD, UsnRecord.FileName ) +
                                                                   (FileName->FileNameLength * sizeof(WCHAR)));
                    FcbUsnRecord->Fcb = Fcb;

                    FcbUsnRecord->UsnRecord.RecordLength = FcbUsnRecord->NodeByteSize -
                                                           FIELD_OFFSET( FCB_USN_RECORD, UsnRecord );

                    FcbUsnRecord->UsnRecord.MajorVersion = 2;
                    FcbUsnRecord->UsnRecord.FileReferenceNumber = *(PULONGLONG)&Fcb->FileReference;
                    FcbUsnRecord->UsnRecord.ParentFileReferenceNumber = *(PULONGLONG)&FileName->ParentDirectory;
                    FcbUsnRecord->UsnRecord.SecurityId = Fcb->SecurityId;
                    FcbUsnRecord->UsnRecord.FileNameLength = FileName->FileNameLength * 2;
                    FcbUsnRecord->UsnRecord.FileNameOffset = FIELD_OFFSET( USN_RECORD, FileName );

                    RtlCopyMemory( FcbUsnRecord->UsnRecord.FileName,
                                   FileName->FileName,
                                   FileName->FileNameLength * 2 );

                    //
                    //  If the record is there then copy the existing reasons and source info.
                    //

                    if (Fcb->FcbUsnRecord != NULL) {

                        FcbUsnRecord->UsnRecord.Reason = Fcb->FcbUsnRecord->UsnRecord.Reason;
                        FcbUsnRecord->UsnRecord.SourceInfo = Fcb->FcbUsnRecord->UsnRecord.SourceInfo;

                        //
                        //  Deallocate the existing block if still there.
                        //

                        NtfsLockFcb( IrpContext, Fcb->Vcb->UsnJournal->Fcb );

                        //
                        //  Put the new block into the modified list if the current one is
                        //  already there.
                        //

                        if (Fcb->FcbUsnRecord->ModifiedOpenFilesLinks.Flink != NULL) {

                            InsertTailList( &Fcb->FcbUsnRecord->ModifiedOpenFilesLinks,
                                            &FcbUsnRecord->ModifiedOpenFilesLinks );
                            RemoveEntryList( &Fcb->FcbUsnRecord->ModifiedOpenFilesLinks );

                            if (Fcb->FcbUsnRecord->TimeOutLinks.Flink != NULL) {

                                InsertTailList( &Fcb->FcbUsnRecord->TimeOutLinks,
                                                &FcbUsnRecord->TimeOutLinks );
                                RemoveEntryList( &Fcb->FcbUsnRecord->TimeOutLinks );
                            }
                        }

                        NtfsFreePool( Fcb->FcbUsnRecord );
                        Fcb->FcbUsnRecord = FcbUsnRecord;
                        NtfsUnlockFcb( IrpContext, Fcb->Vcb->UsnJournal->Fcb );

                    //
                    //  Otherwise this is a new usn structure.
                    //

                    } else {

                        Fcb->FcbUsnRecord = FcbUsnRecord;

                    }
                } else {

                    //
                    //  We are going to reuse the current fcb record in this path.
                    //  This can happen in races between the write path which has only paged sharing
                    //  and the close record path which has only main exclusive. In this
                    //  case the only synchronization we have is the fcb->mutex
                    //  The old usnrecord should be identical to the current one we would have constructed
                    //

                    ASSERT( FileName->FileNameLength * 2 == Fcb->FcbUsnRecord->UsnRecord.FileNameLength );
                    ASSERT( RtlEqualMemory( FileName->FileName, Fcb->FcbUsnRecord->UsnRecord.FileName,  Fcb->FcbUsnRecord->UsnRecord.FileNameLength ) );
                }

                //
                //  Set the flag indicating that the Usn name is valid.
                //

                SetFlag( Fcb->FcbState, FCB_STATE_VALID_USN_NAME );


            } finally {

                if (LockedFcb) {
                    NtfsUnlockFcb( IrpContext, Fcb );
                }
                if (AcquiredFcb) {
                    NtfsReleaseResource( IrpContext, Fcb );
                }
                NtfsCleanupAttributeContext( IrpContext, &AttributeContext );
            }
        }
    }

    //
    //  If we have memory structures for the usn reasons fill in the new reasons
    //  Note: this means that journal may not be active at this point. We will always
    //  accumulate reasons once we have started
    //

    if (Fcb->FcbUsnRecord != NULL) {

        //
        //  Scan the list to see if we already have an entry for this Fcb.  If there are
        //  no entries then use the position in the IrpContext, otherwise allocate a USN_FCB
        //  and chain this into the IrpContext.  Typical case for this is rename.
        //

        do {

            if (ThisUsn->CurrentUsnFcb == Fcb) { break; }

            //
            //  Check if we are at the last entry then we want to use the entry in the
            //  IrpContext.
            //

            if (ThisUsn->CurrentUsnFcb == NULL) {

                RtlZeroMemory( &ThisUsn->CurrentUsnFcb,
                               sizeof( USN_FCB ) - FIELD_OFFSET( USN_FCB, CurrentUsnFcb ));
                ThisUsn->CurrentUsnFcb = Fcb;
                break;
            }

            if (ThisUsn->NextUsnFcb == NULL) {

                //
                //  Allocate a new entry.
                //

                ThisUsn->NextUsnFcb = NtfsAllocatePool( PagedPool, sizeof( USN_FCB ));
                ThisUsn = ThisUsn->NextUsnFcb;

                RtlZeroMemory( ThisUsn, sizeof( USN_FCB ));
                ThisUsn->CurrentUsnFcb = Fcb;
                break;
            }

            ThisUsn = ThisUsn->NextUsnFcb;

        } while (TRUE);

        //
        //  If the Reason is one of the data stream reasons, and this is the named data
        //  steam, then change the code.
        //

        ASSERT(USN_REASON_NAMED_DATA_OVERWRITE == (USN_REASON_DATA_OVERWRITE << 4));
        ASSERT(USN_REASON_NAMED_DATA_EXTEND == (USN_REASON_DATA_EXTEND << 4));
        ASSERT(USN_REASON_NAMED_DATA_TRUNCATION == (USN_REASON_DATA_TRUNCATION << 4));

        if ((Reason & (USN_REASON_DATA_OVERWRITE | USN_REASON_DATA_EXTEND | USN_REASON_DATA_TRUNCATION)) &&
            (Scb->AttributeName.Length != 0)) {

            //
            //  If any flag other than these three are set already, the shift will make
            //  them look like other flags.  For instance, USN_REASON_NAMED_DATA_EXTEND
            //  will become USN_REASON_FILE_DELETE, which will cause a number of problems.
            //

            ASSERT(!FlagOn( Reason, ~(USN_REASON_DATA_OVERWRITE | USN_REASON_DATA_EXTEND | USN_REASON_DATA_TRUNCATION) ));

            Reason <<= 4;
        }

        //
        //  If there are no new reasons, then we can ignore this change.
        //
        //  We will generate a new record if the SourceInfo indicates some
        //  change to the source info in the record.
        //

        NtfsLockFcb( IrpContext, Fcb );

        //
        //  The rename flags are the only ones that do not accumulate until final close, since
        //  we write records designating old and new names.  So if we are writing one flag
        //  we must clear the other.
        //

        if (FlagOn(Reason, USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME)) {

            ClearFlag( ThisUsn->NewReasons,
                       (Reason ^ (USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME)) );
            ClearFlag( Fcb->FcbUsnRecord->UsnRecord.Reason,
                       (Reason ^ (USN_REASON_RENAME_OLD_NAME | USN_REASON_RENAME_NEW_NAME)) );
        }

        //
        //  Check if the reason is a new reason.
        //

        NewReasons = FlagOn( ~(Fcb->FcbUsnRecord->UsnRecord.Reason | ThisUsn->NewReasons), Reason );
        if (NewReasons != 0) {

            //
            //  Check if we will remove a bit from the source info.
            //

            if ((Fcb->FcbUsnRecord->UsnRecord.SourceInfo != 0) &&
                (Fcb->FcbUsnRecord->UsnRecord.Reason != 0) &&
                (Reason != USN_REASON_CLOSE)) {

                RemovedSourceInfo = FlagOn( Fcb->FcbUsnRecord->UsnRecord.SourceInfo,
                                            ~(IrpContext->SourceInfo | ThisUsn->RemovedSourceInfo) );

                if (RemovedSourceInfo != 0) {

                    SetFlag( ThisUsn->RemovedSourceInfo, RemovedSourceInfo );
                }
            }

            //
            //  Post the new reasons to the IrpContext.
            //

            ThisUsn->CurrentUsnFcb = Fcb;
            SetFlag( ThisUsn->NewReasons, NewReasons );
            SetFlag( ThisUsn->UsnFcbFlags, USN_FCB_FLAG_NEW_REASON );

        //
        //  Check if there is a change only to the source info.
        //  We look to see if we would remove a bit from the
        //  source info only if there has been at least one
        //  usn record already.
        //

        } else if ((Fcb->FcbUsnRecord->UsnRecord.SourceInfo != 0) &&
                   (Fcb->FcbUsnRecord->UsnRecord.Reason != 0) &&
                   (Reason != USN_REASON_CLOSE)) {

            //
            //  Remember the bit being removed.
            //

            RemovedSourceInfo = FlagOn( Fcb->FcbUsnRecord->UsnRecord.SourceInfo,
                                        ~(IrpContext->SourceInfo | ThisUsn->RemovedSourceInfo) );

            if (RemovedSourceInfo != 0) {

                SetFlag( ThisUsn->RemovedSourceInfo, RemovedSourceInfo );
                ThisUsn->CurrentUsnFcb = Fcb;
                SetFlag( ThisUsn->UsnFcbFlags, USN_FCB_FLAG_NEW_REASON );

            } else {

                Reason = 0;
            }

        //
        //  If we did not apply the changes, then make sure we do no more special processing
        //  below.
        //

        } else {

            Reason = 0;
        }

        NtfsUnlockFcb( IrpContext, Fcb );

        //
        //  For data overwrites it is necessary to actually write the Usn journal now, in
        //  case we crash before the request is completed, yet the data makes it out.  Also
        //  we need to capture the Lsn to flush to if the data is getting flushed.
        //
        //  We don't need to make this call if we are doing a rename.  Rename will rewrite previous
        //  records.
        //

        if ((IrpContext->MajorFunction != IRP_MJ_SET_INFORMATION) &&
            FlagOn( Reason, USN_REASON_DATA_OVERWRITE | USN_REASON_NAMED_DATA_OVERWRITE )) {

            LSN UpdateLsn;

            //
            //  For now assume we are not already a transaction, since we will be doing a
            //  checkpoint.  (If this ASSERT ever fires, verify that it is ok to checkpoint
            //  the transaction in that case and fix the ASSERT!)
            //

            ASSERT(IrpContext->TransactionId == 0);

            //
            //  Now write the journal, checkpoint the transaction, and free the UsnJournal to
            //  reduce contention. Get rid of any pinned Mft records, because WriteUsnJournal is going
            //  to acquire the Scb resource.
            //

            NtfsPurgeFileRecordCache( IrpContext );
            NtfsWriteUsnJournalChanges( IrpContext );
            NtfsCheckpointCurrentTransaction( IrpContext );

            //
            //  Capture the Lsn to flush to *in the first thread to set one of the above bits*,
            //  before letting any data hit the disk.  Synchronize it with the Fcb lock.
            //

            UpdateLsn = LfsQueryLastLsn( Fcb->Vcb->LogHandle );
            NtfsLockFcb( IrpContext, Fcb );
            Fcb->UpdateLsn = UpdateLsn;
            NtfsUnlockFcb( IrpContext, Fcb );
        }
    }

    return ThisUsn->NewReasons;
}


VOID
NtfsWriteUsnJournalChanges (
    PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine is called to write a set of posted changes from the IrpContext
    to the UsnJournal, if they have not already been posted.

Arguments:

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
    PFCB Fcb;
    PVCB Vcb;
    PSCB UsnJournal;
    PUSN_FCB ThisUsn;
    ULONG PreserveWaitState;
    BOOLEAN WroteUsnRecord = FALSE;
    BOOLEAN ReleaseFcbs = FALSE;
    BOOLEAN CleanupContext = FALSE;

    ThisUsn = &IrpContext->Usn;

    do {

        //
        //  Is there an Fcb with usn reasons in the current irpcontext usn_fcb structures ?
        //  Also are there any new reasons to report for this fcb.
        //

        if ((ThisUsn->CurrentUsnFcb != NULL) &&
            FlagOn( ThisUsn->UsnFcbFlags, USN_FCB_FLAG_NEW_REASON )) {

            Fcb = ThisUsn->CurrentUsnFcb;
            Vcb = Fcb->Vcb;
            UsnJournal = Vcb->UsnJournal;

            //
            //  Remember that we wrote a record.
            //

            WroteUsnRecord = TRUE;

            //
            //  We better be waitable.
            //

            PreserveWaitState = (IrpContext->State ^ IRP_CONTEXT_STATE_WAIT) & IRP_CONTEXT_STATE_WAIT;
            SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );

            if (FlagOn( Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE )) {

                //
                //  Acquire the Usn journal and lock the Fcb fields.
                //

                NtfsAcquireExclusiveScb( IrpContext, Vcb->MftScb );
                NtfsAcquireExclusiveScb( IrpContext, UsnJournal );
                ReleaseFcbs = TRUE;
            }

            try {

                USN Usn;
                ULONG BytesLeftInPage;

                //
                //  Make sure the changes have not already been logged.  We're
                //  looking for new reasons or a change to the source info.
                //

                NtfsLockFcb( IrpContext, Fcb );

                //
                //  This is the tricky synchronization case. Assumption is
                //  that if name goes invalid we have both resources exclusive and any writes will
                //  be preceded by a post which will remove the invalid record
                //  This occurs when we remove a link and generate one record under the old name
                //  with the flag set as invalid
                //

                ASSERT( !FlagOn( Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE ) ||
                        FlagOn( Fcb->FcbState, FCB_STATE_VALID_USN_NAME ) ||
                        (NtfsIsExclusiveFcb( Fcb ) &&
                         ((Fcb->PagingIoResource == NULL) || (NtfsIsExclusiveFcbPagingIo( Fcb )))) );

                //
                //  Initialize the Fcb source info if this is our first record.
                //

                if (Fcb->FcbUsnRecord->UsnRecord.Reason == 0) {

                    Fcb->FcbUsnRecord->UsnRecord.SourceInfo = IrpContext->SourceInfo;
                }

                //
                //  Accumulate all reasons and store in the Fcb before unlocking the Fcb.
                //

                SetFlag( Fcb->FcbUsnRecord->UsnRecord.Reason, ThisUsn->NewReasons );

                //
                //  Now clear the source info flags not supported by this
                //  caller.
                //

                ClearFlag( Fcb->FcbUsnRecord->UsnRecord.SourceInfo, ThisUsn->RemovedSourceInfo );

                //
                //  Unlock Fcb now so we do not deadlock if we do a checkpoint.
                //

                NtfsUnlockFcb( IrpContext, Fcb );

                //
                //  Only actually persist to disk if the journal is active
                //

                if (FlagOn( Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE )) {

                    ASSERT( UsnJournal != NULL );

                    //
                    //  Initialize the context structure if we are doing a close.
                    //

                    if (FlagOn( Fcb->FcbUsnRecord->UsnRecord.Reason, USN_REASON_CLOSE )) {
                        NtfsInitializeAttributeContext( &AttributeContext );
                        CleanupContext = TRUE;
                    }

                    Usn = UsnJournal->Header.FileSize.QuadPart;
                    BytesLeftInPage = USN_PAGE_SIZE - ((ULONG)Usn & (USN_PAGE_SIZE - 1));

                    //
                    //  If there is not enough room left in this page for the
                    //  current Usn Record, then advance to the next page boundary
                    //  by writing 0's (these pages not zero-initialized( and update  the Usn.
                    //

                    if (BytesLeftInPage < Fcb->FcbUsnRecord->UsnRecord.RecordLength) {

                        ASSERT( Fcb->FcbUsnRecord->UsnRecord.RecordLength <= USN_PAGE_SIZE );

                        NtOfsPutData( IrpContext, UsnJournal, -1, BytesLeftInPage, NULL );
                        Usn += BytesLeftInPage;
                    }

                    Fcb->FcbUsnRecord->UsnRecord.Usn = Usn;

                    //
                    //  Build the FileAttributes from the Fcb.
                    //

                    Fcb->FcbUsnRecord->UsnRecord.FileAttributes = Fcb->Info.FileAttributes & FILE_ATTRIBUTE_VALID_FLAGS;

                    //
                    //  We have to generate the DIRECTORY attribute.
                    //

                    if (IsDirectory( &Fcb->Info ) || IsViewIndex( &Fcb->Info )) {
                        SetFlag( Fcb->FcbUsnRecord->UsnRecord.FileAttributes, FILE_ATTRIBUTE_DIRECTORY );
                    }

                    //
                    //  If there are no flags set then explicitly set the NORMAL flag.
                    //

                    if (Fcb->FcbUsnRecord->UsnRecord.FileAttributes == 0) {
                        Fcb->FcbUsnRecord->UsnRecord.FileAttributes = FILE_ATTRIBUTE_NORMAL;
                    }

                    KeQuerySystemTime( &Fcb->FcbUsnRecord->UsnRecord.TimeStamp );

                    //
                    //  Append the record to the UsnJournal.  We should never see a record with
                    //  both rename flags or the close flag with the old name flag.
                    //

                    ASSERT( !FlagOn( Fcb->FcbUsnRecord->UsnRecord.Reason, USN_REASON_RENAME_OLD_NAME ) ||
                            !FlagOn( Fcb->FcbUsnRecord->UsnRecord.Reason,
                                     USN_REASON_CLOSE | USN_REASON_RENAME_NEW_NAME ));

                    NtOfsPutData( IrpContext,
                                  UsnJournal,
                                  -1,
                                  Fcb->FcbUsnRecord->UsnRecord.RecordLength,
                                  &Fcb->FcbUsnRecord->UsnRecord );

#ifdef BRIANDBG
                    //
                    //  The Usn better be in an allocated piece.
                    //

                    {
                        LCN Lcn;
                        LONGLONG ClusterCount;

                        if (!NtfsLookupAllocation( IrpContext,
                                                   UsnJournal,
                                                   LlClustersFromBytesTruncate( Vcb, Usn ),
                                                   &Lcn,
                                                   &ClusterCount,
                                                   NULL,
                                                   NULL ) ||
                            (Lcn == UNUSED_LCN)) {
                            ASSERT( FALSE );
                        }
                    }
#endif
                    //
                    //  If this is the close record, then we must update the Usn in the file record.
                    //

                    if (!FlagOn( Fcb->FcbUsnRecord->UsnRecord.Reason, USN_REASON_FILE_DELETE ) &&
                        !FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )) {

                        //
                        //  See if we need to actually grow Standard Information first.
                        //  Do this even if we don't write the Usn record now.  We may
                        //  generate a close record for this file during mount and
                        //  we expect the STANDARD_INFORMATION to support Usns.
                        //

                        if (!FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO )) {

                            //
                            //  Grow the standard information.
                            //

                            NtfsGrowStandardInformation( IrpContext, Fcb );
                        }


                        if (FlagOn( Fcb->FcbUsnRecord->UsnRecord.Reason, USN_REASON_CLOSE )) {

                            //
                            //  Locate the standard information, it must be there.
                            //

                            if (!NtfsLookupAttributeByCode( IrpContext,
                                                            Fcb,
                                                            &Fcb->FileReference,
                                                            $STANDARD_INFORMATION,
                                                            &AttributeContext )) {

                                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
                            }

                            ASSERT(NtfsFoundAttribute( &AttributeContext )->Form.Resident.ValueLength ==
                                sizeof( STANDARD_INFORMATION ));

                            //
                            //  Call to change the attribute value.
                            //

                            NtfsChangeAttributeValue( IrpContext,
                                                      Fcb,
                                                      FIELD_OFFSET(STANDARD_INFORMATION, Usn),
                                                      &Usn,
                                                      sizeof(Usn),
                                                      FALSE,
                                                      FALSE,
                                                      FALSE,
                                                      FALSE,
                                                      &AttributeContext );
                        }
                    }

                    //
                    //  Remember to release these resources as soon as possible now.
                    //  Note, if we are not sure that we became a transaction (else
                    //  case below) then our finally clause will do the release.
                    //
                    //  If the system has already gone through shutdown we won't be
                    //  able to start a transaction.  Test that we have a transaction
                    //  before setting these flags.
                    //

                    if (IrpContext->TransactionId != 0) {

                        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_RELEASE_USN_JRNL |
                                                    IRP_CONTEXT_FLAG_RELEASE_MFT );
                    }
                }

                //
                //  Clear the flag indicating that there are new reasons to report.
                //

                ClearFlag( ThisUsn->UsnFcbFlags, USN_FCB_FLAG_NEW_REASON );


            } finally {

                //
                //  Cleanup the context structure if we are doing a close.
                //

                if (CleanupContext) {
                    NtfsCleanupAttributeContext( IrpContext, &AttributeContext );
                }

                if (ReleaseFcbs) {
                    NtfsReleaseScb( IrpContext, UsnJournal );
                    ASSERT( NtfsIsExclusiveScb( Vcb->MftScb ) );
                    NtfsReleaseScb( IrpContext, Vcb->MftScb );
                }
            }

            ClearFlag( IrpContext->State, PreserveWaitState );
        }

        //
        //  Go to the next entry if present.  If we are at the last entry then walk through all of the
        //  entries and clear the flag indicating we have new reasons.
        //

        if (ThisUsn->NextUsnFcb == NULL) {

            //
            //  Exit if we didn't write any records.
            //

            if (!WroteUsnRecord) { break; }

            ThisUsn = &IrpContext->Usn;

            do {

                ClearFlag( ThisUsn->UsnFcbFlags, USN_FCB_FLAG_NEW_REASON );
                if (ThisUsn->NextUsnFcb == NULL) { break; }

                ThisUsn = ThisUsn->NextUsnFcb;

            } while (TRUE);

            break;
        }

        ThisUsn = ThisUsn->NextUsnFcb;

    } while (TRUE);

    return;
}


VOID
NtfsSetupUsnJournal (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN ULONG CreateIfNotExist,
    IN ULONG Restamp,
    IN PCREATE_USN_JOURNAL_DATA NewJournalData
    )

/*++

Routine Description:

    This routine is called to setup the Usn Journal - the stream may or may
    not yet exist.  This routine is responsible for cleaning up the disk and
    in-memory structures on failure.

Arguments:

    Vcb - Supplies the volume being initialized.

    Fcb - Supplies the file for the Usn Journal.

    CreateIfNotExist - Indicates that we should use the values in the Vcb instead of on-disk.

    Restamp - Indicates if we should restamp the journal with a new Id.

    NewJournalData - Allocation size and delta for Usn journal if we are not reading from disk.

Return Value:

    None.

--*/

{
    RTL_GENERIC_TABLE UsnControlTable;
    PSCB UsnJournal;
    PUSN_RECORD UsnRecord, UsnRecordInTable;
    BOOLEAN CleanupControlTable = FALSE;

    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
    MAP_HANDLE MapHandle;
    USN StartUsn;
    LONGLONG ClusterCount;

    PUSN_JOURNAL_INSTANCE UsnJournalData;
    USN_JOURNAL_INSTANCE UsnJournalInstance, VcbUsnInstance;
    PUSN_JOURNAL_INSTANCE InstanceToRestore;

    PBCB Bcb = NULL;

    LONGLONG SavedReservedSpace;
    LONGLONG RequiredReserved;

    BOOLEAN FoundMax;
    BOOLEAN NewMax = FALSE;
    BOOLEAN InsufficientReserved = FALSE;

    BOOLEAN DecrementCloseCount = TRUE;

    LARGE_INTEGER LastTimeStamp;

    ULONG TempUlong;
    LONGLONG TempLonglong;

    PAGED_CODE( );

    //
    //  Make sure we don't move to a larger page size.
    //

    ASSERT( USN_PAGE_BOUNDARY >= PAGE_SIZE );

    //
    //  Open/Create the Usn Journal stream.  We should never have an Scb
    //  if we are mounting a new volume.
    //

    ASSERT( (((ULONG) USN_JOURNAL_CACHE_BIAS) & (VACB_MAPPING_GRANULARITY - 1)) == 0 );

    NtOfsCreateAttribute( IrpContext,
                          Fcb,
                          JournalStreamName,
                          CREATE_OR_OPEN,
                          TRUE,
                          &UsnJournal );

    ASSERT( NtfsIsExclusiveScb( UsnJournal ) && NtfsIsExclusiveScb( Vcb->MftScb ) );

    //
    //  Initialize the enumeration context and map handle.
    //

    NtfsInitializeAttributeContext( &AttributeContext );
    NtOfsInitializeMapHandle( &MapHandle );

    //
    //  Let's build the journal instance data.  Assume we have current valid
    //  values in the Vcb for the Id and lowest valid usn.
    //

    UsnJournalInstance.MaximumSize = NewJournalData->MaximumSize;
    UsnJournalInstance.AllocationDelta = NewJournalData->AllocationDelta;

    UsnJournalInstance.JournalId = Vcb->UsnJournalInstance.JournalId;
    UsnJournalInstance.LowestValidUsn = Vcb->UsnJournalInstance.LowestValidUsn;

    //
    //  Capture the current reservation in the Journal Scb and also the
    //  current JournalData in the Vcb to restore on error.
    //

    SavedReservedSpace = UsnJournal->ScbType.Data.TotalReserved;

    RtlCopyMemory( &VcbUsnInstance,
                   &Vcb->UsnJournalInstance,
                   sizeof( USN_JOURNAL_INSTANCE ));

    InstanceToRestore = &VcbUsnInstance;

    try {

        //
        //  Make sure the Scb is initialized.
        //

        if (!FlagOn( UsnJournal->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

            NtfsUpdateScbFromAttribute( IrpContext, UsnJournal, NULL );
        }

        //
        //  Always create the journal non-resident.  Otherwise in
        //  ConvertToNonResident we always need to check for this case
        //  which only happens once per volume.
        //

        if (FlagOn( UsnJournal->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            NtfsLookupAttributeForScb( IrpContext, UsnJournal, NULL, &AttributeContext );
            ASSERT( NtfsIsAttributeResident( NtfsFoundAttribute( &AttributeContext )));
            NtfsConvertToNonresident( IrpContext,
                                      Fcb,
                                      NtfsFoundAttribute( &AttributeContext ),
                                      FALSE,
                                      &AttributeContext );

            NtfsCleanupAttributeContext( IrpContext, &AttributeContext );
        }

        //
        //  Remember to restamp if an earlier delete operation failed.  This flag should
        //  never be set if there is a current UsnJournal Scb in the Vcb.
        //


        ASSERT( !FlagOn( Vcb->VcbState, VCB_STATE_INCOMPLETE_USN_DELETE ) ||
                (Vcb->UsnJournal == NULL) );

        if (FlagOn( Vcb->VcbState, VCB_STATE_INCOMPLETE_USN_DELETE )) {

            Restamp = TRUE;
        }

        //
        //  If the $Max doesn't exist or we want to restamp then generate the
        //  new ID and Lowest ID.
        //

        if (!(FoundMax = NtfsLookupAttributeByName( IrpContext,
                                                    Fcb,
                                                    &Fcb->FileReference,
                                                    $DATA,
                                                    &$Max,
                                                    NULL,
                                                    FALSE,
                                                    &AttributeContext )) ||
            Restamp ) {

            NtfsAdvanceUsnJournal( Vcb, &UsnJournalInstance, UsnJournal->Header.FileSize.QuadPart, &NewMax );

        //
        //  Examine the current $Max attribute for validity and either use the current values or
        //  generate new values.
        //

        } else {

            //
            //  Get the size of the $Max attribute.  It should always be resident but we will rewrite it in
            //  the case where it isn't.
            //

            if (NtfsIsAttributeResident( NtfsFoundAttribute( &AttributeContext ))) {

                TempUlong = (NtfsFoundAttribute( &AttributeContext))->Form.Resident.ValueLength;

            } else {

                TempUlong = (ULONG) (NtfsFoundAttribute( &AttributeContext))->Form.Nonresident.FileSize;
                NewMax = TRUE;
            }

            //
            //  Map the attribute and check it for consistency.
            //

            NtfsMapAttributeValue( IrpContext,
                                   Fcb,
                                   &UsnJournalData,
                                   &TempUlong,
                                   &Bcb,
                                   &AttributeContext );

            //
            //  Only copy over the range of values we would understand.  If the size is not one
            //  we recognize then restamp the journal.  We handle the V1 case as well as V2 case.
            //

            if (TempUlong == sizeof( CREATE_USN_JOURNAL_DATA )) {

                UsnJournalInstance.LowestValidUsn = 0;
                KeQuerySystemTime( (PLARGE_INTEGER) &UsnJournalInstance.JournalId );

                //
                //  Put version 2 on the disk.
                //

                NewMax = TRUE;

                //
                //  If this is not an overwrite then copy the size and delta from the attribute.
                //

                if (!CreateIfNotExist) {

                    //
                    //  Assume we will use the values from the disk.
                    //

                    RtlCopyMemory( &UsnJournalInstance,
                                   UsnJournalData,
                                   TempUlong );
                }

            } else if (TempUlong == sizeof( USN_JOURNAL_INSTANCE )) {

                //
                //  Assume we will use the values from the disk.
                //

                if (CreateIfNotExist) {

                    NewMax = TRUE;
                    UsnJournalInstance.LowestValidUsn = UsnJournalData->LowestValidUsn;
                    UsnJournalInstance.JournalId = UsnJournalData->JournalId;

                } else {

                    //
                    //  Get the data from the disk.
                    //

                    RtlCopyMemory( &UsnJournalInstance,
                                   UsnJournalData,
                                   TempUlong );
                }

            } else {

                //
                //  Restamp in this case.
                //  We move forward in the file to the next Usn boundary.
                //

                NtfsAdvanceUsnJournal( Vcb, &UsnJournalInstance, UsnJournal->Header.FileSize.QuadPart, &NewMax );
            }

            //
            //  Put the Bcb back into the context if we removed it.
            //

            if (NtfsFoundBcb( &AttributeContext ) == NULL) {

                NtfsFoundBcb( &AttributeContext ) = Bcb;
                Bcb = NULL;
            }
        }

        //
        //  Check that the file doesn't end on a sparse hole.
        //

        if (!NewMax &&
            (UsnJournal->Header.AllocationSize.QuadPart != 0) &&
            (UsnJournalInstance.LowestValidUsn != UsnJournal->Header.AllocationSize.QuadPart)) {

            LCN Lcn;
            LONGLONG ClusterCount;

            if (!NtfsLookupAllocation( IrpContext,
                                       UsnJournal,
                                       LlClustersFromBytesTruncate( Vcb, UsnJournal->Header.AllocationSize.QuadPart - 1 ),
                                       &Lcn,
                                       &ClusterCount,
                                       NULL,
                                       NULL ) ||
                (Lcn == UNUSED_LCN)) {

                NtfsAdvanceUsnJournal( Vcb, &UsnJournalInstance, UsnJournal->Header.AllocationSize.QuadPart, &NewMax );
            }
        }

        //
        //  Enforce minimum sizes and allocation deltas, do not let them eat the whole volume,
        //  and round them to a Cache Manager View Size.  All of these decisions are arbitrary,
        //  but hopefully reasonable.  An option would be to take the cases other than those
        //  dealing with rounding, and return an error.
        //

        if ((ULONGLONG) UsnJournalInstance.MaximumSize < (ULONGLONG) VcbUsnInstance.MaximumSize) {

            UsnJournalInstance.MaximumSize = VcbU
                                       0,
                                       TRUE )) {

                Status = STATUS_CANCELLED;

            //
            //  Add it to the work queue if we were able to set the
            //  cancel routine.
            //

            } else {

                InsertTailList( &Vcb->NotifyUsnDeleteIrps,
                                &Irp->Tail.Overlay.ListEntry );
            }

            NtfsReleaseUsnNotify( Vcb );
            AcquiredNotify = FALSE;
        }

    } finally {

        if (AcquiredNotify) {

            NtfsReleaseUsnNotify( Vcb );
        }

        //
        //  Release the Usn journal if held.
        //

        if (ReleaseUsnJournal) {

            NtfsReleaseScb( IrpContext, ReleaseUsnJournal );
        }

        //
        //  Release the Vcb if held.
        //

        if (VcbAcquired) {

            NtfsReleaseVcb( IrpContext, Vcb );
        }

        //
        //  Release the checkpoint if held.
        //

        if (CheckpointHeld) {

            NtfsAcquireCheckpoint( IrpContext, Vcb );
            ClearFlag( Vcb->CheckpointFlags, VCB_CHECKPOINT_IN_PROGRESS );
            NtfsSetCheckpointNotify( IrpContext, Vcb );
            NtfsReleaseCheckpoint( IrpContext, Vcb );
        }
    }

    //
    //  Complete the irp as appropriate.
    //

    NtfsCompleteRequest( IrpContext,
                         (Status == STATUS_PENDING) ? NULL : Irp,
                         Status );
    return Status;
}


VOID
NtfsDeleteUsnSpecial (
    IN PIRP_CONTEXT IrpContext,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called to perform the work of deleting a Usn journal for a volume.
    It is called after the original entry point has done the preliminary work of stopping
    future journal activity and cleaning up active journal requests.  Once we reach this
    point then this routine will make sure the Mft values are reset, delete the journal
    file itself and wake up anyone waiting for the delete journal to complete.

Arguments:

    IrpContext - context of the call

    Context - DELETE_USN_CONTEXT structure used to manage the delete.

Return Value:

    None

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PNTFS_DELETE_JOURNAL_DATA DeleteData = (PNTFS_DELETE_JOURNAL_DATA) Context;
    ULONG AcquiredVcb = FALSE;
    PVCB Vcb = IrpContext->Vcb;
    PFCB UsnFcb = NULL;
    BOOLEAN AcquiredExtendDirectory = FALSE;

    PIRP UsnNotifyIrp;

    PLIST_ENTRY Links;
    PSCB Scb;
    PFCB Fcb;

    PAGED_CODE();

    //
    //  Use a try-except to catch errors.
    //

    try {

        if (NtfsIsVolumeReadOnly( Vcb )) {

            Vcb->DeleteUsnData.FinalStatus = STATUS_MEDIA_WRITE_PROTECTED;
            NtfsRaiseStatus( IrpContext, STATUS_MEDIA_WRITE_PROTECTED, NULL, NULL );
        }

        //
        //  Make sure to walk the Mft to set the Usn value back to zero.
        //

        if (!FlagOn( DeleteData->DeleteState, DELETE_USN_RESET_MFT )) {

            try {

                Status = NtfsIterateMft( IrpContext,
                                         IrpContext->Vcb,
                                         &DeleteData->DeleteUsnFileReference,
                                         NtfsDeleteUsnWorker,
                                         Context );

            } except (NtfsCleanupExceptionFilter( IrpContext, GetExceptionInformation(), &Status )) {

                NOTHING;
            }

            if (!NT_SUCCESS( Status ) && (Status != STATUS_END_OF_FILE)) {

                //
                //  If the operation is going to fail then decide if this is retryable.
                //

                if (Status == STATUS_VOLUME_DISMOUNTED) {

                    Vcb->DeleteUsnData.FinalStatus = STATUS_VOLUME_DISMOUNTED;

                } else if ((Status != STATUS_LOG_FILE_FULL) &&
                           (Status != STATUS_CANT_WAIT)) {

                    Vcb->DeleteUsnData.FinalStatus = Status;

                    //
                    //  Set all the flags for delete operations so we stop at this point.
                    //

                    SetFlag( DeleteData->DeleteState,
                             DELETE_USN_RESET_MFT | DELETE_USN_REMOVE_JOURNAL | DELETE_USN_FINAL_CLEANUP );

                    Status = STATUS_CANT_WAIT;
                }

                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
            }

            Status = STATUS_SUCCESS;

            NtfsPurgeFileRecordCache( IrpContext );
            NtfsCheckpointCurrentTransaction( IrpContext );
            SetFlag( DeleteData->DeleteState, DELETE_USN_RESET_MFT );
        }

        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        AcquiredVcb = TRUE;

        //
        //  If the volume is no longer available then raise STATUS_VOLUME_DISMOUNTED.  Someone
        //  else will find all of the waiters.
        //

        if (!NtfsIsVcbAvailable( Vcb )) {

            Vcb->DeleteUsnData.FinalStatus = STATUS_VOLUME_DISMOUNTED;
            NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
        }

        //
        //  The next step is to remove the file if present.
        //

        if (!FlagOn( DeleteData->DeleteState, DELETE_USN_REMOVE_JOURNAL )) {

            try {

                if (Vcb->ExtendDirectory != NULL) {

                    NtfsAcquireExclusiveScb( IrpContext, Vcb->ExtendDirectory );
                    AcquiredExtendDirectory = TRUE;

                    UsnFcb = NtfsInitializeFileInExtendDirectory( IrpContext,
                                                                  Vcb,
                                                                  &NtfsUsnJrnlName,
                                                                  FALSE,
                                                                  FALSE );

                    if (UsnFcb != NULL) {

                        //
                        //  For lock order acquire in canonical order after unsafe try
                        //

                        if (!NtfsAcquireExclusiveFcb( IrpContext, UsnFcb, NULL, ACQUIRE_NO_DELETE_CHECK  | ACQUIRE_DONT_WAIT)) {
                            NtfsReleaseScb( IrpContext, Vcb->ExtendDirectory );
                            NtfsAcquireExclusiveFcb( IrpContext, UsnFcb, NULL, ACQUIRE_NO_DELETE_CHECK );
                            NtfsAcquireExclusiveScb( IrpContext, Vcb->ExtendDirectory );
                        }

                        NtfsDeleteFile( IrpContext,
                                        UsnFcb,
                                        Vcb->ExtendDirectory,
                                        &AcquiredExtendDirectory,
                                        NULL,
                                        NULL );

                        NtfsPurgeFileRecordCache( IrpContext );
                        NtfsCheckpointCurrentTransaction( IrpContext );

                        ClearFlag( UsnFcb->FcbState, FCB_STATE_SYSTEM_FILE );
                        SetFlag( UsnFcb->FcbState, FCB_STATE_FILE_DELETED );

                        //
                        //  Walk all of the Scbs for this file and mark them
                        //  deleted.  This will keep the lazy writer from trying to
                        //  flush them.
                        //

                        Links = UsnFcb->ScbQueue.Flink;

                        while (Links != &UsnFcb->ScbQueue) {

                            Scb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                            //
                            //  Recover the reservation for the Scb now instead of waiting for it
                            //  to go away.
                            //

                            if ((Scb->AttributeTypeCode == $DATA) &&
                                (Scb->ScbType.Data.TotalReserved != 0)) {

                                NtfsAcquireReservedClusters( Vcb );

                                Vcb->TotalReserved -= LlClustersFromBytes( Vcb,
                                                                           Scb->ScbType.Data.TotalReserved );
                                Scb->ScbType.Data.TotalReserved = 0;
                                NtfsReleaseReservedClusters( Vcb );
                            }

                            Scb->ValidDataToDisk =
                            Scb->Header.AllocationSize.QuadPart =
                            Scb->Header.FileSize.QuadPart =
                            Scb->Header.ValidDataLength.QuadPart = 0;

                            Scb->AttributeTypeCode = $UNUSED;
                            SetFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );

                            Links = Links->Flink;
                        }

                        //
                        //  Now teardown the Fcb.
                        //

                        NtfsTeardownStructures( IrpContext,
                                                UsnFcb,
                                                NULL,
                                                FALSE,
                                                ACQUIRE_NO_DELETE_CHECK,
                                                NULL );
                    }
                }

            } except (NtfsCleanupExceptionFilter( IrpContext, GetExceptionInformation(), &Status )) {

                //
                //    We hit some failure and can't complete the operation.
                //    Remember the error, set the flags in the delete Usn structure
                //    and raise CANT_WAIT so we can abort and then do the final cleanup.
                //

                Vcb->DeleteUsnData.FinalStatus = Status;

                //
                //  Set all the flags for delete operations so we stop at this point.
                //

                SetFlag( DeleteData->DeleteState,
                         DELETE_USN_RESET_MFT | DELETE_USN_REMOVE_JOURNAL | DELETE_USN_FINAL_CLEANUP );

                Status = STATUS_CANT_WAIT;
            }

            if (!NT_SUCCESS( Status )) {

                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
            }

            SetFlag( DeleteData->DeleteState, DELETE_USN_REMOVE_JOURNAL );
        }

        if (!FlagOn( DeleteData->DeleteState, DELETE_USN_FINAL_CLEANUP )) {

            //
            //  Clear the on-disk flag indicating the delete is in progress.
            //

            try {

                NtfsSetVolumeInfoFlagState( IrpContext,
                                            Vcb,
                                            VOLUME_DELETE_USN_UNDERWAY,
                                            FALSE,
                                            TRUE );

            } except (NtfsCleanupExceptionFilter( IrpContext, GetExceptionInformation(), &Status )) {

                //
                //    We hit some failure and can't complete the operation.
                //    Remember the error, set the flags in the delete Usn structure
                //    and raise CANT_WAIT so we can abort and then do the final cleanup.
                //

                Vcb->DeleteUsnData.FinalStatus = Status;

                //
                //  Set all the flags for delete operations so we stop at this point.
                //

                SetFlag( DeleteData->DeleteState,
                         DELETE_USN_RESET_MFT | DELETE_USN_REMOVE_JOURNAL | DELETE_USN_FINAL_CLEANUP );

                Status = STATUS_CANT_WAIT;
            }

            if (!NT_SUCCESS( Status )) {

                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
            }
        }

        //
        //  Make sure we don't own any resources at this point.
        //

        NtfsPurgeFileRecordCache( IrpContext );
        NtfsCheckpointCurrentTransaction( IrpContext );

        //
        //  Finally, now that we have written the forget record, we can free
        //  any exclusive Scbs that we have been holding.
        //

        while (!IsListEmpty(&IrpContext->ExclusiveFcbList)) {

            Fcb = (PFCB)CONTAINING_RECORD(IrpContext->ExclusiveFcbList.Flink,
                                          FCB,
                                          ExclusiveFcbLinks );

            NtfsReleaseFcb( IrpContext, Fcb );
        }

        //
        //  Remember any saved status code.
        //

        if (Vcb->DeleteUsnData.FinalStatus != STATUS_SUCCESS) {

            Status = Vcb->DeleteUsnData.FinalStatus;

            //
            //  Since we failed make sure to leave the flag set in the Vcb which indicates the
            //  incomplete delete.
            //

            SetFlag( Vcb->VcbState, VCB_STATE_INCOMPLETE_USN_DELETE );
        }

        //
        //  Cleanup the context and flags in the Vcb.
        //

        RtlZeroMemory( &Vcb->DeleteUsnData, sizeof( NTFS_DELETE_JOURNAL_DATA ));
        RtlZeroMemory( &Vcb->UsnJournalInstance, sizeof( USN_JOURNAL_INSTANCE ));
        Vcb->FirstValidUsn = 0;
        Vcb->LowestOpenUsn = 0;

        ClearFlag( Vcb->VcbState, VCB_STATE_USN_JOURNAL_PRESENT | VCB_STATE_USN_DELETE );

        //
        //  Finally complete all of the waiting Irps in the Usn notify queue.
        //

        NtfsAcquireUsnNotify( Vcb );

        Links = Vcb->NotifyUsnDeleteIrps.Flink;

        while (Links != &Vcb->NotifyUsnDeleteIrps) {

            UsnNotifyIrp = CONTAINING_RECORD( Links,
                                              IRP,
                                              Tail.Overlay.ListEntry );

            //
            //  Remember to move forward in any case.
            //

            Links = Links->Flink;

            //
            //  Clear the notify routine and detect if cancel has
            //  already been called.
            //

            if (NtfsClearCancelRoutine( UsnNotifyIrp )) {

                RemoveEntryList( &UsnNotifyIrp->Tail.Overlay.ListEntry );
                NtfsCompleteRequest( NULL, UsnNotifyIrp, Status );
            }
        }

        NtfsReleaseUsnNotify( Vcb );

    } except( NtfsExceptionFilter( IrpContext, GetExceptionInformation())) {

        Status = IrpContext->TopLevelIrpContext->ExceptionStatus;
    }

    if (AcquiredVcb) {

        NtfsReleaseVcb( IrpContext, Vcb );
    }

    //
    //  If this is a fatal failure then do any final cleanup.
    //

    if (!NT_SUCCESS( Status )) {

        NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
    }

    return;

    UNREFERENCED_PARAMETER( Context );
}


//
//  Local support routine
//

RTL_GENERIC_COMPARE_RESULTS
NtfsUsnTableCompare (
    IN PRTL_GENERIC_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )

/*++

Routine Description:

    This is a generic table support routine to compare two File References
    in Usn Records.

Arguments:

    Table - Supplies the generic table being queried.  Not used.

    FirstStruct - Supplies the first Usn Record to compare

    SecondStruct - Supplies the second Usn Record to compare

Return Value:

    RTL_GENERIC_COMPARE_RESULTS - The results of comparing the two
        input structures

--*/

{
    PAGED_CODE();

    if (*((PLONGLONG) &((PUSN_RECORD) FirstStruct)->FileReferenceNumber) <
        *((PLONGLONG) &((PUSN_RECORD) SecondStruct)->FileReferenceNumber)) {

        return GenericLessThan;
    }

    if (*((PLONGLONG) &((PUSN_RECORD) FirstStruct)->FileReferenceNumber) >
        *((PLONGLONG) &((PUSN_RECORD) SecondStruct)->FileReferenceNumber)) {

        return GenericGreaterThan;
    }

    return GenericEqual;

    UNREFERENCED_PARAMETER( Table );
}


//
//  Local support routine
//

PVOID
NtfsUsnTableAllocate (
    IN PRTL_GENERIC_TABLE Table,
    CLONG ByteSize
    )

/*++

Routine Description:

    This is a generic table support routine to allocate memory

Arguments:

    Table - Supplies the generic table being used

    ByteSize - Supplies the number of bytes to allocate

Return Value:

    PVOID - Returns a pointer to the allocated data

--*/

{
    UNREFERENCED_PARAMETER( Table );

    PAGED_CODE();

    return NtfsAllocatePool( PagedPool, ByteSize );
}


//
//  Local support routine
//

VOID
NtfsUsnTableFree (
    IN PRTL_GENERIC_TABLE Table,
    IN PVOID Buffer
    )

/*++

Routine Description:

    This is a generic table support routine to free memory

Arguments:

    Table - Supplies the generic table being used

    Buffer - Supplies pointer to the buffer to be freed

Return Value:

    None

--*/

{
    UNREFERENCED_PARAMETER( Table );

    PAGED_CODE();

    NtfsFreePool( Buffer );
}


//
//  Local support routine
//

VOID
NtfsCancelReadUsnJournal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine may be called by the I/O system to cancel an outstanding
    Irp in NtfsReadUsnJournal.

Arguments:

    DeviceObject - DeviceObject from I/O system

    Irp - Supplies the pointer to the Irp being canceled.

Return Value:

    None

--*/

{
    PWAIT_FOR_NEW_LENGTH WaitForNewLength;

    IoSetCancelRoutine( Irp, NULL );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    //  Capture the Wait block out of the Status field.  We know the Irp can't
    //  go away at this point.
    //

    WaitForNewLength = (PWAIT_FOR_NEW_LENGTH) Irp->IoStatus.Information;
    Irp->IoStatus.Information = 0;

    //
    //  Take a different action depending on whether we are completing the irp
    //  or simply signaling the cancel.
    //


    //
    //  This is the async case.  We can simply complete this irp.
    //

    if (FlagOn( WaitForNewLength->Flags, NTFS_WAIT_FLAG_ASYNC )) {

        //
        //  Acquire the mutex in order to remove this from the list and complete
        //  the Irp.
        //

        ExAcquireFastMutex( WaitForNewLength->Stream->Header.FastMutex );
        RemoveEntryList( &WaitForNewLength->WaitList );
        ExReleaseFastMutex( WaitForNewLength->Stream->Header.FastMutex );

        InterlockedDecrement( &WaitForNewLength->Stream->CloseCount );

        NtfsCompleteRequest( NULL, Irp, STATUS_CANCELLED );
        NtfsFreePool( WaitForNewLength );

    //
    //  If there is not an Irp we simply signal the event and let someone else
    //  do the work.  This is the synchronous case.
    //

    } else {

        WaitForNewLength->Status = STATUS_CANCELLED;
        KeSetEvent( &WaitForNewLength->Event, 0, FALSE );
    }

    return;
    UNREFERENCED_PARAMETER( DeviceObject );
}


//
//  Local support routine
//

VOID
NtfsCancelDeleteUsnJournal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine may be called by the I/O system to cancel an outstanding
    Irp waiting for the usn journal to be deleted.

Arguments:

    DeviceObject - DeviceObject from I/O system

    Irp - Supplies the pointer to the Irp being canceled.

Return Value:

    None

--*/

{
    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    PVCB Vcb;

    //
    //  Block out future cancels.
    //

    IoSetCancelRoutine( Irp, NULL );

    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Capture the Vcb so we can do the necessary synchronization.
    //

    FileObject = IrpSp->FileObject;
    Vcb = ((PSCB)(FileObject->FsContext))->Vcb;

    //
    //  Acquire the list and remove the Irp.  Complete the Irp with
    //  STATUS_CANCELLED.
    //

    NtfsAcquireUsnNotify( Vcb );
    RemoveEntryList( &Irp->Tail.Overlay.ListEntry );
    NtfsReleaseUsnNotify( Vcb );

    Irp->IoStatus.Information = 0;
    NtfsCompleteRequest( NULL, Irp, STATUS_CANCELLED );

    return;

    UNREFERENCED_PARAMETER( DeviceObject );
}


//
//  Local support routine
//

NTSTATUS
NtfsDeleteUsnWorker (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    )

/*++

Routine Description:

    This routines resets the Usn in the file record for the Fcb to zero.

Arguments:

    IrpContext - context of the call

    Fcb - Fcb for the file record to clear

    Context - Unused

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PVOID UsnRecord;

    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    PATTRIBUTE_RECORD_HEADER Attribute;
    STANDARD_INFORMATION NewStandardInformation;
    USN Usn = 0;

    PAGED_CODE();

    //
    //  Initialize the search context.
    //

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-except to catch all of the errors.
    //

    try {

        //
        //  Use a try-finally to facilitate cleanup.
        //

        try {

            //
            //  Look up the standard information attribute and modify the usn field if
            //  the attribute is found and it is a large standard attribute.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO ) &&
                NtfsLookupAttributeByCode( IrpContext,
                                           Fcb,
                                           &Fcb->FileReference,
                                           $STANDARD_INFORMATION,
                                           &AttrContext )) {

                Attribute = NtfsFoundAttribute( &AttrContext );

                if (((PSTANDARD_INFORMATION) NtfsAttributeValue( Attribute ))->Usn != 0) {

                    RtlCopyMemory( &NewStandardInformation,
                                   NtfsAttributeValue( Attribute ),
                                   sizeof( STANDARD_INFORMATION ));

                    NewStandardInformation.Usn = 0;

                    NtfsChangeAttributeValue( IrpContext,
                                              Fcb,
                                              0,
                                              &NewStandardInformation,
                                              sizeof( STANDARD_INFORMATION ),
                                              FALSE,
                                              FALSE,
                                              FALSE,
                                              FALSE,
                                              &AttrContext );
                }
            }

            //
            //  Make sure the Fcb reflects this change.
            //

            NtfsLockFcb( IrpContext, Fcb );

            Fcb->Usn = 0;
            UsnRecord = Fcb->FcbUsnRecord;
            Fcb->FcbUsnRecord = NULL;

            NtfsUnlockFcb( IrpContext, Fcb );

            if (UsnRecord != NULL) {

                NtfsFreePool( UsnRecord );
            }

        } finally {

            //
            //  Be sure to clean up the context.
            //

            NtfsCleanupAttributeContext( IrpContext, &AttrContext );
        }

    //
    //  We want to swallow any expected errors except LOG_FILE_FULL and CANT_WAIT.
    //

    } except ((FsRtlIsNtstatusExpected( Status = GetExceptionCode()) &&
               (Status != STATUS_LOG_FILE_FULL) &&
               (Status != STATUS_CANT_WAIT)) ?
              EXCEPTION_EXECUTE_HANDLER :
              EXCEPTION_CONTINUE_SEARCH) {

        NOTHING;
    }

    //
    //  Always return success from this routine.
    //

    IrpContext->ExceptionStatus = STATUS_SUCCESS;
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( Context );
}


//
//  Local support routine
//

BOOLEAN
NtfsValidateUsnPage (
    IN PUSN_RECORD UsnRecord,
    IN USN PageUsn,
    IN USN *UserStartUsn OPTIONAL,
    IN LONGLONG UsnFileSize,
    OUT PBOOLEAN ValidUserStartUsn OPTIONAL,
    OUT USN *NextUsn
    )

/*++

Routine Description:

    This routine checks the offsets within a single page of the usn journal.  This allows the caller to
    then walk safely through the page.

Arguments:

    UsnRecord - Pointer to the start of the Usn page.

    PageUsn - This is the Usn for the first record of the page.

    UserStartUsn - If specified then do an additional check that the user's specified usn in fact
        lies correctly on this page.  The output boolean must also be specified if this is.

    UsnFileSize - This is the current size of the usn journal.  If we are looking at the last page then
        we only check to this point.

    ValidUserStartUsn - Address to result of check on user specified start Usn.

    NextUsn - This is the Usn past the valid portion of the page.  It will point to a position on the
        current page unless the last record on the page completely fills the page.  If the page isn't valid
        then it points to the position where the invalid record was detected.

Return Value:

    BOOLEAN - TRUE if the page is valid until a legal terminating condition.  FALSE if there is internal
        corruption on the page.

--*/

{
    ULONG RemainingPageBytes;
    ULONG RecordLength;
    BOOLEAN ValidPage = TRUE;
    BOOLEAN FoundEntry = FALSE;

    PAGED_CODE();

    //
    //  Verify a few input values.
    //

    ASSERT( UsnFileSize > PageUsn );
    ASSERT( !FlagOn( *((PULONG) &UsnRecord), USN_PAGE_SIZE - 1 ));
    ASSERT( !ARGUMENT_PRESENT( UserStartUsn ) || ARGUMENT_PRESENT( ValidUserStartUsn ));
    ASSERT( !ARGUMENT_PRESENT( ValidUserStartUsn ) || ARGUMENT_PRESENT( UserStartUsn ));

    //
    //  Compute the Usn past the valid data on this page.  It is either the end of the journal or
    //  the next page of the journal.
    //

    RemainingPageBytes = USN_PAGE_SIZE;

    if (UsnFileSize < (PageUsn + USN_PAGE_SIZE)) {

        RemainingPageBytes = (ULONG) (UsnFileSize - PageUsn);
    }

    //
    //  Assume the user's Usn is invalid unless it wasn't specified.
    //

    if (!ARGUMENT_PRESENT( ValidUserStartUsn )) {

        ValidUserStartUsn = (PBOOLEAN) NtfsAllocateFromStack( sizeof( BOOLEAN ));
        *ValidUserStartUsn = TRUE;

    } else {

        *ValidUserStartUsn = FALSE;
    }

    //
    //  Keep track of our current position in the page with the user's pointer.
    //

    *NextUsn = PageUsn;

    //
    //  Check each entry in the page for the following.
    //
    //      1 - Fixed portion of the header won't fit within the remaining bytes on the page.
    //      2 - Record header is zeroed.
    //      3 - Record length is not quad-aligned.
    //      4 - Record length is larger than the remaining bytes on the page.
    //      5 - Usn on the page doesn't match the computed value.
    //

    while (RemainingPageBytes != 0) {

        //
        //  Not enough bytes even for the full Usn header.
        //

        if (RemainingPageBytes < (FIELD_OFFSET( USN_RECORD, FileName ) + sizeof( WCHAR ))) {

            //
            //  If there is at least a ulong it better be zeroed.
            //

            if ((RemainingPageBytes >= sizeof( ULONG )) &&
                (UsnRecord->RecordLength != 0)) {

                ValidPage = FALSE;

            //
            //  If the user's Usn points to this offset then it is valid.
            //

            } else if (!(*ValidUserStartUsn) &&
                        (*NextUsn == *UserStartUsn)) {

                *ValidUserStartUsn = TRUE;
            }

            break;
        }

        //
        //  There should be at least one entry on the page.  We attempt to detect
        //  a local loss of data through zeroing but won't check to the end of
        //  the page.
        //

        RecordLength = UsnRecord->RecordLength;
        if (RecordLength == 0) {

            //
            //  Fail if we haven't found at least one entry.
            //

            if (!FoundEntry) {

                ValidPage = FALSE;

            //
            //  We know we should be dealing with the tail of the page.  It should
            //  be zeroed through the fixed portion of a Usn record.  Theoretically
            //  it should be zeroed to the end of the page but we will assume that we
            //  are only looking for local corruption.  If we lost data through the
            //  end of the page we can't detect it anyway.
            //

            } else {

                PCHAR CurrentByte = (PCHAR) UsnRecord;
                ULONG Count = FIELD_OFFSET( USN_RECORD, FileName ) + sizeof( WCHAR );

                while (Count != 0) {

                    if (*CurrentByte != 0) {

                        ValidPage = FALSE;
                        break;
                    }

                    Count -= 1;
                    CurrentByte += 1;
                }

                //
                //  If the page is valid then check if the user's Usn is at this point.  It is
                //  legal for him to specify the point where the zeroes begin.
                //

                if (ValidPage &&
                    !(*ValidUserStartUsn) &&
                    (*NextUsn == *UserStartUsn)) {

                    *ValidUserStartUsn = TRUE;
                }
            }

            break;
        }

        //
        //  Invalid if record length is not-quad aligned or is larger than
        //  remaining bytes on the page.
        //

        if (FlagOn( RecordLength, sizeof( ULONGLONG ) - 1 ) ||
            (RecordLength > RemainingPageBytes)) {

            ValidPage = FALSE;
            break;
        }

        //
        //  Now check that the Usn is the expected value.
        //

        if (UsnRecord->Usn != *NextUsn) {

            ValidPage = FALSE;
            break;
        }

        //
        //  Remember that we found a valid entry.
        //

        FoundEntry = TRUE;

        //
        //  If the user's Usn matches this one then remember his is valid.
        //

        if (!(*ValidUserStartUsn) &&
            (*NextUsn == *UserStartUsn)) {

            *ValidUserStartUsn = TRUE;
        }

        //
        //  Advance to the next record in the page.
        //

        UsnRecord = Add2Ptr( UsnRecord, RecordLength );

        RemainingPageBytes -= RecordLength;
        *NextUsn += RecordLength;
    }

    return ValidPage;
}
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     /*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    VAttrSup.c

Abstract:

    This module implements the attribute routines for NtOfs

Author:

    Tom Miller      [TomM]          10-Apr-1996

Revision History:

--*/

#include "NtfsProc.h"

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('vFtN')

#undef NtOfsMapAttribute
NTFSAPI
VOID
NtOfsMapAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG Offset,
    IN ULONG Length,
    OUT PVOID *Buffer,
    OUT PMAP_HANDLE MapHandle
    );

#undef NtOfsPreparePinWrite
NTFSAPI
VOID
NtOfsPreparePinWrite (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG Offset,
    IN ULONG Length,
    OUT PVOID *Buffer,
    OUT PMAP_HANDLE MapHandle
    );

#undef NtOfsPinRead
NTFSAPI
VOID
NtOfsPinRead(
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG Offset,
    IN ULONG Length,
    OUT PMAP_HANDLE MapHandle
    );

#undef NtOfsReleaseMap
NTFSAPI
VOID
NtOfsReleaseMap (
    IN PIRP_CONTEXT IrpContext,
    IN PMAP_HANDLE MapHandle
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtOfsCreateAttribute)
#pragma alloc_text(PAGE, NtOfsCreateAttributeEx)
#pragma alloc_text(PAGE, NtOfsCloseAttribute)
#pragma alloc_text(PAGE, NtOfsDeleteAttribute)
#pragma alloc_text(PAGE, NtOfsQueryLength)
#pragma alloc_text(PAGE, NtOfsSetLength)
#pragma alloc_text(PAGE, NtfsHoldIrpForNewLength)
#pragma alloc_text(PAGE, NtOfsPostNewLength)
#pragma alloc_text(PAGE, NtOfsFlushAttribute)
#pragma alloc_text(PAGE, NtOfsPutData)
#pragma alloc_text(PAGE, NtOfsMapAttribute)
#pragma alloc_text(PAGE, NtOfsReleaseMap)
#endif


NTFSAPI
NTSTATUS
NtOfsCreateAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN UNICODE_STRING Name,
    IN CREATE_OPTIONS CreateOptions,
    IN ULONG LogNonresidentToo,
    OUT PSCB *ReturnScb
    )

/*++

Routine Description:

    This routine may be called to create / open a named data attribute
    within a given file, which may or may not be recoverable.

Arguments:

    Fcb - File in which the attribute is to be created.  It is acquired exclusive

    Name - Name of the attribute for all related Scbs and attributes on disk.

    CreateOptions - Standard create flags.

    LogNonresidentToo - Supplies nonzero if updates to the attribute should
                        be logged.

    ReturnScb - Returns an Scb as handle for the attribute.

Return Value:

    STATUS_OBJECT_NAME_COLLISION -- if CreateNew and attribute already exists
    STATUS_OBJECT_NAME_NOT_FOUND -- if OpenExisting and attribute does not exist

--*/

{
    return NtOfsCreateAttributeEx( IrpContext,
                                   Fcb,
                                   Name,
                                   $DATA,
                                   CreateOptions,
                                   LogNonresidentToo,
                                   ReturnScb );
}


NTFSAPI
NTSTATUS
NtOfsCreateAttributeEx (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN UNICODE_STRING Name,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN CREATE_OPTIONS CreateOptions,
    IN ULONG LogNonresidentToo,
    OUT PSCB *ReturnScb
    )

/*++

Routine Description:

    This routine may be called to create / open a named data attribute
    within a given file, which may or may not be recoverable.

Arguments:

    Fcb - File in which the attribute is to be created.  It is acquired exclusive

    Name - Name of the attribute for all related Scbs and attributes on disk.

    CreateOptions - Standard create flags.

    LogNonresidentToo - Supplies nonzero if updates to the attribute should
                        be logged.

    ReturnScb - Returns an Scb as handle for the attribute.

Return Value:

    STATUS_OBJECT_NAME_COLLISION -- if CreateNew and attribute already exists
    STATUS_OBJECT_NAME_NOT_FOUND -- if OpenExisting and attribute does not exist

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT LocalContext;
    BOOLEAN FoundAttribute;
    NTSTATUS Status = STATUS_SUCCESS;
    PSCB Scb = NULL;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT( NtfsIsExclusiveFcb( Fcb ));

    PAGED_CODE();

    if (AttributeTypeCode != $DATA &&
        AttributeTypeCode != $LOGGED_UTILITY_STREAM) {

        ASSERTMSG( "Invalid attribute type code in NtOfsCreateAttributeEx", FALSE );

        *ReturnScb = NULL;
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Now, just create the Data Attribute.
    //

    NtfsInitializeAttributeContext( &LocalContext );

    try {

        //
        //  First see if the attribute already exists, by searching for the root
        //  attribute.
        //

        FoundAttribute = NtfsLookupAttributeByName( IrpContext,
                                                    Fcb,
                                                    &Fcb->FileReference,
                                                    AttributeTypeCode,
                                                    &Name,
                                                    NULL,
                                                    TRUE,
                                                    &LocalContext );

        //
        //  If it is not there, and the CreateOptions allow, then let's create
        //  the attribute root now.  (First cleaning up the attribute context from
        //  the lookup).
        //

        if (!FoundAttribute && (CreateOptions <= CREATE_OR_OPEN)) {

            //
            //  Make sure we acquire the quota resource before creating the stream.  Just
            //  in case we need the Mft during the create.
            //

            if (NtfsIsTypeCodeSubjectToQuota( AttributeTypeCode ) &&
                NtfsPerformQuotaOperation( Fcb )) {

                //
                //  The quota index must be acquired before the mft scb is acquired.
                //

                ASSERT( !NtfsIsExclusiveScb( Fcb->Vcb->MftScb ) ||
                        ExIsResourceAcquiredSharedLite( Fcb->Vcb->QuotaTableScb->Fcb->Resource ));

                NtfsAcquireQuotaControl( IrpContext, Fcb->QuotaControl );
            }

            NtfsCleanupAttributeContext( IrpContext, &LocalContext );

            NtfsCreateAttributeWithValue( IrpContext,
                                          Fcb,
                                          AttributeTypeCode,
                                          &Name,
                                          NULL,
                                          0,
                                          0,
                                          NULL,
                                          TRUE,
                                          &LocalContext );

        //
        //  If the attribute is already there, and we were asked to create it, then
        //  return an error.
        //

        } else if (FoundAttribute && (CreateOptions == CREATE_NEW)) {

            Status = STATUS_OBJECT_NAME_COLLISION;
            leave;

        //
        //  If the attribute is not there, and we  were supposed to open existing, then
        //  return an error.
        //

        } else if (!FoundAttribute) {

            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            leave;
        }

        //
        //  Otherwise create/find the Scb and reference it.
        //

        Scb = NtfsCreateScb( IrpContext, Fcb, AttributeTypeCode, &Name, FALSE, &FoundAttribute );

        //
        //  Make sure things are correctly reference counted
        //

        NtfsIncrementCloseCounts( Scb, TRUE, FALSE );

        //
        //  If we created the Scb, then get the no modified write set correctly.
        //

        ASSERT( !FoundAttribute ||
                (LogNonresidentToo == BooleanFlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE)) );

        if (!FoundAttribute && LogNonresidentToo) {
            SetFlag( Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE );
            Scb->Header.ValidDataLength.QuadPart = MAXLONGLONG;
        }

        //
        //  Make sure the stream can be mapped internally.  Defer this for the Usn journal
        //  until we set up the journal bias.
        //

        if ((Scb->FileObject == NULL) && !FlagOn( Scb->ScbPersist, SCB_PERSIST_USN_JOURNAL )) {
            NtfsCreateInternalAttributeStream( IrpContext, Scb, TRUE, NULL );
        }

        NtfsUpdateScbFromAttribute( IrpContext, Scb, NtfsFoundAttribute(&LocalContext) );

    } finally {

        if (AbnormalTermination( )) {
            if (Scb != NULL) {
                NtOfsCloseAttribute( IrpContext, Scb );
            }
        }

        NtfsCleanupAttributeContext( IrpContext, &LocalContext );
    }

    *ReturnScb = Scb;

    return Status;
}


NTFSAPI
VOID
NtOfsCloseAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to close a previously returned handle on an attribute.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this attribute.

Return Value:

    None.

--*/

{
    ASSERT( NtfsIsExclusiveFcb( Scb->Fcb ));

    PAGED_CODE();

    //
    //  We either need the caller to empty this list before closing (as assumed here),
    //  or possibly empty it here.  At this point it seems better to assume that the
    //  caller must take action to insure any waiting threads will shutdown and not
    //  touch the stream anymore, then call NtOfsPostNewLength to flush the queue.
    //  If the queue is nonempty here, maybe the caller didn't think this through!
    //

    ASSERT( IsListEmpty( &Scb->ScbType.Data.WaitForNewLength ) ||
            (Scb->CloseCount > 1) );

    NtfsDecrementCloseCounts( IrpContext, Scb, NULL, TRUE, FALSE, TRUE );
}


NTFSAPI
VOID
NtOfsDeleteAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to delete an attribute with type code
    $LOGGED_UTILITY_STREAM.

Arguments:

    Fcb - Supplies an Fcb as the previously returned object handle for the file

    Scb - Supplies an Scb as the previously returned handle for this attribute.

Return Value:

    None (Deleting a nonexistant index is benign).

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT LocalContext;
    BOOLEAN FoundAttribute;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, 0 );

    try {

        //
        //  Make sure we aren't deleting a data stream.  We do this after
        //  initializing the attribute context to make the finally clause simpler.
        //  This test can be removed if some trusted component using the NtOfs
        //  API has a legitimate need to delete other types of attributes.
        //

        NtfsInitializeAttributeContext( &LocalContext );

        if (Scb->AttributeTypeCode != $LOGGED_UTILITY_STREAM) {

            leave;
        }

        //
        //  First see if there is some attribute allocation, and if so truncate it
        //  away allowing this operation to be broken up.
        //

        if (NtfsLookupAttributeByName( IrpContext,
                                       Fcb,
                                       &Fcb->FileReference,
                                       Scb->AttributeTypeCode,
                                       &Scb->AttributeName,
                                       NULL,
                                       FALSE,
                                       &LocalContext )

                &&

            !NtfsIsAttributeResident( NtfsFoundAttribute( &LocalContext ))) {

            ASSERT( Scb->FileObject != NULL );

            NtfsDeleteAllocation( IrpContext, NULL, Scb, 0, MAXLONGLONG, TRUE, TRUE );
        }

        NtfsCleanupAttr //

            NewBias = FirstValidUsn & ~(USN_JOURNAL_CACHE_BIAS - 1);
            if (NewBias != 0) {
                NewBias -= USN_JOURNAL_CACHE_BIAS;
            }

            if (NewBias != Vcb->UsnCacheBias) {

                //
                //  Flush And Purge releases all resources in exclusive list so acquire
                //  the mft an extra time beforehand and restore back afterwards
                //

                NtfsAcquireResourceExclusive( IrpContext, Vcb->MftScb, TRUE );
                NtfsReleaseScb( IrpContext, Vcb->MftScb );
                AcquiredMft = TRUE;

                NtfsFlushAndPurgeScb( IrpContext, UsnJournal, NULL );
                Vcb->UsnCacheBias = NewBias;
                SavedBias = NewBias;
            }

            //
            //  If we reach here, then we can advance FirstValidUsn.  (Otherwise
            //  any retryable conditions will just resume work on this range.
            //

            Vcb->FirstValidUsn = FirstValidUsn;

        } finally {

            //
            //  Restore the error if we raised while deallocating.
            //

            if (SavedBias != Vcb->UsnCacheBias) {
                Vcb->UsnCacheBias = SavedBias;
            }

            if (SavedReserved != UsnJournal->ScbType.Data.TotalReserved) {

                NtfsAcquireReservedClusters( Vcb );
                Vcb->TotalReserved += LlClustersFromBytesTruncate( Vcb, SavedReserved );
                Vcb->TotalReserved -= LlClustersFromBytesTruncate( Vcb, RequiredReserved );
                UsnJournal->ScbType.Data.TotalReserved = SavedReserved;
                NtfsReleaseReservedClusters( Vcb );
            }

            NtfsReleaseScb( IrpContext, UsnJournal );

            if (AcquiredMft) {
                NtfsReleaseResource( IrpContext, Vcb->MftScb );
            } else {
                NtfsReleaseScb( IrpContext, Vcb->MftScb );
            }
        }

    } else {

        //
        //  Clear the checkpoint flags at this point.
        //

        NtfsAcquireCheckpoint( IrpContext, Vcb );
        ClearFlag( Vcb->CheckpointFlags,
                   VCB_CHECKPOINT_SYNC_FLAGS | VCB_DUMMY_CHECKPOINT_POSTED);

        NtfsSetCheckpointNotify( IrpContext, Vcb );
        NtfsReleaseCheckpoint( IrpContext, Vcb );
    }
}


NTSTATUS
NtfsQueryUsnJournal (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the worker routine which returns the information about the current instance
    of the Usn journal.

Arguments:

    Irp - This is the Irp for the request.

Return Value:

    NTSTATUS - Result for this request.

--*/

{
    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PUSN_JOURNAL_DATA JournalData;

    PAGED_CODE();

    //
    //  Always make this request synchronous.
    //

    SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );

    //
    //  Get the current stack location and extract the output
    //  buffer information.  The output parameter will receive
    //  the compressed state of the file/directory.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Get a pointer to the output buffer.  Look at the system buffer field in th
    //  irp first.  Then the Irp Mdl.
    //

    if (Irp->AssociatedIrp.SystemBuffer != NULL) {

        JournalData = (PUSN_JOURNAL_DATA) Irp->AssociatedIrp.SystemBuffer;

    } else if (Irp->MdlAddress != NULL) {

        JournalData = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

        if (JournalData == NULL) {

            NtfsCompleteRequest( IrpContext, Irp, STATUS_INSUFFICIENT_RESOURCES );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

    //
    //  Make sure the output buffer is large enough for the journal data.
    //

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < sizeof( USN_JOURNAL_DATA )) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

#ifdef __ND_NTFS_SECONDARY__

	if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) {
	
		IrpContext->InputBuffer	 = NULL;
		IrpContext->outputBuffer = JournalData;
		return STATUS_SUCCESS;
	}

#endif

    //
    //  Decode the file object.  We only support this call for volume opens.
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if ((Ccb == NULL) || !FlagOn( Ccb->AccessFlags, MANAGE_VOLUME_ACCESS)) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_ACCESS_DENIED );
        return STATUS_ACCESS_DENIED;
    }

    //
    //  Acquire shared access to the Scb and check that the volume is still mounted.
    //

    NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

    if (!FlagOn(Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED)) {

        NtfsReleaseVcb( IrpContext, Vcb );
        NtfsCompleteRequest( IrpContext, Irp, STATUS_VOLUME_DISMOUNTED );
        return STATUS_VOLUME_DISMOUNTED;
    }

    //
    //  Indicate if the journal is being deleted or has not started.
    //

    if (FlagOn( Vcb->VcbState, VCB_STATE_USN_DELETE )) {

        NtfsReleaseVcb( IrpContext, Vcb );
        NtfsCompleteRequest( IrpContext, Irp, STATUS_JOURNAL_DELETE_IN_PROGRESS );
        return STATUS_JOURNAL_DELETE_IN_PROGRESS;
    }

    if (!FlagOn( Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE )) {

        NtfsReleaseVcb( IrpContext, Vcb );
        NtfsCompleteRequest( IrpContext, Irp, STATUS_JOURNAL_NOT_ACTIVE );
        return STATUS_JOURNAL_NOT_ACTIVE;
    }

    //
    //  Otherwise serialize with the Usn journal and copy the data from the journal Scb
    //  and Vcb.
    //

    NtfsAcquireSharedScb( IrpContext, Vcb->UsnJournal );

    JournalData->UsnJournalID = Vcb->UsnJournalInstance.JournalId;
    JournalData->FirstUsn = Vcb->FirstValidUsn;
    JournalData->NextUsn = Vcb->UsnJournal->Header.FileSize.QuadPart;
    JournalData->LowestValidUsn = Vcb->UsnJournalInstance.LowestValidUsn;
    JournalData->MaxUsn = MAXFILESIZE;
    JournalData->MaximumSize = Vcb->UsnJournalInstance.MaximumSize;
    JournalData->AllocationDelta = Vcb->UsnJournalInstance.AllocationDelta;

    NtfsReleaseScb( IrpContext, Vcb->UsnJournal );

    ASSERT( JournalData->FirstUsn >= JournalData->LowestValidUsn );

    NtfsReleaseVcb( IrpContext, Vcb );
    Irp->IoStatus.Information = sizeof( USN_JOURNAL_DATA );

    NtfsCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
    return STATUS_SUCCESS;
}


NTSTATUS
NtfsDeleteUsnJournal (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called when the user want to delete the current usn journal.  This will
    initiate the work to scan the Mft and reset all usn values to zero and remove the
    UsnJournal file from the disk.

Arguments:

    Irp - This is the Irp for the request.

Return Value:

    NTSTATUS - Result for this request.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    PDELETE_USN_JOURNAL_DATA DeleteData;

    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN CheckpointHeld = FALSE;
    BOOLEAN AcquiredNotify = FALSE;
    PSCB ReleaseUsnJournal = NULL;

    PLIST_ENTRY Links;

    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  We always wait in this path.
    //

    SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );

    //
    //  Perform a check on the input buffer.
    //

    if (Irp->AssociatedIrp.SystemBuffer != NULL) {

        DeleteData = (PDELETE_USN_JOURNAL_DATA) Irp->AssociatedIrp.SystemBuffer;

    } else if (Irp->MdlAddress != NULL) {

        DeleteData = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

        if (DeleteData == NULL) {

            NtfsCompleteRequest( IrpContext, Irp, STATUS_INSUFFICIENT_RESOURCES );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

    if (IrpSp->Parameters.FileSystemControl.InputBufferLength < sizeof( DELETE_USN_JOURNAL_DATA )) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

#ifdef __ND_NTFS_SECONDARY__

	if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) {
	
		IrpContext->InputBuffer	 = DeleteData;
		IrpContext->outputBuffer = NULL;
		return STATUS_SUCCESS;
	}

#endif

    //
    //  Decode the file object type
    //

    NtfsDecodeFileObject( IrpContext,
                          IrpSp->FileObject,
                          &Vcb,
                          &Fcb,
                          &Scb,
                          &Ccb,
                          TRUE );

    if ((Ccb == NULL) || !FlagOn( Ccb->AccessFlags, MANAGE_VOLUME_ACCESS)) {

    
        NtfsCompleteRequest( IrpContext, Irp, STATUS_ACCESS_DENIED );
        return STATUS_ACCESS_DENIED;
    }

    //
    //  We only support deleting and waiting for delete.
    //

    if (DeleteData->DeleteFlags == 0) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    if (FlagOn( DeleteData->DeleteFlags, ~USN_DELETE_VALID_FLAGS )) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    if (NtfsIsVolumeReadOnly( Vcb )) {

        NtfsCompleteRequest( IrpContext, Irp, STATUS_MEDIA_WRITE_PROTECTED );
        return STATUS_MEDIA_WRITE_PROTECTED;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Serialize with chkpoints and acquire the Vcb.  We need to carefully remove
        //  the journal from the Vcb.
        //

        NtfsAcquireCheckpoint( IrpContext, Vcb );

        while (FlagOn( Vcb->CheckpointFlags, VCB_CHECKPOINT_IN_PROGRESS )) {

            //
            //  Release the checkpoint event because we cannot stop the log file now.
            //

            NtfsReleaseCheckpoint( IrpContext, Vcb );
            NtfsWaitOnCheckpointNotify( IrpContext, Vcb );
            NtfsAcquireCheckpoint( IrpContext, Vcb );
        }

        SetFlag( Vcb->CheckpointFlags, VCB_CHECKPOINT_IN_PROGRESS );
        NtfsResetCheckpointNotify( IrpContext, Vcb );
        NtfsReleaseCheckpoint( IrpContext, Vcb );
        CheckpointHeld = TRUE;

        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        VcbAcquired = TRUE;

        //
        //  Check that the volume is still mounted.
        //

        if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

            Status = STATUS_VOLUME_DISMOUNTED;
            leave;
        }

        //
        //  If the user wants to delete the journal then make sure the delete hasn't
        //  already started.
        //

        if (FlagOn( DeleteData->DeleteFlags, USN_DELETE_FLAG_DELETE )) {

            //
            //  If the journal is already being deleted and this caller wanted to
            //  do the delete then let him know it has already begun.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_USN_DELETE )) {

                Status = STATUS_JOURNAL_DELETE_IN_PROGRESS;
                leave;
            }

            //
            //  Proceed with the delete if there is a Usn journal on disk.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_USN_JOURNAL_PRESENT ) ||
                (Vcb->UsnJournal != NULL)) {

                PSCB UsnJournal = Vcb->UsnJournal;

                //
                //  If the journal is running then the caller needs to match the journal ID.
                //

                if ((UsnJournal != NULL) &&
                    (DeleteData->UsnJournalID != Vcb->UsnJournalInstance.JournalId)) {

                    Status = STATUS_INVALID_PARAMETER;
                    leave;
                }

                //
                //  Write the bit to disk to indicate that the journal is being deleted.
                //  Checkpoint the transaction.
                //

                NtfsSetVolumeInfoFlagState( IrpContext,
                                            Vcb,
                                            VOLUME_DELETE_USN_UNDERWAY,
                                            TRUE,
                                            TRUE );

                NtfsCheckpointCurrentTransaction( IrpContext );

                //
                //  We are going to proceed with the delete.  Clear the flag in the Vcb that
                //  indicates the journal is active.  Then acquire and drop all of the files in
                //  order to serialize with anyone using the journal.
                //

                ClearFlag( Vcb->VcbState, VCB_STATE_USN_JOURNAL_ACTIVE );

                NtfsAcquireAllFiles( IrpContext,
                                     Vcb,
                                     TRUE,
                                     TRUE,
                                     TRUE );

                ReleaseUsnJournal = UsnJournal;
                if (UsnJournal != NULL) {

                    NtfsAcquireExclusiveScb( IrpContext, UsnJournal );
                }

                //
                //  Set the delete flag in the Vcb and remove the journal from the Vcb.
                //

                SetFlag( Vcb->VcbState, VCB_STATE_USN_DELETE );
                NtfsSetSegmentNumber( &Vcb->DeleteUsnData.DeleteUsnFileReference,
                                      0,
                                      MASTER_FILE_TABLE_NUMBER );

                Vcb->DeleteUsnData.DeleteUsnFileReference.SequenceNumber = 0;
                Vcb->DeleteUsnData.DeleteState = 0;
                Vcb->DeleteUsnData.PriorJournalScb = Vcb->UsnJournal;
                Vcb->UsnJournal = NULL;

                if (UsnJournal != NULL) {

                    //
                    //  Let's purge the data in the Usn journal and clear the bias
                    //  and file reference numbers in the Vcb.
                    //

                    CcPurgeCacheSection( &UsnJournal->NonpagedScb->SegmentObject,
                                         NULL,
                                         0,
                                         FALSE );

                    ClearFlag( UsnJournal->ScbPersist, SCB_PERSIST_USN_JOURNAL );
                }

                Vcb->UsnCacheBias = 0;
                *((PLONGLONG) &Vcb->UsnJournalReference) = 0;

                //
                //  Release the checkpoint if held.
                //

                NtfsAcquireCheckpoint( IrpContext, Vcb );
                ClearFlag( Vcb->CheckpointFlags, VCB_CHECKPOINT_IN_PROGRESS );
                NtfsSetCheckpointNotify( IrpContext, Vcb );
                NtfsReleaseCheckpoint( IrpContext, Vcb );
                CheckpointHeld = FALSE;

                //
                //  Walk through the Irps waiting for new Usn data and cause them to be completed.
                //

                if (UsnJournal != NULL) {

                    PWAIT_FOR_NEW_LENGTH Waiter, NextWaiter;

                    ExAcquireFastMutex( UsnJournal->Header.FastMutex );
                    Waiter = (PWAIT_FOR_NEW_LENGTH) UsnJournal->ScbType.Data.WaitForNewLength.Flink;

                    while (Waiter != (PWAIT_FOR_NEW_LENGTH) &UsnJournal->ScbType.Data.WaitForNewLength) {

                        NextWaiter = (PWAIT_FOR_NEW_LENGTH) Waiter->WaitList.Flink;

                        //
                        //  We want to complete all of the Irps on the waiting list.  If cancel
                        //  has already been called on the Irp we don't have to do anything.
                        //  Otherwise complete the async Irps and signal the event on
                        //  the sync irps.
                        //

                        if (NtfsClearCancelRoutine( Waiter->Irp )) {

                            //
                            //  If this is an async request then complete the Irp.
                            //

                            if (FlagOn( Waiter->Flags, NTFS_WAIT_FLAG_ASYNC )) {

                                //
                                //  Make sure we decrement the reference count in the Scb.
                                //  Then remove the waiter from the queue and complete the Irp.
                                //

                                InterlockedDecrement( &UsnJournal->CloseCount );
                                RemoveEntryList( &Waiter->WaitList );

                                NtfsCompleteRequest( NULL, Waiter->Irp, STATUS_JOURNAL_DELETE_IN_PROGRESS );
                                NtfsFreePool( Waiter );

                            //
                            //  This is a synch Irp.  All we can do is set the event and note the status
                            //  code.
                            //

                            } else {

                                Waiter->Status = STATUS_JOURNAL_DELETE_IN_PROGRESS;
                                KeSetEvent( &Waiter->Event, 0, FALSE );
                            }
                        }

                        Waiter = NextWaiter;
                    }


                    //
                    //  Walk through all of the Fcb Usn records and deallocate them.
                    //

                    Links = Vcb->ModifiedOpenFiles.Flink;

                    while (Vcb->ModifiedOpenFiles.Flink != &Vcb->ModifiedOpenFiles) {

                        RemoveEntryList( Links );
                        Links->Flink = NULL;

                        //
                        //  Look to see if we need to remove the TimeOut link as well.
                        //

                        Links = &(CONTAINING_RECORD( Links, FCB_USN_RECORD, ModifiedOpenFilesLinks ))->TimeOutLinks;

                        if (Links->Flink != NULL) {

                            RemoveEntryList( Links );
                        }

                        Links = Vcb->ModifiedOpenFiles.Flink;
                    }

                    ExReleaseFastMutex( UsnJournal->Header.FastMutex );

                    //
                    //  Make sure remove our reference on the Usn journal.
                    //

                    NtOfsCloseAttributeSafe( IrpContext, UsnJournal );
                    ReleaseUsnJournal = NULL;
                }

                //
                //  If this caller wants to wait for this then acquire the notify
                //  mutex now.
                //

                if (FlagOn( DeleteData->DeleteFlags, USN_DELETE_FLAG_NOTIFY )) {

                    NtfsAcquireUsnNotify( Vcb );
                    AcquiredNotify = TRUE;
                }

                //
                //  Post the work item to do the rest of the delete.
                //

                NtfsPostSpecial( IrpContext, Vcb, NtfsDeleteUsnSpecial, &Vcb->DeleteUsnData );
            }
        }

        //
        //  Check if our caller wants to wait for the delete to complete.
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_USN_DELETE ) &&
            FlagOn( DeleteData->DeleteFlags, USN_DELETE_FLAG_NOTIFY )) {

            if (!AcquiredNotify) {

                NtfsAcquireUsnNotify( Vcb );
                AcquiredNotify = TRUE;
            }

            Status = STATUS_PENDING;
            if (!NtfsSetCancelRoutine( Irp,
                                       NtfsCancelDeleteUsnJournal,FILE0  „Sr      8  X                ®é           `           H      j‡U©´EÃ 'Â6zb j‡U©´EÃ§ìë„tLÃ                    
          ¯g:    0   p          T     úé    j‡U©´EÃj‡U©´EÃj‡U©´EÃj‡U©´EÃ                        	l s u t i l s . h     Ä   H                        @               ü      ü      ASπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ›Ur      8  –                ©é           `           H      j‡U©´EÃ 'Â6zb j‡U©´EÃD“ä„tLÃ                    
          ®i:    0   p          V     úé    j‡U©´EÃj‡U©´EÃj‡U©´EÃj‡U©´EÃ                       
N D F S I N ~ 1 . H c 0   x          ^     úé    j‡U©´EÃj‡U©´EÃj‡U©´EÃj‡U©´EÃ                       N d f s I n t e f a c e . h   Ä   H                         @              ¿      ¿      AUπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  0Vr      8  ‡                ™é           `           H      j‡U©´EÃ 'Â6zb j‡U©´EÃ¶ã„tLÃ                    
          †k:    0   p          V     úé    j‡U©´EÃj‡U©´EÃj‡U©´EÃj‡U©´EÃ @                      
N D F S P R ~ 1 . H o 0   à          l     úé    j‡U©´EÃj‡U©´EÃj‡U©´EÃj‡U©´EÃ @                      N d f s P r o t o c o l H e a d e r 2 . h     Ä   H                        @        @      í<      í<      AVπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  'Xr      8  –                ´é           `           H      
gW©´EÃ 'Â6zb 
gW©´EÃ¸Íë„tLÃ                    
          hm:    0   p          V     úé    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       
T C P T D I ~ 1 . H . 0   x          Z     úé    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       t c p t d i p r o c . h       Ä   H                         @              *      *      AZπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  CXr      8  –                ¨é           `           H      
gW©´EÃ 'Â6zb 
gW©´EÃ0Rí„tLÃ                    
           o:    0   p          V     úé    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       
W I N S C S ~ 1 . H . 0   x          Z     úé    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       w i n s c s i e x t . h       Ä   H                         @              ≤      ≤      A[πÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  r≤Pq      8  @                ≠é           `           H      
gW©´EÃãÅ]©´EÃãÅ]©´EÃò ?¢ÇLÃ                   	          Hp:    0   h          J     úé    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                      . s v n       0   h          L     úé    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                      S V N ~ 1     ê   X        8       $ I 3 0 0               (   (                            †   P   @                    H                           $ I 3 0 A^πÖ  ˇ∞   (               $ I 3 0        ˇˇˇˇÇyG
gW©´EÃ       ˆ	              A L L - W C ~ 1       Øé    ` P     ≠é    
gW©´EÃ 'Â6zb 
gW©´EÃ
gW©´EÃ       @
              e n t r i e s ∞é    h T     ≠é    
gW©´EÃ ∆@‚êx 
gW©´EÃ
gW©´EÃ                       	p r o p - b a s e     ∞é    h R     ≠é    
gW©´EÃ ∆@‚êx 
gW©´EÃ
gW©´EÃ                       P R O P - B ~ 1                     ˇˇˇˇÇyG                                                               FILE0  _Xr      8  »                Æé           `           H      
gW©´EÃ 'Â6zb 
gW©´EÃÆ§ô„tLÃ                    
          q:    0   p          R     ≠é    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       A L L - W C ~ 1 o p s 0   p          X     ≠é    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       a l l - w c p r o p s Ä   H                         @              ˆ	      ˆ	      A\πÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  HYr      8  P                Øé           `           H      
gW©´EÃ 'Â6zb 
gW©´EÃÿﬁô„tLÃ                    
          às:    0   h          P     ≠é    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       e n t r i e s Ä   H                         @              @
      @
      A]πÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ˜hPq      8  –                ∞é           `           H      
gW©´EÃ ∆@‚êx 
gW©´EÃ…?¢ÇLÃ                    	          xt:    0   p          R     ≠é    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       P R O P - B ~ 1 e     0   p          T     ≠é    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       	p r o p - b a s e     ê   P        0       $ I 3 0 0                                         ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ∆lPq      8  X                ±é           `           H      
gW©´EÃ ∆@‚êx ´ÌX©´EÃÚﬁ?¢ÇLÃ                    	          Xu:    0   h          L     ≠é    
gW©´EÃ
gW©´EÃ
gW©´EÃ
gW©´EÃ                       p r o p s     ê   P        0       $ I 3 0 0                                         ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ö¢Pq      8  P                ≤é           `           H      ´ÌX©´EÃãÅ]©´EÃãÅ]©´EÃP˘?¢ÇLÃ                    	          @v:    0   p          R     ≠é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       T E X T - B ~ 1 e     0   p          T     ≠é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       	t e x t - b a s e     ê   X        8       $ I 3 0 0               (   (                            †   P   @                    H                           $ I 3 0 AbπÖ   ∞   (               $ I 3 0        ˇˇˇˇÇyG 'Â6zb ´ÌX©´EÃ´ÌX©´EÃ       >	              C I P H E R ~ 1 . S V N       ¥é    x d     ≤é    ´ÌX©´EÃ 'Â6zb ´ÌX©´EÃ´ÌX©´EÃ       >
              d e v r e g . h . s v n - b a s e     ¥é    p Z     ≤é    ´ÌX©´EÃ 'Â6zb ´ÌX©´EÃ´ÌX©´EÃ       >
              D E V R E G ~ 1 . S V N                     ˇˇˇˇÇyG                                                                                               FILE0  GZr      8  ‡                ≥é           `           H      ´ÌX©´EÃ 'Â6zb ´ÌX©´EÃLÄù„tLÃ                    
          x:    0   x          Z     ≤é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       C I P H E R ~ 1 . S V N - b a 0   Ä          d     ≤é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       c i p h e r . h . s v n - b a s e     Ä   H                         @              >	      >	      A_πÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  cZr      8  ‡                ¥é           `           H      ´ÌX©´EÃ 'Â6zb ´ÌX©´EÃB’ù„tLÃ                    
          y:    0   x          Z     ≤é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       D E V R E G ~ 1 . S V N - b a 0   Ä          d     ≤é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       d e v r e g . h . s v n - b a s e     Ä   H                         @              >
      >
      A`πÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  Zr      8  ‡                µé           `           H      ´ÌX©´EÃ 'Â6zb ´ÌX©´EÃ|˙ù„tLÃ                    
          –{:    0   x          Z     ≤é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       K D E B U G ~ 1 . S V N - b a 0   Ä          d     ≤é    ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ´ÌX©´EÃ                       k d e b u g . h . s v n - b a s e     Ä   H                         @              ˘      ˘      AaπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ë[r      8  ‡                ∂é           `           H      KtZ©´EÃ 'Â6zb KtZ©´EÃä<û„tLÃ                    
          ∞}:    0   x          Z     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ                        L P X T D I ~ 1 . S V N - b a 0   Ä          d     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ                        l p x t d i . h . s v n - b a s e     Ä   H                        @               å      å      AcπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ¶\r      8  ‡                ∑é           `           H      KtZ©´EÃ 'Â6zb KtZ©´EÃLû„tLÃ                    
          ê:    0   x          Z     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ `                      L S C C B H ~ 1 . S V N b a s 0   Ä          b     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ `                      l s c c b . h . s v n - b a s e       Ä   H                        @        `      ÔX      ÔX      AeπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ¬\r      8  ‡                ∏é           `           H      KtZ©´EÃ 'Â6zb KtZ©´EÃd‚†„tLÃ                    
          ÄÅ:    0   x          Z     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ                       L S K L I B ~ 1 . S V N - b a 0   Ä          d     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ                       l s k l i b . h . s v n - b a s e     Ä   H                         @              õ      õ      AkπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ]r      8  ‡                πé           `           H      KtZ©´EÃ 'Â6zb KtZ©´EÃ>°„tLÃ                    
          `É:    0   x          Z     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ 0                      L S L U R N ~ 1 . S V N - b a 0   Ä          d     ≤é    KtZ©´EÃKtZ©´EÃKtZ©´EÃKtZ©´EÃ 0                      l s l u r n . h . s v n - b a s e     Ä   H                        @        0      ›(      ›(      AlπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  :_r      8  Ë                ∫é           `           H      Î˙[©´EÃ 'Â6zb Î˙[©´EÃÜQ°„tLÃ                    
          `Ö:    0   x          Z     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ                        L S L U R N ~ 2 . S V N s v n 0   à          j     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ                        l s l u r n i d e . h . s v n - b a s e       Ä   H                        @               ~      ~      AoπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  É`r      8  ‡                ªé           `           H      Î˙[©´EÃ 'Â6zb Î˙[©´EÃ‰Œ¢„tLÃ                    
          Há:    0   x          Z     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ @                      L S P R O T ~ 1 . S V N n - b 0   Ä          f     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ @                      l s p r o t o . h . s v n - b a s e   Ä   H                        @        @      1      1      AqπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  kôPq      8  –                ºé   "       `           H      Î˙[©´EÃ 'Â6zb Î˙[©´EÃ
£„tLÃ                    
          Hâ:    0   x          Z     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ                      L S P R O T ~ 2 . S V N . s v 0   à          l     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ                      l s p r o t o i d e . h . s v n - b a s e     Ä   0             #ifndef _LANSCSI_IDE_PROTOCOL_H_
#define _LANSCSI_IDE_PROTOCOL_H_

#include lpxtdi.h"
#include "lanscsi.h"
#include "binparams.h"
#include "lsproto.h"
#include "lsprotoidespec.h"


//////////////////////////////////////////////////////////////////////////
//
//	Export variables
//

extern LANSCSIPROTOCOL_INTERFACE	LsProtoIdeV10Interface ;
extern LANSCSIPROTOCOL_INTERFACE	LsProtoIdeV11Interface ;

//////////////////////////////////////////////////////////////////////////
//
//	Export functions
//

#endif    ˇˇˇˇÇyG                                               FILE0  éar      8  Ë                Ωé           `           H      Î˙[©´EÃ 'Â6zb Î˙[©´EÃ¥+£„tLÃ                    
          Pã:    0   x          Z     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ 0                      L S T R A N ~ 1 . S V N h . s 0   à          n     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ 0                      l s t r a n s p o r t . h . s v n - b a s e   Ä   H                        @        0      ¬(      ¬(      AuπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ™ar      8  ‡                æé           `           H      Î˙[©´EÃ 'Â6zb Î˙[©´EÃÃ€£„tLÃ                    
          8ç:    0   x          Z     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ                        L S U T I L ~ 1 . S V N n - b 0   Ä          f     ≤é    Î˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃÎ˙[©´EÃ                        l s u t i l s . h . s v n - b a s e   Ä   H                        @               ü      ü      AxπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ∆ar      8  Ë                øé           `           H      ãÅ]©´EÃ 'Â6zb ãÅ]©´EÃ2õ„tLÃ                    
          Xè:    0   x          Z     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       N D F S I N ~ 1 . S V N . h . 0   à          p     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       N d f s I n t e f a c e . h . s v n - b a s e Ä   H                         @              ¿      ¿      AzπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ‚ar      8  ¯                ¿é           `           H      ãÅ]©´EÃ 'Â6zb ãÅ]©´EÃ‘9ù„tLÃ                    
          ‡ë:    0   x          Z     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ @                      N D F S P R ~ 1 . S V N H e a 0   ò          ~     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ @                      N d f s P r o t o c o l H e a d e r 2 . h . s v n - b a s e   Ä   H                        @        @      í<      í<      A{πÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ◊cr      8  Ë                ¡é           `           H      ãÅ]©´EÃ 'Â6zb ãÅ]©´EÃV§„tLÃ                    
          ¯ì:    0   x          Z     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       T C P T D I ~ 1 . S V N . s v 0   à          l     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       t c p t d i p r o c . h . s v n - b a s e     Ä   H                         @              *      *      AπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  Ber      8  Ë                ¬é           `           H      ãÅ]©´EÃ 'Â6zb ãÅ]©´EÃÇM§„tLÃ                    
           ñ:    0   x          Z     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       W I N S C S ~ 1 . S V N . s v 0   à          l     ≤é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       w i n s c s i e x t . h . s v n - b a s e     Ä   H                         @              ≤      ≤      AÄπÖ   ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ¿∏Pq      8  P                √é  ~        `           H      ãÅ]©´EÃ+_©´EÃ+_©´EÃ$`A¢ÇLÃ                    	          ¯ñ:    0   `          H     ≠é    ãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃãÅ]©´EÃ                       t m p ê   P       0      $ I 3 0 0                         ƒé    h T     √é    +_©´EÃ ∆@‚êx +_©´EÃ¬≠A¢ÇLÃ                       	p r o p - b a s e     ƒé    h R     √é    +_©´EÃ ∆@‚êx +_©´EÃ¬≠A¢ÇLÃ                       P R O P - B  1       ≈é    ` L     √é    +_©´EÃ ∆@‚êx +_©´EÃ0¬A¢ÇLÃ                       p r o p s     ∆é    h T     √é    +_©´EÃ ∆@‚êx +_©´EÃ⁄÷A¢ÇLÃ                       	t e x t - b a s e     ∆é    h R     √é    +_©´EÃ ∆@‚êx +_©´EÃ⁄÷A¢ÇLÃ                       T E X T - B ~ 1                     ˇˇˇˇÇyG                                                                                                                                                                               FILE0  &∂Pq      8  –                ƒé           `           H      +_©´EÃ ∆@‚êx +_©´EÃ¬≠A¢ÇLÃ                    	          ‡ó:    0   p          R     √é    +_©´EÃ+_©´EÃ+_©´EÃ+_©´EÃ                       P R O P - B ~ 1 e     0   p          T     √é    +_©´EÃ+_©´EÃ+_©´EÃ+_©´EÃ                       	p r o p - b a s e     ê   P        0       $ I 3 0 0                                         ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  ]∏Pq      8  X                ≈é           `           H      +_©´EÃ ∆@‚êx +_©´EÃ0¬A¢ÇLÃ                    	          ¿ò:    0   h          L     √é    +_©´EÃ+_©´EÃ+_©´EÃ+_©´EÃ                       p r o p s     ê   P        0       $ I 3 0 0                                         ˇˇˇˇÇyG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  †∫Pq      8  –                ∆é           `           H      +_©´EÃ ∆@‚êx +_©´EÃ⁄÷A¢ÇLÃ                    	          ®ô:    0   p          R     √é    +_©´EÃ+_©´EÃ+_©´EÃ+_©´EÃ                       T E X T - B ~ 1 e     0   p          T     √é    +_©´EÃ+_©´EÃ+_©´EÃ+_©´EÃ                       	t e x t - b a s e     ê   P        0       $ I 3 0 0                                         ˇˇˇˇÇyG                                                                                                                          