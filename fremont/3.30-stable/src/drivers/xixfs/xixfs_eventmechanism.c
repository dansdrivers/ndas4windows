#include "xixfs_types.h"
#include "xixfs_debug.h"

#include "xcsystem/system.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_event.h"
#include "xixfs_global.h"
#include "xixfs_internal.h"






NTSTATUS
xixfs_EventIsFileLocked(
	IN PXIXFS_VCB		pVCB,
	IN uint64			LotNumber,
	IN OUT uint32		*LockState
	);

NTSTATUS
xixfs_EventSendLockReply(
	IN PXIXFS_VCB 		pVCB,
	IN PXIXFSDG_PKT		pRequest,
	IN uint32			LockState
);

VOID
xixfs_EventProcessLockRequest(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID	Parameter
);


NTSTATUS
xixfs_EventCheckLockRequest(
		IN PXIFS_COMM_CTX	SvrCtx,
		IN PXIXFSDG_PKT		pRequest
);


NTSTATUS
xixfs_EventSetFileLock(
	IN PXIXFS_VCB		pVCB,
	IN uint64			LotNumber
	);


VOID
xixfs_EventProcessLockBroadCast(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID	Parameter
);

NTSTATUS
xixfs_EventFileFlush(
	IN PXIXFS_VCB 	pVCB, 
	IN uint64		LotNumber,
	IN uint64		StartOffset, 
	IN uint32		DataSize
	);

VOID
xixfs_EventProcessFlushBroadCast(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID	Parameter
);

NTSTATUS
xixfs_EventChangeFileLen(
	IN PXIXFS_VCB		pVCB,
	IN uint64		LotNumber,
	IN uint64		FileLen,
	IN uint64		AllocationLen,
	IN uint64		WriteStartOffset
);	


VOID
xixfs_EventProcessFileLenChange(
	IN PDEVICE_OBJECT DeviceObject,					
	IN PVOID	Parameter
);


NTSTATUS
xixfs_EventFileChange(
	IN PXIXFS_VCB	pVCB,
	IN uint64		LotNumber,
	IN uint32		SubCommand,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentFcb
);

VOID
xixfs_EventProcessFileChange(
	IN PDEVICE_OBJECT DeviceObject,				
	IN PVOID	Parameter
);


NTSTATUS
xixfs_EventDirChange(
	IN PXIXFS_VCB pVCB, 
	IN uint64	LotNumber, 
	IN uint32 ChildSlotNumber,
	IN uint32 SubCommand
);


VOID
xixfs_EventProcessDirChange(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID	Parameter
);


BOOLEAN
xixfs_EventDispatchDGReqPkt(
				IN PXIFS_COMM_CTX		SvrCtx,
				IN OUT PXIXFSDG_PKT		pRequest
);





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_EventIsFileLocked)
#pragma alloc_text(PAGE, xixfs_EventSendLockReply)
#pragma alloc_text(PAGE, xixfs_EventProcessLockRequest)
#pragma alloc_text(PAGE, xixfs_EventCheckLockRequest)
#pragma alloc_text(PAGE, xixfs_EventSetFileLock)
#pragma alloc_text(PAGE, xixfs_EventProcessLockBroadCast)
#pragma alloc_text(PAGE, xixfs_EventFileFlush)
#pragma alloc_text(PAGE, xixfs_EventProcessFlushBroadCast)
#pragma alloc_text(PAGE, xixfs_EventFileChange)
#pragma alloc_text(PAGE, xixfs_EventProcessFileChange)
#pragma alloc_text(PAGE, xixfs_EventChangeFileLen)
#pragma alloc_text(PAGE, xixfs_EventProcessFileLenChange)
#pragma alloc_text(PAGE, xixfs_EventDirChange)
#pragma alloc_text(PAGE, xixfs_EventProcessDirChange)
#pragma alloc_text(PAGE, xixfs_EventDispatchDGReqPkt)
#pragma alloc_text(PAGE, xixfs_EventComCliThreadProc)
#pragma alloc_text(PAGE, xixfs_EventComSvrThreadProc)
#endif


VOID
xixfs_SevEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) {
	PXIFS_COMM_CTX	ServCtx = NULL;
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_SevEventCallBack\n" ) );
	ServCtx = (PXIFS_COMM_CTX)Context;

	KeSetEvent(&ServCtx->ServNetworkEvent, 0, FALSE);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_SevEventCallBack\n" ) );
	return;
}


VOID
xixfs_CliEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) {
	PXIFS_COMM_CTX	CliCtx = (PXIFS_COMM_CTX)Context;
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_CliEventCallBack\n" ) );

	KeSetEvent(&CliCtx->CliNetworkEvent, 0, FALSE);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_CliEventCallBack\n" ) );

	return;
}


