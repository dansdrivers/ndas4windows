/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <boost/mem_fn.hpp>
#include <scrc32.h>
#include <ntddscsi.h>
#include <lfsfiltctl.h>
#include <ndasbusctl.h>
#include <ndas/ndasportctl.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasop.h>
#include <ndas/ndasvolex.h>
#include <ndas/ndasportctl.h>
#include <xixfsctl.h>
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


LURN_TYPE
LogicalDeviceTypeToLurTargetType(
	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType);

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
	m_NdasLocation(0),
	m_NdasPort(FALSE),
	m_fShutdown(FALSE),
	m_ulAdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT),
	m_RaidFailReason(NDAS_RAID_FAIL_REASON_NONE),
	m_fMountable(FALSE),
	m_fDegradedMode(FALSE)
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

BOOL 
CNdasLogicalDevice::Invalidate()
{
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) 
	{	
		CNdasDevicePtr pDevice;
		CNdasUnitDevicePtr pUnitDevice;
		NDAS_LOGICALDEVICE_GROUP ldGroup;

		pUnitDevice = GetUnitDevice(i);
		// Handle degraded mount.
		if (CNdasUnitDeviceNullPtr == pUnitDevice) {
			//
			// Invalidate former missing member
			//
			pDevice = pGetNdasDevice(GetLDGroup().UnitDevices[i].DeviceId);
			if (CNdasDeviceNullPtr == pDevice) {
				continue;
			}
			pDevice->InvalidateUnitDevice(GetLDGroup().UnitDevices[i].UnitNo);
		} else {
			pDevice = pUnitDevice->GetParentDevice();
			pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
		}
	}
	return TRUE;
}

LPCTSTR
CNdasLogicalDevice::GetDevicePath() const
{
	return m_DevicePath;
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
CNdasLogicalDevice::UpdateBindStateAndError()
{
	NDAS_RAID_FAIL_REASON PrevFailReason = m_RaidFailReason;
	BOOL PrevMountable = m_fMountable;
	BOOL PrevDegradedMode = m_fDegradedMode;
	NDAS_LOGICALDEVICE_ERROR PrevLastError = m_lastError;
	
	// Update m_fMountable and m_fDegradedMode flags.
	_RefreshBindStatus();

	if (IsMountable())
	{
		if (m_fDegradedMode) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE);
		}
		else 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}
	}
	else
	{
		// Handle serious case first
		if (m_RaidFailReason & (
			NDAS_RAID_FAIL_REASON_RMD_CORRUPTED |
			NDAS_RAID_FAIL_REASON_DIB_MISMATCH |
			NDAS_RAID_FAIL_REASON_SPARE_USED |
			NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION |
			NDAS_RAID_FAIL_REASON_UNSUPPORTED_RAID |
			NDAS_RAID_FAIL_REASON_DEFECTIVE |
			NDAS_RAID_FAIL_REASON_DIFFERENT_RAID_SET |
			NDAS_RAID_FAIL_REASON_MEMBER_IO_FAIL |
			NDAS_RAID_FAIL_REASON_NOT_A_RAID |
			NDAS_RAID_FAIL_REASON_IRRECONCILABLE |
			NDAS_RAID_FAIL_REASON_INCONSISTENT_DIB |
			NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT
			)) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
		} 
		else if (m_RaidFailReason & (
			NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE |
			NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED |
			NDAS_RAID_FAIL_REASON_MEMBER_DISABLED
			))
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		} 
		else if (m_RaidFailReason & NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED)
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_UPGRADE);
		} 
		else 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}		
	}	

	BOOL raid = FALSE;

	CNdasUnitDevicePtr pPrimaryUnitDevice = GetPrimaryUnit();
	if (m_NdasPort == FALSE) 
	{
		UCHAR targetType = 
			LogicalDeviceTypeToNdasBusTargetType(m_logicalDeviceGroup.Type);
	    if (pPrimaryUnitDevice && 
		    NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
		    NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
		    NDASSCSI_TYPE_DISK_RAID5 == targetType) 
		{
		    raid = TRUE;
		}
	} 
	else 
	{
		LURN_TYPE targetType = 
			LogicalDeviceTypeToLurTargetType(m_logicalDeviceGroup.Type);

		if (pPrimaryUnitDevice && 
		    LURN_RAID1R == targetType ||
		    LURN_RAID4R == targetType ||
		    LURN_RAID5 == targetType) 
		{
		    raid = TRUE;
	    }
 	}

	if (raid) 
	{
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());

		if (pUnitDiskDevice && pUnitDiskDevice->GetAddTargetInfo()) 
		{
			PNDAS_RAID_INFO InfoRaid = (PNDAS_RAID_INFO) pUnitDiskDevice->GetAddTargetInfo();
			//
			// RAID config can be changed by kernel.
			// Check it here and invalidate devices if it is changed.
			//
			if (InfoRaid->ConfigSetId != m_ConfigSetId) 
			{
				XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
					"Logical Device config set change detected.\n");			
				return TRUE;
			}
		}
	}
	
	//
	// Return TRUE at any mount information changes. 
	// If spare has changed, m_fMountable and m_fDegradedMode is not changed. so compare m_RaidFailReason too.
	//
	if (PrevMountable != m_fMountable || 
		PrevLastError != m_lastError || 
		m_fDegradedMode !=m_fDegradedMode ||
		m_RaidFailReason != PrevFailReason) 
	{
		return TRUE;
	} 
	else 
	{
		return FALSE;
	}
}

