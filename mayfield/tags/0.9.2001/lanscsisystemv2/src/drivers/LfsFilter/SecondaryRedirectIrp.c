#define	__SECONDARY__
#include "LfsProc.h"


#define QuadAlign(P) (		\
    ((((P)) + 7) & (-8))	\
)

#define IsCharZero(C)    (((C) & 0x000000ff) == 0x00000000)
#define IsCharMinus1(C)  (((C) & 0x000000ff) == 0x000000ff)
#define IsCharLtrZero(C) (((C) & 0x00000080) == 0x00000080)
#define IsCharGtrZero(C) (!IsCharLtrZero(C) && !IsCharZero(C))

VOID
PrintFileRecordSegmentHeader(
	IN PFILE_RECORD_SEGMENT_HEADER	FileRecordSegmentHeader
	);

#define	INITIALIZE_NDFS_WINXP_REQUEST_HEADER(	\
				MndfsWinxpRequestHeader,		\
				Mirp,							\
				MirpSp,							\
				MprimaryFileHandle				\
				);								\
{																			\
	(MndfsWinxpRequestHeader)->IrpTag			= (_U32)(Mirp);				\
	(MndfsWinxpRequestHeader)->IrpMajorFunction = (MirpSp)->MajorFunction;	\
	(MndfsWinxpRequestHeader)->IrpMinorFunction = (MirpSp)->MinorFunction;	\
	(MndfsWinxpRequestHeader)->FileHandle		= (MprimaryFileHandle);		\
	(MndfsWinxpRequestHeader)->IrpFlags			= (Mirp)->Flags;			\
	(MndfsWinxpRequestHeader)->IrpSpFlags		= (MirpSp)->Flags;			\
}

NTSTATUS	
ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
	PSECONDARY			Secondary,
	PBOOLEAN			FastMutexSet,
	PFILE_EXTENTION		FileExt,
	PBOOLEAN			Retry,
	PSECONDARY_REQUEST	SecondaryRequest,
	ULONG				CurrentSessionId
	)
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
	//ExAcquireFastMutex(&Secondary->FastMutex);
	//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

	ASSERT(Secondary->Semaphore.Header.SignalState <= Secondary->Thread.SessionContext.RequestsPerSession);
	//if(Secondary->Semaphore.Header.SignalState != 1)
	//	DbgBreakPoint();

	//if(!IsListEmpty(&Secondary->Thread.RequestQueue))
	//	DbgBreakPoint();

	*FastMutexSet = TRUE;											
	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	
	if(Secondary->Flags & SECONDARY_ERROR
		|| Secondary->SessionId != CurrentSessionId
		|| FileExt/* if IRP_MJ_CREATE, fileExt is NULL */ && FileExt->Corrupted == TRUE)
	{
		if(SecondaryRequest)
		{	
			DereferenceSecondaryRequest(SecondaryRequest);
			SecondaryRequest = NULL;
		}

		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
		
		*Retry		 = TRUE;
		*FastMutexSet = FALSE;
																									
		return STATUS_UNSUCCESSFUL;																	
	}																							
	
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	return STATUS_SUCCESS;																			
}


NTSTATUS
Secondary_MakeFullFileName(
	IN PFILE_OBJECT		FileObject, 
	IN PUNICODE_STRING	FullFileName,
	IN BOOLEAN			fileDirectoryFile
	)
{
	NTSTATUS	returnStatus;

	if(FileObject->RelatedFileObject)
	{
		PLFS_FCB		relatedFcb = FileObject->RelatedFileObject->FsContext;
		PFILE_EXTENTION	relatedFileExt = FileObject->RelatedFileObject->FsContext2;
		
		//ASSERT(relatedFileExt->TypeOfOpen == UserDirectoryOpen);

		returnStatus = RtlAppendUnicodeStringToString(
								FullFileName,
								&relatedFcb->FullFileName
								);

		if(returnStatus != STATUS_SUCCESS)
			return returnStatus;

		if(relatedFcb->FullFileName.Buffer[relatedFcb->FullFileName.Length/sizeof(WCHAR)-1] != L'\\')
		{
			//ASSERT(LFS_UNEXPECTED);

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Secondary_MakeFullFileName, relatedFcb->FullFileName = %wZ, FileObject->FileName = %wZ\n", &relatedFcb->FullFileName, &FileObject->FileName));

			returnStatus = RtlAppendUnicodeToString(
									FullFileName,
									L"\\"
									);
		}else
		{
			if(FileObject->FileName.Length == 0)
			{
				ASSERT(LFS_REQUIRED);
			}
		}
	}
	
	returnStatus = RtlAppendUnicodeStringToString(
							FullFileName,
							&FileObject->FileName
							);

	if(returnStatus != STATUS_SUCCESS)
		return returnStatus;

	if(fileDirectoryFile && FullFileName->Buffer[FullFileName->Length/sizeof(WCHAR)-1] != L'\\')
	{
		returnStatus = RtlAppendUnicodeToString(
							FullFileName,
							L"\\"
							);
		if(returnStatus != STATUS_SUCCESS)
			return returnStatus;
	}
		
	return STATUS_SUCCESS;
}


NTSTATUS
RedirectIrpMajorCreate(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	)
{
	NTSTATUS					returnStatus;
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	PLFS_FCB					fcb;
	PFILE_EXTENTION				fileExt;

	ULONG						dataSize;
	PSECONDARY_REQUEST			secondaryRequest;

	struct Create				create;
    ULONG						eaLength;			
		
	WINXP_REQUEST_CREATE		CreateContext;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;
	NTSTATUS					waitStatus;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

	PFILE_RECORD_SEGMENT_HEADER	fileRecordSegmentHeader;
	UNICODE_STRING				fullFileName;
	WCHAR						fullFileNameBuffer[NDFS_MAX_PATH];
	NTSTATUS					appendStatus;
	TYPE_OF_OPEN				typeOfOpen;

	ULONG						fileExtSize;


	create.EaLength			= irpSp->Parameters.Create.EaLength;
	create.FileAttributes	= irpSp->Parameters.Create.FileAttributes;
	create.Options			= irpSp->Parameters.Create.Options;
	create.SecurityContext	= irpSp->Parameters.Create.SecurityContext;
	create.ShareAccess		= irpSp->Parameters.Create.ShareAccess;

	ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
	
	if(BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE))
	{
		ASSERT(LFS_REQUIRED);
		PrintIrp(LFS_DEBUG_SECONDARY_INFO, "SL_OPEN_PAGING_FILE", Secondary->LfsDeviceExt, Irp);
	}

	if(FILE_OPEN_BY_FILE_ID & create.Options)
		PrintIrp(LFS_DEBUG_SECONDARY_INFO, "RedirectIrp FILE_OPEN_BY_FILE_ID", Secondary->LfsDeviceExt, Irp);

	if(Irp->AssociatedIrp.SystemBuffer != NULL)
	{
		PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
		
		eaLength = 0;
		while(fileFullEa->NextEntryOffset)
		{
			eaLength += fileFullEa->NextEntryOffset;
			fileFullEa = (PFILE_FULL_EA_INFORMATION)((_U8 *)fileFullEa + fileFullEa->NextEntryOffset);
		}

		eaLength += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("RedirectIrp, IRP_MJ_CREATE: Ea is set create->EaLength = %d, eaLength = %d\n",
								create.EaLength, eaLength));
	}
	else
		eaLength = 0;

	ASSERT(fileObject->FsContext == NULL);	
	ASSERT(Secondary_LookUpFileExtension(Secondary, fileObject) == NULL);

	ASSERT(fileObject->FileName.Length + eaLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);

	if(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS)
	{
		dataSize = ((eaLength + fileObject->FileName.Length) > Secondary->Thread.SessionContext.BytesPerFileRecordSegment)
					? (eaLength + fileObject->FileName.Length) 
					  : Secondary->Thread.SessionContext.BytesPerFileRecordSegment;
	}
	else
		dataSize = eaLength + fileObject->FileName.Length;

	secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
						Secondary,
						IRP_MJ_CREATE,
						dataSize
						);

	if(secondaryRequest == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	secondaryRequest->OutputBuffer = NULL;
	secondaryRequest->OutputBufferLength = 0;

	ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
	INITIALIZE_NDFS_REQUEST_HEADER(
		ndfsRequestHeader,
		NDFS_COMMAND_EXECUTE,
		Secondary,
		IRP_MJ_CREATE,
		(eaLength + fileObject->FileName.Length)
		);

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
	INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
		ndfsWinxpRequestHeader,
		Irp,
		irpSp,
		0
		);

	if(fileObject->RelatedFileObject)
	{
		PFILE_EXTENTION	relatedFileExt = fileObject->RelatedFileObject->FsContext2;


		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
						("RedirectIrp, IRP_MJ_CREATE: RelatedFileObject is binded\n")) ;

		if(relatedFileExt->LfsMark != LFS_MARK)
		{
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;

			return STATUS_UNSUCCESSFUL;	
		}
	
		// ASSERT(relatedFileExt->SessionId == Secondary->SessionId); It's tested later implicitely
		if(relatedFileExt->Corrupted == TRUE)
		{
			DereferenceSecondaryRequest(secondaryRequest);
			secondaryRequest = NULL;

			Irp->IoStatus.Status = STATUS_OBJECT_PATH_NOT_FOUND;
			Irp->IoStatus.Information = 0;

			return STATUS_SUCCESS;
		}

		relatedFileExt->Fcb->FileRecordSegmentHeaderAvail = FALSE;
	
		CreateContext.RelatedFileHandle = relatedFileExt->PrimaryFileHandle; 
	}
	else {
		CreateContext.RelatedFileHandle = 0;
	}		
  
	CreateContext.DesiredAccess		= create.SecurityContext->DesiredAccess;
	CreateContext.Options			= create.Options;
	CreateContext.FileAttributes	= create.FileAttributes;
	CreateContext.ShareAccess		= create.ShareAccess;
	
	CreateContext.EaLength			= eaLength; /* ? create->EaLength : 0; */
		
	CreateContext.AllocationSizeLowPart  = Irp->Overlay.AllocationSize.LowPart;
	CreateContext.AllocationSizeHighPart = Irp->Overlay.AllocationSize.HighPart;
		
	//added by ktkim 03/15/2004
	CreateContext.FullCreateOptions	= create.SecurityContext->FullCreateOptions;
	CreateContext.FileNameLength	= fileObject->FileName.Length;

	RtlCopyMemory(
		&ndfsWinxpRequestHeader->Create,
		&CreateContext,
		sizeof(WINXP_REQUEST_CREATE)
		);
		
	ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
	//encryptedData = ndfsWinxpRequestData + ADD_ALIGN8(create->EaLength + fileObject->FileName.Length);

	if(eaLength)
	{
		// It have structure align Problem. If you wanna release, Do more
		PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;
		RtlCopyMemory(
			ndfsWinxpRequestData,
			eaBuffer,
			eaLength
			);
	}

	if(fileObject->FileName.Length)
	{
		RtlCopyMemory(
			ndfsWinxpRequestData + eaLength,
			fileObject->FileName.Buffer,
			fileObject->FileName.Length
			);
	}

	fileExt = NULL;
	if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
			Secondary,
			FastMutexSet,
			fileExt,
			Retry,
			secondaryRequest,
			secondaryRequest->SessionId
			) != STATUS_SUCCESS)
	{
		return STATUS_UNSUCCESSFUL;
	}

	secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
	QueueingSecondaryRequest(Secondary, secondaryRequest);
				
	timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
	waitStatus = KeWaitForSingleObject(
						&secondaryRequest->CompleteEvent,
						Executive,
						KernelMode,
						FALSE,
//						&timeOut
						0
						);

	KeClearEvent(&secondaryRequest->CompleteEvent);

	if(waitStatus != STATUS_SUCCESS) 
	{
		ASSERT(LFS_BUG);

		secondaryRequest = NULL;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;
		return STATUS_TIMEOUT;
	}

	if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
	{
		returnStatus = secondaryRequest->ExecuteStatus;	
		DereferenceSecondaryRequest(secondaryRequest);
			
		secondaryRequest = NULL;
		return returnStatus;
	}
				
	ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			
	Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
	Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;

	if(ndfsWinxpReplytHeader->Status != STATUS_SUCCESS)
	{
		DereferenceSecondaryRequest(secondaryRequest);				
		secondaryRequest = NULL;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;

		return STATUS_SUCCESS;
	}

	if(Secondary->Thread.SessionContext.BytesPerFileRecordSegment 
		&& fileObject->FileName.Length 
		&& Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS)
	{
		if(secondaryRequest->NdfsReplyHeader.MessageSize ==
				(sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + Secondary->Thread.SessionContext.BytesPerFileRecordSegment))
		{	
			fileRecordSegmentHeader = (PFILE_RECORD_SEGMENT_HEADER)(ndfsWinxpReplytHeader+1);
			PrintFileRecordSegmentHeader(fileRecordSegmentHeader);
		}
		else
		{
			ASSERT(secondaryRequest->NdfsReplyHeader.MessageSize ==
					(sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER)));
			fileRecordSegmentHeader = NULL;
		}
	}
	else
		fileRecordSegmentHeader = NULL;

	if(fileRecordSegmentHeader)
	{
		if(BooleanFlagOn(fileRecordSegmentHeader->Flags, FILE_FILE_NAME_INDEX_PRESENT))
			typeOfOpen = UserDirectoryOpen;
		else
		{
			if(fileObject->FileName.Buffer[fileObject->FileName.Length/sizeof(WCHAR)-1] == L'\\'
				|| BooleanFlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE))
			{
				typeOfOpen = UserDirectoryOpen;
			}
			else
				typeOfOpen = UserFileOpen;
		}
	}
	else
	{
		if(fileObject->FileName.Length == 0)
		{
			typeOfOpen = UserVolumeOpen;
		}
		else if((fileObject->FileName.Buffer[fileObject->FileName.Length/sizeof(WCHAR)-1] == L'\\')
				|| BooleanFlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE))
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrpMajorCreate: Directory fileObject->FileName = %wZ BooleanFlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE) = %d\n",
						&fileObject->FileName, BooleanFlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE)));
		
			typeOfOpen = UserDirectoryOpen;
		}
		else
		{
			typeOfOpen = UserFileOpen;	
		}
	}
	
	if(typeOfOpen == UserFileOpen && irpSp->Parameters.Create.Options & FILE_NO_INTERMEDIATE_BUFFERING)
		fileObject->Flags |= FO_CACHE_SUPPORTED;

	RtlInitEmptyUnicodeString( 
		&fullFileName,
		fullFileNameBuffer,
        sizeof(fullFileNameBuffer) 
		);

	appendStatus = Secondary_MakeFullFileName(
						fileObject, 
						&fullFileName,
						(typeOfOpen == UserDirectoryOpen)	
						);

	if(appendStatus != STATUS_SUCCESS)
	{
		ASSERT(LFS_UNEXPECTED);
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;
		return STATUS_UNSUCCESSFUL;
	}

	fcb = Secondary_LookUpFcb(
			Secondary,
			&fullFileName,
			!BooleanFlagOn(irpSp->Flags, SL_CASE_SENSITIVE)
			);

	if(fcb == NULL)
	{
		KIRQL	oldIrql;
		BOOLEAN fcbQueueEmpty;
			
			
		KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);
		fcbQueueEmpty = IsListEmpty(&Secondary->FcbQueue);
		KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);
			
