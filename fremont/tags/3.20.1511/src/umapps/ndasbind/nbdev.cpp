#include "stdafx.h"
#include "resource.h"

#include "nbmainfrm.h"
#include "nbdev.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasop.h>
#include <ndas/ndasid.h>

const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT = { 0xCD, NDAS_VID_DEFAULT, 0xFF, 0xFF };

static 
CString pGetMenuString(UINT MenuId)
{
	CMenu menu;
	CString menuString;
	menu.LoadMenu(MAKEINTRESOURCE(IDR_MAINPOPUP));
	menu.GetMenuString(MenuId, menuString, MF_BYCOMMAND);
	menuString.Remove(_T('&'));
	menu.DestroyMenu();
	return menuString;
}

CNBNdasDevice::CNBNdasDevice(PNDASUSER_DEVICE_ENUM_ENTRY pBaseInfo, NDAS_DEVICE_STATUS status)
{
	ATLASSERT(pBaseInfo);

	CopyMemory(&m_BaseInfo, pBaseInfo, sizeof(NDASUSER_DEVICE_ENUM_ENTRY));
	m_status = status;

	ATLVERIFY(SUCCEEDED( StringCchCopy(
		m_NdasId.Id, RTL_NUMBER_OF(m_NdasId.Id), pBaseInfo->szDeviceStringId) ));

	if (pBaseInfo->GrantedAccess & GENERIC_WRITE)
	{
		// We uses the magic write key in case write access is granted.
		ATLVERIFY(SUCCEEDED( StringCchCopy(
			m_NdasId.Key, RTL_NUMBER_OF(m_NdasId.Key), _T("*****")) ));
	}

	ZeroMemory(&m_DeviceId, sizeof(NDAS_DEVICE_ID));

	NdasIdStringToDeviceEx(pBaseInfo->szDeviceStringId, &m_DeviceId, NULL, &m_IdExtData);
	m_DeviceId.VID = m_IdExtData.VID;
		
#if 0
	NDASUSER_DEVICE_INFORMATION ndasDeviceInfo;
	ATLVERIFY( NdasQueryDeviceInformation(pBaseInfo->SlotNo, &ndasDeviceInfo) );
	m_DeviceId = ndasDeviceInfo.HardwareInfo.NdasDeviceId;
#endif

	if (m_DeviceId.VID == 0) {
		ATLTRACE(_T("Assume VID is 1 if VID=0\n"));
		m_DeviceId.VID = 1;
	}
	ATLTRACE(_T("new CNBNdasDevice(%p) : Name %s, ID %s, Slot %d, Access %08x\n"),
		this, pBaseInfo->szDeviceName, pBaseInfo->szDeviceStringId, pBaseInfo->SlotNo, pBaseInfo->GrantedAccess);
}

CNBNdasDevice::CNBNdasDevice(LPCTSTR DeviceName, PNDAS_DEVICE_ID pDeviceId, NDAS_DEVICE_STATUS status)
{
	NDASID_EXT_DATA NdasIdExtension = NDAS_ID_EXTENSION_DEFAULT;
	StringCchCopy(m_BaseInfo.szDeviceName,  RTL_NUMBER_OF(m_BaseInfo.szDeviceName), DeviceName);

	NdasIdExtension.VID = pDeviceId->VID;
	NdasIdDeviceToStringEx(pDeviceId, 
		m_BaseInfo.szDeviceStringId,
		NULL,
		NULL,
		&NdasIdExtension);
	
	m_BaseInfo.GrantedAccess = GENERIC_READ | GENERIC_WRITE;
	m_BaseInfo.SlotNo = 0;

	m_status = status;

	ATLVERIFY(SUCCEEDED( StringCchCopy(
		m_NdasId.Id, RTL_NUMBER_OF(m_NdasId.Id), m_BaseInfo.szDeviceStringId) ));

	if (m_BaseInfo.GrantedAccess & GENERIC_WRITE)
	{
		// We uses the magic write key in case write access is granted.
		ATLVERIFY(SUCCEEDED( StringCchCopy(
			m_NdasId.Key, RTL_NUMBER_OF(m_NdasId.Key), _T("*****")) ));
	}

	memcpy(&m_DeviceId, pDeviceId, sizeof(m_DeviceId));
	if (m_DeviceId.VID == 0) {
		ATLTRACE(_T("Assume VID is 1 if VID=0\n"));		
		m_DeviceId.VID = 1;
	}

	ATLTRACE(_T("new CNBNdasDevice(%p) : Name %s, ID %s, Slot N/A, Access %08x\n"),
		this, m_BaseInfo.szDeviceName, m_BaseInfo.szDeviceStringId, m_BaseInfo.GrantedAccess);
}

CNBNdasDevice::CNBNdasDevice(CNBNdasDevice* SrcDev, BOOL CopyUnit)
{
	CopyMemory(&m_BaseInfo, &SrcDev->m_BaseInfo, sizeof(NDASUSER_DEVICE_ENUM_ENTRY));
	m_status = SrcDev->m_status;
	CopyMemory(&m_DeviceId, &SrcDev->m_DeviceId, sizeof(NDAS_DEVICE_ID));
	CopyMemory(&m_NdasId, &SrcDev->m_NdasId, sizeof(NDAS_ID));	
	m_bServiceInfo = SrcDev->m_bServiceInfo;

	if (CopyUnit)	{
		// to do: copy unit. Copy all member of m_mapUnitDevices;
		ATLTRACE(_T("new CNBNdasDevice: constructing with copying unit is not implemented"));		
	}

	ATLTRACE(_T("new CNBNdasDevice(%p) : Name %s, ID %s, Slot %d, Access %08x\n"),
		this, m_BaseInfo.szDeviceName, m_BaseInfo.szDeviceStringId, m_BaseInfo.SlotNo, m_BaseInfo.GrantedAccess);

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

CNBUnitDevice* CNBNdasDevice::UnitDeviceAdd(PNDASUSER_UNITDEVICE_ENUM_ENTRY pBaseInfo, DWORD RaidFlags)
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
		return NULL;

	m_mapUnitDevices[pBaseInfo->UnitNo] = pUnitDevice;
	pUnitDevice->m_pDevice = this;
	pUnitDevice->m_RaidFlags = RaidFlags;

// 	pUnitDevice->Initialize(); // Not doing here because it takes time.. 
	return pUnitDevice;
}


CString CNBNdasDevice::GetName()
{
	CString strText;

	strText.Format(_T("%s"), m_BaseInfo.szDeviceName);

	return strText;
}

