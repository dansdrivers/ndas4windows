#include "stdafx.h"
#include "resource.h"

#include "nbmainfrm.h"
#include "nbdev.h"
#include <ndas/ndasid.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasop.h>

#define NDASBIND_UNIT_DEVICE_RMD_INVALID	0x00000001
#define NDASBIND_UNIT_DEVICE_RMD_FAULT		0x00000002
#define NDASBIND_UNIT_DEVICE_RMD_SPARE		0x00000004

#define NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED	0x00000001
#define NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY	0x00000002
#define NDASBIND_UNIT_DEVICE_STATUS_MOUNTED			0x00000004

#define NDASBIND_LOGICAL_DEVICE_RMD_INVALID	0x00000001
#define NDASBIND_LOGICAL_DEVICE_RMD_FAULT	0x00000002
#define NDASBIND_LOGICAL_DEVICE_RMD_MISSING	0x00000004
#define NDASBIND_LOGICAL_DEVICE_RMD_BROKEN	0x00000008

#define NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED	0x00000001
#define NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY	0x00000002
#define NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED		0x00000004

CNBNdasDevice::CNBNdasDevice(PNDASUSER_DEVICE_ENUM_ENTRY pBaseInfo, NDAS_DEVICE_STATUS status)
{
	ATLASSERT(pBaseInfo);

	CopyMemory(&m_BaseInfo, pBaseInfo, sizeof(NDASUSER_DEVICE_ENUM_ENTRY));
	m_status = status;

	ATLVERIFY(NdasIdStringToDevice(pBaseInfo->szDeviceStringId, &m_DeviceId));

	ATLTRACE(_T("new CNBNdasDevice(%p) : Name %s, ID %s, Slot %d, Access %08x\n"),
		this, pBaseInfo->szDeviceName, pBaseInfo->szDeviceStringId, pBaseInfo->SlotNo, pBaseInfo->GrantedAccess);
}

CNBNdasDevice::~CNBNdasDevice()
{
	ClearUnitDevices();
}

void CNBNdasDevice::ClearUnitDevices()
{
	for(NBUnitDevicePtrMap::iterator itUnitDevice = m_mapUnitDevices.begin();
		itUnitDevice != m_mapUnitDevices.end();
		itUnitDevice++)
	{
		if(itUnitDevice->second)
		{
			delete itUnitDevice->second;
			itUnitDevice->second = NULL;
		}
	}
}

BOOL CNBNdasDevice::UnitDeviceAdd(PNDASUSER_UNITDEVICE_ENUM_ENTRY pBaseInfo)
{
	if(!pBaseInfo)
		return FALSE;

	// already exist
	if(m_mapUnitDevices.count(pBaseInfo->UnitNo))
	{
		delete m_mapUnitDevices[pBaseInfo->UnitNo];
		m_mapUnitDevices[pBaseInfo->UnitNo] = NULL;
	}

	CNBUnitDevice *pUnitDevice = new CNBUnitDevice(pBaseInfo);
	if(!pUnitDevice)
		return FALSE;

	m_mapUnitDevices[pBaseInfo->UnitNo] = pUnitDevice;
	pUnitDevice->m_pDevice = this;

	return TRUE;
}

BOOL CNBNdasDevice::UnitDevicesInitialize()
{
	for(NBUnitDevicePtrMap::iterator itUnitDevice = m_mapUnitDevices.begin();
		itUnitDevice != m_mapUnitDevices.end(); itUnitDevice++)
	{
		if(!itUnitDevice->second->Initialize())
		{
			m_status = NDAS_DEVICE_STATUS_DISCONNECTED;
			break;
		}
	}

	if(0 == m_mapUnitDevices.size() || NDAS_DEVICE_STATUS_CONNECTED != m_status)
	{
		m_status = NDAS_DEVICE_STATUS_DISCONNECTED;

		ClearUnitDevices();

		NDASUSER_UNITDEVICE_ENUM_ENTRY BaseInfo;
		BaseInfo.UnitDeviceType = NDAS_UNITDEVICE_TYPE_UNKNOWN;
		BaseInfo.UnitNo = 0;

		CNBUnitDevice *pUnitDevice = new CNBUnitDevice(&BaseInfo);
		pUnitDevice->m_pDevice = this;
		m_mapUnitDevices[0] = pUnitDevice;
		pUnitDevice->Initialize();
	}

	return TRUE;
}

