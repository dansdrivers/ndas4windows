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
#include "ndasctype.h"
#include "ndastype.h"
#include "socketlpx.h"

#include <pshpack1.h>
typedef UNALIGNED struct _NDAS_UNITDEVICE_PRIMARY_HOST_INFO {
	DWORD LastUpdate;
	LPX_ADDRESS Host;
	UCHAR SWMajorVersion;
	UCHAR SWMinorVersion;
	UINT32 SWBuildNumber;
	USHORT NDFSCompatVersion;
	USHORT NDFSVersion;
} NDAS_UNITDEVICE_PRIMARY_HOST_INFO, *PNDAS_UNITDEVICE_PRIMARY_HOST_INFO;
#include <poppack.h>

#endif /* _NDAS_TYPE_EX_H_ */
