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

#define NDASSCSI_IOCTL_GET_SLOT_NO			NDASSCSI_CTL_CODE(0x001)		// LU
//#define NDASSCSI_IOCTL_READ_ONLY			NDASSCSI_CTL_CODE(0x002)		// LU Obsolete
//#define NDASSCSI_IOCTL_QUERYINFO			NDASSCSI_CTL_CODE(0x003)		// LU Obsolete
#define NDASSCSI_IOCTL_QUERYINFO_EX			NDASSCSI_CTL_CODE(0x004)		// LU

//#define NDASSCSI_IOCTL_ADD_TARGET			NDASSCSI_CTL_CODE(0x005)		// adapter Obsolete
//#define NDASSCSI_IOCTL_REMOVE_TARGET		NDASSCSI_CTL_CODE(0x006)		// adapter Obsolete
//#define NDASSCSI_IOCTL_ADD_DEVICE			NDASSCSI_CTL_CODE(0x007)		// adapter
//#define NDASSCSI_IOCTL_REMOVE_DEVICE		NDASSCSI_CTL_CODE(0x008)		// adapter

#define NDASSCSI_IOCTL_UPGRADETOWRITE		NDASSCSI_CTL_CODE(0x009)		// LU
#define NDASSCSI_IOCTL_NOOP					NDASSCSI_CTL_CODE(0x00A)		// LU
//#define NDASSCSI_IOCTL_RECOVER_TARGET		NDASSCSI_CTL_CODE(0x00B)		// LU Obsolete
//#define NDASSCSI_IOCTL_DELAYEDOP			NDASSCSI_CTL_CODE(0x00C)		// LU Obsolete
#define NDASSCSI_IOCTL_ADD_USERBACL			NDASSCSI_CTL_CODE(0x00D)		// LU
#define NDASSCSI_IOCTL_REMOVE_USERBACL		NDASSCSI_CTL_CODE(0x00E)		// LU
#define NDASSCSI_IOCTL_DEVICE_LOCK			NDASSCSI_CTL_CODE(0x010)		// LU

#define NDASSCSI_IOCTL_GET_DVD_STATUS		NDASSCSI_CTL_CODE(0x100)		// LU Optical disk device

#define NDASSCSI_IOCTL_GET_VERSION			NDASSCSI_CTL_CODE(0x201)		// adapter


#define ETHERNET_ADDR_LENGTH				6

#define	NDASSCSI_SVCNAME					L"ndasscsi"

#define NDAS_MAX_TRANSFER_LENGTH	(1024 * 1024)

//
// NDASSCSI address
// LU specific IO control should include NDASSCSI_ADDRESS in the input header.
//

typedef union _NDASSCSI_ADDRESS {
	ULONG			NdasScsiAddress;
	struct {
		UINT8		PortId;
		UINT8		PathId;
		UINT8		TargetId;
		UINT8		Lun;
	};
	ULONG			SlotNo;
} NDASSCSI_ADDRESS, *PNDASSCSI_ADDRESS;

// NDASSCSI_IOCTL_GET_SLOT_NO
// Input: NDASSCSI_ADDRESS
// Output: Slot number

//
// Device types to be set in AddTargetData.
//

#define NDASSCSI_TYPE_ATA_DIRECT_ACCESS	0x01	// V1 : normal || V2 nDiskCount >= 1 || no info
#define NDASSCSI_TYPE_DISK_NORMAL		NDASSCSI_TYPE_ATA_DIRECT_ACCESS	// deprecated

#define NDASSCSI_TYPE_DISK_MIRROR		0x02	// V1 mirror
#define NDASSCSI_TYPE_DISK_AGGREGATION	0x03	// V1 aggr
#define NDASSCSI_TYPE_DISK_RAID0		0x06	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID1		0x04	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID4		0x05	// V2 nDiskCount >= 3
#define NDASSCSI_TYPE_DISK_RAID1R2		0x0A	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID4R2		0x0B	// V2 nDiskCount >= 3
#define NDASSCSI_TYPE_DISK_RAID1R3		0x0C	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID4R3		0x0D	// V2 nDiskCount >= 3
#define NDASSCSI_TYPE_DISK_RAID5        0x0E

