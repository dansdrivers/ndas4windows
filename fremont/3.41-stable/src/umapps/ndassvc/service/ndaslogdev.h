/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASLOGDEV_H_
#define _NDASLOGDEV_H_

#include <xtl/xtlautores.h>
#include "ndascomobjectsimpl.hpp"

/*++

NDAS Logical Device class

--*/

class CNdasLogicalUnit :
	public CComObjectRootEx<CComMultiThreadModel>,
	public ILockImpl<INdasLogicalUnit>,
	public INdasLogicalUnitPnpSink,
	public INdasTimerEventSink
{
	typedef CAutoLock<CNdasLogicalUnit> CAutoLock;

public:

	BEGIN_COM_MAP(CNdasLogicalUnit)
		COM_INTERFACE_ENTRY(INdasLogicalUnit)
		COM_INTERFACE_ENTRY(INdasLogicalUnitPnpSink)
		COM_INTERFACE_ENTRY(INdasTimerEventSink)
	END_COM_MAP()

protected:

	//
	// Logical Device ID
	//
	NDAS_LOGICALDEVICE_ID m_NdasLogicalUnitId;

	//
	// Logical device group information
	//
	NDAS_LOGICALUNIT_DEFINITION m_NdasLogicalUnitDefinition;

	CAtlArray<INdasUnit*> m_NdasUnits;

	CComBSTR m_SystemDevicePath;

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
	CComBSTR m_RegistrySubPath;

	// Autonomous mount flag
	//
	ACCESS_MASK m_mountOnReadyAccess;
	BOOL m_fReducedMountOnReadyAccess;
	BOOL m_fMountOnReady;

	BOOL m_fRiskyMount;

	DWORD m_dwMountTick;

	//
	// System shutdown flag (retain mount information)
	//
	DWORD m_fShutdown;

	NDAS_RAID_FAIL_REASON m_RaidFailReason;
	
	DWORD m_Abnormalities;

	BOOL m_PendingReconcilation;

public:

	//
	// Constructor for a multiple member logical device
	//
	CNdasLogicalUnit();

	HRESULT Initialize(
		__in NDAS_LOGICALDEVICE_ID LogicalUnitId, 
		__in const NDAS_LOGICALUNIT_DEFINITION& LogicalUnitDef,
		__in BSTR RegistrySubPath);

	void FinalRelease();
	
	STDMETHODIMP get_Id(__out NDAS_LOGICALDEVICE_ID* Id);
	STDMETHODIMP get_Type(__out NDAS_LOGICALDEVICE_TYPE* Type);

	STDMETHODIMP get_LogicalUnitDefinition(__out NDAS_LOGICALUNIT_DEFINITION* LogicalUnitDefinition);
	STDMETHODIMP get_LogicalUnitAddress(__out NDAS_LOGICALUNIT_ADDRESS* LogicalUnitAddress);

	STDMETHODIMP get_Status(__out NDAS_LOGICALDEVICE_STATUS * Status);
	STDMETHODIMP get_Error(__out NDAS_LOGICALDEVICE_ERROR * Error);
	STDMETHODIMP get_IsHidden(__out BOOL * Hidden);

	//
	// Set the unit device ID at a sequence 
	// to a unit device member ID list
	//
	STDMETHODIMP AddNdasUnitInstance(INdasUnit* pNdasUnit);

	//
	// Remove the unit device ID from the list
	//
	STDMETHODIMP RemoveNdasUnitInstance(INdasUnit* pNdasUnit);

	//
	// Plug-in the logical device to the LANSCSI Bus and
	// make it available to the OS as a logical (storage) device
	//
	STDMETHODIMP PlugIn(
		ACCESS_MASK requestingAccess, 
		DWORD LdpfFlags, 
		DWORD LdpfValues,
		DWORD PlugInFlags);


	//
	// Eject the logical device from the LANSCSI Bus,
	// which will be an action of ejecting a device from the OS
	//
	STDMETHODIMP Eject();

	STDMETHODIMP EjectEx(
		CONFIGRET* pConfigRet, 
		PPNP_VETO_TYPE pVetoType, 
		LPTSTR pszVetoName,
		DWORD nNameLength);

	STDMETHODIMP EjectEx(
		__out CONFIGRET* ConfigRet, 
		__out PNP_VETO_TYPE* VetoType,
		__out BSTR* VetoName);

	//
	// Unplug is a similar to the eject operation.
	// However, this action makes a device removed from the OS
	// without giving a chance to applications to 
	STDMETHODIMP Unplug();
	
	//
	// Get the combined access mask of the all member unit devices
	// based on the registration information in the device registrar
	//
	STDMETHODIMP get_GrantedAccess(__out ACCESS_MASK* GrantedAccess);

	//
	// Get the combined access mask of the all member unit devices
	// which is allowed at the time of calling this function.
	// An allowing access is a runtime access mask
	// whereas a granted access is configured access mask.
	//
	STDMETHODIMP get_AllowedAccess(__out ACCESS_MASK* GrantedAccess);

	//
	// Get the mounted access mask of the logical device 
	//
	STDMETHODIMP get_MountedAccess(__out ACCESS_MASK * Access);

	//
	// Get drive letters of volumes in the logical device
	// 
	STDMETHODIMP get_MountedDriveSet(__out DWORD * LogicalDrives);

	STDMETHODIMP get_UserBlocks(__out UINT64* Blocks);
	STDMETHODIMP get_DevicePath(__deref_out BSTR* DevicePath);
	STDMETHODIMP get_AdapterStatus(__out ULONG * AdapterStatus);
	STDMETHODIMP put_AdapterStatus(__out ULONG AdapterStatus);
	STDMETHODIMP get_DisconnectEvent(__out HANDLE* EventHandle);
	STDMETHODIMP get_AlarmEvent(__out HANDLE* EventHandle);
	STDMETHODIMP get_NdasUnitInstanceCount(__out DWORD* UnitCount);
	STDMETHODIMP get_NdasUnitCount(__out DWORD* UnitCount);
	STDMETHODIMP get_NdasUnitId(__in DWORD Sequence, __out NDAS_UNITDEVICE_ID* NdasUnitId);

	//
	// Shared Write as Primary Host
	//
	STDMETHODIMP GetSharedWriteInfo(__out BOOL * SharedWrite, __out BOOL * PrimaryHost);

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
	STDMETHODIMP SetMountOnReady(
		__in ACCESS_MASK access, 
		__in BOOL fReducedMountOnReadyAccess /*= FALSE */);

	//
	// Clear Risky Mount Flag
	// (Called from NDSAEVENTMON)
	// 
	STDMETHODIMP SetRiskyMountFlag(__in BOOL RiskyState);

	STDMETHODIMP get_ContentEncryption(__out NDAS_CONTENT_ENCRYPT* Encryption);

	STDMETHODIMP get_Abnormalities(__out DWORD * Abnormalities);

	///////////////////////////////////////////////////////////////////////
	//
	// Event Handlers
	//
	///////////////////////////////////////////////////////////////////////

	STDMETHODIMP_(void) OnTimer();

	//
	// INdasLogicalUnitPnpSink implementation
	//

	//
	// Service handler call is when the system is being shutdown
	//
	STDMETHODIMP_(void) OnSystemShutdown();

	//
	// Logical device is disconnected
	//
	STDMETHODIMP_(void) OnDisconnected();

	//
	// PNP Event Handler calls this on mounted
	//
	STDMETHODIMP_(void) OnMounted(BSTR DevicePath, NDAS_LOGICALUNIT_ABNORMALITIES Abnormalities);

	//
	// PNP Event Handler calls this on unmount process is completed
	//
	STDMETHODIMP_(void) OnDismounted();

	//
	// PNP Event Handler calls this when the unmount process is failed.
	//
	STDMETHODIMP_(void) OnDismountFailed();

	//
	// Set the mounted drive letters
	// (Called from the PNP Handler)
	//
	// void SetMountedDriveSet(DWORD DriveSet, DWORD DriveSetMask);
	STDMETHODIMP_(void) OnMountedDriveSetChanged(DWORD RemoveBits, DWORD AddBits);

protected:

	//
	// Get the type of the logical device
	//
	const NDAS_LOGICALUNIT_DEFINITION& pGetLogicalUnitConfig() const;

	HRESULT pGetMemberNdasUnit(DWORD Seq, INdasUnit** ppNdasUnit);
	HRESULT pGetPrimaryNdasUnit(INdasUnit** ppNdasUnit);

	HRESULT pPlugInNdasBus(
		ACCESS_MASK requestingAccess, 
		DWORD LdpfFlags, 
		DWORD LdpfValues);

	HRESULT pPlugInNdasPort(
		ACCESS_MASK requestingAccess, 
		DWORD LdpfFlags, 
		DWORD LdpfValues);

	BOOL pIsVolatile();

	NDAS_LOGICALUNIT_ADDRESS pGetNdasLogicalUnitAddress();

	DWORD pGetUnitDeviceCount() const;
	DWORD pGetUnitDeviceCountSpare() const;
	DWORD pGetUnitDeviceCountInRaid() const;

	//
	// Invalidate all unit disks and logical device information.
	//
	void pInvalidateNdasUnits();

	//
	// Update RiskyMountFlag from the registry
	// (Do not use this to check risky status, use pIsRiskyMount())
	//
	BOOL pGetRiskyMountFlag();

	//
	// Get Risky Mount Flag
	//
	BOOL pIsRiskyMount();

	//
	// Set the mounted mask to the registry
	//
	void pSetLastMountAccess(ACCESS_MASK mountedAccess);

	//
	// Previous mounted mask
	//
	ACCESS_MASK pGetLastMountAccess();

	//
	// Get the mounted tick
	// 0 if no mount tick is available
	//
	DWORD pGetMountTick();

#if 0
	//
	// Update m_fMountable, m_fDegradedMode, m_RaidFailReason
	// Return TRUE if mountable state has changed and notification is required.
	//
	BOOL pUpdateBindStateAndError();

	//
	// Update RAID mountable information
	// 
	BOOL pRefreshBindStatus();
#endif

	HRESULT pIsSafeToPlugIn(ACCESS_MASK requestingAccess);	

	//
	// Is this logical device is complete?
	//
	BOOL pIsComplete();

	//
	// Set the mounted access of the logical device
	// (Called from the PNP Handler)
	//
	void pSetMountedAccess(ACCESS_MASK mountedAccess);

	//
	// Set the current logical device status
	//
	void pSetStatus(NDAS_LOGICALDEVICE_STATUS newStatus);

	//
	// Set the last device error
	//
	void pSetLastDeviceError(NDAS_LOGICALDEVICE_ERROR logDevError);
	
	BOOL pIsSharedWriteModeCapable();

	HRESULT pIsWriteAccessAllowed(BOOL fPSWriteShare, INdasUnit* pNdasUnit);

	HRESULT pReconcileWithNdasBus();
	HRESULT pReconcileWithNdasPort();

	HRESULT pUpdateSystemDeviceInformation();

	BOOL pGetLastLogicalUnitPlugInFlags(DWORD& flags, DWORD& values);
	BOOL pSetLastLogicalUnitPlugInFlags(DWORD flags, DWORD values);
	BOOL pClearLastLogicalUnitPlugInFlags();

	HRESULT pGetLastPlugInFlags(DWORD &Flags);
	HRESULT pSetLastPlugInFlags(DWORD Flags);
	HRESULT pClearLastPlugInFlags();

	BOOL pIsUpgradeRequired();

	BOOL pIsRaid();

	void pGetNdasUnitInstances(CInterfaceArray<INdasUnit>& NdasUnits);
	BOOL pIsEmergencyMode();
	HRESULT pCheckRaidStatus(DWORD PlugInFlags);
};

#endif
