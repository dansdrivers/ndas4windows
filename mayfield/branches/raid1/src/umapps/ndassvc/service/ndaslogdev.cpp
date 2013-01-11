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
#include "ndasobjs.h"
#include "lsbusctl.h"
#include "lpxcomm.h"
#include "autores.h"
#include "ndascfg.h"
#include "ndasunitdevfactory.h"

#include "ndasmsg.h"
#include "ndastype_str.h"
#include "scrc32.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASLOGDEV
#include "xdebug.h"

static 
BOOL 
pCheckStatusValidity(
	NDAS_LOGICALDEVICE_STATUS oldStatus,
	NDAS_LOGICALDEVICE_STATUS newStatus);

static 
UCHAR 
pConvertToLSBusTargetType(
	NDAS_LOGICALDEVICE_TYPE logDevType);

ULONG
CNdasLogicalDevice::AddRef()
{
	ximeta::CAutoLock autolock(this);

	ULONG ulCount = ximeta::CExtensibleObject::AddRef();
	DBGPRT_INFO(_FT("AddRef Reference: %u\n"), ulCount);
	return ulCount;
}

ULONG
CNdasLogicalDevice::Release()
{
	ximeta::CAutoLock autolock(this);

	ULONG ulCount = ximeta::CExtensibleObject::Release();
	DBGPRT_INFO(_FT("Release Reference: %u\n"), ulCount);
	return ulCount;
}

//
// Constructor for a multiple member logical device
//
CNdasLogicalDevice::CNdasLogicalDevice(
	NDAS_LOGICALDEVICE_ID logDevId, 
	CONST NDAS_LOGICALDEVICE_GROUP& ldGroup) :
	m_logicalDeviceId(logDevId),
	m_nUnitDeviceInstances(0),
	m_status(NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED),
	m_lastError(NDAS_LOGICALDEVICE_ERROR_NONE),
	m_dwMountedDriveSet(0),
	m_MountedAccess(0),
	m_hAlarmEvent(NULL),
	m_hDisconnectedEvent(NULL),
	m_bReconnecting(FALSE),
	m_fMountOnReady(FALSE),
	m_fReducedMountOnReadyAccess(FALSE),
	m_mountOnReadyAccess(0),
	m_fRiskyMount(FALSE),
	m_dwMountTick(0),
	m_dwCurrentMRB(0),
	m_logicalDeviceGroup(ldGroup),
	m_NdasScsiLocation(0),
	m_fShutdown(FALSE)
{
	//
	// Clear out unit device instances
	//
	::ZeroMemory(
		m_pUnitDevices,
		sizeof(CNdasUnitDevice*) * MAX_NDAS_LOGICALDEVICE_GROUP_MEMBER);

	//
	// Locate Registry Container based on its hash value
	//
	cpLocateRegContainer();

	DBGPRT_TRACE(_FT("ctor: %s\n"), ToString());
}

//
// Destructor
//
CNdasLogicalDevice::~CNdasLogicalDevice()
{
	ximeta::CAutoLock autolock(this);

	DBGPRT_TRACE(_FT("dtor: %s\n"), ToString());

	_ASSERTE(m_nUnitDeviceInstances == 0);

	if (NULL != m_hDisconnectedEvent) {
		BOOL fSuccess = ::CloseHandle(m_hDisconnectedEvent);
		_ASSERTE(fSuccess);
		m_hDisconnectedEvent = NULL;
	}

	if (NULL != m_hAlarmEvent) {
		BOOL fSuccess = ::CloseHandle(m_hAlarmEvent);
		_ASSERTE(fSuccess);
		m_hAlarmEvent = NULL;
	}
}

BOOL
CNdasLogicalDevice::Initialize()
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED != m_status) {
		// Already initialized
		return FALSE;
	}

	if (NULL == m_hDisconnectedEvent) {
		m_hDisconnectedEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hDisconnectedEvent) {
			DBGPRT_ERR_EX(_FT("Disconnect event creation failed: "));
			return FALSE;
		}
	}

	if (NULL == m_hAlarmEvent) {
		m_hAlarmEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hAlarmEvent) {
			DBGPRT_ERR_EX(_FT("Alarm event creation failed: "));
			::CloseHandle(m_hDisconnectedEvent);
			return FALSE;
		}
	}

	ACCESS_MASK mountMask;

	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("MountMask"),
		(LPDWORD)&mountMask);

	BOOL fRiskyMountFlag = GetRiskyMountFlag();
	if (fRiskyMountFlag) {
		m_fRiskyMount = fRiskyMountFlag;
	}

	if (fSuccess && (mountMask > 0) && !IsRiskyMount()) {
		SetMountOnReady(mountMask, FALSE);
	}

	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	DBGPRT_INFO(_FT("Logical Device %d initialized successfully.\n"), m_logicalDeviceId);

	return TRUE;
}

NDAS_LOGICALDEVICE_ID
CNdasLogicalDevice::GetLogicalDeviceId()
{
	ximeta::CAutoLock autolock(this);
	return m_logicalDeviceId;
}

//
// Set the unit device ID at a sequence 
// to a unit device member ID list
//
BOOL 
CNdasLogicalDevice::AddUnitDevice(CNdasUnitDevice& unitDevice)
{
	ximeta::CAutoLock autolock(this);
	DWORD ldSequence = unitDevice.GetLDSequence();

	_ASSERTE(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices) {
		DBGPRT_ERR(_FT("Invalid sequence (%d) of the unit device.\n"), ldSequence);
		return FALSE;
	}

	_ASSERTE(NULL == m_pUnitDevices[ldSequence]);
	if (NULL != m_pUnitDevices[ldSequence]) {
		DBGPRT_ERR(_FT("Sequence (%d) is already occupied.\n"), ldSequence);
		return FALSE;
	}

	_ASSERTE(NULL == m_pUnitDevices[ldSequence]);
	m_pUnitDevices[ldSequence] = &unitDevice;
	m_pUnitDevices[ldSequence]->AddRef();
	++m_nUnitDeviceInstances;

	_ASSERTE(m_nUnitDeviceInstances >= 0 && 
		m_nUnitDeviceInstances <= GetUnitDeviceCount());

	if (m_nUnitDeviceInstances < GetUnitDeviceCount()) {
		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
	} else {
		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_NONE);
	}

	//
	// Allocate NDAS SCSI Location when the first unit device is
	// available
	//
	if (0 == ldSequence) {
		cpAllocateNdasScsiLocation();
	}

	if (IsComplete()) {
		CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();
		pLdm->RegisterNdasScsiLocation(m_NdasScsiLocation, *this);
	}

	//
	// Boot-time Plug In Support
	//
	if (m_nUnitDeviceInstances == GetUnitDeviceCount()) {
		if (m_fMountOnReady) {
			if (m_mountOnReadyAccess & GetAllowingAccess())
			{
				(VOID) PlugIn(m_mountOnReadyAccess);
			} else if ((GENERIC_READ & GetAllowingAccess()) && 
				m_fReducedMountOnReadyAccess)
			{
				(VOID) PlugIn(GENERIC_READ);
			}
		}
	}

	DBGPRT_INFO(_FT("Added %s to Logical Device %s\n"), 
		unitDevice.ToString(), 
		this->ToString());

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
	(VOID) pEventPublisher->LogicalDeviceRelationChanged(m_logicalDeviceId);
	return TRUE;
}