BOOL 
CNdasLogicalDevice::_RefreshBindStatus() 
{
	BOOL bResult;
	NDASCOMM_CONNECTION_INFO ci;
	DWORD devCount;
	DWORD UnitCount;
	DWORD i;
	CNdasUnitDevicePtr UnitDev;
	CNdasDevicePtr NdasDev;
	NDASOP_RAID_INFO RaidInfo;

	if (!_IsRaid()) 
	{
		if (GetUnitDeviceInstanceCount() ==0) 
		{
			m_fMountable = FALSE;
		} 
		else 
		{
			m_fMountable = TRUE;
			UnitDev = GetUnitDevice(0);
		}
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_NONE;
		m_fDegradedMode = FALSE;
		return TRUE;
	}
	UnitCount = GetUnitDeviceInstanceCount();
	if (UnitCount ==0) 
	{
		// No unit device.
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;		
		return TRUE;
	}
	// Connect to primary device

	UnitDev = GetPrimaryUnit();
	if (CNdasUnitDeviceNullPtr == UnitDev) 
	{
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
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

	HNDAS hNDAS = NdasCommConnect(&ci);

	if (hNDAS == NULL) 
	{
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;	
		goto discon_out;	
	}
	
	RaidInfo.Size = sizeof(RaidInfo); // To check version.
	bResult = NdasOpGetRaidInfo(hNDAS, &RaidInfo);
	if (!bResult) {
		// NdasOpGetRaidInfo returns FALSE only for internal error such as version mismatch, memory allocation failure.
		XTLASSERT(FALSE);
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;	
		goto discon_out;			
	}

	m_ConfigSetId = RaidInfo.ConfigSetId;
		
	if (RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE)
	{
		if(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION)
		{
			// DIB version is higher than this version can handle.
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	
			goto discon_out;
		}
		else if(RaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT)
		{
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	
			goto discon_out;
		}
	}

	//
	// Check all device is registered and not disabled.
	// 

	for ( i = 0; i < RaidInfo.MemberCount; i++) 
	{
		NdasDev = pGetNdasDevice(RaidInfo.Members[i].DeviceId);
		if (NdasDev == NULL) 
		{
			// Member is not registered
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;				
			goto discon_out;	
		}
		
		if (NdasDev ->GetStatus() == NDAS_DEVICE_STATUS_DISABLED) 
		{
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_DISABLED;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	
			goto discon_out;
		}
	}

	m_RaidFailReason = (NDAS_RAID_FAIL_REASON) RaidInfo.FailReason;

	m_fMountable  = (RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE)?TRUE:FALSE;
	m_fDegradedMode = (RaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_DEGRADED)?TRUE:FALSE;
	
	// To do: check online member is also recognized as online by svc, too
discon_out:	

	if (hNDAS) 
	{
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

	HRESULT hr;
	BOOL success;

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

	UpdateBindStateAndError();

	if (!IsMountable())	 
	{
		if (GetStatus() == NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
		{
			// If this device is already mounted, we should not set unmounted status.
		}
		else 
		{
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
			_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			return TRUE;
		}
	}
			
	//
	// Disk is mountable but slot is not allocated yet.
	//
	if (IsMountable() && !GetNdasLocation()) 
	{
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		// Allocate NdasLocation when mountable.
		_AllocateNdasLocation();
		success = manager.RegisterNdasLocation(m_NdasLocation, shared_from_this());
		if (!success) 
		{
			_ASSERT(FALSE); // this should not happen anyway..
			//
			// This NdasScisiLocation is already used. User need to resolve this situation using bindtool
			//
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
			_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			return TRUE;
		}
	}

	if (IsMountable()) 
	{
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
		
		//
		// Reconciliation Support
		//

		BOOL activeLogicalUnit = FALSE;

		if (m_NdasPort == FALSE) 
		{
			BOOL bAlive, bAdapterError;
		    success = ::NdasBusCtlQueryNodeAlive(
			    m_NdasLocation, 
			    &bAlive, 
			    &bAdapterError);

			if (success && bAlive)
			{
				activeLogicalUnit = TRUE;
				ReconcileWithNdasBus();
			}
		} 
		else 
		{
		    NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress = 
				NdasLocationToLogicalUnitAddress(m_NdasLocation);

		    hr = NdasPortCtlQueryNodeAlive(logicalUnitAddress);

			if (SUCCEEDED(hr))
			{
				activeLogicalUnit = TRUE;
				ReconcileWithNdasPort();
			}
		}

		if (!activeLogicalUnit && m_fMountOnReady) 
		{
			if (IsComplete() && !m_fDegradedMode) 
			{
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

					hr = PlugIn(m_mountOnReadyAccess, LdpfFlags, LdpfValues);

					if (FAILED(hr))
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
							"Boot-time mount (%s) failed, error=0x%X\n", 
							this->ToStringA(), GetLastError());
					}
					else
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
							"Boot-time mount (%s) succeeded.\n", this->ToStringA());
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

					hr = PlugIn(GENERIC_READ, LdpfFlags, LdpfValues);

					if (FAILED(hr))
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
							"RO Boot-time mount (%s) failed, error=0x%X\n", 
							this->ToStringA(), GetLastError());
					}
					else
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
							"RO Boot-time mount (%s) succeeded.\n", 
							this->ToStringA());
					}
				}
			} 
			else 
			{
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

	UpdateBindStateAndError();

	//
	// Remove m_NdasLocation if this logical device is not mounted and unmountable anymore
	//
	if (0 != GetNdasLocation() && !IsMountable() && 
		(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != GetStatus() &&
		NDAS_LOGICALDEVICE_STATUS_MOUNTED != GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING != GetStatus()))
	{
		// If the previous status was complete and now is not. unregister ndasscsi location
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		manager.UnregisterNdasLocation(m_NdasLocation, shared_from_this());
		_DeallocateNdasLocation();
	}

	//
	// Set Device Error
	//
	if (IsMountable()) {
		//
		// If confliction source is removed , RAID can be mounted when device is removed.
		// 
		if (!GetNdasLocation()) 
		{
			BOOL fSuccess;
			CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
			// Allocate NdasLocation when mountable.
			_AllocateNdasLocation();
			fSuccess = manager.RegisterNdasLocation(m_NdasLocation, shared_from_this());
			if (!fSuccess) 
			{
				_ASSERT(FALSE);
				//
				// This NdasScisiLocation is already used. User need to resolve this situation using bindtool
				//
				_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
				CNdasEventPublisher& epub = pGetNdasEventPublisher();
				epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
				_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			}
		}	
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

	//
	// Temp fault tolerant RAID work-around.
	// Remove NdasLocation  that we couldn't remove because it was mounted.
	//
	if (0 != GetNdasLocation() && 
		m_fMountable== FALSE && 
		!(LDS_MOUNTED == newStatus || 
		LDS_MOUNT_PENDING == newStatus || 
		LDS_UNMOUNT_PENDING== newStatus)) 
	{
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		manager.UnregisterNdasLocation(m_NdasLocation, shared_from_this());
		_DeallocateNdasLocation();
	}
}

void 
CNdasLogicalDevice::_SetLastDeviceError(
	NDAS_LOGICALDEVICE_ERROR logDevError)
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

	if (m_NdasPort)
	{
		DWORD lun;

		BOOL success = _NdasSystemCfg.GetValueEx(
			m_szRegContainer,
			_T("LastLun"),
			&lun);

		if (!success)
		{
			success = _NdasSystemCfg.GetValueEx(
				_T("LogicalUnits"),
				_T("NextLun"),
				&lun);

			if (!success)
			{
				lun = 0;
			}
		}


		XTLVERIFY(_NdasSystemCfg.SetValueEx(
			_T("LogicalUnits"),
			_T("NextLun"),
			lun + 1));

		XTLVERIFY( _NdasSystemCfg.SetValueEx(
			m_szRegContainer,
			_T("LastLun"),
			lun));

		return lun;
	}
	else
	{
		DWORD SlotNo = 0;

		//
		// Find currently allocate slot number from registry
		//

		BOOL success = _NdasSystemCfg.GetValueEx(
			m_szRegContainer,
			_T("SlotNo"),
			&SlotNo);

		if (!success || 0 == SlotNo) 
		{
			success = _NdasSystemCfg.GetValueEx(
				_T("LogDevices"),
				_T("LastSlotNo"), 
				&SlotNo);

			if (!success) 
			{
				SlotNo = 10000;
			}
			else 
			{
				SlotNo++;
			}

			success = _NdasSystemCfg.SetValueEx(
				_T("LogDevices"),
				_T("LastSlotNo"), 
				SlotNo);

			success = _NdasSystemCfg.SetValueEx(
				m_szRegContainer,
				_T("SlotNo"),
				SlotNo);		
		}

		return SlotNo;
	}
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
	if (fSuccess || fNoPSWriteShare)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"NoPSWriteShare is set at %s.\n", ToStringA());
		return FALSE;
	}

	// even though NoPSWriteShare is not set, if there is no active
	// LFS filter or Xixfs, then PSWriteShare is denied.
	WORD wNDFSMajor, wNDFSMinor;
	fSuccess = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL, 
		&wNDFSMajor, &wNDFSMinor);
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LFSFilter does not exist.\n");

		fSuccess = ::XixfsCtlGetVersion(
			NULL, NULL, NULL, NULL,
			&wNDFSMajor, &wNDFSMinor);
		if (!fSuccess)
		{

			// LFS nor Xixfs exists or it is not working NoPSWriteShare
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
				"Xixfs does not exist. NoPSWriteShare.\n");
			return FALSE;

		}

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

HRESULT
CNdasLogicalDevice::_IsWriteAccessAllowed(
	BOOL fPSWriteShare,
	CNdasUnitDevicePtr pUnitDevice)
{
	XTLENSURE_RETURN_T(CNdasUnitDeviceNullPtr != pUnitDevice, E_FAIL);

	DWORD nROHosts, nRWHosts;
	BOOL fSuccess = pUnitDevice->GetHostUsageCount(&nROHosts, &nRWHosts, TRUE);
	if (!fSuccess) 
	{
		return NDASSVC_ERROR_NDAS_DEVICE_COMMUNICATION_FAILURE;
	}

	if (nRWHosts > 0)
	{
		if (!fPSWriteShare)
		{
			return NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED;
		}
		else
		{
			fSuccess = pUnitDevice->CheckNDFSCompatibility();
			if (!fSuccess)
			{
				return NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED;
			}
		}
	}

	return S_OK;
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

HRESULT
CNdasLogicalDevice::PlugInNdasBus(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	InstanceAutoLock autolock(this);
	HRESULT hr;
	NDAS_DEV_ACCESSMODE deviceMode;

	BOOL fPSWriteShare = _IsPSWriteShareCapable();

	BOOL fSuccess = _CheckPlugInCondition(requestingAccess);
	if (!fSuccess) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		//
		// Force pdate unit device info.
		// 
		if (hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION ||
			hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING) 
		{
			//
			// If bind information is changed, make it reload.
			//
			Invalidate();
		}
		return hr;
	}

	//
	// Plug In
	// - NDAS Controller
	//

	CNdasUnitDevicePtr pPrimaryUnitDevice = GetPrimaryUnit();

	if (CNdasUnitDeviceNullPtr == pPrimaryUnitDevice) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
		return hr;
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
			hr = HRESULT_FROM_WIN32(GetLastError());
			return hr;
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
		sizeof(NDASBUS_ADD_TARGET_DATA) - sizeof(NDASBUS_UNITDISK) +
		GetUnitDeviceCount() * sizeof(NDASBUS_UNITDISK);

	DWORD cbAddTargetDataSize = 
		cbAddTargetDataSizeWithoutBACL + pPrimaryUnitDevice->GetBACLSize();

	PNDASBUS_ADD_TARGET_DATA pAddTargetData = 
		(PNDASBUS_ADD_TARGET_DATA) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbAddTargetDataSize);

	if (NULL == pAddTargetData) 
	{
		// TODO: Out of memory
		return E_OUTOFMEMORY;
	}

	//
	//	Determine device mode
	//
	XTLASSERT(requestingAccess & GENERIC_READ);
	if (requestingAccess & GENERIC_WRITE) 
	{
		if (NdasIsLogicalDiskType(m_logicalDeviceGroup.Type))
		{
			if (fPSWriteShare) 
			{
				deviceMode = DEVMODE_SHARED_READWRITE;
			}
			else 
			{
				deviceMode = DEVMODE_EXCLUSIVE_READWRITE;
			}
		}
		else
		{
			deviceMode = DEVMODE_EXCLUSIVE_READWRITE;
		}
	} 
	else 
	{
		deviceMode = DEVMODE_SHARED_READONLY;
	}

	// automatically free the heap when it goes out of the scope
	XTL::AutoProcessHeap autoHeap = pAddTargetData;

	UCHAR targetType = LogicalDeviceTypeToNdasBusTargetType(m_logicalDeviceGroup.Type);

	pAddTargetData->ulSize = cbAddTargetDataSize;
	pAddTargetData->ulSlotNo = m_NdasLocation;
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
	if (0 != pPrimaryUnitDevice->GetBACLSize())
	{
		pAddTargetData->BACLOffset = cbAddTargetDataSizeWithoutBACL;
		pPrimaryUnitDevice->FillBACL(((BYTE *)pAddTargetData) + pAddTargetData->BACLOffset);
	}

	if (NDASSCSI_TYPE_DISK_RAID1 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4 == targetType ||
		NDASSCSI_TYPE_DISK_RAID1R2 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R2 == targetType ||
		NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
		NDASSCSI_TYPE_DISK_RAID5 == targetType )
	{
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());
		XTLASSERT(NULL != pUnitDiskDevice->GetAddTargetInfo());

		::CopyMemory(
			&pAddTargetData->RAID_Info,
			pUnitDiskDevice->GetAddTargetInfo(),
			sizeof(NDAS_RAID_INFO));

		if (0 == pAddTargetData->RAID_Info.BlocksPerBit) 
		{
			hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION;
			return hr;
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
			hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
			return hr;
		}

		pDevice = pUnitDevice->GetParentDevice();
		XTLASSERT(CNdasDeviceNullPtr != pDevice);
