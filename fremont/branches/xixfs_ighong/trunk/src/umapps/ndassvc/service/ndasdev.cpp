/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <queue>
#include <ndas/ndastypeex.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndascomm.h>
#include <ndasbusioctl.h>

#include "ndasdev.h"
#include "ndasunitdev.h"
#include "ndasdevhb.h"
#include "ndascfg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasunitdevfactory.h"
#include "ndasobjs.h"
#include "lpxcomm.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASDEV
#include "xdebug.h"

#include "traceflags.h"

//////////////////////////////////////////////////////////////////////////
//
// AutoNdasHandle
//
//////////////////////////////////////////////////////////////////////////

struct AutoNdasHandleConfig
{
	static HNDAS GetInvalidValue() { return NULL; }
	static void Release(HNDAS h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::NdasCommDisconnect(h);
		XTLASSERT(fSuccess);
		::SetLastError(dwError);
	}
};

typedef XTL::AutoResourceT<HNDAS, AutoNdasHandleConfig> AutoNdasHandle;

//////////////////////////////////////////////////////////////////////////

inline bool 
IsSupportedHardwareVersion(UCHAR ucType, UCHAR ucVersion)
{
	return (0 == ucType) && (0 == ucVersion || 1 == ucVersion || 2 == ucVersion);
}

const NDAS_OEM_CODE& 
GetDefaultNdasOemCode(
	const NDAS_DEVICE_ID& DeviceId,
	const NDASID_EXT_DATA& NdasIdExtension)
{
	if (NDAS_VID_SEAGATE == NdasIdExtension.VID)
	{
		return NDAS_OEM_CODE_SEAGATE;
	}

	// Sample hardware range: 00:F0:0F:xx:xx:xx
	if (0x00 == DeviceId.Node[0] && 
		0xF0 == DeviceId.Node[1] && 
		0x0F == DeviceId.Node[2])
	{
		return NDAS_OEM_CODE_SAMPLE;
	} 
	// Rutter Tech NDAS Device address range:
	// 00:0B:D0:20:00:00 - 00:0B:D0:21:FF:FF
	//
	else if (
		0x00 == DeviceId.Node[0] &&
		0x0B == DeviceId.Node[1] &&
		0xD0 == DeviceId.Node[2] &&
		(0x20 == (DeviceId.Node[3] & 0xFE)))
	{
		return NDAS_OEM_CODE_RUTTER;
	}
	// Retail Devices
	return NDAS_OEM_CODE_DEFAULT;
}

class CNdasEventMonitor;

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDevice
//
//////////////////////////////////////////////////////////////////////////

//
// constructor
//

namespace
{
	__forceinline bool
	FlagIsSet(DWORD Flags, DWORD TargetFlag)
	{
		return (Flags & TargetFlag) == (TargetFlag);
	}

	struct CNdasUnitDeviceStat : _NDAS_UNITDEVICE_STAT {
		CNdasUnitDeviceStat() {
			::ZeroMemory(this, sizeof(NDAS_UNITDEVICE_STAT));
			this->Size = sizeof(NDAS_UNITDEVICE_STAT);
		}
	};
	struct CNdasDeviceStat : _NDAS_DEVICE_STAT {
		CNdasDeviceStat() {
			::ZeroMemory(this, sizeof(NDAS_DEVICE_STAT));
			this->Size = sizeof(NDAS_DEVICE_STAT);
			this->UnitDevices[0].Size = sizeof(NDAS_UNITDEVICE_STAT);
			this->UnitDevices[1].Size = sizeof(NDAS_UNITDEVICE_STAT);
		}
	};

	struct UnitDeviceNoEquals : std::unary_function<CNdasUnitDevicePtr,bool> {
		const DWORD UnitNo;
		UnitDeviceNoEquals(DWORD UnitNo) : UnitNo(UnitNo) {}
		bool operator()(const CNdasUnitDevicePtr& pUnitDevice) const {
			return (UnitNo == pUnitDevice->GetUnitNo());
		}
	};

	struct UnitDeviceStatusEquals : std::unary_function<CNdasUnitDevicePtr,bool> {
		const NDAS_UNITDEVICE_STATUS Status;
		UnitDeviceStatusEquals(DWORD Status) : Status(Status) {}
		bool operator()(const CNdasUnitDevicePtr& pUnitDevice) const {
			return (Status == pUnitDevice->GetStatus());
		}
	};

