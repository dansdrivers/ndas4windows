/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <lfsfiltctl.h>
#include <ndasbusctl.h>
#include <xixfsctl.h>
#include <ndas/ndasportctl.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasop.h>
#include <ndas/ndasvolex.h>
#include <ndas/ndasportctl.h>
#include "ndasdevid.h"
#include "ndasdevreg.h"
#include "ndaslogdev.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "lpxcomm.h"
#include "ndascfg.h"
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

VOID
LogicalDeviceTypeToRootLurnType(
	__in NDAS_LOGICALDEVICE_TYPE LogicalDeviceType,
	__out PLURN_TYPE	LurnType,
	__out PUCHAR 		LurnDeviceInterface);

VOID
UnitDeviceTypeToLeafLurnType(
	__in NDAS_UNIT_TYPE UnitDeviceType,
	__out PLURN_TYPE	LurnType,
	__out PUCHAR 		LurnDeviceInterface);


} // namespace

//
// Constructor for a multiple member logical device
//
CNdasLogicalUnit::CNdasLogicalUnit() :
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
	m_NdasLocation(0),
	m_fShutdown(FALSE),
	m_ulAdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT),
	m_RaidFailReason(NDAS_RAID_FAIL_REASON_NONE),
	m_fMountable(FALSE),
	m_fDegradedMode(FALSE)
{
}

HRESULT 
CNdasLogicalUnit::Initialize(
	__in NDAS_LOGICALDEVICE_ID LogicalUnitId, 
	const NDAS_LOGICALDEVICE_GROUP& LogicalUnitDef)
{
	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		__FUNCTION__ " %d\n", LogicalUnitId);

	m_NdasLogicalUnitId = LogicalUnitId;
	m_NdasLogicalUnitDefinition = LogicalUnitDef;

	// Locate Registry Container based on its hash value
	pLocateRegContainer();

	if (m_hDisconnectedEvent.IsInvalid())
	{
		m_hDisconnectedEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hDisconnectedEvent.IsInvalid()) 
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"CreateEvent(Disconnect) failed, hr=0x%X\n", hr);
			return hr;
		}
	}

	if (m_hAlarmEvent.IsInvalid()) 
	{
		m_hAlarmEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hAlarmEvent.IsInvalid()) 
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"CreateEvent(Alarm) failed, hr=0x%X\n", hr);
			XTLVERIFY(CloseHandle(m_hDisconnectedEvent));
			return hr;
		}
	}

	ACCESS_MASK lastMountAccess = pGetLastMountAccess();

	BOOL fRiskyMountFlag = pGetRiskyMountFlag();

	if (fRiskyMountFlag) 
	{
		m_fRiskyMount = fRiskyMountFlag;
	}

	if ((lastMountAccess > 0) && !pIsRiskyMount()) 
	{
		SetMountOnReady(lastMountAccess, FALSE);
	}

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Logical Device %d initialized successfully.\n", m_NdasLogicalUnitId);

	return S_OK;
}

void 
CNdasLogicalUnit::FinalRelease()
{
	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		__FUNCTION__ " %d\n", m_NdasLogicalUnitId);
}

STDMETHODIMP 
CNdasLogicalUnit::get_Id(
	__out NDAS_LOGICALDEVICE_ID* Id)
{
	*Id = m_NdasLogicalUnitId;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_Type(__out NDAS_LOGICALDEVICE_TYPE* Type)
{
	*Type = m_NdasLogicalUnitDefinition.Type;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_Status(__out NDAS_LOGICALDEVICE_STATUS * Status)
{
	CAutoLock autolock(this);
	*Status = m_status;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_Error(__out NDAS_LOGICALDEVICE_ERROR * Error)
{
	CAutoLock autolock(this);
	*Error = m_lastError;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_LogicalUnitDefinition(
	__out NDAS_LOGICALDEVICE_GROUP* LogicalUnitDefinition)
{
	*LogicalUnitDefinition = m_NdasLogicalUnitDefinition;
	return S_OK;
}

struct InvalidateNdasUnit : std::unary_function<INdasUnit*, void>
{
	void operator()(INdasUnit* pNdasUnit) const
	{
		// Skip the null pointer
		if (!pNdasUnit) return;

		CComPtr<INdasDevice> pNdasDevice;
		COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));

		pNdasDevice->InvalidateNdasUnit(pNdasUnit);
	}
};

void
CNdasLogicalUnit::pInvalidateNdasUnits()
{
	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);

	AtlForEach(ndasUnits, InvalidateNdasUnit());
}

STDMETHODIMP 
CNdasLogicalUnit::get_DevicePath(__deref_out BSTR* DevicePath)
{
	CAutoLock autolock(this);
	return m_DevicePath.CopyTo(DevicePath);
}

STDMETHODIMP 
CNdasLogicalUnit::get_DisconnectEvent(__out HANDLE* EventHandle)
{
	CAutoLock autolock(this);
	*EventHandle = m_hDisconnectedEvent;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_AlarmEvent(__out HANDLE* EventHandle)
{
	CAutoLock autolock(this);
	*EventHandle = m_hAlarmEvent;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_NdasUnitCount(__out DWORD* UnitCount)
{
	*UnitCount = m_NdasLogicalUnitDefinition.nUnitDevices;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_NdasUnitId(__in DWORD Sequence, __out NDAS_UNITDEVICE_ID* NdasUnitId)
{
	if (Sequence >= m_NdasLogicalUnitDefinition.nUnitDevices)
	{
		return E_INVALIDARG;
	}

	*NdasUnitId = m_NdasLogicalUnitDefinition.UnitDevices[Sequence];
	return S_OK;
}


DWORD 
CNdasLogicalUnit::pGetUnitDeviceCount() const
{
	return m_NdasLogicalUnitDefinition.nUnitDevices;
}

DWORD 
CNdasLogicalUnit::pGetUnitDeviceCountSpare() const
{
	return m_NdasLogicalUnitDefinition.nUnitDevicesSpare;
}

DWORD 
CNdasLogicalUnit::pGetUnitDeviceCountInRaid() const
{
	return pGetUnitDeviceCount() - pGetUnitDeviceCountSpare();
}

const NDAS_LOGICALDEVICE_GROUP&
CNdasLogicalUnit::pGetLogicalUnitConfig() const
{
	return m_NdasLogicalUnitDefinition;
}

STDMETHODIMP 
CNdasLogicalUnit::get_AdapterStatus(__out ULONG * AdapterStatus)
{
	CAutoLock autolock(this);
	*AdapterStatus = m_ulAdapterStatus;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::put_AdapterStatus(__out ULONG AdapterStatus)
{
	CAutoLock autolock(this);
	m_ulAdapterStatus = AdapterStatus;
	return S_OK;
}

BOOL
CNdasLogicalUnit::pUpdateBindStateAndError()
{
	NDAS_RAID_FAIL_REASON PrevFailReason = m_RaidFailReason;
	BOOL PrevMountable = m_fMountable;
	BOOL PrevDegradedMode = m_fDegradedMode;
	NDAS_LOGICALDEVICE_ERROR PrevLastError = m_lastError;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
		"** Begin updating LogicalUnit=%d\n", m_NdasLogicalUnitId);

	// Update m_fMountable and m_fDegradedMode flags.
	pRefreshBindStatus();

	if (pIsMountable())
	{
		if (m_fDegradedMode) 
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE);
		}
		else 
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}
	}
	else
	{
		enum {
			RESOLUTION_REQUIRED_FLAGS = 
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
				NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT,

			MISSING_MEMBER_FLAGS = 
				NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE |
				NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED |
				NDAS_RAID_FAIL_REASON_MEMBER_DISABLED,

			UPGRADE_REQUIRED_FLAGS = 
				NDAS_RAID_FAIL_REASON_MIGRATION_REQUIRED,
		};

		// Handle serious case first
		if (m_RaidFailReason & RESOLUTION_REQUIRED_FLAGS)
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
		} 
		else if (m_RaidFailReason & MISSING_MEMBER_FLAGS)
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		} 
		else if (m_RaidFailReason & UPGRADE_REQUIRED_FLAGS)
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_UPGRADE);
		} 
		else 
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
		}		
	}	

	CComPtr<INdasUnit> pPrimaryNdasUnit;
	HRESULT hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);
	if (SUCCEEDED(hr))
	{
		BOOL raid = FALSE;

		if (IsNdasPortMode()) 
		{
			LURN_TYPE targetType;
			LogicalDeviceTypeToRootLurnType(m_NdasLogicalUnitDefinition.Type, &targetType, NULL);

			if (pPrimaryNdasUnit && 
				LURN_RAID1R == targetType ||
				LURN_RAID4R == targetType ||
				LURN_RAID5 == targetType) 
			{
				raid = TRUE;
			}
		} 
		else 
		{
			UCHAR targetType = 
				LogicalDeviceTypeToNdasBusTargetType(m_NdasLogicalUnitDefinition.Type);
			if (pPrimaryNdasUnit && 
				NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID5 == targetType) 
			{
				raid = TRUE;
			}
		}

		if (raid) 
		{
			CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pPrimaryNdasUnit);
			ATLASSERT(pNdasDiskUnit.p);

			PVOID raidInfoBuffer = NULL;
			COMVERIFY(pNdasDiskUnit->get_RaidInfo(&raidInfoBuffer));

			if (pNdasDiskUnit && raidInfoBuffer)
			{
				PNDAS_RAID_INFO raidInfo = static_cast<PNDAS_RAID_INFO>(raidInfoBuffer);
				//
				// RAID config can be changed by kernel.
				// Check it here and invalidate devices if it is changed.
				//
				if (raidInfo->ConfigSetId != m_ConfigSetId) 
				{
					XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
						"Logical Device config set change detected.\n");			

					XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
						"** End updating LogicalUnit=%d\n", m_NdasLogicalUnitId);
					return TRUE;
				}
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
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
			"** End updating LogicalUnit=%d\n", m_NdasLogicalUnitId);
		return TRUE;
	} 
	else 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
			"** End updating LogicalUnit=%d\n", m_NdasLogicalUnitId);
		return FALSE;
	}
}

BOOL 
CNdasLogicalUnit::pRefreshBindStatus() 
{
	HRESULT hr;

	if (!pIsRaid()) 
	{
		DWORD instances;
		COMVERIFY(get_NdasUnitInstanceCount(&instances));
		if (0 == instances) 
		{
			m_fMountable = FALSE;
		} 
		else 
		{
			m_fMountable = TRUE;
		}
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_NONE;
		m_fDegradedMode = FALSE;
		return TRUE;
	}

	DWORD instances;
	COMVERIFY(get_NdasUnitInstanceCount(&instances));
	if (0 == instances) 
	{
		// No unit device.
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;		
		return TRUE;
	}
	// Connect to primary device

	CComPtr<INdasUnit> pNdasUnit;
	hr = pGetPrimaryNdasUnit(&pNdasUnit);
	if (FAILED(hr)) 
	{
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;
		return TRUE;
	}

	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	pNdasUnit->get_UnitNo(&ci.UnitNo);
	ci.WriteAccess = FALSE;
	pNdasUnit->get_NdasDevicePassword(&ci.OEMCode.UI64Value);
	ci.PrivilegedOEMCode.UI64Value = 0;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	NDAS_UNITDEVICE_ID ndasUnitId;
	pNdasUnit->get_NdasUnitId(&ndasUnitId);
	ci.Address.DeviceId = ndasUnitId.DeviceId;

	HNDAS hNDAS = NdasCommConnect(&ci);

	if (hNDAS == NULL) 
	{
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;

		return TRUE;
	}
	
	NDASOP_RAID_INFO ndasRaidInfo = {0};
	ndasRaidInfo.Size = sizeof(ndasRaidInfo); // To check version.
	BOOL success = NdasOpGetRaidInfo(hNDAS, &ndasRaidInfo);
	if (!success) 
	{
		//
		// NdasOpGetRaidInfo returns FALSE only for internal error 
		// such as version mismatch, memory allocation failure.
		//
		XTLASSERT(FALSE);
		m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_OFFLINE;
		m_fMountable = FALSE;
		m_fDegradedMode = FALSE;	

		XTLVERIFY( NdasCommDisconnect(hNDAS) );
		return TRUE;
	}

	m_ConfigSetId = ndasRaidInfo.ConfigSetId;
		
	if (ndasRaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_UNMOUNTABLE)
	{
		if (ndasRaidInfo.FailReason & NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION)
		{
			// DIB version is higher than this version can handle.
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_UNSUPPORTED_DIB_VERSION;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	

			XTLVERIFY( NdasCommDisconnect(hNDAS) );
			return TRUE;
		}
		else if (ndasRaidInfo.FailReason & NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT)
		{
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	

			XTLVERIFY( NdasCommDisconnect(hNDAS) );
			return TRUE;
		}
	}

	//
	// Check all device is registered and not disabled.
	// 

	for (DWORD i = 0; i < ndasRaidInfo.MemberCount; i++) 
	{
		CComPtr<INdasDevice> pNdasDevice;
		hr = pGetNdasDevice(ndasRaidInfo.Members[i].DeviceId, &pNdasDevice);
		if (FAILED(hr)) 
		{
			// Member is not registered
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_NOT_REGISTERED;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;				

			XTLVERIFY( NdasCommDisconnect(hNDAS) );
			return TRUE;
		}

		NDAS_DEVICE_STATUS status;
		COMVERIFY(pNdasDevice->get_Status(&status));

		if (NDAS_DEVICE_STATUS_DISABLED == status) 
		{
			m_RaidFailReason = NDAS_RAID_FAIL_REASON_MEMBER_DISABLED;
			m_fMountable = FALSE;
			m_fDegradedMode = FALSE;	

			XTLVERIFY( NdasCommDisconnect(hNDAS) );
			return TRUE;
		}
	}

	m_RaidFailReason = (NDAS_RAID_FAIL_REASON) ndasRaidInfo.FailReason;

	m_fMountable  = (ndasRaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_MOUNTABLE)?TRUE:FALSE;
	m_fDegradedMode = (ndasRaidInfo.MountablityFlags & NDAS_RAID_MOUNTABILITY_DEGRADED)?TRUE:FALSE;

	// To do: check online member is also recognized as online by svc, too

	XTLVERIFY( NdasCommDisconnect(hNDAS) );

	return TRUE;
}