#else 	
		pUnitDevice = GetUnitDevice(i);
		//
		// Handle degraded mount.
		//
		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{	
			//
			// TODO: Create CMissingNdasUnit class?
			// Missing member. This is allowed only for redundent disk.
			//
			if (NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID5 == targetType) 
			{
				// For redundent RAID, get device information.
				pDevice = pGetNdasDevice(GetLDGroup().UnitDevices[i].DeviceId);
				if (CNdasDeviceNullPtr == pDevice) 
				{
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
					return hr;
				}
			} 
			else 
			{
				// for non-redundent bind, reject plugin
				Invalidate();
				hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
				return hr;
			}		
		} 
		else 
		{
			pDevice = pUnitDevice->GetParentDevice();
			XTLASSERT(CNdasDeviceNullPtr != pDevice);

			ldGroup = pUnitDevice->GetLDGroup();
			if (::memcmp(&ldGroup, &m_logicalDeviceGroup, sizeof(ldGroup)) != 0) 
			{
				//
				// This device is online but not is not configured as a member of this logical device.
				//		or not yet recognized by svc.
				if (NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
					NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
					NDASSCSI_TYPE_DISK_RAID5 == targetType) 
				{
					//
					// For redundent RAID, Handle this unit as missing member. ndasscsi will handle it for redundent RAID.
					//
					pUnitDevice = CNdasUnitDeviceNullPtr;
				}
				else 
				{
					// In other RAID, missing member is not acceptable. This may caused by inconsistent logical information.
					// Refresh it.
					Invalidate();
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
					return hr;
				}
			} 
		}
#endif

		PNDASBUS_UNITDISK pud = &pAddTargetData->UnitDiskList[i];

		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			// Temp fix. pDevice's remote address is not initialized until it has been online at least one moment.		
			// Use address from LDgroup.
			::CopyMemory(
				pud->Address.Node, 
				GetLDGroup().UnitDevices[i].DeviceId.Node, 
				sizeof(pud->Address.Node));
		} 
		else 
		{
			::CopyMemory(
				pud->Address.Node, 
				pDevice->GetRemoteLpxAddress().Node, 
				sizeof(pud->Address.Node));
		}
		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);

		C_ASSERT(
			sizeof(pud->NICAddr.Node) ==
			sizeof(pDevice->GetLocalLpxAddress().Node));

		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			// Temp fix. pDevice's remote address is not initialized until it has been online at least one moment.		
			ZeroMemory(&pud->NICAddr.Node, sizeof(pud->NICAddr.Node));
		}
		else 
		{
			::CopyMemory(
				pud->NICAddr.Node, 
				pDevice->GetLocalLpxAddress().Node, 
				sizeof(pud->NICAddr.Node));
		}
		
		pud->NICAddr.Port = htons(0); // should be zero

		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			// This is missing member. This is temp fix.
			// iUserID and iPassword will not work for future version!!.
			pud->iUserID = CNdasUnitDevice::GetDeviceUserID(GetLDGroup().UnitDevices[i].UnitNo, requestingAccess);
			pud->iPassword = pDevice->GetHardwarePassword();
			
			pud->ulUnitBlocks = pPrimaryUnitDevice->GetUserBlockCount(); //  Assume RAID1, RAID4 and use primary device's
			pud->ulPhysicalBlocks = 0; // Unknown..
			pud->ucUnitNumber = static_cast<UCHAR>(GetLDGroup().UnitDevices[i].UnitNo);

			pud->ucHWType = 0/*HW_TYPE_ASIC*/; 		// Don't know right now..
			pud->ucHWVersion = GetLDGroup().DeviceHwVersions[i]; // Use hint from DIB
			pud->ucHWRevision = 0;	// Don't know right now..
			pud->LurnOptions |= LURNOPTION_MISSING;
		}
		else 
		{
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
		
		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			//
			// Temp fix: This may not work for some HW such as emulator.
			// Driver also limit Max transfer size if needed.
			pud->UnitMaxDataRecvLength = pReadMaxRequestBlockLimitConfig(pud->ucHWVersion) * 512;
			pud->UnitMaxDataSendLength = pud->UnitMaxDataRecvLength;
		}
		else 
		{
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
			if (!pUnitDiskDevice->HasSameDIBInfo())
			{
				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());

				//
				// Force refresh
				//
				CNdasEventPublisher& epub = pGetNdasEventPublisher();
				epub.LogicalDeviceRelationChanged(m_logicalDeviceId);				
				hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
				return hr;
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
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_CORRUPTED_BIND_INFORMATION;
					return hr;
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
		hr = S_OK;

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
			hr = _IsWriteAccessAllowed(fPSWriteShare, pPrimaryUnitDevice);
			if (SUCCEEDED(hr)) 
			{
				break;
			}
		}

		if (FAILED(hr))
		{
			return hr;
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
		"NdasBusCtlPlugInEx2, SlotNo=%08X, MaxReqBlock=%d, DisEvt=%p, RecEvt=%p\n",
		m_NdasLocation, 
		dwMaxRequestBlocks, 
		m_hDisconnectedEvent, 
		m_hAlarmEvent);

	SetReconnectFlag(FALSE);

	BOOL fVolatileRegister = IsVolatile();

	_ASSERT(m_NdasLocation);
	
	fSuccess = NdasBusCtlPlugInEx2(
		m_NdasLocation,
		dwMaxRequestBlocks,
		m_hDisconnectedEvent,
		m_hAlarmEvent,
		fVolatileRegister);

	if (!fSuccess) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlPlugInEx2 failed, hr=0x%X\n", hr);
		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LsBusCtlPlugInEx completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	fSuccess = NdasBusCtlAddTarget(pAddTargetData);
	if (!fSuccess) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlAddTarget failed, hr=0x%X\n", hr);

		Sleep(1000);

		NdasBusCtlEject(m_NdasLocation);

		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasBusCtlAddTarget completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

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

	SetAdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"PlugIn completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

static
struct {
	USHORT		LurDeviceType;
	BOOL		CanHaveChildren;
	LURN_TYPE	ChildNodeType;
} LurnTypeTable[] = {
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_AGGREGATION
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_MIRRORING
		{LUR_DEVTYPE_HDD, FALSE, LURN_NULL},  // LURN_IDE_DISK
		{LUR_DEVTYPE_ODD, FALSE, LURN_NULL},  // LURN_IDE_ODD
		{LUR_DEVTYPE_MOD, FALSE, LURN_NULL},  // LURN_IDE_MO
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_RAID1	obsolete
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_RAID4	obsolete
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_RAID0
		{LUR_DEVTYPE_HDD, FALSE, LURN_NULL},  // LURN_AOD
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_RAID1R
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_RAID4R
		{LUR_DEVTYPE_HDD,  TRUE, LURN_IDE_DISK},  // LURN_RAID5		
};

