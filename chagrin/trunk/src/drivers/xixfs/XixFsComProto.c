
#include <ntifs.h>
#include <stdlib.h>
#include <tdikrnl.h>

#include "XixFsType.h"
#include "XixFsDebug.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "XixFsComProto.h"




//////////////////////////////////////////////////////////////
//
//	statistics
//
LONG	PacketAlloc ;
LONG	PacketFreed ;
LONG	PacketExported ;
LONG	PacketRefs ;
LONG	PacketSent ;
LONG	PacketSentAcked ;

///////////////////////////////////////////////////////////////////////////
//
//	Detecting networking event
//
static VOID
FindOutChanges(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled
);


static VOID
NetEventDetectorProc(
		PVOID lpParameter   // thread data
	);

//#pragma alloc_text(PAGE, LfsAllocDGPkt)
//#pragma alloc_text(PAGE, LfsReferenceDGPkt)
//#pragma alloc_text(PAGE, LfsDereferenceDGPkt)
//#pragma alloc_text(PAGE, LfsFreeDGPkt)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FindOutChanges)
#pragma alloc_text(PAGE, NetEventDetectorProc)
#pragma alloc_text(PAGE, NetEvtInit)
#pragma alloc_text(PAGE, NetEvtTerminate)
#pragma alloc_text(PAGE, LfsOpenDGSocket)
#pragma alloc_text(PAGE, LfsCloseDGSocket)
#pragma alloc_text(PAGE, LfsSendDatagram)
#pragma alloc_text(PAGE, LfsRegisterDatagramRecvHandler)
#pragma alloc_text(PAGE, LfsIsFromLocal)
#endif