//
// Set the unit device ID at a sequence 
// to a unit device member ID list
//
STDMETHODIMP
CNdasLogicalUnit::AddNdasUnitInstance(INdasUnit* pNdasUnit)
{
	CAutoLock autolock(this);

	HRESULT hr;
	BOOL success;

	NDAS_RAID_MOUNTABILITY_FLAGS RaidMountablity;
	NDAS_RAID_FAIL_REASON RaidFailReason;

	DWORD luseq;
	COMVERIFY(pNdasUnit->get_LogicalUnitSequence(&luseq));

	XTLASSERT(luseq < m_NdasLogicalUnitDefinition.nUnitDevices);
	if (luseq >= m_NdasLogicalUnitDefinition.nUnitDevices) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Invalid unit device sequence, seq=%d, gcount=%d\n", 
			luseq, m_NdasLogicalUnitDefinition.nUnitDevices);
		return E_INVALIDARG;
	}

	//
	// Check for multiple add unit device calls
	//
	{
		CInterfaceArray<INdasUnit> ndasUnits;
		pGetNdasUnitInstances(ndasUnits);

		size_t ndasUnitCount = ndasUnits.GetCount();
		for (size_t i = 0; i < ndasUnitCount; ++i)
		{
			INdasUnit* p = ndasUnits.GetAt(i);
			if (p == pNdasUnit)
			{
				XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
					"Duplicate unit device sequence, seq=%d\n", luseq); 
				XTLASSERT(FALSE && "Duplicate calls to AddUnitDevice");
				return E_FAIL;
			}
		}
	}

	m_NdasUnits.Add(pNdasUnit);

	// TODO: CHANGE THIS LOGIC

	// pUpdateBindStateAndError();
	m_RaidFailReason = NDAS_RAID_FAIL_REASON_NONE;
	m_fMountable = TRUE;
	m_fDegradedMode = FALSE;	

	BOOL PrevMountable = m_fMountable;
	BOOL PrevDegradedMode = m_fDegradedMode;
	NDAS_LOGICALDEVICE_ERROR PrevLastError = m_lastError;

	if (!pIsMountable())	 
	{
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == m_status) 
		{
			// If this device is already mounted, we should not set unmounted status.
		}
		else 
		{
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);
			pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			return S_OK;
		}
	}
			
	//
	// Disk is mountable but slot is not allocated yet.
	//
	if (pIsMountable() && 0 == m_NdasLocation) 
	{
		CComPtr<INdasLogicalUnitManagerInternal> pManager;
		COMVERIFY(hr = pGetNdasLogicalUnitManagerInternal(&pManager));

		// Allocate NdasLocation when mountable.
		pAllocateNdasLocation();
		hr = pManager->RegisterNdasLocation(m_NdasLocation, this);
		if (FAILED(hr)) 
		{
			_ASSERT(FALSE); // this should not happen anyway..
			//
			// This NdasScisiLocation is already used. User need to resolve this situation using bindtool
			//
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);
			pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			return S_OK;
		}
	}

	if (pIsMountable()) 
	{
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);
		
		//
		// Reconciliation Support
		//

		BOOL activeLogicalUnit = FALSE;

		if (IsNdasPortMode()) 
		{
			NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress = 
				NdasLocationToLogicalUnitAddress(m_NdasLocation);

			hr = NdasPortCtlQueryNodeAlive(logicalUnitAddress);

			if (SUCCEEDED(hr))
			{
				activeLogicalUnit = TRUE;
				pReconcileWithNdasPort();
			}
		} 
		else 
		{
			BOOL bAlive, bAdapterError;
			success = ::NdasBusCtlQueryNodeAlive(
				m_NdasLocation, 
				&bAlive, 
				&bAdapterError);

			if (success && bAlive)
			{
				activeLogicalUnit = TRUE;
				pReconcileWithNdasBus();
			}
		}

		if (!activeLogicalUnit && m_fMountOnReady) 
		{
			if (pIsComplete() && !m_fDegradedMode) 
			{
				ACCESS_MASK allowedAccess;
				COMVERIFY(get_AllowedAccess(&allowedAccess));

				//
				// Try to mount only in not degraded mode.
				// (RAID can be in degraded mode even if the RAID member is complete, in case that disk has same DIB, different RAID set ID)
				//
				// If the NDAS Bus do not have the pdo,
				// the NDAS service will mount the device
				//
				if ((m_mountOnReadyAccess & allowedAccess) == m_mountOnReadyAccess)
				{
					DWORD LdpfFlags = 0, LdpfValues = 0;
					if (!pGetLastLogicalUnitPlugInFlags(LdpfFlags, LdpfValues))
					{
						LdpfFlags = LdpfValues = 0;
					}

					hr = PlugIn(m_mountOnReadyAccess, LdpfFlags, LdpfValues);

					if (FAILED(hr))
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
							"Boot-time mount (%d) failed, hr=0x%X\n", 
							m_NdasLogicalUnitId, hr);
					}
					else
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
							"Boot-time mount (%d) succeeded.\n", m_NdasLogicalUnitId);
					}

				} 
				else if ((GENERIC_READ & allowedAccess) && m_fReducedMountOnReadyAccess) 
				{
					// When RW access is not available, we will mount the device as RO.
					// if ReducedMountOnReadyAccess is true.
					DWORD LdpfFlags = 0, LdpfValues = 0;
					if (!pGetLastLogicalUnitPlugInFlags(LdpfFlags, LdpfValues))
					{
						LdpfFlags = LdpfValues = 0;
					}

					hr = PlugIn(GENERIC_READ, LdpfFlags, LdpfValues);

					if (FAILED(hr))
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
							"RO Boot-time mount (%d) failed, hr=0x%X\n", 
							m_NdasLogicalUnitId, hr);
					}
					else
					{
						XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
							"RO Boot-time mount (%d) succeeded.\n", 
							m_NdasLogicalUnitId);
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
		"Added Sequence %d to Logical Unit %d\n", 
		luseq, 
		m_NdasLogicalUnitId);

	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);

	return S_OK;
}

//
// Remove the unit device ID from the list
//
STDMETHODIMP
CNdasLogicalUnit::RemoveNdasUnitInstance(INdasUnit* pNdasUnit)
{
	CAutoLock autolock(this);

	HRESULT hr;
	DWORD luseq;	
	COMVERIFY(pNdasUnit->get_LogicalUnitSequence(&luseq));

	NDAS_RAID_MOUNTABILITY_FLAGS RaidMoutablity;
	NDAS_RAID_FAIL_REASON RaidFailReason;
	DWORD PrevMountablity = m_fMountable;

	XTLASSERT(luseq < m_NdasLogicalUnitDefinition.nUnitDevices);
	if (luseq >= m_NdasLogicalUnitDefinition.nUnitDevices) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Invalid sequence (%d) of the unit device.\n", luseq);
		return E_INVALIDARG;
	}

	size_t index = 0;
	size_t ndasUnitCount = m_NdasUnits.GetCount();
	for (; index < ndasUnitCount; ++index)
	{
		INdasUnit* p = m_NdasUnits.GetAt(index);
		if (p == pNdasUnit)
		{
			break;
		}
	}
	if (index >= ndasUnitCount)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Unit device in sequence (%d) is not occupied.\n", luseq);
		XTLASSERT(FALSE && "RemoveUnitDevice called for non-registered");
		return E_FAIL;
	}

	m_NdasUnits.RemoveAt(index);

	// TODO: CHANGE THIS LOGIC
	// pUpdateBindStateAndError();

	//
	// Remove m_NdasLocation if this logical device is not mounted and unmountable anymore
	//
	NDAS_LOGICALDEVICE_STATUS status = m_status;
	if (0 != m_NdasLocation && !pIsMountable() && 
		(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != status &&
		NDAS_LOGICALDEVICE_STATUS_MOUNTED != status ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING != status))
	{
		// If the previous status was complete and now is not. unregister ndasscsi location
		CComPtr<INdasLogicalUnitManagerInternal> pManager;
		COMVERIFY(hr = pGetNdasLogicalUnitManagerInternal(&pManager));
		pManager->UnregisterNdasLocation(m_NdasLocation, this);
		pDeallocateNdasLocation();
	}

	//
	// Set Device Error
	//
	if (pIsMountable()) 
	{
		//
		// If conflict source is removed , RAID can be mounted when device is removed.
		// 
		if (0 == m_NdasLocation) 
		{
			CComPtr<INdasLogicalUnitManagerInternal> pManager;
			COMVERIFY(hr = pGetNdasLogicalUnitManagerInternal(&pManager));
			// Allocate NdasLocation when mountable.
			pAllocateNdasLocation();
			hr = pManager->RegisterNdasLocation(m_NdasLocation, this);
			if (FAILED(hr)) 
			{
				_ASSERT(FALSE);
				//
				// This NdasScisiLocation is already used. User need to resolve this situation using bindtool
				//
				pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
				CNdasEventPublisher& epub = pGetNdasEventPublisher();
				epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);
				pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			}
		}	
	}

	//
	// Publish Event
	//
	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);

	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_NdasUnitInstanceCount(__out DWORD* UnitCount)
{
	CAutoLock autolock(this);
	DWORD instances = static_cast<DWORD>(m_NdasUnits.GetCount());
	*UnitCount = instances;
	return S_OK;
}

BOOL
CNdasLogicalUnit::pIsComplete()
{
	bool complete = (m_NdasLogicalUnitDefinition.nUnitDevices == m_NdasUnits.GetCount());
	return complete;
}

BOOL
CNdasLogicalUnit::pIsMountable()
{
	return m_fMountable;
}

struct NotifyNdasUnitMountCompleted : std::unary_function<INdasUnit*, void>
{
	void operator()(INdasUnit* pNdasUnit)
	{
		CComQIPtr<INdasUnitPnpSink> pSink(pNdasUnit);
		ATLASSERT(pSink.p);
		pSink->MountCompleted();
	};
};

struct NotifyNdasUnitDismountCompleted : std::unary_function<INdasUnit*, void>
{
	void operator()(INdasUnit* pNdasUnit)
	{
		CComQIPtr<INdasUnitPnpSink> pSink(pNdasUnit);
		ATLASSERT(pSink.p);
		pSink->DismountCompleted();
	};
};

