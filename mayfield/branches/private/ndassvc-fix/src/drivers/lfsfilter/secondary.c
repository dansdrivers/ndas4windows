_REQUEST_PER_SESSION; mid++)
				{
					if(Secondary->Thread.ProcessingSecondaryRequest[mid] == NULL)
						break;
				}
				
				RtlZeroMemory(
					&Secondary->Thread.SessionContext,
					sizeof(Secondary->Thread.SessionContext)
					);

				Secondary->Thread.TdiReceiveContext.Irp = NULL;
				KeInitializeEvent(&Secondary->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE);

				Secondary->Thread.SessionContext.PrimaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
				Secondary->Thread.SessionContext.SecondaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
			}
			else
			{
				ASSERT(LFS_BUG);
				return FALSE;
			}
		}

		if(Secondary->LfsDeviceExt->PurgeVolumeSafe == TRUE)
		{
//			if(reconnectionTry >= 2) // first and second lookup primary address
			{
				LfsDeviceExt_SecondaryToPrimary(Secondary->LfsDeviceExt);
			}
		}

		LfsTable_CleanCachePrimaryAddress(
						GlobalLfs.LfsTable,
						&netdiskPartitionInfo,
						&Secondary->PrimaryAddress
						);

		tableStatus = LfsTable_QueryPrimaryAddress(
							GlobalLfs.LfsTable,
							&netdiskPartitionInfo,
							&Secondary->PrimaryAddress
							);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("RecoverSession: LfsTable_QueryPrimaryAddress tableStatus = %X\n", tableStatus));

		if(tableStatus == STATUS_SUCCESS)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("LFS: RecoverySession: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
					Secondary->PrimaryAddress.Node[0],
					Secondary->PrimaryAddress.Node[1],
					Secondary->PrimaryAddress.Node[2],
					Secondary->PrimaryAddress.Node[3],
					Secondary->PrimaryAddress.Node[4],
					Secondary->PrimaryAddress.Node[5],
					NTOHS(Secondary->PrimaryAddress.Port)
					));
			if(Lfs_IsLocalAddress(&Secondary->PrimaryAddress)
				&& Secondary->LfsDeviceExt->SecondaryState == SECONDARY_STATE) // not yet purged
			{
				// another volume changed to primary and this volume didn't purge yet 
				result = FALSE;

				if(Secondary->LfsDeviceExt->PurgeVolumeSafe != TRUE)
				{
					ASSERT(LFS_REQUIRED);
					break;
				}
			
				continue;
			}
		} 
		else
		{
			result = FALSE;
			continue;
		}	
		
		KeInitializeEvent(&Secondary->Thread.ReadyEvent, NotificationEvent, FALSE) ;
		KeInitializeEvent(&Secondary->Thread.RequestEvent, NotificationEvent, FALSE) ;

		InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

		waitStatus = PsCreateSystemThread(
						&Secondary->ThreadHandle,
						THREAD_ALL_ACCESS,
						&objectAttributes,
						NULL,
						NULL,
						SecondaryThreadProc,
						Secondary
		);

		if(!NT_SUCCESS(waitStatus)) 
		{
			ASSERT(LFS_UNEXPECTED);
			result = FALSE;
			break;
		}

		waitStatus = ObReferenceObjectByHandle(
							Secondary->ThreadHandle,
							FILE_READ_DATA,
							NULL,
							KernelMode,
							&Secondary->ThreadObject,
							NULL
							);

		if(!NT_SUCCESS(waitStatus)) 
		{
			ASSERT(LFS_INSUFFICIENT_RESOURCES);
			result = FALSE;
			break;
		}

		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
						&Secondary->Thread.ReadyEvent,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);
			result = FALSE;
			break;
		}

		KeClearEvent(&Secondary->Thread.ReadyEvent);

		InterlockedIncrement(&Secondary->SessionId);	

		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
		if(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_UNCONNECTED)) 
		{
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

			if(Secondary->Thread.ConnectionStatus == STATUS_FILE_CORRUPT_ERROR)
			{
				result = FALSE;
				break;
			}

			continue;
		} 
		else 
		{
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
		}

		ASSERT(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_CONNECTED) && !BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_ERROR));

		result = TRUE;
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("RecoverySession Success Secondary = %p\n", Secondary));

		break;
	}

	if(result == FALSE)
	{
		//
		//	if failed to recover the connection to a primary.
		//
		while(!IsListEmpty(&tempRequestQueue))
		{	
			secondaryRequestEntry = 
					RemoveHeadList(
								&tempRequestQueue
								);
		
			ASSERT(LFS_BUG);
			ExInterlockedInsertTailList(
				&Secondary->Thread.RequestQueue, 
				secondaryRequestEntry,
				&Secondary->Thread.RequestQSpinLock
				);
		}
		return result;
	}
	
	//
	//	if recovering the connection to a primary is successful,
	//	try to reopen files to continue I/O.
	//

    for (fileExtlistEntry = Secondary->FileExtQueue.Blink;
         fileExtlistEntry != &Secondary->FileExtQueue;
         fileExtlistEntry = fileExtlistEntry->Blink) 
	{
		PFILE_EXTENTION				fileExt;
		ULONG						disposition;
		
		ULONG						dataSize;
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		KIRQL						oldIrql;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		fileExt = CONTAINING_RECORD (fileExtlistEntry, FILE_EXTENTION, ListEntry);
		fileExt->Fcb->FileRecordSegmentHeaderAvail = FALSE;

		if(fileExt->CreateContext.RelatedFileHandle != 0)
		{
			PFILE_EXTENTION	relatedFileExt;

			if(fileExt->FileObject->RelatedFileObject == NULL)
			{
				ASSERT(LFS_UNEXPECTED);
				result = FALSE;	
				break;
			}

			if(fileExt->RelatedFileObjectClosed == FALSE)
			{
				relatedFileExt = Secondary_LookUpFileExtension(Secondary, fileExt->FileObject->RelatedFileObject);
				ASSERT(relatedFileExt != NULL && relatedFileExt->LfsMark == LFS_MARK);				
				ASSERT(relatedFileExt->SessionId == Secondary->SessionId); // must be already Updated

				if(relatedFileExt->Corrupted == TRUE)
				{
					fileExt->SessionId = Secondary->SessionId;
					fileExt->Corrupted = TRUE;
					//InterlockedIncrement(&fileExt->Fcb->CleanCountByCorruption);
				
					continue;
				}

				fileExt->CreateContext.RelatedFileHandle = relatedFileExt->PrimaryFileHandle;
			}
			else // relateFileObject is already closed;
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					("RecoverSession: relateFileObject is already closed\n"));
				
				fileExt->CreateContext.RelatedFileHandle = 0;
				fileExt->CreateContext.FileNameLength = fileExt->Fcb->FullFileName.Length;

				if(fileExt->Fcb->FullFileName.Length)
				{
					RtlCopyMemory(
						fileExt->Buffer + fileExt->CreateContext.EaLength,
						fileExt->Fcb->FullFileName.Buffer,
						fileExt->Fcb->FullFileName.Length
						);
				}
			}	
		}
				
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("RecoverSession: fileExt->Fcb->FullFileName = %wZ fileExt->FileObject->CurrentByteOffset.QuadPart = %I64d\n", 
					&fileExt->Fcb->FullFileName, fileExt->FileObject->CurrentByteOffset.QuadPart));

		if(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS)
		{
			dataSize = ((fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength) > Secondary->Thread.SessionContext.BytesPerFileRecordSegment)
						? (fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength) 
						  : Secondary->Thread.SessionContext.BytesPerFileRecordSegment;
		}
		else
			dataSize = fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength;

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_CREATE,
								dataSize
							);

		if(secondaryRequest == NULL)
		{
			result = FALSE;	
			break;
		}

		secondaryRequest->OutputBuffer = NULL;
		secondaryRequest->OutputBufferLength = 0;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	
		RtlCopyMemory(ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol));
		ndfsRequestHeader->Command	= NDFS_COMMAND_EXECUTE;
		ndfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;
		ndfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;
		ndfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize
			= (Secondary->Thread.SessionContext.MessageSecurity == 1)
				? sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + ADD_ALIGN8(fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength)
					: sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength;

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

		ndfsWinxpRequestHeader->IrpTag   = (_U32)fileExt;
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle = 0;

		ndfsWinxpRequestHeader->IrpFlags   = fileExt->IrpFlags;
		ndfsWinxpRequestHeader->IrpSpFlags = fileExt->IrpSpFlags;

		RtlCopyMemory(
			&ndfsWinxpRequestHeader->Create,
			&fileExt->CreateContext,
			sizeof(WINXP_REQUEST_CREATE)
			);
		
		disposition = FILE_OPEN_IF;
		ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
		ndfsWinxpRequestHeader->Create.Options |= (disposition << 24);

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

		RtlCopyMemory(
			ndfsWinxpRequestData,
			fileExt->Buffer,
			fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength
			);

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
			result = FALSE;
			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(secondaryRequest);
			
			result = FALSE;		
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
		if(ndfsWinxpReplytHeader->Status == STATUS_SUCCESS)
		{
	 		fileExt->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
		}
		else
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,			
				("RecoverSession: fileExt->Fcb->FullFileName = %wZ Corrupted\n", &fileExt->Fcb->FullFileName));

			fileExt->Corrupted = TRUE;
		}

		fileExt->SessionId = Secondary->SessionId;

		DereferenceSecondaryRequest(secondaryRequest);
	}

	while(!IsListEmpty(&tempRequestQueue))
	{	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			("RecoverSession: extract from tempRequestQueue\n"));

		secondaryRequestEntry = RemoveHeadList(&tempRequestQueue);
		
		ExInterlockedInsertTailList(
			&Secondary->Thread.RequestQueue, 
			secondaryRequestEntry,
			&Secondary->Thread.RequestQSpinLock
			);
	}

	ASSERT(Secondary->SemaphoreConsumeRequiredCount == 0);

	if(result == TRUE)
	{
		if(previousRequestPerSession < Secondary->Thread.SessionContext.RequestsPerSession)
		{
			while(previousRequestPerSession < Secondary->Thread.SessionContext.RequestsPerSession)
			{
				Secondary->SemaphoreReturnCount++;
				previousRequestPerSession++;
			}
		}
		else if(previousRequestPerSession > Secondary->Thread.SessionContext.RequestsPerSession)
		{
			_U16	consumeRequireCount = previousRequestPerSession - Secondary->Thread.SessionContext.RequestsPerSession;
			
			if(consumeRequireCount <= Secondary->SemaphoreReturnCount)
			{
				Secondary->SemaphoreReturnCount -= consumeRequireCount;
			}
			else
			{
				Secondary->SemaphoreConsumeRequiredCount = consumeRequireCount - Secondary->SemaphoreReturnCount;
				Secondary->SemaphoreReturnCount = 0;
			}
		}
			
		ASSERT(queuedRequestCount + 1 + Secondary->SemaphoreReturnCount - Secondary->SemaphoreConsumeRequiredCount 
				== Secondary->Thread.SessionContext.RequestsPerSession);

		KeSetEvent(&Secondary->Thread.RequestEvent, IO_DISK_INCREMENT, FALSE);
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
		("RecoverySession Completed. Secondary = %p, result = %d\n", Secondary, result));

	return result;
}