WTL::CString CNBNdasDevice::GetName()
{
	WTL::CString strText;

	strText.Format(_T("%s"), m_BaseInfo.szDeviceName);

	return strText;
}

WTL::CString CNBDevice::GetCapacityString(UINT64 ui64capacity)
{
	WTL::CString strText;
	UINT64 ui64capacityMB = ui64capacity / (1024 * 1024);
	UINT32 uicapacityGB = (UINT32)(ui64capacityMB / 1000);
	UINT32 uicapacityMB = (UINT32)(ui64capacityMB % 1000);

	if(0 == ui64capacityMB)
	{
		strText = _T("");
	}
	else
	{
		strText.FormatMessage(IDS_DISKPROPERTYPAGE_SIZE_IN_GB, uicapacityGB, uicapacityMB);
	}

	return strText;
}

WTL::CString CNBDevice::GetCapacityString()
{
	return GetCapacityString(GetCapacityInByte());
}

UINT CNBDevice::FindIconIndex(UINT idicon, UINT *anIconIDs, int nCount, int iDefault)
{
	for(UINT32 i = 0; i < nCount; i++)
	{
		if(idicon == anIconIDs[i])
			return i;
	}
	
	return (iDefault < nCount) ? iDefault : 0;
}


CNBUnitDevice::CNBUnitDevice(PNDASUSER_UNITDEVICE_ENUM_ENTRY pBaseInfo)
{
	ATLASSERT(pBaseInfo);

	m_pDevice = NULL;
	m_pLogicalDevice = NULL;

	CopyMemory(&m_BaseInfo, pBaseInfo, sizeof(NDASUSER_UNITDEVICE_ENUM_ENTRY));

	ATLTRACE(_T("new CNBUnitDevice(%p) : UnitNo %d, Type %d\n"), this, pBaseInfo->UnitNo, pBaseInfo->UnitDeviceType);
}

BOOL CNBUnitDevice::InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess)
{
	if(!m_pDevice)
		return FALSE;

	// check granted access
	if(!(GENERIC_WRITE & m_pDevice->m_BaseInfo.GrantedAccess) && bWriteAccess)
	{
		return FALSE;
	}

	ZeroMemory(ci, sizeof(NDASCOMM_CONNECTION_INFO));
	ci->address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_DEVICE;
	ci->login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci->UnitNo = m_BaseInfo.UnitNo;
	ci->bWriteAccess = bWriteAccess;
	ci->ui64OEMCode = NULL;
	ci->bSupervisor = FALSE;
	ci->protocol = NDASCOMM_TRANSPORT_LPX;
	
	CopyMemory(ci->AddressLPX, m_pDevice->m_DeviceId.Node, sizeof(ci->AddressLPX));

	return TRUE;
}

BOOL CNBUnitDevice::Initialize()
{
	BOOL bReturn = FALSE;
	NDASCOMM_CONNECTION_INFO ci;
	HNDAS hNDAS = NULL;
	UINT32 nDIBSize = sizeof(m_DIB);

	if(NDAS_UNITDEVICE_TYPE_UNKNOWN == m_BaseInfo.UnitDeviceType)
	{
		m_DIB.iMediaType = NMT_INVALID;
		m_DIB.nDiskCount = 1;
	}

	if(!InitConnectionInfo(&ci, FALSE))
		goto out;
	
	if(!(hNDAS = NdasCommConnect(&ci, 0, NULL)))
		goto out;

	if(!NdasOpReadDIB(hNDAS, &m_DIB, &nDIBSize))
		goto out;

	if (NMT_RAID1 == m_DIB.iMediaType ||
		NMT_RAID4 == m_DIB.iMediaType)
	{
		if(!NdasOpRMDRead(&ci, &m_RMD))
			goto out;

		for(UINT32 i = 0; i < m_DIB.nDiskCount + m_DIB.nSpareCount; i++)
		{
			if(m_DIB.iSequence == m_RMD.UnitMetaData[i].iUnitDeviceIdx)
			{
				m_cSequenceInRMD = i;
				break;
			}
		}
	}

	bReturn = TRUE;
out:
	if(hNDAS && !NdasCommDisconnect(hNDAS))
		return FALSE;

	return bReturn;
}
/*
switch(m_DIB.iMediaType)
{
case NMT_INVALID:
case NMT_SINGLE:
case NMT_MIRROR:
case NMT_SAFE_RAID1:
case NMT_AGGREGATE:
case NMT_RAID0:
case NMT_RAID1:
case NMT_RAID4:
case NMT_AOD:
case NMT_VDVD:
case NMT_CDROM:
case NMT_OPMEM:
case NMT_FLASH:
default:
}
*/

