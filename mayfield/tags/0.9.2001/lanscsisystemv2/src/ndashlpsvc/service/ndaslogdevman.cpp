/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "lsbusctl.h"

#include "ndaslogdevman.h"

#include "ndaslogdev.h"
#include "ndasdev.h"
#include "ndasdevreg.h"

#include "ndaserror.h"
#include "ndasinstman.h"
#include "ndaseventpub.h"

#include "ndastype_str.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASLOGDEVMAN
#include "xdebug.h"

CNdasLogicalDeviceManager::
CNdasLogicalDeviceManager(PCNdasDeviceRegistrar pRegistrar) :
	m_pRegistrar(pRegistrar),
	m_slotbitset(1) // first bit (slot 0) is always occupied and reserved.
{
}

CNdasLogicalDeviceManager::
~CNdasLogicalDeviceManager()
{
}

const PCNdasDeviceRegistrar 
CNdasLogicalDeviceManager::
GetNdasDeviceRegistrar()
{
	return m_pRegistrar;
}

PCNdasUnitDevice 
CNdasLogicalDeviceManager::
_FindUnitDevice(NDAS_UNITDEVICE_ID unitDeviceId)
{
	PCNdasDevice pDevice = m_pRegistrar->Find(unitDeviceId.DeviceId);
	if (NULL == pDevice) {
		// TODO: Device Not Found
		DPError(
			_FT("Registering Device Not Found from the Device Registrar - %s\n"),
			LPCTSTR(CNdasDeviceId(unitDeviceId.DeviceId)));
		return NULL;
	}

	PCNdasUnitDevice pUnitDevice = pDevice->GetUnitDevice(unitDeviceId.UnitNo);
	if (NULL == pUnitDevice) {
		// TODO: Unit Device Not Found
		DPError(
			_FT("Registering Unit Device Not Found from the Device Registrar - (%s,%d)\n"),
			LPCTSTR(CNdasDeviceId(unitDeviceId.DeviceId)),
			unitDeviceId.UnitNo);
		return NULL;
	}
	return pUnitDevice;
}

//
// Find the first available slot number
//
// Returns 0 if no slot is available
//
DWORD
CNdasLogicalDeviceManager::
_GetAvailSlot()
{
	for (DWORD i = 1; i < MAX_SLOT_NO; ++i) {
		if (!m_slotbitset.test(i)) {
			return i;
		}
	}
	return 0;
}

