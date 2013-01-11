#include "NtfsProc.h"

#if __NDAS_NTFS_PRIMARY__


#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('PftN')

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)


PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest (
	IN	BOOLEAN	Synchronous
	);

FORCEINLINE
NTSTATUS
QueueingPrimarySessionRequest (
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest,
	IN  BOOLEAN						FastMutexAcquired
	);


PPRIMARY_SESSION
PrimarySession_Create (
	IN  PIRP_CONTEXT			IrpContext,  
	IN	PVOLUME_DEVICE_OBJECT	VolDo,		 
	IN  PSESSION_INFORMATION	SessionInformation
	)
{
	PPRIMARY_SESSION	primarySession;
 	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			status;
	LARGE_INTEGER		timeOut;

		
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	VolDo_Reference( VolDo );

	primarySession = FsRtlAllocatePoolWithTag( NonPagedPool, sizeof(PRIMARY_SESSION), NDASNTFS_ALLOC_TAG );
	
	if (primarySession == NULL) {

		ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
		VolDo_Dereference( VolDo );
		return NULL;
	}

	try {
	
		RtlZeroMemory( primarySession, sizeof(PRIMARY_SESSION) );

		primarySession->Flags = PRIMARY_SESSION_FLAG_INITIALIZING;

		primarySession->ReferenceCount = 1;
		primarySession->VolDo = VolDo;
		
		ExInitializeFastMutex( &primarySession->FastMutex );
		
		InitializeListHead( &primarySession->ListEntry );

		primarySession->NetdiskPartitionInformation = SessionInformation->NetdiskPartitionInformation;
	
		RtlInitEmptyUnicodeString( &primarySession->NetdiskPartitionInformation.VolumeName,
								   primarySession->NetdiskPartitionInformation.VolumeNameBuffer,
								   sizeof(primarySession->NetdiskPartitionInformation.VolumeNameBuffer) );

		if (RtlAppendUnicodeStringToString( &primarySession->NetdiskPartitionInformation.VolumeName,
											&SessionInformation->NetdiskPartitionInformation.VolumeName) != STATUS_SUCCESS) {

			ASSERT( NDASNTFS_UNEXPECTED );
		}

		ASSERT( primarySession->NetdiskPartitionInformation.VolumeName.Buffer == primarySession->NetdiskPartitionInformation.VolumeNameBuffer );

		primarySession->ConnectionFileHandle		= SessionInformation->ConnectionFileHandle;
		primarySession->ConnectionFileObject		= SessionInformation->ConnectionFileObject;

		LpxTdiV2MoveOverlappedContext( &primarySession->Thread.ReceiveOverlapped, &SessionInformation->OverlappedContext );

		primarySession->RemoteAddress				= SessionInformation->RemoteAddress;
		primarySession->IsLocalAddress				= SessionInformation->IsLocalAddress;

		primarySession->SessionContext = SessionInformation->SessionContext;

		primarySession->SessionContext.PrimaryMaxDataSize	= DEFAULT_NDAS_MAX_DATA_SIZE; //SessionInformation->PrimaryMaxDataSize;
		primarySession->SessionContext.SecondaryMaxDataSize	= DEFAULT_NDAS_MAX_DATA_SIZE; // SessionInformation->SecondaryMaxDataSize;

		DebugTrace( 0, Dbg2, ("primarySession->ConnectionFileHandle = %x " 
							   "primarySession->SessionContext.PrimaryMaxDataSize = %x primarySession->SessionContext.SecondaryMaxDataSize = %x\n", 
							    primarySession->ConnectionFileHandle, primarySession->SessionContext.PrimaryMaxDataSize, primarySession->SessionContext.SecondaryMaxDataSize) );

		KeInitializeEvent( &primarySession->ReadyEvent, NotificationEvent, FALSE );
	
		InitializeListHead( &primarySession->RequestQueue );
		KeInitializeSpinLock( &primarySession->RequestQSpinLock );
		KeInitializeEvent( &primarySession->RequestEvent, NotificationEvent, FALSE );

		primarySession->ThreadHandle = 0;
		primarySession->ThreadObject = NULL;

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

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &primarySession->ReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );


		if (!NT_SUCCESS(status)) {

			leave;
		}

		KeClearEvent( &primarySession->ReadyEvent );

		DebugTrace( 0, Dbg, ("PrimarySession_Create: The primary thread are ready\n") );
	
		DebugTrace( 0, Dbg2, ("Fat PrimarySession_Create: primarySession = %p\n", primarySession) );
	
	} finally {

		if (AbnormalTermination()) {

			status = IrpContext->ExceptionStatus;
		}

		if (!NT_SUCCESS(status)) {

			ASSERT( NDASNTFS_UNEXPECTED );
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
	DebugTrace( 0, Dbg2, ("PrimarySession_Close: PrimarySession = %p\n", PrimarySession) );

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );
	if (FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP)) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		return;
	}

	SetFlag( PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
	
	if (PrimarySession->ThreadHandle == NULL) {

		NDASNTFS_ASSERT( FALSE );
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

		timeOut.QuadPart = - NDASNTFS_TIME_OUT;
		ntStatus = KeWaitForSingleObject( PrimarySession->ThreadObject,
										  Executive,
										  KernelMode,
										  FALSE,
										  &timeOut );

		if (ntStatus == STATUS_SUCCESS) {

		    DebugTrace( 0, Dbg, ("PrimarySession_Close: thread stoped\n") );

			ObDereferenceObject( PrimarySession->ThreadObject );

			PrimarySession->ThreadHandle = NULL;
			PrimarySession->ThreadObject = NULL;
				
		} else {

			NDASNTFS_ASSERT( FALSE );
			return;
		}
	}

#if 0
    interval.QuadPart = (5 * DELAY_ONE_SECOND);      //delay 5 seconds
    KeDelayExecutionThread( KernelMode, FALSE, &interval );
#endif

	if (PrimarySession->ConnectionFileHandle) {

		LpxTdiV2DisassociateAddress( PrimarySession->ConnectionFileObject );
		LpxTdiV2CloseConnection( PrimarySession->ConnectionFileHandle, 
								 PrimarySession->ConnectionFileObject,
								 &PrimarySession->Thread.ReceiveOverlapped );
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

	DebugTrace( 0, Dbg2, ("PrimarySession_FileSystemShutdown: PrimarySession = %p\n", PrimarySession) );
		
	primarySessionRequest = AllocPrimarySessionRequest( TRUE );
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_SHUTDOWN;

	QueueingPrimarySessionRequest( PrimarySession, primarySessionRequest, FALSE );

	timeOut.QuadPart = -NDASNTFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		DebugTrace( 0, Dbg, ("PrimarySession_FileSystemShutdown: thread shutdown\n") );		
	
	} else {

		NDASNTFS_ASSERT( FALSE );
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

	DebugTrace( 0, Dbg2, 
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

	timeOut.QuadPart = -NDASNTFS_QUERY_REMOVE_TIME_OUT;
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

		DebugTrace( 0, Dbg, ("PrimarySession_CloseFiles: thread close files\n") );

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

	DebugTrace( 0, Dbg2, 
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

	timeOut.QuadPart = -NDASNTFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		DebugTrace( 0, Dbg, ("PrimarySession_CloseFiles: thread close files\n") );		
	
	} else {

		NDASNTFS_ASSERT( FALSE );
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

	DebugTrace( 0, Dbg2, 
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

	timeOut.QuadPart = -NDASNTFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primarySessionRequest->CompleteEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );
	
	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primarySessionRequest->CompleteEvent );
	DereferencePrimarySessionRequest( primarySessionRequest );

	if (ntStatus == STATUS_SUCCESS) {

		DebugTrace( 0, Dbg, ("PrimarySession_CancelStopping: thread close files\n") );		
	
	} else {

		NDASNTFS_ASSERT( FALSE );
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
		
		ExFreePoolWithTag( PrimarySession, NDASNTFS_ALLOC_TAG	);

		DebugTrace( 0, Dbg2,
					("PrimarySession_Dereference: PrimarySession = %p is Freed\n", PrimarySession) );

		VolDo_Dereference( volDo );
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

		ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
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
QueueingPrimarySessionRequest (
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
		DebugTrace( 0, Dbg,	("FreePrimarySessionRequest: PrimarySessionRequest freed\n") );
	}

	return;
}


POPEN_FILE
PrimarySession_AllocateOpenFile (
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject
	) 
{
	POPEN_FILE	openFile;


	openFile = ExAllocatePoolWithTag( NonPagedPool,
									  sizeof(OPEN_FILE),
									  OPEN_FILE_TAG );
	
	if (openFile == NULL) {

		ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( openFile,	sizeof(OPEN_FILE) );

	openFile->FileHandle = FileHandle;
	openFile->FileObject = FileObject;

	openFile->PrimarySession = PrimarySession;

	InitializeListHead( &openFile->ListEntry );
	
	ExInterlockedInsertHeadList( &PrimarySession->Thread.OpenedFileQueue,
								 &openFile->ListEntry,
								 &PrimarySession->Thread.OpenedFileQSpinLock );

	DebugTrace( 0,Dbg,
				 ("PrimarySession_AllocateOpenFile OpenFile = %p\n", openFile) );

	return openFile;
}

VOID
PrimarySession_FreeOpenFile (
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	)
{
	UNREFERENCED_PARAMETER( PrimarySession );

	DebugTrace( 0, Dbg,
				 ("PrimarySession_FreeOpenFile OpenFile = %p\n", OpenedFile) );
	
	ASSERT( OpenedFile->ListEntry.Flink == OpenedFile->ListEntry.Blink );
	ASSERT( OpenedFile );
	ASSERT( OpenedFile->FileHandle == NULL );
	ASSERT( OpenedFile->FileObject == NULL );
	ASSERT( OpenedFile->EventHandle == NULL );
	//ASSERT(OpenedFile->CleanUp == TRUE);
	//ASSERT(OpenedFile->AlreadyClosed == TRUE);
	OpenedFile->FileHandle = 0;
	OpenedFile->PrimarySession = NULL;

	ExFreePoolWithTag( OpenedFile, OPEN_FILE_TAG );
}


POPEN_FILE
PrimarySession_FindOpenFile (
	IN  PPRIMARY_SESSION PrimarySession,
	IN	_U64			 OpenFileId
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeAcquireSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql );

	openFile = NULL;

    for (listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
         listEntry = listEntry->Flink) {

		openFile = CONTAINING_RECORD(listEntry, OPEN_FILE, ListEntry);

		if (openFile->OpenFileId == OpenFileId)
			break;

		openFile = NULL;
	}

	KeReleaseSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, oldIrql );

	return openFile;
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


	//ASSERT(ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize && PrimarySession->MessageSecurity == 0);

	if(ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize) {

#if __NDAS_NTFS_DES__
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

		if(tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}
		
		if(PrimarySession->SessionContext.MessageSecurity == 0) {

			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
									 (_U8 *)NdfsWinxpReplyHeader,
									 NdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
									 NULL );

			return tdiStatus;
		}
			
#if __NDAS_NTFS_DES__

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
			DebugTrace( 0, Dbg,
						("DispatchRequest: PrimarySession->RwDataSecurity = %d\n", PrimarySession->RwDataSecurity) );

		if(NdfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->RwDataSecurity == 0)
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

	if(tdiStatus != STATUS_SUCCESS) {
		
		return tdiStatus;
	} 


	if(PrimarySession->SessionContext.MessageSecurity)
	{
#if __NDAS_NTFS_DES__

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

		if(tdiStatus != STATUS_SUCCESS)	{

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

		if(remaninigDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize)
			NdfsReplyHeader->Splitted = 1;

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
								 (_U8 *)NdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL );

		if(tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}

		if(PrimarySession->SessionContext.MessageSecurity)
		{
#if __NDAS_NTFS_DES__
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
		
		if(tdiStatus != STATUS_SUCCESS) {
			return tdiStatus;
		}

		if(NdfsReplyHeader->Splitted)
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
#if __NDAS_NTFS_DES__
	int							desResult;
#endif

	cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;	
	//ASSERT(ndfsRequestHeader->Splitted == 0 && ndfsRequestHeader->MessageSecurity == 0);
		
	if(ndfsRequestHeader->Splitted == 0)
		{
		ASSERT(ndfsRequestHeader->MessageSize <= PrimarySession->Thread.SessionSlot[Mid].RequestMessageBufferLength);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	
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

#if __NDAS_NTFS_DES__

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
#endif
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

#if __NDAS_NTFS_DES__
				desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (_U8 *)(ndfsWinxpRequestHeader+1), cryptWinxpRequestMessage, ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER));
				ASSERT(desResult == IDOK);
#endif
			}
		}

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}

	ASSERT(ndfsRequestHeader->Splitted == 1);

//	if(ndfsRequestHeader->MessageSize > (PrimarySession->RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool 
			= ExAllocatePoolWithTag(
				NonPagedPool,
				PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength,
				PRIMARY_SESSION_BUFFERE_TAG
				);
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
		if(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL) {
			DebugTrace( 0, Dbg,	("ReceiveNdfsWinxpMessage: failed to allocate ExtendWinxpRequestMessagePool\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
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
#if __NDAS_NTFS_DES__
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
		if(tdiStatus != STATUS_SUCCESS)
			return tdiStatus;

		if(!(
			PrimarySession->Thread.SessionState == SESSION_SETUP
			&& ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid
			&& ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid
			))
		{
			ASSERT(NDASNTFS_BUG);
			return STATUS_UNSUCCESSFUL;
		}

		if(ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							PrimarySession->ConnectionFileObject,
							(_U8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize,
							splitNdfsRequestHeader->Splitted 
								? PrimarySession->SessionContext.PrimaryMaxDataSize
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
								? PrimarySession->SessionContext.PrimaryMaxDataSize
								: (splitNdfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER)),
							NULL
							);
			if(tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
#if __NDAS_NTFS_DES__

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

		if(splitNdfsRequestHeader->Splitted)
			continue;

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}
}


#endif