#if 0
		if(fcbQueueEmpty)
		{
			ASSERT(Secondary->VolumeRootFileHandle == NULL);
			Secondary->VolumeRootFileHandle = OpenVolumeRootFile(Secondary->LfsDeviceExt); // Only one can update
		}

		if(Secondary->VolumeRootFileHandle == NULL)
		{
			ASSERT(LFS_UNEXPECTED);
			ExReleaseFastMutex(&Secondary->FastMutex);
			*FastMutexSet = FALSE;
			return STATUS_UNSUCCESSFUL;
		}
#endif

		fcb = AllocateFcb(
					Secondary,
					&fullFileName,
					fileRecordSegmentHeader ? Secondary->Thread.SessionContext.BytesPerFileRecordSegment : 0
					);

		if(fcb == NULL) 
		{
			ASSERT(LFS_INSUFFICIENT_RESOURCES);

			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			
			*FastMutexSet = FALSE;
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		if(fileRecordSegmentHeader 
			&& !(fcb->FileRecordSegmentHeaderAvail == FALSE && fcb->OpenTime.HighPart != 0))
		{
			ASSERT(secondaryRequest->NdfsReplyHeader.MessageSize ==
				(sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + Secondary->Thread.SessionContext.BytesPerFileRecordSegment));
						
			RtlCopyMemory(
				fcb->Buffer,
				fileRecordSegmentHeader,
				Secondary->Thread.SessionContext.BytesPerFileRecordSegment
				);

			fcb->OpenTime.HighPart = ndfsWinxpReplytHeader->Open.OpenTimeHigPart;
			fcb->OpenTime.LowPart = 0;
			((_U8 *)&fcb->OpenTime.LowPart)[3] = ndfsWinxpReplytHeader->Open.OpenTimeLowPartMsb;

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrp, IRP_MJ_CREATE: fcb->OpenTime.LowPart = %x\n",
							fcb->OpenTime.LowPart));

			fcb->FileRecordSegmentHeaderAvail = TRUE;
		}
		else
			fcb->FileRecordSegmentHeaderAvail = FALSE;
		
		ExInterlockedInsertHeadList(
					&Secondary->FcbQueue,
					&fcb->ListEntry,
					&Secondary->FcbQSpinLock
					);
	}
	else
	{
		if(fcb->FileRecordSegmentHeaderAvail == TRUE)
		{
			ASSERT(fileRecordSegmentHeader);
			RtlCopyMemory(
				fcb->Buffer,
				fileRecordSegmentHeader,
				Secondary->Thread.SessionContext.BytesPerFileRecordSegment
				);

			fcb->OpenTime.HighPart = ndfsWinxpReplytHeader->Open.OpenTimeHigPart;
			fcb->OpenTime.LowPart = 0;
			((_U8 *)&fcb->OpenTime.LowPart)[3] = ndfsWinxpReplytHeader->Open.OpenTimeLowPartMsb;
		}
	}

	if(create.SecurityContext->FullCreateOptions & FILE_DELETE_ON_CLOSE)
		fcb->DeletePending = TRUE;
	
	InterlockedIncrement(&fcb->OpenCount);
	InterlockedIncrement(&fcb->UncleanCount);

	fileExtSize = eaLength + NDFS_MAX_PATH*sizeof(WCHAR) + Secondary->Thread.SessionContext.BytesPerSector;
	if(fcb->FileRecordSegmentHeaderAvail == TRUE)
		fileExtSize += Secondary->Thread.SessionContext.BytesPerSector;

	fileExt = AllocateFileExt(
				Secondary,
				fileObject,
				fileExtSize
				);

	if(fileExt == NULL) 
	{
		KIRQL oldIrql;

		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);
		InterlockedDecrement(&fcb->UncleanCount);
		if(InterlockedDecrement(&fcb->OpenCount) == 0)
		{
			RemoveEntryList(&fcb->ListEntry);
			InitializeListHead(&fcb->ListEntry);
		}	
		KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);
		Secondary_DereferenceFcb(fcb);

		fileObject->FsContext = NULL;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	fileExt->CachedVcn = UNUSED_VCN;
	if(fcb->FileRecordSegmentHeaderAvail == TRUE)
	{
		fileExt->Cache = fileExt->Buffer + eaLength + NDFS_MAX_PATH*sizeof(WCHAR);
	}

	ASSERT(ndfsWinxpReplytHeader->Open.FileHandle != 0);
	fileExt->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
	fileExt->Fcb = fcb;
	fileExt->TypeOfOpen = typeOfOpen;

	RtlCopyMemory(
		&fileExt->CreateContext,
		&CreateContext,
		sizeof(WINXP_REQUEST_CREATE)
		);

	if(eaLength)
	{
		// It have structure align Problem. If you wanna release, Do more
		PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;
		
		RtlCopyMemory(
			fileExt->Buffer,
			eaBuffer,
			eaLength
			);
	}

	if(fileObject->FileName.Length)
		RtlCopyMemory(
			fileExt->Buffer + eaLength,
			fileObject->FileName.Buffer,
			fileObject->FileName.Length
			);

    ExAcquireFastMutex(&Secondary->FileExtQMutex);
	InsertHeadList(
		&Secondary->FileExtQueue,
		&fileExt->ListEntry
		);
	ExReleaseFastMutex(&Secondary->FileExtQMutex);

	fileObject->FsContext = fcb ;
	fileObject->FsContext2 = fileExt;

#if 0
	if(Irp->IoStatus.Information == FILE_CREATED)
	{
		USHORT	targetNameOffset;
		USHORT	i;

		
		for(i=(fcb->FullFileName.Length/sizeof(WCHAR)); i>0; i--)
		{
			if(fcb->FullFileName.Buffer[i-1] == L'\\')
				break;
		}

		targetNameOffset = i*sizeof(WCHAR);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrp, IRP_MJ_CREATE: targetNameOffset = %d, i = %d Length = %d, Name = %wZ\n",
							targetNameOffset, i, fcb->FullFileName.Length, &fcb->FullFileName));
		
		FsRtlNotifyFullReportChange(
			Secondary->NotifySync,
			&Secondary->DirNotifyList,			
			(PSTRING)&fcb->FullFileName,
			targetNameOffset,
			NULL,
			NULL,
			FILE_NOTIFY_CHANGE_DIR_NAME,
            FILE_ACTION_ADDED,
			NULL
			);
	}
#endif

	if(ndfsWinxpReplytHeader->Open.SetSectionObjectPointer == TRUE)
		fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;
	else
		fileObject->SectionObjectPointer = NULL;

	fileObject->Vpb = Secondary->LfsDeviceExt->Vpb;

	DereferenceSecondaryRequest(secondaryRequest);
				
	secondaryRequest = NULL;

	//ExReleaseFastMutex(&Secondary->FastMutex);
	KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

	*FastMutexSet = FALSE;
	
	return STATUS_SUCCESS;
}




BOOLEAN
NtfsLookupInFileRecord(
	IN  PSECONDARY					Secondary,
	IN  PFILE_RECORD_SEGMENT_HEADER	FileRecordSegmentHeader,
    IN  ATTRIBUTE_TYPE_CODE			QueriedTypeCode,
	IN	PATTRIBUTE_RECORD_HEADER	CurrentAttributeRecordHeader,
	OUT PATTRIBUTE_RECORD_HEADER	*AttributeRecordHeader
	);


NTSTATUS
RedirectIrpMajorRead(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	)
{
	NTSTATUS					returnStatus;
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	PLFS_FCB					fcb = fileObject->FsContext;
	PFILE_EXTENTION				fileExt = fileObject->FsContext2;

	PSECONDARY_REQUEST			secondaryRequest;
	
	struct Read					read;

	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PUCHAR						outputBuffer = (PUCHAR)MapOutputBuffer(Irp);
		
	BOOLEAN						synchronousIo = BooleanFlagOn(fileObject->Flags, FO_SYNCHRONOUS_IO);
	BOOLEAN						pagingIo      = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
	BOOLEAN						nonCachedIo   = BooleanFlagOn(Irp->Flags,IRP_NOCACHE);
		
	ULONG						totalReadRequestLength;
	NTSTATUS					lastStatus;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

	BOOLEAN						endOfFile = FALSE;
	BOOLEAN						checkResult = FALSE;


	read.ByteOffset	= irpSp->Parameters.Read.ByteOffset;
	read.Key		= irpSp->Parameters.Read.Key;
	read.Length		= irpSp->Parameters.Read.Length;

	ASSERT(!(read.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
					&& read.ByteOffset.HighPart == -1));

	totalReadRequestLength = 0;

#if 0
	if(synchronousIo)
	{
		if((read.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& read.ByteOffset.HighPart == -1))
		{
			currentByteOffset.QuadPart = fileObject->CurrentByteOffset.QuadPart;
		} else
			currentByteOffset.QuadPart = read->ByteOffset.QuadPart;
	} else
		currentByteOffset.QuadPart = read->ByteOffset.QuadPart;
#endif

#if 0
	if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
			Secondary,
			Retry,
			FastMutexSet,
			fileExt,
			NULL,
			Secondary->SessionId
			) != STATUS_SUCCESS)
	{
		return STATUS_UNSUCCESSFUL;
	}
	
	ExReleaseFastMutex(&Secondary->FastMutex);
	*FastMutexSet = FALSE;
	RtlZeroMemory(
		(PCHAR)outputBuffer,
		read.Length
		);
#endif
	
	

	if(!pagingIo && nonCachedIo && fileObject->SectionObjectPointer->DataSectionObject != NULL)
	{
		IO_STATUS_BLOCK	ioStatusBlock;

		CcFlushCache(
			fileObject->SectionObjectPointer,
			&read.ByteOffset,
			read.Length,
			&ioStatusBlock
			);

		ASSERT(ioStatusBlock.Status == STATUS_SUCCESS);
	}

