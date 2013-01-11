/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#include "stdafx.h"
#include "ndasdevreg.h"
#include "ndasmsg.h"

#include "ndascfg.h"

#include "ndasinstman.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "autores.h"

#include <shlwapi.h>

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASDEVREG
#include "xdebug.h"

const TCHAR CNdasDeviceRegistrar::CFG_CONTAINER[] = _T("Devices");

static
BOOL
pCharToHex(TCHAR c, LPDWORD lpValue);

static
BOOL
pConvertStringToDeviceId(
	LPCTSTR szAddressString, 
	NDAS_DEVICE_ID* pDeviceId);

CNdasDeviceRegistrar::CNdasDeviceRegistrar(DWORD dwMaxSlotNo) :
	m_fBootstrapping(FALSE),
	m_dwMaxSlotNo(dwMaxSlotNo)
{
	DBGPRT_TRACE(_FT("ctor\n"));

	m_pbSlotOccupied = new BOOL[m_dwMaxSlotNo + 1];
	if (NULL == m_pbSlotOccupied) {
		// Out of memory... Throw exception?
		// throw CException(EX_OUT_OF_MEMORY);
	}

	// Slot 0 is always occupied and reserved!
	m_pbSlotOccupied[0] = TRUE;
	for (DWORD i = 1; i <= m_dwMaxSlotNo; i++) {
		m_pbSlotOccupied[i] = FALSE;
	}
}

CNdasDeviceRegistrar::~CNdasDeviceRegistrar()
{
	delete [] m_pbSlotOccupied;

	DeviceSlotMap::iterator itr = m_deviceSlotMap.begin();
	for(; itr != m_deviceSlotMap.end(); itr++) {
		CNdasDevice* pDevice = itr->second;
		pDevice->Release();
	}
	m_deviceSlotMap.clear();
	m_deviceIdMap.clear();

	DBGPRT_TRACE(_FT("dtor\n"));
}

PCNdasDevice
CNdasDeviceRegistrar::Register(const NDAS_DEVICE_ID& DeviceId, DWORD dwSlotNo)
{
	//
	// this will lock this class from here
	// and releases the lock when the function returns;
	//
	ximeta::CAutoLock autolock(this);
	
	DPInfo(_FT("Registering device %s at slot %d\n"), 
		LPCTSTR(CNdasDeviceId(DeviceId)), dwSlotNo);

	// check slot number
	if (dwSlotNo < 1 || dwSlotNo > m_dwMaxSlotNo) {
		::SetLastError(NDASHLPSVC_ERROR_INVALID_SLOT_NUMBER);
		return NULL;
	}

	// check and see if the slot is occupied
	if (m_pbSlotOccupied[dwSlotNo]) {
		::SetLastError(NDASHLPSVC_ERROR_SLOT_ALREADY_OCCUPIED);
		return NULL;
	}

	// find an duplicate address
	if (NULL != Find(DeviceId)) {
		::SetLastError(NDASHLPSVC_ERROR_DUPLICATE_DEVICE_ENTRY);
		return NULL;
	}

	// register
	PCNdasDevice pDevice = new CNdasDevice(dwSlotNo, DeviceId);
	if (NULL == pDevice) {
		// memory allocation failed
		// No need to set error here!
		return NULL;
	}
	pDevice->AddRef();

	BOOL fSuccess = pDevice->Initialize();
	if (!fSuccess) {
//		DebugPrintError((ERROR_T("Device initialization failed!")));
		pDevice->Release();
		return NULL;
	}

	m_pbSlotOccupied[dwSlotNo] = TRUE;

	bool insertResult;

	insertResult = 
		m_deviceSlotMap.insert(DeviceSlotMap::value_type(dwSlotNo, pDevice)).second;
	_ASSERTE(insertResult == true);
	insertResult =	
		m_deviceIdMap.insert(DeviceIdMap::value_type(DeviceId, pDevice)).second;
	_ASSERTE(insertResult == true);

	_ASSERTE(m_deviceSlotMap.size() == m_deviceIdMap.size());

	TCHAR szContainer[30];
	HRESULT hr = ::StringCchPrintf(szContainer, 30, TEXT("Devices\\%04d"), dwSlotNo);
	_ASSERT(SUCCEEDED(hr));

	fSuccess = _NdasSystemCfg.SetSecureValueEx(
		szContainer, 
		_T("DeviceId"), 
		&DeviceId, 
		sizeof(DeviceId));

	if (!fSuccess) {
		DPWarningEx(
			_FT("Writing registration entry to the registry failed at %s.\n"), 
			szContainer);
	}

	//
	// During bootstrapping, we do not publish this event
	// Bootstrapper will do this later.
	//
	if (!m_fBootstrapping) {
		CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
		(VOID) pEventPublisher->DeviceEntryChanged();
	}

	return pDevice;
}

