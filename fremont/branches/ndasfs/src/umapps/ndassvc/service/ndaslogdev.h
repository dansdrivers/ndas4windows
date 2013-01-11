/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEV_H_
#define _NDASLOGDEV_H_

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <ndas/ndastypeex.h>
#include <ndas/ndascntenc.h>
#include <ndas/ndasop.h>
#include <ndas/ndasportioctl.h>
#include <xtl/xtlautores.h>
#include "syncobj.h"
#include "ndassvcdef.h"
#include "ndasdevid.h"
#include "ndaslogdevman.h"
#include "objstr.h"

/*++

NDAS Logical Device class

--*/

class CNdasLogicalDevice :
	public ximeta::CCritSecLockGlobal,
	public CStringizerA<32>,
	public boost::enable_shared_from_this<CNdasLogicalDevice>
{

protected:

	//
	// Logical Device ID
	//
	const NDAS_LOGICALDEVICE_ID m_logicalDeviceId;

	NDAS_LOCATION m_NdasLocation;

	BOOL m_NdasPort;

	//
	// Logical device group information
	//
	const NDAS_LOGICALDEVICE_GROUP m_logicalDeviceGroup;

	CNdasUnitDeviceWeakVector m_unitDevices;

	TCHAR m_DevicePath[MAX_PATH];

	//
	// Logical device group hash value
	//
	DWORD m_dwHashValue;

	////
	//// Unit device instance count
	////
	//DWORD m_nUnitDeviceInstances;

	//
	// Current status of the logical device
	//
	NDAS_LOGICALDEVICE_STATUS m_status;

	//
	// Recent AdapterStatus retrieved by ::NdasBusCtlQueryStatus
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
	XTL::AutoObjectHandle m_hDisconnectedEvent;

	//
	// Alarm event handle for a notification from the LANSCSI Bus
	//
	XTL::AutoObjectHandle m_hAlarmEvent;

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

	//
	// Copy of the encryption information from the primary unit device
	// This is just a placeholder to hold the content during
	// the lifetime of the logical device
	//
	NDAS_CONTENT_ENCRYPT m_contentEncrypt;

	BOOL m_fMountable;
	BOOL m_fDegradedMode;

	NDAS_RAID_FAIL_REASON m_RaidFailReason;
	GUID m_ConfigSetId;
	
public:

	// internal routines
	void SetReconnectFlag(BOOL bReconnecting) 
	{ 
		m_bReconnecting = bReconnecting; 
	}

	BOOL GetReconnectFlag() 
	{ 
		return m_bReconnecting; 
	}

public:

	typedef ximeta::CAutoLock<CNdasLogicalDevice> InstanceAutoLock;

	//
	// Constructor for a multiple member logical device
	//
	CNdasLogicalDevice(
		NDAS_LOGICALDEVICE_ID logDevId,
		const NDAS_LOGICALDEVICE_GROUP& group);

	//
	// Destructor
	//
	~CNdasLogicalDevice();

	//
	// Initializer interface for Logical Devices
	//
	BOOL Initialize();

	//
	// Invalidate all unit disks and logical device information.
	//
	BOOL Invalidate();
	
	//
	// Logical Device ID
	//
	NDAS_LOGICALDEVICE_ID GetLogicalDeviceId() const;

	//
	// Get the defined maximum number of unit devices
	//
	DWORD GetUnitDeviceCount() const;
	DWORD GetUnitDeviceCountSpare() const;
	DWORD GetUnitDeviceCountInRaid() const;

	//
	// Get the type of the logical device
	//
	NDAS_LOGICALDEVICE_TYPE GetType() const;
	const NDAS_UNITDEVICE_ID& GetUnitDeviceID(DWORD ldSequence) const;
	const NDAS_LOGICALDEVICE_GROUP& GetLDGroup() const;
	CNdasUnitDevicePtr GetUnitDevice(DWORD ldSequence) const;

	LPCTSTR GetDevicePath() const;

	// Support for filtering hidden devices
	bool IsHidden();

	//
	// AdapterStatus
	//
	ULONG GetAdapterStatus();
	ULONG SetAdapterStatus(ULONG ulAdapterStatus);

	//
	// Set the unit device ID at a sequence 
	// to a unit device member ID list
	//
	BOOL AddUnitDevice(CNdasUnitDevicePtr pUnitDevice);

	//
	// Remove the unit device ID from the list
	//
	BOOL RemoveUnitDevice(CNdasUnitDevicePtr pUnitDevice);

	//
	// Get the unit device instance count
	//
	DWORD GetUnitDeviceInstanceCount();

	NDAS_LOCATION GetNdasLocation();
	NDAS_LOGICALUNIT_ADDRESS GetNdasLogicalUnitAddress();
	VOID SetNdasPortExistence(const BOOL NdasPortExistence);

	//
	// Plug-in the logical device to the LANSCSI Bus and
	// make it available to the OS as a logical (storage) device
	//
	HRESULT PlugIn(
		ACCESS_MASK requestingAccess, 
		DWORD LdpfFlags, 
		DWORD LdpfValues);

	HRESULT PlugInNdasBus(
		ACCESS_MASK requestingAccess, 
		DWORD LdpfFlags, 
		DWORD LdpfValues);

	HRESULT PlugInNdasPort(
		ACCESS_MASK requestingAccess, 
		DWORD LdpfFlags, 
		DWORD LdpfValues);

	//
	// Eject the logical device from the LANSCSI Bus,
	// which will be an action of ejecting a device from the OS
	//
	HRESULT Eject();

	HRESULT EjectEx(
		CONFIGRET* pConfigRet, 
		PPNP_VETO_TYPE pVetoType, 
		LPTSTR pszVetoName,
		DWORD nNameLength);
	//
	// Unplug is a similar to the eject operation.
	// However, this action makes a device removed from the OS
	// without giving a chance to applications to 
	HRESULT Unplug();

	CNdasUnitDevicePtr GetPrimaryUnit();
	
	//
	// Get the combined access mask of the all member unit devices
	// based on the registration information in the device registrar
	//
	ACCESS_MASK GetGrantedAccess();

	//
	// Get the combined access mask of the all member unit devices
	// which is allowed at the time of calling this function.
	// An allowing access is a runtime access mask
	// whereas a granted access is configured access mask.
	//
	ACCESS_MASK GetAllowingAccess();

	//
	// Get the mounted access mask of the logical device 
	//
	ACCESS_MASK GetMountedAccess();

	//
	// Get drive letters of volumes in the logical device
	// 
	DWORD GetMountedDriveSet();

	//
	// Get the status of the logical device
	//
	NDAS_LOGICALDEVICE_STATUS GetStatus();

	//
	// Get the last logical device error
	//
	NDAS_LOGICALDEVICE_ERROR GetLastError();

	//
	// Get an event handle for disconnection
	//
	HANDLE GetDisconnectEvent() const;

	//
	// Get an event handle for alarm
	//
	HANDLE GetAlarmEvent() const;

	//
	// Get the minimum Max Request Block size
	// for whole members.
	//
	DWORD GetCurrentMaxRequestBlocks();

	//
	// Total available block count
	//
	UINT64 GetUserBlockCount();

	//
	// Shared Write as Primary Host
	//
	BOOL GetSharedWriteInfo(
		LPBOOL lpbSharedWriteFeature, 
		LPBOOL lpbPrimary);

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
	void SetMountOnReady(
		ACCESS_MASK access, 
		BOOL fReducedMountOnReadyAccess = FALSE);

	UINT32 GetBACLSize(int nBACLSkipped) const;

	//
	// Get Risky Mount Flag
	//
	BOOL _IsRiskyMount();

	//
	// Update RiskyMountFlag from the registry
	// (Do not use this to check risky status, use IsRiskyMount())
	//
	BOOL GetRiskyMountFlag();

	DWORD GetRaidSlotNo();
	
	//
	// Set Fault after NDASSCSI_ADAPTER_STATUSFLAG_MEMBER_FAULT
	//
	void SetAllUnitDevicesFault();
	bool IsAnyUnitDevicesFault();

	//
	// Clear Risky Mount Flag
	// (Called from NDSAEVENTMON)
	// 
	void SetRiskyMountFlag(BOOL fRisky);

	//
	// Set the mounted mask to the registry
	//
	void SetLastMountAccess(ACCESS_MASK mountedAccess);

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

	//
	// Update RAID mountablility information
	// 
	BOOL _RefreshBindStatus();

	//
	// Update m_fMountable, m_fDegradedMode, m_RaidFailReason
	// Return TRUE if mountable state has changed and notification is required.
	//
	
	BOOL UpdateBindStateAndError();
	
	const NDAS_CONTENT_ENCRYPT* GetContentEncrypt();
	BOOL GetContentEncrypt(NDAS_CONTENT_ENCRYPT* pce);

	///////////////////////////////////////////////////////////////////////
	//
	// Event Handlers
	//
	///////////////////////////////////////////////////////////////////////

	void OnPeriodicCheckup();

	//
	// Service handler call is when the system is being shutdown
	//
	void OnShutdown();

	//
	// Logical device is disconnected
	//
	void OnDisconnected();

	//
	// PNP Event Handler calls this on mounted
	//
	void OnMounted(LPCTSTR DevicePath);

	//
	// PNP Event Handler calls this on unmount process is completed
	//
	void OnUnmounted();

	//
	// PNP Event Handler calls this when the unmount process is failed.
	//
	void OnUnmountFailed();

protected:

	BOOL _CheckPlugInCondition(ACCESS_MASK requestingAccess);
	
//	DWORD _GetMaxRequestBlocks();

	void _AllocateNdasLocation();
	void _DeallocateNdasLocation();


	//
	// Is this logical device is complete?
	//
	BOOL IsComplete();

	//
	// Is this logical device is mountable?
	//
	BOOL IsMountable();

	//
	// Set the mounted access of the logical device
	// (Called from the PNP Handler)
	//
	void SetMountedAccess(ACCESS_MASK mountedAccess);

	//
	// Set the mounted drive letters
	// (Called from the PNP Handler)
	//
	void SetMountedDriveSet(DWORD dwDriveSet);

	//
	// Set the current logical device status
	//
	void _SetStatus(NDAS_LOGICALDEVICE_STATUS newStatus);

	//
	// Set the last device error
	//
	void _SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR logDevError);
	
	//
	// Calculate the hash value of this object based on
	// the NDAS_LOGICALDEVICE_GROUP value
	//
	DWORD _GetHashValue();

	//
	// Locate the reg container based on the hash value
	// The actual container may different to the hash value
	// due to the collision
	//
	void _LocateRegContainer();

	BOOL _IsPSWriteShareCapable();

	HRESULT _IsWriteAccessAllowed(BOOL fPSWriteShare, CNdasUnitDevicePtr pUnitDevice);

	BOOL ReconcileWithNdasBus();
	BOOL ReconcileWithNdasPort();

	BOOL GetLastLdpf(DWORD& flags, DWORD& values);
	BOOL SetLastLdpf(DWORD flags, DWORD values);
	BOOL ClearLastLdpf();

	BOOL _IsUpgradeRequired();

	BOOL _IsRaid();
};

#endif
