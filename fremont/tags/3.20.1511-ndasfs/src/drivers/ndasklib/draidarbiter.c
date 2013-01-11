#include <ntddk.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "draid.h"
#include "lslurnassoc.h"
#include "Scrc32.h"
#include "draidexp.h"

#include "cipher.h"
#include "lslurnide.h"
#include "draid.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DRaidArbiter"

//
// DRAID Aribiter: manager for DRAID operation:
//	Manage LWR and out-of-sync bitmap.
//	Initiate recovery process.
//	Merge and relay RAID/LURN status update from client.
//	Update on-disk RMD/LWR/Bitmap
//

//
// Event that arbiter should wait
//		1. stop event
//		2. Client's connection request
//		3. Request from each client.
//		4. Various Timeout(for IO throttling, etc)
//		5. send completion - Currently we will not wait for simplicity. Send in synchronized mode with short timeout value.
//		6. reply packet - Currently we will not wait for simplicity. Wait for reply.
//

//
// Local functions 
//
BOOLEAN
DraidArbiterRefreshRaidStatus(
	PDRAID_ARBITER_INFO		pArbiter	,
	BOOLEAN				ForceChange // Test RAID status change when node is not changed
	);

VOID
DraidArbiterChangeOosBitmapBit(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN		Set,	// TRUE for set, FALSE for clear
	UINT64	Addr,
	UINT64	Length);

NTSTATUS 
DraidArbiterUpdateOnDiskOosBitmap(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN UpdateAll
);

NTSTATUS
DraidRebuildIoStart(
	PDRAID_ARBITER_INFO pArbiter
);

NTSTATUS
DraidRebuildIoStop(
	PDRAID_ARBITER_INFO pArbiter
);

NTSTATUS
DraidRebuildIoCancel(
	PDRAID_ARBITER_INFO Arbiter
);

NTSTATUS DraidRebuilldIoInitiate(
	PDRAID_ARBITER_INFO Arbiter
);

VOID DraidRebuildIoAcknowledge(
	PDRAID_ARBITER_INFO Arbiter
);

VOID
DraidArbiterTerminateClient(
	PDRAID_ARBITER_INFO		Arbiter,
	PDRAID_CLIENT_CONTEXT Client,
	BOOLEAN				CleanExit
);

VOID
DraidArbiterUpdateLwrBitmapBit(
	PDRAID_ARBITER_INFO pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT HintAddedLock,
	PDRAID_ARBITER_LOCK_CONTEXT HintRemovedLock
);


LONG g_ArbiterLockCount = 0;

PDRAID_ARBITER_LOCK_CONTEXT
DraidArbiterAllocLock(
	PDRAID_ARBITER_INFO pArbiter,
	UCHAR		LockType,
	UCHAR 	LockMode,
	UINT64				Addr,
	UINT32				Length
) {
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	KIRQL	oldIrql;
	Lock = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_ARBITER_LOCK_CONTEXT), DRAID_ARBITER_LOCK_POOL_TAG);
	if (Lock == NULL) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to alloc arbiter lock\n"));
		return NULL;
	}
	RtlZeroMemory(Lock, sizeof(DRAID_ARBITER_LOCK_CONTEXT));

	Lock->Status = DRAID_ARBITER_LOCK_STATUS_NONE;
	Lock->LockType = LockType;
	Lock->LockMode = LockMode;
	Lock->LockAddress = Addr;	
	Lock->LockLength = Length;
	Lock->LockGranularity = pArbiter->LockRangeGranularity;
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	Lock->LockId = pArbiter->NextLockId;
	pArbiter->NextLockId++;	
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);		

	InitializeListHead(&Lock->ClientAcquiredLink);
	InitializeListHead(&Lock->ArbiterAcquiredLink);
	InitializeListHead(&Lock->ToYieldLink);
	InitializeListHead(&Lock->PendingLink);
	Lock->Owner = NULL;
	InterlockedIncrement(&g_ArbiterLockCount);
	return Lock;
}

VOID
DraidArbiterFreeLock(
	PDRAID_ARBITER_INFO pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT Lock
) {
	UNREFERENCED_PARAMETER(pArbiter);
	ASSERT(IsListEmpty(&Lock->ToYieldLink));
	ExFreePoolWithTag(Lock, DRAID_ARBITER_LOCK_POOL_TAG);
	InterlockedDecrement(&g_ArbiterLockCount);	
}


//
// Modify lock range to fit into lock range granularity
// Called with pArbiter->Spinlock
//
// Return unsuccessful if CheckOverlap is TRUE and cannot find available lock
//
NTSTATUS
DraidArbiterArrangeLockRange(
	PDRAID_ARBITER_INFO		pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT Lock,
	UINT32 Granularity,
	BOOLEAN CheckOverlap
) {
	UINT64 StartAddr; // inclusive
	UINT64 EndAddr;	// exclusive
	UINT32 Length;
	UINT64 OverlapStart, OverlapEnd;
	PDRAID_ARBITER_LOCK_CONTEXT AckLock;
	BOOLEAN Ok;
	PLIST_ENTRY listEntry;
	ULONG OverlapStatus;
	
	if (CheckOverlap) {
		StartAddr = (Lock->LockAddress / Granularity) * Granularity;
		EndAddr = ((Lock->LockAddress + Lock->LockLength-1)/Granularity + 1) * Granularity;
recalc:
		Length = (UINT32)(EndAddr - StartAddr);
		Ok = TRUE;
		
		for (listEntry = pArbiter->AcquiredLockList.Flink;
			listEntry != &pArbiter->AcquiredLockList;
			listEntry = listEntry->Flink) 
		{
			AckLock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
			OverlapStatus = DraidGetOverlappedRange(Lock->LockAddress, Lock->LockLength,
				AckLock->LockAddress, AckLock->LockLength, 
				&OverlapStart, &OverlapEnd);
			if (OverlapStatus != DRAID_RANGE_NO_OVERLAP) {
				KDPrintM(DBG_LURN_INFO, ("Lock %I64x:%x ovelaps with lock %I64x:%x. Cannot arrange range\n", 
					Lock->LockAddress, Lock->LockLength,
					AckLock->LockAddress, AckLock->LockLength
				));
				Ok = FALSE;
				break;
			}
			OverlapStatus = DraidGetOverlappedRange(StartAddr, Length,
				AckLock->LockAddress, AckLock->LockLength, 
				&OverlapStart, &OverlapEnd);
			if (OverlapStatus != DRAID_RANGE_NO_OVERLAP) {
				if (OverlapStatus == DRAID_RANGE_SRC1_HEAD_OVERLAP) {
					StartAddr =OverlapEnd+1;
					ASSERT(StartAddr<=Lock->LockAddress);			
					goto recalc;
				} else if (OverlapStatus == DRAID_RANGE_SRC1_TAIL_OVERLAP) {
					EndAddr = OverlapStart;
					ASSERT(EndAddr>=Lock->LockAddress+Lock->LockLength);
					goto recalc;
				} else if (OverlapStatus == DRAID_RANGE_SRC1_CONTAINS_SRC2) {
					if (AckLock->LockAddress + AckLock->LockLength -1 < Lock->LockAddress) {
						StartAddr=AckLock->LockAddress +AckLock->LockLength;
					} else {
						EndAddr = AckLock->LockAddress;
					}
					ASSERT(StartAddr<=Lock->LockAddress);
					ASSERT(EndAddr>=Lock->LockAddress+Lock->LockLength);
					goto recalc;
				} else if (OverlapStatus == DRAID_RANGE_SRC2_CONTAINS_SRC1) {
					KDPrintM(DBG_LURN_INFO, ("Lock %I64x:%x ovelaps with lock %I64x:%x. Cannot arrange range\n", 
						Lock->LockAddress, Lock->LockLength,
						AckLock->LockAddress, AckLock->LockLength
					));
					Ok = FALSE;
					break;					
				}		
			}
		}
		ASSERT(StartAddr<=Lock->LockAddress);
		ASSERT(EndAddr>=Lock->LockAddress+Lock->LockLength);
	} else {
		if (Granularity < DRAID_LOCK_GRANULARITY_LOW_THRES) {
			Granularity = 1; // Too low Granularity. Go to exact match.
		}
 		// Use default Granularity without checking overlap with acquired lock
		StartAddr = (Lock->LockAddress /  Granularity) * Granularity;
		EndAddr = ((Lock->LockAddress + Lock->LockLength-1)/Granularity + 1) * Granularity;
		Length = (UINT32)(EndAddr - StartAddr);
		Ok = TRUE;		
	}
 
	if (Ok) {
		if (Lock->LockLength != EndAddr-StartAddr) {
			KDPrintM(DBG_LURN_TRACE, ("Expanding lock range %I64x:%x to %I64x:%x\n",
				Lock->LockAddress, Lock->LockLength,
				StartAddr, EndAddr-StartAddr
				));
		}
		Lock->LockAddress= StartAddr;
		Lock->LockLength = (UINT32)(EndAddr - StartAddr);
		return STATUS_SUCCESS;
	} else {
		return STATUS_UNSUCCESSFUL;
	}
}

//
// Flush range with given lock ID and client.
// Use verify command for 1.0 chips instead of SCSIOP_SYNCHRONIZE_CACHE
// 		because 1.0 chip does not accept cache flush ide command.
// We run it here because LURN IDE does not know which range is dirty.
//
static 
NTSTATUS
DraidArbiterFlushDirtyCacheNdas1_0(
	IN PDRAID_ARBITER_INFO	pArbiter,
	UINT64 LockId,
	PDRAID_CLIENT_CONTEXT pClient
) {
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	ULONG AcquiredLockCount;
	ULONG lockIte;
	NTSTATUS status;
	struct _LockAddrLenPair {
		UINT64 LockAddress;	// in sector
		UINT32 LockLength;
	} *pLockAddrLenPair;
	PLURELATION_NODE Lurn;
	PLURNEXT_IDE_DEVICE IdeDisk;
	UINT32 i;
	
	status = STATUS_SUCCESS;
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++)
	{
		if ((pArbiter->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE) || !(pArbiter->NodeFlags[i] & DRIX_NODE_FLAG_RUNNING)) {
			continue;
		}

		if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
			continue;
		}

		IdeDisk = (PLURNEXT_IDE_DEVICE)pArbiter->Lurn->LurnChildren[i]->LurnExtension;
		if (IdeDisk->LuHwData.HwVersion != LANSCSIIDE_VERSION_1_0) {
			continue;
		}
		ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);	
		pLockAddrLenPair = ExAllocatePoolWithTag(NonPagedPool, 
			pArbiter->AcquiredLockCount * sizeof(struct _LockAddrLenPair), 
			DRAID_ARBITER_LOCK_ADDRLEN_PAIR_POOL_TAG
		);
		AcquiredLockCount = 0;
		for (listEntry = pArbiter->AcquiredLockList.Flink;
			listEntry != &pArbiter->AcquiredLockList;
			listEntry = listEntry->Flink)
		{
			if (AcquiredLockCount >= (ULONG)pArbiter->AcquiredLockCount) {
				ASSERT(FALSE);
				break;
			}
			Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
			if (Lock->Owner == pClient &&
				(Lock->LockId == LockId || Lock->LockId == DRIX_LOCK_ID_ALL)) {
				pLockAddrLenPair[AcquiredLockCount].LockAddress = Lock->LockAddress;
				pLockAddrLenPair[AcquiredLockCount].LockLength = Lock->LockLength;
				AcquiredLockCount++;
			}
		}				
		RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);		

		status = STATUS_SUCCESS;
		Lurn = pArbiter->Lurn->LurnChildren[i];
		for(lockIte = 0; lockIte<AcquiredLockCount;lockIte++) {
			UINT64 LockAddr;
			UINT32 LockLength;
			static const UINT32 MaxVerifyLength = 0x8000; // Limit due to CDB's Length bit size.
			for(LockAddr = pLockAddrLenPair[lockIte].LockAddress; 
				LockAddr < pLockAddrLenPair[lockIte].LockAddress + pLockAddrLenPair[lockIte].LockLength;) 
			{
				if (LockAddr + MaxVerifyLength  > pLockAddrLenPair[lockIte].LockAddress + pLockAddrLenPair[lockIte].LockLength) {
					LockLength = (UINT32)(pLockAddrLenPair[lockIte].LockAddress + pLockAddrLenPair[lockIte].LockLength - LockAddr);
				} else {
					LockLength = MaxVerifyLength;
				}
				KDPrintM(DBG_LURN_INFO, ("Flushing 1.0 HW range %I64x:%x.\n", 
					LockAddr, LockLength
					));
				status = LurnExecuteSyncMulti(
					1,	&Lurn,	SCSIOP_VERIFY16,
					NULL,	LockAddr,(UINT16)LockLength,	NULL);
				if(!NT_SUCCESS(status))
					break;
				LockAddr +=LockLength;
			}
			if(!NT_SUCCESS(status))
				break;
		}
		ExFreePoolWithTag(pLockAddrLenPair, DRAID_ARBITER_LOCK_ADDRLEN_PAIR_POOL_TAG);			
		if(!NT_SUCCESS(status))
			break;
	}

	return status;
}

//
// Write meta data to all non-defective running disk with flush
//
NTSTATUS
DraidArbiterWriteMetaSync(
	IN PDRAID_ARBITER_INFO	pArbiter,
	IN PUCHAR	BlockBuffer,
	IN INT64		Addr,
	IN UINT32	Length 	// in sector
) {
	NTSTATUS status = STATUS_SUCCESS;
	ULONG i;
	UCHAR NodeFlags;
	BOOLEAN NodeFlagMismatch[MAX_DRAID_MEMBER_DISK] = {0};	// Report Lurn status and node flag is not matched.
	BOOLEAN NodeUpdateHandled[MAX_DRAID_MEMBER_DISK] = {0}; // This node's metadata update is handled whether it is succeed or not
	BOOLEAN UpdateSucceeded;
	BOOLEAN LurnStatusMismatch;
	PLURNEXT_IDE_DEVICE		IdeDisk;	
		
	if(!(GENERIC_WRITE & pArbiter->Lurn->AccessRight))
	{
		// Only primary can call this
		ASSERT(FALSE);
		return STATUS_SUCCESS;
	}

	//
	// Flush all disk before updating metadata because updated metadata information is valid under assumption that all written data is on disk.
	//
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++)
	{
		NodeFlags = pArbiter->NodeFlags[i];	
		if ((NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(NodeFlags & DRIX_NODE_FLAG_RUNNING)) {
			KDPrintM(DBG_LURN_INFO, ("Node %d flag: %x, defect: %x. Skipping metadata update\n", i, NodeFlags, pArbiter->DefectCodes[i]));
			continue;
		}

		if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
			NodeFlagMismatch[i] = TRUE;
			continue;
		}

		IdeDisk = (PLURNEXT_IDE_DEVICE)pArbiter->Lurn->LurnChildren[i]->LurnExtension;
		if (IdeDisk && IdeDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_1_0) {
			// We already flushed when releasing lock.
			status =STATUS_SUCCESS;
		} else {	
			status = LurnExecuteSyncMulti(
				1,
				&pArbiter->Lurn->LurnChildren[i],
				SCSIOP_SYNCHRONIZE_CACHE,
				NULL,
				0,
				0,
				NULL);
		}
		
		if(!NT_SUCCESS(status))
		{
			KDPrintM(DBG_LURN_INFO, ("Failed to flush node %d.\n", i));
			continue;			
		}
	}

	//
	// Write non-out-of-sync disk and non spare disk first. 
	// Because only in-sync disk is gauranteed to have up-to-date OOS bitmap
	//	because bitmap is not written to disk always(only dirty map is written)
	// We should always keep in-sync disk has most largest USN number to rule out OOS disk when finding up-to-date RMD.
	// If we fail to update no in-sync-rmd, we fail whole metadata writing.
	//
	UpdateSucceeded = FALSE;
	for(i = 0; i < pArbiter->ActiveDiskCount; i++) { // i is role index.
		ULONG NodeIdx = pArbiter->RoleToNodeMap[i];
		if (i!=pArbiter->OutOfSyncRole) {
			NodeUpdateHandled[NodeIdx] = TRUE;
			NodeFlags = pArbiter->NodeFlags[NodeIdx];
			if ((NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(NodeFlags & DRIX_NODE_FLAG_RUNNING)) {
				continue;
			}

			if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
				NodeFlagMismatch[NodeIdx] = TRUE;
				continue;
			}
			
			//LurnExecuteSyncWrite do flush.
			status = LurnExecuteSyncWrite(pArbiter->Lurn->LurnChildren[NodeIdx], BlockBuffer,
				Addr, Length);

			if(!NT_SUCCESS(status))
			{
				KDPrintM(DBG_LURN_INFO, ("Failed to flush node %d.\n", i));
				continue;			
			}
			UpdateSucceeded = TRUE;
		}
	}
	if (!UpdateSucceeded) {
		KDPrintM(DBG_LURN_INFO, ("Failed to update any in-sync disk\n"));
		goto out;
	}
		
	// Now update other nodes
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++)
	{
		if (NodeUpdateHandled[i])
			continue;
		NodeFlags = pArbiter->NodeFlags[i];	
		if ((NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(NodeFlags & DRIX_NODE_FLAG_RUNNING)) {
//			KDPrintM(DBG_LURN_INFO, ("Node %d is not good for RMD writing. Skipping\n", i));			
			continue;
		}

		if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
			NodeFlagMismatch[i] = TRUE;
//			KDPrintM(DBG_LURN_INFO, ("Normal node %d is not running. Handling this case is not yet implemented\n", i));
			continue;
		}
		//LurnExecuteSyncWrite do flush.
		status = LurnExecuteSyncWrite(pArbiter->Lurn->LurnChildren[i], BlockBuffer,
			Addr, Length);

		if(!NT_SUCCESS(status))
		{
			KDPrintM(DBG_LURN_INFO, ("Failed to flush node %d.\n", i));
			continue;			
		}
	}

out:
	if (UpdateSucceeded) {
		status = STATUS_SUCCESS;
	} else {
		KDPrintM(DBG_LURN_INFO, ("Failed to update metadata to any node\n"));
		status = STATUS_UNSUCCESSFUL;
	}

	//
	// Report lurn status if any lurn status and arbiter node flag is not matched.
	//
	LurnStatusMismatch = FALSE;
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++) {
		if (NodeFlagMismatch[i]) {
			LurnStatusMismatch = TRUE;
			break;
		}
	}
	if (LurnStatusMismatch) {
		KDPrintM(DBG_LURN_INFO, ("LURN status is different from node flag. Requesting to update local client.\n"));
		if (pArbiter->LocalClient && pArbiter->LocalClient->LocalClient) {
			DraidClientUpdateAllNodeFlags(pArbiter->LocalClient->LocalClient);
		} else {
			KDPrintM(DBG_LURN_INFO, ("No local client to notify.\n"));
		}
	}
	return status;
}

NTSTATUS
DraidArbiterWriteRmd(
			IN PDRAID_ARBITER_INFO	pArbiter,
			OUT PNDAS_RAID_META_DATA rmd)
{
	NTSTATUS status;
	
	KDPrintM(DBG_LURN_INFO, ("IN\n"));

	status = DraidArbiterWriteMetaSync(pArbiter, (PUCHAR) rmd, NDAS_BLOCK_LOCATION_RMD_T, 1);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("Failed update second RMD\n"));		
		return status;
	}
		
	status = DraidArbiterWriteMetaSync(pArbiter, (PUCHAR) rmd, NDAS_BLOCK_LOCATION_RMD, 1);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("Failed update first RMD\n"));		
		return status;
	}
	KDPrintM(DBG_LURN_INFO, ("OUT\n"));

	return status;
}

//
// Set Arbiter->RMD fields based on current status.
// Return TRUE if InCore RMD has changed

