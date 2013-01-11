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
DradArbiterChangeOosBitmapBit(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN		Set,	// TRUE for set, FALSE for clear
	UINT64	Addr,
	UINT64	Length);

NTSTATUS 
DradArbiterUpdateOnDiskOosBitmap(
	PDRAID_ARBITER_INFO pArbiter
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
	PDRAID_CLIENT_CONTEXT Client
);

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
	Lock->ClientAcquired = NULL;

	return Lock;
}

VOID
DraidArbiterFreeLock(
	PDRAID_ARBITER_INFO pArbiter,
	PDRAID_ARBITER_LOCK_CONTEXT Lock
) {
	UNREFERENCED_PARAMETER(pArbiter);
	ExFreePoolWithTag(Lock, DRAID_ARBITER_LOCK_POOL_TAG);
}


//
// Modify lock to fit into lock range granularity
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
	
	if (CheckOverlap) {
recalc:
		StartAddr = (Lock->LockAddress / Granularity) * Granularity;
		EndAddr = ((Lock->LockAddress + Lock->LockLength-1)/Granularity + 1) * Granularity;
		Length = (UINT32)(EndAddr - StartAddr);
		Ok = TRUE;
		
		for (listEntry = pArbiter->AcquiredLockList.Flink;
			listEntry != &pArbiter->AcquiredLockList;
			listEntry = listEntry->Flink) 
		{
			AckLock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
			if (DraidGetOverlappedRange(StartAddr, Length,
				AckLock->LockAddress, AckLock->LockLength, 
				&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
				KDPrintM(DBG_LURN_INFO, ("Lock %I64x:%x ovelaps with granuality %x\n", 
					Lock->LockAddress, Lock->LockLength, Granularity));
				Granularity =Granularity/2;
				if (Granularity < DRAID_MINIMUM_LOCK_GRANULARITY) {
					ASSERT(FALSE); // this should not happen.
					KDPrintM(DBG_LURN_INFO, ("Lock %I64x:%x ovelaps with minimum granuality. Giving up.\n",
						Lock->LockAddress, Lock->LockLength));
					Ok = FALSE;
					break;
				} else {
					goto recalc;
				}
			}
		}
	} else {
 		// Use default Granularity without checking overlap with acquired lock
		StartAddr = (Lock->LockAddress /  Granularity) * Granularity;
		EndAddr = ((Lock->LockAddress + Lock->LockLength-1)/Granularity + 1) * Granularity;
		Length = (UINT32)(EndAddr - StartAddr);
		Ok = TRUE;		
	}
 
	if (Ok) {
		KDPrintM(DBG_LURN_INFO, ("Expanding lock range %I64x:%x to %I64x:%x\n",
			Lock->LockAddress, Lock->LockLength,
			StartAddr, EndAddr-StartAddr
			));
		ASSERT( (EndAddr-StartAddr) % Granularity == 0);
		Lock->LockAddress= StartAddr;
		Lock->LockLength = (UINT32)(EndAddr - StartAddr);
		return STATUS_SUCCESS;
	} else {
		return STATUS_UNSUCCESSFUL;
	}
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
	BOOLEAN NodeFlagFailed[MAX_DRAID_MEMBER_DISK] = {0};	 	// At least one node should success.
	BOOLEAN UpdateSucceeded;
	BOOLEAN LurnStatusMismatch;
	
	if(!(GENERIC_WRITE & pArbiter->Lurn->AccessRight))
	{
		// Only primary can call this
		ASSERT(FALSE);
		return STATUS_SUCCESS;
	}

	//
	// Flush all disk before updating metadata
	//
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++)
	{
		NodeFlags = pArbiter->NodeFlags[i];	
		if ((NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(NodeFlags & DRIX_NODE_FLAG_RUNNING)) {
			KDPrintM(DBG_LURN_INFO, ("Node %d flag: %x, defect: %x. Skipping metadata update\n", i, NodeFlags, pArbiter->DefectCodes[i]));
			NodeFlagFailed[i] = TRUE;
			continue;
		}

		if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
			NodeFlagMismatch[i] = TRUE;
			continue;
		}

		status = LurnExecuteSync(
			1,
			&pArbiter->Lurn->LurnChildren[i],
			SCSIOP_SYNCHRONIZE_CACHE,
			NULL,
			0,
			0,
			NULL);

		if(!NT_SUCCESS(status))
		{
			KDPrintM(DBG_LURN_INFO, ("Failed to flush node %d.\n", i));
			NodeFlagFailed[i] = TRUE;
			continue;			
		}
	}

	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++)
	{
		NodeFlags = pArbiter->NodeFlags[i];	
		if ((NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(NodeFlags & DRIX_NODE_FLAG_RUNNING)) {
			NodeFlagFailed[i] = TRUE;
//			KDPrintM(DBG_LURN_INFO, ("Node %d is not good for RMD writing. Skipping\n", i));			
			continue;
		}

		if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
			NodeFlagMismatch[i] = TRUE;
//			KDPrintM(DBG_LURN_INFO, ("Normal node %d is not running. Handling this case is not yet implemented\n", i));
			continue;
		}

		status = LurnExecuteSyncWrite(pArbiter->Lurn->LurnChildren[i], BlockBuffer,
			Addr, Length);

		if(!NT_SUCCESS(status))
		{
			NodeFlagFailed[i] = TRUE;
			KDPrintM(DBG_LURN_INFO, ("Failed to flush node %d.\n", i));
			continue;			
		}
	}
	
	//
	// Flush all disk again.
	//
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++)
	{
		NodeFlags = pArbiter->NodeFlags[i];	
		if ((NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) || !(NodeFlags & DRIX_NODE_FLAG_RUNNING)) {
			NodeFlagFailed[i] = TRUE;
//			KDPrintM(DBG_LURN_INFO, ("Node %d is not good for RMD writing. Skipping\n", i));			
			continue;
		}

		if(LURN_STATUS_RUNNING != pArbiter->Lurn->LurnChildren[i]->LurnStatus) {
			NodeFlagMismatch[i] = TRUE;
			KDPrintM(DBG_LURN_INFO, ("Normal node %d is not running. \n", i));
			continue;
		}

		status = LurnExecuteSync(
			1,
			&pArbiter->Lurn->LurnChildren[i],
			SCSIOP_SYNCHRONIZE_CACHE,
			NULL,
			0,
			0,
			NULL);

		if(!NT_SUCCESS(status))
		{
			NodeFlagFailed[i] = TRUE;
			KDPrintM(DBG_LURN_INFO, ("Failed to flush node %d.\n", i));
			continue;			
		}
	}


	//
	// Check metadata is updated to at least one node.
	//
	UpdateSucceeded = FALSE;
	for(i = 0; i < pArbiter->Lurn->LurnChildrenCnt; i++) {
		if (!NodeFlagFailed[i]) {
			UpdateSucceeded  = TRUE;
		} 
	}
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
DradArbiterUpdateOnDiskLwr(
	PDRAID_ARBITER_INFO pArbiter
) {
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	UINT32 SectorNum, SectorCount;
	UINT16 EntryNumInSector;
	UINT16 TotalEntryNum;
	NTSTATUS status;
	UINT64 SequenceNum;
	ULONG i;
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	SequenceNum = pArbiter->LwrBlocks[0].SequenceNum + 1;
	// Pack all acquired lock info into LWR buffer.(including rebuild lock)
	TotalEntryNum = 0;
	for (listEntry = pArbiter->AcquiredLockList.Flink;
		listEntry != &pArbiter->AcquiredLockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
		SectorNum = TotalEntryNum/NDAS_LWR_ENTRY_PER_BLOCK;
		EntryNumInSector = TotalEntryNum%NDAS_LWR_ENTRY_PER_BLOCK;
		
		pArbiter->LwrBlocks[SectorNum].Entry[EntryNumInSector].Address = Lock->LockAddress;
		pArbiter->LwrBlocks[SectorNum].Entry[EntryNumInSector].Length = Lock->LockLength;

		// Update entry number/sector
		TotalEntryNum++;
		if (TotalEntryNum >= NDAS_LWR_ENTRY_PER_BLOCK * NDAS_BLOCK_SIZE_LWR) {
			KDPrintM(DBG_LURN_INFO, ("Too many LWR entries. Cannot save more LWR\n"));	
			ASSERT(FALSE);
			break;			
		}
	}

	SectorCount = (TotalEntryNum + NDAS_LWR_ENTRY_PER_BLOCK - 1)/NDAS_LWR_ENTRY_PER_BLOCK;
	if (SectorCount == 0)
		SectorCount = 1; // Write at least one sector
		
	// Set sequence number
	for(i=0;i<SectorCount;i++) {
		pArbiter->LwrBlocks[i].SequenceNum = SequenceNum;
		pArbiter->LwrBlocks[i].SequenceNumTail = SequenceNum;
		pArbiter->LwrBlocks[i].LwrEntryCount = TotalEntryNum;
		pArbiter->LwrBlocks[i].LwrSectorCount = (UCHAR) SectorCount;
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	status = DraidArbiterWriteMetaSync(pArbiter, (PUCHAR)pArbiter->LwrBlocks, 
		NDAS_BLOCK_LOCATION_LWR, pArbiter->LwrBlocks[0].LwrSectorCount);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("Failed to update LWR\n"));	
	} else {
		KDPrintM(DBG_LURN_INFO, ("Updated LWR Seq %I64x, %d sectors, %d entries\n",
			pArbiter->LwrBlocks[0].SequenceNum,
			pArbiter->LwrBlocks[0].LwrSectorCount,
			pArbiter->LwrBlocks[0].LwrEntryCount));
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
	NewRmd.guid = pArbiter->Rmd.guid;
	NewRmd.uiUSN = pArbiter->Rmd.uiUSN;

	if (DRAID_ARBITER_STATUS_TERMINATING == pArbiter->Status) {
		NewRmd.state = NDAS_RAID_META_DATA_STATE_UNMOUNTED;
	} else {
		NewRmd.state = NDAS_RAID_META_DATA_STATE_MOUNTED;
	}

#if 0
	NewRmd.LwrPosition = pArbiter->Rmd.LwrPosition;
	NewRmd.LwrLength = pArbiter->Rmd.LwrLength;
	NewRmd.OosBitmapPosition = pArbiter->Rmd.OosBitmapPosition;
	NewRmd.OosBitmapLength = pArbiter->Rmd.OosBitmapLength; 
#endif

	if (NewRmd.state == NDAS_RAID_META_DATA_STATE_MOUNTED) {
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

			MacAddr = ((PTDI_ADDRESS_LPX)&IdeDisk->LanScsiSession.SourceAddress.Address[0].Address)->Node;
				
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
		NodeFlags = pArbiter->NodeFlags[NewRmd.UnitMetaData[i].iUnitDeviceIdx];
		if (NodeFlags & DRIX_NODE_FLAG_DEFECTIVE) {
			NewRmd.UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_DEFECTIVE;
			NewRmd.UnitMetaData[i].DefectCode = pArbiter->Rmd.UnitMetaData[i].DefectCode;
		}
		
		if (i == pArbiter->OutOfSyncRole) {
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
	default:
		ReplyLength = sizeof(DRIX_HEADER);
		break;
	}

	//
	// Create reply
	//
 	ReplyMsg = 	ExAllocatePoolWithTag(NonPagedPool, ReplyLength, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG);
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

			KDPrintM(DBG_LURN_INFO, ("Arbiter received ACQUIRE_LOCK message: %I64x:%x\n", Addr, Length));

			// Check lock list if lock is held by other client
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			for (listEntry = pArbiter->AcquiredLockList.Flink;
				listEntry != &pArbiter->AcquiredLockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
				if (DraidGetOverlappedRange(Addr, Length,
					Lock->LockAddress, Lock->LockLength, 
					&OverlapStart, &OverlapEnd) != DRAID_RANGE_NO_OVERLAP) {
					UINT32 Granularity = pArbiter->LockRangeGranularity;
					MatchFound = TRUE;

					if (Lock == pArbiter->RebuildInfo.RebuildLock) {
						// Rebuilding IO is holding this lock.
						KDPrintM(DBG_LURN_INFO, ("Area is locked for rebuilding.\n"));
						Granularity = Lock->LockGranularity / 2;
						DraidRebuildIoCancel(pArbiter);
					} else {
						KDPrintM(DBG_LURN_INFO, ("Range is locked by another client.\n"));
						Granularity = Lock->LockGranularity / 2;					
						// Client has lock. Queue this lock to to-yield-list. This will be handled by Arbiter thread later.
						if (IsListEmpty(&Lock->ToYieldLink))
							InsertTailList(&pArbiter->ToYieldLockList, &Lock->ToYieldLink);
					}						
					
					RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);		

					//
					// Create lock context for this host and set to client
					//

					NewLock = DraidArbiterAllocLock(pArbiter, AcqMsg->LockType, AcqMsg->LockMode,
						Addr, Length);
					ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
					if (NewLock ==NULL) {
						ResultCode = DRIX_RESULT_FAIL;
						break;
					} else {
						// Arrange lock with reduced range.
						DraidArbiterArrangeLockRange(pArbiter, NewLock, Granularity, FALSE);
						// Add to pending list.
						InsertTailList(&pClient->PendingLockList, &NewLock->PendingLink);
						ResultCode = DRIX_RESULT_PENDING;
					}
				}
			}
			Lock = NULL;
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			if (MatchFound == FALSE) {
				//
				// We need to allocate new lock and send reply with 
				//
				NewLock = DraidArbiterAllocLock(pArbiter, AcqMsg->LockType, AcqMsg->LockMode,
						Addr, Length);
				if (NewLock == NULL) {
					ResultCode = DRIX_RESULT_FAIL;
				} else {
					ResultCode = DRIX_RESULT_GRANTED;
					NewLock->ClientAcquired = pClient;
					ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);

					// Adjust addr/length to align bitmap
					DraidArbiterArrangeLockRange(pArbiter, NewLock, pArbiter->LockRangeGranularity, TRUE);
					// Add to arbiter list
					InsertTailList(&pArbiter->AcquiredLockList, &NewLock->ArbiterAcquiredLink);
					InterlockedIncrement(&pArbiter->AcquiredLockCount);
//					KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",pArbiter->AcquiredLockCount));
					
					InsertTailList(&pClient->AcquiredLockList, &NewLock->ClientAcquiredLink);
					if (pArbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
						KDPrintM(DBG_LURN_INFO, ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",
							NewLock->LockAddress, NewLock->LockLength));
						DradArbiterChangeOosBitmapBit(pArbiter, TRUE, NewLock->LockAddress, NewLock->LockLength);
					}
					RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

					// Need to update BMP and LWR before client start writing using this lock
					status = DradArbiterUpdateOnDiskOosBitmap(pArbiter);
					if (!NT_SUCCESS(status)) {
						KDPrintM(DBG_LURN_ERROR, ("Failed to update bitmap\n"));
					}

					status = DradArbiterUpdateOnDiskLwr(pArbiter);
					if (!NT_SUCCESS(status)) {
						KDPrintM(DBG_LURN_ERROR, ("Failed to update LWR\n"));
					}
				}
			}
			if (NewLock) {
				PDRIX_ACQUIRE_LOCK_REPLY AcqReply = (PDRIX_ACQUIRE_LOCK_REPLY) ReplyMsg;
				AcqReply->LockType = AcqMsg->LockType;
				AcqReply->LockMode = AcqMsg->LockMode;
				AcqReply->LockId = NTOHLL(NewLock->LockId);
				AcqReply->Addr = NTOHLL(NewLock->LockAddress);
				AcqReply->Length = NTOHL(NewLock->LockLength);
			}
		}
		break;
	case DRIX_CMD_RELEASE_LOCK:
		{
			//
			// Check lock is owened by this client.
			//
			PDRAID_ARBITER_LOCK_CONTEXT Lock;
			PDRIX_RELEASE_LOCK RelMsg = (PDRIX_RELEASE_LOCK) Message;
			UINT64 LockId = NTOHLL(RelMsg->LockId);

			KDPrintM(DBG_LURN_INFO, ("Arbiter received RELEASE_LOCK message: %I64x\n", LockId));

			ResultCode = DRIX_RESULT_INVALID_LOCK_ID;
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			// Search for matching Lock ID
			for (listEntry = pArbiter->AcquiredLockList.Flink;
				listEntry != &pArbiter->AcquiredLockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ArbiterAcquiredLink);
				if (LockId == DRIX_LOCK_ID_ALL && Lock->ClientAcquired == pClient) {
					KDPrintM(DBG_LURN_INFO, ("Releasing all locks - Lock %I64x:%x\n", Lock->LockAddress, Lock->LockLength));
					// Remove from all list
					listEntry = listEntry->Blink;	// We will change link in the middle. Take care of listEntry
					RemoveEntryList(&Lock->ArbiterAcquiredLink);
					InterlockedDecrement(&pArbiter->AcquiredLockCount);
					RemoveEntryList(&Lock->ToYieldLink);
					RemoveEntryList(&Lock->ClientAcquiredLink);
					ResultCode = DRIX_RESULT_SUCCESS;
					ExFreePoolWithTag(Lock, DRAID_ARBITER_LOCK_POOL_TAG);					
				} else if (Lock->LockId == LockId) {
					if (Lock->ClientAcquired != pClient) {
						KDPrintM(DBG_LURN_ERROR, ("Lock ID matched but client didn't match. release lock failed\n"));
						ASSERT(FALSE);
						break;
					} else {
						// Remove from all list
						RemoveEntryList(&Lock->ArbiterAcquiredLink);
						InterlockedDecrement(&pArbiter->AcquiredLockCount);
//						KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",pArbiter->AcquiredLockCount));						
						RemoveEntryList(&Lock->ToYieldLink);
						RemoveEntryList(&Lock->ClientAcquiredLink);
						ResultCode = DRIX_RESULT_SUCCESS;
						ExFreePoolWithTag(Lock, DRAID_ARBITER_LOCK_POOL_TAG);
						break;
					}
				}
			}
				
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
			status = DradArbiterUpdateOnDiskLwr(pArbiter);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Failed to update LWR\n"));
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
		KDPrintM(DBG_LURN_INFO, ("DRAID Sending reply to request %x with result %x to local client(event=%p)\n", 
			Message->Command, ResultCode,
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
		KDPrintM(DBG_LURN_INFO, ("DRAID Sending reply to request %x with result %x to remote client\n", 
			Message->Command, ResultCode));
		
		status =	LpxTdiSend(pClient->RequestConnection->ConnectionFileObject, (PUCHAR)ReplyMsg, ReplyLength,
			0, &Timeout, NULL, &Result);
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
	
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
	//
	// Check this emergency state is long enough to use spare.
	//
	if (pArbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
		LARGE_INTEGER time_diff;
		LARGE_INTEGER current_time;
		KeQueryTickCount(&current_time);
		time_diff.QuadPart = (current_time.QuadPart - pArbiter->DegradedSince.QuadPart) * KeQueryTimeIncrement();
		if (time_diff.QuadPart < NDAS_RAID_SPARE_HOLDING_TIMEOUT) {
			KDPrintM(DBG_LURN_INFO, ("Fault device is not alive but wait for more before using spare:%d sec remains\n",
				(ULONG)((NDAS_RAID_SPARE_HOLDING_TIMEOUT-time_diff.QuadPart) / HZ)));
		} else {
			BOOLEAN SpareFound = FALSE;
			UCHAR SpareNode = 0;

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
				
				// Replaced node is still out-of-sync. Keep pArbiter->OutOfSyncNode
				
				// Set all bitmap dirty
				DradArbiterChangeOosBitmapBit(pArbiter, TRUE, 0, pArbiter->Lurn->UnitBlocks);
				SpareUsed = TRUE;
			} else {
				if (pArbiter->ActiveDiskCount < pArbiter->TotalDiskCount) {
					KDPrintM(DBG_LURN_INFO, ("Spare disk is not running\n"));
				} else {

				}
			}
		}
	}

	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	if (SpareUsed) {
		// Update RAID status. 
		DraidArbiterRefreshRaidStatus(pArbiter, TRUE);
		DradArbiterUpdateOnDiskOosBitmap(pArbiter);
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
	
	KDPrintM(DBG_LURN_INFO, ("Notifying client with command %02x\n", Command));	
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
		KDPrintM(DBG_LURN_INFO, ("DRAID Sending notification %x to local client\n", Command));	
		MsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
		if (MsgEntry) {
			RtlZeroMemory(MsgEntry, sizeof(DRIX_MSG_CONTEXT));
			InitializeListHead(&MsgEntry->Link);
			MsgEntry->Message = NotifyMsg;
			ExInterlockedInsertTailList(&Client->NotificationChannel->Queue, &MsgEntry->Link, &Client->NotificationChannel->Lock);
			KDPrintM(DBG_LURN_INFO, ("Setting event %p\n", &Client->NotificationChannel->Event));
			KeSetEvent(&Client->NotificationChannel->Event,IO_NO_INCREMENT, FALSE);

			KDPrintM(DBG_LURN_INFO, ("Waiting for notification reply %p\n", &Client->NotificationReplyChannel.Event));
			// Wait for reply
			status = KeWaitForSingleObject(
				&Client->NotificationReplyChannel.Event,
				Executive,
				KernelMode,
				FALSE,
				NULL
			);
			KeClearEvent(&Client->NotificationReplyChannel.Event);
			KDPrintM(DBG_LURN_INFO, ("Received reply\n"));
			
			listEntry = ExInterlockedRemoveHeadList(&Client->NotificationReplyChannel.Queue, &Client->NotificationReplyChannel.Lock);
			ASSERT(listEntry);
			ReplyMsgEntry = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
			ReplyMsg = ReplyMsgEntry->Message;
			// Notification always success.
			ExFreePoolWithTag(ReplyMsgEntry, DRAID_MSG_LINK_POOL_TAG);
			status = STATUS_SUCCESS;
			KDPrintM(DBG_LURN_INFO, ("DRAID Notification result=%x\n", ReplyMsg->Result));		
			ExFreePoolWithTag(ReplyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);		
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

		KDPrintM(DBG_LURN_INFO, ("DRAID Sending notification %x to remote client\n", Command));
		status =	LpxTdiSend(Client->NotificationConnection->ConnectionFileObject, (PUCHAR)NotifyMsg, MsgLength,
			0, &Timeout, NULL, &Result);
		ExFreePoolWithTag(NotifyMsg, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to send notification message\n"));
			ErrorOccured = TRUE;
			status = STATUS_UNSUCCESSFUL;
		} else {
		
			// Start synchrous receiving with short timeout.
			// To do: Receive reply in asynchrous mode and handle receive in seperate function to remove arbiter blocking.
			Timeout.QuadPart = 5 * HZ;
			status = LpxTdiRecv(
							Client->NotificationConnection->ConnectionFileObject,
							(PUCHAR)&ReplyMsg, sizeof(DRIX_HEADER), 0, &Timeout, NULL, &result);

			if (NT_SUCCESS(status) && result == sizeof(DRIX_HEADER)) {
				// Check validity
				if (ReplyMsg.Signature == HTONL(DRIX_SIGNATURE) &&
					ReplyMsg.Command ==Command &&
					ReplyMsg.ReplyFlag == 1 &&
					ReplyMsg.Length == HTONS((UINT16)sizeof(DRIX_HEADER)) &&
					ReplyMsg.Sequence == HTONS(Client->NotifySequence-1)) {
					KDPrintM(DBG_LURN_INFO, ("DRAID Notification result=%x\n", ReplyMsg.Result));
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
			DraidArbiterTerminateClient(pArbiter, Client);
			KDPrintM(DBG_LURN_INFO, ("Freeing client context\n"));
			ExFreePoolWithTag(Client, DRAID_CLIENT_CONTEXT_POOL_TAG);
		}
	}
out:
	return status;
}


NTSTATUS
DraidArbiterGrantLockIfPossible(
	PDRAID_ARBITER_INFO Arbiter,
	PDRAID_CLIENT_CONTEXT Client
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
	for(PendingListEntry = Client->PendingLockList.Flink;
		PendingListEntry != &Client->PendingLockList;
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
				KDPrintM(DBG_LURN_INFO, ("Area is locked\n"));						
				RangeAvailable = FALSE;
			}
		}
		if (RangeAvailable) {
			KDPrintM(DBG_LURN_INFO, ("Granting pending Lock %I64x\n", PendingLock->LockId));
			Lock = PendingLock;
			RemoveEntryList(&Lock->PendingLink);
			Lock->ClientAcquired = Client;
//			DraidArbiterArrangeLockRange(Arbiter, Lock, 0); // Lock range is alreay arranged.
			// Add to arbiter list
			InsertTailList(&Arbiter->AcquiredLockList, &Lock->ArbiterAcquiredLink);
			InterlockedIncrement(&Arbiter->AcquiredLockCount);
//			KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n", Arbiter->AcquiredLockCount));
			
			InsertTailList(&Client->AcquiredLockList, &Lock->ClientAcquiredLink);
			if (Arbiter->RaidStatus == DRIX_RAID_STATUS_DEGRADED) {
				KDPrintM(DBG_LURN_INFO, ("Granted lock in degraded mode. Mark OOS bitmap %I64x:%x\n",
					Lock->LockAddress, Lock->LockLength));
				DradArbiterChangeOosBitmapBit(Arbiter, TRUE, Lock->LockAddress, Lock->LockLength);
			}
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

			// Need to update BMP and LWR before client start writing using this lock
			status = DradArbiterUpdateOnDiskOosBitmap(Arbiter);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Failed to update bitmap\n"));
			}

			status = DradArbiterUpdateOnDiskLwr(Arbiter);
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Failed to update LWR\n"));
			}
			status = DraidArbiterNotify(Arbiter, Client, DRIX_CMD_GRANT_LOCK, (UINT64)Lock, 0, 0);
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
	BOOLEAN NewDisk = FALSE;

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
			
			//
			// Flag priority:  DRIX_NODE_FLAG_DEFECTIVE > DRIX_NODE_FLAG_STOP > DRIX_NODE_FLAG_RUNNING > DRIX_NODE_FLAG_UNKNOWN
			//
			if (Client->NodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE)  {
				NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING|DRIX_NODE_FLAG_STOP);
				NewNodeFlags[i] |= DRIX_NODE_FLAG_DEFECTIVE;
				if (Client->DefectCode[i] != 0)
					pArbiter->DefectCodes[i] = Client->DefectCode[i];
			}

			if ((Client->NodeFlags[i] & DRIX_NODE_FLAG_STOP) &&
				!(NewNodeFlags[i] & DRIX_NODE_FLAG_DEFECTIVE)
			) {
				NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN|DRIX_NODE_FLAG_RUNNING);	
				NewNodeFlags[i] |= DRIX_NODE_FLAG_STOP;
			}

			if ((Client->NodeFlags[i] & DRIX_NODE_FLAG_RUNNING) &&
				!(NewNodeFlags[i] & (DRIX_NODE_FLAG_DEFECTIVE|DRIX_NODE_FLAG_STOP))
			) {
				NewNodeFlags[i] &= ~(DRIX_NODE_FLAG_UNKNOWN);	
				NewNodeFlags[i] |= DRIX_NODE_FLAG_RUNNING;
			}
