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
#include "ndas/ndasmsg.h"
#include "ndasinstman.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndas/ndastype_str.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASLOGDEVMAN
#include "xdebug.h"

CNdasLogicalDeviceManager::CNdasLogicalDeviceManager() :
	m_slotbitset(1) // first bit (slot 0) is always occupied and reserved.
{
	DBGPRT_TRACE(_FT("ctor\n"));
}

CNdasLogicalDeviceManager::~CNdasLogicalDeviceManager()
{
	DBGPRT_TRACE(_FT("dtor\n"));
}

//
// Find the first available slot number
//
// Returns 0 if no slot is available
//

CNdasLogicalDevice* 
CNdasLogicalDeviceManager::Register(CNdasUnitDevice& unitDevice)
{
	ximeta::CAutoLock autolock(this);

	CONST NDAS_LOGICALDEVICE_GROUP& ldGroup = unitDevice.GetLDGroup();
	DWORD ldSequence = unitDevice.GetLDSequence();

	CNdasLogicalDevice* pLogDevice = NULL;
	LDGroupMap::iterator itr = m_LDGroupMap.find(ldGroup);

	if (itr == m_LDGroupMap.end()) {
		//
		// New Logical Device Instance
		//
		NDAS_LOGICALDEVICE_ID id = cpAllocateID();
		if (0 == id) {
			// SLOT FULL
			::SetLastError(NDASSVC_ERROR_LOGICALDEVICE_SLOT_FULL);
			return NULL;
		}

		pLogDevice = new CNdasLogicalDevice(id, ldGroup);
		if (NULL == pLogDevice) {
			::SetLastError(ERROR_OUTOFMEMORY);
			return NULL;
		}
		BOOL fSuccess = pLogDevice->Initialize();
		if (!fSuccess) {
			delete pLogDevice;
			return NULL;
		}
		pLogDevice->AddRef(); // internal pointer reference
		m_LDGroupMap.insert(LDGroupMap::value_type(ldGroup, pLogDevice));
		m_LDIDMap.insert(LDIDMap::value_type(id, pLogDevice));

		CRefObjPtr<CNdasDevice> pDevice = unitDevice.GetParentDevice();
		_ASSERTE(NULL != pDevice.p);
		if (pDevice->IsAutoRegistered()) {
			pLogDevice->SetMountOnReady(pDevice->GetGrantedAccess());
			// auto registered devices always ignore RiskyMountFlag
			pLogDevice->SetRiskyMountFlag(FALSE);
		}

	} else {
		pLogDevice = itr->second;
	}

	BOOL fSuccess = pLogDevice->AddUnitDevice(unitDevice);
	_ASSERTE(fSuccess);
	if (!fSuccess) {
		return NULL;
	}

	if (NULL != pLogDevice) pLogDevice->AddRef(); // external pointer reference
	return pLogDevice;
}

BOOL
CNdasLogicalDeviceManager::Unregister(CNdasUnitDevice& unitDevice)
{
	ximeta::CAutoLock autolock(this);

	CRefObjPtr<CNdasLogicalDevice> pLogDevice = unitDevice.GetLogicalDevice();
	if (NULL == pLogDevice.p) {
		return FALSE;
	}

	DWORD ldSequence = unitDevice.GetLDSequence();
	pLogDevice->RemoveUnitDevice(unitDevice);

	if (pLogDevice->GetUnitDeviceInstanceCount() == 0) {

		NDAS_LOGICALDEVICE_ID id = pLogDevice->GetLogicalDeviceId();

		LDGroupMap::size_type ldg_erased = 
			m_LDGroupMap.erase(pLogDevice->GetLDGroup());
		_ASSERTE(1 == ldg_erased);

		LDIDMap::size_type id_erased = 
			m_LDIDMap.erase(id);
		_ASSERTE(1 == id_erased);
		
		pLogDevice->Release();

		BOOL fSuccess = cpDeallocateID(id);
		_ASSERTE(fSuccess);
	}

	return TRUE;
}

CNdasLogicalDevice*
CNdasLogicalDeviceManager::Find(CONST NDAS_LOGICALDEVICE_GROUP& ldGroup)
{
	ximeta::CAutoLock autolock(this);

	LDGroupMap::const_iterator citr = m_LDGroupMap.find(ldGroup);
	if (m_LDGroupMap.end() == citr) {
		return NULL;
	}
	CNdasLogicalDevice* pLogicalDevice = citr->second;
	if (NULL != pLogicalDevice) pLogicalDevice->AddRef();
	return pLogicalDevice;
}

