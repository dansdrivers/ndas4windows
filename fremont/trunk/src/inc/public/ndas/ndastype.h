/*++

  NDAS USER API Type Definitions

  Copyright (C) 2002-2004 XIMETA, Inc.
  All rights reserved.

  Remarks:

  This header contains the structures for using NDAS USER API.

--*/

#ifndef _NDAS_TYPE_H_
#define _NDAS_TYPE_H_

#pragma once
#include <cfgmgr32.h>

#include <ndas/ndasdib.h>

/* All structures in this header are 8-byte unaligned. */
#include <pshpack8.h>

#ifndef MSC_DEPRECATE_SUPPORTED
#if (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#define MSC_DEPRECATE_SUPPORTED
#endif
#endif

/* Constants */

#define NDAS_DEVICE_ID_KEY_LEN         20
#define NDAS_DEVICE_WRITE_KEY_LEN      5

#define NDAS_DEVICE_STRING_ID_PART_LEN 5
#define NDAS_DEVICE_STRING_ID_PARTS    4
#define NDAS_DEVICE_STRING_ID_LEN \
	(NDAS_DEVICE_STRING_ID_PART_LEN * NDAS_DEVICE_STRING_ID_PARTS)

#define NDAS_DEVICE_STRING_KEY_LEN     5
#define MAX_NDAS_DEVICE_NAME_LEN       31 /* excluding NULL character */
#define MAX_NDAS_REGISTRATION_DATA     252

#define NDAS_BLOCK_SIZE					512

#define MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER	32

/* Host count is unavailable */
#define NDAS_HOST_COUNT_UNKNOWN ((DWORD)(-1))

/* number of available locks in the NDAS device */
#define NDAS_DEVICE_LOCK_COUNT 4 

// Assume NDAS_VID_DEFAULT if NDAS_VID_NONE

typedef enum _NDAS_VID {

	NDAS_VID_NONE			= 0x00,
	NDAS_VID_DEFAULT		= 0x01, 
	NDAS_VID_LINUX_ONLY		= 0x10,
	NDAS_VID_WINDWOS_RO		= 0x20,
	NDAS_VID_SEAGATE		= 0x41,
	NDAS_VID_AUTO_REGISTRY	= 0x60,
	NDAS_VID_PNP			= 0xFF,

} NDAS_VID;

/* <TITLE NDAS_DEVICE_ID> */
/* NDAS Device ID of the NDAS device hardware. 
   By default NDAS Device ID is a hardware MAC address.
*/

static BYTE ZeroNode[6] = {0};

typedef struct _NDAS_DEVICE_ID {

	BYTE Node[6];
	BYTE Reserved;
	BYTE Vid;			// vendor ID

} NDAS_DEVICE_ID, *PNDAS_DEVICE_ID;

C_ASSERT( sizeof(NDAS_DEVICE_ID) == 8 );

/* NDAS Unit Device Type */

typedef WORD NDAS_UNIT_TYPE, NDAS_UNITDEVICE_TYPE;

typedef enum _NDAS_UNIT_TYPE {

	NDAS_UNITDEVICE_TYPE_UNKNOWN		= 0x00,
	NDAS_UNITDEVICE_TYPE_DISK			= 0x01,
	NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK	= 0x02,
	NDAS_UNITDEVICE_TYPE_CDROM			= 0x10,
	NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY = 0x11,
};

/* NDAS Unit Disk Device Type */

typedef WORD NDAS_DISK_UNIT_TYPE, NDAS_UNITDEVICE_DISK_TYPE;

typedef enum _NDAS_DISK_TYPE {
	NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN       = 0x0000,
	NDAS_UNITDEVICE_DISK_TYPE_SINGLE        = 0x1000,
	NDAS_UNITDEVICE_DISK_TYPE_CONFLICT	    = 0x1001,
	NDAS_UNITDEVICE_DISK_TYPE_VIRTUAL_DVD   = 0x8000,
	NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED    = 0xA000,
	NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER = 0xB000,
	NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE  = 0xB100,
	NDAS_UNITDEVICE_DISK_TYPE_RAID0         = 0xC000,
	NDAS_UNITDEVICE_DISK_TYPE_RAID1         = 0xC001,
	NDAS_UNITDEVICE_DISK_TYPE_RAID4         = 0xC004,
	NDAS_UNITDEVICE_DISK_TYPE_RAID1_R2      = 0xC005,
	NDAS_UNITDEVICE_DISK_TYPE_RAID4_R2      = 0xC006,
	NDAS_UNITDEVICE_DISK_TYPE_RAID1_R3      = 0xC007,
	NDAS_UNITDEVICE_DISK_TYPE_RAID4_R3      = 0xC008,
	NDAS_UNITDEVICE_DISK_TYPE_RAID5         = 0xC009,
};

/* NDAS Unit CDROM Device Type */

typedef WORD NDAS_CDROM_UNIT_TYPE, NDAS_UNITDEVICE_CDROM_TYPE;