#if 0
			if ((Client->NodeFlags[i] & DRIX_NODE_FLAG_NEW_DISK) && ) {
				// Don't need to store about new disk. Just set all bitmap
				KDPrintM(DBG_LURN_INFO, ("Changing Node flags from %02x to %02x\n", pArbiter->NodeFlags[i], NewNodeFlags[i]));						
				NewDisk = TRUE;
				DradArbiterChangeOosBitmapBit(pArbiter, TRUE, 0, pArbiter->Lurn->UnitBlocks);
			}
#endif			
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
				KDPrintM(DBG_LURN_INFO, ("Changing Node flags from %02x to %02x\n", pArbiter->NodeFlags[i], NewNodeFlags[i]));
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
				DradArbiterChangeOosBitmapBit(pArbiter, TRUE, Lock->LockAddress, Lock->LockLength);
				// We need to write updated bitmap before any lock information is changed.
				BmpChanged = TRUE;
			}
		}
		
		if (NewRaidStatus == DRIX_RAID_STATUS_REBUILDING) {
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
		status = DradArbiterUpdateOnDiskOosBitmap(pArbiter);
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
	PDRAID_CLIENT_CONTEXT Client
) {
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	PLIST_ENTRY	listEntry;
	NTSTATUS status;
	KIRQL oldIrql;
	PDRIX_MSG_CONTEXT MsgEntry;
	KDPrintM(DBG_LURN_INFO, ("Terminating client..\n"));

	// To do: retire message. Merge LWR to bitmap if retiring fails.
	
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);

	//
	// to do: merge lwr to bitmap if client is terminated cleanly.
	//
	
	//
	// Free locks acquired by this client
	//
	while(TRUE) {
		listEntry = RemoveHeadList (&Client->AcquiredLockList);
		if (listEntry == &Client->AcquiredLockList)
			break;
		Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, ClientAcquiredLink);
		// Remove from arbiter's list too.
		RemoveEntryList(&Lock->ArbiterAcquiredLink);
		// Remove from to yield lock list.
		RemoveEntryList(&Lock->ToYieldLink);
		InterlockedDecrement(&Arbiter->AcquiredLockCount);
