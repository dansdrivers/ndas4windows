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

#if   defined(NDASOP_EXPORTS)
#define NDASOP_LINKAGE __declspec(dllexport)
#elif defined(NDASOP_SLIB)
#define NDASOP_LINKAGE
#else
#define NDASOP_LINKAGE __declspec(dllimport)
#endif

#else /* defined(WIN32) || defined(UNDER_CE) */
#define NDASOP_LINKAGE 
#endif

#define NDASOPAPI __stdcall
#define NDASOPAPI_CALLBACK __stdcall

#ifndef _NDAS_TYPE_H_
#include <ndas/ndastype.h>
#endif /* _NDAS_TYPE_H_ */

#ifndef _NDAS_EVENT_H_
#include <ndas/ndasevent.h>
#endif /* _NDAS_EVENT_H_ */

#ifndef _NDAS_DIB_H_
#include <ndas/ndasdib.h>
#endif /* _NDAS_DIB_H_ */

#ifndef _NDASCOMM_H_
#include <ndas/ndascomm.h>
#endif /* _NDASCOMM_H_ */


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

typedef void* HNDAS;

NDASOP_LINKAGE
DWORD
NDASOPAPI
NdasOpGetAPIVersion();

/*++

NdasOpBind function bind or unbind(single) NDAS disks.

Parameters:

nDiskCount
[in] Number of NDAS disks to bind(or unbind). nDiskCount should be correct for each raid type

pConnectionInfo
[in] An array of NDASCOMM_CONNECTION_INFO  to bind(or unbind). 
The size of *pConnectionInfo must be nDiskCount.

BindType
[in] Raid type. See ndasdib.h for details. following types are supported.
NMT_SINGLE,	NMT_AGGREGATE,	NMT_RAID0,	NMT_RAID1,	NMT_RAID4, NMT_SAFE_RAID1.
NMT_SINGLE unbinds disks.

Return Values:

If the function succeeds, the return value is nDiskCount.

If the function fails, the return value is not nDiskCount.
Depends on GetLastError, return value can be a index of pUnitDeviceIDs which failed to operate.

--*/

NDASOP_LINKAGE
UINT32
NDASOPAPI
NdasOpBind(
	UINT32	nDiskCount,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	UINT32	BindType
);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpIsBitmapClean(
	HNDAS hNdasDevice,
	BOOL *pIsClean);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpGetLastWrttenSectorInfo(
	HNDAS hNdasDevice,
	PLAST_WRITTEN_SECTOR pLWS);

NDASOP_LINKAGE
BOOL
NDASOPAPI
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

typedef BOOL (NDASOPAPI_CALLBACK *PNDAS_OP_RECOVER_CALLBACK)(
	DWORD dwStatus,
	UINT32 Total,
	UINT32 Current,
	LPVOID lpParameter
);
typedef PNDAS_OP_RECOVER_CALLBACK LPNDAS_OP_RECOVER_CALLBACK;

/*--
Not implemented
--*/
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRepair(
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfoReplace
);

/*--
Not implemented
--*/
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRecover(
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	LPNDAS_OP_RECOVER_CALLBACK callback_func,
	LPVOID lpParameter
);

/*--
Not implemented
--*/
typedef BOOL (NDASOPAPI_CALLBACK *PNDAS_OP_SYNC_CALLBACK)(
	UINT32 Total,
	UINT32 Current,
	LPVOID lpParameter
);
typedef PNDAS_OP_SYNC_CALLBACK LPNDAS_OP_SYNC_CALLBACK;

/*--
Not implemented
--*/
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSynchronize(
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
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
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReadDIB(
	HNDAS hNdasDevice,
	NDAS_DIB_V2 *pDIB_V2,
	UINT32 *pnDIBSize);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDWrite(
	NDASCOMM_CONNECTION_INFO *ci,
	NDAS_RAID_META_DATA *rmd);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDRead(
	NDASCOMM_CONNECTION_INFO *ci,
	NDAS_RAID_META_DATA *rmd);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSpareAdd(
	NDASCOMM_CONNECTION_INFO *ci,
	NDASCOMM_CONNECTION_INFO *ciSpare);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSpareRemove(
	NDASCOMM_CONNECTION_INFO *ci_spare);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASOP_H_ */
