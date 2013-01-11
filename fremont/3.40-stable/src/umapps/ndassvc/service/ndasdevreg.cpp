/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#include "stdafx.h"
#include <ndas/ndasmsg.h>
#include "ndasdevid.h"
#include "ndascfg.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"

#include "ndascomobjectsimpl.hpp"

#include "ndasdev.h"

#include "ndasdevreg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasdevreg.tmh"
#endif

LONG DbgLevelSvcReg = DBG_LEVEL_SVC_REG;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelSvcReg) {								\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)


const TCHAR CNdasDeviceRegistrar::CFG_CONTAINER[] = _T("Devices");

typedef struct _NDAS_DEVICE_ID_REG_DATA {
	NDAS_DEVICE_ID DeviceId;
	NDASID_EXT_DATA NdasIdExtension;
} NDAS_DEVICE_ID_REG_DATA, *PNDAS_DEVICE_ID_REG_DATA;

namespace
{
	BOOL pCharToHex(TCHAR c, LPDWORD lpValue);
	BOOL pConvertStringToDeviceId(LPCTSTR szAddressString, NDAS_DEVICE_ID* pDeviceId);
}

CNdasDeviceRegistrar::CNdasDeviceRegistrar() :
	m_fBootstrapping(FALSE),
	m_slotbit(MAX_SLOT_NUMBER + 1)
{
	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"CNdasDeviceRegistrar::ctor()\n");

	// Slot 0 is always occupied and reserved!
	m_slotbit[0] = true;
}

CNdasDeviceRegistrar::~CNdasDeviceRegistrar()
{
	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"CNdasDeviceRegistrar::dtor()\n");
}