	struct RegisterUnitDevice : std::unary_function<CNdasUnitDevicePtr,void> {
		void operator()(const CNdasUnitDevicePtr& pUnitDevice) const {
			XTLVERIFY( pUnitDevice->RegisterToLogicalDeviceManager() );
		}
	};

	struct UnregisterUnitDevice : std::unary_function<CNdasUnitDevicePtr,void> {
		void operator()(const CNdasUnitDevicePtr& pUnitDevice) const {
			XTLVERIFY( pUnitDevice->UnregisterFromLogicalDeviceManager() );
		}
	};


}

const NDAS_DEVICE_HARDWARE_INFO NullHardwareInfo = {0};

CNdasDevice::CNdasDevice(
	__in DWORD SlotNo, 
	__in const NDAS_DEVICE_ID& DeviceId,
	__in DWORD RegFlags,
	__in_opt const NDASID_EXT_DATA* NdasIdExtension) :
	m_status(NDAS_DEVICE_STATUS_DISABLED),
	m_lastError(NDAS_DEVICE_ERROR_NONE),
	m_dwSlotNo(SlotNo),
	m_deviceId(DeviceId),
	m_grantedAccess(0x00000000L),
	m_dwLastHeartbeatTick(0),
	m_dwCommFailureCount(0),
	m_fAutoRegistered(FlagIsSet(RegFlags,NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED)),
	m_fVolatile(FlagIsSet(RegFlags,NDAS_DEVICE_REG_FLAG_VOLATILE)),
	m_fHidden(FlagIsSet(RegFlags,NDAS_DEVICE_REG_FLAG_HIDDEN)),
	m_ndasIdExtension(NdasIdExtension ? *NdasIdExtension : NDAS_ID_EXTENSION_DEFAULT),
	m_OemCode(GetDefaultNdasOemCode(DeviceId, NdasIdExtension ? *NdasIdExtension : NDAS_ID_EXTENSION_DEFAULT)),
	m_dstat(CNdasDeviceStat()),
	m_hardwareInfo(NullHardwareInfo),
	CStringizer(_T("{%03X}%s"),	SlotNo, CNdasDeviceId(DeviceId).ToString())
{
	XTLCALLTRACE2(TCDevice);

	m_szDeviceName[0] = 0;
	m_unitDevices.reserve(MAX_NDAS_UNITDEVICE_COUNT);

	XTLVERIFY(SUCCEEDED(
		::StringCchPrintf(m_szCfgContainer, 30, TEXT("Devices\\%04d"), SlotNo)));

	//
	// After we allocate m_szCfgContainer, we can use SetConfigValue
	//
	if (m_fAutoRegistered)
	{
		(void) SetConfigValue(_T("AutoRegistered"), m_fAutoRegistered);
	}
	else
	{
		(void) DeleteConfigValue(_T("AutoRegistered"));		
	}
}

//
// destructor
//

CNdasDevice::~CNdasDevice()
{
	XTLTRACE2(TCDevice,TLTrace,__FUNCTION__ "%ws\n", ToString());
	// _DestroyAllUnitDevices();
}

//
// initializer
//

BOOL
CNdasDevice::Initialize()
{
	return TRUE;
}

bool
CNdasDevice::IsAutoRegistered() const
{
	return m_fAutoRegistered;
}


bool
CNdasDevice::IsVolatile() const
{
	return m_fVolatile;
}

bool
CNdasDevice::IsHidden() const
{
	return m_fHidden;
}

//
// Device Name Setter
//

void
CNdasDevice::SetName(LPCTSTR szName)
{
	InstanceAutoLock autolock(this);

	HRESULT hr = ::StringCchCopy(
		m_szDeviceName, 
		MAX_NDAS_DEVICE_NAME_LEN, 
		szName);

	XTLASSERT(SUCCEEDED(hr));

	(void) SetConfigValue(_T("DeviceName"), szName);
	(void) pGetNdasEventPublisher().DevicePropertyChanged(m_dwSlotNo);
}

template <typename T>
BOOL
CNdasDevice::SetConfigValue(LPCTSTR szName, T value)
{
	// We don't write to a registry if volatile 
	if (IsVolatile()) return TRUE;

	BOOL fSuccess = _NdasSystemCfg.
		SetValueEx(m_szCfgContainer, szName, value);
	if (!fSuccess)
	{
		DBGPRT_WARN_EX(_FT("Writing device configuration to %s failed: "),
			m_szCfgContainer);
	}

	return fSuccess;
}

