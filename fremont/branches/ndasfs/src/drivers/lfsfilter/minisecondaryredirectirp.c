#include "LfsProc.h"

#if __NDAS_FS_MINI__


NTSTATUS
MiniRedirectIrpMajorCreate (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	);

NTSTATUS
MiniRedirectIrpMajorRead (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	);

NTSTATUS
MiniRedirectIrpMajorQueryInformation (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	);


#define	MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(	\
				MndfsWinxpRequestHeader,			\
				Mirp,								\
				Miopb,								\
				MprimaryFileHandle					\
				);									\
{																						\
	(MndfsWinxpRequestHeader)->IrpTag4			= HTONL((UINT32)PtrToUlong(Mirp));		\
	(MndfsWinxpRequestHeader)->IrpMajorFunction = (Miopb)->MajorFunction;				\
	(MndfsWinxpRequestHeader)->IrpMinorFunction = (Miopb)->MinorFunction;				\
	(MndfsWinxpRequestHeader)->FileHandle8		= HTONLL((UINT64)(MprimaryFileHandle));	\
	(MndfsWinxpRequestHeader)->IrpFlags4		= HTONL((Mirp)->IrpFlags);				\
	(MndfsWinxpRequestHeader)->IrpSpFlags		= (Miopb)->OperationFlags;				\
}



NTSTATUS
MiniRedirectIrp (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	)
{
	NTSTATUS				returnStatus = DBG_CONTINUE; // for debugging
	
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation(Irp);
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;

	PLFS_FCB				fcb = NULL;
	PLFS_CCB				ccb = NULL;

	PSECONDARY_REQUEST		secondaryRequest = NULL;


	*FastMutexSet = FALSE;
	*Retry		  = FALSE;
	

	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	if (iopb->MajorFunction != IRP_MJ_CREATE) {

		fcb = fileObject->FsContext;
		ccb = fileObject->FsContext2;
	
		ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == ccb );
		ASSERT( ccb->Fcb == fcb );
 
		if (iopb->MajorFunction != IRP_MJ_CLOSE && iopb->MajorFunction != IRP_MJ_CLEANUP) {

			if (ccb->Corrupted == TRUE) {

				Data->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Data->IoStatus.Information = 0;

				return STATUS_SUCCESS;
			}
		}
	}


	switch (iopb->MajorFunction) {

    case IRP_MJ_CREATE: //0x00
	{
		returnStatus = MiniRedirectIrpMajorCreate( Secondary, Data, FastMutexSet, Retry );
		break;
	}

	case IRP_MJ_CLOSE: // 0x02
	{
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;

		//fcb->FileRecordSegmentHeaderAvail = FALSE;

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_ERROR) || 
			ccb->Corrupted == TRUE || 
			!(FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTED)))
		{
			SecondaryFileObjectClose( Secondary, fileObject );
		
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				( "RedirectIrp: IRP_MJ_CLOSE: free a corrupted file extension:%p\n", fileObject)); 		

			secondaryRequest = NULL;		
			*FastMutexSet = FALSE;
			*Retry = FALSE;
		
			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;
			returnStatus = STATUS_SUCCESS;

			break;
		}		

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 0 );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;	
			break;
		}

		secondaryRequest->OutputBuffer = NULL;
		secondaryRequest->OutputBufferLength = 0;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
			ndfsRequestHeader,
			NDFS_COMMAND_EXECUTE,
			Secondary,
			IRP_MJ_CLOSE,
			0
			);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);
	
		returnStatus = AcquireLockAndTestCorruptError( Secondary,
													   FastMutexSet,
													   ccb,
													   Retry,
													   &secondaryRequest,
													   secondaryRequest->SessionId );

		if (returnStatus != STATUS_SUCCESS) {
			
			secondaryRequest = NULL;

			if (returnStatus == STATUS_DEVICE_REQUIRES_CLEANING) {
	
				SecondaryFileObjectClose( Secondary, fileObject );

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

				return STATUS_SUCCESS;
			
			}
			
			return returnStatus;
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
				Secondary,
				secondaryRequest
				);
				
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			returnStatus = STATUS_TIMEOUT;	
			
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);
			
			*FastMutexSet = FALSE;

			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			returnStatus = secondaryRequest->ExecuteStatus;	
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			
			secondaryRequest = NULL;
	
			break;
		}
				
		SecondaryFileObjectClose( Secondary, fileObject );

		Data->IoStatus.Status = STATUS_SUCCESS;
		Data->IoStatus.Information = 0;


		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
			( "RedirectIrp: IRP_MJ_CLOSE: free a file extension:%p\n", fileObject));

		DereferenceSecondaryRequest( secondaryRequest );
		secondaryRequest = NULL;

		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);
		*FastMutexSet = FALSE;

		returnStatus = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_READ: // 0x03
	{
		secondaryRequest = NULL;
		returnStatus = MiniRedirectIrpMajorRead( Secondary, Data, FastMutexSet, Retry );
		break;
	}

    case IRP_MJ_WRITE: // 0x04
	{
		struct Write		write;

		PVOID				inputBuffer; // = MiniMapInputBuffer(iopb);
		PVOID				outputBuffer = NULL;
		ULONG				outputBufferLength = 0;

		BOOLEAN				synchronousIo = BooleanFlagOn(fileObject->Flags, FO_SYNCHRONOUS_IO);
		BOOLEAN				pagingIo      = BooleanFlagOn(iopb->IrpFlags, IRP_PAGING_IO);
		BOOLEAN				writeToEof;

		IO_STATUS_BLOCK		ioStatus;
		LONGLONG			lastCurrentByteOffset;

		if (iopb->Parameters.Write.WriteBuffer) {

			inputBuffer = iopb->Parameters.Write.WriteBuffer;

		} else if (iopb->Parameters.Write.MdlAddress) {

			inputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.Write.MdlAddress, NormalPagePriority);
		}

		ASSERT( inputBuffer );

		fcb->FileRecordSegmentHeaderAvail = FALSE;

		write.ByteOffset	= iopb->Parameters.Write.ByteOffset;
		write.Key			= iopb->Parameters.Write.Key;
		write.Length		= iopb->Parameters.Write.Length;


		ASSERT(!(write.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& write.ByteOffset.HighPart == -1));

		writeToEof = ((write.ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE) 
						&& (write.ByteOffset.HighPart == -1) );


//		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("WRITE: Offset:%I64d Length:%d\n", write.ByteOffset.QuadPart, write.Length)) ;
		if (iopb->TargetFileObject) {
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("WRITE: %wZ Offset:%I64d Length:%d\n", 
				&iopb->TargetFileObject->FileName, write.ByteOffset.QuadPart, write.Length)) ;
		}

		ioStatus.Information = 0;
		ioStatus.Status = 0;

		do
		{
			ULONG						inputBufferLength;
			UINT8							*ndfsWinxpRequestData;
	
			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			
			//
			//	[64bit issue]
			//	We assume Information value of WRITE operation will be
			//	less than 32bit.
			//

			ASSERT(ioStatus.Information <= 0xffffffff);

			inputBufferLength = (write.Length-ioStatus.Information <= Secondary->Thread.SessionContext.PrimaryMaxDataSize) 
								? (write.Length-(ULONG)ioStatus.Information) : Secondary->Thread.SessionContext.PrimaryMaxDataSize;

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, inputBufferLength );

			if(secondaryRequest == NULL)
			{
				ASSERT(LFS_REQUIRED);
				
				returnStatus = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
				
			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_WRITE,
				inputBufferLength
				);

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
			MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				iopb,
				iopb,
				ccb->PrimaryFileHandle
				);

			
			ndfsWinxpRequestHeader->Write.Length	= inputBufferLength;
			ndfsWinxpRequestHeader->Write.Key		= write.Key;
			
			if(writeToEof)
				ndfsWinxpRequestHeader->Write.ByteOffset = write.ByteOffset.QuadPart;
			else
				ndfsWinxpRequestHeader->Write.ByteOffset = write.ByteOffset.QuadPart+ioStatus.Information;

			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
			if(inputBufferLength) {

				try {

					RtlCopyMemory(
						ndfsWinxpRequestData,
						(PUCHAR)inputBuffer + ioStatus.Information,
						inputBufferLength
						);

				} except (EXCEPTION_EXECUTE_HANDLER) {
					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
						("RedirectIrp: Exception - Input buffer is not valid\n"));
					ioStatus.Status = GetExceptionCode();
					returnStatus = STATUS_SUCCESS;
					break;
				}
			}

			returnStatus = AcquireLockAndTestCorruptError( Secondary,
																			FastMutexSet,
																			ccb,
																			Retry,
																			&secondaryRequest,
																			secondaryRequest->SessionId );

			if (returnStatus != STATUS_SUCCESS) {
	
				return returnStatus;
			}

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest(
				Secondary,
				secondaryRequest
				);

			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
#if DBG
			if(KeGetCurrentIrql() == APC_LEVEL) {
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("PrimaryAgentThreadProc: WRITE: IRLQ is APC! going to sleep.\n"));
			}
#endif
			waitStatus = KeWaitForSingleObject(
								&secondaryRequest->CompleteEvent,
								Executive,
								KernelMode,
								FALSE,
								&timeOut
								);

			KeClearEvent(&secondaryRequest->CompleteEvent);

#if DBG
			if(KeGetCurrentIrql() == APC_LEVEL) {
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("PrimaryAgentThreadProc: WRITE:  IRLQ is APC! going to sleep.\n"));
			}
#endif

			if(waitStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_BUG);
				secondaryRequest = NULL;

				KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

				*FastMutexSet = FALSE;
				returnStatus = STATUS_TIMEOUT;			
				break;
			}

			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				returnStatus = secondaryRequest->ExecuteStatus;
				DereferenceSecondaryRequest(secondaryRequest);
				secondaryRequest = NULL;
				break;
			}
				
			returnStatus = STATUS_SUCCESS;
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			
			if(NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS)
			{
				ASSERT(NTOHL(ndfsWinxpReplytHeader->Information32) == 0);
				if(ioStatus.Information) // already read
				{
					ioStatus.Status = STATUS_SUCCESS;
				}
				else
				{
					ioStatus.Status = NTOHL(ndfsWinxpReplytHeader->Status4);
					ioStatus.Information = 0;
				}
				
				DereferenceSecondaryRequest(secondaryRequest);
				secondaryRequest = NULL;

				KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

				*FastMutexSet = FALSE;
				break;
			}

			ioStatus.Information += NTOHL(ndfsWinxpReplytHeader->Information32);
			ioStatus.Status	= STATUS_SUCCESS;
			returnStatus = STATUS_SUCCESS;
			
			lastCurrentByteOffset = NTOHLL(ndfsWinxpReplytHeader->CurrentByteOffset8);
			
			DereferenceSecondaryRequest(secondaryRequest);
			secondaryRequest = NULL;

#if 0
			if(totalWriteRequestLength != ioStatus.Information)
			{
				ASSERT(LFS_UNEXPECTED);
				ExReleaseFastMutex(&Secondary->FastMutex) ;
				*FastMutexSet = FALSE;

				break; // Write Failed
			}
#endif

			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;

		} while(ioStatus.Information < write.Length);

		secondaryRequest = NULL;
		if(returnStatus != STATUS_SUCCESS)
			break;

#if 0
		if(ioStatus.Status == STATUS_SUCCESS)
		{    
			if (synchronousIo && !pagingIo) 
			{
				if(!writeToEof && fileObject->CurrentByteOffset.QuadPart != write.ByteOffset.QuadPart)
				{
					fileObject->CurrentByteOffset.QuadPart = write.ByteOffset.QuadPart;
					fileObject->CurrentByteOffset.QuadPart += ioStatus.Information;
				}
#if 0				
				if(writeToEof) 
				{
				}
				else
				{
					ASSERT(fileObject->CurrentByteOffset.QuadPart == write.ByteOffset.QuadPart);
					fileObject->CurrentByteOffset.QuadPart += ioStatus.Information;
				}
#endif
			}
		}
