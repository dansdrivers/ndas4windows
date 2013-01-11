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

BOOL
pGetNdasSlotNumberForScsiPort(
	HANDLE hScsiPort, 
	LPDWORD NdasSlotNumber)
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

	BOOL fSuccess = ::DeviceIoControl(
		hScsiPort,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	if (!fSuccess) 
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"DeviceIoControl(IOCTL_SCSI_MINIPORT, "
			"NDASSCSI_IOCTL_GET_SLOT_NO) failed, error=0x%X",
			GetLastError());
		return FALSE;
	}

	if (0 != pSrbIoControl->ReturnCode) 
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"DeviceIoControl(IOCTL_SCSI_MINIPORT, "
			"NDASSCSI_IOCTL_GET_SLOT_NO) failed, "
			"returnCode=0x%X, error=0x%X\n",
			pSrbIoControl->ReturnCode, GetLastError());
		return FALSE;
	}

	//
	// Calculate the slot number offset and retrieve the value
	//

	*NdasSlotNumber = *reinterpret_cast<DWORD*>(lpInBuffer + cbHeader);

	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION,
		"NDAS Logical Device Slot: %d\n", *NdasSlotNumber);

	return TRUE;
}

BOOL
pGetNdasSlotNumberForScsiPortDeviceName(
	LPCTSTR ScsiPortDeviceName,
	LPDWORD NdasSlotNumber)
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
		return FALSE;
	}
	return pGetNdasSlotNumberForScsiPort(hScsiPort, NdasSlotNumber);

}

BOOL
pGetNdasSlotNumberForScsiPortNumber(
	DWORD ScsiPortNumber,
	LPDWORD NdasSlotNumber)
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

	return pGetNdasSlotNumberForScsiPortDeviceName(scsiPortName, NdasSlotNumber);
}

//////////////////////////////////////////////////////////////////////////
//
// pGetScsiAddressForDiskXXX
//
//////////////////////////////////////////////////////////////////////////

BOOL
pGetScsiAddressForDisk(
	IN HANDLE hDisk,
	OUT PSCSI_ADDRESS ScsiAddress)
{
	//
	// Query Storage Property
	//
	XTLTRACE2(NdasVolTrace, 3, "DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY)\n");

	XTL::AutoProcessHeapPtr<STORAGE_ADAPTER_DESCRIPTOR> adapterDescriptor =
		pStorageQueryAdapterProperty(hDisk);
	if (adapterDescriptor.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"pStorageQueryAdapterProperty failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	//
	// Ignore non-SCSI device
	//

	if (BusTypeScsi != adapterDescriptor->BusType) 
	{
		XTLTRACE2(NdasVolTrace, 2, "Ignoring non-scsi bus\n");
		::SetLastError(NDASVOL_ERROR_NON_NDAS_VOLUME);
		return FALSE;
	}

	//
	// Query SCSI Address, given that the physical drive is a SCSI device
	//

	XTLTRACE2(NdasVolTrace, 3, "DeviceIoControl(IOCTL_SCSI_GET_ADDRESS)\n");

	SCSI_ADDRESS scsiAddress = {0};
	BOOL fSuccess = pScsiGetAddress(hDisk, &scsiAddress);
	if (!fSuccess) 
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"pScsiGetAddress failed, error=0x%X\n",
			GetLastError());
		::SetLastError(NDASVOL_ERROR_NON_NDAS_VOLUME);
		return FALSE;
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
	return TRUE;
}

BOOL
pGetScsiAddressForDiskDeviceName(
	IN LPCTSTR DiskName,
	OUT PSCSI_ADDRESS ScsiAddress)
{
	XTL::AutoFileHandle hDisk = pOpenDiskDevice(DiskName);
	if (hDisk.IsInvalid())
	{
		return FALSE;
	}
	return pGetScsiAddressForDisk(hDisk, ScsiAddress);
}

BOOL
pGetScsiAddressForDiskNumber(
	IN DWORD DiskNumber,
	OUT PSCSI_ADDRESS ScsiAddress)
{
	XTL::AutoFileHandle hDisk = pOpenDiskDeviceByNumber(DiskNumber);
	if (hDisk.IsInvalid())
	{
		return FALSE;
	}
	return pGetScsiAddressForDisk(hDisk, ScsiAddress);
}

//////////////////////////////////////////////////////////////////////////
//
// pGetScsiAddressForDiskXXX
//
//////////////////////////////////////////////////////////////////////////

