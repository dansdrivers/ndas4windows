/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#include "stdafx.h"
#include "ndasdevreg.h"
#include "ndaserror.h"
// #include <ndas/ndasmsg.h>

#include "ndascfg.h"

#include "ndasinstman.h"
#include "ndaseventpub.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASDEVREG
#include "xdebug.h"

const TCHAR CNdasDeviceRegistrar::CFG_CONTAINER[] = _T("Devices");

CNdasDeviceRegistrar::
CNdasDeviceRegistrar(DWORD dwMaxSlotNo) :
	m_dwMaxSlotNo(dwMaxSlotNo)
{
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

CNdasDeviceRegistrar::
~CNdasDeviceRegistrar()
{
	delete [] m_pbSlotOccupied;

	DeviceSlotMap::iterator itr = m_deviceSlotMap.begin();
	for(; itr != m_deviceSlotMap.end(); itr++) {
		delete itr->second;
	}
	m_deviceSlotMap.clear();
	m_deviceIdMap.clear();
}

PCNdasDevice
CNdasDeviceRegistrar::
Register(NDAS_DEVICE_ID DeviceId, DWORD dwSlotNo)
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

	BOOL fSuccess = pDevice->Initialize();
	if (!fSuccess) {
//		DebugPrintError((ERROR_T("Device initialization failed!")));
		delete pDevice;
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

	TCHAR szContainer[12];
	HRESULT hr = ::StringCchPrintf(szContainer, 12, TEXT("Devices\\%d"), dwSlotNo);
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

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	(void) pEventPublisher->DeviceEntryChanged();

	return pDevice;
}

PCNdasDevice
CNdasDeviceRegistrar::
Register(NDAS_DEVICE_ID DeviceId)
{
	ximeta::CAutoLock autolock(this);

	DWORD dwEmptySlot = LookupEmptySlot();
	if (0 == dwEmptySlot) {
		return NULL;
	}

	return Register(DeviceId, dwEmptySlot);
}

BOOL 
CNdasDeviceRegistrar::
Unregister(NDAS_DEVICE_ID DeviceId)
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

	delete pDevice;

	TCHAR szContainer[12];
	HRESULT hr = ::StringCchPrintf(szContainer, 12, TEXT("Devices\\%d"), dwSlotNo);
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
CNdasDeviceRegistrar::
Unregister(DWORD dwSlotNo)
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

	delete pDevice;

	TCHAR szContainer[12];
	HRESULT hr = ::StringCchPrintf(szContainer, 12, TEXT("Devices\\%d"), dwSlotNo);
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


PCNdasDevice
CNdasDeviceRegistrar::
Find(DWORD dwSlotNo)
{
	ximeta::CAutoLock autolock(this);
	DeviceSlotMap::const_iterator itr = m_deviceSlotMap.find(dwSlotNo);
	return (m_deviceSlotMap.end() == itr) ? NULL : itr->second;
}

PCNdasDevice
CNdasDeviceRegistrar::
Find(NDAS_DEVICE_ID DeviceId)
{
	ximeta::CAutoLock autolock(this);
	DeviceIdMap::const_iterator itr = m_deviceIdMap.find(DeviceId);
	return (m_deviceIdMap.end() == itr) ? NULL : itr->second;
}

BOOL
CNdasDeviceRegistrar::
Bootstrap()
{

	TCHAR szSubcontainer[30];
	for (DWORD i = 0; i < MAX_SLOT_NUMBER; ++i) {

		NDAS_DEVICE_ID deviceId;

		HRESULT hr = ::StringCchPrintf(szSubcontainer, 30, TEXT("%s\\%d"), CFG_CONTAINER, i);
		_ASSERTE(SUCCEEDED(hr) && "CFG_CONTAINER is too large???");

		BOOL fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer, 
			_T("DeviceId"),
			&deviceId,
			sizeof(deviceId));

		// ignore read fault - tampered or not exists
		if (!fSuccess) {
			continue;
		}

		PCNdasDevice pDevice = Register(deviceId, i);
		_ASSERTE(NULL != pDevice && "Failure of registration should not happed during bootstrap!");
		
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
			MAX_NDAS_DEVICE_NAME_LEN + 1);

		if (fSuccess) {
			pDevice->SetName(szDeviceName);
		}

		BOOL bEnabled(FALSE);
		fSuccess = _NdasSystemCfg.GetValueEx(
			szSubcontainer,
			_T("Enabled"),
			&bEnabled);

		if (fSuccess && bEnabled) {
			pDevice->Enable(bEnabled);
		}
	}

	return TRUE;
}

DWORD
CNdasDeviceRegistrar::
LookupEmptySlot()
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
CNdasDeviceRegistrar::
Size()
{
	ximeta::CAutoLock autolock(this);

	return m_deviceSlotMap.size();
}

DWORD
CNdasDeviceRegistrar::
MaxSlotNo()
{
	return m_dwMaxSlotNo;
}

CNdasDeviceRegistrar::ConstIterator
CNdasDeviceRegistrar::
begin()
{
	return m_deviceSlotMap.begin();
}

CNdasDeviceRegistrar::ConstIterator
CNdasDeviceRegistrar::
end()
{
	return m_deviceSlotMap.end();
}
