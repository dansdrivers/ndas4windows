h <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_INFORMATION,
								outputBufferLength
								);

		if(secondaryRequest == NULL)
		{
			returnResult = FALSE;
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

		ndfsWinxpRequestHeader->IrpTag				= (_U32)LfsFileIo;
		ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_QUERY_INFORMATION;
		ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
		ndfsWinxpRequestHeader->FileHandle			= fileExt->PrimaryFileHandle;
		ndfsWinxpRequestHeader->IrpFlags			= 0;
		ndfsWinxpRequestHeader->IrpSpFlags			= 0;

		ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

		{														
			KIRQL oldIrql;										
			NTSTATUS	waitStatus;
															
			//ExAcquireFastMutex(&Secondary->FastMutex);
			waitStatus = KeWaitForSingleObject( 
								&Secondary->Semaphore,
								Executive,
								KernelMode,
								FALSE,
								NULL
								);
			ASSERT(waitStatus == STATUS_SUCCESS);
															
			//ExAcquireFastMutex(&Secondary->FastMutex);																											
			//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
																											
			if(Secondary->Flags & SECONDARY_ERROR
				|| Secondary->SessionId != secondaryRequest->SessionId
				|| fileExt->Corrupted == TRUE)
			{						
				DereferenceSecondaryRequest(			
					secondaryRequest	
					);				
				secondaryRequest = NULL;									
																			
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);		

				//ExReleaseFastMutex(&Secondary->FastMutex);				
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
		
				return FALSE;
			}											
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
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
			returnResult = FALSE;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
	
			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;		
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			returnResult = FALSE;	
			break;
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		queryStandardInfo->IoStatus->Status		= ndfsWinxpReplytHeader->Status;
		queryStandardInfo->IoStatus->Information = ndfsWinxpReplytHeader->Information; 

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: Irp->IoStatus.Status = %d, Irp->IoStatus.Information = %d\n",
					queryStandardInfo->IoStatus->Status, queryStandardInfo->IoStatus->Information));

		returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

		if(returnedDataSize)
		{
			ASSERT(queryStandardInfo->IoStatus->Status == STATUS_SUCCESS 
				|| queryStandardInfo->IoStatus->Status == STATUS_BUFFER_OVERFLOW);
		
			if(queryStandardInfo->IoStatus->Status == STATUS_SUCCESS)
				ASSERT(ADD_ALIGN8(returnedDataSize) == ADD_ALIGN8(ndfsWinxpReplytHeader->Information));
				
			ASSERT(queryStandardInfo->IoStatus->Information <= outputBufferLength);
			ASSERT(outputBuffer);
		
			RtlCopyMemory(
				outputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				queryStandardInfo->IoStatus->Information
				);
		}
		
		DereferenceSecondaryRequest(
			secondaryRequest
			);
		
		secondaryRequest = NULL;
		returnResult = TRUE;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		break;
	}
	default:

		returnResult = FALSE;
		break;
	}

	return returnResult;
}


BOOLEAN
RecoverySession(
	IN  PSECONDARY	Secondary
	)
{
	BOOLEAN					result;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	LIST_ENTRY				tempRequestQueue;
	PLIST_ENTRY				secondaryRequestEntry;
	_U16					mid;
	_U16					previousRequestPerSession = Secondary->Thread.SessionContext.RequestsPerSession;
	_U16					queuedRequestCount = 0;

	ULONG					reconnectionTry;
    PLIST_ENTRY				fileExtlistEntry;

	KIRQL					oldIrql;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
		("RecoverSession Called Secondary = %p\n", Secondary));

	ASSERT(Secondary->ThreadHandle);

	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	InitializeListHead(&tempRequestQueue);

	for(mid=0; mid < Secondary->Thread.SessionContext.RequestsPerSession; mid++)
	{
		if(Secondary->Thread.ProcessingSecondaryRequest[mid] != NULL)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,			
				("RecoverSession: insert to tempRequestQueue\n"));
		
			InsertHea