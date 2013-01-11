#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__

#define BugCheckFileId                   (NTFS_BUG_CHECK_FLUSH)

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('XftN')


static NTSTATUS
NdasNtfsFlushUserStream (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PLONGLONG FileOffset OPTIONAL,
    IN ULONG Length
    );

static NTSTATUS
NdasNtfsFlushCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );

#define FlushScb(IRPC,SCB,IOS) {                                                \
    (IOS)->Status = NdasNtfsFlushUserStream((IRPC),(SCB),NULL,0);                 \
}


static NTSTATUS
NdasNtfsFlushUserStream (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PLONGLONG FileOffset OPTIONAL,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine flushes a user stream as a top-level action.  To do so
    it checkpoints the current transaction first and frees all of the
    caller's snapshots.  After doing the flush, it snapshots the input
    Scb again, just in case the caller plans to do any more work on that
    stream.  If the caller needs to modify any other streams (presumably
    metadata), it must know to snapshot them itself after calling this
    routine.

Arguments:

    Scb - Stream to flush

    FileOffset - FileOffset at which the flush is to start, or NULL for
                 entire stream.

    Length - Number of bytes to flush.  Ignored if FileOffset not specified.

Return Value:

    Status of the flush

--*/

{
    IO_STATUS_BLOCK IoStatus;
    BOOLEAN ScbAcquired = FALSE;

    PAGED_CODE();

    //
    //  Checkpoint the current transaction and free all of its snapshots,
    //  in order to treat the flush as a top-level action with his own
    //  snapshots, etc.
    //
#if 0
    NtfsCheckpointCurrentTransaction( IrpContext );
    NtfsFreeSnapshotsForFcb( IrpContext, NULL );
#endif

    //
    //  Set the wait flag in the IrpContext so we don't hit a case where the
    //  reacquire below fails because we can't wait.  If our caller was asynchronous
    //  and we get this far we will continue synchronously.
    //

    SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );

    //
    //  We must free the Scb now before calling through MM to prevent
    //  collided page deadlocks.
    //

    //
    //  We are about to flush the stream.  The Scb may be acquired exclusive
    //  and, thus, is linked onto the IrpContext or onto one higher
    //  up in the IoCallDriver stack.  We are about to make a
    //  call back into Ntfs which may acquire the Scb exclusive, but
    //  NOT put it onto the nested IrpContext exclusive queue which prevents
    //  the nested completion from freeing the Scb.
    //
    //  This is only a problem for Scb's without a paging resource.
    //
    //  We acquire the Scb via ExAcquireResourceExclusiveLite, sidestepping
    //  Ntfs bookkeeping, and release it via NtfsReleaseScb.
    //

	ScbAcquired = NtfsIsExclusiveScb( Scb );

    if (ScbAcquired) {
        if (Scb->Header.PagingIoResource == NULL) {
            NtfsAcquireResourceExclusive( IrpContext, Scb, TRUE );
        }
        NtfsReleaseScb( IrpContext, Scb );
    }

#ifdef  COMPRESS_ON_WIRE
    if (Scb->Header.FileObjectC != NULL) {

        PCOMPRESSION_SYNC CompressionSync = NULL;

        //
        //  Use a try-finally to clean up the compression sync.
        //

        try {

            NtfsSynchronizeUncompressedIo( Scb,
                                           NULL,
                                           0,
                                           TRUE,
                                           &CompressionSync );

        } finally {

            NtfsReleaseCompressionSync( CompressionSync );
        }
    }
#endif

    //
    //  Clear the file record cache before doing the flush.  Otherwise FlushVolume may hold this
    //  file and be purging the Mft at the same time this thread has a Vacb in the Mft and is
    //  trying to reacquire the file in the recursive IO thread.
    //

    NtfsPurgeFileRecordCache( IrpContext );

    //
    //  Now do the flush he wanted as a top-level action
    //

    CcFlushCache( &Scb->NonpagedScb->SegmentObject, (PLARGE_INTEGER)FileOffset, Length, &IoStatus );

    //
    //  Now reacquire for the caller.
    //

    if (ScbAcquired) {
        NtfsAcquireExclusiveScb( IrpContext, Scb );
        if (Scb->Header.PagingIoResource == NULL) {
            NtfsReleaseResource( IrpContext, Scb );
        }
    }

    return IoStatus.Status;
}


static NTSTATUS
NdasNtfsFlushCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Contxt );

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

        Irp->IoStatus.Status = STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
