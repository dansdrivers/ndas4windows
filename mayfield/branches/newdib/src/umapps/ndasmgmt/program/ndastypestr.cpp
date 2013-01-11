#include "stdafx.h"
#include "ndastypestr.h"
#include "resource.h"

template <typename T>
struct ResIDTable 
{
	T Value;
	UINT nResID;
};

template <typename T>
BOOL 
pLoadFromTable(
	CString& str, 
	SIZE_T nEntries, 
	const ResIDTable<T>* pTable, 
	T value)
{
	for (SIZE_T i = 0; i < nEntries; ++i) {
		if (pTable[i].Value == value) {
			return str.LoadString(pTable[i].nResID);
		}
	}
	return FALSE;
}

CString& 
pCapacityString(
	IN OUT CString& str, 
	IN DWORD lowPart, 
	IN DWORD highPart)
{
	//
	// Sector Size = 512 Bytes = 0x200 Bytes
	//
	// 1024 = 0x400
	//
	// sectors = high * 0x1,0000,00000 + low
	// bytes = high * 0x1,0000,0000 + low * 0x0200
	//       = high * 0x200,0000,0000 + low * 0x200
	// kilobytes = high * 0x8000,0000 + low / 0x2
	// megabytes = high * 0x20,0000 + low / 0x800
	// gigabytes = high * 0x800 + low / 0x20,0000
	// terabytes = high * 0x2 + low / 0x8000,0000

	BOOL fSuccess;
	DWORD dwMB = highPart * 0x200000 + lowPart / 0x0002;

	if (dwMB > 1026) 
	{
		DWORD dwGB = highPart * 0x800 + lowPart / 0x200000;
		DWORD dwFrac = (dwMB % 1024) * 10 / 1024;
		fSuccess = str.Format(_T("%d.%01d GB"), dwGB, dwFrac);
	} 
	else 
	{
		fSuccess = str.Format(_T("%d MB"), dwMB);
	}
	ATLASSERT(fSuccess);
	return str;
}

CString& 
pCreateDelimitedDeviceId(
	IN OUT CString& strBuffer, 
	IN CONST CString& strDevId,
	IN TCHAR chPassword)
{
	BOOL fSuccess = strBuffer.Format(
		_T("%s-%s-%s-%c%c%c%c%c"), 
		strDevId.Mid(0, 5),
		strDevId.Mid(5, 5),
		strDevId.Mid(10, 5),
		chPassword, chPassword, chPassword, chPassword, chPassword);
		// strDevId.Mid(15, 5)
	ATLASSERT(fSuccess);
	return strBuffer;
}

CString& 
pHWVersionString(
	IN OUT CString& strBuffer, 
	IN DWORD dwHWVersion)
{
	switch (dwHWVersion) 
	{
	case 0: strBuffer = _T("1.0"); break;
	case 1: strBuffer = _T("1.1"); break;
	case 2: strBuffer =  _T("2.0"); break;
	default:
		{
			BOOL fSuccess = strBuffer.Format(
				_T("Unsupported Version %d"), dwHWVersion);
			_ASSERTE(fSuccess);
		}
	}

	return strBuffer;
}

CString& 
pUnitDeviceMediaTypeString(
	IN OUT CString& strType, 
	IN DWORD type)
{
	const ResIDTable<NDAS_LOGICALDEVICE_TYPE> table[] = 
	{
		{NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE, IDS_UNITDEV_MEDIA_TYPE_DISK},
		{NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE, IDS_UNITDEV_MEDIA_TYPE_CDROM},
		{NDAS_UNITDEVICE_MEDIA_TYPE_COMPACT_BLOCK_DEVICE, IDS_UNITDEV_MEDIA_TYPE_COMPACT_FLASH},
		{NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE, IDS_UNITDEV_MEDIA_TYPE_OPMEM}
	};

	BOOL fSuccess = pLoadFromTable<NDAS_LOGICALDEVICE_TYPE>(
		strType, RTL_NUMBER_OF(table), table, type);
	if (fSuccess) { return strType; }

	// Unknown Type (%1!08X!)
	fSuccess = strType.FormatMessage(IDS_UNITDEV_MEDIA_TYPE_UNKNOWN_FMT, type);
	ATLASSERT(fSuccess);

	return strType;
}

