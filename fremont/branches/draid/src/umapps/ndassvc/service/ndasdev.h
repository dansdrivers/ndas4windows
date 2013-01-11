/*//////////////////////////////////////////////////////////////////////////
//
// Copyright (C)2002-2004 XIMETA, Inc.
// All rights reserved.
//
//////////////////////////////////////////////////////////////////////////*/

#pragma once
#include <socketlpx.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasdib.h>
#include <ndas/ndasid.h>
#include <boost/enable_shared_from_this.hpp>
#include "ndassvcdef.h"
#include "ndasunitdev.h"
#include "ndasdevid.h"
#include "syncobj.h"
#include "lpxcomm.h"
#include "ndaslogdev.h"
#include "observer.h"
#include "objstr.h"

const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE  = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT = { 0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };
const NDAS_OEM_CODE NDAS_OEM_CODE_RUTTER  = NDAS_OEM_CODE_DEFAULT;
const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE = { 0x52, 0x41, 0x27, 0x46, 0xBC, 0x6E, 0xA2, 0x99 };

const BYTE NDAS_VID_DEFAULT = 0x01;
const BYTE NDAS_VID_SEAGATE = 0x41;

const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT = { 0xCD, NDAS_VID_DEFAULT, 0xFF, 0xFF };
const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE = { 0xCD, NDAS_VID_SEAGATE, 0xFF, 0xFF };

class CNdasDeviceComm;

//////////////////////////////////////////////////////////////////////////
//
// Device Class
//
//////////////////////////////////////////////////////////////////////////