HRESULT
CNdasLogicalDevice::PlugInNdasPort(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	HRESULT hr;

	InstanceAutoLock autolock(this);
	NDAS_DEV_ACCESSMODE deviceMode;

	BOOL fPSWriteShare = _IsPSWriteShareCapable();

	BOOL fSuccess = _CheckPlugInCondition(requestingAccess);
	if (!fSuccess) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		//
		// Force pdate unit device info.
		// 
		if (hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION ||
			hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING) 
		{
			//
			// If bind information is changed, make it reload.
			//
			Invalidate();
		}
		return hr;
	}

	//
	// Plug In
	// - NDAS Controller
	//

	CNdasUnitDevicePtr pPrimaryUnitDevice = GetPrimaryUnit();

	if (CNdasUnitDeviceNullPtr == pPrimaryUnitDevice) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
		return hr;
	}

	CNdasDevicePtr pPrimaryNdasDevice = pPrimaryUnitDevice->GetParentDevice();
	if (CNdasDeviceNullPtr == pPrimaryNdasDevice) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
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
			hr = HRESULT_FROM_WIN32(GetLastError());
			return hr;
		}
	}
#endif

	//
	// Resetting an event always succeeds if the handle is valid
	//
	XTLVERIFY( ::ResetEvent(m_hDisconnectedEvent) );
	XTLVERIFY( ::ResetEvent(m_hAlarmEvent) );
#if 0
	//
	// Add Target
	// - Add a disk device to the NDAS controller
	//

	DWORD cbAddTargetDataSizeWithoutBACL =  
		sizeof(NDASBUS_ADD_TARGET_DATA) - sizeof(NDASBUS_UNITDISK) +
		GetUnitDeviceCount() * sizeof(NDASBUS_UNITDISK);
	DWORD cbAddTargetDataSize = 
		cbAddTargetDataSizeWithoutBACL + pPrimaryUnitDevice->GetBACLSize();

	PNDASBUS_ADD_TARGET_DATA pAddTargetData = 
		(PNDASBUS_ADD_TARGET_DATA) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbAddTargetDataSize);