BOOLEAN
DraidArbiterUpdateInCoreRmd(
	IN PDRAID_ARBITER_INFO pArbiter,
	IN BOOLEAN ForceUpdate
) {
	NDAS_RAID_META_DATA NewRmd = {0};
	UINT32	i, j;
	UCHAR NodeFlags;
	NewRmd.Signature = pArbiter->Rmd.Signature;
	NewRmd.RaidSetId = pArbiter->RaidSetId;
	NewRmd.uiUSN = pArbiter->Rmd.uiUSN;
	NewRmd.ConfigSetId = pArbiter->ConfigSetId;
		
	if (DRAID_ARBITER_STATUS_TERMINATING == pArbiter->Status) {
		NewRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;
	} else {
		NewRmd.state = NDAS_RAID_META_DATA_STATE_MOUNTED;
	}

	// Keep NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED.
	switch(pArbiter->RaidStatus) {
	case DRIX_RAID_STATUS_DEGRADED :
		if (!(pArbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {
			KDPrintM(DBG_LURN_INFO, ("Marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n"));
		}
		NewRmd.state |= NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;		
		break;
	case DRIX_RAID_STATUS_NORMAL:
	case DRIX_RAID_STATUS_REBUILDING:
		if (pArbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
			KDPrintM(DBG_LURN_INFO, ("Clearing NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n"));			
		}
		NewRmd.state &= ~NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;
		break;
	default: // Keep previous flag
		NewRmd.state |= (pArbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED);
		if (pArbiter->Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
			KDPrintM(DBG_LURN_INFO, ("Keep marking NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED flag\n"));
		}
		break;
	}

	if (NewRmd.state & NDAS_RAID_META_DATA_STATE_MOUNTED) {
		UINT32 UsedAddrCount;
		PUCHAR	MacAddr;
		BOOLEAN IsNewAddr;
		PLURNEXT_IDE_DEVICE		IdeDisk;
		
		RtlZeroMemory(NewRmd.ArbiterInfo, sizeof(NewRmd.ArbiterInfo));
		ASSERT(sizeof(NewRmd.ArbiterInfo) == 12 * 8);
		//
		// Get list of bind address from each children.
		//
		UsedAddrCount = 0;
		for (i=0;i<pArbiter->TotalDiskCount;i++) {
			//
			// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
			//
			if (!pArbiter->Lurn->LurnChildren[i])
				continue;
			if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus)
				continue;
			IdeDisk = (PLURNEXT_IDE_DEVICE)pArbiter->Lurn->LurnChildren[i]->LurnExtension;
			if (!IdeDisk)
				continue;

			MacAddr = ((PTDI_ADDRESS_LPX)&IdeDisk->LanScsiSession.BindAddress.Address[0].Address)->Node;
				
			IsNewAddr = TRUE;
			// Search address is already recorded.
			for(j=0;j<UsedAddrCount;j++) {
				if (RtlCompareMemory(NewRmd.ArbiterInfo[j].Addr, MacAddr,6) == 6) {
					// This bind address is alreay in entry. Skip
					IsNewAddr = FALSE;
					break;
				}
			}
			if (IsNewAddr) {
				NewRmd.ArbiterInfo[UsedAddrCount].Type = NDAS_DRAID_ARBITER_TYPE_LPX;
				RtlCopyMemory(NewRmd.ArbiterInfo[UsedAddrCount].Addr, MacAddr, 6);
				UsedAddrCount++;
			}
			if (UsedAddrCount >= NDAS_DRAID_ARBITER_ADDR_COUNT) {
				break;
			}
		}
//		ASSERT(UsedAddrCount >0);
	} else {
		RtlZeroMemory(NewRmd.ArbiterInfo, sizeof(NewRmd.ArbiterInfo));
		for(i=0;i<NDAS_DRAID_ARBITER_ADDR_COUNT;i++) {
			NewRmd.ArbiterInfo[i].Type = NDAS_DRAID_ARBITER_TYPE_NONE;
		}
	}
	
	for(i=0;i<pArbiter->TotalDiskCount;i++) { // i is role index.
		NewRmd.UnitMetaData[i].iUnitDeviceIdx = pArbiter->RoleToNodeMap[i];
		// NodeFlags is flag of role i's node flag.(Not ith node)
		NodeFlags = pArbiter->NodeFlags[NewRmd.UnitMetaData[i].iUnitDeviceIdx];
		if (NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) {
			UCHAR DefectCode = pArbiter->DefectCodes[NewRmd.UnitMetaData[i].iUnitDeviceIdx];
			// Relative defect code such as RMD mismatch is not needed to be written to disk
			// Record physical defects and spare-used flag only
			NewRmd.UnitMetaData[i].UnitDeviceStatus |= DraidNodeDefectCodeToRmdUnitStatus(DefectCode);
		}
		
		if (i == pArbiter->OutOfSyncRole) {
			// Only one unit can be out-of-sync.
			NewRmd.UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		}

		if (i >= pArbiter->ActiveDiskCount) {
			NewRmd.UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_SPARE;
		}
	}
	SET_RMD_CRC(crc32_calc, NewRmd);
	if (ForceUpdate ||
		RtlCompareMemory(&NewRmd, &pArbiter->Rmd, sizeof(NDAS_RAID_META_DATA)) != sizeof(NDAS_RAID_META_DATA)) {
		// Changed
		KDPrintM(DBG_LURN_INFO, ("Changing in memory RMD\n"));
		NewRmd.uiUSN++;
		SET_RMD_CRC(crc32_calc, NewRmd);
		RtlCopyMemory(&pArbiter->Rmd, &NewRmd, sizeof(NDAS_RAID_META_DATA));
		return TRUE;
	} else {
		return FALSE;
	}
}

//
// Handle requests sent by DRAID clients. Return error only if error is fatal.
//
NTSTATUS
DraidArbiterHandleRequestMsg(
	PDRAID_ARBITER_INFO pArbiter, 
	PDRAID_CLIENT_CONTEXT pClient,
	PDRIX_HEADER Message
)
{
	NTSTATUS		status;
	PDRIX_HEADER	ReplyMsg;
//	KIRQL	oldIrql;
	UINT32	i;
	UINT32 ReplyLength;
	BOOLEAN Ret;
	PLIST_ENTRY listEntry;
	UCHAR ResultCode = DRIX_RESULT_SUCCESS;
	KIRQL	oldIrql;
	PDRAID_ARBITER_LOCK_CONTEXT NewLock = NULL;
	UINT32 Granularity = pArbiter->LockRangeGranularity;

	//
	// Check data validity.
	//
	if (NTOHL(Message->Signature) != DRIX_SIGNATURE) {
		KDPrintM(DBG_LURN_INFO, ("DRIX signature mismatch\n"));
		ASSERT(FALSE);
		return STATUS_UNSUCCESSFUL;
	}

	if (Message->ReplyFlag != 0) {
		KDPrintM(DBG_LURN_INFO, ("Should not be reply packet\n"));	
		ASSERT(FALSE);
		return STATUS_UNSUCCESSFUL;
	}
	// Get reply packet size
	switch(Message->Command) {
	case DRIX_CMD_ACQUIRE_LOCK:
		ReplyLength = sizeof(DRIX_ACQUIRE_LOCK_REPLY);
		break;
	case DRIX_CMD_NODE_CHANGE:
	case DRIX_CMD_RELEASE_LOCK:
	default:
		ReplyLength = sizeof(DRIX_HEADER);
		break;
	}

	//
	// Create reply
	//
 	ReplyMsg = ExAllocatePoolWithTag(NonPagedPool, ReplyLength, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG);
	if (ReplyMsg==NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(ReplyMsg, ReplyLength);

	ReplyMsg->Signature = HTONL(DRIX_SIGNATURE);
//	ReplyMsg->Version = DRIX_CURRENT_VERSION;
	ReplyMsg->ReplyFlag = TRUE;
	ReplyMsg->Command = Message->Command;
	ReplyMsg->Length = HTONS((UINT16)ReplyLength);
	ReplyMsg->Sequence = Message->Sequence;

	//
	// Process request
	//
	switch(Message->Command) {

	case DRIX_CMD_NODE_CHANGE:
		{
			PDRIX_NODE_CHANGE NcMsg = (PDRIX_NODE_CHANGE)Message;

			KDPrintM(DBG_LURN_INFO, ("Arbiter received node change message\n"));		
			//
			// Update local node information from packet
			//
			for(i=0;i<NcMsg->UpdateCount;i++) {
				pClient->NodeFlags[NcMsg->Node[i].NodeNum] = NcMsg->Node[i].NodeFlags;
				pClient->DefectCode[NcMsg->Node[i].NodeNum] = NcMsg->Node[i].DefectCode;
				KDPrintM(DBG_LURN_INFO, ("   Node %d: Flag %x, Defect %x\n", i, NcMsg->Node[i].NodeFlags, NcMsg->Node[i].DefectCode));
			}
			Ret = DraidArbiterRefreshRaidStatus(pArbiter, FALSE);
			if (Ret == TRUE) {
				KDPrintM(DBG_LURN_INFO, ("RAID/Node status has been changed\n"));
				ResultCode = DRIX_RESULT_REQUIRE_SYNC;
			} else {
				KDPrintM(DBG_LURN_INFO, ("RAID/Node status has not been changed\n"));			
				ResultCode = DRIX_RESULT_NO_CHANGE;
			}
		}
		break;
	case DRIX_CMD_ACQUIRE_LOCK:
		{
			PDRAID_ARBITER_LOCK_CONTEXT Lock;
			PDRIX_ACQUIRE_LOCK AcqMsg = (PDRIX_ACQUIRE_LOCK) Message;
			UINT64 OverlapStart, OverlapEnd;
			UINT64 Addr = NTOHLL(AcqMsg->Addr);
			UINT32 Length = NTOHL(AcqMsg->Length);
			BOOLEAN MatchFound = FALSE;

			KDPrintM(DBG_LURN_TRACE, ("Arbiter received ACQUIRE_LOCK message: %I64x:%x\n", Addr, Length));

			// Check lock list if lock overlaps with lock acquired by other client
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			for (listEntry = pArbiter->AcquiredLockList.Flink;
				listEntry != &pArbiter->AcquiredLockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
				if (DraidGetOverlappedRange(Addr, Length,
					Lock->LockAddress, Lock->LockLength, 
					&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
					MatchFound = TRUE;

					if (Lock == pArbiter->RebuildInfo.RebuildLock) {
						// Rebuilding IO is holding this lock.
						KDPrintM(DBG_LURN_INFO, ("Area is locked for rebuilding.\n"));
						status = DraidRebuildIoCancel(pArbiter);
						if (NT_SUCCESS(status)) {

						} else {
							// In aggressive rebuild mode. Reduce granularity to rebuild size.
							Granularity = DRAID_AGGRESSIVE_REBUILD_SIZE;
						}
					} else {
						KDPrintM(DBG_LURN_INFO, ("Range is locked by another client. Return exact match\n"));
						Granularity = 1;
						// Client has lock. Queue this lock to to-yield-list. This will be handled by Arbiter thread later.
						if (IsListEmpty(&Lock->ToYieldLink))
							InsertTailList(&pArbiter->ToYieldLockList, &Lock->ToYieldLink);
						// Continue: multiple lock may be overlapped.
					}						
				}
			}

			if (MatchFound == FALSE) {
				//
				// Check whether another client is already waiting for this range.
				//
				for (listEntry = pArbiter->PendingLockList.Flink;
					listEntry != &pArbiter->PendingLockList;
					listEntry = listEntry->Flink)  {
					Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
					if (DraidGetOverlappedRange(Addr, Length,
						Lock->LockAddress, Lock->LockLength, 
						&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
						KDPrintM(DBG_LURN_INFO, ("Another client is waiting for range %I64x:%x. Return PENDING\n", Lock->LockAddress, Lock->LockLength));
						MatchFound = TRUE;
						Granularity = 1;
						break;
					}
				}
			}
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

			NewLock = DraidArbiterAllocLock(pArbiter, AcqMsg->LockType, AcqMsg->LockMode,
					Addr, Length);
			if (NewLock==NULL) {
				ResultCode = DRIX_RESULT_FAIL;
			} else if (MatchFound) {
				//
				// Lock is pending.
				//
				ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
				NewLock->Owner = pClient;
				// Arrange lock with reduced range.
				DraidArbiterArrangeLockRange(pArbiter, NewLock, Granularity, FALSE);
				// Add to pending list.
				NewLock->Status = DRAID_ARBITER_LOCK_STATUS_PENDING;
				NewLock->Owner = pClient;
				InsertTailList(&pArbiter->PendingLockList, &NewLock->PendingLink);
				ResultCode = DRIX_RESULT_PENDING;
				KDPrintM(DBG_LURN_INFO, ("Lock %I64x(%I64x:%Ix) is pended.\n",
					NewLock->LockId, NewLock->LockAddress, NewLock->LockLength));
				RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			} else {
				//
				// Lock is granted
				//
				ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
				ResultCode = DRIX_RESULT_GRANTED;
				NewLock->Owner = pClient;
				NewLock->Status = DRAID_ARBITER_LOCK_STATUS_GRANTED;
				// Adjust addr/length to possible larger size.
				status = DraidArbiterArrangeLockRange(pArbiter, NewLock, pArbiter->LockRangeGranularity, TRUE);
				ASSERT(status == STATUS_SUCCESS);	// We should get success
				// Add to arbiter list
				InsertTailList(&pArbiter->AcquiredLockList, &NewLock->ArbiterAcquiredLink);
				InterlockedIncrement(&pArbiter->AcquiredLockCount);
				
				InsertTailList(&pClient->AcquiredLockList, &NewLock->ClientAcquiredLink);
				DraidArbiterUpdateLwrBitmapBit(pArbiter, NewLock, NULL);
				if (pArbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
					KDPrintM(DBG_LURN_INFO, ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",
						NewLock->LockAddress, NewLock->LockLength));
					DraidArbiterChangeOosBitmapBit(pArbiter, TRUE, NewLock->LockAddress, NewLock->LockLength);
				} else {
					KDPrintM(DBG_LURN_TRACE, ("Granted lock %I64x(%I64x:%Ix).\n",
						NewLock->LockId, NewLock->LockAddress, NewLock->LockLength));
				}
				RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

				// Need to update BMP and LWR before client start to write using this lock
				status = DraidArbiterUpdateOnDiskOosBitmap(pArbiter, FALSE);
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_ERROR, ("Failed to update bitmap\n"));
				}
			}
			if (NewLock) {
				PDRIX_ACQUIRE_LOCK_REPLY AcqReply = (PDRIX_ACQUIRE_LOCK_REPLY) ReplyMsg;
				AcqReply->LockType = AcqMsg->LockType;
				AcqReply->LockMode = AcqMsg->LockMode;
				AcqReply->LockId = NTOHLL(NewLock->LockId);
				AcqReply->Addr = NTOHLL(NewLock->LockAddress);
				AcqReply->Length = NTOHL(NewLock->LockLength);
			} else {
				PDRIX_ACQUIRE_LOCK_REPLY AcqReply = (PDRIX_ACQUIRE_LOCK_REPLY) ReplyMsg;
				AcqReply->LockType = 0;
				AcqReply->LockMode = 0;
				AcqReply->LockId = 0;
				AcqReply->Addr = 0;
				AcqReply->Length = 0;
			}
		}
		break;
	case DRIX_CMD_RELEASE_LOCK:
		{
			//
			// Check lock is owned by this client.
			//
			PDRAID_ARBITER_LOCK_CONTEXT Lock;
			PDRIX_RELEASE_LOCK RelMsg = (PDRIX_RELEASE_LOCK) Message;
			UINT64 LockId = NTOHLL(RelMsg->LockId);

			KDPrintM(DBG_LURN_INFO, ("Arbiter received RELEASE_LOCK message: %I64x\n", LockId));

			// 1.0 chip does not support cache flush. 
			// Flush before releasing the lock.
			DraidArbiterFlushDirtyCacheNdas1_0(pArbiter, LockId, pClient);
				
			ResultCode = DRIX_RESULT_INVALID_LOCK_ID;
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			// Search for matching Lock ID
			for (listEntry = pArbiter->AcquiredLockList.Flink;
				listEntry != &pArbiter->AcquiredLockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
				if (LockId == DRIX_LOCK_ID_ALL && Lock->Owner == pClient) {
					KDPrintM(DBG_LURN_INFO, ("Releasing all locks - Lock %I64x:%x\n", Lock->LockAddress, Lock->LockLength));
					// Remove from all list
					listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
					RemoveEntryList(&Lock->ArbiterAcquiredLink);
					InterlockedDecrement(&pArbiter->AcquiredLockCount);
					RemoveEntryList(&Lock->ToYieldLink);
					InitializeListHead(&Lock->ToYieldLink); // to check bug...
					RemoveEntryList(&Lock->ClientAcquiredLink);
					ResultCode = DRIX_RESULT_SUCCESS;
					DraidArbiterFreeLock(pArbiter, Lock);
				} else if (Lock->LockId == LockId) {
					if (Lock->Owner != pClient) {
						KDPrintM(DBG_LURN_ERROR, ("Lock ID matched but client didn't match. release lock failed\n"));
						ASSERT(FALSE);
						break;
					} else {
						// Remove from all list
						RemoveEntryList(&Lock->ArbiterAcquiredLink);
						InterlockedDecrement(&pArbiter->AcquiredLockCount);
//						KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",pArbiter->AcquiredLockCount));						
						RemoveEntryList(&Lock->ToYieldLink);
						InitializeListHead(&Lock->ToYieldLink); // to check bug...
						RemoveEntryList(&Lock->ClientAcquiredLink);
						ResultCode = DRIX_RESULT_SUCCESS;
						DraidArbiterFreeLock(pArbiter, Lock);
						break;
					}
				}
			}
				
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			DraidArbiterUpdateLwrBitmapBit(pArbiter, NULL, NULL);
			status = DraidArbiterUpdateOnDiskOosBitmap(pArbiter, FALSE);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Failed to update OOS bitmap\n"));
			}			
		}
		break;
	case DRIX_CMD_REGISTER:
//	case DRIX_CMD_UNREGISTER:
	default:
		KDPrintM(DBG_LURN_INFO, ("Unsupported command\n"));
		ASSERT(FALSE);
		ResultCode = DRIX_RESULT_UNSUPPORTED;
		break;
	}
	ReplyMsg->Result = ResultCode;

	//
	// Send reply
	//
	if (pClient->LocalClient) {
		PDRIX_MSG_CONTEXT ReplyMsgEntry;
		KDPrintM(DBG_LURN_TRACE, ("DRAID Sending reply to request %s with result %s to local client(event=%p)\n", 
			DrixGetCmdString(Message->Command), DrixGetResultString(ResultCode),
			&pClient->RequestReplyChannel->Queue));
		ReplyMsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
		if (ReplyMsgEntry !=NULL) {
			RtlZeroMemory(ReplyMsgEntry, sizeof(DRIX_MSG_CONTEXT));
			InitializeListHead(&ReplyMsgEntry->Link);
			ReplyMsgEntry->Message = ReplyMsg;
			ExInterlockedInsertTailList(&pClient->RequestReplyChannel->Queue, &ReplyMsgEntry->Link, &pClient->RequestReplyChannel->Lock);
			KeSetEvent(&pClient->RequestReplyChannel->Event, IO_NO_INCREMENT, FALSE);
			return STATUS_SUCCESS;
		} else {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	} else {
		LARGE_INTEGER Timeout;
		LONG Result;
		Timeout.QuadPart = 5 * HZ;
		KDPrintM(DBG_LURN_INFO, ("DRAID Sending reply to request %s with result %s to remote client\n", 
			DrixGetCmdString(Message->Command), DrixGetResultString(ResultCode)));
		if (pClient->RequestConnection->ConnectionFileObject) {
			status =	LpxTdiSend(pClient->RequestConnection->ConnectionFileObject, (PUCHAR)ReplyMsg, ReplyLength,
				0, &Timeout, NULL, &Result);
			ASSERT(ReplyMsg->Signature == HTONL(DRIX_SIGNATURE));// Check wheter data is corrupted somehow..
		} else {
			status = STATUS_UNSUCCESSFUL;
		}
		ExFreePoolWithTag(ReplyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
		if (NT_SUCCESS(status)) {
			return STATUS_SUCCESS;
		} else {
			KDPrintM(DBG_LURN_INFO, ("Failed to send request reply\n"));
			return STATUS_UNSUCCESSFUL;
		}		
	}
}


NTSTATUS
DraidArbiterUseSpareIfNeeded(
	PDRAID_ARBITER_INFO pArbiter
) {
	KIRQL oldIrql;
	BOOLEAN SpareUsed = FALSE;
	UINT32 i;
	NTSTATUS status;
	
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	//
	// Check this degraded state is long enough to use spare.
	//
	if (pArbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
		LARGE_INTEGER time_diff;
		LARGE_INTEGER current_time;
		static BOOLEAN spareHoldingTimeoutMsgShown = FALSE;
		KeQueryTickCount(&current_time);
		time_diff.QuadPart = (current_time.QuadPart - pArbiter->DegradedSince.QuadPart) * KeQueryTimeIncrement();
		if (time_diff.QuadPart < NDAS_RAID_SPARE_HOLDING_TIMEOUT) {
			if (!spareHoldingTimeoutMsgShown) {
				KDPrintM(DBG_LURN_INFO, ("Fault device is not alive. %d sec left before using spare\n",
					(ULONG)((NDAS_RAID_SPARE_HOLDING_TIMEOUT-time_diff.QuadPart) / HZ)));
				spareHoldingTimeoutMsgShown = TRUE;
			} 
		} else {
			BOOLEAN SpareFound = FALSE;
			UCHAR SpareNode = 0;

			spareHoldingTimeoutMsgShown = FALSE;
			//
			// Change RAID maps to use spare disk and set all bitmap dirty.
			//

			// Find running spare
			for(i=pArbiter->ActiveDiskCount;i<pArbiter->TotalDiskCount;i++) { // i is indexed by role
				if ((pArbiter->NodeFlags[pArbiter->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING) && 
					!(pArbiter->NodeFlags[pArbiter->RoleToNodeMap[i]] & DRIX_NODE_FLAG_DEFECTIVE)) {
					SpareFound = TRUE;
					SpareNode = pArbiter->RoleToNodeMap[i];
					break;
				}
			}

			if (SpareFound) {
				UCHAR SpareRole, OosNode;
				// Swap defective disk with spare disk
				KDPrintM(DBG_LURN_INFO, ("Running spare disk found. Swapping defective node %d(role %d) with spare %d\n",
					pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole], pArbiter->OutOfSyncRole, SpareNode));
				ASSERT(SpareNode<pArbiter->TotalDiskCount);
				ASSERT((pArbiter->NodeFlags[pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole]] & DRIX_NODE_FLAG_STOP) ||
					(pArbiter->NodeFlags[pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole]] & DRIX_NODE_FLAG_DEFECTIVE));

				OosNode = pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole];
				SpareRole = pArbiter->NodeToRoleMap[SpareNode];

				pArbiter->NodeToRoleMap[OosNode] = SpareRole;
				pArbiter->NodeToRoleMap[SpareNode] = pArbiter->OutOfSyncRole;

				pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole] = SpareNode;
				pArbiter->RoleToNodeMap[SpareRole] = OosNode;
				
				// Replaced node is still out-of-sync. Keep pArbiter->OutOfSyncRole
				pArbiter->NodeFlags[OosNode] |= DRIX_NODE_FLAG_DEFECTIVE;
				pArbiter->DefectCodes[OosNode] |= DRIX_NODE_DEFECT_REPLACED_BY_SPARE;
				
				// Set all bitmap dirty
				DraidArbiterChangeOosBitmapBit(pArbiter, TRUE, 0, pArbiter->Lurn->UnitBlocks);
				SpareUsed = TRUE;
			} else {
				if (pArbiter->ActiveDiskCount < pArbiter->TotalDiskCount) {
					KDPrintM(DBG_LURN_TRACE, ("Spare disk is not running\n"));
				} else {

				}
			}
		}
	}

	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	if (SpareUsed) {
		KDPrintM(DBG_LURN_INFO, ("Creating new config set ID\n"));
		status = ExUuidCreate(&pArbiter->ConfigSetId);
		ASSERT(status == STATUS_SUCCESS);
		
		// Update RAID status. 
		DraidArbiterRefreshRaidStatus(pArbiter, TRUE);
		DraidArbiterUpdateOnDiskOosBitmap(pArbiter, TRUE);
	}
	return STATUS_SUCCESS;
}