void 
CNdasLogicalUnit::pSetStatus(NDAS_LOGICALDEVICE_STATUS newStatus)
{
	CAutoLock autolock(this);

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
	if ((NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == oldStatus && 
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == newStatus) ||
		(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == oldStatus && 
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == newStatus))
	{
		// Attach to the event monitor
		CNdasEventMonitor& emon = pGetNdasEventMonitor();
		emon.Attach(this);
	}
	else if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == newStatus) 
	{
		if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == oldStatus) {
			// Nothing to do. Already detached.
		} else if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED != oldStatus) 
		{
			// Detach from the event monitor
			CNdasEventMonitor& emon = pGetNdasEventMonitor();
			emon.Detach(this);
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

	if ((NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == oldStatus && 
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == newStatus) ||
		(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == oldStatus && 
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == newStatus) ||
		(NDAS_LOGICALDEVICE_STATUS_MOUNTED == oldStatus && 
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == newStatus))
	{
		// MOUNTED
		AtlForEach(m_NdasUnits, NotifyNdasUnitMountCompleted());
	}
	else if ((NDAS_LOGICALDEVICE_STATUS_MOUNTED == oldStatus && NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == newStatus) ||
		(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == oldStatus && NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == newStatus))
	{
		// UNMOUNTED
		AtlForEach(m_NdasUnits, NotifyNdasUnitDismountCompleted());
	}
	
	// publish a status change event
	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceStatusChanged(m_NdasLogicalUnitId, oldStatus, newStatus);

	//
	// Temp fault tolerant RAID work-around.
	// Remove NdasLocation  that we couldn't remove because it was mounted.
	//
	if (0 != m_NdasLocation && 
		m_fMountable== FALSE && 
		!(NDAS_LOGICALDEVICE_STATUS_MOUNTED == newStatus || 
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == newStatus || 
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING== newStatus)) 
	{
		HRESULT hr;
		CComPtr<INdasLogicalUnitManagerInternal> pManager;
		COMVERIFY(hr = pGetNdasLogicalUnitManagerInternal(&pManager));
		pManager->UnregisterNdasLocation(m_NdasLocation, this);
		pDeallocateNdasLocation();
	}
}

void 
CNdasLogicalUnit::pSetLastDeviceError(
	NDAS_LOGICALDEVICE_ERROR logDevError)
{
	CAutoLock autolock(this);
	m_lastError = logDevError;
}


STDMETHODIMP
CNdasLogicalUnit::get_MountedAccess(__out ACCESS_MASK * Access)
{
	CAutoLock autolock(this);
	*Access = m_MountedAccess;
	return S_OK;
}

void 
CNdasLogicalUnit::pSetMountedAccess(ACCESS_MASK mountedAccess)
{ 
	CAutoLock autolock(this);
	m_MountedAccess = mountedAccess; 
}

HRESULT 
CNdasLogicalUnit::pGetMemberNdasUnit(DWORD Seq, INdasUnit** ppNdasUnit)
{
	*ppNdasUnit = 0;

	XTLASSERT(Seq < m_NdasLogicalUnitDefinition.nUnitDevices);
	if (Seq >= m_NdasLogicalUnitDefinition.nUnitDevices)
	{
		return E_INVALIDARG;
	}

	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pGetNdasUnit(m_NdasLogicalUnitDefinition.UnitDevices[Seq], &pNdasUnit);
	if (FAILED(hr))
	{
		return hr;
	}

	*ppNdasUnit = pNdasUnit.Detach();

	return S_OK;
}

HRESULT 
CNdasLogicalUnit::pGetPrimaryNdasUnit(INdasUnit** ppNdasUnit)
{
	CAutoLock autolock(this);

	*ppNdasUnit = 0;

	// Find primary unit to use
	DWORD devCount = pGetUnitDeviceCount();
	for (DWORD i = 0; i < devCount; i++) 
	{
		CComPtr<INdasUnit> pNdasUnit;
		if (FAILED(pGetMemberNdasUnit(i, &pNdasUnit)))
		{
			continue;
		}

		NDAS_LOGICALDEVICE_GROUP ludef;
		COMVERIFY(pNdasUnit->get_LogicalUnitDefinition(&ludef));

		if (0 != memcmp(&ludef, &m_NdasLogicalUnitDefinition, sizeof(ludef)))
		{
			// This unit is not configured as RAID member.
			continue;
		}

		size_t count = m_NdasUnits.GetCount();
		for (size_t i = 0; i < count; ++i)
		{
			INdasUnit* p = m_NdasUnits.GetAt(i);
			if (p == pNdasUnit)
			{
				*ppNdasUnit = pNdasUnit.Detach();
				return S_OK;
			}
		}

		//
		// not found
		// This unit is offline or not in this logical device.
		//
	}
	return E_FAIL;
}

struct SumGrantedAccess : public std::unary_function<INdasUnit*,void>
{
	ACCESS_MASK AccessMask;
	SumGrantedAccess() : AccessMask(0xFFFFFFFFL) {}
	result_type operator()(INdasUnit* pNdasUnit)
	{
		ACCESS_MASK access = 0;
		COMVERIFY(pNdasUnit->get_GrantedAccess(&access));
		AccessMask &= access;
	}
};

struct SumAllowedAccess : public std::unary_function<INdasUnit*,void>
{
	ACCESS_MASK AccessMask;
	SumAllowedAccess() : AccessMask(0xFFFFFFFFL) {}
	result_type operator()(INdasUnit* pNdasUnit)
	{
		ACCESS_MASK access = 0;
		COMVERIFY(pNdasUnit->get_AllowedAccess(&access));
		AccessMask &= access;
	}
};

STDMETHODIMP
CNdasLogicalUnit::get_GrantedAccess(__out ACCESS_MASK * GrantedAccess)
{
	*GrantedAccess = 0;

	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);

	if (ndasUnits.IsEmpty())
	{
		return S_FALSE;
	}

	SumGrantedAccess sum;
	AtlForEach(ndasUnits, sum);

	*GrantedAccess = sum.AccessMask;

	return S_OK;
}

STDMETHODIMP
CNdasLogicalUnit::get_AllowedAccess(__out ACCESS_MASK * AllowedAccess)
{
	*AllowedAccess = 0;

	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);

	if (ndasUnits.IsEmpty())
	{
		return S_FALSE;
	}

	SumAllowedAccess sum;
	AtlForEach(ndasUnits, sum);

	*AllowedAccess = sum.AccessMask;

	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_MountedDriveSet(__out DWORD * LogicalDrives)
{
	CAutoLock autolock(this);
	*LogicalDrives = m_dwMountedDriveSet;
	return S_OK;
}

void 
CNdasLogicalUnit::OnMountedDriveSetChanged(DWORD Remove, DWORD Add)
{ 
	CAutoLock autolock(this);
	//m_dwMountedDriveSet = 
	//	(m_dwMountedDriveSet & ~DriveSetMask) |
	//	(DriveSet & DriveSetMask);
	DWORD drives = (m_dwMountedDriveSet & ~Remove) | Add;
	if (drives != m_dwMountedDriveSet)
	{
		m_dwMountedDriveSet = drives;
		autolock.Release();
		pGetNdasEventPublisher().LogicalDevicePropertyChanged(m_NdasLogicalUnitId);
	}
}

BOOL
CNdasLogicalUnit::pIsRiskyMount()
{
	CAutoLock autolock(this);
	return m_fRiskyMount;
}

BOOL
CNdasLogicalUnit::pGetRiskyMountFlag()
{
	CAutoLock autolock(this);

	BOOL fRisky = FALSE;
	BOOL success = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("RiskyMountFlag"),
		&fRisky);

	return fRisky;
}

DWORD
CNdasLogicalUnit::pGetRaidSlotNo()
{
	CAutoLock autolock(this);

	if (IsNdasPortMode())
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

STDMETHODIMP 
CNdasLogicalUnit::SetRiskyMountFlag(__in BOOL RiskyState)
{
	CAutoLock autolock(this);

	BOOL success = _NdasSystemCfg.SetValueEx(
		m_szRegContainer,
		_T("RiskyMountFlag"),
		RiskyState);

	m_fRiskyMount = RiskyState;

	return S_OK;
}

void
CNdasLogicalUnit::pSetLastMountAccess(ACCESS_MASK mountedAccess)
{
	CAutoLock autolock(this);

	BOOL success = _NdasSystemCfg.SetValueEx(
		m_szRegContainer, 
		_T("MountMask"), 
		(DWORD) mountedAccess);
}

ACCESS_MASK
CNdasLogicalUnit::pGetLastMountAccess()
{
	CAutoLock autolock(this);

	ACCESS_MASK mountMask = 0;

	BOOL success = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("MountMask"),
		(LPDWORD)&mountMask);

	if (!success) 
	{
		return 0;
	}

	return mountMask;
}

BOOL
CNdasLogicalUnit::pIsPSWriteShareCapable()
{
	BOOL fNoPSWriteShare = NdasServiceConfig::Get(nscDontUseWriteShare);

	// logical device specific option
	BOOL success = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("NoPSWriteShare"),
		&fNoPSWriteShare);
	if (success || fNoPSWriteShare)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"NoPSWriteShare is set at logical unit %d.\n", m_NdasLogicalUnitId);
		return FALSE;
	}

	// even though NoPSWriteShare is not set, if there is no active
	// LFS filter or Xixfs, then PSWriteShare is denied.
	WORD wNDFSMajor, wNDFSMinor;
	success = ::LfsFiltCtlGetVersion(
		NULL, NULL, NULL, NULL, 
		&wNDFSMajor, &wNDFSMinor);
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LFSFilter does not exist.\n");

		HRESULT hr = XixfsCtlGetVersion(
			NULL, NULL, NULL, NULL,
			&wNDFSMajor, &wNDFSMinor);

		if (FAILED(hr))
		{
			// LFS nor Xixfs exists or it is not working NoPSWriteShare
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
				"Xixfs does not exist. NoPSWriteShare.\n");
			return FALSE;

		}

	}

	if (NdasServiceConfig::Get(nscDisableRAIDWriteShare))
	{
		if (NDAS_LOGICALDEVICE_TYPE_DISK_RAID1 == m_NdasLogicalUnitDefinition.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2 == m_NdasLogicalUnitDefinition.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID4 == m_NdasLogicalUnitDefinition.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2 == m_NdasLogicalUnitDefinition.Type)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION, 
				"WriteShare is disabled for RAID1, RAID1_R2, RAID4, RAID4_R2.\n");

			return FALSE;
		}
	}

	return TRUE;
}

