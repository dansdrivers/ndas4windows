#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef
__callback
HRESULT
CALLBACK
SDIDEVICENAMEENUMCALLBACK(
	__in LPCGUID DevInterfaceGuid,
	__in LPCWSTR DeviceName,
	__in PVOID Context);

typedef SDIDEVICENAMEENUMCALLBACK *PSDIDEVICENAMEENUMCALLBACK;

HRESULT
WINAPI
SdiEnumerateDevicesByInterfaceInSet(
	__in HDEVINFO DevInfoSet,
	__in LPCGUID DevInterfaceGuid,
	__in PSDIDEVICENAMEENUMCALLBACK EnumCallback,
	__in_opt PVOID EnumContext);

HRESULT
WINAPI
SdiEnumerateDevicesByInterface(
	__in LPCGUID DevInterfaceGuid,
	__in PSDIDEVICENAMEENUMCALLBACK EnumCallback,
	__in_opt PVOID EnumContext);

HRESULT 
WINAPI
SdiStorageReadCapacity(
	__in HANDLE DeviceHandle,
	__out PSTORAGE_READ_CAPACITY Capacity);

HRESULT
WINAPI
SdiDiskGetLengthInformation(
	__in HANDLE DeviceHandle,
	__out PLARGE_INTEGER Length);

HRESULT
SdiDiskGetDriveLayout(
	__in HANDLE DeviceHandle,
	__deref_out PDRIVE_LAYOUT_INFORMATION* DriveLayout);

HRESULT
WINAPI
SdiDiskGetDriveLayoutEx(
	__in HANDLE DeviceHandle,
	__deref_out PDRIVE_LAYOUT_INFORMATION_EX* DriveLayout);

HRESULT
WINAPI
SdiVolumeGetVolumeDiskExtents(
	__in HANDLE DeviceHandle,
	__deref_out PVOLUME_DISK_EXTENTS* Extents,
	__out_opt LPDWORD ReturnedBytes);

HRESULT
SdiGetVolumePathNamesForVolumeName(
	__in LPCWSTR VolumeName,
	__deref_out LPWSTR* VolumePathNames);

#if 0

HRESULT
WINAPI
SdiMountmgrQueryPoints(
	__in HANDLE MountmgrHandle,
	__in_opt LPCWSTR SymbolicLinkName,
	__in_opt LPCWSTR UniqueId,
	__in_opt LPCWSTR DeviceName,
	__deref_out PMOUNTMGR_MOUNT_POINTS * MountPoints);

#endif

#ifdef __cplusplus
}
#endif
