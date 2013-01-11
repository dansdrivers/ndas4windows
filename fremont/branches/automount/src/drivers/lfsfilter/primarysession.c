#include "LfsProc.h"




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
PrimarySession_FindOpenFileByName(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  ACCESS_MASK			DesiredAccess
	);



_U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	);


PPRIMARY_SESSION
PrimarySession_Create (
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

	try {

		primarySession = FsRtlAllocatePoolWithTag( NonPagedPool, sizeof(PRIMARY_SESSION), LFS_ALLOC_TAG );
	
	} except (1) {
		
		ASSERT( FALSE );
		GetExceptionCode();
	}
	
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
		
		ExInitializeFastMutex( &primarySession->FastMutex );
		
		InitializeListHead( &primarySession->ListEntry );

		primarySession->ConnectionFileHandle	= ListenFileHandle;
		primarySession->ConnectionFileObject	= ListenFileObject;
		RtlCopyMemory( &primarySession->RemoteAddress, RemoteAddress, sizeof(LPX_ADDRESS) );
		primarySession->IsLocalAddress			= Lfs_IsLocalAddress(RemoteAddress);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("primarySession->ConnectionFileHandle = %p\n", primarySession->ConnectionFileHandle) );

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
		primarySession->SessionContext.SessionKey = (_U32)PtrToUlong(primarySession);

		primarySession->SessionContext.PrimaryMaxDataSize = (LfsRegistry.MaxDataTransferPri < DEFAULT_MAX_DATA_SIZE)
													? LfsRegistry.MaxDataTransferPri:DEFAULT_MAX_DATA_SIZE;

		primarySession->SessionContext.SecondaryMaxDataSize = (LfsRegistry.MaxDataTransferSec < DEFAULT_MAX_DATA_SIZE)
													? LfsRegistry.MaxDataTransferSec:DEFAULT_MAX_DATA_SIZE;

		//
		//	Initialize transport context for traffic control
		//

		InitTransCtx( &primarySession->Thread.TransportCtx, primarySession->SessionContext.SecondaryMaxDataSize );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_Create: PriMaxData:%08u SecMaxData:%08u\n",
												  primarySession->SessionContext.PrimaryMaxDataSize,
												  primarySession->SessionContext.SecondaryMaxDataSize) );

		primarySession->SessionContext.Uid = (_U16)primarySession;

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
PrimarySession_Close (
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
PrimarySession_FileSystemShutdown (
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
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_SHUTDOWN;

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
PrimarySession_Stopping (
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

	if (FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP)) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_STOPPING;
	primarySessionRequest->ExecuteStatus = STATUS_PENDING;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest, TRUE );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );

	timeOut.QuadPart = -LFS_QUERY_REMOVE_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	if (ntStatus != STATUS_SUCCESS) {

		ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );

		if (primarySessionRequest->ExecuteStatus == STATUS_PENDING) {

			primarySessionRequest->Synchronous = FALSE;
		
		} else {

			ntStatus = STATUS_SUCCESS;
		}

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );	
	}
	
	if (ntStatus == STATUS_SUCCESS) {
		
		KeClearEvent( &primarySessionRequest->CompleteEvent );
		DereferencePrimarySessionRequest( primarySessionRequest );
	}

	if (ntStatus == STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySession_CloseFiles: thread close files\n") );

		ASSERT( FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING) || FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_STOPED) );
	
	} else {

		ASSERT( ntStatus == STATUS_TIMEOUT );
	}

	PrimarySession_Dereference( PrimarySession );
	return;
}


