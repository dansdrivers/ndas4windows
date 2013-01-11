#include "NtfsProc.h"

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('PftN')

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)



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

		NDAS_ASSERT( NDASNTFS_INSUFFICIENT_RESOURCES );
		VolDo_Dereference( VolDo );
		return NULL;
	}

	try {
	
		RtlZeroMemory( primarySession, sizeof(PRIMARY_SESSION) );

		primarySession->ReferenceCount = 1;

		ExInitializeFastMutex( &primarySession->FastMutex );

		primarySession->Flags = PRIMARY_SESSION_FLAG_INITIALIZING;

		primarySession->PrimarySessionId = (PRIMARY_SESSION_ID)primarySession;

		primarySession->VolDo = VolDo;
		
		InitializeListHead( &primarySession->ListEntry );

		KeInitializeEvent( &primarySession->ReadyEvent, NotificationEvent, FALSE );
	
		InitializeListHead( &primarySession->RequestQueue );
		KeInitializeSpinLock( &primarySession->RequestQSpinLock );
		KeInitializeEvent( &primarySession->RequestEvent, NotificationEvent, FALSE );

		primarySession->ThreadHandle = 0;
		primarySession->ThreadObject = NULL;

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

		NdasFcInitialize( &primarySession->SendNdasFcStatistics );
		NdasFcInitialize( &primarySession->RecvNdasFcStatistics );

		DebugTrace2( 0, Dbg2, ("primarySession->ConnectionFileHandle = %x " 
							   "primarySession->SessionContext.PrimaryMaxDataSize = %x primarySession->SessionContext.SecondaryMaxDataSize = %x\n", 
							    primarySession->ConnectionFileHandle, primarySession->SessionContext.PrimaryMaxDataSize, primarySession->SessionContext.SecondaryMaxDataSize) );

		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		primarySession->Thread.SessionState = SESSION_TREE_CONNECT;
	
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

		ExInterlockedInsertTailList( &VolDo->PrimarySessionQueue,
									 &primarySession->ListEntry,
									 &VolDo->PrimarySessionQSpinLock );

		DebugTrace2( 0, Dbg, ("PrimarySession_Create: The primary thread are ready\n") );
		DebugTrace2( 0, Dbg, ("Fat PrimarySession_Create: primarySession = %p\n", primarySession) );
	
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
	DebugTrace2( 0, Dbg, ("PrimarySession_Close: PrimarySession = %p\n", PrimarySession) );

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );
	if (FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP)) {

		ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
		return;
	}

	SetFlag( PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOP );

	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );
	
	if (PrimarySession->ThreadHandle == NULL) {

		ASSERT( NDASNTFS_BUG );
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

		    DebugTrace2( 0, Dbg, ("PrimarySession_Close: thread stoped\n") );

			ObDereferenceObject( PrimarySession->ThreadObject );

			PrimarySession->ThreadHandle = NULL;
			PrimarySession->ThreadObject = NULL;
				
		} else {

			ASSERT( NDASNTFS_BUG );
			return;
		}
	}

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

	DebugTrace2( 0, Dbg2, 
				   ("PrimarySession_FileSystemShutdown: PrimarySession = %p\n", PrimarySession) );
		
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

		DebugTrace2( 0, Dbg2, ("PrimarySession_FileSystemShutdown: thread shutdown\n") );		
	
	} else {

		ASSERT( NDASNTFS_BUG );
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

	DebugTrace2( 0, Dbg2, 
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

		DebugTrace2( 0, Dbg, ("PrimarySession_CloseFiles: thread close files\n") );

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

	DebugTrace2( 0, Dbg2, 
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

		DebugTrace2( 0, Dbg, ("PrimarySession_CloseFiles: thread close files\n") );		
	
	} else {

		ASSERT( NDASNTFS_BUG );
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

	DebugTrace2( 0, Dbg2, 
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

		DebugTrace2( 0, Dbg, ("PrimarySession_CancelStopping: thread close files\n") );		
	
	} else {

		ASSERT( NDASNTFS_BUG );
		PrimarySession_Dereference( PrimarySession );
		return;
	}

	PrimarySession_Dereference( PrimarySession );

	return;
}

VOID
PrimarySession_SurpriseRemoval (
	IN 	PPRIMARY_SESSION	PrimarySession
	)
{
	PPRIMARY_SESSION_REQUEST	primarySessionRequest;
	NTSTATUS					ntStatus;
	LARGE_INTEGER				timeOut;

	
	PrimarySession_Reference( PrimarySession );

	DebugTrace2( 0, Dbg2, 
				   ("PrimarySession_SurpriseRemoval: PrimarySession = %p\n", PrimarySession) );

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
	primarySessionRequest->RequestType = PRIMARY_SESSION_REQ_SURPRISE_REMOVAL;
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

		DebugTrace2( 0, Dbg, ("PrimarySession_SurpriseRemoval: thread close files\n") );

		ASSERT( FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_SURPRISE_REMOVAL) );
	
	} else {

		ASSERT( ntStatus == STATUS_TIMEOUT );
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
		PVOLUME_DEVICE_OBJECT	volDo = PrimarySession->VolDo;	

		KeAcquireSpinLock( &volDo->PrimarySessionQSpinLock, &oldIrql );
		RemoveEntryList( &PrimarySession->ListEntry );
		KeReleaseSpinLock( &volDo->PrimarySessionQSpinLock, oldIrql );
				
		ExFreePoolWithTag( PrimarySession, NDASNTFS_ALLOC_TAG );

		DebugTrace2( 0, Dbg,
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
		DebugTrace2( 0, Dbg,	("FreePrimarySessionRequest: PrimarySessionRequest freed\n") );
	}

	return;
}


NTSTATUS
SendNdfsWinxpMessage (
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN UINT32						ReplyDataSize,
	IN UINT16						Mid
	)
{
	NTSTATUS	tdiStatus;
	UINT32		remaninigDataSize;		


	//ASSERT(ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize && PrimarySession->MessageSecurity == 0);

	if (ReplyDataSize <= PrimarySession->SessionContext.SecondaryMaxDataSize) {

#if __NDAS_NTFS_DES__
		int desResult;
		UINT8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
#endif

		RtlCopyMemory( NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol) );
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		NdfsReplyHeader->Uid2		= HTONS(PrimarySession->SessionContext.Uid);
		NdfsReplyHeader->Tid2		= HTONS(PrimarySession->SessionContext.Tid);
		NdfsReplyHeader->Mid2		= HTONS(Mid);
		NdfsReplyHeader->MessageSize4 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->SessionContext.MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));
		
		NdfsReplyHeader->MessageSize4 = NTOHL(NdfsReplyHeader->MessageSize4);

		ASSERT( NTOHL(NdfsReplyHeader->MessageSize4) <= PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBufferLength );

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							     &PrimarySession->SendNdasFcStatistics,
							     NULL,
								 (UINT8 *)NdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER) );

		if (tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}
		
		if (PrimarySession->SessionContext.MessageSecurity == 0) {

			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							         &PrimarySession->SendNdasFcStatistics,
							         NULL,
									 (UINT8 *)NdfsWinxpReplyHeader,
									 NTOHL(NdfsReplyHeader->MessageSize4) - sizeof(NDFS_REPLY_HEADER) );

			return tdiStatus;
		}
			