#endif

		Data->IoStatus.Status = ioStatus.Status;
		Data->IoStatus.Information = ioStatus.Information;
			
		if (Data->IoStatus.Status == STATUS_SUCCESS && synchronousIo && !pagingIo) 
		{
			if(writeToEof)
			{
				if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
					fileObject->CurrentByteOffset.QuadPart = lastCurrentByteOffset;
			}
			else
			{
				fileObject->CurrentByteOffset.QuadPart = write.ByteOffset.QuadPart + ioStatus.Information;
			}
		}

		if(fileObject->SectionObjectPointer == NULL)
			fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

		returnStatus = STATUS_SUCCESS;
		
		break;
	}

    case IRP_MJ_QUERY_INFORMATION: // 0x05
	{
#if 0
		struct QueryFile			queryFile;
		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = MiniMapOutputBuffer(iopb);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
#endif


		secondaryRequest = NULL;
		returnStatus = MiniRedirectIrpMajorQueryInformation(Secondary, Data, FastMutexSet, Retry);
		break;
#if 0
		queryFile.FileInformationClass	= iopb->Parameters.QueryFile.FileInformationClass;
		queryFile.Length				= iopb->Parameters.QueryFile.Length;
		outputBufferLength				= queryFile.Length;


		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, outputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_INFORMATION,
				0
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);
	
		ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

		returnStatus = STATUS_SUCCESS;
		
		break;
#endif
	}

    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		struct SetFile				setFile;

		PVOID						inputBuffer; // = MiniMapInputBuffer(iopb);
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		ULONG						outputBufferLength = 0;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

		
		inputBuffer = iopb->Parameters.SetFileInformation.InfoBuffer;

		fcb->FileRecordSegmentHeaderAvail = FALSE;
		
		setFile.AdvanceOnly				= iopb->Parameters.SetFileInformation.AdvanceOnly;
		setFile.ClusterCount			= iopb->Parameters.SetFileInformation.ClusterCount;
		setFile.DeleteHandle			= iopb->Parameters.SetFileInformation.DeleteHandle;
		setFile.FileInformationClass	= iopb->Parameters.SetFileInformation.FileInformationClass;
		setFile.FileObject				= iopb->Parameters.SetFileInformation.ParentOfTarget;
		setFile.Length					= iopb->Parameters.SetFileInformation.Length;
		setFile.ReplaceIfExists			= iopb->Parameters.SetFileInformation.ReplaceIfExists;


		if(setFile.FileInformationClass == FileBasicInformation)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FileBasicInformation\n") ) ;
		}
		else if(setFile.FileInformationClass == FileLinkInformation) 
		{
			PFILE_LINK_INFORMATION		linkInfomation = (PFILE_LINK_INFORMATION)inputBuffer;
			
			inputBufferLength = linkInfomation->FileNameLength;
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FileLinkInformation\n") ) ;
		}
		else if(setFile.FileInformationClass == FileRenameInformation) 
		{
			PFILE_RENAME_INFORMATION	renameInformation = (PFILE_RENAME_INFORMATION)inputBuffer;
			
			inputBufferLength = renameInformation->FileNameLength;
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FileRenameInformation\n") ) ;
		}
		else if(setFile.FileInformationClass == FileDispositionInformation) 
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FileDispositionInformation\n") ) ;
		}
		else if(setFile.FileInformationClass == FileEndOfFileInformation) 
		{
			IO_STATUS_BLOCK					ioStatusBlock;

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FileEndOfFileInformation\n") ) ;

            CcFlushCache( &fcb->NonPaged->SectionObjectPointers, NULL, 0, &ioStatusBlock);
			if(ioStatusBlock.Status != STATUS_SUCCESS)
			{
				secondaryRequest = NULL;

				Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Data->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}
		}
		else if(setFile.FileInformationClass == FileAllocationInformation) 
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FileAllocationInformation\n") ) ;
		}
		else if(FilePositionInformation == setFile.FileInformationClass)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("LFS: Sencodary_RedirectIrp: process SET_INFORMATION: FilePositionInformation\n") ) ;
		}
		else
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				( "RedirectIrp: setFile.FileInformationClass = %d\n", setFile.FileInformationClass)) ;
			ASSERT(LFS_REQUIRED);

			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Data->IoStatus.Information = 0;
			
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;

			break;					
		}

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		
		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, inputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_SET_INFORMATION,
				inputBufferLength
				);
	
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->SetFile.Length					= setFile.Length;
		ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;
		
		{
			PLFS_CCB	setFileCcb;

			setFileCcb = Secondary_LookUpCcb(Secondary, fileObject);

			if(setFileCcb == NULL)
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;

				Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Data->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}

			ndfsWinxpRequestHeader->SetFile.FileHandle = setFileCcb->PrimaryFileHandle;
		}

		ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
		ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;

		//
		//	[64bit issue] 
		//	I assume DeleteHandle's value is guaranteed to be less than 0xffffffff
		//	becuase every file handle comes from the primary host through 32bit protocol.
		//

		ASSERT((ULONG_PTR)setFile.DeleteHandle < 0xffffffff);

		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= 0; //setFile.DeleteHandle;


		if(setFile.FileInformationClass == FileBasicInformation)
		{
			PFILE_BASIC_INFORMATION	basicInformation = (PFILE_BASIC_INFORMATION)inputBuffer;
			
			ndfsWinxpRequestHeader->SetFile.BasicInformation.CreationTime   = basicInformation->CreationTime.QuadPart;
			ndfsWinxpRequestHeader->SetFile.BasicInformation.LastAccessTime = basicInformation->LastAccessTime.QuadPart;
			ndfsWinxpRequestHeader->SetFile.BasicInformation.LastWriteTime  = basicInformation->LastWriteTime.QuadPart;
			ndfsWinxpRequestHeader->SetFile.BasicInformation.ChangeTime     = basicInformation->ChangeTime.QuadPart;
			ndfsWinxpRequestHeader->SetFile.BasicInformation.FileAttributes = basicInformation->FileAttributes;
		}
		else if(setFile.FileInformationClass == FileLinkInformation) 
		{
			PFILE_LINK_INFORMATION	linkInformation = (PFILE_LINK_INFORMATION)inputBuffer;
			PLFS_CCB			rootCcb;

			ndfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists = linkInformation->ReplaceIfExists;
			ndfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength	= linkInformation->FileNameLength;
			
			if(linkInformation->RootDirectory == NULL) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					( "RedirectIrp: FileLinkInformation: No RootDirectory\n")) ;

				ndfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle = 0;	
			} else 
			{
				rootCcb = Secondary_LookUpCcbByHandle(Secondary, linkInformation->RootDirectory);
				if(!rootCcb) 
				{
					ASSERT(LFS_BUG);
					DereferenceSecondaryRequest(
						secondaryRequest
						);
					secondaryRequest = NULL;

					Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
					Data->IoStatus.Information = 0;
			
					returnStatus = STATUS_SUCCESS;
					break;		
				}

				ndfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle = rootCcb->PrimaryFileHandle;
			}

			if(linkInformation->FileNameLength)
			{
				ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
				RtlCopyMemory(
					ndfsWinxpRequestData,
					linkInformation->FileName,
					linkInformation->FileNameLength
					);
			}
		}
		else if(setFile.FileInformationClass == FileRenameInformation) 
		{
			PFILE_RENAME_INFORMATION	renameInformation = (PFILE_RENAME_INFORMATION)inputBuffer;
			PLFS_CCB				rootCcb;
			

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
				("FileRenameInformation renameInformation->FileName = %ws", renameInformation->FileName));
			
			//PrintIrp(LFS_DEBUG_SECONDARY_TRACE, NULL, Secondary->LfsDeviceExt, Irp);
			ndfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists = renameInformation->ReplaceIfExists;
			ndfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength  = renameInformation->FileNameLength;
			
			if(renameInformation->RootDirectory == NULL) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					( "RedirectIrp: FileRenameInformation: No RootDirectory\n")) ;

				ndfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle = 0;
			}		
			else
			{
				rootCcb = Secondary_LookUpCcbByHandle(Secondary, renameInformation->RootDirectory);
				if(!rootCcb) 
				{
					ASSERT(LFS_BUG);
					DereferenceSecondaryRequest(
						secondaryRequest
						);
					secondaryRequest = NULL;

					Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
					Data->IoStatus.Information = 0;
			
					returnStatus = STATUS_SUCCESS;
					break;		
				}

				ndfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle = rootCcb->PrimaryFileHandle;
			}

			if(renameInformation->FileNameLength)
			{
				ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
				RtlCopyMemory(
					ndfsWinxpRequestData,
					renameInformation->FileName,
					renameInformation->FileNameLength
					);
			}
		}
		else if(setFile.FileInformationClass == FileDispositionInformation) 
		{
			PFILE_DISPOSITION_INFORMATION	dispositionInformation = (PFILE_DISPOSITION_INFORMATION)inputBuffer;
			
			ndfsWinxpRequestHeader->SetFile.DispositionInformation.DeleteFile = dispositionInformation->DeleteFile;
		}
		else if(setFile.FileInformationClass == FileEndOfFileInformation)
		{
			PFILE_END_OF_FILE_INFORMATION	fileEndOfFileInformation = (PFILE_END_OF_FILE_INFORMATION)inputBuffer;
			
			ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile  = fileEndOfFileInformation->EndOfFile.QuadPart;
		}
		else if(setFile.FileInformationClass == FileAllocationInformation)
		{
			PFILE_ALLOCATION_INFORMATION	fileAllocationInformation = (PFILE_ALLOCATION_INFORMATION)inputBuffer;
			
			ndfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize  = fileAllocationInformation->AllocationSize.QuadPart;
		}
		else if(FilePositionInformation == setFile.FileInformationClass)
		{
			PFILE_POSITION_INFORMATION filePositionInformation = (PFILE_POSITION_INFORMATION)inputBuffer;

			ndfsWinxpRequestHeader->SetFile.PositionInformation.CurrentByteOffset = filePositionInformation->CurrentByteOffset.QuadPart ;
		}
		else
			ASSERT(LFS_BUG);
		
		returnStatus = AcquireLockAndTestCorruptError( Secondary,
																		FastMutexSet,
																		ccb,
																		Retry,
																		&secondaryRequest,
																		secondaryRequest->SessionId );

		if (returnStatus != STATUS_SUCCESS) {
	
			return returnStatus;
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
				Secondary,
				secondaryRequest
				);
				
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
//							&timeOut
							0
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			returnStatus = STATUS_TIMEOUT;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;

			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			returnStatus = secondaryRequest->ExecuteStatus;	
			DereferenceSecondaryRequest(
				secondaryRequest
				);			
			secondaryRequest = NULL;
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			
		Data->IoStatus.Status = NTOHL(ndfsWinxpReplytHeader->Status4);
		Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32);


		if(NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(
					secondaryRequest
					);
				
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
	
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);
	
			*FastMutexSet = FALSE;
	
			break;
		}
		
		if(setFile.FileInformationClass == FilePositionInformation) 
		{
			PFILE_POSITION_INFORMATION filePositionInformation = (PFILE_POSITION_INFORMATION)inputBuffer;

			ASSERT(!(filePositionInformation->CurrentByteOffset.HighPart == -1));
			fileObject->CurrentByteOffset.QuadPart = filePositionInformation->CurrentByteOffset.QuadPart;
		}
		else if(setFile.FileInformationClass == FileEndOfFileInformation)
		{
			if(fcb->Header.PagingIoResource)
			{
				ASSERT(LFS_REQUIRED);
			}				

            FsRtlFastUnlockAll( 
				&fcb->FileLock,
                fileObject,
                IoThreadToProcess(Data->Thread),
                NULL
				);

			CcPurgeCacheSection( 
				&fcb->NonPaged->SectionObjectPointers,
				NULL,
                0,
                FALSE
				);
		}
		
		DereferenceSecondaryRequest(
			secondaryRequest
			);
				
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;

		//ExReleaseFastMutex(&Secondary->FastMutex) ;
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;
				
		break;
	}
	
    case IRP_MJ_QUERY_EA: // 0x07
	{
		struct QueryEa				queryEa;

		PVOID						inputBuffer;
		ULONG						inputBufferLength; /* = queryEa->EaListLength;*/
		PVOID						outputBuffer; // = MiniMapOutputBuffer(iopb);
		ULONG						outputBufferLength;
		ULONG						bufferLength; /*= (inputBufferLength >= outputBufferLength)?inputBufferLength:outputBufferLength ; */

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
		ULONG						returnedDataSize;

		if (iopb->Parameters.QueryEa.EaBuffer) {

			outputBuffer = iopb->Parameters.QueryEa.EaBuffer;

		} else if (iopb->Parameters.QueryEa.MdlAddress) {

			outputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.QueryEa.MdlAddress, NormalPagePriority);
		}

		ASSERT( outputBuffer );

		queryEa.EaIndex			= iopb->Parameters.QueryEa.EaIndex;
		queryEa.EaList			= iopb->Parameters.QueryEa.EaList;
		queryEa.EaListLength	= iopb->Parameters.QueryEa.EaListLength;
		queryEa.Length			= iopb->Parameters.QueryEa.Length;
		
		inputBuffer				= queryEa.EaList;
		outputBufferLength		= queryEa.Length;

		if(inputBuffer != NULL)
		{
			PFILE_GET_EA_INFORMATION	fileGetEa = (PFILE_GET_EA_INFORMATION)inputBuffer;
		
			inputBufferLength = 0;
			while(fileGetEa->NextEntryOffset)
			{
				inputBufferLength += fileGetEa->NextEntryOffset;
				fileGetEa = (PFILE_GET_EA_INFORMATION)((UINT8 *)fileGetEa + fileGetEa->NextEntryOffset);
			}

			inputBufferLength += (sizeof(FILE_GET_EA_INFORMATION) - sizeof(CHAR) + fileGetEa->EaNameLength);
		}
		else
			inputBufferLength = 0;
		
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED) = %d queryEa.EaIndex = %d queryEa.EaList = %p queryEa.Length = %d, inputBufferLength = %d\n",
							BooleanFlagOn(iopb->IrpFlags, SL_INDEX_SPECIFIED), queryEa.EaIndex, queryEa.EaList, queryEa.EaListLength, inputBufferLength));

		bufferLength = (inputBufferLength >= outputBufferLength)?inputBufferLength:outputBufferLength ;
		
		ASSERT(bufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, bufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer			= outputBuffer;
		secondaryRequest->OutputBufferLength	= outputBufferLength;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_EA,
				inputBufferLength
				);
	
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->QueryEa.Length			= queryEa.Length;
		ndfsWinxpRequestHeader->QueryEa.EaIndex			= queryEa.EaIndex;
		ndfsWinxpRequestHeader->QueryEa.EaListLength	= queryEa.EaListLength;
