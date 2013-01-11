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






NTSTATUS
IsFileLocked(
	IN PXIFS_VCB		pVCB,
	IN uint64			LotNumber,
	IN OUT uint32		*LockState
	);

NTSTATUS
SendLockReply(
	IN PXIFS_VCB 		pVCB,
	IN PXIFSDG_PKT		pRequest,
	IN uint32			LockState
);

VOID
ProcessLockRequest(
	IN PVOID	Parameter
);


NTSTATUS
CheckLockRequest(
		IN PXIFS_COMM_CTX	SvrCtx,
		IN PXIFSDG_PKT		pRequest
);


NTSTATUS
SetFileLock(
	IN PXIFS_VCB		pVCB,
	IN uint64			LotNumber
	);


VOID
ProcessLockBroadCast(
	IN PVOID	Parameter
);

NTSTATUS
FileFlush(
	IN PXIFS_VCB 	pVCB, 
	IN uint64		LotNumber,
	IN uint64		StartOffset, 
	IN uint32		DataSize
	);

VOID
ProcessFlushBroadCast(
	IN PVOID	Parameter
);

NTSTATUS
ChangeFileLen(
	IN PXIFS_VCB		pVCB,
	IN uint64		LotNumber,
	IN uint64		FileLen,
	IN uint64		AllocationLen,
	IN uint64		WriteStartOffset
);	


VOID
ProcessFileLenChange(
	IN PVOID	Parameter
);


NTSTATUS
FileChange(
	IN PXIFS_VCB	pVCB,
	IN uint64		LotNumber,
	IN uint32		SubCommand,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentFcb
);

VOID
ProcessFileChange(
	IN PVOID	Parameter
);


NTSTATUS
DirChange(
	IN PXIFS_VCB pVCB, 
	IN uint64	LotNumber, 
	IN uint32 ChildSlotNumber,
	IN uint32 SubCommand
);


VOID
ProcessDirChange(
	IN PVOID	Parameter
);


BOOLEAN
DispatchDGReqPkt(
				IN PXIFS_COMM_CTX		SvrCtx,
				IN OUT PXIFSDG_PKT		pRequest
);

//#pragma alloc_text(PAGE, XixFsdSevEventCallBack)
//#pragma alloc_text(PAGE, XixFsdCliEventCallBack)
//#pragma alloc_text(PAGE, XixFsdSvrDatagramRecvHandler)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IsFileLocked)
#pragma alloc_text(PAGE, SendLockReply)
#pragma alloc_text(PAGE, ProcessLockRequest)
#pragma alloc_text(PAGE, CheckLockRequest)
#pragma alloc_text(PAGE, SetFileLock)
#pragma alloc_text(PAGE, ProcessLockBroadCast)
#pragma alloc_text(PAGE, FileFlush)
#pragma alloc_text(PAGE, ProcessFlushBroadCast)
#pragma alloc_text(PAGE, ChangeFileLen)
#pragma alloc_text(PAGE, ProcessFileLenChange)
#pragma alloc_text(PAGE, FileChange)
#pragma alloc_text(PAGE, ProcessFileChange)
#pragma alloc_text(PAGE, DirChange)
#pragma alloc_text(PAGE, ProcessDirChange)
#pragma alloc_text(PAGE, DispatchDGReqPkt)
#pragma alloc_text(PAGE, XixFsdComSvrThreadProc)
#pragma alloc_text(PAGE, XixFsdComCliThreadProc)
#endif



VOID
XixFsdSevEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) {
	PXIFS_COMM_CTX	ServCtx = NULL;
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter XixFsdSevEventCallBack\n" ) );
	ServCtx = (PXIFS_COMM_CTX)Context;

	KeSetEvent(&ServCtx->ServNetworkEvent, 0, FALSE);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit XixFsdSevEventCallBack\n" ) );
	return;
}


VOID
XixFsdCliEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) {
	PXIFS_COMM_CTX	CliCtx = (PXIFS_COMM_CTX)Context;
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter XixFsdCliEventCallBack\n" ) );

	KeSetEvent(&CliCtx->CliNetworkEvent, 0, FALSE);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit XixFsdCliEventCallBack\n" ) );

	return;
}