static VOID
FindOutChanges(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled
	) {
	LONG	idx_ori, idx_updated, idx_disabled, idx_enabled ;
	BOOLEAN	found ;
	UINT32	matchmask ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter FindOutChanges \n"));


	ASSERT(sizeof(matchmask) * 8 >= MAX_SOCKETLPX_INTERFACE) ;

	idx_disabled  = 0 ;
	Disabled->iAddressCount = 0 ;
	matchmask = 0 ;
	for(idx_ori = 0 ; idx_ori < Original->iAddressCount ; idx_ori ++ ) {

		found = FALSE ;

		//
		//	find disabled ones.
		//
		for(idx_updated = 0 ; idx_updated < Updated->iAddressCount ; idx_updated ++) {
			
			if( RtlCompareMemory(
						Original->SocketLpx[idx_ori].LpxAddress.Node,
						Updated->SocketLpx[idx_updated].LpxAddress.Node,
						ETHER_ADDR_LENGTH
						) == ETHER_ADDR_LENGTH ) {

				//
				//	check this match in the bit mask.
				//	help find enabled ones.
				//
				matchmask |= 1 << idx_updated ;

				found = TRUE ;
				break ;
			}
			
		}

		//
		//	add disabled one to the list
		//
		if(!found) {
			RtlCopyMemory(Disabled->SocketLpx[idx_disabled].LpxAddress.Node,
							Original->SocketLpx[idx_ori].LpxAddress.Node,
							ETHER_ADDR_LENGTH
						) ;

			Disabled->iAddressCount ++ ;
			idx_disabled ++ ;
		}
	}

	//
	//	find enabled ones.
	//
	idx_enabled = 0 ;
	Enabled->iAddressCount = 0 ;
	for(idx_updated = 0 ; idx_updated < Updated->iAddressCount ; idx_updated ++) {
		//
		//	add enabled one to the list.
		//
		if(!(matchmask & (1 << idx_updated))) {
			RtlCopyMemory( 
					Enabled->SocketLpx[idx_enabled].LpxAddress.Node,
					Updated->SocketLpx[idx_updated].LpxAddress.Node,
					ETHER_ADDR_LENGTH
				) ;

			Enabled->iAddressCount ++ ;
			idx_enabled ++ ;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit FindOutChanges \n"));
}

static VOID
NetEventDetectorProc(
		PVOID lpParameter   // thread data
	) {
	PNETEVTCTX	NetEvtCtx  = (PNETEVTCTX)lpParameter;
	NTSTATUS	ntStatus ;
	SOCKETLPX_ADDRESS_LIST	addressList ;
	LARGE_INTEGER	TimeOut ;
	LONG			idx_callbacks ;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter NetEventDetectorProc \n"));


	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("[LFS] NetEventDetectorProc: Initializing Networking event detector...\n")) ;
	//
	//	get the address list
	//
	ntStatus = LpxTdiGetAddressList(
			&NetEvtCtx->AddressList
		);
	if(!NT_SUCCESS(ntStatus)) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			( "[LFS] NetEventDetectorProc: LpxTdiGetAddressList() failed. NTSTATUS:%lu\n", ntStatus));
//		goto termination ;
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
		("[LFS] NetEventDetectorProc: Networking event detector started...\n")) ;

	// Main loop...
	while(1) {
		TimeOut.QuadPart = - NETEVT_FREQ ;
		ntStatus = KeWaitForSingleObject(
				&NetEvtCtx->ShutdownEvent,
				Executive,
				KernelMode,
				FALSE,
				&TimeOut
			);
		
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
			( "[LFS] NetEventDetectorProc: NTSTATUS:%lu\n", ntStatus));

		if(0 == ntStatus) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				( "[COM] NetEventDetectorProc: ShutDown event received. NTSTATUS:%lu\n", ntStatus));
			break ;
		} else if(STATUS_TIMEOUT == ntStatus) {
			//
			//	Time Out.
			//
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
				( "[COM] NetEventDetectorProc: Time out. Go ahead and check changes. NTSTATUS:%lu\n", ntStatus));
		} else {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				( "[COM] NetEventDetectorProc: KeWaitForSingleObject() failed. NTSTATUS:%lu\n", ntStatus));
			break ;
		}

		//
		//	check changes
		//
		ntStatus = LpxTdiGetAddressList(
				&addressList
			);
		if(!NT_SUCCESS(ntStatus)) {
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				( "[LFS] NetEventDetectorProc: LpxTdiGetAddressList() failed. NTSTATUS:%lu\n", ntStatus));

			continue ;
		}

		FindOutChanges(
				&NetEvtCtx->AddressList,
				&addressList,
				&NetEvtCtx->DisabledAddressList,
				&NetEvtCtx->EnabledAddressList
			);

		//
		//	call back
		//
		if(
			NetEvtCtx->DisabledAddressList.iAddressCount == 0 &&
			NetEvtCtx->EnabledAddressList.iAddressCount == 0
			) {

			continue ;
		}

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
			( "[LFS] NetEventDetectorProc: Networking event detected.\n"));

		//
		//	call callbacks and update address list.
		//
		for(idx_callbacks = 0 ; idx_callbacks < NetEvtCtx->CallbackCnt ; idx_callbacks ++ ) {

			if(NetEvtCtx->Callbacks[idx_callbacks]) {
				NetEvtCtx->Callbacks[idx_callbacks](
						&NetEvtCtx->AddressList,
						&addressList,
						&NetEvtCtx->DisabledAddressList,
						&NetEvtCtx->EnabledAddressList,
						NetEvtCtx->CallbackContext[idx_callbacks]
					) ;
			} else {
				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
					("[LFS] NetEventDetectorProc: Callback #%d is NULL.\n", idx_callbacks)) ;
			}
		}

		RtlCopyMemory(&NetEvtCtx->AddressList, &addressList, sizeof(SOCKETLPX_ADDRESS_LIST)) ;
	}


//termination:
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
		("[LFS] Networking event detector terminated.\n")) ;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit NetEventDetectorProc \n"));
	PsTerminateSystemThread(0) ;
}