//
// Send message to client and wait for reply.
//
NTSTATUS
DraidArbiterNotify(
	PDRAID_ARBITER_INFO pArbiter, 
	PDRAID_CLIENT_CONTEXT Client,
	UCHAR					Command,
	UINT64					CmdParam1,	// CmdParam* are dependent on Command.
	UINT64					CmdParam2,
	UINT32					CmdParam3
	) 
{
	PDRIX_HEADER NotifyMsg;
	UINT32 MsgLength;
	UINT32 i;
	NTSTATUS status;
	KIRQL oldIrql;
	
	KDPrintM(DBG_LURN_INFO, ("Notifying client with command %s\n", DrixGetCmdString(Command)));
	//
	// Create notification message.
	//
	switch(Command) {
	case DRIX_CMD_CHANGE_STATUS:
		MsgLength = SIZE_OF_DRIX_CHANGE_STATUS(pArbiter->TotalDiskCount);
		break;
	case DRIX_CMD_REQ_TO_YIELD_LOCK:
		MsgLength = sizeof(DRIX_REQ_TO_YIELD_LOCK);
		break;
	case DRIX_CMD_GRANT_LOCK:
		MsgLength = sizeof(DRIX_GRANT_LOCK);
		break;
	case DRIX_CMD_RETIRE:
	case DRIX_CMD_STATUS_SYNCED:
	default:
		MsgLength = sizeof(DRIX_HEADER);
		break;
	}

	// For local client, msg will be freed by receiver, for remote client, msg will be freed after sending message.
	NotifyMsg = ExAllocatePoolWithTag(NonPagedPool, MsgLength, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
	if (NotifyMsg==NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(NotifyMsg, MsgLength);

	NotifyMsg->Signature = HTONL(DRIX_SIGNATURE);
//	NotifyMsg->Version = DRIX_CURRENT_VERSION;
	NotifyMsg->Command = Command;
	NotifyMsg->Length = HTONS((UINT16)MsgLength);
	NotifyMsg->Sequence = HTONS(Client->NotifySequence);
	Client->NotifySequence++;

	if (DRIX_CMD_CHANGE_STATUS == Command) {
		PDRIX_CHANGE_STATUS CsMsg = (PDRIX_CHANGE_STATUS) NotifyMsg;
		
		//
		// Set additional info
		//
		// CmdParam1: WaitForSync
		//
		CsMsg->Usn = NTOHL(pArbiter->Rmd.uiUSN);
		CsMsg->RaidStatus = (UCHAR)pArbiter->RaidStatus;
		CsMsg->NodeCount = (UCHAR)pArbiter->TotalDiskCount;
		RtlCopyMemory(&CsMsg->ConfigSetId, &pArbiter->ConfigSetId, sizeof(CsMsg->ConfigSetId));
		if (CmdParam1)  {
			CsMsg->WaitForSync = (UCHAR)1;
		} else {
			CsMsg->WaitForSync = (UCHAR)0;
		}
		for(i=0;i<CsMsg->NodeCount;i++) {
			CsMsg->Node[i].NodeFlags = pArbiter->NodeFlags[i];
			if (pArbiter->OutOfSyncRole != NO_OUT_OF_SYNC_ROLE &&  i==pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole])
				CsMsg->Node[i].NodeFlags |= DRIX_NODE_FLAG_OUT_OF_SYNC;
			CsMsg->Node[i].NodeRole = pArbiter->NodeToRoleMap[i];
		}
	}else if (DRIX_CMD_REQ_TO_YIELD_LOCK == Command) {
		PDRIX_REQ_TO_YIELD_LOCK YieldMsg = (PDRIX_REQ_TO_YIELD_LOCK) NotifyMsg;
		// CmdParam2: LockId
		// CmdParam3: Reason
		YieldMsg->LockId = HTONLL(CmdParam2);
		YieldMsg->Reason = HTONL(CmdParam3);
	} else if (DRIX_CMD_GRANT_LOCK == Command){
		PDRIX_GRANT_LOCK GrantMsg = (PDRIX_GRANT_LOCK) NotifyMsg;
		PDRAID_ARBITER_LOCK_CONTEXT Lock = (PDRAID_ARBITER_LOCK_CONTEXT) CmdParam1;
		// CmdParam1: Ptr to Lock
		
		GrantMsg->LockId = HTONLL(Lock->LockId);
		GrantMsg->LockType = Lock->LockType;
		GrantMsg->LockMode = Lock->LockMode;
		GrantMsg->Addr = HTONLL(Lock->LockAddress);
		GrantMsg->Length = HTONL(Lock->LockLength);
	}

	if (Client->LocalClient) {
		//
		// Send via local notification channel.
		//
		
		PDRIX_MSG_CONTEXT MsgEntry;
		PDRIX_MSG_CONTEXT ReplyMsgEntry;
		PLIST_ENTRY listEntry;

		PDRIX_HEADER ReplyMsg =NULL;	
		ACQUIRE_SPIN_LOCK(&Client->LocalClient->SpinLock, &oldIrql);
		if (Client->LocalClient->ClientStatus != DRAID_CLIENT_STATUS_ARBITER_CONNECTED) {
			RELEASE_SPIN_LOCK(&Client->LocalClient->SpinLock, oldIrql);
			KDPrintM(DBG_LURN_INFO, ("Local client has terminated\n"));
			ExFreePoolWithTag(NotifyMsg, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}
		RELEASE_SPIN_LOCK(&Client->LocalClient->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_TRACE, ("DRAID Sending notification %x to local client\n", DrixGetCmdString(Command)));	
		MsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
		if (MsgEntry) {
			PKEVENT				events[2];
			LONG				eventCount = 2;

			RtlZeroMemory(MsgEntry, sizeof(DRIX_MSG_CONTEXT));
			InitializeListHead(&MsgEntry->Link);
			MsgEntry->Message = NotifyMsg;
			ExInterlockedInsertTailList(&Client->NotificationChannel->Queue, &MsgEntry->Link, &Client->NotificationChannel->Lock);
			KDPrintM(DBG_LURN_TRACE, ("Setting event %p\n", &Client->NotificationChannel->Event));
			KeSetEvent(&Client->NotificationChannel->Event,IO_NO_INCREMENT, FALSE);

			KDPrintM(DBG_LURN_INFO, ("Waiting for notification reply %p\n", &Client->NotificationReplyChannel.Event));
			// Wait for reply or local client terminate event.
			events[0] = &Client->NotificationReplyChannel.Event;
			events[1] = pArbiter->LocalClientTerminatingEvent;

			status = KeWaitForMultipleObjects(
				eventCount, 
				events,
				WaitAny, 	Executive,KernelMode,
				TRUE,	NULL, NULL);
			if (status == STATUS_WAIT_0) {
				KeClearEvent(&Client->NotificationReplyChannel.Event);
				KDPrintM(DBG_LURN_TRACE, ("Received reply\n"));
				
				listEntry = ExInterlockedRemoveHeadList(&Client->NotificationReplyChannel.Queue, &Client->NotificationReplyChannel.Lock);
				if (listEntry) {
					ReplyMsgEntry = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
					ReplyMsg = ReplyMsgEntry->Message;
					// Notification always success.
					ExFreePoolWithTag(ReplyMsgEntry, DRAID_MSG_LINK_POOL_TAG);
					status = STATUS_SUCCESS;
					KDPrintM(DBG_LURN_INFO, ("DRAID Notification result=%x\n", ReplyMsg->Result));		
					ExFreePoolWithTag(ReplyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
				}
			} else {
				KDPrintM(DBG_LURN_INFO, ("Local client terminating. Stop waiting for notification.\n"));
				status = STATUS_UNSUCCESSFUL;
			}
		} else {
			status = STATUS_INSUFFICIENT_RESOURCES;
		}
	} else {
		LARGE_INTEGER Timeout;
		LONG Result;
		DRIX_HEADER ReplyMsg;
		LONG result;
		BOOLEAN ErrorOccured = FALSE;
		Timeout.QuadPart = 5 * HZ;

		KDPrintM(DBG_LURN_INFO, ("DRAID Sending notification %s to remote client\n", DrixGetCmdString(Command)));
		ASSERT(Client);
		ASSERT(Client->NotificationConnection);
		if (Client->NotificationConnection->ConnectionFileObject) {
			status =	LpxTdiSend(Client->NotificationConnection->ConnectionFileObject, (PUCHAR)NotifyMsg, MsgLength,
				0, &Timeout, NULL, &Result);
		} else {
			status = STATUS_UNSUCCESSFUL;
		}
		ExFreePoolWithTag(NotifyMsg, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to send notification message\n"));
			ErrorOccured = TRUE;
			status = STATUS_UNSUCCESSFUL;
		} else {
		
			// Start synchrous receiving with short timeout.
			// To do: Receive reply in asynchrous mode and handle receive in seperate function to remove arbiter blocking.
			Timeout.QuadPart = 5 * HZ;
			if (Client->NotificationConnection->ConnectionFileObject) {
				status = LpxTdiRecv(
								Client->NotificationConnection->ConnectionFileObject,
								(PUCHAR)&ReplyMsg, sizeof(DRIX_HEADER), 0, &Timeout, NULL, &result);
			} else {
				status = STATUS_UNSUCCESSFUL;
			}
			if (NT_SUCCESS(status) && result == sizeof(DRIX_HEADER)) {
				// Check validity
				if (ReplyMsg.Signature == HTONL(DRIX_SIGNATURE) &&
					ReplyMsg.Command ==Command &&
					ReplyMsg.ReplyFlag == 1 &&
					ReplyMsg.Length == HTONS((UINT16)sizeof(DRIX_HEADER)) &&
					ReplyMsg.Sequence == HTONS(Client->NotifySequence-1)) {
					KDPrintM(DBG_LURN_INFO, ("DRAID Notification result=%s\n", DrixGetResultString(ReplyMsg.Result)));
					status = STATUS_SUCCESS;
					ErrorOccured  = FALSE;
				} else {
					KDPrintM(DBG_LURN_INFO, ("Invalid reply packet\n"));
					status = STATUS_UNSUCCESSFUL;
					ErrorOccured  = TRUE;
					ASSERT(FALSE);
				}
			} else {
				KDPrintM(DBG_LURN_INFO, ("Failed to recv reply\n"));
				status = STATUS_UNSUCCESSFUL;
				ErrorOccured = TRUE;
			}
		}
		if (ErrorOccured) {
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			RemoveEntryList(&Client->Link);
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);				
			DraidArbiterTerminateClient(pArbiter, Client, FALSE);
			KDPrintM(DBG_LURN_INFO, ("Freeing client context\n"));
			ExFreePoolWithTag(Client, DRAID_CLIENT_CONTEXT_POOL_TAG);
		} else {
			//
			// Handle reply.
			//
			if (Command == DRIX_CMD_RETIRE) {
				KDPrintM(DBG_LURN_INFO, ("Terminating client context after RETIRING message.\n"));
				// Don't remove from Client->Link. Client is already removed from link before calling this.

				if (ReplyMsg.Result != DRIX_RESULT_SUCCESS) {
					DraidArbiterTerminateClient(pArbiter, Client, FALSE);
				} else {
					DraidArbiterTerminateClient(pArbiter, Client, TRUE);
				}
				ExFreePoolWithTag(Client, DRAID_CLIENT_CONTEXT_POOL_TAG);
			}
		}
	}
out:
	return status;
}

NTSTATUS
DraidArbiterGrantLockIfPossible(
	PDRAID_ARBITER_INFO Arbiter
) {
	UINT64 OverlapStart, OverlapEnd;
	BOOLEAN RangeAvailable;
	PLIST_ENTRY AcquiredListEntry;
	PLIST_ENTRY PendingListEntry;
	KIRQL oldIrql;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	PDRAID_ARBITER_LOCK_CONTEXT PendingLock;
	NTSTATUS status = STATUS_SUCCESS;
	
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	// For each pending locks.
	for(PendingListEntry = Arbiter->PendingLockList.Flink;
		PendingListEntry != &Arbiter->PendingLockList;
		PendingListEntry = PendingListEntry ->Flink)
	{
		PendingLock = CONTAINING_RECORD (PendingListEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
		RangeAvailable = TRUE;
		// Check lock list if lock is held by other client
		for (AcquiredListEntry = Arbiter->AcquiredLockList.Flink;
			AcquiredListEntry != &Arbiter->AcquiredLockList;
			AcquiredListEntry = AcquiredListEntry->Flink) 
		{
			Lock = CONTAINING_RECORD (AcquiredListEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
			if (DraidGetOverlappedRange(PendingLock->LockAddress, 
				PendingLock->LockLength, Lock->LockAddress, Lock->LockLength, 
				&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
				KDPrintM(DBG_LURN_INFO, ("Pending lock %I64x(%I64x:%x) overlaps with lock %I64x(%I64x:%x)\n",
					PendingLock->LockId, PendingLock->LockAddress, PendingLock->LockLength,
					Lock->LockId, Lock->LockAddress, Lock->LockLength
					));
				if (IsListEmpty(&Lock->ToYieldLink)) {
					InsertTailList(&Arbiter->ToYieldLockList, &Lock->ToYieldLink);
				}
				RangeAvailable = FALSE;
				break;
			}
		}
		if (RangeAvailable) {
			KDPrintM(DBG_LURN_INFO, ("Granting pending Lock %I64x\n", PendingLock->LockId));
			Lock = PendingLock;
			RemoveEntryList(&Lock->PendingLink);
			ASSERT(Lock->Owner);

			// Add to arbiter list
			ASSERT(Lock->Status == DRAID_ARBITER_LOCK_STATUS_PENDING);
			Lock->Status = DRAID_ARBITER_LOCK_STATUS_GRANTED;
			InsertTailList(&Arbiter->AcquiredLockList, &Lock->ArbiterAcquiredLink);
			InterlockedIncrement(&Arbiter->AcquiredLockCount);
//			KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n", Arbiter->AcquiredLockCount));

			DraidArbiterUpdateLwrBitmapBit(Arbiter, Lock, NULL);
			InsertTailList(&Lock->Owner->AcquiredLockList, &Lock->ClientAcquiredLink);
			if (Arbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
				KDPrintM(DBG_LURN_INFO, ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",
					Lock->LockAddress, Lock->LockLength));
				DraidArbiterChangeOosBitmapBit(Arbiter, TRUE, Lock->LockAddress, Lock->LockLength);
			}
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

			// Need to update BMP and LWR before client start writing using this lock
			status = DraidArbiterUpdateOnDiskOosBitmap(Arbiter, FALSE);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Failed to update bitmap\n"));
			}

			status = DraidArbiterNotify(Arbiter, Lock->Owner, DRIX_CMD_GRANT_LOCK, (UINT64)Lock, 0, 0);
			ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Failed to notify.\n"));
				break;
			}
		} 
	}
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	return status;
}

//
// Refresh RAID status and Node status from node status reported by clients.
// If ForceChange is TRUE, Test RAID status change when node is not changed. This happens when NodeFlags is manually updated.
// Return TRUE if any RAID status has changed.
//
BOOLEAN
DraidArbiterRefreshRaidStatus(
	PDRAID_ARBITER_INFO		pArbiter	,
	BOOLEAN				ForceChange
	)
{
	UINT32 i;
	KIRQL oldIrql;
	BOOLEAN Changed = FALSE;
	PLIST_ENTRY	listEntry;
	PDRAID_CLIENT_CONTEXT Client;
	UINT32 NewRaidStatus;
	UCHAR NewNodeFlags[MAX_DRAID_MEMBER_DISK] = { 0};
	BOOLEAN BmpChanged = FALSE;
	NTSTATUS status;	
#if 0
	BOOLEAN NewDisk = FALSE;
#endif
	//
	// Merge report from each client.
	//
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);

	//
	//	Gather each client's node information
	//
	for(i=0;i<pArbiter->TotalDiskCount;i++) {
		if (IsListEmpty(&pArbiter->ClientList)) {
			NewNodeFlags[i] = pArbiter->NodeFlags[i]; // Keep previous flag if no client exists.
			continue;
		}

		NewNodeFlags[i] = 0;
		for (listEntry = pArbiter->ClientList.Flink;
			listEntry != &pArbiter->ClientList;
			listEntry = listEntry->Flink) 
		{
			Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
			if (Client->Initialized == FALSE)
				continue;

			// If already defective node, add up any new defective code and keep defective flag
			if (pArbiter->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE) {
				NewNodeFlags[i] |= DRIX_NODE_FLAG_DEFECTIVE;
				if (Client->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE)  {
					KDPrintM(DBG_LURN_INFO, ("Node %d keep defective flag. Defect code %x -> %x\n",
						i, pArbiter->DefectCodes[i], Client->DefectCode[i]));
					NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING|DRIX_NODE_FLAG_STOP);
					if (Client->DefectCode[i] != 0) {
						pArbiter->DefectCodes[i] |= Client->DefectCode[i];
					}
				} else {
					KDPrintM(DBG_LURN_INFO, ("Node %d keep defective code %x\n",
						i, pArbiter->DefectCodes[i]));
				}
				continue;
			}

			//
			// Flag priority:  DRIX_NODE_FLAG_DEFECTIVE > DRIX_NODE_FLAG_STOP > DRIX_NODE_FLAG_RUNNING > DRIX_NODE_FLAG_UNKNOWN
			//
			if (Client->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE)  {
				NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING|DRIX_NODE_FLAG_STOP);
				NewNodeFlags[i] |= DRIX_NODE_FLAG_DEFECTIVE;
				if (Client->DefectCode[i] != 0) {
					pArbiter->DefectCodes[i] |= Client->DefectCode[i];
				}
			} else if ((Client->NodeFlags[i] & DRIX_NODE_FLAG_STOP) &&
				!(NewNodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE)
			) {
				NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING);	
				NewNodeFlags[i] |= DRIX_NODE_FLAG_STOP;
			} else if ((Client->NodeFlags[i] & DRIX_NODE_FLAG_RUNNING) &&
				!(NewNodeFlags[i] & (DRIX_NODE_FLAG_DEFECTIVE|DRIX_NODE_FLAG_STOP))
			) {
				NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN);	
				NewNodeFlags[i] |= DRIX_NODE_FLAG_RUNNING;
			}
			KDPrintM(DBG_LURN_INFO, ("Node %d: Old flag %x, Client Flag %x, New flag %x\n", 
				i, pArbiter->NodeFlags[i], Client->NodeFlags[i], NewNodeFlags[i]));