NTSTATUS
XixFsdSvrDatagramRecvHandler(
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
	PXIFSDG_PKT				Pkt;
	BOOLEAN					bRet;
	static UCHAR			Protocol[4] = XIFS_DATAGRAM_PROTOCOL;
	
	//PAGED_CODE();

	UNREFERENCED_PARAMETER(SourceAddressLength);
	UNREFERENCED_PARAMETER(OptionsLength);
	UNREFERENCED_PARAMETER(Options);
	UNREFERENCED_PARAMETER(ReceiveDatagramFlags);
	UNREFERENCED_PARAMETER(BytesTaken);
	UNREFERENCED_PARAMETER(IoRequestPacket);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter XifsSvrDatagramRecvHandler\n" ) );



	if(	BytesAvailable < sizeof(XIFS_COMM_HEADER) ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsSvrDatagramRecvHandler: too small bytes.\n" ) );
		DbgPrint("DROP PACKET size is too small !!!\n");
		goto not_accepted;
	}

	bRet = LfsAllocDGPkt(
				&Pkt,
				NULL,
				NULL,
				XIFS_TYPE_UNKOWN
			);
	
	if(FALSE == bRet) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsSvrDatagramRecvHandler: LfsAllocDGPkt() failed.\n"));
		DbgPrint("DROP PACKET can't allocate pkt !!!\n");
		goto not_accepted;
	}

	RtlCopyMemory(&Pkt->RawHeadDG, Tsdu, sizeof(Pkt->RawHeadDG) );

	if(	RtlCompareMemory(Pkt->RawHeadDG.Protocol, Protocol, 4) != 4 ||
		NTOHL(Pkt->RawHeadDG.XifsMajorVersion) != XIFS_PROTO_MAJOR_VERSION ||
		NTOHL(Pkt->RawHeadDG.XifsMinorVersion) != XIFS_PROTO_MINOR_VERSION ||
		!(NTOHL(Pkt->RawHeadDG.Type) & XIFS_TYPE_MASK)  ) {
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
		LfsDereferenceDGPkt(Pkt);
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
	Pkt->DataSize	=  NTOHL(Pkt->RawHeadDG.MessageSize) - sizeof(XIFS_COMM_HEADER);		

	if( Pkt->PacketSize != sizeof(XIFS_COMM_HEADER) + Pkt->DataSize ) {

		LfsDereferenceDGPkt(Pkt);
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
IsFileLocked(
	IN PXIFS_VCB		pVCB,
	IN uint64			LotNumber,
	IN OUT uint32		*LockState
	)
{
	PXIFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter IsFileLocked\n" ) );
	
	
	if(LotNumber == pVCB->HostRegLotMapIndex){
		if(pVCB->HostRegLotMapLockStatus == FCB_FILE_LOCK_HAS){
			*LockState = FCB_FILE_LOCK_HAS;
		}else{
			*LockState = FCB_FILE_LOCK_INVALID;
		}
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), 
			( "LockState (%x)\n", *LockState ) );

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), 
			( "VCB lock Request !!! Exit IsFileLocked\n"));

		return STATUS_SUCCESS;
	}


	pFCB = XixFsdLookupFCBTable(pVCB,LotNumber);
	
	if(pFCB){
		
		if(pFCB->HasLock == FCB_FILE_LOCK_HAS){
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

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit IsFileLocked Status (0x%x)\n", RC ) );
	return RC;	
}


NTSTATUS
SendLockReplyFromAuxLotLockInfo(
	IN PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo,
	IN PXIFSDG_PKT		pRequest,
	IN uint32			LockState
)
{
	PXIFS_COMM_HEADER	pHeader = NULL;
	PXIFS_LOCK_REPLY		pPacketData = NULL;
	uint64				LotNumber = 0;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter SendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);
	// Changed by ILGU HONG
	//	chesung suggest
	//RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 6);
	//RtlCopyMemory(pHeader->SrcMac, AuxLotInfo->HostMac, 6);
	
	RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 32);
	RtlCopyMemory(pHeader->SrcMac, AuxLotInfo->HostMac, 32);

	pHeader->Type = HTONL(XIFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == FCB_FILE_LOCK_HAS){
		pPacketData->LotState = HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = HTONL(LOCK_INVALID);
	}

	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pRequest->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit SendLockReply\n" ) );
	return STATUS_SUCCESS;
}


NTSTATUS
SendLockReplyRaw(
	IN PXIFSDG_PKT		pRequest,
	IN uint8			*SrcMac,
	IN uint32			LockState
)
{
	PXIFS_COMM_HEADER	pHeader = NULL;
	PXIFS_LOCK_REPLY	pPacketData = NULL;
	uint64				LotNumber = 0;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter SendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);	
	RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 32);
	RtlCopyMemory(pHeader->SrcMac, SrcMac, 32);

	pHeader->Type = HTONL(XIFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == FCB_FILE_LOCK_HAS){
		pPacketData->LotState = HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = HTONL(LOCK_INVALID);
	}

	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pRequest->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit SendLockReply\n" ) );
	return STATUS_SUCCESS;
}



NTSTATUS
SendLockReply(
	IN PXIFS_VCB 		pVCB,
	IN PXIFSDG_PKT		pRequest,
	IN uint32			LockState
)
{
	PXIFS_COMM_HEADER	pHeader = NULL;
	PXIFS_LOCK_REPLY		pPacketData = NULL;
	uint64				LotNumber = 0;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter SendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);
	// Changed by ILGU HONG
	//	chesung suggest
	//RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 6);
	//RtlCopyMemory(pHeader->SrcMac, pVCB->HostMac, 6);

	RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 32);
	RtlCopyMemory(pHeader->SrcMac, pVCB->HostMac, 32);

	pHeader->Type = HTONL(XIFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == FCB_FILE_LOCK_HAS){
		pPacketData->LotState = HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = HTONL(LOCK_INVALID);
	}

	ExAcquireFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);
	InsertTailList(&(XiGlobalData.XifsComCtx.SendPktList),
								&(pRequest->PktListEntry) );
	ExReleaseFastMutex(&XiGlobalData.XifsComCtx.SendPktListMutex);

	KeSetEvent(&(XiGlobalData.XifsComCtx.CliSendDataEvent),0, FALSE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit SendLockReply\n" ) );
	return STATUS_SUCCESS;
}