CString&
pLogicalDeviceStatusString(
	IN OUT CString& strStatus,
	IN NDAS_LOGICALDEVICE_STATUS status,
	IN ACCESS_MASK access)
{
	const ResIDTable<NDAS_LOGICALDEVICE_STATUS> table[] = 
	{
		{NDAS_LOGICALDEVICE_STATUS_UNMOUNTED, IDS_LOGDEV_STATUS_UNMOUNTED},
		{NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING, IDS_LOGDEV_STATUS_MOUNT_PENDING},
		{NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING, IDS_LOGDEV_STATUS_UNMOUNT_PENDING}
		// {NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE, IDS_LOGDEV_STATUS_NOT_MOUNTABLE}
	};

	BOOL fSuccess;

	if (status == NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
	{
		if (GENERIC_WRITE & access) 
		{
			fSuccess = strStatus.LoadString(IDS_LOGDEV_STATUS_MOUNTED_RW);
		} 
		else 
		{
			fSuccess = strStatus.LoadString(IDS_LOGDEV_STATUS_MOUNTED_RO);
		}
		ATLASSERT(fSuccess);
		return strStatus;
	}

	fSuccess = pLoadFromTable<NDAS_LOGICALDEVICE_STATUS>(
		strStatus, RTL_NUMBER_OF(table), table, status);
	if (fSuccess) 
	{ 
		return strStatus; 
	}

	// Unknown Status (0x%1!08X!)
	fSuccess = strStatus.FormatMessage(IDS_LOGDEV_STATUS_UNKNOWN_FMT, status);
	ATLASSERT(fSuccess);

	return strStatus;
}

CString&
pLogicalDeviceTypeString(
	IN OUT CString& strType, 
	IN NDAS_LOGICALDEVICE_TYPE type)
{
	const ResIDTable<NDAS_LOGICALDEVICE_TYPE> ldtable[] = 
	{
		{NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE, IDS_LOGDEV_TYPE_SINGLE_DISK},
		{NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED, IDS_LOGDEV_TYPE_AGGREGATED_DISK},
		{NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED, IDS_LOGDEV_TYPE_MIRRORED_DISK},
		{NDAS_LOGICALDEVICE_TYPE_DISK_RAID0, IDS_LOGDEV_TYPE_DISK_RAID0},
		{NDAS_LOGICALDEVICE_TYPE_DISK_RAID1, IDS_LOGDEV_TYPE_DISK_RAID1},
		{NDAS_LOGICALDEVICE_TYPE_DISK_RAID4, IDS_LOGDEV_TYPE_DISK_RAID4},
		{NDAS_LOGICALDEVICE_TYPE_DVD, IDS_LOGDEV_TYPE_DVD_DRIVE},
		{NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD, IDS_LOGDEV_TYPE_VIRTUAL_DVD_DRIVE},
		{NDAS_LOGICALDEVICE_TYPE_MO, IDS_LOGDEV_TYPE_MO_DRIVE},
		{NDAS_LOGICALDEVICE_TYPE_MO, IDS_LOGDEV_TYPE_MO_DRIVE},
		{NDAS_LOGICALDEVICE_TYPE_FLASHCARD, IDS_LOGDEV_TYPE_CF_DRIVE}
	};

	BOOL fSuccess = pLoadFromTable<NDAS_LOGICALDEVICE_TYPE>(
		strType, RTL_NUMBER_OF(ldtable), ldtable, type);
	if (fSuccess) 
	{ 
		return strType; 
	}

	fSuccess = strType.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, type);
	ATLASSERT(fSuccess);

	return strType;
}

CString&
pDeviceStatusString(
	IN OUT CString& strStatus,
	IN NDAS_DEVICE_STATUS status,
	IN NDAS_DEVICE_ERROR lastError)
{
	const ResIDTable<NDAS_DEVICE_STATUS> table[] = 
	{
		{NDAS_DEVICE_STATUS_CONNECTED, IDS_NDAS_DEVICE_STATUS_CONNECTED},
		{NDAS_DEVICE_STATUS_DISABLED, IDS_NDAS_DEVICE_STATUS_DISABLED}
	};

	BOOL fSuccess;

	if (status == NDAS_DEVICE_STATUS_DISCONNECTED) 
	{

		CString strBuffer;
		fSuccess = strBuffer.LoadString(IDS_NDAS_DEVICE_STATUS_DISCONNECTED);
		ATLASSERT(fSuccess);

		if (0 != lastError) 
		{
			fSuccess = strStatus.Format(_T("%s - Error %08X"), strBuffer, lastError);
			ATLASSERT(fSuccess);
		}
		else 
		{
			fSuccess = strStatus.Format(_T("%s"), strBuffer);
			ATLASSERT(fSuccess);
		}

		return strStatus;
	}

	fSuccess = pLoadFromTable(
		strStatus, RTL_NUMBER_OF(table), table, status);
	if (fSuccess) 
	{ 
		return strStatus; 
	}

	fSuccess = strStatus.LoadString(IDS_NDAS_DEVICE_STATUS_UNKNOWN);
	ATLASSERT(fSuccess);

	return strStatus;
}

