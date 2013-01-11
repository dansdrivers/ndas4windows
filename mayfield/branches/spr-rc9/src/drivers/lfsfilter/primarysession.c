#define	__NETDISK_MANAGER__
#define	__PRIMARY__
#include "LfsProc.h"

//
//	ntifs.h of Windows XP does not include ZwSetVolumeInformationFile nor NtSetVolumeInformationFile.
//
NTSYSAPI
NTSTATUS
NTAPI
ZwSetVolumeInformationFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID FsInformation,
    IN ULONG Length,
    IN FS_INFORMATION_CLASS FsInformationClass
    );


NTSTATUS
NtQueryEaFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID Buffer,
    IN ULONG Length,
    IN BOOLEAN ReturnSingleEntry,
    IN PVOID EaList OPTIONAL,
    IN ULONG EaListLength,
    IN PULONG EaIndex OPTIONAL,
    IN BOOLEAN RestartScan
    );


NTSTATUS
NtSetEaFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID Buffer,
    IN ULONG Length
    );


VOID
DisconnectFromSecondary(
	IN	PPRIMARY_SESSION			PrimarySession
	);


VOID
PrimarySessionThreadProc(
	IN PPRIMARY_SESSION PrimarySession
	);


POPEN_FILE
PrimarySession_AllocateOpenFile(
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject,
	IN	PUNICODE_STRING		FullFileName
	);


POPEN_FILE
PrimarySession_FindOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	ULONG			 OpenFileId
	);


POPEN_FILE
PrimarySession_FindOpenFileByName(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  ACCESS_MASK			DesiredAccess
	);


POPEN_FILE
PrimarySession_FindOpenFileCleanUpedAndNotClosed(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  BOOLEAN				DeleteOnClose
	);


VOID
PrimarySession_FreeOpenFile(
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	);


_U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	);

NTSTATUS
ReceiveNtfsWinxpMessage(
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
	);


NTSTATUS
SendNtfsWinxpMessage(
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN _U32						ReplyDataSize,
	IN _U16						Mid
	);


PPRIMARY_SESSION
PrimarySession_Create(
	IN  PPRIMARY			Primary,
	IN	HANDLE				ListenFileHandle,
	IN  PFILE_OBJECT		ListenFileObject,
	IN  ULONG				ListenSocketIndex
)
{
	PPRIMARY_SESSION	primarySession;
 	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			ntStatus;
	LARGE_INTEGER		timeOut;

	
	ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL) ;

	Primary_Reference (Primary);

	primarySession = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(PRIMARY_SESSION),
						LFS_ALLOC_TAG
						);
	
	if (primarySession == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);

		Primary_Dereference(Primary);
		return NULL;
	}

	RtlZeroMemory(
		primarySession,
		sizeof(PRIMARY_SESSION)
		);

	primarySession->Flags = PRIMARY_SESSION_INITIALIZING;

	KeInitializeSpinLock(&primarySession->SpinLock);
	primarySession->ReferenceCount = 1;
	primarySession->Primary = Primary;
	primarySession->ListenSocketIndex = ListenSocketIndex;

	InitializeListHead(&primarySession->ListEntry);

	primarySession->ConnectionFileHandle = ListenFileHandle;
	primarySession->ConnectionFileObject = ListenFileObject;

	KeInitializeEvent(&primarySession->ReadyEvent, NotificationEvent, FALSE) ;
	
	InitializeListHead(&primarySession->RequestQueue);
	KeInitializeSpinLock(&primarySession->RequestQSpinLock);
	KeInitializeEvent(&primarySession->RequestEvent, NotificationEvent, FALSE) ;

	primarySession->ThreadHandle = 0 ;
	primarySession->ThreadObject = NULL;

	primarySession->TdiReceiveContext.Irp = NULL;
	KeInitializeEvent(&primarySession->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
	
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	primarySession->State = SESSION_CLOSE;
	primarySession->SessionKey = (_U32)PtrToUlong(primarySession);
	primarySession->PrimaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
	primarySession->SecondaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;

	primarySession->Uid = (_U16)primarySession;
	
	ntStatus = PsCreateSystemThread(
					&primarySession->ThreadHandle,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					PrimarySessionThreadProc,
					primarySession
					);
	
	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_UNEXPECTED);
		PrimarySession_Close(primarySession);

		return NULL;
	}

	ntStatus = ObReferenceObjectByHandle(
					primarySession->ThreadHandle,
					FILE_READ_DATA,
					NULL,
					KernelMode,
					&primarySession->ThreadObject,
					NULL
					);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_UNEXPECTED);
		PrimarySession_Close(primarySession);

		return NULL;
	}

	timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
	ntStatus = KeWaitForSingleObject(
					&primarySession->ReadyEvent,
					Executive,
					KernelMode,
					FALSE,
					&timeOut
					) ;

	ASSERT(ntStatus == STATUS_SUCCESS);

	KeClearEvent(&primarySession->ReadyEvent);

	if(ntStatus != STATUS_SUCCESS) 
	{
		ASSERT(LFS_BUG);
	}

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("PrimarySession_Create: The primary thread are ready\n"));

	
	ExInterlockedInsertTailList(
		&Primary->PrimarySessionQueue[ListenSocketIndex],
		&primarySession->ListEntry,
		&Primary->PrimarySessionQSpinLock[ListenSocketIndex]
		);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
		("PrimarySession_Create: primarySession = %p\n", primarySession));

#if DBG
	LfsObjectCounts.PrimarySessionCount++;
#endif

	return primarySession;
}


VOID
PrimarySession_Close(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
//	LARGE_INTEGER interval;

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
		("PrimarySession_Close: PrimarySession = %p\n", PrimarySession));

	if(PrimarySession->ThreadHandle == NULL)
	{
		ASSERT(LFS_BUG);
		PrimarySession_Dereference(PrimarySession);

		return;
	}

	ASSERT(PrimarySession->ThreadObject != NULL);

	if(PrimarySession->ThreadFlags & PRIMARY_SESSION_THREAD_TERMINATED)
	{
		ObDereferenceObject(PrimarySession->ThreadObject) ;

		PrimarySession->ThreadHandle = NULL;
		PrimarySession->ThreadObject = NULL;

	} else
	{
		PPRIMARY_SESSION_REQUEST	primarySessionRequest ;
		NTSTATUS					ntStatus ;
		LARGE_INTEGER				timeOut ;
	
		
		primarySessionRequest = AllocPrimarySessionRequest(FALSE);
		primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DISCONNECT;

		QueueingPrimarySessionRequest(
				PrimarySession,
				primarySessionRequest
				);

		primarySessionRequest = AllocPrimarySessionRequest (FALSE);
		primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DOWN;

		QueueingPrimarySessionRequest(
				PrimarySession,
				primarySessionRequest
				);

		timeOut.QuadPart = - LFS_TIME_OUT;
		ntStatus = KeWaitForSingleObject(
						PrimarySession->ThreadObject,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						) ;

		ASSERT(ntStatus == STATUS_SUCCESS);

		if(ntStatus == STATUS_SUCCESS) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							("PrimarySession_Close: thread stoped\n"));

			ObDereferenceObject(PrimarySession->ThreadObject) ;

			PrimarySession->ThreadHandle = NULL;
			PrimarySession->ThreadObject = NULL;
		
		} else
		{
			ASSERT(LFS_BUG);
			return;
		}
	}

#if 0
    interval.QuadPart = (5 * DELAY_ONE_SECOND);      //delay 5 seconds
    KeDelayExecutionThread( KernelMode, FALSE, &interval );
#endif

	LpxTdiDisassociateAddress(PrimarySession->ConnectionFileObject);
	LpxTdiCloseConnection(
				PrimarySession->ConnectionFileHandle, 
				PrimarySession->ConnectionFileObject
				);

	PrimarySession->ConnectionFileHandle = NULL;
	PrimarySession->ConnectionFileObject = NULL;

	PrimarySession_Dereference(PrimarySession);

	return;
}


VOID
PrimarySession_FileSystemShutdown(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest ;
	NTSTATUS					ntStatus ;
	LARGE_INTEGER				timeOut ;

	
	PrimarySession_Reference(PrimarySession);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
		("PrimarySession_FileSystemShutdown: PrimarySession = %p\n", PrimarySession));

	if(PrimarySession->ThreadHandle == NULL)
	{
		ASSERT(LFS_BUG);
		PrimarySession_Dereference(PrimarySession);

		return;
	}

	ASSERT(PrimarySession->ThreadObject != NULL);

	if(PrimarySession->ThreadFlags & PRIMARY_SESSION_THREAD_TERMINATED)
	{
		//ASSERT(LFS_BUG);
		PrimarySession_Dereference(PrimarySession);

		return;

	}
		
	primarySessionRequest = AllocPrimarySessionRequest(TRUE);
	primarySessionRequest->RequestType = PRIMARY_SESSION_SHUTDOWN;

	QueueingPrimarySessionRequest(
			PrimarySession,
			primarySessionRequest
			);

	timeOut.QuadPart = - LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject(
					&primarySessionRequest->CompleteEvent,
					Executive,
					KernelMode,
					FALSE,
					&timeOut
					) ;
	ASSERT(ntStatus == STATUS_SUCCESS);

	KeClearEvent(&primarySessionRequest->CompleteEvent);

	if(ntStatus == STATUS_SUCCESS) 
	{
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("PrimarySession_FileSystemShutdown: thread shutdown\n"));		
	} else
	{
		ASSERT(LFS_BUG);
		PrimarySession_Dereference(PrimarySession);
		return;
	}

	PrimarySession_Dereference(PrimarySession);

	return;
}


VOID
PrimarySession_Reference (
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
    LONG result;
	
    result = InterlockedIncrement (&PrimarySession->ReferenceCount);

    ASSERT (result >= 0);
}


VOID
PrimarySession_Dereference (
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
    LONG result;


    result = InterlockedDecrement (&PrimarySession->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) 
	{
		KIRQL	oldIrql;
		PPRIMARY    primary = PrimarySession->Primary;

		
		KeAcquireSpinLock(&PrimarySession->Primary->PrimarySessionQSpinLock[PrimarySession->ListenSocketIndex], &oldIrql);
		RemoveEntryList(&PrimarySession->ListEntry);
		KeReleaseSpinLock(&PrimarySession->Primary->PrimarySessionQSpinLock[PrimarySession->ListenSocketIndex], oldIrql);
		
		ExFreePoolWithTag(
			PrimarySession,
			LFS_ALLOC_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
			("PrimarySession_Dereference: PrimarySession = %p is Freed\n", PrimarySession));
#if DBG
		LfsObjectCounts.PrimarySessionCount--;
#endif
		Primary_Dereference (primary);
	}
}

#if WINVER <= 0x0501

NTSTATUS
NtUnlockFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PLARGE_INTEGER ByteOffset,
    IN PLARGE_INTEGER Length,
    IN ULONG Key
    );

#endif

#define IO_INFINITE_RETRIES         -1       // Try for ever

PIRP
IopAllocateIrpMustSucceed(
    IN CCHAR StackSize
    )

/*++

Routine Description:

    This routine is invoked to allocate an IRP when there are no appropriate
    packets remaining on the look-aside list, and no memory was available
    from the general non-paged pool, and yet, the code path requiring the
    packet has no way of backing out and simply returning an error.  There-
    fore, it must allocate an IRP.  Hence, this routine is called to allocate
    that packet.

Arguments:

    StackSize - Supplies the number of IRP I/O stack locations that the
        packet must have when allocated.

Return Value:

    A pointer to the allocated I/O Request Packet.

--*/

{
    PIRP irp;
    USHORT packetSize;
    LONG  numTries;
    LARGE_INTEGER interval;

    //
    // Attempt to allocate the IRP normally and failing that,
    // wait a second and try again. If we exhaust 7 minutes then
    // allocate the IRP from nonpaged must succeed pool.
    //

    numTries = IO_INFINITE_RETRIES;

    irp = IoAllocateIrp(StackSize, FALSE);

    while (!irp && numTries)  {

        interval.QuadPart = -1000 * 1000 * 10; // 10 Msec.
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
        irp = IoAllocateIrp(StackSize, FALSE);
        if (numTries != IO_INFINITE_RETRIES) {
            numTries--;
        }
    }

    //
    // We tried for seven minutes and we have not freed memory.
    // Its time to try the must succeed pool.
    //

    if (!irp) {
        packetSize = IoSizeOfIrp(StackSize);
        irp = ExAllocatePoolWithTag(NonPagedPoolMustSucceed, packetSize, ' prI');
        IoInitializeIrp(irp, packetSize, StackSize);
        irp->AllocationFlags |= IRP_ALLOCATED_MUST_SUCCEED;
    }

    return irp;
}


_U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	)
{
	_U32 returnSize;


	switch(NdfsWinxpRequestHeader->IrpMajorFunction)
	{
    case IRP_MJ_CREATE: //0x00
	{
		if(NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CREATE
			&& PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NTFS)
		{
			returnSize = PrimarySession->BytesPerFileRecordSegment;
		}
		else
			returnSize = 0;
	
		break;
	}

	case IRP_MJ_CLOSE: // 0x02
	{
		returnSize = 0;
		break;
	}

    case IRP_MJ_READ: // 0x03
	{
		returnSize = NdfsWinxpRequestHeader->Read.Length;
		break;
	}
		
    case IRP_MJ_WRITE: // 0x04
	{
		returnSize = 0;
		break;
	}
	
    case IRP_MJ_QUERY_INFORMATION: // 0x05
	{
		returnSize = NdfsWinxpRequestHeader->QueryFile.Length;
		break;
	}
	
    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		returnSize = 0;
		break;
	}
	
     case IRP_MJ_FLUSH_BUFFERS: // 0x09
	{
		returnSize = 0;
		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: // 0x0A
	{
		returnSize = NdfsWinxpRequestHeader->QueryVolume.Length;
		break;
	}

	case IRP_MJ_SET_VOLUME_INFORMATION:	// 0x0B
	{
		returnSize = 0;
		break;
	}

	case IRP_MJ_DIRECTORY_CONTROL: // 0x0C
	{
        if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_QUERY_DIRECTORY) 
		{
			returnSize = NdfsWinxpRequestHeader->QueryDirectory.Length;
		} else
		{
			returnSize = 0;
		}

		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: // 0x0D
	{
		returnSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;
		break;
	}

    case IRP_MJ_DEVICE_CONTROL: // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	{
		returnSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;
		break;
	}
	
	case IRP_MJ_LOCK_CONTROL: // 0x11
	{
		returnSize = 0;
		break;
	}

	case IRP_MJ_CLEANUP: // 0x12
	{
		returnSize = 0;
		break;
	}

    case IRP_MJ_QUERY_SECURITY:
	{
		returnSize = NdfsWinxpRequestHeader->QuerySecurity.Length;
		break;
	}

    case IRP_MJ_SET_SECURITY:
	{
		returnSize = 0;
		break;
	}

	default:

		returnSize = 0;
		break;
	}

	return returnSize;
}


#define SAFELEN_ALIGNMENT_ADD		7 // for 4 bytes(long) alignment.


ULONG
CalculateSafeLength(
	FILE_INFORMATION_CLASS	fileInformationClass,
	ULONG					requestLength,
	ULONG					returnedLength,
	PCHAR					Buffer
	) 
{
	ULONG	offset = 0;
	ULONG	safeAddSize = 0;


	switch(fileInformationClass) 
	{
	case FileDirectoryInformation: 
	{
		PFILE_DIRECTORY_INFORMATION	fileDirectoryInformation;

		while(offset < returnedLength) 
		{
			fileDirectoryInformation = (PFILE_DIRECTORY_INFORMATION)(Buffer + offset);
			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if(fileDirectoryInformation->NextEntryOffset == 0)
				break ;

			offset += fileDirectoryInformation->NextEntryOffset;
		}
		break ;
	}
	case FileFullDirectoryInformation: 
	{
		PFILE_FULL_DIR_INFORMATION	fileFullDirInformation;

		while(offset < returnedLength) 
		{
			fileFullDirInformation = (PFILE_FULL_DIR_INFORMATION)(Buffer + offset);
			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if(fileFullDirInformation->NextEntryOffset == 0)
				break ;

			offset += fileFullDirInformation->NextEntryOffset;
		}
		break ;
	}
	case FileBothDirectoryInformation: 
	{
		PFILE_BOTH_DIR_INFORMATION	fileBothDirInformation;

		while(offset < returnedLength) 
		{
			fileBothDirInformation = (PFILE_BOTH_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileBothDirInformation->NextEntryOffset)
				break ;

			offset += fileBothDirInformation->NextEntryOffset;
		}
		break ;
	}
	case FileIdBothDirectoryInformation: 
	{
		PFILE_ID_BOTH_DIR_INFORMATION	fileIdBothDirInformation;

		while(offset < returnedLength) 
		{
			fileIdBothDirInformation = (PFILE_ID_BOTH_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileIdBothDirInformation->NextEntryOffset)
				break ;

			offset += fileIdBothDirInformation->NextEntryOffset;
		}
		break ;
	}
	case FileIdFullDirectoryInformation: 
	{
		PFILE_ID_FULL_DIR_INFORMATION	fileIdFullDirInformation;

		while(offset < returnedLength) 
		{
			fileIdFullDirInformation = (PFILE_ID_FULL_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileIdFullDirInformation->NextEntryOffset)
				break ;

			offset += fileIdFullDirInformation->NextEntryOffset;
		}
		break;
	}
	case FileNamesInformation: 
	{
		PFILE_NAMES_INFORMATION	fileNamesInformation;

		while(offset < returnedLength) 
		{
			fileNamesInformation = (PFILE_NAMES_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileNamesInformation->NextEntryOffset)
				break ;

			offset += fileNamesInformation->NextEntryOffset;
		}
		break;
	}
	case FileStreamInformation: 
	{
		PFILE_STREAM_INFORMATION	fileStreamInformation;

		while(offset < returnedLength) 
		{
			fileStreamInformation = (PFILE_STREAM_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileStreamInformation->NextEntryOffset)
				break ;

			offset += fileStreamInformation->NextEntryOffset;
		}
		break;
	}
	case FileFullEaInformation: 
	{
		PFILE_FULL_EA_INFORMATION	fileFullEaInformation;

		while(offset < returnedLength) 
		{
			fileFullEaInformation = (PFILE_FULL_EA_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileFullEaInformation->NextEntryOffset)
				break ;

			offset += fileFullEaInformation->NextEntryOffset;
		}
		break;
	}
	case FileQuotaInformation: 
	{
		PFILE_QUOTA_INFORMATION	fileQuotaInformation;

		while(offset < returnedLength) 
		{
			fileQuotaInformation = (PFILE_QUOTA_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileQuotaInformation->NextEntryOffset)
				break ;

			offset += fileQuotaInformation->NextEntryOffset;
		}
		break ;
	}

	default:
		safeAddSize = 0 ;
	}

	return ((returnedLength + safeAddSize) < requestLength) ? (returnedLength+safeAddSize) : requestLength;
}