NTSTATUS
xixfs_EventSvrDatagramRecvHandler(
		   IN PVOID		TdiEventContext,
		   IN LONG		SourceAddressLength,
		   IN PVOID		SourceAddress,
		   IN LONG		OptionsLength,
		   IN PVOID		Options,
		   IN ULONG		ReceiveDatagramFlags,
		   IN ULONG		BytesIndicated,
		   IN ULONG		BytesAvailable,
		   OUT ULONG	*BytesTaken,
		   IN PVOID		Tsdu,
		   OUT PIRP		*IoRequestPacket
		   )
{
	PXIFS_COMM_CTX			SvrCtx = (PXIFS_COMM_CTX)TdiEventContext;
	PTRANSPORT_ADDRESS		ClientAddr = (PTRANSPORT_ADDRESS)SourceAddress;
	PLPX_ADDRESS			ClientLpxAddr = (PLPX_ADDRESS)ClientAddr->Address[0].Address;
	PXIXFSDG_PKT				Pkt;
	BOOLEAN					bRet;
	static UCHAR			Protocol[4] = XIXFS_DATAGRAM_PROTOCOL;
	
	

	UNREFERENCED_PARAMETER(SourceAddressLength);
	UNREFERENCED_PARAMETER(OptionsLength);
	UNREFERENCED_PARAMETER(Options);
	UNREFERENCED_PARAMETER(ReceiveDatagramFlags);
	UNREFERENCED_PARAMETER(BytesTaken);
	UNREFERENCED_PARAMETER(IoRequestPacket);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter XifsSvrDatagramRecvHandler\n" ) );



	if(	BytesAvailable < sizeof(XIXFS_COMM_HEADER) ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsSvrDatagramRecvHandler: too small bytes.\n" ) );
		//DbgPrint("DROP PACKET size is too small !!!\n");
		goto not_accepted;
	}

	bRet = xixfs_AllocDGPkt(
				&Pkt,
				NULL,
				NULL,
				XIXFS_TYPE_UNKOWN
			);
	
	if(FALSE == bRet) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsSvrDatagramRecvHandler: xixfs_AllocDGPkt() failed.\n"));
		//DbgPrint("DROP PACKET can't allocate pkt !!!\n");
		goto not_accepted;
	}

	RtlCopyMemory(&Pkt->RawHeadDG, Tsdu, sizeof(Pkt->RawHeadDG) );

	if(	RtlCompareMemory(Pkt->RawHeadDG.Protocol, Protocol, 4) != 4 ||
		NTOHL(Pkt->RawHeadDG.XifsMajorVersion) != XIXFS_PROTO_MAJOR_VERSION ||
		NTOHL(Pkt->RawHeadDG.XifsMinorVersion) != XIXFS_PROTO_MINOR_VERSION ||
		!(NTOHL(Pkt->RawHeadDG.Type) & XIXFS_TYPE_MASK)  ) {
		/*
		DbgPrint("DROP PROTOC(%c,%c,%c,%c) MV(%d) MIV(%d) TYPE(%d)\n",
				Pkt->RawHeadDG.Protocol[0],
				Pkt->RawHeadDG.Protocol[1],
				Pkt->RawHeadDG.Protocol[2],
				Pkt->RawHeadDG.Protocol[3],
				NTOHL(Pkt->RawHeadDG.XifsMajorVersion),
				NTOHL(Pkt->RawHeadDG.XifsMinorVersion),
				NTOHL(Pkt->RawHeadDG.Type)
				);
		

		DbgPrint("DROP PACKET\n");
		*/
		xixfs_DereferenceDGPkt(Pkt);
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("[LFS] LfsSvrDatagramRecvHandler: Invalid reply header.\n"));
		goto not_accepted;
	}

	/*
	DbgPrint("PROTOC(%c,%c,%c,%c) MV(%d) MIV(%d) TYPE(%d)\n",
		Pkt->RawHeadDG.Protocol[0],
		Pkt->RawHeadDG.Protocol[1],
		Pkt->RawHeadDG.Protocol[2],
		Pkt->RawHeadDG.Protocol[3],
		NTOHL(Pkt->RawHeadDG.XifsMajorVersion),
		NTOHL(Pkt->RawHeadDG.XifsMinorVersion),
		NTOHL(Pkt->RawHeadDG.Type)
		);
	*/




	Pkt->PacketSize	= NTOHL(Pkt->RawHeadDG.MessageSize);
	Pkt->DataSize	=  NTOHL(Pkt->RawHeadDG.MessageSize) - sizeof(XIXFS_COMM_HEADER);		

	if( Pkt->PacketSize != sizeof(XIXFS_COMM_HEADER) + Pkt->DataSize ) {

		xixfs_DereferenceDGPkt(Pkt);
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsSvrDatagramRecvHandler: Invalid reply packet size.\n"));
		goto not_accepted;
	}
	
	//
	//	read the data
	//
	RtlCopyMemory(&Pkt->RawDataDG, (PUCHAR)Tsdu + sizeof(Pkt->RawHeadDG), Pkt->DataSize);

	//
	//	insert to the packet queue
	//
	InitializeListHead(&Pkt->PktListEntry);
	
	
	InsertTailList(&SvrCtx->RecvPktList,&Pkt->PktListEntry);
	
	//
	//	extract the source address
	//
	RtlCopyMemory(&Pkt->SourceAddr, ClientLpxAddr, sizeof(LPX_ADDRESS));
	KeSetEvent(&SvrCtx->ServDatagramRecvEvent, IO_NO_INCREMENT, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit XifsSvrDatagramRecvHandler\n" ) );

	return STATUS_SUCCESS;
	
not_accepted:	
	DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,( "[LFS] LfsSvrDatagramRecvHandler: a datagram packet rejected.\n"));
	return STATUS_DATA_NOT_ACCEPTED;

}

NTSTATUS
xixfs_EventIsFileLocked(
	IN PXIXFS_VCB		pVCB,
	IN uint64			LotNumber,
	IN OUT uint32		*LockState
	)
{
	PXIXFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventIsFileLocked\n" ) );
	
	
	if(LotNumber == pVCB->XixcoreVcb.MetaContext.HostRegLotMapIndex){
		if(pVCB->XixcoreVcb.MetaContext.HostRegLotMapLockStatus == FCB_FILE_LOCK_HAS){
			*LockState = FCB_FILE_LOCK_HAS;
		}else{
			*LockState = FCB_FILE_LOCK_INVALID;
		}
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), 
			( "LockState (%x)\n", *LockState ) );

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), 
			( "VCB lock Request !!! Exit xixfs_EventIsFileLocked\n"));

		return STATUS_SUCCESS;
	}


	pFCB = xixfs_FCBTLBLookupEntry(pVCB,LotNumber);
	
	if(pFCB){
		
		if(pFCB->XixcoreFcb.HasLock == FCB_FILE_LOCK_HAS){
			*LockState = FCB_FILE_LOCK_HAS;
		}else {
			*LockState = FCB_FILE_LOCK_INVALID;
		}
		RC = STATUS_SUCCESS;
	}else{
	
		*LockState = FCB_FILE_LOCK_INVALID;
		RC =  STATUS_SUCCESS;
	}
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), ( "LockState (%x)\n", *LockState ) );

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventIsFileLocked Status (0x%x)\n", RC ) );
	return RC;	
}


NTSTATUS
xixfs_EventSendLockReplyFromAuxLotLockInfo(
	IN PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo,
	IN PXIXFSDG_PKT		pRequest,
	IN uint32			LockState
)
{
	PXIXFS_COMM_HEADER	pHeader = NULL;
	PXIXFS_LOCK_REPLY		pPacketData = NULL;
	uint64				LotNumber = 0;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventSendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);

	
	RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 32);
	RtlCopyMemory(pHeader->SrcMac, AuxLotInfo->HostMac, 32);

	pHeader->Type = HTONL(XIXFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == FCB_FILE_LOCK_HAS){
		pPacketData->LotState = HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = HTONL(LOCK_INVALID);
	}

	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pRequest->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventSendLockReply\n" ) );
	return STATUS_SUCCESS;
}


NTSTATUS
xixfs_EventSendLockReplyRaw(
	IN PXIXFSDG_PKT		pRequest,
	IN uint8			*SrcMac,
	IN uint32			LockState
)
{
	PXIXFS_COMM_HEADER	pHeader = NULL;
	PXIXFS_LOCK_REPLY	pPacketData = NULL;
	uint64				LotNumber = 0;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventSendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);	
	RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 32);
	RtlCopyMemory(pHeader->SrcMac, SrcMac, 32);

	pHeader->Type = HTONL(XIXFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == FCB_FILE_LOCK_HAS){
		pPacketData->LotState = HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = HTONL(LOCK_INVALID);
	}

	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pRequest->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventSendLockReply\n" ) );
	return STATUS_SUCCESS;
}



NTSTATUS
xixfs_EventSendLockReply(
	IN PXIXFS_VCB 		pVCB,
	IN PXIXFSDG_PKT		pRequest,
	IN uint32			LockState
)
{
	PXIXFS_COMM_HEADER	pHeader = NULL;
	PXIXFS_LOCK_REPLY		pPacketData = NULL;
	uint64				LotNumber = 0;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventSendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);

	RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 32);
	RtlCopyMemory(pHeader->SrcMac, pVCB->XixcoreVcb.HostMac, 32);

	pHeader->Type = HTONL(XIXFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == FCB_FILE_LOCK_HAS){
		pPacketData->LotState = HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = HTONL(LOCK_INVALID);
	}

	ExAcquireFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pRequest->PktListEntry) );
	ExReleaseFastMutexUnsafe(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventSendLockReply\n" ) );
	return STATUS_SUCCESS;
}

