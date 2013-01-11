#pragma once
#include <windows.h>
#include <tchar.h>

#ifndef _CFGMGR32_H_
#error This header requires cfgmgr32.h to be included first
#endif

#ifndef _INC_SETUPAPI
#error This header requires setupapi.h to be included first
#endif

#ifndef _NTDDSCSIH_
#error this header requires ntddscsi.h to be included first
#endif

#ifndef _NDAS_TYPE_H_
#error This header requires ndastype.h to be included first
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// Returns the DeviceUI number property of the device instance
// (Internal use only)
//

DWORD  
__stdcall
pGetDeviceUINumber(
	HDEVINFO DeviceInfoSet, 
	PSP_DEVINFO_DATA DeviceInfoData);

//
// Calls CM_Request_Device_Eject for the given NDAS SCSI controller.
//
// Successful return does not ensure that the ejection is done.
// Check the value of pConfigRet for the success also.
//

BOOL 
__stdcall 
pRequestNdasScsiDeviceEject(
	DWORD SlotNo,
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName, 
	DWORD nNameLength);

//
// Returns the disk device name of the given NDAS SCSI location.
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//

LPTSTR 
__stdcall
pGetDiskForNdasScsiLocation(
	CONST NDAS_SCSI_LOCATION* NdasScsiLocation);

//
// Returns the disk device name or device instance id depending on 
// the value of SymbolicLinkOrDeviceInstId, of which value indicates
// to return disk device name if it is non-zero.
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//

LPTSTR 
__stdcall
pGetDiskForNdasScsiLocationEx(
	CONST NDAS_SCSI_LOCATION* NdasScsiLocation,
	BOOL SymbolicLinkOrDevInstId);

//
// Returns the symbolic link name list of the volume.
// Each entry is null-terminated and the last entry is terminated 
// by an additional null character
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//

LPTSTR 
__stdcall
pGetVolumesForNdasScsiLocation(
	CONST NDAS_SCSI_LOCATION* NdasScsiLocation);


//
// Returns volume device instance id list of the disk device instance id.
// Each entry is null-terminated and the last entry is terminated 
// by an additional null character
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//

LPTSTR 
__stdcall
pGetVolumeInstIdListForDisk(
	LPCTSTR DiskInstId);

// devrel.cpp

/* NDASSCSI helpers */

BOOL 
__stdcall
pGetNdasSlotNumberForScsiPort(
	HANDLE hScsiPort, 
	LPDWORD NdasSlotNumber);

BOOL 
__stdcall
pGetNdasSlotNumberForScsiPortDeviceName(
	LPCTSTR ScsiPortDeviceName,
	LPDWORD NdasSlotNumber);

BOOL 
__stdcall
pGetNdasSlotNumberForScsiPortNumber(
	DWORD ScsiPortNumber,
	LPDWORD NdasSlotNumber);

/* SCSI Address for Disk helpers */

BOOL 
__stdcall
pGetScsiAddressForDisk(
	IN HANDLE hDisk,
	OUT PSCSI_ADDRESS ScsiAddress);

BOOL 
__stdcall
pGetScsiAddressForDiskDeviceName(
	IN LPCTSTR DiskDeviceName,
	OUT PSCSI_ADDRESS ScsiAddress);

BOOL 
__stdcall
pGetScsiAddressForDiskNumber(
	IN DWORD DiskNumber,
	OUT PSCSI_ADDRESS ScsiAddress);

/* Disk to NDAS SCSI helper */

BOOL 
__stdcall
pGetNdasScsiLocationForDisk(
	HANDLE hDisk,
	PNDAS_SCSI_LOCATION pNdasScsiLocation);

/* Volume to Disks helper */

BOOL 
__stdcall
pIsVolumeSpanningNdasDevice(
	HANDLE hVolume);

/* The caller should free the returned buffer with HeapFree if not null */
LPTSTR 
__stdcall
pGetVolumeDeviceNameForMountPoint(
	LPCTSTR VolumeMountPoint);

/* The caller should free the returned buffer with HeapFree if not null */
LPTSTR
__stdcall
pGetVolumeMountPointForPath(
	LPCTSTR Path);

#ifdef __cplusplus
}
#endif
