#include "FatProcs.h"

#ifdef __ND_FAT_PRIMARY__


#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('PftN')

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)


static PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest(
	IN	BOOLEAN	Synchronous
	);

static FORCEINLINE
NTSTATUS
QueueingPrimarySessionRequest(
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	);

static VOID
DereferencePrimarySessionRequest(
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	); 

static VOID
PrimarySessionThreadProc(
	IN PPRIMARY_SESSION PrimarySession
	);

static VOID
CloseOpenFiles(
	IN PPRIMARY_SESSION		PrimarySession
	);

static VOID
DisconnectFromSecondary(
	IN	PPRIMARY_SESSION	PrimarySession
	);

static NTSTATUS
SendNdfsWinxpMessage(
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN _U32						ReplyDataSize,
	IN _U16						Mid
	);



PPRIMARY_SESSION
PrimarySession_Create(
	IN  PIRP_CONTEXT			IrpContext,  
	IN	PVOLUME_DEVICE_OBJECT	VolDo,		 
	IN  PSESSION_INFORMATION	SessionInformation,
	IN  PIRP					Irp
	)
{
	PPRIMARY_SESSION	primarySession;
 	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			status;
	LARGE_INTEGER		timeOut;

		
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	VolDo_Reference( VolDo );

	primarySession = FsRtlAllocatePoolWithTag( NonPagedPool, sizeof(PRIMARY_SESSION), NDFAT_ALLOC_TAG );
	
	if (primarySession == NULL) {

		ASSERT( NDFAT_INSUFFICIENT_RESOURCES );
		VolDo_Dereference( VolDo );
		return NULL;
	}

	try {
	
		RtlZeroMemory( primarySession, sizeof(PRIMARY_SESSION) );

		primarySession->Flags = PRIMARY_SESSION_FLAG_INITIALIZING;

		primarySession->ReferenceCount = 1;
		primarySession->VolDo = VolDo;
		
		ExInitializeFastMutex( &primarySession->FastMutex )
		
		InitializeListHead( &primarySession->ListEntry );

		primarySession->ConnectionFileHandle		= SessionInformation->ConnectionFileHandle;
		primarySession->ConnectionFileObject		= SessionInformation->ConnectionFileObject;
		primarySession->NetdiskPartitionInformation = SessionInformation->NetdiskPartitionInformation;
		primarySession->Irp							= Irp;

		primarySession->SessionContext.SessionKey			= SessionInformation->SessionKey;
		primarySession->SessionContext.Flags				= SessionInformation->SessionFlags;
		primarySession->SessionContext.PrimaryMaxDataSize	= SessionInformation->PrimaryMaxDataSize;
		primarySession->SessionContext.SecondaryMaxDataSize	= SessionInformation->SecondaryMaxDataSize;
		primarySession->SessionContext.Uid					= SessionInformation->Uid;
		primarySession->SessionContext.Tid					= SessionInformation->Tid;
		primarySession->SessionContext.NdfsMajorVersion		= SessionInformation->NdfsMajorVersion;
		primarySession->SessionContext.NdfsMinorVersion		= SessionInformation->NdfsMinorVersion;
		primarySession->SessionContext.SessionSlotCount		= (UCHAR)SessionInformation->RequestPerSession;

		DebugTrace2( 0, Dbg2, ("primarySession->ConnectionFileHandle = %x\n", primarySession->ConnectionFileHandle) );

		KeInitializeEvent( &primarySession->ReadyEvent, NotificationEvent, FALSE );
	
		InitializeListHead( &primarySession->RequestQueue );
		KeInitializeSpinLock( &primarySession->RequestQSpinLock );
		KeInitializeEvent( &primarySession->RequestEvent, NotificationEvent, FALSE );

		primarySession->ThreadHandle = 0;
		primarySession->ThreadObject = NULL;

		primarySession->Thread.TdiReceiveContext.Irp = NULL;
		KeInitializeEvent( &primarySession->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );

		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		primarySession->Thread.SessionState = SESSION_TREE_CONNECT;
	
		ExInterlockedInsertTailList( &VolDo->PrimarySessionQueue,
									 &primarySession->ListEntry,
									 &VolDo->PrimarySessionQSpinLock );

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

		timeOut.QuadPart = -NDFAT_TIME_OUT;
		status = KeWaitForSingleObject( &primarySession->ReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );


		if (!NT_SUCCESS(status)) {

			leave;
		}

		KeClearEvent( &primarySession->ReadyEvent );

		DebugTrace2( 0, Dbg, ("PrimarySession_Create: The primary thread are ready\n") );
	
		DebugTrace2( 0, Dbg2, ("Fat PrimarySession_Create: primarySession = %p\n", primarySession) );
	
	} finally {

		if (AbnormalTermination()) {

			status = IrpContext->ExceptionStatus;
		}

		if (!NT_SUCCESS(status)) {

			ASSERT( NDFAT_UNEXPECTED );
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
	DebugTrace2( 0, Dbg2, ("PrimarySession_Close: PrimarySession = %p\n", PrimarySession) );

	if (PrimarySession->ThreadHandle == NULL) {

		ASSERT( NDFAT_BUG );
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
									   primarySessionRequest );

		primarySessionRequest = AllocPrimarySessionRequest( FALSE );
		primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_DOWN;

		QueueingPrimarySessionRequest( PrimarySession,
									   primarySessionRequest );

		timeOut.QuadPart = - NDFAT_TIME_OUT;
		ntStatus = KeWaitForSingleObject( PrimarySession->ThreadObject,
										  Executive,
										  KernelMode,
										  FALSE,
										  &timeOut );

		if (ntStatus == STATUS_SUCCESS) {

		    DebugTrace2( 0, Dbg, ("PrimarySession_Close: thread stoped\n") );

			ObDereferenceObject( PrimarySession->ThreadObject );

			PrimarySession->ThreadHandle = NULL;
			PrimarySession->ThreadObject = NULL;
		
		
		} else {

			ASSERT( NDFAT_BUG );
			return;
		}
	}

	if (PrimarySession->Irp) {

		PPRIMARY_SESSION	*outputBuffer;

		outputBuffer = PrimarySession->Irp->AssociatedIrp.SystemBuffer;
		*outputBuffer = PrimarySession;
		DebugTrace2( 0, Dbg2, ("PrimarySession_Close: IOCTL_INSERT_PRIMARY_SESSION returned %x\n", PrimarySession) );		

		PrimarySession->Irp->IoStatus.Status = STATUS_SUCCESS;
		PrimarySession->Irp->IoStatus.Information = sizeof( PrimarySession );
        IoCompleteRequest( PrimarySession->Irp, IO_DISK_INCREMENT );
		PrimarySession->Irp = NULL;
	}

	PrimarySession_Dereference( PrimarySession );

	return;
}

VOID
PrimarySession_FileSystemShutdown(
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;
	NTSTATUS					ntStatus ;
	LARGE_INTEGER				timeOut ;

	
	PrimarySession_Reference( PrimarySession );

	DebugTrace2( 0, Dbg2, ("PrimarySession_FileSystemShutdown: PrimarySession = %p\n", PrimarySession) );

	if (PrimarySession->ThreadHandle == NULL) {

		ASSERT(NDFAT_BUG);
		PrimarySession_Dereference(PrimarySession);

		return;
	}

	ASSERT(PrimarySession->ThreadObject != NULL);

	if (FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_TERMINATED)) {

		PrimarySession_Dereference(PrimarySession);
		return;
	}
		
	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_SHUTDOWN;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest );

	timeOut.QuadPart = - NDFAT_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );

	if (ntStatus == STATUS_SUCCESS) {

		DebugTrace2( 0, Dbg, ("PrimarySession_FileSystemShutdown: thread shutdown\n") );		
	
	} else {

		ASSERT( NDFAT_BUG );
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

		KIRQL					oldIrql;
		PVOLUME_DEVICE_OBJECT	volDo = PrimarySession->VolDo;	

		
		KeAcquireSpinLock( &volDo->PrimarySessionQSpinLock, &oldIrql );
		RemoveEntryList( &PrimarySession->ListEntry );
		KeReleaseSpinLock( &volDo->PrimarySessionQSpinLock, oldIrql );
		
		ExFreePoolWithTag( PrimarySession, NDFAT_ALLOC_TAG	);

		DebugTrace2( 0, Dbg2,
					("PrimarySession_Dereference: PrimarySession = %p is Freed\n", PrimarySession) );

		VolDo_Dereference( volDo );
	}
}


static PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest(
	IN	BOOLEAN	Synchronous
	) 
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;


	primarySessionRequest = ExAllocatePoolWithTag( NonPagedPool,
												   sizeof(PRIMARY_SESSION_REQUEST),
												   PRIMARY_SESSION_MESSAGE_TAG );

	if (primarySessionRequest == NULL) {

		ASSERT( NDFAT_INSUFFICIENT_RESOURCES );
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
static NTSTATUS
QueueingPrimarySessionRequest(
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	)
{
	NTSTATUS	status;


	ASSERT( PrimarySessionRequest->ListEntry.Flink == PrimarySessionRequest->ListEntry.Blink );

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


static VOID
DereferencePrimarySessionRequest(
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	) 
{
	LONG	result;

	result = InterlockedDecrement( &PrimarySessionRequest->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		ExFreePoolWithTag(PrimarySessionRequest, PRIMARY_SESSION_MESSAGE_TAG);
		DebugTrace2( 0, Dbg,	("FreePrimarySessionRequest: PrimarySessionRequest freed\n") );
	}

	return;
}

POPEN_FILE
PrimarySession_AllocateOpenFile(
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject
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
		ASSERT(NDFAT_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(openFile,	sizeof(OPEN_FILE));

	openFile->FileHandle = FileHandle;
	openFile->FileObject = FileObject;

	openFile->PrimarySession = PrimarySession;

#if 0
    RtlInitEmptyUnicodeString(
				&openFile->FullFileName,
                openFile->FullFileNameBuffer,
				sizeof(openFile->FullFileNameBuffer)
				);

	if (FullFileName)
		RtlCopyUnicodeString( &openFile->FullFileName, FullFileName );
#endif

	InitializeListHead(&openFile->ListEntry);
	
	ExInterlockedInsertHeadList(
			&PrimarySession->Thread.OpenedFileQueue,
			&openFile->ListEntry,
			&PrimarySession->Thread.OpenedFileQSpinLock
			);

#if DBG
//	InterlockedIncrement(&LfsObjectCounts.OpenFileCount);
#endif

//	DebugTrace2( 0,Dbg,
//		("PrimarySession_AllocateOpenFile OpenFile = %p, OpenFileCount = %d\n", openFile, LfsObjectCounts.OpenFileCount));

	return openFile;
}

VOID
PrimarySession_FreeOpenFile(
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	)
{
	UNREFERENCED_PARAMETER(PrimarySession);

//	DebugTrace2( 0,Dbg,
//		("PrimarySession_FreeOpenFile OpenFile = %p, OpenFileCount = %d\n", OpenedFile, LfsObjectCounts.OpenFileCount));
	
	ASSERT(OpenedFile->ListEntry.Flink == OpenedFile->ListEntry.Blink);

	ASSERT(OpenedFile);
	ASSERT(OpenedFile->FileHandle == NULL);
	ASSERT(OpenedFile->FileObject == NULL);
	ASSERT(OpenedFile->EventHandle == NULL);
	//ASSERT(OpenedFile->CleanUp == TRUE);
	//ASSERT(OpenedFile->AlreadyClosed == TRUE);
	OpenedFile->FileHandle = 0;
	OpenedFile->PrimarySession = NULL;

#if DBG
//	InterlockedDecrement(&LfsObjectCounts.OpenFileCount);
#endif

	ExFreePoolWithTag(
		OpenedFile,
		OPEN_FILE_TAG
		);
}


POPEN_FILE
PrimarySession_FindOpenFile(
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U64			 OpenFileId
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeAcquireSpinLock(&PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql);

	openFile = NULL;

    for (listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
         listEntry = listEntry->Flink) 
	{

		openFile = CONTAINING_RECORD (listEntry, OPEN_FILE, ListEntry);

		if (openFile->OpenFileId == OpenFileId)
			break;

		openFile = NULL;
	}

	KeReleaseSpinLock(&PrimarySession->Thread.OpenedFileQSpinLock, oldIrql);

	return openFile;
}


static VOID
PrimarySessionThreadProc(
	IN PPRIMARY_SESSION PrimarySession
	)
{
	BOOLEAN		primarySessionTerminate = FALSE;
	NTSTATUS	status;
	_U16		slotIndex;
	PLIST_ENTRY	primarySessionRequestEntry;


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	DebugTrace2( 0, Dbg2, ("PrimarySessionThreadProc: Start PrimarySession = %p\n", PrimarySession) );
	
	PrimarySession_Reference( PrimarySession );

	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_INITIALIZING );
	
	InitializeListHead( &PrimarySession->Thread.OpenedFileQueue );
	KeInitializeSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock );

	KeInitializeEvent( &PrimarySession->Thread.WorkCompletionEvent, NotificationEvent, FALSE );

	for (slotIndex = 0; slotIndex < PrimarySession->SessionContext.SessionSlotCount; slotIndex ++) {

		PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_WAIT;
		PrimarySession->Thread.IdleSlotCount ++;
	}

	ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_INITIALIZING );
	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_START );
	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );

	KeSetEvent( &PrimarySession->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	status = LpxTdiRecvWithCompletionEvent( PrimarySession->ConnectionFileObject,
										   &PrimarySession->Thread.TdiReceiveContext,
										   (PUCHAR)&PrimarySession->Thread.NdfsRequestHeader,
										   sizeof(NDFS_REQUEST_HEADER),
										   0,
										   NULL,
										   NULL );

	if (NT_SUCCESS(status)) {

		PrimarySession->Thread.TdiReceiving = TRUE;
	
	} else {
	
		ASSERT( NDFAT_BUG );
		SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
		primarySessionTerminate = TRUE;
	} 

	while (primarySessionTerminate == FALSE) {

		PKEVENT				events[3];
		LONG				eventCount;
		NTSTATUS			eventStatus;
		LARGE_INTEGER		timeOut;


		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		eventCount = 0;

		events[eventCount++] = &PrimarySession->RequestEvent;
		events[eventCount++] = &PrimarySession->Thread.WorkCompletionEvent;
		
		if (FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN)) {
			
			if (PrimarySession->Thread.IdleSlotCount == PrimarySession->SessionContext.SessionSlotCount) {

				CloseOpenFiles( PrimarySession );
				KeSetEvent( &PrimarySession->Thread.ShutdownPrimarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
				primarySessionTerminate = TRUE;
				continue;
			}
		} 
		
		if (PrimarySession->Thread.TdiReceiving == TRUE) {

			ASSERT( PrimarySession->Thread.IdleSlotCount != 0 );
			events[eventCount++] = &PrimarySession->Thread.TdiReceiveContext.CompletionEvent;
		}

		ASSERT( eventCount <= THREAD_WAIT_OBJECTS );

		timeOut.QuadPart = -5*HZ;
		eventStatus = KeWaitForMultipleObjects( eventCount, 
												events, 
												WaitAny, 
												Executive, 
												KernelMode,
												TRUE,
												&timeOut,
												NULL );

		if (eventStatus == STATUS_TIMEOUT) {

			continue;
		}

		if (!NT_SUCCESS(eventStatus) || eventStatus >= eventCount) {

			ASSERT(NDFAT_UNEXPECTED);
			SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
			primarySessionTerminate = TRUE;
			continue;
		}
		
		KeClearEvent( events[eventStatus] );

		if (eventStatus == 0) {

			while (primarySessionRequestEntry = ExInterlockedRemoveHeadList( &PrimarySession->RequestQueue,
																			&PrimarySession->RequestQSpinLock)) {

				PPRIMARY_SESSION_REQUEST	primarySessionRequest;
			
				primarySessionRequest = CONTAINING_RECORD( primarySessionRequestEntry,
														   PRIMARY_SESSION_REQUEST,
														   ListEntry );

				if (primarySessionRequest->RequestType == PRIMARY_SESSION_REQ_DISCONNECT) {

					//DisconnectFromSecondary( PrimarySession );
					ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );

					if (primarySessionRequest->Synchronous == TRUE)
						KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferencePrimarySessionRequest( primarySessionRequest );
				
				} else if (primarySessionRequest->RequestType == PRIMARY_SESSION_REQ_DOWN) {
					
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_STOPED );
					primarySessionTerminate = TRUE;

					if (primarySessionRequest->Synchronous == TRUE)
						KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferencePrimarySessionRequest( primarySessionRequest );
				
				} else if (primarySessionRequest->RequestType == PRIMARY_SESSION_SHUTDOWN) {

					DebugTrace2( 0, Dbg2, ("PrimarySessionThreadProc: PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN\n") );
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN );

					ASSERT (primarySessionRequest->Synchronous == TRUE);

					PrimarySession->Thread.ShutdownPrimarySessionRequest = primarySessionRequest;
				
				} else {

					ASSERT( NDFAT_BUG );
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
				}
			}

			continue;
		
		} else if (eventStatus == 1) {

			while (TRUE) {

				_U16	slotIndex;
	
				for (slotIndex = 0; slotIndex < PrimarySession->SessionContext.SessionSlotCount; slotIndex ++) {

					if (PrimarySession->Thread.SessionSlot[slotIndex].State == SLOT_FINISH)
						break;
				}

				if (slotIndex == PrimarySession->SessionContext.SessionSlotCount)
					break;
			
				PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_WAIT;
				PrimarySession->Thread.IdleSlotCount ++;

				if (PrimarySession->Thread.SessionSlot[slotIndex].status == STATUS_SUCCESS) {

					PNDFS_REPLY_HEADER		ndfsReplyHeader;

					ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[slotIndex].ReplyMessageBuffer;
										
					PrimarySession->Thread.SessionSlot[slotIndex].status
						= SendNdfsWinxpMessage( PrimarySession,
												ndfsReplyHeader,
												PrimarySession->Thread.SessionSlot[slotIndex].NdfsWinxpReplyHeader,
												PrimarySession->Thread.SessionSlot[slotIndex].ReplyDataSize,
												slotIndex );

				}
	
				if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool) {

					ExFreePool(PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool);	
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool = NULL;
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
				}
		
				if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool) {

					ExFreePool(PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool);	
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool = NULL;
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
				}

				if (!(PrimarySession->Thread.SessionSlot[slotIndex].status == STATUS_SUCCESS || 
					 PrimarySession->Thread.SessionSlot[slotIndex].status == STATUS_PENDING)) {

					 SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
					 primarySessionTerminate = TRUE;
					 break;		
				 }
				
				if (PrimarySession->Thread.SessionState == SESSION_CLOSED) {

					primarySessionTerminate = TRUE;
					break;		
				}

				if (PrimarySession->Thread.SessionSlot[slotIndex].status == STATUS_SUCCESS) {

					if (PrimarySession->Thread.TdiReceiving == FALSE) {

						status = LpxTdiRecvWithCompletionEvent( PrimarySession->ConnectionFileObject,
															   &PrimarySession->Thread.TdiReceiveContext,
															   (PUCHAR)&PrimarySession->Thread.NdfsRequestHeader,
															   sizeof(NDFS_REQUEST_HEADER),
															   0,
															   NULL,
															   NULL );

						if (!NT_SUCCESS(status)) {

							ASSERT(NDFAT_BUG);
							SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
							primarySessionTerminate = TRUE;
							break;
						}
						
						PrimarySession->Thread.TdiReceiving = TRUE;
					}
				}
			}		
		
			continue;
		
		} else {

			ASSERT( eventStatus == 2 && !BooleanFlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN)  );  // Receive Event
	
			if (PrimarySession->Thread.TdiReceiveContext.Result != sizeof(NDFS_REQUEST_HEADER)) {

				DebugTrace2( 0, Dbg2,
							("DispatchRequest: Disconnected, PrimarySession = Data received:%d\n",
							PrimarySession->Thread.TdiReceiveContext.Result) );

				SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
				primarySessionTerminate = TRUE;
				
				continue;		
			}

			PrimarySession->Thread.TdiReceiving = FALSE;

			status = DispatchRequest( PrimarySession );

			if (!(status == STATUS_SUCCESS || status == STATUS_PENDING)) {

				primarySessionTerminate = TRUE;
				continue;		
			}

			if (PrimarySession->Thread.SessionState == SESSION_CLOSED) {

				primarySessionTerminate = TRUE;
				continue;		
			}

			if (status == STATUS_SUCCESS) {

				if (PrimarySession->Thread.TdiReceiving == FALSE) {

					status = LpxTdiRecvWithCompletionEvent( PrimarySession->ConnectionFileObject,
														   &PrimarySession->Thread.TdiReceiveContext,
														   (PUCHAR)&PrimarySession->Thread.NdfsRequestHeader,
														   sizeof(NDFS_REQUEST_HEADER),
														   0,
														   NULL,
														   NULL );

					if (!NT_SUCCESS(status)) {

						//ASSERT(NDFAT_BUG);
						SetFlag(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR);
						primarySessionTerminate = TRUE;
					}
					PrimarySession->Thread.TdiReceiving = TRUE;
				}
			}
			
			continue;
		}
	}

	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_STOPED );

	while (TRUE) {

		LARGE_INTEGER	timeOut;
		NTSTATUS		eventStatus;


		if (PrimarySession->Thread.IdleSlotCount == PrimarySession->SessionContext.SessionSlotCount)
			break;

		timeOut.QuadPart = -10*HZ;
		eventStatus = KeWaitForSingleObject( &PrimarySession->Thread.WorkCompletionEvent,
											 Executive,
											 KernelMode,
											 FALSE,
											 &timeOut );

		KeClearEvent( &PrimarySession->Thread.WorkCompletionEvent );

		if (eventStatus == STATUS_TIMEOUT) {

			ASSERT( NDFAT_UNEXPECTED );
			continue;
		}

		while (TRUE) {

			_U16	slotIndex;
	
			for (slotIndex = 0; slotIndex < PrimarySession->SessionContext.SessionSlotCount; slotIndex++) {

				if (PrimarySession->Thread.SessionSlot[slotIndex].State == SLOT_FINISH)
					break;
			}

			if (slotIndex == PrimarySession->SessionContext.SessionSlotCount)
				break;

			DebugTrace2( 0, Dbg, ("PrimarySessionThreadProc: eventStatus = %d\n", eventStatus) );
			
			PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_WAIT;
			PrimarySession->Thread.IdleSlotCount++;

			if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool) {

				ExFreePool( PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool );	
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool = NULL;
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
			}
		
			if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool) {

				ExFreePool( PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool );	
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool = NULL;
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
			}
		}		
	}
	
	if (!BooleanFlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED)) {

		DisconnectFromSecondary( PrimarySession );
		ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
		SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
	}

	CloseOpenFiles( PrimarySession );

	while (primarySessionRequestEntry = ExInterlockedRemoveHeadList( &PrimarySession->RequestQueue,
																	 &PrimarySession->RequestQSpinLock)) {

		PPRIMARY_SESSION_REQUEST	primarySessionRequest;
			
		primarySessionRequest = CONTAINING_RECORD( primarySessionRequestEntry,
												   PRIMARY_SESSION_REQUEST,
												   ListEntry );

		if (primarySessionRequest->Synchronous == TRUE)
			KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferencePrimarySessionRequest( primarySessionRequest );
	}

	if (PrimarySession->Irp) {

		PPRIMARY_SESSION	*outputBuffer;

		outputBuffer = PrimarySession->Irp->AssociatedIrp.SystemBuffer;
		*outputBuffer = PrimarySession;
		DebugTrace2( 0, Dbg2, ("FatFsdDeviceControl: IOCTL_INSERT_PRIMARY_SESSION %x\n", *outputBuffer) );		

		PrimarySession->Irp->IoStatus.Status = STATUS_SUCCESS;
		PrimarySession->Irp->IoStatus.Information = sizeof(PrimarySession);
        IoCompleteRequest( PrimarySession->Irp, IO_DISK_INCREMENT );
		PrimarySession->Irp = NULL;
	}

	DebugTrace2( 0, Dbg2,
				("NdFastFat NdFatPrimarySessionThreadProc: PsTerminateSystemThread PrimarySession = %p\n", 
				PrimarySession) );

	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_TERMINATED );

	PrimarySession_Dereference ( PrimarySession );

	PsTerminateSystemThread( STATUS_SUCCESS );
}


