#include "FatProcs.h"

#ifdef __ND_FAT_SECONDARY__

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)


NTSTATUS
NdFatSecondaryCommonRead (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp,
	IN ULONG		BytesToRead
	)
{
	NTSTATUS					status;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	struct Read					read;
	
	PSECONDARY_REQUEST			secondaryRequest = NULL;
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PCCB						ccb;
	BOOLEAN						fcbAcquired = FALSE;

	PUCHAR						outputBuffer;
	ULONG						totalReadLength;

	_U64						primaryFileHandle = 0;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			
			return STATUS_FILE_CORRUPT_ERROR;
		}
	}

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

		ASSERT( FALSE );
        DebugTrace2( 0, Dbg, ("Can't wait in create\n") );

        status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace2( -1, Dbg2, ("NdFatSecondaryCommonRead:  FatFsdPostRequest -> %08lx\n", status) );
        return status;
    }

	if (irpSp->Parameters.Read.ByteOffset.QuadPart == FILE_WRITE_TO_END_OF_FILE && 
		irpSp->Parameters.Read.ByteOffset.HighPart == -1) {

		read.ByteOffset = fcb->Header.FileSize;

	} else {

		read.ByteOffset = irpSp->Parameters.Read.ByteOffset;
	}

	read.Key	= 0;
	read.Length	= irpSp->Parameters.Read.Length;
	read.Length = BytesToRead;


	ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ); 

	//FatAcquireSharedFcb( IrpContext, fcb );
	//fcbAcquired = TRUE;


	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->Secondary->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		outputBuffer = FatMapUserBuffer( IrpContext, Irp );
		totalReadLength = 0;

		do {

			ULONG						outputBufferLength;

			if (fcb->UncleanCount == 0) {

				DebugTrace( 0, Dbg2, "NdFatSecondaryCommonRead: fileName = %wZ\n", &fileObject->FileName );

				status = STATUS_FILE_CLOSED;
				break;
			}

			if (!FlagOn(ccb->NdFatFlags, ND_FAT_CLEANUP_COMPLETE)) {

				primaryFileHandle = ccb->PrimaryFileHandle;

			} else {

				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &fcb->CcbQMutex );
				
				for (primaryFileHandle = 0, ccbListEntry = fcb->CcbQueue.Flink; 
					 ccbListEntry != &fcb->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink) {

					if (!FlagOn(CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->NdFatFlags, ND_FAT_CLEANUP_COMPLETE)) {
						
						primaryFileHandle = CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->PrimaryFileHandle;
						break;
					}
				}

				ExReleaseFastMutex( &fcb->CcbQMutex );
			}

			ASSERT( primaryFileHandle );

			outputBufferLength = ((read.Length-totalReadLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (read.Length-totalReadLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
															  IRP_MJ_READ,
															  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag   = (_U32)Irp;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

			ndfsWinxpRequestHeader->IrpFlags   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
			ndfsWinxpRequestHeader->Read.Key		= read.Key;
			ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			if (status != STATUS_SUCCESS) {

				secondaryRequest = NULL;
				status = STATUS_IO_DEVICE_ERROR;
				leave;
			}

			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			
				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			if (ndfsWinxpReplytHeader->Status == STATUS_END_OF_FILE) {
	
				ASSERT( ndfsWinxpReplytHeader->Information == 0 );

				if (!(read.ByteOffset.QuadPart & (((ULONG)vcb->Bpb.BytesPerSector) - 1))) {

		
					RtlZeroMemory( outputBuffer + totalReadLength,
								   read.Length - totalReadLength );

					totalReadLength = read.Length;
				
				} else {

					ASSERT( FALSE );
				}

				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

				ASSERT( totalReadLength == 0 );
				ASSERT( ndfsWinxpReplytHeader->Status == STATUS_FILE_CLOSED );
							
				DebugTrace2( 0, Dbg, ("ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

				if (totalReadLength)
					status = STATUS_SUCCESS;
				else
					status = ndfsWinxpReplytHeader->Status;

				ASSERT( ndfsWinxpReplytHeader->Information == 0 );
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			ASSERT( ndfsWinxpReplytHeader->Information <= outputBufferLength );
			ASSERT( outputBufferLength == 0 || outputBuffer );

			//if (fcb->Header.FileSize.QuadPart < 100)
			//	DbgPrint( "data = %s\n", (_U8 *)(ndfsWinxpReplytHeader+1) );

			if (ndfsWinxpReplytHeader->Information && outputBuffer) {

				try {

					RtlCopyMemory( outputBuffer + totalReadLength,
								   (_U8 *)(ndfsWinxpReplytHeader+1),
								   ndfsWinxpReplytHeader->Information );

				} finally {

					if (AbnormalTermination()) {

						DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
						totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
						status = STATUS_SUCCESS;
					
					} else {
					
						if (ndfsWinxpReplytHeader->Status == STATUS_SUCCESS)
							totalReadLength += ndfsWinxpReplytHeader->Information;

						if (totalReadLength)
							status = STATUS_SUCCESS;
						else
							status = ndfsWinxpReplytHeader->Status;
					}
				}
			}

			//if (fcb->Header.FileSize.QuadPart < 100)
			//	DbgPrint( "data = %s\n", outputBuffer );
		
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

		} while( totalReadLength < read.Length );


		if (status == STATUS_FILE_CLOSED) {

			_U64	fcbHandle;
			ULONG	dataSize;
			_U8		*ndfsWinxpRequestData;


			ASSERT( ccb );
			ASSERT( totalReadLength == 0 );
			ASSERT( secondaryRequest == NULL );

			if (ccb->CreateContext.RelatedFileHandle != 0) {

				ASSERT( FALSE );
				try_return( status = STATUS_FILE_CLOSED );
			}
				
			DebugTrace2( 0, Dbg, ("SessionRecovery: ccb->Lcb->ExactCaseLink.LinkName = %wZ \n", &ccb->Fcb->FullFileName) );

			dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary,
															  IRP_MJ_CREATE,
															  (dataSize >= DEFAULT_MAX_DATA_SIZE) ? dataSize : DEFAULT_MAX_DATA_SIZE );

			if (secondaryRequest == NULL) {

				ASSERT( FALSE );
				try_return( status = STATUS_FILE_CLOSED );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

			INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
											NDFS_COMMAND_EXECUTE,
											volDo->Secondary,
											IRP_MJ_CREATE,
											(ccb->BufferLength + ccb->Fcb->FullFileName.Length) );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle = 0;

			ndfsWinxpRequestHeader->IrpFlags   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0; //irpSp->Flags;

			ndfsWinxpRequestHeader->Create.AllocationSize = 0;
			ndfsWinxpRequestHeader->Create.EaLength = 0;
			ndfsWinxpRequestHeader->Create.FileAttributes = 0;

			ndfsWinxpRequestHeader->Create.Options = 0; //irpSp->Parameters.Create.Options & ~FILE_DELETE_ON_CLOSE;
			ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
			ndfsWinxpRequestHeader->Create.Options |= (FILE_OPEN << 24);

			ndfsWinxpRequestHeader->Create.FileNameLength 
				= (USHORT)(ccb->Fcb->FullFileName.Length + (ccb->BufferLength - ccb->CreateContext.EaLength));

			ndfsWinxpRequestHeader->Create.FileNameLength = ccb->CreateContext.FileNameLength;
			ndfsWinxpRequestHeader->Create.EaLength = 0; //ccb->CreateContext.EaLength;

			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength,
						   ccb->Fcb->FullFileName.Buffer,
						   ccb->Fcb->FullFileName.Length );

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength + ccb->Fcb->FullFileName.Length,
						   ccb->Buffer + ccb->CreateContext.EaLength,
						   ccb->BufferLength - ccb->CreateContext.EaLength );

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );
				
			timeOut.QuadPart = -NDFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

			KeClearEvent(&secondaryRequest->CompleteEvent);

			if (status != STATUS_SUCCESS) {
		
				ASSERT( NDFAT_BUG );

				secondaryRequest = NULL;
				ASSERT( FALSE );
				try_return( status );
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				status = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				ASSERT( FALSE );

				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
			DebugTrace2( 0, Dbg, ("SessionRecovery: ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

			if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

				ASSERT( FALSE );
				status = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				try_return( status = STATUS_FILE_CLOSED );
			}

			primaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
			ASSERT( fcb->Handle == ndfsWinxpReplytHeader->Open.FcbHandle );

			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

			do {

				ULONG						outputBufferLength;


				outputBufferLength = ((read.Length-totalReadLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (read.Length-totalReadLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

				secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
																  IRP_MJ_READ,
																  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag   = (_U32)Irp;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle = primaryFileHandle;

				ndfsWinxpRequestHeader->IrpFlags   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
				ndfsWinxpRequestHeader->Read.Key		= read.Key;
				ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

				timeOut.QuadPart = -NDFAT_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {

					secondaryRequest = NULL;
					status = STATUS_IO_DEVICE_ERROR;
					leave;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					if (IrpContext->OriginatingIrp)
						PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			
					DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				if (ndfsWinxpReplytHeader->Status == STATUS_END_OF_FILE) {
	
					ASSERT( ndfsWinxpReplytHeader->Information == 0 );

					if (!(read.ByteOffset.QuadPart & (((ULONG)vcb->Bpb.BytesPerSector) - 1))) {

		
						RtlZeroMemory( outputBuffer + totalReadLength,
									   read.Length - totalReadLength );

						totalReadLength = read.Length;
				
					} else {

						ASSERT( FALSE );
					}

					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;				
				
					break;
				}

				if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

					ASSERT( FALSE );
			
					DebugTrace2( 0, Dbg2, ("ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

					if (totalReadLength)
						status = STATUS_SUCCESS;
					else
						status = ndfsWinxpReplytHeader->Status;

					ASSERT( ndfsWinxpReplytHeader->Information == 0 );
					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;				
				
					break;
				}

				ASSERT( ndfsWinxpReplytHeader->Information <= outputBufferLength );
				ASSERT( outputBufferLength == 0 || outputBuffer );

				if (ndfsWinxpReplytHeader->Information && outputBuffer) {

					try {

						RtlCopyMemory( outputBuffer + totalReadLength,
									   (_U8 *)(ndfsWinxpReplytHeader+1),
									   ndfsWinxpReplytHeader->Information );

					} finally {

						if (AbnormalTermination()) {

							DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
							totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
							status = STATUS_SUCCESS;
					
						} else {
					
							if (ndfsWinxpReplytHeader->Status == STATUS_SUCCESS)
								totalReadLength += ndfsWinxpReplytHeader->Information;

							if (totalReadLength)
								status = STATUS_SUCCESS;
							else
								status = ndfsWinxpReplytHeader->Status;
						}
					}
				}
		
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;

			} while( totalReadLength < read.Length );
			
			ASSERT( totalReadLength == read.Length );

			ClosePrimaryFile( volDo->Secondary, primaryFileHandle );
		}

try_exit:

		NOTHING;

	} finally {

		if (totalReadLength) {

			Irp->IoStatus.Information = totalReadLength;
			Irp->IoStatus.Status = STATUS_SUCCESS;
		
		} else {
		
			Irp->IoStatus.Information = 0;
			Irp->IoStatus.Status = status;
		}

		if (Irp->IoStatus.Status != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg2, ("read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
								 read.ByteOffset.QuadPart, read.Length, totalReadLength, status) );

			PrintIrp( Dbg2, "RedirectIrpMajorRead", NULL, Irp );
		}

		if (secondarySessionResourceAcquired == TRUE)
			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );

		if (fcbAcquired) {
             FatReleaseFcb( IrpContext, fcb );
        }

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
			
	return status;
}


NTSTATUS
NdFatSecondaryCommonRead3 (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp
	)
{
	NTSTATUS					status;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	struct Read					read;
	
	PSECONDARY_REQUEST			secondaryRequest = NULL;
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	LARGE_INTEGER				timeOut;

	TYPE_OF_OPEN				typeOfOpen;
	PVCB						vcb;
	PFCB						fcb;
	PCCB						ccb;
	BOOLEAN						fcbAcquired = FALSE;

	PUCHAR						outputBuffer;
	ULONG						totalReadLength;

	_U64						primaryFileHandle = 0;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			
			status = STATUS_FILE_CORRUPT_ERROR;
	        FatCompleteRequest( IrpContext, Irp, status );
			return status;
		}
	}

	if (irpSp->Parameters.Read.ByteOffset.HighPart != 0) {

		status = STATUS_INVALID_PARAMETER;
        FatCompleteRequest( IrpContext, Irp, status );
		return status;

	} else {

		read.ByteOffset = irpSp->Parameters.Read.ByteOffset;
	}

	read.Key	= 0;
	read.Length	= irpSp->Parameters.Read.Length;


	ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) ); 

	//FatAcquireSharedFcb( IrpContext, fcb );
	//fcbAcquired = TRUE;

	try {

		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->Secondary->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		outputBuffer = FatMapUserBuffer( IrpContext, Irp );
		totalReadLength = 0;

		do {

			ULONG						outputBufferLength;

			if (fcb->UncleanCount == 0) {

				DebugTrace( 0, Dbg2, "NdFatSecondaryCommonRead: fileName = %wZ\n", &fileObject->FileName );

				status = STATUS_FILE_CLOSED;
				break;
			}

			if (!FlagOn(ccb->NdFatFlags, ND_FAT_CLEANUP_COMPLETE)) {

				primaryFileHandle = ccb->PrimaryFileHandle;

			} else {

				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &fcb->CcbQMutex );
				
				for (primaryFileHandle = 0, ccbListEntry = fcb->CcbQueue.Flink; 
					 ccbListEntry != &fcb->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink) {

					if (!FlagOn(CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->NdFatFlags, ND_FAT_CLEANUP_COMPLETE)) {
						
						primaryFileHandle = CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->PrimaryFileHandle;
						break;
					}
				}

				ExReleaseFastMutex( &fcb->CcbQMutex );
			}

			ASSERT( primaryFileHandle );

			outputBufferLength = ((read.Length-totalReadLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (read.Length-totalReadLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
															  IRP_MJ_READ,
															  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag   = (_U32)Irp;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle = ccb->PrimaryFileHandle;

			ndfsWinxpRequestHeader->IrpFlags   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
			ndfsWinxpRequestHeader->Read.Key		= read.Key;
			ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			if (status != STATUS_SUCCESS) {

				secondaryRequest = NULL;
				status = STATUS_IO_DEVICE_ERROR;
				leave;
			}

			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			
				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			if (ndfsWinxpReplytHeader->Status == STATUS_END_OF_FILE) {
	
				ASSERT( ndfsWinxpReplytHeader->Information == 0 );

				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

				ASSERT( totalReadLength == 0 );
				ASSERT( ndfsWinxpReplytHeader->Status == STATUS_FILE_CLOSED );
							
				DebugTrace2( 0, Dbg, ("ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

				if (totalReadLength)
					status = STATUS_SUCCESS;
				else
					status = ndfsWinxpReplytHeader->Status;

				ASSERT( ndfsWinxpReplytHeader->Information == 0 );
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			ASSERT( ndfsWinxpReplytHeader->Information <= outputBufferLength );
			ASSERT( outputBufferLength == 0 || outputBuffer );

			//if (fcb->Header.FileSize.QuadPart < 100)
			//	DbgPrint( "data = %s\n", (_U8 *)(ndfsWinxpReplytHeader+1) );

			if (ndfsWinxpReplytHeader->Information && outputBuffer) {

				try {

					RtlCopyMemory( outputBuffer + totalReadLength,
								   (_U8 *)(ndfsWinxpReplytHeader+1),
								   ndfsWinxpReplytHeader->Information );

				} finally {

					if (AbnormalTermination()) {

						DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
						totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
						status = STATUS_SUCCESS;
					
					} else {
					
						if (ndfsWinxpReplytHeader->Status == STATUS_SUCCESS)
							totalReadLength += ndfsWinxpReplytHeader->Information;

						if (totalReadLength)
							status = STATUS_SUCCESS;
						else
							status = ndfsWinxpReplytHeader->Status;
					}
				}
			}

			//if (fcb->Header.FileSize.QuadPart < 100)
			//	DbgPrint( "data = %s\n", outputBuffer );
		
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

		} while( totalReadLength < read.Length );


		if (status == STATUS_FILE_CLOSED) {

			_U64	fcbHandle;
			ULONG	dataSize;
			_U8		*ndfsWinxpRequestData;


			ASSERT( ccb );
			ASSERT( totalReadLength == 0 );
			ASSERT( secondaryRequest == NULL );

			if (ccb->CreateContext.RelatedFileHandle != 0) {

				ASSERT( FALSE );
				try_return( status = STATUS_FILE_CLOSED );
			}
				
			DebugTrace2( 0, Dbg, ("SessionRecovery: ccb->Lcb->ExactCaseLink.LinkName = %wZ \n", &ccb->Fcb->FullFileName) );

			dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary,
															  IRP_MJ_CREATE,
															  (dataSize >= DEFAULT_MAX_DATA_SIZE) ? dataSize : DEFAULT_MAX_DATA_SIZE );

			if (secondaryRequest == NULL) {

				ASSERT( FALSE );
				try_return( status = STATUS_FILE_CLOSED );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

			INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
											NDFS_COMMAND_EXECUTE,
											volDo->Secondary,
											IRP_MJ_CREATE,
											(ccb->BufferLength + ccb->Fcb->FullFileName.Length) );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle = 0;

			ndfsWinxpRequestHeader->IrpFlags   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0; //irpSp->Flags;

			ndfsWinxpRequestHeader->Create.AllocationSize = 0;
			ndfsWinxpRequestHeader->Create.EaLength = 0;
			ndfsWinxpRequestHeader->Create.FileAttributes = 0;

			ndfsWinxpRequestHeader->Create.Options = 0; //irpSp->Parameters.Create.Options & ~FILE_DELETE_ON_CLOSE;
			ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
			ndfsWinxpRequestHeader->Create.Options |= (FILE_OPEN << 24);

			ndfsWinxpRequestHeader->Create.FileNameLength 
				= (USHORT)(ccb->Fcb->FullFileName.Length + (ccb->BufferLength - ccb->CreateContext.EaLength));

			ndfsWinxpRequestHeader->Create.FileNameLength = ccb->CreateContext.FileNameLength;
			ndfsWinxpRequestHeader->Create.EaLength = 0; //ccb->CreateContext.EaLength;

			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength,
						   ccb->Fcb->FullFileName.Buffer,
						   ccb->Fcb->FullFileName.Length );

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength + ccb->Fcb->FullFileName.Length,
						   ccb->Buffer + ccb->CreateContext.EaLength,
						   ccb->BufferLength - ccb->CreateContext.EaLength );

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );
				
			timeOut.QuadPart = -NDFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

			KeClearEvent(&secondaryRequest->CompleteEvent);

			if (status != STATUS_SUCCESS) {
		
				ASSERT( NDFAT_BUG );

				secondaryRequest = NULL;
				ASSERT( FALSE );
				try_return( status );
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				status = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				ASSERT( FALSE );

				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
			DebugTrace2( 0, Dbg, ("SessionRecovery: ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

			if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

				ASSERT( FALSE );
				status = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				try_return( status = STATUS_FILE_CLOSED );
			}

			primaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
			ASSERT( fcb->Handle == ndfsWinxpReplytHeader->Open.FcbHandle );

			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

			do {

				ULONG						outputBufferLength;


				outputBufferLength = ((read.Length-totalReadLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (read.Length-totalReadLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

				secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( volDo->Secondary, 
																  IRP_MJ_READ,
																  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag   = (_U32)Irp;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle = primaryFileHandle;

				ndfsWinxpRequestHeader->IrpFlags   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
				ndfsWinxpRequestHeader->Read.Key		= read.Key;
				ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

				timeOut.QuadPart = -NDFAT_TIME_OUT;
				status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
				if (status != STATUS_SUCCESS) {

					secondaryRequest = NULL;
					status = STATUS_IO_DEVICE_ERROR;
					leave;
				}

				KeClearEvent( &secondaryRequest->CompleteEvent );

				if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

					if (IrpContext->OriginatingIrp)
						PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			
					DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				if (ndfsWinxpReplytHeader->Status == STATUS_END_OF_FILE) {
	
					ASSERT( ndfsWinxpReplytHeader->Information == 0 );

					if (!(read.ByteOffset.QuadPart & (((ULONG)vcb->Bpb.BytesPerSector) - 1))) {

		
						RtlZeroMemory( outputBuffer + totalReadLength,
									   read.Length - totalReadLength );

						totalReadLength = read.Length;
				
					} else {

						ASSERT( FALSE );
					}

					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;				
				
					break;
				}

				if (ndfsWinxpReplytHeader->Status != STATUS_SUCCESS) {

					ASSERT( FALSE );
			
					DebugTrace2( 0, Dbg2, ("ndfsWinxpReplytHeader->Status = %x\n", ndfsWinxpReplytHeader->Status) );

					if (totalReadLength)
						status = STATUS_SUCCESS;
					else
						status = ndfsWinxpReplytHeader->Status;

					ASSERT( ndfsWinxpReplytHeader->Information == 0 );
					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;				
				
					break;
				}

				ASSERT( ndfsWinxpReplytHeader->Information <= outputBufferLength );
				ASSERT( outputBufferLength == 0 || outputBuffer );

				if (ndfsWinxpReplytHeader->Information && outputBuffer) {

					try {

						RtlCopyMemory( outputBuffer + totalReadLength,
									   (_U8 *)(ndfsWinxpReplytHeader+1),
									   ndfsWinxpReplytHeader->Information );

					} finally {

						if (AbnormalTermination()) {

							DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
							totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
							status = STATUS_SUCCESS;
					
						} else {
					
							if (ndfsWinxpReplytHeader->Status == STATUS_SUCCESS)
								totalReadLength += ndfsWinxpReplytHeader->Information;

							if (totalReadLength)
								status = STATUS_SUCCESS;
							else
								status = ndfsWinxpReplytHeader->Status;
						}
					}
				}
		
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;

			} while( totalReadLength < read.Length );
			
			ASSERT( totalReadLength == read.Length );

			ClosePrimaryFile( volDo->Secondary, primaryFileHandle );
		}

try_exit:

		NOTHING;

	} finally {

		if (!AbnormalTermination()) {
		
			if (totalReadLength) {

				Irp->IoStatus.Information = totalReadLength;
				Irp->IoStatus.Status = STATUS_SUCCESS;
		
			} else {
		
				Irp->IoStatus.Information = 0;
				Irp->IoStatus.Status = status;
			}
		}

		if (Irp->IoStatus.Status != STATUS_SUCCESS) {

			DebugTrace2( 0, Dbg2, ("read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
								 read.ByteOffset.QuadPart, read.Length, totalReadLength, status) );

			PrintIrp( Dbg2, "RedirectIrpMajorRead", NULL, Irp );
		}

		if (secondarySessionResourceAcquired == TRUE)
			SecondaryReleaseResourceLite( IrpContext, &volDo->Secondary->SessionResource );

		if (fcbAcquired) {
             FatReleaseFcb( IrpContext, fcb );
        }

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
	
	FatCompleteRequest( IrpContext, Irp, status );
	return status;
}


#endif