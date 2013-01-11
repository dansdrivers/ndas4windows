/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <xtl/xtltrace.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndastype_str.h>
#include "ndasdevid.h"
#include "ndaslogdevman.h"
#include "ndaslogdev.h"
#include "ndasdevreg.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndascfg.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaslogdevman.tmh"
#endif

CNdasLogicalUnitManager::CNdasLogicalUnitManager() :
	m_slotbitset(1) // first bit (slot 0) is always occupied and reserved.
{
}

CNdasLogicalUnitManager::~CNdasLogicalUnitManager()
{
}

//
// Find the first available slot number
//
// Returns 0 if no slot is available
//

HRESULT
CNdasLogicalUnitManager::Register(INdasUnit* pNdasUnit, INdasLogicalUnit** ppNdasLogicalUnit)
{
	HRESULT hr;

	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"Registering NdasUnit=%p to LDM\n", pNdasUnit);

	*ppNdasLogicalUnit = NULL;

	NDAS_LOGICALDEVICE_GROUP ludef;
	COMVERIFY(pNdasUnit->get_LogicalUnitDefinition(&ludef));

	DWORD luseq;
	COMVERIFY(pNdasUnit->get_LogicalUnitSequence(&luseq));

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

	LockInstance();

	LogicalDeviceGroupMap::iterator itr = m_LogicalUnitDefinitionMap.find(ludef);

	if (itr == m_LogicalUnitDefinitionMap.end()) 
	{
		//
		// New Logical Device Instance
		//
		NDAS_LOGICALDEVICE_ID id = pAllocateID();
		if (0 == id) 
		{
			UnlockInstance();
			// SLOT FULL
			hr = NDASSVC_ERROR_LOGICALDEVICE_SLOT_FULL;
			return hr;
		}

		CComObject<CNdasLogicalUnit>* pNdasLogicalUnitInstance;
		hr = CComObject<CNdasLogicalUnit>::CreateInstance(&pNdasLogicalUnitInstance);
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_ERROR, 
				"CNdasLogicalUnit::CreateInstance failed, hr=0x%X\n", hr);
			UnlockInstance();
			return hr;
		}

		hr = pNdasLogicalUnitInstance->Initialize(id, ludef);
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_ERROR, 
				"CNdasLogicalUnit::Initialize failed, hr=0x%X\n", hr);
			UnlockInstance();
			return hr;
		}

		pNdasLogicalUnit = pNdasLogicalUnitInstance;

		m_NdasLogicalUnits.Add(pNdasLogicalUnit);
		m_LogicalUnitDefinitionMap.insert(std::make_pair(ludef, pNdasLogicalUnit));
		m_LogicalUnitIdMap.insert(std::make_pair(id, pNdasLogicalUnit));

		UnlockInstance();

		CComPtr<INdasDevice> pNdasDevice;
		COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
		XTLASSERT(pNdasDevice);

		DWORD regFlags;
		COMVERIFY(pNdasDevice->get_RegisterFlags(&regFlags));

		if (regFlags & NDAS_DEVICE_REG_FLAG_AUTO_REGISTERED) 
		{
			BOOL fMountOnReady = TRUE;
			if (NdasServiceConfig::Get(nscMountOnReadyForEncryptedOnly))
			{
				NDAS_UNITDEVICE_TYPE unitType;
				COMVERIFY(pNdasUnit->get_Type(&unitType));

				if (0 == luseq && NDAS_UNITDEVICE_TYPE_DISK == unitType)
				{
					CComQIPtr<INdasDiskUnit> pNdasDiskUnit(pNdasUnit);
					ATLASSERT(pNdasDiskUnit.p);

					NDAS_CONTENT_ENCRYPT encryption;
					COMVERIFY(pNdasDiskUnit->get_ContentEncryption(&encryption));

					if (NDAS_CONTENT_ENCRYPT_METHOD_NONE != encryption.Method)
					{
						fMountOnReady = TRUE;
					}
					else
					{
						fMountOnReady = FALSE;
					}
				}
			}

			// auto registered devices always ignore RiskyMountFlag
			pNdasLogicalUnit->SetRiskyMountFlag(FALSE);

			if (fMountOnReady)
			{
				ACCESS_MASK granted;
				pNdasDevice->get_GrantedAccess(&granted);
				pNdasLogicalUnit->SetMountOnReady(granted, FALSE);
			}
		}

	}
	else 
	{
		pNdasLogicalUnit = itr->second;
		UnlockInstance();
	}

	COMVERIFY(pNdasLogicalUnit->AddNdasUnitInstance(pNdasUnit));

	*ppNdasLogicalUnit = pNdasLogicalUnit.Detach();

	return S_OK;
}

