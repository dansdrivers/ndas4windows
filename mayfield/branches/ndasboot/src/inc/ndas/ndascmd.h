/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <windows.h>
#include <basetsd.h>
#include "ndasctype.h"
#include "ndastypeex.h"
#include "ndashostinfo.h"
#include "ndassvcparam.h"

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_TYPE
//
//////////////////////////////////////////////////////////////////////////

#define	NDAS_CMD_TYPE_NONE								0x00

#define	NDAS_CMD_TYPE_REGISTER_DEVICE					0x1A
#define NDAS_CMD_TYPE_UNREGISTER_DEVICE					0x1B
#define NDAS_CMD_TYPE_ENUMERATE_DEVICES					0x11
#define	NDAS_CMD_TYPE_SET_DEVICE_PARAM					0x12
#define	NDAS_CMD_TYPE_QUERY_DEVICE_STATUS				0x13
#define	NDAS_CMD_TYPE_QUERY_DEVICE_INFORMATION			0x14

#define	NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES				0x21
#define	NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM				0x22
#define	NDAS_CMD_TYPE_QUERY_UNITDEVICE_STATUS			0x23
#define	NDAS_CMD_TYPE_QUERY_UNITDEVICE_INFORMATION		0x24
#define NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT			0x25
#define	NDAS_CMD_TYPE_FIND_LOGICALDEVICE_OF_UNITDEVICE	0x29

#define	NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES			0x31
#define	NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM			0x32
#define	NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS		0x33
#define	NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION	0x34

#define	NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE				0x41
#define	NDAS_CMD_TYPE_EJECT_LOGICALDEVICE				0x42
#define	NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE				0x43
#define	NDAS_CMD_TYPE_RECOVER_LOGICALDEVICE				0x44

#define NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE				0x51
#define NDAS_CMD_TYPE_QUERY_HOST_LOGICALDEVICE			0x52
#define NDAS_CMD_TYPE_QUERY_HOST_INFO					0x53

#define NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS			0x61

#define NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE			0x71

#define NDAS_CMD_TYPE_SET_SERVICE_PARAM					0x91
#define NDAS_CMD_TYPE_GET_SERVICE_PARAM					0x92

typedef WORD NDAS_CMD_TYPE;

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_STATUS
//
//////////////////////////////////////////////////////////////////////////

#define NDAS_CMD_STATUS_REQUEST 0xFF
#define NDAS_CMD_STATUS_SUCCESS 0x00
#define NDAS_CMD_STATUS_FAILED 0x80
#define NDAS_CMD_STATUS_ERROR_NOT_IMPLEMENTED 0x81
#define NDAS_CMD_STATUS_INVALID_REQUEST 0x82
#define NDAS_CMD_STATUS_TERMINATION	0x83
#define NDAS_CMD_STATUS_UNSUPPORTED_VERSION	0x8F

typedef WORD NDAS_CMD_STATUS;

//////////////////////////////////////////////////////////////////////////
//
// NDAS_CMD_HEADER
//
//////////////////////////////////////////////////////////////////////////

const CHAR NDAS_CMD_PROTOCOL[4] = {'N', 'D', 'C', 'P'};

const UCHAR NDAS_CMD_PROTOCOL_VERSION_MAJOR = 0x01;
const UCHAR NDAS_CMD_PROTOCOL_VERSION_MINOR = 0x00;

UNALIGNED struct NDAS_CMD_HEADER {
	CHAR	Protocol[4];    // 4
	UCHAR	VersionMajor;   // 1
	UCHAR	VersionMinor;   // 1
	WORD	Command;        // 2
	WORD	Status;         // 2
	WORD	TransactionId;  // 2
	DWORD	MessageSize;    // 4
};

typedef NDAS_CMD_HEADER* PNDAS_CMD_HEADER;

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_ERROR
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_ERROR {

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY {
		DWORD dwErrorCode;
		DWORD dwDataLength;
		LPVOID lpData[];
	};
#pragma warning(default: 4200)

};

//////////////////////////////////////////////////////////////////////////
//
// Types for NDASCMD
//
//////////////////////////////////////////////////////////////////////////

UNALIGNED struct NDAS_DEVICE_ID_OR_SLOT {
	BOOL bUseSlotNo;
	union {
		NDAS_DEVICE_ID DeviceId;
		DWORD SlotNo;
	};
};

