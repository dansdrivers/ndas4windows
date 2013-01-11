#include "ndasscsiproc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasdraidarbiter"


VOID
DraidArbiterThreadProc (
	IN	PVOID	Context
	);

VOID
DraidRebuidIoThreadProc (
	IN	PVOID	Context
	);


#define DRAID_REBUILD_REST_INTERVAL 2048				// number of IO blocks wait. Wait every 1Mbyte
#define DRAID_REBUILD_REST_TIME	((HZ/1000)*10)			// 10 msec

//
// Handle rebuild IO request.
//	Wait for arbiter send rebuild request. Stop rebuild when arbiter request cancel 
//	Signal arbiter when rebuild is completed or cancelled.
//
//	to do: limit rebuild speed.
//


NTSTATUS
DraidRebuildIoStart (
	PDRAID_ARBITER_INFO Arbiter
	)
{
	NTSTATUS			status;
	PDRAID_REBUILD_INFO rebuildInfo = &Arbiter->RebuildInfo;
	OBJECT_ATTRIBUTES	objectAttributes;
	UINT32				i;


	KeInitializeEvent( &rebuildInfo->ThreadEvent, NotificationEvent, FALSE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	rebuildInfo->Status = DRAID_REBUILD_STATUS_NONE;

	rebuildInfo->RebuildLock = NULL;
	rebuildInfo->ExitRequested = FALSE;
	rebuildInfo->CancelRequested = FALSE;
	rebuildInfo->UnitRebuildSize = DRAID_REBUILD_BUFFER_SIZE;

	for (i=0;i<Arbiter->ActiveDiskCount-1;i++) {
		
		rebuildInfo->RebuildBuffer[i] = ExAllocatePoolWithTag( NonPagedPool, 
															   rebuildInfo->UnitRebuildSize, 
															   DRAID_REBUILD_BUFFER_POOL_TAG );
		
		if (!rebuildInfo->RebuildBuffer[i]) {
			
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto errout;
		}
	}
	
	status = PsCreateSystemThread( &rebuildInfo->ThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   DraidRebuidIoThreadProc,
								   Arbiter );

	if (!NT_SUCCESS(status)) {

		DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL : DraidRebuildIoStart !!!\n"));
		goto errout;
	}

	status = ObReferenceObjectByHandle( rebuildInfo->ThreadHandle,
										GENERIC_ALL,
										NULL,
										KernelMode,
										&rebuildInfo->ThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		goto errout;
	}

	return STATUS_SUCCESS;

errout:

	for (i=0;i<Arbiter->ActiveDiskCount-1;i++) {
	
		if (rebuildInfo->RebuildBuffer[i]) {
		
			ExFreePoolWithTag(rebuildInfo->RebuildBuffer[i], DRAID_REBUILD_BUFFER_POOL_TAG);
		}
	}

	return status;
}

NTSTATUS
DraidRebuildIoStop (
	PDRAID_ARBITER_INFO Arbiter
	)
{
	NTSTATUS			status;
	PDRAID_REBUILD_INFO rebuildInfo = &Arbiter->RebuildInfo;
	KIRQL				oldIrql;
	UINT32				i;
	
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Stopping Rebuild thread\n") );
	
	ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
	
	rebuildInfo->ExitRequested = TRUE;
	KeSetEvent( &rebuildInfo->ThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );
	
	if (rebuildInfo->ThreadObject) {
	
		DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Wait for rebuild IO thread completion\n"));
		status = KeWaitForSingleObject( rebuildInfo->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );
		
		//	Dereference the thread object.
		
		ObDereferenceObject( rebuildInfo->ThreadObject );

		ZwClose( rebuildInfo->ThreadHandle );
	}

	for (i=0;i<Arbiter->ActiveDiskCount-1;i++) {
	
		if (rebuildInfo->RebuildBuffer[i]) {

			ExFreePoolWithTag( rebuildInfo->RebuildBuffer[i], DRAID_REBUILD_BUFFER_POOL_TAG );
		}
	}

	return STATUS_SUCCESS;
}


VOID
DraidRebuidIoThreadProc (
	IN PDRAID_ARBITER_INFO Arbiter
	)
{
	PDRAID_REBUILD_INFO rebuildInfo = &Arbiter->RebuildInfo;
	NTSTATUS			status;
	KIRQL				oldIrql;
	BOOLEAN				doMore;


	// Set lower priority.
	
	KeSetBasePriorityThread( KeGetCurrentThread(), -1 );
	
	doMore = TRUE;

	while (TRUE) {
	
		if (doMore) {

			doMore = FALSE;

		} else {

			DebugTrace( DBG_LURN_TRACE, ("Waiting rebuild request\n") );
		
			status = KeWaitForSingleObject( &rebuildInfo->ThreadEvent,
											Executive,
											KernelMode,
											FALSE,
											NULL );

			KeClearEvent( &rebuildInfo->ThreadEvent );
		}

		ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
		
		if (rebuildInfo->ExitRequested) {
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Rebuild exit requested\n") );

			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);			
			break;
		}

		if (rebuildInfo->RebuildRequested) {	
			
			UINT64 CurAddr, EndAddr;
			UINT32 IoLength;
			UINT32 RestInterval;
			
			ASSERT( rebuildInfo->Length );
			
			// Rebuild request should be called only when previous result is cleared.
			
			ASSERT( rebuildInfo->Status == DRAID_REBUILD_STATUS_NONE );
			
			rebuildInfo->Status = DRAID_REBUILD_STATUS_WORKING;
			
			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );
			
			doMore = TRUE;

			CurAddr =  rebuildInfo->Addr;
			EndAddr = rebuildInfo->Addr + rebuildInfo->Length;
			
			DebugTrace( DBG_LURN_TRACE, ("Starting rebuild IO from %I64x to %I64x\n", CurAddr, EndAddr-1) );
			
			RestInterval = 0;
			
			while (TRUE) {
			
				if (EndAddr - CurAddr > rebuildInfo->UnitRebuildSize/SECTOR_SIZE) {

					IoLength = rebuildInfo->UnitRebuildSize/SECTOR_SIZE;

				} else {

					IoLength = (UINT32)(EndAddr - CurAddr);
				}

				if (Arbiter->RebuildInfo.AggressiveRebuildMode) {

					// In aggressive mode, take lock on demand and aggressively.
					status = DraidRebuildIoLockRangeAggressive( Arbiter, CurAddr, IoLength );

					if (!NT_SUCCESS(status)) {

						break;
					}
				}

				status = DraidDoRebuildIo( Arbiter, CurAddr, IoLength );

				if (!NT_SUCCESS(status)) {

					break;
				}

				CurAddr +=IoLength;

				if (CurAddr >= EndAddr) {

					status = STATUS_SUCCESS;
					break;
				}

				RestInterval += IoLength;

				if (RestInterval > DRAID_REBUILD_REST_INTERVAL) {

					LARGE_INTEGER RestTime;

					RestTime.QuadPart = -DRAID_REBUILD_REST_TIME;
					KeDelayExecutionThread(KernelMode, FALSE, &RestTime);
					RestInterval = 0;
				}
					
				ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);

				if (rebuildInfo->ExitRequested) {
				
					RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
					status = STATUS_UNSUCCESSFUL;
					break;
				}

				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			}
			
			ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);

			rebuildInfo->RebuildRequested = FALSE;

			if (rebuildInfo->CancelRequested) {

				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Rebuilding range %I64x:%x cancelled\n", rebuildInfo->Addr, rebuildInfo->Length));
				rebuildInfo->Status = DRAID_REBUILD_STATUS_CANCELLED;

			} else if (rebuildInfo->ExitRequested) {

				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Exit requested. Rebuilding range %I64x:%x cancelled\n", rebuildInfo->Addr, rebuildInfo->Length));
				
				rebuildInfo->Status = DRAID_REBUILD_STATUS_CANCELLED;
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);	
				
				break;

			} else if (NT_SUCCESS(status)) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Rebuilding range %I64x:%x done\n", rebuildInfo->Addr, rebuildInfo->Length) );
				
				rebuildInfo->Status = DRAID_REBUILD_STATUS_DONE;

			} else {
			
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Rebuilding range %I64x:%x failed\n", rebuildInfo->Addr, rebuildInfo->Length) );
				
				rebuildInfo->Status = DRAID_REBUILD_STATUS_FAILED;
			}

			// Notify arbiter.

			KeSetEvent( &Arbiter->ArbiterThreadEvent, IO_NO_INCREMENT, FALSE );
			
			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

		} else {

			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );
		}
	}

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Exiting\n") );

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);
	
	PsTerminateSystemThread(STATUS_SUCCESS);
}