STDMETHODIMP
CNdasDeviceRegistrar::DeviceRegister (
	__in_opt DWORD					SlotNo,
	__in const NDAS_DEVICE_ID		&DeviceId,
	__in DWORD						RegFlags,
	__in_opt const NDASID_EXT_DATA  *NdasIdExtension,
	__in BSTR						Name,
	__in ACCESS_MASK				GrantedAccess,
	__in_opt const NDAS_OEM_CODE	*NdasOemCode,
	__deref_out INdasDevice			**ppNdasDevice
	)
{
	HRESULT hr;

	*ppNdasDevice = 0;

	NDAS_DEVICE_ID ndasDeviceId = DeviceId;
	
	//
	// this will lock this class from here
	// and releases the lock when the function returns;
	//
	CAutoLock<CLock> autolock(&m_DataLock);
	
	NdasUiDbgCall( 2, "Registering device %s Vid = %d at slot %d\n", CNdasDeviceId(DeviceId).ToStringA(), DeviceId.Vid, SlotNo );

	if (NULL == NdasIdExtension)
	{
		NdasIdExtension = &NDAS_ID_EXTENSION_DEFAULT;
	}

	// Only DEFAULT and SEAGATE are currently implemented
	
	if (NdasIdExtension->Vid != NDAS_VID_SEAGATE  	&&
		NdasIdExtension->Vid != NDAS_VID_WINDWOS_RO &&
		NdasIdExtension->Vid != NDAS_VID_DEFAULT) {

		NdasUiDbgCall( 1, _T("Unknown Vendor ID=0x%02X %s %0x02\n"), 
					   NdasIdExtension->Vid, CNdasDeviceId(DeviceId).ToString(), DeviceId.Vid );

		hr = NDASSVC_ERROR_UNKNOWN_VENDOR_ID;
		return hr;
	}

	ndasDeviceId.Vid = NdasIdExtension->Vid;
	
	// If SlotNo is zero, automatically assign it.
	// check slot number
	if (0 == SlotNo)
	{
		SlotNo = pLookupEmptySlot();
		if (0 == SlotNo)
		{
			return NDASSVC_ERROR_DEVICE_ENTRY_SLOT_FULL;
		}
	}
	else if (SlotNo > MAX_SLOT_NUMBER)
	{
		return NDASSVC_ERROR_INVALID_SLOT_NUMBER;
	}

	// check and see if the slot is occupied

	if (m_slotbit[SlotNo]) {

		return NDASSVC_ERROR_SLOT_ALREADY_OCCUPIED;
	}

	// find an duplicate address

	CComPtr<INdasDevice> pExistingDevice;

	hr = get_NdasDevice( &ndasDeviceId, &pExistingDevice );
	
	if (SUCCEEDED(hr)) {

		NdasUiDbgCall( 1, _T("Already Registered ID=0x%02X %s %0x02\n"), 
					   NdasIdExtension->Vid, CNdasDeviceId(DeviceId).ToString(), DeviceId.Vid );

		DWORD regFlags = 0;

		pExistingDevice->get_RegisterFlags(&regFlags);

		if (regFlags != (NDAS_DEVICE_REG_FLAG_HIDDEN | NDAS_DEVICE_REG_FLAG_VOLATILE)) {

			ATLASSERT(FALSE);
			return NDASSVC_ERROR_DUPLICATE_DEVICE_ENTRY;
		}

		hr = Deregister(pExistingDevice);

		if (FAILED(hr)) {

			ATLASSERT(FALSE);
			return hr;
		}
	}

	// register

	CComObject<CNdasDevice> *pNdasDeviceInstance;

	hr = CComObject<CNdasDevice>::CreateInstance(&pNdasDeviceInstance);

	if (FAILED(hr)) {

		return hr;
	}

	hr = pNdasDeviceInstance->NdasDevInitialize( SlotNo, ndasDeviceId, RegFlags, NdasIdExtension );

	if (FAILED(hr)) {

		NdasUiDbgCall( 1, "Device initialization failed, error=0x%X\n", GetLastError() );
		return hr;
	}

	CComPtr<INdasDevice> pNdasDevice(pNdasDeviceInstance);

	COMVERIFY(pNdasDevice->put_Name(Name));
	COMVERIFY(pNdasDevice->put_GrantedAccess(GrantedAccess));

	if (NdasOemCode) {

		COMVERIFY( pNdasDevice->put_OemCode(NdasOemCode) );
	}

	m_slotbit[SlotNo] = true;

	bool insertResult;

	m_NdasDevices.Add(pNdasDevice);

	XTLVERIFY( m_deviceSlotMap.insert(std::make_pair(SlotNo, pNdasDevice)).second );
	//DeviceSlotMap::value_type(SlotNo, pNdasDevice)).second;

	XTLVERIFY( m_deviceIdMap.insert(std::make_pair(ndasDeviceId, pNdasDevice)).second );
	//DeviceIdMap::value_type(ndasDeviceId, pNdasDevice)).second;

	XTLASSERT(m_deviceSlotMap.size() == m_deviceIdMap.size());

	//
	// When NdasIdExtension is NULL, NDAS_ID_EXTENSION_DEFAULT is assigned already
	//

	ATLASSERT(ndasDeviceId.Reserved == 0 );

	if (RegFlags & NDAS_DEVICE_REG_FLAG_VOLATILE)
	{
	}
	else
	{
		BOOL success;
		XTL::CStaticStringBuffer<30> containerName(_T("Devices\\%04d"), SlotNo);

		if (0 != memcmp(&NDAS_ID_EXTENSION_DEFAULT, NdasIdExtension, sizeof(NDASID_EXT_DATA)))
		{
			NDAS_DEVICE_ID_REG_DATA regData = {0};
			regData.DeviceId = ndasDeviceId;
			regData.NdasIdExtension = *NdasIdExtension;

			success = _NdasSystemCfg.SetSecureValueEx(
				containerName, 
				_T("DeviceID2"), 
				&regData, 
				sizeof(regData));
		}
		else
		{
			success = _NdasSystemCfg.SetSecureValueEx(
				containerName, 
				_T("DeviceID"), 
				&ndasDeviceId, 
				sizeof(ndasDeviceId));
		}

		if (success == FALSE) {

			NdasUiDbgCall( 1, "Writing registration entry to the registry failed at %ls, error=0x%X\n", 
							   containerName.ToString(), GetLastError() );
		}

		success = _NdasSystemCfg.SetSecureValueEx(
			containerName,
			_T("RegFlags"),
			&RegFlags,
			sizeof(RegFlags));

		if (success == FALSE) {

			NdasUiDbgCall( 1, "Writing registration entry to the registry failed at %ls, error=0x%X\n", 
							   containerName.ToString(), GetLastError() );
		}
	}

	BOOL publishEvent = !m_fBootstrapping;

	autolock.Release();

	//
	// During bootstrapping, we do not publish this event
	// Bootstrap process will publish an event later
	//

	if (publishEvent) 
	{
		(void) pGetNdasEventPublisher().DeviceEntryChanged();
	}

	*ppNdasDevice = pNdasDevice.Detach();

	return S_OK;
}

