#include "precomp.h"

typedef struct _TYPE_STRING_ENTRY {
	UINT Value;
	LPCTSTR DisplayName;
} TYPE_STRING_ENTRY;

/* status type strings */
static const TYPE_STRING_ENTRY 
NdasDeviceStatusStable[] = {
	NDAS_DEVICE_STATUS_UNKNOWN, _T("Unknown"),
	NDAS_DEVICE_STATUS_DISABLED, _T("Disabled"),
	NDAS_DEVICE_STATUS_DISCONNECTED, _T("Disconnected"),
	NDAS_DEVICE_STATUS_CONNECTED, _T("Connected"),
	NDAS_DEVICE_STATUS_CONNECTING, _T("Connecting"),
	NDAS_DEVICE_STATUS_NOT_REGISTERED, _T("Not registered")
};

static const TYPE_STRING_ENTRY
NdasLogicalDeviceStatusStable[] = {
	NDAS_LOGICALDEVICE_STATUS_UNKNOWN, _T("Unknown"),
	NDAS_LOGICALDEVICE_STATUS_UNMOUNTED, _T("Not mounted"),
	NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING, _T("Mounting"),
	NDAS_LOGICALDEVICE_STATUS_MOUNTED, _T("Mounted"),
	NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING, _T("Dismounting"),
	NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED, _T("Not initialized"),
};

static const TYPE_STRING_ENTRY 
NdasUnitDeviceTypeStable[] = {
	NDAS_UNITDEVICE_TYPE_UNKNOWN, _T("Unknown"),
	NDAS_UNITDEVICE_TYPE_DISK, _T("Disk"),
	NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK, _T("CompactFlash"),
	NDAS_UNITDEVICE_TYPE_CDROM, _T("CDROM"),
	NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY, _T("MO")
};

