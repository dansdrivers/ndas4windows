/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "ndasinstman.h"
#include "ndasdevreg.h"
#include "ndasdev.h"
#include "ndaslogdev.h"
#include "ndaseventmon.h"
#include "ndaseventpub.h"

#include "lsbusctl.h"
#include "lpxcomm.h"

#include "ndaserror.h"
#include "ndastype_str.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_NDASLOGDEV
#include "xdebug.h"

//
// Constructor for a multiple member logical device
//
CNdasLogicalDevice::
CNdasLogicalDevice(
	DWORD dwSlot, 
	NDAS_LOGICALDEVICE_TYPE type,
	NDAS_UNITDEVICE_ID primaryUnitDeviceId,
	DWORD nUnitDevices) :

	m_dwSlot(dwSlot),
	m_devType(type),
	m_primaryUnitDeviceId(primaryUnitDeviceId),
	m_nUnitDeviceInstances(0),
	m_nUnitDevices(nUnitDevices),
	m_pUnitDeviceIds(NULL),
	m_status(NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED),
	m_lastError(NDAS_LOGICALDEVICE_ERROR_NONE),
	m_dwMountedDriveSet(0),
	m_MountedAccess(0),
	m_hAlarmEvent(NULL),
	m_hDisconnectedEvent(NULL)
{
	_ASSERTE(m_nUnitDevices > 0);
	_ASSERTE(m_nUnitDevices <= MAX_UNITDEVICE_ENTRY);

	m_pUnitDeviceIds = new NDAS_UNITDEVICE_ID[nUnitDevices];

	if (NULL == m_pUnitDeviceIds) {
		throw("Memory allocation failure.");
	}

	//
	// initially contains NDAS_UNITDEVICE_ID_NULL
	//
	::ZeroMemory(
		m_pUnitDeviceIds, 
		sizeof(NDAS_UNITDEVICE_ID) * nUnitDevices);
}

//
// Destructor
//
CNdasLogicalDevice::
~CNdasLogicalDevice()
{
	_ASSERTE(m_nUnitDeviceInstances == 0);

	if (NULL != m_pUnitDeviceIds) {
		delete [] m_pUnitDeviceIds;
	}

	if (NULL != m_hDisconnectedEvent) {
		::CloseHandle(m_hDisconnectedEvent);
	}

	if (NULL != m_hAlarmEvent) {
		::CloseHandle(m_hAlarmEvent);
	}
}

//
// Set the unit device ID at a sequence 
// to a unit device member ID list
//
BOOL 
CNdasLogicalDevice::
AddUnitDeviceId(
	DWORD dwSequence, 
	NDAS_UNITDEVICE_ID unitDeviceId)
{
	_ASSERTE(dwSequence >= 0 && dwSequence < m_nUnitDevices);
	_ASSERTE(
		(dwSequence == 0 && unitDeviceId == m_primaryUnitDeviceId) ||
		dwSequence > 0);
	_ASSERTE(!IsNullNdasUnitDeviceId(unitDeviceId));

	if (!IsNullNdasUnitDeviceId(m_pUnitDeviceIds[dwSequence])) {
		//
		// Existing values cannot be changed at this time
		//
		_ASSERTE(FALSE && "Existing entry is not-mutable!");
		return FALSE;
	}

	m_pUnitDeviceIds[dwSequence] = unitDeviceId;
	++m_nUnitDeviceInstances;

	_ASSERTE(m_nUnitDeviceInstances >= 0 && 
		m_nUnitDeviceInstances <= MAX_UNITDEVICE_ENTRY);

	return TRUE;
}

//
// Remove the unit device ID from the list
//
BOOL
CNdasLogicalDevice::
RemoveUnitDeviceId(
   NDAS_UNITDEVICE_ID unitDeviceId)
{
	for (DWORD i = 0; i < m_nUnitDevices; ++i) {
		if (m_pUnitDeviceIds[i] == unitDeviceId) {
			m_pUnitDeviceIds[i] = NullNdasUnitDeviceId();
			--m_nUnitDeviceInstances;
			_ASSERTE(m_nUnitDeviceInstances >= 0 && 
				m_nUnitDeviceInstances <= MAX_UNITDEVICE_ENTRY);
			return TRUE;
		}
	}
	return FALSE;
}