PSECONDARY_REQUEST
AllocSecondaryRequest(
	IN	PSECONDARY	Secondary,
	IN 	UINT32	MessageSize,
	IN	BOOLEAN	Synchronous
) 
{
	PSECONDARY_REQUEST	secondaryRequest;
	ULONG				requestSize ;
	KIRQL				oldIrql;
	

	requestSize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + MessageSize + MEMORY_CHECK_SIZE;

	secondaryRequest = ExAllocatePoolWithTag(
							NonPagedPool,
							requestSize,
							SECONDARY_MESSAGE_TAG
							);

	if(secondaryRequest == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL ;
	}

	RtlZeroMemory(secondaryRequest, requestSize);

#if DBG
	{	
		UCHAR	i;
		ULONG	memorySize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + MessageSize;
		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			*((_U8*)secondaryRequest + memorySize + i) = i;
	}
#endif

	secondaryRequest->ReferenceCount = 1;
	InitializeListHead(&secondaryRequest->ListEntry);

	secondaryRequest->Synchronous = Synchronous;
	KeInitializeEvent(&secondaryRequest->CompleteEvent, NotificationEvent, FALSE) ;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	secondaryRequest->SessionId = Secondary->SessionId;
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	secondaryRequest->NdfsMessageLength = MessageSize;
	
#if DBG
	InterlockedIncrement(&LfsObjectCounts.SecondaryRequestCount);
#endif

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
			("AllocSecondaryRequest: secondaryRequest = %p, SecondaryRequestCount = %d\n", secondaryRequest, LfsObjectCounts.SecondaryRequestCount)) ;

	return secondaryRequest;
}


