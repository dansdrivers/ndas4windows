#include "LfsProc.h"

#define MAX_NTFS_METADATA_FILE 11

UNICODE_STRING NtfsMetadataFileNames[] = {

	{ 10, 10, L"\\$Mft" },
	{ 18, 18, L"\\$MftMirr" },
	{ 18, 18, L"\\$LogFile" },
	{ 16, 16, L"\\$Volume" },
	{ 18, 18, L"\\$AttrDef" },
	{ 12, 12, L"\\$Root" },
	{ 16, 16, L"\\$Bitmap" },
	{ 12, 12, L"\\$Boot" },
	{ 18, 18, L"\\$BadClus" },
	{ 16, 16, L"\\$Secure" },
	{ 16, 16, L"\\$UpCase" },
	{ 16, 16, L"\\$Extend" }
};

BOOLEAN	IsMetaFile(PUNICODE_STRING	fileName) {

	LONG	idx_metafile;

	for (idx_metafile = 0; idx_metafile < MAX_NTFS_METADATA_FILE; idx_metafile ++) {
		
		if (RtlCompareUnicodeString( NtfsMetadataFileNames + idx_metafile,fileName, TRUE) == 0) {
			
			return TRUE;
		}
	}

	return FALSE;
}


VOID
ReadonlyThreadProc(
	IN	PREADONLY	Readonly
	);

VOID
Readonly_Reference (
	IN  PREADONLY	Readonly
	);

VOID
Readonly_Dereference (
	IN  PREADONLY	Readonly
	);

PREADONLY_REQUEST
AllocReadonlyRequest (
	IN	PREADONLY	Readonly,
	IN 	UINT32		MessageSize,
	IN	BOOLEAN		Synchronous
	); 

VOID
ReferenceReadonlyRequest (
	IN	PREADONLY_REQUEST	ReadonlyRequest
	); 

VOID
DereferenceReadonlyRequest(
	IN  PREADONLY_REQUEST	ReadonlyRequest
	);

FORCEINLINE
NTSTATUS
QueueingReadonlyRequest (
	IN	PREADONLY			Readonly,
	IN	PREADONLY_REQUEST	ReadonlyRequest
	);

NTSTATUS
ReadonlyDismountVolume (
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	);

NTSTATUS
ReadonlyDismountVolumeStart (
	IN  PREADONLY		Readonly
	);

VOID
ReadonlyDismountVolumeThreadProc (
	IN	PREADONLY	Readonly
	);

NTSTATUS
ReadonlyPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	);

BOOLEAN
ReadonlyIoctl (
	IN	PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp,
	OUT PNTSTATUS					NtStatus
	);

VOID
ReadonlyTryCloseCcb (
	IN	PFILESPY_DEVICE_EXTENSION	DevExt
	);


PREADONLY
Readonly_Create (
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	NTSTATUS				status;
	PREADONLY				readonly;

	OBJECT_ATTRIBUTES		objectAttributes;
	LARGE_INTEGER			timeOut;
	

	readonly = ExAllocatePoolWithTag( NonPagedPool, sizeof(READONLY), LFS_ALLOC_TAG );
	
	if (readonly == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}
	
	RtlZeroMemory( readonly, sizeof(READONLY) );

	readonly->Flags = READONLY_FLAG_INITIALIZING;

#if 0
	ExInitializeResourceLite( &readonly->RecoveryResource );
	ExInitializeResourceLite( &readonly->Resource );
	ExInitializeResourceLite( &readonly->SessionResource );
	ExInitializeResourceLite( &readonly->CreateResource );
#endif

	ExInitializeFastMutex( &readonly->FastMutex );

	readonly->ReferenceCount = 1;

	LfsDeviceExt_Reference( LfsDeviceExt );
	readonly->LfsDeviceExt = LfsDeviceExt;

	readonly->ThreadHandle = NULL;

	InitializeListHead( &readonly->FcbQueue );
	KeInitializeSpinLock( &readonly->FcbQSpinLock );

	InitializeListHead( &readonly->CcbQueue );
    ExInitializeFastMutex( &readonly->CcbQMutex );

#if 0
	InitializeListHead( &readonly->RecoveryCcbQueue );
    ExInitializeFastMutex( &readonly->RecoveryCcbQMutex );

	InitializeListHead( &readonly->DeletedFcbQueue );
#endif

	KeQuerySystemTime( &readonly->TryCloseTime );

#if 0
	readonly->TryCloseWorkItem = IoAllocateWorkItem( (PDEVICE_OBJECT)VolDo );
#endif

	KeInitializeEvent( &readonly->ReadyEvent, NotificationEvent, FALSE );
    
	InitializeListHead( &readonly->RequestQueue );
	KeInitializeSpinLock( &readonly->RequestQSpinLock );
	KeInitializeEvent( &readonly->RequestEvent, NotificationEvent, FALSE );

#if 0
	////////////////////////////////////////
	InitializeListHead( &readonly->FcbQueue );
	ExInitializeFastMutex( &readonly->FcbQMutex );
	/////////////////////////////////////////
#endif

	KeInitializeEvent( &readonly->DiskmountReadyEvent, NotificationEvent, FALSE );

	InitializeListHead( &readonly->DirNotifyList );
	FsRtlNotifyInitializeSync( &readonly->NotifySync );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	readonly->SessionId = 0;
	
	status = PsCreateSystemThread( &readonly->ThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   ReadonlyThreadProc,
								   readonly );

	if (!NT_SUCCESS(status)) {

		ASSERT( LFS_UNEXPECTED );
		Readonly_Close( readonly );
		
		return NULL;
	}

	status = ObReferenceObjectByHandle( readonly->ThreadHandle,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&readonly->ThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		Readonly_Close( readonly );
		
		return NULL;
	}

	readonly->SessionId ++;

	timeOut.QuadPart = -LFS_TIME_OUT;		
	status = KeWaitForSingleObject( &readonly->ReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		LfsDbgBreakPoint();
		Readonly_Close( readonly );
		
		return NULL;
	}

	KeClearEvent( &readonly->ReadyEvent );

	ExAcquireFastMutex( &readonly->FastMutex );

	if (!FlagOn(readonly->Thread.Flags, READONLY_THREAD_FLAG_START) ||
		FlagOn(readonly->Thread.Flags, READONLY_THREAD_FLAG_STOPED)) {

		if (readonly->Thread.SessionStatus != STATUS_DISK_CORRUPT_ERROR &&
			readonly->Thread.SessionStatus != STATUS_UNRECOGNIZED_VOLUME) {
	
			ExReleaseFastMutex( &readonly->FastMutex );

			Readonly_Close( readonly );
			return NULL;
		}
	} 

	ExReleaseFastMutex( &readonly->FastMutex );

	ClearFlag( readonly->Flags, READONLY_FLAG_INITIALIZING );
	SetFlag( readonly->Flags, READONLY_FLAG_START );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
				("Readonly_Create: The client thread are ready readonly = %p\n", readonly) );

	return readonly;
}


