/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#include "stdafx.h"
#include "ndasdevreg.h"
#include <ndas/ndasmsg.h>

#include "ndascfg.h"

#include "ndaseventpub.h"
#include "ndasobjs.h"

#include <shlwapi.h>

#include "trace.h"
#ifdef RUN_WPP
#include "ndasdevreg.tmh"
#endif

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

CNdasDeviceRegistrar::
CNdasDeviceRegistrar(
	CNdasService& service,
	DWORD MaxSlotNo) :
	m_service(service),
	m_fBootstrapping(FALSE),
	m_dwMaxSlotNo(MaxSlotNo),
	m_slotbit(MaxSlotNo + 1)
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

CNdasDevicePtr
CNdasDeviceRegistrar::Register(
	__in_opt DWORD SlotNo,
	__in const NDAS_DEVICE_ID& DeviceId,
	__in DWORD RegFlags,
	__in_opt const NDASID_EXT_DATA* NdasIdExtension)
{
	//
	// this will lock this class from here
	// and releases the lock when the function returns;
	//
	InstanceAutoLock autolock(this);
	
	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"Registering device %s at slot %d\n",
		CNdasDeviceId(DeviceId).ToStringA(), SlotNo);

	if (NULL == NdasIdExtension)
	{
		NdasIdExtension = &NDAS_ID_EXTENSION_DEFAULT;
	}

	//
	// Only DEFAULT and SEAGATE are currently implemented
	//
	if (NDAS_VID_SEAGATE != NdasIdExtension->VID &&
		NDAS_VID_DEFAULT != NdasIdExtension->VID)
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
			"Unknown Vendor ID=0x%02X\n",
			NdasIdExtension->VID);

		::SetLastError(NDASSVC_ERROR_UNKNOWN_VENDOR_ID);
		return CNdasDevicePtr();
	}

	// If SlotNo is zero, automatically assign it.
	// check slot number
	if (0 == SlotNo)
	{
		SlotNo = LookupEmptySlot();
		if (0 == SlotNo)
		{
			return CNdasDevicePtr();
		}
	}
	else if (SlotNo > m_dwMaxSlotNo)
	{
		::SetLastError(NDASSVC_ERROR_INVALID_SLOT_NUMBER);
		return CNdasDevicePtr();
	}

	// check and see if the slot is occupied
	if (m_slotbit[SlotNo])
	{
		::SetLastError(NDASSVC_ERROR_SLOT_ALREADY_OCCUPIED);
		return CNdasDevicePtr();
	}

	// find an duplicate address
	{
		CNdasDevicePtr pExistingDevice = Find(DeviceId);
		if (0 != pExistingDevice.get()) 
		{
			::SetLastError(NDASSVC_ERROR_DUPLICATE_DEVICE_ENTRY);
			return CNdasDevicePtr();
		}
	}

	// register 
	CNdasDevicePtr pDevice(new CNdasDevice(SlotNo, DeviceId, RegFlags, NdasIdExtension));
	if (0 == pDevice.get()) 
	{
		// memory allocation failed
		// No need to set error here!
		return CNdasDevicePtr();
	}

	BOOL fSuccess = pDevice->Initialize();
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
			"Device initialization failed, error=0x%X\n", GetLastError());
		return CNdasDevicePtr();
	}

	m_slotbit[SlotNo] = true;

	bool insertResult;

	XTLVERIFY( m_deviceSlotMap.insert(std::make_pair(SlotNo, pDevice)).second );
	//DeviceSlotMap::value_type(SlotNo, pDevice)).second;

	XTLVERIFY( m_deviceIdMap.insert(std::make_pair(DeviceId, pDevice)).second );
	//DeviceIdMap::value_type(DeviceId, pDevice)).second;

	XTLASSERT(m_deviceSlotMap.size() == m_deviceIdMap.size());

	//
	// When NdasIdExtension is NULL, NDAS_ID_EXTENSION_DEFAULT is assigned already
	//

	if (RegFlags & NDAS_DEVICE_REG_FLAG_VOLATILE)
	{
	}
	else
	{
		XTL::CStaticStringBuffer<30> containerName(_T("Devices\\%04d"), SlotNo);

		if (0 != memcmp(&NDAS_ID_EXTENSION_DEFAULT, NdasIdExtension, sizeof(NDASID_EXT_DATA)))
		{
			NDAS_DEVICE_ID_REG_DATA regData = {0};
			regData.DeviceId = DeviceId;
			regData.NdasIdExtension = *NdasIdExtension;

			fSuccess = _NdasSystemCfg.SetSecureValueEx(
				containerName, 
				_T("DeviceID2"), 
				&regData, 
				sizeof(regData));
		}
		else
		{
			fSuccess = _NdasSystemCfg.SetSecureValueEx(
				containerName, 
				_T("DeviceID"), 
				&DeviceId, 
				sizeof(DeviceId));
		}

		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_WARNING,
				"Writing registration entry to the registry failed at %ls, error=0x%X\n", 
				containerName.ToString(), GetLastError());
		}

		fSuccess = _NdasSystemCfg.SetSecureValueEx(
			containerName,
			_T("RegFlags"),
			&RegFlags,
			sizeof(RegFlags));

		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_WARNING,
				"Writing registration entry to the registry failed at %ls, error=0x%X\n", 
				containerName.ToString(), GetLastError());
		}
	}

	//
	// During bootstrapping, we do not publish this event
	// Bootstrap process will publish an event later
	//
	if (!m_fBootstrapping) 
	{
		(void) m_service.GetEventPublisher().DeviceEntryChanged();
	}

	return pDevice;
}