BOOL 
CNdasDevice::SetConfigValueSecure(
	LPCTSTR szName, LPCVOID lpValue, DWORD cbValue)
{
	// We don't write to a registry if volatile 
	if (IsVolatile()) return TRUE;

	BOOL fSuccess = _NdasSystemCfg.
		SetSecureValueEx(m_szCfgContainer, szName, lpValue, cbValue);
	if (!fSuccess) 
	{
		DBGPRT_WARN_EX(_FT("Writing data to %s of %s failed: "), 
			szName, m_szCfgContainer);
	}
	return fSuccess;
}

template <typename T>
BOOL 
CNdasDevice::SetConfigValueSecure(LPCTSTR szName, T value)
{
	return SetConfigValueSecure(szName, &value, sizeof(T));
}

BOOL 
CNdasDevice::DeleteConfigValue(LPCTSTR szName)
{
	BOOL fSuccess = _NdasSystemCfg.DeleteValue(m_szCfgContainer, szName);
	if (!fSuccess)
	{
		DBGPRT_WARN_EX(_FT("Delete configuration %s\\%s failed: "), 
			m_szCfgContainer, szName);
	}
	return fSuccess;
}

//
// set status of the device (internal use only)
//

void
CNdasDevice::_ChangeStatus(NDAS_DEVICE_STATUS newStatus)
{
	InstanceAutoLock autolock(this);

	const NDAS_DEVICE_STATUS oldStatus = m_status;

	if (oldStatus == newStatus) 
	{
		return;
	}


	//
	// clear failure count for every status change
	//
	m_dwCommFailureCount = 0;

	const NDAS_DEVICE_STATUS 
		ND_DISABLED = NDAS_DEVICE_STATUS_DISABLED,
		ND_DISCONNECTED = NDAS_DEVICE_STATUS_DISCONNECTED,
		ND_CONNECTING = NDAS_DEVICE_STATUS_CONNECTING,
		ND_CONNECTED = NDAS_DEVICE_STATUS_CONNECTED;

	if (ND_DISABLED == oldStatus && ND_DISCONNECTED == newStatus)
	{
		pGetNdasDeviceHeartbeatListener().Attach(this);
		// NO EVENT MONITOR CHANGE
		// NO UNIT DEVICE CHANGE
	}
	else if (ND_DISCONNECTED == oldStatus && ND_DISABLED == newStatus)
	{
		pGetNdasDeviceHeartbeatListener().Detach(this);
		// NO EVENT MONITOR CHANGE
		// NO UNIT DEVICE CHANGE
	}
	else if (ND_DISCONNECTED == oldStatus && ND_CONNECTING == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		// NO EVENT MONITOR CHANGE
		// NO UNIT DEVICE CHANGE
	}
	else if (ND_CONNECTING == oldStatus && ND_DISCONNECTED == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		// NO EVENT MONITOR CHANGE
		_DestroyAllUnitDevices();
	}
	else if (ND_CONNECTING == oldStatus && ND_CONNECTED == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		pGetNdasEventMonitor().Attach(shared_from_this());
		// NO UNIT DEVICE CHANGE
	}
	else if (ND_CONNECTED == oldStatus && ND_DISCONNECTED == newStatus)
	{
		// NO HEARTBEAT MONITOR CHANGE
		pGetNdasEventMonitor().Detach(shared_from_this());
		_DestroyAllUnitDevices();
	}
	else if (ND_CONNECTED == oldStatus && ND_DISABLED == newStatus)
	{
		pGetNdasDeviceHeartbeatListener().Detach(this);
		pGetNdasEventMonitor().Detach(shared_from_this());
		_DestroyAllUnitDevices();
	}
	else
	{
		XTLASSERT(FALSE && "INVALID STATUS CHANGE");
	}

	DBGPRT_INFO(_FT("%s status changed %s to %s\n"),
		ToString(),
		NdasDeviceStatusString(oldStatus),
		NdasDeviceStatusString(newStatus));

	m_status = newStatus;

	(void) pGetNdasEventPublisher().DeviceStatusChanged(m_dwSlotNo, oldStatus, newStatus);

	return;
}

//
// set status of the device
//

NDAS_DEVICE_STATUS 
CNdasDevice::GetStatus()
{
	return m_status;
}

//
// get device id
//

const NDAS_DEVICE_ID&
CNdasDevice::GetDeviceId() const
{
	return m_deviceId;
}

