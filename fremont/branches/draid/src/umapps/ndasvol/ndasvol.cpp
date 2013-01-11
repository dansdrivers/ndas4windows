#include "precomp.hpp"
#include <ndas/ndasvol.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndasvolex.h>
#include <xtl/xtltrace.h>
#include <xtl/xtlautores.h>
#include "ptrsec.hpp"
#include "winioctlhelper.h"

#define CPARAM(pred) \
	if (!(pred)) { ::SetLastError(ERROR_INVALID_PARAMETER); return FALSE; } \
	do {} while(0) 

namespace
{
	BOOL CALLBACK 
	SimpleNdasScsiLocationEnum(
		const NDAS_SCSI_LOCATION* NdasScsiLocation,
		LPVOID Context)
	{
		PNDAS_SCSI_LOCATION ndasScsiLocationFound = 
			static_cast<PNDAS_SCSI_LOCATION>(Context);
		*ndasScsiLocationFound = *NdasScsiLocation;
		return FALSE; // first only
	}
}

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasGetNdasScsiLocationForVolume(
	IN HANDLE hVolume, 
	OUT PNDAS_SCSI_LOCATION NdasScsiLocation)
{
	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION, 
		"NdasGetNdasScsiSlotNumber(%p)\n", hVolume);

	NDAS_SCSI_LOCATION loc = {0};
	BOOL fSuccess = NdasEnumNdasScsiLocationsForVolume(
		hVolume, 
		SimpleNdasScsiLocationEnum, 
		&loc);
	if (!fSuccess)
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"NdasGetNdasScsiLocationsForVolumeEx failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	if (0 == loc.SlotNo) // means not found!
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"Slot No is zero.\n");
		return FALSE;
	}

	*NdasScsiLocation = loc;
	return TRUE;
}

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasEnumNdasScsiLocationsForVolume(
	IN HANDLE hVolume,
	NDASSCSILOCATIONENUMPROC EnumProc,
	LPVOID Context)
{
	CPARAM(!::IsBadCodePtr((FARPROC)EnumProc));

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
			"Detected Disk %d/%d has NDAS Slot Number=%d (first found only).\n", 
			i, extents->NumberOfDiskExtents,
			ndasSlotNumber);

		NDAS_SCSI_LOCATION ndasScsiLocation = {0};
		ndasScsiLocation.SlotNo = ndasSlotNumber;
		ndasScsiLocation.TargetID = scsiAddress.TargetId;
		ndasScsiLocation.LUN = scsiAddress.Lun;

		BOOL fContinue = EnumProc(&ndasScsiLocation, Context);
		if (!fContinue)
		{
			return TRUE;
		}
	}

	return TRUE;
}
	

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasIsNdasVolume(
	IN HANDLE hVolume)
{
	XTLTRACE2(NdasVolTrace, 4, "NdasIdNdasVolume(%p)\n", hVolume);
	return pIsVolumeSpanningNdasDevice(hVolume);
}

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasIsNdasPathW(
	IN LPCWSTR FilePath)
{
	CPARAM(IsValidStringPtrW(FilePath, UINT_PTR(-1)));
	XTLTRACE2(NdasVolTrace, 4, "NdasIsNdasPathW(%ls)\n", FilePath);

	XTL::AutoProcessHeapPtr<TCHAR> mountPoint = 
		pGetVolumeMountPointForPath(FilePath);
	if (mountPoint.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			_T("pGetVolumeMountPointForPath(%s) failed, error=0x%X\n"), 
			FilePath, GetLastError());
		return FALSE;
	}

	XTL::AutoProcessHeapPtr<TCHAR> volumeName = 
		pGetVolumeDeviceNameForMountPoint(mountPoint);
	if (volumeName.IsInvalid())
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			_T("pGetVolumeDeviceNameForMountPoint(%s) failed, error=0x%X\n"), 
			mountPoint, GetLastError());
		return FALSE;
	}

	// Volume is a \\.\C:
	XTL::AutoFileHandle hVolume = ::CreateFileW(
		volumeName,
		GENERIC_READ, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL, 
		OPEN_EXISTING, 
		0,
		NULL);

	if (hVolume.IsInvalid())
	{
		return FALSE;
	}

	return NdasIsNdasVolume(hVolume);
}

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasIsNdasPathA(
	IN LPCSTR FilePath)
{
	CPARAM(IsValidStringPtrA(FilePath, UINT_PTR(-1)));

	XTLTRACE2(NdasVolTrace, 4, "NdasIsNdasPathA(%hs)\n", FilePath);

	int nChars = ::MultiByteToWideChar(CP_ACP, 0, FilePath, -1, NULL, 0);
	++nChars; // additional terminating NULL char
	XTL::AutoProcessHeapPtr<WCHAR> wszFilePath = reinterpret_cast<LPWSTR>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, nChars * sizeof(WCHAR)));
	if (wszFilePath.IsInvalid())
	{
		::SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}
	nChars = ::MultiByteToWideChar(CP_ACP, 0, FilePath, -1, wszFilePath, nChars);
	return NdasIsNdasPathW(wszFilePath);
}

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasRequestDeviceEjectW(
	const NDAS_SCSI_LOCATION* NdasScsiLocation,
	PNDAS_REQUEST_DEVICE_EJECT_DATAW EjectData)
{
	// Slot Number should not be zero
	CPARAM(0 != NdasScsiLocation->SlotNo);

	// For now TargetID and LUN should be 0
	// This will be changed when NDAS SCSI devices are hosted by a single
	// NDAS SCSI controller.
	CPARAM(0 == NdasScsiLocation->LUN && 0 == NdasScsiLocation->TargetID);

	CPARAM(sizeof(NDAS_REQUEST_DEVICE_EJECT_DATAW) == EjectData->Size);

	PNP_VETO_TYPE VetoType = static_cast<PNP_VETO_TYPE>(EjectData->VetoType);

	BOOL fSuccess = pRequestNdasScsiDeviceEject(
		NdasScsiLocation->SlotNo,
		&EjectData->ConfigRet,
		&VetoType,
		EjectData->VetoName,
		MAX_PATH);

	if (!fSuccess)
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"pRequestNdasScsiDeviceEject failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	EjectData->VetoType = static_cast<DWORD>(VetoType);

	return TRUE;
}

extern "C" void NDASVOL_CALL
NdasVolSetTrace(DWORD Flags, DWORD Category, DWORD Level)
{
	if (Flags & 0x00000001)
	{
		XTL::XtlTraceEnableDebuggerTrace(!(Flags & 0x00010000));
	}
	if (Flags & 0x00000002)
	{
		XTL::XtlTraceEnableConsoleTrace(!(Flags & 0x00020000));
	}
	XTL::XtlTraceSetTraceCategory(Category);
	XTL::XtlTraceSetTraceLevel(Level);
}