BOOL CNBUnitDevice::HixChangeNotify(LPCGUID guid)
{
	CNdasHIXChangeNotify HixChangeNotify(guid);

	BOOL bResults = HixChangeNotify.Initialize();
	if(!bResults)
		return FALSE;

	NDAS_UNITDEVICE_ID unitDeviceId;
	CopyMemory(unitDeviceId.DeviceId.Node, m_pDevice->m_DeviceId.Node, 
		sizeof(unitDeviceId.DeviceId.Node));
	unitDeviceId.UnitNo = m_BaseInfo.UnitNo;

	return HixChangeNotify.Notify(unitDeviceId);
}

BOOL CNBUnitDevice::IsFault()
{
	if(!IsGroup())
		return FALSE;

	return NDAS_UNIT_META_BIND_STATUS_FAULT & m_RMD.UnitMetaData[m_cSequenceInRMD].UnitDeviceStatus;
}

BOOL CNBUnitDevice::IsSpare()
{
	if(!IsGroup())
		return FALSE;

	return m_cSequenceInRMD >= m_DIB.nDiskCount;
}

BOOL CNBUnitDevice::GetCommandAbility(int nID)
{
	switch(nID)
	{
	case IDM_TOOL_BIND:
		return TRUE;
	case IDM_TOOL_UNBIND:
		return FALSE;
	case IDM_TOOL_ADDMIRROR: // Should be single logical device
		return FALSE;
	case IDM_TOOL_MIGRATE: // Should be single logical device
		return FALSE;
	case IDM_TOOL_REPLACE_DEVICE:
		return FALSE;
//		return (IsFaultTolerant() && m_pLogicalDevice->IsOperatableAll() && (IsSpare() || NULL == GetFaultTolerance()));
	case IDM_TOOL_REPLACE_UNIT_DEVICE:
		return FALSE;
//		return (IsFaultTolerant() && IsOperatable() && (IsSpare() || NULL == GetFaultTolerance()));
	case IDM_TOOL_SINGLE:
		return (IsHDD() && IsOperatable());
	case IDM_TOOL_SPAREADD:
		return FALSE;
	case IDM_TOOL_SPAREREMOVE:
		return (IsFaultTolerant() && IsSpare() && m_pLogicalDevice->IsOperatableAll());
	default:
		return TRUE;
	}
}


UINT32 CNBUnitDevice::GetType()
{
	if(NDAS_DEVICE_STATUS_CONNECTED != m_pDevice->m_status)
		return NMT_INVALID;

	return m_DIB.iMediaType;
}

WTL::CString CNBUnitDevice::GetTypeString()
{
	// AING_TO_DO : Set proper text
	WTL::CString strText;
	switch(GetType())
	{
	case NMT_INVALID:
	case NMT_SINGLE: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_MIRROR: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_SAFE_RAID1: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_AGGREGATE: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_RAID0: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_RAID1: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_RAID4: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_AOD: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_VDVD: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_CDROM: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_OPMEM: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_FLASH: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	default:
		break;
	}

	return strText;
}

WTL::CString CNBUnitDevice::GetName()
{
	WTL::CString strText;

	if(1 != m_pDevice->UnitDevicesCount())
	{
		strText.Format(_T("%s:%d"), m_pDevice->m_BaseInfo.szDeviceName, m_BaseInfo.UnitNo);
	}
	else
	{
		strText.Format(_T("%s"), m_pDevice->m_BaseInfo.szDeviceName);
	}

	return strText;
}