VOID
Readonly_Close (
	IN  PREADONLY	Readonly
	)
{
	NTSTATUS		status;
	LARGE_INTEGER	timeOut;

	PLIST_ENTRY			readonlyRequestEntry;
	PREADONLY_REQUEST	readonlyRequest;


	SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO, ("Readonly close Readonly = %p\n", Readonly) );

	ExAcquireFastMutex( &Readonly->FastMutex );

	ASSERT( !FlagOn(Readonly->Flags, READONLY_FLAG_RECONNECTING) );

	if (FlagOn(Readonly->Flags, READONLY_FLAG_CLOSED)) {

		//ASSERT( FALSE );
		ExReleaseFastMutex( &Readonly->FastMutex );
		return;
	}

	SetFlag( Readonly->Flags, READONLY_FLAG_CLOSED );

	ExReleaseFastMutex( &Readonly->FastMutex );

	FsRtlNotifyUninitializeSync( &Readonly->NotifySync );

	if (Readonly->ThreadHandle == NULL) {

		//ASSERT( FALSE );
		Readonly_Dereference( Readonly );
		return;
	}

	ASSERT( Readonly->ThreadObject != NULL );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("Readonly close READONLY_REQ_DISCONNECT Readonly = %p\n", Readonly) );

	readonlyRequest = AllocReadonlyRequest( Readonly, 0, FALSE );
	readonlyRequest->RequestType = READONLY_REQ_DISCONNECT;

	QueueingReadonlyRequest( Readonly, readonlyRequest );

	readonlyRequest = AllocReadonlyRequest( Readonly, 0, FALSE );
	readonlyRequest->RequestType = READONLY_REQ_DOWN;

	QueueingReadonlyRequest( Readonly, readonlyRequest );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("Readonly close READONLY_REQ_DISCONNECT end Readonly = %p\n", Readonly) );

	timeOut.QuadPart = -LFS_TIME_OUT;

	status = KeWaitForSingleObject( Readonly->ThreadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status == STATUS_SUCCESS) {
	   
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("Readonly_Close: thread stoped Readonly = %p\n", Readonly));

		ObDereferenceObject( Readonly->ThreadObject );

		Readonly->ThreadHandle = NULL;
		Readonly->ThreadObject = NULL;
	
	} else {

		ASSERT( LFS_BUG );
		return;
	}

	if (!IsListEmpty(&Readonly->FcbQueue))
		LfsDbgBreakPoint();

	if (!IsListEmpty(&Readonly->CcbQueue))
		LfsDbgBreakPoint();

	if (!IsListEmpty(&Readonly->RequestQueue))
		LfsDbgBreakPoint();

	while (readonlyRequestEntry = ExInterlockedRemoveHeadList(&Readonly->RequestQueue,
															   &Readonly->RequestQSpinLock)) {

		PREADONLY_REQUEST readonlyRequest2;

		InitializeListHead( readonlyRequestEntry );
			
		readonlyRequest2 = CONTAINING_RECORD( readonlyRequestEntry,
											   READONLY_REQUEST,
											   ListEntry );
        
		readonlyRequest2->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if (readonlyRequest2->Synchronous == TRUE)
			KeSetEvent( &readonlyRequest2->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceReadonlyRequest( readonlyRequest2 );
	}
	
	Readonly_Dereference( Readonly );

	return;
}


VOID
Readonly_Reference (
	IN  PREADONLY	Readonly
	)
{
    LONG result;
	
    result = InterlockedIncrement ( &Readonly->ReferenceCount );

    ASSERT (result >= 0);
}


VOID
Readonly_Dereference (
	IN  PREADONLY	Readonly
	)
{
    LONG result;
	
    result = InterlockedDecrement( &Readonly->ReferenceCount) ;
    ASSERT (result >= 0);

    if (result == 0) {

		PLFS_DEVICE_EXTENSION lfsDeviceExt = Readonly->LfsDeviceExt;

		ExFreePoolWithTag( Readonly, LFS_ALLOC_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("Readonly_Dereference: Readonly is Freed Readonly  = %p\n", Readonly) );
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO, ("Readonly_Dereference: LfsDeviceExt->Reference = %d, lfsDeviceExt= %p\n", 
			                                     lfsDeviceExt->ReferenceCount, lfsDeviceExt) );

		LfsDeviceExt_Dereference( lfsDeviceExt );
	}
}


