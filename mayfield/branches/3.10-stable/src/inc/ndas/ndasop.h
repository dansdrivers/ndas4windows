/*++

NDAS Device Operation API Header

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _NDASOP_H_
#define _NDASOP_H_

#pragma once

#if defined(_WINDOWS_)

#ifndef _INC_WINDOWS
#error ndasop.h requires windows.h to be included first
#endif

#ifdef NDASOP_EXPORTS
#define NDASOP_API __declspec(dllexport)
#else
#define NDASOP_API __declspec(dllimport)
#endif

#else /* defined(WIN32) || defined(UNDER_CE) */
#define NDASOP_API 
#endif

#ifndef _NDAS_TYPE_H_
#include "ndastype.h"
#endif /* _NDAS_TYPE_H_ */

#ifndef _NDAS_EVENT_H_
#include "ndasevent.h"
#endif /* _NDAS_EVENT_H_ */

#ifndef _NDAS_DIB_H_
#include "ndasdib.h"
#endif /* _NDAS_DIB_H_ */

#ifdef __cplusplus
extern "C" {
#endif

/* 

Use the following definition for WinCE specifics
#if defined(_WIN32_WCE_)
#endif

*/

#define NDASOP_API_VERSION_MAJOR 0
#define NDASOP_API_VERSION_MINOR 1

/*++

NdasOpGetAPIVersion function returns the current version information
of the loaded library

Return Values:

Low word contains the major version number and high word the minor
version number.

--*/

NDASOP_API
DWORD
__stdcall
NdasOpGetAPIVersion();

/*++

NdasOpBind function bind or unbind(single) NDAS disks.

Parameters:

nDiskCount
[in] Number of NDAS disks to bind(or unbind). nDiskCount should be correct for each raid type

pUnitDeviceIDs
[in] An array of NDAS_UNIT_DEVICE_ID which contains unit device ids to bind(or unbind). 
The size of pUnitDeviceIDs must be nDiskCount.

BindType
[in] Raid type. See ndasdib.h for details. following types are supported.
NMT_SINGLE,	NMT_AGGREGATE,	NMT_RAID0,	NMT_RAID1,	NMT_RAID4
NMT_SINGLE unbinds disks.

Return Values:

If the function succeeds, the return value is nDiskCount.

If the function fails, the return value is not nDiskCount.
Depends on GetLastError, return value can be a index of pUnitDeviceIDs which failed to operate.

--*/

NDASOP_API
UINT32
__stdcall
NdasOpBind(
	UINT32	nDiskCount,
	NDAS_CONNECTION_INFO *pConnectionInfo,
	UINT32	BindType
);

NDASOP_API
BOOL
__stdcall
NdasOpIsBitmapClean(
	HNDAS hNdasDevice,
	BOOL *pIsClean);

NDASOP_API
BOOL
__stdcall
NdasOpGetLastWrttenSectorInfo(
	HNDAS hNdasDevice,
	PLAST_WRITTEN_SECTOR pLWS);

NDASOP_API
BOOL
__stdcall
NdasOpGetLastWrttenSectorsInfo(
	HNDAS hNdasDevice,
	PLAST_WRITTEN_SECTORS pLWS);

/*--
Not implemented
--*/

#define NDAS_RECOVER_STATUS_INITIALIZE		1
#define NDAS_RECOVER_STATUS_RUNNING			2
#define NDAS_RECOVER_STATUS_FINALIZE		3
#define NDAS_RECOVER_STATUS_COMPLETE		4
#define NDAS_RECOVER_STATUS_FAILED			5

typedef BOOL (WINAPI *PNDAS_OP_RECOVER_CALLBACK)(
	DWORD dwStatus,
	UINT32 Total,
	UINT32 Current,
	LPVOID lpParameter
);
typedef PNDAS_OP_RECOVER_CALLBACK LPNDAS_OP_RECOVER_CALLBACK;

/*--
Not implemented
--*/
NDASOP_API
BOOL
__stdcall
NdasOpRecover(
	NDAS_CONNECTION_INFO *pConnectionInfo,
	LPNDAS_OP_RECOVER_CALLBACK callback_func,
	LPVOID lpParameter
);

/*--
Not implemented
--*/
typedef BOOL (WINAPI *PNDAS_OP_SYNC_CALLBACK)(
	UINT32 Total,
	UINT32 Current,
	LPVOID lpParameter
);
typedef PNDAS_OP_SYNC_CALLBACK LPNDAS_OP_SYNC_CALLBACK;

/*--
Not implemented
--*/
NDASOP_API
BOOL
__stdcall
NdasOpSynchronize(
	NDAS_CONNECTION_INFO *pConnectionInfo,
	LPNDAS_OP_RECOVER_CALLBACK callback_func,
	LPVOID lpParameter
);

/*++

NdasOpReadDIB function reads DIB information and builds NDAS_DIB_V2

Parameters:

hNDASDevice
[in] Handle to operate NDAS Device.

pDIB_V2
[out] Pointer to NDAS_DIB_V2 structure to retrieve DIB information.
If pDIB_V2 is NULL, pnDIBSize will receive required size for pDIB_V2
if *pnDIBSize is sizeof(NDAS_DIB_V2) and lesser than required size,
only the first sector will be copied to pDIB_V2

pnDIBSize
[in out] size of pDIB_V2, must not be NULL. if size is lesser than
required size, required size will be set to *pnDIBSize

Return Values:

If the function succeeds, the return value is nDiskCount.

If the function fails, the return value is not nDiskCount.
Depends on GetLastError, return value can be a index of pUnitDeviceIDs which failed to operate.

--*/
NDASOP_API
BOOL
__stdcall
NdasOpReadDIB(
	HNDAS hNdasDevice,
	NDAS_DIB_V2 *pDIB_V2,
	UINT32 *pnDIBSize
);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASOP_H_ */