//
// Get the unit device instance count
//
DWORD 
CNdasLogicalDevice::
GetUnitDeviceInstanceCount()
{
	return m_nUnitDeviceInstances;
}

//
// Get the defined maximum number of unit devices
//
DWORD 
CNdasLogicalDevice::
GetUnitDeviceCount()
{
	return m_nUnitDevices;
}

//
// Get the unit device ID in i-th sequence.
// 0 means the primary unit device ID.
//
NDAS_UNITDEVICE_ID 
CNdasLogicalDevice::
GetUnitDeviceId(DWORD dwSequence)
{
	_ASSERTE(dwSequence < m_nUnitDevices);
	return m_pUnitDeviceIds[dwSequence];
}

NDAS_LOGICALDEVICE_TYPE
CNdasLogicalDevice::
GetType()
{
	return m_devType;
}

DWORD
CNdasLogicalDevice::
GetSlot()
{
	return m_dwSlot;
}

LPCTSTR CNdasLogicalDevice::ToString()
{
	LPTSTR pszNext = m_szStringRep;
	size_t cchRemaining = MAX_STRING_REP;

	HRESULT hr = ::StringCchPrintfEx(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
		TEXT("[%d]: %s(%d) %s ["),
		m_dwSlot,
		NdasLogicalDeviceTypeString(m_devType),
		m_nUnitDevices,
		CNdasUnitDeviceId(m_primaryUnitDeviceId).ToString());
	
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	if (STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		return m_szStringRep;
	}

	for (DWORD i = 0; i < m_nUnitDevices; ++i) {
		if (IsNullNdasUnitDeviceId(m_pUnitDeviceIds[i])) {
			continue;
		}
		hr = ::StringCchPrintfEx(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
			TEXT("(%d, %s) "),
			i,
			CNdasUnitDeviceId(m_pUnitDeviceIds[i]).ToString());

		_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

		if (STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
			return m_szStringRep;
		}
	}

	hr = ::StringCchPrintfEx(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
		TEXT("]"));
	
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	return m_szStringRep;
}

static BOOL CheckStatusValidity(
	  NDAS_LOGICALDEVICE_STATUS oldStatus,
	  NDAS_LOGICALDEVICE_STATUS newStatus)
{
	switch (oldStatus) {
	case NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED:
		switch (newStatus) {
		case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
			return TRUE;
		}
	case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
		switch (newStatus) {
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
			return TRUE;
		}
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		switch (newStatus) {
		case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
		case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
			return TRUE;
		}
	case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		switch (newStatus) {
		case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
			return TRUE;
		}
	case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
		switch (newStatus) {
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
			return TRUE;
		}
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
		switch (newStatus) {
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
			return TRUE;
		}
	}
	return FALSE;
}

VOID 
CNdasLogicalDevice::
SetStatus(NDAS_LOGICALDEVICE_STATUS newStatus)
{
	if (m_status == newStatus) {
		return;
	}

	NDAS_LOGICALDEVICE_STATUS oldStatus = m_status;

	BOOL fValid = CheckStatusValidity(m_status, newStatus);
	_ASSERTE(fValid);

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasEventMonitor* pEventMon = pInstMan->GetEventMonitor();
	_ASSERTE(NULL != pEventMon);

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == m_status&&
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == newStatus)
	{
		pEventMon->Attach(this);
	} else if (
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == m_status &&
		NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == newStatus)
	{
		pEventMon->Detach(this);
	}

	m_status = newStatus;

	//
	// Update Unit Device Status
	//

	PCNdasDeviceRegistrar pRegistrar = pInstMan->GetRegistrar();

	for (DWORD i = 0; i < m_nUnitDevices; ++i) {

		PCNdasDevice pDevice = pRegistrar->Find(m_pUnitDeviceIds[i].DeviceId);

		if (NULL == pDevice) {
			DPError(_FT("Parent device of Unit Device %s is not found.\n"), 
				CNdasUnitDeviceId(m_pUnitDeviceIds[i]).ToString());
			continue;
		}

		PCNdasUnitDevice pUnitDevice = 
			pDevice->GetUnitDevice(m_pUnitDeviceIds[i].UnitNo);

		if (NULL == pUnitDevice) {
			DPError(_FT("Unit Device %s is not found.\n"),
				CNdasUnitDeviceId(m_pUnitDeviceIds[i]).ToString());
			continue;
		}

		//
		// TODO: Create a status updater with observer
		//
		switch (m_status) {
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
			pUnitDevice->SetStatus(NDAS_UNITDEVICE_STATUS_MOUNTED);
			break;
		default:
			// otherwise
			pUnitDevice->SetStatus(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED);
		}
	}

	
	CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
	_ASSERTE(NULL != pEventPublisher);

	NDAS_LOGICALDEVICE_ID logicalDeviceId = { m_dwSlot, 0, 0 };
	(void) pEventPublisher->LogicalDeviceStatusChanged(
		logicalDeviceId,
		oldStatus,
		newStatus);

}