class CNdasDevice :
	public ximeta::CCritSecLockGlobal,
	public ximeta::CObserver,
	public CStringizerA<32>,
	public boost::enable_shared_from_this<CNdasDevice>
{

public:

	static const DWORD MAX_NDAS_UNITDEVICE_COUNT = 2;
	typedef ximeta::CAutoLock<CNdasDevice> InstanceAutoLock;

protected:

	// Slot number
	const DWORD m_dwSlotNo;
	// Device ID
	const NDAS_DEVICE_ID m_deviceId;
	// Auto Registered Device
	const bool m_fAutoRegistered;
	// Volatile (Non-persistent) Device
	const bool m_fVolatile;
	// If set, this device will not be shown at the device list
	const bool m_fHidden;

	// Unit Devices
	CNdasUnitDeviceVector m_unitDevices;
	
	// Registry configuration container name
	TCHAR m_szCfgContainer[30];

	// Device status
	NDAS_DEVICE_STATUS m_status;

	// Last error
	NDAS_DEVICE_ERROR m_lastError;

	// Granted access, which is supplied by the user
	// based on the registration key
	ACCESS_MASK m_grantedAccess;

	// Local LPX Address
	LPX_ADDRESS m_localLpxAddress;

	// Remote LPX Address
	LPX_ADDRESS m_remoteLpxAddress;

	// Device Name
	TCHAR m_szDeviceName[MAX_NDAS_DEVICE_NAME_LEN];

	// Last Heartbeat Tick
	DWORD m_dwLastHeartbeatTick;

	// Initial communication failure count
	DWORD m_dwCommFailureCount;

	// NDAS Device OEM Code
	NDAS_OEM_CODE m_OemCode;

	// NDAS Device Stats
	NDAS_DEVICE_STAT m_dstat;

	// NDAS device hardware information
	NDAS_DEVICE_HARDWARE_INFO m_hardwareInfo;

	// NDAS ID Extension Data
	NDASID_EXT_DATA m_ndasIdExtension;

protected:

public:

	NDAS_DEVICE_ERROR GetLastDeviceError();

public:

	//
	// Constructor
	//
	CNdasDevice(
		__in DWORD SlotNo, 
		__in const NDAS_DEVICE_ID& DeviceID,
		__in DWORD RegFlags,
		__in_opt const NDASID_EXT_DATA* NdasIdExtension);

	//
	// Destructor
	//
	~CNdasDevice();

	//
	// Initializer
	//
	// After creating an instance, you should always call initializer 
	// and check the status of the initialization 
	// before doing other operations.
	//
	BOOL Initialize();

	//////////////////////////////////////////////////////////////////////////
	// LOCK-FREE METHODS

	// Is this device registered via automatic discovery?
	bool IsAutoRegistered() const;

	// Is this device hidden
	bool IsHidden() const;

	// Is this device is volatile (not persistent) ?
	// Auto-registered or OEM devices may be volatile,
	// which means the registration will not be retained after reboot.
	bool IsVolatile() const;

	// Device ID
	const NDAS_DEVICE_ID& GetDeviceId() const;
	void GetDeviceId(NDAS_DEVICE_ID& DeviceId) const;

	// Slot Number
	DWORD GetSlotNo() const;

	// LOCK-FREE METHODS
	//////////////////////////////////////////////////////////////////////////

	//
	// Enable or Disable the device
	// When disabled, no heartbeat packet is processed
	//
	BOOL Enable(BOOL bEnable = TRUE);

	//
	// Set the NDAS device name
	//
	void SetName(LPCTSTR szName);
	void SetGrantedAccess(ACCESS_MASK access);

	NDAS_DEVICE_STATUS GetStatus();

	void GetName(DWORD cchBuffer, LPTSTR lpBuffer);

	ACCESS_MASK GetGrantedAccess();
	ACCESS_MASK GetAllowingAccess();

	DWORD GetUnitDeviceCount();

	CNdasUnitDevicePtr GetUnitDevice(DWORD dwUnitNo);
	void GetUnitDevices(CNdasUnitDeviceVector& dest);

	const LPX_ADDRESS& GetLocalLpxAddress();
	const LPX_ADDRESS& GetRemoteLpxAddress();

	UCHAR GetHardwareType();
	UCHAR GetHardwareVersion();
	UINT16 GetHardwareRevision();	
	UINT64 GetHardwarePassword();

	// Internally synchronized function
	void GetOemCode(NDAS_OEM_CODE& OemCode);
	void SetOemCode(const NDAS_OEM_CODE& OemCode);
	void ResetOemCode();
	void GetHardwareInfo(NDAS_DEVICE_HARDWARE_INFO& HardwareInfo);
	void GetStats(NDAS_DEVICE_STAT& dstat);

	// Requires External Synchronization
	const NDAS_OEM_CODE& GetOemCode();
	const NDAS_DEVICE_HARDWARE_INFO& GetHardwareInfo();
	const NDAS_DEVICE_STAT& GetStats();
	const NDASID_EXT_DATA& GetNdasIdExtension();

	DWORD GetMaxTransferBlocks();

	//
	// Observer's Subject Updater
	//
	void Update(ximeta::CSubject* pChangedSubject);

	//
	// Update Device Stats
	//
	BOOL UpdateStats();

	//
	// Invalidating Unit Device
	//
	BOOL InvalidateUnitDevice(DWORD dwUnitNo);

	//
	// Discover Event Subscription
	//
	void OnHeartbeat(
		CONST LPX_ADDRESS& localAddress,
		CONST LPX_ADDRESS& remoteAddress,
		UCHAR ucType,
		UCHAR ucVersion);

	//
	// Status Check Event Subscription
	//
	void OnPeriodicCheckup();
	void OnUnitDeviceUnmounted(CNdasUnitDevicePtr pUnitDevice);

protected:

	void _ChangeStatus(NDAS_DEVICE_STATUS newStatus);
	void _SetLastDeviceError(NDAS_DEVICE_ERROR deviceError);

	//
	// Update Device Information with reconcilation
	//
	BOOL _UpdateDeviceInfo();

	//
	// Retrieve device information
	//
	BOOL _GetDeviceInfo(
		CONST LPX_ADDRESS& localAddress, 
		CONST LPX_ADDRESS& remoteAddress);

	//
	// Reconcile Unit Device Instances
	//
	// Return value: 
	//  TRUE if status can be changed to DISCONNECTED,
	//  FALSE if there are any MOUNTED unit devices left.
	//
	void _DestroyAllUnitDevices();
	CNdasUnitDevicePtr _CreateUnitDevice(DWORD dwUnitNo);

	//
	// Is any unit device is mounted?
	//
	bool _IsAnyUnitDevicesMounted();

	void _ResetUnitDevice();

	//
	// Registry Access Helper
	//
	template <typename T>
	BOOL SetConfigValue(LPCTSTR szName, T value);

	template <typename T>
	BOOL SetConfigValueSecure(LPCTSTR szName, T value);

	BOOL DeleteConfigValue(LPCTSTR szName);

	BOOL SetConfigValueSecure(LPCTSTR szName, LPCVOID lpValue, DWORD cbValue);

private:

	// Copy constructor is prohibited.
	CNdasDevice(const CNdasDevice &);
	// Assignment operation is prohibited.
	CNdasDevice& operator = (const CNdasDevice&);
};