typedef NDAS_DEVICE_ID_OR_SLOT* PNDAS_DEVICE_ID_OR_SLOT;

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_NONE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_NONE
{
	enum { CMD = NDAS_CMD_TYPE_NONE };
	UNALIGNED struct REQUEST {
	};
	UNALIGNED struct REPLY {
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

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID DeviceId;
		WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
		ACCESS_MASK GrantingAccess;
	};

	UNALIGNED struct REPLY {
		DWORD SlotNo;
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_UNREGISTER_DEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_UNREGISTER_DEVICE
{
	enum { CMD = NDAS_CMD_TYPE_UNREGISTER_DEVICE};

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_ENUMERATE_DEVICES
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_ENUMERATE_DEVICES
{
	enum { CMD = NDAS_CMD_TYPE_ENUMERATE_DEVICES};

	UNALIGNED struct REQUEST {
	};

	UNALIGNED struct ENUM_ENTRY {
		NDAS_DEVICE_ID DeviceId;
		DWORD SlotNo;
		WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
		ACCESS_MASK GrantedAccess;
	};
	
	typedef ENUM_ENTRY* PENUM_ENTRY;

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY {
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

#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_NONE		0x00
#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_ENABLE	0x01
#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_NAME		0x02
#define NDAS_CMD_SET_DEVICE_PARAM_TYPE_ACCESS	0x04

typedef DWORD NDAS_CMD_SET_DEVICE_PARAM_TYPE;

struct UNALIGNED NDAS_CMD_SET_DEVICE_PARAM_DATA {
	NDAS_CMD_SET_DEVICE_PARAM_TYPE ParamType;
	union {
		BOOL	bEnable;
		ACCESS_MASK	GrantingAccess;
		WCHAR	wszName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	};
};

typedef NDAS_CMD_SET_DEVICE_PARAM_DATA* PNDAS_CMD_SET_DEVICE_PARAM_DATA;

struct NDAS_CMD_SET_DEVICE_PARAM {

	enum { CMD = NDAS_CMD_TYPE_SET_DEVICE_PARAM};

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		NDAS_CMD_SET_DEVICE_PARAM_DATA Param;
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_SET_UNITDEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

#define NDAS_CMD_SET_UNITDEVICE_PARAM_TYPE_NONE		0x00

typedef DWORD NDAS_CMD_SET_UNITDEVICE_PARAM_TYPE;

struct UNALIGNED NDAS_CMD_SET_UNITDEVICE_PARAM_DATA {
	NDAS_CMD_SET_UNITDEVICE_PARAM_TYPE ParamType;
	union {
		BYTE Reserved[28];
	};
};

typedef NDAS_CMD_SET_UNITDEVICE_PARAM_DATA* PNDAS_CMD_SET_UNITDEVICE_PARAM_DATA;

struct NDAS_CMD_SET_UNITDEVICE_PARAM {

	enum { CMD = NDAS_CMD_TYPE_SET_UNITDEVICE_PARAM};

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD	dwUnitNo;
		NDAS_CMD_SET_UNITDEVICE_PARAM_DATA Param;
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_SET_LOGICALDEVICE_PARAM
//
//////////////////////////////////////////////////////////////////////////

typedef DWORD NDAS_CMD_SET_LOGICALDEVICE_PARAM_TYPE;

struct UNALIGNED NDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA {
	NDAS_CMD_SET_LOGICALDEVICE_PARAM_TYPE ParamType;
	union {
		BYTE Reserved[28];
	};
};

typedef NDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA* PNDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA;

struct NDAS_CMD_SET_LOGICALDEVICE_PARAM {

	enum { CMD = NDAS_CMD_TYPE_SET_LOGICALDEVICE_PARAM};

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
		NDAS_CMD_SET_LOGICALDEVICE_PARAM_DATA Param;
	};

	UNALIGNED struct REPLY {
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

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
	};

	UNALIGNED struct REPLY {
		NDAS_DEVICE_ID DeviceId;
		DWORD SlotNo;
		WCHAR wszDeviceName[MAX_NDAS_DEVICE_NAME_LEN];
		ACCESS_MASK GrantedAccess;
		NDAS_DEVICE_HW_INFORMATION HardwareInfo;
		NDAS_DEVICE_PARAMS DeviceParams;
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

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
	};

	UNALIGNED struct REPLY {
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
	enum { CMD = NDAS_CMD_TYPE_ENUMERATE_UNITDEVICES };

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
	};

	UNALIGNED struct ENUM_ENTRY {
		DWORD UnitNo;
		NDAS_UNITDEVICE_TYPE UnitDeviceType;
	};
	
	typedef ENUM_ENTRY* PENUM_ENTRY;

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY {
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

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
	};

	UNALIGNED struct REPLY {
		NDAS_UNITDEVICE_TYPE UnitDeviceType;
		NDAS_UNITDEVICE_SUBTYPE UnitDeviceSubType;
		NDAS_UNITDEVICE_HW_INFORMATIONW HardwareInfo;
		NDAS_UNITDEVICE_PARAMS UnitDeviceParams;
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

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
	};

	UNALIGNED struct REPLY {
		NDAS_UNITDEVICE_STATUS Status;
		NDAS_UNITDEVICE_ERROR LastError;
	};

};

struct NDAS_CMD_QUERY_UNITDEVICE_HOST_COUNT
{
	enum { CMD = NDAS_CMD_TYPE_QUERY_UNITDEVICE_HOST_COUNT };

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
	};

	UNALIGNED struct REPLY {
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

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
	};

	UNALIGNED struct REPLY {
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
	enum { CMD = NDAS_CMD_TYPE_ENUMERATE_LOGICALDEVICES };

	UNALIGNED struct REQUEST {
	};

	typedef UNALIGNED struct _ENUM_ENTRY {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
		NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
	} ENUM_ENTRY, *PENUM_ENTRY;

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY {
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

struct NDAS_CMD_PLUGIN_LOGICALDEVICE {

	enum { CMD = NDAS_CMD_TYPE_PLUGIN_LOGICALDEVICE };

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
		ACCESS_MASK Access; /* GENERIC_READ [| GENERIC_WRITE] */
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_EJECT_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_EJECT_LOGICALDEVICE {

	enum { CMD = NDAS_CMD_TYPE_EJECT_LOGICALDEVICE };

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_UNPLUG_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_UNPLUG_LOGICALDEVICE {

	enum { CMD = NDAS_CMD_TYPE_UNPLUG_LOGICALDEVICE };

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_RECOVER_LOGICALDEVICE
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_RECOVER_LOGICALDEVICE {

	enum { CMD = NDAS_CMD_TYPE_RECOVER_LOGICALDEVICE };

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};

	UNALIGNED struct REPLY {
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_LOGICALDEVICE_STATUS
//
//////////////////////////////////////////////////////////////////////////

struct NDAS_CMD_QUERY_LOGICALDEVICE_STATUS {

	enum { CMD = NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_STATUS };

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};

	UNALIGNED struct REPLY {
		NDAS_LOGICALDEVICE_STATUS Status;
		NDAS_LOGICALDEVICE_ERROR LastError;
	};

};

//////////////////////////////////////////////////////////////////////////
//
// _NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION
//
//////////////////////////////////////////////////////////////////////////

UNALIGNED struct NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION {
	DWORD Blocks;
	BYTE Reserved[24];
};

typedef NDAS_CMD_LOGICALDEVICE_DISK_INFORMATION* PNDAS_CMD_LOGICALDEVICE_DISK_INFORMATION;

struct NDAS_CMD_QUERY_LOGICALDEVICE_INFORMATION {

	enum { CMD = NDAS_CMD_TYPE_QUERY_LOGICALDEVICE_INFORMATION };

	UNALIGNED struct REQUEST {
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};

	UNALIGNED struct UNITDEVICE_ENTRY {
		NDAS_DEVICE_ID DeviceId;
		DWORD UnitNo;
	};
	
	typedef UNITDEVICE_ENTRY* PUNITDEVICE_ENTRY;

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY {
		NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
		ACCESS_MASK	GrantedAccess;
		ACCESS_MASK MountedAccess;
		union {
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
// NDAS_CMD_TYPE_QUERY_HOST_INFO_UNITDEVICE
//
//////////////////////////////////////////////////////////////////////////

typedef UNALIGNED struct _NDAS_CMD_QUERY_HOST_ENTRY
{
	GUID HostGuid;
	ACCESS_MASK Access;
} NDAS_CMD_QUERY_HOST_ENTRY, *PNDAS_CMD_QUERY_HOST_ENTRY;

struct NDAS_CMD_QUERY_HOST_UNITDEVICE
{
	enum { CMD = NDAS_CMD_TYPE_QUERY_HOST_UNITDEVICE };

	UNALIGNED struct REQUEST {
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
	};

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY {
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

	UNALIGNED struct REQUEST
	{
		NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	};

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY 
	{
		DWORD EntryCount;
		NDAS_CMD_QUERY_HOST_ENTRY Entry[0];
	};
#pragma warning(default: 4200)
};

struct NDAS_CMD_QUERY_HOST_INFO
{
	enum { CMD = NDAS_CMD_TYPE_QUERY_HOST_INFO };

	UNALIGNED struct REQUEST
	{
		GUID HostGuid;
	};

#pragma warning(disable: 4200)
	UNALIGNED struct REPLY
	{
		NDAS_HOST_INFOW HostInfo;
	};
#pragma warning(default: 4200)

};

struct NDAS_CMD_REQUEST_SURRENDER_ACCESS
{
	enum { CMD = NDAS_CMD_TYPE_REQUEST_SURRENDER_ACCESS};

	UNALIGNED struct REQUEST
	{
		GUID HostGuid;
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
		ACCESS_MASK Access; // GENERIC_READ | GENERIC_WRITE
	};

	UNALIGNED struct REPLY
	{
	};
};

struct NDAS_CMD_NOTIFY_UNITDEVICE_CHANGE
{
	enum { CMD = NDAS_CMD_TYPE_NOTIFY_UNITDEVICE_CHANGE };

	UNALIGNED struct REQUEST
	{
		NDAS_DEVICE_ID_OR_SLOT DeviceIdOrSlot;
		DWORD UnitNo;
	};

	UNALIGNED struct REPLY
	{
	};
};

struct NDAS_CMD_SET_SERVICE_PARAM
{
	enum { CMD = NDAS_CMD_TYPE_SET_SERVICE_PARAM };

	UNALIGNED struct REQUEST
	{
		NDAS_SERVICE_PARAM Param;
	};

	UNALIGNED struct REPLY
	{
	};
};

struct NDAS_CMD_GET_SERVICE_PARAM
{
	enum { CMD = NDAS_CMD_TYPE_GET_SERVICE_PARAM };

	UNALIGNED struct REQUEST
	{
		DWORD ParamCode;
	};

	UNALIGNED struct REPLY
	{
		NDAS_SERVICE_PARAM Param;
	};

};