NTSTATUS 
DraidArbiterStart (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status;
	PRAID_INFO			raidInfo = Lurn->LurnRAIDInfo;
	PDRAID_ARBITER_INFO arbiter;
	KIRQL				oldIrql, oldIrql2;
	UINT32				i;
	UCHAR				flags;
	OBJECT_ATTRIBUTES	objectAttributes;
	UINT32				upToDateNode;

	if (raidInfo->pDraidArbiter) {
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Aribter is already running\n") );
		return STATUS_SUCCESS;
	}
	
	raidInfo->pDraidArbiter = ExAllocatePoolWithTag( NonPagedPool, sizeof(DRAID_ARBITER_INFO), DRAID_ARBITER_INFO_POOL_TAG );

	if (NULL == raidInfo->pDraidArbiter) {
		
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	arbiter = raidInfo->pDraidArbiter;
	RtlZeroMemory( arbiter, sizeof(DRAID_ARBITER_INFO) );

	KeInitializeSpinLock( &arbiter->SpinLock );

	arbiter->Lurn = Lurn;
	arbiter->Status = DRAID_ARBITER_STATUS_INITALIZING;

	arbiter->RaidStatus = DRIX_RAID_STATUS_INITIALIZING;

	InitializeListHead( &arbiter->ClientList );
	InitializeListHead( &arbiter->AcquiredLockList );
	InitializeListHead( &arbiter->ToYieldLockList );
	InitializeListHead( &arbiter->PendingLockList );
	InitializeListHead( &arbiter->AllArbiterList );
	

	arbiter->AcquiredLockCount = 0;
	arbiter->TotalDiskCount = Lurn->LurnChildrenCnt;
	arbiter->ActiveDiskCount = Lurn->LurnChildrenCnt - raidInfo->nSpareDisk;

	arbiter->LockRangeGranularity = raidInfo->SectorsPerBit; // Set to sector per bit for time being..

	arbiter->SectorsPerOosBmpBit = raidInfo->SectorsPerBit;

	arbiter->OutOfSyncRole = NO_OUT_OF_SYNC_ROLE;

	RtlCopyMemory( &arbiter->RaidSetId, &raidInfo->RaidSetId, sizeof(GUID) );
	RtlCopyMemory( &arbiter->ConfigSetId, &raidInfo->ConfigSetId, sizeof(GUID) );	
		
	//
	// Initialize arbiter node.
	//

	//
	// 1. Initialize Lurn children with this node's value. (Lurn children is already initialized.)
	//
	// from LurnRAIDInitialize, LurnRAIDThreadProcRecover
	// to do: check whether disk is hot-swapped - disk size has changed(smaller size is not acceptable)/different RAID set ID or no RMD
	//		If hot-swapped disk is small or has different RAID set ID, mark that lurn as defective
	//

	// Read RMD from all available nodes and analyze. them
	
	status = LurnRMDRead( Lurn,&arbiter->Rmd, &upToDateNode );

	if (!NT_SUCCESS(status)) {

		DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("**** **** RAID_STATUS_FAIL **** ****\n"));
		
		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
		
		arbiter->Status = DRAID_ARBITER_STATUS_TERMINATING;
		arbiter->RaidStatus = DRIX_RAID_STATUS_FAILED;
		RELEASE_SPIN_LOCK(&arbiter->SpinLock, oldIrql);
		goto fail;
	}

	// 2. Set initial arbiter flags. 
	 
	ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );	
	
	for (i = 0; i < Lurn->LurnChildrenCnt; i++) {

		ACQUIRE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, &oldIrql2);
		
		switch(Lurn->LurnChildren[i]->LurnStatus) {
		
		case LURN_STATUS_RUNNING:
		case LURN_STATUS_STALL:
		
			flags = DRIX_NODE_FLAG_RUNNING;
			break;

		case LURN_STATUS_INIT:
		case LURN_STATUS_STOP_PENDING:
		case LURN_STATUS_STOP:
		case LURN_STATUS_DESTROYING:

			flags = DRIX_NODE_FLAG_STOP;
			break;

		default:

			ASSERT(FALSE);
			flags = DRIX_NODE_FLAG_UNKNOWN;
			break;
		}

		if (LurnGetCauseOfFault(Lurn->LurnChildren[i]) & (LURN_FCAUSE_BAD_SECTOR|LURN_FCAUSE_BAD_DISK)) {

			flags |= DRIX_NODE_FLAG_DEFECTIVE;
			arbiter->DefectCodes[i] |= ((LurnGetCauseOfFault(Lurn->LurnChildren[i]) & LURN_FCAUSE_BAD_SECTOR) ?
										DRIX_NODE_DEFECT_BAD_SECTOR : DRIX_NODE_DEFECT_BAD_DISK );
		}

		arbiter->NodeFlags[i] = flags;
		
		DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("Setting initial node %d flag: %d\n", i, arbiter->NodeFlags[i]) );

		RELEASE_SPIN_LOCK( &Lurn->LurnChildren[i]->LurnSpinLock, oldIrql2 );
	}

	RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );

	// 3. Map children based on RMD

	ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );	

	for (i = 0; i < Lurn->LurnChildrenCnt; i++)	{ // i is role index
	
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("MAPPING Lurn node %d to RAID role %d\n", arbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx, i) );

		arbiter->RoleToNodeMap[i] = (UCHAR)arbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx;
		arbiter->NodeToRoleMap[arbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx] = (UCHAR)i;	
		
		NDASSCSI_ASSERT( arbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx < Lurn->LurnChildrenCnt );
	}

	// 4. Apply node information from RMD
		
	for (i = 0; i < Lurn->LurnChildrenCnt; i++) { // i : role index. 
		
		UCHAR UnitDeviceStatus = arbiter->Rmd.UnitMetaData[i].UnitDeviceStatus;
		UCHAR NodeIndex = arbiter->RoleToNodeMap[i];
		
		if (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & UnitDeviceStatus) {

			if (i < arbiter->ActiveDiskCount) {
			
				arbiter->OutOfSyncRole = (UCHAR)i;
				DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("Node %d(role %d) is out-of-sync\n",  NodeIndex, i));
				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Setting out of sync role: %d\n", arbiter->OutOfSyncRole));
			}
		}

		if (NDAS_UNIT_META_BIND_STATUS_DEFECTIVE & UnitDeviceStatus) {

			arbiter->NodeFlags[NodeIndex] &= ~(DRIX_NODE_FLAG_UNKNOWN | DRIX_NODE_FLAG_RUNNING | DRIX_NODE_FLAG_STOP);
			arbiter->NodeFlags[NodeIndex] |= DRIX_NODE_FLAG_DEFECTIVE;
		
			// fault device found
			DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("Node %d(role %d) is defective\n",  NodeIndex, i));
			arbiter->DefectCodes[NodeIndex] |= DraidRmdUnitStatusToDefectCode(UnitDeviceStatus);
		}
	}

	RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );

	// 5. Read bitmap.
	 
	status = DradArbiterInitializeOosBitmap( arbiter, upToDateNode );

	// 6. Set initial RAID status.
	
	DraidArbiterRefreshRaidStatus( arbiter, TRUE );

	// Create Arbiter thread
	
	KeInitializeEvent( &arbiter->ArbiterThreadEvent, NotificationEvent, FALSE );
		
	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );
	
	status = PsCreateSystemThread( &arbiter->ThreadArbiterHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   DraidArbiterThreadProc,
								   Lurn );

	if (!NT_SUCCESS(status)) {

		DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL : DraidArbiterStart !!!\n"));
		status = STATUS_THREAD_NOT_IN_PROCESS;
		goto fail;
	}

	status = ObReferenceObjectByHandle( arbiter->ThreadArbiterHandle,
										GENERIC_ALL,
										NULL,
										KernelMode,
										&arbiter->ThreadArbiterObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		ExFreePoolWithTag( raidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG );
		raidInfo->pDraidArbiter = NULL;		
		DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL : DraidArbiterStart !!!\n"));
		
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	return STATUS_SUCCESS;

fail:

	DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("!!! Arbiter failed to start\n") );

	if (raidInfo->pDraidArbiter) {
		
		ExFreePoolWithTag( raidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG );
		raidInfo->pDraidArbiter = NULL;
	}

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS
DraidArbiterStop (
	PLURELATION_NODE	Lurn
	)
{
	KIRQL						oldIrql;
	PLIST_ENTRY					listEntry;
	PRAID_INFO					raidInfo = Lurn->LurnRAIDInfo;
	PDRAID_ARBITER_INFO			arbiterInfo = raidInfo->pDraidArbiter;
	NTSTATUS					status;
	PDRAID_ARBITER_LOCK_CONTEXT lock;

	if (!arbiterInfo) {

		return STATUS_SUCCESS;
	}

	ASSERT(arbiterInfo);
	
	DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Stopping DRAID arbiter\n"));

	DraidUnregisterArbiter( arbiterInfo );
	
	if (arbiterInfo->ThreadArbiterHandle) {

		ACQUIRE_SPIN_LOCK( &arbiterInfo->SpinLock, &oldIrql );

		arbiterInfo->StopRequested = TRUE;
		KeSetEvent( &arbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE ); // This will wake up Arbiter thread.
		
		RELEASE_SPIN_LOCK(&arbiterInfo->SpinLock, oldIrql);

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Wait for Arbiter thread completion\n") );

		status = KeWaitForSingleObject( arbiterInfo->ThreadArbiterObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			ASSERT(FALSE);
		
		} else {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Arbiter thread exited\n") );
		}

		//	Dereference the thread object.

		ObDereferenceObject( arbiterInfo->ThreadArbiterObject );
		ZwClose( arbiterInfo->ThreadArbiterHandle );

		ACQUIRE_SPIN_LOCK( &arbiterInfo->SpinLock, &oldIrql );

		arbiterInfo->ThreadArbiterObject = NULL;
		arbiterInfo->ThreadArbiterHandle = NULL;

		RELEASE_SPIN_LOCK( &arbiterInfo->SpinLock, oldIrql );
	}

	while (TRUE) {

		listEntry = RemoveHeadList(&arbiterInfo->AcquiredLockList);
		
		if (listEntry == &arbiterInfo->AcquiredLockList) {
		
			break;
		}
		
		lock = CONTAINING_RECORD( listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink );
		InterlockedDecrement( &arbiterInfo->AcquiredLockCount );
		RemoveEntryList(&lock->ToYieldLink);
		
		InitializeListHead( &lock->ToYieldLink ); // to check bug...
//		DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("arbiter->AcquiredLockCount %d\n",arbiterInfo->AcquiredLockCount));		
		DraidArbiterFreeLock( arbiterInfo, lock );
	}

	//ASSERT(InterlockedCompareExchange(&arbiterInfo->AcquiredLockCount, 0, 0)==0);

	if (arbiterInfo->AcquiredLockCount) {

		DbgPrint( "BUG BUG BUG arbiterInfo->AcquiredLockCount = %d\n", arbiterInfo->AcquiredLockCount );
	}

	ExFreePoolWithTag( arbiterInfo->OosBmpInCoreBuffer, DRAID_BITMAP_POOL_TAG );
	ExFreePoolWithTag( arbiterInfo->LwrBmpBuffer, DRAID_BITMAP_POOL_TAG );	
	ExFreePoolWithTag( arbiterInfo->OosBmpOnDiskBuffer, DRAID_BITMAP_POOL_TAG );
	ExFreePoolWithTag( raidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG );

	raidInfo->pDraidArbiter = NULL;

	return STATUS_SUCCESS;
}


