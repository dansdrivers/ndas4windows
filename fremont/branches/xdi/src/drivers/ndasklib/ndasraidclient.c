#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasraidclient"


VOID
NdasRaidClientNonArbiterModeThreadProc (
	IN PNDASR_CLIENT	NdasrClient
	);

VOID
NdasRaidClientThreadProc (
	IN PNDASR_CLIENT	NdasrClient
	);

NTSTATUS
NdasRaidClientMonitoringChildrenLurn (
	PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS 
NdasRaidClientHandleCcb (
	PNDASR_CLIENT	NdasrClient
	);

VOID 
NdasRaidClientSetNdasrStatusFlag (
	PNDASR_CLIENT	NdasrClient,
	PCCB			Ccb
	); 

NTSTATUS
NdasRaidClientDispatchCcb (
	PNDASR_CLIENT	NdasrClient, 
	PCCB				Ccb
	);

NTSTATUS
NdasRaidClientQuery (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	);

NTSTATUS 
NdasRaidClientExecuteCcb (
	PNDASR_CLIENT	NdasrClient, 
	PCCB			Ccb
	);

NTSTATUS
NdasRaidClientModeSense (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	);

NTSTATUS 
NdasRaidClientModeSelect (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	); 

NTSTATUS
NdasRaidClientSendCcbToChildrenSyncronously (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	);

NTSTATUS
NdasRaidClientSynchronousCcbCompletion (
	IN PCCB	Ccb,
	IN PCCB	ChildCcb
	);

NTSTATUS
NdasRaidClientUpdateCcbCompletion (
	IN PCCB Ccb,
	IN PCCB ChildCcb
	);

NTSTATUS
NdasRaidClientFlush (
	PNDASR_CLIENT	NdasrClient 
	);

NTSTATUS
NdasRaidClientRefreshCcbStatusFlag (
	IN  PNDASR_CLIENT	NdasrClient,
	OUT PULONG			CcbStatusFlags
	);

VOID
NdasRaidClientRefreshRaidStatusWithoutArbiter (
	PNDASR_CLIENT NdasrClient
	); 

NTSTATUS
NdasRaidClientEstablishArbiter (
	PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS
NdasRaidClientConnectToArbiter (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	);

NTSTATUS
NdasRaidClientDisconnectFromArbiter (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	); 

NTSTATUS
NdasRaidClientRegisterToRemoteArbiter (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection,
	UCHAR						ConType	// NRIX_CONN_TYPE_*
	); 

NTSTATUS 
NdasRaidClientResetRemoteArbiterContext (
	PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS
NdasRaidClientSendRequestAcquireLock (
	PNDASR_CLIENT	NdasrClient,
	UCHAR			LockType,		// NRIX_LOCK_TYPE_BLOCK_IO
	UCHAR			LockMode,		// NRIX_LOCK_MODE_*
	UINT64			BlockAddress,	// in sector for block IO lock
	UINT32			BlockLength,	// in sector.
	PLARGE_INTEGER	Timeout
	);

NTSTATUS
NdasRaidClientSendRequestReleaseLock (
	PNDASR_CLIENT	NdasrClient,
	UINT64			LockId,
	PLARGE_INTEGER	Timeout
	);

NTSTATUS
NdasRaidClientSendRequestNodeChange (
	PNDASR_CLIENT	NdasrClient,
	PLARGE_INTEGER	Timeout
	);

NTSTATUS
NdasRaidClientProcessNodeChageReply ( 
	PNDASR_CLIENT		NdasrClient,
	PNRIX_CHANGE_STATUS	ChangeStatusMsg
	);

NTSTATUS
NdasRaidClientRecvHeaderWithoutWait (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	); 

NTSTATUS
NdasRaidClientRecvAdditionalDataFromRemote (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	); 

NTSTATUS
NdasRaidClientHandleNotificationMsg (
	PNDASR_CLIENT	NdasrClient,
	PNRIX_HEADER	Message
	); 

NTSTATUS
NdasRaidClientCheckChangeStatusMessage (
	PNDASR_CLIENT		NdasrClient,
	PNRIX_CHANGE_STATUS	ChangeStatusMsg
	);

NTSTATUS
NdasRaidClientCheckIoPermission (
	IN PNDASR_CLIENT	NdasrClient,
	IN UINT64			Addr,
	IN UINT32			Length,
	OUT UINT64*			UnauthAddr,
	OUT UINT32*			UnauthLength
	); 


NTSTATUS 
NdasRaidClientStart (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status;
	PNDASR_CLIENT		ndasrClient;
	
	UCHAR				nidx, ridx;
	OBJECT_ATTRIBUTES	objectAttributes;

	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDASSCSI_ASSERT( Lurn->NdasrInfo->NdasrClient == NULL );
	 	
	ndasrClient = ExAllocatePoolWithTag( NonPagedPool, 
										 sizeof(NDASR_CLIENT) - sizeof(UCHAR) + 
										 (Lurn->NdasrInfo->MaxDataRecvLength / 
										  (Lurn->NdasrInfo->ActiveDiskCount - Lurn->NdasrInfo->ParityDiskCount) * 
										  Lurn->NdasrInfo->ActiveDiskCount),
										 NDASR_CLIENT_POOL_TAG );
	
	if (ndasrClient == NULL) {
	
		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	do {

		UCHAR	blockChildrenCount = Lurn->NdasrInfo->ActiveDiskCount - Lurn->NdasrInfo->ParityDiskCount;

		RtlZeroMemory( ndasrClient, 
					   sizeof(NDASR_CLIENT) - sizeof(UCHAR) + 
					   (Lurn->NdasrInfo->MaxDataRecvLength / 
					    (Lurn->NdasrInfo->ActiveDiskCount - Lurn->NdasrInfo->ParityDiskCount) * 
						 Lurn->NdasrInfo->ActiveDiskCount) );

		for (ridx = 0; ridx < Lurn->NdasrInfo->ActiveDiskCount; ridx++) {

			ndasrClient->BlockBuffer[ridx] = ndasrClient->Buffer + Lurn->NdasrInfo->MaxDataRecvLength / blockChildrenCount * ridx;
		}

		InitializeListHead( &ndasrClient->AllListEntry );
		ndasrClient->Lurn = Lurn;

		KeInitializeSpinLock( &ndasrClient->SpinLock );	
		ndasrClient->Status = NDASR_CLIENT_STATUS_INITIALIZING;

		KeInitializeEvent( &ndasrClient->FinishShutdownEvent, NotificationEvent, FALSE );

		InitializeListHead( &ndasrClient->CcbQueue );

		InitializeListHead( &ndasrClient->LockList );

		ndasrClient->NdasrState			= NRIX_RAID_STATE_INITIALIZING;
		ndasrClient->WaitSyncForWrite	= TRUE; 
		ndasrClient->OutOfSyncRoleIndex	= NO_OUT_OF_SYNC_ROLE;
	
		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {
		
			ndasrClient->NdasrNodeFlags[nidx]	= NRIX_NODE_FLAG_UNKNOWN;
			ndasrClient->NdasrDefectCodes[nidx]	= 0;

			ndasrClient->RoleToNodeMap[nidx]	= 0xff;	// not valid until LurnFlag is not unknown.
			ndasrClient->NodeToRoleMap[nidx]	= 0xff;	// not valid until LurnFlag is not unknown.
		}

		KeInitializeSpinLock( &ndasrClient->RequestConnection.SpinLock );	

		KeInitializeSpinLock( &ndasrClient->NotificationConnection.SpinLock );

		ndasrClient->Flush = NdasRaidClientFlush;

		Lurn->NdasrInfo->NdasrClient = ndasrClient;

		// Create client thread
	
		KeInitializeEvent( &ndasrClient->ThreadEvent, NotificationEvent, FALSE );
		KeInitializeEvent( &ndasrClient->ThreadReadyEvent, NotificationEvent, FALSE );

		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		if (LUR_IS_READONLY(ndasrClient->Lurn->Lur) || ndasrClient->Lurn->NdasrInfo->ParityDiskCount == 0) {

			status = PsCreateSystemThread( &ndasrClient->ThreadHandle,
										   THREAD_ALL_ACCESS,
										   &objectAttributes,
										   NULL,
										   NULL,
										   NdasRaidClientNonArbiterModeThreadProc,
										   Lurn->NdasrInfo->NdasrClient );
	
		} else {

			status = PsCreateSystemThread( &ndasrClient->ThreadHandle,
										   THREAD_ALL_ACCESS,
										   &objectAttributes,
										   NULL,
										   NULL,
										   NdasRaidClientThreadProc,
										   Lurn->NdasrInfo->NdasrClient );
		}

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );		

			ndasrClient->ClientState0 = NDASR_CLIENT_STATUS_TERMINATING;
			break;
		}

		status = ObReferenceObjectByHandle( ndasrClient->ThreadHandle,
											GENERIC_ALL,
											NULL,
											KernelMode,
											&ndasrClient->ThreadObject,
											NULL );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );		

			ndasrClient->ClientState0 = NDASR_CLIENT_STATUS_TERMINATING;
			break;
		}

		status = KeWaitForSingleObject( &ndasrClient->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {
	
			NDASSCSI_ASSERT( FALSE );

			ndasrClient->ClientState0 = NDASR_CLIENT_STATUS_TERMINATING;
			break;
		}

		if (FlagOn(ndasrClient->Status, NDASR_CLIENT_STATUS_ERROR)) {

			KeSetEvent( &ndasrClient->ThreadEvent,IO_NO_INCREMENT, FALSE );

			status = KeWaitForSingleObject( ndasrClient->ThreadObject,
											Executive,
											KernelMode,
											FALSE,
											NULL );
			
			NDASSCSI_ASSERT( status == STATUS_SUCCESS );

			status = ndasrClient->ThreadErrorStatus;

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			break;
		}

		NdasRaidRegisterClient( &NdasrGlobalData, ndasrClient );
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		NDASSCSI_ASSERT( FALSE );

		if (ndasrClient->ThreadObject) {

			ObDereferenceObject( ndasrClient->ThreadObject );
		}

		if (ndasrClient->ThreadHandle) {

			ZwClose( ndasrClient->ThreadHandle );
		}

		ExFreePoolWithTag( ndasrClient, NDASR_CLIENT_POOL_TAG );

		Lurn->NdasrInfo->NdasrClient = NULL;
	}

	return status;
}

NTSTATUS
NdasRaidClientStop (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KIRQL				oldIrql;
	
	PNDASR_INFO			ndasrInfo = Lurn->NdasrInfo;
	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	lock;
	PNDASR_CLIENT		ndasrClient = (PNDASR_CLIENT)ndasrInfo->NdasrClient;


	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDASSCSI_ASSERT( Lurn->NdasrInfo->NdasrClient != NULL );
	
	ACQUIRE_SPIN_LOCK( &ndasrInfo->SpinLock, &oldIrql );

	ndasrInfo->NdasrClient  = NULL;

	RELEASE_SPIN_LOCK( &ndasrInfo->SpinLock, oldIrql );	

	NdasRaidUnregisterClient( &NdasrGlobalData, ndasrClient );

	ACQUIRE_SPIN_LOCK( &ndasrClient->SpinLock, &oldIrql );

	ndasrClient->RequestToTerminate = TRUE;
	
	if (ndasrClient->ThreadHandle) {

		DebugTrace(  NDASSCSI_DBG_LURN_NDASR_INFO, ("KeSetEvent\n") );

		KeSetEvent( &ndasrClient->ThreadEvent,IO_NO_INCREMENT, FALSE );
	}
	
	RELEASE_SPIN_LOCK( &ndasrClient->SpinLock, oldIrql );

	if (ndasrClient->ThreadHandle) {

		status = KeWaitForSingleObject( ndasrClient->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );

		ObDereferenceObject( ndasrClient->ThreadObject );
		ZwClose( ndasrClient->ThreadHandle );
	}

	ACQUIRE_SPIN_LOCK( &ndasrClient->SpinLock, &oldIrql );

	ndasrClient->ThreadObject = NULL;
	ndasrClient->ThreadHandle = NULL;

	RELEASE_SPIN_LOCK( &ndasrClient->SpinLock, oldIrql );

	ACQUIRE_SPIN_LOCK( &ndasrClient->SpinLock, &oldIrql );
	
	while (!IsListEmpty(&ndasrClient->LockList)) {

		listEntry = RemoveHeadList( &ndasrClient->LockList );
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Freeing lock\n") );

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );

		NDASSCSI_ASSERT( InterlockedCompareExchange(&lock->InUseCount, 0, 0) == 0 );

		ExFreePoolWithTag( lock, NDASR_CLIENT_LOCK_POOL_TAG );
	}
	
	RELEASE_SPIN_LOCK( &ndasrClient->SpinLock, oldIrql );
	
	ExFreePoolWithTag( ndasrClient, NDASR_CLIENT_POOL_TAG );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client stopped completely\n") );

	return status;
}

NTSTATUS
NdasRaidClientShutdown (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KIRQL				oldIrql;
	
	PNDASR_CLIENT		ndasrClient = Lurn->NdasrInfo->NdasrClient;

	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	NDASSCSI_ASSERT( ndasrClient );
	NDASSCSI_ASSERT( ndasrClient->ThreadHandle );

	ACQUIRE_SPIN_LOCK( &ndasrClient->SpinLock, &oldIrql );

	ndasrClient->RequestToShutdown = TRUE;
	KeSetEvent( &ndasrClient->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &ndasrClient->SpinLock, oldIrql );

	status = KeWaitForSingleObject( &ndasrClient->FinishShutdownEvent,
									Executive,
									KernelMode,
									FALSE,
									NULL );

	if (status != STATUS_SUCCESS) {
	
		NDASSCSI_ASSERT( FALSE );
		return status;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client shutdown completely\n") );

	return status;
}

PNDASR_CLIENT_LOCK
NdasRaidClientAllocLock (
	UINT64		LockId,
	UCHAR 		LockType,
	UCHAR 		LockMode,
	UINT64		Addr,
	UINT32		Length
	) 
{
	PNDASR_CLIENT_LOCK Lock;

	Lock = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_CLIENT_LOCK), NDASR_CLIENT_LOCK_POOL_TAG );

	if (Lock == NULL) {

		NDASSCSI_ASSERT( FALSE );
		return NULL;
	}

	RtlZeroMemory( Lock, sizeof(NDASR_CLIENT_LOCK) );

	InitializeListHead( &Lock->Link );

	Lock->Type			= LockType;
	Lock->Mode			= LockMode;
	Lock->BlockAddress	= Addr;
	Lock->BlockLength	= Length;
	Lock->Id			= LockId;

	Lock->Status		= LOCK_STATUS_NONE;
	
	return Lock;
}

VOID
NdasRaidClientFreeLock (
	PNDASR_CLIENT_LOCK Lock
	)
{
	ExFreePoolWithTag( Lock, NDASR_CLIENT_LOCK_POOL_TAG );
}

VOID
NdasRaidClientNonArbiterModeThreadProc (
	IN PNDASR_CLIENT	NdasrClient
	)
{
	NTSTATUS			status = STATUS_SUCCESS;

	UCHAR				nidx;

	KIRQL				oldIrql;
	LARGE_INTEGER		timeOut;
	

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Starting client\n") );

	NDASSCSI_ASSERT( LUR_IS_READONLY(NdasrClient->Lurn->Lur) ||
					 NdasrClient->Lurn->LurnType == LURN_NDAS_RAID0 ||
					 NdasrClient->Lurn->LurnType == LURN_NDAS_AGGREGATION );
			
	NdasRaidClientRefreshRaidStatusWithoutArbiter( NdasrClient );

	for (nidx = 0; nidx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; nidx++) {

		if (NdasrClient->NodeToRoleMap[nidx] != nidx) {
		
			NDASSCSI_ASSERT( FALSE );

			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );
			NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;

			NdasrClient->ThreadErrorStatus = status;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Error Exiting\n") );

			KeSetEvent( &NdasrClient->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

			PsTerminateSystemThread( STATUS_SUCCESS );

			return;
		}
	}

	if (NdasrClient->NdasrState == NRIX_RAID_STATE_NORMAL) {

		NdasrClient->WaitSyncForWrite	= FALSE; 
	
	} else {

		NDASSCSI_ASSERT( FALSE );
	}

	ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );
	NdasrClient->ClientState0 = NDASR_CLIENT_STATUS_START;
	
	KeSetEvent( &NdasrClient->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	do {
		
		PKEVENT		events[3];
		LONG		eventCount;

		// 8. set shutdown completion event

		if (NdasrClient->RequestToShutdown == TRUE) {

			if (IsListEmpty(&NdasrClient->LockList)) {
					
				KeSetEvent( &NdasrClient->FinishShutdownEvent, IO_DISK_INCREMENT, FALSE );
				NdasrClient->RequestToShutdown = FALSE;
			}
		}

		// 9. Check local node changes 

		for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
			ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
			
			if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
				ClearFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		
				NdasrClient->ClusterState20 = NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED >> 20;

#if DBG
				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
#endif

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Node %d has changed\n", nidx) );
			}
				
			RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
		}

#if DBG
		if (status == STATUS_CLUSTER_NODE_UNREACHABLE) {

			NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );
		}
#endif

		// 10. Send node change message

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED)) {

			NDASSCSI_ASSERT( NdasrClient->ArbiterMode12 == 0 );
			NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

			NdasRaidClientRefreshRaidStatusWithoutArbiter( NdasrClient );
		}

		// 11. Wait for event 

		eventCount = 0;
		events[eventCount] = &NdasrClient->ThreadEvent;
		eventCount++;

		timeOut.QuadPart = -NANO100_PER_SEC * 30; // need to wake-up with timeout to free unused write-locks.

		// To do: Iterate PendingRequestList to find out neareast timeout time
			
		status = KeWaitForMultipleObjects( eventCount, 
										   events,
										   WaitAny, 
										   Executive,
										   KernelMode,
										   TRUE,
										   &timeOut,
										   NULL );
	
		KeClearEvent( &NdasrClient->ThreadEvent );

		// 3. Handle Ccb

		NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 !FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );
		NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );

		status = NdasRaidClientHandleCcb( NdasrClient );

		NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	
			
		if (status != STATUS_SUCCESS) {	

			NDASSCSI_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitSyncForWrite == TRUE ||
							 status == STATUS_CLUSTER_NODE_UNREACHABLE );			
		}
	
		// 5. Check termination request.

		NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 !FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		if (NdasrClient->RequestToTerminate) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RequestToTerminate is set\n") );
			NDASSCSI_ASSERT( IsListEmpty(&NdasrClient->LockList) );
			
			break;
		}

		// 6. check shutdown request	

		NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 !FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

		if (NdasrClient->RequestToShutdown) {

			NdasrClient->Shutdown8 = NDASR_CLIENT_STATUS_SHUTDOWN >> 8;
		}

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN)) {

			NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );
		}

		if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN)) {

			continue;
		}

		// 7. Monitoring

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) &&
						 !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN) );

		NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 !FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		if (NdasrClient->NdasrState == NRIX_RAID_STATE_DEGRADED) {

			NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

			status = NdasRaidClientMonitoringChildrenLurn( NdasrClient );
			NDASSCSI_ASSERT( status == STATUS_SUCCESS );
#if DBG
			for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
				ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
			
				if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
					status = STATUS_CLUSTER_NODE_UNREACHABLE;
				}
				
				RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
			}
#endif
		}

	} while (1);

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_START );
	NdasrClient->ClientState0 = NRIX_RAID_STATE_TERMINATED;
	
	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );	

	// Complete pending 
	
	if (!IsListEmpty(&NdasrClient->CcbQueue)) {
	
		PLIST_ENTRY ccbListEntry;
		PCCB		Ccb;
		
		while (ccbListEntry = ExInterlockedRemoveHeadList(&NdasrClient->CcbQueue,
														  &NdasrClient->SpinLock)) {
			
			Ccb = CONTAINING_RECORD(ccbListEntry, CCB, ListEntry);
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client thread is terminating. Returning CCB with STOP status.\n") );
	
			NDASSCSI_ASSERT( FALSE );

			LsCcbSetStatus( Ccb, CCB_STATUS_STOP );
			LsCcbCompleteCcb( Ccb );
		}
	}
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );
	
	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