NTSTATUS
GetFileRecordSegmentHeader(
	IN PPRIMARY_SESSION				PrimarySession,
	IN HANDLE						FileHandle,
	IN PFILE_RECORD_SEGMENT_HEADER	FileRecordSegmentHeader
	)
{
	IO_STATUS_BLOCK					ioStatusBlock;
	NTSTATUS						fileSystemControlStatus;

	ULONG							usnRecordSize;
	PUSN_RECORD						usnRecord;

	LARGE_INTEGER					fileReferenceNumber;		

	NTFS_FILE_RECORD_INPUT_BUFFER	ntfsFileRecordInputBuffer;
	ULONG							outputBufferLength;
	PNTFS_FILE_RECORD_OUTPUT_BUFFER	ntfsFileRecordOutputBuffer;

	UNREFERENCED_PARAMETER(PrimarySession);

	RtlZeroMemory(
		&ioStatusBlock,
		sizeof(ioStatusBlock)
		);
	
	usnRecordSize = NDFS_MAX_PATH*sizeof(WCHAR) + sizeof(USN_RECORD) - sizeof(WCHAR);
	
	usnRecord = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        usnRecordSize,
						PRIMARY_SESSION_BUFFERE_TAG
						);
	ASSERT(usnRecord);
	if(usnRecord == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
			("GetFileRecordSegmentHeader: failed to allocate UsnRecord\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	memset(usnRecord, 0, usnRecordSize);
		
	fileSystemControlStatus = ZwFsControlFile(
								FileHandle,
								NULL,
								NULL,
								NULL,
								&ioStatusBlock,
								FSCTL_READ_FILE_USN_DATA,
								NULL,
								0,
								usnRecord,
								usnRecordSize
								);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("GetFileRecordSegmentHeader: FSCTL_READ_FILE_USN_DATA %x: FileHandle %p, fileSystemControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						FSCTL_READ_FILE_USN_DATA, FileHandle, fileSystemControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
	if(NT_SUCCESS(fileSystemControlStatus))
	{
		ASSERT(fileSystemControlStatus == STATUS_SUCCESS);	
		ASSERT(fileSystemControlStatus == ioStatusBlock.Status);
	}

	if(fileSystemControlStatus == STATUS_BUFFER_OVERFLOW)
		ASSERT(ioStatusBlock.Information == sizeof(usnRecordSize));
		
	if(!(fileSystemControlStatus == STATUS_SUCCESS || fileSystemControlStatus == STATUS_BUFFER_OVERFLOW))
	{
		ioStatusBlock.Information = 0;
		ASSERT(ioStatusBlock.Information == 0);
	}	

	if(!NT_SUCCESS(fileSystemControlStatus))
	{
		ExFreePoolWithTag( 
			usnRecord, 
			PRIMARY_SESSION_BUFFERE_TAG
			);

		return fileSystemControlStatus;
	}

	fileReferenceNumber.QuadPart = usnRecord->FileReferenceNumber;

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("usnRecord->FileReferenceNumber = %I64x\n", usnRecord->FileReferenceNumber));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("usnRecord->FileName = %ws\n", usnRecord->FileName));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("fileReferenceNumber.QuadPart = %I64x\n", fileReferenceNumber.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("fileReferenceNumber.LowPart = %x\n", fileReferenceNumber.LowPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("fileReferenceNumber.HighPart = %x\n", fileReferenceNumber.HighPart));

	fileReferenceNumber.QuadPart = usnRecord->FileReferenceNumber;

	ntfsFileRecordInputBuffer.FileReferenceNumber = fileReferenceNumber;
	outputBufferLength = sizeof(NTFS_FILE_RECORD_OUTPUT_BUFFER) + PrimarySession->BytesPerFileRecordSegment - sizeof(UCHAR);
	ntfsFileRecordOutputBuffer = ExAllocatePoolWithTag( 
									NonPagedPool, 
									outputBufferLength,
									PRIMARY_SESSION_BUFFERE_TAG
									);
	ASSERT(ntfsFileRecordOutputBuffer);
	if(ntfsFileRecordOutputBuffer == NULL) {
		ExFreePoolWithTag( 
			usnRecord, 
			PRIMARY_SESSION_BUFFERE_TAG
			);
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
			("GetFileRecordSegmentHeader: failed to allocate NtfsFileRecordOutputBuffer\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	fileSystemControlStatus = ZwFsControlFile(
								FileHandle,
								NULL,
								NULL,
								NULL,
								&ioStatusBlock,
								FSCTL_GET_NTFS_FILE_RECORD,
								&ntfsFileRecordInputBuffer,
								sizeof(ntfsFileRecordInputBuffer),
								ntfsFileRecordOutputBuffer,
								outputBufferLength
								);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("GetFileRecordSegmentHeader: FSCTL_GET_NTFS_FILE_RECORD: FileHandle %p, fileSystemControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						FileHandle, fileSystemControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
	if(NT_SUCCESS(fileSystemControlStatus))
	{
		ASSERT(fileSystemControlStatus == STATUS_SUCCESS);	
		ASSERT(fileSystemControlStatus == ioStatusBlock.Status);
	}

	if(fileSystemControlStatus == STATUS_BUFFER_OVERFLOW)
		ASSERT(ioStatusBlock.Information == sizeof(outputBufferLength));
		
	if(!(fileSystemControlStatus == STATUS_SUCCESS || fileSystemControlStatus == STATUS_BUFFER_OVERFLOW))
	{
		ioStatusBlock.Information = 0;
		ASSERT(ioStatusBlock.Information == 0);
	}	

	if(!NT_SUCCESS(fileSystemControlStatus))
	{
		ASSERT(LFS_UNEXPECTED);

		ExFreePoolWithTag( 
			usnRecord, 
			PRIMARY_SESSION_BUFFERE_TAG
			);
		ExFreePoolWithTag( 
			ntfsFileRecordOutputBuffer, 
			PRIMARY_SESSION_BUFFERE_TAG
			);
		return fileSystemControlStatus;
	}

	RtlCopyMemory(
		FileRecordSegmentHeader,
		ntfsFileRecordOutputBuffer->FileRecordBuffer,
		PrimarySession->BytesPerFileRecordSegment
		);

	ExFreePoolWithTag( 
		usnRecord, 
		PRIMARY_SESSION_BUFFERE_TAG
		);
	
	ExFreePoolWithTag( 
		ntfsFileRecordOutputBuffer, 
		PRIMARY_SESSION_BUFFERE_TAG
		);

	return STATUS_SUCCESS;

	{
		PFILE_RECORD_SEGMENT_HEADER	fileRecordSegmentHeader;
		LARGE_INTEGER				fileReferenceNumber;
		PLARGE_INTEGER				baseFileRecordSegment;
		PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;	
		ULONG						attributeOffset;
		ULONG						count = 0;
			
		fileRecordSegmentHeader = (PFILE_RECORD_SEGMENT_HEADER)ntfsFileRecordOutputBuffer->FileRecordBuffer;
				
		ASSERT(fileRecordSegmentHeader);

		RtlCopyMemory(
			FileRecordSegmentHeader,
			fileRecordSegmentHeader,
			PrimarySession->BytesPerFileRecordSegment
			);

		return STATUS_SUCCESS;

		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("ntfsFileRecordInputBuffer->FileReferenceNumber = %I64x\n", 
			ntfsFileRecordInputBuffer.FileReferenceNumber));
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("ntfsFileRecordOutputBuffer->FileReferenceNumber = %I64x\n", 
			ntfsFileRecordOutputBuffer->FileReferenceNumber.QuadPart));
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("ntfsFileRecordOutputBuffer->FileRecordLength = %d\n", 
			ntfsFileRecordOutputBuffer->FileRecordLength));

		fileReferenceNumber.HighPart	= fileRecordSegmentHeader->SegmentNumberHighPart;
		fileReferenceNumber.LowPart		= fileRecordSegmentHeader->SegmentNumberLowPart;

		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("fileReferenceNumber = %I64x\n", fileReferenceNumber.QuadPart));
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("fileRecordSegmentHeader->Lsn = %I64x\n", fileRecordSegmentHeader->Lsn.QuadPart));
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("fileRecordSegmentHeader->SequenceNumber = %x\n", fileRecordSegmentHeader->SequenceNumber));
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("fileRecordSegmentHeader->ReferenceCount = %x\n", fileRecordSegmentHeader->ReferenceCount));
			
		baseFileRecordSegment = (PLARGE_INTEGER)&fileRecordSegmentHeader->BaseFileRecordSegment;
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
			("BaseFileRecordSegment = %I64x\n", baseFileRecordSegment->QuadPart));

		attributeOffset = fileRecordSegmentHeader->FirstAttributeOffset;

		do
		{
			attributeRecordHeader = (PATTRIBUTE_RECORD_HEADER)((PCHAR)fileRecordSegmentHeader + attributeOffset);
				
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
				("\nattributeRecordHeader->TypeCode %8x %s\n", 
					attributeRecordHeader->TypeCode, 
					(attributeRecordHeader->TypeCode == $END) ? "$END                   " : AttributeTypeCode[attributeRecordHeader->TypeCode>>4]));

			if(attributeRecordHeader->TypeCode == $END)
				break;

			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
				("attributeRecordHeader->RecordLength = %d\n", attributeRecordHeader->RecordLength));
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
				("attributeRecordHeader->FormCode = %d\n", attributeRecordHeader->FormCode));
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
				("attributeRecordHeader->NameLength = %d\n", attributeRecordHeader->NameLength));
			if(attributeRecordHeader->NameLength)
			{
				SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
					("attributeRecordHeader->Name = %s\n", (PCHAR)attributeRecordHeader + attributeRecordHeader->NameOffset));
			}
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
				("attributeRecordHeader->Flags= %x\n", attributeRecordHeader->Flags));
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
				("attributeRecordHeader->Instance = %d\n", attributeRecordHeader->Instance));

			attributeOffset += attributeRecordHeader->RecordLength;

			if(attributeRecordHeader->FormCode == RESIDENT_FORM)
			{
				SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
					("attributeRecordHeader->Form.Resident.ValueLength = %d\n", 
					attributeRecordHeader->Form.Resident.ValueLength));
				SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
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

						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE, ("\n"));
						time.QuadPart = standardInformation->CreationTime;
						PrintTime(LFS_DEBUG_PRIMARY_TRACE, &time);
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							(" standardInformation->CreationTime = %I64x\n", standardInformation->CreationTime));
						
						time.QuadPart = standardInformation->LastModificationTime;
						PrintTime(LFS_DEBUG_PRIMARY_TRACE, &time);
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							(" standardInformation->LastModificationTime = %I64x\n", 
							standardInformation->LastModificationTime));

						time.QuadPart = standardInformation->LastChangeTime;
						PrintTime(LFS_DEBUG_PRIMARY_TRACE, &time);
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							(" standardInformation->LastChangeTime = %I64x\n", standardInformation->LastChangeTime));

						time.QuadPart = standardInformation->LastAccessTime;
						PrintTime(LFS_DEBUG_PRIMARY_TRACE, &time);
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							(" standardInformation->LastAccessTime = %I64x\n", standardInformation->LastAccessTime));
							
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE, ("\n"));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->FileAttributes = %x\n", standardInformation->FileAttributes));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->MaximumVersions = %x\n", standardInformation->MaximumVersions));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->VersionNumber = %x\n", standardInformation->VersionNumber));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->ClassId = %x\n", standardInformation->ClassId));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->OwnerId = %x\n", standardInformation->OwnerId));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->SecurityId = %x\n", standardInformation->SecurityId));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
							("standardInformation->QuotaCharged = %I64x\n", standardInformation->QuotaCharged));
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_TRACE,
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
						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO, 
							("fileName->FileName = %ws %x %d\n", 
							fileName->FileName, fileRecordSegmentHeader->Flags, BooleanFlagOn(fileRecordSegmentHeader->Flags, FILE_FILE_NAME_INDEX_PRESENT)));
					}
				}
			}
		
		} while(1); 

		if(fileRecordSegmentHeader)
			RtlCopyMemory(
				FileRecordSegmentHeader,
				fileRecordSegmentHeader,
				PrimarySession->BytesPerFileRecordSegment
				);
	}
	
	return STATUS_SUCCESS;
}


HANDLE
PrimarySessionOpenFile(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	POPEN_FILE			OpenFile
	)
{
	HANDLE					fileHandle = NULL;
	NTSTATUS				createStatus;

	ACCESS_MASK				desiredAccess;
	ULONG					attributes;
	OBJECT_ATTRIBUTES		objectAttributes;
	IO_STATUS_BLOCK			ioStatusBlock;
	LARGE_INTEGER			allocationSize;
	ULONG					fileAttributes;
	ULONG					shareAccess;
	ULONG					createDisposition;
	ULONG					createOptions;
	PFILE_FULL_EA_INFORMATION eaBuffer;
	ULONG					eaLength;			

		
	UNREFERENCED_PARAMETER(PrimarySession);
	
	ioStatusBlock.Information = 0;

	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
						| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT(desiredAccess == 0x0012019F);

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes(
			&objectAttributes,
			&OpenFile->FullFileName,
			attributes,
			NULL,
			NULL
			);
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
	eaBuffer		  = NULL;
	eaLength		  = 0;
				
	createStatus = ZwCreateFile(
						&fileHandle,
						desiredAccess,
						&objectAttributes,
						&ioStatusBlock,
						&allocationSize,
						fileAttributes,
						shareAccess,
						createDisposition,
						createOptions,
						eaBuffer,
						0
						);
		
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("PrimarySessionOpenFile: PrimarySession %p, %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
							PrimarySession, &OpenFile->FullFileName, createStatus, ioStatusBlock.Information));

	if(createStatus == STATUS_SUCCESS)
		return fileHandle;
	else
	{
		return NULL;
	}
}


NTSTATUS
DispatchWinXpRequest(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  _U32						DataSize,
	OUT	_U32						*replyDataSize
	);


NTSTATUS
DispatchWinXpRequest(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  _U32						DataSize,
	OUT	_U32						*replyDataSize
	)
{
	NTSTATUS	ntStatus;
	_U8			*ndfsRequestData;
	
#if DBG

	CHAR		irpMajorString[OPERATION_NAME_BUFFER_SIZE];
	CHAR		irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName (
		NdfsWinxpRequestHeader->IrpMajorFunction,
		NdfsWinxpRequestHeader->IrpMinorFunction,
		NdfsWinxpRequestHeader->FileSystemControl.FsControlCode,
		irpMajorString,
		irpMinorString
		);


    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: PrimarySession = %p, NdfsWinxpRequestHeader->IrpTag = %x, %s %s, DataSize = %d\n", 
					PrimarySession, NdfsWinxpRequestHeader->IrpTag, irpMajorString, irpMinorString, DataSize));
#endif

	if(DataSize)
		ndfsRequestData = (_U8 *)(NdfsWinxpRequestHeader+1);
	else
		ndfsRequestData = NULL;

#if DBG
	if(NdfsWinxpRequestHeader->IrpMajorFunction != IRP_MJ_CREATE)
	{	
		ULONG				openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE			openFile;

		UNICODE_STRING		RECYCLER;
//		ULONG				originalFileSpyDebugLevel; 
		
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		
		if(openFile != NULL)
		{
			RtlInitUnicodeString(&RECYCLER, L"\\RECYCLER");
		
			if(openFile->FileObject && openFile->FileObject->FileName.Buffer 
				&& openFile->FileObject->FileName.Length >= RECYCLER.Length
				&& wcsncmp(RECYCLER.Buffer, openFile->FileObject->FileName.Buffer, RECYCLER.Length) == 0)
			{	
				//originalFileSpyDebugLevel = gFileSpyDebugLevel;
				//gFileSpyDebugLevel = originalFileSpyDebugLevel;
			}
		}
	}

#endif

	switch(NdfsWinxpRequestHeader->IrpMajorFunction)
	{
    case IRP_MJ_CREATE: //0x00
	{
		UNICODE_STRING		fileName;
		WCHAR				fileNameBuffer[NDFS_MAX_PATH];
		NTSTATUS			createStatus;


		HANDLE				fileHandle = NULL;
	    ACCESS_MASK			desiredAccess;
		ULONG				attributes;
		OBJECT_ATTRIBUTES	objectAttributes;
		IO_STATUS_BLOCK		ioStatusBlock;
		LARGE_INTEGER		allocationSize;
		ULONG				fileAttributes;
	    ULONG				shareAccess;
	    ULONG				disposition;
		ULONG				createOptions;
	    PVOID				eaBuffer;
		ULONG				eaLength;
	    CREATE_FILE_TYPE	createFileType;
		ULONG				options;

		POPEN_FILE			openFile = NULL;
		ULONG				relatedOpenFileId = NdfsWinxpRequestHeader->Create.RelatedFileHandle;
		POPEN_FILE			relatedOpenFile = NULL;
		
		PFILE_RECORD_SEGMENT_HEADER	fileRecordSegmentHeader = NULL;
		

	    RtlInitEmptyUnicodeString( 
				&fileName,
                fileNameBuffer,
                sizeof(fileNameBuffer) 
				);

		if(relatedOpenFileId == 0) 
		{
		    SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: No RelatedFileHandle\n"));

			createStatus = RtlAppendUnicodeStringToString(
							&fileName,
							&PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName
							);

			if(createStatus != STATUS_SUCCESS)
			{
				ASSERT(LFS_UNEXPECTED);
				ntStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}
		else
		{
			relatedOpenFile = PrimarySession_FindOpenFile(
									PrimarySession,
									relatedOpenFileId
									);
			//ASSERT(relatedOpenFile && relatedOpenFile->OpenFileId == (_U32)relatedOpenFile);
		    
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: relatedOpenFile->OpenFileId = %X\n",
							relatedOpenFile->OpenFileId));
		}


		if(NdfsWinxpRequestHeader->Create.FileNameLength)
		{
			createStatus = RtlAppendUnicodeToString(
								&fileName,
								(PWCHAR)&ndfsRequestData[NdfsWinxpRequestHeader->Create.EaLength]
								);

			if(createStatus != STATUS_SUCCESS)
			{
				ASSERT(LFS_UNEXPECTED);
				ntStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}

	    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: fileName = %wZ\n", &fileName));
		

		desiredAccess			= NdfsWinxpRequestHeader->Create.DesiredAccess;

		attributes =  OBJ_KERNEL_HANDLE;
		if(!(NdfsWinxpRequestHeader->IrpSpFlags & SL_CASE_SENSITIVE))
			attributes |= OBJ_CASE_INSENSITIVE;

		InitializeObjectAttributes(
			&objectAttributes,
			&fileName,
			attributes,
			relatedOpenFileId ? relatedOpenFile->FileHandle : NULL,
			NULL
			);
		
		allocationSize.LowPart  = NdfsWinxpRequestHeader->Create.AllocationSizeLowPart;
		allocationSize.HighPart = NdfsWinxpRequestHeader->Create.AllocationSizeHighPart;

		fileAttributes			= NdfsWinxpRequestHeader->Create.FileAttributes;		
		shareAccess				= NdfsWinxpRequestHeader->Create.ShareAccess;
		disposition				= (NdfsWinxpRequestHeader->Create.Options & 0xFF000000) >> 24;
		createOptions			= NdfsWinxpRequestHeader->Create.Options & 0x00FFFFFF;
		// added by ktkim 03/15/2004
		createOptions			|= (NdfsWinxpRequestHeader->Create.FullCreateOptions & FILE_DELETE_ON_CLOSE);
		eaBuffer				= &ndfsRequestData[0];
		eaLength				= NdfsWinxpRequestHeader->Create.EaLength;
		createFileType			= CreateFileTypeNone;
		options					= NdfsWinxpRequestHeader->IrpSpFlags & 0x000000FF;
		
		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: DesiredAccess = %X. Synchronize:%d."
						" Dispo:%02lX CreateOptions = %X. Synchronize: Alert=%d Non-Alert=%d EaLen:%d.\n",
							desiredAccess,
							(desiredAccess & SYNCHRONIZE) != 0,
							disposition,
							createOptions,
							(createOptions & FILE_SYNCHRONOUS_IO_ALERT) != 0,
							(createOptions & FILE_SYNCHRONOUS_IO_NONALERT) != 0,
							eaLength
							) );

		if(NdfsWinxpRequestHeader->Create.FileNameLength == 0)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: desiredAccess = %x, fileAtributes %x, "
						"shareAccess %x, disposition %x, createOptions %x, createFileType %x\n",
							desiredAccess,
							fileAttributes,
							shareAccess,
							disposition,
							createOptions,
							createFileType
							) );
		
		//
		//	force the file to be synchronized.
		//	added by hootch
		desiredAccess |= SYNCHRONIZE;
		if(!(createOptions & FILE_SYNCHRONOUS_IO_NONALERT))
			createOptions |= FILE_SYNCHRONOUS_IO_ALERT;

		do 
		{
			createStatus = IoCreateFile(
							&fileHandle,
							desiredAccess,
							&objectAttributes,
							&ioStatusBlock,
							&allocationSize,
							fileAttributes,
							shareAccess,
							disposition,
							createOptions,
							eaBuffer,
							eaLength,
							createFileType,
							NULL,
							options
							);

			if(createStatus != STATUS_SHARING_VIOLATION)
				break;

			{
				POPEN_FILE	siblingOpenFile = NULL;		
			
				siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed(
									PrimarySession,
									&fileName,
									FALSE
									);
			
				if(siblingOpenFile == NULL)
					break;

				{
					NTSTATUS	closeStatus;

					ASSERT(siblingOpenFile->FileObject);
					ObDereferenceObject(siblingOpenFile->FileObject);
					siblingOpenFile->FileObject = NULL;
					closeStatus = ZwClose(siblingOpenFile->FileHandle);
					ASSERT(closeStatus == STATUS_SUCCESS);
					siblingOpenFile->AlreadyClosed = TRUE;
				}
			}

		} while(1);

		if(createStatus == STATUS_SUCCESS)
		{
			PFILE_OBJECT				fileObject;
			HANDLE						eventHandle;
			NTSTATUS					getStatus;


			ASSERT(fileHandle != 0);
			ASSERT(createStatus == STATUS_SUCCESS);
			ASSERT(createStatus == ioStatusBlock.Status);

			openFile = PrimarySession_AllocateOpenFile(
				PrimarySession,
				fileHandle,
				NULL,
				&fileName
				);
			if(openFile == NULL) {
				ASSERT(LFS_UNEXPECTED);
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR, ("Failed to allocate OpenFile structure.\n"));
				ntStatus = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			createStatus = ObReferenceObjectByHandle(
							fileHandle,
							FILE_READ_DATA,
							NULL,
							KernelMode,
							&fileObject,
							NULL);
	
			if(createStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_UNEXPECTED);		
			}

			openFile->FileObject = fileObject;

			createStatus = ZwCreateEvent(
							&eventHandle,
							GENERIC_READ,
							NULL,
							SynchronizationEvent,
							FALSE
							);
			if(createStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_UNEXPECTED);		
			}

			openFile->EventHandle = eventHandle;
			openFile->OpenFileId = (_U32)openFile;
			openFile->AlreadyClosed = FALSE;
			openFile->CleanUp = FALSE;
			openFile->DesiredAccess = desiredAccess;
			openFile->CreateOptions = createOptions;
			
			if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1
				&& NdfsWinxpRequestHeader->Create.FileNameLength 
				&& PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NTFS
				&& PrimarySession->BytesPerFileRecordSegment != 0)
			{
				fileRecordSegmentHeader = (PFILE_RECORD_SEGMENT_HEADER)(NdfsWinxpReplytHeader+1);
			
				getStatus = GetFileRecordSegmentHeader(
								PrimarySession,
								fileHandle,
								fileRecordSegmentHeader
								);

				if(getStatus != STATUS_SUCCESS) 
				{
					//ASSERT(LFS_UNEXPECTED);
					fileRecordSegmentHeader = NULL;
				}
				else
				{
					if(BooleanFlagOn(fileRecordSegmentHeader->Flags, FILE_FILE_NAME_INDEX_PRESENT))
						fileRecordSegmentHeader = NULL;
				}
			}
			else
				fileRecordSegmentHeader = NULL;
		}

