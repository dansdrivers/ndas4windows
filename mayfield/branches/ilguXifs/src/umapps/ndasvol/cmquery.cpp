#include "precomp.hpp"
// extern "C" {
// #include <devioctl.h>
// #include <ntddscsi.h>
// #include <ntddstor.h>
// }

#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>
#include <ndas/ndastype.h>
#include <ndas/ndasvolex.h>
#include "winioctlhelper.h"

namespace XTL
{
	struct AutoDeviceInfoSetConfig {
		static HDEVINFO GetInvalidValue() {
			return (HDEVINFO) INVALID_HANDLE_VALUE; 
		}
		static void Release(HDEVINFO h) {
			XTL_SAVE_LAST_ERROR();
			XTLVERIFY(::SetupDiDestroyDeviceInfoList(h));
		}
	};

	typedef AutoResourceT<HDEVINFO,AutoDeviceInfoSetConfig> AutoDeviceInfoSet;
}

#define NDAS_FACILITY_CONFIGRET 0x1F0
#define NDAS_CUSTOMER_BIT (0x00A0 << 24)
__forceinline DWORD NDAS_CR_MAP(CONFIGRET cret)
{
	static const DWORD FacilityMask = NDAS_FACILITY_CONFIGRET << 16;
	static const DWORD CustomerMask = APPLICATION_ERROR_MASK;
	return (ERROR_SEVERITY_ERROR | CustomerMask | FacilityMask | cret);
}

DWORD
ConfigRetToWin32Error(CONFIGRET cret)
{
	static const struct { 
		CONFIGRET ConfigRet;
		DWORD Win32Error;
	} Mappings[] = {
		CR_SUCCESS, NO_ERROR,
		CR_OUT_OF_MEMORY, ERROR_NOT_ENOUGH_MEMORY,
		CR_INVALID_POINTER, ERROR_INVALID_USER_BUFFER,
		CR_INVALID_DEVNODE, ERROR_NO_SUCH_DEVINST,
		/* #define CR_INVALID_DEVINST CR_INVALID_DEVNODE */
		CR_NO_SUCH_DEVNODE, ERROR_DEVINST_ALREADY_EXISTS,
		/* #define CR_NO_SUCH_DEVINST CR_NO_SUCH_DEVNODE */
		CR_INVALID_DEVICE_ID, ERROR_INVALID_DEVINST_NAME,
		CR_ALREADY_SUCH_DEVNODE, ERROR_DEVINST_ALREADY_EXISTS,
		CR_INVALID_REFERENCE_STRING, ERROR_INVALID_REFERENCE_STRING,
		CR_INVALID_MACHINENAME, ERROR_INVALID_MACHINENAME,
		CR_REMOTE_COMM_FAILURE, ERROR_REMOTE_COMM_FAILURE,
		CR_NO_CM_SERVICES, ERROR_NO_CONFIGMGR_SERVICES,
		CR_ACCESS_DENIED, ERROR_ACCESS_DENIED,
		CR_CALL_NOT_IMPLEMENTED, ERROR_CALL_NOT_IMPLEMENTED,
		CR_NOT_DISABLEABLE, ERROR_NOT_DISABLEABLE
	};
	for (int i = 0; i < RTL_NUMBER_OF(Mappings); ++i)
	{
		if (Mappings[i].ConfigRet == cret)
		{
			return Mappings[i].Win32Error;
		}
	}
	return NDAS_CR_MAP(cret);
}

//
// Caller should free the returned SymbolicLinkNames
// with ::HeapFree(::GetProcessHeap(), ...) on successful return (CR_SUCCESS)
//