#define NDASSCSI_TYPE_AOD				0x08	// Append only disk
#define NDASSCSI_TYPE_VDVD				0x64	// V1 VDVD || V2 iMediaType | NDT_VDVD

#define NDASSCSI_TYPE_ATAPI_CDROM		0x65	// PIdentify config(12:8) == 0x05
#define NDASSCSI_TYPE_DVD	NDASSCSI_TYPE_ATAPI_CDROM	// deprecated

#define NDASSCSI_TYPE_ATAPI_OPTMEM		0x66	// PIdentify config(12:8) == 0x07
#define NDASSCSI_TYPE_MO	NDASSCSI_TYPE_ATAPI_OPTMEM	// deprecated

#define NDASSCSI_TYPE_ATAPI_DIRECTACC	0x67	// PIdentify config(12:8) == 0x00
#define NDASSCSI_TYPE_ATAPI_SEQUANACC	0x68	// PIdentify config(12:8) == 0x01
#define NDASSCSI_TYPE_ATAPI_PRINTER		0x69	// PIdentify config(12:8) == 0x02
#define NDASSCSI_TYPE_ATAPI_PROCESSOR	0x6a	// PIdentify config(12:8) == 0x03
#define NDASSCSI_TYPE_ATAPI_WRITEONCE	0x6b	// PIdentify config(12:8) == 0x04
#define NDASSCSI_TYPE_ATAPI_SCANNER		0x6c	// PIdentify config(12:8) == 0x06
#define NDASSCSI_TYPE_ATAPI_MEDCHANGER	0x6d	// PIdentify config(12:8) == 0x08
#define NDASSCSI_TYPE_ATAPI_COMM		0x6e	// PIdentify config(12:8) == 0x09
#define NDASSCSI_TYPE_ATAPI_ARRAYCONT	0x6f	// PIdentify config(12:8) == 0x0c
#define NDASSCSI_TYPE_ATAPI_ENCLOSURE	0x70	// PIdentify config(12:8) == 0x0d
#define NDASSCSI_TYPE_ATAPI_RBC			0x71	// PIdentify config(12:8) == 0x0e
#define NDASSCSI_TYPE_ATAPI_OPTCARD		0x72	// PIdentify config(12:8) == 0x0f

//////////////////////////////////////////////////////////////////////////
//
//	Query Ioctl
//

//
//	NdscSystemBacl returns NDAS_BLOCK_ACL
//	NdscUserBacl returns NDAS_BLOCK_ACL
//


typedef enum _NDASSCSI_INFORMATION_CLASS {

	// NdscAdapterInformation = 1,		// 1 Obsolete
	NdscPrimaryUnitDiskInformation = 2,	// 2
	NdscLurInformation,					// 3 
	// NdscDriverVersion,				// 4 Obsolete
	NdscSystemBacl = 5,					// 5
	NdscUserBacl,						// 6

} NDASSCSI_INFORMATION_CLASS, *PNDASSCSI_INFORMATION_CLASS;


typedef struct _NDASSCSI_QUERY_INFO_DATA {

	NDASSCSI_ADDRESS			NdasScsiAddress;
	NDASSCSI_INFORMATION_CLASS	InfoClass;
	UINT32						Length;
	UINT32						QueryDataLength;
	UCHAR						QueryData[1];

} NDASSCSI_QUERY_INFO_DATA, *PNDASSCSI_QUERY_INFO_DATA;


//
//	single Adapter
//
//	Keep the same values as in ndasscsi.h
//   See NDASSCSI_NDASSCSI_ADAPTER_STATUSFLAG_*
//
#define NDASSCSI_ADAPTER_STATUS_MASK					0x000000ff
#define	NDASSCSI_ADAPTER_STATUS_INIT					0x00000000
#define	NDASSCSI_ADAPTER_STATUS_RUNNING					0x00000001
#define NDASSCSI_ADAPTER_STATUS_STOPPING				0x00000002
//#define NDASSCSI_ADAPTER_STATUS_IN_ERROR				0x00000003	// Obsolete
#define NDASSCSI_ADAPTER_STATUS_STOPPED					0x00000004

