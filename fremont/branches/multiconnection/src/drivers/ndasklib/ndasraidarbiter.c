#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasraidArbiter"

VOID
NdasRaidArbiterThreadProc (
	IN PNDASR_ARBITER	NdasrArbiter
	);

NTSTATUS
NdasRaidArbiterAcceptClient (
	PNDASR_ARBITER				NdasArbiter,
	PNRIX_REGISTER				RegisterMsg,
	PNDASR_ARBITER_CONNECTION	*Connection
	);

NTSTATUS
NdasRaidArbiterRegisterNewClient ( 
	IN PNDASR_ARBITER NdasrArbiter
	);

NTSTATUS
NdasRaidArbiterCheckRequestMsg (
	IN PNDASR_ARBITER	NdasrArbiter
	); 

NTSTATUS
NdasRaidArbiterHandleRequestMsg (
	PNDASR_ARBITER			NdasrArbiter, 
	PNDASR_CLIENT_CONTEXT	Client,
	PNRIX_HEADER			Message
	);

NTSTATUS
NdasRaidReplyChangeStatus (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PNRIX_HEADER			RequestMsg,
	UCHAR					Result
	);

PNDASR_CLIENT_CONTEXT 
NdasRaidArbiterAllocClientContext (
	VOID
	);

VOID
NdasRaidArbiterTerminateClient (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	Client,
	PBOOLEAN				OosBitmapSet
	); 

NTSTATUS 
NdasRaidArbiterInitializeOosBitmap (
	IN PNDASR_ARBITER	NdasrArbiter,
	IN PBOOLEAN			NodeIsUptoDate,
	IN UCHAR			UpToDateNode
	); 

VOID
NdasRaidArbiterChangeOosBitmapBit (
	PNDASR_ARBITER	NdasrArbiter,
	BOOLEAN			Set,	// TRUE for set, FALSE for clear
	UINT64			Addr,
	UINT64			Length
	);

VOID
NdasRaidArbiterUpdateLwrBitmapBit (
	PNDASR_ARBITER				NdasrArbiter,
	PNDASR_ARBITER_LOCK_CONTEXT HintAddedLock,
	PNDASR_ARBITER_LOCK_CONTEXT HintRemovedLock
	); 

NTSTATUS 
NdasRaidArbiterUpdateOnDiskOosBitmap (
	PNDASR_ARBITER	NdasrArbiter,
	BOOLEAN			UpdateAll
	); 

BOOLEAN
NdasRaidArbiterUpdateInCoreRmd (
	IN PNDASR_ARBITER	NdasrArbiter
	); 

NTSTATUS
NdasRaidArbiterWriteRmd (
	IN PNDASR_ARBITER	 Arbiter,
	OUT PNDAS_RAID_META_DATA Rmd
	);

NTSTATUS
NdasRaidArbiterWriteMetaSync (
	IN PNDASR_ARBITER	NdasrArbiter,
	IN PUCHAR			BlockBuffer,
	IN INT64			BlockAddr,
	IN UINT32			TrasferBlocks, 
	IN BOOLEAN			RelativeAddress
	); 

NTSTATUS
NdasRaidNotifyChangeStatus (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext
	);

NTSTATUS
NdasRaidNotifyRetire (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext
	);

NTSTATUS
NdasRaidArbiterArrangeLockRange (
	PNDASR_ARBITER				NdasrArbiter,
	PNDASR_ARBITER_LOCK_CONTEXT Lock,
	UINT32						Granularity
	); 

BOOLEAN
NdasRaidArbiterRefreshRaidStatus (
	IN PNDASR_ARBITER	NdasrArbiter,
	IN BOOLEAN			ForceChange
	);

NTSTATUS
NdasRaidArbiterUseSpareIfNeeded (
	IN  PNDASR_ARBITER	NdasrArbiter,
	OUT PBOOLEAN		SpareUsed
	);

NTSTATUS
NdasRaidArbiterFlushDirtyCacheNdas1_0 (
	IN PNDASR_ARBITER			NdasrArbiter,
	IN UINT64					LockId,
	IN PNDASR_CLIENT_CONTEXT	ClientContext
	); 

NTSTATUS 
NdasRaidRebuildInitiate (
	PNDASR_ARBITER NdasrArbiter
	); 

NTSTATUS
NdasRaidRebuildAcknowledge (
	PNDASR_ARBITER NdasrArbiter
	); 

NTSTATUS
NdasRaidRebuildStart (
	PNDASR_ARBITER NdasrArbiter
	);

NTSTATUS
NdasRaidRebuildStop (
	PNDASR_ARBITER NdasrArbiter
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


PNDASR_ARBITER_LOCK_CONTEXT
NdasRaidArbiterAllocLock (
	PNDASR_ARBITER	NdasrArbiter,
	UCHAR			LockType,
	UCHAR 			LockMode,
	UINT64			Addr,
	UINT32			Length
	) 
{
	PNDASR_ARBITER_LOCK_CONTEXT lock;
	KIRQL						oldIrql;

	lock = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_ARBITER_LOCK_CONTEXT), NDASR_ARBITER_LOCK_POOL_TAG );
	
	if (lock == NULL) {
	
		NDASSCSI_ASSERT( FALSE );
		return NULL;
	}

	RtlZeroMemory( lock, sizeof(NDASR_ARBITER_LOCK_CONTEXT) );

	lock->Status		= NDASR_ARBITER_LOCK_STATUS_NONE;
	lock->Type			= LockType;
	lock->Mode			= LockMode;
	lock->BlockAddress	= Addr;	
	lock->BlockLength	= Length;
	lock->Granularity	= NdasrArbiter->LockRangeGranularity;
	
	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	
	lock->Id = NdasrArbiter->NextLockId;
	NdasrArbiter->NextLockId++;	
	
	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );		

	InitializeListHead( &lock->ClientAcquiredLink );
	InitializeListHead( &lock->ArbiterAcquiredLink );
	
	lock->Owner = NULL;

	return lock;
}

VOID
NdasRaidArbiterFreeLock (
	PNDASR_ARBITER				NdasrArbiter,
	PNDASR_ARBITER_LOCK_CONTEXT Lock
	) 
{
	UNREFERENCED_PARAMETER( NdasrArbiter );

	NDASSCSI_ASSERT( IsListEmpty(&Lock->ClientAcquiredLink) );
	NDASSCSI_ASSERT( IsListEmpty(&Lock->ArbiterAcquiredLink) );

	NDASSCSI_ASSERT( Lock->Owner == NULL );

	ExFreePoolWithTag( Lock, NDASR_ARBITER_LOCK_POOL_TAG );
}

NTSTATUS 
NdasRaidArbiterStart (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status;

	PNDASR_INFO			ndasrInfo = Lurn->NdasrInfo;
	PNDASR_ARBITER		ndasrArbiter;
	KIRQL				oldIrql, oldIrql2;
	UCHAR				nidx, ridx;
	OBJECT_ATTRIBUTES	objectAttributes;
	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasRaidArbiterStart\n") );

	if (ndasrInfo->NdasrArbiter) {
	
		NDASSCSI_ASSERT( FALSE );
		return STATUS_SUCCESS;
	}
	
	ndasrInfo->NdasrArbiter = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_ARBITER), NDASR_ARBITER_POOL_TAG );

	if (ndasrInfo->NdasrArbiter == NULL) {
		
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ndasrArbiter = ndasrInfo->NdasrArbiter;

	RtlZeroMemory( ndasrArbiter, sizeof(NDASR_ARBITER) );

	InitializeListHead( &ndasrArbiter->AllListEntry );

	ndasrArbiter->Lurn = Lurn;

	KeInitializeSpinLock( &ndasrArbiter->SpinLock );

	ndasrArbiter->Status = NDASR_ARBITER_STATUS_INITIALIZING;

	KeInitializeEvent( &ndasrArbiter->FinishShutdownEvent, NotificationEvent, FALSE );

	ndasrArbiter->NdasrState = NRIX_RAID_STATE_INITIALIZING;

	InitializeListHead( &ndasrArbiter->NewClientQueue );
	InitializeListHead( &ndasrArbiter->ClientQueue );
	InitializeListHead( &ndasrArbiter->TerminatedClientQueue );

	InitializeListHead( &ndasrArbiter->AcquiredLockList );	
	ndasrArbiter->AcquiredLockCount	= 0;

	ndasrArbiter->LockRangeGranularity = ndasrInfo->BlocksPerBit; // Set to sector per bit for time being..

	ndasrArbiter->OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;
	RtlCopyMemory( &ndasrArbiter->ConfigSetId, &ndasrArbiter->Lurn->NdasrInfo->ConfigSetId, sizeof(GUID) );	

	ndasrArbiter->AcceptClient = NdasRaidArbiterAcceptClient;

	do {
	
		// 1. Set initial ndasrArbiter flags. 
	 
		ACQUIRE_SPIN_LOCK( &ndasrArbiter->SpinLock, &oldIrql );	
	
		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {

			ACQUIRE_SPIN_LOCK( &Lurn->LurnChildren[nidx]->SpinLock, &oldIrql2 );

			ndasrArbiter->NodeFlags[nidx] = NdasRaidLurnStatusToNodeFlag( Lurn->LurnChildren[nidx]->LurnStatus );

			if (LurnGetCauseOfFault(Lurn->LurnChildren[nidx]) & (LURN_FCAUSE_BAD_SECTOR | LURN_FCAUSE_BAD_DISK)) {

				ndasrArbiter->NodeFlags[nidx] = NRIX_NODE_FLAG_DEFECTIVE;
				ndasrArbiter->DefectCodes[nidx] = ((LurnGetCauseOfFault(Lurn->LurnChildren[nidx]) & LURN_FCAUSE_BAD_SECTOR) ?
												NRIX_NODE_DEFECT_BAD_SECTOR : NRIX_NODE_DEFECT_BAD_DISK );
			}

			DebugTrace(NDASSCSI_DBG_LURN_NDASR_ERROR, ("Setting initial node %d flag: %d\n", nidx, ndasrArbiter->NodeFlags[nidx]) );

			RELEASE_SPIN_LOCK( &Lurn->LurnChildren[nidx]->SpinLock, oldIrql2 );
		}

		RELEASE_SPIN_LOCK( &ndasrArbiter->SpinLock, oldIrql );

		// 2. Map children based on RMD

		ACQUIRE_SPIN_LOCK( &ndasrArbiter->SpinLock, &oldIrql );	

		for (ridx = 0; ridx < Lurn->LurnChildrenCnt; ridx++) { 

			NDASSCSI_ASSERT( ndasrArbiter->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx < Lurn->LurnChildrenCnt );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("MAPPING Lurn node %d to RAID role %d\n", ndasrArbiter->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx, ridx) );

			ndasrArbiter->RoleToNodeMap[ridx] = (UCHAR)ndasrArbiter->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx;
			ndasrArbiter->NodeToRoleMap[ndasrArbiter->RoleToNodeMap[ridx]] = (UCHAR)ridx;	
		}

		// 3. Apply node information from RMD
		
		for (ridx = 0; ridx < Lurn->LurnChildrenCnt; ridx++) { // i : role index. 
		
			UCHAR unitDeviceStatus = ndasrArbiter->Lurn->NdasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus;
		
			if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

				NDASSCSI_ASSERT( ndasrArbiter->Lurn->NdasrInfo->NodeIsUptoDate[ndasrArbiter->RoleToNodeMap[ridx]] == FALSE );
				if (ridx < ndasrArbiter->Lurn->NdasrInfo->ActiveDiskCount) {

					NDASSCSI_ASSERT( !FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) );
					NDASSCSI_ASSERT( ndasrArbiter->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE );

					ndasrArbiter->OutOfSyncRoleIndex = (UCHAR)ridx;

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is out-of-sync\n",  ndasrArbiter->RoleToNodeMap[ridx], ridx) );
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting out of sync role: %d\n", ndasrArbiter->OutOfSyncRoleIndex) );
				}
			}

			if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

				NDASSCSI_ASSERT( ndasrArbiter->Lurn->NdasrInfo->NodeIsUptoDate[ndasrArbiter->RoleToNodeMap[ridx]] == FALSE );
				NDASSCSI_ASSERT( ridx >= ndasrArbiter->Lurn->NdasrInfo->ActiveDiskCount );

				SetFlag( ndasrArbiter->NodeFlags[ndasrArbiter->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_DEFECTIVE );
		
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Node %d(role %d) is defective\n",  ndasrArbiter->RoleToNodeMap[ridx], ridx) );
			
				SetFlag( ndasrArbiter->DefectCodes[ndasrArbiter->RoleToNodeMap[ridx]], 
						 NdasRaidRmdUnitStatusToDefectCode(unitDeviceStatus) );
			}

			//NDASSCSI_ASSERT( nodeIsUptoDate[ndasrArbiter->RoleToNodeMap[ridx]] == TRUE );
		}

		RELEASE_SPIN_LOCK( &ndasrArbiter->SpinLock, oldIrql );

		// 4. Read bitmap.

		status = NdasRaidArbiterInitializeOosBitmap( ndasrArbiter, 
													 ndasrArbiter->Lurn->NdasrInfo->NodeIsUptoDate, 
													 ndasrArbiter->Lurn->NdasrInfo->UpToDateNode );
		if (status != STATUS_SUCCESS) {

			break;
		}

		// 5. Set initial RAID status.
	
		if (NdasRaidArbiterRefreshRaidStatus(ndasrArbiter, TRUE) == FALSE) {
			
			//NDASSCSI_ASSERT( FALSE );
		}
	
		// 6. Create Arbiter thread

		KeInitializeEvent( &ndasrArbiter->ThreadReadyEvent, NotificationEvent, FALSE );
		KeInitializeEvent( &ndasrArbiter->ThreadEvent, NotificationEvent, FALSE );
		
		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );
	
		status = PsCreateSystemThread( &ndasrArbiter->ThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   NdasRaidArbiterThreadProc,
									   ndasrArbiter );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );
			break;
		}

		status = ObReferenceObjectByHandle( ndasrArbiter->ThreadHandle,
											GENERIC_ALL,
											NULL,
											KernelMode,
											&ndasrArbiter->ThreadObject,
											NULL );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );
			break;
		}

		status = KeWaitForSingleObject( &ndasrArbiter->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );


	} while (0);

	if (status == STATUS_SUCCESS) {

		NDASSCSI_ASSERT( ndasrArbiter->ThreadHandle );
		return STATUS_SUCCESS;
	}

	NDASSCSI_ASSERT( FALSE );

	ACQUIRE_SPIN_LOCK( &ndasrArbiter->SpinLock, &oldIrql );

	ndasrArbiter->ArbiterState0 = NDASR_ARBITER_STATUS_TERMINATING;
	ndasrArbiter->NdasrState = NRIX_RAID_STATE_FAILED;

	RELEASE_SPIN_LOCK( &ndasrArbiter->SpinLock, oldIrql );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("!!! Arbiter failed to start\n") );

	if (ndasrInfo->NdasrArbiter) {
		
		ExFreePoolWithTag( ndasrInfo->NdasrArbiter, NDASR_ARBITER_POOL_TAG );
		ndasrInfo->NdasrArbiter = NULL;
	}

	return status;
}

