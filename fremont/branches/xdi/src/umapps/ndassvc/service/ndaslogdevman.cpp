/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <xtl/xtltrace.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndastype_str.h>
#include "ndaslogdevman.h"
#include "ndaslogdev.h"
#include "ndasdev.h"
#include "ndasdevreg.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndascfg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaslogdevman.tmh"
#endif

CNdasLogicalDeviceManager::
CNdasLogicalDeviceManager(CNdasService& service) :
	m_service(service),
	m_slotbitset(1) // first bit (slot 0) is always occupied and reserved.
{
}

CNdasLogicalDeviceManager::
~CNdasLogicalDeviceManager()
{
}

//
// Find the first available slot number
//
// Returns 0 if no slot is available
//

CNdasLogicalDevicePtr
CNdasLogicalDeviceManager::
Register(CNdasUnitDevicePtr pUnitDevice)
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"Registering %s to LDM\n", pUnitDevice->ToStringA());

	const NDAS_LOGICALDEVICE_GROUP& ldGroup = pUnitDevice->GetLDGroup();
	DWORD ldSequence = pUnitDevice->GetLDSequence();

	CNdasLogicalDevicePtr pLogDevice;
	LogicalDeviceGroupMap::iterator itr = m_LDGroupMap.find(ldGroup);

	if (itr == m_LDGroupMap.end()) 
	{
		//
		// New Logical Device Instance
		//
		NDAS_LOGICALDEVICE_ID id = _AllocateID();
		if (0 == id) 
		{
			// SLOT FULL
			::SetLastError(NDASSVC_ERROR_LOGICALDEVICE_SLOT_FULL);
			return CNdasLogicalDeviceNullPtr;
		}

		pLogDevice = CNdasLogicalDevicePtr(new CNdasLogicalDevice(id, ldGroup));
		if (!pLogDevice)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return CNdasLogicalDeviceNullPtr;
		}

		BOOL fSuccess = pLogDevice->Initialize();
		if (!fSuccess) 
		{
			return CNdasLogicalDeviceNullPtr;
		}

		// Delegate NDAS port existence value from NDAS service to the NDAS
		// logical device.
		pLogDevice->SetNdasPortExistence(m_service.NdasPortExists());

		m_LDGroupMap.insert(std::make_pair(ldGroup, pLogDevice));
		m_LDIDMap.insert(std::make_pair(id, pLogDevice));
		//LogicalDeviceGroupMap::value_type(ldGroup, pLogDevice));
		// LogicalDeviceIdMap::value_type(id, pLogDevice));

		CNdasDevicePtr pDevice = pUnitDevice->GetParentDevice();
		XTLASSERT(CNdasDeviceNullPtr != pDevice);
		if (pDevice->IsAutoRegistered()) 
		{
			BOOL fMountOnReady = TRUE;
			if (NdasServiceConfig::Get(nscMountOnReadyForEncryptedOnly))
			{
				if (0 == ldSequence &&
					NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType())
				{
					CNdasUnitDiskDevice& unitDiskDevice = 
						reinterpret_cast<CNdasUnitDiskDevice&>(*pUnitDevice.get());
					const NDAS_CONTENT_ENCRYPT& ucenc = unitDiskDevice.GetEncryption();
					if (NDAS_CONTENT_ENCRYPT_METHOD_NONE != ucenc.Method)
					{
						fMountOnReady = TRUE;
					}
					else
					{
						fMountOnReady = FALSE;
					}
				}
			}

			if (fMountOnReady)
			{
				pLogDevice->SetMountOnReady(pDevice->GetGrantedAccess());
			}

			// auto registered devices always ignore RiskyMountFlag
			pLogDevice->SetRiskyMountFlag(FALSE);
		}

	}
	else 
	{
		pLogDevice = itr->second;
	}

	XTLENSURE_RETURN_T(pLogDevice->AddUnitDevice(pUnitDevice), CNdasLogicalDeviceNullPtr);

	return pLogDevice;
}

bool
CNdasLogicalDeviceManager::Unregister(CNdasUnitDevicePtr pUnitDevice)
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"Unregistering %s from LDM\n", pUnitDevice->ToStringA());

	XTLENSURE_RETURN_T(CNdasUnitDeviceNullPtr != pUnitDevice, false);

	CNdasLogicalDevicePtr pLogDevice = pUnitDevice->GetLogicalDevice();
	XTLENSURE_RETURN_T(CNdasLogicalDeviceNullPtr != pLogDevice, false);

	DWORD ldSequence = pUnitDevice->GetLDSequence();
	pLogDevice->RemoveUnitDevice(pUnitDevice);

	if (pLogDevice->GetUnitDeviceInstanceCount() == 0) 
	{
		NDAS_LOGICALDEVICE_ID id = pLogDevice->GetLogicalDeviceId();
		XTLVERIFY(1 == m_LDGroupMap.erase(pLogDevice->GetLDGroup()));
		XTLVERIFY(1 == m_LDIDMap.erase(id));
		XTLVERIFY(_DeallocateID(id));
	}

	return true;
}

