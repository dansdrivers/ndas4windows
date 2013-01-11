#include "stdafx.h"
#include "sysutil.h"
#include <winioctl.h>
#include <cfgmgr32.h>
#include <ntddndis.h>
#include "autores.h"
#include "xdebug.h"

namespace sysutil {

BOOL DisableDiskWriteCache(HANDLE hDisk)
{
	BOOL fSuccess = FALSE;
	DWORD cbReturned = 0;
	DISK_CACHE_INFORMATION diskCacheInfo = {0};

	fSuccess = ::DeviceIoControl(
		hDisk,
		IOCTL_DISK_GET_CACHE_INFORMATION,
		NULL,
		0,
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_DISK_GET_CACHE_INFORMATION) failed: "));
		return FALSE;
	}

	DPInfo(_FT("Disk Write Cache Enabled: %d.\n"), diskCacheInfo.WriteCacheEnabled);
	if (!diskCacheInfo.WriteCacheEnabled) {
		DPInfo(_FT("Disk Write Cache Already Disabled. Ignoring.\n"));
		return TRUE;
	}

	diskCacheInfo.WriteCacheEnabled = FALSE;
	fSuccess = ::DeviceIoControl(
		hDisk,
		IOCTL_DISK_SET_CACHE_INFORMATION,
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_DISK_SET_CACHE_INFORMATION) failed: "));
		return FALSE;
	}

	DPInfo(_FT("Disable Write Cache (%p) completed successfully.\n"), hDisk);
	return TRUE;

}

BOOL DisableDiskWriteCache(LPCTSTR szDiskPath)
{
	DPInfo(_FT("CreateFile(%s).\n"), szDiskPath);

	AutoFileHandle hDisk = ::CreateFile(
		szDiskPath, 
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == hDisk) {
		DPErrorEx(_FT("CreateFile(%s) failed: "), szDiskPath);
		return FALSE;
	}

	return DisableDiskWriteCache(hDisk);
}

BOOL GetStorageHotplugInfo(
	HANDLE hStorage, 
	LPBOOL lpbMediaRemovable,
	LPBOOL lpbMediaHotplug, 
	LPBOOL lpbDeviceHotplug)
{
	STORAGE_HOTPLUG_INFO HotplugInfo = {0};

	DWORD cbReturned = 0;

	BOOL fSuccess = DeviceIoControl(
		hStorage,
		IOCTL_STORAGE_GET_HOTPLUG_INFO,
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&cbReturned, NULL);

	if (fSuccess) {
		if (lpbMediaRemovable) *lpbMediaRemovable = (BOOL)HotplugInfo.MediaRemovable;
		if (lpbMediaHotplug) *lpbMediaHotplug = (BOOL)HotplugInfo.MediaHotplug;
		if (lpbDeviceHotplug) *lpbDeviceHotplug = (BOOL)HotplugInfo.DeviceHotplug;
	}

	return fSuccess;
}

BOOL SetStorageHotplugInfo(
	HANDLE hStorage,
	BOOL bMediaRemovable,
	BOOL bMediaHotplug,
	BOOL bDeviceHotplug)
{
	//TCHAR pBuffer[MAX_PATH];
	//DWORD cbBuffer = MAX_PATH;

	//CONFIGRET cr = CM_Get_DevNode_Registry_Property(
	//	devInst,
	//	CM_DRP_PHYSICAL_DEVICE_OBJECT_NAME,
	//	NULL,
	//	pBuffer,
	//	&cbBuffer,
	//	0);

	STORAGE_HOTPLUG_INFO HotplugInfo = {0};
	HotplugInfo.Size = sizeof(STORAGE_HOTPLUG_INFO);
	HotplugInfo.MediaRemovable = (BOOLEAN)bMediaRemovable;
	HotplugInfo.MediaHotplug = (BOOLEAN)bMediaHotplug;
	HotplugInfo.DeviceHotplug = (BOOLEAN)bDeviceHotplug;

	// The following value is not used.
	// HotplugInfo.WriteCacheEnableOverride;

	DWORD cbReturned = 0;
	BOOL fSuccess = DeviceIoControl(
		hStorage,
		IOCTL_STORAGE_SET_HOTPLUG_INFO,
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&cbReturned, NULL);

	return fSuccess;
}

BOOL 
pMatchNetConnAddr(
	HANDLE hDevice,
	DWORD cbAddr, 
	BYTE* pPhysicalAddr)
{
	NDIS_OID oid_addr = OID_802_3_CURRENT_ADDRESS;
	BYTE nic_addr[6];

	DWORD cbReturned;
	BOOL fSuccess = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid_addr,
		sizeof(NDIS_OID),
		(PVOID)&nic_addr,
		6,
		&cbReturned,
		NULL);

	if (!fSuccess) {
		return FALSE;
	}

	if (cbAddr > 0) {
		INT iCmp = ::memcmp(nic_addr,pPhysicalAddr,cbAddr);
		return (0 == iCmp);
	} else {
		return TRUE;
	}

}