//		KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount));
		
		KDPrintM(DBG_LURN_INFO, ("Freeing terminated client's lock %I64x(%I64x:%x)\n", Lock->LockId,
			Lock->LockAddress, Lock->LockLength));
		DraidArbiterFreeLock(Arbiter, Lock);
	}

	while(TRUE) {
		listEntry = RemoveHeadList (&Client->PendingLockList);
		if (listEntry == &Client->PendingLockList)
			break;
		Lock = CONTAINING_RECORD (listEntry, DRAID_ARBITER_LOCK_CONTEXT, PendingLink);
		KDPrintM(DBG_LURN_INFO, ("Freeing terminated client's pending lock %I64x(%I64x:%x)\n", Lock->LockId,
			Lock->LockAddress, Lock->LockLength));
		DraidArbiterFreeLock(Arbiter, Lock);
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
	} else {
		// to do: cleanup remote client
//		ASSERT(FALSE);

	}
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	
	status = DradArbiterUpdateOnDiskLwr(Arbiter);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to update LWR\n"));
	}

	if (Client->NotificationConnection) {
		if (Client->NotificationConnection->ConnectionFileObject) {
			LpxTdiDisconnect(Client->NotificationConnection->ConnectionFileObject, 0);
			LpxTdiDisassociateAddress(Client->NotificationConnection->ConnectionFileObject);
			LpxTdiCloseConnection(Client->NotificationConnection->ConnectionFileHandle, Client->NotificationConnection->ConnectionFileObject);
		}
		ExFreePoolWithTag(Client->NotificationConnection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);
	} 
	if (Client->RequestConnection) {
		if (Client->RequestConnection->ConnectionFileObject) {
			LpxTdiDisconnect(Client->RequestConnection->ConnectionFileObject, 0);
			LpxTdiDisassociateAddress(Client->RequestConnection->ConnectionFileObject);
			LpxTdiCloseConnection(Client->RequestConnection->ConnectionFileHandle, Client->RequestConnection->ConnectionFileObject);
		}
		ExFreePoolWithTag(Client->RequestConnection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);
	}	
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
				ASSERT(FALSE);
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
					status = LpxTdiRecv(Client->RequestConnection->ConnectionFileObject, 
						(PUCHAR)(Client->RequestConnection->ReceiveBuf + sizeof(DRIX_HEADER)),
						AddtionalLength,
						0, &Timeout, NULL, &result);
					if (!NT_SUCCESS(status) || result != AddtionalLength) {
						KDPrintM(DBG_LURN_INFO, ("Failed to get remaining message\n", MsgLength));		
						ErrorOccured = TRUE;
						ASSERT(FALSE);
					}
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
				DraidArbiterTerminateClient(Arbiter, Client);
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
						status = LpxTdiRecvWithCompletionEvent(
										Client->RequestConnection->ConnectionFileObject,
										&Client->RequestConnection->TdiReceiveContext,
										Client->RequestConnection->ReceiveBuf,
										sizeof(DRIX_HEADER),	0, NULL,NULL
						);
						if (!NT_SUCCESS(status)) {
							KDPrintM(DBG_LURN_INFO, ("Failed to start to recv from client. Terminating this client\n"));
							ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
							RemoveEntryList(&Client->Link);
							RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
							DraidArbiterTerminateClient(Arbiter, Client);
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

				DraidArbiterTerminateClient(Arbiter, Client);

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
			if (Lock->ClientAcquired) {
				status = DraidArbiterNotify(Arbiter, Lock->ClientAcquired, DRIX_CMD_REQ_TO_YIELD_LOCK, 0, Lock->LockId,0);
				if (!NT_SUCCESS(status)) {
					// ClientAcquired may have destroyed and ToYieldLockList may have been changed. restart
					KDPrintM(DBG_LURN_INFO, ("Notify failed during yield lock. Restarting.\n"));
					goto restart;
				}
			}
			InitializeListHead(&Lock->ToYieldLink);
		}

		//
		// To do: send yield message if pArbiter->AcquiredLockCount is over threshold. 
		//

		// 
		// Send grant message if any pending lock exist.
		//
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		for (listEntry = Arbiter->ClientList.Flink;
			listEntry != &Arbiter->ClientList;
			listEntry = listEntry->Flink)  {
			Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_CONTEXT, Link);
			if (Client->Initialized == FALSE)
				continue;
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			status = DraidArbiterGrantLockIfPossible(Arbiter, Client);
			ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
			if (!NT_SUCCESS(status)) {
				break;
			}
		}
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		if (Arbiter->RaidStatus == DRIX_RAID_STATUS_REBUILDING) {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			DraidRebuilldIoInitiate(Arbiter);
		} else {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		}
		// Check termination
		ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
		if (DRAID_ARBITER_STATUS_TERMINATING == Arbiter->Status) {
			KDPrintM(DBG_LURN_INFO, ("DRAID Aribter stop requested\n"));			
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);		
			break;
		}
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);

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

	DraidRebuildIoStop(Arbiter);

	ASSERT(DRAID_ARBITER_STATUS_TERMINATING== Arbiter->Status);
	
	DraidArbiterUpdateInCoreRmd(Arbiter, FALSE);
	status = DraidArbiterWriteRmd(Arbiter, &Arbiter->Rmd);

