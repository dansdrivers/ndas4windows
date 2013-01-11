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

HRESULT
__stdcall
pGetDeviceUINumber(
	__in HDEVINFO DeviceInfoSet, 
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__out LPDWORD UINumber);

//
// Calls CM_Request_Device_Eject for the given NDAS SCSI controller.
//
// Successful return does not ensure that the ejection is done.
// Check the value of pConfigRet for the success also.
//

HRESULT 
__stdcall 
pRequestNdasScsiDeviceEjectW(
	__in NDAS_LOCATION NdasLocation,
	__out_opt CONFIGRET* pConfigRet, 
	__out_opt PPNP_VETO_TYPE pVetoType, 
	__out_ecount_opt(nNameLength) LPTSTR pszVetoName, 
	__in_opt DWORD nNameLength);

//
// Calls CM_Request_Device_Eject for the given NDAS port controller.
//
// Successful return does not ensure that the ejection is done.
// Check the value of pConfigRet for the success also.
//

HRESULT 
pRequestNdasPortDeviceEjectW(
	__in ULONG NdasLogicalUnitAddress,
	__out_opt CONFIGRET* pConfigRet, 
	__out_opt PPNP_VETO_TYPE pVetoType, 
	__out_ecount_opt(nNameLength) LPWSTR pszVetoName, 
	__in_opt DWORD nNameLength);

//
// Returns the disk device name of the given NDAS SCSI location.
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//

LPTSTR 
__stdcall
pGetDiskForNdasLocation(
	__in NDAS_LOCATION NdasLocation);

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
pGetDiskForNdasLocationEx(
	__in NDAS_LOCATION NdasLocation,
	__in BOOL SymbolicLinkOrDevInstId);

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
pGetVolumesForNdasLocation(
	__in NDAS_LOCATION NdasLocation);

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
pGetVolumeInstIdListForDiskW(
	__in LPCWSTR DiskInstId);

// devrel.cpp

/* NDASSCSI helpers */

HRESULT
__stdcall
pGetNdasSlotNumberFromDeviceHandle(
	__in HANDLE DeviceHandle, 
	__out LPDWORD NdasSlotNumber);

HRESULT 
__stdcall
pGetNdasSlotNumberFromDeviceNameW(
	__in LPCWSTR DeviceName,
	__out LPDWORD NdasSlotNumber);

HRESULT
__stdcall
pGetNdasSlotNumberForScsiPortNumber(
	__in DWORD ScsiPortNumber,
	__out LPDWORD NdasSlotNumber);

HRESULT
pGetNdasSlotNumberFromStorageDeviceHandle(
	__in HANDLE DiskHandle,
	__out LPDWORD NdasSlotNumber);

HRESULT
pGetNdasSlotNumberFromDiskNumber(
	__in DWORD DiskNumber,
	__out LPDWORD NdasSlotNumber);

/* SCSI Address for Disk helpers */

HRESULT
__stdcall
pGetScsiAddressForDisk(
	__in HANDLE hDisk,
	__out PSCSI_ADDRESS ScsiAddress);

HRESULT
__stdcall
pGetScsiAddressForDiskDeviceName(
	__in LPCTSTR DiskDeviceName,
	__out PSCSI_ADDRESS ScsiAddress);

HRESULT
__stdcall
pGetScsiAddressForDiskNumber(
	__in DWORD DiskNumber,
	__out PSCSI_ADDRESS ScsiAddress);

/* Volume to Disks helper */

HRESULT
__stdcall
pIsVolumeSpanningNdasDevice(
	__in HANDLE hVolume);

/* The caller should free the returned buffer with HeapFree if not null */
HRESULT 
__stdcall
pGetVolumeDeviceNameForMountPointW(
	__in LPCWSTR VolumeMountPoint,
	__out LPWSTR* VolumeDeviceName);

/* The caller should free the returned buffer with HeapFree if not null */
HRESULT
__stdcall
pGetVolumeMountPointForPathW(
	__in LPCWSTR Path,
	__out LPWSTR* MountPoint);

//
// Get the device number ( disk device number )
//

HRESULT
__stdcall
pGetStorageDeviceNumber(
	HANDLE hDevice, 
	PULONG	DeviceNumber);

HANDLE
__stdcall
pOpenStorageDeviceByNumber(
	DWORD StorageDeviceNumber,
	ACCESS_MASK DeviceFileAccessMask
);

#ifdef __cplusplus
}
#endif
