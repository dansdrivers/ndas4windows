#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasraidArbitrator"

VOID
NdasRaidArbitratorThreadProc (
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	);

NTSTATUS
NdasRaidArbitratorAcceptClient (
	PNDASR_ARBITRATOR				NdasArbitrator,
	PNRMX_REGISTER					RegisterMsg,
	PNDASR_ARBITRATOR_CONNECTION	*Connection
	);

NTSTATUS
NdasRaidArbitratorRegisterNewClient ( 
	IN PNDASR_ARBITRATOR NdasrArbitrator
	);

NTSTATUS
NdasRaidArbitratorCheckRequestMsg (
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	); 

NTSTATUS
NdasRaidArbitratorHandleRequestMsg (
	PNDASR_ARBITRATOR		NdasrArbitrator, 
	PNDASR_CLIENT_CONTEXT	Client,
	PNRMX_HEADER			Message
	);

NTSTATUS
NdasRaidReplyChangeStatus (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PNRMX_HEADER			RequestMsg,
	UCHAR					Result
	);

PNDASR_CLIENT_CONTEXT 
NdasRaidArbitratorAllocClientContext (
	VOID
	);

VOID
NdasRaidArbitratorTerminateClient (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	Client,
	PBOOLEAN				OosBitmapSet
	); 

NTSTATUS 
NdasRaidArbitratorInitializeOosBitmap (
	IN PNDASR_ARBITRATOR	NdasrArbitrator,
	IN PBOOLEAN				NodeIsUptoDate,
	IN UCHAR				UpToDateNode
	); 

VOID
NdasRaidArbitratorChangeOosBitmapBit (
	PNDASR_ARBITRATOR	NdasrArbitrator,
	BOOLEAN				Set,	// TRUE for set, FALSE for clear
	UINT64				Addr,
	UINT64				Length
	);

VOID
NdasRaidArbitratorUpdateLwrBitmapBit (
	PNDASR_ARBITRATOR				NdasrArbitrator,
	PNDASR_ARBITRATOR_LOCK_CONTEXT	HintAddedLock,
	PNDASR_ARBITRATOR_LOCK_CONTEXT	HintRemovedLock
	); 

NTSTATUS 
NdasRaidArbitratorUpdateOnDiskOosBitmap (
	PNDASR_ARBITRATOR	NdasrArbitrator,
	BOOLEAN				UpdateAll
	); 

BOOLEAN
NdasRaidArbitratorUpdateInCoreRmd (
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	); 

NTSTATUS
NdasRaidArbitratorWriteRmd (
	IN PNDASR_ARBITRATOR	 Arbitrator,
	OUT PNDAS_RAID_META_DATA Rmd
	);

NTSTATUS
NdasRaidArbitratorWriteMetaSync (
	IN PNDASR_ARBITRATOR	NdasrArbitrator,
	IN PUCHAR				BlockBuffer,
	IN INT64				BlockAddr,
	IN UINT32				TrasferBlocks, 
	IN BOOLEAN				RelativeAddress
	); 

NTSTATUS
NdasRaidNotifyChangeStatus (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext
	);

NTSTATUS
NdasRaidNotifyRetire (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext
	);

NTSTATUS
NdasRaidArbitratorArrangeLockRange (
	PNDASR_ARBITRATOR				NdasrArbitrator,
	PNDASR_ARBITRATOR_LOCK_CONTEXT Lock,
	UINT32							Granularity
	); 

BOOLEAN
NdasRaidArbitratorRefreshRaidStatus (
	IN PNDASR_ARBITRATOR	NdasrArbitrator,
	IN BOOLEAN				ForceChange
	);

NTSTATUS
NdasRaidArbitratorUseSpareIfNeeded (
	IN  PNDASR_ARBITRATOR	NdasrArbitrator,
	OUT PBOOLEAN			SpareUsed
	);

NTSTATUS
NdasRaidArbitratorFlushDirtyCacheNdas1_0 (
	IN PNDASR_ARBITRATOR		NdasrArbitrator,
	IN UINT64					LockId,
	IN PNDASR_CLIENT_CONTEXT	ClientContext
	); 

NTSTATUS 
NdasRaidRebuildInitiate (
	PNDASR_ARBITRATOR NdasrArbitrator
	); 

NTSTATUS
NdasRaidRebuildAcknowledge (
	PNDASR_ARBITRATOR NdasrArbitrator
	); 

NTSTATUS
NdasRaidRebuildStart (
	PNDASR_ARBITRATOR NdasrArbitrator
	);

NTSTATUS
NdasRaidRebuildStop (
	PNDASR_ARBITRATOR NdasrArbitrator
	);

VOID
NdasRaidRebuildThreadProc (
	IN PNDASR_REBUILDER	NdasrRebuilder
	);

NTSTATUS
NdasRaidRebuildRequest (
	IN PNDASR_REBUILDER	NdasrRebuilder,
	IN UINT64			BlockAddr,
	IN UINT32			BlockLength
	);

NTSTATUS 
NdasRaidRebuildDoIo (
	PNDASR_REBUILDER	NdasrRebuilder,
	UINT64				BlockAddr,
	UINT32 				BlockLength
	); 


PNDASR_ARBITRATOR_LOCK_CONTEXT
NdasRaidArbitratorAllocLock (
	PNDASR_ARBITRATOR	NdasrArbitrator,
	UCHAR				LockType,
	UCHAR 				LockMode,
	UINT64				Addr,
	UINT32				Length
	) 
{
	PNDASR_ARBITRATOR_LOCK_CONTEXT lock;
	KIRQL							oldIrql;

	lock = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_ARBITRATOR_LOCK_CONTEXT), NDASR_ARBITRATOR_LOCK_POOL_TAG );
	
	if (lock == NULL) {
	
		NDAS_ASSERT( FALSE );
		return NULL;
	}

	RtlZeroMemory( lock, sizeof(NDASR_ARBITRATOR_LOCK_CONTEXT) );

	lock->Status		= NDASR_ARBITRATOR_LOCK_STATUS_NONE;
	lock->Type			= LockType;
	lock->Mode			= LockMode;
	lock->BlockAddress	= Addr;	
	lock->BlockLength	= Length;
	lock->Granularity	= NdasrArbitrator->LockRangeGranularity;
	
	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	
	lock->Id = NdasrArbitrator->NextLockId;
	NdasrArbitrator->NextLockId++;	
	
	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );		

	InitializeListHead( &lock->ClientAcquiredLink );
	InitializeListHead( &lock->ArbitratorAcquiredLink );
	
	lock->Owner = NULL;

	return lock;
}

VOID
NdasRaidArbitratorFreeLock (
	PNDASR_ARBITRATOR				NdasrArbitrator,
	PNDASR_ARBITRATOR_LOCK_CONTEXT	Lock
	) 
{
	UNREFERENCED_PARAMETER( NdasrArbitrator );

	NDAS_ASSERT( IsListEmpty(&Lock->ClientAcquiredLink) );
	NDAS_ASSERT( IsListEmpty(&Lock->ArbitratorAcquiredLink) );

	NDAS_ASSERT( Lock->Owner == NULL );

	ExFreePoolWithTag( Lock, NDASR_ARBITRATOR_LOCK_POOL_TAG );
}

NTSTATUS 
NdasRaidArbitratorStart (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status;

	PNDASR_INFO			ndasrInfo = Lurn->NdasrInfo;
	PNDASR_ARBITRATOR	ndasrArbitrator;
	KIRQL				oldIrql, oldIrql2;
	UCHAR				nidx, ridx;
	OBJECT_ATTRIBUTES	objectAttributes;
	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasRaidArbitratorStart\n") );

	if (ndasrInfo->NdasrArbitrator) {
	
		NDAS_ASSERT( FALSE );
		return STATUS_SUCCESS;
	}
	
	ndasrInfo->NdasrArbitrator = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_ARBITRATOR), NDASR_ARBITRATOR_POOL_TAG );

	if (ndasrInfo->NdasrArbitrator == NULL) {
		
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ndasrArbitrator = ndasrInfo->NdasrArbitrator;

	RtlZeroMemory( ndasrArbitrator, sizeof(NDASR_ARBITRATOR) );

	InitializeListHead( &ndasrArbitrator->AllListEntry );

	ndasrArbitrator->Lurn = Lurn;

	KeInitializeSpinLock( &ndasrArbitrator->SpinLock );

	ndasrArbitrator->Status = NDASR_ARBITRATOR_STATUS_INITIALIZING;

	KeInitializeEvent( &ndasrArbitrator->FinishShutdownEvent, NotificationEvent, FALSE );

	ndasrArbitrator->NdasrState = NRMX_RAID_STATE_INITIALIZING;

	InitializeListHead( &ndasrArbitrator->NewClientQueue );
	InitializeListHead( &ndasrArbitrator->ClientQueue );
	InitializeListHead( &ndasrArbitrator->TerminatedClientQueue );

	InitializeListHead( &ndasrArbitrator->AcquiredLockList );	
	ndasrArbitrator->AcquiredLockCount	= 0;

	ndasrArbitrator->LockRangeGranularity = ndasrInfo->BlocksPerBit; // Set to sector per bit for time being..

	ndasrArbitrator->OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;
	RtlCopyMemory( &ndasrArbitrator->ConfigSetId, &ndasrArbitrator->Lurn->NdasrInfo->ConfigSetId, sizeof(GUID) );	

	ndasrArbitrator->AcceptClient = NdasRaidArbitratorAcceptClient;

	do {
	
		// 1. Set initial ndasrArbitrator flags. 
	 
		ACQUIRE_SPIN_LOCK( &ndasrArbitrator->SpinLock, &oldIrql );	
	
		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {

			ACQUIRE_SPIN_LOCK( &Lurn->LurnChildren[nidx]->SpinLock, &oldIrql2 );

			ndasrArbitrator->NodeFlags[nidx] = NdasRaidLurnStatusToNodeFlag( Lurn->LurnChildren[nidx]->LurnStatus );

			if (LurnGetCauseOfFault(Lurn->LurnChildren[nidx]) & (LURN_FCAUSE_BAD_SECTOR | LURN_FCAUSE_BAD_DISK)) {

				ndasrArbitrator->NodeFlags[nidx] = NRMX_NODE_FLAG_DEFECTIVE;
				ndasrArbitrator->DefectCodes[nidx] = ((LurnGetCauseOfFault(Lurn->LurnChildren[nidx]) & LURN_FCAUSE_BAD_SECTOR) ?
												NRMX_NODE_DEFECT_BAD_SECTOR : NRMX_NODE_DEFECT_BAD_DISK );
			}

			DebugTrace(NDASSCSI_DBG_LURN_NDASR_ERROR, ("Setting initial node %d flag: %d\n", nidx, ndasrArbitrator->NodeFlags[nidx]) );

			RELEASE_SPIN_LOCK( &Lurn->LurnChildren[nidx]->SpinLock, oldIrql2 );
		}

		RELEASE_SPIN_LOCK( &ndasrArbitrator->SpinLock, oldIrql );

		// 2. Map children based on RMD

		ACQUIRE_SPIN_LOCK( &ndasrArbitrator->SpinLock, &oldIrql );	

		for (ridx = 0; ridx < Lurn->LurnChildrenCnt; ridx++) { 

			NDAS_ASSERT( ndasrArbitrator->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx < Lurn->LurnChildrenCnt );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("MAPPING Lurn node %d to RAID role %d\n", ndasrArbitrator->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx, ridx) );

			ndasrArbitrator->RoleToNodeMap[ridx] = (UCHAR)ndasrArbitrator->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx;
			ndasrArbitrator->NodeToRoleMap[ndasrArbitrator->RoleToNodeMap[ridx]] = (UCHAR)ridx;	
		}

		// 3. Apply node information from RMD
		
		for (ridx = 0; ridx < Lurn->LurnChildrenCnt; ridx++) { // i : role index. 
		
			UCHAR unitDeviceStatus = ndasrArbitrator->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus;
		
			if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

				NDAS_ASSERT( ndasrArbitrator->Lurn->NdasrInfo->NodeIsUptoDate[ndasrArbitrator->RoleToNodeMap[ridx]] == FALSE );
				if (ridx < ndasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount) {

					NDAS_ASSERT( !FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) );
					NDAS_ASSERT( ndasrArbitrator->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE );

					ndasrArbitrator->OutOfSyncRoleIndex = (UCHAR)ridx;

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is out-of-sync\n",  ndasrArbitrator->RoleToNodeMap[ridx], ridx) );
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting out of sync role: %d\n", ndasrArbitrator->OutOfSyncRoleIndex) );
				}
			}

			if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

				NDAS_ASSERT( ndasrArbitrator->Lurn->NdasrInfo->NodeIsUptoDate[ndasrArbitrator->RoleToNodeMap[ridx]] == FALSE );
				NDAS_ASSERT( ridx >= ndasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount );

				SetFlag( ndasrArbitrator->NodeFlags[ndasrArbitrator->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_DEFECTIVE );
		
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is defective\n",  ndasrArbitrator->RoleToNodeMap[ridx], ridx) );
			
				SetFlag( ndasrArbitrator->DefectCodes[ndasrArbitrator->RoleToNodeMap[ridx]], 
						 NdasRaidRmdUnitStatusToDefectCode(unitDeviceStatus) );
			}

			//NDAS_ASSERT( nodeIsUptoDate[ndasrArbitrator->RoleToNodeMap[ridx]] == TRUE );
		}

		RELEASE_SPIN_LOCK( &ndasrArbitrator->SpinLock, oldIrql );

		// 4. Read bitmap.

		status = NdasRaidArbitratorInitializeOosBitmap( ndasrArbitrator, 
													 ndasrArbitrator->Lurn->NdasrInfo->NodeIsUptoDate, 
													 ndasrArbitrator->Lurn->NdasrInfo->UpToDateNode );
		if (status != STATUS_SUCCESS) {

			break;
		}

		// 5. Set initial RAID status.
	
		if (NdasRaidArbitratorRefreshRaidStatus(ndasrArbitrator, TRUE) == FALSE) {
			
			//NDAS_ASSERT( FALSE );
		}
	
		// 6. Create Arbitrator thread

		KeInitializeEvent( &ndasrArbitrator->ThreadReadyEvent, NotificationEvent, FALSE );
		KeInitializeEvent( &ndasrArbitrator->ThreadEvent, NotificationEvent, FALSE );
		
		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );
	
		status = PsCreateSystemThread( &ndasrArbitrator->ThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   NdasRaidArbitratorThreadProc,
									   ndasrArbitrator );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );
			break;
		}

		status = ObReferenceObjectByHandle( ndasrArbitrator->ThreadHandle,
											GENERIC_ALL,
											NULL,
											KernelMode,
											&ndasrArbitrator->ThreadObject,
											NULL );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );
			break;
		}

		status = KeWaitForSingleObject( &ndasrArbitrator->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );


	} while (0);

	if (status == STATUS_SUCCESS) {

		NDAS_ASSERT( ndasrArbitrator->ThreadHandle );
		return STATUS_SUCCESS;
	}

	NDAS_ASSERT( FALSE );

	ACQUIRE_SPIN_LOCK( &ndasrArbitrator->SpinLock, &oldIrql );

	ndasrArbitrator->ArbitratorState0 = NDASR_ARBITRATOR_STATUS_TERMINATING;
	ndasrArbitrator->NdasrState = NRMX_RAID_STATE_FAILED;

	RELEASE_SPIN_LOCK( &ndasrArbitrator->SpinLock, oldIrql );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("!!! Arbitrator failed to start\n") );

	if (ndasrInfo->NdasrArbitrator) {
		
		ExFreePoolWithTag( ndasrInfo->NdasrArbitrator, NDASR_ARBITRATOR_POOL_TAG );
		ndasrInfo->NdasrArbitrator = NULL;
	}

	return status;
}

