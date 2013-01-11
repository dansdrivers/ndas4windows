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

#include "ndascfg.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"
#include "ndasunitdevfactory.h"

#include "ndastype_str.h"
#include "lsbusioctl.h"
#include "ndasobjs.h"
#include "ndasmsg.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASDEV
#include "xdebug.h"

class CNdasEventMonitor;
typedef CNdasEventMonitor *PCNdasEventMonitor;

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
	DBGPRT_INFO(_FT("AddRef Reference: %u\n"), ulCount);
	return ulCount;
}

ULONG
CNdasDevice::Release()
{
	ximeta::CAutoLock autolock(this);

	ULONG ulCount = ximeta::CExtensibleObject::Release();
	DBGPRT_INFO(_FT("Release Reference: %u\n"), ulCount);
	return ulCount;
}

//
// constructor
//

CNdasDevice::CNdasDevice(
	DWORD dwSlotNo, 
	CONST NDAS_DEVICE_ID& deviceId) :
	m_status(NDAS_DEVICE_STATUS_DISABLED),
	m_lastError(NDAS_DEVICE_ERROR_NONE),
	m_dwSlotNo(dwSlotNo),
	m_deviceId(deviceId),
	m_grantedAccess(0x00000000L),
	m_dwLastHeartbeatTick(0),
	m_dwCommFailureCount(0),
	m_fAutoRegistered(FALSE)
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

	hr = ::StringCchPrintf(
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

	_ASSERTE(SUCCEEDED(hr));

	DBGPRT_TRACE(_FT("ctor: %s\n"), ToString());
}

//
// destructor
//

CNdasDevice::~CNdasDevice()
{
	DBGPRT_TRACE(_FT("dtor: %s\n"), ToString());

	ximeta::CAutoLock autolock(this);

	pGetNdasDeviceHeartbeatListner()->Detach(this);

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {
		if (NULL != m_pUnitDevices[i]) {
			CNdasUnitDevice* pUnitDevice = m_pUnitDevices[i];
			m_pUnitDevices[i] = NULL;
			BOOL fSuccess = pUnitDevice->UnregisterFromLDM();
			_ASSERTE(fSuccess);
			pUnitDevice->Release();
		}
	}
}

//
// initializer
//

BOOL
CNdasDevice::Initialize()
{
	ximeta::CAutoLock autolock(this);
	return TRUE;
}

//
// Device Name Setter
//

BOOL 
CNdasDevice::SetName(LPCTSTR szName)
{
	ximeta::CAutoLock autolock(this);

	HRESULT hr = ::StringCchCopy(
		m_szDeviceName, 
		MAX_NDAS_DEVICE_NAME_LEN, 
		szName);

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		m_szCfgContainer, 
		TEXT("DeviceName"), 
		szName);

	if (!fSuccess) {
		DPWarningEx(
			_FT("Writing device name entry to the registry failed at %s.\n"), 
			m_szCfgContainer);
	}

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
	(VOID) pEventPublisher->DevicePropertyChanged(m_dwSlotNo);

	return (SUCCEEDED(hr));
}

//
// Device Name Getter
//