#endif


	//
	//	Determine device mode
	//
	XTLASSERT(requestingAccess & GENERIC_READ);
	if (requestingAccess & GENERIC_WRITE) 
	{
		if (NdasIsLogicalDiskType(m_logicalDeviceGroup.Type))
		{
			if (fPSWriteShare) 
			{
				deviceMode = DEVMODE_SHARED_READWRITE;
			}
			else 
			{
				deviceMode = DEVMODE_EXCLUSIVE_READWRITE;
			}
		}
		else
		{
			deviceMode = DEVMODE_EXCLUSIVE_READWRITE;
		}
	} 
	else 
	{
		deviceMode = DEVMODE_SHARED_READONLY;
	}

	PNDAS_LOGICALUNIT_DESCRIPTOR logicalUnitDescriptor;
	NDASPORTCTL_NODE_INITDATA rootNodeInitData;
	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress;
	LURN_TYPE nodeType = LogicalDeviceTypeToLurTargetType(m_logicalDeviceGroup.Type);
	BOOL rootNodeExists  = FALSE;
	USHORT lurDeviceType;

	if (nodeType >= RTL_NUMBER_OF(LurnTypeTable)) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
		hr = NDASSVC_ERROR_UNSUPPORTED_LOGICALDEVICE_TYPE;
		return hr;
	}

	//
	// LUR init data
	// Root node init data
	//
	ndasLogicalUnitAddress = NdasLocationToLogicalUnitAddress(m_NdasLocation);

	rootNodeExists = LurnTypeTable[nodeType].CanHaveChildren;
	lurDeviceType = LurnTypeTable[nodeType].LurDeviceType;

	rootNodeInitData.NodeType = nodeType;
	rootNodeInitData.StartLogicalBlockAddress.QuadPart = 0;

	if (nodeType == LURN_RAID0) 
	{
		//
		// Current RAID0 uses block concatenation, so forms a large block
		// by adding each bock of member disks.
		//
		fSuccess = 	NdasPortCtlGetRaidEndAddress(
			nodeType,
			GetUserBlockCount() - 1,
			GetUnitDeviceCount(),
			&rootNodeInitData.EndLogicalBlockAddress.QuadPart);
		XTLASSERT(fSuccess);
		if (!fSuccess) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
			hr = NDASSVC_ERROR_UNSUPPORTED_LOGICALDEVICE_TYPE;
			return hr;
		}
	} 
	else 
	{
		rootNodeInitData.EndLogicalBlockAddress.QuadPart = GetUserBlockCount() - 1;
	}

	if (nodeType == LURN_RAID1R ||
		nodeType == LURN_RAID4R ||
		nodeType == LURN_RAID5) 
	{
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());

		PNDAS_RAID_INFO	infoRaid = (PNDAS_RAID_INFO)
			pUnitDiskDevice->GetAddTargetInfo();

		XTLASSERT(NULL != pUnitDiskDevice->GetAddTargetInfo());

		if (0 == infoRaid->BlocksPerBit) 
		{
			hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION;
			return hr;
		}

		rootNodeInitData.NodeSpecificData.Raid.BlocksPerBit = infoRaid->BlocksPerBit;
		rootNodeInitData.NodeSpecificData.Raid.SpareDiskCount = infoRaid->SpareDiskCount;
		CopyMemory(&rootNodeInitData.NodeSpecificData.Raid.NdasRaidId,
			&infoRaid->NdasRaidId, sizeof(GUID));
		CopyMemory(
			&rootNodeInitData.NodeSpecificData.Raid.ConfigSetId,
			&infoRaid->ConfigSetId, sizeof(GUID));
	}

	LARGE_INTEGER logicalUnitEndAddress;
	logicalUnitEndAddress.QuadPart = rootNodeInitData.EndLogicalBlockAddress.QuadPart;
	logicalUnitDescriptor = NdasPortCtlBuildNdasDluDeviceDescriptor(
		ndasLogicalUnitAddress,
		0,
		deviceMode,
		lurDeviceType,
		&logicalUnitEndAddress,
		rootNodeExists?GetUnitDeviceCount():0, // Leaf count
		0,
		pPrimaryUnitDevice->GetBACLSize(),
		rootNodeExists?(&rootNodeInitData):NULL);

	if (NULL == logicalUnitDescriptor) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	// automatically free the heap when it goes out of the scope
	XTL::AutoProcessHeap autoHeap = logicalUnitDescriptor;
	PNDAS_DLU_DESCRIPTOR ndasDluDesc = (PNDAS_DLU_DESCRIPTOR)logicalUnitDescriptor;
	PLURELATION_DESC lurDesc = &ndasDluDesc->LurDesc;

	// Get Default LUR Options
	lurDesc->LurOptions = NdasServiceConfig::Get(nscLUROptions);

	// Set LdpfFlags
	SetLurOptionsFromMappings(
		lurDesc->LurOptions, 
		LdpfFlags, 
		LdpfValues);

	// Set Content Encryption from the primary unit device
	// (Only for Disk Devices)
	if (NDAS_UNITDEVICE_TYPE_DISK == pPrimaryUnitDevice->GetType()) 
	{
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice.get());

		const NDAS_CONTENT_ENCRYPT& encrypt = pUnitDiskDevice->GetEncryption();
		NDAS_DISK_ENCRYPTION_DESCRIPTOR diskEncDesc;

		XTLASSERT(encrypt.KeyLength <= 0xFF);
		XTLASSERT(encrypt.Method <= 0xFF);

		diskEncDesc.EncryptKeyLength = static_cast<UCHAR>(encrypt.KeyLength);
		diskEncDesc.EncryptType = static_cast<NDAS_DISK_ENCRYPTION_TYPE>(encrypt.Method);
		::CopyMemory(
			diskEncDesc.EncryptKey,
			encrypt.Key,
			encrypt.KeyLength);

		//
		// logicalUnitDescriptor may be reallocated in the function
		// NdasPortCtlSetNdasDiskEncryption. We should detach the
		// managed resource and attach it again after the function call.
		// Note that logicalUnitDescriptor will be remain as it is
		// when the function fails and we should free the buffer even so.
		//

		autoHeap.Detach();

		hr = NdasPortCtlSetNdasDiskEncryption(
			&logicalUnitDescriptor, 
			&diskEncDesc);

		autoHeap.Attach(logicalUnitDescriptor);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlSetNdasDiskEncryption failed, hr=0x%X\n", hr);
			return hr;
		}
		
	}

	// set BACL data
	if (0 != pPrimaryUnitDevice->GetBACLSize())
	{
		PNDAS_BLOCK_ACL bacl;
		hr = NdasPortCtlGetNdasBacl(logicalUnitDescriptor, &bacl);
		_ASSERT(SUCCEEDED(hr));
		pPrimaryUnitDevice->FillBACL(bacl);
	}

	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) 
	{	
		CNdasDevicePtr pDevice;
		CNdasUnitDevicePtr pUnitDevice;
		NDAS_LOGICALDEVICE_GROUP ldGroup;
		ULONG				idx_lurn = i + (rootNodeExists?1:0);

		pUnitDevice = GetUnitDevice(i);
		// Handle degraded mount.
		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{	
			// TODO: Create CMissingNdasUnit class?
			// Missing member. This is allowed only for redundent disk.
			if (LURN_RAID1R == nodeType ||
				LURN_RAID4R == nodeType ||
				LURN_RAID5 == nodeType ) 
			{
				// For redundent RAID, get device information.
				pDevice = pGetNdasDevice(GetLDGroup().UnitDevices[i].DeviceId);
				if (CNdasDeviceNullPtr == pDevice) 
				{
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
					return hr;
				}
			}
			else 
			{
				// for non-redundent bind, reject plugin
				Invalidate();
				hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
				return hr;		
			}		
		} 
		else 
		{
			pDevice = pUnitDevice->GetParentDevice();
			XTLASSERT(CNdasDeviceNullPtr != pDevice);

			ldGroup = pUnitDevice->GetLDGroup();
			if (::memcmp(&ldGroup, &m_logicalDeviceGroup, sizeof(ldGroup)) != 0) 
			{
				//
				// This device is online but not is not configured as a member of this logical device.
				//		or not yet recognized by svc.
				if (LURN_RAID1R == nodeType ||
					LURN_RAID4R == nodeType ||
					LURN_RAID5 == nodeType) 
				{
					//
					// For redundent RAID, Handle this unit as missing member. ndasscsi will handle it for redundent RAID.
					//
					pUnitDevice = CNdasUnitDeviceNullPtr;
				}
				else 
				{
					// In other RAID, missing member is not acceptable. This may caused by inconsistent logical information.
					// Refresh it.
					Invalidate();
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
					return hr;
				}
			} 
		}

		PLURELATION_NODE_DESC lurNode = NdasPortCtlFindNodeDesc(logicalUnitDescriptor, idx_lurn);
		NDASPORTCTL_NODE_INITDATA nodeInitData;
		PNDASPORTCTL_INIT_ATADEV ataSpecific = &nodeInitData.NodeSpecificData.Ata;

		nodeInitData.NodeType = rootNodeExists ?
			LurnTypeTable[nodeType].ChildNodeType : nodeType;

		ataSpecific->ValidFieldMask =
				NDASPORTCTL_ATAINIT_VALID_TRANSPORT_PORTNO |
				NDASPORTCTL_ATAINIT_VALID_BINDING_ADDRESS |
				NDASPORTCTL_ATAINIT_VALID_USERID |
				NDASPORTCTL_ATAINIT_VALID_USERPASSWORD |
				NDASPORTCTL_ATAINIT_VALID_OEMCODE;

		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			nodeInitData.StartLogicalBlockAddress.QuadPart = 0;
			//
			//  Assume RAID1, RAID4 and use primary device's
			//
			nodeInitData.EndLogicalBlockAddress.QuadPart = 
				pPrimaryUnitDevice->GetUserBlockCount() - 1; 

			//
			// Temp fix. pDevice's remote address is not initialized until it has been online at least one moment.		
			// Use address from LDgroup.
			//
			::CopyMemory(
				ataSpecific->DeviceIdentifier.Identifier, 
				GetLDGroup().UnitDevices[i].DeviceId.Node, 
				sizeof(ataSpecific->DeviceIdentifier.Identifier));
			//
			// Temp fix. pDevice's remote address is not initialized until it has been online at least one moment.
			//
			ZeroMemory(&ataSpecific->BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
			ataSpecific->DeviceIdentifier.UnitNumber = static_cast<UCHAR>(GetLDGroup().UnitDevices[i].UnitNo);
			ataSpecific->HardwareType = 0; 	/*HW_TYPE_ASIC*/	// Don't know right now..
			ataSpecific->HardwareVersion = GetLDGroup().DeviceHwVersions[i]; // Use hint from DIB
			ataSpecific->HardwareRevision = 0;	// Don't know right now..
			ataSpecific->TransportPortNumber = htons(LPXRP_NDAS_PROTOCOL);
			// lurNode->ulPhysicalBlocks = 0; // Unknown..
			ataSpecific->UserId = CNdasUnitDevice::GetDeviceUserID(GetLDGroup().UnitDevices[i].UnitNo, requestingAccess);
			ZeroMemory(ataSpecific->UserPassword, NDASPORTCTL_USERPASSWORD_LENGTH);

			UINT64	hardwareDefaultOemCode = pDevice->GetHardwarePassword();
			CopyMemory(ataSpecific->DeviceOemCode, &hardwareDefaultOemCode, NDASPORTCTL_OEMCODE_LENGTH);
		} 
		else 
		{
			nodeInitData.StartLogicalBlockAddress.QuadPart = 0;
			nodeInitData.EndLogicalBlockAddress.QuadPart = pUnitDevice->GetUserBlockCount() - 1;

			::CopyMemory(
				ataSpecific->DeviceIdentifier.Identifier, 
				pDevice->GetRemoteLpxAddress().Node, 
				sizeof(ataSpecific->DeviceIdentifier.Identifier));
			LpxCommConvertLpxAddressToTaLsTransAddress(
				&pDevice->GetLocalLpxAddress(),
				&ataSpecific->BindingAddress);
			ataSpecific->DeviceIdentifier.UnitNumber = static_cast<UCHAR>(pUnitDevice->GetUnitNo());
			ataSpecific->HardwareType =pDevice->GetHardwareType();;
			ataSpecific->HardwareVersion = pDevice->GetHardwareVersion();
			ataSpecific->HardwareRevision = static_cast<UCHAR>(pDevice->GetHardwareRevision());	
			ataSpecific->TransportPortNumber = htons(LPXRP_NDAS_PROTOCOL);
			ataSpecific->UserId = pUnitDevice->GetDeviceUserID(requestingAccess);
			ZeroMemory(ataSpecific->UserPassword, NDASPORTCTL_USERPASSWORD_LENGTH);

			UINT64	unitDefaultOemCode = pUnitDevice->GetDevicePassword();

			CopyMemory(
				ataSpecific->DeviceOemCode, 
				&unitDefaultOemCode, 
				NDASPORTCTL_OEMCODE_LENGTH);

			// lurNode->ulPhysicalBlocks = pUnitDevice->GetPhysicalBlockCount();
		}

		ataSpecific->TransportPortNumber = NDAS_DEVICE_LPX_PORT;

		if ( NdasPortCtlSetupLurNode(lurNode, deviceMode, &nodeInitData) == FALSE ) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
			hr = NDASSVC_ERROR_UNSUPPORTED_LOGICALDEVICE_TYPE;
			return hr;
		}

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

				lurNode->LurnOptions |= LURNOPTION_SET_RECONNECTION;
				lurNode->ReconnTrial = dwReconnect;
				lurNode->ReconnInterval = dwReconnectInterval;
			}
		}

		//
		// Get the optimal data send/receive length.
		// TODO: separate send and receive data length.
		//       Get request length in bytes.
		//
		
		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			//
			// Temp fix: This may not work for some HW such as emulator.
			// Driver also limit Max transfer size if needed.
			lurNode->MaxDataRecvLength = pReadMaxRequestBlockLimitConfig(
				nodeInitData.NodeSpecificData.Ata.HardwareVersion) * 512;
			lurNode->MaxDataSendLength = lurNode->MaxDataRecvLength;
		}
		else 
		{
			lurNode->MaxDataSendLength = pUnitDevice->GetOptimalMaxRequestBlock() * 512;
			lurNode->MaxDataRecvLength = pUnitDevice->GetOptimalMaxRequestBlock() * 512;
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
			if (!pUnitDiskDevice->HasSameDIBInfo())
			{
				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());

				//
				// Force refresh
				//
				CNdasEventPublisher& epub = pGetNdasEventPublisher();
				epub.LogicalDeviceRelationChanged(m_logicalDeviceId);				
				hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
				return hr;
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
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_CORRUPTED_BIND_INFORMATION;
					return hr;
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

		hr = S_OK;
		for (DWORD i = 0; i < dwMaxNDFSCompatCheck; ++i)
		{
			hr = _IsWriteAccessAllowed(fPSWriteShare, pPrimaryUnitDevice);
			if (SUCCEEDED(hr)) 
			{
				break;
			}
		}

		if (FAILED(hr))
		{
			return hr;
		}
	}
		

	// After this, we used up a Disconnected flag, so we can clear it.
	m_fDisconnected = FALSE;

	//
	//	We don't need an algorithm for a SCSI adapter's max request blocks
	//	We just need one more registry key to override.
	//	We set 0 to use driver's default max request blocks.
	//

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"PlugIn, SlotNo=%08X\n",
		m_NdasLocation);

	SetReconnectFlag(FALSE);

	//
	// Call the driver to plug in
	//

	XTL::AutoFileHandle handle;

	hr = NdasPortCtlCreateControlDevice(
		GENERIC_READ | GENERIC_WRITE, &handle);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasPortCtlCreateControlDevice failed, hr=0x%X\n", hr);

		return hr;
	}

	logicalUnitDescriptor->Address.PortNumber = 0;

	hr = NdasPortCtlPlugInLogicalUnit(
		handle,
		logicalUnitDescriptor);

	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasPortCtlPlugInLogicalUnit failed, hr=0x%X\n", hr);

		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasPortCtlPlugInLogicalUnit completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

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

	SetAdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"PlugIn completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

