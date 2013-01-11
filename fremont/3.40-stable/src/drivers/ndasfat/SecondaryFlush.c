#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#define BugCheckFileId                   (FAT_BUG_CHECK_FLUSH)


NTSTATUS
FatFlushCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );


NTSTATUS
NdasFatCommonFlushBuffers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for flushing a buffer.

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
    PCCB Ccb;

    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN FcbAcquired = FALSE;

    PDIRENT Dirent;
    PBCB DirentBcb = NULL;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;


    PAGED_CODE();

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatCommonFlushBuffers\n", 0);
    DebugTrace( 0, Dbg, "Irp           = %08lx\n", Irp);
    DebugTrace( 0, Dbg, "->FileObject  = %08lx\n", IrpSp->FileObject);

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = FatDecodeFileObject( FileObject, &Vcb, &Fcb, &Ccb );

    //
    //  CcFlushCache is always synchronous, so if we can't wait enqueue
    //  the irp to the Fsp.
    //

    if ( !FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ) {

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatCommonFlushBuffers -> %08lx\n", Status );
        return Status;
    }

    Status = STATUS_SUCCESS;

    try {

		if (!FlagOn(Ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

			do {
			
				secondarySessionResourceAcquired 
					= SecondaryAcquireResourceExclusiveLite( IrpContext, 
															 &volDo->SessionResource, 
															 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

				if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

					PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
					NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
				}

				secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, IRP_MJ_FLUSH_BUFFERS, 0 );

				if (secondaryRequest == NULL) {
	
					NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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

				timeOut.QuadPart = -NDASFAT_TIME_OUT;		
				Status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
			
				if (Status != STATUS_SUCCESS) {

					ASSERT( NDASFAT_BUG );
					break;
				}

				KeClearEvent (&secondaryRequest->CompleteEvent);

				if (BooleanFlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {

					NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}

				if (secondaryRequest->ExecuteStatus == STATUS_SUCCESS) {

					ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
					ASSERT(NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS);
				}

				if (secondaryRequest) {

					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;
				}

				if ( secondarySessionResourceAcquired == TRUE ) {
					
					SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
					secondarySessionResourceAcquired = FALSE;
				}

				break;

			} while(0);
		} 

		Status = STATUS_SUCCESS;

        //
        //  Case on the type of open that we are trying to flush
        //

        switch (TypeOfOpen) {

        case VirtualVolumeFile:
        case EaFile:
        case DirectoryFile:

            DebugTrace(0, Dbg, "Flush that does nothing\n", 0);
            break;

        case UserFileOpen:

            DebugTrace(0, Dbg, "Flush User File Open\n", 0);

            (VOID)FatAcquireExclusiveFcb( IrpContext, Fcb );

            FcbAcquired = TRUE;

            FatVerifyFcb( IrpContext, Fcb );

            //
            //  If the file is cached then flush its cache
            //

            Status = FatFlushFile( IrpContext, Fcb, Flush );

            //
            //  Also update and flush the file's dirent in the parent directory if the
            //  file flush worked.
            //

            if (NT_SUCCESS( Status )) {

                //
                //  Insure that we get the filesize to disk correctly.  This is
                //  benign if it was already good.
                //
                //  (why do we need to do this?)
                //

                SetFlag(FileObject->Flags, FO_FILE_SIZE_CHANGED);

#if 0
                FatUpdateDirentFromFcb( IrpContext, FileObject, Fcb, Ccb );
#endif                
                
                //
                //  Flush the volume file to get any allocation information
                //  updates to disk.
                //

                if (FlagOn(Fcb->FcbState, FCB_STATE_FLUSH_FAT)) {

                    Status = FatFlushFat( IrpContext, Vcb );

                    ClearFlag(Fcb->FcbState, FCB_STATE_FLUSH_FAT);
                }

                //
                //  Set the write through bit so that these modifications
                //  will be completed with the request.
                //

                SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
            }

            break;

        case UserDirectoryOpen:

            //
            //  If the user had opened the root directory then we'll
            //  oblige by flushing the volume.
            //

            if (NodeType(Fcb) != FAT_NTC_ROOT_DCB) {

                DebugTrace(0, Dbg, "Flush a directory does nothing\n", 0);
                break;
            }

        case UserVolumeOpen:

            DebugTrace(0, Dbg, "Flush User Volume Open, or root dcb\n", 0);

            //
            //  Acquire exclusive access to the Vcb.
            //

            {
                BOOLEAN Finished;
                Finished = FatAcquireExclusiveVcb( IrpContext, Vcb );
                ASSERT( Finished );
            }

            VcbAcquired = TRUE;

            //
            //  Mark the volume clean and then flush the volume file,
            //  and then all directories
            //

            Status = FatFlushVolume( IrpContext, Vcb, Flush );

            //
            //  If the volume was dirty, do the processing that the delayed
            //  callback would have done.
            //

            if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY)) {

                //
                //  Cancel any pending clean volumes.
                //

                (VOID)KeCancelTimer( &Vcb->CleanVolumeTimer );
                (VOID)KeRemoveQueueDpc( &Vcb->CleanVolumeDpc );

                //
                //  The volume is now clean, note it.
                //

                if (!FlagOn(Vcb->VcbState, VCB_STATE_FLAG_MOUNTED_DIRTY)) {

                    FatMarkVolume( IrpContext, Vcb, VolumeClean );
                    ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY );
                }

                //
                //  Unlock the volume if it is removable.
                //

                if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA) &&
                    !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_BOOT_OR_PAGING_FILE)) {

                    FatToggleMediaEjectDisable( IrpContext, Vcb, FALSE );
                }
            }

            break;

        default:

            FatBugCheck( TypeOfOpen, 0, 0 );
        }

        FatUnpinBcb( IrpContext, DirentBcb );

        FatUnpinRepinnedBcbs( IrpContext );

    } finally {

        DebugUnwind( FatCommonFlushBuffers );

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if (secondarySessionResourceAcquired) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

        FatUnpinBcb( IrpContext, DirentBcb );

        if (VcbAcquired) { FatReleaseVcb( IrpContext, Vcb ); }

        if (FcbAcquired) { FatReleaseFcb( IrpContext, Fcb ); }

        //
        //  If this is a normal termination then pass the request on
        //  to the target device object.
        //

        if (!AbnormalTermination()) {

            NTSTATUS DriverStatus;
            PIO_STACK_LOCATION NextIrpSp;

            //
            //  Get the next stack location, and copy over the stack location
            //

            NextIrpSp = IoGetNextIrpStackLocation( Irp );

            *NextIrpSp = *IrpSp;

            //
            //  Set up the completion routine
            //

            IoSetCompletionRoutine( Irp,
                                    FatFlushCompletionRoutine,
                                    ULongToPtr( Status ),
                                    TRUE,
                                    TRUE,
                                    TRUE );

            //
            //  Send the request.
            //

            DriverStatus = IoCallDriver(Vcb->TargetDeviceObject, Irp);

            Status = (DriverStatus == STATUS_INVALID_DEVICE_REQUEST) ?
                     Status : DriverStatus;

            //
            //  Free the IrpContext and return to the caller.
            //

            FatCompleteRequest( IrpContext, FatNull, STATUS_SUCCESS );
        }

        DebugTrace(-1, Dbg, "FatCommonFlushBuffers -> %08lx\n", Status);
    }

    return Status;
}


#endif