NTSTATUS
NdasRaidArbitratorStop (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS						status;

	KIRQL							oldIrql;
	PLIST_ENTRY						listEntry;
	PNDASR_INFO						ndasrInfo = Lurn->NdasrInfo;
	PNDASR_ARBITRATOR				ndasrArbitrator = ndasrInfo->NdasrArbitrator;
	PNDASR_ARBITRATOR_LOCK_CONTEXT	lock;
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Stopping DRAID arbiter\n") );

	NdasRaidUnregisterArbitrator( &NdasrGlobalData, ndasrArbitrator );
	
	if (ndasrArbitrator->ThreadHandle) {

		ACQUIRE_SPIN_LOCK( &ndasrArbitrator->SpinLock, &oldIrql );

		ndasrArbitrator->RequestToTerminate = TRUE;
		KeSetEvent( &ndasrArbitrator->ThreadEvent,IO_NO_INCREMENT, FALSE ); // This will wake up Arbitrator thread.
		
		RELEASE_SPIN_LOCK( &ndasrArbitrator->SpinLock, oldIrql );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Wait for Arbitrator thread completion\n") );

		status = KeWaitForSingleObject( ndasrArbitrator->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			ASSERT(FALSE);
		
		} else {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbitrator thread exited\n") );
		}

		//	Dereference the thread object.

		ObDereferenceObject( ndasrArbitrator->ThreadObject );
		ZwClose( ndasrArbitrator->ThreadHandle );

		ACQUIRE_SPIN_LOCK( &ndasrArbitrator->SpinLock, &oldIrql );

		ndasrArbitrator->ThreadObject = NULL;
		ndasrArbitrator->ThreadHandle = NULL;

		RELEASE_SPIN_LOCK( &ndasrArbitrator->SpinLock, oldIrql );
	}

	while (TRUE) {

		listEntry = RemoveHeadList(&ndasrArbitrator->AcquiredLockList);
		
		if (listEntry == &ndasrArbitrator->AcquiredLockList) {
		
			break;
		}
		
		lock = CONTAINING_RECORD( listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ArbitratorAcquiredLink );
		InterlockedDecrement( &ndasrArbitrator->AcquiredLockCount );

		NdasRaidArbitratorFreeLock( ndasrArbitrator, lock );
	}

	//ASSERT(InterlockedCompareExchange(&ndasrArbitrator->AcquiredLockCount, 0, 0)==0);

	if (ndasrArbitrator->AcquiredLockCount) {

		DbgPrint( "BUG BUG BUG ndasrArbitrator->AcquiredLockCount = %d\n", ndasrArbitrator->AcquiredLockCount );
	}

	if (ndasrArbitrator->Lurn->NdasrInfo->ParityDiskCount != 0) {

		ExFreePoolWithTag( ndasrArbitrator->OosBmpBuffer, NDASR_BITMAP_POOL_TAG );
		ExFreePoolWithTag( ndasrArbitrator->LwrBmpBuffer, NDASR_BITMAP_POOL_TAG );	
		ExFreePoolWithTag( ndasrArbitrator->DirtyBmpSector, NDASR_BITMAP_POOL_TAG );
		ExFreePoolWithTag( ndasrArbitrator->OosBmpOnDiskBuffer, NDASR_BITMAP_POOL_TAG );
	}

	ExFreePoolWithTag( ndasrInfo->NdasrArbitrator, NDASR_ARBITRATOR_POOL_TAG );

	ndasrInfo->NdasrArbitrator = NULL;

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidArbitratorShutdown (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS		status = STATUS_SUCCESS;
	KIRQL			oldIrql;
	
	PNDASR_ARBITRATOR	ndasrArbitrator = Lurn->NdasrInfo->NdasrArbitrator;

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	NDAS_ASSERT( ndasrArbitrator );
	NDAS_ASSERT( ndasrArbitrator->ThreadHandle );

	ACQUIRE_SPIN_LOCK( &ndasrArbitrator->SpinLock, &oldIrql );

	ndasrArbitrator->RequestToShutdown = TRUE;
	KeSetEvent( &ndasrArbitrator->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &ndasrArbitrator->SpinLock, oldIrql );

	status = KeWaitForSingleObject( &ndasrArbitrator->FinishShutdownEvent,
									Executive,
									KernelMode,
									FALSE,
									NULL );

	if (status != STATUS_SUCCESS) {
	
		NDAS_ASSERT( FALSE );
		return status;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ndasrArbitrator shutdown completely\n") );

	return status;
}

NTSTATUS
NdasRaidArbitratorAcceptClient (
	PNDASR_ARBITRATOR				NdasrArbitrator,
	PNRMX_REGISTER					RegisterMsg,
	PNDASR_ARBITRATOR_CONNECTION	*Connection
	) 
{
	PLIST_ENTRY				listEntry;
	PNDASR_CLIENT_CONTEXT	clientContext;
	KIRQL					oldIrql;


	(*Connection)->ConnType = RegisterMsg->ConnType;

	if (RegisterMsg->ConnType != NRMX_CONN_TYPE_NOTIFICATION && RegisterMsg->ConnType != NRMX_CONN_TYPE_REQUEST) {

		NDAS_ASSERT( FALSE );
		
		return STATUS_UNSUCCESSFUL;
	}
	
	// Find client with this address, if not found, create it.

	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

	clientContext = NULL;

	for (listEntry = NdasrArbitrator->NewClientQueue.Flink;
		 listEntry != &NdasrArbitrator->NewClientQueue;
		 listEntry = listEntry->Flink) {

		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

		if (RtlCompareMemory(clientContext->RemoteClientAddr, (*Connection)->RemoteAddr.Node, 6) == 6) {

			if (clientContext->UnregisterRequest == FALSE) {
				
				break;
			}
		}

		clientContext = NULL;
	}

	// Check whether same connection is already exists.
	
	if (clientContext) {
	
		if (RegisterMsg->ConnType == NRMX_CONN_TYPE_NOTIFICATION) {

			if (clientContext->NotificationConnection) {

				clientContext->UnregisterRequest = TRUE;
				clientContext = NULL;
			}

		} else if (RegisterMsg->ConnType == NRMX_CONN_TYPE_REQUEST) {
			
			if (clientContext->RequestConnection) {
			
				NDAS_ASSERT( FALSE );
				clientContext->UnregisterRequest = TRUE;
				clientContext = NULL;
			}
		} 
	} 

	if (!clientContext) {

		clientContext = NdasRaidArbitratorAllocClientContext();

		if (!clientContext) {

			NDAS_ASSERT( FALSE );
			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );			

			return STATUS_INSUFFICIENT_RESOURCES;
		}

		clientContext->LocalClient = RegisterMsg->LocalClient;
		RtlCopyMemory( clientContext->RemoteClientAddr, (*Connection)->RemoteAddr.Node, 6 );
		InsertTailList( &NdasrArbitrator->NewClientQueue, &clientContext->Link );
	}

	if (RegisterMsg->ConnType == NRMX_CONN_TYPE_NOTIFICATION) {

		clientContext->NotificationConnection = (*Connection);
		RtlCopyMemory( &clientContext->NotificationConnection->RegisterMsg, RegisterMsg, sizeof(NRMX_REGISTER) );

		clientContext->NotificationConnection->NeedReply = TRUE;

	} else if (RegisterMsg->ConnType == NRMX_CONN_TYPE_REQUEST) {

		NDAS_ASSERT( clientContext->NotificationConnection );

		clientContext->RequestConnection = (*Connection);
		RtlCopyMemory( &clientContext->RequestConnection->RegisterMsg, RegisterMsg, sizeof(NRMX_REGISTER) );

		clientContext->RequestConnection->NeedReply = TRUE;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("Accepted client %p %p %02X:%02X:%02X:%02X:%02X:%02X\n", 
				clientContext, Connection, clientContext->RemoteClientAddr[0], clientContext->RemoteClientAddr[1], clientContext->RemoteClientAddr[2],
				clientContext->RemoteClientAddr[3], clientContext->RemoteClientAddr[4], clientContext->RemoteClientAddr[5]) );

	*Connection = NULL;

	// Wakeup arbiter to handle this new client

	KeSetEvent( &NdasrArbitrator->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
	
	return STATUS_SUCCESS;
}

VOID
NdasRaidArbitratorThreadProc (
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	)
{
	NTSTATUS				status;
	KIRQL					oldIrql;

	PLIST_ENTRY				listEntry;
	PNDASR_CLIENT_CONTEXT	clientContext;

	BOOLEAN					spareUsed;

	INT32 					allocEventCount = 0;
	PKEVENT					*events = NULL;
	PKWAIT_BLOCK			waitBlocks = NULL;
	INT32 					eventCount;
	LARGE_INTEGER			timeOut;

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	NDAS_ASSERT( NdasrArbitrator->Lurn->NdasrInfo->ParityDiskCount != 0 );

	NDAS_ASSERT( NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit > 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Arbitrator thread starting\n") );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("ChildCount: %d, DiskCount : %d\n", 
				 NdasrArbitrator->Lurn->LurnChildrenCnt, NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount) );
	
	do {

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		status = NdasRaidRebuildStart( NdasrArbitrator );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );
			break;
		}

		allocEventCount = NdasRaidReallocEventArray( &events, &waitBlocks, allocEventCount );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribiter enter running status\n") );

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

		ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_INITIALIZING );
		NdasrArbitrator->ArbitratorState0 = NDASR_ARBITRATOR_STATUS_START;

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );		

		NdasRaidRegisterArbitrator( &NdasrGlobalData, NdasrArbitrator );

		// Insert LocalClient

		clientContext = NdasRaidArbitratorAllocClientContext();

		if (!clientContext) {

			NDAS_ASSERT( FALSE );

			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		clientContext->LocalClient = TRUE;
		InsertTailList( &NdasrArbitrator->NewClientQueue, &clientContext->Link );

		status = NdasRaidArbitratorRegisterNewClient( NdasrArbitrator );

		NDAS_ASSERT( status == STATUS_SUCCESS );
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );

		KeSetEvent( &NdasrArbitrator->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );
	
		NdasrArbitrator->ArbitratorState0 = NDASR_ARBITRATOR_STATUS_TERMINATED;
		NdasrArbitrator->NdasrState = NRMX_RAID_STATE_FAILED;
	
		PsTerminateSystemThread( STATUS_SUCCESS );
		
		return;
	}

	KeSetEvent( &NdasrArbitrator->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	do {

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		if (events == NULL || waitBlocks == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			break;
		}

		// 9. Handle rebuild IO result
		
		if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD)) {

			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

			if (NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_SUCCESS		||
				NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_FAILED		||
				NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_CANCELLED) {
		
				RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			
				status = NdasRaidRebuildAcknowledge( NdasrArbitrator );

				if (status == STATUS_SUCCESS) {

					if (RtlNumberOfSetBits(&NdasrArbitrator->OosBmpHeader) == 0) {
	
						if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, TRUE) == FALSE) {

							NDAS_ASSERT( FALSE );
						}

						ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE );
					}
				
				} else if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

					if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, FALSE) == FALSE) {

						if (NdasrArbitrator->NdasrState != NRMX_RAID_STATE_DEGRADED) {
						
							NDAS_ASSERT( FALSE );
						}
					}

					ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE );
				}

				KeSetEvent( &NdasrArbitrator->ThreadEvent, IO_NO_INCREMENT, FALSE );

				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

				ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD );
			}

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
		} 

		// 10. check node status

		if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD)) {

			// 10. refresh raid status

			if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE)) {

				if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, FALSE) == FALSE) {

					NDAS_ASSERT( FALSE );
				}

				ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE );
			}
		}

		// 11. check sync state

		if (NdasrArbitrator->SyncState == NDASR_SYNC_REQUIRED) {			

			if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD)) {
			
				NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE) );

				NdasrArbitrator->SyncState = NDASR_SYNC_IN_PROGRESS;

				// Send raid status change to all client

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending NRMX_CMD_CHANGE_STATUS to all client\n") );

				listEntry = NdasrArbitrator->ClientQueue.Flink;
			
				while (listEntry != &NdasrArbitrator->ClientQueue) {
						
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

					listEntry = listEntry->Flink;

					status = NdasRaidNotifyChangeStatus( NdasrArbitrator, clientContext );

					if (status != STATUS_SUCCESS) {

						NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &clientContext->Link );
						
						RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );				
					}
				}
			}
		}

		// 12. terminate client

		if (!IsListEmpty(&NdasrArbitrator->TerminatedClientQueue)) {

			if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE)) {

				BOOLEAN oosBitmapSet;

				if (NdasrArbitrator->SyncState == NDASR_SYNC_IN_PROGRESS) {

					PNDASR_CLIENT_CONTEXT	notSyncedClientContext = NULL;

					NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD) );
		
					for (listEntry = NdasrArbitrator->ClientQueue.Flink;
						 listEntry != &NdasrArbitrator->ClientQueue;
						 listEntry = listEntry->Flink) {
			
						notSyncedClientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
				
						if (notSyncedClientContext->Usn != NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN) {

							break;
						}

						notSyncedClientContext = NULL;
					}

					if (notSyncedClientContext == NULL) {

						PNDASR_CLIENT_CONTEXT	clientContext;

						NdasrArbitrator->SyncState = NDASR_SYNC_DONE;

						listEntry = NdasrArbitrator->ClientQueue.Flink;

						while (listEntry != &NdasrArbitrator->ClientQueue) {
				
							clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
							listEntry = listEntry->Flink;

							status = NdasRaidNotifyChangeStatus( NdasrArbitrator, clientContext );

							if (status != STATUS_SUCCESS) {

								NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

								RemoveEntryList( &clientContext->Link );
								InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &clientContext->Link );

								DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
											("Notify failed during status synched. Restarting.\n") );
							}
						}
					}
				}

				oosBitmapSet = FALSE;

				do {

					listEntry = NdasrArbitrator->TerminatedClientQueue.Flink;
		
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

					RemoveEntryList( &clientContext->Link );
					NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, &oosBitmapSet );
			
				} while (!IsListEmpty(&NdasrArbitrator->TerminatedClientQueue));

				if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, oosBitmapSet) == FALSE) {

					NDAS_ASSERT( oosBitmapSet == FALSE );
				}
			
				ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE );
			}
		}

		// 13. check to be stopped

		if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_TERMINATING)) {

			if (NdasrArbitrator->SyncState == NDASR_SYNC_DONE) {

				if (IsListEmpty(&NdasrArbitrator->AcquiredLockList)) {

					NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD) );
					NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE) );

					NdasRaidRebuildStop( NdasrArbitrator );
					break;
				}
			}
		}

		// 13. set receive   

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

		status = STATUS_SUCCESS;

		listEntry = NdasrArbitrator->ClientQueue.Flink;
		
		while (listEntry != &NdasrArbitrator->ClientQueue) {

			clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

			listEntry = listEntry->Flink;

			if (clientContext->LocalClient == TRUE) {

				ACQUIRE_DPC_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock );

				if (IsListEmpty(&NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.RequestQueue)) {

					KeClearEvent( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.RequestEvent );
				}

				RELEASE_DPC_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock );

				continue;
			}

			NDAS_ASSERT( clientContext->RequestConnection &&
							 clientContext->RequestConnection->ConnectionFileObject );

			if (!LpxTdiV2IsRequestPending(&clientContext->RequestConnection->ReceiveOverlapped, 0)) {
					
				// Receive from request connection if not receiving.
				RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

				status = LpxTdiV2Recv( clientContext->RequestConnection->ConnectionFileObject,
									   clientContext->RequestConnection->ReceiveBuf,
									   sizeof(NRMX_HEADER),
									   0,
									   NULL,
									   &clientContext->RequestConnection->ReceiveOverlapped,
									   0,
									   NULL );


				if (!NT_SUCCESS(status)) {

					LpxTdiV2CompleteRequest( &clientContext->RequestConnection->ReceiveOverlapped, 0 );

					NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );
						
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to start to recv from client. Terminating this client\n") );
							
					ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
					RemoveEntryList( &clientContext->Link );
					RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

					ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

					break;

				}

				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
					
				NDAS_ASSERT( status == STATUS_SUCCESS || status == STATUS_PENDING );
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			
		if (status != STATUS_SUCCESS && status != STATUS_PENDING) {
		
			NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );
			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}

		// 14, check request setting

		if (NdasrArbitrator->SyncState == NDASR_SYNC_DONE) {

			NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE) );

			if (NdasrArbitrator->RequestToTerminate) {

				if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_TERMINATING)) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("SkipSetEvent\n") );
					goto SkipSetEvent;				
				}
			}

			if (NdasrArbitrator->RequestToShutdown) {

				if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN)) {

					goto SkipSetEvent;
				}
			}
		}

		// 1. set event

		eventCount = 0;
		events[eventCount++] = &NdasrArbitrator->ThreadEvent;

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

		status = STATUS_SUCCESS;

		listEntry = NdasrArbitrator->ClientQueue.Flink;
		
		while (listEntry != &NdasrArbitrator->ClientQueue) {

			if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE)) {

				NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD) );
				break;
			}

			clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

			listEntry = listEntry->Flink;

			if (allocEventCount < eventCount+1) {
						
				RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
					
				allocEventCount = NdasRaidReallocEventArray( &events, &waitBlocks, allocEventCount );

				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
			}

			if (events == NULL || waitBlocks == NULL) {

				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );

				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}


			if (clientContext->LocalClient) {

				events[eventCount++] = &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.RequestEvent;
				continue;
			}

			NDAS_ASSERT( clientContext->RequestConnection &&
							 clientContext->RequestConnection->ConnectionFileObject );

			NDAS_ASSERT( LpxTdiV2IsRequestPending(&clientContext->RequestConnection->ReceiveOverlapped, 0) );

			events[eventCount++] = &clientContext->RequestConnection->ReceiveOverlapped.Request[0].CompletionEvent;
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			
		if (status != STATUS_SUCCESS) {
		
			NDAS_ASSERT( status == STATUS_INSUFFICIENT_RESOURCES );
			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}

		NDAS_ASSERT( eventCount <= 4 );

		timeOut.QuadPart = -NANO100_PER_SEC * 30; // need to wake-up to handle dirty bitmap

		status = KeWaitForMultipleObjects( eventCount, 
										   events, 
										   WaitAny, 
										   Executive, 
										   KernelMode, 
										   TRUE,	
										   &timeOut, 
										   waitBlocks );

		KeClearEvent( &NdasrArbitrator->ThreadEvent );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Wake up, status = %x\n", status) );

SkipSetEvent:

		if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE)) {

			NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD) );
			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}

		// 2. check client request message

		status = NdasRaidArbitratorCheckRequestMsg( NdasrArbitrator );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_CLUSTER_NODE_UNREACHABLE );
			
			if (status == STATUS_CLUSTER_NODE_UNREACHABLE) {

				NdasrArbitrator->ClusterState16 = NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE >> 16;
			}

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}

		// 3. check SyncState transition

		if (NdasrArbitrator->SyncState == NDASR_SYNC_IN_PROGRESS) {

			PNDASR_CLIENT_CONTEXT	notSyncedClientContext = NULL;
		
			for (listEntry = NdasrArbitrator->ClientQueue.Flink;
				 listEntry != &NdasrArbitrator->ClientQueue;
				 listEntry = listEntry->Flink) {
			
				notSyncedClientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
				
				if (notSyncedClientContext->Usn != NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN) {

					break;
				}

				notSyncedClientContext = NULL;
			}

			if (notSyncedClientContext == NULL) {

				PNDASR_CLIENT_CONTEXT	clientContext;

				NdasrArbitrator->SyncState = NDASR_SYNC_DONE;

				listEntry = NdasrArbitrator->ClientQueue.Flink;

				while (listEntry != &NdasrArbitrator->ClientQueue) {
				
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					listEntry = listEntry->Flink;

					status = NdasRaidNotifyChangeStatus( NdasrArbitrator, clientContext );

					if (status != STATUS_SUCCESS) {

						NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
						
						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &clientContext->Link );
						
						RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

						DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notify failed during change status. Restarting.\n") );
					}
				}
			}
		}

		if (!IsListEmpty(&NdasrArbitrator->TerminatedClientQueue)) {

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}

		if (NdasrArbitrator->SyncState != NDASR_SYNC_DONE) {

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}


		// 4. check stop request

		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

		if (NdasrArbitrator->RequestToTerminate) {

			if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_TERMINATING)) {

				NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_START) );

				ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_START );
				NdasrArbitrator->ArbitratorState0 = NDASR_ARBITRATOR_STATUS_TERMINATING;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter stop requested. Sending RETIRE message to all client..\n") );	

				listEntry = NdasrArbitrator->ClientQueue.Flink;

				while (listEntry != &NdasrArbitrator->ClientQueue) {
							
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					listEntry = listEntry->Flink;

					if (clientContext->LocalClient) {

						continue;
					}

					RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

					status = NdasRaidNotifyRetire( NdasrArbitrator, clientContext );

					ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

					if (status != STATUS_SUCCESS) {

						NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &clientContext->Link );
					}

					// Continue to next client even if notification has failed.
				}

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sent RETIRE to all client. Exiting NdasrArbitrator loop.\n") );
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );


		// 5. check shutdown request

		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

		if (NdasrArbitrator->RequestToShutdown) {

			NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

			if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN)) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter shutdown requested.Sending RETIRE message to all client..\n") );	

				NdasrArbitrator->Shutdown8 = NDASR_ARBITRATOR_STATUS_SHUTDOWN >> 8;

				listEntry = NdasrArbitrator->ClientQueue.Flink;
			
				while (listEntry != &NdasrArbitrator->ClientQueue) {
				
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					listEntry = listEntry->Flink;

					if (clientContext->LocalClient) {

						continue;
					}

					RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
						
					status = NdasRaidNotifyRetire( NdasrArbitrator, clientContext );
						
					ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

					if (status != STATUS_SUCCESS) {

						NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &clientContext->Link );
					}
				}		
			
			} else {
			
				BOOLEAN clean;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter already Send RETIRE message to all client..\n") );	

				clean = TRUE;

				for (listEntry = NdasrArbitrator->ClientQueue.Flink;
					 listEntry != &NdasrArbitrator->ClientQueue;
					 listEntry = listEntry->Flink) {

					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					
					if (clientContext->LocalClient == FALSE) {

						clean = FALSE;
						break;
					}
				}

				if (clean == TRUE) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter set FinishShutdownEvent\n") );	

					RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

					if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, TRUE) == FALSE) {

							//NDAS_ASSERT( FALSE );
					}

					ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE );

					ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

					NdasrArbitrator->RequestToShutdown = FALSE;
					KeSetEvent( &NdasrArbitrator->FinishShutdownEvent, IO_DISK_INCREMENT, FALSE );
				}
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		if (NdasrArbitrator->SyncState != NDASR_SYNC_DONE) {

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}

		if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_START) || 
			FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN)) {

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
			continue;
		}


		// 6. rebuild

		NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_START) );
		NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN) );
		NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE) );
		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

		if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC) {

			NDAS_ASSERT( NdasrArbitrator->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE );

			if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD)) {
				
				status = NdasRaidRebuildInitiate( NdasrArbitrator );

				if (status == STATUS_SUCCESS) {

					if (RtlNumberOfSetBits(&NdasrArbitrator->OosBmpHeader) != 0) {

						NdasrArbitrator->RebuildState12 = NDASR_ARBITRATOR_STATUS_REBUILD >> 12;

						NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD) );

					} else {

						if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, TRUE) == FALSE) {
						
							NDAS_ASSERT( FALSE );
						}
					}
				
				} else {

					NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

					NdasrArbitrator->ClusterState16 = NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE >> 16;

					NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
					continue;
				}

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						    ("status = %x, NdasrArbitrator->Status = %x\n", status, NdasrArbitrator->Status) );
			}
		}

		// 7. check to use spare 

		NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_START) );
		NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN) );
		NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE) );
		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

		if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_DEGRADED) {

			NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_REBUILD) );
			spareUsed = FALSE;

			status = NdasRaidArbitratorUseSpareIfNeeded( NdasrArbitrator, &spareUsed );

			NDAS_ASSERT( status == STATUS_SUCCESS );
	
			if (spareUsed) {

				if (NdasRaidArbitratorRefreshRaidStatus(NdasrArbitrator, FALSE) == FALSE) {

					NDAS_ASSERT( FALSE );
				}

				NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
				continue;
			}
		}

		// 8. check new clientContext

		NDAS_ASSERT( FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_START) );
		NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN) );
		NDAS_ASSERT( !FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE) );
		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

		status = NdasRaidArbitratorRegisterNewClient( NdasrArbitrator );

		NDAS_ASSERT( status == STATUS_SUCCESS );

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	} while (TRUE);


	NDAS_ASSERT( IsListEmpty(&NdasrArbitrator->AcquiredLockList) );

	while (listEntry = ExInterlockedRemoveHeadList(&NdasrArbitrator->ClientQueue, &NdasrArbitrator->SpinLock)) {

		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		
		NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, NULL );
	}

	while (listEntry = ExInterlockedRemoveHeadList(&NdasrArbitrator->NewClientQueue, &NdasrArbitrator->SpinLock)) {

		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		
		NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, NULL );
	}

	ClearFlag( NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_TERMINATING );
	NdasrArbitrator->ArbitratorState0 = NDASR_ARBITRATOR_STATUS_TERMINATED;

	if (NdasRaidArbitratorUpdateInCoreRmd(NdasrArbitrator) == FALSE) {

		NDAS_ASSERT( FALSE );
	}

	if (NdasrArbitrator->NdasrState != NRMX_RAID_STATE_FAILED) {

		NdasRaidArbitratorWriteRmd( NdasrArbitrator, &NdasrArbitrator->Lurn->NdasrInfo->Rmd );
	}

	NDAS_ASSERT( NdasrArbitrator->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_UNMOUNTED ||
					 NdasrArbitrator->Lurn->NdasrInfo->Rmd.state == (NDAS_RAID_META_DATA_STATE_UNMOUNTED | NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) );

	NdasrArbitrator->NdasrState = NRMX_RAID_STATE_TERMINATED;
	
	NdasRaidFreeEventArray( events, waitBlocks );
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );

	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