#if 0
			if ((Client->NodeFlags[i] & DRIX_NODE_FLAG_NEW_DISK) && ) {
				// Don't need to store about new disk. Just set all bitmap
				KDPrintM(DBG_LURN_INFO, ("Changing Node flags from %02x to %02x\n", pArbiter->NodeFlags[i], NewNodeFlags[i]));						
				NewDisk = TRUE;
				DraidArbiterChangeOosBitmapBit(pArbiter, TRUE, 0, pArbiter->Lurn->UnitBlocks);
			}
#endif			
		}
		if (NewNodeFlags[i] == 0) {
			//
			// No client is initialized yet. Just keep old flag.
			//
			KDPrintM(DBG_LURN_INFO, ("No client is initialized. Keep node %d flags %x\n",
				i, pArbiter->NodeFlags[i]));
			NewNodeFlags[i] = pArbiter->NodeFlags[i];
		}
	}

	if (ForceChange) {
		Changed = TRUE;
	} else {
		// 
		// Check any node information has changed
		//
		for(i=0;i<pArbiter->TotalDiskCount;i++) {
			if (NewNodeFlags[i] != pArbiter->NodeFlags[i]) {
				KDPrintM(DBG_LURN_INFO, ("Changing Node %d flags from %02x to %02x\n", i, pArbiter->NodeFlags[i], NewNodeFlags[i]));
				pArbiter->NodeFlags[i] = NewNodeFlags[i];
				Changed = TRUE;
			}
		}
	}
	//
	// Test new RAID status only when needed, i.e: node has changed or first time.
	//
	if (Changed) {
		BOOLEAN NewFaultyNode = FALSE;
		UINT32 FaultCount = 0;
		UCHAR FaultRole = 0;
		for(i=0;i<pArbiter->ActiveDiskCount;i++) { // i : role index
			if (!(NewNodeFlags[pArbiter->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING)) {
				FaultCount++;
				if (pArbiter->OutOfSyncRole != i) {
					// Faulty disk is not marked as out-of-sync.
					NewFaultyNode = TRUE;
					FaultRole = (UCHAR)i;
				}
			}
		}

		if (FaultCount == 0) {
			if (pArbiter->OutOfSyncRole == NO_OUT_OF_SYNC_ROLE) {
				NewRaidStatus = DRIX_RAID_STATUS_NORMAL;
			} else {
				NewRaidStatus = DRIX_RAID_STATUS_REBUILDING;
			}
		} else if (FaultCount == 1) {
			if (pArbiter->OutOfSyncRole != NO_OUT_OF_SYNC_ROLE) {
				// There was out-of-sync node and..
				if (NewFaultyNode) { 
					// New faulty node appeared.
					KDPrintM(DBG_LURN_INFO, ("Role %d(node %d) also failed. RAID failure\n", 
						FaultRole, pArbiter->RoleToNodeMap[FaultRole]));
					NewRaidStatus = DRIX_RAID_STATUS_FAILED;
				} else {	
					// OutOfSync node is still faulty.
					NewRaidStatus = DRIX_RAID_STATUS_DEGRADED;
				}
			} else {
				// Faulty node appeared from zero fault.
				pArbiter->OutOfSyncRole = FaultRole;
				KDPrintM(DBG_LURN_INFO, ("Setting out of sync role: %d\n", FaultRole));
				NewRaidStatus = DRIX_RAID_STATUS_DEGRADED;
			}
		} else { // FaultCount > 1
			KDPrintM(DBG_LURN_INFO, ("More than 1 active node is at fault. RAID failure\n"));
			NewRaidStatus = DRIX_RAID_STATUS_FAILED;
		} 
	}else {
		NewRaidStatus = pArbiter->RaidStatus;
	}
	
	if (pArbiter->RaidStatus != NewRaidStatus) {
		KDPrintM(DBG_LURN_INFO, ("Changing DRAID Status from %x to %x\n", pArbiter->RaidStatus, NewRaidStatus));
		if (NewRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
			PDRAID_ARBITER_LOCK_CONTEXT Lock;
			KeQueryTickCount(&pArbiter->DegradedSince);
			//
			// We need to merge all LWR to OOS bitmap
			//	because defective/stopped node may have lost data in disk's write-buffer.
			//
			KDPrintM(DBG_LURN_INFO, ("Merging LWRs to OOS bitmap\n"));
			for (listEntry = pArbiter->AcquiredLockList.Flink;
				listEntry != &pArbiter->AcquiredLockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
				KDPrintM(DBG_LURN_ERROR, ("Merging LWR %I64x:%x to OOS bitmap\n", 
					Lock->LockAddress, Lock->LockLength));
				DraidArbiterChangeOosBitmapBit(pArbiter, TRUE, Lock->LockAddress, Lock->LockLength);
				// We need to write updated bitmap before any lock information is changed.
				BmpChanged = TRUE;
			}
		}
		
		if (NewRaidStatus == DRIX_RAID_STATUS_REBUILDING) {
			// Initialize some values before enter rebuilding mode.
			pArbiter->RebuildInfo.AggressiveRebuildMode = FALSE;
		}
		
		pArbiter->RaidStatus = NewRaidStatus;
		Changed = TRUE;
	}

	if (Changed) {
		pArbiter->SyncStatus = DRAID_SYNC_REQUIRED;
		// RMD update and RAID status sync will be done by arbiter thread.		
	}
	
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	if (Changed) {
		// Need to force-update because revived node may not have up-to-date RMD.
		DraidArbiterUpdateInCoreRmd(pArbiter, TRUE);
		DraidArbiterWriteRmd(pArbiter,  &pArbiter->Rmd);
	}
	if (BmpChanged) {
		// We need to write updated bitmap before any lock information is changed.
		status = DraidArbiterUpdateOnDiskOosBitmap(pArbiter, FALSE);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("Failed to update bitmap\n"));
		}
	}
	return Changed;
}

//
// 
//
VOID
DraidArbiterTerminateClient(
	PDRAID_ARBITER_INFO		Arbiter,
	PDRAID_CLIENT_CONTEXT Client,
	BOOLEAN					CleanExit	// Client is terminated cleanly through retire.
) {
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	PLIST_ENTRY	listEntry;
	NTSTATUS status;
	KIRQL oldIrql;
	PDRIX_MSG_CONTEXT MsgEntry;
	BOOLEAN AbandonedLockExist;
	KDPrintM(DBG_LURN_INFO, ("Terminating client..\n"));
	
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	
	//
	// Free locks acquired by this client
	//
	AbandonedLockExist = FALSE;
	while(TRUE) {
		listEntry = RemoveHeadList (&Client->AcquiredLockList);
		if (listEntry == &Client->AcquiredLockList)
			break;
		Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ClientAcquiredLink);
		// Remove from arbiter's list too.
		RemoveEntryList(&Lock->ArbiterAcquiredLink);
		// Remove from to yield lock list.
		RemoveEntryList(&Lock->ToYieldLink);
		InitializeListHead(&Lock->ToYieldLink); // to check bug...
		InterlockedDecrement(&Arbiter->AcquiredLockCount);
//		KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount));
		KDPrintM(DBG_LURN_INFO, ("Freeing terminated client's lock %I64x(%I64x:%x)\n", Lock->LockId,
			Lock->LockAddress, Lock->LockLength));
		if (!CleanExit) {
			KDPrintM(DBG_LURN_INFO, ("Unclean termination. Merging this lock range to dirty bitmap\n"));			
			// Merge this client's LWR to bitmap
			DraidArbiterChangeOosBitmapBit(Arbiter, TRUE, Lock->LockAddress, Lock->LockLength);
			AbandonedLockExist = TRUE;			
		}
		DraidArbiterFreeLock(Arbiter, Lock);
	}
	if (AbandonedLockExist) {
		//
		// RAID data may be inconsistent due to disconnected client.
		// Try to synchronize regions locked by this client
		//

		if (Arbiter->OutOfSyncRole == NO_OUT_OF_SYNC_ROLE) {
			// Select one node.
			KDPrintM(DBG_LURN_ERROR, ("Client exited without unlocking locks. Marking node %d as out-of-sync.\n", Arbiter->RoleToNodeMap[Arbiter->ActiveDiskCount-1]));
			Arbiter->OutOfSyncRole = Arbiter->ActiveDiskCount-1;
		}
	}
	
	for (listEntry = Arbiter->PendingLockList.Flink;
		listEntry != &Arbiter->PendingLockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
		if (Lock->Owner == Client) {
			KDPrintM(DBG_LURN_INFO, ("Freeing terminated client's pending lock %I64x(%I64x:%x)\n", Lock->LockId,
				Lock->LockAddress, Lock->LockLength));
			// Remove from all list
			listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
			RemoveEntryList(&Lock->PendingLink);
			
			ASSERT(IsListEmpty(&Lock->ArbiterAcquiredLink));
			ASSERT(IsListEmpty(&Lock->ToYieldLink));			
			ASSERT(Lock->Status == DRAID_ARBITER_LOCK_STATUS_PENDING);
			DraidArbiterFreeLock(Arbiter, Lock);
		}
	}

	if (Client->LocalClient) {
		while(TRUE)
		{	
			listEntry = RemoveHeadList(&Client->RequestChannel.Queue);
			if (listEntry == &Client->RequestChannel.Queue) {
				break;
			}
			MsgEntry = CONTAINING_RECORD(
					listEntry,
					DRIX_MSG_CONTEXT,
					Link
			);
			
			KDPrintM(DBG_LURN_INFO, ("Freeing unhandled request message\n"));
			ExFreePoolWithTag(MsgEntry->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);
			ExFreePoolWithTag(MsgEntry, DRAID_MSG_LINK_POOL_TAG);
		}

		while(TRUE)
		{
			listEntry = RemoveHeadList(&Client->NotificationReplyChannel.Queue);
			if (listEntry == &Client->NotificationReplyChannel.Queue) {
				break;
			}
			MsgEntry = CONTAINING_RECORD(
					listEntry,
					DRIX_MSG_CONTEXT,
					Link
			);
			
			KDPrintM(DBG_LURN_INFO, ("Freeing unhandled notification-reply message\n"));
			ExFreePoolWithTag(MsgEntry->Message, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
			ExFreePoolWithTag(MsgEntry, DRAID_MSG_LINK_POOL_TAG);
		}
	} 

	DraidArbiterUpdateLwrBitmapBit(Arbiter, NULL, NULL);
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);


	status = DraidArbiterUpdateOnDiskOosBitmap(Arbiter, FALSE);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to update OOS bitmap\n"));
	}

	if (Client->NotificationConnection) {
		if (Client->NotificationConnection->ConnectionFileObject) {
			LpxTdiDisconnect(Client->NotificationConnection->ConnectionFileObject, 0);
			LpxTdiDisassociateAddress(Client->NotificationConnection->ConnectionFileObject);
			LpxTdiCloseConnection(Client->NotificationConnection->ConnectionFileHandle, Client->NotificationConnection->ConnectionFileObject);
		}
		ExFreePoolWithTag(Client->NotificationConnection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);
		Client->NotificationConnection = NULL;
	} 
	if (Client->RequestConnection) {
		if (Client->RequestConnection->ConnectionFileObject) {
			LpxTdiDisconnect(Client->RequestConnection->ConnectionFileObject, 0);
			LpxTdiDisassociateAddress(Client->RequestConnection->ConnectionFileObject);
			LpxTdiCloseConnection(Client->RequestConnection->ConnectionFileHandle, Client->RequestConnection->ConnectionFileObject);
		}
		ExFreePoolWithTag(Client->RequestConnection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);
		Client->RequestConnection = NULL;
	}

	DraidArbiterRefreshRaidStatus(Arbiter, AbandonedLockExist);
}

//
// MsgHandled is changed only when message is not handled.
//
NTSTATUS
DraidArbiterCheckRequestMsg(
	PDRAID_ARBITER_INFO Arbiter,
	PBOOLEAN	MsgHandled
) {
	NTSTATUS status= STATUS_SUCCESS;
	PDRAID_CLIENT_CONTEXT Client;
	PLIST_ENTRY listEntry;
	KIRQL oldIrql;
		
	*MsgHandled = FALSE;
	if (Arbiter->LocalClient) {
		KeClearEvent(&Arbiter->LocalClient->RequestChannel.Event);
		//
		// Handle any pending requests from local client
		//
		while(listEntry = ExInterlockedRemoveHeadList(
							&Arbiter->LocalClient->RequestChannel.Queue,
							&Arbiter->LocalClient->RequestChannel.Lock
							)) 
		{
			PDRIX_MSG_CONTEXT Msg;
			
			Msg = CONTAINING_RECORD(
					listEntry,
					DRIX_MSG_CONTEXT,
					Link
			);
			status = DraidArbiterHandleRequestMsg(Arbiter, Arbiter->LocalClient, Msg->Message);
			if (!NT_SUCCESS(status)) {
				ASSERT(FALSE);
			}
			*MsgHandled = TRUE;
			//
			// Free received message
			//
			ExFreePoolWithTag(Msg->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);
			ExFreePoolWithTag(Msg, DRAID_MSG_LINK_POOL_TAG);
		}
	}
restart:
	// Check request is received through request connection.
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	for (listEntry = Arbiter->ClientList.Flink;
		listEntry != &Arbiter->ClientList;
		listEntry = listEntry->Flink)  {
		Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
		if (Client->RequestConnection && 
			Client->RequestConnection->Receiving &&
			KeReadStateEvent(&Client->RequestConnection->TdiReceiveContext.CompletionEvent)) {
			BOOLEAN ErrorOccured = FALSE;
			Client->RequestConnection->Receiving = FALSE;
			KeClearEvent(&Client->RequestConnection->TdiReceiveContext.CompletionEvent);
			if (Client->RequestConnection->TdiReceiveContext.Result < 0) {
				KDPrintM(DBG_LURN_INFO, ("Failed to receive from %02x:%02x:%02x:%02x:%02x:%02x\n",
					Client->RemoteClientAddr[0], Client->RemoteClientAddr[1], Client->RemoteClientAddr[2],
					Client->RemoteClientAddr[3], Client->RemoteClientAddr[4], Client->RemoteClientAddr[5]
					));
				ErrorOccured = TRUE;
//				ASSERT(FALSE);
			} else if (Client->RequestConnection->TdiReceiveContext.Result != sizeof(DRIX_HEADER)) {
				KDPrintM(DBG_LURN_INFO, ("Failed to receive from %02x:%02x:%02x:%02x:%02x:%02x. Only %d bytes received.\n",
					Client->RemoteClientAddr[0], Client->RemoteClientAddr[1], Client->RemoteClientAddr[2],
					Client->RemoteClientAddr[3], Client->RemoteClientAddr[4], Client->RemoteClientAddr[5],
					Client->RequestConnection->TdiReceiveContext.Result
					));
				ErrorOccured = TRUE;
			} else {
				PDRIX_HEADER	Message;
				ULONG MsgLength;
				KDPrintM(DBG_LURN_INFO, ("Request received from %02x:%02x:%02x:%02x:%02x:%02x\n",
					Client->RemoteClientAddr[0], Client->RemoteClientAddr[1], Client->RemoteClientAddr[2],
					Client->RemoteClientAddr[3], Client->RemoteClientAddr[4], Client->RemoteClientAddr[5]
					));

				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

				//
				// Read remaining data if needed.
				//

				Message = (PDRIX_HEADER) Client->RequestConnection->ReceiveBuf;
				MsgLength = NTOHS(Message->Length);
				if (MsgLength > DRIX_MAX_REQUEST_SIZE) {
					KDPrintM(DBG_LURN_INFO, ("Message is too long %d\n", MsgLength));
					ErrorOccured = TRUE;
				} else if (MsgLength > sizeof(DRIX_HEADER)) {
					LARGE_INTEGER Timeout;
					LONG result;
					ULONG AddtionalLength;
					Timeout.QuadPart = 5 * HZ;
					AddtionalLength = MsgLength -sizeof(DRIX_HEADER);
					KDPrintM(DBG_LURN_INFO, ("Reading additional message data %d bytes\n", AddtionalLength));
					if (Client->RequestConnection->ConnectionFileObject) {
						status = LpxTdiRecv(Client->RequestConnection->ConnectionFileObject, 
							(PUCHAR)(Client->RequestConnection->ReceiveBuf + sizeof(DRIX_HEADER)),
							AddtionalLength,
							0, &Timeout, NULL, &result);
					} else {
						status = STATUS_UNSUCCESSFUL;
					}
					if (!NT_SUCCESS(status) || result != AddtionalLength) {
						KDPrintM(DBG_LURN_INFO, ("Failed to get remaining message\n", MsgLength));		
						ErrorOccured = TRUE;
						ASSERT(FALSE);
					}
				} else if (MsgLength < sizeof(DRIX_HEADER)) {
					KDPrintM(DBG_LURN_INFO, ("Message is too short %d\n", MsgLength));
					ErrorOccured = TRUE;
				}
				
				if (!ErrorOccured) {
					status = DraidArbiterHandleRequestMsg(Arbiter, Client, (PDRIX_HEADER)Client->RequestConnection->ReceiveBuf);
					if (NT_SUCCESS(status)) {						
						*MsgHandled = TRUE;
					} else {
						ErrorOccured = TRUE;
					}
				}
				ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
			}
			if (ErrorOccured) {
				RemoveEntryList(&Client->Link);
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);				
				DraidArbiterTerminateClient(Arbiter, Client, FALSE);
				KDPrintM(DBG_LURN_INFO, ("Freeing client context\n"));
				ExFreePoolWithTag(Client, DRAID_CLIENT_CONTEXT_POOL_TAG);
				goto restart;
			}			
		}	
	}
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	return status;
}



