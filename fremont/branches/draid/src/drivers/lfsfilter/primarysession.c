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
	IN	_U64			 OpenFileId
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

POPEN_FILE
PrimarySession_FindOrReopenOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U64			 OpenFileId
	);

_U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	);


PPRIMARY_SESSION
PrimarySession_Create(
	IN  PPRIMARY			Primary,
	IN	HANDLE				ListenFileHandle,
	IN  PFILE_OBJECT		ListenFileObject,
	IN  ULONG				ListenSocketIndex,
	IN  PLPX_ADDRESS		RemoteAddress
	)
{
	PPRIMARY_SESSION	primarySession;
 	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			status;
	LARGE_INTEGER		timeOut;

		
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	Primary_Reference( Primary );

	primarySession = FsRtlAllocatePoolWithTag( NonPagedPool, sizeof(PRIMARY_SESSION), LFS_ALLOC_TAG );
	
	if (primarySession == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		Primary_Dereference( Primary );
		return NULL;
	}

	try {
	
		RtlZeroMemory( primarySession, sizeof(PRIMARY_SESSION) );

		primarySession->Flags = PRIMARY_SESSION_FLAG_INITIALIZING;

		primarySession->ReferenceCount = 1;
		primarySession->Primary = Primary;
		
		ExInitializeFastMutex( &primarySession->FastMutex )
		
		InitializeListHead( &primarySession->ListEntry );

		primarySession->ConnectionFileHandle		= ListenFileHandle;
		primarySession->ConnectionFileObject		= ListenFileObject;
		RtlCopyMemory( &primarySession->RemoteAddress, RemoteAddress, sizeof(LPX_ADDRESS) );
		primarySession->IsLocalAddress				= Lfs_IsLocalAddress(RemoteAddress);
#if 0
		primarySession->SessionContext.SessionKey			= SessionInformation->SessionKey;
		primarySession->SessionContext.Flags				= SessionInformation->SessionFlags;
		primarySession->SessionContext.PrimaryMaxDataSize	= SessionInformation->PrimaryMaxDataSize;
		primarySession->SessionContext.SecondaryMaxDataSize	= SessionInformation->SecondaryMaxDataSize;
		primarySession->SessionContext.Uid					= SessionInformation->Uid;
		primarySession->SessionContext.Tid					= SessionInformation->Tid;
		primarySession->SessionContext.NdfsMajorVersion		= SessionInformation->NdfsMajorVersion;
		primarySession->SessionContext.NdfsMinorVersion		= SessionInformation->NdfsMinorVersion;
		primarySession->SessionContext.SessionSlotCount		= (UCHAR)SessionInformation->SessionSlotCount;
#endif
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("primarySession->ConnectionFileHandle = %x\n", primarySession->ConnectionFileHandle) );

		KeInitializeEvent( &primarySession->ReadyEvent, NotificationEvent, FALSE );
	
		InitializeListHead( &primarySession->RequestQueue );
		KeInitializeSpinLock( &primarySession->RequestQSpinLock );
		KeInitializeEvent( &primarySession->RequestEvent, NotificationEvent, FALSE );

		InitializeListHead( &primarySession->NetdiskPartitionListEntry );

		primarySession->ThreadHandle = 0;
		primarySession->ThreadObject = NULL;

		primarySession->Thread.TdiReceiveContext.Irp = NULL;
		KeInitializeEvent( &primarySession->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );

		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		primarySession->Thread.SessionState = SESSION_CLOSE;
		primarySession->Thread.SessionKey = (_U32)PtrToUlong(primarySession);

		primarySession->Thread.PrimaryMaxDataSize = (LfsRegistry.MaxDataTransferPri < DEFAULT_MAX_DATA_SIZE)
													? LfsRegistry.MaxDataTransferPri:DEFAULT_MAX_DATA_SIZE;

		primarySession->Thread.SecondaryMaxDataSize = (LfsRegistry.MaxDataTransferSec < DEFAULT_MAX_DATA_SIZE)
													? LfsRegistry.MaxDataTransferSec:DEFAULT_MAX_DATA_SIZE;

		//
		//	Initialize transport context for traffic control
		//

		InitTransCtx( &primarySession->Thread.TransportCtx, primarySession->Thread.SecondaryMaxDataSize );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_Create: PriMaxData:%08u SecMaxData:%08u\n",
												  primarySession->Thread.PrimaryMaxDataSize,
												  primarySession->Thread.SecondaryMaxDataSize) );

		primarySession->Thread.Uid = (_U16)primarySession;

		status = PsCreateSystemThread( &primarySession->ThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   PrimarySessionThreadProc,
									   primarySession );
	
		if (!NT_SUCCESS(status)) {

			leave;
		}

		status = ObReferenceObjectByHandle( primarySession->ThreadHandle,
											FILE_READ_DATA,
											NULL,
											KernelMode,
											&primarySession->ThreadObject,
											NULL );

		if (!NT_SUCCESS(status)) {

			leave;
		}

		timeOut.QuadPart = -LFS_TIME_OUT;
		status = KeWaitForSingleObject( &primarySession->ReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );


		if (!NT_SUCCESS(status)) {

			leave;
		}

		KeClearEvent( &primarySession->ReadyEvent );

		ExInterlockedInsertTailList( &Primary->PrimarySessionQueue[ListenSocketIndex],
									 &primarySession->ListEntry,
									 &Primary->PrimarySessionQSpinLock[ListenSocketIndex] );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_Create: The primary thread are ready\n") );
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("Fat PrimarySession_Create: primarySession = %p\n", primarySession) );
	
	} finally {

		if (AbnormalTermination()) {

			status = STATUS_UNSUCCESSFUL;
		}

		if (!NT_SUCCESS(status)) {

			ASSERT( LFS_UNEXPECTED );
			PrimarySession_Close( primarySession );
			primarySession = NULL;
		}
	}

	return primarySession;
}

VOID
PrimarySession_Close(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_Close: PrimarySession = %p\n", PrimarySession) );

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );
	if (FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP)) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		return;
	}

	SetFlag( PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
	
	if (PrimarySession->ThreadHandle == NULL) {

		ASSERT( LFS_BUG );
		PrimarySession_Dereference( PrimarySession );

		return;
	}

	ASSERT( PrimarySession->ThreadObject != NULL );

	if (FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_TERMINATED)) {

		ObDereferenceObject( PrimarySession->ThreadObject );

		PrimarySession->ThreadHandle = NULL;
		PrimarySession->ThreadObject = NULL;

	
	} else {

		PPRIMARY_SESSION_REQUEST	primarySessionRequest;
		NTSTATUS					ntStatus;
		LARGE_INTEGER				timeOut;
	
		
		primarySessionRequest = AllocPrimarySessionRequest( FALSE );
		primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DISCONNECT;

		QueueingPrimarySessionRequest( PrimarySession,
									   primarySessionRequest,
									   FALSE );

		primarySessionRequest = AllocPrimarySessionRequest( FALSE );
		primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DOWN;

		QueueingPrimarySessionRequest( PrimarySession,
									   primarySessionRequest,
									   FALSE );

		timeOut.QuadPart = - LFS_TIME_OUT;
		ntStatus = KeWaitForSingleObject( PrimarySession->ThreadObject,
										  Executive,
										  KernelMode,
										  FALSE,
										  &timeOut );

		if (ntStatus == STATUS_SUCCESS) {

		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_Close: thread stoped\n") );

			ObDereferenceObject( PrimarySession->ThreadObject );

			PrimarySession->ThreadHandle = NULL;
			PrimarySession->ThreadObject = NULL;
				
		} else {

			ASSERT( LFS_BUG );
			return;
		}
	}

#if 0
    interval.QuadPart = (5 * DELAY_ONE_SECOND);      //delay 5 seconds
    KeDelayExecutionThread( KernelMode, FALSE, &interval );
#endif

	if (PrimarySession->ConnectionFileHandle) {

		LpxTdiDisassociateAddress( PrimarySession->ConnectionFileObject );
		LpxTdiCloseConnection( PrimarySession->ConnectionFileHandle, PrimarySession->ConnectionFileObject );
	}

	PrimarySession->ConnectionFileHandle = NULL;
	PrimarySession->ConnectionFileObject = NULL;

	PrimarySession_Dereference( PrimarySession );

	return;
}


