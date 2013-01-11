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

typedef struct _NDAS_UNITDEVICE_PRIMARY_HOST_INFO {
	DWORD LastUpdate;
	LPX_ADDRESS Host;
	UCHAR SWMajorVersion;
	UCHAR SWMinorVersion;
	UINT32 SWBuildNumber;
	USHORT NDFSCompatVersion;
	USHORT NDFSVersion;
} NDAS_UNITDEVICE_PRIMARY_HOST_INFO, *PNDAS_UNITDEVICE_PRIMARY_HOST_INFO;


/*  Unit Device Information */

typedef struct _NDAS_UNITDEVICE_INFORMATION {

	DWORD dwRWHosts;
	DWORD dwROHosts;

	BOOL bLBA;
	BOOL bLBA48;
	BOOL bPIO;
	BOOL bDMA;
	BOOL bUDMA;

	NDAS_UNITDEVICE_MEDIA_TYPE MediaType;

	unsigned _int64	SectorCount;

	TCHAR szModel[40];
	TCHAR szModelTerm;
	TCHAR szFwRev[8];
	TCHAR szFwRevTerm;
	TCHAR szSerialNo[20];
	TCHAR szSerialTerm;

} NDAS_UNITDEVICE_INFORMATION, *PNDAS_UNITDEVICE_INFORMATION;

#include <poppack.h>

#endif /* _NDAS_TYPE_EX_H_ */
