/********************************************************************

created:	2003/12/15
created:	15:12:2003   19:51
filename: 	ndasscsiioctl.h
file path:	src\inc\drivers
author:		XIMETA, Inc.

purpose:	IO control code and structure of NDASSCSI

*********************************************************************/

#ifndef NDASSCSI_IOCTL_H
#define NDASSCSI_IOCTL_H

#include "lanscsi.h"

//
//	define IOCTL code rule.
//
#ifndef CTL_CODE

#define CTL_CODE( DeviceType, Function, Method, Access ) \
	(((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define METHOD_BUFFERED                 0
#define METHOD_IN_DIRECT                1
#define METHOD_OUT_DIRECT               2
#define METHOD_NEITHER                  3
#define FILE_ANY_ACCESS                 0
#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe

#endif

//
//	define NDASSCSI ioctl rule.
//
#define NDASSCSI_DEVICE_TYPE 0x89e9
#define NDASSCSI_CTL_CODE(x) \
	CTL_CODE(NDASSCSI_DEVICE_TYPE, x, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
//	signature of NDASSCSI IO control.
//
#define NDASSCSI_IOCTL_SIGNATURE "XIMETA_1"

#define NDASSCSI_IOCTL_GET_SLOT_NO			NDASSCSI_CTL_CODE(0x001)
//#define NDASSCSI_IOCTL_READ_ONLY			NDASSCSI_CTL_CODE(0x002)		// Obsolete
//#define NDASSCSI_IOCTL_QUERYINFO			NDASSCSI_CTL_CODE(0x003)		// Obsolete
#define NDASSCSI_IOCTL_QUERYINFO_EX			NDASSCSI_CTL_CODE(0x004)
#define NDASSCSI_IOCTL_ADD_TARGET			NDASSCSI_CTL_CODE(0x005)
#define NDASSCSI_IOCTL_REMOVE_TARGET		NDASSCSI_CTL_CODE(0x006)
#define NDASSCSI_IOCTL_ADD_DEVICE			NDASSCSI_CTL_CODE(0x007)
#define NDASSCSI_IOCTL_REMOVE_DEVICE		NDASSCSI_CTL_CODE(0x008)
#define NDASSCSI_IOCTL_UPGRADETOWRITE		NDASSCSI_CTL_CODE(0x009)
#define NDASSCSI_IOCTL_NOOP					NDASSCSI_CTL_CODE(0x00A)
//#define NDASSCSI_IOCTL_RECOVER_TARGET		NDASSCSI_CTL_CODE(0x00B)		// Obsolete
//#define NDASSCSI_IOCTL_DELAYEDOP			NDASSCSI_CTL_CODE(0x00C)		// Obsolete
#define NDASSCSI_IOCTL_ADD_USERBACL			NDASSCSI_CTL_CODE(0x00D)
#define NDASSCSI_IOCTL_REMOVE_USERBACL		NDASSCSI_CTL_CODE(0x00E)
#define NDASSCSI_IOCTL_DEVICE_LOCK			NDASSCSI_CTL_CODE(0x010)
#define NDASSCSI_IOCTL_GET_DVD_STATUS		NDASSCSI_CTL_CODE(0x100)
#define NDASSCSI_IOCTL_GET_VERSION			NDASSCSI_CTL_CODE(0x201)


#define ETHERNET_ADDR_LENGTH						6

#define	NDASSCSI_SVCNAME						L"ndasscsi"


//////////////////////////////////////////////////////////////////////////
//
//	Query Ioctl
//

//
//	NdscSystemBacl returns NDAS_BLOCK_ACL
//	NdscUserBacl returns NDAS_BLOCK_ACL
//


typedef enum _NDASSCSI_INFORMATION_CLASS {

	NdscAdapterInformation = 1,			// 1
	NdscPrimaryUnitDiskInformation,		// 2
	NdscAdapterLurInformation,			// 3
	NdscDriverVersion,					// 4
	NdscSystemBacl,						// 5
	NdscUserBacl,						// 6

} NDASSCSI_INFORMATION_CLASS, *PNDASSCSI_INFORMATION_CLASS;


typedef struct _NDASSCSI_QUERY_INFO_DATA {

	UINT32					   Length;
	NDASSCSI_INFORMATION_CLASS InfoClass;
    UINT32					   SlotNo;				// Optional
	UINT32					   QueryDataLength;
	UCHAR					   QueryData[1];

} NDASSCSI_QUERY_INFO_DATA, *PNDASSCSI_QUERY_INFO_DATA;


//
//	single Adapter
//
//	Keep the same values as in ndasscsi.h
//
#define NDASSCSI_ADAPTERINFO_STATUS_MASK            0x000000ff
#define	NDASSCSI_ADAPTERINFO_STATUS_INIT            0x00000000
#define	NDASSCSI_ADAPTERINFO_STATUS_RUNNING         0x00000001
#define NDASSCSI_ADAPTERINFO_STATUS_STOPPING        0x00000002
#define NDASSCSI_ADAPTERINFO_STATUS_IN_ERROR        0x00000003
#define NDASSCSI_ADAPTERINFO_STATUS_STOPPED         0x00000004

#define NDASSCSI_ADAPTERINFO_STATUSFLAG_MASK                0xffffff00
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_RECONNECT_PENDING   0x00000100
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_RESTARTING          0x00000200
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_BUSRESET_PENDING    0x00000400
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_MEMBER_FAULT        0x00000800
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_RECOVERING          0x00001000
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_ABNORMAL_TERMINAT   0x00002000
#define NDASSCSI_ADAPTERINFO_STATUSFLAG_RESETSTATUS         0x01000000


#define LURN_STATUS_INIT							0x00000000
#define LURN_STATUS_RUNNING							0x00000001
#define LURN_STATUS_STALL							0x00000002
#define LURN_STATUS_STOP_PENDING					0x00000003
#define LURN_STATUS_STOP							0x00000004

#ifndef NDASSCSI_USE_MACRO
__forceinline BOOLEAN ADAPTERINFO_ISSTATUS(ULONG AdapterStatus, ULONG Status) {
	return (AdapterStatus & NDASSCSI_ADAPTERINFO_STATUS_MASK) == Status;
}
#else  /* NDASSCSI_USE_MACRO */
#define ADAPTERINFO_ISSTATUS(ADAPTERSTATUS, STATUS)	\
		(((ADAPTERSTATUS) & NDASSCSI_ADAPTERINFO_STATUS_MASK) == (STATUS))
#endif /* NDASSCSI_USE_MACRO */

#ifndef NDASSCSI_USE_MACRO
__forceinline BOOLEAN ADAPTERINFO_ISSTATUSFLAG(ULONG AdapterStatus, ULONG Flags) {
	return ((AdapterStatus & Flags) != 0);
}
#else  /* NDASSCSI_USE_MACRO */
#define ADAPTERINFO_ISSTATUSFLAG(ADAPTERSTATUS, STATUSFLAG)		\
		(((ADAPTERSTATUS) & (STATUSFLAG)) != 0)
#endif /* NDASSCSI_USE_MACRO */

#ifndef NDASSCSI_USE_MACRO
__forceinline BOOLEAN LURN_IS_RUNNING(ULONG LurnStatus) {
	return (LurnStatus == LURN_STATUS_RUNNING) || (LurnStatus == LURN_STATUS_STALL);
}
#else  /* NDASSCSI_USE_MACRO */
#define LURN_IS_RUNNING(LURNSTATUS)				(((LURNSTATUS) == LURN_STATUS_RUNNING) || ((LURNSTATUS) == LURN_STATUS_STALL))
#endif /* NDASSCSI_USE_MACRO */

typedef struct _NDSC_ADAPTER {

	UINT32					Length;
	UINT32					SlotNo;
	UCHAR					InitiatorId;
    UCHAR					NumberOfBuses;
    UCHAR					MaximumNumberOfTargets;
    UCHAR					MaximumNumberOfLogicalUnits;
	UINT32					MaxDataTransferLength;
	UINT32					Status;

}	NDSC_ADAPTER, *PNDSC_ADAPTER;

typedef struct _NDSC_LUR {

	UINT32					Length;
	UINT32					DevType;
	UINT32					TargetId;
	UINT32					Lun;
	UINT32					DeviceMode;
	UINT32					SupportedFeatures;
	UINT32					EnabledFeatures;
	UINT32					LurnCnt;
	UCHAR					Reserved[16];

}	NDSC_LUR, *PNDSC_LUR;

//
//	single UnitDisk
//

#define NDASSCSI_UNITDISK_SF_UPGRADEPENDING 0x00000001
#define NDASSCSI_UNITDISK_SF_UPGRADEDONE    0x00000002

typedef struct _NDSC_UNITDISK {

	UINT32					Length;
	TA_LSTRANS_ADDRESS		NetDiskAddress;
	TA_LSTRANS_ADDRESS		BindingAddress;
	UINT32					SlotNo;
	UCHAR					UnitDiskId;
	UCHAR					Reserved;
	UCHAR					UserID[4];
	UCHAR					Password[8];
	UINT32					Reserved2;
	UINT32					GrantedAccess;
	UINT32					UnitBlocks;
	UINT32					StatusFlags;

}	NDSC_UNITDISK, *PNDSC_UNITDISK;

typedef struct _NDSC_LURN_FULL {

	UINT32					Length;
	UINT32					LurnId;
	UINT32					LurnType;
	TA_LSTRANS_ADDRESS		NetDiskAddress;
	TA_LSTRANS_ADDRESS		BindingAddress;
	UCHAR					UnitDiskId;
	UCHAR					Reserved;
	UCHAR					UserID[4];
	UCHAR					Password[8];
	ACCESS_MASK				AccessRight;
	UINT64					UnitBlocks;
	UINT32					StatusFlags;

	UCHAR					Reserved2[16];

}	NDSC_LURN_FULL, *PNDSC_LURN_FULL;

//
//	Adapter Information
//
typedef struct _NDSCIOCTL_ADAPTERINFO {

	UINT32					Length;
	NDSC_ADAPTER			Adapter;

}	NDSCIOCTL_ADAPTERINFO, *PNDSCIOCTL_ADAPTERINFO;

//
//	Primary UnitDisk information
//
typedef struct _NDSCIOCTL_PRIMUNITDISKINFO {

	UINT32					Length;
	NDSC_ADAPTER			Adapter;
	NDSC_LUR				Lur;
	LARGE_INTEGER			EnabledTime;
	UCHAR					Reserved[16];
	NDSC_UNITDISK			UnitDisk;

}	NDSCIOCTL_PRIMUNITDISKINFO, *PNDSCIOCTL_PRIMUNITDISKINFO;

//
//	Adapter full Information
//
typedef struct _NDSCIOCTL_ADAPTERLURINFO {

	UINT32					Length;
	NDSC_ADAPTER			Adapter;
	NDSC_LUR				Lur;
	LARGE_INTEGER			EnabledTime;
	UCHAR					Reserved[16];
	UINT32					UnitDiskCnt;
	NDSC_LURN_FULL		UnitDisks[1];

}	NDSCIOCTL_ADAPTERLURINFO, *PNDSCIOCTL_ADAPTERLURINFO;

typedef struct _NDSCIOCTL_DRVVER {

	USHORT						VersionMajor;
	USHORT						VersionMinor;
	USHORT						VersionBuild;
	USHORT						VersionPrivate;
	UCHAR						Reserved[16];

}	NDSCIOCTL_DRVVER, *PNDSCIOCTL_DRVVER;

typedef struct _NDSCIOCTL_NOOP {

	BYTE						PathId;
	BYTE						TargetId;
	BYTE						Lun;

}	NDSCIOCTL_NOOP, *PNDSCIOCTL_NOOP;

//////////////////////////////////////////////////////////////////////////
//
// NDAS device lock control
//

//
// Lock reservation
// NDAS chip-wide lock IDs,not unit device-wide.
// NOTE: The number of lock index are irrelevant to NDAS chip lock index.
//		 Keep the same numbers with lslurn.h
//

#define NDSCLOCK_ID_NONE			0
#define NDSCLOCK_ID_XIFS			1

//
// Lock data length
//

#define NDSCLOCK_LOCKDATA_LENGTH	64

//
// Lock operation codes
//

//
// Acquire a lock
// NOTE: NDASSCSI may lose lock acquisition during reconnection.
//       The NDAS device releases any locks acquired by disconnected connection.
//       Or, the NDAS device might get a reset.
//
//       Therefore, when an IO requires a specific lock acquisition,
//       specify the IO's address range with the lock request.
//       NDASSCSI will fail the IO in the IO range of lost locks before getting
//       lock release request.
//       Invalidate the former lock data, and issue lock release request.
//       However, lock data will not be set in the NDAS device.
//
//  * Lock data in/out
//    General purpose lock mode
//      - INPUT: N/A
//      - OUTPUT: 4 bytes of Device lock count
//    Advanced general purpose lock mode ( available in NDAS chip 2.5 )
//      - INPUT: N/A
//      - OUTPUT: 64 byte of Device lock data
//

#define NDSCLOCK_OPCODE_ACQUIRE		0x01	// Available in NDAS chip 1.1, 2.0.

//
// Release a lock
//
//  * Lock data in/out
//    General purpose lock mode
//      - INPUT: N/A
//      - OUTPUT: 4 bytes of Device lock count
//    Advanced general purpose lock mode
//      - INPUT: 64 bytes of Device lock data
//      - OUTPUT: N/A
//

#define NDSCLOCK_OPCODE_RELEASE		0x02	// Available in NDAS chip 1.1, 2.0.

//
// Query the owner of a lock.
//
//  * Lock data in/out
//    General purpose lock mode
//      - INPUT: N/A
//      - OUTPUT: 6 byte MAC address. 2 byte port number ( 0x0000 )
//    Advanced general purpose lock mode
//      - INPUT: N/A
//      - OUTPUT: 6 byte MAC address. 2 byte port number. 
//

#define NDSCLOCK_OPCODE_QUERY_OWNER	0x03	// Available in NDAS chip 1.1, 2.0.

//
// Get the lock data of a lock.
//
//  * Lock data in/out
//  General purpose lock mode
//      - INPUT: N/A
//      - OUTPUT: 6 byte MAC address. 2 byte port number ( 0x0000 )
//  Advanced general purpose lock mode
//      - INPUT: N/A
//      - OUTPUT: 6 byte MAC address. 2 byte port number. 
//

#define NDSCLOCK_OPCODE_GETDATA		0x04	// Available in NDAS chip 1.1, 2.0.

//
// Set the lock data of a lock
//  General purpose lock mode
//
//  * Lock data in/out
//    - INPUT: N/A
//    - OUTPUT: 4 bytes of Device lock count
//  Advanced general purpose lock mode
//    - INPUT: N/A
//    - OUTPUT: 64 bytes of Device lock data
//

#define NDSCLOCK_OPCODE_SETDATA		0x05	// Available in NDAS chip 2.5 and later.

//
// Break ( forcedly release ) a lock that is owned by other clients.
//
//  * Lock data in/out
//    General purpose lock mode
//      - Not supported
//    Advanced general purpose lock mode
//      - INPUT: 64 bytes of Device lock data
//      - OUTPUT: N/A

#define NDSCLOCK_OPCODE_BREAK		0x06	// Available in NDAS chip 2.5 and later.

//
// NDASSCSI_IOCTL_DEVICE_LOCK
//

typedef struct _NDSCIOCTL_DEVICELOCK {
	UINT32	LockId;
	BYTE	LockOpCode;
	BYTE	AdvancedLock:1;				// advanced GP lock operation. ADV_GPLOCK feature required.
	BYTE	AddressRangeValid:1;		// starting and ending address range is valid.
	BYTE	RequireLockAcquisition:1;	// lock required before performing the operation
	BYTE	Reserved1:5;
	BYTE	Reserved2[2];
	UINT64	StartingAddress;			// Starting IO address after lock acquisition
	UINT64	EndingAddress;				// Ending IO address after lock acquisition
	UINT64	ContentionTimeOut;			// 100ns unit

	//
	// Lock data
	//

	BYTE	LockData[NDSCLOCK_LOCKDATA_LENGTH];
} NDSCIOCTL_DEVICELOCK, *PNDSCIOCTL_DEVICELOCK;

#endif