CONFIGRET
pCMGetDeviceInterfaceList(
	LPTSTR* SymbolicLinkNameList,
	LPCGUID InterfaceClassGuid,
	DEVINSTID DevInstId,
	ULONG Flags /*= CM_GET_DEVICE_INTERFACE_LIST_PRESENT*/)
{
	*SymbolicLinkNameList = NULL;

	ULONG bufferLength;
	CONFIGRET ret = ::CM_Get_Device_Interface_List_Size(
		&bufferLength, 
		const_cast<LPGUID>(InterfaceClassGuid), 
		DevInstId,
		Flags);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("CM_Get_Device_Interface_List_Size failed with cr=%x.\n", ret);
		return ret;
	}
	
	XTLTRACE("RequiredBufferSize=%d chars\n", bufferLength);
	if (0 == bufferLength)
	{
		TCHAR* empty = static_cast<TCHAR*>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 2 * sizeof(TCHAR)));
		*SymbolicLinkNameList = empty;
		return CR_SUCCESS;
	}

	XTL::AutoProcessHeapPtr<TCHAR> buffer = static_cast<TCHAR*>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, bufferLength * sizeof(TCHAR)));
	if (buffer.IsInvalid())
	{
		XTLTRACE("HeapAlloc failed for %d bytes.\n", bufferLength * sizeof(TCHAR));
		return CR_OUT_OF_MEMORY;
	}

	ret = ::CM_Get_Device_Interface_List(
		const_cast<LPGUID>(InterfaceClassGuid),
		DevInstId,
		buffer,
		bufferLength,
		Flags);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("CM_Get_Device_Interface_List failed with cr=%x.\n", ret);
		return ret;
	}

	*SymbolicLinkNameList = buffer.Detach();		
	XTLTRACE(_T("SymbolicLinkNameList=%s\n"), *SymbolicLinkNameList);

	return CR_SUCCESS;
}

//
// Caller should free the returned RelDeviceInstanceIdList
// with ::HeapFree(::GetProcessHeap(), ...) on successful return (CR_SUCCESS)
//

CONFIGRET
pCMGetDeviceIDList(
	LPTSTR* RelDevInstIdList,
	LPCTSTR DevInstId,
	ULONG Flags)
{
	XTLTRACE(_T("DevInstId=%s,Flags=%08X\n"), DevInstId, Flags);

	*RelDevInstIdList = NULL;

	ULONG bufferLength;
	CONFIGRET ret = ::CM_Get_Device_ID_List_Size(
		&bufferLength, 
		DevInstId,
		Flags);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("CM_Get_Device_ID_List_Size failed with cr=%x.\n", ret);
		return ret;
	}

	XTLTRACE("RequiredBufferSize=%d chars\n", bufferLength);
	if (0 == bufferLength)
	{
		TCHAR* empty = static_cast<TCHAR*>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, 2 * sizeof(TCHAR)));
		*RelDevInstIdList = empty;
		return CR_SUCCESS;
	}

	XTL::AutoProcessHeapPtr<TCHAR> buffer = static_cast<TCHAR*>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, bufferLength * sizeof(TCHAR)));
	if (buffer.IsInvalid())
	{
		XTLTRACE("HeapAlloc failed for %d bytes.\n", bufferLength * sizeof(TCHAR));
		return CR_OUT_OF_MEMORY;
	}

	ret = ::CM_Get_Device_ID_List(
		DevInstId, 
		buffer, 
		bufferLength, 
		Flags);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("CM_Get_Device_ID_List failed with cr=%x.\n", ret);
		return ret;
	}

	*RelDevInstIdList = buffer.Detach();
	XTLTRACE(_T("RelDevInstIdList=%s\n"), *RelDevInstIdList);

	return CR_SUCCESS;
}


DWORD 
pGetDeviceUINumber(
	HDEVINFO DeviceInfoSet, 
	SP_DEVINFO_DATA* DeviceInfoData)
{
	DWORD uiNumber;
	BOOL fSuccess = ::SetupDiGetDeviceRegistryProperty(
		DeviceInfoSet,
		DeviceInfoData,
		SPDRP_UI_NUMBER,
		NULL,
		reinterpret_cast<PBYTE>(&uiNumber),
		sizeof(uiNumber),
		NULL);

	if (!fSuccess)
	{
		return 0;
	}

	return uiNumber;
}

