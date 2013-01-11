/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _SOCKET_LPX_H_
#define _SOCKET_LPX_H_


#define LPX_BOUND_DEVICE_NAME_PREFIX	L"\\Device\\Lpx"
#define SOCKETLPX_DEVICE_NAME			L"\\Device\\SocketLpx"
#define SOCKETLPX_DOSDEVICE_NAME		L"\\DosDevices\\SocketLpx"

#define FSCTL_LPX_BASE     FILE_DEVICE_NETWORK

#define _LPX_CTL_CODE(function, method, access) \
            CTL_CODE(FSCTL_LPX_BASE, function, method, access)

#define IOCTL_LPX_QUERY_ADDRESS_LIST  \
            _LPX_CTL_CODE(0, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_LPX_SET_INFORMATION_EX  \
            _LPX_CTL_CODE(1, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_LPX_GET_RX_DROP_RATE \
            _LPX_CTL_CODE(100, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LPX_SET_RX_DROP_RATE \
            _LPX_CTL_CODE(101, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_LPX_GET_TX_DROP_RATE \
            _LPX_CTL_CODE(102, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LPX_SET_TX_DROP_RATE \
            _LPX_CTL_CODE(103, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_LPX_GET_VERSION  \
            _LPX_CTL_CODE(0x201, METHOD_NEITHER, FILE_ANY_ACCESS)


typedef struct _LPXDRV_VER {
	USHORT						VersionMajor;
	USHORT						VersionMinor;
	USHORT						VersionBuild;
	USHORT						VersionPrivate;
	UCHAR						Reserved[16];
} LPXDRV_VER, *PLPXDRV_VER;

enum {
	AF_LPX = 0,
	AF_LPX2 = 63,
	LPXPROTO_TCP = 214,
	LPXPROTO_UDP = 215,
	LPXPROTO_STREAM = LPXPROTO_TCP,
	LPXPROTO_DGRAM = LPXPROTO_UDP,
	IPPROTO_LPXTCP = LPXPROTO_TCP,
	IPPROTO_LPXUDP = LPXPROTO_UDP,
};

typedef UNALIGNED struct _TDI_ADDRESS_LPX {
	USHORT	Port;
	UCHAR	Node[6];
	UCHAR	Reserved[10];	// To make the same size as NetBios
} TDI_ADDRESS_LPX, *PTDI_ADDRESS_LPX;

typedef struct _TA_ADDRESS_LPX {

    LONG  TAAddressCount;
    struct  _AddrLpx {
        USHORT				AddressLength;
        USHORT				AddressType;
        TDI_ADDRESS_LPX		Address[1];
    } Address [1];

} TA_LPX_ADDRESS, *PTA_LPX_ADDRESS;


#define LPXADDR_NODE_LENGTH	6

#define TDI_ADDRESS_TYPE_LPX		TDI_ADDRESS_TYPE_UNSPEC
#define TDI_ADDRESS_TYPE_INVALID	((USHORT)(0x1A5C))

#define TDI_ADDRESS_LENGTH_LPX sizeof (TDI_ADDRESS_LPX)

#define LSTRANS_COMPARE_LPXADDRESS(PTDI_LPXADDRESS1, PTDI_LPXADDRESS2) (		\
					RtlCompareMemory( (PLPX_ADDRESS1), (PLPX_ADDRESS2),			\
					sizeof(TDI_ADDRESS_LPX)) == sizeof(TDI_ADDRESS_LPX)	)

#define LPX_ADDRESS  TDI_ADDRESS_LPX	
#define PLPX_ADDRESS PTDI_ADDRESS_LPX 

typedef UNALIGNED struct _SOCKADDR_LPX {
    SHORT					sin_family;
    LPX_ADDRESS				LpxAddress;

} SOCKADDR_LPX, *PSOCKADDR_LPX;


#define MAX_SOCKETLPX_INTERFACE		8

typedef struct _SOCKETLPX_ADDRESS_LIST {
    LONG			iAddressCount;
    SOCKADDR_LPX	SocketLpx[MAX_SOCKETLPX_INTERFACE];
} SOCKETLPX_ADDRESS_LIST, *PSOCKETLPX_ADDRESS_LIST;

//
// Alway keep the same format with LS_TRANS_STAT in transport.h
//
#define LPXTDI_QUERY_CONNECTION_TRANSSTAT	0x80000001
#define LPX_NDIS_QUERY_GLOBAL_STATS			0x80000002

typedef struct _TRANS_STAT {
    ULONG Retransmits;
    ULONG PacketLoss;
} TRANS_STAT, *PTRANS_STAT;

//
//	LPX Reserved Port Numbers
//	When these port numbers are put in LPX address, must be converted to
//  Big-endian.
//
// Reserved port should be under 0x4000
//

#define		LPXRP_LFS_PRIMARY			((UINT16)0x0001)	// stream
#define		LPXRP_LFS_CALLBACK			((UINT16)0x0002)	// stream
#define		LPXRP_LFS_DATAGRAM			((UINT16)0x0003)	// datagram
#define		LPXRP_LSHELPER_INFOEX		((UINT16)0x0011)	// datagram
#define		LPXRP_NDAS_PROTOCOL			((UINT16)0x2710)	// stream
#define		LPXRP_NDAS_HEARTBEAT_SRC	((UINT16)0x2711)	// datagram
#define		LPXRP_NDAS_HEARTBEAT_DEST	((UINT16)0x2712)	// datagram

#define		LPXRP_NRMX_ARBITRRATOR_PORT	((UINT16)0x0010)	// stream
#define		LPXRP_HIX_PORT				((UINT16)0x00EE)	// stream

#define		LPXRP_XIXFS_SVRPORT			((UINT16)0x1000) // stream

#ifndef __NDAS_COMMON_HEADER_H__
#define HTONS(Data)		((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))
#define NTOHS(Data)		(UINT16)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))

#define HTONL(Data)		( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
						| (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))
#define NTOHL(Data)		( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
						| (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))

#define HTONLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define NTOHLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#endif

#endif