BOOL 
CNdasDevice::GetName(SIZE_T cchName, LPTSTR lpName)
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
CNdasDevice::ChangeStatus(NDAS_DEVICE_STATUS newStatus)
{
	ximeta::CAutoLock autolock(this);

	if (m_status == newStatus) {
		return;
	}

	NDAS_DEVICE_STATUS oldStatus = m_status;

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventMonitor* pMonitor = pInstMan->GetEventMonitor();
	_ASSERTE(NULL != pMonitor);

	CNdasLogicalDeviceManager *pLdm = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLdm);

	CNdasDeviceHeartbeatListener *pHBListener = pInstMan->GetHBListener();
	_ASSERTE(NULL != pHBListener);

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	//
	// clear failure count for every status change
	//
	m_dwCommFailureCount = 0;

	switch (newStatus) {
	case NDAS_DEVICE_STATUS_DISABLED:
		{
			pHBListener->Detach(this);
			pMonitor->Detach(this);
			for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {
				if (NULL != m_pUnitDevices[i]) {
					CNdasUnitDevice* pUnitDevice = m_pUnitDevices[i];
					m_pUnitDevices[i] = NULL;
					BOOL fSuccess = pUnitDevice->UnregisterFromLDM();
					_ASSERTE(fSuccess);
					pUnitDevice->Release();
				}
			}

		}
		break;
	case NDAS_DEVICE_STATUS_CONNECTED:
		{
			pMonitor->Attach(this);
		}
		break;
	case NDAS_DEVICE_STATUS_DISCONNECTED:
		{
			//
			// Detaching from the Monitor will be done at OnStatusCheck
			// by returning TRUE to detach this device from the monitor
			//
			pHBListener->Attach(this);
			for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {
				if (NULL != m_pUnitDevices[i]) {
					CNdasUnitDevice* pUnitDevice = m_pUnitDevices[i];
					m_pUnitDevices[i] = NULL;
					BOOL fSuccess = pUnitDevice->UnregisterFromLDM();
					_ASSERTE(fSuccess);
					pUnitDevice->Release();
				}
			}
		}
		break;
	default:
		_ASSERTE(FALSE);
	}

	DPInfo(_FT("%s at slot %d status changed %s to %s\n"),
		CNdasDeviceId(m_deviceId).ToString(),
		m_dwSlotNo,
		NdasDeviceStatusString(m_status),
		NdasDeviceStatusString(newStatus));

	m_status = newStatus;

	(VOID) pEventPublisher->DeviceStatusChanged(
		m_dwSlotNo, 
		oldStatus,
		newStatus);

	return;
}

//
// set status of the device
//

NDAS_DEVICE_STATUS 
CNdasDevice::GetStatus()
{
	ximeta::CAutoLock autolock(this);

	return m_status;
}

//
// get last error the device
//

NDAS_DEVICE_ERROR
CNdasDevice::GetLastError()
{
	ximeta::CAutoLock autolock(this);

	return m_lastError;
}

//
// get device id
//

CONST NDAS_DEVICE_ID&
CNdasDevice::GetDeviceId()
{
	ximeta::CAutoLock autolock(this);

	return m_deviceId;
}

//
// get slot number of the NDAS device
// (don't be confused with logical device slot number.)
//

DWORD 
CNdasDevice::GetSlotNo()
{
	ximeta::CAutoLock autolock(this);

	return m_dwSlotNo;
}

//
// get the device name
// returned buffer is valid only while the instance is valid
// 

LPCTSTR 
CNdasDevice::GetName()
{
	ximeta::CAutoLock autolock(this);

	return m_szDeviceName;
}

//
// get granted (registered) access permission
//

ACCESS_MASK 
CNdasDevice::GetGrantedAccess()
{
	ximeta::CAutoLock autolock(this);

	return m_grantedAccess;
}

//
// set granted access permission
// use this to change the (valid) access of the registered device
//

BOOL
CNdasDevice::SetGrantedAccess(ACCESS_MASK access)
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

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
	(VOID) pEventPublisher->DevicePropertyChanged(m_dwSlotNo);

	return TRUE;
}

//
// on CONNECTED status only
// the local LPX address where the NDAS device was found
//

LPX_ADDRESS 
CNdasDevice::GetLocalLpxAddress()
{
	ximeta::CAutoLock autolock(this);
	return m_localLpxAddress;
}

//
// remote LPX address of this device
// this may be different than the actual device ID. (Proxied?)
//