PREADONLY_REQUEST
AllocReadonlyRequest (
	IN	PREADONLY	Readonly,
	IN 	UINT32		MessageSize,
	IN	BOOLEAN		Synchronous
	) 
{
	PREADONLY_REQUEST	readonlyRequest;
	ULONG				allocationSize;
	
	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	allocationSize = FIELD_OFFSET(READONLY_REQUEST, NdfsMessage) + MessageSize;

	readonlyRequest = ExAllocatePoolWithTag( NonPagedPool,
											 allocationSize,
											 READONLY_MESSAGE_TAG );

	if (readonlyRequest == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( readonlyRequest, allocationSize );

	readonlyRequest->ReferenceCount = 1;
	InitializeListHead( &readonlyRequest->ListEntry );

	readonlyRequest->Synchronous = Synchronous;
	KeInitializeEvent( &readonlyRequest->CompleteEvent, NotificationEvent, FALSE );

	ExAcquireFastMutex( &Readonly->FastMutex );
	readonlyRequest->SessionId = Readonly->SessionId;
	ExReleaseFastMutex( &Readonly->FastMutex );

	readonlyRequest->NdfsMessageAllocationSize = MessageSize;

	return readonlyRequest;
}


VOID
ReferenceReadonlyRequest(
	IN	PREADONLY_REQUEST	ReadonlyRequest
	) 
{
	LONG	result;

	result = InterlockedIncrement( &ReadonlyRequest->ReferenceCount );

	ASSERT( result > 0 );
}


VOID
DereferenceReadonlyRequest(
	IN  PREADONLY_REQUEST	ReadonlyRequest
	)
{
	LONG	result;


	result = InterlockedDecrement(&ReadonlyRequest->ReferenceCount);

	ASSERT( result >= 0 );

	if (0 == result) {

		ASSERT( ReadonlyRequest->ListEntry.Flink == ReadonlyRequest->ListEntry.Blink );
		ExFreePool( ReadonlyRequest );
	}
}


FORCEINLINE
NTSTATUS
QueueingReadonlyRequest(
	IN	PREADONLY			Readonly,
	IN	PREADONLY_REQUEST	ReadonlyRequest
	)
{
	NTSTATUS	status;


	ASSERT( ReadonlyRequest->ListEntry.Flink == ReadonlyRequest->ListEntry.Blink );

	ExAcquireFastMutex( &Readonly->FastMutex );

	if (FlagOn(Readonly->Thread.Flags, READONLY_THREAD_FLAG_START) &&
		!FlagOn(Readonly->Thread.Flags, READONLY_THREAD_FLAG_STOPED)) {

		ExInterlockedInsertTailList( &Readonly->RequestQueue,
									 &ReadonlyRequest->ListEntry,
									 &Readonly->RequestQSpinLock );

		KeSetEvent( &Readonly->RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		status = STATUS_UNSUCCESSFUL;
	}

	ExReleaseFastMutex( &Readonly->FastMutex );

	if (status == STATUS_UNSUCCESSFUL) {
	
		ReadonlyRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;

		if (ReadonlyRequest->Synchronous == TRUE)
			KeSetEvent( &ReadonlyRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceReadonlyRequest( ReadonlyRequest );
	}

	return status;
}

VOID
ReadonlyThreadProc (
	IN	PREADONLY	Readonly
	)
{
	BOOLEAN		readonlyThreadTerminate = FALSE;
	PLIST_ENTRY	readonlyRequestEntry;

	
	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("ReadonlyThreadProc: Start Readonly = %p\n", Readonly) );
	
	Readonly_Reference( Readonly );
	
	Readonly->Thread.Flags = READONLY_THREAD_FLAG_INITIALIZING;

	ExAcquireFastMutex( &Readonly->FastMutex );		
	SetFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_START );
	ClearFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_INITIALIZING );
	ExReleaseFastMutex( &Readonly->FastMutex );
			
	KeSetEvent( &Readonly->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	readonlyThreadTerminate = FALSE;
	
	while (readonlyThreadTerminate == FALSE) {

		PKEVENT			events[2];
		LONG			eventCount;
		NTSTATUS		eventStatus;
		LARGE_INTEGER	timeOut;
		

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		eventCount = 0;
		events[eventCount++] = &Readonly->RequestEvent;

		timeOut.QuadPart = -LFS_READONLY_THREAD_FLAG_TIME_OUT;

		eventStatus = KeWaitForMultipleObjects(	eventCount,
												events,
												WaitAny,
												Executive,
												KernelMode,
												TRUE,
												&timeOut,
												NULL );

		if (eventStatus == STATUS_TIMEOUT) {

			ReadonlyTryCloseCcb( CONTAINING_RECORD(Readonly->LfsDeviceExt, FILESPY_DEVICE_EXTENSION, LfsDeviceExt) );
			ReadonlyDismountVolumeStart( Readonly );
			continue;
		}

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
		ASSERT( eventCount < THREAD_WAIT_OBJECTS );
		
		if (!NT_SUCCESS( eventStatus ) || eventStatus >= eventCount) {

			ASSERT( LFS_UNEXPECTED );
			SetFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_ERROR );
			readonlyThreadTerminate = TRUE;
			continue;
		}
		
		KeClearEvent( events[eventStatus] );

		if (eventStatus == 0) {

			while (!FlagOn(Readonly->Thread.Flags, READONLY_THREAD_FLAG_STOPED) && 
				   (readonlyRequestEntry = ExInterlockedRemoveHeadList(&Readonly->RequestQueue,
																	    &Readonly->RequestQSpinLock))) {

				PREADONLY_REQUEST	readonlyRequest;


				InitializeListHead( readonlyRequestEntry );

				readonlyRequest = CONTAINING_RECORD( readonlyRequestEntry,
													  READONLY_REQUEST,
													  ListEntry );

				if (!(readonlyRequest->RequestType == READONLY_REQ_DISCONNECT ||
					  readonlyRequest->RequestType == READONLY_REQ_DOWN		  ||
					  readonlyRequest->RequestType == READONLY_REQ_SEND_MESSAGE)) {

					ASSERT( FALSE );

					ExAcquireFastMutex( &Readonly->FastMutex );
					SetFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_STOPED | READONLY_THREAD_FLAG_ERROR );
					ExReleaseFastMutex( &Readonly->FastMutex );

					ExInterlockedInsertHeadList( &Readonly->RequestQueue,
												 &readonlyRequest->ListEntry,
												 &Readonly->RequestQSpinLock );

					readonlyThreadTerminate = TRUE;
					break;
				}

				if (readonlyRequest->RequestType == READONLY_REQ_DISCONNECT) {
				
					if (readonlyRequest->Synchronous == TRUE)
						KeSetEvent(&readonlyRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
					else
						DereferenceReadonlyRequest( readonlyRequest );

					continue;
				}

				if (readonlyRequest->RequestType == READONLY_REQ_DOWN) {

					SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("ReadonlyThread READONLY_REQ_DOWN Readonly = %p\n", Readonly) );

					ExAcquireFastMutex( &Readonly->FastMutex );		
					SetFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_STOPED );
					ExReleaseFastMutex( &Readonly->FastMutex );

					ASSERT( IsListEmpty(&Readonly->RequestQueue) );

					if (readonlyRequest->Synchronous == TRUE)
						KeSetEvent( &readonlyRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferenceReadonlyRequest( readonlyRequest );

					readonlyThreadTerminate = TRUE;
					break;
				}

				ASSERT( readonlyRequest->RequestType == READONLY_REQ_SEND_MESSAGE );
				ASSERT( readonlyRequest->Synchronous == TRUE );

			} // while 
		
		} else {

			LfsDbgBreakPoint();
		}
	}

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	ExAcquireFastMutex( &Readonly->FastMutex );

	SetFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_STOPED );

	while (readonlyRequestEntry = ExInterlockedRemoveHeadList(&Readonly->RequestQueue,
															   &Readonly->RequestQSpinLock)) {

		PREADONLY_REQUEST readonlyRequest;

		InitializeListHead( readonlyRequestEntry );
			
		readonlyRequest = CONTAINING_RECORD( readonlyRequestEntry,
											  READONLY_REQUEST,
											  ListEntry );
        
		readonlyRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if (readonlyRequest->Synchronous == TRUE)
			KeSetEvent( &readonlyRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceReadonlyRequest( readonlyRequest );
	}

	ExReleaseFastMutex( &Readonly->FastMutex );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
				   ("ReadonlyThreadProc: PsTerminateSystemThread Readonly = %p, IsListEmpty(&Readonly->RequestQueue) = %d\n", 
				     Readonly, IsListEmpty(&Readonly->RequestQueue)) );
	
	ExAcquireFastMutex( &Readonly->FastMutex );
	SetFlag( Readonly->Thread.Flags, READONLY_THREAD_FLAG_TERMINATED );
	ExReleaseFastMutex( &Readonly->FastMutex );
	
	Readonly_Dereference( Readonly );

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	PsTerminateSystemThread( STATUS_SUCCESS );
}

NTSTATUS
ReadonlyDismountVolumeStart(
	IN  PREADONLY		Readonly
	)
{
	NTSTATUS			status;
	OBJECT_ATTRIBUTES	objectAttributes;
	LARGE_INTEGER		timeOut;

	if (Readonly->DismountThreadHandle)
		return STATUS_SUCCESS;

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	status = PsCreateSystemThread( &Readonly->DismountThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   ReadonlyDismountVolumeThreadProc,
								   Readonly );

	if (!NT_SUCCESS(status)) {
		
		return status;
	}

	timeOut.QuadPart = -LFS_TIME_OUT;		
	
	status = KeWaitForSingleObject( &Readonly->DiskmountReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		return status;
	}

	KeClearEvent( &Readonly->DiskmountReadyEvent );

	return status;
}

VOID
ReadonlyDismountVolumeThreadProc (
	IN	PREADONLY	Readonly
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("ReadonlyDismountVolumeThreadProc: Start Readonly = %p\n", Readonly) );
				
	Readonly_Reference( Readonly );

	KeSetEvent( &Readonly->DiskmountReadyEvent, IO_DISK_INCREMENT, FALSE );

	ReadonlyDismountVolume( Readonly->LfsDeviceExt );

	Readonly->DismountThreadHandle = NULL;

	Readonly_Dereference( Readonly );

	PsTerminateSystemThread( STATUS_SUCCESS );
}