CString CNBDevice::GetCapacityString(UINT64 ui64capacity)
{
	CString strText;
	UINT64 ui64capacityMB = ui64capacity / (1024 * 1024);
	UINT32 uicapacityGB = (UINT32)(ui64capacityMB / 1024);
	UINT32 uicapacityMB = (UINT32)(ui64capacityMB % 1024) * 100 / 1024;

	if(0 == ui64capacityMB)
	{
		strText = _T("");
	}
	else
	{
		strText.FormatMessage(IDS_DISK_SIZE_IN_GB, uicapacityGB, uicapacityMB);
	}

	return strText;
}

UINT CNBDevice::FindIconIndex(UINT idicon, const UINT *anIconIDs, int nCount, int iDefault)
{
	for(int i = 0; i < nCount; i++)
	{
		if(idicon == anIconIDs[i])
		{
			return i;
		}
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
	ci->Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci->LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci->UnitNo = m_BaseInfo.UnitNo;
	ci->WriteAccess = bWriteAccess;
	//
	// Following two lines are not necessary, ZeroMemory does these.
	// But just to clarify!
	//
	ci->OEMCode.UI64Value = 0;
	ci->PrivilegedOEMCode.UI64Value = 0;
	ci->Protocol = NDASCOMM_TRANSPORT_LPX;

	ci->AddressType = NDASCOMM_CIT_NDAS_ID;
	ci->Address.NdasId = m_pDevice->m_NdasId;

	return TRUE;
}

CString CNBUnitDevice::GetCapacityString()
{
	return CNBDevice::GetCapacityString(GetPhysicalCapacityInByte());
}

BOOL CNBUnitDevice::IsThisUnit(BYTE Node[6], BYTE Vid, DWORD UnitNo)
{
	return !memcmp(
		m_pDevice->m_DeviceId.Node, Node, 
		sizeof(m_pDevice->m_DeviceId.Node))
		&& m_pDevice->m_DeviceId.VID == Vid
		&& m_BaseInfo.UnitNo == UnitNo;
}

BOOL CNBUnitDevice::IsThisDevice(BYTE Node[6], BYTE Vid)
{
	return !memcmp(
		m_pDevice->m_DeviceId.Node, Node, 
		sizeof(m_pDevice->m_DeviceId.Node))
		&& m_pDevice->m_DeviceId.VID == Vid;
}

BOOL CNBUnitDevice::Initialize(BOOL MissingInLogical)
{
	BOOL bReturn = FALSE;
	NDASCOMM_CONNECTION_INFO ci;
	HNDAS hNDAS = NULL;
	UINT32 nDIBSize = sizeof(m_DIB);
	NDAS_UNITDEVICE_HARDWARE_INFOW UnitInfo;

	m_PhysicalCapacity = 0;
	
	UpdateStatus();
	
	if(NDAS_UNITDEVICE_TYPE_UNKNOWN == m_BaseInfo.UnitDeviceType)
	{
		m_DIB.iMediaType = NMT_INVALID;
		m_DIB.nDiskCount = 1;
		// We don't need to connect to unknown device.
		return TRUE;
	}
	m_bMissingMember = MissingInLogical;

	if(!InitConnectionInfo(&ci, FALSE))
		goto out;
	
	if(!(hNDAS = NdasCommConnect(&ci))) {
		goto out;
	}

	::ZeroMemory(&UnitInfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));
	UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	bReturn = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &UnitInfo);
	if (!bReturn)
		goto out;
	m_PhysicalCapacity = (UnitInfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA) * SECTOR_SIZE;
	
	if (MissingInLogical) {
		// This disk's DIB and RMD is not used.
	
	} else {
		if(!NdasOpReadDIB(hNDAS, &m_DIB, &nDIBSize))
			goto out;
		m_SequenceInDib = m_DIB.iSequence;
		
		if (NMT_RAID1R2 == m_DIB.iMediaType ||
			NMT_RAID4R2 == m_DIB.iMediaType ||
			NMT_RAID1R3 == m_DIB.iMediaType ||
			NMT_RAID4R3 == m_DIB.iMediaType)
		{
			if(!NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_RMD, 1, (PBYTE)&m_RMD)) {
				goto out;
			}
			for(UINT32 i = 0; i < m_DIB.nDiskCount + m_DIB.nSpareCount; i++)
			{
				if(m_DIB.iSequence == m_RMD.UnitMetaData[i].iUnitDeviceIdx)
				{
					m_cSequenceInRMD = i;
					break;
				}
			}
		} 
		else if (NMT_MIRROR == m_DIB.iMediaType  ||
			NMT_AGGREGATE == m_DIB.iMediaType ||
			NMT_RAID0 == m_DIB.iMediaType  ||
			NMT_RAID1 == m_DIB.iMediaType  ||
			NMT_RAID4 == m_DIB.iMediaType)
		{
			// 3.11 RAID0 and aggregation may have RMD but Pre-3.10 aggregation may not have RMD.
			// And it is safe to ignore them.
			::ZeroMemory((PBYTE)&m_RMD, sizeof(m_RMD));
			m_cSequenceInRMD = m_SequenceInDib;
		} else {
			::ZeroMemory((PBYTE)&m_RMD, sizeof(m_RMD));
			m_cSequenceInRMD = m_SequenceInDib;
		}
	}

	bReturn = TRUE;
out:
	if (!bReturn) {
		// We may be able to connect to device when enumerating but failed to connect or read DIB here.
		m_DIB.iMediaType = NMT_INVALID;
		m_DIB.nDiskCount = 1;
	}
	if(hNDAS && !NdasCommDisconnect(hNDAS))
		return FALSE;

	return bReturn;
}

BOOL CNBUnitDevice::HixChangeNotify(LPCGUID guid)
{
	if (m_pDevice) {
		return NdasCommNotifyUnitDeviceChange(
			NDAS_DIC_NDAS_ID,
			m_pDevice->m_NdasId.Id,
			m_BaseInfo.UnitNo,
			guid);
	} else {
		return FALSE;
	}
}

BOOL CNBUnitDevice::IsDefective()
{
	if(!IsGroup())
		return FALSE;
	if (GetRaidStatus() & (NDAS_RAID_MEMBER_FLAG_BAD_SECTOR | NDAS_RAID_MEMBER_FLAG_BAD_DISK))
		return TRUE;
	else
		return FALSE;
}


BOOL CNBUnitDevice::IsNotSynced()
{
	if(!IsGroup())
		return FALSE;
	if (GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC)
		return TRUE;
	else
		return FALSE;

//	return NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & m_RMD.UnitMetaData[m_cSequenceInRMD].UnitDeviceStatus;
}

