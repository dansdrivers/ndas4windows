/*++

NDAS Device Operation API Header

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _NDASOP_H_
#define _NDASOP_H_

#include <ndas/ndastype.h>
#include <ndas/ndasctype.h>
#include <ndas/ndasdib.h>

#pragma once

#if defined(_WINDOWS_)

#ifndef _INC_WINDOWS
	#error ndasop.h requires windows.h to be included first
#endif

#if   defined(NDASOP_EXPORTS)
#define NDASOP_LINKAGE
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

#ifdef __cplusplus
extern "C" {
#endif


C_ASSERT( sizeof(HRESULT) == 4 );

#include <pshpack1.h>

/* 

Use the following definition for WinCE specifics
#if defined(_WIN32_WCE_)
#endif

*/

#define NDASOP_API_VERSION_MAJOR 0x0001
#define NDASOP_API_VERSION_MINOR 0x0010

/*++

NdasOpGetAPIVersion function returns the current version information
of the loaded library

Return Values:

Low word contains the major version number and high word the minor
version number.

--*/

NDASOP_LINKAGE
DWORD
NDASOPAPI
NdasOpGetAPIVersion();

//
// RAID_MOUNTABLITY flags.
//
// 	
typedef enum _NDAS_RAID_MOUNTABILITY_FLAGS {

	// Set if RAID is unmountable
	NDAS_RAID_MOUNTABILITY_UNMOUNTABLE   = 0x00000001, 
	// Set if RAID is mountable
	NDAS_RAID_MOUNTABILITY_MOUNTABLE     = 0x00000002,
	// RAID is normal status. 
	// Set only when RAID_MOUNTABLITY_MOUNTABLE is set
	NDAS_RAID_MOUNTABILITY_NORMAL        = 0x00000004, 
	// RAID can be mounted in degraded mode. 
	// Set only when RAID_MOUNTABLITY_MOUNTABLE is set
	NDAS_RAID_MOUNTABILITY_DEGRADED      = 0x00000008,

	//
	// Spare member information
	//

	// Set if one or more spare disk is missing.
	NDAS_RAID_MOUNTABILITY_MISSING_SPARE = 0x00000100,
	// One or more spare disk is available. 
	// If more than 1 disk are configured as spare, 
	// RAID_MOUNTABLITY_MISSING_SPARE and RAID_MOUNTABLITY_SPARE_EXIST 
	// can be co-exist. 
	NDAS_RAID_MOUNTABILITY_SPARE_EXIST   = 0x00000200,

	//
	// Out-of-sync member exist.
	//
	NDAS_RAID_MOUNTABILITY_OUT_OF_SYNC  = 0x00000400

} NDAS_RAID_MOUNTABILITY_FLAGS;

//
//	Reason for being unmountable or being degraded mode.
//
typedef enum _NDAS_RAID_FAIL_REASON {
	NDAS_RAID_FAIL_REASON_NONE                    = 0x00000000,
	NDAS_RAID_FAIL_REASON_RMD_CORRUPTED           = 0x00000001,
	// Active member is off-line
	NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE          = 0x00000002,
	// DIB information is different.
	NDAS_RAID_FAIL_REASON_DIB_MISMATCH            = 0x00000004,
	// Spare disk is used. Mount is rejected when spare is used.
	NDAS_RAID_FAIL_REASON_SPARE_USED = 0x00000008,
	// DIB information is inconsistent.
	NDAS_RAID_FAIL_REASON_INCONSISTENT_DIB    = 0x00000010,
	// For future version of DIB
	NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION = 0x00000020,
	// For previous revision of RAID
	NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED      = 0x00000040,
	// For future version of RAID
	NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID        = 0x00000080,
	// Active member is defective
	NDAS_RAID_FAIL_REASON_DEFECTIVE               = 0x00000100,
	// RAID set is different
	NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET      = 0x00000200,
	// ndasop does not know the list of registered members, so cannot be set by ndasop. 
	NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED = 0x00000400, 
	// Access right is not enough 
	/* NDAS_RAID_FAIL_REASON_NO_ACCESS             = 0x00000800, */
	// Failed to perform IO operation on a member disk
	NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL          = 0x00001000,
	// Selected member is not a RAID member
	NDAS_RAID_FAIL_REASON_NOT_A_RAID              = 0x00002000,
	// RAID members are updated seperately in degraded mode.
	NDAS_RAID_FAIL_REASON_IRRECONCILABLE      = 0x00004000,
	// One of RAID member is disabled state.
	NDAS_RAID_FAIL_REASON_MEMBER_DISABLED = 0x00008000, 
	NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT = 0x00010000,
} NDAS_RAID_FAIL_REASON;


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
NMT_SINGLE,	NMT_AGGREGATE,	NMT_RAID0, NMT_RAID1R2, NMT_RAID4R2, NMT_RAID1R3, NMT_RAID4R3, NMT_SAFE_RAID1
NMT_SINGLE unbinds disks.