#if 0
	do	// just for structural programing
	{
		BOOLEAN						lookupResult;
		PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;	
	 
		PSTANDARD_INFORMATION		standardInformation;
		LARGE_INTEGER				currentByteOffset;
		LARGE_INTEGER				lastUpdateTime;

		
		break;

		if(fcb->FileRecordSegmentHeaderAvail == FALSE)
			break;

		if(fileExt->TypeOfOpen != UserFileOpen)
			break;

		if(read.Length == 0)
			break;

		if(read.Key != 0)
			break;

		currentByteOffset.QuadPart = read.ByteOffset.QuadPart;

		lookupResult = 	NtfsLookupInFileRecord(
							Secondary,
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$STANDARD_INFORMATION,
							NULL,
							&attributeRecordHeader
							);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("RedirectIrpMajorRead: LookupAllocation lookupResult = %d\n", lookupResult));
		
		if(lookupResult == FALSE || attributeRecordHeader->FormCode != RESIDENT_FORM)
		{
			ASSERT(LFS_UNEXPECTED);
			break;
		}

		standardInformation = (PSTANDARD_INFORMATION)((PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset);

		//SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
		//		("standardInformation->LastAccessTime = %I64d, standardInformation->LastModificationTime = %I64d %I64d\n", 
		//			standardInformation->LastAccessTime, standardInformation->LastModificationTime, standardInformation->LastAccessTime - standardInformation->LastModificationTime));

		//ASSERT(standardInformation->LastAccessTime > standardInformation->LastModificationTime);
		//ASSERT(standardInformation->LastAccessTime > standardInformation->LastChangeTime);
		
		lastUpdateTime.QuadPart 
			= (standardInformation->LastModificationTime > standardInformation->LastChangeTime)
			? standardInformation->LastModificationTime : standardInformation->LastChangeTime;

		//ASSERT(fcb->OpenTime.QuadPart >= ((lastUpdateTime.QuadPart >> 24) << 24)); // It can be occured
			
		if(fcb->OpenTime.QuadPart < lastUpdateTime.QuadPart
			|| fcb->OpenTime.QuadPart - lastUpdateTime.QuadPart < 600*HZ)
			break;

		lookupResult = 	NtfsLookupInFileRecord(
							Secondary,
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$DATA,
							NULL,
							&attributeRecordHeader
							);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("RedirectIrpMajorRead: LookupAllocation lookupResult = %d\n", lookupResult)); 		

		if(lookupResult == FALSE)
		{
			ASSERT(LFS_UNEXPECTED);
			break;
		}
		
		if(NtfsLookupInFileRecord(
						Secondary, 
						(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
						$DATA, attributeRecordHeader, NULL) == TRUE
						)
		{
			break;
		}

		if(attributeRecordHeader->FormCode == RESIDENT_FORM)
		{
			PCHAR	fileData;
			ULONG	remainFileLength;
			ULONG	readLength;


			break;
			
			if(BooleanFlagOn(attributeRecordHeader->Form.Resident.ResidentFlags, RESIDENT_FORM_INDEXED))
				break;

			if(read.ByteOffset.HighPart != 0)
			{
				ASSERT(LFS_UNEXPECTED);
				break;
			}

			if(attributeRecordHeader->Form.Resident.ValueLength == 0)
				break;
			
			if(attributeRecordHeader->Form.Resident.ValueLength <= read.ByteOffset.LowPart)
			{
				ASSERT(totalReadRequestLength == 0);
				if(nonCachedIo || pagingIo)
				{
					endOfFile = TRUE;		
					break;
				}

				//ExReleaseFastMutex(&Secondary->FastMutex);
				*FastMutexSet = FALSE;
				*Retry		  = FALSE;
			
				lastStatus = STATUS_END_OF_FILE;
				returnStatus = STATUS_SUCCESS;
				goto ReadOut;
			}
			
			fileData = (PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset;
			fileData += read.ByteOffset.LowPart;

			remainFileLength = attributeRecordHeader->Form.Resident.ValueLength - read.ByteOffset.LowPart;

			readLength = (read.Length <= remainFileLength)
							? read.Length : remainFileLength;

			ASSERT(readLength);
			ASSERT(totalReadRequestLength <= read.Length);
									
			RtlCopyMemory(
				(PCHAR)outputBuffer + totalReadRequestLength,
				fileData,
				readLength
				);			
						
			totalReadRequestLength += readLength;
			currentByteOffset.QuadPart += readLength;

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("RedirectIrpMajorRead: RESIDENT_FORM totalReadRequestLength = %d\n",
						totalReadRequestLength));
			PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);
			
			//ExReleaseFastMutex(&Secondary->FastMutex);
			*FastMutexSet = FALSE;
			*Retry		  = FALSE;
			
			returnStatus = STATUS_SUCCESS;
			
			//Irp->IoStatus.Information = readLength;
			//totalReadRequestLength = 0;
			//checkResult = TRUE;
			//break;

			goto ReadOut;
		}

		//PrintIrp(LFS_DEBUG_SECONDARY_INFO, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);

		ASSERT(attributeRecordHeader->Form.Nonresident.LowestVcn == 0);
		ASSERT(attributeRecordHeader->FormCode == NONRESIDENT_FORM);
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("RedirectIrpMajorRead: read.Length = %d, read.Key = %d, %d\n", 
					read.Length, read.Key, read.ByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector != 0));
		
		//if(currentByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector
		//		|| read.Length % Secondary->Thread.SessionContext.BytesPerSector)
		//	break;

		//if(!(pagingIo || nonCachedIo) && read.ByteOffset.QuadPart + read.Length > attributeRecordHeader->Form.Nonresident.FileSize)
		//	break;

		if(attributeRecordHeader->Form.Nonresident.FileSize <= 10*1024*1024)
			break;

		if(pagingIo || nonCachedIo)
		{
			ASSERT(read.ByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector == 0);
			ASSERT(read.Length % Secondary->Thread.SessionContext.BytesPerSector == 0);
		}

		if(attributeRecordHeader->Form.Nonresident.FileSize <= read.ByteOffset.QuadPart)
		{
			ASSERT(totalReadRequestLength == 0);
			if(nonCachedIo || pagingIo)
			{
				endOfFile = TRUE;		
				break;
			}

			//ExReleaseFastMutex(&Secondary->FastMutex);
			*FastMutexSet = FALSE;
			*Retry		  = FALSE;
			
			lastStatus = STATUS_END_OF_FILE;
			returnStatus = STATUS_SUCCESS;
			goto ReadOut;
		}

		do 
		{
			VCN				vcn;
			VCN				startVcn;
			LCN				lcn;
			LONGLONG		clusterCount;
			LONGLONG		remainDataSizeInClulsters;
			ULONG			readLength;
			LONGLONG		remainFileLength;
			LARGE_INTEGER	startingOffset;
			LONGLONG		currentSector;
			NTSTATUS		readStatus;
	
			vcn = LlClustersFromBytesTruncate(
					Secondary->Thread.SessionContext.ClusterShift,
					currentByteOffset.QuadPart
					);

			lookupResult = LookupAllocation(
								Secondary,
								attributeRecordHeader,
								vcn,
								&startVcn,
								&lcn,
								&clusterCount
								);
			
			if(lookupResult == FALSE)
			{
				ASSERT(LFS_UNEXPECTED);
				break;
			}

			remainDataSizeInClulsters 
				= (clusterCount << Secondary->Thread.SessionContext.ClusterShift) 
					- (currentByteOffset.QuadPart - (startVcn << Secondary->Thread.SessionContext.ClusterShift));

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("RedirectIrpMajorRead: %I64d, %I64d, %I64d\n",
					(clusterCount << Secondary->Thread.SessionContext.ClusterShift),
					(startVcn << Secondary->Thread.SessionContext.ClusterShift),
					(currentByteOffset.QuadPart - (startVcn << Secondary->Thread.SessionContext.ClusterShift))));

			if((read.Length - totalReadRequestLength) <= remainDataSizeInClulsters)
			{
				readLength = read.Length - totalReadRequestLength;
			}
			else
			{
				readLength = (ULONG) remainDataSizeInClulsters;
			}

			remainFileLength = attributeRecordHeader->Form.Nonresident.FileSize - currentByteOffset.QuadPart;
			
			readLength = (readLength <= remainFileLength) ? readLength : (ULONG)remainFileLength;

			ASSERT(readLength);
					
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("RedirectIrpMajorRead: %ws LookupAllocation lookupResult = %d, vcn = %I64x, lcn = %I64x, ClusterCount = %I64x\n",
					&fcb->FullFileName, lookupResult, vcn, lcn, clusterCount));
				
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, 
					("read->Length = %d %s\n",
					read.Length, ((clusterCount << Secondary->Thread.SessionContext.ClusterShift) >= read.Length) 
					? "included" : "not included"));

			PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);
	
			currentSector = LlClustersFromBytesTruncate(
								Secondary->Thread.SessionContext.SectorShift,
								currentByteOffset.QuadPart
								);

			startingOffset.QuadPart = lcn << Secondary->Thread.SessionContext.ClusterShift;
			startingOffset.QuadPart 
				+= ((currentSector << Secondary->Thread.SessionContext.SectorShift) 
					- (startVcn << Secondary->Thread.SessionContext.ClusterShift));
							
			//if(pagingIo || nonCachedIo)
			//{
			//	ASSERT(currentByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector == 0);
			//	if(currentByteOffset.QuadPart + readLength >= attributeRecordHeader->Form.Nonresident.FileSize)
			//	{
			//		readLength = ClustersFromBytes(
			//						Secondary->Thread.SessionContext.SectorShift,
			//						Secondary->Thread.SessionContext.SectorMask,
			//						readLength
			//						);
			//		readLength <<= Secondary->Thread.SessionContext.SectorShift;				
			//		ASSERT(readLength);
			//	}
			//}

			ASSERT(totalReadRequestLength + readLength <= read.Length);
			
			if(currentByteOffset.QuadPart != (currentSector << Secondary->Thread.SessionContext.SectorShift))
			{
				ULONG	sectorOffset;

				ASSERT(currentByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector);

				sectorOffset = (ULONG)(currentByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector);

				ASSERT(sectorOffset);

				readLength = (readLength <= Secondary->Thread.SessionContext.BytesPerSector - sectorOffset)
						? readLength : (Secondary->Thread.SessionContext.BytesPerSector - sectorOffset);
				
				ASSERT(totalReadRequestLength == 0);
				
				readStatus = ReadBuffer(
								Secondary,
								&startingOffset,
								Secondary->Thread.SessionContext.BytesPerSector,
								&fileExt->Cache[0]
								);
			
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
					break;
				}
				
				RtlCopyMemory(
					(PCHAR)outputBuffer + totalReadRequestLength,
					&fileExt->Cache[sectorOffset],
					readLength
					);
			}
			else if(readLength >= Secondary->Thread.SessionContext.BytesPerSector)
			{
				readLength >>= Secondary->Thread.SessionContext.SectorShift;
				readLength <<= Secondary->Thread.SessionContext.SectorShift;
				
				readStatus = ReadBuffer(
								Secondary,
								&startingOffset,
								readLength,
								(PCHAR)outputBuffer + totalReadRequestLength
								);

				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
					break;
				}
			}
			else
			{
				readStatus = ReadBuffer(
								Secondary,
								&startingOffset,
								Secondary->Thread.SessionContext.BytesPerSector,
								&fileExt->Cache[0]
								);
			
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
					break;
				}
				
				RtlCopyMemory(
					(PCHAR)outputBuffer + totalReadRequestLength,
					&fileExt->Cache[0],
					readLength
					);
			}

			totalReadRequestLength += readLength;
			currentByteOffset.QuadPart += readLength;

			ASSERT(totalReadRequestLength <= read.Length);
			ASSERT(currentByteOffset.QuadPart <= attributeRecordHeader->Form.Nonresident.FileSize);

			//if((nonCachedIo || pagingIo)
			//	&& currentByteOffset.QuadPart >= attributeRecordHeader->Form.Nonresident.FileSize)
			//{
			//	totalReadRequestLength = (ULONG)(attributeRecordHeader->Form.Nonresident.FileSize - read.ByteOffset.QuadPart);
			//	currentByteOffset.QuadPart = attributeRecordHeader->Form.Nonresident.FileSize;

			//	RtlZeroMemory(
			//		(PCHAR)outputBuffer + (ULONG)(attributeRecordHeader->Form.Nonresident.FileSize - read.ByteOffset.QuadPart),
			//		(ULONG)(currentByteOffset.QuadPart - attributeRecordHeader->Form.Nonresident.FileSize)
			//		);

			//	break;
			//}
		
		} while(totalReadRequestLength < read.Length 
				&& currentByteOffset.QuadPart < attributeRecordHeader->Form.Nonresident.FileSize);

		if(totalReadRequestLength
			&& (totalReadRequestLength == read.Length
				|| currentByteOffset.QuadPart == attributeRecordHeader->Form.Nonresident.FileSize))
		{
			//ExReleaseFastMutex(&Secondary->FastMutex);
			*FastMutexSet = FALSE;
			*Retry		  = FALSE;
			
			returnStatus = STATUS_SUCCESS;

			//Irp->IoStatus.Information = totalReadRequestLength;
			//totalReadRequestLength = 0;
			//checkResult = TRUE;
			//break;

			goto ReadOut;
		}
		
	} while(0);
	
	ASSERT(totalReadRequestLength == 0);

#endif

	do
	{
		ULONG						outputBufferLength;
		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			

		outputBufferLength = ((read.Length-totalReadRequestLength) <= Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
								? (read.Length-totalReadRequestLength) : Secondary->Thread.SessionContext.SecondaryMaxDataSize;

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_READ,
								outputBufferLength
								);

		if(secondaryRequest == NULL)
		{
			ASSERT(LFS_REQUIRED);

			returnStatus = STATUS_INSUFFICIENT_RESOURCES;	
			break;
		}

		secondaryRequest->FileExt = fileExt;
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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				Irp,
				irpSp,
				fileExt->PrimaryFileHandle
				);

		//ndfsWinxpRequestHeader->IrpTag |= ~(IRP_PAGING_IO | IRP_NOCACHE);

		ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
		ndfsWinxpRequestHeader->Read.Key		= read.Key;
		ndfsWinxpRequestHeader->Read.ByteOffset = read.ByteOffset.QuadPart+totalReadRequestLength;

		if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
			Secondary,
			FastMutexSet,
			fileExt,
			Retry,
			secondaryRequest,
			secondaryRequest->SessionId
			) != STATUS_SUCCESS)
		{
			return STATUS_UNSUCCESSFUL;
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
			returnStatus = STATUS_TIMEOUT;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

		totalReadRequestLength += ndfsWinxpReplytHeader->Information;
		lastStatus = ndfsWinxpReplytHeader->Status;
		returnStatus = STATUS_SUCCESS;

		if(lastStatus != STATUS_SUCCESS)
		{
			ASSERT(ndfsWinxpReplytHeader->Information == 0);
			DereferenceSecondaryRequest(secondaryRequest);
			secondaryRequest = NULL;				
			
			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			*FastMutexSet = FALSE;
		
			break;
		}

		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("%d %d %d\n", 
					nonCachedIo, pagingIo, synchronousIo));

		ASSERT(ADD_ALIGN8(ndfsWinxpReplytHeader->Information) == ADD_ALIGN8(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)));
		ASSERT(ndfsWinxpReplytHeader->Information <= secondaryRequest->OutputBufferLength);
		ASSERT(secondaryRequest->OutputBufferLength == 0 || secondaryRequest->OutputBuffer);

		if(ndfsWinxpReplytHeader->Information)
		{
			if(checkResult)
			{	
				ULONG i;
				for(i=0; i<ndfsWinxpReplytHeader->Information; i++)
				{
					if(secondaryRequest->OutputBuffer[i] != ((_U8 *)(ndfsWinxpReplytHeader+1))[i])
						ASSERT(LFS_UNEXPECTED);
				}
			}
			RtlCopyMemory(
				secondaryRequest->OutputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				ndfsWinxpReplytHeader->Information
				);
		}