NTSTATUS
ReadonlyDismountVolume (
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	)
{
	NTSTATUS				status;

	HANDLE					eventHandle;

	HANDLE					fileHandle = NULL;
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

	PIRP						topLevelIrp;
	READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
	
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
				   ("ReadonlyDismountVolume: LfsDeviceExt = %p, LfsDeviceExt->FileSystemType = %d LfsDeviceExt->Vpb->ReferenceCount = %d\n", 
					 LfsDeviceExt, LfsDeviceExt->FileSystemType, LfsDeviceExt->Vpb->ReferenceCount) );


	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES;
	desiredAccess |= FILE_WRITE_EA | FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT( desiredAccess == 0x0012019F );

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes( &objectAttributes,
								&LfsDeviceExt->NetdiskPartitionInformation.VolumeName,
								attributes,
								NULL,
								NULL );
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
	eaBuffer		  = NULL;
	eaLength		  = 0;

	RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

	topLevelIrp = IoGetTopLevelIrp();
	ASSERT( topLevelIrp == NULL );

	readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
	readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
	readonlyRedirectRequest.DevExt				= CONTAINING_RECORD( LfsDeviceExt, FILESPY_DEVICE_EXTENSION, LfsDeviceExt );
	readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
	readonlyRedirectRequest.ReadonlyDismount	= 1;

	IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

	status = ZwCreateFile( &fileHandle,
						   desiredAccess,
						   &objectAttributes,
						   &ioStatusBlock,
						   &allocationSize,
						   fileAttributes,
						   shareAccess,
						   createDisposition,
						   createOptions,
						   eaBuffer,
						   eaLength );

	ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
	IoSetTopLevelIrp( topLevelIrp );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
				   ("ReadonlyDismountVolume: LfsDeviceExt = %p ZwCreateFile fileHandle =%p, status = %X, ioStatusBlock = %X\n",
					 LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );

	if (status != STATUS_SUCCESS) {
	
		return status;
	
	} else {

		ASSERT( ioStatusBlock.Information == FILE_OPENED );
	}

	do {

		status = ZwCreateEvent( &eventHandle,
								GENERIC_READ,
								NULL,
								SynchronizationEvent,
								FALSE );

		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			break;
		}
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= CONTAINING_RECORD( LfsDeviceExt, FILESPY_DEVICE_EXTENSION, LfsDeviceExt );
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 1;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );
		
		status = ZwFsControlFile( fileHandle,
								  eventHandle,
								  NULL,
								  NULL,
								  &ioStatusBlock,
								  FSCTL_LOCK_VOLUME,
								  NULL,
								  0,
								  NULL,
								  0 );

		if (status == STATUS_PENDING) {

			status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

			if (status != STATUS_SUCCESS) {
				
				LfsDbgBreakPoint();
			
			} else {

				status = ioStatusBlock.Status;
			}
		}

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		ZwClose( eventHandle );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO,
							("ReadonlyDismountVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_LOCK_VOLUME fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );
		

#if 0

			status = ZwCreateEvent( &eventHandle,
									GENERIC_READ,
									NULL,
									SynchronizationEvent,
									FALSE );

			if (status != STATUS_SUCCESS) {

				ASSERT( LFS_UNEXPECTED );
				break;
			}
		
			RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
			status = ZwFsControlFile( fileHandle,
									  eventHandle,
									  NULL,
									  NULL,
									  &ioStatusBlock,
									  FSCTL_DISMOUNT_VOLUME,
									  NULL,
									  0,
									  NULL,
									  0 );

			if (status == STATUS_PENDING) {

				status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

				if (status != STATUS_SUCCESS)
					LfsDbgBreakPoint();
				else
					status = ioStatusBlock.Status;
			}

			ZwClose( eventHandle );

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO,
						   ("ReadonlyDismountVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_DISMOUNT_VOLUME fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );

			if (status != STATUS_SUCCESS) {
		
				break;
			}

			break;

#endif

			break;
		}

		status = ZwCreateEvent( &eventHandle,
								GENERIC_READ,
								NULL,
								SynchronizationEvent,
								FALSE );

		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			break;
		}
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= CONTAINING_RECORD( LfsDeviceExt, FILESPY_DEVICE_EXTENSION, LfsDeviceExt );
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 1;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );
		
		status = ZwFsControlFile( fileHandle,
								  eventHandle,
								  NULL,
								  NULL,
								  &ioStatusBlock,
								  FSCTL_UNLOCK_VOLUME,
								  NULL,
								  0,
								  NULL,
								  0 );

		if (status == STATUS_PENDING) {

			status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

			if (status != STATUS_SUCCESS) {
				
				LfsDbgBreakPoint();
				break;
			}

			status = ioStatusBlock.Status;
		}

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		ZwClose( eventHandle );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
							("ReadonlyDismountVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_UNLOCK_VOLUME fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );
		
			break;
		}
	
	} while(0);

	ZwClose( fileHandle );

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO,
				   ("ReadonlyDismountVolume: LfsDeviceExt = %p status = %X\n",
				    LfsDeviceExt, status) );

	if (status != STATUS_SUCCESS)
		return status;

#if 0

	do {

		UNICODE_STRING			fileName;
		PWCHAR					fileNameBuffer;

		fileNameBuffer = ExAllocatePool(PagedPool,NDFS_MAX_PATH);

		if(GlobalLfs.ShutdownOccured == TRUE)
			break;

		RtlInitEmptyUnicodeString( &fileName,
								   fileNameBuffer,
								   NDFS_MAX_PATH );
        
		RtlCopyUnicodeString( &fileName, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName );

		ioStatusBlock.Information = 0;

		status = RtlAppendUnicodeToString( &fileName, REMOUNT_VOLUME_FILE_NAME );

		if (status != STATUS_SUCCESS) {

			ExFreePool( fileNameBuffer );
			ASSERT( LFS_UNEXPECTED );

			status = STATUS_SUCCESS;
			break;
		}

		desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA;
		desiredAccess |= FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

		ASSERT( desiredAccess == 0x0012019F );

		attributes  = OBJ_KERNEL_HANDLE;
		attributes |= OBJ_CASE_INSENSITIVE;

		InitializeObjectAttributes( &objectAttributes,
									&fileName,
									attributes,
									NULL,
									NULL );
		
		allocationSize.LowPart  = 0;
		allocationSize.HighPart = 0;

		fileAttributes	  = 0;		
		shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
		createDisposition = FILE_OPEN;
		createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
		eaBuffer		  = NULL;
		eaLength		  = 0;
		
		status = ZwCreateFile( &fileHandle,
							   desiredAccess,
							   &objectAttributes,
							   &ioStatusBlock,
							   NULL,
							   fileAttributes,
							   shareAccess,
							   createDisposition,
							   createOptions,
							   NULL,
							   0 );
		
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					   ("ReadonlyDismountVolume: New Volume Create %p %wZ, status = %x, ioStatusBlock.Information = %d\n",
			             LfsDeviceExt, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName, status, ioStatusBlock.Information) );

		if (status == STATUS_SUCCESS) {

			ASSERT( ioStatusBlock.Information == FILE_OPENED );
			ZwClose( fileHandle );
		
		} else {

			LfsDbgBreakPoint();
		}

		status = STATUS_SUCCESS;

		ExFreePool( fileNameBuffer );

	} while(0);