static VOID
DisconnectFromSecondary(
	IN	PPRIMARY_SESSION PrimarySession
	)
{
	ASSERT( PrimarySession->ConnectionFileHandle );
	ASSERT( PrimarySession->ConnectionFileObject );

	LpxTdiDisconnect( PrimarySession->ConnectionFileObject, 0 );

	return;
}	


static VOID
CloseOpenFiles(
	IN PPRIMARY_SESSION	PrimarySession
	)
{
	PLIST_ENTRY	openFileEntry;

	while(openFileEntry = ExInterlockedRemoveHeadList( &PrimarySession->Thread.OpenedFileQueue,
													   &PrimarySession->Thread.OpenedFileQSpinLock)) {

		POPEN_FILE openFile;
		NTSTATUS   closeStatus;
			

		openFile = CONTAINING_RECORD( openFileEntry, OPEN_FILE, ListEntry );

		//ASSERT( FALSE );

		if (openFile->CleanUp == FALSE) {
		
			TYPE_OF_OPEN	typeOfOpen;
			PVCB			vcb;
			PFCB			fcb;
			//PSCB			scb;
			PCCB			ccb;


			typeOfOpen = FatDecodeFileObject( openFile->FileObject, &vcb, &fcb, &ccb );
		
			if (typeOfOpen == UserFileOpen) {

				PIRP				irp;
				PFILE_OBJECT		fileObject;
				PDEVICE_OBJECT		deviceObject;
				KPROCESSOR_MODE		requestorMode;
				PIO_STACK_LOCATION	irpSp;
				BOOLEAN				synchronousIo;
				PKEVENT				eventObject = (PKEVENT) NULL;
				ULONG				keyValue = 0;
				LARGE_INTEGER		fileOffset = {0,0};
				PULONG				majorFunction;
				PETHREAD			currentThread;
				KEVENT				event;

				NDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;	
				PIRP						topLevelIrp;
				PRIMARY_REQUEST_INFO		primaryRequestInfo;
				NTSTATUS					cleanupStatus;

				do {

					synchronousIo = openFile->FileObject ? BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO) : TRUE;
					ASSERT( synchronousIo == TRUE );

					deviceObject = &PrimarySession->VolDo->DeviceObject;
					fileObject = openFile->FileObject;
					currentThread = PsGetCurrentThread ();
					ASSERT( deviceObject->StackSize >= 1 );
					irp = IoAllocateIrp( deviceObject->StackSize, TRUE );
					requestorMode = KernelMode;
				
					if (!irp) {

						ASSERT( NDFAT_INSUFFICIENT_RESOURCES );
						break;
					}

					irp->Tail.Overlay.OriginalFileObject = fileObject;
					irp->Tail.Overlay.Thread = currentThread;
					irp->Tail.Overlay.AuxiliaryBuffer = (PVOID) NULL;
					irp->RequestorMode = requestorMode;
					irp->PendingReturned = FALSE;
					irp->Cancel = FALSE;
					irp->CancelRoutine = (PDRIVER_CANCEL) NULL;
	
					irp->UserEvent = eventObject;
					irp->UserIosb = NULL; //&ioStatusBlock;
					irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL; //ApcRoutine;
					irp->Overlay.AsynchronousParameters.UserApcContext = NULL; //ApcContext;

					KeInitializeEvent( &event, NotificationEvent, FALSE );
			
					IoSetCompletionRoutine( irp,
											PrimaryCompletionRoutine,
											&event,
											TRUE,
											TRUE,
											TRUE );

					IoSetNextIrpStackLocation( irp );
					irpSp = IoGetCurrentIrpStackLocation( irp ); // = &currentIrpSp; // = IoGetNextIrpStackLocation( irp );
					majorFunction = (PULONG) (&irpSp->MajorFunction);
					*majorFunction = IRP_MJ_CLEANUP;
					irpSp->Control = (SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);
					irpSp->MinorFunction = IRP_MJ_CLEANUP;
					irpSp->FileObject = fileObject;
					irpSp->DeviceObject = deviceObject;
					irp->AssociatedIrp.SystemBuffer = (PVOID) NULL;
					irp->MdlAddress = (PMDL) NULL;

					ndfsWinxpRequestHeader.CleanUp.AllocationSize	= fcb->Header.AllocationSize.QuadPart;
					ndfsWinxpRequestHeader.CleanUp.FileSize			= fcb->Header.FileSize.QuadPart;
					ndfsWinxpRequestHeader.CleanUp.ValidDataLength	= fcb->Header.FileSize.QuadPart;
					ndfsWinxpRequestHeader.CleanUp.VaildDataToDisk	= fcb->Header.FileSize.QuadPart;

					primaryRequestInfo.PrimaryTag = 0xe2027482;
					primaryRequestInfo.PrimarySession = PrimarySession;
					primaryRequestInfo.NdfsWinxpRequestHeader = &ndfsWinxpRequestHeader;

					topLevelIrp = IoGetTopLevelIrp();
					ASSERT( topLevelIrp == NULL );
					IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

					cleanupStatus = FatFsdCleanup( PrimarySession->VolDo, irp );
				
					if (cleanupStatus == STATUS_PENDING) {

						KeWaitForSingleObject( &event,
											   Executive,
											   KernelMode,
											   FALSE,
											   NULL );

					}

					IoSetTopLevelIrp( topLevelIrp );

					cleanupStatus = irp->IoStatus.Status;
					ASSERT( cleanupStatus == STATUS_SUCCESS );
				
					if (irp->MdlAddress != NULL) {

						MmUnlockPages( irp->MdlAddress );
						IoFreeMdl( irp->MdlAddress );
					}

					IoFreeIrp( irp );

				} while (0);
			}

			openFile->CleanUp = TRUE;
		}
	
		ObDereferenceObject( openFile->FileObject );		
		openFile->FileObject = NULL;

		closeStatus = ZwClose( openFile->FileHandle );
		openFile->FileHandle = NULL;
		ASSERT( closeStatus == STATUS_SUCCESS );

		if (openFile->EventHandle) {

			closeStatus = ZwClose( openFile->EventHandle );
			openFile->EventHandle = NULL;
			ASSERT(closeStatus == STATUS_SUCCESS);
		}

		openFile->FileHandle = NULL;

		InitializeListHead( &openFile->ListEntry );
		PrimarySession_FreeOpenFile( PrimarySession, openFile );
	}
	
	return;
}