VOID
NdasRaidClientThreadProc (
	IN PNDASR_CLIENT	NdasrClient
	)
{
	NTSTATUS			status = STATUS_SUCCESS;

	UCHAR				nidx;

	KIRQL				oldIrql;
	LARGE_INTEGER		timeOut;

	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	lock;

	LARGE_INTEGER		currentTick;
	

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Starting client\n") );

	NDASSCSI_ASSERT( !LUR_IS_READONLY(NdasrClient->Lurn->Lur) && 
					 (NdasrClient->Lurn->LurnType == LURN_NDAS_RAID1 ||
					  NdasrClient->Lurn->LurnType == LURN_NDAS_RAID4 ||
					  NdasrClient->Lurn->LurnType == LURN_NDAS_RAID5) );
			
	NdasRaidClientRefreshRaidStatusWithoutArbiter( NdasrClient );

	NDASSCSI_ASSERT( NdasrClient->NdasrState == NRIX_RAID_STATE_OUT_OF_SYNC ||
					 NdasrClient->NdasrState == NRIX_RAID_STATE_NORMAL );

	for (;;) {

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {
		
			PLIST_ENTRY			listEntry;
			PNDASR_LOCAL_MSG	ndasrLocalMsg;

			status = NdasRaidArbiterStart( NdasrClient->Lurn );

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( FALSE );
				break;
			}
	
			status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
											Executive, 
											KernelMode,
											FALSE,
											NULL );

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( FALSE );
				break;
			}

			NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_CONNECTED >> 16;
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED );

			listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
													 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

			ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

			status = NdasRaidClientHandleNotificationMsg( NdasrClient, 
														  ndasrLocalMsg->NrixHeader );
	
			ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( FALSE );
				break;
			}

			NdasrClient->ArbiterMode12 = NDASR_CLIENT_STATUS_ARBITER_MODE >> 12;
			break;
		}
	
		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {

			NDASSCSI_ASSERT( FALSE );
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				    ("Calling NdasRaidClientEstablishArbiter LUR_IS_PRIMARY(Lurn->Lur) = %d\n", 
					 LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) );

		status = NdasRaidLurnReadRmd( NdasrClient->Lurn, 
									   &NdasrClient->Lurn->NdasrInfo->Rmd, 
									   NdasrClient->Lurn->NdasrInfo->NodeIsUptoDate, 
									   &NdasrClient->Lurn->NdasrInfo->UpToDateNode );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client failed to read RMD. Exiting client thread\n") );
			break;
		}
		
		ClearFlag( NdasrClient->Status, (NDASR_CLIENT_STATUS_ARBITER_MODE) );
		NdasrClient->ArbiterMode12 = NDASR_CLIENT_STATUS_EASTBLISH_ARBITER_MODE >> 12;

		status = NdasRaidClientEstablishArbiter( NdasrClient );	

		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITER_MODE );

		if (status == STATUS_SUCCESS) {

			NdasrClient->ArbiterMode12 = NDASR_CLIENT_STATUS_ARBITER_MODE >> 12;
			break;
		}

		NdasrClient->ConnectionState16 = 0;
		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITER_MODE );
		NdasrClient->WaitSyncForWrite = TRUE;

		NDASSCSI_ASSERT( NdasrClient->ArbiterMode12 == 0 );		
		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == TRUE );

		for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) { 

			UCHAR	nidx2;
			UCHAR	ccbStatus;

			nidx2 = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt - nidx - 1;

			if (!FlagOn(NdasrClient->NdasrNodeFlags[nidx2], NRIX_NODE_FLAG_RUNNING)) {

				continue;
			}

			status = NdasRaidLurnUpdateSynchrously( NdasrClient->Lurn->LurnChildren[nidx2],
													 LURN_UPDATECLASS_WRITEACCESS_USERID,
												   	 &ccbStatus );
				
			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				break;
			}

			if (ccbStatus == CCB_STATUS_NO_ACCESS) {

				NDASSCSI_ASSERT( nidx2 == NdasrClient->Lurn->LurnChildrenCnt - 1 );
				break;
			}

			NDASSCSI_ASSERT( FlagOn(NdasrClient->Lurn->LurnChildren[nidx2]->AccessRight, GENERIC_WRITE) );
		}

		if (nidx == NdasrClient->Lurn->LurnChildrenCnt) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
						("Chage from secondary to primary\n") );

			ClearFlag( NdasrClient->Lurn->Lur->EnabledNdasFeatures, NDASFEATURE_SECONDARY );
			SetFlag( NdasrClient->Lurn->AccessRight, GENERIC_WRITE );
		
		} else {

			LARGE_INTEGER	interval;

			interval.QuadPart = (-5 * NANO100_PER_SEC);
			KeDelayExecutionThread( KernelMode, FALSE, &interval );
		}
	}

	if (status == STATUS_SUCCESS) {

		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );
		NdasrClient->ClientState0 = NDASR_CLIENT_STATUS_START;
	
	} else {

		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );
		NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;

		NdasrClient->ThreadErrorStatus = status;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Error Exiting\n") );

		KeSetEvent( &NdasrClient->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

		PsTerminateSystemThread( STATUS_SUCCESS );
		return;
	}

	KeSetEvent( &NdasrClient->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	do {
		
		PKEVENT		events[3];
		LONG		eventCount;


		// 8. set shutdown completion event

		if (NdasrClient->RequestToShutdown == TRUE) {

			if (IsListEmpty(&NdasrClient->LockList)) {
					
				KeSetEvent( &NdasrClient->FinishShutdownEvent, IO_DISK_INCREMENT, FALSE );
				NdasrClient->RequestToShutdown = FALSE;
			}
		}


		// 9. Check local node changes and send state update request.

		for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
			ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
			
			if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
				ClearFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		
				NdasrClient->ClusterState20 = NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED >> 20;

#if DBG
				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
#endif

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Node %d has changed\n", nidx) );
			}
				
			RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
		}

#if DBG
		if (status == STATUS_CLUSTER_NODE_UNREACHABLE) {

			NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );
		}
#endif

		// 10. Send node change message

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED)) {

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE)) {

				if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED)) {

					if (NdasrClient->WaitSyncForWrite == FALSE) {

						status = NdasRaidClientSendRequestNodeChange( NdasrClient, NULL );

						if (status == STATUS_SUCCESS) {

							ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED );
						}

						if (status != STATUS_SUCCESS) {

							NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_SYNCHRONIZATION_REQUIRED );

							if (status == STATUS_CONNECTION_DISCONNECTED) {

								NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;
								ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
							}
						}
					}
				}
				
			} else {

				NDASSCSI_ASSERT( NdasrClient->ArbiterMode12 == 0 );
				NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

				NdasRaidClientRefreshRaidStatusWithoutArbiter( NdasrClient );
			}
		}

		// 11. Wait for event 

		eventCount = 0;
		events[eventCount] = &NdasrClient->ThreadEvent;
		eventCount++;

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

			ACQUIRE_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock, &oldIrql );

			if (IsListEmpty(&NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue)) {

				KeClearEvent( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent );
			}

			RELEASE_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock, oldIrql );
		
			events[eventCount] = &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent;
			eventCount++;
		
		} else if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED)) {

			if (!LpxTdiV2IsRequestPending(&NdasrClient->NotificationConnection.ReceiveOverlapped, 0)) {

				status = LpxTdiV2Recv( NdasrClient->NotificationConnection.ConnectionFileObject,
									  NdasrClient->NotificationConnection.ReceiveBuf,
									  sizeof(NRIX_HEADER),
									  0,
									  NULL,
									  &NdasrClient->NotificationConnection.ReceiveOverlapped,
									  0,
									  NULL );

				if (!NT_SUCCESS(status)) {

					LpxTdiV2CompleteRequest( &NdasrClient->NotificationConnection.ReceiveOverlapped, 0 );
					
				} else {

					NDASSCSI_ASSERT( status == STATUS_SUCCESS || status == STATUS_PENDING );
				}
			}

			if (!NT_SUCCESS(status)) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to recv notification message\n") );

				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;
				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );

				continue;
			}

			events[eventCount] = &NdasrClient->NotificationConnection.ReceiveOverlapped.Request[0].CompletionEvent;
			eventCount++;
		}

		if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED)) {

			timeOut.QuadPart = -NANO100_PER_SEC * 5; // need to wake-up with timeout to free unused write-locks.
		
		} else {

			timeOut.QuadPart = -NANO100_PER_SEC * 30; // need to wake-up with timeout to free unused write-locks.
		}

		// To do: Iterate PendingRequestList to find out neareast timeout time
			
		status = KeWaitForMultipleObjects( eventCount, 
										   events,
										   WaitAny, 
										   Executive,
										   KernelMode,
										   TRUE,
										   &timeOut,
										   NULL );
	
		KeClearEvent( &NdasrClient->ThreadEvent );
	

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			KeQueryTickCount( &currentTick );

			if (((currentTick.QuadPart - NdasrClient->TemporalPrimarySince.QuadPart) * KeQueryTimeIncrement()) >= (10 * 60 * NANO100_PER_SEC) ) {

				NDASSCSI_ASSERT( FALSE );
			}
		}


		// 1. connect to arbiter

		for(;;) {

			LARGE_INTEGER	interval;

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED)) {

				NDASSCSI_ASSERT( NdasrClient->ArbiterMode12 == NDASR_CLIENT_STATUS_ARBITER_MODE >> 12 );
				break;
			}

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED)) {

				NdasRaidClientResetRemoteArbiterContext( NdasrClient );
			}

			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {
	
				PLIST_ENTRY			listEntry;
				PNDASR_LOCAL_MSG	ndasrLocalMsg;

				status = NdasRaidArbiterStart( NdasrClient->Lurn );

				if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( FALSE );
					NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;
					break;
				}
	
				status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
												Executive, 
												KernelMode,
												FALSE,
												NULL );

				if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( FALSE );
					NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;
					break;
				}

				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_CONNECTED >> 16;
				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED );

				listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
														 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

				ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

				status = NdasRaidClientHandleNotificationMsg( NdasrClient, 
															  ndasrLocalMsg->NrixHeader );
			
				ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

				if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( FALSE );
					NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;
					break;
				}

				NdasrClient->ArbiterMode12 = NDASR_CLIENT_STATUS_ARBITER_MODE >> 12;
				break;

			} 
	
			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

				NDASSCSI_ASSERT( FALSE );
			}

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					    ("Calling NdasRaidClientEstablishArbiter LUR_IS_PRIMARY(Lurn->Lur) = %d\n", 
						 LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) );

			status = NdasRaidLurnReadRmd( NdasrClient->Lurn, 
										   &NdasrClient->Lurn->NdasrInfo->Rmd, 
										   NdasrClient->Lurn->NdasrInfo->NodeIsUptoDate, 
										   &NdasrClient->Lurn->NdasrInfo->UpToDateNode );

			if (status != STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client failed to read RMD. Exiting client thread\n") );
				NDASSCSI_ASSERT( FALSE );
				NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;
				break;
			}
			
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE );
			NdasrClient->ArbiterMode12 = NDASR_CLIENT_STATUS_EASTBLISH_ARBITER_MODE >> 12;

			status = NdasRaidClientEstablishArbiter( NdasrClient );	

			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITER_MODE );

			if (status == STATUS_SUCCESS) {

				NdasrClient->ArbiterMode12 = NDASR_CLIENT_STATUS_ARBITER_MODE >> 12;
				break;
			
			}

			NdasrClient->ConnectionState16 = 0;
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITER_MODE );

			interval.QuadPart = (-5 * NANO100_PER_SEC );
			KeDelayExecutionThread( KernelMode, FALSE, &interval );

			NDASSCSI_ASSERT( NdasrClient->ArbiterMode12 == 0 );		

			for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) { 

				UCHAR	nidx2;
				UCHAR	ccbStatus;

				nidx2 = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt - nidx - 1;

				if (!FlagOn(NdasrClient->NdasrNodeFlags[nidx2], NRIX_NODE_FLAG_RUNNING)) {

					continue;
				}

				status = NdasRaidLurnUpdateSynchrously( NdasrClient->Lurn->LurnChildren[nidx2],
														 LURN_UPDATECLASS_WRITEACCESS_USERID,
													   	 &ccbStatus );
				
				if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

					break;
				}

				if (ccbStatus == CCB_STATUS_NO_ACCESS) {

					NDASSCSI_ASSERT( nidx2 == NdasrClient->Lurn->LurnChildrenCnt - 1 );
					break;
				}

				NDASSCSI_ASSERT( FlagOn(NdasrClient->Lurn->LurnChildren[nidx2]->AccessRight, GENERIC_WRITE) );
			}

			if (nidx == NdasrClient->Lurn->LurnChildrenCnt) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
							("Set Flag NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE\n") );

				KeQueryTickCount( &NdasrClient->TemporalPrimarySince );
				NdasrClient->TemporalPrimay24 = NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE >> 24;
			}

			continue;
		}

		// 2. Handle any pending notifications

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			PLIST_ENTRY			listEntry;
			PNDASR_LOCAL_MSG	ndasrLocalMsg;

			listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
													 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );


			if (listEntry) {

				ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

				status = NdasRaidClientHandleNotificationMsg( NdasrClient, 
															  ndasrLocalMsg->NrixHeader );

				ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );
			
			} else {

				status = STATUS_PENDING;
			}

		} else {

			status = NdasRaidClientRecvHeaderWithoutWait( NdasrClient, &NdasrClient->NotificationConnection );
		
			if (status == STATUS_PENDING) {

				// Packet is not arrived yet.
			
			} else {

				if (status == STATUS_SUCCESS) {
				
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Received through notification connection.\n") );

					status = NdasRaidClientHandleNotificationMsg( NdasrClient, 
																  (PNRIX_HEADER)NdasrClient->NotificationConnection.ReceiveBuf );				
				} 
			}
		}

		if (status == STATUS_SYNCHRONIZATION_REQUIRED) {

			continue;
		}

		if (status != STATUS_PENDING && status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Receive through notification connection failed. Reset current context and enter initialization step.\n"));
				
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
			NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;

			continue;
		}
	
		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED)) {

			continue;
		}

		if (NdasrClient->WaitSyncForWrite == TRUE) {

			continue;
		}

		// 3. Handle Ccb

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );
		NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );

		status = NdasRaidClientHandleCcb( NdasrClient );

		NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	
			
		if (status != STATUS_SUCCESS) {	

			if (status == STATUS_CONNECTION_DISCONNECTED) {

				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;
			
			} else {
				
				NDASSCSI_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitSyncForWrite == TRUE ||
								 status == STATUS_CLUSTER_NODE_UNREACHABLE );			
			}

			continue;
		}

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {

				KeQueryTickCount( &currentTick );

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
							("Clear Flag NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE after %d milisencond\n",
							 (currentTick.QuadPart - NdasrClient->TemporalPrimarySince.QuadPart) * KeQueryTimeIncrement() * 1000 / NANO100_PER_SEC ) );

				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE );
			}
		}

		// 4. Check any lock is not used for a while

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
			
		KeQueryTickCount( &currentTick );
			
		status = STATUS_SUCCESS;

		listEntry = NdasrClient->LockList.Flink;
			
		while (listEntry != &NdasrClient->LockList) {
				
			lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );
			listEntry = listEntry->Flink;			
				
			if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
				
				NDASSCSI_ASSERT( FALSE );
				continue;
			}

			if (lock->Status != LOCK_STATUS_GRANTED) {

				NDASSCSI_ASSERT( LOCK_STATUS_NONE );
				continue;
			}

			if (((currentTick.QuadPart - lock->LastAccesseTick.QuadPart) * KeQueryTimeIncrement()) >= (HZ * NRIX_IO_LOCK_CLEANUP_TIMEOUT)) { // 60 HZ
								
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Releasing locked region(Id=%I64x, Range=%I64x:%x)\n",
							 lock->Id, lock->BlockAddress, lock->BlockLength) );
										
				RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
					
				status = NdasRaidClientSendRequestReleaseLock( NdasrClient, lock->Id, NULL );

				ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

				if (status != STATUS_SUCCESS) {

					break;
				}

				// Remove from acquired lock list and send message				

				RemoveEntryList( &lock->Link );
				NdasRaidClientFreeLock( lock );					
			}
		}

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

		if (status != STATUS_SUCCESS) {	

			if (status == STATUS_CONNECTION_DISCONNECTED) {

				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;

			} else {

				NDASSCSI_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitSyncForWrite == TRUE );
			}

			continue;
		}
	
		// 5. Check termination request.

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		if (NdasrClient->RequestToTerminate) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RequestToTerminate is set\n") );

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE)) {

				NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );		

				status = NdasRaidClientFlush( NdasrClient );

				if (status != STATUS_SUCCESS) {	

					if (status == STATUS_CONNECTION_DISCONNECTED) {

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
						NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;

					} else {

						NDASSCSI_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitSyncForWrite == TRUE );
					}

					continue;
				}
			}

			NDASSCSI_ASSERT( IsListEmpty(&NdasrClient->LockList) );
			break;
		}

		// 6. check shutdown request	

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

		if (NdasrClient->RequestToShutdown) {

			NdasrClient->Shutdown8 = NDASR_CLIENT_STATUS_SHUTDOWN >> 8;
		}

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN)) {

			NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE)) {

				status = NdasRaidClientFlush( NdasrClient );

				if (status != STATUS_SUCCESS) {	

					if (status == STATUS_CONNECTION_DISCONNECTED) {

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
						NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;

					} else {

						NDASSCSI_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitSyncForWrite == TRUE );
					}

					continue;
				}
			}

			NDASSCSI_ASSERT( IsListEmpty(&NdasrClient->LockList) );
		}

		if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN)) {

				continue;
		}

		// 7. Monitoring

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) &&
						 !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN) );

		NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		if (NdasrClient->NdasrState == NRIX_RAID_STATE_DEGRADED) {

			NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

			status = NdasRaidClientMonitoringChildrenLurn( NdasrClient );
			NDASSCSI_ASSERT( status == STATUS_SUCCESS );
#if DBG
			for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
				ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
			
				if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
					status = STATUS_CLUSTER_NODE_UNREACHABLE;
				}
				
				RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
			}
#endif
		}

	} while (1);

	if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE)) {

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			PLIST_ENTRY			listEntry;
			PNDASR_LOCAL_MSG	ndasrLocalMsg;

			while (listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
															&NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock) ) {
						
				ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

				ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );
			}

		} else {

			NdasRaidClientDisconnectFromArbiter( NdasrClient, &NdasrClient->RequestConnection );
			NdasRaidClientDisconnectFromArbiter( NdasrClient, &NdasrClient->NotificationConnection );
		}

		NdasrClient->ArbiterMode12 = 0;
		NdasrClient->ConnectionState16 = 0;
	}

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_START );
	NdasrClient->ClientState0 = NRIX_RAID_STATE_TERMINATED;
	
	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );	

	// Complete pending 
	
	if (!IsListEmpty(&NdasrClient->CcbQueue)) {
	
		PLIST_ENTRY ccbListEntry;
		PCCB		Ccb;
		
		while (ccbListEntry = ExInterlockedRemoveHeadList(&NdasrClient->CcbQueue,
														  &NdasrClient->SpinLock)) {
			
			Ccb = CONTAINING_RECORD(ccbListEntry, CCB, ListEntry);
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client thread is terminating. Returning CCB with STOP status.\n") );
	
			NDASSCSI_ASSERT( FALSE );

			LsCcbSetStatus( Ccb, CCB_STATUS_STOP );
			LsCcbCompleteCcb( Ccb );
		}
	}
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );
	
	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

NTSTATUS
NdasRaidClientMonitoringChildrenLurn (
	PNDASR_CLIENT	NdasrClient
	) 
{
	NTSTATUS			status;
	
	LARGE_INTEGER		currentTick;

	ULONG				nidx;
	PLURELATION_NODE	childLurn;
	KIRQL				oldIrql;

	NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) );
	
	KeQueryTickCount( &currentTick );

	if (currentTick.QuadPart < NdasrClient->NextMonitoringTick.QuadPart) {

		return STATUS_SUCCESS;
	}

	NdasrClient->NextMonitoringTick = currentTick;
	NdasrClient->NextMonitoringTick.QuadPart += (30*NANO100_PER_SEC) / KeQueryTimeIncrement();

	// Check termination request.
		
	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
		
	if (NdasrClient->RequestToTerminate) {

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
		return STATUS_SUCCESS;
	}

	if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED)) {

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
		return STATUS_SUCCESS;
	}

	if (NdasrClient->WaitSyncForWrite) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client is in transition\n") );

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
		return STATUS_SUCCESS;
	}

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_UPDATING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is monitoring\n") );

			RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
			return STATUS_SUCCESS;
		}
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Start\n") );

	status = STATUS_SUCCESS;

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

		//BOOLEAN	 defected = FALSE;
		//NDAS_RAID_META_DATA	rmd;

		childLurn = NdasrClient->Lurn->LurnChildren[nidx];

		NDASSCSI_ASSERT( childLurn->LurnChildIdx == nidx );


#if 0
		if (childLurn->LurnStatus == LURN_STATUS_STALL) {
				
			PCCB ccb;
				
			// If no more request came after returning busy because of stalling, lurn may have no chance to reconnect.
			// Send NOOP to reinitiate reconnect.
				
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Send NOOP message to stalling node %d\n", NdasrClient->RoleToNodeMap[nidx]) );

			status = LsCcbAllocate( &ccb );
				
			if (!NT_SUCCESS(status)) {
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Failed allocate Ccb") );
				break;
			}

			LSCCB_INITIALIZE( ccb );
			
			ccb->OperationCode		= CCB_OPCODE_NOOP;
			ccb->HwDeviceExtension	= NULL;

			LsCcbSetFlag( ccb, CCB_FLAG_ALLOCATED );
			LsCcbSetCompletionRoutine( ccb, NULL, NULL );

			//	Send a CCB to the LURN.
				
			status = LurnRequest( childLurn, ccb );
				
			if (!NT_SUCCESS(status)) {
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Failed to send NOOP request\n") );
				LsCcbFree( ccb );
			}

			continue;
		}

#endif

		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {
				
			// Node is defect. No need to revive

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Don't revive defective node %d\n", nidx) );
			continue;
		}
		
		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRIX_NODE_FLAG_RUNNING)) {

			continue;
		}

		NDASSCSI_ASSERT( NdasrClient->NdasrNodeFlags[nidx] == NRIX_NODE_FLAG_STOP );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Trying to revive node %d\n", nidx) );
				
		// We may need to change LurnDesc if access right is updated.

		LurnSendStopCcb( childLurn );
		LurnDestroy( childLurn );

		status = LurnInitialize( childLurn, childLurn->Lur, childLurn->SavedLurnDesc );
		
		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("stopped node %d is not yet alive\n", nidx) );

			//NdasRaidClientUpdateNodeFlags( NdasrClient, childLurn, 0, 0 );

			status = STATUS_SUCCESS;
			continue;
		}
			
		NDASSCSI_ASSERT( LURN_IS_RUNNING(childLurn->LurnStatus) );
				
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Revived node %d\n", nidx) );