VOID
DraidArbiterThreadProc (
	IN PLURELATION_NODE	Lurn
	)
{
	PRAID_INFO					raidInfo;
	PDRAID_ARBITER_INFO			arbiter;
	KIRQL						oldIrql;
	NTSTATUS					status;
	LARGE_INTEGER				timeOut;
	UINT32						childCount;
	UINT32						activeDiskCount;
	UINT32						spareCount;
	UINT32						sectorsPerBit;
	BOOLEAN						rmdChanged;
	PLIST_ENTRY					listEntry;
	PDRAID_CLIENT_CONTEXT		client;
	PDRAID_ARBITER_LOCK_CONTEXT lock;
	INT32 						maxEventCount =0;
	PKEVENT						*events = NULL;
	PKWAIT_BLOCK				waitBlocks = NULL;
	NTSTATUS					waitStatus;
	INT32 						eventCount;
	BOOLEAN						handled;

	BOOLEAN						arbiterTerminateThread;
	BOOLEAN						doMore; 
	BOOLEAN						relooping;


	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("DRAID Arbiter thread starting\n") );
	
	maxEventCount = DraidReallocEventArray (&events, &waitBlocks, maxEventCount );

	ASSERT( LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType );

	raidInfo = Lurn->LurnRAIDInfo;

	arbiter = (PDRAID_ARBITER_INFO) raidInfo->pDraidArbiter;

	// Set some value from svc.

	sectorsPerBit = raidInfo->SectorsPerBit;
	ASSERT( sectorsPerBit > 0 );

	childCount = Lurn->LurnChildrenCnt;
	activeDiskCount = Lurn->LurnChildrenCnt - raidInfo->nSpareDisk;
	spareCount = raidInfo->nSpareDisk;

	ASSERT( activeDiskCount == raidInfo->nDiskCount );

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("ChildCount: %d, DiskCount : %d\n", childCount, activeDiskCount) );

	DraidArbiterUpdateInCoreRmd( arbiter, TRUE );

	status = DraidArbiterWriteRmd( arbiter, &arbiter->Rmd );

	if (!NT_SUCCESS(status)) {

		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to update RMD\n") );
		goto fail;
	}

	status = DraidRebuildIoStart( arbiter );

	if (!NT_SUCCESS(status)) {
	
		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to start rebuild IO thread\n") );
		goto fail;
	}

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("DRAID Aribiter enter running status\n") );

	ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
	arbiter->Status = DRAID_ARBITER_STATUS_RUNNING;
	RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );		

	DraidRegisterArbiter( arbiter );

	arbiterTerminateThread = FALSE;
	doMore = FALSE;
	relooping = FALSE;
	
	// Wait for request from client or stop request.
	
	do {

		// Check client unregister request

		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
		
		for (listEntry = arbiter->ClientList.Flink;
			 listEntry != &arbiter->ClientList;
			 )  {

			client = CONTAINING_RECORD( listEntry, DRAID_CLIENT_CONTEXT, Link );
			listEntry = listEntry->Flink;

			if (client->UnregisterRequest) {

				if (client == arbiter->LocalClient) {
					
					arbiter->LocalClient = NULL;
				}

				RemoveEntryList( &client->Link );
				RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
				DraidArbiterTerminateClient( arbiter, client, FALSE );
				ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
				
				continue;
			}
		}		

		RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
		
		// Handle requested rebuild IO
		
		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );

		if (arbiter->RebuildInfo.Status == DRAID_REBUILD_STATUS_DONE	||
			arbiter->RebuildInfo.Status == DRAID_REBUILD_STATUS_FAILED	||
			arbiter->RebuildInfo.Status == DRAID_REBUILD_STATUS_CANCELLED) {
		
			RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
			
			DraidRebuildIoAcknowledge( arbiter );
			doMore = TRUE;

		} else {
			
			RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
		}

		
		// Send status update if not initialized
		
		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
		
		for (listEntry = arbiter->ClientList.Flink;
			 listEntry != &arbiter->ClientList;
			 )  {
			
			client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
			listEntry = listEntry->Flink;

			if (!client->Initialized) {
				
				RELEASE_SPIN_LOCK(&arbiter->SpinLock, oldIrql);

				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Notifying initial status to client\n"));
				
				status = DraidArbiterNotify( arbiter, client, DRIX_CMD_CHANGE_STATUS, 0, 0, 0 );
				
				if (status != STATUS_SUCCESS) {
				
					ACQUIRE_SPIN_LOCK(&arbiter->SpinLock, &oldIrql);					

					// Failed to notify. This client may have been removed. Re-iterate from first.

					continue;
				
				} else {
				
					ACQUIRE_SPIN_LOCK(&arbiter->SpinLock, &oldIrql);
					client->Initialized = TRUE;
					RtlCopyMemory(client->NodeFlags, arbiter->NodeFlags, sizeof(arbiter->NodeFlags));
				}

				doMore = TRUE;
			}
		}

		RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );

		DraidArbiterCheckRequestMsg( arbiter, &handled );
		
		if (handled) {

			doMore = TRUE;
		}

		// Check spare disk is usable.
		
		DraidArbiterUseSpareIfNeeded( arbiter );
		
		// RAID status may have changed
		
		if (arbiter->SyncStatus == DRAID_SYNC_REQUIRED) {			
		
			arbiter->SyncStatus = DRAID_SYNC_IN_PROGRESS;

			// Send raid status change to all client

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Sending DRIX_CMD_CHANGE_STATUS to all client\n") );

			for (listEntry = arbiter->ClientList.Flink;
				 listEntry != &arbiter->ClientList;
				 )  {
				
				client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
				listEntry = listEntry->Flink;

				if (client->Initialized == FALSE) {
				
					continue;
				}

				status = DraidArbiterNotify( arbiter, client, DRIX_CMD_CHANGE_STATUS, 1, 0, 0 );
				
				if (!NT_SUCCESS(status)) {
				
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Notify failed during change status. Restarting.\n") );

					arbiter->SyncStatus = DRAID_SYNC_REQUIRED;
				}
			}

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Sending DRIX_CMD_STATUS_SYNCED to all client\n") );				
			
			for (listEntry = arbiter->ClientList.Flink;
				 listEntry != &arbiter->ClientList;
				 )  {
				
				client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
				listEntry = listEntry->Flink;

				if (client->Initialized == FALSE) {

					continue;
				}

				status = DraidArbiterNotify( arbiter, client, DRIX_CMD_STATUS_SYNCED, 0, 0, 0 );

				if (!NT_SUCCESS(status)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Notify failed during sync. Restarting.\n") );
					arbiter->SyncStatus = DRAID_SYNC_REQUIRED;
				}				
			}

			arbiter->SyncStatus = DRAID_SYNC_DONE;
			doMore = TRUE;
		}

		//  Send yield message if other client is requested.
		
		while (listEntry = ExInterlockedRemoveHeadList(&arbiter->ToYieldLockList, &arbiter->SpinLock)) {

			lock = CONTAINING_RECORD( listEntry, DRAID_ARBITER_LOCK_CONTEXT, ToYieldLink );
			
			InitializeListHead( &lock->ToYieldLink );
			
			if (lock->Owner) {

				status = DraidArbiterNotify( arbiter, lock->Owner, DRIX_CMD_REQ_TO_YIELD_LOCK, 0, lock->LockId, 0 );

				if (!NT_SUCCESS(status)) {
				
					// ClientAcquired may have destroyed and ToYieldLockList may have been changed. restart

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Notify failed during yield lock. Restarting.\n") );
					
					relooping = TRUE;
					break;
				}
			}
		}

		if (relooping) {

			relooping = FALSE;
			continue;
		}

		// Send grant message if any pending lock exist.
		
		status = DraidArbiterGrantLockIfPossible( arbiter );
		
		if (!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( FALSE );
			break;
		}
		
		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );

		if (arbiter->RaidStatus == DRIX_RAID_STATUS_REBUILDING) {

			RELEASE_SPIN_LOCK(&arbiter->SpinLock, oldIrql);
			DraidRebuilldIoInitiate(arbiter);

		} else {

			RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
		}

		// Check termination

		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );

		if (arbiter->StopRequested) {

			arbiter->Status = DRAID_ARBITER_STATUS_TERMINATING;
			arbiter->StopRequested = FALSE;

			RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
			
			DraidRebuildIoStop( arbiter );
			DraidRebuildIoAcknowledge( arbiter );
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("DRAID Aribter stop requested.Sending RETIRE message to all client..\n") );	
			
			while (listEntry = ExInterlockedRemoveHeadList(&arbiter->ClientList, &arbiter->SpinLock)) {
				
				client = CONTAINING_RECORD(listEntry, DRAID_CLIENT_CONTEXT, Link);
				status = DraidArbiterNotify( arbiter, client, DRIX_CMD_RETIRE, 0, 0, 0 ); // Client list may has been changed.
				
				// Continue to next client even if notification has failed.
			}

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Sent RETIRE to all client. Exiting arbiter loop.\n") );
			
			break; // Exit arbiter loop.

		} else {

			RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
		}

		rmdChanged = DraidArbiterUpdateInCoreRmd( arbiter, FALSE );

		if (rmdChanged) {
		
			DraidArbiterWriteRmd( arbiter,  &arbiter->Rmd );
		}
	
		// Recheck any pending request without waiting 

		if (doMore) {
	
			doMore = FALSE;
			continue;
		}

		// To do: Broadcast Arbiter node presence time to time..

		eventCount = 0;
		events[eventCount] = &arbiter->ArbiterThreadEvent;
		eventCount++;

		if (arbiter->LocalClient) {
			
			events[eventCount] = &arbiter->LocalClient->RequestChannel.Event;
			eventCount++;
		}

		ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );

		for (listEntry = arbiter->ClientList.Flink;
			listEntry != &arbiter->ClientList;
			)  {
				
			client = CONTAINING_RECORD( listEntry, DRAID_CLIENT_CONTEXT, Link );

			listEntry = listEntry->Flink;

			if (client->RequestConnection) {
					
				if (maxEventCount < eventCount+1) {
						
					RELEASE_SPIN_LOCK(&arbiter->SpinLock, oldIrql);
					
					maxEventCount = DraidReallocEventArray(&events, &waitBlocks, maxEventCount);

					if (events == NULL || waitBlocks == NULL) {

						DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Insufficient memory\n") );

						ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );

						break;
					}

					ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
				}

				if (!client->RequestConnection->Receiving) {
					
					// Receive from request connection if not receiving.
					RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
					
					NDASSCSI_ASSERT( client->RequestConnection->TdiReceiveContext.Result >= 0 );
						
					client->RequestConnection->TdiReceiveContext.Irp = NULL;
					client->RequestConnection->TdiReceiveContext.Result = 0;
							
					KeInitializeEvent(&client->RequestConnection->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
							
					if (client->RequestConnection->ConnectionFileObject) {
								
						status = LpxTdiRecvWithCompletionEvent( client->RequestConnection->ConnectionFileObject,
																&client->RequestConnection->TdiReceiveContext,
																client->RequestConnection->ReceiveBuf,
																sizeof(DRIX_HEADER),
																0, 
																NULL, 
																NULL );
						
					} else {

						status = STATUS_UNSUCCESSFUL;
					}

					if (!NT_SUCCESS(status)) {
						
						DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Failed to start to recv from client. Terminating this client\n"));
							
						ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
							
						RemoveEntryList( &client->Link );

						RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );

						DraidArbiterTerminateClient(arbiter, client, FALSE);
							
						ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
							
						continue;
					}

					ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );
					client->RequestConnection->Receiving = TRUE;
				}

				events[eventCount] = &client->RequestConnection->TdiReceiveContext.CompletionEvent;
				eventCount++;
			}

			// to do: Add event for reply to notification
		}

		RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );
			
		if (events == NULL || waitBlocks == NULL) {

			break;
		}

		timeOut.QuadPart = -NANO100_PER_SEC * 30; // need to wake-up to handle dirty bitmap
			
		waitStatus = KeWaitForMultipleObjects( eventCount, 
											   events, 
											   WaitAny, 
											   Executive, 
											   KernelMode, 
											   TRUE,	
											   &timeOut, 
											   waitBlocks );
			
		KeClearEvent( &arbiter->ArbiterThreadEvent );

	} while (arbiterTerminateThread == FALSE);


	while (listEntry = ExInterlockedRemoveHeadList(&arbiter->ClientList, &arbiter->SpinLock)) {

		client = CONTAINING_RECORD(listEntry, DRAID_CLIENT_CONTEXT, Link);
		DraidArbiterTerminateClient( arbiter, client, FALSE );
	}

	ASSERT( DRAID_ARBITER_STATUS_TERMINATING == arbiter->Status );
	
	DraidArbiterUpdateInCoreRmd( arbiter, FALSE );
	status = DraidArbiterWriteRmd( arbiter, &arbiter->Rmd );

fail:

	ACQUIRE_SPIN_LOCK( &arbiter->SpinLock, &oldIrql );

	arbiter->Status = DRAID_ARBITER_STATUS_TERMINATING;
	arbiter->RaidStatus = DRIX_RAID_STATUS_FAILED;
	
	RELEASE_SPIN_LOCK( &arbiter->SpinLock, oldIrql );

	DraidFreeEventArray( events, waitBlocks );
	
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Exiting\n") );

	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}

//
// Refresh RAID status and Node status from node status reported by clients.
// If ForceChange is TRUE, Test RAID status change when node is not changed. This happens when NodeFlags is manually updated.
// Return TRUE if any RAID status has changed.
//

BOOLEAN
DraidArbiterRefreshRaidStatus (
	PDRAID_ARBITER_INFO	Arbiter,
	BOOLEAN				ForceChange
	)
{
	UINT32					i;
	KIRQL					oldIrql;
	BOOLEAN					changed = FALSE;
	PLIST_ENTRY				listEntry;
	PDRAID_CLIENT_CONTEXT	client;
	UINT32					newRaidStatus;
	UCHAR					newNodeFlags[MAX_DRAID_MEMBER_DISK] = { 0 };
	BOOLEAN					bmpChanged = FALSE;
	NTSTATUS				status;	


	// Merge report from each client.
	
	ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );

	//	Gather each client's node information
	
	for (i=0;i<Arbiter->TotalDiskCount;i++) {
		
		if (IsListEmpty(&Arbiter->ClientList)) {
		
			newNodeFlags[i] = Arbiter->NodeFlags[i]; // Keep previous flag if no client exists.
			continue;
		}

		newNodeFlags[i] = 0;

		for (listEntry = Arbiter->ClientList.Flink;
			 listEntry != &Arbiter->ClientList;
			 listEntry = listEntry->Flink) {

			client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
			
			if (client->Initialized == FALSE) {

				continue;
			}
			
			// If already defective node, add up any new defective code and keep defective flag
			
			if (Arbiter->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE) {
			
				newNodeFlags[i] |= DRIX_NODE_FLAG_DEFECTIVE;

				if (client->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Node %d keep defective flag. Defect code %x -> %x\n",
								 i, Arbiter->DefectCodes[i], client->DefectCode[i]) );

					newNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN | DRIX_NODE_FLAG_RUNNING | DRIX_NODE_FLAG_STOP);
					
					if (client->DefectCode[i] != 0) {
					
						Arbiter->DefectCodes[i] |= client->DefectCode[i];
					}

				} else {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Node %d keep defective code %x\n", i, Arbiter->DefectCodes[i]) );
				}

				continue;
			}

			// Flag priority:  DRIX_NODE_FLAG_DEFECTIVE > DRIX_NODE_FLAG_STOP > DRIX_NODE_FLAG_RUNNING > DRIX_NODE_FLAG_UNKNOWN
			
			if (client->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE) {
			
				newNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING|DRIX_NODE_FLAG_STOP);
				newNodeFlags[i] |= DRIX_NODE_FLAG_DEFECTIVE;

				if (client->DefectCode[i] != 0) {
				
					Arbiter->DefectCodes[i] |= client->DefectCode[i];
				}

			} else if ((client->NodeFlags[i] & DRIX_NODE_FLAG_STOP) && !(newNodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE)) {
				
				newNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN | DRIX_NODE_FLAG_RUNNING);	
				newNodeFlags[i] |= DRIX_NODE_FLAG_STOP;
			
			} else if ((client->NodeFlags[i] & DRIX_NODE_FLAG_RUNNING) && 
					   !(newNodeFlags[i] & (DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_STOP))) {
				
				newNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN);	
				newNodeFlags[i] |= DRIX_NODE_FLAG_RUNNING;
			}

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Node %d: Old flag %x, Client Flag %x, New flag %x\n", 
						 i, Arbiter->NodeFlags[i], client->NodeFlags[i], newNodeFlags[i]) );
		}
		
		if (newNodeFlags[i] == 0) {
		
			// No client is initialized yet. Just keep old flag.
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("No client is initialized. Keep node %d flags %x\n", i, Arbiter->NodeFlags[i]) );

			newNodeFlags[i] = Arbiter->NodeFlags[i];
		}
	}

	if (ForceChange) {
		
		changed = TRUE;
	
	} else {
	 
		// Check any node information has changed
	
		for (i = 0; i < Arbiter->TotalDiskCount; i++) {
			
			if (newNodeFlags[i] != Arbiter->NodeFlags[i]) {
			
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Changing Node %d flags from %02x to %02x\n", i, Arbiter->NodeFlags[i], newNodeFlags[i]) );

				Arbiter->NodeFlags[i] = newNodeFlags[i];
				changed = TRUE;
			}
		}
	}
	
	// Test new RAID status only when needed, i.e: node has changed or first time.
	
	if (changed) {
	
		BOOLEAN newFaultyNode = FALSE;
		UINT32	faultCount = 0;
		UCHAR	faultRole = 0;

		for (i = 0; i < Arbiter->ActiveDiskCount; i++) { // i : role index
			
			if (!(newNodeFlags[Arbiter->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING)) {
			
				faultCount ++;

				if (Arbiter->OutOfSyncRole != i) {

					// Faulty disk is not marked as out-of-sync.

					newFaultyNode = TRUE;
					faultRole = (UCHAR)i;
				}
			}
		}

		if (faultCount == 0) {

			if (Arbiter->OutOfSyncRole == NO_OUT_OF_SYNC_ROLE) {

				newRaidStatus = DRIX_RAID_STATUS_NORMAL;
		
			} else {

				newRaidStatus = DRIX_RAID_STATUS_REBUILDING;
			}

		} else if (faultCount == 1) {

			ASSERT( FALSE );

			if (Arbiter->OutOfSyncRole != NO_OUT_OF_SYNC_ROLE) {

				// There was out-of-sync node and..

				if (newFaultyNode) { 

					// New faulty node appeared.
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Role %d(node %d) also failed. RAID failure\n", faultRole, Arbiter->RoleToNodeMap[faultRole]) );

					newRaidStatus = DRIX_RAID_STATUS_FAILED;
				
				} else {	
				
					// OutOfSync node is still faulty.
					
					newRaidStatus = DRIX_RAID_STATUS_DEGRADED;
				}

			} else {

				// Faulty node appeared from zero fault.
				Arbiter->OutOfSyncRole = faultRole;

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Setting out of sync role: %d\n", faultRole) );
				
				newRaidStatus = DRIX_RAID_STATUS_DEGRADED;
			}

		} else { // FaultCount > 1
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("More than 1 active node is at fault. RAID failure\n") );

			newRaidStatus = DRIX_RAID_STATUS_FAILED;
		} 

	} else {
	
		newRaidStatus = Arbiter->RaidStatus;
	}
	
	if (Arbiter->RaidStatus != newRaidStatus) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Changing DRAID Status from %x to %x\n", Arbiter->RaidStatus, newRaidStatus) );

		if (newRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
		
			PDRAID_ARBITER_LOCK_CONTEXT Lock;
			KeQueryTickCount(&Arbiter->DegradedSince);

			// We need to merge all LWR to OOS bitmap
			//	because defective/stopped node may have lost data in disk's write-buffer.

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Merging LWRs to OOS bitmap\n") );

			for (listEntry = Arbiter->AcquiredLockList.Flink;
				 listEntry != &Arbiter->AcquiredLockList;
				 listEntry = listEntry->Flink) {

				Lock = CONTAINING_RECORD( listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink );
				
				DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
							("Merging LWR %I64x:%x to OOS bitmap\n", Lock->LockAddress, Lock->LockLength) );

				DraidArbiterChangeOosBitmapBit( Arbiter, TRUE, Lock->LockAddress, Lock->LockLength );
				
				// We need to write updated bitmap before any lock information is changed.
				
				bmpChanged = TRUE;
			}
		}
		
		if (newRaidStatus == DRIX_RAID_STATUS_REBUILDING) {

			// Initialize some values before enter rebuilding mode.
			Arbiter->RebuildInfo.AggressiveRebuildMode = FALSE;
		}
		
		Arbiter->RaidStatus = newRaidStatus;
		changed = TRUE;
	}

	if (changed) {

		Arbiter->SyncStatus = DRAID_SYNC_REQUIRED;
		// RMD update and RAID status sync will be done by arbiter thread.		
	}
	
	RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

	if (changed) {

		// Need to force-update because revived node may not have up-to-date RMD.

		DraidArbiterUpdateInCoreRmd( Arbiter, TRUE );
		DraidArbiterWriteRmd( Arbiter, &Arbiter->Rmd );
	}

	if (bmpChanged) {
		
		// We need to write updated bitmap before any lock information is changed.
		
		status = DraidArbiterUpdateOnDiskOosBitmap( Arbiter, FALSE );
	
		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to update bitmap\n") );
		}
	}

	return changed;
}