typedef enum _NDAS_CDROM_TYPE {
	NDAS_UNITDEVICE_CDROM_TYPE_UNKNOWN = 0x0000,
	NDAS_UNITDEVICE_CDROM_TYPE_CD = 0x0100,
	NDAS_UNITDEVICE_CDROM_TYPE_DVD = 0x0200,
};

/* NDAS Unit Optical Memory Device */
/* e.g. MO */

typedef WORD NDAS_UNITDEVICE_OPTICAL_MEMORY_TYPE;

typedef enum _NDAS_OPTICAL_MEMORY_TYPE {
	NDAS_UNITDEVICE_OPTICAL_MEMORY_TYPE_UNKNOWN = 0x0000,
	NDAS_UNITDEVICE_OPTICAL_MEMORY_TYPE_MO = 0x0001,
};

/* NDAS Compact Block Device */
/* e.g. Flash Card */

typedef WORD NDAS_UNITDEVICE_COMPACT_BLOCK_TYPE;

typedef enum _NDAS_UNITDEVICE_COMPACT_BLOCK_TYPE {
	NDAS_UNITDEVICE_COMPACT_BLOCK_TYPE_UNKNOWN		= 0x0000,
	NDAS_UNITDEVICE_COMPACT_BLOCK_TYPE_FLASHCARD	= 0x0001,
};

/* NDAS Unit Device Sub Type */

typedef union _NDAS_UNITDEVICE_SUBTYPE {
	NDAS_DISK_UNIT_TYPE DiskDeviceType;
	NDAS_UNITDEVICE_CDROM_TYPE CDROMDeviceType;
	NDAS_UNITDEVICE_OPTICAL_MEMORY_TYPE OptMemDevType;
	NDAS_UNITDEVICE_COMPACT_BLOCK_TYPE CompactDevType;
} NDAS_UNITDEVICE_SUBTYPE, *PNDAS_UNITDEVICE_SUBTYPE;

/* NDAS Device Status */

typedef DWORD NDAS_DEVICE_STATUS;

typedef enum _NDAS_DEVICE_STATUS {

	NDAS_DEVICE_STATUS_UNKNOWN			= 0x0000,
	NDAS_DEVICE_STATUS_NOT_REGISTERED	= 0x0010,
	NDAS_DEVICE_STATUS_OFFLINE			= 0x0020,
	NDAS_DEVICE_STATUS_ONLINE			= 0x0030,
	NDAS_DEVICE_STATUS_CONNECTING		= 0x0040
};

/* NDAS Device Alarm Status Flags. Keep in sync with values at ndasscsi.h, ndasscsiioctl.h */

typedef DWORD NDAS_DEVICE_ALARM_STATUSFLAGS;

typedef enum _NDAS_DEVICE_ALARM_FLAGS {
	NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING = 0x00000100,
	NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT      = 0x00000800,
	NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING        = 0x00001000,
	NDAS_DEVICE_ALARM_STATUSFLAG_ABNORMAL_TERMINAT = 0x00002000,
	NDAS_DEVICE_ALARM_STATUSFLAG_RAID_FAILURE      = 0x00010000,
	NDAS_DEVICE_ALARM_STATUSFLAG_RAID_NORMAL       = 0x00020000,
};

typedef enum _NDAS_DEVICE_ALARM_TYPE {
	NDAS_DEVICE_ALARM_NORMAL = 0,
	NDAS_DEVICE_ALARM_RECONNECTING = 1,
	NDAS_DEVICE_ALARM_RECONNECTED = 2,
	NDAS_DEVICE_ALARM_MEMBER_FAULT = 3,
	NDAS_DEVICE_ALARM_RECOVERING = 4,
	NDAS_DEVICE_ALARM_RECOVERED = 5,
	NDAS_DEVICE_ALARM_RAID_FAILURE = 6,
};

/* NDAS Device Error */

typedef DWORD NDAS_DEVICE_ERROR;

typedef enum _NDAS_DEVICE_ERROR {

	NDAS_DEVICE_ERROR_NONE                      = 0x0000,
	NDAS_DEVICE_ERROR_UNSUPPORTED_VERSION       = 0xFF10,
	NDAS_DEVICE_ERROR_LPX_SOCKET_FAILED         = 0xFF12,
	NDAS_DEVICE_ERROR_DISCOVER_FAILED           = 0xFF13,
	NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE = 0xFF14,
	NDAS_DEVICE_ERROR_FROM_SYSTEM               = 0xFF15,
	NDAS_DEVICE_ERROR_LOGIN_FAILED              = 0xFF16,
	NDAS_DEVICE_ERROR_CONNECTION_FAILED         = 0xFF17,
	NDAS_DEVICE_ERROR_GET_HARDWARE_INFO_FAILED  = 0x1,
	NDAS_DEVICE_ERROR_GET_HARDWARE_STAT_FAILED  = 0x2,
	NDAS_DEVICE_ERROR_GET_SOKADDR_LIST_FAILED   = 0x4,
};


