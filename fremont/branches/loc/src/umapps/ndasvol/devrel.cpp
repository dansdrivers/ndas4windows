#include "precomp.hpp"
#include <initguid.h>
#include <ntddstor.h>
#include <ndasscsiioctl.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndasvolex.h>
#include <xtl/xtltrace.h>
#include <xtl/xtlautores.h>
#include "winioctlhelper.h"

namespace
{

const DWORD DeviceFileAccess = GENERIC_READ;
const DWORD DeviceFileShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

void 
pRemoveTrailingBackslash(LPTSTR lpBuffer)
{
	LPTSTR lp = lpBuffer;
	if (0 == lpBuffer || 0 == *lp)
	{
		return;
	}
	while (0 != *lp)
	{
		++lp;
	}
	--lp;
	while (_T('\\') == *lp && lp > lpBuffer)
	{
		*lp = 0;
		--lp;
	}
}


HANDLE
pOpenDiskDevice(LPCTSTR DiskName)
{
	XTL::AutoFileHandle hDisk = ::CreateFile(
		DiskName, 
		DeviceFileAccess, 
		DeviceFileShareMode,
		NULL, 
		OPEN_EXISTING, 
		0, 
		NULL);
	if (hDisk.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"CreateFile(%ls) failed.\n", DiskName);
		return INVALID_HANDLE_VALUE;
	}
	return hDisk.Detach();
}

HANDLE
pOpenDiskDeviceByNumber(DWORD DiskNumber)
{
	// Device Name Format: \\.\PhysicalDriveXX
	TCHAR deviceName[24];
	XTLVERIFY(SUCCEEDED(
		::StringCchPrintf(
			deviceName, RTL_NUMBER_OF(deviceName), 
			_T("\\\\.\\PhysicalDrive%d"), DiskNumber)));
	return pOpenDiskDevice(deviceName);
}

HANDLE
pOpenScsiPortDevice(
	LPCTSTR ScsiPortDeviceName)
{
	//
	// Open SCSI Port Device
	//

	/* SCSI miniport IOCTL required GENERIC_WRITE for now, really? */
	XTL::AutoFileHandle hScsiPort = ::CreateFile(
		ScsiPortDeviceName,
		DeviceFileAccess | GENERIC_WRITE, 
		DeviceFileShareMode,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hScsiPort.IsInvalid()) 
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			_T("CreateFile(%s) failed, error=0x%X\n"), 
			ScsiPortDeviceName, GetLastError());
		return INVALID_HANDLE_VALUE;
	}

	return hScsiPort.Detach();
}

HANDLE
pOpenScsiPortDeviceByNumber(
	DWORD ScsiPortNumber)
{
	//
	// Make up SCSI Port Name
	//
	
	TCHAR scsiPortName[MAX_PATH];

	XTLVERIFY(SUCCEEDED(
		::StringCchPrintf(
			scsiPortName, MAX_PATH,
			_T("\\\\.\\Scsi%d:"), ScsiPortNumber)));

	XTLTRACE2(NdasVolTrace, 2, _T("SCSI Port Name: %s\n"), scsiPortName);

	return pOpenScsiPortDevice(scsiPortName);
}


}


//////////////////////////////////////////////////////////////////////////
//
// pGetNdasSlotNumberForScsiPortXXX
//
//////////////////////////////////////////////////////////////////////////