NTSTATUS
NdasRaidArbitratorRegisterNewClient ( 
	IN PNDASR_ARBITRATOR NdasrArbitrator
	)
{
	NTSTATUS				status;

	KIRQL					oldIrql;

	PLIST_ENTRY				listEntry;
	PNDASR_CLIENT_CONTEXT	clientContext;


	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

	listEntry = NdasrArbitrator->NewClientQueue.Flink;

	while (listEntry != &NdasrArbitrator->NewClientQueue) {
				
		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		listEntry = listEntry->Flink;

		if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN) && clientContext->LocalClient == FALSE) {

			continue;
		}

		if (clientContext->UnregisterRequest) {

			RemoveEntryList( &clientContext->Link );

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, NULL );
			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

			continue;
		}

		if (clientContext->NotificationConnection && clientContext->NotificationConnection->NeedReply) {
	
			NRMX_HEADER	registerReply = {0};
			ULONG		result;

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

			registerReply.Signature = NTOHL(NRMX_SIGNATURE);
			registerReply.Command	= NRMX_CMD_REGISTER;
			registerReply.Length	= NTOHS((UINT16)sizeof(NRMX_HEADER));
			registerReply.ReplyFlag = 1;
			registerReply.Sequence	= clientContext->NotificationConnection->RegisterMsg.Header.Sequence;
			registerReply.Result	= NRMX_RESULT_SUCCESS;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Sending registration reply(result=%x) to remote client\n", registerReply.Result) );

			result = 0;

			status = LpxTdiV2Send ( clientContext->NotificationConnection->ConnectionFileObject, 
									(PUCHAR)&registerReply, 
									sizeof(NRMX_HEADER), 
									0,
									NULL,
									NULL,
									0,
									&result );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend NotificationConnection status=%x, result=%x.\n", status, result) );

			if (result != sizeof(NRMX_HEADER)) {

				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
				RemoveEntryList( &clientContext->Link );

				RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
				NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, NULL );
				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

				status = STATUS_SUCCESS;
				continue;
			}

			clientContext->NotificationConnection->NeedReply = FALSE;

			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
		}

		if (clientContext->RequestConnection && clientContext->RequestConnection->NeedReply) {

			NRMX_HEADER	registerReply = {0};
			ULONG		result;

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
	
			registerReply.Signature = NTOHL(NRMX_SIGNATURE);
			registerReply.Command	= NRMX_CMD_REGISTER;
			registerReply.Length	= NTOHS((UINT16)sizeof(NRMX_HEADER));
			registerReply.ReplyFlag = 1;
			registerReply.Sequence	= clientContext->RequestConnection->RegisterMsg.Header.Sequence;
			registerReply.Result	= NRMX_RESULT_SUCCESS;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Sending registration reply(result=%x) to remote client\n", registerReply.Result) );

			result = 0;

			status = LpxTdiV2Send ( clientContext->RequestConnection->ConnectionFileObject, 
									(PUCHAR)&registerReply, 
									sizeof(NRMX_HEADER), 
									0,
									NULL,
									NULL,
									0,
									&result );


			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend RequestConnection status=%x, result=%x.\n", status, result) );

			if (result != sizeof(NRMX_HEADER)) {

				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

				RemoveEntryList( &clientContext->Link );

				RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
				NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, NULL );
				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

				status = STATUS_SUCCESS;
				continue;
			}

			clientContext->RequestConnection->NeedReply = FALSE;

			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
		}

		if (clientContext->LocalClient == TRUE ||
			clientContext->NotificationConnection && clientContext->RequestConnection) {

			RemoveEntryList( &clientContext->Link );

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying initial status to client\n") );
			
			clientContext->Usn = NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN;

			status = NdasRaidNotifyChangeStatus( NdasrArbitrator, clientContext );
				
			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

			if (status == STATUS_SUCCESS) {

				RtlCopyMemory( clientContext->NodeFlags, NdasrArbitrator->NodeFlags, sizeof(NdasrArbitrator->NodeFlags) );
				InsertTailList( &NdasrArbitrator->ClientQueue, &clientContext->Link );
			
				continue;
			}

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			NdasRaidArbitratorTerminateClient( NdasrArbitrator, clientContext, NULL );
			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

			if (status == STATUS_CONNECTION_DISCONNECTED) {
						
				status = STATUS_SUCCESS;
				continue;
					
			} else {

				NDAS_ASSERT( FALSE );
				break;
			}
		}
	}

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	return status;
}

PNDASR_CLIENT_CONTEXT 
NdasRaidArbitratorAllocClientContext (
	VOID
	)
{
	PNDASR_CLIENT_CONTEXT ClientContext;
	
	ClientContext = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_CLIENT_CONTEXT), NDASR_CLIENT_CONTEXT_POOL_TAG );
	
	if (ClientContext == NULL) {

		NDAS_ASSERT( FALSE );
		return NULL;
	}

	RtlZeroMemory( ClientContext, sizeof(NDASR_CLIENT_CONTEXT) );

	InitializeListHead( &ClientContext->Link );
	InitializeListHead( &ClientContext->AcquiredLockList );
	
	return ClientContext;
}

VOID
NdasRaidArbitratorTerminateClient (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PBOOLEAN				OosBitmapSet
	) 
{
	PNDASR_ARBITRATOR_LOCK_CONTEXT	lock;
	PLIST_ENTRY						listEntry;
	KIRQL							oldIrql;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Terminating client.. Client = %p\n", ClientContext) );
	
	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

	if (!IsListEmpty(&ClientContext->AcquiredLockList)) {

		if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_NORMAL) {

			NDAS_ASSERT( NdasrArbitrator->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE );

			if (OosBitmapSet) {

				*OosBitmapSet = TRUE;
			}
		}
	}

	// Free locks acquired by this client
	
	while (!IsListEmpty(&ClientContext->AcquiredLockList)) {
	
		listEntry = RemoveHeadList( &ClientContext->AcquiredLockList );

		lock = CONTAINING_RECORD( listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ClientAcquiredLink );

		InitializeListHead( &lock->ClientAcquiredLink ); // to check bug...
		
		// Remove from arbiter's list too.
		
		RemoveEntryList( &lock->ArbitratorAcquiredLink );
		InitializeListHead( &lock->ArbitratorAcquiredLink ); // to check bug...

		lock->Owner = NULL;

		InterlockedDecrement( &NdasrArbitrator->AcquiredLockCount );
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Freeing terminated client's lock %I64x(%I64x:%x)\n", 
					 lock->Id, lock->BlockAddress, lock->BlockLength) );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Unclean termination. Merging this lock range to dirty bitmap\n") );

		// Merge this client's LWR to bitmap
			
		NdasRaidArbitratorChangeOosBitmapBit( NdasrArbitrator, TRUE, lock->BlockAddress, lock->BlockLength );
		NdasRaidArbitratorFreeLock( NdasrArbitrator, lock );
	}

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	if (ClientContext->NotificationConnection) {

		if (ClientContext->NotificationConnection->ConnectionFileObject) {

			if (LpxTdiV2IsRequestPending(&ClientContext->NotificationConnection->ReceiveOverlapped, 0)) {

				LpxTdiV2CancelRequest( ClientContext->NotificationConnection->ConnectionFileObject,
									   &ClientContext->NotificationConnection->ReceiveOverlapped,
									   0, 
									   FALSE,
									   0 );
			}

			LpxTdiV2Disconnect( ClientContext->NotificationConnection->ConnectionFileObject, 0 );
			LpxTdiV2DisassociateAddress( ClientContext->NotificationConnection->ConnectionFileObject );
			LpxTdiV2CloseConnection( ClientContext->NotificationConnection->ConnectionFileHandle, 
								     ClientContext->NotificationConnection->ConnectionFileObject, 
									 &ClientContext->NotificationConnection->ReceiveOverlapped );
		}

		ExFreePoolWithTag( ClientContext->NotificationConnection, NDASR_REMOTE_CLIENT_CHANNEL_POOL_TAG );
	} 

	if (ClientContext->RequestConnection) {
	
		if (ClientContext->RequestConnection->ConnectionFileObject) {

			if (LpxTdiV2IsRequestPending(&ClientContext->RequestConnection->ReceiveOverlapped, 0)) {

				LpxTdiV2CancelRequest( ClientContext->RequestConnection->ConnectionFileObject,
									   &ClientContext->RequestConnection->ReceiveOverlapped,
									   0,
									   FALSE,
									   0 );
			}

			LpxTdiV2Disconnect( ClientContext->RequestConnection->ConnectionFileObject, 0 );
			LpxTdiV2DisassociateAddress( ClientContext->RequestConnection->ConnectionFileObject );
			LpxTdiV2CloseConnection( ClientContext->RequestConnection->ConnectionFileHandle, 
								     ClientContext->RequestConnection->ConnectionFileObject,
									 &ClientContext->RequestConnection->ReceiveOverlapped );
		}

		ExFreePoolWithTag( ClientContext->RequestConnection, NDASR_REMOTE_CLIENT_CHANNEL_POOL_TAG );
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("Freeing client context = %p %p %p\n", 
				ClientContext, ClientContext->RequestConnection, ClientContext->NotificationConnection) );
	
	ExFreePoolWithTag( ClientContext, NDASR_CLIENT_CONTEXT_POOL_TAG );
}

NTSTATUS 
NdasRaidArbitratorInitializeOosBitmap (
	IN PNDASR_ARBITRATOR	NdasrArbitrator,
	IN PBOOLEAN				NodeIsUptoDate,
	IN UCHAR				UpToDateNode
	) 
{
	NTSTATUS				status;
	UINT32					i;
	UINT32					byteCount;
	ULONG					setBits;
	UCHAR					nidx;
	PNDAS_OOS_BITMAP_BLOCK	tmpOosBmpOnDiskBuffer = NULL;
	BOOLEAN					secondOosBmp;

	// Calc bitmap size. 
	
	NdasrArbitrator->OosBmpBitCount		  = (ULONG)((NdasrArbitrator->Lurn->UnitBlocks + NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit -1)/ NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit);
	NdasrArbitrator->OosBmpByteCount		  = ((NdasrArbitrator->OosBmpBitCount + sizeof(ULONG)*8 -1) /(sizeof(ULONG)*8))*sizeof(ULONG); // In core bit size should be padded to ULONG size.
	NdasrArbitrator->OosBmpOnDiskSectorCount = (NdasrArbitrator->OosBmpBitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;

	NDAS_ASSERT( NdasrArbitrator->OosBmpOnDiskSectorCount );
	NDAS_ASSERT( NdasrArbitrator->OosBmpOnDiskSectorCount < (2 << 16) );

	NdasrArbitrator->OosBmpBuffer = ExAllocatePoolWithTag( NonPagedPool, 
														NdasrArbitrator->OosBmpByteCount, 
														NDASR_BITMAP_POOL_TAG );

	if (NdasrArbitrator->OosBmpBuffer == NULL) {
	
		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasrArbitrator->OosBmpOnDiskBuffer = ExAllocatePoolWithTag( NonPagedPool, 
															  NdasrArbitrator->OosBmpOnDiskSectorCount * SECTOR_SIZE, 
															  NDASR_BITMAP_POOL_TAG );
	
	if (NdasrArbitrator->OosBmpOnDiskBuffer == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasrArbitrator->DirtyBmpSector = ExAllocatePoolWithTag( NonPagedPool, 
														  NdasrArbitrator->OosBmpOnDiskSectorCount * sizeof(BOOLEAN), 
														  NDASR_BITMAP_POOL_TAG );
	
	if (NdasrArbitrator->DirtyBmpSector == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasrArbitrator->LwrBmpBuffer = ExAllocatePoolWithTag( NonPagedPool, NdasrArbitrator->OosBmpByteCount, NDASR_BITMAP_POOL_TAG );

	if (NdasrArbitrator->LwrBmpBuffer == NULL) {
	
		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	tmpOosBmpOnDiskBuffer = ExAllocatePoolWithTag( NonPagedPool, 
												   NdasrArbitrator->OosBmpOnDiskSectorCount * SECTOR_SIZE, 
												   NDASR_BITMAP_POOL_TAG );
	
	if (tmpOosBmpOnDiskBuffer == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( NdasrArbitrator->OosBmpBuffer, NdasrArbitrator->OosBmpByteCount );
	RtlZeroMemory( NdasrArbitrator->LwrBmpBuffer, NdasrArbitrator->OosBmpByteCount );
	RtlZeroMemory( NdasrArbitrator->OosBmpOnDiskBuffer, NdasrArbitrator->OosBmpOnDiskSectorCount * SECTOR_SIZE );
	RtlZeroMemory( NdasrArbitrator->DirtyBmpSector, NdasrArbitrator->OosBmpOnDiskSectorCount * sizeof(BOOLEAN) );
	RtlZeroMemory( tmpOosBmpOnDiskBuffer, NdasrArbitrator->OosBmpOnDiskSectorCount * SECTOR_SIZE );

	RtlInitializeBitMap( &NdasrArbitrator->OosBmpHeader, NdasrArbitrator->OosBmpBuffer, NdasrArbitrator->OosBmpByteCount * 8 );
	RtlInitializeBitMap( &NdasrArbitrator->LwrBmpHeader, NdasrArbitrator->LwrBmpBuffer, NdasrArbitrator->OosBmpByteCount * 8 );


	// Read from UpToDateNode.
	// Assume UpToDateNode is non-spare and in-sync disk

	secondOosBmp = FALSE;

	for (nidx=0; nidx < NdasrArbitrator->Lurn->LurnChildrenCnt; nidx++) {

		status = NdasRaidLurnExecuteSynchrously( NdasrArbitrator->Lurn->LurnChildren[UpToDateNode], 
												  SCSIOP_READ16,
												  FALSE,
												  FALSE,
												  secondOosBmp ? (PUCHAR)tmpOosBmpOnDiskBuffer : (PUCHAR)NdasrArbitrator->OosBmpOnDiskBuffer, 
												  (-1*NDAS_BLOCK_LOCATION_BITMAP), 
												  NdasrArbitrator->OosBmpOnDiskSectorCount,
												  TRUE );

		if (status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to read bitmap from node %d\n", UpToDateNode) );			
			return status;
		} 
	
		// Check each sector for validity.
		
		for (i = 0; i < NdasrArbitrator->OosBmpOnDiskSectorCount; i++) {

			if (secondOosBmp) {

				if (tmpOosBmpOnDiskBuffer[i].SequenceNumHead != tmpOosBmpOnDiskBuffer[i].SequenceNumTail) {
			
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Bitmap sequence head/tail for sector %d mismatch %I64x:%I64x. Setting all dirty on this sector\n", 
								 i, NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead, NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumTail) );

					break;
				}

				continue;
			}

			if (NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead != NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumTail) {
			
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Bitmap sequence head/tail for sector %d mismatch %I64x:%I64x. Setting all dirty on this sector\n", 
							 i, NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead, NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumTail) );

				break;
			}			
		}

		if (i != NdasrArbitrator->OosBmpOnDiskSectorCount) {
		
			UCHAR	nidx2;

			NDAS_ASSERT( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("OOS bitmap has fault. Set all bits\n") );

			if (secondOosBmp) {

				continue;
			}
		
			for (nidx2 = nidx+1; nidx2 < NdasrArbitrator->Lurn->LurnChildrenCnt; nidx2++) {

				if (NodeIsUptoDate[nidx2] == TRUE) {

					break;
				}
			}

			if (nidx2 != NdasrArbitrator->Lurn->LurnChildrenCnt) {

				continue;
			}
	
			NDAS_ASSERT( FALSE );
			
			for (i=0; i<NdasrArbitrator->OosBmpOnDiskSectorCount; i++) {
		
				NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead = NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumTail = 0;

                if (i== NdasrArbitrator->OosBmpOnDiskSectorCount-1) {

                    UINT		remainingBits = NdasrArbitrator->OosBmpBitCount - i * NDAS_BIT_PER_OOS_BITMAP_BLOCK;
                    RTL_BITMAP	lastSectorBmpHeader;
                    
					RtlInitializeBitMap( &lastSectorBmpHeader, 
										 (PULONG)(NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits), 
										 NDAS_BIT_PER_OOS_BITMAP_BLOCK );
                    
					RtlClearAllBits( &lastSectorBmpHeader );
                    RtlSetBits( &lastSectorBmpHeader, 0, remainingBits );
                    
				} else {
				
					RtlFillMemory(NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits, sizeof(NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits), 0x0ff);
				}

				NdasrArbitrator->DirtyBmpSector[i] = TRUE;
			}
		}

		if (secondOosBmp == FALSE) {

			secondOosBmp = TRUE;
			continue;
		}

		if (NdasrArbitrator->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_UNMOUNTED) {

			NDAS_ASSERT( RtlEqualMemory(NdasrArbitrator->OosBmpOnDiskBuffer, 
											tmpOosBmpOnDiskBuffer, 
											NdasrArbitrator->OosBmpOnDiskSectorCount*SECTOR_SIZE) == TRUE );
		}

		for (i=0; i<NdasrArbitrator->OosBmpOnDiskSectorCount; i++) {

			ULONG j;

			for (j=0; j<NDAS_BYTE_PER_OOS_BITMAP_BLOCK; j++) {

				NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits[j] = 
					NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits[j] | tmpOosBmpOnDiskBuffer[i].Bits[j];

				if (NdasrArbitrator->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_UNMOUNTED && 
					NdasrArbitrator->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {
					
					NDAS_ASSERT( NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits[j] == 0 );
				}
			}
		}
	}

	NDAS_ASSERT( secondOosBmp == TRUE );

	// Convert on disk bitmap to in-memory bitmap
	
	for (i=0; i<NdasrArbitrator->OosBmpOnDiskSectorCount; i++) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Bitmap block %d sequence num=%I64x\n", 
											  i, NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead) );

		if (i == NdasrArbitrator->OosBmpOnDiskSectorCount-1) {
		
			// Last bitmap sector. 

			byteCount = NdasrArbitrator->OosBmpByteCount % NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
		
		} else {
		
			byteCount = NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
		}

		RtlCopyMemory( ((PUCHAR)NdasrArbitrator->OosBmpBuffer) + NDASR_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(i,0), 
						NdasrArbitrator->OosBmpOnDiskBuffer[i].Bits, 
						byteCount );
	}

	setBits = RtlNumberOfSetBits( &NdasrArbitrator->OosBmpHeader );

	if (setBits == FALSE) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Bitmap is clean\n") );
	}

	ExFreePoolWithTag( tmpOosBmpOnDiskBuffer, NDASR_BITMAP_POOL_TAG );

	return status;
}

