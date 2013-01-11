/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

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
	public ximeta::CExtensibleObject
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

	//
	// internal routine to update device information
	//
	BOOL cpUpdateDeviceInfo(
		CONST LPX_ADDRESS& localAddress, 
		CONST LPX_ADDRESS& remoteAddress,
		UCHAR ucType,
		UCHAR ucVersion);

	VOID SetLastDeviceError(NDAS_DEVICE_ERROR deviceError);

public:

	virtual ULONG AddRef();
	virtual ULONG Release();

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
	BOOL SetName(LPCTSTR szName);

	//
	// Get the NDAS device name
	//
	BOOL GetName(SIZE_T cchName, LPTSTR lpName);

	NDAS_DEVICE_STATUS GetStatus();
	NDAS_DEVICE_ERROR GetLastError();

	CONST NDAS_DEVICE_ID& GetDeviceId();
	DWORD GetSlotNo();
	LPCTSTR GetName();

	ACCESS_MASK GetGrantedAccess();
	BOOL SetGrantedAccess(ACCESS_MASK access);

	DWORD GetUnitDeviceCount();
	CNdasUnitDevice* GetUnitDevice(DWORD dwUnitNo);

	LPX_ADDRESS GetLocalLpxAddress();
	LPX_ADDRESS GetRemoteLpxAddress();

	UCHAR GetHWType();
	UCHAR GetHWVersion();
	UINT64 GetHWPassword();

	CONST HARDWARE_INFO& GetHardwareInfo();

	DWORD GetMaxRequestBlocks();

	BOOL OnDiscovered(
		CONST LPX_ADDRESS& localAddress,
		CONST LPX_ADDRESS& remoteAddress,
		UCHAR ucType,
		UCHAR ucVersion);

	BOOL UpdateDeviceInfo();
	BOOL OnStatusCheck();

	BOOL SetAutoRegistered(
		BOOL fAutoRegistered = TRUE, 
		ACCESS_MASK access = (GENERIC_READ | GENERIC_WRITE));

	BOOL GetAutoRegistered();

	virtual void Update(ximeta::PCSubject pChangedSubject);

	virtual LPCTSTR ToString();

	virtual BOOL InvalidateUnitDevice(DWORD dwUnitNo);

private:

	//
	// Copy constructor is prohibited.
	//
	CNdasDevice(const CNdasDevice &);

	//
	// Assignment operation is prohibited.
	//
	CNdasDevice& operator = (const CNdasDevice&);

public:

	//
	// ToString buffer
	//
	static const size_t CCH_STR_BUF = 30;
	// RTL_NUMBER_OF(_T("{000}FF:FF:FF:FF:FF:FF"));

protected:

	TCHAR m_szStrBuf[CCH_STR_BUF];

};