VOID
PrimarySession_FileSystemShutdown(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;
	NTSTATUS					ntStatus;
	LARGE_INTEGER				timeOut;

	
	PrimarySession_Reference( PrimarySession );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
				   ("PrimarySession_FileSystemShutdown: PrimarySession = %p\n", PrimarySession) );
		
	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_SHUTDOWN;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest, FALSE );

	timeOut.QuadPart = -LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("PrimarySession_FileSystemShutdown: thread shutdown\n") );		
	
	} else {

		ASSERT( LFS_BUG );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	PrimarySession_Dereference( PrimarySession );

	return;
}


VOID
PrimarySession_Stopping(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;
	NTSTATUS					ntStatus;
	LARGE_INTEGER				timeOut;

	
	PrimarySession_Reference( PrimarySession );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
				   ("PrimarySession_Stopping: PrimarySession = %p\n", PrimarySession) );

	ASSERT( PrimarySession->ThreadObject != NULL );

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );

	if (PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_STOPPING;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest, TRUE );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );

	timeOut.QuadPart = -LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_CloseFiles: thread close files\n") );

		ASSERT( FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING) );
	
	} else {

		ASSERT( LFS_BUG );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	PrimarySession_Dereference( PrimarySession );

	return;
}


VOID
PrimarySession_Disconnect(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;
	NTSTATUS					ntStatus;
	LARGE_INTEGER				timeOut;

	
	PrimarySession_Reference( PrimarySession );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
				   ("PrimarySession_Disconnect: PrimarySession = %p\n", PrimarySession) );

	ASSERT( PrimarySession->ThreadObject != NULL );

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );

	if (!FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING)) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DISCONNECT;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest, TRUE );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );

	timeOut.QuadPart = -LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_CloseFiles: thread close files\n") );		
	
	} else {

		ASSERT( LFS_BUG );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	PrimarySession_Dereference( PrimarySession );

	return;
}


VOID
PrimarySession_CancelStopping(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;
	NTSTATUS					ntStatus;
	LARGE_INTEGER				timeOut;

	
	PrimarySession_Reference( PrimarySession );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
				   ("PrimarySession_CancelStopping: PrimarySession = %p\n", PrimarySession) );

	ASSERT( PrimarySession->ThreadObject != NULL );

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );

	if (!FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING)) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		PrimarySession_Dereference( PrimarySession );
		return;
	}
		
	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_CANCEL_STOPPING;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest, TRUE );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );

	timeOut.QuadPart = -LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_CancelStopping: thread close files\n") );		
	
	} else {

		ASSERT( LFS_BUG );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	PrimarySession_Dereference( PrimarySession );

	return;
}


VOID
PrimarySession_Reference (
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
    LONG result;
	
    result = InterlockedIncrement( &PrimarySession->ReferenceCount );

    ASSERT( result >= 0 );
}


VOID
PrimarySession_Dereference (
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
    LONG result;


    result = InterlockedDecrement( &PrimarySession->ReferenceCount );
    ASSERT( result >= 0 );

    if (result == 0) {

		KIRQL		oldIrql;
		PPRIMARY    primary = PrimarySession->Primary;

		
		KeAcquireSpinLock(&PrimarySession->Primary->PrimarySessionQSpinLock[PrimarySession->ListenSocketIndex], &oldIrql);
		RemoveEntryList(&PrimarySession->ListEntry);
		KeReleaseSpinLock(&PrimarySession->Primary->PrimarySessionQSpinLock[PrimarySession->ListenSocketIndex], oldIrql);
		
		ExFreePoolWithTag( PrimarySession, LFS_ALLOC_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("PrimarySession_Dereference: PrimarySession = %p is Freed\n", PrimarySession) );

		Primary_Dereference( primary );
	}
}


PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest(
	IN	BOOLEAN	Synchronous
	) 
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;


	primarySessionRequest = ExAllocatePoolWithTag( NonPagedPool,
												   sizeof(PRIMARY_SESSION_REQUEST),
												   PRIMARY_SESSION_MESSAGE_TAG );

	if (primarySessionRequest == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( primarySessionRequest, sizeof(PRIMARY_SESSION_REQUEST) );

	primarySessionRequest->ReferenceCount = 1;
	InitializeListHead( &primarySessionRequest->ListEntry );
	
	primarySessionRequest->Synchronous = Synchronous;
	KeInitializeEvent( &primarySessionRequest->CompleteEvent, NotificationEvent, FALSE );


	return primarySessionRequest;
}


