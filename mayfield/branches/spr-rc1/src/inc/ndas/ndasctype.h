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

//#include <windows.h>
//#include <socketlpx.h>
//#include <winsock2.h>
//#include <ws2tcpip.h>
//
//typedef BYTE NDAS_DEVICE_ADDRESS_TYPE;
//
//static const NDAS_DEVICE_ADDRESS_TYPE NDAS_DEVICE_ADDRESS_TYPE_LPX = 1;
//static const NDAS_DEVICE_ADDRESS_TYPE NDAS_DEVICE_ADDRESS_TYPE_IPv4 = 2;
//static const NDAS_DEVICE_ADDRESS_TYPE NDAS_DEVICE_ADDRESS_TYPE_IPv6 = 3;
//static const NDAS_DEVICE_ADDRESS_TYPE NDAS_DEVICE_ADDRESS_TYPE_IPv6_old = 4;
//
//struct NDAS_DEVICE_ADDRESS {
//	NDAS_DEVICE_ADDRESS_TYPE AddressType;
//	union {
//		LPX_ADDRESS LpxAddress;
//		sockaddr_in IPv4Address;
//		sockaddr_in6 IPv6Address;
//		sockaddr_in6_old IPv6OAddress;
//	};
//};
//
//typedef NDAS_DEVICE_ADDRESS* PNDAS_DEVICE_ADDRESS;


/* All structures in this header are unaligned. */
#include <pshpack1.h>

/* NDAS Device ID */

typedef struct _NDAS_DEVICE_ID {
	BYTE Node[6];
} NDAS_DEVICE_ID, *PNDAS_DEVICE_ID;

/* LPX Address Node */

typedef struct _LPX_ADDRESS_NODE {
	BYTE Node[6];
} LPX_ADDRESS_NODE, *PLPX_ADDRESS_NODE;

/* NDAS Unit Device ID */

typedef UNALIGNED struct _NDAS_UNITDEVICE_ID {
	NDAS_DEVICE_ID DeviceId;
	DWORD UnitNo;
} NDAS_UNITDEVICE_ID, *PNDAS_UNITDEVICE_ID;

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
__inline bool IsNullNdasUnitDeviceId(const NDAS_UNITDEVICE_ID& unitDeviceId)
#else
__inline BOOL IsNullNdasUnitDeviceId(NDAS_UNITDEVICE_ID unitDeviceId)
#endif
{
	return IsNullNdasDeviceId(unitDeviceId.DeviceId) &&
		(0 == unitDeviceId.UnitNo);
}

/* NULL value check function for NDAS Unit Device ID */

#ifdef __cplusplus
__inline const NDAS_UNITDEVICE_ID& NullNdasUnitDeviceId()
#else
__inline NDAS_UNITDEVICE_ID NullNdasUnitDeviceId()
#endif
{
	static const NDAS_UNITDEVICE_ID NullUnitDeviceId = {0};
	return NullUnitDeviceId;
}

#define MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER	32

typedef DWORD NDAS_LOGICALDEVICE_TYPE;

typedef struct _NDAS_LOGICALDEVICE_GROUP {
	NDAS_LOGICALDEVICE_TYPE Type;
	DWORD nUnitDevices; // DIB_V2.nDiskCount + DIB_V2.nSpareCount
	DWORD nUnitDevicesSpare; // nUnitDevicesInRaid = nUnitDevices - nUnitDevicesSpare
	NDAS_UNITDEVICE_ID UnitDevices[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];
} NDAS_LOGICALDEVICE_GROUP, *PNDAS_LOGICALDEVICE_GROUP;

#include <poppack.h>

#endif /* _NDAS_CTYPE_H_ */