NdasNtfsCommonFlushBuffers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for flush buffers called by both the fsd and fsp
    threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PLCB Lcb = NULL;
    PSCB ParentScb = NULL;

    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN ScbAcquired = FALSE;
    BOOLEAN ParentScbAcquired = FALSE;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;


    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );
    ASSERT( FlagOn( IrpContext->TopLevelIrpContext->State, IRP_CONTEXT_STATE_OWNS_TOP_LEVEL ));

    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonFlushBuffers\n") );
    DebugTrace( 0, Dbg, ("Irp           = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("->FileObject  = %08lx\n", IrpSp->FileObject) );

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  abort immediately for non files
    //

    if (UnopenedFileObject == TypeOfOpen) {
        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Nuthin-doing if the volume is mounted read only.
    //
#if __NDAS_NTFS_SECONDARY__
	if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) // avoid NtfsIsVolumeReadOnly;
#endif
    if (NtfsIsVolumeReadOnly( Vcb )) {

        Status = STATUS_MEDIA_WRITE_PROTECTED;
        NtfsCompleteRequest( IrpContext, Irp, Status );

        DebugTrace( -1, Dbg, ("NtfsCommonFlushBuffers -> %08lx\n", Status) );
        return Status;
    }

	if (!FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

		return NtfsPostRequest( IrpContext, Irp );
	}

	if(volDo->Secondary == NULL) {

		Status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
		Irp->IoStatus.Information = 0;
		NtfsCompleteRequest( IrpContext, Irp, Status );
		return Status;
	}

	Status = STATUS_SUCCESS;

    try {

		if (!FlagOn(Ccb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

			do {
			
				secondarySessionResourceAcquired 
					= SecondaryAcquireResourceExclusiveLite( IrpContext, 
															 &volDo->SessionResource, 
															 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

				if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

					PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
					NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
				}

				secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, IRP_MJ_FLUSH_BUFFERS, 0 );

				if(secondaryRequest == NULL) {
	
					Status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

				INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_FLUSH_BUFFERS, 0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, 
													  IrpContext->OriginatingIrp, 
													  IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp), 
													  Ccb->PrimaryFileHandle );
				
				ASSERT( !ExIsResourceAcquiredSharedLite(&IrpContext->Vcb->Resource) );	

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASNTFS_TIME_OUT;
				Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
			
				if(Status != STATUS_SUCCESS) {

					ASSERT( NDASNTFS_BUG );
					break;
				}

				KeClearEvent (&secondaryRequest->CompleteEvent);

				if( secondarySessionResourceAcquired == TRUE ) {
					
					SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
					secondarySessionResourceAcquired = FALSE;
				}


				if (BooleanFlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {

					NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
				}

				if(secondaryRequest->ExecuteStatus == STATUS_SUCCESS) {

					ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
					ASSERT(ndfsWinxpReplytHeader->Status == STATUS_SUCCESS);
					break;
				}

			}while(0);
		} 

		Status = STATUS_SUCCESS;

        //
        //  Case on the type of open that we are trying to flush
        //

        switch (TypeOfOpen) {

        case UserFileOpen:

            DebugTrace( 0, Dbg, ("Flush User File Open\n") );

            //
            //  Acquire the Vcb so we can update the duplicate information as well.
            //

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
            VcbAcquired = TRUE;

            //
            //  While we have the Vcb, let's make sure it's still mounted.
            //

            if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                try_return( Status = STATUS_VOLUME_DISMOUNTED );
            }

            //
            //  Make sure the data gets out to disk.
            //

            NtfsAcquireExclusivePagingIo( IrpContext, Fcb );

            //
            //  Acquire exclusive access to the Scb and enqueue the irp
            //  if we didn't get access
            //

            NtfsAcquireExclusiveScb( IrpContext, Scb );
            ScbAcquired = TRUE;

            //
            //  Flush the stream and verify there were no errors.
            //

            FlushScb( IrpContext, Scb, &Irp->IoStatus );

            //
            //  Now commit what we've done so far.
            //
#if 0
            NtfsCheckpointCurrentTransaction( IrpContext );

            //
            //  Update the time stamps and file sizes in the Fcb based on
            //  the state of the File Object.
            //

            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );

            //
            //  If we are to update standard information then do so now.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                NtfsUpdateStandardInformation( IrpContext, Fcb );
            }

            //
            //  If this is the system hive there is more work to do.  We want to flush
            //  all of the file records for this file as well as for the parent index
            //  stream.  We also want to flush the parent index stream.  Acquire the
            //  parent stream exclusively now so that the update duplicate call won't
            //  acquire it shared first.
            //

            if (FlagOn( Ccb->Flags, CCB_FLAG_SYSTEM_HIVE )) {

                //
                //  Start by acquiring all of the necessary files to avoid deadlocks.
                //

                if (Ccb->Lcb != NULL) {

                    ParentScb = Ccb->Lcb->Scb;

                    if (ParentScb != NULL) {

                        NtfsAcquireExclusiveScb( IrpContext, ParentScb );
                        ParentScbAcquired = TRUE;
                    }
                }
            }

            //
            //  Update the duplicate information if there are updates to apply.
            //

            if (FlagOn( Fcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS )) {

                Lcb = Ccb->Lcb;

                NtfsPrepareForUpdateDuplicate( IrpContext, Fcb, &Lcb, &ParentScb, TRUE );
                NtfsUpdateDuplicateInfo( IrpContext, Fcb, Lcb, ParentScb );
                NtfsUpdateLcbDuplicateInfo( Fcb, Lcb );

                if (ParentScbAcquired) {

                    NtfsReleaseScb( IrpContext, ParentScb );
                    ParentScbAcquired = FALSE;
                }
            }

            //
            //  Now flush the file records for this stream.
            //

            if (FlagOn( Ccb->Flags, CCB_FLAG_SYSTEM_HIVE )) {

                //
                //  Flush the file records for this file.
                //

                Status = NtfsFlushFcbFileRecords( IrpContext, Scb->Fcb );

                //
                //  Now flush the parent index stream.
                //

                if (NT_SUCCESS(Status) && (ParentScb != NULL)) {

                    CcFlushCache( &ParentScb->NonpagedScb->SegmentObject, NULL, 0, &Irp->IoStatus );
                    Status = Irp->IoStatus.Status;

                    //
                    //  Finish by flushing the file records for the parent out
                    //  to disk.
                    //

                    if (NT_SUCCESS( Status )) {

                        Status = NtfsFlushFcbFileRecords( IrpContext, ParentScb->Fcb );
                    }
                }
            }

            //
            //  If our status is still success then flush the log file and
            //  report any changes.
            //

            if (NT_SUCCESS( Status )) {

                ULONG FilterMatch;

                LfsFlushToLsn( Vcb->LogHandle, LiMax );

                //
                //  We only want to do this DirNotify if we updated duplicate
                //  info and set the ParentScb.
                //

                if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID ) &&
                    (Vcb->NotifyCount != 0) &&
                    FlagOn( Fcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS )) {

                    FilterMatch = NtfsBuildDirNotifyFilter( IrpContext, Fcb->InfoFlags );

                    if (FilterMatch != 0) {

                        NtfsReportDirNotify( IrpContext,
                                             Fcb->Vcb,
                                             &Ccb->FullFileName,
                                             Ccb->LastFileNameOffset,
                                             NULL,
                                             ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                               (Ccb->Lcb != NULL) &&
                                               (Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Length != 0)) ?
                                              &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                              NULL),
                                             FilterMatch,
                                             FILE_ACTION_MODIFIED,
                                             ParentScb->Fcb );
                    }
                }

                ClearFlag( Fcb->InfoFlags,
                           FCB_INFO_NOTIFY_FLAGS | FCB_INFO_DUPLICATE_FLAGS );
            }