static NTSTATUS
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


	//ASSERT(ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize && PrimarySession->MessageSecurity == 0);

	if (ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize) {

#ifdef __ND_FAT_DES__
		int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
#endif

		RtlCopyMemory( NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol) );
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		NdfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
		NdfsReplyHeader->Tid		= PrimarySession->SessionContext.Tid;
		NdfsReplyHeader->Mid		= Mid;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->SessionContext.MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));

		ASSERT( NdfsReplyHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBufferLength );

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)NdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}
		
		if (PrimarySession->SessionContext.MessageSecurity == 0) {

			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)NdfsWinxpReplyHeader,
									 NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
									 NULL );

			return tdiStatus;
		}
			
#ifdef __ND_FAT_DES__

		if (NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
			DebugTrace2( 0, Dbg,
						("DispatchRequest: PrimarySession->RwDataSecurity = %d\n", PrimarySession->RwDataSecurity) );

		if (NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->RwDataSecurity == 0)
		{
			RtlCopyMemory(cryptWinxpRequestMessage, NdfsWinxpReplyHeader, sizeof(NDFS_WINXP_REPLY_HEADER));
			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)NdfsWinxpReplyHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
			ASSERT(desResult == IDOK);
			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)NdfsWinxpReplyHeader,
									 NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
									 NULL );
			}
			else {

			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
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

#endif
	}

	ASSERT( (_U8 *)NdfsWinxpReplyHeader == PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool );
	ASSERT( ReplyDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize );

	RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
	NdfsReplyHeader->Status		= NDFS_SUCCESS;
	NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
	NdfsReplyHeader->Splitted	= 1;
	NdfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
	NdfsReplyHeader->Tid		= PrimarySession->SessionContext.Tid;
	NdfsReplyHeader->Mid		= 0;
	NdfsReplyHeader->MessageSize 
			= sizeof(NDFS_REPLY_HEADER) 
			+ (PrimarySession->SessionContext.MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));
		
	tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)NdfsReplyHeader,
							 sizeof(NDFS_REPLY_HEADER),
							 NULL );

	if (tdiStatus != STATUS_SUCCESS) {
		
		return tdiStatus;
	} 

	if (PrimarySession->SessionContext.MessageSecurity)
	{
#ifdef __ND_FAT_DES__

		int desResult;
		_U8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, cryptWinxpRequestMessage, (_U8 *)NdfsWinxpReplyHeader, sizeof(NDFS_WINXP_REPLY_HEADER));
		ASSERT(desResult == IDOK);
		tdiStatus = SendMessage(
					PrimarySession->ConnectionFileObject,
					cryptWinxpRequestMessage,
					sizeof(NDFS_WINXP_REPLY_HEADER),
					NULL
					);