VOID
DraidArbiterTerminateClient (
	PDRAID_ARBITER_INFO		Arbiter,
	PDRAID_CLIENT_CONTEXT	Client,
	BOOLEAN					CleanExit	// Client is terminated cleanly through retire.
	) 
{
	PDRAID_ARBITER_LOCK_CONTEXT	lock;
	PLIST_ENTRY					listEntry;
	NTSTATUS					status;
	KIRQL						oldIrql;
	PDRIX_MSG_CONTEXT			msgEntry;
	BOOLEAN						abandonedLockExist;


	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Terminating client..\n") );
	
	ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
	
	// Free locks acquired by this client
	
	abandonedLockExist = FALSE;
	
	while (TRUE) {
	
		listEntry = RemoveHeadList( &Client->AcquiredLockList );

		if (listEntry == &Client->AcquiredLockList) {

			break;
		}

		lock = CONTAINING_RECORD( listEntry, DRAID_ARBITER_LOCK_CONTEXT, ClientAcquiredLink );
		
		// Remove from arbiter's list too.
		RemoveEntryList( &lock->ArbiterAcquiredLink );
		
		// Remove from to yield lock list.
		RemoveEntryList( &lock->ToYieldLink );
		
		InitializeListHead( &lock->ToYieldLink ); // to check bug...
		
		InterlockedDecrement( &Arbiter->AcquiredLockCount );
//		DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Arbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount));
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("Freeing terminated client's lock %I64x(%I64x:%x)\n", 
					 lock->LockId, lock->LockAddress, lock->LockLength) );

		if (!CleanExit) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Unclean termination. Merging this lock range to dirty bitmap\n") );

			// Merge this client's LWR to bitmap
			
			DraidArbiterChangeOosBitmapBit( Arbiter, TRUE, lock->LockAddress, lock->LockLength );
			abandonedLockExist = TRUE;			
		}
		
		DraidArbiterFreeLock( Arbiter, lock );
	}

	if (abandonedLockExist) {
		
		// RAID data may be inconsistent due to disconnected client.
		// Try to synchronize regions locked by this client
		
		if (Arbiter->OutOfSyncRole == NO_OUT_OF_SYNC_ROLE) {
		
			// Select one node.

			ASSERT( FALSE );

			DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
						("Client exited without unlocking locks. Marking node %d as out-of-sync.\n", 
						 Arbiter->RoleToNodeMap[Arbiter->ActiveDiskCount-1]) );

			Arbiter->OutOfSyncRole = Arbiter->ActiveDiskCount-1;
		}
	}
	
	for (listEntry = Arbiter->PendingLockList.Flink;
		 listEntry != &Arbiter->PendingLockList;
		 listEntry = listEntry->Flink) {

		lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
		
		if (lock->Owner == Client) {
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Freeing terminated client's pending lock %I64x(%I64x:%x)\n", 
						 lock->LockId, lock->LockAddress, lock->LockLength) );

			// Remove from all list
			
			listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
			RemoveEntryList( &lock->PendingLink );
			
			ASSERT( IsListEmpty(&lock->ArbiterAcquiredLink) );
			ASSERT( IsListEmpty(&lock->ToYieldLink) );			
			ASSERT( lock->Status == DRAID_ARBITER_LOCK_STATUS_PENDING );
		
			DraidArbiterFreeLock( Arbiter, lock );
		}
	}

	if (Client->LocalClient) {
		
		// Remove local client's reference to this client context.
		
		Client->LocalClient->RequestChannel = NULL;
		Client->LocalClient->NotificationReplyChannel = NULL;
		
		while (TRUE) {

			listEntry = RemoveHeadList( &Client->RequestChannel.Queue );
			
			if (listEntry == &Client->RequestChannel.Queue) {

				break;
			}

			msgEntry = CONTAINING_RECORD(listEntry, DRIX_MSG_CONTEXT, Link );
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing unhandled request message\n") );
			
			ExFreePoolWithTag( msgEntry->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG );
			ExFreePoolWithTag( msgEntry, DRAID_MSG_LINK_POOL_TAG );
		}

		while (TRUE) {

			listEntry = RemoveHeadList(&Client->NotificationReplyChannel.Queue);
			
			if (listEntry == &Client->NotificationReplyChannel.Queue) {
			
				break;
			}

			msgEntry = CONTAINING_RECORD(listEntry, DRIX_MSG_CONTEXT, Link);
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing unhandled notification-reply message\n") );

			ExFreePoolWithTag( msgEntry->Message, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG );
			ExFreePoolWithTag( msgEntry, DRAID_MSG_LINK_POOL_TAG );
		}

		Client->LocalClient = NULL;
	} 

	DraidArbiterUpdateLwrBitmapBit( Arbiter, NULL, NULL );
	
	RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

	status = DraidArbiterUpdateOnDiskOosBitmap( Arbiter, FALSE );

	if (!NT_SUCCESS(status)) {
	
		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to update OOS bitmap\n") );
	}

	if (Client->NotificationConnection) {

		if (Client->NotificationConnection->ConnectionFileObject) {
		
			LpxTdiDisconnect( Client->NotificationConnection->ConnectionFileObject, 0 );
			LpxTdiDisassociateAddress( Client->NotificationConnection->ConnectionFileObject );
			LpxTdiCloseConnection( Client->NotificationConnection->ConnectionFileHandle, 
								   Client->NotificationConnection->ConnectionFileObject );
		}

		ExFreePoolWithTag( Client->NotificationConnection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG );
		Client->NotificationConnection = NULL;
	} 

	if (Client->RequestConnection) {
	
		if (Client->RequestConnection->ConnectionFileObject) {

			LpxTdiDisconnect( Client->RequestConnection->ConnectionFileObject, 0 );
			LpxTdiDisassociateAddress( Client->RequestConnection->ConnectionFileObject );
			LpxTdiCloseConnection( Client->RequestConnection->ConnectionFileHandle, 
								   Client->RequestConnection->ConnectionFileObject );
		}

		ExFreePoolWithTag( Client->RequestConnection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG );
		Client->RequestConnection = NULL;
	}

	DraidArbiterRefreshRaidStatus( Arbiter, abandonedLockExist );

	if (Client->UnregisterRequest) {

		KeSetEvent( Client->UnregisterDoneEvent, IO_NO_INCREMENT, FALSE );			
	}
	
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing client context\n") );

	ExFreePoolWithTag( Client, DRAID_CLIENT_CONTEXT_POOL_TAG );
}

//
// MsgHandled is changed only when message is not handled.
//

NTSTATUS
DraidArbiterCheckRequestMsg (
	IN PDRAID_ARBITER_INFO	Arbiter,
	IN PBOOLEAN				MsgHandled
	) 
{
	NTSTATUS				status = STATUS_SUCCESS;
	PDRAID_CLIENT_CONTEXT	client;
	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
		
	*MsgHandled = FALSE;

	if (Arbiter->LocalClient) {
	
		KeClearEvent( &Arbiter->LocalClient->RequestChannel.Event );

		// Handle any pending requests from local client
		
		while(listEntry = ExInterlockedRemoveHeadList(&Arbiter->LocalClient->RequestChannel.Queue,
													  &Arbiter->LocalClient->RequestChannel.Lock)) {

			PDRIX_MSG_CONTEXT msg;
			
			msg = CONTAINING_RECORD( listEntry, DRIX_MSG_CONTEXT, Link );

			status = DraidArbiterHandleRequestMsg( Arbiter, Arbiter->LocalClient, msg->Message );
			
			if (!NT_SUCCESS(status)) {

				ASSERT(FALSE);
			}

			*MsgHandled = TRUE;
			
			// Free received message
			
			ExFreePoolWithTag( msg->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG );
			ExFreePoolWithTag( msg, DRAID_MSG_LINK_POOL_TAG );
		}
	}

restart:
	
	// Check request is received through request connection.
	
	ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
	
	for (listEntry = Arbiter->ClientList.Flink;
		 listEntry != &Arbiter->ClientList;
		 listEntry = listEntry->Flink) {
		
		client = CONTAINING_RECORD( listEntry, DRAID_CLIENT_CONTEXT, Link );
		
		if (client->RequestConnection				&& 
			client->RequestConnection->Receiving	&&
			KeReadStateEvent(&client->RequestConnection->TdiReceiveContext.CompletionEvent)) {
			
			BOOLEAN errorOccured = FALSE;
			
			client->RequestConnection->Receiving = FALSE;
			
			KeClearEvent( &client->RequestConnection->TdiReceiveContext.CompletionEvent );
			
			if (client->RequestConnection->TdiReceiveContext.Result < 0) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Failed to receive from %02x:%02x:%02x:%02x:%02x:%02x\n",
							 client->RemoteClientAddr[0], client->RemoteClientAddr[1], client->RemoteClientAddr[2],
							 client->RemoteClientAddr[3], client->RemoteClientAddr[4], client->RemoteClientAddr[5]) );
				
				errorOccured = TRUE;
//				ASSERT(FALSE);
			
			} else if (client->RequestConnection->TdiReceiveContext.Result != sizeof(DRIX_HEADER)) {
			
				NDASSCSI_ASSERT( FALSE );

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Failed to receive from %02x:%02x:%02x:%02x:%02x:%02x. Only %d bytes received.\n",
							 client->RemoteClientAddr[0], client->RemoteClientAddr[1], client->RemoteClientAddr[2],
							 client->RemoteClientAddr[3], client->RemoteClientAddr[4], client->RemoteClientAddr[5],
							 client->RequestConnection->TdiReceiveContext.Result) );
				
				errorOccured = TRUE;

			} else {

				PDRIX_HEADER	message;
				ULONG			msgLength;
				
				DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, 
							("Request received from %02x:%02x:%02x:%02x:%02x:%02x\n",
							 client->RemoteClientAddr[0], client->RemoteClientAddr[1], client->RemoteClientAddr[2],
							 client->RemoteClientAddr[3], client->RemoteClientAddr[4], client->RemoteClientAddr[5]) );

				RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

				// Read remaining data if needed.
				
				message = (PDRIX_HEADER) client->RequestConnection->ReceiveBuf;
				
				msgLength = NTOHS(message->Length);
				
				if (msgLength > DRIX_MAX_REQUEST_SIZE) {

					NDASSCSI_ASSERT( FALSE );
					errorOccured = TRUE;
				
				} else if (msgLength > sizeof(DRIX_HEADER)) {

					LARGE_INTEGER	timeout;
					LONG			result;
					ULONG			addtionalLength;
					
					timeout.QuadPart = 5 * HZ;
					addtionalLength = msgLength -sizeof(DRIX_HEADER);

					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Reading additional message data %d bytes\n", addtionalLength) );
					
					if (client->RequestConnection->ConnectionFileObject) {
						
						status = LpxTdiRecv( client->RequestConnection->ConnectionFileObject, 
											 (PUCHAR)(client->RequestConnection->ReceiveBuf + sizeof(DRIX_HEADER)),
											 addtionalLength,
											 0, 
											 &timeout, 
											 NULL, 
											 &result );
					
					} else {

						status = STATUS_UNSUCCESSFUL;
					}
					
					if (!NT_SUCCESS(status) || result != addtionalLength) {
					
						NDASSCSI_ASSERT( FALSE );

						DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to get remaining message\n", msgLength) );		
						
						errorOccured = TRUE;
						ASSERT(FALSE);
					}

				} else if (msgLength < sizeof(DRIX_HEADER)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Message is too short %d\n", msgLength) );
					
					errorOccured = TRUE;
				}
				
				if (!errorOccured) {
					
					status = DraidArbiterHandleRequestMsg( Arbiter, client, (PDRIX_HEADER)client->RequestConnection->ReceiveBuf );
					
					if (NT_SUCCESS(status)) {						
					
						*MsgHandled = TRUE;

					} else {

						errorOccured = TRUE;
					}
				}

				ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
			}

			if (errorOccured) {
			
				RemoveEntryList( &client->Link );
				RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );				
				DraidArbiterTerminateClient( Arbiter, client, FALSE );

				goto restart;
			}			
		}	
	}

	RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

	return status;
}