VOID
ReferenceSencondaryRequest(
	IN	PSECONDARY_REQUEST	SecondaryRequest
	) 
{
	LONG	result ;

	result = InterlockedIncrement(&SecondaryRequest->ReferenceCount) ;

	ASSERT( result > 0) ;
}


VOID
DereferenceSecondaryRequest(
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	LONG	result ;


#if DBG
	{	
		UCHAR	i;
		ULONG	memorySize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + SecondaryRequest->NdfsMessageLength;
		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			if(*((_U8*)SecondaryRequest + memorySize + i) != i)
			{
				ASSERT(LFS_BUG);
				break;
			}
	}
#endif

	result = InterlockedDecrement(&SecondaryRequest->ReferenceCount) ;

	ASSERT( result >= 0) ;

	if(0 == result)
	{
		ASSERT(SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink);
		ExFreePool(SecondaryRequest) ;

#if DBG
		InterlockedDecrement(&LfsObjectCounts.SecondaryRequestCount);
#endif

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("FreeSecondaryRequest: SecondaryRequest = %p, SecondaryRequestCount = %d\n", SecondaryRequest, LfsObjectCounts.SecondaryRequestCount)) ;
	}
}


//
//	make sure to enter secondary critical section before calling.
//
FORCEINLINE
VOID
QueueingSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	)
{


	ASSERT(SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink);

	ExInterlockedInsertTailList(
		&Secondary->Thread.RequestQueue,
		&SecondaryRequest->ListEntry,
		&Secondary->Thread.RequestQSpinLock
		);

	KeSetEvent(&Secondary->Thread.RequestEvent, IO_DISK_INCREMENT, FALSE) ;
}


PLFS_FCB
AllocateFcb (
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN  ULONG			BufferLength
    )
{
    PLFS_FCB fcb;


	UNREFERENCED_PARAMETER(Secondary);
	
 
    fcb = FsRtlAllocatePoolWithTag( 
				NonPagedPool,
                sizeof(LFS_FCB) - sizeof(CHAR) + BufferLength,
                LFS_FCB_TAG 
				);
	if(fcb == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
			("AllocateFcb: failed to allocate fcb\n"));
		return NULL;
	}
	
	RtlZeroMemory(fcb, sizeof(LFS_FCB) - sizeof(CHAR) + BufferLength);

	//
	//	set NodeTypeCode to avoid BSOD when calling XxxxAcquireFileForCcFlush()
	//
	if(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT) 
	{
			fcb->Header.NodeTypeCode = FAT_NTC_FCB ;
	}

    fcb->NonPaged = LfsAllocateNonPagedFcb();
	if(fcb->NonPaged == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
			("AllocateFcb: failed to allocate fcb->NonPaged\n"));
		ExFreePool(fcb);
		return NULL;
	}

    RtlZeroMemory(fcb->NonPaged, sizeof(NON_PAGED_FCB));

	fcb->Header.IsFastIoPossible = FastIoIsPossible ;
    fcb->Header.Resource = LfsAllocateResource();
	fcb->Header.PagingIoResource = NULL; //fcb->Header.Resource;
	if(fcb->Header.Resource == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
			("AllocateFcb: failed to allocate fcb->Header.Resource\n"));
		ExFreePool(fcb->NonPaged);
		ExFreePool(fcb);
		return NULL;
	}

	ExInitializeFastMutex(&fcb->NonPaged->AdvancedFcbHeaderMutex);
#if WINVER >= 0x0501
	if(IS_WINDOWSXP_OR_LATER())
	{
    FsRtlSetupAdvancedHeader( 
					&fcb->Header, 
                    &fcb->NonPaged->AdvancedFcbHeaderMutex);

	}
#endif
    FsRtlInitializeFileLock(&fcb->FileLock, NULL, NULL);

	fcb->ReferenceCount = 1;
	InitializeListHead(&fcb->ListEntry);

    RtlInitEmptyUnicodeString( 
				&fcb->FullFileName,
                fcb->FullFileNameBuffer,
                sizeof(fcb->FullFileNameBuffer) 
				);

	RtlCopyUnicodeString(
		&fcb->FullFileName,
		FullFileName
		);

    RtlInitEmptyUnicodeString( 
				&fcb->CaseInSensitiveFullFileName,
                fcb->CaseInSensitiveFullFileNameBuffer,
                sizeof(fcb->CaseInSensitiveFullFileNameBuffer) 
				);

	RtlDowncaseUnicodeString(
		&fcb->CaseInSensitiveFullFileName,
		&fcb->FullFileName,
		FALSE);

	if(FullFileName->Length)
	if(FullFileName->Buffer[0] != L'\\')
		ASSERT(LFS_BUG);
	