FORCEINLINE
NTSTATUS
QueueingPrimarySessionRequest(
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest,
	IN  BOOLEAN						FastMutexAcquired
	)
{
	NTSTATUS	status;


	ASSERT( PrimarySessionRequest->ListEntry.Flink == PrimarySessionRequest->ListEntry.Blink );

	if (FastMutexAcquired == FALSE)
		ExAcquireFastMutex( &PrimarySession->FastMutex );

	if (FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_START) &&
		!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_STOPED)) {

		ExInterlockedInsertTailList( &PrimarySession->RequestQueue,
									 &PrimarySessionRequest->ListEntry,
									 &PrimarySession->RequestQSpinLock );

		KeSetEvent( &PrimarySession->RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		status = STATUS_UNSUCCESSFUL;
	}

	if (FastMutexAcquired == FALSE)
		ExReleaseFastMutex( &PrimarySession->FastMutex );

	if (status == STATUS_UNSUCCESSFUL) {
	
		PrimarySessionRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;

		if (PrimarySessionRequest->Synchronous == TRUE)
			KeSetEvent( &PrimarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferencePrimarySessionRequest( PrimarySessionRequest );
	}

	return status;
}


VOID
DereferencePrimarySessionRequest(
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	) 
{
	LONG	result;

	result = InterlockedDecrement( &PrimarySessionRequest->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		ExFreePoolWithTag( PrimarySessionRequest, PRIMARY_SESSION_MESSAGE_TAG );
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,	("FreePrimarySessionRequest: PrimarySessionRequest freed\n") );
	}

	return;
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


_U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	)
{
	_U32 returnSize;

	UNREFERENCED_PARAMETER( PrimarySession );

	switch(NdfsWinxpRequestHeader->IrpMajorFunction)
	{
    case IRP_MJ_CREATE: //0x00
	{
#if 0
		if(NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CREATE
			&& PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NTFS)
		{
			returnSize = PrimarySession->Thread.BytesPerFileRecordSegment;
		}
		else
#endif
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
	outputBufferLength = sizeof(NTFS_FILE_RECORD_OUTPUT_BUFFER) + PrimarySession->Thread.BytesPerFileRecordSegment - sizeof(UCHAR);
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
//		ASSERT(LFS_UNEXPECTED);
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
					("GetFileRecordSegmentHeader: FSCTL_GET_NTFS_FILE_RECORD failed FileHandle %p, fileSystemControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						FileHandle, fileSystemControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));

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
		PrimarySession->Thread.BytesPerFileRecordSegment
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
/*
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
			PrimarySession->Thread.BytesPerFileRecordSegment
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
				PrimarySession->Thread.BytesPerFileRecordSegment
				);
	}
	
	return STATUS_SUCCESS; */
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
	HANDLE					eventHandle;

	UNREFERENCED_PARAMETER(PrimarySession);

	if (OpenFile->EventHandle == NULL) {
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
		} else {
			OpenFile->EventHandle = eventHandle;
		}
	}
	
	ioStatusBlock.Information = 0;

#if 1
	// to do: OpenFile->DesiredAccess may have changed after file has opened.
	//			Need to handle this case.
	desiredAccess = OpenFile->DesiredAccess;
#else
	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
						| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT(desiredAccess == 0x0012019F);
#endif
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
#if 1
	createOptions     = OpenFile->CreateOptions;
#else
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_WRITE_THROUGH;
#endif
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
	if(createStatus != STATUS_SUCCESS) {
		if (OpenFile->EventHandle != NULL) {
			ZwClose(OpenFile->EventHandle);
		}
		return NULL;
	}
	// If previous file position is set, set it again.
	if (createOptions &(FILE_SYNCHRONOUS_IO_ALERT |FILE_SYNCHRONOUS_IO_NONALERT) 
		&& (OpenFile->CurrentByteOffset.LowPart !=0 || OpenFile->CurrentByteOffset.HighPart !=0)) {
		NTSTATUS SetStatus;
		IO_STATUS_BLOCK IoStatus;
		FILE_POSITION_INFORMATION FileInfo;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
			("Setting offset of reopened file to %I64x\n", OpenFile->CurrentByteOffset.QuadPart));
		FileInfo.CurrentByteOffset = OpenFile->CurrentByteOffset;
		//
		// File position is meaningful only in this open mode.
		//
		SetStatus = ZwSetInformationFile(
			fileHandle,
			&IoStatus, 
			(PVOID)&FileInfo,
			sizeof(FileInfo),
			FilePositionInformation
		);
		if (!NT_SUCCESS(SetStatus)) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				("Failed to set offset of reopened file to %I64d\n", OpenFile->CurrentByteOffset.QuadPart));
		}
		OpenFile->CurrentByteOffset.LowPart = 0;
		OpenFile->CurrentByteOffset.HighPart = 0;
	}
	return fileHandle;
}

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
		_U64				openFileId = NdfsWinxpRequestHeader->FileHandle;
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
		PWCHAR				fileNameBuffer;
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
		_U64				relatedOpenFileId = NdfsWinxpRequestHeader->Create.RelatedFileHandle;
		POPEN_FILE			relatedOpenFile = NULL;
		
		PFILE_RECORD_SEGMENT_HEADER	fileRecordSegmentHeader = NULL;
		

		//
		//	Allocate name buffer
		//

		fileNameBuffer = ExAllocatePool(NonPagedPool, NDFS_MAX_PATH);
		if(fileNameBuffer == NULL) {
			ASSERT(LFS_UNEXPECTED);
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

	    RtlInitEmptyUnicodeString( 
				&fileName,
                fileNameBuffer,
                NDFS_MAX_PATH
				);

		if(relatedOpenFileId == 0) 
		{
		    SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: No RelatedFileHandle\n"));

			createStatus = RtlAppendUnicodeStringToString(
							&fileName,
							&PrimarySession->NetdiskPartitionInformation.VolumeName
							);

			if(createStatus != STATUS_SUCCESS)
			{
				ExFreePool(fileNameBuffer);
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
			ASSERT(relatedOpenFile);					
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
				ExFreePool(fileNameBuffer);
				ASSERT(LFS_UNEXPECTED);
				ntStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		}

	    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: fileName = %wZ\n", &fileName));
		

#if __LFS__
		desiredAccess			= NdfsWinxpRequestHeader->Create.SecurityContext.DesiredAccess;
#else
		desiredAccess			= NdfsWinxpRequestHeader->Create.DesiredAccess;
#endif

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

#if __LFS__
		allocationSize.QuadPart = NdfsWinxpRequestHeader->Create.AllocationSize;
#else
		allocationSize.LowPart  = NdfsWinxpRequestHeader->Create.AllocationSizeLowPart;
		allocationSize.HighPart = NdfsWinxpRequestHeader->Create.AllocationSizeHighPart;
#endif

		fileAttributes			= NdfsWinxpRequestHeader->Create.FileAttributes;		
		shareAccess				= NdfsWinxpRequestHeader->Create.ShareAccess;
		disposition				= (NdfsWinxpRequestHeader->Create.Options & 0xFF000000) >> 24;
		createOptions			= NdfsWinxpRequestHeader->Create.Options & 0x00FFFFFF;
		// added by ktkim 03/15/2004
#if __LFS__
		createOptions			|= (NdfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions & FILE_DELETE_ON_CLOSE);
#else
		createOptions			|= (NdfsWinxpRequestHeader->Create.FullCreateOptions & FILE_DELETE_ON_CLOSE);
#endif
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
		//
		desiredAccess |= SYNCHRONIZE;
		if(!(createOptions & FILE_SYNCHRONOUS_IO_NONALERT))
			createOptions |= FILE_SYNCHRONOUS_IO_ALERT;
		createOptions |=  FILE_WRITE_THROUGH;
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

			if(createStatus != STATUS_SHARING_VIOLATION) {
				break;
			} else	{
				//
				// Close open handle that I may already opened.
				//
				POPEN_FILE	siblingOpenFile = NULL;

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: IoCreateFile failed %x\n", createStatus));

				siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed(
									PrimarySession,
									&fileName,
									FALSE
									);

				if(siblingOpenFile == NULL) {
					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							("DispatchWinXpRequest: Failed to open sibling open file. createStatus=%x\n", createStatus));
					break;
				} else {
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
				ExFreePool(fileNameBuffer);
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
			openFile->AlreadyClosed = FALSE;
			openFile->CleanUp = FALSE;
			openFile->DesiredAccess = desiredAccess;
			openFile->CreateOptions = createOptions;
			
			if(PrimarySession->Thread.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0
				&& NdfsWinxpRequestHeader->Create.FileNameLength 
				&& PrimarySession->FileSystemType == LFS_FILE_SYSTEM_NTFS
				&& PrimarySession->Thread.BytesPerFileRecordSegment != 0)
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

		//
		//	[64bit issue]
		//	We assume Information value of CREATE operation will be
		//	less than 32bit.
		//
		//	FILE_SUPERSEDED <= Information <= FILE_DOES_NOT_EXIST 
		//	 (0x00000000)                         (0x00000005)

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;

		if(createStatus == STATUS_SUCCESS)
		{
			//DbgPrint( "openFile->OpenFileId = %p\n", openFile->OpenFileId );

			NdfsWinxpReplytHeader->Open.FileHandle = openFile->OpenFileId;
			NdfsWinxpReplytHeader->Open.SetSectionObjectPointer = openFile->FileObject->SectionObjectPointer ? TRUE : FALSE;
		}

		if(fileRecordSegmentHeader)
		{
			LARGE_INTEGER  currentTime;

			KeQuerySystemTime(&currentTime);	

			*replyDataSize = PrimarySession->Thread.BytesPerFileRecordSegment;

			NdfsWinxpReplytHeader->Open.OpenTimeHigPart = currentTime.HighPart;
			NdfsWinxpReplytHeader->Open.OpenTimeLowPartMsb = ((_U8 *)&currentTime.LowPart)[3];
		}
		else
		{
			*replyDataSize = 0;
		}

		//
		//	Free name buffer
		//

		ExFreePool(fileNameBuffer);

		ntStatus = STATUS_SUCCESS;

		break;
	}

   case IRP_MJ_CLOSE: // 0x02
	{		
		_U64				openFileId = NdfsWinxpRequestHeader->FileHandle;
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
			openFile->EventHandle = NULL;
			openFile->AlreadyClosed = TRUE;
		}
		
		KeAcquireSpinLock(&PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql);
		RemoveEntryList(&openFile->ListEntry);
		KeReleaseSpinLock(&PrimarySession->Thread.OpenedFileQSpinLock, oldIrql);
		
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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;

		HANDLE					fileHandle = NULL;	
//		BOOLEAN					closeOnExit = FALSE;

		BOOLEAN					synchronousIo;
		
	    //PDEVICE_OBJECT		deviceObject;
		//PIRP					irp;
	    //PIO_STACK_LOCATION	irpSp;


		IO_STATUS_BLOCK			ioStatusBlock;
		PVOID					buffer;
		ULONG					length;
		LARGE_INTEGER			byteOffset;
		ULONG					key;

#if 0	// old code. Refactored into PrimarySession_FindOrReopenOpenFile
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile);

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
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		fileHandle = openFile->FileHandle;
#endif

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
			ASSERT(openFile->EventHandle !=NULL);		
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

		//
		//	[64bit issue]
		//	We assume Information value of READ operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status		= readStatus;
		NdfsWinxpReplytHeader->Information	= (_U32)ioStatusBlock.Information;
		//NdfsWinxpReplytHeader->CurrentByteOffset = openFile->FileObject->CurrentByteOffset.QuadPart;

		*replyDataSize = (_U32)ioStatusBlock.Information;
		ASSERT(*replyDataSize <= NdfsWinxpRequestHeader->Read.Length);

		ntStatus = STATUS_SUCCESS;

#if 0
		if(closeOnExit)
			ZwClose(fileHandle);
#endif
		break;
	}
	

    case IRP_MJ_WRITE: // 0x04
	{
		NTSTATUS				writeStatus;
		LONG					trial = 0;

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;

		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;
//		BOOLEAN					closeOnExit = FALSE;

		IO_STATUS_BLOCK			ioStatusBlock;
		PVOID					buffer;
		ULONG					length;
		LARGE_INTEGER			byteOffset;
		ULONG					key;

		
#if 0 // refactored into PrimarySession_FindOrReopenOpenFile
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile);

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
					POPEN_FILE	siblingOpenFileTemp;

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
									
					siblingOpenFileTemp = PrimarySession_FindOpenFileCleanUpedAndNotClosed(
										PrimarySession,
										&openFile->FullFileName,
										FALSE
										);
			
					if(siblingOpenFileTemp) 
					{
						NTSTATUS	closeStatus;

						ASSERT(siblingOpenFileTemp->FileObject);
						ObDereferenceObject(siblingOpenFileTemp->FileObject);
						siblingOpenFileTemp->FileObject = NULL;
						closeStatus = ZwClose(siblingOpenFileTemp->FileHandle);
						ASSERT(closeStatus == STATUS_SUCCESS);

						ASSERT(openFile->EventHandle !=NULL);
						closeStatus = ZwClose(siblingOpenFileTemp->EventHandle);
						ASSERT(closeStatus == STATUS_SUCCESS);
						openFile->EventHandle = NULL;
						siblingOpenFileTemp->AlreadyClosed = TRUE;

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
#else
RETRY_WRITE:
		openFile = PrimarySession_FindOrReopenOpenFile(
							PrimarySession,
							openFileId
							);
		fileHandle = openFile->FileHandle;
		fileObject = openFile->FileObject;

#endif

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

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				("DispatchWinXpRequest: NtWriteFile: openFileId = %wZ, Offset=%I64x, Length=%x, Result=%08x\n",
					&openFile->FullFileName, byteOffset.QuadPart, length, writeStatus));
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


		//
		//	[64bit issue]
		//	We assume Information value of WRITE operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status			 = writeStatus;
		NdfsWinxpReplytHeader->Information		 = (_U32)ioStatusBlock.Information;
		NdfsWinxpReplytHeader->CurrentByteOffset = fileObject->CurrentByteOffset.QuadPart;

		*replyDataSize = 0;

		ntStatus = STATUS_SUCCESS;