UINT CNBUnitDevice::GetIconIndex(UINT *anIconIDs, int nCount)
{
	return FindIconIndex(IDI_BASIC, anIconIDs, nCount);
}

UINT CNBUnitDevice::GetSelectIconIndex(UINT *anIconIDs, int nCount)
{
	return GetIconIndex(anIconIDs, nCount);
}

BOOL CNBUnitDevice::IsGroup()
{
	switch(m_DIB.iMediaType)
	{
	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1:
	case NMT_RAID4:
		return TRUE;
	case NMT_INVALID:
	case NMT_SINGLE:
	case NMT_SAFE_RAID1:
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	default:
		return FALSE;
	}
}

BOOL CNBUnitDevice::IsHDD()
{
	switch(m_DIB.iMediaType)
	{
	case NMT_SINGLE:
	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1:
	case NMT_RAID4:
	case NMT_AOD:
	case NMT_VDVD:
		return TRUE;
	case NMT_INVALID:
	case NMT_SAFE_RAID1:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	default:
		return FALSE;
	}
	return TRUE;
}

WTL::CString CNBUnitDevice::GetIDString()
{
	WTL::CString strID = m_pDevice->m_BaseInfo.szDeviceStringId;
	WTL::CString strText;
	strID.Remove(_T('-'));

	strText += 
		strID.Mid(0, 5) + _T("-") +
		strID.Mid(5, 5) + _T("-") +
		strID.Mid(10, 5) + _T("-") + _T("*****");
	//		strID.Mid(0, 5) + _T("-") +

	return strText;
}

UINT64 CNBUnitDevice::GetCapacityInByte()
{
	if(!IsHDD())
		return 0;

	return m_DIB.sizeUserSpace * SECTOR_SIZE;
}

DWORD CNBUnitDevice::GetStatus()
{
	DWORD status = NULL;
	// no write key
	if(!(GENERIC_WRITE & m_pDevice->m_BaseInfo.GrantedAccess))
	{
		status |= NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY;
	}

	// disconnected
	if(NDAS_DEVICE_STATUS_CONNECTED != m_pDevice->m_status)
	{
		status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;

		// can not process anymore
		return status;
	}

	// in use
	NDASCOMM_CONNECTION_INFO ci;
	if(!InitConnectionInfo(&ci, FALSE))
		return FALSE;

	ci.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
	NDASCOMM_UNIT_DEVICE_STAT UnitDynStat;

	if (NdasCommGetUnitDeviceStat(&ci, &UnitDynStat, 0, NULL) &&
		(
			UnitDynStat.NRRWHost || 
			(
				UnitDynStat.NRROHost && 
				NDAS_HOST_COUNT_UNKNOWN != UnitDynStat.NRROHost
			)
		)
	)
	{
		status |= NDASBIND_UNIT_DEVICE_STATUS_MOUNTED;
	}

	return status;
}

WTL::CString CNBUnitDevice::GetStatusString()
{
	WTL::CString strText;

	DWORD status = GetStatus();

	if(NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED & status)
	{
		strText.LoadString(IDS_STATUS_NOT_CONNECTED);
	}
	else if(NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & status)
	{
		strText.LoadString(IDS_STATUS_IN_USE);
	}
	else if(NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY & status)
	{
		strText.LoadString(IDS_STATUS_READ_ONLY);
	}
	else
	{
		strText.LoadString(IDS_STATUS_FINE);
	}

	return strText;
}

BOOL CNBUnitDevice::IsFaultTolerant()
{
	switch(GetType())
	{
	case NMT_RAID1:
	case NMT_RAID4:
		return TRUE;
	default:
		return FALSE;
	}
}