//
// Remove the unit device ID from the list
//
BOOL
CNdasLogicalDevice::RemoveUnitDevice(CNdasUnitDevice& unitDevice)
{
	ximeta::CAutoLock autolock(this);
	DWORD ldSequence = unitDevice.GetLDSequence();

	_ASSERTE(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	if (ldSequence >= m_logicalDeviceGroup.nUnitDevices) {
		DBGPRT_ERR(_FT("Invalid sequence (%d) of the unit device.\n"), ldSequence);
		return FALSE;
	}

	_ASSERTE(&unitDevice == m_pUnitDevices[ldSequence]);
	if (&unitDevice != m_pUnitDevices[ldSequence]) {
		DBGPRT_ERR(_FT("Unit device in sequence (%d) is not occupied.\n"), ldSequence);
		return FALSE;
	}

	m_pUnitDevices[ldSequence]->Release();
	m_pUnitDevices[ldSequence] = NULL;

	//
	// Workaround for Fault-tolerant Mode
	// -> Even if the logical device is incomplete,
	//    Mounted Logical Device Location should be intact
	//    if it is mounted
	if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == GetStatus()) 
	{
		--m_nUnitDeviceInstances;
		return TRUE;
	}

	if (IsComplete()) {
		CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();
		pLdm->UnregisterNdasScsiLocation(m_NdasScsiLocation);
	}

	--m_nUnitDeviceInstances;

	//
	// Deallocation NDAS SCSI Location when the fist unit device is removed
	//
	if (0 == ldSequence) {
		cpDeallocateNdasScsiLocation();
	}

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
	(VOID) pEventPublisher->LogicalDeviceRelationChanged(m_logicalDeviceId);
	SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);

	return TRUE;
}

//
// Get the unit device instance count
//
DWORD 
CNdasLogicalDevice::GetUnitDeviceInstanceCount()
{
	ximeta::CAutoLock autolock(this);
	return m_nUnitDeviceInstances;
}

BOOL
CNdasLogicalDevice::IsComplete()
{
	ximeta::CAutoLock autolock(this);
	return m_nUnitDeviceInstances == m_logicalDeviceGroup.nUnitDevices;
}

//
// Get the defined maximum number of unit devices
//
DWORD 
CNdasLogicalDevice::GetUnitDeviceCount()
{
	ximeta::CAutoLock autolock(this);
	return m_logicalDeviceGroup.nUnitDevices;
}

//
// Get the unit device ID in i-th sequence.
//
CONST NDAS_UNITDEVICE_ID&
CNdasLogicalDevice::GetUnitDeviceID(DWORD ldSequence)
{
	ximeta::CAutoLock autolock(this);
	_ASSERTE(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	return m_logicalDeviceGroup.UnitDevices[ldSequence];
}

CNdasUnitDevice*
CNdasLogicalDevice::GetUnitDevice(DWORD ldSequence)
{
	ximeta::CAutoLock autolock(this);
	_ASSERTE(ldSequence < m_logicalDeviceGroup.nUnitDevices);
	return m_pUnitDevices[ldSequence];
}

CONST NDAS_LOGICALDEVICE_GROUP&
CNdasLogicalDevice::GetLDGroup()
{
	ximeta::CAutoLock autolock(this);
	return m_logicalDeviceGroup;
}

NDAS_LOGICALDEVICE_TYPE
CNdasLogicalDevice::GetType()
{
	ximeta::CAutoLock autolock(this);
	return m_logicalDeviceGroup.Type;
}

VOID
CNdasLogicalDevice::OnDisconnect()
{
	ximeta::CAutoLock autolock(this);
	//
	// We need to report two status for unmounting process
	//
	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);
	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
}

VOID 
CNdasLogicalDevice::SetStatus(NDAS_LOGICALDEVICE_STATUS newStatus)
{
	ximeta::CAutoLock autolock(this);

	if (m_status == newStatus) {
		return;
	}

	NDAS_LOGICALDEVICE_STATUS oldStatus = m_status;

	BOOL fValid = pCheckStatusValidity(m_status, newStatus);
	_ASSERTE(fValid);

	//
	// Attach or detach from the event monitor
	//
	CNdasEventMonitor* pEventMon = pGetNdasEventMonitor();

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == m_status&&
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == newStatus)
	{
		pEventMon->Attach(this);
	} 
	else if (
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == m_status &&
		NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == newStatus)
	{
		pEventMon->Detach(this);
	}

	m_status = newStatus;

	//
	// mount mask
	//
	{
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == m_status) {
			
			BOOL fSuccess = _NdasSystemCfg.SetValueEx(
				m_szRegContainer, 
				_T("MountMask"), 
				m_MountedAccess);

			DWORD dwTick = ::GetTickCount();
			if (0 == dwTick) dwTick = 1; // 0 is used for special purpose
			m_dwMountTick = dwTick;

			SetRiskyMountFlag(TRUE);

		} else if (
			// NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == oldStatus &&
			NDAS_LOGICALDEVICE_STATUS_UNMOUNTED == m_status) 
		{	
			if (!m_fShutdown) {
				BOOL fSuccess = _NdasSystemCfg.SetValueEx(
					m_szRegContainer, 
					_T("MountMask"), 
					(DWORD) 0);
			}

			m_dwMountTick = 0;
			SetRiskyMountFlag(FALSE);
		}

	}

	//
	// Update Unit Device Status
	//

	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {

		PCNdasUnitDevice pUnitDevice = GetUnitDevice(i);

		if (NULL == pUnitDevice) {
			DPError(_FT("Unit Device %s is not found.\n"),
				CNdasUnitDeviceId(GetUnitDeviceID(i)).ToString());
			continue;
		}

		//
		// TODO: Create a status updater with observer
		//
		switch (m_status) {
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
		case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
			pUnitDevice->SetStatus(NDAS_UNITDEVICE_STATUS_MOUNTED);
			break;
		default:
			// otherwise
			pUnitDevice->SetStatus(NDAS_UNITDEVICE_STATUS_NOT_MOUNTED);
		}
	}

	
	//
	// publish an event
	//

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();

	(VOID) pEventPublisher->
		LogicalDeviceStatusChanged(m_logicalDeviceId, oldStatus, newStatus);

}

VOID 
CNdasLogicalDevice::SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR logDevError)
{
	ximeta::CAutoLock autolock(this);
	m_lastError = logDevError;
}

ACCESS_MASK 
CNdasLogicalDevice::GetMountedAccess()
{ 
	ximeta::CAutoLock autolock(this);
	return m_MountedAccess; 
}

VOID 
CNdasLogicalDevice::SetMountedAccess(ACCESS_MASK mountedAccess)
{ 
	ximeta::CAutoLock autolock(this);
	m_MountedAccess = mountedAccess; 
}

ACCESS_MASK 
CNdasLogicalDevice::GetGrantedAccess()
{
	ximeta::CAutoLock autolock(this);
	if (0 == GetUnitDeviceCount()) {
		return 0x00000000L;
	}

	ACCESS_MASK access(0xFFFFFFFFL);
	//
	// Any single missing entry will revoke all accesses
	//
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {

		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);

		if (NULL == pUnitDevice) {
			return 0x00000000L;
		}

		access &= pUnitDevice->GetGrantedAccess();
	}

	return access;
}