STDMETHODIMP
CNdasDeviceRegistrar::Deregister(INdasDevice* pNdasDevice)
{
	NDAS_DEVICE_ID ndasDeviceId;
	HRESULT hr;
	
	COMVERIFY(hr = pNdasDevice->get_NdasDeviceId(&ndasDeviceId));
	if (FAILED(hr))
	{
		return hr;
	}

	DWORD slotNo;
	COMVERIFY(hr = pNdasDevice->get_SlotNo(&slotNo));
	XTLASSERT(0 != slotNo);

	NDAS_DEVICE_STATUS status;
	COMVERIFY(pNdasDevice->get_Status(&status));
	if (NDAS_DEVICE_STATUS_NOT_REGISTERED != status)
	{
		return NDASSVC_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE;
	}

	CAutoLock<CLock> autolock(&m_DataLock);

	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"Unregister device %s\n", CNdasDeviceId(ndasDeviceId).ToStringA());

	bool found = false;
	size_t count = m_NdasDevices.GetCount();
	for (size_t i = 0; i < count; ++i)
	{
		CComPtr<INdasDevice> p = m_NdasDevices.GetAt(i);
		if (p == pNdasDevice)
		{
			m_NdasDevices.RemoveAt(i);
			found = true;
			break;
		}
	}

	if (!found)
	{
		return NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND;
	}

	DeviceIdMap::iterator itrId = m_deviceIdMap.find(ndasDeviceId);
	ATLASSERT(m_deviceIdMap.end() != itrId);

	DeviceSlotMap::iterator itrSlot = m_deviceSlotMap.find(slotNo);
	ATLASSERT(m_deviceSlotMap.end() != itrSlot);

	m_deviceIdMap.erase(itrId);
	m_deviceSlotMap.erase(itrSlot);

	m_slotbit[slotNo] = false;

	XTL::CStaticStringBuffer<30> containerName(_T("Devices\\%04d"), slotNo);
	BOOL success = _NdasSystemCfg.DeleteContainer(containerName, TRUE);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_WARNING,
			"Deleting registration entry from the registry failed at %ls, error=0x%X\n", 
			containerName, GetLastError());
	}

	autolock.Release();

	(void) pGetNdasEventPublisher().DeviceEntryChanged();

	return S_OK;
}

STDMETHODIMP 
CNdasDeviceRegistrar::get_NdasDevice(__in DWORD SlotNo, __deref_out INdasDevice** ppNdasDevice)
{
	*ppNdasDevice = 0;

	CAutoLock<CLock> autolock(&m_DataLock);

	DeviceSlotMap::const_iterator itr = m_deviceSlotMap.find(SlotNo);
	if (m_deviceSlotMap.end() == itr)
	{
		return E_FAIL;
	}
	CComPtr<INdasDevice> pNdasDevice = itr->second;
	*ppNdasDevice = pNdasDevice.Detach();
	return S_OK;
}

STDMETHODIMP 
CNdasDeviceRegistrar::get_NdasDevice(__in NDAS_DEVICE_ID* DeviceId, __deref_out INdasDevice** ppNdasDevice)
{
	*ppNdasDevice = 0;

	CAutoLock<CLock> autolock(&m_DataLock);

	DeviceIdMap::const_iterator itr = m_deviceIdMap.find(*DeviceId);
	if (m_deviceIdMap.end() == itr)
	{
		return E_FAIL;
	}
	CComPtr<INdasDevice> pNdasDevice = itr->second;
	*ppNdasDevice = pNdasDevice.Detach();
	return S_OK;
}

STDMETHODIMP 
CNdasDeviceRegistrar::get_NdasDevice(__in NDAS_DEVICE_ID_EX* Device, __deref_out INdasDevice** ppNdasDevice)
{
	return Device->UseSlotNo ? 
		get_NdasDevice(Device->SlotNo, ppNdasDevice) : 
		get_NdasDevice(&Device->DeviceId, ppNdasDevice);
}

