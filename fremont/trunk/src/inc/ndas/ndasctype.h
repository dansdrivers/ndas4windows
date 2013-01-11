/*++

NDAS Core Type Header

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

Remarks:

Core Type header is an internal header.
This header is used for the service and API library.
API library SHOULD NOT expose types in this header to library users.
Only NDASUSER.H and NDASTYPE.H are public headers.

If you need some data types in this header,
make a super/sub data type containing some fields in this header.

Also, when working with USER API client application,
do not try to use this header.

--*/

#ifndef _NDAS_CTYPE_H_
#define _NDAS_CTYPE_H_

#pragma once

#ifndef _NDAS_TYPE_H_
#error include ndastype.h prior to include ndasctype.h
#endif

/* All structures in this header are 8-byte aligned. */
#include <pshpack8.h>

/* LPX Address Node */

typedef struct _LPX_ADDRESS_NODE {
	BYTE Node[6];
} LPX_ADDRESS_NODE, *PLPX_ADDRESS_NODE;

C_ASSERT(6 == sizeof(LPX_ADDRESS_NODE));

/* NDAS Unit Device ID */

typedef struct _NDAS_UNITDEVICE_ID {
	NDAS_DEVICE_ID DeviceId;
	DWORD UnitNo;
} NDAS_UNITDEVICE_ID, *PNDAS_UNITDEVICE_ID;

C_ASSERT(12 == sizeof(NDAS_UNITDEVICE_ID));

/* NDAS Logical Device ID is defined in ndastype.h */

/* NULL value function for NDAS Device ID */

#ifdef __cplusplus
__inline const NDAS_DEVICE_ID& NullNdasDeviceId()
#else
__inline NDAS_DEVICE_ID NullNdasDeviceId()
#endif
{
	static const NDAS_DEVICE_ID NullId = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return NullId;
}

/* NULL value check function for NDAS Device ID */

#ifdef __cplusplus
__inline bool IsNullNdasDeviceId(const NDAS_DEVICE_ID& deviceId)
#else
__inline BOOL IsNullNdasDeviceId(NDAS_DEVICE_ID deviceId)
#endif
{
	return (deviceId.Node[0] == 0x00 &&
		deviceId.Node[1] == 0x00 &&
		deviceId.Node[2] == 0x00 &&
		deviceId.Node[3] == 0x00 &&
		deviceId.Node[4] == 0x00 &&
		deviceId.Node[5] == 0x00);
}


/* NULL value function for NDAS Unit Device ID */

#ifdef __cplusplus
__inline bool IsNullNdasUnitDeviceId(const NDAS_UNITDEVICE_ID& unitDeviceId) {
#else
__inline BOOL IsNullNdasUnitDeviceId(NDAS_UNITDEVICE_ID unitDeviceId) {
#endif

	return IsNullNdasDeviceId(unitDeviceId.DeviceId) &&
		(0 == unitDeviceId.UnitNo);
}

/* NULL value check function for NDAS Unit Device ID */

#ifdef __cplusplus
__inline const NDAS_UNITDEVICE_ID& NullNdasUnitDeviceId() {
#else
__inline NDAS_UNITDEVICE_ID NullNdasUnitDeviceId() {
#endif

	static const NDAS_UNITDEVICE_ID NullUnitDeviceId = {0};
	return NullUnitDeviceId;
}

typedef DWORD NDAS_LOGICALDEVICE_TYPE;

typedef enum NDAS_LOGICALUNIT_WARNING {

	NDAS_LOGICALUNIT_WARNING_BAD_DISK		   = 0x00000004,
	NDAS_LOGICALUNIT_WARNING_BAD_SECTOR		   = 0x00000008,
	NDAS_LOGICALUNIT_WARNING_REPLACED_BY_SPARE = 0x00000010
};

typedef struct _NDAS_LOGICALUNIT_DEFINITION {

	DWORD					Size;
	NDAS_LOGICALDEVICE_TYPE Type;
	GUID					ConfigurationGuid;

	DWORD					DiskCount;			// DIB_V2.nDiskCount
	DWORD					SpareCount;			// DIB_V2.nSpareCount
	
	BOOL					NotLockable;
	UINT64					StartOffset;

	NDAS_DEVICE_ID			NdasChildDeviceId[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];			// nidx
	CHAR					NdasChildSerial[NDAS_MAX_UNITS_IN_V2_1][NDAS_DIB_SERIAL_LEN];	// nidx

	BOOL					ActiveNdasUnits[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];			// ridx

} NDAS_LOGICALUNIT_DEFINITION, *PNDAS_LOGICALUNIT_DEFINITION;

typedef enum _NDAS_RAID_SIMPLE_STATUS_FLAG {

	NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_REGULAR		= 0x00000001,
	NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_REGULAR	= 0x00000002,
	NDAS_RAID_SIMPLE_STATUS_REPLACED_IN_REGULAR		= 0x00000004,

	NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_SPARE		= 0x00000010,
	NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_SPARE		= 0x00000020,
	NDAS_RAID_SIMPLE_STATUS_REPLACED_IN_SPARE		= 0x00000040,
	
	NDAS_RAID_SIMPLE_STATUS_NOT_SYNCED				= 0x00000100,
	NDAS_RAID_SIMPLE_STATUS_EMERGENCY				= 0x00000200,

} NDAS_RAID_SIMPLE_STATUS_FLAG, *PNDAS_RAID_SIMPLE_STATUS_FLAG;

#include <poppack.h>

#endif /* _NDAS_CTYPE_H_ */
