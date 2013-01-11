#include "lfsproc.h"



///////////////////////////////////////////////////////////////////////////
//
//	LFS datagram server
//

LFSDGRAMSVR_CTX	LFSDGSvrCtx;
LFSDGRAMNTC_CTX	LFSNtcCtx;

//	Datagram receive handler

NTSTATUS
LfsSvrDatagramRecvHandler (
	IN PVOID	TdiEventContext,
	IN LONG		SourceAddressLength,
	IN PVOID	SourceAddress,
	IN LONG		OptionsLength,
	IN PVOID	Options,
	IN ULONG	ReceiveDatagramFlags,
	IN ULONG	BytesIndicated,
	IN ULONG	BytesAvailable,
	OUT ULONG   *BytesTaken,
	IN PVOID	Tsdu,
	OUT PIRP	*IoRequestPacket
	)
{
	PLFSDGRAMSVR_CTX	SvrCtx = (PLFSDGRAMSVR_CTX)TdiEventContext;
	PTRANSPORT_ADDRESS	ClientAddr = (PTRANSPORT_ADDRESS)SourceAddress;
	PLPX_ADDRESS		ClientLpxAddr = (PLPX_ADDRESS)ClientAddr->Address[0].Address;
	PLFSDG_PKT			Pkt;
	BOOLEAN				bRet;
	static UCHAR		Protocol[4] = LFS_DATAGRAM_PROTOCOL;

	UNREFERENCED_PARAMETER( SourceAddressLength );
	UNREFERENCED_PARAMETER( OptionsLength );
	UNREFERENCED_PARAMETER( Options );
	UNREFERENCED_PARAMETER( ReceiveDatagramFlags );
	UNREFERENCED_PARAMETER( BytesTaken );
	UNREFERENCED_PARAMETER( IoRequestPacket );

#if !DBG
	UNREFERENCED_PARAMETER(BytesIndicated);
#endif

	// BytesIndicated, BytesAvailable, BytesTaken
	
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
				   ("[LFS] LfsSvrDatagramRecvHandler: BytesIndicated : %d BytesAvailable : %d\n",
					BytesIndicated, BytesAvailable) );

	if (BytesAvailable < sizeof(NDFT_HEADER) ) {
		
		SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] LfsSvrDatagramRecvHandler: too small bytes.\n" ) );
		goto not_accepted;
	}

	bRet = LfsAllocDGPkt( &Pkt, MAX_DATAGRAM_DATA_SIZE );
	
	if (FALSE == bRet) {
	
		SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] LfsSvrDatagramRecvHandler: LfsAllocDGPkt() failed.\n") );
		goto not_accepted;
	}

	//	read the head

	RtlCopyMemory( &Pkt->RawHeadDG, Tsdu, sizeof(Pkt->RawHeadDG) );

	//	validation check

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					("[LFS] LfsSvrDatagramRecvHandler: Protocol:%lx Ver:%x.%x TotalSize:%ld"
					 " Type:%x\n",
					 *((ULONG *)Pkt->RawHeadDG.Protocol), 
					 NTOHS(Pkt->RawHeadDG.NdfsMajorVersion2), 
					 NTOHS(Pkt->RawHeadDG.NdfsMinorVersion2),
					 NTOHL(Pkt->RawHeadDG.MessageSize4), 
					 NTOHS(Pkt->RawHeadDG.Type2)) );

	if (RtlCompareMemory(Pkt->RawHeadDG.Protocol, Protocol, 4) != 4 ||
		NTOHS(Pkt->RawHeadDG.NdfsMajorVersion2) != LFS_DATAGRAM_MAJVER		||
		NTOHS(Pkt->RawHeadDG.NdfsMinorVersion2) != LFS_DATAGRAM_MINVER		||
		(NTOHS(Pkt->RawHeadDG.Type2) & LFSPKTTYPE_PREFIX) != (LFSPKTTYPE_DATAGRAM | LFSPKTTYPE_REQUEST) ) {

		LfsDereferenceDGPkt(Pkt);

		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("[LFS] LfsSvrDatagramRecvHandler: Invalid reply header.\n") );
		goto not_accepted;
	}

	Pkt->PacketSize	= NTOHL(Pkt->RawHeadDG.MessageSize4);
	Pkt->DataSize	= NTOHL(Pkt->RawHeadDG.MessageSize4) - sizeof(NDFT_HEADER);

	if (Pkt->PacketSize != sizeof(NDFT_HEADER) + Pkt->DataSize ) {

		LfsDereferenceDGPkt(Pkt);
		SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ("[LFS] LfsSvrDatagramRecvHandler: Invalid reply packet size.\n") );
		goto not_accepted;
	}

	if (BytesAvailable < Pkt->PacketSize) {
	
		LfsDereferenceDGPkt(Pkt);
		SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ("[LFS] LfsSvrDatagramRecvHandler: wrong message size contained.\n") );
		goto not_accepted;
	}

	//	retrieve the source address.
	//	Do not trust Owner's address in the packet.

	RtlCopyMemory(&Pkt->SourceAddr, ClientLpxAddr, sizeof(LPX_ADDRESS));

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
				  ("[LFS] LfsSvrDatagramRecvHandler:"
				   " received a datagram packet from %02X:%02X:%02X:%02X:%02X:%02X/%d\n",
				   ClientLpxAddr->Node[0],ClientLpxAddr->Node[1],ClientLpxAddr->Node[2],
				   ClientLpxAddr->Node[3],ClientLpxAddr->Node[4],ClientLpxAddr->Node[5],
				   (int)ClientLpxAddr->Port) );

	//	read the data

	RtlCopyMemory( &Pkt->RawDataDG, (PUCHAR)Tsdu + sizeof(Pkt->RawHeadDG), Pkt->DataSize );

	//	insert to the packet queue

	InitializeListHead( &Pkt->PktListEntry );

	ExInterlockedInsertTailList( &SvrCtx->RecvDGPktQueue,
								 &Pkt->PktListEntry,
								 &SvrCtx->RecvDGPktQSpinLock );

	KeSetEvent( &SvrCtx->DatagramRecvEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_SUCCESS;

not_accepted:

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
				   ("[LFS] LfsSvrDatagramRecvHandler: a datagram packet rejected.\n") );

	return STATUS_DATA_NOT_ACCEPTED;
}