BOOL
CNdasDeviceRegistrar::pImportLegacyEntry(DWORD SlotNo, HKEY hEntryKey)
{
	NdasUiDbgCall( 2, "in\n" );

	static CONST size_t CB_ADDR = sizeof(TCHAR) * 18;

	HRESULT hr = E_FAIL;
	TCHAR szAddrVal[CB_ADDR + 1];
	DWORD cbAddrVal = sizeof(szAddrVal);
	DWORD dwValueType;

	LONG lResult = ::RegQueryValueEx(
		hEntryKey, 
		_T("Address"), 
		0, 
		&dwValueType, 
		(LPBYTE)szAddrVal,
		&cbAddrVal);

	if (ERROR_SUCCESS != lResult) 
	{
		// Ignore invalid values
		return FALSE;
	}

	if (cbAddrVal != CB_ADDR) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
			"Invalid Entry(A): %ls, ignored\n", szAddrVal);
		return FALSE;
	}

	//
	// 00:0B:D0:00:D4:2F to NDAS_DEVICE_ID
	//

	NDAS_DEVICE_ID deviceId = {0};
	BOOL success = pConvertStringToDeviceId(szAddrVal, &deviceId);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
			"Invalid Entry(D): %ls, ignored\n", szAddrVal);
		return FALSE;
	}

	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"Importing an entry: %s\n", 
		CNdasDeviceId(deviceId).ToStringA());

	TCHAR szNameVal[MAX_NDAS_DEVICE_NAME_LEN + 1] = {0};
	DWORD cbNameVal = sizeof(szNameVal);
	lResult = ::RegQueryValueEx(
		hEntryKey,
		_T("Name"),
		0,
		&dwValueType,
		(LPBYTE)szNameVal,
		&cbNameVal);

	if (ERROR_SUCCESS != lResult || _T('\0') == szNameVal[0]) 
	{
		TCHAR szDefaultName[MAX_NDAS_DEVICE_NAME_LEN + 1] = {0};
		success = _NdasSystemCfg.GetValueEx(
			_T("Devices"), 
			_T("DefaultPrefix"),
			szDefaultName,
			sizeof(szDefaultName));

		if (!success) 
		{
			COMVERIFY( StringCchCopy(
				szDefaultName, 
				MAX_NDAS_DEVICE_NAME_LEN + 1,
				_T("NDAS Device ")) );
		}

		hr = ::StringCchPrintf(
			szNameVal,
			MAX_NDAS_DEVICE_NAME_LEN,
			_T("%s %d"), 
			szDefaultName, 
			SlotNo);
	}


	BYTE pbSerialKeyVal[9];
	DWORD cbSerialKeyVal = sizeof(pbSerialKeyVal);
	lResult = ::RegQueryValueEx(
		hEntryKey,
		_T("SerialKey"),
		0,
		&dwValueType,
		(LPBYTE)pbSerialKeyVal,
		&cbSerialKeyVal);

	if (ERROR_SUCCESS != lResult) 
	{
		return FALSE;
	}

	if (cbSerialKeyVal != sizeof(pbSerialKeyVal)) 
	{
		return FALSE;
	}

	ACCESS_MASK fAccessMode = GENERIC_READ;
	if (0xFF == pbSerialKeyVal[8]) 
	{
		// Registered as RW
		fAccessMode |= GENERIC_WRITE;
	}
	else if (0x00 == pbSerialKeyVal[8]) 
	{
		// Registered as RO
	}
	else 
	{
		// Invalid value
		return FALSE;
	}

	//
	// Register function returns the locked pointer
	//

	CComPtr<INdasDevice> pNdasDevice;
	
	hr = DeviceRegister(
		SlotNo, 
		deviceId, 
		0, 
		NULL, 
		CComBSTR(szNameVal), 
		fAccessMode, 
		NULL, 
		&pNdasDevice);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
			"Failed to register %s at %d during import, error=0x%X",
			CNdasDeviceId(deviceId).ToStringA(), SlotNo, hr);
		return FALSE;
	}

	// Always enable this!
	COMVERIFY(pNdasDevice->put_Enabled(TRUE));

	return TRUE;
}