HRESULT
pGetNdasSlotNumberFromDeviceHandle(
	__in HANDLE DeviceHandle, 
	__out LPDWORD NdasSlotNumber)
{
	//
	// Make up IOCTL In-parameter
	// NDASSCSI_IOCTL_GET_SLOT_NO to get Logical Device Slot No
	//
	const DWORD cbHeader = sizeof(SRB_IO_CONTROL);
	const DWORD cbData = sizeof(ULONG);
	const DWORD cbInBuffer = cbHeader + cbData;
	DWORD cbReturned = 0;

	BYTE lpInBuffer[cbInBuffer] = {0};
	PSRB_IO_CONTROL pSrbIoControl = (PSRB_IO_CONTROL) lpInBuffer;

	pSrbIoControl->HeaderLength = cbHeader;
	::CopyMemory(pSrbIoControl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	pSrbIoControl->Timeout = 10;
	pSrbIoControl->ControlCode = ULONG(NDASSCSI_IOCTL_GET_SLOT_NO);
	pSrbIoControl->Length = cbData;

	//
	// Get Logical Device Slot No
	//

	BOOL success = ::DeviceIoControl(
		DeviceHandle,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	if (!success) 
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"DeviceIoControl(IOCTL_SCSI_MINIPORT, "
			"NDASSCSI_IOCTL_GET_SLOT_NO) failed, hr=0x%X\n",
			hr);
		return hr;
	}

	if (1 != pSrbIoControl->ReturnCode /* SRB_STATUS_SUCCESS*/) 
	{
		HRESULT hr = E_FAIL;
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"DeviceIoControl(IOCTL_SCSI_MINIPORT, "
			"NDASSCSI_IOCTL_GET_SLOT_NO) failed, "
			"returnCode=0x%X, hr=0x%X\n",
			pSrbIoControl->ReturnCode, hr);
		return hr;
	}

	//
	// Calculate the slot number offset and retrieve the value
	//

	*NdasSlotNumber = *reinterpret_cast<DWORD*>(lpInBuffer + cbHeader);

	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION,
		"NDAS Logical Device Slot: %d\n", *NdasSlotNumber);

	return S_OK;
}

HRESULT
pGetNdasSlotNumberFromDeviceNameW(
	LPCWSTR DeviceName,
	LPDWORD NdasSlotNumber)
{
	//
	// Open SCSI Port Device
	//

	/* SCSI miniport IOCTL required GENERIC_WRITE for now, really? */
	XTL::AutoFileHandle deviceHandle = CreateFileW(
		DeviceName,
		DeviceFileAccess | GENERIC_WRITE, 
		DeviceFileShareMode,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (deviceHandle.IsInvalid()) 
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, hr=0x%X\n",
			DeviceName, hr);
		return hr;
	}

	return pGetNdasSlotNumberFromDeviceHandle(deviceHandle, NdasSlotNumber);
}

HRESULT
pGetNdasSlotNumberForScsiPortNumber(
	__in DWORD ScsiPortNumber,
	__out LPDWORD NdasSlotNumber)
{
	//
	// Make up SCSI Port Name
	//
	
	WCHAR scsiPortName[MAX_PATH];

	XTLVERIFY(SUCCEEDED(
		StringCchPrintfW(
			scsiPortName, MAX_PATH,
			L"\\\\.\\Scsi%d:", ScsiPortNumber)));

	XTLTRACE2(NdasVolTrace, 2, "SCSI Port Name: %ls\n", scsiPortName);

	return pGetNdasSlotNumberFromDeviceNameW(scsiPortName, NdasSlotNumber);
}

HRESULT
pGetNdasSlotNumberFromStorageDeviceHandle(
	__in HANDLE DiskHandle,
	__out LPDWORD NdasSlotNumber)
{
	SCSI_ADDRESS scsiAddress = {0};
	HRESULT hr;
	
	//
	// try the disk device
	//

	hr = pGetNdasSlotNumberFromDeviceHandle(DiskHandle, NdasSlotNumber);
	if(SUCCEEDED(hr))
	{
		return hr;
	}

	XTLTRACE2(NdasVolTrace, 1, "pGetNdasSlotNumberFromDeviceHandle failed,"
		" DiskHandle=%p, hr=0x%X. Try the SCSI adapter device directly.\n", 
		DiskHandle, hr);

	//
	// Try the SCSI adapter device directly.
	//
	
	hr = pGetScsiAddressForDisk(DiskHandle, &scsiAddress);
	if (FAILED(hr))
	{
		XTLTRACE2(NdasVolTrace, 1, "GetScsiAddress failed, DiskHandle=%p, hr=0x%X\n", 
			DiskHandle, hr);
		return hr;
	}

	hr = pGetNdasSlotNumberForScsiPortNumber(
		scsiAddress.PortNumber, NdasSlotNumber);

	if (FAILED(hr))
	{
		XTLTRACE2(NdasVolTrace, 1, "GetNdasSlotNumberForScsiPortNumber failed, hr=0x%X\n",
			hr);
		return hr;
	}

	return S_OK;
}

