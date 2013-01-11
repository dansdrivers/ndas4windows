#include "LfsProc.h"


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

		KeInitializeEvent( &primarySession->ReadyEvent, NotificationEvent, FALSE );
	
		InitializeListHead( &primarySession->RequestQueue );
		KeInitializeSpinLock( &primarySession->RequestQSpinLock );
		KeInitializeEvent( &primarySession->RequestEvent, NotificationEvent, FALSE );

		InitializeListHead( &primarySession->NetdiskPartitionListEntry );

		primarySession->ThreadHandle = 0;
		primarySession->ThreadObject = NULL;

		primarySession->Thread.TdiReceiveContext.Irp = NULL;
		KeInitializeEvent( &primarySession->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );

		primarySession->ConnectionFileHandle	= ListenFileHandle;
		primarySession->ConnectionFileObject	= ListenFileObject;
		RtlCopyMemory( &primarySession->RemoteAddress, RemoteAddress, sizeof(LPX_ADDRESS) );
		primarySession->IsLocalAddress			= Lfs_IsLocalAddress(RemoteAddress);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("primarySession->ConnectionFileHandle = %p\n", primarySession->ConnectionFileHandle) );

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

		ExAcquireFastMutex( &GlobalLfs.FastMutex );
		primarySession->SessionContext.Uid = GlobalLfs.Uid++;
		ExReleaseFastMutex( &GlobalLfs.FastMutex );

		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

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
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,	("FreePrimarySessionRequest: PrimarySessionRequest freed\n") );
	}

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
SendNdfsWinxpMessage (
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

