/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <boost/mem_fn.hpp>
#include <scrc32.h>
#include <lfsfiltctl.h>
#include <ndasbusctl.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasop.h>
#include "ndasdevreg.h"
#include "ndasdev.h"
#include "ndaslogdev.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "lpxcomm.h"
#include "ndascfg.h"
#include "ndasunitdevfactory.h"
#include "ndaspnp.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaslogdev.tmh"
#endif

namespace
{

bool
pIsValidStatusChange(
	NDAS_LOGICALDEVICE_STATUS oldStatus,
	NDAS_LOGICALDEVICE_STATUS newStatus);

UCHAR
LogicalDeviceTypeToNdasBusTargetType(
	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType);

struct AutoDeviceInfoSetConfig {
	static HDEVINFO GetInvalidValue() { return (HDEVINFO) INVALID_HANDLE_VALUE; }
	static void Release(HDEVINFO h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::SetupDiDestroyDeviceInfoList(h);
		XTLASSERT(fSuccess);
		::SetLastError(dwError);
	}
};

typedef XTL::AutoResourceT<HDEVINFO,AutoDeviceInfoSetConfig> AutoDeviceInfoSet;

DWORD 
GetDeviceUINumber(
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

BOOL pRequestEject(
	DWORD SlotNo,
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName, 
	DWORD nNameLength)
{
	// Get devices under Enum\NDAS
	AutoDeviceInfoSet deviceInfoSet = ::SetupDiGetClassDevs(
		NULL,  
		_T("NDAS"),
		NULL, 
		DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (INVALID_HANDLE_VALUE == (HDEVINFO) deviceInfoSet)
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
		DWORD uiNumber = GetDeviceUINumber(deviceInfoSet, &deviceInfoData);
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

} // namespace

//
// Constructor for a multiple member logical device
//
CNdasLogicalDevice::CNdasLogicalDevice(
	NDAS_LOGICALDEVICE_ID logDeviceId, 
	const NDAS_LOGICALDEVICE_GROUP& ldGroup) :
	CStringizerA<32>("LD.%02d", logDeviceId),
	m_logicalDeviceId(logDeviceId),
	m_status(NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED),
	m_lastError(NDAS_LOGICALDEVICE_ERROR_NONE),
	m_dwMountedDriveSet(0),
	m_MountedAccess(0),
	m_bReconnecting(FALSE),
	m_fDisconnected(FALSE),
	m_fMountOnReady(FALSE),
	m_fReducedMountOnReadyAccess(FALSE),
	m_mountOnReadyAccess(0),
	m_fRiskyMount(FALSE),
	m_dwMountTick(0),
	m_dwCurrentMRB(0),
	m_logicalDeviceGroup(ldGroup),
	m_NdasScsiLocation(0),
	m_fShutdown(FALSE),
	m_ulAdapterStatus(NDASSCSI_ADAPTERINFO_STATUS_INIT)
{
	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		__FUNCTION__ " %s\n", ToStringA());
}

//
// Destructor
//
CNdasLogicalDevice::~CNdasLogicalDevice()
{
	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		__FUNCTION__ " %s\n", ToStringA());
}

BOOL
CNdasLogicalDevice::Initialize()
{
	if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED != m_status) 
	{
		XTLASSERT(FALSE);
		// Already initialized
		return TRUE;
	}

	// Locate Registry Container based on its hash value
	_LocateRegContainer();

	if (m_hDisconnectedEvent.IsInvalid())
	{
		m_hDisconnectedEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hDisconnectedEvent.IsInvalid()) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"CreateEvent(Disconnect) failed, error=0x%X\n", GetLastError());
			return FALSE;
		}
	}

	if (m_hAlarmEvent.IsInvalid()) 
	{
		m_hAlarmEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hAlarmEvent.IsInvalid()) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"CreateEvent(Alarm) failed, error=0x%X\n", GetLastError());
			XTLVERIFY(CloseHandle(m_hDisconnectedEvent));
			return FALSE;
		}
	}

	ACCESS_MASK lastMountAccess = GetLastMountAccess();

	BOOL fRiskyMountFlag = GetRiskyMountFlag();

	if (fRiskyMountFlag) 
	{
		m_fRiskyMount = fRiskyMountFlag;
	}

	if ((lastMountAccess > 0) && !_IsRiskyMount()) 
	{
		SetMountOnReady(lastMountAccess, FALSE);
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Logical Device %d initialized successfully.\n", m_logicalDeviceId);

	return TRUE;
}

NDAS_LOGICALDEVICE_ID
CNdasLogicalDevice::GetLogicalDeviceId() const
{
	return m_logicalDeviceId;
}

HANDLE
CNdasLogicalDevice::GetDisconnectEvent() const
{
	return m_hDisconnectedEvent;
}

HANDLE
CNdasLogicalDevice::GetAlarmEvent() const
{
	return m_hAlarmEvent;
}

//
// Get the defined maximum number of unit devices
//
DWORD 
CNdasLogicalDevice::GetUnitDeviceCount() const
{
	return m_logicalDeviceGroup.nUnitDevices;
}

DWORD 
CNdasLogicalDevice::GetUnitDeviceCountSpare() const
{
	return m_logicalDeviceGroup.nUnitDevicesSpare;
}

DWORD 
CNdasLogicalDevice::GetUnitDeviceCountInRaid() const
{
	return GetUnitDeviceCount() - GetUnitDeviceCountSpare();
}