VOID 
CNdasLogicalDevice::
SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR logDevError)
{
	m_lastError = logDevError;
}

ACCESS_MASK 
CNdasLogicalDevice::
GetMountedAccess()
{ 
	return m_MountedAccess; 
}

VOID 
CNdasLogicalDevice::
SetMountedAccess(ACCESS_MASK mountedAccess)
{ 
	m_MountedAccess = mountedAccess; 
}

ACCESS_MASK 
CNdasLogicalDevice::
GetGrantedAccess()
{
	ACCESS_MASK access(0xFFFFFFFFL);

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	PCNdasDeviceRegistrar pRegistrar = 	pInstMan->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	//
	// Any single missing entry will revoke all accesses
	//
	for (DWORD i = 0; i < m_nUnitDevices; ++i) {

		PCNdasDevice pDevice = pRegistrar->Find(m_pUnitDeviceIds[i].DeviceId);

		if (NULL == pDevice) {
			return 0x00000000L;
		}

		PCNdasUnitDevice pUnitDevice = 
			pDevice->GetUnitDevice(m_pUnitDeviceIds[i].UnitNo);

		if (NULL == pUnitDevice) {
			return 0x00000000L;
		}

		access &= pUnitDevice->GetGrantedAccess();
	}

	return access;
}

ACCESS_MASK 
CNdasLogicalDevice::
GetAllowingAccess()
{
	ACCESS_MASK access(0xFFFFFFFFL);

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	PCNdasDeviceRegistrar pRegistrar = 	pInstMan->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	//
	// Any single missing entry will revoke all accesses
	//

	for (DWORD i = 0; i < m_nUnitDevices; ++i) {

		PCNdasDevice pDevice = pRegistrar->Find(m_pUnitDeviceIds[i].DeviceId);

		if (NULL == pDevice) {
			return 0x00000000L;
		}

		PCNdasUnitDevice pUnitDevice = 
			pDevice->GetUnitDevice(m_pUnitDeviceIds[i].UnitNo);

		if (NULL == pUnitDevice) {
			return 0x00000000L;
		}

		access &= pUnitDevice->GetAllowingAccess();
	}

	return access;
}

DWORD 
CNdasLogicalDevice::
GetMountedDriveSet()
{ 
	return m_dwMountedDriveSet; 
}

VOID 
CNdasLogicalDevice::
SetMountedDriveSet(DWORD dwDriveSet)
{ 
	m_dwMountedDriveSet = dwDriveSet; 
}

NDAS_LOGICALDEVICE_ERROR
CNdasLogicalDevice::
GetLastError()
{
	return m_lastError;
}

HANDLE
CNdasLogicalDevice::
GetDisconnectEvent()
{
	return m_hDisconnectedEvent;
}

