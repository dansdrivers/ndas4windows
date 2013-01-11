#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)


NTSTATUS
NdasFatSecondaryCommonRead (
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

	UINT64						primaryFileHandle = 0;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			
			return STATUS_FILE_CORRUPT_ERROR;
		}
	}

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

		ASSERT( FALSE );
        DebugTrace2( 0, Dbg, ("Can't wait in create\n") );

        status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace2( -1, Dbg2, ("NdasFatSecondaryCommonRead:  FatFsdPostRequest -> %08lx\n", status) );
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
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		outputBuffer = FatMapUserBuffer( IrpContext, Irp );
		totalReadLength = 0;

		do {

			ULONG						outputBufferLength;

			if (fcb->UncleanCount == 0) {

				DebugTrace( 0, Dbg2, "NdasFatSecondaryCommonRead: fileName = %wZ\n", &fileObject->FileName );

				status = STATUS_FILE_CLOSED;
				break;
			}

			if (!FlagOn(ccb->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {

				primaryFileHandle = ccb->PrimaryFileHandle;

			} else {

				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &fcb->NonPaged->CcbQMutex );
				
				for (primaryFileHandle = 0, ccbListEntry = fcb->NonPaged->CcbQueue.Flink; 
					 ccbListEntry != &fcb->NonPaged->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink) {

					if (!FlagOn(CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {
						
						primaryFileHandle = CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->PrimaryFileHandle;
						break;
					}
				}

				ExReleaseFastMutex( &fcb->NonPaged->CcbQMutex );
			}

			ASSERT( primaryFileHandle );

			outputBufferLength = ((read.Length-totalReadLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (read.Length-totalReadLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_READ,
															  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
			ndfsWinxpRequestHeader->Read.Key		= read.Key;
			ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
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

				NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_END_OF_FILE) {
	
				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );

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

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

				ASSERT( totalReadLength == 0 );
				ASSERT( NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_FILE_CLOSED );
							
				DebugTrace2( 0, Dbg, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

				if (totalReadLength)
					status = STATUS_SUCCESS;
				else
					status = NTOHL(ndfsWinxpReplytHeader->Status4);

				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) <= outputBufferLength );
			ASSERT( outputBufferLength == 0 || outputBuffer );

			//if (fcb->Header.FileSize.LowPart < 100)
			//	DbgPrint( "data = %s\n", (UINT8 *)(ndfsWinxpReplytHeader+1) );

			if (NTOHL(ndfsWinxpReplytHeader->Information32) && outputBuffer) {

				try {

					RtlCopyMemory( outputBuffer + totalReadLength,
								   (UINT8 *)(ndfsWinxpReplytHeader+1),
								   NTOHL(ndfsWinxpReplytHeader->Information32) );

				} finally {

					if (AbnormalTermination()) {

						DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
						totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
						status = STATUS_SUCCESS;
					
					} else {
					
						if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS)
							totalReadLength += NTOHL(ndfsWinxpReplytHeader->Information32);

						if (totalReadLength)
							status = STATUS_SUCCESS;
						else
							status = NTOHL(ndfsWinxpReplytHeader->Status4);
					}
				}
			}

			//if (fcb->Header.FileSize.LowPart < 100)
			//	DbgPrint( "data = %s\n", outputBuffer );
		
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

		} while( totalReadLength < read.Length );


		if (status == STATUS_FILE_CLOSED) {

			UINT64	fcbHandle;
			ULONG	dataSize;
			UINT8		*ndfsWinxpRequestData;


			ASSERT( ccb );
			ASSERT( totalReadLength == 0 );
			ASSERT( secondaryRequest == NULL );

			if (ccb->CreateContext.RelatedFileHandle != 0) {

				ASSERT( FALSE );
				try_return( status = STATUS_FILE_CLOSED );
			}
				
			DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: ccb->Lcb->ExactCaseLink.LinkName = %wZ \n", &ccb->Fcb->FullFileName) );

			dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary,
															  IRP_MJ_CREATE,
															  (dataSize >= DEFAULT_NDAS_MAX_DATA_SIZE) ? dataSize : DEFAULT_NDAS_MAX_DATA_SIZE );

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

			ndfsWinxpRequestHeader->FileHandle8 = 0;

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
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

			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength,
						   ccb->Fcb->FullFileName.Buffer,
						   ccb->Fcb->FullFileName.Length );

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength + ccb->Fcb->FullFileName.Length,
						   ccb->Buffer + ccb->CreateContext.EaLength,
						   ccb->BufferLength - ccb->CreateContext.EaLength );

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );
				
			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

			KeClearEvent(&secondaryRequest->CompleteEvent);

			if (status != STATUS_SUCCESS) {
		
				ASSERT( NDASFAT_BUG );

				secondaryRequest = NULL;
				ASSERT( FALSE );
				try_return( status );
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				status = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				ASSERT( FALSE );

				NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
			DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

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

				secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
																  IRP_MJ_READ,
																  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle8 = HTONLL(primaryFileHandle);

				ndfsWinxpRequestHeader->IrpFlags4   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
				ndfsWinxpRequestHeader->Read.Key		= read.Key;
				ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASFAT_TIME_OUT;
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

					NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_END_OF_FILE) {
	
					ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );

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

				if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

					ASSERT( FALSE );
			
					DebugTrace2( 0, Dbg2, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

					if (totalReadLength)
						status = STATUS_SUCCESS;
					else
						status = NTOHL(ndfsWinxpReplytHeader->Status4);

					ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;				
				
					break;
				}

				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) <= outputBufferLength );
				ASSERT( outputBufferLength == 0 || outputBuffer );

				if (NTOHL(ndfsWinxpReplytHeader->Information32) && outputBuffer) {

					try {

						RtlCopyMemory( outputBuffer + totalReadLength,
									   (UINT8 *)(ndfsWinxpReplytHeader+1),
									   NTOHL(ndfsWinxpReplytHeader->Information32) );

					} finally {

						if (AbnormalTermination()) {

							DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
							totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
							status = STATUS_SUCCESS;
					
						} else {
					
							if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS)
								totalReadLength += NTOHL(ndfsWinxpReplytHeader->Information32);

							if (totalReadLength)
								status = STATUS_SUCCESS;
							else
								status = NTOHL(ndfsWinxpReplytHeader->Status4);
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
			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );

		if (fcbAcquired) {
             FatReleaseFcb( IrpContext, fcb );
        }

		if (secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );
	}
			
	return status;
}