//
// Get the unit device ID in i-th sequence.
//
const NDAS_UNITDEVICE_ID&
CNdasLogicalDevice::GetUnitDeviceID(DWORD ldSequence) const
{
	XTLASSERT(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	return m_logicalDeviceGroup.UnitDevices[ldSequence];
}

//
// Get the unit device of i-th sequence
//
CNdasUnitDevicePtr
CNdasLogicalDevice::GetUnitDevice(DWORD ldSequence) const
{
	XTLASSERT(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices)
	{
		return CNdasUnitDeviceNullPtr;
	}
	return pGetNdasUnitDevice(m_logicalDeviceGroup.UnitDevices[ldSequence]);
}

const NDAS_LOGICALDEVICE_GROUP&
CNdasLogicalDevice::GetLDGroup() const
{
	return m_logicalDeviceGroup;
}

NDAS_LOGICALDEVICE_TYPE
CNdasLogicalDevice::GetType() const
{
	return m_logicalDeviceGroup.Type;
}


ULONG
CNdasLogicalDevice::GetAdapterStatus()
{
	InstanceAutoLock autolock(this);
	return m_ulAdapterStatus;
}

ULONG
CNdasLogicalDevice::SetAdapterStatus(ULONG ulAdapterStatus)
{
	InstanceAutoLock autolock(this);
	ULONG ulOldAdapterStatus;
	ulOldAdapterStatus = m_ulAdapterStatus;
	m_ulAdapterStatus = ulAdapterStatus;
	return ulOldAdapterStatus;
}

BOOL 
CNdasLogicalDevice::CheckMountability(
	NDAS_RAID_MOUNTABILITY_FLAGS* Mountablity, 
	NDAS_RAID_FAIL_REASON* FailReason
) {
	BOOL bResult;
	NDASCOMM_CONNECTION_INFO ci;
	HNDAS hNDAS = NULL;
	DWORD devCount;
	DWORD UnitCount;
	DWORD i;
	CNdasUnitDevicePtr UnitDev;
	CNdasDevicePtr NdasDev;
	NDASOP_RAID_INFO RaidInfo;

	if (!_IsRaid()) {
		if (GetUnitDeviceInstanceCount() ==0) {
			*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			m_fMountable = FALSE;
		} else {
			*Mountablity = NDAS_RAID_MOUNTABILITY_MOUNTABLE;
			m_fMountable = TRUE;
			UnitDev = GetUnitDevice(0);
		}
		*FailReason = NDAS_RAID_FAIL_REASON_NONE;
		m_fDegradedMode = FALSE;
		return TRUE;
	}
	UnitCount = GetUnitDeviceInstanceCount();
	if (UnitCount ==0) {
		// No unit device.
		*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		*FailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;		
		return TRUE;
	}
	// Connect to primary device

	UnitDev = GetPrimaryUnit();
	if (CNdasUnitDeviceNullPtr == UnitDev) {
		*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		*FailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;
		return TRUE;
	}

	ZeroMemory(&ci, sizeof(NDASCOMM_CONNECTION_INFO));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.UnitNo = UnitDev->GetUnitNo();
	ci.WriteAccess = FALSE;
	ci.OEMCode.UI64Value = UnitDev->GetDevicePassword();
	ci.PrivilegedOEMCode.UI64Value = 0;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.Address.DeviceId = UnitDev->GetUnitDeviceId().DeviceId;
	hNDAS = NdasCommConnect(&ci);

	if (hNDAS == NULL) {
		*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		*FailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;	
		goto discon_out;	
	}
	
	RaidInfo.Size = sizeof(RaidInfo); // To check version.
	bResult = NdasOpGetRaidInfo(hNDAS, &RaidInfo);
	if (!bResult) {
		// NdasOpGetRaidInfo returns FALSE only for internal error such as version mismatch, memory allocation failure.
		XTLASSERT(FALSE);
		*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		*FailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;	
		goto discon_out;			
	}

	if ((RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE) &&
		(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION))
	{
		// DIB version is higher than this version can handle.
		*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
		*FailReason = NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;		
		goto discon_out;
	}
	//
	// Check all device is registered and not disabled.
	// 
	for(i=0;i<RaidInfo.MemberCount;i++) {
		NdasDev = pGetNdasDevice(RaidInfo.Members[i].DeviceId);
		if (NdasDev == NULL) {
			// Member is not registered
			*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			*FailReason = NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;				
			goto discon_out;	
		}
		
		if (NdasDev ->GetStatus() == NDAS_DEVICE_STATUS_DISABLED) {
			*Mountablity = NDAS_RAID_MOUNTABILITY_UNMOUNTABLE;
			*FailReason = NDAS_RAID_FAIL_REASON_MEMBER_DISABLED;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	
			goto discon_out;
		}
	}
	*Mountablity = (NDAS_RAID_MOUNTABILITY_FLAGS) RaidInfo.MountablityFlags;
	*FailReason = (NDAS_RAID_FAIL_REASON) RaidInfo.FailReason;

	m_fMountable  = (RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE)?TRUE:FALSE;
	m_fDegradedMode = (RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_DEGRADED)?TRUE:FALSE;

	// To do: check online member is also recognized as online by svc, too
discon_out:	
	if (hNDAS) {
		NdasCommDisconnect(hNDAS);
	}
	return TRUE;	
}

//
// Set the unit device ID at a sequence 
// to a unit device member ID list
//
BOOL 
CNdasLogicalDevice::AddUnitDevice(CNdasUnitDevicePtr pUnitDevice)
{
	InstanceAutoLock autolock(this);
	BOOL fSuccess;
	NDAS_RAID_MOUNTABILITY_FLAGS RaidMountablity;
	NDAS_RAID_FAIL_REASON RaidFailReason;

	DWORD ldSequence = pUnitDevice->GetLDSequence();

	XTLASSERT(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Invalid unit device sequence, seq=%d, gcount=%d\n", 
			ldSequence, m_logicalDeviceGroup.nUnitDevices);
		return FALSE;
	}

	//
	// Check for multiple add unit device calls
	//
	{
		CNdasUnitDeviceVector unitDevices(m_unitDevices.size());
		std::transform(
			m_unitDevices.begin(), m_unitDevices.end(),
			unitDevices.begin(),
			weak_ptr_to_shared_ptr<CNdasUnitDevice>());
		CNdasUnitDeviceVector::const_iterator itr = 
			std::find(
				unitDevices.begin(), unitDevices.end(),
				pUnitDevice);
		if (unitDevices.end() != itr)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Duplicate unit device sequence, seq=%d\n", ldSequence); 
			XTLASSERT(FALSE && "Duplicate calls to AddUnitDevice");
			return FALSE;
		}
	}

	m_unitDevices.push_back(pUnitDevice);

	CheckMountability(&RaidMountablity, &RaidFailReason);
	
	if (IsMountable())
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
	}
	else
	{
		// Handle serious case first
		if (RaidFailReason & (
			NDAS_RAID_FAIL_REASON_RMD_CORRUPTED |
			NDAS_RAID_FAIL_REASON_DIB_MISMATCH |
			NDAS_RAID_FAIL_REASON_DIFFERENT_CONFIG_SET |
			NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION |
			NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID |
			NDAS_RAID_FAIL_REASON_DEFECTIVE |
			NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET |
			NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL |
			NDAS_RAID_FAIL_REASON_NOT_A_RAID |
			NDAS_RAID_FAIL_REASON_INDEPENDENT_UPDATE
			)) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
		} 
		else if (RaidFailReason & (
			NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE |
			NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED |
			NDAS_RAID_FAIL_REASON_MEMBER_DISABLED
			))
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		} 
		else if (RaidFailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED)
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_UPGRADE);
		} 
		else 
		{
			// Should not happen.
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}
		
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
		_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
		return TRUE;
	}		
	//
	// Disk is mountable but slot is not allocated yet.
	//
	if (IsMountable() && !GetNdasScsiLocation().SlotNo) 
	{
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		// Allocate NdasScsiLocation when mountable.
		_AllocateNdasScsiLocation();
		fSuccess = manager.RegisterNdasScsiLocation(m_NdasScsiLocation, shared_from_this());
		if (!fSuccess) 
		{
			//
			// This NdasScisiLocation is already used. User need to resolve this situation using bindtool
			//
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
			_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			//
			// To do: remove already registered ndasscsilocation. 
			//			
			return TRUE;
		}
	}

	if (IsMountable()) 
	{
		if (m_fDegradedMode) {
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE);
		} else {
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}

		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
		
		//
		// Reconciliation Support
		//
		BOOL bAlive, bAdapterError;
		fSuccess = ::LsBusCtlQueryNodeAlive(
			m_NdasScsiLocation.SlotNo, 
			&bAlive, 
			&bAdapterError);

		if (fSuccess && bAlive)
		{
			//
			// Reconciliation
			//
			ReconcileFromNdasBus();
			// This device is already mounted. Change unit device's status
//			pUnitDevice->OnMounted();
		}
		else if (m_fMountOnReady) 
		{
			if (IsComplete() && !m_fDegradedMode) {
				//
				// Try to mount only in not degraded mode.
				// (RAID can be in degraded mode even if the RAID member is complete, in case that disk has same DIB, different RAID set ID)
				//
				// If the NDAS Bus do not have the pdo,
				// the NDAS service will mount the device
				//
				if ((m_mountOnReadyAccess & GetAllowingAccess()) == m_mountOnReadyAccess)
				{
					DWORD LdpfFlags = 0, LdpfValues = 0;
					if (!GetLastLdpf(LdpfFlags, LdpfValues))
					{
						LdpfFlags = LdpfValues = 0;
					}

					fSuccess = PlugIn(m_mountOnReadyAccess, LdpfFlags, LdpfValues);

					if (fSuccess)
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
							"Boot-time mount (%s) succeeded.\n", this->ToStringA());
					}
					else
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
							"Boot-time mount (%s) failed, error=0x%X\n", 
							this->ToStringA(), GetLastError());
					}

				} 
				else if ((GENERIC_READ & GetAllowingAccess()) && m_fReducedMountOnReadyAccess) 
				{
					// When RW access is not available, we will mount the device as RO.
					// if ReducedMountOnReadyAccess is true.
					DWORD LdpfFlags = 0, LdpfValues = 0;
					if (!GetLastLdpf(LdpfFlags, LdpfValues))
					{
						LdpfFlags = LdpfValues = 0;
					}

					fSuccess = PlugIn(GENERIC_READ, LdpfFlags, LdpfValues);

					if (fSuccess)
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
							"RO Boot-time mount (%s) succeeded.\n", 
							this->ToStringA());
					}
					else
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
							"RO Boot-time mount (%s) failed, error=0x%X\n", 
							this->ToStringA(), GetLastError());
					}
				}
			} else {
				//
				// Do not mount if member is missing. 
				// To do: add option to allow automatic mount in degraded mode.
			}
		}
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Added %s to Logical Device %s\n", 
		pUnitDevice->ToStringA(), 
		this->ToStringA());

	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	epub.LogicalDeviceRelationChanged(m_logicalDeviceId);

	return TRUE;
}

//
// Remove the unit device ID from the list
//
BOOL
CNdasLogicalDevice::RemoveUnitDevice(CNdasUnitDevicePtr pUnitDevice)
{
	InstanceAutoLock autolock(this);

	DWORD ldSequence = pUnitDevice->GetLDSequence();
	NDAS_RAID_MOUNTABILITY_FLAGS RaidMoutablity;
	NDAS_RAID_FAIL_REASON RaidFailReason;
	DWORD PrevMountablity = m_fMountable;

	XTLASSERT(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Invalid sequence (%d) of the unit device.\n", ldSequence);
		return FALSE;
	}

	CNdasUnitDeviceVector unitDevices(m_unitDevices.size());

	std::transform(
		m_unitDevices.begin(), m_unitDevices.end(),
		unitDevices.begin(),
		weak_ptr_to_shared_ptr<CNdasUnitDevice>());

	CNdasUnitDeviceVector::iterator itr = 
		std::find(
			unitDevices.begin(), unitDevices.end(),
			pUnitDevice);

	if (unitDevices.end() == itr)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Unit device in sequence (%d) is not occupied.\n", ldSequence);
		XTLASSERT(FALSE && "RemoveUnitDevice called for non-registered");
		return FALSE;
	}

	m_unitDevices.erase(
		m_unitDevices.begin() + 
		std::distance(unitDevices.begin(), itr));

	CheckMountability(&RaidMoutablity, &RaidFailReason);

	//
	// Remove m_NdasScsiLocation if this logical device is not mounted and unmountable anymore
	//
	if (GetNdasScsiLocation().SlotNo && !IsMountable() && 
		(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != GetStatus() &&
		NDAS_LOGICALDEVICE_STATUS_MOUNTED != GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING != GetStatus()))
	{
		// If the previous status was complete and now is not. unregister ndasscsi location
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		manager.UnregisterNdasScsiLocation(m_NdasScsiLocation, shared_from_this());
		_DeallocateNdasScsiLocation();
	}

	//
	// Set Device Error
	//
	if (IsMountable()) {
		if (m_fDegradedMode) {
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE);
		} else {
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}
	} else {
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
	}

	//
	// Publish Event
	//
	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceRelationChanged(m_logicalDeviceId);

	return TRUE;
}

//
// Get the unit device instance count
//
DWORD 
CNdasLogicalDevice::GetUnitDeviceInstanceCount()
{
	InstanceAutoLock autolock(this);
	DWORD instances = static_cast<DWORD>(m_unitDevices.size());
	return instances;
}

BOOL
CNdasLogicalDevice::IsComplete()
{
	InstanceAutoLock autolock(this);
	bool complete = (m_logicalDeviceGroup.nUnitDevices == m_unitDevices.size());
	return complete;
}

BOOL
CNdasLogicalDevice::IsMountable()
{
	InstanceAutoLock autolock(this);
	return m_fMountable;
}