//		ASSERT(Irp->PendingReturned == FALSE) ;

#if 0
		if(ndfsWinxpReplytHeader->Information != outputBufferLength) 
		{
			DereferenceSecondaryRequest(secondaryRequest);

			secondaryRequest = NULL;
			ExReleaseFastMutex(&Secondary->FastMutex);
			*FastMutexSet = FALSE;
			break; // Maybe End of File
		}
#endif

		DereferenceSecondaryRequest(secondaryRequest);
		secondaryRequest = NULL;

		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;

	} while(totalReadRequestLength < read.Length);

//ReadOut:

	if(checkResult)
	{
		ASSERT(Irp->IoStatus.Information == totalReadRequestLength);
	}

	if(endOfFile == TRUE)
	{
		ASSERT(totalReadRequestLength == 0 && lastStatus == STATUS_END_OF_FILE);
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("%d %d read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
					nonCachedIo, pagingIo, read.ByteOffset.QuadPart, read.Length, totalReadRequestLength, lastStatus));

		PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);
	}

	secondaryRequest = NULL;
	if(returnStatus != STATUS_SUCCESS)
		return returnStatus;


#if 0	
	if (synchronousIo && !pagingIo) 
	{
		ASSERT(fileObject->CurrentByteOffset.QuadPart == read.ByteOffset.QuadPart);
		fileObject->CurrentByteOffset.QuadPart += totalReadRequestLength;
	}
#endif
	
	if(totalReadRequestLength)
	{
#if 0
		if(nonCachedIo || pagingIo)
		{
			Irp->IoStatus.Information = read.Length;
			Irp->IoStatus.Status = STATUS_SUCCESS;
		}
		else
#endif
		{
			Irp->IoStatus.Information = totalReadRequestLength;
			Irp->IoStatus.Status = STATUS_SUCCESS;
		}
	}
	else
	{
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = lastStatus;
	}

	//if(lastStatus == STATUS_END_OF_FILE)
	//	ASSERT(totalReadRequestLength == 0);

	if(Irp->IoStatus.Status == STATUS_SUCCESS && totalReadRequestLength != read.Length)
	{
		RtlZeroMemory(
			(PCHAR)outputBuffer + totalReadRequestLength,
			read.Length - totalReadRequestLength
			//((read.Length - totalReadRequestLength) <= 10) ? (read.Length - totalReadRequestLength) : 10
			);
	}

	if (Irp->IoStatus.Status == STATUS_SUCCESS && synchronousIo && !pagingIo) 
	//if (synchronousIo || (!nonCachedIo && !pagingIo))
	{
		fileObject->CurrentByteOffset.QuadPart = read.ByteOffset.QuadPart + totalReadRequestLength;
	}

	if(fileObject->SectionObjectPointer == NULL)
		fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

	returnStatus = STATUS_SUCCESS;

	if(Irp->IoStatus.Status != STATUS_SUCCESS)
	{
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("%d %d read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
					nonCachedIo, pagingIo, read.ByteOffset.QuadPart, read.Length, totalReadRequestLength, lastStatus));

		PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrpMajorRead", Secondary->LfsDeviceExt, Irp);
	}

	return STATUS_SUCCESS;
}
	

NTSTATUS
RedirectIrpMajorQueryInformation(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	)
{
	NTSTATUS					returnStatus;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	PLFS_FCB					fcb = fileObject->FsContext;
	PFILE_EXTENTION				fileExt = fileObject->FsContext2;

	struct QueryFile			queryFile;

	PVOID						inputBuffer = NULL;
	ULONG						inputBufferLength = 0;
	PVOID						outputBuffer = MapOutputBuffer(Irp);
	ULONG						outputBufferLength;

	BOOLEAN						checkResult = FALSE;

	PSECONDARY_REQUEST			secondaryRequest;	
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

	LARGE_INTEGER				timeOut;
	NTSTATUS					waitStatus;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	_U32						returnedDataSize;


	queryFile.FileInformationClass	= irpSp->Parameters.QueryFile.FileInformationClass;
	queryFile.Length				= irpSp->Parameters.QueryFile.Length;
	outputBufferLength				= queryFile.Length;


	*Retry = FALSE;
	
	if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
			Secondary,
			FastMutexSet,
			fileExt,
			Retry,
			NULL,
			Secondary->SessionId
			) != STATUS_SUCCESS)
	{
		return STATUS_UNSUCCESSFUL;
	}
	
	do	// just for structural programming
	{
		break;

		if(fcb->FileRecordSegmentHeaderAvail == FALSE)
			break;

		if(queryFile.FileInformationClass == FileBasicInformation)
		{
			PFILE_BASIC_INFORMATION		fileBasicInformation = outputBuffer;

			BOOLEAN						lookupResult;
			PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;		 
			PSTANDARD_INFORMATION		standardInformation;


			if(outputBufferLength < sizeof(FILE_BASIC_INFORMATION))
				break;

			lookupResult = 	NtfsLookupInFileRecord(
								Secondary,
								(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
								$STANDARD_INFORMATION,
								NULL,
								&attributeRecordHeader
								);

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrpMajorQueryInformation: LookupAllocation lookupResult = %d\n", lookupResult));
		
			if(lookupResult == FALSE || attributeRecordHeader->FormCode != RESIDENT_FORM)
			{
				ASSERT(LFS_UNEXPECTED);
			}

			standardInformation = (PSTANDARD_INFORMATION)((PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset);

			fileBasicInformation->CreationTime.QuadPart		= standardInformation->CreationTime;
			fileBasicInformation->LastAccessTime.QuadPart	= standardInformation->LastAccessTime;
			fileBasicInformation->LastWriteTime.QuadPart	= standardInformation->LastModificationTime;
			fileBasicInformation->ChangeTime.QuadPart		= standardInformation->LastChangeTime;
			fileBasicInformation->FileAttributes			= standardInformation->FileAttributes;

			ASSERT(standardInformation->LastAccessTime >= standardInformation->CreationTime);
			
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("RedirectIrpMajorQueryInformation: fileBasicInformation->LastAccessTime.LowPart = %u, fileBasicInformation->LastAccessTime.LowPart = %u\n",
							fileBasicInformation->LastAccessTime.LowPart, fileBasicInformation->LastAccessTime.HighPart));

			if(standardInformation->LastAccessTime < standardInformation->LastModificationTime)
				fileBasicInformation->LastAccessTime.QuadPart = standardInformation->LastModificationTime;
			if(standardInformation->LastAccessTime < standardInformation->LastChangeTime)
				fileBasicInformation->LastAccessTime.QuadPart = standardInformation->LastChangeTime;
	
			if (fileBasicInformation->FileAttributes == 0) 
			{
		        fileBasicInformation->FileAttributes = FILE_ATTRIBUTE_NORMAL;
			}

			Irp->IoStatus.Status	  = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(FILE_BASIC_INFORMATION); 
			
			//goto queryInformationOut;						
			checkResult = TRUE;
		}
		else if(queryFile.FileInformationClass == FileStandardInformation)
		{
			BOOLEAN						lookupResult;
			PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;	
			PSTANDARD_INFORMATION		standardInformation;
	 
			ULONGLONG					allocationSize;
			ULONGLONG					endOfFile;
			BOOLEAN						fountEntry;
			ULONG						linkCount;

			PFILE_STANDARD_INFORMATION fileStandardInformation = outputBuffer;


			if(outputBufferLength < sizeof(STANDARD_INFORMATION))
				break;

			lookupResult = 	NtfsLookupInFileRecord(
								Secondary,
								(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
								$DATA,
								NULL,
								&attributeRecordHeader
								);

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrpMajorRead: LookupAllocation lookupResult = %d\n", lookupResult));
		
			if(lookupResult == FALSE)
			{
				ASSERT(LFS_UNEXPECTED);
				break;
			}

			if(NtfsLookupInFileRecord(
							Secondary, 
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$DATA, attributeRecordHeader, NULL) == TRUE
							)
			{
				break;
			}

			if(attributeRecordHeader->FormCode == RESIDENT_FORM)
			{
				endOfFile = attributeRecordHeader->Form.Resident.ValueLength;
				allocationSize = QuadAlign(attributeRecordHeader->Form.Resident.ValueLength);
#if 0				
				allocationSize = LlClustersFromBytes(
									Secondary->Thread.SessionContext.ClusterShift,
									Secondary->Thread.SessionContext.ClusterMask,
									endOfFile
									);
				allocationSize <<= Secondary->Thread.SessionContext.ClusterShift;
#endif
			}
			else if(attributeRecordHeader->FormCode == NONRESIDENT_FORM)
			{
				endOfFile = attributeRecordHeader->Form.Nonresident.FileSize;
				allocationSize = attributeRecordHeader->Form.Nonresident.AllocatedLength;
			}

			lookupResult = 	NtfsLookupInFileRecord(
								Secondary,
								(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
								$STANDARD_INFORMATION,
								NULL,
								&attributeRecordHeader
								);

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrpMajorRead: LookupAllocation lookupResult = %d\n", lookupResult));
		
			if(lookupResult == FALSE || attributeRecordHeader->FormCode != RESIDENT_FORM)
			{
				ASSERT(LFS_UNEXPECTED);
				break;
			}

			standardInformation = 
				(PSTANDARD_INFORMATION)((PCHAR)attributeRecordHeader 
					+ attributeRecordHeader->Form.Resident.ValueOffset);

			
			linkCount = 0;
			fountEntry = FALSE;
			
			do 
			{
				PFILE_NAME fileName;
				
				fountEntry = NtfsLookupInFileRecord(
								Secondary,
								(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
								$FILE_NAME,
								attributeRecordHeader,
								&attributeRecordHeader
								);
				if(fountEntry == FALSE)
					break;

				if(attributeRecordHeader->FormCode != RESIDENT_FORM)
				{
					ASSERT(LFS_REQUIRED);
					fountEntry = FALSE;
				}

				if(attributeRecordHeader->Form.Resident.ValueLength)
					fileName = (PFILE_NAME)((PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset);
				else
					fileName = NULL;
				
				if(fileName && fileName->Flags != FILE_NAME_DOS)
					linkCount++; 
			}while(1);
	
			if(fountEntry == FALSE)
				break;
		
			fileStandardInformation->AllocationSize.QuadPart	= allocationSize;
			fileStandardInformation->EndOfFile.QuadPart			= endOfFile;
			fileStandardInformation->NumberOfLinks				= linkCount;
			fileStandardInformation->DeletePending				= fcb->DeletePending;
			fileStandardInformation->Directory					
				= BooleanFlagOn(((PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer)->Flags, FILE_FILE_NAME_INDEX_PRESENT);

			Irp->IoStatus.Status	  = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION); 

			goto queryInformationOut;						
			checkResult = TRUE;
		}
		else if(queryFile.FileInformationClass == FileNameInformation)
		{
		}
		else if(queryFile.FileInformationClass == FileAllInformation)
		{
		}
		else if(queryFile.FileInformationClass == FileStreamInformation)
		{
			BOOLEAN						lookupResult;
			PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;	
	 
			PFILE_STREAM_INFORMATION	fileStreamInformation = outputBuffer;

			ULONG						thisLength;
			ULONG						nameLength;
			PWCHAR						streamName;
			

			if(outputBufferLength < sizeof(STANDARD_INFORMATION))
				break;

			lookupResult = 	NtfsLookupInFileRecord(
								Secondary,
								(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
								$DATA,
								NULL,
								&attributeRecordHeader
								);

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						("RedirectIrpMajorRead: LookupAllocation lookupResult = %d\n", lookupResult));
		
			if(lookupResult == FALSE)
			{
				ASSERT(LFS_UNEXPECTED);
				break;
			}

			if(NtfsLookupInFileRecord(
							Secondary, 
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$DATA, attributeRecordHeader, NULL) == TRUE
							)
			{
				break;
			}

			nameLength = ((2 + attributeRecordHeader->NameLength) * sizeof(WCHAR))
                             + (sizeof(L"$DATA") - sizeof(WCHAR));

			thisLength = FIELD_OFFSET(FILE_STREAM_INFORMATION, StreamName[0]) + nameLength;

			if(outputBufferLength < thisLength)
				break;

			fileStreamInformation->NextEntryOffset = 0;
            if (attributeRecordHeader->FormCode == RESIDENT_FORM) 
			{
				fileStreamInformation->StreamSize.QuadPart = attributeRecordHeader->Form.Resident.ValueLength;
				fileStreamInformation->StreamAllocationSize.QuadPart 
					= QuadAlign(attributeRecordHeader->Form.Resident.ValueLength);
			} else 
			{
				fileStreamInformation->StreamSize.QuadPart = attributeRecordHeader->Form.Nonresident.FileSize;
				fileStreamInformation->StreamAllocationSize.QuadPart 
					= attributeRecordHeader->Form.Nonresident.AllocatedLength;
			}
			
			fileStreamInformation->StreamNameLength = nameLength;
			
			streamName = (PWCHAR) fileStreamInformation->StreamName;

			*streamName = L':';
			streamName += 1;

			RtlCopyMemory(
				streamName,
                (PCHAR)attributeRecordHeader + attributeRecordHeader->NameOffset,
                attributeRecordHeader->NameLength * sizeof(WCHAR)
				);

			streamName += attributeRecordHeader->NameLength;
            
			*streamName = L':';
            streamName += 1;

            RtlCopyMemory(
				streamName,
                L"$DATA",
				sizeof(L"$DATA") - sizeof(WCHAR)
				);

			Irp->IoStatus.Status	  = STATUS_SUCCESS;
			Irp->IoStatus.Information = thisLength;

			goto queryInformationOut;						
			checkResult = TRUE;
		}
		else if(queryFile.FileInformationClass == FileNetworkOpenInformation)
		{
		}
		else if(queryFile.FileInformationClass == FileAttributeTagInformation)
		{
		}
		else
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
				("RedirectIrpMajorQueryInformation: queryFile.FileInformationClass = %d\n", queryFile.FileInformationClass) ) ;
			
	} while(0);

	//ExReleaseFastMutex(&Secondary->FastMutex);
	KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

	*FastMutexSet = FALSE;

	ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
	secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
							Secondary,
							IRP_MJ_QUERY_INFORMATION,
							outputBufferLength
							);

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
	INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
		ndfsWinxpRequestHeader,
		Irp,
		irpSp,
		fileExt->PrimaryFileHandle
		);
	
	ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
	ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

	if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
			Secondary,
			FastMutexSet,
			fileExt,
			Retry,
			secondaryRequest,
			secondaryRequest->SessionId
			) != STATUS_SUCCESS)
	{
		return STATUS_UNSUCCESSFUL;
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
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;

		return STATUS_TIMEOUT;
	}

	if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
	{
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			("RedirectIrpMajorQueryInformation: secondaryRequest->ExecuteStatus = %x, Retry = %d\n",
				secondaryRequest->ExecuteStatus, *Retry));
		returnStatus = secondaryRequest->ExecuteStatus;	
		DereferenceSecondaryRequest(secondaryRequest);
		secondaryRequest = NULL;

		return returnStatus;
	}

	ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

	Irp->IoStatus.Status	  = ndfsWinxpReplytHeader->Status;
	Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information; 
	
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
			("RedirectIrpMajorQueryInformation: Irp->IoStatus.Status = %d, Irp->IoStatus.Information = %d\n",
				Irp->IoStatus.Status, Irp->IoStatus.Information));

	returnedDataSize 
		= secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

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
			(_U8 *)(ndfsWinxpReplytHeader+1),
			(returnedDataSize < queryFile.Length) ? returnedDataSize : queryFile.Length
			);
	}
		
	DereferenceSecondaryRequest(secondaryRequest);