VOID
DraidArbiterThreadProc(
	IN	PVOID	Context
	)
{
	PLURELATION_NODE		Lurn;
	PRAID_INFO				pRaidInfo;
	PDRAID_ARBITER_INFO		Arbiter;
	KIRQL oldIrql;
	NTSTATUS status;
	LARGE_INTEGER TimeOut;
	UINT32					nChildCount;
	UINT32					nActiveDiskCount;
	UINT32					nSpareCount;
	UINT32					SectorsPerBit;
//	UCHAR Flags;
	BOOLEAN RmdChanged;
	PLIST_ENTRY	listEntry;
	PDRAID_CLIENT_CONTEXT Client;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	INT32 				MaxEventCount =0;
	PKEVENT				*events = NULL;
	PKWAIT_BLOCK		WaitBlocks = NULL;
	NTSTATUS			waitStatus;
	INT32 				eventCount;
	BOOLEAN				Handled;
	BOOLEAN	DoMore; 
	
	KDPrintM(DBG_LURN_INFO, ("DRAID Arbiter thread starting\n"));
	
	MaxEventCount = DraidReallocEventArray(&events, &WaitBlocks, MaxEventCount);

	// Initialize basic values and check sanity.
	Lurn = (PLURELATION_NODE)Context;
	ASSERT(Lurn);
	ASSERT(LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType);
	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);
	Arbiter = (PDRAID_ARBITER_INFO) pRaidInfo->pDraidArbiter;
	ASSERT(Arbiter);

	// Set some value from svc.
	SectorsPerBit = pRaidInfo->SectorsPerBit;
	ASSERT(SectorsPerBit > 0);

	nChildCount = Lurn->LurnChildrenCnt;
	nActiveDiskCount = Lurn->LurnChildrenCnt - pRaidInfo->nSpareDisk;
	nSpareCount = pRaidInfo->nSpareDisk;

	ASSERT(nActiveDiskCount == pRaidInfo->nDiskCount);
	KDPrintM(DBG_LURN_INFO, ("ChildCount: %d, DiskCount : %d\n", nChildCount, nActiveDiskCount));



	DraidArbiterUpdateInCoreRmd(Arbiter, TRUE);
	status = DraidArbiterWriteRmd(Arbiter, &Arbiter->Rmd);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to update RMD\n"));
		goto fail;
	}

	status = DraidRebuildIoStart(Arbiter);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to start rebuild IO thread\n"));
		goto fail;
	}

	KDPrintM(DBG_LURN_INFO, ("DRAID Aribiter enter running status\n"));
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	Arbiter->Status = DRAID_ARBITER_STATUS_RUNNING;
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);		

	DraidRegisterArbiter(Arbiter);

	DoMore = TRUE;
	//
	// Wait for request from client or stop request.
	//
	while(TRUE) {
restart:
		if (events == NULL || WaitBlocks == NULL) {
			KDPrintM(DBG_LURN_INFO, ("Insufficient memory\n"));
			break;
		}
		
		//
		// To do: Broadcast Arbiter node presence time to time..
		//

		if (DoMore) {
			// Recheck any pending request without waiting 
			DoMore = FALSE;
		} else {			
			eventCount = 0;
			events[eventCount] = &Arbiter->ArbiterThreadEvent;
			eventCount++;
			if (Arbiter->LocalClient) {
				events[eventCount] = &Arbiter->LocalClient->RequestChannel.Event;
				eventCount++;
			}
			ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
			for (listEntry = Arbiter->ClientList.Flink;
				listEntry != &Arbiter->ClientList;
				listEntry = listEntry->Flink)  {
				Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
				if (Client->RequestConnection) {
					if (MaxEventCount < eventCount+1) {
						RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
						MaxEventCount = DraidReallocEventArray(&events, &WaitBlocks, MaxEventCount);
						goto restart;
					}
					if (!Client->RequestConnection->Receiving) {
						// Receive from request connection if not receiving.
						RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
						Client->RequestConnection->TdiReceiveContext.Irp = NULL;
						KeInitializeEvent(&Client->RequestConnection->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
						if (Client->RequestConnection->ConnectionFileObject) {
							status = LpxTdiRecvWithCompletionEvent(
											Client->RequestConnection->ConnectionFileObject,
											&Client->RequestConnection->TdiReceiveContext,
											Client->RequestConnection->ReceiveBuf,
											sizeof(DRIX_HEADER),	0, NULL,NULL
							);
						} else {
							status = STATUS_UNSUCCESSFUL;
						}
						if (!NT_SUCCESS(status)) {
							KDPrintM(DBG_LURN_INFO, ("Failed to start to recv from client. Terminating this client\n"));
							ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
							RemoveEntryList(&Client->Link);
							RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
							DraidArbiterTerminateClient(Arbiter, Client, FALSE);
							KDPrintM(DBG_LURN_INFO, ("Freeing client context\n"));
							ExFreePoolWithTag(Client, DRAID_CLIENT_CONTEXT_POOL_TAG);
							DoMore = TRUE;
							ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
							break;
						}
						ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
						Client->RequestConnection->Receiving = TRUE;
					}
					events[eventCount] = &Client->RequestConnection->TdiReceiveContext.CompletionEvent;
					eventCount++;
				}
				// to do: Add event for reply to notification
				
			}
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			TimeOut.QuadPart = - NANO100_PER_SEC * 30; // need to wake-up to handle dirty bitmap
			waitStatus = KeWaitForMultipleObjects(
					eventCount, 	events,	WaitAny, 	Executive,KernelMode,
					TRUE,	&TimeOut, WaitBlocks);
			KeClearEvent(&Arbiter->ArbiterThreadEvent);
		}

		// Check client unregister request
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		for (listEntry = Arbiter->ClientList.Flink;
			listEntry != &Arbiter->ClientList;
			listEntry = listEntry->Flink)  {
			Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
			if (Client->UnregisterRequest) {
				if (Client == Arbiter->LocalClient) {
					Arbiter->LocalClient = NULL;
				}
				Client->UnregisterRequest = FALSE;
				RemoveEntryList(&Client->Link);
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

				DraidArbiterTerminateClient(Arbiter, Client, FALSE);

				KeSetEvent(&Client->UnregisterDoneEvent, IO_NO_INCREMENT, FALSE);
				ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
				break;
			}
		}		
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

		//
		// Handle requested rebuild IO
		//
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		if (Arbiter->RebuildInfo.Status == DRAID_REBUILD_STATUS_DONE ||
			Arbiter->RebuildInfo.Status == DRAID_REBUILD_STATUS_FAILED ||
			Arbiter->RebuildInfo.Status == DRAID_REBUILD_STATUS_CANCELLED) {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			DraidRebuildIoAcknowledge(Arbiter);
			DoMore = TRUE;
		} else {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		}

		
		//
		// Send status update if not initialized
		//
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		for (listEntry = Arbiter->ClientList.Flink;
			listEntry != &Arbiter->ClientList;
			listEntry = listEntry->Flink)  {
			Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
			if (!Client->Initialized) {
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				KDPrintM(DBG_LURN_INFO, ("Notifying initial status to client\n"));
				status = DraidArbiterNotify(Arbiter, Client, DRIX_CMD_CHANGE_STATUS, 0, 0,0);
				if (status !=STATUS_SUCCESS) {
					ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);					
					// Failed to notify. This client may have been removed. Re-iterate from first.
					DoMore = TRUE;
					break;
				} else {
					ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
					Client->Initialized = TRUE;
					RtlCopyMemory(Client->NodeFlags, Arbiter->NodeFlags, sizeof(Arbiter->NodeFlags));
					DoMore = TRUE;
				}
			}
		}
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

		DraidArbiterCheckRequestMsg(Arbiter, &Handled);
		if (Handled)
			DoMore = TRUE;
		//
		// Check spare disk is usable.
		//
		DraidArbiterUseSpareIfNeeded(Arbiter);
		
		// RAID status may have changed
		if (Arbiter->SyncStatus == DRAID_SYNC_REQUIRED) {			
			Arbiter->SyncStatus = DRAID_SYNC_IN_PROGRESS;
			//
			// Send raid status change to all client
			//
			KDPrintM(DBG_LURN_INFO, ("Sending DRIX_CMD_CHANGE_STATUS to all client\n"));
			for (listEntry = Arbiter->ClientList.Flink;
				listEntry != &Arbiter->ClientList;
				listEntry = listEntry->Flink)  {
				Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
				if (Client->Initialized == FALSE)
					continue;
				status = DraidArbiterNotify(Arbiter, Client, DRIX_CMD_CHANGE_STATUS, 1, 0,0);
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_INFO, ("Notify failed during change status. Restarting.\n"));
					Arbiter->SyncStatus = DRAID_SYNC_REQUIRED;
					DoMore = TRUE;
					goto restart;
				}
			}
			KDPrintM(DBG_LURN_INFO, ("Sending DRIX_CMD_STATUS_SYNCED to all client\n"));				
			for (listEntry = Arbiter->ClientList.Flink;
				listEntry != &Arbiter->ClientList;
				listEntry = listEntry->Flink)  {
				Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
				if (Client->Initialized == FALSE)
					continue;
				status = DraidArbiterNotify(Arbiter, Client, DRIX_CMD_STATUS_SYNCED, 0, 0,0);
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_INFO, ("Notify failed during sync. Restarting.\n"));
					Arbiter->SyncStatus = DRAID_SYNC_REQUIRED;
					DoMore = TRUE;
					goto restart;
				}				
			}
			Arbiter->SyncStatus = DRAID_SYNC_DONE;
			DoMore = TRUE;
		}

		//
		//  Send yield message if other client is requested.
		//
		while(listEntry = ExInterlockedRemoveHeadList(
			&Arbiter->ToYieldLockList,
			&Arbiter->SpinLock)) {
			Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ToYieldLink);
			InitializeListHead(&Lock->ToYieldLink);
			if (Lock->Owner) {
				status = DraidArbiterNotify(Arbiter, Lock->Owner, DRIX_CMD_REQ_TO_YIELD_LOCK, 0, Lock->LockId,0);
				if (!NT_SUCCESS(status)) {
					// ClientAcquired may have destroyed and ToYieldLockList may have been changed. restart
					KDPrintM(DBG_LURN_INFO, ("Notify failed during yield lock. Restarting.\n"));
					DoMore = TRUE;
					goto restart;
				}
			}
		}

		// 
		// Send grant message if any pending lock exist.
		//
		status = DraidArbiterGrantLockIfPossible(Arbiter);
		if (!NT_SUCCESS(status)) {
			break;
		}
		
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		if (Arbiter->RaidStatus == DRIX_RAID_STATUS_REBUILDING) {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			DraidRebuilldIoInitiate(Arbiter);
		} else {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		}
		// Check termination
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		if (Arbiter->StopRequested) {
			Arbiter->Status = DRAID_ARBITER_STATUS_TERMINATING;
			Arbiter->StopRequested = FALSE;
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			DraidRebuildIoStop(Arbiter);
			DraidRebuildIoAcknowledge(Arbiter);
			
			KDPrintM(DBG_LURN_INFO, ("DRAID Aribter stop requested.Sending RETIRE message to all client..\n"));	
			while(listEntry = ExInterlockedRemoveHeadList(&Arbiter->ClientList, &Arbiter->SpinLock)) {
				Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
				status = DraidArbiterNotify(Arbiter, Client, DRIX_CMD_RETIRE, 0, 0,0); // Client list may has been changed.
				// Continue to next client even if notification has failed.
			}
			KDPrintM(DBG_LURN_INFO, ("Sent RETIRE to all client. Exiting arbiter loop.\n"));
			break; // Exit arbiter loop.
		} else {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		}

		RmdChanged = DraidArbiterUpdateInCoreRmd(Arbiter, FALSE);
		if (RmdChanged) {
			DraidArbiterWriteRmd(Arbiter,  &Arbiter->Rmd);
		}
	}

#if 0
	status = KeWaitForSingleObject(
		&Arbiter->ArbiterThreadEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);
#endif


	while(listEntry = ExInterlockedRemoveHeadList(&Arbiter->ClientList, &Arbiter->SpinLock)) {
		Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
		DraidArbiterTerminateClient(Arbiter, Client, FALSE);
		ExFreePoolWithTag(Client, DRAID_CLIENT_CONTEXT_POOL_TAG);
	}

	ASSERT(DRAID_ARBITER_STATUS_TERMINATING== Arbiter->Status);
	
	DraidArbiterUpdateInCoreRmd(Arbiter, FALSE);
	status = DraidArbiterWriteRmd(Arbiter, &Arbiter->Rmd);

fail:
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	Arbiter->Status = DRAID_ARBITER_STATUS_TERMINATING;
	Arbiter->RaidStatus = DRIX_RAID_STATUS_FAILED;
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

	DraidFreeEventArray(events, WaitBlocks);
	
	KDPrintM(DBG_LURN_INFO, ("Exiting\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
	return;
}


NTSTATUS 
DradArbiterInitializeOosBitmap(
	PDRAID_ARBITER_INFO pArbiter,
	UINT32	UpToDateNode
) {
	PRAID_INFO				pRaidInfo = pArbiter->Lurn->LurnRAIDInfo;
	UINT32	i;
	NTSTATUS status;
	BOOLEAN ValidBitmapBlock;
	UINT32 ByteCount;
	ULONG SetBits;
	//
	// Calc bitmap size. 
	//
	if (pRaidInfo->SectorsPerBit == 0) {
		KDPrintM(DBG_LURN_ERROR, ("SectorPerBit is 0\n"));
		status = STATUS_UNSUCCESSFUL;
		ASSERT(FALSE);
		goto errout;
	}


	pArbiter->OosBmpBitCount = (ULONG)((pArbiter->Lurn->UnitBlocks + pArbiter->SectorsPerOosBmpBit -1)/ pArbiter->SectorsPerOosBmpBit);
	pArbiter->OosBmpByteCount = ((pArbiter->OosBmpBitCount + sizeof(ULONG)*8 -1) /(sizeof(ULONG)*8))*sizeof(ULONG); // In core bit size should be padded to ULONG size.
	pArbiter->OosBmpSectorCount = (pArbiter->OosBmpBitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;
	if (pArbiter->OosBmpSectorCount == 0) {
		KDPrintM(DBG_LURN_ERROR, ("BMP Sector count is 0!!\n"));
		ASSERT(FALSE);
		pArbiter->OosBmpSectorCount = 1;
	}
	pArbiter->OosBmpInCoreBuffer = ExAllocatePoolWithTag(NonPagedPool, pArbiter->OosBmpByteCount,
								DRAID_BITMAP_POOL_TAG);
	if (pArbiter->OosBmpInCoreBuffer == NULL) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to allocate OOS in memory bitmap\n"));
		ASSERT(FALSE);
		status =STATUS_INSUFFICIENT_RESOURCES;
		goto errout;
	}
	pArbiter->OosBmpOnDiskBuffer = ExAllocatePoolWithTag(NonPagedPool, pArbiter->OosBmpSectorCount * SECTOR_SIZE, DRAID_BITMAP_POOL_TAG);
	if (pArbiter->OosBmpOnDiskBuffer == NULL) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to allocate OOS On disk buffer\n"));
		ASSERT(FALSE);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	pArbiter->LwrBmpBuffer = ExAllocatePoolWithTag(NonPagedPool, pArbiter->OosBmpByteCount,
								DRAID_BITMAP_POOL_TAG);
	if (pArbiter->LwrBmpBuffer == NULL) {
		ExFreePoolWithTag(pArbiter->OosBmpInCoreBuffer, DRAID_BITMAP_POOL_TAG);
		KDPrintM(DBG_LURN_ERROR, ("Failed to allocate OOS in memory bitmap\n"));
		ASSERT(FALSE);
		status =STATUS_INSUFFICIENT_RESOURCES;
		goto errout;
	}

	RtlZeroMemory(pArbiter->OosBmpInCoreBuffer, pArbiter->OosBmpByteCount);
	RtlZeroMemory(pArbiter->LwrBmpBuffer, pArbiter->OosBmpByteCount);
	RtlZeroMemory(pArbiter->OosBmpOnDiskBuffer, pArbiter->OosBmpSectorCount * SECTOR_SIZE);

	RtlInitializeBitMap(&pArbiter->OosBmpHeader, pArbiter->OosBmpInCoreBuffer, 
		pArbiter->OosBmpByteCount * 8);

	RtlInitializeBitMap(&pArbiter->LwrBmpHeader, pArbiter->LwrBmpBuffer, 
		pArbiter->OosBmpByteCount * 8);

	
	//
	// Read from UpToDateNode.
	// Assume UpToDateNode is non-spare and in-sync disk
	//
	ValidBitmapBlock = FALSE;

	if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[UpToDateNode]->LurnStatus) {
		KDPrintM(DBG_LURN_INFO, ("Lurn is not running. Cannot read bitmap from node %d\n", UpToDateNode));
		status = STATUS_UNSUCCESSFUL;
		goto errout;		
	}  else {
		status = LurnExecuteSyncRead(pArbiter->Lurn->LurnChildren[UpToDateNode], (PUCHAR)pArbiter->OosBmpOnDiskBuffer,
			NDAS_BLOCK_LOCATION_BITMAP, pArbiter->OosBmpSectorCount);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to read bitmap from node %d\n", UpToDateNode));
			goto errout;
		} else {
			ValidBitmapBlock = TRUE;
		}
	}
	
	if (ValidBitmapBlock == FALSE) {
		KDPrintM(DBG_LURN_INFO, ("Failed to read bitmap from any node. Set all bits\n"));
	} else {
		//
		// Check each sector for validity.
		//
		for(i=0;i<pArbiter->OosBmpSectorCount;i++) {
			if (pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead != pArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail) {
				KDPrintM(DBG_LURN_INFO, ("Bitmap sequence head/tail for sector %d mismatch %I64x:%I64x. Setting all dirty on this sector\n", 
					i, 
					pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead, 
					pArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail
				));
				ValidBitmapBlock = FALSE;
				break;
			}			
		}
	}
	if (ValidBitmapBlock == FALSE) {
		KDPrintM(DBG_LURN_INFO, ("OOS bitmap has fault. Set Set all bits\n"));
		for(i=0;i<pArbiter->OosBmpSectorCount;i++) {
			pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead = pArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = 0;
			RtlFillMemory(pArbiter->OosBmpOnDiskBuffer[i].Bits, sizeof(pArbiter->OosBmpOnDiskBuffer[i].Bits), 0x0ff);
			pArbiter->DirtyBmpSector[i] = TRUE;
		}
	}
	
	//
	// Convert on disk bitmap to in-memory bitmap
	//
	for(i=0;i<pArbiter->OosBmpSectorCount;i++) {
		KDPrintM(DBG_LURN_INFO, ("Bitmap block %d sequence num=%I64x\n", 
			i, pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead));
		if (i==pArbiter->OosBmpSectorCount-1) {
			// Last bitmap sector. 
			ByteCount = pArbiter->OosBmpByteCount%NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
		} else {
			ByteCount = NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
		}

		RtlCopyMemory(((PUCHAR)pArbiter->OosBmpInCoreBuffer)+DRAID_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(i,0), 
			pArbiter->OosBmpOnDiskBuffer[i].Bits, ByteCount);
	}
	status = STATUS_SUCCESS;

	// Check any bit is set.
	SetBits = RtlNumberOfSetBits(&pArbiter->OosBmpHeader);
	if (SetBits) {
		KDPrintM(DBG_LURN_INFO, ("OOS Bitmap is not clean: %d/%d is out-of-sync\n", 
			SetBits, pArbiter->OosBmpBitCount));

		if (pArbiter->OutOfSyncRole == NO_OUT_OF_SYNC_ROLE) {
			BOOLEAN NormalStatus;
			NormalStatus = TRUE;
			for(i = 0; i < pArbiter->ActiveDiskCount; i++) // i : role index.
			{
				if (!(pArbiter->NodeFlags[pArbiter->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING)) {
					NormalStatus = FALSE;
					pArbiter->OutOfSyncRole = (UCHAR)i;
					break;
				}
			}
			if (NormalStatus) {
				pArbiter->OutOfSyncRole = (UCHAR)(pArbiter->ActiveDiskCount-1);
				KDPrintM(DBG_LURN_INFO, 
					("All node is running without out of sync flag. Selecting %d as out-of-sync node\n",pArbiter->OutOfSyncRole));
			} else {
				KDPrintM(DBG_LURN_INFO, ("Some node is not running. Selecting %d as out-of-sync node\n",pArbiter->OutOfSyncRole));
			}
		}
	} else {
		KDPrintM(DBG_LURN_INFO, ("Bitmap is clean\n"));
	}

	// Make sure that all online disk has same bitmap
	DraidArbiterUpdateOnDiskOosBitmap(pArbiter, TRUE);
	
errout:
	return status;
}

