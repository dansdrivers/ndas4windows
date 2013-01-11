#include "ndasklibproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasraidclient"


VOID
NdasRaidClientNonArbitratorModeThreadProc (
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
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb,
	IN PCCB				ChildCcb
	);

#if 0
NTSTATUS
NdasRaidClientUpdateCcbCompletion (
	IN PCCB Ccb,
	IN PCCB ChildCcb
	);
#endif

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
NdasRaidClientRefreshRaidStatusWithoutArbitrator (
	PNDASR_CLIENT NdasrClient
	); 

NTSTATUS
NdasRaidClientEstablishArbitrator (
	PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS
NdasRaidClientConnectToArbitrator (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	);

NTSTATUS
NdasRaidClientDisconnectFromArbitrator (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	); 

NTSTATUS
NdasRaidClientRegisterToRemoteArbitrator (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection,
	UCHAR						ConType	// NRMX_CONN_TYPE_*
	); 

NTSTATUS 
NdasRaidClientResetRemoteArbitratorContext (
	PNDASR_CLIENT	NdasrClient
	); 

NTSTATUS
NdasRaidClientSendRequestAcquireLock (
	PNDASR_CLIENT	NdasrClient,
	UCHAR			LockType,		// NRMX_LOCK_TYPE_BLOCK_IO
	UCHAR			LockMode,		// NRMX_LOCK_MODE_*
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
	PNRMX_CHANGE_STATUS	ChangeStatusMsg
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
	PNRMX_HEADER	Message,
	PBOOLEAN		RetireMessage
	); 

NTSTATUS
NdasRaidClientCheckChangeStatusMessage (
	PNDASR_CLIENT		NdasrClient,
	PNRMX_CHANGE_STATUS	ChangeStatusMsg
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( Lurn->NdasrInfo->NdasrClient == NULL );
	 	
	ndasrClient = ExAllocatePoolWithTag( NonPagedPool, 
										 sizeof(NDASR_CLIENT) - sizeof(UCHAR) + 
										 (Lurn->NdasrInfo->MaxDataRecvLength / 
										  (Lurn->NdasrInfo->ActiveDiskCount - Lurn->NdasrInfo->ParityDiskCount) * 
										   Lurn->NdasrInfo->ActiveDiskCount),
										 NDASR_CLIENT_POOL_TAG );
	
	if (ndasrClient == NULL) {
	
		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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

			ndasrClient->BlockBuffer[ridx] = 
				ndasrClient->Buffer + Lurn->NdasrInfo->MaxDataRecvLength / blockChildrenCount * ridx;
		}

		InitializeListHead( &ndasrClient->AllListEntry );
		ndasrClient->Lurn = Lurn;

		KeInitializeSpinLock( &ndasrClient->SpinLock );	
		ndasrClient->Status = NDASR_CLIENT_STATUS_INITIALIZING;

		InitializeListHead( &ndasrClient->CcbQueue );

		InitializeListHead( &ndasrClient->LockList );

		ndasrClient->NdasrState			= NRMX_RAID_STATE_INITIALIZING;
		ndasrClient->WaitForSync		= TRUE; 
		ndasrClient->OutOfSyncRoleIndex	= NO_OUT_OF_SYNC_ROLE;
	
		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {
		
			ndasrClient->NdasrNodeFlags[nidx]	= NRMX_NODE_FLAG_UNKNOWN;
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

		NdasRaidClientRefreshRaidStatusWithoutArbitrator( ndasrClient );

		if (ndasrClient->Lurn->Lur->EmergencyMode == TRUE) {

			if (ndasrClient->NdasrState != NRMX_RAID_STATE_FAILED) {
				
				NDAS_ASSERT(FALSE);
				status = STATUS_INVALID_PARAMETER;
				break;
			}
		}

		if (ndasrClient->NdasrState == NRMX_RAID_STATE_FAILED) {

			if (ndasrClient->Lurn->Lur->EmergencyMode == FALSE) {

				NDAS_ASSERT(FALSE);
			
			} else {
				
				ndasrClient->EmergencyMode = TRUE;
				NdasRaidClientRefreshRaidStatusWithoutArbitrator( ndasrClient );		
			}
		}

		if (ndasrClient->NdasrState == NRMX_RAID_STATE_FAILED) {

			NDAS_ASSERT(FALSE);
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		if (LUR_IS_READONLY(ndasrClient->Lurn->Lur)				|| 
			ndasrClient->Lurn->NdasrInfo->ParityDiskCount == 0	||
			ndasrClient->EmergencyMode) {

			status = PsCreateSystemThread( &ndasrClient->ThreadHandle,
										   THREAD_ALL_ACCESS,
										   &objectAttributes,
										   NULL,
										   NULL,
										   NdasRaidClientNonArbitratorModeThreadProc,
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

			NDAS_ASSERT(FALSE);		

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

			NDAS_ASSERT(FALSE);		

			ndasrClient->ClientState0 = NDASR_CLIENT_STATUS_TERMINATING;
			break;
		}

		status = KeWaitForSingleObject( &ndasrClient->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {
	
			NDAS_ASSERT(FALSE);

			ndasrClient->ClientState0 = NDASR_CLIENT_STATUS_TERMINATING;
			break;
		}

		NdasRaidRegisterClient( &NdasrGlobalData, ndasrClient );
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT(FALSE);

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


	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( Lurn->NdasrInfo->NdasrClient != NULL );
	
	KeAcquireSpinLock( &ndasrInfo->SpinLock, &oldIrql );

	ndasrInfo->NdasrClient = NULL;

	KeReleaseSpinLock( &ndasrInfo->SpinLock, oldIrql );	

	NdasRaidUnregisterClient( &NdasrGlobalData, ndasrClient );

	KeAcquireSpinLock( &ndasrClient->SpinLock, &oldIrql );

	ndasrClient->RequestToTerminate = TRUE;
	
	if (ndasrClient->ThreadHandle) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("KeSetEvent\n") );

		KeSetEvent( &ndasrClient->ThreadEvent,IO_NO_INCREMENT, FALSE );
	}
	
	KeReleaseSpinLock( &ndasrClient->SpinLock, oldIrql );

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

	KeAcquireSpinLock( &ndasrClient->SpinLock, &oldIrql );

	ndasrClient->ThreadObject = NULL;
	ndasrClient->ThreadHandle = NULL;
	
	while (!IsListEmpty(&ndasrClient->LockList)) {

		listEntry = RemoveHeadList( &ndasrClient->LockList );
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Freeing lock\n") );

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );

		NDAS_ASSERT( InterlockedCompareExchange(&lock->InUseCount, 0, 0) == 0 );

		ExFreePoolWithTag( lock, NDASR_CLIENT_LOCK_POOL_TAG );
	}
	
	KeReleaseSpinLock( &ndasrClient->SpinLock, oldIrql );
	
	if (ndasrClient) {

		ExFreePoolWithTag( ndasrClient, NDASR_CLIENT_POOL_TAG );
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client stopped completely\n") );

	return status;
}

#if 0

NTSTATUS
NdasRaidClientShutdown (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KIRQL				oldIrql;
	
	PNDASR_CLIENT		ndasrClient = Lurn->NdasrInfo->NdasrClient;

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	NDAS_ASSERT( ndasrClient );
	NDAS_ASSERT( ndasrClient->ThreadHandle );

	KeAcquireSpinLock( &ndasrClient->SpinLock, &oldIrql );

	ndasrClient->RequestToShutdown = TRUE;
	KeSetEvent( &ndasrClient->ThreadEvent,IO_NO_INCREMENT, FALSE );

	KeReleaseSpinLock( &ndasrClient->SpinLock, oldIrql );

	status = KeWaitForSingleObject( &ndasrClient->FinishShutdownEvent,
									Executive,
									KernelMode,
									FALSE,
									NULL );

	if (status != STATUS_SUCCESS) {
	
		NDAS_ASSERT(FALSE);
		return status;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client shutdown completely\n") );

	return status;
}

#endif

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

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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
NdasRaidClientNonArbitratorModeThreadProc (
	IN PNDASR_CLIENT	NdasrClient
	)
{
	NTSTATUS			status = STATUS_SUCCESS;

	BOOLEAN				terminateThread = FALSE;

	UCHAR				nidx;

	KIRQL				oldIrql;
	LARGE_INTEGER		timeOut;

	UCHAR				ridx;

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Starting client\n") );

	NDAS_ASSERT( LUR_IS_READONLY(NdasrClient->Lurn->Lur)			||
				 NdasrClient->EmergencyMode							||
				 NdasrClient->Lurn->LurnType == LURN_NDAS_RAID0		||
				 NdasrClient->Lurn->LurnType == LURN_NDAS_AGGREGATION );
			
	NdasRaidClientRefreshRaidStatusWithoutArbitrator( NdasrClient );

	NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_NORMAL	||
				 LUR_IS_READONLY(NdasrClient->Lurn->Lur)			||
				 NdasrClient->EmergencyMode && NdasrClient->NdasrState != NRMX_RAID_STATE_FAILED );

	if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0) {

		for (nidx = 0; nidx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; nidx++) {

			if (NdasrClient->NodeToRoleMap[nidx] != nidx) {
		
				NDAS_ASSERT(FALSE);

				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );

				NdasrClient->ThreadErrorStatus = STATUS_INVALID_PARAMETER;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Error Exiting\n") );

				KeSetEvent( &NdasrClient->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

				PsTerminateSystemThread( STATUS_SUCCESS );

				return;
			}
		}
	}

	NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_NORMAL	||
				 LUR_IS_READONLY(NdasrClient->Lurn->Lur)			||
				 NdasrClient->EmergencyMode && NdasrClient->NdasrState != NRMX_RAID_STATE_FAILED );

	NdasrClient->WaitForSync = FALSE; 

	ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );
	NdasrClient->ClientState0 = NDASR_CLIENT_STATUS_START;
	
	KeSetEvent( &NdasrClient->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	do {
		
		PKEVENT		events[3];
		LONG		eventCount;

		// 9. Check local node changes 

		for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			
			if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
				ClearFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		
				NdasrClient->ClusterState20 = NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED >> 20;

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Node %d has changed\n", nidx) );
			}
				
			KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
		}

		if (status == STATUS_CLUSTER_NODE_UNREACHABLE) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );
		}

		// 10. Send node change message

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED)) {

			NDAS_ASSERT( NdasrClient->ArbitratorMode12 == 0 );
			NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

			NdasRaidClientRefreshRaidStatusWithoutArbitrator( NdasrClient );

			if (NdasrClient->NdasrState != NRMX_RAID_STATE_NORMAL) {

				NdasrClient->NdasrState = NRMX_RAID_STATE_FAILED;
			}
		}

		// 11. Wait for event 

		eventCount = 0;
		events[eventCount] = &NdasrClient->ThreadEvent;
		eventCount++;

		if (NdasrClient->Lurn->LockingTime.QuadPart) {

			timeOut.QuadPart = -1 * NANO100_PER_MSEC;

		} else {

			timeOut.QuadPart = -30 * NANO100_PER_SEC; // need to wake-up with timeout to free unused write-locks.
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

		if (NdasrClient->Lurn->LockingTime.QuadPart) {

			NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );
			NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );

			if (IsListEmpty(&NdasrClient->CcbQueue)) {
				
				NTSTATUS	lockStatus;

				KeEnterCriticalRegion();
				ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

				for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx ++) { 

					if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

						continue;
					}

					if (NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] == TRUE) {

						lockStatus = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
																  LURNLOCK_OPCODE_RELEASE,
																  NDAS_RW_LOCK,
																  TRUE );
		
						NdasrClient->Lurn->LockingTime.QuadPart = 0;

						if (lockStatus != STATUS_SUCCESS) {
							
							NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] = FALSE;
						}
					}

					break;
				}
		
				ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
				KeLeaveCriticalRegion();
			}
		}


		KeClearEvent( &NdasrClient->ThreadEvent );

		// 3. Handle Ccb

		NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
					 !FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

		NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );
		//NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );

		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

		status = NdasRaidClientHandleCcb( NdasrClient );

		if (NdasrClient->Lurn->LockingTime.QuadPart) {
			
			if (!NT_SUCCESS(status)	||
				(NdasCurrentTime().QuadPart - NdasrClient->Lurn->LockingTime.QuadPart) >= NDAS_MAX_LOCKING_DURATION) {
				
				NTSTATUS	lockStatus;

				for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx ++) { 

					if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

						continue;
					}

					if (NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] == TRUE) {

						lockStatus = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
																  LURNLOCK_OPCODE_RELEASE,
																  NDAS_RW_LOCK,
																  TRUE );
		
						NdasrClient->Lurn->LockingTime.QuadPart = 0;

						if (lockStatus != STATUS_SUCCESS) {
								
							NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] = FALSE;
						}
					}

					break;
				}
			}
		}

		ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
		KeLeaveCriticalRegion();

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	

		if (status != STATUS_SUCCESS) {	

			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );			
			
			KeAcquireSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, &oldIrql );

			for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

				NdasRaidClientUpdateLocalNodeFlags( NdasrClient, NdasrClient->Lurn->LurnChildren[nidx], 0, 0 );
			}

			KeReleaseSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, oldIrql );

			continue;
		}
	
		// 5. Check termination request.

		NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
					 !FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

		NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

		if (NdasrClient->RequestToTerminate) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RequestToTerminate is set\n") );
			NDAS_ASSERT( IsListEmpty(&NdasrClient->LockList) );

			terminateThread = TRUE;
			continue;
		}

		// 7. complete shutdown ccb

		if (NdasrClient->ShutdownCcb) {

			LsCcbCompleteCcb( NdasrClient->ShutdownCcb );
			NdasrClient->ShutdownCcb = NULL;
		}

	} while (terminateThread == FALSE);

	NDAS_ASSERT( NdasrClient->MonitorThreadHandle == NULL );

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_START );
	NdasrClient->ClientState0 = NRMX_RAID_STATE_TERMINATED;
	
	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );	

	// Complete pending 
	
	if (!IsListEmpty(&NdasrClient->CcbQueue)) {
	
		PLIST_ENTRY ccbListEntry;
		PCCB		Ccb;
		
		while (ccbListEntry = ExInterlockedRemoveHeadList(&NdasrClient->CcbQueue,
														  &NdasrClient->SpinLock)) {
			
			Ccb = CONTAINING_RECORD(ccbListEntry, CCB, ListEntry);
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client thread is terminating. Returning CCB with STOP status.\n") );
	
			NDAS_ASSERT(FALSE);

			LsCcbSetStatus( Ccb, CCB_STATUS_STOP );
			LsCcbCompleteCcb( Ccb );
		}
	}
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );
	
	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

NTSTATUS
NdasRaidClientReleaseRwLock (
	IN PNDASR_CLIENT	NdasrClient
	)
{
	NTSTATUS	lockStatus = STATUS_SUCCESS;
	UCHAR		ridx;


	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

	for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx ++) { 

		if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

			continue;
		}

		if (NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] == TRUE) {

			lockStatus = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
													  LURNLOCK_OPCODE_RELEASE,
													  NDAS_RW_LOCK,
													  TRUE );
		
			NdasrClient->Lurn->LockingTime.QuadPart = 0;

			if (lockStatus != STATUS_SUCCESS) {
							
				NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] = FALSE;
			}
		}

		break;
	}
		
	ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
	KeLeaveCriticalRegion();

	return lockStatus;
}

