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

CNBNdasDevice::CNBNdasDevice(LPCTSTR DeviceName, PNDAS_DEVICE_ID pDeviceId, NDAS_DEVICE_STATUS status, ACCESS_MASK GrantedAccess)
{
	m_strDeviceName = DeviceName;
	m_DeviceId = *pDeviceId;
	m_GrantedAccess = GrantedAccess;
	m_status = status;

	if (m_DeviceId.VID == 0) {
		ATLTRACE(_T("Assume VID is 1 if VID=0\n"));		
		m_DeviceId.VID = 1;
	}

	ATLTRACE(_T("new CNBNdasDevice(%p) : Name(%s), Device(%02X:%02X:%02X:%02X:%02X:%02X), VID(%02X), status(%08X), Access(%08X)\n"),
		this,
		m_strDeviceName,
		m_DeviceId.Node[0],
		m_DeviceId.Node[1],
		m_DeviceId.Node[2],
		m_DeviceId.Node[3],
		m_DeviceId.Node[4],
		m_DeviceId.Node[5],
		m_DeviceId.VID,
		m_status,
		m_GrantedAccess);
}

/*
* Copy CNBNdasDevice
*/

CNBNdasDevice::CNBNdasDevice(CNBNdasDevice* SrcDev)
{
	ATLASSERT(SrcDev);

	m_strDeviceName = SrcDev->m_strDeviceName;
	m_DeviceId = SrcDev->m_DeviceId;
	m_status = SrcDev->m_status;
	m_GrantedAccess = SrcDev->m_GrantedAccess;

	ATLTRACE(_T("new CNBNdasDevice(%p) : Name(%s), Device(%02X:%02X:%02X:%02X:%02X:%02X), VID(%02X), status(%08X), Access(%08X)\n"),
		this,
		m_strDeviceName,
		m_DeviceId.Node[0],
		m_DeviceId.Node[1],
		m_DeviceId.Node[2],
		m_DeviceId.Node[3],
		m_DeviceId.Node[4],
		m_DeviceId.Node[5],
		m_DeviceId.VID,
		m_status,
		m_GrantedAccess);
}

CNBNdasDevice::~CNBNdasDevice()
{
	ClearUnitDevices();
}