void 
CNdasDevice::GetDeviceId(NDAS_DEVICE_ID& DeviceId) const
{
	DeviceId = m_deviceId;
}

//
// get slot number of the NDAS device
// (don't be confused with logical device slot number.)
//

DWORD 
CNdasDevice::GetSlotNo() const
{
	return m_dwSlotNo;
}

//
// get the device name
// returned buffer is valid only while the instance is valid
// 

void
CNdasDevice::GetName(DWORD cchBuffer, LPTSTR lpBuffer)
{
	InstanceAutoLock autolock(this);
	HRESULT hr = ::StringCchCopy(lpBuffer, cchBuffer, m_szDeviceName);
	XTLASSERT(SUCCEEDED(hr));
}

//
// get granted (registered) access permission
//

ACCESS_MASK 
CNdasDevice::GetGrantedAccess()
{
	InstanceAutoLock autolock(this);
	return m_grantedAccess;
}

//
// set granted access permission
// use this to change the (valid) access of the registered device
//

void
CNdasDevice::SetGrantedAccess(ACCESS_MASK access)
{
	InstanceAutoLock autolock(this);

	// only GENERIC_READ and GENERIC_WRITE are acceptable
	m_grantedAccess = (access & (GENERIC_READ | GENERIC_WRITE));

	const DWORD cbData = sizeof(m_grantedAccess) + sizeof(m_deviceId);
	BYTE lpbData[cbData] = {0};

	::CopyMemory(lpbData, &m_grantedAccess, sizeof(m_grantedAccess));
	::CopyMemory(lpbData + sizeof(m_grantedAccess), &m_deviceId, sizeof(m_deviceId));

	(void) SetConfigValueSecure(_T("GrantedAccess"), lpbData, cbData);
	(void) pGetNdasEventPublisher().DevicePropertyChanged(m_dwSlotNo);
}

//
// on CONNECTED status only
// the local LPX address where the NDAS device was found
//

const LPX_ADDRESS&
CNdasDevice::GetLocalLpxAddress()
{
	InstanceAutoLock autolock(this);
	return m_localLpxAddress;
}

//
// remote LPX address of this device
// this may be different than the actual device ID. (Proxied?)
//

const LPX_ADDRESS&
CNdasDevice::GetRemoteLpxAddress()
{
	InstanceAutoLock autolock(this);
	return m_remoteLpxAddress;
}

UCHAR 
CNdasDevice::GetHardwareType()
{
	InstanceAutoLock autolock(this);
	return static_cast<UCHAR>(m_hardwareInfo.HardwareType);
}

UCHAR 
CNdasDevice::GetHardwareVersion()
{
	InstanceAutoLock autolock(this);
	return static_cast<UCHAR>(m_hardwareInfo.HardwareVersion);
}

UINT64 
CNdasDevice::GetHardwarePassword()
{
	InstanceAutoLock autolock(this);
	return m_OemCode.UI64Value;
}

// Internal Synchronized Function
void
CNdasDevice::GetOemCode(NDAS_OEM_CODE& OemCode)
{
	InstanceAutoLock autolock(this);
	OemCode = m_OemCode;
}

// Required External Synchronization
const NDAS_OEM_CODE&
CNdasDevice::GetOemCode()
{
	return m_OemCode;
}

void 
CNdasDevice::SetOemCode(const NDAS_OEM_CODE& OemCode)
{
	InstanceAutoLock autolock(this);
	m_OemCode = OemCode;
	XTLVERIFY(SetConfigValueSecure(
		_T("OEMCode"), &m_OemCode, sizeof(NDAS_OEM_CODE)));
}

void 
CNdasDevice::ResetOemCode()
{
	InstanceAutoLock autolock(this);
	m_OemCode = GetDefaultNdasOemCode(m_deviceId, m_ndasIdExtension);
	(void) DeleteConfigValue(_T("OEMCode"));
}

// Required External Synchronization
const NDASID_EXT_DATA& 
CNdasDevice::GetNdasIdExtension()
{
	return m_ndasIdExtension;
}

//
// get a unit device of a given unit number
// returns NULL if no unit device is available in a given unit.
//