#endif

	return status;
}


BOOLEAN
ReadonlyPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	)
{
	BOOLEAN					result = FALSE;
    PIO_STACK_LOCATION		irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT			fileObject = irpSp->FileObject;


	UNREFERENCED_PARAMETER( DeviceObject );

	ASSERT( DevExt->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &DevExt->LfsDeviceExt );	

	PrintIrp( LFS_DEBUG_READONLY_NOISE, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );

	if (DevExt->LfsDeviceExt.Filtering != TRUE) {

		LfsDbgBreakPoint();

		LfsDeviceExt_Dereference( &DevExt->LfsDeviceExt );
		return FALSE;
	}

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );
	ASSERT( DevExt->LfsDeviceExt.AttachedToDeviceObject == DevExt->AttachedToDeviceObject );

	try {

		if (DevExt->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			LfsDbgBreakPoint();

			result = FALSE;
			leave;
		}

		if (!(FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_START) && !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_STOP))) {

			ASSERT( DevExt->LfsDeviceExt.Readonly == NULL || ReadonlyLookUpCcb(DevExt, fileObject) == NULL );
			
			result = FALSE;
			leave;
		}

#if !__LFS_READONLY_PURGE_VERSION__

		result = ReadOnlyPassThrough( DeviceObject, Irp, irpSp, &DevExt->LfsDeviceExt, NtStatus );

		if (result == TRUE) {

			leave;
		}

#else

		if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

			PNDAS_CCB	ccb = NULL;

			ccb = ReadonlyLookUpCcb( DevExt, irpSp->FileObject );
		
			if (ccb == NULL || ccb->TypeOfOpen != UserVolumeOpen) {

				*NtStatus = Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				leave;
			}

			result = ReadonlyIoctl( DevExt, Irp, NtStatus );

			leave;
		}

		switch (irpSp->MajorFunction) {

		case IRP_MJ_PNP:

			break;

		default: 

			*NtStatus = ReadonlyRedirectIrp( DevExt, Irp, &result );

			leave;
		}

#endif

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			PrintIrp( LFS_DEBUG_READONLY_NOISE, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );

			if (irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				if (DevExt->LfsDeviceExt.NetdiskPartition == NULL) {

					LfsDbgBreakPoint();
				
				} else {

					NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
													DevExt->LfsDeviceExt.NetdiskPartition,
													DevExt->LfsDeviceExt.NetdiskEnabledMode );
				}

				DevExt->LfsDeviceExt.NetdiskPartition = NULL;			
				SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_SURPRISE_REMOVAL );

				result = FALSE;
				leave;

			}

			// Need to test much whether this is okay..
	
			if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				result = FALSE;
				leave;
			
			} else if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				KEVENT				waitEvent;

#if __LFS_READONLY_PURGE_VERSION__

				ReadonlyTryCloseCcb( DevExt );

				if (!IsListEmpty(&DevExt->LfsDeviceExt.Readonly->FcbQueue)) {

					LARGE_INTEGER interval;
				
					// Wait all files closed
					interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
					KeDelayExecutionThread( KernelMode, FALSE, &interval );
				}
			
				if (!IsListEmpty(&DevExt->LfsDeviceExt.Readonly->FcbQueue)) {

					*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
					result = TRUE;
					leave;
				
				} 

#endif

				SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_STOPPING );

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										ReadonlyPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				*NtStatus = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (*NtStatus == STATUS_PENDING) {

					*NtStatus = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( *NtStatus == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

				ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_STOPPING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_STOP) );
					LfsCleanupMountedDevice( DevExt->LfsDeviceExt.FileSpyDeviceObject );
				}

				*NtStatus = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;
			}

			result = FALSE;
			leave;
		}

	} finally {

		LfsDeviceExt_Dereference( &DevExt->LfsDeviceExt );
	}

	return result;
}



NTSTATUS
ReadonlyPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("%s: called\n", __FUNCTION__) );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}


PNDAS_FCB
ReadonlyAllocateFcb(
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PUNICODE_STRING				FullFileName
    )
{
    PNDAS_FCB fcb;

	
    fcb = FsRtlAllocatePoolWithTag( NonPagedPool,
									sizeof(NDAS_FCB),
									LFS_FCB_TAG );
	
	if (fcb == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_ERROR, ("ReadonlyAllocateFcb: failed to allocate fcb\n") );
		return NULL;
	}
	
	RtlZeroMemory( fcb, sizeof(NDAS_FCB) );

    fcb->NonPaged = LfsAllocateNonPagedFcb();
	
	if (fcb->NonPaged == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_ERROR, ("ReadonlyAllocateFcb: failed to allocate fcb->NonPaged\n") );
		ExFreePool( fcb );
		
		return NULL;
	}

    RtlZeroMemory( fcb->NonPaged, sizeof(NON_PAGED_FCB) );

#define FAT_NTC_FCB                      (0x0502)
	
	fcb->Header.NodeTypeCode = FAT_NTC_FCB;
	fcb->Header.IsFastIoPossible = FastIoIsPossible;
    fcb->Header.Resource = LfsAllocateResource();
	fcb->Header.PagingIoResource = NULL; //fcb->Header.Resource;
	
	if (fcb->Header.Resource == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_ERROR, ("ReadonlyAllocateFcb: failed to allocate fcb->Header.Resource\n") );
		ExFreePool( fcb->NonPaged );
		ExFreePool( fcb );
		return NULL;
	}

	ExInitializeFastMutex( &fcb->NonPaged->AdvancedFcbHeaderMutex );

#if WINVER >= 0x0501

	if (IS_WINDOWSXP_OR_LATER()) {

		FsRtlSetupAdvancedHeader( &fcb->Header, &fcb->NonPaged->AdvancedFcbHeaderMutex );
	}

#endif

	FsRtlInitializeFileLock( &fcb->FileLock, NULL, NULL );

	fcb->ReferenceCount = 1;
	InitializeListHead( &fcb->ListEntry );

	fcb->Readonly = DevExt->LfsDeviceExt.Readonly;

    RtlInitEmptyUnicodeString( &fcb->FullFileName,
							   fcb->FullFileNameBuffer,
							   sizeof(fcb->FullFileNameBuffer) );

	RtlCopyUnicodeString( &fcb->FullFileName, FullFileName );

    RtlInitEmptyUnicodeString( &fcb->CaseInSensitiveFullFileName,
							   fcb->CaseInSensitiveFullFileNameBuffer,
							   sizeof(fcb->CaseInSensitiveFullFileNameBuffer) );

	RtlDowncaseUnicodeString( &fcb->CaseInSensitiveFullFileName,
							  &fcb->FullFileName,
							  FALSE );

	if(FullFileName->Length)
		if(FullFileName->Buffer[0] != L'\\')
			ASSERT( LFS_BUG );
	
	return fcb;
}