#if DBG
		if(NdfsWinxpRequestHeader->Create.FileNameLength)
		{
			UNICODE_STRING	RECYCLER;
			PWCHAR			createFileName = (PWCHAR)&ndfsRequestData[NdfsWinxpRequestHeader->Create.EaLength];	
			

			RtlInitUnicodeString(&RECYCLER, L"\\RECYCLER");
	
			if(wcslen(createFileName) >= RECYCLER.Length
				&& wcsncmp(RECYCLER.Buffer, createFileName, RECYCLER.Length) == 0)
			{	
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: openFile->OpenFileId = %X, createStatus = %X, ioStatusBlock = %X\n",
						openFile ? openFile->OpenFileId : 0, createStatus, ioStatusBlock.Information));
			}
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: openFile->OpenFileId = %X, createStatus = %X, ioStatusBlock = %X\n",
						openFile ? openFile->OpenFileId : 0, createStatus, ioStatusBlock.Information));
#endif
		
		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction	= NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction	= NdfsWinxpRequestHeader->IrpMinorFunction;

		//NdfsWinxpReplytHeader->Reply		  = LFS_WINXP_REPLY_CREATE;
		//NdfsWinxpReplytHeader->ReplyResult = LFS_WINXP_REPLY_SUCCESS;

		NdfsWinxpReplytHeader->Status	  = createStatus;
		NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;

		if(createStatus == STATUS_SUCCESS)
		{
			NdfsWinxpReplytHeader->Open.FileHandle = openFile->OpenFileId;
			NdfsWinxpReplytHeader->Open.SetSectionObjectPointer = openFile->FileObject->SectionObjectPointer ? TRUE : FALSE;
		}

		if(fileRecordSegmentHeader)
		{
			LARGE_INTEGER  currentTime;

			KeQuerySystemTime(&currentTime);	

			*replyDataSize = PrimarySession->BytesPerFileRecordSegment;

			NdfsWinxpReplytHeader->Open.OpenTimeHigPart = currentTime.HighPart;
			NdfsWinxpReplytHeader->Open.OpenTimeLowPartMsb = ((_U8 *)&currentTime.LowPart)[3];
		}
		else
		{
			*replyDataSize = 0;
		}

		ntStatus = STATUS_SUCCESS;

		break;
	}

   case IRP_MJ_CLOSE: // 0x02
	{		
		ULONG				openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE			openFile;
		KIRQL				oldIrql;

		
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile);
		
		if(openFile->AlreadyClosed == FALSE)
		{
			NTSTATUS			closeStatus;

			ASSERT(openFile->FileObject);
			ObDereferenceObject(openFile->FileObject);
			openFile->FileObject = NULL;
			closeStatus = ZwClose(openFile->FileHandle);
			ASSERT(closeStatus == STATUS_SUCCESS);
			closeStatus = ZwClose(openFile->EventHandle);
			ASSERT(closeStatus == STATUS_SUCCESS);
			openFile->AlreadyClosed = TRUE;
		}
		
		KeAcquireSpinLock(&PrimarySession->OpenedFileQSpinLock, &oldIrql);
		RemoveEntryList(&openFile->ListEntry);
		KeReleaseSpinLock(&PrimarySession->OpenedFileQSpinLock, oldIrql);
		
		InitializeListHead(&openFile->ListEntry);
		PrimarySession_FreeOpenFile(
				PrimarySession,
				openFile
				);
		
		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status		= STATUS_SUCCESS;
		NdfsWinxpReplytHeader->Information	= 0;

		*replyDataSize = 0;
		ntStatus = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_READ: // 0x03
	{
		NTSTATUS				readStatus;
		//NTSTATUS				ioCallStatus;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;

		HANDLE					fileHandle = NULL;	
		BOOLEAN					closeOnExit = FALSE;

		BOOLEAN					synchronousIo;
		
	    //PDEVICE_OBJECT		deviceObject;
		//PIRP					irp;
	    //PIO_STACK_LOCATION	irpSp;


		IO_STATUS_BLOCK			ioStatusBlock;
		PVOID					buffer;
		ULONG					length;
		LARGE_INTEGER			byteOffset;
		ULONG					key;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);

		if(openFile->AlreadyClosed == TRUE)
		{
			POPEN_FILE	siblingOpenFile;
		    ACCESS_MASK	desiredAccess = FILE_READ_DATA;

			siblingOpenFile = PrimarySession_FindOpenFileByName(
								PrimarySession,
								&openFile->FullFileName,
								desiredAccess
								);
			
			if(siblingOpenFile)
			{
				fileHandle = siblingOpenFile->FileHandle;
				closeOnExit = FALSE;
			}
			else
			{
				fileHandle = PrimarySessionOpenFile(PrimarySession, openFile);

				if(fileHandle == NULL)
				{
					ASSERT(LFS_REQUIRED);			
					ntStatus = STATUS_UNSUCCESSFUL;

					break;
				}
				else
					closeOnExit = TRUE;
			}
		}
		else
		{
			ASSERT(openFile && openFile->FileObject);
			fileHandle = openFile->FileHandle;
			closeOnExit = FALSE;
		}

		RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));

		buffer				= (_U8 *)(NdfsWinxpReplytHeader+1);

		length				= NdfsWinxpRequestHeader->Read.Length;
		byteOffset.QuadPart = NdfsWinxpRequestHeader->Read.ByteOffset;
		
		key = (openFile->AlreadyClosed == FALSE) ? NdfsWinxpRequestHeader->Read.Key : 0;
		synchronousIo = openFile->FileObject ? BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO) : TRUE;

		if(synchronousIo)
		{
			readStatus = NtReadFile(
							fileHandle,
							NULL,
							NULL,
							NULL,
							&ioStatusBlock,
							buffer,
							length,
							&byteOffset,
							key ? &key : NULL
							);
		}
		else
		{
			readStatus = NtReadFile(
								fileHandle,
								openFile->EventHandle,
								NULL,
								NULL,
								&ioStatusBlock,
								buffer,
								length,
								&byteOffset,
								key ? &key : NULL
								);
		
			if (readStatus == STATUS_PENDING) 
			{
				readStatus = ZwWaitForSingleObject(openFile->EventHandle, TRUE, NULL);
			}
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: ZwReadFile: openFileId = %X, synchronousIo = %d, length = %d, readStatus = %X, byteOffset = %I64d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						openFileId, synchronousIo, length, readStatus, byteOffset.QuadPart, ioStatusBlock.Status, ioStatusBlock.Information));

		if(NT_SUCCESS(readStatus))
		{
#if 0
			FILE_STANDARD_INFORMATION	fileStandardInformation;
			NTSTATUS					queryInformationStatus;
		    IO_STATUS_BLOCK				queryIoStatusBlock;
			
			ASSERT(readStatus == STATUS_SUCCESS);
			ASSERT(readStatus == ioStatusBlock.Status);


			queryInformationStatus = ZwQueryInformationFile(
											fileHandle,
											&queryIoStatusBlock,
											&fileStandardInformation,
											sizeof(FILE_STANDARD_INFORMATION),
											FileStandardInformation
											);
			
			ASSERT(queryInformationStatus == STATUS_SUCCESS);

			ASSERT(fileStandardInformation.EndOfFile.QuadPart >= byteOffset.QuadPart + ioStatusBlock.Information);
#endif	    
			
		}else
		{
			ASSERT(ioStatusBlock.Information == 0);
			ioStatusBlock.Information = 0;
		}

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status		= readStatus;
		NdfsWinxpReplytHeader->Information	= ioStatusBlock.Information;
		//NdfsWinxpReplytHeader->CurrentByteOffset = openFile->FileObject->CurrentByteOffset.QuadPart;

		*replyDataSize = ioStatusBlock.Information;
		ASSERT(*replyDataSize <= NdfsWinxpRequestHeader->Read.Length);
		
		ntStatus = STATUS_SUCCESS;

		if(closeOnExit)
			ZwClose(fileHandle);

		break;
	}
	

    case IRP_MJ_WRITE: // 0x04
	{
		NTSTATUS				writeStatus;
		LONG					trial = 0;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;

		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;
		BOOLEAN					closeOnExit = FALSE;

		IO_STATUS_BLOCK			ioStatusBlock;
		PVOID					buffer;
		ULONG					length;
		LARGE_INTEGER			byteOffset;
		ULONG					key;

		
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);

		if(openFile->AlreadyClosed == FALSE)
		{
			ASSERT(openFile && openFile->FileObject);
			fileHandle = openFile->FileHandle;
			fileObject = openFile->FileObject;
			closeOnExit = FALSE;
		}
		else
		{
			POPEN_FILE	siblingOpenFile = NULL;
		    ACCESS_MASK	desiredAccess;

RETRY_WRITE:
			
			desiredAccess = FILE_GENERIC_WRITE;

			siblingOpenFile = PrimarySession_FindOpenFileByName(
								PrimarySession,
								&openFile->FullFileName,
								desiredAccess
								);
			
			if(siblingOpenFile)
			{
				fileHandle = siblingOpenFile->FileHandle;
				fileObject = siblingOpenFile->FileObject;
				closeOnExit = FALSE;
			}
			else
			{
				while(1) 
				{
					POPEN_FILE	siblingOpenFile;

					fileHandle = PrimarySessionOpenFile(PrimarySession, openFile);

					if(fileHandle)
					{
						NTSTATUS	createStatus;

						createStatus = ObReferenceObjectByHandle(fileHandle, FILE_READ_DATA, NULL, KernelMode, &fileObject, NULL);
						if(createStatus != STATUS_SUCCESS) 
						{
							ASSERT(LFS_UNEXPECTED);
							ZwClose(fileHandle);
							fileHandle = NULL;
						}
						else
							closeOnExit = TRUE;

						break;
					}
									
					siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed(
										PrimarySession,
										&openFile->FullFileName,
										FALSE
										);
			
					if(siblingOpenFile) 
					{
						NTSTATUS	closeStatus;

						ASSERT(siblingOpenFile->FileObject);
						ObDereferenceObject(siblingOpenFile->FileObject);
						siblingOpenFile->FileObject = NULL;
						closeStatus = ZwClose(siblingOpenFile->FileHandle);
						ASSERT(closeStatus == STATUS_SUCCESS);
						closeStatus = ZwClose(siblingOpenFile->EventHandle);
						ASSERT(closeStatus == STATUS_SUCCESS);
						siblingOpenFile->AlreadyClosed = TRUE;

						continue;
					}
					else
						break;
				}

				if(fileHandle == NULL)
				{
					ASSERT(LFS_REQUIRED);
					
					NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
					NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
					NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

					NdfsWinxpReplytHeader->Status	   = STATUS_SUCCESS;
					NdfsWinxpReplytHeader->Information = NdfsWinxpRequestHeader->Write.Length;

					*replyDataSize = NdfsWinxpRequestHeader->Write.Length;		
					ntStatus = STATUS_SUCCESS;

					break;
				}
			}

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("IRP_MJ_WRITE: PrimarySession %p, %wZ, siblingOpenFile = %p, closeOnExit = %d\n",
							PrimarySession, &openFile->FullFileName, siblingOpenFile, closeOnExit));
		}			


		buffer				= (_U8 *)(NdfsWinxpRequestHeader+1);
		length				= NdfsWinxpRequestHeader->Write.Length;
		byteOffset.QuadPart = NdfsWinxpRequestHeader->Write.ByteOffset;

		key = (openFile->AlreadyClosed == FALSE) ? NdfsWinxpRequestHeader->Write.Key : 0;

		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);
		
		writeStatus = NtWriteFile(
							fileHandle,
							NULL,
							NULL,
							NULL,
							&ioStatusBlock,
							buffer,
							length,
							&byteOffset,
							key ? &key : NULL
							);

#if 0
		if(PrimarySession->ExtendWinxpReplyMessagePool != NULL
			|| writeStatus != STATUS_SUCCESS)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: ZwWriteFile: openFileId = %X, %wZ, openFile->CleanUp = %d, openFile->DesiredAccess = %x, writeStatus = %x, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						openFileId, &openFile->FullFileName, openFile->CleanUp, openFile->DesiredAccess, writeStatus, ioStatusBlock.Status, ioStatusBlock.Information));
