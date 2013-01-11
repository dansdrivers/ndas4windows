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
BOOL
NDASOPAPI
NdasOpMigrate(
   CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo
);

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

NDASOP_LINKAGE
UINT32
NDASOPAPI
NdasOpBind(
	UINT32	nDiskCount,
	CONST NDASCOMM_CONNECTION_INFO *pConnectionInfo,
	UINT32	BindType,
	UINT32	uiUserSpace
);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpIsBitmapClean(
	HNDAS hNdasDevice,
	BOOL *pIsClean);

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
	NDAS_DIB_V2 *DIB_V2_Ref,
	NDAS_RAID_META_DATA *rmd);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRMDRead(
	NDAS_DIB_V2 *DIB_V2_Ref,
	NDAS_RAID_META_DATA *rmd);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSpareAdd(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ciSpare);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpSpareRemove(
	CONST NDASCOMM_CONNECTION_INFO *ci_spare);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpClearDefectMark(
	CONST NDASCOMM_CONNECTION_INFO *ci,	// Primary device's CI
	CONST UINT32 nReplace	// DIB index whose defect is cleared
);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpRemoveFromRaid(
	CONST NDASCOMM_CONNECTION_INFO *ci,	// Primary device's CI
	CONST UINT32 nRemove	// DIB index to being replaced
);

NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReplaceDevice(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ci_replace,
	CONST UINT32 nReplace);

#if 0
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReplaceUnitDevice(
	CONST NDASCOMM_CONNECTION_INFO *ci,
	CONST NDASCOMM_CONNECTION_INFO *ci_replace,
	CONST UINT32 nReplace);
#endif

/*
Writes BACL information on hNdasDevice
*/
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpWriteBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL);

/*
Reads BACL information into pBACK
If 0 == pBACL->ElementCount, set ElementCount (0 if no list)
*/
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpReadBACL(
	HNDAS hNdasDevice,
	BLOCK_ACCESS_CONTROL_LIST *pBACL);


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
	// RAID members config is changed.
	NDAS_RAID_FAIL_REASON_DIFFERENT_CONFIG_SET    = 0x00000008,
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
	// RAID members are updated independently in degraded mode.
	NDAS_RAID_FAIL_REASON_INDEPENDENT_UPDATE      = 0x00004000,
	// One of RAID member is disabled state.
	NDAS_RAID_FAIL_REASON_MEMBER_DISABLED = 0x00008000, 
} NDAS_RAID_FAIL_REASON;

//
// Information about each member
//
typedef enum _NDAS_RAID_MEMBER_FLAG {
	// This member is active RAID member
	NDAS_RAID_MEMBER_FLAG_ACTIVE               = 0x00000001,
	// This member is spare
	NDAS_RAID_MEMBER_FLAG_SPARE                = 0x00000002,
	// Member is found
	NDAS_RAID_MEMBER_FLAG_ONLINE               = 0x00000004,
	// Member is not found
	NDAS_RAID_MEMBER_FLAG_OFFLINE              = 0x00000008,
	// Data is not synced. Disk can be used.
	NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC          = 0x00000010,
	// RAID set ID is different.
	NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET   = 0x00000020,
	// Config set ID is different.
	NDAS_RAID_MEMBER_FLAG_DIFFERENT_CONFIG_SET = 0x00000040,
	// Device is online but IO operation failed
	NDAS_RAID_MEMBER_FLAG_IO_FAILURE           = 0x00000080,
	// This member does not have valid RMD
	NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED        = 0x00000100,
	// DIB does not match with the primary. 
	// This member may be not a member of this RAID set.
	NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH         = 0x00000200,
	// Disk is marked as defect.
	NDAS_RAID_MEMBER_FLAG_DEFECTIVE            = 0x00000400,
	// Disk is not registered. Cannot be set by ndasop.
	NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED           = 0x00000800,


	// This member disk didn't prove itself that it is the member of this RAID set.
	NDAS_RAID_MEMBER_FLAG_INVALID_MEMBER = NDAS_RAID_MEMBER_FLAG_OFFLINE|
												NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET|
												NDAS_RAID_MEMBER_FLAG_DIFFERENT_CONFIG_SET|
												NDAS_RAID_MEMBER_FLAG_IO_FAILURE|
												NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED|
												NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH|
												NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED												
} NDAS_RAID_MEMBER_FLAG;

__inline BOOL NdasOpIsMemberUsable(DWORD NdasRaidMemberFlags);

__inline BOOL NdasOpIsMemberUsable(DWORD NdasRaidMemberFlags)
{
	return (!(NdasRaidMemberFlags & (
		NDAS_RAID_MEMBER_FLAG_OFFLINE |
		NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET|
		NDAS_RAID_MEMBER_FLAG_DIFFERENT_CONFIG_SET|
		NDAS_RAID_MEMBER_FLAG_IO_FAILURE|
		NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED|
		NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH|
		NDAS_RAID_MEMBER_FLAG_DEFECTIVE)) && 
		(NdasRaidMemberFlags & NDAS_RAID_MEMBER_FLAG_ONLINE));
}

typedef struct _NDASOP_RAID_INFO {
	DWORD 				Size;
	DWORD				Type;				// NMT_*
	DWORD 				MountablityFlags; 	// Actually NDAS_RAID_MOUNTABILITY_FLAGS type, but used DWORD for packing
											// ORed value of RAID_MOUNTABLITY_*. 
	DWORD				FailReason;			// Reason to being unmountable or being degraded mode.
											// Actually NDAS_RAID_FAIL_REASON type, but used DWORD type for packing
	DWORD				MemberCount;
	DWORD				TotalBitCount;
	DWORD				OosBitCount;
	DWORD				Reserved[1]; 		// Padding to 16 byte boundary.
	GUID 				RaidSetId;			// Valid only when RAID_MOUNTABLITY_MOUNTABLE
	GUID 				ConfigSetId;			// Valid only when RAID_MOUNTABLITY_MOUNTABLE. 
	struct {
		NDAS_DEVICE_ID DeviceId;
		UCHAR			UnitNo;
		UCHAR			DefectCode; // From primary device's RMD. NDAS_UNIT_DEFECT_*
		UCHAR			Reserved[2];
		DWORD			Flags;				// ORed value of RAID_MEMBERFLAG_*
	} Members[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];
} NDASOP_RAID_INFO, *PNDASOP_RAID_INFO;

//
// Return FALSE if internal error such as struct size is mismatch or memory failure occurs. Return TRUE in other cases.
//
NDASOP_LINKAGE
BOOL
NDASOPAPI
NdasOpGetRaidInfo(
	HNDAS hNdasDevice,		// Primary device
	PNDASOP_RAID_INFO RaidInfo);

#include <poppack.h>

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASOP_H_ */