#endif	
	}
	else {

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)NdfsWinxpReplyHeader,
								 sizeof(NDFS_WINXP_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS)	{

			return tdiStatus;
		}
	} 

	remaninigDataSize = ReplyDataSize;

	while(1) {

		RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		NdfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
		NdfsReplyHeader->Tid		= PrimarySession->SessionContext.Tid;
		NdfsReplyHeader->Mid		= 0;
		NdfsReplyHeader->MessageSize 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->SessionContext.MessageSecurity ? ADD_ALIGN8(remaninigDataSize) : remaninigDataSize);

		if (remaninigDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize)
			NdfsReplyHeader->Splitted = 1;

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)NdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}

		if (PrimarySession->SessionContext.MessageSecurity)
		{
#ifdef __ND_FAT_DES__
			int desResult;
			_U8 *cryptNdfsWinxpReplyHeader = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							cryptNdfsWinxpReplyHeader, 
							(_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ? PrimarySession->SessionContext.SecondaryMaxDataSize : (NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER))
							);
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptNdfsWinxpReplyHeader,
							NdfsReplyHeader->Splitted ? PrimarySession->SessionContext.SecondaryMaxDataSize : (NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);
#endif
		}
		else
		{	
			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
									 NdfsReplyHeader->Splitted ? PrimarySession->SessionContext.SecondaryMaxDataSize : (NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
									 NULL );
		}
		
		if (tdiStatus != STATUS_SUCCESS) {
			return tdiStatus;
		}

		if (NdfsReplyHeader->Splitted)
			remaninigDataSize -= PrimarySession->SessionContext.SecondaryMaxDataSize;
		else
			return STATUS_SUCCESS;

		ASSERT( (_S32)remaninigDataSize > 0) ;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