VOID
NdasRaidArbitratorChangeOosBitmapBit (
	PNDASR_ARBITRATOR	NdasrArbitrator,
	BOOLEAN				Set,	// TRUE for set, FALSE for clear
	UINT64				Addr,
	UINT64				Length
	) 
{
	UINT32 bitOffset;
	UINT32 numberOfBit;

	ASSERT( KeGetCurrentIrql() ==  DISPATCH_LEVEL ); // should be called with spinlock locked.
	
	bitOffset	= (UINT32)(Addr / NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit);
	numberOfBit = (UINT32)((Addr + Length -1) / NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit - bitOffset + 1);


//	DebugTrace(NDASSCSI_DBG_LURN_NDASR_INFO, ("Before BitmapByte[0]=%x\n", Arbitrator->OosBmpBuffer[0]));	
	
	if (Set) {
	
		DebugTrace( DBG_LURN_TRACE, ("Setting in-memory bitmap offset %x:%x\n", bitOffset, numberOfBit) );
		
		RtlSetBits( &NdasrArbitrator->OosBmpHeader, bitOffset, numberOfBit );

	} else {

		DebugTrace( DBG_LURN_TRACE, ("Clearing in-memory bitmap offset %x:%x\n", bitOffset, numberOfBit) );
		
		RtlClearBits( &NdasrArbitrator->OosBmpHeader, bitOffset, numberOfBit );
	}
}

VOID
NdasRaidArbitratorUpdateLwrBitmapBit (
	PNDASR_ARBITRATOR				NdasrArbitrator,
	PNDASR_ARBITRATOR_LOCK_CONTEXT	HintAddedLock,
	PNDASR_ARBITRATOR_LOCK_CONTEXT	HintRemovedLock
	) 
{
	PLIST_ENTRY						listEntry;
	PNDASR_ARBITRATOR_LOCK_CONTEXT	lock;
	UINT32							bitOffset;
	UINT32							numberOfBit;
	ULONG							lockCount = 0;

	if (HintAddedLock && HintRemovedLock == NULL) {

		bitOffset = (UINT32) (HintAddedLock->BlockAddress / NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit);
		numberOfBit = (UINT32) ((HintAddedLock->BlockAddress + HintAddedLock->BlockLength -1) / NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit - 
								bitOffset + 1);
		
		DebugTrace( DBG_LURN_TRACE, ("Setting LWR bit %x:%x\n", bitOffset, numberOfBit) );
		
		RtlSetBits( &NdasrArbitrator->LwrBmpHeader, bitOffset, numberOfBit );
		
		lockCount = 1;
	
	} else {
		
		// Recalc all lock
		
		RtlClearAllBits( &NdasrArbitrator->LwrBmpHeader );

		for (listEntry = NdasrArbitrator->AcquiredLockList.Flink;
			listEntry != &NdasrArbitrator->AcquiredLockList;
			listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD(listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ArbitratorAcquiredLink);

			bitOffset = (UINT32)(lock->BlockAddress / NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit);
			numberOfBit = (UINT32)((lock->BlockAddress + lock->BlockLength - 1) / NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit - bitOffset + 1);

			// DebugTrace(DBG_LURN_TRACE, ("Setting bit %x:%x\n", BitOffset, NumberOfBit));
			
			RtlSetBits( &NdasrArbitrator->LwrBmpHeader, bitOffset, numberOfBit );
			
			lockCount++;
		}
	}

	DebugTrace( DBG_LURN_TRACE, ("Updated LWR bitmap with %d locks\n", lockCount) );		
}

NTSTATUS 
NdasRaidArbitratorUpdateOnDiskOosBitmap (
	PNDASR_ARBITRATOR	NdasrArbitrator,
	BOOLEAN			UpdateAll
	) 
{
	NTSTATUS	status = STATUS_SUCCESS; // in case for there is no dirty bitmap
	ULONG		i;
	KIRQL		oldIrql;
	ULONG		bitValues;
	ULONG		sector;
	ULONG		offset;
	
	 
	//	Merge InCoreOosbitmap and LWR into Ondisk buffer.
	//	Set dirty flag if data is changed.
	
	
	for (i=0; i<NdasrArbitrator->OosBmpByteCount/sizeof(ULONG); i++) {
	
		sector	  = NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(i*sizeof(ULONG));
		offset	  = NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(i*sizeof(ULONG));
		
		bitValues = NdasrArbitrator->OosBmpBuffer[i] | NdasrArbitrator->LwrBmpBuffer[i];
	
		if (UpdateAll)  {

			*((PULONG)&NdasrArbitrator->OosBmpOnDiskBuffer[sector].Bits[offset]) = bitValues;
			NdasrArbitrator->DirtyBmpSector[sector] = TRUE;
		}

		if (*((PULONG)&NdasrArbitrator->OosBmpOnDiskBuffer[sector].Bits[offset]) == bitValues) {
			
			continue;
		}
			
		DebugTrace( DBG_LURN_TRACE, ("Bitmap offset %x changed from %08x to %08x\n", 
									  i*4, *((PULONG)&NdasrArbitrator->OosBmpOnDiskBuffer[sector].Bits[offset]), bitValues) );

		*((PULONG)&NdasrArbitrator->OosBmpOnDiskBuffer[sector].Bits[offset]) = bitValues;
		NdasrArbitrator->DirtyBmpSector[sector] = TRUE;
	}

	// Update dirty bitmap sector only
	
	for (i=0; i<NdasrArbitrator->OosBmpOnDiskSectorCount; i++) {

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
		
		if (NdasrArbitrator->DirtyBmpSector[i]) {
		
			NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead++;
			NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumTail = NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead;
			NdasrArbitrator->DirtyBmpSector[i] = FALSE;
			
			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			
			DebugTrace( DBG_LURN_NOISE, 
						("Updating dirty bitmap sector %d, Seq = %I64x\n", i, NdasrArbitrator->OosBmpOnDiskBuffer[i].SequenceNumHead) );
			
			status = NdasRaidArbitratorWriteMetaSync( NdasrArbitrator, 
													(PUCHAR)&(NdasrArbitrator->OosBmpOnDiskBuffer[i]), 
													-1*(NDAS_BLOCK_LOCATION_BITMAP+i), 
													1, 
													TRUE );

			if (status != STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to update dirty bitmap sector %d\n", i) );	

				return status;
			}

		} else {

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
		}
	}

	return status;
}

NTSTATUS
NdasRaidArbitratorCheckRequestMsg (
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	) 
{
	NTSTATUS				status;
	PNDASR_CLIENT_CONTEXT	clientContext;
	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
		

	// Check request is received through request connection.

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	
	listEntry = NdasrArbitrator->ClientQueue.Flink;

	while (listEntry != &NdasrArbitrator->ClientQueue) {
		
		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		listEntry = listEntry->Flink;

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		if (clientContext->LocalClient) {

			PNDASR_LOCAL_MSG	ndasrRequestLocalMsg;
			PLIST_ENTRY			listEntry;

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock, &oldIrql );

			if (IsListEmpty(&NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.RequestQueue)) {

				RELEASE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock, oldIrql );

				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
				continue;
			}

			listEntry = RemoveHeadList( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.RequestQueue );
			ndasrRequestLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

			RELEASE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock, oldIrql );

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			status = NdasRaidArbitratorHandleRequestMsg( NdasrArbitrator, 
													  clientContext, 
													  (PNRMX_HEADER)ndasrRequestLocalMsg->NrmxHeader );

			NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
			continue;
		}

		NDAS_ASSERT( clientContext->RequestConnection && 
						 LpxTdiV2IsRequestPending(&clientContext->RequestConnection->ReceiveOverlapped, 0) );
		
		if (KeReadStateEvent(&clientContext->RequestConnection->ReceiveOverlapped.Request[0].CompletionEvent)) {
			
			LpxTdiV2CompleteRequest( &clientContext->RequestConnection->ReceiveOverlapped, 0 );

			do { 

				PNRMX_HEADER	message;
				ULONG			msgLength;
		
				if (clientContext->RequestConnection->ReceiveOverlapped.Request[0].IoStatusBlock.Status != STATUS_SUCCESS) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Failed to receive from %02x:%02x:%02x:%02x:%02x:%02x\n",
								 clientContext->RemoteClientAddr[0], clientContext->RemoteClientAddr[1], clientContext->RemoteClientAddr[2],
								 clientContext->RemoteClientAddr[3], clientContext->RemoteClientAddr[4], clientContext->RemoteClientAddr[5]) );
				
					status = STATUS_CONNECTION_DISCONNECTED;
					break;
				}	
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, 
							("Request received from %02x:%02x:%02x:%02x:%02x:%02x\n",
							clientContext->RemoteClientAddr[0], clientContext->RemoteClientAddr[1], clientContext->RemoteClientAddr[2],
							clientContext->RemoteClientAddr[3], clientContext->RemoteClientAddr[4], clientContext->RemoteClientAddr[5]) );


				if (clientContext->RequestConnection->ReceiveOverlapped.Request[0].IoStatusBlock.Information != sizeof(NRMX_HEADER)) {
			
					NDAS_ASSERT( FALSE );

					status = STATUS_CONNECTION_DISCONNECTED;
					break;
				} 

				// Read remaining data if needed.
				
				message = (PNRMX_HEADER) clientContext->RequestConnection->ReceiveBuf;
				
				msgLength = NTOHS(message->Length);
				
				if (msgLength > NRMX_MAX_REQUEST_SIZE || msgLength < sizeof(NRMX_HEADER)) {

					NDAS_ASSERT( FALSE );

					status = STATUS_CONNECTION_DISCONNECTED;
					break;
				} 
				
				if (msgLength > sizeof(NRMX_HEADER)) {

					LONG	result;
					
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Reading additional message data %d bytes\n", msgLength - sizeof(NRMX_HEADER)) );
					
					result = 0;

					status = LpxTdiV2Recv( clientContext->RequestConnection->ConnectionFileObject, 
										   (PUCHAR)(clientContext->RequestConnection->ReceiveBuf + sizeof(NRMX_HEADER)),
										   msgLength - sizeof(NRMX_HEADER),
										   0, 
										   NULL, 
										   NULL,
										   0,
										   &result );

					if (result != msgLength - sizeof(NRMX_HEADER)) {
					
						NDAS_ASSERT( FALSE );

						status = STATUS_CONNECTION_DISCONNECTED;
						break;
					}

					NDAS_ASSERT( status == STATUS_SUCCESS );

				} 			

				status = NdasRaidArbitratorHandleRequestMsg( NdasrArbitrator, 
														   clientContext, 
														   (PNRMX_HEADER)clientContext->RequestConnection->ReceiveBuf );
					
				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_CLUSTER_NODE_UNREACHABLE );
					break;
				}
			
			} while (0);

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_CLUSTER_NODE_UNREACHABLE );

				if (status == STATUS_CONNECTION_DISCONNECTED) {

					ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
					RemoveEntryList( &clientContext->Link );
					InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &clientContext->Link );
					RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );				
				}
				
				ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
				break;
			}			
		}

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	}

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	return status;
}

NTSTATUS
NdasRaidArbitratorHandleRequestMsg (
	PNDASR_ARBITRATOR		NdasrArbitrator, 
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PNRMX_HEADER			RequestMsg
	)
{
	NTSTATUS				status;

	PNRMX_HEADER			replyMsg;
	NRMX_HEADER				commonReplyMsg;
	NRMX_ACQUIRE_LOCK_REPLY	acquireLockReply;
	UINT32					replyLength;

	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
	ULONG					result;


	// Check data validity.

	if (NTOHL(RequestMsg->Signature) != NRMX_SIGNATURE || RequestMsg->ReplyFlag != 0) {

		NDAS_ASSERT( FALSE );	
		return STATUS_UNSUCCESSFUL;
	}

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

	if (NTOHL(RequestMsg->Usn) != NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN) {

		NDAS_ASSERT( ((signed _int32)(NTOHL(RequestMsg->Usn) - NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN)) < 0 );
		NDAS_ASSERT( NdasrArbitrator->SyncState != NDASR_SYNC_DONE );

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		return NdasRaidReplyChangeStatus( NdasrArbitrator, ClientContext, RequestMsg, NRMX_RESULT_LOWER_USN );
	}

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	if (NdasrArbitrator->SyncState != NDASR_SYNC_DONE) {

		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_IN_PROGRESS );
		NDAS_ASSERT( RequestMsg->Command == NRMX_CMD_NODE_CHANGE );

		if (RequestMsg->Command != NRMX_CMD_NODE_CHANGE) {

			NDAS_ASSERT( FALSE );

			return NdasRaidReplyChangeStatus( NdasrArbitrator, ClientContext, RequestMsg, NRMX_RESULT_UNSUCCESSFUL );
		}

		ClientContext->Usn = NTOHL(RequestMsg->Usn);
	}

	// Create reply
	
	// Process request

	switch (RequestMsg->Command) {

	case NRMX_CMD_NODE_CHANGE: {

		PNRMX_NODE_CHANGE	nodeChangeMsg = (PNRMX_NODE_CHANGE)RequestMsg;
		UINT32				i;

		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE || NdasrArbitrator->SyncState == NDASR_SYNC_IN_PROGRESS );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbitrator received node change message\n") );		

		replyLength = sizeof(NRMX_HEADER);
		replyMsg = &commonReplyMsg;

		RtlZeroMemory( &commonReplyMsg, replyLength );

		commonReplyMsg.Signature	= HTONL(NRMX_SIGNATURE);
		commonReplyMsg.ReplyFlag	= TRUE;
		commonReplyMsg.Command		= RequestMsg->Command;
		commonReplyMsg.Length		= HTONS((UINT16)replyLength);
		commonReplyMsg.Sequence		= RequestMsg->Sequence;

		// Update local node information from packet
		
		for (i=0; i<nodeChangeMsg->NodeCount; i++) {
			
			ClientContext->NodeFlags[nodeChangeMsg->Node[i].NodeNum] = nodeChangeMsg->Node[i].NodeFlags;
			ClientContext->DefectCodes[nodeChangeMsg->Node[i].NodeNum] = nodeChangeMsg->Node[i].DefectCode;
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d: Flag %x, Defect %x\n", i, nodeChangeMsg->Node[i].NodeFlags, nodeChangeMsg->Node[i].DefectCode) );
		}

		NdasRaidArbitratorRefreshRaidStatus( NdasrArbitrator, FALSE );
	
		if (NdasrArbitrator->SyncState != NDASR_SYNC_DONE) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RAID/Node status has been changed\n") );
			commonReplyMsg.Result = NRMX_RESULT_REQUIRE_SYNC;

		} else {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RAID/Node status has not been changed\n") );			
			commonReplyMsg.Result = NRMX_RESULT_NO_CHANGE;
		}

		break;
	}

	case NRMX_CMD_ACQUIRE_LOCK: {

		PNRMX_ACQUIRE_LOCK			acquireLockMsg	 = (PNRMX_ACQUIRE_LOCK) RequestMsg;
		UINT64						blockAddress	 = NTOHLL(acquireLockMsg->BlockAddress);
		UINT32						blockLength		 = NTOHL(acquireLockMsg->BlockLength);
		PNDASR_ARBITRATOR_LOCK_CONTEXT newLock;

		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );

		DebugTrace( DBG_LURN_TRACE, ("Arbitrator received ACQUIRE_LOCK message: %I64x:%x\n", blockAddress, blockLength) );

		replyLength = sizeof(NRMX_ACQUIRE_LOCK_REPLY);

		replyMsg = (PVOID)&acquireLockReply;

		RtlZeroMemory( &acquireLockReply, replyLength );

		acquireLockReply.Header.Signature	= HTONL(NRMX_SIGNATURE);
		acquireLockReply.Header.ReplyFlag	= TRUE;
		acquireLockReply.Header.Command		= RequestMsg->Command;
		acquireLockReply.Header.Length		= HTONS((UINT16)replyLength);
		acquireLockReply.Header.Sequence	= RequestMsg->Sequence;

		// Check lock list if lock overlaps with lock acquired by other client

#if DBG

		for (listEntry = ClientContext->AcquiredLockList.Flink;
			 listEntry != &ClientContext->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			 PNDASR_ARBITRATOR_LOCK_CONTEXT lock;

			lock = CONTAINING_RECORD( listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ClientAcquiredLink );
		
			NDAS_ASSERT( NdasRaidGetOverlappedRange( blockAddress, 
														  blockLength,
														  lock->BlockAddress, 
														  lock->BlockLength,
														  NULL, 
														  NULL ) == NDASR_RANGE_NO_OVERLAP );
		}