VOID
NdasRaidClientThreadProc (
	IN PNDASR_CLIENT	NdasrClient
	)
{
	NTSTATUS			status = STATUS_SUCCESS;

	BOOLEAN				terminateThread = FALSE;

	UCHAR				nidx;

	KIRQL				oldIrql;
	LARGE_INTEGER		timeOut;

	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	lock;

	LARGE_INTEGER		currentTick;


	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Starting client\n") );

	NDAS_ASSERT( !LUR_IS_READONLY(NdasrClient->Lurn->Lur) && 
				 (NdasrClient->Lurn->LurnType == LURN_NDAS_RAID1 ||
				  NdasrClient->Lurn->LurnType == LURN_NDAS_RAID4 ||
				  NdasrClient->Lurn->LurnType == LURN_NDAS_RAID5) );
			
	NdasRaidClientRefreshRaidStatusWithoutArbitrator( NdasrClient );

	NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_NORMAL		||
				 NdasrClient->NdasrState == NRMX_RAID_STATE_DEGRADED	||
				 NdasrClient->NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC );

	for (;;) {

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {
		
			PLIST_ENTRY			listEntry;
			PNDASR_LOCAL_MSG	ndasrLocalMsg;

			NdasrClient->Lurn->NdasrInfo->LocalClientRegisterUsn = NdasrClient->Usn;

			status = NdasRaidArbitratorStart( NdasrClient->Lurn );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);
				break;
			}
	
			status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
											Executive, 
											KernelMode,
											FALSE,
											NULL );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);
				break;
			}

			NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED >> 16;
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED );

			listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
													 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

			ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

			status = NdasRaidClientHandleNotificationMsg( NdasrClient, ndasrLocalMsg->NrmxHeader, NULL );
	
			ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);
				break;
			}

			NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_ARBITRATOR_MODE >> 12;
			break;
		}
	
		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {

			NDAS_ASSERT(FALSE);
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				    ("Calling NdasRaidClientEstablishArbitrator LUR_IS_PRIMARY(Lurn->Lur) = %d\n", 
					 LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) );

		status = NdasRaidLurnReadRmd( NdasrClient->Lurn, 
									  &NdasrClient->Lurn->NdasrInfo->Rmd, 
									  NdasrClient->Lurn->NdasrInfo->NodeIsUptoDate, 
									  &NdasrClient->Lurn->NdasrInfo->UpToDateNode );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( NDAS_ASSERT_NETWORK_FAIL );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client failed to read RMD. Exiting client thread\n") );
			break;
		}
		
		ClearFlag( NdasrClient->Status, (NDASR_CLIENT_STATUS_ARBITRATOR_MODE) );
		NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE >> 12;

		status = NdasRaidClientEstablishArbitrator( NdasrClient );	

		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE );

		if (status == STATUS_SUCCESS) {

			NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_ARBITRATOR_MODE >> 12;
			break;
		}

		NdasrClient->ConnectionState16 = 0;
		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE );
		NdasrClient->WaitForSync = TRUE;

		NDAS_ASSERT( NdasrClient->ArbitratorMode12 == 0 );		
		NDAS_ASSERT( NdasrClient->WaitForSync == TRUE );

		for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) { 

			UCHAR	nidx2;
			UCHAR	ccbStatus;

			nidx2 = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt - nidx - 1;

			if (!FlagOn(NdasrClient->NdasrNodeFlags[nidx2], NRMX_NODE_FLAG_RUNNING)) {

				continue;
			}

			status = NdasRaidLurnUpdateSynchrously( NdasrClient->Lurn->LurnChildren[nidx2],
													 LURN_UPDATECLASS_WRITEACCESS_USERID,
												   	 &ccbStatus );
				
			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				break;
			}

			if (ccbStatus == CCB_STATUS_NO_ACCESS) {

				if (LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[NdasrClient->Lurn->LurnChildrenCnt - 1]->LurnStatus)) {

					NDAS_ASSERT( nidx2 == NdasrClient->Lurn->LurnChildrenCnt - 1 );

				} else {

					NDAS_ASSERT( nidx2 == NdasrClient->Lurn->LurnChildrenCnt - 2 );
				}

				break;
			}

			NDAS_ASSERT( FlagOn(NdasrClient->Lurn->LurnChildren[nidx2]->AccessRight, GENERIC_WRITE) );
		}

		if (nidx != NdasrClient->Lurn->LurnChildrenCnt) {

			LARGE_INTEGER	interval;

			interval.QuadPart = (-5 * NANO100_PER_SEC);
			KeDelayExecutionThread( KernelMode, FALSE, &interval );

			continue;
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Chage from secondary to primary\n") );

		ClearFlag( NdasrClient->Lurn->Lur->EnabledNdasFeatures, NDASFEATURE_SECONDARY );
		SetFlag( NdasrClient->Lurn->AccessRight, GENERIC_WRITE );

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {
		
			PLIST_ENTRY			listEntry;
			PNDASR_LOCAL_MSG	ndasrLocalMsg;
			LARGE_INTEGER		timeOut;

			NdasrClient->Lurn->NdasrInfo->LocalClientRegisterUsn = NdasrClient->Usn;

			status = NdasRaidArbitratorStart( NdasrClient->Lurn );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);
				break;
			}
	
			timeOut.QuadPart = -10 * NANO100_PER_SEC;

			status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
											Executive, 
											KernelMode,
											FALSE,
											&timeOut );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);
				break;
			}

			NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED >> 16;
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED );

			listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
													 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

			ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

			status = NdasRaidClientHandleNotificationMsg( NdasrClient, ndasrLocalMsg->NrmxHeader, NULL );
	
			ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);
				break;
			}

			NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_ARBITRATOR_MODE >> 12;
			break;
		}
	
		NDAS_ASSERT(FALSE);
	}

	if (status == STATUS_SUCCESS) {

		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );
		NdasrClient->ClientState0 = NDASR_CLIENT_STATUS_START;
	
	} else {

		NDAS_ASSERT(FALSE);

		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_INITIALIZING );

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


		// 9. Check local node changes and send state update request.

		for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			
			if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
				ClearFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		
				NdasrClient->ClusterState20 = NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED >> 20;

				// in or out

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE || status == STATUS_SUCCESS );;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Node %d has changed\n", nidx) );
			}
				
			KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
		}

		if (status == STATUS_CLUSTER_NODE_UNREACHABLE) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );
		}

		// 10. Send node change message

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED)) {

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE)
				&& FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED)) {

				if (NdasrClient->WaitForSync == FALSE) {

					if (NdasrClient->Lurn->LockingTime.QuadPart) {

						NdasRaidClientReleaseRwLock( NdasrClient );
					}

					status = NdasRaidClientSendRequestNodeChange( NdasrClient, NULL );

					if (status == STATUS_SUCCESS) {

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED );					
						KeSetEvent( &NdasrClient->ThreadEvent, 0, FALSE );
					}

					if (status != STATUS_SUCCESS) {

						NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_SYNCHRONIZATION_REQUIRED );

						if (status == STATUS_CONNECTION_DISCONNECTED) {

							NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;
							ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );

							continue;
						}
					}
				}
			
			} else {

				if (NdasrClient->Lurn->LockingTime.QuadPart) {

					NdasRaidClientReleaseRwLock( NdasrClient );
				}

				NdasRaidClientRefreshRaidStatusWithoutArbitrator( NdasrClient );
			
				if (NdasrClient->NdasrState == NRMX_RAID_STATE_FAILED) {
					
					terminateThread = TRUE;
					continue;
				}
			}
		}

		// 11. Wait for event 

		eventCount = 0;
		events[eventCount] = &NdasrClient->ThreadEvent;
		eventCount++;

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

			KeAcquireSpinLock( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock, &oldIrql );

			if (IsListEmpty(&NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue)) {

				KeClearEvent( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent );
			}

			KeReleaseSpinLock( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock, oldIrql );
		
			events[eventCount] = &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent;
			eventCount++;
		
		} else if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED)) {

			if (!LpxTdiV2IsRequestPending(&NdasrClient->NotificationConnection.ReceiveOverlapped, 0)) {

				status = LpxTdiV2Recv( NdasrClient->NotificationConnection.ConnectionFileObject,
									   NdasrClient->NotificationConnection.ReceiveBuf,
									   sizeof(NRMX_HEADER),
									   0,
									   NULL,
									   &NdasrClient->NotificationConnection.ReceiveOverlapped,
									   0,
									   NULL );

				if (!NT_SUCCESS(status)) {

					LpxTdiV2CompleteRequest( &NdasrClient->NotificationConnection.ReceiveOverlapped, 0 );
					
				} else {

					NDAS_ASSERT( status == STATUS_SUCCESS || status == STATUS_PENDING );
				}
			}

			if (!NT_SUCCESS(status)) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to recv notification message\n") );

				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;
				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );

				continue;
			}

			events[eventCount] = &NdasrClient->NotificationConnection.ReceiveOverlapped.Request[0].CompletionEvent;
			eventCount++;
		}

		if (NdasrClient->Lurn->LockingTime.QuadPart) {

			timeOut.QuadPart = -1 * NANO100_PER_MSEC;
		
		} else {

			timeOut.QuadPart = -30 * NANO100_PER_SEC; // need to wake-up with timeout to free unused write-locks.
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

		if (NdasrClient->Lurn->LockingTime.QuadPart) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

			NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );
			NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );

			if (IsListEmpty(&NdasrClient->CcbQueue)) {
				
				NTSTATUS	lockStatus;

				lockStatus = NdasRaidClientReleaseRwLock( NdasrClient );
			}
		}

		KeClearEvent( &NdasrClient->ThreadEvent );

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			KeQueryTickCount( &currentTick );

			if (((currentTick.QuadPart - NdasrClient->TemporalPrimarySince.QuadPart) * KeQueryTimeIncrement()) >= (10 * 60 * NANO100_PER_SEC) ) {

				NDAS_ASSERT(FALSE);
			}
		}

		// 1. connect to arbiter

		for (;;) {

			LARGE_INTEGER	interval;

			NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE) );

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED)) {

				NDAS_ASSERT( NdasrClient->ArbitratorMode12 == NDASR_CLIENT_STATUS_ARBITRATOR_MODE >> 12 );
				break;
			}

			if (NdasrClient->Lurn->LockingTime.QuadPart) {

				NdasRaidClientReleaseRwLock( NdasrClient );
			}

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_RETIRE_MODE)) {

				if (IsListEmpty(&NdasrClient->CcbQueue)) {

					break;
				}
			
			} else if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED)) {

				NdasRaidClientResetRemoteArbitratorContext( NdasrClient );
			}

			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {
	
				PLIST_ENTRY			listEntry;
				PNDASR_LOCAL_MSG	ndasrLocalMsg;
				LARGE_INTEGER		timeOut;

				NdasrClient->Lurn->NdasrInfo->LocalClientRegisterUsn = NdasrClient->Usn;

				status = NdasRaidArbitratorStart( NdasrClient->Lurn );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT(FALSE);
					break;
				}
	
				timeOut.QuadPart = -10 * NANO100_PER_SEC;

				status = KeWaitForSingleObject( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
												Executive, 
												KernelMode,
												FALSE,
												&timeOut );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT(FALSE);
					break;
				}

				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED >> 16;
				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED );

				listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
														 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

				ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

				status = NdasRaidClientHandleNotificationMsg( NdasrClient, ndasrLocalMsg->NrmxHeader, NULL );
			
				ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT(FALSE);
					break;
				}

				NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_ARBITRATOR_MODE >> 12;
				break;

			} 
	
			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

				NDAS_ASSERT(FALSE);
			}

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					    ("Calling NdasRaidClientEstablishArbitrator LUR_IS_PRIMARY(Lurn->Lur) = %d\n", 
						 LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) );

			status = NdasRaidLurnReadRmd( NdasrClient->Lurn, 
										  &NdasrClient->Lurn->NdasrInfo->Rmd, 
										  NdasrClient->Lurn->NdasrInfo->NodeIsUptoDate, 
										  &NdasrClient->Lurn->NdasrInfo->UpToDateNode );

			if (status != STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client failed to read RMD. Exiting client thread\n") );
				NDAS_ASSERT( NDAS_ASSERT_NETWORK_FAIL );
				NdasrClient->ErrorStatus28 = NDASR_CLIENT_STATUS_ERROR >> 28;
				break;
			}
			
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE );
			NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE >> 12;

			status = NdasRaidClientEstablishArbitrator( NdasrClient );	

			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE );

			if (status == STATUS_SUCCESS) {

				NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_ARBITRATOR_MODE >> 12;
				break;			
			}

			NdasrClient->ConnectionState16 = 0;
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE );

			interval.QuadPart = (-5 * NANO100_PER_SEC );
			KeDelayExecutionThread( KernelMode, FALSE, &interval );

			NDAS_ASSERT( NdasrClient->ArbitratorMode12 == 0 );		

			for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) { 

				UCHAR	nidx2;
				UCHAR	ccbStatus;

				nidx2 = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt - nidx - 1;

				if (!FlagOn(NdasrClient->NdasrNodeFlags[nidx2], NRMX_NODE_FLAG_RUNNING)) {

					continue;
				}

				status = NdasRaidLurnUpdateSynchrously( NdasrClient->Lurn->LurnChildren[nidx2],
														LURN_UPDATECLASS_WRITEACCESS_USERID,
													   	&ccbStatus );
				
				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
					break;
				}

				if (ccbStatus == CCB_STATUS_NO_ACCESS) {

					if (LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[NdasrClient->Lurn->LurnChildrenCnt - 1]->LurnStatus)) {

						NDAS_ASSERT( nidx2 == NdasrClient->Lurn->LurnChildrenCnt - 1 );
					
					} else {

						NDAS_ASSERT( nidx2 == NdasrClient->Lurn->LurnChildrenCnt - 2 );
					}

					break;
				}

				NDAS_ASSERT( FlagOn(NdasrClient->Lurn->LurnChildren[nidx2]->AccessRight, GENERIC_WRITE) );
			}

			if (nidx == NdasrClient->Lurn->LurnChildrenCnt) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
							("Set Flag NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE\n") );

				KeQueryTickCount( &NdasrClient->TemporalPrimarySince );
				NdasrClient->TemporalPrimay24 = NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE >> 24;
			
			} else {
				
				if (NdasrClient->NdasrState != NRMX_RAID_STATE_NORMAL		&&
					NdasrClient->NdasrState != NRMX_RAID_STATE_DEGRADED		&&
					NdasrClient->NdasrState != NRMX_RAID_STATE_OUT_OF_SYNC) {

					NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_FAILED );

					NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE >> 12;
					NdasrClient->WaitForSync = FALSE;

					break;
				}
			} 

			continue;
		}

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE)) {
			
			NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_FAILED );

			terminateThread = TRUE;
			continue;
		}

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_RETIRE_MODE)) {

			continue;
		}

		// 2. Handle any pending notifications

		NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE)		&& 
					 FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED)			||
					 FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE)	&&
					 NdasrClient->NdasrState == NRMX_RAID_STATE_FAILED );

		if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

			PLIST_ENTRY			listEntry;
			PNDASR_LOCAL_MSG	ndasrLocalMsg;

			listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
													 &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock );


			if (listEntry) {

				if (NdasrClient->Lurn->LockingTime.QuadPart) {

					NdasRaidClientReleaseRwLock( NdasrClient );
				}

				ndasrLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

				status = NdasRaidClientHandleNotificationMsg( NdasrClient, ndasrLocalMsg->NrmxHeader, NULL );

				ExFreePoolWithTag( ndasrLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );
			
			} else {

				status = STATUS_PENDING;
			}

		} else if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE)) {

			NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_FAILED );

			status = STATUS_SUCCESS;

		} else {

			status = NdasRaidClientRecvHeaderWithoutWait( NdasrClient, &NdasrClient->NotificationConnection );
		
			if (status == STATUS_PENDING) {

				// Packet is not arrived yet.
			
			} else {

				if (status == STATUS_SUCCESS) {
	
					BOOLEAN		retireMessage;

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Received through notification connection.\n") );

					if (NdasrClient->Lurn->LockingTime.QuadPart) {

						NdasRaidClientReleaseRwLock( NdasrClient );
					}

					status = NdasRaidClientHandleNotificationMsg( NdasrClient, 
																  (PNRMX_HEADER)NdasrClient->NotificationConnection.ReceiveBuf,
																  &retireMessage );	

					if (retireMessage) {

						DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("receive retire message\n") );

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
						NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE );
						NdasrClient->ArbitratorMode12 = NDASR_CLIENT_STATUS_ARBITRATOR_RETIRE_MODE >> 12;

						NdasRaidClientResetRemoteArbitratorContext( NdasrClient );

						continue;
					}
				} 
			}
		}

		if (status == STATUS_SYNCHRONIZATION_REQUIRED) {

			continue;
		}

		if (status != STATUS_PENDING && status != STATUS_SUCCESS) {

			NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Receive through notification connection failed. Reset current context and enter initialization step.\n"));
				
			ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
			NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;

			continue;
		}
	
		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED)) {

			continue;
		}

		if (NdasrClient->WaitForSync == TRUE) {

			continue;
		}

		// 3. Handle Ccb

		NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
					 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

		NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );
		NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED) );

		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

		status = NdasRaidClientHandleCcb( NdasrClient );

		if (NdasrClient->Lurn->LockingTime.QuadPart) {
			
			if (/*!LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)		|| */
				!NT_SUCCESS(status)								||
				(NdasCurrentTime().QuadPart - NdasrClient->Lurn->LockingTime.QuadPart) >= NDAS_MAX_LOCKING_DURATION) {
				
				NTSTATUS	lockStatus;

				lockStatus = NdasRaidClientReleaseRwLock( NdasrClient );
			}
		}

		ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
		KeLeaveCriticalRegion();

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	
			
		if (!NT_SUCCESS(status)) {

			if (status == STATUS_CONNECTION_DISCONNECTED) {

				ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
				NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;
			
			} else if (status == STATUS_SYNCHRONIZATION_REQUIRED) {

				NDAS_ASSERT( NdasrClient->WaitForSync == TRUE );
			
			} else {

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );			
			
				KeAcquireSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, &oldIrql );

				for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

					NdasRaidClientUpdateLocalNodeFlags( NdasrClient, NdasrClient->Lurn->LurnChildren[nidx], 0, 0 );
				}

				KeReleaseSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, oldIrql );
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

		if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE)) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
						 FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

			NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			
			KeQueryTickCount( &currentTick );
			
			status = STATUS_SUCCESS;

			listEntry = NdasrClient->LockList.Flink;
			
			while (listEntry != &NdasrClient->LockList) {
				
				lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );
				listEntry = listEntry->Flink;			
				
				if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
				
					NDAS_ASSERT(FALSE);
					continue;
				}

				if (lock->Status != LOCK_STATUS_GRANTED) {

					NDAS_ASSERT( LOCK_STATUS_NONE );
					continue;
				}

				if (((currentTick.QuadPart - lock->LastAccesseTick.QuadPart) * KeQueryTimeIncrement()) >= 
					 (NANO100_PER_SEC * NRMX_IO_LOCK_CLEANUP_TIMEOUT)) { // 60 NANO100_PER_SEC
								
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Releasing locked region(Id=%I64x, Range=%I64x:%x)\n",
								 lock->Id, lock->BlockAddress, lock->BlockLength) );
										
					KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
					
					if (NdasrClient->Lurn->LockingTime.QuadPart) {

						NdasRaidClientReleaseRwLock( NdasrClient );
					}

					status = NdasRaidClientSendRequestReleaseLock( NdasrClient, lock->Id, NULL );

					KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

					if (status != STATUS_SUCCESS) {

						break;
					}

					// Remove from acquired lock list and send message				

					RemoveEntryList( &lock->Link );
					NdasRaidClientFreeLock( lock );					
				}
			}

			KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

			if (status != STATUS_SUCCESS) {	

				if (status == STATUS_CONNECTION_DISCONNECTED) {

					ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
					NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;

				} else {

					NDAS_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitForSync == TRUE );
				}

				continue;
			}
		}
	
		// 5. Check termination request.

		NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
					FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

		NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

		if (NdasrClient->RequestToTerminate) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RequestToTerminate is set2\n") );

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE)) {

				NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );		

				if (NdasrClient->Lurn->LockingTime.QuadPart) {

					NdasRaidClientReleaseRwLock( NdasrClient );
				}

				status = NdasRaidClientFlush( NdasrClient );

				if (status != STATUS_SUCCESS) {	

					if (status == STATUS_CONNECTION_DISCONNECTED) {

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
						NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;

					} else {

						NDAS_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitForSync == TRUE );
					}

					continue;
				}
			}

			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

				KeAcquireSpinLock( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock, &oldIrql );

				if (IsListEmpty(&NdasrClient->Lurn->NdasrInfo->NotitifyChannel.RequestQueue)) {

					NdasrClient->Lurn->NdasrInfo->NotitifyChannel.Terminated = TRUE;
				} 

				KeReleaseSpinLock( &NdasrClient->Lurn->NdasrInfo->NotitifyChannel.SpinLock, oldIrql );

				if (NdasrClient->Lurn->NdasrInfo->NotitifyChannel.Terminated == FALSE) {

					continue;
				}
			}

			NDAS_ASSERT( IsListEmpty(&NdasrClient->LockList) );

			terminateThread = TRUE;
			continue;
		}

		// 6. check shutdown request	

		NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
					 FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

		NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

		if (NdasrClient->ShutdownCcb) {

			NdasrClient->Shutdown8 = NDASR_CLIENT_STATUS_SHUTDOWN >> 8;
		}

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN)) {

			NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

			if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE)) {

				if (NdasrClient->Lurn->LockingTime.QuadPart) {

					NdasRaidClientReleaseRwLock( NdasrClient );
				}

				status = NdasRaidClientFlush( NdasrClient );

				if (status != STATUS_SUCCESS) {	

					if (status == STATUS_CONNECTION_DISCONNECTED) {

						ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
						NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;

					} else {

						NDAS_ASSERT( status == STATUS_SYNCHRONIZATION_REQUIRED && NdasrClient->WaitForSync == TRUE );
					}

					continue;
				}
			}

			NDAS_ASSERT( IsListEmpty(&NdasrClient->LockList) );
		}

		// 7. call arbitrator shutdown routine and complete shutdown ccb

		if (NdasrClient->ShutdownCcb) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN) );

			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

				status = NdasRaidArbitratorShutdown( NdasrClient->Lurn );

				if (status == STATUS_TIMEOUT) {

					continue;
				}
			}

			LsCcbCompleteCcb( NdasrClient->ShutdownCcb );

			NdasrClient->ShutdownCcb = NULL;
		}

		if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) ||
			FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN)) {

			continue;
		}

		// 8. Monitoring

		if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE) &&
			!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE)) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) &&
						!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_SHUTDOWN) );

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) && 
						FlagOn(NdasrClient->Status,NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

			NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

			if (NdasrClient->NdasrState == NRMX_RAID_STATE_DEGRADED) {

				NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

				status = NdasRaidClientMonitoringChildrenLurn( NdasrClient );
				NDAS_ASSERT( status == STATUS_SUCCESS );
#if 0
				for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
					KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			
					if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {
				
						status = STATUS_CLUSTER_NODE_UNREACHABLE;
					}
				
					KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
				}
#endif
			}
		}

	} while (terminateThread == FALSE);

	for (;;) {
		
		LARGE_INTEGER	interval;

		if (NdasrClient->MonitorThreadHandle == NULL) {
	
			break;
		}

		interval.QuadPart = (-10*NANO100_PER_SEC);      //delay 10 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );
	}

	if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE)) {

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

			NdasRaidClientDisconnectFromArbitrator( NdasrClient, &NdasrClient->RequestConnection );
			NdasRaidClientDisconnectFromArbitrator( NdasrClient, &NdasrClient->NotificationConnection );
		}

		NdasrClient->ArbitratorMode12 = 0;
		NdasrClient->ConnectionState16 = 0;
	}

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_START );
	NdasrClient->ClientState0 = NRMX_RAID_STATE_TERMINATED;

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );	
	
	while (listEntry = ExInterlockedRemoveHeadList(&NdasrClient->LockList,
												   &NdasrClient->SpinLock)) {

	   if (NdasrClient->NdasrState != NRMX_RAID_STATE_FAILED) {

		   NDAS_ASSERT(FALSE);
	   }

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );
		listEntry = listEntry->Flink;			
				
		RemoveEntryList( &lock->Link );
		NdasRaidClientFreeLock( lock );					
	}

	// Complete pending 
	
	if (!IsListEmpty(&NdasrClient->CcbQueue)) {
	
		PLIST_ENTRY ccbListEntry;
		PCCB		ccb;
		
		while (ccbListEntry = ExInterlockedRemoveHeadList(&NdasrClient->CcbQueue,
														  &NdasrClient->SpinLock)) {
		
			ccb = CONTAINING_RECORD(ccbListEntry, CCB, ListEntry);
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Client thread is terminating. Returning CCB with STOP status.\n") );
	
			NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

			LsCcbSetStatus( ccb, CCB_STATUS_STOP );
			LsCcbCompleteCcb(ccb);
		}
	}
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );
	
	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

