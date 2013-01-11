/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include "ndassvcdef.h"
#include "ndasdev.h"
#include "syncobj.h"
#include <bitset>

typedef std::map<DWORD,CNdasDevicePtr> DeviceSlotMap;
typedef std::map<NDAS_DEVICE_ID,CNdasDevicePtr> DeviceIdMap;

//typedef std::pair<DWORD,CNdasDevicePtr> DeviceSlotPair;
//typedef std::pair<NDAS_DEVICE_ID,CNdasDevicePtr> DeviceIdPair;

class CNdasDeviceRegistrar :
	public ximeta::CCritSecLockGlobal // support class lock
{
	// to make NDAS_DEVICE_ID as a key,
	// we made a specialization of the std::less structure template
	// refer to 
	CNdasService& m_service;
	DeviceSlotMap m_deviceSlotMap;
	DeviceIdMap m_deviceIdMap;

	// PBOOL m_pbSlotOccupied;

	std::vector<bool> m_slotbit;

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

	//
	// Import Legacy Settings function
	//
	BOOL ImportLegacyEntry(DWORD dwSlotNo, HKEY hEntryKey);
	BOOL ImportLegacySettings();

	BOOL m_fBootstrapping;

public:

	typedef ximeta::CAutoLock<CNdasDeviceRegistrar> InstanceAutoLock;

	//
	// constructor
	//
	// dwMaxSlotNo is a maximum slot number.
	// Slot number is a 1-based index.
	//
	CNdasDeviceRegistrar(
		CNdasService& service,
		DWORD dwMaxSlotNo = MAX_SLOT_NUMBER);

	//
	// destructor
	//
	virtual ~CNdasDeviceRegistrar();

	//
	// Register a ndas host to a specified slot
	//
	// Returns a registered ndas host if succeeded, otherwise returns NULL.
	// Call GetLastError for an extended information.
	//
	CNdasDevicePtr Register(
		__in_opt DWORD SlotNo,
		__in const NDAS_DEVICE_ID& DeviceId,
		__in DWORD Flags,
		__in_opt const NDASID_EXT_DATA* NdasIdExtension);

	//
	// Unregister a ndas host from a slot
	//
	// Returns non-zero if succeeded.
	// Call GetLastError for an extended information.
	//
	BOOL Unregister(const NDAS_DEVICE_ID& DeviceId);
	BOOL Unregister(DWORD dwSlotNo);

	//
	// Look up a ndas device by host id
	//
	// Returns a ndas device if found, otherwise returns NULL.
	// 
	CNdasDevicePtr Find(const NDAS_DEVICE_ID& DeviceId);

	//
	// Look up a ndas device by slot number
	//
	// Returns a ndas device if found, otherwise returns NULL.
	// Call GetLastError for an extended information.
	//
	CNdasDevicePtr Find(DWORD dwSlotNo);

	//
	// Look up a ndas device by device id or slot
	//
	CNdasDevicePtr Find(const NDAS_DEVICE_ID_EX& Device);

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

	//
	// Shutdown Notification
	//
	void OnServiceShutdown();

	//
	// Process Terminate Notification
	//
	void OnServiceStop();

	void GetItems(CNdasDeviceVector& dest);

	void Cleanup();

private:
	// NOT IMPLEMENTED
	CNdasDeviceRegistrar(const CNdasDeviceRegistrar&);
	CNdasDeviceRegistrar& operator=(const CNdasDeviceRegistrar&);
};