BOOL CNBUnitDevice::IsSibling(CNBUnitDevice *pUnitDevice)
{
	if(!pUnitDevice)
		return FALSE;

	if(!pUnitDevice->IsGroup())
		return FALSE;

	if (m_DIB.nDiskCount == pUnitDevice->m_DIB.nDiskCount &&
		m_DIB.nSpareCount == pUnitDevice->m_DIB.nSpareCount &&
		!memcmp(m_DIB.UnitDisks, pUnitDevice->m_DIB.UnitDisks, 
			(m_DIB.nDiskCount + pUnitDevice->m_DIB.nSpareCount) * sizeof(UNIT_DISK_LOCATION)))
		return TRUE;
	
	return FALSE;
}


UINT32 CNBUnitDevice::GetSequence()
{
	ATLASSERT(m_pLogicalDevice);

	return (IsGroup()) ? m_DIB.iSequence : 0;
}

DWORD CNBUnitDevice::GetFaultTolerance()
{
	DWORD status = NULL;
	if(!IsFaultTolerant())
		return NDASBIND_UNIT_DEVICE_RMD_INVALID;

	if (NDAS_UNIT_META_BIND_STATUS_SPARE & m_RMD.UnitMetaData[m_cSequenceInRMD].UnitDeviceStatus)
		status |= NDASBIND_UNIT_DEVICE_RMD_SPARE;

	if (NDAS_UNIT_META_BIND_STATUS_FAULT & m_RMD.UnitMetaData[m_cSequenceInRMD].UnitDeviceStatus)
		status |= NDASBIND_UNIT_DEVICE_RMD_FAULT;

	return status;
}

WTL::CString CNBUnitDevice::GetFaultToleranceString()
{
	WTL::CString strText;
	DWORD status = GetFaultTolerance();

	if(NDASBIND_UNIT_DEVICE_RMD_INVALID & status)
	{
	}
	else if(NDASBIND_UNIT_DEVICE_RMD_FAULT & status)
	{
		strText.LoadString(IDS_FT_FAULT_CHILD);
	}
	else if(NDASBIND_UNIT_DEVICE_RMD_SPARE & status)
	{
		strText.LoadString(IDS_FT_FAULT_SPARE);
	}

	return strText;
}

DWORD CNBUnitDevice::GetAccessMask()
{
	return m_pDevice->m_BaseInfo.GrantedAccess;
}

BOOL CNBUnitDevice::IsOperatable()
{
	if(!IsHDD())
		return FALSE;
	if((NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED |
		NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY |
		NDASBIND_UNIT_DEVICE_STATUS_MOUNTED) &
		GetStatus())
	{
		return FALSE;
	}

	return TRUE;
}

CNBLogicalDevice::CNBLogicalDevice()
{
}

BOOL CNBLogicalDevice::IsGroup()
{
	ATLASSERT(m_mapUnitDevices.size());

	return AnyUnitDevice()->IsGroup();
}

UINT32 CNBLogicalDevice::GetType()
{
	ATLASSERT(m_mapUnitDevices.size());

	return DIB()->iMediaType;
}

WTL::CString CNBLogicalDevice::GetTypeString()
{
	WTL::CString strText;

	switch(GetType())
	{
	case NMT_INVALID: strText = _T(""); break;
	case NMT_SINGLE: strText.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
	case NMT_AGGREGATE: strText.LoadString(IDS_LOGDEV_TYPE_AGGREGATED_DISK); break;
	case NMT_MIRROR: strText.LoadString(IDS_LOGDEV_TYPE_MIRRORED_DISK); break;
	case NMT_RAID0: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID0); break;
	case NMT_RAID1: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1); break;
	case NMT_RAID4: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4); break;
	case NMT_CDROM: strText.LoadString(IDS_LOGDEV_TYPE_DVD_DRIVE); break;
	case NMT_OPMEM: strText.LoadString(IDS_LOGDEV_TYPE_MO_DRIVE); break;
	case NMT_FLASH: strText.LoadString(IDS_LOGDEV_TYPE_CF_DRIVE); break;
	default:
		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, GetType());
	}

	return strText;
}

WTL::CString CNBLogicalDevice::GetName()
{
	ATLASSERT(m_mapUnitDevices.size());

	WTL::CString strText;

	if(0 == m_mapUnitDevices.size())
	{
		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, NMT_INVALID);
	}
	else if(IsGroup())
	{
		strText = GetTypeString();
	}
	else
	{
		ATLASSERT(1 == m_mapUnitDevices.size() && m_mapUnitDevices.count(0));

		strText = m_mapUnitDevices[0]->GetName();
	}

	return strText;
}