#endif

		do {

			if (blockAddress >= NdasrArbitrator->Lurn->UnitBlocks ) {

				NDAS_ASSERT( FALSE );

				acquireLockReply.Header.Result = NRMX_RESULT_UNSUCCESSFUL;
				break;	
			}

			newLock = NdasRaidArbitratorAllocLock( NdasrArbitrator, 
												 acquireLockMsg->LockType, 
												 acquireLockMsg->LockMode, 
												 blockAddress, 
												 blockLength );
	
			if (newLock == NULL) {

				NDAS_ASSERT( FALSE );

				acquireLockReply.Header.Result = NRMX_RESULT_UNSUCCESSFUL;
				break;	
			}
				
			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

			status = NdasRaidArbitratorArrangeLockRange( NdasrArbitrator, newLock, NdasrArbitrator->LockRangeGranularity );

			ASSERT( status == STATUS_SUCCESS );

			NdasRaidArbitratorUpdateLwrBitmapBit( NdasrArbitrator, newLock, NULL );

			if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_DEGRADED) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					        ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",newLock->BlockAddress, newLock->BlockLength) );

				NdasRaidArbitratorChangeOosBitmapBit( NdasrArbitrator, TRUE, newLock->BlockAddress, newLock->BlockLength );
			
			} else {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Granted lock %I64x(%I64x:%Ix).\n", newLock->Id, newLock->BlockAddress, newLock->BlockLength) );
			}
			
			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

			// Need to update BMP and LWR before client start to write using this lock
			
			status = NdasRaidArbitratorUpdateOnDiskOosBitmap( NdasrArbitrator, FALSE );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Failed to update bitmap\n") );

				NdasRaidArbitratorFreeLock( NdasrArbitrator, newLock );

				acquireLockReply.Header.Result = NRMX_RESULT_REQUIRE_SYNC;
				break;
			}
			
			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

			acquireLockReply.Header.Result = NRMX_RESULT_GRANTED;
			newLock->Owner = ClientContext;
			newLock->Status = NDASR_ARBITRATOR_LOCK_STATUS_GRANTED;

			// Add to arbiter list

			InsertTailList( &NdasrArbitrator->AcquiredLockList, &newLock->ArbitratorAcquiredLink );
			InterlockedIncrement( &NdasrArbitrator->AcquiredLockCount );

			InsertTailList( &ClientContext->AcquiredLockList, &newLock->ClientAcquiredLink );

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		} while (0);

		if (acquireLockReply.Header.Result == NRMX_RESULT_GRANTED) {

			acquireLockReply.LockType		= acquireLockMsg->LockType;
			acquireLockReply.LockMode		= acquireLockMsg->LockMode;
			acquireLockReply.LockId			= NTOHLL(newLock->Id);
			acquireLockReply.BlockAddress   = NTOHLL(newLock->BlockAddress);
			acquireLockReply.BlockLength    = NTOHL(newLock->BlockLength);
		
		} else {

			acquireLockReply.LockType		= 0;
			acquireLockReply.LockMode		= 0;
			acquireLockReply.LockId			= 0;
			acquireLockReply.BlockAddress	= 0;
			acquireLockReply.BlockLength	= 0;
		}

		break;
	}

	case NRMX_CMD_RELEASE_LOCK: {

		// Check lock is owned by this client.
	
		PNRMX_RELEASE_LOCK			releaseLockMsg = (PNRMX_RELEASE_LOCK) RequestMsg;
		PNDASR_ARBITRATOR_LOCK_CONTEXT	lock;
		UINT64						lockId = NTOHLL(releaseLockMsg->LockId);

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbitrator received RELEASE_LOCK message: %I64x\n", lockId) );

		NDAS_ASSERT( NdasrArbitrator->SyncState == NDASR_SYNC_DONE );
		
		replyLength = sizeof(NRMX_HEADER);
		replyMsg = &commonReplyMsg;

		RtlZeroMemory( &commonReplyMsg, replyLength );

		commonReplyMsg.Signature	= HTONL(NRMX_SIGNATURE);
		commonReplyMsg.ReplyFlag	= TRUE;
		commonReplyMsg.Command		= RequestMsg->Command;
		commonReplyMsg.Length		= HTONS((UINT16)replyLength);
		commonReplyMsg.Sequence		= RequestMsg->Sequence;

		// 1.0 chip does not support cache flush. 
		// Flush before releasing the lock.
	
		NdasRaidArbitratorFlushDirtyCacheNdas1_0( NdasrArbitrator, lockId, ClientContext );
				
		commonReplyMsg.Result = NRMX_RESULT_INVALID_LOCK_ID;

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

		// Search for matching Lock ID

		for (listEntry = NdasrArbitrator->AcquiredLockList.Flink;
			 listEntry != &NdasrArbitrator->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ArbitratorAcquiredLink);
		
			if (lockId == NRMX_LOCK_ID_ALL && lock->Owner == ClientContext) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						    ("Releasing all locks - Lock %I64x:%x\n", lock->BlockAddress, lock->BlockLength) );

				// Remove from all list

				listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
				
				RemoveEntryList( &lock->ArbitratorAcquiredLink );
				InitializeListHead( &lock->ArbitratorAcquiredLink );	// to check bug...
				InterlockedDecrement( &NdasrArbitrator->AcquiredLockCount );

				RemoveEntryList( &lock->ClientAcquiredLink );
				InitializeListHead( &lock->ClientAcquiredLink );	// to check bug...
				
				lock->Owner = NULL;

				commonReplyMsg.Result = NRMX_RESULT_SUCCESS;
				
				NdasRaidArbitratorFreeLock( NdasrArbitrator, lock );

				continue;

			} 
			
			if (lock->Id == lockId) {

				if (lock->Owner != ClientContext) {

					NDAS_ASSERT(FALSE);	
					break;
				} 

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Freeing client's lock %I64x(%I64x:%x)\n", 
							 lock->Id, lock->BlockAddress, lock->BlockLength) );
				
				// Remove from all list

				RemoveEntryList( &lock->ArbitratorAcquiredLink );
				InitializeListHead( &lock->ArbitratorAcquiredLink );	// to check bug...
				InterlockedDecrement( &NdasrArbitrator->AcquiredLockCount );

				RemoveEntryList( &lock->ClientAcquiredLink );
				InitializeListHead( &lock->ClientAcquiredLink );	// to check bug...

				lock->Owner = NULL;

				commonReplyMsg.Result = NRMX_RESULT_SUCCESS;

				NdasRaidArbitratorFreeLock( NdasrArbitrator, lock );

				break;
			}
		}

		if (lockId != NRMX_LOCK_ID_ALL) {

			NDAS_ASSERT( commonReplyMsg.Result == NRMX_RESULT_SUCCESS );
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		NdasRaidArbitratorUpdateLwrBitmapBit( NdasrArbitrator, NULL, NULL );
		
		status = NdasRaidArbitratorUpdateOnDiskOosBitmap( NdasrArbitrator, FALSE );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Failed to update OOS bitmap\n") );
		}

		break;
	}

	case NRMX_CMD_REGISTER:
//	case NRMX_CMD_UNREGISTER:
	default:

		NDAS_ASSERT( FALSE );

		replyLength = sizeof(NRMX_HEADER);
		replyMsg = &commonReplyMsg;

		commonReplyMsg.Signature	= HTONL(NRMX_SIGNATURE);
		commonReplyMsg.ReplyFlag	= TRUE;
		commonReplyMsg.Command		= RequestMsg->Command;
		commonReplyMsg.Length		= HTONS((UINT16)replyLength);
		commonReplyMsg.Sequence		= RequestMsg->Sequence;
		commonReplyMsg.Result		= NRMX_RESULT_UNSUPPORTED;

		break;
	}

	// Send reply
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("DRAID Sending reply to request %s with result %s to remote client\n", 
				 NdasRixGetCmdString(RequestMsg->Command), NdasRixGetResultString(replyMsg->Result)) );

	if (ClientContext->LocalClient) {

		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;

		ndasrReplyLocalMsg = &NdasrArbitrator->Lurn->NdasrInfo->RequestChannelReply;
		InitializeListHead( &ndasrReplyLocalMsg->ListEntry );

		ndasrReplyLocalMsg->NrmxHeader = &NdasrArbitrator->Lurn->NdasrInfo->NrmxHeader;

		RtlCopyMemory( ndasrReplyLocalMsg->NrmxHeader,
					   replyMsg,
					   replyLength );

		ExInterlockedInsertTailList( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
									 &ndasrReplyLocalMsg->ListEntry,
									 &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDAS_ASSERT( ClientContext->RequestConnection->ConnectionFileObject );

		result = 0;

		LpxTdiV2Send( ClientContext->RequestConnection->ConnectionFileObject, 
					  (PUCHAR)replyMsg, 
					  replyLength,
					  0,
					  NULL,
					  NULL,
					  0,
					  &result );

		if (result != replyLength) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to send request reply\n") );

			if (status == STATUS_SUCCESS) {

				status = STATUS_CONNECTION_DISCONNECTED;
			}
		}	
	}

	return status;
}

NTSTATUS
NdasRaidReplyChangeStatus (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PNRMX_HEADER			RequestMsg,
	UCHAR					Result
	)
{
	NTSTATUS			status;

	PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
	PNRMX_CHANGE_STATUS	changeStatusMsg = NULL;
	UINT32				msgLength;
	LONG				result;
	ULONG				i;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reply client with command DIRX_CMD_CHANGE_STATUS\n") );

	msgLength = SIZE_OF_NRMX_CHANGE_STATUS( NdasrArbitrator->Lurn->LurnChildrenCnt );

	if (ClientContext->LocalClient) {

		ndasrReplyLocalMsg = &NdasrArbitrator->Lurn->NdasrInfo->RequestChannelReply;	
		InitializeListHead( &ndasrReplyLocalMsg->ListEntry );

		changeStatusMsg = (PNRMX_CHANGE_STATUS)&NdasrArbitrator->Lurn->NdasrInfo->NrmxHeader;

	} else {

		changeStatusMsg = ExAllocatePoolWithTag( NonPagedPool, 
												 msgLength, 
												 NDASR_ARBITRATOR_NOTIFY_MSG_POOL_TAG );
	}

	RtlZeroMemory( changeStatusMsg, msgLength );

	changeStatusMsg->Header.Signature		= HTONL(NRMX_SIGNATURE);
	changeStatusMsg->Header.ReplyFlag		= TRUE;
	changeStatusMsg->Header.RaidInformation = TRUE;
	changeStatusMsg->Header.Command			= RequestMsg->Command;
	changeStatusMsg->Header.Length			= HTONS((UINT16)msgLength);
	changeStatusMsg->Header.Sequence		= RequestMsg->Sequence;
	changeStatusMsg->Header.Result			= Result;
	
	changeStatusMsg->Usn				= NTOHL(NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN);
	changeStatusMsg->RaidState			= (UCHAR)NdasrArbitrator->NdasrState;
	changeStatusMsg->NodeCount			= (UCHAR)NdasrArbitrator->Lurn->LurnChildrenCnt;
	changeStatusMsg->OutOfSyncRoleIndex = NdasrArbitrator->OutOfSyncRoleIndex;

	changeStatusMsg->WaitForSync = (NdasrArbitrator->SyncState == NDASR_SYNC_DONE) ? FALSE : TRUE;  

	NDAS_ASSERT( changeStatusMsg->WaitForSync );

	RtlCopyMemory( &changeStatusMsg->ConfigSetId, 
				   &NdasrArbitrator->Lurn->NdasrInfo->Rmd.ConfigSetId, 
				   sizeof(changeStatusMsg->ConfigSetId) );
		
	for (i=0; i<changeStatusMsg->NodeCount; i++) {
			
		changeStatusMsg->Node[i].NodeFlags = NdasrArbitrator->NodeFlags[i];
		changeStatusMsg->Node[i].NodeRole = NdasrArbitrator->NodeToRoleMap[i];
	}

	if (ClientContext->LocalClient) {

		ndasrReplyLocalMsg->NrmxHeader = (PNRMX_HEADER)changeStatusMsg;

		ExInterlockedInsertTailList( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
									 &ndasrReplyLocalMsg->ListEntry,
									 &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrArbitrator->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDAS_ASSERT( ClientContext->RequestConnection->ConnectionFileObject );

		result = 0;

		status = LpxTdiV2Send( ClientContext->RequestConnection->ConnectionFileObject, 
							   (PUCHAR)changeStatusMsg, 
							   msgLength,
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != msgLength) {

			NDAS_ASSERT( !NT_SUCCESS(status) );

			status = STATUS_CONNECTION_DISCONNECTED;
		}

		ExFreePoolWithTag( changeStatusMsg, NDASR_ARBITRATOR_NOTIFY_MSG_POOL_TAG );
	}

	return status;
}

NTSTATUS
NdasRaidNotifyChangeStatus (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext
	)
{
	NTSTATUS			status;

	PNDASR_LOCAL_MSG	ndasrNotifyLocalMsg;
	PNRMX_CHANGE_STATUS	changeStatusMsg = NULL;
	UINT32				msgLength;
	LONG				result;
	ULONG				i;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying client with command DIRX_CMD_CHANGE_STATUS\n") );

	msgLength = SIZE_OF_NRMX_CHANGE_STATUS( NdasrArbitrator->Lurn->LurnChildrenCnt );

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg = ExAllocatePoolWithTag( NonPagedPool, 
													 sizeof(NDASR_LOCAL_MSG) + msgLength, 
													 NDASR_LOCAL_MSG_POOL_TAG );
	
		InitializeListHead( &ndasrNotifyLocalMsg->ListEntry );

		changeStatusMsg = (PNRMX_CHANGE_STATUS)(ndasrNotifyLocalMsg + 1);

	} else {

		changeStatusMsg = ExAllocatePoolWithTag( NonPagedPool, 
												 msgLength, 
												 NDASR_ARBITRATOR_NOTIFY_MSG_POOL_TAG );
	}

	RtlZeroMemory( changeStatusMsg, msgLength );

	changeStatusMsg->Header.Signature = HTONL(NRMX_SIGNATURE);
	changeStatusMsg->Header.Command   = NRMX_CMD_CHANGE_STATUS;
	changeStatusMsg->Header.Length    = HTONS((UINT16)msgLength);
	changeStatusMsg->Header.Sequence  = HTONS(ClientContext->NotifySequence);
	
	ClientContext->NotifySequence++;

	changeStatusMsg->Usn				= NTOHL(NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN);
	changeStatusMsg->RaidState			= (UCHAR)NdasrArbitrator->NdasrState;
	changeStatusMsg->NodeCount			= (UCHAR)NdasrArbitrator->Lurn->LurnChildrenCnt;
	changeStatusMsg->OutOfSyncRoleIndex = NdasrArbitrator->OutOfSyncRoleIndex;

	changeStatusMsg->WaitForSync = (NdasrArbitrator->SyncState == NDASR_SYNC_DONE) ? FALSE : TRUE;  
		
	RtlCopyMemory( &changeStatusMsg->ConfigSetId, 
				   &NdasrArbitrator->Lurn->NdasrInfo->Rmd.ConfigSetId, 
				   sizeof(changeStatusMsg->ConfigSetId) );
		
	for (i=0; i<changeStatusMsg->NodeCount; i++) {
			
		changeStatusMsg->Node[i].NodeFlags = NdasrArbitrator->NodeFlags[i];
		changeStatusMsg->Node[i].NodeRole = NdasrArbitrator->NodeToRoleMap[i];
	}

	if (ClientContext->LocalClient) {

		KIRQL				oldIrql;

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.SpinLock, &oldIrql );
		
		if (NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.Terminated) {

			ExFreePoolWithTag( ndasrNotifyLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

			status = STATUS_CONNECTION_DISCONNECTED;
		
		} else {

			ndasrNotifyLocalMsg->NrmxHeader = (PNRMX_HEADER)changeStatusMsg;

			InsertTailList( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
							&ndasrNotifyLocalMsg->ListEntry );

			KeSetEvent( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
					    IO_NO_INCREMENT,
						FALSE );

			status = STATUS_SUCCESS;
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.SpinLock, oldIrql );

	} else {

		NDAS_ASSERT( ClientContext->NotificationConnection->ConnectionFileObject );

		result = 0;

		status = LpxTdiV2Send( ClientContext->NotificationConnection->ConnectionFileObject, 
							   (PUCHAR)changeStatusMsg, 
							   msgLength,
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != msgLength) {
		
			status = STATUS_CONNECTION_DISCONNECTED;
		} 

		ExFreePoolWithTag( changeStatusMsg, NDASR_ARBITRATOR_NOTIFY_MSG_POOL_TAG );
	}

	return status;
}

NTSTATUS
NdasRaidNotifyStatusSynced (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext
	)
{
	NTSTATUS			status;
	PNDASR_LOCAL_MSG	ndasrNotifyLocalMsg;
	NRMX_HEADER			statusSyncedMsg;
	UINT32				msgLength;
	LONG				result;
	KIRQL				oldIrql;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying client with command NRMX_CMD_STATUS_SYNCED\n") );

	msgLength = sizeof(NRMX_HEADER);

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg = ExAllocatePoolWithTag( NonPagedPool, 
													 sizeof(NDASR_LOCAL_MSG) + msgLength, 
													 NDASR_LOCAL_MSG_POOL_TAG );

		InitializeListHead( &ndasrNotifyLocalMsg->ListEntry );

	}

	RtlZeroMemory( &statusSyncedMsg, msgLength );

	statusSyncedMsg.Signature = HTONL(NRMX_SIGNATURE);
	statusSyncedMsg.Command	  = NRMX_CMD_STATUS_SYNCED;
	statusSyncedMsg.Length	  = HTONS((UINT16)msgLength);
	statusSyncedMsg.Sequence  = HTONS(ClientContext->NotifySequence);
	
	ClientContext->NotifySequence++;

	if (ClientContext->LocalClient) {

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.SpinLock, &oldIrql );
		
		if (NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.Terminated) {

			ExFreePoolWithTag( ndasrNotifyLocalMsg, NDASR_LOCAL_MSG_POOL_TAG );

			status = STATUS_CONNECTION_DISCONNECTED;
		
		} else {

			ndasrNotifyLocalMsg->NrmxHeader = (PNRMX_HEADER)(ndasrNotifyLocalMsg+1);
			RtlCopyMemory( ndasrNotifyLocalMsg->NrmxHeader,
						   &statusSyncedMsg,
						   sizeof(statusSyncedMsg) );

			InsertTailList( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
							&ndasrNotifyLocalMsg->ListEntry );

			status = KeSetEvent( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
								 IO_NO_INCREMENT,
								 FALSE );

			status = STATUS_SUCCESS;
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.SpinLock, oldIrql );

	} else {

		NDAS_ASSERT( ClientContext->NotificationConnection->ConnectionFileObject );

		result = 0;

		status = LpxTdiV2Send( ClientContext->NotificationConnection->ConnectionFileObject, 
							   (PUCHAR)&statusSyncedMsg, 
							   msgLength,
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != msgLength) {

			status = STATUS_CONNECTION_DISCONNECTED;

			ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
			RemoveEntryList( &ClientContext->Link );
			InsertTailList( &NdasrArbitrator->TerminatedClientQueue, &ClientContext->Link );
			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );				
		} 
	}

	return status;
}

