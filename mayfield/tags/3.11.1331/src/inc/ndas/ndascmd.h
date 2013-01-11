/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <windows.h>
#include <basetsd.h>
#include <cfgmgr32.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndashostinfo.h>
#include <ndas/ndassvcparam.h>
#include <ndas/ndasid.h>

#include <pshpack8.h>

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_TYPE
//
//////////////////////////////////////////////////////////////////////////

#define NDAS_CMD_TYPE_NONE                              0x00

#define NDAS_CMD_TYPE_REGISTER_DEVICE                   0x1A
#define NDAS_CMD_TYPE_UNREGISTER_DEVICE                 0x1B
#define NDAS_CMD_TYPE_REGISTER_DEVICE_V2                0x1C
#define NDAS_CMD_TYPE_ENUMERATE_DEVICES                 0x11
#define NDAS_CMD_TYPE_SET_DEVICE_PARAM                  0x12
#define NDAS_CMD_TYPE_QUERY_DEVICE_STATUS               0x13
#define NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION          0x14
#define NDAS_CMD_TYPE_QUERY_DEVICE_STAT                 0x15
#define NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION_V2       0x16
#define NDAS_CMD_TYPE_ENUMERATE_DEVICES_V2              0x17

#define NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES             0x21
#define NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM              0x22
#define NDAS_CMD_TYPE_QUERY_UNITDEVICE_STATUS           0x23
#define NDAS_CMD_TYPE_QUERY_UNITDEVICE_INFORMATION      0x24
#define NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT       0x25
#define NDAS_CMD_TYPE_QUERY_UNITDEVICE_STAT             0x26
#define NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE  0x29

#define NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES          0x31
#define NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM           0x32
#define NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS        0x33
#define NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION   0x34
#define NDAS_CMD_TYPE_QUERY_NDAS_SCSI_LOCATION          0x35
#define NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_VOLUMES       0x36
#define NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION_V2 0x37

#define NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE              0x41
#define NDAS_CMD_TYPE_EJECT_LOGICALDEVICE               0x42
#define NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE              0x43
// Obsolete
//#define   NDAS_CMD_TYPE_RECOVER_LOGICALDEVICE           0x44
#define NDAS_CMD_TYPE_EJECT_LOGICALDEVICE_2             0x45

#define NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE             0x51
#define NDAS_CMD_TYPE_QUERY_HOST_LOGICALDEVICE          0x52
#define NDAS_CMD_TYPE_QUERY_HOST_INFO                   0x53

#define NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS          0x61

#define NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE          0x71
#define NDAS_CMD_TYPE_NOTIFY_DEVICE_CHANGE              0x72

#define NDAS_CMD_TYPE_SET_SERVICE_PARAM                 0x91
#define NDAS_CMD_TYPE_GET_SERVICE_PARAM                 0x92

typedef WORD NDAS_CMD_TYPE;

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_STATUS
//
//////////////////////////////////////////////////////////////////////////

#define NDAS_CMD_STATUS_REQUEST               0xFF
#define NDAS_CMD_STATUS_SUCCESS               0x00
#define NDAS_CMD_STATUS_FAILED                0x80
#define NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED 0x81
#define NDAS_CMD_STATUS_INVALID_REQUEST       0x82
#define NDAS_CMD_STATUS_TERMINATION           0x83
#define NDAS_CMD_STATUS_UNSUPPORTED_VERSION   0x8F

typedef WORD NDAS_CMD_STATUS;

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_HEADER
//
//////////////////////////////////////////////////////////////////////////

const CHAR NDAS_CMD_PROTOCOL[4] = {'N', 'D', 'C', 'P'};
const UCHAR NDAS_CMD_PROTOCOL_VERSION_MAJOR = 0x02;
const UCHAR NDAS_CMD_PROTOCOL_VERSION_MINOR = 0x00;

struct NDAS_CMD_HEADER 
{
    CHAR    Protocol[4];    // 4
    UCHAR   VersionMajor;   // 1
    UCHAR   VersionMinor;   // 1
    WORD    Command;        // 2
    WORD    Status;         // 2
    WORD    TransactionId;  // 2
    DWORD   MessageSize;    // 4
};