BOOL CNBUnitDevice::IsSpare()
{
	if(!IsGroup())
		return FALSE;
	if (GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_SPARE)
		return TRUE;
	return FALSE;
//	return m_cSequenceInRMD >= m_DIB.nDiskCount;
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
	case IDM_TOOL_REMOVE_FROM_RAID:
		if (!(NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & GetStatus()) && 
			IsFaultTolerant() && 
			GetLogicalDevice()->DevicesSpare() && GetLogicalDevice()->IsOperatable() &&
			!(GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE)) {
			// Available only for redundant RAID with spare disk. 
			return TRUE;
		}
		return FALSE;
	case IDM_TOOL_REPLACE_DEVICE:
		if (!(NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & GetStatus()) 	&& 
			IsFaultTolerant() && GetLogicalDevice()->IsOperatable() &&
			!(GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE)) {
			//
			// Available only for redundant RAID. More check is required by OnReplaceDevice
			//
			// Later we may add replace device for aggregation/RAID0 by copying disk contents and reconfiguration.
			//
			return TRUE;
		} 
		return FALSE;
	case IDM_TOOL_USE_AS_MEMBER:
		if (IsFaultTolerant() && IsOperatable() && 
			(GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_INVALID_MEMBER)	&& 
			GetLogicalDevice()->IsOperatable()&&
			!(GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE)) {
			// This command is available only for the disk that is online and has no physical error but is currently not a member.
			// And logical device is not in use.
			return TRUE;
		}
		return FALSE;
//	case IDM_TOOL_SINGLE:
//		return (IsHDD() && IsOperatable());
	case IDM_TOOL_SPAREADD:
		return FALSE;
	case IDM_TOOL_RESET_BIND_INFO:
		return FALSE;
	case IDM_TOOL_CLEAR_DEFECTIVE:
		if (IsOperatable() && IsDefective() && GetLogicalDevice()->IsOperatable()) {
			return TRUE;
		}
		return FALSE;
	case IDM_TOOL_USE_AS_BASIC:
		if (IsFaultTolerant() && IsOperatable() && !IsMissingMember() && GetLogicalDevice()->IsOperatable()) {
			return TRUE;
		}
		return FALSE;
//	case IDM_TOOL_SPAREREMOVE:
//		return (IsFaultTolerant() && IsSpare() && m_pLogicalDevice->IsOperatableAll());
	default:
		ATLASSERT(FALSE);
		return TRUE;
	}
}


UINT32 CNBUnitDevice::GetType()
{
	if (m_pLogicalDevice) {
		return m_pLogicalDevice->GetType();
	}

	if(m_pDevice == NULL || NDAS_DEVICE_STATUS_CONNECTED != m_pDevice->m_status) {
		return NMT_INVALID;
	}

	return m_DIB.iMediaType;
}

CString CNBUnitDevice::GetName()
{
	CString strText;
	if (m_pDevice) {
		if(1 != m_pDevice->UnitDevicesCount() || m_BaseInfo.UnitNo>0)
		{
			strText.Format(_T("%s:%d"), m_pDevice->m_BaseInfo.szDeviceName, m_BaseInfo.UnitNo + 1);
		}
		else
		{
			strText.Format(_T("%s"), m_pDevice->m_BaseInfo.szDeviceName);
		}
	} else {
		strText = "";
	}
	return strText;
}

UINT CNBUnitDevice::GetIconIndex(const UINT *anIconIDs, int nCount)
{
	if (m_pDevice->m_status == NDAS_DEVICE_STATUS_DISABLED ||
		m_pDevice->m_status == NDAS_DEVICE_STATUS_DISCONNECTED) {
		return FindIconIndex(IDI_DEVICE_FAIL, anIconIDs, nCount);
	} else if (m_pLogicalDevice->IsGroup() && 
		(!NdasOpIsValidMember(m_RaidFlags) || 
		NdasOpIsConflictMember(m_RaidFlags) ||
		m_pDevice->m_status == NDAS_DEVICE_STATUS_NOT_REGISTERED)) {
		return FindIconIndex(IDI_DEVICE_WARN, anIconIDs, nCount);
	} else {
		return FindIconIndex(IDI_DEVICE_BASIC, anIconIDs, nCount);
	}
}

UINT CNBUnitDevice::GetSelectIconIndex(const UINT *anIconIDs, int nCount)
{
	return GetIconIndex(anIconIDs, nCount);
}

BOOL CNBUnitDevice::IsGroup()
{
	switch(GetType())
	{
	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID1R3:		
	case NMT_RAID4:
	case NMT_RAID4R2:
	case NMT_RAID4R3:		
		return TRUE;
	case NMT_INVALID:
	case NMT_SINGLE:
	case NMT_SAFE_RAID1:
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	case NMT_CONFLICT:
	default:
		return FALSE;
	}
}

BOOL CNBUnitDevice::IsHDD()
{
	switch(GetType())
	{
	case NMT_SINGLE:
	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID1R3:		
	case NMT_RAID4:
	case NMT_RAID4R2:
	case NMT_RAID4R3:		
	case NMT_AOD:
	case NMT_VDVD:
		return TRUE;
	case NMT_INVALID:
	case NMT_SAFE_RAID1:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	case NMT_CONFLICT:
		return FALSE;
	}
	return TRUE;
}

CString CNBUnitDevice::GetIDString(TCHAR HiddenChar)
{
	CString strText;
	if (m_pDevice) {
		CString strID = m_pDevice->m_BaseInfo.szDeviceStringId;
		strID.Remove(_T('-'));

		strText += 
			strID.Mid(0, 5) + _T("-") +
			strID.Mid(5, 5) + _T("-") +
			strID.Mid(10, 5) + _T("-");
		strText += HiddenChar;
		strText += HiddenChar;
		strText += HiddenChar;
		strText += HiddenChar;
		strText += HiddenChar;
		//		strID.Mid(0, 5) + _T("-") +
	} else {
		//
		// to do: create ID from device ID.(But we don't have corresponding NDAS device object!!)

		//
		strText += "";
	}
	return strText;
}

UINT64 CNBUnitDevice::GetLogicalCapacityInByte()
{
	if(!IsHDD())
	{
		return 0;
	}
	if (IsMissingMember()) 
	{
		return 0;
	}
	return m_DIB.sizeUserSpace * SECTOR_SIZE;
}