HRESULT
pGetNdasSlotNumberFromDiskNumber(
	__in DWORD DiskNumber,
	__out LPDWORD NdasSlotNumber)
{
	WCHAR diskName[MAX_PATH];

	XTLVERIFY(SUCCEEDED(
		StringCchPrintfW(
			diskName, MAX_PATH,
			L"\\\\.\\PhysicalDrive%d", DiskNumber)));

	XTLTRACE2(NdasVolTrace, 2, "Disk Name: %ls\n", diskName);

	return pGetNdasSlotNumberFromDeviceNameW(diskName, NdasSlotNumber);
}

//////////////////////////////////////////////////////////////////////////
//
// pGetScsiAddressForDiskXXX
//
//////////////////////////////////////////////////////////////////////////

HRESULT
pGetScsiAddressForDisk(
	__in HANDLE hDisk,
	__out PSCSI_ADDRESS ScsiAddress)
{
	HRESULT hr;
	//
	// Query Storage Property
	//
	XTLTRACE2(NdasVolTrace, 3, "DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY)\n");

	if (NULL == ScsiAddress) 
	{
		return E_POINTER;
	}

	XTL::AutoProcessHeapPtr<STORAGE_ADAPTER_DESCRIPTOR> adapterDescriptor =
		pStorageQueryAdapterProperty(hDisk);
	if (adapterDescriptor.IsInvalid())
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"pStorageQueryAdapterProperty failed, hr=0x%X\n",
			hr);
		return hr;
	}

	//
	// Ignore non-SCSI device
	//

	if (BusTypeScsi != adapterDescriptor->BusType) 
	{
		hr = NDASVOL_ERROR_NON_NDAS_VOLUME;
		XTLTRACE2(NdasVolTrace, 2, "Ignoring non-scsi bus, hr=0x%X\n", hr);
		return hr;
	}

	//
	// Query SCSI Address, given that the physical drive is a SCSI device
	//

	XTLTRACE2(NdasVolTrace, 3, "DeviceIoControl(IOCTL_SCSI_GET_ADDRESS)\n");

	SCSI_ADDRESS scsiAddress = {0};
	hr = pScsiGetAddress(hDisk, &scsiAddress);
	if (FAILED(hr)) 
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"pScsiGetAddress failed, hr=0x%X\n",
			hr);
		hr = NDASVOL_ERROR_NON_NDAS_VOLUME;
		return hr;
	}

	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION,
		"SCSIAddress: Len: %d, PortNumber: %d, "
		"PathId: %d, TargetId: %d, Lun: %d\n",
		(DWORD) scsiAddress.Length,
		(DWORD) scsiAddress.PortNumber,
		(DWORD) scsiAddress.PathId,
		(DWORD) scsiAddress.TargetId,
		(DWORD) scsiAddress.Lun);

	//
	// Return the result
	//
	*ScsiAddress = scsiAddress;
	return S_OK;
}

HRESULT
pGetScsiAddressForDiskDeviceName(
	__in LPCTSTR DiskName,
	__out PSCSI_ADDRESS ScsiAddress)
{
	XTL::AutoFileHandle hDisk = pOpenDiskDevice(DiskName);
	if (hDisk.IsInvalid())
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return pGetScsiAddressForDisk(hDisk, ScsiAddress);
}

HRESULT
pGetScsiAddressForDiskNumber(
	__in DWORD DiskNumber,
	__out PSCSI_ADDRESS ScsiAddress)
{
	XTL::AutoFileHandle hDisk = pOpenDiskDeviceByNumber(DiskNumber);
	if (hDisk.IsInvalid())
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return pGetScsiAddressForDisk(hDisk, ScsiAddress);
}

