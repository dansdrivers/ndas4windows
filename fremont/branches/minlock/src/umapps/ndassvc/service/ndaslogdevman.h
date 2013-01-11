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

typedef std::map<NDAS_LOGICALDEVICE_GROUP,INdasLogicalUnit*> LogicalDeviceGroupMap;
typedef std::map<NDAS_LOGICALDEVICE_ID,INdasLogicalUnit*> LogicalDeviceIdMap;
typedef std::map<NDAS_LOCATION,INdasLogicalUnit*> LocationMap;

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
	STDMETHODIMP Register(INdasUnit* pNdasUnit, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHODIMP Unregister(INdasUnit* pNdasUnit);

	STDMETHODIMP get_NdasLogicalUnit(NDAS_LOGICALDEVICE_ID id, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHODIMP get_NdasLogicalUnit(NDAS_LOGICALDEVICE_GROUP* config, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHODIMP get_NdasLogicalUnitByNdasLocation(NDAS_LOCATION location, INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHODIMP RegisterNdasLocation(NDAS_LOCATION location, INdasLogicalUnit* logicalDevice);
	STDMETHODIMP UnregisterNdasLocation(NDAS_LOCATION location, INdasLogicalUnit* logicalDevice);

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

	std::bitset<MAX_SLOT_NO> m_slotbitset;

	CInterfaceArray<INdasLogicalUnit> m_NdasLogicalUnits;
	LocationMap m_NdasLocationMap;
	LogicalDeviceGroupMap m_LogicalUnitDefinitionMap;
	LogicalDeviceIdMap m_LogicalUnitIdMap;

	NDAS_LOGICALDEVICE_ID pAllocateID();
	bool pDeallocateID(NDAS_LOGICALDEVICE_ID logDeviceId);

	typedef CAutoLock<CNdasLogicalUnitManager> CAutoLock;
	friend class CAutoLock;

};

#endif