CNdasLogicalDevice*
CNdasLogicalDeviceManager::Find(NDAS_LOGICALDEVICE_ID ldid)
{
	ximeta::CAutoLock autolock(this);

	LDIDMap::const_iterator citr = m_LDIDMap.find(ldid);
	if (m_LDIDMap.end() == citr) {
		return NULL;
	}
	CNdasLogicalDevice* pLogicalDevice = citr->second;
	if (NULL != pLogicalDevice) pLogicalDevice->AddRef();
	return pLogicalDevice;
}

CNdasLogicalDevice*
CNdasLogicalDeviceManager::Find(CONST NDAS_SCSI_LOCATION& location)
{
	ximeta::CAutoLock autolock(this);

	LocationMap::const_iterator citr = m_NdasScsiLocationMap.find(location);
	if (m_NdasScsiLocationMap.end() == citr) {
		return NULL;
	}
	CNdasLogicalDevice* pLogicalDevice = citr->second;
	if (NULL != pLogicalDevice) pLogicalDevice->AddRef();
	return pLogicalDevice;
}

void
CNdasLogicalDeviceManager::ShutdownOf(
	const CNdasLogicalDeviceManager::LDIDMap::value_type& pair)
{
	CNdasLogicalDevice* pLogicalDevice = pair.second;
	pLogicalDevice->OnShutdown();
}

VOID
CNdasLogicalDeviceManager::OnShutdown()
{
	//
	// Clear the risky mount flag on shutdown
	// if the shutdown is initiated before the
	// monitor will clear the flag
	//
	ximeta::CAutoLock autolock(this);

	std::for_each(
		m_LDIDMap.begin(),
		m_LDIDMap.end(),
		ShutdownOf);
}

DWORD
CNdasLogicalDeviceManager::Size()
{
	ximeta::CAutoLock autolock(this);
	return m_LDIDMap.size();
}

/*
CNdasLogicalDeviceManager::ConstIterator
CNdasLogicalDeviceManager::begin()
{
	return m_LDIDMap.begin();
}

CNdasLogicalDeviceManager::ConstIterator
CNdasLogicalDeviceManager::end()
{
	return m_LDIDMap.end();
}
*/

void 
CNdasLogicalDeviceManager::GetItems(CNdasLogicalDeviceCollection& coll)
{
	typedef push_map_value_to<
		LDIDMap::value_type, 
		CNdasLogicalDeviceCollection
	> copy_functor;

	std::for_each(
		m_LDIDMap.begin(), 
		m_LDIDMap.end(), 
		copy_functor(coll));
}

NDAS_LOGICALDEVICE_ID 
CNdasLogicalDeviceManager::cpAllocateID()
{
	for (DWORD i = 0; i <= MAX_SLOT_NO; ++i) {
		if (!m_slotbitset.test(i)) {
			m_slotbitset.set(i);
			return static_cast<NDAS_LOGICALDEVICE_ID>(i);
		}
	}

	return 0;
}

BOOL 
CNdasLogicalDeviceManager::cpDeallocateID(NDAS_LOGICALDEVICE_ID id)
{
	DWORD n = static_cast<DWORD>(id);

	if (n < 1 || n > MAX_SLOT_NO) {
		return FALSE;
	}

	if (m_slotbitset.test(n)) {
		m_slotbitset.set(n, false);
		return TRUE;
	}

	return FALSE;
}

BOOL
CNdasLogicalDeviceManager::RegisterNdasScsiLocation(
	CONST NDAS_SCSI_LOCATION& location, 
	CNdasLogicalDevice& logicalDevice)
{
	ximeta::CAutoLock autolock(this);

	std::pair<LocationMap::iterator,bool> 
		insertResult = m_NdasScsiLocationMap.insert(
			LocationMap::value_type(location,&logicalDevice));
	_ASSERTE(insertResult.second);

	return insertResult.second;
}

BOOL
CNdasLogicalDeviceManager::UnregisterNdasScsiLocation(
	CONST NDAS_SCSI_LOCATION& location)
{
	ximeta::CAutoLock autolock(this);

	LocationMap::size_type nErased = m_NdasScsiLocationMap.erase(location);
	if (1 != nErased) {
		DBGPRT_WARN(
			_FT("NdasScsiLocation not registered at %s"), 
			CNdasScsiLocation(location).ToString());
	}
	_ASSERTE(1 == nErased);

	return (1 == nErased);
}