LPX_ADDRESS 
CNdasDevice::GetRemoteLpxAddress()
{
	ximeta::CAutoLock autolock(this);
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
	return 
		(dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) ?
		NULL : m_pUnitDevices[dwUnitNo];
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
	BOOL bEnabled = bEnable;

	if (bEnable) {
		//
		// To enable this device
		//
		if (m_status != NDAS_DEVICE_STATUS_DISABLED) {
			return TRUE;
		} else {
			ChangeStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
		}
	} else {
		//
		// You cannot disable this device when a unit device is mounted
		//
		for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {
			CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);
			if (NULL != pUnitDevice &&
				NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus())
			{
				::SetLastError(NDASHLPSVC_ERROR_CANNOT_DISABLE_MOUNTED_DEVICE);
				return FALSE;
			}
		}
		//
		// To disable this device
		//
		if (m_status == NDAS_DEVICE_STATUS_DISABLED) {
			return TRUE;
		} else {
			ChangeStatus(NDAS_DEVICE_STATUS_DISABLED);
		}
	}

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		m_szCfgContainer, 
		TEXT("Enabled"), 
		bEnabled);
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
CNdasDevice::Update(ximeta::PCSubject pChangedSubject)
{
	ximeta::CAutoLock autolock(this);

	PCNdasDeviceHeartbeatListener pListener = 
		pGetNdasDeviceHeartbeatListner();

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

		CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();
		for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; ++i) {

			CNdasUnitDevice* pUnitDevice = m_pUnitDevices[i];
			if (NULL == pUnitDevice) continue;

			CNdasLogicalDevice* pLogDevice = pUnitDevice->GetLogicalDevice();
			if(NULL == pLogDevice) return FALSE;

			NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
			if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
				NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status ||
				NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == status)
			{
				// lock status and
				// do not detach from the monitor
				return FALSE;
			}
		}

		//
		// Do not disconnect the device when the debugger is attachted
		//
		if (::IsDebuggerPresent()) {
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
// called by Notify()
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
	BOOL fSuccess = cpUpdateDeviceInfo(localAddress, remoteAddress, ucType, ucVersion);
	if (!fSuccess) {
		SetLastError(NDAS_DEVICE_ERROR_DISCOVER_FAILED);
		++m_dwCommFailureCount;
		return FALSE;
	}

	m_dwCommFailureCount = 0;

	//
	// Status should be changed to CONNECTED
	// Before creating Unit Device Instances
	//

	ChangeStatus(NDAS_DEVICE_STATUS_CONNECTED);

	//
	// We should look up every unit device as there may be only a secondary
	// IDE device in the NDAS device.
	//

	DWORD nUnitDeviceCount = MAX_NDAS_UNITDEVICE_COUNT; // pPath->iNRTargets;

	for (DWORD i = 0; i < MAX_NDAS_UNITDEVICE_COUNT; i++) {

		//
		// recreate unit device
		//
		if (NULL != m_pUnitDevices[i]) {
			if (m_pUnitDevices[i]->GetStatus() == NDAS_UNITDEVICE_STATUS_MOUNTED) {
				//
				// already mounted unit devices should remain intact
				//

				//
				// reconcile its status only
				//
				continue;
			}
		}

		//
		// not-mounted unit devices can be re-instantiated.
		//
		if (NULL != m_pUnitDevices[i]) {
			CNdasUnitDevice* p = m_pUnitDevices[i];
			m_pUnitDevices[i] = NULL;
			BOOL fSuccess = p->UnregisterFromLDM();
			_ASSERTE(fSuccess);
			p->Release();
		}

		//
		// If LANSCSI command shows there is no unit device,
		// do not attempt to create an unit device instance
		//
		if (!m_fUnitDevicePresent[i]) {
			continue;
		}

		CNdasUnitDeviceCreator udCreator(*this, i);
		CNdasUnitDevice* pUnitDevice = 	udCreator.CreateUnitDevice();

		if (NULL == pUnitDevice) {
			DPErrorEx(_FT("Creating a unit device instance failed: "));
			continue;
		}

		m_pUnitDevices[i] = pUnitDevice;
		m_pUnitDevices[i]->AddRef();
		fSuccess = m_pUnitDevices[i]->RegisterToLDM();
		_ASSERTE(fSuccess);

	}

	return TRUE;
}