#endif
		
		if(NT_SUCCESS(writeStatus))
		{
			ASSERT(writeStatus == STATUS_SUCCESS);
			ASSERT(writeStatus == ioStatusBlock.Status);
		}else
		{
			ASSERT(ioStatusBlock.Information == 0);
			ioStatusBlock.Information = 0;
		}

		NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status			 = writeStatus;
		NdfsWinxpReplytHeader->Information		 = ioStatusBlock.Information;
		NdfsWinxpReplytHeader->CurrentByteOffset = fileObject->CurrentByteOffset.QuadPart;

		*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		if(closeOnExit == TRUE)
		{
			ObDereferenceObject(fileObject);
			ZwClose(fileHandle);
		}
		else if(writeStatus == STATUS_ACCESS_DENIED)
		{
			if(openFile->AlreadyClosed == TRUE)
			{
				ASSERT(LFS_UNEXPECTED);
				
				NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
				NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
				NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

				NdfsWinxpReplytHeader->Status	   = STATUS_SUCCESS;
				NdfsWinxpReplytHeader->Information = NdfsWinxpRequestHeader->Write.Length;

				*replyDataSize = NdfsWinxpRequestHeader->Write.Length;		
				ntStatus = STATUS_SUCCESS;
			}
			else if(openFile->CleanUp == TRUE)
			{
				NTSTATUS	closeStatus;

				ASSERT(openFile->FileObject);
				ObDereferenceObject(openFile->FileObject);
				openFile->FileObject = NULL;
				closeStatus = ZwClose(openFile->FileHandle);
				ASSERT(closeStatus == STATUS_SUCCESS);
				closeStatus = ZwClose(openFile->EventHandle);
				ASSERT(closeStatus == STATUS_SUCCESS);
				openFile->AlreadyClosed = TRUE;
				
				goto RETRY_WRITE;
			}
			else
			{
				goto RETRY_WRITE;
			}
		}

		break;
	}
	
    case IRP_MJ_QUERY_INFORMATION: // 0x05
	{
		NTSTATUS				queryInformationStatus;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;
		BOOLEAN					closeOnExit = FALSE;

	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;
		ULONG					returnedLength = 0;
		

		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);

		if(openFile->AlreadyClosed == TRUE)
		{
			POPEN_FILE	siblingOpenFile;
		    ACCESS_MASK	desiredAccess = FILE_READ_ATTRIBUTES;

			siblingOpenFile = PrimarySession_FindOpenFileByName(
								PrimarySession,
								&openFile->FullFileName,
								desiredAccess
								);
			
			if(siblingOpenFile)
			{
				fileHandle = siblingOpenFile->FileHandle;
				fileObject = siblingOpenFile->FileObject;
				closeOnExit = FALSE;
			}
			else
			{
				fileHandle = PrimarySessionOpenFile(PrimarySession, openFile);

				if(fileHandle)
				{
					NTSTATUS	createStatus;

					createStatus = ObReferenceObjectByHandle(fileHandle, FILE_READ_DATA, NULL, KernelMode, &fileObject, NULL);
					if(createStatus != STATUS_SUCCESS) 
					{
						ASSERT(LFS_UNEXPECTED);
						ZwClose(fileHandle);
						fileHandle = NULL;
					}
					else
						closeOnExit = TRUE;
				}

				if(fileHandle == NULL)
				{
					ASSERT(LFS_REQUIRED);
#if 0
					NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
					NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
					NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

					NdfsWinxpReplytHeader->Status	   = STATUS_HANDLES_CLOSED;
					NdfsWinxpReplytHeader->Information = 0;

					*replyDataSize = 0;		
#endif
					ntStatus = STATUS_UNSUCCESSFUL;

					break;
				}
			}

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("IRP_MJ_QUERY_INFORMATION: PrimarySession %p, %wZ, siblingOpenFile = %p, closeOnExit = %d\n",
							PrimarySession, &openFile->FullFileName, siblingOpenFile, closeOnExit));
		}			
		else
		{
			ASSERT(openFile && openFile->FileObject);
			fileHandle = openFile->FileHandle;
			fileObject = openFile->FileObject;
			closeOnExit = FALSE;
		}

		ASSERT(NdfsWinxpRequestHeader->QueryFile.Length <= PrimarySession->PrimaryMaxDataSize);
		fileInformation		 = (_U8 *)(NdfsWinxpReplytHeader+1);
		length				 = NdfsWinxpRequestHeader->QueryFile.Length;
		fileInformationClass = NdfsWinxpRequestHeader->QueryFile.FileInformationClass;


		queryInformationStatus = IoQueryFileInformation(
										fileObject,
										fileInformationClass,
										length,
										fileInformation,
										&returnedLength
										);

		if(queryInformationStatus == STATUS_BUFFER_OVERFLOW)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: IoQueryFileInformation: openFileId = %X, length = %d, queryInformationStatus = %X, returnedLength = %d\n",
						openFileId, length, queryInformationStatus, returnedLength));

		if(NT_SUCCESS(queryInformationStatus))
			ASSERT(queryInformationStatus == STATUS_SUCCESS);

		if(queryInformationStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(length == returnedLength);

		if(!(queryInformationStatus == STATUS_SUCCESS || queryInformationStatus == STATUS_BUFFER_OVERFLOW))
		{
			returnedLength = 0;
			//ASSERT(returnedLength == 0);
		}
		
		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;
		
		NdfsWinxpReplytHeader->Status	   = queryInformationStatus;
		NdfsWinxpReplytHeader->Information = returnedLength;

		//*replyDataSize = NdfsWinxpRequestHeader->QueryFile.Length <= returnedLength 
		//					? NdfsWinxpRequestHeader->QueryFile.Length : returnedLength;
		*replyDataSize = returnedLength;
		
		ntStatus = STATUS_SUCCESS;

		if(closeOnExit == TRUE)
		{
			ObDereferenceObject(fileObject);
			ZwClose(fileHandle);
		}

		break;
	}
	

    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		NTSTATUS				setInformationStatus;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		fileInformation = ExAllocatePoolWithTag( 
								NonPagedPool, 
								NdfsWinxpRequestHeader->SetFile.Length 
								+ PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length,
								PRIMARY_SESSION_BUFFERE_TAG
								);

		if (fileInformation == NULL)
		{
			ASSERT(LFS_INSUFFICIENT_RESOURCES);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(
			fileInformation,
			NdfsWinxpRequestHeader->SetFile.Length 
				+ PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length
			);

		if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileBasicInformation)
		{
			PFILE_BASIC_INFORMATION		basicInformation = fileInformation;
				
			basicInformation->CreationTime.QuadPart   = NdfsWinxpRequestHeader->SetFile.BasicInformation.CreationTime;
			basicInformation->LastAccessTime.QuadPart = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastAccessTime;
			basicInformation->LastWriteTime.QuadPart  = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastWriteTime;
			basicInformation->ChangeTime.QuadPart     = NdfsWinxpRequestHeader->SetFile.BasicInformation.ChangeTime;
			basicInformation->FileAttributes          = NdfsWinxpRequestHeader->SetFile.BasicInformation.FileAttributes;

		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileLinkInformation) 
		{
			PFILE_LINK_INFORMATION		linkInfomation = fileInformation;
			POPEN_FILE					rootDirectoryFile;

			ASSERT(sizeof(FILE_LINK_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
				
			if(NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0)
			{
				rootDirectoryFile = PrimarySession_FindOpenFile(
										PrimarySession,
										NdfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle
										);
				ASSERT(rootDirectoryFile->AlreadyClosed == FALSE);
			}
			else
				rootDirectoryFile = NULL;

			linkInfomation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists;
			linkInfomation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;
			linkInfomation->FileNameLength = NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength;
			
			RtlCopyMemory(
				linkInfomation->FileName,
				ndfsRequestData,
				linkInfomation->FileNameLength
				);
			
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: linkInfomation->FileNameLength = %ws\n", linkInfomation->FileNameLength));
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileRenameInformation) 
		{
			PFILE_RENAME_INFORMATION	renameInformation = fileInformation;
			POPEN_FILE					rootDirectoryFile;
			
			ASSERT(sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
						
			if(NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0)
			{
				rootDirectoryFile = PrimarySession_FindOpenFile(
										PrimarySession,
										NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle
										);

				ASSERT(rootDirectoryFile->AlreadyClosed == FALSE);
			}
			else
				rootDirectoryFile = NULL;
		
			renameInformation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists;
			renameInformation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;

#if 0
			renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
			RtlCopyMemory(
				renameInformation->FileName,
				ndfsRequestData,
				renameInformation->FileNameLength
				);

			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName));
#endif
			
			//
			// Check to see whether or not a fully qualified pathname was supplied.
			// If so, then more processing is required.
			//
			if(ndfsRequestData[0] == (UCHAR) OBJ_NAME_PATH_SEPARATOR ||
				renameInformation->RootDirectory != NULL
				) 
			{
				ULONG		byteoffset ;
				ULONG		idx_unicodestr ;
				PWCHAR		unicodestr ;
				BOOLEAN		found ;

				found = FALSE ;
				unicodestr = (PWCHAR)ndfsRequestData;
				for(	idx_unicodestr = 0 ;
						idx_unicodestr < ( NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength / sizeof(WCHAR) - 1 );
						idx_unicodestr ++
					) {
					if( L':'  == unicodestr[idx_unicodestr] &&
						L'\\' == unicodestr[idx_unicodestr + 1] ) {

						idx_unicodestr ++;
						found = TRUE;
						break ;
					}
				}

				if(found) {
					RtlCopyMemory(
						renameInformation->FileName,
						PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Buffer,
						PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length
					);
					renameInformation->FileNameLength 
						= PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length;

					byteoffset = idx_unicodestr*sizeof(WCHAR);
					RtlCopyMemory(
							(PUCHAR)renameInformation->FileName + renameInformation->FileNameLength,
							(PUCHAR)ndfsRequestData + byteoffset,
							NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset
						) ;
					renameInformation->FileNameLength += NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset ;
				} else {
					ASSERT(LFS_BUG);
				}

				SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName));
			} 
			else 
			{
				renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
				RtlCopyMemory(
					renameInformation->FileName,
					ndfsRequestData,
					renameInformation->FileNameLength
				);
			}
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileDispositionInformation) 
		{
			PFILE_DISPOSITION_INFORMATION	dispositionInformation = fileInformation;
			
			dispositionInformation->DeleteFile = NdfsWinxpRequestHeader->SetFile.DispositionInformation.DeleteFile;
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileEndOfFileInformation) 
		{
			PFILE_END_OF_FILE_INFORMATION fileEndOfFileInformation = fileInformation;
			
			fileEndOfFileInformation->EndOfFile.QuadPart = NdfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile;
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileAllocationInformation) 
		{
			PFILE_ALLOCATION_INFORMATION fileAllocationInformation = fileInformation;

			fileAllocationInformation->AllocationSize.QuadPart = NdfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize;
		}
		else if( FilePositionInformation == NdfsWinxpRequestHeader->SetFile.FileInformationClass) 
		{
			PFILE_POSITION_INFORMATION filePositionInformation = fileInformation;

			filePositionInformation->CurrentByteOffset.QuadPart = NdfsWinxpRequestHeader->SetFile.PositionInformation.CurrentByteOffset;
		}
		else
			ASSERT(LFS_BUG);


		length = NdfsWinxpRequestHeader->SetFile.Length;
		fileInformationClass = NdfsWinxpRequestHeader->SetFile.FileInformationClass;
		
		setInformationStatus = IoSetInformation(
										openFile->FileObject,
										fileInformationClass,
										length,
										fileInformation
										);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: IoSetInformation: fileHandle = %X, fileInformationClass = %X, setInformationStatus = %X\n",
						openFile->FileHandle, fileInformationClass, setInformationStatus));

		if(NT_SUCCESS(setInformationStatus))
			ASSERT(setInformationStatus == STATUS_SUCCESS);
		
		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	   = setInformationStatus;

		if(NT_SUCCESS(setInformationStatus))
			NdfsWinxpReplytHeader->Information = length;
		else
			NdfsWinxpReplytHeader->Information = 0;

		*replyDataSize = 0;
		
		
		ntStatus = STATUS_SUCCESS;

		ASSERT(fileInformation);
		
		ExFreePoolWithTag(
			fileInformation,
			PRIMARY_SESSION_BUFFERE_TAG
			);

		break;
	}
	
     case IRP_MJ_FLUSH_BUFFERS: // 0x09
	{		
#if 0
		NTSTATUS			flushBufferStatus;

		
		closeStatus = ZwClose((HANDLE)NdfsWinxpRequestHeader->FileHandle);

		ASSERT(closeStatus == STATUS_SUCCESS);
		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
						("DispatchWinXpRequest: IRP_MJ_CLOSE: openFileId = %x, closeStatus = %x\n",
										openFileId, closeStatus));
#endif

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = STATUS_SUCCESS;
		NdfsWinxpReplytHeader->Information = 0;

		*replyDataSize = 0;

		
		ntStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: // 0x0A
	{
		NTSTATUS				queryVolumeInformationStatus;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;

	    PVOID					fsInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fsInformationClass;
	    ULONG					returnedLength = 0;
		

		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		ASSERT(NdfsWinxpRequestHeader->QueryVolume.Length <= PrimarySession->PrimaryMaxDataSize);
		fsInformation		 = (_U8 *)(NdfsWinxpReplytHeader+1);
		length				 = NdfsWinxpRequestHeader->QueryVolume.Length;
		fsInformationClass   = NdfsWinxpRequestHeader->QueryVolume.FsInformationClass;
		
		queryVolumeInformationStatus = IoQueryVolumeInformation(
										openFile->FileObject,
										fsInformationClass,
										length,
										fsInformation,
										&returnedLength
										);

		if(NT_SUCCESS(queryVolumeInformationStatus))
			ASSERT(queryVolumeInformationStatus == STATUS_SUCCESS);

		if(queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(length == returnedLength);

		if(!(queryVolumeInformationStatus == STATUS_SUCCESS || queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW))
		{
			returnedLength = 0;
			//ASSERT(returnedLength == 0);
		} 
#ifdef __ENABLE_RECYLEBIN_PATCH__
		else {
			//
			//	Added to patch recycle bin corruption problems.
			//	When we fix the problem, remove this if-clause.
			//
			if(fsInformationClass == FileFsAttributeInformation) {
				PFILE_FS_ATTRIBUTE_INFORMATION info = (PFILE_FS_ATTRIBUTE_INFORMATION)fsInformation;
				info->FileSystemAttributes &= ~(FILE_PERSISTENT_ACLS);
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("PRIMARY: FileFsAttributeInformation: turn off FILE_PERSISTENT_ACLS\n"));
			}
		}
#endif

		if(queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: IoQueryVolumeInformation: fileHandle = %X, queryVolumeInformationStatus = %X, length = %d, returnedLength = %d\n",
						openFile->FileHandle, queryVolumeInformationStatus, length, returnedLength));


		NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	     = queryVolumeInformationStatus;
		NdfsWinxpReplytHeader->Information   = returnedLength;

		//*replyDataSize = NdfsWinxpRequestHeader->QueryVolume.Length <= returnedLength 
		//					? NdfsWinxpRequestHeader->QueryVolume.Length : returnedLength;
		*replyDataSize = returnedLength;
		
		ntStatus = STATUS_SUCCESS;

		break;
	}
	case IRP_MJ_SET_VOLUME_INFORMATION:
	{
		NTSTATUS				setVolumeStatus;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
		IO_STATUS_BLOCK			IoStatusBlock;
	    PVOID					volumeInformation;
		ULONG					length;
		FS_INFORMATION_CLASS	volumeInformationClass;

		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);

		length				   = NdfsWinxpRequestHeader->SetVolume.Length;
		volumeInformationClass = NdfsWinxpRequestHeader->SetVolume.FsInformationClass ;
		volumeInformation	   = (_U8 *)(NdfsWinxpRequestHeader+1);

		setVolumeStatus = ZwSetVolumeInformationFile(
									openFile->FileHandle,
									&IoStatusBlock,
									volumeInformation,
									length,
									volumeInformationClass
								); 

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: ZwSetVolumeInformationFile: fileHandle = %X, volumeInformationClass = %X, setVolumeStatus = %X\n",
						openFile->FileHandle, volumeInformationClass, setVolumeStatus));

		if(NT_SUCCESS(setVolumeStatus))
			ASSERT(setVolumeStatus == STATUS_SUCCESS);

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = setVolumeStatus;
		NdfsWinxpReplytHeader->Information = length;

		*replyDataSize = 0;

		ntStatus = STATUS_SUCCESS;
		break ;
	}

	case IRP_MJ_DIRECTORY_CONTROL: // 0x0C
	{
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
				("DispatchWinXpRequest: IRP_MJ_DIRECTORY_CONTROL: MinorFunction = %X\n",
					NdfsWinxpRequestHeader->IrpMajorFunction));

        if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_QUERY_DIRECTORY) 
		{
			NTSTATUS				queryDirectoryStatus;

			ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
			POPEN_FILE				openFile;
		
			BOOLEAN					synchronousIo;

			IO_STATUS_BLOCK			ioStatusBlock;
		    PVOID					fileInformation;
			ULONG					length;
			FILE_INFORMATION_CLASS	fileInformationClass;
			BOOLEAN					returnSingleEntry;
			UNICODE_STRING			fileName;
			WCHAR					fileNameBuffer[NDFS_MAX_PATH];
			BOOLEAN					restartScan;
 		

			openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
			ASSERT(openFile && openFile->FileObject);
			//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

			ASSERT(NdfsWinxpRequestHeader->QueryDirectory.Length <= PrimarySession->PrimaryMaxDataSize);
	
			fileInformation			= (_U8 *)(NdfsWinxpReplytHeader+1);
			length					= NdfsWinxpRequestHeader->QueryDirectory.Length;
			fileInformationClass	= NdfsWinxpRequestHeader->QueryDirectory.FileInformationClass;
			returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
			restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;

			RtlInitEmptyUnicodeString( 
					&fileName,
					fileNameBuffer,
					sizeof(fileNameBuffer) 
					);

			queryDirectoryStatus = RtlAppendUnicodeToString(
										&fileName,
										(PWCHAR)ndfsRequestData
										);

			if(queryDirectoryStatus != STATUS_SUCCESS)
			{
				ASSERT(LFS_UNEXPECTED);
				ntStatus = STATUS_UNSUCCESSFUL;
				break;
			}


			RtlZeroMemory(
				&ioStatusBlock,
				sizeof(ioStatusBlock)
				);

			synchronousIo = BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO);

			if(synchronousIo)
			{
				queryDirectoryStatus = NtQueryDirectoryFile(
										openFile->FileHandle,
										NULL,
										NULL,
										NULL,
										&ioStatusBlock,
										fileInformation,
										length,
										fileInformationClass,
										returnSingleEntry,
										&fileName,
										restartScan
										);
			}
			else
			{
			queryDirectoryStatus = NtQueryDirectoryFile(
										openFile->FileHandle,
										openFile->EventHandle,
										NULL,
										NULL,
										&ioStatusBlock,
										fileInformation,
										length,
										fileInformationClass,
										returnSingleEntry,
										&fileName,
										restartScan
										);

				if (queryDirectoryStatus == STATUS_PENDING) 
				{
					queryDirectoryStatus = ZwWaitForSingleObject(openFile->EventHandle, TRUE, NULL);
				}
			}

			if(queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: NtQueryDirectoryFile: openFileId = %X, queryDirectoryStatus = %X, length = %d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
										openFileId, queryDirectoryStatus, length, ioStatusBlock.Status, ioStatusBlock.Information));

#if 0
			//
			//	sometimes it returns STATUS_PENDING even though NtQueryDirectoryFile() is a synchronous operation.
			//	We assume STATUS_PENDING as STATUS_SUCCESS here.
			//
			if(STATUS_PENDING == queryDirectoryStatus) {
				ASSERT(LFS_REQUIRED);
				SPY_LOG_PRINT( SPYDEBUG_TRACE_LEVEL1, ("LFS: DispatchWinXpRequest: translate STATUS_PENDING to %08lx\n", ioStatusBlock.Status)) ;	
				queryDirectoryStatus = ioStatusBlock.Status ;
			}
