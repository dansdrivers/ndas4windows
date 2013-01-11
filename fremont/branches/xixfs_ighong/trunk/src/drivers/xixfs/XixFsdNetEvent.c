#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "XixFsComProto.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsAreYouHaveLotLock)
#pragma alloc_text(PAGE, XixFsIHaveLotLock)
#pragma alloc_text(PAGE, XixFsdSendRenameLinkBC)
#pragma alloc_text(PAGE, XixFsdSendFileChangeRC)
#endif


BOOLEAN
XixFsAreYouHaveLotLock(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint8		* LockOwnerMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint8		* LockOwnerId
)
{
	// Request LotLock state to Lock Owner
	
	
	NTSTATUS					RC = STATUS_UNSUCCESSFUL;
	PXIFSDG_PKT					pPacket = NULL;
	PXIFS_LOCK_REQUEST			pPacketData = NULL;
	XIFS_LOCK_CONTROL			LockControl;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsAreYouHaveLotLock \n"));

	ASSERT(Wait == TRUE);
	
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, LockOwnerMac, XIFS_TYPE_LOCK_REQUEST))
	{
		return FALSE;
	}


	// Changed by ILGU HONG
	//	chesung suggest
	/*
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Packet Dest Info  [0x%02x:%02x:%02x:%02x:%02x:%02x]\n",
		LockOwnerMac[0], LockOwnerMac[1], LockOwnerMac[2],
		LockOwnerMac[3], LockOwnerMac[4], LockOwnerMac[5]));
	*/
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Packet Dest Info  [0x%02x:%02x:%02x:%02x:%02x:%02x]\n",
		LockOwnerMac[26], LockOwnerMac[27], LockOwnerMac[28],
		LockOwnerMac[29], LockOwnerMac[30], LockOwnerMac[31]));

	pPacketData = &(pPacket->RawDataDG.LockReq);
	RtlCopyMemory(pPacketData->LotOwnerID, LockOwnerId, 16);
	// Changed by ILGU HONG
	//	chesung suggest
	//RtlCopyMemory(pPacketData->DiskId, DiskId, 6);
	//RtlCopyMemory(pPacketData->LotOwnerMac, LockOwnerMac, 6);
	RtlCopyMemory(pPacketData->DiskId, DiskId, 16);
	RtlCopyMemory(pPacketData->LotOwnerMac, LockOwnerMac, 32);
	pPacketData->PartionId = HTONL(PartitionId);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacketData->PacketNumber = HTONL(XiGlobalData.XifsComCtx.PacketNumber);
	XiGlobalData.XifsComCtx.PacketNumber++;

	pPacket->TimeOut.QuadPart = XixGetSystemTime().QuadPart + DEFAULT_REQUEST_MAX_TIMEOUT;


	KeInitializeEvent(&LockControl.KEvent, SynchronizationEvent, FALSE);
	LockControl.Status = LOCK_INVALID;

	pPacket->pLockContext = &LockControl;


	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	RC = KeWaitForSingleObject(&LockControl.KEvent, Executive, KernelMode, FALSE, NULL);

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XixFsAreYouHaveLotLock \n"));
		return FALSE;
	}

	if(LockControl.Status == LOCK_OWNED_BY_OWNER){
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XixFsAreYouHaveLotLock Lock is realy acquired by other \n"));
		return TRUE;
	}


	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdAreYouHaveLotLock Lock is status(0x%x) \n", LockControl.Status));
	/*
		TRUE --> Lock is Owner by Me
		FALSE --> Lock is Not Mine
		FALSE--> TimeOut Lock Owner is not in Network
	*/
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdAreYouHaveLotLock \n"));

	return FALSE;
}


