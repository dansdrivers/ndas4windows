/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <queue>
#include "ndas/ndastypeex.h"
#include "ndas/ndastype_str.h"
#include "ndas/ndasmsg.h"

#include "ndasdev.h"
#include "ndasunitdev.h"

#include "ndasdevhb.h"
#include "ndasinstman.h"

#include "autores.h"
#include "task.h"

#include "ndascfg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasunitdevfactory.h"

#include "lsbusioctl.h"
#include "ndasobjs.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASDEV
#include "xdebug.h"

class CNdasEventMonitor;

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDevice
//
//////////////////////////////////////////////////////////////////////////

ULONG
CNdasDevice::AddRef()
{
	ximeta::CAutoLock autolock(this);

	ULONG ulCount = ximeta::CExtensibleObject::AddRef();
	DBGPRT_INFO(_FT("%s: %u\n"), ToString(), ulCount);
	return ulCount;
}

ULONG
CNdasDevice::Release()
{
	{
		ximeta::CAutoLock autolock(this);
		DBGPRT_INFO(_FT("%s\n"), ToString());
	}
	ULONG ulCount = ximeta::CExtensibleObject::Release();
	//
	// After release there should be no member function calls!
	// The above block is to prevent autolock's dtor be called after this line
	//
	DBGPRT_INFO(_FT("RefCount=%u\n"), ulCount);
	return ulCount;
}

//
// constructor
//

CNdasDevice::CNdasDevice(
	DWORD dwSlotNo, 
	CONST NDAS_DEVICE_ID& deviceId,
	BOOL fVolatile,
	BOOL fAutoRegistered) :
	m_status(NDAS_DEVICE_STATUS_DISABLED),
	m_lastError(NDAS_DEVICE_ERROR_NONE),
	m_dwSlotNo(dwSlotNo),
	m_deviceId(deviceId),
	m_grantedAccess(0x00000000L),
	m_dwLastHeartbeatTick(0),
	m_dwCommFailureCount(0),
	m_fAutoRegistered(fAutoRegistered),
	m_fVolatile(fVolatile)
{
	m_szDeviceName[0] = TEXT('\0');

	::ZeroMemory(
		&m_hwInfo, 
		sizeof(HARDWARE_INFO));

	::ZeroMemory(
		m_pUnitDevices, 
		sizeof(CNdasUnitDevice*) * MAX_NDAS_UNITDEVICE_COUNT);

	HRESULT hr = ::StringCchPrintf(
		m_szCfgContainer, 
		30, 
		TEXT("Devices\\%04d"), 
		dwSlotNo);

	_ASSERT(SUCCEEDED(hr));

	//
	// After we allocate m_szCfgContainer, we can use SetConfigValue
	//
	if (fAutoRegistered)
	{
		(VOID) SetConfigValue(_T("AutoRegistered"), fAutoRegistered);
	}
	else
	{
		(VOID) DeleteConfigValue(_T("AutoRegistered"));		
	}

	DBGPRT_TRACE(_FT("ctor: %s\n"), ToString());
}

//
// destructor
//

CNdasDevice::~CNdasDevice()
{
	ximeta::CAutoLock autolock(this);

	BOOL fSuccess = DestroyAllUnitDevices();
	_ASSERTE(fSuccess);

	DBGPRT_TRACE(_FT("dtor: %s\n"), ToString());
}

//
// initializer
//

BOOL
CNdasDevice::Initialize()
{
	// ximeta::CAutoLock autolock(this);
	return TRUE;
}

//
// Device Name Setter
//