ACCESS_MASK 
CNdasLogicalDevice::GetAllowingAccess()
{
	ximeta::CAutoLock autolock(this);
	if (0 == GetUnitDeviceCount()) {
		return 0x00000000L;
	}

	ACCESS_MASK access(0xFFFFFFFFL);
	//
	// Any single missing entry will revoke all accesses
	//
	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {

		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);

		if (NULL == pUnitDevice) {
			return 0x00000000L;
		}

		access &= pUnitDevice->GetAllowingAccess();
	}

	return access;
}

DWORD 
CNdasLogicalDevice::GetMountedDriveSet()
{ 
	ximeta::CAutoLock autolock(this);
	return m_dwMountedDriveSet; 
}

VOID 
CNdasLogicalDevice::SetMountedDriveSet(DWORD dwDriveSet)
{ 
	ximeta::CAutoLock autolock(this);
	m_dwMountedDriveSet = dwDriveSet; 
}

NDAS_LOGICALDEVICE_ERROR
CNdasLogicalDevice::GetLastError()
{
	ximeta::CAutoLock autolock(this);
	return m_lastError;
}

HANDLE
CNdasLogicalDevice::GetDisconnectEvent()
{
	ximeta::CAutoLock autolock(this);
	return m_hDisconnectedEvent;
}

HANDLE
CNdasLogicalDevice::GetAlarmEvent()
{
	ximeta::CAutoLock autolock(this);
	return m_hAlarmEvent;
}

NDAS_LOGICALDEVICE_STATUS
CNdasLogicalDevice::GetStatus()
{
	ximeta::CAutoLock autolock(this);

	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == m_status) {

		_ASSERTE(!m_NdasScsiLocation.IsInvalid());

		BOOL bAdapterError, bAlive;

		BOOL fSuccess = LsBusCtlQueryNodeAlive(
			m_NdasScsiLocation.SlotNo, 
			&bAlive, 
			&bAdapterError);
		
		//
		// if LsBusCtlQueryNodeAlive fails, 
		// there may be no lanscsibus device instance...
		//

		if (!fSuccess) {
			DBGPRT_ERR_EX(_FT("LsBusCtlQueryNodeAlive at %s failed: "), 
				m_NdasScsiLocation.ToString());
			// TODO: Fix this? Remove ROFilter?
			SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

		} else {

			if (!bAlive) {
				SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
			}

			if (bAdapterError) {
				DBGPRT_ERR_EX(_FT("LsBusCtlQueryNodeAlive reported an adapter error.\n"));
				SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
			}
		}

	}

	return m_status;
}

DWORD
CNdasLogicalDevice::GetCurrentMaxRequestBlocks()
{
	ximeta::CAutoLock autolock(this);

	return m_dwCurrentMRB;
}

DWORD
CNdasLogicalDevice::cpGetMaxRequestBlocks()
{
	ximeta::CAutoLock autolock(this);

	//
	// MaxRequestBlock is a NDAS Device's dependent property
	// So we only have to look at NDAS devices
	//

	DWORD dwMaxRequestBlocks(0);

	for (DWORD i = 0; i < GetUnitDeviceCount(); i++) {

		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);
		if (NULL == pUnitDevice) {
			return 0;
		}

		if (0 == i) {
			dwMaxRequestBlocks = pUnitDevice->GetOptimalMaxRequestBlock();
		} else {
			dwMaxRequestBlocks = 
				min(dwMaxRequestBlocks, pUnitDevice->GetOptimalMaxRequestBlock());
		}

	}

	m_dwCurrentMRB = dwMaxRequestBlocks;
	return dwMaxRequestBlocks;

}

LPCTSTR 
CNdasLogicalDevice::ToString()
{
	ximeta::CAutoLock autolock(this);

	LPTSTR pszNext = m_szStringRep;
	size_t cchRemaining = MAX_STRING_REP;

	HRESULT hr = ::StringCchPrintfEx(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
		TEXT("[%02X]: %s(%d) ["),
		m_logicalDeviceId,
		(NdasLogicalDeviceTypeString(GetType()) + 24), // remove prefix
		GetUnitDeviceCount());
	
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	if (STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		return m_szStringRep;
	}

	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {
		hr = ::StringCchPrintfEx(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, STRSAFE_IGNORE_NULLS,
			TEXT("%s(%p) "),
			CNdasUnitDeviceId(GetUnitDeviceID(i)).ToString(),
			m_pUnitDevices[i]);

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

BOOL
CNdasLogicalDevice::IsRiskyMount()
{
	ximeta::CAutoLock autolock(this);
	return m_fRiskyMount;
}

BOOL
CNdasLogicalDevice::GetRiskyMountFlag()
{
	ximeta::CAutoLock autolock(this);

	BOOL fRisky = FALSE;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		m_szRegContainer,
		_T("RiskyMountFlag"),
		&fRisky);

	return fRisky;
}

VOID
CNdasLogicalDevice::SetAllUnitDevicesFault()
{
	DWORD i;
	for(i = 0; i < m_logicalDeviceGroup.nUnitDevices; i++)
		m_pUnitDevices[i]->SetFault();
}

BOOL
CNdasLogicalDevice::IsAnyUnitDevicesFault()
{
	DWORD i;
	for(i = 0; i < m_logicalDeviceGroup.nUnitDevices; i++)
		if(m_pUnitDevices[i]->IsFault())
			return TRUE;

	return FALSE;
}

VOID
CNdasLogicalDevice::SetRiskyMountFlag(BOOL fRisky)
{
	ximeta::CAutoLock autolock(this);

	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		m_szRegContainer,
		_T("RiskyMountFlag"),
		fRisky);

	m_fRiskyMount = fRisky;
}
BOOL 
CNdasLogicalDevice::PlugIn(ACCESS_MASK requestingAccess)
{
	ximeta::CAutoLock autolock(this);

	BOOL fSuccess = cpCheckPlugInCondition(requestingAccess);
	if (!fSuccess) {
		return FALSE;
	}

	//
	// Plug In
	// - NDAS Controller
	//
	CNdasUnitDevice* pPrimaryUnitDevice = GetUnitDevice(0);

	if (NULL == pPrimaryUnitDevice) {
		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
		return FALSE;
	}

	CNdasDevice* pDevice = pPrimaryUnitDevice->GetParentDevice();
	if (NULL == pDevice) {
		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
	}

	//
	// Max DVD Instance (Global Instance) Constraint
	//
	// Do not allow local host to have multiple DVD devices
	// with Write Access.
	// I-O Data's request?
	//
#if NDAS_FEATURE_DONOT_ALLOW_MULTIPLE_DVD_RW_INSTANCES
	if (NDAS_LOGICALDEVICE_TYPE_DVD == GetType()) {
		fSuccess = cpCheckPlugInCondForDVD(requestingAccess);
		if (!fSuccess) {
			// SetLastError is set by the pCheckPlugInCondForDVD
			return FALSE;
		}
	}
#endif

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
		GetUnitDeviceCount() * sizeof(LSBUS_UNITDISK);

	PLANSCSI_ADD_TARGET_DATA pAddTargetData = 
		(PLANSCSI_ADD_TARGET_DATA) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbAddTargetDataSize);

	if (NULL == pAddTargetData) {
		// TODO: Out of memory
		return FALSE;
	}

	//
	// automatically free the heap when it goes out of the scope
	//
	AutoProcessHeap autoHeap = pAddTargetData;

	UCHAR ucTargetType = pConvertToLSBusTargetType(m_logicalDeviceGroup.Type);

	pAddTargetData->ulSize = cbAddTargetDataSize;
	pAddTargetData->ulSlotNo = m_NdasScsiLocation.SlotNo;
	pAddTargetData->ulTargetBlocks = 0; // initialization and will be added
	pAddTargetData->DesiredAccess = requestingAccess;
	pAddTargetData->ulNumberOfUnitDiskList = GetUnitDeviceCount();
	pAddTargetData->ucTargetType = ucTargetType;

	//
	// Set Content Encryption from the primary unit device
	// (Only for Disk Devices)
	//

	if (NDAS_UNITDEVICE_TYPE_DISK == pPrimaryUnitDevice->GetType()) {
		
		CNdasUnitDiskDevice* pUnitDiskDevice = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pPrimaryUnitDevice);

		CONST NDAS_CONTENT_ENCRYPT& encrypt = pUnitDiskDevice->GetEncryption();

		pAddTargetData->CntEcrKeyLength = encrypt.KeyLength;
		pAddTargetData->CntEcrMethod = encrypt.Method;
		::CopyMemory(
			pAddTargetData->CntEcrKey,
			encrypt.Key,
			encrypt.KeyLength);

	}

	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {

		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);
		if (NULL == pUnitDevice) {
			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
			return FALSE;
		}

		CNdasDevice* pDevice = pUnitDevice->GetParentDevice();
		_ASSERTE(NULL != pDevice);

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

		pud->ulUnitBlocks = pUnitDevice->GetUserBlockCount();
		pud->ulPhysicalBlocks = pUnitDevice->GetPhysicalBlockCount();
		pud->ucUnitNumber = pUnitDevice->GetUnitNo();
		pud->ucHWType = pDevice->GetHWType();
		pud->ucHWVersion = pDevice->GetHWVersion();
		pud->IsWANConnection = FALSE;

		//
		// Add Target Info
		//
		if (NDASSCSI_TYPE_DISK_RAID1 == ucTargetType) {

			CNdasUnitDiskDevice* pUnitDiskDevice = 
				reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);
			_ASSERTE(NULL != pUnitDiskDevice->GetAddTargetInfo());

			::CopyMemory(
				&pud->RAID_Info,
				pUnitDiskDevice->GetAddTargetInfo(),
				sizeof(INFO_RAID));

			if (0 == pud->RAID_Info.SectorsPerBit) {
				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION);
				return FALSE;
			}
		}
		else if(NDASSCSI_TYPE_DISK_RAID4 == ucTargetType) {

			CNdasUnitDiskDevice* pUnitDiskDevice = 
				reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);
			_ASSERTE(NULL != pUnitDiskDevice->GetAddTargetInfo());

			::CopyMemory(
				&pud->RAID_Info,
				pUnitDiskDevice->GetAddTargetInfo(),
				sizeof(INFO_RAID));

			if (0 == pud->RAID_Info.SectorsPerBit) {
				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION);
				return FALSE;
			}
		}

		if (NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType()) {
			CNdasUnitDiskDevice* pUnitDiskDevice =
				reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);

			//
			// check if last DIB information is same with current one
			//
			if(!pUnitDiskDevice->HasSameDIBInfo())
			{
				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION);
				return FALSE;
			}

			// driver will check bitmap. pass here