#endif

			if(NT_SUCCESS(queryDirectoryStatus))
			{
				ASSERT(queryDirectoryStatus == STATUS_SUCCESS);		
				ASSERT(queryDirectoryStatus == ioStatusBlock.Status);
			}
			
			if(queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
				ASSERT(length == ioStatusBlock.Information);

			if(!(queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW))
			{
				ASSERT(ioStatusBlock.Information == 0);
				ioStatusBlock.Information = 0;
			}

			NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
			NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
			NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

			NdfsWinxpReplytHeader->Status	  = queryDirectoryStatus;
			NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;

			if(queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
			{
				if(ioStatusBlock.Information)
					*replyDataSize = CalculateSafeLength(
										fileInformationClass,
										length,
										ioStatusBlock.Information,
										fileInformation
										);
			}else


			{
				*replyDataSize = 0;
			}

			//*replyDataSize = NdfsWinxpRequestHeader->QueryDirectory.Length <= ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->QueryDirectory.Length : ioStatusBlock.Information;
			//*replyDataSize = ioStatusBlock.Information;
		
			ntStatus = STATUS_SUCCESS;
		} else
		{
			ASSERT(LFS_BUG);
			ntStatus = STATUS_UNSUCCESSFUL;
		}

		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: // 0x0D
	{
		NTSTATUS		fileSystemControlStatus;

		ULONG			openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			fsControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength <= MAX_PRIMARY_SESSION_SEND_BUFFER);


		fsControlCode		= NdfsWinxpRequestHeader->FileSystemControl.FsControlCode;
		inputBufferLength	= NdfsWinxpRequestHeader->FileSystemControl.InputBufferLength;

		if(fsControlCode == FSCTL_MOVE_FILE)		// 29
		{
			MOVE_FILE_DATA	moveFileData;	
			POPEN_FILE		moveFile;
			
			ASSERT(sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
						
			moveFile = PrimarySession_FindOpenFile(
									PrimarySession,
									NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.FileHandle
									);

			ASSERT(moveFile->AlreadyClosed == FALSE);

			moveFileData.FileHandle				= moveFile->FileHandle;
			moveFileData.StartingVcn.QuadPart	= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingVcn;
			moveFileData.StartingLcn.QuadPart	= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingLcn;
			moveFileData.ClusterCount			= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.ClusterCount;

			inputBuffer = &moveFileData;
		} 
		else if(fsControlCode == FSCTL_MARK_HANDLE)	// 63
		{
			MARK_HANDLE_INFO	markHandleInfo;
			POPEN_FILE			markFile;

			markFile = PrimarySession_FindOpenFile(
									PrimarySession,
									NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.VolumeHandle
									);

			ASSERT(markFile->AlreadyClosed == FALSE);

			markHandleInfo.UsnSourceInfo = NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.UsnSourceInfo;
			markHandleInfo.VolumeHandle	 = markFile->FileHandle;
			markHandleInfo.HandleInfo    = NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.HandleInfo;

			inputBuffer = &markHandleInfo;
		}
		else
		{
			inputBuffer	= ndfsRequestData;
		}

		outputBuffer		= (_U8 *)(NdfsWinxpReplytHeader+1);
		outputBufferLength	= NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;

		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);

		fileSystemControlStatus = ZwFsControlFile(
									openFile->FileHandle,
									NULL,
									NULL,
									NULL,
									&ioStatusBlock,
									fsControlCode,
									inputBuffer,
									inputBufferLength,
									outputBuffer,
									outputBufferLength
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: IRP_MJ_FILE_SYSTEM_CONTROL: openFileId = %x, function = %d, fileSystemControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
										openFileId, (fsControlCode & 0x00003FFC) >> 2, fileSystemControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
		if(NT_SUCCESS(fileSystemControlStatus))
		{
			ASSERT(fileSystemControlStatus == STATUS_SUCCESS);	
			ASSERT(fileSystemControlStatus == ioStatusBlock.Status);
		}

		if(fileSystemControlStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(ioStatusBlock.Information == inputBufferLength || ioStatusBlock.Information == outputBufferLength);
		
		if(!(fileSystemControlStatus == STATUS_SUCCESS || fileSystemControlStatus == STATUS_BUFFER_OVERFLOW))
		{
			ioStatusBlock.Information = 0;
			ASSERT(ioStatusBlock.Information == 0);
		}

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = fileSystemControlStatus;
		NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;

		if(outputBufferLength)
		{
			*replyDataSize = ioStatusBlock.Information;
			//*replyDataSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength <= ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength : ioStatusBlock.Information;
		}
		else
			*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_DEVICE_CONTROL: // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	{
		NTSTATUS		deviceControlStatus;

		ULONG			openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			ioControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 		

		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		//ASSERT(NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength <= PrimarySession->PrimaryMaxBufferSize);

		ioControlCode		= NdfsWinxpRequestHeader->DeviceIoControl.IoControlCode;
		inputBuffer			= ndfsRequestData;
		inputBufferLength	= NdfsWinxpRequestHeader->DeviceIoControl.InputBufferLength;
		outputBuffer		= (_U8 *)(NdfsWinxpReplytHeader+1);
		outputBufferLength	= NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;


		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);

		deviceControlStatus = ZwDeviceIoControlFile(
								openFile->FileHandle,
								NULL,
								NULL,
								NULL,
								&ioStatusBlock,
								ioControlCode,
								inputBuffer,
								inputBufferLength,
								outputBuffer,
								outputBufferLength
								);
		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
						("DispatchWinXpRequest: IRP_MJ_DEVICE_CONTROL: CtrlCode:%l08x openFileId = %X, deviceControlStatus = %X, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
										ioControlCode, openFileId, deviceControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
		if(NT_SUCCESS(deviceControlStatus))
		{
			ASSERT(deviceControlStatus == STATUS_SUCCESS);		
			ASSERT(deviceControlStatus == ioStatusBlock.Status);
		}

		if(deviceControlStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(ioStatusBlock.Information == inputBufferLength || ioStatusBlock.Information == outputBufferLength);

		if(!(deviceControlStatus == STATUS_SUCCESS || deviceControlStatus == STATUS_BUFFER_OVERFLOW))
		{
			ioStatusBlock.Information = 0;
			ASSERT(ioStatusBlock.Information == 0);
		}

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = deviceControlStatus;
		NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;

		if(ioStatusBlock.Information)
			ASSERT(outputBufferLength);

		if(outputBufferLength)
		{
			*replyDataSize = ioStatusBlock.Information;
			//*replyDataSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength <= ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength : ioStatusBlock.Information;
		} 
		else
			*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		break;
	}
	
	case IRP_MJ_LOCK_CONTROL: // 0x11
	{
		NTSTATUS			lockControlStatus;

		ULONG				openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE			openFile;
		
		IO_STATUS_BLOCK		ioStatusBlock;
		LARGE_INTEGER		byteOffset;
		LARGE_INTEGER		length;
		ULONG				key;
		BOOLEAN				failImmediately;
		BOOLEAN				exclusiveLock;
		

		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);
		//ASSERT(NdfsWinxpRequestHeader->LockControl.Length <= MAX_PRIMARY_SESSION_SEND_BUFFER);

		byteOffset.QuadPart	= NdfsWinxpRequestHeader->LockControl.ByteOffset;
		length.QuadPart		= NdfsWinxpRequestHeader->LockControl.Length;
		key					= NdfsWinxpRequestHeader->LockControl.Key;

        failImmediately		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_FAIL_IMMEDIATELY);
		exclusiveLock		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_EXCLUSIVE_LOCK);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ( "LFS: IRP_MJ_LOCK_CONTROL: Length:%I64d Key:%08lx Offset:%I64d Ime:%d Ex:%d\n",
						length.QuadPart,
						key,
						byteOffset.QuadPart,
						key,
						failImmediately,
						exclusiveLock
					)) ;

		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);
		
		if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_LOCK)
		{
#if 1
			lockControlStatus = NtLockFile(
										openFile->FileHandle,
										NULL,
										NULL,
										NULL,
										&ioStatusBlock,
										&byteOffset,
										&length,
										key,
										failImmediately,
										exclusiveLock
										);
#else
			ioStatusBlock.Information = (ULONG_PTR)length.QuadPart;
			lockControlStatus = STATUS_SUCCESS ;
#endif
		}
		else if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_UNLOCK_SINGLE)
		{
#if 1
			lockControlStatus = NtUnlockFile(
										openFile->FileHandle,
										&ioStatusBlock,
										&byteOffset,
										&length,
										key
										);		
#else
			ioStatusBlock.Information = (ULONG_PTR)length.QuadPart;
			lockControlStatus = STATUS_SUCCESS ;
#endif
		}
		else if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_UNLOCK_ALL)
		{
#ifndef		__LFS_HCT_TEST_MODE__
			ASSERT(LFS_REQUIRED);
#endif
			lockControlStatus = STATUS_SUCCESS;
			//lockControlStatus = STATUS_NOT_IMPLEMENTED;
		}
		else
		{
			ASSERT(LFS_BUG);
			lockControlStatus = STATUS_NOT_IMPLEMENTED;
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: NtLockFile: openFileId = %X, LockControlStatus = %X, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						openFileId, lockControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));

		
		NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = lockControlStatus;

		if(NT_SUCCESS(lockControlStatus))
		{
			ASSERT(lockControlStatus == ioStatusBlock.Status);
			NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;
		}
		else
			NdfsWinxpReplytHeader->Information = 0;

		*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		break;
	}
	case IRP_MJ_CLEANUP: // 0x12
	{
		ULONG				openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE			openFile;

		//PIRP				irp;
		//PIO_STACK_LOCATION	irpSp;
		//PDEVICE_OBJECT	deviceObject;
		//PFAST_IO_DISPATCH	fastIoDispatch;
		//NTSTATUS			status;
		//KEVENT			event;
		//KIRQL				irql;

		
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);

#if 0
        if (!(openFile->FileObject->Flags & FO_DIRECT_DEVICE_OPEN)) 
		{
            deviceObject = IoGetRelatedDeviceObject(openFile->FileObject);
        } 
		else 
		{
            deviceObject = IoGetAttachedDevice(openFile->FileObject->DeviceObject);
        }

		if(deviceObject)
		{
			irp = IopAllocateIrpMustSucceed(deviceObject->StackSize);
			irp->Tail.Overlay.OriginalFileObject = openFile->FileObject;
			irp->Tail.Overlay.Thread = PsGetCurrentThread();
			irp->RequestorMode = KernelMode;

			irp->UserEvent = &event;
			irp->UserIosb = &irp->IoStatus;
			irp->Overlay.AsynchronousParameters.UserApcRoutine = (PIO_APC_ROUTINE) NULL;
			irp->Flags = NdfsWinxpRequestHeader->IrpFlags | IRP_SYNCHRONOUS_API; // | IRP_CLOSE_OPERATION;

			irpSp = IoGetNextIrpStackLocation(irp);
			irpSp->MajorFunction = IRP_MJ_CLEANUP;
			irpSp->FileObject = openFile->FileObject;

			IopQueueThreadIrp(irp);
			//IopUpdateOtherOperationCount();

			status = IoCallDriver(deviceObject, irp);

			if (status == STATUS_PENDING) 
			{
				(VOID) KeWaitForSingleObject(
							&event,
							UserRequest,
							KernelMode,
							FALSE,
							(PLARGE_INTEGER) NULL 
							);
			}

			//KeRaiseIrql(APC_LEVEL, &irql);
			//IopDequeueThreadIrp(irp);
			//KeLowerIrql(irql);

			IoFreeIrp(irp);
		}
#endif

		ASSERT(openFile && openFile->FileObject);

		if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1 
			&& BooleanFlagOn(NdfsWinxpRequestHeader->IrpFlags, IRP_CLOSE_OPERATION))
		{
			openFile->CleanUp = TRUE;
#if 0
			NTSTATUS			closeStatus;

			ObDereferenceObject(openFile->FileObject);
			openFile->FileObject = NULL;
			closeStatus = ZwClose(openFile->FileHandle);
			ASSERT(closeStatus == STATUS_SUCCESS);
			closeStatus = ZwClose(openFile->EventHandle);
			ASSERT(closeStatus == STATUS_SUCCESS);
			openFile->AlreadyClosed = TRUE;
#endif
		}
		
		NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = STATUS_SUCCESS;
		NdfsWinxpReplytHeader->Information = 0;

		*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		break;
	}
    case IRP_MJ_QUERY_SECURITY:
	{
		NTSTATUS				querySecurityStatus;
		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;
		PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					returnedLength = 0;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);
		ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length <= PrimarySession->PrimaryMaxDataSize);

		length					= NdfsWinxpRequestHeader->QuerySecurity.Length;
		securityInformation		= NdfsWinxpRequestHeader->QuerySecurity.SecurityInformation;

		securityDescriptor		= (PSECURITY_DESCRIPTOR)(NdfsWinxpReplytHeader+1);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, 
			("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_SECURITY: OutputBufferLength:%d\n", length));

		querySecurityStatus = ZwQuerySecurityObject(
										openFile->FileHandle,
										securityInformation,
										securityDescriptor,
										length,
										&returnedLength
									);

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction	= NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction	= NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(querySecurityStatus))
			ASSERT(querySecurityStatus == STATUS_SUCCESS);		

		if( querySecurityStatus == STATUS_SUCCESS ||
			querySecurityStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information = returnedLength;
			*replyDataSize = returnedLength;

		} else if(querySecurityStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			querySecurityStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information = returnedLength; //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*replyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information = 0;
			*replyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status	  = querySecurityStatus;

		ntStatus = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_SET_SECURITY:
	{
		NTSTATUS				setSecurityStatus;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
	    PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		securityInformation = NdfsWinxpRequestHeader->SetSecurity.SecurityInformation;
		length				= NdfsWinxpRequestHeader->SetSecurity.Length;
		securityDescriptor	= (PSECURITY_DESCRIPTOR)ndfsRequestData ;

		setSecurityStatus = ZwSetSecurityObject(
										openFile->FileHandle,
										securityInformation,
										securityDescriptor
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: ZwSetSecurityObject: fileHandle = %X, securityInformation = %X, setSecurityStatus = %X\n",
						openFile->FileHandle, securityInformation, setSecurityStatus));

		if(NT_SUCCESS(setSecurityStatus))
			ASSERT(setSecurityStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = setSecurityStatus;
		NdfsWinxpReplytHeader->Information = length;

		*replyDataSize = 0;

		ntStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_EA:
	{
		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
		NTSTATUS				queryEaStatus;
		IO_STATUS_BLOCK			ioStatusBlock ;
		PVOID					buffer;
		ULONG					length;
		BOOLEAN					returnSingleEntry;
		PVOID					eaList;
		ULONG					eaListLength;
		ULONG					eaIndex ;
		BOOLEAN					restartScan ;

		BOOLEAN					indexSpecified;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		buffer				= (_U8 *)(NdfsWinxpReplytHeader+1);
		length				= NdfsWinxpRequestHeader->QueryEa.Length;
		returnSingleEntry	= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
		eaList				= ndfsRequestData;
		eaListLength		= NdfsWinxpRequestHeader->QueryEa.EaListLength;
		if(eaListLength == 0)
			eaList = NULL;
		eaIndex				= NdfsWinxpRequestHeader->QueryEa.EaIndex;
		restartScan			= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, 
			("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_EA: length:%d\n", length));

		indexSpecified = BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_INDEX_SPECIFIED);

		queryEaStatus = NtQueryEaFile(
								openFile->FileHandle,
								&ioStatusBlock,
								buffer,
								length,
								returnSingleEntry,
								eaList,
								eaListLength,
								indexSpecified ? &eaIndex : NULL,
								restartScan
								);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: ZwQueryEaFile: fileHandle = %p, ioStatusBlock->Information = %d, queryEaStatus = %x\n",
						openFile->FileHandle, ioStatusBlock.Information, queryEaStatus));

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(queryEaStatus))
			ASSERT(queryEaStatus == STATUS_SUCCESS);		

		if( queryEaStatus == STATUS_SUCCESS ||
			queryEaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;
		
			if(ioStatusBlock.Information != 0)
			{
				PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)buffer;
		
				*replyDataSize = 0;
			
				while(fileFullEa->NextEntryOffset)
				{
					*replyDataSize += fileFullEa->NextEntryOffset;
					fileFullEa = (PFILE_FULL_EA_INFORMATION)((_U8 *)fileFullEa + fileFullEa->NextEntryOffset);
				}

				*replyDataSize += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
								("DispatchWinXpRequest, IRP_MJ_QUERY_EA: Ea is set QueryEa.Length = %d, inputBufferLength = %d\n",
										NdfsWinxpRequestHeader->QueryEa.Length, *replyDataSize));
				*replyDataSize = ((*replyDataSize < length) ? *replyDataSize : length);
			}
		} 
		else if(queryEaStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryEaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information = ioStatusBlock.Information; //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*replyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information = 0;
			*replyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status	  = queryEaStatus;

		ntStatus = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_SET_EA:
	{
		NTSTATUS				setEaStatus;
		IO_STATUS_BLOCK			ioStatusBlock;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		ULONG					length;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		length = NdfsWinxpRequestHeader->SetEa.Length;

		setEaStatus = NtSetEaFile(
							openFile->FileHandle,
							&ioStatusBlock,
							ndfsRequestData,
							length
							);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: ZwSetEaFile: fileHandle = %p, ioStatusBlock->Information = %d, setEaStatus = %x\n",
						openFile->FileHandle, ioStatusBlock.Information, setEaStatus));

		if(NT_SUCCESS(setEaStatus))
			ASSERT(setEaStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = setEaStatus;
		NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;

		*replyDataSize = 0;

		ntStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_QUOTA:
	{
		NTSTATUS				queryQuotaStatus;
		IO_STATUS_BLOCK			ioStatusBlock ;
		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
		PVOID					outputBuffer ;
		ULONG					outputBufferLength;
		PVOID					inputBuffer ;
		ULONG					inputBufferLength ;
		PVOID					startSid ;
		BOOLEAN					restartScan ;
		BOOLEAN					returnSingleEntry ;

		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		outputBufferLength		= NdfsWinxpRequestHeader->QueryQuota.Length;
		if(outputBufferLength)
			outputBuffer			= (_U8 *)(NdfsWinxpReplytHeader+1);
		else
			outputBuffer			= NULL ;

		inputBufferLength		= NdfsWinxpRequestHeader->QueryQuota.InputLength ;
		if(inputBufferLength)
			inputBuffer				= ndfsRequestData;
		else
			inputBuffer				= NULL;

		if(NdfsWinxpRequestHeader->QueryQuota.StartSidOffset)
			startSid			= (PCHAR)ndfsRequestData + NdfsWinxpRequestHeader->QueryQuota.StartSidOffset;
		else
			startSid			= NULL;

		restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;
		returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, 
			("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_QUOTA: OutputBufferLength:%d\n", outputBufferLength));

		queryQuotaStatus = NtQueryQuotaInformationFile(
										openFile->FileHandle,
										&ioStatusBlock,
										outputBuffer,
										outputBufferLength,
										returnSingleEntry,
										inputBuffer,
										inputBufferLength,
										startSid,
										restartScan
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: NtQueryQuotaInformationFile: fileHandle = %X, ioStatusBlock->Information = %X, queryQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, queryQuotaStatus));

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(queryQuotaStatus))
			ASSERT(queryQuotaStatus == STATUS_SUCCESS);		

		if( queryQuotaStatus == STATUS_SUCCESS ||
			queryQuotaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;
			*replyDataSize = ioStatusBlock.Information;

		} else if(queryQuotaStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryQuotaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information = ioStatusBlock.Information; //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*replyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information = 0;
			*replyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status	  = queryQuotaStatus;

		ntStatus = STATUS_SUCCESS;

		break ;
	}
	case IRP_MJ_SET_QUOTA:
	{
		NTSTATUS				setQuotaStatus;
		IO_STATUS_BLOCK			ioStatusBlock;

		ULONG					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		ULONG					length;


		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

		length				= NdfsWinxpRequestHeader->SetQuota.Length;

		setQuotaStatus = NtSetQuotaInformationFile(
									openFile->FileHandle,
									&ioStatusBlock,
									ndfsRequestData,
									length
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: NtSetQuotaInformationFile: fileHandle = %X, ioStatusBlock.Information = %X, setQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, setQuotaStatus));

		if(NT_SUCCESS(setQuotaStatus))
			ASSERT(setQuotaStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	  = setQuotaStatus;
		NdfsWinxpReplytHeader->Information = ioStatusBlock.Information;

		*replyDataSize = 0;

		ntStatus = STATUS_SUCCESS;

		break;
	}
	default:

		ASSERT(LFS_BUG);
		ntStatus = STATUS_UNSUCCESSFUL;

		break;
	}

	if(!(
			NdfsWinxpReplytHeader->Status == STATUS_SUCCESS
		||  NdfsWinxpReplytHeader->Status == STATUS_BUFFER_OVERFLOW
//		||  NdfsWinxpReplytHeader->Status == STATUS_END_OF_FILE
		))
		ASSERT(*replyDataSize == 0);

	ASSERT(NdfsWinxpReplytHeader->Status != STATUS_INVALID_HANDLE);
	ASSERT(NdfsWinxpReplytHeader->Status != STATUS_PENDING);

	return ntStatus;	
}