fail:
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
	RtlZeroMemory(pArbiter->OosBmpInCoreBuffer, pArbiter->OosBmpByteCount);
	RtlZeroMemory(pArbiter->OosBmpOnDiskBuffer, pArbiter->OosBmpSectorCount * SECTOR_SIZE);

	RtlInitializeBitMap(&pArbiter->OosBmpHeader, (PULONG)pArbiter->OosBmpInCoreBuffer, 
		pArbiter->OosBmpByteCount * 8);
	
	//
	// Read from UpToDateNode.
	// Bitmap from any non OOS disk should be okay 
	//     even if disks have different sequence number due to crash during bitmap update. (Bit set or clear is both correct)
	//     And partial updated block will be set to all dirty anyway.
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
		for(i=0;i<pArbiter->OosBmpSectorCount;i++) {
			pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead = pArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = 0;
			RtlFillMemory(pArbiter->OosBmpOnDiskBuffer[i].Bits, sizeof(pArbiter->OosBmpOnDiskBuffer[i].Bits), 0x0ff);		
		}
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
				pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead = pArbiter->OosBmpOnDiskBuffer[i].SequenceNumTail = 0;
				RtlFillMemory(pArbiter->OosBmpOnDiskBuffer[i].Bits, sizeof(pArbiter->OosBmpOnDiskBuffer[i].Bits), 0x0ff);
			}			
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

		RtlCopyMemory(pArbiter->OosBmpInCoreBuffer+DRAID_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(i,0), 
			pArbiter->OosBmpOnDiskBuffer[i].Bits, ByteCount);
	}
	status = STATUS_SUCCESS;