#if 0
		if(closeOnExit == TRUE)
		{
			ObDereferenceObject(fileObject);
			ZwClose(fileHandle);
		}
		else
#endif
		if(writeStatus == STATUS_ACCESS_DENIED)
		{
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: ZwWriteFile: Access denied\n"));
			
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

				ASSERT(openFile->EventHandle !=NULL);
				closeStatus = ZwClose(openFile->EventHandle);
				ASSERT(closeStatus == STATUS_SUCCESS);
				openFile->EventHandle = NULL;
				openFile->AlreadyClosed = TRUE;
				
				goto RETRY_WRITE;
			}
			else
			{
				goto RETRY_WRITE;
			}
		} else if (!NT_SUCCESS(ntStatus)) {
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: ZwWriteFile: failed %x\n", ntStatus));
		}

		break;
	}
	
    case IRP_MJ_QUERY_INFORMATION: // 0x05
	{
		NTSTATUS				queryInformationStatus;

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;
		BOOLEAN					closeOnExit = FALSE;

	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;
		ULONG					returnedLength = 0;
		
#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile);

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
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		fileHandle = openFile->FileHandle;
		fileObject = openFile->FileObject;
#endif


		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				("DispatchWinXpRequest: IoQueryFileInformation: openFileId = %X, Infoclass = %X\n",
					openFileId, NdfsWinxpRequestHeader->QueryFile.FileInformationClass));

		ASSERT(NdfsWinxpRequestHeader->QueryFile.Length <= PrimarySession->Thread.PrimaryMaxDataSize);
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
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
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
	
#if 0

    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		NTSTATUS				setInformationStatus;

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;