queryInformationOut:

	//ExReleaseFastMutex(&Secondary->FastMutex);
	KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

	*FastMutexSet = FALSE;
	*Retry = FALSE;

	return STATUS_SUCCESS;
}

	
NTSTATUS
RedirectIrp(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	)
{
	NTSTATUS			returnStatus = DBG_CONTINUE; // for debbuging
	
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject;

	PLFS_FCB			fcb = NULL;
	PFILE_EXTENTION		fileExt = NULL;

	PSECONDARY_REQUEST	secondaryRequest = NULL;

	
	*FastMutexSet = FALSE;
	*Retry		  = FALSE;
	

	ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL) ;

	fileObject = irpSp->FileObject;
 
	if(irpSp->MajorFunction != IRP_MJ_CREATE)
	{
		fcb = fileObject->FsContext;
		fileExt = fileObject->FsContext2;
	
		ASSERT(Secondary_LookUpFileExtension(Secondary, fileObject) == fileExt);
		ASSERT(fileExt->Fcb == fcb);
 
		if(irpSp->MajorFunction != IRP_MJ_CLOSE && irpSp->MajorFunction != IRP_MJ_CLEANUP)
		{
			if(fileExt->Corrupted == TRUE)
			{			
				Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Irp->IoStatus.Information = 0;

				return STATUS_SUCCESS;
			}
		}
	}


	switch(irpSp->MajorFunction) 
	{
    case IRP_MJ_CREATE: //0x00
	{
		returnStatus = RedirectIrpMajorCreate(Secondary, Irp, FastMutexSet, Retry);
		break;
	}

	case IRP_MJ_CLOSE: // 0x02
	{
		BOOLEAN						fcbQueueEmpty;
			
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

		KIRQL						oldIrql;	
		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;

		//fcb->FileRecordSegmentHeaderAvail = FALSE;

		if(fileExt->Corrupted == TRUE)
		{
			fileObject->SectionObjectPointer = NULL;
			KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);
			if(InterlockedDecrement(&fcb->OpenCount) == 0)
			{
				RemoveEntryList(&fcb->ListEntry);
				InitializeListHead(&fcb->ListEntry) ;
			}
		    fcbQueueEmpty = IsListEmpty(&Secondary->FcbQueue);
			KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);
			Secondary_DereferenceFcb(fcb);
	
#if 0
			//ASSERT(Secondary->VolumeRootFileHandle);
			if(fcbQueueEmpty)
			{
				if(Secondary->VolumeRootFileHandle == NULL)
					CloseVolumeRootFile(Secondary->VolumeRootFileHandle);
				Secondary->VolumeRootFileHandle = NULL;
			}
#endif

			ExAcquireFastMutex(&Secondary->FileExtQMutex);
			RemoveEntryList(&fileExt->ListEntry);
		    ExReleaseFastMutex(&Secondary->FileExtQMutex);

			InitializeListHead(&fileExt->ListEntry) ;
			FreeFileExt(Secondary, fileExt);

			fileObject->FsContext = NULL;
			fileObject->FsContext2 = NULL;
		
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				( "DispatchReply: IRP_MJ_CLOSE: free a corrupted file extension:%p\n", fileObject)); 		

			secondaryRequest = NULL;		
			*FastMutexSet = FALSE;
			*Retry = FALSE;
		
			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			returnStatus = STATUS_SUCCESS;

			break;
		}		

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
							Secondary,
							IRP_MJ_CLOSE,
							0
							);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);
	
		if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
			Secondary,
			FastMutexSet,
			fileExt,
			Retry,
			secondaryRequest,
			secondaryRequest->SessionId
			) != STATUS_SUCCESS)
		{
			return STATUS_UNSUCCESSFUL;
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
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			
			*FastMutexSet = FALSE;

			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			//ASSERT(LFS_BUG);
			returnStatus = secondaryRequest->ExecuteStatus;	
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			
			secondaryRequest = NULL;
	
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;

		fileObject->SectionObjectPointer = NULL;

		KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);
		if(InterlockedDecrement(&fcb->OpenCount) == 0)
		{
			RemoveEntryList(&fcb->ListEntry);
			InitializeListHead(&fcb->ListEntry) ;
		    fcbQueueEmpty = IsListEmpty(&Secondary->FcbQueue);
		}
		KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);
		Secondary_DereferenceFcb(fcb);
	
#if 0
		//ASSERT(Secondary->VolumeRootFileHandle);
		if(fcbQueueEmpty)
		{
			if(Secondary->VolumeRootFileHandle != NULL)
				CloseVolumeRootFile(Secondary->VolumeRootFileHandle);
			Secondary->VolumeRootFileHandle = NULL;
		}
#endif
		
//		KeAcquireSpinLock(&Secondary->FileExtQSpinLock, &oldIrql);
	    ExAcquireFastMutex(&Secondary->FileExtQMutex);
		RemoveEntryList(&fileExt->ListEntry);
//		KeReleaseSpinLock(&Secondary->FileExtQSpinLock, oldIrql);
	    ExReleaseFastMutex(&Secondary->FileExtQMutex);

		InitializeListHead(&fileExt->ListEntry) ;

		FreeFileExt(Secondary, fileExt);

		fileObject->FsContext = NULL;
		fileObject->FsContext2 = NULL;
		
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
			( "DispatchReply: IRP_MJ_CLOSE: free a file extension:%p\n", fileObject)); 		
			
		DereferenceSecondaryRequest(
					secondaryRequest
					);
				
		secondaryRequest = NULL;

		returnStatus = STATUS_SUCCESS;
		
		//ExReleaseFastMutex(&Secondary->FastMutex) ;
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;

#if DBG
		if(!LfsObjectCounts.FileExtCount)
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				( "DispatchReply: IRP_MJ_CLOSE: LfsObjectCounts.FileExtCount= %d\n", LfsObjectCounts.FileExtCount)); 
#endif
		break;
	}

    case IRP_MJ_READ: // 0x03
	{
		secondaryRequest = NULL;
		returnStatus = RedirectIrpMajorRead(Secondary, Irp, FastMutexSet, Retry);
		break;
	}

    case IRP_MJ_WRITE: // 0x04
	{
		struct Write		write;

		PVOID				inputBuffer = MapInputBuffer(Irp);
		PVOID				outputBuffer = NULL;
		ULONG				outputBufferLength = 0;

		BOOLEAN				synchronousIo = BooleanFlagOn(fileObject->Flags, FO_SYNCHRONOUS_IO);
		BOOLEAN				pagingIo      = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
		BOOLEAN				writeToEof;

		//LARGE_INTEGER		currentByteOffset;
		//ULONG				totalWriteRequestLength;
		IO_STATUS_BLOCK		ioStatus;
		LONGLONG			lastCurrentByteOffset;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		write.ByteOffset	= irpSp->Parameters.Write.ByteOffset;
		write.Key			= irpSp->Parameters.Write.Key;
		write.Length		= irpSp->Parameters.Write.Length;


		ASSERT(!(write.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& write.ByteOffset.HighPart == -1));

		writeToEof = ((write.ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE) 
						&& (write.ByteOffset.HighPart == -1) );


		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("WRITE: Offset:%I64d Length:%d\n", write.ByteOffset.QuadPart, write.Length)) ;
#if 0
//		ASSERT(write->Length != 800);
		if(synchronousIo && !pagingIo)
		{
			if(/*read->ByteOffset.QuadPart == 0
				|| */(read->ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& read->ByteOffset.HighPart == -1))
			{
				currentByteOffset.QuadPart = fileObject->CurrentByteOffset.QuadPart;
			} else
				currentByteOffset.QuadPart = read->ByteOffset.QuadPart;
		} else
			currentByteOffset.QuadPart = read->ByteOffset.QuadPart;
		if(writeToEof)
		{
			currentByteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;
			currentByteOffset.HighPart = -1;
		}
		else
			currentByteOffset.QuadPart = write.ByteOffset.QuadPart;