VOID
CNdasDevice::SetName(LPCTSTR szName)
{
	ximeta::CAutoLock autolock(this);

	HRESULT hr = ::StringCchCopy(
		m_szDeviceName, 
		MAX_NDAS_DEVICE_NAME_LEN, 
		szName);

	_ASSERTE(SUCCEEDED(hr));

	(VOID) SetConfigValue(_T("DeviceName"), szName);

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
	(VOID) pEventPublisher->DevicePropertyChanged(m_dwSlotNo);
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

VOID
CNdasDevice::ChangeStatus(NDAS_DEVICE_STATUS newStatus)
{
	ximeta::CAutoLock autolock(this);

	if (m_status == newStatus) {
		return;
	}

	NDAS_DEVICE_STATUS oldStatus = m_status;

	//
	// clear failure count for every status change
	//
	m_dwCommFailureCount = 0;

	switch (newStatus) {
	case NDAS_DEVICE_STATUS_DISABLED:
		{
			pGetNdasDeviceHeartbeatListner()->Detach(this);
			pGetNdasEventMonitor()->Detach(this);
			DestroyAllUnitDevices();
		}
		break;
	case NDAS_DEVICE_STATUS_CONNECTED:
		{
			pGetNdasEventMonitor()->Attach(this);
		}
		break;
	case NDAS_DEVICE_STATUS_DISCONNECTED:
		{
			//
			// Detaching from the Monitor will be done at OnStatusCheck
			// by returning TRUE to detach this device from the monitor
			//
			pGetNdasDeviceHeartbeatListner()->Attach(this);
			DestroyAllUnitDevices();
		}
		break;
	default:
		_ASSERTE(FALSE);
	}

	DBGPRT_INFO(_FT("%s status changed %s to %s\n"),
		ToString(),
		NdasDeviceStatusString(m_status),
		NdasDeviceStatusString(newStatus));

	m_status = newStatus;

	(VOID) pGetNdasEventPublisher()->
		DeviceStatusChanged(m_dwSlotNo, oldStatus, newStatus);

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

CONST NDAS_DEVICE_ID&
CNdasDevice::GetDeviceId()
{
	return m_deviceId;
}

//
// get slot number of the NDAS device
// (don't be confused with logical device slot number.)
//

DWORD 
CNdasDevice::GetSlotNo()
{
	return m_dwSlotNo;
}

//
// get the device name
// returned buffer is valid only while the instance is valid
// 

LPCTSTR 
CNdasDevice::GetName()
{
	return m_szDeviceName;
}

//
// get granted (registered) access permission
//

ACCESS_MASK 
CNdasDevice::GetGrantedAccess()
{
	return m_grantedAccess;
}

//
// set granted access permission
// use this to change the (valid) access of the registered device
//

VOID
CNdasDevice::SetGrantedAccess(ACCESS_MASK access)
{
	ximeta::CAutoLock autolock(this);

	// only GENERIC_READ and GENERIC_WRITE are acceptable
	m_grantedAccess = (access & (GENERIC_READ | GENERIC_WRITE));

	const DWORD cbData = sizeof(m_grantedAccess) + sizeof(m_deviceId);
	BYTE lpbData[cbData] = {0};

	::CopyMemory(lpbData, &m_grantedAccess, sizeof(m_grantedAccess));
	::CopyMemory(lpbData + sizeof(m_grantedAccess), &m_deviceId, sizeof(m_deviceId));

	(VOID) SetConfigValueSecure(_T("GrantedAccess"), lpbData, cbData);
	(VOID) pGetNdasEventPublisher()->DevicePropertyChanged(m_dwSlotNo);
}

//
// on CONNECTED status only
// the local LPX address where the NDAS device was found
//

CONST LPX_ADDRESS&
CNdasDevice::GetLocalLpxAddress()
{
	return m_localLpxAddress;
}

//
// remote LPX address of this device
// this may be different than the actual device ID. (Proxied?)
//

CONST LPX_ADDRESS&
CNdasDevice::GetRemoteLpxAddress()
{
	return m_remoteLpxAddress;
}

//
// hardware type discovered
//

UCHAR 
CNdasDevice::GetHWType()
{
	ximeta::CAutoLock autolock(this);
	return m_hwInfo.ucType;
}

//
// hardware version discovered
//

UCHAR 
CNdasDevice::GetHWVersion()
{
	ximeta::CAutoLock autolock(this);
	return m_hwInfo.ucVersion;
}

//
// hardware password to be used to communicate with the device
// this function is to provide an unified access to the password
// which may be different by Hardware Types or Versions
// 

UINT64
CNdasDevice::GetHWPassword()
{
	ximeta::CAutoLock autolock(this);
	//
	// Sample hardware address starts with 00:F0:0F
	//
	//
	// Rutter Tech NDAS Device address range:
	//
	// 00:0B:D0:20:00:00 - 00:0B:D0:21:FF:FF
	//
	static const UCHAR DevicePassword_V1_Sample[8] = 
		{0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00};

	static const UCHAR	DevicePassword_V1[8] = 
		{0xbb,0xea,0x30,0x15,0x73,0x50,0x4a,0x1f};

	static const UCHAR	DevicePassword_V1_Rutter[8] = 
		{0xbb,0xea,0x30,0x15,0x73,0x50,0x4a,0x1f};

	UINT64 ui64password;

	const UCHAR* pPassword = DevicePassword_V1;

	if (0x00 == m_deviceId.Node[0] &&
		0xF0 == m_deviceId.Node[1] &&
		0x0F == m_deviceId.Node[2])
	{
		pPassword = DevicePassword_V1_Sample;
	} 
	else if (0x00 == m_deviceId.Node[0] &&
		0x0B == m_deviceId.Node[1] &&
		0xD0 == m_deviceId.Node[2] &&
		(0x20 == (m_deviceId.Node[3] & 0xFE)))
	{
		pPassword = DevicePassword_V1_Rutter;
	}

	::CopyMemory(&ui64password, pPassword, sizeof(UCHAR) * 8);

	// const UINT64 DevicePassword_V1 = 0xbbea301573504a1f;
	if (GetHWType() == 0 && GetHWVersion() == 0 ) {
		return ui64password;
	} else if (GetHWType() == 0 && GetHWVersion() == 1) {
		return ui64password;
	}
	// default
	return ui64password;
}

//
// get a unit device of a given unit number
// returns NULL if no unit device is available in a given unit.
//

CNdasUnitDevice*
CNdasDevice::GetUnitDevice(DWORD dwUnitNo)
{
	ximeta::CAutoLock autolock(this);
	_ASSERTE(dwUnitNo < MAX_NDAS_UNITDEVICE_COUNT);

	CNdasUnitDevice* pUnitDevice = 
		(dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) ?
		NULL : m_pUnitDevices[dwUnitNo];
	if (NULL != pUnitDevice)
	{
		pUnitDevice->AddRef();
	}
	return pUnitDevice;
}

//
// Enable/disable NDAS device
// returns TRUE if enable/disable is done successfully
// (now always returns TRUE)
//

BOOL
CNdasDevice::Enable(BOOL bEnable)
{
	ximeta::CAutoLock autolock(this);

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
		ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
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
		if (IsAnyUnitDevicesMounted())
		{
			::SetLastError(NDASHLPSVC_ERROR_CANNOT_DISABLE_MOUNTED_DEVICE);
			return FALSE;
		}

		//
		// DISCONNECTED/CONNECTED -> DISABLED
		//
		ChangeStatus(NDAS_DEVICE_STATUS_DISABLED);
	}

	(VOID) SetConfigValue(_T("Enabled"), bEnable);

	//
	// Clear Device Error
	//
	SetLastDeviceError(NDAS_DEVICE_ERROR_NONE);

	return TRUE;
}

//
// Subject to the Heartbeat Listener object
//

void 
CNdasDevice::Update(ximeta::CSubject* pChangedSubject)
{
	ximeta::CAutoLock autolock(this);

	CNdasDeviceHeartbeatListener* pListener = pGetNdasDeviceHeartbeatListner();

	//
	// Ignore other than subscribed heartbeat listener
	//
	if (pListener == pChangedSubject) 
	{

		NDAS_DEVICE_HEARTBEAT_DATA hbData;

		pListener->GetHeartbeatData(&hbData);

		//
		// matching device id (address) only
		//
		// LPX_ADDRESS and NDAS_DEVICE_ID are different type
		// so we cannot merely use CompareLpxAddress function here
		//
		if (0 == ::memcmp(
			hbData.remoteAddr.Node, 
			m_deviceId.Node, 
			sizeof(m_deviceId.Node[0]) * 6))
		{
			OnDiscovered(
				hbData.localAddr,
				hbData.remoteAddr,
				hbData.ucType,
				hbData.ucVersion);
		}

	}
	
}

//
// status check event handler
// to reconcile the status 
//
// to be connected status, broadcast packet
// should be received within MAX_ALLOWED_HEARTBEAT_INTERVAL
// 
// returns TRUE to detach from the monitor
// FALSE otherwise.
//
BOOL
CNdasDevice::OnStatusCheck()
{
	ximeta::CAutoLock autolock(this);

	//
	// Only when the device is connected!
	//
	if (NDAS_DEVICE_STATUS_CONNECTED != m_status) {
//		_ASSERTE(FALSE && "OnStatusCheck should be called when connected!");
		DBGPRT_WARN(_FT("OnStatusCheck is called on connected. Detaching.\n"));
		// Detach from the monitor
		return TRUE;
	}

	DWORD dwCurrentTick = ::GetTickCount();
	DWORD dwElapsed = dwCurrentTick - m_dwLastHeartbeatTick;

	if (dwElapsed > MAX_ALLOWED_HEARTBEAT_INTERVAL) {

		//
		// When just a single unit device is mounted,
		// status will not be changed to DISCONNECTED!
		//

		if (IsAnyUnitDevicesMounted()) {
			return FALSE;
		}

		//
		// Do not disconnect the device when the debugger is attached
		//
		if (::IsDebuggerPresent()) {
			return FALSE;
		}

		BOOL fSuccess = DestroyAllUnitDevices();
		if (!fSuccess) {
			return FALSE;
		}

		ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		return TRUE;
	}

	return FALSE;
}

//
// Discovered event handler
//
// on discovered, check the supported hardware type and version,
// and get the device information
//


BOOL 
CNdasDevice::OnDiscovered(
	const LPX_ADDRESS& localAddress,
	const LPX_ADDRESS& remoteAddress,
	UCHAR ucType,
	UCHAR ucVersion)
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_DEVICE_STATUS_DISABLED == m_status) 
	{
		return FALSE;
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
			return FALSE;
		}
	}

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
		SetLastDeviceError(NDAS_DEVICE_ERROR_UNSUPPORTED_VERSION);

		return FALSE;
	}

	// Update heartbeat tick
	m_dwLastHeartbeatTick = ::GetTickCount();

	// Connected status ignores this heartbeat
	// We only updates Last Heartbeat Tick
	if (NDAS_DEVICE_STATUS_CONNECTED == m_status)
	{
		//
		//BUGBUG!!!
		//
		// TODO: Dangling case not handled!
		//
		for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++)
		{
			if(NULL != m_pUnitDevices[i])
			{
				m_pUnitDevices[i]->SetFault(FALSE);
				m_pUnitDevices[i]->m_pLogicalDevice->Recover();
			}
		}
		return FALSE;
	}

	DWORD maxFailure = ndascfg::sys::MaxHeartbeatFailure::GetValue();
	// Now the device is in DISCONNECTED status

	// Failure Count to prevent possible locking of the service
	// because of the discover failure timeout
	static const DWORD MAX_HEARTBEAT_FAILURE_DEFAULT = 10;
	static const DWORD MAX_HEARTBEAT_FAILURE_MIN = 1;
	static const DWORD MAX_HEARTBEAT_FAILURE_MAX = 0xFFFF;

	DWORD dwMaxHeartbeatFailure = MAX_HEARTBEAT_FAILURE_DEFAULT;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		_T("ndassvc"),
		_T("MaxHeartbeatFailure"),
		&dwMaxHeartbeatFailure);

	if (!fSuccess ||
		MAX_HEARTBEAT_FAILURE_MIN < dwMaxHeartbeatFailure ||
		dwMaxHeartbeatFailure > MAX_HEARTBEAT_FAILURE_MAX)
	{
		dwMaxHeartbeatFailure = MAX_HEARTBEAT_FAILURE_DEFAULT;
	}

	if (m_dwCommFailureCount >= dwMaxHeartbeatFailure) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE
		//
		SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE);
		return FALSE;
	}

	DBGPRT_INFO(_FT("Discovered %s at local %s.\n"), 
		CLpxAddress(remoteAddress).ToString(), 
		CLpxAddress(localAddress).ToString());

	fSuccess = GetDeviceInfo(
		localAddress, 
		remoteAddress, 
		ucType, 
		ucVersion);

	if (!fSuccess) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_FAILED
		//
		SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		++m_dwCommFailureCount;
		return FALSE;
	}

	SetLastDeviceError(NDAS_DEVICE_ERROR_NONE);
	m_dwCommFailureCount = 0;

	// Status should be changed to CONNECTED before creating Unit Devices
	ChangeStatus(NDAS_DEVICE_STATUS_CONNECTED);

	// Not necessary, but just to make sure that there is no instances
	fSuccess = DestroyAllUnitDevices();
	_ASSERTE(fSuccess);

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++) 
	{
		// we can ignore unit device creation errors here
		(VOID) CreateUnitDevice(i);
	}


	return TRUE;
}

