#ifndef LANSCSIHELPER_H
#define LANSCSIHELPER_H

//////////////////////////////////////////////////////////////////////////
//
//	External defines
//
//////////////////////////////////////////////////////////////////////////
#define	INFOX_UPDATE_TIMEOUT		(1000 * 4) // 4 sec in unit of 1 millisec

//////////////////////////////////////////////////////////////////////////
//
//	External variables
//
//////////////////////////////////////////////////////////////////////////
extern DWORD	WinMajorVer ;
extern DWORD	WinMinorVer ;
extern DWORD	WinBuild ;

//extern CMutex	DataMutex;

#define LSHFLAG_UNEXPECTED_HANG		0x00000001

//extern ULONG	LSHRegistryFlags ;
//extern HKEY		hRegKey;

//
//	Hostname resolution
//
#define MAX_HOSTNAME_LENGTH		256

#define LSNODENAME_UNKNOWN				0x0000

#define LSNODENAME_DNSHOSTNAME			0x0010
#define LSNODENAME_DNSDOMAIN			0x0011
#define LSNODENAME_PHYSICALDNSDOMAIN	0x0012
#define LSNODENAME_DNSFULLYQ			0x0013
#define LSNODENAME_PHYSICALDNSFULLYQ	0x0014

#define LSNODENAME_NETBOIS				0x0020
#define LSNODENAME_PHYSICALNETBIOS		0x0021


//
//
//	define address type and access right
//
#define LSNODE_ADDRTYPE_ETHER		0x0101
#define LSNODE_ADDRTYPE_IP			0x0202
#define LSNODE_ADDRTYPE_IPV6		0x0203

#define LSNODE_ADDR_LENGTH			16

#define LSSESSION_ACCESS_READ		0x0001
#define LSSESSION_ACCESS_WRITE		0x0002

typedef struct _LSNODE_ADDRESS {

    USHORT	AddressType;
	USHORT	AddressLen;
	UCHAR	Address[LSNODE_ADDR_LENGTH];		// 20 bytes

} LSNODE_ADDRESS, *PLSNODE_ADDRESS;

typedef struct _NETDISK_USAGE {

	LSNODE_ADDRESS	HostLanAddr;
	LSNODE_ADDRESS	HostWanAddr;
	LONG			HostNameLength;
	USHORT			HostNameType;
	USHORT			HostName[MAX_HOSTNAME_LENGTH];		// Unicode
	UCHAR			UsageId;
	UCHAR			AccessRight;

} NETDISK_USAGE, *PNETDISK_USAGE;


typedef struct _NETDISK_USAGE_INFORMATION {

	LONG			Length;
	LPX_ADDRESS		NetDiskLpxAddr;
	LONG			UsageROCount;
	LONG			UsageRWCount;
	LONG			UsageCount;
	NETDISK_USAGE	Usage[1];

} NETDISK_USAGE_INFORMATION, *PNETDISK_USAGE_INFORMATION;


//////////////////////////////////////////////////////////////////////////
//
//	External functions
//
//////////////////////////////////////////////////////////////////////////

////
////	defined in LDServ.cpp
////
//BOOL
//AcquireLSHMutex() ;
//
//VOID
//ReleaseLSHMutex() ;
//
////
////	defined in NetDisk.cpp
////
//int 
//GetInterfaceList(
//				 LPSOCKET_ADDRESS_LIST	socketAddressList,
//				 DWORD					socketAddressListLength
//				 );
//
////
////	defined in InfoXchanger.cpp
////
//BOOL
//InfoX_StartAgent(VOID) ;
//
//BOOL
//InfoX_StartBroadcaster(VOID) ;
//
//BOOL
//NICDetector_Start(VOID) ;
//
//DWORD
//InfoXQueryUsage(
//		PLPX_ADDRESS				NetDiskAddress,
//		ULONG						UnitDiskNo,
//		LONG						UsageInfoLen,
//		PNETDISK_USAGE_INFORMATION	UsageInfo,
//		LONG						ExpectedReply
//	) ;
//
////
////	defined in service.cpp
////
//BOOL
//ShowModalessMessageBox(
//		HINSTANCE	InstanceHandle,
//		UINT		MsgID,
//		PTCHAR		MsgArg1,
//		UINT		MsgArg1Len,
//		UINT		MBOpt
//) ;

#endif