VOID
NdasRaidClientMoniteringThreadProc (
	IN PNDASR_CLIENT NdasrClient
	)
{
	NTSTATUS	status;

	// Set lower priority.
	
	KeSetBasePriorityThread( KeGetCurrentThread(), -1 );

	KeSetEvent( &NdasrClient->MonitorThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	status = LurnCreate( NdasrClient->MonitorChild->Lur, NdasrClient->MonitorChild );
	
	if (status == STATUS_SUCCESS) {

		NdasrClient->MonitorRevivedChild = TRUE;
	}

	NdasrClient->MonitorChild = NULL;
	NdasrClient->MonitorThreadHandle = NULL;

	PsTerminateSystemThread( STATUS_SUCCESS );
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

	NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_START) );

	KeQueryTickCount( &currentTick );

	if (NdasrClient->MonitorRevivedChild == FALSE) {
		
		if (currentTick.QuadPart < NdasrClient->NextMonitoringTick.QuadPart) {

			return STATUS_SUCCESS;
		}
	}

	if (NdasrClient->MonitorThreadHandle) {

		// monitor thread is not yet terminated

		//NDAS_ASSERT(FALSE);
		return STATUS_SUCCESS;
	}

	NdasrClient->NextMonitoringTick = currentTick;
	NdasrClient->NextMonitoringTick.QuadPart += (30*NANO100_PER_SEC) / KeQueryTimeIncrement();

	// Check termination request.
		
	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
		
	if (NdasrClient->RequestToTerminate) {

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
		return STATUS_SUCCESS;
	}

	if (!FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED)) {

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
		return STATUS_SUCCESS;
	}

	if (NdasrClient->WaitForSync) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client is in transition\n") );

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
		return STATUS_SUCCESS;
	}

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx], NDASR_NODE_CHANGE_FLAG_UPDATING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is monitoring\n") );

			KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
			return STATUS_SUCCESS;
		}
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Start\n") );

	status = STATUS_SUCCESS;

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

		//BOOLEAN	 defected = FALSE;
		//NDAS_RAID_META_DATA	rmd;

		childLurn = NdasrClient->Lurn->LurnChildren[nidx];

		NDAS_ASSERT( childLurn->LurnChildIdx == nidx );

		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRMX_NODE_FLAG_RUNNING)) {

			continue;
		}

		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {
				
			// Node is defect. No need to revive

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Don't revive defective node %d\n", nidx) );
			continue;
		}
		
		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRMX_NODE_FLAG_OFFLINE)) {

			continue;
		}

		NDAS_ASSERT( NdasrClient->NdasrNodeFlags[nidx] == NRMX_NODE_FLAG_STOP );

		if (NdasrClient->MonitorRevivedChild == FALSE) {

			OBJECT_ATTRIBUTES	objectAttributes;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Trying to revive node %d\n", nidx) );
				
			LurnSendStopCcb( childLurn );
			LurnClose( childLurn );

			// We may need to change LurnDesc if access right is updated.

			if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
				FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

				SetFlag( childLurn->LurnDesc->AccessRight, GENERIC_WRITE );
			}

			NdasrClient->MonitorChild = childLurn;

			KeInitializeEvent( &NdasrClient->MonitorThreadReadyEvent, NotificationEvent, FALSE );

			InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

			status = PsCreateSystemThread( &NdasrClient->MonitorThreadHandle,
										   THREAD_ALL_ACCESS,
										   &objectAttributes,
										   NULL,
										   NULL,
										   NdasRaidClientMoniteringThreadProc,
										   NdasrClient );
				
			if (status != STATUS_SUCCESS || NdasrClient->MonitorThreadHandle == NULL) {
				
				NDAS_ASSERT( FALSE);
				status = STATUS_SUCCESS;
				break;
			}

			KeWaitForSingleObject( &NdasrClient->MonitorThreadReadyEvent,
								   Executive,
								   KernelMode,
								   FALSE,
								   NULL );

			status = STATUS_SUCCESS;
			break;
		}

		NdasrClient->MonitorRevivedChild = FALSE;

		LurnSendStopCcb( childLurn );
		LurnClose( childLurn );

		status = LurnCreate( childLurn->Lur, childLurn );
		
		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("stopped node %d is not yet alive\n", nidx) );

			status = STATUS_SUCCESS;
			continue;
		}
			
		NDAS_ASSERT( LURN_IS_RUNNING(childLurn->LurnStatus) );
				
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Revived node %d\n", nidx) );

		// Update status.
			
		KeAcquireSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, &oldIrql );
		NdasRaidClientUpdateLocalNodeFlags( NdasrClient, childLurn, 0, 0 );
		KeReleaseSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, oldIrql );
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

	UCHAR		ridx;

	LONG		lockFailCount;


	status = STATUS_SUCCESS;

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	while (!IsListEmpty(&NdasrClient->CcbQueue)) {

		listEntry = NdasrClient->CcbQueue.Flink;
		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

		if (ccb->OperationCode == CCB_OPCODE_SHUTDOWN) {

			if (NdasrClient->ShutdownCcb) {

				KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
				LsCcbCompleteCcb( NdasrClient->ShutdownCcb );
				KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			}

			RemoveEntryList( &ccb->ListEntry );
			NdasrClient->ShutdownCcb = ccb;

			continue;
		}

		LsCcbSetStatus( ccb, CCB_STATUS_UNKNOWN_STATUS );

		if (FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ERROR) ||
			NdasrClient->NdasrState != NRMX_RAID_STATE_NORMAL		&&
			NdasrClient->NdasrState != NRMX_RAID_STATE_DEGRADED		&& 
			NdasrClient->NdasrState != NRMX_RAID_STATE_OUT_OF_SYNC) {

			NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_FAILED );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("ccb = %p, ccb->OperationCode = %s, ccb->Cdb[0] =%s\n", 
						 ccb, CcbOperationCodeString(ccb->OperationCode), CdbOperationString(ccb->Cdb[0])) );

			RemoveEntryList( &ccb->ListEntry );

			LsCcbSetStatus( ccb, CCB_STATUS_STOP );
			NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

			KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
			LsCcbCompleteCcb(ccb);
			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

			continue;
		}						

		if (ccb->OperationCode == CCB_OPCODE_UPDATE) {

			PLURN_UPDATE	lurnUpdate;

			lurnUpdate = (PLURN_UPDATE)ccb->DataBuffer;

			switch (lurnUpdate->UpdateClass) {

			case LURN_UPDATECLASS_WRITEACCESS_USERID: {

				if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {

					RemoveEntryList( &ccb->ListEntry );
					ccb->CcbStatus = CCB_STATUS_SUCCESS;
					NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

					KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
					LsCcbCompleteCcb(ccb);
					KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

					continue;
				}

				break;
			}

			case LURN_UPDATECLASS_READONLYACCESS: {

				if (LUR_IS_SECONDARY(NdasrClient->Lurn->Lur)) {

					RemoveEntryList( &ccb->ListEntry );
					ccb->CcbStatus = CCB_STATUS_SUCCESS;
					NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

					KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
					LsCcbCompleteCcb(ccb);
					KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

					continue;
				}

				break;
			}

			default:

				NDAS_ASSERT(FALSE);

				RemoveEntryList( &ccb->ListEntry );
				ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

				KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
				LsCcbCompleteCcb(ccb);
				KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

				continue;
			}
		}

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

		if (ccb->OperationCode == CCB_OPCODE_UPDATE) {

			PLURN_UPDATE	lurnUpdate;

			lurnUpdate = (PLURN_UPDATE)ccb->DataBuffer;

			switch (lurnUpdate->UpdateClass) {

			case LURN_UPDATECLASS_WRITEACCESS_USERID: {

				if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur)) {

					RemoveEntryList( &ccb->ListEntry );
					ccb->CcbStatus = CCB_STATUS_SUCCESS;
					NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

					LsCcbCompleteCcb(ccb);

					continue;
				}

				break;
			}

			case LURN_UPDATECLASS_READONLYACCESS: {

				if (LUR_IS_SECONDARY(NdasrClient->Lurn->Lur)) {

					RemoveEntryList( &ccb->ListEntry );
					ccb->CcbStatus = CCB_STATUS_SUCCESS;
					NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

					LsCcbCompleteCcb(ccb);

					continue;
				}

				break;
			}

			default:

				NDAS_ASSERT(FALSE);

				RemoveEntryList( &ccb->ListEntry );
				ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

				LsCcbCompleteCcb(ccb);

				continue;
			}
		}

		if (ccb->OperationCode == CCB_OPCODE_EXECUTE && 
			(ccb->SrbCdb.CDB10.OperationCode == SCSIOP_WRITE || ccb->SrbCdb.CDB10.OperationCode == SCSIOP_WRITE16)) {

			UINT64	blockAddr;
			UINT32	blockLength;
			UINT64	addrToLock;		 
			UINT32	lengthToLock;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, ("SCSIOP_WRITE\n") );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, ("ccb->DataBufferLength %d\n", ccb->DataBufferLength) );

			LsCcbGetAddressAndLength( &ccb->SrbCdb, &blockAddr, &blockLength );

			NDAS_ASSERT( blockAddr < NdasrClient->Lurn->Lur->MaxChildrenSectorCount && 
						 (blockAddr + blockLength) <= NdasrClient->Lurn->DataUnitBlocks );

			status = NdasRaidClientCheckIoPermission( NdasrClient, 
													  blockAddr, 
													  blockLength, 
													  &addrToLock, 
													  &lengthToLock );
	
			if (status == STATUS_SUCCESS) {

				NdasRaidClientReleaseBlockIoPermissionToClient( NdasrClient, ccb );

			} else { 

				status = NdasRaidClientReleaseRwLock( NdasrClient );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

					KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
					break;
				}

				ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
				KeLeaveCriticalRegion();

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Send lock request for range %I64x:%x\n", addrToLock, lengthToLock) );

				status = NdasRaidClientSendRequestAcquireLock( NdasrClient, 
															   NRMX_LOCK_TYPE_BLOCK_IO,
															   NRMX_LOCK_MODE_SH, 
															   addrToLock, 
															   lengthToLock, 
															   NULL );


				KeEnterCriticalRegion();
				ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

				if (status == STATUS_SUCCESS) {

					KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
					continue;
				}

				NDAS_ASSERT( ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );
				NDAS_ASSERT (status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_SYNCHRONIZATION_REQUIRED );
			}
		}

		if (status != STATUS_SUCCESS) {

			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			break;
		}

		if (!FlagOn(NdasrClient->Lurn->Lur->SupportedNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {

			status = NdasRaidClientDispatchCcb( NdasrClient, ccb );

		} else if (ccb->OperationCode == CCB_OPCODE_FLUSH		||
				   ccb->OperationCode == CCB_OPCODE_EXECUTE					 && 
				   ccb->SrbCdb.CDB10.OperationCode == SCSIOP_START_STOP_UNIT &&
				   ccb->SrbCdb.START_STOP.Start == STOP_UNIT_CODE) {

			status = NdasRaidClientReleaseRwLock( NdasrClient );

			ExReleaseResourceLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource );
			KeLeaveCriticalRegion();

			if (status == STATUS_SUCCESS) {

				status = NdasRaidClientDispatchCcb( NdasrClient, ccb );
			}

			KeEnterCriticalRegion();
			ExAcquireResourceExclusiveLite( &NdasrClient->Lurn->NdasrInfo->BufLockResource, TRUE );

		} else if (!LsCcbIsFlagOn(ccb, CCB_FLAG_ACQUIRE_BUFLOCK)) {

			status = NdasRaidClientDispatchCcb( NdasrClient, ccb );
	
		} else {

			for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx ++) { 

				if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

					continue;
				}

				if (NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] == TRUE) {

					break;
				}

				if (NdasrClient->Lurn->CascadeLock) {

					NDAS_ASSERT( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_CONTENT_LOCK] == FALSE );

					status = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
														  LURNLOCK_OPCODE_ACQUIRE,
														  NDAS_CONTENT_LOCK,
														  TRUE );

					if (status != STATUS_SUCCESS) {
						
						break;
					}
				}

				lockFailCount = 0;

				for (;;) {

					status = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
														  LURNLOCK_OPCODE_ACQUIRE,
														  NDAS_RW_LOCK,
														  FALSE );

					if (status == STATUS_SUCCESS) {

						NDAS_ASSERT( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] == TRUE );
						break;
					}

					if (status == STATUS_LOCK_NOT_GRANTED) {

						if (lockFailCount == 0) {

							if (NdasrClient->NdasrState != NRMX_RAID_STATE_OUT_OF_SYNC) {

								LARGE_INTEGER delay;

								delay.QuadPart = 
									NDAS_MAX_LOCKING_DELAY <= NdasrClient->Lurn->LockingDelay.QuadPart ?
										-NDAS_MAX_LOCKING_DELAY : -NdasrClient->Lurn->LockingDelay.QuadPart;

								delay.QuadPart --;	// prevent to be 0
						
								NDAS_ASSERT( delay.QuadPart < 0 );

								KeDelayExecutionThread( KernelMode, FALSE, &delay );
							}
						}

						lockFailCount++;
						continue;
					}

					NDAS_ASSERT( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_RW_LOCK] == FALSE );
					NDAS_ASSERT( status != STATUS_SUCCESS && status != STATUS_LOCK_NOT_GRANTED );

					break;
				}

				if (status == STATUS_SUCCESS) {

					NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );
					NdasrClient->Lurn->LockingTime = NdasCurrentTime();
				}

				if (NdasrClient->Lurn->CascadeLock) {

					if (status == STATUS_SUCCESS) {

						status = NdasRaidLurnLockSynchrously( NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]],
															  LURNLOCK_OPCODE_RELEASE,
															  NDAS_CONTENT_LOCK,
															  TRUE );

					} 
					
					if (status != STATUS_SUCCESS) {

						NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->NdasLocked[NDAS_CONTENT_LOCK] = FALSE;
					}						
				}

				break;
			}

			NDAS_ASSERT( ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount );

			if (status == STATUS_SUCCESS) {

				status = NdasRaidClientDispatchCcb( NdasrClient, ccb );
			}
		}

		if (status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("status = %x\n", status) );

			NDAS_ASSERT( ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );
			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			break;
		}

		if (ccb->CcbStatus == CCB_STATUS_BAD_SECTOR) {
								
			if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0 || NdasrClient->EmergencyMode) {

				NDAS_ASSERT( NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0  ||
							 NdasrClient->NdasrState == NRMX_RAID_STATE_DEGRADED ||
							 NdasrClient->NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC );

				KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
				RemoveEntryList( &ccb->ListEntry );				
				KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

				LsCcbCompleteCcb(ccb);
				
				KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
				continue;
			}
		}

		if (!(ccb->CcbStatus == CCB_STATUS_SUCCESS			||
			  ccb->CcbStatus == CCB_STATUS_DATA_OVERRUN		||
			  ccb->CcbStatus == CCB_STATUS_INVALID_COMMAND	||
			  ccb->CcbStatus == CCB_STATUS_BUSY				||
			  ccb->CcbStatus == CCB_STATUS_NO_ACCESS)) {

			NDAS_ASSERT( ccb->CcbStatus == CCB_STATUS_STOP			||
						 ccb->CcbStatus == CCB_STATUS_BAD_SECTOR	||
						 ccb->CcbStatus == CCB_STATUS_NOT_EXIST );

			status = STATUS_CLUSTER_NODE_UNREACHABLE;

			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
			break;
		}

		KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

		RemoveEntryList( &ccb->ListEntry );

		if (ccb->CcbStatus != CCB_STATUS_BUSY) {

			NdasRaidClientSetNdasrStatusFlag( NdasrClient, ccb );

		}

		if (ccb->CcbStatus == CCB_STATUS_BUSY) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					   ("ccb->CcbStatusFlags = %X\n", ccb->CcbStatusFlags) );
		}

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

		LsCcbCompleteCcb(ccb);

		KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

	if (status == STATUS_SUCCESS) {

		if (NdasrClient->Lurn->LockingTime.QuadPart) {

			if (lockFailCount == 0) {

				NdasrClient->Lurn->CascadeLock = FALSE;

			} else {

				NdasrClient->Lurn->CascadeLock = TRUE;
			}

			NdasrClient->Lurn->LockingDelay.QuadPart += (NdasCurrentTime().QuadPart - NdasrClient->Lurn->LockingTime.QuadPart);
			NdasrClient->Lurn->LockingDelay.QuadPart >>= 1;

			if (lockFailCount <= 1) {

				NdasrClient->Lurn->LockingDelay.QuadPart >>= 1;
			}

			DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
						("NdasrClient->Lurn->LockingDelay.QuadPart = %I64d\n", NdasrClient->Lurn->LockingDelay.QuadPart) );
		}
	}

	return status;
}