VOID
ProcessLockRequest(
		IN PVOID	Parameter
)
{
	PXIFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIFS_VCB	pVCB = NULL;
	PXIFS_LOCK_REQUEST	pDataHeader = NULL;
	uint32				LockState = FCB_FILE_LOCK_INVALID;
	

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessLockRequest\n" ) );

	pRequest = (PXIFSDG_PKT)Parameter;
	ASSERT(pRequest);


	pDataHeader = &(pRequest->RawDataDG.LockReq);
	
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);
		// Changed by ILGU HONG
		/*
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 6) == 6)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId) )){
			
					
			RC = IsFileLocked(pVCB, NTOHLL(pDataHeader->LotNumber), &LockState);
			
			if(NT_SUCCESS(RC)){
				// this pk disappered by client routine
				SendLockReply(pVCB, pRequest, LockState);
				XifsdReleaseGData(TRUE);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockRequest\n" ) );
				return;
			}else{
				XifsdReleaseGData(TRUE);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockRequest\n" ) );
				LfsDereferenceDGPkt(pRequest);
				return ;
			}
			
		}
		*/

		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 16) == 16)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId) )){
			
					
			RC = IsFileLocked(pVCB, NTOHLL(pDataHeader->LotNumber), &LockState);
			
			if(NT_SUCCESS(RC)){
				// this pk disappered by client routine
				SendLockReply(pVCB, pRequest, LockState);
				DbgPrint("!!! Send Lock Reply 1!!\n");
				XifsdReleaseGData(TRUE);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockRequest\n" ) );
				return;
			}else{
				XifsdReleaseGData(TRUE);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockRequest\n" ) );
				LfsDereferenceDGPkt(pRequest);
				return ;
			}
			
		}



		pVCB = NULL;
		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);

	if(pVCB == NULL){
	
		PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
		PLIST_ENTRY					pAuxLotLockList = NULL;
		BOOLEAN						bNewEntry = FALSE;

		pAuxLotLockList = XiGlobalData.XifsAuxLotLockList.Flink;
		// Check Aux List
		ExAcquireFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));


		// changed by ILGU HONG
		/*
		while(pAuxLotLockList != &(XiGlobalData.XifsAuxLotLockList))
		{
			AuxLotInfo = CONTAINING_RECORD(pAuxLotLockList, XIFS_AUXI_LOT_LOCK_INFO, AuxLink);
			if((RtlCompareMemory(pDataHeader->DiskId, AuxLotInfo->DiskId, 6) == 6) && 
				(NTOHL(pDataHeader->PartionId) == AuxLotInfo->PartitionId) &&
				(NTOHLL(pDataHeader->LotNumber) == AuxLotInfo->LotNumber) )
			{
				
				XixFsRefAuxLotLock(AuxLotInfo);
				ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));

				LockState = AuxLotInfo->HasLock;
				SendLockReplyFromAuxLotLockInfo(AuxLotInfo, pRequest, LockState);
				XixFsDeRefAuxLotLock(AuxLotInfo);
				return;
			}

			AuxLotInfo = NULL;
			pAuxLotLockList = pAuxLotLockList->Flink;
		}
		*/
		
		while(pAuxLotLockList != &(XiGlobalData.XifsAuxLotLockList))
		{
			AuxLotInfo = CONTAINING_RECORD(pAuxLotLockList, XIFS_AUXI_LOT_LOCK_INFO, AuxLink);
			if((RtlCompareMemory(pDataHeader->DiskId, AuxLotInfo->DiskId, 16) == 16) && 
				(NTOHL(pDataHeader->PartionId) == AuxLotInfo->PartitionId) &&
				(NTOHLL(pDataHeader->LotNumber) == AuxLotInfo->LotNumber) )
			{
				
				XixFsRefAuxLotLock(AuxLotInfo);
				ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));

				LockState = AuxLotInfo->HasLock;
				SendLockReplyFromAuxLotLockInfo(AuxLotInfo, pRequest, LockState);
				DbgPrint("!!! Send Lock Reply 2!!\n");
				XixFsDeRefAuxLotLock(AuxLotInfo);
				return;
			}

			AuxLotInfo = NULL;
			pAuxLotLockList = pAuxLotLockList->Flink;
		}

		ExReleaseFastMutex(&(XiGlobalData.XifsAuxLotLockListMutex));		
		SendLockReplyRaw(pRequest, XiGlobalData.HostMac, FCB_FILE_LOCK_INVALID);
		DbgPrint("!!! Send Lock Reply 3!!\n");
		return;

	}

	LfsDereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockRequest\n" ) );
	return;	
}


NTSTATUS
CheckLockRequest(
		IN PXIFS_COMM_CTX	SvrCtx,
		IN PXIFSDG_PKT		pRequest
)
{
	PLIST_ENTRY	pListEntry = NULL;
	KIRQL		OldIrql;
	PXIFSDG_PKT	pPkt = NULL;
	PXIFS_LOCK_REQUEST 	pReqDataHeader = NULL;
	PXIFS_LOCK_REPLY		pReplyDataHeader = NULL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter CheckLockRequest\n" ) );

	pReplyDataHeader = &pRequest->RawDataDG.LockReply;
	
	
	ExAcquireFastMutex(&SvrCtx->SendPktListMutex);
	pListEntry = SvrCtx->SendPktList.Flink;
	while(pListEntry != &(SvrCtx->SendPktList)){
		pPkt = (PXIFSDG_PKT)CONTAINING_RECORD(pListEntry, XIFSDG_PKT, PktListEntry);
		if( XIFS_TYPE_LOCK_REQUEST == NTOHL(pPkt->RawHeadDG.Type) ){
			pReqDataHeader = &pPkt->RawDataDG.LockReq;
		
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				("Req #Packet(%ld) PartId(%ld) LotNumber(%I64d) : Rep #Packet(%ld) PartId(%ld) LotNumber(%I64d)\n",
					NTOHL(pReqDataHeader->PacketNumber), NTOHL(pReqDataHeader->PartionId), NTOHLL(pReqDataHeader->LotNumber),
					NTOHL(pReplyDataHeader->PacketNumber),NTOHL(pReplyDataHeader->PartionId),NTOHLL(pReplyDataHeader->LotNumber) )); 


			// Changed by ILGU HONG
			/*
			if((NTOHL(pReqDataHeader->PacketNumber) == NTOHL(pReplyDataHeader->PacketNumber))
				&& (RtlCompareMemory(pReqDataHeader->DiskId, pReplyDataHeader->DiskId, 6) == 6)
				&& (NTOHL(pReqDataHeader->PartionId) == NTOHL(pReplyDataHeader->PartionId))
				&& ( NTOHLL(pReqDataHeader->LotNumber) == NTOHLL(pReplyDataHeader->LotNumber)) )
			{	

				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
					("Search Req and Set Event\n"));

				RemoveEntryList(&pPkt->PktListEntry);
				InitializeListHead(&pPkt->PktListEntry);
				pPkt->pLockContext->Status = pReplyDataHeader->LotState;
				KeSetEvent(&(pPkt->pLockContext->KEvent), 0, FALSE);
				LfsDereferenceDGPkt(pPkt);
				ExReleaseFastMutex(&SvrCtx->SendPktListMutex);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit CheckLockRequest Event SET!!\n" ) );
				return STATUS_SUCCESS;
			}
			*/
			if((NTOHL(pReqDataHeader->PacketNumber) == NTOHL(pReplyDataHeader->PacketNumber))
				&& (RtlCompareMemory(pReqDataHeader->DiskId, pReplyDataHeader->DiskId, 16) == 16)
				&& (NTOHL(pReqDataHeader->PartionId) == NTOHL(pReplyDataHeader->PartionId))
				&& ( NTOHLL(pReqDataHeader->LotNumber) == NTOHLL(pReplyDataHeader->LotNumber)) )
			{	

				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
					("Search Req and Set Event\n"));

				RemoveEntryList(&pPkt->PktListEntry);
				InitializeListHead(&pPkt->PktListEntry);
				pPkt->pLockContext->Status = NTOHL(pReplyDataHeader->LotState);
				KeSetEvent(&(pPkt->pLockContext->KEvent), 0, FALSE);

				DbgPrint("!!! Receive Lock Reply 4!!\n");

				LfsDereferenceDGPkt(pPkt);
				ExReleaseFastMutex(&SvrCtx->SendPktListMutex);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit CheckLockRequest Event SET!!\n" ) );
				return STATUS_SUCCESS;
			}

		}
		pListEntry = pListEntry->Flink;
	}
	ExReleaseFastMutex(&SvrCtx->SendPktListMutex);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit CheckLockRequest\n" ) );
	return STATUS_SUCCESS;
}