//	dispatch a request packet so that it can build a proper reply packet

BOOLEAN
DispatchDGReqPkt (
	IN PLFSDGRAMSVR_CTX		SvrCtx,
	IN OUT PLFSDG_PKT		pRequest
	) 
{
	BOOLEAN				bSendReply = FALSE;
	PNDFT_HEADER		RawHead = &pRequest->RawHeadDG;
	PLFSDG_RAWPKT_DATA	RawData = &pRequest->RawDataDG;


	UINT8	NdscId[NDSC_ID_LENGTH];	

	UNREFERENCED_PARAMETER(SvrCtx);

	RawHead->Type2 &= HTONS(~(LFSPKTTYPE_REQUEST | LFSPKTTYPE_BROADCAST));
	RawHead->Type2 |= HTONS((LFSPKTTYPE_REPLY | LFSPKTTYPE_DATAGRAM));

	switch (NTOHS(RawHead->Type2) & LFSPKTTYPE_MAJTYPE) {

	case NDFT_PRIMARY_UPDATE_MESSAGE: {
		
		LPX_ADDRESS			NetDiskAddress;
		LPX_ADDRESS			PrimaryAddress;

		//	try to update NetDisk entry specified in the request packet.
		
		RtlCopyMemory( NetDiskAddress.Node, RawData->Owner.NetDiskNode, ETHER_ADDR_LENGTH );
		NetDiskAddress.Port = HTONS(RawData->Owner.NetDiskPort);
		RtlCopyMemory( NdscId, RawData->Owner.NdscId, NDSC_ID_LENGTH );

		RtlCopyMemory( PrimaryAddress.Node, pRequest->SourceAddr.Node, ETHER_ADDR_LENGTH );
		PrimaryAddress.Port = HTONS(RawData->Owner.PrimaryPort);

		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					   ("[LFS] DispatchDGReqPkt: BCASTOWNER: %02x:%02x:%02x:%02x:%02x:%02x/%d.\n",
						NetDiskAddress.Node[0], NetDiskAddress.Node[1], NetDiskAddress.Node[2],
						NetDiskAddress.Node[3], NetDiskAddress.Node[4], NetDiskAddress.Node[5],
						NTOHS(NetDiskAddress.Port)) );

		LfsTable_UpdatePrimaryInfo( SvrCtx->LfsTable, 
									&NetDiskAddress, 
									(UCHAR)RawData->Owner.UnitDiskNo, 
									&PrimaryAddress, 
									NdscId );
		break;
	}

	default:

		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ( "[LFS] DispatchDGReqPkt: invalid request.\n") );
	}

	return bSendReply;
}