VOID
xixfs_EventProcessLockRequest(
		IN PDEVICE_OBJECT DeviceObject,
		IN PVOID	Parameter
)
{
	PXIXFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIXFS_VCB	pVCB = NULL;
	PXIXFS_LOCK_REQUEST	pDataHeader = NULL;
	uint32				LockState = FCB_FILE_LOCK_INVALID;
	

	PAGED_CODE();
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessLockRequest\n" ) );

	pRequest = (PXIXFSDG_PKT)Parameter;
	ASSERT(pRequest);
	
	

	pDataHeader = &(pRequest->RawDataDG.LockReq);
	
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);
		// Changed by ILGU HONG

		if( RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 16){
			
					
			RC = xixfs_EventIsFileLocked(pVCB, NTOHLL(pDataHeader->LotNumber), &LockState);
			
			if(NT_SUCCESS(RC)){
				IoFreeWorkItem(pRequest->WorkQueueItem);
				// this pk disappered by client routine
				xixfs_EventSendLockReply(pVCB, pRequest, LockState);
				//DbgPrint("!!! Send Lock Reply 1!!\n");
				XifsdReleaseGData(TRUE);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessLockRequest\n" ) );
				XixFsdDecGlobalWorkItem();
				
				return;
			}else{
				XifsdReleaseGData(TRUE);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessLockRequest\n" ) );
				IoFreeWorkItem(pRequest->WorkQueueItem);
				xixfs_DereferenceDGPkt(pRequest);
				XixFsdDecGlobalWorkItem();
				
				return ;
			}
			
		}



		pVCB = NULL;
		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);

	if(pVCB == NULL){
		PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
		PXIXCORE_LISTENTRY			pAuxLotLockList = NULL;
		BOOLEAN						bNewEntry = FALSE;
		XIXCORE_IRQL				oldIrql;

		
		xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);

		pAuxLotLockList = xixcore_global.tmp_lot_lock_list.next;
		// changed by ILGU HONG
		
		while(pAuxLotLockList != &(xixcore_global.tmp_lot_lock_list))
		{
			AuxLotInfo = CONTAINING_RECORD(pAuxLotLockList, XIXCORE_AUXI_LOT_LOCK_INFO, AuxLink);
			if((RtlCompareMemory(pDataHeader->VolumeId, AuxLotInfo->VolumeId, 16) == 16) && 
				(NTOHLL(pDataHeader->LotNumber) == AuxLotInfo->LotNumber) )
			{
				
				xixcore_RefAuxLotLock(AuxLotInfo);
				xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
				IoFreeWorkItem(pRequest->WorkQueueItem);
				LockState = AuxLotInfo->HasLock;
				xixfs_EventSendLockReplyFromAuxLotLockInfo(AuxLotInfo, pRequest, LockState);
				//DbgPrint("!!! Send Lock Reply 2!!\n");
				xixcore_DeRefAuxLotLock(AuxLotInfo);
				XixFsdDecGlobalWorkItem();
				
				return;
			}

			AuxLotInfo = NULL;
			pAuxLotLockList = pAuxLotLockList->next;
		}

		xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
		IoFreeWorkItem(pRequest->WorkQueueItem);
		xixfs_EventSendLockReplyRaw(pRequest, XiGlobalData.HostMac, FCB_FILE_LOCK_INVALID);
		//DbgPrint("!!! Send Lock Reply 3!!\n");
		XixFsdDecGlobalWorkItem();
		
		return;

	}

	IoFreeWorkItem(pRequest->WorkQueueItem);
	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessLockRequest\n" ) );
	XixFsdDecGlobalWorkItem();
	
	return;	
}


NTSTATUS
xixfs_EventCheckLockRequest(
		IN PXIFS_COMM_CTX	SvrCtx,
		IN PXIXFSDG_PKT		pRequest
)
{
	PLIST_ENTRY	pListEntry = NULL;
	KIRQL		OldIrql;
	PXIXFSDG_PKT	pPkt = NULL;
	PXIXFS_LOCK_REQUEST 	pReqDataHeader = NULL;
	PXIXFS_LOCK_REPLY		pReplyDataHeader = NULL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventCheckLockRequest\n" ) );

	pReplyDataHeader = &pRequest->RawDataDG.LockReply;
	
	
	ExAcquireFastMutexUnsafe(&SvrCtx->SendPktListMutex);
	pListEntry = SvrCtx->SendPktList.Flink;
	while(pListEntry != &(SvrCtx->SendPktList)){
		pPkt = (PXIXFSDG_PKT)CONTAINING_RECORD(pListEntry, XIXFSDG_PKT, PktListEntry);
		if( XIXFS_TYPE_LOCK_REQUEST == NTOHL(pPkt->RawHeadDG.Type) ){
			pReqDataHeader = &pPkt->RawDataDG.LockReq;
		
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				("Req #Packet(%ld)  LotNumber(%I64d) : Rep #Packet(%ld)  LotNumber(%I64d)\n",
					NTOHL(pReqDataHeader->PacketNumber), NTOHLL(pReqDataHeader->LotNumber),
					NTOHL(pReplyDataHeader->PacketNumber), NTOHLL(pReplyDataHeader->LotNumber) )); 


			// Changed by ILGU HONG
			if((NTOHL(pReqDataHeader->PacketNumber) == NTOHL(pReplyDataHeader->PacketNumber))
				&& (RtlCompareMemory(pReqDataHeader->VolumeId, pReplyDataHeader->VolumeId, 16) == 16)
				&& ( NTOHLL(pReqDataHeader->LotNumber) == NTOHLL(pReplyDataHeader->LotNumber)) )
			{	

				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
					("Search Req and Set Event\n"));

				RemoveEntryList(&pPkt->PktListEntry);
				InitializeListHead(&pPkt->PktListEntry);
				pPkt->pLockContext->Status = NTOHL(pReplyDataHeader->LotState);
				KeSetEvent(&(pPkt->pLockContext->KEvent), 0, FALSE);

				//DbgPrint("!!! Receive Lock Reply 4!!\n");

				xixfs_DereferenceDGPkt(pPkt);
				ExReleaseFastMutexUnsafe(&SvrCtx->SendPktListMutex);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventCheckLockRequest Event SET!!\n" ) );
				return STATUS_SUCCESS;
			}

		}
		pListEntry = pListEntry->Flink;
	}
	ExReleaseFastMutexUnsafe(&SvrCtx->SendPktListMutex);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventCheckLockRequest\n" ) );
	return STATUS_SUCCESS;
}

NTSTATUS
xixfs_EventSetFileLock(
	IN PXIXFS_VCB		pVCB,
	IN uint64			LotNumber
	)
{
	PXIXFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventSetFileLock\n" ) );
	
	pFCB = xixfs_FCBTLBLookupEntry(pVCB,LotNumber);
	
	if(pFCB){

		if(pFCB->XixcoreFcb.HasLock == FCB_FILE_LOCK_OTHER_HAS){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventSetFileLock FCB_FILE_LOCK_OTHER_HAS \n" ) );	
			RC = STATUS_SUCCESS;
		}
		else {
			pFCB->XixcoreFcb.HasLock = FCB_FILE_LOCK_OTHER_HAS;
		}
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventSetFileLock FCB_FILE_LOCK_OTHER_HAS\n" ) );	
		return STATUS_SUCCESS;	
	}
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventSetFileLock\n" ) );	
	return STATUS_SUCCESS;	
}