ReceiveNdfsWinxpMessage(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*cryptWinxpRequestMessage;

	NTSTATUS					tdiStatus;
#ifdef __ND_FAT_DES__
	int							desResult;
#endif

	cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;	
	//ASSERT(ndfsRequestHeader->Splitted == 0 && ndfsRequestHeader->MessageSecurity == 0);
		
	if (ndfsRequestHeader->Splitted == 0)
		{
		ASSERT(ndfsRequestHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].RequestMessageBufferLength);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	
		if (ndfsRequestHeader->MessageSecurity == 0)
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

		ASSERT(ndfsRequestHeader->MessageSecurity == 1);
		
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						cryptWinxpRequestMessage,
							sizeof(NDFS_WINXP_REQUEST_HEADER),
							NULL
							);
			if (tdiStatus != STATUS_SUCCESS)
			{
			return tdiStatus;
			}

#ifdef __ND_FAT_DES__

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
#endif
		if (ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER))
		{
			if (ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE && ndfsRequestHeader->RwDataSecurity == 0)
			{
				tdiStatus = RecvMessage(
								PrimarySession->ConnectionFileObject,
								(_U8 *)(ndfsWinxpRequestHeader+1),
								ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
								NULL
								);
				if (tdiStatus != STATUS_SUCCESS)
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
				if (tdiStatus != STATUS_SUCCESS)
				{
					return tdiStatus;
				}

#ifdef __ND_FAT_DES__
				desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)(ndfsWinxpRequestHeader+1), cryptWinxpRequestMessage, ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER));
				ASSERT(desResult == IDOK);