void
CNBNdasDevice::GetNdasID(NDAS_ID *ndasId)
{
	ATLASSERT(NULL != ndasId);

	/*
	* CNBNdasDevice does not store NDASID_EXT_DATA.
	* Instead,CNBNdasDevice creates NDASID_EXT_DATA using m_DeviceId.VID
	* If there are any other 'variable' in the future except VID, CNBNdasDevice must have it.
	*/
	NDASID_EXT_DATA IdExtData = NDAS_ID_EXTENSION_DEFAULT;
	IdExtData.VID = m_DeviceId.VID;

	ZeroMemory(ndasId, sizeof(NDAS_ID));
	NdasIdDeviceToStringEx(&m_DeviceId, 
		ndasId->Id,
		NULL,
		NULL,
		&IdExtData);
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

BOOL CNBNdasDevice::IsAlive()
{
	return
		(NDAS_DEVICE_STATUS_CONNECTED == m_status ||
		NDAS_DEVICE_STATUS_CONNECTING == m_status);
}

CNBUnitDevice* CNBNdasDevice::AddUnitDevice(
	DWORD UnitNo,
	NDAS_UNITDEVICE_TYPE UnitDeviceType,
	DWORD RaidFlags)
{
	if(NDAS_UNITDEVICE_TYPE_UNKNOWN == UnitDeviceType)
	{
		/*
		* retrieve the type of unit device
		*/
		NDASCOMM_CONNECTION_INFO ci;
		InitConnectionInfo(&ci, FALSE, UnitNo);

		HNDAS hNDAS = NdasCommConnect(&ci);
		ATLASSERT(NULL != hNDAS);
		if(NULL == hNDAS)
		{
			// this is abnormal case, because NDAS device is online.
			UnitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
//			return NULL;
		}
		else
		{
			NDAS_UNITDEVICE_HARDWARE_INFO UnitInfo;
			UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
			if(NdasCommGetUnitDeviceHardwareInfo(hNDAS, &UnitInfo))
			{
				switch(UnitInfo.MediaType)
				{
				case NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE:
					UnitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
					break;
				case NDAS_UNIT_ATAPI_CDROM_DEVICE:
					UnitDeviceType = NDAS_UNITDEVICE_TYPE_CDROM;
					break;
				case NDAS_UNIT_ATAPI_DIRECT_ACCESS_DEVICE:
					UnitDeviceType = NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK;
					break;
				case NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE:
					UnitDeviceType = NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY;
					break;
				case NDAS_UNIT_UNKNOWN_DEVICE:
				default:
					UnitDeviceType = NDAS_UNITDEVICE_TYPE_UNKNOWN;
					break;
				}
			}
			else
			{
				UnitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
			}
		}

		if(hNDAS)
		{
			NdasCommDisconnectEx(hNDAS, 0);
			hNDAS = NULL;
		}
	}

	if(m_mapUnitDevices.count(UnitNo))
	{
		// already exist
		delete m_mapUnitDevices[UnitNo];
		m_mapUnitDevices[UnitNo] = NULL;
	}

	CNBUnitDevice *pUnitDevice = new CNBUnitDevice(UnitNo, UnitDeviceType);
	if(!pUnitDevice)
		return NULL;

	/*
	* complete initialization
	*/
	ATLTRACE(_T(" - (%s)\n"), m_strDeviceName);

	m_mapUnitDevices[UnitNo] = pUnitDevice;
	pUnitDevice->m_pDevice = this;
	pUnitDevice->m_RaidFlags = RaidFlags;

	return pUnitDevice;
}

/*
* creates NDASCOMM_CONNECTION_INFO
*/
BOOL
CNBNdasDevice::InitConnectionInfo(
	NDASCOMM_CONNECTION_INFO *ci,
	BOOL bWriteAccess,
	DWORD UnitNo)
{
	ATLASSERT(ci);

	// write access requires granted write access
	if(!(GENERIC_WRITE & m_GrantedAccess) && bWriteAccess)
	{
		return FALSE;
	}

	ZeroMemory(ci, sizeof(NDASCOMM_CONNECTION_INFO));
	ci->Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci->LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci->UnitNo = UnitNo;
	ci->WriteAccess = bWriteAccess;
	//
	// Following two lines are not necessary, ZeroMemory does these.
	// But just to clarify!
	//
	ci->OEMCode.UI64Value = 0;
	ci->PrivilegedOEMCode.UI64Value = 0;
	ci->Protocol = NDASCOMM_TRANSPORT_LPX;

	ci->AddressType = NDASCOMM_CIT_NDAS_ID;
	GetNdasID(&ci->Address.NdasId);

	return TRUE;
}

CString CNBNdasDevice::GetName()
{
	return m_strDeviceName;
}

/*
* ex: 120.74GB
*/
CString CNBDevice::GetCapacityString(UINT64 ui64capacity)
{
	CString strText;
	UINT64 ui64capacityMB = ui64capacity / (1024 * 1024);

	if(0 == ui64capacityMB)
	{
		strText = _T("");
	}
	else
	{
		strText.FormatMessage(
			IDS_DISK_SIZE_IN_GB,
			(UINT32)(ui64capacityMB / 1024),
			(UINT32)(ui64capacityMB % 1024) * 100 / 1024);
	}

	return strText;
}

UINT
CNBDevice::FindIconIndex(
	UINT idicon,
	const UINT *anIconIDs,
	int nCount,
	int iDefault)
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


CNBUnitDevice::CNBUnitDevice(
	DWORD UnitNo,
	NDAS_UNITDEVICE_TYPE UnitDeviceType)
{
	m_UnitNo = UnitNo;
	m_UnitDeviceType = UnitDeviceType;

	m_pDevice = NULL;
	m_pLogicalDevice = NULL;

	ATLTRACE(
		_T("  new CNBUnitDevice(%p) : UnitNo %d, Type %d"),
		this,
		m_UnitNo,
		m_UnitDeviceType);
}

BOOL
CNBUnitDevice::InitConnectionInfo(
	NDASCOMM_CONNECTION_INFO *ci,
	BOOL bWriteAccess)
{
	ATLASSERT(NULL != ci);
	ATLASSERT(NULL != m_pDevice);

	return m_pDevice->InitConnectionInfo(ci, bWriteAccess, m_UnitNo);
}

CString CNBUnitDevice::GetCapacityString()
{
	return CNBDevice::GetCapacityString(GetPhysicalCapacityInByte());
}

BOOL CNBUnitDevice::Equals(BYTE Node[6], BYTE Vid, DWORD UnitNo)
{
	return
		(
			!memcmp(
				m_pDevice->m_DeviceId.Node,
				Node,
				sizeof(m_pDevice->m_DeviceId.Node))
		) &&
		(m_pDevice->m_DeviceId.VID == Vid) &&
		(m_UnitNo == UnitNo);
}

BOOL CNBUnitDevice::Initialize(BOOL MissingInLogical)
{
	BOOL bReturn = FALSE;
	NDASCOMM_CONNECTION_INFO ci;
	HNDAS hNDAS = NULL;
	UINT32 nDIBSize = sizeof(m_DIB);
	NDAS_UNITDEVICE_HARDWARE_INFOW UnitInfo;

	m_PhysicalCapacity = 0;
	
	m_definition.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);
	if(FAILED(NdasVsmInitializeLogicalUnitDefinition(
		&m_pDevice->m_DeviceId,
		m_UnitNo,
		&m_definition)))
	{
		goto out;
	}

	m_bMissingMember = MissingInLogical;
	if (m_bMissingMember) 
	{
		goto out;
	}

	UpdateStatus();

	if(NDAS_UNITDEVICE_TYPE_UNKNOWN == m_UnitDeviceType)
	{
		m_DIB.iMediaType = NMT_INVALID;
		m_DIB.nDiskCount = 1;
		// We don't need to connect to unknown device.
		return TRUE;
	}

	if(!m_pDevice->IsAlive())
		goto out;

	if(!InitConnectionInfo(&ci, FALSE))
		goto out;
	
	if(!(hNDAS = NdasCommConnect(&ci)))
		goto out;

	::ZeroMemory(&UnitInfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));
	UnitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	bReturn = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &UnitInfo);
	if (!bReturn)
		goto out;

	m_PhysicalCapacity =
		(UnitInfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA) * SECTOR_SIZE;
	
	// set m_SequenceInDib and m_cSequenceInRMD
	if (m_bMissingMember)
	{
		// This disk's DIB and RMD is not used.
	}
	else
	{
		m_definition.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);
		if(FAILED(NdasVsmReadLogicalUnitDefinition(hNDAS, &m_definition)))
			goto out;

		if(!NdasOpReadDIB(hNDAS, &m_DIB, &nDIBSize))
			goto out;
		m_SequenceInDib = m_DIB.iSequence;
		
		if (NMT_RAID1R2 == m_DIB.iMediaType ||
			NMT_RAID4R2 == m_DIB.iMediaType ||
			NMT_RAID1R3 == m_DIB.iMediaType ||
			NMT_RAID4R3 == m_DIB.iMediaType ||
			NMT_RAID5 == m_DIB.iMediaType )
		{
			if (!NdasCommBlockDeviceRead(
				hNDAS,
				NDAS_BLOCK_LOCATION_RMD,
				1,
				(PBYTE)&m_RMD))
			{
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
	if (!bReturn)
	{
		// We may be able to connect to device when enumerating but failed to connect or read DIB here.
		m_DIB.iMediaType = NMT_INVALID;
		m_DIB.nDiskCount = 1;
		m_SequenceInDib = 0;
	}

	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
	}

	return bReturn;
}