BOOL
pGetNdasScsiLocationForDisk(
	HANDLE hDisk,
	PNDAS_SCSI_LOCATION pNdasScsiLocation)
{
	SCSI_ADDRESS scsiAddress = {0};
	if (!pGetScsiAddressForDisk(hDisk, &scsiAddress))
	{
		XTLTRACE2(NdasVolTrace, 1, 
			"Disk (Handle=%p) does not seem to be attached to the SCSI controller.\n", 
			hDisk);
		return FALSE;
	}

	XTLTRACE2(NdasVolTrace, 2, 
		"SCSI Address (PortNumber=%d,PathId=%d,TargetId=%d,LUN=%d)\n", 
		scsiAddress.PortNumber, scsiAddress.PathId, 
		scsiAddress.TargetId, scsiAddress.Lun);

	DWORD ndasSlotNumber;
	if (!pGetNdasSlotNumberForScsiPortNumber(scsiAddress.PortNumber, &ndasSlotNumber))
	{
		XTLTRACE2(NdasVolTrace, 2,
			"ScsiPort %d does not seem to an NDAS SCSI.\n", 
			scsiAddress.PortNumber);
		return FALSE;
	}

	XTLTRACE2(NdasVolTrace, 2, 
		"Disk is a NDAS disk device at NDASSCSI(%d,%d,%d).\n",
		ndasSlotNumber, scsiAddress.TargetId, scsiAddress.Lun);

	pNdasScsiLocation->SlotNo = ndasSlotNumber;
	pNdasScsiLocation->TargetID = scsiAddress.TargetId;
	pNdasScsiLocation->LUN = scsiAddress.Lun;

	return TRUE;
}

BOOL
pIsVolumeSpanningNdasDevice(
	HANDLE hVolume)
{
	XTL::AutoProcessHeapPtr<VOLUME_DISK_EXTENTS> extents = 
		pVolumeGetVolumeDiskExtents(hVolume);

	if (extents.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i)
	{
		const DISK_EXTENT* diskExtent = &extents->Extents[i];
		SCSI_ADDRESS scsiAddress = {0};
		if (!pGetScsiAddressForDiskNumber(diskExtent->DiskNumber, &scsiAddress))
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
		if (!pGetNdasSlotNumberForScsiPortNumber(scsiAddress.PortNumber, &ndasSlotNumber))
		{
			XTLTRACE2(NdasVolTrace, 2,
				"ScsiPort %d does not seem to an NDAS SCSI.\n", 
				scsiAddress.PortNumber);
			continue;
		}

		XTLTRACE2(NdasVolTrace, 2, 
			"Detected Disk %d/%d has a NDAS Slot Number=%d (first found only).\n", 
			i + 1, extents->NumberOfDiskExtents,
			ndasSlotNumber);

		return TRUE;
	}

	return FALSE;
}


LPTSTR
pGetVolumeDeviceNameForMountPoint(
	LPCTSTR VolumeMountPoint)
{
	// The lpszVolumeMountPoint parameter may be a drive letter with 
	// appended backslash (\), such as "D:\". Alternatively, it may be 
	// a path to a volume mount point, again with appended backslash (\), 
	// such as "c:\mnt\edrive\".

	// A reasonable size for the buffer to accommodate the largest possible 
	// volume name is 50 characters --> wrong 100

	const DWORD MAX_VOLUMENAME_LEN = 50;
	DWORD volumeDeviceNameLength = MAX_VOLUMENAME_LEN;
	
	XTL::AutoProcessHeapPtr<TCHAR> volumeDeviceName = 
		static_cast<TCHAR*>(
			::HeapAlloc(
				::GetProcessHeap(), 
				HEAP_ZERO_MEMORY, 
				volumeDeviceNameLength * sizeof(TCHAR)));

	if (volumeDeviceName.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, 0,
			_T("HeapAlloc for %d bytes failed.\n"), volumeDeviceNameLength);
		return NULL;
	}

	BOOL fSuccess = ::GetVolumeNameForVolumeMountPoint(
		VolumeMountPoint, 
		volumeDeviceName, 
		volumeDeviceNameLength);
	if (!fSuccess)
	{
		XTLTRACE2(NdasVolTrace, 0,
			_T("GetVolumeNameForVolumeMountPoint(%s) failed.\n"), VolumeMountPoint);
		return NULL;
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
		_T("VolumeMountPoint(%s)=>Volume(%s)\n"), VolumeMountPoint, volumeDeviceName);

	return volumeDeviceName.Detach();
}

LPTSTR
pGetVolumeMountPointForPath(
	LPCTSTR Path)
{
	// TODO: It is possible to be the path is more than MAX_PATH
	// in case of supporting unicode file names up to 65534
	DWORD mountPointLength = MAX_PATH;
	XTL::AutoProcessHeapPtr<TCHAR> mountPoint = 
		static_cast<TCHAR*>(
			::HeapAlloc(
				::GetProcessHeap(), 
				HEAP_ZERO_MEMORY,
				mountPointLength * sizeof(TCHAR)));
	if (mountPoint.IsInvalid())
	{
		return NULL;
	}

	if (!::GetVolumePathName(Path, mountPoint, mountPointLength))
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			_T("GetVolumePathName(%s) failed, error=0x%X\n"),
			Path, GetLastError());
		return NULL;
	}

	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION,
		_T("Path(%s) is mounted from %s.\n"), Path, mountPoint);

	return mountPoint.Detach();
}