void 
CNdasLogicalDevice::_SetStatus(NDAS_LOGICALDEVICE_STATUS newStatus)
{
	InstanceAutoLock autolock(this);

	NDAS_LOGICALDEVICE_STATUS oldStatus = m_status;

	//
	// Ignore duplicate status change(except UNMOUNTED to propagate last error code changes)
	//
	if (oldStatus == newStatus && 
		newStatus != NDAS_LOGICALDEVICE_STATUS_UNMOUNTED &&
		newStatus != NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
	{
		return;
	}

	XTLVERIFY( pIsValidStatusChange(oldStatus, newStatus) );

	const DWORD LDS_NOT_INIT = NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED;
	const DWORD LDS_UNMOUNTED = NDAS_LOGICALDEVICE_STATUS_UNMOUNTED;
	const DWORD LDS_MOUNTED = NDAS_LOGICALDEVICE_STATUS_MOUNTED;
	const DWORD LDS_MOUNT_PENDING = NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING;
	const DWORD LDS_UNMOUNT_PENDING = NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING;
//	const DWORD LDS_NOT_MOUNTABLE = NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE;

	//
	// Attaching to the event monitor
	//
    //              +--   +--------------------------------+ 
	//              | |   | (D)                            | (A)
	//              ^ V   V         (A)                    v
	// NOT_INIT --> UNMOUNTED <----> MOUNT_PENDING --> MOUNTED
	//               ^     (D)                        ^   ^ v
	//               |   (D)                          |   | |
	//               +-------- UNMOUNT_PENDING  <-----+   +-+
	//
	if ((LDS_UNMOUNTED == oldStatus && LDS_MOUNT_PENDING == newStatus) ||
		(LDS_UNMOUNTED == oldStatus && LDS_MOUNTED == newStatus))
	{
		// Attach to the event monitor
		CNdasEventMonitor& emon = pGetNdasEventMonitor();
		emon.Attach(shared_from_this());
	}
	else if (LDS_UNMOUNTED == newStatus) 
	{
		if (LDS_UNMOUNTED == oldStatus) {
			// Nothing to do. Already detached.
		} else if (LDS_NOT_INIT != oldStatus) 
		{
			// Detach from the event monitor
			CNdasEventMonitor& emon = pGetNdasEventMonitor();
			emon.Detach(shared_from_this());
		}
	}
	m_status = newStatus;

	//
	// Unit Device Notification
	//
    //                 +--------------------------------+ 
	//                 | U                              |M
	//                 V                                v
	// NOT_INIT --> UNMOUNTED <----> MOUNT_PENDING --> MOUNTED
	//              ^  ^ U        M                     ^
	//              |  |                                |
	//              |  +-------- UNMOUNT_PENDING  <-----+
	//              V
	//        NOT_MOUNTABLE
	//

	if ((LDS_UNMOUNTED == oldStatus && LDS_MOUNT_PENDING == newStatus) ||
		(LDS_UNMOUNTED == oldStatus && LDS_MOUNTED == newStatus) ||
		(LDS_MOUNTED == oldStatus && LDS_MOUNTED == newStatus))
	{
		// MOUNTED
		CNdasUnitDeviceVector unitDevices(m_unitDevices.size());
		std::transform(
			m_unitDevices.begin(), m_unitDevices.end(),
			unitDevices.begin(),
			weak_ptr_to_shared_ptr<CNdasUnitDevice>());
		std::for_each(
			unitDevices.begin(), unitDevices.end(),
			boost::mem_fn(&CNdasUnitDevice::OnMounted));
	}
	else if ((LDS_MOUNTED == oldStatus && LDS_UNMOUNTED == newStatus) ||
		(LDS_UNMOUNT_PENDING == oldStatus && LDS_UNMOUNTED == newStatus))
	{
		// UNMOUNTED
		CNdasUnitDeviceVector unitDevices(m_unitDevices.size());
		std::transform(
			m_unitDevices.begin(), m_unitDevices.end(),
			unitDevices.begin(),
			weak_ptr_to_shared_ptr<CNdasUnitDevice>());
		std::for_each(
			unitDevices.begin(), unitDevices.end(),
			boost::mem_fn(&CNdasUnitDevice::OnUnmounted));
	}
	
	// publish a status change event
	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceStatusChanged(m_logicalDeviceId, oldStatus, newStatus);


	// Temp fault tolerant RAID work-around.
	// Remove NdasScsiLocation  that we couldn't remove because it was mounted.
	//
	if (GetNdasScsiLocation().SlotNo && m_fMountable== FALSE && 
		!(LDS_MOUNTED == newStatus || LDS_MOUNT_PENDING == newStatus || LDS_UNMOUNT_PENDING== newStatus)) {
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		manager.UnregisterNdasScsiLocation(m_NdasScsiLocation, shared_from_this());
		_DeallocateNdasScsiLocation();
	}
}

void 
CNdasLogicalDevice::_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR logDevError)
{
	InstanceAutoLock autolock(this);
	m_lastError = logDevError;
}

ACCESS_MASK 
CNdasLogicalDevice::GetMountedAccess()
{ 
	return m_MountedAccess; 
}

void 
CNdasLogicalDevice::SetMountedAccess(ACCESS_MASK mountedAccess)
{ 
	InstanceAutoLock autolock(this);
	m_MountedAccess = mountedAccess; 
}

CNdasUnitDevicePtr CNdasLogicalDevice::GetPrimaryUnit()
{
	InstanceAutoLock autolock(this);
	DWORD devCount;
	DWORD i;
	CNdasUnitDevicePtr UnitDev;
	NDAS_LOGICALDEVICE_GROUP ldGroup;	
		
	// Find primary unit to use
	devCount = GetUnitDeviceCount();
	for(i=0;i<devCount;i++) {
		UnitDev = GetUnitDevice(i);
		if (!UnitDev || 0 == UnitDev.get()) {
			continue;
		}
		ldGroup =UnitDev->GetLDGroup();
		if (::memcmp(&ldGroup, &m_logicalDeviceGroup, sizeof(ldGroup)) != 0) {
			// This unit is not configured as RAID member.
			continue;
		}

		CNdasUnitDeviceVector unitDevices(m_unitDevices.size());
		std::transform(
			m_unitDevices.begin(), m_unitDevices.end(),
			unitDevices.begin(),
			weak_ptr_to_shared_ptr<CNdasUnitDevice>());
		CNdasUnitDeviceVector::const_iterator itr = 
			std::find(
				unitDevices.begin(), unitDevices.end(),
				UnitDev);
		if (unitDevices.end() == itr)
		{
			// This unit is offline or not in this logical device.
			continue;
		}
		return UnitDev;
	}
	return CNdasUnitDeviceNullPtr;
}


ACCESS_MASK 
CNdasLogicalDevice::GetGrantedAccess()
{
	InstanceAutoLock autolock(this);
	if (0 == GetUnitDeviceCount()) {
		return 0x00000000L;
	}

	ACCESS_MASK access(0xFFFFFFFFL);
	//
	// Get minimum access level. No access if device is not registered.
	//
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {
		CNdasDevicePtr pNdasDev;
		pNdasDev = pGetNdasDevice(GetLDGroup().UnitDevices[i].DeviceId);
		if (0 == pNdasDev.get()) {
			// Device is not registered.
			return 0x00000000L;
		}
		access &= pNdasDev->GetGrantedAccess();
	}

	return access;
}

ACCESS_MASK 
CNdasLogicalDevice::GetAllowingAccess()
{
	InstanceAutoLock autolock(this);
	if (0 == GetUnitDeviceCount()) 
	{
		return 0x00000000L;
	}

	ACCESS_MASK access(0xFFFFFFFFL);
	//
	// Get minimum access level. No access if device is not registered.
	//
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) 
	{
		CNdasDevicePtr pNdasDev;
		pNdasDev = pGetNdasDevice(GetLDGroup().UnitDevices[i].DeviceId);
		if (0 == pNdasDev.get()) {
			// Device is not registered.
			return 0x00000000L;
		}
		access &= pNdasDev->GetAllowingAccess();
	}

	return access;
}

DWORD 
CNdasLogicalDevice::GetMountedDriveSet()
{ 
	return m_dwMountedDriveSet; 
}

void 
CNdasLogicalDevice::SetMountedDriveSet(DWORD dwDriveSet)
{ 
	InstanceAutoLock autolock(this);
	m_dwMountedDriveSet = dwDriveSet; 
}

NDAS_LOGICALDEVICE_ERROR
CNdasLogicalDevice::GetLastError()
{
	InstanceAutoLock autolock(this);
	return m_lastError;
}


NDAS_LOGICALDEVICE_STATUS
CNdasLogicalDevice::GetStatus()
{
	InstanceAutoLock autolock(this);
	return m_status;
}

DWORD
CNdasLogicalDevice::GetCurrentMaxRequestBlocks()
{
	InstanceAutoLock autolock(this);

	return m_dwCurrentMRB;
}


BOOL
CNdasLogicalDevice::_IsRiskyMount()
{
	InstanceAutoLock autolock(this);
	return m_fRiskyMount;
}

BOOL
CNdasLogicalDevice::GetRiskyMountFlag()
{
	InstanceAutoLock autolock(this);

	BOOL fRisky = FALSE;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("RiskyMountFlag"),
		&fRisky);

	return fRisky;
}

DWORD
CNdasLogicalDevice::GetRaidSlotNo()
{
	InstanceAutoLock autolock(this);
	DWORD SlotNo = 0;
	BOOL fSuccess;	

	// Find currently allocate slot number from registry
	fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("SlotNo"),
		&SlotNo);
	if (SlotNo == 0) {
		fSuccess = _NdasSystemCfg.GetValueEx(
			_T("LogDevices"),
			_T("LastSlotNo"), 
			&SlotNo);
		if (!fSuccess) {
			SlotNo = 10000;
		} else {
			SlotNo++;
		}
		fSuccess = _NdasSystemCfg.SetValueEx(
			_T("LogDevices"),
			_T("LastSlotNo"), 
			SlotNo);
		fSuccess = _NdasSystemCfg.SetValueEx(
			m_szRegContainer,
			_T("SlotNo"),
			SlotNo);		
	}
	return SlotNo;
}

struct SetUnitDeviceFault : public std::unary_function<void, CNdasUnitDevicePtr> {
	void operator()(const CNdasUnitDevicePtr& pUnitDevice) const {
		pUnitDevice->SetFault();
	}
};

void
CNdasLogicalDevice::SetAllUnitDevicesFault()
{
	InstanceAutoLock autolock(this);

	CNdasUnitDeviceVector unitDevices(m_unitDevices.size());

	std::transform(
		m_unitDevices.begin(), m_unitDevices.end(),
		unitDevices.begin(),
		weak_ptr_to_shared_ptr<CNdasUnitDevice>());

	std::for_each(
		unitDevices.begin(), unitDevices.end(),
		SetUnitDeviceFault());
}

