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
#include <ndas/ndasctype.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasvolex.h>
#include <ndas/ndasportctl.h>
#include <ndas/ndasop.h>
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

LONG DbgLevelSvcLogDev = DBG_LEVEL_SVC_LOGDEV;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelSvcLogDev) {							\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

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

struct VerifyNdasUnit : public std::unary_function<INdasUnit*, void>
{
	HRESULT m_hr;
	VerifyNdasUnit() : m_hr(S_OK) {}
	void operator()(INdasUnit* pNdasUnit)
	{
		if (NULL == pNdasUnit) return;
		HRESULT hr = pNdasUnit->VerifyNdasLogicalUnitDefinition();
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"VerifyNdasLogicalUnitDefinition failed, NdasUnit=%p, hr=0x%X\n", 
				pNdasUnit, hr);
			m_hr = hr;
		}
	}
	HRESULT GetResult() { return m_hr; }
};

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
	m_fShutdown(FALSE),
	m_ulAdapterStatus(NDASSCSI_ADAPTER_STATUS_INIT),
	m_RaidFailReason(NDAS_RAID_FAIL_REASON_NONE),
	m_Abnormalities(NDAS_LOGICALUNIT_ABNORM_NONE),
	m_PendingReconcilation(FALSE)
{
}

HRESULT 
CNdasLogicalUnit::Initialize(
	__in NDAS_LOGICALDEVICE_ID LogicalUnitId, 
	__in const NDAS_LOGICALUNIT_DEFINITION& LogicalUnitDef,
	__in BSTR RegistrySubPath)
{
	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LogicalUnitId=%d\n", LogicalUnitId);

	m_NdasLogicalUnitId = LogicalUnitId;
	m_NdasLogicalUnitDefinition = LogicalUnitDef;

	m_RegistrySubPath.AssignBSTR(RegistrySubPath);

	//
	// Initially, m_NdasUnits contains (NdasUnitCount) null pointers
	//

	m_NdasUnits.SetCount(m_NdasLogicalUnitDefinition.DiskCount+m_NdasLogicalUnitDefinition.SpareCount);

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
	__out NDAS_LOGICALUNIT_DEFINITION* LogicalUnitDefinition)
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
	return m_SystemDevicePath.CopyTo(DevicePath);
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
	*UnitCount = m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount;
	return S_OK;
}

STDMETHODIMP 
CNdasLogicalUnit::get_NdasUnitId(__in DWORD Sequence, __out NDAS_UNITDEVICE_ID* NdasUnitId)
{
	if (Sequence >= m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount) {

		return E_INVALIDARG;
	}
	
	NdasUnitId->DeviceId = m_NdasLogicalUnitDefinition.NdasChildDeviceId[Sequence];
	
	if (m_NdasUnits.GetAt(Sequence)) {
		
		m_NdasUnits.GetAt(Sequence)->get_UnitNo(&NdasUnitId->UnitNo);
	
	} else {
		
		NdasUnitId->UnitNo = -1;
	}

	return S_OK;
}

DWORD 
CNdasLogicalUnit::pGetUnitDeviceCount() const
{
	return m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount;
}

DWORD 
CNdasLogicalUnit::pGetUnitDeviceCountSpare() const
{
	return m_NdasLogicalUnitDefinition.SpareCount;
}

DWORD 
CNdasLogicalUnit::pGetUnitDeviceCountInRaid() const
{
	return pGetUnitDeviceCount() - pGetUnitDeviceCountSpare();
}

const NDAS_LOGICALUNIT_DEFINITION&
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

#if 0
BOOL
CNdasLogicalUnit::pUpdateBindStateAndError()
{
	NDAS_RAID_FAIL_REASON PrevFailReason = m_RaidFailReason;
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
#endif

#if 0
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

		if (NDAS_DEVICE_STATUS_NOT_REGISTERED == status) 
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
#endif

//
// Set the unit device ID at a sequence 
// to a unit device member ID list
//
STDMETHODIMP
CNdasLogicalUnit::AddNdasUnitInstance(INdasUnit* pNdasUnit)
{
	CAutoLock autolock(this);

	HRESULT hr;
	BOOL	success;

	NDAS_RAID_MOUNTABILITY_FLAGS RaidMountablity;
	NDAS_RAID_FAIL_REASON		 RaidFailReason;

	NDAS_LOGICALUNIT_DEFINITION	ludef;

	COMVERIFY( pNdasUnit->get_LogicalUnitDefinition(&ludef) );

	XTLASSERT( memcmp(&ludef, &m_NdasLogicalUnitDefinition, sizeof(NDAS_LOGICALUNIT_DEFINITION)) == 0 );

	DWORD luseq;

	COMVERIFY(pNdasUnit->get_LogicalUnitSequence(&luseq));

	if (luseq >= m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount) {

		XTLASSERT(FALSE);

		XTLTRACE2( NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				   "Invalid unit device sequence, seq=%d, count=%d\n", 
				   luseq, m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount );

		return E_INVALIDARG;
	}

	//
	// Check for multiple add unit device calls
	//
	INdasUnit* pExistingNdasUnit = m_NdasUnits.GetAt(luseq);
	if (NULL != pExistingNdasUnit)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Duplicate unit device sequence, seq=%d\n", luseq); 
		XTLASSERT(FALSE && "Duplicate calls to AddUnitDevice");
		return E_FAIL;
	}

	XTLASSERT(NULL == pExistingNdasUnit);
	m_NdasUnits.SetAt(luseq, pNdasUnit);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Added Sequence %d to Logical Unit %d\n", 
		luseq, 
		m_NdasLogicalUnitId);

	m_RaidFailReason = NDAS_RAID_FAIL_REASON_NONE;

	NDAS_LOGICALDEVICE_ERROR PrevLastError = m_lastError;

	if (pIsComplete())
	{	
		//
		// Reconcile with the storage port driver if already mounted
		//

		BOOL activeLogicalUnit = FALSE;

		if (IsNdasPortMode()) 
		{
			NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress = 
				NdasLocationToLogicalUnitAddress(m_NdasLogicalUnitId);

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
				m_NdasLogicalUnitId, 
				&bAlive, 
				&bAdapterError);

			if (success && bAlive)
			{
				activeLogicalUnit = TRUE;
				pReconcileWithNdasBus();
			}
		}

		//
		// If not mounted and the logical unit should be mounted
		// on ready, we should proceed to mount this unit now
		//

		if (!activeLogicalUnit && m_fMountOnReady) 
		{
			ACCESS_MASK allowedAccess;
			COMVERIFY(get_AllowedAccess(&allowedAccess));

			//
			// Try to mount only in not degraded mode.
			// (RAID can be in degraded mode even if the RAID member is complete, 
			// in case that disk has same DIB, different RAID set ID)
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

				//
				// Mount-on-ready does ignore the warnings for mount
				//

				hr = PlugIn(
					m_mountOnReadyAccess, 
					LdpfFlags, LdpfValues, 
					NDAS_LOGICALUNIT_PLUGIN_FLAGS_IGNORE_WARNINGS);

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

				//
				// Mount-on-ready does ignore the warnings for mount
				//

				hr = PlugIn(
					GENERIC_READ, 
					LdpfFlags, 
					LdpfValues,
					NDAS_LOGICALUNIT_PLUGIN_FLAGS_IGNORE_WARNINGS);

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
	}
	else
	{
		//
		// NdasUnits instances are incomplete 
		//
	}

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

	if (luseq >= m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount) {

		XTLASSERT(FALSE);

		XTLTRACE2( NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				   "Invalid unit device sequence, seq=%d, count=%d\n", 
				   luseq, m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount );

		return E_INVALIDARG;
	}

	CComPtr<INdasUnit> pExistingNdasUnit = m_NdasUnits.GetAt(luseq);

	if (!pExistingNdasUnit)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasUnit in sequence (%d) is not occupied.\n", luseq);
		XTLASSERT(FALSE && "RemoveUnitDevice called for non-registered");
		return E_FAIL;
	}

	if (pExistingNdasUnit != pNdasUnit)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasUnit in sequence (%d) is different, NdasUnit=%p, Existing=%p.\n", 
			luseq, pNdasUnit, pExistingNdasUnit.p);
		XTLASSERT(FALSE && "RemoveUnitDevice called for non-registered");
		return E_FAIL;
	}

	//
	// Clear the ndas unit instance
	//

	m_NdasUnits.SetAt(luseq, NULL);