NTSTATUS
NdasRaidArbiterStop (
	PLURELATION_NODE	Lurn
	)
{
	KIRQL						oldIrql;
	PLIST_ENTRY					listEntry;
	PNDASR_INFO					ndasrInfo = Lurn->NdasrInfo;
	PNDASR_ARBITER				ndasrArbiter = ndasrInfo->NdasrArbiter;
	NTSTATUS					status;
	PNDASR_ARBITER_LOCK_CONTEXT lock;
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Stopping DRAID arbiter\n") );

	NdasRaidUnregisterArbiter( &NdasrGlobalData, ndasrArbiter );
	
	if (ndasrArbiter->ThreadHandle) {

		ACQUIRE_SPIN_LOCK( &ndasrArbiter->SpinLock, &oldIrql );

		ndasrArbiter->RequestToTerminate = TRUE;
		KeSetEvent( &ndasrArbiter->ThreadEvent,IO_NO_INCREMENT, FALSE ); // This will wake up Arbiter thread.
		
		RELEASE_SPIN_LOCK( &ndasrArbiter->SpinLock, oldIrql );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Wait for Arbiter thread completion\n") );

		status = KeWaitForSingleObject( ndasrArbiter->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			ASSERT(FALSE);
		
		} else {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbiter thread exited\n") );
		}

		//	Dereference the thread object.

		ObDereferenceObject( ndasrArbiter->ThreadObject );
		ZwClose( ndasrArbiter->ThreadHandle );

		ACQUIRE_SPIN_LOCK( &ndasrArbiter->SpinLock, &oldIrql );

		ndasrArbiter->ThreadObject = NULL;
		ndasrArbiter->ThreadHandle = NULL;

		RELEASE_SPIN_LOCK( &ndasrArbiter->SpinLock, oldIrql );
	}

	while (TRUE) {

		listEntry = RemoveHeadList(&ndasrArbiter->AcquiredLockList);
		
		if (listEntry == &ndasrArbiter->AcquiredLockList) {
		
			break;
		}
		
		lock = CONTAINING_RECORD( listEntry, NDASR_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink );
		InterlockedDecrement( &ndasrArbiter->AcquiredLockCount );

		NdasRaidArbiterFreeLock( ndasrArbiter, lock );
	}

	//ASSERT(InterlockedCompareExchange(&ndasrArbiter->AcquiredLockCount, 0, 0)==0);

	if (ndasrArbiter->AcquiredLockCount) {

		DbgPrint( "BUG BUG BUG ndasrArbiter->AcquiredLockCount = %d\n", ndasrArbiter->AcquiredLockCount );
	}

	if (ndasrArbiter->Lurn->NdasrInfo->ParityDiskCount != 0) {

		ExFreePoolWithTag( ndasrArbiter->OosBmpBuffer, NDASR_BITMAP_POOL_TAG );
		ExFreePoolWithTag( ndasrArbiter->LwrBmpBuffer, NDASR_BITMAP_POOL_TAG );	
		ExFreePoolWithTag( ndasrArbiter->DirtyBmpSector, NDASR_BITMAP_POOL_TAG );
		ExFreePoolWithTag( ndasrArbiter->OosBmpOnDiskBuffer, NDASR_BITMAP_POOL_TAG );
	}

	ExFreePoolWithTag( ndasrInfo->NdasrArbiter, NDASR_ARBITER_POOL_TAG );

	ndasrInfo->NdasrArbiter = NULL;

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidArbiterShutdown (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS		status = STATUS_SUCCESS;
	KIRQL			oldIrql;
	
	PNDASR_ARBITER	ndasrArbiter = Lurn->NdasrInfo->NdasrArbiter;

	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	NDASSCSI_ASSERT( ndasrArbiter );
	NDASSCSI_ASSERT( ndasrArbiter->ThreadHandle );

	ACQUIRE_SPIN_LOCK( &ndasrArbiter->SpinLock, &oldIrql );

	ndasrArbiter->RequestToShutdown = TRUE;
	KeSetEvent( &ndasrArbiter->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &ndasrArbiter->SpinLock, oldIrql );

	status = KeWaitForSingleObject( &ndasrArbiter->FinishShutdownEvent,
									Executive,
									KernelMode,
									FALSE,
									NULL );

	if (status != STATUS_SUCCESS) {
	
		NDASSCSI_ASSERT( FALSE );
		return status;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ndasrArbiter shutdown completely\n") );

	return status;
}

NTSTATUS
NdasRaidArbiterAcceptClient (
	PNDASR_ARBITER				NdasrArbiter,
	PNRIX_REGISTER				RegisterMsg,
	PNDASR_ARBITER_CONNECTION	*Connection
	) 
{
	PLIST_ENTRY				listEntry;
	PNDASR_CLIENT_CONTEXT	clientContext;
	KIRQL					oldIrql;


	(*Connection)->ConnType = RegisterMsg->ConnType;

	if (RegisterMsg->ConnType != NRIX_CONN_TYPE_NOTIFICATION && RegisterMsg->ConnType != NRIX_CONN_TYPE_REQUEST) {

		NDASSCSI_ASSERT( FALSE );
		
		return STATUS_UNSUCCESSFUL;
	}
	
	// Find client with this address, if not found, create it.

	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

	clientContext = NULL;

	for (listEntry = NdasrArbiter->NewClientQueue.Flink;
		 listEntry != &NdasrArbiter->NewClientQueue;
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
	
		if (RegisterMsg->ConnType == NRIX_CONN_TYPE_NOTIFICATION) {

			if (clientContext->NotificationConnection) {

				clientContext->UnregisterRequest = TRUE;
				clientContext = NULL;
			}

		} else if (RegisterMsg->ConnType == NRIX_CONN_TYPE_REQUEST) {
			
			if (clientContext->RequestConnection) {
			
				NDASSCSI_ASSERT( FALSE );
				clientContext->UnregisterRequest = TRUE;
				clientContext = NULL;
			}
		} 
	} 

	if (!clientContext) {

		clientContext = NdasRaidArbiterAllocClientContext();

		if (!clientContext) {

			NDASSCSI_ASSERT( FALSE );
			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );			

			return STATUS_INSUFFICIENT_RESOURCES;
		}

		clientContext->LocalClient = RegisterMsg->LocalClient;
		RtlCopyMemory( clientContext->RemoteClientAddr, (*Connection)->RemoteAddr.Node, 6 );
		InsertTailList( &NdasrArbiter->NewClientQueue, &clientContext->Link );
	}

	if (RegisterMsg->ConnType == NRIX_CONN_TYPE_NOTIFICATION) {

		clientContext->NotificationConnection = (*Connection);
		RtlCopyMemory( &clientContext->NotificationConnection->RegisterMsg, RegisterMsg, sizeof(NRIX_REGISTER) );

		clientContext->NotificationConnection->NeedReply = TRUE;

	} else if (RegisterMsg->ConnType == NRIX_CONN_TYPE_REQUEST) {

		NDASSCSI_ASSERT( clientContext->NotificationConnection );

		clientContext->RequestConnection = (*Connection);
		RtlCopyMemory( &clientContext->RequestConnection->RegisterMsg, RegisterMsg, sizeof(NRIX_REGISTER) );

		clientContext->RequestConnection->NeedReply = TRUE;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("Accepted client %p %p %02X:%02X:%02X:%02X:%02X:%02X\n", 
				clientContext, Connection, clientContext->RemoteClientAddr[0], clientContext->RemoteClientAddr[1], clientContext->RemoteClientAddr[2],
				clientContext->RemoteClientAddr[3], clientContext->RemoteClientAddr[4], clientContext->RemoteClientAddr[5]) );

	*Connection = NULL;

	// Wakeup arbiter to handle this new client

	KeSetEvent( &NdasrArbiter->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
	
	return STATUS_SUCCESS;
}

VOID
NdasRaidArbiterThreadProc (
	IN PNDASR_ARBITER	NdasrArbiter
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

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	NDASSCSI_ASSERT( NdasrArbiter->Lurn->NdasrInfo->ParityDiskCount != 0 );

	NDASSCSI_ASSERT( NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit > 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Arbiter thread starting\n") );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("ChildCount: %d, DiskCount : %d\n", 
				 NdasrArbiter->Lurn->LurnChildrenCnt, NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount) );
	
	do {

		status = NdasRaidRebuildStart( NdasrArbiter );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );
			break;
		}

		allocEventCount = NdasRaidReallocEventArray( &events, &waitBlocks, allocEventCount );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribiter enter running status\n") );

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

		ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_INITIALIZING );
		NdasrArbiter->ArbiterState0 = NDASR_ARBITER_STATUS_START;

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );		

		NdasRaidRegisterArbiter( &NdasrGlobalData, NdasrArbiter );

		// Insert LocalClient

		clientContext = NdasRaidArbiterAllocClientContext();

		if (!clientContext) {

			NDASSCSI_ASSERT( FALSE );

			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		clientContext->LocalClient = TRUE;
		InsertTailList( &NdasrArbiter->NewClientQueue, &clientContext->Link );

		status = NdasRaidArbiterRegisterNewClient( NdasrArbiter );

		NDASSCSI_ASSERT( status == STATUS_SUCCESS );
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		NDASSCSI_ASSERT( FALSE );

		KeSetEvent( &NdasrArbiter->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );
	
		NdasrArbiter->ArbiterState0 = NDASR_ARBITER_STATUS_TERMINATED;
		NdasrArbiter->NdasrState = NRIX_RAID_STATE_FAILED;
	
		PsTerminateSystemThread( STATUS_SUCCESS );
		
		return;
	}

	KeSetEvent( &NdasrArbiter->ThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	do {

		if (events == NULL || waitBlocks == NULL) {

			NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
			break;
		}

		// 9. Handle rebuild IO result
		
		if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD)) {

			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

			if (NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_SUCCESS		||
				NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_FAILED		||
				NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_CANCELLED) {
		
				RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			
				status = NdasRaidRebuildAcknowledge( NdasrArbiter );

				if (status == STATUS_SUCCESS) {

					if (RtlNumberOfSetBits(&NdasrArbiter->OosBmpHeader) == 0) {
	
						if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, TRUE) == FALSE) {

							NDASSCSI_ASSERT( FALSE );
						}

						ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE );
					}
				
				} else if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

					if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, FALSE) == FALSE) {

						if (NdasrArbiter->NdasrState != NRIX_RAID_STATE_DEGRADED) {
						
							NDASSCSI_ASSERT( FALSE );
						}
					}

					ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE );
				}

				KeSetEvent( &NdasrArbiter->ThreadEvent, IO_NO_INCREMENT, FALSE );

				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

				ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD );
			}

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
		} 

		// 10. check node status

		if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD)) {

			// 10. refresh raid status

			if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE)) {

				if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, FALSE) == FALSE) {

					NDASSCSI_ASSERT( FALSE );
				}

				ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE );
			}
		}

		// 11. check sync state

		if (NdasrArbiter->SyncState == NDASR_SYNC_REQUIRED) {			

			NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );
			NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE) );

			NdasrArbiter->SyncState = NDASR_SYNC_IN_PROGRESS;

			// Send raid status change to all client

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sending NRIX_CMD_CHANGE_STATUS to all client\n") );

			listEntry = NdasrArbiter->ClientQueue.Flink;
			
			while (listEntry != &NdasrArbiter->ClientQueue) {
						
				clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

				listEntry = listEntry->Flink;

				status = NdasRaidNotifyChangeStatus( NdasrArbiter, clientContext );

				if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

					RemoveEntryList( &clientContext->Link );
					InsertTailList( &NdasrArbiter->TerminatedClientQueue, &clientContext->Link );
						
					RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );				
				}
			}
		}

		// 12. terminate client

		if (!IsListEmpty(&NdasrArbiter->TerminatedClientQueue)) {

			if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE)) {

				BOOLEAN oosBitmapSet;

				if (NdasrArbiter->SyncState == NDASR_SYNC_IN_PROGRESS) {

					PNDASR_CLIENT_CONTEXT	notSyncedClientContext = NULL;

					NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );
		
					for (listEntry = NdasrArbiter->ClientQueue.Flink;
						 listEntry != &NdasrArbiter->ClientQueue;
						 listEntry = listEntry->Flink) {
			
						notSyncedClientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
				
						if (notSyncedClientContext->Usn != NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN) {

							break;
						}

						notSyncedClientContext = NULL;
					}

					if (notSyncedClientContext == NULL) {

						PNDASR_CLIENT_CONTEXT	clientContext;

						NdasrArbiter->SyncState = NDASR_SYNC_DONE;

						listEntry = NdasrArbiter->ClientQueue.Flink;

						while (listEntry != &NdasrArbiter->ClientQueue) {
				
							clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
							listEntry = listEntry->Flink;

							RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
							status = NdasRaidNotifyChangeStatus( NdasrArbiter, clientContext );
							ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

							if (status != STATUS_SUCCESS) {

								NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

								RemoveEntryList( &clientContext->Link );
								InsertTailList( &NdasrArbiter->TerminatedClientQueue, &clientContext->Link );

								DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notify failed during change status. Restarting.\n") );
							}
						}
					}
				}

				oosBitmapSet = FALSE;

				do {

					listEntry = NdasrArbiter->TerminatedClientQueue.Flink;
		
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

					RemoveEntryList( &clientContext->Link );
					NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, &oosBitmapSet );
			
				} while (!IsListEmpty(&NdasrArbiter->TerminatedClientQueue));

				if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, oosBitmapSet) == FALSE) {

					NDASSCSI_ASSERT( oosBitmapSet == FALSE );
				}
			
				ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE );
			}
		}

		// 13. check to be stopped

		if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_TERMINATING)) {

			if (NdasrArbiter->SyncState == NDASR_SYNC_DONE) {

				if (IsListEmpty(&NdasrArbiter->AcquiredLockList)) {

					NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );
					NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE) );

					NdasRaidRebuildStop( NdasrArbiter );
					break;
				}
			}
		}

		// 13. set receive   

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

		status = STATUS_SUCCESS;

		listEntry = NdasrArbiter->ClientQueue.Flink;
		
		while (listEntry != &NdasrArbiter->ClientQueue) {

			clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

			listEntry = listEntry->Flink;

			if (clientContext->LocalClient == TRUE) {

				ACQUIRE_DPC_SPIN_LOCK( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock );

				if (IsListEmpty(&NdasrArbiter->Lurn->NdasrInfo->RequestChannel.RequestQueue)) {

					KeClearEvent( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.RequestEvent );
				}

				RELEASE_DPC_SPIN_LOCK( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock );

				continue;
			}

			NDASSCSI_ASSERT( clientContext->RequestConnection &&
							 clientContext->RequestConnection->ConnectionFileObject );

			if (!LpxTdiV2IsRequestPending(&clientContext->RequestConnection->ReceiveOverlapped, 0)) {
					
				// Receive from request connection if not receiving.
				RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

				status = LpxTdiV2Recv( clientContext->RequestConnection->ConnectionFileObject,
									   clientContext->RequestConnection->ReceiveBuf,
									   sizeof(NRIX_HEADER),
									   0,
									   NULL,
									   &clientContext->RequestConnection->ReceiveOverlapped,
									   0,
									   NULL );


				if (!NT_SUCCESS(status)) {

					LpxTdiV2CompleteRequest( &clientContext->RequestConnection->ReceiveOverlapped, 0 );

					NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );
						
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to start to recv from client. Terminating this client\n") );
							
					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
					RemoveEntryList( &clientContext->Link );
					RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

					break;

				}

				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
					
				NDASSCSI_ASSERT( status == STATUS_SUCCESS || status == STATUS_PENDING );
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			
		if (status != STATUS_SUCCESS && status != STATUS_PENDING) {
		
			NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );
			continue;
		}

		// 14, check request setting

		if (NdasrArbiter->SyncState == NDASR_SYNC_DONE) {

			NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE) );

			if (NdasrArbiter->RequestToTerminate) {

				if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_TERMINATING)) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("SkipSetEvent\n") );
					goto SkipSetEvent;				
				}
			}

			if (NdasrArbiter->RequestToShutdown) {

				if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN)) {

					goto SkipSetEvent;
				}
			}
		}

		// 1. set event

		eventCount = 0;
		events[eventCount++] = &NdasrArbiter->ThreadEvent;

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

		status = STATUS_SUCCESS;

		listEntry = NdasrArbiter->ClientQueue.Flink;
		
		while (listEntry != &NdasrArbiter->ClientQueue) {

			if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE)) {

				NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );
				break;
			}

			clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );

			listEntry = listEntry->Flink;

			if (allocEventCount < eventCount+1) {
						
				RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
					
				allocEventCount = NdasRaidReallocEventArray( &events, &waitBlocks, allocEventCount );

				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
			}

			if (events == NULL || waitBlocks == NULL) {

				NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );

				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}


			if (clientContext->LocalClient) {

				events[eventCount++] = &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.RequestEvent;
				continue;
			}

			NDASSCSI_ASSERT( clientContext->RequestConnection &&
							 clientContext->RequestConnection->ConnectionFileObject );

			NDASSCSI_ASSERT( LpxTdiV2IsRequestPending(&clientContext->RequestConnection->ReceiveOverlapped, 0) );

			events[eventCount++] = &clientContext->RequestConnection->ReceiveOverlapped.Request[0].CompletionEvent;
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			
		if (status != STATUS_SUCCESS) {
		
			NDASSCSI_ASSERT( status == STATUS_INSUFFICIENT_RESOURCES );
			continue;
		}

		NDASSCSI_ASSERT( eventCount <= 4 );

		timeOut.QuadPart = -NANO100_PER_SEC * 30; // need to wake-up to handle dirty bitmap

		status = KeWaitForMultipleObjects( eventCount, 
										   events, 
										   WaitAny, 
										   Executive, 
										   KernelMode, 
										   TRUE,	
										   &timeOut, 
										   waitBlocks );

		KeClearEvent( &NdasrArbiter->ThreadEvent );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Wake up, status = %x\n", status) );