VOID 
NdasRaidClientSetNdasrStatusFlag (
	PNDASR_CLIENT	NdasrClient,
	PCCB			Ccb
	) 
{
	NDAS_ASSERT( Ccb->NdasrStatusFlag8 == 0 );

	// Do not set RAID flag if RAID status is in transition.

	if (NdasrClient->WaitForSync) {

		return;
	}

	KeAcquireSpinLockAtDpcLevel( &Ccb->CcbSpinLock );

	LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_FLAG_VALID );

	switch (NdasrClient->NdasrState) {

	case NRMX_RAID_STATE_NORMAL:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_NORMAL );
		
		break;

	case NRMX_RAID_STATE_DEGRADED:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_DEGRADED );

		break;

	case NRMX_RAID_STATE_OUT_OF_SYNC:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_RECOVERING );

		break;

	case NRMX_RAID_STATE_FAILED:

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_RAID_FAILURE );

		break;
	}

	KeReleaseSpinLockFromDpcLevel( &Ccb->CcbSpinLock );

	NDAS_ASSERT( Ccb->NdasrStatusFlag8 != 0 );

}

NTSTATUS
NdasRaidClientDispatchCcb (
	PNDASR_CLIENT	NdasrClient, 
	PCCB			Ccb
	)
{
	NTSTATUS			status;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, 
				("ccb = %p, ccb->OperationCode = %s, ccb->Cdb[0] =%s\n", 
				 Ccb, CcbOperationCodeString(Ccb->OperationCode), CdbOperationString(Ccb->Cdb[0])) );

	switch (Ccb->OperationCode) {

	case CCB_OPCODE_EXECUTE:

		status = NdasRaidClientExecuteCcb( NdasrClient, Ccb );
		break;

	case CCB_OPCODE_ABORT_COMMAND:

		NDAS_ASSERT(FALSE);

		LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		status = STATUS_SUCCESS;
		break;

	case CCB_OPCODE_RESTART:
		
		NDAS_ASSERT(FALSE);

		LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		status = STATUS_SUCCESS;
		break;

	case CCB_OPCODE_RESETBUS:

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
					("ccb = %p, ccb->OperationCode = %s, ccb->Cdb[0] =%s\n", 
					 Ccb, CcbOperationCodeString(Ccb->OperationCode), CdbOperationString(Ccb->Cdb[0])) );
		
		//NDAS_ASSERT(FALSE);

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

			NDAS_ASSERT(FALSE);
		
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

		NDAS_ASSERT( Ccb->OperationCode == CCB_OPCODE_SMART );

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

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("CCB_OPCODE_QUERY\n") );

	//	Check to see if the CCB is coming for only this LURN.
	
	if (LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {

		NDAS_ASSERT(FALSE);
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
		lurnInfo->UnitBlocks	= NdasrClient->Lurn->DataUnitBlocks;
		lurnInfo->BlockBytes	= NdasrClient->Lurn->BlockBytes;
		lurnInfo->Connections	= 0;
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

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurPrimaryLurnInformation\n") );

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


	NDAS_ASSERT( Ccb->OperationCode == CCB_OPCODE_EXECUTE );
	NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );		

	switch (Ccb->Cdb[0]) {

	case SCSIOP_WRITE:
	case SCSIOP_WRITE16: {

		UINT64	blockAddr;
		UINT32	blockLength;
		UINT64	addrToLock;		//	Address to lock 
		UINT32	lengthToLock;	//


		DebugTrace( NDASSCSI_DBG_LURN_IDE_NOISE, ("SCSIOP_WRITE\n") );

		DebugTrace( NDASSCSI_DBG_LURN_IDE_NOISE, ("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength) );

		if (NdasrClient->Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {

			// Fake write is handled by LurProcessWrite.
			// Just check assumptions
		
			NDAS_ASSERT( NdasrClient->Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE );
		}

		if (NdasrClient->EmergencyMode == FALSE && NdasrClient->Lurn->NdasrInfo->ParityDiskCount) {

			NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) );
		
		} else {

			NDAS_ASSERT( !FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) );
		}

		LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

		if (blockAddr < 4) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("blockAddr = %I64x, blockLength = %x\n", blockAddr, blockLength) );
		}

		if (blockLength * NdasrClient->Lurn->BlockBytes != Ccb->DataBufferLength) {
			
			NDAS_ASSERT(FALSE);
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->MaxDataSendLength < Ccb->DataBufferLength) {

			NDAS_ASSERT(FALSE);
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->Striping == FALSE) {

			NDAS_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes );

		} else {

			NDAS_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->ChildBlockBytes % 
						 (NdasrClient->Lurn->NdasrInfo->ActiveDiskCount - NdasrClient->Lurn->NdasrInfo->ParityDiskCount) == 0 );
		}

		if (blockAddr < NdasrClient->Lurn->Lur->MaxChildrenSectorCount && 
			(blockAddr + blockLength) > NdasrClient->Lurn->DataUnitBlocks ) {

			NDAS_ASSERT(FALSE);

			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (LUR_IS_SECONDARY(NdasrClient->Lurn->Lur)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Secondary writes directly: %I64x:%x\n", blockAddr, blockLength) );
		}
	
		status = NdasRaidClientCheckIoPermission( NdasrClient, 
												  blockAddr, 
												  blockLength, 
												  &addrToLock, 
												  &lengthToLock );
	
		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT(FALSE);
			break;
		}


		// Lock is covered. Handle Ccb here.
			
		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			

		NdasRaidClientReleaseBlockIoPermissionToClient( NdasrClient, Ccb );

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
			
			NDAS_ASSERT(FALSE);
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->MaxDataRecvLength < Ccb->DataBufferLength) {

			NDAS_ASSERT(FALSE);
			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			status = STATUS_SUCCESS;
			break;
		}

		if (NdasrClient->Lurn->NdasrInfo->Striping == FALSE) {

			NDAS_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes );

		} else {

			NDAS_ASSERT( Ccb->DataBufferLength / NdasrClient->Lurn->ChildBlockBytes % 
						(NdasrClient->Lurn->NdasrInfo->ActiveDiskCount - NdasrClient->Lurn->NdasrInfo->ParityDiskCount) == 0 );
		}

		status = NdasRaidClientSendCcbToChildrenSyncronously( NdasrClient, Ccb );			
		break;
	}

	case SCSIOP_INQUIRY: {
		
		INQUIRYDATA	inquiryData;
		UCHAR		model[16] = {0};


		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]) );

		switch (NdasrClient->Lurn->LurnType) {

		case LURN_NDAS_AGGREGATION:
			
			NDAS_ASSERT( sizeof(NDAS_AGGREGATION_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_AGGREGATION_MODEL_NAME, sizeof(NDAS_AGGREGATION_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID0:

			NDAS_ASSERT( sizeof(NDAS_RAID0_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID0_MODEL_NAME, sizeof(NDAS_RAID0_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID1:

			NDAS_ASSERT( sizeof(NDAS_RAID1_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID1_MODEL_NAME, sizeof(NDAS_RAID1_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID4:

			NDAS_ASSERT( sizeof(NDAS_RAID4_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID4_MODEL_NAME, sizeof(NDAS_RAID4_MODEL_NAME) );
			break;

		case LURN_NDAS_RAID5:

			NDAS_ASSERT( sizeof(NDAS_RAID5_MODEL_NAME) <= 16 );
			RtlCopyMemory( model, NDAS_RAID5_MODEL_NAME, sizeof(NDAS_RAID5_MODEL_NAME) );
			break;

		default:

			NDAS_ASSERT(FALSE);
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

		blockCount = NdasrClient->Lurn->DataUnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = blockCount - 1;

		if (logicalBlockAddress < 0xffffffff) {
		
			REVERSE_BYTES( &readCapacityData->LogicalBlockAddress, &logicalBlockAddress );

		} else {

			readCapacityData->LogicalBlockAddress = 0xffffffff;
		}

		blockSize = NdasrClient->Lurn->BlockBytes;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("blockCount = %d\n", blockCount/1024/1024/2) );

		REVERSE_BYTES( &readCapacityData->BytesPerBlock, &blockSize );

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

		blockCount = NdasrClient->Lurn->DataUnitBlocks;

		readCapacityDataEx = (PREAD_CAPACITY_DATA_EX)Ccb->DataBuffer;

		logicalBlockAddress = blockCount - 1;
		REVERSE_BYTES_QUAD( &readCapacityDataEx->LogicalBlockAddress.QuadPart, &logicalBlockAddress );

		blockSize = NdasrClient->Lurn->BlockBytes;

		REVERSE_BYTES( &readCapacityDataEx->BytesPerBlock, &blockSize );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("blockCount = %d\n", blockCount/1024/1024/2) );

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
		
			if (NdasrClient->NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC) {
		
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

			NDAS_ASSERT(FALSE);

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
		
		NDAS_ASSERT( Ccb->Cdb[0] == SCSIOP_LOG_SENSE );

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

			NDAS_ASSERT( !LsCcbIsFlagOn(Ccb, CCB_FLAG_W2K_READONLY_PATCH) );

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
	
	//BlockCount = (ULONG)(NdasrClient->Lurn->EndBlockAddr - NdasrClient->Lurn->StartBlockAddr + 1);
	BlockCount = (ULONG)(NdasrClient->Lurn->DataUnitBlocks);

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

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE,
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

	} else if (cdb->MODE_SELECT.SPBit != 0)	{

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("unsupported save page to non-volitile memory:%x.\n", (ULONG)cdb->MODE_SELECT.SPBit) );

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
	UNREFERENCED_PARAMETER(Ccb);

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

	PCCB						nextCcb[NRMX_MAX_MEMBER_DISK] = {NULL};
	NDASR_CUSTOM_DATA_BUFFER	customBuffer = {FALSE, 0, {0}, {0}, {NULL}, {0}};

	UCHAR					targetChildrenCount = 0;
	UCHAR					filteredTargetChildenCount = 0;

	NDASR_CASCADE_OPTION	ndasRaidCascade;
	BOOLEAN					nonDefectiveOnly;
	BOOLEAN					runningOnly;
	BOOLEAN					oneByOne;


	NDAS_ASSERT( !LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN) );

	try {

		switch (Ccb->OperationCode) {

		case CCB_OPCODE_EXECUTE: {

			if (ndasrInfo->Striping == FALSE) {
				
				if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16 ||
					Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16					  ||
					Ccb->Cdb[0] == SCSIOP_VERIFY || Ccb->Cdb[0] == SCSIOP_VERIFY16) {

					UINT64	blockAddr;
					UINT32	blockLength;
					
					UINT64	accumulateBlockAddress;

					targetChildrenCount = ndasrInfo->ActiveDiskCount;

					ndasRaidCascade	 = NDASR_CASCADE_FORWARD;
					nonDefectiveOnly = TRUE;
					runningOnly		 = TRUE;
					oneByOne		 = FALSE;

					LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

#if __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__
					blockAddr *= __TEST_BLOCK_SIZE__;
					blockLength *= __TEST_BLOCK_SIZE__;
#endif

					customBuffer.DataBufferCount = 0;
					customBuffer.DataBufferAllocated = FALSE;

					accumulateBlockAddress = 0;

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

						if (blockAddr < 
							accumulateBlockAddress + 
							(NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks -
							 NdasrClient->Lurn->Lur->StartOffset)) {

							customBuffer.DataBufferCount ++;

							customBuffer.DataBlockAddress[ridx] = blockAddr - accumulateBlockAddress;

							if (Ccb->DataBuffer) {

								customBuffer.DataBuffer[ridx] = Ccb->DataBuffer;
							}

							if (blockAddr + blockLength <= 
								accumulateBlockAddress + 
								(NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks -
								 NdasrClient->Lurn->Lur->StartOffset)) {

								customBuffer.DataBlockLength[ridx] = blockLength;

								if (Ccb->DataBuffer) {

									customBuffer.DataBufferLength[ridx] = Ccb->DataBufferLength;
								}

							} else {

								NDAS_ASSERT( ridx < ndasrInfo->ActiveDiskCount - 1 );

								customBuffer.DataBlockLength[ridx] = 
									(UINT32)(accumulateBlockAddress + 
											 (NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks -
											  NdasrClient->Lurn->Lur->StartOffset) - 
											 blockAddr);
								
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

								NDAS_ASSERT( customBuffer.DataBlockLength[ridx] + customBuffer.DataBlockLength[ridx+1] == blockLength );
								NDAS_ASSERT( customBuffer.DataBufferLength[ridx] + customBuffer.DataBufferLength[ridx+1] == Ccb->DataBufferLength );
							}

							break;
						}

						accumulateBlockAddress += 
							(NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->UnitBlocks -
							 NdasrClient->Lurn->Lur->StartOffset);
					}

					NDAS_ASSERT( customBuffer.DataBufferCount > 0 );
					break;
				}
			}

			switch (Ccb->Cdb[0]) {

			case SCSIOP_WRITE:
			case SCSIOP_WRITE16: {

				UCHAR	blockChildrenCount;
				ULONG	i, j, k;
				UINT64	blockAddr;
				UINT32	blockLength;

				LsCcbGetAddressAndLength( (PCDB)&Ccb->Cdb[0], &blockAddr, &blockLength );

				targetChildrenCount = ndasrInfo->ActiveDiskCount;

				ndasRaidCascade  = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;
				oneByOne		 = FALSE;

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

						if (ndasrInfo->DistributedParity) {

							parityRoleIndex = (UCHAR)((blockAddr+i) % ndasrInfo->ActiveDiskCount);
					
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

					NDAS_ASSERT( j == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount );

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

						NDAS_ASSERT( NdasrClient->Lurn->ChildBlockBytes % 8 == 0 );

						for (k = 0; k < NdasrClient->Lurn->ChildBlockBytes; k += 8) {

							*((UINT64 *)(customBuffer.DataBuffer[parityRoleIndex] + i * NdasrClient->Lurn->ChildBlockBytes + k)) ^=
								*((UINT64 *)(customBuffer.DataBuffer[ridx] + i * NdasrClient->Lurn->ChildBlockBytes + k));									
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

						NDAS_ASSERT( parityChar == 0 );
					}
				}
#endif

				break;
			}

			case SCSIOP_VERIFY:
			case SCSIOP_VERIFY16: {
	
				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

				ndasRaidCascade  = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;
				oneByOne		 = FALSE;

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
				oneByOne		 = FALSE;

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
				oneByOne		 = FALSE;

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
	
						NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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
				oneByOne		 = FALSE;

				break;
			}

			case SCSIOP_TEST_UNIT_READY: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;
				oneByOne		 = FALSE;

				break;
			}

			case SCSIOP_SYNCHRONIZE_CACHE: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;
				oneByOne		 = FALSE;

				break;
			}

			default:

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Ccb->Cdb[0] = %X is not supported\n", Ccb->Cdb[0]) );

				NDAS_ASSERT( Ccb->Cdb[0] == SCSIOP_LOG_SENSE );

				targetChildrenCount = 0;

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

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
			oneByOne		 = FALSE;

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
			oneByOne		 = TRUE;

			break;
		}

		case CCB_OPCODE_QUERY: {

			switch (((PLUR_QUERY)Ccb->DataBuffer)->InfoClass) {

			case LurEnumerateLurn: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = FALSE;
				runningOnly		 = FALSE;
				oneByOne		 = FALSE;

				break;
			}
	
			case LurRefreshLurn: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount + NdasrClient->Lurn->NdasrInfo->SpareDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = FALSE;
				runningOnly		 = FALSE;
				oneByOne		 = FALSE;

				break;
			}

			case LurPrimaryLurnInformation: {

				targetChildrenCount = NdasrClient->Lurn->NdasrInfo->ActiveDiskCount;

				ndasRaidCascade = NDASR_CASCADE_FORWARD;
				nonDefectiveOnly = TRUE;
				runningOnly		 = TRUE;
				oneByOne		 = FALSE;

				break;
			}
	
			default:
		
				targetChildrenCount = 0;

				NDAS_ASSERT(FALSE);
				leave;

				break;
			}

			break;
		}

		default:

			targetChildrenCount = 0;

			NDAS_ASSERT(FALSE);
			leave;

			break;
		}

		filteredTargetChildenCount = 0;

		for (ridx = 0; ridx < targetChildrenCount; ridx++) {

			if (runningOnly) {
	
				if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {
					
					if (!(nonDefectiveOnly && FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_DEFECTIVE))) {

						NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_DEGRADED || 
									 ridx == NdasrClient->Lurn->NdasrInfo->ActiveDiskCount );
					}

					continue;
				}

				if (!LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->LurnStatus)) {

					//NDAS_ASSERT(FALSE);
					//continue;
				}
			}

			if (nonDefectiveOnly) {
			
				if (FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_DEFECTIVE)) {

					NDAS_ASSERT( NdasrClient->NdasrState == NRMX_RAID_STATE_DEGRADED || 
								 ridx == NdasrClient->Lurn->NdasrInfo->ActiveDiskCount );
					continue;
				}
			}

			if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {
				
				if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16) {

					if (ridx == NdasrClient->OutOfSyncRoleIndex) {

						NDAS_ASSERT( NdasrClient->Lurn->NdasrInfo->ParityDiskCount != 0 );
						continue;
					}
				}
			}

			if (ndasrInfo->Striping == FALSE) {

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

			status = LsCcbInitializeByCcb( Ccb, nextCcb[ridx] );
		
			if (status != STATUS_SUCCESS) {

				leave;
			}

			LsCcbResetFlag( nextCcb[ridx], CCB_FLAG_ACQUIRE_BUFLOCK );

			ASSERT( Ccb->DataBuffer == nextCcb[ridx]->DataBuffer );
	
			//nextCcb[ridx]->AssociateID = ridx;

			LsCcbSetFlag( nextCcb[ridx], Ccb->Flags & CCB_FLAG_SYNCHRONOUS );

			if (customBuffer.DataBuffer[ridx]) {

				nextCcb[ridx]->DataBuffer = customBuffer.DataBuffer[ridx];
				nextCcb[ridx]->DataBufferLength = customBuffer.DataBufferLength[ridx];
			}

			if (ndasrInfo->Striping == FALSE) {

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

			if (filteredTargetChildenCount == targetChildrenCount - 1) {

				if (ndasrInfo->ParityDiskCount == 1) {

					if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

						if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16) {

							break;
						}
					}
				}
			}
		}	 

		if (Ccb->OperationCode == CCB_OPCODE_QUERY &&
			((PLUR_QUERY)Ccb->DataBuffer)->InfoClass == LurPrimaryLurnInformation) {

			if (filteredTargetChildenCount < 1) {

				NDAS_ASSERT(FALSE);

				status = STATUS_CLUSTER_NODE_UNREACHABLE;
				leave;
			}
	
		} else if (ndasrInfo->Striping == FALSE && Ccb->OperationCode == CCB_OPCODE_EXECUTE &&
				   (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16 ||
				    Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16					  ||
				    Ccb->Cdb[0] == SCSIOP_VERIFY || Ccb->Cdb[0] == SCSIOP_VERIFY16)) {

			if (filteredTargetChildenCount < 1 || filteredTargetChildenCount > 2) {
				
				NDAS_ASSERT(FALSE);

				status = STATUS_CLUSTER_NODE_UNREACHABLE;
				leave;
			}

		} else if (Ccb->OperationCode == CCB_OPCODE_UPDATE &&
				   FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		} else {
		
			if (filteredTargetChildenCount < ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) {
	
				NDAS_ASSERT(FALSE);

				status = STATUS_CLUSTER_NODE_UNREACHABLE;
				leave;
			}
		}

		NDAS_ASSERT( status == STATUS_SUCCESS );

		for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

			if (nextCcb[nidx] == NULL) {

				continue;
			}

			switch (nextCcb[nidx]->OperationCode) {

			case CCB_OPCODE_EXECUTE: {

				UINT64	logicalBlockAddress = 0;	
				ULONG	transferBlocks = 0;

				switch (nextCcb[nidx]->SrbCdb.AsByte[0]) {

				case 0x3E:		// READ_LONG
				case SCSIOP_READ:
				case SCSIOP_WRITE:
				case SCSIOP_VERIFY:
				case SCSIOP_SYNCHRONIZE_CACHE: {

					logicalBlockAddress = nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte0 << 24;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte1 << 16;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte2 << 8;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte3;

					if (nextCcb[nidx]->SrbCdb.AsByte[0] == SCSIOP_SYNCHRONIZE_CACHE) {

						NDAS_ASSERT( logicalBlockAddress == 0 );

						transferBlocks = nextCcb[nidx]->SrbCdb.CDB10.TransferBlocksMsb << 8;
						transferBlocks |= nextCcb[nidx]->SrbCdb.CDB10.TransferBlocksLsb;

						NDAS_ASSERT( transferBlocks == 0 );

						break;
					}

					logicalBlockAddress += NdasrClient->Lurn->Lur->StartOffset;

					nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte0 = (UCHAR)(logicalBlockAddress >> 24);
					nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte1 = (UCHAR)(logicalBlockAddress >> 16);
					nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte2 = (UCHAR)(logicalBlockAddress >> 8);
					nextCcb[nidx]->SrbCdb.CDB10.LogicalBlockByte3 = (UCHAR)(logicalBlockAddress);

					break;
				}

				case SCSIOP_READ16:
				case SCSIOP_WRITE16:
				case SCSIOP_VERIFY16:
				case SCSIOP_SYNCHRONIZE_CACHE16: {

					logicalBlockAddress = (UINT64)(nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[0]) << 56;
					logicalBlockAddress |= (UINT64)(nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[1]) << 48;
					logicalBlockAddress |= (UINT64)(nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[2]) << 40;
					logicalBlockAddress |= (UINT64)(nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[3]) << 32;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[4] << 24;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[5] << 16;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[6] << 8;
					logicalBlockAddress |= nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[7];

					if (Ccb->SrbCdb.AsByte[0] == SCSIOP_SYNCHRONIZE_CACHE16) {

						NDAS_ASSERT( logicalBlockAddress == 0 );

						transferBlocks = nextCcb[nidx]->SrbCdb.CDB16.TransferLength[0] << 24;
						transferBlocks |= nextCcb[nidx]->SrbCdb.CDB16.TransferLength[1] << 16;
						transferBlocks |= nextCcb[nidx]->SrbCdb.CDB16.TransferLength[2] << 8;
						transferBlocks |= nextCcb[nidx]->SrbCdb.CDB16.TransferLength[3] << 0;

						NDAS_ASSERT( transferBlocks == 0 );

						break;
					}

					logicalBlockAddress += NdasrClient->Lurn->Lur->StartOffset;

					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[0] = (UCHAR)(logicalBlockAddress >> 56);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[1] = (UCHAR)(logicalBlockAddress >> 48);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[2] = (UCHAR)(logicalBlockAddress >> 40);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[3] = (UCHAR)(logicalBlockAddress >> 32);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[4] = (UCHAR)(logicalBlockAddress >> 24);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[5] = (UCHAR)(logicalBlockAddress >> 16);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[6] = (UCHAR)(logicalBlockAddress >> 8);
					nextCcb[nidx]->SrbCdb.CDB16.LogicalBlock[7] = (UCHAR)(logicalBlockAddress);

					break;
				}

				case SCSIOP_INQUIRY: {
		
					break;
				}

				case SCSIOP_START_STOP_UNIT: {

					break;
				}
	
				case SCSIOP_MODE_SENSE:
	
					break;

				case SCSIOP_MODE_SELECT:

					break;

				case SCSIOP_TEST_UNIT_READY: {

					break;
				}

				default:
		
					NDAS_ASSERT(FALSE);
					break;
				}
		
				break;
			}

			case CCB_OPCODE_ABORT_COMMAND:

				break;

			case CCB_OPCODE_RESTART:
		
				break;

			case CCB_OPCODE_RESETBUS:

			case CCB_OPCODE_NOOP: {

				break;
			}
	
			case CCB_OPCODE_FLUSH:
	
				break;

			case CCB_OPCODE_UPDATE: {

				break;
			}

			case CCB_OPCODE_QUERY:

				break;

			default:

				NDAS_ASSERT(FALSE);
				break;
			}
		}

		if (status != STATUS_SUCCESS) {
			
			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

		} else {

			KEVENT	completionEvent[NRMX_MAX_MEMBER_DISK];
			UCHAR	nidx3;

			for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

				UCHAR	nidx2;

				if (ndasRaidCascade == NDASR_CASCADE_BACKWARD || ndasRaidCascade == NDASR_CASCADE_BACKWARD_CHAINING) {

					nidx2 = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt - nidx - 1;
			
				} else {

					nidx2 = nidx;
				} 

				if (nextCcb[NdasrClient->NodeToRoleMap[nidx2]] == NULL) {

					continue;
				}

				KeInitializeEvent( &completionEvent[NdasrClient->NodeToRoleMap[nidx2]], SynchronizationEvent, FALSE );

				LsCcbSetCompletionRoutine( nextCcb[NdasrClient->NodeToRoleMap[nidx2]], 
										   NdasRaidClientSendCcbToChildrenSyncronouslyCompletionRoutine, 
										   &completionEvent[NdasrClient->NodeToRoleMap[nidx2]] );

				status = LurnRequest( NdasrClient->Lurn->LurnChildren[nidx2], nextCcb[NdasrClient->NodeToRoleMap[nidx2]] );
		
				DebugTrace( NDASSCSI_DBG_LURN_IDE_NOISE, ("LurnRequest status : %08x\n", status) );

				if (status != STATUS_SUCCESS && status != STATUS_PENDING) {

					NDAS_ASSERT(FALSE);
					break;
				}

				if (oneByOne == FALSE) {
					
					continue;
				}

				NDAS_ASSERT( Ccb->OperationCode == CCB_OPCODE_UPDATE );

				status = KeWaitForSingleObject( &completionEvent[NdasrClient->NodeToRoleMap[nidx2]],
											    Executive,
												KernelMode,
												FALSE,
												NULL );

				NDAS_ASSERT( status == STATUS_SUCCESS );

				if (!LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx2]->LurnStatus)) {
				
					if (Ccb->OperationCode != CCB_OPCODE_RESETBUS) {

						NDAS_ASSERT( nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_STOP ||
									 nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_BUSY ||
									 nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_NOT_EXIST );
					}
				}
			
				if (nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_STOP ||
					nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_NOT_EXIST) {
			
					NDAS_ASSERT( !LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx2]->LurnStatus) );
				}

				NdasRaidClientSynchronousCcbCompletion( NdasrClient, Ccb, nextCcb[NdasrClient->NodeToRoleMap[nidx2]] );

				if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

					break;
				}
			}

			nidx3 = nidx;

			if (status == STATUS_PENDING || status == STATUS_SUCCESS) {

				status = STATUS_SUCCESS;
			}

			for (nidx = 0; nidx < nidx3; nidx++) {

				UCHAR		nidx2;
				NTSTATUS	executeStatus;

				if (oneByOne) {

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

				executeStatus = KeWaitForSingleObject( &completionEvent[NdasrClient->NodeToRoleMap[nidx2]],
													   Executive,
													   KernelMode,
													   FALSE,
													   NULL );

				NDAS_ASSERT( executeStatus == STATUS_SUCCESS );

				if (!LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx2]->LurnStatus)) {
				
					if (Ccb->OperationCode != CCB_OPCODE_RESETBUS) {

						NDAS_ASSERT( nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_STOP ||
									 nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_BUSY ||
									 nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_NOT_EXIST );
					}
				}
			
				if (nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_STOP ||
					nextCcb[NdasrClient->NodeToRoleMap[nidx2]]->CcbStatus == CCB_STATUS_NOT_EXIST) {
			
					NDAS_ASSERT( !LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx2]->LurnStatus) );
				}

				if (status == STATUS_SUCCESS && (Ccb->CcbStatus == CCB_STATUS_SUCCESS || Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS)) {

					NdasRaidClientSynchronousCcbCompletion( NdasrClient, Ccb, nextCcb[NdasrClient->NodeToRoleMap[nidx2]] );
				}
			}
		}

		if (status == STATUS_SUCCESS) {

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_SUCCESS			||
						 Ccb->CcbStatus == CCB_STATUS_INVALID_COMMAND	||
						 Ccb->CcbStatus == CCB_STATUS_BAD_SECTOR		||
						 Ccb->CcbStatus == CCB_STATUS_STOP				||
						 Ccb->CcbStatus == CCB_STATUS_NOT_EXIST			||
						 Ccb->CcbStatus == CCB_STATUS_NO_ACCESS			||
						 Ccb->CcbStatus == CCB_STATUS_BUSY );
		}

		//	post-process

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, 
					("Completing Ccb %s with CcbStatus %x, status = %x\n", 
					 CdbOperationString(Ccb->Cdb[0]), Ccb->CcbStatus, status) );

		if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Completing Ccb %s with CcbStatus %x, status = %x\n", 
						 CdbOperationString(Ccb->Cdb[0]), Ccb->CcbStatus, status) );
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Completing Ccb with status %x\n", Ccb->CcbStatus) );
		}

		if (Ccb->CcbStatus == CCB_STATUS_SUCCESS) {

			if (Ccb->OperationCode != CCB_OPCODE_RESETBUS) {

				for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

					if (nextCcb[ridx]) {
		
						NDAS_ASSERT( LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[NdasrClient->RoleToNodeMap[ridx]]->LurnStatus) );
					}
				}
			}
		}

		if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

			if (ndasrInfo->Striping == FALSE) {

				// LURN_NDAS_AGGREGATION

			} else if (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount == 1) {

				// LURN_NDAS_RAID1

			} else if (Ccb->Cdb[0] == 0x3E || Ccb->Cdb[0] == SCSIOP_READ || Ccb->Cdb[0] == SCSIOP_READ16) {

				ULONG	i, j, k;	
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

					NDAS_ASSERT( nextCcb[unReadRidx] == NULL );
			
					if (unReadRidx == 0) {

						nextChild = 1;
					
					} else {

						nextChild = 0;
					}

					NDAS_ASSERT( nextCcb[nextChild] != NULL );

					RtlCopyMemory( customBuffer.DataBuffer[unReadRidx],
								   customBuffer.DataBuffer[nextChild],
								   customBuffer.DataBufferLength[nextChild] );

					nextChild ++;

					for (ridx = nextChild; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

						if (ridx == unReadRidx) {

							continue;
						}

						NDAS_ASSERT( nextCcb[ridx] != NULL );

						for (k = 0; k < customBuffer.DataBufferLength[unReadRidx]; k ++) {

							customBuffer.DataBuffer[unReadRidx][k] ^= customBuffer.DataBuffer[ridx][k];									
						}
					}
				}

				if (ndasrInfo->ParityDiskCount) {

					UCHAR	parityChar;

					for (k = 0; k < customBuffer.DataBufferLength[0]; k++) {

						parityChar = 0;

						for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

							parityChar ^= customBuffer.DataBuffer[ridx][k];
						}

						NDAS_ASSERT( parityChar == 0 );
					}
				}

				for (i = 0; i < Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes; i++ ) {

					UCHAR	parityRoleIndex;

					if (ndasrInfo->ParityDiskCount != 0) {

						if (ndasrInfo->DistributedParity) {

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

					NDAS_ASSERT( j == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount );
				}

#else

				for (i = 0; i < Ccb->DataBufferLength / NdasrClient->Lurn->BlockBytes; i++ ) {

					UCHAR	parityRoleIndex;
					UCHAR	blockChildrenCount;

					blockChildrenCount = ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount;

					if (ndasrInfo->ParityDiskCount == 0) { 

						parityRoleIndex = 0xFF;
					
					} else {

						if (ndasrInfo->DistributedParity) {

							parityRoleIndex = (UCHAR)((blockAddr+i) % ndasrInfo->ActiveDiskCount);

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

							NDAS_ASSERT( NdasrClient->Lurn->ChildBlockBytes % 8 == 0 );

							for (k = 0; k < NdasrClient->Lurn->ChildBlockBytes; k += 8) {

								*((UINT64 *)(customBuffer.DataBuffer[parityRoleIndex] + i * NdasrClient->Lurn->ChildBlockBytes + k)) ^=
									*((UINT64 *)(customBuffer.DataBuffer[ridx] + i * NdasrClient->Lurn->ChildBlockBytes + k));									
							}
						}
					}

					j = 0;

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount; ridx ++) {
		
						UCHAR ridx2;

						if (ridx == parityRoleIndex) {

							continue;
						}

						if (NdasrClient->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {
					
							if (ridx == blockChildrenCount) {

								ridx2 = parityRoleIndex;
							
							} else {

								ridx2 = ridx;
							}

						} else {

							if (ridx == NdasrClient->OutOfSyncRoleIndex) {

								ridx2 = parityRoleIndex;
							
							} else {

								ridx2 = ridx;
							}
						}

						NDAS_ASSERT( ridx2 != NdasrClient->OutOfSyncRoleIndex &&
									ridx2 >= 0 && ridx2 < ndasrInfo->ActiveDiskCount );

						RtlCopyMemory( ((PUCHAR)Ccb->DataBuffer) + i*NdasrClient->Lurn->BlockBytes + j*NdasrClient->Lurn->ChildBlockBytes,
										customBuffer.DataBuffer[ridx2] + i * NdasrClient->Lurn->ChildBlockBytes,
								        NdasrClient->Lurn->ChildBlockBytes );

						j ++;
					}

					NDAS_ASSERT( j == blockChildrenCount );
				}

#endif 

				if (blockAddr == 0) {

					UCHAR zeroBuffer[512] = {0};

#if 1
					UCHAR mbrBuffer[512] = 
					{ 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0xfb, 0x50, 0x07, 0x50, 0x1f, 0xfc, 0xbe, 0x1b, 0x7c,
					  0xbf, 0x1b, 0x06, 0x50, 0x57, 0xb9, 0xe5, 0x01, 0xf3, 0xa4, 0xcb, 0xbd, 0xbe, 0x07, 0xb1, 0x04,
					  0x38, 0x6e, 0x00, 0x7c, 0x09, 0x75, 0x13, 0x83, 0xc5, 0x10, 0xe2, 0xf4, 0xcd, 0x18, 0x8b, 0xf5,
					  0x83, 0xc6, 0x10, 0x49, 0x74, 0x19, 0x38, 0x2c, 0x74, 0xf6, 0xa0, 0xb5, 0x07, 0xb4, 0x07, 0x8b, 
					  0xf0, 0xac, 0x3c, 0x00, 0x74, 0xfc, 0xbb, 0x07, 0x00, 0xb4, 0x0e, 0xcd, 0x10, 0xeb, 0xf2, 0x88, 
					  0x4e, 0x10, 0xe8, 0x46, 0x00, 0x73, 0x2a, 0xfe, 0x46, 0x10, 0x80, 0x7e, 0x04, 0x0b, 0x74, 0x0b, 
					  0x80, 0x7e, 0x04, 0x0c, 0x74, 0x05, 0xa0, 0xb6, 0x07, 0x75, 0xd2, 0x80, 0x46, 0x02, 0x06, 0x83, 
					  0x46, 0x08, 0x06, 0x83, 0x56, 0x0a, 0x00, 0xe8, 0x21, 0x00, 0x73, 0x05, 0xa0, 0xb6, 0x07, 0xeb,  
					  0xbc, 0x81, 0x3e, 0xfe, 0x7d, 0x55, 0xaa, 0x74, 0x0b, 0x80, 0x7e, 0x10, 0x00, 0x74, 0xc8, 0xa0,  
					  0xb7, 0x07, 0xeb, 0xa9, 0x8b, 0xfc, 0x1e, 0x57, 0x8b, 0xf5, 0xcb, 0xbf, 0x05, 0x00, 0x8a, 0x56,
					  0x00, 0xb4, 0x08, 0xcd, 0x13, 0x72, 0x23, 0x8a, 0xc1, 0x24, 0x3f, 0x98, 0x8a, 0xde, 0x8a, 0xfc,
					  0x43, 0xf7, 0xe3, 0x8b, 0xd1, 0x86, 0xd6, 0xb1, 0x06, 0xd2, 0xee, 0x42, 0xf7, 0xe2, 0x39, 0x56,
					  0x0a, 0x77, 0x23, 0x72, 0x05, 0x39, 0x46, 0x08, 0x73, 0x1c, 0xb8, 0x01, 0x02, 0xbb, 0x00, 0x7c,
					  0x8b, 0x4e, 0x02, 0x8b, 0x56, 0x00, 0xcd, 0x13, 0x73, 0x51, 0x4f, 0x74, 0x4e, 0x32, 0xe4, 0x8a,
					  0x56, 0x00, 0xcd, 0x13, 0xeb, 0xe4, 0x8a, 0x56, 0x00, 0x60, 0xbb, 0xaa, 0x55, 0xb4, 0x41, 0xcd,
					  0x13, 0x72, 0x36, 0x81, 0xfb, 0x55, 0xaa, 0x75, 0x30, 0xf6, 0xc1, 0x01, 0x74, 0x2b, 0x61, 0x60,
					  0x6a, 0x00, 0x6a, 0x00, 0xff, 0x76, 0x0a, 0xff, 0x76, 0x08, 0x6a, 0x00, 0x68, 0x00, 0x7c, 0x6a,
					  0x01, 0x6a, 0x10, 0xb4, 0x42, 0x8b, 0xf4, 0xcd, 0x13, 0x61, 0x61, 0x73, 0x0e, 0x4f, 0x74, 0x0b,
					  0x32, 0xe4, 0x8a, 0x56, 0x00, 0xcd, 0x13, 0xeb, 0xd6, 0x61, 0xf9, 0xc3, 0x49, 0x6e, 0x76, 0x61,
					  0x6c, 0x69, 0x64, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x74, 0x61,
					  0x62, 0x6c, 0x65, 0x00, 0x45, 0x72, 0x72, 0x6f, 0x72, 0x20, 0x6c, 0x6f, 0x61, 0x64, 0x69, 0x6e,
					  0x67, 0x20, 0x6f, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74,
					  0x65, 0x6d, 0x00, 0x4d, 0x69, 0x73, 0x73, 0x69, 0x6e, 0x67, 0x20, 0x6f, 0x70, 0x65, 0x72, 0x61,
					  0x74, 0x69, 0x6e, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x44, 0x63, 0xc5, 0xcd, 0x5e, 0x52, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa }; 
#else 
					UCHAR mbrBuffer[512] = {
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc5, 0x79, 0xc7, 0x61, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa };

#endif

					for (ridx = 0; ridx < ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount; ridx ++) {

						if (!RtlEqualMemory((UCHAR*)(Ccb->DataBuffer) + 512*ridx, zeroBuffer, 512)) {

							break;
						}
					}

					NDAS_ASSERT( ridx == 0 || ridx == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount );

					if (NdasrGlobalData.OsMajorVersion >= 6) { // vista or later

						if (ridx == ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) {

							// not initialized

							DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting MBR, ridx= %d\n", ridx) );

							RtlCopyMemory( Ccb->DataBuffer, mbrBuffer, 512 );
						}
					}
				}
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
					
#if __NDAS_SCSI_REMAIN_QUERY_IDE_BUG__
					if (ndasrInfo->Striping == TRUE) {

						RtlCopyMemory( lurnInfo->PrimaryId, &NdasrClient->Lurn->NdasrInfo->NdasRaidId, sizeof(lurnInfo->PrimaryId) );
					}
#else
					if (ndasrInfo->ParityDiskCount) {

						RtlCopyMemory( lurnInfo->PrimaryId, &NdasrClient->Lurn->NdasrInfo->NdasRaidId, sizeof(lurnInfo->PrimaryId) );
					}
#endif
				}
			}
		}

		if (Ccb->OperationCode == CCB_OPCODE_UPDATE) {

			//	Complete the original CCB

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Ccb:%p. Ccb->StatusFlags:%08lx\n", Ccb, Ccb->CcbStatusFlags) );

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
		
						DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Starting DRAID arbiter from updated permission\n") );

						for (nidx=0; nidx<(LONG)NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

							if (FlagOn(NdasrClient->Lurn->LurnChildren[nidx]->AccessRight, GENERIC_WRITE)) {
							
								if (!LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx]->LurnStatus)) {
							
									NDAS_ASSERT(FALSE);									
									break;
								}
							
							} else {

								if (LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx]->LurnStatus)) {
							
									NDAS_ASSERT(FALSE);									
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

								if (LURN_IS_RUNNING(NdasrClient->Lurn->LurnChildren[nidx]->LurnStatus)) {

									SetFlag( NdasrClient->Lurn->LurnChildren[nidx]->AccessRight, GENERIC_WRITE );
								}

								if (NdasrClient->Lurn->LurnChildren[nidx]->LurnDesc) {
							
									NdasrClient->Lurn->LurnChildren[nidx]->LurnDesc->AccessRight |= GENERIC_WRITE;
								}
							}
						}
			
					} else {
				
						// Other case does not happen.

						NDAS_ASSERT(FALSE);
					}

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to update access right of all nodes\n") );

					for (nidx = 0; nidx < (LONG)NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

						if (FlagOn(NdasrClient->Lurn->LurnChildren[nidx]->AccessRight, GENERIC_WRITE)) {

							NDAS_ASSERT(FALSE);
						}
					}
				}
			}

			if (Ccb->CcbStatus == CCB_STATUS_BUSY) {
	
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("CCB_OPCODE_UPDATE: return CCB_STATUS_BUSY\n") );
			}
		}

		status = STATUS_SUCCESS;

	} finally {

		for (ridx = 0; ridx < targetChildrenCount; ridx++) {

			if (nextCcb[ridx]) {

				LsCcbFree(nextCcb[ridx]);
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
	IN PNDASR_CLIENT	NdasrClient, 
	IN PCCB				Ccb,
	IN PCCB				ChildCcb
	)
{
	KIRQL				oldIrql;


	if (ChildCcb->CcbStatus != CCB_STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ChildCcb status %x\n", ChildCcb->CcbStatus) );
	}

	if (ChildCcb->OperationCode == CCB_OPCODE_STOP) {

		KeAcquireSpinLock( &Ccb->CcbSpinLock, &oldIrql );

		switch (ChildCcb->CcbStatus) {
	
		case CCB_STATUS_SUCCESS:
	
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;		
			break;

		default:

			NDAS_ASSERT(FALSE);
			Ccb->CcbStatus = ChildCcb->CcbStatus;

			break;
		}
	
		LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

		KeReleaseSpinLock( &Ccb->CcbSpinLock, oldIrql );

		return STATUS_SUCCESS;
	}

	// Find proper Ccbstatus based on OriginalCcb->CcbStatus(Empty or may contain first completed child Ccb's staus) and this Ccb.

	if (ChildCcb->CcbStatus != CCB_STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, ("LurnRAID1RCcbCompletion: CcbStatus = %x\n", ChildCcb->CcbStatus) );
	}

	KeAcquireSpinLock( &Ccb->CcbSpinLock, &oldIrql );

	if (ChildCcb->OperationCode == CCB_OPCODE_UPDATE) {

		switch (ChildCcb->CcbStatus) {

		case CCB_STATUS_SUCCESS:

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			Ccb->CcbStatus = STATUS_SUCCESS;
			break;

		case CCB_STATUS_NO_ACCESS:

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );

			Ccb->CcbStatus = CCB_STATUS_NO_ACCESS;
	
			break;

		case CCB_STATUS_BUSY: 

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			Ccb->CcbStatus = CCB_STATUS_BUSY;

			break;

		case CCB_STATUS_STOP:

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

			Ccb->CcbStatus = CCB_STATUS_STOP;

		default: 

			NDAS_ASSERT(FALSE);
			Ccb->CcbStatus = ChildCcb->CcbStatus;
		}

	} else if (ChildCcb->OperationCode == CCB_OPCODE_QUERY		||
			   ChildCcb->OperationCode == CCB_OPCODE_RESETBUS	||
			   ChildCcb->OperationCode == CCB_OPCODE_NOOP) {

		NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

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

			NDAS_ASSERT(FALSE);

			Ccb->CcbStatus = ChildCcb->CcbStatus;
			break;
		}
	
	} else if (ChildCcb->OperationCode == CCB_OPCODE_EXECUTE) {

		switch (ChildCcb->Cdb[0]) {
		
		case 0x3E:			// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:	

			if (NdasrClient->Lurn->LurnType == LURN_NDAS_RAID1) {

				NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );
			
			} else {

				NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );
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
	
			case CCB_STATUS_BAD_SECTOR:

				Ccb->CcbStatus = CCB_STATUS_BAD_SECTOR;
				break;

			default:			

				NDAS_ASSERT(FALSE);

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

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

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

				NDAS_ASSERT(FALSE);

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

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

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

				NDAS_ASSERT(FALSE);

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

			NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

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

			case CCB_STATUS_BAD_SECTOR:

				Ccb->CcbStatus = CCB_STATUS_BAD_SECTOR;
				break;

			default:			

				NDAS_ASSERT(FALSE);

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

		NDAS_ASSERT(FALSE);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;			
	}

	LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

	KeReleaseSpinLock( &Ccb->CcbSpinLock, oldIrql );

	return STATUS_SUCCESS;
}