NTSTATUS
NdasRaidNotifyRetire (
	PNDASR_ARBITRATOR		NdasrArbitrator,
	PNDASR_CLIENT_CONTEXT	ClientContext
	)
{
	NTSTATUS			status;
	PNDASR_LOCAL_MSG	ndasrNotifyLocalMsg;
	NRMX_HEADER			retireMsg;
	UINT32				msgLength;
	LONG				result;

	UNREFERENCED_PARAMETER( NdasrArbitrator );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying client with command NRMX_CMD_RETIRE\n") );

	msgLength = sizeof(NRMX_HEADER);

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg = ExAllocatePoolWithTag( NonPagedPool, 
													 sizeof(NDASR_LOCAL_MSG) + msgLength, 
													 NDASR_LOCAL_MSG_POOL_TAG );

		InitializeListHead( &ndasrNotifyLocalMsg->ListEntry );

	}

	RtlZeroMemory( &retireMsg, msgLength );

	retireMsg.Signature = HTONL(NRMX_SIGNATURE);
	retireMsg.Command   = NRMX_CMD_RETIRE;
	retireMsg.Length    = HTONS((UINT16)msgLength);
	retireMsg.Sequence  = HTONS(ClientContext->NotifySequence);
	
	ClientContext->NotifySequence++;

	if (ClientContext->LocalClient) {

		NDAS_ASSERT( FALSE );

		ndasrNotifyLocalMsg->NrmxHeader = (PNRMX_HEADER)(ndasrNotifyLocalMsg+1);
		RtlCopyMemory( ndasrNotifyLocalMsg->NrmxHeader,
					   &retireMsg,
					   sizeof(retireMsg) );

		ExInterlockedInsertTailList( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
									 &ndasrNotifyLocalMsg->ListEntry,
									 &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

		status = KeSetEvent( &NdasrArbitrator->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
							 IO_NO_INCREMENT,
							 FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDAS_ASSERT( ClientContext->NotificationConnection->ConnectionFileObject );

		result = 0;

		status = LpxTdiV2Send( ClientContext->NotificationConnection->ConnectionFileObject, 
							   (PUCHAR)&retireMsg, 
							   msgLength,
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != msgLength) {

			status = STATUS_CONNECTION_DISCONNECTED;
		} 
	}

	return status;
}

BOOLEAN
NdasRaidArbitratorRefreshRaidStatus (
	IN PNDASR_ARBITRATOR	NdasrArbitrator,
	IN BOOLEAN				ForceChange
	)
{
	BOOLEAN					changed;

	UCHAR					nidx, ridx;
	KIRQL					oldIrql, oldIrql2;
	PLIST_ENTRY				listEntry;
	PNDASR_CLIENT_CONTEXT	clientContext;

	UCHAR					newNdasrState;
	UCHAR					newNodeFlags[MAX_NDASR_MEMBER_DISK];
	UCHAR					newDefectCodes[MAX_NDASR_MEMBER_DISK];

	NTSTATUS				status;	

retry:

	changed = FALSE;

	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );

	// Gather each client's node information
	
	for (nidx = 0; nidx < NdasrArbitrator->Lurn->LurnChildrenCnt; nidx++) {

		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->Lurn->LurnChildren[nidx]->SpinLock, &oldIrql2 );

		newNodeFlags[nidx] = NdasRaidLurnStatusToNodeFlag( NdasrArbitrator->Lurn->LurnChildren[nidx]->LurnStatus );
		
		if (LurnGetCauseOfFault(NdasrArbitrator->Lurn->LurnChildren[nidx]) & (LURN_FCAUSE_BAD_SECTOR | LURN_FCAUSE_BAD_DISK)) {

			newNodeFlags[nidx]	 = NRMX_NODE_FLAG_DEFECTIVE;
			newDefectCodes[nidx] = ((LurnGetCauseOfFault(NdasrArbitrator->Lurn->LurnChildren[nidx]) & LURN_FCAUSE_BAD_SECTOR) ?
									NRMX_NODE_DEFECT_BAD_SECTOR : NRMX_NODE_DEFECT_BAD_DISK );
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->Lurn->LurnChildren[nidx]->SpinLock, oldIrql2 );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting initial node %d flag: %d\n", nidx, NdasrArbitrator->NodeFlags[nidx]) );

		for (listEntry = NdasrArbitrator->ClientQueue.Flink;
			 listEntry != &NdasrArbitrator->ClientQueue;
			 listEntry = listEntry->Flink) {

			clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
	
			// Flag priority:  NRMX_NODE_FLAG_DEFECTIVE > NRMX_NODE_FLAG_STOP > NRMX_NODE_FLAG_RUNNING > NRMX_NODE_FLAG_UNKNOWN

			if (FlagOn(newNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {
			
				if (FlagOn(clientContext->NodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Node %d: Arbitrator->NodeFlag %x Old flag %x, Client Flag %x\n", 
								 nidx, NdasrArbitrator->DefectCodes[nidx], newDefectCodes[nidx], clientContext->DefectCodes[nidx] ) );

					SetFlag( newDefectCodes[nidx], clientContext->DefectCodes[nidx] );
				}

				continue;
			}
			
			if (FlagOn(clientContext->NodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {
	
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Node %d: Arbitrator->NodeFlag %x Old flag %x, Client Flag %x\n", 
							 nidx, NdasrArbitrator->NodeFlags[nidx], newNodeFlags[nidx], clientContext->NodeFlags[nidx] ) );

				newNodeFlags[nidx] = NRMX_NODE_FLAG_DEFECTIVE;
				SetFlag( newDefectCodes[nidx], clientContext->DefectCodes[nidx] );

				continue;
			}

			if (FlagOn(newNodeFlags[nidx], NRMX_NODE_FLAG_STOP)) {

				continue;
			}

			if (FlagOn(clientContext->NodeFlags[nidx], NRMX_NODE_FLAG_STOP)) {
							
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Node %d: Arbitrator->NodeFlag %x Old flag %x, Client Flag %x\n", 
							 nidx, NdasrArbitrator->NodeFlags[nidx], newNodeFlags[nidx], clientContext->NodeFlags[nidx] ) );

				newNodeFlags[nidx] = NRMX_NODE_FLAG_STOP;

				continue;
			}
			
			if (FlagOn(newNodeFlags[nidx], NRMX_NODE_FLAG_RUNNING)) {

				continue;
			}

			if (FlagOn(clientContext->NodeFlags[nidx], NRMX_NODE_FLAG_RUNNING)) {
							
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Node %d: Arbitrator->NodeFlag %x Old flag %x, Client Flag %x\n", 
							 nidx, NdasrArbitrator->NodeFlags[nidx], newNodeFlags[nidx], clientContext->NodeFlags[nidx] ) );

				newNodeFlags[nidx] = NRMX_NODE_FLAG_RUNNING;

				continue;
			}

			NDAS_ASSERT( FALSE );
		}

		if (FlagOn(NdasrArbitrator->NodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

			NDAS_ASSERT( FlagOn(newNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE) );

			if (NdasrArbitrator->DefectCodes[nidx] != newDefectCodes[nidx]) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
							("NdasrArbitrator->DefectCodes[nidx] = %x, newDefectCodes[nidx] = %x\n", 
							 NdasrArbitrator->DefectCodes[nidx], newDefectCodes[nidx]) );
				
				SetFlag( NdasrArbitrator->DefectCodes[nidx], newDefectCodes[nidx] );

				changed = TRUE;
			}
		}
		
		if (NdasrArbitrator->NodeFlags[nidx] != newNodeFlags[nidx]) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Changing Node %d flags from %02x to %02x\n", nidx, NdasrArbitrator->NodeFlags[nidx], newNodeFlags[nidx]) );
			
			NdasrArbitrator->NodeFlags[nidx] = newNodeFlags[nidx];

			changed = TRUE;
		}
	}

	if (changed == FALSE) {

		if (ForceChange != TRUE) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasrArbitrator->NdasrState = %x\n", NdasrArbitrator->NdasrState) );

			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			return FALSE;
		}
	}

	for (nidx = 0; nidx < NdasrArbitrator->Lurn->LurnChildrenCnt; nidx++) {

		NDAS_ASSERT( NdasrArbitrator->NodeFlags[nidx] == NRMX_NODE_FLAG_RUNNING	||
						 NdasrArbitrator->NodeFlags[nidx] == NRMX_NODE_FLAG_DEFECTIVE	||
						 NdasrArbitrator->NodeFlags[nidx] == NRMX_NODE_FLAG_STOP );
	}

	// Test new RAID status only when needed, i.e: node has changed or first time.
	
	newNdasrState = NRMX_RAID_STATE_NORMAL;

	if (RtlNumberOfSetBits(&NdasrArbitrator->OosBmpHeader) != 0) {
		
		newNdasrState = NRMX_RAID_STATE_OUT_OF_SYNC;
	}

	for (ridx = 0; ridx < NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { // i : role index
			
		if (!FlagOn(NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

			switch (newNdasrState) {

			case NRMX_RAID_STATE_NORMAL: {

				NdasrArbitrator->OutOfSyncRoleIndex = ridx;
				newNdasrState = NRMX_RAID_STATE_DEGRADED;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting new out of sync role: %d\n", ridx) );

				break;
			}

			case NRMX_RAID_STATE_OUT_OF_SYNC: {

				if (NdasrArbitrator->OutOfSyncRoleIndex == ridx) {
					
					newNdasrState = NRMX_RAID_STATE_DEGRADED;
				
				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrArbitrator->RoleToNodeMap[ridx]) );

					//NDAS_ASSERT( FALSE );

					newNdasrState = NRMX_RAID_STATE_FAILED;
				}

				break;
			}

			case NRMX_RAID_STATE_DEGRADED: {

				NDAS_ASSERT( NdasrArbitrator->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE && NdasrArbitrator->OutOfSyncRoleIndex != ridx );
				newNdasrState = NRMX_RAID_STATE_FAILED;

				//NDAS_ASSERT( FALSE );

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrArbitrator->RoleToNodeMap[ridx]) );

				break;
			}

			default:

				break;			
			}
		}
	}

	NDAS_ASSERT( newNdasrState == NRMX_RAID_STATE_NORMAL		||
					 newNdasrState == NRMX_RAID_STATE_DEGRADED		||
					 newNdasrState == NRMX_RAID_STATE_OUT_OF_SYNC	||
					 newNdasrState == NRMX_RAID_STATE_FAILED );

	if (ForceChange != TRUE) {

		NDAS_ASSERT( NdasrArbitrator->NdasrState != newNdasrState );
	}

	if (NdasrArbitrator->NdasrState != newNdasrState) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("*************Changing DRAID Status from %x to %x****************\n", 
					 NdasrArbitrator->NdasrState, newNdasrState) );

		if (newNdasrState == NRMX_RAID_STATE_NORMAL) {

			NdasrArbitrator->OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;
		}

		if (newNdasrState == NRMX_RAID_STATE_OUT_OF_SYNC) {

			if (NdasrArbitrator->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {

				NdasrArbitrator->OutOfSyncRoleIndex = NdasrArbitrator->NodeToRoleMap[NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount-1];
			}
		}

		if (newNdasrState == NRMX_RAID_STATE_DEGRADED) {
		
			PNDASR_ARBITRATOR_LOCK_CONTEXT Lock;

			KeQueryTickCount( &NdasrArbitrator->DegradedSince );

			// We need to merge all LWR to OOS bitmap
			//	because defective/stopped node may have lost data in disk's write-buffer.

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Merging LWRs to OOS bitmap\n") );

			for (listEntry = NdasrArbitrator->AcquiredLockList.Flink;
				 listEntry != &NdasrArbitrator->AcquiredLockList;
				 listEntry = listEntry->Flink) {

				Lock = CONTAINING_RECORD( listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ArbitratorAcquiredLink );
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
							("Merging LWR %I64x:%x to OOS bitmap\n", Lock->BlockAddress, Lock->BlockLength) );

				NdasRaidArbitratorChangeOosBitmapBit( NdasrArbitrator, TRUE, Lock->BlockAddress, Lock->BlockLength );
			}
		}

		NdasrArbitrator->NdasrState = newNdasrState;
	}

	NdasrArbitrator->SyncState = NDASR_SYNC_REQUIRED;
	
	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	changed = NdasRaidArbitratorUpdateInCoreRmd( NdasrArbitrator );
	
	if (changed == FALSE) {

		if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_INITIALIZING) &&
			!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN)) {

			NDAS_ASSERT( FALSE );
		}
	}
	
	if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_NORMAL) {

		status = NdasRaidArbitratorUpdateOnDiskOosBitmap( NdasrArbitrator, TRUE );

		if (!NT_SUCCESS(status)) {

			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}

		status = NdasRaidArbitratorWriteRmd( NdasrArbitrator, &NdasrArbitrator->Lurn->NdasrInfo->Rmd );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}

	} else if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_DEGRADED ||
			   NdasrArbitrator->NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC) {

		status = NdasRaidArbitratorWriteRmd( NdasrArbitrator, &NdasrArbitrator->Lurn->NdasrInfo->Rmd );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}

		status = NdasRaidArbitratorUpdateOnDiskOosBitmap( NdasrArbitrator, TRUE );

		if (!NT_SUCCESS(status)) {

			NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasrArbitrator->NdasrState = %x\n", NdasrArbitrator->NdasrState) );

	return changed;
}

BOOLEAN
NdasRaidArbitratorUpdateInCoreRmd (
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	) 
{
	NDAS_RAID_META_DATA newRmd = {0};
	UINT32				nidx, ridx;
	UINT32				j;
	UCHAR				nodeFlags;


	newRmd.Signature	= NdasrArbitrator->Lurn->NdasrInfo->Rmd.Signature;
	newRmd.RaidSetId	= NdasrArbitrator->Lurn->NdasrInfo->Rmd.RaidSetId;
	newRmd.uiUSN		= NdasrArbitrator->Lurn->NdasrInfo->Rmd.uiUSN;
	newRmd.ConfigSetId	= NdasrArbitrator->ConfigSetId;

	if (!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_INITIALIZING) &&
		!FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN)) {

		NDAS_ASSERT( NdasrArbitrator->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_MOUNTED ||
						 NdasrArbitrator->Lurn->NdasrInfo->Rmd.state == (NDAS_RAID_META_DATA_STATE_MOUNTED | NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED ));
	}

	if (FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_TERMINATED) ||
		FlagOn(NdasrArbitrator->Status, NDASR_ARBITRATOR_STATUS_SHUTDOWN) && IsListEmpty(&NdasrArbitrator->AcquiredLockList)) {

		newRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;

	} else {

		newRmd.state = NDAS_RAID_META_DATA_STATE_MOUNTED;
	}

	// Keep NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED.

	switch (NdasrArbitrator->NdasrState) {

	case NRMX_RAID_STATE_DEGRADED:
		
		if (!FlagOn(NdasrArbitrator->Lurn->NdasrInfo->Rmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );
		}

		SetFlag( newRmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED );		

		break;

	case NRMX_RAID_STATE_NORMAL:
	case NRMX_RAID_STATE_OUT_OF_SYNC:
		
		if (FlagOn(NdasrArbitrator->Lurn->NdasrInfo->Rmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Clearing NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );			
		}

		break;
	
	default: // Keep previous flag
	
		newRmd.state |= (NdasrArbitrator->Lurn->NdasrInfo->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED);

		if (FlagOn(NdasrArbitrator->Lurn->NdasrInfo->Rmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Keep marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );
		}

		break;
	}

	RtlZeroMemory( newRmd.ArbitratorInfo, sizeof(newRmd.ArbitratorInfo) );
		
	for (j=0; j<NDAS_RAID_ARBITRATOR_ADDR_COUNT; j++) {

		newRmd.ArbitratorInfo[j].Type = NDAS_RAID_ARBITRATOR_TYPE_NONE;
	}
	
	if (FlagOn(newRmd.state, NDAS_RAID_META_DATA_STATE_MOUNTED) &&
		NdasrArbitrator->NdasrState != NRMX_RAID_STATE_FAILED) {

		UINT32					usedAddrCount;
		PUCHAR					macAddr;
		PLURNEXT_IDE_DEVICE		ideDisk;
		
		RtlZeroMemory( newRmd.ArbitratorInfo, sizeof(newRmd.ArbitratorInfo) );

		ASSERT( sizeof(newRmd.ArbitratorInfo) == 12*8 );
		
		// Get list of bind address from each children.
		
		usedAddrCount = 0;
		
		for (nidx = 0; nidx < NdasrArbitrator->Lurn->LurnChildrenCnt; nidx++) {
		
			// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
			
			if (!FlagOn(NdasrArbitrator->NodeFlags[nidx], NRMX_NODE_FLAG_RUNNING) ||
				FlagOn(NdasrArbitrator->NodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

				continue;
			}

			ideDisk = (PLURNEXT_IDE_DEVICE)NdasrArbitrator->Lurn->LurnChildren[nidx]->LurnExtension;
			
			if (!ideDisk) {

				NDAS_ASSERT( FALSE );
				continue;
			}

			macAddr = ((PTDI_ADDRESS_LPX)ideDisk->LanScsiSession->NdasBindAddress.Address)->Node;
			
			// Search address is already recorded.
			
			for (j=0; j < usedAddrCount; j++) {
			
				if (RtlCompareMemory(newRmd.ArbitratorInfo[j].Addr, macAddr, 6) == 6) {
					
					// This bind address is alreay in entry. Skip

					break;
				}
			}

			if (j != usedAddrCount) {

				continue;
			}

			newRmd.ArbitratorInfo[usedAddrCount].Type = NDAS_RAID_ARBITRATOR_TYPE_LPX;
				
			RtlCopyMemory( newRmd.ArbitratorInfo[usedAddrCount].Addr, macAddr, 6 );
			usedAddrCount++;

			if (usedAddrCount >= NDAS_RAID_ARBITRATOR_ADDR_COUNT) {
				
				break;
			}
		}

		NDAS_ASSERT( usedAddrCount > 0 );	
	}

	for (ridx = 0; ridx < NdasrArbitrator->Lurn->LurnChildrenCnt; ridx++) { // i is role index.
		
		newRmd.UnitMetaData[ridx].iUnitDeviceIdx = NdasrArbitrator->RoleToNodeMap[ridx];
		
		nodeFlags = NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[ridx]];
		
		if (FlagOn(nodeFlags, NRMX_NODE_FLAG_DEFECTIVE)) {
		
			UCHAR defectCode = NdasrArbitrator->DefectCodes[NdasrArbitrator->RoleToNodeMap[ridx]];

			// Relative defect code such as RMD mismatch is not needed to be written to disk
			// Record physical defects and spare-used flag only

			SetFlag( newRmd.UnitMetaData[ridx].UnitDeviceStatus, NdasRaidNodeDefectCodeToRmdUnitStatus(defectCode) );
		}
		
		if (ridx == NdasrArbitrator->OutOfSyncRoleIndex) {

			// Only one unit can be out-of-sync.

			SetFlag( newRmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED );
		}

		if (ridx >= NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount) {

			SetFlag( newRmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_SPARE );
		}
	}

	SET_RMD_CRC( crc32_calc, newRmd );

	if (RtlCompareMemory(&newRmd, &NdasrArbitrator->Lurn->NdasrInfo->Rmd, sizeof(NDAS_RAID_META_DATA)) != sizeof(NDAS_RAID_META_DATA)) {
		
		newRmd.uiUSN++;

		SET_RMD_CRC( crc32_calc, newRmd );
		
		RtlCopyMemory( &NdasrArbitrator->Lurn->NdasrInfo->Rmd, &newRmd, sizeof(NDAS_RAID_META_DATA) );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Changing in memory RMD\n") );
	
		return TRUE;
	
	} else {
	
		SET_RMD_CRC( crc32_calc, newRmd );

		return FALSE;
	}
}

NTSTATUS
NdasRaidArbitratorWriteRmd (
	IN  PNDASR_ARBITRATOR		NdasrArbitrator,
	OUT PNDAS_RAID_META_DATA	Rmd
	)
{
	NTSTATUS status;
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("IN\n") );

	status = NdasRaidArbitratorWriteMetaSync( NdasrArbitrator, (PUCHAR) Rmd, (-1*NDAS_BLOCK_LOCATION_RMD_T), 1, TRUE );
	
	if (status != STATUS_SUCCESS) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed update second RMD\n") );		

		return status;
	}
		
	status = NdasRaidArbitratorWriteMetaSync( NdasrArbitrator, (PUCHAR) Rmd, (-1*NDAS_BLOCK_LOCATION_RMD), 1, TRUE );
	
	if (status != STATUS_SUCCESS) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed update first RMD\n") );		
	
		return status;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("OUT\n") );

	return status;
}

