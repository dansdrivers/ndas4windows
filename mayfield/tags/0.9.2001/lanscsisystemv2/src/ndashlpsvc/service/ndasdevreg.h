/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include "ndasdev.h"
#include "syncobj.h"

class CNdasDeviceRegistrar;
typedef CNdasDeviceRegistrar *PCNdasDeviceRegistrar;

class CNdasDeviceRegistrar :
	public ximeta::CCritSecLock // support class lock
{
	friend class CNdasDeviceIterator;

	// to make NDAS_DEVICE_ID as a key,
	// we made a specialization of the std::less structure template
	// refer to 
	typedef std::map<DWORD,PCNdasDevice> DeviceSlotMap;
	typedef std::map<NDAS_DEVICE_ID,PCNdasDevice> DeviceIdMap;

	typedef std::pair<DWORD,PCNdasDevice> DeviceSlotPair;
	typedef std::pair<NDAS_DEVICE_ID,PCNdasDevice> DeviceIdPair;

	DeviceSlotMap m_deviceSlotMap;
	PBOOL m_pbSlotOccupied;

	DeviceIdMap m_deviceIdMap;

	const DWORD m_dwMaxSlotNo;

	//
	// Look up empty slot
	//
	// returns an empty slot number if successful.
	// otherwise returns 0 - as the slot no is one-based index.
	//
	DWORD LookupEmptySlot();

	static const DWORD MAX_SLOT_NUMBER = 255;

	static const TCHAR CFG_CONTAINER[];

private:
	// NOT IMPLEMENTED
	CNdasDeviceRegistrar(const CNdasDeviceRegistrar&);
	CNdasDeviceRegistrar& operator=(const CNdasDeviceRegistrar&);

public:
	//
	// constructor
	//
	// dwMaxSlotNo is a maximum slot number.
	// Slot number is a 1-based index.
	//
	CNdasDeviceRegistrar(DWORD dwMaxSlotNo = MAX_SLOT_NUMBER);

	//
	// destructor
	//
	virtual ~CNdasDeviceRegistrar();

	//
	// Register a ndas host to an empty slot
	//
	// Returns a registered ndas host if succeeded, otherwise returns NULL.
	// Call GetLastError for an extended information.
	//
	PCNdasDevice Register(NDAS_DEVICE_ID DeviceId);

	//
	// Register a ndas host to a specified slot
	//
	// Returns a registered ndas host if succeeded, otherwise returns NULL.
	// Call GetLastError for an extended information.
	//
	PCNdasDevice Register(NDAS_DEVICE_ID DeviceId, DWORD dwSlotNo);

	//
	// Unregister a ndas host from a slot
	//
	// Returns non-zero if succeeded.
	// Call GetLastError for an extended information.
	//
	BOOL Unregister(NDAS_DEVICE_ID DeviceId);
	BOOL Unregister(DWORD dwSlotNo);

	//
	// Look up a ndas host by host id
	//
	// Returns a ndas host if found, otherwise returns NULL.
	// 
	PCNdasDevice Find(NDAS_DEVICE_ID DeviceId);

	//
	// Look up a ndas host by slot number
	//
	// Returns a ndas host if found, otherwise returns NULL.
	// Call GetLastError for an extended information.
	//
	PCNdasDevice Find(DWORD dwSlotNo);

	//
	// Returns the number of occupied slot
	//
	DWORD Size();

	//
	// Returns the maximum slot no
	//
	DWORD MaxSlotNo();

	//
	// Bootstrap from the registry
	//
	BOOL Bootstrap();

	// in-class iteratoration support
	typedef DeviceSlotMap::const_iterator ConstIterator;
	ConstIterator begin();
	ConstIterator end();
};