#if DBG
	InterlockedIncrement(&LfsObjectCounts.FcbCount);
#endif

	return fcb;
}


VOID
Secondary_DereferenceFcb (
	IN	PLFS_FCB	Fcb
   )
{
	LONG		result;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
			("Secondary_DereferenceFcb: Fcb->OpenCount = %d, Fcb->UncleanCount = %d\n", Fcb->OpenCount, Fcb->UncleanCount));

	ASSERT(Fcb->OpenCount >= Fcb->UncleanCount);
	result = InterlockedDecrement(&Fcb->ReferenceCount);

	ASSERT( result >= 0);

	if(0 == result)
	{
		ASSERT(Fcb->ListEntry.Flink == Fcb->ListEntry.Blink);
		ASSERT(Fcb->OpenCount == 0);
		
	    LfsFreeResource(Fcb->Header.Resource);
		LfsFreeNonPagedFcb(Fcb->NonPaged);
	    ExFreePool(Fcb);	
#if DBG
		InterlockedDecrement(&LfsObjectCounts.FcbCount);
#endif
	}
}


PLFS_FCB
Secondary_LookUpFcb(
	IN PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
    IN BOOLEAN			CaseInSensitive
	)
{
	PLFS_FCB		fcb = NULL;
    PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	UNICODE_STRING	caseInSensitiveFullFileName;
	WCHAR			caseInSensitiveFullFileNameBuffer[NDFS_MAX_PATH];
	NTSTATUS		downcaseStatus;


	ASSERT(FullFileName->Length <= NDFS_MAX_PATH*sizeof(WCHAR));

	if(CaseInSensitive == TRUE)
	{
		//ASSERT(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS);

		RtlInitEmptyUnicodeString( 
				&caseInSensitiveFullFileName,
                caseInSensitiveFullFileNameBuffer,
                sizeof(caseInSensitiveFullFileNameBuffer) 
				);
		downcaseStatus = RtlDowncaseUnicodeString(
							&caseInSensitiveFullFileName,
							FullFileName,
							FALSE
							);
	
		if(downcaseStatus != STATUS_SUCCESS)
		{
			ASSERT(LFS_UNEXPECTED);
			return NULL;
		}
	}

	KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);

    for (listEntry = Secondary->FcbQueue.Flink;
         listEntry != &Secondary->FcbQueue;
         listEntry = listEntry->Flink) 
	{
		fcb = CONTAINING_RECORD (listEntry, LFS_FCB, ListEntry);
		if(fcb->FullFileName.Length != FullFileName->Length)
		{
			fcb = NULL;
			continue;
		}
		if(CaseInSensitive == TRUE)
		{
			if(RtlEqualMemory(
				fcb->CaseInSensitiveFullFileName.Buffer, 
				caseInSensitiveFullFileName.Buffer, 
				fcb->CaseInSensitiveFullFileName.Length))
		{
			InterlockedIncrement(&fcb->ReferenceCount);
			break;
		}
		}
		else
		{
			if(RtlEqualMemory(
				fcb->FullFileName.Buffer, 
				FullFileName->Buffer,
				fcb->FullFileName.Length))
			{
				InterlockedIncrement(&fcb->ReferenceCount);
				break;
			}
		}

		fcb = NULL;
	}

	KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);

	return fcb;
}


PFILE_EXTENTION
AllocateFileExt(
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	) 
{
	PFILE_EXTENTION	fileExt ;
	KIRQL			oldIrql;


	fileExt = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength + MEMORY_CHECK_SIZE,
						FILE_EXT_TAG
						);
	
	if (fileExt == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(
		fileExt,
        sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength
		);
	
#if DBG
	{	
		UCHAR	i;
		ULONG	memorySize = sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength;
		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			*((_U8*)fileExt + memorySize + i) = i;
	}
#endif

	fileExt->LfsMark    = LFS_MARK;
	fileExt->Secondary	= Secondary;
	fileExt->FileObject	= FileObject;

	InitializeListHead(&fileExt->ListEntry) ;

	fileExt->BufferLength = BufferLength;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	fileExt->SessionId = Secondary->SessionId;
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

#if DBG
	InterlockedIncrement(&LfsObjectCounts.FileExtCount);
#endif

	InterlockedIncrement(&Secondary->FileExtCount);

	return fileExt;
}


VOID
FreeFileExt(
	IN  PSECONDARY		Secondary,
	IN  PFILE_EXTENTION	FileExt
	)
{
	PLIST_ENTRY		listEntry;


	ASSERT(FileExt->ListEntry.Flink == FileExt->ListEntry.Blink);

#if DBG
	{	
		UCHAR	i;
		ULONG	memorySize = sizeof(FILE_EXTENTION) - sizeof(_U8) + FileExt->BufferLength;

		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			if(*((_U8*)FileExt + memorySize + i) != i)
			{
				ASSERT(LFS_BUG);
				break;
			}
	}
#endif

	ExAcquireFastMutex(&Secondary->FileExtQMutex);

    for (listEntry = Secondary->FileExtQueue.Flink;
         listEntry != &Secondary->FileExtQueue;
         listEntry = listEntry->Flink) 
	{
		PFILE_EXTENTION	childFileExt;
		
		childFileExt = CONTAINING_RECORD (listEntry, FILE_EXTENTION, ListEntry);
        
		if(childFileExt->CreateContext.RelatedFileHandle == FileExt->PrimaryFileHandle)
			childFileExt->RelatedFileObjectClosed = TRUE;
	}

    ExReleaseFastMutex(&Secondary->FileExtQMutex);

	InterlockedDecrement(&Secondary->FileExtCount);

	ExFreePoolWithTag(
		FileExt,
		FILE_EXT_TAG
		);
#if DBG
	InterlockedDecrement(&LfsObjectCounts.FileExtCount);
#endif
}