UINT CNBLogicalDevice::GetIconIndex(UINT *anIconIDs, int nCount)
{
	ATLASSERT(m_mapUnitDevices.size());
	UINT idicon;

	switch(GetType())
	{
	case NMT_INVALID:
		return FindIconIndex(IDI_FAIL, anIconIDs, nCount);
	case NMT_SINGLE:
	case NMT_AOD:
		return FindIconIndex(IDI_BASIC, anIconIDs, nCount);
	case NMT_MIRROR:
		return FindIconIndex(IDI_RAID0, anIconIDs, nCount);
	case NMT_AGGREGATE:
	case NMT_RAID0:
		return FindIconIndex(IDI_AGGR, anIconIDs, nCount);
	case NMT_RAID1:
		return FindIconIndex(IDI_RAID1, anIconIDs, nCount);
	case NMT_RAID4:
		return FindIconIndex(IDI_RAID4, anIconIDs, nCount);
	case NMT_VDVD:
		return FindIconIndex(IDI_VDVD, anIconIDs, nCount);
	case NMT_CDROM:
		return FindIconIndex(IDI_DVD, anIconIDs, nCount);
	case NMT_FLASH:
		return FindIconIndex(IDI_FLASH, anIconIDs, nCount);
	case NMT_OPMEM:
	case NMT_SAFE_RAID1:
	default:
		return FindIconIndex(IDI_BASIC, anIconIDs, nCount);
	}
}

UINT CNBLogicalDevice::GetSelectIconIndex(UINT *anIconIDs, int nCount)
{
	ATLASSERT(m_mapUnitDevices.size());

	return GetIconIndex(anIconIDs, nCount);
}

BOOL CNBLogicalDevice::IsMember(CNBUnitDevice *pUnitDevice)
{
	ATLASSERT(m_mapUnitDevices.size());
	
	ATLASSERT(pUnitDevice);
	if(!pUnitDevice)
		return FALSE;

	if (AnyUnitDevice()->IsSibling(pUnitDevice))
		return TRUE;

	return FALSE;
}

BOOL CNBLogicalDevice::UnitDeviceAdd(CNBUnitDevice *pUnitDevice)
{
	UINT32 iSequence;

	if(pUnitDevice->IsGroup())
	{
		if(m_mapUnitDevices.size() && !IsMember(pUnitDevice))
		{
			ATLASSERT(FALSE);
			return FALSE;
		}

		iSequence = pUnitDevice->m_DIB.iSequence;

	}
	else
	{
		iSequence = 0;
	}

	m_mapUnitDevices[iSequence] = pUnitDevice;
	ATLTRACE(_T("CNBLogicalDevice(%p).m_mapUnitDevices[%d] = (%p) : %s\n"),
		this, iSequence, pUnitDevice, pUnitDevice->GetName());

	pUnitDevice->m_pLogicalDevice = this;

	return TRUE;
}

CNBUnitDevice *CNBLogicalDevice::AnyUnitDevice()
{
	ATLASSERT(m_mapUnitDevices.size());

	if(!m_mapUnitDevices.size())
		return NULL;
	
	return m_mapUnitDevices.begin()->second;
}

UINT32 CNBLogicalDevice::DevicesTotal(BOOL bAliveOnly)
{
	ATLASSERT(m_mapUnitDevices.size());

	return (IsGroup()) ?
		DIB()->nDiskCount + DIB()->nSpareCount : 1;
}

UINT32 CNBLogicalDevice::DevicesInRaid(BOOL bAliveOnly)
{
	ATLASSERT(m_mapUnitDevices.size());

	if(!IsGroup())
		return 1;
	
	return DIB()->nDiskCount;
}

UINT32 CNBLogicalDevice::DevicesSpare(BOOL bAliveOnly)
{
	ATLASSERT(m_mapUnitDevices.size());

	if(!IsGroup())
		return 0;

	return DIB()->nSpareCount;
}

