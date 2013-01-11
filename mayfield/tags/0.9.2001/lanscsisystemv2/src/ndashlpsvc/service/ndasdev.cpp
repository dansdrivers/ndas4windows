/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <queue>

#include "ndasdev.h"
#include "ndasunitdev.h"

#include "ndasdevhb.h"
#include "ndasinstman.h"

#include "autores.h"
#include "task.h"
// #include "landisk.h"

#include "ndascfg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"

#include "ndastype_str.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASDEV
#include "xdebug.h"

#include "lsbusioctl.h"

class CNdasEventMonitor;
typedef CNdasEventMonitor *PCNdasEventMonitor;

//////////////////////////////////////////////////////////////////////////
//
// Implementation of CNdasDevice
//
//////////////////////////////////////////////////////////////////////////

//
// constructor
//

CNdasDevice::
CNdasDevice(
	DWORD dwSlotNo, 
	NDAS_DEVICE_ID deviceId) :
	m_status(NDAS_DEVICE_STATUS_DISABLED),
	m_lastError(NDAS_DEVICE_ERROR_NONE),
	m_dwSlotNo(dwSlotNo),
	m_deviceId(deviceId),
	m_dwUnitDeviceCount(0),
	m_grantedAccess(0x00000000L),
	m_dwLastHeartbeatTick(0),
	m_dwCommFailureCount(0)
{
	m_szDeviceName[0] = TEXT('\0');
	::ZeroMemory(&m_hwInfo, sizeof(HARDWARE_INFO));

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++) {
		m_unitDevices[i] = NULL;
		m_bUnitDevicePresent[i] = FALSE;
	}

	HRESULT hr = ::StringCchPrintf(m_szCfgContainer, 12, TEXT("Devices\\%d"), dwSlotNo);
	_ASSERT(SUCCEEDED(hr));

	m_szStrBuf[0] = _T('\0');
}

//
// destructor
//

CNdasDevice::
~CNdasDevice()
{
	ximeta::CAutoLock autolock(this);
	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {
		if (NULL != m_unitDevices[i]) {
			delete m_unitDevices[i];
		}
	}

	CNdasInstanceManager::Instance()->GetHBListener()->Detach(this);
}

//
// initializer
//

BOOL
CNdasDevice::
Initialize()
{
	ximeta::CAutoLock autolock(this);
	return TRUE;
}

//
// Device Name Setter
//

BOOL 
CNdasDevice::
SetName(LPCTSTR szName)
{
	ximeta::CAutoLock autolock(this);

	HRESULT hr = ::StringCchCopy(
		m_szDeviceName, 
		MAX_NDAS_DEVICE_NAME_LEN, 
		szName);

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(m_szCfgContainer, TEXT("DeviceName"), szName);
	if (!fSuccess) {
		DPWarningEx(
			_FT("Writing device name entry to the registry failed at %s.\n"), 
			m_szCfgContainer);
	}

	return (SUCCEEDED(hr));
}

//
// Device Name Getter
//

BOOL 
CNdasDevice::
GetName(SIZE_T cchName, LPTSTR lpName)
{
	ximeta::CAutoLock autolock(this);

	HRESULT hr = ::StringCchCopy(
		lpName, 
		cchName, 
		m_szDeviceName);

	return (SUCCEEDED(hr));
}

//
// set status of the device (internal use only)
//

VOID
CNdasDevice::
ChangeStatus(NDAS_DEVICE_STATUS newStatus)
{
	if (m_status == newStatus) {
		return;
	}

	NDAS_DEVICE_STATUS oldStatus = m_status;

	ximeta::CAutoLock autolock(this);

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventMonitor* pMonitor = pInstMan->GetEventMonitor();
	_ASSERTE(NULL != pMonitor);

	CNdasLogicalDeviceManager *pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	//
	// clear failure count for every status change
	//
	m_dwCommFailureCount = 0;

	switch (newStatus) {
	case NDAS_DEVICE_STATUS_DISABLED:
		{
			pMonitor->Detach(this);
		}
		break;
	case NDAS_DEVICE_STATUS_CONNECTED:
		{
			pMonitor->Attach(this);
		}
		break;
	case NDAS_DEVICE_STATUS_DISCONNECTED:
		{
		}
		break;
	default:
		_ASSERTE(FALSE);
	}

	DPInfo(_FT("%s at slot %d status changed %s to %s\n"),
		LPCTSTR(CNdasDeviceId(m_deviceId)),
		m_dwSlotNo,
		NdasDeviceStatusString(m_status),
		NdasDeviceStatusString(newStatus));

	m_status = newStatus;

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	(void) pEventPublisher->DeviceStatusChanged(
		m_dwSlotNo, 
		oldStatus,
		newStatus);

	return;
}