#if 0

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

	KeAcquireSpinLock( &Ccb->CcbSpinLock, &oldIrql );


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ChildCcb update status %x\n", ChildCcb->CcbStatus) );

	switch (ChildCcb->CcbStatus) {

	case CCB_STATUS_SUCCESS:

		if (Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS) {

			Ccb->CcbStatus = STATUS_SUCCESS;
		}

		break;

	case CCB_STATUS_NO_ACCESS:

		NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS );

		Ccb->CcbStatus = CCB_STATUS_NO_ACCESS;
	
		break;

	case CCB_STATUS_BUSY: 

		NDAS_ASSERT( Ccb->CcbStatus == CCB_STATUS_UNKNOWN_STATUS || Ccb->CcbStatus == STATUS_SUCCESS );

		Ccb->CcbStatus = CCB_STATUS_BUSY;

		break;

	default: 

		NDAS_ASSERT(FALSE);
	}

	LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

	KeReleaseSpinLock( &Ccb->CcbSpinLock, oldIrql );

	return STATUS_SUCCESS;
}

#endif

NTSTATUS
NdasRaidClientRefreshCcbStatusFlag (
	IN  PNDASR_CLIENT	NdasrClient,
	OUT PULONG			CcbStatusFlags
	)
{
	NTSTATUS		status;

	CCB				ccb;

	PLUR_QUERY		lurQuery;
	BYTE			lurBuffer[SIZE_OF_LURQUERY(0, sizeof(LURN_REFRESH))];
	PLURN_REFRESH	lurnRefresh;

	return STATUS_SUCCESS;

	//
	//	initialize query CCB
	//
	LsCcbInitialize( NULL, NULL, TRUE, &ccb );

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

	//ccb.CcbCurrentStackLocation->Lurn = NdasrClient->Lurn;

	status = NdasRaidClientQuery( NdasrClient, &ccb );

	if (!NT_SUCCESS(status)) {

		NDAS_ASSERT(FALSE);

		KDPrint( 1, ("LurnRequest() failed.\n") );

		LsCcbCleanUp( &ccb ); 
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	*CcbStatusFlags |= lurnRefresh->CcbStatusFlags;

	LsCcbCleanUp( &ccb ); 
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

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	if (IsListEmpty(&NdasrClient->LockList)) {

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LockList Empty\n") );

		return STATUS_SUCCESS;
	}

	for (listEntry = NdasrClient->LockList.Flink;
		 listEntry != &NdasrClient->LockList;
		 listEntry = listEntry->Flink) {

		lock = CONTAINING_RECORD (listEntry, NDASR_CLIENT_LOCK, Link);

		NDAS_ASSERT( lock->Status == LOCK_STATUS_GRANTED || lock->Status == LOCK_STATUS_HOLDING );

		if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
			
			NDAS_ASSERT(FALSE);
		}

		lock->Status = LOCK_STATUS_HOLDING;
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

	NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_MODE) );

	status = NdasRaidClientSendRequestReleaseLock( NdasrClient, NRMX_LOCK_ID_ALL, NULL );

	if (status != STATUS_SUCCESS) {

		return status;
	}

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
	
	while (!IsListEmpty(&NdasrClient->LockList)) {
	
		listEntry = RemoveHeadList( &NdasrClient->LockList );
		
		lock = CONTAINING_RECORD(listEntry, NDASR_CLIENT_LOCK, Link);

		NDAS_ASSERT( lock->Status == LOCK_STATUS_HOLDING );
		
		NdasRaidClientFreeLock( lock );
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

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

	addr += NdasrClient->Lurn->Lur->StartOffset;

	if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0) {

		return STATUS_SUCCESS;
	}
		
	KeAcquireSpinLock( &ndasrInfo->SpinLock, &oldIrql );

	KeAcquireSpinLockAtDpcLevel( &NdasrClient->SpinLock );

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

	KeReleaseSpinLockFromDpcLevel( &NdasrClient->SpinLock );
	
	KeReleaseSpinLock( &ndasrInfo->SpinLock, oldIrql );

	return status;
}