VOID
PrimarySession_Disconnect (
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
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DISCONNECT_AND_TERMINATE;

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
PrimarySession_CancelStopping (
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
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_CANCEL_STOPPING;

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
AllocPrimarySessionRequest (
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
DereferencePrimarySessionRequest (
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


#ifndef NTDDI_VERSION

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
DispatchWinXpRequestWorker (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    )
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_REPLY_HEADER			ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer; 
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader;
	
	_U32						replyDataSize;

	
	NDASFS_ASSERT( Mid == ndfsRequestHeader->Mid );
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: entered PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command) );

	NDASFS_ASSERT( PrimarySession->Thread.SessionSlot[Mid].State == SLOT_EXECUTING );

	replyDataSize = CaculateReplyDataLength(PrimarySession, ndfsWinxpRequestHeader);

	if (replyDataSize <= 
		(ULONG)(PrimarySession->SessionContext.SecondaryMaxDataSize || sizeof(PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER))) {

		if (ndfsRequestHeader->MessageSecurity == 1) {

			if (ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->SessionContext.RwDataSecurity == 0) {

				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
			
			} else {

				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
			}
		
		} else {

			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
		}
	
	} else {

		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength = ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + replyDataSize);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool 
			= ExAllocatePoolWithTag( NonPagedPool,
									 PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength,
									 PRIMARY_SESSION_BUFFERE_TAG );

		NDASFS_ASSERT( PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool );
		
		if (PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool == NULL) {
		   
			PrimarySession->Thread.SessionSlot[Mid].Status = STATUS_INSUFFICIENT_RESOURCES; 
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR, ("failed to allocate ExtendWinxpReplyMessagePool\n") );

			goto fail_replypoolalloc;
		}

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader 
			= (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool;
	}
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("DispatchWinXpRequestWorker: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					 PrimarySession, ndfsRequestHeader->Command) );

	PrimarySession->Thread.SessionSlot[Mid].Status 
		= DispatchWinXpRequest( PrimarySession, 
								ndfsWinxpRequestHeader,
								PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader,
								ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
								&PrimarySession->Thread.SessionSlot[Mid].ReplyDataSize );

	ASSERT( PrimarySession->Thread.SessionSlot[Mid].Status == STATUS_SUCCESS );

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("DispatchWinXpRequestWorker: Return PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					 PrimarySession, ndfsRequestHeader->Command) );

fail_replypoolalloc:

	PrimarySession->Thread.SessionSlot[Mid].State = SLOT_FINISH;

	KeSetEvent( &PrimarySession->Thread.WorkCompletionEvent, IO_NO_INCREMENT, FALSE );

	return;
}


NTSTATUS
ReceiveNtfsWinxpMessage (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*cryptWinxpRequestMessage;

	NTSTATUS					tdiStatus;
	//int						desResult;


	cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;	

	//
	// If the request is not split, receive the request at a time
	//	and return to the caller.
	//

	if (ndfsRequestHeader->Splitted == 0) {

		ASSERT( ndfsRequestHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].RequestMessageBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	

		//
		// Receive non-encrypted request at a time and return to the caller.
		//

		if (ndfsRequestHeader->MessageSecurity == 0) {

			tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)ndfsWinxpRequestHeader,
									 ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER),
									 NULL );
	
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

	ASSERT( ndfsRequestHeader->Splitted == 1 );

	//
	//	Allocate memory for extended WinXP header
	//

//	if(ndfsRequestHeader->MessageSize > (PrimarySession->RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		ASSERT( PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL );
		
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength = 
			ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool = 
			ExAllocatePoolWithTag( NonPagedPool,
								   PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength,
								   PRIMARY_SESSION_BUFFERE_TAG );

		ASSERT( PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool );

		if (PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL) {
		
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,	("ReceiveNtfsWinxpMessage: failed to allocate ExtendWinxpRequestMessagePool\n") );
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
	}
//	else
//		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);

	//
	//  Receive WinXP request header
	//

	if (ndfsRequestHeader->MessageSecurity == 0) {

		tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)ndfsWinxpRequestHeader,
								 sizeof(NDFS_WINXP_REQUEST_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

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

	while (1) {

		PNDFS_REQUEST_HEADER	splitNdfsRequestHeader = &PrimarySession->Thread.SessionSlot[Mid].SplitNdfsRequestHeader;

		//
		//	Receive NDFS request
		//

		tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)splitNdfsRequestHeader,
								 sizeof(NDFS_REQUEST_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS)
			return tdiStatus;

		if (!(ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid &&
			  ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid)) {

			ASSERT( LFS_BUG );
			return STATUS_UNSUCCESSFUL;
		}

		//
		// receive a part of data
		//

		if (ndfsRequestHeader->MessageSecurity == 0) {

			tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize,
									 splitNdfsRequestHeader->Splitted ? 
										PrimarySession->SessionContext.PrimaryMaxDataSize : (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER)),
									NULL );

			if (tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
		}
#if 0
		else
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							cryptWinxpRequestMessage,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->SessionContext.PrimaryMaxDataSize
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
								? PrimarySession->SessionContext.PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER))
								);
			ASSERT(desResult == IDOK);
		}
