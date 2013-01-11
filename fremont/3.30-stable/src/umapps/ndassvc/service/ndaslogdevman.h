/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEVMAN_H_
#define _NDASLOGDEVMAN_H_

#include "ndas/ndastypeex.h"
#include "ndassvcdef.h"
#include "ndascomobjectsimpl.hpp"

// forward declaration
class CNdasLogicalUnitManager;
class CNdasService;

typedef std::map<NDAS_LOGICALUNIT_DEFINITION,INdasLogicalUnit*> NdasLogicalUnitDefinitionMap;
typedef std::map<NDAS_LOGICALDEVICE_ID,INdasLogicalUnit*> LogicalDeviceIdMap;

class CNdasLogicalUnitManager :
	public CComObjectRootEx<CComMultiThreadModel>,
	public INdasLogicalUnitManagerInternal,
	private ILockImpl<INativeLock>
{
public:

	BEGIN_COM_MAP(CNdasLogicalUnitManager)
		COM_INTERFACE_ENTRY(INdasLogicalUnitManagerInternal)
	END_COM_MAP()

	CNdasLogicalUnitManager();
	~CNdasLogicalUnitManager();

	//
	// Registers an unit device to create an instance of the logical device
	// or to add to an existing instance of the logical device
	// Logical device can be instantiated (plugged in) when
	// all member unit devices are registered to the logical device
	//
	STDMETHODIMP Register(
		__in INdasUnit* pNdasUnit, 
		__deref_out INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHODIMP Unregister(
		__in INdasUnit* pNdasUnit);

	STDMETHODIMP get_NdasLogicalUnit(
		__in NDAS_LOGICALDEVICE_ID id, 
		__deref_out INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHODIMP get_NdasLogicalUnit(
		__in NDAS_LOGICALUNIT_DEFINITION* config, 
		__deref_out INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHODIMP get_NdasLogicalUnits(
		__in DWORD Flags /*= NDAS_ENUM_DEFAULT */,
		__out CInterfaceArray<INdasLogicalUnit>& v);

	STDMETHODIMP OnSystemShutdown();
	STDMETHODIMP Cleanup();

public:
	
	enum 
	{ 
		// Slot Number is 1-based
		MAX_SLOT_NO = 255, // [1...255]
		// Unit Number is 0-based
		MAX_UNIT_NO = 1    // [0, 1]
	};

protected:

	CInterfaceArray<INdasLogicalUnit> m_NdasLogicalUnits;
	NdasLogicalUnitDefinitionMap m_LogicalUnitDefinitionMap;
	LogicalDeviceIdMap m_LogicalUnitIdMap;

	HRESULT pAllocateNdasLogicalUnitId(
		__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
		__in BSTR RegistrySubPath,
		__in INdasUnit* pNdasUnit,
		__out NDAS_LOGICALDEVICE_ID * NdasLogicalUnitId);

	HRESULT pAllocateVStorId(
		__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
		__in BSTR RegistrySubPath,
		__out NDAS_LOGICALDEVICE_ID * NdasLogicalUnitId);

	HRESULT pAllocateRegistryPath(
		__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
		__deref_out BSTR* RegistrySubPath);

	bool pDeallocateID(NDAS_LOGICALDEVICE_ID logDeviceId);

	typedef CAutoLock<CNdasLogicalUnitManager> CAutoLock;
	friend class CAutoLock;

};

#endif