HANDLE
CNdasLogicalDevice::
GetAlarmEvent()
{
	return m_hAlarmEvent;
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasLogicalDisk Implementation
//
//////////////////////////////////////////////////////////////////////////

CNdasLogicalDisk::
CNdasLogicalDisk(
	DWORD dwSlot,
	NDAS_LOGICALDEVICE_TYPE Type,
	NDAS_UNITDEVICE_ID unitDeviceId) :
	CNdasLogicalDevice(dwSlot, Type, unitDeviceId)
{
}

CNdasLogicalDisk::
CNdasLogicalDisk(
	DWORD dwSlot,
	NDAS_LOGICALDEVICE_TYPE Type,
	NDAS_UNITDEVICE_ID primaryUnitDeviceId,
	DWORD dwUnitDevices) :
	CNdasLogicalDevice(dwSlot, Type, primaryUnitDeviceId, dwUnitDevices)
{
}

CNdasLogicalDisk::
~CNdasLogicalDisk()
{
	if (NULL != m_hAlarmEvent) {
		BOOL fSuccess = ::CloseHandle(m_hAlarmEvent);
		_ASSERT(fSuccess);
	}
	if (NULL != m_hDisconnectedEvent) {
		BOOL fSuccess = ::CloseHandle(m_hDisconnectedEvent);
		_ASSERT(fSuccess);
	}
}

BOOL
CNdasLogicalDisk::
Initialize()
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED != m_status) {
		// Already initialized
		return FALSE;
	}

	if (NULL == m_hDisconnectedEvent) {
		m_hDisconnectedEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hDisconnectedEvent) {
			DPErrorEx(_FT("Disconnect event creation failed: "));
			return FALSE;
		}
	}

	if (NULL == m_hAlarmEvent) {
		m_hAlarmEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hAlarmEvent) {
			DPErrorEx(_FT("Alarm event creation failed: "));
			::CloseHandle(m_hDisconnectedEvent);
			return FALSE;
		}
	}

	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	DPInfo(_FT("Logical Disk initialized successfully at slot %d.\n"), m_dwSlot);

	return TRUE;
}

