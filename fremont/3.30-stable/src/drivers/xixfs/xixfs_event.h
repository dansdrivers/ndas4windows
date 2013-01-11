#ifndef __XIXFS_EVENT_H__
#define __XIXFS_EVENT_H__

#include "xixfs_types.h"
#include "SocketLpx.h"

// Little Endian

#include <pshpack8.h>
#pragma warning(error: 4820)





///////////////////////////////////////////////////////////////////////////
//
//
//
//			XIFS DEFINDED VALUE
//
//
//
//




#define XIXFS_MEM_TAG_PACKET		'tpix'
#define XIXFS_MEM_TAG_SOCKET		'tsix'

#define	DEFAULT_XIXFS_CLIWAIT				( 1 * TIMERUNIT_SEC)	// 1 seconds.
#define DEFAULT_REQUEST_MAX_TIMEOUT			(5 * TIMERUNIT_SEC)
#define MAX_XI_DATAGRAM_DATA_SIZE			(1024 - sizeof(XIXFS_COMM_HEADER))
///////////////////////////////////////////////////////////////////////////
//
// define the packet format using datagramtype LPX
//
//
//	head part of a LFS datagram packet
//



typedef union _XIXFSDG_RAWPK_DATA {
		XIXFS_LOCK_REQUEST						LockReq;
		XIXFS_LOCK_REPLY							LockReply;
		XIXFS_LOCK_BROADCAST					LockBroadcast;
		XIXFS_RANGE_FLUSH_BROADCAST			FlushReq;
		XIXFS_DIR_ENTRY_CHANGE_BROADCAST		DirChangeReq;
		XIXFS_FILE_LENGTH_CHANGE_BROADCAST		FileLenChangeReq;
		XIXFS_FILE_CHANGE_BROADCAST				FileChangeReq;
		XIXFS_FILE_RANGE_LOCK_BROADCAST		FileRangeLockReq;
}XIXFSDG_RAWPK_DATA, *PXIXFSDG_RAWPK_DATA;

C_ASSERT(sizeof(XIXFSDG_RAWPK_DATA) == 128);


#pragma warning(default: 4820)
#include <poppack.h>

typedef struct _XIXFSDG_PKT{
	//
	//	raw packet
	//	Do not insert any code between RawHeadDG and RawDataDG
	//	We assume that RawHead and RawData are continuos memory area.
	//
	//	do not insert field befoure ...
	XIXFS_COMM_HEADER			RawHeadDG ;
	XIXFSDG_RAWPK_DATA			RawDataDG ;	
	//  packed data
	

	LONG				RefCnt ;				// reference count
	ULONG				Flags ;			
	PXIFS_LOCK_CONTROL	pLockContext;	
	PIO_WORKITEM		WorkQueueItem;
	LARGE_INTEGER		TimeOut;
	LIST_ENTRY			PktListEntry ;

	LPX_ADDRESS			SourceAddr ;
	ULONG				PacketSize ;
	ULONG				DataSize ;			// Data part size in packet.
	UINT32				Reserved ;
	//--> 40 bytes

}XIXFSDG_PKT, *PXIXFSDG_PKT;



//
//	Datagram
//
typedef struct _LFSDG_NICSocket {

	PFILE_OBJECT	AddressFile ;
    HANDLE			AddressFileHandle ;
	LPX_ADDRESS		NICAddr ;

} LFSDG_NICSocket, *PLFSDG_NICSocket ;

typedef struct _LFSDG_Socket {
	USHORT	SocketCnt ;
	USHORT	Port ;
	LFSDG_NICSocket Sockets[MAX_SOCKETLPX_INTERFACE] ;
} LFSDG_Socket, *PLFSDG_Socket ;

NTSTATUS
xixfs_OpenDGSocket(
		OUT PLFSDG_Socket	Socket,
		IN USHORT		PortNo
	) ;

VOID
xixfs_CloseDGSocket(
		IN PLFSDG_Socket	Socket
	) ;


NTSTATUS
xixfs_SendDatagram(
	IN PLFSDG_Socket		socket,
	IN PXIXFSDG_PKT			pkt,
	PLPX_ADDRESS		LpxRemoteAddress

);

BOOLEAN
xixfs_AllocDGPkt(
		PXIXFSDG_PKT	*ppkt,
		uint8 		*SrcMac,
		uint8		*DstMac,
		uint32		Type
);

VOID
xixfs_ReferenceDGPkt(
		PXIXFSDG_PKT pkt
	) ;

VOID
xixfs_DereferenceDGPkt(
		PXIXFSDG_PKT pkt
	) ;

BOOLEAN
xixfs_FreeDGPkt(
		PXIXFSDG_PKT pkt
	) ;

NTSTATUS
xixfs_RegisterDatagramRecvHandler(
		IN	PLFSDG_Socket	ServerDatagramSocket,
		IN	PVOID			EventHandler,
		IN	PVOID			EventContext
   ) ;

BOOLEAN
xixfs_IsFromLocal(
		PLPX_ADDRESS	NICAddr
	) ;

///////////////////////////////////////////////////////////////////
//
//	LFS exported variables
//


//
//	TODO: remove in the future
//
#define ETHER_ADDR_LENGTH 6


//////////////////////////////////////////////////////////////////////////
//
//	networking event detector.
//
#define NETEVT_MAX_CALLBACKS	4
#define NETEVT_FREQ		( 1 * TIMERUNIT_SEC)	// 1 seconds.

typedef VOID (*NETEVT_CALLBACK)(
				PSOCKETLPX_ADDRESS_LIST	Original,
				PSOCKETLPX_ADDRESS_LIST	Updated,
				PSOCKETLPX_ADDRESS_LIST	Disabled,
				PSOCKETLPX_ADDRESS_LIST	Enabled,
				PVOID					Context
		) ;

typedef struct _NETEVTCTX {
	KEVENT	ShutdownEvent ;
	HANDLE	HThread ;
	SOCKETLPX_ADDRESS_LIST	AddressList ;
	SOCKETLPX_ADDRESS_LIST	EnabledAddressList ;
	SOCKETLPX_ADDRESS_LIST	DisabledAddressList ;
	LONG					CallbackCnt ;
	NETEVT_CALLBACK			Callbacks[NETEVT_MAX_CALLBACKS] ;
	PVOID					CallbackContext[NETEVT_MAX_CALLBACKS] ;
} NETEVTCTX, *PNETEVTCTX ;

BOOLEAN
xixfs_NetEvtInit(PNETEVTCTX	NetEvtCtx) ;

BOOLEAN
xixfs_NetEvtTerminate(PNETEVTCTX	NetEvtCtx) ;

#if DBG

VOID
DbgPrintLpxAddrList(
		ULONG					DbgMask,
		PSOCKETLPX_ADDRESS_LIST	AddrList
) ;

#endif



#endif // __XIXFS_EVENT_H__