NTSTATUS
GetVolumeInformation(
	IN PPRIMARY_SESSION	PrimarySession,
	IN PUNICODE_STRING	VolumeName
	)
{
	HANDLE					volumeHandle = NULL;
    ACCESS_MASK				desiredAccess;
	ULONG					attributes;
	OBJECT_ATTRIBUTES		objectAttributes;
	IO_STATUS_BLOCK			ioStatusBlock;
	LARGE_INTEGER			allocationSize;
	ULONG					fileAttributes;
    ULONG					shareAccess;
    ULONG					createDisposition;
	ULONG					createOptions;
    PVOID					eaBuffer;
	ULONG					eaLength;

	NTSTATUS				createStatus;
	NTSTATUS				fsControlStatus;

	NTFS_VOLUME_DATA_BUFFER	ntfsVolumeDataBuffer;
	
#if DBG
#else
	UNREFERENCED_PARAMETER(PrimarySession);
#endif

	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
					| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT(desiredAccess == 0x0012019F);

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes(
			&objectAttributes,
			VolumeName,
			attributes,
			NULL,
			NULL
			);
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
	eaBuffer		  = NULL;
	eaLength		  = 0;
	

	RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("GetVolumeInformation: PrimarySession = %p\n", PrimarySession));

	createStatus = ZwCreateFile(
						&volumeHandle,
						desiredAccess,
						&objectAttributes,
						&ioStatusBlock,
						&allocationSize,
						fileAttributes,
						shareAccess,
						createDisposition,
						createOptions,
						eaBuffer,
						eaLength
						);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("GetVolumeInformation: PrimarySession = %p ZwCreateFile volumeHandle =%x, createStatus = %X, ioStatusBlock = %X\n",
						PrimarySession, volumeHandle, createStatus, ioStatusBlock.Information));

	if(!(createStatus == STATUS_SUCCESS))
	{
		return STATUS_UNSUCCESSFUL;
	}else
		ASSERT(ioStatusBlock.Information == FILE_OPENED);

	RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));

	fsControlStatus = ZwFsControlFile(
								volumeHandle,
								NULL,
								NULL,
								NULL,
								&ioStatusBlock,
								FSCTL_GET_NTFS_VOLUME_DATA,
								NULL,
								0,
								&ntfsVolumeDataBuffer,
								sizeof(ntfsVolumeDataBuffer)
								);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("GetFileRecordSegmentHeader: FSCTL_GET_NTFS_VOLUME_DATA: volumeHandle %p, fsControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						volumeHandle, fsControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
	if(NT_SUCCESS(fsControlStatus))
	{
		ASSERT(fsControlStatus == STATUS_SUCCESS);	
		ASSERT(fsControlStatus == ioStatusBlock.Status);
	}

	if(fsControlStatus == STATUS_BUFFER_OVERFLOW)
		ASSERT(ioStatusBlock.Information == sizeof(ntfsVolumeDataBuffer));
		
	if(!(fsControlStatus == STATUS_SUCCESS || fsControlStatus == STATUS_BUFFER_OVERFLOW))
	{
		ioStatusBlock.Information = 0;
		ASSERT(ioStatusBlock.Information == 0);
	}	

	if(!NT_SUCCESS(fsControlStatus))
	{
		PrimarySession->BytesPerFileRecordSegment	= 0;
		PrimarySession->BytesPerSector				= 0;
		PrimarySession->BytesPerCluster				= 0;
		
		ZwClose(volumeHandle);
		return STATUS_SUCCESS;
	}
	
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->VolumeSerialNumber.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.VolumeSerialNumber.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->NumberSectors.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.NumberSectors.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->TotalClusters.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.TotalClusters.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->FreeClusters.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.FreeClusters.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->TotalReserved.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.TotalReserved.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->BytesPerSector = %u\n", 
			ntfsVolumeDataBuffer.BytesPerSector));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->BytesPerCluster = %u\n", 
			ntfsVolumeDataBuffer.BytesPerCluster));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->BytesPerFileRecordSegment = %u\n", 
			ntfsVolumeDataBuffer.BytesPerFileRecordSegment));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->ClustersPerFileRecordSegment = %u\n", 
			ntfsVolumeDataBuffer.ClustersPerFileRecordSegment));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->MftValidDataLength.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.MftValidDataLength.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->MftStartLcn.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.MftStartLcn.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->MftZoneStart.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.MftZoneStart.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("ntfsVolumeDataBuffer->MftZoneEnd.QuadPart = %I64u\n", 
			ntfsVolumeDataBuffer.MftZoneEnd.QuadPart));

	PrimarySession->BytesPerFileRecordSegment	= ntfsVolumeDataBuffer.BytesPerFileRecordSegment;
	PrimarySession->BytesPerSector				= ntfsVolumeDataBuffer.BytesPerSector;
	PrimarySession->BytesPerCluster				= ntfsVolumeDataBuffer.BytesPerCluster;

	ZwClose(volumeHandle);
	return STATUS_SUCCESS;
}


VOID
DispatchWinXpRequestWorker(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    )
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Slot[Mid].RequestMessageBuffer;
	PNDFS_REPLY_HEADER			ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Slot[Mid].ReplyMessageBuffer; 
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = PrimarySession->Slot[Mid].NdfsWinxpRequestHeader;
	
	_U32						replyDataSize;

	
	ASSERT(Mid == ndfsRequestHeader->Mid);
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: entered PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));

	ASSERT(PrimarySession->Slot[Mid].SlotState == SLOT_EXECUTING);


	replyDataSize = CaculateReplyDataLength(PrimarySession, ndfsWinxpRequestHeader);

	if(replyDataSize <= (ULONG)(PrimarySession->SecondaryMaxDataSize || sizeof(PrimarySession->Slot[Mid].ReplyMessageBuffer) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		if(ndfsRequestHeader->MessageSecurity == 1)
		{
			if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->RwDataSecurity == 0)
				PrimarySession->Slot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
			else
				PrimarySession->Slot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Slot[Mid].CryptWinxpMessageBuffer;
		}
		else
			PrimarySession->Slot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	}
	else
	{
		PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePoolLength = ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + replyDataSize);
		PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePool = ExAllocatePoolWithTag(
																	NonPagedPool,
																	PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePoolLength,
																	PRIMARY_SESSION_BUFFERE_TAG
																	);		
		ASSERT(PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePool);
		if(PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePool == NULL) {
		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("failed to allocate ExtendWinxpReplyMessagePool\n"));
			goto fail_replypoolalloc;
		}
		PrimarySession->Slot[Mid].NdfsWinxpReplyHeader 
			= (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePool;
	}
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));

	PrimarySession->Slot[Mid].ReturnStatus 
			= DispatchWinXpRequest(
					PrimarySession, 
					ndfsWinxpRequestHeader,
					PrimarySession->Slot[Mid].NdfsWinxpReplyHeader,
					ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
					&PrimarySession->Slot[Mid].ReplyDataSize
					);

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: Return PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));
fail_replypoolalloc:
	PrimarySession->Slot[Mid].SlotState = SLOT_FINISH;

	KeSetEvent(&PrimarySession->WorkCompletionEvent, IO_NO_INCREMENT, FALSE);

	return;
}


VOID
DispatchWinXpRequestWorker0(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker(PrimarySession, 0);
}


VOID
DispatchWinXpRequestWorker1(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker(PrimarySession, 1);
}


VOID
DispatchWinXpRequestWorker2(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker(PrimarySession, 2);
}


VOID
DispatchWinXpRequestWorker3(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker(PrimarySession, 3);
}


NTSTATUS
DispatchRequest(
	IN PPRIMARY_SESSION	PrimarySession
	)
{
	NTSTATUS				returnStatus;
	IN PNDFS_REQUEST_HEADER	ndfsRequestHeader;


	ASSERT(PrimarySession->NdfsRequestHeader.Mid < PrimarySession->RequestPerSession);

	RtlCopyMemory(
		PrimarySession->Slot[PrimarySession->NdfsRequestHeader.Mid].RequestMessageBuffer,
		&PrimarySession->NdfsRequestHeader,
		sizeof(NDFS_REQUEST_HEADER)
		);

	ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Slot[PrimarySession->NdfsRequestHeader.Mid].RequestMessageBuffer;
   
	ASSERT(PrimarySession->TdiReceiveContext.Result == sizeof(NDFS_REQUEST_HEADER));

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchRequest: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));

	switch(ndfsRequestHeader->Command)
	{
	case NDFS_COMMAND_NEGOTIATE:
	{
		PNDFS_REQUEST_NEGOTIATE	ndfsRequestNegotiate;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_NEGOTIATE	ndfsReplyNegotiate;

		NTSTATUS				tdiStatus;
		
		
		if(PrimarySession->State != SESSION_CLOSE)
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT(ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_NEGOTIATE));
		ndfsRequestNegotiate = (PNDFS_REQUEST_NEGOTIATE)(ndfsRequestHeader+1);
		
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsRequestNegotiate,
						sizeof(NDFS_REQUEST_NEGOTIATE),
						NULL
						);
	
		if(tdiStatus != STATUS_SUCCESS)
		{
			ASSERT(LFS_BUG);
			returnStatus = tdiStatus;

			break;
		}

		PrimarySession->SessionFlags = ndfsRequestNegotiate->Flags;
		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestNegotiate+1);

		RtlCopyMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol));
		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = PrimarySession->SessionFlags;
		ndfsReplyHeader->Uid		= 0;
		ndfsReplyHeader->Tid		= 0;
		ndfsReplyHeader->Mid		= 0;
		ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_NEGOTIATE);

		ndfsReplyNegotiate = (PNDFS_REPLY_NEGOTIATE)(ndfsReplyHeader+1);

		if(
			ndfsRequestNegotiate->NdfsMajorVersion == NDFS_PROTOCOL_MAJOR_1
			&& ndfsRequestNegotiate->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0
			&& ndfsRequestNegotiate->OsMajorType == OS_TYPE_WINDOWS
			&& ndfsRequestNegotiate->OsMinorType == OS_TYPE_WINXP
			)
		{
			PrimarySession->NdfsMajorVersion = ndfsRequestNegotiate->NdfsMajorVersion;
			PrimarySession->NdfsMinorVersion = ndfsRequestNegotiate->NdfsMinorVersion;
			if(ndfsRequestNegotiate->MinorVersionPlusOne)
				PrimarySession->NdfsMinorVersion++;

			ndfsReplyNegotiate->Status = NDFS_NEGOTIATE_SUCCESS;
			ndfsReplyNegotiate->NdfsMajorVersion = PrimarySession->NdfsMajorVersion;
			ndfsReplyNegotiate->NdfsMinorVersion = PrimarySession->NdfsMinorVersion;
			ndfsReplyNegotiate->OsMajorType = OS_TYPE_WINDOWS;	
			ndfsReplyNegotiate->OsMinorType = OS_TYPE_WINXP;
			ndfsReplyNegotiate->SessionKey = PrimarySession->SessionKey;
			ndfsReplyNegotiate->MaxBufferSize = PrimarySession->PrimaryMaxDataSize;
			RtlCopyMemory(
				ndfsReplyNegotiate->ChallengeBuffer,
				&PrimarySession,
				sizeof(PPRIMARY_SESSION)
				);
			ndfsReplyNegotiate->ChallengeLength = sizeof(PPRIMARY_SESSION);

			PrimarySession->State = SESSION_NEGOTIATE;
		}
		else
		{
			ndfsReplyNegotiate->Status = NDFS_NEGOTIATE_UNSUCCESSFUL;
		}

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsReplyHeader,
						ndfsReplyHeader->MessageSize,
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			returnStatus = tdiStatus;
			break;
		}

		returnStatus = STATUS_SUCCESS;
		break;
	}
	case NDFS_COMMAND_SETUP:
	{
		PNDFS_REQUEST_SETUP	ndfsRequestSetup;
		PNDFS_REPLY_HEADER	ndfsReplyHeader;
		PNDFS_REPLY_SETUP	ndfsReplySetup;

		NTSTATUS			tdiStatus;
		_U8					ndfsReplySetupStatus;

		unsigned char		idData[1];
		struct MD5Context	context;
		_U8					responseBuffer[16]; 
		
		
		if(PrimarySession->State != SESSION_NEGOTIATE)
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT(ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_SETUP));
		ndfsRequestSetup = (PNDFS_REQUEST_SETUP)(ndfsRequestHeader+1);
		
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsRequestSetup,
						sizeof(NDFS_REQUEST_SETUP),
						NULL
						);
	
		if(tdiStatus != STATUS_SUCCESS)
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}
	
		do 
		{		
			ASSERT(PrimarySession->NetdiskPartition == NULL);

			if(ndfsRequestSetup->SessionKey != PrimarySession->SessionKey)
			{
				ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;				
				break;
			}

			RtlCopyMemory(
				PrimarySession->NetDiskAddress.Node,
				ndfsRequestSetup->NetDiskNode,
				6
				);
			
			PrimarySession->NetDiskAddress.Port = HTONS(ndfsRequestSetup->NetDiskPort);//HTONS(ndfsRequestSetup->NetDiskPort);
			PrimarySession->UnitDiskNo = ndfsRequestSetup->UnitDiskNo;

			if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
			{
				PrimarySession->StartingOffset.QuadPart = ndfsRequestSetup->StartingOffset;

				PrimarySession->NetdiskPartition 
					= MountManager_GetPrimaryPartition(
							GlobalLfs.NetdiskManager,
							&PrimarySession->NetDiskAddress,
							PrimarySession->UnitDiskNo,
							&PrimarySession->StartingOffset
							);

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
						("Primary_LookUpVolume: PrimarySession->NetdiskPartition = %p netDiskPartitionInfo.StartingOffset = %I64x\n", 
							PrimarySession->NetdiskPartition, PrimarySession->StartingOffset));

				if(PrimarySession->NetdiskPartition == NULL)
				{
					ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;				
					break;
				}

				PrimarySession->Tid  = (_U16)PrimarySession->NetdiskPartition;
			}
			else if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1)
			{
				PrimarySession->NetdiskPartition 
					= MountManager_GetPrimaryPartition(
							GlobalLfs.NetdiskManager,
							&PrimarySession->NetDiskAddress,
							PrimarySession->UnitDiskNo,
							NULL
							);

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
						("Primary_LookUpVolume: PrimarySession->NetdiskPartition = %p netDiskPartitionInfo.StartingOffset = %I64x\n", 
							PrimarySession->NetdiskPartition, PrimarySession->StartingOffset));

				if(PrimarySession->NetdiskPartition == NULL)
				{
					ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;				
					break;
				}
			}
			else
				ASSERT(LFS_BUG);

			
			MD5Init(&context);

			/* id byte */
			idData[0] = (unsigned char)PrimarySession->SessionKey;
			MD5Update(&context, idData, 1);

			MD5Update(&context, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetDiskInformation.Password, 8);
			MD5Update(&context, &(UCHAR)PrimarySession, sizeof(PPRIMARY_SESSION));
			MD5Final(responseBuffer, &context);

			if(!RtlEqualMemory(
				ndfsRequestSetup->ResponseBuffer,
				responseBuffer,
				16))
			{
				ASSERT(LFS_BUG);
				ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;				
				break;
			}

			ndfsReplySetupStatus = NDFS_SETUP_SUCCESS;
		
		} while(0);

		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestSetup+1);
			
		RtlCopyMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol));
		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = 0;
		ndfsReplyHeader->Uid		= 0;
		ndfsReplyHeader->Tid		= 0;
		ndfsReplyHeader->Mid		= 0;
		ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP);

		if(ndfsReplySetupStatus == NDFS_SETUP_SUCCESS)
		{
			if(ndfsRequestSetup->MaxBufferSize)
			{
				PrimarySession->SecondaryMaxDataSize
					= (ndfsRequestSetup->MaxBufferSize <= DEFAULT_MAX_DATA_SIZE)
						? ndfsRequestSetup->MaxBufferSize : DEFAULT_MAX_DATA_SIZE;
			}

			ndfsReplyHeader->Uid = PrimarySession->Uid;
			ndfsReplyHeader->Tid = PrimarySession->Tid;
		}
		else
		{
			if(PrimarySession->NetdiskPartition)
			{		
				MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, PrimarySession->NetdiskPartition);
				PrimarySession->NetdiskPartition = NULL;
			}
		}

		ndfsReplySetup = (PNDFS_REPLY_SETUP)(ndfsReplyHeader+1);
		ndfsReplySetup->Status = ndfsReplySetupStatus;


		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsReplyHeader,
						ndfsReplyHeader->MessageSize,
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			returnStatus = tdiStatus;
			break;
		}

		if(ndfsReplySetupStatus == NDFS_SETUP_SUCCESS)
			PrimarySession->State = SESSION_SETUP;
		
		returnStatus = STATUS_SUCCESS;

		break;
	}
	case NDFS_COMMAND_TREE_CONNECT:
	{
		PNDFS_REQUEST_TREE_CONNECT	ndfsRequestTreeConnect;
		PNDFS_REPLY_HEADER			ndfsReplyHeader;
		PNDFS_REPLY_TREE_CONNECT	ndfsReplyTreeConnect;
	
		NTSTATUS					tdiStatus;
		_U8							ndfsReplyTreeConnectStatus;
		
		
		if(!(
			PrimarySession->State == SESSION_SETUP
			&& ndfsRequestHeader->Uid == PrimarySession->Uid
			))
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT(ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_TREE_CONNECT));
		ndfsRequestTreeConnect = (PNDFS_REQUEST_TREE_CONNECT)(ndfsRequestHeader+1);

		tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsRequestTreeConnect,
							sizeof(NDFS_REQUEST_TREE_CONNECT),
							NULL
							);
	
		if(tdiStatus != STATUS_SUCCESS)
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}

		do 
		{		
			NTSTATUS			getVolumeInformationStatus;
			PNETDISK_PARTITION	netdiskPartition;


			ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestTreeConnect+1);

			RtlCopyMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol));
			ndfsReplyHeader->Status		= NDFS_SUCCESS;
			ndfsReplyHeader->Flags	    = 0;
			ndfsReplyHeader->Uid		= PrimarySession->Uid;
			ndfsReplyHeader->Tid		= 0;
			ndfsReplyHeader->Mid		= 0;
			ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_TREE_CONNECT);

			PrimarySession->StartingOffset.QuadPart = ndfsRequestTreeConnect->StartingOffset;

			netdiskPartition 
				= MountManager_GetPrimaryPartition(
							GlobalLfs.NetdiskManager,
							&PrimarySession->NetDiskAddress,
							PrimarySession->UnitDiskNo,
							&PrimarySession->StartingOffset
							);

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
				("NDFS_COMMAND_TREE_CONNECT: netdiskPartition = %p netDiskPartitionInfo.StartingOffset = %I64x\n", 
					netdiskPartition, PrimarySession->StartingOffset));

			if(netdiskPartition == NULL)
			{
				ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_NO_PARTITION;				
				break;
			}

			if(BooleanFlagOn(netdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED))
			{
				ndfsReplyTreeConnectStatus = NDFS_TREE_CORRUPTED;
				MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, netdiskPartition);
				break;
			}

			if(netdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NTFS
				&& IS_WINDOWSXP_OR_LATER())
			{
				getVolumeInformationStatus 
					= GetVolumeInformation(
						PrimarySession, 
						&netdiskPartition->NetdiskPartitionInformation.VolumeName
						);
		
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
					("NDFS_COMMAND_TREE_CONNECT: getVolumeInformationStatus = %x\n", 
						getVolumeInformationStatus));

				if(getVolumeInformationStatus != STATUS_SUCCESS)
				{
					MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, netdiskPartition);
					ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_UNSUCCESSFUL;				
					break;
				}
			}
		
			ASSERT(PrimarySession->NetdiskPartition);
			MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, PrimarySession->NetdiskPartition);
			PrimarySession->NetdiskPartition = netdiskPartition;

			PrimarySession->Tid  = (_U16)PrimarySession->NetdiskPartition;
			ndfsReplyHeader->Tid = PrimarySession->Tid;

			ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_SUCCESS;				
		
		} while(0);
		
		ndfsReplyTreeConnect = (PNDFS_REPLY_TREE_CONNECT)(ndfsReplyHeader+1);
		ndfsReplyTreeConnect->Status = ndfsReplyTreeConnectStatus;
		
		ndfsReplyTreeConnect->RequestsPerSession = REQUEST_PER_SESSION;
		
		ndfsReplyTreeConnect->BytesPerFileRecordSegment	= PrimarySession->BytesPerFileRecordSegment;
		ndfsReplyTreeConnect->BytesPerSector			= PrimarySession->BytesPerSector;
		ndfsReplyTreeConnect->BytesPerCluster			= PrimarySession->BytesPerCluster;

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsReplyHeader,
						ndfsReplyHeader->MessageSize,
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			returnStatus = tdiStatus;
			break;
		}

		if(ndfsReplyTreeConnectStatus == NDFS_SETUP_SUCCESS)
			PrimarySession->State = SESSION_TREE_CONNECT;

		returnStatus = STATUS_SUCCESS;

		break;
	}		
	case NDFS_COMMAND_LOGOFF:
	{
		PNDFS_REQUEST_LOGOFF	ndfsRequestLogoff;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_LOGOFF		ndfsReplyLogoff;
		
		NTSTATUS				tdiStatus;
		

		if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
		{
			if(PrimarySession->State != SESSION_SETUP)
			{
				ASSERT(LFS_BUG);
				returnStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}
		else if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1)
		{
			if(PrimarySession->State != SESSION_TREE_CONNECT)
			{
				ASSERT(LFS_BUG);
				returnStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}

		if(!(
			ndfsRequestHeader->Uid == PrimarySession->Uid
			&& ndfsRequestHeader->Tid == PrimarySession->Tid
			))
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT(ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_LOGOFF));

		ndfsRequestLogoff = (PNDFS_REQUEST_LOGOFF)(ndfsRequestHeader+1);
		
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsRequestLogoff,
						sizeof(NDFS_REQUEST_LOGOFF),
						NULL
						);
	
		if(tdiStatus != STATUS_SUCCESS)
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}

		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestLogoff+1);

		RtlCopyMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol));
		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = 0;
		ndfsReplyHeader->Uid		= PrimarySession->Uid;
		ndfsReplyHeader->Tid		= 0;
		ndfsReplyHeader->Mid		= 0;
		ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF);

		ndfsReplyLogoff = (PNDFS_REPLY_LOGOFF)(ndfsReplyHeader+1);

		if(ndfsRequestLogoff->SessionKey != PrimarySession->SessionKey)
		{
			ndfsReplyLogoff->Status = NDFS_LOGOFF_UNSUCCESSFUL;
		}
		else
		{
			ndfsReplyLogoff->Status = NDFS_LOGOFF_SUCCESS;
		}

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsReplyHeader,
						ndfsReplyHeader->MessageSize,
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			returnStatus = tdiStatus;
			break;
		}

		PrimarySession->State = SESSION_CLOSED;
		LpxTdiDisconnect(PrimarySession->ConnectionFileObject, 0);
		PrimarySession->ThreadFlags |= PRIMARY_SESSION_THREAD_DISCONNECTED;

		returnStatus = STATUS_SUCCESS;

		break;
	}
	case NDFS_COMMAND_EXECUTE:
	{
		_U16	mid;


		if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
		{
			if(PrimarySession->State != SESSION_SETUP)
			{
				ASSERT(LFS_BUG);
				returnStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}
		else if(PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1)
		{
			if(PrimarySession->State != SESSION_TREE_CONNECT)
			{
				ASSERT(LFS_BUG);
				returnStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}

		if(!(
			ndfsRequestHeader->Uid == PrimarySession->Uid
			&& ndfsRequestHeader->Tid == PrimarySession->Tid
			))
		{
			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}

		mid = ndfsRequestHeader->Mid;

		PrimarySession->Slot[mid].RequestMessageBufferLength = sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE;
		RtlZeroMemory(
			&PrimarySession->Slot[mid].RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER)], 
			PrimarySession->Slot[mid].RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER)
			);
		PrimarySession->Slot[mid].ReplyMessageBufferLength = sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_MAX_DATA_SIZE;
		RtlZeroMemory(PrimarySession->Slot[mid].ReplyMessageBuffer, PrimarySession->Slot[mid].ReplyMessageBufferLength);

		ASSERT(ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER));
		returnStatus = ReceiveNtfsWinxpMessage(PrimarySession, mid);

		if(returnStatus != STATUS_SUCCESS)
			break;

		if(PrimarySession->Slot[mid].SlotState != SLOT_WAIT)
		{
			break;
		}
	
		PrimarySession->Slot[mid].SlotState = SLOT_EXECUTING;
	
		if(mid == 0)
			ExInitializeWorkItem(
				&PrimarySession->Slot[mid].WorkQueueItem,
				DispatchWinXpRequestWorker0,
				PrimarySession 
				);
		if(mid == 1)
			ExInitializeWorkItem(
				&PrimarySession->Slot[mid].WorkQueueItem,
				DispatchWinXpRequestWorker1,
				PrimarySession 
				);
		if(mid == 2)
			ExInitializeWorkItem(
				&PrimarySession->Slot[mid].WorkQueueItem,
				DispatchWinXpRequestWorker2,
				PrimarySession 
				);
		if(mid == 3)
			ExInitializeWorkItem(
				&PrimarySession->Slot[mid].WorkQueueItem,
				DispatchWinXpRequestWorker3,
				PrimarySession 
				);

		ExQueueWorkItem(&PrimarySession->Slot[mid].WorkQueueItem, DelayedWorkQueue);	
		returnStatus = STATUS_PENDING;