VOID CNBUnitDevice::UpdateStatus()
{
	DWORD status = 0;
	// no write key
	if(!(GENERIC_WRITE & m_pDevice->m_BaseInfo.GrantedAccess))
	{
		status |= NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY;
	}

	// disconnected
	if(NDAS_DEVICE_STATUS_CONNECTED != m_pDevice->m_status)
	{
		if (NDAS_DEVICE_STATUS_DISABLED == m_pDevice->m_status) 
		{
			status |= NDASBIND_UNIT_DEVICE_STATUS_DISABLED;
		} 
		else if (NDAS_DEVICE_STATUS_NOT_REGISTERED == m_pDevice->m_status)
		{
			status |= NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED;
		} 
		else 
		{
			status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;
		}
		// can not process anymore
		m_Status = status;
		return;
	}
	// svc's report may not be up to date. 
	if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_OFFLINE) {
		status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;
		m_Status = status;
		return;
	}

	// Check device is in use.
	NDASCOMM_CONNECTION_INFO ci;
	if(!InitConnectionInfo(&ci, FALSE))
	{
		m_Status = status;
		return;
	}

	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;

	NDAS_UNITDEVICE_STAT udstat = {0};
	udstat.Size = sizeof(NDAS_UNITDEVICE_STAT);

	if (NdasCommGetUnitDeviceStat(&ci, &udstat)) {
		if (udstat.RWHostCount || 
			(udstat.ROHostCount && 	NDAS_HOST_COUNT_UNKNOWN != udstat.ROHostCount) )
		{
			status |= NDASBIND_UNIT_DEVICE_STATUS_MOUNTED;
		} 
	} else {
		status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;
	}
	m_Status = status;
	return;
}

DWORD CNBUnitDevice::GetStatus()
{
	return m_Status;
}

CString CNBUnitDevice::GetStatusString()
{
	CString strText;

	DWORD status = GetStatus();

	if(NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED & status)
	{
		strText.LoadString(IDS_STATUS_NOT_CONNECTED);
	}
	else if (NDASBIND_UNIT_DEVICE_STATUS_DISABLED & status)
	{
		strText.LoadString(IDS_STATUS_DISABLED);
	}
	else if (NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED & status)
	{
		strText.LoadString(IDS_STATUS_NOT_REGISTERED);
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
	case NMT_RAID1R2:
	case NMT_RAID4R2:
	case NMT_RAID4:
		return FALSE;
	case NMT_RAID1R3:
	case NMT_RAID4R3:		
		return TRUE;
	default:
		return FALSE;
	}
}

UINT32 CNBUnitDevice::GetSequence()
{
//	ATLASSERT(m_pLogicalDevice);
	return (IsGroup()) ? m_SequenceInDib: 0;
}

CString CNBUnitDevice::GetRaidStatusString()
{
	CString strText;
	CString strTemp;
	switch(GetType()) {
	case NMT_INVALID:
	case NMT_AOD: 
	case NMT_VDVD: 
	case NMT_CDROM: 
	case NMT_OPMEM: 
	case NMT_FLASH: 
	case NMT_SINGLE: 		
	case NMT_CONFLICT:
		// Not a RAID.
		strText = _T("");
		break;
	case NMT_MIRROR: 
	case NMT_SAFE_RAID1: 	
	case NMT_RAID1: 
	case NMT_RAID1R2: 	
	case NMT_RAID4:
	case NMT_RAID4R2:
		// No status string. Logical dev will show Migrate status.
		break;
	case NMT_AGGREGATE: 
	case NMT_RAID0: 
	case NMT_RAID1R3: 
	case NMT_RAID4R3: 
		// Handle serious problem first
		if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED) {
			strText.LoadString(IDS_RAID_STATUS_CORRUPTED_INFO);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET) {
			// Hardly happen??
			strText.LoadString(IDS_RAID_STATUS_ANOTHER_RAID_MEMBER);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_IO_FAILURE) {
			strText.LoadString(IDS_RAID_STATUS_READ_FAILURE);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH) {
			strText.LoadString(IDS_RAID_STATUS_NOT_A_MEMBER);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_REPLACED_BY_SPARE) {
			if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE) {
				strText.LoadString(IDS_RAID_STATUS_IRRECONCILABLE_FROM_DEGRADED_USE);
				strText += _T("\n");				
			} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_BAD_SECTOR) {
				strText.LoadString(IDS_RAID_STATUS_BAD_SECTOR);
				strText += _T("\n");
			} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_BAD_DISK) {
				strText.LoadString(IDS_RAID_STATUS_BAD_DISK);
				strText += _T("\n");
			}			
			strTemp.LoadString(IDS_RAID_STATUS_REPLACED_BY_SPARE);
			strText += strTemp;
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE) {
			strText.LoadString(IDS_RAID_STATUS_IRRECONCILABLE_FROM_DEGRADED_USE);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_BAD_SECTOR) {
			strText.LoadString(IDS_RAID_STATUS_BAD_SECTOR);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_BAD_DISK) {
			strText.LoadString(IDS_RAID_STATUS_BAD_DISK);
		} else {
			// Normal status.
			if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC) {
				strText.LoadString(IDS_RAID_STATUS_OUT_OF_SYNC);
			} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_SPARE) {
				strText.LoadString(IDS_RAID_STATUS_SPARE);
			}
		}
		break;
	default:
		strText = _T(""); // anyway this does not happen.
		break;
	}

	return strText; // Don't need to show
}