VOID
ReadonlyDereferenceFcb (
	IN	PNDAS_FCB	Fcb
	)
{
	LONG		result;


	SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
				   ("ReadonlyDereferenceFcb: Fcb->OpenCount = %d, Fcb->UncleanCount = %d\n", Fcb->OpenCount, Fcb->UncleanCount) );

	ASSERT( Fcb->OpenCount >= Fcb->UncleanCount );
	result = InterlockedDecrement( &Fcb->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		ASSERT( Fcb->ListEntry.Flink == Fcb->ListEntry.Blink );
		ASSERT( Fcb->OpenCount == 0 );
		
	    LfsFreeResource( Fcb->Header.Resource );
		LfsFreeNonPagedFcb( Fcb->NonPaged );
	    ExFreePool( Fcb );	
	}
}


PNDAS_FCB
ReadonlyLookUpFcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PUNICODE_STRING				FullFileName,
    IN BOOLEAN						CaseInSensitive
	)
{
	PNDAS_FCB		fcb = NULL;
    PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	UNICODE_STRING	caseInSensitiveFullFileName;
	PWCHAR			caseInSensitiveFullFileNameBuffer;
	NTSTATUS		downcaseStatus;

	//
	//	Allocate a name buffer
	//

	caseInSensitiveFullFileNameBuffer = ExAllocatePoolWithTag(NonPagedPool, NDFS_MAX_PATH, LFS_ALLOC_TAG);
	
	if (caseInSensitiveFullFileNameBuffer == NULL) {
	
		ASSERT( LFS_REQUIRED );
		return NULL;
	}

	ASSERT( FullFileName->Length <= NDFS_MAX_PATH*sizeof(WCHAR) );

	if (CaseInSensitive == TRUE) {

		RtlInitEmptyUnicodeString( &caseInSensitiveFullFileName,
								   caseInSensitiveFullFileNameBuffer,
								   NDFS_MAX_PATH );

		downcaseStatus = RtlDowncaseUnicodeString( &caseInSensitiveFullFileName,
												   FullFileName,
												   FALSE );
	
		if (downcaseStatus != STATUS_SUCCESS) {

			ExFreePool( caseInSensitiveFullFileNameBuffer );
			ASSERT( LFS_UNEXPECTED );
			return NULL;
		}
	}

	KeAcquireSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, &oldIrql );

    for (listEntry = DevExt->LfsDeviceExt.Readonly->FcbQueue.Flink;
         listEntry != &DevExt->LfsDeviceExt.Readonly->FcbQueue;
         listEntry = listEntry->Flink) {

		fcb = CONTAINING_RECORD( listEntry, NDAS_FCB, ListEntry );
		
		if (fcb->FullFileName.Length != FullFileName->Length) {

			fcb = NULL;
			continue;
		}

		if (CaseInSensitive == TRUE) {

			if (RtlEqualMemory(fcb->CaseInSensitiveFullFileName.Buffer, 
							   caseInSensitiveFullFileName.Buffer, 
							   fcb->CaseInSensitiveFullFileName.Length)) {

				InterlockedIncrement( &fcb->ReferenceCount );
				break;
			}
		
		} else {

			if (RtlEqualMemory(fcb->FullFileName.Buffer,
							   FullFileName->Buffer,
							   fcb->FullFileName.Length)) {

				InterlockedIncrement( &fcb->ReferenceCount );
				break;
			}
		}

		fcb = NULL;
	}

	KeReleaseSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, oldIrql );

	ExFreePool( caseInSensitiveFullFileNameBuffer );

	return fcb;
}


PNDAS_CCB
ReadonlyAllocateCcb(
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN	PFILE_OBJECT				FileObject,
	IN  ULONG						BufferLength
	) 
{
	PNDAS_CCB	ccb;


	ccb = ExAllocatePoolWithTag( NonPagedPool, 
								 sizeof(NDAS_CCB) - sizeof(_U8) + BufferLength + MEMORY_CHECK_SIZE,
								 FILE_EXT_TAG );
	
	if (ccb == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( ccb, sizeof(NDAS_CCB) - sizeof(_U8) + BufferLength );
	
#if DBG && MEMORY_CHECK_SIZE
	{
		UCHAR	i;
		ULONG	memorySize = sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength;

		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			*((_U8*)fileExt + memorySize + i) = i;
	}
#endif

	ccb->Mark		= CCB_MARK;

	ccb->Readonly	= DevExt->LfsDeviceExt.Readonly;
	ccb->FileObject	= FileObject;

	InitializeListHead( &ccb->ListEntry );

	ccb->BufferLength = BufferLength;

	InterlockedIncrement( &DevExt->LfsDeviceExt.Readonly->CcbCount );

	return ccb;
}


VOID
ReadonlyFreeCcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PNDAS_CCB					Ccb
	)
{
	PLIST_ENTRY		listEntry;


	ASSERT( Ccb->ListEntry.Flink == Ccb->ListEntry.Blink );

#if DBG && MEMORY_CHECK_SIZE

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

	ExAcquireFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

    for (listEntry = DevExt->LfsDeviceExt.Readonly->CcbQueue.Flink;
         listEntry != &DevExt->LfsDeviceExt.Readonly->CcbQueue;
         listEntry = listEntry->Flink) {

		PNDAS_CCB	childCcb;
		
		childCcb = CONTAINING_RECORD( listEntry, NDAS_CCB, ListEntry );
        
		if (childCcb->CreateContext.RelatedFileHandle == Ccb->ReadonlyFileHandle)
			childCcb->RelatedFileObjectClosed = TRUE;
	}

    ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

	InterlockedDecrement( &DevExt->LfsDeviceExt.Readonly->CcbCount );

	ExFreePoolWithTag( Ccb, FILE_EXT_TAG );
}


PNDAS_CCB
ReadonlyLookUpCcbByHandle (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN HANDLE						FileHandle
	)
{
	NTSTATUS		status;
	PFILE_OBJECT	fileObject = NULL;


	status = ObReferenceObjectByHandle( FileHandle,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&fileObject,
										NULL );

    if (status != STATUS_SUCCESS) {

		return NULL;
	}
	
	ObDereferenceObject( fileObject );

	return ReadonlyLookUpCcb( DevExt, fileObject );
}

	
PNDAS_CCB
ReadonlyLookUpCcb(
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PFILE_OBJECT					FileObject
	)
{
	PNDAS_CCB		ccb = NULL;
    PLIST_ENTRY		listEntry;

	
    ExAcquireFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

    for (listEntry = DevExt->LfsDeviceExt.Readonly->CcbQueue.Flink;
         listEntry != &DevExt->LfsDeviceExt.Readonly->CcbQueue;
         listEntry = listEntry->Flink) {

		 ccb = CONTAINING_RECORD( listEntry, NDAS_CCB, ListEntry );
         
		 if (ccb->FileObject == FileObject)
			break;

		ccb = NULL;
	}

    ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

	return ccb;
}


