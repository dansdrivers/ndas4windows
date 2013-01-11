#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>
#include <winioctl.h>
#include <devguid.h>
#include <strsafe.h>
// #include <mountmgr.h>
#include <crtdbg.h>
#include "sdi.h"

#ifndef XTLTRACE
#define XTLTRACE printf
#endif

HRESULT
SdiEnumerateDevicesByInterface(
	__in LPCGUID DevInterfaceGuid,
	__in PSDIDEVICENAMEENUMCALLBACK EnumCallback,
	__in_opt PVOID EnumContext)
{
	HRESULT hr;

	HDEVINFO devInfoSet = SetupDiGetClassDevs(
		DevInterfaceGuid,
		NULL,
		NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE("SetupDiGetClassDevs failed, hr=0x%X\n", hr);
		return hr;
	}
	
	hr = SdiEnumerateDevicesByInterfaceInSet(
		devInfoSet,
		DevInterfaceGuid,
		EnumCallback,
		EnumContext);

	SetupDiDestroyDeviceInfoList(devInfoSet);

	return hr;
}

HRESULT
SdiEnumerateDevicesByInterfaceInSet(
	__in HDEVINFO DevInfoSet,
	__in LPCGUID DevInterfaceGuid,
	__in PSDIDEVICENAMEENUMCALLBACK EnumCallback,
	__in_opt PVOID EnumContext)
{
	HRESULT hr;

	SP_DEVICE_INTERFACE_DATA devInterfaceData;
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD index = 0;; ++index)
	{
		BOOL success = SetupDiEnumDeviceInterfaces(
			DevInfoSet,
			NULL,
			DevInterfaceGuid,
			index,
			&devInterfaceData);
			
		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			if (ERROR_NO_MORE_ITEMS == GetLastError())
			{
				return S_OK;
			}
			XTLTRACE("EnumDeviceInterfaces(%d) failed, error=x0%X\n",
					 index, hr);
			return hr;
		}

		PSP_DEVICE_INTERFACE_DETAIL_DATA devInterfaceDetail = NULL;

		DWORD detailDataSize = 0;

		success = SetupDiGetDeviceInterfaceDetail(
			DevInfoSet,
			&devInterfaceData,
			devInterfaceDetail,
			detailDataSize,
			&detailDataSize,
			NULL);

		if (!success && ERROR_INSUFFICIENT_BUFFER == GetLastError())
		{
			devInterfaceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
				malloc(detailDataSize);
			devInterfaceDetail->cbSize = 
				sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			success = SetupDiGetDeviceInterfaceDetail(
				DevInfoSet,
				&devInterfaceData,
				devInterfaceDetail,
				detailDataSize,
				&detailDataSize,
				NULL);
		}

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE("GetDeviceInterfaceDetail(%d) failed, hr=x0%X\n",
					 index, hr);
			free(devInterfaceDetail);
			continue;
		}

		hr = (*EnumCallback)(
			DevInterfaceGuid,
			devInterfaceDetail->DevicePath,
			EnumContext);

		if (FAILED(hr))
		{
			free(devInterfaceDetail);
			return hr;
		}
			
		free(devInterfaceDetail);
	}
}

HRESULT
SdiDiskGetLengthInformation(
	__in HANDLE DeviceHandle,
	__out PLARGE_INTEGER Length)
{
	HRESULT hr;
	GET_LENGTH_INFORMATION lengthInfo;
	DWORD bytesReturned;

	BOOL success = DeviceIoControl(
		DeviceHandle,
		IOCTL_DISK_GET_LENGTH_INFO,
		NULL,
		0,
		&lengthInfo,
		sizeof(GET_LENGTH_INFORMATION),
		&bytesReturned,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE("DISK_GET_LENGTH_INFO failed, hr=0x%X\n", hr);
		return hr;
	}

	XTLTRACE("Length=%I64d bytes, %I64d KiB, %I64d MiB, %I64d GiB\n", 
			 lengthInfo.Length.QuadPart,
			 lengthInfo.Length.QuadPart / 1024,
			 lengthInfo.Length.QuadPart / 1024 / 1024,
			 lengthInfo.Length.QuadPart / 1024 / 1024 / 1024);

	*Length = lengthInfo.Length;

	return S_OK;
}