/*
			//
			// check if bitmap status in RAID 1, 4. Do not plug in
			//
			if (NDAS_UNITDEVICE_DISK_TYPE_RAID1 == pUnitDiskDevice->GetSubType().DiskDeviceType ||
				NDAS_UNITDEVICE_DISK_TYPE_RAID4 == pUnitDiskDevice->GetSubType().DiskDeviceType)
			{
				if (!pUnitDiskDevice->IsBitmapClean()) {
					::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_CORRUPTED_BIND_INFORMATION);
					return FALSE;
				}
			}
*/
			//
			// Done checking plug in condition
			//
		}
	}

	//
	// Check Multiple Write Access Compatibility
	//
	if (GENERIC_WRITE & requestingAccess) {
		DWORD nROHosts, nRWHosts;
		BOOL fSuccess = pPrimaryUnitDevice->GetHostUsageCount(
			&nROHosts,
			&nRWHosts,
			TRUE);
		if (!fSuccess) {
			::SetLastError(NDASHLPSVC_ERROR_NDAS_DEVICE_COMMUNICATION_FAILURE);
			return FALSE;
		}
		if (nRWHosts > 0) {
			fSuccess = pPrimaryUnitDevice->CheckNDFSCompatibility();
			if (!fSuccess) {
				::SetLastError(NDASHLPSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED);
				return FALSE;
			}
		}
	}

	pAddTargetData->ulTargetBlocks = GetUserBlockCount();
	DWORD dwMaxRequestBlocks = cpGetMaxRequestBlocks();

	DBGPRT_INFO(
		_FT("LsBusCtlPluginEx(SlotNo %d, MaxReqBlock %d, DisEvt %p, RecEvt %p).)\n"),
		m_NdasScsiLocation.SlotNo, 
		dwMaxRequestBlocks, 
		m_hDisconnectedEvent, 
		m_hAlarmEvent);

	SetReconnectFlag(FALSE);

	fSuccess = LsBusCtlPlugInEx(
		m_NdasScsiLocation.SlotNo,
		dwMaxRequestBlocks,
		m_hDisconnectedEvent,
		m_hAlarmEvent);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LsBusCtlPluginEx failed: \n"));
		_ASSERTE(fSuccess);
		return FALSE;
	}

	DBGPRT_INFO(_FT("LsBusCtlPluginEx succeeded.\n"));

	BEGIN_DBGPRT_BLOCK_INFO()
		DBGPRT_INFO(_FT("LsBusCtlAddTarget(pAddTargetData)\n"));
		DPType(XDebug::OL_INFO, pAddTargetData, cbAddTargetDataSize);
	END_DBGPRT_BLOCK()

	fSuccess = LsBusCtlAddTarget(pAddTargetData);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("AddTarget failed.\n"));
		::Sleep(1000);
		LsBusCtlEject(m_NdasScsiLocation.SlotNo);
		_ASSERTE(fSuccess);
		return FALSE;
	}

	DBGPRT_INFO(_FT("LsBusCtlAddTarget succeeded.\n"));

	SetMountedAccess(requestingAccess);
	SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);

	DBGPRT_INFO(_FT("Plugged in successfully at %s.\n"), m_NdasScsiLocation.ToString());

	return TRUE;
}