NTSTATUS
NdasRaidArbitratorWriteMetaSync (
	IN PNDASR_ARBITRATOR	NdasrArbitrator,
	IN PUCHAR				BlockBuffer,
	IN INT64				BlockAddr,
	IN UINT32				TrasferBlocks, 	// in sector
	IN BOOLEAN				RelativeAddress
	) 
 {
	NTSTATUS			status = STATUS_SUCCESS;
	
	UCHAR				ridx;
	PLURELATION_NODE	lurnChildren[MAX_NDASR_MEMBER_DISK] = {0};
	UCHAR				lurnCount;	
	UCHAR				lidx;

	NDAS_ASSERT( BlockAddr > 0 );
	NDAS_ASSERT( FlagOn(NdasrArbitrator->Lurn->AccessRight, GENERIC_WRITE) ||
					 NdasrArbitrator->Lurn->NdasrInfo->NdasrClient == NULL || 
					 FlagOn(((PNDASR_CLIENT)(NdasrArbitrator->Lurn->NdasrInfo->NdasrClient))->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE) );

	lurnCount = 0;

	for (ridx = 0; ridx < NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; ridx++) {
	
		if (ridx == NdasrArbitrator->OutOfSyncRoleIndex) {

			continue;
		}

		if (!FlagOn(NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", 
						 NdasrArbitrator->RoleToNodeMap[ridx], NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[ridx]], 
						 NdasrArbitrator->DefectCodes[NdasrArbitrator->RoleToNodeMap[ridx]]) );

			continue;
		}
			
		lurnChildren[lurnCount++] = NdasrArbitrator->Lurn->LurnChildren[NdasrArbitrator->RoleToNodeMap[ridx]];
	}

	if (NdasrArbitrator->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE) {
		
		if (!FlagOn(NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]], NRMX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", 
						 NdasrArbitrator->RoleToNodeMap[ridx], NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]], 
						 NdasrArbitrator->DefectCodes[NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]]) );
		
		} else {

			lurnChildren[lurnCount++] = 
				NdasrArbitrator->Lurn->LurnChildren[NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]];
		}
	}

	if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_NORMAL ||
		NdasrArbitrator->NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC) {

		NDAS_ASSERT( lurnCount == NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount );
	
	} else if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_DEGRADED) {

		NDAS_ASSERT( lurnCount == NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount - 1 );
	
	} else {

		NDAS_ASSERT( lurnCount < NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount - 1 );
	}
 
	if (lurnCount < NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount - 1) {

		NDAS_ASSERT( NDAS_ASSERT_NODE_UNRECHABLE );
		return STATUS_CLUSTER_NODE_UNREACHABLE;
	}

	for (ridx = NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; ridx < NdasrArbitrator->Lurn->LurnChildrenCnt; ridx++) {
	
		if (!FlagOn(NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[ridx]], NRMX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", 
						 NdasrArbitrator->RoleToNodeMap[ridx], NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[ridx]],
						 NdasrArbitrator->DefectCodes[NdasrArbitrator->RoleToNodeMap[ridx]]) );

			continue;
		}
					
		lurnChildren[lurnCount++] = NdasrArbitrator->Lurn->LurnChildren[NdasrArbitrator->RoleToNodeMap[ridx]];
	}

	// Flush all disk before updating metadata because updated metadata information is valid under assumption that all written data is on disk.
	
	for (lidx = 0; lidx < lurnCount; lidx++) { 

		PLURNEXT_IDE_DEVICE	ideDisk;	


		NDAS_ASSERT( NdasrArbitrator->NodeFlags[lurnChildren[lidx]->LurnChildIdx] == NRMX_NODE_FLAG_RUNNING );
		
		if (!LURN_IS_RUNNING(NdasrArbitrator->Lurn->LurnChildren[lurnChildren[lidx]->LurnChildIdx]->LurnStatus)) {
			
			//NDAS_ASSERT( !FlagOn(NdasrArbitrator->Lurn->NdasrInfo->LocalNodeFlags[lurnChildren[ridx]->LurnChildIdx], NRMX_NODE_FLAG_RUNNING) );
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)lurnChildren[lidx]->LurnExtension;

		if (ideDisk && ideDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_1_0) {

			// We already flushed when releasing lock.

			status = STATUS_SUCCESS;

		} else {

			status = NdasRaidLurnExecuteSynchrously( lurnChildren[lidx], 
													   SCSIOP_SYNCHRONIZE_CACHE,
													   FALSE,
													   FALSE,
													   NULL, 
													   0, 
													   0,
													   FALSE );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to flush node %d.\n", lurnChildren[lidx]->LurnChildIdx) );
			}
		}

		if (status == STATUS_SUCCESS) {

			KeEnterCriticalRegion();
			ExAcquireResourceExclusiveLite( &NdasrArbitrator->Lurn->NdasrInfo->BufLockResource, TRUE );

			status = NdasRaidLurnExecuteSynchrously( lurnChildren[lidx], 
													  SCSIOP_WRITE16,
													  TRUE,
													  TRUE,
													  BlockBuffer, 
													  BlockAddr, 
													  TrasferBlocks,
													  RelativeAddress );

			ExReleaseResourceLite( &NdasrArbitrator->Lurn->NdasrInfo->BufLockResource );
			KeLeaveCriticalRegion();

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to write node %d.\n", lurnChildren[lidx]->LurnChildIdx) );
			}
		}

		if (status != STATUS_SUCCESS) {
		
			break;
		}
	}
	
	return status;
}

NTSTATUS
NdasRaidArbitratorArrangeLockRange (
	PNDASR_ARBITRATOR				NdasrArbitrator,
	PNDASR_ARBITRATOR_LOCK_CONTEXT	Lock,
	UINT32							Granularity
	) 
{
	UNREFERENCED_PARAMETER( NdasrArbitrator );

	Lock->BlockAddress = (Lock->BlockAddress/Granularity) * Granularity;
	Lock->BlockLength  = Granularity;

	if ((Lock->BlockAddress + Lock->BlockLength) > NdasrArbitrator->Lurn->UnitBlocks) {

		Lock->BlockLength = (UINT32)(NdasrArbitrator->Lurn->UnitBlocks - Lock->BlockLength);

		NDAS_ASSERT( Lock->BlockLength < NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit );
	}

	NDAS_ASSERT( (Lock->BlockAddress + Lock->BlockLength) <= NdasrArbitrator->Lurn->UnitBlocks );

	return STATUS_SUCCESS;

#if 0

	UINT64	startAddr; // inclusive
	UINT32	blockLength;


	UNREFERENCED_PARAMETER( NdasrArbitrator );

	//if (Granularity < NDASR_LOCK_GRANULARITY_LOW_THRES) {
		
	//	Granularity = 1; // Too low Granularity. Go to exact match.
	//}

	// Use default Granularity without checking overlap with acquired lock

	startAddr = (Lock->BlockAddress /  Granularity) * Granularity;
	endAddr = ((Lock->BlockAddress + Lock->BlockLength-1)/Granularity + 1) * Granularity;
	length = (UINT32)(endAddr - startAddr);
 
	if (Lock->BlockLength != endAddr- startAddr) {

		DebugTrace(DBG_LURN_TRACE, ("Expanding lock range from %I64x:%x to %I64x:%x\n",
									 Lock->BlockAddress, Lock->BlockLength,
									 startAddr, endAddr-startAddr) );
	}

	Lock->BlockAddress = startAddr;
	Lock->BlockLength = (UINT32)(endAddr - startAddr);

	return STATUS_SUCCESS;
#endif
}

NTSTATUS
NdasRaidArbitratorUseSpareIfNeeded (
	IN  PNDASR_ARBITRATOR	NdasrArbitrator,
	OUT PBOOLEAN			SpareUsed
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;
	BOOLEAN		spareUsed = FALSE;
	UINT32		i;
	

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	
	// Check this degraded state is long enough to use spare.
		
	if (NdasrArbitrator->NdasrState == NRMX_RAID_STATE_DEGRADED) {
	
		static BOOLEAN	spareHoldingTimeoutMsgShown = FALSE;
		LARGE_INTEGER	currentTick;

		if (!FlagOn(NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex], NRMX_NODE_FLAG_DEFECTIVE)) {

			*SpareUsed = FALSE;
			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			return STATUS_SUCCESS;
		}

		KeQueryTickCount( &currentTick );
		
		if (((currentTick.QuadPart - NdasrArbitrator->DegradedSince.QuadPart) * KeQueryTimeIncrement()) < NDAS_RAID_SPARE_HOLDING_TIMEOUT) {
		
			if (!spareHoldingTimeoutMsgShown) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Fault device is not alive. %d sec left before using spare\n",
							 (ULONG)((NDAS_RAID_SPARE_HOLDING_TIMEOUT-((currentTick.QuadPart - NdasrArbitrator->DegradedSince.QuadPart) * KeQueryTimeIncrement())) / NANO100_PER_SEC)) );

				spareHoldingTimeoutMsgShown = TRUE;
			}

		} else {
			
			BOOLEAN spareFound = FALSE;
			UCHAR	spareNode = 0;

			spareHoldingTimeoutMsgShown = FALSE;
			
			// Change RAID maps to use spare disk and set all bitmap dirty.
			
			// Find running spare
			
			for (i = NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; i < NdasrArbitrator->Lurn->LurnChildrenCnt; i++) { // i is indexed by role
				
				if ((NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[i]] & NRMX_NODE_FLAG_RUNNING) && 
					!(NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[i]] & NRMX_NODE_FLAG_DEFECTIVE)) {

					spareFound = TRUE;
					spareNode = NdasrArbitrator->RoleToNodeMap[i];
					
					break;
				}
			}

			if (spareFound) {

				UCHAR spareRole, oosNode;
				
				// Swap defective disk with spare disk
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Running spare disk found. Swapping defective node %d(role %d) with spare %d\n",
							 NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex], NdasrArbitrator->OutOfSyncRoleIndex, spareNode) );

				NDAS_ASSERT( spareNode < NdasrArbitrator->Lurn->LurnChildrenCnt );
				NDAS_ASSERT( NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]] & NRMX_NODE_FLAG_STOP ||
								 NdasrArbitrator->NodeFlags[NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]] & NRMX_NODE_FLAG_DEFECTIVE );

				oosNode = NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex];
				spareRole = NdasrArbitrator->NodeToRoleMap[spareNode];

				NdasrArbitrator->NodeToRoleMap[oosNode] = spareRole;
				NdasrArbitrator->NodeToRoleMap[spareNode] = NdasrArbitrator->OutOfSyncRoleIndex;

				NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex] = spareNode;
				NdasrArbitrator->RoleToNodeMap[spareRole] = oosNode;
				
				// Replaced node is still out-of-sync. Keep Arbitrator->OutOfSyncRoleIndex
				
				NdasrArbitrator->NodeFlags[oosNode] |= NRMX_NODE_FLAG_DEFECTIVE;
				NdasrArbitrator->DefectCodes[oosNode] |= NRMX_NODE_DEFECT_REPLACED_BY_SPARE;
				
				// Set all bitmap dirty
	
				NdasRaidArbitratorChangeOosBitmapBit( NdasrArbitrator, TRUE, 0, NdasrArbitrator->Lurn->UnitBlocks );
				spareUsed = TRUE;
			
			} else {
			
				if (NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount < NdasrArbitrator->Lurn->LurnChildrenCnt) {

					DebugTrace( DBG_LURN_TRACE, ("Spare disk is not running\n") );
				
				} else {

				}
			}
		}
	}

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	if (spareUsed) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Creating new config set ID\n") );
		
		status = ExUuidCreate( &NdasrArbitrator->ConfigSetId );

		NDAS_ASSERT( status == STATUS_SUCCESS );
	}

	*SpareUsed = spareUsed;

	return status;
}

NTSTATUS
NdasRaidArbitratorFlushDirtyCacheNdas1_0 (
	IN PNDASR_ARBITRATOR		NdasrArbitrator,
	IN UINT64					LockId,
	IN PNDASR_CLIENT_CONTEXT	ClientContext
	) 
{
	KIRQL							oldIrql;
	PLIST_ENTRY						listEntry;
	PNDASR_ARBITRATOR_LOCK_CONTEXT	lock;
	ULONG							acquiredLockCount;
	ULONG							lockIte;
	NTSTATUS						status;

	struct _LockAddrLenPair {
	
		UINT64 LockAddress;	// in sector
		UINT32 LockLength;

	} *lockAddrLenPair;
	
	PLURELATION_NODE	lurn;
	PLURNEXT_IDE_DEVICE ideDisk;
	UINT32				i;
	

	status = STATUS_SUCCESS;
	
	for(i = 0; i < NdasrArbitrator->Lurn->LurnChildrenCnt; i++) {

		if ((NdasrArbitrator->NodeFlags[i] & NRMX_NODE_FLAG_DEFECTIVE) || !(NdasrArbitrator->NodeFlags[i] & NRMX_NODE_FLAG_RUNNING)) {
		
			continue;
		}

		if (LURN_STATUS_RUNNING != NdasrArbitrator->Lurn->LurnChildren[i]->LurnStatus) {
		
			continue;
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)NdasrArbitrator->Lurn->LurnChildren[i]->LurnExtension;
		
		if (ideDisk->LuHwData.HwVersion != LANSCSIIDE_VERSION_1_0) {
			
			continue;
		}
		
		ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );	
		
		lockAddrLenPair = ExAllocatePoolWithTag( NonPagedPool, 
												 NdasrArbitrator->AcquiredLockCount * sizeof(struct _LockAddrLenPair), 
												 NDASR_ARBITRATOR_LOCK_ADDRLEN_PAIR_POOL_TAG );

		if (lockAddrLenPair == NULL) {
			
			NDAS_ASSERT( FALSE );
			RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		acquiredLockCount = 0;
		
		for (listEntry = NdasrArbitrator->AcquiredLockList.Flink;
			 listEntry != &NdasrArbitrator->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			if (acquiredLockCount >= (ULONG)NdasrArbitrator->AcquiredLockCount) {
				
				NDAS_ASSERT( FALSE );
				break;
			}

			lock = CONTAINING_RECORD( listEntry, NDASR_ARBITRATOR_LOCK_CONTEXT, ArbitratorAcquiredLink );
			
			if (lock->Owner == ClientContext &&
				(lock->Id == LockId || lock->Id == NRMX_LOCK_ID_ALL)) {
				
				lockAddrLenPair[acquiredLockCount].LockAddress = lock->BlockAddress;
				lockAddrLenPair[acquiredLockCount].LockLength = lock->BlockLength;
				acquiredLockCount++;
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		status = STATUS_SUCCESS;

		lurn = NdasrArbitrator->Lurn->LurnChildren[i];
		
		for (lockIte = 0; lockIte<acquiredLockCount;lockIte++) {
		
			UINT64 lockAddr;
			UINT32 lockLength;
			static const UINT32 maxVerifyLength = 0x8000; // Limit due to CDB's Length bit size.

			for(lockAddr = lockAddrLenPair[lockIte].LockAddress; 
				lockAddr < lockAddrLenPair[lockIte].LockAddress + lockAddrLenPair[lockIte].LockLength;) {

				if (lockAddr + maxVerifyLength  > lockAddrLenPair[lockIte].LockAddress + lockAddrLenPair[lockIte].LockLength) {

					lockLength = (UINT32)(lockAddrLenPair[lockIte].LockAddress + lockAddrLenPair[lockIte].LockLength - lockAddr);
				
				} else {
				
					lockLength = maxVerifyLength;
				}

				DebugTrace( DBG_LURN_INFO, ("Flushing 1.0 HW range %I64x:%x.\n", lockAddr, lockLength ));

				status = NdasRaidLurnExecuteSynchrously( NdasrArbitrator->Lurn->LurnChildren[i], 
														 SCSIOP_VERIFY16,
														 FALSE,
														 FALSE,
														 NULL,
														 lockAddr,
														 lockLength, 
														 FALSE );

				if (!NT_SUCCESS(status)) {

					break;
				}

				lockAddr += lockLength;
			}

			if (!NT_SUCCESS(status)) {

				break;
			}
		}

		ExFreePoolWithTag( lockAddrLenPair, NDASR_ARBITRATOR_LOCK_ADDRLEN_PAIR_POOL_TAG );			
		
		if (!NT_SUCCESS(status)) {

			break;
		}
	}

	return status;
}

NTSTATUS 
NdasRaidRebuildInitiate (
	PNDASR_ARBITRATOR NdasrArbitrator
	) 
{
	NTSTATUS	status;

	KIRQL		oldIrql;
	UINT64		bitToRecovery;

	UINT64		blockAddr;
	UINT32		blockLength;

	NDAS_ASSERT( NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_NONE &&
					 NdasrArbitrator->NdasrRebuilder.RebuildRequested == FALSE );
	
	NDAS_ASSERT( NdasrArbitrator->NdasrRebuilder.RebuildLock == NULL );

	// Check rebuild thread is available 
	
	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	
	// Find set bits that is not locked by client.
	
	bitToRecovery = RtlFindSetBits( &NdasrArbitrator->OosBmpHeader, 1, 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("First set dirty bit: %x\n", bitToRecovery) );
	
	if (bitToRecovery == 0xFFFFFFFF || bitToRecovery >= NdasrArbitrator->OosBmpBitCount) {

		NDAS_ASSERT( RtlNumberOfSetBits(&NdasrArbitrator->OosBmpHeader) == 0 );

		// No region to recover. Finish recovery.
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("No set bit in OOS bitmap. Finishing recovery\n") );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Clearing Out of sync node mark from role %d(node %d)\n", 
					  NdasrArbitrator->OutOfSyncRoleIndex,
					  NdasrArbitrator->RoleToNodeMap[NdasrArbitrator->OutOfSyncRoleIndex]) );

		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Rebuilding completed\n") );

		return STATUS_SUCCESS;
	} 
	
	// Search and fire
	// Calc addr, length pair from bitmap and valid disk range.

	blockAddr = bitToRecovery * NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit; 
	blockLength = NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit;
			
	if ((blockAddr + blockLength) > NdasrArbitrator->Lurn->UnitBlocks) {
			
		blockLength = (UINT32)(NdasrArbitrator->Lurn->UnitBlocks - blockAddr);
				
		NDAS_ASSERT( blockLength < NdasrArbitrator->Lurn->NdasrInfo->BlocksPerBit );
	}

	NDAS_ASSERT( (blockAddr + blockLength) <= NdasrArbitrator->Lurn->UnitBlocks );

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

	NdasrArbitrator->NdasrRebuilder.RebuildLock = NdasRaidArbitratorAllocLock( NdasrArbitrator, 
																		  NRMX_LOCK_TYPE_BLOCK_IO,
																		  NRMX_LOCK_MODE_SH,
																		  blockAddr, 
																		  blockLength ); 
					
	if (!NdasrArbitrator->NdasrRebuilder.RebuildLock) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	status = NdasRaidArbitratorArrangeLockRange( NdasrArbitrator,
											   NdasrArbitrator->NdasrRebuilder.RebuildLock, 
											   NdasrArbitrator->LockRangeGranularity );

	// Add lock to acquired lock list
					
	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
					
	NdasrArbitrator->NdasrRebuilder.RebuildLock->Status = NDASR_ARBITRATOR_LOCK_STATUS_GRANTED;
				
	InsertTailList( &NdasrArbitrator->AcquiredLockList, &NdasrArbitrator->NdasrRebuilder.RebuildLock->ArbitratorAcquiredLink );
	InterlockedIncrement( &NdasrArbitrator->AcquiredLockCount );
					
	// DebugTrace(NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbitrator->AcquiredLockCount %d\n",Arbitrator->AcquiredLockCount));	
				
	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
					
	// Update LWR before doing IO.
					
	NdasRaidArbitratorUpdateLwrBitmapBit( NdasrArbitrator, NdasrArbitrator->NdasrRebuilder.RebuildLock, NULL );
					
	status = NdasRaidArbitratorUpdateOnDiskOosBitmap( NdasrArbitrator, FALSE );

	if (status != STATUS_SUCCESS) {
					
		RemoveEntryList( &NdasrArbitrator->NdasrRebuilder.RebuildLock->ArbitratorAcquiredLink );
		InitializeListHead( &NdasrArbitrator->NdasrRebuilder.RebuildLock->ArbitratorAcquiredLink );
		InterlockedDecrement( &NdasrArbitrator->AcquiredLockCount );

		NdasRaidArbitratorFreeLock( NdasrArbitrator, NdasrArbitrator->NdasrRebuilder.RebuildLock );

		NdasrArbitrator->NdasrRebuilder.RebuildLock = NULL;

		NdasrArbitrator->NdasrRebuilder.RebuildRequested = FALSE;
		NdasrArbitrator->NdasrRebuilder.CancelRequested = FALSE;

		NdasrArbitrator->NdasrRebuilder.Status = NDASR_REBUILD_STATUS_NONE;

		RELEASE_SPIN_LOCK(&NdasrArbitrator->SpinLock, oldIrql);

		NdasRaidArbitratorUpdateLwrBitmapBit( NdasrArbitrator, NULL, NULL );

		return status;
	}

	status = NdasRaidRebuildRequest( &NdasrArbitrator->NdasrRebuilder, blockAddr, blockLength );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	return status;
}