HRESULT
SdiDiskGetDriveLayout(
	__in HANDLE DeviceHandle,
	__deref_out PDRIVE_LAYOUT_INFORMATION* DriveLayout)
{
	HRESULT hr;

	*DriveLayout = NULL;

	DWORD bufferSize = 
		FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION, PartitionEntry) +
		sizeof(PARTITION_INFORMATION) * 4;

	PDRIVE_LAYOUT_INFORMATION driveLayout = 
		static_cast<PDRIVE_LAYOUT_INFORMATION>(malloc(bufferSize));

	if (NULL == driveLayout)
	{
		return E_OUTOFMEMORY;
	}

	for (;;)
	{
		DWORD bytesReturned;

		ZeroMemory(driveLayout, bufferSize);

		BOOL success = DeviceIoControl(
			DeviceHandle,
			IOCTL_DISK_GET_DRIVE_LAYOUT,
			NULL,
			0,
			driveLayout,
			bufferSize,
			&bytesReturned,
			NULL);

		if (!success)
		{
			if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
			{
				bufferSize += sizeof(PARTITION_INFORMATION) * 4;
				PVOID p = realloc(driveLayout, bufferSize);
				if (NULL == p)
				{
					free(driveLayout);
					return E_OUTOFMEMORY;
				}
				driveLayout = static_cast<PDRIVE_LAYOUT_INFORMATION>(p);
				continue;
			}

			hr = HRESULT_FROM_WIN32(GetLastError());
			free(driveLayout);
			XTLTRACE("DISK_GET_DRIVE_LAYOUT failed, hr=0x%X\n", hr);
			return hr;
		}

		*DriveLayout = driveLayout;

		return S_OK;
	}
}

HRESULT
SdiDiskGetDriveLayoutEx(
	__in HANDLE DeviceHandle,
	__deref_out PDRIVE_LAYOUT_INFORMATION_EX* DriveLayout)
{
	HRESULT hr;

	*DriveLayout = NULL;

	DWORD bufSize = 
		FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry) +
		sizeof(PARTITION_INFORMATION_EX) * 4;

	PDRIVE_LAYOUT_INFORMATION_EX driveLayout = 
		static_cast<PDRIVE_LAYOUT_INFORMATION_EX>(malloc(bufSize));

	if (NULL == driveLayout)
	{
		return E_OUTOFMEMORY;
	}

	for (;;)
	{
		DWORD bytesReturned;

		ZeroMemory(driveLayout, bufSize);

		BOOL success = DeviceIoControl(
			DeviceHandle,
			IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
			NULL,
			0,
			driveLayout,
			bufSize,
			&bytesReturned,
			NULL);

		if (!success)
		{
			if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
			{
				bufSize += sizeof(PARTITION_INFORMATION_EX) * 4;
				PVOID p = realloc(driveLayout, bufSize);
				if (NULL == p)
				{
					free(driveLayout);
					return E_OUTOFMEMORY;
				}
				driveLayout = static_cast<PDRIVE_LAYOUT_INFORMATION_EX>(p);
				continue;
			}

			hr = HRESULT_FROM_WIN32(GetLastError());
			free(driveLayout);
			XTLTRACE("DISK_GET_DRIVE_LAYOUT_EX failed, hr=0x%X\n", hr);
			return hr;
		}

		*DriveLayout = driveLayout;

		return S_OK;
	}
}

HRESULT 
SdiStorageReadCapacity(
	__in HANDLE DeviceHandle,
	__out PSTORAGE_READ_CAPACITY Capacity)
{
	_ASSERT(NULL != Capacity);
	if (NULL == Capacity)
	{
		return E_POINTER;
	}

	Capacity->Version = sizeof(STORAGE_READ_CAPACITY);

	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		DeviceHandle,
		IOCTL_STORAGE_READ_CAPACITY,
		NULL,
		0,
		Capacity,
		sizeof(STORAGE_READ_CAPACITY),
		&bytesReturned,
		NULL);

	if (!success)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE("IOCTL_STORAGE_READ_CAPACITY failed, hr=0x%X\n", hr);
		return hr;
	}
	
	return S_OK;
}

