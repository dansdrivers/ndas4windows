#include "LfsProc.h"

#include <ntifs.h>
#include <stdlib.h>
#include <tdikrnl.h>
#include "filespy.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "fspyKern.h"
#include "lfs.h"
#include "LfsTable.h"
#include "NdftProtocolHeader.h"




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
	) {
	LONG	idx_ori, idx_updated, idx_disabled, idx_enabled ;
	BOOLEAN	found ;
	UINT32	matchmask ;

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
       LONG             AgeCount; // Force refreshing of open socket. This is temporary fix.
       
       SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] NetEventDetectorProc: Initializing Networking event detector...\n")) ;
	//
	//	get the address list
	//
	ntStatus = LpxTdiGetAddressList(
			&NetEvtCtx->AddressList
		);
	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEventDetectorProc: LpxTdiGetAddressList() failed. NTSTATUS:%lu\n", ntStatus));
//		goto termination ;
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] NetEventDetectorProc: Networking event detector started...\n")) ;

        AgeCount = 0;
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
		SPY_LOG_PRINT( LFS_DEBUG_TABLE_NOISE, ( "[LFS] NetEventDetectorProc: NTSTATUS:%lu\n", ntStatus));

		if(0 == ntStatus) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEventDetectorProc: ShutDown event received. NTSTATUS:%lu\n", ntStatus));
			break ;
		} else if(STATUS_TIMEOUT == ntStatus) {
			//
			//	Time Out.
			//
			SPY_LOG_PRINT( LFS_DEBUG_TABLE_NOISE, ( "[LFS] NetEventDetectorProc: Time out. Go ahead and check changes. NTSTATUS:%lu\n", ntStatus));
			AgeCount++;
		} else {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEventDetectorProc: KeWaitForSingleObject() failed. NTSTATUS:%lu\n", ntStatus));
			break ;
		}

		//
		//	check changes
		//
		ntStatus = LpxTdiGetAddressList(
				&addressList
			);
		if(!NT_SUCCESS(ntStatus)) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEventDetectorProc: LpxTdiGetAddressList() failed. NTSTATUS:%lu\n", ntStatus));

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
			NetEvtCtx->EnabledAddressList.iAddressCount == 0 &&
			AgeCount <= 10
			) {
                    
			continue ;
		}

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEventDetectorProc: Networking event detected.\n"));

		AgeCount = 0;
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
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] NetEventDetectorProc: Callback #%d is NULL.\n", idx_callbacks)) ;
			}
		}

		RtlCopyMemory(&NetEvtCtx->AddressList, &addressList, sizeof(SOCKETLPX_ADDRESS_LIST)) ;
	}


//termination:
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] Networking event detector terminated.\n")) ;
	PsTerminateSystemThread(0) ;
}

BOOLEAN
NetEvtInit(PNETEVTCTX	NetEvtCtx) {
	NTSTATUS			ntStatus ;
	OBJECT_ATTRIBUTES	objectAttributes;

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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] couldn't start Redirection server\n")) ;
		return FALSE ;
	}

	return TRUE ;
}


BOOLEAN
NetEvtTerminate(PNETEVTCTX	NetEvtCtx) {
	VOID			*ThreadObject ;	
	LARGE_INTEGER	TimeOut ;
	NTSTATUS		ntStatus ;

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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEvtTerminate: referencing to the thread object failed\n")) ;
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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEvtTerminate: waiting for the thread failed\n")) ;
	}

	ObDereferenceObject(ThreadObject) ;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NetEvtTerminate: Shut down successfully.\n")) ;

out:

	return TRUE ;
}