#if 0
	//
	// Set Device Error
	//
	if (pIsMountable()) 
	{
		//
		// If conflict source is removed , RAID can be mounted when device is removed.
		// 
		if (0 == m_NdasLogicalUnitId) 
		{
			CComPtr<INdasLogicalUnitManagerInternal> pManager;
			COMVERIFY(hr = pGetNdasLogicalUnitManagerInternal(&pManager));
			// Allocate NdasLocation when mountable.
			pAllocateNdasLocation();
			hr = pManager->RegisterNdasLocation(m_NdasLogicalUnitId, this);
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
#endif

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
	DWORD instances = 0;
	size_t count = m_NdasUnits.GetCount();
	for (size_t i = 0; i < count; ++i)
	{
		if (NULL != m_NdasUnits.GetAt(i))
		{
			++instances;
		}
	}
	*UnitCount = instances;
	return S_OK;
}

BOOL
CNdasLogicalUnit::pIsComplete()
{
	DWORD activeCount = 0;
	for (DWORD i = 0; i < m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount; ++i)
	{
		if (m_NdasLogicalUnitDefinition.ActiveNdasUnits[i])
		{
			++activeCount;
		}
	}
	DWORD instanceCount = 0;
	COMVERIFY(get_NdasUnitInstanceCount(&instanceCount));

	if (instanceCount != activeCount)
	{
		return FALSE;
	}

	return TRUE;
}

struct NotifyNdasUnitMountCompleted : std::unary_function<INdasUnit*, void>
{
	void operator()(INdasUnit* pNdasUnit)
	{
		if (NULL == pNdasUnit) return;
		CComQIPtr<INdasUnitPnpSink> pSink(pNdasUnit);
		ATLASSERT(pSink.p);
		pSink->MountCompleted();
	};
};

struct NotifyNdasUnitDismountCompleted : std::unary_function<INdasUnit*, void>
{
	void operator()(INdasUnit* pNdasUnit)
	{
		if (NULL == pNdasUnit) return;
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

	if (Seq >= m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount) {

		XTLASSERT(FALSE);

		return E_INVALIDARG;
	}

	CComPtr<INdasUnit> pExistingNdasUnit = m_NdasUnits.GetAt(Seq);

	if (!pExistingNdasUnit)
	{
		return E_FAIL;
	}

	*ppNdasUnit = pExistingNdasUnit.Detach();

	return S_OK;
}

HRESULT 
CNdasLogicalUnit::pGetPrimaryNdasUnit(INdasUnit** ppNdasUnit)
{
	CAutoLock autolock(this);

	*ppNdasUnit = 0;

	size_t count = m_NdasUnits.GetCount();
	for (size_t i = 0; i < count; ++i)
	{
		CComPtr<INdasUnit> pNdasUnit = m_NdasUnits.GetAt(i);
		if (pNdasUnit)
		{
			*ppNdasUnit = pNdasUnit.Detach();
			return S_OK;
		}
	}

	return E_FAIL;
}

struct SumGrantedAccess : public std::unary_function<INdasUnit*,void>
{
	ACCESS_MASK AccessMask;
	SumGrantedAccess() : AccessMask(0xFFFFFFFFL) {}
	result_type operator()(INdasUnit* pNdasUnit)
	{
		if (NULL == pNdasUnit) return;
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
		if (NULL == pNdasUnit) return;
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

	SumGrantedAccess sum = AtlForEach(ndasUnits, SumGrantedAccess());

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

	SumAllowedAccess sum = AtlForEach(ndasUnits, SumAllowedAccess());

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
		m_RegistrySubPath,
		_T("RiskyMountFlag"),
		&fRisky);

	return fRisky;
}

STDMETHODIMP 
CNdasLogicalUnit::SetRiskyMountFlag(__in BOOL RiskyState)
{
	CAutoLock autolock(this);

	BOOL success = _NdasSystemCfg.SetValueEx(
		m_RegistrySubPath,
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
		m_RegistrySubPath, 
		_T("MountMask"), 
		(DWORD) mountedAccess);
}

ACCESS_MASK
CNdasLogicalUnit::pGetLastMountAccess()
{
	CAutoLock autolock(this);

	ACCESS_MASK mountMask = 0;

	BOOL success = _NdasSystemCfg.GetValueEx(
		m_RegistrySubPath,
		_T("MountMask"),
		(LPDWORD)&mountMask);

	if (!success) 
	{
		return 0;
	}

	return mountMask;
}

BOOL
CNdasLogicalUnit::pIsSharedWriteModeCapable()
{
	BOOL fNoPSWriteShare = NdasServiceConfig::Get(nscDontUseWriteShare);

	// logical device specific option
	BOOL success = _NdasSystemCfg.GetValueEx(
		m_RegistrySubPath,
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
CNdasLogicalUnit::pIsWriteAccessAllowed (
	BOOL		fPSWriteShare,
	INdasUnit	*pNdasUnit
	)
{
	if (pNdasUnit == NULL) {

		ATLASSERT(FALSE);
		return E_FAIL;
	}

	DWORD nROHosts, nRWHosts;

	HRESULT hr = pNdasUnit->GetHostUsageCount( &nROHosts, &nRWHosts, TRUE );
	
	if (FAILED(hr)) {

		ATLASSERT(FALSE);
		return hr;
	}

	if (nRWHosts > 0) {

		if (!fPSWriteShare) {

			return NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED;
		
		} else {

			HRESULT hr = pNdasUnit->CheckNdasfsCompatibility();

			if (FAILED(hr)) {

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
		NDAS_LDPF_SIMULTANEOUS_WRITE,   LUROPTION_ON_SIMULTANEOUS_WRITE,	LUROPTION_OFF_SIMULTANEOUS_WRITE,
		NDAS_LDPF_OUTOFBOUND_WRITE,     LUROPTION_ON_OOB_WRITE,				LUROPTION_OFF_OOB_WRITE,
		NDAS_LDPF_NDAS_2_0_WRITE_CHECK, LUROPTION_ON_NDAS_2_0_WRITE_CHECK,	LUROPTION_OFF_NDAS_2_0_WRITE_CHECK,
		NDAS_LDPF_DYNAMIC_REQUEST_SIZE, LUROPTION_ON_DYNAMIC_REQUEST_SIZE,	LUROPTION_OFF_DYNAMIC_REQUEST_SIZE,
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
	NdasUiDbgCall( 2, "in\n" );

	CAutoLock autolock(this);
	HRESULT hr;

	BOOL fPSWriteShare = pIsSharedWriteModeCapable();

	hr = pIsSafeToPlugIn(requestingAccess);
	if (FAILED(hr)) 
	{
		//
		// Force pdate unit device info.
		// 
		//if (hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION ||
		//	hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING ||
		//	hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_FIX_REQUIRED) 
		//{
			//
			// If bind information is changed, make it reload.
			//
			autolock.Release();
			pInvalidateNdasUnits();
		//}
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

#if 0
	DWORD addTargetDataSize = 
		cbAddTargetDataSizeWithoutBACL + blockAclSize;
#else
	DWORD	addTargetDataSize = sizeof(NDASBUS_ADD_TARGET_DATA);
	XTLASSERT( blockAclSize <= BACL_BUFFER_SIZE );
#endif

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
	addTargetData->ulSlotNo = m_NdasLogicalUnitId;
	addTargetData->ulTargetBlocks = 0; // initialization and will be added
	addTargetData->DeviceMode = deviceMode;
	addTargetData->ulNumberOfUnitDiskList = pGetUnitDeviceCount();
	addTargetData->ucTargetType = targetType;
	addTargetData->LockImpossible = (BOOLEAN)(m_NdasLogicalUnitDefinition.NotLockable);
	addTargetData->StartOffset = (UINT32)m_NdasLogicalUnitDefinition.StartOffset;
	addTargetData->EmergencyMode = (BOOLEAN)pIsEmergencyMode();


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
#if 0
		addTargetData->BACLOffset = cbAddTargetDataSizeWithoutBACL;
#else
		addTargetData->BACLOffset = FIELD_OFFSET(NDASBUS_ADD_TARGET_DATA, BaclBuffer);
#endif
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

	for (DWORD i = 0; i < m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount; ++i) 
	{	
		CComPtr<INdasDevice> pNdasDevice;
		CComPtr<INdasUnit>	 pNdasUnit;

		HRESULT hr = pGetMemberNdasUnit(i, &pNdasUnit);

		NDAS_UNITDEVICE_ID	ndasUnitId = {0};

		ndasUnitId.DeviceId = m_NdasLogicalUnitDefinition.NdasChildDeviceId[i];

		if (m_NdasUnits.GetAt(i)) {

			m_NdasUnits.GetAt(i)->get_UnitNo(&ndasUnitId.UnitNo);

		} else {

			ATLASSERT(memcmp(ndasUnitId.DeviceId.Node, ZeroNode, 6) == 0 );

			ndasUnitId.UnitNo = 0;
		}

		PNDASBUS_UNITDISK pud = &addTargetData->UnitDiskList[i];

		if (pNdasUnit) {

			COMVERIFY( pNdasUnit->get_ParentNdasDevice(&pNdasDevice) );
		}

		if (!pNdasUnit) {

			// Temp fix. pNdasDevice's remote address is not initialized 
			// until it has been online at least one moment.		
			// Use address from LDgroup.

			::CopyMemory( pud->Address.Node, ndasUnitId.DeviceId.Node, sizeof(pud->Address.Node) );
		
		} else {

			XTLASSERT(pNdasDevice);

			SOCKADDR_LPX	sockAddrLpx;
			SOCKET_ADDRESS	socketAddress;

			socketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
			socketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&sockAddrLpx);

			COMVERIFY(pNdasDevice->get_RemoteAddress(&socketAddress));

			::CopyMemory( pud->Address.Node, sockAddrLpx.LpxAddress.Node, sizeof(pud->Address.Node) );
		}

		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);

		if (!pNdasUnit) {

			// Temp fix. pNdasDevice's remote address is not initialized until it has been online at least one moment.		
			
			ZeroMemory(&pud->NICAddr.Node, sizeof(pud->NICAddr.Node));
		
		} else {

			XTLASSERT(pNdasDevice);

			SOCKADDR_LPX	localSockAddrLpx;
			SOCKET_ADDRESS	localSocketAddress;

			localSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
			localSocketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&localSockAddrLpx);

			COMVERIFY(pNdasDevice->get_LocalAddress(&localSocketAddress));

			C_ASSERT( sizeof(pud->NICAddr.Node) == sizeof(localSockAddrLpx.LpxAddress.Node) );

			::CopyMemory( pud->NICAddr.Node, localSockAddrLpx.LpxAddress.Node, sizeof(pud->NICAddr.Node) );
		}
		
		pud->NICAddr.Port = htons(0); // should be zero

		if (!pNdasUnit) {

			// This is missing member. This is temp fix.
			// iUserID and iPassword will not work for future version!!.
			
			pud->iUserID = pGetNdasUserId(ndasUnitId.UnitNo, requestingAccess);

			// We do not know the hardware password at this time

			pud->iPassword = 0;

			//  Assume RAID1, RAID4 and use primary device's

			COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&pud->ulUnitBlocks));

			pud->ulPhysicalBlocks = 0; // Unknown..
			pud->ucUnitNumber = static_cast<UCHAR>(ndasUnitId.UnitNo);

			pud->ucHWType = 0/*HW_TYPE_ASIC*/; 		// Don't know right now..

			// TODO: Use the highest version for missing ndas units

			// it was: pGetLogicalUnitConfig().DeviceHwVersions[i]; // Use hint from DIB

			pud->ucHWVersion = LANSCSIIDE_CURRENT_VERSION; 
			pud->ucHWRevision = 0;	// Don't know right now..
			pud->LurnOptions |= LURNOPTION_MISSING;

		} else {

			XTLASSERT(pNdasDevice);

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

		// Set Reconnect Retry Count, Retry Interval
		// if overridden by the user
		//
		// default:
		// ReconnTrial = 19, ReconnInterval = 3000
		//
		// reddotnet: (will be set by the installer)
		// ReconnTrial = 2, ReconnInterval = 3000

		BOOL fOverride = NdasServiceConfig::Get(nscOverrideReconnectOptions);

		if (fOverride) {

			DWORD dwReconnect		  = NdasServiceConfig::Get(nscLogicalDeviceReconnectRetryLimit);
			DWORD dwReconnectInterval = NdasServiceConfig::Get(nscLogicalDeviceReconnectInterval);

			pud->LurnOptions	|= LURNOPTION_SET_RECONNECTION;
			pud->ReconnTrial	= dwReconnect;
			pud->ReconnInterval = dwReconnectInterval;
		}

		// Get the optimal data send/receive length.
		// TODO: separate send and receive data length.
		//       Get request length in bytes.

		if (!pNdasUnit) {

			// Temp fix: This may not work for some HW such as emulator.
			// Driver also limit Max transfer size if needed.
			pud->UnitMaxDataRecvLength = pReadMaxRequestBlockLimitConfig(pud->ucHWVersion) * 512;
			pud->UnitMaxDataSendLength = pud->UnitMaxDataRecvLength;

		} else {

			DWORD optimalTransferBlocks;
			
			COMVERIFY(pNdasUnit->get_OptimalMaxTransferBlocks(&optimalTransferBlocks));
			pud->UnitMaxDataSendLength = optimalTransferBlocks * 512;
			pud->UnitMaxDataRecvLength = optimalTransferBlocks * 512;
		}
	}

	// Check Multiple Write Access Compatibility

	if (GENERIC_WRITE & requestingAccess) {

		hr = S_OK;

		DWORD dwMaxNDFSCompatCheck = 1;

		if (m_fDisconnected) {

			// On disconnection (other than power failure), 
			// they may exist an inactive R/W connection at the NDAS device. 
			// In such case, no host will reply to NDFS Compatibility Check.
			// As an workaround for that we try NDFS Compatibility Check 
			// more than once if failed.
			
			dwMaxNDFSCompatCheck = NdasServiceConfig::Get(nscWriteAccessCheckLimitOnDisconnect);
		}

		for (DWORD i = 0; i < dwMaxNDFSCompatCheck; ++i) {

			hr = pIsWriteAccessAllowed(fPSWriteShare, pPrimaryNdasUnit);
			
			if (SUCCEEDED(hr)) {

				break;
			}
		}

		if (FAILED(hr)) {

			return hr;
		}
	}
		
	// After this, we used up a Disconnected flag, so we can clear it.

	m_fDisconnected = FALSE;

	UINT64 userBlocks;
	COMVERIFY(get_UserBlocks(&userBlocks));

	addTargetData->ulTargetBlocks = userBlocks;

	//	We don't need an algorithm for a SCSI adapter's max request blocks
	//	We just need one more registry key to override.
	//	We set 0 to use driver's default max request blocks.
	//
	//	Use driver's default value

	DWORD dwMaxRequestBlocks = 0;

	NdasUiDbgCall( 2, _T("NdasBusCtlPlugInEx2, SlotNo=%08X, MaxReqBlock=%d, DisEvt=%p, RecEvt=%p\n"),
				   m_NdasLogicalUnitId, dwMaxRequestBlocks, m_hDisconnectedEvent, m_hAlarmEvent );

	BOOL fVolatileRegister = pIsVolatile();
	
	BOOL success = NdasBusCtlPlugInEx2( m_NdasLogicalUnitId,
										dwMaxRequestBlocks,
										addTargetData,
										m_hDisconnectedEvent,
										m_hAlarmEvent,
										fVolatileRegister );

	if (!success) {

		hr = HRESULT_FROM_WIN32(GetLastError());

		NdasUiDbgCall( 1, _T("NdasBusCtlPlugInEx2 failed, hr=0x%X\n"), hr );

		return hr;
	}

	NdasUiDbgCall( 2, "completed successfully, SlotNo=%08X\n", m_NdasLogicalUnitId );

	// Set the status

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

	NdasUiDbgCall( 2, "completed successfully, SlotNo=%08X\n", m_NdasLogicalUnitId );

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

	BOOL fPSWriteShare = pIsSharedWriteModeCapable();

	hr = pIsSafeToPlugIn(requestingAccess);
	if (FAILED(hr)) 
	{
		//
		// Force to update unit device info.
		// 
		//if (hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION ||
		//	hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING ||
		//	hr == NDASSVC_ERROR_NDAS_LOGICALDEVICE_FIX_REQUIRED) 
		//{
			//
			// If bind information is changed, make it reload.
			//
			autolock.Release();
			pInvalidateNdasUnits();
		//}
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
	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress;

	LURN_TYPE nodeType;
	UCHAR lurnDeviceInterface;

	LogicalDeviceTypeToRootLurnType(
		m_NdasLogicalUnitDefinition.Type, 
		&nodeType, 
		&lurnDeviceInterface);

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

	ndasLogicalUnitAddress = NdasLocationToLogicalUnitAddress(m_NdasLogicalUnitId);

	BOOL rootNodeExists  = LurnTypeTable[nodeType].CanHaveChildren;

	NDASPORTCTL_NODE_INITDATA rootNodeInitData;
	rootNodeInitData.NodeType = nodeType;
	rootNodeInitData.StartLogicalBlockAddress.QuadPart = 0;

	//
	// TODO: replace more flexible method to get interface type
	//   according to the logical unit type.
	//
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
		CComQIPtr<INdasDiskUnit> pPrimaryNdasDiskUnit(pPrimaryNdasUnit);
		ATLASSERT(pPrimaryNdasDiskUnit.p);

		PNDAS_RAID_INFO	infoRaid = NULL;
		COMVERIFY(pPrimaryNdasDiskUnit->get_RaidInfo(reinterpret_cast<PVOID*>(&infoRaid)));

		XTLASSERT(NULL != infoRaid);

		if (0 == infoRaid->BlocksPerBit) 
		{
			hr = NDASSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION;
			return hr;
		}

		rootNodeInitData.NodeSpecificData.Raid.BlocksPerBit = infoRaid->BlocksPerBit;
		rootNodeInitData.NodeSpecificData.Raid.SpareDiskCount = infoRaid->SpareDiskCount;

		CopyMemory(
			&rootNodeInitData.NodeSpecificData.Raid.NdasRaidId,
			&infoRaid->NdasRaidId, 
			sizeof(GUID));
		
		CopyMemory(
			&rootNodeInitData.NodeSpecificData.Raid.ConfigSetId,
			&infoRaid->ConfigSetId, 
			sizeof(GUID));
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

	lurDesc->LockImpossible = (BOOLEAN)(m_NdasLogicalUnitDefinition.NotLockable);
	lurDesc->StartOffset = (UINT32)m_NdasLogicalUnitDefinition.StartOffset;
	lurDesc->EmergencyMode = (BOOLEAN)pIsEmergencyMode();

	//
	// Set Content Encryption from the primary unit device
	// (Only for Disk Devices)
	//

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

	for (DWORD i = 0; i < m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount; ++i) 
	{	
		ULONG lurnIndex = i + (rootNodeExists?1:0);

		CComPtr<INdasUnit> pNdasUnit;
		pGetMemberNdasUnit(i, &pNdasUnit);

		NDAS_UNITDEVICE_ID ndasUnitId;

		ndasUnitId.DeviceId = m_NdasLogicalUnitDefinition.NdasChildDeviceId[i];

		if (m_NdasUnits.GetAt(i)) {

			m_NdasUnits.GetAt(i)->get_UnitNo(&ndasUnitId.UnitNo);

		} else {

			ndasUnitId.UnitNo = -1;
		}

		PLURELATION_NODE_DESC lurNode = NdasPortCtlFindNodeDesc(logicalUnitDescriptor, lurnIndex);

		NDASPORTCTL_NODE_INITDATA nodeConfig;

		PNDASPORTCTL_INIT_ATADEV ataNodeConfig = &nodeConfig.NodeSpecificData.Ata;

		ataNodeConfig->ValidFieldMask =
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
			nodeConfig.NodeType = LURN_DIRECT_ACCESS;

			//
			// We don't know the device interface type, so put in
			// LURN virtual interface.
			// TODO: Should specify device interface here.
			//

			nodeConfig.NodeDeviceInterface = LURN_DEVICE_INTERFACE_LURN;

			nodeConfig.StartLogicalBlockAddress.QuadPart = 0;

			//
			//  Assume RAID1, RAID4 and use primary device's
			//

			UINT64 userBlocks;
			COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&userBlocks));
			nodeConfig.EndLogicalBlockAddress.QuadPart = 
				userBlocks - 1; 

			//
			// Temp fix. pNdasDevice's remote address is not initialized 
			// until it has been online at least one moment.		
			// Use address from LDgroup.
			//

			::CopyMemory(
				ataNodeConfig->DeviceIdentifier.Identifier, 
				ndasUnitId.DeviceId.Node,
				sizeof(ataNodeConfig->DeviceIdentifier.Identifier));

			//
			// Temp fix. pNdasDevice's remote address is not initialized 
			// until it has been online at least one moment.
			//

			ZeroMemory(&ataNodeConfig->BindingAddress, sizeof(TA_NDAS_ADDRESS));
			ataNodeConfig->DeviceIdentifier.UnitNumber = static_cast<UCHAR>(ndasUnitId.UnitNo);
			ataNodeConfig->HardwareType = 0; 	/*HW_TYPE_ASIC*/	// Don't know right now..
			//
			// TODO: Use the highest version for missing ndas units
			//
			// it was: pGetLogicalUnitConfig().DeviceHwVersions[i]; // Use hint from DIB
			//
			ataNodeConfig->HardwareVersion = LANSCSIIDE_CURRENT_VERSION; 
			ataNodeConfig->HardwareRevision = 0;	// Don't know right now..

			ataNodeConfig->TransportPortNumber = NDAS_DEVICE_LPX_PORT;

			// lurNode->ulPhysicalBlocks = 0; // Unknown..
			ataNodeConfig->UserId = pGetNdasUserId(ndasUnitId.UnitNo, requestingAccess);
			ZeroMemory(ataNodeConfig->UserPassword, NDASPORTCTL_USERPASSWORD_LENGTH);

			CopyMemory(
				ataNodeConfig->DeviceOemCode, 
				&NDAS_OEM_CODE_DEFAULT, 
				NDASPORTCTL_OEMCODE_LENGTH);
		} 
		else 
		{
			CComPtr<INdasDevice> pNdasDevice;

			COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));

			NDAS_UNITDEVICE_HARDWARE_INFO unitdevHwInfo;
			pNdasUnit->get_HardwareInfo(&unitdevHwInfo);

			UnitDeviceTypeToLeafLurnType(
				static_cast<NDAS_UNIT_TYPE>(unitdevHwInfo.MediaType),
				&nodeConfig.NodeType,
				&nodeConfig.NodeDeviceInterface);

			UINT64 userBlocks;
			COMVERIFY(pNdasUnit->get_UserBlocks(&userBlocks));

			nodeConfig.StartLogicalBlockAddress.QuadPart = 0;
			nodeConfig.EndLogicalBlockAddress.QuadPart = userBlocks - 1;

			NDAS_DEVICE_ID ndasDeviceId;
			COMVERIFY(pNdasDevice->get_NdasDeviceId(&ndasDeviceId));

			::CopyMemory(
				ataNodeConfig->DeviceIdentifier.Identifier, 
				ndasDeviceId.Node,
				sizeof(ataNodeConfig->DeviceIdentifier.Identifier));

			SOCKADDR_LPX localSockAddrLpx;
			SOCKET_ADDRESS localSocketAddress;
			localSocketAddress.iSockaddrLength = sizeof(SOCKADDR_LPX);
			localSocketAddress.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&localSockAddrLpx);

			COMVERIFY(pNdasDevice->get_LocalAddress(&localSocketAddress));

			LpxCommConvertLpxAddressToTaLsTransAddress(
				&localSockAddrLpx.LpxAddress,
				&ataNodeConfig->BindingAddress);

			DWORD unitNo;

			pNdasUnit->get_UnitNo(&unitNo);

			ataNodeConfig->DeviceIdentifier.UnitNumber = static_cast<UCHAR>(unitNo);

			DWORD type, version, revision;

			pNdasDevice->get_HardwareType(&type);
			pNdasDevice->get_HardwareVersion(&version);
			pNdasDevice->get_HardwareRevision(&revision);

			ataNodeConfig->HardwareType = static_cast<UCHAR>(type);
			ataNodeConfig->HardwareVersion = static_cast<UCHAR>(version);
			ataNodeConfig->HardwareRevision = static_cast<UCHAR>(revision);	
			
			ataNodeConfig->TransportPortNumber = NDAS_DEVICE_LPX_PORT;

			DWORD userId;
			pNdasUnit->get_NdasDeviceUserId(requestingAccess, &userId);

			ataNodeConfig->UserId = userId;

			ZeroMemory(ataNodeConfig->UserPassword, NDASPORTCTL_USERPASSWORD_LENGTH);

			UINT64 ndasUnitOemCode;

			pNdasUnit->get_NdasDevicePassword(&ndasUnitOemCode);

			CopyMemory(
				ataNodeConfig->DeviceOemCode, 
				&ndasUnitOemCode, 
				NDASPORTCTL_OEMCODE_LENGTH);
		}

		if ( NdasPortCtlSetupLurNode(lurNode, deviceMode, &nodeConfig) == FALSE ) 
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
				nodeConfig.NodeSpecificData.Ata.HardwareVersion) * 512;
			lurNode->MaxDataSendLength = lurNode->MaxDataRecvLength;
		}
		else 
		{
			DWORD optimalMaxTransferBlocks;
			COMVERIFY(pNdasUnit->get_OptimalMaxTransferBlocks(&optimalMaxTransferBlocks));
			lurNode->MaxDataSendLength = optimalMaxTransferBlocks * 512;
			lurNode->MaxDataRecvLength = optimalMaxTransferBlocks * 512;
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
		m_NdasLogicalUnitId);

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
		m_NdasLogicalUnitId);

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
		m_NdasLogicalUnitId);

	return S_OK;
}