#if 0
		// Read rmd
				
		status = NdasRaidLurnExecuteSynchrously( childLurn, 
												  SCSIOP_READ,
												  FALSE,
												  (PUCHAR)&rmd, 
												  (-1*NDAS_BLOCK_LOCATION_RMD), 
												  1,
												  TRUE );

		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( FALSE );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to read RMD from node %d. Marking disk as defective\n", ridx) );
					
			NdasRaidClientUpdateNodeFlags( NdasrClient, 
											childLurn, 
											NRIX_NODE_FLAG_DEFECTIVE, 
											NRIX_NODE_DEFECT_BAD_DISK );

			status = STATUS_SUCCESS;
			LurnSendStopCcb( childLurn );

			continue;			
		} 

		defected = FALSE;

		if (NDAS_RAID_META_DATA_SIGNATURE != rmd.Signature  || !IS_RMD_CRC_VALID(crc32_calc, rmd)) {

			// Check RMD signature. Currently hot-swapping is not supported. Mark new disk as defective disk.
			// To do for hot-swapping:
			// Check disk is large enough
			// Clear defective flag and mark all bit dirty
					
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("RMD signature or CRC of node %d is not correct. May be disk is replaced with new one.\n", ridx) );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Currently hot-swapping is not supported %d\n", ridx) );

			NDASSCSI_ASSERT( FALSE );

			NdasRaidClientUpdateNodeFlags( NdasrClient, 
											childLurn, 
											NRIX_NODE_FLAG_DEFECTIVE, 
											NRIX_NODE_DEFECT_ETC );

			defected = TRUE;
				
		} else if (RtlCompareMemory(&rmd.RaidSetId, &NdasrClient->Lurn->NdasrInfo->RaidSetId, sizeof(GUID)) != sizeof(GUID)) {	// Check RMD's RAID set is same		
				
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RAID Set ID for node %d is not matched. Consider defective.\n", ridx) );

			NDASSCSI_ASSERT( FALSE );

			NdasRaidClientUpdateNodeFlags(	NdasrClient, 
											childLurn, 
											NRIX_NODE_FLAG_DEFECTIVE, 
											NRIX_NODE_DEFECT_ETC );
			defected = TRUE;
				
		} else if (RtlCompareMemory(&rmd.ConfigSetId, &NdasrClient->Lurn->NdasrInfo->ConfigSetId, sizeof(GUID)) != sizeof(GUID)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Configuration Set ID for node %d is not matched. Consider defective.\n", ridx));

			NDASSCSI_ASSERT( FALSE );
					
			NdasRaidClientUpdateNodeFlags( NdasrClient, 
											childLurn, 
											NRIX_NODE_FLAG_DEFECTIVE, 
											NRIX_NODE_DEFECT_ETC );
			defected = TRUE;
				
		} else if ((rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) && ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount) {

			// NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED is invalid for spare disk.
					
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("This member is used without this disk in degraded mode. We cannot accept this disk as member\n") );

			NDASSCSI_ASSERT( FALSE );

			NdasRaidClientUpdateNodeFlags( NdasrClient, 
											childLurn, 
											NRIX_NODE_FLAG_DEFECTIVE, 
											NRIX_NODE_DEFECT_ETC );
			defected = TRUE;
		}

		if (defected) {

			NDASSCSI_ASSERT( FALSE );
			LurnSendStopCcb( childLurn );
		}

#endif

		// Update status.
			
		NdasRaidClientUpdateNodeFlags( NdasrClient, childLurn, 0, 0 );
	}		
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );

	return status;
}

NTSTATUS 
NdasRaidClientHandleCcb (
	PNDASR_CLIENT	NdasrClient
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;

	PLIST_ENTRY listEntry;
	PCCB		ccb;

	UCHAR		nidx;


	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	listEntry = NdasrClient->CcbQueue.Flink;

	while (listEntry != &NdasrClient->CcbQueue) {

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

		LsCcbSetStatus( ccb, CCB_STATUS_UNKNOWN_STATUS );

		if (NdasrClient->NdasrState != NRIX_RAID_STATE_NORMAL		&&
			NdasrClient->NdasrState != NRIX_RAID_STATE_DEGRADED		&& 
			NdasrClient->NdasrState != NRIX_RAID_STATE_OUT_OF_SYNC) {

			NDASSCSI_ASSERT( NdasrClient->NdasrState == NRIX_RAID_STATE_FAILED );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("ccb = %p, ccb->OperationCode = %x, ccb->Cdb[0] =%s\n", 
						 ccb, CcbOperationCodeString(ccb->OperationCode), CdbOperationString(ccb->Cdb[0])) );

			listEntry = listEntry->Flink;
			RemoveEntryList( &ccb->ListEntry );

			LsCcbSetStatus( ccb, CCB_STATUS_STOP );
			NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );
			LsCcbCompleteCcb( ccb );

			continue;
		}						

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

		status = NdasRaidClientDispatchCcb( NdasrClient, ccb );

		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

		if (status == STATUS_SUCCESS) {

			if (ccb->CcbStatus == CCB_STATUS_SUCCESS			||
				ccb->CcbStatus == CCB_STATUS_DATA_OVERRUN		||
				ccb->CcbStatus == CCB_STATUS_INVALID_COMMAND	||
				ccb->CcbStatus == CCB_STATUS_BUSY				||
				ccb->CcbStatus == CCB_STATUS_NO_ACCESS) {
								
				listEntry = listEntry->Flink;
				RemoveEntryList( &ccb->ListEntry );

				RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

				if (ccb->CcbStatus != CCB_STATUS_BUSY) {

					NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );
				}
				LsCcbCompleteCcb( ccb );

				ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

				continue;
			}

			NDASSCSI_ASSERT( ccb->CcbStatus == CCB_STATUS_STOP ||
							 ccb->CcbStatus == CCB_STATUS_NOT_EXIST );
	
			RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

			for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

				NdasRaidClientUpdateNodeFlags( NdasrClient, 
												NdasrClient->Lurn->LurnChildren[nidx], 
												0, 
												0 );
			}

			ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

			status = STATUS_CLUSTER_NODE_UNREACHABLE;
			break;
		}

		NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED ||
						 status == STATUS_SYNCHRONIZATION_REQUIRED );
	
		NDASSCSI_ASSERT( ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("NdasRaidClientHandleCcb failed. NTSTATUS:%08lx\n", status) );

		break;
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	return status;
}

VOID 
NdasRaidClientSetNdasrStatusFlag (
	PNDASR_CLIENT	NdasrClient,
	PCCB			Ccb
	) 
{
	KIRQL	oldIrql;

	NDASSCSI_ASSERT( Ccb->NdasrStatusFlag8 == 0 );

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
		
	// Do not set RAID flag if RAID status is in transition.

	if (NdasrClient->WaitSyncForWrite) {

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
		return;
	}

	ACQUIRE_DPC_SPIN_LOCK( &Ccb->CcbSpinLock );

	LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_FLAG_VALID );

	switch (NdasrClient->NdasrState) {

	case NRIX_RAID_STATE_NORMAL:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_NORMAL );
		
		break;

	case NRIX_RAID_STATE_DEGRADED:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_DEGRADED );

		break;

	case NRIX_RAID_STATE_OUT_OF_SYNC:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_RECOVERING );

		break;

	case NRIX_RAID_STATE_FAILED:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_FAILURE );

		break;
	}

	RELEASE_DPC_SPIN_LOCK( &Ccb->CcbSpinLock );

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	NDASSCSI_ASSERT( Ccb->NdasrStatusFlag8 != 0 );

}

NTSTATUS
NdasRaidClientDispatchCcb (
	PNDASR_CLIENT	NdasrClient, 
	PCCB			Ccb
	)
{
	NTSTATUS			status;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, 
				("ccb = %p, ccb->OperationCode = %s, ccb->Cdb[0] =%s\n", 
				 Ccb, CcbOperationCodeString(Ccb->OperationCode), CdbOperationString(Ccb->Cdb[0])) );

	switch (Ccb->OperationCode) {

	case CCB_OPCODE_EXECUTE:

		status = NdasRaidClientExecuteCcb( NdasrClient, Ccb );
		break;

	case CCB_OPCODE_ABORT_COMMAND:

		NDASSCSI_ASSERT( FALSE );

		LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		status = STATUS_SUCCESS;
		break;

	case CCB_OPCODE_RESTART:
		
		NDASSCSI_ASSERT( FALSE );

		LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		status = STATUS_SUCCESS;
		break;

	case CCB_OPCODE_RESETBUS:

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
					("ccb = %p, ccb->OperationCode = %s, ccb->Cdb[0] =%s\n", 
					 Ccb, CcbOperationCodeString(Ccb->OperationCode), CdbOperationString(Ccb->Cdb[0])) );
		
		//NDASSCSI_ASSERT( FALSE );

	case CCB_OPCODE_NOOP: {

		//	Check to see if the CCB is coming for only this LURN.
	
		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			status = STATUS_SUCCESS;
			break;
		}
		
		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );					
		break;
	}
	
	case CCB_OPCODE_FLUSH:
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("CCB_OPCODE_FLUSH\n") );
		
		// This code may be running at DPC level.
		// Flush operation should not block
		
		status = NdasRaidClientFlush( NdasrClient );

		if (status == STATUS_SUCCESS) {
		
			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
		}

		break;

	case CCB_OPCODE_UPDATE: {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("CCB_OPCODE_UPDATE requested to RAID1\n") );	

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {
			
			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			status = STATUS_SUCCESS;
			break;
		};

		//	Check to see if the CCB is coming for only this LURN.
		
		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {

			NDASSCSI_ASSERT( FALSE );
		
			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			status = STATUS_SUCCESS;
			break;
		}

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );					
		break;
	}

	case CCB_OPCODE_QUERY:

		status = NdasRaidClientQuery( NdasrClient, Ccb );		
		break;

	default:

		NDASSCSI_ASSERT( Ccb->OperationCode == CCB_OPCODE_SMART );

		LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
		status = STATUS_SUCCESS;
		break;
	}

	return status;
}

NTSTATUS
NdasRaidClientQuery (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	)
{
	NTSTATUS	status;
	PLUR_QUERY	query;


	if (CCB_OPCODE_QUERY != Ccb->OperationCode) {

		return STATUS_INVALID_PARAMETER;
	}

	DebugTrace( DBG_LURN_TRACE, ("CCB_OPCODE_QUERY\n") );

	//	Check to see if the CCB is coming for only this LURN.
	
	if (LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {

		NDASSCSI_ASSERT( FALSE );
		LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		return STATUS_SUCCESS;
	}

	query = (PLUR_QUERY)Ccb->DataBuffer;

	switch (query->InfoClass) {

	case LurEnumerateLurn: {

		PLURN_ENUM_INFORMATION	returnInfo;
		PLURN_INFORMATION		lurnInfo;

		returnInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION( query );
		
		lurnInfo = &returnInfo->Lurns[NdasrClient->Lurn->LurnId];
		
		lurnInfo->Length		= sizeof(LURN_INFORMATION);
		lurnInfo->LurnId		= NdasrClient->Lurn->LurnId;
		lurnInfo->LurnType		= NdasrClient->Lurn->LurnType;
		lurnInfo->UnitBlocks	= NdasrClient->Lurn->UnitBlocks;
		lurnInfo->BlockBytes	= NdasrClient->Lurn->BlockBytes;
		lurnInfo->AccessRight	= NdasrClient->Lurn->AccessRight;
		lurnInfo->StatusFlags	= 0;

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );					
		break;
	}
	
	case LurRefreshLurn: {

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );					
		break;
	}

	case LurPrimaryLurnInformation: {

		DebugTrace( DBG_LURN_TRACE, ("LurPrimaryLurnInformation\n") );

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );					
		break;
	}
	
	default:
	
		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );					
		break;
	}

	return status;
}

NTSTATUS 
NdasRaidClientExecuteCcb (
	PNDASR_CLIENT	NdasrClient, 
	PCCB			Ccb
	)
{
	NTSTATUS			status;


	NDASSCSI_ASSERT( Ccb->OperationCode == CCB_OPCODE_EXECUTE );
	NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );		

	switch (Ccb->Cdb[0]) {

	case SCSIOP_WRITE:
	case SCSIOP_WRITE16: {

		UINT64	blockAddr;
		UINT32	blockLength;
		UINT64	addrToLock;		//	Address to lock 
		UINT32	lengthToLock;	//


		DebugTrace( DBG_LURN_NOISE,("SCSIOP_WRITE\n") );

		DebugTrace( DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength) );

		if (NdasrClient->Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {

			// Fake write is handled by LurProcessWrite.
			// Just check assumptions
		
			NDASSCSI_ASSERT( NdasrClient->Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE );
		}

		if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount) {

			NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) );
		
		} else {

			NDASSCSI_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) );
		}

		LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

		if (blockAddr < 4) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("blockAddr = %I64x, blockLength = %x\n", blockAddr, blockLength) );
		}

		if (blockLength * NdasrClient->Lurn->BlockBytes != Ccb->DataBufferLength) {
			
			NDASSCSI_ASSERT( FALSE );
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->MaxDataSendLength < Ccb->DataBufferLength) {

			NDASSCSI_ASSERT( FALSE );
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->SerialMode) {

			NDASSCSI_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes );

		} else {

			NDASSCSI_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->ChildBlockBytes % 
							 (NdasrClient->Lurn->NdasrInfo->ActiveDiskCount - NdasrClient->Lurn->NdasrInfo->ParityDiskCount) == 0 );
		}

#if __NDAS_SCSI_BOUNDARY_CHECK__

	if (blockAddr < NdasrClient->Lurn->Lur->MaxChildrenSectorCount && (blockAddr + blockLength) > NdasrClient->Lurn->UnitBlocks ) {

		NDASSCSI_ASSERT( FALSE );

		LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
		status = STATUS_SUCCESS;
		break;
	}

#endif

		if (LUR_IS_SECONDARY(NdasrClient->Lurn->Lur)) {

			DebugTrace( DBG_LURN_TRACE,  ("Secondary writes directly: %I64x:%x\n", blockAddr, blockLength) );
		}
	
		while (1) {

			status = NdasRaidClientCheckIoPermission( NdasrClient, blockAddr, blockLength, &addrToLock, &lengthToLock );
	
			if (status == STATUS_SUCCESS) {

				// Lock is covered. Handle Ccb here.
			
				status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
				break;

			} 
			
			DebugTrace( DBG_LURN_TRACE, ("Send lock request for range %I64x:%x\n", addrToLock, lengthToLock) );

			status = NdasRaidClientSendRequestAcquireLock( NdasrClient, 
															NRIX_LOCK_TYPE_BLOCK_IO,
															NRIX_LOCK_MODE_EX, 
															addrToLock, 
															lengthToLock, 
															NULL );
						
			if (status != STATUS_SUCCESS) {

				break;
			}
		}

		break;
	}

	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16: {

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
		break;
	}

	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_READ16: {

		UINT64	blockAddr;
		UINT32	blockLength;

		if (Ccb->DataBufferLength & 0x000003FF) {

			//DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Ccb->DataBufferLength = %x\n", Ccb->DataBufferLength) );
		}

		LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

		if (blockAddr < 4) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("blockAddr = %I64x, blockLength = %x\n", blockAddr, blockLength) );
		}

		if (blockLength * NdasrClient->Lurn->BlockBytes != Ccb->DataBufferLength) {
			
			NDASSCSI_ASSERT( FALSE );
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->MaxDataRecvLength < Ccb->DataBufferLength) {

			NDASSCSI_ASSERT( FALSE );
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->SerialMode) {

			NDASSCSI_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes );

		} else {

			NDASSCSI_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->ChildBlockBytes % 
							 (NdasrClient->Lurn->NdasrInfo->ActiveDiskCount - NdasrClient->Lurn->NdasrInfo->ParityDiskCount) == 0 );
		}

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
		break;
	}

	case SCSIOP_INQUIRY: {
		
		INQUIRYDATA	inquiryData;
		UCHAR		model[16] = {0};


		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]) );

		switch (NdasrClient->Lurn->LurnType) {

		case LURN_NDAS_AGGREGATION:
			
			NDASSCSI_ASSERT( sizeof(NDAS_AGGREGATION_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_AGGREGATION_MODEL_NAME, sizeof(NDAS_AGGREGATION_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID0:

			NDASSCSI_ASSERT( sizeof(NDAS_RAID0_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID0_MODEL_NAME, sizeof(NDAS_RAID0_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID1:

			NDASSCSI_ASSERT( sizeof(NDAS_RAID1_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID1_MODEL_NAME, sizeof(NDAS_RAID1_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID4:

			NDASSCSI_ASSERT( sizeof(NDAS_RAID4_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID4_MODEL_NAME, sizeof(NDAS_RAID4_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID5:

			NDASSCSI_ASSERT( sizeof(NDAS_RAID5_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID5_MODEL_NAME, sizeof(NDAS_RAID5_MODEL_NAME) );
			break;

		default:

			NDASSCSI_ASSERT( FALSE );
			break;
		}

		//	We don't support EVPD(enable vital product data).
		
		if (Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("SCSIOP_INQUIRY: got EVPD. Not supported.\n") );

			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );

			status = STATUS_SUCCESS;
			break;
		}

		RtlZeroMemory( Ccb->DataBuffer, Ccb->DataBufferLength );
		RtlZeroMemory( &inquiryData, sizeof(INQUIRYDATA) );

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;

		inquiryData.DeviceTypeModifier; // : 7;
		inquiryData.RemovableMedia = FALSE;

		inquiryData.Versions = 2;
#if WINVER >= 0x0501
        inquiryData.ANSIVersion; // : 3;
        inquiryData.ECMAVersion; // : 3;
        inquiryData.ISOVersion;  // : 2;
#endif
		inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;		// : 1;
		inquiryData.NormACA;		// : 1;

#if WINVER >= 0x0501
		inquiryData.TerminateTask;	// : 1;
#endif
		inquiryData.AERC;			// : 1;

		inquiryData.AdditionalLength = 31;
		inquiryData.Reserved;

#if WINVER >= 0x0501
		inquiryData.Addr16;				// : 1;               // defined only for SIP devices.
		inquiryData.Addr32	= TRUE;		// : 1;               // defined only for SIP devices.
		inquiryData.AckReqQ;			// : 1;               // defined only for SIP devices.
		inquiryData.MediumChanger;		// : 1;
		inquiryData.MultiPort;			// : 1;
		inquiryData.ReservedBit2;		// : 1;
		inquiryData.EnclosureServices;	// : 1;
		inquiryData.ReservedBit3;		// : 1;
#endif

		inquiryData.SoftReset;			// : 1;
		inquiryData.CommandQueue;		// : 1;
#if WINVER >= 0x0501
		inquiryData.TransferDisable;	// : 1;      // defined only for SIP devices.
#endif		
		inquiryData.LinkedCommands;		// : 1;
		inquiryData.Synchronous;		// : 1;          // defined only for SIP devices.
		inquiryData.Wide16Bit;			// : 1;            // defined only for SIP devices.
		inquiryData.Wide32Bit;			// : 1;            // defined only for SIP devices.
		inquiryData.RelativeAddressing;	// : 1;
		
		RtlCopyMemory( inquiryData.VendorId,
					   NDAS_DISK_VENDOR_ID,
					   (strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8 );

		RtlCopyMemory( inquiryData.ProductId,
					   model,
					   16 );

		RtlCopyMemory( inquiryData.ProductRevisionLevel,
					   PRODUCT_REVISION_LEVEL,
					   (strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  (strlen(PRODUCT_REVISION_LEVEL)+1) : 4 );

		inquiryData.VendorSpecific;
		inquiryData.Reserved3;

		RtlMoveMemory( Ccb->DataBuffer,
					   &inquiryData,
					   Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? sizeof (INQUIRYDATA) : Ccb->DataBufferLength );

		NdasRaidClientRefreshCcbStatusFlag( NdasrClient, &Ccb->CcbStatusFlags );
		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE );
		LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
		status = STATUS_SUCCESS;

		break;
	}

	case SCSIOP_READ_CAPACITY: {

		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				blockCount;
		UINT64				logicalBlockAddress;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("SCSIOP_READ_CAPACITY\n") );

		blockCount = NdasrClient->Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = blockCount - 1;

		if (logicalBlockAddress < 0xffffffff) {
		
			REVERSE_BYTES( &readCapacityData->LogicalBlockAddress, &logicalBlockAddress );

		} else {

			readCapacityData->LogicalBlockAddress = 0xffffffff;
		}

		blockSize = NdasrClient->Lurn->BlockBytes;

		REVERSE_BYTES(&readCapacityData->BytesPerBlock, &blockSize);

		status = NdasRaidClientRefreshCcbStatusFlag( NdasrClient, &Ccb->CcbStatusFlags );
		
		if (status == STATUS_SUCCESS) {

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
		}

		break;
	}

	case SCSIOP_READ_CAPACITY16: {

		PREAD_CAPACITY_DATA_EX	readCapacityDataEx;
		ULONG					blockSize;
		UINT64					blockCount;
		UINT64					logicalBlockAddress;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("SCSIOP_READ_CAPACITY16\n") );

		blockCount = NdasrClient->Lurn->UnitBlocks;

		readCapacityDataEx = (PREAD_CAPACITY_DATA_EX)Ccb->DataBuffer;

		logicalBlockAddress = blockCount - 1;
		REVERSE_BYTES_QUAD( &readCapacityDataEx->LogicalBlockAddress.QuadPart, &logicalBlockAddress );

		blockSize = NdasrClient->Lurn->BlockBytes;
		REVERSE_BYTES( &readCapacityDataEx->BytesPerBlock, &blockSize );

		status = NdasRaidClientRefreshCcbStatusFlag( NdasrClient, &Ccb->CcbStatusFlags );

		if (status == STATUS_SUCCESS) {

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
		}

		break;
	}

	case SCSIOP_START_STOP_UNIT: {

		PCDB		cdb = (PCDB)(Ccb->Cdb);

		if (cdb->START_STOP.Start == START_UNIT_CODE) {

			status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
			break;

		} else if (cdb->START_STOP.Start == STOP_UNIT_CODE) {
	
			// In rebuilding state, don't send stop to child.
			// To do: check another host is accessing the disk.
			// STOP is sent to spin-down HDD. It may be safe to ignore.
		
			if (NdasrClient->NdasrState == NRIX_RAID_STATE_OUT_OF_SYNC) {
		
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR,
							("RAID is in rebuilding status: Don't stop unit\n") );

				status = NdasRaidClientFlush( NdasrClient );

				if (status == STATUS_SUCCESS) {

					LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
				}

				break;
			}
			
			// Flush to reset dirty bitmaps.
			// LurnRAID1RFlushCompletionForStopUnit will send stop to child.
			
			status = NdasRaidClientFlush( NdasrClient );

			if (status == STATUS_SUCCESS) {

				LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			}
				
		} else {

			NDASSCSI_ASSERT( FALSE );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR,
						("SCSIOP_START_STOP_UNIT: Invaild operation!!! %d %d.\n", Ccb->LurId[1], cdb->START_STOP.LogicalUnitNumber) );

			Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;	//SRB_STATUS_INVALID_REQUEST;
			break;
		}

		break;
	}
	
	case SCSIOP_MODE_SENSE:
	
#if __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;	//SRB_STATUS_INVALID_REQUEST;
		status = STATUS_SUCCESS;
		break;
#endif
		status = NdasRaidClientModeSense( NdasrClient, Ccb );
		break;

	case SCSIOP_MODE_SELECT:
#if __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;	//SRB_STATUS_INVALID_REQUEST;
		status = STATUS_SUCCESS;
		break;
#endif

		status = NdasRaidClientModeSelect( NdasrClient, Ccb );
		break;

	case SCSIOP_TEST_UNIT_READY: {

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
		break;
	}

	case SCSIOP_SYNCHRONIZE_CACHE: {

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
		break;
	}

	default:
		
		NDASSCSI_ASSERT( FALSE );

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
		break;
	}
	
	// Ccb still pending

	return status;
}

NTSTATUS
NdasRaidClientModeSense (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	) 
{
	PCDB					cdb;
	PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
	PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));
	ULONG					requiredLen;
	ULONG					BlockCount;

	
	// Check Ccb sanity check and set default sense data.
			
	// Buffer size should larger than MODE_PARAMETER_HEADER
		
	requiredLen = sizeof(MODE_PARAMETER_HEADER);

	if (Ccb->DataBufferLength < requiredLen) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Buffer too small. %d.\n", Ccb->DataBufferLength) );
		
		LsCcbSetStatus( Ccb, CCB_STATUS_DATA_OVERRUN );
		return STATUS_SUCCESS;
	}
	
	RtlZeroMemory (Ccb->DataBuffer, Ccb->DataBufferLength );

	cdb = (PCDB)Ccb->Cdb;

	// We only report current values.
	
	if (cdb->MODE_SENSE.Pc != (MODE_SENSE_CURRENT_VALUES>>6)) {
	
		DebugTrace(NDASSCSI_DBG_LURN_NDASR_INFO, ("unsupported page control:%x\n", (ULONG)cdb->MODE_SENSE.Pc) );

		LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );

		LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
		return STATUS_SUCCESS;
	
	}
	
	// Mode parameter header.

	parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) - sizeof(parameterHeader->ModeDataLength);
	parameterHeader->MediumType = 00;	// Default medium type.

	// Fill device specific parameter

	if (!(NdasrClient->Lurn->AccessRight & GENERIC_WRITE)) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n") );

		parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;

		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_W2K_READONLY_PATCH) || LsCcbIsFlagOn(Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS)) {

			parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
		}

	} else {

		parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	}

	// Mode parameter block

	requiredLen += sizeof(MODE_PARAMETER_BLOCK);

	if (Ccb->DataBufferLength < requiredLen) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Could not fill out parameter block. %d.\n", Ccb->DataBufferLength) );

		LsCcbSetStatus( Ccb, CCB_STATUS_DATA_OVERRUN );
		return STATUS_SUCCESS;
	}

	// Set the length of mode parameter block descriptor to the parameter header.
	
	parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);
	parameterHeader->ModeDataLength += sizeof(MODE_PARAMETER_BLOCK);

	// Make Block.
	
	BlockCount = (ULONG)(NdasrClient->Lurn->EndBlockAddr - NdasrClient->Lurn->StartBlockAddr + 1);

	parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
	parameterBlock->NumberOfBlocks[0] = (BYTE)(BlockCount>>16);
	parameterBlock->NumberOfBlocks[1] = (BYTE)(BlockCount>>8);
	parameterBlock->NumberOfBlocks[2] = (BYTE)(BlockCount);

	if (cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL || cdb->MODE_SENSE.PageCode == MODE_PAGE_CACHING) {	// all pages
		
		PMODE_CACHING_PAGE	cachingPage;

		requiredLen += sizeof(MODE_CACHING_PAGE);
		
		if (Ccb->DataBufferLength < requiredLen) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Could not fill out caching page. %d.\n", Ccb->DataBufferLength) );

			LsCcbSetStatus( Ccb, CCB_STATUS_DATA_OVERRUN );
			return STATUS_SUCCESS;
		}

		parameterHeader->ModeDataLength += sizeof(MODE_CACHING_PAGE);
		cachingPage = (PMODE_CACHING_PAGE)((PUCHAR)parameterBlock + sizeof(MODE_PARAMETER_BLOCK));

		cachingPage->PageCode = MODE_PAGE_CACHING;
		cachingPage->PageLength = sizeof(MODE_CACHING_PAGE) - (FIELD_OFFSET(MODE_CACHING_PAGE, PageLength) + sizeof(cachingPage->PageLength));
		
		// Set default value.
		
		cachingPage->WriteCacheEnable = 1;
		cachingPage->ReadDisableCache = 0;
	
	} else {

		DebugTrace( DBG_LURN_TRACE,
					("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)cdb->MODE_SENSE.PageCode) );

		LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );

		LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
		return STATUS_SUCCESS;
	}
	
	Ccb->ResidualDataLength = Ccb->DataBufferLength - requiredLen;

	return NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );
}