/*
* Broadcast HIX 'Unit device changed'
*/
void CNBUnitDevice::HixChangeNotify(LPCGUID guid)
{
	NDAS_ID ndasId;
	m_pDevice->GetNdasID(&ndasId);
	if (m_pDevice)
	{
		NdasCommNotifyUnitDeviceChange(
			NDAS_DIC_NDAS_ID,
			ndasId.Id,
			m_UnitNo,
			guid);
	}
}

BOOL CNBUnitDevice::IsDefective()
{
	if (!IsGroup())
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
}

BOOL CNBUnitDevice::IsSpare()
{
	if(!IsGroup())
		return FALSE;

	if (GetRaidStatus() & NDAS_RAID_MEMBER_FLAG_SPARE)
		return TRUE;

	return FALSE;
}

BOOL CNBUnitDevice::IsCommandAvailable(int nID)
{
	switch(nID)
	{
	case IDM_TOOL_BIND:
		return TRUE;
	case IDM_TOOL_UNBIND:
		return FALSE;
	case IDM_TOOL_ADDMIRROR: // Should be single logical device
		return FALSE;
	case IDM_TOOL_APPEND:
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
		return FALSE;		//	case IDM_TOOL_SINGLE:
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
	case IDM_TOOL_FIX_RAID_STATE:
		return FALSE;
	default:
		ATLASSERT(FALSE);
		return TRUE;
	}
}