HRESULT
pIsVolumeSpanningNdasDevice(
	__in HANDLE hVolume)
{
	HRESULT hr;

	XTL::AutoProcessHeapPtr<VOLUME_DISK_EXTENTS> extents = 
		pVolumeGetVolumeDiskExtents(hVolume);

	if (extents.IsInvalid())
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, hr=0x%X\n",
			hr);
		return hr;
	}

	for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i)
	{
		const DISK_EXTENT* diskExtent = &extents->Extents[i];

		XTLTRACE2(NdasVolTrace, 2, 
			"Disk Number=%d\n", diskExtent->DiskNumber);

		DWORD ndasSlotNumber;

		hr = pGetNdasSlotNumberFromDiskNumber(
			diskExtent->DiskNumber,
			&ndasSlotNumber);

		if (FAILED(hr))
		{
			SCSI_ADDRESS scsiAddress = {0};

			//
			// Since Windows Server 2003, SCSIPORT PDO does not send
			// IOCTL_SCSI_MINIPORT down to the stack. Hence we should send
			// the IOCTL directly to SCSI controller.
			//

			XTLTRACE2(NdasVolTrace, 2,
				"PhysicalDrive%d does not seem to an NDAS SCSI. Try with the adapter\n", 
				diskExtent->DiskNumber);

			hr = pGetScsiAddressForDiskNumber(
				diskExtent->DiskNumber, 
				&scsiAddress);

			if (FAILED(hr))
			{
				XTLTRACE2(NdasVolTrace, 1, 
					"Disk %d does not seem to be attached to the SCSI controller.\n", 
					diskExtent->DiskNumber);
				continue;
			}

			XTLTRACE2(NdasVolTrace, 2, 
				"SCSI Address (PortNumber=%d,PathId=%d,TargetId=%d,LUN=%d)\n", 
				scsiAddress.PortNumber, scsiAddress.PathId, 
				scsiAddress.TargetId, scsiAddress.Lun);

			DWORD ndasSlotNumber;

			hr = pGetNdasSlotNumberForScsiPortNumber(
				scsiAddress.PortNumber, 
				&ndasSlotNumber);

			if (FAILED(hr))
			{
				XTLTRACE2(NdasVolTrace, 2,
					"ScsiPort %d does not seem to an NDAS SCSI.\n", 
					scsiAddress.PortNumber);
				continue;
			}
		}

		XTLTRACE2(NdasVolTrace, 2, 
			"Detected Disk %d/%d has a NDAS Slot Number=%d (first found only).\n", 
			i + 1, extents->NumberOfDiskExtents,
			ndasSlotNumber);

		return S_OK;
	}

	return E_FAIL;
}