#endif

		ioStatus.Information = 0;

		do
		{
			ULONG						inputBufferLength;
			_U8							*ndfsWinxpRequestData;
	
			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			

			inputBufferLength = (write.Length-ioStatus.Information <= Secondary->Thread.SessionContext.PrimaryMaxDataSize) 
								? (write.Length-ioStatus.Information) : Secondary->Thread.SessionContext.PrimaryMaxDataSize;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_WRITE,
								inputBufferLength
								);

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
			INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				Irp,
				irpSp,
				fileExt->PrimaryFileHandle
				);

			//ndfsWinxpRequestHeader->IrpTag |= ~(IRP_PAGING_IO | IRP_NOCACHE);
			
			ndfsWinxpRequestHeader->Write.Length	= inputBufferLength;
			ndfsWinxpRequestHeader->Write.Key		= write.Key;
			
			if(writeToEof)
				ndfsWinxpRequestHeader->Write.ByteOffset = write.ByteOffset.QuadPart;
			else
				ndfsWinxpRequestHeader->Write.ByteOffset = write.ByteOffset.QuadPart+ioStatus.Information;

			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
			if(inputBufferLength)
				RtlCopyMemory(
					ndfsWinxpRequestData,
					(PUCHAR)inputBuffer + ioStatus.Information,
					inputBufferLength
					);

			if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
				Secondary,
				FastMutexSet,
				fileExt,
				Retry,
				secondaryRequest,
				secondaryRequest->SessionId
				) != STATUS_SUCCESS)
			{
				return STATUS_UNSUCCESSFUL;
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
				
				//ExReleaseFastMutex(&Secondary->FastMutex) ;
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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
			
			if(ndfsWinxpReplytHeader->Status != STATUS_SUCCESS)
			{
				ASSERT(ndfsWinxpReplytHeader->Information == 0);
				if(ioStatus.Information) // already read
				{
					ioStatus.Status = STATUS_SUCCESS;
				} 
				else
				{
					ioStatus.Status = ndfsWinxpReplytHeader->Status;
					ioStatus.Information = 0;
				}
				
				DereferenceSecondaryRequest(secondaryRequest);
				secondaryRequest = NULL;

				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				*FastMutexSet = FALSE;
				break;
			}

			ioStatus.Information += ndfsWinxpReplytHeader->Information;
			ioStatus.Status	= STATUS_SUCCESS;
			returnStatus = STATUS_SUCCESS;
			
			lastCurrentByteOffset = ndfsWinxpReplytHeader->CurrentByteOffset;
			
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

			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

		Irp->IoStatus.Status = ioStatus.Status;
		Irp->IoStatus.Information = ioStatus.Information;
			
		if (Irp->IoStatus.Status == STATUS_SUCCESS && synchronousIo && !pagingIo) 
		{
			if(writeToEof)
			{
				if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1)
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
		struct QueryFile			queryFile;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		secondaryRequest = NULL;
		returnStatus = RedirectIrpMajorQueryInformation(Secondary, Irp, FastMutexSet, Retry);
		break;

		queryFile.FileInformationClass	= irpSp->Parameters.QueryFile.FileInformationClass;
		queryFile.Length				= irpSp->Parameters.QueryFile.Length;
		outputBufferLength				= queryFile.Length;


		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_INFORMATION,
								outputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);
	
		ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

		returnStatus = STATUS_SUCCESS;
		
		break;
	}
	
    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		struct SetFile				setFile;

		PVOID						inputBuffer = MapInputBuffer(Irp);
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		ULONG						outputBufferLength = 0;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		fcb->FileRecordSegmentHeaderAvail = FALSE;
		
		setFile.AdvanceOnly				= irpSp->Parameters.SetFile.AdvanceOnly;
		setFile.ClusterCount			= irpSp->Parameters.SetFile.ClusterCount;
		setFile.DeleteHandle			= irpSp->Parameters.SetFile.DeleteHandle;
		setFile.FileInformationClass	= irpSp->Parameters.SetFile.FileInformationClass;
		setFile.FileObject				= irpSp->Parameters.SetFile.FileObject;
		setFile.Length					= irpSp->Parameters.SetFile.Length;
		setFile.ReplaceIfExists			= irpSp->Parameters.SetFile.ReplaceIfExists;


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

				Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Irp->IoStatus.Information = 0;
			
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
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				( "RedirectIrp: setFile.FileInformationClass = %d\n", setFile.FileInformationClass)) ;
			ASSERT(LFS_REQUIRED);

			Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;
			
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;

			break;					
		}

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_SET_INFORMATION,
								inputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->SetFile.Length					= setFile.Length;
		ndfsWinxpRequestHeader->SetFile.FileInformationClass	= setFile.FileInformationClass;
		
		{
			PFILE_EXTENTION	setFileFileExt;

			setFileFileExt = Secondary_LookUpFileExtension(Secondary, fileObject);

			if(setFileFileExt == NULL)
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;

				Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Irp->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}

			ndfsWinxpRequestHeader->SetFile.FileHandle = setFileFileExt->PrimaryFileHandle;
		}

		ndfsWinxpRequestHeader->SetFile.ReplaceIfExists = setFile.ReplaceIfExists;
		ndfsWinxpRequestHeader->SetFile.AdvanceOnly		= setFile.AdvanceOnly;
		ndfsWinxpRequestHeader->SetFile.ClusterCount	= setFile.ClusterCount;
		ndfsWinxpRequestHeader->SetFile.DeleteHandle	= (_U32)setFile.DeleteHandle;
		

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
			PFILE_EXTENTION			rootFileExt;

			ndfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists = linkInformation->ReplaceIfExists;
			ndfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength	= linkInformation->FileNameLength;
			
			if(linkInformation->RootDirectory == NULL) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					( "RedirectIrp: FileLinkInformation: No RootDirectory\n")) ;

				ndfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle = 0;	
			} else 
			{
				rootFileExt = Secondary_LookUpFileExtensionByHandle(Secondary, linkInformation->RootDirectory);
				if(!rootFileExt) 
				{
					ASSERT(LFS_BUG);
					DereferenceSecondaryRequest(
						secondaryRequest
						);
					secondaryRequest = NULL;

					Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
					Irp->IoStatus.Information = 0;
			
					returnStatus = STATUS_SUCCESS;
					break;		
				}

				ndfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle = rootFileExt->PrimaryFileHandle;
			}

			if(linkInformation->FileNameLength)
			{
				ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
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
			PFILE_EXTENTION				rootFileExt;
			

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, 
				("FileRenameInformation renameInformation->FileName = %ws", renameInformation->FileName));
			
			PrintIrp(LFS_DEBUG_SECONDARY_INFO, NULL, Secondary->LfsDeviceExt, Irp);
			ndfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists = renameInformation->ReplaceIfExists;
			ndfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength  = renameInformation->FileNameLength;
			
			if(renameInformation->RootDirectory == NULL) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					( "RedirectIrp: FileRenameInformation: No RootDirectory\n")) ;

				ndfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle = 0;
			}		
			else
			{
				rootFileExt = Secondary_LookUpFileExtensionByHandle(Secondary, renameInformation->RootDirectory);
				if(!rootFileExt) 
				{
					ASSERT(LFS_BUG);
					DereferenceSecondaryRequest(
						secondaryRequest
						);
					secondaryRequest = NULL;

					Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
					Irp->IoStatus.Information = 0;
			
					returnStatus = STATUS_SUCCESS;
					break;		
				}

				ndfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle = rootFileExt->PrimaryFileHandle;
			}

			if(renameInformation->FileNameLength)
			{
				ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
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
		
		if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
				Secondary,
				FastMutexSet,
				fileExt,
				Retry,
				secondaryRequest,
				secondaryRequest->SessionId
				) != STATUS_SUCCESS)
		{
			return STATUS_UNSUCCESSFUL;
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
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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
			
		Irp->IoStatus.Status = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information;


		if(ndfsWinxpReplytHeader->Status != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(
					secondaryRequest
					);
				
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
	
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
	
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
                IoGetRequestorProcess(Irp),
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
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;
				
		break;
	}
	
    case IRP_MJ_QUERY_EA: // 0x07
	{
		struct QueryEa				queryEa;

		PVOID						inputBuffer;
		ULONG						inputBufferLength; /* = queryEa->EaListLength;*/
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;
		ULONG						bufferLength; /*= (inputBufferLength >= outputBufferLength)?inputBufferLength:outputBufferLength ; */

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
		ULONG						returnedDataSize;


		queryEa.EaIndex			= irpSp->Parameters.QueryEa.EaIndex;
		queryEa.EaList			= irpSp->Parameters.QueryEa.EaList;
		queryEa.EaListLength	= irpSp->Parameters.QueryEa.EaListLength;
		queryEa.Length			= irpSp->Parameters.QueryEa.Length;
		
		inputBuffer				= queryEa.EaList;
		outputBufferLength		= queryEa.Length;

		if(inputBuffer != NULL)
		{
			PFILE_GET_EA_INFORMATION	fileGetEa = (PFILE_GET_EA_INFORMATION)inputBuffer;
		
			inputBufferLength = 0;
			while(fileGetEa->NextEntryOffset)
			{
				inputBufferLength += fileGetEa->NextEntryOffset;
				fileGetEa = (PFILE_GET_EA_INFORMATION)((_U8 *)fileGetEa + fileGetEa->NextEntryOffset);
			}

			inputBufferLength += (sizeof(FILE_GET_EA_INFORMATION) - sizeof(CHAR) + fileGetEa->EaNameLength);
		}
		else
			inputBufferLength = 0;
		
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED) = %d queryEa.EaIndex = %d queryEa.EaList = %p queryEa.Length = %d, inputBufferLength = %d\n",
							BooleanFlagOn(irpSp->Flags, SL_INDEX_SPECIFIED), queryEa.EaIndex, queryEa.EaList, queryEa.EaListLength, inputBufferLength));

		bufferLength = (inputBufferLength >= outputBufferLength)?inputBufferLength:outputBufferLength ;
		
		ASSERT(bufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_EA,
								bufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->QueryEa.Length			= queryEa.Length;
		ndfsWinxpRequestHeader->QueryEa.EaIndex			= queryEa.EaIndex;
		ndfsWinxpRequestHeader->QueryEa.EaListLength	= queryEa.EaListLength;
//		ndfsWinxpRequestHeader->QueryEa.EaListLength	= inputBufferLength;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength);

		returnStatus = STATUS_SUCCESS;
		
		if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
				Secondary,
				FastMutexSet,
				fileExt,
				Retry,
				secondaryRequest,
				secondaryRequest->SessionId
				) != STATUS_SUCCESS)
		{
			return STATUS_UNSUCCESSFUL;
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
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

		Irp->IoStatus.Status	  = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information; 

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: Irp->IoStatus.Status = %d, Irp->IoStatus.Information = %d\n",
					Irp->IoStatus.Status, Irp->IoStatus.Information));

		returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

		if(returnedDataSize)
		{
			ASSERT(returnedDataSize <= ADD_ALIGN8(queryEa.Length));
			ASSERT(outputBuffer);
		
			RtlCopyMemory(
				outputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				(returnedDataSize < queryEa.Length) ? returnedDataSize : queryEa.Length
				);
		}
		
		DereferenceSecondaryRequest(secondaryRequest);
		
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;		

/*
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Irp->IoStatus.Status = STATUS_EAS_NOT_SUPPORTED;
		Irp->IoStatus.Information = 0;
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;
*/
		break;
    }

    case IRP_MJ_SET_EA: // 0x08
	{
		struct SetEa				setEa;
		PVOID						inputBuffer = MapInputBuffer(Irp);
		ULONG						inputBufferLength; /* = setEa->Length; */
		
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setEa.Length =	irpSp->Parameters.SetEa.Length;

		if(inputBuffer != NULL)
		{
			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)inputBuffer;
		
			inputBufferLength = 0;
			while(fileFullEa->NextEntryOffset)
			{
				inputBufferLength += fileFullEa->NextEntryOffset;
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((_U8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			inputBufferLength += (sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength);

		}
		else
			inputBufferLength = 0;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
							("RedirectIrp, IRP_MJ_SET_EA: Ea is set setEa->Length = %d, inputBufferLength = %d\n",
									setEa.Length, inputBufferLength));

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_SET_EA,
								inputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->SetEa.Length = setEa.Length;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength);

		returnStatus = STATUS_SUCCESS;

/*
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Irp->IoStatus.Status = STATUS_EAS_NOT_SUPPORTED;
		Irp->IoStatus.Information = 0;
		secondaryRequest = NULL;			
		returnStatus = STATUS_SUCCESS;
*/
		break;
    }
    
     case IRP_MJ_FLUSH_BUFFERS: // 0x09
	{
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_FLUSH_BUFFERS,
								0
								);

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
			Irp->IoStatus.Information = 0;
			
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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		returnStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: // 0x0a
	{
		struct QueryVolume			queryVolume;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		queryVolume.FsInformationClass	= irpSp->Parameters.QueryVolume.FsInformationClass;
		queryVolume.Length				= irpSp->Parameters.QueryVolume.Length;
		
		outputBufferLength				= queryVolume.Length;

		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_VOLUME_INFORMATION,
								outputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);
	
		ndfsWinxpRequestHeader->QueryVolume.Length			   = outputBufferLength;
		ndfsWinxpRequestHeader->QueryVolume.FsInformationClass = queryVolume.FsInformationClass;

		returnStatus = STATUS_SUCCESS;
		
		break;
	}
		
	case IRP_MJ_SET_VOLUME_INFORMATION:  // 0x0b
	{
		struct	SetVolume			setVolume;
		PVOID						inputBuffer = MapInputBuffer(Irp);
		ULONG						inputBufferLength;
		PVOID						outputBuffer = NULL;
		ULONG						outputBufferLength = 0;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setVolume.FsInformationClass	= irpSp->Parameters.SetVolume.FsInformationClass;
		setVolume.Length				= irpSp->Parameters.SetVolume.Length;
		
		inputBufferLength				= setVolume.Length;

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_SET_VOLUME_INFORMATION,
								inputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);
	

		ndfsWinxpRequestHeader->SetVolume.Length				= setVolume.Length;
		ndfsWinxpRequestHeader->SetVolume.FsInformationClass	= setVolume.FsInformationClass ;
	
		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, setVolume.Length) ;

		returnStatus = STATUS_SUCCESS;
		break;
	}
		
	case IRP_MJ_DIRECTORY_CONTROL: // 0x0C
	{		
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_NOISE,
				("RedirectIrp: IRP_MJ_DIRECTORY_CONTROL: MinorFunction = %X\n",
					irpSp->MinorFunction));

        if(irpSp->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY) 
		{
#if 0
			ULONG CompletionFilter;
			BOOLEAN WatchTree;
			

			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_NOISE,
					("RedirectIrp: IRP_MJ_DIRECTORY_CONTROL: IRP_MN_NOTIFY_CHANGE_DIRECTORY Called\n"));
			
			CompletionFilter = irpSp->Parameters.NotifyDirectory.CompletionFilter;
			WatchTree = BooleanFlagOn(irpSp->Flags, SL_WATCH_TREE);

			FsRtlNotifyFullChangeDirectory( 
				Secondary->NotifySync,
				&Secondary->DirNotifyList,
				fileExt,
				(PSTRING)&fcb->FullFileName,
				WatchTree,
				FALSE,
				CompletionFilter,
				Irp,
				NULL,
				NULL
				);

			Irp->IoStatus.Status = STATUS_SUCCESS;