BOOL
CNdasDeviceRegistrar::pImportLegacySettings()
{
	NdasUiDbgCall( 2, "in\n" );

	BOOL success = FALSE;

	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"Importing legacy settings.\n");

	XTL::AutoKeyHandle hRootKey;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		_T("SOFTWARE\\XIMETA\\NetDisks"),
		0,
		KEY_READ,
		&hRootKey);

	if (ERROR_SUCCESS != lResult) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_WARNING,
			"Opening legacy configuration registry key failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	for (DWORD index = 0; ; ++index)
	{
		static CONST DWORD MAX_KEY_NAME = 200;
		FILETIME ftLastWritten;
		TCHAR szKeyName[MAX_KEY_NAME] = {0};
		DWORD cchKeyName = MAX_KEY_NAME;
		lResult = ::RegEnumKeyEx(
			hRootKey, 
			index, 
			szKeyName, 
			&cchKeyName,
			0,
			NULL,
			NULL,
			&ftLastWritten);

		if (ERROR_SUCCESS != lResult) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
				"RegEnumKeyEx failed, index=%d, error=0x%X\n", index, lResult);
			break;
		}

		int iKeyName;
		success = ::StrToIntEx(szKeyName, STIF_DEFAULT, &iKeyName);
		if (!success || 0 == iKeyName) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
				"KeyName conversion to integer failed, keyName=%ls\n", szKeyName);
			continue;
		}

		XTL::AutoKeyHandle hSubKey;
		lResult = ::RegOpenKeyEx(
			hRootKey,
			szKeyName,
			0,
			KEY_READ,
			&hSubKey);

		if (ERROR_SUCCESS != lResult) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
				"RegOpenKeyEx failed, keyName=%ls\n", szKeyName);
			continue;
		}

		pImportLegacyEntry(iKeyName, hSubKey);
	}

	return TRUE;
}