UINT32 CNBUnitDevice::GetType()
{
	if (m_pLogicalDevice)
	{
		return m_pLogicalDevice->GetType();
	}

	if (m_pDevice == NULL || !m_pDevice->IsAlive())
	{
		return NMT_INVALID;
	}

	return m_DIB.iMediaType;
}

CString CNBUnitDevice::GetName()
{
	CString strText;
	if (m_pDevice) {
		if(1 != m_pDevice->UnitDevicesCount() || m_UnitNo>0)
		{
			strText.Format(_T("%s:%d"), m_pDevice->GetName(), m_UnitNo + 1);
		}
		else
		{
			strText.Format(_T("%s"), m_pDevice->GetName());
		}
	} else {
		strText = "";
	}
	return strText;
}

UINT CNBUnitDevice::GetIconIndex(const UINT *anIconIDs, int nCount)
{
	if (IsMissingMember())
		return FindIconIndex(IDI_DEVICE_INVALID, anIconIDs, nCount);
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
	case NMT_RAID5:		
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
	case NMT_RAID5:		
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_SAFE_RAID1:
	case NMT_CONFLICT:
		return TRUE;
	case NMT_INVALID:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
		return FALSE;
	}
	return TRUE;
}

CString CNBUnitDevice::GetIDString(TCHAR HiddenChar)
{
	CString strText;
	if (m_pDevice) {
		NDAS_ID ndasId;
		m_pDevice->GetNdasID(&ndasId);
		CString strID = ndasId.Id;
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
	if(!(GENERIC_WRITE & m_pDevice->m_GrantedAccess))
	{
		status |= NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY;
	}

	// disconnected
	if(!m_pDevice->IsAlive())
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
		} else if (0 == udstat.RWHostCount && NDAS_HOST_COUNT_UNKNOWN == udstat.ROHostCount) {
			status |= NDASBIND_UNIT_DEVICE_STATUS_UNKNOWN_RO_MOUNT;
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

	if (IsMissingMember())
	{
		
		strText.LoadString(IDS_STATUS_INVALID);
	}
	else if(NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED & status)
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
	case NMT_RAID5:		
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
	case NMT_RAID5: 		
		// Handle serious problem first
		if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_RMD_CORRUPTED) {
			strText.LoadString(IDS_RAID_STATUS_CORRUPTED_INFO);
		} else if (m_RaidFlags & NDAS_RAID_MEMBER_FLAG_DEGRADE_MODE_CONFLICT) {
			// Hardly happen??
			strText.LoadString(IDS_RAID_STATUS_DEGRADE_MODE_CONFLICT);
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
	case NMT_RAID5:		
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
	return m_pDevice->m_GrantedAccess;
}

BOOL CNBUnitDevice::IsOperatable()
{
	if(!IsHDD())
		return FALSE;

	if((NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED |
		NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY |
		NDASBIND_UNIT_DEVICE_STATUS_MOUNTED) &
		GetStatus())
		return FALSE;

	if(NDAS_RAID_MEMBER_FLAG_DIFFERENT_RAID_SET & m_RaidFlags)
		return FALSE;

	return TRUE;
}

CNBLogicalDevice::CNBLogicalDevice()
{
}

CNBLogicalDevice::~CNBLogicalDevice()
{
	for(NBUnitDevicePtrMap::iterator itUnitDevice = m_mapUnitDevices.begin();
		itUnitDevice != m_mapUnitDevices.end(); itUnitDevice++)
	{
		itUnitDevice->second->m_pLogicalDevice = NULL;
		itUnitDevice->second->m_SequenceInDib = 0;
	}
}

BOOL CNBLogicalDevice::IsGroup()
{
	ATLASSERT(m_mapUnitDevices.size());

	return PrimaryUnitDevice()->IsGroup();
}
#if 0
BOOL CNBLogicalDevice::IsKnownType()
{
	ATLASSERT(m_mapUnitDevices.size());

	switch(GetType()) {
	case NMT_SINGLE:
	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID1:
	case NMT_RAID4:
	case NMT_RAID0:
	case NMT_SAFE_RAID1:
	case NMT_AOD:
	case NMT_RAID1R2:
	case NMT_RAID4R2:
	case NMT_RAID1R3:
	case NMT_RAID4R3:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	case NMT_CONFLICT:
		return TRUE;
	case NMT_INVALID:
	default:
		return FALSE;	
	}
}
#endif
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
			case NMT_RAID5: strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID5); break;
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
		NMT_RAID5,      IDI_DEVICE_BOUND,		
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

	if(!pUnitDevice->IsGroup())
		return FALSE;

	return (0 == memcmp(
		&pUnitDevice->m_definition,
		&PrimaryUnitDevice()->m_definition,
		sizeof(NDAS_LOGICALUNIT_DEFINITION)));
}

