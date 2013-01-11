#include "stdafx.h"
#include "sysutil.h"
#include <winioctl.h>
#include <cfgmgr32.h>
#include <ntddndis.h>
#include <xtl/xtlautores.h>

#include "trace.h"
#ifdef RUN_WPP
#include "sysutil.tmh"
#endif

namespace sysutil {

LPCSTR NDIS_PHYSICAL_MEDIUM_StringA(sysutil::NDIS_PHYSICAL_MEDIUM m);
LPCSTR NDIS_MEDIUM_StringA(sysutil::NDIS_MEDIUM m);

BOOL DisableDiskWriteCache(HANDLE hDisk)
{
	DWORD cbReturned = 0;
	DISK_CACHE_INFORMATION diskCacheInfo = {0};

	BOOL success = ::DeviceIoControl(
		hDisk,
		IOCTL_DISK_GET_CACHE_INFORMATION,
		NULL,
		0,
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&cbReturned,
		NULL);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_ERROR, 
			"DiskGetCacheInformation failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_INFORMATION, 
		"Disk Write Cache Enabled? %s.\n", 
		diskCacheInfo.WriteCacheEnabled ? "yes" : "no");

	if (!diskCacheInfo.WriteCacheEnabled) 
	{
		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_INFORMATION, 
			"Disk Write Cache Already Disabled. Ignoring.\n");
		return TRUE;
	}

	diskCacheInfo.WriteCacheEnabled = FALSE;
	success = ::DeviceIoControl(
		hDisk,
		IOCTL_DISK_SET_CACHE_INFORMATION,
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&cbReturned,
		NULL);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_ERROR, 
			"DiskSetCacheInformation failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_INFORMATION, 
		"Disable Write Cache (%p) completed successfully.\n", hDisk);

	return TRUE;

}

BOOL
DisableDiskWriteCache(LPCTSTR szDiskPath)
{
	XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_INFORMATION, 
		"CreateFile(%ls).\n", szDiskPath);

	XTL::AutoFileHandle hDisk = ::CreateFile(
		szDiskPath, 
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hDisk.IsInvalid()) 
	{
		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_INFORMATION, 
			"CreateFile(%ls) failed, error=0x%X\n", szDiskPath, GetLastError());
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
	BOOL success = DeviceIoControl(
		hStorage,
		IOCTL_STORAGE_GET_HOTPLUG_INFO,
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&cbReturned, NULL);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_ERROR, 
			"StorageGetHotPlugInfo failed, error=0x%X\n", GetLastError());
	}
	else
	{
		if (lpbMediaRemovable) *lpbMediaRemovable = (BOOL)HotplugInfo.MediaRemovable;
		if (lpbMediaHotplug) *lpbMediaHotplug = (BOOL)HotplugInfo.MediaHotplug;
		if (lpbDeviceHotplug) *lpbDeviceHotplug = (BOOL)HotplugInfo.DeviceHotplug;
	}

	return success;
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
	BOOL success = DeviceIoControl(
		hStorage,
		IOCTL_STORAGE_SET_HOTPLUG_INFO,
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&HotplugInfo, sizeof(STORAGE_HOTPLUG_INFO),
		&cbReturned, NULL);
	if (!success)
	{
		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_ERROR, 
			"StorageSetHotPlugInfo failed, error=0x%X\n", GetLastError());
	}
	return success;
}

BOOL 
pMatchNetConnAddr(
	HANDLE hDevice,
	DWORD cbAddr, 
	CONST BYTE* pPhysicalAddr)
{
	NDIS_OID oid_addr = OID_802_3_CURRENT_ADDRESS;
	BYTE nic_addr[6];

	DWORD cbReturned;
	BOOL success = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid_addr,
		sizeof(NDIS_OID),
		(PVOID)&nic_addr,
		6,
		&cbReturned,
		NULL);

	if (!success) {
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
	BOOL success = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid,
		sizeof(NDIS_OID),
		(PVOID)&llLinkSpeed,
		sizeof(LONGLONG),
		&cbReturned,
		NULL);

	if (!success) {
		return 0;
	}

	return llLinkSpeed;
}