STDMETHODIMP
CNdasDeviceRegistrar::Bootstrap()
{
	NdasUiDbgCall( 2, "in\n" );

	HRESULT hr;

	BOOL success = FALSE;
	BOOL fMigrated = FALSE;

	//
	// Set bootstrapping flag to prevent multiple events 
	// for DeviceSetChange Events
	//

	m_fBootstrapping = TRUE;

	TCHAR szSubcontainer[30] = {0};
	for (DWORD i = 0; i < MAX_SLOT_NUMBER; ++i) 
	{
		COMVERIFY(StringCchPrintf(
			szSubcontainer, 30, _T("%s\\%04d"), CFG_CONTAINER, i));

		BOOL fAutoRegistered = FALSE;
		success = _NdasSystemCfg.GetValueEx(
			szSubcontainer,
			_T("AutoRegistered"),
			&fAutoRegistered);

		if (success && fAutoRegistered)
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
				"Deleting %ls\n", szSubcontainer);
			// Auto registered devices are not persistent
			// it is an error to show up here.
			// We just ignore those entries
			success = _NdasSystemCfg.DeleteContainer(szSubcontainer, TRUE);
			if (!success)
			{
				XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
					"Deleting a RegKey=%ls failed, error=0x%X\n", 
					szSubcontainer, GetLastError());
			}
			continue;
		}

		DWORD cbUsed;

		NDAS_DEVICE_ID_REG_DATA regData = {0};
		const NDASID_EXT_DATA* ndasIdExtension = NULL;

		ATLASSERT(regData.DeviceId.Reserved == 0 );

		success = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer, 
			_T("DeviceID2"),
			&regData,
			sizeof(NDAS_DEVICE_ID_REG_DATA),
			&cbUsed);

		if (!success) 
		{
			//
			// Non-extension data
			//
			success = _NdasSystemCfg.GetSecureValueEx(
				szSubcontainer, 
				_T("DeviceID"),
				&regData.DeviceId,
				sizeof(regData.DeviceId),
				&cbUsed);

			//
			// ignore read fault - tampered or not exists
			//
			if (!success || cbUsed != sizeof(NDAS_DEVICE_ID))
			{
				continue;
			}
			// For VID's other than 1 (or 0), DeviceID2 should be used instead.
			// In this case, VID is 1.
			// (Assume VID 0 is VID 1 to support registry entry created 
			// by older software(~3.11))
			regData.DeviceId.Vid = 1;
		}
		else
		{
			if (cbUsed != sizeof(NDAS_DEVICE_ID_REG_DATA))
			{
				//
				// maybe more recent versions, unrecognized, ignore
				//
				continue;
			}
			// DeviceId.VID may not be written correctly by some version of SW.
			regData.DeviceId.Vid = regData.NdasIdExtension.Vid;
			ndasIdExtension = &regData.NdasIdExtension;
		}

		if (regData.DeviceId.Reserved) {
			
			ATLASSERT(FALSE);
			regData.DeviceId.Reserved = 0;
		}

		//
		// RegFlags
		//

		DWORD regFlags;
		success = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("RegFlags"),
			&regFlags,
			sizeof(regFlags));

		if (!success)
		{
			regFlags = NDAS_DEVICE_REG_FLAG_NONE;
		}

		//
		// NDAS OEM Code
		//

		const NDAS_OEM_CODE* ndasOemCode = NULL;
		NDAS_OEM_CODE ndasOemCodeBuffer;
		success = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("OEMCode"),
			&ndasOemCodeBuffer,
			sizeof(NDAS_OEM_CODE),
			&cbUsed);

		if (success && cbUsed == sizeof(NDAS_OEM_CODE))
		{
			ndasOemCode = &ndasOemCodeBuffer;
		}

		//
		// Granted Access
		//

		ACCESS_MASK grantedAccess = GENERIC_READ;
		const DWORD cbBuffer = sizeof(ACCESS_MASK) + sizeof(NDAS_DEVICE_ID);
		BYTE pbBuffer[cbBuffer];

		success = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("GrantedAccess"),
			pbBuffer,
			cbBuffer);

		if (success) 
		{
			grantedAccess = *((ACCESS_MASK*)(pbBuffer));
		}
		grantedAccess |= GENERIC_READ; // to prevent invalid access mask configuration

		//
		// NDAS Device Name
		//

		TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
		success = _NdasSystemCfg.GetValueEx(
			szSubcontainer, 
			_T("DeviceName"),
			szDeviceName,
			sizeof(TCHAR)*(MAX_NDAS_DEVICE_NAME_LEN + 1));

		if (!success) 
		{
			COMVERIFY(StringCchCopy(
				szDeviceName, RTL_NUMBER_OF(szDeviceName), _T("NDAS Device")));
		}

		//
		// Register
		//

		CComPtr<INdasDevice> pNdasDevice;
		hr = DeviceRegister(
			i, 
			regData.DeviceId, 
			regFlags, 
			ndasIdExtension, 
			CComBSTR(szDeviceName),
			grantedAccess,
			ndasOemCode,
			&pNdasDevice);

		if (FAILED(hr)) {

			// This may happen due to auto-register feature!

			NdasUiDbgCall( 1, _T("Registration failed for %s, hr=0x%X\n"), CNdasDeviceId(regData.DeviceId).ToString(), hr );

			// During bootstrapping register may fail for unsupported VID.
			// In that case, we should retain this slot number to avoid
			// overwriting the existing data which may be created by
			// the higher version.

			m_slotbit[i] = true;
			continue;
		}

		BOOL fEnabled = FALSE;
		success = _NdasSystemCfg.GetValueEx(
			szSubcontainer,
			_T("Enabled"),
			&fEnabled);

		ATLASSERT( success && fEnabled );

		if (success && fEnabled) 
		{
			COMVERIFY(pNdasDevice->put_Enabled(fEnabled));
		}
	}

	//
	// Migration will be done only once 
	// if there is no registered devices in the current configurations
	// and if the migration flag (Install\Migrate = 1) is set
	//
	if (m_deviceSlotMap.size() == 0) 
	{
		success = _NdasSystemCfg.GetValueEx(_T("Install"), _T("Migrated"), &fMigrated);
		if (!success || !fMigrated) 
		{
			fMigrated = TRUE;
			pImportLegacySettings();
			_NdasSystemCfg.SetValueEx(_T("Install"), _T("Migrated"), fMigrated);
		}
	}

	//
	// Clear bootstrapping state
	//
	m_fBootstrapping = FALSE;

	return S_OK;
}