VOID
NdasRaidClientRefreshRaidStatusWithoutArbitrator (
	PNDASR_CLIENT	NdasrClient
	)
{
	NTSTATUS	status;
	KIRQL		oldIrql;
	UCHAR		nidx, ridx;
	UCHAR		newNdasrState;

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	// Update LocalNodeFlags to NdasrNodeFlags
	
	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
		NdasrClient->NdasrNodeFlags[nidx] = NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx];

		if (NdasrClient->EmergencyMode == FALSE) {

			if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

				NdasrClient->NdasrNodeFlags[nidx] = NRMX_NODE_FLAG_DEFECTIVE;			
			}
		
		} else {

			NdasrClient->NdasrNodeFlags[nidx] &= ~NRMX_NODE_FLAG_DEFECTIVE;
		}
	}

	NdasrClient->OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;

	status = STATUS_SUCCESS;

	for (ridx = 0; ridx < NdasrClient->Lurn->LurnChildrenCnt; ridx++) {

		if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0) {

			NdasrClient->RoleToNodeMap[ridx] = ridx;
			NdasrClient->NodeToRoleMap[NdasrClient->RoleToNodeMap[ridx]] = (UCHAR)ridx;

			continue;
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
					("MAPPING Lurn node %d to RAID role %d\n", 
					 NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx, ridx) );

		NdasrClient->RoleToNodeMap[ridx] = (UCHAR)NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx;
		NdasrClient->NodeToRoleMap[NdasrClient->RoleToNodeMap[ridx]] = (UCHAR)ridx;

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

			if (ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount) {

				//NDAS_ASSERT( !FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) );
				
				if (NdasrClient->EmergencyMode == FALSE) {

					if (NdasrClient->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {
						
						NdasrClient->OutOfSyncRoleIndex = (UCHAR)ridx;
					
					} else {

						NDAS_ASSERT(FALSE);
						status = STATUS_INVALID_PARAMETER;
						break;
					}
				}

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is out-of-sync\n", NdasrClient->RoleToNodeMap[ridx], ridx) );
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting out of sync role: %d\n", NdasrClient->OutOfSyncRoleIndex) );
			}
		}

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

			if (NdasrClient->EmergencyMode == FALSE) {

				//NDAS_ASSERT( ridx >= NdasrClient->Lurn->NdasrInfo->ActiveDiskCount );

				SetFlag( NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_DEFECTIVE );
		
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is defective\n",  NdasrClient->RoleToNodeMap[ridx], ridx) );
			
				SetFlag( NdasrClient->NdasrDefectCodes[NdasrClient->RoleToNodeMap[ridx]], 
						 NdasRaidRmdUnitStatusToDefectCode(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus) );
			}
		}

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_OFFLINE)) {

			NDAS_ASSERT( FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_OFFLINE) &&
						FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_STOP) );
					
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
						("Offlien Node %d is enabled\n", NdasrClient->RoleToNodeMap[ridx]) );
				
			continue;
		}
	}

	if (status != STATUS_SUCCESS) {

		NdasrClient->NdasrState = NRMX_RAID_STATE_FAILED;
		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
		return;
	}

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {

		NDAS_ASSERT( NdasrClient->NdasrNodeFlags[nidx] == NRMX_NODE_FLAG_RUNNING							||
					 NdasrClient->NdasrNodeFlags[nidx] == NRMX_NODE_FLAG_DEFECTIVE							||
					 NdasrClient->Lurn->Lur->EmergencyMode &&
					 NdasrClient->NdasrNodeFlags[nidx] == (NRMX_NODE_FLAG_RUNNING | NRMX_NODE_FLAG_OFFLINE) ||
					 NdasrClient->NdasrNodeFlags[nidx] == (NRMX_NODE_FLAG_STOP | NRMX_NODE_FLAG_OFFLINE)	||
					 NdasrClient->NdasrNodeFlags[nidx] == NRMX_NODE_FLAG_STOP );
	}

	// Test new RAID status only when needed, i.e: node has changed or first time.

	newNdasrState = NRMX_RAID_STATE_NORMAL;

	if (NdasrClient->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE) {

		newNdasrState = NRMX_RAID_STATE_OUT_OF_SYNC;
	}

	for (ridx = 0; ridx < NdasrClient->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { // i : role index
			
		if (!FlagOn(NdasrClient->NdasrNodeFlags[NdasrClient->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

			switch (newNdasrState) {

			case NRMX_RAID_STATE_NORMAL: {

				NdasrClient->OutOfSyncRoleIndex = ridx;
				newNdasrState = NRMX_RAID_STATE_DEGRADED;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting new out of sync role: %d\n", ridx) );

				break;
			}

			case NRMX_RAID_STATE_OUT_OF_SYNC: {

				if (NdasrClient->OutOfSyncRoleIndex == ridx) {
					
					newNdasrState = NRMX_RAID_STATE_DEGRADED;
				
				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrClient->RoleToNodeMap[ridx]) );

					newNdasrState = NRMX_RAID_STATE_FAILED;
				}

				break;
			}

			case NRMX_RAID_STATE_DEGRADED: {

				NDAS_ASSERT( NdasrClient->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE && 
							NdasrClient->OutOfSyncRoleIndex != ridx );
				
				newNdasrState = NRMX_RAID_STATE_FAILED;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrClient->RoleToNodeMap[ridx]) );

				break;
			}

			default:

				break;			
			}
		}
	}

	if (NdasrClient->NdasrState != newNdasrState) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Changing DRAID Status from %x to %x\n", NdasrClient->NdasrState, newNdasrState) );

		NdasrClient->NdasrState = newNdasrState;
	}

	NdasrClient->Usn = NdasrClient->Lurn->NdasrInfo->Rmd.uiUSN;

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
}

NTSTATUS
NdasRaidClientEstablishArbitrator (
	PNDASR_CLIENT	NdasrClient
	) 
{
	NTSTATUS		status;

	PNRMX_HEADER	message;
	ULONG			msgLength;

	LONG			result;
	ULONG			addtionalLength;

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	UNREFERENCED_PARAMETER( NdasrClient );
	
	do {

		status = NdasRaidClientConnectToArbitrator( NdasrClient, &NdasrClient->NotificationConnection );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to connect notification\n") );
			break;
		}

		// Register as notification type
	
		status = NdasRaidClientRegisterToRemoteArbitrator( NdasrClient, 
														   &NdasrClient->NotificationConnection, 
														   NRMX_CONN_TYPE_NOTIFICATION );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to register connect notification\n") );
			break;
		}
	
		status = NdasRaidClientConnectToArbitrator( NdasrClient, &NdasrClient->RequestConnection );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to connect request\n") );
			break;
		}

		// Register as request type

		status = NdasRaidClientRegisterToRemoteArbitrator( NdasrClient, 
														   &NdasrClient->RequestConnection, 
														   NRMX_CONN_TYPE_REQUEST );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to register connect request\n") );
			break;
		}

		NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED >> 16;
		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED );

		// Wait for network event of notification connection 

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Connected to arbiter\n") );	

		result = 0;
	
		status = LpxTdiV2Recv( NdasrClient->NotificationConnection.ConnectionFileObject, 
							   (PUCHAR)NdasrClient->NotificationConnection.ReceiveBuf,
							   sizeof(NRMX_HEADER),
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRMX_HEADER)) {

			status = STATUS_CONNECTION_DISCONNECTED;
			break;
		}

		message = (PNRMX_HEADER)NdasrClient->NotificationConnection.ReceiveBuf;
		msgLength = NTOHS( message->Length2 );
	
		if (msgLength > NRMX_MAX_REQUEST_SIZE || msgLength < sizeof(NRMX_HEADER)) {

			NDAS_ASSERT(FALSE);
			status = STATUS_CONNECTION_DISCONNECTED;
			break;
		}

		if (msgLength != sizeof(NRMX_HEADER)) {

			result = 0;
			addtionalLength = msgLength - sizeof(NRMX_HEADER);
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reading additional message data %d bytes\n", addtionalLength) );
		
			status = LpxTdiV2Recv( NdasrClient->NotificationConnection.ConnectionFileObject, 
								   (PUCHAR)(NdasrClient->NotificationConnection.ReceiveBuf + sizeof(NRMX_HEADER)),
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
													  (PNRMX_HEADER)NdasrClient->NotificationConnection.ReceiveBuf,
													  NULL );

		if (status != STATUS_SUCCESS) {

			break;
		}
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		if (NdasrClient->RequestConnection.State == NDASR_CLIENT_CONNECTION_STATE_CONNECTED ||
			NdasrClient->RequestConnection.State == NDASR_CLIENT_CONNECTION_STATE_REGISTERED) {

			NdasRaidClientDisconnectFromArbitrator( NdasrClient, &NdasrClient->RequestConnection );
		}

		if (NdasrClient->NotificationConnection.State == NDASR_CLIENT_CONNECTION_STATE_CONNECTED ||
			NdasrClient->NotificationConnection.State == NDASR_CLIENT_CONNECTION_STATE_REGISTERED) {

			NdasRaidClientDisconnectFromArbitrator( NdasrClient, &NdasrClient->NotificationConnection );
		}

		NdasrClient->ConnectionState16 = NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED >> 16;
		ClearFlag( NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED );
	}
	
	return status;
}