BOOL
CNdasLogicalDevice::Unplug()
{
	BOOL fSuccess(FALSE);

	ximeta::CAutoLock autolock(this);

	DBGPRT_INFO(_FT("Unplugging %s\n"), ToString());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED &&
		m_status != NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING &&
		m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING) 
	{
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		return FALSE;
	}

	//
	// LsBusCtl is overhauled only to use SlotNo for RemoveTargetData
	//

	//
	// Remove target ejects the disk and the volume.
	//

	fSuccess = LsBusCtlRemoveTarget(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) {
		DBGPRT_WARN_EX(_FT("LsBusCtlRemoveTarget failed: "));
	}

	//{
	//	// TODO: Overhauling this!
	//	LANSCSI_REMOVE_TARGET_DATA removeTargetData;
	//	removeTargetData.ulSlotNo = m_dwSlot;
	//	::CopyMemory(
	//		removeTargetData.MasterUnitDisk.Address.Node,
	//		&m_pUnitDeviceIds[0].DeviceId,
	//		sizeof(removeTargetData.MasterUnitDisk.Address.Node));

	//	// TODO: Should not be hard-coded <- LPX_PORT_NUMBER
	//	removeTargetData.MasterUnitDisk.Address.Port =
	//		htons(NDAS_DEVICE_LPX_PORT);

	//	removeTargetData.MasterUnitDisk.ucUnitNumber = 
	//		m_pUnitDeviceIds[0].UnitNo;

	//	fSuccess = LsBusCtlRemoveTarget(&removeTargetData);
	//	if (!fSuccess) {
	//		// WARNING
	//		DBGPRT_WARN_EX(_FT("LsBusCtlUnplug failed\n"));
	//	}

	//}

	// Intentional break
	::Sleep(100);

	//
	// BUG:
	// What happened when RemoveTarget succeeded and 
	// Unplugging LANSCSI port is failed?
	//

	fSuccess = LsBusCtlUnplug(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LsBusCtlUnplug failed: "));
		// last error from lsbusctl unplug
		return FALSE;
	}

	//
	// BUGBUG: This line was commented. But why?
	// We need to set this device as disconnected here
	// Otherwise, the logical device will be locked as MOUNT_PENDING
	// when the device driver sets an disconnect event to us.
	OnDisconnect();

	DBGPRT_INFO(_FT("Unplugged successfully at slot %s.\n"),
		CNdasScsiLocation(m_NdasScsiLocation).ToString());

	return TRUE;

}

BOOL
CNdasLogicalDevice::Recover()
{
	BOOL fSuccess(FALSE);

	ximeta::CAutoLock autolock(this);

	DBGPRT_INFO(_FT("Recovering %s\n"), ToString());

	switch(GetType())
	{
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		break;
	default:
		return FALSE;
	}

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) 
	{
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		return FALSE;
	}

	// Do not recover if any NDAS unit device is not alive
	DWORD ldSequence = 0;
	for(ldSequence = 0; ldSequence < m_logicalDeviceGroup.nUnitDevices; ldSequence++)
	{
		if(NDAS_UNITDEVICE_STATUS_MOUNTED != m_pUnitDevices[ldSequence]->GetStatus())
		{
			::SetLastError(NDASHLPSVC_ERROR_NDAS_UNITDEVICE_NOT_MOUNTED);
			return FALSE;
		}
	}

	if(IsAnyUnitDevicesFault())
	{
		::SetLastError(NDASHLPSVC_ERROR_NDAS_UNITDEVICE_NOT_MOUNTED);
		return FALSE;
	}

	ULONG ulStatus;

	fSuccess = ::LsBusCtlQueryStatus(		
		m_NdasScsiLocation.SlotNo,
		&ulStatus);

	if (!fSuccess) {
		DPErrorEx(_FT("Unable to get status"));
		return FALSE;
	}

	// Do not recover if NDAS logical device is not under emergency
	// Do not recover if NDAS logical device is already recovering
	if(!ADAPTERINFO_ISSTATUSFLAG(ulStatus, ADAPTERINFO_STATUSFLAG_MEMBER_FAULT))
	{
		DPErrorEx(_FT("Not in emergency mode or already recovering"));
		return FALSE;
	}


	//
	// LsBusCtl is overhauled only to use SlotNo for RemoveTargetData
	//

	//
	// Remove target ejects the disk and the volume.
	//

	fSuccess = LsBusCtlRecoverTarget(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) {
		DBGPRT_WARN_EX(_FT("LsBusCtlRemoveTarget failed: "));
	}

	DBGPRT_INFO(_FT("Started recovering successfully at slot %s.\n"),
		CNdasScsiLocation(m_NdasScsiLocation).ToString());

	return TRUE;

}

BOOL
CNdasLogicalDevice::Eject()
{
	ximeta::CAutoLock autolock(this);

	DBGPRT_INFO(_FT("Ejecting %s\n"), ToString());

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		DPError(_FT("Eject is requested to not initialized logical device"));
		return FALSE;
	}

	if (m_status != NDAS_LOGICALDEVICE_STATUS_MOUNTED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_MOUNTED);
		DPError(_FT("Eject is requested to not mounted logical device"));
		return FALSE;
	}

	BOOL fSuccess = ::LsBusCtlEject(m_NdasScsiLocation.SlotNo);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LsBusCtlEject failed at %s.\n"), m_NdasScsiLocation.ToString());
		return FALSE;
	}

	SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);

	DBGPRT_INFO(_FT("Ejected successfully at slot %s.\n"), m_NdasScsiLocation.ToString());

	return TRUE;
}

BOOL 
CNdasLogicalDevice::GetSharedWriteInfo(
	LPBOOL lpbSharedWrite, LPBOOL lpbPrimary)
{
	if (lpbSharedWrite) {
#ifdef NDAS_FEATURE_DISABLE_SHARED_WRITE
		*lpbSharedWrite = FALSE;
#else
		*lpbSharedWrite = TRUE;
#endif
	}

#ifdef NDAS_FEATURE_DISABLE_SHARED_WRITE
	// none for no shared write software
#else

	if (NULL == lpbPrimary) {
		return TRUE;
	}

	BUSENUM_QUERY_INFORMATION BusEnumQuery = {0};
	BUSENUM_INFORMATION BusEnumInformation = {0};

	BusEnumQuery.InfoClass = INFORMATION_PDO;
	BusEnumQuery.Size = sizeof(BUSENUM_QUERY_INFORMATION);
	BusEnumQuery.SlotNo = m_NdasScsiLocation.SlotNo;

	BOOL fSuccess = ::LsBusCtlQueryInformation(
		&BusEnumQuery,
		sizeof(BUSENUM_QUERY_INFORMATION),
		&BusEnumInformation,
		sizeof(BUSENUM_INFORMATION));

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("LanscsiQueryInformation failed at slot %d: "), 
			m_NdasScsiLocation.SlotNo);
		return FALSE;
	}

	if (ND_ACCESS_ISRW(BusEnumInformation.PdoInfo.GrantedAccess)) {
		*lpbPrimary = TRUE;
	} else {
		*lpbPrimary = FALSE;
	}

#endif

	return TRUE;

}

VOID
CNdasLogicalDevice::SetMountOnReady(
	ACCESS_MASK access, 
	BOOL fReducedMountOnReadyAccess)
{
	ximeta::CAutoLock autolock(this);

	if (0 == access) {
		m_mountOnReadyAccess = 0;
		m_fMountOnReady = FALSE;
		m_fReducedMountOnReadyAccess = fReducedMountOnReadyAccess;
	} else {
		m_fMountOnReady = TRUE;
		m_mountOnReadyAccess = access;
		m_fReducedMountOnReadyAccess = fReducedMountOnReadyAccess;
	}
}

DWORD
CNdasLogicalDevice::GetMountTick()
{
	return m_dwMountTick;
}

CONST NDAS_SCSI_LOCATION& 
CNdasLogicalDevice::GetNdasScsiLocation()
{
	return m_NdasScsiLocation;
}

VOID
CNdasLogicalDevice::OnShutdown()
{
	ximeta::CAutoLock autolock(this);

	SetRiskyMountFlag(FALSE);
	m_fShutdown = TRUE;
}

BOOL
CNdasLogicalDevice::cpCheckPlugInCondition(ACCESS_MASK requestingAccess)
{
	ximeta::CAutoLock autolock(this);

	BOOL fSuccess = FALSE;

	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
		return FALSE;
	}

	//
	// only from NOT_MOUNTED_STATUS
	//
	if (m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNTED) {
		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_UNMOUNTED);
		return FALSE;
	}

	if (GetUnitDeviceCount() != m_nUnitDeviceInstances) {
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

	return TRUE;
}