//
// Handle requests sent by DRAID clients. Return error only if error is fatal.
//

NTSTATUS
DraidArbiterHandleRequestMsg (
	PDRAID_ARBITER_INFO		Arbiter, 
	PDRAID_CLIENT_CONTEXT	Client,
	PDRIX_HEADER			Message
	)
{
	NTSTATUS					status;
	PDRIX_HEADER				replyMsg;
	UINT32						i;
	UINT32						replyLength;
	BOOLEAN						result;
	PLIST_ENTRY					listEntry;
	UCHAR						resultCode = DRIX_RESULT_SUCCESS;
	KIRQL						oldIrql;
	PDRAID_ARBITER_LOCK_CONTEXT newLock = NULL;
	UINT32						granularity = Arbiter->LockRangeGranularity;

	
	// Check data validity.

	if (NTOHL(Message->Signature) != DRIX_SIGNATURE) {

		NDASSCSI_ASSERT( FALSE );
		
		return STATUS_UNSUCCESSFUL;
	}

	if (Message->ReplyFlag != 0) {

		NDASSCSI_ASSERT( FALSE );

		return STATUS_UNSUCCESSFUL;
	}

	// Get reply packet size

	switch (Message->Command) {
	
	case DRIX_CMD_ACQUIRE_LOCK:
	
		replyLength = sizeof(DRIX_ACQUIRE_LOCK_REPLY);
		break;

	case DRIX_CMD_NODE_CHANGE:
	case DRIX_CMD_RELEASE_LOCK:
	default:
	
		replyLength = sizeof(DRIX_HEADER);
		break;
	}

	// Create reply
	
 	replyMsg = ExAllocatePoolWithTag( NonPagedPool, replyLength, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG );
	
	if (replyMsg == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( replyMsg, replyLength );

	replyMsg->Signature = HTONL(DRIX_SIGNATURE);

	replyMsg->ReplyFlag = TRUE;
	replyMsg->Command = Message->Command;
	replyMsg->Length = HTONS((UINT16)replyLength);
	replyMsg->Sequence = Message->Sequence;

	// Process request
	
	switch (Message->Command) {

	case DRIX_CMD_NODE_CHANGE: {

		PDRIX_NODE_CHANGE NcMsg = (PDRIX_NODE_CHANGE)Message;

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Arbiter received node change message\n") );		
		
		// Update local node information from packet
		
		for (i=0;i<NcMsg->UpdateCount;i++) {
			
			Client->NodeFlags[NcMsg->Node[i].NodeNum] = NcMsg->Node[i].NodeFlags;
			Client->DefectCode[NcMsg->Node[i].NodeNum] = NcMsg->Node[i].DefectCode;
			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("   Node %d: Flag %x, Defect %x\n", i, NcMsg->Node[i].NodeFlags, NcMsg->Node[i].DefectCode));
		}

		result = DraidArbiterRefreshRaidStatus(Arbiter, FALSE);
	
		if (result == TRUE) {
		
			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("RAID/Node status has been changed\n"));
			resultCode = DRIX_RESULT_REQUIRE_SYNC;

		} else {

			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("RAID/Node status has not been changed\n"));			
			resultCode = DRIX_RESULT_NO_CHANGE;
		}

		break;
	}

	case DRIX_CMD_ACQUIRE_LOCK: {

		PDRAID_ARBITER_LOCK_CONTEXT	lock;
		PDRIX_ACQUIRE_LOCK			acqMsg = (PDRIX_ACQUIRE_LOCK) Message;
		UINT64						overlapStart, overlapEnd;
		UINT64						addr = NTOHLL(acqMsg->Addr);
		UINT32						length = NTOHL(acqMsg->Length);
		BOOLEAN						matchFound = FALSE;


		DebugTrace( DBG_LURN_TRACE, ("Arbiter received ACQUIRE_LOCK message: %I64x:%x\n", addr, length) );

		// Check lock list if lock overlaps with lock acquired by other client
		
		ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
	
		for (listEntry = Arbiter->AcquiredLockList.Flink;
			 listEntry != &Arbiter->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
		
			if (DraidGetOverlappedRange(addr, 
										length,
										lock->LockAddress, 
										lock->LockLength, 
										&overlapStart, 
										&overlapEnd) != DRAID_RANGE_NO_OVERLAP) {
			
				matchFound = TRUE;

				if (lock == Arbiter->RebuildInfo.RebuildLock) {
				
					// Rebuilding IO is holding this lock.
					
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Area is locked for rebuilding.\n") );

					status = DraidRebuildIoCancel( Arbiter );

					if (NT_SUCCESS(status)) {

					} else {

						// In aggressive rebuild mode. Reduce granularity to rebuild size.

						granularity = DRAID_AGGRESSIVE_REBUILD_SIZE;
					}
				
				} else {
				
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Range is locked by another client. Return exact match\n") );

					granularity = 1;
					
					// Client has lock. Queue this lock to to-yield-list. This will be handled by Arbiter thread later.

					if (IsListEmpty(&lock->ToYieldLink)) {

						InsertTailList( &Arbiter->ToYieldLockList, &lock->ToYieldLink );
					}
					
					// Continue: multiple lock may be overlapped.
				}						
			}
		}

		if (matchFound == FALSE) {
		
			// Check whether another client is already waiting for this range.
		
			for (listEntry = Arbiter->PendingLockList.Flink;
				 listEntry != &Arbiter->PendingLockList;
				 listEntry = listEntry->Flink)  {
			
				lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
				
				if (DraidGetOverlappedRange(addr, 
											length,
											lock->LockAddress, 
											lock->LockLength, 
											&overlapStart, 
											&overlapEnd) != DRAID_RANGE_NO_OVERLAP) {
						
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Another client is waiting for range %I64x:%x. Return PENDING\n", lock->LockAddress, lock->LockLength) );

					matchFound = TRUE;
					granularity = 1;
					
					break;
				}
			}
		}
		
		RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

		newLock = DraidArbiterAllocLock( Arbiter, acqMsg->LockType, acqMsg->LockMode, addr, length );
	
		if (newLock == NULL) {

			resultCode = DRIX_RESULT_FAIL;
		
		} else if (matchFound) {
		
			// Lock is pending.
			
			ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
			
			newLock->Owner = Client;
			
			// Arrange lock with reduced range.

			DraidArbiterArrangeLockRange( Arbiter, newLock, granularity, FALSE );

			// Add to pending list.

			newLock->Status = DRAID_ARBITER_LOCK_STATUS_PENDING;
			newLock->Owner = Client;
			InsertTailList(&Arbiter->PendingLockList, &newLock->PendingLink);
			resultCode = DRIX_RESULT_PENDING;

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Lock %I64x(%I64x:%Ix) is pended.\n", newLock->LockId, newLock->LockAddress, newLock->LockLength) );
			
			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

		} else {
	
			// Lock is granted

			ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );

			resultCode = DRIX_RESULT_GRANTED;
			newLock->Owner = Client;
			newLock->Status = DRAID_ARBITER_LOCK_STATUS_GRANTED;

			// Adjust addr/length to possible larger size.
			
			status = DraidArbiterArrangeLockRange( Arbiter, newLock, Arbiter->LockRangeGranularity, TRUE );

			ASSERT( status == STATUS_SUCCESS );	// We should get success

			// Add to arbiter list

			InsertTailList( &Arbiter->AcquiredLockList, &newLock->ArbiterAcquiredLink );
			InterlockedIncrement( &Arbiter->AcquiredLockCount );
				
			InsertTailList( &Client->AcquiredLockList, &newLock->ClientAcquiredLink );
			
			DraidArbiterUpdateLwrBitmapBit( Arbiter, newLock, NULL );

			if (Arbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					        ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",newLock->LockAddress, newLock->LockLength) );

				DraidArbiterChangeOosBitmapBit( Arbiter, TRUE, newLock->LockAddress, newLock->LockLength );
			
			} else {
			
				DebugTrace( DBG_LURN_TRACE, 
							("Granted lock %I64x(%I64x:%Ix).\n", newLock->LockId, newLock->LockAddress, newLock->LockLength) );
			}
			
			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

			// Need to update BMP and LWR before client start to write using this lock
			
			status = DraidArbiterUpdateOnDiskOosBitmap( Arbiter, FALSE );

			if (!NT_SUCCESS(status)) {

				DebugTrace(NDASSCSI_DEBUG_LURN_ERROR, ("Failed to update bitmap\n"));
			}
		}

		if (newLock) {

			PDRIX_ACQUIRE_LOCK_REPLY AcqReply = (PDRIX_ACQUIRE_LOCK_REPLY) replyMsg;

			AcqReply->LockType = acqMsg->LockType;
			AcqReply->LockMode = acqMsg->LockMode;
			AcqReply->LockId   = NTOHLL(newLock->LockId);
			AcqReply->Addr     = NTOHLL(newLock->LockAddress);
			AcqReply->Length   = NTOHL(newLock->LockLength);

		} else {

			PDRIX_ACQUIRE_LOCK_REPLY AcqReply = (PDRIX_ACQUIRE_LOCK_REPLY) replyMsg;

			AcqReply->LockType = 0;
			AcqReply->LockMode = 0;
			AcqReply->LockId   = 0;
			AcqReply->Addr     = 0;
			AcqReply->Length   = 0;
		}

		break;
	}

	case DRIX_CMD_RELEASE_LOCK: {

		// Check lock is owned by this client.
	
		PDRAID_ARBITER_LOCK_CONTEXT	lock;
		PDRIX_RELEASE_LOCK			relMsg = (PDRIX_RELEASE_LOCK) Message;
		UINT64						lockId = NTOHLL(relMsg->LockId);

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Arbiter received RELEASE_LOCK message: %I64x\n", lockId) );

		// 1.0 chip does not support cache flush. 
		// Flush before releasing the lock.
	
		DraidArbiterFlushDirtyCacheNdas1_0( Arbiter, lockId, Client );
				
		resultCode = DRIX_RESULT_INVALID_LOCK_ID;

		ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );

		// Search for matching Lock ID

		for (listEntry = Arbiter->AcquiredLockList.Flink;
			 listEntry != &Arbiter->AcquiredLockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
		
			if (lockId == DRIX_LOCK_ID_ALL && lock->Owner == Client) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						    ("Releasing all locks - Lock %I64x:%x\n", lock->LockAddress, lock->LockLength) );

				// Remove from all list

				listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
				
				RemoveEntryList( &lock->ArbiterAcquiredLink );
				InterlockedDecrement( &Arbiter->AcquiredLockCount );
				
				RemoveEntryList( &lock->ToYieldLink );
				InitializeListHead( &lock->ToYieldLink ); // to check bug...
				RemoveEntryList( &lock->ClientAcquiredLink );
				
				resultCode = DRIX_RESULT_SUCCESS;
				DraidArbiterFreeLock( Arbiter, lock );

			} else if (lock->LockId == lockId) {

				if (lock->Owner != Client) {

					NDASSCSI_ASSERT(FALSE);					
					break;

				} else {

					// Remove from all list
					RemoveEntryList( &lock->ArbiterAcquiredLink );
					InterlockedDecrement( &Arbiter->AcquiredLockCount );
					
					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Arbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount) );						
					
					RemoveEntryList( &lock->ToYieldLink );
					InitializeListHead( &lock->ToYieldLink ); // to check bug...
					RemoveEntryList( &lock->ClientAcquiredLink );
					resultCode = DRIX_RESULT_SUCCESS;

					DraidArbiterFreeLock( Arbiter, lock );

					break;
				}
			}
		}
				
		RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

		DraidArbiterUpdateLwrBitmapBit( Arbiter, NULL, NULL );
		
		status = DraidArbiterUpdateOnDiskOosBitmap( Arbiter, FALSE );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to update OOS bitmap\n") );
		}

		break;
	}

	case DRIX_CMD_REGISTER:
//	case DRIX_CMD_UNREGISTER:
	default:

		NDASSCSI_ASSERT( FALSE );

		resultCode = DRIX_RESULT_UNSUPPORTED;
		break;
	}

	replyMsg->Result = resultCode;

	
	// Send reply
	
	if (Client->LocalClient) {
	
		PDRIX_MSG_CONTEXT replyMsgEntry;
		
		DebugTrace( DBG_LURN_TRACE, 
					("DRAID Sending reply to request %s with result %s to local client(event=%p)\n", 
					 DrixGetCmdString(Message->Command), DrixGetResultString(resultCode), &Client->RequestReplyChannel->Queue) );

		replyMsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);

		if (replyMsgEntry != NULL) {

			RtlZeroMemory( replyMsgEntry, sizeof(DRIX_MSG_CONTEXT) );

			InitializeListHead( &replyMsgEntry->Link );
			replyMsgEntry->Message = replyMsg;
			ExInterlockedInsertTailList( &Client->RequestReplyChannel->Queue, &replyMsgEntry->Link, &Client->RequestReplyChannel->Lock );
			KeSetEvent(&Client->RequestReplyChannel->Event, IO_NO_INCREMENT, FALSE);

			return STATUS_SUCCESS;

		} else {

			return STATUS_INSUFFICIENT_RESOURCES;
		}

	} else {

		LARGE_INTEGER	timeout;
		LONG			result;

		timeout.QuadPart = 5 * HZ;

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("DRAID Sending reply to request %s with result %s to remote client\n", 
					 DrixGetCmdString(Message->Command), DrixGetResultString(resultCode)) );

		if (Client->RequestConnection->ConnectionFileObject) {
		
			status = LpxTdiSend( Client->RequestConnection->ConnectionFileObject, (PUCHAR)replyMsg, replyLength,
								 0, &timeout, NULL, &result );

			ASSERT( replyMsg->Signature == HTONL(DRIX_SIGNATURE) );// Check wheter data is corrupted somehow..
		
		} else {
		
			status = STATUS_UNSUCCESSFUL;
		}

		ExFreePoolWithTag( replyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG );

		if (NT_SUCCESS(status)) {

			return STATUS_SUCCESS;

		} else {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to send request reply\n") );
		
			return STATUS_UNSUCCESSFUL;
		}		
	}
}


NTSTATUS 
DraidArbiterUpdateOnDiskOosBitmap (
	PDRAID_ARBITER_INFO	Arbiter,
	BOOLEAN				UpdateAll
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
	
	
	for (i=0; i<Arbiter->OosBmpByteCount/sizeof(ULONG); i++) {
	
		sector	  = DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(i*sizeof(ULONG));
		offset	  = DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(i*sizeof(ULONG));
		
		bitValues = Arbiter->OosBmpInCoreBuffer[i] | Arbiter->LwrBmpBuffer[i];
		
		if (*((PULONG)&Arbiter->OosBmpOnDiskBuffer[sector].Bits[offset]) == bitValues) {
			
			if (UpdateAll)  {
			
				Arbiter->DirtyBmpSector[sector] = TRUE;
			}
			continue;

		} else {
			
			DebugTrace( DBG_LURN_TRACE, ("Bitmap offset %x changed from %08x to %08x\n", 
										i*4, *((PULONG)&Arbiter->OosBmpOnDiskBuffer[sector].Bits[offset]), bitValues) );
		}

		*((PULONG)&Arbiter->OosBmpOnDiskBuffer[sector].Bits[offset]) = bitValues;

		Arbiter->DirtyBmpSector[sector] = TRUE;
	}

	// Update dirty bitmap sector only
	
	for (i=0; i<Arbiter->OosBmpSectorCount; i++) {

		ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
		
		if (Arbiter->DirtyBmpSector[i]) {
		
			Arbiter->OosBmpOnDiskBuffer[i].SequenceNumHead++;
			Arbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = Arbiter->OosBmpOnDiskBuffer[i].SequenceNumHead;
			Arbiter->DirtyBmpSector[i] = FALSE;
			
			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );
			
			DebugTrace( DBG_LURN_NOISE, 
						("Updating dirty bitmap sector %d, Seq = %I64x\n", i, Arbiter->OosBmpOnDiskBuffer[i].SequenceNumHead) );
			
			status = DraidArbiterWriteMetaSync( Arbiter, 
												(PUCHAR)&(Arbiter->OosBmpOnDiskBuffer[i]), 
												NDAS_BLOCK_LOCATION_BITMAP + i, 
												1 );

			if (!NT_SUCCESS(status)) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to update dirty bitmap sector %d\n", i) );	

				return status;
			}

		} else {

			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );
		}
	}

	return status;
}

//
// Write meta data to all non-defective running disk with flush
//

NTSTATUS
DraidArbiterWriteMetaSync (
	IN PDRAID_ARBITER_INFO	Arbiter,
	IN PUCHAR				BlockBuffer,
	IN INT64				Addr,
	IN UINT32				Length 	// in sector
	) 
{
	NTSTATUS			status = STATUS_SUCCESS;
	ULONG				i;
	UCHAR				nodeFlags;
	BOOLEAN				nodeFlagMismatch[MAX_DRAID_MEMBER_DISK] = {0};	// Report Lurn status and node flag is not matched.
	BOOLEAN				nodeUpdateHandled[MAX_DRAID_MEMBER_DISK] = {0}; // This node's metadata update is handled whether it is succeed or not
	BOOLEAN				updateSucceeded;
	BOOLEAN				lurnStatusMismatch;
	PLURNEXT_IDE_DEVICE	ideDisk;	
	UCHAR				defectCodes;
		
	if (!(GENERIC_WRITE & Arbiter->Lurn->AccessRight)) {

		// Only primary can call this
		
		NDASSCSI_ASSERT( FALSE );
		return STATUS_SUCCESS;
	}

	// Flush all disk before updating metadata because updated metadata information is valid under assumption that all written data is on disk.
	
	for (i = 0; i < Arbiter->Lurn->LurnChildrenCnt; i++) {

		nodeFlags = Arbiter->NodeFlags[i];	
		defectCodes = Arbiter->DefectCodes[i];

		if ( (nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) && (defectCodes & DRIX_NODE_DEFECT_BAD_SECTOR)) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Node %d flag: %x, defect: %x. Try to update bad sector information\n", 
						 i, nodeFlags, Arbiter->DefectCodes[i]) );

		} else if ((nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(nodeFlags & DRIX_NODE_FLAG_RUNNING)) {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Node %d flag: %x, defect: %x. Skipping metadata update\n", i, nodeFlags, Arbiter->DefectCodes[i]) );
			continue;
		}

		if (!LURN_IS_RUNNING(Arbiter->Lurn->LurnChildren[i]->LurnStatus)) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Node %d status is not running %x\n", i, Arbiter->Lurn->LurnChildren[i]->LurnStatus) );
			nodeFlagMismatch[i] = TRUE;

			continue;
		}

		ideDisk = (PLURNEXT_IDE_DEVICE)Arbiter->Lurn->LurnChildren[i]->LurnExtension;

		if (ideDisk && ideDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_1_0) {

			// We already flushed when releasing lock.
			status = STATUS_SUCCESS;

		} else {

			status = LurnExecuteSyncMulti( 1,
										   &Arbiter->Lurn->LurnChildren[i],
										   SCSIOP_SYNCHRONIZE_CACHE,
										   NULL,
										   0,
										   0,
										   NULL );
		}
		
		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to flush node %d.\n", i) );
			continue;			
		}
	}


	// Write non-out-of-sync disk and non spare disk first. 
	// Because only in-sync disk is gauranteed to have up-to-date OOS bitmap
	//	because bitmap is not written to disk always(only dirty map is written)
	// We should always keep in-sync disk has most largest USN number to rule out OOS disk when finding up-to-date RMD.
	// If we fail to update no in-sync-rmd, we fail whole metadata writing.

	updateSucceeded = FALSE;
	
	for (i = 0; i < Arbiter->ActiveDiskCount; i++) { // i is role index.
	
		ULONG nodeIdx = Arbiter->RoleToNodeMap[i];

		if (i!=Arbiter->OutOfSyncRole) {
		
			nodeUpdateHandled[nodeIdx] = TRUE;
			nodeFlags = Arbiter->NodeFlags[nodeIdx];
			defectCodes = Arbiter->DefectCodes[i];

			if ((nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) && (defectCodes & DRIX_NODE_DEFECT_BAD_SECTOR)) {
			
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Node %d flag: %x, defect: %x. Try to update bad sector information\n", 
							 i, nodeFlags, Arbiter->DefectCodes[i]) );
			
			} else if ((nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(nodeFlags & DRIX_NODE_FLAG_RUNNING)) {

				continue;
			}

			if (!LURN_IS_RUNNING(Arbiter->Lurn->LurnChildren[nodeIdx]->LurnStatus)) {

				nodeFlagMismatch[nodeIdx] = TRUE;
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Node %d is not running\n", nodeIdx) );
				
				continue;
			}
			
			//LurnExecuteSyncWrite do flush.

			status = LurnExecuteSyncWrite( Arbiter->Lurn->LurnChildren[nodeIdx], BlockBuffer, Addr, Length );

			if (!NT_SUCCESS(status)) {

				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Failed to flush node %d.\n", i));
				continue;			
			}

			updateSucceeded = TRUE;
		}
	}

	if (!updateSucceeded) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to update any in-sync disk\n") );

		goto out;
	}
		
	// Now update other nodes

	for (i = 0; i < Arbiter->Lurn->LurnChildrenCnt; i++) {

		if (nodeUpdateHandled[i]) {

			continue;
		}

		nodeFlags = Arbiter->NodeFlags[i];	
		defectCodes = Arbiter->DefectCodes[i];

		if ((nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) && (defectCodes & DRIX_NODE_DEFECT_BAD_SECTOR)) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Node %d flag: %x, defect: %x. Try to update bad sector information\n", 
						 i, nodeFlags, Arbiter->DefectCodes[i]) );
		
		} else if ((nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(nodeFlags & DRIX_NODE_FLAG_RUNNING)) {

//			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Node %d is not good for RMD writing. Skipping\n", i));			

			continue;
		}

		if (!LURN_IS_RUNNING(Arbiter->Lurn->LurnChildren[i]->LurnStatus)) {
			
			nodeFlagMismatch[i] = TRUE;
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Normal node %d is not running\n", i) );

			continue;
		}

		// LurnExecuteSyncWrite do flush.
		
		status = LurnExecuteSyncWrite( Arbiter->Lurn->LurnChildren[i], BlockBuffer, Addr, Length );

		if (!NT_SUCCESS(status)) {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to flush node %d.\n", i) );

			continue;			
		}
	}

out:

	if (updateSucceeded) {
	
		status = STATUS_SUCCESS;

	} else {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to update metadata to any node\n") );
		status = STATUS_UNSUCCESSFUL;
	}

	// Report lurn status if any lurn status and arbiter node flag is not matched.
	
	lurnStatusMismatch = FALSE;
	
	for(i = 0; i < Arbiter->Lurn->LurnChildrenCnt; i++) {
	
		if (nodeFlagMismatch[i]) {
		
			lurnStatusMismatch = TRUE;
			break;
		}
	}

	if (lurnStatusMismatch) {
	
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("LURN status is different from node flag. Requesting to update local client.\n") );
		
		if (Arbiter->LocalClient && Arbiter->LocalClient->LocalClient) {
		
			DraidClientUpdateAllNodeFlags( Arbiter->LocalClient->LocalClient );

		} else {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("No local client to notify.\n") );
		}
	}

	return status;
}


//
// Set Arbiter->RMD fields based on current status.
// Return TRUE if InCore RMD has changed