NTSTATUS
SetFileLock(
	IN PXIFS_VCB		pVCB,
	IN uint64			LotNumber
	)
{
	PXIFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter SetFileLock\n" ) );
	
	pFCB = XixFsdLookupFCBTable(pVCB,LotNumber);
	
	if(pFCB){

		if(pFCB->HasLock == FCB_FILE_LOCK_OTHER_HAS){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit SetFileLock FCB_FILE_LOCK_OTHER_HAS \n" ) );	
			RC = STATUS_SUCCESS;
		}
		else {
			pFCB->HasLock = FCB_FILE_LOCK_OTHER_HAS;
		}
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit SetFileLock FCB_FILE_LOCK_OTHER_HAS\n" ) );	
		return STATUS_SUCCESS;	
	}
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit SetFileLock\n" ) );	
	return STATUS_SUCCESS;	
}


VOID
ProcessLockBroadCast(
		IN PVOID			Parameter
)
{
	PXIFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIFS_VCB	pVCB = NULL;
	PXIFS_LOCK_BROADCAST	pDataHeader = NULL;



	PAGED_CODE();

	pRequest = (PXIFSDG_PKT)Parameter;
	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessLockBroadCast\n" ) );
	

	pDataHeader = &(pRequest->RawDataDG.LockBroadcast);
	
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);

		// Changed by ILGU HONG
		/*
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 6) == 6)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId) ) ){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = SetFileLock(pVCB, pDataHeader->LotNumber);
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockBroadCast\n" ) );
			return ;
		}
		*/
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 16) == 16)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId) ) ){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = SetFileLock(pVCB, NTOHLL(pDataHeader->LotNumber));
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockBroadCast\n" ) );
			return ;
		}

		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	LfsDereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessLockBroadCast\n" ) );

	return ;
}

NTSTATUS
FileFlush(
	IN PXIFS_VCB 	pVCB, 
	IN uint64		LotNumber,
	IN uint64		StartOffset, 
	IN uint32		DataSize
	)
{
	PXIFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;
	IO_STATUS_BLOCK  IoStatus;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter FileFlush\n" ) );
	
	pFCB = XixFsdLookupFCBTable(pVCB,LotNumber);

	if(pFCB){
		
		XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
		
		if(pFCB->FCBType == FCB_TYPE_FILE){
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
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit FileFlush\n" ) );
		return RC;
	}
	
	RC = STATUS_SUCCESS;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit FileFlush\n" ) );
	return RC;	
}



VOID
ProcessFlushBroadCast(
		IN PVOID			Parameter
)
{
	PXIFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIFS_VCB	pVCB = NULL;
	PXIFS_RANGE_FLUSH_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();

	pRequest = (PXIFSDG_PKT)Parameter;
	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessFlushBroadCast\n" ) );

	pDataHeader = &(pRequest->RawDataDG.FlushReq);
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);

		// Changed by ILGU HONG
		/*
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 6) == 6)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId))){
			
			
			RC = FileFlush(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHLL(pDataHeader->StartOffset), 
					NTOHL(pDataHeader->DataSize));

			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFlushBroadCast\n" ) );
			return;
			
			
		}
		*/
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 16) == 16)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId))){
			
			
			RC = FileFlush(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHLL(pDataHeader->StartOffset), 
					NTOHL(pDataHeader->DataSize));

			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFlushBroadCast\n" ) );
			return;
			
			
		}
		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	LfsDereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFlushBroadCast\n" ) );
	return ;	
}


NTSTATUS
ChangeFileLen(
	IN PXIFS_VCB		pVCB,
	IN uint64		LotNumber,
	IN uint64		FileLen,
	IN uint64		AllocationLen,
	IN uint64		WriteStartOffset
)	
{
	PXIFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;
	uint32			DataLength;
	LARGE_INTEGER	PrevFileSize;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ChangeFileLen\n" ) );	
	
	pFCB = XixFsdLookupFCBTable(pVCB,LotNumber);


	if(pFCB){
		XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
		XifsdLockFcb(TRUE,pFCB);
		PrevFileSize = pFCB->FileSize;
		pFCB->FileSize.QuadPart = FileLen;
		pFCB->ValidDataLength.QuadPart = FileLen;
		pFCB->AllocationSize.QuadPart = AllocationLen;
		//XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
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
				DbgPrint("Update Chagne Length Purge!!!!!");
				CcPurgeCacheSection(&(pFCB->SectionObject), &ByteOffset, DataLength, FALSE);
				//CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
				//CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);
				//CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
			}
		}

		
		XifsdReleaseFcb(TRUE, pFCB);
		
		XixFsdNotifyReportChange(
							pFCB,
							FILE_NOTIFY_CHANGE_SIZE,
							FILE_ACTION_MODIFIED
							);

		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ChangeFileLen\n" ) );
		return RC;
	}
	
	RC = STATUS_SUCCESS;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit ChangeFileLen\n" ) );	
	return RC;	
}