WTL::CString CNBLogicalDevice::GetIDString()
{
	WTL::CString strText;
	ATLASSERT(m_mapUnitDevices.size());

	if(IsGroup())
	{
		strText = _T("");
	}
	else
	{
		ATLASSERT(1 == m_mapUnitDevices.size() && m_mapUnitDevices.count(0));
		strText = AnyUnitDevice()->GetIDString();
	}

	return strText;
}

UINT64 CNBLogicalDevice::GetCapacityInByte()
{
	UINT64 size = 0;
	if(!IsGroup())
		return AnyUnitDevice()->GetCapacityInByte();

	switch(GetType())
	{
	case NMT_AGGREGATE:
		for(UINT32 i = 0; i < DevicesTotal(); i++)
		{
			if(m_mapUnitDevices.count(i))
				size += m_mapUnitDevices[i]->GetCapacityInByte();
		}
		break;
	case NMT_MIRROR:
	case NMT_RAID0:
	case NMT_RAID1:
		size = AnyUnitDevice()->GetCapacityInByte() * DevicesInRaid();
		break;
	case NMT_RAID4:
		size = AnyUnitDevice()->GetCapacityInByte() * (DevicesInRaid() -1);
		break;
	default:
		ATLASSERT(FALSE);
	}

	return size;
}

DWORD CNBLogicalDevice::GetStatus()
{
	DWORD status = NULL, status_unit;

	for(UINT32 i = 0; i < DevicesInRaid(); i++)
	{
		if(!m_mapUnitDevices.count(i))
		{
			status |= NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED;
			continue;
		}

		status_unit = m_mapUnitDevices[i]->GetStatus();

		if(NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED & status_unit)
		{
			status |= NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED;
			continue;
		}

		if(NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY & status_unit)
		{
			status |= NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY;
		}

		if(NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & status_unit)
		{
			status |= NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED;
		}
	}

	return status;
}

WTL::CString CNBLogicalDevice::GetStatusString()
{
	WTL::CString strText;
	DWORD status = GetStatus();

	if(NMT_INVALID == GetType())
	{
		strText = _T("");
	}
	else if(NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY & status)
	{
		strText.LoadString(IDS_STATUS_READ_ONLY);
	}
	else if(NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED & status)
	{
		strText.LoadString(IDS_STATUS_IN_USE);
	}
	else
	{
		strText.LoadString(IDS_STATUS_FINE);
	}
 
	return strText;
}

BOOL CNBLogicalDevice::IsFaultTolerant()
{
	return AnyUnitDevice()->IsFaultTolerant();
}

DWORD CNBLogicalDevice::GetFaultTolerance()
{
	DWORD status = NULL;
	UINT32 nFaultCount = 0;
	UINT32 nFailedCount = 0;
	DWORD fault_tolerance_unit;
	CNBUnitDevice *pUnitDevice;
	if(!IsFaultTolerant())
		return NDASBIND_LOGICAL_DEVICE_RMD_INVALID;

	for(UINT32 i = 0; i < DevicesInRaid(); i++)
	{
		pUnitDevice = UnitDeviceInRMD(i);
		if(!pUnitDevice)
		{
			nFailedCount++;
			continue;
		}

		fault_tolerance_unit = UnitDeviceInRMD(i)->GetFaultTolerance();

		if(NDASBIND_UNIT_DEVICE_RMD_FAULT & fault_tolerance_unit)
		{
			nFaultCount++;
		}
	}

	// same for RAID1, 4
	if(nFailedCount)
		status |= NDASBIND_LOGICAL_DEVICE_RMD_MISSING;

	if(1 == nFaultCount)
	{
		status |= NDASBIND_LOGICAL_DEVICE_RMD_FAULT;
	}
	else if(2 <= nFaultCount)
	{
		status |= NDASBIND_LOGICAL_DEVICE_RMD_BROKEN;
	}

	return status;
}