void CNBLogicalDevice::UnitDeviceSet(CNBUnitDevice *pUnitDevice, UINT32 SequenceNum)
{
	NDAS_DEVICE_STATUS DeviceStatus;
	
	if (!m_mapUnitDevices.size()) {
		// Assume as primary if inserted first.
		// Set identify of this logical device
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
}

CNBUnitDevice *CNBLogicalDevice::PrimaryUnitDevice()
{
	ATLASSERT(m_mapUnitDevices.size());

	return m_PrimaryUnit;
}

UINT32 CNBLogicalDevice::DevicesTotal(BOOL bAliveOnly)
{
	ATLASSERT(m_mapUnitDevices.size());

	UINT32 total = 0;

	if(bAliveOnly)
	{		
		UINT32 nDevicesTotal = DevicesTotal(FALSE);
		for(UINT32 i = 0; i < nDevicesTotal; i++)
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
			UINT32 nDevicesTotal = DevicesTotal(FALSE);
			for(UINT32 i = 0; i < nDevicesTotal; i++)
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
	case NMT_RAID5:
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
		UINT32 nDevicesTotal = DevicesTotal(FALSE);
		for(UINT32 i = 0; i < nDevicesTotal; i++)
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
	case NMT_RAID5: 		
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
				NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT |
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
	case NMT_RAID5: 		
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
				NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT |
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
		strFmt.LoadString(IDS_RAID_COMMENT_UNKNOWN_TYPE_FMT);
		menuStr1 = pGetMenuString(IDM_TOOL_RESET_BIND_INFO);
		strText.FormatMessage(strFmt, menuStr1);
		break;
	}
	return strText;
}


BOOL CNBLogicalDevice::IsHDD()
{
	ATLASSERT(m_mapUnitDevices.size());

	return PrimaryUnitDevice()->IsHDD();
}

UINT32 CNBLogicalDevice::GetOperatableCount()
{
	if(!IsHDD())
		return 0;

	if((NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED | 
		NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY) &
		GetStatus())
		return 0;

	UINT32 uiOperatable = 0;
	// At least one member should be connected
	UINT32 nDevicesTotal = DevicesTotal(FALSE);
	for(UINT32 i = 0; i < nDevicesTotal; i++)
	{
		if(m_mapUnitDevices.count(i) && m_mapUnitDevices[i]->IsOperatable())
		{
			uiOperatable++;
		}
	}

	return uiOperatable;
}

BOOL CNBLogicalDevice::IsOperatable()
{
	return (GetOperatableCount() > 0);
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
	return (GetOperatableCount() == DevicesTotal(FALSE));
}

BOOL CNBLogicalDevice::HasMissingMember()
{
	UINT32 nDevicesTotal = DevicesTotal(FALSE);
	for(UINT32 i = 0; i < nDevicesTotal; i++)
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

BOOL CNBLogicalDevice::IsFixRaidStateRequired()
{
	BOOL bResult;
	NDASCOMM_CONNECTION_INFO ci;
	bResult = InitConnectionInfo(&ci, TRUE);

	if(GetOperatableCount() < DevicesTotal(FALSE) -1)
	{
		// RAID 3, 4, 5 requires n-1 at least
		return FALSE;
	}

	switch(GetType())
	{
	case NMT_RAID1R3:
		bResult = NdasOpRAID1R3IsFixRequired(&ci, FALSE);
		break;
	case NMT_RAID4R3:
	case NMT_RAID5:
		bResult = NdasOpRAID4R3or5IsFixRequired(&ci, FALSE);
		break;
	default:
		bResult = FALSE;
	}

	return bResult;
}

BOOL CNBLogicalDevice::IsCommandAvailable(int nID)
{
	if (IsGroup()) {
		switch(nID)
		{
		case IDM_TOOL_BIND:
			return TRUE;
		case IDM_TOOL_UNBIND:
			// Cannot unbind devices in use.
//			return !(GetStatus() & NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED);
			return TRUE; // Try to unbind whatever state it is
		case IDM_TOOL_ADDMIRROR:
			return FALSE;
		case IDM_TOOL_MIGRATE:
			return (
				IsHDD() && IsMigrationRequired() && IsOperatableAll() && !HasMissingMember());
		case IDM_TOOL_REPLACE_DEVICE:
			return FALSE;
		case IDM_TOOL_USE_AS_MEMBER:
			return FALSE;
		case IDM_TOOL_APPEND:
			return (GetType() == NMT_AGGREGATE && !IsMigrationRequired() && IsOperatableAll() && IsHealthy());
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
		case IDM_TOOL_FIX_RAID_STATE:
			return IsOperatable() && IsFixRaidStateRequired();
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
			return (IsHDD() && IsOperatableAll() && (GetType() != NMT_CONFLICT));
		case IDM_TOOL_APPEND:
			return (IsHDD() && IsOperatableAll() && (GetType() != NMT_CONFLICT));
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
		case IDM_TOOL_FIX_RAID_STATE:
			return FALSE;
		default:
			ATLASSERT(FALSE);
			return FALSE;
		}
	}
	return FALSE;
}

void CNBLogicalDevice::HixChangeNotify(LPCGUID guid)
{
	UINT32 nDevicesTotal = DevicesTotal(FALSE);
	for(UINT32 i = 0; i < nDevicesTotal; i++)
	{
		if(!m_mapUnitDevices.count(i))
		{
			continue;
		}

		m_mapUnitDevices[i]->HixChangeNotify(guid);
	}
}

NBUnitDevicePtrList CNBLogicalDevice::GetOperatableDevices()
{
	NBUnitDevicePtrList listUnitDevices;

	UINT32 nDevicesTotal = DevicesTotal(FALSE);
	for(UINT32 i = 0; i < nDevicesTotal; i++)
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

BOOL CNBLogicalDevice::InitConnectionInfo(NDASCOMM_CONNECTION_INFO *ci, BOOL bWriteAccess)
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
	UINT32 nDevicesTotal = DevicesTotal(FALSE);
	for(UINT32 i = 0; i < nDevicesTotal; i++)
	{
		if(!m_mapUnitDevices.count(i))
			continue;
		m_mapUnitDevices[i]->UpdateStatus();
	}
}