#define NDASSCSI_ADAPTER_STATUSFLAG_MASK                0xffffff00
#define NDASSCSI_ADAPTER_STATUSFLAG_RECONNECT_PENDING   0x00000100
#define NDASSCSI_ADAPTER_STATUSFLAG_RESTARTING          0x00000200
#define NDASSCSI_ADAPTER_STATUSFLAG_BUSRESET_PENDING    0x00000400
#define NDASSCSI_ADAPTER_STATUSFLAG_MEMBER_FAULT        0x00000800
#define NDASSCSI_ADAPTER_STATUSFLAG_RECOVERING          0x00001000
#define NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT   0x00002000
//#define NDASSCSI_ADAPTER_STATUSFLAG_DEFECTIVE			0x00004000	// Obsolete
#define NDASSCSI_ADAPTER_STATUSFLAG_POWERRECYCLED		0x00008000
#define NDASSCSI_ADAPTER_STATUSFLAG_RAID_FAILURE		0x00010000
#define NDASSCSI_ADAPTER_STATUSFLAG_RAID_NORMAL			0x00020000
#define NDASSCSI_ADAPTER_STATUSFLAG_RESETSTATUS         0x01000000	// only used for NDASBUS notification
#define NDASSCSI_ADAPTER_STATUSFLAG_NEXT_EVENT_EXIST	0x02000000	// only used for NDASSVC notification


#ifndef NDASSCSI_USE_MACRO
static
__forceinline BOOLEAN ADAPTERINFO_ISSTATUS(ULONG AdapterStatus, ULONG Status) {
	return (AdapterStatus & NDASSCSI_ADAPTER_STATUS_MASK) == Status;
}
#else  /* NDASSCSI_USE_MACRO */
#define ADAPTERINFO_ISSTATUS(ADAPTERSTATUS, STATUS)	\
		(((ADAPTERSTATUS) & NDASSCSI_ADAPTER_STATUS_MASK) == (STATUS))
#endif /* NDASSCSI_USE_MACRO */

#ifndef NDASSCSI_USE_MACRO
static
__forceinline BOOLEAN ADAPTERINFO_ISSTATUSFLAG(ULONG AdapterStatus, ULONG Flags) {
	return ((AdapterStatus & Flags) != 0);
}
#else  /* NDASSCSI_USE_MACRO */
#define ADAPTERINFO_ISSTATUSFLAG(ADAPTERSTATUS, STATUSFLAG)		\
		(((ADAPTERSTATUS) & (STATUSFLAG)) != 0)
#endif /* NDASSCSI_USE_MACRO */



typedef struct _NDSC_LUR {

	UINT32					Length;
	UINT32					Reserved2;
	UINT32					TargetId;
	UINT32					Lun;
	UINT32					DeviceMode;
	UINT32					SupportedFeatures;
	UINT32					EnabledFeatures;
	UINT32					LurnCnt;
	UCHAR					Reserved[12];

}	NDSC_LUR, *PNDSC_LUR;

//
//	single UnitDisk
//

#define NDASSCSI_UNITDISK_SF_UPGRADEPENDING 0x00000001
#define NDASSCSI_UNITDISK_SF_UPGRADEDONE    0x00000002

typedef struct _NDSC_UNITDISK {

	UINT32					Length;
	TA_NDAS_ADDRESS			NetDiskAddress;
	TA_NDAS_ADDRESS			BindingAddress;
	UINT32					SlotNo;
	UCHAR					UnitDiskId;
	UCHAR					Connections;
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
	TA_NDAS_ADDRESS			NetDiskAddress;
	TA_NDAS_ADDRESS			BindingAddress;
	UCHAR					UnitDiskId;
	UCHAR					Connections;
	UCHAR					UserID[4];
	UCHAR					Password[8];
	ACCESS_MASK				AccessRight;
	UINT64					UnitBlocks;
	UINT32					StatusFlags;

	UCHAR					Reserved2[16];

}	NDSC_LURN_FULL, *PNDSC_LURN_FULL;

//
//	Primary UnitDisk information
//
typedef struct _NDSCIOCTL_PRIMUNITDISKINFO {

	UINT32					Length;
	UCHAR					Reserved[20];
	NDSC_LUR				Lur;
	LARGE_INTEGER			EnabledTime;
	UCHAR					NDSC_ID[16];
	NDSC_UNITDISK			UnitDisk;

}	NDSCIOCTL_PRIMUNITDISKINFO, *PNDSCIOCTL_PRIMUNITDISKINFO;

//
//	LUR full Information
//