#endif
			Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;

			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
			break;
		}
        else if(irpSp->MinorFunction == IRP_MN_QUERY_DIRECTORY) 
		{
			struct QueryDirectory		queryDirectory;
			PVOID						inputBuffer;
			ULONG						inputBufferLength;
			PVOID						outputBuffer = MapOutputBuffer(Irp);
			ULONG						outputBufferLength;

			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
			_U8							*ndfsWinxpRequestData;

			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			ULONG						returnedDataSize;

			queryDirectory.FileIndex			= irpSp->Parameters.QueryDirectory.FileIndex;
			queryDirectory.FileInformationClass = irpSp->Parameters.QueryDirectory.FileInformationClass;
			queryDirectory.FileName				= irpSp->Parameters.QueryDirectory.FileName;
			queryDirectory.Length				= irpSp->Parameters.QueryDirectory.Length;

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
			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_DIRECTORY_CONTROL,
								((inputBufferLength > outputBufferLength) ? inputBufferLength: outputBufferLength)
								);

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
			INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				Irp,
				irpSp,
				fileExt->PrimaryFileHandle
				);
	
			ndfsWinxpRequestHeader->QueryDirectory.Length				= outputBufferLength;
			ndfsWinxpRequestHeader->QueryDirectory.FileInformationClass	= queryDirectory.FileInformationClass;
			ndfsWinxpRequestHeader->QueryDirectory.FileIndex			= queryDirectory.FileIndex;

			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
			if(inputBufferLength)
				RtlCopyMemory(
					ndfsWinxpRequestData,
					inputBuffer,
					inputBufferLength
					);
		
			if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
					Secondary,
					FastMutexSet,
					fileExt,
					Retry,
					secondaryRequest,
					secondaryRequest->SessionId
					) != STATUS_SUCCESS)
			{
				return STATUS_UNSUCCESSFUL;
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
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

			Irp->IoStatus.Status	  = ndfsWinxpReplytHeader->Status;
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information; 

			returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

			if(returnedDataSize)
			{
				ASSERT(returnedDataSize <= ADD_ALIGN8(queryDirectory.Length));
				ASSERT(outputBuffer);
		
				RtlCopyMemory(
					outputBuffer,
					(_U8 *)(ndfsWinxpReplytHeader+1),
					(returnedDataSize < queryDirectory.Length) ? returnedDataSize : queryDirectory.Length
					);
			}
		
			DereferenceSecondaryRequest(
				secondaryRequest
				);
		
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
		
			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			*FastMutexSet = FALSE;		

			break;
		} 
		else
		{
			ASSERT(LFS_UNEXPECTED);
			secondaryRequest = NULL;

			Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;
			
			returnStatus = STATUS_SUCCESS;

			break;
		}

		returnStatus = STATUS_SUCCESS;
		break;
	}	
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: // 0x0D
	{
		struct FileSystemControl	fileSystemControl;
		PVOID						inputBuffer = MapInputBuffer(Irp);
		ULONG						inputBufferLength;
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
		
		fcb->FileRecordSegmentHeaderAvail = FALSE;
	
		fileSystemControl.FsControlCode			= irpSp->Parameters.FileSystemControl.FsControlCode;
		fileSystemControl.InputBufferLength		= irpSp->Parameters.FileSystemControl.InputBufferLength;
		fileSystemControl.OutputBufferLength	= irpSp->Parameters.FileSystemControl.OutputBufferLength;
		fileSystemControl.Type3InputBuffer		= irpSp->Parameters.FileSystemControl.Type3InputBuffer;

		outputBufferLength = fileSystemControl.OutputBufferLength;

		if(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST 
			|| irpSp->MinorFunction == IRP_MN_KERNEL_CALL) 
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp: IRP_MJ_FILE_SYSTEM_CONTROL: MajorFunction = %X MinorFunction = %X Function = %d outputBufferLength = %d\n",
					irpSp->MajorFunction, irpSp->MinorFunction, (fileSystemControl.FsControlCode & 0x00003FFC) >> 2, outputBufferLength));

			//
			//	Do not allow exclusive access to the volume and dismount volume to protect format
			//
			
			if(fileSystemControl.FsControlCode == FSCTL_LOCK_VOLUME				// 6
				|| fileSystemControl.FsControlCode == FSCTL_DISMOUNT_VOLUME)	// 8
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("RedirectIrp, IRP_MJ_FILE_SYSTEM_CONTROL: Secondary is trying to acquire the volume exclusively. Denied it.\n")) ;

				ASSERT(fileObject 
						&& fileObject->FileName.Length == 0 
						&& fileObject->RelatedFileObject == NULL); 

				Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0 ;

				returnStatus = STATUS_SUCCESS;
				break ;
			}
		}
		else if(irpSp->MinorFunction == IRP_MN_MOUNT_VOLUME 
			|| irpSp->MinorFunction == IRP_MN_VERIFY_VOLUME 
			|| irpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) 
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
		else
		{
			ASSERT(LFS_UNEXPECTED);

			Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;
			
			returnStatus = STATUS_SUCCESS;
			break ;
		}
		
		//ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		//ASSERT(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST);
		
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

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
							Secondary,
								IRP_MJ_FILE_SYSTEM_CONTROL,
								((inputBufferLength > outputBufferLength) ? inputBufferLength: outputBufferLength)
							);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);
	
		ndfsWinxpRequestHeader->FileSystemControl.OutputBufferLength	= fileSystemControl.OutputBufferLength;
		ndfsWinxpRequestHeader->FileSystemControl.InputBufferLength		= fileSystemControl.InputBufferLength;
		ndfsWinxpRequestHeader->FileSystemControl.FsControlCode			= fileSystemControl.FsControlCode;

		if(fileSystemControl.FsControlCode == FSCTL_MOVE_FILE)			// 29
		{
			PMOVE_FILE_DATA	moveFileData = inputBuffer;	
			PFILE_EXTENTION	moveFileExt;

			moveFileExt = Secondary_LookUpFileExtensionByHandle(Secondary, moveFileData->FileHandle);			
			if(!moveFileExt) 
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;

				Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Irp->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}

			moveFileExt->Fcb->FileRecordSegmentHeaderAvail = FALSE;

			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.FileHandle	= moveFileExt->PrimaryFileHandle;
			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingVcn	= moveFileData->StartingVcn.QuadPart;
			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingLcn	= moveFileData->StartingLcn.QuadPart;
			ndfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.ClusterCount	= moveFileData->ClusterCount;
		} 
		else if(fileSystemControl.FsControlCode == FSCTL_MARK_HANDLE)	// 63
		{
			PMARK_HANDLE_INFO	markHandleInfo = inputBuffer;	
			PFILE_EXTENTION		volumeFileExt;


			volumeFileExt = Secondary_LookUpFileExtensionByHandle(Secondary, markHandleInfo->VolumeHandle);
			if(!volumeFileExt) 
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;

				Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Irp->IoStatus.Information = 0;
			
				returnStatus = STATUS_SUCCESS;
				break;		
			}

			volumeFileExt->Fcb->FileRecordSegmentHeaderAvail = FALSE;

			ndfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.UsnSourceInfo	= markHandleInfo->UsnSourceInfo;
			ndfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.VolumeHandle	= volumeFileExt->PrimaryFileHandle;
			ndfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.HandleInfo		= markHandleInfo->HandleInfo;
		}
		else
		{
			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

			if(inputBufferLength)
				RtlCopyMemory(
					ndfsWinxpRequestData,
					inputBuffer,
					inputBufferLength
					);
		}

		if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
				Secondary,
				FastMutexSet,
				fileExt,
				Retry,
				secondaryRequest,
				secondaryRequest->SessionId
				) != STATUS_SUCCESS)
		{
			return STATUS_UNSUCCESSFUL;
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
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

		Irp->IoStatus.Status	  = ndfsWinxpReplytHeader->Status;
		Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information; 

		if(Irp->IoStatus.Status == STATUS_SUCCESS || Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW)
			ASSERT(ADD_ALIGN8(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)) == ADD_ALIGN8(ndfsWinxpReplytHeader->Information));

		if(fileSystemControl.OutputBufferLength)
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp: IRP_MJ_FILE_SYSTEM_CONTROL: Function = %d fileSystemControl.OutputBufferLength = %d ndfsWinxpReplytHeader->Information = %d\n",
					(fileSystemControl.FsControlCode & 0x00003FFC) >> 2, fileSystemControl.OutputBufferLength, ndfsWinxpReplytHeader->Information));

			ASSERT(Irp->IoStatus.Information <= secondaryRequest->OutputBufferLength);
			ASSERT(secondaryRequest->OutputBuffer);
		
			RtlCopyMemory(
				secondaryRequest->OutputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				ndfsWinxpReplytHeader->Information
				);
		}
		
		DereferenceSecondaryRequest(
			secondaryRequest
			);
		
		secondaryRequest = NULL;
		returnStatus = STATUS_SUCCESS;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		*FastMutexSet = FALSE;
			
		break;
	}


//	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
    case IRP_MJ_DEVICE_CONTROL:  // 0E
	{
		struct DeviceIoControl		deviceIoControl;
		PVOID						inputBuffer = MapInputBuffer(Irp);
		ULONG						inputBufferLength;
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		deviceIoControl.IoControlCode		= irpSp->Parameters.DeviceIoControl.IoControlCode;
		deviceIoControl.InputBufferLength	= irpSp->Parameters.DeviceIoControl.InputBufferLength;
		deviceIoControl.OutputBufferLength	= irpSp->Parameters.DeviceIoControl.OutputBufferLength;
		deviceIoControl.Type3InputBuffer	= irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

		inputBufferLength  = deviceIoControl.InputBufferLength;
		outputBufferLength = deviceIoControl.OutputBufferLength;

		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("RedirectIrp: IRP_MJ_DEVICE_CONTROL: MajorFunction = %X MinorFunction = %X Function = %d outputBufferLength = %d\n",
				irpSp->MajorFunction, irpSp->MinorFunction, (deviceIoControl.IoControlCode & 0x00003FFC) >> 2, outputBufferLength));

		//ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		//ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
							Secondary,
								IRP_MJ_DEVICE_CONTROL,
								((inputBufferLength > outputBufferLength) ? inputBufferLength: outputBufferLength)
							);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		ndfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength	= outputBufferLength;
		ndfsWinxpRequestHeader->DeviceIoControl.InputBufferLength	= inputBufferLength;
		ndfsWinxpRequestHeader->DeviceIoControl.IoControlCode		= deviceIoControl.IoControlCode;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("LFS: Secondary_RedirectIrp: IO_CONTROL: code %08lx in:%d out:%d \n",
												deviceIoControl.IoControlCode,
												inputBufferLength,
												outputBufferLength
												)) ;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
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
		Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		Irp->IoStatus.Information = 0;
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

		lockControl.ByteOffset	= irpSp->Parameters.LockControl.ByteOffset;
		lockControl.Key			= irpSp->Parameters.LockControl.Key;
		lockControl.Length		= irpSp->Parameters.LockControl.Length;

		ASSERT(!(lockControl.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION 
						|| lockControl.ByteOffset.HighPart == -1));

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("IRP_MJ_LOCK_CONTROL: irpSp->MinorFunction = %d, lockControl.Key = %x\n", 
					irpSp->MinorFunction, lockControl.Key));
		PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrp", Secondary->LfsDeviceExt, Irp);

		if(irpSp->MinorFunction == IRP_MN_LOCK)
		{
		}
		else if(irpSp->MinorFunction == IRP_MN_UNLOCK_SINGLE)
		{
		}
		else if(irpSp->MinorFunction == IRP_MN_UNLOCK_ALL)
		{
#ifndef		__LFS_HCT_TEST_MODE__
			ASSERT(LFS_REQUIRED);
#endif
		}
		else
		{
#if DBG
			UNICODE_STRING	nullName;
		
			RtlInitUnicodeString(&nullName, L"NULL");

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				( "IRQL:%d IRP:%p %s(%d:%d) file: %p %wZ %c%c%c%c\n",
					KeGetCurrentIrql(),
					Irp,
					IrpMajors[irpSp->MajorFunction],
					(int)irpSp->MajorFunction,
					(int)irpSp->MinorFunction,
					fileObject,
					(fileObject) ? &fileObject->FileName : &nullName,
		            (Irp->Flags & IRP_PAGING_IO) ? '*' : ' ',
			        (Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
					(Irp->Flags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
					(fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' '
					));
#endif
		
			//ASSERT(LFS_IRP_NOT_IMPLEMENTED);
			Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;
			
			returnStatus = STATUS_SUCCESS;
			break;
		}

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_LOCK_CONTROL,
								0
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		if(irpSp->MinorFunction == IRP_MN_LOCK || irpSp->MinorFunction == IRP_MN_UNLOCK_SINGLE)
		{
			ndfsWinxpRequestHeader->LockControl.Length		= lockControl.Length->QuadPart;
			ndfsWinxpRequestHeader->LockControl.Key			= lockControl.Key;
			ndfsWinxpRequestHeader->LockControl.ByteOffset	= lockControl.ByteOffset.QuadPart;

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ( "LFS: IRP_MJ_LOCK_CONTROL: Length:%I64d Key:%08lx Offset:%I64d Ime:%d Ex:%d\n",
						lockControl.Length->QuadPart,
						lockControl.Key,
						lockControl.ByteOffset.QuadPart,
						(irpSp->Flags & SL_FAIL_IMMEDIATELY) != 0,
						(irpSp->Flags & SL_EXCLUSIVE_LOCK) != 0
						)) ;
		}

		returnStatus = STATUS_SUCCESS;
		break;
	}

	case IRP_MJ_CLEANUP: // 0x12
	{          
		if(fileExt->TypeOfOpen == UserFileOpen && fileObject->SectionObjectPointer)
		{
			IO_STATUS_BLOCK		ioStatusBlock;
			LARGE_INTEGER		largeZero = {0,0};
		
			if(fcb->Header.PagingIoResource)
			{
				ASSERT(LFS_REQUIRED);

				Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Irp->IoStatus.Information = 0;
				secondaryRequest = NULL;			
				returnStatus = STATUS_SUCCESS;

				InterlockedDecrement(&fcb->UncleanCount);
				break;
			}				
			
            FsRtlFastUnlockAll( 
				&fcb->FileLock,
                fileObject,
                IoGetRequestorProcess(Irp),
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

		if(fileExt->Corrupted == TRUE)
		{
			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
			
			secondaryRequest = NULL;
			returnStatus = STATUS_SUCCESS;
		}
		else
		{
			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
									Secondary,
									IRP_MJ_CLEANUP,
									0
									);

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
			INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
				ndfsWinxpRequestHeader,
				Irp,	
				irpSp,
				fileExt->PrimaryFileHandle
				);

			if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1 
				&& fileExt->TypeOfOpen != UserFileOpen)
			{
				ClearFlag(ndfsWinxpRequestHeader->IrpFlags, IRP_CLOSE_OPERATION);
			}

			if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
				Secondary,
				FastMutexSet,
				fileExt,
				Retry,
				secondaryRequest,
				secondaryRequest->SessionId
				) != STATUS_SUCCESS)
			{
				return STATUS_UNSUCCESSFUL;
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
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
				
			DereferenceSecondaryRequest(secondaryRequest);	
			secondaryRequest = NULL;
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		querySecurity.Length				= irpSp->Parameters.QuerySecurity.Length;
		querySecurity.SecurityInformation	= irpSp->Parameters.QuerySecurity.SecurityInformation;

		outputBufferLength = querySecurity.Length;

		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_SECURITY,
								Secondary->Thread.SessionContext.SecondaryMaxDataSize
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
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
		_U8							*ndfsWinxpRequestData;

		NTSTATUS					securityStatus;
		ULONG						securityLength = 0;


		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setSecurity.SecurityDescriptor  = irpSp->Parameters.SetSecurity.SecurityDescriptor;
		setSecurity.SecurityInformation = irpSp->Parameters.SetSecurity.SecurityInformation;

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
			Irp->IoStatus.Status = securityStatus;
			Irp->IoStatus.Information = 0;
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
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_SET_SECURITY,
								inputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);
	

		ndfsWinxpRequestHeader->SetSecurity.Length					= inputBufferLength;
		ndfsWinxpRequestHeader->SetSecurity.SecurityInformation		= setSecurity.SecurityInformation;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

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
			Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;
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
		_U8							*ndfsWinxpRequestData;


		queryQuota.Length			= irpSp->Parameters.QueryQuota.Length;
		queryQuota.SidList			= irpSp->Parameters.QueryQuota.SidList;
		queryQuota.SidListLength	= irpSp->Parameters.QueryQuota.SidListLength;
		queryQuota.StartSid			= irpSp->Parameters.QueryQuota.StartSid;

		inputBuffer = queryQuota.SidList ;
		inputBufferLength = queryQuota.SidListLength ;
		outputBuffer = MapOutputBuffer(Irp);
		outputBufferLength = queryQuota.Length ;
		bufferLength = (inputBufferLength >= outputBufferLength)?inputBufferLength:outputBufferLength ;
		

		ASSERT(bufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_QUOTA,
								bufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		//
		//	set request-specific parameters
		//
		ndfsWinxpRequestHeader->QueryQuota.Length			= outputBufferLength;
		ndfsWinxpRequestHeader->QueryQuota.InputLength		= inputBufferLength;
		ndfsWinxpRequestHeader->QueryQuota.StartSidOffset	= (PCHAR)queryQuota.StartSid - (PCHAR)queryQuota.SidList ;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
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
		_U8							*ndfsWinxpRequestData;

		
		fcb->FileRecordSegmentHeaderAvail = FALSE;

		setQuota.Length = irpSp->Parameters.SetQuota.Length;

		inputBuffer			= MapInputBuffer(Irp) ;
		inputBufferLength	= setQuota.Length ;

		ASSERT(inputBufferLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_SET_QUOTA,
								inputBufferLength
								);

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
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER(
			ndfsWinxpRequestHeader,
			Irp,
			irpSp,
			fileExt->PrimaryFileHandle
			);

		//
		//	set request-specific parameters
		//
		ndfsWinxpRequestHeader->SetQuota.Length	= inputBufferLength;

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
		RtlCopyMemory(ndfsWinxpRequestData, inputBuffer, inputBufferLength) ;
		returnStatus = STATUS_SUCCESS;

		break ;
	}

	default:

