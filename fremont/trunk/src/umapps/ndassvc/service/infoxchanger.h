#ifndef LSHELPER_INFOEXC_H
#define LSHELPER_INFOEXC_H

#include "SocketLpx.h"

#define INFOEX_PORT	 LPXRP_LSHELPER_INFOEX

// #define BROADCAST_PERIOD	(HZ * 1)	// 1 second
// #define USAGE_TIMEOUT		(HZ / 5)	// 0.2 second
#define BROADCAST_PERIOD	(1000)	// 1 second
#define USAGE_TIMEOUT		(1000 / 5)	// 0.2 second
#define USAGE_TIMEOUT_LOOP	(2)			// 0.4 seconds

#define _U8		UCHAR
#define _U16	USHORT
#define _U32	ULONG
#define _U64	ULONGLONG


#include <pshpack1.h>


typedef struct _INFOXNODE_ADDRESS {

    _U16	AddressType;
	_U16	AddressLen;
	_U8		Address[LSNODE_ADDR_LENGTH];		// 20 bytes

} INFOXNODE_ADDRESS, *PINFOXNODE_ADDRESS;

//
// NOTE: Little Endian
//
typedef struct _LSINFOX_HEADER
{
	_U8		Protocol[4];
	_U16	LSInfoXMajorVersion;
	_U16	LSInfoXMinorVersion;		// 8 bytes
	_U16	OsMajorType;
	_U16	OsMinorType;
	_U16	Type;
	_U16	Reserved;					// 16 bytes
	_U32	MessageSize; // Total Message Size (including Header Size)	

	// <--- 20 bytes

	_U8		Reserved2[16] ;

} LSINFOX_HEADER, *PLSINFOX_HEADER;


typedef struct _LSINFOX_PRIMARY_UPDATE
{
	union
	{
		_U8		Type;
		struct 
		{
			_U8		AddDelete:1; // Add = 0, Delete = 1
			_U8		Reserved:7;
		};
	};

	_U8		Reserved1 ;			// 2 bytes
	_U8		PrimaryNode[6];		// 8 bytes
	_U16	PrimaryPort;
	_U8		NetDiskNode[6];		// 16 bytes
	_U16	NetDiskPort;
	_U16	UnitDiskNo;			// 20 bytes
	_U8		SWMajorVersion ;
	_U8		SWMinorVersion ;
	_U16	SWBuildNumber ;		// 24 bytes
	_U16	NDFSCompatVersion ;
	_U16	NDFSVersion ;
	// <-- 28 bytes

	_U8		Reserved3[512 - 28] ;

} LSINFOX_PRIMARY_UPDATE, *PLSINFOX_PRIMARY_UPDATE;


typedef struct _LSINFOX_NDASDEV_USAGE_REQUEST
{
	_U8		NetDiskNode[6];
	_U16	NetDiskPort;		// 8 bytes
	_U16	UnitDiskNo;			// 10 bytes
//	_U8		Reserved[54 + 512];

} LSINFOX_NDASDEV_USAGE_REQUEST, *PLSINFOX_NDASDEV_USAGE_REQUEST;


typedef struct _LSINFOX_NDASDEV_USAGE_REPLY
{
	INFOXNODE_ADDRESS	HostLanAddr;	// 20 bytes
	INFOXNODE_ADDRESS	HostWanAddr;	// 40 bytes
	_U8		NetDiskNode[6];
	_U16	NetDiskPort;		// 48 bytes
	_U16	UnitDiskNo;
	_U8		UsageID;
	_U8		AccessRight;
	_U8		SWMajorVersion;
	_U8		SWMinorVersion;
	_U16	SWBuildNumber;		// 56 bytes
	_U16	NDFSCompatVersion;
	_U16	NDFSVersion;		// 60 bytes
	_U16	HostNameType;
	_U16	HostNameLength;		// 64 bytes
	_U16	HostName[1];		// Unicode
	// <-- 66 bytes
} LSINFOX_NDASDEV_USAGE_REPLY, *PLSINFOX_NDASDEV_USAGE_REPLY;


#include <poppack.h>

//
//	memory pool tag
//
#define INFOX_MEM_TAG_PACKET		'pSFL'
#define INFOX_MEM_TAG_SOCKET		'sSFL'

//
// Protocol Signature
//
#define INFOX_DATAGRAM_PROTOCOL	{ 'L', 'S', 'H', 'P' }

//
//	LFS current version
//
#define	INFOX_DATAGRAM_MAJVER		0
#define	INFOX_DATAGRAM_MINVER		1

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

#define OSTYPE_UNKNOWN			0xFFFF

//
//	packet type masks
//
#define LSINFOX_TYPE_PREFIX		0xF000
#define	LSINFOX_TYPE_MAJTYPE	0x0FF0
#define	LSINFOX_TYPE_MINTYPE	0x000F

//
//	packet type prefixes
//
#define	LSINFOX_TYPE_REQUEST		0x1000	//	request packet
#define	LSINFOX_TYPE_REPLY			0x2000	//	reply packet
#define	LSINFOX_TYPE_DATAGRAM		0x4000	//	datagram packet
#define	LSINFOX_TYPE_BROADCAST		0x8000	//	broadcasting packet

#define LSINFOX_PRIMARY_UPDATE_MESSAGE		0x0010
#define LSINFOX_PRIMARY_USAGE_MESSAGE		0x0020

///////////////////////////////////////////////////////////////////////////
//
// define the packet format using datagramtype LPX
//
//
//	head part of a LFS datagram packet
//


#include <pshpack1.h>

#define INFOX_MAX_DATAGRAM_PKT_SIZE	(1024)

typedef	union _LSINFOX_RawPktData {

		UCHAR							Data[INFOX_MAX_DATAGRAM_PKT_SIZE - sizeof(LSINFOX_HEADER)];
		LSINFOX_PRIMARY_UPDATE			Update;
Y( HeapFree(::GetProcessHeap(), 0, m_pAddTargetInfo) );
		m_pAddTargetInfo = NULL;
	}
	if (NULL != m_pDIBv2) 
	{
		XTLVERIFY( HeapFree(::GetProcessHeap(), 0, m_pDIBv2) );
		m_pDIBv2  = NULL;
	}
	if (NULL != m_pBACL) 
	{
		XTLVERIFY( HeapFree(::GetProcessHeap(), 0, m_pBACL) );
		m_pBACL  = NULL;
	}
}

BOOL
CNdasUnitDiskDevice::HasSameDIBInfo()
{
	CNdasUnitDeviceCreator udCreator(GetParentDevice(), GetUnitNo());

	CNdasUnitDevicePtr pUnitDeviceNow( udCreator.CreateUnitDevice() );

	if (CNdasUnitDeviceNullPtr== pUnitDeviceNow) 
	{
		return FALSE;
	}

	if (GetType() != pUnitDeviceNow->GetType()) 
	{
		return