NTSTATUS
NdasRaidClientConnectToArbitrator (
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

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Connecting to arbiter\n") );
	
	KeAcquireSpinLock(&Connection->SpinLock, &oldIrql);

	NDAS_ASSERT( Connection->State == NDASR_CLIENT_CONNECTION_STATE_INIT );

	Connection->State = NDASR_CLIENT_CONNECTION_STATE_CONNECTING;

	KeReleaseSpinLock(&Connection->SpinLock, oldIrql);

	// Find local address to use.

	for (nidx = 0; nidx < NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
		
		// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
	
		if (!FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_RUNNING) ||
			FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {
	
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Lurn is not running. Skip reading node %d.\n", nidx) );
			
			continue;
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)NdasrClient->Lurn->LurnChildren[nidx]->LurnExtension;

		if (!ideDisk) {

			continue;
		}

		localAddr = ideDisk->LanScsiSession->NdasBindAddress.Address[0].NdasAddress.Lpx;

		break;
	}

	if (nidx == NdasrClient->Lurn->LurnChildrenCnt) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("No local address to use\n") );

		status = STATUS_UNSUCCESSFUL;

		goto errout;
	}

	connected = FALSE;

	// Try to connect for each arbiter address
	
	for (i=0; i < NDAS_RAID_ARBITRATOR_ADDR_COUNT; i++) {

		if (NdasrClient->Lurn->NdasrInfo->Rmd.ArbitratorInfo[i].Type != NDAS_RAID_ARBITRATOR_TYPE_LPX) {

			NDAS_ASSERT( NdasrClient->Lurn->NdasrInfo->Rmd.ArbitratorInfo[i].Type == NDAS_RAID_ARBITRATOR_TYPE_NONE );
			continue;
		}

		RtlCopyMemory( remoteAddr.Node, NdasrClient->Lurn->NdasrInfo->Rmd.ArbitratorInfo[i].Addr, 6 );
		remoteAddr.Port = HTONS(NRMX_ARBITRRATOR_LPX_PORT);

		status = LpxTdiV2OpenAddress( &localAddr, &addressFileHandle, &addressFileObject );

		if (!NT_SUCCESS(status)) {

			NDAS_ASSERT(FALSE);

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

			NDAS_ASSERT(FALSE);

			LpxTdiV2CloseAddress( addressFileHandle, addressFileObject );
			addressFileHandle = NULL; 
			addressFileObject = NULL;

			continue;
		}

		status = LpxTdiV2AssociateAddress( connectionFileObject, addressFileHandle );

		if (!NT_SUCCESS(status)) {

			NDAS_ASSERT(FALSE);

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
	
	KeAcquireSpinLock( &Connection->SpinLock, &oldIrql );
	Connection->State = NDASR_CLIENT_CONNECTION_STATE_CONNECTED;
	KeReleaseSpinLock( &Connection->SpinLock, oldIrql );
	
	return STATUS_SUCCESS;

errout:
	
	KeAcquireSpinLock( &Connection->SpinLock, &oldIrql );
	Connection->State = NDASR_CLIENT_CONNECTION_STATE_INIT;
	KeReleaseSpinLock( &Connection->SpinLock, oldIrql );
	
	return status;
}

NTSTATUS
NdasRaidClientDisconnectFromArbitrator (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection
	) 
{
	KIRQL oldIrql;

	UNREFERENCED_PARAMETER( NdasrClient );

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Disconnecting from arbiter\n") );
	
	if (Connection->State == NDASR_CLIENT_CONNECTION_STATE_INIT) {

		NDAS_ASSERT(FALSE);
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Already disconnected\n") );
		
		return STATUS_SUCCESS;
	}

	Connection->Sequence = 0;

	if (LpxTdiV2IsRequestPending(&Connection->ReceiveOverlapped, 0)) {

		LpxTdiV2CancelRequest( Connection->ConnectionFileObject, &Connection->ReceiveOverlapped, 0, FALSE, 0 );
	}

	NDAS_ASSERT( !LpxTdiV2IsRequestPending(&Connection->ReceiveOverlapped, 0) );
	
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

	KeAcquireSpinLock( &Connection->SpinLock, &oldIrql );
	Connection->State = NDASR_CLIENT_CONNECTION_STATE_INIT;
	KeReleaseSpinLock( &Connection->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientRegisterToRemoteArbitrator (
	PNDASR_CLIENT				NdasrClient,
	PNDASR_CLIENT_CONNECTION	Connection,
	UCHAR						ConType	// NRMX_CONN_TYPE_*
	) 
{
	NTSTATUS		status;

	NRMX_REGISTER 	registerMsg = {0};
	NRMX_HEADER		replyMsg = {0};
	LONG			result;
	KIRQL			oldIrql;
	
	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	NDAS_ASSERT( Connection->State == NDASR_CLIENT_CONNECTION_STATE_CONNECTED );

	Connection->Sequence++;

	registerMsg.Header.Signature = HTONL(NRMX_SIGNATURE);
	registerMsg.Header.Command	 = NRMX_CMD_REGISTER;
	registerMsg.Header.Length2   = HTONS((UINT16)sizeof(NRMX_REGISTER));
	registerMsg.Header.ReplyFlag = 0;
	registerMsg.Header.Sequence2 = HTONS((UINT16)Connection->Sequence);
	registerMsg.Header.Result    = 0;

	registerMsg.ConnType		= ConType;
	registerMsg.Usn4			= HTONL( NdasrClient->Lurn->NdasrInfo->Rmd.uiUSN );
	
	registerMsg.LocalClient = LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) || 
							  FlagOn( NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE );

	RtlCopyMemory( &registerMsg.RaidSetId, &NdasrClient->Lurn->NdasrInfo->NdasRaidId, sizeof(GUID) );
	RtlCopyMemory( &registerMsg.ConfigSetId, &NdasrClient->Lurn->NdasrInfo->ConfigSetId, sizeof(GUID) );	
		
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending register message to remote arbiter\n") );

	// Send register message

	status = LpxTdiV2Send( Connection->ConnectionFileObject, 
						   (PUCHAR)&registerMsg, 
						   sizeof(NRMX_REGISTER), 
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != sizeof(NRMX_REGISTER)) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send %x\n", status) );

		return STATUS_CONNECTION_DISCONNECTED;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend status=%x, result=%x.\n", status, result) );

	status = LpxTdiV2Recv( Connection->ConnectionFileObject, 
						   (PUCHAR)&replyMsg, 
						   sizeof(NRMX_HEADER),
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != sizeof(NRMX_HEADER)) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to recv %x\n", status) );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	// Check received message
	
	if (NTOHL(replyMsg.Signature) != NRMX_SIGNATURE ||
		replyMsg.Command != NRMX_CMD_REGISTER		|| 
		replyMsg.ReplyFlag !=1 ||
		NTOHS(replyMsg.Length2) !=  sizeof(NRMX_HEADER)) {
		
		NDAS_ASSERT(FALSE);
		return STATUS_UNSUCCESSFUL;
	}
	
	switch (replyMsg.Result) {
	
	case NRMX_RESULT_SUCCESS:

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Registration succeeded.\n") );
		
		KeAcquireSpinLock( &Connection->SpinLock, &oldIrql );
		Connection->State = NDASR_CLIENT_CONNECTION_STATE_REGISTERED;
		KeReleaseSpinLock( &Connection->SpinLock, oldIrql );	
		
		return STATUS_SUCCESS;
	
	case NRMX_RESULT_UNSUCCESSFUL:
	case NRMX_RESULT_RAID_SET_NOT_FOUND:
	default:
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Registration failed with result %s\n", NdasRixGetResultString(replyMsg.Result)) );
		
		return STATUS_UNSUCCESSFUL;
	}
}

NTSTATUS 
NdasRaidClientResetRemoteArbitratorContext (
	PNDASR_CLIENT		NdasrClient
	) 
{
	KIRQL				oldIrql;
	PLIST_ENTRY			listEntry;
	PNDASR_CLIENT_LOCK	lock;
	LARGE_INTEGER		timeout;
	BOOLEAN				lockInUse;

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Resetting remote arbiter context\n") );
	
	NdasRaidClientDisconnectFromArbitrator( NdasrClient, &NdasrClient->RequestConnection );	
	NdasRaidClientDisconnectFromArbitrator( NdasrClient, &NdasrClient->NotificationConnection );

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
	
	// Remove lock contexts

	while (!IsListEmpty(&NdasrClient->LockList)) {

		lockInUse = FALSE;

		listEntry = NdasrClient->LockList.Flink;

		lock = CONTAINING_RECORD( listEntry, NDASR_CLIENT_LOCK, Link );

		if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {

			NDAS_ASSERT(FALSE);

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Lock %I64x:%x is in use. Waiting for freed\n", lock->BlockAddress, lock->BlockLength) );
			
			timeout.QuadPart = - NANO100_PER_SEC /2;

			// Wait until lock is not in use.
			
			KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
			
			KeDelayExecutionThread( KernelMode, FALSE, &timeout );
			
			KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

		} else {

			// No one is using the lock. Free it and restart outer loop

			RemoveEntryList( &lock->Link );
			NdasRaidClientFreeLock( lock );
		}
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}


NTSTATUS
NdasRaidClientSendRequestAcquireLock (
	PNDASR_CLIENT	NdasrClient,
	UCHAR			LockType,		// NRMX_LOCK_TYPE_BLOCK_IO
	UCHAR			LockMode,		// NRMX_LOCK_MODE_*
	UINT64			BlockAddress,	// in sector for block IO lock
	UINT32			BlockLength,	// in sector.
	PLARGE_INTEGER	Timeout
   )
{
	NTSTATUS					status;

	NRMX_ACQUIRE_LOCK			acquireLockRequest;
	PNRMX_ACQUIRE_LOCK_REPLY	acquireLockReply;
	UINT32						msgLength;
	LONG						result;
	PNDASR_CLIENT_LOCK			lock;
	
	UNREFERENCED_PARAMETER( Timeout );

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );
	NDAS_ASSERT( Timeout == NULL );
	NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
			    ("Sending NRMX_CMD_ACQUIRE_LOCK message BlockAddress = %I64x, BlockLength = %x\n", BlockAddress, BlockLength) );

	msgLength = sizeof(NRMX_ACQUIRE_LOCK);

	RtlZeroMemory( &acquireLockRequest, msgLength );

	acquireLockRequest.Header.Signature	= NTOHL(NRMX_SIGNATURE);
	acquireLockRequest.Header.Command	= NRMX_CMD_ACQUIRE_LOCK;
	acquireLockRequest.Header.Length2	= NTOHS((UINT16)msgLength);
	acquireLockRequest.Header.Sequence2	= NTOHS(NdasrClient->RequestSequence);
	acquireLockRequest.Header.Usn4		= NTOHL(NdasrClient->Usn );

	NdasrClient->RequestSequence++;

	acquireLockRequest.LockType			= LockType;
	acquireLockRequest.LockMode			= LockMode;
	acquireLockRequest.BlockAddress8	= NTOHLL(BlockAddress);
	acquireLockRequest.BlockLength4		= NTOHL(BlockLength);

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		NDASR_LOCAL_MSG		ndasrRequestLocalMsg;
		
		PLIST_ENTRY			listEntry;
		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
		
		InitializeListHead( &ndasrRequestLocalMsg.ListEntry );
		ndasrRequestLocalMsg.NrmxHeader = (PNRMX_HEADER)&acquireLockRequest;

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

			NDAS_ASSERT(FALSE);
			return STATUS_CONNECTION_DISCONNECTED;
		}

		KeClearEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent );

		listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
												 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		NDAS_ASSERT( listEntry );

		ndasrReplyLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );
		acquireLockReply = (PNRMX_ACQUIRE_LOCK_REPLY)ndasrReplyLocalMsg->NrmxHeader;

		NDAS_ASSERT( ndasrReplyLocalMsg == &NdasrClient->Lurn->NdasrInfo->RequestChannelReply );
		NDAS_ASSERT( (PNRMX_HEADER)acquireLockReply == &NdasrClient->Lurn->NdasrInfo->NrmxHeader );

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

			NDAS_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		acquireLockReply = (PNRMX_ACQUIRE_LOCK_REPLY)NdasrClient->RequestConnection.ReceiveBuf;
	
		result = 0;

		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject,
							   (PUCHAR)acquireLockReply,
							   sizeof(NRMX_HEADER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRMX_HEADER)) {

			NDAS_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to received request header\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	if (acquireLockReply->Header.Signature != HTONL(NRMX_SIGNATURE)	||
		acquireLockReply->Header.Command != NRMX_CMD_ACQUIRE_LOCK	||
		!acquireLockReply->Header.ReplyFlag) {

		NDAS_ASSERT(FALSE);
	
		return STATUS_CONNECTION_DISCONNECTED;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
				("acquireLockReply->Header.Result = %x\n", acquireLockReply->Header.Result) );

	if (acquireLockReply->Header.Result == NRMX_RESULT_LOWER_USN) {

		NDAS_ASSERT( acquireLockReply->Header.RaidInformation == TRUE );

		status = NdasRaidClientProcessNodeChageReply( NdasrClient, (PNRMX_CHANGE_STATUS)acquireLockReply );

		if (status != STATUS_SUCCESS) {

			return status;
		}

		if (NdasrClient->WaitForSync) {

			return STATUS_SYNCHRONIZATION_REQUIRED;
		}

		NDAS_ASSERT(FALSE);
	}
	
	if (acquireLockReply->Header.Result == NRMX_RESULT_REQUIRE_SYNC) {

		NDAS_ASSERT( NdasrClient->WaitForSync == FALSE );

		NdasrClient->WaitForSync = TRUE;

		return STATUS_SYNCHRONIZATION_REQUIRED;
	}

	if (acquireLockReply->Header.Result != NRMX_RESULT_GRANTED) {

		NDAS_ASSERT(FALSE);
		return STATUS_CONNECTION_DISCONNECTED;	
	}

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {
	
	} else {

		result = 0;
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Reading additional message data %d bytes\n", sizeof(NRMX_ACQUIRE_LOCK_REPLY) - sizeof(NRMX_HEADER)) );
		
		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject, 
							   (PUCHAR)(acquireLockReply) + sizeof(NRMX_HEADER),
							   sizeof(NRMX_ACQUIRE_LOCK_REPLY) - sizeof(NRMX_HEADER),
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRMX_ACQUIRE_LOCK_REPLY) - sizeof(NRMX_HEADER)) {

			NDAS_ASSERT(FALSE);
	
			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	lock = NdasRaidClientAllocLock( NTOHLL(acquireLockReply->LockId8), 
									acquireLockReply->LockType, 
									acquireLockReply->LockMode, 
									NTOHLL(acquireLockReply->BlockAddress8),
									NTOHL(acquireLockReply->BlockLength4) );

	if (lock == NULL) {

		NDAS_ASSERT(FALSE);
	
	} else {

		KIRQL	oldIrql;

		KeQueryTickCount( &lock->LastAccesseTick );	

		lock->Status = LOCK_STATUS_GRANTED;
					
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Reply - lock is granted lock id = %I64x, range= %I64x:%x\n", 
					 lock->Id, lock->BlockAddress, lock->BlockLength) );

		KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql ); 
		InsertTailList( &NdasrClient->LockList, &lock->Link );
		KeReleaseSpinLock(&NdasrClient->SpinLock, oldIrql);
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

	NRMX_RELEASE_LOCK	releaseLockRequest;
	PNRMX_HEADER		releaseLockReply;
	UINT32				msgLength;
	LONG				result;
	
	UNREFERENCED_PARAMETER( Timeout );

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );
	NDAS_ASSERT( Timeout == NULL );
	NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending RELEASE_LOCK message lock id = %I64x\n", LockId) );

	msgLength = sizeof(NRMX_RELEASE_LOCK);

	RtlZeroMemory( &releaseLockRequest, msgLength );

	releaseLockRequest.Header.Signature	= NTOHL(NRMX_SIGNATURE);
	releaseLockRequest.Header.Command	= NRMX_CMD_RELEASE_LOCK;
	releaseLockRequest.Header.Length2	= NTOHS((UINT16)msgLength);
	releaseLockRequest.Header.Sequence2	= NTOHS(NdasrClient->RequestSequence);
	releaseLockRequest.Header.Usn4		= NTOHL( NdasrClient->Usn );

	NdasrClient->RequestSequence++;

	releaseLockRequest.LockId8 = HTONLL(LockId);

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {

		NDASR_LOCAL_MSG		ndasrRequestLocalMsg;
		
		PLIST_ENTRY			listEntry;
		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
		
		InitializeListHead( &ndasrRequestLocalMsg.ListEntry );
		ndasrRequestLocalMsg.NrmxHeader = (PNRMX_HEADER)&releaseLockRequest;

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

			NDAS_ASSERT(FALSE);

			return STATUS_CONNECTION_DISCONNECTED;
		}

		KeClearEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent );

		listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
												 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		NDAS_ASSERT( listEntry );

		ndasrReplyLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );
		releaseLockReply = ndasrReplyLocalMsg->NrmxHeader;

		NDAS_ASSERT( ndasrReplyLocalMsg == &NdasrClient->Lurn->NdasrInfo->RequestChannelReply );
		NDAS_ASSERT( releaseLockReply == &NdasrClient->Lurn->NdasrInfo->NrmxHeader );

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

			NDAS_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}

		result = 0;

		releaseLockReply = (PNRMX_HEADER)NdasrClient->RequestConnection.ReceiveBuf;

		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject,
							   (PUCHAR)releaseLockReply,
							   sizeof(NRMX_HEADER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRMX_HEADER)) {

			NDAS_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to received request header\n") );

			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	if (releaseLockReply->Signature != HTONL(NRMX_SIGNATURE) ||
		releaseLockReply->Command != NRMX_CMD_RELEASE_LOCK	 ||
		!releaseLockReply->ReplyFlag) {

		NDAS_ASSERT(FALSE);
	
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (releaseLockReply->Result == NRMX_RESULT_LOWER_USN) {

		NDAS_ASSERT( releaseLockReply->RaidInformation == TRUE );

		status = NdasRaidClientProcessNodeChageReply( NdasrClient, (PNRMX_CHANGE_STATUS)releaseLockReply );

		if (status != STATUS_SUCCESS) {

			return status;
		}

		if (NdasrClient->WaitForSync) {

			return STATUS_SYNCHRONIZATION_REQUIRED;
		}

		NDAS_ASSERT(FALSE);
	}

	if (releaseLockReply->Result != NRMX_RESULT_SUCCESS) {

		NDAS_ASSERT(FALSE);
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

	PNRMX_NODE_CHANGE	nodeChangeRequest = NULL;
	PNRMX_HEADER		nodeChangeReply;
	UINT32				msgLength;
	LONG				result;
	UCHAR				nidx;
	KIRQL				oldIrql;
	
	UNREFERENCED_PARAMETER( Timeout );

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );
	NDAS_ASSERT( Timeout == NULL );
	NDAS_ASSERT( FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending NODE_CHANGE message\n") );

	msgLength = SIZE_OF_NRMX_CHANGE_STATUS(NdasrClient->Lurn->LurnChildrenCnt);

	nodeChangeRequest = ExAllocatePoolWithTag( NonPagedPool, msgLength, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
	RtlZeroMemory( nodeChangeRequest, msgLength );

	nodeChangeRequest->Header.Signature	= NTOHL(NRMX_SIGNATURE);
	nodeChangeRequest->Header.Command	= NRMX_CMD_NODE_CHANGE;
	nodeChangeRequest->Header.Length2	= NTOHS((UINT16)msgLength);
	nodeChangeRequest->Header.Sequence2	= NTOHS(NdasrClient->RequestSequence);
	nodeChangeRequest->Header.Usn4		= NTOHL(NdasrClient->Usn );

	NdasrClient->RequestSequence++;

	nodeChangeRequest->NodeCount = (UCHAR)NdasrClient->Lurn->LurnChildrenCnt;

	for (nidx=0; nidx<nodeChangeRequest->NodeCount; nidx++) {
		
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
		ndasrRequestLocalMsg.NrmxHeader = (PNRMX_HEADER)nodeChangeRequest;

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

			NDAS_ASSERT(FALSE);

			return STATUS_CONNECTION_DISCONNECTED;
		}

		KeClearEvent( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyEvent );

		listEntry = ExInterlockedRemoveHeadList( &NdasrClient->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
												 &NdasrClient->Lurn->NdasrInfo->RequestChannel.SpinLock );

		NDAS_ASSERT( listEntry );

		ndasrReplyLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );
		nodeChangeReply = ndasrReplyLocalMsg->NrmxHeader;

		NDAS_ASSERT( ndasrReplyLocalMsg == &NdasrClient->Lurn->NdasrInfo->RequestChannelReply );
		NDAS_ASSERT( nodeChangeReply == &NdasrClient->Lurn->NdasrInfo->NrmxHeader );

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

			NDAS_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request\n") );

			ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
			return STATUS_CONNECTION_DISCONNECTED;
		}

		result = 0;

		nodeChangeReply = (PNRMX_HEADER)NdasrClient->RequestConnection.ReceiveBuf;

		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject,
							   (PUCHAR)nodeChangeReply,
							   sizeof(NRMX_HEADER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRMX_HEADER)) {

			NDAS_ASSERT( status != STATUS_SUCCESS );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to received request header\n") );

			ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	if (nodeChangeReply->Signature != HTONL(NRMX_SIGNATURE)	||
		nodeChangeReply->Command != NRMX_CMD_NODE_CHANGE	||
		!nodeChangeReply->ReplyFlag) {

		NDAS_ASSERT(FALSE);
	
		ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (nodeChangeReply->Result == NRMX_RESULT_LOWER_USN) {

		NDAS_ASSERT( nodeChangeReply->RaidInformation == TRUE );

		status = NdasRaidClientProcessNodeChageReply( NdasrClient, (PNRMX_CHANGE_STATUS)nodeChangeReply );

		if (status != STATUS_SUCCESS) {

			return status;
		}

		if (NdasrClient->WaitForSync) {

			ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
			return STATUS_SYNCHRONIZATION_REQUIRED;
		}

		NDAS_ASSERT(FALSE);
	}

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	if (nodeChangeReply->Result == NRMX_RESULT_REQUIRE_SYNC) {

		NdasrClient->WaitForSync = TRUE;
		
	} else if (nodeChangeReply->Result == NRMX_RESULT_NO_CHANGE) {
		
		NdasrClient->WaitForSync = FALSE;

	} else {

		NDAS_ASSERT(FALSE);
	}

	for (nidx=0; nidx<NdasrClient->Lurn->LurnChildrenCnt; nidx++) {
			
		NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[nidx] &= ~NDASR_NODE_CHANGE_FLAG_UPDATING;
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );				

	ExFreePoolWithTag( nodeChangeRequest, NDASR_CLIENT_REQUEST_MSG_POOL_TAG );
	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidClientProcessNodeChageReply ( 
	PNDASR_CLIENT		NdasrClient,
	PNRMX_CHANGE_STATUS	ChangeStatusMsg
	)
{
	NTSTATUS	status;

	ULONG		msgLength;
	LONG		result;

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	if (LUR_IS_PRIMARY(NdasrClient->Lurn->Lur) ||
		FlagOn(NdasrClient->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE)) {
	
	} else {

		msgLength = NTOHS( ChangeStatusMsg->Header.Length2 );
	
		if (msgLength > NRMX_MAX_REQUEST_SIZE || msgLength < sizeof(NRMX_HEADER)) {

			NDAS_ASSERT(FALSE);
			return STATUS_CONNECTION_DISCONNECTED;
		}

		NDAS_ASSERT( msgLength != sizeof(NRMX_HEADER) );
	
		result = 0;
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reading additional message data %d bytes\n", msgLength - sizeof(NRMX_HEADER)) );
		
		status = LpxTdiV2Recv( NdasrClient->RequestConnection.ConnectionFileObject, 
							   (PUCHAR)(ChangeStatusMsg) + sizeof(NRMX_HEADER),
							   msgLength - sizeof(NRMX_HEADER),
							   0, 
							   NULL, 
							   NULL,
							   0,
							   &result );

		if (result != msgLength - sizeof(NRMX_HEADER)) {

			NDAS_ASSERT(FALSE);
	
			return STATUS_CONNECTION_DISCONNECTED;
		}
	}

	return NdasRaidClientCheckChangeStatusMessage( NdasrClient, ChangeStatusMsg );
}

NTSTATUS
NdasRaidClientCheckChangeStatusMessage (
	PNDASR_CLIENT		NdasrClient,
	PNRMX_CHANGE_STATUS	ChangeStatusMsg
	)
{
	KIRQL	oldIrql;
	UCHAR	nidx;

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	if (ChangeStatusMsg->NodeCount != NdasrClient->Lurn->LurnChildrenCnt) {

		NDAS_ASSERT(FALSE);
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (NTOHL(ChangeStatusMsg->Usn4) != NdasrClient->Usn) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("USN changed. Reload RMD (not needed until online unit change is implemented)\n") );
	}

	KeAcquireSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, &oldIrql );
	KeAcquireSpinLockAtDpcLevel( &NdasrClient->SpinLock );

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
	NdasrClient->Usn				= HTONL(ChangeStatusMsg->Usn4);

	for (nidx = 0; nidx < ChangeStatusMsg->NodeCount; nidx++) {
			
		if (NdasrClient->NdasrNodeFlags[nidx] != ChangeStatusMsg->Node[nidx].NodeFlags) {
					
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Changing node %d flag from %x to %x!!!\n", 
						nidx, NdasrClient->NdasrNodeFlags[nidx] ,ChangeStatusMsg->Node[nidx].NodeFlags) );
		}

		NdasrClient->NdasrNodeFlags[nidx] = ChangeStatusMsg->Node[nidx].NodeFlags;
				
		if (FlagOn(NdasrClient->NdasrNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {
			
			if (!FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Setting defective flag to node %d %d!!!\n", 
							nidx, NdasrClient->NdasrNodeFlags[nidx] ,ChangeStatusMsg->Node[nidx].NodeFlags) );
						
				SetFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE );
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

	if (ChangeStatusMsg->WaitForSync) {
				
		NdasrClient->WaitForSync = TRUE;

	} else {
				
		NdasrClient->WaitForSync = FALSE;
	}

	KeReleaseSpinLockFromDpcLevel( &NdasrClient->SpinLock );
	KeReleaseSpinLock( &NdasrClient->Lurn->NdasrInfo->SpinLock, oldIrql );

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
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Receive result=%x\n",Connection->ReceiveOverlapped.Request[0].IoStatusBlock.Information) );

			status = STATUS_CONNECTION_DISCONNECTED;

		} else if (Connection->ReceiveOverlapped.Request[0].IoStatusBlock.Information != sizeof(NRMX_HEADER)) {

			NDAS_ASSERT(FALSE);

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
	PNRMX_HEADER	message;
	ULONG			msgLength;

	LONG			result;
	ULONG			addtionalLength;


	UNREFERENCED_PARAMETER( NdasrClient );
	
	// Read remaining data if needed.

	message = (PNRMX_HEADER) Connection->ReceiveBuf;
	msgLength = NTOHS(message->Length2);
	
	if (msgLength > NRMX_MAX_REQUEST_SIZE || msgLength < sizeof(NRMX_HEADER)) {

		NDAS_ASSERT(FALSE);
		return STATUS_CONNECTION_DISCONNECTED;
	}

	if (msgLength == sizeof(NRMX_HEADER)) {

		return STATUS_SUCCESS;
	}

	result = 0;
	addtionalLength = msgLength - sizeof(NRMX_HEADER);
		
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reading additional message data %d bytes\n", addtionalLength) );
		
	status = LpxTdiV2Recv( Connection->ConnectionFileObject, 
						   (PUCHAR)(Connection->ReceiveBuf + sizeof(NRMX_HEADER)),
						   addtionalLength,
						   0, 
						   NULL, 
						   NULL,
						   0,
						   &result );

	if (result != addtionalLength) {

		NDAS_ASSERT(FALSE);
		return STATUS_CONNECTION_DISCONNECTED;
	}

	return status;
}

NTSTATUS
NdasRaidClientHandleNotificationMsg (
	PNDASR_CLIENT	NdasrClient,
	PNRMX_HEADER	Message,
	PBOOLEAN		RetireMessage
	) 
{
	NTSTATUS			status;
	KIRQL				oldIrql;
	UCHAR				resultCode = NRMX_RESULT_SUCCESS;
	BOOLEAN				retireMessage = FALSE;

	NDAS_ASSERT( NdasrClient->Lurn->LockingTime.QuadPart == 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Client handling message %s\n", NdasRixGetCmdString(Message->Command)) );
	
	// Check data validity.

	if (NTOHL(Message->Signature) != NRMX_SIGNATURE || Message->ReplyFlag != 0) {

		NDAS_ASSERT(FALSE);

		if (RetireMessage) {
			
			*RetireMessage = retireMessage;
		}

		return STATUS_CONNECTION_DISCONNECTED;
	}

	switch (Message->Command) {

	case NRMX_CMD_RETIRE: {

		retireMessage = TRUE;

		// Enter transition mode and release all locks.
		
		KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

		NdasrClient->WaitForSync = TRUE;
		
		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );	

		break;
	}

	case NRMX_CMD_CHANGE_STATUS: {

		PNRMX_CHANGE_STATUS changeStatusMsg = (PNRMX_CHANGE_STATUS) Message;
	
		status = NdasRaidClientCheckChangeStatusMessage( NdasrClient, changeStatusMsg );

		break;
	}
	
	case NRMX_CMD_STATUS_SYNCED: { // RAID/Node status is synced. Continue IO

		KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );
		
		NdasrClient->WaitForSync = FALSE;

		KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
	
		status = STATUS_SUCCESS;
		break;
	}

	default:

		NDAS_ASSERT(FALSE);

		status = STATUS_CONNECTION_DISCONNECTED;
	}

	// must send release lock

	if (Message->Command == NRMX_CMD_RETIRE) {

		status = NdasRaidClientFlush( NdasrClient );
	}

	// must send node change message

	if (Message->Command == NRMX_CMD_CHANGE_STATUS) {

		status = NdasRaidClientSendRequestNodeChange( NdasrClient, NULL );
	}

	if (Message->Command == NRMX_CMD_STATUS_SYNCED) {

		LURN_EVENT lurnEvent;

		status = NdasRaidClientFlush(NdasrClient);

		// not obligation, but recommended
		// Set a event to make NDASSCSI fire NoOperation to trigger ndasscsi status change alarm.

		lurnEvent.LurnId = NdasrClient->Lurn->LurnId;
		lurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;

		LurCallBack( NdasrClient->Lurn->Lur, &lurnEvent );	
	}

	if (RetireMessage) {

		*RetireMessage = retireMessage;
	}

	return status;
}