#endif
            break;

        case UserViewIndexOpen:
        case UserDirectoryOpen:

#if 0
            //
            //  If the user had opened the root directory then we'll
            //  oblige by flushing the volume.
            //

            if (NodeType(Scb) != NTFS_NTC_SCB_ROOT_INDEX) {

                DebugTrace( 0, Dbg, ("Flush a directory does nothing\n") );
                break;
            }

        case UserVolumeOpen:

            DebugTrace( 0, Dbg, ("Flush User Volume Open\n") );

            NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
            VcbAcquired = TRUE;

            //
            //  While we have the Vcb, let's make sure it's still mounted.
            //

            if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                try_return( Status = STATUS_VOLUME_DISMOUNTED );
            }

            NtfsFlushVolume( IrpContext,
                             Vcb,
                             TRUE,
                             FALSE,
                             TRUE,
                             FALSE );

            //
            //  Make sure all of the data written in the flush gets to disk.
            //

            LfsFlushToLsn( Vcb->LogHandle, LiMax );
#endif
			break;

        case StreamFileOpen:

            //
            //  Nothing to do here.
            //

            break;

        default:

            //
            //  Nothing to do if we have our driver object.
            //

            break;
        }

        //
        //  Abort transaction on error by raising.
        //
#if 0
        NtfsCleanupTransaction( IrpContext, Status, FALSE );
#endif


    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsCommonFlushBuffers );

		if( secondarySessionResourceAcquired == TRUE ) {
					
			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		//
		//  Release any resources which were acquired.
        //

        if (ScbAcquired) {
            NtfsReleaseScb( IrpContext, Scb );
        }

        if (ParentScbAcquired) {
            NtfsReleaseScb( IrpContext, ParentScb );
        }

        if (VcbAcquired) {
            NtfsReleaseVcb( IrpContext, Vcb );
        }

        //
        //  If this is a normal termination then pass the request on
        //  to the target device object.
        //

        if (!AbnormalTermination()) {

            NTSTATUS DriverStatus;
            PIO_STACK_LOCATION NextIrpSp;

            //
            //  Free the IrpContext now before calling the lower driver.  Do this
            //  now in case this fails so that we won't complete the Irp in our
            //  exception routine after passing it to the lower driver.
            //

            NtfsCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

            ASSERT( Vcb != NULL );

            //
            //  Get the next stack location, and copy over the stack location
            //


            NextIrpSp = IoGetNextIrpStackLocation( Irp );

            *NextIrpSp = *IrpSp;


            //
            //  Set up the completion routine
            //

            IoSetCompletionRoutine( Irp,
                                    NdasNtfsFlushCompletionRoutine,
                                    NULL,
                                    TRUE,
                                    TRUE,
                                    TRUE );

            //
            //  Send the request.
            //

            DriverStatus = IoCallDriver(Vcb->TargetDeviceObject, Irp);

            Status = (DriverStatus == STATUS_INVALID_DEVICE_REQUEST) ?
                     Status : DriverStatus;

        }

        DebugTrace( -1, Dbg, ("NtfsCommonFlushBuffers -> %08lx\n", Status) );
    }

    return Status;
}

#endif