#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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
				ASSERT(rootDirectoryFile);
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
						("DispatchWinXpRequest: linkInfomation->FileNameLength = %u\n", linkInfomation->FileNameLength));
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
				ASSERT(rootDirectoryFile);
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
					("DispatchWinXpRequest: IoSetInformation: fileHandle = %p, fileInformationClass = %X, setInformationStatus = %X\n",
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

#else

	case IRP_MJ_SET_INFORMATION:  { // 0x06

		NTSTATUS				setInformationStatus;

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;


		openFile = PrimarySession_FindOrReopenOpenFile( PrimarySession, openFileId );

		//ASSERT( openFile && openFile->OpenFileId == openFile );

		fileInformation = ExAllocatePoolWithTag( NonPagedPool, 
												 NdfsWinxpRequestHeader->SetFile.Length +
												 2 + // NULL
												 8 + // 64 bit adjustment
												 8 + // dummy
												 PrimarySession->NetdiskPartitionInformation.VolumeName.Length,
												 PRIMARY_SESSION_BUFFERE_TAG );

		if (fileInformation == NULL) {

			ASSERT( LFS_INSUFFICIENT_RESOURCES );
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory( fileInformation, 
					   NdfsWinxpRequestHeader->SetFile.Length + 
					   2 +
					   8 +
					   8 +
					   PrimarySession->NetdiskPartitionInformation.VolumeName.Length );

		if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileBasicInformation) {

			PFILE_BASIC_INFORMATION		basicInformation = fileInformation;
				
			basicInformation->CreationTime.QuadPart   = NdfsWinxpRequestHeader->SetFile.BasicInformation.CreationTime;
			basicInformation->LastAccessTime.QuadPart = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastAccessTime;
			basicInformation->LastWriteTime.QuadPart  = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastWriteTime;
			basicInformation->ChangeTime.QuadPart     = NdfsWinxpRequestHeader->SetFile.BasicInformation.ChangeTime;
			basicInformation->FileAttributes          = NdfsWinxpRequestHeader->SetFile.BasicInformation.FileAttributes;

			length = sizeof( FILE_BASIC_INFORMATION );
			fileInformationClass = FileBasicInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileLinkInformation) {

			PFILE_LINK_INFORMATION		linkInfomation = fileInformation;
			POPEN_FILE					rootDirectoryFile;

			ASSERT(sizeof(FILE_LINK_INFORMATION) - sizeof(WCHAR) - 8 + NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);

			if (NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0) {

				rootDirectoryFile = PrimarySession_FindOpenFile( PrimarySession,
																 NdfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle );

				//ASSERT( rootDirectoryFile && rootDirectoryFile->OpenFileId == rootDirectoryFile );
				ASSERT( rootDirectoryFile->AlreadyClosed == FALSE );
			
			} else {

				rootDirectoryFile = NULL;
			}

			linkInfomation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists;
			linkInfomation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;
			linkInfomation->FileNameLength = NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength;
			
			RtlCopyMemory( linkInfomation->FileName,
						   ndfsRequestData,
						   linkInfomation->FileNameLength );
			
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("DispatchWinXpRequest: linkInfomation->FileNameLength = %u\n", linkInfomation->FileNameLength) );

			length = sizeof(FILE_LINK_INFORMATION) - sizeof(WCHAR) + linkInfomation->FileNameLength;
			fileInformationClass = FileLinkInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileRenameInformation) {

			PFILE_RENAME_INFORMATION	renameInformation = fileInformation;
			POPEN_FILE					rootDirectoryFile;
			
			ASSERT( sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) - 8 + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length );
						
			if (NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0) {

				rootDirectoryFile = PrimarySession_FindOpenFile( PrimarySession,
																 NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle );

				//ASSERT( rootDirectoryFile && rootDirectoryFile->OpenFileId == rootDirectoryFile );
				ASSERT( rootDirectoryFile->AlreadyClosed == FALSE );
			
			} else {

				rootDirectoryFile = NULL;
			}
		
			renameInformation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists;
			renameInformation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;

#if 0
			renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
			RtlCopyMemory( renameInformation->FileName,
						   ndfsRequestData,
						   renameInformation->FileNameLength );

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
						   ("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName) );
#endif
			
			//
			// Check to see whether or not a fully qualified pathname was supplied.
			// If so, then more processing is required.
			//

			if (ndfsRequestData[0] == (UCHAR) OBJ_NAME_PATH_SEPARATOR ||
				renameInformation->RootDirectory != NULL ) {

				ULONG		byteoffset;
				ULONG		idx_unicodestr;
				PWCHAR		unicodestr;
				BOOLEAN		found;

				found = FALSE;

				unicodestr = (PWCHAR)ndfsRequestData;
				
				for (idx_unicodestr = 0;
					 idx_unicodestr < ( NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength / sizeof(WCHAR) - 1 );
					 idx_unicodestr ++) {

					if (L':'  == unicodestr[idx_unicodestr] && L'\\' == unicodestr[idx_unicodestr + 1]) {

						idx_unicodestr ++;
						found = TRUE;
						break;
					}
				}

				if (found) {

					RtlCopyMemory( renameInformation->FileName,
								   PrimarySession->NetdiskPartitionInformation.VolumeName.Buffer,
								   PrimarySession->NetdiskPartitionInformation.VolumeName.Length );

					renameInformation->FileNameLength = 
						PrimarySession->NetdiskPartitionInformation.VolumeName.Length;

					byteoffset = idx_unicodestr*sizeof(WCHAR);
					
					RtlCopyMemory( (PUCHAR)renameInformation->FileName + renameInformation->FileNameLength,
								   (PUCHAR)ndfsRequestData + byteoffset,
								   NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset );

					renameInformation->FileNameLength += 
						NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset;
				
				} else {
				
					ASSERT( LFS_BUG );
				}

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
							   ("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName) );
			
			} else {

				renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
				
				RtlCopyMemory( renameInformation->FileName,
							   ndfsRequestData,
							   renameInformation->FileNameLength );
			}

			length = sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + renameInformation->FileNameLength;
			fileInformationClass = FileRenameInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileDispositionInformation) {

			PFILE_DISPOSITION_INFORMATION	dispositionInformation = fileInformation;
			
			dispositionInformation->DeleteFile = NdfsWinxpRequestHeader->SetFile.DispositionInformation.DeleteFile;
			
			length = sizeof( FILE_DISPOSITION_INFORMATION );
			fileInformationClass = FileDispositionInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileEndOfFileInformation) {
			
			PFILE_END_OF_FILE_INFORMATION fileEndOfFileInformation = fileInformation;
			
			fileEndOfFileInformation->EndOfFile.QuadPart = NdfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile;

			length = sizeof( FILE_END_OF_FILE_INFORMATION );
			fileInformationClass = FileEndOfFileInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileAllocationInformation) {

			PFILE_ALLOCATION_INFORMATION fileAllocationInformation = fileInformation;

			fileAllocationInformation->AllocationSize.QuadPart = NdfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize;
			
			length = sizeof( FILE_ALLOCATION_INFORMATION );
			fileInformationClass = FileAllocationInformation;

		} else if (FilePositionInformation == NdfsWinxpRequestHeader->SetFile.FileInformationClass) {

			PFILE_POSITION_INFORMATION filePositionInformation = fileInformation;

			filePositionInformation->CurrentByteOffset.QuadPart = NdfsWinxpRequestHeader->SetFile.PositionInformation.CurrentByteOffset;
			
			length = sizeof( FILE_POSITION_INFORMATION );
			fileInformationClass = FilePositionInformation;

		} else {

			ASSERT( LFS_BUG );
		}
	
		setInformationStatus = IoSetInformation( openFile->FileObject,
												 fileInformationClass,
												 length,
												 fileInformation );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					   ("DispatchWinXpRequest: IoSetInformation: fileHandle = %p, fileInformationClass = %X, setInformationStatus = %X\n",
						openFile->FileHandle, fileInformationClass, setInformationStatus) );

		if (NT_SUCCESS(setInformationStatus))
			ASSERT( setInformationStatus == STATUS_SUCCESS );
		
		NdfsWinxpReplytHeader->IrpTag			 = NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status	   = setInformationStatus;

		if (NT_SUCCESS(setInformationStatus))
			NdfsWinxpReplytHeader->Information = length;
		else
			NdfsWinxpReplytHeader->Information = 0;

		*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		ASSERT( fileInformation );
		
		ExFreePoolWithTag( fileInformation, PRIMARY_SESSION_BUFFERE_TAG );

		break;
	}