BOOL 
CNdasDeviceRegistrar::Unregister(const NDAS_DEVICE_ID& DeviceId)
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
		"Unregister device %s\n", CNdasDeviceId(DeviceId).ToStringA());

	DeviceIdMap::iterator itrId = m_deviceIdMap.find(DeviceId);
	if (m_deviceIdMap.end() == itrId) 
	{
		::SetLastError(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
	}

	CNdasDevicePtr pDevice = itrId->second;
	
	if (pDevice->GetStatus() != NDAS_DEVICE_STATUS_DISABLED) 
	{
		::SetLastError(NDASSVC_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		return FALSE;
	}

	DWORD SlotNo = pDevice->GetSlotNo();
	XTLASSERT(0 != SlotNo);

	DeviceSlotMap::iterator itrSlot = m_deviceSlotMap.find(SlotNo);

	m_deviceIdMap.erase(itrId);
	m_deviceSlotMap.erase(itrSlot);
	m_slotbit[SlotNo] = false;

	XTL::CStaticStringBuffer<30> containerName(_T("Devices\\%04d"), SlotNo);
	BOOL fSuccess = _NdasSystemCfg.DeleteContainer(containerName, TRUE);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_WARNING,
			"Deleting registration entry from the registry failed at %ls, error=0x%X\n", 
			containerName, GetLastError());
	}

	(void) m_service.GetEventPublisher().DeviceEntryChanged();

	return TRUE;
}

BOOL
CNdasDeviceRegistrar::Unregister(DWORD SlotNo)
{
	InstanceAutoLock autolock(this);

	DeviceSlotMap::iterator slot_itr = m_deviceSlotMap.find(SlotNo);

	if (m_deviceSlotMap.end() == slot_itr) 
	{
		// TODO: Make more specific error code
		::SetLastError(NDASSVC_ERROR_DEVICE_ENTRY_NOT_FOUND);
		return FALSE;
	}

	CNdasDevicePtr pDevice = slot_itr->second;

	if (NDAS_DEVICE_STATUS_DISABLED != pDevice->GetStatus()) 
	{
		// TODO: ::SetLastError(NDAS_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		// TODO: Make more specific error code
		::SetLastError(NDASSVC_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE);
		return FALSE;
	}

	DeviceIdMap::iterator id_itr = m_deviceIdMap.find(pDevice->GetDeviceId());

	XTLASSERT(m_deviceIdMap.end() != id_itr);

	m_deviceIdMap.erase(id_itr);
	m_deviceSlotMap.erase(slot_itr);
	m_slotbit[SlotNo] = FALSE;

	XTL::CStaticStringBuffer<30> containerName(_T("Devices\\%04d"), SlotNo);
	BOOL fSuccess = _NdasSystemCfg.DeleteContainer(containerName, FALSE);
	
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_WARNING,
			"Deleting registration entry from the registry failed at %ls, error=0x%X\n", 
			containerName, GetLastError());
	}

	(void) m_service.GetEventPublisher().DeviceEntryChanged();

	return TRUE;

}


CNdasDevicePtr
CNdasDeviceRegistrar::Find(DWORD SlotNo)
{
	InstanceAutoLock autolock(this);
	DeviceSlotMap::const_iterator itr = m_deviceSlotMap.find(SlotNo);
	CNdasDevicePtr pDevice = (m_deviceSlotMap.end() == itr) ? CNdasDevicePtr() : itr->second;
	return pDevice;
}

CNdasDevicePtr
CNdasDeviceRegistrar::Find(const NDAS_DEVICE_ID& DeviceId)
{
	InstanceAutoLock autolock(this);
	DeviceIdMap::const_iterator itr = m_deviceIdMap.find(DeviceId);
	CNdasDevicePtr pDevice = (m_deviceIdMap.end() == itr) ? CNdasDevicePtr() : itr->second;
	return pDevice;
}