VOID
ProcessFileLenChange(
		IN PVOID Parameter
)
{
	PXIFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIFS_VCB	pVCB = NULL;
	PXIFS_FILE_LENGTH_CHANGE_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();

	pRequest = (PXIFSDG_PKT)Parameter;

	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessFileLenChange\n" ) );

	pDataHeader = &(pRequest->RawDataDG.FileLenChangeReq);
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);

		// Changed by ILGU HONG
		/*
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 6) == 6)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId) )){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = ChangeFileLen(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHLL(pDataHeader->FileLength), 
					NTOHLL(pDataHeader->AllocationLength),
					NTOHLL(pDataHeader->WriteStartOffset));
			
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFileLenChange\n" ) );
			return ;
		}
		*/
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 16) == 16)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId) )){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = ChangeFileLen(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHLL(pDataHeader->FileLength), 
					NTOHLL(pDataHeader->AllocationLength),
					NTOHLL(pDataHeader->WriteStartOffset));
			
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFileLenChange\n" ) );
			return ;
		}

		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	LfsDereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFileLenChange\n" ) );
	return ;		
}

NTSTATUS
FileChange(
	IN PXIFS_VCB	pVCB,
	IN uint64		LotNumber,
	IN uint32		SubCommand,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentFcb
)
{
	PXIFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter FileChange\n" ) );	
	
	DbgPrint("CALL Enter FileChange!!!!!");

	pFCB = XixFsdLookupFCBTable(pVCB,LotNumber);

	if(pFCB){
		
		//XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
		XifsdLockFcb(TRUE, pFCB);
		if(SubCommand == XIFS_SUBTYPE_FILE_DEL){
			DbgPrint("CALL XIFS_SUBTYPE_FILE_DELL EVENT!!!!!");
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_DELETED);
		}else if(SubCommand == XIFS_SUBTYPE_FILE_MOD){
			DbgPrint("CALL XIFS_SUBTYPE_FILE_MOD EVENT!!!!!");
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_RELOAD);
		}else if(SubCommand == XIFS_SUBTYPE_FILE_RENAME){
			DbgPrint("CALL XIFS_SUBTYPE_FILE_RENAME EVENT!!!!!");
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_RENAME);	
		}else if(SubCommand == XIFS_SUBTYPE_FILE_LINK){
			DbgPrint("CALL XIFS_SUBTYPE_FILE_LINK EVENT!!!!!");
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_LINK);
		}
		XifsdUnlockFcb(TRUE, pFCB);

		if(SubCommand == XIFS_SUBTYPE_FILE_RENAME){
	
			if(OldParentLotNumber != NewParentFcb){
				PXIFS_FCB	ParentFcb = NULL;
				PXIFS_LCB	Lcb = NULL;
				PLIST_ENTRY	ListLinks = NULL;
				
				ParentFcb = XixFsdLookupFCBTable(pVCB, OldParentLotNumber);
				
				if(ParentFcb) {

					

					for (ListLinks = pFCB->ParentLcbQueue.Flink;
						 ListLinks != &pFCB->ParentLcbQueue;
						 ) {
						
						Lcb = CONTAINING_RECORD( ListLinks, XIFS_LCB, ChildFcbLinks );

						ListLinks = ListLinks->Flink;

						if (Lcb->ParentFcb == ParentFcb) {
							XifsdAcquireFcbExclusive(TRUE, ParentFcb, FALSE);
							XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
							XixFsdRemovePrefix( TRUE, Lcb );
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


		if((SubCommand == XIFS_SUBTYPE_FILE_RENAME) || (SubCommand == XIFS_SUBTYPE_FILE_LINK)){
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
			XixFsReLoadFileFromFcb(pFCB);
			XifsdReleaseFcb(TRUE, pFCB);

			XixFsdNotifyReportChange(
								pFCB,
								((pFCB->FCBType == FCB_TYPE_FILE)
									?FILE_NOTIFY_CHANGE_FILE_NAME
									: FILE_NOTIFY_CHANGE_DIR_NAME ),
								FILE_ACTION_RENAMED_NEW_NAME
								);

		}



		if(SubCommand == XIFS_SUBTYPE_FILE_DEL){
			PXIFS_FCB	ParentFcb = NULL;
			PXIFS_LCB	Lcb = NULL;
			PLIST_ENTRY	ListLinks = NULL;
			
			DbgPrint("CALL XIFS_SUBTYPE_FILE_DELL EVENT!!!!!");
			ParentFcb = XixFsdLookupFCBTable(pVCB, OldParentLotNumber);
			
			if(ParentFcb) {

				for (ListLinks = pFCB->ParentLcbQueue.Flink;
					 ListLinks != &pFCB->ParentLcbQueue;
					 ) {
					
					Lcb = CONTAINING_RECORD( ListLinks, XIFS_LCB, ChildFcbLinks );

					ListLinks = ListLinks->Flink;

					if (Lcb->ParentFcb == ParentFcb) {
						XifsdSetFlag(Lcb->LCBFlags, XIFSD_LCB_STATE_LINK_IS_GONE);
						XifsdAcquireFcbExclusive(TRUE, ParentFcb, FALSE);
						XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
						XixFsdRemovePrefix( TRUE, Lcb );
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

			if (pFCB->SectionObject.ImageSectionObject != NULL) {
				MmFlushImageSection( &pFCB->SectionObject, MmFlushForDelete );
			}

			if(pFCB->SectionObject.DataSectionObject != NULL) {
				DbgPrint("Update Chagne DEL Purge!!!!!");
				CcPurgeCacheSection(&(pFCB->SectionObject), NULL, 0, FALSE);
			}



			XixFsdNotifyReportChange(
						pFCB,
						FILE_NOTIFY_CHANGE_CREATION,
						FILE_ACTION_REMOVED
						);

		}


		//XifsdReleaseFcb(TRUE, pFCB);
		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit FileChange\n" ) );	
		return RC;
	}
	
	RC = STATUS_SUCCESS;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit FileChange\n" ) );	
	return RC;	
}


VOID
ProcessFileChange(
		IN PVOID Parmeter
)
{
	PXIFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIFS_VCB	pVCB = NULL;
	PXIFS_FILE_CHANGE_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();
	pRequest = (PXIFSDG_PKT)Parmeter;
	ASSERT(pRequest);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessFileChange\n" ) );	

	pDataHeader = &(pRequest->RawDataDG.FileChangeReq);
	
	
	
		DbgPrint("FILE CHANGE LotNum(%I64d), SubCom(%d) PrevP(%I64d), NewP(%I64d)\n",
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->SubCommand),
					NTOHLL(pDataHeader->PrevParentLotNumber),
					NTOHLL(pDataHeader->NewParentLotNumber)
					);
	


	XifsdAcquireGData(TRUE);
	
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);

		// Changed by ILGU HONG
		/*
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 6) == 6)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId))){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = FileChange(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					pDataHeader->SubCommand,
					NTOHLL(pDataHeader->PrevParentLotNumber),
					NTOHLL(pDataHeader->NewParentLotNumber)
					);
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFileChange\n" ) );	
			return ;
			
			
		}
		*/

		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 16) == 16)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId))){
			
			//XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			RC = FileChange(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->SubCommand),
					NTOHLL(pDataHeader->PrevParentLotNumber),
					NTOHLL(pDataHeader->NewParentLotNumber)
					);
			//XifsdReleaseVcb(TRUE, pVCB);
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFileChange\n" ) );	
			return ;
			
			
		}

		pListEntry = pListEntry->Flink;
	}
	XifsdReleaseGData(TRUE);
	LfsDereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit ProcessFileChange\n" ) );	
	return ;		
}