HRESULT
CNdasLogicalDevice::PlugIn(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	if (m_NdasPort == FALSE)
	{
		return PlugInNdasBus(requestingAccess, LdpfFlags, LdpfValues);
	}
	else 
	{
		return PlugInNdasPort(requestingAccess, LdpfFlags, LdpfValues);
	}
}

HRESULT
CNdasLogicalDevice::Unplug()
{
	HRESULT hr;
	BOOL fSuccess(FALSE);

	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplugging, LogicalDevice=%s\n", ToStringA());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED;
		return hr;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED &&
		m_status != NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING &&
		m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING) 
	{
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED;
		return hr;
	}

	//
	// Call the driver
	//

	if (m_NdasPort == FALSE) 
	{
		//
		// Remove target ejects the disk and the volume.
		//

		fSuccess = NdasBusCtlRemoveTarget(m_NdasLocation);
		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
				"NdasBusCtlRemoveTarget failed, error=0x%X\n", GetLastError());
		}

		// Intentional break
		::Sleep(100);

		//
		// BUG:
		// What happened when RemoveTarget succeeded and 
		// Unplugging LANSCSI port is failed?
		//

		fSuccess = NdasBusCtlUnplug(m_NdasLocation);

		if (!fSuccess)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}
	else 
	{
		XTL::AutoFileHandle handle;
		
		hr = NdasPortCtlCreateControlDevice(
			GENERIC_READ | GENERIC_WRITE, &handle);

		if (FAILED(hr))
	    {
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			    "NdasPortCtlCreateControlDevice failed, hr=0x%X\n", hr);

			return hr;
	    }

		NDAS_LOGICALUNIT_ADDRESS address = GetNdasLogicalUnitAddress();

	    hr = NdasPortCtlUnplugLogicalUnit(handle, address, 0);

	    if (FAILED(hr)) 
	    {
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			    "NdasPortCtlUnplugLogicalUnit failed, hr=0x%X\n", hr);

			return hr;
	    }
	}


#if 0
	//
	// Change the status to unmounted
	//

	if (_IsRaid()) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"Invalidating RAID after unplugging to update RAID information, SlotNo=%08X\n",
			m_NdasLocation);

		//
		// If this is RAID, RAID configuration may have been changed by kernel 
		// But the change is ignored while it is mounted.
		// So we need to update it now.
		//
		Invalidate();
	}
#endif

	UpdateBindStateAndError();	

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Remove Ldpf from the registry
	(void) ClearLastLdpf();

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplug completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

HRESULT 
CNdasLogicalDevice::Eject()
{
	InstanceAutoLock autolock(this);

	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Ejecting, LogicalDevice=%s\n", ToStringA());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED;

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Eject is requested to non-initialized logical device");

		return hr;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
	{
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED;

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Eject is requested to not mounted logical device\n");

		return hr;
	}

	//
	// Call the driver
	//

	BOOL fSuccess;
	if (m_NdasPort == FALSE) 
	{
		fSuccess = ::NdasBusCtlEject(m_NdasLocation);

		if (!fSuccess) 
		{
			hr = HRESULT_FROM_WIN32(GetLastError());

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlEject failed, SlotNo=%08X, error=0x%X\n", 
				m_NdasLocation, hr);

			return hr;
		}
	}
	else
	{
	    XTL::AutoFileHandle handle;

		hr = NdasPortCtlCreateControlDevice(
			GENERIC_READ | GENERIC_WRITE,
			&handle);

	    if (FAILED(hr))
	    {
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			    "NdasPortCtlCreateControlDevice failed, hr=0x%X\n", hr);

			return hr;
	    }

		NDAS_LOGICALUNIT_ADDRESS address = GetNdasLogicalUnitAddress();

		hr = NdasPortCtlGetPortNumber(handle, &address.PortNumber);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			    "NdasPortCtlGetPortNumber failed, hr=0x%X\n", hr);

			return hr;
	    }
    
	    hr = NdasPortCtlEjectLogicalUnit(handle, address, 0);

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlEjectLogicalUnit failed, SlotNo=%08X, hr=0x%X\n", 
				m_NdasLocation, hr);

			return hr;
		}
	}

#if 0
	if (_IsRaid()) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"Invalidating RAID after unplugging to update RAID information, SlotNo=%08X\n",
			m_NdasLocation);

		//
		// If this is RAID, RAID configuration may have been changed by kernel 
		// But the change is ignored while it is mounted.
		// So we need to update it now.
		//
		Invalidate();
	}
#endif

	UpdateBindStateAndError();	
	
	//
	// Now we have to wait until the ejection is complete
	//
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Eject completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

HRESULT
CNdasLogicalDevice::EjectEx(
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName,
	DWORD nNameLength)
{
	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"EjectEx, LogicalDevice=%s\n", ToStringA());

	Lock();

	NDAS_LOCATION ndasLocation = GetNdasLocation();
	NDAS_LOGICALDEVICE_STATUS status = GetStatus();

	if (status != NDAS_LOGICALDEVICE_STATUS_MOUNTED)
	{
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED;

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"EjectEx is requested to not mounted logical device\n");

		Unlock();
		return hr;
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	Unlock();

	CONFIGRET cret;

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	if (m_NdasPort == FALSE) 
	{
	    hr = pRequestNdasScsiDeviceEjectW(
		    ndasLocation, 
		    &cret, 
		    pVetoType, 
		    pszVetoName, 
		    nNameLength);
	}
	else 
	{
		//
		// Get the port number
		//

		XTL::AutoFileHandle handle;

		hr = NdasPortCtlCreateControlDevice(
			GENERIC_READ | GENERIC_WRITE,
			&handle);
		
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlCreateControlDevice failed, hr=0x%X\n", hr);
			return hr;
		}

		NDAS_LOGICALUNIT_ADDRESS lun;
		lun = GetNdasLogicalUnitAddress();

		hr = NdasPortCtlGetPortNumber(handle, &lun.PortNumber);
		
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlGetPortNumber failed, hr=0x%X\n", hr);
			return hr;
		}

		handle.Release();

		hr = pRequestNdasPortDeviceEjectW(
			lun.Address,
			&cret, 
			pVetoType, 
			pszVetoName, 
			nNameLength);
	}

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"RequestEject failed, ndasLocation=%d, hr=0x%X\n", ndasLocation, hr);
		_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
	}
	else if (S_FALSE == hr)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"RequestEject failed, ndasLocation=%d, cret=0x%X\n", ndasLocation, cret);

		if (pszVetoName)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Vetoed by %ls\n", pszVetoName);
		}

#ifndef ERROR_PLUGPLAY_QUERY_VETOED
#define ERROR_PLUGPLAY_QUERY_VETOED      683L
#endif
		hr = HRESULT_FROM_WIN32(ERROR_PLUGPLAY_QUERY_VETOED);
	}
	else
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"RequestEjectEx completed successfully, ndasLocation=%d\n", ndasLocation);
	}

	if (pConfigRet) 
	{
		*pConfigRet = cret;
	}

	return hr;
}