// Called with pArbiter->SpinLock
VOID
DraidArbiterUpdateLwrBitmapBit(
	PDRAID_ARBITER_INFO pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT HintAddedLock,
	PDRAID_ARBITER_LOCK_CONTEXT HintRemovedLock
) {
	PLIST_ENTRY listEntry;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	UINT32 BitOffset;
	UINT32 NumberOfBit;
	ULONG LockCount =0;
	if (HintAddedLock && HintRemovedLock==NULL) {
		BitOffset = (UINT32)(HintAddedLock->LockAddress / pArbiter->SectorsPerOosBmpBit);
		NumberOfBit = (UINT32)((HintAddedLock->LockAddress + HintAddedLock->LockLength -1) / pArbiter->SectorsPerOosBmpBit - BitOffset + 1);
		KDPrintM(DBG_LURN_TRACE, ("Setting LWR bit %x:%x\n", BitOffset, NumberOfBit));
		RtlSetBits(&pArbiter->LwrBmpHeader, BitOffset, NumberOfBit);
		LockCount = 1;
	} else {
		// Recalc all lock
		RtlClearAllBits(&pArbiter->LwrBmpHeader);

		for (listEntry = pArbiter->AcquiredLockList.Flink;
			listEntry != &pArbiter->AcquiredLockList;
			listEntry = listEntry->Flink)
		{
			Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);

			BitOffset = (UINT32)(Lock->LockAddress / pArbiter->SectorsPerOosBmpBit);
			NumberOfBit = (UINT32)((Lock->LockAddress + Lock->LockLength -1) / pArbiter->SectorsPerOosBmpBit - BitOffset + 1);
//			KDPrintM(DBG_LURN_TRACE, ("Setting bit %x:%x\n", BitOffset, NumberOfBit));
			RtlSetBits(&pArbiter->LwrBmpHeader, BitOffset, NumberOfBit);
			LockCount++;
		}
	}
	KDPrintM(DBG_LURN_TRACE, ("Updated LWR bitmap with %d locks\n", LockCount));		
}

//
// Should be called with pArbiter->Spinlock locked.
//
VOID
DraidArbiterChangeOosBitmapBit(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN		Set,	// TRUE for set, FALSE for clear
	UINT64	Addr,
	UINT64	Length
) {
//	UINT32 i;
//	UINT32 ByteOffset;
//	UINT32 BmpSectorOffset;
//	UINT32 BmpByteOffset;
#if 0
	UINT32 PrevBits;
#endif
//	UINT32 ByteCount;
	UINT32 BitOffset;
	UINT32 NumberOfBit;

	ASSERT(KeGetCurrentIrql() ==  DISPATCH_LEVEL); // should be called with spinlock locked.
	
	BitOffset = (UINT32)(Addr / pArbiter->SectorsPerOosBmpBit);
	NumberOfBit = (UINT32)((Addr + Length -1) / pArbiter->SectorsPerOosBmpBit - BitOffset + 1);


//	KDPrintM(DBG_LURN_INFO, ("Before BitmapByte[0]=%x\n", pArbiter->OosBmpInCoreBuffer[0]));	
	if (Set) {
		KDPrintM(DBG_LURN_TRACE, ("Setting in-memory bitmap offset %x:%x\n", BitOffset, NumberOfBit));
		RtlSetBits(&pArbiter->OosBmpHeader, BitOffset, NumberOfBit);
	} else {
		KDPrintM(DBG_LURN_TRACE, ("Clearing in-memory bitmap offset %x:%x\n", BitOffset, NumberOfBit));
		RtlClearBits(&pArbiter->OosBmpHeader, BitOffset, NumberOfBit);
	}
//	KDPrintM(DBG_LURN_INFO, ("After BitmapByte[0]=%x\n", pArbiter->OosBmpInCoreBuffer[0]));	
#if 0
	//
	// Apply to on-disk bmp too.
	//
	if (BitOffset == 0 && NumberOfBit == pArbiter->OosBmpBitCount) {
		// Do it fast if all bits are requested to set
		KDPrintM(DBG_LURN_INFO, ("Applying all in-memory OOS bitmap to on-disk OOS\n"));
		for(i=0;i<pArbiter->OosBmpSectorCount;i++) {
			if (i==pArbiter->OosBmpSectorCount-1) {
				// Last bitmap sector. 
				ByteCount = pArbiter->OosBmpByteCount%NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
			} else {
				ByteCount = NDAS_BYTE_PER_OOS_BITMAP_BLOCK;
			}
			RtlCopyMemory(pArbiter->OosBmpOnDiskBuffer[i].Bits,
				pArbiter->OosBmpInCoreBuffer+DRAID_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(i,0),
				ByteCount);
			pArbiter->DirtyBmpSector[i] = TRUE;
		}
	} else {
		for(i=0;i<NumberOfBit;i++) {
			ByteOffset =  ((BitOffset + i)/(sizeof(ULONG)*8)) * sizeof(ULONG);
			ASSERT(ByteOffset<pArbiter->OosBmpByteCount);
			BmpSectorOffset = DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(ByteOffset);
			BmpByteOffset = DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(ByteOffset);
			ASSERT(BmpSectorOffset<pArbiter->OosBmpSectorCount);
#if 0
			PrevBits = (*((PUINT32)&pArbiter->OosBmpOnDiskBuffer[BmpSectorOffset].Bits[BmpByteOffset]));
#endif
			RtlCopyMemory(&pArbiter->OosBmpOnDiskBuffer[BmpSectorOffset].Bits[BmpByteOffset], 
				&pArbiter->OosBmpInCoreBuffer[ByteOffset],
				sizeof(ULONG));
#if 0
			KDPrintM(DBG_LURN_INFO, ("On-disk OOS bmp sector %d, offset %d has changed from %08x to %08x\n",
				BmpSectorOffset, BmpByteOffset,
				PrevBits, (*((PUINT32)&pArbiter->OosBmpOnDiskBuffer[BmpSectorOffset].Bits[BmpByteOffset]))
				));
#endif
			pArbiter->DirtyBmpSector[BmpSectorOffset] = TRUE;
		}
	}
#endif	
}

NTSTATUS 
DraidArbiterUpdateOnDiskOosBitmap(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN UpdateAll
) {
	NTSTATUS status = STATUS_SUCCESS; // in case for there is no dirty bitmap
	ULONG	i;
	KIRQL oldIrql;
	ULONG BitValues;
	ULONG Sector;
	ULONG Offset;
	
	// 
	//	Merge InCoreOosbitmap and LWR into Ondisk buffer.
	//	Set dirty flag if data is changed.
	//
	for(i=0;i<pArbiter->OosBmpByteCount/sizeof(ULONG);i++) {
		Sector = DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(i*sizeof(ULONG));
		Offset = DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(i*sizeof(ULONG));
		BitValues = pArbiter->OosBmpInCoreBuffer[i] | pArbiter->LwrBmpBuffer[i];
		if (*((PULONG)&pArbiter->OosBmpOnDiskBuffer[Sector].Bits[Offset]) == BitValues) {
			if (UpdateAll)  {
				pArbiter->DirtyBmpSector[Sector] = TRUE;
			}
			continue;
		} else {
			KDPrintM(DBG_LURN_TRACE, ("Bitmap offset %x changed from %08x to %08x\n", 
				i*4, *((PULONG)&pArbiter->OosBmpOnDiskBuffer[Sector].Bits[Offset]), BitValues
			));
		}
		*((PULONG)&pArbiter->OosBmpOnDiskBuffer[Sector].Bits[Offset]) = BitValues;
		pArbiter->DirtyBmpSector[Sector] = TRUE;
	}

	//
	// Update dirty bitmap sector only
	//
	for(i=0;i<pArbiter->OosBmpSectorCount;i++) {
		ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
		if (pArbiter->DirtyBmpSector[i]) {
			pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead++;
			pArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead;
			pArbiter->DirtyBmpSector[i] = FALSE;
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			KDPrintM(DBG_LURN_NOISE, ("Updating dirty bitmap sector %d, Seq = %I64x\n", i, pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead));
			
			status = DraidArbiterWriteMetaSync(pArbiter, (PUCHAR)&(pArbiter->OosBmpOnDiskBuffer[i]), 
				NDAS_BLOCK_LOCATION_BITMAP + i, 1);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_INFO, ("Failed to update dirty bitmap sector %d\n", i));	
				return status;
			}
		} else {
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		}
	}
	return status;
}

NTSTATUS 
DraidArbiterStart(
	PLURELATION_NODE	Lurn
	)
{
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	NTSTATUS ntStatus;
	PDRAID_ARBITER_INFO pArbiter;
	KIRQL oldIrql, oldIrql2;
	UINT32 i;
	UCHAR Flags;
	OBJECT_ATTRIBUTES objectAttributes;
	UINT32 UpToDateNode;

	if (pRaidInfo->pDraidArbiter) {
		KDPrintM(DBG_LURN_INFO, ("Aribter is already running\n"));
		return STATUS_SUCCESS;
	}
	
	pRaidInfo->pDraidArbiter = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_ARBITER_INFO),
		DRAID_ARBITER_INFO_POOL_TAG);
	if (NULL ==  pRaidInfo->pDraidArbiter) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	pArbiter = pRaidInfo->pDraidArbiter;
	RtlZeroMemory(pArbiter, sizeof(DRAID_ARBITER_INFO));

	KeInitializeSpinLock(&pArbiter->SpinLock);

	pArbiter->Lurn = Lurn;
	pArbiter->Status = DRAID_ARBITER_STATUS_INITALIZING;

	pArbiter->RaidStatus = DRIX_RAID_STATUS_INITIALIZING;

	InitializeListHead(&pArbiter->ClientList);
	InitializeListHead(&pArbiter->AcquiredLockList);
	InitializeListHead(&pArbiter->ToYieldLockList);
	InitializeListHead(&pArbiter->PendingLockList);
	InitializeListHead(&pArbiter->AllArbiterList);
	
#if 0
	pArbiter->FlushAllRequested = FALSE;
	pArbiter->FlushingInProgress = FALSE;
	pArbiter->FlushingDoneEvent = NULL;
#endif

	pArbiter->AcquiredLockCount = 0;
	pArbiter->TotalDiskCount = Lurn->LurnChildrenCnt;
	pArbiter->ActiveDiskCount = Lurn->LurnChildrenCnt - pRaidInfo->nSpareDisk;

	pArbiter->LockRangeGranularity = pRaidInfo->SectorsPerBit; // Set to sector per bit for time being..

	pArbiter->SectorsPerOosBmpBit = pRaidInfo->SectorsPerBit;

	pArbiter->OutOfSyncRole = NO_OUT_OF_SYNC_ROLE;

	RtlCopyMemory(&pArbiter->RaidSetId, &pRaidInfo->RaidSetId, sizeof(GUID));
	RtlCopyMemory(&pArbiter->ConfigSetId, &pRaidInfo->ConfigSetId, sizeof(GUID));	
		
#if 0
	RtlInitializeBitMap(&pArbiter->RebuildInfo.AggressiveRebuildMapHeader, 
		(PULONG)pArbiter->RebuildInfo.AggressiveRebuildMapBuffer, 
		DRAID_Aggressive_REBUILD_FRAGMENT);
#endif
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
	ntStatus = LurnRMDRead(Lurn,&pArbiter->Rmd, &UpToDateNode);

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("**** **** RAID_STATUS_FAIL **** ****\n"));
		ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
		pArbiter->Status = DRAID_ARBITER_STATUS_TERMINATING;
		pArbiter->RaidStatus = DRIX_RAID_STATUS_FAILED;
		RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		goto fail;
	}

	//
	// 2. Set initial arbiter flags. 
	// 
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);	
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		ACQUIRE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, &oldIrql2);
		switch(Lurn->LurnChildren[i]->LurnStatus) {
		case LURN_STATUS_RUNNING:
			Flags = DRIX_NODE_FLAG_RUNNING;
			break;
		case LURN_STATUS_INIT:
		case LURN_STATUS_STALL:
		case LURN_STATUS_STOP_PENDING:
		case LURN_STATUS_STOP:
		case LURN_STATUS_DESTROYING:
			Flags = DRIX_NODE_FLAG_STOP;
			break;
		default:
			ASSERT(FALSE);
			Flags = DRIX_NODE_FLAG_UNKNOWN;
			break;
		}
		if (LurnGetCauseOfFault(Lurn->LurnChildren[i]) & (LURN_FCAUSE_BAD_SECTOR|LURN_FCAUSE_BAD_DISK)) {
			Flags |= DRIX_NODE_FLAG_DEFECTIVE;
			pArbiter->DefectCodes[i] |= ((LurnGetCauseOfFault(Lurn->LurnChildren[i]) & LURN_FCAUSE_BAD_SECTOR)?
				DRIX_NODE_DEFECT_BAD_SECTOR:DRIX_NODE_DEFECT_BAD_DISK);
		}
		pArbiter->NodeFlags[i] = Flags;
		KDPrintM(DBG_LURN_ERROR, ("Setting initial node %d flag: %d\n",
			i, pArbiter->NodeFlags[i]));
		RELEASE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, oldIrql2);
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);


	//
	// 3. Map children based on RMD
	//
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);	
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)	// i is role index
	{
		KDPrintM(DBG_LURN_ERROR, ("MAPPING Lurn node %d to RAID role %d\n",
			pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx, i));
		pArbiter->RoleToNodeMap[i] = (UCHAR)pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx;
		pArbiter->NodeToRoleMap[pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx] = (UCHAR)i;	
		ASSERT(pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx <Lurn->LurnChildrenCnt);
	}

	//
	// 4. Apply node information from RMD
	//

	
	for(i = 0; i < Lurn->LurnChildrenCnt; i++) // i : role index.
	{
		UCHAR UnitDeviceStatus = pArbiter->Rmd.UnitMetaData[i].UnitDeviceStatus;
		UCHAR NodeIndex = pArbiter->RoleToNodeMap[i];
		if(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & UnitDeviceStatus)
		{
			if (i<pArbiter->ActiveDiskCount) {
				pArbiter->OutOfSyncRole = (UCHAR)i;
				KDPrintM(DBG_LURN_ERROR, ("Node %d(role %d) is out-of-sync\n",  NodeIndex, i));
				KDPrintM(DBG_LURN_INFO, ("Setting out of sync role: %d\n", pArbiter->OutOfSyncRole));
			}
		}
		if(NDAS_UNIT_META_BIND_STATUS_DEFECTIVE & UnitDeviceStatus)
		{
			pArbiter->NodeFlags[NodeIndex] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING|DRIX_NODE_FLAG_STOP);
			pArbiter->NodeFlags[NodeIndex] |= DRIX_NODE_FLAG_DEFECTIVE;
		
			// fault device found
			KDPrintM(DBG_LURN_ERROR, ("Node %d(role %d) is defective\n",  NodeIndex, i));
			pArbiter->DefectCodes[NodeIndex] |= DraidRmdUnitStatusToDefectCode(UnitDeviceStatus);
		}
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	//
	// 5. Read bitmap.
	// 
	ntStatus = DradArbiterInitializeOosBitmap(pArbiter, UpToDateNode);

	//
	// 6. Set initial RAID status.
	//
	DraidArbiterRefreshRaidStatus(pArbiter, TRUE);


#if 0	
	//
	// 7. Search valid LWR and process LWR
	//

	ntStatus = DraidArbiterProcessDirtyLwr(pArbiter, UpToDateNode);

	//
	//	DraidArbiterProcessDirtyLwr may have changed node status.
	//
	DraidArbiterRefreshRaidStatus(pArbiter, TRUE);
#endif

	//
	// Create Arbiter thread
	//
	KeInitializeEvent(&pArbiter->ArbiterThreadEvent, NotificationEvent, FALSE);
		
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);
	
	ntStatus = PsCreateSystemThread(
		&pArbiter->ThreadArbiterHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		DraidArbiterThreadProc,
		Lurn
		);

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL : DraidArbiterStart !!!\n"));
		ntStatus = STATUS_THREAD_NOT_IN_PROCESS;
		goto fail;
	}

	ntStatus = ObReferenceObjectByHandle(
		pArbiter->ThreadArbiterHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		&pArbiter->ThreadArbiterObject,
		NULL
		);

	if(!NT_SUCCESS(ntStatus))
	{
		ExFreePoolWithTag(pRaidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG);
		pRaidInfo->pDraidArbiter = NULL;		
		KDPrintM(DBG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL : DraidArbiterStart !!!\n"));
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	return STATUS_SUCCESS;
fail:
	KDPrintM(DBG_LURN_ERROR, ("!!! Arbiter failed to start\n"));
	if (pRaidInfo->pDraidArbiter) {
		ExFreePoolWithTag(pRaidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG);
		pRaidInfo->pDraidArbiter = NULL;
	}

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS
DraidArbiterStop(
	PLURELATION_NODE	Lurn
	)
{
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	PDRAID_ARBITER_INFO pArbiterInfo = pRaidInfo->pDraidArbiter;
	NTSTATUS status;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;

	if (!pArbiterInfo) {
		return STATUS_SUCCESS;
	}
	ASSERT(pArbiterInfo);
	KDPrintM(DBG_LURN_INFO, ("Stopping DRAID arbiter\n"));

	DraidUnregisterArbiter(pArbiterInfo);
	
	if(pArbiterInfo->ThreadArbiterHandle)
	{
		ACQUIRE_SPIN_LOCK(&pArbiterInfo->SpinLock, &oldIrql);

		pArbiterInfo->StopRequested = TRUE;
		KeSetEvent(&pArbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE); // This will wake up Arbiter thread.
		RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);

		KDPrintM(DBG_LURN_INFO, ("Wait for Arbiter thread completion\n"));

		status = KeWaitForSingleObject(
			pArbiterInfo->ThreadArbiterObject,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);

		if (status != STATUS_SUCCESS) {
			ASSERT(FALSE);
		} else {
			KDPrintM(DBG_LURN_INFO, ("Arbiter thread exited\n"));
		}

		//
		//	Dereference the thread object.
		//

		ObDereferenceObject(pArbiterInfo->ThreadArbiterObject);
		ZwClose(pArbiterInfo->ThreadArbiterHandle);
		ACQUIRE_SPIN_LOCK(&pArbiterInfo->SpinLock, &oldIrql);

		pArbiterInfo->ThreadArbiterObject = NULL;
		pArbiterInfo->ThreadArbiterHandle = NULL;

		RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);
	}

	while(TRUE)
	{
		listEntry = RemoveHeadList(&pArbiterInfo->AcquiredLockList);
		if (listEntry == &pArbiterInfo->AcquiredLockList) {
			break;
		}
		Lock = CONTAINING_RECORD(
				listEntry,
				DRAID_ARBITER_LOCK_CONTEXT,
				ArbiterAcquiredLink
		);
		InterlockedDecrement(&pArbiterInfo->AcquiredLockCount);
		RemoveEntryList(&Lock->ToYieldLink);
		InitializeListHead(&Lock->ToYieldLink); // to check bug...
//		KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",pArbiterInfo->AcquiredLockCount));		
		DraidArbiterFreeLock(pArbiterInfo, Lock);
	}

#if 0	// to do: clean up remote clients
	while(TRUE) 
	{
		listEntry = RemoveHeadList(&pArbiterInfo->ClientList);
		DraidArbiterTerminateClient(Arbiter, Client, FALSE);
	}
#endif
	ASSERT(InterlockedCompareExchange(&pArbiterInfo->AcquiredLockCount, 0, 0)==0);

	ExFreePoolWithTag(pArbiterInfo->OosBmpInCoreBuffer, DRAID_BITMAP_POOL_TAG);
	ExFreePoolWithTag(pArbiterInfo->LwrBmpBuffer, DRAID_BITMAP_POOL_TAG);	
	ExFreePoolWithTag(pArbiterInfo->OosBmpOnDiskBuffer, DRAID_BITMAP_POOL_TAG);
	ExFreePoolWithTag(pRaidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG);
	pRaidInfo->pDraidArbiter = NULL;

	return STATUS_SUCCESS;
}