NTSTATUS
DirChange(
	IN PXIFS_VCB pVCB, 
	IN uint64	LotNumber, 
	IN uint32 ChildSlotNumber,
	IN uint32 SubCommand
)
{
	PXIFS_FCB	pFCB = NULL;
	NTSTATUS		RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER	ByteOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter DirChange\n" ) );		
	
	pFCB = XixFsdLookupFCBTable(pVCB,LotNumber);


	if(pFCB){
	
		XifsdAcquireFcbExclusive(TRUE,pFCB, FALSE);
	
		
		
		
		if(!XifsdCheckFlagBoolean(pFCB->Type, 
			(XIFS_FD_TYPE_DIRECTORY| XIFS_FD_TYPE_ROOT_DIRECTORY)))
		{
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "Not Dir\n" ) );
			return STATUS_SUCCESS;
		}
		

		if(SubCommand == XIFS_SUBTYPE_DIR_ADD){
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
			}
			XifsdSetFlag(pChild->ChildFlag, XIFSD_CHILD_CHANGED_ADDED) ;
			setBitToMap(ChildSlotNumber, pFCB->ChildLotMap);
			*/
		}else if(SubCommand == XIFS_SUBTYPE_DIR_DEL){
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
			
			}
			clearBitToMap(ChildSlotNumber, pFCB->ChildLotMap);
			XifsdSetFlag(pChild->ChildFlag, XIFSD_CHILD_CHANGED_DELETE) ;
			*/
		}else {
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
				
			}
		
			XifsdSetFlag(pChild->ChildFlag, XIFSD_CHILD_CHANGED_UPDATE) ;
			*/
			
		}
		
		XifsdReleaseFcb(TRUE,pFCB);
		RC = STATUS_SUCCESS;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit DirChange\n" ) );
		return RC;	
	}
	
	RC = STATUS_SUCCESS;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit DirChange\n" ) );	
	return RC;	
}

VOID
ProcessDirChange(
		IN PVOID Parameter
)
{
	PXIFSDG_PKT		pRequest = NULL;
	NTSTATUS	RC = STATUS_UNSUCCESSFUL;
	PLIST_ENTRY 	pListEntry = NULL;
	PXIFS_VCB	pVCB = NULL;
	PXIFS_DIR_ENTRY_CHANGE_BROADCAST	pDataHeader = NULL;

	PAGED_CODE();
	
	pRequest = (PXIFSDG_PKT)Parameter;
	ASSERT(pRequest);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessDirChange\n" ) );	

	pDataHeader = &(pRequest->RawDataDG.DirChangeReq);
	
	XifsdAcquireGData(TRUE);
	pListEntry = XiGlobalData.XifsVDOList.Flink;
	while(pListEntry != &(XiGlobalData.XifsVDOList)){
		pVCB = CONTAINING_RECORD(pListEntry, XIFS_VCB, VCBLink);

		// Changed by ILGU HONG
		/*
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 6) == 6)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId))){
			
			

			RC = DirChange(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->ChildSlotNumber),
					pDataHeader->SubCommand);
			
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessDirChange\n" ) );
			return ;
		}
		*/
		if( (RtlCompareMemory(pVCB->DiskId, pDataHeader->DiskId, 16) == 16)
			&& (pVCB->PartitionId == NTOHL(pDataHeader->PartionId))){
			
			

			RC = DirChange(pVCB, 
					NTOHLL(pDataHeader->LotNumber), 
					NTOHL(pDataHeader->ChildSlotNumber),
					NTOHL(pDataHeader->SubCommand)
					);
			
			XifsdReleaseGData(TRUE);
			LfsDereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessDirChange\n" ) );
			return ;
		}

		pListEntry = pListEntry->Flink;

	}
	XifsdReleaseGData(TRUE);
	LfsDereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter ProcessDirChange\n" ) );	
	return ;		
}