BOOLEAN
NetEvtInit(PNETEVTCTX	NetEvtCtx) {
	NTSTATUS			ntStatus ;
	OBJECT_ATTRIBUTES	objectAttributes;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter NetEvtInit \n"));

	//
	//	create the datagram server thread
	//
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	KeInitializeEvent(
		&NetEvtCtx->ShutdownEvent,
		NotificationEvent,
		FALSE
		) ;

	ntStatus = PsCreateSystemThread(
					&NetEvtCtx->HThread,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					NetEventDetectorProc,
					NetEvtCtx
	);
	if(!NT_SUCCESS(ntStatus)) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("[LFS] couldn't start Redirection server\n")) ;
		return FALSE ;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit NetEvtInit \n"));
	return TRUE ;
}


BOOLEAN
NetEvtTerminate(PNETEVTCTX	NetEvtCtx) {
	VOID			*ThreadObject ;	
	LARGE_INTEGER	TimeOut ;
	NTSTATUS		ntStatus ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit NetEvtTerminate \n"));

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL) ;

	KeSetEvent(&NetEvtCtx->ShutdownEvent, IO_NO_INCREMENT, FALSE) ;
	ntStatus = ObReferenceObjectByHandle(
			NetEvtCtx->HThread,
			FILE_READ_DATA,
			NULL,
			KernelMode,
			&ThreadObject,
			NULL
		) ;
	if(!NT_SUCCESS(ntStatus)) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			( "[LFS] NetEvtTerminate: referencing to the thread object failed\n")) ;
		goto out ;
	}

	TimeOut.QuadPart = - 20 * 10000 ;		// 20 sec
	ntStatus = KeWaitForSingleObject(
			ThreadObject,
			Executive,
			KernelMode,
			FALSE,
			&TimeOut
		) ;
	if(!NT_SUCCESS(ntStatus)) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			( "[LFS] NetEvtTerminate: waiting for the thread failed\n")) ;
	}

	ObDereferenceObject(ThreadObject) ;

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
		( "[LFS] NetEvtTerminate: Shut down successfully.\n")) ;


out:
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit NetEvtTerminate \n"));

	return TRUE ;
}

///////////////////////////////////////////////////////////////////////////
//
//	Datagram routines
//
BOOLEAN
LfsAllocDGPkt(
		PXIFSDG_PKT	*ppkt,
		uint8 		*SrcMac,
		uint8		*DstMac,
		uint32		Type
) {
	PXIFS_COMM_HEADER	rawHead ;
	ULONG			lfsDGPktSz ;
	static			UCHAR	Protocol[4] = XIFS_DATAGRAM_PROTOCOL ;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter LfsAllocDGPkt \n"));

	lfsDGPktSz = sizeof(XIFSDG_PKT);	// context + header
														// body

	*ppkt = ExAllocatePoolWithTag(
		NonPagedPool,
		lfsDGPktSz,
		XIFS_MEM_TAG_PACKET
    );
	if(NULL == *ppkt) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			( "[LFS] LfsAllocPacket: not enough memory.\n") );
		return FALSE ;
	}

	RtlZeroMemory(*ppkt, lfsDGPktSz) ;

	(*ppkt)->RefCnt		= 1 ;
	(*ppkt)->PacketSize = sizeof(XIFS_COMM_HEADER) + sizeof(XIFSDG_RAWPK_DATA) ;
	(*ppkt)->DataSize	= sizeof(XIFSDG_RAWPK_DATA);
	InitializeListHead(&(*ppkt)->PktListEntry) ;

	rawHead = &(*ppkt)->RawHeadDG ;

	RtlCopyMemory(rawHead->Protocol, Protocol, 4) ;
	// Changed by ILGU HONG
	//	chesung suggest
	//if(DstMac)RtlCopyMemory(rawHead->DstMac, DstMac, 6);
	//if(SrcMac)RtlCopyMemory(rawHead->SrcMac, SrcMac, 6);
	if(DstMac)RtlCopyMemory(rawHead->DstMac, DstMac, 32);
	if(SrcMac)RtlCopyMemory(rawHead->SrcMac, SrcMac, 32);
	rawHead->Type = HTONL(Type);
	rawHead->XifsMajorVersion = HTONL(XIFS_PROTO_MAJOR_VERSION)	;
	rawHead->XifsMinorVersion = HTONL(XIFS_PROTO_MINOR_VERSION)	;
	rawHead->MessageSize		= HTONL(sizeof(XIFS_COMM_HEADER) + sizeof(XIFSDG_RAWPK_DATA)) ;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit LfsAllocDGPkt \n"));

	return TRUE ;
}


