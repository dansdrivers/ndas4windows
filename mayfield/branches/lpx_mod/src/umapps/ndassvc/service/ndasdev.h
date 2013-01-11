/*//////////////////////////////////////////////////////////////////////////
//
// Copyright (C)2002-2004 XIMETA, Inc.
// All rights reserved.
//
//////////////////////////////////////////////////////////////////////////*/

#pragma once
#include <socketlpx.h>
#include <lanscsiop.h>
#include "ndastypeex.h"
#include "ndasdib.h"
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
typedef CNdasDevice *PCNdasDevice;

class CNdasUnitDevice;
typedef CNdasUnitDevice *PCNdasUnitDevice;

class CNdasDeviceComm;

//////////////////////////////////////////////////////////////////////////
//
// Device Class
//
//////////////////////////////////////////////////////////////////////////

class CNdasDevice :
	public ximeta::CCritSecLock,
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
	CONST DWORD m_dwSlotNo;

	//
	// Device ID
	//
	CONST NDAS_DEVICE_ID m_deviceId;

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

	BOOL m_fAutoRegistered;

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
	CNdasDevice(DWORD dwSlotNo, CONST NDAS_DEVICE_ID& deviceID);

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

	NDAS_DEVICE_STATUS GetStatus();

	CONST NDAS_DEVICE_ID& GetDeviceId();
	DWORD GetSlotNo();
	LPCTSTR GetName();

	ACCESS_MASK GetGrantedAccess();
	VOID SetGrantedAccess(ACCESS_MASK access);

	DWORD GetUnitDeviceCount();
	CNdasUnitDevice* GetUnitDevice(DWORD dwUnitNo);

	CONST LPX_ADDRESS& GetLocalLpxAddress();
	CONST LPX_ADDRESS& GetRemoteLpxAddress();

	UCHAR GetHWType();
	UCHAR GetHWVersion();
	UINT64 GetHWPassword();

	CONST HARDWARE_INFO& GetHardwareInfo();

	DWORD GetMaxRequestBlocks();

	//
	// Auto Registration Support Methods
	//
	VOID SetAutoRegistered(BOOL fAutoRegistered = TRUE);

	BOOL IsAutoRegistered();

	//
	// Observer's Subject Updater
	//
	void Update(ximeta::PCSubject pChangedSubject);

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