BOOL 
pRequestNdasScsiDeviceEject(
	DWORD SlotNo,
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName, 
	DWORD nNameLength)
{
	// Get devices under Enum\NDAS
	XTL::AutoDeviceInfoSet deviceInfoSet = ::SetupDiGetClassDevs(
		NULL,  
		_T("NDAS"),
		NULL, 
		DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (deviceInfoSet.IsInvalid())
	{
		return FALSE;
	}

	for (DWORD i = 0; ; ++i)
	{
		SP_DEVINFO_DATA deviceInfoData = {0};
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		BOOL fSuccess = ::SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData);
		if (!fSuccess)
		{
			break;
		}
		DWORD uiNumber = pGetDeviceUINumber(deviceInfoSet, &deviceInfoData);
		if (SlotNo == uiNumber)
		{
			//*pConfigRet = ::CM_Query_And_Remove_SubTree(
			//	deviceInfoData.DevInst,
			//	pVetoType,
			//	pszVetoName,
			//	nNameLength,
			//	0);
			*pConfigRet = ::CM_Request_Device_Eject(
				deviceInfoData.DevInst,
				pVetoType,
				pszVetoName,
				nNameLength,
				0);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL
pFindNdasScsiDevInst(
	DEVINST* TargetDevInst,
	DWORD UINumber)
{
	// Get devices under Enum\NDAS
	XTL::AutoDeviceInfoSet devInfoSet = ::SetupDiGetClassDevs(
		NULL,  
		_T("NDAS"),
		NULL, 
		DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (devInfoSet.IsInvalid())
	{
		XTLTRACE_ERR("SetupDiGetClassDevs failed.\n");
		return FALSE;
	}
	for (DWORD i = 0; ; ++i)
	{
		SP_DEVINFO_DATA devInfoData = {0};
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		BOOL fSuccess = ::SetupDiEnumDeviceInfo(devInfoSet, i, &devInfoData);
		if (!fSuccess)
		{
			XTLTRACE_ERR("SetupDiEnumDeviceInfo failed.\n");
			break;
		}
		DWORD uiNumber = pGetDeviceUINumber(devInfoSet, &devInfoData);
		if (UINumber == uiNumber)
		{
			*TargetDevInst = devInfoData.DevInst;
			return TRUE;
		}
	}
	return FALSE;
}

//
// Root\NDASBus ROOT\SYSTEM\XXXX
// -- Bus Relations --> NDAS\SCSIAdapter_R01\<InstanceTag>
// -- Bus Relations --> SCSI\Disk&Ven_XXXX...
// -- Remove Relations --> STORAGE\Volume\1&000XXXX...
//

//
// Returns the symbolic link name of the disk.
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//
LPTSTR
pGetChildDevInstIdsForNdasScsiLocation(
	const NDAS_SCSI_LOCATION* NdasScsiLocation)
{
	DEVINST NdasScsiDevInst;
	if (!pFindNdasScsiDevInst(&NdasScsiDevInst, NdasScsiLocation->SlotNo))
	{
		XTLTRACE_ERR("FindNdasScsiDevInst failed.\n");
		return NULL;
	}

	TCHAR NdasScsiInstanceId[MAX_DEVICE_ID_LEN];
	CONFIGRET ret = ::CM_Get_Device_ID(
		NdasScsiDevInst, 
		NdasScsiInstanceId, 
		RTL_NUMBER_OF(NdasScsiInstanceId), 
		0);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("CM_Get_Device_ID failed CONFIGRET=%x\n", ret);
		return NULL;
	}

	XTL::AutoProcessHeapPtr<TCHAR> ChildDevInstIdList;
	ret = pCMGetDeviceIDList(
		&ChildDevInstIdList, 
		NdasScsiInstanceId, 
		CM_GETIDLIST_FILTER_BUSRELATIONS);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("pCMGetDeviceIDList failed CONFIGRET=%x\n", ret);
		::SetLastError(ConfigRetToWin32Error(ret));
		return NULL;
	}

	return ChildDevInstIdList.Detach();
}

LPTSTR
pGetDiskForNdasScsiLocationEx(
	const NDAS_SCSI_LOCATION* NdasScsiLocation,
	BOOL SymbolicLinkOrDevInstId)
{
	// Get the child device instances of the NDAS SCSI pdo
	XTL::AutoProcessHeapPtr<TCHAR> ChildDevInstIdList =
		pGetChildDevInstIdsForNdasScsiLocation(NdasScsiLocation);

	// Find the disk of TargetID and LUN for all child device instances
	for (LPCTSTR ChildDevInstId = ChildDevInstIdList; 
		ChildDevInstId && *ChildDevInstId; 
		ChildDevInstId += ::lstrlen(ChildDevInstId) + 1)
	{
		XTLTRACE(_T("ChildDevInstId:%s\n"), ChildDevInstId);

		// Ensure that the child really is present (not ghosted)
		DEVINST ChildDevInst;
		CONFIGRET ret = ::CM_Locate_DevNode(
			&ChildDevInst, 
			const_cast<DEVINSTID>(ChildDevInstId), 
			CM_LOCATE_DEVNODE_NORMAL);
		if (CR_SUCCESS != ret)
		{
			XTLTRACE("CM_Locate_DevNode failed CONFIGRET=%x\n", ret);
			continue;
		}

		// Query the Disk Class Interface, if there are no disk class
		// interface, it is not of a disk class.
		XTL::AutoProcessHeapPtr<TCHAR> SymLinkList;
		ret = pCMGetDeviceInterfaceList(
			&SymLinkList,
			&DiskClassGuid,
			const_cast<DEVINSTID>(ChildDevInstId),
			CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
		if (CR_SUCCESS != ret)
		{
			XTLTRACE("pCMGetDeviceInterfaceList failed CONFIGRET=%x\n", ret);
			continue;
		}

		// Returned list contains the list of the device file names (or
		// symbolic links) which we can open with CreateFile for IO_CTLs
		for (LPCTSTR DeviceName = SymLinkList;
			DeviceName && *DeviceName;
			DeviceName += ::lstrlen(DeviceName) + 1)
		{
			XTLTRACE(_T("Device:%s\n"), DeviceName);
			XTL::AutoFileHandle hDevice = ::CreateFile(
				DeviceName, 
				GENERIC_READ, 
				FILE_SHARE_READ | FILE_SHARE_WRITE, 
				NULL, 
				OPEN_EXISTING, 
				0, 
				NULL);
			if (hDevice.IsInvalid())
			{
				XTLTRACE_ERR("CreateFile(%ls) failed.\n", DeviceName);
				continue;
			}
			// Query the SCSI Address and compare TargetID and LUN to
			// compare them with those of NDAS SCSI location.
			SCSI_ADDRESS scsiAddress;
			if (!pScsiGetAddress(hDevice, &scsiAddress))
			{
				XTLTRACE_ERR("GetScsiAddress(%ls) failed.\n", DeviceName);
				continue;
			}
			if (NdasScsiLocation->TargetID == scsiAddress.TargetId &&
				NdasScsiLocation->LUN == scsiAddress.Lun)
			{
				// We found the target disk, and we create a buffer to
				// return. If SymbolicLinkOrDevInstId is non-zero (TRUE),
				// we will returns SymbolicLink, otherwise, we will return the DevInstId
				if (SymbolicLinkOrDevInstId)
				{
					DWORD TargetLength = (::lstrlen(DeviceName) + 1) * sizeof(TCHAR);
					XTL::AutoProcessHeapPtr<TCHAR> TargetDeviceName = 
						static_cast<LPTSTR>(
							::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, TargetLength));
					if (TargetDeviceName.IsInvalid())
					{
						XTLTRACE_ERR("HeapAlloc %d bytes failed.\n", TargetLength);
						return NULL;
					}
					::CopyMemory(TargetDeviceName, DeviceName, TargetLength);
					return TargetDeviceName.Detach();
				}
				else
				{
					DWORD TargetLength = (::lstrlen(ChildDevInstId) + 1) * sizeof(TCHAR);
					XTL::AutoProcessHeapPtr<TCHAR> TargetDevInstId = 
						static_cast<LPTSTR>(
							::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, TargetLength));
					if (TargetDevInstId.IsInvalid())
					{
						XTLTRACE_ERR("HeapAlloc %d bytes failed.\n", TargetLength);
						return NULL;
					}
					::CopyMemory(TargetDevInstId, ChildDevInstId, TargetLength);
					return TargetDevInstId.Detach();
				}
			}
		}
	}

	return NULL;
}

LPTSTR
pGetDiskForNdasScsiLocation(
	const NDAS_SCSI_LOCATION* NdasScsiLocation)
{
	return pGetDiskForNdasScsiLocationEx(NdasScsiLocation, TRUE);
}


//
// Returns the symbolic link name list of the volume.
// Each entry is null-terminated and the last entry is terminated 
// by an additional null character
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//

LPTSTR
pGetVolumeInstIdListForDisk(
	LPCTSTR DiskInstId)
{
	// Disk and volumes are removal relations.
	XTL::AutoProcessHeapPtr<TCHAR> VolumeInstIdList;
	CONFIGRET ret = pCMGetDeviceIDList(
		&VolumeInstIdList, 
		DiskInstId, 
		CM_GETIDLIST_FILTER_REMOVALRELATIONS);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("pCMGetDeviceIDList failed CONFIGRET=%x\n", ret);
		::SetLastError(ConfigRetToWin32Error(ret));
		return NULL;
	}
	return VolumeInstIdList.Detach();
}