HRESULT
CNdasLogicalUnit::pIsWriteAccessAllowed(
	BOOL fPSWriteShare,
	INdasUnit* pNdasUnit)
{
	if (NULL == pNdasUnit)
	{
		return E_FAIL;
	}

	DWORD nROHosts, nRWHosts;
	HRESULT hr = pNdasUnit->GetHostUsageCount(&nROHosts, &nRWHosts, TRUE);
	if (FAILED(hr)) 
	{
		return hr;
	}

	if (nRWHosts > 0)
	{
		if (!fPSWriteShare)
		{
			return NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED;
		}
		else
		{
			HRESULT hr = pNdasUnit->CheckNDFSCompatibility();
			if (FAILED(hr))
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
CNdasLogicalUnit::pPlugInNdasBus(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	CAutoLock autolock(this);
	HRESULT hr;

	BOOL fPSWriteShare = pIsPSWriteShareCapable();

	hr = pIsSafeToPlugIn(requestingAccess);
	if (FAILED(hr)) 
	{
		//
		// Force pdate unit device info.
		// 
		if (hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION ||
			hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING) 
		{
			//
			// If bind information is changed, make it reload.
			//
			autolock.Release();
			pInvalidateNdasUnits();
		}
		return hr;
	}

	//
	// Plug In
	// - NDAS Controller
	//

	CComPtr<INdasUnit> pPrimaryNdasUnit;
	hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);

	if (FAILED(hr))
	{
		pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
		return hr;
	}

	CComPtr<INdasDevice> pNdasDevice;
	COMVERIFY(hr = pPrimaryNdasUnit->get_ParentNdasDevice(&pNdasDevice));
	if (FAILED(hr))
	{
		pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
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
		success = cpCheckPlugInCondForDVD(requestingAccess);
		if (!success) 
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
		pGetUnitDeviceCount() * sizeof(NDASBUS_UNITDISK);

	DWORD blockAclSize;
	COMVERIFY(pPrimaryNdasUnit->get_BlockAclSize(0, &blockAclSize));

	DWORD addTargetDataSize = 
		cbAddTargetDataSizeWithoutBACL + blockAclSize;

	CHeapPtr<NDASBUS_ADD_TARGET_DATA> addTargetData; 

	if (!addTargetData.AllocateBytes(addTargetDataSize))
	{
		// TODO: Out of memory
		return E_OUTOFMEMORY;
	}

	ZeroMemory(
		static_cast<PNDASBUS_ADD_TARGET_DATA>(addTargetData),
		addTargetDataSize);

	//
	//	Determine device mode
	//
	NDAS_DEV_ACCESSMODE deviceMode;

	XTLASSERT(requestingAccess & GENERIC_READ);
	if (requestingAccess & GENERIC_WRITE) 
	{
		if (NdasIsLogicalDiskType(m_NdasLogicalUnitDefinition.Type))
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

	UCHAR targetType = LogicalDeviceTypeToNdasBusTargetType(m_NdasLogicalUnitDefinition.Type);

	addTargetData->ulSize = addTargetDataSize;
	addTargetData->ulSlotNo = m_NdasLocation;
	addTargetData->ulTargetBlocks = 0; // initialization and will be added
	addTargetData->DeviceMode = deviceMode;
	addTargetData->ulNumberOfUnitDiskList = pGetUnitDeviceCount();
	addTargetData->ucTargetType = targetType;

	// Get Default LUR Options
	addTargetData->LurOptions = NdasServiceConfig::Get(nscLUROptions);

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
		addTargetData->LurOptions, 
		LdpfFlags, 
		LdpfValues);

	// Set Content Encryption from the primary unit device
	// (Only for Disk Devices)

	NDAS_UNITDEVICE_TYPE unitType;
	COMVERIFY(pPrimaryNdasUnit->get_Type(&unitType));

	if (NDAS_UNITDEVICE_TYPE_DISK == unitType) 
	{
		CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pPrimaryNdasUnit);
		ATLASSERT(pNdasDiskUnit.p);

		NDAS_CONTENT_ENCRYPT encryption;
		
		COMVERIFY(pNdasDiskUnit->get_ContentEncryption(&encryption));

		XTLASSERT(encryption.KeyLength <= 0xFF);
		XTLASSERT(encryption.Method <= 0xFF);
		addTargetData->CntEcrKeyLength = static_cast<UCHAR>(encryption.KeyLength);
		addTargetData->CntEcrMethod = static_cast<UCHAR>(encryption.Method);

		::CopyMemory(
			addTargetData->CntEcrKey,
			encryption.Key,
			encryption.KeyLength);

	}

	// set BACL data
	if (0 != blockAclSize)
	{
		addTargetData->BACLOffset = cbAddTargetDataSizeWithoutBACL;
		PBLOCK_ACCESS_CONTROL_LIST blockAcl = 
			ByteOffset<BLOCK_ACCESS_CONTROL_LIST>(
				addTargetData, addTargetData->BACLOffset);
		COMVERIFY(pPrimaryNdasUnit->FillBlockAcl(blockAcl));
	}

	if (NDASSCSI_TYPE_DISK_RAID1 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4 == targetType ||
		NDASSCSI_TYPE_DISK_RAID1R2 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R2 == targetType ||
		NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
		NDASSCSI_TYPE_DISK_RAID5 == targetType )
	{
		CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pPrimaryNdasUnit);
		ATLASSERT(pNdasDiskUnit.p);

		PVOID raidInfo = NULL;
		COMVERIFY(pNdasDiskUnit->get_RaidInfo(&raidInfo));

		ATLASSERT(NULL != raidInfo);

		::CopyMemory(
			&addTargetData->RAID_Info,
			raidInfo,
			sizeof(NDAS_RAID_INFO));

		if (0 == addTargetData->RAID_Info.BlocksPerBit) 
		{
			hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION;
			return hr;
		}
	}

	for (DWORD i = 0; i < pGetUnitDeviceCount(); ++i) 
	{	
		CComPtr<INdasDevice> pNdasDevice;
		NDAS_LOGICALDEVICE_GROUP ldGroup;
#if 0	
		pNdasUnit = GetUnitDevice(i);
		if (CNdasUnitDeviceNullPtr == pNdasUnit) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
			hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
			return hr;
		}

		pNdasDevice = pNdasUnit->GetParentDevice();
		XTLASSERT(CNdasDeviceNullPtr != pNdasDevice);
#else 	
		CComPtr<INdasUnit> pNdasUnit;
		HRESULT hr = pGetMemberNdasUnit(i, &pNdasUnit);

		//
		// Handle degraded mount.
		//
		if (FAILED(hr)) 
		{	
			//
			// TODO: Create CMissingNdasUnit class?
			// Missing member. This is allowed only for redundent disk.
			//
			if (NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
				NDASSCSI_TYPE_DISK_RAID5 == targetType) 
			{
				// For redundant RAID, get device information.
				hr = pGetNdasDevice(
					pGetLogicalUnitConfig().UnitDevices[i].DeviceId,
					&pNdasDevice);

				if (FAILED(hr)) 
				{
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
					return hr;
				}
			} 
			else 
			{
				// for non-redundant bind, reject plugin
				autolock.Release();
				pInvalidateNdasUnits();
				hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
				return hr;
			}		
		} 
		else 
		{
			COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
			XTLASSERT(pNdasDevice);

			COMVERIFY(pNdasUnit->get_LogicalUnitDefinition(&ldGroup));

			if (::memcmp(&ldGroup, &m_NdasLogicalUnitDefinition, sizeof(ldGroup)) != 0) 
			{
				//
				// This device is online but not is not configured as a member of this logical device.
				//		or not yet recognized by svc.
				if (NDASSCSI_TYPE_DISK_RAID1R3 == targetType ||
					NDASSCSI_TYPE_DISK_RAID4R3 == targetType ||
					NDASSCSI_TYPE_DISK_RAID5 == targetType) 
				{
					//
					// For redundant RAID, Handle this unit as missing member. 
					// ndasscsi will handle it for redundent RAID.
					//
					pNdasUnit.Release();
				}
				else 
				{
					// In other RAID, missing member is not acceptable. This may caused by inconsistent logical information.
					// Refresh it.
					autolock.Release();
					pInvalidateNdasUnits();
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
					return hr;
				}
			} 
		}
#endif

		PNDASBUS_UNITDISK pud = &addTargetData->UnitDiskList[i];

		if (!pNdasUnit) 
		{
			// Temp fix. pNdasDevice's remote address is not initialized until it has been online at least one moment.		
			// Use address from LDgroup.
			::CopyMemory(
				pud->Address.Node, 
				pGetLogicalUnitConfig().UnitDevices[i].DeviceId.Node, 
				sizeof(pud->Address.Node));
		} 
		else 
		{
			SOCKADDR_LPX sockAddrLpx;
			SOCKET_ADDRESS socketAddress;
			socketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
			socketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&sockAddrLpx);

			COMVERIFY(pNdasDevice->get_RemoteAddress(&socketAddress));

			::CopyMemory(
				pud->Address.Node, 
				sockAddrLpx.LpxAddress.Node,
				sizeof(pud->Address.Node));
		}
		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);

		SOCKADDR_LPX localSockAddrLpx;
		SOCKET_ADDRESS localSocketAddress;
		localSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
		localSocketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&localSockAddrLpx);

		COMVERIFY(pNdasDevice->get_LocalAddress(&localSocketAddress));

		C_ASSERT(
			sizeof(pud->NICAddr.Node) ==
			sizeof(localSockAddrLpx.LpxAddress.Node));

		if (!pNdasUnit) 
		{
			// Temp fix. pNdasDevice's remote address is not initialized until it has been online at least one moment.		
			ZeroMemory(&pud->NICAddr.Node, sizeof(pud->NICAddr.Node));
		}
		else 
		{
			::CopyMemory(
				pud->NICAddr.Node, 
				localSockAddrLpx.LpxAddress.Node, 
				sizeof(pud->NICAddr.Node));
		}
		
		pud->NICAddr.Port = htons(0); // should be zero

		if (!pNdasUnit) 
		{
			// This is missing member. This is temp fix.
			// iUserID and iPassword will not work for future version!!.
			pud->iUserID = pGetNdasUserId(
				m_NdasLogicalUnitDefinition.UnitDevices[i].UnitNo, 
				requestingAccess);

			pNdasDevice->get_HardwarePassword(&pud->iPassword);

			//  Assume RAID1, RAID4 and use primary device's
			COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&pud->ulUnitBlocks));
			pud->ulPhysicalBlocks = 0; // Unknown..
			pud->ucUnitNumber = static_cast<UCHAR>(pGetLogicalUnitConfig().UnitDevices[i].UnitNo);

			pud->ucHWType = 0/*HW_TYPE_ASIC*/; 		// Don't know right now..
			pud->ucHWVersion = pGetLogicalUnitConfig().DeviceHwVersions[i]; // Use hint from DIB
			pud->ucHWRevision = 0;	// Don't know right now..
			pud->LurnOptions |= LURNOPTION_MISSING;
		}
		else 
		{
			DWORD userId;
			DWORD unitNo;

			pNdasUnit->get_NdasDeviceUserId(requestingAccess, &userId);
			pNdasUnit->get_UnitNo(&unitNo);

			pud->iUserID = userId;
			pud->ucUnitNumber = static_cast<UCHAR>(unitNo);
			pNdasUnit->get_NdasDevicePassword(&pud->iPassword);
			pNdasUnit->get_UserBlocks(&pud->ulUnitBlocks);
			pNdasUnit->get_PhysicalBlocks(&pud->ulPhysicalBlocks);

			DWORD type, version, revision;

			pNdasDevice->get_HardwareType(&type);
			pNdasDevice->get_HardwareVersion(&version);
			pNdasDevice->get_HardwareRevision(&revision);

			pud->ucHWType = static_cast<UCHAR>(type);
			pud->ucHWVersion = static_cast<UCHAR>(version);
			pud->ucHWRevision = static_cast<USHORT>(revision);
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
		
		if (!pNdasUnit) 
		{
			//
			// Temp fix: This may not work for some HW such as emulator.
			// Driver also limit Max transfer size if needed.
			pud->UnitMaxDataRecvLength = pReadMaxRequestBlockLimitConfig(pud->ucHWVersion) * 512;
			pud->UnitMaxDataSendLength = pud->UnitMaxDataRecvLength;
		}
		else 
		{
			DWORD optimalTransferBlocks;
			COMVERIFY(pNdasUnit->get_OptimalMaxTransferBlocks(&optimalTransferBlocks));
			pud->UnitMaxDataSendLength = optimalTransferBlocks * 512;
			pud->UnitMaxDataRecvLength = optimalTransferBlocks * 512;
		}
		
		//
		// Add Target Info
		//

		NDAS_UNITDEVICE_TYPE unitType = NDAS_UNITDEVICE_TYPE_UNKNOWN;

		if (!!pNdasUnit)
		{
			COMVERIFY(pNdasUnit->get_Type(&unitType));
		}

		if (NDAS_UNITDEVICE_TYPE_DISK == unitType) 
		{
			CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pNdasUnit);
			ATLASSERT(pNdasDiskUnit.p);

			//
			// check if last DIB information is same with current one
			//
			hr = pNdasDiskUnit->IsDibUnchanged(); 
			if (FAILED(hr))
			{
				CComQIPtr<ILock> pNdasUnitLock(pNdasUnit);
				ATLASSERT(pNdasUnitLock.p);

				autolock.Release();
				pNdasDevice->InvalidateNdasUnit(pNdasUnit);

				//
				// Force refresh
				//
				CNdasEventPublisher& epub = pGetNdasEventPublisher();
				epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);				
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
				if (FAILED(pUnitDiskDevice->IsBitmapClean()))
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
			hr = pIsWriteAccessAllowed(fPSWriteShare, pPrimaryNdasUnit);
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

	UINT64 userBlocks;
	COMVERIFY(get_UserBlocks(&userBlocks));

	addTargetData->ulTargetBlocks = userBlocks;


	//
	//	We don't need an algorithm for a SCSI adapter's max request blocks
	//	We just need one more registry key to override.
	//	We set 0 to use driver's default max request blocks.
	//
	//	Use driver's default value
	DWORD dwMaxRequestBlocks = 0;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"NdasBusCtlPlugInEx2, SlotNo=%08X, MaxReqBlock=%d, DisEvt=%p, RecEvt=%p\n",
		m_NdasLocation, 
		dwMaxRequestBlocks, 
		m_hDisconnectedEvent, 
		m_hAlarmEvent);

	BOOL fVolatileRegister = pIsVolatile();

	_ASSERT(m_NdasLocation);
	
	BOOL success = NdasBusCtlPlugInEx2(
		m_NdasLocation,
		dwMaxRequestBlocks,
		m_hDisconnectedEvent,
		m_hAlarmEvent,
		fVolatileRegister);

	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlPlugInEx2 failed, hr=0x%X\n", hr);
		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LsBusCtlPlugInEx completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	success = NdasBusCtlAddTarget(addTargetData);
	if (!success) 
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

	pSetMountedAccess(requestingAccess);

	(void) pSetLastLogicalUnitPlugInFlags(LdpfFlags, LdpfValues);

	//
	// Set the status as pending, actual mount completion is
	// reported from PNP event handler to call OnMounted()
	// to complete this process
	//

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

	//
	// Clear Adapter Status
	//

	COMVERIFY(put_AdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT));

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"PlugIn completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