LONGLONG 
pGetNetConnLinkSpeed(HANDLE hDevice)
{
	NDIS_OID oid = OID_GEN_LINK_SPEED;
	LONGLONG llLinkSpeed = 0;

	DWORD cbReturned;
	BOOL fSuccess = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid,
		sizeof(NDIS_OID),
		(PVOID)&llLinkSpeed,
		sizeof(LONGLONG),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		return 0;
	}

	return llLinkSpeed;
}


BOOL WINAPI
pGetNetConnMediaInUse(HANDLE hDevice, NDIS_MEDIUM* pMedium)
{
	NDIS_OID oid = OID_GEN_MEDIA_IN_USE;

	DWORD cbReturned;
	BOOL fSuccess = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid,
		sizeof(NDIS_OID),
		(PVOID)pMedium,
		sizeof(NDIS_MEDIUM),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI
pGetNetConnPhysicalMedium(HANDLE hDevice, NDIS_PHYSICAL_MEDIUM* pPhyMedium)
{
	NDIS_OID oid = OID_GEN_PHYSICAL_MEDIUM;

	DWORD cbReturned;
	BOOL fSuccess = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid,
		sizeof(NDIS_OID),
		(PVOID)pPhyMedium,
		sizeof(NDIS_PHYSICAL_MEDIUM),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI
GetNetConnCharacteristics(
	IN DWORD cbAddr, 
	IN BYTE* pPhysicalAddr,
	OUT PLONGLONG pllLinkSpeed,
	OUT sysutil::NDIS_MEDIUM* pMediumInUse,
	OUT sysutil::NDIS_PHYSICAL_MEDIUM* pPhysicalMedium)
{
	BOOL fSuccess;
	HRESULT hr;

	LPCTSTR NetConnectionKeyPath = 
		_T("System\\CurrentControlSet\\Control\\Network\\")
		_T("{4D36E972-E325-11CE-BFC1-08002BE10318}");

	CONST DWORD MAX_REG_KEY_LEN = 255;

	HKEY hConnKey;

	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		NetConnectionKeyPath,
		0,
		KEY_READ,
		&hConnKey);

	if (ERROR_SUCCESS != lResult) {
		return FALSE;
	}

	AutoHKey autoConnKey = hConnKey;

	for (DWORD i = 0; ; ++i) {

		TCHAR szKeyName[MAX_REG_KEY_LEN] = {0};
		DWORD cchKeyName = MAX_REG_KEY_LEN;
		FILETIME ftLastWritten;
		LONG lResult = ::RegEnumKeyEx(
			hConnKey, 
			i, 
			szKeyName, 
			&cchKeyName,
			NULL,
			NULL,
			NULL,
			&ftLastWritten);

		if (ERROR_NO_MORE_ITEMS == lResult ||
			ERROR_SUCCESS != lResult) 
		{
			break;
		}


		//
		// szKeyName contains the Network Connection Service name
		//
		// We are only interested in GUID strings
		// 38 is a length of {XXXXXXXX-....}
		if (38 != cchKeyName) {
			continue;
		}

		_tprintf(_T("%s\n"), szKeyName);

		//
		// Let's open the device
		//
		LPCTSTR szServiceName = szKeyName;
		TCHAR szDeviceName[MAX_PATH];
		hr = ::StringCchPrintf(
			szDeviceName, MAX_PATH, 
			_T("\\Device\\%s"), szServiceName);
		_ASSERTE(SUCCEEDED(hr));

		fSuccess = ::DefineDosDevice(
			DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM, 
			szServiceName, 
			szDeviceName);

		if (!fSuccess) {
			continue;
		}

		//
		// From here we have to call 
		//  ::DefineDosDevice(DDD_REMOVE_DEFINITION)
		// when gets out of scope
		// 

		//
		// reuse szDeviceName
		//
		hr = ::StringCchPrintf(
			szDeviceName, MAX_PATH, 
			_T("\\\\.\\%s"), szServiceName);

		_ASSERTE(SUCCEEDED(hr));

		AutoFileHandle hNIC = CreateFile(
			szDeviceName,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);

		if (INVALID_HANDLE_VALUE == hNIC) {
			fSuccess = ::DefineDosDevice(DDD_REMOVE_DEFINITION, szServiceName, NULL);
			_ASSERTE(fSuccess);
			continue;
		}

		BOOL fMatch = pMatchNetConnAddr(
			hNIC, 
			cbAddr, 
			pPhysicalAddr);

		if (!fMatch) {
			fSuccess = ::DefineDosDevice(DDD_REMOVE_DEFINITION, szServiceName, NULL);
			_ASSERTE(fSuccess);
			continue;
		}

		if (pllLinkSpeed) {
			*pllLinkSpeed = pGetNetConnLinkSpeed(hNIC);
		}
		
		if (pMediumInUse) {
			fSuccess = pGetNetConnMediaInUse(hNIC, pMediumInUse);
			if (!fSuccess) {
				*pMediumInUse = NdisMediumMax;
			}
		}

		if (pPhysicalMedium) {
			fSuccess = pGetNetConnPhysicalMedium(hNIC, pPhysicalMedium);
			if (!fSuccess) {
				*pPhysicalMedium = NdisPhysicalMediumMax;
			}
		}

		fSuccess = ::DefineDosDevice( DDD_REMOVE_DEFINITION, szServiceName, NULL );
		_ASSERTE(fSuccess);

//		_tprintf(_T("%s, %s, Link Speed = %I64u Mbps\n"), 
//			NDIS_MEDIUM_String(*pMediumInUse),
//			NDIS_PHYSICAL_MEDIUM_String(*pPhysicalMedium),
//			(*pllLinkSpeed / (10 * 1000)));
		return TRUE;
	}

	return FALSE;
}

LONGLONG WINAPI
GetNetConnLinkSpeed(
	DWORD cbAddr, 
	BYTE* pConnAddr)
{
	LONGLONG llLinkSpeed;

	BOOL fSuccess = GetNetConnCharacteristics(
		cbAddr, 
		pConnAddr, 
		&llLinkSpeed, 
		NULL, 
		NULL);

	if (!fSuccess) {
		return 0;
	}

	return llLinkSpeed;
}

LPCTSTR NDIS_PHYSICAL_MEDIUM_String(sysutil::NDIS_PHYSICAL_MEDIUM m)
{
	switch (m) {
	case sysutil::NdisPhysicalMediumUnspecified: return _T("NdisPhysicalMediumUnspecified");
	case sysutil::NdisPhysicalMediumWirelessLan: return _T("NdisPhysicalMediumWirelessLan");
	case sysutil::NdisPhysicalMediumCableModem: return _T("NdisPhysicalMediumCableModem");
	case sysutil::NdisPhysicalMediumPhoneLine: return _T("NdisPhysicalMediumPhoneLine");
	case sysutil::NdisPhysicalMediumPowerLine: return _T("NdisPhysicalMediumPowerLine");
	case sysutil::NdisPhysicalMediumDSL: return _T("NdisPhysicalMediumDSL");
	case sysutil::NdisPhysicalMediumFibreChannel: return _T("NdisPhysicalMediumFibreChannel");
	case sysutil::NdisPhysicalMedium1394: return _T("NdisPhysicalMedium1394");
	case sysutil::NdisPhysicalMediumWirelessWan: return _T("NdisPhysicalMediumWirelessWan");
	case sysutil::NdisPhysicalMediumMax: 
	default:
	return _T("NdisPhysicalMedium_???");
	}
}

LPCTSTR NDIS_MEDIUM_String(sysutil::NDIS_MEDIUM m)
{
	switch (m) {
	case sysutil::NdisMedium802_3: return _T("NdisMedium802_3");
	case sysutil::NdisMedium802_5: return _T("NdisMedium802_5");
	case sysutil::NdisMediumFddi: return _T("NdisMediumFddi");
	case sysutil::NdisMediumWan: return _T("NdisMediumWan");
	case sysutil::NdisMediumLocalTalk: return _T("NdisMediumLocalTalk");
	case sysutil::NdisMediumDix: return _T("NdisMediumDix");
	case sysutil::NdisMediumArcnetRaw: return _T("NdisMediumArcnetRaw");
	case sysutil::NdisMediumArcnet878_2: return _T("NdisMediumArcnet878_2");
	case sysutil::NdisMediumAtm:	return _T("NdisMediumAtm");
	case sysutil::NdisMediumWirelessWan: return _T("NdisMediumWirelessWan");
	case sysutil::NdisMediumIrda: return _T("NdisMediumIrda");
	case sysutil::NdisMediumBpc: return _T("NdisMediumBpc");
	case sysutil::NdisMediumCoWan: return _T("NdisMediumCoWan");
	case sysutil::NdisMedium1394: return _T("NdisMedium1394");
	case sysutil::NdisMediumInfiniBand: return _T("NdisMediumInfiniBand");
	case sysutil::NdisMediumMax:
	default:
	return _T("NdisMedium_???");
	}
}

} // namespace sysutil