PFILE_EXTENTION
Secondary_LookUpFileExtensionByHandle(
	IN PSECONDARY	Secondary,
	IN HANDLE		FileHandle
	)
{
	NTSTATUS		referenceStatus;
	PFILE_OBJECT	fileObject = NULL;


	referenceStatus = ObReferenceObjectByHandle(
									FileHandle,
									FILE_READ_DATA,
									0,
									KernelMode,
									&fileObject,
									NULL
									);

    if(referenceStatus != STATUS_SUCCESS)
	{
		return NULL;
	}
	
	ObDereferenceObject(fileObject);

	return Secondary_LookUpFileExtension(Secondary, fileObject);
}

	
PFILE_EXTENTION
Secondary_LookUpFileExtension(
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
	)
{
	PFILE_EXTENTION	fileExt = NULL;
    PLIST_ENTRY		listEntry;

	
    ExAcquireFastMutex(&Secondary->FileExtQMutex);

    for (listEntry = Secondary->FileExtQueue.Flink;
         listEntry != &Secondary->FileExtQueue;
         listEntry = listEntry->Flink) 
	{
		 fileExt = CONTAINING_RECORD (listEntry, FILE_EXTENTION, ListEntry);
         if(fileExt->FileObject == FileObject)
			break;

		fileExt = NULL;
	}

    ExReleaseFastMutex(&Secondary->FileExtQMutex);
he IRP to the primary host.
		//
		redirectStatus = RedirectIrp(Secondary, Irp, &fastMutexSet, &retry);

#if DBG

		if(LfsObjectCounts.RedirectIrpCount > 3)
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("LfsObjectCounts.RedirectIrpCount = %d\n", LfsObjectCounts.RedirectIrpCount));

		InterlockedDecrement(&LfsObjectCounts.RedirectIrpCount);

		if(redirectStatus != STATUS_SUCCESS)
		{
			PrintIrp(LFS_DEBUG_SECONDARY_ERROR, "RedirectIrp Error", Secondary->LfsDeviceExt, Irp);
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("PurgeVolumeSafe = %d, retry = %d\n", Secondary->LfsDeviceExt->PurgeVolumeSafe, retry));
		}

#endif

		if(retry == TRUE)
			continue;

		if(redirectStatus == STATUS_ABANDONED)
		{
			KIRQL	oldIrql;

			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("redirectStatus == STATUS_ABANDONED\n"));

			ASSERT(fastMutexSet == TRUE);

			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
			
			if(Secondary->SemaphoreConsumeRequiredCount == 0)
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			else
				Secondary->SemaphoreConsumeRequiredCount --;
			
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

			fastMutexSet = FALSE;

			continue;
		}

		if(redirectStatus == STATUS_WORKING_SET_LIMIT_RANGE)
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("redirectStatus == STATUS_WORKING_SET_LIMIT_RANGE\n"));

			ASSERT(fastMutexSet == TRUE);
			//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			fastMutexSet = FALSE;

			continue;
		}

		if(redirectStatus == STATUS_SUCCESS)
		{
			ASSERT(fastMutexSet == FALSE);

			*NtStatus = Irp->IoStatus.Status;
			if(*NtStatus == STATUS_PENDING)
			{
				ASSERT(LFS_BUG);
				IoMarkIrpPending(Irp);
			}
			else 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						( "completed. Stat=%08lx Info=%d\n", 
							Irp->IoStatus.Status, Irp->IoStatus.Information));
				PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrp", Secondary->LfsDeviceExt, Irp);
				if(!(
					*NtStatus == STATUS_SUCCESS
					|| irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_NO_SUCH_FILE && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_OBJECT_NAME_NOT_FOUND && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_OBJECT_PATH_NOT_FOUND && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL && *NtStatus == STATUS_NO_MORE_FILES && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL && *NtStatus == STATUS_NO_SUCH_FILE && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_READ && *NtStatus == STATUS_END_OF_FILE && Irp->IoStatus.Information == 0
					))
				{
					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
							( "completed. Stat=%08lx Info=%d\t", 
								Irp->IoStatus.Status, Irp->IoStatus.Information));
					PrintIrp(LFS_DEBUG_SECONDARY_INFO, NULL, Secondary->LfsDeviceExt, Irp);
				}
			}
			break;
		}
		else if(redirectStatus == STATUS_TIMEOUT)
		{
			*NtStatus = Irp->IoStatus.Status = STATUS_PENDING;
			IoMarkIrpPending(Irp);

			break;
		}
		else
		{
			PLIST_ENTRY		secondaryRequestEntry;
	
			ASSERT(fastMutexSet == TRUE);
			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;

			ASSERT(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED));

			if(Secondary->LfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE
				|| BooleanFlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL)
				|| GlobalLfs.ShutdownOccured == TRUE)
			{
				SetFlag(Secondary->Flags, SECONDARY_ERROR);
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
				
				*NtStatus = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
				Irp->IoStatus.Information = 0;
	
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				fastMutexSet = FALSE;
				break;
			}

			if(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED))
			{
				BOOLEAN			recoveryResult;


				ASSERT(Secondary->SemaphoreReturnCount == 0);

				while(1)
				{
					NTSTATUS		waitStatus;
					LARGE_INTEGER	timeOut;
								
					timeOut.QuadPart = HZ >> 2; 

					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
					waitStatus = KeWaitForSingleObject( 
									&Secondary->Semaphore,
									Executive,
									KernelMode,
									FALSE,
									&timeOut
									);
					KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);

					if(waitStatus == STATUS_TIMEOUT)
						break;

					if(waitStatus == STATUS_SUCCESS)
					{
						Secondary->SemaphoreReturnCount++;
					}
					else
					{
						ASSERT(LFS_UNEXPECTED);
						break;
					}	
				}

				ASSERT(!BooleanFlagOn(Secondary->Flags, SECONDARY_RECONNECTING));
				SetFlag(Secondary->Flags, SECONDARY_RECONNECTING);

				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
				recoveryResult = RecoverySession(Secondary);
				KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);


				while(Secondary->SemaphoreReturnCount)
				{
					KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
					Secondary->SemaphoreReturnCount--;
				}

				if(recoveryResult == TRUE)
				{						
					ClearFlag(Secondary->Flags, SECONDARY_RECONNECTING);

					if(Secondary->SemaphoreConsumeRequiredCount == 0)
						KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
					else
						Secondary->SemaphoreConsumeRequiredCount--;
					
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
	
					fastMutexSet = FALSE;
					continue;
				}
			}

			SetFlag(Secondary->Flags, SECONDARY_ERROR);
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;

			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			fastMutexSet = FALSE;

			*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
			Irp->IoStatus.Information = 0;

			while(secondaryRequestEntry = 
							ExInterlockedRemoveHeadList(
										&Secondary->Thread.RequestQueue,
										&Secondary->Thread.RequestQSpinLock
										))
			{
				PSECONDARY_REQUEST	secondaryRequest;

				InitializeListHead(secondaryRequestEntry);

				secondaryRequest = CONTAINING_RECORD(
										secondaryRequestEntry,
										SECONDARY_REQUEST,
										ListEntry
										) ;

				ASSERT(secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT);
				secondaryRequest->ExecuteStatus = STATUS_ABANDONED;

				ASSERT(secondaryRequest->Synchronous == TRUE);
				KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);				
			}

			break;
		}

		break;
	}

	if(*NtStatus != STATUS_PENDING)
	{
		if (!pendingReturned && Irp->PendingReturned) 
		{
			//
			//	It must not happen.
			//
			ASSERT(FALSE) ;
/*
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				( "Secondary_PassThrough: PendingReturned = TRUE. Mark pending.\n")) ;

			IoMarkIrpPending( Irp );
			*NtStatus = STATUS_PENDING ;
*/
		}
		IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;
	}

	Secondary_Dereference(Secondary);
	return TRUE;
}