HRESULT
SdiVolumeGetVolumeDiskExtents(
	__in HANDLE DeviceHandle,
	__deref_out PVOLUME_DISK_EXTENTS* Extents,
	__out_opt LPDWORD ReturnedBytes)
{
	HRESULT hr;

	if (NULL == Extents) 
	{
		return E_POINTER;
	}

	*Extents = NULL;
	if (ReturnedBytes) *ReturnedBytes = 0;

	DWORD bufSize = FIELD_OFFSET(VOLUME_DISK_EXTENTS, Extents) +
		sizeof(DISK_EXTENT) * 1;

	PVOLUME_DISK_EXTENTS vdextents = 
		static_cast<PVOLUME_DISK_EXTENTS>(malloc(bufSize));

	if (NULL == vdextents)
	{
		return E_OUTOFMEMORY;
	}

	for (;;)
	{
		DWORD bytesReturned;

		BOOL success = DeviceIoControl(
			DeviceHandle,
			IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
			NULL,
			0,
			vdextents,
			bufSize,
			&bytesReturned,
			NULL);

		if (!success)
		{
			if (ERROR_MORE_DATA == GetLastError())
			{
				bufSize = FIELD_OFFSET(VOLUME_DISK_EXTENTS, Extents) +
					sizeof(DISK_EXTENT ) * vdextents->NumberOfDiskExtents;

				PVOID p = realloc(vdextents, bufSize);
				if (NULL == p)
				{
					free(vdextents);
					return E_OUTOFMEMORY;
				}
				vdextents = static_cast<PVOLUME_DISK_EXTENTS>(p);
				continue;
			}

			hr = HRESULT_FROM_WIN32(GetLastError());
			free(vdextents);
			XTLTRACE("VOLUME_GET_VOLUME_DISK_EXTENTS failed, hr=0x%X\n", hr);
			return hr;
		}

		if (ReturnedBytes) *ReturnedBytes = bytesReturned;
		*Extents = vdextents;
		return S_OK;
	}
}


namespace dload
{

typedef BOOL WINAPI GETVOLUMEPATHNAMESFORVOLUMENAMEW(LPCWSTR, LPWCH, DWORD, PDWORD);
typedef GETVOLUMEPATHNAMESFORVOLUMENAMEW *PGETVOLUMEPATHNAMESFORVOLUMENAMEW;

BOOL
WINAPI
GetVolumePathNamesForVolumeNameW(
    __in  LPCWSTR lpszVolumeName,
    __out_ecount_part_opt(cchBufferLength, *lpcchReturnLength) __nullnullterminated LPWCH lpszVolumePathNames,
    __in  DWORD cchBufferLength,
    __out PDWORD lpcchReturnLength)
{
	HMODULE h = LoadLibraryW(L"kernel32.dll");
	if (NULL == h) return FALSE;
	PVOID p = GetProcAddress(h, "GetVolumePathNamesForVolumeNameW");
	if (NULL == p) return FALSE;
	BOOL success = static_cast<PGETVOLUMEPATHNAMESFORVOLUMENAMEW>(p)(
		lpszVolumeName, lpszVolumePathNames, cchBufferLength, lpcchReturnLength);
	DWORD e = GetLastError();
	FreeLibrary(h);
	SetLastError(e);
	return success;
}

}

HRESULT
SdiGetVolumePathNamesForVolumeName(
	__in LPCWSTR VolumeName,
	__deref_out LPWSTR* VolumePathNames)
{
	using dload::GetVolumePathNamesForVolumeNameW;

	_ASSERTE(NULL != VolumePathNames);
	if (NULL == VolumePathNames)
	{
		return E_POINTER;
	}
	*VolumePathNames = NULL;

#if _DEBUG
	DWORD namesLength = 0;
#else
	DWORD namesLength = 36;
#endif

	LPWSTR names = static_cast<LPWSTR>(calloc(namesLength, sizeof(WCHAR)));
	if (NULL == names)
	{
		return E_OUTOFMEMORY;
	}

	BOOL success;

	while (TRUE)
	{
		success = GetVolumePathNamesForVolumeNameW(
			VolumeName, 
			names,
			namesLength,
			&namesLength);

		if (!success && ERROR_MORE_DATA == GetLastError())
		{
			PVOID p = realloc(names, sizeof(WCHAR) * namesLength);
			if (NULL == p)
			{
				free(names);
				return E_OUTOFMEMORY;
			}
			names = static_cast<LPWSTR>(p);
			continue;
		}

		break;
	}

	if (!success)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	*VolumePathNames = names;

	return S_OK;
}