#endif

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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
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

		ASSERT(NdfsWinxpRequestHeader->QueryVolume.Length <= PrimarySession->Thread.PrimaryMaxDataSize);
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

		if(queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: IoQueryVolumeInformation: fileHandle = %p, queryVolumeInformationStatus = %X, length = %d, returnedLength = %d\n",
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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
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
					("DispatchWinXpRequest: ZwSetVolumeInformationFile: fileHandle = %p, volumeInformationClass = %X, setVolumeStatus = %X\n",
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

			_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
			POPEN_FILE				openFile;
		
			BOOLEAN					synchronousIo;

			IO_STATUS_BLOCK			ioStatusBlock;
		    PVOID					fileInformation;
			ULONG					length;
			FILE_INFORMATION_CLASS	fileInformationClass;
			BOOLEAN					returnSingleEntry;
			UNICODE_STRING			fileName;
			PWCHAR					fileNameBuffer;
			BOOLEAN					restartScan;
			BOOLEAN					indexSpecified;

			//
			//	Allocate a name buffer
			//
			fileNameBuffer = ExAllocatePool(NonPagedPool, NDFS_MAX_PATH);
			if(fileNameBuffer == NULL) {
				ASSERT(LFS_UNEXPECTED);
				ntStatus = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

#if 0
			openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
			openFile = PrimarySession_FindOrReopenOpenFile(
				PrimarySession,
				openFileId);
#endif
			ASSERT(openFile && openFile->FileObject);
			//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);

			ASSERT(NdfsWinxpRequestHeader->QueryDirectory.Length <= PrimarySession->Thread.PrimaryMaxDataSize);
	
			fileInformation			= (_U8 *)(NdfsWinxpReplytHeader+1);
			length					= NdfsWinxpRequestHeader->QueryDirectory.Length;
			fileInformationClass	= NdfsWinxpRequestHeader->QueryDirectory.FileInformationClass;
			returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
			restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;
			indexSpecified 			= (NdfsWinxpRequestHeader->IrpSpFlags & SL_INDEX_SPECIFIED) ? TRUE : FALSE;	

			RtlInitEmptyUnicodeString( 
					&fileName,
					fileNameBuffer,
					NDFS_MAX_PATH
					);

			queryDirectoryStatus = RtlAppendUnicodeToString(
										&fileName,
										(PWCHAR)ndfsRequestData
										);

			if(queryDirectoryStatus != STATUS_SUCCESS)
			{
				ExFreePool(fileNameBuffer);
				ASSERT(LFS_UNEXPECTED);
				ntStatus = STATUS_UNSUCCESSFUL;
				break;
			}

			RtlZeroMemory(
				&ioStatusBlock,
				sizeof(ioStatusBlock)
				);

			if (indexSpecified) {
				queryDirectoryStatus = LfsQueryDirectoryByIndex(
											openFile->FileHandle,
											fileInformationClass,
											fileInformation,
											length,
											(ULONG)NdfsWinxpRequestHeader->QueryDirectory.FileIndex,				
											&fileName,
											&ioStatusBlock,											
											returnSingleEntry
										);
			} else {
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
					  ASSERT(openFile->EventHandle !=NULL);
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

			//
			//	[64bit issue]
			//	We assume Information value of DIRECTORY_CONTROL operation will be
			//	less than 32bit.
			//

			ASSERT(ioStatusBlock.Information <= 0xffffffff);

			NdfsWinxpReplytHeader->Status	  = queryDirectoryStatus;
			NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;

			if(queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
			{
				if(ioStatusBlock.Information)
					*replyDataSize = CalculateSafeLength(
										fileInformationClass,
										length,
										(_U32)ioStatusBlock.Information,
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

			//
			//	Free the name buffer
			//

			ExFreePool(fileNameBuffer);
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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			fsControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 #if 0
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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

			ASSERT(moveFile);
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
			ASSERT(markFile);
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

		//
		//	[64bit issue]
		//	We assume Information value of FILESYSTEM_CONTROL operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status	  = fileSystemControlStatus;
		NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;

		if(outputBufferLength && NdfsWinxpReplytHeader->Information)
		{
			*replyDataSize = (_U32)ioStatusBlock.Information;
			//*replyDataSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength <= (_U32)ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength : (_U32)ioStatusBlock.Information;
		}
		else
			*replyDataSize = 0;

		if (outputBufferLength < *replyDataSize)
			DbgBreakPoint();
		
		ntStatus = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_DEVICE_CONTROL: // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	{
		NTSTATUS		deviceControlStatus;

		_U64			openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			ioControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 		
#if 0
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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
						("DispatchWinXpRequest: IRP_MJ_DEVICE_CONTROL: "
						"CtrlCode:%x openFileId = %X, deviceControlStatus = %X, "
						"ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
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

		//
		//	[64bit issue]
		//	We assume Information value of DEVICE_CONTROL operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status	  = deviceControlStatus;
		NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;

		if(ioStatusBlock.Information)
			ASSERT(outputBufferLength);

		if(outputBufferLength && NdfsWinxpReplytHeader->Information)
		{
			*replyDataSize = (_U32)ioStatusBlock.Information;
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

		_U64				openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE			openFile;
		
		IO_STATUS_BLOCK		ioStatusBlock;
		LARGE_INTEGER		byteOffset;
		LARGE_INTEGER		length;
		ULONG				key;
		BOOLEAN				failImmediately;
		BOOLEAN				exclusiveLock;
		
#if 0
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);
		//ASSERT(NdfsWinxpRequestHeader->LockControl.Length <= MAX_PRIMARY_SESSION_SEND_BUFFER);

		byteOffset.QuadPart	= NdfsWinxpRequestHeader->LockControl.ByteOffset;
		length.QuadPart		= NdfsWinxpRequestHeader->LockControl.Length;
		key					= NdfsWinxpRequestHeader->LockControl.Key;

        failImmediately		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_FAIL_IMMEDIATELY);
		exclusiveLock		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_EXCLUSIVE_LOCK);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ( "LFS: IRP_MJ_LOCK_CONTROL: Length:%I64d Key:%08lx Offset:%I64d key:%u Ime:%d Ex:%d\n",
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

			//
			//	[64bit issue]
			//	We assume Information value of LOCK_CONTROL operation will be
			//	less than 32bit.
			//

			//ASSERT(ioStatusBlock.Information <= 0xffffffff);

			NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;
		}
		else
			NdfsWinxpReplytHeader->Information = 0;

		*replyDataSize = 0;
		
		ntStatus = STATUS_SUCCESS;

		break;
	}
	case IRP_MJ_CLEANUP: // 0x12
	{
		_U64				openFileId = NdfsWinxpRequestHeader->FileHandle;
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
		ASSERT(openFile && openFile->FileObject);

#if __LFS__
		if(PrimarySession->Thread.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0 
			&& BooleanFlagOn(NdfsWinxpRequestHeader->IrpFlags, IRP_CLOSE_OPERATION))
#else
		if(PrimarySession->Thread.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1 
			&& BooleanFlagOn(NdfsWinxpRequestHeader->IrpFlags, IRP_CLOSE_OPERATION))
#endif
		{
			openFile->CleanUp = TRUE;
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
		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;
		PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					returnedLength = 0;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (_U32)openFile);
		ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length <= PrimarySession->Thread.PrimaryMaxDataSize);

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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
	    PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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
					("DispatchWinXpRequest: ZwSetSecurityObject: fileHandle = %p, securityInformation = %X, setSecurityStatus = %X\n",
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
		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
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

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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

		//
		//	[64bit issue]
		//	We assume Information value of QUERY_EA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		if( queryEaStatus == STATUS_SUCCESS ||
			queryEaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;
		
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
			NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information; //NdfsWinxpRequestHeader->QuerySecurity.Length;
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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		ULONG					length;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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

		//
		//	[64bit issue]
		//	We assume Information value of SET_EA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status	  = setEaStatus;
		NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;

		*replyDataSize = 0;

		ntStatus = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_QUOTA:
	{
		NTSTATUS				queryQuotaStatus;
		IO_STATUS_BLOCK			ioStatusBlock ;
		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		
		PVOID					outputBuffer ;
		ULONG					outputBufferLength;
		PVOID					inputBuffer ;
		ULONG					inputBufferLength ;
		PVOID					startSid ;
		BOOLEAN					restartScan ;
		BOOLEAN					returnSingleEntry ;
#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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
					("DispatchWinXpRequest: NtQueryQuotaInformationFile: fileHandle = %p, ioStatusBlock->Information = %X, queryQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, queryQuotaStatus));

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(queryQuotaStatus))
			ASSERT(queryQuotaStatus == STATUS_SUCCESS);		

		//
		//	[64bit issue]
		//	We assume Information value of QUERY_QUOTA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);


		if( queryQuotaStatus == STATUS_SUCCESS ||
			queryQuotaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;
			*replyDataSize = (_U32)ioStatusBlock.Information;

		} else if(queryQuotaStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryQuotaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information; //NdfsWinxpRequestHeader->QuerySecurity.Length;
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

		_U64					openFileId = NdfsWinxpRequestHeader->FileHandle;
		POPEN_FILE				openFile;
		ULONG					length;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
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
					("DispatchWinXpRequest: NtSetQuotaInformationFile: fileHandle = %p, ioStatusBlock.Information = %X, setQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, setQuotaStatus));

		if(NT_SUCCESS(setQuotaStatus))
			ASSERT(setQuotaStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag			= NdfsWinxpRequestHeader->IrpTag;
		NdfsWinxpReplytHeader->IrpMajorFunction = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction = NdfsWinxpRequestHeader->IrpMinorFunction;

		//
		//	[64bit issue]
		//	We assume Information value of SET_QUOTA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);


		NdfsWinxpReplytHeader->Status	  = setQuotaStatus;
		NdfsWinxpReplytHeader->Information = (_U32)ioStatusBlock.Information;

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

//	ASSERT(NdfsWinxpReplytHeader->Status != STATUS_INVALID_HANDLE); // this can be happen...
	if (NdfsWinxpReplytHeader->Status == STATUS_INVALID_HANDLE) {
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: ReplyHeader->Statu == STATUS_INVALID_HANDLE\n"));
	}
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

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
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

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("GetVolumeInformation: PrimarySession = %p ZwCreateFile volumeHandle =%p, createStatus = %X, ioStatusBlock = %X\n",
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

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
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
		PrimarySession->Thread.BytesPerFileRecordSegment	= 0;
		PrimarySession->Thread.BytesPerSector				= 0;
		PrimarySession->Thread.BytesPerCluster				= 0;
		
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

	PrimarySession->Thread.BytesPerFileRecordSegment	= ntfsVolumeDataBuffer.BytesPerFileRecordSegment;
	PrimarySession->Thread.BytesPerSector				= ntfsVolumeDataBuffer.BytesPerSector;
	PrimarySession->Thread.BytesPerCluster				= ntfsVolumeDataBuffer.BytesPerCluster;

	ZwClose(volumeHandle);
	return STATUS_SUCCESS;
}


VOID
DispatchWinXpRequestWorker(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    )
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_REPLY_HEADER			ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer; 
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader;
	
	_U32						replyDataSize;

	
	ASSERT(Mid == ndfsRequestHeader->Mid);
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: entered PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));

	ASSERT(PrimarySession->Thread.SessionSlot[Mid].State == SLOT_EXECUTING);


	replyDataSize = CaculateReplyDataLength(PrimarySession, ndfsWinxpRequestHeader);

	if(replyDataSize <= (ULONG)(PrimarySession->Thread.SecondaryMaxDataSize || sizeof(PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		if(ndfsRequestHeader->MessageSecurity == 1)
		{
			if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->Thread.RwDataSecurity == 0)
				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
			else
				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
		}
		else
			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	}
	else
	{
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength = ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + replyDataSize);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool = ExAllocatePoolWithTag(
																	NonPagedPool,
																	PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength,
																	PRIMARY_SESSION_BUFFERE_TAG
																	);		
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool);
		if(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool == NULL) {
		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("failed to allocate ExtendWinxpReplyMessagePool\n"));
			goto fail_replypoolalloc;
		}
		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader 
			= (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool;
	}
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));

	PrimarySession->Thread.SessionSlot[Mid].Status 
			= DispatchWinXpRequest(
					PrimarySession, 
					ndfsWinxpRequestHeader,
					PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader,
					ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
					&PrimarySession->Thread.SessionSlot[Mid].ReplyDataSize
					);

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: Return PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));
fail_replypoolalloc:
	PrimarySession->Thread.SessionSlot[Mid].State = SLOT_FINISH;

	KeSetEvent(&PrimarySession->Thread.WorkCompletionEvent, IO_NO_INCREMENT, FALSE);

	return;
}






NTSTATUS
ReceiveNtfsWinxpMessage(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*cryptWinxpRequestMessage;

	NTSTATUS					tdiStatus;
	//int							desResult;


	cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;	

	//
	// If the request is not split, receive the request at a time
	//	and return to the caller.
	//

	if(ndfsRequestHeader->Splitted == 0)
		{
		ASSERT(ndfsRequestHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].RequestMessageBufferLength);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	

		//
		// Receive non-encrypted request at a time and return to the caller.
		//

		if(ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsWinxpRequestHeader,
							ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER),
							NULL
							);
	
			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;
	
			return tdiStatus;
		}

		ASSERT( FALSE );
#if 0
		//
		//  Receive encrypted WinXP request header
		//	and return to the caller
		//

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
		DES_CBCInit(&PrimarySession->DesCbcContext,
					PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password,
					PrimarySession->Iv,
					DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext,
								(_U8 *)ndfsWinxpRequestHeader,
								cryptWinxpRequestMessage,
								sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);

		//
		//  Receive encrypted WinXP request data
		//

		ASSERT(ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER));

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

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;


		//
		//	return to the caller
		//

		return STATUS_SUCCESS;