BOOL 
CNdasLogicalDevice::cpCheckPlugInCondForDVD(ACCESS_MASK requestingAccess)
{
	ximeta::CAutoLock autolock(this);

	CNdasLogicalDeviceManager* pLdm = pGetNdasLogicalDeviceManager();
	pLdm->Lock();

	DWORD nRWMountedDVD = 0;
	CNdasLogicalDeviceManager::ConstIterator itr;
	for (itr = pLdm->begin(); itr != pLdm->end(); ++itr) {
		CNdasLogicalDevice* pLogDevice = itr->second;
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus() ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == pLogDevice->GetStatus() ||
			NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == pLogDevice->GetStatus())
		{
			if (NDAS_LOGICALDEVICE_TYPE_DVD == pLogDevice->GetType() &&
				GENERIC_WRITE & pLogDevice->GetMountedAccess()) 
			{
				++nRWMountedDVD;
			}
		}
	}
	pLdm->Unlock();

	if (nRWMountedDVD > 0) {
		::SetLastError(NDASHLPSVC_ERROR_NO_MORE_DVD_WRITE_ACCESS_INSTANCE_ALLOWED);
		return FALSE;
	}

	return TRUE;
}

VOID
CNdasLogicalDevice::cpLocateRegContainer()
{
	ximeta::CAutoLock autolock(this);

	//
	// Registry Container
	// HKLM\Software\NDAS\LogDevices\XXXXXXXX
	//

	BOOL fSuccess, fWriteData = TRUE;

	m_dwHashValue = cpGetHashValue();

	while (TRUE) {

		HRESULT hr = ::StringCchPrintf(
			m_szRegContainer, 30, 
			_T("LogDevices\\%08X"), m_dwHashValue);

		_ASSERTE(SUCCEEDED(hr));

		NDAS_LOGICALDEVICE_GROUP ldGroup;
		DWORD cbData = 0;
		fSuccess = _NdasSystemCfg.GetSecureValueEx(
			m_szRegContainer,
			_T("Data"),
			&ldGroup,
			sizeof(ldGroup),
			&cbData);

		if (fSuccess && cbData == sizeof(ldGroup)) {
			if (0 != ::memcmp(&ldGroup, &m_logicalDeviceGroup, sizeof(ldGroup))) {
				// collision on hash value
				// increment the hash value and recalculate
				++m_dwHashValue;
				continue;
			} else {
				fWriteData = FALSE;
			}
		}

		break;
	}


	if (fWriteData) {
		fSuccess = _NdasSystemCfg.SetSecureValueEx(
			m_szRegContainer,
			_T("Data"),
			&m_logicalDeviceGroup,
			sizeof(m_logicalDeviceGroup));
		if (!fSuccess) {
			DBGPRT_WARN_EX(_FT("Writing LDData failed: "));
		}
	}

	DBGPRT_INFO(_FT("Hash Value: %08X\n"), m_dwHashValue);
	DBGPRT_INFO(_FT("RegContainer: %s\n"), m_szRegContainer);
}

DWORD
CNdasLogicalDevice::cpGetHashValue()
{
	ximeta::CAutoLock autolock(this);

	return ::crc32_calc(
		(CONST UCHAR*) &m_logicalDeviceGroup, 
		sizeof(NDAS_LOGICALDEVICE_GROUP));
}

VOID
CNdasLogicalDevice::cpAllocateNdasScsiLocation()
{
	ximeta::CAutoLock autolock(this);

	_ASSERTE(NULL != m_pUnitDevices[0]);
	_ASSERTE(NULL != m_pUnitDevices[0]->GetParentDevice());
	m_NdasScsiLocation.SlotNo = 
		m_pUnitDevices[0]->GetParentDevice()->GetSlotNo() * 10 +
		m_pUnitDevices[0]->GetUnitNo();
}

VOID
CNdasLogicalDevice::cpDeallocateNdasScsiLocation()
{
	ximeta::CAutoLock autolock(this);

	m_NdasScsiLocation.SlotNo = 0;
}

DWORD 
CNdasLogicalDevice::GetUserBlockCount()
{
	ximeta::CAutoLock autolock(this);

	if (!IsComplete()) {
		return 0;
	}

	if (!IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_logicalDeviceGroup.Type)) {
		_ASSERTE(1 == GetUnitDeviceCount());
		CNdasUnitDevice* pUnitDevice = GetUnitDevice(0);
		_ASSERTE(NULL != pUnitDevice);
		if (NULL == pUnitDevice) {
			return 0;
		}
		return pUnitDevice->GetUserBlockCount();
	}

	DWORD dwBlocks = 0;
	for (DWORD i = 0; i < GetUnitDeviceCount(); i++) {

		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);
		_ASSERTE(NULL != pUnitDevice);
		if (NULL == pUnitDevice) {
			return 0;
		}

		_ASSERTE(pUnitDevice->GetType() == NDAS_UNITDEVICE_TYPE_DISK);

		CNdasUnitDiskDevice* pUnitDisk = 
			reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);

		// dwBlocks += pUnitDisk->GetBlocks();

		_ASSERTE(IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_logicalDeviceGroup.Type));

		switch (m_logicalDeviceGroup.Type) {
		case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
			if (0 == i) {
				dwBlocks = pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
			dwBlocks += pUnitDisk->GetUserBlockCount();
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
			if (i % 2) {
				dwBlocks += pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
			if (i != GetUnitDeviceCount() - 1) {
				//
				// do not count parity disk
				//
				dwBlocks += pUnitDisk->GetUserBlockCount();
			}
			break;
		case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
			dwBlocks = pUnitDisk->GetUserBlockCount();
			break;
		default: 
			// not implemented yet : DVD, VDVD, MO, FLASH ...
			_ASSERTE(FALSE);
			break;
		}

	}

	return dwBlocks;

}

//////////////////////////////////////////////////////////////////////////
//
// CNdasLogicalDisk Implementation
//
//////////////////////////////////////////////////////////////////////////

