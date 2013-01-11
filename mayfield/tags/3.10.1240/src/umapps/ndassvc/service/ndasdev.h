/*//////////////////////////////////////////////////////////////////////////
//
// Copyright (C)2002-2004 XIMETA, Inc.
// All rights reserved.
//
//////////////////////////////////////////////////////////////////////////*/

#pragma once
#include <socketlpx.h>
#include <lanscsiop.h>
#include "ndas/ndastypeex.h"
#include "ndas/ndasdib.h"
#include "ndasdev.h"
#include "ndasunitdev.h"
#include "ndasdevid.h"
#include "syncobj.h"
#include "lpxcomm.h"
#include "ndaslogdev.h"
#include "observer.h"
#include "extobj.h"
#include "objstr.h"

class CNdasDevice;
class CNdasUnitDevice;
class CNdasDeviceComm;

//////////////////////////////////////////////////////////////////////////
//
// Device Class
//
//////////////////////////////////////////////////////////////////////////

class CNdasDevice :
	public ximeta::CCritSecLockGlobal,
	public ximeta::CObserver,
	public ximeta::CExtensibleObject,
	public ximeta::CStringizer<CNdasDevice,32>
{
	friend class CNdasUnitDevice;

public:

	static const DWORD MAX_NDAS_UNITDEVICE_COUNT = 2;
	static const DWORD MAX_ALLOWED_HEARTBEAT_INTERVAL = 15 * 1000; // 15 sec
	static const DWORD MAX_ALLOWED_FAILURE_COUNT = 3;

	typedef struct _HARDWARE_INFO {
		UCHAR ucType;
		UCHAR ucVersion;
		DWORD nSlots;
		DWORD nTargets;
		DWORD nMaxTargets;
		DWORD nMaxLUs;
		DWORD nMaxRequestBlocks;
	} HARDWARE_INFO, *PHARDWARE_INFO;

protected:

	//
	// Registry configuration container name
	//
	TCHAR m_szCfgContainer[30];

	//
	// Device status
	//
	NDAS_DEVICE_STATUS m_status;

	//
	// Last error
	//
	NDAS_DEVICE_ERROR m_lastError;

	//
	// Slot number
	//
	const DWORD m_dwSlotNo;

	//
	// Device ID
	//
	const NDAS_DEVICE_ID m_deviceId;

	//
	// Granted access, which is supplied by the user
	// based on the registration key
	//
	ACCESS_MASK m_grantedAccess;

	LPX_ADDRESS m_localLpxAddress;
	LPX_ADDRESS m_remoteLpxAddress;

	TCHAR m_szDeviceName[MAX_NDAS_DEVICE_NAME_LEN];

	CNdasUnitDevice* m_pUnitDevices[MAX_NDAS_UNITDEVICE_COUNT];

	//
	// Unit Device Existence discovered by LANSCSI Command
	//
	BOOL m_fUnitDevicePresent[MAX_NDAS_UNITDEVICE_COUNT];

	DWORD m_dwLastHeartbeatTick;

	HARDWARE_INFO m_hwInfo;

	DWORD m_dwCommFailureCount;

	//
	// Auto Registered Device
	//
	const BOOL m_fAutoRegistered : 1;

	//
	// Volatile (Non-persistent) Device
	//
	const BOOL m_fVolatile : 1;

	//
	// If set, this device will not be shown at the device list
	//
	const BOOL m_fHidden : 1;

protected:

	VOID ChangeStatus(NDAS_DEVICE_STATUS newStatus);
	VOID SetLastDeviceError(NDAS_DEVICE_ERROR deviceError);

public:

	NDAS_DEVICE_ERROR GetLastDeviceError();

public:

	ULONG AddRef();
	ULONG Release();

public:

	//
	// Constructor
	//
	CNdasDevice(
		DWORD dwSlotNo, 
		CONST NDAS_DEVICE_ID& deviceID, 
		DWORD dwFlags);

	//
	// Destructor
	//
	virtual ~CNdasDevice();

	//
	// Initializer
	//
	// After creating an instance, you should always call initializer 
	// and check the status of the initialization 
	// before doing other operations.
	//
	BOOL Initialize();

	//
	// Enable or Disable the device
	// When disabled, no heartbeat packet is processed
	//
	BOOL Enable(BOOL bEnable = TRUE);

	//
	// Set the NDAS device name
	//
	VOID SetName(LPCTSTR szName);
	VOID SetGrantedAccess(ACCESS_MASK access);

	NDAS_DEVICE_STATUS GetStatus();

	const NDAS_DEVICE_ID& GetDeviceId();
	DWORD GetSlotNo();
	BOOL GetName(DWORD cchBuffer, LPTSTR lpBuffer);

	ACCESS_MASK GetGrantedAccess();

	DWORD GetUnitDeviceCount();
	CNdasUnitDevice* GetUnitDevice(DWORD dwUnitNo);

	const LPX_ADDRESS& GetLocalLpxAddress();
	const LPX_ADDRESS& GetRemoteLpxAddress();

	UCHAR GetHWType();
	UCHAR GetHWVersion();
	UINT64 GetHWPassword();

	const HARDWARE_INFO& GetHardwareInfo();

	DWORD GetMaxRequestBlocks();

	//
	// Is this device registered via automatic discovery?
	//
	BOOL IsAutoRegistered();

	//
	// Is this device hidden
	//
	BOOL IsHidden();

	//
	// Observer's Subject Updater
	//
	void Update(ximeta::CSubject* pChangedSubject);

	//
	// Update Device Information with reconcilation
	//
	BOOL UpdateDeviceInfo();

	//
	// Invalidating Unit Device
	//
	BOOL InvalidateUnitDevice(DWORD dwUnitNo);

	//
	// Discover Event Subscription
	//
	BOOL OnDiscovered(
		CONST LPX_ADDRESS& localAddress,
		CONST LPX_ADDRESS& remoteAddress,
		UCHAR ucType,
		UCHAR ucVersion);

	//
	// Status Check Event Subscription
	//
	BOOL OnStatusCheck();

	//
	// CStringizer Implementation
	//
	LPCTSTR ToString();

	//
	// Is this device is volatile (not persistent) ?
	// Auto-registered or OEM devices may be volatile,
	// which means the registration will not be retained after reboot.
	//
	BOOL IsVolatile();

protected:

	static BOOL IsSupportedHardwareVersion(
		UCHAR ucType, 
		UCHAR ucVersion);

	//
	// Retrieve device information
	//
	BOOL GetDeviceInfo(
		CONST LPX_ADDRESS& localAddress, 
		CONST LPX_ADDRESS& remoteAddress,
		UCHAR ucType,
		UCHAR ucVersion);

	//
	// Reconcile Unit Device Instances
	//
	// Return value: 
	//  TRUE if status can be changed to DISCONNECTED,
	//  FALSE if there are any MOUNTED unit devices left.
	//
	BOOL DestroyAllUnitDevices();
	BOOL DestroyUnitDevice(DWORD dwUnitNo);
	BOOL CreateUnitDevice(DWORD dwUnitNo);

	//
	// Is any unit device is mounted?
	//
	BOOL IsAnyUnitDevicesMounted();

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

	//
	// Copy constructor is prohibited.
	//
	CNdasDevice(const CNdasDevice &);

	//
	// Assignment operation is prohibited.
	//
	CNdasDevice& operator = (const CNdasDevice&);
};