static const struct {
	BOOL CanHaveChildren;
	LURN_TYPE	ChildNodeType;
} LurnTypeTable[] = {
	TRUE, LURN_IDE_DISK,  // 0: LURN_AGGREGATION
	TRUE, LURN_IDE_DISK,  // 1: LURN_MIRRORING
	FALSE, LURN_NULL,     // 2: LURN_DIRECT_ACCESS
	FALSE, LURN_NULL,     // 3:LURN_CDROM
	FALSE, LURN_NULL,     // 4: LURN_OPTICAL_MEMORY
	TRUE, LURN_IDE_DISK,  // 5: LURN_RAID1	obsolete
	TRUE, LURN_IDE_DISK,  // 6: LURN_RAID4	obsolete
	TRUE, LURN_IDE_DISK,  // 7: LURN_RAID0
	FALSE, LURN_NULL,     // 8: LURN_AOD
	TRUE, LURN_IDE_DISK,  // 9: LURN_RAID1R
	TRUE, LURN_IDE_DISK,  // 10: LURN_RAID4R
	TRUE, LURN_IDE_DISK,  // 11: LURN_RAID5		
	FALSE, LURN_NULL,  // 12: LURN_SEQUENTIAL_ACCESS
	FALSE, LURN_NULL,  // 13: LURN_PRINTER
	FALSE, LURN_NULL,  // 14: LURN_PROCCESSOR
	FALSE, LURN_NULL,  // 15: LURN_WRITE_ONCE
	FALSE, LURN_NULL,  // 16: LURN_SCANNER
	FALSE, LURN_NULL,  // 17: LURN_MEDIUM_CHANGER
	FALSE, LURN_NULL,  // 18: LURN_COMMUNICATIONS
	FALSE, LURN_NULL,  // 19: LURN_ARRAY_CONTROLLER
	FALSE, LURN_NULL,  // 20: LURN_ENCLOSURE_SERVICES
	FALSE, LURN_NULL,  // 21: LURN_REDUCED_BLOCK_COMMAND
	FALSE, LURN_NULL,  // 22: LURN_OPTIOCAL_CARD
};


HRESULT
CNdasLogicalUnit::pPlugInNdasPort(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	HRESULT hr;

	CAutoLock autolock(this);
	NDAS_DEV_ACCESSMODE deviceMode;

	BOOL fPSWriteShare = pIsPSWriteShareCapable();

	hr = pIsSafeToPlugIn(requestingAccess);
	if (FAILED(hr)) 
	{
		//
		// Force pdate unit device info.
		// 
		if (hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION ||
			hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING) 
		{
			//
			// If bind information is changed, make it reload.
			//
			autolock.Release();
			pInvalidateNdasUnits();
		}
		return hr;
	}

	//
	// Plug In
	// - NDAS Controller
	//

	CComPtr<INdasUnit> pPrimaryNdasUnit;
	hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);
	if (FAILED(hr)) 
	{
		pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
		return hr;
	}

	CComPtr<INdasDevice> pPrimaryNdasDevice;
	hr = pPrimaryNdasUnit->get_ParentNdasDevice(&pPrimaryNdasDevice);
	if (FAILED(hr)) 
	{
		pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
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
		success = cpCheckPlugInCondForDVD(requestingAccess);
		if (!success) 
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

	//
	//	Determine device mode
	//
	XTLASSERT(requestingAccess & GENERIC_READ);
	if (requestingAccess & GENERIC_WRITE) 
	{
		if (NdasIsLogicalDiskType(m_NdasLogicalUnitDefinition.Type))
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
	LURN_TYPE nodeType;
	BOOL rootNodeExists  = FALSE;
	UCHAR lurnDeviceInterface;

	LogicalDeviceTypeToRootLurnType(m_NdasLogicalUnitDefinition.Type, &nodeType, &lurnDeviceInterface);

	if (nodeType >= RTL_NUMBER_OF(LurnTypeTable)) 
	{
		pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
		hr = NDASSVC_ERROR_UNSUPPORTED_LOGICALDEVICE_TYPE;
		return hr;
	}

	//
	// LUR init data
	// Root node init data
	//
	ndasLogicalUnitAddress = NdasLocationToLogicalUnitAddress(m_NdasLocation);

	rootNodeExists = LurnTypeTable[nodeType].CanHaveChildren;

	rootNodeInitData.NodeType = nodeType;
	rootNodeInitData.StartLogicalBlockAddress.QuadPart = 0;

	// TODO: replace more flexible method to get interface type
	//   according to the logical unit type.
	rootNodeInitData.NodeDeviceInterface = LURN_DEVICE_INTERFACE_LURN;

	if (nodeType == LURN_NDAS_RAID0 ||
		nodeType == LURN_NDAS_RAID1 ||
		nodeType == LURN_NDAS_RAID4 ||
		nodeType == LURN_NDAS_RAID5) 
	{
		//
		// Current RAID0/4/5 uses block concatenation, so forms a large block
		// by adding each bock of member disks.
		//
		UINT64 userBlocks;
		COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&userBlocks));
		rootNodeInitData.EndLogicalBlockAddress.QuadPart = userBlocks - 1;
	} 
	else 
	{
		UINT64 userBlocks;
		COMVERIFY(get_UserBlocks(&userBlocks));
		rootNodeInitData.EndLogicalBlockAddress.QuadPart = userBlocks - 1;
	}

	if (nodeType == LURN_NDAS_RAID1 ||
		nodeType == LURN_NDAS_RAID4 ||
		nodeType == LURN_NDAS_RAID5) 
	{
		CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pPrimaryNdasUnit);
		ATLASSERT(pNdasDiskUnit.p);

		PNDAS_RAID_INFO	infoRaid = NULL;
		COMVERIFY(pNdasDiskUnit->get_RaidInfo(reinterpret_cast<PVOID*>(&infoRaid)));

		XTLASSERT(NULL != infoRaid);

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

	DWORD blockAclSize;
	COMVERIFY(pPrimaryNdasUnit->get_BlockAclSize(0, &blockAclSize));

	LARGE_INTEGER logicalUnitEndAddress;
	logicalUnitEndAddress.QuadPart = rootNodeInitData.EndLogicalBlockAddress.QuadPart;
	logicalUnitDescriptor = NdasPortCtlBuildNdasDluDeviceDescriptor(
		ndasLogicalUnitAddress,
		0,
		deviceMode,
		0,
		&logicalUnitEndAddress,
		rootNodeExists?pGetUnitDeviceCount():0, // Leaf count
		0,
		blockAclSize,
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
	NDAS_UNITDEVICE_TYPE unitType;
	COMVERIFY(pPrimaryNdasUnit->get_Type(&unitType));

	if (NDAS_UNITDEVICE_TYPE_DISK == unitType) 
	{
		CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pPrimaryNdasUnit);
		ATLASSERT(pNdasDiskUnit.p);

		NDAS_CONTENT_ENCRYPT encryption;

		COMVERIFY(pNdasDiskUnit->get_ContentEncryption(&encryption));

		NDAS_DISK_ENCRYPTION_DESCRIPTOR diskEncDesc;

		XTLASSERT(encryption.KeyLength <= 0xFF);
		XTLASSERT(encryption.Method <= 0xFF);

		diskEncDesc.EncryptKeyLength = static_cast<UCHAR>(encryption.KeyLength);
		diskEncDesc.EncryptType = static_cast<NDAS_DISK_ENCRYPTION_TYPE>(encryption.Method);
		::CopyMemory(
			diskEncDesc.EncryptKey,
			encryption.Key,
			encryption.KeyLength);

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
	if (0 != blockAclSize)
	{
		PNDAS_BLOCK_ACL bacl;
		COMVERIFY(NdasPortCtlGetNdasBacl(logicalUnitDescriptor, &bacl));
		COMVERIFY(pPrimaryNdasUnit->FillBlockAcl(bacl));
	}

	for (DWORD i = 0; i < pGetUnitDeviceCount(); ++i) 
	{	
		NDAS_LOGICALDEVICE_GROUP ludef;
		ULONG idx_lurn = i + (rootNodeExists?1:0);

		CComPtr<INdasUnit> pNdasUnit;
		pGetMemberNdasUnit(i, &pNdasUnit);

		CComPtr<INdasDevice> pNdasDevice;

		// Handle degraded mount.
		if (!pNdasUnit) 
		{	
			// TODO: Create CMissingNdasUnit class?
			// Missing member. This is allowed only for redundent disk.
			if (LURN_RAID1R == nodeType ||
				LURN_RAID4R == nodeType ||
				LURN_RAID5 == nodeType ) 
			{
				// For redundent RAID, get device information.
				hr = pGetNdasDevice(
					pGetLogicalUnitConfig().UnitDevices[i].DeviceId,
					&pNdasDevice);
				if (FAILED(hr)) 
				{
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
					return hr;
				}
			}
			else 
			{
				// for non-redundent bind, reject plugin
				autolock.Release();
				pInvalidateNdasUnits();
				hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
				return hr;		
			}		
		} 
		else 
		{
			COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
			XTLASSERT(pNdasDevice);

			COMVERIFY(pNdasUnit->get_LogicalUnitDefinition(&ludef));

			if (::memcmp(&ludef, &m_NdasLogicalUnitDefinition, sizeof(ludef)) != 0) 
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
					pNdasUnit.Release();
				}
				else 
				{
					// In other RAID, missing member is not acceptable. This may caused by inconsistent logical information.
					// Refresh it.
					autolock.Release();
					pInvalidateNdasUnits();
					hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
					return hr;
				}
			} 
		}

		PLURELATION_NODE_DESC lurNode = NdasPortCtlFindNodeDesc(logicalUnitDescriptor, idx_lurn);
		NDASPORTCTL_NODE_INITDATA nodeInitData;
		PNDASPORTCTL_INIT_ATADEV ataSpecific = &nodeInitData.NodeSpecificData.Ata;

		ataSpecific->ValidFieldMask =
				NDASPORTCTL_ATAINIT_VALID_TRANSPORT_PORTNO |
				NDASPORTCTL_ATAINIT_VALID_BINDING_ADDRESS |
				NDASPORTCTL_ATAINIT_VALID_USERID |
				NDASPORTCTL_ATAINIT_VALID_USERPASSWORD |
				NDASPORTCTL_ATAINIT_VALID_OEMCODE;

		if (!pNdasUnit) 
		{
			//
			// Assume LURN is a direct access device.
			//
			nodeInitData.NodeType = LURN_DIRECT_ACCESS;
			// We don't know the device interface type, so put in
			// LURN virtual interface.
			// TODO: Should specify device interface here.
			nodeInitData.NodeDeviceInterface = LURN_DEVICE_INTERFACE_LURN;

			nodeInitData.StartLogicalBlockAddress.QuadPart = 0;
			//
			//  Assume RAID1, RAID4 and use primary device's
			//
			UINT64 userBlocks;
			COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&userBlocks));
			nodeInitData.EndLogicalBlockAddress.QuadPart = 
				userBlocks - 1; 

			//
			// Temp fix. pNdasDevice's remote address is not initialized until it has been online at least one moment.		
			// Use address from LDgroup.
			//
			::CopyMemory(
				ataSpecific->DeviceIdentifier.Identifier, 
				pGetLogicalUnitConfig().UnitDevices[i].DeviceId.Node, 
				sizeof(ataSpecific->DeviceIdentifier.Identifier));
			//
			// Temp fix. pNdasDevice's remote address is not initialized until it has been online at least one moment.
			//
			ZeroMemory(&ataSpecific->BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
			ataSpecific->DeviceIdentifier.UnitNumber = static_cast<UCHAR>(pGetLogicalUnitConfig().UnitDevices[i].UnitNo);
			ataSpecific->HardwareType = 0; 	/*HW_TYPE_ASIC*/	// Don't know right now..
			ataSpecific->HardwareVersion = pGetLogicalUnitConfig().DeviceHwVersions[i]; // Use hint from DIB
			ataSpecific->HardwareRevision = 0;	// Don't know right now..
			ataSpecific->TransportPortNumber = htons(LPXRP_NDAS_PROTOCOL);
			// lurNode->ulPhysicalBlocks = 0; // Unknown..
			ataSpecific->UserId = pGetNdasUserId(
				m_NdasLogicalUnitDefinition.UnitDevices[i].UnitNo, 
				requestingAccess);
			ZeroMemory(ataSpecific->UserPassword, NDASPORTCTL_USERPASSWORD_LENGTH);

			UINT64 hardwareDefaultOemCode;
			pNdasDevice->get_HardwarePassword(&hardwareDefaultOemCode);
			CopyMemory(
				ataSpecific->DeviceOemCode, 
				&hardwareDefaultOemCode, 
				NDASPORTCTL_OEMCODE_LENGTH);
		} 
		else 
		{
			NDAS_UNITDEVICE_HARDWARE_INFO unitdevHwInfo;
			pNdasUnit->get_HardwareInfo(&unitdevHwInfo);

			UnitDeviceTypeToLeafLurnType(
				static_cast<NDAS_UNIT_TYPE>(unitdevHwInfo.MediaType),
				&nodeInitData.NodeType,
				&nodeInitData.NodeDeviceInterface);

			UINT64 userBlocks;
			COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&userBlocks));

			nodeInitData.StartLogicalBlockAddress.QuadPart = 0;
			nodeInitData.EndLogicalBlockAddress.QuadPart = userBlocks - 1;

			NDAS_DEVICE_ID ndasDeviceId;
			COMVERIFY(pNdasDevice->get_NdasDeviceId(&ndasDeviceId));

			::CopyMemory(
				ataSpecific->DeviceIdentifier.Identifier, 
				ndasDeviceId.Node,
				sizeof(ataSpecific->DeviceIdentifier.Identifier));

			SOCKADDR_LPX localSockAddrLpx;
			SOCKET_ADDRESS localSocketAddress;
			localSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
			localSocketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&localSockAddrLpx);

			COMVERIFY(pNdasDevice->get_LocalAddress(&localSocketAddress));

			LpxCommConvertLpxAddressToTaLsTransAddress(
				&localSockAddrLpx.LpxAddress,
				&ataSpecific->BindingAddress);

			DWORD unitNo;
			pNdasUnit->get_UnitNo(&unitNo);

			ataSpecific->DeviceIdentifier.UnitNumber = static_cast<UCHAR>(unitNo);

			DWORD type, version, revision;
			pNdasDevice->get_HardwareType(&type);
			pNdasDevice->get_HardwareVersion(&version);
			pNdasDevice->get_HardwareRevision(&revision);

			ataSpecific->HardwareType = static_cast<UCHAR>(type);
			ataSpecific->HardwareVersion = static_cast<UCHAR>(version);
			ataSpecific->HardwareRevision = static_cast<UCHAR>(revision);	
			
			ataSpecific->TransportPortNumber = htons(LPXRP_NDAS_PROTOCOL);

			DWORD userId;
			pNdasUnit->get_NdasDeviceUserId(requestingAccess, &userId);

			ataSpecific->UserId = userId;

			ZeroMemory(ataSpecific->UserPassword, NDASPORTCTL_USERPASSWORD_LENGTH);

			UINT64 unitDefaultOemCode;

			pNdasUnit->get_NdasDevicePassword(&unitDefaultOemCode);

			CopyMemory(
				ataSpecific->DeviceOemCode, 
				&unitDefaultOemCode, 
				NDASPORTCTL_OEMCODE_LENGTH);

			// lurNode->ulPhysicalBlocks = pNdasUnit->GetPhysicalBlockCount();
		}

		ataSpecific->TransportPortNumber = NDAS_DEVICE_LPX_PORT;

		if ( NdasPortCtlSetupLurNode(lurNode, deviceMode, &nodeInitData) == FALSE ) 
		{
			pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_RESOLUTION);
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
		
		if (!pNdasUnit) 
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
			DWORD optimalMaxTransferBlocks;
			COMVERIFY(pNdasUnit->get_OptimalMaxTransferBlocks(&optimalMaxTransferBlocks));
			lurNode->MaxDataSendLength = optimalMaxTransferBlocks * 512;
			lurNode->MaxDataRecvLength = optimalMaxTransferBlocks * 512;
		}
		
		//
		// Add Target Info
		//

		NDAS_UNITDEVICE_TYPE unitType = NDAS_UNITDEVICE_TYPE_UNKNOWN;
		if (!!pNdasUnit)
		{
			COMVERIFY(pNdasUnit->get_Type(&unitType));
		}

		if (NDAS_UNITDEVICE_TYPE_DISK == unitType)
		{
			CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pNdasUnit);
			ATLASSERT(pNdasDiskUnit.p);

			//
			// check if last DIB information is same with current one
			//
			hr = pNdasDiskUnit->IsDibUnchanged(); 
			if (FAILED(hr))
			{
				CComQIPtr<ILock> pNdasUnitLock(pNdasUnit);
				ATLASSERT(pNdasUnitLock.p);

				autolock.Release();
				pNdasDevice->InvalidateNdasUnit(pNdasUnit);

				//
				// Force refresh
				//
				CNdasEventPublisher& epub = pGetNdasEventPublisher();
				epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);				
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
				if (FAILED(pUnitDiskDevice->IsBitmapClean()))
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
			hr = pIsWriteAccessAllowed(fPSWriteShare, pPrimaryNdasUnit);
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

	pSetMountedAccess(requestingAccess);

	(void) pSetLastLogicalUnitPlugInFlags(LdpfFlags, LdpfValues);

	//
	// Set the status as pending, actual mount completion is
	// reported from PNP event handler to call OnMounted()
	// to complete this process
	//

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

	//
	// Clear Adapter Status
	//

	put_AdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"PlugIn completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