VOID
DgSvr_NetEvtCallback (
	PSOCKETLPX_ADDRESS_LIST	Original,
	PSOCKETLPX_ADDRESS_LIST	Updated,
	PSOCKETLPX_ADDRESS_LIST	Disabled,
	PSOCKETLPX_ADDRESS_LIST	Enabled,
	PLFSDGRAMSVR_CTX		SvrCtx
	) 
{

#if DBG
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Original Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Original);
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Updated Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Updated);
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Disabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Disabled);
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Enabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Enabled);
#else
	UNREFERENCED_PARAMETER(Original);
	UNREFERENCED_PARAMETER(Updated);
	UNREFERENCED_PARAMETER(Disabled);
	UNREFERENCED_PARAMETER(Enabled);
#endif

	KeSetEvent( &SvrCtx->NetworkEvent, IO_NO_INCREMENT, FALSE );
}

//	server thread of LFS datagram server

VOID
RedirDataGramSvrThreadProc (
	PLFSDGRAMSVR_CTX	SvrCtx
	) 
{
	BOOLEAN				bret;
	NTSTATUS			status;
	LFSDG_Socket		ServerDatagramSocket;
	PKEVENT				Evts[3];
	PLIST_ENTRY			listEntry;
	PLFSDG_PKT			pkt;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
				   ("[LFS] RedirDataGramSvrThreadProc: Initializing Redirection Datagram server...\n") );

	// Open server datagram port

	status = LfsOpenDGSocket( &ServerDatagramSocket, DEFAULT_DATAGRAM_SVRPORT );

	if (status == STATUS_SUCCESS) {

		// register Datagram handler

		status = LfsRegisterDatagramRecvHandler( &ServerDatagramSocket,
												 LfsSvrDatagramRecvHandler,
												 SvrCtx );
	}

	Evts[0] = &SvrCtx->ShutdownEvent;
	Evts[1] = &SvrCtx->DatagramRecvEvent;
	Evts[2] = &SvrCtx->NetworkEvent;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
					("[LFS] RedirDataGramSvrThreadProc: Redirection Datagram server started...\n") );

	// Main loop...

	while(1) {
	
		status = KeWaitForMultipleObjects( 3,
										   Evts,
										   WaitAny,
										   Executive,
										   KernelMode,
										   TRUE,
										   NULL,
										   NULL );

		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					   ("[LFS] RedirDataGramSvrThreadProc: NTSTATUS:%lu\n", status) );

		if (status == 0) {

			SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
						   ("[LFS] RedirDataGramSvrThreadProc: ShutDown event received. NTSTATUS:%lu\n", status) );

			break;
		
		} else if (status == 1) {
			
			//	Datagram received.
			
			KeClearEvent( &SvrCtx->DatagramRecvEvent );

			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						   ("[LFS] RedirDataGramSvrThreadProc: DatagramRecv event received. NTSTATUS:%lu\n", status) );
		
		} else if (status == 2) {
			
			// Renew server datagram port
			
			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						   ("[LFS] RedirDataGramSvrThreadProc: Networking event received. NTSTATUS:%lu\n", status) );

			KeClearEvent( &SvrCtx->NetworkEvent );

			LfsCloseDGSocket( &ServerDatagramSocket );

			status = LfsOpenDGSocket( &ServerDatagramSocket, DEFAULT_DATAGRAM_SVRPORT );
			
			if (status == STATUS_SUCCESS) {
				
				// register Datagram handler
				
				status = LfsRegisterDatagramRecvHandler( &ServerDatagramSocket,
														 LfsSvrDatagramRecvHandler,
														 SvrCtx );
			}

			continue;

		} else {
			
			SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
						   ("[LFS] RedirDataGramSvrThreadProc: KeWaitForMultipleObjects() failed. NTSTATUS:%lu\n", status) );
			break;
		}

		//	get packets to the dispatch routine.

		while (1) {

			listEntry = ExInterlockedRemoveHeadList( &SvrCtx->RecvDGPktQueue,
													 &SvrCtx->RecvDGPktQSpinLock );

			if (!listEntry) {
			
				SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
							   ("[LFS] RedirDataGramSvrThreadProc: Datagram queue empty. back to waiting mode.\n") );
				break;
			}

			pkt = CONTAINING_RECORD( listEntry, LFSDG_PKT, PktListEntry );

			bret = DispatchDGReqPkt( SvrCtx, pkt );

			if (FALSE == bret) {
			
				SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ( "[LFS] RedirDataGramSvrThreadProc: Don't send a reply.\n") );

			} else {

				SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] RedirDataGramSvrThreadProc: Send a reply.\n") );

				status = LfsSendDatagram( &ServerDatagramSocket, pkt, &pkt->SourceAddr );

				if (status != STATUS_SUCCESS) {

					SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] RedirDataGramSvrThreadProc: LfsSendDatagram() failed.\n"));
				}
			}

			//	dereference the datagram packet
	
			LfsDereferenceDGPkt( pkt );
		}
	}

	LfsCloseDGSocket( &ServerDatagramSocket );

	//	free datagram packets.

	while (1) {

		listEntry = ExInterlockedRemoveHeadList( &SvrCtx->RecvDGPktQueue,
												 &SvrCtx->RecvDGPktQSpinLock );
		
		if (!listEntry) {
		
			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						   ("[LFS] RedirDataGramSvrThreadProc: Cleared datagram packets.\n") );
			break;
		}

		pkt = CONTAINING_RECORD(listEntry, LFSDG_PKT, PktListEntry);
		
		LfsDereferenceDGPkt( pkt );
	}

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] RedirDataGramSvrThreadProc: terminated..\n") );
	PsTerminateSystemThread(0);

	return;
}