bool
CNdasLogicalDevice::IsAnyUnitDevicesFault()
{
	InstanceAutoLock autolock(this);

	CNdasUnitDeviceVector unitDevices(m_unitDevices.size());

	std::transform(
		m_unitDevices.begin(), m_unitDevices.end(),
		unitDevices.begin(),
		weak_ptr_to_shared_ptr<CNdasUnitDevice>());

	CNdasUnitDeviceVector::const_iterator itr = 
		std::find_if(
			unitDevices.begin(), unitDevices.end(),
			boost::mem_fn(&CNdasUnitDevice::IsFault));

	if (unitDevices.end() != itr)
	{
		return true;
	}

	return false;
}

void
CNdasLogicalDevice::SetRiskyMountFlag(BOOL fRisky)
{
	InstanceAutoLock autolock(this);

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		m_szRegContainer,
		_T("RiskyMountFlag"),
		fRisky);

	m_fRiskyMount = fRisky;
}

void
CNdasLogicalDevice::SetLastMountAccess(ACCESS_MASK mountedAccess)
{
	InstanceAutoLock autolock(this);

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		m_szRegContainer, 
		_T("MountMask"), 
		(DWORD) mountedAccess);
}

ACCESS_MASK
CNdasLogicalDevice::GetLastMountAccess()
{
	InstanceAutoLock autolock(this);

	ACCESS_MASK mountMask = 0;

	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("MountMask"),
		(LPDWORD)&mountMask);

	if (!fSuccess) 
	{
		return 0;
	}

	return mountMask;
}

BOOL
CNdasLogicalDevice::_IsPSWriteShareCapable()
{
	BOOL fNoPSWriteShare = NdasServiceConfig::Get(nscDontUseWriteShare);

	// logical device specific option
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("NoPSWriteShare"),
		&fNoPSWriteShare);
	if (fSuccess && fNoPSWriteShare)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"NoPSWriteShare is set at %s.\n", ToStringA());
		return FALSE;
	}

	// even though NoPSWriteShare is not set, if there is no active
	// LFS filter, then PSWriteShare is denied.
	WORD wNDFSMajor, wNDFSMinor;
	fSuccess = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL, 
		&wNDFSMajor, &wNDFSMinor);
	if (!fSuccess)
	{
		// no LFS exists or it is not working NoPSWriteShare
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"LFSFilter does not exist. NoPSWriteShare.\n");
		return FALSE;
	}

	if (NdasServiceConfig::Get(nscDisableRAIDWriteShare))
	{
		if (NDAS_LOGICALDEVICE_TYPE_DISK_RAID1 == m_logicalDeviceGroup.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2 == m_logicalDeviceGroup.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID4 == m_logicalDeviceGroup.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2 == m_logicalDeviceGroup.Type)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION, 
				"WriteShare is disabled for RAID1, RAID1_R2, RAID4, RAID4_R2.\n");

			return FALSE;
		}
	}

	return TRUE;
}

bool
CNdasLogicalDevice::_IsWriteAccessAllowed(
	BOOL fPSWriteShare,
	CNdasUnitDevicePtr pUnitDevice)
{
	XTLENSURE_RETURN_T(CNdasUnitDeviceNullPtr != pUnitDevice, false);

	DWORD nROHosts, nRWHosts;
	BOOL fSuccess = pUnitDevice->GetHostUsageCount(&nROHosts, &nRWHosts, TRUE);
	if (!fSuccess) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_DEVICE_COMMUNICATION_FAILURE);
		return FALSE;
	}

	if (nRWHosts > 0)
	{
		if (!fPSWriteShare)
		{
			::SetLastError(NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED);
			return FALSE;
		}
		else
		{
			fSuccess = pUnitDevice->CheckNDFSCompatibility();
			if (!fSuccess)
			{
				::SetLastError(NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED);
				return FALSE;
			}
		}
	}

	return TRUE;
}

namespace
{
	struct LDPF_LUR_MAPPING {
		DWORD  LdpfBit;
		struct { 
			UINT32 OnBit; 
			UINT32 OffBit;
		} LurOption;
	};

	const LDPF_LUR_MAPPING ldpf_lur_mappings[] = {
		//NDAS_LDPF_PSWRITESHARE,     LUROPTION_ON_WRITESHARE_PS,   LUROPTION_OFF_WRITESHARE_PS,
		//NDAS_LDPF_FAKE_WRITE,       LUROPTION_ON_FAKEWRITE,       LUROPTION_OFF_FAKEWRITE,
		// NDAS_LDPF_OOB_ACCESS,       0, 0,
		//NDAS_LDPF_LOCKED_WRITE,     LUROPTION_ON_LOCKEDWRITE,     LUROPTION_OFF_LOCKEDWRITE,
		NDAS_LDPF_SIMULTANEOUS_WRITE,   LUROPTION_ON_SIMULTANEOUS_WRITE,   LUROPTION_OFF_SIMULTANEOUS_WRITE,
		NDAS_LDPF_OUTOFBOUND_WRITE,     LUROPTION_ON_OOB_WRITE,            LUROPTION_OFF_OOB_WRITE,
		NDAS_LDPF_NDAS_2_0_WRITE_CHECK, LUROPTION_ON_NDAS_2_0_WRITE_CHECK, LUROPTION_OFF_NDAS_2_0_WRITE_CHECK,
		NDAS_LDPF_DYNAMIC_REQUEST_SIZE, LUROPTION_ON_DYNAMIC_REQUEST_SIZE, LUROPTION_OFF_DYNAMIC_REQUEST_SIZE,
	};

	const LDPF_LUR_MAPPING ldpf_lur_null_mapping = {0};

	inline
	void
	SetLurOptionBit(UINT32& LurOptions, UINT32 OnBit, UINT32 OffBit, bool On)
	{
		LurOptions |= On ? OnBit : OffBit;
		LurOptions &= ~(On ? OffBit : OnBit);
	}

	inline
	void
	SetLurOptionsFromMappings(UINT32& LurOptions, DWORD LdpfFlags, DWORD LdpfValues)
	{
		for (int i = 0; i < RTL_NUMBER_OF(ldpf_lur_mappings); ++i)
		{
			const LDPF_LUR_MAPPING& mapping = ldpf_lur_mappings[i];
			// If LdpfBit in LdpfFlags is set, 
			// set LurOptions by corresponding bit in LdpfValues
			if (mapping.LdpfBit & LdpfFlags)
			{
				// Flags is set, turn it on or off by the bit in LdpfValues
				SetLurOptionBit(
					LurOptions, 
					mapping.LurOption.OnBit, 
					mapping.LurOption.OffBit, 
					(mapping.LdpfBit & LdpfValues) ? true : false);
			}
		}
	}
}

BOOL 
CNdasLogicalDevice::PlugIn(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	InstanceAutoLock autolock(this);
	NDAS_DEV_ACCESSMODE deviceMode;

	BOOL fPSWriteShare = _IsPSWriteShareCapable();

	BOOL fSuccess = _CheckPlugInCondition(requestingAccess);
	if (!fSuccess) 
	{
		return FALSE;
	}

	//
	// Plug In
	// - NDAS Controller
	//

	CNdasUnitDevicePtr pPrimaryUnitDevice = GetPrimaryUnit();

	if (CNdasUnitDeviceNullPtr == pPrimaryUnitDevice) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	}

	CNdasDevicePtr pPrimaryNdasDevice = pPrimaryUnitDevice->GetParentDevice();
	if (CNdasDeviceNullPtr == pPrimaryNdasDevice) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
	}

	//
	// Max DVD Instance (Global Instance) Constraint
	//
	// Do not allow local host to have multiple DVD devices
	// with Write Access.
	// I-O Data's request?
	//
#if NDAS_FEATURE_DONOT_ALLOW_MULTIPLE_DVD_RW_INSTANCES
	if (NDAS_LOGICALDEVICE_TYPE_DVD == GetType()) 
	{
		fSuccess = cpCheckPlugInCondForDVD(requestingAccess);
		if (!fSuccess) 
		{
			// SetLastError is set by the pCheckPlugInCondForDVD
			return FALSE;
		}
	}
#endif

	//
	// Resetting an event always succeeds if the handle is valid
	//
	XTLVERIFY( ::ResetEvent(m_hDisconnectedEvent) );
	XTLVERIFY( ::ResetEvent(m_hAlarmEvent) );

	//
	// Add Target
	// - Add a disk device to the NDAS controller
	//

	DWORD cbAddTargetDataSizeWithoutBACL =  
		sizeof(LANSCSI_ADD_TARGET_DATA) - sizeof(LSBUS_UNITDISK) +
		GetUnitDeviceCount() * sizeof(LSBUS_UNITDISK);
	DWORD cbAddTargetDataSize = 
		cbAddTargetDataSizeWithoutBACL + pPrimaryUnitDevice->GetBACLSize();

	PLANSCSI_ADD_TARGET_DATA pAddTargetData = 
		(PLANSCSI_ADD_TARGET_DATA) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbAddTargetDataSize);

	if (NULL == pAddTargetData) 
	{
		// TODO: Out of memory
		return FALSE;
	}

	//
	//	Determine device mode
	//
	XTLASSERT(requestingAccess & GENERIC_READ);
	if(requestingAccess & GENERIC_WRITE) {

		if(fPSWriteShare) {
			deviceMode = DEVMODE_SHARED_READWRITE;
		} else {
			deviceMode = DEVMODE_EXCLUSIVE_READWRITE;
		}

	} else {
		deviceMode = DEVMODE_SHARED_READONLY;
	}

	// automatically free the heap when it goes out of the scope
	XTL::AutoProcessHeap autoHeap = pAddTargetData;

	UCHAR targetType = LogicalDeviceTypeToNdasBusTargetType(m_logicalDeviceGroup.Type);

	pAddTargetData->ulSize = cbAddTargetDataSize;
	pAddTargetData->ulSlotNo = m_NdasScsiLocation.SlotNo;
	pAddTargetData->ulTargetBlocks = 0; // initialization and will be added
	pAddTargetData->DeviceMode = deviceMode;
	pAddTargetData->ulNumberOfUnitDiskList = GetUnitDeviceCount();
	pAddTargetData->ucTargetType = targetType;

	// Get Default LUR Options
	pAddTargetData->LurOptions = NdasServiceConfig::Get(nscLUROptions);

	// if PSWriteShare is not capable, we should specify the LUROption
	// to turn off the PSWriteShare explicitly.
	//
	// NOTE: Deprecated.
	// Should specify DEVMODE_EXCLUSIVE_READWRITE to the device mode, instead.
	//