//
// set status of the device
//

NDAS_DEVICE_STATUS 
CNdasDevice::
GetStatus()
{
	return m_status;
}

//
// get last error the device
//

NDAS_DEVICE_ERROR
CNdasDevice::
GetLastError()
{
	return m_lastError;
}

//
// get device id
//

NDAS_DEVICE_ID 
CNdasDevice::
GetDeviceId()
{
	return m_deviceId;
}

//
// get slot number of the NDAS device
// (don't be confused with logical device slot number.)
//

DWORD 
CNdasDevice::
GetSlotNo()
{
	return m_dwSlotNo;
}

//
// get the device name
// returned buffer is valid only while the instance is valid
// 

LPCTSTR 
CNdasDevice::
GetName()
{
	return m_szDeviceName;
}

//
// get granted (registered) access permission
//

ACCESS_MASK 
CNdasDevice::
GetGrantedAccess()
{
	return m_grantedAccess;
}

//
// set granted access permission
// use this to change the (valid) access of the registered device
//

BOOL
CNdasDevice::
SetGrantedAccess(ACCESS_MASK access)
{
	ximeta::CAutoLock autolock(this);

	// only GENERIC_READ and GENERIC_WRITE are acceptable
	m_grantedAccess = (access & (GENERIC_READ | GENERIC_WRITE));

	const DWORD cbData = sizeof(m_grantedAccess) + sizeof(m_deviceId);
	BYTE lpbData[cbData];

	::CopyMemory(lpbData, &m_grantedAccess, sizeof(m_grantedAccess));
	::CopyMemory(lpbData + sizeof(m_grantedAccess), &m_deviceId, sizeof(m_deviceId));

	BOOL fSuccess = _NdasSystemCfg.SetSecureValueEx(
		m_szCfgContainer, 
		TEXT("GrantedAccess"), 
		lpbData,
		cbData);

	if (!fSuccess) {
		DPWarningEx(
			_FT("Writing device name entry to the registry failed at %s.\n"), 
			m_szCfgContainer);
	}

	return TRUE;
}

//
// on CONNECTED status only
// the local LPX address where the NDAS device was found
//

LPX_ADDRESS 
CNdasDevice::
GetLocalLpxAddress()
{
	ximeta::CAutoLock autolock(this);
	return m_localLpxAddress;
}

//
// remote LPX address of this device
// this may be different than the actual device ID. (Proxied?)
//

LPX_ADDRESS 
CNdasDevice::
GetRemoteLpxAddress()
{
	ximeta::CAutoLock autolock(this);
	return m_remoteLpxAddress;
}

//
// hardware type discovered
//

UCHAR 
CNdasDevice::
GetHWType()
{
	return m_hwInfo.ucType;
}

//
// hardware version discovered
//

UCHAR 
CNdasDevice::
GetHWVersion()
{
	return m_hwInfo.ucVersion;
}

//
// hardware password to be used to communicate with the device
// this function is to provide an unified access to the password
// which may be different by Hardware Types or Versions
// 