CString&
pUnitDeviceTypeString(
	IN OUT CString& strType,
	IN NDAS_UNITDEVICE_TYPE type,
	IN NDAS_UNITDEVICE_SUBTYPE subType)
{
	const ResIDTable<NDAS_UNITDEVICE_DISK_TYPE> diskTypeTable[] = 
	{
		{ NDAS_UNITDEVICE_DISK_TYPE_SINGLE,	IDS_UNITDEV_TYPE_DISK_SINGLE },
		{ NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED,	IDS_UNITDEV_TYPE_DISK_AGGREGATED},
		{ NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER,	IDS_UNITDEV_TYPE_DISK_MIRROR_MASTER},
		{ NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE,	IDS_UNITDEV_TYPE_DISK_MIRROR_SLAVE},
		{ NDAS_UNITDEVICE_DISK_TYPE_RAID0,	IDS_UNITDEV_TYPE_DISK_RAID0},
		{ NDAS_UNITDEVICE_DISK_TYPE_RAID1,	IDS_UNITDEV_TYPE_DISK_RAID1},
		{ NDAS_UNITDEVICE_DISK_TYPE_RAID4,	IDS_UNITDEV_TYPE_DISK_RAID4}
	};

	BOOL fSuccess;

	if (NDAS_UNITDEVICE_TYPE_DISK == type) 
	{

		fSuccess = pLoadFromTable<NDAS_UNITDEVICE_DISK_TYPE>(
			strType, 
			RTL_NUMBER_OF(diskTypeTable), 
			diskTypeTable, 
			subType.DiskDeviceType);
		if (fSuccess) 
		{ 
			return strType; 
		}

		fSuccess = strType.FormatMessage(
			IDS_UNITDEV_TYPE_DISK_UNKNOWN_FMT, 
			subType.DiskDeviceType);
		ATLASSERT(fSuccess);

		return strType;

	} 
	else if (NDAS_UNITDEVICE_TYPE_CDROM == type) 
	{
		
		fSuccess = strType.LoadString(IDS_UNITDEV_TYPE_CDROM);
		ATLASSERT(fSuccess);

		return strType;
	}

	fSuccess = strType.FormatMessage(IDS_UNITDEV_TYPE_UNKNOWN_FMT, type);
	ATLASSERT(fSuccess);

	return strType;
}

CString&
pUnitDeviceStatusString(
	IN OUT CString& strStatus, 
	IN NDAS_UNITDEVICE_STATUS status)
{
	const ResIDTable<NDAS_UNITDEVICE_STATUS> table[] = 
	{
		{NDAS_UNITDEVICE_STATUS_MOUNTED,IDS_UNITDEVICE_STATUS_MOUNTED},
		{NDAS_UNITDEVICE_STATUS_NOT_MOUNTED,IDS_UNITDEVICE_STATUS_NOT_MOUNTED}
	};

	BOOL fSuccess = pLoadFromTable<NDAS_UNITDEVICE_STATUS>(
		strStatus, RTL_NUMBER_OF(table), table, status);
	if (fSuccess) 
	{ 
		return strStatus; 
	}

	// Unknown (%1!04X!)
	fSuccess = strStatus.FormatMessage(IDS_UNITDEVICE_STATUS_UNKNOWN_FMT, status);
	_ASSERTE(fSuccess);
	return strStatus;
}

CString&
pLPXAddrString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_LPX_ADDR* pAddr)
{
	strBuffer.Format(_T("%02X-%02X-%02X-%02X-%02X-%02X"),
		pAddr->Bytes[0], pAddr->Bytes[1], pAddr->Bytes[2],
		pAddr->Bytes[3], pAddr->Bytes[4], pAddr->Bytes[5]);
	return strBuffer;
}

CString& 
pIPV4AddrString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_IPV4_ADDR* pAddr)
{
	strBuffer.Format(_T("%d.%d.%d.%d"), 
		pAddr->Bytes[0],
		pAddr->Bytes[1],
		pAddr->Bytes[2],
		pAddr->Bytes[3]);
	return strBuffer;
}

CString& 
pAddressString(
	CString& strBuffer,
	CONST NDAS_HOST_INFO_LPX_ADDR_ARRAY* pLPXAddrs,
	CONST NDAS_HOST_INFO_IPV4_ADDR_ARRAY* pIPV4Addrs)
{
	DWORD nAddrs = 0;
	CString strAddr;
	strBuffer.Empty();

	for (DWORD i = 0; i < pIPV4Addrs->AddressCount; ++i) 
	{
		if (nAddrs > 0) 
		{ 
			strBuffer += _T(", "); 
		}
		strBuffer += pIPV4AddrString(strAddr,&pIPV4Addrs->Addresses[i]);
		++nAddrs;
	}

	for (DWORD i = 0; i < pLPXAddrs->AddressCount; ++i) 
	{
		if (nAddrs > 0) 
		{ 
			strBuffer += _T(", "); 
		}
		strBuffer += pLPXAddrString(strAddr,&pLPXAddrs->Addresses[i]);
		++nAddrs;
	}
	return strBuffer;
}