FORCEINLINE
PSECONDARY_REQUEST
ALLOC_WINXP_SECONDARY_REQUEST(
	IN PSECONDARY	Secondary,
	IN _U8			IrpMajorFunction,
	IN UINT32		DataSize
	)
{
	if(DataSize > Secondary->Thread.SessionContext.SecondaryMaxDataSize)
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			("ALLOC_WINXP_SECONDARY_REQUEST %s(%d) MdataSize = %X\n", 
				IrpMajors[IrpMajorFunction], IrpMajorFunction, DataSize));

		
	return AllocSecondaryRequest(
				(Secondary),
				sizeof(NDFS_REQUEST_HEADER)
				+ (
					(Secondary->Thread.SessionContext.MessageSecurity == 0)
						? (sizeof(NDFS_WINXP_REQUEST_HEADER) + (DataSize))
							: (
								 ((IrpMajorFunction == IRP_MJ_WRITE
									&& Secondary->Thread.SessionContext.RwDataSecurity == 0
									&& DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)
								 ||
								 (IrpMajorFunction == IRP_MJ_READ
									&& Secondary->Thread.SessionContext.RwDataSecurity == 0	
									&& DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize))		
								   ? (ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER))*2 + DataSize)		
								   : (ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)*2)
					)
				  ),
				TRUE
				);
}


FORCEINLINE
VOID
INITIALIZE_NDFS_REQUEST_HEADER(
	PNDFS_REQUEST_HEADER	NdfsRequestHeader,
	_U8						Command,
	PSECONDARY				Secondary,
	_U8						IrpMajorFunction,
	_U32					DataSize
	)
{
	KIRQL	oldIrql;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);

	RtlCopyMemory(NdfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsRequestHeader->Protocol));
	NdfsRequestHeader->Command	= Command;				
	NdfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;							    
	NdfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;									
	NdfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;									
	NdfsRequestHeader->Mid		= 0;																	    
	NdfsRequestHeader->MessageSize																			
		= sizeof(NDFS_REQUEST_HEADER)																			
			+ (																									
				(Secondary->Thread.SessionContext.MessageSecurity == 0)										
	 			 ? sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize											
				 : (																							
					 ((IrpMajorFunction == IRP_MJ_WRITE														
						&& Secondary->Thread.SessionContext.RwDataSecurity == 0								
						&& DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)				
					 ||																							
					 IrpMajorFunction == IRP_MJ_READ																
						&& Secondary->Thread.SessionContext.RwDataSecurity == 0								
						&& DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize)

					? ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)							
					: ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)				
				   )																							
				);																								
	
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	return;
}																										


// FALSE : Irp IO is occurred
// TRUE	 : Fast IO is executed