VOID
LfsReferenceDGPkt(PXIFSDG_PKT pkt) {
	LONG	result ;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter LfsReferenceDGPkt \n"));

	result = InterlockedIncrement(&pkt->RefCnt) ;

	ASSERT( result > 0) ;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit LfsReferenceDGPkt \n"));
}


VOID
LfsDereferenceDGPkt(PXIFSDG_PKT pkt) {
	LONG	result ;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter LfsDereferenceDGPkt \n"));

	result = InterlockedDecrement(&pkt->RefCnt) ;

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( "[LFS] LfsDereferenceDGPkt: A packet RefCnt:%d\n", result) );

	ASSERT( result >= 0) ;
	if(0 == result)
		LfsFreeDGPkt(pkt) ;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit LfsDereferenceDGPkt \n"));
}

//
//
//
BOOLEAN
LfsFreeDGPkt(PXIFSDG_PKT pkt) {
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter LfsFreeDGPkt \n"));

	ExFreePool(pkt) ;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit LfsFreeDGPkt \n"));
	return TRUE ;
}

//////////////////////////////////////////////////////////////
//
//	connection management
//



//
//	open a datagram socket
//
NTSTATUS
LfsOpenDGSocket(
	OUT PLFSDG_Socket		Socket,
	IN USHORT				PortNo
) {
	HANDLE						addressFileHandle = NULL;
	HANDLE						connectionFileHandle = NULL;
	PFILE_OBJECT					addressFileObject = NULL;
	PFILE_OBJECT					connectionFileObject = NULL;
	NTSTATUS					ntStatus ;
	SOCKETLPX_ADDRESS_LIST		socketLpxAddressList;
	LONG						idx_addr ;
	ULONG						open_addr ;

	LPX_ADDRESS				NICAddr ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter  LfsOpenDGSocket \n"));
	

	Socket->SocketCnt = 0 ;

	//
	//	get addresses from LPX
	//
	ntStatus = LpxTdiGetAddressList(
				&socketLpxAddressList
   				 ) ;
	
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}
	if(0 == socketLpxAddressList.iAddressCount) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	open a port for each NIC.
	//
	open_addr = 0 ;

	for(idx_addr = 0 ; idx_addr < socketLpxAddressList.iAddressCount; idx_addr ++) {

		if( (0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[5]) ) {
			continue ;
		}

		RtlCopyMemory(&NICAddr, &socketLpxAddressList.SocketLpx[idx_addr].LpxAddress, sizeof(LPX_ADDRESS)) ;
		NICAddr.Port = HTONS(PortNo) ;

		//
		//	open a connection and address.
		//	if this calling for a datagram server, don't create a connection.
		//
		ntStatus = LpxTdiOpenAddress(
				&addressFileHandle,
				&addressFileObject,
				&NICAddr
			);
		if(!NT_SUCCESS(ntStatus)) {
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				( "[LFS] LfsOpenDGSocket: couldn't open a address %d:'%02X:%02X:%02X:%02X:%02X:%02X/%d'\n",
						idx_addr,
						NICAddr.Node[0],NICAddr.Node[1],NICAddr.Node[2],
						NICAddr.Node[3],NICAddr.Node[4],NICAddr.Node[5],
						(int)NTOHS(NICAddr.Port) ) );
			continue ;
		}


		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
			( "[LFS] LfsOpenDGSocket: opened a address:'%02X:%02X:%02X:%02X:%02X:%02X/%d'\n",
					NICAddr.Node[0],NICAddr.Node[1],NICAddr.Node[2],
					NICAddr.Node[3],NICAddr.Node[4],NICAddr.Node[5],
					(int)NTOHS(NICAddr.Port)
			) );
		//
		//	return values
		//	close handles, but leave objects
		//
		Socket->Sockets[open_addr].AddressFile = addressFileObject ;
		Socket->Sockets[open_addr].AddressFileHandle = addressFileHandle ;
		RtlCopyMemory(&Socket->Sockets[open_addr].NICAddr, &NICAddr, sizeof(LPX_ADDRESS)) ;

		open_addr ++ ;


	}

	Socket->SocketCnt = (USHORT)open_addr ;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit  LfsOpenDGSocket \n"));
	return ntStatus ;
}