PCNdasDevice
CNdasDeviceRegistrar::Register(const NDAS_DEVICE_ID& DeviceId)
{
	ximeta::CAutoLock autolock(this);

	DWORD dwEmptySlot = LookupEmptySlot();
	if (0 == dwEmptySlot) {
		return NULL;
	}

	return Register(DeviceId, dwEmptySlot);
}

BOOL 
CNdasDeviceRegistrar::Unregister(const NDAS_DEVICE_ID& DeviceId)
{
	ximeta::CAutoLock autolock(this);

	CNdasDeviceId cdevid(DeviceId);
	DPInfo(_FT("Unregister device %s\n"), (LPCTSTR)cdevid);

	DeviceIdMap::iterator itrId = m_deviceIdMap.find(DeviceId);
	if (m_deviceIdMap.end() == itrId) {
		// TODO: ::SetLastError(NDAS_ERROR_DEVICE_NOT_FOUND);
		// TODO: Make more specific error code
		::SetLastError(NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
	}

	PCNdasDevice pDevice = itrId->second;
	
	if (pDevice->GetStatus() != NDAS_DEVICE_STATUS_DISABLED) {
		// TODO: ::SetLastError(NDAS_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		// TODO: Make more specific error code
		::SetLastError(NDASHLPSVC_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		return FALSE;
	}

	DWORD dwSlotNo = pDevice->GetSlotNo();
	
	_ASSERT(0 != dwSlotNo);

	DeviceSlotMap::iterator itrSlot = m_deviceSlotMap.find(dwSlotNo);

	m_deviceIdMap.erase(itrId);

	m_deviceSlotMap.erase(itrSlot);
	m_pbSlotOccupied[dwSlotNo] = FALSE;

	pDevice->Release();

	TCHAR szContainer[30];
	HRESULT hr = ::StringCchPrintf(szContainer, 30, TEXT("Devices\\%04d"), dwSlotNo);
	_ASSERT(SUCCEEDED(hr));

	BOOL fSuccess = _NdasSystemCfg.DeleteContainer(szContainer, TRUE);
	
	if (!fSuccess) {
		DPWarningEx(
			_FT("Deleting registration entry from the registry failed at %s.\n"), 
			szContainer);
	}

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	(void) pEventPublisher->DeviceEntryChanged();

	return TRUE;
}

BOOL
CNdasDeviceRegistrar::Unregister(DWORD dwSlotNo)
{
	ximeta::CAutoLock autolock(this);

	DeviceSlotMap::iterator itrSlot = 
		m_deviceSlotMap.find(dwSlotNo);

	if (m_deviceSlotMap.end() == itrSlot) {
		// TODO: Make more specific error code
		::SetLastError(NDASHLPSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
		return FALSE;
	}

	PCNdasDevice pDevice = itrSlot->second;

	if (pDevice->GetStatus() != NDAS_DEVICE_STATUS_DISABLED) {
		// TODO: ::SetLastError(NDAS_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		// TODO: Make more specific error code
		::SetLastError(NDASHLPSVC_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		return FALSE;
	}

	DeviceIdMap::iterator itrId = 
		m_deviceIdMap.find(pDevice->GetDeviceId());

	_ASSERTE(m_deviceIdMap.end() != itrId);

	m_deviceIdMap.erase(itrId);
	m_deviceSlotMap.erase(itrSlot);
	m_pbSlotOccupied[dwSlotNo] = FALSE;

	pDevice->Release();

	TCHAR szContainer[30];
	HRESULT hr = ::StringCchPrintf(szContainer, 30, TEXT("Devices\\%04d"), dwSlotNo);
	_ASSERT(SUCCEEDED(hr));

	BOOL fSuccess = _NdasSystemCfg.DeleteContainer(szContainer, FALSE);
	
	if (!fSuccess) {
		DPWarningEx(
			_FT("Deleting registration entry from the registry failed at %s.\n"), 
			szContainer);
	}

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	(void) pEventPublisher->DeviceEntryChanged();

	return TRUE;

}


PCNdasDevice
CNdasDeviceRegistrar::Find(DWORD dwSlotNo)
{
	ximeta::CAutoLock autolock(this);
	DeviceSlotMap::const_iterator itr = m_deviceSlotMap.find(dwSlotNo);
	return (m_deviceSlotMap.end() == itr) ? NULL : itr->second;
}

PCNdasDevice
CNdasDeviceRegistrar::Find(const NDAS_DEVICE_ID& DeviceId)
{
	ximeta::CAutoLock autolock(this);
	DeviceIdMap::const_iterator itr = m_deviceIdMap.find(DeviceId);
	return (m_deviceIdMap.end() == itr) ? NULL : itr->second;
}

BOOL
CNdasDeviceRegistrar::ImportLegacyEntry(DWORD dwSlotNo, HKEY hEntryKey)
{
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

	if (ERROR_SUCCESS != lResult) {
		// Ignore invalid values
		return FALSE;
	}

	if (cbAddrVal != CB_ADDR) {
		DBGPRT_ERR(_FT("Invalid Entry(A): %s, ignored\n"), szAddrVal);
		return FALSE;
	}

	//
	// 00:0B:D0:00:D4:2F to NDAS_DEVICE_ID
	//

	NDAS_DEVICE_ID deviceId = {0};
	BOOL fSuccess = pConvertStringToDeviceId(szAddrVal, &deviceId);
	if (!fSuccess) {
		DBGPRT_ERR(_FT("Invalid Entry(D): %s, ignored\n"), szAddrVal);
		return FALSE;
	}

	DBGPRT_INFO(_FT("Importing an entry: %s\n"), CNdasDeviceId(deviceId).ToString());

	TCHAR szNameVal[MAX_NDAS_DEVICE_NAME_LEN + 1] = {0};
	DWORD cbNameVal = sizeof(szNameVal);
	lResult = ::RegQueryValueEx(
		hEntryKey,
		_T("Name"),
		0,
		&dwValueType,
		(LPBYTE)szNameVal,
		&cbNameVal);

	if (ERROR_SUCCESS != lResult || _T('\0') == szNameVal[0]) {
		TCHAR szDefaultName[MAX_NDAS_DEVICE_NAME_LEN + 1] = {0};
		fSuccess = _NdasSystemCfg.GetValueEx(
			_T("Devices"), 
			_T("DefaultPrefix"),
			szDefaultName,
			sizeof(szDefaultName));

		if (!fSuccess) {
			hr = ::StringCchCopy(
				szDefaultName, 
				MAX_NDAS_DEVICE_NAME_LEN + 1,
				_T("NDAS Device "));
			_ASSERTE(SUCCEEDED(hr));
		}

		hr = ::StringCchPrintf(
			szNameVal,
			MAX_NDAS_DEVICE_NAME_LEN,
			_T("%s %d"), 
			szDefaultName, 
			dwSlotNo);
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

	if (ERROR_SUCCESS != lResult) {
		return FALSE;
	}

	if (cbSerialKeyVal != sizeof(pbSerialKeyVal)) {
		return FALSE;
	}

	ACCESS_MASK fAccessMode = GENERIC_READ;
	if (0xFF == pbSerialKeyVal[8]) {
		// Registered as RW
		fAccessMode |= GENERIC_WRITE;
	} else if (0x00 == pbSerialKeyVal[8]) {
		// Registered as RO
	} else {
		// Invalid value
		return FALSE;
	}

	PCNdasDevice pDevice = Register(deviceId, dwSlotNo);

	if (NULL == pDevice) {
		DBGPRT_ERR_EX(_FT("Failed to register %s at %d during import: "),
			CNdasDeviceId(deviceId).ToString(),
			dwSlotNo);
		return FALSE;
	}

	// Always enable this!
	pDevice->Enable(TRUE);
	pDevice->SetName(szNameVal);
	pDevice->SetGrantedAccess(fAccessMode);

	return TRUE;
}

BOOL
CNdasDeviceRegistrar::ImportLegacySettings()
{
	BOOL fSuccess = FALSE;

	DBGPRT_INFO(_FT("Importing a legacy settings.\n"));

	HKEY hRootKey = (HKEY) INVALID_HANDLE_VALUE;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		_T("SOFTWARE\\XIMETA\\NetDisks"),
		0,
		KEY_READ,
		&hRootKey);

	if (ERROR_SUCCESS != lResult) {
		DBGPRT_ERR_EX(_FT("Opening a legacy configuration key has failed:"));
		return FALSE;
	}

	AutoHKey autoRootKey = hRootKey;

	DWORD n = 0;
	
	while (TRUE) {

		static CONST DWORD MAX_KEY_NAME = 200;
		FILETIME ftLastWritten;
		TCHAR szKeyName[MAX_KEY_NAME] = {0};
		DWORD cchKeyName = MAX_KEY_NAME;
		lResult = ::RegEnumKeyEx(
			hRootKey, 
			n, 
			szKeyName, 
			&cchKeyName,
			0,
			NULL,
			NULL,
			&ftLastWritten);

		if (ERROR_SUCCESS != lResult) {
			break;
		}

		int iKeyName;
		fSuccess = ::StrToIntEx(szKeyName, STIF_DEFAULT, &iKeyName);
		if (!fSuccess || 0 == iKeyName) {
			continue;
		}

		HKEY hSubKey = (HKEY) INVALID_HANDLE_VALUE;

		lResult = ::RegOpenKeyEx(
			hRootKey,
			szKeyName,
			0,
			KEY_READ,
			&hSubKey);

		if (ERROR_SUCCESS != lResult) {
			continue;
		}

		AutoHKey autoSubKey = hSubKey;

		ImportLegacyEntry(iKeyName, hSubKey);

		++n;
	}

	return TRUE;
}

BOOL
CNdasDeviceRegistrar::Bootstrap()
{
	BOOL fSuccess = FALSE;
	BOOL fMigrated = FALSE;

	//
	// Set bootstrapping flag to prevent multiple events 
	// for DeviceSetChange Events
	//

	m_fBootstrapping = TRUE;

	TCHAR szSubcontainer[30];
	for (DWORD i = 0; i < MAX_SLOT_NUMBER; ++i) {

		NDAS_DEVICE_ID deviceId;

		HRESULT hr = ::StringCchPrintf(szSubcontainer, 30, TEXT("%s\\%04d"), CFG_CONTAINER, i);
		_ASSERTE(SUCCEEDED(hr) && "CFG_CONTAINER is too large???");

		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer, 
			_T("DeviceID"),
			&deviceId,
			sizeof(deviceId));

		// ignore read fault - tampered or not exists
		if (!fSuccess) {
			continue;
		}

		PCNdasDevice pDevice = Register(deviceId, i);
		// _ASSERTE(NULL != pDevice && "Failure of registration should not happed during bootstrap!");
		// This may happen due to auto-register feature!
		if (NULL == pDevice) {
			continue;
		}
		
		ACCESS_MASK grantedAccess = GENERIC_READ;
		const DWORD cbBuffer = sizeof(ACCESS_MASK) + sizeof(NDAS_DEVICE_ID);
		BYTE pbBuffer[cbBuffer];
		
		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("GrantedAccess"),
			pbBuffer,
			cbBuffer);

		if (fSuccess) {
			grantedAccess = *((ACCESS_MASK*)(pbBuffer));
		}
		grantedAccess |= GENERIC_READ; // to prevent invalid access mask configuration
		// _ASSERTE(grantedAccess & GENERIC_READ); // invalid configuration?
		pDevice->SetGrantedAccess(grantedAccess);

		TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
		fSuccess = _NdasSystemCfg.GetValueEx(
			szSubcontainer, 
			_T("DeviceName"),
			szDeviceName,
			sizeof(TCHAR)*(MAX_NDAS_DEVICE_NAME_LEN + 1));

		if (fSuccess) {
			pDevice->SetName(szDeviceName);
		}

		BOOL fEnabled = FALSE;
		fSuccess = _NdasSystemCfg.GetValueEx(
			szSubcontainer,
			_T("Enabled"),
			&fEnabled);

		if (fSuccess && fEnabled) {
			pDevice->Enable(fEnabled);
		}
	}

	//
	// Migration will be done only once 
	// if there is no registered devices in the current configurations
	// and if the migration flag (Install\Migrate = 1) is set
	//
	if (m_deviceSlotMap.size() == 0) {
		fSuccess = _NdasSystemCfg.GetValueEx(_T("Install"), _T("Migrated"), &fMigrated);
		if (!fSuccess || !fMigrated) {
			fMigrated = TRUE;
			ImportLegacySettings();
			_NdasSystemCfg.SetValueEx(_T("Install"), _T("Migrated"), fMigrated);
		}
	}

	//
	// Clear bootstrapping state
	//
	m_fBootstrapping = FALSE;
	return TRUE;
}

DWORD
CNdasDeviceRegistrar::LookupEmptySlot()
{
	ximeta::CAutoLock autolock(this);

	for (DWORD i = 1; i <= m_dwMaxSlotNo; ++i) {
		if (FALSE == m_pbSlotOccupied[i]) {
			return i;
		}
	}
	::SetLastError(NDASHLPSVC_ERROR_DEVICE_ENTRY_SLOT_FULL);
	return 0;
}

DWORD
CNdasDeviceRegistrar::Size()
{
	ximeta::CAutoLock autolock(this);

	return m_deviceSlotMap.size();
}

DWORD
CNdasDeviceRegistrar::MaxSlotNo()
{
	return m_dwMaxSlotNo;
}

CNdasDeviceRegistrar::ConstIterator
CNdasDeviceRegistrar::begin()
{
	return m_deviceSlotMap.begin();
}

CNdasDeviceRegistrar::ConstIterator
CNdasDeviceRegistrar::end()
{
	return m_deviceSlotMap.end();
}

static
BOOL
pCharToHex(TCHAR c, LPDWORD lpValue)
{
	_ASSERTE(!IsBadWritePtr(lpValue, sizeof(DWORD)));
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

static
BOOL
pConvertStringToDeviceId(
	LPCTSTR szAddressString, 
	NDAS_DEVICE_ID* pDeviceId)
{
	_ASSERTE(!IsBadStringPtr(szAddressString,-1));
	_ASSERTE(!IsBadWritePtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));

	static CONST size_t CCH_ADDR = 17;
	size_t cch = 0;
	HRESULT hr = StringCchLength(szAddressString, STRSAFE_MAX_CCH, &cch);
	if (FAILED(hr) || cch != CCH_ADDR) {
		return FALSE;
	}

	for (DWORD i = 0; i < 6; ++i) {
		CONST TCHAR* psz = szAddressString + i * 3;
		DWORD v1 = 0, v2 = 0;
		BOOL fSuccess = 
			pCharToHex(psz[0], &v1) &&
			pCharToHex(psz[1], &v2);
		if (!fSuccess) {
			return FALSE;
		}
		pDeviceId->Node[i] = v1 * 0x10 + v2;
	}
	return TRUE;
}