#endif
			}
		}

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}

	ASSERT(ndfsRequestHeader->Splitted == 1);

//	if (ndfsRequestHeader->MessageSize > (PrimarySession->RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool 
			= ExAllocatePoolWithTag(
				NonPagedPool,
				PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength,
				PRIMARY_SESSION_BUFFERE_TAG
				);
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
		if (PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL) {
			DebugTrace2( 0, Dbg,	("ReceiveNdfsWinxpMessage: failed to allocate ExtendWinxpRequestMessagePool\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
	}
//	else
//		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);

	if (ndfsRequestHeader->MessageSecurity == 0)
	{
		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)ndfsWinxpRequestHeader,
						sizeof(NDFS_WINXP_REQUEST_HEADER),
						NULL
						);
		if (tdiStatus != STATUS_SUCCESS)
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

		if (tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
#ifdef __ND_FAT_DES__
		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
#endif
	}

	while(1)
	{
		PNDFS_REQUEST_HEADER	splitNdfsRequestHeader = &PrimarySession->Thread.SessionSlot[Mid].SplitNdfsRequestHeader;


		tdiStatus = RecvMessage(
						PrimarySession->ConnectionFileObject,
						(_U8 *)splitNdfsRequestHeader,
						sizeof(NDFS_REQUEST_HEADER),
						NULL
						);
		if (tdiStatus != STATUS_SUCCESS)
			return tdiStatus;

		if (!(
			PrimarySession->Thread.SessionState == SESSION_SETUP
			&& ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid
			&& ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid
			))
		{
			ASSERT(NDFAT_BUG);
			return STATUS_UNSUCCESSFUL;
		}

		if (ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->SessionContext.PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER)),
							NULL
								);
			if (tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
		}
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
			if (tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
#ifdef __ND_FAT_DES__

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							(_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize, 
							cryptWinxpRequestMessage, 
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->SessionContex.PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER))
								);
			ASSERT(desResult == IDOK);
#endif
		}

		if (splitNdfsRequestHeader->Splitted)
			continue;

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}
}


#endif