CNdasUnitDevicePtr
CNdasDevice::GetUnitDevice(DWORD UnitNo)
{
	XTLENSURE_RETURN_T(UnitNo < MAX_NDAS_UNITDEVICE_COUNT, CNdasUnitDeviceNullPtr);
	InstanceAutoLock autolock(this);

	CNdasUnitDeviceVector::const_iterator itr = 
		std::find_if(
			m_unitDevices.begin(), m_unitDevices.end(),
			UnitDeviceNoEquals(UnitNo));

	if (m_unitDevices.end() == itr)
	{
		return CNdasUnitDeviceNullPtr;
	}

	CNdasUnitDevicePtr pUnitDevice = *itr;
	return pUnitDevice;
}

void 
CNdasDevice::GetUnitDevices(CNdasUnitDeviceVector& dest)
{
	InstanceAutoLock autolock(this);
	dest.resize(m_unitDevices.size());
	std::copy(
		m_unitDevices.begin(),
		m_unitDevices.end(),
		dest.begin());
}

//
// Enable/disable NDAS device
// returns TRUE if enable/disable is done successfully
// (now always returns TRUE)
//

BOOL
CNdasDevice::Enable(BOOL bEnable)
{
	InstanceAutoLock autolock(this);

	if (bEnable) 
	{
		//
		// To enable this device
		//
		if (NDAS_DEVICE_STATUS_DISABLED != m_status) 
		{
			return TRUE;
		} 

		//
		// DISABLED -> DISCONNECTED
		//
		_ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
	} 
	else 
	{
		//
		// To disable this device
		//
		if (NDAS_DEVICE_STATUS_DISABLED == m_status) 
		{
			return TRUE;
		} 

		//
		// You cannot disable this device when a unit device is mounted
		//
		if (_IsAnyUnitDevicesMounted())
		{
			::SetLastError(NDASSVC_ERROR_CANNOT_DISABLE_MOUNTED_DEVICE);
			return FALSE;
		}

		//
		// DISCONNECTED/CONNECTED -> DISABLED
		//
		_ChangeStatus(NDAS_DEVICE_STATUS_DISABLED);
	}

	XTLVERIFY( SetConfigValue(_T("Enabled"), bEnable) );

	//
	// Clear Device Error
	//
	_SetLastDeviceError(NDAS_DEVICE_ERROR_NONE);

	return TRUE;
}

//
// Subject to the Heartbeat Listener object
//

void 
CNdasDevice::Update(ximeta::CSubject* pChangedSubject)
{
	InstanceAutoLock autolock(this);

	CNdasDeviceHeartbeatListener& listener = pGetNdasDeviceHeartbeatListener();

	//
	// Ignore other than subscribed heartbeat listener
	//
	if (&listener == pChangedSubject) 
	{

		NDAS_DEVICE_HEARTBEAT_DATA hbData;

		listener.GetHeartbeatData(&hbData);

		//
		// matching device id (address) only
		//
		// LPX_ADDRESS and NDAS_DEVICE_ID are different type
		// so we cannot merely use CompareLpxAddress function here
		//
		XTLC_ASSERT_EQUAL_SIZE(hbData.remoteAddr.Node, m_deviceId.Node);
		if (0 != ::memcmp(hbData.remoteAddr.Node, m_deviceId.Node, sizeof(m_deviceId.Node)))
		{
			return;
		}
		OnHeartbeat(hbData.localAddr, hbData.remoteAddr, hbData.Type, hbData.Version);
	}
	
}

//
// status check event handler
// to reconcile the status 
//
// to be connected status, broadcast packet
// should be received within MAX_ALLOWED_HEARTBEAT_INTERVAL
//
void
CNdasDevice::OnPeriodicCheckup()
{
	InstanceAutoLock autolock(this);

	// No checkup is required if not connected
	if (NDAS_DEVICE_STATUS_CONNECTED != m_status) 
	{
		return;
	}

	// When just a single unit device is mounted,
	// status will not be changed to DISCONNECTED!
	if (_IsAnyUnitDevicesMounted()) 
	{
		return;
	}

	const DWORD MaxHeartbeatInterval = NdasServiceConfig::Get(nscMaximumHeartbeatInterval);
	DWORD dwCurrentTick = ::GetTickCount();
	DWORD dwElapsed = dwCurrentTick - m_dwLastHeartbeatTick;
	if (dwElapsed > MaxHeartbeatInterval) 
	{
		//
		// Do not disconnect the device when the debugger is attached
		//
		if (!NdasServiceConfig::Get(nscDisconnectOnDebug) &&
			::IsDebuggerPresent()) 
		{
		}
		else
		{
			_ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		}
	}
	return;
}