static const TYPE_STRING_ENTRY 
NdasLogicalDeviceTypeStable[] = {
	NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE, _T("Basic"),
	NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED, _T("Mirror Disk (Downlevel)"),
	NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED, _T("Aggregated"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID0, _T("RAID-0"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID1, _T("RAID-1"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID4, _T("RAID-4"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2, _T("RAID-1 Rev. 2"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2, _T("RAID-4 Rev. 2"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3, _T("RAID-1 Rev. 3"),
	NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3, _T("RAID-4 Rev. 3"),
	NDAS_LOGICALDEVICE_TYPE_DVD, _T("DVD"),
	NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD, _T("Virtual DVD"),
	NDAS_LOGICALDEVICE_TYPE_MO, _T("MO"),
	NDAS_LOGICALDEVICE_TYPE_FLASHCARD, _T("FLASHCARD"),
};

static const TYPE_STRING_ENTRY 
NdasLogicalDeviceErrorStable[] = {
	NDAS_LOGICALDEVICE_ERROR_NONE, _T("No error"),
	NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER, _T("Some members are missing"),
	NDAS_LOGICALDEVICE_ERROR_REQUIRE_UPGRADE, _T("Requires upgrade"),
	NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION, _T("Requires conflict resolution"),
	NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE, _T("Mountable in degraded mode")
};

static const TYPE_STRING_ENTRY 
NdasDeviceErrorStable[] = {
	NDAS_DEVICE_ERROR_NONE, _T("No error"),
	NDAS_DEVICE_ERROR_UNSUPPORTED_VERSION, _T("Unsupported hardware version"),
	NDAS_DEVICE_ERROR_LPX_SOCKET_FAILED, _T("LPX socket failed"),
	NDAS_DEVICE_ERROR_DISCOVER_FAILED, _T("Discover failed"),
	NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE, _T("Discover failed too many times"),
	NDAS_DEVICE_ERROR_FROM_SYSTEM, _T("System error"),
	NDAS_DEVICE_ERROR_LOGIN_FAILED, _T("Login failed"),
	NDAS_DEVICE_ERROR_CONNECTION_FAILED, _T("Connection failed")
};

static const TYPE_STRING_ENTRY 
VetoTypeStable[] = {
	PNP_VetoTypeUnknown, _T("Unknown"),
	PNP_VetoLegacyDevice, _T("Legacy Device"),
	PNP_VetoPendingClose, _T("Pending Close"),
	PNP_VetoWindowsApp, _T("Windows Application"),
	PNP_VetoWindowsService, _T("Windows Service"),
	PNP_VetoOutstandingOpen, _T("Outstanding Open"),
	PNP_VetoDevice, _T("Device"),
	PNP_VetoDriver, _T("Driver"),
	PNP_VetoIllegalDeviceRequest, _T("Illegal Device Request"),
	PNP_VetoInsufficientPower, _T("Insufficient Power"),
	PNP_VetoNonDisableable, _T("Non Disableable"),
	PNP_VetoLegacyDriver, _T("Legacy Driver"),
	PNP_VetoInsufficientRights, _T("Insufficient Rights")
};

LPTSTR
string_table_lookup(
	const TYPE_STRING_ENTRY* table,
	UINT tablesize,
	UINT value,
	LPTSTR buf,
	UINT buflen)
{
	UINT i;
	int n;
	for (i = 0; i < tablesize; ++i)
	{
		if (table[i].Value == value)
		{
			if (IS_INTRESOURCE(table[i].DisplayName))
			{
				n = LoadString(
					AppResourceInstance, 
					(UINT)(UINT_PTR)table[i].DisplayName,
					buf,
					buflen);
				if (n > 0)
				{
					return buf;
				}
			}
			else
			{
				StringCchCopy(buf, buflen, table[i].DisplayName);
				return buf;
			}
		}
	}
	StringCchPrintf(buf, buflen, _T("(0x%X)"), value);
	return buf;
}

LPTSTR
ndas_device_status_to_string(NDAS_DEVICE_STATUS status, LPTSTR buf, DWORD buflen)
{
	return string_table_lookup(
		NdasDeviceStatusStable,
		RTL_NUMBER_OF(NdasDeviceStatusStable),
		status, buf, buflen);
}

LPTSTR ndas_logicaldevice_status_to_string(NDAS_LOGICALDEVICE_STATUS status, LPTSTR buf, DWORD buflen)
{
	return string_table_lookup(
		NdasLogicalDeviceStatusStable,
		RTL_NUMBER_OF(NdasLogicalDeviceStatusStable),
		status, buf, buflen);
}

LPTSTR ndas_unitdevice_type_to_string(NDAS_UNITDEVICE_TYPE type, LPTSTR buf, DWORD buflen)
{
	return string_table_lookup(
		NdasUnitDeviceTypeStable,
		RTL_NUMBER_OF(NdasUnitDeviceTypeStable),
		type, buf, buflen);
}

LPTSTR ndas_logicaldevice_type_to_string(NDAS_LOGICALDEVICE_TYPE type,	LPTSTR buf, DWORD buflen)
{
	return string_table_lookup(
		NdasLogicalDeviceTypeStable,
		RTL_NUMBER_OF(NdasLogicalDeviceTypeStable),
		type, buf, buflen);
}

LPTSTR ndas_logicaldevice_error_to_string(NDAS_LOGICALDEVICE_ERROR err, LPTSTR buf, DWORD buflen)
{
	return string_table_lookup(
		NdasLogicalDeviceErrorStable,
		RTL_NUMBER_OF(NdasLogicalDeviceErrorStable),
		err, buf, buflen);
}

LPTSTR ndas_device_error_to_string(NDAS_DEVICE_ERROR err, LPTSTR buf, DWORD buflen)
{
	return string_table_lookup(
		NdasDeviceErrorStable,
		RTL_NUMBER_OF(NdasDeviceErrorStable),
		err, buf, buflen);
}

LPTSTR veto_type_string(PNP_VETO_TYPE type, LPTSTR buf, UINT buflen)
{
	return string_table_lookup(
		VetoTypeStable,
		RTL_NUMBER_OF(VetoTypeStable),
		type, buf, buflen);
}

HRESULT
guid_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCGUID lpGuid)
{
	HRESULT hr = StringCchPrintf(
		lpBuffer,
		cchBuffer,
		_T("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"),
		lpGuid->Data1,
		lpGuid->Data2,
		lpGuid->Data3,
		lpGuid->Data4[0], lpGuid->Data4[1],
		lpGuid->Data4[2], lpGuid->Data4[3],
		lpGuid->Data4[4], lpGuid->Data4[5],
		lpGuid->Data4[6], lpGuid->Data4[7]);
	return hr;
}

HRESULT
blocksize_to_string(LPTSTR lpBuffer, DWORD cchBuffer, UINT64 dwBlocks)
{
	static TCHAR* szSuffixes[] = {
		_T("KB"), _T("MB"), _T("GB"),
		_T("TB"), _T("PB"), _T("EB"),
		NULL };

	HRESULT hr;
	UINT64 dwKB = dwBlocks / 2; // 1 BLOCK = 512 Bytes = 1/2 KB
	UINT64 dwBase = dwKB;
	DWORD dwSub = 0;
	TCHAR ** ppszSuffix = szSuffixes; // KB

	while (dwBase > 1024 && NULL != *(ppszSuffix + 1)) {
		//
		// e.g. 1536 bytes
		// 1536 bytes % 1024 = 512 Bytes
		// 512 bytes * 1000 / 1024 = 500
		// 500 / 100 = 5 -> 0.5
		dwSub = MulDiv((DWORD)(dwBase % 1024), 1000, 1024) / 100;
		dwBase = dwBase / 1024;
		++ppszSuffix;
	}

	if (dwSub == 0) {
		hr = StringCchPrintf(
			lpBuffer,
			cchBuffer,
			_T("%d %s"),
			(UINT32)dwBase,
			*ppszSuffix);
		_ASSERTE(SUCCEEDED(hr));
	} else {
		hr = StringCchPrintf(
			lpBuffer,
			cchBuffer,
			_T("%d.%d %s"),
			(UINT32)dwBase,
			dwSub,
			*ppszSuffix);
		_ASSERTE(SUCCEEDED(hr));
	}

	return hr;
}

HRESULT
ndas_device_id_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCTSTR szDeviceID)
{
	HRESULT hr = StringCchPrintf(
		lpBuffer,
		cchBuffer,
		_T("%C%C%C%C%C-%C%C%C%C%C-%C%C%C%C%C-*****"),
		szDeviceID[0], szDeviceID[1], szDeviceID[2], szDeviceID[3], szDeviceID[4], 
		szDeviceID[5], szDeviceID[6], szDeviceID[7], szDeviceID[8],	szDeviceID[9], 
		szDeviceID[10], szDeviceID[11], szDeviceID[12], szDeviceID[13], szDeviceID[14]);
	return hr;
}