BOOL
CNdasLogicalUnit::pIsEmergencyMode()
{
	switch(m_NdasLogicalUnitDefinition.Type)
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:
		break;
	default:
		return FALSE;
	}

	HRESULT hr;
	DWORD RaidSimpleStatusFlags;
	CComPtr<INdasUnit> pPrimaryNdasUnit;
	
	hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);
	if(FAILED(hr))
		return FALSE;

	UINT8 ndasUnitNoTemp[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];

	for (DWORD i = 0; i < m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount; ++i) {

		if (m_NdasUnits.GetAt(i)) {

			DWORD unitNo;

			m_NdasUnits.GetAt(i)->get_UnitNo(&unitNo);
			ndasUnitNoTemp[i] = (UINT8)unitNo;

		} else {

			ndasUnitNoTemp[i] = -1;
		}
	}

	hr = pPrimaryNdasUnit->GetRaidSimpleStatus( &m_NdasLogicalUnitDefinition, ndasUnitNoTemp, &RaidSimpleStatusFlags );

	if(FAILED(hr))
		return FALSE;

	if (NDAS_RAID_SIMPLE_STATUS_EMERGENCY & RaidSimpleStatusFlags)
		return TRUE;

	return FALSE;
}
								

HRESULT
CNdasLogicalUnit::pCheckRaidStatus(
	DWORD PlugInFlags)
{
	switch(m_NdasLogicalUnitDefinition.Type)
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:
		break;
	default:
		return S_OK;
	}

	if (PlugInFlags & NDAS_LOGICALUNIT_PLUGIN_FLAGS_IGNORE_WARNINGS)
	{
		return S_OK;
	}

	HRESULT hr;
	DWORD RaidSimpleStatusFlags;
	CComPtr<INdasUnit> pPrimaryNdasUnit;

	hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);
	if(FAILED(hr))
		return S_OK;

	UINT8 ndasUnitNoTemp[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];

	for (DWORD i = 0; i < m_NdasLogicalUnitDefinition.DiskCount + m_NdasLogicalUnitDefinition.SpareCount; ++i) {

		if (m_NdasUnits.GetAt(i)) {

			DWORD unitNo;

			m_NdasUnits.GetAt(i)->get_UnitNo(&unitNo);
			ndasUnitNoTemp[i] = (UINT8)unitNo;

		} else {

			ndasUnitNoTemp[i] = -1;
		}
	}

	hr = pPrimaryNdasUnit->GetRaidSimpleStatus( &m_NdasLogicalUnitDefinition, ndasUnitNoTemp, &RaidSimpleStatusFlags );

	if(FAILED(hr))
		return hr;

	if (NDAS_RAID_SIMPLE_STATUS_EMERGENCY & RaidSimpleStatusFlags)
	{
		if (!(PlugInFlags & NDAS_LOGICALUNIT_PLUGIN_FLAGS_IGNORE_WARNINGS))
		{
			// ask emergency mode
			return NDASSVC_ERROR_NDAS_LOGICALUNIT_ASK_EMREGENCY_MOUNT;
		}
		else
		{
			// allow emergency mode
			return S_OK;
		}
	}

	if (NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_REGULAR & RaidSimpleStatusFlags ||
		NDAS_RAID_SIMPLE_STATUS_BAD_DISK_IN_SPARE & RaidSimpleStatusFlags)
	{
		return NDASSVC_ERROR_NDAS_LOGICALUNIT_HAS_BAD_DISK;
	}
	if (NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_REGULAR & RaidSimpleStatusFlags ||
		NDAS_RAID_SIMPLE_STATUS_BAD_SECTOR_IN_SPARE & RaidSimpleStatusFlags)
	{
		return NDASSVC_ERROR_NDAS_LOGICALUNIT_HAS_BAD_SECTOR;
	}
	if (NDAS_RAID_SIMPLE_STATUS_REPLACED_IN_REGULAR & RaidSimpleStatusFlags ||
		NDAS_RAID_SIMPLE_STATUS_REPLACED_IN_SPARE & RaidSimpleStatusFlags)
	{
		return NDASSVC_ERROR_NDAS_LOGICALUNIT_REPLACED_DEVICE_WITH_SPARE;
	}

	return S_OK;
}