/* <TITLE NDAS_DEVICE_REG_FLAGS> */
/* NDAS Device Registration Flags */

typedef enum _NDAS_DEVICE_REG_FLAGS {
	NDAS_DEVICE_REG_FLAG_NONE            = 0x00000000,
	/* Registration will be not preserved after reboot. */
	NDAS_DEVICE_REG_FLAG_VOLATILE        = 0x00000001,
	/* This NDAS device is not shown in the list. */
	NDAS_DEVICE_REG_FLAG_HIDDEN          = 0x00000002,
	/* NDAS OEM Code is set for this NDAS device. */
	NDAS_DEVICE_REG_FLAG_USE_OEM_CODE    = 0x00000004,
	/* Read-only Flags */
	/* This NDAS device is automatically registered. */
	NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED = 0x00010000,
};

/* NDAS Unit Device Status */

typedef DWORD NDAS_UNITDEVICE_STATUS;

typedef enum _NDAS_UNIT_STATUS {
	NDAS_UNITDEVICE_STATUS_UNKNOWN = 0x0000,
	NDAS_UNITDEVICE_STATUS_ERROR = 0x0000,
	NDAS_UNITDEVICE_STATUS_NOT_MOUNTED = 0x0020,
	NDAS_UNITDEVICE_STATUS_MOUNTED = 0x0030,
};

/* NDAS Unit Device Error */

typedef DWORD NDAS_UNITDEVICE_ERROR;

typedef enum _NDAS_UNIT_ERROR {
	NDAS_UNITDEVICE_ERROR_NONE                = 0x0000,
	NDAS_UNITDEVICE_ERROR_IDENTIFY_FAILURE    = 0x0011,
	NDAS_UNITDEVICE_ERROR_HDD_READ_FAILURE    = 0x0012,
	NDAS_UNITDEVICE_ERROR_HDD_UNKNOWN_LDTYPE  = 0x0013,
	NDAS_UNITDEVICE_ERROR_HDD_ECKEY_FAILURE   = 0x0014,
	NDAS_UNITDEVICE_ERROR_SEAGATE_RESTRICTION = 0x0015,
};

/* NDAS Logical Device ID */
/* NDAS Logical Device ID has a non-zero value if invalid. */

typedef DWORD NDAS_LOGICALDEVICE_ID, *PNDAS_LOGICALDEVICE_ID;

/* NULL value check function for NDAS Logical Device ID */
/* <COMBINE NDAS_LOGICALDEVICE_ID> */
#define INVALID_NDAS_LOGICALDEVICE_ID 0x0;

/* NDAS Location */
/* NDAS Location is a location identifier 
   which indicates where the NDAS logical device is located */
   
typedef DWORD NDAS_LOCATION, *PNDAS_LOCATION;

/* NDAS SCSI Location */
/* NDAS SCSI Location is a location identifier 
   which indicates where the NDAS logical device is located.
   NDAS_SCSI_LOCATION is deprecated. Use NDAS_LOCATION instead. */
   
typedef struct _NDAS_SCSI_LOCATION {
	DWORD SlotNo;
	DWORD TargetID;
	DWORD LUN;
} NDAS_SCSI_LOCATION, *PNDAS_SCSI_LOCATION;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(12 == sizeof(NDAS_SCSI_LOCATION));
/*DOM-IGNORE-END*/

#ifdef MSC_DEPRECATE_SUPPORTED
#pragma deprecated(NDAS_SCSI_LOCATION)
#endif

/* NDAS Logical Device Status */

typedef DWORD NDAS_LOGICALUNIT_STATUS;

typedef enum _NDAS_LOGICALUNIT_STATUS {
	NDAS_LOGICALUNIT_STATUS_UNKNOWN = 0,
	NDAS_LOGICALUNIT_STATUS_DISMOUNTED = 1,
	NDAS_LOGICALUNIT_STATUS_MOUNT_PENDING = 2,
	NDAS_LOGICALUNIT_STATUS_MOUNTED = 3,
	NDAS_LOGICALUNIT_STATUS_DISMOUNT_PENDING = 4,
	NDAS_LOGICALUNIT_STATUS_NOT_INITIALIZED = 0xFFFF,
};

typedef NDAS_LOGICALUNIT_STATUS NDAS_LOGICALDEVICE_STATUS;

#define NDAS_LOGICALDEVICE_STATUS_UNKNOWN         NDAS_LOGICALUNIT_STATUS_UNKNOWN
#define NDAS_LOGICALDEVICE_STATUS_UNMOUNTED       NDAS_LOGICALUNIT_STATUS_DISMOUNTED
#define NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING   NDAS_LOGICALUNIT_STATUS_MOUNT_PENDING
#define NDAS_LOGICALDEVICE_STATUS_MOUNTED         NDAS_LOGICALUNIT_STATUS_MOUNTED
#define NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING NDAS_LOGICALUNIT_STATUS_DISMOUNT_PENDING
#define NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED NDAS_LOGICALUNIT_STATUS_NOT_INITIALIZED