///////////////////////////////////////////////////////////////////////////
//
//	Datagram routines
//
BOOLEAN
LfsAllocDGPkt(
		PLFSDG_PKT	*ppkt,
		UINT32		dataSz
) {
	PNDFT_HEADER	rawHead ;
	ULONG			lfsDGPktSz ;
	static			UCHAR	Protocol[4] = LFS_DATAGRAM_PROTOCOL ;

	lfsDGPktSz = ( sizeof(LFSDG_PKT) - sizeof(LFSDG_RAWPKT_DATA) )	// context + header
				+ dataSz ;											// body

	*ppkt = ExAllocatePoolWithTag(
		NonPagedPool,
		lfsDGPktSz,
		LFS_MEM_TAG_PACKET
    );
	if(NULL == *ppkt) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsAllocPacket: not enough memory.\n") );
		return FALSE ;
	}

	RtlZeroMemory(*ppkt, lfsDGPktSz) ;

	(*ppkt)->RefCnt		= 1 ;
	(*ppkt)->PacketSize = sizeof(NDFT_HEADER) + dataSz ;
	(*ppkt)->DataSize	= dataSz ;
	InitializeListHead(&(*ppkt)->PktListEntry) ;

	rawHead = &(*ppkt)->RawHeadDG ;

	RtlCopyMemory(rawHead->Protocol, Protocol, 4) ;

	rawHead->NdfsMajorVersion		= LFS_DATAGRAM_MAJVER ;
	rawHead->NdfsMinorVersion		= LFS_DATAGRAM_MINVER ;
	rawHead->OsMajorType			= OSTYPE_WINDOWS ;
	rawHead->Type					= NDFT_PRIMARY_UPDATE_MESSAGE ;

	if(IS_WINDOWS2K())
		rawHead->OsMinorType	= OSTYPE_WIN2K ;
	else if(IS_WINDOWSXP())
		rawHead->OsMinorType	= OSTYPE_WINXP ;
	else if(IS_WINDOWSNET())
		rawHead->OsMinorType	= OSTYPE_WIN2003SERV ;
	else {	// default setting
		rawHead->OsMinorType	= OSTYPE_WIN2K ;
	}

	rawHead->MessageSize		= sizeof(NDFT_HEADER) + dataSz ;

	return TRUE ;
}


VOID
LfsReferenceDGPkt(PLFSDG_PKT pkt) {
	LONG	result ;

	result = InterlockedIncrement(&pkt->RefCnt) ;

	ASSERT( result > 0) ;
}


VOID
LfsDereferenceDGPkt(PLFSDG_PKT pkt) {
	LONG	result ;

	result = InterlockedDecrement(&pkt->RefCnt) ;

	SPY_LOG_PRINT( LFS_DEBUG_TABLE_NOISE, ( "[LFS] LfsDereferenceDGPkt: A packet RefCnt:%d\n", result) );

	ASSERT( result >= 0) ;
	if(0 == result)
		LfsFreeDGPkt(pkt) ;
}

//
//
//
BOOLEAN
LfsFreeDGPkt(PLFSDG_PKT pkt) {
	ExFreePool(pkt) ;
	SPY_LOG_PRINT( LFS_DEBUG_TABLE_NOISE, ( "[LFS] LfsFreeDGPkt: A packet freed:%p\n", pkt) );
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
    HANDLE					addressFileHandle = NULL;
	HANDLE					connectionFileHandle = NULL;
    PFILE_OBJECT			addressFileObject = NULL;
	PFILE_OBJECT			connectionFileObject = NULL;
	NTSTATUS				ntStatus ;
	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	LONG					idx_addr ;
	ULONG					open_addr ;

	LPX_ADDRESS				NICAddr ;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsOpenDGSocket: Entered\n"));

	Socket->SocketCnt = 0 ;

	//
	//	get addresses from LPX
	//
	ntStatus = LpxTdiGetAddressList(
		&socketLpxAddressList
    ) ;
	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsOpenDGSocket: LpxTdiGetAddressList() failed.\n")) ;
		return ntStatus;
	}
	if(0 == socketLpxAddressList.iAddressCount) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsOpenDGSocket: No NICs in the host.\n") );
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

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsOpenDGSocket: We don't use SocketLpx device.\n")) ;
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

			if (ntStatus != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
				ASSERT(LFS_LPX_BUG);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsOpenDGSocket: couldn't open a address %d:'%02X:%02X:%02X:%02X:%02X:%02X/%d'\n",
						idx_addr,
						NICAddr.Node[0],NICAddr.Node[1],NICAddr.Node[2],
						NICAddr.Node[3],NICAddr.Node[4],NICAddr.Node[5],
						(int)NTOHS(NICAddr.Port) ) );
			continue ;
		}


		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsOpenDGSocket: opened a address:'%02X:%02X:%02X:%02X:%02X:%02X/%d'\n",
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

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsCloseDGSocket: Entered\n"));

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
}