BOOL 
CNdasLogicalDevice::GetSharedWriteInfo(
	LPBOOL lpbSharedWrite, 
	LPBOOL lpbPrimary)
{
	HRESULT hr;
	
#ifdef NDAS_FEATURE_DISABLE_SHARED_WRITE
	if (lpbSharedWrite) *lpbSharedWrite = FALSE;
	if (lpbPrimary) *lpbPrimary = FALSE;
	return TRUE;
#endif

	NDASBUS_QUERY_INFORMATION BusEnumQuery = {0};
	NDASBUS_INFORMATION BusEnumInformation = {0};

	//
	// Logical device address for NDAS port.
	//

	NDAS_LOGICALUNIT_ADDRESS ndasLogicalDeviceAddress
		= NdasLocationToLogicalUnitAddress(m_NdasLocation);

	//
	// Call the NDAS port driver to get the port number of the logical device address.
	// If failure, assume NDAS port not exist, and try with NDAS bus.
	//
	BOOL fSuccess = FALSE;
	BOOL ndasPort = FALSE;

	XTL::AutoFileHandle ndasPortHandle;

	hr = NdasPortCtlCreateControlDevice(
		GENERIC_READ | GENERIC_WRITE,
		&ndasPortHandle);
	
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasPortCtlCreateControlDevice failed, hr=0x%X\n", hr);
		XTLASSERT(FALSE && "GetSharedWriteInfo Failed");
	}
	else 
	{
		hr = NdasPortCtlGetPortNumber(
			ndasPortHandle, 
			&ndasLogicalDeviceAddress.PortNumber);
		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlGetPortNumber failed, hr=0x%X\n", hr);
			XTLASSERT(FALSE && "GetSharedWriteInfo Failed");
			return FALSE;
		}

		//
		// Indicate NDAS port is detected.
		//
		ndasPort = TRUE;
	}

	//
	// Get default primary/secondary information
	//

	NDAS_DEV_ACCESSMODE	deviceMode;
	DWORD				supportedFeatures, enabledFeatures, connectionCount;

	if (ndasPort == FALSE) 
	{
		//
		// the NDAS bus driver.
		//

		BusEnumQuery.InfoClass = INFORMATION_PDO;
		BusEnumQuery.Size = sizeof(NDASBUS_QUERY_INFORMATION);
		BusEnumQuery.SlotNo = m_NdasLocation;
		BusEnumQuery.Flags = 0;

		fSuccess = ::NdasBusCtlQueryInformation(
			&BusEnumQuery,
			sizeof(NDASBUS_QUERY_INFORMATION),
			&BusEnumInformation,
			sizeof(NDASBUS_INFORMATION));
		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlQueryInformation failed at slot %d\n", 
				m_NdasLocation);
			return FALSE;
		}

		deviceMode = BusEnumInformation.PdoInfo.DeviceMode;
		supportedFeatures = BusEnumInformation.PdoInfo.SupportedFeatures;
		enabledFeatures = BusEnumInformation.PdoInfo.EnabledFeatures;

	}
	else 
	{
		XTL::AutoFileHandle storageDeviceHandle = CreateFile(
			m_DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_DEVICE,
			NULL);

		if (INVALID_HANDLE_VALUE == storageDeviceHandle)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"CreateFile(%ls) failed, error=0x%X\n",
				m_DevicePath, GetLastError());
			return FALSE;
		}
		
		//
		// Get default primary/secondary information from the NDAS port driver.
		//
		hr = NdasPortCtlQueryDeviceMode(
			storageDeviceHandle,
			ndasLogicalDeviceAddress,
			(PULONG)&deviceMode,
			&supportedFeatures,
			&enabledFeatures,
			&connectionCount);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlQueryDeviceMode failed at slot %X, hr=0x%X\n", 
				m_NdasLocation, hr);
			return FALSE;
		}
	}

	if (deviceMode == DEVMODE_SHARED_READWRITE) 
	{
		if (lpbSharedWrite) *lpbSharedWrite = TRUE;

		if (enabledFeatures & NDASFEATURE_SECONDARY)
		{
			if (lpbPrimary)	*lpbPrimary = FALSE;
		} 
		else 
		{
			if (lpbPrimary) *lpbPrimary = TRUE;
		}
	}
	else 
	{
		if (lpbSharedWrite) *lpbSharedWrite = FALSE;
		if (lpbPrimary) *lpbPrimary = FALSE;
	}

	//
	// If XixFS exists, skip query to the LfsFilter
	//
	// TODO: provide the same protection as LfsFilter gets.
	//
	fSuccess = ::XixfsCtlGetVersion(NULL,NULL,NULL,NULL,NULL,NULL);
	if (fSuccess) 
	{
		if (lpbSharedWrite)
			*lpbSharedWrite = TRUE;
		return TRUE;
	}
	else 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"XixfsCtlGetVersion failed at slot %d\n", 
			m_NdasLocation);
	}


	//
	// Get the NDAS logical device usage from LfsFilter
	//

	LFSCTL_NDAS_USAGE usage = {0};
	if (m_NdasPort == FALSE)
	{
	    fSuccess = ::LfsFiltQueryNdasUsage(
		    m_NdasLocation,
		    &usage);
	}
	else
	{
		fSuccess = ::LfsFiltQueryNdasUsage(
			ndasLogicalDeviceAddress.Address,
			&usage);
	}

	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LfsFiltQueryNdasUsage failed, SlotNo=%08X, error=0x%X\n",
			m_NdasLocation, GetLastError());
		return FALSE;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LfsFiltQueryNdasUsage: SlotNo=%08X, primary=%d, secondary=%d, hasLockedVolume=%d.\n",
		m_NdasLocation,
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

NDAS_LOCATION 
CNdasLogicalDevice::GetNdasLocation()
{
	InstanceAutoLock autolock(this);
	return m_NdasLocation;
}

NDAS_LOGICALUNIT_ADDRESS
CNdasLogicalDevice::GetNdasLogicalUnitAddress()
{
	InstanceAutoLock autolock(this);
	return NdasLocationToLogicalUnitAddress(m_NdasLocation);
}

VOID
CNdasLogicalDevice::SetNdasPortExistence(const BOOL NdasPortExistence)
{
	m_NdasPort = NdasPortExistence;
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


	BOOL MountableStateChanged;
	MountableStateChanged = UpdateBindStateAndError();	

	if (!IsMountable()) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	} else if (MountableStateChanged) {
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
		// To do: use another error code.
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION);
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
CNdasLogicalDevice::_AllocateNdasLocation()
{
	InstanceAutoLock autolock(this);
	//
	// We have different policy for single and RAID
	//
	if (m_NdasPort)
	{
		NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress = {0};
		if (NdasIsRaidDiskType(m_logicalDeviceGroup.Type)) 
		{
			//
			// (PathId, TargetId, Lun) = (1, x, y)
			// where x = 1 - 31 and y = 1 - 127
			//
			// lun = y * 15 + x;
			//
			DWORD lun = GetRaidSlotNo();
			logicalUnitAddress.PathId = static_cast<UCHAR>(m_logicalDeviceGroup.Type);
			logicalUnitAddress.TargetId = static_cast<UCHAR>(lun / 127);
			logicalUnitAddress.Lun = static_cast<UCHAR>(lun % 127 + 1);
		}
		else
		{
			CNdasUnitDevicePtr pFirstUnitDevice = GetUnitDevice(0);
			XTLASSERT(CNdasUnitDeviceNullPtr != pFirstUnitDevice);

			logicalUnitAddress.PathId = 0;
			logicalUnitAddress.TargetId = static_cast<UCHAR>(
				pFirstUnitDevice->GetParentDevice()->GetSlotNo());
			logicalUnitAddress.Lun = static_cast<UCHAR>(
				pFirstUnitDevice->GetUnitNo());
		}

		m_NdasLocation = NdasLogicalUnitAddressToLocation(logicalUnitAddress);
	}
	else
	{
		if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_SET_GROUP(m_logicalDeviceGroup.Type)) 
		{
			m_NdasLocation = GetRaidSlotNo();
		}
		else
		{
			CNdasUnitDevicePtr pFirstUnitDevice = GetUnitDevice(0);
			XTLASSERT(CNdasUnitDeviceNullPtr != pFirstUnitDevice);
			m_NdasLocation = 
				pFirstUnitDevice->GetParentDevice()->GetSlotNo() * 10 + 
				pFirstUnitDevice->GetUnitNo();
		}
	}
	XTLASSERT(m_NdasLocation != 0);
}

void
CNdasLogicalDevice::_DeallocateNdasLocation()
{
	InstanceAutoLock autolock(this);
	m_NdasLocation = 0;
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

	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK(m_logicalDeviceGroup.Type)) 
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
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:	
		blocks += pUnitDevice->GetUserBlockCount() * (GetUnitDeviceCountInRaid() - 1);
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		blocks = pUnitDevice->GetUserBlockCount();
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
		blocks = 0;
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
#if 0
	// NDASBUS will do unplug instead.
	BOOL fSuccess = Unplug();
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"%s: Failed to handle disconnect event, error=0x%X\n", 
			ToStringA(), GetLastError());
	}
#endif

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Set the disconnected flag
	m_fDisconnected = TRUE;
}