#endif
	}

	ASSERT(ndfsRequestHeader->Splitted == 1);

	//
	//	Allocate memory for extended WinXP header
	//

//	if(ndfsRequestHeader->MessageSize > (PrimarySession->RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool 
			= ExAllocatePoolWithTag(
				NonPagedPool,
				PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength,
				PRIMARY_SESSION_BUFFERE_TAG
				);
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
		if(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL) {
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,	("ReceiveNtfsWinxpMessage: failed to allocate ExtendWinxpRequestMessagePool\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
	}
//	else
//		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);

	//
	//  Receive WinXP request header
	//

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

#if 0

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
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
	}

#endif

	//
	//	Receive a pair of NDFS request header and data
	//

	while(1)
	{
		PNDFS_REQUEST_HEADER	splitNdfsRequestHeader = &PrimarySession->Thread.SessionSlot[Mid].SplitNdfsRequestHeader;

		//
		//	Receive NDFS request
		//

		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)splitNdfsRequestHeader,
						sizeof(NDFS_REQUEST_HEADER),
						NULL
						);
		if(tdiStatus != STATUS_SUCCESS)
			return tdiStatus;

		if(!(
//			PrimarySession->Thread.SessionState == SESSION_SETUP &&
			ndfsRequestHeader->Uid == PrimarySession->Thread.Uid &&
			ndfsRequestHeader->Tid == PrimarySession->Thread.Tid
			))
		{
			ASSERT(LFS_BUG);
			return STATUS_UNSUCCESSFUL;
		}

		//
		// receive a part of data
		//

		if(ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->Thread.PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER)),
							NULL
								);
			if(tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
		}
#if 0
		else
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							cryptWinxpRequestMessage,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->Thread.PrimaryMaxDataSize
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
								? PrimarySession->Thread.PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER))
								);
			ASSERT(desResult == IDOK);
		}
#endif
		if(splitNdfsRequestHeader->Splitted)
			continue;

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}
}



NTSTATUS
SendNdfsWinxpMessage(
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN _U32						ReplyDataSize,
	IN _U16						Mid
	)
{
	NTSTATUS	tdiStatus;
	_U32		remaninigDataSize;		



	//
	//	If the replying data is less than max data size for the secondary,
	//	Send header and body at a time and return to the caller
	//

	if(ReplyDataSize <= PrimarySession->Thread.SecondaryMaxDataSize)
		{
		//int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;


		//
		//	Set up reply NDFS header
		//

		RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->Thread.SessionFlags;
		NdfsReplyHeader->Uid		= PrimarySession->Thread.Uid;
		NdfsReplyHeader->Tid		= PrimarySession->Thread.Tid;
		NdfsReplyHeader->Mid		= Mid;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->Thread.MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));

		ASSERT(NdfsReplyHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBufferLength);

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)NdfsReplyHeader,
						sizeof(NDFS_REPLY_HEADER),
						NULL,
						&PrimarySession->Thread.TransportCtx
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
		

		//
		//	If message security is not set,
		//	send a header and body in raw, and return to the caller.
		//

		if(PrimarySession->Thread.MessageSecurity == 0)
		{
			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)NdfsWinxpReplyHeader,
							NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL,
							&PrimarySession->Thread.TransportCtx
							);
			return tdiStatus;
		}

		ASSERT( FALSE );

#if 0

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchRequest: PrimarySession->Thread.RwDataSecurity = %d\n", PrimarySession->Thread.RwDataSecurity));

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->Thread.RwDataSecurity == 0)
			{
			RtlCopyMemory(cryptWinxpRequestMessage, NdfsWinxpReplyHeader, sizeof(NDFS_WINXP_REPLY_HEADER));
			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)NdfsWinxpReplyHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)NdfsWinxpReplyHeader,
							NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL,
							&PrimarySession->Thread.TransportCtx
							);
			}
			else
			{
			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, cryptWinxpRequestMessage, (_U8 *)NdfsWinxpReplyHeader, NdfsReplyHeader->MessageSize-sizeof(NDFS_REPLY_HEADER));
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptWinxpRequestMessage,
							NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL,
							&PrimarySession->Thread.TransportCtx
							);
		}

		//
		//	Return to the caller
		//

		return tdiStatus;

#endif

	}


	ASSERT((_U8 *)NdfsWinxpReplyHeader == PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool);
	ASSERT(ReplyDataSize > PrimarySession->Thread.SecondaryMaxDataSize);

	RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
	NdfsReplyHeader->Status		= NDFS_SUCCESS;
	NdfsReplyHeader->Flags	    = PrimarySession->Thread.SessionFlags;
	NdfsReplyHeader->Splitted	= 1;	// indicate the split.
	NdfsReplyHeader->Uid		= PrimarySession->Thread.Uid;
	NdfsReplyHeader->Tid		= PrimarySession->Thread.Tid;
	NdfsReplyHeader->Mid		= 0;
	NdfsReplyHeader->MessageSize 
			= sizeof(NDFS_REPLY_HEADER) 
			+ (PrimarySession->Thread.MessageSecurity ? 
					ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) :
					(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));
	//
	//	Send reply NDFS header
	//
	tdiStatus = SendMessage(
				PrimarySession->ConnectionFileObject,
				(_U8 *)NdfsReplyHeader,
				sizeof(NDFS_REPLY_HEADER),
				NULL,
				&PrimarySession->Thread.TransportCtx
				);

	if(tdiStatus != STATUS_SUCCESS)
	{
		return tdiStatus;
	} 

	//
	//	Send reply WinXp header
	//

#if 0

	if(PrimarySession->Thread.MessageSecurity)
	{
		int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		DES_CBCInit(&PrimarySession->DesCbcContext,
					PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, 
					PrimarySession->Iv, DES_ENCRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext,
								cryptWinxpRequestMessage,
								(_U8 *)NdfsWinxpReplyHeader,
								sizeof(NDFS_WINXP_REPLY_HEADER));
		ASSERT(desResult == IDOK);
	
		tdiStatus = SendMessage(
					PrimarySession->ConnectionFileObject,
					cryptWinxpRequestMessage,
					sizeof(NDFS_WINXP_REPLY_HEADER),
					NULL,
					&PrimarySession->Thread.TransportCtx
					);
	}
	else