VOID
xixfs_EventProcessLockBroadCast(
		IN PDEVICE_OBJECT DeviceObject,
		IN PVOID			Parameter
)
{
	PXIXFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIXFS_VCB	pVCB = NULL;
	PXIXFS_LOCK_BROADCAST	pDataHeader = NULL;

	

	PAGED_CODE();
	
	pRequest = (PXIXFSDG_PKT)Parameter;
	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessLockBroadCast\n" ) );
	

	pDataHeader = &(pRequest->RawDataDG.LockBroadcast);
	
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);

		// Changed by ILGU HONG
		if( RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 16 ){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = xixfs_EventSetFileLock(pVCB, NTOHLL(pDataHeader->LotNumber));
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			IoFreeWorkItem(pRequest->WorkQueueItem);
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessLockBroadCast\n" ) );
			XixFsdDecGlobalWorkItem();	
			
			return ;
		}

		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	IoFreeWorkItem(pRequest->WorkQueueItem);
	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessLockBroadCast\n" ) );
	XixFsdDecGlobalWorkItem();
	
	return ;
}

NTSTATUS
xixfs_EventFileFlush(
	IN PXIXFS_VCB 	pVCB, 
	IN uint64		LotNumber,
	IN uint64		StartOffset, 
	IN uint32		DataSize
	)
{
	PXIXFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;
	IO_STATUS_BLOCK  IoStatus;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventFileFlush\n" ) );
	
	pFCB = xixfs_FCBTLBLookupEntry(pVCB,LotNumber);

	if(pFCB){
		
		XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
		
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
			ByteOffset.QuadPart = StartOffset;

			if (pFCB->SectionObject.ImageSectionObject != NULL) {

				MmFlushImageSection( &pFCB->SectionObject, MmFlushForWrite );
			}



			if(pFCB->SectionObject.DataSectionObject != NULL) 
			{
				CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);
				//CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
				/*
				CcFlushCache(&(pFCB->SectionObject), &ByteOffset, DataSize, &IoStatus);
				DbgPrint("CcFlush  2 File(%wZ)\n", &pFCB->FCBFullPath);
				if (!NT_SUCCESS(IoStatus.Status)) {
					XifsdReleaseFcb(TRUE, pFCB);
					return STATUS_SUCCESS;
				}
				*/
				//CcPurgeCacheSection(&(pFCB->SectionObject), &ByteOffset, DataSize, FALSE);
				//CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
				
				
			}
		}

		XifsdReleaseFcb(TRUE, pFCB);


		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventFileFlush\n" ) );
		return RC;
	}
	
	RC = STATUS_SUCCESS;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventFileFlush\n" ) );
	return RC;	
}



VOID
xixfs_EventProcessFlushBroadCast(
		IN PDEVICE_OBJECT DeviceObject,
		IN PVOID			Parameter
)
{
	PXIXFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIXFS_VCB	pVCB = NULL;
	PXIXFS_RANGE_FLUSH_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();

	
	pRequest = (PXIXFSDG_PKT)Parameter;
	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessFlushBroadCast\n" ) );

	pDataHeader = &(pRequest->RawDataDG.FlushReq);
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);

		// Changed by ILGU HONG
		if( RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 16){
			
			
			RC = xixfs_EventFileFlush(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHLL(pDataHeader->StartOffset), 
					NTOHL(pDataHeader->DataSize));

			XifsdReleaseGData(TRUE);
			IoFreeWorkItem(pRequest->WorkQueueItem);
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessFlushBroadCast\n" ) );
			XixFsdDecGlobalWorkItem();
			
			return;
			
			
		}
		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	IoFreeWorkItem(pRequest->WorkQueueItem);
	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessFlushBroadCast\n" ) );
	XixFsdDecGlobalWorkItem();
	
	return ;	
}


NTSTATUS
xixfs_EventChangeFileLen(
	IN PXIXFS_VCB		pVCB,
	IN uint64		LotNumber,
	IN uint64		FileLen,
	IN uint64		AllocationLen,
	IN uint64		WriteStartOffset
)	
{
	PXIXFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;
	uint32			DataLength;
	LARGE_INTEGER	PrevFileSize;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventChangeFileLen\n" ) );	
	
	pFCB = xixfs_FCBTLBLookupEntry(pVCB,LotNumber);


	if(pFCB){
		XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
		XifsdLockFcb(TRUE,pFCB);
		PrevFileSize = pFCB->FileSize;
		pFCB->FileSize.QuadPart = FileLen;
		pFCB->ValidDataLength.QuadPart = FileLen;
		pFCB->AllocationSize.QuadPart = AllocationLen;
		//XIXCORE_SET_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		XifsdUnlockFcb(TRUE,pFCB);

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
			("PrevFileSize (%I64d) FileLen %I64d ValidDataLength %I64d AllocationSize %I64d\n",
			PrevFileSize, FileLen, FileLen, AllocationLen) );


		if((uint64)PrevFileSize.QuadPart > WriteStartOffset){
			ByteOffset.QuadPart = WriteStartOffset;
			DataLength = (uint32)(PrevFileSize.QuadPart - WriteStartOffset);

			if (pFCB->SectionObject.ImageSectionObject != NULL) {
				MmFlushImageSection( &pFCB->SectionObject, MmFlushForDelete );
			}

			if(pFCB->SectionObject.DataSectionObject != NULL) {
				//DbgPrint("Update Chagne Length Purge!!!!!");
				CcPurgeCacheSection(&(pFCB->SectionObject), &ByteOffset, DataLength, FALSE);
				//CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
				//CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);
				//CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
			}
		}

		
		XifsdReleaseFcb(TRUE, pFCB);
		
		xixfs_NotifyReportChangeToXixfs(
							pFCB,
							FILE_NOTIFY_CHANGE_SIZE,
							FILE_ACTION_MODIFIED
							);

		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventChangeFileLen\n" ) );
		return RC;
	}
	
	RC = STATUS_SUCCESS;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventChangeFileLen\n" ) );	
	return RC;	
}


VOID
xixfs_EventProcessFileLenChange(
		IN PDEVICE_OBJECT DeviceObject,					
		IN PVOID Parameter
)
{
	PXIXFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIXFS_VCB	pVCB = NULL;
	PXIXFS_FILE_LENGTH_CHANGE_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();

	
	pRequest = (PXIXFSDG_PKT)Parameter;

	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessFileLenChange\n" ) );

	pDataHeader = &(pRequest->RawDataDG.FileLenChangeReq);
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);

		// Changed by ILGU HONG
		
		if( RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 16){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = xixfs_EventChangeFileLen(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHLL(pDataHeader->FileLength), 
					NTOHLL(pDataHeader->AllocationLength),
					NTOHLL(pDataHeader->WriteStartOffset));
			
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			IoFreeWorkItem(pRequest->WorkQueueItem);
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessFileLenChange\n" ) );
			XixFsdDecGlobalWorkItem();
			
			return ;
		}

		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	IoFreeWorkItem(pRequest->WorkQueueItem);
	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessFileLenChange\n" ) );
	XixFsdDecGlobalWorkItem();
	
	return ;		
}