BOOLEAN
DispatchDGReqPkt(
				IN PXIFS_COMM_CTX		SvrCtx,
				IN OUT PXIFSDG_PKT		pRequest
)
{
	NTSTATUS							RC = STATUS_SUCCESS;
	PXIFS_COMM_HEADER					pHeader = NULL;
	PXIFSDG_RAWPK_DATA					pData = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter DispatchDGReqPkt\n" ) );

	pHeader = &pRequest->RawHeadDG;
	pData = &pRequest->RawDataDG;
	
	switch(NTOHL(pHeader->Type)){
	case XIFS_TYPE_LOCK_REQUEST:
	{
		// Changed by ILGU HONG
		//	chesung suggest
		/*
		LPX_ADDRESS CheckAddress;
		RtlZeroMemory(&CheckAddress, sizeof(LPX_ADDRESS));

		CheckAddress.Port = 0;
		RtlCopyMemory(CheckAddress.Node, pData->LockReq.LotOwnerMac, 6);
		if(LfsIsFromLocal(&CheckAddress)){

			ExInitializeWorkItem( 
							&(pRequest->WorkQueueItem),
							ProcessLockRequest,
							(VOID *)pRequest 
							);

			ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );			
			return FALSE;

		}
		return TRUE;
		*/
		
		if(RtlCompareMemory(SvrCtx->HostMac, pData->LockReq.LotOwnerMac, 32) == 32){
			ExInitializeWorkItem( 
							&(pRequest->WorkQueueItem),
							ProcessLockRequest,
							(VOID *)pRequest 
							);

			ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );			
			return FALSE;
		}
		return TRUE;


	}
	break;
	case XIFS_TYPE_LOCK_REPLY:
	{
		// Changed by ILGU HONG
		//	chesung suggest
		/*
		LPX_ADDRESS CheckAddress;
		RtlZeroMemory(&CheckAddress, sizeof(LPX_ADDRESS));

		CheckAddress.Port = 0;
		RtlCopyMemory(CheckAddress.Node, pHeader->DstMac, 6);
		if(LfsIsFromLocal(&CheckAddress)){
			RC= CheckLockRequest(SvrCtx, pRequest);
			if(RC == STATUS_PENDING) return FALSE;
			return TRUE;
		}
		return TRUE;
		*/

		if(RtlCompareMemory(SvrCtx->HostMac, pHeader->DstMac, 32) == 32){
			RC= CheckLockRequest(SvrCtx, pRequest);
			if(RC == STATUS_PENDING) return FALSE;
			return TRUE;
		}
		return TRUE;

	}
	break;
	case XIFS_TYPE_LOCK_BROADCAST:
	{

		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						ProcessLockBroadCast,
						(VOID *)pRequest 
						);

		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	

		return FALSE;
	}
	break;
	case XIFS_TYPE_FLUSH_BROADCAST:
	{

		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						ProcessFlushBroadCast,
						(VOID *)pRequest 
						);

		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	

		return FALSE;
	}
	break;
	case XIFS_TYPE_FILE_LENGTH_CHANGE:
	{

		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						ProcessFileLenChange,
						(VOID *)pRequest 
						);

		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	

		return FALSE;			
	}
	break;
	case XIFS_TYPE_FILE_CHANGE:
	{
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						ProcessFileChange,
						(VOID *)pRequest 
						);

		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	

		return FALSE;			
	}
	break;
	case XIFS_TYPE_DIR_CHANGE:
	{
		ExInitializeWorkItem( 
						&(pRequest->WorkQueueItem),
						ProcessDirChange,
						(VOID *)pRequest 
						);

		ExQueueWorkItem( &pRequest->WorkQueueItem, CriticalWorkQueue );	
		return FALSE;							
	}
	break;
	default:
		return TRUE;
		break;
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit DispatchDGReqPkt\n" ) );
}

VOID
XixFsdComSvrThreadProc(
		PVOID	lpParameter
)
{
	PXIFS_COMM_CTX		SvrCtx = (PXIFS_COMM_CTX)lpParameter;
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	LFSDG_Socket		ServerDatagramSocket;
	PLIST_ENTRY			listEntry = NULL;	
	PKEVENT				Evts[3];
	PXIFSDG_PKT			pPacket = NULL;
	BOOLEAN				bret = FALSE;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Enter XifsComSvrThreadProc\n" ) );

	// Open server datagram port
	RC = LfsOpenDGSocket(
		&ServerDatagramSocket,
		DEFAULT_XIFS_SVRPORT
	);

	if(NT_SUCCESS(RC)) {

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Open Complete  and call Register Receive Handler\n" ) );
		// register Datagram handler
		RC = LfsRegisterDatagramRecvHandler(
			&ServerDatagramSocket,
			XixFsdSvrDatagramRecvHandler,
			SvrCtx
		);

	}else{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "ERROR XifsComSvrThreadProc:LfsOpenDGSocket Fail \n" ) );
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
			LfsCloseDGSocket(
				&ServerDatagramSocket
			);