BOOL
CNdasLogicalDisk::
PlugIn(ACCESS_MASK requestingAccess)
{
	ximeta::CAutoLock autolock(this);

	BOOL fSuccess(FALSE);

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	//
	// only from NOT_MOUNTED_STATUS
	//
	if (m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNTED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	if (m_nUnitDeviceInstances != m_nUnitDevices) {
		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	}

	//
	// check access permission
	//

	//
	// only GENERIC_READ and GENERIC_WRITE is allowed
	//
	requestingAccess &= (GENERIC_READ | GENERIC_WRITE);

	ACCESS_MASK grantedAccess = GetGrantedAccess();
	if ((grantedAccess & requestingAccess) != requestingAccess) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED);
		return FALSE;
	}

	ACCESS_MASK allowingAccess = GetAllowingAccess();
	if ((requestingAccess & allowingAccess) != requestingAccess) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_FAILED);
		return FALSE;
	}

	//
	// Plug In
	// - NDAS Controller
	//
	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	PCNdasDeviceRegistrar pRegistrar = pInstMan->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	PCNdasDevice pDevice = pRegistrar->Find(m_pUnitDeviceIds[0].DeviceId);

	if (NULL == pDevice) {
		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	}

	//
	// Resetting an event always succeeds if the handle is valid
	//
	fSuccess = ::ResetEvent(m_hDisconnectedEvent);
	_ASSERTE(fSuccess);

	fSuccess = ::ResetEvent(m_hAlarmEvent);
	_ASSERTE(fSuccess);

	//
	// Add Target
	// - Add a disk device to the NDAS controller
	//

	SIZE_T cbAddTargetDataSize =  
		sizeof(LANSCSI_ADD_TARGET_DATA) - sizeof(LSBUS_UNITDISK) +
		m_nUnitDevices * sizeof(LSBUS_UNITDISK);

	PLANSCSI_ADD_TARGET_DATA pAddTargetData = 
		(PLANSCSI_ADD_TARGET_DATA) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbAddTargetDataSize);

	if (NULL == pAddTargetData) {
		// TODO: Out of memory
		LsBusCtlEject(m_dwSlot);
		return FALSE;
	}

	UCHAR ucTargetType;

	switch (m_devType) {
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		ucTargetType = DISK_TYPE_NORMAL;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		ucTargetType = DISK_TYPE_MIRROR;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		ucTargetType = DISK_TYPE_AGGREGATION;
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID_1:
		ucTargetType = DISK_TYPE_BIND_RAID1;
		break;
	default:
		_ASSERTE(FALSE);
		// Non Disk Type should not be here!
		return FALSE;
	}

	pAddTargetData->ulSize = cbAddTargetDataSize;
	pAddTargetData->ulSlotNo = m_dwSlot;
	pAddTargetData->ulTargetBlocks = 0; // initialization and will be added
	pAddTargetData->DesiredAccess = requestingAccess;
	pAddTargetData->hEvent; // TODO: Add this!
	pAddTargetData->ulNumberOfUnitDiskList = m_nUnitDevices;
	pAddTargetData->ucTargetType = ucTargetType;

	//
	// primary unit device
	//

	for (DWORD i = 0; i < m_nUnitDevices; ++i) {

		NDAS_UNITDEVICE_ID UnitDeviceId = m_pUnitDeviceIds[i];

		PCNdasDevice pDevice = pRegistrar->Find(UnitDeviceId.DeviceId);
		if (NULL == pDevice) {
			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
			::HeapFree(::GetProcessHeap(), 0, pAddTargetData);
			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
			return FALSE;
		}

		PCNdasUnitDevice pUnitDevice = pDevice->GetUnitDevice(UnitDeviceId.UnitNo);
		if (NULL == pUnitDevice) {
			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
			::HeapFree(::GetProcessHeap(), 0, pAddTargetData);
			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
			return FALSE;
		}

		if (NDAS_UNITDEVICE_TYPE_DISK != pUnitDevice->GetType()) {
			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_INVALID_MEMBER);
			::HeapFree(::GetProcessHeap(), 0, pAddTargetData);
			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
			return FALSE;
		}

		PCNdasUnitDiskDevice pUnitDiskDevice = 
			reinterpret_cast<PCNdasUnitDiskDevice>(pUnitDevice);

		PLSBUS_UNITDISK pud = &pAddTargetData->UnitDiskList[i];

		::CopyMemory(
			pud->Address.Node, 
			pDevice->GetRemoteLpxAddress().Node, 
			sizeof(pud->Address.Node));
		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);

		::CopyMemory(
			pud->NICAddr.Node, 
			pDevice->GetLocalLpxAddress().Node, 
			sizeof(pud->NICAddr.Node));
		pud->NICAddr.Port = htons(0); // should be zero

		pud->iUserID = (requestingAccess & GENERIC_WRITE) ? 
			READ_WRITE_USER_ID : READ_ONLY_USER_ID;
		pud->iPassword = pDevice->GetHWPassword();

		// pud->ulUnitBlocks = pUnitDiskDevice->GetBlocks();
		pud->ulUnitBlocks = pUnitDiskDevice->GetUserBlockCount();
		pud->ulPhysicalBlocks = pUnitDiskDevice->GetPhysicalBlockCount();
		pud->ucUnitNumber = UnitDeviceId.UnitNo;
		pud->ucHWType = pDevice->GetHWType();
		pud->ucHWVersion = pDevice->GetHWVersion();
		// pAddTargetData->ulTargetBlocks += pud->ulUnitBlocks;
		pud->IsWANConnection = FALSE;

		// ulTargetBlocks & additional disk information
		switch(ucTargetType)
		{
		case DISK_TYPE_NORMAL:
			break;
		case DISK_TYPE_MIRROR: // count only master's
			break;
		case DISK_TYPE_AGGREGATION:
			break;
		case DISK_TYPE_BIND_RAID1:
			_ASSERT(NULL != pUnitDiskDevice->GetAddTargetInfo());
			::CopyMemory(&pud->RAID_1, pUnitDiskDevice->GetAddTargetInfo(), sizeof(INFO_RAID_1));
			break;
		default:
			break;
		}
	}

	pAddTargetData->ulTargetBlocks = GetUserBlockCount();

	DWORD dwMaxRequestBlocks = GetMaxRequestBlocks();

	DPInfo(_FT("LsBusCtlPluginEx(SlotNo %d, MaxReqBlock %d, DisEvt %p, RecEvt %p).)\n"),
		m_dwSlot, dwMaxRequestBlocks, m_hDisconnectedEvent, m_hAlarmEvent);

	fSuccess = LsBusCtlPlugInEx(
		m_dwSlot,
		dwMaxRequestBlocks,
		m_hDisconnectedEvent,
		m_hAlarmEvent
		);

	if (!fSuccess) {
		DPErrorEx(_FT("LsBusCtlPluginEx failed - Error 0x%08x\n"));
		return FALSE;
	}

	DPInfo(_FT("LsBusCtlPluginEx succeeded.\n"));

	DPInfo(_FT("LsBusCtlAddTarget(pAddTargetData)\n"));
	DPType(XDebug::OL_INFO, pAddTargetData, cbAddTargetDataSize);

	fSuccess = LsBusCtlAddTarget(pAddTargetData);
	if (!fSuccess) {
		DPErrorEx(_FT("AddTarget failed.\n"));
		::HeapFree(::GetProcessHeap(), 0, pAddTargetData);

		::Sleep(1000);
		LsBusCtlEject(m_dwSlot);
		return FALSE;
	}

	DPInfo(_FT("LsBusCtlAddTarget succeeded.\n"));

	SetMountedAccess(requestingAccess);
	SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

	::HeapFree(::GetProcessHeap(), 0, pAddTargetData);

	DPInfo(_FT("Plugged in successfully at slot %d (Disk).\n"), m_dwSlot);

	return TRUE;
}

