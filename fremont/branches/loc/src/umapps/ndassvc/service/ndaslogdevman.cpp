/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <xtl/xtltrace.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastype.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasportctl.h>
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

CNdasLogicalUnitManager::CNdasLogicalUnitManager()
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

	NDAS_LOGICALUNIT_DEFINITION ludef;
	COMVERIFY(pNdasUnit->get_LogicalUnitDefinition(&ludef));

	DWORD luseq;
	COMVERIFY(pNdasUnit->get_LogicalUnitSequence(&luseq));

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

	LockInstance();

	NdasLogicalUnitDefinitionMap::iterator itr = m_LogicalUnitDefinitionMap.find(ludef);

	if (itr == m_LogicalUnitDefinitionMap.end()) 
	{
		CComBSTR registrySubPath;

		COMVERIFY(pAllocateRegistryPath(ludef, &registrySubPath));

		//
		// New Logical Device Instance
		//
		NDAS_LOGICALDEVICE_ID id;
		
		COMVERIFY(pAllocateNdasLogicalUnitId(ludef, registrySubPath, pNdasUnit, &id));

		CComObject<CNdasLogicalUnit>* pNdasLogicalUnitInstance;
		hr = CComObject<CNdasLogicalUnit>::CreateInstance(&pNdasLogicalUnitInstance);
		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVMANAGER, TRACE_LEVEL_ERROR, 
				"CNdasLogicalUnit::CreateInstance failed, hr=0x%X\n", hr);
			UnlockInstance();
			return hr;
		}

		hr = pNdasLogicalUnitInstance->Initialize(id, ludef, registrySubPath);
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

		NDAS_LOGICALUNIT_DEFINITION logicalUnitDefinition;
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
	NDAS_LOGICALUNIT_DEFINITION* config, 
	INdasLogicalUnit** ppNdasLogicalUnit)
{
	*ppNdasLogicalUnit = 0;

	CAutoLock autolock(this);

	NdasLogicalUnitDefinitionMap::const_iterator citr = m_LogicalUnitDefinitionMap.find(*config);
	if (m_LogicalUnitDefinitionMap.end() == citr) 
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

HRESULT
CNdasLogicalUnitManager::pAllocateRegistryPath(
	const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
	BSTR* RegistrySubPath)
{
	//
	// Registry Container
	// HKLM\Software\NDAS\LogDevices\XXXXXXXX
	//

	TCHAR subPath[30];
	BOOL success, fWriteData = TRUE;

	DWORD hashValue = ::crc32_calc(
		(const UCHAR*) &NdasLogicalUnitDefinition, 
		sizeof(NDAS_LOGICALUNIT_DEFINITION));

	while (TRUE) 
	{
		COMVERIFY(StringCchPrintf(
			subPath, RTL_NUMBER_OF(subPath), 
			_T("LogDevices\\%08X"), hashValue));

		NDAS_LOGICALUNIT_DEFINITION ludef;
		DWORD cbData = 0;
		success = _NdasSystemCfg.GetSecureValueEx(
			subPath,
			_T("Data"),
			&ludef,
			sizeof(ludef),
			&cbData);

		if (success && cbData == sizeof(ludef)) 
		{
			if (0 != ::memcmp(&ludef, &NdasLogicalUnitDefinition, sizeof(ludef))) 
			{
				// collision on hash value
				// increment the hash value and recalculate
				++hashValue;
				continue;
			} 
			else
			{
				// Existing entry.
				fWriteData = FALSE;
			}
		}

		break;
	}


	if (fWriteData) 
	{
		success = _NdasSystemCfg.SetSecureValueEx(
			subPath,
			_T("Data"),
			&NdasLogicalUnitDefinition,
			sizeof(NDAS_LOGICALUNIT_DEFINITION));

		if (!success) 
		{
			XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_ERROR,
				"Writing LDData failed, error=0x%X", GetLastError());
		}
	}

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"Hash Value=%08X\n", hashValue);

	XTLTRACE2(NDASSVC_NDASLOGDEVICE, TRACE_LEVEL_INFORMATION,
		"RegContainer: %ls\n", subPath);

	CComBSTR bstrSubPath(subPath);
	*RegistrySubPath = bstrSubPath.Detach();

	return S_OK;
}

