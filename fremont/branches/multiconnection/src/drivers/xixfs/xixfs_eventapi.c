#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"
#include "xcsystem/system.h"
#include "xixcore/callback.h"



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixcore_HaveLotLock)
#pragma alloc_text(PAGE, xixfs_IHaveLotLock)
#pragma alloc_text(PAGE, xixfs_SendRenameLinkBC)
#pragma alloc_text(PAGE, xixfs_SendFileChangeRC)
#endif





BOOLEAN
xixfs_IHaveLotLock(
	IN uint8	* HostMac,
	IN uint64	 LotNumber,
	IN uint8	* VolumeId
)
{
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	PXIXFSDG_PKT			pPacket = NULL;
	PXIXFS_LOCK_BROADCAST	pPacketData = NULL;
	uint8				DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixfs_IHaveLotLock \n"));
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==xixfs_AllocDGPkt(&pPacket, HostMac, DstAddress, XIXFS_TYPE_LOCK_BROADCAST))
	{
		return FALSE;
	}
	*/
	if(FALSE ==xixfs_AllocDGPkt(&pPacket, HostMac, NULL, XIXFS_TYPE_LOCK_BROADCAST))
	{
		return FALSE;
	}


	pPacketData = &(pPacket->RawDataDG.LockBroadcast);

	// Changed by ILGU HONG
	RtlCopyMemory(pPacketData->VolumeId, VolumeId, 16);
	
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacket->TimeOut.QuadPart = xixcore_GetCurrentTime64() + DEFAULT_REQUEST_MAX_TIMEOUT;

	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixfs_IHaveLotLock \n"));
	return TRUE;
}


NTSTATUS
xixfs_SendRenameLinkBC(
	IN BOOLEAN		Wait,
	IN uint32		SubCommand,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* VolumeId,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentLotNumber
)
{
	NTSTATUS						RC = STATUS_UNSUCCESSFUL;
	PXIXFSDG_PKT						pPacket = NULL;
	PXIXFS_FILE_CHANGE_BROADCAST		pPacketData = NULL;
	XIFS_LOCK_CONTROL				LockControl;
	uint8							DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XifsdSendRenameLinkBC \n"));

	ASSERT(Wait);
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==xixfs_AllocDGPkt(&pPacket, HostMac, DstAddress, XIXFS_TYPE_FILE_CHANGE))
	{
		return FALSE;
	}
	*/
	if(FALSE ==xixfs_AllocDGPkt(&pPacket, HostMac, NULL, XIXFS_TYPE_FILE_CHANGE))
	{
		return FALSE;
	}

	pPacketData = &(pPacket->RawDataDG.FileChangeReq);
	// Changed by ILGU HONG
	RtlCopyMemory(pPacketData->VolumeId, VolumeId, 16);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacketData->PrevParentLotNumber = HTONLL(OldParentLotNumber);
	pPacketData->NewParentLotNumber = HTONLL(NewParentLotNumber);
	pPacketData->SubCommand =HTONL(SubCommand);

	pPacket->TimeOut.QuadPart = xixcore_GetCurrentTime64() + DEFAULT_REQUEST_MAX_TIMEOUT;




	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdSendRenameLinkBC \n"));

	return STATUS_SUCCESS;	
}


NTSTATUS
xixfs_SendFileChangeRC(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* VolumeId,
	IN uint64		FileLength,
	IN uint64		AllocationLength,
	IN uint64		UpdateStartOffset
)
{
	NTSTATUS							RC = STATUS_UNSUCCESSFUL;
	PXIXFSDG_PKT							pPacket = NULL;
	PXIXFS_FILE_LENGTH_CHANGE_BROADCAST	pPacketData = NULL;
	uint8								DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixfs_SendFileChangeRC \n"));

	ASSERT(Wait);
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==xixfs_AllocDGPkt(&pPacket, HostMac, DstAddress, XIXFS_TYPE_FILE_LENGTH_CHANGE))
	{
		return FALSE;
	}
	*/
	
	if(FALSE ==xixfs_AllocDGPkt(&pPacket, HostMac, DstAddress, XIXFS_TYPE_FILE_LENGTH_CHANGE))
	{
		return FALSE;
	}



	pPacketData = &(pPacket->RawDataDG.FileLenChangeReq);
	// Changed by ILGU HONG
	RtlCopyMemory(pPacketData->VolumeId, VolumeId, 16);
	pPacketData->LotNumber = HTONLL(LotNumber);
	pPacketData->FileLength = HTONLL(FileLength);
	pPacketData->AllocationLength = HTONLL(AllocationLength);
	pPacketData->WriteStartOffset =	HTONLL(UpdateStartOffset);
	pPacket->TimeOut.QuadPart = xixcore_GetCurrentTime64() + DEFAULT_REQUEST_MAX_TIMEOUT;




	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pPacket->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit xixfs_SendFileChangeRC \n"));

	return STATUS_SUCCESS;	
}