errout:
	return status;
}

//
// Should be called with pArbiter->Spinlock locked.
//
VOID
DradArbiterChangeOosBitmapBit(
	PDRAID_ARBITER_INFO pArbiter,
	BOOLEAN		Set,	// TRUE for set, FALSE for clear
	UINT64	Addr,
	UINT64	Length
) {
	UINT32 i;
	UINT32 ByteOffset;
	UINT32 BmpSectorOffset;
	UINT32 BmpByteOffset;
#if 0
	UINT32 PrevBits;
#endif
	UINT32 ByteCount;
	UINT32 BitOffset;
	UINT32 NumberOfBit;

	ASSERT(KeGetCurrentIrql() ==  DISPATCH_LEVEL); // should be called with spinlock locked.
	
	BitOffset = (UINT32)(Addr / pArbiter->SectorsPerOosBmpBit);
	NumberOfBit = (UINT32)((Addr + Length -1) / pArbiter->SectorsPerOosBmpBit - BitOffset + 1);


//	KDPrintM(DBG_LURN_INFO, ("Before BitmapByte[0]=%x\n", pArbiter->OosBmpInCoreBuffer[0]));	
	if (Set) {
		KDPrintM(DBG_LURN_INFO, ("Setting in-memory bitmap offset %x:%x\n", BitOffset, NumberOfBit));
		RtlSetBits(&pArbiter->OosBmpHeader, BitOffset, NumberOfBit);
	} else {
		KDPrintM(DBG_LURN_INFO, ("Clearing in-memory bitmap offset %x:%x\n", BitOffset, NumberOfBit));
		RtlClearBits(&pArbiter->OosBmpHeader, BitOffset, NumberOfBit);
	}
//	KDPrintM(DBG_LURN_INFO, ("After BitmapByte[0]=%x\n", pArbiter->OosBmpInCoreBuffer[0]));	

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
}