PCNdasLogicalDevice 
CNdasLogicalDeviceManager::
Register(
	NDAS_LOGICALDEVICE_TYPE logicalDeviceType,
	NDAS_UNITDEVICE_ID unitDeviceId,
	DWORD nSequence, 
	NDAS_UNITDEVICE_ID primaryUnitDeviceId,
	DWORD nUnitDevices)
{
	ximeta::CAutoLock autolock(this);

	DPInfo(_FT("Registering %s, %s at seq %d to primary %s (nUnitDevices=%d)\n"),
		NdasLogicalDeviceTypeString(logicalDeviceType),
		CNdasUnitDeviceId(unitDeviceId).ToString(),
		nSequence,
		CNdasUnitDeviceId(primaryUnitDeviceId).ToString(),
		nUnitDevices);
		
	_ASSERTE(nUnitDevices > 0);
	_ASSERTE(nUnitDevices <= CNdasLogicalDevice::MAX_UNITDEVICE_ENTRY);
	_ASSERTE(nSequence < nUnitDevices);
	_ASSERTE(
		(
		(primaryUnitDeviceId == unitDeviceId && nSequence == 0) ||
		(primaryUnitDeviceId != unitDeviceId && nSequence > 0 && nUnitDevices > 1)
		) && "Logical device entry criteria does not meet!");

	//
	// There should be no logical device with unit device ID.
	//
	PCNdasLogicalDevice pLogDevice = Find(primaryUnitDeviceId);

	//
	// If there is no primary logical device instance,
	// register a logical device first with primary unit device information.
	//
	// (New logical device instance has no unit device instances)
	//
	if (NULL == pLogDevice) {

		DWORD dwSlot = 0;
		do {

			dwSlot = _GetAvailSlot();
			if (0 == dwSlot) {
				// MAXIMUM SLOT REACHED
				DPWarning(_FT("No more available slot!\n"));
				::SetLastError(NDASHLPSVC_ERROR_LOGICALDEVICE_SLOT_FULL);
				return NULL;
			}

			//
			// Workaround for dangled logical device due to inconsistency
 			//
			BOOL bAdapterError = FALSE;
			BOOL fAlreadAlive = ::LsBusCtlQueryNodeAlive(dwSlot, &bAdapterError);
			if (fAlreadAlive) {
				DPWarning(_FT("Unmanaged logical device detected at slot %d\n"), dwSlot);
				m_slotbitset.set(dwSlot);
				continue;
			}

			break;

		} while (1);

		//
		// For non-existing logical device,
		// create an instance using primary unit device id
		// and add an entry of unit device id to that logical device
		//
		if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(logicalDeviceType)) {

			pLogDevice = new CNdasLogicalDisk(
				dwSlot,
				logicalDeviceType,
				primaryUnitDeviceId,
				nUnitDevices);

			_ASSERTE(NULL != pLogDevice && "Out of memory?");
			

		} else {
			_ASSERTE("Only Logical Disk Device is supported at the moment");
			::SetLastError(NDASHLPSVC_ERROR_UNSUPPORTED_LOGICALDEVICE_TYPE);
			return NULL;
		}

		//
		// Set slot usage
		//
		m_slotbitset.set(dwSlot);

		//
		// When making a new logical device instances, 
		// be sure to register it to maps
		//
		m_LDSlotToLDMap.insert(
			LDSlotToLogicalDeviceMap::value_type(dwSlot, pLogDevice));

		m_UnitDeviceIdToLDSlotMap.insert(
			UnitDeviceIdToLDSlotMap::value_type(primaryUnitDeviceId, dwSlot));

	} else {
		
		//
		// For an existing logical device, we need to check
		// if there are any type conflicts
		//
		if (pLogDevice->GetUnitDeviceCount() != nUnitDevices) {
			::SetLastError(NDASHLPSVC_ERROR_CONFLICT_LOGICALDEVICE_ENTRY);
			return NULL;
		}

		if (! IsNullNdasUnitDeviceId(pLogDevice->GetUnitDeviceId(nSequence))) {
			::SetLastError(NDASHLPSVC_ERROR_CONFLICT_LOGICALDEVICE_ENTRY);
			return NULL;
		}

		if (pLogDevice->GetType() != logicalDeviceType) {
			::SetLastError(NDASHLPSVC_ERROR_CONFLICT_LOGICALDEVICE_ENTRY);
			return NULL;
		}
	}

	//
	// No conflict. Add an entry
	//

	//
	// We need to add unit device ID to the logical device
	// if the unit device ID is different to primary unit device ID
	//
	DPInfo(_FT("Adding an unit device (%d,%s) instance to %s.\n"),
		nSequence,
		CNdasUnitDeviceId(unitDeviceId).ToString(),
		pLogDevice->ToString());

	BOOL fSuccess = pLogDevice->AddUnitDeviceId(nSequence, unitDeviceId);
	_ASSERTE(fSuccess);

	//
	// Add an entry to the map
	//
	if (unitDeviceId != primaryUnitDeviceId) {
		m_UnitDeviceIdToLDSlotMap.insert(
			UnitDeviceIdToLDSlotMap::value_type(
				unitDeviceId, pLogDevice->GetSlot()));
	}

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	(void) pEventPublisher->LogicalDeviceEntryChanged();

	return pLogDevice;
}