LPTSTR
pGetDeviceSymbolicLinkList(
	LPCGUID InterfaceClassGuid,
	DEVINSTID DevInstId,
	ULONG Flags /*= CM_GET_DEVICE_INTERFACE_LIST_PRESENT*/)
{
	XTL::AutoProcessHeapPtr<TCHAR> SymbolicLinkList;
	CONFIGRET ret = pCMGetDeviceInterfaceList(
		&SymbolicLinkList,
		const_cast<LPGUID>(InterfaceClassGuid),
		DevInstId,
		Flags);
	if (CR_SUCCESS != ret)
	{
		XTLTRACE("pCMGetDeviceInterfaceList failed CONFIGRET=%x\n", ret);
		::SetLastError(ConfigRetToWin32Error(ret));
		return NULL;
	}
	return SymbolicLinkList.Detach();
}

//
// Returns the symbolic link name list of the volume.
// Each entry is null-terminated and the last entry is terminated 
// by an additional null character
//
// Caller should free the non-null returned pointer 
// with HeapFree(GetProcessHeap(),...)
//
LPTSTR
pGetVolumesForNdasScsiLocation(
	const NDAS_SCSI_LOCATION* NdasScsiLocation)
{
	// Get the disk instance id of the location
	XTL::AutoProcessHeapPtr<TCHAR> DiskInstId =
		pGetDiskForNdasScsiLocationEx(NdasScsiLocation, FALSE);

	if (DiskInstId.IsInvalid())
	{
		XTLTRACE_ERR("GetDiskDevInstId failed.\n");
		return NULL;
	}

	// Get the volume instance id list of the disk
	XTL::AutoProcessHeapPtr<TCHAR> VolumeInstIdList =
		pGetVolumeInstIdListForDisk(DiskInstId);
	if (VolumeInstIdList.IsInvalid())
	{
		XTLTRACE_ERR("GetVolumeInstIdList failed.\n");
		return NULL;
	}

	// Get the volume names of the disk (via interface query)
	// Preallocate the buffer up to MAX_PATH
	DWORD bufferLength = MAX_PATH * sizeof(TCHAR);
	DWORD bufferRemaining = bufferLength;
	XTL::AutoProcessHeapPtr<TCHAR> buffer = static_cast<LPTSTR>(
		::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, bufferLength));
	LPTSTR bufferNext = static_cast<LPTSTR>(buffer); // direct resource access (non-const)

	for (LPCTSTR VolumeInstId = VolumeInstIdList;
		 VolumeInstId && *VolumeInstId;
		 VolumeInstId += ::lstrlen(VolumeInstId) + 1)
	{
		// Actually returned buffer is a list of null-terminated strings
		// but we queried for a specific instance id and for the specific
		// class, where only one or zero volume name may return. 
		// So the return type is just a single string.
		XTL::AutoProcessHeapPtr<TCHAR> VolumeName = 
			pGetDeviceSymbolicLinkList(
				&GUID_DEVINTERFACE_VOLUME,
				const_cast<LPTSTR>(VolumeInstId),
				CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
		if (VolumeName.IsInvalid())
		{
			// may not be a volume
			XTLTRACE(_T("%s is not a volume device\n"), VolumeInstId);
			continue;
		}
		for (LPCTSTR VN = VolumeName; VN && *VN; VN += ::lstrlen(VN) + 1)
		{
			_tprintf(_T("VN:%s\n"), VN);
		}

		XTLTRACE(_T("VolumeName=%s\n"), VolumeName);
		DWORD volumeNameLength = ::lstrlen(VolumeName);
		DWORD bufferRequired =  (volumeNameLength + 1) * sizeof(TCHAR);
		if (bufferRemaining < bufferRequired)
		{
			XTLTRACE(_T("Reallocation\n"));
			XTL::AutoProcessHeapPtr<TCHAR> newBuffer = static_cast<LPTSTR>(
				::HeapReAlloc(
					::GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					buffer,
					bufferLength + bufferRequired - bufferRemaining));
			if (newBuffer.IsInvalid())
			{
				XTLTRACE(_T("Memory allocation %d bytes failed.\n"), 
						 bufferLength + bufferLength - bufferRemaining);
				return NULL;
			}
			size_t bufferNextOffset = 
				reinterpret_cast<BYTE*>(bufferNext) - 
				reinterpret_cast<BYTE*>(*(&buffer));
			// discard the old buffer and attach it to the new buffer
			buffer.Detach();
			buffer = newBuffer.Detach();
			bufferNext = reinterpret_cast<TCHAR*>(
				reinterpret_cast<BYTE*>(static_cast<LPTSTR>(buffer)) + bufferNextOffset);
		}
		::CopyMemory(bufferNext, VolumeName, bufferRequired);
		bufferNext += volumeNameLength + 1;
		bufferRemaining -= bufferRequired;
	}

	return buffer.Detach();
}