#if DBG
	{
		UNICODE_STRING	nullName;
		
		RtlInitUnicodeString(&nullName, L"NULL");

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			( "IRQL:%d IRP:%p %s(%d:%d) file: %p %wZ %c%c%c%c\n",
				KeGetCurrentIrql(),
				Irp,
				IrpMajors[irpSp->MajorFunction],
				(int)irpSp->MajorFunction,
				(int)irpSp->MinorFunction,
				fileObject,
				(fileObject) ? &fileObject->FileName : &nullName,
                (Irp->Flags & IRP_PAGING_IO) ? '*' : ' ',
                (Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
				(Irp->Flags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
				(fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' '
				));
	}

#endif
		ASSERT(LFS_IRP_NOT_IMPLEMENTED);
		Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		Irp->IoStatus.Information = 0;
		secondaryRequest = NULL;			
		returnStatus = STATUS_SUCCESS;

		break;
	}

	if(secondaryRequest)
	{
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;

		if(ACQUIRE_FASTMUTEX_AND_TEST_CORRUPT_ERROR_RETURN(
				Secondary,
				FastMutexSet,
				fileExt,
				Retry,
				secondaryRequest,
				secondaryRequest->SessionId
				) != STATUS_SUCCESS)
		{
			return STATUS_UNSUCCESSFUL;
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
			_U32						returnedDataSize;
		
			
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
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

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

			Irp->IoStatus.Status	  = ndfsWinxpReplytHeader->Status;
			Irp->IoStatus.Information = ndfsWinxpReplytHeader->Information; 

			returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

			if(returnedDataSize)
			{
				ASSERT(Irp->IoStatus.Status == STATUS_SUCCESS || Irp->IoStatus.Status == STATUS_BUFFER_OVERFLOW);
				if(Irp->IoStatus.Status == STATUS_SUCCESS)
					ASSERT(ADD_ALIGN8(returnedDataSize) == ADD_ALIGN8(ndfsWinxpReplytHeader->Information));
				
				ASSERT(Irp->IoStatus.Information <= secondaryRequest->OutputBufferLength);
				ASSERT(secondaryRequest->OutputBuffer);
					RtlCopyMemory(
						secondaryRequest->OutputBuffer,
						(_U8 *)(ndfsWinxpReplytHeader+1),
						ndfsWinxpReplytHeader->Information
						);
#if DBG
				if(IRP_MJ_QUERY_INFORMATION == irpSp->MajorFunction) {
					struct QueryFile *queryFile =(struct QueryFile *)&(irpSp->Parameters.QueryFile) ;

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
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			*FastMutexSet = FALSE;
			
		} while(0);
	}

	if(*Retry == FALSE)
		ASSERT(returnStatus != DBG_CONTINUE);


	return returnStatus;
}


VOID
PrintFileRecordSegmentHeader(
	IN PFILE_RECORD_SEGMENT_HEADER	FileRecordSegmentHeader
	)
{
	LARGE_INTEGER				fileReferenceNumber;
	PLARGE_INTEGER				baseFileRecordSegment;
	PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;	
	ULONG						attributeOffset;
	ULONG						count = 0;
			

	fileReferenceNumber.HighPart	= FileRecordSegmentHeader->SegmentNumberHighPart;
	fileReferenceNumber.LowPart		= FileRecordSegmentHeader->SegmentNumberLowPart;

	SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
		("fileReferenceNumber = %I64x\n", fileReferenceNumber.QuadPart));
	SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
		("fileRecordSegmentHeader->Lsn = %I64x\n", FileRecordSegmentHeader->Lsn.QuadPart));
	SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
		("fileRecordSegmentHeader->SequenceNumber = %x\n", FileRecordSegmentHeader->SequenceNumber));
	SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
		("fileRecordSegmentHeader->ReferenceCount = %x\n", FileRecordSegmentHeader->ReferenceCount));
			
	baseFileRecordSegment = (PLARGE_INTEGER)&FileRecordSegmentHeader->BaseFileRecordSegment;
	SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
		("BaseFileRecordSegment = %I64x\n", baseFileRecordSegment->QuadPart));

	attributeOffset = FileRecordSegmentHeader->FirstAttributeOffset;

	do
	{
		attributeRecordHeader = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecordSegmentHeader + attributeOffset);
				
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("\nattributeRecordHeader->TypeCode %8x %s\n", 
				attributeRecordHeader->TypeCode, 
				(attributeRecordHeader->TypeCode == $END) ? "$END                   " : AttributeTypeCode[attributeRecordHeader->TypeCode>>4]));

		if(attributeRecordHeader->TypeCode == $END)
			break;

		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("attributeRecordHeader->RecordLength = %d\n", attributeRecordHeader->RecordLength));
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("attributeRecordHeader->FormCode = %d\n", attributeRecordHeader->FormCode));
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("attributeRecordHeader->NameLength = %d\n", attributeRecordHeader->NameLength));
		if(attributeRecordHeader->NameLength)
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("attributeRecordHeader->Name = %s\n", (PCHAR)attributeRecordHeader + attributeRecordHeader->NameOffset));
		}
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("attributeRecordHeader->Flags= %x\n", attributeRecordHeader->Flags));
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
			("attributeRecordHeader->Instance = %d\n", attributeRecordHeader->Instance));

		attributeOffset += attributeRecordHeader->RecordLength;

		if(attributeRecordHeader->FormCode == RESIDENT_FORM)
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("attributeRecordHeader->Form.Resident.ValueLength = %d\n", 
				attributeRecordHeader->Form.Resident.ValueLength));
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
				("attributeRecordHeader->Form.Resident.ResidentFlags = %x\n", 
				attributeRecordHeader->Form.Resident.ResidentFlags));
		}
			
		if(attributeRecordHeader->TypeCode == $STANDARD_INFORMATION)
		{
		    PSTANDARD_INFORMATION standardInformation;

			if(attributeRecordHeader->FormCode == RESIDENT_FORM)
			{
				if(attributeRecordHeader->Form.Resident.ValueLength)
					standardInformation = (PSTANDARD_INFORMATION)((PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset);
				else
					standardInformation = NULL;

				if(standardInformation)
				{
					TIME		time;

					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("\n"));
					time.QuadPart = standardInformation->CreationTime;
					PrintTime(LFS_DEBUG_SECONDARY_TRACE, &time);
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						(" standardInformation->CreationTime = %I64x\n", standardInformation->CreationTime));
						
					time.QuadPart = standardInformation->LastModificationTime;
					PrintTime(LFS_DEBUG_SECONDARY_TRACE, &time);
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->LastModificationTime = %I64x\n", standardInformation->LastModificationTime));

					time.QuadPart = standardInformation->LastChangeTime;
					PrintTime(LFS_DEBUG_SECONDARY_TRACE, &time);
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						(" standardInformation->LastChangeTime = %I64x\n", standardInformation->LastChangeTime));

					time.QuadPart = standardInformation->LastAccessTime;
					PrintTime(LFS_DEBUG_SECONDARY_TRACE, &time);
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						(" standardInformation->LastAccessTime = %I64x\n", standardInformation->LastAccessTime));
							
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("\n"));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->FileAttributes = %x\n", standardInformation->FileAttributes));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->MaximumVersions = %x\n", standardInformation->MaximumVersions));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->VersionNumber = %x\n", standardInformation->VersionNumber));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->ClassId = %x\n", standardInformation->ClassId));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->OwnerId = %x\n", standardInformation->OwnerId));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->SecurityId = %x\n", standardInformation->SecurityId));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->QuotaCharged = %I64x\n", standardInformation->QuotaCharged));
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("standardInformation->Usn = %I64x\n", standardInformation->Usn));
				}
			}
		}

		if(attributeRecordHeader->TypeCode == $FILE_NAME)
		{
			PFILE_NAME fileName;

			if(attributeRecordHeader->FormCode == RESIDENT_FORM)
			{
				if(attributeRecordHeader->Form.Resident.ValueLength)
					fileName = (PFILE_NAME)((PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset);
				else
					fileName = NULL;


				if(fileName && fileName->FileNameLength)
				{
					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, 
						("fileName->FileName = %ws\n", fileName->FileName));
				}
			}
		}
		
		if(attributeRecordHeader->TypeCode == $DATA)
		{
			if(attributeRecordHeader->FormCode == NONRESIDENT_FORM)
			{
				VCN		nextVcn;
				LCN		currentLcn;
				PCHAR	ch;


				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.LowestVcn = %x\n", 
						attributeRecordHeader->Form.Nonresident.LowestVcn));				
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.HighestVcn = %x\n", 
						attributeRecordHeader->Form.Nonresident.HighestVcn));
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.MappingPairsOffset = %x\n", 
						attributeRecordHeader->Form.Nonresident.MappingPairsOffset));
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.CompressionUnit = %x\n", 
						attributeRecordHeader->Form.Nonresident.CompressionUnit));
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.AllocatedLength = %x\n", 
						attributeRecordHeader->Form.Nonresident.AllocatedLength));
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.FileSize = %x\n", 
						attributeRecordHeader->Form.Nonresident.FileSize));
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.ValidDataLength = %x\n", 
						attributeRecordHeader->Form.Nonresident.ValidDataLength));
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
					("attributeRecordHeader->Form.Nonresident.TotalAllocated = %x\n", 
						attributeRecordHeader->Form.Nonresident.TotalAllocated));

			    nextVcn = attributeRecordHeader->Form.Nonresident.LowestVcn;
				currentLcn = 0;
				ch = (PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Nonresident.MappingPairsOffset;

				while (!IsCharZero(*ch)) 
				{
					ULONG		vcnBytes, lcnBytes;
					LONGLONG	change;
					
					vcnBytes = *ch & 0xF;
					lcnBytes = *ch++ >> 4;

					if (IsCharLtrZero(*(ch + vcnBytes - 1))) 
					{
						//NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, NULL );
						break;
			        }

					change = 0;
					RtlCopyMemory( &change, ch, vcnBytes);
					ch += vcnBytes;
					nextVcn = nextVcn + change;
			
					if (lcnBytes != 0) 
					{
						change = 0;
						if (IsCharLtrZero(*(ch+lcnBytes-1))) 
						{
							change = change-1; //negative
						}
						RtlCopyMemory(&change, ch, lcnBytes);
						ch += lcnBytes;
						currentLcn = currentLcn + change;
					}

					SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
						("nextVcn = %I64x, currentLcn = %I64x\n", nextVcn, currentLcn));
				}
			}
		}

	} while(1);

	return;
}