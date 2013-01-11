/*++

  Another internal header for type defs

  Copyright (C) 2002-2004 XIMETA, Inc.
  All rights reserved.

  Remarks:

  This header contains the structures for
  extended type information for internal implementation
  for services and user API dlls.

--*/

#ifndef _NDAS_TYPE_EX_H_
#define _NDAS_TYPE_EX_H_

#pragma once
#include <ndas/ndastype.h>
#include <ndas/ndasctype.h>
#include "socketlpx.h"

/* 8 byte alignment */
#include <pshpack8.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Internal Constants */

#define NDAS_MAX_CONNECTION_V11 64

/* PRIMARY HOST INFO */

typedef struct _NDAS_UNITDEVICE_PRIMARY_HOST_INFO {
	DWORD LastUpdate;
	LPX_ADDRESS Host;
	UCHAR SWMajorVersion;
	UCHAR SWMinorVersion;
	UINT32 SWBuildNumber;
	USHORT NDFSCompatVersion;
	USHORT NDFSVersion;
} NDAS_UNITDEVICE_PRIMARY_HOST_INFO, *PNDAS_UNITDEVICE_PRIMARY_HOST_INFO;

typedef struct _NDAS_DEVICE_ID_EX {
	BOOL UseSlotNo;
	union {
		NDAS_DEVICE_ID DeviceId;
		DWORD SlotNo;
	};
} NDAS_DEVICE_ID_EX, *PNDAS_DEVICE_ID_EX;

C_ASSERT(12 == sizeof(NDAS_DEVICE_ID_EX));

typedef struct _NDAS_UNITDEVICE_ID_EX {
	NDAS_DEVICE_ID_EX DeviceIdEx;
	DWORD UnitNo;
} NDAS_UNITDEVICE_ID_EX, *PNDAS_UNITDEVICE_ID_EX;

C_ASSERT(16 == sizeof(NDAS_UNITDEVICE_ID_EX));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include <poppack.h>

#endif /* _NDAS_TYPE_EX_H_ */