HRESULT
CNdasLogicalUnitManager::pAllocateVStorId(
	__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
	__in BSTR RegistrySubPath,
	__out NDAS_LOGICALDEVICE_ID* NdasLogicalUnitId)
{
	*NdasLogicalUnitId = 0;

	if (IsNdasPortMode())
	{
		DWORD lun;

		BOOL success = _NdasSystemCfg.GetValueEx(
			RegistrySubPath,
			_T("LastLun"),
			&lun);

		if (!success)
		{
			success = _NdasSystemCfg.GetValueEx(
				_T("LogicalUnits"),
				_T("NextLun"),
				&lun);

			if (!success)
			{
				lun = 0;
			}
		}


		XTLVERIFY(_NdasSystemCfg.SetValueEx(
			_T("LogicalUnits"),
			_T("NextLun"),
			lun + 1));

		XTLVERIFY( _NdasSystemCfg.SetValueEx(
			RegistrySubPath,
			_T("LastLun"),
			lun));

		*NdasLogicalUnitId = lun;

		return S_OK;
	}
	else
	{
		DWORD SlotNo = 0;

		//
		// Find currently allocate slot number from registry
		//

		BOOL success = _NdasSystemCfg.GetValueEx(
			RegistrySubPath,
			_T("SlotNo"),
			&SlotNo);

		if (!success || 0 == SlotNo) 
		{
			success = _NdasSystemCfg.GetValueEx(
				_T("LogDevices"),
				_T("LastSlotNo"), 
				&SlotNo);

			if (!success) 
			{
				SlotNo = 10000;
			}
			else 
			{
				SlotNo++;
			}

			XTLVERIFY(_NdasSystemCfg.SetValueEx(
				_T("LogDevices"),
				_T("LastSlotNo"), 
				SlotNo));

			XTLVERIFY(_NdasSystemCfg.SetValueEx(
				RegistrySubPath,
				_T("SlotNo"),
				SlotNo));
		}

		*NdasLogicalUnitId = SlotNo;

		return S_OK;
	}
}

HRESULT
CNdasLogicalUnitManager::pAllocateNdasLogicalUnitId(
	__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
	__in BSTR RegistrySubPath,
	__in INdasUnit* pNdasUnit,
	__out NDAS_LOGICALDEVICE_ID* NdasLogicalUnitId)
{
#if 0
	for (DWORD i = 0; i <= MAX_SLOT_NO; ++i) 
	{
		if (!m_slotbitset.test(i)) 
		{
			m_slotbitset.set(i);
			return static_cast<NDAS_LOGICALDEVICE_ID>(i);
		}
	}
	return 0;
#endif

	NDAS_LOGICALDEVICE_ID ndasLogicalUnitId = 0;

	//
	// We have different policy for single and RAID
	//
	if (IsNdasPortMode())
	{
		NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress = {0};
		if (NdasIsRaidDiskType(NdasLogicalUnitDefinition.Type)) 
		{
			//
			// (PathId, TargetId, Lun) = (1, x, y)
			// where x = 1 - 31 and y = 1 - 127
			//
			// lun = y * 15 + x;
			//
			DWORD lun = 0;
			
			COMVERIFY(pAllocateVStorId(NdasLogicalUnitDefinition, RegistrySubPath, &lun));

			logicalUnitAddress.PathId = static_cast<UCHAR>(NdasLogicalUnitDefinition.Type);
			logicalUnitAddress.TargetId = static_cast<UCHAR>(lun / 127);
			logicalUnitAddress.Lun = static_cast<UCHAR>(lun % 127 + 1);
		}
		else
		{
			DWORD slotNo, unitNo;

			CComPtr<INdasDevice> pNdasDevice;
			COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
			COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));
			COMVERIFY(pNdasUnit->get_UnitNo(&unitNo));

			logicalUnitAddress.PathId = 0;
			logicalUnitAddress.TargetId = static_cast<UCHAR>(slotNo);
			logicalUnitAddress.Lun = static_cast<UCHAR>(unitNo);
		}

		ndasLogicalUnitId = NdasLogicalUnitAddressToLocation(logicalUnitAddress);
	}
	else
	{
		if (NdasIsRaidDiskType(NdasLogicalUnitDefinition.Type)) 
		{
			COMVERIFY(pAllocateVStorId(NdasLogicalUnitDefinition, RegistrySubPath, &ndasLogicalUnitId));
		}
		else
		{
			DWORD slotNo, unitNo;

			CComPtr<INdasDevice> pNdasDevice;
			COMVERIFY(pNdasUnit->get_ParentNdasDevice(&pNdasDevice));
			COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));
			COMVERIFY(pNdasUnit->get_UnitNo(&unitNo));

			ndasLogicalUnitId = slotNo * 10 + unitNo;
		}
	}

	XTLASSERT(ndasLogicalUnitId != 0);

	*NdasLogicalUnitId = ndasLogicalUnitId;

	return S_OK;
}

STDMETHODIMP
CNdasLogicalUnitManager::Cleanup()
{
	m_LogicalUnitIdMap.clear();
	return S_OK;
}