//
//	send a packet.
//
NTSTATUS
LfsSendDatagram(
	IN PLFSDG_Socket	socket,
	IN PLFSDG_PKT		pkt,
	PLPX_ADDRESS		LpxRemoteAddress

) {
	NTSTATUS	ntStatus ;
	ULONG		result ;
	ULONG		idx_addr ;

	//
	//	send a packet
	//
	for(idx_addr = 0 ; idx_addr < socket->SocketCnt ; idx_addr ++ ) {
		ntStatus = LpxTdiSendDataGram(
				socket->Sockets[idx_addr].AddressFile,
				LpxRemoteAddress,
				(PUCHAR)&pkt->RawHeadDG,
				pkt->PacketSize,
				0,
				&result
			) ;
		if(STATUS_PENDING == ntStatus) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsSendDatagram: outgoing packet is pending.\n")) ;
		} else if(!NT_SUCCESS(ntStatus)) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsSendDatagram: sending failed.\n")) ;
		} else if(result != pkt->PacketSize) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsSendDatagram: unexpected data length sent. len:%lu\n", result)) ;
		}
		//
		//	patch02172004
		//	Some NICs don't support loop-back for broadcast packet.
		//	set Source address here.
		if(NT_SUCCESS(ntStatus)) {
			RtlCopyMemory(&pkt->SourceAddr, &socket->Sockets[idx_addr].NICAddr, sizeof(LPX_ADDRESS)) ;
		}


	}

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

	ntStatus = STATUS_SUCCESS ;

	for(idx_addr = 0 ; idx_addr < DatagramSocket->SocketCnt ; idx_addr ++) {

		ntStatus = LpxTdiSetReceiveDatagramHandler(
				DatagramSocket->Sockets[idx_addr].AddressFile,
				EventHandler,
				EventContext
			) ;
		if(!NT_SUCCESS(ntStatus)) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsRegisterDatagramRecvHandler: LpxTdiSetReceiveDatagramHandler() failed.\n")) ;
		}
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsRegisterDatagramRecvHandler: left...\n") );

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
	
	
	SPY_LOG_PRINT( LFS_DEBUG_TABLE_NOISE, ( "[LFS] LfsIsFromLocal: Entered with Addr:%02x:%02x:%02x:%02x:%02x:%02x\n",
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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsIsFromLocal: LpxTdiGetAddressList() failed.\n")) ;
		return FALSE ;
	}
	if(0 == socketLpxAddressList.iAddressCount) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsIsFromLocal: No NICs in the host.\n")) ;
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

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] LfsIsFromLocal: We don't use SocketLpx device.\n") );
			continue ;

		}

		if( RtlCompareMemory(Addr->Node, socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node, ETHER_ADDR_LENGTH)
			== ETHER_ADDR_LENGTH ) {
				SPY_LOG_PRINT( LFS_DEBUG_TABLE_NOISE, ( "[LFS] LfsIsFromLocal: found a address matching.\n")) ;
				return TRUE ;
		}
	}


	return FALSE ;
}


#if DBG

VOID
DbgPrintLpxAddrList(
		ULONG					DbgMask,
		PSOCKETLPX_ADDRESS_LIST	AddrList
) {
	LONG	idx_addr ;

	UNREFERENCED_PARAMETER(DbgMask);

	for(idx_addr = 0 ; idx_addr < AddrList->iAddressCount ; idx_addr ++ ) {
		SPY_LOG_PRINT( DbgMask,
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
}

#endif