#if 0
HRESULT
SdiMountmgrQueryPoints(
	__in HANDLE MountmgrHandle,
	__in_opt LPCWSTR SymbolicLinkName,
	__in_opt LPCWSTR UniqueId,
	__in_opt LPCWSTR DeviceName,
	__deref_out PMOUNTMGR_MOUNT_POINTS * MountPoints)
{
	HRESULT hr;

	_ASSERT(NULL != MountPoints);
	if (NULL == MountPoints)
	{
		return E_POINTER;
	}

	*MountPoints = NULL;

	DWORD symbolicLinkNameLength = 
		SymbolicLinkName ? lstrlenW(SymbolicLinkName) * sizeof(WCHAR) : 0;
	
	DWORD uniqueIdLength = 
		UniqueId ? lstrlenW(UniqueId) * sizeof(WCHAR) : 0;
	
	DWORD deviceNameLength = 
		DeviceName ? lstrlenW(DeviceName) * sizeof(WCHAR) : 0;

	DWORD inputBufferSize = 
		sizeof(MOUNTMGR_MOUNT_POINT) +
		symbolicLinkNameLength +
		uniqueIdLength +
		deviceNameLength;

	PMOUNTMGR_MOUNT_POINT mmMountPoint = 
		static_cast<PMOUNTMGR_MOUNT_POINT>(malloc(inputBufferSize));

	if (NULL == mmMountPoint)
	{
		return E_OUTOFMEMORY;
	}

	ZeroMemory(mmMountPoint, inputBufferSize);

	ULONG nameBufferOffset = sizeof(MOUNTMGR_MOUNT_POINT);
	LPBYTE nameBuffer = reinterpret_cast<LPBYTE>(mmMountPoint + 1);

	if (symbolicLinkNameLength > 0)
	{
		mmMountPoint->SymbolicLinkNameLength = static_cast<USHORT>(symbolicLinkNameLength);
		mmMountPoint->SymbolicLinkNameOffset = nameBufferOffset;
		CopyMemory(nameBuffer, SymbolicLinkName, symbolicLinkNameLength);
		nameBuffer += symbolicLinkNameLength;
		nameBufferOffset += symbolicLinkNameLength;
	}

	if (uniqueIdLength > 0)
	{
		mmMountPoint->UniqueIdLength = static_cast<USHORT>(uniqueIdLength);
		mmMountPoint->UniqueIdOffset = sizeof(MOUNTMGR_MOUNT_POINT);
		CopyMemory(nameBuffer, UniqueId, uniqueIdLength);
		nameBuffer += uniqueIdLength;
		nameBufferOffset += uniqueIdLength;
	}

	if (deviceNameLength > 0)
	{
		mmMountPoint->SymbolicLinkNameLength = static_cast<USHORT>(deviceNameLength);
		mmMountPoint->SymbolicLinkNameOffset = sizeof(MOUNTMGR_MOUNT_POINT);
		CopyMemory(nameBuffer, DeviceName, deviceNameLength);
		nameBuffer += deviceNameLength;
		nameBufferOffset += deviceNameLength;
	}

	DWORD mmMountPointsSize = 
		FIELD_OFFSET(MOUNTMGR_MOUNT_POINTS, MountPoints) +
		sizeof(MOUNTMGR_MOUNT_POINT) * 1;

	PMOUNTMGR_MOUNT_POINTS mmMountPoints = 
		static_cast<PMOUNTMGR_MOUNT_POINTS>(malloc(mmMountPointsSize));

	if (NULL == mmMountPoints)
	{
		free(mmMountPoint);
		return E_OUTOFMEMORY;
	}

	DWORD bytesReturned;

	for (;;)
	{
		BOOL success = DeviceIoControl(
			MountmgrHandle,
			IOCTL_MOUNTMGR_QUERY_POINTS,
			mmMountPoint,
			inputBufferSize,
			mmMountPoints,
			mmMountPointsSize,
			&bytesReturned,
			NULL);

		if (!success)
		{
			if (ERROR_MORE_DATA == GetLastError())
			{
				mmMountPointsSize = mmMountPoints->Size;
				PVOID p = realloc(mmMountPoints, mmMountPointsSize);
				if (NULL == p)
				{
					free(mmMountPoints);
					free(mmMountPoint);
					return E_OUTOFMEMORY;
				}
				mmMountPoints = static_cast<PMOUNTMGR_MOUNT_POINTS>(p);
				continue;
			}

			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE("MOUNTMGR_QUERY_POINTS failed, hr=0x%X\n", hr);
			free(mmMountPoints);
			free(mmMountPoint);
			return hr;
		}

		*MountPoints = mmMountPoints;

		return S_OK;
	}
}
#endif