//	if (!fPSWriteShare)
//	{
//		SetLurOptionBit(
//			pAddTargetData->LurOptions, 
//			LUROPTION_ON_WRITESHARE_PS, 
//			LUROPTION_OFF_WRITESHARE_PS, 
//			false);
//	}

	// Set LdpfFlags
	SetLurOptionsFromMappings(
		pAddTargetData->LurOptions, 
		LdpfFlags, 
		LdpfValues);

	// Set Content Encryption from the primary unit device
	// (Only for Disk Devices)
	if (NDAS_UNITDEVICE_TYPE_DISK == pPrimaryUnitDevice->GetType()) 
	{
		
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());

		const NDAS_CONTENT_ENCRYPT& encrypt = pUnitDiskDevice->GetEncryption();

		XTLASSERT(encrypt.KeyLength <= 0xFF);
		XTLASSERT(encrypt.Method <= 0xFF);
		pAddTargetData->CntEcrKeyLength = static_cast<UCHAR>(encrypt.KeyLength);
		pAddTargetData->CntEcrMethod = static_cast<UCHAR>(encrypt.Method);

		::CopyMemory(
			pAddTargetData->CntEcrKey,
			encrypt.Key,
			encrypt.KeyLength);

	}

	// set BACL data
	if(0 != pPrimaryUnitDevice->GetBACLSize())
	{
		pAddTargetData->BACLOffset = cbAddTargetDataSizeWithoutBACL;
		pPrimaryUnitDevice->FillBACL(((BYTE *)pAddTargetData) + pAddTargetData->BACLOffset);
	}

	if (NDASSCSI_TYPE_DISK_RAID1 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4 == targetType ||
		NDASSCSI_TYPE_DISK_RAID1R2 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R2 == targetType ||
		NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R3 == targetType)
	{
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());
		XTLASSERT(NULL != pUnitDiskDevice->GetAddTargetInfo());

		::CopyMemory(
			&pAddTargetData->RAID_Info,
			pUnitDiskDevice->GetAddTargetInfo(),
			sizeof(INFO_RAID));

		if (0 == pAddTargetData->RAID_Info.SectorsPerBit) 
		{
			::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION);
			return FALSE;
		}
	}

	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) 
	{	
		CNdasDevicePtr pDevice;
		CNdasUnitDevicePtr pUnitDevice;
		NDAS_LOGICALDEVICE_GROUP ldGroup;
#if 0	
		pUnitDevice = GetUnitDevice(i);
		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
			::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
			return FALSE;
		}

		pDevice = pUnitDevice->GetParentDevice();
		XTLASSERT(CNdasDeviceNullPtr != pDevice);
#else 	
		pUnitDevice = GetUnitDevice(i);
		// Handle degraded mount.
		if (CNdasUnitDeviceNullPtr == pUnitDevice) {	// to do: Create CMissingNdasUnit...
			// Missing member. This is allowed only for redundent disk.
			XTLASSERT(NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID4R3 == targetType);		
			pDevice = pGetNdasDevice(GetLDGroup().UnitDevices[i].DeviceId);
			if (CNdasDeviceNullPtr == pDevice) {
				::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
				return FALSE;
			}
		} else {
			pDevice = pUnitDevice->GetParentDevice();
			XTLASSERT(CNdasDeviceNullPtr != pDevice);

			ldGroup = pUnitDevice->GetLDGroup();
			if (::memcmp(&ldGroup, &m_logicalDeviceGroup, sizeof(ldGroup)) != 0) {
				//
				// This device is online but not is not configured as a member of this logical device.
				// Handle this unit as missing member. ndasscsi will handle it.
				//
				pUnitDevice = CNdasUnitDeviceNullPtr;
			} 
		}
#endif

		PLSBUS_UNITDISK pud = &pAddTargetData->UnitDiskList[i];

		if (CNdasUnitDeviceNullPtr == pUnitDevice) {
			// Temp fix. pDevice's remote address is not initialized until it has been online at least one moment.		
			// Use address from LDgroup.
			::CopyMemory(
				pud->Address.Node, 
				GetLDGroup().UnitDevices[i].DeviceId.Node, 
				sizeof(pud->Address.Node));
		} else {
			::CopyMemory(
				pud->Address.Node, 
				pDevice->GetRemoteLpxAddress().Node, 
				sizeof(pud->Address.Node));
		}
		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);

		C_ASSERT(
			sizeof(pud->NICAddr.Node) ==
			sizeof(pDevice->GetLocalLpxAddress().Node));

		if (CNdasUnitDeviceNullPtr == pUnitDevice) {
			// Temp fix. pDevice's remote address is not initialized until it has been online at least one moment.		
			ZeroMemory(&pud->NICAddr.Node, sizeof(pud->NICAddr.Node));
		} else {
			::CopyMemory(
				pud->NICAddr.Node, 
				pDevice->GetLocalLpxAddress().Node, 
				sizeof(pud->NICAddr.Node));
		}
		pud->NICAddr.Port = htons(0); // should be zero

		if (CNdasUnitDeviceNullPtr == pUnitDevice) {
			// This is missing member. This is temp fix.
			// iUserID and iPassword will not work for future version!!.
			pud->iUserID = CNdasUnitDevice::GetDeviceUserID(GetLDGroup().UnitDevices[i].UnitNo, requestingAccess);
			pud->iPassword = pDevice->GetHardwarePassword();
			
			pud->ulUnitBlocks = pPrimaryUnitDevice->GetUserBlockCount(); //  Assume RAID1, RAID4 and use primary device's
			pud->ulPhysicalBlocks = 0; // Unknown..
			pud->ucUnitNumber = GetLDGroup().UnitDevices[i].UnitNo;

			pud->ucHWType = 0/*HW_TYPE_ASIC*/; 		// Don't know right now..
			pud->ucHWVersion = GetLDGroup().DeviceHwVersions[i]; // Use hint from DIB
			pud->ucHWRevision = 0;	// Don't know right now..
			pud->LurnOptions |= LURNOPTION_MISSING;
		} else {
			pud->iUserID = pUnitDevice->GetDeviceUserID(requestingAccess);
			pud->iPassword = pUnitDevice->GetDevicePassword();

			pud->ulUnitBlocks = pUnitDevice->GetUserBlockCount();
			pud->ulPhysicalBlocks = pUnitDevice->GetPhysicalBlockCount();
			pud->ucUnitNumber = static_cast<UCHAR>(pUnitDevice->GetUnitNo());
			pud->ucHWType = pDevice->GetHardwareType();
			pud->ucHWVersion = pDevice->GetHardwareVersion();
			pud->ucHWRevision = pDevice->GetHardwareRevision();	
		}
		pud->IsWANConnection = FALSE;

		//
		// Set Reconnect Retry Count, Retry Interval
		// if overridden by the user
		//
		// default:
		// ReconnTrial = 19, ReconnInterval = 3000
		//
		// reddotnet: (will be set by the installer)
		// ReconnTrial = 2, ReconnInterval = 3000
		//
		{
			BOOL fOverride = NdasServiceConfig::Get(nscOverrideReconnectOptions);

			if (fOverride)
			{
				DWORD dwReconnect = 
					NdasServiceConfig::Get(nscLogicalDeviceReconnectRetryLimit);

				DWORD dwReconnectInterval = 
					NdasServiceConfig::Get(nscLogicalDeviceReconnectInterval);

				pud->LurnOptions |= LURNOPTION_SET_RECONNECTION;
				pud->ReconnTrial = dwReconnect;
				pud->ReconnInterval = dwReconnectInterval;
			}
		}

		//
		// Get the optimal data send/receive length.
		// TODO: separate send and receive data length.
		//       Get request length in bytes.
		//
		
		if (CNdasUnitDeviceNullPtr == pUnitDevice) {
			//
			// Temp fix: This may not work for some HW such as emulator.
			// Driver also limit Max transfer size if needed.
			pud->UnitMaxDataRecvLength = pReadMaxRequestBlockLimitConfig(pud->ucHWVersion) * 512;
			pud->UnitMaxDataSendLength = pud->UnitMaxDataRecvLength;
		} else {
			pud->UnitMaxDataSendLength = pUnitDevice->GetOptimalMaxRequestBlock() * 512;
			pud->UnitMaxDataRecvLength = pUnitDevice->GetOptimalMaxRequestBlock() * 512;
		}
		
		//
		// Add Target Info
		//

		if (CNdasUnitDeviceNullPtr != pUnitDevice &&
			NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType()) 
		{
			CNdasUnitDiskDevice* pUnitDiskDevice =
				reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice.get());

			//
			// check if last DIB information is same with current one
			//
			if(!pUnitDiskDevice->HasSameDIBInfo())
			{
				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
				::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION);
				return FALSE;
			}

			//
			// check if bitmap status in RAID 1, 4. Do not plug in
			//
#ifdef __DO_NOT_MOUNT_CORRUPTED_RAID__ // now driver supports recover on mount
			if (NDAS_UNITDEVICE_DISK_TYPE_RAID1 == pUnitDiskDevice->GetSubType().DiskDeviceType ||
				NDAS_UNITDEVICE_DISK_TYPE_RAID4 == pUnitDiskDevice->GetSubType().DiskDeviceType)
			{
				if (!pUnitDiskDevice->IsBitmapClean()) 
				{
					::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_CORRUPTED_BIND_INFORMATION);
					return FALSE;
				}
			}
