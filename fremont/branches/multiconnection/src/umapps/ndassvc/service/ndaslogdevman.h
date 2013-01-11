/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEVMAN_H_
#define _NDASLOGDEVMAN_H_

#include "ndas/ndastypeex.h"
#include "ndasdevid.h"
#include "syncobj.h"
#include "ndassvcdef.h"

// external reference declarations without including headers
class CNdasLogicalDevice;
class CNdasDeviceRegistrar;
class CNdasDevice;
class CNdasUnitDevice;

// forward declaration
class CNdasLogicalDeviceManager;
class CNdasService;

#include <bitset>

typedef std::map<NDAS_LOGICALDEVICE_GROUP,CNdasLogicalDevicePtr> LogicalDeviceGroupMap;
typedef std::map<NDAS_LOGICALDEVICE_ID,CNdasLogicalDevicePtr> LogicalDeviceIdMap;
typedef std::map<NDAS_LOCATION,CNdasLogicalDevicePtr> LocationMap;


class CNdasLogicalDeviceManager :
	public ximeta::CCritSecLockGlobal
{
public:
	
	// Slot Number is 1-based
	static const DWORD MAX_SLOT_NO = 255; // [1...255]
	
	// Unit Number is 0-based
	static const DWORD MAX_UNIT_NO = 1; // [0,1]

protected:

	std::bitset<MAX_SLOT_NO> m_slotbitset;

	CNdasService& m_service;

	LocationMap m_NdasLocationMap;
	LogicalDeviceGroupMap m_LDGroupMap;
	LogicalDeviceIdMap m_LDIDMap;

	NDAS_LOGICALDEVICE_ID _AllocateID();
	bool _DeallocateID(NDAS_LOGICALDEVICE_ID logDeviceId);

public:

	typedef ximeta::CAutoLock<CNdasLogicalDeviceManager> InstanceAutoLock;

	CNdasLogicalDeviceManager(CNdasService& service);
	~CNdasLogicalDeviceManager();

	//
	// Registers an unit device to create an instance of the logical device
	// or to add to an existing instance of the logical device
	// Logical device can be instantiated (plugged in) when
	// all member unit devices are registered to the logical device
	//
	CNdasLogicalDevicePtr Register(CNdasUnitDevicePtr unitDevice);
	bool Unregister(CNdasUnitDevicePtr unitDevice);

	CNdasLogicalDevicePtr Find(NDAS_LOGICALDEVICE_ID logDevId);
	CNdasLogicalDevicePtr Find(const NDAS_LOGICALDEVICE_GROUP& group);
	CNdasLogicalDevicePtr FindByNdasLocation(NDAS_LOCATION location);

	DWORD Size();

	//
	// Registers the NDAS SCSI Location for mounted devices
	//
	BOOL RegisterNdasLocation(
		const NDAS_LOCATION& location, 
		CNdasLogicalDevicePtr logicalDevice);

	BOOL UnregisterNdasLocation(
		const NDAS_LOCATION& location,
		CNdasLogicalDevicePtr logicalDevice);

	void GetItems(CNdasLogicalDeviceVector& v);
	void Cleanup();
	void OnShutdown();
};

#endif