//////////////////////////////////////////////////////////////////////////
//
//	Datagram notifier
//


VOID
DgNtc_NetEvtCallback (
	PSOCKETLPX_ADDRESS_LIST	Original,
	PSOCKETLPX_ADDRESS_LIST	Updated,
	PSOCKETLPX_ADDRESS_LIST	Disabled,
	PSOCKETLPX_ADDRESS_LIST	Enabled,
	PVOID					Context
	) 
{
	PLFSDGRAMNTC_CTX	NtcCtx = (PLFSDGRAMNTC_CTX)Context;

#if DBG
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Original Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Original);
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Updated Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Updated);
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Disabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Disabled);
	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Enabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_LIB_NOISE, Enabled);
#else
	UNREFERENCED_PARAMETER(Original);
	UNREFERENCED_PARAMETER(Updated);
	UNREFERENCED_PARAMETER(Disabled);
	UNREFERENCED_PARAMETER(Enabled);
#endif

	InterlockedExchange( &NtcCtx->NetEvt, 1 );
}

VOID
RedirDataGramNotifierThreadProc (
	IN PLFSDGRAMNTC_CTX	NtcCtx
	) 
{
	NTSTATUS			status;
	LFSDG_Socket		DGBcastSocket;
	PLIST_ENTRY			listEntry;
	LARGE_INTEGER		TimeOut;
	LPX_ADDRESS			BroadcastAddr;
	PLFSDG_PKT			Dgpkt;
	PLFSTAB_ENTRY		LfsTableEntry;
	KIRQL				oldIrql;
	BOOLEAN				bRet;
	LONG				NetEvent;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ("[LFS] DataGramNotifier: Initializing Datagram broadcaster...\n") );

	// Open server datagram port

	status = LfsOpenDGSocket( &DGBcastSocket, 0 );

	BroadcastAddr.Node[0] = 0xFF;
	BroadcastAddr.Node[1] = 0xFF;
	BroadcastAddr.Node[2] = 0xFF;
	BroadcastAddr.Node[3] = 0xFF;
	BroadcastAddr.Node[4] = 0xFF;
	BroadcastAddr.Node[5] = 0xFF;
	BroadcastAddr.Port = HTONS(DEFAULT_DATAGRAM_SVRPORT);

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ("[LFS] DataGramNotifier: Redirection Datagram server started...\n") );

	// Main loop...

	do {

		TimeOut.QuadPart = - DGNOTIFICATION_FREQ;
		
		status = KeWaitForSingleObject( &NtcCtx->ShutdownEvent,
										Executive,
										KernelMode,
										FALSE,
										&TimeOut );

		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ( "[LFS] DataGramNotifier: NTSTATUS:%lu\n", status) );
		
		if (status == 0) {

			SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
						   ("[LFS] DataGramNotifier: ShutDown event received. NTSTATUS:%lu\n", status) );
			break;

		} else if (status == STATUS_TIMEOUT) {
		
			//	Time Out.
		
			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						   ("[LFS] DataGramNotifier: ShutDown event time out. Go ahead and broadcast. NTSTATUS:%lu\n", 
						    status) );
		
		} else {

			NDAS_ASSERT( FALSE );
			break;
		}

		//	check to see if network event arise.

		NetEvent = InterlockedExchange( &NtcCtx->NetEvt, 0 );

		if (NetEvent) {

			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("DataGramNotifier: Renew the socket.\n") );

			//	renew the socket.
			
			LfsCloseDGSocket( &DGBcastSocket );

			status = LfsOpenDGSocket( &DGBcastSocket, 0 );

			if (status != STATUS_SUCCESS) {
			
				LARGE_INTEGER	interval;

				SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
							   ("DataGramNotifier: could not renew the socket. NTSTATUS:%08lx\n", status) );

				interval.QuadPart = -NANO100_PER_SEC;
				KeDelayExecutionThread(KernelMode, FALSE, &interval);
		
				continue;
			}
		}

		//	traverse through to make up broadcast messages.

		KeAcquireSpinLock( &NtcCtx->LfsTable->SpinLock, &oldIrql );

		listEntry = NtcCtx->LfsTable->LfsTabPartitionList.Flink;

		while (listEntry != &NtcCtx->LfsTable->LfsTabPartitionList) {

			LfsTableEntry = CONTAINING_RECORD(listEntry, LFSTAB_ENTRY, LfsTabPartitionEntry );
			listEntry = listEntry->Flink;

			//	If this host is primary for the NDAS device,
			//	broadcast it.

			if (!FlagOn(LfsTableEntry->Flags, LFSTABENTRY_FLAG_ACTPRIMARY)) {

				continue;
			}

			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						   ("DataGramNotifier: broadcast NetDisk: %02x:%02x:%02x:%02x:%02x:%02x/%d UD:%d\n", 
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node[0],
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node[1],
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node[2],
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node[3],
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node[4],
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node[5],
							 NTOHS(LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Port),
							 LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo) );

			bRet = LfsAllocDGPkt( &Dgpkt, sizeof(NDFT_PRIMARY_UPDATE) );

			if (bRet == FALSE) {

				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
				continue;
			}

			//	Initialize the datagram packet.
			//	We do not use owner's network address.
			//	datagram packet receiving handler must be possible to 
			//  retrieve the sender's address(owner's network address).
			
			Dgpkt->RawHeadDG.Type2 = HTONS( LFSPKTTYPE_DATAGRAM | LFSPKTTYPE_REQUEST | NDFT_PRIMARY_UPDATE_MESSAGE );
			
			RtlCopyMemory( Dgpkt->RawDataDG.Owner.NetDiskNode,
						   LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Node,
						   ETHER_ADDR_LENGTH );

			Dgpkt->RawDataDG.Owner.NetDiskPort = NTOHS(LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NetDiskAddress.Port);
			Dgpkt->RawDataDG.Owner.UnitDiskNo = (UCHAR)LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.UnitDiskNo;

			RtlCopyMemory( Dgpkt->RawDataDG.Owner.NdscId,
						   LfsTableEntry->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.NdscId,
						   NDSC_ID_LENGTH );

			Dgpkt->RawDataDG.Owner.PrimaryPort = DEFAULT_PRIMARY_PORT;

			//	Insert SendPkts Queue

			InsertHeadList( &NtcCtx->SendPkts, &Dgpkt->PktListEntry );
		}

		KeReleaseSpinLock( &NtcCtx->LfsTable->SpinLock, oldIrql );

		//	send notification messages by broadcast.

		while (!IsListEmpty(&NtcCtx->SendPkts)) {
			
			LPX_ADDRESS			NetDiskAddress;
			LPX_ADDRESS			PrimaryAddress;
			PNDFT_HEADER		RawHead;
			PLFSDG_RAWPKT_DATA	RawData;

			UINT8				NdscId[NDSC_ID_LENGTH];

			listEntry = RemoveHeadList(&NtcCtx->SendPkts);
			Dgpkt = CONTAINING_RECORD(listEntry, LFSDG_PKT, PktListEntry);

			status = LfsSendDatagram( &DGBcastSocket, Dgpkt, &BroadcastAddr );

			if (status != STATUS_SUCCESS) {
			
				SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] DataGramNotifier: LfsSendDatagram() failed.\n") );
				NDAS_ASSERT( NDAS_ASSERT_NETWORK_FAIL );
			}

			//	NDIS in Windows Server 2003 doesn't seem to support loopback of broadcasting packets.
			//	Update NetDisk table here.

			RawHead = &Dgpkt->RawHeadDG;
			RawData = &Dgpkt->RawDataDG;

			RtlCopyMemory(NetDiskAddress.Node, RawData->Owner.NetDiskNode, ETHER_ADDR_LENGTH);
			NetDiskAddress.Port = HTONS(RawData->Owner.NetDiskPort);

			RtlCopyMemory( NdscId, RawData->Owner.NdscId, NDSC_ID_LENGTH );

			RtlCopyMemory(PrimaryAddress.Node, Dgpkt->SourceAddr.Node, ETHER_ADDR_LENGTH);
			PrimaryAddress.Port = HTONS(RawData->Owner.PrimaryPort);

			SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						   ("[LFS] RedirDataGramNotifierThread: updating %02x:%02x:%02x:%02x:%02x:%02x/%d.\n",
							NetDiskAddress.Node[0], NetDiskAddress.Node[1], NetDiskAddress.Node[2],
							NetDiskAddress.Node[3], NetDiskAddress.Node[4], NetDiskAddress.Node[5],
							NTOHS(NetDiskAddress.Port)) );

			LfsTable_UpdatePrimaryInfo( NtcCtx->LfsTable, 
										&NetDiskAddress, 
										(UCHAR)RawData->Owner.UnitDiskNo, 
										&PrimaryAddress, 
										NdscId );

			//	Because LfsSendDatagram() is a synchronous I/O function,
			//	we can dereference the pkt here.
	
			LfsDereferenceDGPkt( Dgpkt );
		}
	
	} while (1);

	LfsCloseDGSocket( &DGBcastSocket );

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, ( "[LFS] DataGramNotifier: terminated..\n") );

	PsTerminateSystemThread( 0 );

	return;
}