CString CNBUnitDevice::GetCommentString()
{
	CString strText;
	CString strFmt;
	CString menuStr1;
	CString menuStr2;
	CString menuStr3;	
	switch(GetType()) {
	case NMT_INVALID:
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	case NMT_SINGLE:
	case NMT_CONFLICT:
		// No comment.
		break;
	case NMT_MIRROR:
	case NMT_SAFE_RAID1:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID4:
	case NMT_RAID4R2:
		// Older version. No comment.
		break;
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
		// Check serious case first
		if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED) {
			strFmt.LoadString(IDS_RAID_COMMENT_RMD_CORRUPTED_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);
			strText.FormatMessage(strFmt, menuStr1);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET) {
			// Hard to happen??
			strText = _T("");
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_IO_FAILURE) {
			strText.LoadString(IDS_RAID_COMMENT_IO_FAILURE);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_DIB_MISMATCH) {
			strText.LoadString(IDS_RAID_COMMENT_DIB_MISMATCH);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_IRRECONCILABLE) {
			strFmt.LoadString(IDS_RAID_COMMENT_IRRECONCILABLE_FROM_DEGRADED_USE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_BASIC);
			strText.FormatMessage(strFmt, menuStr1, menuStr2);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_REPLACED_BY_SPARE) {
			strFmt.LoadString(IDS_RAID_COMMENT_REPLACED_BY_SPARE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_REPLACE_DEVICE);
			menuStr2 = pGetMenuString(IDM_TOOL_REMOVE_FROM_RAID);
			menuStr3 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);			
			strText.FormatMessage(strFmt, menuStr1, menuStr2, menuStr3);
		} else if (m_RaidFlags & (NDAS_RAID_MEMBER_FLAG_BAD_SECTOR |NDAS_RAID_MEMBER_FLAG_BAD_DISK)) {
			strFmt.LoadString(IDS_RAID_COMMENT_DEFECTIVE_FMT);
			menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_MEMBER);
			menuStr2 = pGetMenuString(IDM_TOOL_CLEAR_DEFECTIVE);
			strText.FormatMessage(strFmt, menuStr1, menuStr2);
		} else {
			// Normal status.
			if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_OUT_OF_SYNC) {
				strText.LoadString(IDS_RAID_COMMENT_OUT_OF_SYNC);
			} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_SPARE) {
				strText.LoadString(IDS_RAID_COMMENT_SPARE);
			}
		}
		break;
	default:
		strFmt.LoadString(IDS_RAID_COMMENT_UNKNOWN_TYPE_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_USE_AS_BASIC);
		strText.FormatMessage(strFmt, menuStr1);
		break;
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

	return PrimaryUnitDevice()->IsGroup();
}

CString CNBLogicalDevice::GetCapacityString()
{
	return CNBDevice::GetCapacityString(GetLogicalCapacityInByte());
}

UINT32 CNBLogicalDevice::GetType()
{
	ATLASSERT(m_mapUnitDevices.size());

	return DIB()->iMediaType;
}

CString CNBLogicalDevice::GetName()
{
	ATLASSERT(m_mapUnitDevices.size());

	CString strText;

	if(0 == m_mapUnitDevices.size())
	{
		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, NMT_INVALID);
	}
	else if(IsGroup())
	{
		strText = GetRaidStatusString();
		switch(GetType()) {
			case NMT_AGGREGATE: strText.LoadString(IDS_LOGDEV_TYPE_AGGREGATED_DISK); break;
			case NMT_MIRROR: strText.LoadString(IDS_LOGDEV_TYPE_MIRRORED_DISK); break;
			case NMT_RAID0: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID0); break;
			case NMT_RAID1: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1); break;
			case NMT_RAID1R2: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1R2); break;
			case NMT_RAID1R3: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1R3); break;	
			case NMT_RAID4: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4); break;
			case NMT_RAID4R2: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4R2); break;
			case NMT_RAID4R3: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4R3); break;	
			default:		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, GetType());
		}
	}
	else
	{
		ATLASSERT(1 == m_mapUnitDevices.size() && m_mapUnitDevices.count(0));

		strText = m_mapUnitDevices[0]->GetName();
	}

	return strText;
}

UINT CNBLogicalDevice::GetIconIndex(const UINT *anIconIDs, int nCount)
{
	ATLASSERT(m_mapUnitDevices.size());

	const struct { 
		UINT32 Type; 
		UINT IconID; 
	} ImageIndexTable[] = {
		NMT_INVALID,    IDI_DEVICE_FAIL,
		NMT_AOD,        IDI_DEVICE_BASIC,
		NMT_MIRROR,     IDI_DEVICE_BOUND,
		NMT_AGGREGATE,  IDI_DEVICE_BOUND,
		NMT_RAID0,      IDI_DEVICE_BOUND,
		NMT_RAID1,      IDI_DEVICE_BOUND,
		NMT_RAID1R2,      IDI_DEVICE_BOUND,
		NMT_RAID1R3,      IDI_DEVICE_BOUND,		
		NMT_RAID4,      IDI_DEVICE_BOUND,
		NMT_RAID4R2,      IDI_DEVICE_BOUND,
		NMT_RAID4R3,      IDI_DEVICE_BOUND,		
		NMT_VDVD,       IDI_DEVICE_BASIC,
		NMT_CDROM,      IDI_DEVICE_BASIC,
		NMT_FLASH,      IDI_DEVICE_BASIC,
		NMT_OPMEM,      IDI_DEVICE_BASIC,
		NMT_SAFE_RAID1, IDI_DEVICE_BASIC,
		NMT_CONFLICT,  IDI_DEVICE_WARN,
	};

	UINT32 Type = GetType();

	for (size_t i = 0; i < RTL_NUMBER_OF(ImageIndexTable); ++i)
	{
		if (Type == ImageIndexTable[i].Type)
		{
			return FindIconIndex(ImageIndexTable[i].IconID, anIconIDs, nCount);
		}
	}
	return 1;
}

UINT CNBLogicalDevice::GetSelectIconIndex(const UINT *anIconIDs, int nCount)
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

	if (m_DIB.nDiskCount == pUnitDevice->m_DIB.nDiskCount &&
		m_DIB.nSpareCount == pUnitDevice->m_DIB.nSpareCount &&
		!memcmp(m_DIB.UnitDisks, pUnitDevice->m_DIB.UnitDisks, 
			(m_DIB.nDiskCount + pUnitDevice->m_DIB.nSpareCount) * sizeof(UNIT_DISK_LOCATION)) &&
		!memcmp(&m_RaidSetId, &pUnitDevice->m_RMD.RaidSetId, sizeof(GUID)) 
		/* &&!memcmp(&m_ConfigSetId, &pUnitDevice->m_RMD.ConfigSetId, sizeof(GUID))*/) // Don't compare config set.
	{
		return TRUE;
	}
	return FALSE;
}