#endif
			//
			// Done checking plug in condition
			//
		}
	}

	//
	// Check Multiple Write Access Compatibility
	//
	if (GENERIC_WRITE & requestingAccess) 
	{
		DWORD dwMaxNDFSCompatCheck = 1;

		if (m_fDisconnected)
		{
			// On disconnection (other than power failure), 
			// they may exist an inactive R/W connection at the NDAS device. 
			// In such case, no host will reply to NDFS Compatibility Check.
			// As an workaround for that we try NDFS Compatibility Check 
			// more than once if failed.
			dwMaxNDFSCompatCheck = 
				NdasServiceConfig::Get(nscWriteAccessCheckLimitOnDisconnect);
		}

		for (DWORD i = 0; i < dwMaxNDFSCompatCheck; ++i)
		{
			fSuccess = _IsWriteAccessAllowed(fPSWriteShare, pPrimaryUnitDevice);
			if (fSuccess) 
			{
				break;
			}
		}

		if (!fSuccess)
		{
			return FALSE;
		}
	}
		

	// After this, we used up a Disconnected flag, so we can clear it.
	m_fDisconnected = FALSE;

	pAddTargetData->ulTargetBlocks = GetUserBlockCount();


	//
	//	We don't need an algorithm for a SCSI adapter's max request blocks
	//	We just need one more registry key to override.
	//	We set 0 to use driver's default max request blocks.
	//

#if 0
	DWORD dwMaxRequestBlocksPerUnit = _GetMaxRequestBlocks();
	DWORD dwMaxRequestBlocks;

	// use max of MBR
	switch(GetType())
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		dwMaxRequestBlocks = dwMaxRequestBlocksPerUnit * GetUnitDeviceCountInRaid();
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
		dwMaxRequestBlocks = dwMaxRequestBlocksPerUnit * (GetUnitDeviceCountInRaid() -1);
		break;
	default:
		dwMaxRequestBlocks = dwMaxRequestBlocksPerUnit;
		break;
	}
#else
	//	Use driver's default value
	DWORD dwMaxRequestBlocks = 0;
#endif

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LsBusCtlPlugInEx2, SlotNo=%d, MaxReqBlock=%d, DisEvt=%p, RecEvt=%p\n",
		m_NdasScsiLocation.SlotNo, 
		dwMaxRequestBlocks, 
		m_hDisconnectedEvent, 
		m_hAlarmEvent);

	SetReconnectFlag(FALSE);

	BOOL fVolatileRegister = IsVolatile();

	fSuccess = LsBusCtlPlugInEx2(
		m_NdasScsiLocation.SlotNo,
		dwMaxRequestBlocks,
		m_hDisconnectedEvent,
		m_hAlarmEvent,
		fVolatileRegister);

	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LsBusCtlPlugInEx2 failed, error=0x%X\n", GetLastError());
		XTLASSERT(FALSE && "PlugIn failure");
		return FALSE;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LsBusCtlPlugInEx completed successfully, SlotNo=%d\n",
		m_NdasScsiLocation.SlotNo);

	fSuccess = LsBusCtlAddTarget(pAddTargetData);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LsBusCtlAddTarget failed, error=0x%X\n", GetLastError());
		XTLASSERT(FALSE && "AddTarget Failed");

		Sleep(1000);
		LsBusCtlEject(m_NdasScsiLocation.SlotNo);

		return FALSE;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LsBusCtlAddTarget completed successfully, SlotNo=%d\n",
		m_NdasScsiLocation.SlotNo);

	//
	// Set the status
	//

	SetMountedAccess(requestingAccess);

	(void) SetLastLdpf(LdpfFlags, LdpfValues);

	//
	// Set the status as pending, actual mount completion is
	// reported from PNP event handler to call OnMounted()
	// to complete this process
	//

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

	//
	// Clear Adapter Status
	//

	SetAdapterStatus(NDASSCSI_ADAPTERINFO_STATUS_INIT);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"PlugIn completed successfully, SlotNo=%d\n",
		m_NdasScsiLocation.SlotNo);

	return TRUE;
}

BOOL
CNdasLogicalDevice::Unplug()
{
	BOOL fSuccess(FALSE);

	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplugging, LogicalDevice=%s\n", ToStringA());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED &&
		m_status != NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING &&
		m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		return FALSE;
	}

	//
	// Remove target ejects the disk and the volume.
	//

	fSuccess = LsBusCtlRemoveTarget(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
			"LsBusCtlRemoveTarget failed, error=0x%X\n", GetLastError());
	}

	// Intentional break
	::Sleep(100);

	//
	// BUG:
	// What happened when RemoveTarget succeeded and 
	// Unplugging LANSCSI port is failed?
	//

	fSuccess = LsBusCtlUnplug(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LsBusCtlUnplug failed, error=0x%X\n", GetLastError());
		// last error from lsbusctl unplug
		return FALSE;
	}

	//
	// Change the status to unmounted
	//
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Remove Ldpf from the registry
	(void) ClearLastLdpf();

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplug completed successfully, SlotNo=%d\n",
		m_NdasScsiLocation.SlotNo);

	return TRUE;

}

BOOL
CNdasLogicalDevice::Eject()
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Ejecting, LogicalDevice=%s\n", ToStringA());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Eject is requested to non-initialized logical device");

		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Eject is requested to not mounted logical device\n");

		return FALSE;
	}

	BOOL fSuccess = ::LsBusCtlEject(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LsBusCtlEject failed at %s, error=0x%X\n", 
			m_NdasScsiLocation.ToStringA(), GetLastError());
		return FALSE;
	}

	//
	// Now we have to wait until the ejection is complete
	//
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Eject completed successfully, SlotNo=%d\n",
		m_NdasScsiLocation.SlotNo);

	return TRUE;
}

BOOL 
CNdasLogicalDevice::EjectEx(
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName,
	DWORD nNameLength)
{
	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"EjectEx, LogicalDevice=%s\n", ToStringA());

	DWORD slotNo = GetNdasScsiLocation().SlotNo;
	CONFIGRET cret;
	BOOL fSuccess = pRequestEject(slotNo, &cret, pVetoType, pszVetoName, nNameLength);

	if (fSuccess && CR_SUCCESS == cret)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"RequestEject failed, slotNo=%d, cret=0x%X\n", slotNo, cret);
	}
	else
	{
		if (!fSuccess)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"RequestEject failed, slotNo=%d, error=0x%X\n", slotNo, GetLastError());
		}
		else
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"RequestEject failed, slotNo=%d, cret=0x%X\n", slotNo, cret);

			if (pszVetoName)
			{
				XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
					"Vetoed by %ls\n", pszVetoName);
			}
		}
	}

	if (fSuccess && CR_SUCCESS == cret)
	{
		_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);
	}

	if (pConfigRet) *pConfigRet = cret;
	return fSuccess;
}

BOOL 
CNdasLogicalDevice::GetSharedWriteInfo(
	LPBOOL lpbSharedWrite, 
	LPBOOL lpbPrimary)
{
#ifdef NDAS_FEATURE_DISABLE_SHARED_WRITE
	if (lpbSharedWrite) *lpbSharedWrite = FALSE;
	if (lpbPrimary) *lpbPrimary = FALSE;
	return TRUE;
#endif

	//BUSENUM_QUERY_INFORMATION BusEnumQuery = {0};
	//BUSENUM_INFORMATION BusEnumInformation = {0};

	//BusEnumQuery.InfoClass = INFORMATION_PDO;
	//BusEnumQuery.Size = sizeof(BUSENUM_QUERY_INFORMATION);
	//BusEnumQuery.SlotNo = m_NdasScsiLocation.SlotNo;

	//BOOL fSuccess = ::LsBusCtlQueryInformation(
	//	&BusEnumQuery,
	//	sizeof(BUSENUM_QUERY_INFORMATION),
	//	&BusEnumInformation,
	//	sizeof(BUSENUM_INFORMATION));

	//if (!fSuccess) 
	//{
	//	DBGPRT_ERR_EX(_FT("LanscsiQueryInformation failed at slot %d: "), 
	//		m_NdasScsiLocation.SlotNo);
	//	return FALSE;
	//}

	//if (ND_ACCESS_ISRW(BusEnumInformation.PdoInfo.GrantedAccess)) 
	//{
	//	*lpbPrimary = TRUE;
	//} 
	//else 
	//{
	//	*lpbPrimary = FALSE;
	//}
	
	LFSCTL_NDAS_USAGE usage = {0};
	BOOL fSuccess = ::LfsFiltQueryNdasUsage(
		m_NdasScsiLocation.SlotNo,
		&usage);

	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LfsFiltQueryNdasUsage for %s failed, error=0x%X\n",
			m_NdasScsiLocation.ToStringA(), GetLastError());
		return FALSE;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LfsFiltQueryNdasUsage for %s returned: Primary=%d, Secondary=%d, HasLockedVolume=%d.\n",
		m_NdasScsiLocation.ToStringA(),
		usage.ActPrimary,
		usage.ActSecondary,
		usage.HasLockedVolume);

	if (lpbSharedWrite) 
	{
		//
		// LFS filter should report Primary or Secondary
		// Otherwise, the filter is not active on the NDAS device.
		// If the volume is locked, shared write is not capable.
		//
		if ((usage.ActPrimary || usage.ActSecondary) && !usage.HasLockedVolume)
		{
			*lpbSharedWrite = TRUE;
		}
		else
		{
			*lpbSharedWrite = FALSE;
		}
	}

	if (lpbPrimary)
	{
		*lpbPrimary = usage.ActPrimary;
	}

	return TRUE;
} 

void
CNdasLogicalDevice::SetMountOnReady(
	ACCESS_MASK access, 
	BOOL fReducedMountOnReadyAccess)
{
	InstanceAutoLock autolock(this);

	if (0 == access) 
	{
		m_mountOnReadyAccess = 0;
		m_fMountOnReady = FALSE;
		m_fReducedMountOnReadyAccess = fReducedMountOnReadyAccess;
	}
	else 
	{
		m_mountOnReadyAccess = access;
		m_fMountOnReady = TRUE;
		m_fReducedMountOnReadyAccess = fReducedMountOnReadyAccess;
	}
}

DWORD
CNdasLogicalDevice::GetMountTick()
{
	return m_dwMountTick;
}