NTSTATUS
xixfs_EventFileChange(
	IN PXIXFS_VCB	pVCB,
	IN uint64		LotNumber,
	IN uint32		SubCommand,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentFcb
)
{
	PXIXFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventFileChange\n" ) );	
	
	//DbgPrint("CALL Enter xixfs_EventFileChange!!!!!");

	pFCB = xixfs_FCBTLBLookupEntry(pVCB,LotNumber);

	if(pFCB){
		
		//XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
		XifsdLockFcb(TRUE, pFCB);
		if(SubCommand == XIXFS_SUBTYPE_FILE_DEL){
			//DbgPrint("CALL XIXFS_SUBTYPE_FILE_DELL EVENT!!!!!");
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
		}else if(SubCommand == XIXFS_SUBTYPE_FILE_MOD){
			//DbgPrint("CALL XIXFS_SUBTYPE_FILE_MOD EVENT!!!!!");
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RELOAD);
		}else if(SubCommand == XIXFS_SUBTYPE_FILE_RENAME){
			//DbgPrint("CALL XIXFS_SUBTYPE_FILE_RENAME EVENT!!!!!");
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RENAME);	
		}else if(SubCommand == XIXFS_SUBTYPE_FILE_LINK){
			//DbgPrint("CALL XIXFS_SUBTYPE_FILE_LINK EVENT!!!!!");
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_LINK);
		}
		XifsdUnlockFcb(TRUE, pFCB);

		if(SubCommand == XIXFS_SUBTYPE_FILE_RENAME){
	
			if(OldParentLotNumber != NewParentFcb){
				PXIXFS_FCB	ParentFcb = NULL;
				PXIXFS_LCB	Lcb = NULL;
				PLIST_ENTRY	ListLinks = NULL;
				
				ParentFcb = xixfs_FCBTLBLookupEntry(pVCB, OldParentLotNumber);
				
				if(ParentFcb) {

					

					for (ListLinks = pFCB->ParentLcbQueue.Flink;
						 ListLinks != &pFCB->ParentLcbQueue;
						 ) {
						
						Lcb = CONTAINING_RECORD( ListLinks, XIXFS_LCB, ChildFcbLinks );

						ListLinks = ListLinks->Flink;

						if (Lcb->ParentFcb == ParentFcb) {
							XifsdAcquireFcbExclusive(TRUE, ParentFcb, FALSE);
							XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
							xixfs_FCBTLBRemovePrefix( TRUE, Lcb );
							XifsdLockVcb(TRUE, pVCB);
							XifsdDecRefCount( ParentFcb, 1, 1 );
							XifsdUnlockVcb(TRUE, pVCB);
							XifsdReleaseFcb(TRUE, pFCB);
							XifsdReleaseFcb(TRUE, ParentFcb);
						}
					}					
				}



			}



		}


		if((SubCommand == XIXFS_SUBTYPE_FILE_RENAME) || (SubCommand == XIXFS_SUBTYPE_FILE_LINK)){
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
			xixfs_ReLoadFileFromFcb(pFCB);
			XifsdReleaseFcb(TRUE, pFCB);

			xixfs_NotifyReportChangeToXixfs(
								pFCB,
								((pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE)
									?FILE_NOTIFY_CHANGE_FILE_NAME
									: FILE_NOTIFY_CHANGE_DIR_NAME ),
								FILE_ACTION_RENAMED_NEW_NAME
								);

		}



		if(SubCommand == XIXFS_SUBTYPE_FILE_DEL){
			PXIXFS_FCB	ParentFcb = NULL;
			PXIXFS_LCB	Lcb = NULL;
			PLIST_ENTRY	ListLinks = NULL;
			
			//DbgPrint("CALL XIXFS_SUBTYPE_FILE_DELL EVENT!!!!!");
			ParentFcb = xixfs_FCBTLBLookupEntry(pVCB, OldParentLotNumber);
			
			if(ParentFcb) {

				for (ListLinks = pFCB->ParentLcbQueue.Flink;
					 ListLinks != &pFCB->ParentLcbQueue;
					 ) {
					
					Lcb = CONTAINING_RECORD( ListLinks, XIXFS_LCB, ChildFcbLinks );

					ListLinks = ListLinks->Flink;

					if (Lcb->ParentFcb == ParentFcb) {
						XIXCORE_SET_FLAGS(Lcb->LCBFlags, XIFSD_LCB_STATE_LINK_IS_GONE);
						XifsdAcquireFcbExclusive(TRUE, ParentFcb, FALSE);
						XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
						xixfs_FCBTLBRemovePrefix( TRUE, Lcb );
						XifsdLockVcb(TRUE, pVCB);
						XifsdDecRefCount( ParentFcb, 1, 1 );
						XifsdUnlockVcb(TRUE, pVCB);

						


						XifsdReleaseFcb(TRUE, pFCB);
						XifsdReleaseFcb(TRUE, ParentFcb);
					}




				}					
			}

			XifsdLockFcb(TRUE,pFCB);
			pFCB->FileSize.QuadPart = 0;
			pFCB->ValidDataLength.QuadPart = 0;
			pFCB->AllocationSize.QuadPart = 0;
			XifsdUnlockFcb(TRUE,pFCB);





			xixfs_NotifyReportChangeToXixfs(
						pFCB,
						FILE_NOTIFY_CHANGE_CREATION,
						FILE_ACTION_REMOVED
						);

			if (pFCB->SectionObject.ImageSectionObject != NULL) {
				MmFlushImageSection( &pFCB->SectionObject, MmFlushForDelete );
			}

			if(pFCB->SectionObject.DataSectionObject != NULL) {
				//DbgPrint("Update Chagne DEL Purge!!!!!");
				CcPurgeCacheSection(&(pFCB->SectionObject), NULL, 0, FALSE);
			}

		}


		//XifsdReleaseFcb(TRUE, pFCB);
		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventFileChange\n" ) );	
		return RC;
	}
	
	RC = STATUS_SUCCESS;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventFileChange\n" ) );	
	return RC;	
}


VOID
xixfs_EventProcessFileChange(
		IN PDEVICE_OBJECT DeviceObject,				
		IN PVOID Parmeter
)
{
	PXIXFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIXFS_VCB	pVCB = NULL;
	PXIXFS_FILE_CHANGE_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();

	
	pRequest = (PXIXFSDG_PKT)Parmeter;
	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessFileChange\n" ) );	

	pDataHeader = &(pRequest->RawDataDG.FileChangeReq);
	
	
		/*
		DbgPrint("FILE CHANGE LotNum(%I64d), SubCom(%d) PrevP(%I64d), NewP(%I64d)\n",
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->SubCommand),
					NTOHLL(pDataHeader->PrevParentLotNumber),
					NTOHLL(pDataHeader->NewParentLotNumber)
					);
		*/


	XifsdAcquireGData(TRUE);
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);

		// Changed by ILGU HONG

		if(RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 16){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = xixfs_EventFileChange(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->SubCommand),
					NTOHLL(pDataHeader->PrevParentLotNumber),
					NTOHLL(pDataHeader->NewParentLotNumber)
					);
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			IoFreeWorkItem(pRequest->WorkQueueItem);
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessFileChange\n" ) );
			XixFsdDecGlobalWorkItem();	
			
			return ;
			
			
		}

		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	IoFreeWorkItem(pRequest->WorkQueueItem);
	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventProcessFileChange\n" ) );
	XixFsdDecGlobalWorkItem();
	
	return ;		
}