BOOLEAN
DraidArbiterUpdateInCoreRmd (
	IN PDRAID_ARBITER_INFO	Arbiter,
	IN BOOLEAN				ForceUpdate
	) 
{
	NDAS_RAID_META_DATA newRmd = {0};
	UINT32				i, j;
	UCHAR				nodeFlags;


	newRmd.Signature = Arbiter->Rmd.Signature;
	newRmd.RaidSetId = Arbiter->RaidSetId;
	newRmd.uiUSN = Arbiter->Rmd.uiUSN;
	newRmd.ConfigSetId = Arbiter->ConfigSetId;
		
	if (DRAID_ARBITER_STATUS_TERMINATING == Arbiter->Status) {

		newRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;

	} else {

		newRmd.state = NDAS_RAID_META_DATA_STATE_MOUNTED;
	}

	// Keep NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED.

	switch (Arbiter->RaidStatus) {

	case DRIX_RAID_STATUS_DEGRADED:
		
		if (!(Arbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );
		}

		newRmd.state |= NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;		
		break;

	case DRIX_RAID_STATUS_NORMAL:
	case DRIX_RAID_STATUS_REBUILDING:
		
		if (Arbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Clearing NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );			
		}

		newRmd.state &= ~NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;

		break;
	
	default: // Keep previous flag
	
		newRmd.state |= (Arbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED);

		if (Arbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Keep marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n") );
		}

		break;
	}

	if (newRmd.state & NDAS_RAID_META_DATA_STATE_MOUNTED) {

		UINT32					usedAddrCount;
		PUCHAR					macAddr;
		BOOLEAN					isNewAddr;
		PLURNEXT_IDE_DEVICE		ideDisk;
		
		RtlZeroMemory( newRmd.ArbiterInfo, sizeof(newRmd.ArbiterInfo) );

		ASSERT(sizeof(newRmd.ArbiterInfo) == 12 * 8);
		
		// Get list of bind address from each children.
		
		usedAddrCount = 0;
		
		for (i = 0; i < Arbiter->TotalDiskCount; i++) {
		
			// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
			
			if (!Arbiter->Lurn->LurnChildren[i]) {
				
				continue;
			}

			if(LURN_STATUS_RUNNING != Arbiter->Lurn->LurnChildren[i]->LurnStatus) {

				continue;
			}

			ideDisk = (PLURNEXT_IDE_DEVICE)Arbiter->Lurn->LurnChildren[i]->LurnExtension;
			
			if (!ideDisk) {

				continue;
			}

			macAddr = ((PTDI_ADDRESS_LPX)&ideDisk->LanScsiSession.BindAddress.Address[0].Address)->Node;
				
			isNewAddr = TRUE;

			// Search address is already recorded.
			
			for (j=0; j < usedAddrCount; j++) {
			
				if (RtlCompareMemory(newRmd.ArbiterInfo[j].Addr, macAddr, 6) == 6) {
					
					// This bind address is alreay in entry. Skip

					isNewAddr = FALSE;
				
					break;
				}
			}

			if (isNewAddr) {

				newRmd.ArbiterInfo[usedAddrCount].Type = NDAS_DRAID_ARBITER_TYPE_LPX;
				
				RtlCopyMemory( newRmd.ArbiterInfo[usedAddrCount].Addr, macAddr, 6 );
				usedAddrCount++;
			}

			if (usedAddrCount >= NDAS_DRAID_ARBITER_ADDR_COUNT) {
				
				break;
			}
		}

//		ASSERT(UsedAddrCount >0);
	
	} else {
	
		RtlZeroMemory( newRmd.ArbiterInfo, sizeof(newRmd.ArbiterInfo) );
		
		for (i=0; i<NDAS_DRAID_ARBITER_ADDR_COUNT; i++) {

			newRmd.ArbiterInfo[i].Type = NDAS_DRAID_ARBITER_TYPE_NONE;
		}
	}
	
	for (i=0;i<Arbiter->TotalDiskCount;i++) { // i is role index.
		
		newRmd.UnitMetaData[i].iUnitDeviceIdx = Arbiter->RoleToNodeMap[i];
		
		// NodeFlags is flag of role i's node flag.(Not ith node)
		
		nodeFlags = Arbiter->NodeFlags[newRmd.UnitMetaData[i].iUnitDeviceIdx];
		
		if (nodeFlags & DRIX_NODE_FLAG_DEFECTIVE) {
		
			UCHAR defectCode = Arbiter->DefectCodes[newRmd.UnitMetaData[i].iUnitDeviceIdx];

			// Relative defect code such as RMD mismatch is not needed to be written to disk
			// Record physical defects and spare-used flag only

			newRmd.UnitMetaData[i].UnitDeviceStatus |= DraidNodeDefectCodeToRmdUnitStatus( defectCode );
		}
		
		if (i == Arbiter->OutOfSyncRole) {

			// Only one unit can be out-of-sync.

			newRmd.UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		}

		if (i >= Arbiter->ActiveDiskCount) {

			newRmd.UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_SPARE;
		}
	}

	SET_RMD_CRC( crc32_calc, newRmd );

	if (ForceUpdate ||
		RtlCompareMemory(&newRmd, &Arbiter->Rmd, sizeof(NDAS_RAID_META_DATA)) != sizeof(NDAS_RAID_META_DATA)) {
		
		// Changed
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Changing in memory RMD\n") );
		
		newRmd.uiUSN++;
		
		SET_RMD_CRC( crc32_calc, newRmd );

		RtlCopyMemory( &Arbiter->Rmd, &newRmd, sizeof(NDAS_RAID_META_DATA) );
		
		return TRUE;
	
	} else {
	
		return FALSE;
	}
}


NTSTATUS
DraidArbiterNotify (
	PDRAID_ARBITER_INFO		Arbiter, 
	PDRAID_CLIENT_CONTEXT	Client,
	UCHAR					Command,
	UINT64					CmdParam1,	// CmdParam* are dependent on Command.
	UINT64					CmdParam2,
	UINT32					CmdParam3
	) 
{
	NTSTATUS		status;
	PDRIX_HEADER	notifyMsg;
	UINT32			msgLength;
	UINT32			i;
	KIRQL			oldIrql;
	

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Notifying client with command %s\n", DrixGetCmdString(Command)) );
	
	// Create notification message.
	
	switch (Command) {
	
	case DRIX_CMD_CHANGE_STATUS:
		
		msgLength = SIZE_OF_DRIX_CHANGE_STATUS(Arbiter->TotalDiskCount);
		break;
	
	case DRIX_CMD_REQ_TO_YIELD_LOCK:

		msgLength = sizeof(DRIX_REQ_TO_YIELD_LOCK);
		break;
	
	case DRIX_CMD_GRANT_LOCK:
	
		msgLength = sizeof(DRIX_GRANT_LOCK);
		break;

	case DRIX_CMD_RETIRE:
	case DRIX_CMD_STATUS_SYNCED:
	default:

		msgLength = sizeof(DRIX_HEADER);
		break;
	}

	// For local client, msg will be freed by receiver, for remote client, msg will be freed after sending message.
	
	notifyMsg = ExAllocatePoolWithTag(NonPagedPool, msgLength, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
	
	if (notifyMsg == NULL) {
	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( notifyMsg, msgLength );

	notifyMsg->Signature = HTONL(DRIX_SIGNATURE);
	notifyMsg->Command = Command;
	notifyMsg->Length = HTONS((UINT16)msgLength);
	notifyMsg->Sequence = HTONS(Client->NotifySequence);
	Client->NotifySequence++;

	if (DRIX_CMD_CHANGE_STATUS == Command) {

		PDRIX_CHANGE_STATUS csMsg = (PDRIX_CHANGE_STATUS) notifyMsg;
		
		// Set additional info
		// CmdParam1: WaitForSync
		
		csMsg->Usn = NTOHL(Arbiter->Rmd.uiUSN);
		csMsg->RaidStatus = (UCHAR)Arbiter->RaidStatus;
		csMsg->NodeCount = (UCHAR)Arbiter->TotalDiskCount;
		
		RtlCopyMemory(&csMsg->ConfigSetId, &Arbiter->ConfigSetId, sizeof(csMsg->ConfigSetId));
		
		if (CmdParam1)  {
		
			csMsg->WaitForSync = (UCHAR)1;

		} else {

			csMsg->WaitForSync = (UCHAR)0;
		}

		for (i=0; i<csMsg->NodeCount; i++) {
			
			csMsg->Node[i].NodeFlags = Arbiter->NodeFlags[i];
			
			if (Arbiter->OutOfSyncRole != NO_OUT_OF_SYNC_ROLE &&  i==Arbiter->RoleToNodeMap[Arbiter->OutOfSyncRole]) {

				csMsg->Node[i].NodeFlags |= DRIX_NODE_FLAG_OUT_OF_SYNC;
			}
			
			csMsg->Node[i].NodeRole = Arbiter->NodeToRoleMap[i];
		}

	} else if (DRIX_CMD_REQ_TO_YIELD_LOCK == Command) {
		
		PDRIX_REQ_TO_YIELD_LOCK yieldMsg = (PDRIX_REQ_TO_YIELD_LOCK) notifyMsg;
	
		// CmdParam2: LockId
		// CmdParam3: Reason

		yieldMsg->LockId = HTONLL(CmdParam2);
		yieldMsg->Reason = HTONL(CmdParam3);
	
	} else if (DRIX_CMD_GRANT_LOCK == Command) {
	
		PDRIX_GRANT_LOCK grantMsg = (PDRIX_GRANT_LOCK) notifyMsg;
		PDRAID_ARBITER_LOCK_CONTEXT lock = (PDRAID_ARBITER_LOCK_CONTEXT) CmdParam1;
		
		// CmdParam1: Ptr to Lock
		
		grantMsg->LockId = HTONLL(lock->LockId);
		grantMsg->LockType = lock->LockType;
		grantMsg->LockMode = lock->LockMode;
		grantMsg->Addr = HTONLL(lock->LockAddress);
		grantMsg->Length = HTONL(lock->LockLength);
	}

	if (Client->LocalClient) {
		
		// Send via local notification channel.
				
		PDRIX_MSG_CONTEXT	msgEntry;
		PDRIX_MSG_CONTEXT	replyMsgEntry;
		PLIST_ENTRY			listEntry;

		PDRIX_HEADER		replyMsg =NULL;

		ACQUIRE_SPIN_LOCK(&Client->LocalClient->SpinLock, &oldIrql);
		
		if (Client->LocalClient->ClientState != DRAID_CLIENT_STATE_ARBITER_CONNECTED) {
		
			RELEASE_SPIN_LOCK(&Client->LocalClient->SpinLock, oldIrql);
			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Local client has terminated\n"));
			ExFreePoolWithTag(notifyMsg, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}

		RELEASE_SPIN_LOCK(&Client->LocalClient->SpinLock, oldIrql);
		DebugTrace(DBG_LURN_TRACE, ("DRAID Sending notification %x to local client\n", DrixGetCmdString(Command)));	
		msgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
		
		if (msgEntry) {
		
			PKEVENT	events[2];
			LONG	eventCount = 2;

			RtlZeroMemory(msgEntry, sizeof(DRIX_MSG_CONTEXT));
			InitializeListHead(&msgEntry->Link);
			msgEntry->Message = notifyMsg;
			ExInterlockedInsertTailList(&Client->NotificationChannel->Queue, &msgEntry->Link, &Client->NotificationChannel->Lock);
			DebugTrace(DBG_LURN_TRACE, ("Setting event %p\n", &Client->NotificationChannel->Event));
			KeSetEvent(&Client->NotificationChannel->Event,IO_NO_INCREMENT, FALSE);

			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Waiting for notification reply %p\n", &Client->NotificationReplyChannel.Event));
			// Wait for reply or local client terminate event.
			events[0] = &Client->NotificationReplyChannel.Event;
			events[1] = Arbiter->ClientTerminatingEvent;

			status = KeWaitForMultipleObjects( eventCount, 
											   events,
											   WaitAny,
											   Executive,KernelMode,
											   TRUE,
											   NULL,
											   NULL );

			if (status == STATUS_WAIT_0) {
			
				KeClearEvent(&Client->NotificationReplyChannel.Event);
				DebugTrace(DBG_LURN_TRACE, ("Received reply\n"));
				
				listEntry = ExInterlockedRemoveHeadList( &Client->NotificationReplyChannel.Queue, 
														 &Client->NotificationReplyChannel.Lock );

				if (listEntry) {

					replyMsgEntry = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
					replyMsg = replyMsgEntry->Message;
					
					// Notification always success.
					
					ExFreePoolWithTag(replyMsgEntry, DRAID_MSG_LINK_POOL_TAG);
					status = STATUS_SUCCESS;
					DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("DRAID Notification result=%x\n", replyMsg->Result));		
					ExFreePoolWithTag(replyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
				}

			} else {
				
				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Local client terminating. Stop waiting for notification.\n"));
				status = STATUS_UNSUCCESSFUL;
			}

		} else {
			
			status = STATUS_INSUFFICIENT_RESOURCES;
		}

	} else {
		
		LARGE_INTEGER	timeout;
		LONG			result;
		DRIX_HEADER		replyMsg;
		BOOLEAN			errorOccured = FALSE;

		timeout.QuadPart = 5 * HZ;

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("DRAID Sending notification %s to remote client\n", DrixGetCmdString(Command)) );
		
		ASSERT(Client);
		ASSERT(Client->NotificationConnection);

		if (Client->NotificationConnection && Client->NotificationConnection->ConnectionFileObject) {
		
			status = LpxTdiSend( Client->NotificationConnection->ConnectionFileObject, 
								 (PUCHAR)notifyMsg, 
								 msgLength,
								 0, 
								 &timeout, 
								 NULL, 
								 &result );
		} else {

			DebugTrace(DBG_LURN_ERROR, ("Notification connection does not exist\n"));
			status = STATUS_UNSUCCESSFUL;
		}

		ExFreePoolWithTag(notifyMsg, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
		
		if (!NT_SUCCESS(status)) {

			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Failed to send notification message\n"));
			errorOccured = TRUE;
			status = STATUS_UNSUCCESSFUL;
		
		} else {
		
			// Start synchrous receiving with short timeout.
			// To do: Receive reply in asynchrous mode and handle receive in seperate function to remove arbiter blocking.
			
			timeout.QuadPart = 5 * HZ;
			
			if (Client->NotificationConnection->ConnectionFileObject) {
			
				status = LpxTdiRecv( Client->NotificationConnection->ConnectionFileObject,
									 (PUCHAR)&replyMsg, 
									 sizeof(DRIX_HEADER), 
									 0, 
									 &timeout, 
									 NULL, 
									 &result );
			
			} else {
			
				status = STATUS_UNSUCCESSFUL;
			}

			if (NT_SUCCESS(status) && result == sizeof(DRIX_HEADER)) {

				// Check validity

				if (replyMsg.Signature == HTONL(DRIX_SIGNATURE) &&
					replyMsg.Command ==Command &&
					replyMsg.ReplyFlag == 1 &&
					replyMsg.Length == HTONS((UINT16)sizeof(DRIX_HEADER)) &&
					replyMsg.Sequence == HTONS(Client->NotifySequence-1)) {
					
					DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("DRAID Notification result=%s\n", DrixGetResultString(replyMsg.Result)));
					
					status = STATUS_SUCCESS;
					errorOccured  = FALSE;
				
				} else {
				
					DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Invalid reply packet\n"));
					status = STATUS_UNSUCCESSFUL;
					errorOccured  = TRUE;
					ASSERT(FALSE);
				}

			} else {
				
				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Failed to recv reply\n"));
				status = STATUS_UNSUCCESSFUL;
				errorOccured = TRUE;
			}
		}

		if (errorOccured) {
		
			ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
			RemoveEntryList(&Client->Link);
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);				
			DraidArbiterTerminateClient(Arbiter, Client, FALSE);

		} else {

			// Handle reply.

			if (Command == DRIX_CMD_RETIRE) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Terminating client context after RETIRING message.\n") );

				// Don't remove from Client->Link. Client is already removed from link before calling this.

				if (replyMsg.Result != DRIX_RESULT_SUCCESS) {

					DraidArbiterTerminateClient( Arbiter, Client, FALSE );

				} else {

					DraidArbiterTerminateClient( Arbiter, Client, TRUE );
				}
			}
		}
	}
out:
	return status;
}

