#ifndef _SOCKET_LPX_H_
#define _SOCKET_LPX_H_

//#include <tdi.h>
//#include <winsock2.h>
//#include <ws2tcpip.h>

//#include <winnt.h>
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <tdikrnl.h>
//#include <tcpinfo.h>
//#include <tdiinfo.h>

#define HTONS(Data)		((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))
#define NTOHS(Data)		(USHORT)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))

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

#define LPX_DEVICE_NAME_PREFIX		L"\\Device\\Lpx"
#define SOCKETLPX_DEVICE_NAME		L"\\Device\\SocketLpx"
#define SOCKETLPX_DOSDEVICE_NAME	L"\\DosDevices\\SocketLpx"
#define TDI_DISCONNECT_DC_DISABLE	0x00001919

#define FSCTL_TCP_BASE     FILE_DEVICE_NETWORK

#define _TCP_CTL_CODE(function, method, access) \
            CTL_CODE(FSCTL_TCP_BASE, function, method, access)

#define IOCTL_TCP_QUERY_INFORMATION_EX  \
            _TCP_CTL_CODE(0, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_TCP_SET_INFORMATION_EX  \
            _TCP_CTL_CODE(1, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_LPX_GET_DROP_RATE \
            _TCP_CTL_CODE(100, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_LPX_SET_DROP_RATE \
            _TCP_CTL_CODE(101, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTLLPX_GET_VERSION  \
            _TCP_CTL_CODE(0x201, METHOD_NEITHER, FILE_ANY_ACCESS)


typedef struct _LPXDRV_VER {
	USHORT						VersionMajor;
	USHORT						VersionMinor;
	USHORT						VersionBuild;
	USHORT						VersionPrivate;
	UCHAR						Reserved[16];
} LPXDRV_VER, *PLPXDRV_VER;


#define AF_LPX	AF_UNSPEC

#define LPXPROTO_DGRAM 215
#define LPXPROTO_STREAM 214

#define IPPROTO_LPXTCP 214
#define IPPROTO_LPXUDP 215

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

#define TDI_ADDRESS_TYPE_LPX TDI_ADDRESS_TYPE_UNSPEC
#define TDI_ADDRESS_LENGTH_LPX sizeof (TDI_ADDRESS_LPX)

#define LSTRANS_COMPARE_LPXADDRESS(PTDI_LPXADDRESS1, PTDI_LPXADDRESS2) (		\
					RtlCompareMemory( (PLPX_ADDRESS1), (PLPX_ADDRESS2),			\
					sizeof(TDI_ADDRESS_LPX)) == sizeof(TDI_ADDRESS_LPX)	)

#define LPX_ADDRESS  TDI_ADDRESS_LPX	
#define PLPX_ADDRESS PTDI_ADDRESS_LPX 

typedef UNALIGNED struct _SOCKADDR_LPX {
    SHORT					sin_family;
	UNALIGNED LPX_ADDRESS	LpxAddress;
//	UCHAR					Align[10];
} SOCKADDR_LPX, *PSOCKADDR_LPX;


#define MAX_SOCKETLPX_INTERFACE		8

typedef struct _SOCKETLPX_ADDRESS_LIST {
    LONG			iAddressCount;
    SOCKADDR_LPX	SocketLpx[MAX_SOCKETLPX_INTERFACE];
} SOCKETLPX_ADDRESS_LIST, *PSOCKETLPX_ADDRESS_LIST;

//
// LPX uses DriverContext to store expiration time.
//
//	added by hootch 02092004
//
#define GET_IRP_EXPTIME(PIRP)			(((PLARGE_INTEGER)(PIRP)->Tail.Overlay.DriverContext)->QuadPart)
#define SET_IRP_EXPTIME(PIRP, EXPTIME)	(((PLARGE_INTEGER)(PIRP)->Tail.Overlay.DriverContext)->QuadPart = (EXPTIME))


//
//	LPX Reserved Port Numbers
//	added by hootch 03022004
//
#define		LPXRP_LFS_PRIMARY		((USHORT)0x0001)
#define		LPXRP_LFS_CALLBACK		((USHORT)0x0002)
#define		LPXRP_LFS_DATAGRAM		((USHORT)0x0003)
#define		LPXRP_LSHELPER_INFOEX	((USHORT)0x0011)


#if DBG
	
// Packet Drop Rate.
extern ULONG ulPacketDropRate;

#endif


#endif