//CNdasLogicalDisk::CNdasLogicalDisk(
//	NDAS_LOGICALDEVICE_ID logDevId,
//	CONST NDAS_LOGICALDEVICE_GROUP& ldGroup) :
//	CNdasLogicalDevice(logDevId, ldGroup)
//{
//	DBGPRT_TRACE(_FT("ctor: %s\n"), ToString());
//}
//
//CNdasLogicalDisk::~CNdasLogicalDisk()
//{
//	DBGPRT_TRACE(_FT("dtor: %s\n"), ToString());
//}
//
//BOOL
//CNdasLogicalDisk::PlugIn(ACCESS_MASK requestingAccess)
//{
//	ximeta::CAutoLock autolock(this);
//
//	BOOL fSuccess(FALSE);
//
//	if (m_status == NDAS_LOGICALDEVICE_STATUS_NOT_INITIALIZED) {
//		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
//		return FALSE;
//	}
//
//	//
//	// only from NOT_MOUNTED_STATUS
//	//
//	if (m_status != NDAS_LOGICALDEVICE_STATUS_UNMOUNTED) {
//		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_NOT_INITIALIZED);
//		return FALSE;
//	}
//
//	if (m_nUnitDeviceInstances != m_logicalDeviceGroup.nUnitDevices) {
//		SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
//		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
//		return FALSE;
//	}
//
//	//
//	// check access permission
//	//
//
//	//
//	// only GENERIC_READ and GENERIC_WRITE are valid
//	//
//	requestingAccess &= (GENERIC_READ | GENERIC_WRITE);
//
//	ACCESS_MASK grantedAccess = GetGrantedAccess();
//	if ((grantedAccess & requestingAccess) != requestingAccess) {
//		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_DENIED);
//		return FALSE;
//	}
//
//	ACCESS_MASK allowingAccess = GetAllowingAccess();
//	if ((requestingAccess & allowingAccess) != requestingAccess) {
//		::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_ACCESS_FAILED);
//		return FALSE;
//	}
//
//	//
//	// Plug In
//	// - NDAS Controller
//	//
//	PCNdasDeviceRegistrar pRegistrar = 	pGetNdasDeviceRegistrar();
//
//	{
//		//
//		// NDFS Version Check
//		// (NDAS IX only broadcasts the primary unit device)
//		// Hence, we cannot check all unit devices,
//		// for compatibility reasons.
//		//
//		PCNdasUnitDevice pUnitDevice = GetUnitDevice(0);
//		if (NULL == pUnitDevice) {
//			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
//			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
//			return FALSE;
//		}
//
//		if (allowingAccess & GENERIC_WRITE) {
//
//			DWORD nROHosts, nRWHosts;
//
//			fSuccess = pUnitDevice->GetHostUsageCount(&nROHosts,&nRWHosts, TRUE);
//			if (!fSuccess) {
//				::SetLastError(NDASHLPSVC_ERROR_NDAS_DEVICE_COMMUNICATION_FAILURE);
//				return FALSE;
//			}
//
//			if (nRWHosts > 0) {
//				fSuccess = pUnitDevice->CheckNDFSCompatibility();
//				if (!fSuccess) {
//					::SetLastError(NDASHLPSVC_ERROR_INCOMPATIBLE_HOSTS_FOR_MULTIPLE_WRITE_ACCESS);
//					return FALSE;
//				}
//			}
//		}
//	}
//
//	//
//	// Resetting an event always succeeds if the handle is valid
//	//
//	fSuccess = ::ResetEvent(m_hDisconnectedEvent);
//	_ASSERTE(fSuccess);
//
//	fSuccess = ::ResetEvent(m_hAlarmEvent);
//	_ASSERTE(fSuccess);
//
//	//
//	// Add Target
//	// - Add a disk device to the NDAS controller
//	//
//
//	SIZE_T cbAddTargetDataSize =  
//		sizeof(LANSCSI_ADD_TARGET_DATA) - sizeof(LSBUS_UNITDISK) +
//		m_logicalDeviceGroup.nUnitDevices * sizeof(LSBUS_UNITDISK);
//
//	PLANSCSI_ADD_TARGET_DATA pAddTargetData = 
//		(PLANSCSI_ADD_TARGET_DATA) ::HeapAlloc(
//		::GetProcessHeap(),
//		HEAP_ZERO_MEMORY,
//		cbAddTargetDataSize);
//
//	AutoProcessHeap autoHeap = (PVOID) pAddTargetData;
//
//	if (NULL == pAddTargetData) {
//		// TODO: Out of memory
//		return FALSE;
//	}
//
//	UCHAR ucTargetType = pConvertToLSBusTargetType(m_logicalDeviceGroup.Type);
//
//	pAddTargetData->ulSize = cbAddTargetDataSize;
//	pAddTargetData->ulSlotNo = m_NdasScsiLocation.SlotNo;
//	pAddTargetData->ulTargetBlocks = 0; // initialization and will be added
//	pAddTargetData->DesiredAccess = requestingAccess;
//	pAddTargetData->ulNumberOfUnitDiskList = m_logicalDeviceGroup.nUnitDevices;
//	pAddTargetData->ucTargetType = ucTargetType;
//
//	//
//	// Set Content Encryption from the primary unit device
//	//
//	{
//		CNdasUnitDevice* pUnitDevice = GetUnitDevice(0);
//		if (NULL == pUnitDevice) {
//			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
//			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
//			return FALSE;
//		}
//
//		if (NDAS_UNITDEVICE_TYPE_DISK != pUnitDevice->GetType()) {
//			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_INVALID_MEMBER);
//			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
//			return FALSE;
//		}
//
//		CNdasUnitDiskDevice* pUnitDiskDevice = 
//			reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);
//
//		CONST NDAS_CONTENT_ENCRYPT encryption = pUnitDiskDevice->GetEncryption();
//
//		pAddTargetData->CntEcrKeyLength = encryption.KeyLength;
//		pAddTargetData->CntEcrMethod = encryption.Method;
//		::CopyMemory(
//			pAddTargetData->CntEcrKey,
//			encryption.Key,
//			encryption.KeyLength);
//
//	}
//
//	for (DWORD i = 0; i < GetUnitDeviceCount(); ++i) {
//
//		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);
//
//		if (NULL == pUnitDevice) {
//			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_MISSING_MEMBER);
//			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
//			return FALSE;
//		}
//
//		if (NDAS_UNITDEVICE_TYPE_DISK != pUnitDevice->GetType()) {
//			SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_INVALID_MEMBER);
//			::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MEMBER_MISSING);
//			return FALSE;
//		}
//
//		PCNdasDevice pDevice = pUnitDevice->GetParentDevice();
//
//		CNdasUnitDiskDevice* pUnitDiskDevice = 
//			reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);
//
//		PLSBUS_UNITDISK pud = &pAddTargetData->UnitDiskList[i];
//
//		::CopyMemory(
//			pud->Address.Node, 
//			pDevice->GetRemoteLpxAddress().Node, 
//			sizeof(pud->Address.Node));
//		pud->Address.Port = htons(NDAS_DEVICE_LPX_PORT);
//
//		::CopyMemory(
//			pud->NICAddr.Node, 
//			pDevice->GetLocalLpxAddress().Node, 
//			sizeof(pud->NICAddr.Node));
//
//		pud->NICAddr.Port = htons(0); // should be zero
//
//		pud->iUserID = (requestingAccess & GENERIC_WRITE) ? 
//			READ_WRITE_USER_ID : READ_ONLY_USER_ID;
//		pud->iPassword = pDevice->GetHWPassword();
//
//		// pud->ulUnitBlocks = pUnitDiskDevice->GetBlocks();
//		pud->ulUnitBlocks = pUnitDiskDevice->GetUserBlockCount();
//		pud->ulPhysicalBlocks = pUnitDiskDevice->GetPhysicalBlockCount();
//		pud->ucUnitNumber = pUnitDevice->GetUnitNo();
//		pud->ucHWType = pDevice->GetHWType();
//		pud->ucHWVersion = pDevice->GetHWVersion();
//		// pAddTargetData->ulTargetBlocks += pud->ulUnitBlocks;
//		pud->IsWANConnection = FALSE;
//
//		// ulTargetBlocks & additional disk information
//		switch(ucTargetType)
//		{
//		case NDASSCSI_TYPE_DISK_RAID1:
//			_ASSERT(NULL != pUnitDiskDevice->GetAddTargetInfo());
//			::CopyMemory(
//				&pud->RAID1, 
//				pUnitDiskDevice->GetAddTargetInfo(), 
//				sizeof(INFO_RAID));
//
//			if(0 == pud->RAID1.SectorsPerBit)
//			{
//				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_INVALID_BIND_INFORMATION);
//				return FALSE;
//			}
//			break;
//		case NDASSCSI_TYPE_DISK_NORMAL:
//		case NDASSCSI_TYPE_DISK_MIRROR: // count only master's
//		case NDASSCSI_TYPE_DISK_AGGREGATION:
//		case NDASSCSI_TYPE_DISK_RAID0:
//		case NDASSCSI_TYPE_DISK_RAID4:
//			break;
//		default:
//			// Other types should not be here!
//			_ASSERTE(FALSE);
//			break;
//		}
//
//		// check if last DIB information is same with current one
//		{
//			PCNdasUnitDevice pUnitDeviceNow;
//
//			CNdasUnitDeviceCreator udCreator(*pDevice, pUnitDevice->GetUnitNo());
//			pUnitDeviceNow = udCreator.CreateUnitDevice();
//
//			if(NULL == pUnitDeviceNow)
//			{
//				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
//				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION);
//				return FALSE;
//			}
//
//			if (pUnitDevice->GetType() != pUnitDeviceNow->GetType()) {
//				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
//				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION);
//				delete pUnitDeviceNow;
//				return FALSE;
//			}
//
//			CNdasUnitDiskDevice* pUnitDiskDeviceNow = 
//				reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDeviceNow);
//
//			if(!pUnitDiskDevice->HasSameDIBInfo(*pUnitDiskDeviceNow))
//			{
//				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
//				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_MODIFIED_BIND_INFORMATION);
//				delete pUnitDeviceNow;
//				return FALSE;
//			}
//		}
//		
//		// check if bitmap status in RAID 1, 4. Do not plug in
//
//		{
//			if (NDAS_UNITDEVICE_TYPE_DISK == pUnitDevice->GetType() &&
//				(NDASSCSI_TYPE_DISK_RAID1== ucTargetType ||
//				NDASSCSI_TYPE_DISK_RAID4 == ucTargetType) &&
//				!pUnitDiskDevice->IsBitmapClean())
//			{
//				pDevice->InvalidateUnitDevice(pUnitDevice->GetUnitNo());
//				::SetLastError(NDASHLPSVC_ERROR_NDAS_LOGICALDEVICE_CORRUPTED_BIND_INFORMATION);
//				return FALSE;
//			}
//		}
//	}
//
//	pAddTargetData->ulTargetBlocks = GetUserBlockCount();
//	DWORD dwMaxRequestBlocks = CalcMaxRequestBlocks();
//
//	DBGPRT_INFO(
//		_FT("LsBusCtlPluginEx(Location %s, MaxReqBlock %d, DisEvt %p, RecEvt %p).)\n"),
//		m_NdasScsiLocation.ToString(), 
//		dwMaxRequestBlocks, 
//		m_hDisconnectedEvent, 
//		m_hAlarmEvent);
//
//	// make sure
//	SetReconnectFlag(FALSE);
//	SetDisconnected(FALSE);
//
//	fSuccess = LsBusCtlPlugInEx(
//		m_NdasScsiLocation.SlotNo,
//		dwMaxRequestBlocks,
//		m_hDisconnectedEvent,
//		m_hAlarmEvent);
//
//	if (!fSuccess) {
//		DBGPRT_ERR_EX(_FT("LsBusCtlPlugInEx failed - Error 0x%08x\n"));
//		return FALSE;
//	}
//
//	DBGPRT_INFO(_FT("LsBusCtlPlugInEx succeeded.\n"));
//
//	BEGIN_DBGPRT_BLOCK_INFO()
//		DBGPRT_INFO(_FT("LsBusCtlAddTarget(pAddTargetData)\n"));
//		DPType(XDebug::OL_INFO, pAddTargetData, cbAddTargetDataSize);
//	END_DBGPRT_BLOCK()
//
//	fSuccess = LsBusCtlAddTarget(pAddTargetData);
//	if (!fSuccess) {
//		DBGPRT_ERR_EX(_FT("AddTarget failed: "));
//		::Sleep(1000);
//		(VOID) LsBusCtlEject(m_NdasScsiLocation.SlotNo);
//		cpDeallocateNdasScsiLocation();
//		return FALSE;
//	}
//
//	DBGPRT_INFO(_FT("LsBusCtlAddTarget succeeded.\n"));
//
//	SetMountedAccess(requestingAccess);
//	SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);
//
//	DBGPRT_INFO(_FT("Plugged in successfully at %s.\n"), 
//		m_NdasScsiLocation.ToString());
//
//	return TRUE;
//}
//
//DWORD 
//CNdasLogicalDisk::GetUserBlockCount()
//{
//	DWORD dwBlocks(0);
//
//	for (DWORD i = 0; i < GetUnitDeviceCount(); i++) {
//
//		CNdasUnitDevice* pUnitDevice = GetUnitDevice(i);
//		if (NULL == pUnitDevice) {
//			return 0;
//		}
//
//		_ASSERTE(pUnitDevice->GetType() == NDAS_UNITDEVICE_TYPE_DISK);
//
//		CNdasUnitDiskDevice* pUnitDisk = 
//			reinterpret_cast<CNdasUnitDiskDevice*>(pUnitDevice);
//
//		// dwBlocks += pUnitDisk->GetBlocks();
//
//		_ASSERTE(IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(m_logicalDeviceGroup.Type));
//
//		switch (m_logicalDeviceGroup.Type) {
//		case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
//			if(0 == i) {
//				dwBlocks = pUnitDisk->GetUserBlockCount();
//			}
//			break;
//		case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
//		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
//			dwBlocks += pUnitDisk->GetUserBlockCount();
//			break;
//		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
//			if(i % 2) {
//				dwBlocks += pUnitDisk->GetUserBlockCount();
//			}
//			break;
//		case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
//			if(i != GetUnitDeviceCount() - 1) { // do not count parity disk
//				dwBlocks += pUnitDisk->GetUserBlockCount();
//			}
//			break;
//		case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
//			dwBlocks = pUnitDisk->GetUserBlockCount();
//			break;
//		default: // not implemented yet : DVD, VDVD, MO, FLASH ...
//			_ASSERTE(FALSE);
//			break;
//		}
//
//	}
//
//	return dwBlocks;
//}

static BOOL 
pCheckStatusValidity(
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
		case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
			return TRUE;
		}
	}
	return FALSE;
}

static UCHAR 
pConvertToLSBusTargetType(NDAS_LOGICALDEVICE_TYPE logDevType)
{
	switch (logDevType) {
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		return NDASSCSI_TYPE_DISK_NORMAL;
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		return NDASSCSI_TYPE_DISK_MIRROR;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		return NDASSCSI_TYPE_DISK_AGGREGATION;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID0:
		return NDASSCSI_TYPE_DISK_RAID0;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
		return NDASSCSI_TYPE_DISK_RAID1;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
		return NDASSCSI_TYPE_DISK_RAID4;
	case NDAS_LOGICALDEVICE_TYPE_DVD:
		return NDASSCSI_TYPE_DVD;
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
		return NDASSCSI_TYPE_VDVD;
	case NDAS_LOGICALDEVICE_TYPE_MO:
		return NDASSCSI_TYPE_MO;
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
		return NDASSCSI_TYPE_DISK_NORMAL;
	default:
		_ASSERTE(FALSE);
		return NDASSCSI_TYPE_DISK_NORMAL;
	}
}