HRESULT
CNdasLogicalUnit::PlugIn(
	ACCESS_MASK requestingAccess,
	DWORD LdpfFlags,
	DWORD LdpfValues,
	DWORD PlugInFlags)
{
	HRESULT hr;

	hr = pCheckRaidStatus(PlugInFlags);

	if(FAILED(hr))
	{
		return hr;
	}

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

#if 0
		//
		// Remove target ejects the disk and the volume.
		//

		success = NdasBusCtlRemoveTarget(m_NdasLogicalUnitId);
		if (!success) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
				"NdasBusCtlRemoveTarget failed, error=0x%X\n", GetLastError());
		}

		// Intentional break
		::Sleep(100);

#endif

		//
		// BUG:
		// What happened when RemoveTarget succeeded and 
		// Unplugging LANSCSI port is failed?
		//

		success = NdasBusCtlUnplug(m_NdasLogicalUnitId);

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
			m_NdasLogicalUnitId);

		//
		// If this is RAID, RAID configuration may have been changed by kernel 
		// But the change is ignored while it is mounted.
		// So we need to update it now.
		//
		Invalidate();
	}
#endif

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Remove Ldpf from the registry
	(void) pClearLastLogicalUnitPlugInFlags();

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Unplug completed successfully, SlotNo=%08X\n",
		m_NdasLogicalUnitId);

	//
	// Verify every ndas unit
	//
	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);
	HRESULT hr2 = AtlForEach(ndasUnits, VerifyNdasUnit()).GetResult();
	if (FAILED(hr2))
	{
		autolock.Release();
		pInvalidateNdasUnits();
	}

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
				m_NdasLogicalUnitId, hr);

			return hr;
		}
	}
	else
	{
		BOOL success = ::NdasBusCtlEject(m_NdasLogicalUnitId);

		if (!success) 
		{
			hr = HRESULT_FROM_WIN32(GetLastError());

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"NdasBusCtlEject failed, SlotNo=%08X, error=0x%X\n", 
				m_NdasLogicalUnitId, hr);

			return hr;
		}
	}