PNDAS_CCB
ReadonlyLookUpCcbByReadonlyFileObject(
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PFILE_OBJECT					ReadonlyFileObject
	)
{
	PNDAS_CCB		ccb = NULL;
    PLIST_ENTRY		listEntry;

	
    ExAcquireFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

    for (listEntry = DevExt->LfsDeviceExt.Readonly->CcbQueue.Flink;
         listEntry != &DevExt->LfsDeviceExt.Readonly->CcbQueue;
         listEntry = listEntry->Flink) {

		 ccb = CONTAINING_RECORD( listEntry, NDAS_CCB, ListEntry );
         
		 if (ccb->ReadonlyFileObject == ReadonlyFileObject)
			break;

		ccb = NULL;
	}

    ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

	return ccb;
}

NTSTATUS
ReadonlyMakeFullFileName (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PFILE_OBJECT					FileObject, 
	IN PUNICODE_STRING				FullFileName,
	IN BOOLEAN						FileDirectoryFile
	)
{
	NTSTATUS	status;

	do {

		if (FileObject->RelatedFileObject) {

			if (ReadonlyLookUpCcb(DevExt, FileObject->RelatedFileObject)) {

				PNDAS_FCB	relatedFcb = FileObject->RelatedFileObject->FsContext;
				PNDAS_CCB	relatedCcb = FileObject->RelatedFileObject->FsContext2;
		
				ASSERT( relatedCcb->Mark == CCB_MARK );

				status = RtlAppendUnicodeStringToString( FullFileName,
													     &relatedFcb->FullFileName );

				if (status != STATUS_SUCCESS)
					break;

				if (relatedFcb->FullFileName.Length >= 2 && relatedFcb->FullFileName.Buffer[relatedFcb->FullFileName.Length/sizeof(WCHAR)-1] != L'\\') {

					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
								   ("Secondary_MakeFullFileName, relatedFcb->FullFileName = %wZ, FileObject->FileName = %wZ\n", 
								    &relatedFcb->FullFileName, &FileObject->FileName) );

					status = RtlAppendUnicodeToString( FullFileName, L"\\" );

					if (status != STATUS_SUCCESS)
						break;
		
				} else {

					if (FileObject->FileName.Length == 0) {
		
						//LfsDbgBreakPoint();
					}
				}
			
			} else {

				if (FileObject->RelatedFileObject->FileName.Length == 0) {

					status = RtlAppendUnicodeStringToString( FullFileName,
															 &DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName );
				
					if (status != STATUS_SUCCESS)
						break;

				} else {

					status = STATUS_OBJECT_NAME_INVALID;
					break;
				}
			}
	
		} else {

			status = RtlAppendUnicodeStringToString( FullFileName,
													 &DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName );

			if (status != STATUS_SUCCESS)
				break;
		}
	
		status = RtlAppendUnicodeStringToString( FullFileName, &FileObject->FileName );

		if (status != STATUS_SUCCESS)
			break;
		
		if (FileDirectoryFile && FullFileName->Buffer[FullFileName->Length/sizeof(WCHAR)-1] != L'\\') {

			status = RtlAppendUnicodeToString( FullFileName, L"\\" );
		}
		
	} while (0);

	return status;
}

NTSTATUS
ReadonlyIoctlCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("ReadonlyIoctlCompletion: called\n") );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}

BOOLEAN
ReadonlyIoctl (
	IN	PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp,
	OUT PNTSTATUS					NtStatus
	)
{
    PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
    KEVENT				waitEvent;
	NTSTATUS			status;
	ULONG				ioctlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
	ULONG				devType = DEVICE_TYPE_FROM_CTL_CODE(ioctlCode);
	UCHAR				function = (UCHAR)((irpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);


	if (IS_WINDOWS2K() && DevExt->LfsDeviceExt.DiskDeviceObject == NULL) {

		LfsDbgBreakPoint();
		return FALSE;
	}

	if (IS_WINDOWSXP_OR_LATER() && DevExt->LfsDeviceExt.MountVolumeDeviceObject == NULL) {

		LfsDbgBreakPoint();
		return FALSE;
	}

	if (ioctlCode == IOCTL_VOLUME_SET_GPT_ATTRIBUTES) {

		Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

		if (NtStatus)
			*NtStatus = STATUS_NOT_SUPPORTED;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		return TRUE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
				   ("Open files from secondary: LfsDeviceExt = %p, irpSp->MajorFunction = %x, irpSp->MinorFunction = %x\n", 
					 &DevExt->LfsDeviceExt, irpSp->MajorFunction, irpSp->MinorFunction) );


	IoCopyCurrentIrpStackLocationToNext( Irp );
	KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

	IoSetCompletionRoutine( Irp,
							ReadonlyIoctlCompletion,
							&waitEvent,
							TRUE,
							TRUE,
							TRUE );

	if (IS_WINDOWS2K())
		status = IoCallDriver( DevExt->LfsDeviceExt.DiskDeviceObject, Irp );
	else
		status = IoCallDriver( DevExt->LfsDeviceExt.MountVolumeDeviceObject, Irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &waitEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );
	}

	ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
	SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, 
				   ("ReadonlyIoctl: Irp->IoStatus.Status = %x\n", Irp->IoStatus.Status) );

	*NtStatus = Irp->IoStatus.Status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );

	return TRUE;
}


VOID
ReadonlyTryCloseCcb (
	IN	PFILESPY_DEVICE_EXTENSION	DevExt
	)
{
	PLIST_ENTRY		listEntry;


	Readonly_Reference( DevExt->LfsDeviceExt.Readonly );	
	
	if (ExTryToAcquireFastMutex(&DevExt->LfsDeviceExt.Readonly->CcbQMutex) == FALSE) {

		Readonly_Dereference( DevExt->LfsDeviceExt.Readonly );
		return;
	}
	
	listEntry = DevExt->LfsDeviceExt.Readonly->CcbQueue.Flink;

	while (listEntry != &DevExt->LfsDeviceExt.Readonly->CcbQueue) {

		PNDAS_CCB					ccb = NULL;
		PNDAS_FCB					fcb;
		PSECTION_OBJECT_POINTERS	section;
		BOOLEAN						dataSectionExists;
		BOOLEAN						imageSectionExists;

		ccb = CONTAINING_RECORD( listEntry, NDAS_CCB, ListEntry );
		listEntry = listEntry->Flink;

		if (ccb->TypeOfOpen != UserFileOpen)
			break;

		fcb = ccb->Fcb;

		if (fcb == NULL) {

			ASSERT( LFS_BUG );
			break;
		}	

		SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, 
					   ("ReadonlyTryCloseCcb: fcb->FullFileName = %wZ\n", &fcb->FullFileName) );

		if (fcb->UncleanCount != 0) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
						   ("ReadonlyTryCloseCcb: fcb->FullFileName = %wZ\n", &fcb->FullFileName) );
			break;
		}

	    if (fcb->Header.PagingIoResource != NULL) {

			ASSERT( LFS_REQUIRED );
			break;
		}

		section = &fcb->NonPaged->SectionObjectPointers;			
		if (section == NULL)
			break;

        //CcFlushCache(section, NULL, 0, &ioStatusBlock);

		dataSectionExists = (BOOLEAN)(section->DataSectionObject != NULL);
		imageSectionExists = (BOOLEAN)(section->ImageSectionObject != NULL);

		if (imageSectionExists) {

			(VOID)MmFlushImageSection( section, MmFlushForWrite );
		}

		if (dataSectionExists) {

            CcPurgeCacheSection( section, NULL, 0, FALSE );
	    }
	}

	ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );
	Readonly_Dereference( DevExt->LfsDeviceExt.Readonly );

	return;
}