BOOLEAN
RdsvrDatagramInit (
	PLFS_TABLE	LfsTable,
	PVOID		*DGSvrCtx,
	PVOID		*NtcCtx
	) 
{
	NTSTATUS			status;
	OBJECT_ATTRIBUTES	objectAttributes;

	//	create the datagram server thread

	LFSDGSvrCtx.LfsTable = LfsTable;
	*DGSvrCtx = &LFSDGSvrCtx;

	KeInitializeEvent( &LFSDGSvrCtx.ShutdownEvent, NotificationEvent, FALSE );
	KeInitializeEvent( &LFSDGSvrCtx.NetworkEvent, NotificationEvent, FALSE );
	KeInitializeEvent( &LFSDGSvrCtx.DatagramRecvEvent, NotificationEvent, FALSE );

	KeInitializeSpinLock( &LFSDGSvrCtx.RecvDGPktQSpinLock );
	InitializeListHead( &LFSDGSvrCtx.RecvDGPktQueue );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	status = PsCreateSystemThread( &LFSDGSvrCtx.hSvrThread,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   RedirDataGramSvrThreadProc,
								   &LFSDGSvrCtx );

	if (!NT_SUCCESS(status)) {

		NDAS_ASSERT( FALSE );
		return FALSE;
	}

	//	create the datagram notification broadcaster
	
	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	KeInitializeEvent( &LFSNtcCtx.ShutdownEvent, NotificationEvent, FALSE );

	LFSNtcCtx.LfsTable = LfsTable;
	*NtcCtx = &LFSNtcCtx;

	InitializeListHead( &LFSNtcCtx.SendPkts );

	status = PsCreateSystemThread( &LFSNtcCtx.hThread,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   RedirDataGramNotifierThreadProc,
								   &LFSNtcCtx );

	if (!NT_SUCCESS(status)) {

		NDAS_ASSERT( FALSE );
		return FALSE;
	}

	return TRUE;
}