const NDAS_SCSI_LOCATION& 
CNdasLogicalDevice::GetNdasScsiLocation()
{
	InstanceAutoLock autolock(this);
	return m_NdasScsiLocation;
}

	
BOOL
CNdasLogicalDevice::_CheckPlugInCondition(ACCESS_MASK requestingAccess)
{
	InstanceAutoLock autolock(this);

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	//
	// We allows plug in calls from NDAS_LOGICALDEVICE_STATUS_UNMOUNTED only.
	//
	if (m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNTED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_UNMOUNTED);
		return FALSE;
	}

	NDAS_RAID_MOUNTABILITY_FLAGS Mountablity;
	NDAS_RAID_FAIL_REASON FailReason;
	// Check mountable again.
	CheckMountability(&Mountablity, &FailReason);

	if (!IsMountable()) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	}

	//
	// check access permission
	//

	//
	// only GENERIC_READ and GENERIC_WRITE is allowed
	//
	requestingAccess &= (GENERIC_READ | GENERIC_WRITE);

	ACCESS_MASK grantedAccess = GetGrantedAccess();
	if ((grantedAccess & requestingAccess) != requestingAccess) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED);
		return FALSE;
	}

	ACCESS_MASK allowingAccess = GetAllowingAccess();
	if ((requestingAccess & allowingAccess) != requestingAccess) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_FAILED);
		return FALSE;
	}

	return TRUE;
}

namespace
{
	bool 
	IsLogicalDVDMountedAsRW(CNdasLogicalDevicePtr pLogDevice)
	{
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		switch (status)
		{
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED :
		case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
			if (NDAS_LOGICALDEVICE_TYPE_DVD == pLogDevice->GetType() &&
				GENERIC_WRITE & pLogDevice->GetMountedAccess()) 
			{
				return true;
			}
		}
		return false;
	}
}

BOOL 
CNdasLogicalDevice::_CheckPlugInCondForDVD(ACCESS_MASK requestingAccess)
{
	CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();

	CNdasLogicalDeviceVector logDevices;
	manager.Lock();
	manager.GetItems(logDevices);
	manager.Unlock();

	SIZE_T mounted = std::count_if(
		logDevices.begin(), 
		logDevices.end(), 
		IsLogicalDVDMountedAsRW);

	if (mounted > 0) 
	{
		::SetLastError(NDASSVC_ERROR_NO_MORE_DVD_WRITE_ACCESS_INSTANCE_ALLOWED);
		return FALSE;
	}

	return TRUE;
}

void
CNdasLogicalDevice::_LocateRegContainer()
{
	//
	// Registry Container
	// HKLM\Software\NDAS\LogDevices\XXXXXXXX
	//

	BOOL fSuccess, fWriteData = TRUE;

	m_dwHashValue = _GetHashValue();

	while (TRUE) 
	{
		COMVERIFY(StringCchPrintf(
			m_szRegContainer, 30, 
			_T("LogDevices\\%08X"), m_dwHashValue));

		NDAS_LOGICALDEVICE_GROUP ldGroup;
		DWORD cbData = 0;
		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			m_szRegContainer,
			_T("Data"),
			&ldGroup,
			sizeof(ldGroup),
			&cbData);

		if (fSuccess && cbData == sizeof(ldGroup)) 
		{
			if (0 != ::memcmp(&ldGroup, &m_logicalDeviceGroup, sizeof(ldGroup))) 
			{
				// collision on hash value
				// increment the hash value and recalculate
				++m_dwHashValue;
				continue;
			} 
			else
			{
				// Existing entry.
				fWriteData = FALSE;
			}
		}

		break;
	}


	if (fWriteData) 
	{
		fSuccess = _NdasSystemCfg.SetSecureValueEx(
			m_szRegContainer,
			_T("Data"),
			&m_logicalDeviceGroup,
			sizeof(m_logicalDeviceGroup));
		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Writing LDData failed, error=0x%X", GetLastError());
		}
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Hash Value=%08X\n", m_dwHashValue);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"RegContainer: %ls\n", m_szRegContainer);
}

DWORD
CNdasLogicalDevice::_GetHashValue()
{
	InstanceAutoLock autolock(this);

	return ::crc32_calc(
		(const UCHAR*) &m_logicalDeviceGroup, 
		sizeof(NDAS_LOGICALDEVICE_GROUP));
}

void
CNdasLogicalDevice::_AllocateNdasScsiLocation()
{
	InstanceAutoLock autolock(this);
	// We have different policy for single and RAID
	if (NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE == m_logicalDeviceGroup.Type) {
		CNdasUnitDevicePtr pFirstUnitDevice = GetUnitDevice(0);
		XTLASSERT(CNdasUnitDeviceNullPtr != pFirstUnitDevice);
		m_NdasScsiLocation.SlotNo = 
			pFirstUnitDevice->GetParentDevice()->GetSlotNo() * 10 + 
			pFirstUnitDevice->GetUnitNo();
	} else {
		m_NdasScsiLocation.SlotNo = GetRaidSlotNo();
	}
	XTLASSERT(m_NdasScsiLocation.SlotNo != 0);
}

void
CNdasLogicalDevice::_DeallocateNdasScsiLocation()
{
	InstanceAutoLock autolock(this);
	m_NdasScsiLocation.SlotNo = 0;
}

UINT64
CNdasLogicalDevice::GetUserBlockCount()
{
	InstanceAutoLock autolock(this);

	if (!IsMountable()) 
	{
		return 0;
	}

	CNdasUnitDevicePtr pUnitDevice = GetPrimaryUnit();
	if (0 == pUnitDevice.get()) 
	{
		return 0;
	}

	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_logicalDeviceGroup.Type)) 
	{
		return pUnitDevice->GetUserBlockCount();
	}

	UINT64 blocks = 0;
	DWORD i;
	switch(m_logicalDeviceGroup.Type) {
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		// Device don't have DIB. First device's user block count is size of mirror.
		pUnitDevice = GetUnitDevice(0);
		if (0 == pUnitDevice.get())  {
			blocks = 0;
		} else {
			blocks = pUnitDevice->GetUserBlockCount();
		}
		break;
		
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		if (IsComplete()) {
			for(i=0;i<GetUnitDeviceCount();i++) {
				pUnitDevice = GetUnitDevice(i);
				if (pUnitDevice.get()) {
					blocks += pUnitDevice->GetUserBlockCount();
				} else {
					blocks = 0;
					break;
				}
			}
		} else {
			// We don't know the size if all unit is online.
			blocks = 0;
		}
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		blocks = pUnitDevice->GetUserBlockCount() * GetUnitDeviceCount();
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:			
		blocks = pUnitDevice->GetUserBlockCount();
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:			
		blocks += pUnitDevice->GetUserBlockCount() * (GetUnitDeviceCountInRaid() - 1);
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		blocks = pUnitDevice->GetUserBlockCount();
		break;
	default: 
		// not implemented yet : DVD, VDVD, MO, FLASH ...
		XTLASSERT(FALSE);
		break;
	}
	return blocks;
}

void
CNdasLogicalDevice::OnShutdown()
{
	InstanceAutoLock autolock(this);

	SetRiskyMountFlag(FALSE);

	m_fShutdown = TRUE;
}

void
CNdasLogicalDevice::OnDisconnected()
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"%s: Disconnect Event.\n", ToStringA());

	BOOL fSuccess = Unplug();
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"%s: Failed to handle disconnect event, error=0x%X\n", 
			ToStringA(), GetLastError());
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Set the disconnected flag
	m_fDisconnected = TRUE;
}


void
CNdasLogicalDevice::OnMounted()
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Logical device %s is MOUNTED.\n", ToStringA());

	DWORD dwTick = ::GetTickCount();
	m_dwMountTick = (dwTick == 0) ? 1: dwTick; // 0 is used for special purpose

	SetLastMountAccess(m_MountedAccess);
	SetRiskyMountFlag(TRUE);
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
}

BOOL
CNdasLogicalDevice::ReconcileFromNdasBus()
{
	HANDLE hAlarm, hDisconnect;
	BOOL fSuccess = ::LsBusCtlQueryPdoEvent(
		m_NdasScsiLocation.SlotNo, 
		&hAlarm,
		&hDisconnect);
	//
	// Reconciliation failure?
	//
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LsBusCtlQueryPdoEvent at %s failed, error=0x%X\n", 
			m_NdasScsiLocation.ToStringA(), GetLastError());
	}
	else
	{
		m_hAlarmEvent.Release();
		m_hAlarmEvent = hAlarm;
		m_hDisconnectedEvent.Release();
		m_hDisconnectedEvent = hDisconnect;
	}

	PNDSCIOCTL_ADAPTERLURINFO pLurInfo = NULL;
	fSuccess = ::LsBusCtlQueryMiniportFullInformation(
		m_NdasScsiLocation.SlotNo, 
		&pLurInfo);
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"QueryMiniportFullInformation at %s failed, error=0x%X\n", 
			m_NdasScsiLocation.ToStringA(), GetLastError());
	}
	else
	{
		ACCESS_MASK	desiredAccess;

		//
		//	Translate the device mode to the access mask.
		//

		if(pLurInfo->Lur.DeviceMode == DEVMODE_SHARED_READONLY) {
			desiredAccess = GENERIC_READ;
		} else if(
			pLurInfo->Lur.DeviceMode == DEVMODE_SHARED_READWRITE ||
			pLurInfo->Lur.DeviceMode == DEVMODE_EXCLUSIVE_READWRITE ||
			pLurInfo->Lur.DeviceMode == DEVMODE_SUPER_READWRITE) {

			desiredAccess = GENERIC_READ|GENERIC_WRITE;
		}  else {
			desiredAccess = 0;
		}

		if(desiredAccess != 0) {
			//
			// TODO: alternate m_dwCurrentMRB to the byte-unit variable.
			//
			m_dwCurrentMRB = pLurInfo->Adapter.MaxDataTransferLength / 512;
			SetMountedAccess(desiredAccess);
		}
	}

	HANDLE hNdasScsi = NULL;
	fSuccess = ::LsBusCtlQueryPdoFileHandle(
		m_NdasScsiLocation.SlotNo, 
		&hNdasScsi);
	XTL::AutoFileHandle autoNdasScsiHandle = hNdasScsi;

	if (!fSuccess || INVALID_HANDLE_VALUE == hNdasScsi)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"QueryPdoFileHandle at %s failed, error=0x%X\n", 
			m_NdasScsiLocation.ToStringA(), GetLastError());
	}
	else
	{
		CNdasServiceDeviceEventHandler& sde = pGetNdasDeviceEventHandler();
		if (!sde.AddDeviceNotificationHandle(
				hNdasScsi, 
				DeviceNotifyData(
					DeviceNotifyData::DNTStoragePort,
					m_NdasScsiLocation)))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"AddDeviceNotificationHandle at %s failed, error=0x%X\n", 
				m_NdasScsiLocation.ToStringA(), GetLastError());
		}
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	return TRUE;
}