SkipSetEvent:

		if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE)) {

			NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );
			continue;
		}

		// 2. check client request message

		status = NdasRaidArbiterCheckRequestMsg( NdasrArbiter );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_CLUSTER_NODE_UNREACHABLE );
			
			if (status == STATUS_CLUSTER_NODE_UNREACHABLE) {

				NdasrArbiter->ClusterState16 = NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE >> 16;
			}

			continue;
		}

		// 3. check SyncState transition

		if (NdasrArbiter->SyncState == NDASR_SYNC_IN_PROGRESS) {

			PNDASR_CLIENT_CONTEXT	notSyncedClientContext = NULL;
		
			for (listEntry = NdasrArbiter->ClientQueue.Flink;
				 listEntry != &NdasrArbiter->ClientQueue;
				 listEntry = listEntry->Flink) {
			
				notSyncedClientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
				
				if (notSyncedClientContext->Usn != NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN) {

					break;
				}

				notSyncedClientContext = NULL;
			}

			if (notSyncedClientContext == NULL) {

				PNDASR_CLIENT_CONTEXT	clientContext;

				NdasrArbiter->SyncState = NDASR_SYNC_DONE;

				listEntry = NdasrArbiter->ClientQueue.Flink;

				while (listEntry != &NdasrArbiter->ClientQueue) {
				
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					listEntry = listEntry->Flink;

					status = NdasRaidNotifyChangeStatus( NdasrArbiter, clientContext );

					if (status != STATUS_SUCCESS) {

						NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
						
						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbiter->TerminatedClientQueue, &clientContext->Link );
						
						RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

						DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notify failed during change status. Restarting.\n") );
					}
				}
			}
		}

		if (!IsListEmpty(&NdasrArbiter->TerminatedClientQueue)) {

			continue;
		}

		if (NdasrArbiter->SyncState != NDASR_SYNC_DONE) {

			continue;
		}


		// 4. check stop request

		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

		if (NdasrArbiter->RequestToTerminate) {

			if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_TERMINATING)) {

				NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_START) );

				ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_START );
				NdasrArbiter->ArbiterState0 = NDASR_ARBITER_STATUS_TERMINATING;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter stop requested. Sending RETIRE message to all client..\n") );	

				listEntry = NdasrArbiter->ClientQueue.Flink;

				while (listEntry != &NdasrArbiter->ClientQueue) {
							
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					listEntry = listEntry->Flink;

					if (clientContext->LocalClient) {

						continue;
					}

					RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

					status = NdasRaidNotifyRetire( NdasrArbiter, clientContext );

					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

					if (status != STATUS_SUCCESS) {

						NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbiter->TerminatedClientQueue, &clientContext->Link );
					}

					// Continue to next client even if notification has failed.
				}

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Sent RETIRE to all client. Exiting NdasrArbiter loop.\n") );
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );


		// 5. check shutdown request

		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

		if (NdasrArbiter->RequestToShutdown) {

			NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

			if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN)) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter shutdown requested.Sending RETIRE message to all client..\n") );	

				NdasrArbiter->Shutdown8 = NDASR_ARBITER_STATUS_SHUTDOWN >> 8;

				listEntry = NdasrArbiter->ClientQueue.Flink;
			
				while (listEntry != &NdasrArbiter->ClientQueue) {
				
					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					listEntry = listEntry->Flink;

					if (clientContext->LocalClient) {

						continue;
					}

					RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
						
					status = NdasRaidNotifyRetire( NdasrArbiter, clientContext );
						
					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

					if (status != STATUS_SUCCESS) {

						NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED );

						RemoveEntryList( &clientContext->Link );
						InsertTailList( &NdasrArbiter->TerminatedClientQueue, &clientContext->Link );
					}
				}		
			
			} else {
			
				BOOLEAN clean;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter already Send RETIRE message to all client..\n") );	

				clean = TRUE;

				for (listEntry = NdasrArbiter->ClientQueue.Flink;
					 listEntry != &NdasrArbiter->ClientQueue;
					 listEntry = listEntry->Flink) {

					clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
					
					if (clientContext->LocalClient == FALSE) {

						clean = FALSE;
						break;
					}
				}

				if (clean == TRUE) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Aribter set FinishShutdownEvent\n") );	

					RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

					if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, TRUE) == FALSE) {

							//NDASSCSI_ASSERT( FALSE );
					}

					ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE );

					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

					NdasrArbiter->RequestToShutdown = FALSE;
					KeSetEvent( &NdasrArbiter->FinishShutdownEvent, IO_DISK_INCREMENT, FALSE );
				}
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		if (NdasrArbiter->SyncState != NDASR_SYNC_DONE) {

			continue;
		}

		if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_START) || 
			FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN)) {

			continue;
		}


		// 6. rebuild

		NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_START) );
		NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN) );
		NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE) );
		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

		if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_OUT_OF_SYNC) {

			NDASSCSI_ASSERT( NdasrArbiter->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE );

			if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD)) {
				
				status = NdasRaidRebuildInitiate( NdasrArbiter );

				if (status == STATUS_SUCCESS) {

					if (RtlNumberOfSetBits(&NdasrArbiter->OosBmpHeader) != 0) {

						NdasrArbiter->RebuildState12 = NDASR_ARBITER_STATUS_REBUILD >> 12;

						NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );

					} else {

						if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, TRUE) == FALSE) {
						
							NDASSCSI_ASSERT( FALSE );
						}
					}
				
				} else {

					NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

					NdasrArbiter->ClusterState16 = NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE >> 16;

					continue;
				}

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						    ("status = %x, NdasrArbiter->Status = %x\n", status, NdasrArbiter->Status) );
			}
		}

		// 7. check to use spare 

		NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_START) );
		NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN) );
		NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE) );
		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

		if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_DEGRADED) {

			NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_REBUILD) );
			spareUsed = FALSE;

			status = NdasRaidArbiterUseSpareIfNeeded( NdasrArbiter, &spareUsed );

			NDASSCSI_ASSERT( status == STATUS_SUCCESS );
	
			if (spareUsed) {

				if (NdasRaidArbiterRefreshRaidStatus(NdasrArbiter, FALSE) == FALSE) {

					NDASSCSI_ASSERT( FALSE );
				}

				continue;
			}
		}

		// 8. check new clientContext

		NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_START) );
		NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN) );
		NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_CLUSTER_NODE_UNREACHABLE) );
		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

		status = NdasRaidArbiterRegisterNewClient( NdasrArbiter );

		NDASSCSI_ASSERT( status == STATUS_SUCCESS );

	} while (TRUE);


	NDASSCSI_ASSERT( IsListEmpty(&NdasrArbiter->AcquiredLockList) );

	while (listEntry = ExInterlockedRemoveHeadList(&NdasrArbiter->ClientQueue, &NdasrArbiter->SpinLock)) {

		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		
		NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, NULL );
	}

	while (listEntry = ExInterlockedRemoveHeadList(&NdasrArbiter->NewClientQueue, &NdasrArbiter->SpinLock)) {

		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		
		NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, NULL );
	}

	ClearFlag( NdasrArbiter->Status, NDASR_ARBITER_STATUS_TERMINATING );
	NdasrArbiter->ArbiterState0 = NDASR_ARBITER_STATUS_TERMINATED;

	if (NdasRaidArbiterUpdateInCoreRmd(NdasrArbiter) == FALSE) {

		NDASSCSI_ASSERT( FALSE );
	}

	if (NdasrArbiter->NdasrState != NRIX_RAID_STATE_FAILED) {

		NdasRaidArbiterWriteRmd( NdasrArbiter, &NdasrArbiter->Lurn->NdasrInfo->Rmd );
	}

	NDASSCSI_ASSERT( NdasrArbiter->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_UNMOUNTED ||
					 NdasrArbiter->Lurn->NdasrInfo->Rmd.state == (NDAS_RAID_META_DATA_STATE_UNMOUNTED | NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) );

	NdasrArbiter->NdasrState = NRIX_RAID_STATE_TERMINATED;
	
	NdasRaidFreeEventArray( events, waitBlocks );
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );

	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

NTSTATUS
NdasRaidArbiterRegisterNewClient ( 
	IN PNDASR_ARBITER NdasrArbiter
	)
{
	NTSTATUS				status;

	KIRQL					oldIrql;

	PLIST_ENTRY				listEntry;
	PNDASR_CLIENT_CONTEXT	clientContext;


	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

	listEntry = NdasrArbiter->NewClientQueue.Flink;

	while (listEntry != &NdasrArbiter->NewClientQueue) {
				
		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		listEntry = listEntry->Flink;

		if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN) && clientContext->LocalClient == FALSE) {

			continue;
		}

		if (clientContext->UnregisterRequest) {

			RemoveEntryList( &clientContext->Link );

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, NULL );
			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

			continue;
		}

		if (clientContext->NotificationConnection && clientContext->NotificationConnection->NeedReply) {
	
			NRIX_HEADER	registerReply = {0};
			ULONG		result;

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

			registerReply.Signature = NTOHL(NRIX_SIGNATURE);
			registerReply.Command	= NRIX_CMD_REGISTER;
			registerReply.Length	= NTOHS((UINT16)sizeof(NRIX_HEADER));
			registerReply.ReplyFlag = 1;
			registerReply.Sequence	= clientContext->NotificationConnection->RegisterMsg.Header.Sequence;
			registerReply.Result	= NRIX_RESULT_SUCCESS;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Sending registration reply(result=%x) to remote client\n", registerReply.Result) );

			result = 0;

			status = LpxTdiV2Send ( clientContext->NotificationConnection->ConnectionFileObject, 
									(PUCHAR)&registerReply, 
									sizeof(NRIX_HEADER), 
									0,
									NULL,
									NULL,
									0,
									&result );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend NotificationConnection status=%x, result=%x.\n", status, result) );

			if (result != sizeof(NRIX_HEADER)) {

				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
				RemoveEntryList( &clientContext->Link );

				RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
				NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, NULL );
				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

				status = STATUS_SUCCESS;
				continue;
			}

			clientContext->NotificationConnection->NeedReply = FALSE;

			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
		}

		if (clientContext->RequestConnection && clientContext->RequestConnection->NeedReply) {

			NRIX_HEADER	registerReply = {0};
			ULONG		result;

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
	
			registerReply.Signature = NTOHL(NRIX_SIGNATURE);
			registerReply.Command	= NRIX_CMD_REGISTER;
			registerReply.Length	= NTOHS((UINT16)sizeof(NRIX_HEADER));
			registerReply.ReplyFlag = 1;
			registerReply.Sequence	= clientContext->RequestConnection->RegisterMsg.Header.Sequence;
			registerReply.Result	= NRIX_RESULT_SUCCESS;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Sending registration reply(result=%x) to remote client\n", registerReply.Result) );

			result = 0;

			status = LpxTdiV2Send ( clientContext->RequestConnection->ConnectionFileObject, 
									(PUCHAR)&registerReply, 
									sizeof(NRIX_HEADER), 
									0,
									NULL,
									NULL,
									0,
									&result );


			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend RequestConnection status=%x, result=%x.\n", status, result) );

			if (result != sizeof(NRIX_HEADER)) {

				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

				RemoveEntryList( &clientContext->Link );

				RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
				NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, NULL );
				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

				status = STATUS_SUCCESS;
				continue;
			}

			clientContext->RequestConnection->NeedReply = FALSE;

			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
		}

		if (clientContext->LocalClient == TRUE ||
			clientContext->NotificationConnection && clientContext->RequestConnection) {

			RemoveEntryList( &clientContext->Link );

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying initial status to client\n") );
			
			clientContext->Usn = NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN;

			status = NdasRaidNotifyChangeStatus( NdasrArbiter, clientContext );
				
			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

			if (status == STATUS_SUCCESS) {

				RtlCopyMemory( clientContext->NodeFlags, NdasrArbiter->NodeFlags, sizeof(NdasrArbiter->NodeFlags) );
				InsertTailList( &NdasrArbiter->ClientQueue, &clientContext->Link );
			
				continue;
			}

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			NdasRaidArbiterTerminateClient( NdasrArbiter, clientContext, NULL );
			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

			if (status == STATUS_CONNECTION_DISCONNECTED) {
						
				status = STATUS_SUCCESS;
				continue;
					
			} else {

				NDASSCSI_ASSERT( FALSE );
				break;
			}
		}
	}

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

	return status;
}

PNDASR_CLIENT_CONTEXT 
NdasRaidArbiterAllocClientContext (
	VOID
	)
{
	PNDASR_CLIENT_CONTEXT ClientContext;
	
	ClientContext = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_CLIENT_CONTEXT), NDASR_CLIENT_CONTEXT_POOL_TAG );
	
	if (ClientContext == NULL) {

		NDASSCSI_ASSERT( FALSE );
		return NULL;
	}

	RtlZeroMemory( ClientContext, sizeof(NDASR_CLIENT_CONTEXT) );

	InitializeListHead( &ClientContext->Link );
	InitializeListHead( &ClientContext->AcquiredLockList );
	
	return ClientContext;
}

VOID
NdasRaidArbiterTerminateClient (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PBOOLEAN				OosBitmapSet
	) 
{
	PNDASR_ARBITER_LOCK_CONTEXT	lock;
	PLIST_ENTRY					listEntry;
	KIRQL						oldIrql;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Terminating client.. Client = %p\n", ClientContext) );
	
	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

	if (!IsListEmpty(&ClientContext->AcquiredLockList)) {

		if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_NORMAL) {

			NDASSCSI_ASSERT( NdasrArbiter->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE );

			if (OosBitmapSet) {

				*OosBitmapSet = TRUE;
			}
		}
	}

	// Free locks acquired by this client
	
	while (!IsListEmpty(&ClientContext->AcquiredLockList)) {
	
		listEntry = RemoveHeadList( &ClientContext->AcquiredLockList );

		lock = CONTAINING_RECORD( listEntry, NDASR_ARBITER_LOCK_CONTEXT, ClientAcquiredLink );

		InitializeListHead( &lock->ClientAcquiredLink ); // to check bug...
		
		// Remove from arbiter's list too.
		
		RemoveEntryList( &lock->ArbiterAcquiredLink );
		InitializeListHead( &lock->ArbiterAcquiredLink ); // to check bug...

		lock->Owner = NULL;

		InterlockedDecrement( &NdasrArbiter->AcquiredLockCount );
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Freeing terminated client's lock %I64x(%I64x:%x)\n", 
					 lock->Id, lock->BlockAddress, lock->BlockLength) );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Unclean termination. Merging this lock range to dirty bitmap\n") );

		// Merge this client's LWR to bitmap
			
		NdasRaidArbiterChangeOosBitmapBit( NdasrArbiter, TRUE, lock->BlockAddress, lock->BlockLength );
		NdasRaidArbiterFreeLock( NdasrArbiter, lock );
	}

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

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
NdasRaidArbiterInitializeOosBitmap (
	IN PNDASR_ARBITER	NdasrArbiter,
	IN PBOOLEAN			NodeIsUptoDate,
	IN UCHAR			UpToDateNode
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
	
	NdasrArbiter->OosBmpBitCount		  = (ULONG)((NdasrArbiter->Lurn->UnitBlocks + NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit -1)/ NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit);
	NdasrArbiter->OosBmpByteCount		  = ((NdasrArbiter->OosBmpBitCount + sizeof(ULONG)*8 -1) /(sizeof(ULONG)*8))*sizeof(ULONG); // In core bit size should be padded to ULONG size.
	NdasrArbiter->OosBmpOnDiskSectorCount = (NdasrArbiter->OosBmpBitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;

	NDASSCSI_ASSERT( NdasrArbiter->OosBmpOnDiskSectorCount );
	NDASSCSI_ASSERT( NdasrArbiter->OosBmpOnDiskSectorCount < (2 << 16) );

	NdasrArbiter->OosBmpBuffer = ExAllocatePoolWithTag( NonPagedPool, 
														NdasrArbiter->OosBmpByteCount, 
														NDASR_BITMAP_POOL_TAG );

	if (NdasrArbiter->OosBmpBuffer == NULL) {
	
		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasrArbiter->OosBmpOnDiskBuffer = ExAllocatePoolWithTag( NonPagedPool, 
															  NdasrArbiter->OosBmpOnDiskSectorCount * SECTOR_SIZE, 
															  NDASR_BITMAP_POOL_TAG );
	
	if (NdasrArbiter->OosBmpOnDiskBuffer == NULL) {

		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasrArbiter->DirtyBmpSector = ExAllocatePoolWithTag( NonPagedPool, 
														  NdasrArbiter->OosBmpOnDiskSectorCount * sizeof(BOOLEAN), 
														  NDASR_BITMAP_POOL_TAG );
	
	if (NdasrArbiter->DirtyBmpSector == NULL) {

		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasrArbiter->LwrBmpBuffer = ExAllocatePoolWithTag( NonPagedPool, NdasrArbiter->OosBmpByteCount, NDASR_BITMAP_POOL_TAG );

	if (NdasrArbiter->LwrBmpBuffer == NULL) {
	
		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	tmpOosBmpOnDiskBuffer = ExAllocatePoolWithTag( NonPagedPool, 
												   NdasrArbiter->OosBmpOnDiskSectorCount * SECTOR_SIZE, 
												   NDASR_BITMAP_POOL_TAG );
	
	if (tmpOosBmpOnDiskBuffer == NULL) {

		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( NdasrArbiter->OosBmpBuffer, NdasrArbiter->OosBmpByteCount );
	RtlZeroMemory( NdasrArbiter->LwrBmpBuffer, NdasrArbiter->OosBmpByteCount );
	RtlZeroMemory( NdasrArbiter->OosBmpOnDiskBuffer, NdasrArbiter->OosBmpOnDiskSectorCount * SECTOR_SIZE );
	RtlZeroMemory( NdasrArbiter->DirtyBmpSector, NdasrArbiter->OosBmpOnDiskSectorCount * sizeof(BOOLEAN) );
	RtlZeroMemory( tmpOosBmpOnDiskBuffer, NdasrArbiter->OosBmpOnDiskSectorCount * SECTOR_SIZE );

	RtlInitializeBitMap( &NdasrArbiter->OosBmpHeader, NdasrArbiter->OosBmpBuffer, NdasrArbiter->OosBmpByteCount * 8 );
	RtlInitializeBitMap( &NdasrArbiter->LwrBmpHeader, NdasrArbiter->LwrBmpBuffer, NdasrArbiter->OosBmpByteCount * 8 );


	// Read from UpToDateNode.
	// Assume UpToDateNode is non-spare and in-sync disk

	secondOosBmp = FALSE;

	for (nidx=0; nidx < NdasrArbiter->Lurn->LurnChildrenCnt; nidx++) {

		status = NdasRaidLurnExecuteSynchrously( NdasrArbiter->Lurn->LurnChildren[UpToDateNode], 
												  SCSIOP_READ16,
												  FALSE,
												  FALSE,
												  secondOosBmp ? (PUCHAR)tmpOosBmpOnDiskBuffer : (PUCHAR)NdasrArbiter->OosBmpOnDiskBuffer, 
												  (-1*NDAS_BLOCK_LOCATION_BITMAP), 
												  NdasrArbiter->OosBmpOnDiskSectorCount,
												  TRUE );

		if (status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to read bitmap from node %d\n", UpToDateNode) );			
			return status;
		} 
	
		// Check each sector for validity.
		
		for (i = 0; i < NdasrArbiter->OosBmpOnDiskSectorCount; i++) {

			if (secondOosBmp) {

				if (tmpOosBmpOnDiskBuffer[i].SequenceNumHead != tmpOosBmpOnDiskBuffer[i].SequenceNumTail) {
			
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Bitmap sequence head/tail for sector %d mismatch %I64x:%I64x. Setting all dirty on this sector\n", 
								 i, NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead, NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail) );

					break;
				}

				continue;
			}

			if (NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead != NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail) {
			
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Bitmap sequence head/tail for sector %d mismatch %I64x:%I64x. Setting all dirty on this sector\n", 
							 i, NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead, NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail) );

				break;
			}			
		}

		if (i != NdasrArbiter->OosBmpOnDiskSectorCount) {
		
			UCHAR	nidx2;

			NDASSCSI_ASSERT( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("OOS bitmap has fault. Set all bits\n") );

			if (secondOosBmp) {

				continue;
			}
		
			for (nidx2 = nidx+1; nidx2 < NdasrArbiter->Lurn->LurnChildrenCnt; nidx2++) {

				if (NodeIsUptoDate[nidx2] == TRUE) {

					break;
				}
			}

			if (nidx2 != NdasrArbiter->Lurn->LurnChildrenCnt) {

				continue;
			}
	
			NDASSCSI_ASSERT( FALSE );
			
			for (i=0; i<NdasrArbiter->OosBmpOnDiskSectorCount; i++) {
		
				NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead = NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = 0;

                if (i== NdasrArbiter->OosBmpOnDiskSectorCount-1) {

                    UINT		remainingBits = NdasrArbiter->OosBmpBitCount - i * NDAS_BIT_PER_OOS_BITMAP_BLOCK;
                    RTL_BITMAP	lastSectorBmpHeader;
                    
					RtlInitializeBitMap( &lastSectorBmpHeader, 
										 (PULONG)(NdasrArbiter->OosBmpOnDiskBuffer[i].Bits), 
										 NDAS_BIT_PER_OOS_BITMAP_BLOCK );
                    
					RtlClearAllBits( &lastSectorBmpHeader );
                    RtlSetBits( &lastSectorBmpHeader, 0, remainingBits );
                    
				} else {
				
					RtlFillMemory(NdasrArbiter->OosBmpOnDiskBuffer[i].Bits, sizeof(NdasrArbiter->OosBmpOnDiskBuffer[i].Bits), 0x0ff);
				}

				NdasrArbiter->DirtyBmpSector[i] = TRUE;
			}
		}

		if (secondOosBmp == FALSE) {

			secondOosBmp = TRUE;
			continue;
		}

		if (NdasrArbiter->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_UNMOUNTED) {

			NDASSCSI_ASSERT( RtlEqualMemory(NdasrArbiter->OosBmpOnDiskBuffer, 
											tmpOosBmpOnDiskBuffer, 
											NdasrArbiter->OosBmpOnDiskSectorCount*SECTOR_SIZE) == TRUE );
		}

		for (i=0; i<NdasrArbiter->OosBmpOnDiskSectorCount; i++) {

			ULONG j;

			for (j=0; j<NDAS_BYTE_PER_OOS_BITMAP_BLOCK; j++) {

				NdasrArbiter->OosBmpOnDiskBuffer[i].Bits[j] = 
					NdasrArbiter->OosBmpOnDiskBuffer[i].Bits[j] | tmpOosBmpOnDiskBuffer[i].Bits[j];

				if (NdasrArbiter->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_UNMOUNTED && 
					NdasrArbiter->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {
					
					NDASSCSI_ASSERT( NdasrArbiter->OosBmpOnDiskBuffer[i].Bits[j] == 0 );
				}
			}
		}
	}

	NDASSCSI_ASSERT( secondOosBmp == TRUE );

	// Convert on disk bitmap to in-memory bitmap
	
	for (i=0; i<NdasrArbiter->OosBmpOnDiskSectorCount; i++) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Bitmap block %d sequence num=%I64x\n", 
											  i, NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead) );

		if (i == NdasrArbiter->OosBmpOnDiskSectorCount-1) {
		
			// Last bitmap sector. 

			byteCount = NdasrArbiter->OosBmpByteCount % NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
		
		} else {
		
			byteCount = NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
		}

		RtlCopyMemory( ((PUCHAR)NdasrArbiter->OosBmpBuffer) + NDASR_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(i,0), 
						NdasrArbiter->OosBmpOnDiskBuffer[i].Bits, 
						byteCount );
	}

	setBits = RtlNumberOfSetBits( &NdasrArbiter->OosBmpHeader );

	if (setBits == FALSE) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Bitmap is clean\n") );
	}

	ExFreePoolWithTag( tmpOosBmpOnDiskBuffer, NDASR_BITMAP_POOL_TAG );

	return status;
}