BOOLEAN
RdsvrDatagramDestroy (
	VOID
	) 
{
	VOID			*threadObject;	
	LARGE_INTEGER	timeOut;
	NTSTATUS		status;

	ASSERT( KeGetCurrentIrql() <= PASSIVE_LEVEL );

	//	shutdown Datagram Server

	KeSetEvent( &LFSDGSvrCtx.ShutdownEvent, IO_NO_INCREMENT, FALSE );

	status = ObReferenceObjectByHandle( LFSDGSvrCtx.hSvrThread,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&threadObject,
										NULL );

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );
		goto next_step;
	}

	timeOut.QuadPart = - 20 * NANO100_PER_SEC;		// 20 sec
	
	status = KeWaitForSingleObject( threadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	ObDereferenceObject( threadObject );

next_step:

	//	shutdown Datagram Notifier

	KeSetEvent( &LFSNtcCtx.ShutdownEvent, IO_NO_INCREMENT, FALSE );

	status = ObReferenceObjectByHandle( LFSNtcCtx.hThread,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&threadObject,
										NULL );

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );
		goto out;
	}

	timeOut.QuadPart = - 20 * 10000;		// 20 sec
	
	status = KeWaitForSingleObject( threadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	ObDereferenceObject( threadObject );

	SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO, 
				   ("[LFS] RdsvrDatagramDestroy: Redirection datagram server shut down successfully.\n") );

out:

	return TRUE;
}