#if 0
		returnStatus = DispatchWinXpRequest(
							PrimarySession, 
							ndfsWinxpRequestHeader,
							ndfsWinxpReplyHeader,
							ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
							&replyDataSize
							);
	
		if(returnStatus == STATUS_SUCCESS)
		{
			returnStatus = SendNtfsWinxpMessage(
								PrimarySession,
								ndfsReplyHeader,
								ndfsWinxpReplyHeader,
								replyDataSize,
								ndfsRequestHeader->Mid
								);
		}

		if(PrimarySession->ExtendWinxpRequestMessagePool)
		{
			ExFreePool(PrimarySession->ExtendWinxpRequestMessagePool);	
			PrimarySession->ExtendWinxpRequestMessagePool = NULL;
			PrimarySession->ExtendWinxpReplyMessagePoolLength = 0;
		}
		
		if(PrimarySession->ExtendWinxpReplyMessagePool)
		{
			ExFreePool(PrimarySession->ExtendWinxpReplyMessagePool);	
			PrimarySession->ExtendWinxpReplyMessagePool = NULL;
			PrimarySession->ExtendWinxpReplyMessagePoolLength = 0;
		}
#endif

		break;
	}

	default:

		ASSERT(LPX_BUG);
		returnStatus = STATUS_UNSUCCESSFUL;
		
		break;
	}

	return returnStatus;
}


NTSTATUS
ReceiveNtfsWinxpMessage(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Slot[Mid].RequestMessageBuffer;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*cryptWinxpRequestMessage;

	NTSTATUS					tdiStatus;
	int							desResult;


	cryptWinxpRequestMessage = PrimarySession->Slot[Mid].CryptWinxpMessageBuffer;	
	//ASSERT(ndfsRequestHeader->Splitted == 0 && ndfsRequestHeader->MessageSecurity == 0);
		
	if(ndfsRequestHeader->Splitted == 0)
		{
		ASSERT(ndfsRequestHeader->MessageSize <= PrimarySession->Slot[Mid].RequestMessageBufferLength);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	
		if(ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsWinxpRequestHeader,
							ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER),
							NULL
							);
	
			PrimarySession->Slot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;
	
			return tdiStatus;
		}

		ASSERT(ndfsRequestHeader->MessageSecurity == 1);
		
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						cryptWinxpRequestMessage,
							sizeof(NDFS_WINXP_REQUEST_HEADER),
							NULL
							);
			if(tdiStatus != STATUS_SUCCESS)
			{
			return tdiStatus;
			}

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetDiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);

		if(ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER))
		{
			if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE && ndfsRequestHeader->RwDataSecurity == 0)
			{
				tdiStatus = RecvMessage(
								PrimarySession->ConnectionFileObject,
								(_U8 *)(ndfsWinxpRequestHeader+1),
								ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
								NULL
								);
				if(tdiStatus != STATUS_SUCCESS)
				{
					return tdiStatus;
				}
			}
			else
			{
				tdiStatus = RecvMessage(
								PrimarySession->ConnectionFileObject,
								cryptWinxpRequestMessage,
								ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
								NULL
								);
				if(tdiStatus != STATUS_SUCCESS)
				{
					return tdiStatus;
				}

				desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)(ndfsWinxpRequestHeader+1), cryptWinxpRequestMessage, ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER));
				ASSERT(desResult == IDOK);
			}
		}

		PrimarySession->Slot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}

	ASSERT(ndfsRequestHeader->Splitted == 1);

//	if(ndfsRequestHeader->MessageSize > (PrimarySession->RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		PrimarySession->Slot[Mid].ExtendWinxpRequestMessagePoolLength = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER);
		PrimarySession->Slot[Mid].ExtendWinxpRequestMessagePool 
			= ExAllocatePoolWithTag(
				NonPagedPool,
				PrimarySession->Slot[Mid].ExtendWinxpRequestMessagePoolLength,
				PRIMARY_SESSION_BUFFERE_TAG
				);
		ASSERT(PrimarySession->Slot[Mid].ExtendWinxpRequestMessagePool);
		if(PrimarySession->Slot[Mid].ExtendWinxpRequestMessagePool == NULL) {
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,	("ReceiveNtfsWinxpMessage: failed to allocate ExtendWinxpRequestMessagePool\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(PrimarySession->Slot[Mid].ExtendWinxpRequestMessagePool);
	}
//	else
//		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);

	if(ndfsRequestHeader->MessageSecurity == 0)
	{
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsWinxpRequestHeader,
						sizeof(NDFS_WINXP_REQUEST_HEADER),
						NULL
						);
		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
	}
	else
	{
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						cryptWinxpRequestMessage,
						sizeof(NDFS_WINXP_REQUEST_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetDiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
	}

	while(1)
	{
		PNDFS_REQUEST_HEADER	splitNdfsRequestHeader = &PrimarySession->Slot[Mid].SplitNdfsRequestHeader;


		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)splitNdfsRequestHeader,
						sizeof(NDFS_REQUEST_HEADER),
						NULL
						);
		if(tdiStatus != STATUS_SUCCESS)
			return tdiStatus;

		if(!(
			PrimarySession->State == SESSION_SETUP
			&& ndfsRequestHeader->Uid == PrimarySession->Uid
			&& ndfsRequestHeader->Tid == PrimarySession->Tid
			))
		{
			ASSERT(LFS_BUG);
			return STATUS_UNSUCCESSFUL;
		}

		if(ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER)),
							NULL
								);
			if(tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
		}
		else
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							cryptWinxpRequestMessage,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER)),
							NULL
							);
			if(tdiStatus != STATUS_SUCCESS)
				return tdiStatus;

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							(_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize, 
							cryptWinxpRequestMessage, 
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER))
								);
			ASSERT(desResult == IDOK);
		}

		if(splitNdfsRequestHeader->Splitted)
			continue;

		PrimarySession->Slot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}
}
		

NTSTATUS
SendNtfsWinxpMessage(
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN _U32						ReplyDataSize,
	IN _U16						Mid
	)
{
	NTSTATUS	tdiStatus;
	_U32		remaninigDataSize;		


	//ASSERT(ReplyDataSize <= PrimarySession->SecondaryMaxDataSize && PrimarySession->MessageSecurity == 0);
	if(ReplyDataSize <= PrimarySession->SecondaryMaxDataSize)
		{
		int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Slot[Mid].CryptWinxpMessageBuffer;

		RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionFlags;
		NdfsReplyHeader->Uid		= PrimarySession->Uid;
		NdfsReplyHeader->Tid		= PrimarySession->Tid;
		NdfsReplyHeader->Mid		= Mid;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));

		ASSERT(NdfsReplyHeader->MessageSize <= PrimarySession->Slot[Mid].ReplyMessageBufferLength);

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)NdfsReplyHeader,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
		
		if(PrimarySession->MessageSecurity == 0)
		{
			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)NdfsWinxpReplyHeader,
							NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL
							);
			return tdiStatus;
		}
			
		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchRequest: PrimarySession->RwDataSecurity = %d\n", PrimarySession->RwDataSecurity));

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->RwDataSecurity == 0)
			{
			RtlCopyMemory(cryptWinxpRequestMessage, NdfsWinxpReplyHeader, sizeof(NDFS_WINXP_REPLY_HEADER));
			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetDiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)NdfsWinxpReplyHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)NdfsWinxpReplyHeader,
							NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL
							);
			}
			else
			{
			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetDiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, cryptWinxpRequestMessage, (_U8 *)NdfsWinxpReplyHeader, NdfsReplyHeader->MessageSize-sizeof(NDFS_REPLY_HEADER));
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptWinxpRequestMessage,
							NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL
							);
		}

		return tdiStatus;
		}

	ASSERT((_U8 *)NdfsWinxpReplyHeader == PrimarySession->Slot[Mid].ExtendWinxpReplyMessagePool);
	ASSERT(ReplyDataSize > PrimarySession->SecondaryMaxDataSize);

	RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
	NdfsReplyHeader->Status		= NDFS_SUCCESS;
	NdfsReplyHeader->Flags	    = PrimarySession->SessionFlags;
	NdfsReplyHeader->Splitted	= 1;
	NdfsReplyHeader->Uid		= PrimarySession->Uid;
	NdfsReplyHeader->Tid		= PrimarySession->Tid;
	NdfsReplyHeader->Mid		= 0;
	NdfsReplyHeader->MessageSize 
			= sizeof(NDFS_REPLY_HEADER) 
			+ (PrimarySession->MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));
		
		tdiStatus = SendMessage(
					PrimarySession->ConnectionFileObject,
					(_U8 *)NdfsReplyHeader,
					sizeof(NDFS_REPLY_HEADER),
					NULL
					);

		if(tdiStatus != STATUS_SUCCESS)
		{
		return tdiStatus;
	} 

	if(PrimarySession->MessageSecurity)
	{
		int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Slot[Mid].CryptWinxpMessageBuffer;

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetDiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, cryptWinxpRequestMessage, (_U8 *)NdfsWinxpReplyHeader, sizeof(NDFS_WINXP_REPLY_HEADER));
		ASSERT(desResult == IDOK);
	
		tdiStatus = SendMessage(
					PrimarySession->ConnectionFileObject,
					cryptWinxpRequestMessage,
					sizeof(NDFS_WINXP_REPLY_HEADER),
					NULL
					);
	}
	else
	{
		tdiStatus = SendMessage(
					PrimarySession->ConnectionFileObject,
					(_U8 *)NdfsWinxpReplyHeader,
					sizeof(NDFS_WINXP_REPLY_HEADER),
					NULL
					);
		} 

	if(tdiStatus != STATUS_SUCCESS)
		{
		return tdiStatus;
		}

	remaninigDataSize = ReplyDataSize;

	while(1)
	{
		RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionFlags;
		NdfsReplyHeader->Uid		= PrimarySession->Uid;
		NdfsReplyHeader->Tid		= PrimarySession->Tid;
		NdfsReplyHeader->Mid		= 0;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->MessageSecurity ? ADD_ALIGN8(remaninigDataSize) : remaninigDataSize);

		if(remaninigDataSize > PrimarySession->SecondaryMaxDataSize)
			NdfsReplyHeader->Splitted = 1;

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)NdfsReplyHeader,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
	}

		if(PrimarySession->MessageSecurity)
		{
			int desResult;
			_U8 *cryptNdfsWinxpReplyHeader = PrimarySession->Slot[Mid].CryptWinxpMessageBuffer;

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							cryptNdfsWinxpReplyHeader, 
							(_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ? PrimarySession->SecondaryMaxDataSize : (NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER))
							);
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptNdfsWinxpReplyHeader,
							NdfsReplyHeader->Splitted ? PrimarySession->SecondaryMaxDataSize : (NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);
		}
		else
		{	
			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ? PrimarySession->SecondaryMaxDataSize : (NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);
		}
		
		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
	}

		if(NdfsReplyHeader->Splitted)
			remaninigDataSize -= PrimarySession->SecondaryMaxDataSize;
		else
			return STATUS_SUCCESS;

		ASSERT((_S32)remaninigDataSize > 0);
	}
}


FORCEINLINE
VOID
CloseOpenFiles(
	IN PPRIMARY_SESSION	PrimarySession
	)
{
	PLIST_ENTRY	openFileEntry;

	while(openFileEntry = 
			ExInterlockedRemoveHeadList(
					&PrimarySession->OpenedFileQueue,
					&PrimarySession->OpenedFileQSpinLock
					)
		) 
	{
		POPEN_FILE openFile;
		NTSTATUS   closeStatus;
			
		openFile = CONTAINING_RECORD(
						openFileEntry,
						OPEN_FILE,
						ListEntry
						);
	
		if(openFile->AlreadyClosed == FALSE)
		{
			closeStatus = ZwClose(openFile->FileHandle);
			ObDereferenceObject(openFile->FileObject);
			ZwClose(openFile->EventHandle);
			openFile->AlreadyClosed = TRUE;
		}

		ASSERT(closeStatus == STATUS_SUCCESS);
		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
						("PrimarySessionThreadProc: ZwClose: flieHandle = %x, closeStatus = %x\n",
										openFile->FileHandle, closeStatus));

		InitializeListHead(&openFile->ListEntry) ;
		PrimarySession_FreeOpenFile(
						PrimarySession,
						openFile
						);
	}
	
	return;
}