//		ndfsWinxpRequestHeader->QueryEa.EaListLength	= inputBufferLength;

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength);

		returnStatus = STATUS_SUCCESS;
		
		returnStatus = AcquireLockAndTestCorruptError( Secondary,
																		FastMutexSet,
																		ccb,
																		Retry,
																		&secondaryRequest,
																		secondaryRequest->SessionId );

		if (returnStatus != STATUS_SUCCESS) {
	
			return returnStatus;
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);
			
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			returnStatus = STATUS_TIMEOUT;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;

			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			returnStatus = secondaryRequest->ExecuteStatus;	
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;		
			break;
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		Data->IoStatus.Status	  = NTOHL(ndfsWinxpReplytHeader->Status4);
		Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32); 

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: Data->IoStatus.Status = %d, Data->IoStatus.Information = %d\n",
					Data->IoStatus.Status, Data->IoStatus.Information));

		returnedDataSize = NTOHL(secondaryRequest->NdfsReplyHeader.MessageSize4) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

		if(returnedDataSize)
		{
			ASSERT(returnedDataSize <= ADD_ALIGN8(queryEa.Length));
			ASSERT(outputBuffer);
		
			RtlCopyMemory(
				outputBuffer,
				(UINT8 *)(ndfsWinxpReplytHeader+1),
				(returnedDataSize < queryEa.Length) ? returnedDataSize : queryEa.Length
				);
		}
		
		DereferenceSecondaryRequest(secondaryRequest);
		
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;		

/*
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Data->IoStatus.Status = STATUS_EAS_NOT_SUPPORTED;
		Data->IoStatus.Information = 0;
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;
*/
		break;
    }

    case IRP_MJ_SET_EA: // 0x08
	{
		struct SetEa				setEa;
		PVOID						inputBuffer; // = MiniMapInputBuffer(iopb);
		ULONG						inputBufferLength; /* = setEa->Length; */
		
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		if (iopb->Parameters.SetEa.EaBuffer) {

			inputBuffer = iopb->Parameters.SetEa.EaBuffer;

		} else if (iopb->Parameters.SetEa.MdlAddress) {

			inputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.SetEa.MdlAddress, NormalPagePriority);
		}

		ASSERT( inputBuffer );

		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setEa.Length =	iopb->Parameters.SetEa.Length;

		if(inputBuffer != NULL)
		{
			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)inputBuffer;
		
			inputBufferLength = 0;
			while(fileFullEa->NextEntryOffset)
			{
				inputBufferLength += fileFullEa->NextEntryOffset;
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			inputBufferLength += (sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength);

		}
		else
			inputBufferLength = 0;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
							("RedirectIrp, IRP_MJ_SET_EA: Ea is set setEa->Length = %d, inputBufferLength = %d\n",
									setEa.Length, inputBufferLength));

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, inputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_SET_EA,
				inputBufferLength
				);
	
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->SetEa.Length = setEa.Length;

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength);

		returnStatus = STATUS_SUCCESS;

/*
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Data->IoStatus.Status = STATUS_EAS_NOT_SUPPORTED;
		Data->IoStatus.Information = 0;
		secondaryRequest = NULL;			
		returnStatus = STATUS_SUCCESS;
*/
		break;
    }
    
     case IRP_MJ_FLUSH_BUFFERS: // 0x09
	{
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 0 );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			Data->IoStatus.Information = 0;
			
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_FLUSH_BUFFERS,
				0
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		returnStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: // 0x0a
	{
		struct QueryVolume			queryVolume;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer; // = MiniMapOutputBuffer(iopb);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

		outputBuffer = iopb->Parameters.QueryVolumeInformation.VolumeBuffer; //MapOutputBuffer(Irp);

		queryVolume.FsInformationClass	= iopb->Parameters.QueryVolumeInformation.FsInformationClass;
		queryVolume.Length				= iopb->Parameters.QueryVolumeInformation.Length;
		
		outputBufferLength				= queryVolume.Length;

		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, outputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_VOLUME_INFORMATION,
				0
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->QueryVolume.Length			   = outputBufferLength;
		ndfsWinxpRequestHeader->QueryVolume.FsInformationClass = queryVolume.FsInformationClass;

		returnStatus = STATUS_SUCCESS;
#if DBG
		if(queryVolume.FsInformationClass == FileFsVolumeInformation){
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,("FileFsVolumeInformation\n"));
		}
#endif
		//
		//  Notify anyone who cares about the label change
		//	NOTE: this is a workaround for Windows explorer on secondaries that
		//		does not refresh volume names when other hosts change volume names
		//
		if(queryVolume.FsInformationClass == FileFsSizeInformation ||
			queryVolume.FsInformationClass == FileFsFullSizeInformation) {
			Secondary->Thread.VolRefreshTick ++;

			if(	(Secondary->Thread.VolRefreshTick%10) == 0 &&
				Secondary->LfsDeviceExt && Secondary->LfsDeviceExt->DiskDeviceObject) {
				TARGET_DEVICE_CUSTOM_NOTIFICATION changeEvent;

				changeEvent.Version = 1;
				changeEvent.FileObject = NULL;
				changeEvent.NameBufferOffset = -1;
				changeEvent.Size = (USHORT)FIELD_OFFSET( TARGET_DEVICE_CUSTOM_NOTIFICATION, CustomDataBuffer);

				RtlCopyMemory( &changeEvent.Event, &GUID_IO_VOLUME_CHANGE, sizeof(GUID_IO_VOLUME_CHANGE));

				IoReportTargetDeviceChange(Secondary->LfsDeviceExt->DiskDeviceObject, &changeEvent);

				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("Reported volume change. VolRefreshTick %d\n", Secondary->Thread.VolRefreshTick));
			}
		}
		break;
	}

	case IRP_MJ_SET_VOLUME_INFORMATION:  // 0x0b
	{
		struct	SetVolume			setVolume;
		PVOID						inputBuffer; // = MiniMapInputBuffer(iopb);
		ULONG						inputBufferLength;
		PVOID						outputBuffer = NULL;
		ULONG						outputBufferLength = 0;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		inputBuffer = iopb->Parameters.SetVolumeInformation.VolumeBuffer;

		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setVolume.FsInformationClass	= iopb->Parameters.SetVolumeInformation.FsInformationClass;
		setVolume.Length				= iopb->Parameters.SetVolumeInformation.Length;
		
		inputBufferLength				= setVolume.Length;

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, inputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_SET_VOLUME_INFORMATION,
				inputBufferLength
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);
	

		ndfsWinxpRequestHeader->SetVolume.Length				= setVolume.Length;
		ndfsWinxpRequestHeader->SetVolume.FsInformationClass	= setVolume.FsInformationClass ;
	
		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, setVolume.Length) ;

		returnStatus = STATUS_SUCCESS;
		break;
	}
		
	case IRP_MJ_DIRECTORY_CONTROL: // 0x0C
	{		
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_NOISE,
				("RedirectIrp: IRP_MJ_DIRECTORY_CONTROL: MinorFunction = %X\n",
					iopb->MinorFunction));

        if(iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY) 
		{
#if 0
			ULONG CompletionFilter;
			BOOLEAN WatchTree;
			

			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_NOISE,
					("RedirectIrp: IRP_MJ_DIRECTORY_CONTROL: IRP_MN_NOTIFY_CHANGE_DIRECTORY Called\n"));
			
			CompletionFilter = iopb->Parameters.NotifyDirectory.CompletionFilter;
			WatchTree = BooleanFlagOn(iopb->Flags, SL_WATCH_TREE);

			FsRtlNotifyFullChangeDirectory( 
				Secondary->NotifySync,
				&Secondary->DirNotifyList,
				ccb,
				(PSTRING)&fcb->FullFileName,
				WatchTree,
				FALSE,
				CompletionFilter,
				Irp,
				NULL,
				NULL
				);

			Data->IoStatus.Status = STATUS_SUCCESS;