NTSTATUS 
DradArbiterUpdateOnDiskOosBitmap(
	PDRAID_ARBITER_INFO pArbiter
) {
	NTSTATUS status = STATUS_SUCCESS; // in case for there is no dirty bitmap
	ULONG	i;
	KIRQL oldIrql;
	
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
			KDPrintM(DBG_LURN_INFO, ("Updating dirty bitmap sector %d, Seq = %I64x\n", i, pArbiter->OosBmpOnDiskBuffer[i].SequenceNumHead));
			
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



//
// Read LWR from UpToDateNode and set to bitmap
//
NTSTATUS 
DraidArbiterProcessDirtyLwr(
	PDRAID_ARBITER_INFO pArbiter,
	UINT32 UpToDateNode
) {
	ULONG i, j;
	NTSTATUS status;
	UINT32 EntryCount;
	UINT32 SectorCount;
	BOOLEAN		LwrCorrupted = FALSE;
	KIRQL oldIrql;
	
	// We need to check LWR only when disk is not cleanly unmounted.
	if (pArbiter->Rmd.state == NDAS_RAID_META_DATA_STATE_MOUNTED) {
		KDPrintM(DBG_LURN_ERROR, ("DRAID is not cleanly unmounted. Process LWR\n"));

		status = LurnExecuteSyncRead(pArbiter->Lurn->LurnChildren[UpToDateNode], (PUCHAR)pArbiter->LwrBlocks,
			NDAS_BLOCK_LOCATION_LWR, (UINT32)NDAS_BLOCK_SIZE_LWR);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to read LWR from up-to-date node %d\n", UpToDateNode));	
			return status;
		}
		//
		// Check LWR validity
		//

		ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);			

		EntryCount = pArbiter->LwrBlocks[0].LwrEntryCount;
		SectorCount = pArbiter->LwrBlocks[0].LwrSectorCount;
		if (EntryCount > NDAS_LWR_ENTRY_PER_BLOCK * NDAS_BLOCK_SIZE_LWR || SectorCount > NDAS_LWR_ENTRY_PER_BLOCK
			|| pArbiter->LwrBlocks[0].SequenceNum != pArbiter->LwrBlocks[0].SequenceNumTail) {
			LwrCorrupted= TRUE;
			KDPrintM(DBG_LURN_ERROR, ("Invalid LWR.\n"));
		} else if (EntryCount == 0) {
			KDPrintM(DBG_LURN_ERROR, ("No LWR entry\n"));
		} else {
			UINT32 EntryNum=0;
			UINT64 Addr;
			UINT32 Length;
			UINT64 SequenceNum = pArbiter->LwrBlocks[0].SequenceNum;

			//
			// Select out-of-sync disk if no disk is marked as out-of-sync
			//
			ASSERT(pArbiter->RaidStatus != DRIX_RAID_STATUS_INITIALIZING);

			if (pArbiter->RaidStatus == DRIX_RAID_STATUS_NORMAL) {
				// Maybe primary host crashed so no disk is marked as out-of-sync
				// Select any node as out-of-sync
				KDPrintM(DBG_LURN_ERROR, ("LWR is not clear and no node is marked as out-of-sync.\n"));
				KDPrintM(DBG_LURN_ERROR, ("Marking node %d as out-of-sync.\n", pArbiter->RoleToNodeMap[pArbiter->ActiveDiskCount-1]));

				pArbiter->OutOfSyncRole = pArbiter->ActiveDiskCount-1;
				// This change will be written to RMD when arbiter thread starts.
			}

			for(i=0;i<SectorCount;i++) {
				//
				// Check block integrity. All valid blocks should have same sequence number
				//
				if (SequenceNum != pArbiter->LwrBlocks[i].SequenceNum ||
					SequenceNum != pArbiter->LwrBlocks[i].SequenceNumTail) {
					KDPrintM(DBG_LURN_ERROR, ("Invalid LWR at sector %d\n", i));
					LwrCorrupted = TRUE;
					break;
				}
				for(j=0;j<NDAS_LWR_ENTRY_PER_BLOCK;j++) {
					Addr = pArbiter->LwrBlocks[i].Entry[j].Address;
					Length = pArbiter->LwrBlocks[i].Entry[j].Length;	
					if (Addr+Length > pArbiter->Lurn->UnitBlocks) {
						KDPrintM(DBG_LURN_ERROR, ("Invalid LWR entry %d-out of bound range.\n", EntryNum));
						LwrCorrupted= TRUE;						
						break;
					}
					
					KDPrintM(DBG_LURN_ERROR, ("Merging LWR %d(%I64x:%x) to OOS bitmap\n", 
						EntryNum, Addr, Length));
					DradArbiterChangeOosBitmapBit(pArbiter, TRUE, Addr, Length);
					EntryNum++;
					if (EntryNum >= EntryCount)
						break;
				}
				if (LwrCorrupted)
					break;
			}
		}

		if (LwrCorrupted) {
			KDPrintM(DBG_LURN_INFO, ("Setting all OOS bitmap bits due to LWR corruption\n"));
			DradArbiterChangeOosBitmapBit(pArbiter, TRUE, 0, pArbiter->Lurn->UnitBlocks);
		}
		RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);
		
		// Update merged bitmap to disk
		status = DradArbiterUpdateOnDiskOosBitmap(pArbiter);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to write bitmap\n"));	
			return status;
		}
	} else {
		KDPrintM(DBG_LURN_INFO, ("DRAID is cleanly unmounted or never mounted. No need to read LWR\n"));
	}

		
	// We always need to clean LWR when mounting.
	KDPrintM(DBG_LURN_INFO, ("Writing empty LWRs.\n"));
	RtlZeroMemory(pArbiter->LwrBlocks, (size_t)(SECTOR_SIZE * NDAS_BLOCK_SIZE_LWR));
	for(i=0;i<NDAS_BLOCK_SIZE_LWR;i++) {
		pArbiter->LwrBlocks[i].SequenceNum = 1;
		pArbiter->LwrBlocks[i].SequenceNumTail = 1;
		pArbiter->LwrBlocks[i].LwrEntryCount = 0;
		pArbiter->LwrBlocks[i].LwrSectorCount = 1; // Minimum 1 sector.
	}		

	status = DraidArbiterWriteMetaSync(pArbiter, (PUCHAR)pArbiter->LwrBlocks, 
		NDAS_BLOCK_LOCATION_LWR, (UINT32)NDAS_BLOCK_SIZE_LWR);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("Failed to write empty LWR\n"));	
		return status;
	}

	return STATUS_SUCCESS;
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
	
	ASSERT(pRaidInfo->pDraidArbiter == NULL); // Multiple arbiter is not possible.
	
	pRaidInfo->pDraidArbiter = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_ARBITER_INFO),
		DRAID_ARBITER_INFO_POOL_TAG);
	if (NULL ==  pRaidInfo->pDraidArbiter) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	pArbiter = pRaidInfo->pDraidArbiter;
	RtlZeroMemory(pArbiter, sizeof(DRAID_ARBITER_INFO));

	pArbiter->LwrBlocks = ExAllocatePoolWithTag(NonPagedPool, (size_t)SECTOR_SIZE * NDAS_BLOCK_SIZE_LWR,
		DRAID_LWR_BLOCK_POOL_TAG);
	if (NULL ==  pArbiter->LwrBlocks) {
		ExFreePoolWithTag(pArbiter, DRAID_ARBITER_INFO_POOL_TAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
		
	RtlZeroMemory(pArbiter->LwrBlocks, (size_t)SECTOR_SIZE * NDAS_BLOCK_SIZE_LWR);

	KeInitializeSpinLock(&pArbiter->SpinLock);

	pArbiter->Lurn = Lurn;
	pArbiter->Status = DRAID_ARBITER_STATUS_INITALIZING;

	pArbiter->RaidStatus = DRIX_RAID_STATUS_INITIALIZING;

	InitializeListHead(&pArbiter->ClientList);
	InitializeListHead(&pArbiter->AcquiredLockList);
	InitializeListHead(&pArbiter->ToYieldLockList);

	pArbiter->AcquiredLockCount = 0;
	pArbiter->TotalDiskCount = Lurn->LurnChildrenCnt;
	pArbiter->ActiveDiskCount = Lurn->LurnChildrenCnt - pRaidInfo->nSpareDisk;

	pArbiter->LockRangeGranularity = pRaidInfo->SectorsPerBit; // Set to sector per bit for time being..

	pArbiter->SectorsPerOosBmpBit = pRaidInfo->SectorsPerBit;

	pArbiter->OutOfSyncRole = NO_OUT_OF_SYNC_ROLE;

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
	// 2. Set initial lurn status
	// 
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);	
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		ACQUIRE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, &oldIrql2);
		switch(Lurn->LurnChildren[i]->LurnStatus) {
		case LURN_STATUS_RUNNING:
		case LURN_STATUS_STALL:
		case LURN_STATUS_STOP_PENDING:
			Flags = DRIX_NODE_FLAG_RUNNING;
			break;
		case LURN_STATUS_STOP:
		case LURN_STATUS_DESTROYING:
			Flags = DRIX_NODE_FLAG_STOP;
			break;
		case LURN_STATUS_DEFECTIVE:
			Flags = DRIX_NODE_FLAG_DEFECTIVE;
			break;
		case LURN_STATUS_INIT:
		default:
			Flags = DRIX_NODE_FLAG_UNKNOWN;
			break;
		}
		pArbiter->NodeFlags[i] |= Flags;
		KDPrintM(DBG_LURN_ERROR, ("Setting initial node %d flag: %d\n",
			i, pArbiter->NodeFlags[i]));
		RELEASE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, oldIrql2);
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);


	//
	// 3. Map children based on RMD
	//
	ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);	
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		KDPrintM(DBG_LURN_ERROR, ("MAPPING Lurn node %d to RAID role %d\n",
			pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx, i));
		pArbiter->RoleToNodeMap[i] = (UCHAR)pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx;
		pArbiter->NodeToRoleMap[pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx] = (UCHAR)i;

		ASSERT(pArbiter->Rmd.UnitMetaData[i].iUnitDeviceIdx <Lurn->LurnChildrenCnt);
	}
	RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);

	//
	// 4. Apply node information from RMD
	//

	
	for(i = 0; i < Lurn->LurnChildrenCnt; i++) // i : role index.
	{
		if(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & pArbiter->Rmd.UnitMetaData[i].UnitDeviceStatus)
		{
			if (i<pArbiter->ActiveDiskCount) {
				pArbiter->OutOfSyncRole = (UCHAR)i;
				KDPrintM(DBG_LURN_ERROR, ("Node %d(role %d) is out-of-sync\n",  pArbiter->RoleToNodeMap[i], i));
				KDPrintM(DBG_LURN_INFO, ("Setting out of sync role: %d\n", pArbiter->OutOfSyncRole));
			}
		}
		if(NDAS_UNIT_META_BIND_STATUS_DEFECTIVE & pArbiter->Rmd.UnitMetaData[i].UnitDeviceStatus)
		{
			pArbiter->NodeFlags[pArbiter->RoleToNodeMap[i]] |= DRIX_NODE_FLAG_DEFECTIVE;
		
			// fault device found
			KDPrintM(DBG_LURN_ERROR, ("Node %d(role %d) is defective\n",  pArbiter->RoleToNodeMap[i], i));
		}
		pArbiter->DefectCodes[pArbiter->RoleToNodeMap[i]] = pArbiter->Rmd.UnitMetaData[i].DefectCode;
#if 0			
		// fill bitmaps first & write to recover info disk
		LurnSetOutOfSyncBitmapAll(Lurn);
#endif
	}

	//
	// 5. Set initial RAID status.
	//
	DraidArbiterRefreshRaidStatus(pArbiter, TRUE);


	//
	// 6. Read bitmap.
	// 
	ntStatus = DradArbiterInitializeOosBitmap(pArbiter, UpToDateNode);
	
	//
	// 7. Search valid LWR and process LWR
	//

	ntStatus = DraidArbiterProcessDirtyLwr(pArbiter, UpToDateNode);

	//
	//	DraidArbiterProcessDirtyLwr may have changed node status.
	//
	DraidArbiterRefreshRaidStatus(pArbiter, TRUE);

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
	LARGE_INTEGER TimeOut;
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	PDRAID_ARBITER_INFO pArbiterInfo = pRaidInfo->pDraidArbiter;
	NTSTATUS status;
	PDRAID_ARBITER_LOCK_CONTEXT Lock;
	
	ASSERT(pArbiterInfo);
	KDPrintM(DBG_LURN_INFO, ("Stopping DRAID arbiter\n"));

	DraidUnregisterArbiter(pArbiterInfo);
	
	ACQUIRE_SPIN_LOCK(&pArbiterInfo->SpinLock, &oldIrql);
	if(pArbiterInfo->ThreadArbiterHandle)
	{
		pArbiterInfo->Status = DRAID_ARBITER_STATUS_TERMINATING;
		KeSetEvent(&pArbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE); // This will wake up Arbiter thread.
	}
	RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);

	TimeOut.QuadPart = - NANO100_PER_SEC * 120;

	KDPrintM(DBG_LURN_INFO, ("Wait for Arbiter thread completion\n"));
	status = KeWaitForSingleObject(
		pArbiterInfo->ThreadArbiterObject,
		Executive,
		KernelMode,
		FALSE,
		&TimeOut
		);

	KDPrintM(DBG_LURN_INFO, ("Arbiter thread exited\n"));

	ASSERT(status == STATUS_SUCCESS);

	//
	//	Dereference the thread object.
	//

	ObDereferenceObject(pArbiterInfo->ThreadArbiterObject);
	ZwClose(pArbiterInfo->ThreadArbiterHandle);
	
	ACQUIRE_SPIN_LOCK(&pArbiterInfo->SpinLock, &oldIrql);

	pArbiterInfo->ThreadArbiterObject = NULL;
	pArbiterInfo->ThreadArbiterHandle = NULL;

	RELEASE_SPIN_LOCK(&pArbiterInfo->SpinLock, oldIrql);


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
//		KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",pArbiterInfo->AcquiredLockCount));		
		ExFreePoolWithTag(Lock, DRAID_ARBITER_LOCK_POOL_TAG);
	}

