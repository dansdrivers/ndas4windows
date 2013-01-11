/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEVMAN_H_
#define _NDASLOGDEVMAN_H_

#include "ndastypeex.h"
#include "ndasdevid.h"
#include "syncobj.h"

// external reference declarations without including headers
class CNdasLogicalDevice;
class CNdasDeviceRegistrar;
class CNdasDevice;
class CNdasUnitDevice;

// forward declaration
class CNdasLogicalDeviceManager;
typedef CNdasLogicalDeviceManager* PCNdasLogicalDeviceManager;

#include <bitset>

class CNdasLogicalDeviceManager :
	public ximeta::CCritSecLock
{
public:
	
	// Slot Number is 1-based
	static const DWORD MAX_SLOT_NO = 255; // [1...255]
	
	// Unit Number is 0-based
	static const DWORD MAX_UNIT_NO = 1; // [0,1]

protected:

	std::bitset<MAX_SLOT_NO> m_slotbitset;

	typedef std::map<NDAS_LOGICALDEVICE_GROUP,CNdasLogicalDevice*> LDGroupMap;
	typedef std::map<NDAS_LOGICALDEVICE_ID,CNdasLogicalDevice*> LDIDMap;
	typedef std::map<NDAS_SCSI_LOCATION,CNdasLogicalDevice*> LocationMap;

	LocationMap m_NdasScsiLocationMap;
	LDGroupMap m_LDGroupMap;
	LDIDMap m_LDIDMap;

	NDAS_LOGICALDEVICE_ID cpAllocateID();
	BOOL cpDeallocateID(NDAS_LOGICALDEVICE_ID id);

public:

	CNdasLogicalDeviceManager();
	virtual ~CNdasLogicalDeviceManager();

	//
	// Registers an unit device to create an instance of the logical device
	// or to add to an existing instance of the logical device
	// Logical device can be instantiated (plugged in) when
	// all member unit devices are registered to the logical device
	//
	CNdasLogicalDevice* Register(
		CNdasUnitDevice& unitDevice,
		ACCESS_MASK acMountOnReady = 0,
		BOOL fReducedMountOnReady = FALSE);

	BOOL Unregister(CNdasUnitDevice& unitDevice);

	CNdasLogicalDevice* Find(NDAS_LOGICALDEVICE_ID logDevId);
	CNdasLogicalDevice* Find(CONST NDAS_LOGICALDEVICE_GROUP& group);
	CNdasLogicalDevice* Find(CONST NDAS_SCSI_LOCATION& location);

	DWORD Size();

	//
	// Registers the NDAS SCSI Location for mounted devices
	//
	BOOL RegisterNdasScsiLocation(
		CONST NDAS_SCSI_LOCATION& location, 
		CNdasLogicalDevice& logicalDevice);

	BOOL UnregisterNdasScsiLocation(
		CONST NDAS_SCSI_LOCATION& location);

	// in-class iteration support
	typedef LDIDMap::const_iterator ConstIterator;
	ConstIterator begin();
	ConstIterator end();

	VOID OnShutdown();
};

#endif