VOID
NdasRaidClientUpdateLocalNodeFlags (
	PNDASR_CLIENT		NdasrClient,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,		// Temp parameter.. set though lurn node info.
	UCHAR 				NewDefectCodes  // Temp parameter.. set though lurn node info.
	) 
{
	KIRQL oldIrql;
	UCHAR newLocalNodeFlags;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Enter\n") );

	// Defective flag should not be not cleared

	newLocalNodeFlags = NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx];

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

	switch (ChildLurn->LurnStatus) {

	case LURN_STATUS_RUNNING:
	case LURN_STATUS_STALL:
		
		// Defective flag should not be not cleared

		ClearFlag( newLocalNodeFlags, (NRMX_NODE_FLAG_STOP | NRMX_NODE_FLAG_UNKNOWN) );
		SetFlag( newLocalNodeFlags, NRMX_NODE_FLAG_RUNNING );
		
		NDAS_ASSERT( !FlagOn(newLocalNodeFlags, NRMX_NODE_FLAG_DEFECTIVE) );
		break;
	
	case LURN_STATUS_STOP_PENDING:
	case LURN_STATUS_STOP:
	case LURN_STATUS_DESTROYING:
		
		// Defective flag should not be not cleared

		ClearFlag( newLocalNodeFlags, (NRMX_NODE_FLAG_RUNNING | NRMX_NODE_FLAG_UNKNOWN) );
		SetFlag( newLocalNodeFlags, NRMX_NODE_FLAG_STOP );

		break;
	
	case LURN_STATUS_INIT:

		//NDAS_ASSERT(FALSE);

		// Defective flag should not be not cleared

		ClearFlag( newLocalNodeFlags, (NRMX_NODE_FLAG_RUNNING | NRMX_NODE_FLAG_STOP) );
		SetFlag( newLocalNodeFlags, NRMX_NODE_FLAG_STOP );

		break;

	default:

		NDAS_ASSERT(FALSE);

		break;
	}

	// Check bad sector or disk is defected. - to do: get defective info in cleaner way..
	
	if (LurnGetCauseOfFault(ChildLurn) & (LURN_FCAUSE_BAD_SECTOR | LURN_FCAUSE_BAD_DISK)) {
		
		SetFlag( newLocalNodeFlags, NRMX_NODE_FLAG_DEFECTIVE );
		
		NewDefectCodes = (LurnGetCauseOfFault(ChildLurn) & LURN_FCAUSE_BAD_SECTOR) ? 
							NRMX_NODE_DEFECT_BAD_SECTOR : NRMX_NODE_DEFECT_BAD_DISK;
	}

	SetFlag( newLocalNodeFlags, FlagsToAdd );

	NDAS_ASSERT( newLocalNodeFlags == NRMX_NODE_FLAG_RUNNING	||
				 newLocalNodeFlags == NRMX_NODE_FLAG_STOP		||
				 newLocalNodeFlags == (NRMX_NODE_FLAG_RUNNING | NRMX_NODE_FLAG_DEFECTIVE) );

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
			
			NDAS_ASSERT(FALSE);
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is updating\n") );
		
		} else if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is changed\n") );
		
		} else {

			SetFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		}

	} else if (NdasrClient->Lurn->NdasrInfo->LocalNodeFlags[ChildLurn->LurnChildIdx] != NdasrClient->NdasrNodeFlags[ChildLurn->LurnChildIdx]) {

		if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_UPDATING)) {

			NDAS_ASSERT(FALSE);
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is updating\n") );
		
		} else if (FlagOn(NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Set node changed flag while node is changed\n") );
		
		} else {

			SetFlag( NdasrClient->Lurn->NdasrInfo->LocalNodeChanged[ChildLurn->LurnChildIdx], NDASR_NODE_CHANGE_FLAG_CHANGED );
		}
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );
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

	NDAS_ASSERT( Length > 0 && Length <= 2048 );

	Addr += NdasrClient->Lurn->Lur->StartOffset;

	if (NdasrClient->Lurn->NdasrInfo->ParityDiskCount == 0 || NdasrClient->EmergencyMode == TRUE) {

		return STATUS_SUCCESS;
	}

	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

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
			NDAS_ASSERT( fullOverlap->Status == LOCK_STATUS_GRANTED );

			break;
		}
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

	if (fullOverlap) {

		InterlockedIncrement( &fullOverlap->InUseCount );
		KeQueryTickCount( &fullOverlap->LastAccesseTick );

		return STATUS_SUCCESS;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("No full match. Addr %I64x, Length %x\n", Addr, Length) );	
	
	// No full match. Need to add up partial matched.
	// Pass2. Bitmap search.

	bitmapBitCount = Length;
	
	bitmapUlongCount = ((bitmapBitCount+31)/32); //RtlInitializeBitMap accept multiple of 32bit only.
	
	RtlInitializeBitMap( &bitmap, bitmapBuf, bitmapUlongCount*32 );
	RtlClearAllBits( &bitmap );
	
	if (bitmapBitCount % 32 != 0) {

		RtlSetBits( &bitmap, bitmapBitCount, bitmapUlongCount*32 - bitmapBitCount );
	}
	
	KeAcquireSpinLock( &NdasrClient->SpinLock, &oldIrql );

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

			NDAS_ASSERT( startIndex >= 0 );
			NDAS_ASSERT( numberToSet > 0 );
			NDAS_ASSERT( startIndex + numberToSet <= (LONG)Length );

			RtlSetBits( &bitmap, startIndex, numberToSet );

			NDAS_ASSERT( lock->Status == LOCK_STATUS_GRANTED );
		}
	}	

	if (RtlAreBitsSet(&bitmap, 0, Length)) {
	
		// All bit is set. Lock is covered
		 
		DebugTrace( NDASSCSI_DBG_LURN_IDE_NOISE, 
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
					
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, 
							("Partial match with lock %I64x:%x\n", lock->BlockAddress, lock->BlockLength) );
			}
		}

	} else {
		
		ULONG startIndex = 0;
		ULONG numberOfClear;
		
		// Find clear bits

		numberOfClear = RtlFindFirstRunClear( &bitmap, &startIndex );

		NDAS_ASSERT( numberOfClear > 0 && numberOfClear <= Length );
		
		// Currently return only one range. If multiple range is missing, multiple lock_acquire need to be sent.
		// to do: return all unlocked range.
		
		*UnauthAddr		= startIndex + Addr;
		*UnauthLength	= numberOfClear;

		NDAS_ASSERT( Addr <= *UnauthAddr && (*UnauthAddr + *UnauthLength) <= (Addr + Length) );

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

			NDAS_ASSERT( overlapStatus == NDASR_RANGE_NO_OVERLAP );
		}

#endif

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Lock range %I64x:%x is not covered.\n", *UnauthAddr, *UnauthLength) );
		
		status = STATUS_UNSUCCESSFUL;
	}

	KeReleaseSpinLock( &NdasrClient->SpinLock, oldIrql );

	return status;
}