typedef enum _NDAS_LOGICALUNIT_ABNORMALITIES {
	NDAS_LOGICALUNIT_ABNORM_NONE = 0x00000000,
	NDAS_LOGICALUNIT_ABNORM_DISK_NOT_INITIALIZED = 1,
	NDAS_LOGICALUNIT_ABNORM_DISK_WITH_NO_DATA_PARTITION = 2,
	NDAS_LOGICALUNIT_ABNORM_DYNAMIC_DISK = 3,
} NDAS_LOGICALUNIT_ABNORMALITIES;

/* NDAS Logical Device Error */

typedef DWORD NDAS_LOGICALDEVICE_ERROR;

typedef enum _NDAS_LOGICALUNIT_ERROR {
	NDAS_LOGICALUNIT_ERROR_NONE = 0,
	NDAS_LOGICALUNIT_ERROR_MISSING_MEMBER = 0xFF30,
	NDAS_LOGICALUNIT_ERROR_REQURIE_UPGRADE = 0xFF31,
	/* This logical device is in conflicted status and need manual resolution using bindtool */
	NDAS_LOGICALUNIT_ERROR_REQUIRE_RESOLUTION = 0xFF32,
	/* This logical device is mountable in degraded mode. */
	NDAS_LOGICALUNIT_ERROR_DEGRADED_MODE = 0xFF33,
} NDAS_LOGICALUNIT_ERROR;

#define NDAS_LOGICALDEVICE_ERROR_NONE               NDAS_LOGICALUNIT_ERROR_NONE
#define NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER     NDAS_LOGICALUNIT_ERROR_MISSING_MEMBER
#define NDAS_LOGICALDEVICE_ERROR_REQUIRE_UPGRADE    NDAS_LOGICALUNIT_ERROR_REQURIE_UPGRADE
#define NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION NDAS_LOGICALUNIT_ERROR_REQUIRE_RESOLUTION
#define NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE      NDAS_LOGICALUNIT_ERROR_DEGRADED_MODE

/* NDAS Logical Device Type */

typedef DWORD NDAS_LOGICALDEVICE_TYPE;

typedef enum _NDAS_LOGICALDEVICE_TYPE {
	NDAS_LOGICALDEVICE_TYPE_UNKNOWN = 0x00,
	/* 	 NDAS_LOGICALDEVICE_TYPE_DISK = 0x10 */
	NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE = 0x11,
	NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED = 0x12,
	NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED = 0x13,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID0 = 0x14,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID1 = 0x15,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID4 = 0x16,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2 = 0x17,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2 = 0x18,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3 = 0x19,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3 = 0x1a,
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID5 = 0x1B,
	NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB = 0x1C,
	NDAS_LOGICALDEVICE_TYPE_DVD = 0x20,
	NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD = 0x2F,
	NDAS_LOGICALDEVICE_TYPE_MO = 0x30,
	NDAS_LOGICALDEVICE_TYPE_FLASHCARD = 0x40,
	NDAS_LOGICALDEVICE_TYPE_ATAPI = 0x80,
} _NDAS_LOGICALDEVICE_TYPE_ENUM;

#define NDAS_LOGICALDEVICE_TYPE_DISK_LAST NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB;

FORCEINLINE BOOL NdasIsLogicalDiskType(NDAS_LOGICALDEVICE_TYPE Type)
{
	return Type >= NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE && 
		Type <= NDAS_LOGICALDEVICE_TYPE_DISK_LAST;
}

FORCEINLINE BOOL NdasIsRaidDiskType(NDAS_LOGICALDEVICE_TYPE Type)
{
	return Type >= NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED &&
		Type <= NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB;
}

FORCEINLINE BOOL NdasIsCdromType(NDAS_LOGICALDEVICE_TYPE Type)
{
	return Type >= NDAS_LOGICALDEVICE_TYPE_DVD &&
		Type <= NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD;
}

#define IS_NDAS_LOGICALDEVICE_TYPE_DISK NdasIsLogicalDiskType
#define IS_NDAS_LOGICALDEVICE_TYPE_DISK_SET_GROUP NdasIsRaidDiskType
#define IS_NDAS_LOGICALDEVICE_TYPE_DVD_GROUP NdasIsCdromType

/* NDAS Logical Device Parameters */

typedef struct _NDAS_LOGICALDEVICE_PARAMS {

	DWORD CurrentMaxRequestBlocks;
	DWORD MountedLogicalDrives;
	DWORD Abnormalities;
	UCHAR Reserved[52];

} NDAS_LOGICALDEVICE_PARAMS, *PNDAS_LOGICALDEVICE_PARAMS;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(64 == sizeof(NDAS_LOGICALDEVICE_PARAMS));
/*DOM-IGNORE-END*/