VOID
NdasRaidArbiterChangeOosBitmapBit (
	PNDASR_ARBITER	NdasrArbiter,
	BOOLEAN			Set,	// TRUE for set, FALSE for clear
	UINT64			Addr,
	UINT64			Length
	) 
{
	UINT32 bitOffset;
	UINT32 numberOfBit;

	ASSERT( KeGetCurrentIrql() ==  DISPATCH_LEVEL ); // should be called with spinlock locked.
	
	bitOffset	= (UINT32)(Addr / NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit);
	numberOfBit = (UINT32)((Addr + Length -1) / NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit - bitOffset + 1);


//	DebugTrace(NDASSCSI_DBG_LURN_NDASR_INFO, ("Before BitmapByte[0]=%x\n", Arbiter->OosBmpBuffer[0]));	
	
	if (Set) {
	
		DebugTrace( DBG_LURN_TRACE, ("Setting in-memory bitmap offset %x:%x\n", bitOffset, numberOfBit) );
		
		RtlSetBits( &NdasrArbiter->OosBmpHeader, bitOffset, numberOfBit );

	} else {

		DebugTrace( DBG_LURN_TRACE, ("Clearing in-memory bitmap offset %x:%x\n", bitOffset, numberOfBit) );
		
		RtlClearBits( &NdasrArbiter->OosBmpHeader, bitOffset, numberOfBit );
	}
}

VOID
NdasRaidArbiterUpdateLwrBitmapBit (
	PNDASR_ARBITER				NdasrArbiter,
	PNDASR_ARBITER_LOCK_CONTEXT HintAddedLock,
	PNDASR_ARBITER_LOCK_CONTEXT HintRemovedLock
	) 
{
	PLIST_ENTRY					listEntry;
	PNDASR_ARBITER_LOCK_CONTEXT lock;
	UINT32						bitOffset;
	UINT32						numberOfBit;
	ULONG						lockCount = 0;

	if (HintAddedLock && HintRemovedLock == NULL) {

		bitOffset = (UINT32) (HintAddedLock->BlockAddress / NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit);
		numberOfBit = (UINT32) ((HintAddedLock->BlockAddress + HintAddedLock->BlockLength -1) / NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit - 
								bitOffset + 1);
		
		DebugTrace( DBG_LURN_TRACE, ("Setting LWR bit %x:%x\n", bitOffset, numberOfBit) );
		
		RtlSetBits( &NdasrArbiter->LwrBmpHeader, bitOffset, numberOfBit );
		
		lockCount = 1;
	
	} else {
		
		// Recalc all lock
		
		RtlClearAllBits( &NdasrArbiter->LwrBmpHeader );

		for (listEntry = NdasrArbiter->AcquiredLockList.Flink;
			listEntry != &NdasrArbiter->AcquiredLockList;
			listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD(listEntry, NDASR_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);

			bitOffset = (UINT32)(lock->BlockAddress / NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit);
			numberOfBit = (UINT32)((lock->BlockAddress + lock->BlockLength - 1) / NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit - bitOffset + 1);

			// DebugTrace(DBG_LURN_TRACE, ("Setting bit %x:%x\n", BitOffset, NumberOfBit));
			
			RtlSetBits( &NdasrArbiter->LwrBmpHeader, bitOffset, numberOfBit );
			
			lockCount++;
		}
	}

	DebugTrace( DBG_LURN_TRACE, ("Updated LWR bitmap with %d locks\n", lockCount) );		
}

NTSTATUS 
NdasRaidArbiterUpdateOnDiskOosBitmap (
	PNDASR_ARBITER	NdasrArbiter,
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
	
	
	for (i=0; i<NdasrArbiter->OosBmpByteCount/sizeof(ULONG); i++) {
	
		sector	  = NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(i*sizeof(ULONG));
		offset	  = NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(i*sizeof(ULONG));
		
		bitValues = NdasrArbiter->OosBmpBuffer[i] | NdasrArbiter->LwrBmpBuffer[i];
	
		if (UpdateAll)  {

			*((PULONG)&NdasrArbiter->OosBmpOnDiskBuffer[sector].Bits[offset]) = bitValues;
			NdasrArbiter->DirtyBmpSector[sector] = TRUE;
		}

		if (*((PULONG)&NdasrArbiter->OosBmpOnDiskBuffer[sector].Bits[offset]) == bitValues) {
			
			continue;
		}
			
		DebugTrace( DBG_LURN_TRACE, ("Bitmap offset %x changed from %08x to %08x\n", 
									  i*4, *((PULONG)&NdasrArbiter->OosBmpOnDiskBuffer[sector].Bits[offset]), bitValues) );

		*((PULONG)&NdasrArbiter->OosBmpOnDiskBuffer[sector].Bits[offset]) = bitValues;
		NdasrArbiter->DirtyBmpSector[sector] = TRUE;
	}

	// Update dirty bitmap sector only
	
	for (i=0; i<NdasrArbiter->OosBmpOnDiskSectorCount; i++) {

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
		
		if (NdasrArbiter->DirtyBmpSector[i]) {
		
			NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead++;
			NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead;
			NdasrArbiter->DirtyBmpSector[i] = FALSE;
			
			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			
			DebugTrace( DBG_LURN_NOISE, 
						("Updating dirty bitmap sector %d, Seq = %I64x\n", i, NdasrArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead) );
			
			status = NdasRaidArbiterWriteMetaSync( NdasrArbiter, 
													(PUCHAR)&(NdasrArbiter->OosBmpOnDiskBuffer[i]), 
													-1*(NDAS_BLOCK_LOCATION_BITMAP+i), 
													1, 
													TRUE );

			if (status != STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to update dirty bitmap sector %d\n", i) );	

				return status;
			}

		} else {

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
		}
	}

	return status;
}

NTSTATUS
NdasRaidArbiterCheckRequestMsg (
	IN PNDASR_ARBITER	NdasrArbiter
	) 
{
	NTSTATUS				status;
	PNDASR_CLIENT_CONTEXT	clientContext;
	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
		

	// Check request is received through request connection.

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	
	listEntry = NdasrArbiter->ClientQueue.Flink;

	while (listEntry != &NdasrArbiter->ClientQueue) {
		
		clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
		listEntry = listEntry->Flink;

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		if (clientContext->LocalClient) {

			PNDASR_LOCAL_MSG	ndasrRequestLocalMsg;
			PLIST_ENTRY			listEntry;

			ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			ACQUIRE_SPIN_LOCK( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock, &oldIrql );

			if (IsListEmpty(&NdasrArbiter->Lurn->NdasrInfo->RequestChannel.RequestQueue)) {

				RELEASE_SPIN_LOCK( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock, oldIrql );

				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
				continue;
			}

			listEntry = RemoveHeadList( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.RequestQueue );
			ndasrRequestLocalMsg = CONTAINING_RECORD( listEntry, NDASR_LOCAL_MSG, ListEntry );

			RELEASE_SPIN_LOCK( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock, oldIrql );

			ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			status = NdasRaidArbiterHandleRequestMsg( NdasrArbiter, 
													  clientContext, 
													  (PNRIX_HEADER)ndasrRequestLocalMsg->NrixHeader );

			ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
			continue;
		}

		NDASSCSI_ASSERT( clientContext->RequestConnection && 
						 LpxTdiV2IsRequestPending(&clientContext->RequestConnection->ReceiveOverlapped, 0) );
		
		if (KeReadStateEvent(&clientContext->RequestConnection->ReceiveOverlapped.Request[0].CompletionEvent)) {
			
			LpxTdiV2CompleteRequest( &clientContext->RequestConnection->ReceiveOverlapped, 0 );

			do { 

				PNRIX_HEADER	message;
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


				if (clientContext->RequestConnection->ReceiveOverlapped.Request[0].IoStatusBlock.Information != sizeof(NRIX_HEADER)) {
			
					NDASSCSI_ASSERT( FALSE );

					status = STATUS_CONNECTION_DISCONNECTED;
					break;
				} 

				// Read remaining data if needed.
				
				message = (PNRIX_HEADER) clientContext->RequestConnection->ReceiveBuf;
				
				msgLength = NTOHS(message->Length);
				
				if (msgLength > NRIX_MAX_REQUEST_SIZE || msgLength < sizeof(NRIX_HEADER)) {

					NDASSCSI_ASSERT( FALSE );

					status = STATUS_CONNECTION_DISCONNECTED;
					break;
				} 
				
				if (msgLength > sizeof(NRIX_HEADER)) {

					LONG	result;
					
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Reading additional message data %d bytes\n", msgLength - sizeof(NRIX_HEADER)) );
					
					result = 0;

					status = LpxTdiV2Recv( clientContext->RequestConnection->ConnectionFileObject, 
										   (PUCHAR)(clientContext->RequestConnection->ReceiveBuf + sizeof(NRIX_HEADER)),
										   msgLength - sizeof(NRIX_HEADER),
										   0, 
										   NULL, 
										   NULL,
										   0,
										   &result );

					if (result != msgLength - sizeof(NRIX_HEADER)) {
					
						NDASSCSI_ASSERT( FALSE );

						status = STATUS_CONNECTION_DISCONNECTED;
						break;
					}

					NDASSCSI_ASSERT( status == STATUS_SUCCESS );

				} 			

				status = NdasRaidArbiterHandleRequestMsg( NdasrArbiter, 
														   clientContext, 
														   (PNRIX_HEADER)clientContext->RequestConnection->ReceiveBuf );
					
				if (status != STATUS_SUCCESS) {

					NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_CLUSTER_NODE_UNREACHABLE );
					break;
				}
			
			} while (0);

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( status == STATUS_CONNECTION_DISCONNECTED || status == STATUS_CLUSTER_NODE_UNREACHABLE );

				if (status == STATUS_CONNECTION_DISCONNECTED) {

					ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
					RemoveEntryList( &clientContext->Link );
					InsertTailList( &NdasrArbiter->TerminatedClientQueue, &clientContext->Link );
					RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );				
				}
				
				ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
				break;
			}			
		}

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	}

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

	return status;
}

NTSTATUS
NdasRaidArbiterHandleRequestMsg (
	PNDASR_ARBITER			NdasrArbiter, 
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PNRIX_HEADER			RequestMsg
	)
{
	NTSTATUS				status;

	PNRIX_HEADER			replyMsg;
	NRIX_HEADER				commonReplyMsg;
	NRIX_ACQUIRE_LOCK_REPLY	acquireLockReply;
	UINT32					replyLength;

	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
	ULONG					result;


	// Check data validity.

	if (NTOHL(RequestMsg->Signature) != NRIX_SIGNATURE || RequestMsg->ReplyFlag != 0) {

		NDASSCSI_ASSERT( FALSE );	
		return STATUS_UNSUCCESSFUL;
	}

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

	if (NTOHL(RequestMsg->Usn) != NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN) {

		NDASSCSI_ASSERT( ((signed _int32)(NTOHL(RequestMsg->Usn) - NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN)) < 0 );
		NDASSCSI_ASSERT( NdasrArbiter->SyncState != NDASR_SYNC_DONE );

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		return NdasRaidReplyChangeStatus( NdasrArbiter, ClientContext, RequestMsg, NRIX_RESULT_LOWER_USN );
	}

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

	if (NdasrArbiter->SyncState != NDASR_SYNC_DONE) {

		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_IN_PROGRESS );
		NDASSCSI_ASSERT( RequestMsg->Command == NRIX_CMD_NODE_CHANGE );

		if (RequestMsg->Command != NRIX_CMD_NODE_CHANGE) {

			NDASSCSI_ASSERT( FALSE );

			return NdasRaidReplyChangeStatus( NdasrArbiter, ClientContext, RequestMsg, NRIX_RESULT_FAIL );
		}

		ClientContext->Usn = NTOHL(RequestMsg->Usn);
	}

	// Create reply
	
	// Process request

	switch (RequestMsg->Command) {

	case NRIX_CMD_NODE_CHANGE: {

		PNRIX_NODE_CHANGE	nodeChangeMsg = (PNRIX_NODE_CHANGE)RequestMsg;
		UINT32				i;

		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE || NdasrArbiter->SyncState == NDASR_SYNC_IN_PROGRESS );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbiter received node change message\n") );		

		replyLength = sizeof(NRIX_HEADER);
		replyMsg = &commonReplyMsg;

		RtlZeroMemory( &commonReplyMsg, replyLength );

		commonReplyMsg.Signature	= HTONL(NRIX_SIGNATURE);
		commonReplyMsg.ReplyFlag	= TRUE;
		commonReplyMsg.Command		= RequestMsg->Command;
		commonReplyMsg.Length		= HTONS((UINT16)replyLength);
		commonReplyMsg.Sequence		= RequestMsg->Sequence;

		// Update local node information from packet
		
		for (i=0; i<nodeChangeMsg->UpdateCount; i++) {
			
			ClientContext->NodeFlags[nodeChangeMsg->Node[i].NodeNum] = nodeChangeMsg->Node[i].NodeFlags;
			ClientContext->DefectCodes[nodeChangeMsg->Node[i].NodeNum] = nodeChangeMsg->Node[i].DefectCode;
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d: Flag %x, Defect %x\n", i, nodeChangeMsg->Node[i].NodeFlags, nodeChangeMsg->Node[i].DefectCode) );
		}

		NdasRaidArbiterRefreshRaidStatus( NdasrArbiter, FALSE );
	
		if (NdasrArbiter->SyncState != NDASR_SYNC_DONE) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RAID/Node status has been changed\n") );
			commonReplyMsg.Result = NRIX_RESULT_WAIT_SYNC_FOR_WRITE;

		} else {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("RAID/Node status has not been changed\n") );			
			commonReplyMsg.Result = NRIX_RESULT_NO_CHANGE;
		}

		break;
	}

	case NRIX_CMD_ACQUIRE_LOCK: {

		PNRIX_ACQUIRE_LOCK			acquireLockMsg	 = (PNRIX_ACQUIRE_LOCK) RequestMsg;
		UINT64						blockAddress	 = NTOHLL(acquireLockMsg->BlockAddress);
		UINT32						blockLength		 = NTOHL(acquireLockMsg->BlockLength);
		PNDASR_ARBITER_LOCK_CONTEXT newLock;

		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );

		DebugTrace( DBG_LURN_TRACE, ("Arbiter received ACQUIRE_LOCK message: %I64x:%x\n", blockAddress, blockLength) );

		replyLength = sizeof(NRIX_ACQUIRE_LOCK_REPLY);

		replyMsg = (PVOID)&acquireLockReply;

		RtlZeroMemory( &acquireLockReply, replyLength );

		acquireLockReply.Header.Signature	= HTONL(NRIX_SIGNATURE);
		acquireLockReply.Header.ReplyFlag	= TRUE;
		acquireLockReply.Header.Command		= RequestMsg->Command;
		acquireLockReply.Header.Length		= HTONS((UINT16)replyLength);
		acquireLockReply.Header.Sequence	= RequestMsg->Sequence;

		// Check lock list if lock overlaps with lock acquired by other client