//
//	close a datagram socket
//
VOID
LfsCloseDGSocket(
	IN PLFSDG_Socket	Socket
) {
	LONG					idx_addr ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter  LfsCloseDGSocket \n"));
	

	ASSERT(Socket) ;

	for(idx_addr = 0 ; idx_addr < Socket->SocketCnt; idx_addr ++) {

		if(Socket->Sockets[idx_addr].AddressFile) {

			ObDereferenceObject(Socket->Sockets[idx_addr].AddressFile) ;
			ZwClose(Socket->Sockets[idx_addr].AddressFileHandle) ;

		}
	}

	//
	// reset socket count
	//
	Socket->SocketCnt = 0 ;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit  LfsCloseDGSocket \n"));
}


//
//	send a packet.
//
NTSTATUS
LfsSendDatagram(
	IN PLFSDG_Socket		socket,
	IN PXIFSDG_PKT			pkt,
	PLPX_ADDRESS		LpxRemoteAddress

) {
	NTSTATUS	ntStatus ;
	ULONG		result ;
	ULONG		idx_addr ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter  LfsSendDatagram \n"));

	//
	//	send a packet
	//
	for(idx_addr = 0 ; idx_addr < socket->SocketCnt ; idx_addr ++ ) {

		RtlCopyMemory(&pkt->SourceAddr, &socket->Sockets[idx_addr].NICAddr, sizeof(LPX_ADDRESS)) ;
		

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
			( "[LFS] LfsSendDatagram: Souce address: [0x%02X:%02X:%02X:%02X:%02X:%02X] : Port (%ld)\n",
					pkt->SourceAddr.Node[0],pkt->SourceAddr.Node[1],pkt->SourceAddr.Node[2],
					pkt->SourceAddr.Node[3],pkt->SourceAddr.Node[4],pkt->SourceAddr.Node[5], NTOHS(pkt->SourceAddr.Port)) );


		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
			( "[LFS] LfsSendDatagram: Dest address: [0x%02X:%02X:%02X:%02X:%02X:%02X] : Port (%ld)\n",
					LpxRemoteAddress->Node[0],LpxRemoteAddress->Node[1],LpxRemoteAddress->Node[2],
					LpxRemoteAddress->Node[3],LpxRemoteAddress->Node[4],LpxRemoteAddress->Node[5], NTOHS(LpxRemoteAddress->Port) ));



		ntStatus = LpxTdiSendDataGram(
				socket->Sockets[idx_addr].AddressFile,
				LpxRemoteAddress,
				(PUCHAR)&pkt->RawHeadDG,
				pkt->PacketSize,
				0,
				&result
			) ;
		if(STATUS_PENDING == ntStatus) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				( "[COM] LfsSendDatagram: outgoing packet is pending.\n")) ;
		} else if(!NT_SUCCESS(ntStatus)) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				( "[COM] LfsSendDatagram: sending failed.\n")) ;
		} else if(result != pkt->PacketSize) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				( "[COM] LfsSendDatagram: unexpected data length sent. len:%lu\n", result)) ;
		}
		//
		//	patch02172004
		//	Some NICs don't support loop-back for broadcast packet.
		//	set Source address here.
		//if(NT_SUCCESS(ntStatus)) {
			
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				( "[COM] LfsSendDatagram: Sending OK. \n")) ;

		//	RtlCopyMemory(&pkt->SourceAddr, &socket->Sockets[idx_addr].NICAddr, sizeof(LPX_ADDRESS)) ;
		//}


	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit  LfsSendDatagram \n"));
	return STATUS_SUCCESS ;
}