//
// Discovered event handler
//
// on discovered, check the supported hardware type and version,
// and get the device information
//


void
CNdasDevice::OnHeartbeat(
	const LPX_ADDRESS& localAddress,
	const LPX_ADDRESS& remoteAddress,
	UCHAR ucType,
	UCHAR ucVersion)
{
	InstanceAutoLock autolock(this);

	if (NDAS_DEVICE_STATUS_DISABLED == m_status) 
	{
		return;
	}

	//
	// If the local address is different, ignore it.
	// because we may be receiving from multiple interfaces.
	//
	// In case of interface change, as we don't update the heartbeat tick
	// here, device will be disconnected and re-discovered.
	//
	// So, it's safe to ignore non-bound local address
	//
	// However, we can assume the local address is valid
	// only if the status is CONNECTED.
	// 
	if (NDAS_DEVICE_STATUS_CONNECTED == m_status) 
	{
		if (!IsEqualLpxAddress(m_localLpxAddress, localAddress))
		{
			return;
		}
	}

	// Update heartbeat tick
	m_dwLastHeartbeatTick = ::GetTickCount();

	//
	// Version Checking
	//
	if (!IsSupportedHardwareVersion(ucType, ucVersion))
	{
		DBGPRT_ERR(
			_FT("Unsupported NDAS device detected - Type %d, Version %d.\n"), 
			ucType, 
			ucVersion);

		//
		// TODO: EVENTLOG unsupported version detected!
		//
		_SetLastDeviceError(NDAS_DEVICE_ERROR_UNSUPPORTED_VERSION);

		return;
	}

	// Connected status ignores this heartbeat
	// We only updates Last Heartbeat Tick
	if (NDAS_DEVICE_STATUS_CONNECTED == m_status ||
		NDAS_DEVICE_STATUS_CONNECTING == m_status)
	{
		return;
	}

	// Now the device is in DISCONNECTED status

	// Failure Count to prevent possible locking of the service
	// because of the discover failure timeout
	DWORD dwMaxHeartbeatFailure = NdasServiceConfig::Get(nscHeartbeatFailLimit);
	if (m_dwCommFailureCount >= dwMaxHeartbeatFailure) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE
		//
		_SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE);
		return;
	}

	DBGPRT_INFO(_FT("Discovered %s at local %s.\n"), 
		CLpxAddress(remoteAddress).ToString(), 
		CLpxAddress(localAddress).ToString());

	BOOL fSuccess = _GetDeviceInfo(localAddress, remoteAddress);

	if (!fSuccess) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_FAILED
		//
		_SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		++m_dwCommFailureCount;
		return;
	}

	if (!this->UpdateStats())
	{
		_SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		++m_dwCommFailureCount;
		return;
	}

	_SetLastDeviceError(NDAS_DEVICE_ERROR_NONE);
	m_dwCommFailureCount = 0;

	//////////////////////////////////////////////////////////////////////////
	// Dummy 
	_ChangeStatus(NDAS_DEVICE_STATUS_CONNECTING);

	//////////////////////////////////////////////////////////////////////////
	// Status should be changed to CONNECTED before creating Unit Devices
	_ChangeStatus(NDAS_DEVICE_STATUS_CONNECTED);

	for (DWORD i = 0; i < m_dstat.NumberOfUnitDevices; i++) 
	{
		if (m_dstat.UnitDevices[i].IsPresent)
		{
			// we can ignore unit device creation errors here
			CNdasUnitDevicePtr pUnitDevice = _CreateUnitDevice(i);
			if (pUnitDevice)
			{
				m_unitDevices.push_back(pUnitDevice);
				RegisterUnitDevice()(pUnitDevice);
			}
		}
	}


	return;
}

void 
CNdasDevice::OnUnitDeviceUnmounted(CNdasUnitDevicePtr pUnitDevice)
{
	InstanceAutoLock autolock(this);
	// When just a single unit device is mounted,
	// status will not be changed to DISCONNECTED!
	if (!UpdateStats())
	{
		if (!_IsAnyUnitDevicesMounted()) 
		{
			_ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		}
	}
}