NTSTATUS 
NdasRaidClientModeSelect (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	) 
{
	PCDB				cdb;
	
	PMODE_PARAMETER_HEADER	parameterHeader;
	PMODE_PARAMETER_BLOCK	parameterBlock;
	
	ULONG				requiredLen;
	UCHAR				parameterLength;
	PUCHAR				modePages;
	

	// Check buffer is enough
	
	cdb = (PCDB)Ccb->Cdb;
	
	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)parameterHeader + sizeof(MODE_PARAMETER_HEADER));
	parameterLength = cdb->MODE_SELECT.ParameterListLength;

	// Buffer size should larger than MODE_PARAMETER_HEADER

	requiredLen = sizeof(MODE_PARAMETER_HEADER);

	if (Ccb->DataBufferLength < requiredLen || parameterLength < requiredLen) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Buffer too small. %d.\n", Ccb->DataBufferLength) );
		
		LsCcbSetStatus( Ccb, CCB_STATUS_DATA_OVERRUN );
		return STATUS_SUCCESS;
	}

	requiredLen += sizeof(MODE_PARAMETER_BLOCK);
	
	if (Ccb->DataBufferLength < requiredLen ||parameterLength < requiredLen) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Buffer too small. %d.\n", Ccb->DataBufferLength) );

		LsCcbSetStatus( Ccb, CCB_STATUS_DATA_OVERRUN );
		return STATUS_SUCCESS;
	}

	// We only handle mode pages and volatile settings.

	if (cdb->MODE_SELECT.PFBit == 0) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("unsupported page format:%x\n", (ULONG)cdb->MODE_SELECT.PFBit) );

		LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;

		return STATUS_SUCCESS;

	} else if(cdb->MODE_SELECT.SPBit != 0)	{

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("unsupported save page to non-volitile memory:%x.\n", (ULONG)cdb->MODE_SELECT.SPBit) );

		LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		
		return STATUS_SUCCESS;
	
	}

	// Get the mode pages

	modePages = (PUCHAR)parameterBlock + sizeof(MODE_PARAMETER_BLOCK);

	// Caching mode page

	if ((*modePages & 0x3f) != MODE_PAGE_CACHING) {	// all pages
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("unsupported pagecode:%x\n", (ULONG)cdb->MODE_SENSE.PageCode) );

		LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);	
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;	//SRB_STATUS_SUCCESS;
		
		return STATUS_SUCCESS;
	}
		
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Caching page\n") );

	requiredLen += sizeof(MODE_CACHING_PAGE);

	if (Ccb->DataBufferLength < requiredLen ||parameterLength < requiredLen) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Buffer too small. %d.\n", Ccb->DataBufferLength) );

		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		return STATUS_SUCCESS;
	}

	return NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );
}

NTSTATUS
NdasRaidClientSendCcbToChildrenSyncronouslyCompletionRoutine (
	IN PCCB		Ccb,
	IN PKEVENT	Event
	)
{
	UNREFERENCED_PARAMETER( Ccb );

	KeSetEvent( Event, 0, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
NdasRaidClientSendCcbToChildrenSyncronously (
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb
	)
{
	NTSTATUS			status;

	PNDASR_INFO			ndasrInfo = NdasrClient->Lurn->NdasrInfo;

	UCHAR				nidx, ridx;

	PCCB						nextCcb[LUR_MAX_LURNS_PER_LUR] = {NULL};
	NDASR_CUSTOM_DATA_BUFFER	customBuffer = {FALSE, 0, {0}, {0}, {NULL}, {0}};

	UCHAR				targetChildrenCount;
	UCHAR				filteredTargetChildenCount;

	NDASR_CASCADE_OPTION	ndasRaidCascade;
	BOOLEAN					nonDefectiveOnly;
	BOOLEAN					runningOnly;

	PLURNEXT_IDE_DEVICE ideDisk;
	BOOLEAN				lockAquired[MAX_NDASR_MEMBER_DISK] = {FALSE};


	//NDASSCSI_ASSERT( LsCcbIsFlagOn(Ccb, CCB_FLAG_SYNCHRONOUS) );

	NDASSCSI_ASSERT( Ccb->pExtendedCommand == NULL );
	NDASSCSI_ASSERT( !LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN) );


	try {

		switch (Ccb->OperationCode) {

		case CCB_OPCODE_EXECUTE: {

			if (ndasrInfo->SerialMode) {
				
				if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16 ||
					Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16					  ||
					Ccb->Cdb[0] == SCSIOP_VERIFY || Ccb->Cdb[0] == SCSIOP_VERIFY16) {

					UINT64	blockAddr;
					UINT32	blockLength;
					
					UINT64	accumulateBlockAddress;

					targetChildrenCount = ndasrInfo->ActiveDiskCount;

					ndasRaidCascade = NDASR_CASCADE_FORWARD;
					nonDefectiveOnly = TRUE;
					runningOnly		 = TRUE;

					LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

#if __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__
					blockAddr *= 8;
					blockLength *= 8;
#endif

					customBuffer.DataBufferCount = 0;
					customBuffer.DataBufferAllocated = FALSE;

					accumulateBlockAddress = 0;

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

						if (blockAddr < accumulateBlockAddress + NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks) {

							customBuffer.DataBufferCount ++;

							customBuffer.DataBlockAddress[ridx] = blockAddr - accumulateBlockAddress;

							if (Ccb->DataBuffer) {

								customBuffer.DataBuffer[ridx] = Ccb->DataBuffer;
							}

							if (blockAddr + blockLength <= accumulateBlockAddress + NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks) {

								customBuffer.DataBlockLength[ridx] = blockLength;

								if (Ccb->DataBuffer) {

									customBuffer.DataBufferLength[ridx] = Ccb->DataBufferLength;
								}

							} else {

								NDASSCSI_ASSERT( ridx < ndasrInfo->ActiveDiskCount - 1 );

								customBuffer.DataBlockLength[ridx] = 
									accumulateBlockAddress + NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks - blockAddr;
								
								if (Ccb->DataBuffer) {
									
									customBuffer.DataBufferLength[ridx] = customBuffer.DataBlockLength[ridx] * NdasrClient->Lurn->BlockBytes;
								}

								customBuffer.DataBufferCount ++;

								customBuffer.DataBlockAddress[ridx+1] = 0;
								customBuffer.DataBlockLength[ridx+1] = blockLength - customBuffer.DataBlockLength[ridx];

								if (Ccb->DataBuffer) {
		
									customBuffer.DataBuffer[ridx+1] = ((PUCHAR)Ccb->DataBuffer) + customBuffer.DataBufferLength[ridx];
									customBuffer.DataBufferLength[ridx+1] = Ccb->DataBufferLength - customBuffer.DataBufferLength[ridx];
								}

								NDASSCSI_ASSERT( customBuffer.DataBlockLength[ridx] + customBuffer.DataBlockLength[ridx+1] == blockLength );
								NDASSCSI_ASSERT( customBuffer.DataBufferLength[ridx] + customBuffer.DataBufferLength[ridx+1] == Ccb->DataBufferLength );
							}

							break;
						}

						accumulateBlockAddress += NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks;
					}

					NDASSCSI_ASSERT( customBuffer.DataBufferCount > 0 );
					break;
				}
			}

			switch (Ccb->Cdb[0]) {

			case SCSIOP_WRITE:
			case SCSIOP_WRITE16: {

				UCHAR	blockChildrenCount;
				LONG	i, j, k;
				UINT64	blockAddr;
				UINT32	blockLength;

				LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

				targetChildrenCount = ndasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				blockChildrenCount = ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount;

				if (blockChildrenCount == 1) { //LURN_NDASR_RAID1
			
					break;
				}

				customBuffer.DataBufferCount = 0;
				customBuffer.DataBufferAllocated = FALSE;

				for (ridx = 0; ridx < targetChildrenCount; ridx++) {

					customBuffer.DataBuffer[ridx] = NdasrClient->BlockBuffer[ridx];

					customBuffer.DataBufferLength[ridx] = Ccb->DataBufferLength / blockChildrenCount;
					customBuffer.DataBufferCount++;
				}

				for (i = 0; i < Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes; i++ ) {

					UCHAR	parityRoleIndex;
					UCHAR	nextChild;


					if (ndasrInfo->ParityDiskCount) {

						if (ndasrInfo->InterleaveMode) {

							parityRoleIndex = (blockAddr+i) % ndasrInfo->ActiveDiskCount;
					
						} else {

							parityRoleIndex = blockChildrenCount;
						}

					} else {

						parityRoleIndex = 0xFF;
					}

					j = 0;

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx ++) {

						if (ridx == parityRoleIndex) {

							continue;
						}

						RtlCopyMemory( customBuffer.DataBuffer[ridx] + i * NdasrClient->Lurn->ChildBlockBytes,
								       ((PUCHAR)Ccb->DataBuffer) + i*NdasrClient->Lurn->BlockBytes + j*NdasrClient->Lurn->ChildBlockBytes,
								       NdasrClient->Lurn->ChildBlockBytes );

						j++;
					}

					NDASSCSI_ASSERT( j == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount );

					if (parityRoleIndex == 0xFF) {

						continue;
					}

					if (parityRoleIndex == 0) {

						nextChild = 1;
					
					} else {

						nextChild = 0;
					}

					RtlCopyMemory( customBuffer.DataBuffer[parityRoleIndex] + i * NdasrClient->Lurn->ChildBlockBytes,
								   customBuffer.DataBuffer[nextChild] + i * NdasrClient->Lurn->ChildBlockBytes,
								   NdasrClient->Lurn->ChildBlockBytes );

					nextChild ++;

					for (ridx = nextChild; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

						if (ridx == parityRoleIndex) {

							continue;
						}

						for (k = 0; k < NdasrClient->Lurn->ChildBlockBytes; k ++) {

							customBuffer.DataBuffer[parityRoleIndex][i * NdasrClient->Lurn->ChildBlockBytes + k] ^=
								customBuffer.DataBuffer[ridx][i * NdasrClient->Lurn->ChildBlockBytes + k];									
						}
					}
				}

#if 0
				if (ndasrInfo->ParityDiskCount) {

					UCHAR	parityChar;
				
					for (k = 0; k < customBuffer.DataBufferLength[0]; k++) {

						parityChar = 0;

						for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

							parityChar ^= customBuffer.DataBuffer[ridx][k];
						}

						NDASSCSI_ASSERT( parityChar == 0 );
					}
				}
#endif

				break;
			}

			case SCSIOP_VERIFY:
			case SCSIOP_VERIFY16: {
	
				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				break;
			}

			case 0x3E:		// READ_LONG
			case SCSIOP_READ:
			case SCSIOP_READ16: {
	
				UCHAR	blockChildrenCount;

				targetChildrenCount = ndasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				blockChildrenCount = ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount;

				if (blockChildrenCount == 1) { //LURN_NDASR_RAID1
			
					break;
				}

				customBuffer.DataBufferCount = 0;
				customBuffer.DataBufferAllocated = FALSE;

				for (ridx = 0; ridx < targetChildrenCount; ridx++) {

					customBuffer.DataBuffer[ridx] = NdasrClient->BlockBuffer[ridx];

					customBuffer.DataBufferLength[ridx] = Ccb->DataBufferLength / blockChildrenCount;
					customBuffer.DataBufferCount++;
				}

				break;
			}

			case SCSIOP_START_STOP_UNIT: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				break;
			}

			case SCSIOP_MODE_SENSE: 
			case SCSIOP_MODE_SELECT: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

				customBuffer.DataBufferCount = 0;
				customBuffer.DataBufferAllocated = TRUE;
	
				for (ridx = 0; ridx < targetChildrenCount; ridx++) {
	
					customBuffer.DataBuffer[ridx] = ExAllocatePoolWithTag( NonPagedPool,
																		   Ccb->DataBufferLength,
																		   CCB_CUSTOM_BUFFER_POOL_TAG );

					if (!customBuffer.DataBuffer[ridx]) {
	
						NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
						Ccb->CcbStatus = CCB_STATUS_SUCCESS;
						status = STATUS_INSUFFICIENT_RESOURCES;
						leave;
					}
 
					if (Ccb->Cdb[0] == SCSIOP_MODE_SELECT) {

						RtlCopyMemory( customBuffer.DataBuffer[ridx], Ccb->DataBuffer, Ccb->DataBufferLength );
					}

					customBuffer.DataBufferLength[ridx] = Ccb->DataBufferLength;
					customBuffer.DataBufferCount++;
				}
	
				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				break;
			}

			case SCSIOP_TEST_UNIT_READY: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				break;
			}

			case SCSIOP_SYNCHRONIZE_CACHE: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				break;
			}

			default:

				NDASSCSI_ASSERT( FALSE );
				leave;

				break;
			}

			break;		
		}

		case CCB_OPCODE_RESETBUS:
		case CCB_OPCODE_NOOP: {

			targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

			ndasRaidCascade = NDASR_CASCADE_FORWARD;
			nonDefectiveOnly = TRUE;
			runningOnly		 = TRUE;

			break;
		}

		case CCB_OPCODE_UPDATE: {

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

				targetChildrenCount = 0;
				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				status = STATUS_SUCCESS;

			} else {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;
			}

			ndasRaidCascade = NDASR_CASCADE_BACKWARD;
			nonDefectiveOnly = TRUE;
			runningOnly		 = TRUE;

			break;
		}

		case CCB_OPCODE_QUERY: {

			switch (((PLUR_QUERY)Ccb->DataBuffer)->InfoClass) {

			case LurEnumerateLurn: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = FALSE;
				runningOnly		 = FALSE;

				break;
			}
	
			case LurRefreshLurn: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = FALSE;
				runningOnly		 = FALSE;

				break;
			}

			case LurPrimaryLurnInformation: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;

				break;
			}
	
			default:
		
				NDASSCSI_ASSERT( FALSE );
				leave;

				break;
			}

			break;
		}

		default:

			NDASSCSI_ASSERT( FALSE );
			leave;

			break;
		}

		filteredTargetChildenCount = 0;

		for (ridx = 0; ridx < targetChildrenCount; ridx++) {

			if (runningOnly) {
			
				if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_RUNNING)) {

					NDASSCSI_ASSERT( NdasrClient->NdasrState == NRIX_RAID_STATE_DEGRADED );
					continue;
				}
			}

			if (nonDefectiveOnly) {
			
				if (FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_DEFECTIVE)) {

					NDASSCSI_ASSERT( NdasrClient->NdasrState == NRIX_RAID_STATE_DEGRADED );
					continue;
				}
			}

			if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {
				
				if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16) {

					if (ridx == NdasrClient->OutOfSyncRoleIndex) {

						NDASSCSI_ASSERT( NdasrClient->Lurn->NdasrInfo->ParityDiskCount != 0 );
						continue;
					}
				}
			}

			if (ndasrInfo->SerialMode) {

				if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {
				
					if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16 ||
						Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16					  ||
						Ccb->Cdb[0] == SCSIOP_VERIFY || Ccb->Cdb[0] == SCSIOP_VERIFY16) {

						if (customBuffer.DataBlockLength[ridx] == 0) {

							continue;
						}
					}
				}
			}

			status = LsCcbAllocate( &nextCcb[ridx] );

			if (status != STATUS_SUCCESS) {

				leave;
			}

			status = LsCcbInitializeByCcb( Ccb, NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]], nextCcb[ridx] );
		
			if (status != STATUS_SUCCESS) {

				leave;
			}

			ASSERT( Ccb->DataBuffer == nextCcb[ridx]->DataBuffer );
	
			nextCcb[ridx]->AssociateID = ridx;

			LsCcbSetFlag( nextCcb[ridx], CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED );
			LsCcbSetFlag( nextCcb[ridx], Ccb->Flags & CCB_FLAG_SYNCHRONOUS );

			if (customBuffer.DataBuffer[ridx]) {

				nextCcb[ridx]->DataBuffer = customBuffer.DataBuffer[ridx];
				nextCcb[ridx]->DataBufferLength = customBuffer.DataBufferLength[ridx];
			}

			if (ndasrInfo->SerialMode) {

				if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {
				
					if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16 ||
						Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16					  ||
						Ccb->Cdb[0] == SCSIOP_VERIFY || Ccb->Cdb[0] == SCSIOP_VERIFY16) {

						LsCcbSetLogicalAddress( (PCDB)(nextCcb[ridx]->Cdb), customBuffer.DataBlockAddress[ridx] );
						LsCcbSetTransferLength( (PCDB)(nextCcb[ridx]->Cdb), customBuffer.DataBlockLength[ridx] );
					}
				}
			}

			filteredTargetChildenCount ++;

			// send to only one child

			if (Ccb->OperationCode == CCB_OPCODE_QUERY) {

				if (((PLUR_QUERY)Ccb->DataBuffer)->InfoClass == LurPrimaryLurnInformation) {

					break;
				}
			}

			if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

				if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16) {

#if __NDAS_SCSI_RAID_DISABLE_READ_LOCK__

					LsCcbResetFlag( nextCcb[ridx], CCB_FLAG_ACQUIRE_BUFLOCK );
#endif

					if (filteredTargetChildenCount == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) {

						break;
					}
				}
			}
		}	 

		if (Ccb->OperationCode == CCB_OPCODE_QUERY &&
			((PLUR_QUERY)Ccb->DataBuffer)->InfoClass == LurPrimaryLurnInformation) {

			if (filteredTargetChildenCount < 1) {

				NDASSCSI_ASSERT( FALSE );

				status = STATUS_CLUSTER_NODE_UNREACHABLE;
				leave;
			}
	
		} else if (ndasrInfo->SerialMode && Ccb->OperationCode == CCB_OPCODE_EXECUTE &&
				   (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16 ||
				    Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16					  ||
				    Ccb->Cdb[0] == SCSIOP_VERIFY || Ccb->Cdb[0] == SCSIOP_VERIFY16)) {

			if (filteredTargetChildenCount < 1 || filteredTargetChildenCount > 2) {
				
				NDASSCSI_ASSERT( FALSE );

				status = STATUS_CLUSTER_NODE_UNREACHABLE;
				leave;
			}

		} else if (Ccb->OperationCode == CCB_OPCODE_UPDATE &&
				   FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		} else {
		
			if (filteredTargetChildenCount < ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) {
	
				NDASSCSI_ASSERT( FALSE );

				status = STATUS_CLUSTER_NODE_UNREACHABLE;
				leave;
			}
		}

		if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

			if (Ccb->Cdb[0] == SCSIOP_WRITE ||Ccb->Cdb[0] == SCSIOP_WRITE16) {

				KeEnterCriticalRegion();
				ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

				for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;) { 

					UCHAR	ccbStatus;

					if (nextCcb[ridx]) {

						NDASSCSI_ASSERT( LsCcbIsFlagOn(nextCcb[ridx], CCB_FLAG_ACQUIRE_BUFLOCK) );

#if !__NDAS_SCSI_LOCK_BUG_AVOID__
						LsCcbResetFlag( nextCcb[ridx], CCB_FLAG_ACQUIRE_BUFLOCK );
#endif

						status = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
															   LURNDEVLOCK_ID_BUFFLOCK,
															   NDSCLOCK_OPCODE_ACQUIRE,
															   &ccbStatus );

						if (status == STATUS_LOCK_NOT_GRANTED) {

							if (ccbStatus == CCB_STATUS_LOST_LOCK) {

								status = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
																	  LURNDEVLOCK_ID_BUFFLOCK,
																	  NDSCLOCK_OPCODE_RELEASE,
																	  &ccbStatus );

								if (status == STATUS_SUCCESS) {

									continue;
								}
							}

							NDASSCSI_ASSERT( FALSE );
						}

						if (status != STATUS_SUCCESS) {

							NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

							Ccb->CcbStatus = ccbStatus;
							break;
						}
			
						lockAquired[ridx] = TRUE;

						ideDisk = (PLURNEXT_IDE_DEVICE)NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->LurnExtension;