#endif
			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Data->IoStatus.Information = 0;

			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
			break;
		}
        else if(iopb->MinorFunction == IRP_MN_QUERY_DIRECTORY) 
		{
			struct QueryDirectory		queryDirectory;
			PVOID						inputBuffer;
			ULONG						inputBufferLength;
			PVOID						outputBuffer; // = MiniMapOutputBuffer(iopb);
			ULONG						outputBufferLength;

			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
			UINT8							*ndfsWinxpRequestData;

			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			ULONG						returnedDataSize;

			if (iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer) {

				outputBuffer = iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;

			} else if (iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress) {

				outputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress, NormalPagePriority);
			}

			queryDirectory.FileIndex			= iopb->Parameters.DirectoryControl.QueryDirectory.FileIndex;
			queryDirectory.FileInformationClass = iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass;
			queryDirectory.FileName				= (PSTRING)iopb->Parameters.DirectoryControl.QueryDirectory.FileName;
			queryDirectory.Length				= iopb->Parameters.DirectoryControl.QueryDirectory.Length;

			inputBuffer			= (queryDirectory.FileName) ? (queryDirectory.FileName->Buffer) : NULL;
			inputBufferLength	= (queryDirectory.FileName) ? (queryDirectory.FileName->Length) : 0;
			outputBufferLength	= queryDirectory.Length;

			if(queryDirectory.FileName)
			{
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_NOISE,
					("RedirectIrp: IRP_MN_QUERY_DIRECTORY: queryFileName = %wZ\n",
						queryDirectory.FileName));
			}

			ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
			ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary,
															 (inputBufferLength > outputBufferLength) ? inputBufferLength: outputBufferLength );

			if(secondaryRequest == NULL)
			{
				returnStatus = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_DIRECTORY_CONTROL,
				inputBufferLength
				);

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
			MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				iopb,
				iopb,
				ccb->PrimaryFileHandle
				);
	
			ndfsWinxpRequestHeader->QueryDirectory.Length				= outputBufferLength;
			ndfsWinxpRequestHeader->QueryDirectory.FileInformationClass	= queryDirectory.FileInformationClass;
			ndfsWinxpRequestHeader->QueryDirectory.FileIndex			= queryDirectory.FileIndex;

			// 
			// Modify request if this secondary has connected to new primary 
			//		because new one does not know about current query context.
			//
			if (ccb->LastQueryFileIndex != (ULONG)-1 &&
				ccb->LastDirectoryQuerySessionId != ccb->SessionId &&
				!(iopb->IrpFlags & SL_RESTART_SCAN)) {
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO,
					("RedirectIrp: IRP_MN_QUERY_DIRECTORY: Primary has changed. Recover query context...\n"));
				ndfsWinxpRequestHeader->IrpSpFlags |= SL_INDEX_SPECIFIED;
				ndfsWinxpRequestHeader->QueryDirectory.FileIndex = ccb->LastQueryFileIndex;
			}

			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
			if(inputBufferLength)
				RtlCopyMemory(
					ndfsWinxpRequestData,
					inputBuffer,
					inputBufferLength
					);
		
			returnStatus = AcquireLockAndTestCorruptError( Secondary,
																		    FastMutexSet,
																			ccb,
																			Retry,
																			&secondaryRequest,
																			secondaryRequest->SessionId );

			if (returnStatus != STATUS_SUCCESS) {

				return returnStatus;
			}

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest(
				Secondary,
				secondaryRequest
				);
			
			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
			waitStatus = KeWaitForSingleObject(
								&secondaryRequest->CompleteEvent,
								Executive,
								KernelMode,
								FALSE,
								&timeOut
								);

			KeClearEvent(&secondaryRequest->CompleteEvent);

			if(waitStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_BUG);

				secondaryRequest = NULL;
				returnStatus = STATUS_TIMEOUT;	
				
				//ExReleaseFastMutex(&Secondary->FastMutex) ;
				KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

				*FastMutexSet = FALSE;

				break;
			}

			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				returnStatus = secondaryRequest->ExecuteStatus;	
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;		
				break;
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			Data->IoStatus.Status	  = NTOHL(ndfsWinxpReplytHeader->Status4);
			Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32); 

			returnedDataSize = NTOHL(secondaryRequest->NdfsReplyHeader.MessageSize4) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

			if(returnedDataSize)
			{
				ASSERT(returnedDataSize <= ADD_ALIGN8(queryDirectory.Length));
				ASSERT(outputBuffer);
		
				RtlCopyMemory(
					outputBuffer,
					(UINT8 *)(ndfsWinxpReplytHeader+1),
					(returnedDataSize < queryDirectory.Length) ? returnedDataSize : queryDirectory.Length
					);
				//	
				// Save last query index for the case that primary changes and lose its query context.
				//
				ccb->LastQueryFileIndex = LfsGetLastFileIndexFromQuery(
					queryDirectory.FileInformationClass,
					outputBuffer,
					(returnedDataSize < queryDirectory.Length) ? returnedDataSize : queryDirectory.Length);
				ccb->LastDirectoryQuerySessionId = ccb->SessionId;
			} else {
				//	
				// Save last query index for the case that primary changes and lose its query context.
				//
				ccb->LastQueryFileIndex = (ULONG)-1;
				ccb->LastDirectoryQuerySessionId = ccb->SessionId;
			}
		
			DereferenceSecondaryRequest(
				secondaryRequest
				);
		
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
		
			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;		

			break;
		} 
		else
		{
			ASSERT(LFS_UNEXPECTED);
			secondaryRequest = NULL;

			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Data->IoStatus.Information = 0;
			
			returnStatus = STATUS_SUCCESS;

			break;
		}

		returnStatus = STATUS_SUCCESS;
		break;
	}	
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: // 0x0D
	{
		struct FileSystemControl	fileSystemControl;
		PVOID						inputBuffer = MinispyMapInputBuffer(iopb, Data->RequestorMode);
		ULONG						inputBufferLength;
		PVOID						outputBuffer = MinispyMapOutputBuffer(iopb, Data->RequestorMode);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
		
		fcb->FileRecordSegmentHeaderAvail = FALSE;
	
		fileSystemControl.FsControlCode			= iopb->Parameters.FileSystemControl.Common.FsControlCode;
		fileSystemControl.InputBufferLength		= iopb->Parameters.FileSystemControl.Common.InputBufferLength;
		fileSystemControl.OutputBufferLength	= iopb->Parameters.FileSystemControl.Common.OutputBufferLength;
		//fileSystemControl.Type3InputBuffer		= iopb->Parameters.FileSystemControl.Common.Type3InputBuffer;

		outputBufferLength = fileSystemControl.OutputBufferLength;

		if(iopb->MinorFunction == IRP_MN_USER_FS_REQUEST 
			|| iopb->MinorFunction == IRP_MN_KERNEL_CALL) 
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp: IRP_MJ_FILE_SYSTEM_CONTROL: MajorFunction = %X MinorFunction = %X Function = %d outputBufferLength = %d\n",
					iopb->MajorFunction, iopb->MinorFunction, (fileSystemControl.FsControlCode & 0x00003FFC) >> 2, outputBufferLength));

			//
			//	Do not allow exclusive access to the volume and dismount volume to protect format
			//
			
			if(fileSystemControl.FsControlCode == FSCTL_LOCK_VOLUME				// 6
				|| fileSystemControl.FsControlCode == FSCTL_DISMOUNT_VOLUME)	// 8
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrp, IRP_MJ_FILE_SYSTEM_CONTROL: Secondary is trying to acquire the volume exclusively. Denied it.\n")) ;

				ASSERT(fileObject 
						&& fileObject->FileName.Length == 0 
						&& fileObject->RelatedFileObject == NULL); 

				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0 ;

				returnStatus = STATUS_SUCCESS;
				break ;
			}
		}
		else if(iopb->MinorFunction == IRP_MN_MOUNT_VOLUME 
			|| iopb->MinorFunction == IRP_MN_VERIFY_VOLUME 
			|| iopb->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) 
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
		else
		{
			ASSERT(LFS_UNEXPECTED);

			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Data->IoStatus.Information = 0;
			
			returnStatus = STATUS_SUCCESS;
			break ;
		}

		//DbgPrint( "fileSystemControl.FsControlCode = %x FSCTL_MARK_HANDLE = %x\n", fileSystemControl.FsControlCode, FSCTL_MARK_HANDLE );

		if (fileSystemControl.FsControlCode == FSCTL_MARK_HANDLE) {

			Data->IoStatus.Status = STATUS_SUCCESS; //STATUS_INVALID_DEVICE_REQUEST;
			Data->IoStatus.Information = 0;

			returnStatus = STATUS_SUCCESS;
			break;
		}
#if 1
		if (fileSystemControl.FsControlCode == FSCTL_GET_COMPRESSION	||
			fileSystemControl.FsControlCode == FSCTL_GET_VOLUME_BITMAP	||
			fileSystemControl.FsControlCode == FSCTL_FILESYSTEM_GET_STATISTICS) {

			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED; //STATUS_INVALID_DEVICE_REQUEST;
			Data->IoStatus.Information = 0;

			returnStatus = STATUS_SUCCESS;
			break;
		}
#endif

		if (Secondary->Thread.SessionContext.SecondaryMaxDataSize < outputBufferLength) {

			NDAS_ASSERT( FALSE );
			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED; //STATUS_INVALID_DEVICE_REQUEST;
			Data->IoStatus.Information = 0;

			returnStatus = STATUS_SUCCESS;
			break;
		}

		//ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		//ASSERT(iopb->MinorFunction == IRP_MN_USER_FS_REQUEST);
		
		if(fileSystemControl.FsControlCode == FSCTL_MOVE_FILE)			// 29
		{
			inputBufferLength = 0;			
		} 
		else if(fileSystemControl.FsControlCode == FSCTL_MARK_HANDLE)	// 63
		{
			inputBufferLength = 0;			
		}
		else
		{
			inputBufferLength  = fileSystemControl.InputBufferLength;
		}

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary,
														  (inputBufferLength > outputBufferLength) ? inputBufferLength: outputBufferLength );
		
		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;		
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_FILE_SYSTEM_CONTROL,
				inputBufferLength
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);
	
		ndfsWinxpRequestHeader->FileSystemControl.OutputBufferLength	= fileSystemControl.OutputBufferLength;
		ndfsWinxpRequestHeader->FileSystemControl.InputBufferLength		= fileSystemControl.InputBufferLength;
		ndfsWinxpRequestHeader->FileSystemControl.FsControlCode			= fileSystemControl.FsControlCode;

		if(fileSystemControl.FsControlCode == FSCTL_MOVE_FILE)			// 29
		{
			PMOVE_FILE_DATA	moveFileData = inputBuffer;	
			PLFS_CCB	moveCcb;

			moveCcb = Secondary_LookUpCcbByHandle(Secondary, moveFileData->FileHandle);			
			if(!moveCcb) 
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;

				Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Data->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}

			moveCcb->Fcb->FileRecordSegmentHeaderAvail = FALSE;

			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.FileHandle	= moveCcb->PrimaryFileHandle;
			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingVcn	= moveFileData->StartingVcn.QuadPart;
			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingLcn	= moveFileData->StartingLcn.QuadPart;
			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.ClusterCount	= moveFileData->ClusterCount;
		} 
		else if(fileSystemControl.FsControlCode == FSCTL_MARK_HANDLE)	// 63
		{
			PMARK_HANDLE_INFO	markHandleInfo = inputBuffer;	
			PLFS_CCB		volumeCcb;


			volumeCcb = Secondary_LookUpCcbByHandle(Secondary, markHandleInfo->VolumeHandle);
			if(!volumeCcb) 
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;

				Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Data->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}

			volumeCcb->Fcb->FileRecordSegmentHeaderAvail = FALSE;

			ndfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.UsnSourceInfo	= markHandleInfo->UsnSourceInfo;
			ndfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.VolumeHandle	= volumeCcb->PrimaryFileHandle;
			ndfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.HandleInfo		= markHandleInfo->HandleInfo;
		}
		else
		{
			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

			if(inputBufferLength)
				RtlCopyMemory(
					ndfsWinxpRequestData,
					inputBuffer,
					inputBufferLength
					);
		}

		returnStatus = AcquireLockAndTestCorruptError( Secondary,
																		FastMutexSet,
																		ccb,
																		Retry,
																		&secondaryRequest,
																		secondaryRequest->SessionId );

		if (returnStatus != STATUS_SUCCESS) {
	
			return returnStatus;
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);
			
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			returnStatus = STATUS_TIMEOUT;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;

			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			returnStatus = secondaryRequest->ExecuteStatus;	
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;		
			break;
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		Data->IoStatus.Status	  = NTOHL(ndfsWinxpReplytHeader->Status4);
		Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32); 

		if(Data->IoStatus.Status == STATUS_SUCCESS || Data->IoStatus.Status == STATUS_BUFFER_OVERFLOW)
			ASSERT(ADD_ALIGN8(NTOHL(secondaryRequest->NdfsReplyHeader.MessageSize4) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)) == ADD_ALIGN8(NTOHL(ndfsWinxpReplytHeader->Information32)));

		if(fileSystemControl.OutputBufferLength)
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp: IRP_MJ_FILE_SYSTEM_CONTROL: Function = %d fileSystemControl.OutputBufferLength = %d NTOHL(ndfsWinxpReplytHeader->Information32) = %d\n",
					(fileSystemControl.FsControlCode & 0x00003FFC) >> 2, fileSystemControl.OutputBufferLength, NTOHL(ndfsWinxpReplytHeader->Information32)));

			ASSERT(Data->IoStatus.Information <= secondaryRequest->OutputBufferLength);
			ASSERT(secondaryRequest->OutputBuffer);
		
			RtlCopyMemory(
				secondaryRequest->OutputBuffer,
				(UINT8 *)(ndfsWinxpReplytHeader+1),
				NTOHL(ndfsWinxpReplytHeader->Information32)
				);
		}
		
		DereferenceSecondaryRequest(
			secondaryRequest
			);
		
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;
			
		break;
	}