BOOL
CNdasDevice::UpdateStats()
{
	InstanceAutoLock autolock(this);

	LPSOCKET_ADDRESS_LIST lpBindAddrList = 
		CreateLpxSocketAddressList(&m_localLpxAddress);
	if (NULL == lpBindAddrList)
	{
		DBGPRT_ERR_EX(_FT("CreateLpxSocketAddressList failed: "));
		return FALSE;
	}
	XTL::AutoProcessHeap ah = lpBindAddrList;

	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.Address.DeviceId = this->GetDeviceId();
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
	ci.OEMCode = this->GetOemCode();
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.UnitNo = 0; /* Use 0 for discover */
	ci.BindingSocketAddressList = lpBindAddrList;

	NDAS_DEVICE_STAT dstat = {0};
	dstat.Size = sizeof(NDAS_DEVICE_STAT);

	BOOL fSuccess = NdasCommGetDeviceStat(&ci, &dstat);
	if (!fSuccess)
	{
		DBGPRT_ERR_EX(_FT("NdasCommGetDeviceStat failed: "));
		return FALSE;
	}

	m_dstat = dstat;

	return TRUE;
}

BOOL 
CNdasDevice::_GetDeviceInfo(
	const LPX_ADDRESS& localAddress, 
	const LPX_ADDRESS& remoteAddress)
{
	InstanceAutoLock autolock(this);

	//
	// Get Device Information
	//

	// Create a Socket Address List from SOCKADDR_LPX
	LPSOCKET_ADDRESS_LIST lpBindAddrList = CreateLpxSocketAddressList(&localAddress);

	if (NULL == lpBindAddrList)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}

	// Set to automatic cleanup for the heap pointer
	XTL::AutoProcessHeap autoHeapPtr = lpBindAddrList;

	// Connection information
	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
	ci.OEMCode = this->GetOemCode();
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.UnitNo = 0;
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	LpxAddressToNdasDeviceId(&ci.Address.DeviceId, &remoteAddress);
	ci.BindingSocketAddressList = lpBindAddrList;

	// Connect to NDAS device (discover mode)
	AutoNdasHandle hNdas = ::NdasCommConnect(&ci);
	if (NULL == (HNDAS) hNdas)
	{
		DBGPRT_ERR_EX(_FT("Discovery failed: "));
		_SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		return FALSE;
	}

	// On discover login, only GetDeviceInfo is available

	//
	// Discover
	//
	NDAS_DEVICE_HARDWARE_INFO dinfo = {0};
	dinfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
	BOOL fSuccess = NdasCommGetDeviceHardwareInfo(hNdas, &dinfo);
	if (!fSuccess) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_FAILED
		//
		DBGPRT_ERR_EX(_FT("NdasCommGetDeviceHardwareInfo failed: "));
		_SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		return FALSE;
	}

	m_hardwareInfo = dinfo;
	m_localLpxAddress = localAddress;
	m_remoteLpxAddress = remoteAddress;

	return TRUE;
}

void 
CNdasDevice::GetHardwareInfo(
	NDAS_DEVICE_HARDWARE_INFO& hardwareInfo)
{
	InstanceAutoLock autolock(this);
	hardwareInfo = m_hardwareInfo;
}

const NDAS_DEVICE_HARDWARE_INFO&
CNdasDevice::GetHardwareInfo()
{
	return m_hardwareInfo;
}

DWORD
CNdasDevice::GetMaxTransferBlocks()
{
	InstanceAutoLock autolock(this);
	return m_hardwareInfo.MaximumTransferBlocks;
}

DWORD
CNdasDevice::GetUnitDeviceCount()
{
	InstanceAutoLock autolock(this);
	return m_unitDevices.size();
}

//
// get last error of the device
//
NDAS_DEVICE_ERROR
CNdasDevice::GetLastDeviceError()
{
	InstanceAutoLock autolock(this);
	return m_lastError;
}

//
// set last error of the device
//
void
CNdasDevice::_SetLastDeviceError(NDAS_DEVICE_ERROR deviceError)
{
	InstanceAutoLock autolock(this);
	m_lastError = deviceError;
}

void
CNdasDevice::_DestroyAllUnitDevices()
{
	InstanceAutoLock autolock(this);
	// Status cannot be changed to DISCONNECTED 
	// if any of unit devices are mounted.
	std::for_each(
		m_unitDevices.begin(), m_unitDevices.end(),
		UnregisterUnitDevice());
	m_unitDevices.clear();
}