#if DBG

		for (listEntry = ClientContext->AcquiredLockList.Flink;
			 listEntry != &ClientContext->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			 PNDASR_ARBITER_LOCK_CONTEXT lock;

			lock = CONTAINING_RECORD( listEntry, NDASR_ARBITER_LOCK_CONTEXT, ClientAcquiredLink );
		
			NDASSCSI_ASSERT( NdasRaidGetOverlappedRange( blockAddress, 
														  blockLength,
														  lock->BlockAddress, 
														  lock->BlockLength,
														  NULL, 
														  NULL ) == NDASR_RANGE_NO_OVERLAP );
		}
#endif

		do {

#if __NDAS_SCSI_BOUNDARY_CHECK__

			if (blockAddress >= NdasrArbiter->Lurn->UnitBlocks ) {

				NDASSCSI_ASSERT( FALSE );

				acquireLockReply.Header.Result = NRIX_RESULT_FAIL;
				break;	
			}

#endif

			newLock = NdasRaidArbiterAllocLock( NdasrArbiter, 
												 acquireLockMsg->LockType, 
												 acquireLockMsg->LockMode, 
												 blockAddress, 
												 blockLength );
	
			if (newLock == NULL) {

				NDASSCSI_ASSERT( FALSE );

				acquireLockReply.Header.Result = NRIX_RESULT_FAIL;
				break;	
			}
				
			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

			status = NdasRaidArbiterArrangeLockRange( NdasrArbiter, newLock, NdasrArbiter->LockRangeGranularity );

			ASSERT( status == STATUS_SUCCESS );

			NdasRaidArbiterUpdateLwrBitmapBit( NdasrArbiter, newLock, NULL );

			if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_DEGRADED) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					        ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",newLock->BlockAddress, newLock->BlockLength) );

				NdasRaidArbiterChangeOosBitmapBit( NdasrArbiter, TRUE, newLock->BlockAddress, newLock->BlockLength );
			
			} else {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Granted lock %I64x(%I64x:%Ix).\n", newLock->Id, newLock->BlockAddress, newLock->BlockLength) );
			}
			
			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

			// Need to update BMP and LWR before client start to write using this lock
			
			status = NdasRaidArbiterUpdateOnDiskOosBitmap( NdasrArbiter, FALSE );

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Failed to update bitmap\n") );

				NdasRaidArbiterFreeLock( NdasrArbiter, newLock );

				acquireLockReply.Header.Result = NRIX_RESULT_WAIT_SYNC_FOR_WRITE;
				break;
			}
			
			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

			acquireLockReply.Header.Result = NRIX_RESULT_GRANTED;
			newLock->Owner = ClientContext;
			newLock->Status = NDASR_ARBITER_LOCK_STATUS_GRANTED;

			// Add to arbiter list

			InsertTailList( &NdasrArbiter->AcquiredLockList, &newLock->ArbiterAcquiredLink );
			InterlockedIncrement( &NdasrArbiter->AcquiredLockCount );

			InsertTailList( &ClientContext->AcquiredLockList, &newLock->ClientAcquiredLink );

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		} while (0);

		if (acquireLockReply.Header.Result == NRIX_RESULT_GRANTED) {

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

	case NRIX_CMD_RELEASE_LOCK: {

		// Check lock is owned by this client.
	
		PNRIX_RELEASE_LOCK			releaseLockMsg = (PNRIX_RELEASE_LOCK) RequestMsg;
		PNDASR_ARBITER_LOCK_CONTEXT	lock;
		UINT64						lockId = NTOHLL(releaseLockMsg->LockId);

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbiter received RELEASE_LOCK message: %I64x\n", lockId) );

		NDASSCSI_ASSERT( NdasrArbiter->SyncState == NDASR_SYNC_DONE );
		
		replyLength = sizeof(NRIX_HEADER);
		replyMsg = &commonReplyMsg;

		RtlZeroMemory( &commonReplyMsg, replyLength );

		commonReplyMsg.Signature	= HTONL(NRIX_SIGNATURE);
		commonReplyMsg.ReplyFlag	= TRUE;
		commonReplyMsg.Command		= RequestMsg->Command;
		commonReplyMsg.Length		= HTONS((UINT16)replyLength);
		commonReplyMsg.Sequence		= RequestMsg->Sequence;

		// 1.0 chip does not support cache flush. 
		// Flush before releasing the lock.
	
		NdasRaidArbiterFlushDirtyCacheNdas1_0( NdasrArbiter, lockId, ClientContext );
				
		commonReplyMsg.Result = NRIX_RESULT_INVALID_LOCK_ID;

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

		// Search for matching Lock ID

		for (listEntry = NdasrArbiter->AcquiredLockList.Flink;
			 listEntry != &NdasrArbiter->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, NDASR_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
		
			if (lockId == NRIX_LOCK_ID_ALL && lock->Owner == ClientContext) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						    ("Releasing all locks - Lock %I64x:%x\n", lock->BlockAddress, lock->BlockLength) );

				// Remove from all list

				listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
				
				RemoveEntryList( &lock->ArbiterAcquiredLink );
				InitializeListHead( &lock->ArbiterAcquiredLink );	// to check bug...
				InterlockedDecrement( &NdasrArbiter->AcquiredLockCount );

				RemoveEntryList( &lock->ClientAcquiredLink );
				InitializeListHead( &lock->ClientAcquiredLink );	// to check bug...
				
				lock->Owner = NULL;

				commonReplyMsg.Result = NRIX_RESULT_SUCCESS;
				
				NdasRaidArbiterFreeLock( NdasrArbiter, lock );

				continue;

			} 
			
			if (lock->Id == lockId) {

				if (lock->Owner != ClientContext) {

					NDASSCSI_ASSERT(FALSE);	
					break;
				} 

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Freeing client's lock %I64x(%I64x:%x)\n", 
							 lock->Id, lock->BlockAddress, lock->BlockLength) );
				
				// Remove from all list

				RemoveEntryList( &lock->ArbiterAcquiredLink );
				InitializeListHead( &lock->ArbiterAcquiredLink );	// to check bug...
				InterlockedDecrement( &NdasrArbiter->AcquiredLockCount );

				RemoveEntryList( &lock->ClientAcquiredLink );
				InitializeListHead( &lock->ClientAcquiredLink );	// to check bug...

				lock->Owner = NULL;

				commonReplyMsg.Result = NRIX_RESULT_SUCCESS;

				NdasRaidArbiterFreeLock( NdasrArbiter, lock );

				break;
			}
		}

		if (lockId != NRIX_LOCK_ID_ALL) {

			NDASSCSI_ASSERT( commonReplyMsg.Result == NRIX_RESULT_SUCCESS );
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		NdasRaidArbiterUpdateLwrBitmapBit( NdasrArbiter, NULL, NULL );
		
		status = NdasRaidArbiterUpdateOnDiskOosBitmap( NdasrArbiter, FALSE );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("Failed to update OOS bitmap\n") );
		}

		break;
	}

	case NRIX_CMD_REGISTER:
//	case NRIX_CMD_UNREGISTER:
	default:

		NDASSCSI_ASSERT( FALSE );

		replyLength = sizeof(NRIX_HEADER);
		replyMsg = &commonReplyMsg;

		commonReplyMsg.Signature	= HTONL(NRIX_SIGNATURE);
		commonReplyMsg.ReplyFlag	= TRUE;
		commonReplyMsg.Command		= RequestMsg->Command;
		commonReplyMsg.Length		= HTONS((UINT16)replyLength);
		commonReplyMsg.Sequence		= RequestMsg->Sequence;
		commonReplyMsg.Result		= NRIX_RESULT_UNSUPPORTED;

		break;
	}

	// Send reply
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("DRAID Sending reply to request %s with result %s to remote client\n", 
				 NdasRixGetCmdString(RequestMsg->Command), NdasRixGetResultString(replyMsg->Result)) );

	if (ClientContext->LocalClient) {

		PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;

		ndasrReplyLocalMsg = &NdasrArbiter->Lurn->NdasrInfo->RequestChannelReply;
		InitializeListHead( &ndasrReplyLocalMsg->ListEntry );

		ndasrReplyLocalMsg->NrixHeader = &NdasrArbiter->Lurn->NdasrInfo->NrixHeader;

		RtlCopyMemory( ndasrReplyLocalMsg->NrixHeader,
					   replyMsg,
					   replyLength );

		ExInterlockedInsertTailList( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
									 &ndasrReplyLocalMsg->ListEntry,
									 &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDASSCSI_ASSERT( ClientContext->RequestConnection->ConnectionFileObject );

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
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext,
	PNRIX_HEADER			RequestMsg,
	UCHAR					Result
	)
{
	NTSTATUS			status;

	PNDASR_LOCAL_MSG	ndasrReplyLocalMsg;
	PNRIX_CHANGE_STATUS	changeStatusMsg = NULL;
	UINT32				msgLength;
	LONG				result;
	ULONG				i;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reply client with command DIRX_CMD_CHANGE_STATUS\n") );

	msgLength = SIZE_OF_NRIX_CHANGE_STATUS( NdasrArbiter->Lurn->LurnChildrenCnt );

	if (ClientContext->LocalClient) {

		ndasrReplyLocalMsg = &NdasrArbiter->Lurn->NdasrInfo->RequestChannelReply;	
		InitializeListHead( &ndasrReplyLocalMsg->ListEntry );

		changeStatusMsg = (PNRIX_CHANGE_STATUS)&NdasrArbiter->Lurn->NdasrInfo->NrixHeader;

	} else {

		changeStatusMsg = ExAllocatePoolWithTag( NonPagedPool, 
												 msgLength, 
												 NDASR_ARBITER_NOTIFY_MSG_POOL_TAG );
	}

	RtlZeroMemory( changeStatusMsg, msgLength );

	changeStatusMsg->Header.Signature		= HTONL(NRIX_SIGNATURE);
	changeStatusMsg->Header.ReplyFlag		= TRUE;
	changeStatusMsg->Header.RaidInformation = TRUE;
	changeStatusMsg->Header.Command			= RequestMsg->Command;
	changeStatusMsg->Header.Length			= HTONS((UINT16)msgLength);
	changeStatusMsg->Header.Sequence		= RequestMsg->Sequence;
	changeStatusMsg->Header.Result			= Result;
	
	changeStatusMsg->Usn				= NTOHL(NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN);
	changeStatusMsg->RaidState			= (UCHAR)NdasrArbiter->NdasrState;
	changeStatusMsg->NodeCount			= (UCHAR)NdasrArbiter->Lurn->LurnChildrenCnt;
	changeStatusMsg->OutOfSyncRoleIndex = NdasrArbiter->OutOfSyncRoleIndex;

	changeStatusMsg->WaitSyncForWrite = (NdasrArbiter->SyncState == NDASR_SYNC_DONE) ? FALSE : TRUE;  

	NDASSCSI_ASSERT( changeStatusMsg->WaitSyncForWrite );

	RtlCopyMemory( &changeStatusMsg->ConfigSetId, 
				   &NdasrArbiter->Lurn->NdasrInfo->Rmd.ConfigSetId, 
				   sizeof(changeStatusMsg->ConfigSetId) );
		
	for (i=0; i<changeStatusMsg->NodeCount; i++) {
			
		changeStatusMsg->Node[i].NodeFlags = NdasrArbiter->NodeFlags[i];
		changeStatusMsg->Node[i].NodeRole = NdasrArbiter->NodeToRoleMap[i];
	}

	if (ClientContext->LocalClient) {

		ndasrReplyLocalMsg->NrixHeader = (PNRIX_HEADER)changeStatusMsg;

		ExInterlockedInsertTailList( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.ReplyQueue,
									 &ndasrReplyLocalMsg->ListEntry,
									 &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.SpinLock );

		KeSetEvent( &NdasrArbiter->Lurn->NdasrInfo->RequestChannel.ReplyEvent,
					IO_NO_INCREMENT,
					FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDASSCSI_ASSERT( ClientContext->RequestConnection->ConnectionFileObject );

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

			NDASSCSI_ASSERT( !NT_SUCCESS(status) );

			status = STATUS_CONNECTION_DISCONNECTED;
		}

		ExFreePoolWithTag( changeStatusMsg, NDASR_ARBITER_NOTIFY_MSG_POOL_TAG );
	}

	return status;
}

NTSTATUS
NdasRaidNotifyChangeStatus (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext
	)
{
	NTSTATUS			status;

	PNDASR_LOCAL_MSG	ndasrNotifyLocalMsg;
	PNRIX_CHANGE_STATUS	changeStatusMsg = NULL;
	UINT32				msgLength;
	LONG				result;
	ULONG				i;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying client with command DIRX_CMD_CHANGE_STATUS\n") );

	msgLength = SIZE_OF_NRIX_CHANGE_STATUS( NdasrArbiter->Lurn->LurnChildrenCnt );

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg = ExAllocatePoolWithTag( NonPagedPool, 
													 sizeof(NDASR_LOCAL_MSG) + msgLength, 
													 NDASR_LOCAL_MSG_POOL_TAG );
	
		InitializeListHead( &ndasrNotifyLocalMsg->ListEntry );

		changeStatusMsg = (PNRIX_CHANGE_STATUS)(ndasrNotifyLocalMsg + 1);

	} else {

		changeStatusMsg = ExAllocatePoolWithTag( NonPagedPool, 
												 msgLength, 
												 NDASR_ARBITER_NOTIFY_MSG_POOL_TAG );
	}

	RtlZeroMemory( changeStatusMsg, msgLength );

	changeStatusMsg->Header.Signature = HTONL(NRIX_SIGNATURE);
	changeStatusMsg->Header.Command   = NRIX_CMD_CHANGE_STATUS;
	changeStatusMsg->Header.Length    = HTONS((UINT16)msgLength);
	changeStatusMsg->Header.Sequence  = HTONS(ClientContext->NotifySequence);
	
	ClientContext->NotifySequence++;

	changeStatusMsg->Usn				= NTOHL(NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN);
	changeStatusMsg->RaidState			= (UCHAR)NdasrArbiter->NdasrState;
	changeStatusMsg->NodeCount			= (UCHAR)NdasrArbiter->Lurn->LurnChildrenCnt;
	changeStatusMsg->OutOfSyncRoleIndex = NdasrArbiter->OutOfSyncRoleIndex;

	changeStatusMsg->WaitSyncForWrite = (NdasrArbiter->SyncState == NDASR_SYNC_DONE) ? FALSE : TRUE;  
		
	RtlCopyMemory( &changeStatusMsg->ConfigSetId, 
				   &NdasrArbiter->Lurn->NdasrInfo->Rmd.ConfigSetId, 
				   sizeof(changeStatusMsg->ConfigSetId) );
		
	for (i=0; i<changeStatusMsg->NodeCount; i++) {
			
		changeStatusMsg->Node[i].NodeFlags = NdasrArbiter->NodeFlags[i];
		changeStatusMsg->Node[i].NodeRole = NdasrArbiter->NodeToRoleMap[i];
	}

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg->NrixHeader = (PNRIX_HEADER)changeStatusMsg;

		ExInterlockedInsertTailList( &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
									 &ndasrNotifyLocalMsg->ListEntry,
									 &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

		KeSetEvent( &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
				    IO_NO_INCREMENT,
					FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDASSCSI_ASSERT( ClientContext->NotificationConnection->ConnectionFileObject );

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

		ExFreePoolWithTag( changeStatusMsg, NDASR_ARBITER_NOTIFY_MSG_POOL_TAG );
	}

	return status;
}

NTSTATUS
NdasRaidNotifyStatusSynced (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext
	)
{
	NTSTATUS			status;
	PNDASR_LOCAL_MSG	ndasrNotifyLocalMsg;
	NRIX_HEADER			statusSyncedMsg;
	UINT32				msgLength;
	LONG				result;
	KIRQL				oldIrql;


	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying client with command NRIX_CMD_STATUS_SYNCED\n") );

	msgLength = sizeof(NRIX_HEADER);

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg = ExAllocatePoolWithTag( NonPagedPool, 
													 sizeof(NDASR_LOCAL_MSG) + msgLength, 
													 NDASR_LOCAL_MSG_POOL_TAG );

		InitializeListHead( &ndasrNotifyLocalMsg->ListEntry );

	}

	RtlZeroMemory( &statusSyncedMsg, msgLength );

	statusSyncedMsg.Signature = HTONL(NRIX_SIGNATURE);
	statusSyncedMsg.Command	  = NRIX_CMD_STATUS_SYNCED;
	statusSyncedMsg.Length	  = HTONS((UINT16)msgLength);
	statusSyncedMsg.Sequence  = HTONS(ClientContext->NotifySequence);
	
	ClientContext->NotifySequence++;

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg->NrixHeader = (PNRIX_HEADER)(ndasrNotifyLocalMsg+1);
		RtlCopyMemory( ndasrNotifyLocalMsg->NrixHeader,
					   &statusSyncedMsg,
					   sizeof(statusSyncedMsg) );

		ExInterlockedInsertTailList( &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
									 &ndasrNotifyLocalMsg->ListEntry,
									 &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

		status = KeSetEvent( &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
							 IO_NO_INCREMENT,
							 FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDASSCSI_ASSERT( ClientContext->NotificationConnection->ConnectionFileObject );

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

			ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
			RemoveEntryList( &ClientContext->Link );
			InsertTailList( &NdasrArbiter->TerminatedClientQueue, &ClientContext->Link );
			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );				
		} 
	}

	return status;
}