#if 0
	if (_IsRaid()) 
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
			"Invalidating RAID after unplugging to update RAID information, SlotNo=%08X\n",
			m_NdasLogicalUnitId);

		//
		// If this is RAID, RAID configuration may have been changed by kernel 
		// But the change is ignored while it is mounted.
		// So we need to update it now.
		//
		Invalidate();
	}
#endif
	
	//
	// Now we have to wait until the ejection is complete
	//
	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Eject completed successfully, SlotNo=%08X\n",
		m_NdasLogicalUnitId);

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

	NDAS_LOCATION ndasLocation = m_NdasLogicalUnitId;
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
		= NdasLocationToLogicalUnitAddress(m_NdasLogicalUnitId);

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
		//XTLASSERT(FALSE && "GetSharedWriteInfo Failed");
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
		BusEnumQuery.SlotNo = m_NdasLogicalUnitId;
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
				m_NdasLogicalUnitId, hr);

			return hr;
		}

		deviceMode = BusEnumInformation.PdoInfo.DeviceMode;
		supportedFeatures = BusEnumInformation.PdoInfo.SupportedFeatures;
		enabledFeatures = BusEnumInformation.PdoInfo.EnabledFeatures;

	}
	else 
	{
		XTL::AutoFileHandle storageDeviceHandle = CreateFile(
			m_SystemDevicePath,
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
				m_SystemDevicePath, hr);
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
				m_NdasLogicalUnitId, hr);
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
	if (SUCCEEDED(hr)) 
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
			m_NdasLogicalUnitId);
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
			m_NdasLogicalUnitId,
			&usage);
	}

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = SUCCEEDED(hr) ? E_FAIL : hr;

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LfsFiltQueryNdasUsage failed, SlotNo=%08X, error=0x%X\n",
			m_NdasLogicalUnitId, hr);
		
		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"LfsFiltQueryNdasUsage: SlotNo=%08X, primary=%d, secondary=%d, hasLockedVolume=%d.\n",
		m_NdasLogicalUnitId,
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