BOOL CNBLogicalDevice::UnitDeviceSet(CNBUnitDevice *pUnitDevice, UINT32 SequenceNum)
{
	NDAS_DEVICE_STATUS DeviceStatus;
	
	if (!m_mapUnitDevices.size()) {
		// Assume as primary if inserted first.
		// Set identify of this logical device
		memcpy(&m_RaidSetId, &pUnitDevice->m_RMD.RaidSetId, sizeof(m_RaidSetId));
		memcpy(&m_ConfigSetId, &pUnitDevice->m_RMD.ConfigSetId, sizeof(m_ConfigSetId));			
		memcpy(&m_DIB, &pUnitDevice->m_DIB, sizeof(m_DIB));
		m_PrimaryUnit = pUnitDevice;
	} else {
		// If this unit has larger USN, then make this device as primary device.
		if (!pUnitDevice->IsMissingMember()) {
			if (m_PrimaryUnit->m_RMD.uiUSN < pUnitDevice->m_RMD.uiUSN) {
				m_PrimaryUnit = pUnitDevice;
			}
		}
	}
#if 0	
	if (pUnitDevice->m_RaidFlags & NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED) {
		// We cannot mount not-registered device. Change RAID info.
		m_RaidInfo.MountablityFlags &= ~(NDAS_RAID_MOUNTABILITY_MOUNTABLE);
		m_RaidInfo.MountablityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		m_RaidInfo.FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED;
	}
#endif	
	DeviceStatus = pUnitDevice->GetNdasDevice()->m_status;
	if (DeviceStatus == NDAS_DEVICE_STATUS_NOT_REGISTERED) {
		// We cannot mount RAID with not-registered device. Change RAID info.
		m_RaidInfo.MountablityFlags &= ~(NDAS_RAID_MOUNTABILITY_MOUNTABLE);
		m_RaidInfo.MountablityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		m_RaidInfo.FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED;
	} else if (DeviceStatus == NDAS_DEVICE_STATUS_DISABLED) {
		// We cannot mount RAID with disabled device. Change RAID info.
		m_RaidInfo.MountablityFlags &= ~(NDAS_RAID_MOUNTABILITY_MOUNTABLE);
		m_RaidInfo.MountablityFlags |= NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		m_RaidInfo.FailReason |= NDAS_RAID_FAIL_REASON_MEMBER_DISABLED;
	}
		
	m_mapUnitDevices[SequenceNum] = pUnitDevice;
	ATLTRACE(_T("CNBLogicalDevice(%p).m_mapUnitDevices[%d] = (%p) : %s\n"),
		this, SequenceNum, pUnitDevice, pUnitDevice->GetName());

	pUnitDevice->m_pLogicalDevice = this;
	pUnitDevice->m_SequenceInDib = SequenceNum;
	return TRUE;
}

CNBUnitDevice *CNBLogicalDevice::PrimaryUnitDevice()
{
	ATLASSERT(m_mapUnitDevices.size());

	if(!m_mapUnitDevices.size())
		return NULL;
	return m_PrimaryUnit;
//	return m_mapUnitDevices.begin()->second;
}

UINT32 CNBLogicalDevice::DevicesTotal(BOOL bAliveOnly)
{
	ATLASSERT(m_mapUnitDevices.size());

	UINT32 total = 0;

	if(bAliveOnly)
	{		
		for(UINT32 i = 0; i < DevicesTotal(FALSE); i++)
		{
			if(m_mapUnitDevices.count(i))
			{
				total++;
			}
		}
	}
	else
	{
		total = (IsGroup()) ? DIB()->nDiskCount + DIB()->nSpareCount : 1;
	}

	return total;
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

CString CNBLogicalDevice::GetIDString(TCHAR HiddenChar)
{
	CString strText;
	ATLASSERT(m_mapUnitDevices.size());

	if(IsGroup())
	{
		strText = _T("");
	}
	else
	{
		ATLASSERT(1 == m_mapUnitDevices.size() && m_mapUnitDevices.count(0));
		strText = PrimaryUnitDevice()->GetIDString(HiddenChar);
	}

	return strText;
}

UINT64 CNBLogicalDevice::GetLogicalCapacityInByte()
{
	UINT64 size = 0;
	if(!IsGroup())
		return PrimaryUnitDevice()->GetLogicalCapacityInByte();

	switch(GetType())
	{
	case NMT_AGGREGATE:
		if (DevicesInRaid() != DevicesTotal(TRUE)) {
			// We need all disk's size to know total size.
			size = 0;
		} else {
			for(UINT32 i = 0; i < DevicesTotal(); i++)
			{
				if(m_mapUnitDevices.count(i)) {
					if (m_mapUnitDevices[i]->IsMissingMember()) {
						size =0;
						break;
					}
					size += m_mapUnitDevices[i]->GetLogicalCapacityInByte();
				} else {
					size = 0;
					break;
				}
			}
		}
		break;
	case NMT_RAID0:
		size = PrimaryUnitDevice()->GetLogicalCapacityInByte() * DevicesInRaid();
		break;
	case NMT_MIRROR:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID1R3:		
		size = PrimaryUnitDevice()->GetLogicalCapacityInByte();
		break;
	case NMT_RAID4:
	case NMT_RAID4R2:
	case NMT_RAID4R3:		
		size = PrimaryUnitDevice()->GetLogicalCapacityInByte() * (DevicesInRaid() -1);
		break;
	default:
		ATLASSERT(FALSE);
	}

	return size;
}

DWORD CNBLogicalDevice::GetStatus()
{
	DWORD status = NULL, status_unit;

	if (IsGroup()){
		for(UINT32 i = 0; i < DevicesTotal(); i++)
		{
			status_unit = m_mapUnitDevices[i]->GetStatus();

			//
			// we don't need check disconnected status for RAID
			
			if(NDASBIND_UNIT_DEVICE_STATUS_DISABLED & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_DISABLED;
				continue;
			}

			if(NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY;
			}

			if (m_mapUnitDevices[i]->IsMissingMember()) {
				// We don't care whether it is in use if it is current member.
				
			} else {
				if (NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & status_unit)
				{
					status |= NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED;
				}
			}

			if (NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED;
			}
		}

	} else {
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

			if(NDASBIND_UNIT_DEVICE_STATUS_DISABLED & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_DISABLED;
				continue;
			}

			if(NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY;
			}

			if (NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED;
			}
			if (NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED & status_unit)
			{
				status |= NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED;
			}
		}
	}
	return status;
}

CString CNBLogicalDevice::GetStatusString()
{
	CString strText;
	DWORD status = GetStatus();

	if (NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED & status) {
		strText.LoadString(IDS_STATUS_NOT_CONNECTED);
	} 
	else if (NDASBIND_LOGICAL_DEVICE_STATUS_DISABLED & status) {
		strText.LoadString(IDS_STATUS_DISABLED);
	} 
	else if (NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED & status) {
		strText.LoadString(IDS_STATUS_NOT_REGISTERED);
	}
	else if(NMT_INVALID == GetType())
	{
		// Maybe failed to connect inspite of reported as connected.
		// Safe to handle as if not connected?
		strText.LoadString(IDS_STATUS_NOT_CONNECTED);
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
		if (IsGroup()) {
			if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE)  {
				strText.LoadString(IDS_STATUS_FINE);
			} else {
				strText.LoadString(IDS_STATUS_UNMOUNTABLE);
			}
		} else {
			strText.LoadString(IDS_STATUS_FINE);
		}
	}
 
	return strText;
}