HRESULT
CNdasLogicalUnitManager::Unregister(INdasUnit* pNdasUnit)
{
	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"Unregistering NdasUnit=%p from LDM\n", pNdasUnit);

	if (NULL == pNdasUnit)
	{
		return E_POINTER;
	}

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
	COMVERIFY(pNdasUnit->get_NdasLogicalUnit(&pNdasLogicalUnit));

	DWORD luseq;
	COMVERIFY(pNdasUnit->get_LogicalUnitSequence(&luseq));

	COMVERIFY(pNdasLogicalUnit->RemoveNdasUnitInstance(pNdasUnit));

	DWORD instanceCount;
	COMVERIFY(pNdasLogicalUnit->get_NdasUnitInstanceCount(&instanceCount));

	if (0 == instanceCount) 
	{
		NDAS_LOGICALDEVICE_ID logicalUnitId;
		COMVERIFY(pNdasLogicalUnit->get_Id(&logicalUnitId));

		NDAS_LOGICALDEVICE_GROUP logicalUnitDefinition;
		COMVERIFY(pNdasLogicalUnit->get_LogicalUnitDefinition(&logicalUnitDefinition));

		LockInstance();
		XTLVERIFY(1 == m_LogicalUnitDefinitionMap.erase(logicalUnitDefinition));
		XTLVERIFY(1 == m_LogicalUnitIdMap.erase(logicalUnitId));
		size_t count = m_NdasLogicalUnits.GetCount();
		for (size_t i = 0; i < count; ++i)
		{
			INdasLogicalUnit* p = m_NdasLogicalUnits.GetAt(i);
			if (p == pNdasLogicalUnit)
			{
				m_NdasLogicalUnits.RemoveAt(i);
				break;
			}
		}
		XTLVERIFY(pDeallocateID(logicalUnitId));
		UnlockInstance();
	}

	return S_OK;
}

STDMETHODIMP
CNdasLogicalUnitManager::get_NdasLogicalUnit(
	NDAS_LOGICALDEVICE_ID id, 
	INdasLogicalUnit** ppNdasLogicalUnit)
{
	*ppNdasLogicalUnit = 0;

	CAutoLock autolock(this);

	LogicalDeviceIdMap::const_iterator citr = m_LogicalUnitIdMap.find(id);
	if (m_LogicalUnitIdMap.end() == citr) 
	{
		return E_FAIL;
	}
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit = citr->second;
	*ppNdasLogicalUnit = pNdasLogicalUnit.Detach();
	return S_OK;
}

STDMETHODIMP
CNdasLogicalUnitManager::get_NdasLogicalUnit(
	NDAS_LOGICALDEVICE_GROUP* config, 
	INdasLogicalUnit** ppNdasLogicalUnit)
{
	*ppNdasLogicalUnit = 0;

	CAutoLock autolock(this);

	LogicalDeviceGroupMap::const_iterator citr = m_LogicalUnitDefinitionMap.find(*config);
	if (m_LogicalUnitDefinitionMap.end() == citr) 
	{
		return E_FAIL;
	}
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit = citr->second;
	*ppNdasLogicalUnit = pNdasLogicalUnit.Detach();
	return S_OK;
}

STDMETHODIMP
CNdasLogicalUnitManager::get_NdasLogicalUnitByNdasLocation(
	NDAS_LOCATION location, 
	INdasLogicalUnit** ppNdasLogicalUnit)
{
	*ppNdasLogicalUnit = 0;

	CAutoLock autolock(this);

	LocationMap::const_iterator citr = m_NdasLocationMap.find(location);
	if (m_NdasLocationMap.end() == citr) 
	{
		return E_FAIL;
	}
	CComPtr<INdasLogicalUnit> pNdasLogicalUnit = citr->second;
	*ppNdasLogicalUnit = pNdasLogicalUnit.Detach();
	return S_OK;
}

