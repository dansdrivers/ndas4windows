#ifndef __NDFT_PROTOCOL_HEADER_H__
#define __NDFT_PROTOCOL_HEADER_H__


#define _U8		UCHAR
#define _U16	USHORT
#define _U32	ULONG
#define _U64	ULONGLONG


// Little Endian


#include <pshpack1.h>

#define	DEFAULT_NDFT_PORT				((USHORT)0x0003)
#define DEFAULT_NDFT_MAX_MESSAGE_SIZE	1024 // header is included



typedef struct _NDFT_HEADER
{
	_U8		Protocol[4];
	_U16	NdfsMajorVersion;
	_U16	NdfsMinorVersion;		// 8 bytes
	_U16	OsMajorType;
	_U16	OsMinorType;
	_U16	Type;
	_U16	Reserved;				// 16 bytes
	_U32	MessageSize;			// 20 bytes
									// Total Message Size (including Header Size)

} NDFT_HEADER, *PNDFT_HEADER;

#define NDSC_ID_LENGTH	16

typedef struct _NDFT_PRIMARY_UPDATE
{
	union
	{
		_U8		Type;
		struct 
		{
			_U8		AddDelete:1;	// Add = 0, Delete = 1
			_U8		Reserved:7;
		};
	};
	_U8		Reserved1;				// 2 bytes

	_U8		PrimaryNode[6];			// 8 bytes
	_U16	PrimaryPort;
	_U8		NetDiskNode[6];			// 16 bytes
	_U16	NetDiskPort;
	_U16	UnitDiskNo;				// 20 bytes
	_U8		NdscId[NDSC_ID_LENGTH];	// 36 bytes

} NDFT_PRIMARY_UPDATE, *PNDFT_PRIMARY_UPDATE;


#include <poppack.h>


#define	TIMERUNIT_SEC	(10 * 1000 * 1000)
#define	WAITUNIT		(TIMERUNIT_SEC / 10)	// 0.1 sec

#define LFS_MAX_BUFFER_SIZE	0x10000

#define	DEFAULT_SVRPORT					((USHORT)0x0001)
#define	DEFAULT_CBPORT					((USHORT)0x0002)
#define	DEFAULT_DATAGRAM_SVRPORT		((USHORT)0x0003)

//
//	memory pool tag
//
#define LFS_MEM_TAG_PACKET		'ptFL'
#define LFS_MEM_TAG_SOCKET		'stFL'

//
//
//
#define LFS_DATAGRAM_PROTOCOL	{ 'N', 'D', 'F', 'S' }

//
//	LFS current version
//
#define	LFS_DATAGRAM_MAJVER		0
#define	LFS_DATAGRAM_MINVER		2
//
// OS's types
//
#define OSTYPE_WINDOWS	0x0001
#define		OSTYPE_WIN98SE		0x0001
#define		OSTYPE_WIN2K		0x0002
#define		OSTYPE_WINXP		0x0003
#define		OSTYPE_WIN2003SERV	0x0004

#define	OSTYPE_LINUX	0x0002
#define		OSTYPE_LINUX22		0x0001
#define		OSTYPE_LINUX24		0x0002

#define	OSTYPE_MAC		0x0003
#define		OSTYPE_MACOS9		0x0001
#define		OSTYPE_MACOSX		0x0002

//
//	packet type masks
//
#define LFSPKTTYPE_PREFIX		0xF000
#define	LFSPKTTYPE_MAJTYPE		0x0FF0
#define	LFSPKTTYPE_MINTYPE		0x000F


//
//	packet type prefixes
//
#define	LFSPKTTYPE_REQUEST		0x1000	//	request packet
#define	LFSPKTTYPE_REPLY		0x2000	//	reply packet
#define	LFSPKTTYPE_DATAGRAM		0x4000	//	datagram packet
#define	LFSPKTTYPE_BROADCAST	0x8000	//	broadcasting packet

#define NDFT_PRIMARY_UPDATE_MESSAGE		0x0010

///////////////////////////////////////////////////////////////////////////
//
// define the packet format using datagramtype LPX
//
//
//	head part of a LFS datagram packet
//


#include <pshpack1.h>

#define MAX_DATAGRAM_DATA_SIZE	(1024 - sizeof(NDFT_HEADER))

typedef	union _LFSDG_RawPktData {

		UCHAR					Data[1] ;
		NDFT_PRIMARY_UPDATE		Owner ;

} LFSDG_RAWPKT_DATA, *PLFSDG_RAWPKT_DATA ;

typedef	struct _LFSDG_Pkt {
	LONG			RefCnt ;			// reference count
	ULONG			Flags ;
	LIST_ENTRY		PktListEntry ;

	LPX_ADDRESS		SourceAddr ;
	ULONG			PacketSize ;
	ULONG			DataSize ;			// Data part size in packet.

	PLFSDG_RAWPKT_DATA	pExtRawData ;	// support for external data part
	UINT32			Reserved ;
	//--> 40 bytes

	//
	//	raw packet
	//	Do not insert any code between RawHeadDG and RawDataDG
	//	We assume that RawHead and RawData are continuous memory area.
	//
	NDFT_HEADER			RawHeadDG ;
	LFSDG_RAWPKT_DATA	RawDataDG ;
} LFSDG_PKT, *PLFSDG_PKT ;

#include <poppack.h>


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
LfsOpenDGSocket(
		OUT PLFSDG_Socket	Socket,
		IN USHORT		PortNo
	) ;

VOID
LfsCloseDGSocket(
		IN PLFSDG_Socket	Socket
	) ;


NTSTATUS
LfsSendDatagram(
	IN PLFSDG_Socket	socket,
	IN PLFSDG_PKT		pkt,
	PLPX_ADDRESS		LpxRemoteAddress
) ;

BOOLEAN
LfsAllocDGPkt(
		PLFSDG_PKT *ppkt,
		UINT32 dataSz
	) ;

VOID
LfsReferenceDGPkt(
		PLFSDG_PKT pkt
	) ;

VOID
LfsDereferenceDGPkt(
		PLFSDG_PKT pkt
	) ;

BOOLEAN
LfsFreeDGPkt(
		PLFSDG_PKT pkt
	) ;

NTSTATUS
LfsRegisterDatagramRecvHandler(
		IN	PLFSDG_Socket	ServerDatagramSocket,
		IN	PVOID			EventHandler,
		IN	PVOID			EventContext
   ) ;

BOOLEAN
LfsIsFromLocal(
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
NetEvtInit(PNETEVTCTX	NetEvtCtx) ;

BOOLEAN
NetEvtTerminate(PNETEVTCTX	NetEvtCtx) ;


#if DBG

VOID
DbgPrintLpxAddrList(
		ULONG					DbgMask,
		PSOCKETLPX_ADDRESS_LIST	AddrList
) ;

#endif



#endif // __NDAS_FS_PROTOCOL_HEADER_H__