void
CNdasLogicalDevice::OnUnmounted()
{
	InstanceAutoLock autolock(this);

	if (m_fDisconnected)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"%s: Unmount Completed (by disconnection).\n", ToStringA());
	}
	else
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"%s: Unmount Completed.\n", ToStringA());
	}

	m_dwMountTick = 0;

	if (!m_fDisconnected)
	{
		// clears the mount flag only on unmount by user's request
		SetLastMountAccess(0); 
	}

	// clears the risky mount flag
	SetRiskyMountFlag(FALSE);

	// Remove Ldpf from the registry
	(void) ClearLastLdpf();

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
}

void
CNdasLogicalDevice::OnUnmountFailed()
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unmount failed from logical device %s.\n", ToStringA());

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == GetStatus()) 
	{
		_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
	}

}

BOOL
CNdasLogicalDevice::IsVolatile()
{
	InstanceAutoLock autolock(this);

	CNdasUnitDeviceVector unitDevices(m_unitDevices.size());

	std::transform(
		m_unitDevices.begin(), m_unitDevices.end(),
		unitDevices.begin(),
		weak_ptr_to_shared_ptr<CNdasUnitDevice>());

	// If any single device is volatile, then it's volatile.
	CNdasUnitDeviceVector::const_iterator itr = 
		std::find_if(
			unitDevices.begin(), unitDevices.end(),
			boost::mem_fn(&CNdasUnitDevice::IsVolatile));

	bool result = (unitDevices.end() != itr);
	return result;
}

BOOL 
CNdasLogicalDevice::GetLastLdpf(DWORD& flags, DWORD& values)
{
	DWORD data[2] = {0};
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("LdpfData"),
		data,
		sizeof(data));
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"%s: GetLastLdpf failed, error=0x%X\n", 
			ToStringA(), GetLastError());
		return FALSE;
	}
	flags = data[0];
	values = data[1];
	return TRUE;
}

BOOL 
CNdasLogicalDevice::SetLastLdpf(DWORD flags, DWORD values)
{
	DWORD data[2] = {flags, values};
	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		m_szRegContainer,
		_T("LdpfData"),
		REG_BINARY,
		data,
		sizeof(data));
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"%s: SetLastLdpf failed, flags=%08X, value=%08X, error=0x%X\n", 
			ToStringA(), flags, values, GetLastError());

		return FALSE;
	}
	return TRUE;
}

BOOL
CNdasLogicalDevice::ClearLastLdpf()
{
	BOOL fSuccess = _NdasSystemCfg.DeleteValue(
		m_szRegContainer, 
		_T("LdpfData"));
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"%s: ClearLastLdpf failed, error=0x%X\n", 
			ToStringA(), GetLastError());
		return FALSE;
	}
	return TRUE;
}

const NDAS_CONTENT_ENCRYPT* 
CNdasLogicalDevice::GetContentEncrypt()
{
	CNdasUnitDevicePtr pPrimaryUnitDevice = GetPrimaryUnit();
	if (!pPrimaryUnitDevice)
	{
		return NULL;
	}

	if (NDAS_UNITDEVICE_TYPE_DISK != pPrimaryUnitDevice->GetType())
	{
		return NULL;
	}

	CNdasUnitDiskDevice* pUnitDiskDevice =
		reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());

	m_contentEncrypt = pUnitDiskDevice->GetEncryption();

	return &m_contentEncrypt;
}

BOOL 
CNdasLogicalDevice::GetContentEncrypt(NDAS_CONTENT_ENCRYPT* pce)
{
	InstanceAutoLock autolock(this);

	const NDAS_CONTENT_ENCRYPT* p = GetContentEncrypt();
	if (NULL == p)
	{
		return FALSE;
	}
	*pce = *p;
	return TRUE;
}

bool 
CNdasLogicalDevice::IsHidden()
{
	InstanceAutoLock autolock(this);

	CNdasUnitDevicePtr pUnitDevice = GetPrimaryUnit();
	//
	// If the first unit device of the logical device is not registered,
	// it is assumed that the device of the unit device is NOT hidden.
	// This happens when RAID members are partially registered,
	// and the first one is not registered.
	//
	if (CNdasUnitDeviceNullPtr == pUnitDevice)
	{
		return false;
	}
	CNdasDevicePtr pDevice = pUnitDevice->GetParentDevice();
	XTLASSERT(CNdasDeviceNullPtr != pDevice);
	bool ret = pDevice->IsHidden();
	return ret;
}

void
CNdasLogicalDevice::OnPeriodicCheckup()
{
	InstanceAutoLock autolock(this);

	NDAS_LOGICALDEVICE_STATUS status = GetStatus();
	if (!(
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status ||
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == status))
	{
		return;
	}

	// 3 minutes
	const DWORD LOGICALDEVICE_RISKY_MOUNT_TIMEOUT = 3 * 60 * 1000;
	//
	// Clear the risky mount flag 
	// in LOGICALDEVICE_RISK_MOUNT_INTERVAL after mounting
	//

	DWORD mountTick = GetMountTick();
	if (_IsRiskyMount() && 0 != mountTick)
	{
		DWORD dwBiased = ::GetTickCount() - mountTick;
		//
		// In case of rollover (in 19 days),
		// it is safe to clear that flag.
		// This is not a strict time tick check
		//
		if (dwBiased > LOGICALDEVICE_RISKY_MOUNT_TIMEOUT) 
		{
			SetRiskyMountFlag(FALSE);
		}
	}

	//
	// During mount pending, NDAS SCSI is not available until the user
	// accept the warning message of non-signed driver
	//
	// This may cause the logical device being unmounted. 
	//

	CNdasScsiLocation location = GetNdasScsiLocation();
	XTLASSERT(!location.IsInvalid());

	BOOL fAlive, fAdapterError;
	BOOL fSuccess = ::LsBusCtlQueryNodeAlive(
		location.SlotNo, 
		&fAlive, 
		&fAdapterError);

	//
	// if LsBusCtlQueryNodeAlive fails, 
	// there may be no NDAS SCSI device instance...
	//

	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LsBusCtlQueryNodeAlive at %s failed, error=0x%X\n", 
			m_NdasScsiLocation.ToStringA(), GetLastError());
		return;
	}

	if (!fAlive) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
			"Logical device %s instance does not exist anymore.\n", 
			ToStringA());
		OnUnmounted();
		//_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
	}

	//if (fAdapterError) {
	//	XTLTRACE_ERR("LsBusCtlQueryNodeAlive reported an adapter error.\n"));
	//	pLogDevice->_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
	//}
}

bool 
CNdasLogicalDevice::_IsUpgradeRequired()
{
	// RAID1, RAID1R2 and RAID4,RAID4R2 is replaced with RAID1_R3 and RAID4_R3
	// And the underlying device driver does not handle RAID1 and RAID4
	// So we have to make these types as NOT_MOUNTABLE and REQUIRE_UPGRADE
	// Error.
	NDAS_LOGICALDEVICE_TYPE Type = GetType();
	switch (Type)
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
		return true;
	}
	return false;
}

bool CNdasLogicalDevice::_IsRaid()
{
	return IS_NDAS_LOGICALDEVICE_TYPE_DISK_SET_GROUP(GetType());
}

	
// anonymous namespace
namespace
{

bool 
pIsValidStatusChange(
	  NDAS_LOGICALDEVICE_STATUS oldStatus,
	  NDAS_LOGICALDEVICE_STATUS newStatus)
{
    //                 +--------------------------------+ 
	//                 | (D)                            | (A)
	//                 V          (A)                   v
	// NOT_INIT --> UNMOUNTED <----> MOUNT_PENDING --> MOUNTED
	//                 ^     (D)                        ^  ^ V
	//                 |   (D)                          |  | |
	//                 +-------- UNMOUNT_PENDING  <-----+  +-+
	//                 +----> NOT_MOUNTABLE
	//

	switch (oldStatus) 
	{
	case NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED:
		switch (newStatus) 
		{
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
			return true;
		}
		break;
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		switch (newStatus) 
		{
		case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED: // From Reconciliation
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
			return true;
		}
		break;
	case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		switch (newStatus) 
		{
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
			return true;
		}
		break;
	case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
		switch (newStatus) 
		{
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:	// This path is possible for redudendant RAID.
			return true;
		}
		break;
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
		switch (newStatus) 
		{
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
			return true;
		}
		break;
	}
	return false;
}

UCHAR
LogicalDeviceTypeToNdasBusTargetType(
	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType)
{
	switch (LogicalDeviceType) 
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		return NDASSCSI_TYPE_DISK_NORMAL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		return NDASSCSI_TYPE_DISK_MIRROR;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		return NDASSCSI_TYPE_DISK_AGGREGATION;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		return NDASSCSI_TYPE_DISK_RAID0;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
		return NDASSCSI_TYPE_DISK_RAID1;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
		return NDASSCSI_TYPE_DISK_RAID1R2;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
		return NDASSCSI_TYPE_DISK_RAID1R3;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		return NDASSCSI_TYPE_DISK_RAID4;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
		return NDASSCSI_TYPE_DISK_RAID4R2;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
		return NDASSCSI_TYPE_DISK_RAID4R3;
	case NDAS_LOGICALDEVICE_TYPE_DVD:
		return NDASSCSI_TYPE_DVD;
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
		return NDASSCSI_TYPE_VDVD;
	case NDAS_LOGICALDEVICE_TYPE_MO:
		return NDASSCSI_TYPE_MO;
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
		return NDASSCSI_TYPE_DISK_NORMAL;
	default:
		XTLASSERT(FALSE);
		return NDASSCSI_TYPE_DISK_NORMAL;
	}
}

}
// anonymous namespace