BOOL CNBLogicalDevice::IsFaultTolerant()
{
	return PrimaryUnitDevice()->IsFaultTolerant();
}


CString CNBLogicalDevice::GetRaidStatusString()
{
	CString strText;

	switch(GetType())
	{
	case NMT_INVALID: strText = _T(""); break;
	case NMT_SINGLE: strText = _T(""); break;
	case NMT_CDROM: strText.LoadString(IDS_LOGDEV_TYPE_DVD_DRIVE); break;
	case NMT_OPMEM: strText.LoadString(IDS_LOGDEV_TYPE_MO_DRIVE); break;
	case NMT_FLASH: strText.LoadString(IDS_LOGDEV_TYPE_CF_DRIVE); break;
	case NMT_CONFLICT: strText.LoadString(IDS_RAID_STATUS_DIB_CONFLICT);	break;
	case NMT_MIRROR: 
	case NMT_RAID1: 
	case NMT_RAID1R2: 
	case NMT_RAID4: 
	case NMT_RAID4R2: 
		strText.LoadString(IDS_RAID_STATUS_MIGRATION_REQUIRED);
		break;
	case NMT_AGGREGATE: 
	case NMT_RAID0: 
	case NMT_RAID1R3: 
	case NMT_RAID4R3: 
		if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE) 
		{
			if (m_RaidInfo.FailReason & 
				(NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID |
				NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION)) 
			{
				strText.LoadString(IDS_RAID_STATUS_UNSUPPORTED);
			} 
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_IRRECONCILABLE) 
			{
				strText.LoadString(IDS_RAID_STATUS_IRRECONCILABLE);
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_RMD_CORRUPTED) 
			{
				strText.LoadString(IDS_RAID_STATUS_UNMOUTABLE_BY_RMD_CORRUPTION);
			} 
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_SPARE_USED)
			{
				strText.LoadString(IDS_RAID_STATUS_SPARE_USED);
			}
			else if (m_RaidInfo.FailReason & 
				(NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE|
				NDAS_RAID_FAIL_REASON_DIB_MISMATCH|
				NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET|
				NDAS_RAID_FAIL_REASON_NOT_A_RAID|
				NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL))
			{
				strText.LoadString(IDS_RAID_STATUS_MISSING_MEMBER);
			}  
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED)
			{
				strText.LoadString(IDS_RAID_STATUS_HAS_INVALID_MEMBER);
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_DISABLED)
			{
				strText.LoadString(IDS_RAID_STATUS_HAS_INVALID_MEMBER);
			}		
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED)
			{
				strText.LoadString(IDS_RAID_STATUS_MIGRATION_REQ);
			} 
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DEFECTIVE)
			{
				strText.LoadString(IDS_RAID_STATUS_DEFECTIVE_DISK);
			} else {
				strText = _T("");
			}
		} else if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE) {
			if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_DEGRADED) {
				strText.LoadString(IDS_RAID_STATUS_DEGRADED);				
			} else if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_OUT_OF_SYNC) {
				DWORD SynchingProgress;
				if (m_RaidInfo.TotalBitCount) {
					SynchingProgress = 100 - (m_RaidInfo.OosBitCount * 100/m_RaidInfo.TotalBitCount);
					if (SynchingProgress == 100)
						SynchingProgress =99;
					strText.FormatMessage(IDS_RAID_STATUS_NEED_RESYNC_PROGRESS, SynchingProgress);
				} else {
					strText.LoadString(IDS_RAID_STATUS_NEED_RESYNC);
				}
			} else {
				strText.LoadString(IDS_RAID_STATUS_HEALTHY);
			}
		} else {
			// Should not happen.
			strText = _T("");
		}
		break;
	default:
		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, GetType());
		break;
	}
	return strText;
}

CString CNBLogicalDevice::GetCommentString()
{
	CString strText;
	CString strFmt;
	CString menuStr1;
	
	switch(GetType())
	{
	case NMT_INVALID: 
	case NMT_SINGLE: 
	case NMT_CDROM: 
	case NMT_OPMEM: 
	case NMT_FLASH: 
		strText = _T(""); break;
	case NMT_CONFLICT: 
		strFmt.LoadString(IDS_RAID_COMMENT_DIB_CONFLICT_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_RESET_BIND_INFO);
		strText.FormatMessage(strFmt, menuStr1);
		break;
	case NMT_MIRROR: 
	case NMT_RAID1: 
	case NMT_RAID1R2: 
		strFmt.LoadString(IDS_RAID_COMMENT_MIGRATE_MIRROR_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_MIGRATE);
		strText.FormatMessage(strFmt, menuStr1);
		break;
	case NMT_RAID4: 
	case NMT_RAID4R2:
		strFmt.LoadString(IDS_RAID_COMMENT_MIGRATE_RAID4_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_MIGRATE);
		strText.FormatMessage(strFmt, menuStr1);
		break;
	case NMT_AGGREGATE: 
	case NMT_RAID0: 
	case NMT_RAID1R3: 
	case NMT_RAID4R3: 
		if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE) 
		{
			if (m_RaidInfo.FailReason & 
				(NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID |
				NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION)) 
			{
				strText.LoadString(IDS_RAID_COMMENT_HIGHER_VER);	
			} 
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_IRRECONCILABLE) 
			{
				strText.LoadString(IDS_RAID_COMMENT_IRRECONCILABLE);
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_RMD_CORRUPTED) 
			{
				strText.LoadString(IDS_RAID_COMMENT_RECONF_RMD_CORRUPTED);
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_SPARE_USED)
			{
				strText.LoadString(IDS_RAID_COMMENT_SPARE_USED);
			} else if (m_RaidInfo.FailReason & 
				(NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE|
				NDAS_RAID_FAIL_REASON_DIB_MISMATCH|
				NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET|
				NDAS_RAID_FAIL_REASON_NOT_A_RAID|
				NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL))
			{
				if (GetType() == NMT_AGGREGATE ||
					GetType() == NMT_RAID0) {
					strText.LoadString(IDS_RAID_COMMENT_NO_FAULT_TOLERANT);
				} else {
					strText.LoadString(IDS_RAID_COMMENT_NOT_ENOUGH_MEMBER);				
				}
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED) 
			{
				strText.LoadString(IDS_RAID_COMMENT_HAS_UNREGISTERED_MEMBER);
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MEMBER_DISABLED) 
			{
				strText.LoadString(IDS_RAID_COMMENT_HAS_DISABLED_MEMBER);
			}
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED)
			{
				strText.LoadString(IDS_RAID_COMMENT_MIGRATION_REQ);
			} 
			else if (m_RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DEFECTIVE)
			{
				strText.LoadString(IDS_RAID_COMMENT_DEFECTIVE_RAID);
			} else {
				strText = _T("");
			}			
		} else if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE) {
			if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_DEGRADED) {
				strText.LoadString(IDS_RAID_COMMENT_DEGRADED);				
			} else if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_OUT_OF_SYNC) {
				strText.LoadString(IDS_RAID_COMMENT_RESYNC_OUT_OF_SYNC_RAID);
			} else {
				strText = _T("");
			}
		} else {
			// Should not happen.
			strText = _T("");
		}
		break;
	default:
		strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, GetType());
		break;
	}
	return strText;
}