WTL::CString CNBLogicalDevice::GetFaultToleranceString()
{
	WTL::CString strText;
	DWORD status = GetFaultTolerance();

	if(NDASBIND_LOGICAL_DEVICE_RMD_INVALID & status)
	{
		strText.LoadString(IDS_FT_NOT_FAULT_TOLERANT);
	}
	else if(NDASBIND_LOGICAL_DEVICE_RMD_MISSING & status)
	{
		strText.LoadString(IDS_FT_MISSING);
	}
	else if(NDASBIND_LOGICAL_DEVICE_RMD_BROKEN & status)
	{
		strText.LoadString(IDS_FT_FAULT);
	}
	else if(NDASBIND_LOGICAL_DEVICE_RMD_FAULT & status)
	{
		strText.LoadString(IDS_FT_DIRTY);

		UINT64 SectorsFault;
		NDASCOMM_CONNECTION_INFO ci;
		AnyUnitDevice()->InitConnectionInfo(&ci, FALSE);
		
		if(NdasOpGetFaultSectorSize(&ci, &SectorsFault))
		{
			WTL::CString strText2 = strText;
			strText.Format(_T("%s(%d%%)"), strText2, 100 - (SectorsFault * SECTOR_SIZE * 100) / AnyUnitDevice()->GetCapacityInByte());
		}
	}
	else
	{
		strText.LoadString(IDS_FT_FAULT_TOLERANT);		
	}

	return strText;
}


BOOL CNBLogicalDevice::IsHDD()
{
	ATLASSERT(m_mapUnitDevices.size());

	return AnyUnitDevice()->IsHDD();
}

CNBUnitDevice *CNBLogicalDevice::UnitDeviceInRMD(UINT32 nIndex)
{
	if(!IsFaultTolerant())
		return NULL;

	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i))
			continue;

        if(m_mapUnitDevices[i]->m_cSequenceInRMD == nIndex)
			return m_mapUnitDevices[i];
	}

	return NULL;
}

BOOL CNBLogicalDevice::IsOperatable()
{
	if(!IsHDD())
		return FALSE;

	if((NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED | 
		NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY) &
		GetStatus())
		return FALSE;

	return TRUE;
}

BOOL CNBLogicalDevice::IsOperatableAll()
{
	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i) || !m_mapUnitDevices[i]->IsOperatable())
			return FALSE;
	}

	return TRUE;
}

DWORD CNBLogicalDevice::GetAccessMask()
{
	DWORD dwGrantedAccess = 0xFFFFFFFF;
	for(NBUnitDevicePtrMap::iterator itUnitDevice = m_mapUnitDevices.begin();
		itUnitDevice != m_mapUnitDevices.end(); itUnitDevice++)
	{
		dwGrantedAccess &= itUnitDevice->second->GetAccessMask();
	}

	return dwGrantedAccess;
}

BOOL CNBLogicalDevice::GetCommandAbility(int nID)
{
	switch(nID)
	{
	case IDM_TOOL_BIND:
		return TRUE;
	case IDM_TOOL_UNBIND:
		return (IsGroup() && IsOperatable());
	case IDM_TOOL_ADDMIRROR:
		return (IsHDD() && !IsGroup() && IsOperatableAll());
	case IDM_TOOL_MIGRATE:
		return (IsHDD() && NMT_MIRROR == GetType() && IsOperatableAll());
	case IDM_TOOL_REPLACE_DEVICE:
		return FALSE;
	case IDM_TOOL_REPLACE_UNIT_DEVICE:
		return FALSE;
	case IDM_TOOL_SINGLE:
		return (IsHDD() && !IsGroup() && IsOperatable());
	case IDM_TOOL_SPAREADD:
		return (IsFaultTolerant() && IsOperatableAll());
	case IDM_TOOL_SPAREREMOVE:
		return FALSE;
	default:
		return TRUE;
	}

	return FALSE;
}

BOOL CNBLogicalDevice::HixChangeNotify(LPCGUID guid)
{
	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i))
			return FALSE;

		m_mapUnitDevices[i]->HixChangeNotify(guid);
	}

	return TRUE;
}

NBUnitDevicePtrList CNBLogicalDevice::GetOperatableDevices()
{
	NBUnitDevicePtrList listUnitDevices;

	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i))
			continue;

		listUnitDevices.push_back(m_mapUnitDevices[i]);
	}

	return listUnitDevices;
}