BOOL 
CNdasDevice::cpUpdateDeviceInfo(
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
		DPErrorEx(_FT("Error Create Connection: "));
		return FALSE;
	}

	//
	// Discover
	//
	lspath.connsock = autoSock;
	INT iResult = Discovery(&lspath);
	if (0 != iResult) {
		DPErrorEx(_FT("Discovery failed! - returned (%d) : "), iResult);
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

		m_fUnitDevicePresent[i] = lspath.PerTarget[i].bPresent;

		if (m_pUnitDevices[i] != NULL) {
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
	ximeta::CAutoLock autolock(this);

	return m_hwInfo;
}

DWORD
CNdasDevice::GetMaxRequestBlocks()
{
	ximeta::CAutoLock autolock(this);

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


BOOL
CNdasDevice::UpdateDeviceInfo()
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_DEVICE_STATUS_CONNECTED != m_status) {
		return FALSE;
	}

	return cpUpdateDeviceInfo(
		m_localLpxAddress, 
		m_remoteLpxAddress,
		GetHWType(),
		GetHWVersion());
}

VOID
CNdasDevice::SetLastDeviceError(NDAS_DEVICE_ERROR deviceError)
{
	ximeta::CAutoLock autolock(this);

	m_lastError = deviceError;
}

BOOL
CNdasDevice::SetAutoRegistered(BOOL fAutoRegistered, ACCESS_MASK access)
{
	ximeta::CAutoLock autolock(this);

	BOOL fSuccess = SetGrantedAccess(access);
	if (!fSuccess) {
		return FALSE;
	}
	m_fAutoRegistered = fAutoRegistered;
	return TRUE;
}

BOOL
CNdasDevice::GetAutoRegistered()
{
	ximeta::CAutoLock autolock(this);

	return m_fAutoRegistered;
}

LPCTSTR
CNdasDevice::ToString()
{
	ximeta::CAutoLock autolock(this);

	return m_szStrBuf;
}

BOOL
CNdasDevice::InvalidateUnitDevice(DWORD dwUnitNo)
{
	ximeta::CAutoLock autoLock(this);

	DBGPRT_INFO(_FT("Invalidating Unit Device %d\n"), dwUnitNo);

	if (NDAS_DEVICE_STATUS_CONNECTED != GetStatus()) {
		DBGPRT_INFO(_T("Non-connected device ignored - %s, %d\n"), ToString(), dwUnitNo);
		return TRUE;
	}

	if (dwUnitNo >= MAX_NDAS_UNITDEVICE_COUNT) {
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	CNdasUnitDevice* pUnitDevice = m_pUnitDevices[dwUnitNo];

	//
	// recreate unit device
	//
	if (NULL != pUnitDevice) {
		if (NDAS_UNITDEVICE_STATUS_MOUNTED == pUnitDevice->GetStatus()) {
			//
			// already mounted unit devices should remain intact
			//
			return TRUE;
		}
		//
		// not-mounted unit devices can be re-instantiated.
		//
		m_pUnitDevices[dwUnitNo] = NULL;
		BOOL fSuccess = pUnitDevice->UnregisterFromLDM();
		_ASSERTE(fSuccess);
		pUnitDevice->Release();
	}

	CNdasUnitDeviceCreator udCreator(*this, dwUnitNo);
	pUnitDevice = udCreator.CreateUnitDevice();

	if (NULL == pUnitDevice) {
		DPErrorEx(_FT("Creating a unit device instance failed: "));
		return FALSE;
	}

	m_pUnitDevices[dwUnitNo] = pUnitDevice;
	m_pUnitDevices[dwUnitNo]->RegisterToLDM();
	m_pUnitDevices[dwUnitNo]->AddRef();

	return TRUE;
}