/* NDAS Device Hardware Information
   
   See Also
   NDAS_LOGICALDEVICE_TYPE_DISK_RAID0, NdasCommConnect */

typedef struct _NDAS_DEVICE_HARDWARE_INFO
{
	/* Should be set as sizeof(NDAS_DEVICE_HARDWARE_INFO) */
	DWORD Size;
	/* Always 0 */
	DWORD HardwareType;
	/* NDAS device hardware version 0 for 1.0, 1 for 1.1 and 2 for 2.0 */
	DWORD HardwareVersion;
	/* NDAS device hardware revision. 0 for 1.0, 1.1, 2.0. 0x10 for 2.0G. 0x18 for 2.0G 100M ver*/
	DWORD HardwareRevision;
	/* Number of Command Processing Slots */
	DWORD NumberOfCommandProcessingSlots;
	/* Number of targets is not available in this structure. */
	/* Use GetDeviceStats instead */
	/* (deprecated) DWORD NumberOfTargets; */
	DWORD reserved1;
	/* Maximum number of targets */
	DWORD MaximumNumberOfTargets;
	/* Maximum LUs of targets */
	DWORD MaximumNumberOfLUs;
	/* Maximum Number of Transfer Blocks */
	DWORD MaximumTransferBlocks;
	/* Protocol Type, always 0*/
	DWORD ProtocolType;
	/* Protocol Version 0 for 1.0, 1 for 1.1 and 2.0 */
	DWORD ProtocolVersion;
	/* Header Encryption Mode */
	DWORD HeaderEncryptionMode;
	/* Header Encryption Mode */
	DWORD HeaderDigestMode;
	/* Data Encryption Mode */
	DWORD DataEncryptionMode;
	/* Data Encryption Mode */
	DWORD DataDigestMode;
	/* NDAS Device ID (may be filled with zeros) */
	NDAS_DEVICE_ID NdasDeviceId;
	/* Reserved */
	BYTE reserved[4];
} NDAS_DEVICE_HARDWARE_INFO, *PNDAS_DEVICE_HARDWARE_INFO;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(72 == sizeof(NDAS_DEVICE_HARDWARE_INFO));
/*DOM-IGNORE-END*/

/* NDAS Unit Device Hardware Information */

typedef WORD NDAS_UNITDEVICE_MEDIA_TYPE;

enum _NDAS_UNIT_TYPE_DETAIL {
	NDAS_UNIT_UNKNOWN_DEVICE = 0,
	NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE = 1,
	NDAS_UNIT_ATAPI_DIRECT_ACCESS_DEVICE = 0x8000,
	NDAS_UNIT_ATAPI_SEQUENTIAL_ACCESS_DEVICE = 0x8001,
	NDAS_UNIT_ATAPI_PRINTER_DEVICE = 0x8002,
	NDAS_UNIT_ATAPI_PROCESSOR_DEVICE = 0x8003,
	NDAS_UNIT_ATAPI_WRITE_ONCE_DEVICE = 0x8004,
	NDAS_UNIT_ATAPI_CDROM_DEVICE = 3, /* 0x8005 */
	NDAS_UNIT_ATAPI_SCANNER_DEVICE = 0x8006,
	NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE = 4,/* 0x8007 */
	NDAS_UNIT_ATAPI_MEDIUM_CHANGER_DEVICE = 0x8008,
	NDAS_UNIT_ATAPI_COMMUNICATIONS_DEVICE = 0x8009,
	NDAS_UNIT_ATAPI_ARRAY_CONTROLLER_DEVICE = 0x800C,
	NDAS_UNIT_ATAPI_ENCLOSURE_SERVICES_DEVICE = 0x800D,
	NDAS_UNIT_ATAPI_REDUCED_BLOCK_COMMAND_DEVICE = 0x800E,
	NDAS_UNIT_ATAPI_OPTICAL_CARD_READER_WRITER_DEVICE = 0x800F,
};

#define NDAS_UNITDEVICE_MEDIA_TYPE_UNKNOWN_DEVICE NDAS_UNIT_UNKNOWN_DEVICE
#define NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE 
#define NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE NDAS_UNIT_ATAPI_CDROM_DEVICE 
#define NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE 

/* <TITLE NDAS_UNITDEVICE_STAT> */

typedef struct _NDAS_UNITDEVICE_STAT {

	// Size of the structure, set as sizeof(NDAS_UNITDEVICE_STAT)

	DWORD Size;
	
	// Non-zero if present, zero otherwise

	BOOL  IsPresent;

	DWORD RwConnectionCount;
	DWORD RoConnectionCount;

	// Number of hosts count which is connected to the NDAS Device with read write access

	DWORD RwHostCount;
	
	// Number of hosts count which is connected to the NDAS Device with read only access

	DWORD RoHostCount;

	// Reserved

	BYTE  TargetData[8];

} NDAS_UNITDEVICE_STAT, *PNDAS_UNITDEVICE_STAT;

/*DOM-IGNORE-BEGIN*/