HRESULT
CNdasLogicalUnit::PlugIn(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues)
{
	if (IsNdasPortMode())
	{
		return pPlugInNdasPort(requestingAccess, LdpfFlags, LdpfValues);
	}
	else 
	{
		return pPlugInNdasBus(requestingAccess, LdpfFlags, LdpfValues);
	}
}

HRESULT
CNdasLogicalUnit::Unplug()
{
	HRESULT hr;
	BOOL success(FALSE);

	CAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplugging, LogicalUnit=%d\n", m_NdasLogicalUnitId);

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

	if (IsNdasPortMode())
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

		NDAS_LOGICALUNIT_ADDRESS address = pGetNdasLogicalUnitAddress();

		hr = NdasPortCtlUnplugLogicalUnit(handle, address, 0);

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlUnplugLogicalUnit failed, hr=0x%X\n", hr);

			return hr;
		}
	}
	else
	{
		//
		// Remove target ejects the disk and the volume.
		//

		success = NdasBusCtlRemoveTarget(m_NdasLocation);
		if (!success) 
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

		success = NdasBusCtlUnplug(m_NdasLocation);

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
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

	pUpdateBindStateAndError();	

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Remove Ldpf from the registry
	(void) pClearLastLogicalUnitPlugInFlags();

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplug completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

HRESULT 
CNdasLogicalUnit::Eject()
{
	CAutoLock autolock(this);

	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Ejecting, LogicalUnit=%d\n", m_NdasLogicalUnitId);

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

	if (IsNdasPortMode())
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

		NDAS_LOGICALUNIT_ADDRESS address = pGetNdasLogicalUnitAddress();

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
	else
	{
		BOOL success = ::NdasBusCtlEject(m_NdasLocation);

		if (!success) 
		{
			hr = HRESULT_FROM_WIN32(GetLastError());

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlEject failed, SlotNo=%08X, error=0x%X\n", 
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

	pUpdateBindStateAndError();	
	
	//
	// Now we have to wait until the ejection is complete
	//
	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Eject completed successfully, SlotNo=%08X\n",
		m_NdasLocation);

	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::EjectEx(
	__out CONFIGRET* ConfigRet, 
	__out PNP_VETO_TYPE* VetoType,
	__out BSTR* VetoName)
{
	TCHAR vetoNameBuffer[MAX_PATH] = {0};
	HRESULT hr = EjectEx(ConfigRet, VetoType, vetoNameBuffer, MAX_PATH);
	if (FAILED(hr))
	{
		return hr;
	}
	CComBSTR vetoName(vetoNameBuffer);
	COMVERIFY(vetoName.CopyTo(VetoName));
	return hr;
}

STDMETHODIMP
CNdasLogicalUnit::EjectEx(
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName,
	DWORD nNameLength)
{
	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"EjectEx, LogicalDevice=%d\n", m_NdasLogicalUnitId);

	CAutoLock autolock(this);

	NDAS_LOCATION ndasLocation = m_NdasLocation;
	NDAS_LOGICALDEVICE_STATUS status = m_status;

	if (status != NDAS_LOGICALDEVICE_STATUS_MOUNTED)
	{
		hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED;

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"EjectEx is requested to not mounted logical device\n");

		return hr;
	}

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	autolock.Release();

	CONFIGRET cret;

	if (IsNdasPortMode())
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
		lun = pGetNdasLogicalUnitAddress();

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
	else
	{
		hr = pRequestNdasScsiDeviceEjectW(
			ndasLocation, 
			&cret, 
			pVetoType, 
			pszVetoName, 
			nNameLength);
	}

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"RequestEject failed, ndasLocation=%d, hr=0x%X\n", ndasLocation, hr);
		pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
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

STDMETHODIMP 
CNdasLogicalUnit::GetSharedWriteInfo(
	__out BOOL * SharedWrite, 
	__out BOOL * PrimaryHost)
{
	HRESULT hr;

#ifdef NDAS_FEATURE_DISABLE_SHARED_WRITE
	if (SharedWrite) *SharedWrite = FALSE;
	if (PrimaryHost) *PrimaryHost = FALSE;
	return S_OK;
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
	BOOL success = FALSE;
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
			return hr;
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
	DWORD supportedFeatures, enabledFeatures, connectionCount;

	if (ndasPort == FALSE) 
	{
		//
		// the NDAS bus driver.
		//

		BusEnumQuery.InfoClass = INFORMATION_PDO;
		BusEnumQuery.Size = sizeof(NDASBUS_QUERY_INFORMATION);
		BusEnumQuery.SlotNo = m_NdasLocation;
		BusEnumQuery.Flags = 0;

		success = ::NdasBusCtlQueryInformation(
			&BusEnumQuery,
			sizeof(NDASBUS_QUERY_INFORMATION),
			&BusEnumInformation,
			sizeof(NDASBUS_INFORMATION));
		if (!success) 
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			hr = SUCCEEDED(hr) ? E_FAIL : hr;

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlQueryInformation failed at slot %d, hr=0x%X\n", 
				m_NdasLocation, hr);

			return hr;
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
			hr = HRESULT_FROM_WIN32(GetLastError());
			hr = SUCCEEDED(hr) ? E_FAIL : hr;

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"CreateFile(%ls) failed, hr=0x%X\n",
				m_DevicePath, hr);
			return hr;
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
			return hr;
		}
	}

	if (deviceMode == DEVMODE_SHARED_READWRITE) 
	{
		if (SharedWrite) *SharedWrite = TRUE;

		if (enabledFeatures & NDASFEATURE_SECONDARY)
		{
			if (PrimaryHost)	*PrimaryHost = FALSE;
		} 
		else 
		{
			if (PrimaryHost) *PrimaryHost = TRUE;
		}
	}
	else 
	{
		if (SharedWrite) *SharedWrite = FALSE;
		if (PrimaryHost) *PrimaryHost = FALSE;
	}

	//
	// If XixFS exists, skip query to the LfsFilter
	//
	// TODO: provide the same protection as LfsFilter gets.
	//
	hr = XixfsCtlGetVersion(NULL,NULL,NULL,NULL,NULL,NULL);
	if (FAILED(hr)) 
	{
		if (SharedWrite)
		{
			*SharedWrite = TRUE;
		}
		return S_OK;
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
	if (IsNdasPortMode())
	{
		success = ::LfsFiltQueryNdasUsage(
			ndasLogicalDeviceAddress.Address,
			&usage);
	}
	else
	{
		success = ::LfsFiltQueryNdasUsage(
			m_NdasLocation,
			&usage);
	}

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LfsFiltQueryNdasUsage failed, SlotNo=%08X, error=0x%X\n",
			m_NdasLocation, hr);
		
		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LfsFiltQueryNdasUsage: SlotNo=%08X, primary=%d, secondary=%d, hasLockedVolume=%d.\n",
		m_NdasLocation,
		usage.ActPrimary,
		usage.ActSecondary,
		usage.HasLockedVolume);

	if (SharedWrite) 
	{
		//
		// LFS filter should report Primary or Secondary
		// Otherwise, the filter is not active on the NDAS device.
		// If the volume is locked, shared write is not capable.
		//
		if ((usage.ActPrimary || usage.ActSecondary) && !usage.HasLockedVolume)
		{
			*SharedWrite = TRUE;
		}
		else
		{
			*SharedWrite = FALSE;
		}
	}

	if (PrimaryHost)
	{
		*PrimaryHost = usage.ActPrimary;
	}

	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::SetMountOnReady(
	__in ACCESS_MASK access, 
	__in BOOL fReducedMountOnReadyAccess /*= FALSE */)
{
	CAutoLock autolock(this);

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

	return S_OK;
}