#if !__NDAS_SCSI_LOCK_BUG_AVOID__
						NDASSCSI_ASSERT( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == TRUE );
#endif
					}

					ridx ++;
				}
			}
		}

		for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

			KEVENT	completionEvent;
			UCHAR	nidx2;

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				break;
			}

			if (ndasRaidCascade == NDASR_CASCADE_BACKWARD || ndasRaidCascade == NDASR_CASCADE_BACKWARD_CHAINING) {

				nidx2 = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt - nidx - 1;
			
			} else {

				nidx2 = nidx;
			} 

			if (nextCcb[NdasrClient->NodeToRoleMap[nidx2]] == NULL) {

				continue;
			}

			KeInitializeEvent( &completionEvent, SynchronizationEvent, FALSE );

			LsCcbSetCompletionRoutine( nextCcb[NdasrClient->NodeToRoleMap[nidx2]], 
									   NdasRaidClientSendCcbToChildrenSyncronouslyCompletionRoutine, 
									   &completionEvent );

			status = LurnRequest( NdasrClient->Lurn->LurnChildren[nidx2], nextCcb[NdasrClient->NodeToRoleMap[nidx2]] );
		
			DebugTrace( DBG_LURN_NOISE, ("LurnRequest status : %08x\n", status) );

			if (status != STATUS_SUCCESS && status != STATUS_PENDING) {

				NDASSCSI_ASSERT( FALSE );

				break;
			}

			if (status == STATUS_PENDING) {

				status = KeWaitForSingleObject( &completionEvent,
												Executive,
												KernelMode,
												FALSE,
												NULL );

				NDASSCSI_ASSERT( status == STATUS_SUCCESS );
			}

			if (!LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx2]->LurnStatus)) {
				
				if (Ccb->OperationCode != CCB_OPCODE_RESETBUS) {

					NDASSCSI_ASSERT( nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_STOP ||
									 nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_BUSY ||
									 nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_NOT_EXIST );
				}
			}
			
			if (nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_STOP ||
				nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_NOT_EXIST) {
			
				NDASSCSI_ASSERT( !LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx2]->LurnStatus) );
			}

			NdasRaidClientSynchronousCcbCompletion( Ccb, nextCcb[NdasrClient->NodeToRoleMap[nidx2]] );

			if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

				break;
			}
		}

		NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_SUCCESS			||
						 Ccb->CcbStatus == CCB_STATUS_INVALID_COMMAND	||
						 Ccb->CcbStatus == CCB_STATUS_STOP				||
						 Ccb->CcbStatus == CCB_STATUS_NOT_EXIST			||
						 Ccb->CcbStatus == CCB_STATUS_NO_ACCESS			||
						 Ccb->CcbStatus == CCB_STATUS_BUSY );

		//	post-process

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, 
					("Completing Ccb %s with CcbStatus %x, status = %x\n", CdbOperationString(Ccb->Cdb[0]), Ccb->CcbStatus, status) );

		if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Completing Ccb with status %x\n", Ccb->CcbStatus) );
		}

		if (Ccb->CcbStatus == CCB_STATUS_SUCCESS) {

			if (Ccb->OperationCode != CCB_OPCODE_RESETBUS) {

				for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

					if (nextCcb[ridx]) {
		
						NDASSCSI_ASSERT( LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->LurnStatus) );
					}
				}
			}
		}

		if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

			if (Ccb->Cdb[0] == SCSIOP_WRITE ||Ccb->Cdb[0] == SCSIOP_WRITE16) {

				for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

					NTSTATUS status2;

					if (lockAquired[ridx] == TRUE) {

						status2 = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
																LURNDEVLOCK_ID_BUFFLOCK,
																NDSCLOCK_OPCODE_RELEASE,
																NULL );
	
						if (status2 == STATUS_CLUSTER_NODE_UNREACHABLE) {

							//status = STATUS_CLUSTER_NODE_UNREACHABLE;
						}

						NDASSCSI_ASSERT( status2 == STATUS_SUCCESS || status2 == STATUS_CLUSTER_NODE_UNREACHABLE );
					}

					ideDisk = (PLURNEXT_IDE_DEVICE)NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->LurnExtension;
#if !__NDAS_SCSI_LOCK_BUG_AVOID__
					NDASSCSI_ASSERT( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == FALSE );
#endif
				}

				ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
				KeLeaveCriticalRegion();

				NdasRaidClientReleaseBlockIoPermissionToClient( NdasrClient, Ccb );
			}

			if (ndasrInfo->SerialMode) {

				// LURN_NDAS_AGGREGATION

			} else if (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount == 1) {

				// LURN_NDAS_RAID1

			} else if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16) {

				LONG	i, j, k;	
				UINT64	blockAddr;
				UINT32	blockLength;

				LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

#if 0

				if (ndasrInfo->ParityDiskCount != 0) {

					UCHAR	nextChild;
					UCHAR	unReadRidx;

					if (NdasrClient->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE) {

						unReadRidx = NdasrClient->OutOfSyncRoleIndex;
				
					} else {

						unReadRidx = ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount;
					}

					NDASSCSI_ASSERT( nextCcb[unReadRidx] == NULL );
			
					if (unReadRidx == 0) {

						nextChild = 1;
					
					} else {

						nextChild = 0;
					}

					NDASSCSI_ASSERT( nextCcb[nextChild] != NULL );

					RtlCopyMemory( customBuffer.DataBuffer[unReadRidx],
								   customBuffer.DataBuffer[nextChild],
								   customBuffer.DataBufferLength[nextChild] );

					nextChild ++;

					for (ridx = nextChild; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

						if (ridx == unReadRidx) {

							continue;
						}

						NDASSCSI_ASSERT( nextCcb[ridx] != NULL );

						for (k = 0; k < customBuffer.DataBufferLength[unReadRidx]; k ++) {

							customBuffer.DataBuffer[unReadRidx][k] ^= customBuffer.DataBuffer[ridx][k];									
						}
					}
				}

#if 0

				if (ndasrInfo->ParityDiskCount) {

					UCHAR	parityChar;

					for (k = 0; k < customBuffer.DataBufferLength[0]; k++) {

						parityChar = 0;

						for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

							parityChar ^= customBuffer.DataBuffer[ridx][k];
						}

						NDASSCSI_ASSERT( parityChar == 0 );
					}
				}

#endif

				for (i = 0; i < Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes; i++ ) {

					UCHAR	parityRoleIndex;

					if (ndasrInfo->ParityDiskCount != 0) {

						if (ndasrInfo->InterleaveMode) {

							parityRoleIndex = (blockAddr+i) % ndasrInfo->ActiveDiskCount;

						} else {

							parityRoleIndex = ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount;
						}
				
					} else {

						parityRoleIndex = 0xFF;
					}

					j = 0;

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx ++) {

						if (ridx == parityRoleIndex) {

							continue;
						}

						RtlCopyMemory( ((PUCHAR)Ccb->DataBuffer) + i*NdasrClient->Lurn->BlockBytes + j*NdasrClient->Lurn->ChildBlockBytes,
										customBuffer.DataBuffer[ridx] + i * NdasrClient->Lurn->ChildBlockBytes,
								        NdasrClient->Lurn->ChildBlockBytes );

						j ++;
					}

					NDASSCSI_ASSERT( j == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount );
				}

#else

				for (i = 0; i < Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes; i++ ) {

					UCHAR	parityRoleIndex;
					UCHAR	blockChildrenCount;

					blockChildrenCount = ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount;

					if (ndasrInfo->ParityDiskCount == 0) { 

						parityRoleIndex = 0xFF;
					
					} else {

						if (ndasrInfo->InterleaveMode) {

							parityRoleIndex = (blockAddr+i) % ndasrInfo->ActiveDiskCount;

						} else {

							parityRoleIndex = blockChildrenCount;
						}
					}

					if (parityRoleIndex == 0xFF) {

					} else if (parityRoleIndex == NdasrClient->OutOfSyncRoleIndex) {
						
					} else if (NdasrClient->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE &&
							   parityRoleIndex == blockChildrenCount) {
					
					} else {

						for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx ++) {
		
							if (ridx == parityRoleIndex) {

								continue;
							}

							if (ridx == NdasrClient->OutOfSyncRoleIndex) {

								continue;
							}

							if (NdasrClient->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE &&
								ridx == blockChildrenCount) {

								continue;
							}

							for (k = 0; k < NdasrClient->Lurn->ChildBlockBytes; k ++) {

								customBuffer.DataBuffer[parityRoleIndex][i * NdasrClient->Lurn->ChildBlockBytes + k] ^=
									customBuffer.DataBuffer[ridx][i * NdasrClient->Lurn->ChildBlockBytes + k];									
							}
						}
					}

					j = 0;

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx ++) {
		
						UCHAR ridx2;

						if (ridx == parityRoleIndex) {

							continue;
						}

						if (ridx == NdasrClient->OutOfSyncRoleIndex || ridx == blockChildrenCount) {

							ridx2 = parityRoleIndex;
						
						} else {

							ridx2 = ridx;
						}

						RtlCopyMemory( ((PUCHAR)Ccb->DataBuffer) + i*NdasrClient->Lurn->BlockBytes + j*NdasrClient->Lurn->ChildBlockBytes,
										customBuffer.DataBuffer[ridx2] + i * NdasrClient->Lurn->ChildBlockBytes,
								        NdasrClient->Lurn->ChildBlockBytes );

						j ++;
					}

					NDASSCSI_ASSERT( j == blockChildrenCount );
				}

#endif
			}
		}

		if (Ccb->OperationCode == CCB_OPCODE_QUERY) {

			if (Ccb->CcbStatus == CCB_STATUS_SUCCESS) {

				if (((PLUR_QUERY)Ccb->DataBuffer)->InfoClass == LurPrimaryLurnInformation) {
				
					PLURN_PRIMARYINFORMATION	returnInfo;
					PLURN_INFORMATION			lurnInfo;

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting RAID set ID as LurPrimaryLurnInformation's primary id\n") );

					returnInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION( (PLUR_QUERY)Ccb->DataBuffer );
					lurnInfo = &returnInfo->PrimaryLurn;
					
					RtlCopyMemory( lurnInfo->PrimaryId, &NdasrClient->Lurn->NdasrInfo->RaidSetId, sizeof(lurnInfo->PrimaryId) );
				}
			}
		}

		if (Ccb->OperationCode == CCB_OPCODE_UPDATE) {

			//	Complete the original CCB

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,("Ccb:%p. Ccb->StatusFlags:%08lx\n", Ccb, Ccb->CcbStatusFlags) );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
						("Ccb->OperationCode = %08x, Ccb->CcbStatus = %08x, LurnUpdate->UpdateClass = %08x\n",
						 Ccb->OperationCode, Ccb->CcbStatus, ((PLURN_UPDATE)Ccb->DataBuffer)->UpdateClass) );

			// set event to work as primary
	
			if (Ccb->CcbStatus == CCB_STATUS_SUCCESS) {

				PLURN_UPDATE	lurnUpdate;
	
				lurnUpdate = (PLURN_UPDATE)Ccb->DataBuffer;

				if (lurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
								("********** Lurn->LurnType(%d) : R/O ->R/W : Start to initialize **********\n",
								 NdasrClient->Lurn->LurnType) );

					if (LURN_IS_ROOT_NODE(NdasrClient->Lurn->LurnParent)) {
					
						DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
									("Updated enabled feature: %08lx\n",NdasrClient->Lurn->Lur->EnabledNdasFeatures) );
		
						DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,("Starting DRAID arbiter from updated permission\n") );

						for (nidx=0; nidx<(LONG)NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

							if (FlagOn(NdasrClient->Lurn->LurnChildren[nidx]->AccessRight, GENERIC_WRITE)) {
							
								if (!LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx]->LurnStatus)) {
							
									NDASSCSI_ASSERT( FALSE );									

									ACQUIRE_DPC_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->SpinLock );
									NdasRaidClientUpdateNodeFlags( NdasrClient, NdasrClient->Lurn->LurnChildren[nidx], 0, 0 );
									RELEASE_DPC_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->SpinLock );

									break;
								}
							
							} else {

								if (LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx]->LurnStatus)) {
							
									NDASSCSI_ASSERT( FALSE );									
								
									ACQUIRE_DPC_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->SpinLock );
									NdasRaidClientUpdateNodeFlags( NdasrClient, NdasrClient->Lurn->LurnChildren[nidx], 0, 0 );
									RELEASE_DPC_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->SpinLock );

									break;
								}
							}
						}

						if (nidx != NdasrClient->Lurn->LurnChildrenCnt) {

							Ccb->CcbStatus = CCB_STATUS_STOP;

						} else {

							NdasrClient->Lurn->Lur->EnabledNdasFeatures &= ~NDASFEATURE_SECONDARY;
							NdasrClient->Lurn->AccessRight |= GENERIC_WRITE;

							for (nidx = 0; nidx < (LONG)NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

								DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
											("Node %d is not updated to have write access. Giving write access for revival.\n", nidx) );

								// Update access write of stopped child too

								SetFlag( NdasrClient->Lurn->LurnChildren[nidx]->AccessRight, GENERIC_WRITE );

								if (NdasrClient->Lurn->LurnChildren[nidx]->SavedLurnDesc) {
							
									NdasrClient->Lurn->LurnChildren[nidx]->SavedLurnDesc->AccessRight |= GENERIC_WRITE;
								}
							}
						}
			
					} else {
				
						// Other case does not happen.

						NDASSCSI_ASSERT( FALSE );
					}

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to update access right of all nodes\n") );

					for (nidx = 0; nidx < (LONG)NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

						if (FlagOn(NdasrClient->Lurn->LurnChildren[nidx]->AccessRight, GENERIC_WRITE)) {

							NDASSCSI_ASSERT( FALSE );
						}
					}
				}
			}

			if (Ccb->CcbStatus == CCB_STATUS_BUSY) {
	
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,("CCB_OPCODE_UPDATE: return CCB_STATUS_BUSY\n") );
			}
		}

		status = STATUS_SUCCESS;

	} finally {

		for (ridx = 0; ridx < targetChildrenCount; ridx++) {

			if (nextCcb[ridx]) {

				LsCcbFree( nextCcb[ridx] );
				nextCcb[ridx] = NULL;
			}

			if (customBuffer.DataBufferAllocated == TRUE) {

				if (customBuffer.DataBuffer[ridx]) {

					ExFreePoolWithTag( customBuffer.DataBuffer[ridx], CCB_CUSTOM_BUFFER_POOL_TAG );
				}
			}
		}
	}

	return status;
}