BOOL
CNdasDevice::UpdateDeviceInfo()
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_DEVICE_STATUS_CONNECTED != m_status) {
		return FALSE;
	}

	BOOL fSuccess = GetDeviceInfo(
		m_localLpxAddress, 
		m_remoteLpxAddress, 
		GetHWType(), 
		GetHWVersion());

	if (!fSuccess) 
	{
		BOOL fDisconnectable = DestroyAllUnitDevices();
		if (fDisconnectable)
		{
			DBGPRT_INFO(_FT("%s: Changing to DISCONNECTED.\n"), ToString());
			ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		} 
		else 
		{
			DBGPRT_INFO(_T("%s: Some unit devices are mounted!\n"), ToString());
		}

		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasDevice::GetDeviceInfo(
	const LPX_ADDRESS& localAddress, 
	const LPX_ADDRESS& remoteAddress,
	UCHAR ucType,
	UCHAR ucVersion)
{
	ximeta::CAutoLock autolock(this);

	//
	// Get Device Information
	//
	LANSCSI_PATH lspath = {0};

	// Discover does not need any user id
	lspath.iUserID = 0x0000;
	lspath.iPassword = GetHWPassword();

	lspath.HWType = ucType;
	lspath.HWVersion = ucVersion;
	lspath.HWProtoType = ucType;
	lspath.HWProtoVersion = 
		(LANSCSIIDE_VERSION_1_1 == ucVersion) ?
		LSIDEPROTO_VERSION_1_1 : 
		LSIDEPROTO_VERSION_1_0;

	//
	// create an LPX (auto) socket
	//

	AutoSocket autoSock = CreateLpxConnection(&remoteAddress, &localAddress);

	//
	// autoSock will close the handle when it goes out of scope
	//

	if (INVALID_SOCKET == (SOCKET) autoSock) 
	{
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_LPX_SOCKET_FAILED
		//
		DBGPRT_ERR_EX(_FT("Error Create Connection: "));
		SetLastDeviceError(NDAS_DEVICE_ERROR_LPX_SOCKET_FAILED);
		return FALSE;
	}

	//
	// Discover
	//
	lspath.connsock = autoSock;
	INT iResult = Discovery(&lspath);
	if (0 != iResult) {
		//
		// TODO: EVENTLOG NDAS_DEVICE_ERROR_DISCOVER_FAILED
		//
		DBGPRT_ERR_EX(_FT("Discovery failed! - returned (%d) : "), iResult);
		SetLastDeviceError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		return FALSE;
	}

	//
	// If discovering succeeded, set pController's
	// LANSCSI path to discovered LANSCSI path
	//
	m_hwInfo.nMaxRequestBlocks = lspath.iMaxBlocks;
	m_hwInfo.nSlots = lspath.iNumberofSlot;
	m_hwInfo.nTargets = lspath.iNRTargets;
	m_hwInfo.nMaxTargets = lspath.iMaxTargets;
	m_hwInfo.nMaxLUs = lspath.iMaxLUs;

	::CopyMemory(&m_localLpxAddress, &localAddress, sizeof(LPX_ADDRESS));
	::CopyMemory(&m_remoteLpxAddress, &remoteAddress, sizeof(LPX_ADDRESS));

	//
	// actual hardware type and version is filled in lspath
	//

	m_hwInfo.ucType = lspath.HWType;
	m_hwInfo.ucVersion = lspath.HWVersion;

	//
	// at discovery only lspath.PerTarget[i].NORWHost and NOROHost are valid.
	//

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++) 
	{
		m_fUnitDevicePresent[i] = lspath.PerTarget[i].bPresent;
		if (NULL != m_pUnitDevices[i]) 
		{
			m_pUnitDevices[i]->SetHostUsageCount(
				lspath.PerTarget[i].NRROHost,
				lspath.PerTarget[i].NRRWHost);
		}

	}

	return TRUE;
}