void
CNdasLogicalDevice::OnMounted(LPCTSTR DevicePath)
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Logical device %s is MOUNTED as %ls\n", 
		ToStringA(),
		DevicePath);

	DWORD dwTick = ::GetTickCount();
	m_dwMountTick = (dwTick == 0) ? 1: dwTick; // 0 is used for special purpose

	StringCchCopy(m_DevicePath, RTL_NUMBER_OF(m_DevicePath), DevicePath);

	SetLastMountAccess(m_MountedAccess);
	SetRiskyMountFlag(TRUE);
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
}

BOOL
CNdasLogicalDevice::ReconcileWithNdasPort()
{
	XTLASSERT(m_NdasPort);

	m_dwCurrentMRB = 0;

	CNdasServiceDeviceEventHandler& devEventHandler =
		pGetNdasDeviceEventHandler();

	HRESULT hr = devEventHandler.GetLogicalUnitDevicePath(
		m_NdasLocation,
		m_DevicePath,
		RTL_NUMBER_OF(m_DevicePath));

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit device path is unknown\n");
	}
	else
	{
		BOOL success;
		NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = 
			GetNdasLogicalUnitAddress();
		XTL::AutoFileHandle logicalUnitHandle = CreateFile(
			m_DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_DEVICE,
			NULL);

		if (logicalUnitHandle.IsInvalid())
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Opening device file failed, path=%ls, error=0x%X\n", 
				m_DevicePath, GetLastError());
		}
		else
		{
			//
			// Get the device mode
			//
			ULONG deviceMode;

			hr = NdasPortCtlQueryDeviceMode(
				logicalUnitHandle, 
				ndasLogicalUnitAddress, 
				&deviceMode, 
				NULL, 
				NULL,
				NULL);

			if (FAILED(hr))
			{
				XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
					"NdasPortCtlQueryDeviceMode failed, SlotNo=%08X, hr=0x%X\n", 
					m_NdasLocation, hr);
			}
			else
			{
				ACCESS_MASK	desiredAccess;

				//
				//	Translate the device mode to the access mask.
				//

				if (deviceMode == DEVMODE_SHARED_READONLY) 
				{
					desiredAccess = GENERIC_READ;
				}
				else if (
					deviceMode == DEVMODE_SHARED_READWRITE ||
					deviceMode == DEVMODE_EXCLUSIVE_READWRITE ||
					deviceMode == DEVMODE_SUPER_READWRITE) 
				{
					desiredAccess = GENERIC_READ|GENERIC_WRITE;
				}
				else 
				{
					desiredAccess = 0;
				}

				if (desiredAccess != 0) 
				{
					SetMountedAccess(desiredAccess);
				}
			}
		}

		//
		// Retrieve the LUR full information
		//
		PNDSCIOCTL_LURINFO pLurInfo = NULL;

		hr = NdasPortCtlQueryLurFullInformation(
			logicalUnitHandle,
			ndasLogicalUnitAddress,
			&pLurInfo);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlQueryLurFullInformation failed, SlotNo=%08X, hr=0x%X\n", 
				ndasLogicalUnitAddress, hr);

			_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

			return TRUE;
		}
		
		HeapFree(GetProcessHeap(), 0, pLurInfo);

		//
		// TODO: alternate m_dwCurrentMRB to the byte-unit variable.
		//

		m_dwCurrentMRB = 0;
		
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	return TRUE;
}

BOOL
CNdasLogicalDevice::ReconcileWithNdasBus()
{
	HANDLE hAlarm, hDisconnect;
	ULONG deviceMode;
	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = GetNdasLogicalUnitAddress();

	BOOL fSuccess = ::NdasBusCtlQueryPdoEvent(
	    m_NdasLocation, 
	    &hAlarm,
	    &hDisconnect);

	//
	// Reconciliation failure?
	//
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlQueryPdoEvent failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLocation, GetLastError());
	}
	else
	{
		m_hAlarmEvent.Release();
		m_hAlarmEvent = hAlarm;
		m_hDisconnectedEvent.Release();
		m_hDisconnectedEvent = hDisconnect;
	}

	//
	// Get the device mode
	//
	fSuccess = ::NdasBusCtlQueryDeviceMode(m_NdasLocation, &deviceMode);
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlQueryDeviceMode failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLocation, GetLastError());
	}
	else
	{
		ACCESS_MASK	desiredAccess;

		//
		//	Translate the device mode to the access mask.
		//

		if (deviceMode == DEVMODE_SHARED_READONLY) {
			desiredAccess = GENERIC_READ;
		} else if (
			deviceMode == DEVMODE_SHARED_READWRITE ||
			deviceMode == DEVMODE_EXCLUSIVE_READWRITE ||
			deviceMode == DEVMODE_SUPER_READWRITE) {

			desiredAccess = GENERIC_READ|GENERIC_WRITE;
		}  else {
			desiredAccess = 0;
		}

		if (desiredAccess != 0) {
			SetMountedAccess(desiredAccess);
		}
	}

	//
	// Retrieve the LUR full information
	//
	PNDSCIOCTL_LURINFO pLurInfo = NULL;
	fSuccess = ::NdasBusCtlQueryMiniportFullInformation(
		m_NdasLocation, &pLurInfo);
	if (fSuccess && pLurInfo)
	{
		HeapFree(GetProcessHeap(), 0, pLurInfo);
	}
	if (!fSuccess)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"QueryMiniportFullInformation failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLocation, GetLastError());

		_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

		return TRUE;
	}
	else 
	{
		//
		// TODO: alternate m_dwCurrentMRB to the byte-unit variable.
		//

		// NDASSCSI does not require MRB any more.
		// m_dwCurrentMRB = pLurInfo->Adapter.MaxDataTransferLength / 512;
		m_dwCurrentMRB = 0;
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

	UpdateBindStateAndError();
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

	if (m_NdasPort == FALSE) 
	{
		NDAS_LOCATION location = GetNdasLocation();
		XTLASSERT(0 != location);

		BOOL fAlive, fAdapterError;
		BOOL fSuccess;

		fSuccess = ::NdasBusCtlQueryNodeAlive(
		    location, 
		    &fAlive, 
		    &fAdapterError);
		//
		// if NdasBusCtlQueryNodeAlive fails, 
		// there may be no NDAS SCSI device instance...
		//

		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlQueryNodeAlive failed, SlotNo=%08X, error=0x%X\n", 
				m_NdasLocation, GetLastError());
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
		//	XTLTRACE_ERR("NdasBusCtlQueryNodeAlive reported an adapter error.\n"));
		//	pLogDevice->_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
		//}
    } 
	else 
	{
	    NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = 
			GetNdasLogicalUnitAddress();

	    HRESULT hr = NdasPortCtlQueryNodeAlive(ndasLogicalUnitAddress);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlQueryNodeAlive failed, LogicalUnitAddress=%08X, hr=0x%X\n", 
				m_NdasLocation, hr);

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Reset to Unmounted\n");

			OnUnmounted();
			
			return;
		}
    }

}

BOOL
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
		return TRUE;
	}
	return FALSE;
}

BOOL
CNdasLogicalDevice::_IsRaid()
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
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:
		return NDASSCSI_TYPE_DISK_RAID5;
	case NDAS_LOGICALDEVICE_TYPE_DVD:
		return NDASSCSI_TYPE_DVD;
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
		return NDASSCSI_TYPE_VDVD;
	case NDAS_LOGICALDEVICE_TYPE_MO:
		return NDASSCSI_TYPE_MO;
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
		return NDASSCSI_TYPE_DISK_NORMAL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
		return NDASSCSI_TYPE_DISK_NORMAL;
	default:
		XTLASSERT(FALSE);
		return NDASSCSI_TYPE_DISK_NORMAL;
	}
}

LURN_TYPE
LogicalDeviceTypeToLurTargetType(
	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType)
{
	switch (LogicalDeviceType) 
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		return LURN_IDE_DISK;
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		return LURN_MIRRORING;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		return LURN_AGGREGATION;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		return LURN_RAID0;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
		XTLASSERT(FALSE);
		return LURN_NULL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
		XTLASSERT(FALSE);
		return LURN_NULL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
		return LURN_RAID1R;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		XTLASSERT(FALSE);
		return LURN_NULL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
		XTLASSERT(FALSE);
		return LURN_NULL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
		return LURN_RAID4R;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:
		return LURN_RAID5;
	case NDAS_LOGICALDEVICE_TYPE_DVD:
		return LURN_IDE_ODD;
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
		XTLASSERT(FALSE);
		return LURN_NULL;
	case NDAS_LOGICALDEVICE_TYPE_MO:
		return LURN_IDE_MO;
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
		XTLASSERT(FALSE);
		return LURN_NULL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
		XTLASSERT(FALSE);
		return LURN_NULL;
	default:
		XTLASSERT(FALSE);
		return LURN_NULL;
	}
}

}
// anonymous namespace