NTSTATUS
NdasRaidClientSynchronousCcbCompletion (
	IN PCCB	Ccb,
	IN PCCB	ChildCcb
	)
{
	KIRQL				oldIrql;


	if (ChildCcb->CcbStatus != CCB_STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ChildCcb status %x\n", ChildCcb->CcbStatus) );
	}

	if (ChildCcb->OperationCode == CCB_OPCODE_STOP) {

		ACQUIRE_SPIN_LOCK( &Ccb->CcbSpinLock, &oldIrql );

		switch (ChildCcb->CcbStatus) {
	
		case CCB_STATUS_SUCCESS:
	
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;		
			break;

		default:

			NDASSCSI_ASSERT( FALSE );
			Ccb->CcbStatus = ChildCcb->CcbStatus;

			break;
		}
	
		LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

		RELEASE_SPIN_LOCK( &Ccb->CcbSpinLock, oldIrql );

		return STATUS_SUCCESS;
	}

	// Find proper Ccbstatus based on OriginalCcb->CcbStatus(Empty or may contain first completed child Ccb's staus) and this Ccb.

	if (ChildCcb->CcbStatus != CCB_STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, ("LurnRAID1RCcbCompletion: CcbStatus = %x\n", ChildCcb->CcbStatus) );
	}

	ACQUIRE_SPIN_LOCK( &Ccb->CcbSpinLock, &oldIrql );

	if (ChildCcb->OperationCode == CCB_OPCODE_UPDATE) {

		switch (ChildCcb->CcbStatus) {

		case CCB_STATUS_SUCCESS:

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			Ccb->CcbStatus = STATUS_SUCCESS;
			break;

		case CCB_STATUS_NO_ACCESS:

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );

			Ccb->CcbStatus = CCB_STATUS_NO_ACCESS;
	
			break;

		case CCB_STATUS_BUSY: 

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			Ccb->CcbStatus = CCB_STATUS_BUSY;

			break;

		case CCB_STATUS_STOP:

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			Ccb->CcbStatus = CCB_STATUS_STOP;

		default: 

			NDASSCSI_ASSERT( FALSE );
			Ccb->CcbStatus = ChildCcb->CcbStatus;
		}

		LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );
	
	} else if (ChildCcb->OperationCode == CCB_OPCODE_QUERY		||
			   ChildCcb->OperationCode == CCB_OPCODE_RESETBUS	||
			   ChildCcb->OperationCode == CCB_OPCODE_NOOP) {

		NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

		switch (ChildCcb->CcbStatus) {
	
		case CCB_STATUS_SUCCESS:
	
			Ccb->CcbStatus = STATUS_SUCCESS;
			break;

		case CCB_STATUS_BUSY:

			Ccb->CcbStatus = CCB_STATUS_BUSY;
			break;

		case CCB_STATUS_STOP:
		
			Ccb->CcbStatus = CCB_STATUS_STOP;
			break;
	
		case CCB_STATUS_NOT_EXIST:
		
			Ccb->CcbStatus = CCB_STATUS_NOT_EXIST;
			break;

		default:			

			NDASSCSI_ASSERT( FALSE );

			Ccb->CcbStatus = ChildCcb->CcbStatus;
			break;
		}
	
	} else if (ChildCcb->OperationCode == CCB_OPCODE_EXECUTE) {

		switch (ChildCcb->Cdb[0]) {
		
		case 0x3E:			// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:	

			if (((PLURELATION_NODE)(Ccb->CcbCurrentStackLocation->Lurn))->LurnType == LURN_NDAS_RAID1) {

				NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );
			
			} else {

				NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );
			}

			switch (ChildCcb->CcbStatus) {
				
			case CCB_STATUS_SUCCESS:
	
				Ccb->CcbStatus = STATUS_SUCCESS;
				break;

			case CCB_STATUS_BUSY:

				Ccb->CcbStatus = CCB_STATUS_BUSY;
				break;

			case CCB_STATUS_STOP:
		
				Ccb->CcbStatus = CCB_STATUS_STOP;
				break;
	
			default:			

				NDASSCSI_ASSERT( FALSE );

				Ccb->CcbStatus = ChildCcb->CcbStatus;
				break;
			}

			if (Ccb->CcbStatus != STATUS_SUCCESS) {

				if (Ccb->SenseBuffer != NULL) {

					RtlCopyMemory( Ccb->SenseBuffer, ChildCcb->SenseBuffer, ChildCcb->SenseDataLength );
				}
			}

			break;

		case SCSIOP_MODE_SELECT:

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			switch (ChildCcb->CcbStatus) {
	
			case CCB_STATUS_SUCCESS:
	
				Ccb->CcbStatus = STATUS_SUCCESS;
				break;

			case CCB_STATUS_BUSY:

				Ccb->CcbStatus = CCB_STATUS_BUSY;
				break;

			case CCB_STATUS_STOP:
		
				Ccb->CcbStatus = CCB_STATUS_STOP;
				break;

			case CCB_STATUS_INVALID_COMMAND:

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				break;

			default:			

				NDASSCSI_ASSERT( FALSE );

				Ccb->CcbStatus = ChildCcb->CcbStatus;
				break;
			}

			if (Ccb->CcbStatus != STATUS_SUCCESS) {

				if (Ccb->SenseBuffer != NULL) {

					RtlCopyMemory( Ccb->SenseBuffer, ChildCcb->SenseBuffer, ChildCcb->SenseDataLength );
				}
			}

			break;

		case SCSIOP_MODE_SENSE: {

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			switch (ChildCcb->CcbStatus) {
	
			case CCB_STATUS_SUCCESS:
	
				Ccb->CcbStatus = STATUS_SUCCESS;
				break;

			case CCB_STATUS_BUSY:

				Ccb->CcbStatus = CCB_STATUS_BUSY;
				break;

			case CCB_STATUS_STOP:
		
				Ccb->CcbStatus = CCB_STATUS_STOP;
				break;
	
			default:			

				NDASSCSI_ASSERT( FALSE );

				Ccb->CcbStatus = ChildCcb->CcbStatus;
				break;
			}

			if (ChildCcb->CcbStatus == CCB_STATUS_SUCCESS) {
		
				// Update original CCB's MODE_SENSE data.
		
				PMODE_CACHING_PAGE	ccbCachingPage;
				PMODE_CACHING_PAGE	childCcbCachingPage;
				UINT32				cachingPageOffset;

				cachingPageOffset = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK);
		
				ccbCachingPage = (PMODE_CACHING_PAGE)(((PUCHAR)Ccb->DataBuffer) + cachingPageOffset);
				childCcbCachingPage = (PMODE_CACHING_PAGE)(((PUCHAR)ChildCcb->DataBuffer) + cachingPageOffset);
		
				if (childCcbCachingPage->WriteCacheEnable == 0) {

					ccbCachingPage->WriteCacheEnable = 0;
				}

				if (childCcbCachingPage->ReadDisableCache == 1) {
		
					ccbCachingPage->ReadDisableCache = 1;	
				}
			}

			break;
		}
		
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:
		case SCSIOP_VERIFY:
		case SCSIOP_VERIFY16:		 // fall through
		default:

			NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			switch (ChildCcb->CcbStatus) {
	
			case CCB_STATUS_SUCCESS:
	
				Ccb->CcbStatus = STATUS_SUCCESS;
				break;

			case CCB_STATUS_BUSY:

				Ccb->CcbStatus = CCB_STATUS_BUSY;
				break;

			case CCB_STATUS_STOP:
		
				Ccb->CcbStatus = CCB_STATUS_STOP;
				break;
	
			case CCB_STATUS_NOT_EXIST:

				Ccb->CcbStatus = CCB_STATUS_NOT_EXIST;
				break;

			default:			

				NDASSCSI_ASSERT( FALSE );

				Ccb->CcbStatus = ChildCcb->CcbStatus;
				break;
			}

			if (Ccb->CcbStatus != STATUS_SUCCESS) {

				if (Ccb->SenseBuffer != NULL) {

					RtlCopyMemory( Ccb->SenseBuffer, ChildCcb->SenseBuffer, ChildCcb->SenseDataLength );
				}
			}

			break;
		}

	} else {

		NDASSCSI_ASSERT( FALSE );
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;			
	}

	LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

	RELEASE_SPIN_LOCK( &Ccb->CcbSpinLock, oldIrql );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientUpdateCcbCompletion (
	IN PCCB Ccb,
	IN PCCB ChildCcb
	)
{
	PLURELATION_NODE	lurn;
	KIRQL				oldIrql;
	BOOLEAN				failAll = FALSE;
		
	lurn = Ccb->CcbCurrentStackLocation->Lurn;

	ACQUIRE_SPIN_LOCK( &Ccb->CcbSpinLock, &oldIrql );


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ChildCcb update status %x\n", ChildCcb->CcbStatus) );

	switch (ChildCcb->CcbStatus) {

	case CCB_STATUS_SUCCESS:

		if (Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS) {

			Ccb->CcbStatus = STATUS_SUCCESS;
		}

		break;

	case CCB_STATUS_NO_ACCESS:

		NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );

		Ccb->CcbStatus = CCB_STATUS_NO_ACCESS;
	
		break;

	case CCB_STATUS_BUSY: 

		NDASSCSI_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

		Ccb->CcbStatus = CCB_STATUS_BUSY;

		break;

	default: 

		NDASSCSI_ASSERT( FALSE );
	}

	LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

	RELEASE_SPIN_LOCK( &Ccb->CcbSpinLock, oldIrql );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientRefreshCcbStatusFlag (
	IN  PNDASR_CLIENT	NdasrClient,
	OUT PULONG			CcbStatusFlags
	)
{
	NTSTATUS					status;

	CCB							ccb;

	PLUR_QUERY					lurQuery;
	BYTE						lurBuffer[SIZE_OF_LURQUERY(0, sizeof(LURN_REFRESH))];
	PLURN_REFRESH				lurnRefresh;

	return STATUS_SUCCESS;

	//
	//	initialize query CCB
	//
	LSCCB_INITIALIZE( &ccb );

	ccb.OperationCode = CCB_OPCODE_QUERY;
	LsCcbSetFlag( &ccb, CCB_FLAG_SYNCHRONOUS );

	RtlZeroMemory( lurBuffer, sizeof(lurBuffer) );
	
	lurQuery = (PLUR_QUERY)lurBuffer;
	lurQuery->InfoClass = LurRefreshLurn;
	lurQuery->QueryDataLength = 0;

	lurnRefresh = (PLURN_REFRESH)LUR_QUERY_INFORMATION(lurQuery);
	lurnRefresh->Length = sizeof(LURN_REFRESH);

	ccb.DataBuffer = lurQuery;
	ccb.DataBufferLength = lurQuery->Length;

	ccb.CcbCurrentStackLocation->Lurn = NdasrClient->Lurn;

	status = NdasRaidClientQuery( NdasrClient, &ccb );

	if (!NT_SUCCESS(status)) {

		NDASSCSI_ASSERT( FALSE );

		KDPrint( 1,("LurnRequest() failed.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	*CcbStatusFlags |= lurnRefresh->CcbStatusFlags;

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientFlush (
	PNDASR_CLIENT	NdasrClient 
	)
{
	NTSTATUS			status;
	KIRQL				oldIrql;
	PNDASR_CLIENT_LOCK	lock;
	PLIST_ENTRY			listEntry;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Called\n") );

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	if (IsListEmpty(&NdasrClient->LockList)) {

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LockList Empty\n") );

		return STATUS_SUCCESS;
	}

	for (listEntry = NdasrClient->LockList.Flink;
		 listEntry != &NdasrClient->LockList;
		 listEntry = listEntry->Flink) {

		lock = CONTAINING_RECORD (listEntry, NDASR_CLIENT_LOCK, Link);

		NDASSCSI_ASSERT( lock->Status == LOCK_STATUS_GRANTED || lock->Status == LOCK_STATUS_HOLDING );

		if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
			
			NDASSCSI_ASSERT( FALSE );
		}

		lock->Status = LOCK_STATUS_HOLDING;
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_MODE) );

	status = NdasRaidClientSendRequestReleaseLock( NdasrClient, NRIX_LOCK_ID_ALL, NULL );

	if (status != STATUS_SUCCESS) {

		return status;
	}

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
	
	while (!IsListEmpty(&NdasrClient->LockList)) {
	
		listEntry = RemoveHeadList( &NdasrClient->LockList );
		
		lock = CONTAINING_RECORD(listEntry, NDASR_CLIENT_LOCK, Link);

		NDASSCSI_ASSERT( lock->Status == LOCK_STATUS_HOLDING );
		
		NdasRaidClientFreeLock( lock );
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	return status;
}

NTSTATUS
NdasRaidClientReleaseBlockIoPermissionToClient (
	PNDASR_CLIENT	NdasrClient,
	PCCB			Ccb
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PLURELATION_NODE	lurn = NdasrClient->Lurn;
	PNDASR_INFO			ndasrInfo = lurn->NdasrInfo;
	KIRQL				oldIrql;
	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	clientLock;
	UINT64				addr;
	UINT32				length;
	UINT32				overlapStatus;

	LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &addr, &length );

	if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0) {

		return STATUS_SUCCESS;
	}
		
	ACQUIRE_SPIN_LOCK( &ndasrInfo->SpinLock, &oldIrql );

	ACQUIRE_DPC_SPIN_LOCK( &NdasrClient->SpinLock );

	// to do: save used lock info to Ccb instead of searching again.

	for (listEntry = NdasrClient->LockList.Flink;
		 listEntry != &NdasrClient->LockList;
		 listEntry = listEntry->Flink) {

		clientLock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );
		
		overlapStatus = NdasRaidGetOverlappedRange( addr, 
													length,
													clientLock->BlockAddress,
													clientLock->BlockLength,
													NULL, 
													NULL );

		if (overlapStatus == NDASR_RANGE_NO_OVERLAP) {
					
		} else {
		
			InterlockedDecrement( &clientLock->InUseCount );

			ASSERT( clientLock->InUseCount >= 0 );
			
			KeQueryTickCount( &clientLock->LastAccesseTick );
		}
	}

	RELEASE_DPC_SPIN_LOCK( &NdasrClient->SpinLock );
	
	RELEASE_SPIN_LOCK( &ndasrInfo->SpinLock, oldIrql );

	return status;
}

VOID
NdasRaidClientRefreshRaidStatusWithoutArbiter (
	PNDASR_CLIENT	NdasrClient
	) 
{
	KIRQL	oldIrql;
	UCHAR	nidx, ridx;
	UCHAR	newNdasrState;


	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	// Update LocalNodeFlags to NdasrNodeFlags
	
	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
		NdasrClient->NdasrNodeFlags[nidx] = NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx];

		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {

			NdasrClient->NdasrNodeFlags[nidx] = NRIX_NODE_FLAG_DEFECTIVE;
		}
	}
	
	for (ridx = 0; ridx < NdasrClient->Lurn->LurnChildrenCnt; ridx++) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
					("MAPPING Lurn node %d to RAID role %d\n", NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx, ridx) );

		NdasrClient->RoleToNodeMap[ridx] = (UCHAR)NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx;
		NdasrClient->NodeToRoleMap[NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx] = (UCHAR)ridx;

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

			if (ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount) {

				NDASSCSI_ASSERT( !FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) );
				NDASSCSI_ASSERT( NdasrClient->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE || 
								 NdasrClient->OutOfSyncRoleIndex == ridx );

				NdasrClient->OutOfSyncRoleIndex = (UCHAR)ridx;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is out-of-sync\n",  NdasrClient->RoleToNodeMap[ridx], ridx) );
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting out of sync role: %d\n", NdasrClient->OutOfSyncRoleIndex) );
			}
		}

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

			NDASSCSI_ASSERT( ridx >= NdasrClient->Lurn->NdasrInfo->ActiveDiskCount );

			SetFlag(  NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_DEFECTIVE );
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is defective\n",   NdasrClient->RoleToNodeMap[ridx], ridx) );
			
			SetFlag( NdasrClient->NdasrDefectCodes[NdasrClient->RoleToNodeMap[ridx]], 
					 NdasRaidRmdUnitStatusToDefectCode(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus) );
		}
	}

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

		NDASSCSI_ASSERT( NdasrClient->NdasrNodeFlags[nidx] == NRIX_NODE_FLAG_RUNNING	||
						 NdasrClient->NdasrNodeFlags[nidx] == NRIX_NODE_FLAG_DEFECTIVE	||
						 NdasrClient->NdasrNodeFlags[nidx] == NRIX_NODE_FLAG_STOP );
	}

	// Test new RAID status only when needed, i.e: node has changed or first time.

	newNdasrState = NRIX_RAID_STATE_NORMAL;

	if (NdasrClient->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE) {

		newNdasrState = NRIX_RAID_STATE_OUT_OF_SYNC;
	}

	for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { // i : role index
			
		if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_RUNNING)) {

			switch (newNdasrState) {

			case NRIX_RAID_STATE_NORMAL: {

				NdasrClient->OutOfSyncRoleIndex = ridx;
				newNdasrState = NRIX_RAID_STATE_DEGRADED;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting new out of sync role: %d\n", ridx) );

				break;
			}

			case NRIX_RAID_STATE_OUT_OF_SYNC: {

				if (NdasrClient->OutOfSyncRoleIndex == ridx) {
					
					newNdasrState = NRIX_RAID_STATE_DEGRADED;
				
				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrClient->RoleToNodeMap[ridx]) );

					newNdasrState = NRIX_RAID_STATE_FAILED;
				}

				break;
			}

			case NRIX_RAID_STATE_DEGRADED: {

				NDASSCSI_ASSERT( NdasrClient->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE && 
								 NdasrClient->OutOfSyncRoleIndex != ridx );
				
				newNdasrState = NRIX_RAID_STATE_FAILED;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrClient->RoleToNodeMap[ridx]) );

				break;
			}

			default:

				break;			
			}
		}
	}

	NDASSCSI_ASSERT( newNdasrState == NRIX_RAID_STATE_NORMAL || newNdasrState == NRIX_RAID_STATE_OUT_OF_SYNC );

	if (NdasrClient->NdasrState != newNdasrState) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Changing DRAID Status from %x to %x\n", NdasrClient->NdasrState, newNdasrState) );

		NdasrClient->NdasrState = newNdasrState;
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
}

NTSTATUS
NdasRaidClientEstablishArbiter (
	PNDASR_CLIENT	NdasrClient
	) 
{
	NTSTATUS		status;

	PNRIX_HEADER	message;
	ULONG			msgLength;

	LONG			result;
	ULONG			addtionalLength;


	UNREFERENCED_PARAMETER( NdasrClient );
	
	do {

		status = NdasRaidClientConnectToArbiter( NdasrClient, &NdasrClient->NotificationConnection );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to connect notification\n") );
			break;
		}

		// Register as notification type
	
		status = NdasRaidClientRegisterToRemoteArbiter( NdasrClient, &NdasrClient->NotificationConnection, NRIX_CONN_TYPE_NOTIFICATION );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to register connect notification\n") );
			break;
		}
	
		status = NdasRaidClientConnectToArbiter( NdasrClient, &NdasrClient->RequestConnection );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to connect request\n") );
			break;
		}

		// Register as request type

		status = NdasRaidClientRegisterToRemoteArbiter( NdasrClient, &NdasrClient->RequestConnection, NRIX_CONN_TYPE_REQUEST );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to register connect request\n") );
			break;
		}

		NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_CONNECTED >> 16;
		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED );

		// Wait for network event of notification connection 

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Connected to arbiter\n") );	

		result = 0;
	
		status = LpxTdiV2Recv( NdasrClient->NotificationConnection.ConnectionFileObject, 
							   (PUCHAR)NdasrClient->NotificationConnection.ReceiveBuf,
							   sizeof(NRIX_HEADER),
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRIX_HEADER)) {

			status = STATUS_CONNECTION_DISCONNECTED;
			break;
		}

		message = (PNRIX_HEADER)NdasrClient->NotificationConnection.ReceiveBuf;
		msgLength = NTOHS(message->Length);
	
		if (msgLength > NRIX_MAX_REQUEST_SIZE || msgLength < sizeof(NRIX_HEADER)) {

			NDASSCSI_ASSERT( FALSE );
			status = STATUS_CONNECTION_DISCONNECTED;
			break;
		}

		if (msgLength != sizeof(NRIX_HEADER)) {

			result = 0;
			addtionalLength = msgLength - sizeof(NRIX_HEADER);
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reading additional message data %d bytes\n", addtionalLength) );
		
			status = LpxTdiV2Recv( NdasrClient->NotificationConnection.ConnectionFileObject, 
								   (PUCHAR)(NdasrClient->NotificationConnection.ReceiveBuf + sizeof(NRIX_HEADER)),
								   addtionalLength,
								   0, 
								   NULL, 
								   NULL,
								   0,
								   &result );

			if (result != addtionalLength) {
	
				status = STATUS_CONNECTION_DISCONNECTED;
				break;
			}
		}

		status = NdasRaidClientHandleNotificationMsg( NdasrClient, 
													   (PNRIX_HEADER)NdasrClient->NotificationConnection.ReceiveBuf );

		if (status != STATUS_SUCCESS) {

			break;
		}
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		if (NdasrClient->RequestConnection.State == NDASR_CLIENT_CONNECTION_STATE_CONNECTED ||
			NdasrClient->RequestConnection.State == NDASR_CLIENT_CONNECTION_STATE_REGISTERED) {

			NdasRaidClientDisconnectFromArbiter( NdasrClient, &NdasrClient->RequestConnection );
		}

		if (NdasrClient->NotificationConnection.State == NDASR_CLIENT_CONNECTION_STATE_CONNECTED ||
			NdasrClient->NotificationConnection.State == NDASR_CLIENT_CONNECTION_STATE_REGISTERED) {

			NdasRaidClientDisconnectFromArbiter( NdasrClient, &NdasrClient->NotificationConnection );
		}

		NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITER_DISCONNECTED >> 16;
		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED );
	}
	
	return status;
}

NTSTATUS
NdasRaidClientConnectToArbiter (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	) 
{
	NTSTATUS			status;	

	KIRQL				oldIrql;
	PLURNEXT_IDE_DEVICE	ideDisk;
	LPX_ADDRESS			localAddr = {0};
	LPX_ADDRESS			remoteAddr = {0};
	UCHAR				nidx;
	UCHAR				i;
	HANDLE				addressFileHandle = NULL;
	PFILE_OBJECT		addressFileObject = NULL;
	HANDLE				connectionFileHandle = NULL;
	PFILE_OBJECT		connectionFileObject = NULL;
	BOOLEAN				connected;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Connecting to arbiter\n") );
	
	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);

	NDASSCSI_ASSERT( Connection->State == NDASR_CLIENT_CONNECTION_STATE_INIT );

	Connection->State = NDASR_CLIENT_CONNECTION_STATE_CONNECTING;

	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);

	// Find local address to use.

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
		// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
	
		if (!FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRIX_NODE_FLAG_RUNNING) ||
			FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {
	
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Lurn is not running. Skip reading node %d.\n", nidx) );
			
			continue;
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)NdasrClient->Lurn->LurnChildren[nidx]->LurnExtension;

		if (!ideDisk) {

			continue;
		}

#if !__NDAS_SCSI_LPXTDI_V2__
		RtlCopyMemory( &localAddr, &ideDisk->LanScsiSession->BindAddress.Address[0].Address, sizeof(LPX_ADDRESS) );
#else

		RtlCopyMemory( &localAddr, 
					   ideDisk->LanScsiSession->NdasBindAddress.Address, 
					   sizeof(LPX_ADDRESS) );