UINT64
CNdasDevice::
GetHWPassword()
{
	static const UCHAR	DevicePassword_V1[8] = {0xbb,0xea,0x30,0x15,0x73,0x50,0x4a,0x1f} ;
	UINT64 ui64password;
	::CopyMemory(&ui64password, DevicePassword_V1, sizeof(DevicePassword_V1));

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

PCNdasUnitDevice
CNdasDevice::
GetUnitDevice(DWORD dwUnitNo)
{
	ximeta::CAutoLock autolock(this);
	return 
		(dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) ?
		NULL : m_unitDevices[dwUnitNo];
}

//
// Enable/disable NDAS device
// returns TRUE if enable/disable is done successfully
// (now always returns TRUE)
//

BOOL
CNdasDevice::
Enable(BOOL bEnable)
{
	ximeta::CAutoLock autolock(this);
	BOOL bEnabled = bEnable;

	if (bEnable) {

		if (m_status != NDAS_DEVICE_STATUS_DISABLED) {
		} else {
			CNdasInstanceManager::Instance()->GetHBListener()->Attach(this);
			ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		}
	} else {

		if (m_status == NDAS_DEVICE_STATUS_DISABLED) {
		} else {
			CNdasInstanceManager::Instance()->GetHBListener()->Detach(this);
			ChangeStatus(NDAS_DEVICE_STATUS_DISABLED);
		}
	}

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(m_szCfgContainer, TEXT("Enabled"), bEnabled);
	if (!fSuccess) {
		DPWarningEx(
			_FT("Writing device enable status entry to the registry failed at %s.\n"), 
			m_szCfgContainer);
	}

	return TRUE;
}

//
// Subject to the Heartbeat Listener object
//

void 
CNdasDevice::
Update(ximeta::PCSubject pChangedSubject)
{
	PCNdasDeviceHeartbeatListener pListener = 
		CNdasInstanceManager::Instance()->GetHBListener();

	//
	// Ignore other than subscribed heartbeat listener
	//

	if (pListener == pChangedSubject) {

		NDAS_DEVICE_HEARTBEAT_DATA hbData;

		pListener->GetHeartbeatData(&hbData);

		//
		// matching device id (address) only
		//
		// LPX_ADDRESS and NDAS_DEVICE_ID are different type
		// so we cannot merely use CompareLpxAddress function here
		//
		if (hbData.remoteAddr.Node[0] == m_deviceId.Node[0] &&
			hbData.remoteAddr.Node[1] == m_deviceId.Node[1] &&
			hbData.remoteAddr.Node[2] == m_deviceId.Node[2] &&
			hbData.remoteAddr.Node[3] == m_deviceId.Node[3] &&
			hbData.remoteAddr.Node[4] == m_deviceId.Node[4] &&
			hbData.remoteAddr.Node[5] == m_deviceId.Node[5])
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
CNdasDevice::
OnStatusCheck()
{
	ximeta::CAutoLock autolock(this);

	//
	// Only when the device is connected!
	//
	if (NDAS_DEVICE_STATUS_CONNECTED != m_status) {
		_ASSERTE(FALSE && "OnStatusCheck should be called when connected!");
		return FALSE;
	}

	DWORD dwCurrentTick = ::GetTickCount();
	DWORD dwElapsed = dwCurrentTick - m_dwLastHeartbeatTick;

	if (dwElapsed > MAX_ALLOWED_HEARTBEAT_INTERVAL) {
		ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		return TRUE;
	}

	return FALSE;
}

//
// Discovered event handler
//
// called by Notify()
//
// on discovered, check the supported hardware type and version,
// and get the device information
//

BOOL 
CNdasDevice::
OnDiscovered(
	const LPX_ADDRESS& localAddress,
	const LPX_ADDRESS& remoteAddress,
	UCHAR ucType,
	UCHAR ucVersion)
{
	ximeta::CAutoLock autolock(this);

	if (m_status == NDAS_DEVICE_STATUS_DISABLED) {
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
	if (NDAS_DEVICE_STATUS_CONNECTED == m_status) {
		if ( ! IsEqualLpxAddress(m_localLpxAddress, localAddress) ) {
			return FALSE;
		}
	}


	//
	// Version Checking
	//
	// Current supported versions:
	//
	// ucVersion 0 (1.0) 
	// ucVersion 1 (1.1)
	// uvVersion 2 (2.0)

	if (!(
		(ucType == 0 && ucVersion == 0) || 
		(ucType == 0 && ucVersion == 1) ||
		(ucType == 0 && ucVersion == 2)
		))
	{
		// TODO: Event Log unsupport version detected!
		// Unsupported Version

		DPError(_FT("Unsupported NDAS device detected - Type %d, Version %d.\n"), ucType, ucVersion);

		SetLastError(NDAS_DEVICE_ERROR_UNSUPPORTED_VERSION);

		return FALSE;
	}

	//
	// Update heartbeat tick
	//

	m_dwLastHeartbeatTick = ::GetTickCount();

	if (NDAS_DEVICE_STATUS_CONNECTED == m_status) {
		return TRUE;
	}

	//
	// Fetch Unit Device Information to migrate to Connected State
	//

	if (m_dwCommFailureCount >= MAX_ALLOWED_FAILURE_COUNT) {
		SetLastError(NDAS_DEVICE_ERROR_DISCOVER_TOO_MANY_FAILURE);
		return FALSE;
	}

	DPInfo(_FT("Discovered %s at local %s.\n"), 
		LPCTSTR(CLpxAddress(remoteAddress)),
		LPCTSTR(CLpxAddress(localAddress)));

	//
	// Failure Count to prevent possible locking of the service
	// because of the discover failure timeout
	//
	BOOL fSuccess = UpdateDeviceInfo_(localAddress, remoteAddress, ucType, ucVersion);
	if (!fSuccess) {
		SetLastError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		++m_dwCommFailureCount;
		return FALSE;
	}

	m_dwCommFailureCount = 0;

	// We should look up every unit device as there may be only a secondary
	// IDE device in the NDAS device.

	DWORD nUnitDeviceCount = MAX_NDAS_UNITDEVICE_COUNT; // pPath->iNRTargets;
	m_dwUnitDeviceCount = 0;

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++) {

		//
		// recreate unit device
		//
		if (NULL != m_unitDevices[i]) {
			if (m_unitDevices[i]->GetStatus() == NDAS_UNITDEVICE_STATUS_MOUNTED) {
				//
				// already mounted unit devices should remain intact
				//

				//
				// reconcile its status only
				//
				++m_dwUnitDeviceCount;
				continue;
			}
		}

		//
		// not-mounted unit devices can be re-instantiated.
		//
		if (NULL != m_unitDevices[i]) {
			delete m_unitDevices[i];
		}

		if (!m_bUnitDevicePresent[i]) {
			m_unitDevices[i] = NULL;
			continue;
		}

		CNdasUnitDeviceCreator udCreator(this, i);
		PCNdasUnitDevice pUnitDevice = 	udCreator.CreateUnitDevice();

		if (NULL == pUnitDevice) {
			DPErrorEx(_FT("Creating a unit device instance failed: "));
			continue;
		}

		m_unitDevices[i] = pUnitDevice;
		++m_dwUnitDeviceCount;

		pUnitDevice->RegisterToLDM();

	}

	ChangeStatus(NDAS_DEVICE_STATUS_CONNECTED);

	return TRUE;
}

BOOL 
CNdasDevice::
UpdateDeviceInfo_(
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
	lspath.HWProtoVersion = (ucVersion == LANSCSIIDE_VERSION_1_1) ?
		LSIDEPROTO_VERSION_1_1 : LSIDEPROTO_VERSION_1_0;

	//
	// create an LPX (auto) socket
	//

	AutoSocket autoSock = CreateLpxConnection(&remoteAddress, &localAddress);

	//
	// autosock will close the handle when it goes out of scope
	//

	if (INVALID_SOCKET == (SOCKET) autoSock) {
		SetLastError(NDAS_DEVICE_ERROR_LPX_SOCKET_FAILED);
		ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		DPErrorExWsa(_FT("Error Create Connection: "));
		return FALSE;
	}

	//
	// Discover
	//
	lspath.connsock = autoSock;
	INT iResult = Discovery(&lspath);
	if (0 != iResult) {
		DPErrorExWsa(_FT("Discovery failed! - returned (%d) : "), iResult);
		SetLastError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
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

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++) {
		m_bUnitDevicePresent[i] = lspath.PerTarget[i].bPresent;

		if (m_unitDevices[i] != NULL) {
			m_unitDevices[i]->SetHostUsageCount(
				lspath.PerTarget[i].NRROHost,
				lspath.PerTarget[i].NRRWHost);
		}

	}

	return TRUE;
}

void
CNdasDevice::
GetHWInfo(PHARDWARE_INFO pInfo)
{
	_ASSERTE( !::IsBadWritePtr(pInfo, sizeof(HARDWARE_INFO)));
	::CopyMemory(pInfo, &m_hwInfo, sizeof(HARDWARE_INFO));
}

DWORD
CNdasDevice::
GetMaxRequestBlocks()
{
	return m_hwInfo.nMaxRequestBlocks;
}


DWORD
CNdasDevice::
GetUnitDeviceCount()
{
	return m_dwUnitDeviceCount;
}


BOOL
CNdasDevice::
UpdateDeviceInfo()
{
	if (NDAS_DEVICE_STATUS_CONNECTED != m_status) {
		return FALSE;
	}

	return UpdateDeviceInfo_(
		m_localLpxAddress, 
		m_remoteLpxAddress,
		GetHWType(),
		GetHWVersion());
}

VOID
CNdasDevice::
SetLastDeviceError(NDAS_DEVICE_ERROR deviceError)
{
	m_lastError = deviceError;
}

LPCTSTR
CNdasDevice::
ToString()
{
	if (m_szStrBuf[0] == _T('0')) {
		HRESULT hr = ::StringCchPrintf(
			m_szStrBuf, 
			CCH_STR_BUF,
			_T("{%03X}%02X:%02X:%02X:%02X:%02X:%02X"),
			m_dwSlotNo,
			m_deviceId.Node[0],
			m_deviceId.Node[1],
			m_deviceId.Node[2],
			m_deviceId.Node[3],
			m_deviceId.Node[4],
			m_deviceId.Node[5]);

	}
	return m_szStrBuf;
}