#endif
		if (splitNdfsRequestHeader->Splitted)
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

	if (ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize) {

		//int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

		//
		//	Set up reply NDFS header
		//

		RtlCopyMemory( NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol) );

		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		NdfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
		NdfsReplyHeader->Tid		= PrimarySession->SessionContext.Tid;
		NdfsReplyHeader->Mid		= Mid;
		NdfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER) + 
									   (PrimarySession->SessionContext.MessageSecurity ? 
									    ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));

		ASSERT( NdfsReplyHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBufferLength );

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)NdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL,
								 &PrimarySession->Thread.TransportCtx );

		if (tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}
		

		//
		//	If message security is not set,
		//	send a header and body in raw, and return to the caller.
		//

		if (PrimarySession->SessionContext.MessageSecurity == 0) {

			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)NdfsWinxpReplyHeader,
									 NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
									 NULL,
									 &PrimarySession->Thread.TransportCtx );

			return tdiStatus;
		}

		ASSERT( FALSE );

#if 0

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchRequest: PrimarySession->SessionContext.RwDataSecurity = %d\n", PrimarySession->SessionContext.RwDataSecurity));

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->SessionContext.RwDataSecurity == 0)
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


	ASSERT( (_U8 *)NdfsWinxpReplyHeader == PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool );
	ASSERT( ReplyDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize );

	RtlCopyMemory( NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol) );
	NdfsReplyHeader->Status		= NDFS_SUCCESS;
	NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
	NdfsReplyHeader->Splitted	= 1;	// indicate the split.
	NdfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
	NdfsReplyHeader->Tid		= PrimarySession->SessionContext.Tid;
	NdfsReplyHeader->Mid		= 0;
	NdfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER) + 
								   (PrimarySession->SessionContext.MessageSecurity ? 
									ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) );

	//
	//	Send reply NDFS header
	//
	tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)NdfsReplyHeader,
							 sizeof(NDFS_REPLY_HEADER),
							 NULL,
							 &PrimarySession->Thread.TransportCtx );

	if (tdiStatus != STATUS_SUCCESS) {

		return tdiStatus;
	} 

	//
	//	Send reply WinXp header
	//

#if 0

	if(PrimarySession->SessionContext.MessageSecurity)
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
		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)NdfsWinxpReplyHeader,
								 sizeof(NDFS_WINXP_REPLY_HEADER),
								 NULL,
								 &PrimarySession->Thread.TransportCtx );
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
		NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		NdfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
		NdfsReplyHeader->Tid		= PrimarySession->SessionContext.Tid;
		NdfsReplyHeader->Mid		= 0;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
					+ (PrimarySession->SessionContext.MessageSecurity ?
					ADD_ALIGN8(remaninigDataSize) : remaninigDataSize);

		if(remaninigDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize)
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

		if(PrimarySession->SessionContext.MessageSecurity)
		{
			int desResult;
			_U8 *cryptNdfsWinxpReplyHeader = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							cryptNdfsWinxpReplyHeader, 
							(_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ?
								PrimarySession->SessionContext.SecondaryMaxDataSize :
								(NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER))
							);
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptNdfsWinxpReplyHeader,
							NdfsReplyHeader->Splitted ?
								PrimarySession->SessionContext.SecondaryMaxDataSize :
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
								PrimarySession->SessionContext.SecondaryMaxDataSize :
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
			remaninigDataSize -= PrimarySession->SessionContext.SecondaryMaxDataSize;
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