BOOLEAN
ReadOnlyPassThrough (
    IN PDEVICE_OBJECT				DeviceObject,
    IN PIRP							Irp,
    IN PIO_STACK_LOCATION			IrpSp,
	IN PLFS_DEVICE_EXTENSION		LfsDevExt,
	OUT	PNTSTATUS					NtStatus
   )
{
	BOOLEAN				ProcessDone ;
	ULONG				options ;
	CHAR				disposition ;
	PFILE_OBJECT		fileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					("IRP_MN_QUERY_DIRECTORY FileSystemType = %x\n", 
					LfsDevExt->FileSystemType));

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					("Is windows2k = %x\n", IS_WINDOWS2K()));

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					("Major = %d, Minor = %d\n", gSpyOsMajorVersion, gSpyOsMinorVersion));

	if(((PFILESPY_DEVICE_EXTENSION)(DeviceObject->DeviceExtension))->DiskDeviceObject == NULL) // It's a Control Device 
		return FALSE ;

	//
	//	skip Meta Files
	//
	fileObject = IrpSp->FileObject;

#if DBG
	if(fileObject) {
		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE,
				("ReadOnlyPassThrough: IrpFlag:%08lx fileObject->FileName = %wZ\n", Irp->Flags, &fileObject->FileName));
	}
#endif

    if(IrpSp->MajorFunction != IRP_MJ_CREATE)
	{
		if( ( fileObject && IsMetaFile(&fileObject->FileName)) ||
			( Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO )		// file system meta file through CCM
			)
		{
			SPY_LOG_PRINT( LFS_DEBUG_LIB_TRACE,
					("ReadOnlyPassThrough: system volume file fileObject->FileName = %wZ\n", &fileObject->FileName));
			return FALSE;
		}
	}

	ProcessDone = FALSE ;

	switch(IrpSp->MajorFunction) 
	{
		case IRP_MJ_CREATE:
			//
			//	if it contains creation, stop it
			//	(Options >> 24) & 0xFF
			options = IrpSp->Parameters.Create.Options & 0xFFFFFF ;
			disposition = (CHAR)(((IrpSp->Parameters.Create.Options) >> 24) & 0xFF) ;
			if( (options & FILE_DELETE_ON_CLOSE) ||
				disposition == FILE_CREATE ||
				disposition == FILE_SUPERSEDE ||
				disposition == FILE_OVERWRITE ||
				disposition == FILE_OVERWRITE_IF
				) 
			{
				*NtStatus = STATUS_MEDIA_WRITE_PROTECTED ;
				Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED ;
				Irp->IoStatus.Information = 0 ;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;

				ProcessDone = TRUE ;

				break ;
			}

			//
			//	check the purge flag.
			//
			if(!LfsDevExt->Purge)
				break ;

#if 0
			//
			//	purge cache
			//
			switch(LfsDevExt->FileSystemType)
			{
			case LFS_FILE_SYSTEM_NTFS:

#ifndef _WIN64
				if(IS_WINDOWS2K())
				{
					BOOLEAN flushResult;
		
	
					flushResult = W2kNtfsFlushMetaFile (
								DeviceObject,
								LfsDevExt->BaseVolumeDeviceObject,
								LfsDevExt->BufferLength,
								LfsDevExt->Buffer
								);

					SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
								("ReadOnlyPassThrough flushResult = %d\n", 
								flushResult));

				} else
				if(IS_WINDOWSXP())
				{
					BOOLEAN flushResult;

					flushResult = WxpNtfsFlushMetaFile (
									DeviceObject,
									LfsDevExt->BaseVolumeDeviceObject,
									LfsDevExt->BufferLength,
									LfsDevExt->Buffer
									);

					SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
								("ReadOnlyPassThrough flushResult = %d\n", 
								flushResult));
				}
#endif
			break;

		default:
			break;
		}
#endif

		break ;

		case IRP_MJ_WRITE:
		case IRP_MJ_SET_INFORMATION:
        case IRP_MJ_SET_EA:
        case IRP_MJ_SET_VOLUME_INFORMATION:
        case IRP_MJ_SET_SECURITY:
		case IRP_MJ_SET_QUOTA:

			*NtStatus = STATUS_MEDIA_WRITE_PROTECTED ;
			Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED ;
			Irp->IoStatus.Information = 0 ;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;

			ProcessDone = TRUE ;

			break ;

        case IRP_MJ_FILE_SYSTEM_CONTROL:
			if(IrpSp->Parameters.DeviceIoControl.IoControlCode == FSCTL_MOVE_FILE) {

				*NtStatus = STATUS_MEDIA_WRITE_PROTECTED ;
				Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED ;
				Irp->IoStatus.Information = 0 ;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;

				ProcessDone = TRUE ;
			}

			break ;

		case IRP_MJ_DIRECTORY_CONTROL:
			//
			//	purge only on Query Directory 
			//
			if(IrpSp->MinorFunction != IRP_MN_QUERY_DIRECTORY)
				break ;

			//
			//	check the purge flag.
			//
			if(!LfsDevExt->Purge)
				break ;

#if 0
			switch(LfsDevExt->FileSystemType)
			{
			case LFS_FILE_SYSTEM_NTFS:
#ifndef _WIN64
				if(IS_WINDOWS2K())
				{
					W2kNtfsFlushOnDirectoryControl(
								DeviceObject,
								LfsDevExt->BaseVolumeDeviceObject,
								IrpSp,
								LfsDevExt->BufferLength,
								LfsDevExt->Buffer
							);

				} else if(IS_WINDOWSXP())
				{
					WxpNtfsFlushOnDirectoryControl(
								DeviceObject,
								LfsDevExt->BaseVolumeDeviceObject,
								IrpSp,
								LfsDevExt->BufferLength,
								LfsDevExt->Buffer
							);
					SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
								("ReadOnlyPassThrough Flush triggered by DirectoryControl\n"));
				}
#endif
				break;

			case LFS_FILE_SYSTEM_FAT:
#ifndef _WIN64
				if(IS_WINDOWS2K())
				{
					W2kFatFlushOnDirectoryControl(
							DeviceObject,
							LfsDevExt->BaseVolumeDeviceObject,
							IrpSp
						);

				} else if(IS_WINDOWSXP())
				{
					WxpFatFlushOnDirectoryControl(
							DeviceObject,
							LfsDevExt->BaseVolumeDeviceObject,
							IrpSp
						);
				} else
#endif
				if(IS_WINDOWSNET())
				{
					WnetFatFlushOnDirectoryControl(
							DeviceObject,
							LfsDevExt->BaseVolumeDeviceObject,
							IrpSp
						);
				}
				break;
			default:
				break ;
			}
#endif
		break ;

	default:
		;
	}

	return ProcessDone ;
}