NTSTATUS
NdasRaidRebuildAcknowledge (
	PNDASR_ARBITRATOR NdasrArbitrator
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;


	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	
	if (NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_SUCCESS) {
	
		status = STATUS_SUCCESS;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Rebuilding range %I64x:%x is done\n", 
					 NdasrArbitrator->NdasrRebuilder.BlockAddress, NdasrArbitrator->NdasrRebuilder.BlockLength) );
		
	} else if (NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_FAILED) {

		status = NdasrArbitrator->NdasrRebuilder.RebuildStatus;

		NDAS_ASSERT( !NT_SUCCESS(status) );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Rebuilding range %I64x:%x is failed\n", 
					 NdasrArbitrator->NdasrRebuilder.BlockAddress, NdasrArbitrator->NdasrRebuilder.BlockLength) );

	} else if (NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_CANCELLED) {
		
		status = NdasrArbitrator->NdasrRebuilder.RebuildStatus;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Rebuilding range %I64x:%x is canceled\n", 
					 NdasrArbitrator->NdasrRebuilder.BlockAddress, NdasrArbitrator->NdasrRebuilder.BlockLength) );
	
	} else {

		NDAS_ASSERT( FALSE );
		RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );

		return STATUS_SUCCESS;
	}

	if (NdasrArbitrator->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_SUCCESS) {
			
		NdasRaidArbitratorChangeOosBitmapBit( NdasrArbitrator, 
											FALSE, 
											NdasrArbitrator->NdasrRebuilder.BlockAddress, 
											NdasrArbitrator->NdasrRebuilder.BlockLength );
	}

	// Remove from acquired lock list.

	RemoveEntryList( &NdasrArbitrator->NdasrRebuilder.RebuildLock->ArbitratorAcquiredLink );
	InitializeListHead( &NdasrArbitrator->NdasrRebuilder.RebuildLock->ArbitratorAcquiredLink );
	InterlockedDecrement( &NdasrArbitrator->AcquiredLockCount );

	NdasRaidArbitratorFreeLock( NdasrArbitrator, NdasrArbitrator->NdasrRebuilder.RebuildLock );

	NdasrArbitrator->NdasrRebuilder.RebuildLock = NULL;

	NdasrArbitrator->NdasrRebuilder.RebuildRequested = FALSE;
	NdasrArbitrator->NdasrRebuilder.CancelRequested = FALSE;
	NdasrArbitrator->NdasrRebuilder.RebuildStatus = STATUS_SUCCESS;

	NdasrArbitrator->NdasrRebuilder.Status = NDASR_REBUILD_STATUS_NONE;

	RELEASE_SPIN_LOCK(&NdasrArbitrator->SpinLock, oldIrql);

	NdasRaidArbitratorUpdateLwrBitmapBit( NdasrArbitrator, NULL, NULL );
	
	if (status == STATUS_SUCCESS) {

		status = NdasRaidArbitratorUpdateOnDiskOosBitmap( NdasrArbitrator, FALSE );
	}

	return status;
}

NTSTATUS
NdasRaidRebuildStart (
	PNDASR_ARBITRATOR NdasrArbitrator
	)
{
	NTSTATUS			status;

	PNDASR_REBUILDER	ndasrRebuilder = &NdasrArbitrator->NdasrRebuilder;
	OBJECT_ATTRIBUTES	objectAttributes;
	UCHAR				i;


	RtlZeroMemory( ndasrRebuilder, sizeof(NDASR_REBUILDER) );
	ndasrRebuilder->NdasrArbitrator = NdasrArbitrator;

	KeInitializeEvent( &ndasrRebuilder->ThreadEvent, NotificationEvent, FALSE );
	KeInitializeEvent( &ndasrRebuilder->ThreadReadyEvent, NotificationEvent, FALSE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	ndasrRebuilder->Status = NDASR_REBUILD_STATUS_NONE;

	ndasrRebuilder->RebuildLock		= NULL;
	ndasrRebuilder->ExitRequested	= FALSE;
	ndasrRebuilder->CancelRequested = FALSE;
	ndasrRebuilder->UnitRebuildSize = NDASR_REBUILD_BUFFER_SIZE;

	do {

		status = STATUS_SUCCESS;

		for (i=0; i<NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; i++) {
		
			ndasrRebuilder->RebuildBuffer[i] = ExAllocatePoolWithTag( NonPagedPool, 
																	  ndasrRebuilder->UnitRebuildSize, 
																	  NDASR_REBUILD_BUFFER_POOL_TAG );
		
			if (!ndasrRebuilder->RebuildBuffer[i]) {
			
				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
				status = STATUS_INSUFFICIENT_RESOURCES;
				
				break;
			}
		}

		if (status != STATUS_SUCCESS) {

			break;
		} 

		status = PsCreateSystemThread( &ndasrRebuilder->ThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   NdasRaidRebuildThreadProc,
									   ndasrRebuilder );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );
			break;
		}

		status = ObReferenceObjectByHandle( ndasrRebuilder->ThreadHandle,
											GENERIC_ALL,
											NULL,
											KernelMode,
											&ndasrRebuilder->ThreadObject,
											NULL );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );
			break;
		}

		status = KeWaitForSingleObject( &ndasrRebuilder->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {
	
			NDAS_ASSERT( FALSE );
			break;
		}

	} while (0);

	if (status == STATUS_SUCCESS) {

		return status;
	}

	for (i=0; i<NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; i++) {
	
		if (ndasrRebuilder->RebuildBuffer[i]) {
		
			ExFreePoolWithTag( ndasrRebuilder->RebuildBuffer[i], NDASR_REBUILD_BUFFER_POOL_TAG );
		}
	}

	return status;
}

NTSTATUS
NdasRaidRebuildStop (
	PNDASR_ARBITRATOR NdasrArbitrator
	)
{
	NTSTATUS			status;
	
	PNDASR_REBUILDER	ndasrRebuilder = &NdasrArbitrator->NdasrRebuilder;
	KIRQL				oldIrql;
	UCHAR				i;
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Stopping Rebuild thread\n") );
	
	ACQUIRE_SPIN_LOCK( &NdasrArbitrator->SpinLock, &oldIrql );
	
	ndasrRebuilder->ExitRequested = TRUE;
	KeSetEvent( &ndasrRebuilder->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &NdasrArbitrator->SpinLock, oldIrql );
	
	NDAS_ASSERT( ndasrRebuilder->ThreadObject );

	if (ndasrRebuilder->ThreadObject) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Wait for rebuild IO thread completion\n") );

		status = KeWaitForSingleObject( ndasrRebuilder->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );
		
		ObDereferenceObject( ndasrRebuilder->ThreadObject );
		ZwClose( ndasrRebuilder->ThreadHandle );
	}

	for (i=0; i<NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; i++) {
	
		if (ndasrRebuilder->RebuildBuffer[i]) {

			ExFreePoolWithTag( ndasrRebuilder->RebuildBuffer[i], NDASR_REBUILD_BUFFER_POOL_TAG );
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidRebuildRequest (
	IN PNDASR_REBUILDER	NdasrRebuilder,
	IN UINT64			BlockAddr,
	IN UINT32			BlockLength
	)
{
	KIRQL				oldIrql;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Requesting to rebuild range %I64x:%x\n", BlockAddr, BlockLength) );

	ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, &oldIrql );

	NDAS_ASSERT( NdasrRebuilder->Status == NDASR_REBUILD_STATUS_NONE || NdasrRebuilder->RebuildRequested );
	
	NdasrRebuilder->BlockAddress		= BlockAddr;
	NdasrRebuilder->BlockLength			= BlockLength;
	NdasrRebuilder->RebuildRequested	= TRUE;

	KeSetEvent( &NdasrRebuilder->ThreadEvent, IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}

#define NDASR_REBUILD_REST_INTERVAL 2048					// number of IO blocks wait. Wait every 1Mbyte
#define NDASR_REBUILD_REST_TIME	((NANO100_PER_SEC/1000)*10)	// 10 msec

VOID
NdasRaidRebuildThreadProc (
	IN PNDASR_REBUILDER	NdasrRebuilder
	)
{
	NTSTATUS	status;
	KIRQL		oldIrql;

	// Set lower priority.
	
	KeSetBasePriorityThread( KeGetCurrentThread(), -1 );

	KeSetEvent( &NdasrRebuilder->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );
	
	do {
	
		DebugTrace( DBG_LURN_TRACE, ("Waiting rebuild request\n") );
		
		status = KeWaitForSingleObject( &NdasrRebuilder->ThreadEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		KeClearEvent( &NdasrRebuilder->ThreadEvent );

		ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, &oldIrql );
		
		if (NdasrRebuilder->ExitRequested) {
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Rebuild exit requested\n") );

			RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );			
			break;
		}

		if (NdasrRebuilder->RebuildRequested) {	
			
			UINT64 startBlockAddr;
			UINT32 blockLength;
			UINT32 restInterval;
			
			NDAS_ASSERT( NdasrRebuilder->BlockLength );
			NDAS_ASSERT( NdasrRebuilder->Status == NDASR_REBUILD_STATUS_NONE );
			
			// Rebuild request should be called only when previous result is cleared.
			
			NdasrRebuilder->Status = NDASR_REBUILD_STATUS_WORKING;
			
			RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Starting rebuild IO from %I64x to %x\n", NdasrRebuilder->BlockAddress,NdasrRebuilder->BlockLength) );
	
			if (NdasrRebuilder->BlockLength == 0) {

				NDAS_ASSERT( FALSE );

				NdasrRebuilder->Status = NDASR_REBUILD_STATUS_SUCCESS;
				KeSetEvent( &NdasrRebuilder->NdasrArbitrator->ThreadEvent, IO_NO_INCREMENT, FALSE );

				NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

				continue;
			}

			restInterval = 0;
			startBlockAddr = NdasrRebuilder->BlockAddress;
			blockLength = NdasrRebuilder->BlockLength;
			
			status = STATUS_SUCCESS;

			do {
	
				if (blockLength > NdasrRebuilder->UnitRebuildSize / SECTOR_SIZE) {

					blockLength = NdasrRebuilder->UnitRebuildSize / SECTOR_SIZE;
				
				}

				status = NdasRaidRebuildDoIo( NdasrRebuilder, startBlockAddr, blockLength );

				if (status != STATUS_SUCCESS) {

					break;
				}

				restInterval += blockLength;
				startBlockAddr += blockLength;
				
				blockLength = NdasrRebuilder->BlockLength - (UINT32)(startBlockAddr - NdasrRebuilder->BlockAddress);

				if (blockLength == 0) {

					break;
				}

				if (restInterval > NDASR_REBUILD_REST_INTERVAL) {

					LARGE_INTEGER restTime;

					restTime.QuadPart = -NDASR_REBUILD_REST_TIME;
					//KeDelayExecutionThread(KernelMode, FALSE, &restTime);
					
					restInterval = 0;
				}
					
				if (NdasrRebuilder->CancelRequested || NdasrRebuilder->ExitRequested) {
				
					break;
				}		
			
			} while (TRUE);
			
			ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, &oldIrql );

			NdasrRebuilder->RebuildRequested = FALSE;

			if (NdasrRebuilder->ExitRequested) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Exit requested. Rebuilding range %I64x:%x cancelled\n", NdasrRebuilder->BlockAddress, NdasrRebuilder->BlockLength) );
				
				NdasrRebuilder->Status = NDASR_REBUILD_STATUS_CANCELLED;
				
				break;
			} 

			if (NdasrRebuilder->CancelRequested) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Rebuilding range %I64x:%x cancelled\n", NdasrRebuilder->BlockAddress, NdasrRebuilder->BlockLength) );

				status = STATUS_CANCELLED;
			}
					
			if (status == STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Rebuilding range %I64x:%x done\n", NdasrRebuilder->BlockAddress, NdasrRebuilder->BlockLength) );
				
				NdasrRebuilder->Status = NDASR_REBUILD_STATUS_SUCCESS;

			} else if (status == STATUS_CANCELLED) {
			
				NdasrRebuilder->Status = NDASR_REBUILD_STATUS_CANCELLED;

			} else {
			
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Rebuilding range %I64x:%x failed\n", NdasrRebuilder->BlockAddress, NdasrRebuilder->BlockLength) );
				
				NdasrRebuilder->Status = NDASR_REBUILD_STATUS_FAILED;
			}

			// Notify arbiter.

			KeSetEvent( &NdasrRebuilder->NdasrArbitrator->ThreadEvent, IO_NO_INCREMENT, FALSE );
		}

		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	} while (TRUE);

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );

	NDAS_ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );
	
	PsTerminateSystemThread( STATUS_SUCCESS );
}

#define NDASR_ADDR_REQUIRES_CDB16(BlockAddr) (BlockAddr >= 0x100000000L)

NTSTATUS 
NdasRaidRebuildDoIo (
	PNDASR_REBUILDER	NdasrRebuilder,
	UINT64				BlockAddr,
	UINT32 				BlockLength
	) 
{
	NTSTATUS			status;

	PNDASR_INFO			ndasrInfo = NdasrRebuilder->NdasrArbitrator->Lurn->NdasrInfo;

	KIRQL				oldIrql;
	PLURELATION_NODE	lurn;
	UCHAR				ridx;
	UCHAR				outOfSyncRoleIndex;
	PLURNEXT_IDE_DEVICE ideDisk;

	BOOLEAN				lockAquired[MAX_NDASR_MEMBER_DISK] = {FALSE};


	// Check current RAID status is suitable.
	
	ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, &oldIrql );

	// Check cancel or terminate request is sent
	
	if (NdasrRebuilder->CancelRequested || NdasrRebuilder->ExitRequested) {
	
		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );
		
		NDAS_ASSERT( FALSE );

		DebugTrace( DBG_LURN_INFO, ("Rebuild IO is cancelled or exited.\n") );		
		
		return STATUS_UNSUCCESSFUL;
	}

	if (NdasrRebuilder->NdasrArbitrator->NdasrState != NRMX_RAID_STATE_OUT_OF_SYNC) {

		NDAS_ASSERT( FALSE );

		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );
		
		return STATUS_UNSUCCESSFUL;
	}

	if (NdasrRebuilder->NdasrArbitrator->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {

		NDAS_ASSERT( FALSE );

		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );
		
		return STATUS_UNSUCCESSFUL;
	}
		
	lurn = NdasrRebuilder->NdasrArbitrator->Lurn;
	outOfSyncRoleIndex = NdasrRebuilder->NdasrArbitrator->OutOfSyncRoleIndex;

	RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbitrator->SpinLock, oldIrql );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &lurn->NdasrInfo->BufLockResource, TRUE );

	do {

		LONG	k;

		ridx = 0;

		while (ridx < NdasrRebuilder->NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount) { 

			UCHAR				ccbStatus;

			status = NdasRaidLurnLockSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[ridx]],
												   LURNDEVLOCK_ID_BUFFLOCK,
												   NDSCLOCK_OPCODE_ACQUIRE,
												   &ccbStatus );
		

			if (status == STATUS_LOCK_NOT_GRANTED) {

				if (ccbStatus == CCB_STATUS_LOST_LOCK) {

					status = NdasRaidLurnLockSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[ridx]],
														   LURNDEVLOCK_ID_BUFFLOCK,
														   NDSCLOCK_OPCODE_RELEASE,
														   &ccbStatus );

					if (status == STATUS_SUCCESS) {

						continue;
					}
				}

				NDAS_ASSERT( FALSE );
			}

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				break;
			}
			
			lockAquired[ridx] = TRUE;

			ideDisk = (PLURNEXT_IDE_DEVICE)NdasrRebuilder->NdasrArbitrator->Lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[ridx]]->LurnExtension;
			NDAS_ASSERT( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == TRUE );
			ridx++;
		}

		if (status != STATUS_SUCCESS) {

			break;
		}

		for (ridx = 0; ridx < NdasrRebuilder->NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

			PCHAR	tempBuffer;

			if (ridx == outOfSyncRoleIndex) {

				continue;
			}

			status = NdasRaidLurnExecuteSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[ridx]], 
													  SCSIOP_READ16,
													  FALSE,
													  FALSE,
													  (PUCHAR)NdasrRebuilder->RebuildBuffer[ridx], 
													  BlockAddr, 
													  BlockLength,
													  FALSE );
	
			if (status != STATUS_SUCCESS) {
	
				break;
			}

			if (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount == 1) {	// LURN_NDASR_RAID1

				tempBuffer = NdasrRebuilder->RebuildBuffer[ridx];
				NdasrRebuilder->RebuildBuffer[ridx] = NdasrRebuilder->RebuildBuffer[outOfSyncRoleIndex];
				NdasrRebuilder->RebuildBuffer[outOfSyncRoleIndex] = tempBuffer;
			}
		}

		if (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount != 1) { // trick for LURN_NDASR_RAID1

			UCHAR	nextChild;
			
			if (outOfSyncRoleIndex == 0) {

				nextChild = 1;
					
			} else {

				nextChild = 0;
			}

			RtlCopyMemory( NdasrRebuilder->RebuildBuffer[outOfSyncRoleIndex],
						   NdasrRebuilder->RebuildBuffer[nextChild],
						   NdasrRebuilder->NdasrArbitrator->Lurn->ChildBlockBytes * BlockLength );

			nextChild ++;

			for (ridx = nextChild; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

				if (ridx == outOfSyncRoleIndex) {

					continue;
				}

				for (k = 0; k < NdasrRebuilder->NdasrArbitrator->Lurn->ChildBlockBytes * BlockLength; k ++) {

					NdasrRebuilder->RebuildBuffer[outOfSyncRoleIndex][k] ^=
						NdasrRebuilder->RebuildBuffer[ridx][k];									
				}
			}
		}

		if (status != STATUS_SUCCESS) {

			DebugTrace( DBG_LURN_ERROR, ("Failed to read from healthy disk. RAID failure\n") );

			break;
		}

		// WRITE sectors to the out-of-sync LURN

		status = NdasRaidLurnExecuteSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[outOfSyncRoleIndex]], 
												  SCSIOP_WRITE16,
												  FALSE,
												  FALSE,
												  NdasrRebuilder->RebuildBuffer[outOfSyncRoleIndex], 
												  BlockAddr, 
												  BlockLength,
												  FALSE );

		if (status != STATUS_SUCCESS) {

			DebugTrace( DBG_LURN_INFO, ("Failed to write to out-of-sync disk\n") );
		}
	
	} while (0);

	for (ridx = 0; ridx < NdasrRebuilder->NdasrArbitrator->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

		NTSTATUS status2;

		if (lockAquired[ridx] == TRUE) {

			status2 = NdasRaidLurnLockSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[ridx]],
												    LURNDEVLOCK_ID_BUFFLOCK,
												    NDSCLOCK_OPCODE_RELEASE,
													NULL );
	
			NDAS_ASSERT( status2 == STATUS_SUCCESS || status2 == STATUS_CLUSTER_NODE_UNREACHABLE );

			if (status2 == STATUS_SUCCESS) {

				ideDisk = (PLURNEXT_IDE_DEVICE)NdasrRebuilder->NdasrArbitrator->Lurn->LurnChildren[NdasrRebuilder->NdasrArbitrator->RoleToNodeMap[ridx]]->LurnExtension;
#if !__NDAS_SCSI_LOCK_BUG_AVOID__
				NDAS_ASSERT( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == FALSE );
#endif
			}

		}
	}

	ExReleaseResourceLite( &lurn->NdasrInfo->BufLockResource );
	KeLeaveCriticalRegion();

	NdasrRebuilder->RebuildStatus = status;

	return status;
}