C_ASSERT( sizeof(NDAS_UNITDEVICE_STAT) == 32 );

/*DOM-IGNORE-END*/

/* Maximum number of unit devices in a NDAS device */

#define NDAS_MAX_UNITDEVICES 2

/* <TITLE NDAS_UNITDEVICE_STAT> */

typedef struct _NDAS_DEVICE_STAT {

	// Size of the structure, set as sizeof(NDAS_DEVICE_STAT)

	DWORD Size;
	
	// Number of unit device(s) which is attached to the NDAS Device

	DWORD NumberOfUnitDevices;
	
	// Unit Device Data

	NDAS_UNITDEVICE_STAT UnitDevices[NDAS_MAX_UNITDEVICES];

} NDAS_DEVICE_STAT, *PNDAS_DEVICE_STAT;

/*DOM-IGNORE-BEGIN*/

C_ASSERT( sizeof(NDAS_DEVICE_STAT) == 72 );

/*DOM-IGNORE-END*/

/* <TITLE NDAS_UNITDEVICE_HARDWARE_INFO> */

typedef struct _NDAS_UNITDEVICE_HARDWARE_INFOW
{
	DWORD Size;
	BOOL  LBA      : 1;
	BOOL  LBA48    : 1;
	BOOL  PIO      : 1;
	BOOL  DMA      : 1;
	BOOL  UDMA     : 1;
	BOOL  _align_1 : 27;
	NDAS_UNITDEVICE_MEDIA_TYPE MediaType;
	WORD  _align_2[3];
	WCHAR Model[40 + 8];
	WCHAR FirmwareRevision[8 + 8];
	WCHAR SerialNumber[20 + 4];
	ULARGE_INTEGER SectorCount;
} NDAS_UNITDEVICE_HARDWARE_INFOW, *PNDAS_UNITDEVICE_HARDWARE_INFOW;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(200 == sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));
/*DOM-IGNORE-END*/

/* <COMBINE NDAS_UNITDEVICE_HARDWARE_INFOW> */
typedef struct _NDAS_UNITDEVICE_HARDWARE_INFOA
{
	DWORD Size;
	BOOL  LBA      : 1;
	BOOL  LBA48    : 1;
	BOOL  PIO      : 1;
	BOOL  DMA      : 1;
	BOOL  UDMA     : 1;
	BOOL  _align_1 : 27;
	NDAS_UNITDEVICE_MEDIA_TYPE MediaType;
	WORD  _align_2[3];
	CHAR  Model[40 + 8];
	CHAR  FirmwareRevision[8 + 8];
	CHAR  SerialNumber[20 + 4];
	ULARGE_INTEGER SectorCount;
} NDAS_UNITDEVICE_HARDWARE_INFOA, *PNDAS_UNITDEVICE_HARDWARE_INFOA;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(112 == sizeof(NDAS_UNITDEVICE_HARDWARE_INFOA));
/*DOM-IGNORE-END*/

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDAS_UNITDEVICE_HARDWARE_INFO NDAS_UNITDEVICE_HARDWARE_INFOW
#define PNDAS_UNITDEVICE_HARDWARE_INFO PNDAS_UNITDEVICE_HARDWARE_INFOW
#else
#define NDAS_UNITDEVICE_HARDWARE_INFO NDAS_UNITDEVICE_HARDWARE_INFOA
#define PNDAS_UNITDEVICE_HARDWARE_INFO PNDAS_UNITDEVICE_HARDWARE_INFOA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NDAS_DEVICE_PARAMS>

NDAS Device Parameters 

*/

typedef struct _NDAS_DEVICE_PARAMS
{
	/* Registration Flags */
	DWORD RegFlags;
	BYTE Reserved[60];
} NDAS_DEVICE_PARAMS, *PNDAS_DEVICE_PARAMS;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(64 == sizeof(NDAS_DEVICE_PARAMS));
/*DOM-IGNORE-END*/

/* <TITLE NDAS_UNITDEVICE_PARAMS>

NDAS Unit Device Parameters 

*/
typedef struct _NDAS_UNITDEVICE_PARAMS
{
	BYTE Reserved[64];
} NDAS_UNITDEVICE_PARAMS, *PNDAS_UNITDEVICE_PARAMS;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(64 == sizeof(NDAS_UNITDEVICE_PARAMS));
/*DOM-IGNORE-END*/

/* <TITLE NDAS_OEM_CODE>

NDAS OEM Code is a 8-byte array of bytes.
I64Value is provided as an union. 
It is recommended not to use I64Value but use Bytes fields.
Existing I64Value should be translated Bytes, 
considering endian of the original development target system.
*/

typedef union _NDAS_OEM_CODE
{
	BYTE Bytes[8];
	UINT64 UI64Value;
	ULARGE_INTEGER Value;
} NDAS_OEM_CODE, *PNDAS_OEM_CODE;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(8 == sizeof(NDAS_OEM_CODE));
/*DOM-IGNORE-END*/