CNdasUnitDevicePtr
CNdasDevice::_CreateUnitDevice(DWORD UnitNo)
{
	InstanceAutoLock autolock(this);

	XTLENSURE_RETURN_T(UnitNo < MAX_NDAS_UNITDEVICE_COUNT, CNdasUnitDeviceNullPtr);

	CNdasUnitDevicePtr pExistingUnitDevice = GetUnitDevice(UnitNo);
	if (CNdasUnitDeviceNullPtr != pExistingUnitDevice)
	{
		XTLASSERT(FALSE && "Unit Device already exists");
		return CNdasUnitDeviceNullPtr;
	}

	if (!m_dstat.UnitDevices[UnitNo].IsPresent)
	{
		DBGPRT_WARN(_FT("HWINFO does not contain unit %d.\n"), UnitNo);
		return CNdasUnitDeviceNullPtr;
	}

	DWORD dwMaxFailure = NdasServiceConfig::Get(nscUnitDeviceIdentifyRetryMax);
	DWORD dwInterval = NdasServiceConfig::Get(nscUnitDeviceIdentifyRetryGap);

	CNdasUnitDevicePtr pUnitDevice;

	for (DWORD i = 0; i < dwMaxFailure + 1; ++i)
	{
		CNdasUnitDeviceCreator udCreator(shared_from_this(), UnitNo);
		pUnitDevice = CNdasUnitDevicePtr( udCreator.CreateUnitDevice() );
		if (pUnitDevice) 
		{
			break;
		}
		::Sleep(dwInterval);
		DBGPRT_ERR_EX(_FT("Creating a unit device instance failed (%d out of %d): "),
			i, dwMaxFailure);
	}

	return pUnitDevice;
}

BOOL
CNdasDevice::InvalidateUnitDevice(DWORD UnitNo)
{
	InstanceAutoLock autolock(this);

	DBGPRT_INFO(_FT("%s: Invalidating Unit Device %d\n"), ToString(), UnitNo);

	if (UnitNo >= MAX_NDAS_UNITDEVICE_COUNT) 
	{
		DBGPRT_INFO(_T("%s: Invalid Unit No: %d\n"), ToString(), UnitNo);
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (NDAS_DEVICE_STATUS_CONNECTED != GetStatus()) 
	{
		DBGPRT_INFO(_T("%s: Non-connected device ignored\n"), ToString());
		return FALSE;
	}

	BOOL fSuccess = _GetDeviceInfo(m_localLpxAddress, m_remoteLpxAddress);
	if (!fSuccess) 
	{
		if (!_IsAnyUnitDevicesMounted())
		{
			XTLTRACE_ERR("%ws: Changing to DISCONNECTED.\n", ToString());
			_ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		}
		return FALSE;
	}

	CNdasUnitDevicePtr pUnitDevice = GetUnitDevice(UnitNo);
	if (pUnitDevice)
	{
		if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus())
		{
			// unit device is mounted, we cannot destroy this unit device
			return FALSE;
		}
		// Destroy Unit Device
		CNdasUnitDeviceVector::iterator itr = std::find(
			m_unitDevices.begin(), m_unitDevices.end(),
			pUnitDevice);
		XTLASSERT(m_unitDevices.end() != itr);
		if (m_unitDevices.end() != itr)
		{
			UnregisterUnitDevice()(*itr);
			m_unitDevices.erase(itr);
		}
	}

	if (m_dstat.UnitDevices[UnitNo].IsPresent)
	{
		// Create a new unit device
		CNdasUnitDevicePtr pUnitDevice = _CreateUnitDevice(UnitNo);
		if (pUnitDevice) 
		{
			m_unitDevices.push_back(pUnitDevice);
			RegisterUnitDevice()(pUnitDevice);
		}
		{
			DBGPRT_ERR_EX(_FT("%s: Creating unit device (%d) failed: "), ToString(), UnitNo);
			return FALSE;
		}
		DBGPRT_INFO(_FT("%s: Unit Device (%d) recreated\n"), ToString(), UnitNo);
	}

	return TRUE;
}

bool
CNdasDevice::_IsAnyUnitDevicesMounted()
{
	InstanceAutoLock autolock(this);

	bool result = m_unitDevices.end() != 
		std::find_if(
			m_unitDevices.begin(), m_unitDevices.end(),
			UnitDeviceStatusEquals(NDAS_UNITDEVICE_STATUS_MOUNTED));

	return result;
}

void 
CNdasDevice::GetStats(NDAS_DEVICE_STAT& dstat)
{
	InstanceAutoLock autolock(this);
	dstat = m_dstat;
}

const NDAS_DEVICE_STAT& 
CNdasDevice::GetStats()
{
	return m_dstat;
}
