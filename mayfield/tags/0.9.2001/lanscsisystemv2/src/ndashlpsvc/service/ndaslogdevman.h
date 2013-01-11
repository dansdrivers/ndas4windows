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
typedef CNdasLogicalDevice *PCNdasLogicalDevice;

class CNdasDeviceRegistrar;
typedef CNdasDeviceRegistrar *PCNdasDeviceRegistrar;

class CNdasDevice;
typedef CNdasDevice *PCNdasDevice;

class CNdasUnitDevice;
typedef CNdasUnitDevice *PCNdasUnitDevice;

// forward declaration
class CNdasLogicalDeviceManager;
typedef CNdasLogicalDeviceManager *PCNdasLogicalDeviceManager;


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

	typedef std::map<DWORD,PCNdasLogicalDevice> LDSlotToLogicalDeviceMap;
	typedef std::map<NDAS_UNITDEVICE_ID,DWORD> UnitDeviceIdToLDSlotMap;

	LDSlotToLogicalDeviceMap m_LDSlotToLDMap;
	UnitDeviceIdToLDSlotMap m_UnitDeviceIdToLDSlotMap;
	const PCNdasDeviceRegistrar m_pRegistrar;

	PCNdasLogicalDevice _FindEntry(NDAS_UNITDEVICE_ID unitDeviceId);
	PCNdasLogicalDevice _FindEntry(DWORD dwSlot);
	
//	bool _AddDevice(PCNdasLogicalDevice pLogicalDevice);
//	bool _RemoveEntry(PCNdasLogicalDevice pLogicalDevice);

	PCNdasUnitDevice _FindUnitDevice(NDAS_UNITDEVICE_ID unitDeviceId);

	DWORD _GetAvailSlot();

public:

	CNdasLogicalDeviceManager(PCNdasDeviceRegistrar pRegistrar);
	virtual ~CNdasLogicalDeviceManager();

	PCNdasLogicalDevice Register(
		NDAS_LOGICALDEVICE_TYPE logicalDeviceType, 
		NDAS_UNITDEVICE_ID unitDeviceId,
		DWORD nSequence, 
		NDAS_UNITDEVICE_ID primaryUnitDeviceId,
		DWORD nUnitDevices);
	
	BOOL Unregister(NDAS_DEVICE_ID deviceId, DWORD dwUnitNo);
	BOOL Unregister(NDAS_UNITDEVICE_ID unitDeviceId);

	PCNdasLogicalDevice Find(NDAS_UNITDEVICE_ID unitDeviceId);
	PCNdasLogicalDevice Find(NDAS_DEVICE_ID deviceId, DWORD dwUnitNo);
	PCNdasLogicalDevice Find(DWORD dwSlot);

	DWORD Size();

	const PCNdasDeviceRegistrar GetNdasDeviceRegistrar();

	// in-class iteration support
	typedef LDSlotToLogicalDeviceMap::const_iterator ConstIterator;
	ConstIterator begin();
	ConstIterator end();

};

#endif
