#include "precomp.hpp"
#include <ndas/ndasvol.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndasvolex.h>
#include <xtl/xtltrace.h>
#include <xtl/xtlautores.h>
#include "ptrsec.hpp"
#include "winioctlhelper.h"

namespace
{
	HRESULT CALLBACK 
	SimpleNdasLocationEnum(
		NDAS_LOCATION NdasLocation,
		LPVOID Context)
	{
		PNDAS_LOCATION ndasLocationFound = 
			static_cast<PNDAS_LOCATION>(Context);
		*ndasLocationFound = NdasLocation;
		return S_FALSE; // first only
	}
}

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasGetNdasLocationForVolume(
	IN HANDLE hVolume, 
	OUT PNDAS_LOCATION NdasLocation)
{
	XTLTRACE2(NdasVolTrace, TRACE_LEVEL_INFORMATION, 
		"NdasGetNdasScsiSlotNumber(%p)\n", hVolume);

	if (NULL == NdasLocation) 
	{
		return E_POINTER;
	}

	*NdasLocation = 0;

	NDAS_LOCATION location = 0;
	
	HRESULT hr = NdasEnumNdasLocationsForVolume(
		hVolume, 
		SimpleNdasLocationEnum, 
		&location);
	
	if (FAILED(hr))
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"NdasEnumNdasLocationsForVolume failed, hr=0x%X\n", hr);
		return hr;
	} 
	else if (0 == location) // means not found!
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"Slot No is zero.\n");
		return S_FALSE;
	}

	*NdasLocation = location;

	return S_OK;
}

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasEnumNdasLocationsForVolume(
	IN HANDLE hVolume,
	NDASLOCATIONENUMPROC EnumProc,
	LPVOID Context)
{
	HRESULT hr;

	if (IsBadCodePtr((FARPROC)EnumProc))
	{
		return E_POINTER;
	}

	XTL::AutoProcessHeapPtr<VOLUME_DISK_EXTENTS> extents = 
		pVolumeGetVolumeDiskExtents(hVolume);

	if (extents.IsInvalid())
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, hr=0x%x\n",
			hr);
		return hr;
	}

	for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i)
	{
		const DISK_EXTENT* diskExtent = &extents->Extents[i];
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
			"Detected Disk %d/%d has NDAS Slot Number=%d (first found only).\n", 
			i, extents->NumberOfDiskExtents,
			ndasSlotNumber);

		NDAS_LOCATION ndasScsiLocation = ndasSlotNumber;

		//
		// EnumProc 
		// S_OK: continue enumeration
		// S_FALSE: returns S_OK to the caller (stop enumeration)
		// otherwise E_XXX: returns E_XXX to the caller
		// 

		hr = EnumProc(ndasScsiLocation, Context);

		if (FAILED(hr))
		{
			return hr;
		}
		else if (S_FALSE == hr)
		{
			return S_OK;
		}
	}

	return S_OK;
}
	

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasVolume(
	__in HANDLE hVolume)
{
	XTLTRACE2(NdasVolTrace, 4, "NdasIdNdasVolume(%p)\n", hVolume);
	return pIsVolumeSpanningNdasDevice(hVolume);
}

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasPathW(
	__in LPCWSTR FilePath)
{
	HRESULT hr;

	if (IsBadStringPtrW(FilePath, UINT_PTR(-1)))
	{
		return E_INVALIDARG;
	}

	XTLTRACE2(NdasVolTrace, 4, "NdasIsNdasPathW(%ls)\n", FilePath);

	LPWSTR mountPoint;
	hr = pGetVolumeMountPointForPathW(FilePath, &mountPoint);

	if (FAILED(hr))
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"pGetVolumeMountPointForPath(%ls) failed, hr=0x%X\n", 
			FilePath, hr);
		return hr;
	}

	XTL::AutoProcessHeapPtr<WCHAR> mountPointPtr = mountPoint;

	LPWSTR deviceName;
	hr = pGetVolumeDeviceNameForMountPointW(mountPoint, &deviceName);

	if (FAILED(hr))
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR, 
			"pGetVolumeDeviceNameForMountPoint(%ls) failed, hr=0x%X\n", 
			mountPoint, hr);
		return hr;
	}

	XTL::AutoProcessHeapPtr<TCHAR> deviceNamePtr = deviceName;

	// Volume is a \\.\C:
	XTL::AutoFileHandle volumeHandle = CreateFileW(
		deviceName,
		GENERIC_READ, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL, 
		OPEN_EXISTING, 
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == static_cast<HANDLE>(volumeHandle))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"CreateFile failed, path=%ls, hr=0x%X\n",
			deviceName, hr);
		return hr;
	}

	return NdasIsNdasVolume(volumeHandle);
}

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasPathA(
	IN LPCSTR FilePath)
{
	if (IsBadStringPtrA(FilePath, UINT_PTR(-1)))
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"Invalid path, path=%hs\n", FilePath); 
		return E_INVALIDARG;
	}

	XTLTRACE2(NdasVolTrace, 4, "NdasIsNdasPathA(%hs)\n", FilePath);

	int nChars = MultiByteToWideChar(CP_ACP, 0, FilePath, -1, NULL, 0);
	++nChars; // additional terminating NULL char
	XTL::AutoProcessHeapPtr<WCHAR> wszFilePath = reinterpret_cast<LPWSTR>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, nChars * sizeof(WCHAR)));
	if (wszFilePath.IsInvalid())
	{
		return E_OUTOFMEMORY;
	}

	nChars = MultiByteToWideChar(CP_ACP, 0, FilePath, -1, wszFilePath, nChars);

	XTLASSERT(nChars > 0);

	return NdasIsNdasPathW(wszFilePath);
}

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasRequestDeviceEjectW(
	__in NDAS_LOCATION NdasLocation,
	__inout PNDAS_REQUEST_DEVICE_EJECT_DATAW EjectData)
{
	// Slot Number should not be zero
	if (0 == NdasLocation)
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"Invalid parameter for NdasRequestDeviceJect, NdasLocation=%d is used\n",
			NdasLocation);
		return E_INVALIDARG;
	}

	// Size field of NDAS_REQUEST_DEVICE_EJECT_DATA should be the same size
	if (sizeof(NDAS_REQUEST_DEVICE_EJECT_DATAW) != EjectData->Size)
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"Invalid parameter for NDAS_REQUEST_DEVICE_EJECT_DATA.Size=%d, Param.Size=%d\n",
			sizeof(NDAS_REQUEST_DEVICE_EJECT_DATAW), EjectData->Size);
		return E_INVALIDARG;
	}

	PNP_VETO_TYPE VetoType = static_cast<PNP_VETO_TYPE>(EjectData->VetoType);

	HRESULT hr = pRequestNdasScsiDeviceEjectW(
		NdasLocation,
		&EjectData->ConfigRet,
		&VetoType,
		EjectData->VetoName,
		MAX_PATH);

	if (FAILED(hr))
	{
		XTLTRACE2(NdasVolTrace, TRACE_LEVEL_ERROR,
			"pRequestNdasScsiDeviceEject failed, hr=0x%X\n", hr);
		return hr;
	}

	EjectData->VetoType = static_cast<DWORD>(VetoType);

	return S_OK;
}

