/*++

	NDAS BIND API header

	Copyright (C)2002-2004 XIMETA, Inc.
	All rights reserved.

--*/

#ifndef _NDASBIND_H_
#define _NDASBIND_H_

#if defined(_WINDOWS_)

#ifndef _INC_WINDOWS
	#error ndasbind.h requires windows.h to be included first
#endif

#ifdef NDASBIND_EXPORTS
#define NDASBIND_API __declspec(dllexport)
#else
#define NDASBIND_API __declspec(dllimport)
#endif

#else /* defined(_WINDOWS_) */
#define NDASBIND_API 
#endif

#ifndef _NDAS_TYPE_H_
#include "ndastype.h"
#endif /* _NDAS_TYPE_H_ */

#include "winbase.h"
#ifdef __cplusplus
extern "C" {
#endif 

/*
 * Structure for specifying a unitdevice
 */
typedef struct _NDAS_UNITDEVICE
{
	DWORD dwSlotNo;		/* Slot number of the device to which the unitdevice belongs */
	DWORD dwUnitNo;		/* Unit number of the unitdevice in the device */
} NDAS_UNITDEVICE, *PNDAS_UNITDEVICE;

/*
 * Structure for containing group of unitdevices
 */
typedef struct _NDAS_UNITDEVICE_GROUP
{
	ULONG cUnitDevices;					/* Number of unitdevices in the aUnitDevices array */
	PNDAS_UNITDEVICE aUnitDevices;	/* Pointer to an array of NDAS_UNITDEVICE_ID */
} NDAS_UNITDEVICE_GROUP, *PNDAS_UNITDEVICE_GROUP;


/*
 * Bind group of unitdevices.
 * The unitdevices will be initialized after they are bound.
 * Here, initialization means clearing MBR and partition informations, thus,
 * the data in the unitdevices cannot be used after initialization.
 *
 * Parameters:
 *
 *  pUnitDeviceIdGroup 
 *   [in] Pointer to a NDAS_UNITDEVICE_ID_GROUP structure that contains the IDs of unitdevices to bind
 * 
 *  diskType
 *   [in] NDAS_UNITDEVICE_DISK_TYPE that specifies desired type of binding.
 *        
 * Return values:
 *
 * If the function succeeds, the return value is non-zero.
 *
 * If the function fails, the return value is zero. To get extended error 
 * information, call GetLastError.
 * The most frequent cases of error are as belows
 *  - One or more unitdevices are not reachable or being used by other program/computer.
 *  - One or more unitdevices are already bound with other unitdevices
 *    These unitdevices should be unbound beforehand.
 */
NDASBIND_API
BOOL
WINAPI
NdasBindUnitDevices(
	PNDAS_UNITDEVICE_GROUP pUnitDeviceIdGroup, 
	NDAS_UNITDEVICE_DISK_TYPE diskType
	);

/*
 * Clear binding information in the unitdevices.
 * The unitdevices will be initialized if each disk cannot be used seperately.
 * see NdasUnBindLogcialDevice for more information about initialization.
 *
 * Parameters:
 *  
 *  pUnitDeviceIdGroup
 *   [in] Pointer to a NDAS_UNITDEVICE_ID_GROUP structure that contains the IDs of unitdevices to unbind.
 *        This function does not verify whether they are in the same binding, it is the responsibility of
 *        caller to verify it. 
 *
 * Return values:
 * 
 * If the function succeeds, the return value is non-zero.
 *
 * If the function fails, the return value is zero. To get extended error 
 * information, call GetLastError.
 * The most frequent cases of error are as belows
 *  - One or more unitdevices are not reachable or being used by other program/computer.
 *  - One or more unitdevices are already unbound.
 */
NDASBIND_API
BOOL
WINAPI
NdasUnBindUnitDevices(
	PNDAS_UNITDEVICE_GROUP pUnitDeviceIdGroup
	);

/*
 * Add a mirror to a unitdevice
 * The data in the master unitdevice will be retained.
 * The unitdevices should be synchronized after the mirror is added.
 *
 * Parameters:
 *
 *  dwMasterSlotNo
 *   [in] Slot number of the device to which the unitdevice belongs
 *        The Master unitdevice will be used as a source disk
 *  dwMasterUnitNo
 *   [in] Unit number of the unitdevice in the device
 *        The Master unitdevice will be used as a source disk
 *  dwMirrorSlotNo
 *   [in] Slot number of the device to which the unitdevice belongs
 *  dwMirrorUnitNo
 *   [in] Unit number of the unitdevice in the device
 *        The Mirror unitdevice will be used as a mirror disk
 *
 * Return values:
 * 
 * If the function succeeds, the return value is non-zero.
 *
 * If the function fails, the return value is zero. To get extended error 
 * information, call GetLastError.
 */
NDASBIND_API
BOOL
WINAPI
NdasAddMirror(
	DWORD dwMasterSlotNo, DWORD dwMasterUnitNo,
	DWORD dwMirrorSlotNo, DWORD dwMirrorUnitNo
	);


/*
 * Definition of a callback function used by NdasSynchronize
 *
 * Parameters:
 *  
 *  TotalDiskSectorCount
 *   [in] Total number of sectors on the disk
 *  TotalDiskSectorsScanned
 *   [in] Number of sectors scanned.
 *  DirtyDiskSectorCount
 *   [in] Total number of sectors to be copied
 *  DirtyDiskSectorCopied
 *   [in] Number of sectors copied.
 *  lpData
 *   [in] Argument passed to NdasSynchronize function
 * Return values:
 *	
 *	 If you want to continue the synchronization, the function should return non-zero value,
 *   otherwise the function should return zero
 *	 
 */
typedef
BOOL
(CALLBACK *LPSYNC_PROGRESS_ROUTINE)(
	LARGE_INTEGER TotalDiskSectorCount,
	LARGE_INTEGER TotalDiskSectorsScanned,
	LARGE_INTEGER DirtyDiskSectorCount,
	LARGE_INTEGER DirtyDiskSectorCopied,
	LPVOID lpData
	);

/*
 * Synchronize the data in the mirror unitdevice with the data in the source unitdevice.
 * Data which is marked as dirty will be copied to the mirror unitdevice.
 *
 * Parameters:
 *
 *  dwSourceSlotNo
 *   [in] Slot number of the device to which the unitdevice belongs
 *  dwSourceUnitNo      
 *   [in] Unit number of the unitdevice in the device
 *  dwMirrorSlotNo
 *   [in] Slot number of the device to which the unitdevice belongs
 *  dwMirrorUnitNo
 *   [in] Unit number of the unitdevice in the device
 *  lpProgressRoutine
 *   [in] Address of a callback function of type LPSYNC_PROGRESS_ROUTINE that is called each time
 *        another portion of the data has been copied. The amount of data copied between each call can vary each time.
 *        This parameter can be NULL.
 *  lpData
 *   [in] Argument to be passed to the callback function. This parameter can be NULL.
 *  pbCancel
 *   [in] If this flag is set to TRUE during the synchronization, the operation will be canceled. 
 *        Otherwise, the copy operation will continue to completion. 
 * 
 * Return values:
 * 
 * If the function succeeds, the return value is non-zero.
 *
 * If the function fails, the return value is zero. To get extended error 
 * information, call GetLastError.
 */
NDASBIND_API
BOOL
WINAPI
NdasSynchronize(
	DWORD dwSourceSlotNo, DWORD dwSourceUnitNo,
	DWORD dwMirrorSlotNo, DWORD dwMirrorUnitNo,
	LPSYNC_PROGRESS_ROUTINE lpProgressRoutine,
	LPVOID lpData, 
	LPBOOL pbCancel
	);


#ifdef __cplusplus
}	/* extern "C" */
#endif 


#endif /* _NDASBIND_H_ */