NTSTATUS
LfsRegisterDatagramRecvHandler(
		IN	PLFSDG_Socket	DatagramSocket,
		IN	PVOID			EventHandler,
		IN	PVOID			EventContext
   ) {
	NTSTATUS	ntStatus ;
	LONG		idx_addr ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
		("Enter  LfsRegisterDatagramRecvHandler \n"));


	ntStatus = STATUS_SUCCESS ;

	for(idx_addr = 0 ; idx_addr < DatagramSocket->SocketCnt ; idx_addr ++) {

		ntStatus = LpxTdiSetReceiveDatagramHandler(
				DatagramSocket->Sockets[idx_addr].AddressFile,
				EventHandler,
				EventContext
			) ;
		if(!NT_SUCCESS(ntStatus)) {
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsRegisterDatagramRecvHandler: LpxTdiSetReceiveDatagramHandler() failed.\n")) ;
		}
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
		("Exit  LfsRegisterDatagramRecvHandler \n"));

	return ntStatus ;

}


//
//	compare addresses to see if the address is local.
//
BOOLEAN
LfsIsFromLocal(
		PLPX_ADDRESS	Addr
	) {
	NTSTATUS				ntStatus;

	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	LONG					idx_addr ;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter  LfsIsFromLocal \n"));


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( "[LFS] LfsIsFromLocal: Entered with Addr:%02x:%02x:%02x:%02x:%02x:%02x\n",
				Addr->Node[0], Addr->Node[1], Addr->Node[2], 
				Addr->Node[3], Addr->Node[4], Addr->Node[5]
			));

	//
	//	get addresses from LPX
	//
	socketLpxAddressList.iAddressCount = 0 ;
	ntStatus = LpxTdiGetAddressList(
		&socketLpxAddressList
    	) ;
	
	if(!NT_SUCCESS(ntStatus)) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsIsFromLocal: LpxTdiGetAddressList() failed.\n")) ;
		return FALSE ;
	}
	if(0 == socketLpxAddressList.iAddressCount) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ( "[LFS] LfsIsFromLocal: No NICs in the host.\n")) ;
		return FALSE ;
	}

	for(idx_addr = 0 ; idx_addr < socketLpxAddressList.iAddressCount ; idx_addr ++ ) {
		//
		//	BUG FIX for LPX: skip SocketLpxDevice
		//
		if( (0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[5]) ) {

			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,  ( "[LFS] LfsIsFromLocal: We don't use SocketLpx device.\n") );
			continue ;

		}

		if( RtlCompareMemory(Addr->Node, socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node, ETHER_ADDR_LENGTH)
			== ETHER_ADDR_LENGTH ) {
				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( "[LFS] LfsIsFromLocal: found a address matching.\n")) ;
				return TRUE ;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit  LfsIsFromLocal \n"));

	return FALSE ;
}


#if DBG

VOID
DbgPrintLpxAddrList(
		ULONG					DbgMask,
		PSOCKETLPX_ADDRESS_LIST	AddrList
) {
	LONG	idx_addr ;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter  DbgPrintLpxAddrList \n"));

	for(idx_addr = 0 ; idx_addr < AddrList->iAddressCount ; idx_addr ++ ) {
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				("%d. %02x:%02x:%02x:%02x:%02x:%02x/%d\n", 
					idx_addr,
					AddrList->SocketLpx[idx_addr].LpxAddress.Node[0],
					AddrList->SocketLpx[idx_addr].LpxAddress.Node[1],
					AddrList->SocketLpx[idx_addr].LpxAddress.Node[2],
					AddrList->SocketLpx[idx_addr].LpxAddress.Node[3],
					AddrList->SocketLpx[idx_addr].LpxAddress.Node[4],
					AddrList->SocketLpx[idx_addr].LpxAddress.Node[5],
					HTONS(AddrList->SocketLpx[idx_addr].LpxAddress.Port)
			) ) ;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit  DbgPrintLpxAddrList \n"));
}

#endif