NTSTATUS
xixfs_EventDirChange(
	IN PXIXFS_VCB pVCB, 
	IN uint64	LotNumber, 
	IN uint32 ChildSlotNumber,
	IN uint32 SubCommand
)
{
	PXIXFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventDirChange\n" ) );		
	
	pFCB = xixfs_FCBTLBLookupEntry(pVCB,LotNumber);


	if(pFCB){
	
		XifsdAcquireFcbExclusive(TRUE,pFCB, FALSE);
	
		
		
		
		if(!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type, 
			(XIFS_FD_TYPE_DIRECTORY| XIFS_FD_TYPE_ROOT_DIRECTORY)))
		{
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "Not Dir\n" ) );
			return STATUS_SUCCESS;
		}
		

		if(SubCommand == XIXFS_SUBTYPE_DIR_ADD){
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
			}
			XIXCORE_SET_FLAGS(pChild->ChildFlag, XIFSD_CHILD_CHANGED_ADDED) ;
			setBitToMap(ChildSlotNumber, pFCB->ChildLotMap);
			*/
		}else if(SubCommand == XIXFS_SUBTYPE_DIR_DEL){
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
			
			}
			clearBitToMap(ChildSlotNumber, pFCB->ChildLotMap);
			XIXCORE_SET_FLAGS(pChild->ChildFlag, XIFSD_CHILD_CHANGED_DELETE) ;
			*/
		}else {
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
				
			}
		
			XIXCORE_SET_FLAGS(pChild->ChildFlag, XIFSD_CHILD_CHANGED_UPDATE) ;
			*/
			
		}
		
		XifsdReleaseFcb(TRUE,pFCB);
		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventDirChange\n" ) );
		return RC;	
	}
	
	RC = STATUS_SUCCESS;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventDirChange\n" ) );	
	return RC;	
}

VOID
xixfs_EventProcessDirChange(
		IN PDEVICE_OBJECT DeviceObject,			
		IN PVOID Parameter
)
{
	PXIXFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIXFS_VCB	pVCB = NULL;
	PXIXFS_DIR_ENTRY_CHANGE_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();
	
	
	pRequest = (PXIXFSDG_PKT)Parameter;
	ASSERT(pRequest);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessDirChange\n" ) );	

	pDataHeader = &(pRequest->RawDataDG.DirChangeReq);
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIXFS_VCB, VCBLink);

		// Changed by ILGU HONG
		if(RtlCompareMemory(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 16){
			
			

			RC = xixfs_EventDirChange(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->ChildSlotNumber),
					NTOHL(pDataHeader->SubCommand)
					);
			
			XifsdReleaseGData(TRUE);
			IoFreeWorkItem(pRequest->WorkQueueItem);
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessDirChange\n" ) );
			XixFsdDecGlobalWorkItem();
			
			return ;
		}

		pListEntry = pListEntry->Flink;

	}
	XifsdReleaseGData(TRUE);
	IoFreeWorkItem(pRequest->WorkQueueItem);
	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventProcessDirChange\n" ) );
	XixFsdDecGlobalWorkItem();
	
	return ;		
}




BOOLEAN
xixfs_EventDispatchDGReqPkt(
				IN PXIFS_COMM_CTX		SvrCtx,
				IN OUT PXIXFSDG_PKT		pRequest
)
{
	NTSTATUS							RC = STATUS_SUCCESS;
	PXIXFS_COMM_HEADER					pHeader = NULL;
	PXIXFSDG_RAWPK_DATA					pData = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter xixfs_EventDispatchDGReqPkt\n" ) );

	pHeader = &pRequest->RawHeadDG;
	pData = &pRequest->RawDataDG;
	
	switch(NTOHL(pHeader->Type)){
	case XIXFS_TYPE_LOCK_REQUEST:
	{
		// Changed by ILGU HONG
		//	chesung suggest
		/*
		LPX_ADDRESS CheckAddress;
		RtlZeroMemory(&CheckAddress, sizeof(LPX_ADDRESS));

		CheckAddress.Port = 0;
		RtlCopyMemory(CheckAddress.Node, pData->LockReq.LotOwnerMac, 6);
		if(xixfs_IsFromLocal(&CheckAddress)){

			ExInitializeWorkItem( 
							&(pRequest->WorkQueueItem),
							xixfs_EventProcessLockRequest,
							(VOID *)pRequest 
							);

			ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );			
			return FALSE;

		}
		return TRUE;
		*/
		
		if(RtlCompareMemory(SvrCtx->HostMac, pData->LockReq.LotOwnerMac, 32) == 32){
			pRequest->WorkQueueItem = IoAllocateWorkItem(XiGlobalData.XifsControlDeviceObject);
			if(!pRequest->WorkQueueItem){
				return TRUE;
			}

			XixFsdIncGlobalWorkItem();		
			IoQueueWorkItem(pRequest->WorkQueueItem,
								xixfs_EventProcessLockRequest,
								DelayedWorkQueue,
								(VOID *)pRequest 
								);
			/*
			ExInitializeWorkItem( 
							&(pRequest->WorkQueueItem),
							xixfs_EventProcessLockRequest,
							(VOID *)pRequest 
							);
			ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
			*/
			
					
			return FALSE;
		}
		return TRUE;


	}
	break;
	case XIXFS_TYPE_LOCK_REPLY:
	{
		// Changed by ILGU HONG
		//	chesung suggest
		/*
		LPX_ADDRESS CheckAddress;
		RtlZeroMemory(&CheckAddress, sizeof(LPX_ADDRESS));

		CheckAddress.Port = 0;
		RtlCopyMemory(CheckAddress.Node, pHeader->DstMac, 6);
		if(xixfs_IsFromLocal(&CheckAddress)){
			RC= xixfs_EventCheckLockRequest(SvrCtx, pRequest);
			if(RC == STATUS_PENDING) return FALSE;
			return TRUE;
		}
		return TRUE;
		*/

		if(RtlCompareMemory(SvrCtx->HostMac, pHeader->DstMac, 32) == 32){
			RC= xixfs_EventCheckLockRequest(SvrCtx, pRequest);
			if(RC == STATUS_PENDING) return FALSE;
			return TRUE;
		}
		return TRUE;

	}
	break;
	case XIXFS_TYPE_LOCK_BROADCAST:
	{

		pRequest->WorkQueueItem = IoAllocateWorkItem(XiGlobalData.XifsControlDeviceObject);
		if(!pRequest->WorkQueueItem){
			return TRUE;
		}

		XixFsdIncGlobalWorkItem();		
		IoQueueWorkItem(pRequest->WorkQueueItem,
							xixfs_EventProcessLockBroadCast,
							DelayedWorkQueue,
							(VOID *)pRequest 
							);

		/*
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						xixfs_EventProcessLockBroadCast,
						(VOID *)pRequest 
						);
	
		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
		*/
		return FALSE;
	}
	break;
	case XIXFS_TYPE_FLUSH_BROADCAST:
	{
		pRequest->WorkQueueItem = IoAllocateWorkItem(XiGlobalData.XifsControlDeviceObject);
		if(!pRequest->WorkQueueItem){
			return TRUE;
		}

		XixFsdIncGlobalWorkItem();	
		IoQueueWorkItem(pRequest->WorkQueueItem,
							xixfs_EventProcessFlushBroadCast,
							DelayedWorkQueue,
							(VOID *)pRequest 
							);


		/*
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						xixfs_EventProcessFlushBroadCast,
						(VOID *)pRequest 
						);

		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
		*/
		return FALSE;
	}
	break;
	case XIXFS_TYPE_FILE_LENGTH_CHANGE:
	{
		pRequest->WorkQueueItem = IoAllocateWorkItem(XiGlobalData.XifsControlDeviceObject);
		if(!pRequest->WorkQueueItem){
			return TRUE;
		}

		XixFsdIncGlobalWorkItem();		
		IoQueueWorkItem(pRequest->WorkQueueItem,
							xixfs_EventProcessFileLenChange,
							DelayedWorkQueue,
							(VOID *)pRequest 
							);

		/*
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						xixfs_EventProcessFileLenChange,
						(VOID *)pRequest 
						);
		
		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
		*/
		return FALSE;			
	}
	break;
	case XIXFS_TYPE_FILE_CHANGE:
	{
		pRequest->WorkQueueItem = IoAllocateWorkItem(XiGlobalData.XifsControlDeviceObject);
		if(!pRequest->WorkQueueItem){
			return TRUE;
		}

		XixFsdIncGlobalWorkItem();	
		IoQueueWorkItem(pRequest->WorkQueueItem,
							xixfs_EventProcessFileChange,
							DelayedWorkQueue,
							(VOID *)pRequest 
							);
		/*
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						xixfs_EventProcessFileChange,
						(VOID *)pRequest 
						);
		
		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
		*/
		return FALSE;			
	}
	break;
	case XIXFS_TYPE_DIR_CHANGE:
	{
		pRequest->WorkQueueItem = IoAllocateWorkItem(XiGlobalData.XifsControlDeviceObject);
		if(!pRequest->WorkQueueItem){
			return TRUE;
		}

		XixFsdIncGlobalWorkItem();		
		IoQueueWorkItem(pRequest->WorkQueueItem,
							xixfs_EventProcessDirChange,
							DelayedWorkQueue,
							(VOID *)pRequest 
							);
		/*
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						xixfs_EventProcessDirChange,
						(VOID *)pRequest 
						);
		
		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
		*/
		return FALSE;							
	}
	break;
	default:
		return TRUE;
		break;
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit xixfs_EventDispatchDGReqPkt\n" ) );
}

VOID
xixfs_EventComSvrThreadProc(
		PVOID	lpParameter
)
{
	PXIFS_COMM_CTX		SvrCtx = (PXIFS_COMM_CTX)lpParameter;
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	LFSDG_Socket		ServerDatagramSocket;
	PLIST_ENTRY			listEntry = NULL;	
	PKEVENT				Evts[3];
	PXIXFSDG_PKT			pPacket = NULL;
	BOOLEAN				bret = FALSE;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter XifsComSvrThreadProc\n" ) );

	// Open server datagram port
	RC = xixfs_OpenDGSocket(
		&ServerDatagramSocket,
		DEFAULT_XIXFS_SVRPORT
	);

	if(NT_SUCCESS(RC)) {

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Open Complete  and call Register Receive Handler\n" ) );
		// register Datagram handler
		RC = xixfs_RegisterDatagramRecvHandler(
			&ServerDatagramSocket,
			xixfs_EventSvrDatagramRecvHandler,
			SvrCtx
		);

	}else{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "ERROR XifsComSvrThreadProc:xixfs_OpenDGSocket Fail \n" ) );
		goto error_out;
	}

	Evts[0] = &SvrCtx->ServShutdownEvent;
	Evts[1] = &SvrCtx->ServDatagramRecvEvent;
	Evts[2] = &SvrCtx->ServNetworkEvent;


	// Main loop...
	while(1) {
		RC = KeWaitForMultipleObjects(
				3,
				Evts,
				WaitAny,
				Executive,
				KernelMode,
				TRUE,
				NULL,
				NULL
			);
		
		if(0 == RC) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, ( "[COM] XifsComSvrThreadProc Shutdown Event \n" ) );
			// Shutdown event received
			break;
		} else if(1 == RC) {
			//
			//	Datagram received.
			//
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, ( "[COM] XifsComSvrThreadProc Datagram receive Event \n" ) );
			KeClearEvent(&SvrCtx->ServDatagramRecvEvent);
		} else if(2 == RC) {
			//
			// Renew server datagram port
			//
			uint32		RetryCount = 0;

			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, ( "[COM] XifsComSvrThreadProc Renew Network receive Event \n" ) );

			KeClearEvent(&SvrCtx->ServNetworkEvent);