#if __NDAS_NTFS_DES__

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
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (UINT8 *)NdfsWinxpReplyHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
			ASSERT(desResult == IDOK);
			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
									 (UINT8 *)NdfsWinxpReplyHeader,
									 NTOHL(NdfsReplyHeader->MessageSize4) - sizeof(NDFS_REPLY_HEADER),
									 NULL );
			}
			else {

			RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
			RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
			//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
			desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, cryptWinxpRequestMessage, (UINT8 *)NdfsWinxpReplyHeader, NTOHL(NdfsReplyHeader->MessageSize4)-sizeof(NDFS_REPLY_HEADER));
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptWinxpRequestMessage,
							NTOHL(NdfsReplyHeader->MessageSize4) - sizeof(NDFS_REPLY_HEADER),
							NULL
							);
		}

		return tdiStatus;

#endif
	}

	ASSERT( (UINT8 *)NdfsWinxpReplyHeader == PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool );
	ASSERT( ReplyDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize );

	RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
	NdfsReplyHeader->Status		= NDFS_SUCCESS;
	NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
	NdfsReplyHeader->Splitted	= 1;
	NdfsReplyHeader->Uid2		= HTONS(PrimarySession->SessionContext.Uid);
	NdfsReplyHeader->Tid2		= HTONS(PrimarySession->SessionContext.Tid);
	NdfsReplyHeader->Mid2		= 0;
	NdfsReplyHeader->MessageSize4 
			= sizeof(NDFS_REPLY_HEADER) 
			+ (PrimarySession->SessionContext.MessageSecurity ? ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize) : (sizeof(NDFS_WINXP_REPLY_HEADER) + ReplyDataSize));
		
	NdfsReplyHeader->MessageSize4 = NTOHL(NdfsReplyHeader->MessageSize4);

	tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							 &PrimarySession->SendNdasFcStatistics,
							 NULL,
							 (UINT8 *)NdfsReplyHeader,
							 sizeof(NDFS_REPLY_HEADER) );

	if (tdiStatus != STATUS_SUCCESS) {
		
		return tdiStatus;
	} 

	if (PrimarySession->SessionContext.MessageSecurity)
	{
#if __NDAS_NTFS_DES__

		int desResult;
		UINT8 *cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_ENCRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, cryptWinxpRequestMessage, (UINT8 *)NdfsWinxpReplyHeader, sizeof(NDFS_WINXP_REPLY_HEADER));
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
							     &PrimarySession->SendNdasFcStatistics,
							     NULL,
								 (UINT8 *)NdfsWinxpReplyHeader,
								 sizeof(NDFS_WINXP_REPLY_HEADER) );

		if (tdiStatus != STATUS_SUCCESS)	{

			return tdiStatus;
		}
	} 

	remaninigDataSize = ReplyDataSize;

	while(1) {

		RtlCopyMemory(NdfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsReplyHeader->Protocol));
		NdfsReplyHeader->Status		= NDFS_SUCCESS;
		NdfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		NdfsReplyHeader->Uid2		= HTONS(PrimarySession->SessionContext.Uid);
		NdfsReplyHeader->Tid2		= HTONS(PrimarySession->SessionContext.Tid);
		NdfsReplyHeader->Mid2		= 0;
		NdfsReplyHeader->MessageSize4 
				= sizeof(NDFS_REPLY_HEADER) 
				+ (PrimarySession->SessionContext.MessageSecurity ? ADD_ALIGN8(remaninigDataSize) : remaninigDataSize);

		NdfsReplyHeader->MessageSize4 = NTOHL(NdfsReplyHeader->MessageSize4);

		if (remaninigDataSize > PrimarySession->SessionContext.SecondaryMaxDataSize)
			NdfsReplyHeader->Splitted = 1;

		tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							     &PrimarySession->SendNdasFcStatistics,
							     NULL,
								 (UINT8 *)NdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER) );

		if (tdiStatus != STATUS_SUCCESS) {

			return tdiStatus;
		}

		if (PrimarySession->SessionContext.MessageSecurity)
		{
#if __NDAS_NTFS_DES__
			int desResult;
			UINT8 *cryptNdfsWinxpReplyHeader = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							cryptNdfsWinxpReplyHeader, 
							(UINT8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
							NdfsReplyHeader->Splitted ? PrimarySession->SessionContext.SecondaryMaxDataSize : (NTOHL(NdfsReplyHeader->MessageSize4) - sizeof(NDFS_REPLY_HEADER))
							);
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
							PrimarySession->ConnectionFileObject,
							cryptNdfsWinxpReplyHeader,
							NdfsReplyHeader->Splitted ? PrimarySession->SessionContext.SecondaryMaxDataSize : (NTOHL(NdfsReplyHeader->MessageSize4) - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);
#endif
		}
		else
		{	
			tdiStatus = SendMessage( PrimarySession->ConnectionFileObject,
							         &PrimarySession->SendNdasFcStatistics,
									 NULL,
									 (UINT8 *)(NdfsWinxpReplyHeader+1) + (ReplyDataSize - remaninigDataSize), 
									 NdfsReplyHeader->Splitted ? PrimarySession->SessionContext.SecondaryMaxDataSize : (NTOHL(NdfsReplyHeader->MessageSize4) - sizeof(NDFS_REPLY_HEADER)) );
		}
		
		if (tdiStatus != STATUS_SUCCESS) {
			return tdiStatus;
		}

		if (NdfsReplyHeader->Splitted)
			remaninigDataSize -= PrimarySession->SessionContext.SecondaryMaxDataSize;
		else
			return STATUS_SUCCESS;

		ASSERT( (INT32)remaninigDataSize > 0) ;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