VOID
PrimarySessionThreadProc(
	IN PPRIMARY_SESSION PrimarySession
	)
{
	BOOLEAN		primarySessionExit = FALSE;
	NTSTATUS	tdiStatus;
	_U16		mid;

#if DBG
	InterlockedIncrement(&LfsObjectCounts.PrimarySessionThreadCount);
#endif

	ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL) ;

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("PrimarySessionThreadProc: Start PrimarySession = %p\n", PrimarySession));
	
	PrimarySession_Reference (PrimarySession) ;
	
	PrimarySession->ThreadFlags = PRIMARY_SESSION_THREAD_INITIALIZING;
	
	InitializeListHead(&PrimarySession->OpenedFileQueue);
	KeInitializeSpinLock(&PrimarySession->OpenedFileQSpinLock);

	KeInitializeEvent(&PrimarySession->WorkCompletionEvent, NotificationEvent, FALSE);

	PrimarySession->RequestPerSession = REQUEST_PER_SESSION;
	for(mid=0; mid<PrimarySession->RequestPerSession; mid++)
		PrimarySession->Slot[mid].SlotState = SLOT_WAIT;

	ClearFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_INITIALIZING);
	SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_START);

	KeSetEvent(&PrimarySession->ReadyEvent, IO_DISK_INCREMENT, FALSE);

	tdiStatus = LpxTdiRecvWithCompletionEvent(
					PrimarySession->ConnectionFileObject,
					&PrimarySession->TdiReceiveContext,
					(PUCHAR)&PrimarySession->NdfsRequestHeader,
					sizeof(NDFS_REQUEST_HEADER),
					0
					);

	if(!NT_SUCCESS(tdiStatus))
	{
		ASSERT(LFS_BUG);
		SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_ERROR);
		primarySessionExit = TRUE;
	}

	PrimarySession->Receiving = TRUE;

	while(primarySessionExit == FALSE)
	{
		PKEVENT				events[3];
		LONG				eventCount;
		NTSTATUS			eventStatus;

		LARGE_INTEGER		timeOut;
		PLIST_ENTRY			primarySessionRequestEntry;


		eventCount = 0;

		events[eventCount++] = &PrimarySession->RequestEvent;
		events[eventCount++] = &PrimarySession->WorkCompletionEvent;
		
		if(!BooleanFlagOn(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_SHUTDOWN))
		{
			_U16	mid;

			for(mid=0; mid<PrimarySession->RequestPerSession; mid++)
			{
				if(PrimarySession->Slot[mid].SlotState == SLOT_WAIT)
				{
					events[eventCount++] = &PrimarySession->TdiReceiveContext.CompletionEvent;
					break;
				}
			}
		}
		else
		{
			_U16	mid;

			for(mid=0; mid<PrimarySession->RequestPerSession; mid++)
			{
				if(PrimarySession->Slot[mid].SlotState != SLOT_WAIT)
				{
					break;
				}
			}
			if(mid == PrimarySession->RequestPerSession)
			{
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,	("DispatchRequest: SHUTDOWN occured before\n"));
				CloseOpenFiles(PrimarySession);
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,	("DispatchRequest: SHUTDOWN occured\n"));
			}
		}

		ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);

		timeOut.QuadPart = -5*HZ;
		eventStatus = KeWaitForMultipleObjects(
						eventCount,
						events,
						WaitAny,
						Executive,
						KernelMode,
						TRUE,
						&timeOut,
						NULL
						);

		if((eventStatus == STATUS_TIMEOUT 
			|| !BooleanFlagOn(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_SHUTDOWN) && eventStatus == 2) 
			&& ((PrimarySession->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0) 
				? (PrimarySession->State == SESSION_SETUP) : (PrimarySession->State == SESSION_TREE_CONNECT))
			&& PrimarySession->NetdiskPartition
			&& !(
				BooleanFlagOn(PrimarySession->NetdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED)
				|| PrimarySession->NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY][0].VolumeState == VolumeMounted
				|| PrimarySession->NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][0].VolumeState == VolumeMounted
				|| PrimarySession->NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY][1].VolumeState == VolumeMounted
				|| PrimarySession->NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY][1].VolumeState == VolumeMounted
			))
		{
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_ERROR,
					("BooleanFlagOn(PrimarySession->NetdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED) = %d\n",
						BooleanFlagOn(PrimarySession->NetdiskPartition->Flags, NETDISK_PARTITION_CORRUPTED)));
			primarySessionExit = TRUE;
			continue;
		}

		if(eventStatus == STATUS_TIMEOUT)
		{
			continue;
		}

		ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
		ASSERT(eventCount <= THREAD_WAIT_OBJECTS);
		ASSERT(eventStatus < THREAD_WAIT_OBJECTS);

		if(!NT_SUCCESS(eventStatus) || eventStatus >= eventCount)
		{
			ASSERT(LFS_UNEXPECTED);
			PrimarySession->ThreadFlags |= PRIMARY_SESSION_THREAD_ERROR;
			primarySessionExit = TRUE;
			continue;
		}
		
		KeClearEvent(events[eventStatus]);

		if(eventStatus == 0)
		{
			while(primarySessionRequestEntry = 
					ExInterlockedRemoveHeadList(
							&PrimarySession->RequestQueue,
							&PrimarySession->RequestQSpinLock
							)
					) 
			{
				PPRIMARY_SESSION_REQUEST	primarySessionRequest;
			
				primarySessionRequest = CONTAINING_RECORD(
											primarySessionRequestEntry,
											PRIMARY_SESSION_REQUEST,
											ListEntry
											);

				if(primarySessionRequest->RequestType == PRIMARY_SESSION_REQ_DISCONNECT) 
				{
					DisconnectFromSecondary(
						PrimarySession
						);

					SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_DISCONNECTED);
				}
				else if(primarySessionRequest->RequestType == PRIMARY_SESSION_REQ_DOWN) 
				{
					primarySessionExit = TRUE;
				}
				else if(primarySessionRequest->RequestType == PRIMARY_SESSION_SHUTDOWN) 
				{
					SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_SHUTDOWN);
				}
				else
				{
					ASSERT(LFS_BUG);
					PrimarySession->ThreadFlags |= PRIMARY_SESSION_THREAD_ERROR;
				}

				if(primarySessionRequest->Synchronous == TRUE)
					KeSetEvent(&primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
				else
					DereferencePrimarySessionRequest(primarySessionRequest);
			}

			continue;
		}
		else if(eventStatus == 1)
		{
			while(1)
			{
				_U16	mid;
	
				for(mid=0; mid<PrimarySession->RequestPerSession; mid++)
				{
					if(PrimarySession->Slot[mid].SlotState == SLOT_FINISH)
						break;
				}

				if(mid == PrimarySession->RequestPerSession)
					break;

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("PrimarySessionThreadProc: eventStatus = %d\n", eventStatus));
			
				PrimarySession->Slot[mid].SlotState = SLOT_WAIT;

				if(PrimarySession->Slot[mid].ReturnStatus == STATUS_SUCCESS)
				{
					PNDFS_REPLY_HEADER		ndfsReplyHeader;

					ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Slot[mid].ReplyMessageBuffer;
										
					PrimarySession->Slot[mid].ReturnStatus 
						= SendNtfsWinxpMessage(
									PrimarySession,
									ndfsReplyHeader,
									PrimarySession->Slot[mid].NdfsWinxpReplyHeader,
									PrimarySession->Slot[mid].ReplyDataSize,
									mid
									);

				}
	
				if(PrimarySession->Slot[mid].ExtendWinxpRequestMessagePool)
				{
					ExFreePool(PrimarySession->Slot[mid].ExtendWinxpRequestMessagePool);	
					PrimarySession->Slot[mid].ExtendWinxpRequestMessagePool = NULL;
					PrimarySession->Slot[mid].ExtendWinxpReplyMessagePoolLength = 0;
				}
		
				if(PrimarySession->Slot[mid].ExtendWinxpReplyMessagePool)
				{
					ExFreePool(PrimarySession->Slot[mid].ExtendWinxpReplyMessagePool);	
					PrimarySession->Slot[mid].ExtendWinxpReplyMessagePool = NULL;
					PrimarySession->Slot[mid].ExtendWinxpReplyMessagePoolLength = 0;
				}

				if(!(PrimarySession->Slot[mid].ReturnStatus == STATUS_SUCCESS || PrimarySession->Slot[mid].ReturnStatus == STATUS_PENDING) 
					|| PrimarySession->State == SESSION_CLOSED
					)
				{
					if(PrimarySession->NetdiskPartition)
					{
						MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, PrimarySession->NetdiskPartition);
						PrimarySession->NetdiskPartition = NULL;
					}
					primarySessionExit = TRUE;
					break;		
				}

				if(PrimarySession->Slot[mid].ReturnStatus == STATUS_SUCCESS)
				{
					if(PrimarySession->Receiving == FALSE)
					{
						tdiStatus = LpxTdiRecvWithCompletionEvent(
										PrimarySession->ConnectionFileObject,
										&PrimarySession->TdiReceiveContext,
										(PUCHAR)&PrimarySession->NdfsRequestHeader,
										sizeof(NDFS_REQUEST_HEADER),
										0
										);

						if(!NT_SUCCESS(tdiStatus))
						{
							ASSERT(LFS_BUG);
							SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_ERROR);
							primarySessionExit = TRUE;
							break;
						}
						PrimarySession->Receiving = TRUE;
					}
				}
			}		
		
			continue;
		}
		else
		{
			NTSTATUS dispatchStatus;
			NTSTATUS tdiStatus;

			ASSERT(!BooleanFlagOn(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_SHUTDOWN) && eventStatus == 2);  // Receive Event
	
			if(PrimarySession->TdiReceiveContext.Result != sizeof(NDFS_REQUEST_HEADER))
			{
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchRequest: Disconnected, PrimarySession = %p Data received:%d\n",
							PrimarySession,
							PrimarySession->TdiReceiveContext.Result
					));

				if(PrimarySession->NetdiskPartition)
				{
					MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, PrimarySession->NetdiskPartition);
					PrimarySession->NetdiskPartition = NULL;
				}
				PrimarySession->ThreadFlags |= PRIMARY_SESSION_THREAD_DISCONNECTED;
				primarySessionExit = TRUE;
				
				continue;		
			}

			PrimarySession->Receiving = FALSE;

			dispatchStatus = DispatchRequest(PrimarySession);

			if(!(dispatchStatus == STATUS_SUCCESS || dispatchStatus == STATUS_PENDING) 
				|| PrimarySession->State == SESSION_CLOSED
				)
			{
				if(PrimarySession->NetdiskPartition)
				{
					MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, PrimarySession->NetdiskPartition);
					PrimarySession->NetdiskPartition = NULL;
				}
				primarySessionExit = TRUE;
				continue;		
			}

			if(dispatchStatus == STATUS_SUCCESS)
			{
				if(PrimarySession->Receiving == FALSE)
				{
					tdiStatus = LpxTdiRecvWithCompletionEvent(
								PrimarySession->ConnectionFileObject,
								&PrimarySession->TdiReceiveContext,
								(PUCHAR)&PrimarySession->NdfsRequestHeader,
								sizeof(NDFS_REQUEST_HEADER),
								0
								);

					if(!NT_SUCCESS(tdiStatus))
					{
						ASSERT(LFS_BUG);
						SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_ERROR);
						primarySessionExit = TRUE;
					}
					PrimarySession->Receiving = TRUE;
				}
			}
			
			continue;
		}
	}

	while(1)
	{
		_U16			midIndex;
		LARGE_INTEGER	timeOut;
		NTSTATUS		eventStatus;


		for(midIndex=0; midIndex<PrimarySession->RequestPerSession; midIndex++)
		{
			if(PrimarySession->Slot[midIndex].SlotState != SLOT_WAIT)
				break;
		}

		if(midIndex == PrimarySession->RequestPerSession)
			break;

		timeOut.QuadPart = -10*HZ;
		eventStatus = KeWaitForSingleObject(
						&PrimarySession->WorkCompletionEvent,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

		KeClearEvent(&PrimarySession->WorkCompletionEvent);

		if(eventStatus == STATUS_TIMEOUT)
		{
			ASSERT(LFS_UNEXPECTED);
		}

		while(1)
		{
			_U16	mid;
	
			for(mid=0; mid<PrimarySession->RequestPerSession; mid++)
			{
				if(PrimarySession->Slot[mid].SlotState == SLOT_FINISH)
					break;
			}

			if(mid == PrimarySession->RequestPerSession)
				break;

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				("PrimarySessionThreadProc: eventStatus = %d\n", eventStatus));
			
			PrimarySession->Slot[mid].SlotState = SLOT_WAIT;

			if(PrimarySession->Slot[mid].ExtendWinxpRequestMessagePool)
			{
				ExFreePool(PrimarySession->Slot[mid].ExtendWinxpRequestMessagePool);	
				PrimarySession->Slot[mid].ExtendWinxpRequestMessagePool = NULL;
				PrimarySession->Slot[mid].ExtendWinxpReplyMessagePoolLength = 0;
			}
		
			if(PrimarySession->Slot[mid].ExtendWinxpReplyMessagePool)
			{
				ExFreePool(PrimarySession->Slot[mid].ExtendWinxpReplyMessagePool);	
				PrimarySession->Slot[mid].ExtendWinxpReplyMessagePool = NULL;
				PrimarySession->Slot[mid].ExtendWinxpReplyMessagePoolLength = 0;
			}
		}		
	}
	
	if(!BooleanFlagOn(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_DISCONNECTED))
	{
		DisconnectFromSecondary(PrimarySession);
		SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_DISCONNECTED);
	}
	
	if(PrimarySession->NetdiskPartition)
	{
		MountManager_ReturnPrimaryPartition(GlobalLfs.NetdiskManager, PrimarySession->NetdiskPartition);
		PrimarySession->NetdiskPartition = NULL;
	}

	CloseOpenFiles(PrimarySession);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
				("PrimarySessionThreadProc: PsTerminateSystemThread PrimarySession = %p\n", PrimarySession));

	SetFlag(PrimarySession->ThreadFlags, PRIMARY_SESSION_THREAD_TERMINATED);

#if DBG
	InterlockedDecrement(&LfsObjectCounts.PrimarySessionThreadCount);
#endif
	PrimarySession_Dereference (PrimarySession) ;

	PsTerminateSystemThread(STATUS_SUCCESS);
}


VOID
DisconnectFromSecondary(
	IN	PPRIMARY_SESSION PrimarySession
	)
{
	ASSERT(PrimarySession->ConnectionFileHandle);
	ASSERT(PrimarySession->ConnectionFileObject);
/*	
    Cancelling IRSs at this point may cause BSOD.
    hootch 02102004
	
	if(PrimarySession->TdiReceiveContext.Irp)
	{
		LARGE_INTEGER		timeOut ;
		NTSTATUS			ntStatus ;
		
		IoCancelIrp(PrimarySession->TdiReceiveContext.Irp);

		timeOut.QuadPart = - LFS_TIME_OUT ;
		ntStatus = KeWaitForSingleObject(
				&PrimarySession->TdiReceiveContext.CompletionEvent,
				Executive,
				KernelMode,
				FALSE,
				&timeOut
				);
			
		if(ntStatus != STATUS_SUCCESS) 
		{
			ASSERT(LPX_BUG);
			return;
		}

		KeClearEvent(&PrimarySession->TdiReceiveContext.CompletionEvent) ;
		PrimarySession->TdiReceiveContext.Irp = NULL;
	}
*/	
	LpxTdiDisconnect(PrimarySession->ConnectionFileObject, 0);

	return;
}	


PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest(
	IN	BOOLEAN	Synchronous
) 
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;


	primarySessionRequest = ExAllocatePoolWithTag(
							NonPagedPool,
							sizeof(PRIMARY_SESSION_REQUEST),
							PRIMARY_SESSION_MESSAGE_TAG
							);

	if(primarySessionRequest == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(primarySessionRequest, sizeof(PRIMARY_SESSION_REQUEST)) ;

	primarySessionRequest->ReferenceCount = 1 ;
	InitializeListHead(&primarySessionRequest->ListEntry) ;
	
	primarySessionRequest->Synchronous = Synchronous;
	KeInitializeEvent(&primarySessionRequest->CompleteEvent, NotificationEvent, FALSE) ;


	return primarySessionRequest ;
}


VOID
DereferencePrimarySessionRequest(
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	) 
{
	LONG	result ;

	result = InterlockedDecrement(&PrimarySessionRequest->ReferenceCount) ;

	ASSERT( result >= 0) ;

	if(0 == result)
	{
		ExFreePoolWithTag(PrimarySessionRequest, PRIMARY_SESSION_MESSAGE_TAG) ;
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				("FreePrimarySessionRequest: PrimarySessionRequest freed\n")) ;
	}

	return;
}


FORCEINLINE
VOID
QueueingPrimarySessionRequest(
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	)
{
	ExInterlockedInsertTailList(
		&PrimarySession->RequestQueue,
		&PrimarySessionRequest->ListEntry,
		&PrimarySession->RequestQSpinLock
		);

	KeSetEvent(&PrimarySession->RequestEvent, IO_DISK_INCREMENT, FALSE) ;
	
	return;
}


POPEN_FILE
PrimarySession_AllocateOpenFile(
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject,
	IN	PUNICODE_STRING		FullFileName
	) 
{
	POPEN_FILE	openFile;


	openFile = ExAllocatePoolWithTag(
						NonPagedPool,
						sizeof(OPEN_FILE),
						OPEN_FILE_TAG
						);
	
	if (openFile == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(openFile,	sizeof(OPEN_FILE));

	openFile->FileHandle = FileHandle;
	openFile->FileObject = FileObject;

	openFile->PrimarySession = PrimarySession;

    RtlInitEmptyUnicodeString(
				&openFile->FullFileName,
                openFile->FullFileNameBuffer,
				sizeof(openFile->FullFileNameBuffer)
				);

	RtlCopyUnicodeString(
		&openFile->FullFileName,
		FullFileName
		);

	InitializeListHead(&openFile->ListEntry);
	
	ExInterlockedInsertHeadList(
			&PrimarySession->OpenedFileQueue,
			&openFile->ListEntry,
			&PrimarySession->OpenedFileQSpinLock
			);

#if DBG
	InterlockedIncrement(&LfsObjectCounts.OpenFileCount);
#endif

	SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
		("PrimarySession_AllocateOpenFile OpenFile = %p, OpenFileCount = %d\n", openFile, LfsObjectCounts.OpenFileCount));

	return openFile;
}


POPEN_FILE
PrimarySession_FindOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U32			 OpenFileId
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;


	KeAcquireSpinLock(&PrimarySession->SpinLock, &oldIrql);

    for (listEntry = PrimarySession->OpenedFileQueue.Flink;
         listEntry != &PrimarySession->OpenedFileQueue;
         listEntry = listEntry->Flink) 
	{

		openFile = CONTAINING_RECORD (listEntry, OPEN_FILE, ListEntry);

		if(openFile->OpenFileId == OpenFileId)
			break;

		openFile = NULL;
	}

	KeReleaseSpinLock(&PrimarySession->SpinLock, oldIrql);

	return openFile;
}


POPEN_FILE
PrimarySession_FindOpenFileByName(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  ACCESS_MASK			DesiredAccess
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;


	KeAcquireSpinLock(&PrimarySession->SpinLock, &oldIrql);

    for (openFile = NULL, listEntry = PrimarySession->OpenedFileQueue.Flink;
         listEntry != &PrimarySession->OpenedFileQueue;
         openFile = NULL, listEntry = listEntry->Flink) 
	{

		openFile = CONTAINING_RECORD (listEntry, OPEN_FILE, ListEntry);

		if(openFile->AlreadyClosed == TRUE)
			continue;

		if(openFile->FullFileName.Length != FullFileName->Length)
			continue;

		if(RtlEqualMemory(openFile->FullFileName.Buffer, FullFileName->Buffer, FullFileName->Length)
			&& ((openFile->DesiredAccess & DesiredAccess) == DesiredAccess))
			break;
	}

	KeReleaseSpinLock(&PrimarySession->SpinLock, oldIrql);

	return openFile;
}


POPEN_FILE
PrimarySession_FindOpenFileCleanUpedAndNotClosed(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  BOOLEAN				DeleteOnClose
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;


	KeAcquireSpinLock(&PrimarySession->SpinLock, &oldIrql);

    for (openFile = NULL, listEntry = PrimarySession->OpenedFileQueue.Flink;
         listEntry != &PrimarySession->OpenedFileQueue;
         openFile = NULL, listEntry = listEntry->Flink) 
	{

		openFile = CONTAINING_RECORD (listEntry, OPEN_FILE, ListEntry);

		if(openFile->CleanUp != TRUE)
			continue;

		if(openFile->AlreadyClosed == TRUE)
			continue;

		if(openFile->FullFileName.Length != FullFileName->Length)
			continue;

		if(RtlEqualMemory(openFile->FullFileName.Buffer, FullFileName->Buffer, FullFileName->Length))
		{
			if(DeleteOnClose == FALSE && openFile->CreateOptions & FILE_DELETE_ON_CLOSE)
				continue;
			else
				break;
		}
	}

	KeReleaseSpinLock(&PrimarySession->SpinLock, oldIrql);

	return openFile;
}


VOID
PrimarySession_FreeOpenFile(
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	)
{
	UNREFERENCED_PARAMETER(PrimarySession);

	SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
		("PrimarySession_FreeOpenFile OpenFile = %p, OpenFileCount = %d\n", OpenedFile, LfsObjectCounts.OpenFileCount));
	
	ASSERT(OpenedFile->ListEntry.Flink == OpenedFile->ListEntry.Blink);

	ASSERT(OpenedFile);
	
	OpenedFile->FileHandle = 0;
	OpenedFile->PrimarySession = NULL;

#if DBG
	InterlockedDecrement(&LfsObjectCounts.OpenFileCount);
#endif

	ExFreePoolWithTag(
		OpenedFile,
		OPEN_FILE_TAG
		);
}