retry2:
			xixfs_CloseDGSocket(
				&ServerDatagramSocket
			);
retry:
			RetryCount++;
			RC  = xixfs_OpenDGSocket(
							&ServerDatagramSocket,
							DEFAULT_XIXFS_SVRPORT
						);
			
			if(NT_SUCCESS(RC )) {
				// register Datagram handler
				RC  = xixfs_RegisterDatagramRecvHandler(
								&ServerDatagramSocket,
								xixfs_EventSvrDatagramRecvHandler,
								SvrCtx
								);

				if(!NT_SUCCESS(RC)){
					if(RetryCount >3){
						goto error_out2;
					}
					goto retry2;
				}
				
			}else{
				if(RetryCount >3){
					goto error_out2;
				}
				goto retry;
			}

			continue;
		} else if(STATUS_ALERTED == RC){
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				( "ERROR XifsComSvrThreadProc: KeWaitForMultipleObjects() failed. NTSTATUS:%lu\n", RC));
			break;
		}

		//
		//	get packets to the dispatch routine.
		//
		while(1) 
		{
			
			ExAcquireFastMutexUnsafe(&SvrCtx->RecvPktListMutex);
			listEntry = RemoveHeadList(&SvrCtx->RecvPktList);
			ExReleaseFastMutexUnsafe(&SvrCtx->RecvPktListMutex);
			

			if(listEntry == &SvrCtx->RecvPktList) {
				DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
					( "[LFS] XifsComSvrThreadProc: Datagram queue empty. back to waiting mode.\n"));
				break;
			}
			pPacket = CONTAINING_RECORD(listEntry, XIXFSDG_PKT, PktListEntry);
			if(xixfs_IsFromLocal(&pPacket->SourceAddr)) {
				DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
					( "[LFS] XifsComSvrThreadProc: Datagram packet came from the local address "
						"%02X:%02X:%02X:%02X:%02X:%02X/%d\n", 
					pPacket->SourceAddr.Node[0], pPacket->SourceAddr.Node[1], pPacket->SourceAddr.Node[2],
					pPacket->SourceAddr.Node[3], pPacket->SourceAddr.Node[4], pPacket->SourceAddr.Node[5],
					pPacket->SourceAddr.Port
					));
				xixfs_DereferenceDGPkt(pPacket);
				
	
				continue;
			}

			DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
				( "[LFS] XifsComSvrThreadProc: Datagram packet came from the other address "
					"%02X:%02X:%02X:%02X:%02X:%02X/%d\n", 
				pPacket->SourceAddr.Node[0], pPacket->SourceAddr.Node[1], pPacket->SourceAddr.Node[2],
				pPacket->SourceAddr.Node[3], pPacket->SourceAddr.Node[4], pPacket->SourceAddr.Node[5],
				pPacket->SourceAddr.Port
				));

			bret = xixfs_EventDispatchDGReqPkt(
				SvrCtx,
				pPacket
			);

			if(TRUE == bret) {
				xixfs_DereferenceDGPkt(pPacket);
			}
		}


	}

	if(XiGlobalData.QueuedWork != 0){
		LARGE_INTEGER	TimeOut;

		TimeOut.QuadPart = - ( 5 * TIMERUNIT_SEC) ;
		RC = KeWaitForSingleObject(
			&XiGlobalData.QueuedWorkcleareEvent,
				Executive,
				KernelMode,
				FALSE,
				&TimeOut
			);
	}

	xixfs_CloseDGSocket(
		&ServerDatagramSocket
	);	


	ExAcquireFastMutexUnsafe(&SvrCtx->RecvPktListMutex);

	while(1) 
	{
		listEntry = RemoveHeadList(&SvrCtx->RecvPktList);
		if(listEntry == &SvrCtx->RecvPktList) {
			DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
				( "[LFS] XifsComSvrThreadProc: Datagram queue empty. back to waiting mode.\n"));
			break;
		}
		pPacket = CONTAINING_RECORD(listEntry, XIXFSDG_PKT, PktListEntry);

		xixfs_DereferenceDGPkt(pPacket);
		continue;
	}
	ExReleaseFastMutexUnsafe(&SvrCtx->RecvPktListMutex);
			




