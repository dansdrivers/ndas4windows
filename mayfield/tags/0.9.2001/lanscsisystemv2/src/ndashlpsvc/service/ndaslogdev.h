/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEV_H_
#define _NDASLOGDEV_H_

#include "ndastypeex.h"

#include "syncobj.h"
#include "ndasdevid.h"
#include "ndaslogdevman.h"

class CNdasLogicalDevice;
typedef CNdasLogicalDevice *PCNdasLogicalDevice;

class CNdasLogicalDisk;
typedef CNdasLogicalDisk *PCNdasLogicalDisk;

class CNdasLogicalDVD;
typedef CNdasLogicalDVD *PCNdasLogicalDVD;

/*++

NDAS Logical Device class

An abstract base class for logical devices.

--*/

class CNdasLogicalDevice :
	public ximeta::CCritSecLock
{
	friend class CNdasServiceDeviceEventHandler;

public:
	static const DWORD MAX_UNITDEVICE_ENTRY = 64;

protected:

	static const size_t MAX_STRING_REP = 256;

	static const DWORD READ_ONLY_USER_ID =  0x00000001;
	static const DWORD READ_WRITE_USER_ID = 0x00010001;


	//
	// String buffer for string representation of the device
	//
	TCHAR m_szStringRep[MAX_STRING_REP];

	//
	// Slot number of the logical device in a LANSCSI bus
	//
	const DWORD m_dwSlot;

	//
	// Logical device type
	//
	const NDAS_LOGICALDEVICE_TYPE m_devType;

	//
	// Logical device is identified by a primary unit device ID
	//
	NDAS_UNITDEVICE_ID m_primaryUnitDeviceId;

	//
	// Unit device instance count
	DWORD m_nUnitDeviceInstances;

	//
	// unit device Id count
	//
	const DWORD m_nUnitDevices;

	//
	// unit device ID array
	//
	PNDAS_UNITDEVICE_ID m_pUnitDeviceIds;

	//
	// Current status of the logical device
	//
	NDAS_LOGICALDEVICE_STATUS m_status;

	//
	// Last error of the logical device
	//
	NDAS_LOGICALDEVICE_ERROR m_lastError;

	//
	// Drive letter bit-mask set of the volumes in a logical device.
	// Only valid when mounted.
	//
	DWORD m_dwMountedDriveSet;
	
	//
	// Mounted access of the logical device
	//
	ACCESS_MASK m_MountedAccess;

	//
	// Disconnected event handle for a notification from the LANSCSI Bus
	//
	HANDLE m_hDisconnectedEvent;

	//
	// Alarm event handle for a notification from the LANSCSI Bus
	//
	HANDLE m_hAlarmEvent;

	//
	// Constructor for a multiple member logical device
	//
	CNdasLogicalDevice(
		DWORD dwSlot, 
		NDAS_LOGICALDEVICE_TYPE type,
		NDAS_UNITDEVICE_ID primaryUnitDeviceId,
		DWORD nUnitDevices = 0);

public:

	//
	// Destructor
	//
	virtual ~CNdasLogicalDevice();

	//
	// Set the unit device ID at a sequence 
	// to a unit device member ID list
	//
	BOOL AddUnitDeviceId(
		DWORD dwSequence, 
		NDAS_UNITDEVICE_ID unitDeviceId);

	//
	// Remove the unit device ID from the list
	//
	BOOL RemoveUnitDeviceId(
		NDAS_UNITDEVICE_ID unitDeviceId);

	//
	// Get the unit device instance count
	//
	DWORD GetUnitDeviceInstanceCount();

	//
	// Get the defined maximum number of unit devices
	//
	DWORD GetUnitDeviceCount();

	//
	// Get the unit device ID in i-th sequence.
	// 0 means the primary unit device ID.
	//
	NDAS_UNITDEVICE_ID GetUnitDeviceId(DWORD dwSequence);

	//
	// Get the type of the logical device
	//
	virtual NDAS_LOGICALDEVICE_TYPE GetType();

	//
	// Initializer interface for Logical Devices
	//
	virtual BOOL Initialize() = 0;

	//
	// Plug-in the logical device to the LANSCSI Bus and
	// make it available to the OS as a logical (storage) device
	//
	virtual BOOL PlugIn(ACCESS_MASK requestingAccess) = 0;

	//
	// Eject the logical device from the LANSCSI Bus,
	// which will be an action of ejecting a device from the OS
	//
	virtual BOOL Eject() = 0;

	//
	// Unplug is a similar to the eject operation.
	// However, this action makes a device removed from the OS
	// without giving a chance to applications to 
	virtual BOOL Unplug() = 0;

	//
	// Get the combined access mask of the all member unit devices
	// based on the registration information in the device registrar
	//
	virtual ACCESS_MASK GetGrantedAccess();

	//
	// Get the combined access mask of the all member unit devices
	// which is allowed at the time of calling this function.
	// An allowing access is a runtime access mask
	// whereas a granted access is configured access mask.
	//
	virtual ACCESS_MASK GetAllowingAccess();

	//
	// Get the mounted access mask of the logical device 
	//
	virtual ACCESS_MASK GetMountedAccess();

	//
	// Slot number of the logical device
	//
	virtual DWORD GetSlot();

	//
	// Get drive letters of volumes in the logical device
	// 
	virtual DWORD GetMountedDriveSet();

	//
	// Get the status of the logical device
	//
	virtual NDAS_LOGICALDEVICE_STATUS GetStatus() = 0;

	//
	// Get the last logical device error
	//
	virtual NDAS_LOGICALDEVICE_ERROR GetLastError();

	//
	// Get the string representation of the logical device
	//
	virtual LPCTSTR ToString();

	//
	// Get an event handle for disconnection
	//
	virtual HANDLE GetDisconnectEvent();

	//
	// Get an event handle for alarm
	//
	virtual HANDLE GetAlarmEvent();

protected:

	//
	// Set the mounted access of the logical device
	// (Called from the PNP Handler)
	//
	virtual VOID SetMountedAccess(ACCESS_MASK mountedAccess);

	//
	// Set the mounted drive letters
	// (Called from the PNP Handler)
	//
	virtual VOID SetMountedDriveSet(DWORD dwDriveSet);

	//
	// Set the current logical device status
	//
	virtual VOID SetStatus(NDAS_LOGICALDEVICE_STATUS newStatus);

	//
	// Set the last device error
	//
	virtual VOID SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR logDevError);

};