NDAS_LOGICALUNIT_ADDRESS 
CNdasLogicalUnit::pGetNdasLogicalUnitAddress()
{
	return NdasLocationToLogicalUnitAddress(m_NdasLogicalUnitId);
}

STDMETHODIMP 
CNdasLogicalUnit::get_LogicalUnitAddress(
	__out NDAS_LOGICALUNIT_ADDRESS* LogicalUnitAddress)
{
	CAutoLock autolock(this);
	*LogicalUnitAddress = NdasLocationToLogicalUnitAddress(m_NdasLogicalUnitId);
	return S_OK;
}

HRESULT
CNdasLogicalUnit::pIsSafeToPlugIn(ACCESS_MASK requestingAccess)
{
	HRESULT hr;

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

	if (!pIsComplete())
	{
		return NDASSVC_ERROR_NDAS_LOGICALUNIT_IS_INCOMPLETE;
	}

	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);

	hr = AtlForEach(ndasUnits, VerifyNdasUnit()).GetResult();

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"VerifyNdasLogicalUnitDefinition failed, hr=0x%X\n", hr);
		return hr;
	}

	//if (!pIsMountable()) 
	//{
	//	if(m_RaidFailReason & NDAS_RAID_FAIL_REASON_DEGRADE_MODE_CONFLICT)
	//		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_FIX_REQUIRED;
	//	else
	//		return NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING;
	//}
	//else if (MountableStateChanged) 
	//{
	//	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	//	epub.LogicalDeviceRelationChanged(m_NdasLogicalUnitId);
	//	// To do: use another error code.
	//	return NDASSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION;
	//}

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

