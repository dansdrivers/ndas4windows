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

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASLOGDEV
#include "xdebug.h"

#include "traceflags.h"

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
	CStringizer(_T("LD.%02d"), logDeviceId),
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
	m_ulAdapterStatus(ADAPTERINFO_STATUS_INIT)
{
	XTLCALLTRACE2(TCLogDevice);
}

//
// Destructor
//
CNdasLogicalDevice::~CNdasLogicalDevice()
{
	XTLTRACE2(TCLogDevice,TLTrace,__FUNCTION__ "%ws\n", ToString());
}

BOOL
CNdasLogicalDevice::Initialize()
{
	XTLCALLTRACE2(TCLogDevice);

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
			DBGPRT_ERR_EX(_FT("Disconnect event creation failed: "));
			return FALSE;
		}
	}

	if (m_hAlarmEvent.IsInvalid()) 
	{
		m_hAlarmEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hAlarmEvent.IsInvalid()) 
		{
			DBGPRT_ERR_EX(_FT("Alarm event creation failed: "));
			::CloseHandle(m_hDisconnectedEvent);
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

	DBGPRT_INFO(_FT("Logical Device %d initialized successfully.\n"), m_logicalDeviceId);

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

//
// Set the unit device ID at a sequence 
// to a unit device member ID list
//
BOOL 
CNdasLogicalDevice::AddUnitDevice(CNdasUnitDevicePtr pUnitDevice)
{
	InstanceAutoLock autolock(this);
	BOOL fSuccess;

	DWORD ldSequence = pUnitDevice->GetLDSequence();

	XTLASSERT(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices) 
	{
		DBGPRT_ERR(_FT("Invalid sequence (%d) of the unit device.\n"), ldSequence);
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
			XTLASSERT(FALSE && "Duplicate calls to AddUnitDevice");
			DBGPRT_ERR(_FT("Sequence (%d) is already occupied.\n"), ldSequence);
			return FALSE;
		}
	}

	m_unitDevices.push_back(pUnitDevice);

	if (IsComplete())
	{
		if (_IsUpgradeRequired())
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_REQUIRE_UPGRADE);
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			epub.LogicalDeviceRelationChanged(m_logicalDeviceId);
			_SetStatus(NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE);
			return TRUE;
		}
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
	}
	else
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
	}

	//
	// Allocate NDAS SCSI Location when the first unit device is
	// available
	//
	if (0 == ldSequence) 
	{
		_AllocateNdasScsiLocation();
	}

	if (IsComplete()) 
	{
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		manager.RegisterNdasScsiLocation(m_NdasScsiLocation, shared_from_this());
	}

	if (IsComplete()) 
	{
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
		}
		else if (m_fMountOnReady) 
		{
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
					DBGPRT_INFO(_FT("Boot-time mount (%s) succeeded.\n"), this->ToString());
				}
				else
				{
					DBGPRT_ERR_EX(_FT("Boot-time mount (%s) failed: "), this->ToString());
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
					DBGPRT_INFO(_FT("RO Boot-time mount (%s) succeeded.\n"), this->ToString());
				}
				else
				{
					DBGPRT_ERR_EX(_FT("RO Boot-time mount (%s) failed: "), this->ToString());
				}
			}
		}
	}

	DBGPRT_INFO(_FT("Added %s to Logical Device %s\n"), 
		pUnitDevice->ToString(), 
		this->ToString());

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

	XTLASSERT(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices) 
	{
		DBGPRT_ERR(_FT("Invalid sequence (%d) of the unit device.\n"), ldSequence);
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
		DBGPRT_ERR(_FT("Unit device in sequence (%d) is not occupied.\n"), ldSequence);
		XTLASSERT(FALSE);
		return FALSE;
	}

	//
	// Workaround for Fault-tolerant Mode
	// -> Even if the logical device is incomplete,
	//    Mounted Logical Device Location should be intact
	//    if it is mounted
	//
	/* NOT IMPLEMENTED YET
	if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == GetStatus()) 
	{
		--m_nUnitDeviceInstances;
		return TRUE;
	}
	*/

	// If the previous status was complete, unregister ndasscsi location
	if (IsComplete()) 
	{
		CNdasLogicalDeviceManager& manager = pGetNdasLogicalDeviceManager();
		manager.UnregisterNdasScsiLocation(m_NdasScsiLocation, shared_from_this());
	}

	m_unitDevices.erase(
		m_unitDevices.begin() + 
		std::distance(unitDevices.begin(), itr));

	//
	// Deallocation NDAS SCSI Location when the fist unit device is removed
	//
	if (0 == ldSequence) 
	{
		_DeallocateNdasScsiLocation();
	}

	//
	// Publish Event
	//
	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceRelationChanged(m_logicalDeviceId);

	//
	// Set Device Error
	//
	_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);

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