/* <TITLE NDAS_DEVICE_REGISTRATION>
*/

typedef struct _NDAS_DEVICE_REGISTRATIONW
{
	/* Size of the structure, set as sizeof(NDAS_DEVICE_REGISTRATION) */
	DWORD         Size;
	/* Registration Flags. See NDAS_DEVICE_REG_FLAGS */
	DWORD         RegFlags;
	/* Device String ID, composed of NDAS_DEVICE_ID_KEY_LEN (20) chars */
	LPCWSTR       DeviceStringId;
	/* Device String Key, composed of NDAS_DEVICE_WRITE_KEY_LEN (5) chars */
	LPCWSTR       DeviceStringKey;
	/* Device Name, up to MAX_NDAS_DEVICE_NAME_LEN chars */
	LPCWSTR       DeviceName;
	/* Alignment */
#ifndef _WIN64
	DWORD         Reserved;
#endif
	/* OEM Code for the device. 
	   Valid only if NDAS_DEVICE_REG_FLAG_USE_OEM_CODE is set in RegFlags */
	NDAS_OEM_CODE OEMCode;
} NDAS_DEVICE_REGISTRATIONW, *PNDAS_DEVICE_REGISTRATIONW;


/*DOM-IGNORE-BEGIN*/
#ifdef _WIN64
C_ASSERT(40 == sizeof(NDAS_DEVICE_REGISTRATIONW));
#else
C_ASSERT(32 == sizeof(NDAS_DEVICE_REGISTRATIONW));
#endif
/*DOM-IGNORE-END*/

/*<COMBINE NDAS_DEVICE_REGISTRATION>*/
typedef struct _NDAS_DEVICE_REGISTRATIONA
{
	DWORD         Size;
	DWORD         RegFlags;
	LPCSTR        DeviceStringId;
	LPCSTR        DeviceStringKey;
	LPCSTR        DeviceName;
#ifndef _WIN64
	DWORD         Reserved;
#endif
	NDAS_OEM_CODE OEMCode;
} NDAS_DEVICE_REGISTRATIONA, *PNDAS_DEVICE_REGISTRATIONA;

/*DOM-IGNORE-BEGIN*/
#ifdef _WIN64
C_ASSERT(40 == sizeof(NDAS_DEVICE_REGISTRATIONA));
#else
C_ASSERT(32 == sizeof(NDAS_DEVICE_REGISTRATIONA));
#endif
/*DOM-IGNORE-END*/

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDAS_DEVICE_REGISTRATION NDAS_DEVICE_REGISTRATIONW
#else
#define NDAS_DEVICE_REGISTRATION NDAS_DEVICE_REGISTRATIONA
#endif
/*DOM-IGNORE-END*/

/*DOM-IGNORE-BEGIN*/
typedef enum _NDAS_LOGICALUNIT_ADD_FLAGS {
	//NDAS_LDPF_PSWRITESHARE = 0x00000001,
	//NDAS_LDPF_FAKE_WRITE = 0x00000002,
	//NDAS_LDPF_OOB_ACCESS = 0x00000004,
	//NDAS_LDPF_LOCKED_WRITE = 0x00000010,
	NDAS_LDPF_SIMULTANEOUS_WRITE = 0x00000020,
	NDAS_LDPF_OUTOFBOUND_WRITE = 0x00000040,
	NDAS_LDPF_NDAS_2_0_WRITE_CHECK = 0x00000080,
	NDAS_LDPF_DYNAMIC_REQUEST_SIZE = 0x00000100,
};
/*DOM-IGNORE-END*/

/* 

NDAS Logical Device Plug-in Parameter options.

For LdpfFlags and LdpfValues, following values are defined.

Bit                            Description                              Default
======================         ===========                              =======
NDAS_LDPF_PSWRITESHARE         Use PS write share feature.              ON  (Deprecated)
NDAS_LDPF_FAKE_WRITE           Use fake write feature in ROFilter mode  ON  (Deprecated)
NDAS_LDPF_OOB_ACCESS                                                        (Deprecated)
NDAS_LDPF_LOCKED_WRITE         Use Locked Write                         OFF (Deprecated)
NDAS_LDPF_SIMULTANEOUS_WRITE   Enable simultaneous write in all regions OFF
NDAS_LDPF_OUTOFBOUND_WRITE     Enable write access in OOB regions regardless of device access mode.
                                                                        OFF
NDAS_LDPF_NDAS_2_0_WRITE_CHECK Enable write check workaround for NDAS chip 2.0
                                                                        ON

To set a feature (enable or disable), 
both LdpfFlags and LdpfValues field should be set.
If LdpfFlags are not set for each NDAS_LDPF_XXX, default value is used.

Returns:

If the function succeeds, the return value is non-zero.
If the function fails, the return value is zero. To get
extended error information, call GetLastError.

Example:

<CODE>
//
// This example shows how to enable NDAS_LDPF_OUTOFBOUND_WRITE feature
// when plugging in a NDAS logical device.
//

NDAS_LOGICALDEVICE_PLUGIN_PARAM param;
ZeroMemory(&param, sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM));

param.Size = sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM);

// or other logical device id 
param.LogicalDeviceId = 1; 

// Enable RW mode
param.Access = GENERIC_READ | GENERIC_WRITE; 

// To enable NDAS_LDPF_OUTOFBOUND_WRITE, NDAS_LDPF_LOCKED_WRITE 
// also must be specified.
param.LdpfFlags = NDAS_LDPF_LOCKED_WRITE | NDAS_LDPF_OUTOFBOUND_WRITE;

// The following line will turn on LOCKED_WRITE and OOB_SHARED_WRITE
param.LdpfValues |= NDAS_LDPF_LOCKED_WRITE;
param.LdpfValues |= NDAS_LDPF_OUTOFBOUND_WRITE;

// To turn off OOB_SHARED_WRITE
// param.LdpfValues &= ~(NDAS_LDPF_LOCKED_WRITE);

</CODE>

See also:
NdasPlugInLogicalDeviceEx
*/