STDMETHODIMP 
CNdasLogicalUnit::get_UserBlocks(__out UINT64* Blocks)
{
	CAutoLock autolock(this);

	*Blocks = 0;

	if (!pIsComplete())
	{
		return S_FALSE;
	}

	CComPtr<INdasUnit> pPrimaryNdasUnit;
	HRESULT hr = pGetPrimaryNdasUnit(&pPrimaryNdasUnit);
	if (FAILED(hr)) 
	{
		return hr;
	}

	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK(m_NdasLogicalUnitDefinition.Type)) 
	{
		return pPrimaryNdasUnit->get_UserBlocks(Blocks);
	}

	UINT64 blocks = 0;
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
					UINT64 singleBlocks;
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
		COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&blocks));
		blocks *= pGetUnitDeviceCount();
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R3:			
		COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&blocks));
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R3:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID5:
		{
			UINT64 singleBlocks;
			COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&singleBlocks));
			blocks += singleBlocks * (pGetUnitDeviceCountInRaid() - 1);
		}
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		COMVERIFY(pPrimaryNdasUnit->get_UserBlocks(&blocks));
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

	NdasUiDbgCall( 2, "LogicalUnit=%d, MOUNTED as %ls\n", m_NdasLogicalUnitId, OLE2CW(DevicePath) );

	DWORD dwTick = ::GetTickCount();
	m_dwMountTick = (dwTick == 0) ? 1: dwTick; // 0 is used for special purpose

	m_SystemDevicePath.AssignBSTR(DevicePath);

	m_Abnormalities = Abnormalities;

	pSetLastMountAccess(m_MountedAccess);
	SetRiskyMountFlag(TRUE);
	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
}