CNdasLogicalDevicePtr
CNdasLogicalDeviceManager::Find(const NDAS_LOGICALDEVICE_GROUP& ldGroup)
{
	InstanceAutoLock autolock(this);
	LogicalDeviceGroupMap::const_iterator citr = m_LDGroupMap.find(ldGroup);
	if (m_LDGroupMap.end() == citr) 
	{
		return CNdasLogicalDeviceNullPtr;
	}
	CNdasLogicalDevicePtr pLogicalDevice(citr->second);
	return pLogicalDevice;
}

CNdasLogicalDevicePtr
CNdasLogicalDeviceManager::Find(NDAS_LOGICALDEVICE_ID ldid)
{
	InstanceAutoLock autolock(this);
	LogicalDeviceIdMap::const_iterator citr = m_LDIDMap.find(ldid);
	if (m_LDIDMap.end() == citr) 
	{
		return CNdasLogicalDeviceNullPtr;
	}
	CNdasLogicalDevicePtr pLogicalDevice(citr->second);
	return pLogicalDevice;
}

CNdasLogicalDevicePtr
CNdasLogicalDeviceManager::FindByNdasLocation(NDAS_LOCATION location)
{
	InstanceAutoLock autolock(this);
	LocationMap::const_iterator citr = m_NdasLocationMap.find(location);
	if (m_NdasLocationMap.end() == citr) 
	{
		return CNdasLogicalDeviceNullPtr;
	}
	CNdasLogicalDevicePtr pLogicalDevice(citr->second);
	return pLogicalDevice;
}

void
CNdasLogicalDeviceManager::OnShutdown()
{
	//
	// Clear the risky mount flag on shutdown
	// if the shutdown is initiated before the
	// monitor will clear the flag
	//
	InstanceAutoLock autolock(this);

	CNdasLogicalDeviceVector logDevices(m_LDIDMap.size());

	std::transform(
		m_LDIDMap.begin(), m_LDIDMap.end(),
		logDevices.begin(),
		select2nd<LogicalDeviceIdMap::value_type>());

	// Call shutdown
	std::for_each(
		logDevices.begin(), logDevices.end(),
		boost::mem_fn(&CNdasLogicalDevice::OnShutdown));
}

DWORD
CNdasLogicalDeviceManager::Size()
{
	InstanceAutoLock autolock(this);
	return m_LDIDMap.size();
}

void 
CNdasLogicalDeviceManager::GetItems(CNdasLogicalDeviceVector& dest)
{
	dest.resize(m_LDIDMap.size());
	std::transform(
		m_LDIDMap.begin(), m_LDIDMap.end(),
		dest.begin(),
		select2nd<LogicalDeviceIdMap::value_type>());
}

NDAS_LOGICALDEVICE_ID 
CNdasLogicalDeviceManager::_AllocateID()
{
	for (DWORD i = 0; i <= MAX_SLOT_NO; ++i) 
	{
		if (!m_slotbitset.test(i)) 
		{
			m_slotbitset.set(i);
			return static_cast<NDAS_LOGICALDEVICE_ID>(i);
		}
	}

	return 0;
}

bool
CNdasLogicalDeviceManager::_DeallocateID(NDAS_LOGICALDEVICE_ID id)
{
	DWORD n = static_cast<DWORD>(id);

	if (n < 1 || n > MAX_SLOT_NO) 
	{
		return false;
	}

	if (m_slotbitset.test(n)) 
	{
		m_slotbitset.set(n, false);
		return true;
	}

	return false;
}

void
CNdasLogicalDeviceManager::Cleanup()
{
	m_LDIDMap.clear();
}

BOOL
CNdasLogicalDeviceManager::
RegisterNdasLocation(
	const NDAS_LOCATION& location, 
	CNdasLogicalDevicePtr pLogDevice)
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"RegisterNdasLocation, slot=%d, logDevice=%s\n",
		location,
		pLogDevice->ToStringA());

	bool result = m_NdasLocationMap.insert(
		std::make_pair(location,pLogDevice)).second;
	//
	// result can be false if RAID information is conflicting status 
	// i.e. First RAID member is member of multiple RAID set.
	//
//	XTLASSERT(result);

	return result;
}

BOOL
CNdasLogicalDeviceManager::
UnregisterNdasLocation(
	const NDAS_LOCATION& location,
	CNdasLogicalDevicePtr pLogDevice)
{
	InstanceAutoLock autolock(this);

	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"Unregistering NdasLocation, slot=%d\n",
		location);

	LocationMap::iterator itr = m_NdasLocationMap.find(location);
	XTLASSERT(m_NdasLocationMap.end() != itr);
	if (m_NdasLocationMap.end() == itr)
	{
		XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_ERROR, 
			"NdasLocation not registered, slot=%d", 
			location);
		return FALSE;
	}

	XTLASSERT(itr->second == pLogDevice);

	m_NdasLocationMap.erase(itr);

	return TRUE;
}