BOOL
CNdasLogicalDeviceManager::
Unregister(NDAS_DEVICE_ID deviceId, DWORD dwUnitNo)
{
	NDAS_UNITDEVICE_ID unitDeviceId = {deviceId, dwUnitNo};
	return Unregister(unitDeviceId);
}

BOOL
CNdasLogicalDeviceManager::
Unregister(NDAS_UNITDEVICE_ID unitDeviceId)
{
	ximeta::CAutoLock autolock(this);

	CNdasUnitDeviceId cUnitDeviceId(unitDeviceId);

	DPInfo(_FT("Unregistering %s.\n"),
		cUnitDeviceId.ToString());

	UnitDeviceIdToLDSlotMap::iterator itrSlot = 
		m_UnitDeviceIdToLDSlotMap.find(unitDeviceId);

	if (m_UnitDeviceIdToLDSlotMap.end() == itrSlot) {
		DPError(_FT("Unit Device (%s) not found.\n"),
			cUnitDeviceId.ToString());
		return FALSE;
	}

	LDSlotToLogicalDeviceMap::iterator itrLogDev =
		m_LDSlotToLDMap.find(itrSlot->second);

	_ASSERTE(itrLogDev != m_LDSlotToLDMap.end() && "Invalid map entry!");

	PCNdasLogicalDevice pLogDevice = itrLogDev->second;

	DPInfo(_FT("Found logical device: %s.\n"), pLogDevice->ToString());

	BOOL fSuccess = pLogDevice->RemoveUnitDeviceId(unitDeviceId);
	_ASSERTE(fSuccess && "Slot map mismatch?");
	
	DPInfo(_FT("Removed unit device %s, logical device is now %s.\n"),
		cUnitDeviceId.ToString(),
		pLogDevice->ToString());

	if (0 == pLogDevice->GetUnitDeviceInstanceCount()) {
		
		DPInfo(_FT("Deleting logical device instance of %s.\n"),
			pLogDevice->ToString());

		m_LDSlotToLDMap.erase(itrSlot->second);
		delete pLogDevice;
	}

	m_UnitDeviceIdToLDSlotMap.erase(unitDeviceId);

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	(void) pEventPublisher->LogicalDeviceEntryChanged();

	return TRUE;
}

PCNdasLogicalDevice
CNdasLogicalDeviceManager::
Find(NDAS_DEVICE_ID deviceId, DWORD dwUnitNo)
{
	NDAS_UNITDEVICE_ID unitDeviceId = { deviceId, dwUnitNo };
	return Find(unitDeviceId);
}

PCNdasLogicalDevice
CNdasLogicalDeviceManager::
Find(NDAS_UNITDEVICE_ID unitDeviceId)
{
	ximeta::CAutoLock autolock(this);

	UnitDeviceIdToLDSlotMap::const_iterator itr =
		m_UnitDeviceIdToLDSlotMap.find(unitDeviceId);
	
	if (m_UnitDeviceIdToLDSlotMap.end() == itr) {
		return NULL;
	}
	
	return Find(itr->second);
}

PCNdasLogicalDevice
CNdasLogicalDeviceManager::
Find(DWORD dwSlot)
{
	ximeta::CAutoLock autolock(this);

	LDSlotToLogicalDeviceMap::const_iterator itr = m_LDSlotToLDMap.find(dwSlot);

	if (m_LDSlotToLDMap.end() == itr) {
		return NULL;
	}
	return itr->second;
}

DWORD
CNdasLogicalDeviceManager::
Size()
{
	ximeta::CAutoLock autolock(this);

	return m_LDSlotToLDMap.size();
}

CNdasLogicalDeviceManager::ConstIterator
CNdasLogicalDeviceManager::
begin()
{
	return m_LDSlotToLDMap.begin();
}

CNdasLogicalDeviceManager::ConstIterator
CNdasLogicalDeviceManager::
end()
{
	return m_LDSlotToLDMap.end();
}