typedef struct _NDSCIOCTL_LURINFO {

	UINT32					Length;
	UCHAR					Reserved1[20];
	NDSC_LUR				Lur;
	LARGE_INTEGER			EnabledTime;
	UCHAR					Reserved2[16];
	UINT32					LurnCnt;
	NDSC_LURN_FULL			Lurns[1];

}	NDSCIOCTL_LURINFO, *PNDSCIOCTL_LURINFO;

//
// Driver versions
//

typedef struct _NDSCIOCTL_DRVVER {

	USHORT					VersionMajor;
	USHORT					VersionMinor;
	USHORT					VersionBuild;
	USHORT					VersionPrivate;
	UCHAR					Reserved[16];

}	NDSCIOCTL_DRVVER, *PNDSCIOCTL_DRVVER;

// NDASSCSI_IOCTL_NOOP

typedef struct _NDSCIOCTL_NOOP {

	NDASSCSI_ADDRESS		NdasScsiAddress;

}	NDSCIOCTL_NOOP, *PNDSCIOCTL_NOOP;

// NDASSCSI_IOCTL_UPGRADETOWRITE
// Input: NDSCIOCTL_UPGRADETOWRITE
// Output: N/A

typedef struct _NDSCIOCTL_UPGRADETOWRITE {

	NDASSCSI_ADDRESS		NdasScsiAddress;

} NDSCIOCTL_UPGRADETOWRITE, *PNDSCIOCTL_UPGRADETOWRITE;

// NDASSCSI_IOCTL_ADD_USERBACL
// Input: NDSCIOCTL_ADD_USERBACL
// Output: BLOCKACE_ID
typedef struct _NDSCIOCTL_ADD_USERBACL {
	NDASSCSI_ADDRESS		NdasScsiAddress;
	NDAS_BLOCK_ACE			NdasBlockAce;
} NDSCIOCTL_ADD_USERBACL, *PNDSCIOCTL_ADD_USERBACL;

// NDASSCSI_IOCTL_REMOVE_USERBACL
// Input: NDSCIOCTL_REMOVE_USERBACL
// Output: N/A
typedef struct _NDSCIOCTL_REMOVE_USERBACL {
	NDASSCSI_ADDRESS		NdasScsiAddress;
	BLOCKACE_ID				NdasBlockAceId;
} NDSCIOCTL_REMOVE_USERBACL, *PNDSCIOCTL_REMOVE_USERBACL;


// NDASSCSI_IOCTL_GET_DVD_STATUS
// NDASBUS_DVD_STATUS structure has slot number field.


//////////////////////////////////////////////////////////////////////////
//
// NDAS device lock control
//
// NDASSCSI_IOCTL_DEVICE_LOCK
//

//
// Lock reservation
// NDAS chip-wide lock IDs,not unit device-wide.
// NOTE: The number of lock index are irrelevant to NDAS chip lock index.
//		 Keep the same numbers with lslur.h
//

#define NDSCLOCK_ID_NONE			0
#define NDSCLOCK_ID_XIFS			1
#define NDSCLOCK_ID_BUFFLOCK		2

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

#define LURNLOCK_OPCODE_ACQUIRE		0x01	// Available in NDAS chip 1.1, 2.0.

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

#define LURNLOCK_OPCODE_RELEASE		0x02	// Available in NDAS chip 1.1, 2.0.

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
	NDASSCSI_ADDRESS	NdasScsiAddress;
	UINT32	LockId;
	UINT8	LockOpCode;
	UINT8	AdvancedLock:1;				// advanced GP lock operation. ADV_GPLOCK feature required.
	UINT8	AddressRangeValid:1;		// starting and ending address range is valid.
	UINT8	RequireLockAcquisition:1;	// lock required before performing the operation
	UINT8	Reserved1:5;
	UINT8	Reserved2[2];
	UINT64	StartingAddress;			// Starting IO address after lock acquisition
	UINT64	EndingAddress;				// Ending IO address after lock acquisition
	UINT64	ContentionTimeOut;			// 100ns unit. Negative is relative time.

	//
	// Lock data
	//

	UINT8	LockData[NDSCLOCK_LOCKDATA_LENGTH];
} NDSCIOCTL_DEVICELOCK, *PNDSCIOCTL_DEVICELOCK;

#endif