BOOL
CNdasLogicalDisk::
Unplug()
{
	BOOL fSuccess(FALSE);

	ximeta::CAutoLock autolock(this);

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		return FALSE;
	}

	{
		// TODO: Overhauling this!
		LANSCSI_REMOVE_TARGET_DATA removeTargetData;
		removeTargetData.ulSlotNo = m_dwSlot;
		::CopyMemory(
			removeTargetData.MasterUnitDisk.Address.Node,
			&m_pUnitDeviceIds[0].DeviceId,
			sizeof(removeTargetData.MasterUnitDisk.Address.Node));

		// TODO: Should not be hard-coded <- LPX_PORT_NUMBER
		removeTargetData.MasterUnitDisk.Address.Port =
			htons(NDAS_DEVICE_LPX_PORT);

		removeTargetData.MasterUnitDisk.ucUnitNumber = 
			m_pUnitDeviceIds[0].UnitNo;

		BOOL fSuccess = LsBusCtlRemoveTarget(&removeTargetData);
		if (!fSuccess) {
			// WARNING
			DPWarningEx(_FT("LsBusCtlUnplug failed\n"));
		}

	}

	::Sleep(1000);

	//
	// BUG:
	// What happened when RemoveTarget succeeded and 
	// Unplugging LANSCSI port is failed?
	//

	fSuccess = LsBusCtlUnplug(m_dwSlot);
	if (!fSuccess) {
		DPErrorEx(_FT("LsBusCtlUnplug failed\n"));
		// last error from lsbusctlunplug
		return FALSE;
	}

	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	DPInfo(_FT("Unplugged successfully at slot %d (Disk).\n"), m_dwSlot);

	return TRUE;

}

BOOL
CNdasLogicalDisk::
Eject()
{
	ximeta::CAutoLock autolock(this);

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		return FALSE;
	}

	BOOL fSuccess = LsBusCtlEject(m_dwSlot);
	if (!fSuccess) {
		DPErrorEx(_FT("LsBusCtlEject failed at slot %d.\n"), m_dwSlot);
		return FALSE;
	}

	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	DPInfo(_FT("Ejected successfully at slot %d (Disk).\n"), m_dwSlot);

	return TRUE;
}