DWORD
CNdasLogicalUnit::pGetMountTick()
{
	return m_dwMountTick;
}

STDMETHODIMP 
CNdasLogicalUnit::get_NdasLocation(__out NDAS_LOCATION* NdasLocation)
{
	CAutoLock autolock(this);
	*NdasLocation = m_NdasLocation;
	return S_OK;
}

NDAS_LOGICALUNIT_ADDRESS 
CNdasLogicalUnit::pGetNdasLogicalUnitAddress()
{
	return NdasLocationToLogicalUnitAddress(m_NdasLocation);
}

STDMETHODIMP 
CNdasLogicalUnit::get_LogicalUnitAddress(
	__out NDAS_LOGICALUNIT_ADDRESS* LogicalUnitAddress)
{
	CAutoLock autolock(this);
	*LogicalUnitAddress = NdasLocationToLogicalUnitAddress(m_NdasLocation);
	return S_OK;
}

HRESULT
CNdasLogicalUnit::pIsSafeToPlugIn(ACCESS_MASK requestingAccess)
{
	CAutoLock autolock(this);

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED;
	}

	//
	// We allows plug in calls from NDAS_LOGICALDEVICE_STATUS_UNMOUNTED only.
	//
	if (m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNTED) 
	{
		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_UNMOUNTED;
	}


	BOOL MountableStateChanged;
	MountableStateChanged = pUpdateBindStateAndError();	

	if (!pIsMountable()) 
	{
		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
	}
	else if (MountableStateChanged) 
	{
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);
		// To do: use another error code.
		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
	}

	//
	// check access permission
	//

	//
	// only GENERIC_READ and GENERIC_WRITE is allowed
	//
	requestingAccess &= (GENERIC_READ | GENERIC_WRITE);

	ACCESS_MASK grantedAccess;
	COMVERIFY(get_GrantedAccess(&grantedAccess));

	if ((grantedAccess & requestingAccess) != requestingAccess) 
	{
		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED;
	}

	ACCESS_MASK allowedAccess;
	COMVERIFY(get_AllowedAccess(&allowedAccess));
	if ((requestingAccess & allowedAccess) != requestingAccess) 
	{
		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_FAILED;
	}

	return S_OK;
}

void
CNdasLogicalUnit::pLocateRegContainer()
{
	//
	// Registry Container
	// HKLM\Software\NDAS\LogDevices\XXXXXXXX
	//

	BOOL success, fWriteData = TRUE;

	m_dwHashValue = pGetHashValue();

	while (TRUE) 
	{
		COMVERIFY(StringCchPrintf(
			m_szRegContainer, 30, 
			_T("LogDevices\\%08X"), m_dwHashValue));

		NDAS_LOGICALDEVICE_GROUP ldGroup;
		DWORD cbData = 0;
		success = _NdasSystemCfg.GetSecureValueEx(
			m_szRegContainer,
			_T("Data"),
			&ldGroup,
			sizeof(ldGroup),
			&cbData);

		if (success && cbData == sizeof(ldGroup)) 
		{
			if (0 != ::memcmp(&ldGroup, &m_NdasLogicalUnitDefinition, sizeof(ldGroup))) 
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
		success = _NdasSystemCfg.SetSecureValueEx(
			m_szRegContainer,
			_T("Data"),
			&m_NdasLogicalUnitDefinition,
			sizeof(m_NdasLogicalUnitDefinition));
		if (!success) 
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
CNdasLogicalUnit::pGetHashValue()
{
	CAutoLock autolock(this);

	return ::crc32_calc(
		(const UCHAR*) &m_NdasLogicalUnitDefinition, 
		sizeof(NDAS_LOGICALDEVICE_GROUP));
}

void
CNdasLogicalUnit::pAllocateNdasLocation()
{
	CAutoLock autolock(this);
	//
	// We have different policy for single and RAID
	//
	if (IsNdasPortMode())
	{
		NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress = {0};
		if (NdasIsRaidDiskType(m_NdasLogicalUnitDefinition.Type)) 
		{
			//
			// (PathId, TargetId, Lun) = (1, x, y)
			// where x = 1 - 31 and y = 1 - 127
			//
			// lun = y * 15 + x;
			//
			DWORD lun = pGetRaidSlotNo();
			logicalUnitAddress.PathId = static_cast<UCHAR>(m_NdasLogicalUnitDefinition.Type);
			logicalUnitAddress.TargetId = static_cast<UCHAR>(lun / 127);
			logicalUnitAddress.Lun = static_cast<UCHAR>(lun % 127 + 1);
		}
		else
		{
			CComPtr<INdasUnit> pFirstUnitDevice;
			COMVERIFY(pGetMemberNdasUnit(0, &pFirstUnitDevice));
			XTLASSERT(pFirstUnitDevice);

			DWORD slotNo, unitNo;

			CComPtr<INdasDevice> pNdasDevice;
			COMVERIFY(pFirstUnitDevice->get_ParentNdasDevice(&pNdasDevice));
			COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));
			COMVERIFY(pFirstUnitDevice->get_UnitNo(&unitNo));

			logicalUnitAddress.PathId = 0;
			logicalUnitAddress.TargetId = static_cast<UCHAR>(slotNo);

			logicalUnitAddress.Lun = static_cast<UCHAR>(unitNo);
		}

		m_NdasLocation = NdasLogicalUnitAddressToLocation(logicalUnitAddress);
	}
	else
	{
		if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_SET_GROUP(m_NdasLogicalUnitDefinition.Type)) 
		{
			m_NdasLocation = pGetRaidSlotNo();
		}
		else
		{
			CComPtr<INdasUnit> pFirstUnitDevice;
			COMVERIFY(pGetMemberNdasUnit(0, &pFirstUnitDevice));
			XTLASSERT(pFirstUnitDevice);

			DWORD slotNo, unitNo;

			CComPtr<INdasDevice> pNdasDevice;
			COMVERIFY(pFirstUnitDevice->get_ParentNdasDevice(&pNdasDevice));
			COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));
			COMVERIFY(pFirstUnitDevice->get_UnitNo(&unitNo));

			m_NdasLocation = slotNo * 10 + unitNo;
		}
	}
	XTLASSERT(m_NdasLocation != 0);
}

void
CNdasLogicalUnit::pDeallocateNdasLocation()
{
	CAutoLock autolock(this);
	m_NdasLocation = 0;
}

STDMETHODIMP 
CNdasLogicalUnit::get_UserBlocks(__out UINT64* Blocks)
{
	CAutoLock autolock(this);

	*Blocks = 0;

	if (!pIsMountable()) 
	{
		return E_FAIL;
	}

	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pGetPrimaryNdasUnit(&pNdasUnit);
	if (FAILED(hr)) 
	{
		return hr;
	}

	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK(m_NdasLogicalUnitDefinition.Type)) 
	{
		return pNdasUnit->get_UserBlocks(Blocks);
	}

	UINT64 blocks = 0;
	UINT64 singleBlocks;
	switch(m_NdasLogicalUnitDefinition.Type) 
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		//
		// Device don't have DIB. First device's user block count is size of mirror.
		//
		{
			CComPtr<INdasUnit> pMemberNdasUnit;
			hr = pGetMemberNdasUnit(0, &pMemberNdasUnit);
			if (FAILED(hr))  
			{
				blocks = 0;
			}
			else 
			{
				COMVERIFY(pMemberNdasUnit->get_UserBlocks(&blocks));
			}
		}
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		if (pIsComplete()) 
		{
			for (DWORD i = 0; i < pGetUnitDeviceCount(); ++i) 
			{
				CComPtr<INdasUnit> pMemberNdasUnit;
				pGetMemberNdasUnit(i, &pMemberNdasUnit);
				if (FAILED(hr)) 
				{
					blocks = 0;
					break;
				}
				else 
				{
					COMVERIFY(pMemberNdasUnit->get_UserBlocks(&singleBlocks));
					blocks += singleBlocks;
				}
			}
		}
		else 
		{
			// We don't know the size if all unit is not online.
			blocks = 0;
		}
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		COMVERIFY(pNdasUnit->get_UserBlocks(&blocks));
		blocks *= pGetUnitDeviceCount();
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:			
		COMVERIFY(pNdasUnit->get_UserBlocks(&blocks));
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:	
		COMVERIFY(pNdasUnit->get_UserBlocks(&singleBlocks));
		blocks += singleBlocks * (pGetUnitDeviceCountInRaid() - 1);
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		COMVERIFY(pNdasUnit->get_UserBlocks(&blocks));
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
		blocks = 0;
		break;
	default: 
		// not implemented yet : DVD, VDVD, MO, FLASH ...
		XTLASSERT(FALSE);
		break;
	}
	*Blocks = blocks;
	return S_OK;
}

void
CNdasLogicalUnit::OnSystemShutdown()
{
	CAutoLock autolock(this);

	SetRiskyMountFlag(FALSE);

	m_fShutdown = TRUE;
}

void
CNdasLogicalUnit::OnDisconnected()
{
	CAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LogicalUnit=%d, Disconnect Event.\n", m_NdasLogicalUnitId);
#if 0
	// NDASBUS will do unplug instead.
	BOOL success = Unplug();
	if (!success) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, Failed to handle disconnect event, error=0x%X\n", 
			m_logicalDeviceId, GetLastError());
	}
#endif

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Set the disconnected flag
	m_fDisconnected = TRUE;
}


void
CNdasLogicalUnit::OnMounted(BSTR DevicePath, NDAS_LOGICALUNIT_ABNORMALITIES Abnormalities)
{
	CAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LogicalUnit=%d, MOUNTED as %ls\n", 
		m_NdasLogicalUnitId,
		DevicePath);

	DWORD dwTick = ::GetTickCount();
	m_dwMountTick = (dwTick == 0) ? 1: dwTick; // 0 is used for special purpose

	m_DevicePath.AssignBSTR(DevicePath);

	pSetLastMountAccess(m_MountedAccess);
	SetRiskyMountFlag(TRUE);
	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
}

HRESULT
CNdasLogicalUnit::pReconcileWithNdasPort()
{
	XTLASSERT(IsNdasPortMode());

	CNdasServiceDeviceEventHandler& devEventHandler =
		pGetNdasDeviceEventHandler();

	HRESULT hr = devEventHandler.GetLogicalUnitDevicePath(
		m_NdasLocation,
		&m_DevicePath);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit device path is unknown\n");
	}
	else
	{
		NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = 
			pGetNdasLogicalUnitAddress();
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

				pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

				return hr;
			}
			else
			{
				//
				// Get the device mode
				//

				ULONG deviceMode;
				ACCESS_MASK	desiredAccess;

				deviceMode = pLurInfo->Lur.DeviceMode;

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
					pSetMountedAccess(desiredAccess);
				}

				// Free the full LUR information.
				HeapFree(GetProcessHeap(), 0, pLurInfo);
			}
		}	
	}

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	pGetNdasDeviceEventHandler().RescanDriveLetters();

	return S_OK;
}