#if 0
// Flush will be done in asynchronous way.
NTSTATUS
DraidArbiterFlushAllClient(
	PDRAID_ARBITER_INFO pArbiterInfo
) {
	KDPrintM(DBG_LURN_INFO, ("Requesting flush all\n")); 
	pArbiterInfo->FlushAllRequested = TRUE;
	KeSetEvent(&pArbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE);
}
#endif

PDRAID_CLIENT_CONTEXT DraidArbiterAllocClientContext(
	void
	)
{
	PDRAID_CLIENT_CONTEXT pClientContext;
	
	pClientContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_CLIENT_CONTEXT),
		DRAID_CLIENT_CONTEXT_POOL_TAG);
	if (NULL ==  pClientContext ) {
		return NULL;
	}

	RtlZeroMemory(pClientContext, sizeof(DRAID_CLIENT_CONTEXT));

	InitializeListHead(&pClientContext->Link);
	InitializeListHead(&pClientContext->AcquiredLockList);

	KeInitializeEvent(&pClientContext->UnregisterDoneEvent, NotificationEvent, FALSE);
	
	DRIX_INITIALIZE_LOCAL_CHANNEL(&pClientContext->RequestChannel);
	DRIX_INITIALIZE_LOCAL_CHANNEL(&pClientContext->NotificationReplyChannel);

	InitializeListHead(&pClientContext->AcquiredLockList);
	
	return pClientContext;
}

//
// Should be called by out of arbiter thread.
//
NTSTATUS
DraidArbiterUnregisterClient(
	PDRAID_ARBITER_INFO pArbiterInfo,
	PDRAID_CLIENT_CONTEXT pClientContext
) {
	KIRQL	oldIrql;		
	ACQUIRE_SPIN_LOCK(&pArbiterInfo->SpinLock, &oldIrql);
	pClientContext->UnregisterRequest = TRUE;
	RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);

	KeSetEvent(pArbiterInfo->LocalClientTerminatingEvent, IO_NO_INCREMENT, FALSE);
	
	//
	// Wakeup arbiter to handle disappearance of local client.
	//
	KeSetEvent(&pArbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE);

	KeWaitForSingleObject(
		&pClientContext->UnregisterDoneEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL
	);
	KDPrintM(DBG_LURN_INFO, ("Freeing client context\n"));
	ExFreePoolWithTag(pClientContext, DRAID_CLIENT_CONTEXT_POOL_TAG);
	return STATUS_SUCCESS;
}

//
// Can't start receiving here. If we start receive here, this receiving will be cancelled when thread reception thread exits
//
NTSTATUS
DraidArbiterAcceptClient(
	PDRAID_ARBITER_INFO Arbiter,
	UCHAR ConnType,
	PDRAID_REMOTE_CLIENT_CONNECTION Connection
) {
	PLIST_ENTRY listEntry;
	PDRAID_CLIENT_CONTEXT Client;
	KIRQL oldIrql;
	BOOLEAN ClientExist;

	Connection->ConnType = ConnType;

	if (ConnType != DRIX_CONN_TYPE_NOTIFICATION &&
		ConnType != DRIX_CONN_TYPE_REQUEST
	) {
		ASSERT(FALSE);
		return STATUS_UNSUCCESSFUL;
	}
	
	//
	// Find client with this address, if not found, create it.
	//
	ClientExist = FALSE;
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	for (listEntry = Arbiter->ClientList.Flink;
		listEntry != &Arbiter->ClientList;
		listEntry = listEntry->Flink) 
	{
		Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
		if (RtlCompareMemory(Client->RemoteClientAddr, Connection->RemoteAddr.Node, 6) == 6) {
			ClientExist = TRUE;
			break;
		}
	}
	// Check whether same connection is already exists.
	if (ClientExist) {
		if (ConnType == DRIX_CONN_TYPE_NOTIFICATION) {
			if (Client->NotificationConnection) {
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				KDPrintM(DBG_LURN_INFO, ("Notification connection already exist for this client. Unregister previous client\n"));
				DraidArbiterUnregisterClient(Arbiter, Client);
				ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
				ClientExist = FALSE;
			}
		} else if (ConnType == DRIX_CONN_TYPE_REQUEST) {
			if (Client->RequestConnection) {
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				KDPrintM(DBG_LURN_INFO, ("Request connection already exist for this client. Unregister previous client\n"));
				DraidArbiterUnregisterClient(Arbiter, Client);
				ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
				ClientExist = FALSE;
			}
		} 
	} 
	if (!ClientExist) {
		Client = DraidArbiterAllocClientContext();
		if (!Client) {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);			
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	if (ConnType == DRIX_CONN_TYPE_NOTIFICATION) {
		Client->NotificationConnection = Connection;
	} else if (ConnType == DRIX_CONN_TYPE_REQUEST) {
		Client->RequestConnection = Connection;
	}
	// Add to list if this is a new client
	if (!ClientExist) {
		RtlCopyMemory(Client->RemoteClientAddr, Connection->RemoteAddr.Node, 6);
		InsertTailList(&Arbiter->ClientList, &Client->Link);
	}
	KDPrintM(DBG_LURN_INFO, ("Accepted client %02X:%02X:%02X:%02X:%02X:%02X\n", 
		Client->RemoteClientAddr[0], Client->RemoteClientAddr[1], Client->RemoteClientAddr[2],
		Client->RemoteClientAddr[3], Client->RemoteClientAddr[4], Client->RemoteClientAddr[5]
	));
	
	// Wakeup arbiter to handle this new client
	KeSetEvent(&Arbiter->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE);

	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	return STATUS_SUCCESS;
}

//
//	Link local client to arbiter
//	Counter to accepting connection from remote client.
//
NTSTATUS
DraidRegisterLocalClientToArbiter(
	PLURELATION_NODE	Lurn,
	PDRAID_CLIENT_INFO	pClient
)
{
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	PDRAID_ARBITER_INFO pArbiterInfo = pRaidInfo->pDraidArbiter;
	PDRAID_CLIENT_CONTEXT pClientContext;
	KIRQL oldIrql;
	ASSERT(pRaidInfo);
	
	KDPrintM(DBG_LURN_INFO, ("Registering local client\n"));

	if (pArbiterInfo == NULL) {
		KDPrintM(DBG_LURN_INFO, ("Failed to register local client. Local arbiter does not exist\n"));
		return STATUS_UNSUCCESSFUL;
	} 
	
	ACQUIRE_SPIN_LOCK(&pArbiterInfo->SpinLock, &oldIrql);
	if (pArbiterInfo->Status == DRAID_ARBITER_STATUS_TERMINATING) {
		KDPrintM(DBG_LURN_INFO, ("Arbiter terminated.\n"));
		RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);
		return STATUS_UNSUCCESSFUL;
	} else if (pArbiterInfo->Status != DRAID_ARBITER_STATUS_RUNNING) {
		KDPrintM(DBG_LURN_INFO, ("Failed to register local client. Local arbiter is not running status.\n"));
		RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);
		return STATUS_RETRY;
	}
	RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);		

	pClientContext = DraidArbiterAllocClientContext();
	if (pClientContext == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	
	// Init LocalClient values
	KeInitializeEvent(&pClient->LocalClientTerminating, NotificationEvent, FALSE);
	
	pClientContext->NotificationChannel = &pClient->NotificationChannel;
	pClientContext->RequestReplyChannel = &pClient->RequestReplyChannel;
	pClientContext->LocalClient = pClient;
	pClient->RequestChannel = &pClientContext->RequestChannel;
	pClient->NotificationReplyChannel = &pClientContext->NotificationReplyChannel;

	pArbiterInfo->LocalClient = pClientContext;
	pArbiterInfo->LocalClientTerminatingEvent = &pClient->LocalClientTerminating;
		
	// Link to client list
	ExInterlockedInsertTailList(&pArbiterInfo->ClientList, &pClientContext->Link, &pArbiterInfo->SpinLock);

	// Set status before notifying arbiter.
	ACQUIRE_SPIN_LOCK(&pClient->SpinLock, &oldIrql);
	pClient->ClientStatus = DRAID_CLIENT_STATUS_ARBITER_CONNECTED;
	pClient->HasLocalArbiter = TRUE;
	RELEASE_SPIN_LOCK(&pClient->SpinLock, oldIrql);

	// Wakeup arbiter to handle this new client
	KeSetEvent(&pArbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE);
	KDPrintM(DBG_LURN_INFO, ("Registered local client\n"));
	return STATUS_SUCCESS;
}


NTSTATUS
DraidUnregisterLocalClient(
	PLURELATION_NODE	Lurn
)
{
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	PDRAID_ARBITER_INFO pArbiterInfo = pRaidInfo->pDraidArbiter;
	PDRAID_CLIENT_CONTEXT pClientContext = pArbiterInfo->LocalClient;
	ASSERT(pClientContext);
	KDPrintM(DBG_LURN_INFO, ("Unregistering local client\n"));
	if (pArbiterInfo == NULL) {
		KDPrintM(DBG_LURN_INFO, ("Local arbiter does not exist.\n"));
	} else {
		DraidArbiterUnregisterClient(pArbiterInfo, pClientContext);
	}
	return STATUS_SUCCESS;
}

#define DRAID_ADDR_REQUIRES_CDB16(BlockAddr) (BlockAddr >= 0x100000000LL)

static
NTSTATUS DraidDoRebuildIo(
	PDRAID_ARBITER_INFO pArbiter,
	UINT64	Addr,
	UINT32 	Length
) {
	PDRAID_REBUILD_INFO RebuildInfo = &pArbiter->RebuildInfo;
	KIRQL	oldIrql;
	NTSTATUS status;
	PLURELATION_NODE		LurnOutOfSync;
	PLURELATION_NODE		LurnsHealthy[MAX_DRAID_MEMBER_DISK];
	UINT32	i,j;
	UINT32 Node;
	PUCHAR	TargetBuf;
	// Check current RAID status is suitable.
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	if (pArbiter->RaidStatus != DRIX_RAID_STATUS_REBUILDING) {
		RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("Cannot do rebuilding IO. RAID is not in rebuilding state\n"));
		return STATUS_UNSUCCESSFUL;
	}
	if (pArbiter->OutOfSyncRole == NO_OUT_OF_SYNC_ROLE) {
		RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("No out-of-sync node\n"));
		return STATUS_UNSUCCESSFUL;
	}
		
	//
	// Check cancel or terminate request is sent
	//
	if (RebuildInfo->CancelRequested || RebuildInfo->ExitRequested) {
		RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("Rebuild IO is cancelled or exited.\n"));		
		return STATUS_UNSUCCESSFUL;
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	// 
	// Find defective disk and setup healthy LURN
	//
	LurnOutOfSync = pArbiter->Lurn->LurnChildren[pArbiter->RoleToNodeMap[pArbiter->OutOfSyncRole]];
	j = 0;
	for(i=0;i<pArbiter->ActiveDiskCount;i++) { // i: role index
		Node = pArbiter->RoleToNodeMap[i];
		if (i == pArbiter->OutOfSyncRole) {
			continue;
		}
		LurnsHealthy[j] = pArbiter->Lurn->LurnChildren[Node];
		j++;
	}
	
	if (j!=pArbiter->ActiveDiskCount-1) {
		KDPrintM(DBG_LURN_INFO, ("Invalid healthy disk count: healthy=%d\n", j));
		return STATUS_UNSUCCESSFUL;
	}
	
	//
	// READ sectors from the healthy LURN.
	//
	
	status = LurnExecuteSyncMulti(
		pArbiter->ActiveDiskCount-1,
		LurnsHealthy,
		DRAID_ADDR_REQUIRES_CDB16(Addr)?SCSIOP_READ16:SCSIOP_READ,
		RebuildInfo->RebuildBuffer,
		Addr,
		(UINT16)Length,
		NULL);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR,
			("Failed to read from healthy disk. RAID failure\n"));
		if (pArbiter->LocalClient && pArbiter->LocalClient->LocalClient) {
			// Report to local client.
			DraidClientUpdateAllNodeFlags(pArbiter->LocalClient->LocalClient);
		} else {
			// Local client may be already stopped due to error.
		}
		return STATUS_UNSUCCESSFUL;
	}


	if(LURN_RAID4R == pArbiter->Lurn->LurnType)
	{
		ASSERT(FALSE); // not implemented.
#if 0
		// parity work
		RtlCopyMemory(buf_write_to_fault, bufs_read_from_healthy[0], buf_size_each_child);
		for(i = 1; i < nDiskCount -1; i++)
		{
			parity_tar_ptr = (PULONG)buf_write_to_fault;
			parity_src_ptr = (PULONG)bufs_read_from_healthy[i];

			// always do parity for all range (ok even if sectors_to_recover < SectorsPerBit
			j = (buf_size_each_child) / sizeof(ULONG);
			while(j--)
			{
				*parity_tar_ptr ^= *parity_src_ptr;
				parity_tar_ptr++;
				parity_src_ptr++;
			}
		}
#endif		
	}else if (LURN_RAID1R == pArbiter->Lurn->LurnType) {
		TargetBuf = RebuildInfo->RebuildBuffer[0];
	}else {
		ASSERT(FALSE);
	}

	//
	// WRITE sectors to the out-of-sync LURN
	//
	status = LurnExecuteSyncMulti(
		1,
		&LurnOutOfSync,
		DRAID_ADDR_REQUIRES_CDB16(Addr)?SCSIOP_WRITE16:SCSIOP_WRITE,
		&TargetBuf,
		Addr,
		(UINT16)Length,
		NULL);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_INFO, ("Failed to write to out-of-sync disk\n"));
		if (pArbiter->LocalClient && pArbiter->LocalClient->LocalClient) {
			// Report to local client.
			DraidClientUpdateNodeFlags(pArbiter->LocalClient->LocalClient, LurnOutOfSync, 0, 0);
		} else {
			KDPrintM(DBG_LURN_INFO, ("Local client is not available\n"));
		}
		return status;
	}
	return STATUS_SUCCESS;
}