BOOLEAN
XixFsIHaveLotLock(
	IN uint8	* HostMac,
	IN uint64	 LotNumber,
	IN uint8	* DiskId,
	IN uint32	 PartitionId
)
{
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	PXIFSDG_PKT			pPacket = NULL;
	PXIFS_LOCK_BROADCAST	pPacketData = NULL;
	uint8				DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsIHaveLotLock \n"));
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_LOCK_BROADCAST))
	{
		return FALSE;
	}
	*/
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, NULL, XIFS_TYPE_LOCK_BROADCAST))
	{
		return FALSE;
	}


	pPacketData = &(pPacket->RawDataDG.LockBroadcast);

	// Changed by ILGU HONG
	//RtlCopyMemory(pPacketData->DiskId, DiskId, 6);
	RtlCopyMemory(pPacketData->DiskId, DiskId, 16);
	
	pPacketData->PartionId = HTONL(PartitionId);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacket->TimeOut.QuadPart = XixGetSystemTime().QuadPart + DEFAULT_REQUEST_MAX_TIMEOUT;

	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsIHaveLotLock \n"));
	return TRUE;
}


NTSTATUS
XixFsdSendRenameLinkBC(
	IN BOOLEAN		Wait,
	IN uint32		SubCommand,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentLotNumber
)
{
	NTSTATUS						RC = STATUS_UNSUCCESSFUL;
	PXIFSDG_PKT						pPacket = NULL;
	PXIFS_FILE_CHANGE_BROADCAST		pPacketData = NULL;
	XIFS_LOCK_CONTROL				LockControl;
	uint8							DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XifsdSendRenameLinkBC \n"));

	ASSERT(Wait == TRUE);
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_FILE_CHANGE))
	{
		return FALSE;
	}
	*/
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, NULL, XIFS_TYPE_FILE_CHANGE))
	{
		return FALSE;
	}

	pPacketData = &(pPacket->RawDataDG.FileChangeReq);
	// Changed by ILGU HONG
	// RtlCopyMemory(pPacketData->DiskId, DiskId, 6);
	RtlCopyMemory(pPacketData->DiskId, DiskId, 16);
	pPacketData->PartionId = HTONL(PartitionId);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacketData->PrevParentLotNumber = HTONLL(OldParentLotNumber);
	pPacketData->NewParentLotNumber = HTONLL(NewParentLotNumber);
	pPacketData->SubCommand =HTONL(SubCommand);

	pPacket->TimeOut.QuadPart = XixGetSystemTime().QuadPart + DEFAULT_REQUEST_MAX_TIMEOUT;




	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdSendRenameLinkBC \n"));

	return STATUS_SUCCESS;	
}


NTSTATUS
XixFsdSendFileChangeRC(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint64		FileLength,
	IN uint64		AllocationLength,
	IN uint64		UpdateStartOffset
)
{
	NTSTATUS							RC = STATUS_UNSUCCESSFUL;
	PXIFSDG_PKT							pPacket = NULL;
	PXIFS_FILE_LENGTH_CHANGE_BROADCAST	pPacketData = NULL;
	uint8								DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsdSendFileChangeRC \n"));

	ASSERT(Wait == TRUE);
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_FILE_LENGTH_CHANGE))
	{
		return FALSE;
	}
	*/
	
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_FILE_LENGTH_CHANGE))
	{
		return FALSE;
	}



	pPacketData = &(pPacket->RawDataDG.FileLenChangeReq);
	// Changed by ILGU HONG
	//	RtlCopyMemory(pPacketData->DiskId, DiskId, 6);
	RtlCopyMemory(pPacketData->DiskId, DiskId, 16);
	pPacketData->PartionId = HTONL(PartitionId);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacketData->FileLength = HTONLL(FileLength);
	pPacketData->AllocationLength = HTONLL(AllocationLength);
	pPacketData->WriteStartOffset =	HTONLL(UpdateStartOffset);
	pPacket->TimeOut.QuadPart = XixGetSystemTime().QuadPart + DEFAULT_REQUEST_MAX_TIMEOUT;




	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XixFsdSendFileChangeRC \n"));

	return STATUS_SUCCESS;	
}