void 
CNdasLogicalDevice::_SetStatus(NDAS_LOGICALDEVICE_STATUS newStatus)
{
	InstanceAutoLock autolock(this);

	NDAS_LOGICALDEVICE_STATUS oldStatus = m_status;

	//
	// Ignore duplicate status change
	//
	if (oldStatus == newStatus) 
	{
		return;
	}

	XTLVERIFY( pIsValidStatusChange(oldStatus, newStatus) );

	const DWORD LDS_NOT_INIT = NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED;
	const DWORD LDS_UNMOUNTED = NDAS_LOGICALDEVICE_STATUS_UNMOUNTED;
	const DWORD LDS_MOUNTED = NDAS_LOGICALDEVICE_STATUS_MOUNTED;
	const DWORD LDS_MOUNT_PENDING = NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING;
	const DWORD LDS_UNMOUNT_PENDING = NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING;
	const DWORD LDS_NOT_MOUNTABLE = NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE;

	//
	// Attaching to the event monitor
	//
    //                 +--------------------------------+ 
	//                 | (D)                            | (A)
	//                 V          (A)                   v
	// NOT_INIT --> UNMOUNTED <----> MOUNT_PENDING --> MOUNTED
	//              ^  ^     (D)                        ^
	//              |  |   (D)                          |
	//              |  +-------- UNMOUNT_PENDING  <-----+
	//              V
	//        NOT_MOUNTABLE
	//
	if ((LDS_UNMOUNTED == oldStatus && LDS_MOUNT_PENDING == newStatus) ||
		(LDS_UNMOUNTED == oldStatus && LDS_MOUNTED == newStatus))
	{
		// Attach to the event monitor
		CNdasEventMonitor& emon = pGetNdasEventMonitor();
		emon.Attach(shared_from_this());
	}
	else if (LDS_UNMOUNTED == newStatus && 
		LDS_NOT_INIT != oldStatus &&
		LDS_NOT_MOUNTABLE != oldStatus)
	{
		// Detach from the event monitor
		CNdasEventMonitor& emon = pGetNdasEventMonitor();
		emon.Detach(shared_from_this());
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
		(LDS_UNMOUNTED == oldStatus && LDS_MOUNTED == newStatus))
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

ACCESS_MASK 
CNdasLogicalDevice::GetGrantedAccess()
{
	InstanceAutoLock autolock(this);
	if (0 == GetUnitDeviceCount()) {
		return 0x00000000L;
	}

	ACCESS_MASK access(0xFFFFFFFFL);
	//
	// Any single missing entry will revoke all accesses
	//
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {

		CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(i);

		if (0 == pUnitDevice.get()) {
			return 0x00000000L;
		}

		access &= pUnitDevice->GetGrantedAccess();
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
	// Any single missing entry will revoke all accesses
	//
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) 
	{

		CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(i);

		if (0 == pUnitDevice.get()) 
		{
			return 0x00000000L;
		}

		access &= pUnitDevice->GetAllowingAccess();
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

#if 0

DWORD
CNdasLogicalDevice::_GetMaxRequestBlocks()
{
	InstanceAutoLock autolock(this);

	//
	// MaxRequestBlock is a NDAS Device's dependent property
	// So we only have to look at NDAS devices
	//

	DWORD dwMaxRequestBlocks(0);

	for (DWORD i = 0; i < GetUnitDeviceCount(); i++) 
	{
		CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(i);
		if (0 == pUnitDevice.get()) 
		{
			return 0;
		}

		if (0 == i) 
		{
			dwMaxRequestBlocks = pUnitDevice->GetOptimalMaxRequestBlock();
		}
		else
		{
			dwMaxRequestBlocks = 
				min(dwMaxRequestBlocks, pUnitDevice->GetOptimalMaxRequestBlock());
		}

	}

	m_dwCurrentMRB = dwMaxRequestBlocks;
	return dwMaxRequestBlocks;

}

#endif

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
		DBGPRT_INFO(_FT("NoPSWriteShare is set at %s.\n"), ToString());
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
		DBGPRT_INFO(_FT("LFSFilter does not exist. NoPSWriteShare.\n"));
		return FALSE;
	}

	if (NdasServiceConfig::Get(nscDisableRAIDWriteShare))
	{
		if (NDAS_LOGICALDEVICE_TYPE_DISK_RAID1 == m_logicalDeviceGroup.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2 == m_logicalDeviceGroup.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID4 == m_logicalDeviceGroup.Type ||
			NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2 == m_logicalDeviceGroup.Type)
		{
			XTLTRACE2(TCLogDevice, TLInfo, 
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
	CNdasUnitDevicePtr pPrimaryUnitDevice = GetUnitDevice(0);

	if (CNdasUnitDeviceNullPtr == pPrimaryUnitDevice) 
	{
		_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	}

	CNdasDevicePtr pDevice = pPrimaryUnitDevice->GetParentDevice();
	if (CNdasDeviceNullPtr == pDevice) 
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
		NDASSCSI_TYPE_DISK_RAID1R == targetType ||
		NDASSCSI_TYPE_DISK_RAID4R == targetType )
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

		CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(i);
		if (CNdasUnitDeviceNullPtr == pUnitDevice) 
		{
			_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
			::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
			return FALSE;
		}

		CNdasDevicePtr pDevice = pUnitDevice->GetParentDevice();
		XTLASSERT(CNdasDeviceNullPtr != pDevice);

		PLSBUS_UNITDISK pud = &pAddTargetData->UnitDiskList[i];

		::CopyMemory(
			pud->Address.Node, 
			pDevice->GetRemoteLpxAddress().Node, 
			sizeof(pud->Address.Node));

		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);

		C_ASSERT(
			sizeof(pud->NICAddr.Node) ==
			sizeof(pDevice->GetLocalLpxAddress().Node));

		::CopyMemory(
			pud->NICAddr.Node, 
			pDevice->GetLocalLpxAddress().Node, 
			sizeof(pud->NICAddr.Node));

		pud->NICAddr.Port = htons(0); // should be zero

		pud->iUserID = pUnitDevice->GetDeviceUserID(requestingAccess);
		pud->iPassword = pUnitDevice->GetDevicePassword();

		pud->ulUnitBlocks = pUnitDevice->GetUserBlockCount();
		pud->ulPhysicalBlocks = pUnitDevice->GetPhysicalBlockCount();
		pud->ucUnitNumber = static_cast<UCHAR>(pUnitDevice->GetUnitNo());
		pud->ucHWType = pDevice->GetHardwareType();
		pud->ucHWVersion = pDevice->GetHardwareVersion();
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

		pud->UnitMaxRequestBlocks = pUnitDevice->GetOptimalMaxRequestBlock();
		
		//
		// Add Target Info
		//

		if (NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType()) 
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

	DBGPRT_INFO(
		_FT("LsBusCtlPlugInEx2(SlotNo %d, MaxReqBlock %d, DisEvt %p, RecEvt %p).)\n"),
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
		DBGPRT_ERR_EX(_FT("LsBusCtlPlugInEx2 failed: \n"));
		XTLASSERT(fSuccess && "PlugIn failure");
		return FALSE;
	}

	DBGPRT_INFO(_FT("LsBusCtlPluginEx succeeded.\n"));

	fSuccess = LsBusCtlAddTarget(pAddTargetData);
	if (!fSuccess) 
	{
		DBGPRT_ERR_EX(_FT("AddTarget failed.\n"));
		::Sleep(1000);
		LsBusCtlEject(m_NdasScsiLocation.SlotNo);
		XTLASSERT(fSuccess);
		return FALSE;
	}

	DBGPRT_INFO(_FT("LsBusCtlAddTarget succeeded.\n"));

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

	SetAdapterStatus(ADAPTERINFO_STATUS_INIT);

	DBGPRT_INFO(_FT("Plugged in successfully at %s.\n"), m_NdasScsiLocation.ToString());

	return TRUE;
}

BOOL
CNdasLogicalDevice::Unplug()
{
	BOOL fSuccess(FALSE);

	InstanceAutoLock autolock(this);

	DBGPRT_INFO(_FT("Unplugging %s\n"), ToString());

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
	if (!fSuccess) {
		DBGPRT_WARN_EX(_FT("LsBusCtlRemoveTarget failed: "));
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
		DBGPRT_ERR_EX(_FT("LsBusCtlUnplug failed: "));
		// last error from lsbusctl unplug
		return FALSE;
	}

	//
	// Change the status to unmounted
	//
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Remove Ldpf from the registry
	(void) ClearLastLdpf();

	DBGPRT_INFO(_FT("Unplugged successfully at slot %s.\n"),
		CNdasScsiLocation(m_NdasScsiLocation).ToString());

	return TRUE;

}

BOOL
CNdasLogicalDevice::Eject()
{
	InstanceAutoLock autolock(this);

	DBGPRT_INFO(_FT("Ejecting %s\n"), ToString());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		DBGPRT_ERR(_FT("Eject is requested to not initialized logical device"));
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
	{
		::SetLastError(NDASSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		DBGPRT_ERR(_FT("Eject is requested to not mounted logical device"));
		return FALSE;
	}

	BOOL fSuccess = ::LsBusCtlEject(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) 
	{
		DBGPRT_ERR_EX(_FT("LsBusCtlEject failed at %s.\n"), m_NdasScsiLocation.ToString());
		return FALSE;
	}

	//
	// Now we have to wait until the ejection is complete
	//
	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	DBGPRT_INFO(_FT("Ejected successfully at slot %s.\n"), m_NdasScsiLocation.ToString());

	return TRUE;
}

BOOL 
CNdasLogicalDevice::EjectEx(
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName,
	DWORD nNameLength)
{
	DBGPRT_INFO(_FT("Ejecting %s\n"), ToString());
	DWORD slotNo = GetNdasScsiLocation().SlotNo;
	CONFIGRET cret;
	BOOL fSuccess = pRequestEject(slotNo, &cret, pVetoType, pszVetoName, nNameLength);

	if (fSuccess && CR_SUCCESS == cret)
	{
		DBGPRT_INFO(_FT("RequestEject(%d) returned CRET=%08X\n"), slotNo, cret);
	}
	else
	{
		if (!fSuccess)
		{
			DBGPRT_ERR_EX(_FT("RequestEject(%d) failed: "), slotNo);
		}
		else
		{
			DBGPRT_ERR(_FT("RequestEject(%d) failed by CRET=%08X\n"), slotNo, cret);
			if (pszVetoName)
			{
				DBGPRT_ERR(_FT("Vetoed by %s\n"), pszVetoName);
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
		XTLTRACE2_ERR(TCLogDevice, TLError,
			"LfsFiltQueryNdasUsage for %s failed.\n",
			m_NdasScsiLocation.ToStringA());
		return FALSE;
	}

	XTLTRACE2(TCLogDevice, TLInfo,
		"LfsFiltQueryNdasUsage for %s returned: Primary(%d), Secondary(%d), LockedVolume(%d).\n",
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

	if (!IsComplete()) 
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
		HRESULT hr = ::StringCchPrintf(
			m_szRegContainer, 30, 
			_T("LogDevices\\%08X"), m_dwHashValue);

		XTLASSERT(SUCCEEDED(hr));

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
			DBGPRT_WARN_EX(_FT("Writing LDData failed: "));
		}
	}

	DBGPRT_INFO(_FT("Hash Value: %08X\n"), m_dwHashValue);
	DBGPRT_INFO(_FT("RegContainer: %s\n"), m_szRegContainer);
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
	CNdasUnitDevicePtr pFirstUnitDevice = GetUnitDevice(0);
	XTLASSERT(CNdasUnitDeviceNullPtr != pFirstUnitDevice);
	m_NdasScsiLocation.SlotNo = 
		pFirstUnitDevice->GetParentDevice()->GetSlotNo() * 10 + 
		pFirstUnitDevice->GetUnitNo();
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

	if (!IsComplete()) 
	{
		return 0;
	}

	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_logicalDeviceGroup.Type)) 
	{
		XTLASSERT(1 == GetUnitDeviceCount());
		CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(0);
		XTLASSERT(0 != pUnitDevice.get());
		if (0 == pUnitDevice.get()) 
		{
			return 0;
		}
		return pUnitDevice->GetUserBlockCount();
	}

	UINT64 blocks = 0;
	for (DWORD i = 0; i < GetUnitDeviceCount(); i++) 
	{

		CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(i);
		XTLASSERT(0 != pUnitDevice.get());
		if (0 == pUnitDevice.get()) 
		{
			return 0;
		}

		XTLASSERT(pUnitDevice->GetType() == NDAS_UNITDEVICE_TYPE_DISK);

		CNdasUnitDiskDevice* pUnitDisk = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice.get());

		// dwBlocks += pUnitDisk->GetBlocks();

		XTLASSERT(IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_logicalDeviceGroup.Type));

		switch (m_logicalDeviceGroup.Type) 
		{
		case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
			if (0 == i) 
			{
				blocks = pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
			blocks += pUnitDisk->GetUserBlockCount();
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1_R2:
			if (i % 2 != 0) 
			{
				blocks += pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
			if (i != GetUnitDeviceCount() - 1) 
			{
				//
				// do not count parity disk
				//
				blocks += pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
			blocks = pUnitDisk->GetUserBlockCount();
			break;
		default: 
			// not implemented yet : DVD, VDVD, MO, FLASH ...
			XTLASSERT(FALSE);
			break;
		}

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

	DBGPRT_INFO(_FT("%s: Disconnect Event.\n"), ToString());

	BOOL fSuccess = Unplug();
	if (!fSuccess) 
	{
		DBGPRT_ERR_EX(_FT("%s: Failed to handle disconnect event: "), ToString());
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	// Set the disconnected flag
	m_fDisconnected = TRUE;
}


void
CNdasLogicalDevice::OnMounted()
{
	InstanceAutoLock autolock(this);

	DBGPRT_INFO(_FT("Logical device %s is MOUNTED.\n"), ToString());

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
		DBGPRT_ERR_EX(
			_FT("LsBusCtlQueryPdoEvent at %s failed: "), 
			m_NdasScsiLocation.ToString());
	}
	else
	{
		m_hAlarmEvent.Release();
		m_hAlarmEvent = hAlarm;
		m_hDisconnectedEvent.Release();
		m_hDisconnectedEvent = hDisconnect;
	}

	PLSMPIOCTL_ADAPTERLURINFO pLurInfo = NULL;
	fSuccess = ::LsBusCtlQueryMiniportFullInformation(
		m_NdasScsiLocation.SlotNo, 
		&pLurInfo);
	if (!fSuccess)
	{
		DBGPRT_ERR_EX(
			_FT("QueryMiniportFullInformation at %s failed: "), 
			m_NdasScsiLocation.ToString());
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
			m_dwCurrentMRB = pLurInfo->Adapter.MaxBlocksPerRequest;
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
		DBGPRT_ERR_EX(
			_FT("QueryPdoFileHandle at %s failed: "),
			m_NdasScsiLocation.ToString());
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
			DBGPRT_ERR_EX(
				_FT("AddDeviceNotificationHandle(%s) failed: "), 
				m_NdasScsiLocation.ToString());
		}
	}

	_SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	return TRUE;
}

void
CNdasLogicalDevice::OnUnmounted()
{
	InstanceAutoLock autolock(this);

	DBGPRT_INFO(_FT("%s: Unmount Completed%s.\n"), ToString(),
		m_fDisconnected ? _T(" (by disconnection)") : _T(""));

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

	DBGPRT_INFO(_FT("Unmount failure from logical device %s.\n"), ToString());

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
		DBGPRT_ERR_EX(_FT("%s: GetLastLdpf failed: "), ToString());
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
		DBGPRT_ERR_EX(_FT("%s: SetLastLdpf(%08X,%08X) failed: "), ToString(), flags, values);
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
		DBGPRT_ERR_EX(_FT("%s: ClearLastLdpf failed: "), ToString());
		return FALSE;
	}
	return TRUE;
}

const NDAS_CONTENT_ENCRYPT* 
CNdasLogicalDevice::GetContentEncrypt()
{
	CNdasUnitDevicePtr pPrimaryUnitDevice = GetUnitDevice(0);
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

	CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(0);
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
		XTLTRACE_ERR("LsBusCtlQueryNodeAlive at %ws failed: ", location.ToString());
		return;
	}

	if (!fAlive) 
	{
		DBGPRT_WARN(_FT("Logical device %s instance does not exist anymore.\n"), ToString());

		_SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
	}

	//if (fAdapterError) {
	//	XTLTRACE_ERR("LsBusCtlQueryNodeAlive reported an adapter error.\n"));
	//	pLogDevice->_SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
	//}
}

bool 
CNdasLogicalDevice::_IsUpgradeRequired()
{
	// RAID1 and RAID4 is replaced with RAID1_R2 and RAID4_R2
	// And the underlying device driver does not handle RAID1 and RAID4
	// So we have to make these types as NOT_MOUNTABLE and REQUIRE_UPGRADE
	// Error.
	NDAS_LOGICALDEVICE_TYPE Type = GetType();
	switch (Type)
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		return true;
	}
	return false;
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
	//                 ^     (D)                        ^
	//                 |   (D)                          |
	//                 +-------- UNMOUNT_PENDING  <-----+
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
		return NDASSCSI_TYPE_DISK_RAID1R;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		return NDASSCSI_TYPE_DISK_RAID4;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4_R2:
		return NDASSCSI_TYPE_DISK_RAID4R;
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