error_out2:
	// to do something
error_out:	
	KeSetEvent(&(SvrCtx->ServStopEvent), 0, FALSE);

	PsTerminateSystemThread(RC);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit XifsComSvrThreadProc\n" ) );
	return;
	
}



VOID
xixfs_EventComCliThreadProc(
		PVOID	lpParameter
)
{
	PXIFS_COMM_CTX	 CliCtx = (PXIFS_COMM_CTX)lpParameter;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LFSDG_Socket		ClieSocketAddress;
	LPX_ADDRESS			BroadcastAddr;
	PKEVENT				Evts[3];
	LARGE_INTEGER		TimeOut;
	PLIST_ENTRY 		pListEntry = NULL;
	PLIST_ENTRY 		pList = NULL;
	PXIXFSDG_PKT			pPacket = NULL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Enter XifsComCliThreadProc\n" ) );	

	// Open server datagram port
	RC  = xixfs_OpenDGSocket(
		&ClieSocketAddress,
		0
	);

	if(!NT_SUCCESS(RC)){
		goto error_out;
	}

	Evts[0] = &CliCtx ->CliShutdownEvent;
	Evts[1] = &CliCtx ->CliNetworkEvent;
	Evts[2] = &CliCtx->CliSendDataEvent;

	while(1){
		TimeOut.QuadPart = - DEFAULT_XIXFS_CLIWAIT;
		RC = KeWaitForMultipleObjects(
				3,
				Evts,
				WaitAny,
				Executive,
				KernelMode,
				TRUE,
				&TimeOut,
				NULL
			);
		
		if(0 == RC) {
			// Shutdown event received
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "[COM] XifsComCliThreadProc Shutdown Event!\n" ) );	
			break;
		} else if(1 == RC) {
			//
			// Renew server datagram port
			//
			
			uint32		RetryCount = 0;

			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "[COM] XifsComCliThreadProc Network Event!\n" ) );

			KeClearEvent(&CliCtx->CliNetworkEvent);


			xixfs_CloseDGSocket(
				&ClieSocketAddress
			);
			
			RetryCount++;
			RC  = xixfs_OpenDGSocket(
							&ClieSocketAddress,
							0
						);
			if(!NT_SUCCESS(RC)){
				if(RetryCount >3){
					goto error_out2;
				}
			}
			
			continue;
		} else if( (2 == RC )||(STATUS_TIMEOUT == RC) ) {
			//
			//	Get Request and process them
			//
			BOOLEAN		IsRemove = FALSE;
			LIST_ENTRY	TempList;
			
			LPX_ADDRESS		DestBroadcastAddr;

			InitializeListHead(&TempList);
			
			KeClearEvent(&CliCtx->CliSendDataEvent);

			RtlZeroMemory(&DestBroadcastAddr, sizeof(LPX_ADDRESS));
			DestBroadcastAddr.Port = HTONS(DEFAULT_XIXFS_SVRPORT);
			DestBroadcastAddr.Node[0] = 0xFF;
			DestBroadcastAddr.Node[1] = 0xFF;
			DestBroadcastAddr.Node[2] = 0xFF;
			DestBroadcastAddr.Node[3] = 0xFF;
			DestBroadcastAddr.Node[4] = 0xFF;
			DestBroadcastAddr.Node[5] = 0xFF;
			
			if(RC == 2){
				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "[COM] XifsComCliThreadProc SendData Event!\n" ) );
			}

			while(1){
				ExAcquireFastMutexUnsafe(&CliCtx->SendPktListMutex);
				pList = RemoveHeadList(&CliCtx->SendPktList);
				ExReleaseFastMutexUnsafe(&CliCtx->SendPktListMutex);

				if(pList == &CliCtx->SendPktList) break;
				
				pPacket =  (PXIXFSDG_PKT)CONTAINING_RECORD(pList, XIXFSDG_PKT, PktListEntry);

				if(NTOHL(pPacket->RawHeadDG.Type) == XIXFS_TYPE_LOCK_REQUEST){
					if(pPacket->TimeOut.QuadPart < (int64)xixcore_GetCurrentTime64()){
						
						DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_HOSTCOM, 
							( "[XifsComCliThreadProc] LOCK TIMEOUT Req(%I64d) Check(%64d)\n", pPacket->TimeOut.QuadPart, xixcore_GetCurrentTime64()));	
						
						InitializeListHead(&pPacket->PktListEntry);
						pPacket->pLockContext->Status = LOCK_TIMEOUT;
						KeSetEvent(&(pPacket->pLockContext->KEvent), 0, FALSE);
						xixfs_DereferenceDGPkt(pPacket);
						continue;
					}
					IsRemove = FALSE;
				}else{
					IsRemove = TRUE;
				}


				DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_HOSTCOM, 
					( "[XifsComCliThreadProc] SendReq (%p)\n ", pPacket));	
				
				RC= xixfs_SendDatagram(
						&ClieSocketAddress,
						pPacket,
						&DestBroadcastAddr
					);					

				if(TRUE ==IsRemove){
					xixfs_DereferenceDGPkt(pPacket);
				}else{
					
					InsertTailList(&(TempList),&(pPacket->PktListEntry) );					
					
				}


	

			}

			pList = RemoveHeadList(&TempList);
			while(pList != &TempList){
				ExAcquireFastMutexUnsafe(&CliCtx->SendPktListMutex);
				InsertTailList(&(CliCtx->SendPktList),&(pPacket->PktListEntry));		
				ExReleaseFastMutexUnsafe(&CliCtx->SendPktListMutex);
				pList = RemoveHeadList(&TempList);
			}			


		}else if(STATUS_ALERTED == RC){
			break;
		}

	}

error_out2:	
    xixfs_CloseDGSocket(
		&ClieSocketAddress
	);	

	ExAcquireFastMutexUnsafe(&CliCtx->SendPktListMutex);
	pList = CliCtx->SendPktList.Flink;
	while(pList != &CliCtx->SendPktList){
		pPacket =  (PXIXFSDG_PKT)CONTAINING_RECORD(pList, XIXFSDG_PKT, PktListEntry);
		pList = pList->Flink;
		RemoveEntryList(&pPacket->PktListEntry);
		InitializeListHead(&pPacket->PktListEntry);
		if(NTOHL(pPacket->RawHeadDG.Type) == XIXFS_TYPE_LOCK_REQUEST){
			pPacket->pLockContext->Status = LOCK_TIMEOUT;
			KeSetEvent(&(pPacket->pLockContext->KEvent), 0, FALSE);
		}
		
		xixfs_DereferenceDGPkt(pPacket);
	}
	ExReleaseFastMutexUnsafe(&CliCtx->SendPktListMutex);
	
	

	KeSetEvent(&(CliCtx ->CliStopEvent), 0, FALSE);

	// to do something
error_out:	
	PsTerminateSystemThread(RC);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit XifsComCliThreadProc\n" ) );	
	return;
}