//	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
    case IRP_MJ_DEVICE_CONTROL:  // 0E
	{
		struct DeviceIoControl		deviceIoControl;
		PVOID						inputBuffer = MinispyMapInputBuffer(iopb, Data->RequestorMode);
		ULONG						inputBufferLength;
		PVOID						outputBuffer = MinispyMapOutputBuffer(iopb, Data->RequestorMode);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		deviceIoControl.IoControlCode		= iopb->Parameters.DeviceIoControl.Common.IoControlCode;
		deviceIoControl.InputBufferLength	= iopb->Parameters.DeviceIoControl.Common.InputBufferLength;
		deviceIoControl.OutputBufferLength	= iopb->Parameters.DeviceIoControl.Common.OutputBufferLength;
		//deviceIoControl.Type3InputBuffer	= iopb->Parameters.DeviceIoControl.Common.Type3InputBuffer;

		inputBufferLength  = deviceIoControl.InputBufferLength;
		outputBufferLength = deviceIoControl.OutputBufferLength;

		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("RedirectIrp: IRP_MJ_DEVICE_CONTROL: MajorFunction = %X MinorFunction = %X Function = %d outputBufferLength = %d\n",
				iopb->MajorFunction, iopb->MinorFunction, (deviceIoControl.IoControlCode & 0x00003FFC) >> 2, outputBufferLength));

		//ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		//ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary,
								                          (inputBufferLength > outputBufferLength) ? inputBufferLength: outputBufferLength );
		
		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_DEVICE_CONTROL,
				inputBufferLength
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength	= outputBufferLength;
		ndfsWinxpRequestHeader->DeviceIoControl.InputBufferLength	= inputBufferLength;
		ndfsWinxpRequestHeader->DeviceIoControl.IoControlCode		= deviceIoControl.IoControlCode;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("LFS: Secondary_RedirectIrp: IO_CONTROL: code %08lx in:%d out:%d \n",
												deviceIoControl.IoControlCode,
												inputBufferLength,
												outputBufferLength
												)) ;

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
		if(inputBufferLength)
			RtlCopyMemory(
				ndfsWinxpRequestData,
				inputBuffer,
				deviceIoControl.InputBufferLength
				);

		returnStatus = STATUS_SUCCESS;
		
		break;
	}
		
    case IRP_MJ_SHUTDOWN: // 0x10
	{
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		Data->IoStatus.Information = 0;
		secondaryRequest = NULL;			
		returnStatus = STATUS_SUCCESS;

		break;
    }

	case IRP_MJ_LOCK_CONTROL: // 0x11
	{
		struct LockControl			lockControl;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		ULONG						outputBufferLength = 0;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		lockControl.ByteOffset	= iopb->Parameters.LockControl.ByteOffset;
		lockControl.Key			= iopb->Parameters.LockControl.Key;
		lockControl.Length		= iopb->Parameters.LockControl.Length;

		ASSERT(!(lockControl.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
						|| lockControl.ByteOffset.HighPart == -1));

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("IRP_MJ_LOCK_CONTROL: iopb->MinorFunction = %d, lockControl.Key = %x\n", 
					iopb->MinorFunction, lockControl.Key));
		//PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrp", Secondary->LfsDeviceExt, Irp);

		if(iopb->MinorFunction == IRP_MN_LOCK)
		{
		}
		else if(iopb->MinorFunction == IRP_MN_UNLOCK_SINGLE)
		{
		}
		else if(iopb->MinorFunction == IRP_MN_UNLOCK_ALL)
		{
#if	__NDAS_FS_HCT_TEST_MODE__
			//ASSERT(LFS_REQUIRED);
#endif
		}
		else
		{
#if DBG
			UNICODE_STRING	nullName;
		
			RtlInitUnicodeString(&nullName, L"NULL");

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				( "IRQL:%d IRP:%p %s(%d:%d) file: %p %wZ %c%c%c%c\n",
					KeGetCurrentIrql(),
					iopb,
					IrpMajors[iopb->MajorFunction],
					(int)iopb->MajorFunction,
					(int)iopb->MinorFunction,
					fileObject,
					(fileObject) ? &fileObject->FileName : &nullName,
		            (iopb->IrpFlags & IRP_PAGING_IO) ? '*' : ' ',
			        (iopb->IrpFlags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
					(iopb->IrpFlags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
					(fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' '
					));
#endif
		
			//ASSERT(LFS_IRP_NOT_IMPLEMENTED);
			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Data->IoStatus.Information = 0;
			
			returnStatus = STATUS_SUCCESS;
			break;
		}

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 0 );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_LOCK_CONTROL,
				0
				);
	
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		if(iopb->MinorFunction == IRP_MN_LOCK || iopb->MinorFunction == IRP_MN_UNLOCK_SINGLE)
		{
			ndfsWinxpRequestHeader->LockControl.Length		= lockControl.Length->QuadPart;
			ndfsWinxpRequestHeader->LockControl.Key			= lockControl.Key;
			ndfsWinxpRequestHeader->LockControl.ByteOffset	= lockControl.ByteOffset.QuadPart;

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ( "LFS: IRP_MJ_LOCK_CONTROL: Length:%I64d Key:%08lx Offset:%I64d Ime:%d Ex:%d\n",
						lockControl.Length->QuadPart,
						lockControl.Key,
						lockControl.ByteOffset.QuadPart,
						(iopb->IrpFlags & SL_FAIL_IMMEDIATELY) != 0,
						(iopb->IrpFlags & SL_EXCLUSIVE_LOCK) != 0
						)) ;
		}

		returnStatus = STATUS_SUCCESS;
		break;
	}

	case IRP_MJ_CLEANUP: // 0x12
	{          
		if(ccb->TypeOfOpen == UserFileOpen && fileObject->SectionObjectPointer)
		{
			IO_STATUS_BLOCK		ioStatusBlock;
			LARGE_INTEGER		largeZero = {0,0};
		
			if(fcb->Header.PagingIoResource)
			{
				ASSERT(LFS_REQUIRED);

				Data->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Data->IoStatus.Information = 0;
				secondaryRequest = NULL;			
				returnStatus = STATUS_SUCCESS;

				InterlockedDecrement(&fcb->UncleanCount);
				SetFlag(fileObject->Flags, FO_CLEANUP_COMPLETE);

				break;
			}				
			
            FsRtlFastUnlockAll( 
				&fcb->FileLock,
                fileObject,
                IoThreadToProcess(Data->Thread),
                NULL
				);

            CcFlushCache(&fcb->NonPaged->SectionObjectPointers, NULL, 0, &ioStatusBlock);
			CcPurgeCacheSection( 
				&fcb->NonPaged->SectionObjectPointers,
				NULL,
                0,
                TRUE
				);

            // CcUninitializeCacheMap(fileObject, &largeZero, NULL);
		}

		if(ccb->Corrupted == TRUE)
		{
			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;
			
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
		}
		else
		{
			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;

			secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, 0 );

			if(secondaryRequest == NULL)
			{
				returnStatus = STATUS_INSUFFICIENT_RESOURCES;	
				break;
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_CLEANUP,
				0
				);

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
			MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				iopb,
				iopb,
				ccb->PrimaryFileHandle
				);

			if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0 
				&& ccb->TypeOfOpen != UserFileOpen)
			{
				ClearFlag(ndfsWinxpRequestHeader->IrpFlags4, HTONL(IRP_CLOSE_OPERATION));
			}

			returnStatus = AcquireLockAndTestCorruptError( Secondary,
														   FastMutexSet,
														   ccb,
														   Retry,
														   &secondaryRequest,
														   secondaryRequest->SessionId );

			if (returnStatus != STATUS_SUCCESS) {
	
				return returnStatus;
			}

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest(
					Secondary,
					secondaryRequest
					);

			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
			waitStatus = KeWaitForSingleObject(
								&secondaryRequest->CompleteEvent,
								Executive,
								KernelMode,
								FALSE,
								&timeOut
								);
	
			KeClearEvent(&secondaryRequest->CompleteEvent);

			if(waitStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_BUG);
				secondaryRequest = NULL;
				returnStatus = STATUS_TIMEOUT;	
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

				*FastMutexSet = FALSE;
				break;
			}
		
			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				returnStatus = secondaryRequest->ExecuteStatus;	
				DereferenceSecondaryRequest(secondaryRequest);				
				secondaryRequest = NULL;		
				break;
			}

			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;
				
			DereferenceSecondaryRequest(secondaryRequest);	
			secondaryRequest = NULL;
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;
			returnStatus = STATUS_SUCCESS;
		}

		InterlockedDecrement(&fcb->UncleanCount);
		SetFlag(fileObject->Flags, FO_CLEANUP_COMPLETE);

		break;
	}

	case IRP_MJ_QUERY_SECURITY: //0x14
	{
		struct QuerySecurity		querySecurity;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer; // = MiniMapOutputBuffer(iopb);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

		if (iopb->Parameters.QuerySecurity.SecurityBuffer) {

			outputBuffer = iopb->Parameters.QuerySecurity.SecurityBuffer;

		} else if (iopb->Parameters.QuerySecurity.MdlAddress) {

			outputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.QuerySecurity.MdlAddress, NormalPagePriority);
		}

		querySecurity.Length				= iopb->Parameters.QuerySecurity.Length;
		querySecurity.SecurityInformation	= iopb->Parameters.QuerySecurity.SecurityInformation;

		outputBufferLength = querySecurity.Length;

		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, Secondary->Thread.SessionContext.SecondaryMaxDataSize );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_SECURITY,
				0
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);
	
		ndfsWinxpRequestHeader->QuerySecurity.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QuerySecurity.SecurityInformation	= querySecurity.SecurityInformation;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ( "LFS: Secondary_RedirectIrp: IRP_MJ_QUERY_SECURITY: OutputBufferLength:%d\n", outputBufferLength)) ;

		returnStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_SET_SECURITY: //0x15
	{
		struct SetSecurity			setSecurity;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		ULONG						outputBufferLength = 0;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		NTSTATUS					securityStatus;
		ULONG						securityLength = 0;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setSecurity.SecurityDescriptor  = iopb->Parameters.SetSecurity.SecurityDescriptor;
		setSecurity.SecurityInformation = iopb->Parameters.SetSecurity.SecurityInformation;

		//
		//	get the input buffer size.
		//
		securityStatus = SeQuerySecurityDescriptorInfo(
								&setSecurity.SecurityInformation,
								NULL,
								&securityLength,
								&setSecurity.SecurityDescriptor
								);
		
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("LFS: Secondary_RedirectIrp: IRP_MJ_SET_SECURITY: "
						"The length of the security desc:%lu\n",securityLength));

		if( ( !securityLength && securityStatus == STATUS_BUFFER_TOO_SMALL ) ||
			( securityLength &&  securityStatus != STATUS_BUFFER_TOO_SMALL ))
		{
			ASSERT(LFS_UNEXPECTED);
#if 0			
			securityLength = Secondary->Thread.SessionContext.PrimaryMaxDataSize;
			Data->IoStatus.Status = securityStatus;
			Data->IoStatus.Information = 0;
			secondaryRequest = NULL;			
			
			returnStatus = STATUS_SUCCESS;
			break;

			ASSERT(LFS_UNEXPECTED);
			secondaryRequest = NULL;
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
#endif
		}


		inputBufferLength = securityLength;

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, inputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_SET_SECURITY,
				inputBufferLength
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);
	

		ndfsWinxpRequestHeader->SetSecurity.Length					= inputBufferLength;
		ndfsWinxpRequestHeader->SetSecurity.SecurityInformation		= setSecurity.SecurityInformation;

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

		securityStatus = SeQuerySecurityDescriptorInfo(
								&setSecurity.SecurityInformation,
								(PSECURITY_DESCRIPTOR)ndfsWinxpRequestData,
								&securityLength,
								&setSecurity.SecurityDescriptor
								);
		
		if(securityStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_UNEXPECTED);
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;
			Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Data->IoStatus.Information = 0;
			secondaryRequest = NULL;			
			returnStatus = STATUS_SUCCESS;
			
			break ;
		}

		ASSERT(securityLength == inputBufferLength);
		
		returnStatus = STATUS_SUCCESS;
		break;
	}

	case IRP_MJ_QUERY_QUOTA:				// 0x19
	{
		struct QueryQuota			queryQuota;

		PVOID						inputBuffer;
		ULONG						inputBufferLength;
		PVOID						outputBuffer;
		ULONG						outputBufferLength;
		ULONG						bufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;


		queryQuota.Length			= iopb->Parameters.QueryQuota.Length;
		queryQuota.SidList			= iopb->Parameters.QueryQuota.SidList;
		queryQuota.SidListLength	= iopb->Parameters.QueryQuota.SidListLength;
		queryQuota.StartSid			= iopb->Parameters.QueryQuota.StartSid;

		inputBuffer = queryQuota.SidList ;
		inputBufferLength = queryQuota.SidListLength ;
		
		if (iopb->Parameters.QueryQuota.QuotaBuffer) {

			outputBuffer = iopb->Parameters.QueryQuota.QuotaBuffer;

		} else if (iopb->Parameters.QueryQuota.MdlAddress) {

			outputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.QueryQuota.MdlAddress, NormalPagePriority);
		}
		outputBufferLength = queryQuota.Length ;
		bufferLength = (inputBufferLength >= outputBufferLength)?inputBufferLength:outputBufferLength ;
		

		ASSERT(bufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, bufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		secondaryRequest->OutputBuffer			= outputBuffer;
		secondaryRequest->OutputBufferLength	= outputBufferLength;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_QUOTA,
				inputBufferLength
				);
	
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		//
		//	set request-specific parameters
		//
		ndfsWinxpRequestHeader->QueryQuota.Length			= outputBufferLength;
		ndfsWinxpRequestHeader->QueryQuota.InputLength		= inputBufferLength;
		ndfsWinxpRequestHeader->QueryQuota.StartSidOffset	= (ULONG)((PCHAR)queryQuota.StartSid - (PCHAR)queryQuota.SidList);

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength) ;

		returnStatus = STATUS_SUCCESS;
		break ;
	}

	case IRP_MJ_SET_QUOTA:					// 0x1a
	{
		struct SetQuota				setQuota;

		PVOID						inputBuffer;
		ULONG						inputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		UINT8							*ndfsWinxpRequestData;

		
		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setQuota.Length = iopb->Parameters.SetQuota.Length;

		if (iopb->Parameters.SetQuota.QuotaBuffer) {

			inputBuffer = iopb->Parameters.SetQuota.QuotaBuffer;

		} else if (iopb->Parameters.SetQuota.MdlAddress) {

			inputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.SetQuota.MdlAddress, NormalPagePriority);
		}

		ASSERT( inputBuffer );

		inputBufferLength	= setQuota.Length ;

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, inputBufferLength );

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_SET_QUOTA,
				inputBufferLength
				);
	
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			iopb,
			iopb,
			ccb->PrimaryFileHandle
			);

		//
		//	set request-specific parameters
		//
		ndfsWinxpRequestHeader->SetQuota.Length	= inputBufferLength;

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength) ;
		returnStatus = STATUS_SUCCESS;

		break ;
	}

	default:

#if DBG
	{
		UNICODE_STRING	nullName;
		
		RtlInitUnicodeString(&nullName, L"NULL");

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
			( "IRQL:%d IRP:%p %s(%d:%d) file: %p %wZ %c%c%c%c\n",
				KeGetCurrentIrql(),
				iopb,
				IrpMajors[iopb->MajorFunction],
				(int)iopb->MajorFunction,
				(int)iopb->MinorFunction,
				fileObject,
				(fileObject) ? &fileObject->FileName : &nullName,
                (iopb->IrpFlags & IRP_PAGING_IO) ? '*' : ' ',
                (iopb->IrpFlags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
				(iopb->IrpFlags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
				(fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' '
				));
	}

#endif
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Data->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		Data->IoStatus.Information = 0;
		secondaryRequest = NULL;			
		returnStatus = STATUS_SUCCESS;

		break;
	}

	if(secondaryRequest)
	{
		Data->IoStatus.Information = 0;
		Data->IoStatus.Status = STATUS_UNSUCCESSFUL;

		returnStatus = AcquireLockAndTestCorruptError( Secondary,
																		FastMutexSet,
																		ccb,
																		Retry,
																		&secondaryRequest,
																		secondaryRequest->SessionId);
		
		if (returnStatus != STATUS_SUCCESS) {

			return returnStatus;
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);
			
		do // just for structural Programing
		{
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			UINT32						returnedDataSize;
		
			
			if(secondaryRequest->Synchronous == FALSE)
				break;
		
				
			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
			waitStatus = KeWaitForSingleObject(
								&secondaryRequest->CompleteEvent,
								Executive,
								KernelMode,
								FALSE,
								&timeOut
								);

			KeClearEvent(&secondaryRequest->CompleteEvent);

			if(waitStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_BUG);

				secondaryRequest = NULL;
				returnStatus = STATUS_TIMEOUT;	
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

				*FastMutexSet = FALSE;
				break;
			}

			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				returnStatus = secondaryRequest->ExecuteStatus;	
				DereferenceSecondaryRequest(secondaryRequest);

				secondaryRequest = NULL;		
				break;
			}

			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			Data->IoStatus.Status	  = NTOHL(ndfsWinxpReplytHeader->Status4);
			Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32); 

			returnedDataSize = NTOHL(secondaryRequest->NdfsReplyHeader.MessageSize4) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

			if(returnedDataSize)
			{
				ASSERT(Data->IoStatus.Status == STATUS_SUCCESS || Data->IoStatus.Status == STATUS_BUFFER_OVERFLOW);
				if(Data->IoStatus.Status == STATUS_SUCCESS)
					ASSERT(ADD_ALIGN8(returnedDataSize) == ADD_ALIGN8(NTOHL(ndfsWinxpReplytHeader->Information32)));
				
				ASSERT(Data->IoStatus.Information <= secondaryRequest->OutputBufferLength);
				ASSERT(secondaryRequest->OutputBuffer);

				// OutputBuffer may not be valid anymore if user process is destroyed
				try {
					RtlCopyMemory(
						secondaryRequest->OutputBuffer,
						(UINT8 *)(ndfsWinxpReplytHeader+1),
						NTOHL(ndfsWinxpReplytHeader->Information32)
						);
				} except(EXCEPTION_EXECUTE_HANDLER) {
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_ERROR, ("LFS: Got OutputBuffer exception in RedirectIrp: Exception code=0x%08x\n",GetExceptionCode()));
					returnStatus = STATUS_SUCCESS;
					Data->IoStatus.Status = GetExceptionCode();
				}

#if DBG
				if(IRP_MJ_QUERY_INFORMATION == iopb->MajorFunction) {
					struct QueryFile *queryFile =(struct QueryFile *)&(iopb->Parameters.QueryFileInformation) ;

					if(FileStandardInformation == queryFile->FileInformationClass) {
						PFILE_STANDARD_INFORMATION info = (PFILE_STANDARD_INFORMATION)secondaryRequest->OutputBuffer ;
						SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("StandartInformation: Alloc:%I64d EOF:%I64d Links:%d Del:%d Dir:%d\n",
							info->AllocationSize.QuadPart,
							info->EndOfFile.QuadPart,
							info->NumberOfLinks,
							info->DeletePending,
							info->Directory)) ;
					}


				}
#endif
			}

			DereferenceSecondaryRequest(
				secondaryRequest
			);
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;
			
		} while(0);
	}

	if(*Retry == FALSE)
		ASSERT(returnStatus != DBG_CONTINUE);


	return returnStatus;
}