DWORD
CNdasLogicalDisk::
GetMaxRequestBlocks()
{
	PCNdasInstanceManager pInst = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInst);

	PCNdasDeviceRegistrar pRegistrar = pInst->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	//
	// MaxRequestBlock is a NDAS Device's dependent property
	// So we only have to look at NDAS devices
	//

	DWORD dwMaxRequestBlocks(0);

	for (DWORD i = 0; i < m_nUnitDevices; i++) {

		PCNdasDevice pDevice = pRegistrar->Find(m_pUnitDeviceIds[i].DeviceId);
		if (NULL == pDevice) {
			return 0;
		}

		if (0 == i) {
			dwMaxRequestBlocks = pDevice->GetMaxRequestBlocks();
		} else {
			dwMaxRequestBlocks = 
				min(dwMaxRequestBlocks, pDevice->GetMaxRequestBlocks());
		}

	}

	return dwMaxRequestBlocks;

}

DWORD 
CNdasLogicalDisk::
GetUserBlockCount()
{
	DWORD dwBlocks(0);

	PCNdasInstanceManager pInst = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInst);

	PCNdasDeviceRegistrar pRegistrar = pInst->GetRegistrar();
	_ASSERTE(NULL != pRegistrar);

	for (DWORD i = 0; i < m_nUnitDevices; i++) {

		PCNdasDevice pDevice = 
			pRegistrar->Find(m_pUnitDeviceIds[i].DeviceId);
		if (NULL == pDevice) {
			return 0;
		}

		PCNdasUnitDevice pUnitDevice = 
			pDevice->GetUnitDevice(m_pUnitDeviceIds[i].UnitNo);
		if (NULL == pUnitDevice) {
			return 0;
		}

		_ASSERTE(pUnitDevice->GetType() == NDAS_UNITDEVICE_TYPE_DISK);

		PCNdasUnitDiskDevice pUnitDisk = 
			reinterpret_cast<PCNdasUnitDiskDevice>(pUnitDevice);

		// dwBlocks += pUnitDisk->GetBlocks();

		_ASSERTE(IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_devType));

		switch (m_devType)
		{
		case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
			if(0 == i) {
				dwBlocks = pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
			dwBlocks += pUnitDisk->GetUserBlockCount();
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID_1:
			if(i % 2) {
				dwBlocks += pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
			dwBlocks = pUnitDisk->GetUserBlockCount();
			break;
		default: // not implemented yet : DVD, VDVD, MO, FLASH ...
			_ASSERTE(FALSE);
			break;
		}

	}

	return dwBlocks;
}

NDAS_LOGICALDEVICE_STATUS
CNdasLogicalDisk::
GetStatus()
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == m_status)
	{
		BOOL bAdapterError(FALSE);
		BOOL fSuccess = LsBusCtlQueryNodeAlive(m_dwSlot, &bAdapterError);

		if (!fSuccess) {
			DPErrorEx(_FT("LsBusCtlQueryNodeAlive returned FALSE at slot %d.\n"), m_dwSlot);
			// TODO: Fix this? Remove ROFilter?
			SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
		}

		if (bAdapterError) {
			DPErrorEx(_FT("LsBusCtlQueryNodeAlive reported an adapter error.\n"));
			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
		}

	}

	return m_status;
}

LPCTSTR 
CNdasLogicalDisk::
ToString()
{
	LPTSTR pszNext = m_szStringRep;
	size_t cchRemaining = MAX_STRING_REP;

	HRESULT hr = ::StringCchPrintfEx(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
		TEXT("[%d]: %s(%d) %s ["),
		m_dwSlot,
		NdasLogicalDeviceTypeString(m_devType),
		m_nUnitDevices,
		CNdasUnitDeviceId(m_primaryUnitDeviceId).ToString());
	
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	if (STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		return m_szStringRep;
	}

	for (DWORD i = 0; i < m_nUnitDevices; ++i) {
		if (IsNullNdasUnitDeviceId(m_pUnitDeviceIds[i])) {
			continue;
		}
		hr = ::StringCchPrintfEx(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
			TEXT("(%d, %s) "),
			i,
			CNdasUnitDeviceId(m_pUnitDeviceIds[i]).ToString());

		_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

		if (STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
			return m_szStringRep;
		}
	}

	hr = ::StringCchPrintfEx(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
		TEXT("]"));
	
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	return m_szStringRep;
}

