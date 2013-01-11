/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEV_H_
#define _NDASLOGDEV_H_

#include "ndas/ndastypeex.h"
#include "extobj.h"
#include "syncobj.h"
#include "ndasdevid.h"
#include "ndaslogdevman.h"

class CNdasLogicalDevice;
class CNdasLogicalDisk;
class CNdasLogicalDVD;

/*++

NDAS Logical Device class

An abstract base class for logical devices.

--*/

class CNdasLogicalDevice :
	public ximeta::CCritSecLockGlobal,
	public ximeta::CExtensibleObject
{
//	friend class CNdasServiceDeviceEventHandler;

public:
	static const DWORD MAX_UNITDEVICE_ENTRY = 64;

protected:

	static const size_t MAX_STRING_REP = 256;

	//
	// String buffer for string representation of the device
	//
	TCHAR m_szStringRep[MAX_STRING_REP];

	//
	// Logical Device ID
	//
	CONST NDAS_LOGICALDEVICE_ID m_logicalDeviceId;

	CNdasScsiLocation m_NdasScsiLocation;
	//
	// Logical device group information
	//
	CONST NDAS_LOGICALDEVICE_GROUP m_logicalDeviceGroup;

	CNdasUnitDevice* m_pUnitDevices[MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER];

	//
	// Logical device group hash value
	//
	DWORD m_dwHashValue;

	//
	// Unit device instance count
	//
	DWORD m_nUnitDeviceInstances;

	//
	// Current status of the logical device
	//
	NDAS_LOGICALDEVICE_STATUS m_status;

	//
	// Recent AdapterStatus retrieved by ::LsBusCtlQueryStatus
	//
	ULONG m_ulAdapterStatus;

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
	// support for reconnecting event
	//
	BOOL m_bReconnecting;

	//
	// support for disconnected event
	BOOL m_fDisconnected;

	//
	// Registry Container Path
	//
	TCHAR m_szRegContainer[30];

	// Autonomous mount flag
	//
	ACCESS_MASK m_mountOnReadyAccess;
	BOOL m_fReducedMountOnReadyAccess;
	BOOL m_fMountOnReady;

	BOOL m_fRiskyMount;

	DWORD m_dwMountTick;

	//
	// Current MRB
	//
	DWORD m_dwCurrentMRB;

	//
	// System shutdown flag (retain mount information)
	//
	DWORD m_fShutdown;

public:

	// internal routines
	VOID SetReconnectFlag(BOOL bReconnecting) 
	{ 
		m_bReconnecting = bReconnecting; 
	}

	BOOL GetReconnectFlag() 
	{ 
		return m_bReconnecting; 
	}

public:

	//
	// CExtensibleObject
	//
	virtual ULONG AddRef();
	virtual ULONG Release();

	//
	// Constructor for a multiple member logical device
	//
	CNdasLogicalDevice(
		NDAS_LOGICALDEVICE_ID logDevId,
		CONST NDAS_LOGICALDEVICE_GROUP& group);

	//
	// Destructor
	//
	virtual ~CNdasLogicalDevice();

	//
	// Logical Device ID
	//
	NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

	//
	// AdapterStatus
	//
	ULONG GetAdapterStatus();
	ULONG SetAdapterStatus(ULONG ulAdapterStatus);

	//
	// Set the unit device ID at a sequence 
	// to a unit device member ID list
	//
	BOOL AddUnitDevice(CNdasUnitDevice& unitDevice);

	//
	// Remove the unit device ID from the list
	//
	BOOL RemoveUnitDevice(CNdasUnitDevice& unitDevice);

	//
	// Get the unit device instance count
	//
	DWORD GetUnitDeviceInstanceCount();

	CONST NDAS_UNITDEVICE_ID& GetUnitDeviceID(DWORD ldSequence);

	CNdasUnitDevice* GetUnitDevice(DWORD ldSequence);

	CONST NDAS_LOGICALDEVICE_GROUP& GetLDGroup();

	CONST NDAS_SCSI_LOCATION& GetNdasScsiLocation();

	//
	// Get the defined maximum number of unit devices
	//
	DWORD GetUnitDeviceCount();
	DWORD GetUnitDeviceCountSpare();
	DWORD GetUnitDeviceCountInRaid();

	//
	// Get the type of the logical device
	//
	virtual NDAS_LOGICALDEVICE_TYPE GetType();

	//
	// Initializer interface for Logical Devices
	//
	virtual BOOL Initialize();

	//
	// Plug-in the logical device to the LANSCSI Bus and
	// make it available to the OS as a logical (storage) device
	//
	virtual BOOL PlugIn(ACCESS_MASK requestingAccess);

	//
	// Eject the logical device from the LANSCSI Bus,
	// which will be an action of ejecting a device from the OS
	//
	virtual BOOL Eject();

	//
	// Unplug is a similar to the eject operation.
	// However, this action makes a device removed from the OS
	// without giving a chance to applications to 
	virtual BOOL Unplug();

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
	// Get drive letters of volumes in the logical device
	// 
	virtual DWORD GetMountedDriveSet();

	//
	// Get the status of the logical device
	//
	virtual NDAS_LOGICALDEVICE_STATUS GetStatus();

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

	//
	// Get the minimum Max Request Block size
	// for whole members.
	//
	virtual DWORD GetCurrentMaxRequestBlocks();

	//
	// Total available block count
	//
	virtual DWORD GetUserBlockCount();

	//
	// Shared Write as Primary Host
	//
	BOOL GetSharedWriteInfo(LPBOOL lpbSharedWriteFeature, LPBOOL lpbPrimary);

	//
	// Set mount-on-ready flag when all instanced are found
	//
	// Parameters:
	//
	// access
	//		GENERIC_READ or GENERIC_READ | GENERIC_WRITE
	//
	// fAllowReducedAccess
	//		if non-zero, when write access fails, 
	//		mount it read-only
	//
	VOID SetMountOnReady(
		ACCESS_MASK access, 
		BOOL fReducedMountOnReadyAccess = FALSE);

	//
	// Get Risky Mount Flag
	//
	BOOL IsRiskyMount();

	//
	// Update RiskyMountFlag from the registry
	// (Do not use this to check risky status, use IsRiskyMount())
	//
	BOOL GetRiskyMountFlag();

	//
	// Set Fault after ADAPTERINFO_STATUSFLAG_MEMBER_FAULT
	//
	VOID SetAllUnitDevicesFault();
	BOOL IsAnyUnitDevicesFault();

	//
	// Clear Risky Mount Flag
	// (Called from NDSAEVENTMON)
	// 
	VOID SetRiskyMountFlag(BOOL fRisky);

	//
	// Set the mounted mask to the registry
	//
	VOID SetLastMountAccess(ACCESS_MASK mountedAccess);

	//
	// Previous mounted mask
	//
	ACCESS_MASK GetLastMountAccess();

	//
	// Get the mounted tick
	// 0 if no mount tick is available
	//
	DWORD GetMountTick();

	//
	// Is any member unit device of the logical device is volatile?
	//
	BOOL IsVolatile();

	///////////////////////////////////////////////////////////////////////
	//
	// Event Handlers
	//
	///////////////////////////////////////////////////////////////////////

	//
	// Service handler call is when the system is being shutdown
	//
	VOID OnShutdown();

	//
	// Logical device is disconnected
	//
	VOID OnDisconnected();

	//
	// Device Monitor calls this when NDAS SCSI is not alive.
	//
	VOID OnDeviceStatusFailure();

	//
	// PNP Event Handler calls this on mounted
	//
	VOID OnMounted();

	//
	// PNP Event Handler calls this on unmount process is completed
	//
	VOID OnUnmounted();

	//
	// PNP Event Handler calls this when the unmount process is failed.
	//
	VOID OnUnmountFailed();

protected:

	BOOL cpCheckPlugInCondition(ACCESS_MASK requestingAccess);
	BOOL cpCheckPlugInCondForDVD(ACCESS_MASK requestingAccess);
	
	DWORD cpGetMaxRequestBlocks();

	VOID cpAllocateNdasScsiLocation();
	VOID cpDeallocateNdasScsiLocation();

	//
	// Is this logical device is complete?
	// To reference the actual NDAS SCSI LOCATION
	//
	BOOL IsComplete();

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

	//
	// Calculate the hash value of this object based on
	// the NDAS_LOGICALDEVICE_GROUP value
	//
	DWORD cpGetHashValue();

	//
	// Locate the reg container based on the hash value
	// The actual container may different to the hash value
	// due to the collision
	//
	VOID cpLocateRegContainer();

	BOOL IsPSWriteShareCapable();
	BOOL IsWriteAccessAllowed(BOOL fPSWriteShare, CNdasUnitDevice* pUnitDevice);

	BOOL ReconcileFromNdasBus();
};

#endif