struct NdasLogicalUnitFireShutdownEvent : 
	public std::unary_function<INdasLogicalUnit*,void>
{
	void operator()(INdasLogicalUnit* pNdasLogicalUnit) const
	{
		CComQIPtr<INdasLogicalUnitPnpSink>
			pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
		ATLASSERT(pNdasLogicalUnitPnpSink.p);
		pNdasLogicalUnitPnpSink->OnSystemShutdown();
	}
};

STDMETHODIMP
CNdasLogicalUnitManager::OnSystemShutdown()
{
	//
	// Clear the risky mount flag on shutdown
	// if the shutdown is initiated before the
	// monitor will clear the flag
	//
	CAutoLock autolock(this);
	CInterfaceArray<INdasLogicalUnit> ndasLogicalUnits;
	get_NdasLogicalUnits(NDAS_ENUM_DEFAULT, ndasLogicalUnits);
	autolock.Release();

	// Call shutdown
	AtlForEach(ndasLogicalUnits, NdasLogicalUnitFireShutdownEvent());

	return S_OK;
}

struct NdasLogicalUnitIsHidden : public std::unary_function<INdasLogicalUnit*, bool>
{
	result_type operator()(argument_type pNdasLogicalUnit) const
	{
		BOOL hidden;
		if (SUCCEEDED(pNdasLogicalUnit->get_IsHidden(&hidden)) && hidden)
		{
			return true;
		}
		return false;
	}
};

STDMETHODIMP
CNdasLogicalUnitManager::get_NdasLogicalUnits(
	DWORD Flags, CInterfaceArray<INdasLogicalUnit>& dest)
{
	CAutoLock autolock(this);
	dest.Copy(m_NdasLogicalUnits);
	autolock.Release();

	if (NDAS_ENUM_EXCLUDE_HIDDEN & Flags)
	{
		//
		// Filter out hidden logical devices
		// A hidden logical device is a logical device, 
		// of which the device of the primary unit device
		// is hidden.
		//
		size_t count = dest.GetCount();
		for (size_t index = 0; index < count; ++index)
		{
			INdasLogicalUnit* p = dest.GetAt(index);
			if (NdasLogicalUnitIsHidden()(p))
			{
				dest.RemoveAt(index);
				--index; --count;
			}
		}
	}

	return S_OK;
}

NDAS_LOGICALDEVICE_ID 
CNdasLogicalUnitManager::pAllocateID()
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
CNdasLogicalUnitManager::pDeallocateID(NDAS_LOGICALDEVICE_ID id)
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

STDMETHODIMP
CNdasLogicalUnitManager::Cleanup()
{
	m_LogicalUnitIdMap.clear();
	return S_OK;
}

STDMETHODIMP
CNdasLogicalUnitManager::RegisterNdasLocation(
	NDAS_LOCATION location, 
	INdasLogicalUnit* pNdasLogicalUnit)
{
	CAutoLock autolock(this);

	NDAS_LOGICALDEVICE_ID logicalUnitId;
	COMVERIFY(pNdasLogicalUnit->get_Id(&logicalUnitId));

	XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_INFORMATION, 
		"RegisterNdasLocation, slot=%d, logDevice=%d\n",
		location,
		logicalUnitId);

	bool result = m_NdasLocationMap.insert(
		std::make_pair(location,pNdasLogicalUnit)).second;
	//
	// result can be false if RAID information is conflicting status 
	// i.e. First RAID member is member of multiple RAID set.
	//
//	XTLASSERT(result);

	return result ? S_OK : E_FAIL;
}

STDMETHODIMP
CNdasLogicalUnitManager::UnregisterNdasLocation(
	NDAS_LOCATION location,
	INdasLogicalUnit* pNdasLogicalUnit)
{
	CAutoLock autolock(this);

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
		return E_FAIL;
	}

	XTLASSERT(itr->second == pNdasLogicalUnit);

	m_NdasLocationMap.erase(itr);

	return S_OK;
}