NTSTATUS
NdasRaidNotifyRetire (
	PNDASR_ARBITER			NdasrArbiter,
	PNDASR_CLIENT_CONTEXT	ClientContext
	)
{
	NTSTATUS			status;
	PNDASR_LOCAL_MSG	ndasrNotifyLocalMsg;
	NRIX_HEADER			retireMsg;
	UINT32				msgLength;
	LONG				result;

	UNREFERENCED_PARAMETER( NdasrArbiter );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Notifying client with command NRIX_CMD_RETIRE\n") );

	msgLength = sizeof(NRIX_HEADER);

	if (ClientContext->LocalClient) {

		ndasrNotifyLocalMsg = ExAllocatePoolWithTag( NonPagedPool, 
													 sizeof(NDASR_LOCAL_MSG) + msgLength, 
													 NDASR_LOCAL_MSG_POOL_TAG );

		InitializeListHead( &ndasrNotifyLocalMsg->ListEntry );

	}

	RtlZeroMemory( &retireMsg, msgLength );

	retireMsg.Signature = HTONL(NRIX_SIGNATURE);
	retireMsg.Command   = NRIX_CMD_RETIRE;
	retireMsg.Length    = HTONS((UINT16)msgLength);
	retireMsg.Sequence  = HTONS(ClientContext->NotifySequence);
	
	ClientContext->NotifySequence++;

	if (ClientContext->LocalClient) {

		NDASSCSI_ASSERT( FALSE );

		ndasrNotifyLocalMsg->NrixHeader = (PNRIX_HEADER)(ndasrNotifyLocalMsg+1);
		RtlCopyMemory( ndasrNotifyLocalMsg->NrixHeader,
					   &retireMsg,
					   sizeof(retireMsg) );

		ExInterlockedInsertTailList( &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.RequestQueue,
									 &ndasrNotifyLocalMsg->ListEntry,
									 &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.SpinLock );

		status = KeSetEvent( &NdasrArbiter->Lurn->NdasrInfo->NotitifyChannel.RequestEvent,
							 IO_NO_INCREMENT,
							 FALSE );

		status = STATUS_SUCCESS;

	} else {

		NDASSCSI_ASSERT( ClientContext->NotificationConnection->ConnectionFileObject );

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
NdasRaidArbiterRefreshRaidStatus (
	IN PNDASR_ARBITER	NdasrArbiter,
	IN BOOLEAN			ForceChange
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

	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );

	// Gather each client's node information
	
	for (nidx = 0; nidx < NdasrArbiter->Lurn->LurnChildrenCnt; nidx++) {

		ACQUIRE_SPIN_LOCK( &NdasrArbiter->Lurn->LurnChildren[nidx]->SpinLock, &oldIrql2 );

		newNodeFlags[nidx] = NdasRaidLurnStatusToNodeFlag( NdasrArbiter->Lurn->LurnChildren[nidx]->LurnStatus );
		
		if (LurnGetCauseOfFault(NdasrArbiter->Lurn->LurnChildren[nidx]) & (LURN_FCAUSE_BAD_SECTOR | LURN_FCAUSE_BAD_DISK)) {

			newNodeFlags[nidx]	 = NRIX_NODE_FLAG_DEFECTIVE;
			newDefectCodes[nidx] = ((LurnGetCauseOfFault(NdasrArbiter->Lurn->LurnChildren[nidx]) & LURN_FCAUSE_BAD_SECTOR) ?
									NRIX_NODE_DEFECT_BAD_SECTOR : NRIX_NODE_DEFECT_BAD_DISK );
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->Lurn->LurnChildren[nidx]->SpinLock, oldIrql2 );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting initial node %d flag: %d\n", nidx, NdasrArbiter->NodeFlags[nidx]) );

		for (listEntry = NdasrArbiter->ClientQueue.Flink;
			 listEntry != &NdasrArbiter->ClientQueue;
			 listEntry = listEntry->Flink) {

			clientContext = CONTAINING_RECORD( listEntry, NDASR_CLIENT_CONTEXT, Link );
	
			// Flag priority:  NRIX_NODE_FLAG_DEFECTIVE > NRIX_NODE_FLAG_STOP > NRIX_NODE_FLAG_RUNNING > NRIX_NODE_FLAG_UNKNOWN

			if (FlagOn(newNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {
			
				if (FlagOn(clientContext->NodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Node %d: Arbiter->NodeFlag %x Old flag %x, Client Flag %x\n", 
								 nidx, NdasrArbiter->DefectCodes[nidx], newDefectCodes[nidx], clientContext->DefectCodes[nidx] ) );

					SetFlag( newDefectCodes[nidx], clientContext->DefectCodes[nidx] );
				}

				continue;
			}
			
			if (FlagOn(clientContext->NodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {
	
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Node %d: Arbiter->NodeFlag %x Old flag %x, Client Flag %x\n", 
							 nidx, NdasrArbiter->NodeFlags[nidx], newNodeFlags[nidx], clientContext->NodeFlags[nidx] ) );

				newNodeFlags[nidx] = NRIX_NODE_FLAG_DEFECTIVE;
				SetFlag( newDefectCodes[nidx], clientContext->DefectCodes[nidx] );

				continue;
			}

			if (FlagOn(newNodeFlags[nidx], NRIX_NODE_FLAG_STOP)) {

				continue;
			}

			if (FlagOn(clientContext->NodeFlags[nidx], NRIX_NODE_FLAG_STOP)) {
							
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Node %d: Arbiter->NodeFlag %x Old flag %x, Client Flag %x\n", 
							 nidx, NdasrArbiter->NodeFlags[nidx], newNodeFlags[nidx], clientContext->NodeFlags[nidx] ) );

				newNodeFlags[nidx] = NRIX_NODE_FLAG_STOP;

				continue;
			}
			
			if (FlagOn(newNodeFlags[nidx], NRIX_NODE_FLAG_RUNNING)) {

				continue;
			}

			if (FlagOn(clientContext->NodeFlags[nidx], NRIX_NODE_FLAG_RUNNING)) {
							
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Node %d: Arbiter->NodeFlag %x Old flag %x, Client Flag %x\n", 
							 nidx, NdasrArbiter->NodeFlags[nidx], newNodeFlags[nidx], clientContext->NodeFlags[nidx] ) );

				newNodeFlags[nidx] = NRIX_NODE_FLAG_RUNNING;

				continue;
			}

			NDASSCSI_ASSERT( FALSE );
		}

		if (FlagOn(NdasrArbiter->NodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {

			NDASSCSI_ASSERT( FlagOn(newNodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE) );

			if (NdasrArbiter->DefectCodes[nidx] != newDefectCodes[nidx]) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
							("NdasrArbiter->DefectCodes[nidx] = %x, newDefectCodes[nidx] = %x\n", 
							 NdasrArbiter->DefectCodes[nidx], newDefectCodes[nidx]) );
				
				SetFlag( NdasrArbiter->DefectCodes[nidx], newDefectCodes[nidx] );

				changed = TRUE;
			}
		}
		
		if (NdasrArbiter->NodeFlags[nidx] != newNodeFlags[nidx]) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Changing Node %d flags from %02x to %02x\n", nidx, NdasrArbiter->NodeFlags[nidx], newNodeFlags[nidx]) );
			
			NdasrArbiter->NodeFlags[nidx] = newNodeFlags[nidx];

			changed = TRUE;
		}
	}

	if (changed == FALSE) {

		if (ForceChange != TRUE) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasrArbiter->NdasrState = %x\n", NdasrArbiter->NdasrState) );

			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			return FALSE;
		}
	}

	for (nidx = 0; nidx < NdasrArbiter->Lurn->LurnChildrenCnt; nidx++) {

		NDASSCSI_ASSERT( NdasrArbiter->NodeFlags[nidx] == NRIX_NODE_FLAG_RUNNING	||
						 NdasrArbiter->NodeFlags[nidx] == NRIX_NODE_FLAG_DEFECTIVE	||
						 NdasrArbiter->NodeFlags[nidx] == NRIX_NODE_FLAG_STOP );
	}

	// Test new RAID status only when needed, i.e: node has changed or first time.
	
	newNdasrState = NRIX_RAID_STATE_NORMAL;

	if (RtlNumberOfSetBits(&NdasrArbiter->OosBmpHeader) != 0) {
		
		newNdasrState = NRIX_RAID_STATE_OUT_OF_SYNC;
	}

	for (ridx = 0; ridx < NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { // i : role index
			
		if (!FlagOn(NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_RUNNING)) {

			switch (newNdasrState) {

			case NRIX_RAID_STATE_NORMAL: {

				NdasrArbiter->OutOfSyncRoleIndex = ridx;
				newNdasrState = NRIX_RAID_STATE_DEGRADED;

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Setting new out of sync role: %d\n", ridx) );

				break;
			}

			case NRIX_RAID_STATE_OUT_OF_SYNC: {

				if (NdasrArbiter->OutOfSyncRoleIndex == ridx) {
					
					newNdasrState = NRIX_RAID_STATE_DEGRADED;
				
				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
								("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrArbiter->RoleToNodeMap[ridx]) );

					//NDASSCSI_ASSERT( FALSE );

					newNdasrState = NRIX_RAID_STATE_FAILED;
				}

				break;
			}

			case NRIX_RAID_STATE_DEGRADED: {

				NDASSCSI_ASSERT( NdasrArbiter->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE && NdasrArbiter->OutOfSyncRoleIndex != ridx );
				newNdasrState = NRIX_RAID_STATE_FAILED;

				//NDASSCSI_ASSERT( FALSE );

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Role %d(node %d) also failed. RAID failure\n", ridx, NdasrArbiter->RoleToNodeMap[ridx]) );

				break;
			}

			default:

				break;			
			}
		}
	}

	NDASSCSI_ASSERT( newNdasrState == NRIX_RAID_STATE_NORMAL		||
					 newNdasrState == NRIX_RAID_STATE_DEGRADED		||
					 newNdasrState == NRIX_RAID_STATE_OUT_OF_SYNC	||
					 newNdasrState == NRIX_RAID_STATE_FAILED );

	if (ForceChange != TRUE) {

		NDASSCSI_ASSERT( NdasrArbiter->NdasrState != newNdasrState );
	}

	if (NdasrArbiter->NdasrState != newNdasrState) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("*************Changing DRAID Status from %x to %x****************\n", 
					 NdasrArbiter->NdasrState, newNdasrState) );

		if (newNdasrState == NRIX_RAID_STATE_NORMAL) {

			NdasrArbiter->OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;
		}

		if (newNdasrState == NRIX_RAID_STATE_OUT_OF_SYNC) {

			if (NdasrArbiter->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {

				NdasrArbiter->OutOfSyncRoleIndex = NdasrArbiter->NodeToRoleMap[NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount-1];
			}
		}

		if (newNdasrState == NRIX_RAID_STATE_DEGRADED) {
		
			PNDASR_ARBITER_LOCK_CONTEXT Lock;

			KeQueryTickCount( &NdasrArbiter->DegradedSince );

			// We need to merge all LWR to OOS bitmap
			//	because defective/stopped node may have lost data in disk's write-buffer.

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Merging LWRs to OOS bitmap\n") );

			for (listEntry = NdasrArbiter->AcquiredLockList.Flink;
				 listEntry != &NdasrArbiter->AcquiredLockList;
				 listEntry = listEntry->Flink) {

				Lock = CONTAINING_RECORD( listEntry, NDASR_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink );
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
							("Merging LWR %I64x:%x to OOS bitmap\n", Lock->BlockAddress, Lock->BlockLength) );

				NdasRaidArbiterChangeOosBitmapBit( NdasrArbiter, TRUE, Lock->BlockAddress, Lock->BlockLength );
			}
		}

		NdasrArbiter->NdasrState = newNdasrState;
	}

	NdasrArbiter->SyncState = NDASR_SYNC_REQUIRED;
	
	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

	changed = NdasRaidArbiterUpdateInCoreRmd( NdasrArbiter );
	
	if (changed == FALSE) {

		if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_INITIALIZING) &&
			!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN)) {

			NDASSCSI_ASSERT( FALSE );
		}
	}
	
	if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_NORMAL) {

		status = NdasRaidArbiterUpdateOnDiskOosBitmap( NdasrArbiter, TRUE );

		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}

		status = NdasRaidArbiterWriteRmd( NdasrArbiter, &NdasrArbiter->Lurn->NdasrInfo->Rmd );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}

	} else if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_DEGRADED ||
			   NdasrArbiter->NdasrState == NRIX_RAID_STATE_OUT_OF_SYNC) {

		status = NdasRaidArbiterWriteRmd( NdasrArbiter, &NdasrArbiter->Lurn->NdasrInfo->Rmd );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}

		status = NdasRaidArbiterUpdateOnDiskOosBitmap( NdasrArbiter, TRUE );

		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
			goto retry;
		}
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasrArbiter->NdasrState = %x\n", NdasrArbiter->NdasrState) );

	return changed;
}