namespace
{
	// not used at this time (reserved for the future use)
	void pDeleteAutoRegistered(INdasDevice* pNdasDevice)
	{
		DWORD SlotNo, RegFlags;
		COMVERIFY(pNdasDevice->get_SlotNo(&SlotNo));
		COMVERIFY(pNdasDevice->get_RegisterFlags(&RegFlags));
		if (RegFlags & NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED)
		{
			TCHAR szContainer[30];

			COMVERIFY( StringCchPrintf(
				szContainer, 
				30, 
				_T("Devices\\%04d"), 
				SlotNo) );

			BOOL success = _NdasSystemCfg.DeleteContainer(szContainer, TRUE);
		}
	}
}

DWORD
CNdasDeviceRegistrar::pLookupEmptySlot()
{
	std::vector<bool>::const_iterator empty_begin = std::find(
		m_slotbit.begin(), m_slotbit.end(), 
		false);

	if (empty_begin == m_slotbit.end()) 
	{
		::SetLastError(NDASSVC_ERROR_DEVICE_ENTRY_SLOT_FULL);
		return 0;
	}

	DWORD emptySlotNo = static_cast<DWORD>(
		std::distance<std::vector<bool>::const_iterator>(
			m_slotbit.begin(), empty_begin));

	return emptySlotNo ;
}

struct NdasDeviceIsHidden : public std::unary_function<INdasDevice*, bool>
{
	bool operator()(INdasDevice* pNdasDevice) const
	{
		DWORD flags;
		COMVERIFY(pNdasDevice->get_RegisterFlags(&flags));
		if (flags & NDAS_DEVICE_REG_FLAG_HIDDEN)
		{
			return true;
		}
		return false;
	}
};

STDMETHODIMP 
CNdasDeviceRegistrar::get_NdasDevices(
	__in DWORD Flags, 
	__deref_out CInterfaceArray<INdasDevice>& NdasDevices)
{
	CAutoLock<CLock> autolock(&m_DataLock);

	NdasDevices.Copy(m_NdasDevices);
	autolock.Release();

	if (Flags & NDAS_ENUM_EXCLUDE_HIDDEN)
	{
		size_t count = NdasDevices.GetCount();
		for (size_t index = 0; index < count; ++index)
		{
			INdasDevice* pNdasDevice = NdasDevices.GetAt(index);
			if (NdasDeviceIsHidden()(pNdasDevice))
			{
				NdasDevices.RemoveAt(index);
				--index; --count;
			}
		}
	}
	return S_OK;
}

STDMETHODIMP
CNdasDeviceRegistrar::Shutdown()
{
	NdasUiDbgCall( 2, "in\n" );

	CAutoLock<CLock> autolock(&m_DataLock);

	m_deviceSlotMap.clear();
	m_deviceIdMap.clear();

	m_NdasDevices.RemoveAll();

	return S_OK;
}

namespace
{

BOOL
pCharToHex(TCHAR c, LPDWORD lpValue)
{
	XTLASSERT(!IsBadWritePtr(lpValue, sizeof(DWORD)));
	if (c >= _T('0') && c <= _T('9')) {
		*lpValue = c - _T('0') + 0x0;
		return TRUE;
	} else if (c >= _T('A') && c <= _T('F')) {
		*lpValue = c - _T('A') + 0xA;
		return TRUE;
	} else if (c >= _T('a') && c <= _T('f')) {
		*lpValue = c - _T('a') + 0xA;
		return TRUE;
	}
	return FALSE;
}

BOOL
pConvertStringToDeviceId(
	LPCTSTR szAddressString, 
	NDAS_DEVICE_ID* pDeviceId)
{
	XTLASSERT(!IsBadStringPtr(szAddressString,-1));
	XTLASSERT(!IsBadWritePtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));

	static CONST size_t CCH_ADDR = 17;
	size_t cch = 0;
	HRESULT hr = StringCchLength(szAddressString, STRSAFE_MAX_CCH, &cch);
	if (FAILED(hr) || cch != CCH_ADDR) 
	{
		return FALSE;
	}

	for (DWORD i = 0; i < 6; ++i) 
	{
		CONST TCHAR* psz = szAddressString + i * 3;
		DWORD v1 = 0, v2 = 0;
		BOOL success = 
			pCharToHex(psz[0], &v1) &&
			pCharToHex(psz[1], &v2);
		if (!success) 
		{
			return FALSE;
		}
		pDeviceId->Node[i] = static_cast<BYTE>(v1) * 0x10 + static_cast<BYTE>(v2);
	}
	return TRUE;
}

} // namespace