// Called with Arbiter->SpinLock

VOID
DraidArbiterUpdateLwrBitmapBit (
	PDRAID_ARBITER_INFO			Arbiter,
	PDRAID_ARBITER_LOCK_CONTEXT HintAddedLock,
	PDRAID_ARBITER_LOCK_CONTEXT HintRemovedLock
	) 
{
	PLIST_ENTRY					listEntry;
	PDRAID_ARBITER_LOCK_CONTEXT lock;
	UINT32						bitOffset;
	UINT32						numberOfBit;
	ULONG						lockCount = 0;

	if (HintAddedLock && HintRemovedLock==NULL) {

		bitOffset = (UINT32) (HintAddedLock->LockAddress / Arbiter->SectorsPerOosBmpBit);
		numberOfBit = (UINT32) ((HintAddedLock->LockAddress + HintAddedLock->LockLength -1) / Arbiter->SectorsPerOosBmpBit - 
								bitOffset + 1);
		
		DebugTrace( DBG_LURN_TRACE, ("Setting LWR bit %x:%x\n", bitOffset, numberOfBit) );
		
		RtlSetBits( &Arbiter->LwrBmpHeader, bitOffset, numberOfBit );
		
		lockCount = 1;
	
	} else {
		
		// Recalc all lock
		
		RtlClearAllBits(&Arbiter->LwrBmpHeader);

		for (listEntry = Arbiter->AcquiredLockList.Flink;
			listEntry != &Arbiter->AcquiredLockList;
			listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD(listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);

			bitOffset = (UINT32)(lock->LockAddress / Arbiter->SectorsPerOosBmpBit);
			numberOfBit = (UINT32)((lock->LockAddress + lock->LockLength - 1) / Arbiter->SectorsPerOosBmpBit - bitOffset + 1);

			// DebugTrace(DBG_LURN_TRACE, ("Setting bit %x:%x\n", BitOffset, NumberOfBit));
			
			RtlSetBits(&Arbiter->LwrBmpHeader, bitOffset, numberOfBit );
			
			lockCount++;
		}
	}

	DebugTrace( DBG_LURN_TRACE, ("Updated LWR bitmap with %d locks\n", lockCount) );		
}

//
// Should be called with Arbiter->Spinlock locked.
//

VOID
DraidArbiterChangeOosBitmapBit (
	PDRAID_ARBITER_INFO	Arbiter,
	BOOLEAN				Set,	// TRUE for set, FALSE for clear
	UINT64				Addr,
	UINT64				Length
	) 
{
	UINT32 bitOffset;
	UINT32 numberOfBit;

	ASSERT( KeGetCurrentIrql() ==  DISPATCH_LEVEL ); // should be called with spinlock locked.
	
	bitOffset	= (UINT32)(Addr / Arbiter->SectorsPerOosBmpBit);
	numberOfBit = (UINT32)((Addr + Length -1) / Arbiter->SectorsPerOosBmpBit - bitOffset + 1);


//	DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Before BitmapByte[0]=%x\n", Arbiter->OosBmpInCoreBuffer[0]));	
	
	if (Set) {
	
		DebugTrace( DBG_LURN_TRACE, ("Setting in-memory bitmap offset %x:%x\n", bitOffset, numberOfBit) );
		
		RtlSetBits( &Arbiter->OosBmpHeader, bitOffset, numberOfBit );

	} else {

		DebugTrace( DBG_LURN_TRACE, ("Clearing in-memory bitmap offset %x:%x\n", bitOffset, numberOfBit) );
		
		RtlClearBits( &Arbiter->OosBmpHeader, bitOffset, numberOfBit );
	}
}


//
// Handle completed rebuild IO. 
//

VOID DraidRebuildIoAcknowledge (
	PDRAID_ARBITER_INFO Arbiter
	) 
{
	KIRQL	oldIrql;
	UINT32	rebuildStatus; 
	BOOLEAN bmpChanged = FALSE;


	ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );

	rebuildStatus = Arbiter->RebuildInfo.Status;
	
	if (rebuildStatus == DRAID_REBUILD_STATUS_DONE) {
	
		// Unset bitmap.
	
		DebugTrace( DBG_LURN_TRACE, 
					("Rebuilding range %I64x:%x is done\n", Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length) );
		
		DraidArbiterChangeOosBitmapBit( Arbiter, FALSE, Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length );
		bmpChanged = TRUE;

	} else if (rebuildStatus == DRAID_REBUILD_STATUS_FAILED) {
		
		// nothing to update
		DebugTrace( DBG_LURN_TRACE, 
					("Rebuilding range %I64x:%x is failed\n", Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length) );

	} else if (rebuildStatus == DRAID_REBUILD_STATUS_CANCELLED) {
		
		Arbiter->RebuildInfo.CancelRequested = FALSE;
		
		DebugTrace( DBG_LURN_TRACE, 
					("Rebuilding range %I64x:%x is canceled\n", Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length) );
	
	} else {
	
		DebugTrace(DBG_LURN_TRACE, ("Rebuilding is not in progess. Nothing to acknowledge\n"));

		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

		return;
	}

	Arbiter->RebuildInfo.Status = DRAID_REBUILD_STATUS_NONE;

	if (Arbiter->RebuildInfo.RebuildLock) {

		// Remove from acquired lock list.

		RemoveEntryList( &Arbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink );
		RemoveEntryList( &Arbiter->RebuildInfo.RebuildLock->ToYieldLink );
		InitializeListHead( &Arbiter->RebuildInfo.RebuildLock->ToYieldLink );
		InterlockedDecrement( &Arbiter->AcquiredLockCount );

		DraidArbiterFreeLock( Arbiter, Arbiter->RebuildInfo.RebuildLock );

		Arbiter->RebuildInfo.RebuildLock = NULL;
	}
	
	Arbiter->RebuildInfo.AggressiveRebuildMode = FALSE;

	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

	DraidArbiterUpdateLwrBitmapBit( Arbiter, NULL, NULL );
	DraidArbiterUpdateOnDiskOosBitmap( Arbiter, FALSE );
} 


NTSTATUS 
DraidArbiterGrantLockIfPossible (
	PDRAID_ARBITER_INFO Arbiter
	) 
{
	NTSTATUS					status = STATUS_SUCCESS;

	UINT64						overlapStart, overlapEnd;
	BOOLEAN						rangeAvailable;
	PLIST_ENTRY					acquiredListEntry;
	PLIST_ENTRY					pendingListEntry;
	KIRQL						oldIrql;
	PDRAID_ARBITER_LOCK_CONTEXT lock;
	PDRAID_ARBITER_LOCK_CONTEXT pendingLock;
	

	ACQUIRE_SPIN_LOCK( &Arbiter->SpinLock, &oldIrql );
	
	// For each pending locks.

	for (pendingListEntry = Arbiter->PendingLockList.Flink;
		 pendingListEntry != &Arbiter->PendingLockList;
		 pendingListEntry = pendingListEntry ->Flink) {

		pendingLock = CONTAINING_RECORD(pendingListEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
		
		rangeAvailable = TRUE;

		// Check lock list if lock is held by other client
		
		for (acquiredListEntry = Arbiter->AcquiredLockList.Flink;
			 acquiredListEntry != &Arbiter->AcquiredLockList;
			 acquiredListEntry = acquiredListEntry->Flink) {

			lock = CONTAINING_RECORD( acquiredListEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink );
			
			acquiredListEntry = acquiredListEntry->Flink;

			if (DraidGetOverlappedRange(pendingLock->LockAddress, 
										pendingLock->LockLength, 
										lock->LockAddress, 
										lock->LockLength, 
										&overlapStart, &overlapEnd) != DRAID_RANGE_NO_OVERLAP) {

				DebugTrace( DBG_LURN_INFO, 
							("Pending lock %I64x(%I64x:%x) overlaps with lock %I64x(%I64x:%x)\n",
							 pendingLock->LockId, pendingLock->LockAddress, pendingLock->LockLength,
							 lock->LockId, lock->LockAddress, lock->LockLength) );
				
				if (IsListEmpty(&lock->ToYieldLink)) {
				
					InsertTailList( &Arbiter->ToYieldLockList, &lock->ToYieldLink );
				}

				rangeAvailable = FALSE;
				break;
			}
		}

		if (rangeAvailable) {
		
			DebugTrace( DBG_LURN_INFO, ("Granting pending Lock %I64x\n", pendingLock->LockId) );
			
			lock = pendingLock;

			RemoveEntryList( &lock->PendingLink );
			
			ASSERT( lock->Owner );

			// Add to arbiter list
			
			ASSERT( lock->Status == DRAID_ARBITER_LOCK_STATUS_PENDING );
			
			lock->Status = DRAID_ARBITER_LOCK_STATUS_GRANTED;

			InsertTailList( &Arbiter->AcquiredLockList, &lock->ArbiterAcquiredLink );
			InterlockedIncrement( &Arbiter->AcquiredLockCount );

			// DebugTrace(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n", Arbiter->AcquiredLockCount));

			DraidArbiterUpdateLwrBitmapBit( Arbiter, lock, NULL );

			InsertTailList( &lock->Owner->AcquiredLockList, &lock->ClientAcquiredLink );
			
			if (Arbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
			
				DebugTrace( DBG_LURN_INFO, 
							("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",
							 lock->LockAddress, lock->LockLength) );

				DraidArbiterChangeOosBitmapBit( Arbiter, TRUE, lock->LockAddress, lock->LockLength );
			}

			RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );

			// Need to update BMP and LWR before client start writing using this lock
		
			status = DraidArbiterUpdateOnDiskOosBitmap( Arbiter, FALSE );

			if (!NT_SUCCESS(status)) {

				DebugTrace(DBG_LURN_ERROR, ("Failed to update bitmap\n"));
				break;
			}

			status = DraidArbiterNotify(Arbiter, lock->Owner, DRIX_CMD_GRANT_LOCK, (UINT64)lock, 0, 0 );
			
			ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);

			if (!NT_SUCCESS(status)) {

				status = STATUS_SUCCESS;

				DebugTrace( DBG_LURN_ERROR, ("Failed to notify.\n") );
				NDASSCSI_ASSERT( FALSE );
			}
		} 
	}

	RELEASE_SPIN_LOCK( &Arbiter->SpinLock, oldIrql );
	
	return status;
}