CONST CNdasDevice::HARDWARE_INFO&
CNdasDevice::GetHardwareInfo()
{
	return m_hwInfo;
}

DWORD
CNdasDevice::GetMaxRequestBlocks()
{
	return m_hwInfo.nMaxRequestBlocks;
}

DWORD
CNdasDevice::GetUnitDeviceCount()
{
	ximeta::CAutoLock autolock(this);

	DWORD n = 0;
	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {
		if (NULL != m_pUnitDevices[i]) {
			++n;
		}
	}
	return n;
}

//
// get last error of the device
//
NDAS_DEVICE_ERROR
CNdasDevice::GetLastDeviceError()
{
	return m_lastError;
}

//
// set last error of the device
//
VOID
CNdasDevice::SetLastDeviceError(NDAS_DEVICE_ERROR deviceError)
{
	ximeta::CAutoLock autolock(this);
	m_lastError = deviceError;
}

BOOL
CNdasDevice::IsAutoRegistered()
{
	return m_fAutoRegistered;
}


BOOL
CNdasDevice::IsVolatile()
{
	return m_fVolatile;
}

//
// Return value: 
//  TRUE if status can be changed to DISCONNECTED,
//  FALSE if there are any MOUNTED unit devices left.
//
BOOL
CNdasDevice::DestroyAllUnitDevices()
{
	// Status cannot be changed to DISCONNECTED 
	// if any of unit devices are mounted.
	
	BOOL fSuccess = TRUE;
	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) 
	{
		BOOL fDisconnectable = DestroyUnitDevice(i);
		if (!fDisconnectable) {
			fSuccess = FALSE;
		}
	}

	return fSuccess;
}