HRESULT
CNdasLogicalUnit::pReconcileWithNdasBus()
{
	HANDLE hAlarm, hDisconnect;
	ULONG deviceMode;
	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = pGetNdasLogicalUnitAddress();

	BOOL success = ::NdasBusCtlQueryPdoEvent(
	    m_NdasLocation, 
	    &hAlarm,
	    &hDisconnect);

	//
	// Reconciliation failure?
	//
	if (!success)
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
	success = ::NdasBusCtlQueryDeviceMode(m_NdasLocation, &deviceMode);
	if (!success)
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
			pSetMountedAccess(desiredAccess);
		}
	}

	//
	// Retrieve the LUR full information
	//
	PNDSCIOCTL_LURINFO pLurInfo = NULL;
	success = ::NdasBusCtlQueryMiniportFullInformation(
		m_NdasLocation, &pLurInfo);
	if (success && pLurInfo)
	{
		HeapFree(GetProcessHeap(), 0, pLurInfo);
	}
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"QueryMiniportFullInformation failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLocation, GetLastError());

		pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

		return S_OK;
	}

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	return S_OK;
}

void
CNdasLogicalUnit::OnDismounted()
{
	CAutoLock autolock(this);

	if (m_fDisconnected)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"LogicalUnit=%d, Unmount Completed (by disconnection).\n", m_NdasLogicalUnitId);
	}
	else
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"LogicalUnit=%d, Unmount Completed.\n", m_NdasLogicalUnitId);
	}

	m_dwMountTick = 0;

	if (!m_fDisconnected)
	{
		// clears the mount flag only on unmount by user's request
		pSetLastMountAccess(0); 
	}

	// clears the risky mount flag
	SetRiskyMountFlag(FALSE);

	// Remove Ldpf from the registry
	(void) pClearLastLogicalUnitPlugInFlags();

	pUpdateBindStateAndError();
	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
}

void
CNdasLogicalUnit::OnDismountFailed()
{
	CAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LogicalUnit=%d, dismount failed.\n", m_NdasLogicalUnitId);

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == m_status) 
	{
		pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
	}

}

struct NdasDeviceIsVolatile : public std::unary_function<INdasDevice*,bool>
{
	result_type operator()(argument_type pNdasDevice) const
	{
		DWORD regFlags;
		COMVERIFY(pNdasDevice->get_RegisterFlags(&regFlags));
		if (regFlags & NDAS_DEVICE_REG_FLAG_VOLATILE)
		{
			return true;
		}
		return false;
	}
};
struct NdasUnitIsVolatile : public std::unary_function<INdasUnit*,bool>
{
	result_type operator()(argument_type pNdasUnit) const
	{
		CComPtr<INdasDevice> pNdasDevice;
		COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
		return NdasDeviceIsVolatile()(pNdasDevice);
	}
};

BOOL
CNdasLogicalUnit::pIsVolatile()
{
	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);

	// If any single device is volatile, then it's volatile.
	size_t index = AtlFindIf(ndasUnits, NdasUnitIsVolatile());
	if (ndasUnits.GetCount() == index)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL 
CNdasLogicalUnit::pGetLastLogicalUnitPlugInFlags(DWORD& flags, DWORD& values)
{
	DWORD data[2] = {0};
	BOOL success = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("LdpfData"),
		data,
		sizeof(data));
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, GetLastLdpf failed, error=0x%X\n", 
			m_NdasLogicalUnitId, GetLastError());
		return FALSE;
	}
	flags = data[0];
	values = data[1];
	return TRUE;
}

BOOL 
CNdasLogicalUnit::pSetLastLogicalUnitPlugInFlags(DWORD flags, DWORD values)
{
	DWORD data[2] = {flags, values};
	BOOL success = _NdasSystemCfg.SetValueEx(
		m_szRegContainer,
		_T("LdpfData"),
		REG_BINARY,
		data,
		sizeof(data));
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, SetLastLdpf failed, flags=%08X, value=%08X, error=0x%X\n", 
			m_NdasLogicalUnitId, flags, values, GetLastError());

		return FALSE;
	}
	return TRUE;
}

BOOL
CNdasLogicalUnit::pClearLastLogicalUnitPlugInFlags()
{
	BOOL success = _NdasSystemCfg.DeleteValue(
		m_szRegContainer, 
		_T("LdpfData"));
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, ClearLastLdpf failed, error=0x%X\n", 
			m_NdasLogicalUnitId, GetLastError());
		return FALSE;
	}
	return TRUE;
}

STDMETHODIMP 
CNdasLogicalUnit::get_ContentEncryption(
	__out NDAS_CONTENT_ENCRYPT* Encryption)
{
	ZeroMemory(Encryption, sizeof(NDAS_CONTENT_ENCRYPT));

	CComPtr<INdasUnit> pPrimaryNdasUnit;
	HRESULT hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);
	if (FAILED(hr))
	{
		return S_FALSE;
	}

	NDAS_UNITDEVICE_TYPE unitType;
	COMVERIFY(pPrimaryNdasUnit->get_Type(&unitType));

	if (NDAS_UNITDEVICE_TYPE_DISK != unitType)
	{
		return S_FALSE;
	}

	CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pPrimaryNdasUnit);
	ATLASSERT(pNdasDiskUnit.p);

	return pNdasDiskUnit->get_ContentEncryption(Encryption);
}

STDMETHODIMP 
CNdasLogicalUnit::get_IsHidden(__out BOOL * Hidden)
{
	CAutoLock autolock(this);

	*Hidden = FALSE;

	//
	// If the first unit device of the logical device is not registered,
	// it is assumed that the device of the unit device is NOT hidden.
	// This happens when RAID members are partially registered,
	// and the first one is not registered.
	//
	CComPtr<INdasUnit> pNdasUnit;
	HRESULT hr = pGetPrimaryNdasUnit(&pNdasUnit);
	if (FAILED(hr))
	{
		*Hidden = FALSE;
		return S_FALSE;
	}

	CComPtr<INdasDevice> pNdasDevice;
	COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
	ATLASSERT(pNdasDevice);

	DWORD regFlags;
	COMVERIFY(pNdasDevice->get_RegisterFlags(&regFlags));

	BOOL hidden = regFlags & NDAS_DEVICE_REG_FLAG_HIDDEN ? TRUE : FALSE;

	*Hidden = hidden;

	return S_OK;
}

STDMETHODIMP_(void)
CNdasLogicalUnit::OnTimer()
{
	CAutoLock autolock(this);

	NDAS_LOGICALDEVICE_STATUS status = m_status;
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

	DWORD mountTick = pGetMountTick();
	if (pIsRiskyMount() && 0 != mountTick)
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

	if (IsNdasPortMode())
	{
		NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = 
			pGetNdasLogicalUnitAddress();

		HRESULT hr = NdasPortCtlQueryNodeAlive(ndasLogicalUnitAddress);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasPortCtlQueryNodeAlive failed, LogicalUnitAddress=%08X, hr=0x%X\n", 
				m_NdasLocation, hr);

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Reset to Unmounted\n");

			OnDismounted();

			return;
		}
	}
	else
	{
		NDAS_LOCATION location = m_NdasLocation;
		XTLASSERT(0 != location);

		BOOL fAlive, fAdapterError;
		BOOL success;

		success = ::NdasBusCtlQueryNodeAlive(
		    location, 
		    &fAlive, 
		    &fAdapterError);
		//
		// if NdasBusCtlQueryNodeAlive fails, 
		// there may be no NDAS SCSI device instance...
		//

		if (!success) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlQueryNodeAlive failed, SlotNo=%08X, error=0x%X\n", 
				m_NdasLocation, GetLastError());
			return;
		}

		if (!fAlive) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
				"LogicalUnit=%d, instance does not exist anymore.\n", 
				m_NdasLogicalUnitId);
			OnDismounted();
			//_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
		}
		
		//if (fAdapterError) {
		//	XTLTRACE_ERR("NdasBusCtlQueryNodeAlive reported an adapter error.\n"));
		//	pNdasLogicalUnit->_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
		//}
    } 	
}

BOOL
CNdasLogicalUnit::pIsUpgradeRequired()
{
	// RAID1, RAID1R2 and RAID4,RAID4R2 is replaced with RAID1_R3 and RAID4_R3
	// And the underlying device driver does not handle RAID1 and RAID4
	// So we have to make these types as NOT_MOUNTABLE and REQUIRE_UPGRADE
	// Error.
	NDAS_LOGICALDEVICE_TYPE type = m_NdasLogicalUnitDefinition.Type;
	switch (type)
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
CNdasLogicalUnit::pIsRaid()
{
	NDAS_LOGICALDEVICE_TYPE type = m_NdasLogicalUnitDefinition.Type;
	return IS_NDAS_LOGICALDEVICE_TYPE_DISK_SET_GROUP(type);
}

void 
CNdasLogicalUnit::pGetNdasUnitInstances(CInterfaceArray<INdasUnit>& NdasUnits)
{
	NdasUnits.RemoveAll();
	CAutoLock autolock(this);
	size_t count = m_NdasUnits.GetCount();
	for (size_t i = 0; i < count; ++i)
	{
		INdasUnit* pNdasUnit = m_NdasUnits.GetAt(i);
		NdasUnits.Add(pNdasUnit);
	}
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

VOID
LogicalDeviceTypeToRootLurnType(
	__in NDAS_LOGICALDEVICE_TYPE LogicalDeviceType,
	__out PLURN_TYPE	LurnType,
	__out PUCHAR		LurnDeviceInterface)
{
	LURN_TYPE	lurnType;
	CHAR		lurnDeviceInterface;


	switch (LogicalDeviceType) 
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		lurnType = LURN_DIRECT_ACCESS;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATA;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		lurnType = LURN_MIRRORING;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		lurnType = LURN_AGGREGATION;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		lurnType = LURN_RAID0;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
		lurnType = LURN_RAID1R;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
		lurnType = LURN_RAID4R;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:
		lurnType = LURN_RAID5;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DVD:
		lurnType = LURN_CDROM;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_LOGICALDEVICE_TYPE_MO:
		lurnType = LURN_OPTICAL_MEMORY;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
	case NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB:
	default:
		XTLASSERT(FALSE);
		lurnType = LURN_NULL;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
	}

	if (LurnType)
		*LurnType = lurnType;
	if (LurnDeviceInterface)
		*LurnDeviceInterface = lurnDeviceInterface;

}


VOID
UnitDeviceTypeToLeafLurnType(
	__in NDAS_UNIT_TYPE UnitDeviceType,
	__out PLURN_TYPE	LurnType,
	__out PUCHAR 		LurnDeviceInterface)
{
	LURN_TYPE	lurnType;
	CHAR		lurnDeviceInterface;


	switch (UnitDeviceType) 
	{
	case NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE:
		lurnType = LURN_DIRECT_ACCESS;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATA;
		break;
	case NDAS_UNIT_ATAPI_DIRECT_ACCESS_DEVICE:
		lurnType = LURN_DIRECT_ACCESS;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_SEQUENTIAL_ACCESS_DEVICE:
		lurnType = LURN_SEQUENTIAL_ACCESS;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_PRINTER_DEVICE:
		lurnType = LURN_PRINTER;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_PROCESSOR_DEVICE:
		lurnType = LURN_PROCCESSOR;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_WRITE_ONCE_DEVICE:
		lurnType = LURN_WRITE_ONCE;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_CDROM_DEVICE:
		lurnType = LURN_CDROM;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_SCANNER_DEVICE:
		lurnType = LURN_SCANNER;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE:
		lurnType = LURN_OPTICAL_MEMORY;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_MEDIUM_CHANGER_DEVICE:
		lurnType = LURN_MEDIUM_CHANGER;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_COMMUNICATIONS_DEVICE:
		lurnType = LURN_COMMUNICATIONS;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_ARRAY_CONTROLLER_DEVICE:
		lurnType = LURN_ARRAY_CONTROLLER;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_ENCLOSURE_SERVICES_DEVICE:
		lurnType = LURN_ENCLOSURE_SERVICES;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_REDUCED_BLOCK_COMMAND_DEVICE:
		lurnType = LURN_REDUCED_BLOCK_COMMAND;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_ATAPI_OPTICAL_CARD_READER_WRITER_DEVICE:
		lurnType = LURN_OPTIOCAL_CARD;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
	case NDAS_UNIT_UNKNOWN_DEVICE:
	default:
		XTLASSERT(FALSE);
		lurnType = LURN_NULL;
		lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
	}

	if (LurnType)
		*LurnType = lurnType;
	if (LurnDeviceInterface)
		*LurnDeviceInterface = lurnDeviceInterface;

}

}
// anonymous namespace