ReceiveNdfsWinxpMessage (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  UINT16				Mid
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	UINT8						*cryptWinxpRequestMessage;

	NTSTATUS					tdiStatus;
#if __NDAS_NTFS_DES__
	int							desResult;
#endif

	cryptWinxpRequestMessage = PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;	
	//ASSERT(ndfsRequestHeader->Splitted == 0 && ndfsRequestHeader->MessageSecurity == 0);
		
	if (ndfsRequestHeader->Splitted == 0) {

		ASSERT( NTOHL(ndfsRequestHeader->MessageSize4) <= PrimarySession->Thread.SessionSlot[Mid].RequestMessageBufferLength );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	
		if (ndfsRequestHeader->MessageSecurity == 0) {

			tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
									 &PrimarySession->RecvNdasFcStatistics,
									 NULL,
									 (UINT8 *)ndfsWinxpRequestHeader,
									 NTOHL(ndfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER) );
	
			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;
	
			return tdiStatus;
		}

		ASSERT(ndfsRequestHeader->MessageSecurity == 1);
		
		tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
							     &PrimarySession->RecvNdasFcStatistics,
							     NULL,
								 cryptWinxpRequestMessage,
								 sizeof(NDFS_WINXP_REQUEST_HEADER) );

			if (tdiStatus != STATUS_SUCCESS) {

				return tdiStatus;
			}