typedef enum _NDAS_LOGICALUNIT_PLUGIN_FLAGS {
	NDAS_LOGICALUNIT_PLUGIN_FLAGS_NONE = 0x00000000,
	NDAS_LOGICALUNIT_PLUGIN_FLAGS_IGNORE_WARNINGS = 0x00000001,
};

typedef struct _NDAS_LOGICALDEVICE_PLUGIN_PARAM {
	/* Size of the structure, set to NDAS_LOGICALDEVICE_PLUGIN_PARAM */
	DWORD Size;
	/* Logical Device ID */
	NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	/* Plug-in Access, Use GENERIC_READ, or GENERIC_READ | GENERIC_WRITE */
	ACCESS_MASK Access;
	/* Specify the NDAS_LDPX_XXXX bit is valid in LdpfValues */
	DWORD LdpfFlags;
	/* Options for NDAS_LDPX_XXXX options. Bits set from LdpfFlags are valid.*/
	DWORD LdpfValues;
	/* Flags See _NDAS_LOGICALUNIT_PLUGIN_FLAGS */
	DWORD PlugInFlags;
} NDAS_LOGICALDEVICE_PLUGIN_PARAM, *PNDAS_LOGICALDEVICE_PLUGIN_PARAM;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(24 == sizeof(NDAS_LOGICALDEVICE_PLUGIN_PARAM));
/*DOM-IGNORE-END*/

typedef struct _NDAS_LOGICALDEVICE_EJECT_PARAMW {
	/* Size of the structure, set to NDAS_LOGICALDEVICE_EJECT_PARAM */
	DWORD Size;
	/* Logical Device ID */
	NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	/* Returned Result for ConfigRet */
	CONFIGRET ConfigRet;
	/* VetoType is set when ConfigRet is CR_REMOVE_VETOED */
	PNP_VETO_TYPE VetoType;
	/* VetoType-specific VetoName */
	WCHAR VetoName[MAX_PATH];
} NDAS_LOGICALDEVICE_EJECT_PARAMW, *PNDAS_LOGICALDEVICE_EJECT_PARAMW;

/*<COMBINE NDAS_LOGICALDEVICE_PLUGIN_PARAMW>*/
typedef struct _NDAS_LOGICALDEVICE_EJECT_PARAMA {
	/* Size of the structure, set to NDAS_LOGICALDEVICE_EJECT_PARAM */
	DWORD Size;
	/* Logical Device ID */
	NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	/* Returned Result for ConfigRet */
	CONFIGRET ConfigRet;
	/* VetoType is set when ConfigRet is CR_REMOVE_VETOED */
	PNP_VETO_TYPE VetoType;
	/* VetoType-specific VetoName */
	WCHAR VetoName[MAX_PATH];
} NDAS_LOGICALDEVICE_EJECT_PARAMA, *PNDAS_LOGICALDEVICE_EJECT_PARAMA;

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDAS_LOGICALDEVICE_EJECT_PARAM NDAS_LOGICALDEVICE_EJECT_PARAMW
#else
#define NDAS_LOGICALDEVICE_EJECT_PARAM NDAS_LOGICALDEVICE_EJECT_PARAMA
#endif
/*DOM-IGNORE-END*/

/* Specific access rights used in ACCESS_MASK.
   Valid only when NDAS_ACCESS_BIT_EXTENDED is set */

typedef enum _NDAS_ACCESS_BITS {
	NDAS_ACCESS_BIT_EXTENDED             = 0x8000,
	NDAS_ACCESS_BIT_EXCLUSIVE_WRITE      = 0x0000,
	NDAS_ACCESS_BIT_SHARED_WRITE         = 0x0001,
	NDAS_ACCESS_BIT_SHARED_WRITE_PRIMARY = 0x0002,
} NDAS_ACCESS_BITS;

/* End of packing */
#include <poppack.h>

#endif /* _NDAS_TYPE_H_ */