/*++

NDAS Logical Disk Class

A class to represent Logical Disks.
Including Single Disk, Bound (Aggregated or Mirrored) Disks 

--*/

class CNdasLogicalDisk :
	public CNdasLogicalDevice
{

protected:

	//
	// Sum of the block numbers of unit devices
	//
	ULONG m_ulBlocks;

public:

	//
	// Constructor for a single disk
	//
	CNdasLogicalDisk(
		DWORD dwSlot, 
		NDAS_LOGICALDEVICE_TYPE Type,
		NDAS_UNITDEVICE_ID unitDeviceId);

	//
	// Constructor for bound disk (aggregated or mirrored)
	//
	CNdasLogicalDisk(
		DWORD dwSlot,
		NDAS_LOGICALDEVICE_TYPE Type,
		NDAS_UNITDEVICE_ID primaryUnitDeviceId,
		DWORD dwUnitDevices);

	//
	// Destructor
	//
	virtual ~CNdasLogicalDisk();

	//
	// Get the sum of the block counts of whole members.
	// Returns 0 when a member is missing from the device registrar
	//
	// virtual DWORD GetBlocks();
	virtual DWORD GetUserBlockCount();

	//
	// Get the minimum Max Request Block size
	// for whole members.
	//
	virtual DWORD GetMaxRequestBlocks();

	//
	// Overloaded member functions
	//

	virtual BOOL Initialize();
	virtual BOOL PlugIn(ACCESS_MASK access);
	virtual BOOL Unplug();
	virtual BOOL Eject();
	virtual NDAS_LOGICALDEVICE_STATUS GetStatus();

	virtual LPCTSTR ToString();
};

/*++

Logical DVD class

A class to represent Logical DVDs
(Incomplete)

--*/

class CNdasLogicalDVD :
	public CNdasLogicalDevice
{
public:

	CNdasLogicalDVD(
		DWORD dwSlot, 
		NDAS_UNITDEVICE_ID unitDeviceId);

	virtual BOOL PlugIn();
	virtual BOOL Unplug();
	virtual BOOL Eject();
	virtual NDAS_LOGICALDEVICE_STATUS GetStatus();
};

#endif