#endif
		break;
	}

	if (nidx == NdasrClient->Lurn->LurnChildrenCnt) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("No local address to use\n") );

		status = STATUS_UNSUCCESSFUL;

		goto errout;
	}

	connected = FALSE;

	// Try to connect for each arbiter address
	
	for (i=0; i < NDAS_DRAID_ARBITER_ADDR_COUNT; i++) {

		if (NdasrClient->Lurn->NdasrInfo->Rmd.ArbiterInfo[i].Type != NDAS_DRAID_ARBITER_TYPE_LPX) {

			NDASSCSI_ASSERT( NdasrClient->Lurn->NdasrInfo->Rmd.ArbiterInfo[i].Type == NDAS_DRAID_ARBITER_TYPE_NONE );
			continue;
		}

		RtlCopyMemory( remoteAddr.Node, NdasrClient->Lurn->NdasrInfo->Rmd.ArbiterInfo[i].Addr, 6 );
		remoteAddr.Port = HTONS(NRIX_ARBITER_PORT_NUM_BASE);

		status = LpxTdiV2OpenAddress( &localAddr, &addressFileHandle, &addressFileObject );

		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( FALSE );

			addressFileHandle = NULL;
			addressFileObject = NULL;

			continue;
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Connecting from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X to %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
					 localAddr.Node[0], localAddr.Node[1], localAddr.Node[2],
					 localAddr.Node[3], localAddr.Node[4], localAddr.Node[5],
					 NTOHS(localAddr.Port),
					 remoteAddr.Node[0], remoteAddr.Node[1], remoteAddr.Node[2],
					 remoteAddr.Node[3], remoteAddr.Node[4], remoteAddr.Node[5],
					 NTOHS(remoteAddr.Port)) );

		status = LpxTdiV2OpenConnection( NULL,
										 &connectionFileHandle, 
										 &connectionFileObject, 
										 &Connection->ReceiveOverlapped  );

		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( FALSE );

			LpxTdiV2CloseAddress( addressFileHandle, addressFileObject );
			addressFileHandle = NULL; 
			addressFileObject = NULL;

			continue;
		}

		status = LpxTdiV2AssociateAddress( connectionFileObject, addressFileHandle );

		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( FALSE );

			LpxTdiV2CloseAddress( addressFileHandle, addressFileObject );
			addressFileHandle = NULL; 
			addressFileObject = NULL;

			LpxTdiV2CloseConnection( connectionFileHandle, connectionFileObject, &Connection->ReceiveOverlapped );
			connectionFileHandle = NULL; 
			connectionFileObject = NULL;
			
			continue;
		}

		status = LpxTdiV2Connect( connectionFileObject, &remoteAddr, NULL, NULL );
		
		if (!NT_SUCCESS(status)) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Connection failed\n") );

			LpxTdiV2DisassociateAddress( connectionFileObject );
			
			LpxTdiV2CloseConnection( connectionFileHandle, connectionFileObject, &Connection->ReceiveOverlapped );
			connectionFileHandle = NULL; 
			connectionFileObject = NULL;
			
			LpxTdiV2CloseAddress ( addressFileHandle, addressFileObject );
			addressFileHandle = NULL; 
			addressFileObject = NULL;
			continue;
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Connected\n") );
		connected = TRUE;

		break;
	}

	if (!connected) {

		status = STATUS_UNSUCCESSFUL;
		goto errout;
	}

	//
	// Store connection info.
	//
	
	Connection->AddressFileHandle = addressFileHandle;
	Connection->AddressFileObject = addressFileObject;
	Connection->ConnectionFileHandle = connectionFileHandle;
	Connection->ConnectionFileObject = connectionFileObject;
	Connection->RemoteAddr = remoteAddr;
	Connection->LocalAddr = localAddr;
	
	ACQUIRE_SPIN_LOCK( &Connection->SpinLock, &oldIrql );
	Connection->State = NDASR_CLIENT_CONNECTION_STATE_CONNECTED;
	RELEASE_SPIN_LOCK( &Connection->SpinLock, oldIrql );
	
	return STATUS_SUCCESS;

errout:
	
	ACQUIRE_SPIN_LOCK( &Connection->SpinLock, &oldIrql );
	Connection->State = NDASR_CLIENT_CONNECTION_STATE_INIT;
	RELEASE_SPIN_LOCK( &Connection->SpinLock, oldIrql );
	
	return status;
}

NTSTATUS
NdasRaidClientDisconnectFromArbiter (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	) 
{
	KIRQL oldIrql;

	UNREFERENCED_PARAMETER( NdasrClient );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Disconnecting from arbiter\n") );
	
	if (Connection->State == NDASR_CLIENT_CONNECTION_STATE_INIT) {

		NDASSCSI_ASSERT( FALSE );
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Already disconnected\n") );
		
		return STATUS_SUCCESS;
	}

	Connection->Sequence = 0;

	if (LpxTdiV2IsRequestPending(&Connection->ReceiveOverlapped, 0)) {

		LpxTdiV2CancelRequest( Connection->ConnectionFileObject, &Connection->ReceiveOverlapped, 0, FALSE, 0 );
	}

	NDASSCSI_ASSERT( !LpxTdiV2IsRequestPending(&Connection->ReceiveOverlapped, 0) );
	
	if (Connection->ConnectionFileObject) {

		LpxTdiV2Disconnect( Connection->ConnectionFileObject, 0 );
		LpxTdiV2DisassociateAddress( Connection->ConnectionFileObject );
		LpxTdiV2CloseConnection( Connection->ConnectionFileHandle, Connection->ConnectionFileObject, &Connection->ReceiveOverlapped );
		
		Connection->ConnectionFileHandle = NULL;
		Connection->ConnectionFileObject = NULL;
	}
	
	if (Connection->AddressFileHandle) {
	
		LpxTdiV2CloseAddress( Connection->AddressFileHandle, Connection->AddressFileObject );

		Connection->AddressFileHandle = NULL; 
		Connection->AddressFileObject = NULL;
	}

	ACQUIRE_SPIN_LOCK( &Connection->SpinLock, &oldIrql );
	Connection->State = NDASR_CLIENT_CONNECTION_STATE_INIT;
	RELEASE_SPIN_LOCK( &Connection->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientRegisterToRemoteArbiter (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection,
	UCHAR						ConType	// NRIX_CONN_TYPE_*
	) 
{
	NTSTATUS		status;

	NRIX_REGISTER 	registerMsg = {0};
	NRIX_HEADER		replyMsg = {0};
	LONG			result;
	KIRQL			oldIrql;
	
	ASSERT( Connection->State == NDASR_CLIENT_CONNECTION_STATE_CONNECTED );

	Connection->Sequence++;

	registerMsg.Header.Signature = HTONL(NRIX_SIGNATURE);
	registerMsg.Header.Command = NRIX_CMD_REGISTER;
	registerMsg.Header.Length = HTONS((UINT16)sizeof(NRIX_REGISTER));
	registerMsg.Header.ReplyFlag = 0;
	registerMsg.Header.Sequence =  HTONS((UINT16)Connection->Sequence);
	registerMsg.Header.Result = 0;

	registerMsg.ConnType	= ConType;
	registerMsg.Usn			= HTONL( NdasrClient->Lurn->NdasrInfo->Rmd.uiUSN );
	registerMsg.LocalClient = LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) || 
							  FlagOn( NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE );

	RtlCopyMemory( &registerMsg.RaidSetId, &NdasrClient->Lurn->NdasrInfo->RaidSetId, sizeof(GUID) );
	RtlCopyMemory( &registerMsg.ConfigSetId, &NdasrClient->Lurn->NdasrInfo->ConfigSetId, sizeof(GUID) );	
		
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending register message to remote arbiter\n") );

	// Send register message

	status = LpxTdiV2Send( Connection->ConnectionFileObject, 
						   (PUCHAR)&registerMsg, 
						   sizeof(NRIX_REGISTER), 
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != sizeof(NRIX_REGISTER)) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send %x\n", status) );

		return STATUS_CONNECTION_DISCONNECTED;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend status=%x, result=%x.\n", status, result) );

	status = LpxTdiV2Recv( Connection->ConnectionFileObject, 
						   (PUCHAR)&replyMsg, 
						   sizeof(NRIX_HEADER),
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != sizeof(NRIX_HEADER)) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to recv %x\n", status) );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	// Check received message
	
	if (NTOHL(replyMsg.Signature) != NRIX_SIGNATURE ||
		replyMsg.Command != NRIX_CMD_REGISTER		|| 
		replyMsg.ReplyFlag !=1 ||
		NTOHS(replyMsg.Length) !=  sizeof(NRIX_HEADER)) {
		
		NDASSCSI_ASSERT( FALSE );
		return STATUS_UNSUCCESSFUL;
	}
	
	switch (replyMsg.Result) {
	
	case NRIX_RESULT_SUCCESS:

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Registration succeeded.\n") );
		
		ACQUIRE_SPIN_LOCK( &Connection->SpinLock, &oldIrql );
		Connection->State = NDASR_CLIENT_CONNECTION_STATE_REGISTERED;
		RELEASE_SPIN_LOCK( &Connection->SpinLock, oldIrql );	
		
		return STATUS_SUCCESS;
	
	case NRIX_RESULT_FAIL:
	case NRIX_RESULT_RAID_SET_NOT_FOUND:
	default:
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Registration failed with result %s\n", NdasRixGetResultString(replyMsg.Result)) );
		
		return STATUS_UNSUCCESSFUL;
	}
}

NTSTATUS 
NdasRaidClientResetRemoteArbiterContext (
	PNDASR_CLIENT		NdasrClient
	) 
{
	KIRQL				oldIrql;
	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	lock;
	LARGE_INTEGER		timeout;
	BOOLEAN				lockInUse;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Resetting remote arbiter context\n") );
	
	NdasRaidClientDisconnectFromArbiter( NdasrClient, &NdasrClient->RequestConnection );	
	NdasRaidClientDisconnectFromArbiter( NdasrClient, &NdasrClient->NotificationConnection );

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
	
	// Remove lock contexts

	while (!IsListEmpty(&NdasrClient->LockList)) {

		lockInUse = FALSE;

		listEntry = NdasrClient->LockList.Flink;

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );

		if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {

			NDASSCSI_ASSERT( FALSE );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Lock %I64x:%x is in use. Waiting for freed\n", lock->BlockAddress, lock->BlockLength) );
			
			timeout.QuadPart = - HZ /2;

			// Wait until lock is not in use.
			
			RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
			
			KeDelayExecutionThread( KernelMode, FALSE, &timeout );
			
			ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

		} else {

			// No one is using the lock. Free it and restart outer loop

			RemoveEntryList( &lock->Link );
			NdasRaidClientFreeLock( lock );
		}
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}


NTSTATUS
NdasRaidClientSendRequestAcquireLock (
	PNDASR_CLIENT	NdasrClient,
	UCHAR			LockType,		// NRIX_LOCK_TYPE_BLOCK_IO
	UCHAR			LockMode,		// NRIX_LOCK_MODE_*
	UINT64			BlockAddress,	// in sector for block IO lock
	UINT32			BlockLength,	// in sector.
	PLARGE_INTEGER	Timeout
   )
{
	NTSTATUS					status;

	NRIX_ACQUIRE_LOCK			acquireLockRequest;
	PNRIX_ACQUIRE_LOCK_REPLY	acquireLockReply;
	UINT32						msgLength;
	LONG						result;
	PNDASR_CLIENT_LOCK			lock;
	

	NDASSCSI_ASSERT( Timeout == NULL );
	NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
			    ("Sending NRIX_CMD_ACQUIRE_LOCK message BlockAddress = %I64x, BlockLength = %x\n", BlockAddress, BlockLength) );

	msgLength = sizeof(NRIX_ACQUIRE_LOCK);

	RtlZeroMemory( &acquireLockRequest, msgLength );

	acquireLockRequest.Header.Signature	= NTOHL(NRIX_SIGNATURE);
	acquireLockRequest.Header.Command	= NRIX_CMD_ACQUIRE_LOCK;
	acquireLockRequest.Header.Length	= NTOHS((UINT16)msgLength);
	acquireLockRequest.Header.Sequence	= NTOHL(NdasrClient->RequestSequence);
	acquireLockRequest.Header.Usn		= NTOHL(NdasrClient->Usn );

	NdasrClient->RequestSequence++;

	acquireLockRequest.LockType = LockType;
	acquireLockRequest.LockMode	= LockMode;
	acquireLockRequest.BlockAddress = NTOHLL(BlockAddress);
	acquireLockRequest.BlockLength = NTOHL(BlockLength);

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		NDASR_LOCAL_MSG		ndasrRequestLocalMsg;
		
		PLIST_ENTRY			listEntry;
		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
		
		InitializeListHead( &ndasrRequestLocalMsg.ListEntry );
		ndasrRequestLocalMsg.NrixHeader = (PNRIX_HEADER)&acquireLockRequest;

		ExInterlockedInsertTailList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.RequestQueue,
									 &ndasrRequestLocalMsg.ListEntry,
									 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.RequestEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
										Executive, 
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		KeClearEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent );

		listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
												 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		NDASSCSI_ASSERT( listEntry );

		ndasrReplyLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );
		acquireLockReply = (PNRIX_ACQUIRE_LOCK_REPLY)ndasrReplyLocalMsg->NrixHeader;

		NDASSCSI_ASSERT( ndasrReplyLocalMsg == &NdasrClient->Lurn->NdasrInfo->RequestChannelReply );
		NDASSCSI_ASSERT( (PNRIX_HEADER)acquireLockReply == &NdasrClient->Lurn->NdasrInfo->NrixHeader );

	} else {

		result = 0;

		status = LpxTdiV2Send( NdasrClient->RequestConnection.ConnectionFileObject, 
							   (PUCHAR)&acquireLockRequest, 
							   msgLength,
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );
	
		if (result != msgLength) {

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		acquireLockReply = (PNRIX_ACQUIRE_LOCK_REPLY)NdasrClient->RequestConnection.ReceiveBuf;
	
		result = 0;

		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject,
							   (PUCHAR)acquireLockReply,
							   sizeof(NRIX_HEADER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRIX_HEADER)) {

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to received request header\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	if (acquireLockReply->Header.Signature != HTONL(NRIX_SIGNATURE)	||
		acquireLockReply->Header.Command != NRIX_CMD_ACQUIRE_LOCK	||
		!acquireLockReply->Header.ReplyFlag) {

		NDASSCSI_ASSERT( FALSE );
	
		return STATUS_CONNECTION_DISCONNECTED;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
				("acquireLockReply->Header.Result = %x\n", acquireLockReply->Header.Result) );

	if (acquireLockReply->Header.Result == NRIX_RESULT_LOWER_USN) {

		NDASSCSI_ASSERT( acquireLockReply->Header.RaidInformation == TRUE );

		status = NdasRaidClientProcessNodeChageReply( NdasrClient, (PNRIX_CHANGE_STATUS)acquireLockReply );

		if (status != STATUS_SUCCESS) {

			return status;
		}

		if (NdasrClient->WaitSyncForWrite) {

			return STATUS_SYNCHRONIZATION_REQUIRED;
		}

		NDASSCSI_ASSERT( FALSE );
	}
	
	if (acquireLockReply->Header.Result == NRIX_RESULT_WAIT_SYNC_FOR_WRITE) {

		NDASSCSI_ASSERT( NdasrClient->WaitSyncForWrite == FALSE );

		NdasrClient->WaitSyncForWrite = TRUE;

		return STATUS_SYNCHRONIZATION_REQUIRED;
	}

	if (acquireLockReply->Header.Result != NRIX_RESULT_GRANTED) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CONNECTION_DISCONNECTED;	
	}

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {
	
	} else {

		result = 0;
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Reading additional message data %d bytes\n", sizeof(NRIX_ACQUIRE_LOCK_REPLY) - sizeof(NRIX_HEADER)) );
		
		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject, 
							   (PUCHAR)(acquireLockReply) + sizeof(NRIX_HEADER),
							   sizeof(NRIX_ACQUIRE_LOCK_REPLY) - sizeof(NRIX_HEADER),
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRIX_ACQUIRE_LOCK_REPLY) - sizeof(NRIX_HEADER)) {

			NDASSCSI_ASSERT( FALSE );
	
			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	lock = NdasRaidClientAllocLock( NTOHLL(acquireLockReply->LockId), 
									 acquireLockReply->LockType, 
									 acquireLockReply->LockMode, 
									 NTOHLL(acquireLockReply->BlockAddress),
									 NTOHL(acquireLockReply->BlockLength) );

	if (lock == NULL) {

		NDASSCSI_ASSERT( FALSE );
	
	} else {

		KIRQL	oldIrql;

		KeQueryTickCount( &lock->LastAccesseTick );	

		lock->Status = LOCK_STATUS_GRANTED;
					
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Reply - lock is granted lock id = %I64x, range= %I64x:%x\n", 
					 lock->Id, lock->BlockAddress, lock->BlockLength) );

		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql ); 
		InsertTailList( &NdasrClient->LockList, &lock->Link );
		RELEASE_SPIN_LOCK(&NdasrClient->SpinLock, oldIrql);
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientSendRequestReleaseLock (
	PNDASR_CLIENT	NdasrClient,
	UINT64			LockId,
	PLARGE_INTEGER	Timeout
	)
{
	NTSTATUS			status;

	NRIX_RELEASE_LOCK	releaseLockRequest;
	PNRIX_HEADER		releaseLockReply;
	UINT32				msgLength;
	LONG				result;
	

	NDASSCSI_ASSERT( Timeout == NULL );
	NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending RELEASE_LOCK message lock id = %I64x\n", LockId) );

	msgLength = sizeof(NRIX_RELEASE_LOCK);

	RtlZeroMemory( &releaseLockRequest, msgLength );

	releaseLockRequest.Header.Signature	= NTOHL(NRIX_SIGNATURE);
	releaseLockRequest.Header.Command	= NRIX_CMD_RELEASE_LOCK;
	releaseLockRequest.Header.Length	= NTOHS((UINT16)msgLength);
	releaseLockRequest.Header.Sequence	= NTOHL(NdasrClient->RequestSequence);
	releaseLockRequest.Header.Usn		= NTOHL( NdasrClient->Usn );

	NdasrClient->RequestSequence++;

	releaseLockRequest.LockId = HTONLL(LockId);

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		NDASR_LOCAL_MSG		ndasrRequestLocalMsg;
		
		PLIST_ENTRY			listEntry;
		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
		
		InitializeListHead( &ndasrRequestLocalMsg.ListEntry );
		ndasrRequestLocalMsg.NrixHeader = (PNRIX_HEADER)&releaseLockRequest;

		ExInterlockedInsertTailList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.RequestQueue,
									 &ndasrRequestLocalMsg.ListEntry,
									 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.RequestEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
										Executive, 
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		KeClearEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent );

		listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
												 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		NDASSCSI_ASSERT( listEntry );

		ndasrReplyLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );
		releaseLockReply = ndasrReplyLocalMsg->NrixHeader;

		NDASSCSI_ASSERT( ndasrReplyLocalMsg == &NdasrClient->Lurn->NdasrInfo->RequestChannelReply );
		NDASSCSI_ASSERT( releaseLockReply == &NdasrClient->Lurn->NdasrInfo->NrixHeader );

	} else {

		result = 0;

		status = LpxTdiV2Send( NdasrClient->RequestConnection.ConnectionFileObject, 
							   (PUCHAR)&releaseLockRequest, 
							   msgLength,
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );
	
		if (result != msgLength) {

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		result = 0;

		releaseLockReply = (PNRIX_HEADER)NdasrClient->RequestConnection.ReceiveBuf;

		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject,
							   (PUCHAR)releaseLockReply,
							   sizeof(NRIX_HEADER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRIX_HEADER)) {

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to received request header\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	if (releaseLockReply->Signature != HTONL(NRIX_SIGNATURE) ||
		releaseLockReply->Command != NRIX_CMD_RELEASE_LOCK	 ||
		!releaseLockReply->ReplyFlag) {

		NDASSCSI_ASSERT( FALSE );
	
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (releaseLockReply->Result == NRIX_RESULT_LOWER_USN) {

		NDASSCSI_ASSERT( releaseLockReply->RaidInformation == TRUE );

		status = NdasRaidClientProcessNodeChageReply( NdasrClient, (PNRIX_CHANGE_STATUS)releaseLockReply );

		if (status != STATUS_SUCCESS) {

			return status;
		}

		if (NdasrClient->WaitSyncForWrite) {

			return STATUS_SYNCHRONIZATION_REQUIRED;
		}

		NDASSCSI_ASSERT( FALSE );
	}

	if (releaseLockReply->Result != NRIX_RESULT_SUCCESS) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientSendRequestNodeChange (
	PNDASR_CLIENT	NdasrClient,
	PLARGE_INTEGER	Timeout
	)
{
	NTSTATUS			status;

	PNRIX_NODE_CHANGE	nodeChangeRequest;
	PNRIX_HEADER		nodeChangeReply;
	UINT32				msgLength;
	LONG				result;
	UCHAR				nidx;
	KIRQL				oldIrql;
	

	NDASSCSI_ASSERT( Timeout == NULL );
	NDASSCSI_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITER_CONNECTED) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending NODE_CHANGE message\n") );

	msgLength = SIZE_OF_NRIX_CHANGE_STATUS(NdasrClient->Lurn->LurnChildrenCnt);

	nodeChangeRequest = ExAllocatePoolWithTag( NonPagedPool, msgLength, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
	RtlZeroMemory( nodeChangeRequest, msgLength );

	nodeChangeRequest->Header.Signature	= NTOHL(NRIX_SIGNATURE);
	nodeChangeRequest->Header.Command	= NRIX_CMD_NODE_CHANGE;
	nodeChangeRequest->Header.Length	= NTOHS((UINT16)msgLength);
	nodeChangeRequest->Header.Sequence	= NTOHL(NdasrClient->RequestSequence);
	nodeChangeRequest->Header.Usn		= NTOHL(NdasrClient->Usn );

	NdasrClient->RequestSequence++;

	nodeChangeRequest->UpdateCount = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt;

	for (nidx=0; nidx<nodeChangeRequest->UpdateCount; nidx++) {
		
		nodeChangeRequest->Node[nidx].NodeNum = nidx;
		nodeChangeRequest->Node[nidx].NodeFlags = NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx];;
		nodeChangeRequest->Node[nidx].DefectCode = NdasrClient->NdasrDefectCodes[nidx];
	}

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		NDASR_LOCAL_MSG		ndasrRequestLocalMsg;
		
		PLIST_ENTRY			listEntry;
		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
		
		InitializeListHead( &ndasrRequestLocalMsg.ListEntry );
		ndasrRequestLocalMsg.NrixHeader = (PNRIX_HEADER)nodeChangeRequest;

		ExInterlockedInsertTailList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.RequestQueue,
									 &ndasrRequestLocalMsg.ListEntry,
									 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.RequestEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
										Executive, 
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		KeClearEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent );

		listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
												 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		NDASSCSI_ASSERT( listEntry );

		ndasrReplyLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );
		nodeChangeReply = ndasrReplyLocalMsg->NrixHeader;

		NDASSCSI_ASSERT( ndasrReplyLocalMsg == &NdasrClient->Lurn->NdasrInfo->RequestChannelReply );
		NDASSCSI_ASSERT( nodeChangeReply == &NdasrClient->Lurn->NdasrInfo->NrixHeader );

	} else {

		result = 0;

		status = LpxTdiV2Send( NdasrClient->RequestConnection.ConnectionFileObject, 
							   (PUCHAR)nodeChangeRequest, 
							   msgLength,
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );
	
		if (result != msgLength) {

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request\n") );

			ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
			return STATUS_CONNECTION_DISCONNECTED;
		}

		result = 0;

		nodeChangeReply = (PNRIX_HEADER)NdasrClient->RequestConnection.ReceiveBuf;

		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject,
							   (PUCHAR)nodeChangeReply,
							   sizeof(NRIX_HEADER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRIX_HEADER)) {

			NDASSCSI_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to received request header\n") );

			ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	if (nodeChangeReply->Signature != HTONL(NRIX_SIGNATURE)	||
		nodeChangeReply->Command != NRIX_CMD_NODE_CHANGE	||
		!nodeChangeReply->ReplyFlag) {

		NDASSCSI_ASSERT( FALSE );
	
		ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (nodeChangeReply->Result == NRIX_RESULT_LOWER_USN) {

		NDASSCSI_ASSERT( nodeChangeReply->RaidInformation == TRUE );

		status = NdasRaidClientProcessNodeChageReply( NdasrClient, (PNRIX_CHANGE_STATUS)nodeChangeReply );

		if (status != STATUS_SUCCESS) {

			return status;
		}

		if (NdasrClient->WaitSyncForWrite) {

			ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
			return STATUS_SYNCHRONIZATION_REQUIRED;
		}

		NDASSCSI_ASSERT( FALSE );
	}

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	if (nodeChangeReply->Result == NRIX_RESULT_WAIT_SYNC_FOR_WRITE) {

		NdasrClient->WaitSyncForWrite = TRUE;
		
	} else if (nodeChangeReply->Result == NRIX_RESULT_NO_CHANGE) {
		
		NdasrClient->WaitSyncForWrite = FALSE;

	} else {

		NDASSCSI_ASSERT( FALSE );
	}

	for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
			
		NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx] &= ~NDASR_NODE_CHANGE_FLAG_UPDATING;
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );				

	ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientProcessNodeChageReply ( 
	PNDASR_CLIENT		NdasrClient,
	PNRIX_CHANGE_STATUS	ChangeStatusMsg
	)
{
	NTSTATUS	status;

	ULONG		msgLength;
	LONG		result;


	msgLength = NTOHS( ChangeStatusMsg->Header.Length );
	
	if (msgLength > NRIX_MAX_REQUEST_SIZE || msgLength < sizeof(NRIX_HEADER)) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	NDASSCSI_ASSERT( msgLength != sizeof(NRIX_HEADER) );
	
	result = 0;
		
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reading additional message data %d bytes\n", msgLength - sizeof(NRIX_HEADER)) );
		
	status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject, 
						   (PUCHAR)(ChangeStatusMsg) + sizeof(NRIX_HEADER),
						   msgLength - sizeof(NRIX_HEADER),
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != msgLength - sizeof(NRIX_HEADER)) {

		NDASSCSI_ASSERT( FALSE );
	
		return STATUS_CONNECTION_DISCONNECTED;
	}

	return NdasRaidClientCheckChangeStatusMessage( NdasrClient, ChangeStatusMsg );
}