BOOL
CNdasDevice::DestroyUnitDevice(DWORD dwUnitNo)
{
	_ASSERTE(dwUnitNo < MAX_NDAS_UNITDEVICE_COUNT);
	if (dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) {
		return FALSE;
	}

	CNdasUnitDevice* pUnitDevice = m_pUnitDevices[dwUnitNo];
	if (NULL != pUnitDevice)
	{
		if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus())
		{
			// any single unit device is mounted, we cannot 
			// destroy this unit device
			return FALSE;
		}
		
		m_pUnitDevices[dwUnitNo] = NULL;
		BOOL fSuccess = pUnitDevice->UnregisterFromLDM();
		_ASSERTE(fSuccess);
		pUnitDevice->Release();
	}

	return TRUE;
}

BOOL
CNdasDevice::CreateUnitDevice(DWORD dwUnitNo)
{
	_ASSERTE(dwUnitNo < MAX_NDAS_UNITDEVICE_COUNT);
	if (dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) 
	{
		return FALSE;
	}

	_ASSERTE(NULL == m_pUnitDevices[dwUnitNo]);

	if (!m_fUnitDevicePresent[dwUnitNo])
	{
		DBGPRT_WARN(_FT("HWINFO does not contain unit %d.\n"), dwUnitNo);
		return FALSE;
	}

	static const DWORD UNITDEVICE_IDENTIFY_FAILURE_RETRY_DEFAULT = 4;
	static const DWORD UNITDEVICE_IDENTIFY_INTERVAL_DEFAULT = 2500;

	DWORD dwMaxFailure = UNITDEVICE_IDENTIFY_FAILURE_RETRY_DEFAULT;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		_T("ndassvc"),
		_T("MaxUnitDeviceIdentifyFailure"),
		&dwMaxFailure);
	if (!fSuccess || 0 == dwMaxFailure)
	{
		dwMaxFailure = UNITDEVICE_IDENTIFY_FAILURE_RETRY_DEFAULT;
	}

	DWORD dwInterval = UNITDEVICE_IDENTIFY_INTERVAL_DEFAULT;
	fSuccess = _NdasSystemCfg.GetValueEx(
		_T("ndassvc"),
		_T("UnitDeviceIdentifyFailureRetryInterval"),
		&dwInterval);
	if (!fSuccess || dwInterval > 300000)
	{
		dwInterval = UNITDEVICE_IDENTIFY_INTERVAL_DEFAULT;
	}

	for (DWORD i = 0; i < dwMaxFailure; ++i)
	{
		CNdasUnitDeviceCreator udCreator(*this, dwUnitNo);
		CNdasUnitDevice* pUnitDevice = udCreator.CreateUnitDevice();
		// CreateUnitDevice already called AddRef

		if (NULL != pUnitDevice) 
		{
			m_pUnitDevices[dwUnitNo] = pUnitDevice;
			pUnitDevice->RegisterToLDM();
			return TRUE;
		}

		::Sleep(dwInterval);

		DBGPRT_ERR_EX(_FT("Creating a unit device instance failed (%d out of %d): "),
			i, dwMaxFailure);
	}

	return FALSE;
}