BOOL CNBLogicalDevice::IsHDD()
{
	ATLASSERT(m_mapUnitDevices.size());

	return PrimaryUnitDevice()->IsHDD();
}

BOOL CNBLogicalDevice::IsOperatable()
{
	if(!IsHDD())
		return FALSE;

	if((NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED | 
		NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY) &
		GetStatus())
		return FALSE;
	
	// At least one member should be connected
	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(m_mapUnitDevices.count(i) && m_mapUnitDevices[i]->IsOperatable()) {
			return TRUE;
		}
	}

	return FALSE;
}

// RAID has no missing member and mountable with no RAID info conflict
BOOL CNBLogicalDevice::IsHealthy()
{
	if (NMT_SINGLE == GetType())
		return TRUE;
		
	if (HasMissingMember())
		return FALSE;
	if (m_RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_NORMAL)
		return TRUE;
	return FALSE;
}

BOOL CNBLogicalDevice::IsMigrationRequired()
{
	if (NMT_MIRROR == GetType() ||
		NMT_RAID1 == GetType() ||
		NMT_RAID4 == GetType() ||
		NMT_RAID1R2 == GetType() ||
		NMT_RAID4R2 == GetType()) {
		return TRUE;
	} else {
		return FALSE;
	}
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

BOOL CNBLogicalDevice::HasMissingMember()
{
	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i) || m_mapUnitDevices[i]->IsMissingMember())
			return TRUE;
	}
	return FALSE;
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
	if (IsGroup()) {
		switch(nID)
		{
		case IDM_TOOL_BIND:
			return TRUE;
		case IDM_TOOL_UNBIND:
			// Cannot unbind devices in use.
			return !(GetStatus() & NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED);
		case IDM_TOOL_ADDMIRROR:
			return FALSE;
		case IDM_TOOL_MIGRATE:
			return (
				IsHDD() && IsMigrationRequired() && IsOperatableAll() && !HasMissingMember());
		case IDM_TOOL_REPLACE_DEVICE:
			return FALSE;
		case IDM_TOOL_USE_AS_MEMBER:
			return FALSE;
		case IDM_TOOL_SPAREADD:
			return (IsFaultTolerant() && !IsMigrationRequired() && IsOperatableAll() && IsHealthy() && DevicesSpare(FALSE)==0);
		case IDM_TOOL_RESET_BIND_INFO:
			return FALSE;
		case IDM_TOOL_CLEAR_DEFECTIVE:
			return FALSE;
		case IDM_TOOL_USE_AS_BASIC:
			return FALSE;
		case IDM_TOOL_REMOVE_FROM_RAID:
			return FALSE;
		default:
			ATLASSERT(FALSE);
			return FALSE;
		}
	} else {
		// Basic disk type.
		switch(nID)
		{
		case IDM_TOOL_BIND:
			return TRUE;
		case IDM_TOOL_UNBIND:
			return FALSE;
		case IDM_TOOL_ADDMIRROR:
			return (IsHDD() && IsOperatableAll());
		case IDM_TOOL_MIGRATE:
			return FALSE;
		case IDM_TOOL_REPLACE_DEVICE:
			return FALSE;
		case IDM_TOOL_USE_AS_MEMBER:
			return FALSE;
		case IDM_TOOL_SPAREADD:
			return FALSE;
		case IDM_TOOL_RESET_BIND_INFO:
			return (IsHDD() && IsOperatable());
		case IDM_TOOL_CLEAR_DEFECTIVE:
			return FALSE;
		case IDM_TOOL_USE_AS_BASIC:
			return FALSE;
		case IDM_TOOL_REMOVE_FROM_RAID:
			return FALSE;
		default:
			ATLASSERT(FALSE);
			return FALSE;
		}
	}
	return FALSE;
}

BOOL CNBLogicalDevice::HixChangeNotify(LPCGUID guid)
{
	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i))
		{
			continue;
		}

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
		if (m_mapUnitDevices[i]->IsMissingMember()) {
			// Cannot touch missing device.
			continue;
		}
		listUnitDevices.push_back(m_mapUnitDevices[i]);
	}

	return listUnitDevices;
}

BOOL CNBLogicalDevice::InitConnectionInfo(PNDASCOMM_CONNECTION_INFO ci, BOOL bWriteAccess)
{
	ATLASSERT(m_mapUnitDevices.size());

	return PrimaryUnitDevice()->InitConnectionInfo(ci, bWriteAccess);
}

BOOL CNBLogicalDevice::InitRaidInfo()
{
	BOOL bResult;
	NDASCOMM_CONNECTION_INFO ci;
	HNDAS hNDAS = NULL;

	bResult = InitConnectionInfo(&ci, FALSE);
	if (!bResult) {
		return FALSE;
	}
		
	if(!(hNDAS = NdasCommConnect(&ci))) {
		return FALSE;
	}
	m_RaidInfo.Size = sizeof(m_RaidInfo);
	bResult = NdasOpGetRaidInfo(hNDAS, &m_RaidInfo);
	if (bResult == FALSE) {
		NdasCommDisconnect(hNDAS);
		return FALSE;
	}
	NdasCommDisconnect(hNDAS);
	return TRUE;	
}

VOID CNBLogicalDevice::UpdateStatus()
{
	// Update status of all members including missing member
	for(UINT32 i = 0; i < DevicesTotal(); i++)
	{
		if(!m_mapUnitDevices.count(i))
			continue;
		m_mapUnitDevices[i]->UpdateStatus();
	}
}