BOOLEAN
NdasRaidArbiterUpdateInCoreRmd (
	IN PNDASR_ARBITER	NdasrArbiter
	) 
{
	NDAS_RAID_META_DATA newRmd = {0};
	UINT32				nidx, ridx;
	UINT32				j;
	UCHAR				nodeFlags;


	newRmd.Signature	= NdasrArbiter->Lurn->NdasrInfo->Rmd.Signature;
	newRmd.RaidSetId	= NdasrArbiter->Lurn->NdasrInfo->Rmd.RaidSetId;
	newRmd.uiUSN		= NdasrArbiter->Lurn->NdasrInfo->Rmd.uiUSN;
	newRmd.ConfigSetId	= NdasrArbiter->ConfigSetId;

	if (!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_INITIALIZING) &&
		!FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN)) {

		NDASSCSI_ASSERT( NdasrArbiter->Lurn->NdasrInfo->Rmd.state == NDAS_RAID_META_DATA_STATE_MOUNTED ||
						 NdasrArbiter->Lurn->NdasrInfo->Rmd.state == (NDAS_RAID_META_DATA_STATE_MOUNTED | NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED ));
	}

	if (FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_TERMINATED) ||
		FlagOn(NdasrArbiter->Status, NDASR_ARBITER_STATUS_SHUTDOWN) && IsListEmpty(&NdasrArbiter->AcquiredLockList)) {

		newRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;

	} else {

		newRmd.state = NDAS_RAID_META_DATA_STATE_MOUNTED;
	}

	// Keep NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED.

	switch (NdasrArbiter->NdasrState) {

	case NRIX_RAID_STATE_DEGRADED:
		
		if (!FlagOn(NdasrArbiter->Lurn->NdasrInfo->Rmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );
		}

		SetFlag( newRmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED );		

		break;

	case NRIX_RAID_STATE_NORMAL:
	case NRIX_RAID_STATE_OUT_OF_SYNC:
		
		if (FlagOn(NdasrArbiter->Lurn->NdasrInfo->Rmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Clearing NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );			
		}

		break;
	
	default: // Keep previous flag
	
		newRmd.state |= (NdasrArbiter->Lurn->NdasrInfo->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED);

		if (FlagOn(NdasrArbiter->Lurn->NdasrInfo->Rmd.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Keep marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );
		}

		break;
	}

	RtlZeroMemory( newRmd.ArbiterInfo, sizeof(newRmd.ArbiterInfo) );
		
	for (j=0; j<NDAS_DRAID_ARBITER_ADDR_COUNT; j++) {

		newRmd.ArbiterInfo[j].Type = NDAS_DRAID_ARBITER_TYPE_NONE;
	}
	
	if (FlagOn(newRmd.state, NDAS_RAID_META_DATA_STATE_MOUNTED) &&
		NdasrArbiter->NdasrState != NRIX_RAID_STATE_FAILED) {

		UINT32					usedAddrCount;
		PUCHAR					macAddr;
		PLURNEXT_IDE_DEVICE		ideDisk;
		
		RtlZeroMemory( newRmd.ArbiterInfo, sizeof(newRmd.ArbiterInfo) );

		ASSERT( sizeof(newRmd.ArbiterInfo) == 12*8 );
		
		// Get list of bind address from each children.
		
		usedAddrCount = 0;
		
		for (nidx = 0; nidx < NdasrArbiter->Lurn->LurnChildrenCnt; nidx++) {
		
			// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
			
			if (!FlagOn(NdasrArbiter->NodeFlags[nidx], NRIX_NODE_FLAG_RUNNING) ||
				FlagOn(NdasrArbiter->NodeFlags[nidx], NRIX_NODE_FLAG_DEFECTIVE)) {

				continue;
			}

			ideDisk = (PLURNEXT_IDE_DEVICE)NdasrArbiter->Lurn->LurnChildren[nidx]->LurnExtension;
			
			if (!ideDisk) {

				NDASSCSI_ASSERT( FALSE );
				continue;
			}

#if !__NDAS_SCSI_LPXTDI_V2__
			macAddr = ((PTDI_ADDRESS_LPX)&ideDisk->LanScsiSession->BindAddress.Address[0].Address)->Node;
#else

			macAddr = ((PTDI_ADDRESS_LPX)ideDisk->LanScsiSession->NdasBindAddress.Address)->Node;
#endif
			
			// Search address is already recorded.
			
			for (j=0; j < usedAddrCount; j++) {
			
				if (RtlCompareMemory(newRmd.ArbiterInfo[j].Addr, macAddr, 6) == 6) {
					
					// This bind address is alreay in entry. Skip

					break;
				}
			}

			if (j != usedAddrCount) {

				continue;
			}

			newRmd.ArbiterInfo[usedAddrCount].Type = NDAS_DRAID_ARBITER_TYPE_LPX;
				
			RtlCopyMemory( newRmd.ArbiterInfo[usedAddrCount].Addr, macAddr, 6 );
			usedAddrCount++;

			if (usedAddrCount >= NDAS_DRAID_ARBITER_ADDR_COUNT) {
				
				break;
			}
		}

		NDASSCSI_ASSERT( usedAddrCount > 0 );	
	}

	for (ridx = 0; ridx < NdasrArbiter->Lurn->LurnChildrenCnt; ridx++) { // i is role index.
		
		newRmd.UnitMetaData[ridx].iUnitDeviceIdx = NdasrArbiter->RoleToNodeMap[ridx];
		
		nodeFlags = NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[ridx]];
		
		if (FlagOn(nodeFlags, NRIX_NODE_FLAG_DEFECTIVE)) {
		
			UCHAR defectCode = NdasrArbiter->DefectCodes[NdasrArbiter->RoleToNodeMap[ridx]];

			// Relative defect code such as RMD mismatch is not needed to be written to disk
			// Record physical defects and spare-used flag only

			SetFlag( newRmd.UnitMetaData[ridx].UnitDeviceStatus, NdasRaidNodeDefectCodeToRmdUnitStatus(defectCode) );
		}
		
		if (ridx == NdasrArbiter->OutOfSyncRoleIndex) {

			// Only one unit can be out-of-sync.

			SetFlag( newRmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED );
		}

		if (ridx >= NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount) {

			SetFlag( newRmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_SPARE );
		}
	}

	SET_RMD_CRC( crc32_calc, newRmd );

	if (RtlCompareMemory(&newRmd, &NdasrArbiter->Lurn->NdasrInfo->Rmd, sizeof(NDAS_RAID_META_DATA)) != sizeof(NDAS_RAID_META_DATA)) {
		
		newRmd.uiUSN++;

		SET_RMD_CRC( crc32_calc, newRmd );
		
		RtlCopyMemory( &NdasrArbiter->Lurn->NdasrInfo->Rmd, &newRmd, sizeof(NDAS_RAID_META_DATA) );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Changing in memory RMD\n") );
	
		return TRUE;
	
	} else {
	
		SET_RMD_CRC( crc32_calc, newRmd );

		return FALSE;
	}
}

NTSTATUS
NdasRaidArbiterWriteRmd (
	IN  PNDASR_ARBITER			NdasrArbiter,
	OUT PNDAS_RAID_META_DATA	Rmd
	)
{
	NTSTATUS status;
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("IN\n") );

	status = NdasRaidArbiterWriteMetaSync( NdasrArbiter, (PUCHAR) Rmd, (-1*NDAS_BLOCK_LOCATION_RMD_T), 1, TRUE );
	
	if (status != STATUS_SUCCESS) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed update second RMD\n") );		

		return status;
	}
		
	status = NdasRaidArbiterWriteMetaSync( NdasrArbiter, (PUCHAR) Rmd, (-1*NDAS_BLOCK_LOCATION_RMD), 1, TRUE );
	
	if (status != STATUS_SUCCESS) {
	
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed update first RMD\n") );		
	
		return status;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("OUT\n") );

	return status;
}

NTSTATUS
NdasRaidArbiterWriteMetaSync (
	IN PNDASR_ARBITER	NdasrArbiter,
	IN PUCHAR			BlockBuffer,
	IN INT64			BlockAddr,
	IN UINT32			TrasferBlocks, 	// in sector
	IN BOOLEAN			RelativeAddress
	) 
 {
	NTSTATUS			status = STATUS_SUCCESS;
	
	UCHAR				ridx;
	PLURELATION_NODE	lurnChildren[MAX_NDASR_MEMBER_DISK] = {0};
	UCHAR				lurnCount;	
	UCHAR				lidx;

	NDASSCSI_ASSERT( BlockAddr > 0 );
	NDASSCSI_ASSERT( FlagOn(NdasrArbiter->Lurn->AccessRight, GENERIC_WRITE) ||
					 NdasrArbiter->Lurn->NdasrInfo->NdasrClient == NULL || 
					 FlagOn(((PNDASR_CLIENT)(NdasrArbiter->Lurn->NdasrInfo->NdasrClient))->Status, NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE) );

	lurnCount = 0;

	for (ridx = 0; ridx < NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; ridx++) {
	
		if (ridx == NdasrArbiter->OutOfSyncRoleIndex) {

			continue;
		}

		if (!FlagOn(NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", 
						 NdasrArbiter->RoleToNodeMap[ridx], NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[ridx]], 
						 NdasrArbiter->DefectCodes[NdasrArbiter->RoleToNodeMap[ridx]]) );

			continue;
		}
			
		lurnChildren[lurnCount++] = NdasrArbiter->Lurn->LurnChildren[NdasrArbiter->RoleToNodeMap[ridx]];
	}

	if (NdasrArbiter->OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE) {
		
		if (!FlagOn(NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]], NRIX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", 
						 NdasrArbiter->RoleToNodeMap[ridx], NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]], 
						 NdasrArbiter->DefectCodes[NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]]) );
		
		} else {

			lurnChildren[lurnCount++] = 
				NdasrArbiter->Lurn->LurnChildren[NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]];
		}
	}

	if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_NORMAL ||
		NdasrArbiter->NdasrState == NRIX_RAID_STATE_OUT_OF_SYNC) {

		NDASSCSI_ASSERT( lurnCount == NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount );
	
	} else if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_DEGRADED) {

		NDASSCSI_ASSERT( lurnCount == NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount - 1 );
	
	} else {

		NDASSCSI_ASSERT( lurnCount < NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount - 1 );
	}
 
	if (lurnCount < NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount - 1) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_CLUSTER_NODE_UNREACHABLE;
	}

	for (ridx = NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; ridx < NdasrArbiter->Lurn->LurnChildrenCnt; ridx++) {
	
		if (!FlagOn(NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[ridx]], NRIX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", 
						 NdasrArbiter->RoleToNodeMap[ridx], NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[ridx]],
						 NdasrArbiter->DefectCodes[NdasrArbiter->RoleToNodeMap[ridx]]) );

			continue;
		}
					
		lurnChildren[lurnCount++] = NdasrArbiter->Lurn->LurnChildren[NdasrArbiter->RoleToNodeMap[ridx]];
	}

	// Flush all disk before updating metadata because updated metadata information is valid under assumption that all written data is on disk.
	
	for (lidx = 0; lidx < lurnCount; lidx++) { 

		PLURNEXT_IDE_DEVICE	ideDisk;	


		NDASSCSI_ASSERT( NdasrArbiter->NodeFlags[lurnChildren[lidx]->LurnChildIdx] == NRIX_NODE_FLAG_RUNNING );
		
		if (!LURN_IS_RUNNING(NdasrArbiter->Lurn->LurnChildren[lurnChildren[lidx]->LurnChildIdx]->LurnStatus)) {
			
			//NDASSCSI_ASSERT( !FlagOn(NdasrArbiter->Lurn->NdasrInfo->LocalNodeFlags[lurnChildren[ridx]->LurnChildIdx], NRIX_NODE_FLAG_RUNNING) );
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

				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to flush node %d.\n", lurnChildren[lidx]->LurnChildIdx) );
			}
		}

		if (status == STATUS_SUCCESS) {

			KeEnterCriticalRegion();
			ExAcquireResourceExclusiveLite( &NdasrArbiter->Lurn->NdasrInfo->BufLockResource, TRUE );

			status = NdasRaidLurnExecuteSynchrously( lurnChildren[lidx], 
													  SCSIOP_WRITE16,
													  TRUE,
													  TRUE,
													  BlockBuffer, 
													  BlockAddr, 
													  TrasferBlocks,
													  RelativeAddress );

			ExReleaseResourceLite( &NdasrArbiter->Lurn->NdasrInfo->BufLockResource );
			KeLeaveCriticalRegion();

			if (status != STATUS_SUCCESS) {

				NDASSCSI_ASSERT( status == STATUS_CLUSTER_NODE_UNREACHABLE );
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
NdasRaidArbiterArrangeLockRange (
	PNDASR_ARBITER				NdasrArbiter,
	PNDASR_ARBITER_LOCK_CONTEXT Lock,
	UINT32						Granularity
	) 
{
	UNREFERENCED_PARAMETER( NdasrArbiter );

	Lock->BlockAddress = (Lock->BlockAddress/Granularity) * Granularity;
	Lock->BlockLength  = Granularity;

	if ((Lock->BlockAddress + Lock->BlockLength) > NdasrArbiter->Lurn->UnitBlocks) {

		Lock->BlockLength = (UINT32)(NdasrArbiter->Lurn->UnitBlocks - Lock->BlockLength);

		NDASSCSI_ASSERT( Lock->BlockLength < NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit );
	}

	NDASSCSI_ASSERT( (Lock->BlockAddress + Lock->BlockLength) <= NdasrArbiter->Lurn->UnitBlocks );

	return STATUS_SUCCESS;

#if 0

	UINT64	startAddr; // inclusive
	UINT32	blockLength;


	UNREFERENCED_PARAMETER( NdasrArbiter );

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
NdasRaidArbiterUseSpareIfNeeded (
	IN  PNDASR_ARBITER	NdasrArbiter,
	OUT PBOOLEAN		SpareUsed
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;
	BOOLEAN		spareUsed = FALSE;
	UINT32		i;
	

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	
	// Check this degraded state is long enough to use spare.
		
	if (NdasrArbiter->NdasrState == NRIX_RAID_STATE_DEGRADED) {
	
		static BOOLEAN	spareHoldingTimeoutMsgShown = FALSE;
		LARGE_INTEGER	currentTick;
		
		KeQueryTickCount( &currentTick );
		
		if (((currentTick.QuadPart - NdasrArbiter->DegradedSince.QuadPart) * KeQueryTimeIncrement()) < NDAS_RAID_SPARE_HOLDING_TIMEOUT) {
		
			if (!spareHoldingTimeoutMsgShown) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Fault device is not alive. %d sec left before using spare\n",
							 (ULONG)((NDAS_RAID_SPARE_HOLDING_TIMEOUT-((currentTick.QuadPart - NdasrArbiter->DegradedSince.QuadPart) * KeQueryTimeIncrement())) / HZ)) );

				spareHoldingTimeoutMsgShown = TRUE;
			}

		} else {
			
			BOOLEAN spareFound = FALSE;
			UCHAR	spareNode = 0;

			spareHoldingTimeoutMsgShown = FALSE;
			
			// Change RAID maps to use spare disk and set all bitmap dirty.
			
			// Find running spare
			
			for (i = NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; i < NdasrArbiter->Lurn->LurnChildrenCnt; i++) { // i is indexed by role
				
				if ((NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[i]] & NRIX_NODE_FLAG_RUNNING) && 
					!(NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[i]] & NRIX_NODE_FLAG_DEFECTIVE)) {

					spareFound = TRUE;
					spareNode = NdasrArbiter->RoleToNodeMap[i];
					
					break;
				}
			}

			if (spareFound) {

				UCHAR spareRole, oosNode;
				
				// Swap defective disk with spare disk
				
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
							("Running spare disk found. Swapping defective node %d(role %d) with spare %d\n",
							 NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex], NdasrArbiter->OutOfSyncRoleIndex, spareNode) );

				NDASSCSI_ASSERT( spareNode < NdasrArbiter->Lurn->LurnChildrenCnt );
				NDASSCSI_ASSERT( NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]] & NRIX_NODE_FLAG_STOP ||
								 NdasrArbiter->NodeFlags[NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]] & NRIX_NODE_FLAG_DEFECTIVE );

				oosNode = NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex];
				spareRole = NdasrArbiter->NodeToRoleMap[spareNode];

				NdasrArbiter->NodeToRoleMap[oosNode] = spareRole;
				NdasrArbiter->NodeToRoleMap[spareNode] = NdasrArbiter->OutOfSyncRoleIndex;

				NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex] = spareNode;
				NdasrArbiter->RoleToNodeMap[spareRole] = oosNode;
				
				// Replaced node is still out-of-sync. Keep Arbiter->OutOfSyncRoleIndex
				
				NdasrArbiter->NodeFlags[oosNode] |= NRIX_NODE_FLAG_DEFECTIVE;
				NdasrArbiter->DefectCodes[oosNode] |= NRIX_NODE_DEFECT_REPLACED_BY_SPARE;
				
				// Set all bitmap dirty
	
				NdasRaidArbiterChangeOosBitmapBit( NdasrArbiter, TRUE, 0, NdasrArbiter->Lurn->UnitBlocks );
				spareUsed = TRUE;
			
			} else {
			
				if (NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount < NdasrArbiter->Lurn->LurnChildrenCnt) {

					DebugTrace( DBG_LURN_TRACE, ("Spare disk is not running\n") );
				
				} else {

				}
			}
		}
	}

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

	if (spareUsed) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Creating new config set ID\n") );
		
		status = ExUuidCreate( &NdasrArbiter->ConfigSetId );

		NDASSCSI_ASSERT( status == STATUS_SUCCESS );

		*SpareUsed = spareUsed;
	}

	return status;
}