//
// Return unsuccessful if failed to lock or exit requested
//
NTSTATUS 
DraidRebuildIoLockRangeAggressive(
	PDRAID_ARBITER_INFO pArbiter, 
	UINT64 StartAddr,
	UINT32 Length
) {
	UINT64 OverlapStart, OverlapEnd;
	PDRAID_REBUILD_INFO RebuildInfo = &pArbiter->RebuildInfo;
	KIRQL oldIrql;
	NTSTATUS status;
	PLIST_ENTRY listEntry;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	LARGE_INTEGER Timeout;
	
	// In aggressive rebuild mode, lock required range only.
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	if (RebuildInfo->RebuildLock) {		
		// Check previous lock cover this range. If not, release it.
		if (DraidGetOverlappedRange(StartAddr, Length,
			pArbiter->RebuildInfo.RebuildLock->LockAddress, 
			pArbiter->RebuildInfo.RebuildLock->LockLength, 
			&OverlapStart, &OverlapEnd) != DRAID_RANGE_SRC2_CONTAINS_SRC1) {
			KDPrintM(DBG_LURN_TRACE, ("Aggressive rebuild lock %I64x:%x does not cover range %I64x:%x\n",
				pArbiter->RebuildInfo.RebuildLock->LockAddress, 
				pArbiter->RebuildInfo.RebuildLock->LockLength,
				StartAddr, Length));
			RemoveEntryList(&pArbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink);
			RemoveEntryList(&pArbiter->RebuildInfo.RebuildLock->ToYieldLink);
			InitializeListHead(&pArbiter->RebuildInfo.RebuildLock->ToYieldLink);						
			InterlockedDecrement(&pArbiter->AcquiredLockCount);
			DraidArbiterFreeLock(pArbiter, pArbiter->RebuildInfo.RebuildLock);
			pArbiter->RebuildInfo.RebuildLock = NULL;
		}					
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
	
	if (!pArbiter->RebuildInfo.RebuildLock) {
		pArbiter->RebuildInfo.RebuildLock = DraidArbiterAllocLock(pArbiter, 
			DRIX_LOCK_TYPE_BLOCK_IO,
			DRIX_LOCK_MODE_EX,
			StartAddr, Length); 
		if (!pArbiter->RebuildInfo.RebuildLock) {
			status = STATUS_INSUFFICIENT_RESOURCES;
		} else {
			while(TRUE) {
				ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
				if (RebuildInfo->ExitRequested)  {
					RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
					KDPrintM(DBG_LURN_INFO, ("Exit requested while taking aggressive rebuild lock\n"));
					status = STATUS_UNSUCCESSFUL;
					break;
				}

				status = DraidArbiterArrangeLockRange(pArbiter, 
					pArbiter->RebuildInfo.RebuildLock, DRAID_AGGRESSIVE_REBUILD_SIZE, TRUE);
				if (NT_SUCCESS(status)) {
					
					// Add lock to acquired lock list
					pArbiter->RebuildInfo.RebuildLock->Status = DRAID_ARBITER_LOCK_STATUS_GRANTED;
					InsertTailList(&pArbiter->AcquiredLockList, &pArbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink);
					InterlockedIncrement(&pArbiter->AcquiredLockCount);
					RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
					status = STATUS_SUCCESS;
					KDPrintM(DBG_LURN_INFO, ("Aggressive rebuild lock %I64x:%x is taken for range %I64x:%x\n",
						pArbiter->RebuildInfo.RebuildLock->LockAddress, 
						pArbiter->RebuildInfo.RebuildLock->LockLength,
						StartAddr, Length));
					DraidArbiterUpdateLwrBitmapBit(pArbiter, pArbiter->RebuildInfo.RebuildLock, NULL);
					DraidArbiterUpdateOnDiskOosBitmap(pArbiter, FALSE);
					break;
				} else {
					// No matching rebuild lock. 
					KDPrintM(DBG_LURN_INFO, ("Failed to take aggressive rebuild lock for range %I64x:%x\n",
						StartAddr, Length));
					// Add overlapping lock to yield list if not already in the list
					for (listEntry = pArbiter->AcquiredLockList.Flink;
						listEntry != &pArbiter->AcquiredLockList;
						listEntry = listEntry->Flink) 
					{
						Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
						if (DraidGetOverlappedRange(StartAddr, Length,
							Lock->LockAddress, Lock->LockLength, 
							&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
							// Client has lock. Queue this lock to to-yield-list. This will be handled by Arbiter thread later.
							if (IsListEmpty(&Lock->ToYieldLink)) {
								InsertTailList(&pArbiter->ToYieldLockList, &Lock->ToYieldLink);
							}
						}
					}
					RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
					// Wait for a while..
					// To fix: this thread may be unable to take lock if client repeatedly acquire the range after releasing.
					//		Need to add rebuild lock to pending list.
					// Use current policy if there is no big problem...
					Timeout.QuadPart = - HZ * 2;
					KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
				}
			}
		}
	}else {
		// Current lock covers requested range.
		status = STATUS_SUCCESS;
	}
	
	return status;
}

#define DRAID_REBUILD_REST_INTERVAL 2048	// number of IO blocks wait. Wait every 1Mbyte
#define DRAID_REBUILD_REST_TIME	((HZ/1000)*10)			// 10 msec

//
// Handle rebuild IO request.
//	Wait for arbiter send rebuild request. Stop rebuild when arbiter request cancel 
//	Signal arbiter when rebuild is completed or cancelled.
//
//	to do: limit rebuild speed.
//
VOID
DraidRebuidIoThreadProc(
	IN	PVOID	Context
	)
{
	PDRAID_ARBITER_INFO pArbiter = (PDRAID_ARBITER_INFO) Context;
	PDRAID_REBUILD_INFO RebuildInfo = &pArbiter->RebuildInfo;
	NTSTATUS status;
	KIRQL	oldIrql;
	BOOLEAN DoMore;

	// Set lower priority.
	KeSetBasePriorityThread(KeGetCurrentThread(), -1);
	
	DoMore = TRUE;
	while(TRUE) {
		if (DoMore) {
			DoMore = FALSE;
		} else {
			ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);
			KDPrintM(DBG_LURN_TRACE, ("Waiting rebuild request\n"));
			status = KeWaitForSingleObject(
				&RebuildInfo->ThreadEvent,
				Executive,
				KernelMode,
				FALSE,
				NULL
			);
			KeClearEvent(&RebuildInfo->ThreadEvent);
		}
		ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
		if (RebuildInfo->ExitRequested) {
			KDPrintM(DBG_LURN_INFO, ("Rebuild exit requested\n"));
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);			
			break;
		}
		if (RebuildInfo->RebuildRequested) {	
			UINT64 CurAddr, EndAddr;
			UINT32 IoLength;
			UINT32 RestInterval;
			
			ASSERT(RebuildInfo->Length);
			
			// Rebuild request should be called only when previous result is cleared.
			ASSERT(RebuildInfo->Status == DRAID_REBUILD_STATUS_NONE);
			RebuildInfo->Status = DRAID_REBUILD_STATUS_WORKING;
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			DoMore = TRUE;

			CurAddr =  RebuildInfo->Addr;
			EndAddr = RebuildInfo->Addr + RebuildInfo->Length;
			KDPrintM(DBG_LURN_TRACE, ("Starting rebuild IO from %I64x to %I64x\n",
				CurAddr, EndAddr-1));
			RestInterval = 0;
			while(TRUE) {
				if (EndAddr - CurAddr > RebuildInfo->UnitRebuildSize/SECTOR_SIZE) {
					IoLength = RebuildInfo->UnitRebuildSize/SECTOR_SIZE;
				} else {
					IoLength = (UINT32)(EndAddr - CurAddr);
				}
				if (pArbiter->RebuildInfo.AggressiveRebuildMode) {
					// In aggressive mode, take lock on demand and aggressively.
					status = DraidRebuildIoLockRangeAggressive(pArbiter, CurAddr, IoLength);
					if (!NT_SUCCESS(status)) {
						break;
					}
				}
				status = DraidDoRebuildIo(pArbiter, CurAddr, IoLength);
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
					
				ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
				if (RebuildInfo->ExitRequested) {
					RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
					status = STATUS_UNSUCCESSFUL;
					break;
				}
				RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			}
			
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			RebuildInfo->RebuildRequested = FALSE;
			if (RebuildInfo->CancelRequested) {
				KDPrintM(DBG_LURN_INFO, ("Rebuilding range %I64x:%x cancelled\n", RebuildInfo->Addr, RebuildInfo->Length));
				RebuildInfo->Status = DRAID_REBUILD_STATUS_CANCELLED;
			} else if (RebuildInfo->ExitRequested) {
				KDPrintM(DBG_LURN_INFO, ("Exit requested. Rebuilding range %I64x:%x cancelled\n", RebuildInfo->Addr, RebuildInfo->Length));
				RebuildInfo->Status = DRAID_REBUILD_STATUS_CANCELLED;
				RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);	
				break;
			} else if (NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_INFO, ("Rebuilding range %I64x:%x done\n", RebuildInfo->Addr, RebuildInfo->Length));
				RebuildInfo->Status = DRAID_REBUILD_STATUS_DONE;
			}else {
				KDPrintM(DBG_LURN_INFO, ("Rebuilding range %I64x:%x failed\n", RebuildInfo->Addr, RebuildInfo->Length));
				RebuildInfo->Status = DRAID_REBUILD_STATUS_FAILED;
			}
			// Notify arbiter.
			KeSetEvent(&pArbiter->ArbiterThreadEvent, IO_NO_INCREMENT, FALSE);
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		} else {
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		}
	}
	KDPrintM(DBG_LURN_INFO, ("Exiting\n"));
	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);
	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS
DraidRebuildIoStart(
	PDRAID_ARBITER_INFO pArbiter
)
{
	NTSTATUS	status;
	PDRAID_REBUILD_INFO RebuildInfo = &pArbiter->RebuildInfo;
	OBJECT_ATTRIBUTES objectAttributes;
	UINT32 i;

	KeInitializeEvent(&RebuildInfo->ThreadEvent, NotificationEvent, FALSE);

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	RebuildInfo->Status = DRAID_REBUILD_STATUS_NONE;

	RebuildInfo->RebuildLock = NULL;
	RebuildInfo->ExitRequested = FALSE;
	RebuildInfo->CancelRequested = FALSE;
	RebuildInfo->UnitRebuildSize = DRAID_REBUILD_BUFFER_SIZE;

	for(i=0;i<pArbiter->ActiveDiskCount-1;i++) {
		RebuildInfo->RebuildBuffer[i] = ExAllocatePoolWithTag(NonPagedPool, RebuildInfo->UnitRebuildSize, DRAID_REBUILD_BUFFER_POOL_TAG);
		if (!RebuildInfo->RebuildBuffer[i]) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto errout;
		}
	}
	
	status = PsCreateSystemThread(
		&RebuildInfo->ThreadHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		DraidRebuidIoThreadProc,
		pArbiter
		);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL : DraidRebuildIoStart !!!\n"));
		goto errout;
	}

	status = ObReferenceObjectByHandle(
		RebuildInfo->ThreadHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		&RebuildInfo->ThreadObject,
		NULL
		);

	if(!NT_SUCCESS(status))
	{
		goto errout;
	}
	return STATUS_SUCCESS;
errout:
	for(i=0;i<pArbiter->ActiveDiskCount-1;i++) {
		if (RebuildInfo->RebuildBuffer[i]) {
			ExFreePoolWithTag(RebuildInfo->RebuildBuffer[i], DRAID_REBUILD_BUFFER_POOL_TAG);
		}
	}
	return status;
}

NTSTATUS
DraidRebuildIoStop(
	PDRAID_ARBITER_INFO pArbiter
)
{
	NTSTATUS	status;
	PDRAID_REBUILD_INFO RebuildInfo = &pArbiter->RebuildInfo;
	KIRQL oldIrql;
	UINT32 i;
	
	KDPrintM(DBG_LURN_INFO, ("Stopping Rebuild thread\n"));
	
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	RebuildInfo->ExitRequested = TRUE;
	KeSetEvent(&RebuildInfo->ThreadEvent,IO_NO_INCREMENT, FALSE);
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
	if (RebuildInfo->ThreadObject) {
		KDPrintM(DBG_LURN_INFO, ("Wait for rebuild IO thread completion\n"));
		status = KeWaitForSingleObject(
			RebuildInfo->ThreadObject,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);

		ASSERT(status == STATUS_SUCCESS);
		//
		//	Dereference the thread object.
		//

		ObDereferenceObject(RebuildInfo->ThreadObject);
		ZwClose(RebuildInfo->ThreadHandle);
	}
	for(i=0;i<pArbiter->ActiveDiskCount-1;i++) {
		if (RebuildInfo->RebuildBuffer[i]) {
			ExFreePoolWithTag(RebuildInfo->RebuildBuffer[i], DRAID_REBUILD_BUFFER_POOL_TAG);
		}
	}

	return STATUS_SUCCESS;
}

// Called with Arbiter->Spinlock locked.
NTSTATUS
DraidRebuildIoCancel(
	PDRAID_ARBITER_INFO Arbiter
)
{
	PDRAID_REBUILD_INFO RebuildInfo = &Arbiter->RebuildInfo;
	if (RebuildInfo->AggressiveRebuildMode) {
		KDPrintM(DBG_LURN_INFO, ("In aggressive rebuild mode. Rejecting rebuild cancel\n"));
		return STATUS_UNSUCCESSFUL;
	} else {
		RebuildInfo->CancelRequested = TRUE;
		KeSetEvent(&RebuildInfo->ThreadEvent, IO_NO_INCREMENT, FALSE);
	}

	return STATUS_SUCCESS;
}

NTSTATUS
DraidRebuildIoRequest(
	PDRAID_ARBITER_INFO Arbiter,
	UINT64	Addr,
	UINT32	Length
)
{
	KIRQL oldIrql;
	PDRAID_REBUILD_INFO RebuildInfo = &Arbiter->RebuildInfo;

	KDPrintM(DBG_LURN_INFO, ("Requesting to rebuild range %I64x:%x\n", Addr, Length));
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	if (RebuildInfo->Status != DRAID_REBUILD_STATUS_NONE || RebuildInfo->RebuildRequested) {
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("Rebuild status is %d. Not ready for new request\n", RebuildInfo->Status));
		return STATUS_UNSUCCESSFUL;
	}
	
	RebuildInfo->Addr = Addr;
	RebuildInfo->Length = Length;
	RebuildInfo->RebuildRequested = TRUE;
	KeSetEvent(&RebuildInfo->ThreadEvent, IO_NO_INCREMENT, FALSE);
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

	return STATUS_SUCCESS;
}

//
// Find region to rebuild and ask rebuild-thread to handle it.
//
NTSTATUS DraidRebuilldIoInitiate(
	PDRAID_ARBITER_INFO Arbiter
) {
	KIRQL oldIrql;
	NTSTATUS	status = STATUS_SUCCESS;
	PDRAID_REBUILD_INFO RebuildInfo = &Arbiter->RebuildInfo;
	ULONG					BitToRecover;

	//
	// Check rebuild thread is available 
	//
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	if (Arbiter->RaidStatus != DRIX_RAID_STATUS_REBUILDING) {
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("RAID is not rebuilding status(%x)\n", Arbiter->RaidStatus));
		return STATUS_UNSUCCESSFUL;
	}
	
	if (RebuildInfo->Status != DRAID_REBUILD_STATUS_NONE || RebuildInfo->RebuildRequested) {
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
//		KDPrintM(DBG_LURN_INFO, ("Rebuild thread is not ready for new request\n"));
		return STATUS_UNSUCCESSFUL;
	}
	// Previous lock should be unlocked by now..
	ASSERT(Arbiter->RebuildInfo.RebuildLock == NULL);

	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	
	//
	// Find set bits that is not locked by client.
	//
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);

	BitToRecover = RtlFindSetBits(&Arbiter->OosBmpHeader, 1, 0);
	KDPrintM(DBG_LURN_TRACE, ("First set dirty bit: %x\n", BitToRecover));
	if (0xFFFFFFFF == BitToRecover || BitToRecover>=Arbiter->OosBmpBitCount) {
		//
		// No region to recover. Finish recovery.
		//
		KDPrintM(DBG_LURN_INFO, ("No set bit in OOS bitmap. Finishing recovery\n"));

		KDPrintM(DBG_LURN_INFO, ("Clearing Out of sync node mark from role %d(node %d)\n", 
			Arbiter->OutOfSyncRole,
			Arbiter->RoleToNodeMap[Arbiter->OutOfSyncRole]
			));
		Arbiter->OutOfSyncRole = NO_OUT_OF_SYNC_ROLE;
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

		//
		// Set force flag 
		// 	because former out-of-sync disk's bitmap may not be clean.(Bitmap is updated only for update sector)
		//
		DraidArbiterUpdateOnDiskOosBitmap(Arbiter, TRUE); 
		
		DraidArbiterRefreshRaidStatus(Arbiter, TRUE); // Set force flag because we manually updated nodeflags.
		DraidArbiterUpdateInCoreRmd(Arbiter, TRUE);
		status = DraidArbiterWriteRmd(Arbiter,  &Arbiter->Rmd);
		KDPrintM(DBG_LURN_INFO, ("Rebuilding completed\n"));
	} else {
		PDRAID_ARBITER_LOCK_CONTEXT Lock;
		UINT64 OverlapStart, OverlapEnd;
		BOOLEAN MatchFound;
		PLIST_ENTRY	listEntry;
		UINT64 Addr;
		UINT32 Length;
			
		// Search and fire
		while(TRUE) {
			// Calc addr, length pair from bitmap and valid disk range.
			Addr = ((UINT64)BitToRecover) * Arbiter->SectorsPerOosBmpBit; 
			Length = Arbiter->SectorsPerOosBmpBit;
			if (Addr+Length > Arbiter->Lurn->UnitBlocks) {
				Length = (UINT32)(Arbiter->Lurn->UnitBlocks - Addr);
				ASSERT(Length < Arbiter->SectorsPerOosBmpBit);
			}
			// Check given bit is locked by client
			MatchFound = FALSE;
			for (listEntry = Arbiter->AcquiredLockList.Flink;
				listEntry != &Arbiter->AcquiredLockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
				if (DraidGetOverlappedRange(Addr, Length,
					Lock->LockAddress, Lock->LockLength, 
					&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
					MatchFound = TRUE;
					break;
				}
			}
			if (MatchFound) {
				ULONG i;
				BOOLEAN BitFound = FALSE;
				KDPrintM(DBG_LURN_TRACE, ("Bit 0x%x is locked. Searching next dirty bit\n", BitToRecover));		
				for(i=BitToRecover+1;i<Arbiter->OosBmpBitCount;i++) {
					if (RtlCheckBit(&Arbiter->OosBmpHeader, i)) {
						BitFound = TRUE;
						BitToRecover = i;
						break;
					}
				}
				if (BitFound) {
					continue; // Check this bit is locked
				} else {
					KDPrintM(DBG_LURN_INFO, ("No more not-locked dirty bitmap. Enter aggressive rebuild-mode\n"));
					Arbiter->RebuildInfo.AggressiveRebuildMode = TRUE;
					
					for (listEntry = Arbiter->AcquiredLockList.Flink;
						listEntry != &Arbiter->AcquiredLockList;
						listEntry = listEntry->Flink) 
					{
						Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
						if (DraidGetOverlappedRange(Addr, Length,
							Lock->LockAddress, Lock->LockLength, 
							&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
							// Request to yield conflicting lock.
							if (IsListEmpty(&Lock->ToYieldLink)) {
								KDPrintM(DBG_LURN_INFO, ("Rebuild process is requesting lock %I64x:%x to yield\n", Lock->LockAddress, Lock->LockLength));
								InsertTailList(&Arbiter->ToYieldLockList, &Lock->ToYieldLink);
							} else {
								// Already in yield list.
							}
							break;
						}
					}
					RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
					break;
				}
			} else {
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				if (Arbiter->RebuildInfo.AggressiveRebuildMode) {
					// In aggressive rebuild mode, rebuild IO thread will take the lock
				} else {
					//
					// Bit is not locked.
					// Lock it myself.
					//
					// New lock is allocated only by rebuild thread and arbiter thread. 
					// So it is safe to acquire this range. If lock can be acquired by another thread, this needs fix.
					//
					Arbiter->RebuildInfo.RebuildLock = DraidArbiterAllocLock(Arbiter, 
						DRIX_LOCK_TYPE_BLOCK_IO,
						DRIX_LOCK_MODE_EX,
						Addr, Length); 
					if (!Arbiter->RebuildInfo.RebuildLock) {
						status = STATUS_INSUFFICIENT_RESOURCES;
						goto errout;
					}
					// Add lock to acquired lock list
					ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
					Arbiter->RebuildInfo.RebuildLock->Status = DRAID_ARBITER_LOCK_STATUS_GRANTED;
					InsertTailList(&Arbiter->AcquiredLockList, &Arbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink);
					InterlockedIncrement(&Arbiter->AcquiredLockCount);
//					KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount));	
					RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
					// Update LWR before doing IO.
					DraidArbiterUpdateLwrBitmapBit(Arbiter, Arbiter->RebuildInfo.RebuildLock, NULL);
					status = DraidArbiterUpdateOnDiskOosBitmap(Arbiter, FALSE);
					if (!NT_SUCCESS(status)) {
						// to do: terminate arbiter?
					}
				}
				DraidRebuildIoRequest(Arbiter, Addr, Length);
				break;  // exit search loop
			}
		}
	}
	
errout:
	return status;
}

//
// Handle completed rebuild IO. 
//
VOID DraidRebuildIoAcknowledge(
	PDRAID_ARBITER_INFO Arbiter
) {
	KIRQL oldIrql;
	UINT32 RebuildStatus; 
	BOOLEAN BmpChanged = FALSE;
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	RebuildStatus = Arbiter->RebuildInfo.Status;
	if (RebuildStatus == DRAID_REBUILD_STATUS_DONE) {
		// Unset bitmap.
		KDPrintM(DBG_LURN_TRACE, ("Rebuilding range %I64x:%x is done\n", 
			Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length));
		DraidArbiterChangeOosBitmapBit(
			Arbiter, FALSE, Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length);
		BmpChanged = TRUE;
	} else if (RebuildStatus == DRAID_REBUILD_STATUS_FAILED) {
		// nothing to update
		KDPrintM(DBG_LURN_TRACE, ("Rebuilding range %I64x:%x is failed\n", 
			Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length));
	} else if (RebuildStatus == DRAID_REBUILD_STATUS_CANCELLED) {
		Arbiter->RebuildInfo.CancelRequested = FALSE;
		KDPrintM(DBG_LURN_TRACE, ("Rebuilding range %I64x:%x is canceled\n", 
			Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length));
	} else {
		KDPrintM(DBG_LURN_TRACE, ("Rebuilding is not in progess. Nothing to acknowledge\n"));
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		return;
	}

	Arbiter->RebuildInfo.Status = DRAID_REBUILD_STATUS_NONE;

	if (Arbiter->RebuildInfo.RebuildLock) {
		// Remove from acquired lock list.
		RemoveEntryList(&Arbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink);
		RemoveEntryList(&Arbiter->RebuildInfo.RebuildLock->ToYieldLink);
		InitializeListHead(&Arbiter->RebuildInfo.RebuildLock->ToYieldLink);
		InterlockedDecrement(&Arbiter->AcquiredLockCount);
		DraidArbiterFreeLock(Arbiter, Arbiter->RebuildInfo.RebuildLock);
		Arbiter->RebuildInfo.RebuildLock = NULL;
	}
	
	Arbiter->RebuildInfo.AggressiveRebuildMode = FALSE;
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	DraidArbiterUpdateLwrBitmapBit(Arbiter, NULL, NULL);
	DraidArbiterUpdateOnDiskOosBitmap(Arbiter, FALSE);
} 