BOOL WINAPI
pGetNetConnMediaInUse(
	HANDLE hDevice, 
	NDIS_MEDIUM* pMedium)
{
	NDIS_OID oid = OID_GEN_MEDIA_IN_USE;

	DWORD cbReturned;
	BOOL success = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid,
		sizeof(NDIS_OID),
		(PVOID)pMedium,
		sizeof(NDIS_MEDIUM),
		&cbReturned,
		NULL);

	if (!success) {
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI
pGetNetConnPhysicalMedium(
	HANDLE hDevice, 
	NDIS_PHYSICAL_MEDIUM* pPhyMedium)
{
	NDIS_OID oid = OID_GEN_PHYSICAL_MEDIUM;

	DWORD cbReturned;
	BOOL success = ::DeviceIoControl(
		hDevice,
		(DWORD)IOCTL_NDIS_QUERY_GLOBAL_STATS,
		(PVOID)&oid,
		sizeof(NDIS_OID),
		(PVOID)pPhyMedium,
		sizeof(NDIS_PHYSICAL_MEDIUM),
		&cbReturned,
		NULL);

	if (!success) {
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI
GetNetConnCharacteristics(
	IN DWORD cbAddr, 
	IN CONST BYTE* pPhysicalAddr,
	OUT PLONGLONG pllLinkSpeed,
	OUT sysutil::NDIS_MEDIUM* pMediumInUse,
	OUT sysutil::NDIS_PHYSICAL_MEDIUM* pPhysicalMedium)
{
	BOOL success;
	HRESULT hr;

	LPCTSTR NetConnectionKeyPath = 
		_T("System\\CurrentControlSet\\Control\\Network\\")
		_T("{4D36E972-E325-11CE-BFC1-08002BE10318}");

	CONST DWORD MAX_REG_KEY_LEN = 255;

	XTL::AutoKeyHandle hConnKey;

	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		NetConnectionKeyPath,
		0,
		KEY_READ,
		&hConnKey);

	if (ERROR_SUCCESS != lResult) 
	{
		return FALSE;
	}

	for (DWORD i = 0; ; ++i) 
	{

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
		if (38 != cchKeyName) 
		{
			continue;
		}

		//
		// Let's open the device
		//
		LPCTSTR szServiceName = szKeyName;
		TCHAR szDeviceName[MAX_PATH];
		hr = ::StringCchPrintf(
			szDeviceName, MAX_PATH, 
			_T("\\Device\\%s"), szServiceName);
		XTLASSERT(SUCCEEDED(hr));

		success = ::DefineDosDevice(
			DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM, 
			szServiceName, 
			szDeviceName);

		if (!success) 
		{
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

		XTLASSERT(SUCCEEDED(hr));

		XTL::AutoFileHandle hNIC = CreateFile(
			szDeviceName,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);

		if (hNIC.IsInvalid()) 
		{
			success = ::DefineDosDevice(DDD_REMOVE_DEFINITION, szServiceName, NULL);
			XTLASSERT(success);
			continue;
		}

		BOOL fMatch = pMatchNetConnAddr(
			hNIC, 
			cbAddr, 
			pPhysicalAddr);

		if (!fMatch) 
		{
			success = ::DefineDosDevice(DDD_REMOVE_DEFINITION, szServiceName, NULL);
			XTLASSERT(success);
			continue;
		}

		if (pllLinkSpeed) 
		{
			*pllLinkSpeed = pGetNetConnLinkSpeed(hNIC);
		}
		
		if (pMediumInUse) 
		{
			success = pGetNetConnMediaInUse(hNIC, pMediumInUse);
			if (!success) 
			{
				*pMediumInUse = NdisMediumMax;
			}
		}

		if (pPhysicalMedium) {
			success = pGetNetConnPhysicalMedium(hNIC, pPhysicalMedium);
			if (!success) {
				*pPhysicalMedium = NdisPhysicalMediumMax;
			}
		}

		success = ::DefineDosDevice( DDD_REMOVE_DEFINITION, szServiceName, NULL );
		XTLASSERT(success);

		XTLTRACE2(NDASSVC_SYSTEMUTIL, TRACE_LEVEL_INFORMATION, 
			"%hs, %hs, Link Speed = %I64u Mbps\n", 
			NDIS_MEDIUM_StringA(*pMediumInUse),
			NDIS_PHYSICAL_MEDIUM_StringA(*pPhysicalMedium),
			(*pllLinkSpeed / (10 * 1000)));
		return TRUE;
	}

	return FALSE;
}

LONGLONG WINAPI
GetNetConnLinkSpeed(
	DWORD cbAddr, 
	CONST BYTE* pConnAddr)
{
	LONGLONG llLinkSpeed;

	BOOL success = GetNetConnCharacteristics(
		cbAddr, 
		pConnAddr, 
		&llLinkSpeed, 
		NULL, 
		NULL);

	if (!success) {
		return 0;
	}

	return llLinkSpeed;
}

LPCSTR 
NDIS_PHYSICAL_MEDIUM_StringA(sysutil::NDIS_PHYSICAL_MEDIUM m)
{
	switch (m) 
	{
	case sysutil::NdisPhysicalMediumUnspecified: return "NdisPhysicalMediumUnspecified";
	case sysutil::NdisPhysicalMediumWirelessLan: return "NdisPhysicalMediumWirelessLan";
	case sysutil::NdisPhysicalMediumCableModem: return "NdisPhysicalMediumCableModem";
	case sysutil::NdisPhysicalMediumPhoneLine: return "NdisPhysicalMediumPhoneLine";
	case sysutil::NdisPhysicalMediumPowerLine: return "NdisPhysicalMediumPowerLine";
	case sysutil::NdisPhysicalMediumDSL: return "NdisPhysicalMediumDSL";
	case sysutil::NdisPhysicalMediumFibreChannel: return "NdisPhysicalMediumFibreChannel";
	case sysutil::NdisPhysicalMedium1394: return "NdisPhysicalMedium1394";
	case sysutil::NdisPhysicalMediumWirelessWan: return "NdisPhysicalMediumWirelessWan";
	case sysutil::NdisPhysicalMediumMax: 
	default:
	return "NdisPhysicalMedium_???";
	}
}

LPCSTR NDIS_MEDIUM_StringA(sysutil::NDIS_MEDIUM m)
{
	switch (m) 
	{
	case sysutil::NdisMedium802_3: return "NdisMedium802_3";
	case sysutil::NdisMedium802_5: return "NdisMedium802_5";
	case sysutil::NdisMediumFddi: return "NdisMediumFddi";
	case sysutil::NdisMediumWan: return "NdisMediumWan";
	case sysutil::NdisMediumLocalTalk: return "NdisMediumLocalTalk";
	case sysutil::NdisMediumDix: return "NdisMediumDix";
	case sysutil::NdisMediumArcnetRaw: return "NdisMediumArcnetRaw";
	case sysutil::NdisMediumArcnet878_2: return "NdisMediumArcnet878_2";
	case sysutil::NdisMediumAtm:	return "NdisMediumAtm";
	case sysutil::NdisMediumWirelessWan: return "NdisMediumWirelessWan";
	case sysutil::NdisMediumIrda: return "NdisMediumIrda";
	case sysutil::NdisMediumBpc: return "NdisMediumBpc";
	case sysutil::NdisMediumCoWan: return "NdisMediumCoWan";
	case sysutil::NdisMedium1394: return "NdisMedium1394";
	case sysutil::NdisMediumInfiniBand: return "NdisMediumInfiniBand";
	case sysutil::NdisMediumMax:
	default:
	return "NdisMedium_???";
	}
}

} // namespace sysutil