NTSTATUS
NdasRaidArbiterFlushDirtyCacheNdas1_0 (
	IN PNDASR_ARBITER			NdasrArbiter,
	IN UINT64					LockId,
	IN PNDASR_CLIENT_CONTEXT	ClientContext
	) 
{
	KIRQL						oldIrql;
	PLIST_ENTRY					listEntry;
	PNDASR_ARBITER_LOCK_CONTEXT lock;
	ULONG						acquiredLockCount;
	ULONG						lockIte;
	NTSTATUS					status;

	struct _LockAddrLenPair {
	
		UINT64 LockAddress;	// in sector
		UINT32 LockLength;

	} *lockAddrLenPair;
	
	PLURELATION_NODE	lurn;
	PLURNEXT_IDE_DEVICE ideDisk;
	UINT32				i;
	

	status = STATUS_SUCCESS;
	
	for(i = 0; i < NdasrArbiter->Lurn->LurnChildrenCnt; i++) {

		if ((NdasrArbiter->NodeFlags[i] & NRIX_NODE_FLAG_DEFECTIVE) || !(NdasrArbiter->NodeFlags[i] & NRIX_NODE_FLAG_RUNNING)) {
		
			continue;
		}

		if (LURN_STATUS_RUNNING != NdasrArbiter->Lurn->LurnChildren[i]->LurnStatus) {
		
			continue;
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)NdasrArbiter->Lurn->LurnChildren[i]->LurnExtension;
		
		if (ideDisk->LuHwData.HwVersion != LANSCSIIDE_VERSION_1_0) {
			
			continue;
		}
		
		ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );	
		
		lockAddrLenPair = ExAllocatePoolWithTag( NonPagedPool, 
												 NdasrArbiter->AcquiredLockCount * sizeof(struct _LockAddrLenPair), 
												 NDASR_ARBITER_LOCK_ADDRLEN_PAIR_POOL_TAG );

		if (lockAddrLenPair == NULL) {
			
			NDASSCSI_ASSERT( FALSE );
			RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		acquiredLockCount = 0;
		
		for (listEntry = NdasrArbiter->AcquiredLockList.Flink;
			 listEntry != &NdasrArbiter->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			if (acquiredLockCount >= (ULONG)NdasrArbiter->AcquiredLockCount) {
				
				NDASSCSI_ASSERT( FALSE );
				break;
			}

			lock = CONTAINING_RECORD( listEntry, NDASR_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink );
			
			if (lock->Owner == ClientContext &&
				(lock->Id == LockId || lock->Id == NRIX_LOCK_ID_ALL)) {
				
				lockAddrLenPair[acquiredLockCount].LockAddress = lock->BlockAddress;
				lockAddrLenPair[acquiredLockCount].LockLength = lock->BlockLength;
				acquiredLockCount++;
			}
		}

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		status = STATUS_SUCCESS;

		lurn = NdasrArbiter->Lurn->LurnChildren[i];
		
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

				status = NdasRaidLurnExecuteSynchrously( NdasrArbiter->Lurn->LurnChildren[i], 
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

		ExFreePoolWithTag( lockAddrLenPair, NDASR_ARBITER_LOCK_ADDRLEN_PAIR_POOL_TAG );			
		
		if (!NT_SUCCESS(status)) {

			break;
		}
	}

	return status;
}

NTSTATUS 
NdasRaidRebuildInitiate (
	PNDASR_ARBITER NdasrArbiter
	) 
{
	NTSTATUS	status;

	KIRQL		oldIrql;
	UINT64		bitToRecovery;

	UINT64		blockAddr;
	UINT32		blockLength;

	NDASSCSI_ASSERT( NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_NONE &&
					 NdasrArbiter->NdasrRebuilder.RebuildRequested == FALSE );
	
	NDASSCSI_ASSERT( NdasrArbiter->NdasrRebuilder.RebuildLock == NULL );

	// Check rebuild thread is available 
	
	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	
	// Find set bits that is not locked by client.
	
	bitToRecovery = RtlFindSetBits( &NdasrArbiter->OosBmpHeader, 1, 0 );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("First set dirty bit: %x\n", bitToRecovery) );
	
	if (bitToRecovery == 0xFFFFFFFF || bitToRecovery >= NdasrArbiter->OosBmpBitCount) {

		NDASSCSI_ASSERT( RtlNumberOfSetBits(&NdasrArbiter->OosBmpHeader) == 0 );

		// No region to recover. Finish recovery.
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("No set bit in OOS bitmap. Finishing recovery\n") );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Clearing Out of sync node mark from role %d(node %d)\n", 
					  NdasrArbiter->OutOfSyncRoleIndex,
					  NdasrArbiter->RoleToNodeMap[NdasrArbiter->OutOfSyncRoleIndex]) );

		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Rebuilding completed\n") );

		return STATUS_SUCCESS;
	} 
	
	// Search and fire
	// Calc addr, length pair from bitmap and valid disk range.

	blockAddr = bitToRecovery * NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit; 
	blockLength = NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit;
			
	if ((blockAddr + blockLength) > NdasrArbiter->Lurn->UnitBlocks) {
			
		blockLength = (UINT32)(NdasrArbiter->Lurn->UnitBlocks - blockAddr);
				
		NDASSCSI_ASSERT( blockLength < NdasrArbiter->Lurn->NdasrInfo->BlocksPerBit );
	}

	NDASSCSI_ASSERT( (blockAddr + blockLength) <= NdasrArbiter->Lurn->UnitBlocks );

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

	NdasrArbiter->NdasrRebuilder.RebuildLock = NdasRaidArbiterAllocLock( NdasrArbiter, 
																		  NRIX_LOCK_TYPE_BLOCK_IO,
																		  NRIX_LOCK_MODE_EX,
																		  blockAddr, 
																		  blockLength ); 
					
	if (!NdasrArbiter->NdasrRebuilder.RebuildLock) {

		NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
			
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	status = NdasRaidArbiterArrangeLockRange( NdasrArbiter,
											   NdasrArbiter->NdasrRebuilder.RebuildLock, 
											   NdasrArbiter->LockRangeGranularity );

	// Add lock to acquired lock list
					
	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
					
	NdasrArbiter->NdasrRebuilder.RebuildLock->Status = NDASR_ARBITER_LOCK_STATUS_GRANTED;
				
	InsertTailList( &NdasrArbiter->AcquiredLockList, &NdasrArbiter->NdasrRebuilder.RebuildLock->ArbiterAcquiredLink );
	InterlockedIncrement( &NdasrArbiter->AcquiredLockCount );
					
	// DebugTrace(NDASSCSI_DBG_LURN_NDASR_INFO, ("Arbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount));	
				
	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
					
	// Update LWR before doing IO.
					
	NdasRaidArbiterUpdateLwrBitmapBit( NdasrArbiter, NdasrArbiter->NdasrRebuilder.RebuildLock, NULL );
					
	status = NdasRaidArbiterUpdateOnDiskOosBitmap( NdasrArbiter, FALSE );

	if (status != STATUS_SUCCESS) {
					
		RemoveEntryList( &NdasrArbiter->NdasrRebuilder.RebuildLock->ArbiterAcquiredLink );
		InitializeListHead( &NdasrArbiter->NdasrRebuilder.RebuildLock->ArbiterAcquiredLink );
		InterlockedDecrement( &NdasrArbiter->AcquiredLockCount );

		NdasRaidArbiterFreeLock( NdasrArbiter, NdasrArbiter->NdasrRebuilder.RebuildLock );

		NdasrArbiter->NdasrRebuilder.RebuildLock = NULL;

		NdasrArbiter->NdasrRebuilder.RebuildRequested = FALSE;
		NdasrArbiter->NdasrRebuilder.CancelRequested = FALSE;

		NdasrArbiter->NdasrRebuilder.Status = NDASR_REBUILD_STATUS_NONE;

		RELEASE_SPIN_LOCK(&NdasrArbiter->SpinLock, oldIrql);

		NdasRaidArbiterUpdateLwrBitmapBit( NdasrArbiter, NULL, NULL );

		return status;
	}

	status = NdasRaidRebuildRequest( &NdasrArbiter->NdasrRebuilder, blockAddr, blockLength );

	NDASSCSI_ASSERT( status == STATUS_SUCCESS );

	return status;
}

NTSTATUS
NdasRaidRebuildAcknowledge (
	PNDASR_ARBITER NdasrArbiter
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;


	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	
	if (NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_SUCCESS) {
	
		status = STATUS_SUCCESS;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Rebuilding range %I64x:%x is done\n", 
					 NdasrArbiter->NdasrRebuilder.BlockAddress, NdasrArbiter->NdasrRebuilder.BlockLength) );
		
	} else if (NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_FAILED) {

		status = NdasrArbiter->NdasrRebuilder.RebuildStatus;

		NDASSCSI_ASSERT( !NT_SUCCESS(status) );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Rebuilding range %I64x:%x is failed\n", 
					 NdasrArbiter->NdasrRebuilder.BlockAddress, NdasrArbiter->NdasrRebuilder.BlockLength) );

	} else if (NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_CANCELLED) {
		
		status = NdasrArbiter->NdasrRebuilder.RebuildStatus;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("Rebuilding range %I64x:%x is canceled\n", 
					 NdasrArbiter->NdasrRebuilder.BlockAddress, NdasrArbiter->NdasrRebuilder.BlockLength) );
	
	} else {

		NDASSCSI_ASSERT( FALSE );
		RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );

		return STATUS_SUCCESS;
	}

	if (NdasrArbiter->NdasrRebuilder.Status == NDASR_REBUILD_STATUS_SUCCESS) {
			
		NdasRaidArbiterChangeOosBitmapBit( NdasrArbiter, 
											FALSE, 
											NdasrArbiter->NdasrRebuilder.BlockAddress, 
											NdasrArbiter->NdasrRebuilder.BlockLength );
	}

	// Remove from acquired lock list.

	RemoveEntryList( &NdasrArbiter->NdasrRebuilder.RebuildLock->ArbiterAcquiredLink );
	InitializeListHead( &NdasrArbiter->NdasrRebuilder.RebuildLock->ArbiterAcquiredLink );
	InterlockedDecrement( &NdasrArbiter->AcquiredLockCount );

	NdasRaidArbiterFreeLock( NdasrArbiter, NdasrArbiter->NdasrRebuilder.RebuildLock );

	NdasrArbiter->NdasrRebuilder.RebuildLock = NULL;

	NdasrArbiter->NdasrRebuilder.RebuildRequested = FALSE;
	NdasrArbiter->NdasrRebuilder.CancelRequested = FALSE;
	NdasrArbiter->NdasrRebuilder.RebuildStatus = STATUS_SUCCESS;

	NdasrArbiter->NdasrRebuilder.Status = NDASR_REBUILD_STATUS_NONE;

	RELEASE_SPIN_LOCK(&NdasrArbiter->SpinLock, oldIrql);

	NdasRaidArbiterUpdateLwrBitmapBit( NdasrArbiter, NULL, NULL );
	
	if (status == STATUS_SUCCESS) {

		status = NdasRaidArbiterUpdateOnDiskOosBitmap( NdasrArbiter, FALSE );
	}

	return status;
}

NTSTATUS
NdasRaidRebuildStart (
	PNDASR_ARBITER NdasrArbiter
	)
{
	NTSTATUS			status;

	PNDASR_REBUILDER	ndasrRebuilder = &NdasrArbiter->NdasrRebuilder;
	OBJECT_ATTRIBUTES	objectAttributes;
	UCHAR				i;


	RtlZeroMemory( ndasrRebuilder, sizeof(NDASR_REBUILDER) );
	ndasrRebuilder->NdasrArbiter = NdasrArbiter;

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

		for (i=0; i<NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; i++) {
		
			ndasrRebuilder->RebuildBuffer[i] = ExAllocatePoolWithTag( NonPagedPool, 
																	  ndasrRebuilder->UnitRebuildSize, 
																	  NDASR_REBUILD_BUFFER_POOL_TAG );
		
			if (!ndasrRebuilder->RebuildBuffer[i]) {
			
				NDASSCSI_ASSERT( NDASSCSI_ASSERT_INSUFFICIENT_RESOURCES );
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

			NDASSCSI_ASSERT( FALSE );
			break;
		}

		status = ObReferenceObjectByHandle( ndasrRebuilder->ThreadHandle,
											GENERIC_ALL,
											NULL,
											KernelMode,
											&ndasrRebuilder->ThreadObject,
											NULL );

		if (status != STATUS_SUCCESS) {

			NDASSCSI_ASSERT( FALSE );
			break;
		}

		status = KeWaitForSingleObject( &ndasrRebuilder->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {
	
			NDASSCSI_ASSERT( FALSE );
			break;
		}

	} while (0);

	if (status == STATUS_SUCCESS) {

		return status;
	}

	for (i=0; i<NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; i++) {
	
		if (ndasrRebuilder->RebuildBuffer[i]) {
		
			ExFreePoolWithTag( ndasrRebuilder->RebuildBuffer[i], NDASR_REBUILD_BUFFER_POOL_TAG );
		}
	}

	return status;
}

NTSTATUS
NdasRaidRebuildStop (
	PNDASR_ARBITER NdasrArbiter
	)
{
	NTSTATUS			status;
	
	PNDASR_REBUILDER	ndasrRebuilder = &NdasrArbiter->NdasrRebuilder;
	KIRQL				oldIrql;
	UCHAR				i;
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Stopping Rebuild thread\n") );
	
	ACQUIRE_SPIN_LOCK( &NdasrArbiter->SpinLock, &oldIrql );
	
	ndasrRebuilder->ExitRequested = TRUE;
	KeSetEvent( &ndasrRebuilder->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &NdasrArbiter->SpinLock, oldIrql );
	
	NDASSCSI_ASSERT( ndasrRebuilder->ThreadObject );

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

	for (i=0; i<NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; i++) {
	
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

	ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, &oldIrql );

	NDASSCSI_ASSERT( NdasrRebuilder->Status == NDASR_REBUILD_STATUS_NONE || NdasrRebuilder->RebuildRequested );
	
	NdasrRebuilder->BlockAddress		= BlockAddr;
	NdasrRebuilder->BlockLength			= BlockLength;
	NdasrRebuilder->RebuildRequested	= TRUE;

	KeSetEvent( &NdasrRebuilder->ThreadEvent, IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}

#define NDASR_REBUILD_REST_INTERVAL 2048				// number of IO blocks wait. Wait every 1Mbyte
#define NDASR_REBUILD_REST_TIME	((HZ/1000)*10)			// 10 msec

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

		ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, &oldIrql );
		
		if (NdasrRebuilder->ExitRequested) {
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Rebuild exit requested\n") );

			RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );			
			break;
		}

		if (NdasrRebuilder->RebuildRequested) {	
			
			UINT64 startBlockAddr;
			UINT32 blockLength;
			UINT32 restInterval;
			
			NDASSCSI_ASSERT( NdasrRebuilder->BlockLength );
			NDASSCSI_ASSERT( NdasrRebuilder->Status == NDASR_REBUILD_STATUS_NONE );
			
			// Rebuild request should be called only when previous result is cleared.
			
			NdasrRebuilder->Status = NDASR_REBUILD_STATUS_WORKING;
			
			RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );
			
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Starting rebuild IO from %I64x to %x\n", NdasrRebuilder->BlockAddress,NdasrRebuilder->BlockLength) );
	
			if (NdasrRebuilder->BlockLength == 0) {

				NDASSCSI_ASSERT( FALSE );

				NdasrRebuilder->Status = NDASR_REBUILD_STATUS_SUCCESS;
				KeSetEvent( &NdasrRebuilder->NdasrArbiter->ThreadEvent, IO_NO_INCREMENT, FALSE );

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
			
			ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, &oldIrql );

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

			KeSetEvent( &NdasrRebuilder->NdasrArbiter->ThreadEvent, IO_NO_INCREMENT, FALSE );
		}

		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );

	} while (TRUE);

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exiting\n") );

	NDASSCSI_ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );
	
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

	PNDASR_INFO			ndasrInfo = NdasrRebuilder->NdasrArbiter->Lurn->NdasrInfo;

	KIRQL				oldIrql;
	PLURELATION_NODE	lurn;
	UCHAR				ridx;
	UCHAR				outOfSyncRoleIndex;
	PLURNEXT_IDE_DEVICE ideDisk;

	BOOLEAN				lockAquired[MAX_NDASR_MEMBER_DISK] = {FALSE};


	// Check current RAID status is suitable.
	
	ACQUIRE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, &oldIrql );

	// Check cancel or terminate request is sent
	
	if (NdasrRebuilder->CancelRequested || NdasrRebuilder->ExitRequested) {
	
		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );
		
		NDASSCSI_ASSERT( FALSE );

		DebugTrace( DBG_LURN_INFO, ("Rebuild IO is cancelled or exited.\n") );		
		
		return STATUS_UNSUCCESSFUL;
	}

	if (NdasrRebuilder->NdasrArbiter->NdasrState != NRIX_RAID_STATE_OUT_OF_SYNC) {

		NDASSCSI_ASSERT( FALSE );

		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );
		
		return STATUS_UNSUCCESSFUL;
	}

	if (NdasrRebuilder->NdasrArbiter->OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE) {

		NDASSCSI_ASSERT( FALSE );

		RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );
		
		return STATUS_UNSUCCESSFUL;
	}
		
	lurn = NdasrRebuilder->NdasrArbiter->Lurn;
	outOfSyncRoleIndex = NdasrRebuilder->NdasrArbiter->OutOfSyncRoleIndex;

	RELEASE_SPIN_LOCK( &NdasrRebuilder->NdasrArbiter->SpinLock, oldIrql );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &lurn->NdasrInfo->BufLockResource, TRUE );

	do {

		LONG	k;

		ridx = 0;

		while (ridx < NdasrRebuilder->NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount) { 

			UCHAR				ccbStatus;

			status = NdasRaidLurnLockSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[ridx]],
												   LURNDEVLOCK_ID_BUFFLOCK,
												   NDSCLOCK_OPCODE_ACQUIRE,
												   &ccbStatus );
		

			if (status == STATUS_LOCK_NOT_GRANTED) {

				if (ccbStatus == CCB_STATUS_LOST_LOCK) {

					status = NdasRaidLurnLockSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[ridx]],
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
				break;
			}
			
			lockAquired[ridx] = TRUE;

			ideDisk = (PLURNEXT_IDE_DEVICE)NdasrRebuilder->NdasrArbiter->Lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[ridx]]->LurnExtension;
#if !__NDAS_SCSI_LOCK_BUG_AVOID__
			NDASSCSI_ASSERT( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == TRUE );
#endif
			ridx++;
		}

		if (status != STATUS_SUCCESS) {

			break;
		}

		for (ridx = 0; ridx < NdasrRebuilder->NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

			PCHAR	tempBuffer;

			if (ridx == outOfSyncRoleIndex) {

				continue;
			}

			status = NdasRaidLurnExecuteSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[ridx]], 
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
						   NdasrRebuilder->NdasrArbiter->Lurn->ChildBlockBytes * BlockLength );

			nextChild ++;

			for (ridx = nextChild; ridx < ndasrInfo->ActiveDiskCount; ridx++) {

				if (ridx == outOfSyncRoleIndex) {

					continue;
				}

				for (k = 0; k < NdasrRebuilder->NdasrArbiter->Lurn->ChildBlockBytes * BlockLength; k ++) {

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

		status = NdasRaidLurnExecuteSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[outOfSyncRoleIndex]], 
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

	for (ridx = 0; ridx < NdasrRebuilder->NdasrArbiter->Lurn->NdasrInfo->ActiveDiskCount; ridx++) { 

		NTSTATUS status2;

		if (lockAquired[ridx] == TRUE) {

			status2 = NdasRaidLurnLockSynchrously( lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[ridx]],
												    LURNDEVLOCK_ID_BUFFLOCK,
												    NDSCLOCK_OPCODE_RELEASE,
													NULL );
	
			NDASSCSI_ASSERT( status2 == STATUS_SUCCESS || status2 == STATUS_CLUSTER_NODE_UNREACHABLE );
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)NdasrRebuilder->NdasrArbiter->Lurn->LurnChildren[NdasrRebuilder->NdasrArbiter->RoleToNodeMap[ridx]]->LurnExtension;
#if !__NDAS_SCSI_LOCK_BUG_AVOID__
		NDASSCSI_ASSERT( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == FALSE );
#endif
	}

	ExReleaseResourceLite( &lurn->NdasrInfo->BufLockResource );
	KeLeaveCriticalRegion();

	NdasrRebuilder->RebuildStatus = status;

	return status;
}