HRESULT
pGetVolumeDeviceNameForMountPointW(
	__in LPCWSTR VolumeMountPoint,
	__out LPWSTR* VolumeDeviceName)
{
	// The lpszVolumeMountPoint parameter may be a drive letter with 
	// appended backslash (\), such as "D:\". Alternatively, it may be 
	// a path to a volume mount point, again with appended backslash (\), 
	// such as "c:\mnt\edrive\".

	// A reasonable size for the buffer to accommodate the largest possible 
	// volume name is 50 characters --> wrong 100

	HRESULT hr;

	XTLASSERT(NULL != VolumeDeviceName);
	if (NULL == VolumeDeviceName)
	{
		return E_POINTER;
	}

	*VolumeDeviceName = NULL;

	const DWORD MAX_VOLUMENAME_LEN = 50;
	DWORD volumeDeviceNameLength = MAX_VOLUMENAME_LEN;
	
	XTL::AutoProcessHeapPtr<TCHAR> volumeDeviceName = 
		static_cast<TCHAR*>(
			HeapAlloc(
				GetProcessHeap(), 
				HEAP_ZERO_MEMORY, 
				volumeDeviceNameLength * sizeof(TCHAR)));

	if (volumeDeviceName.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, 0,
			"HeapAlloc for %d bytes failed.\n", volumeDeviceNameLength);
		return E_OUTOFMEMORY;
	}

	BOOL success = GetVolumeNameForVolumeMountPoint(
		VolumeMountPoint, 
		volumeDeviceName, 
		volumeDeviceNameLength);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, 0,
			"GetVolumeNameForVolumeMountPoint(%ls) failed, hr=0x%X\n", 
			VolumeMountPoint, hr);
		return hr;
	}

	// Volume Name is a format of \\?\Volume{XXXX}\ with trailing backslash
	// Volume device name is that of \\.\Volume{XXXX} without trailing backslash

	_ASSERTE(_T('\\') == volumeDeviceName[0]);
	_ASSERTE(_T('\\') == volumeDeviceName[1]);
	_ASSERTE(_T('?')  == volumeDeviceName[2]);
	_ASSERTE(_T('\\') == volumeDeviceName[3]);

	if (_T('\\') == volumeDeviceName[0] &&
		_T('\\') == volumeDeviceName[1] &&
		_T('?') == volumeDeviceName[2] &&
		_T('\\') == volumeDeviceName[3])
	{
		// replace ? to .
		volumeDeviceName[2] = _T('.');
	}

	// remove trailing backslash
	pRemoveTrailingBackslash(volumeDeviceName);

	XTLTRACE2(NdasVolTrace, 2, 
		"VolumeMountPoint(%ls)=>Volume(%ls)\n", 
		VolumeMountPoint, volumeDeviceName);

	*VolumeDeviceName = volumeDeviceName.Detach();

	return S_OK;
}

HRESULT
pGetVolumeMountPointForPathW(
	__in LPCWSTR Path,
	__out LPWSTR* MountPoint)
{
	if (NULL == MountPoint)
	{
		return E_POINTER;
	}

	*MountPoint = NULL;

	// TODO: It is possible to be the path is more than MAX_PATH
	// in case of supporting unicode file names up to 65534
	DWORD mountPointLength = MAX_PATH;
	XTL::AutoProcessHeapPtr<TCHAR> mountPoint = 
		static_cast<TCHAR*>(
			::HeapAlloc(
				::GetProcessHeap(), 
				HEAP_ZERO_MEMORY,
				mountPointLength * sizeof(WCHAR)));

	if (mountPoint.IsInvalid())
	{
		return E_OUTOFMEMORY;
	}

	if (!GetVolumePathName(Path, mountPoint, mountPointLength))
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"GetVolumePathName(%ls) failed, hr=0x%X\n",
			Path, hr);
		return hr;
	}

	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION,
		"Path(%ls) is mounted from %s.\n", Path, mountPoint);

	*MountPoint = mountPoint.Detach();

	return S_OK;
}

HRESULT
__stdcall
pGetStorageDeviceNumber(
	HANDLE hDevice, 
	PULONG	DeviceNumber)
{
	STORAGE_DEVICE_NUMBER storageDevNum;

	HRESULT	hr = pStorageGetDeviceNumber(hDevice, &storageDevNum);

	if (FAILED(hr))
	{
		return hr;
	}

	if (DeviceNumber)
	{
		*DeviceNumber = storageDevNum.DeviceNumber;
	}

	return S_OK;
}

HANDLE
__stdcall
pOpenStorageDeviceByNumber(
	DWORD StorageDeviceNumber,
	ACCESS_MASK DeviceFileAccessMask
){
	// Device Name Format: \\.\PhysicalDriveXX
	TCHAR deviceName[24];
	XTLVERIFY(SUCCEEDED(
		::StringCchPrintf(
			deviceName, RTL_NUMBER_OF(deviceName), 
			_T("\\\\.\\PhysicalDrive%d"), StorageDeviceNumber)));

	XTL::AutoFileHandle hDisk = ::CreateFile(
		deviceName, 
		DeviceFileAccessMask, 
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, 
		OPEN_EXISTING, 
		0, 
		NULL);
	if (hDisk.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"CreateFile(%ls) failed.\n", deviceName);
		return INVALID_HANDLE_VALUE;
	}
	return hDisk.Detach();
}