#endif
	{
		tdiStatus = SendMessage(
					PrimarySession->ConnectionFileObject,
					(_U8 *)NdfsWinxpReplyHeader,
					sizeof(NDFS_WINXP_REPLY_HEADER),
					NULL,
					&PrimarySession->Thread.TransportCtx
					);
	} 

	if (tdiStatus != STATUS_SUCCESS) {
	
		return tdiStatus;
	}


	//
	//	Send data body
	//

	remaninigDataSize = ReplyDataSize;

	while(1)
	{

		//
		//	Set up reply NDFS header
		//

		RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->Thread.SessionFlags;
		NdfsReplyHeader->Uid		= PrimarySession->Thread.Uid;
		NdfsReplyHeader->Tid		= PrimarySession->Thread.Tid;
		NdfsReplyHeader->Mid		= 0;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
					+ (PrimarySession->Thread.MessageSecurity ?
					ADD_ALIGN8(remaninigDataSize) : remaninigDataSize);

		if(remaninigDataSize > PrimarySession->Thread.SecondaryMaxDataSize)
			NdfsReplyHeader->Splitted = 1;
		else
			NdfsReplyHeader->Splitted = 0;

		//
		//	Send NDFS reply header
		//

		tdiStatus = SendMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)NdfsReplyHeader,
						sizeof(NDFS_REPLY_HEADER),
						NULL,
						&PrimarySession->Thread.TransportCtx
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
		//
		//	Send a part of data body
		//

#if 0

		if(PrimarySession->Thread.MessageSecurity)
		{
			int desResult;
			_U8 *cryptNdfsWinxpReplyHeader = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							cryptNdfsWinxpReplyHeader, 
							(_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ?
								PrimarySession->Thread.SecondaryMaxDataSize :
								(NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER))
							);
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptNdfsWinxpReplyHeader,
							NdfsReplyHeader->Splitted ?
								PrimarySession->Thread.SecondaryMaxDataSize :
								(NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL,
							&PrimarySession->Thread.TransportCtx
							);
		}
		else
#endif
		{	
			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ? 
								PrimarySession->Thread.SecondaryMaxDataSize :
								(NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL,
							&PrimarySession->Thread.TransportCtx
							);
		}

		if(tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}

		//
		//	Decrease remaining bytes
		//

		if(NdfsReplyHeader->Splitted)
			remaninigDataSize -= PrimarySession->Thread.SecondaryMaxDataSize;
		else
			return STATUS_SUCCESS;


		ASSERT((_S32)remaninigDataSize > 0);
	}
}





#if 0

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
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest->
	IN  BOOLEAN						FastMutexAcquired
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

#endif

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

	openFile->CurrentByteOffset.HighPart = 0;
	openFile->CurrentByteOffset.LowPart = 0;
	
    RtlInitEmptyUnicodeString(
				&openFile->FullFileName,
                openFile->FullFileNameBuffer,
				sizeof(openFile->FullFileNameBuffer)
				);

	RtlCopyUnicodeString(
		&openFile->FullFileName,
		FullFileName
		);

#if defined(_WIN64)
			openFile->OpenFileId = (_U64)openFile;
			ASSERT( openFile->OpenFileId == (_U64)openFile );
#else
			openFile->OpenFileId = (_U32)openFile;
			ASSERT( openFile->OpenFileId == (_U32)openFile );
#endif
	
	InitializeListHead(&openFile->ListEntry);
	
	ExInterlockedInsertHeadList(
			&PrimarySession->Thread.OpenedFileQueue,
			&openFile->ListEntry,
			&PrimarySession->Thread.OpenedFileQSpinLock
			);

#if DBG
	InterlockedIncrement(&LfsObjectCounts.OpenFileCount);
#endif

	SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
		("PrimarySession_AllocateOpenFile OpenFile = %p, OpenFileCount = %d\n", openFile, LfsObjectCounts.OpenFileCount));

	return openFile;
}

#if 0 //def __LFS__
POPEN_FILE
PrimarySession_FindOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U64			 OpenFileId
	)
#else
POPEN_FILE
PrimarySession_FindOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U64			 OpenFileId
	)
#endif
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;

	openFile = NULL;

	ExAcquireFastMutex( &PrimarySession->FastMutex );
    
	for (listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
         listEntry = listEntry->Flink) 
	{

		openFile = CONTAINING_RECORD (listEntry, OPEN_FILE, ListEntry);

		if(openFile->OpenFileId == OpenFileId)
			break;

		openFile = NULL;
	}

	ExReleaseFastMutex( &PrimarySession->FastMutex );

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

	// Could be problem if same file is opened multiple times and with same access right by secondary.
	//   And file is IOed with relative position.

	ExAcquireFastMutex( &PrimarySession->FastMutex );

    for (openFile = NULL, listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
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

	ExReleaseFastMutex( &PrimarySession->FastMutex );

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


	ExAcquireFastMutex( &PrimarySession->FastMutex );

    for (openFile = NULL, listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
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

	ExReleaseFastMutex( &PrimarySession->FastMutex );

	return openFile;
}


//
// Find open file including AlreadyClosed file.
// If file is already closed, reopen it.
//
POPEN_FILE
PrimarySession_FindOrReopenOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U64			 OpenFileId
	)
{	
	POPEN_FILE				openFile;

	HANDLE					fileHandle = NULL;	
	PFILE_OBJECT				fileObject;	
	NTSTATUS			createStatus;
	POPEN_FILE			siblingOpenFile;
	
#if __LFS__
	ASSERT( OpenFileId );
#endif

	openFile = PrimarySession_FindOpenFile(
						PrimarySession,
						OpenFileId
						);
	ASSERT(openFile);

	if(openFile->AlreadyClosed == TRUE)
	{
#if __LFS__
		//ASSERT( FALSE );
#endif

		while(1) {
			fileHandle = PrimarySessionOpenFile(PrimarySession, openFile);
			
			if(fileHandle == NULL)
			{
				SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
					("PrimarySession_FindOrReopenOpenFile: Reopening failed\n"));	
				
				//
				// Find all files among cleaned up but not closed files that may be not closed yet.
				//	before try to open them.
				while(1) {
					siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed(
												PrimarySession,
												&openFile->FullFileName,
												FALSE
												);
					if(siblingOpenFile) 
					{
						NTSTATUS	closeStatus;

						SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
							("PrimarySession_FindOrReopenOpenFile: Uncleaned file found. Trying to close it and reopen\n"));	

						ASSERT(siblingOpenFile->FileObject);
						ObDereferenceObject(siblingOpenFile->FileObject);
						siblingOpenFile->FileObject = NULL;
						closeStatus = ZwClose(siblingOpenFile->FileHandle);
						ASSERT(closeStatus == STATUS_SUCCESS);

						ASSERT(openFile->EventHandle !=NULL);
						closeStatus = ZwClose(siblingOpenFile->EventHandle);
						ASSERT(closeStatus == STATUS_SUCCESS);
						openFile->EventHandle = NULL;
						siblingOpenFile->AlreadyClosed = TRUE;
						continue; // find more files
					}
					else
						break;
				}
				continue; // try to open file again.
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

			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
				("PrimarySession_FindOrReopenOpenFile Reopened file = %p\n", openFile));	
							
			openFile->FileHandle = fileHandle;
			openFile->FileObject = fileObject;
			openFile->AlreadyClosed = FALSE;
			break;
		}
	}
	return openFile;
}


VOID
PrimarySession_FreeOpenFile(
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	)
{
	UNREFERENCED_PARAMETER( PrimarySession );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("PrimarySession_FreeOpenFile OpenFile = %p, OpenFileCount = %d\n", 
				   OpenedFile, LfsObjectCounts.OpenFileCount) );
	
	ASSERT( OpenedFile->ListEntry.Flink == OpenedFile->ListEntry.Blink );
	ASSERT( OpenedFile );

	OpenedFile->FileHandle = 0;
	OpenedFile->PrimarySession = NULL;

	ExFreePoolWithTag( OpenedFile, OPEN_FILE_TAG );
}