NTSTATUS
NdasRaidClientCheckChangeStatusMessage (
	PNDASR_CLIENT		NdasrClient,
	PNRIX_CHANGE_STATUS	ChangeStatusMsg
	)
{
	KIRQL	oldIrql;
	UCHAR	nidx;


	if (ChangeStatusMsg->NodeCount != NdasrClient->Lurn->LurnChildrenCnt) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (NTOHL(ChangeStatusMsg->Usn) != NdasrClient->Lurn->NdasrInfo->Rmd.uiUSN) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("USN changed. Reload RMD (not needed until online unit change is implemented)\n") );
	}

	ACQUIRE_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->SpinLock, &oldIrql );
	ACQUIRE_DPC_SPIN_LOCK( &NdasrClient->SpinLock );

	if (ChangeStatusMsg->RaidState != NdasrClient->NdasrState) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Changing RAID status from %x to %x!!!\n", 
					 NdasrClient->NdasrState ,ChangeStatusMsg->RaidState) );
	}

	if (RtlCompareMemory(&ChangeStatusMsg->ConfigSetId, 
						 &NdasrClient->Lurn->NdasrInfo->ConfigSetId , 
						 sizeof(GUID)) != sizeof(GUID)) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Config Set Id changed!!!\n") );

		RtlCopyMemory( &NdasrClient->Lurn->NdasrInfo->ConfigSetId, &ChangeStatusMsg->ConfigSetId, sizeof(GUID) );
	}

	NdasrClient->NdasrState			= ChangeStatusMsg->RaidState;
	NdasrClient->OutOfSyncRoleIndex = ChangeStatusMsg->OutOfSyncRoleIndex;
	NdasrClient->Usn				= HTONL(ChangeStatusMsg->Usn);

	for (nidx = 0; nidx < ChangeStatusMsg->NodeCount; nidx++) {
			
		if (NdasrClient->NdasrNodeFlags[nidx] != ChangeStatusMsg->Node[nidx].NodeFlags) {
					
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Changing node %d flag from %x to %x!!!\n", 
						nidx, NdasrClient->NdasrNodeFlags[nidx] ,ChangeStatusMsg->Node[nidx].NodeFlags) );
		}

		NdasrClient->NdasrNodeFlags[nidx] = ChangeStatusMsg->Node[nidx].NodeFlags;
				
		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {
			
			if (!FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Setting defective flag to node %d %d!!!\n", 
							nidx, NdasrClient->NdasrNodeFlags[nidx] ,ChangeStatusMsg->Node[nidx].NodeFlags) );
						
				SetFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE );
			}
		}

		if (NdasrClient->NodeToRoleMap[nidx] != ChangeStatusMsg->Node[nidx].NodeRole) {
					
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Changing node %d role from %x to %x!!!\n", 
						nidx, NdasrClient->NodeToRoleMap[nidx], ChangeStatusMsg->Node[nidx].NodeRole) );
		}

		NdasrClient->NodeToRoleMap[nidx] = ChangeStatusMsg->Node[nidx].NodeRole;
		NdasrClient->RoleToNodeMap[ChangeStatusMsg->Node[nidx].NodeRole] = nidx;
	}

	if (ChangeStatusMsg->WaitSyncForWrite) {
				
		NdasrClient->WaitSyncForWrite = TRUE;

	} else {
				
		NdasrClient->WaitSyncForWrite = FALSE;
	}

	RELEASE_DPC_SPIN_LOCK( &NdasrClient->SpinLock );
	RELEASE_SPIN_LOCK( &NdasrClient->Lurn->NdasrInfo->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientRecvHeaderWithoutWait (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	) 
{
	NTSTATUS	status = STATUS_SUCCESS;

	if (KeReadStateEvent(&Connection->ReceiveOverlapped.Request[0].CompletionEvent)) {

		LpxTdiV2CompleteRequest( &Connection->ReceiveOverlapped, 0 );

		if (Connection->ReceiveOverlapped.Request[0].IoStatusBlock.Status != STATUS_SUCCESS) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Receive result=%x\n",Connection->ReceiveOverlapped.Request[0].IoStatusBlock.Information) );

			status = STATUS_CONNECTION_DISCONNECTED;

		} else if (Connection->ReceiveOverlapped.Request[0].IoStatusBlock.Information != sizeof(NRIX_HEADER)) {

			NDASSCSI_ASSERT( FALSE );

			status = STATUS_CONNECTION_DISCONNECTED;
		
		} else {

			status = NdasRaidClientRecvAdditionalDataFromRemote( NdasrClient, Connection );
		}

	} else {

		status = STATUS_PENDING;
	}

	return status;
}


NTSTATUS
NdasRaidClientRecvAdditionalDataFromRemote (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	) 
{
	NTSTATUS		status;
	PNRIX_HEADER	message;
	ULONG			msgLength;

	LONG			result;
	ULONG			addtionalLength;


	UNREFERENCED_PARAMETER( NdasrClient );
	
	// Read remaining data if needed.

	message = (PNRIX_HEADER) Connection->ReceiveBuf;
	msgLength = NTOHS(message->Length);
	
	if (msgLength > NRIX_MAX_REQUEST_SIZE || msgLength < sizeof(NRIX_HEADER)) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (msgLength == sizeof(NRIX_HEADER)) {

		return STATUS_SUCCESS;
	}

	result = 0;
	addtionalLength = msgLength - sizeof(NRIX_HEADER);
		
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reading additional message data %d bytes\n", addtionalLength) );
		
	status = LpxTdiV2Recv( Connection->ConnectionFileObject, 
						   (PUCHAR)(Connection->ReceiveBuf + sizeof(NRIX_HEADER)),
						   addtionalLength,
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != addtionalLength) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	return status;
}

NTSTATUS
NdasRaidClientHandleNotificationMsg (
	PNDASR_CLIENT	NdasrClient,
	PNRIX_HEADER	Message
	) 
{
	NTSTATUS			status;
	KIRQL				oldIrql;
	UCHAR				resultCode = NRIX_RESULT_SUCCESS;
	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client handling message %s\n", NdasRixGetCmdString(Message->Command)) );
	
	// Check data validity.

	if (NTOHL(Message->Signature) != NRIX_SIGNATURE || Message->ReplyFlag != 0) {

		NDASSCSI_ASSERT( FALSE );

		return STATUS_CONNECTION_DISCONNECTED;
	}
	
	switch (Message->Command) {

	case NRIX_CMD_RETIRE: {

		// Enter transition mode and release all locks.
		
		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

		NdasrClient->WaitSyncForWrite = TRUE;
		
		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );	

		status = STATUS_SUCCESS;
		break;
	}

	case NRIX_CMD_CHANGE_STATUS: {

		PNRIX_CHANGE_STATUS changeStatusMsg = (PNRIX_CHANGE_STATUS) Message;
	
		status = NdasRaidClientCheckChangeStatusMessage( NdasrClient, changeStatusMsg );

		break;
	}
	
	case NRIX_CMD_STATUS_SYNCED: { // RAID/Node status is synced. Continue IO

		ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );
		
		NdasrClient->WaitSyncForWrite = FALSE;

		RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
	
		status = STATUS_SUCCESS;
		break;
	}

	default:

		NDASSCSI_ASSERT( FALSE );

		status = STATUS_CONNECTION_DISCONNECTED;
	}

	// must send release lock

	if (Message->Command == NRIX_CMD_RETIRE) {

		status = NdasRaidClientFlush( NdasrClient );
	}

	// must send node change message

	if (Message->Command == NRIX_CMD_CHANGE_STATUS) {

		status = NdasRaidClientSendRequestNodeChange( NdasrClient, NULL );
	}

	// not obligation, but recommended

	if (Message->Command == NRIX_CMD_STATUS_SYNCED) {

		LURN_EVENT lurnEvent;

		status = NdasRaidClientFlush( NdasrClient );

		//	Set a event to make NDASSCSI fire NoOperation to trigger ndasscsi status change alarm.

		lurnEvent.LurnId = NdasrClient->Lurn->LurnId;
		lurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;

		LurCallBack( NdasrClient->Lurn->Lur, &lurnEvent );	
	}

	return status;
}

VOID
NdasRaidClientUpdateNodeFlags (
	PNDASR_CLIENT		NdasrClient,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,		// Temp parameter.. set though lurn node info.
	UCHAR 				NewDefectCodes  // Temp parameter.. set though lurn node info.
	) 
{
	KIRQL oldIrql;
	UCHAR newLocalNodeFlags;

	// Defective flag should not be not cleared
	newLocalNodeFlags = NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx];

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	switch (ChildLurn->LurnStatus) {

	case LURN_STATUS_RUNNING:
	case LURN_STATUS_STALL:
		
		// Defective flag should not be not cleared

		ClearFlag( newLocalNodeFlags, (NRIX_NODE_FLAG_STOP | NRIX_NODE_FLAG_UNKNOWN) );
		SetFlag( newLocalNodeFlags, NRIX_NODE_FLAG_RUNNING );
		
		NDASSCSI_ASSERT( !FlagOn(newLocalNodeFlags, NRIX_NODE_FLAG_DEFECTIVE) );
		break;
	
	case LURN_STATUS_STOP_PENDING:
	case LURN_STATUS_STOP:
	case LURN_STATUS_DESTROYING:
		
		// Defective flag should not be not cleared

		ClearFlag( newLocalNodeFlags, (NRIX_NODE_FLAG_RUNNING | NRIX_NODE_FLAG_UNKNOWN) );
		SetFlag( newLocalNodeFlags, NRIX_NODE_FLAG_STOP );

		break;
	
	case LURN_STATUS_INIT:

		NDASSCSI_ASSERT( FALSE );

		// Defective flag should not be not cleared

		ClearFlag( newLocalNodeFlags, (NRIX_NODE_FLAG_RUNNING | NRIX_NODE_FLAG_STOP) );
		SetFlag( newLocalNodeFlags, NRIX_NODE_FLAG_UNKNOWN );

	default:

		NDASSCSI_ASSERT( FALSE );

		break;
	}

	// Check bad sector or disk is defected. - to do: get defective info in cleaner way..
	
	if (LurnGetCauseOfFault(ChildLurn) & (LURN_FCAUSE_BAD_SECTOR | LURN_FCAUSE_BAD_DISK)) {
		
		SetFlag( newLocalNodeFlags, NRIX_NODE_FLAG_DEFECTIVE );
		
		NewDefectCodes = (LurnGetCauseOfFault(ChildLurn) & LURN_FCAUSE_BAD_SECTOR) ? 
							NRIX_NODE_DEFECT_BAD_SECTOR : NRIX_NODE_DEFECT_BAD_DISK;
	}

	SetFlag( newLocalNodeFlags, FlagsToAdd );

	NDASSCSI_ASSERT( newLocalNodeFlags == NRIX_NODE_FLAG_RUNNING ||
					 newLocalNodeFlags == NRIX_NODE_FLAG_STOP	 ||
					 newLocalNodeFlags == (NRIX_NODE_FLAG_STOP | NRIX_NODE_FLAG_DEFECTIVE) );

	// To do: if newflags contains defective flag, convert fault info's defective info to drix format...
		
	if (NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx] != newLocalNodeFlags ||
		NdasrClient->NdasrDefectCodes[ChildLurn->LurnChildIdx] != NewDefectCodes) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Changing local node %d flag from %x to %x(Defect Code=%x)\n",
					 ChildLurn->LurnChildIdx, NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx], 
					 newLocalNodeFlags, NewDefectCodes) );
		
		// Status changed. We need to report to arbiter.
		// Set changed flags
		
		NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx] = newLocalNodeFlags;
		NdasrClient->NdasrDefectCodes[ChildLurn->LurnChildIdx] = NewDefectCodes;

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_UPDATING)) {
			
			NDASSCSI_ASSERT( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is updating\n") );
		
		} else if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is changed\n") );
		
		} else {

			SetFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		}

	} else if (NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx] != NdasrClient->NdasrNodeFlags[ChildLurn->LurnChildIdx]) {

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_UPDATING)) {

			NDASSCSI_ASSERT( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is updating\n") );
		
		} else if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is changed\n") );
		
		} else {

			SetFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		}
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );
}

NTSTATUS
NdasRaidClientCheckIoPermission (
	IN PNDASR_CLIENT	NdasrClient,
	IN UINT64			Addr,
	IN UINT32			Length,
	OUT UINT64*			UnauthAddr,
	OUT UINT32*			UnauthLength
	) 
{
	NTSTATUS			status;

	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	lock;
	UINT32				overlapStatus;
	PNDASR_CLIENT_LOCK	fullOverlap;
	ULONG				bitmapBuf[64]; // Cover up to length 1Mbytes.
	ULONG				bitmapUlongCount;
	ULONG				bitmapBitCount;
	RTL_BITMAP			bitmap;
	KIRQL				oldIrql;

	ASSERT( (1024*1024)/(512*32) == 64 );

	NDASSCSI_ASSERT( Length > 0 && Length <= 2048 );


	if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0) {

		return STATUS_SUCCESS;
	}

	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	// Pass1. Brief search: Check most common case - full match only.
	
	fullOverlap = NULL;
	
	for (listEntry = NdasrClient->LockList.Flink;
		 listEntry != &NdasrClient->LockList;
		 listEntry = listEntry->Flink) {

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );
		
		overlapStatus = NdasRaidGetOverlappedRange( Addr, 
													Length,
													lock->BlockAddress, 
													lock->BlockLength,
													NULL, 
													NULL );

		if (overlapStatus == NDASR_RANGE_SRC2_CONTAINS_SRC1) {
			
			fullOverlap = lock;
			NDASSCSI_ASSERT( fullOverlap->Status == LOCK_STATUS_GRANTED );

			break;
		}
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	if (fullOverlap) {

		InterlockedIncrement( &fullOverlap->InUseCount );
		KeQueryTickCount( &fullOverlap->LastAccesseTick );

		return STATUS_SUCCESS;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("No full match. Addr %I64x, Length %x\n", Addr, Length) );	
	
	// No full match. Need to add up partial matched.
	// Pass2. Bitmap search.

	bitmapBitCount = Length;
	
	bitmapUlongCount = ((bitmapBitCount+31)/32); //RtlInitializeBitMap accept multiple of 32bit only.
	
	RtlInitializeBitMap( &bitmap, bitmapBuf, bitmapUlongCount*32 );
	RtlClearAllBits( &bitmap );
	
	if (bitmapBitCount % 32 != 0) {

		RtlSetBits( &bitmap, bitmapBitCount, bitmapUlongCount*32 - bitmapBitCount );
	}
	
	ACQUIRE_SPIN_LOCK( &NdasrClient->SpinLock, &oldIrql );

	// To do: iterate locks and set matching bits
	// if matching lock is pended, set LockIsPended TRUE
	
	for (listEntry = NdasrClient->LockList.Flink;
		 listEntry != &NdasrClient->LockList;
		 listEntry = listEntry->Flink) {

		UINT64				overlapStartAddr;
		UINT32				overlapLength;

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );
		
		overlapStatus = NdasRaidGetOverlappedRange( Addr, 
													Length,
													lock->BlockAddress, 
													lock->BlockLength,
													&overlapStartAddr, 
													&overlapLength );

		if (overlapStatus != NDASR_RANGE_NO_OVERLAP) {
		
			// Convert overlapping address to bit.
			
			LONG startIndex;
			LONG numberToSet;
			
			startIndex  = (LONG)(overlapStartAddr-Addr);
			numberToSet = overlapLength;

			NDASSCSI_ASSERT( startIndex >= 0 );
			NDASSCSI_ASSERT( numberToSet > 0 );
			NDASSCSI_ASSERT( startIndex + numberToSet <= (LONG)Length );

			RtlSetBits( &bitmap, startIndex, numberToSet );

			NDASSCSI_ASSERT( lock->Status == LOCK_STATUS_GRANTED );
		}
	}	

	if (RtlAreBitsSet(&bitmap, 0, Length)) {
	
		// All bit is set. Lock is covered
		 
		DebugTrace( DBG_LURN_NOISE, 
					("Range %I64x:%x is covered and all range is granted\n", Addr, Length) );
			
		status = STATUS_SUCCESS;
			
		// Increase usage count.
			
		for (listEntry = NdasrClient->LockList.Flink;
			 listEntry != &NdasrClient->LockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, NDASR_CLIENT_LOCK, Link);
				
			overlapStatus = NdasRaidGetOverlappedRange( Addr, 
														 Length,
														 lock->BlockAddress, 
														 lock->BlockLength,
														 NULL, 
														 NULL );

			if (overlapStatus != NDASR_RANGE_NO_OVERLAP) {
					
				InterlockedIncrement( &lock->InUseCount );
				KeQueryTickCount( &lock->LastAccesseTick );
					
				DebugTrace( DBG_LURN_TRACE, 
							("Partial match with lock %I64x:%x\n", lock->BlockAddress, lock->BlockLength) );
			}
		}

	} else {
		
		ULONG startIndex = 0;
		ULONG numberOfClear;
		
		// Find clear bits

		numberOfClear = RtlFindFirstRunClear( &bitmap, &startIndex );

		NDASSCSI_ASSERT( numberOfClear > 0 && numberOfClear <= Length );
		
		// Currently return only one range. If multiple range is missing, multiple lock_acquire need to be sent.
		// to do: return all unlocked range.
		
		*UnauthAddr		= startIndex + Addr;
		*UnauthLength	= numberOfClear;

		NDASSCSI_ASSERT( Addr <= *UnauthAddr && (*UnauthAddr + *UnauthLength) <= (Addr + Length) );

#if DBG

		for (listEntry = NdasrClient->LockList.Flink;
			 listEntry != &NdasrClient->LockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, NDASR_CLIENT_LOCK, Link);
				
			overlapStatus = NdasRaidGetOverlappedRange( *UnauthAddr, 
														 *UnauthLength,
														 lock->BlockAddress, 
														 lock->BlockLength,
														 NULL, 
														 NULL );

			NDASSCSI_ASSERT( overlapStatus == NDASR_RANGE_NO_OVERLAP );
		}

#endif

		DebugTrace( DBG_LURN_INFO, ("Lock range %I64x:%x is not covered.\n", *UnauthAddr, *UnauthLength) );
		
		status = STATUS_UNSUCCESSFUL;
	}

	RELEASE_SPIN_LOCK( &NdasrClient->SpinLock, oldIrql );

	return status;
}