#if __NDAS_NTFS_DES__

		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (UINT8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
#endif
		if (NTOHL(ndfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)) {

			if (ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE && ndfsRequestHeader->RwDataSecurity == 0) {

				tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
										 &PrimarySession->RecvNdasFcStatistics,
										 NULL,
										 (UINT8 *)(ndfsWinxpRequestHeader+1),
										 NTOHL(ndfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER) );

				if (tdiStatus != STATUS_SUCCESS) {

					return tdiStatus;
				}
			
			} else {

				tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
										 &PrimarySession->RecvNdasFcStatistics,
										 NULL,
										 cryptWinxpRequestMessage,
										 NTOHL(ndfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER) );

				if (tdiStatus != STATUS_SUCCESS) {

					return tdiStatus;
				}

#if __NDAS_NTFS_DES__
				desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (UINT8 *)(ndfsWinxpRequestHeader+1), cryptWinxpRequestMessage, ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER));
				ASSERT(desResult == IDOK);
#endif
			}
		}

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader = ndfsWinxpRequestHeader;

		return STATUS_SUCCESS;
	}

	ASSERT( ndfsRequestHeader->Splitted == 1 );

//	if (ndfsRequestHeader->MessageSize > (PrimarySession->RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER)))
	{
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength = NTOHL(ndfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool 
			= ExAllocatePoolWithTag(
				NonPagedPool,
				PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePoolLength,
				PRIMARY_SESSION_BUFFERE_TAG
				);
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
		if (PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool == NULL) {
			DebugTrace2( 0, Dbg,	("ReceiveNdfsWinxpMessage: failed to allocate ExtendWinxpRequestMessagePool\n"));

			NDAS_ASSERT( FALSE );
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpRequestMessagePool);
	}
//	else
//		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);

	if (ndfsRequestHeader->MessageSecurity == 0)
	{
		tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
								 &PrimarySession->RecvNdasFcStatistics,
								 NULL,
								 (UINT8 *)ndfsWinxpRequestHeader,
								 sizeof(NDFS_WINXP_REQUEST_HEADER) );

		if (tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
	}
	else
	{
		tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
								 &PrimarySession->RecvNdasFcStatistics,
								 NULL,
								 cryptWinxpRequestMessage,
								 sizeof(NDFS_WINXP_REQUEST_HEADER) );

		if (tdiStatus != STATUS_SUCCESS)
		{
			return tdiStatus;
		}
#if __NDAS_NTFS_DES__
		RtlZeroMemory(&PrimarySession->DesCbcContext, sizeof(PrimarySession->DesCbcContext));
		RtlZeroMemory(PrimarySession->Iv, sizeof(PrimarySession->Iv));
		//DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartition->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		DES_CBCInit(&PrimarySession->DesCbcContext, PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, PrimarySession->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&PrimarySession->DesCbcContext, (UINT8 *)ndfsWinxpRequestHeader, cryptWinxpRequestMessage, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);
#endif
	}

	while(1)
	{
		PNDFS_REQUEST_HEADER	splitNdfsRequestHeader = &PrimarySession->Thread.SessionSlot[Mid].SplitNdfsRequestHeader;


		tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
								 &PrimarySession->SendNdasFcStatistics,
								 NULL,
								 (UINT8 *)splitNdfsRequestHeader,
								 sizeof(NDFS_REQUEST_HEADER) );

		if (tdiStatus != STATUS_SUCCESS)
			return tdiStatus;

		if (!(
			PrimarySession->Thread.SessionState == SESSION_SETUP
			&& NTOHS(ndfsRequestHeader->Uid2) == PrimarySession->SessionContext.Uid
			&& NTOHS(ndfsRequestHeader->Tid2) == PrimarySession->SessionContext.Tid
			))
		{
			ASSERT(NDASNTFS_BUG);
			return STATUS_UNSUCCESSFUL;
		}

		if (ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
									 &PrimarySession->RecvNdasFcStatistics,
									 NULL,
									 (UINT8 *)ndfsWinxpRequestHeader + NTOHL(ndfsRequestHeader->MessageSize4) - NTOHL(splitNdfsRequestHeader->MessageSize4),
									 splitNdfsRequestHeader->Splitted 
										? PrimarySession->SessionContext.PrimaryMaxDataSize
											: (NTOHL(splitNdfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER)) );

			if (tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
		}
		else
		{
			tdiStatus = RecvMessage( PrimarySession->ConnectionFileObject,
									 &PrimarySession->RecvNdasFcStatistics,
									 NULL,
									 cryptWinxpRequestMessage,
									 splitNdfsRequestHeader->Splitted 
										? PrimarySession->SessionContext.PrimaryMaxDataSize
											: (NTOHL(splitNdfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER)) );

			if (tdiStatus != STATUS_SUCCESS)
				return tdiStatus;
#if __NDAS_NTFS_DES__

			desResult = DES_CBCUpdate(
							&PrimarySession->DesCbcContext, 
							(UINT8 *)ndfsWinxpRequestHeader + ndfsRequestHeader->MessageSize - splitNdfsRequestHeader->MessageSize, 
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

NTSTATUS
PrimarySession_CloseFile (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	HANDLE				FileHandle
	)
{
	NTSTATUS				status;

	PRIMARY_REQUEST_INFO	primaryRequestInfo;
	PIRP					topLevelIrp;

	primaryRequestInfo.PrimaryTag			  = 0xe2027482;
	primaryRequestInfo.PrimarySessionId		  = PrimarySession->PrimarySessionId;
	primaryRequestInfo.PrimarySession		  = PrimarySession;
	primaryRequestInfo.NdfsWinxpRequestHeader = NULL;

	topLevelIrp = IoGetTopLevelIrp();
	
	NDAS_ASSERT( topLevelIrp == NULL );
	
	IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

	status = ZwClose( FileHandle );

	IoSetTopLevelIrp( topLevelIrp );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	return status;
}