uiUserSpace
[in] If non-zero, NdasOpBind uses uiUserSpace as sizeUserSpace in DIB

Return Values:

If the function succeeds, the return value is nDiskCount.

If the function fails, the return value is not nDiskCount.
Depends on GetLastError, return value can be a index of pUnitDeviceIDs which failed to operate.

--*/

typedef struct _NDASCOMM_CONNECTION_INFO *PNDASCOMM_CONNECTION_INFO;
typedef struct _NDASCOMM_HANDLE			 *HNDAS;

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpBind (
	PNDASCOMM_CONNECTION_INFO ConnectionInfo,
	UINT32					  DiskCount,
	NDAS_MEDIA_TYPE			  BindType,
	UINT32					  UserSpace,
	UINT32					  RemoveIdx = 0
	);

/*++

NdasOpMigrate function Migrates older RAID format to current
Mirror -> RAID1R
RAID1 -> RAID1R
RAID4 -> RAID4R

Parameters:

pConnectionInfo
[in] An pointer of NDASCOMM_CONNECTION_INFO  to migrate

Return Values:

If the function succeeds, the return value is nDiskCount.

If the function fails, the return value is not nDiskCount.
Depends on GetLastError, return value can be a index of pUnitDeviceIDs which failed to operate.

--*/

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpMigrate (
   CONST PNDASCOMM_CONNECTION_INFO ConnectionInfo
   );


NDASOP_LINKAGE
HRESULT
NDASOPAPI
GetNdasSimpleSerialNo (
	PNDAS_UNITDEVICE_HARDWARE_INFO	UnitInfo,
	PCHAR							NdasSimpleSerialNo
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
HRESULT
NDASOPAPI
NdasOpReadDib (
	HNDAS		Hndas,
	NDAS_DIB_V2 *DibV2,
	UINT32		*DibSize
	);

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasVsmGetRaidSimpleStatus (
	IN  HNDAS						 NdasHandle,
	IN  PNDAS_LOGICALUNIT_DEFINITION Definition,
	IN  PUINT8						 NdasUnitNo,
	OUT	LPDWORD						 RaidSimpleStatusFlags
	);

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasVsmInitializeLogicalUnitDefinition (
	IN		PNDAS_DEVICE_ID				 DeviceId,
	IN		PCHAR						 UnitSimpleSerialNo,
	IN OUT	PNDAS_LOGICALUNIT_DEFINITION Definition
	);

NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasVsmReadLogicalUnitDefinition (
	IN	   HNDAS					    NdasHandle,
	IN OUT PNDAS_LOGICALUNIT_DEFINITION Definition
	);

HRESULT
NdasOpGetOutOfSyncStatus (
	HNDAS			Hndas, 
	PNDAS_DIB_V2	DibV2,
	DWORD*			TotalBits, 
	DWORD*			SetBits
	);

/*
Writes BACL information on hNdasDevice
*/
NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpWriteBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL);

/*
Reads BACL information into pBACK
If 0 == pBACL->ElementCount, set ElementCount (0 if no list)
*/
NDASOP_LINKAGE
HRESULT
NDASOPAPI
NdasOpReadBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL);


#include <poppack.h>

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASOP_H_ */