NTSTATUS
MiniRedirectIrpMajorCreate (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	)
{
	NTSTATUS					returnStatus;
	//PIO_STACK_LOCATION		iopb = IoGetCurrentIrpStackLocation(Irp);
	PFLT_IO_PARAMETER_BLOCK		iopb = Data->Iopb;
	PFILE_OBJECT				fileObject = iopb->TargetFileObject;

	PLFS_FCB					fcb;
	PLFS_CCB					ccb;

	ULONG						dataSize;
	PSECONDARY_REQUEST			secondaryRequest;

	struct Create				create;
    ULONG						eaLength;			
		
	WINXP_REQUEST_CREATE		CreateContext;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	UINT8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;
	NTSTATUS					waitStatus;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	//PFILE_RECORD_SEGMENT_HEADER	fileRecordSegmentHeader;
	UNICODE_STRING				fullFileName;
	PWCHAR						fullFileNameBuffer;
	NTSTATUS					appendStatus;
	TYPE_OF_OPEN				typeOfOpen;

	ULONG						ccbSize;
	BOOLEAN						potentialDeadVolLock;


	//
	//	Potential dead volume-lock check
	//

	potentialDeadVolLock = FALSE;

	if (Data->RequestorMode == UserMode) {

		if (fileObject) {
		
			UNICODE_STRING		rootDir;

			RtlInitUnicodeString( &rootDir, L"\\" );

			if (RtlEqualUnicodeString(&rootDir, &fileObject->FileName, TRUE) || fileObject->FileName.Length == 0) {

				potentialDeadVolLock = TRUE;
			}
		}
	}

	//
	//	Build parameters for the request
	//

	create.EaLength			= iopb->Parameters.Create.EaLength;
	create.FileAttributes	= iopb->Parameters.Create.FileAttributes;
	create.Options			= iopb->Parameters.Create.Options;
	create.SecurityContext	= iopb->Parameters.Create.SecurityContext;
	create.ShareAccess		= iopb->Parameters.Create.ShareAccess;

	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );
	
	NDAS_ASSERT( !FlagOn(iopb->IrpFlags, SL_OPEN_PAGING_FILE) );

	//if (FILE_OPEN_BY_FILE_ID & create.Options)
	//	PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrp FILE_OPEN_BY_FILE_ID", Secondary->LfsDeviceExt, Irp);

	if (iopb->Parameters.Create.EaBuffer != NULL) {

		PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)iopb->Parameters.Create.EaBuffer;
		
		eaLength = 0;
		
		while (fileFullEa->NextEntryOffset) {

			eaLength += fileFullEa->NextEntryOffset;
			fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
		}

		eaLength += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					   ("RedirectIrp, IRP_MJ_CREATE: Ea is set create->EaLength = %d, eaLength = %d\n",
						create.EaLength, eaLength) );
	
	} else {

		eaLength = 0;
	}

	ASSERT( fileObject->FsContext == NULL );	
	ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

	ASSERT( fileObject->FileName.Length + eaLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize );

	dataSize = eaLength + fileObject->FileName.Length;

	secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, dataSize );

	if (secondaryRequest == NULL) {

		Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		Data->IoStatus.Information = 0;

		return STATUS_SUCCESS;
	}

	secondaryRequest->PotentialDeadVolLock = potentialDeadVolLock;
	secondaryRequest->OutputBuffer = NULL;
	secondaryRequest->OutputBufferLength = 0;

	ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
	
	INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
									NDFS_COMMAND_EXECUTE,
									Secondary,
									IRP_MJ_CREATE,
									eaLength + fileObject->FileName.Length );

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);

	ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

	MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader,
											   iopb,
											   iopb,
											   0 );

	if (fileObject->RelatedFileObject) {

		PLFS_CCB	relatedCcb = fileObject->RelatedFileObject->FsContext2;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("RedirectIrp, IRP_MJ_CREATE: RelatedFileObject is binded\n") );

		if (relatedCcb->LfsMark != LFS_MARK) {

			NDAS_ASSERT( LFS_BUG );
			
			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;

			Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
			Data->IoStatus.Information = 0;

			return STATUS_SUCCESS;
		}
	
		// ASSERT(relatedCcb->SessionId == Secondary->SessionId); It's tested later implicitely
		
		if (relatedCcb->Corrupted == TRUE) {

			DereferenceSecondaryRequest(secondaryRequest);
			secondaryRequest = NULL;

			Data->IoStatus.Status = STATUS_OBJECT_PATH_NOT_FOUND;
			Data->IoStatus.Information = 0;

			return STATUS_SUCCESS;
		}

		relatedCcb->Fcb->FileRecordSegmentHeaderAvail = FALSE;
	
		CreateContext.RelatedFileHandle = relatedCcb->PrimaryFileHandle; 
	
	} else {
	
		CreateContext.RelatedFileHandle = 0;
	}		

	CreateContext.SecurityContext.DesiredAccess	= create.SecurityContext->DesiredAccess;
	CreateContext.Options						= create.Options;
	CreateContext.FileAttributes				= create.FileAttributes;
	CreateContext.ShareAccess					= create.ShareAccess;
	
	CreateContext.EaLength						= eaLength; /* ? create->EaLength : 0; */

	CreateContext.AllocationSize				= iopb->Parameters.Create.AllocationSize.QuadPart;

	//added by ktkim 03/15/2004

	CreateContext.SecurityContext.FullCreateOptions	= create.SecurityContext->FullCreateOptions;
	CreateContext.FileNameLength					= fileObject->FileName.Length;

	RtlCopyMemory( &ndfsWinxpRequestHeader->Create,
				   &CreateContext,
				   sizeof(WINXP_REQUEST_CREATE) );
		
	ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);
	//encryptedData = ndfsWinxpRequestData + ADD_ALIGN8(create->EaLength + fileObject->FileName.Length);

	if (eaLength) {

		// It have structure align Problem. If you wanna release, Do more
		
		PFILE_FULL_EA_INFORMATION eaBuffer = iopb->Parameters.Create.EaBuffer;
		
		RtlCopyMemory( ndfsWinxpRequestData, eaBuffer, eaLength );
	}

	if (fileObject->FileName.Length) {

		RtlCopyMemory( ndfsWinxpRequestData + eaLength,
					   fileObject->FileName.Buffer,
					   fileObject->FileName.Length );
	}

	ccb = NULL;

	returnStatus = AcquireLockAndTestCorruptError( Secondary,
												   FastMutexSet,
												   ccb,
												   Retry,
												   &secondaryRequest,
												   secondaryRequest->SessionId );
	
	if (returnStatus != STATUS_SUCCESS) {

		return returnStatus;
	}

	secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
	QueueingSecondaryRequest(Secondary, secondaryRequest);

	timeOut.QuadPart = -LFS_TIME_OUT;		// 180 sec

	waitStatus = KeWaitForSingleObject( &secondaryRequest->CompleteEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

	KeClearEvent( &secondaryRequest->CompleteEvent );

	if (waitStatus != STATUS_SUCCESS) {

		NDAS_ASSERT( LFS_BUG );

		secondaryRequest = NULL;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;
		return STATUS_ALREADY_DISCONNECTED;
	}

	if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

		returnStatus = secondaryRequest->ExecuteStatus;	
		DereferenceSecondaryRequest(secondaryRequest);
			
		secondaryRequest = NULL;
		
		return returnStatus;
	}

	ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

	if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					   ("RedirectIrpMajorCreate: fileObject->FileName = %wZ, relatedFileObject->FileName = %wZ, create.Options = %x, Status = %x\n",
					    &fileObject->FileName, 
						(fileObject->RelatedFileObject ? &fileObject->RelatedFileObject->FileName : NULL), 
						create.Options,
						NTOHL(ndfsWinxpReplytHeader->Status4)) );
	}

	Data->IoStatus.Status = NTOHL(ndfsWinxpReplytHeader->Status4);
	Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32);

	if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

		DereferenceSecondaryRequest(secondaryRequest);				
		secondaryRequest = NULL;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;

		return STATUS_SUCCESS;
	}

	if (fileObject->FileName.Length == 0) {

		typeOfOpen = UserVolumeOpen;
	
	} else if ((fileObject->FileName.Buffer[fileObject->FileName.Length/sizeof(WCHAR)-1] == L'\\') || 
				BooleanFlagOn( iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE)) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
							("RedirectIrpMajorCreate: Directory fileObject->FileName = %wZ FlagOn(iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE) = %d\n",
							 &fileObject->FileName, BooleanFlagOn(iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE)) );
		
			typeOfOpen = UserDirectoryOpen;
	
	} else {

			typeOfOpen = UserFileOpen;	
	}
	
	if (typeOfOpen == UserFileOpen && iopb->Parameters.Create.Options & FILE_NO_INTERMEDIATE_BUFFERING) {

		fileObject->Flags |= FO_CACHE_SUPPORTED;
	}

	//	Allocate a file name buffer
	
	fullFileNameBuffer = ExAllocatePoolWithTag(NonPagedPool, NDFS_MAX_PATH, LFS_ALLOC_TAG);
	
	if (fullFileNameBuffer == NULL) {
	
		NDAS_ASSERT( LFS_REQUIRED );
		KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );

		*FastMutexSet = FALSE;
		Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		Data->IoStatus.Information = 0;

		return STATUS_SUCCESS;
	}

	RtlInitEmptyUnicodeString( &fullFileName, fullFileNameBuffer, NDFS_MAX_PATH );

	appendStatus = Secondary_MakeFullFileName( fileObject, 
											   &fullFileName,
											   (typeOfOpen == UserDirectoryOpen) );

	if (appendStatus != STATUS_SUCCESS) {

		ExFreePool(fullFileNameBuffer);

		ASSERT(LFS_UNEXPECTED);
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;
		Data->IoStatus.Status = appendStatus;
		Data->IoStatus.Information = 0;

		return STATUS_SUCCESS;
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				   ("RedirectIrpMajorCreate: fileObject->FileName = %wZ, relatedFileObject->FileName = %wZ, fullFileName = %wZ, "
				    "create.Options = %x, Status = %x, Information = %d\n",
				    &fileObject->FileName, 
						(fileObject->RelatedFileObject ? &fileObject->RelatedFileObject->FileName : NULL), 
					&fullFileName,
					create.Options,
					NTOHL(ndfsWinxpReplytHeader->Status4),
					NTOHL(ndfsWinxpReplytHeader->Information32)) );

	fcb = Secondary_LookUpFcb( Secondary,
							   &fullFileName,
							   !BooleanFlagOn(iopb->IrpFlags, SL_CASE_SENSITIVE) );

	if (fcb == NULL) {

		KIRQL	oldIrql;
		BOOLEAN fcbQueueEmpty;
			
			
		KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);
		fcbQueueEmpty = IsListEmpty(&Secondary->FcbQueue);
		KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);
			
		fcb = AllocateFcb( Secondary,
						   &fullFileName,
						   BooleanFlagOn(iopb->OperationFlags, SL_OPEN_PAGING_FILE) );

		if (fcb == NULL) {

			ExFreePool( fullFileNameBuffer );

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );

			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);
			
			*FastMutexSet = FALSE;
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		ExInterlockedInsertHeadList( &Secondary->FcbQueue,
									 &fcb->ListEntry,
									 &Secondary->FcbQSpinLock );
	
	}

	if (create.SecurityContext->FullCreateOptions & FILE_DELETE_ON_CLOSE) {

		fcb->DeletePending = TRUE;
	}

	InterlockedIncrement(&fcb->OpenCount);
	InterlockedIncrement(&fcb->UncleanCount);

	ccbSize = eaLength + NDFS_MAX_PATH*sizeof(WCHAR) + Secondary->Thread.SessionContext.BytesPerSector;
	
	if (fcb->FileRecordSegmentHeaderAvail == TRUE) {

		ccbSize += Secondary->Thread.SessionContext.BytesPerSector;
	}

	ccb = AllocateCcb( Secondary, fileObject, ccbSize );

	if (ccb == NULL) {

		KIRQL oldIrql;

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );

		KeAcquireSpinLock( &Secondary->FcbQSpinLock, &oldIrql );

		InterlockedDecrement( &fcb->UncleanCount );
		
		if (InterlockedDecrement(&fcb->OpenCount) == 0) {

			RemoveEntryList( &fcb->ListEntry );
			InitializeListHead( &fcb->ListEntry );
		}	

		KeReleaseSpinLock( &Secondary->FcbQSpinLock, oldIrql );
		
		Secondary_DereferenceFcb( fcb );

		fileObject->FsContext = NULL;

		ExFreePool( fullFileNameBuffer );
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;
		Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		Data->IoStatus.Information = 0;

		return STATUS_SUCCESS;
	}

	ASSERT(ndfsWinxpReplytHeader->Open.FileHandle != 0);

	ccb->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
	ccb->Fcb = fcb;
	ccb->TypeOfOpen = typeOfOpen;

	RtlCopyMemory( &ccb->CreateContext, &CreateContext, sizeof(WINXP_REQUEST_CREATE) );

	if (eaLength) {

		// It have structure align Problem. If you wanna release, Do more
		
		PFILE_FULL_EA_INFORMATION eaBuffer = iopb->Parameters.Create.EaBuffer;
		
		RtlCopyMemory( ccb->Buffer, eaBuffer, eaLength );
	}

	if (fileObject->FileName.Length) {
		
		RtlCopyMemory( ccb->Buffer + eaLength,
					   fileObject->FileName.Buffer,
					   fileObject->FileName.Length );
	}

    ExAcquireFastMutex( &Secondary->CcbQMutex );
	
	InsertHeadList( &Secondary->CcbQueue, &ccb->ListEntry );
	
	ExReleaseFastMutex( &Secondary->CcbQMutex );

	fileObject->FsContext = fcb;
	fileObject->FsContext2 = ccb;

	if (ndfsWinxpReplytHeader->Open.SetSectionObjectPointer == TRUE) {

		fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;
	
	} else {

		fileObject->SectionObjectPointer = NULL;
	}

	fileObject->Vpb = Secondary->LfsDeviceExt->Vpb;

	DereferenceSecondaryRequest( secondaryRequest );
				
	secondaryRequest = NULL;

	ExFreePool( fullFileNameBuffer );

	KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );

	*FastMutexSet = FALSE;
	
	return STATUS_SUCCESS;
}


NTSTATUS
MiniRedirectIrpMajorRead (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	)
{
	NTSTATUS					returnStatus;
	//PIO_STACK_LOCATION		iopb = IoGetCurrentIrpStackLocation(Irp);
	PFLT_IO_PARAMETER_BLOCK		iopb = Data->Iopb;
	PFILE_OBJECT				fileObject = iopb->TargetFileObject;

	PLFS_FCB					fcb = fileObject->FsContext;
	PLFS_CCB				ccb = fileObject->FsContext2;

	PSECONDARY_REQUEST			secondaryRequest;

	struct Read					read;

	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PUCHAR						outputBuffer; // = (PUCHAR)MiniMapOutputBuffer(iopb);

	BOOLEAN						synchronousIo = BooleanFlagOn(fileObject->Flags, FO_SYNCHRONOUS_IO);
	BOOLEAN						pagingIo      = BooleanFlagOn(iopb->IrpFlags, IRP_PAGING_IO);
	BOOLEAN						nonCachedIo   = BooleanFlagOn(iopb->IrpFlags,IRP_NOCACHE);

	ULONG						totalReadRequestLength;
	NTSTATUS					lastStatus;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

	BOOLEAN						endOfFile = FALSE;
	BOOLEAN						checkResult = FALSE;

	if (iopb->Parameters.Read.ReadBuffer) {

		outputBuffer = iopb->Parameters.Read.ReadBuffer;

	} else if (iopb->Parameters.Read.MdlAddress) {

		outputBuffer = MmGetSystemAddressForMdlSafe(iopb->Parameters.Read.MdlAddress, NormalPagePriority);
	}

	ASSERT( outputBuffer );

	read.ByteOffset	= iopb->Parameters.Read.ByteOffset;
	read.Key		= iopb->Parameters.Read.Key;
	read.Length		= iopb->Parameters.Read.Length;

	ASSERT(!(read.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
					&& read.ByteOffset.HighPart == -1));

	totalReadRequestLength = 0;

	if(!pagingIo && nonCachedIo && fileObject->SectionObjectPointer->DataSectionObject != NULL)
	{
		IO_STATUS_BLOCK	ioStatusBlock;

		CcFlushCache( fileObject->SectionObjectPointer,
					  &read.ByteOffset,
					  read.Length,
					  &ioStatusBlock );

		ASSERT(ioStatusBlock.Status == STATUS_SUCCESS);
	}


	do
	{
		ULONG						outputBufferLength;
		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		outputBufferLength = ((read.Length-totalReadRequestLength) <= Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
								? (read.Length-totalReadRequestLength) : Secondary->Thread.SessionContext.SecondaryMaxDataSize;

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, outputBufferLength );

		if(secondaryRequest == NULL)
		{
			ASSERT(LFS_REQUIRED);

			lastStatus = returnStatus = STATUS_INSUFFICIENT_RESOURCES;	
			break;
		}

		secondaryRequest->Ccb = ccb;
		secondaryRequest->OutputBuffer = (PUCHAR)outputBuffer+totalReadRequestLength;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		//
		//	Initialize NDFS request header
		//

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
			ndfsRequestHeader,
			NDFS_COMMAND_EXECUTE,
			Secondary,
			IRP_MJ_READ,
			0
			);


		//
		//	Initialize WinXP header
		//

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
		MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				iopb,
				iopb,
				ccb->PrimaryFileHandle
				);

		ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
		ndfsWinxpRequestHeader->Read.Key		= read.Key;
		ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart+totalReadRequestLength;


		returnStatus = AcquireLockAndTestCorruptError( Secondary,
																		FastMutexSet,
																		ccb,
																		Retry,
																		&secondaryRequest,
																		secondaryRequest->SessionId );
		if (returnStatus != STATUS_SUCCESS) {

			return returnStatus;
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);

		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec

#if DBG
		if(KeGetCurrentIrql() == APC_LEVEL) {
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("PrimaryAgentThreadProc: READ: IRLQ is APC! going to sleep.\n"));
		}
#endif
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

#if DBG
		if(KeGetCurrentIrql() == APC_LEVEL) {
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("PrimaryAgentThreadProc: READ: IRLQ is APC! woke up.\n"));
		}
#endif

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);
			secondaryRequest = NULL;
			lastStatus = returnStatus = STATUS_TIMEOUT;	
			
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;		
			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			lastStatus = returnStatus = secondaryRequest->ExecuteStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			secondaryRequest = NULL;
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		totalReadRequestLength += NTOHL(ndfsWinxpReplytHeader->Information32);
		lastStatus = NTOHL(ndfsWinxpReplytHeader->Status4);
		returnStatus = STATUS_SUCCESS;

		if(lastStatus != STATUS_SUCCESS)
		{
			ASSERT(NTOHL(ndfsWinxpReplytHeader->Information32) == 0);
			DereferenceSecondaryRequest(secondaryRequest);
			secondaryRequest = NULL;				
			
			KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

			*FastMutexSet = FALSE;
		
			break;
		}

		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("%d %d %d\n", 
					nonCachedIo, pagingIo, synchronousIo));

		ASSERT(ADD_ALIGN8(NTOHL(ndfsWinxpReplytHeader->Information32)) ==
			ADD_ALIGN8(NTOHL(secondaryRequest->NdfsReplyHeader.MessageSize4) -
			sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)));
		ASSERT(NTOHL(ndfsWinxpReplytHeader->Information32) <= secondaryRequest->OutputBufferLength);
		ASSERT(secondaryRequest->OutputBufferLength == 0 || secondaryRequest->OutputBuffer);

		if(NTOHL(ndfsWinxpReplytHeader->Information32) && secondaryRequest->OutputBuffer)
		{
			if(checkResult)
			{
				ULONG i;
				for(i=0; i<NTOHL(ndfsWinxpReplytHeader->Information32); i++)
				{
					if(secondaryRequest->OutputBuffer[i] != ((UINT8 *)(ndfsWinxpReplytHeader+1))[i]) {
						ASSERT(LFS_UNEXPECTED);
					}
				}
			}
			try {
				RtlCopyMemory(
					secondaryRequest->OutputBuffer,
					(UINT8 *)(ndfsWinxpReplytHeader+1),
					NTOHL(ndfsWinxpReplytHeader->Information32)
					);
			} except (EXCEPTION_EXECUTE_HANDLER) {
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
					("RedirectIrpMajorRead: Exception - output buffer is not valid\n"));
				DereferenceSecondaryRequest(secondaryRequest);
				secondaryRequest = NULL;
				totalReadRequestLength = read.Length; // Pretend that we read all the data.Buffer owner is already dead anyway..
				returnStatus = STATUS_SUCCESS;
				KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);
				*FastMutexSet = FALSE;
				break;
			}
		}


		DereferenceSecondaryRequest(secondaryRequest);
		secondaryRequest = NULL;

		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;

	} while(totalReadRequestLength < read.Length);


	if(checkResult)
	{
		ASSERT(Data->IoStatus.Information == totalReadRequestLength);
	}

	if(endOfFile == TRUE)
	{
		ASSERT(totalReadRequestLength == 0 && lastStatus == STATUS_END_OF_FILE);
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("%d %d read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
					nonCachedIo, pagingIo, read.ByteOffset.QuadPart, read.Length, totalReadRequestLength, lastStatus));

		//PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);
	}

	secondaryRequest = NULL;
	if(returnStatus != STATUS_SUCCESS)
		return returnStatus;


	if(totalReadRequestLength)
	{
		Data->IoStatus.Information = totalReadRequestLength;
		Data->IoStatus.Status = STATUS_SUCCESS;
	}
	else
	{
		Data->IoStatus.Information = 0;
		Data->IoStatus.Status = lastStatus;
	}


	if (Data->IoStatus.Status == STATUS_SUCCESS && synchronousIo && !pagingIo) 
	{
		fileObject->CurrentByteOffset.QuadPart = read.ByteOffset.QuadPart + totalReadRequestLength;
	}

	if(fileObject->SectionObjectPointer == NULL)
		fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

	returnStatus = STATUS_SUCCESS;

	if(Data->IoStatus.Status != STATUS_SUCCESS)
	{
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("%d %d read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
					nonCachedIo, pagingIo, read.ByteOffset.QuadPart, read.Length, totalReadRequestLength, lastStatus));

		//PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);
	}

	return STATUS_SUCCESS;
}


NTSTATUS
MiniRedirectIrpMajorQueryInformation (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	)
{
	NTSTATUS					returnStatus;
	//PIO_STACK_LOCATION		iopb = IoGetCurrentIrpStackLocation(Irp);
	PFLT_IO_PARAMETER_BLOCK		iopb = Data->Iopb;
	PFILE_OBJECT				fileObject = iopb->TargetFileObject;

	PLFS_FCB					fcb = fileObject->FsContext;
	PLFS_CCB				ccb = fileObject->FsContext2;

	struct QueryFile			queryFile;

	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PVOID						outputBuffer; // = MiniMapOutputBuffer(iopb);
	ULONG						outputBufferLength;

	BOOLEAN						checkResult = FALSE;

	PSECONDARY_REQUEST			secondaryRequest;	
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

	LARGE_INTEGER				timeOut;
	NTSTATUS					waitStatus;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	UINT32						returnedDataSize;


	outputBuffer = iopb->Parameters.QueryFileInformation.InfoBuffer;

	queryFile.FileInformationClass	= iopb->Parameters.QueryFileInformation.FileInformationClass;
	queryFile.Length				= iopb->Parameters.QueryFileInformation.Length;
	outputBufferLength				= queryFile.Length;


	ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
	secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, outputBufferLength );

	if(secondaryRequest == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	secondaryRequest->OutputBuffer = outputBuffer;
	secondaryRequest->OutputBufferLength = outputBufferLength;

	ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
	INITIALIZE_NDFS_REQUEST_HEADER(
			ndfsRequestHeader,
			NDFS_COMMAND_EXECUTE,
			Secondary,
			IRP_MJ_QUERY_INFORMATION,
			0
			);

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
	MINI_INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
		ndfsWinxpRequestHeader,
		iopb,
		iopb,
		ccb->PrimaryFileHandle
		);
	
	ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
	ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

	returnStatus = AcquireLockAndTestCorruptError( Secondary,
																	FastMutexSet,
																	ccb,
																	Retry,
																	&secondaryRequest,
																	secondaryRequest->SessionId );

	if (returnStatus != STATUS_SUCCESS) {

		return returnStatus;
	}

	secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
	QueueingSecondaryRequest(Secondary, secondaryRequest);

	timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
	waitStatus = KeWaitForSingleObject(
						&secondaryRequest->CompleteEvent,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

	KeClearEvent(&secondaryRequest->CompleteEvent);

	if(waitStatus != STATUS_SUCCESS) 
	{
		ASSERT(LFS_BUG);

		secondaryRequest = NULL;
		//ExReleaseFastMutex(&Secondary->FastMutex) ;
		KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

		*FastMutexSet = FALSE;

		return STATUS_TIMEOUT;
	}

	if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
	{
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
			("RedirectIrpMajorQueryInformation: secondaryRequest->ExecuteStatus = %x, Retry = %d\n",
				secondaryRequest->ExecuteStatus, *Retry));
		returnStatus = secondaryRequest->ExecuteStatus;	
		DereferenceSecondaryRequest(secondaryRequest);
		secondaryRequest = NULL;

		return returnStatus;
	}

	ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

	Data->IoStatus.Status	  = NTOHL(ndfsWinxpReplytHeader->Status4);
	Data->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32); 

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
			("RedirectIrpMajorQueryInformation: Data->IoStatus.Status = %d, Data->IoStatus.Information = %d\n",
				Data->IoStatus.Status, Data->IoStatus.Information));

	returnedDataSize 
		= NTOHL(secondaryRequest->NdfsReplyHeader.MessageSize4) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

	if(checkResult == TRUE)
	{
		if(queryFile.FileInformationClass == FileBasicInformation)
		{
			PFILE_BASIC_INFORMATION	checked, checking;

			checked = (PFILE_BASIC_INFORMATION)outputBuffer;
			checking = (PFILE_BASIC_INFORMATION)(ndfsWinxpReplytHeader+1);

			ASSERT(checked->CreationTime.QuadPart == checking->CreationTime.QuadPart);
			//ASSERT(checked->LastAccessTime.QuadPart == checking->LastAccessTime.QuadPart);
			ASSERT(checked->LastWriteTime.QuadPart == checking->LastWriteTime.QuadPart);
			ASSERT(checked->ChangeTime.QuadPart == checking->ChangeTime.QuadPart);
			ASSERT(checked->FileAttributes == checking->FileAttributes);
		}
		else if(queryFile.FileInformationClass == FileStandardInformation)
		{
			PFILE_STANDARD_INFORMATION	checked, checking;

			checked = (PFILE_STANDARD_INFORMATION)outputBuffer;
			checking = (PFILE_STANDARD_INFORMATION)(ndfsWinxpReplytHeader+1);

			ASSERT(checked->AllocationSize.QuadPart == checking->AllocationSize.QuadPart);
			ASSERT(checked->EndOfFile.QuadPart == checking->EndOfFile.QuadPart);
			ASSERT(checked->NumberOfLinks == checking->NumberOfLinks);
			ASSERT(checked->DeletePending == checked->DeletePending);
			ASSERT(checked->Directory == checking->Directory);
		}
		else if(queryFile.FileInformationClass == FileStreamInformation)
		{
			PFILE_STREAM_INFORMATION	checked, checking;

			checked = (PFILE_STREAM_INFORMATION)outputBuffer;
			checking = (PFILE_STREAM_INFORMATION)(ndfsWinxpReplytHeader+1);

			ASSERT(checked->NextEntryOffset == checking->NextEntryOffset);
			ASSERT(checked->StreamNameLength == checking->StreamNameLength);
			ASSERT(checked->StreamSize.QuadPart == checking->StreamSize.QuadPart);
			ASSERT(checked->StreamAllocationSize.QuadPart == checking->StreamAllocationSize.QuadPart);
			RtlEqualMemory(
				checked->StreamName,
				checking->StreamName,
				checked->StreamNameLength
				);
		}
	}

	if(returnedDataSize)
	{
		ASSERT(returnedDataSize <= ADD_ALIGN8(queryFile.Length));
		ASSERT(outputBuffer);
		
		RtlCopyMemory(
			outputBuffer,
			(UINT8 *)(ndfsWinxpReplytHeader+1),
			(returnedDataSize < queryFile.Length) ? returnedDataSize : queryFile.Length
			);
	}
		
	DereferenceSecondaryRequest(secondaryRequest);


	//ExReleaseFastMutex(&Secondary->FastMutex);
	KeReleaseSemaphore(&Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE);

	*FastMutexSet = FALSE;
	*Retry = FALSE;

	return STATUS_SUCCESS;
}

#endif