HRESULT
CNdasLogicalUnit::pUpdateSystemDeviceInformation()
{
	XTLASSERT(IsNdasPortMode());

	CNdasServiceDeviceEventHandler& devEventHandler =
		pGetNdasDeviceEventHandler();

	HRESULT hr = devEventHandler.GetLogicalUnitDevicePath(
		m_NdasLogicalUnitId,
		&m_SystemDevicePath);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit device path is unknown, LogicalUnit=%p, hr=0x%x\n", 
			this, hr);

		return hr;
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
		"LogicalUnit (%p) device path=%ls\n", 
		this, OLE2CW(m_SystemDevicePath));

	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = 
		pGetNdasLogicalUnitAddress();

	XTL::AutoFileHandle logicalUnitHandle = CreateFile(
		m_SystemDevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	if (INVALID_HANDLE_VALUE == static_cast<HANDLE>(logicalUnitHandle))
	{
		hr = AtlHresultFromLastError();

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"Opening device file failed, path=%ls, hr=0x%x\n", 
			OLE2CW(m_SystemDevicePath), hr);

		return hr;
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

		return hr;
	}
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
		desiredAccess = GENERIC_READ | GENERIC_WRITE;
	}
	else 
	{
		desiredAccess = 0;
	}

	if (desiredAccess != 0) 
	{
		pSetMountedAccess(desiredAccess);

		//
		// Change of the mounted access should be propagated through an event
		//
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		epub.LogicalDevicePropertyChanged(m_NdasLogicalUnitId);
	}

	// Free the full LUR information.
	ATLVERIFY(HeapFree(GetProcessHeap(), 0, pLurInfo));

	return S_OK;
}

HRESULT
CNdasLogicalUnit::pReconcileWithNdasPort()
{
	m_PendingReconcilation = FALSE;

	HRESULT hr = pUpdateSystemDeviceInformation();

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"pUpdateSystemDeviceInformation failed, LogicalUnit=%p, hr=0x%x\n",
			this, hr);

		//
		// This is just a warning and the status will be changed to mounted
		// regardless. However, update system device information is failed,
		// CNdasLogicalUnit::OnTimer will call pUpdateSystemDeviceInformation
		// again to finalize this process.
		//

		m_PendingReconcilation = TRUE;
	}

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	pGetNdasDeviceEventHandler().QueueRescanDriveLetters();

	return S_OK;
}

HRESULT
CNdasLogicalUnit::pReconcileWithNdasBus()
{
	NdasUiDbgCall( 2, "in\n" );

	HANDLE hAlarm, hDisconnect;
	ULONG deviceMode;
	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress = pGetNdasLogicalUnitAddress();

	BOOL success = ::NdasBusCtlQueryPdoEvent(
	    m_NdasLogicalUnitId, 
	    &hAlarm,
	    &hDisconnect);

	//
	// Reconciliation failure?
	//
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlQueryPdoEvent failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLogicalUnitId, GetLastError());
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
	success = ::NdasBusCtlQueryDeviceMode(m_NdasLogicalUnitId, &deviceMode);
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"NdasBusCtlQueryDeviceMode failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLogicalUnitId, GetLastError());
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
			deviceMode == DEVMODE_SUPER_READWRITE) {

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
	}

	//
	// Retrieve the LUR full information
	//
	PNDSCIOCTL_LURINFO pLurInfo = NULL;
	success = ::NdasBusCtlQueryMiniportFullInformation(
		m_NdasLogicalUnitId, &pLurInfo);
	if (success && pLurInfo)
	{
		HeapFree(GetProcessHeap(), 0, pLurInfo);
	}
	if (!success)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"QueryMiniportFullInformation failed, SlotNo=%08X, error=0x%X\n", 
			m_NdasLogicalUnitId, GetLastError());

		pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

		return S_OK;
	}

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	pGetNdasDeviceEventHandler().QueueRescanDriveLetters();

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

	pSetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	//
	// Verify every ndas unit
	//
	CInterfaceArray<INdasUnit> ndasUnits;
	pGetNdasUnitInstances(ndasUnits);
	HRESULT hr = AtlForEach(ndasUnits, VerifyNdasUnit()).GetResult();

	if (FAILED(hr))
	{
		autolock.Release();
		pInvalidateNdasUnits();
	}
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
		m_RegistrySubPath,
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
		m_RegistrySubPath,
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
		m_RegistrySubPath, 
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

HRESULT 
CNdasLogicalUnit::pGetLastPlugInFlags(DWORD &Flags)
{
	Flags = 0;

	HRESULT hr;

	DWORD data = 0;
	BOOL success = _NdasSystemCfg.GetValueEx(
		m_RegistrySubPath,
		_T("PlugInFlags"),
		&data,
		sizeof(data));
	if (!success)
	{
		hr = AtlHresultFromLastError();
		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, pGetLastPlugInFlags failed, hr=0x%X\n", 
			m_NdasLogicalUnitId, hr);
		return hr;
	}
	Flags = data;
	return S_OK;
}

HRESULT 
CNdasLogicalUnit::pSetLastPlugInFlags(DWORD Flags)
{
	HRESULT hr;

	BOOL success = _NdasSystemCfg.SetValueEx(
		m_RegistrySubPath,
		_T("PlugInFlags"),
		REG_BINARY,
		&Flags,
		sizeof(Flags));

	if (!success)
	{
		hr = AtlHresultFromLastError();

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, pSetLastPlugInFlags failed, flags=%08X, hr=0x%X\n", 
			m_NdasLogicalUnitId, Flags, hr);

		return hr;
	}

	return S_OK;
}

HRESULT 
CNdasLogicalUnit::pClearLastPlugInFlags()
{
	HRESULT hr;

	BOOL success = _NdasSystemCfg.DeleteValue(
		m_RegistrySubPath, 
		_T("PlugInFlags"));

	if (!success)
	{
		hr = AtlHresultFromLastError();

		XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
			"LogicalUnit=%d, pClearLastPlugInFlags failed, hr=0x%X\n", 
			m_NdasLogicalUnitId, hr);

		return hr;
	}

	return S_OK;
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

STDMETHODIMP 
CNdasLogicalUnit::get_Abnormalities(__out DWORD * Abnormalities)
{
	CAutoLock autolock(this);
	*Abnormalities = m_Abnormalities;
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
				m_NdasLogicalUnitId, hr);

			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Reset to Unmounted\n");

			OnDismounted();

			return;
		}

		//
		// Reconcile with the logical unit device object again
		// if it failed before.
		//
		if (m_PendingReconcilation)
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
				"UpdateSystemDeviceInformation pending...\n");

			hr = pUpdateSystemDeviceInformation();

			if (FAILED(hr))
			{
				XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
					"UpdateSystemDeviceInformation failed, hr=0x%X\n", hr);
			}
			else
			{
				XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_WARNING,
					"UpdateSystemDeviceInformation completed...\n");

				m_PendingReconcilation = FALSE;
			}
		}
	}
	else
	{
		NDAS_LOCATION location = m_NdasLogicalUnitId;
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
				m_NdasLogicalUnitId, GetLastError());
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
		if (NULL != pNdasUnit)
		{
			NdasUnits.Add(pNdasUnit);
		}
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