CNdasDevicePtr
CNdasDeviceRegistrar::Find(const NDAS_DEVICE_ID_EX& Device)
{
	return Device.UseSlotNo ? Find(Device.SlotNo) : Find(Device.DeviceId);
}

BOOL
CNdasDeviceRegistrar::ImportLegacyEntry(DWORD SlotNo, HKEY hEntryKey)
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
	BOOL fSuccess = pConvertStringToDeviceId(szAddrVal, &deviceId);
	if (!fSuccess) 
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
		fSuccess = _NdasSystemCfg.GetValueEx(
			_T("Devices"), 
			_T("DefaultPrefix"),
			szDefaultName,
			sizeof(szDefaultName));

		if (!fSuccess) 
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

	CNdasDevicePtr pDevice = Register(SlotNo, deviceId, 0, NULL);
	if (0 == pDevice.get()) 
	{
		XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
			"Failed to register %s at %d during import, error=0x%X",
			CNdasDeviceId(deviceId).ToStringA(), SlotNo, GetLastError());
		return FALSE;
	}

	// Always enable this!
	XTLVERIFY( pDevice->Enable(TRUE) );
	pDevice->SetName(szNameVal);
	pDevice->SetGrantedAccess(fAccessMode);

	return TRUE;
}

BOOL
CNdasDeviceRegistrar::ImportLegacySettings()
{
	BOOL fSuccess = FALSE;

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

	DWORD n = 0;
	
	while (TRUE) 
	{

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

		if (ERROR_SUCCESS != lResult) 
		{
			break;
		}

		int iKeyName;
		fSuccess = ::StrToIntEx(szKeyName, STIF_DEFAULT, &iKeyName);
		if (!fSuccess || 0 == iKeyName) 
		{
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
			continue;
		}


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

	TCHAR szSubcontainer[30] = {0};
	for (DWORD i = 0; i < MAX_SLOT_NUMBER; ++i) 
	{
		COMVERIFY(StringCchPrintf(
			szSubcontainer, 30, _T("%s\\%04d"), CFG_CONTAINER, i));

		BOOL fAutoRegistered = FALSE;
		fSuccess = _NdasSystemCfg.GetValueEx(
			szSubcontainer,
			_T("AutoRegistered"),
			&fAutoRegistered);

		if (fSuccess && fAutoRegistered)
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_INFORMATION,
				"Deleting %ls\n", szSubcontainer);
			// Auto registered devices are not persistent
			// it is an error to show up here.
			// We just ignore those entries
			fSuccess = _NdasSystemCfg.DeleteContainer(szSubcontainer, TRUE);
			if (!fSuccess)
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

		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer, 
			_T("DeviceID2"),
			&regData,
			sizeof(NDAS_DEVICE_ID_REG_DATA),
			&cbUsed);

		if (!fSuccess) 
		{
			//
			// Non-extension data
			//
			fSuccess = _NdasSystemCfg.GetSecureValueEx(
				szSubcontainer, 
				_T("DeviceID"),
				&regData.DeviceId,
				sizeof(regData.DeviceId),
				&cbUsed);

			//
			// ignore read fault - tampered or not exists
			//
			if (!fSuccess || cbUsed != sizeof(NDAS_DEVICE_ID))
			{
				continue;
			}
			if (regData.DeviceId.VID == 0) {
				// Assume VID 0 is VID 1 to support registry entry created by older software(~3.11)
				regData.DeviceId.VID = 1;
			}
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
			ndasIdExtension = &regData.NdasIdExtension;
		}

		DWORD regFlags;
		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("RegFlags"),
			&regFlags,
			sizeof(regFlags));

		if (!fSuccess)
		{
			regFlags = NDAS_DEVICE_REG_FLAG_NONE;
		}

		CNdasDevicePtr pDevice = Register(
			i, 
			regData.DeviceId, 
			regFlags,
			ndasIdExtension);

		// This may happen due to auto-register feature!
		if (CNdasDeviceNullPtr == pDevice) 
		{
			XTLTRACE2(NDASSVC_NDASDEVICEREGISTRAR, TRACE_LEVEL_ERROR,
				"Registration failed for %s, error=0x%X\n",
				CNdasDeviceId(regData.DeviceId).ToStringA(),
				GetLastError());
			//
			// During bootstrapping register may fail for unsupported VID.
			// In that case, we should retain this slot number to avoid
			// overwriting the existing data which may be created by
			// the higher version.
			//
			m_slotbit[i] = true;
			continue;
		}

		NDAS_OEM_CODE oemCode;
		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("OEMCode"),
			&oemCode,
			sizeof(NDAS_OEM_CODE),
			&cbUsed);

		if (fSuccess && cbUsed == sizeof(NDAS_OEM_CODE))
		{
			pDevice->SetOemCode(oemCode);
		}

		ACCESS_MASK grantedAccess = GENERIC_READ;
		const DWORD cbBuffer = sizeof(ACCESS_MASK) + sizeof(NDAS_DEVICE_ID);
		BYTE pbBuffer[cbBuffer];
		
		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			szSubcontainer,
			_T("GrantedAccess"),
			pbBuffer,
			cbBuffer);

		if (fSuccess) 
		{
			grantedAccess = *((ACCESS_MASK*)(pbBuffer));
		}
		grantedAccess |= GENERIC_READ; // to prevent invalid access mask configuration
		// XTLASSERT(grantedAccess & GENERIC_READ); // invalid configuration?
		pDevice->SetGrantedAccess(grantedAccess);

		TCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
		fSuccess = _NdasSystemCfg.GetValueEx(
			szSubcontainer, 
			_T("DeviceName"),
			szDeviceName,
			sizeof(TCHAR)*(MAX_NDAS_DEVICE_NAME_LEN + 1));

		if (fSuccess) 
		{
			pDevice->SetName(szDeviceName);
		}

		BOOL fEnabled = FALSE;
		fSuccess = _NdasSystemCfg.GetValueEx(
			szSubcontainer,
			_T("Enabled"),
			&fEnabled);

		if (fSuccess && fEnabled) 
		{
			pDevice->Enable(fEnabled);
		}
	}

	//
	// Migration will be done only once 
	// if there is no registered devices in the current configurations
	// and if the migration flag (Install\Migrate = 1) is set
	//
	if (m_deviceSlotMap.size() == 0) 
	{
		fSuccess = _NdasSystemCfg.GetValueEx(_T("Install"), _T("Migrated"), &fMigrated);
		if (!fSuccess || !fMigrated) 
		{
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

namespace
{
	// not used at this time (reserved for the future use)
	void 
	pDeleteAutoRegistered(
		const DeviceSlotMap::value_type& entry)
	{
		DWORD SlotNo = entry.first;
		CNdasDevicePtr pDevice = entry.second;
		if (pDevice->IsAutoRegistered())
		{
			TCHAR szContainer[30];

			COMVERIFY( StringCchPrintf(
				szContainer, 
				30, 
				_T("Devices\\%04d"), 
				SlotNo) );

			BOOL fSuccess = _NdasSystemCfg.DeleteContainer(szContainer, TRUE);
		}
	}
}

// not used at this time (reserved for the future use)
void
CNdasDeviceRegistrar::OnServiceShutdown()
{
	InstanceAutoLock autolock(this);
	std::for_each(
		m_deviceSlotMap.begin(), 
		m_deviceSlotMap.end(), 
		pDeleteAutoRegistered);
}

// not used at this time (reserved for the future use)
void
CNdasDeviceRegistrar::OnServiceStop()
{
	InstanceAutoLock autolock(this);

	std::for_each(
		m_deviceSlotMap.begin(), 
		m_deviceSlotMap.end(), 
		pDeleteAutoRegistered);
}

DWORD
CNdasDeviceRegistrar::LookupEmptySlot()
{
	InstanceAutoLock autolock(this);

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

DWORD
CNdasDeviceRegistrar::Size()
{
	InstanceAutoLock autolock(this);
	return m_deviceSlotMap.size();
}

DWORD
CNdasDeviceRegistrar::MaxSlotNo()
{
	return m_dwMaxSlotNo;
}

void 
CNdasDeviceRegistrar::GetItems(CNdasDeviceVector& dest)
{
	dest.clear();
	dest.reserve(m_deviceSlotMap.size());
	std::transform(
		m_deviceSlotMap.begin(), m_deviceSlotMap.end(),
		std::back_inserter(dest),
		select2nd<DeviceSlotMap::value_type>());
}

void 
CNdasDeviceRegistrar::Cleanup()
{
	m_deviceSlotMap.clear();
	m_deviceIdMap.clear();
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
		BOOL fSuccess = 
			pCharToHex(psz[0], &v1) &&
			pCharToHex(psz[1], &v2);
		if (!fSuccess) 
		{
			return FALSE;
		}
		pDeviceId->Node[i] = static_cast<BYTE>(v1) * 0x10 + static_cast<BYTE>(v2);
	}
	return TRUE;
}

} // namespace