retry:
			RetryCount++;
			RC  = LfsOpenDGSocket(
							&ServerDatagramSocket,
							DEFAULT_XIFS_SVRPORT
						);
			
			if(NT_SUCCESS(RC )) {
				// register Datagram handler
				RC  = LfsRegisterDatagramRecvHandler(
								&ServerDatagramSocket,
								XixFsdSvrDatagramRecvHandler,
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
			
			ExAcquireFastMutex(&SvrCtx->RecvPktListMutex);
			listEntry = RemoveHeadList(&SvrCtx->RecvPktList);
			ExReleaseFastMutex(&SvrCtx->RecvPktListMutex);
			

			if(listEntry == &SvrCtx->RecvPktList) {
				DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
					( "[LFS] XifsComSvrThreadProc: Datagram queue empty. back to waiting mode.\n"));
				break;
			}
			pPacket = CONTAINING_RECORD(listEntry, XIFSDG_PKT, PktListEntry);
			if(LfsIsFromLocal(&pPacket->SourceAddr)) {
				DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
					( "[LFS] XifsComSvrThreadProc: Datagram packet came from the local address "
						"%02X:%02X:%02X:%02X:%02X:%02X/%d\n", 
					pPacket->SourceAddr.Node[0], pPacket->SourceAddr.Node[1], pPacket->SourceAddr.Node[2],
					pPacket->SourceAddr.Node[3], pPacket->SourceAddr.Node[4], pPacket->SourceAddr.Node[5],
					pPacket->SourceAddr.Port
					));
				LfsDereferenceDGPkt(pPacket);
				
	
				continue;
			}

			DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
				( "[LFS] XifsComSvrThreadProc: Datagram packet came from the other address "
					"%02X:%02X:%02X:%02X:%02X:%02X/%d\n", 
				pPacket->SourceAddr.Node[0], pPacket->SourceAddr.Node[1], pPacket->SourceAddr.Node[2],
				pPacket->SourceAddr.Node[3], pPacket->SourceAddr.Node[4], pPacket->SourceAddr.Node[5],
				pPacket->SourceAddr.Port
				));

			bret = DispatchDGReqPkt(
				SvrCtx,
				pPacket
			);

			if(TRUE == bret) {
				LfsDereferenceDGPkt(pPacket);
			}
		}


	}

	LfsCloseDGSocket(
		&ServerDatagramSocket
	);	
error_out2:
	// to do something
error_out:	
	PsTerminateSystemThread(RC);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "Exit XifsComSvrThreadProc\n" ) );
	return;
	
}



VOID
XixFsdComCliThreadProc(
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
	PXIFSDG_PKT			pPacket = NULL;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Enter XifsComCliThreadProc\n" ) );	

	// Open server datagram port
	RC  = LfsOpenDGSocket(
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
		TimeOut.QuadPart = - DEFAULT_XIFS_CLIWAIT;
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


			LfsCloseDGSocket(
				&ClieSocketAddress
			);
			
			RetryCount++;
			RC  = LfsOpenDGSocket(
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
			DestBroadcastAddr.Port = HTONS(DEFAULT_XIFS_SVRPORT);
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
				ExAcquireFastMutex(&CliCtx->SendPktListMutex);
				pList = RemoveHeadList(&CliCtx->SendPktList);
				ExReleaseFastMutex(&CliCtx->SendPktListMutex);

				if(pList == &CliCtx->SendPktList) break;
				
				pPacket =  (PXIFSDG_PKT)CONTAINING_RECORD(pList, XIFSDG_PKT, PktListEntry);

				if(NTOHL(pPacket->RawHeadDG.Type) == XIFS_TYPE_LOCK_REQUEST){
					if(pPacket->TimeOut.QuadPart < XixGetSystemTime().QuadPart){
						
						DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_HOSTCOM, 
							( "[XifsComCliThreadProc] LOCK TIMEOUT Req(%I64d) Check(%64d)\n", pPacket->TimeOut.QuadPart, XixGetSystemTime().QuadPart));	
						
						InitializeListHead(&pPacket->PktListEntry);
						pPacket->pLockContext->Status = LOCK_TIMEOUT;
						KeSetEvent(&(pPacket->pLockContext->KEvent), 0, FALSE);
						LfsDereferenceDGPkt(pPacket);
						continue;
					}
					IsRemove = FALSE;
				}else{
					IsRemove = TRUE;
				}


				DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_HOSTCOM, 
					( "[XifsComCliThreadProc] SendReq (%p)\n ", pPacket));	
				
				RC= LfsSendDatagram(
						&ClieSocketAddress,
						pPacket,
						&DestBroadcastAddr
					);					

				if(TRUE ==IsRemove){
					LfsDereferenceDGPkt(pPacket);
				}else{
					
					InsertTailList(&(TempList),&(pPacket->PktListEntry) );					
					
				}


	

			}

			pList = RemoveHeadList(&TempList);
			while(pList != &TempList){
				ExAcquireFastMutex(&CliCtx->SendPktListMutex);
				InsertTailList(&(CliCtx->SendPktList),&(pPacket->PktListEntry));		
				ExReleaseFastMutex(&CliCtx->SendPktListMutex);
				pList = RemoveHeadList(&TempList);
			}			


		}else if(STATUS_ALERTED == RC){
			break;
		}

	}

error_out2:	
	ExAcquireFastMutex(&CliCtx->SendPktListMutex);
	pList = CliCtx->SendPktList.Flink;
	while(pList != &CliCtx->SendPktList){
		pPacket =  (PXIFSDG_PKT)CONTAINING_RECORD(pList, XIFSDG_PKT, PktListEntry);
		pList = pList->Flink;
		RemoveEntryList(&pPacket->PktListEntry);
		InitializeListHead(&pPacket->PktListEntry);
		if(NTOHL(pPacket->RawHeadDG.Type) == XIFS_TYPE_LOCK_REQUEST){
			pPacket->pLockContext->Status = LOCK_TIMEOUT;
			KeSetEvent(&(pPacket->pLockContext->KEvent), 0, FALSE);
		}
		
		LfsDereferenceDGPkt(pPacket);
	}
	ExReleaseFastMutex(&CliCtx->SendPktListMutex);
	
	LfsCloseDGSocket(
		&ClieSocketAddress
	);	

	

	// to do something
error_out:	
	PsTerminateSystemThread(RC);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit XifsComCliThreadProc\n" ) );	
	return;
}