BOOLEAN
Secondary_RedirectFileIo(
	IN  PSECONDARY		Secondary,
	IN  PLFS_FILE_IO	LfsFileIo
	)
{
	BOOLEAN			returnResult;
	PFILE_OBJECT	fileObject;

	PLFS_FCB			fcb;
	PFILE_EXTENTION		fileExt;
	PSECONDARY_REQUEST	secondaryRequest;


	return FALSE;

	
	fileObject = LfsFileIo->FileObject;
 
	if(LfsFileIo->FileIoType != LFS_FILE_IO_CREATE)
	{
		fcb = fileObject->FsContext;
		fileExt = fileObject->FsContext2;
	
		ASSERT(Secondary_LookUpFileExtension(Secondary, fileObject) == fileExt);
		ASSERT(fileExt->Fcb == fcb);

		if(fileExt->Corrupted == TRUE)
			return FALSE;
 	}


	switch(LfsFileIo->FileIoType)
	{
    case LFS_FILE_IO_FAST_IO_CHECK_IF_POSSIBLE: // 0x1c
	{
		returnResult = TRUE;
		break;
	}
    case LFS_FILE_IO_READ: // 0x03
	{
		PREAD_FILE_IO				readFileIo = &LfsFileIo->Read;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer;
		
		LARGE_INTEGER				currentByteOffset;

		ULONG						totalReadRequestLength;
		NTSTATUS					lastStatus;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		if(readFileIo->Wait == FALSE)
			return FALSE;

		outputBuffer	= readFileIo->Buffer;
		

		ASSERT(!(readFileIo->FileOffset->LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& readFileIo->FileOffset->HighPart == -1));

		currentByteOffset.QuadPart = readFileIo->FileOffset->QuadPart;
		totalReadRequestLength = 0;
		
		do
		{
			ULONG						outputBufferLength;
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			

//			outputBufferLength = read->Length;
			outputBufferLength = ((readFileIo->Length-totalReadRequestLength) <= Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (readFileIo->Length-totalReadRequestLength) : Secondary->Thread.SessionContext.SecondaryMaxDataSize;


			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_READ,
								outputBufferLength
								);

			if(secondaryRequest == NULL)
			{
				ASSERT(LFS_REQUIRED);

				returnResult = FALSE;	
				break;
			}
			
			secondaryRequest->OutputBuffer = (PUCHAR)outputBuffer+totalReadRequestLength;
			secondaryRequest->OutputBufferLength = outputBufferLength;
	
			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_READ,
				0
				);

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

			ndfsWinxpRequestHeader->IrpTag				= (_U32)LfsFileIo;
			ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_READ;
			ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
			ndfsWinxpRequestHeader->FileHandle			= fileExt->PrimaryFileHandle;
			ndfsWinxpRequestHeader->IrpFlags			= 0;
			ndfsWinxpRequestHeader->IrpSpFlags			= 0;
	
			ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
			ndfsWinxpRequestHeader->Read.Key		= readFileIo->LockKey;
			ndfsWinxpRequestHeader->Read.ByteOffset = currentByteOffset.QuadPart+totalReadRequestLength;

			{														
				KIRQL		oldIrql;
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

			ASSERT(waitStatus == STATUS_SUCCESS);

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
				returnResult = FALSE;	
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				break;
			}

			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				returnResult = FALSE;
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
				
				break;
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			totalReadRequestLength += ndfsWinxpReplytHeader->Information;
			lastStatus = ndfsWinxpReplytHeader->Status;
			returnResult = TRUE;
			
			if(lastStatus != STATUS_SUCCESS)
			{	
				returnResult = TRUE;
				DereferenceSecondaryRequest(
					secondaryReq–* àˇˇ  * àˇˇ#Ø	 àˇˇ                                     @ * àˇˇ@ * àˇˇ`≈!Åˇˇˇˇ                0Ëp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                h* àˇˇÄ&Ø	 àˇˇ                                     ®* àˇˇ®* àˇˇ`≈!Åˇˇˇˇ                0Ïp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        h* àˇˇ–* àˇˇêØ	 àˇˇ                                      	* àˇˇ	* àˇˇ`≈!Åˇˇˇˇ                @Óp4 àˇˇ∞·p4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                êr* àˇˇ8* àˇˇ†ü÷ àˇˇ                                     x* àˇˇx* àˇˇ`≈!Åˇˇˇˇ                †ãl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          * àˇˇ†* àˇˇ`-Ø	 àˇˇ                                     ‡* àˇˇ‡* àˇˇ`≈!Åˇˇˇˇ                †Ëp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ,* àˇˇ* àˇˇ Àº	 àˇˇ                                     H* àˇˇH* àˇˇ`≈!Åˇˇˇˇ                ‡çl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        (n* àˇˇp* àˇˇ¿ÿº	 àˇˇ                                     ∞* àˇˇ∞* àˇˇ`≈!Åˇˇˇˇ                0âl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        p* àˇˇÿ* àˇˇ0‹º	 àˇˇ                                     * àˇˇ* àˇˇ`≈!Åˇˇˇˇ                äl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        8* àˇˇ@#* àˇˇ–∞÷ àˇˇ                                     Ä#* àˇˇÄ#* àˇˇ`≈!Åˇˇˇˇ                Äâl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ∞=* àˇˇ®'* àˇˇ‡—º	 àˇˇ                                     Ë'* àˇˇË'* àˇˇ`≈!Åˇˇˇˇ                `él àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        @#* àˇˇ,* àˇˇ∞¿º	 àˇˇ                                     P,* àˇˇP,* àˇˇ`≈!Åˇˇˇˇ                –ål àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        `{* àˇˇx0* àˇˇpé÷ àˇˇ                                     ∏0* àˇˇ∏0* àˇˇ`≈!Åˇˇˇˇ                †Ål àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        PO* àˇˇ‡4* àˇˇ∞˜º	 àˇˇ                                      5* àˇˇ 5* àˇˇ`≈!Åˇˇˇˇ                êèl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        à\* àˇˇH9* àˇˇ‡ëL  àˇˇ                                     à9* àˇˇà9* àˇˇ`≈!Åˇˇˇˇ                êÁp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        * àˇˇ∞=* àˇˇpŒº	 àˇˇ                                     =* àˇˇ=* àˇˇ`≈!Åˇˇˇˇ                Pçl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ∏S* àˇˇB* àˇˇ ã÷ àˇˇ                                      XB* àˇˇXB* àˇˇ`≈!Åˇˇˇˇ                 àl àˇˇêÜl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                †* àˇˇÄF* àˇˇ©÷ àˇˇ                                     ¿F* àˇˇ¿F* àˇˇ`≈!Åˇˇˇˇ                Päl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ÄF* àˇˇËJ* àˇˇ¿ò÷ àˇˇ                                     (K* àˇˇ(K* àˇˇ`≈!Åˇˇˇˇ                ¿Äl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         X* àˇˇPO* àˇˇ`Ìº	 àˇˇ                                     êO* àˇˇêO* àˇˇ`≈!Åˇˇˇˇ                ¿Öl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        x0* àˇˇ∏S* àˇˇ Ñ÷ àˇˇ                                     ¯S* àˇˇ¯S* àˇˇ`≈!Åˇˇˇˇ                Äçl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        Xe* àˇˇ X* àˇˇÈº	 àˇˇ                                     `X* àˇˇ`X* àˇˇ`≈!Åˇˇˇˇ                0ãl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ‡4* àˇˇà\* àˇˇÄÊº	 àˇˇ                         Ä            »\* àˇˇ»\* àˇˇ`≈!Åˇˇˇˇ                ál àˇˇ Äl àˇˇ Ñl àˇˇ`äl àˇˇ èl àˇˇ–Öl àˇˇ ál àˇˇÄl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                B* àˇˇ`* àˇˇ∞Ä÷ àˇˇ                                     0a* àˇˇ0a* àˇˇ`≈!Åˇˇˇˇ                 Ål àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ¿i* àˇˇXe* àˇˇ„º	 àˇˇ                                      òe* àˇˇòe* àˇˇ`≈!Åˇˇˇˇ                 ål àˇˇ‡Él àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ÿ* àˇˇ¿i* àˇˇ†ﬂº	 àˇˇ                                      j* àˇˇ j* àˇˇ`≈!Åˇˇˇˇ                Pél àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ®'* àˇˇ(n* àˇˇP’º	 àˇˇ                                     hn* àˇˇhn* àˇˇ`≈!Åˇˇˇˇ                †Ül àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        `* àˇˇêr* àˇˇ£÷ àˇˇ                                     –r* àˇˇ–r* àˇˇ`≈!Åˇˇˇˇ                pàl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ËJ* àˇˇ¯v* àˇˇPï÷ àˇˇ                                     8w* àˇˇ8w* àˇˇ`≈!Åˇˇˇˇ                †Él àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ¯v* àˇˇ`{* àˇˇ‡ë÷ àˇˇ                                      †{* àˇˇ†{* àˇˇ`≈!Åˇˇˇˇ                ¿Ål àˇˇ Ñl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         ¿* àˇˇ       êë àˇˇ                                     @Ä* àˇˇ@Ä* àˇˇ`≈!Åˇˇˇˇ                 ãl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ñ* àˇˇhÑ* àˇˇ0ë àˇˇ                                     ®Ñ* àˇˇ®Ñ* àˇˇ`≈!Åˇˇˇˇ                ‡Öl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        XÂ* àˇˇ–à* àˇˇ∞7ë àˇˇ                         0            â* àˇˇâ* àˇˇ`≈!Åˇˇˇˇ                ¿àl àˇˇål àˇˇ äl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        hÑ* àˇˇ8ç* àˇˇ–0ë àˇˇ                                     xç* àˇˇxç* àˇˇ`≈!Åˇˇˇˇ                –âl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         Ä* àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                       8ç* àˇˇ†ë* àˇˇ ë àˇˇ                                      ‡ë* àˇˇ‡ë* àˇˇ`≈!Åˇˇˇˇ                Ål àˇˇPÜl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                Pœ* àˇˇñ* àˇˇ ;ë àˇˇ                                      Hñ* àˇˇHñ* àˇˇ`≈!Åˇˇˇˇ                †Ñl àˇˇ†èl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ¿È* àˇˇpö* àˇˇ#ë àˇˇ                                     ∞ö* àˇˇ∞ö* àˇˇ`≈!Åˇˇˇˇ                ‡âl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        x∞* àˇˇÿû* àˇˇ†ë àˇˇ                                     ü* àˇˇü* àˇˇ`≈!Åˇˇˇˇ                `àl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       †ë* àˇˇ@£* àˇˇ`-ë àˇˇ                                      Ä£* àˇˇÄ£* àˇˇ`≈!Åˇˇˇˇ                `Él àˇˇÄèl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                Ë * àˇˇ®ß* àˇˇ∞ ë àˇˇ                                     Ëß* àˇˇËß* àˇˇ`≈!Åˇˇˇˇ                Öl àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ‡¥* àˇˇ¨* àˇˇÄÊò$ àˇˇ                                     P¨* àˇˇP¨* àˇˇ`≈!Åˇˇˇˇ                êÎp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 †* àˇˇ                                                                                                               ¨* àˇˇx∞* àˇˇ`-Ÿ8 àˇˇ                                      ∏∞* àˇˇ∏∞* àˇˇ`≈!Åˇˇˇˇ                ∞‰p4 àˇˇ∞Âp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                êÚ* àˇˇ‡¥* àˇˇ0Ÿ8 àˇˇ                         ‡             µ* àˇˇ µ* àˇˇ`≈!Åˇˇˇˇ                Ä‡p4 àˇˇÍp4 àˇˇ0Áp4 àˇˇêÔp4 àˇˇ`Ôp4 àˇˇ–Ëp4 àˇˇ Óp4 àˇˇ`Ïp4 àˇˇ‡Íp4 àˇˇ`‚p4 àˇˇpÊp4 àˇˇPÌp4 àˇˇ Îp4 àˇˇ@‚p4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ¬* àˇˇHπ* àˇˇ‡Ÿ8 àˇˇ                         0            àπ* àˇˇàπ* àˇˇ`≈!Åˇˇˇˇ                PÔp4 àˇˇ0Îp4 àˇˇ Áp4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        Hπ* àˇˇ∞Ω* àˇˇ∞ Ÿ8 àˇˇ                                     Ω* àˇˇΩ* àˇˇ`≈!Åˇˇˇˇ                 ·p4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
	return fileExt;
}