BOOL
CNdasDevice::InvalidateUnitDevice(DWORD dwUnitNo)
{
	ximeta::CAutoLock autoLock(this);

	DBGPRT_INFO(_FT("%s: Invalidating Unit Device %d\n"), ToString(), dwUnitNo);

	if (dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) 
	{
		DBGPRT_INFO(_T("%s: Invalid Unit No: %d\n"), ToString(), dwUnitNo);
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (NDAS_DEVICE_STATUS_CONNECTED != GetStatus()) 
	{
		DBGPRT_INFO(_T("%s: Non-connected device ignored\n"), ToString());
		return FALSE;
	}

	BOOL fSuccess = UpdateDeviceInfo();
	if (!fSuccess)
	{
		DBGPRT_ERR_EX(_T("%s: Device is not available.\n"), ToString());
		return FALSE;
	}

	fSuccess = DestroyUnitDevice(dwUnitNo);
	if (!fSuccess) 
	{
		DBGPRT_ERR_EX(_FT("%s: Destroying unit device (%d) failed: "), ToString(), dwUnitNo);
		return FALSE;
	}

	fSuccess = CreateUnitDevice(dwUnitNo);
	if (!fSuccess) 
	{
		DBGPRT_ERR_EX(_FT("%s: Creating unit device (%d) failed: "), ToString(), dwUnitNo);
		return FALSE;
	}

	DBGPRT_INFO(_FT("%s: Unit Device (%d) recreated\n"), ToString(), dwUnitNo);

	return TRUE;
}

BOOL
CNdasDevice::IsSupportedHardwareVersion(
	UCHAR ucType, 
	UCHAR ucVersion)
{
	return (ucType == 0 && ucVersion == 0) || 
		(ucType == 0 && ucVersion == 1) ||
		(ucType == 0 && ucVersion == 2);
}

BOOL 
CNdasDevice::IsAnyUnitDevicesMounted()
{
	ximeta::CAutoLock autolock(this);

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) 
	{
		CNdasUnitDevice* pUnitDevice = m_pUnitDevices[i];
		if (NULL != pUnitDevice &&
			NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus())
		{
			return TRUE;
		}
	}
	return FALSE;
}

LPCTSTR
CNdasDevice::ToString()
{
	ximeta::CAutoLock autolock(this);

	HRESULT hr = ::StringCchPrintf(
		m_lpStrBuf, 
		m_cchStrBuf,
		_T("{%03X}%s"),
		m_dwSlotNo, 
		CNdasDeviceId(m_deviceId).ToString());

	return m_lpStrBuf;
}