NTSTATUS
NdasFatSecondaryCommonRead3 (
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

	UINT64						primaryFileHandle = 0;


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	typeOfOpen = FatDecodeFileObject( fileObject, &vcb, &fcb, &ccb );

	ASSERT( typeOfOpen == UserFileOpen );

	if (FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

		/*if (FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED )) {
	
			ASSERT( FALSE );
			FatRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
					
		} else */{
					
			ASSERT( FlagOn(ccb->NdasFatFlags, ND_FAT_CCB_FLAG_CORRUPTED) );
			
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
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		outputBuffer = FatMapUserBuffer( IrpContext, Irp );
		totalReadLength = 0;

		do {

			ULONG						outputBufferLength;

			if (fcb->UncleanCount == 0) {

				DebugTrace( 0, Dbg2, "NdasFatSecondaryCommonRead: fileName = %wZ\n", &fileObject->FileName );

				status = STATUS_FILE_CLOSED;
				break;
			}

			if (!FlagOn(ccb->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {

				primaryFileHandle = ccb->PrimaryFileHandle;

			} else {

				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &fcb->NonPaged->CcbQMutex );
				
				for (primaryFileHandle = 0, ccbListEntry = fcb->NonPaged->CcbQueue.Flink; 
					 ccbListEntry != &fcb->NonPaged->CcbQueue; 
					 ccbListEntry = ccbListEntry->Flink) {

					if (!FlagOn(CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->NdasFatFlags, ND_FAT_CLEANUP_COMPLETE)) {
						
						primaryFileHandle = CONTAINING_RECORD(ccbListEntry, CCB, FcbListEntry)->PrimaryFileHandle;
						break;
					}
				}

				ExReleaseFastMutex( &fcb->NonPaged->CcbQMutex );
			}

			ASSERT( primaryFileHandle );

			outputBufferLength = ((read.Length-totalReadLength) <= volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (read.Length-totalReadLength) : volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize;

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_READ,
															  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ccb->PrimaryFileHandle);

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = 0;

			ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
			ndfsWinxpRequestHeader->Read.Key		= read.Key;
			ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASFAT_TIME_OUT;
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

				NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_END_OF_FILE) {
	
				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );

				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

				ASSERT( totalReadLength == 0 );
				ASSERT( NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_FILE_CLOSED );
							
				DebugTrace2( 0, Dbg, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

				if (totalReadLength)
					status = STATUS_SUCCESS;
				else
					status = NTOHL(ndfsWinxpReplytHeader->Status4);

				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;				
				
				break;
			}

			ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) <= outputBufferLength );
			ASSERT( outputBufferLength == 0 || outputBuffer );

			//if (fcb->Header.FileSize.LowPart < 100)
			//	DbgPrint( "data = %s\n", (UINT8 *)(ndfsWinxpReplytHeader+1) );

			if (NTOHL(ndfsWinxpReplytHeader->Information32) && outputBuffer) {

				try {

					RtlCopyMemory( outputBuffer + totalReadLength,
								   (UINT8 *)(ndfsWinxpReplytHeader+1),
								   NTOHL(ndfsWinxpReplytHeader->Information32) );

				} finally {

					if (AbnormalTermination()) {

						DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
						totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
						status = STATUS_SUCCESS;
					
					} else {
					
						if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS)
							totalReadLength += NTOHL(ndfsWinxpReplytHeader->Information32);

						if (totalReadLength)
							status = STATUS_SUCCESS;
						else
							status = NTOHL(ndfsWinxpReplytHeader->Status4);
					}
				}
			}

			//if (fcb->Header.FileSize.LowPart < 100)
			//	DbgPrint( "data = %s\n", outputBuffer );
		
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

		} while( totalReadLength < read.Length );


		if (status == STATUS_FILE_CLOSED) {

			UINT64	fcbHandle;
			ULONG	dataSize;
			UINT8		*ndfsWinxpRequestData;


			ASSERT( ccb );
			ASSERT( totalReadLength == 0 );
			ASSERT( secondaryRequest == NULL );

			if (ccb->CreateContext.RelatedFileHandle != 0) {

				ASSERT( FALSE );
				try_return( status = STATUS_FILE_CLOSED );
			}
				
			DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: ccb->Lcb->ExactCaseLink.LinkName = %wZ \n", &ccb->Fcb->FullFileName) );

			dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary,
															  IRP_MJ_CREATE,
															  (dataSize >= DEFAULT_NDAS_MAX_DATA_SIZE) ? dataSize : DEFAULT_NDAS_MAX_DATA_SIZE );

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

			ndfsWinxpRequestHeader->FileHandle8 = 0;

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
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

			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength,
						   ccb->Fcb->FullFileName.Buffer,
						   ccb->Fcb->FullFileName.Length );

			RtlCopyMemory( ndfsWinxpRequestData + ndfsWinxpRequestHeader->Create.EaLength + ccb->Fcb->FullFileName.Length,
						   ccb->Buffer + ccb->CreateContext.EaLength,
						   ccb->BufferLength - ccb->CreateContext.EaLength );

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );
				
			timeOut.QuadPart = -NDASFAT_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );

			KeClearEvent(&secondaryRequest->CompleteEvent);

			if (status != STATUS_SUCCESS) {
		
				ASSERT( NDASFAT_BUG );

				secondaryRequest = NULL;
				ASSERT( FALSE );
				try_return( status );
			}

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				status = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest( secondaryRequest );
				secondaryRequest = NULL;
				ASSERT( FALSE );

				NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
			DebugTrace2( 0, Dbg, ("SecondaryRecoverySessionStart: NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

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

				secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
																  IRP_MJ_READ,
																  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

				if (secondaryRequest == NULL) {

					FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
				}

				ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
				INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_READ, 0 );

				ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
				ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

				//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
				ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_READ;
				ndfsWinxpRequestHeader->IrpMinorFunction = 0;

				ndfsWinxpRequestHeader->FileHandle8 = HTONLL(primaryFileHandle);

				ndfsWinxpRequestHeader->IrpFlags4   = 0;
				ndfsWinxpRequestHeader->IrpSpFlags = 0;

				ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
				ndfsWinxpRequestHeader->Read.Key		= read.Key;
				ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart + totalReadLength;

				secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
				QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

				timeOut.QuadPart = -NDASFAT_TIME_OUT;
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

					NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}

				ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

				if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_END_OF_FILE) {
	
					ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );

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

				if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

					ASSERT( FALSE );
			
					DebugTrace2( 0, Dbg2, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", NTOHL(ndfsWinxpReplytHeader->Status4)) );

					if (totalReadLength)
						status = STATUS_SUCCESS;
					else
						status = NTOHL(ndfsWinxpReplytHeader->Status4);

					ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) == 0 );
					DereferenceSecondaryRequest( secondaryRequest );
					secondaryRequest = NULL;				
				
					break;
				}

				ASSERT( NTOHL(ndfsWinxpReplytHeader->Information32) <= outputBufferLength );
				ASSERT( outputBufferLength == 0 || outputBuffer );

				if (NTOHL(ndfsWinxpReplytHeader->Information32) && outputBuffer) {

					try {

						RtlCopyMemory( outputBuffer + totalReadLength,
									   (UINT8 *)(ndfsWinxpReplytHeader+1),
									   NTOHL(ndfsWinxpReplytHeader->Information32) );

					} finally {

						if (AbnormalTermination()) {

							DebugTrace2( 0, Dbg2, ("RedirectIrpMajorRead: Exception - output buffer is not valid\n") );
							totalReadLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
							status = STATUS_SUCCESS;
					
						} else {
					
							if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS)
								totalReadLength += NTOHL(ndfsWinxpReplytHeader->Information32);

							if (totalReadLength)
								status = STATUS_SUCCESS;
							else
								status = NTOHL(ndfsWinxpReplytHeader->Status4);
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
			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );

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