#if 0	// to do: clean up remote clients
	while(TRUE) 
	{
		listEntry = RemoveHeadList(&pArbiterInfo->ClientList);
		DraidArbiterTerminateClient(Arbiter, Client);
	}
#endif
//	ASSERT(pArbiterInfo->AcquiredLockCount == 0);

	ExFreePoolWithTag(pArbiterInfo->OosBmpInCoreBuffer, DRAID_BITMAP_POOL_TAG);
	ExFreePoolWithTag(pArbiterInfo->OosBmpOnDiskBuffer, DRAID_BITMAP_POOL_TAG);
	ExFreePoolWithTag(pArbiterInfo->LwrBlocks, DRAID_LWR_BLOCK_POOL_TAG);
	ExFreePoolWithTag(pRaidInfo->pDraidArbiter, DRAID_ARBITER_INFO_POOL_TAG);
	pRaidInfo->pDraidArbiter = NULL;

	return status;
}


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
	InitializeListHead(&pClientContext->PendingLockList);

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
#if 0	// Can't start receiving here. If we start receive here, this receiving will be cancelled when thread reception thread exits
	// Start receiving before register to arbiter.
	if (ConnType == DRIX_CONN_TYPE_REQUEST) {
		Connection->TdiReceiveContext.Irp = NULL;
		KeInitializeEvent(&Connection->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
		status = LpxTdiRecvWithCompletionEvent(
						Connection->ConnectionFileObject,
						&Connection->TdiReceiveContext,
						Connection->ReceiveBuf,
						sizeof(DRIX_HEADER),	0, NULL,NULL
		);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to recv\n"));
			return STATUS_UNSUCCESSFUL;
		} else {
			KDPrintM(DBG_LURN_INFO, ("Starting to recv in request connection\n"));
		}
	}