NDASVOL_LINKAGE
HRESULT
NDASVOL_CALL
NdasIsNdasStorage(
	__in HANDLE DiskHandle)
{
	DWORD ndasSlotNumber = 0;

	HRESULT hr = pGetNdasSlotNumberFromDeviceHandle(
		DiskHandle, &ndasSlotNumber);

	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
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

/* for backward compatibility */
#pragma warning(disable: 4995)

namespace
{
	typedef struct _NDAS_LOCATION_CONTEXT_SHIM {
		NDASSCSILOCATIONENUMPROC EnumProc;
		LPVOID Context;
	} NDAS_LOCATION_CONTEXT_SHIM, *PNDAS_LOCATION_CONTEXT_SHIM;

	HRESULT CALLBACK
	NdasLocationCallbackShim(
		NDAS_LOCATION NdasLocation, 
		LPVOID Context)
	{
		PNDAS_LOCATION_CONTEXT_SHIM contextShim = 
			reinterpret_cast<PNDAS_LOCATION_CONTEXT_SHIM>(Context);
		NDAS_SCSI_LOCATION ndasScsiLocation = {0};
		ndasScsiLocation.SlotNo = NdasLocation;
		BOOL cont = contextShim->EnumProc(&ndasScsiLocation, contextShim->Context);
		return cont ? S_OK : S_FALSE;
	}

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
NdasEnumNdasScsiLocationsForVolume(
	IN HANDLE hVolume,
	NDASSCSILOCATIONENUMPROC EnumProc,
	LPVOID Context)
{
	if (IsBadCodePtr((FARPROC)EnumProc))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	NDAS_LOCATION_CONTEXT_SHIM shimContext = { EnumProc, Context };

	return NdasEnumNdasLocationsForVolume(
		hVolume,
		NdasLocationCallbackShim,
		&shimContext);
}

NDASVOL_LINKAGE
BOOL
NDASVOL_CALL
NdasGetNdasScsiLocationForVolume(
	IN HANDLE hVolume, 
	OUT PNDAS_SCSI_LOCATION NdasScsiLocation)
{
	NDAS_SCSI_LOCATION loc = {0};
	
	BOOL fSuccess = NdasEnumNdasScsiLocationsForVolume(
		hVolume, 
		SimpleNdasScsiLocationEnum, 
		&loc);

	if (!fSuccess)
	{
		return FALSE;
	}

	if (0 == loc.SlotNo) // means not found!
	{
		return FALSE;
	}

	*NdasScsiLocation = loc;
	return TRUE;
}

#pragma warning(default: 4995)