C_ASSERT(16 == sizeof(NDAS_CMD_HEADER));

typedef NDAS_CMD_HEADER* PNDAS_CMD_HEADER;

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_ERROR
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_ERROR 
{
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD dwErrorCode;
        DWORD dwDataLength;
        LPVOID lpData[];
    };
#pragma warning(default: 4200)
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_NONE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_NONE
{
    enum { CMD = NDAS_CMD_TYPE_NONE };
    struct REQUEST 
    {
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_REGISTER_DEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_REGISTER_DEVICE
{
    enum { CMD = NDAS_CMD_TYPE_REGISTER_DEVICE };
    struct REQUEST 
    {
        NDAS_DEVICE_ID DeviceId;
        WCHAR          wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
        ACCESS_MASK    GrantingAccess;
        DWORD          RegFlags; /* defined at ndastype.h NDAS_DEVICE_REG_FLAG_XXX*/
        /* OEM Code is valid only if NDAS_DEVICE_REG_FLAG_USE_OEM_CODE is set */
        NDAS_OEM_CODE  OEMCode;
    };
    struct REPLY 
    {
        DWORD SlotNo;
    };
};

struct NDAS_CMD_REGISTER_DEVICE_V2
{
    enum { CMD = NDAS_CMD_TYPE_REGISTER_DEVICE_V2 };
    struct REQUEST 
    {
        NDAS_DEVICE_ID  DeviceId;
        WCHAR           wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
        ACCESS_MASK     GrantingAccess;
        DWORD           RegFlags; /* defined at ndastype.h NDAS_DEVICE_REG_FLAG_XXX*/
        /* OEM Code is valid only if NDAS_DEVICE_REG_FLAG_USE_OEM_CODE is set */
        NDAS_OEM_CODE   OEMCode;
		NDASID_EXT_DATA NdasIdExtension;
    };
    struct REPLY 
    {
        DWORD SlotNo;
    };
};

//typedef struct _NDAS_CMD_REGISTER_DEVICE_V2_REQUEST {
//	NDAS_DEVICE_ID DeviceId;
//	WCHAR          wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
//	ACCESS_MASK    GrantingAccess;
//	DWORD          RegFlags; /* defined at ndastype.h NDAS_DEVICE_REG_FLAG_XXX*/
//	/* OEM Code is valid only if NDAS_DEVICE_REG_FLAG_USE_OEM_CODE is set */
//	NDAS_OEM_CODE  OEMCode;
//	DWORD          DeviceRestrictionType;
//	DWORD          DeviceRestrictionDataOffset; /* Offset from the start */
//	DWORD          AdditionalDataLength;
//} NDAS_CMD_REGISTER_DEVICE_V2_REQUEST, *PNDAS_CMD_REGISTER_DEVICE_V2_REQUEST;
//
//typedef struct _NDAS_CMD_REGISTER_DEVICE_V2_REPLY {
//	DWORD SlotNumber;
//} NDAS_CMD_REGISTER_DEVICE_V2_REPLY, *PNDAS_CMD_REGISTER_DEVICE_V2_REPLY;

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_UNREGISTER_DEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_UNREGISTER_DEVICE
{
    enum { CMD = NDAS_CMD_TYPE_UNREGISTER_DEVICE};
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_ENUMERATE_DEVICES
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_ENUMERATE_DEVICES
{
    typedef struct _ENUM_ENTRY {
        NDAS_DEVICE_ID DeviceId;
        DWORD SlotNo;
        WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
        ACCESS_MASK GrantedAccess;
    } ENUM_ENTRY, *PENUM_ENTRY;

    enum { CMD = NDAS_CMD_TYPE_ENUMERATE_DEVICES};
    struct REQUEST 
    {
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD nDeviceEntries;
        ENUM_ENTRY DeviceEntry[];
    };
#pragma warning(default: 4200)

};

struct NDAS_CMD_ENUMERATE_DEVICES_V2
{
    typedef struct _ENUM_ENTRY {
        NDAS_DEVICE_ID DeviceId;
        DWORD SlotNo;
        WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
        ACCESS_MASK GrantedAccess;
		NDASID_EXT_DATA NdasIdExtension;
    } ENUM_ENTRY, *PENUM_ENTRY;

    enum { CMD = NDAS_CMD_TYPE_ENUMERATE_DEVICES_V2 };
    struct REQUEST 
    {
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD nDeviceEntries;
        ENUM_ENTRY DeviceEntry[];
    };
#pragma warning(default: 4200)

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_SET_DEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_NONE     0x00
#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_ENABLE   0x01
#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME     0x02
#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS   0x04

typedef DWORD NDAS_CMD_SET_DEVICE_PARAM_TYPE;

typedef struct NDAS_CMD_SET_DEVICE_PARAM_DATA {
    NDAS_CMD_SET_DEVICE_PARAM_TYPE ParamType;
    union 
    {
        BOOL    bEnable;
        ACCESS_MASK GrantingAccess;
        WCHAR   wszName[MAX_NDAS_DEVICE_NAME_LEN + 1];
    };
} NDAS_CMD_SET_DEVICE_PARAM_DATA, *PNDAS_CMD_SET_DEVICE_PARAM_DATA;

struct NDAS_CMD_SET_DEVICE_PARAM 
{
    enum { CMD = NDAS_CMD_TYPE_SET_DEVICE_PARAM};
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        NDAS_CMD_SET_DEVICE_PARAM_DATA Param;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_SET_UNITDEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

#define NDAS_CMD_SET_UNITDEVICE_PARAM_TYPE_NONE     0x00

typedef DWORD NDAS_CMD_SET_UNITDEVICE_PARAM_TYPE;

typedef struct NDAS_CMD_SET_UNITDEVICE_PARAM_DATA {
    NDAS_CMD_SET_UNITDEVICE_PARAM_TYPE ParamType;
    union 
    {
        BYTE Reserved[28];
    };
} NDAS_CMD_SET_UNITDEVICE_PARAM_DATA, *PNDAS_CMD_SET_UNITDEVICE_PARAM_DATA;

struct NDAS_CMD_SET_UNITDEVICE_PARAM 
{
    enum { CMD = NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM};
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD   dwUnitNo;
        NDAS_CMD_SET_UNITDEVICE_PARAM_DATA Param;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_SET_LOGICALDEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

typedef DWORD NDAS_CMD_SET_LOGICALDEVICE_PARAM_TYPE;
typedef struct _NDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA {
    NDAS_CMD_SET_LOGICALDEVICE_PARAM_TYPE ParamType;
    union 
    {
        BYTE Reserved[28];
    };
} NDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA, *PNDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA;

struct NDAS_CMD_SET_LOGICALDEVICE_PARAM 
{
    enum { CMD = NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM};
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
        NDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA Param;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_DEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_DEVICE_INFORMATION
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
    };
    struct REPLY 
    {
        NDAS_DEVICE_ID DeviceId;
        DWORD SlotNo;
        WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN];
        ACCESS_MASK GrantedAccess;
        NDAS_DEVICE_HARDWARE_INFO HardwareInfo;
        NDAS_DEVICE_PARAMS DeviceParams;
    };
};

struct NDAS_CMD_QUERY_DEVICE_INFORMATION_V2
{
	enum { CMD = NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION_V2 };
	struct REQUEST 
	{
		NDAS_DEVICE_ID_EX DeviceIdOrSlot;
	};
	struct REPLY 
	{
		NDAS_DEVICE_ID DeviceId;
		DWORD SlotNo;
		WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN];
		ACCESS_MASK GrantedAccess;
		NDAS_DEVICE_HARDWARE_INFO HardwareInfo;
		NDAS_DEVICE_PARAMS DeviceParams;
		NDASID_EXT_DATA NdasIdExtension;
	};
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_DEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_DEVICE_STATUS 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_DEVICE_STATUS };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
    };
    struct REPLY 
    {
        NDAS_DEVICE_STATUS Status;
        NDAS_DEVICE_ERROR LastError;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_ENUMERATE_UNITDEVICES
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_ENUMERATE_UNITDEVICES
{
    typedef struct _ENUM_ENTRY 
    {
        DWORD UnitNo;
        NDAS_UNITDEVICE_TYPE UnitDeviceType;
    } ENUM_ENTRY, *PENUM_ENTRY;

    enum { CMD = NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD nUnitDeviceEntries;
        ENUM_ENTRY UnitDeviceEntries[];
    };
#pragma warning(default: 4200)
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_UNITDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_UNITDEVICE_INFORMATION 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_UNITDEVICE_INFORMATION };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
    struct REPLY 
    {
        NDAS_UNITDEVICE_TYPE    UnitDeviceType;
        NDAS_UNITDEVICE_SUBTYPE UnitDeviceSubType;
        NDAS_UNITDEVICE_HARDWARE_INFOW HardwareInfo;
        NDAS_UNITDEVICE_STAT    Stats;
        NDAS_UNITDEVICE_PARAMS  UnitDeviceParams;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_UNITDEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_UNITDEVICE_STATUS
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_UNITDEVICE_STATUS };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
    struct REPLY 
    {
        NDAS_UNITDEVICE_STATUS Status;
        NDAS_UNITDEVICE_ERROR LastError;
    };
};

struct NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
    struct REPLY 
    {
        DWORD nROHosts;
        DWORD nRWHosts;
    };
};
//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_FIND_LOGICALDEVICE_OF_UNITDEVICE
{
    enum { CMD = NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
    struct REPLY 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_ENUMERATE_LOGICALDEVICES
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_ENUMERATE_LOGICALDEVICES
{
    typedef struct _ENUM_ENTRY {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
        NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
    } ENUM_ENTRY, *PENUM_ENTRY;

    enum { CMD = NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES };
    struct REQUEST 
    {
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD nLogicalDeviceEntries;
        ENUM_ENTRY LogicalDeviceEntries[];
    };
#pragma warning(default: 4200)
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_PLUGIN_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_PLUGIN_LOGICALDEVICE 
{
    enum { CMD = NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_PLUGIN_PARAM LpiParam;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_EJECT_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_EJECT_LOGICALDEVICE 
{
    enum { CMD = NDAS_CMD_TYPE_EJECT_LOGICALDEVICE };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
    struct REPLY 
    {
    };
};

struct NDAS_CMD_EJECT_LOGICALDEVICE_2 
{
    enum { CMD = NDAS_CMD_TYPE_EJECT_LOGICALDEVICE_2 };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
    struct REPLY 
    {
        CONFIGRET ConfigRet;
        PNP_VETO_TYPE VetoType;
        WCHAR VetoName[MAX_PATH];
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_UNPLUG_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_UNPLUG_LOGICALDEVICE 
{
    enum { CMD = NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_LOGICALDEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_LOGICALDEVICE_STATUS 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
    struct REPLY
    {
        NDAS_LOGICALDEVICE_STATUS Status;
        NDAS_LOGICALDEVICE_ERROR LastError;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION 
{
    DWORD Blocks;
    BYTE _align_1[4];
    struct 
    {
        DWORD Revision;
        WORD  Type;
        WORD  KeyLength;
    } ContentEncrypt;
    BYTE _align_2[16];
} NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION, *PNDAS_CMD_LOGICALDEVICE_DISK_INFORMATION;

C_ASSERT(32 == sizeof(NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION) );

struct NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION 
{
    typedef struct _UNITDEVICE_ENTRY 
    {
        NDAS_DEVICE_ID DeviceId;
        DWORD UnitNo;
    } UNITDEVICE_ENTRY, *PUNITDEVICE_ENTRY;

    enum { CMD = NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
        ACCESS_MASK GrantedAccess;
        ACCESS_MASK MountedAccess;
        union 
        {
            NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION LogicalDiskInfo;
            BYTE Reserved[64]; // 64 byte reserved for other usage
        };
        NDAS_LOGICALDEVICE_PARAMS LogicalDeviceParams;
        DWORD nUnitDeviceEntries;
        UNITDEVICE_ENTRY UnitDeviceEntries[];
    };
#pragma warning(default: 4200)
};

struct NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION_V2
{
	typedef struct _UNITDEVICE_ENTRY 
	{
		NDAS_DEVICE_ID DeviceId;
		DWORD UnitNo;
		NDASID_EXT_DATA NdasIdExtension;
	} UNITDEVICE_ENTRY, *PUNITDEVICE_ENTRY;

	enum { CMD = NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION_V2 };
	struct REQUEST 
	{
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};
#pragma warning(disable: 4200)
	struct REPLY 
	{
		NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
		ACCESS_MASK GrantedAccess;
		ACCESS_MASK MountedAccess;
		union 
		{
			NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION LogicalDiskInfo;
			BYTE Reserved[64]; // 64 byte reserved for other usage
		};
		NDAS_LOGICALDEVICE_PARAMS LogicalDeviceParams;
		DWORD nUnitDeviceEntries;
		UNITDEVICE_ENTRY UnitDeviceEntries[];
	};
#pragma warning(default: 4200)
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_NDAS_SCSI_LOCATION
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_NDAS_SCSI_LOCATION 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_NDAS_SCSI_LOCATION };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
    struct REPLY 
    {
        NDAS_SCSI_LOCATION NdasScsiLocation;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_TYPE_QUERY_HOST_INFO_UNITDEVICE
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDAS_CMD_QUERY_HOST_ENTRY
{
    GUID HostGuid;
    ACCESS_MASK Access;
} NDAS_CMD_QUERY_HOST_ENTRY, *PNDAS_CMD_QUERY_HOST_ENTRY;

struct NDAS_CMD_QUERY_HOST_UNITDEVICE
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD EntryCount;
        NDAS_CMD_QUERY_HOST_ENTRY Entry[0];
    };
#pragma warning(default: 4200)
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_TYPE_QUERY_HOST_INFO_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_HOST_LOGICALDEVICE
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_HOST_LOGICALDEVICE };
    struct REQUEST
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
#pragma warning(disable: 4200)
    struct REPLY 
    {
        DWORD EntryCount;
        NDAS_CMD_QUERY_HOST_ENTRY Entry[0];
    };
#pragma warning(default: 4200)
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_HOST_INFO
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_HOST_INFO
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_HOST_INFO };
    struct REQUEST
    {
        GUID HostGuid;
    };
#pragma warning(disable: 4200)
    struct REPLY
    {
        NDAS_HOST_INFOW HostInfo;
    };
#pragma warning(default: 4200)

};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_REQUEST_SURRENDER_ACCESS
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_REQUEST_SURRENDER_ACCESS
{
    enum { CMD = NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS};
    struct REQUEST
    {
        GUID HostGuid;
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
        ACCESS_MASK Access; // GENERIC_READ | GENERIC_WRITE
    };
    struct REPLY
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE
{
    enum { CMD = NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE };
    struct REQUEST
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
    struct REPLY
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_NOTIFY_DEVICE_CHANGE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_NOTIFY_DEVICE_CHANGE
{
    enum { CMD = NDAS_CMD_TYPE_NOTIFY_DEVICE_CHANGE };
    struct REQUEST
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
    };
    struct REPLY
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_SET_SERVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_SET_SERVICE_PARAM
{
    enum { CMD = NDAS_CMD_TYPE_SET_SERVICE_PARAM };
    struct REQUEST 
    {
        NDAS_SERVICE_PARAM Param;
    };
    struct REPLY 
    {
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_GET_SERVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_GET_SERVICE_PARAM
{
    enum { CMD = NDAS_CMD_TYPE_GET_SERVICE_PARAM };
    struct REQUEST
    {
        DWORD ParamCode;
    };
    struct REPLY 
    {
        NDAS_SERVICE_PARAM Param;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_DEVICE_STAT
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_DEVICE_STAT 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_DEVICE_STAT };
    struct REQUEST
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
    };
    struct REPLY 
    {
        NDAS_DEVICE_STAT DeviceStat;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_UNITDEVICE_STAT
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_UNITDEVICE_STAT 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_UNITDEVICE_STAT };
    struct REQUEST 
    {
        NDAS_DEVICE_ID_EX DeviceIdOrSlot;
        DWORD UnitNo;
    };
    struct REPLY 
    {
        NDAS_UNITDEVICE_STAT UnitDeviceStat;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_LOGICALDEVICE_VOLUMES 
{
    enum { CMD = NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_VOLUMES };
    struct REQUEST 
    {
        NDAS_LOGICALDEVICE_ID LogicalDeviceId;
    };
    struct REPLY 
    {
    };
};

#include <poppack.h>