#endif
	if (ClientExist) {
		if (ConnType == DRIX_CONN_TYPE_NOTIFICATION) {
			if (Client->NotificationConnection) {
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				KDPrintM(DBG_LURN_INFO, ("Notification connection already exist for this client. Unregister previous client\n"));
				DraidArbiterUnregisterClient(Arbiter, Client);
				ClientExist = FALSE;
			} else {
				KDPrintM(DBG_LURN_INFO, ("Set notification connection\n"));
				Client->NotificationConnection = Connection;
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			}
		} else if (ConnType == DRIX_CONN_TYPE_REQUEST) {
			if (Client->RequestConnection) {
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				KDPrintM(DBG_LURN_INFO, ("Request connection already exist for this client. Unregister previous client\n"));
				DraidArbiterUnregisterClient(Arbiter, Client);
				ClientExist = FALSE;
			} else {
				KDPrintM(DBG_LURN_INFO, ("Set request connection\n"));
				Client->RequestConnection = Connection;
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
			}
		} else {
			RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);		
			ASSERT(FALSE);
			return STATUS_UNSUCCESSFUL;
		}
	} else {
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		if (ConnType == DRIX_CONN_TYPE_NOTIFICATION) {
			KDPrintM(DBG_LURN_INFO, ("New client connected with notification type \n"));
		} else if (ConnType == DRIX_CONN_TYPE_REQUEST){
			KDPrintM(DBG_LURN_INFO, ("New client connected with request type \n"));		
		} else {
			ASSERT(FALSE);
		}
	}

	if (!ClientExist)	{
		Client = DraidArbiterAllocClientContext();
		if (!Client)
			return STATUS_INSUFFICIENT_RESOURCES;
		RtlCopyMemory(Client->RemoteClientAddr, Connection->RemoteAddr.Node, 6);
		if (ConnType == DRIX_CONN_TYPE_NOTIFICATION) {
			Client->NotificationConnection = Connection;
		} else if (ConnType == DRIX_CONN_TYPE_REQUEST) {
			Client->RequestConnection = Connection;
		}
		// Link to client list if it was not in the list
		ExInterlockedInsertTailList(&Arbiter->ClientList, &Client->Link, &Arbiter->SpinLock);
	}

	KDPrintM(DBG_LURN_INFO, ("Accepted client %02X:%02X:%02X:%02X:%02X:%02X\n", 
		Client->RemoteClientAddr[0], Client->RemoteClientAddr[1], Client->RemoteClientAddr[2],
		Client->RemoteClientAddr[3], Client->RemoteClientAddr[4], Client->RemoteClientAddr[5]
	));
	
	// Wakeup arbiter to handle this new client
	KeSetEvent(&Arbiter->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE);
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
	ASSERT(pRaidInfo);
	ASSERT(pArbiterInfo);

	KDPrintM(DBG_LURN_INFO, ("Registering local client\n"));

	pClientContext = DraidArbiterAllocClientContext();
	if (pClientContext == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	
	// Init LocalClient values
	pClientContext->NotificationChannel = &pClient->NotificationChannel;
	pClientContext->RequestReplyChannel = &pClient->RequestReplyChannel;
	pClientContext->LocalClient = pClient;
	pClient->RequestChannel = &pClientContext->RequestChannel;
	pClient->NotificationReplyChannel = &pClientContext->NotificationReplyChannel;

	pArbiterInfo->LocalClient = pClientContext;
	// Link to client list
	ExInterlockedInsertTailList(&pArbiterInfo->ClientList, &pClientContext->Link, &pArbiterInfo->SpinLock);

	// Wakeup arbiter to handle this new client
	KeSetEvent(&pArbiterInfo->ArbiterThreadEvent,IO_NO_INCREMENT, FALSE);
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

	DraidArbiterUnregisterClient(pArbiterInfo, pClientContext);

	return STATUS_SUCCESS;
}

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
	
	status = LurnExecuteSync(
		pArbiter->ActiveDiskCount-1,
		LurnsHealthy,
		SCSIOP_READ,
		RebuildInfo->RebuildBuffer,
		Addr,
		(UINT16)Length,
		NULL);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR,
			("Failed to read from healthy disk. RAID failure\n"));
		ASSERT(pArbiter->LocalClient);
		ASSERT(pArbiter->LocalClient->LocalClient);		
		// Report to local client.
		DraidClientUpdateAllNodeFlags(pArbiter->LocalClient->LocalClient);
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
	// WRITE sectors to the defected LURN
	//
	status = LurnExecuteSync(
		1,
		&LurnOutOfSync,
		SCSIOP_WRITE,
		&TargetBuf,
		Addr,
		(UINT16)Length,
		NULL);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_INFO, ("Failed to write to out-of-sync disk\n"));
		ASSERT(pArbiter->LocalClient);
		ASSERT(pArbiter->LocalClient->LocalClient);
		// Report to local client.
		DraidClientUpdateNodeFlags(pArbiter->LocalClient->LocalClient, LurnOutOfSync, 0, 0);
		return status;
	}
	return STATUS_SUCCESS;
}

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

	DoMore = TRUE;
	while(TRUE) {
		if (DoMore) {
			DoMore = FALSE;
		} else {
			KDPrintM(DBG_LURN_INFO, ("Waiting rebuild request\n"));
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

			ASSERT(RebuildInfo->Length);
			
			// Rebuild request should be called only when previous result is cleared.
			ASSERT(RebuildInfo->Status == DRAID_REBUILD_STATUS_NONE);
			RebuildInfo->Status = DRAID_REBUILD_STATUS_WORKING;
			RELEASE_SPIN_LOCK(&pArbiter->SpinLock, oldIrql);	
			DoMore = TRUE;

			CurAddr =  RebuildInfo->Addr;
			EndAddr = RebuildInfo->Addr + RebuildInfo->Length;
			while(TRUE) {
				if (EndAddr - CurAddr > RebuildInfo->UnitRebuildSize/SECTOR_SIZE) {
					IoLength = RebuildInfo->UnitRebuildSize/SECTOR_SIZE;
				} else {
					IoLength = (UINT32)(EndAddr - CurAddr);
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
			}
			
			ACQUIRE_SPIN_LOCK(&pArbiter->SpinLock, &oldIrql);
			RebuildInfo->RebuildRequested = FALSE;
			if (NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_INFO, ("Rebuilding range %I64x:%x done\n", RebuildInfo->Addr, RebuildInfo->Length));
				RebuildInfo->Status = DRAID_REBUILD_STATUS_DONE;
			}else if (RebuildInfo->CancelRequested) {
				KDPrintM(DBG_LURN_INFO, ("Rebuilding range %I64x:%x cancelled\n", RebuildInfo->Addr, RebuildInfo->Length));
				RebuildInfo->Status = DRAID_REBUILD_STATUS_CANCELLED;
			} else {
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
	
	ASSERT(Arbiter->RaidStatus == DRIX_RAID_STATUS_REBUILDING);

	//
	// Check rebuild thread is available 
	//
	ACQUIRE_SPIN_LOCK(&Arbiter->SpinLock, &oldIrql);
	if (RebuildInfo->Status != DRAID_REBUILD_STATUS_NONE || RebuildInfo->RebuildRequested) {
		RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("Rebuild thread is not ready for new request\n"));
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
			// Calc addr, length pair from bitmap
			Addr = BitToRecover * Arbiter->SectorsPerOosBmpBit; 
			Length = Arbiter->SectorsPerOosBmpBit;
			
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
				KDPrintM(DBG_LURN_INFO, ("Bit 0x%x is locked. Searching next dirty bit\n", BitToRecover));		
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
								KDPrintM(DBG_LURN_INFO, ("Requesting lock %I64x:%x to yield\n", Lock->LockAddress, Lock->LockLength));
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
				//
				// Bit is not locked.
				// Lock it myself.
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
				InsertTailList(&Arbiter->AcquiredLockList, &Arbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink);
				InterlockedIncrement(&Arbiter->AcquiredLockCount);
				KDPrintM(DBG_LURN_INFO, ("pArbiter->AcquiredLockCount %d\n",Arbiter->AcquiredLockCount));	
				RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
				// Update LWR before doing IO.
				DradArbiterUpdateOnDiskLwr(Arbiter);
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
		KDPrintM(DBG_LURN_INFO, ("Rebuilding range %I64x:%x is done\n", 
			Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length));
		DradArbiterChangeOosBitmapBit(
			Arbiter, FALSE, Arbiter->RebuildInfo.Addr, Arbiter->RebuildInfo.Length);
		BmpChanged = TRUE;
	} else if (RebuildStatus == DRAID_REBUILD_STATUS_FAILED) {
		// nothing to update
	} else if (RebuildStatus == DRAID_REBUILD_STATUS_CANCELLED) {
		Arbiter->RebuildInfo.CancelRequested = FALSE;

	} else {
		ASSERT(FALSE);
	}

	// Remove from acquired lock list.
	RemoveEntryList(&Arbiter->RebuildInfo.RebuildLock->ArbiterAcquiredLink);	
	
	Arbiter->RebuildInfo.Status = DRAID_REBUILD_STATUS_NONE;
	DraidArbiterFreeLock(Arbiter, Arbiter->RebuildInfo.RebuildLock);
	Arbiter->RebuildInfo.RebuildLock = NULL;
	Arbiter->RebuildInfo.AggressiveRebuildMode = FALSE;
	RELEASE_SPIN_LOCK(&Arbiter->SpinLock, oldIrql);
	DradArbiterUpdateOnDiskLwr(Arbiter);
	if (BmpChanged)
		DradArbiterUpdateOnDiskOosBitmap(Arbiter);
} 


