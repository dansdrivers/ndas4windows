#include "stdafx.h"
#include "ndasdev.h"
#include "ndaseventmon.h"
#include "ndas/ndastypeex.h"
#include "ndasinstman.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndascfg.h"

#include "lsbusctl.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_EVENTMON
#include "xdebug.h"


static
BOOL 
pIsViableAlarmStatus(ULONG ulOldAdapterStatus, ULONG ulNewAdapterStatus);

static
ULONG 
pMaskKnownAdapterStatus(ULONG ulAdapterStatus);

CNdasEventMonitor::CNdasEventMonitor() :
	m_hHeartbeatMonitorTimer(INVALID_HANDLE_VALUE),
	m_bIterating(FALSE),
	m_hLogDeviceSetChangeEvent(NULL),
	CTask(_T("NdasEventMonitor Task"))
{
}

CNdasEventMonitor::~CNdasEventMonitor()
{
	if (INVALID_HANDLE_VALUE != m_hHeartbeatMonitorTimer) {
		BOOL fSuccess = ::CloseHandle(m_hHeartbeatMonitorTimer);
		if (!fSuccess) {
			DBGPRT_WARN_EX(_FT("Failed to close Heartbeat Monitor Timer Handle: "));
		}
	}

	if (NULL != m_hLogDeviceSetChangeEvent) {
		BOOL fSuccess = ::CloseHandle(m_hLogDeviceSetChangeEvent);
		if (!fSuccess) {
			DBGPRT_WARN_EX(_FT("Failed to close Logical device set change event: "));
		}
	}
}

BOOL
CNdasEventMonitor::Initialize()
{
	//
	// Initialize routine is reentrant one.
	//

	//
	// Auto-reset waitable timer
	//
	if (INVALID_HANDLE_VALUE == m_hHeartbeatMonitorTimer) {
		m_hHeartbeatMonitorTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);
	}

	if (INVALID_HANDLE_VALUE == m_hHeartbeatMonitorTimer) {
		DBGPRT_ERR_EX(_FT("Timer creation failed: "));
		return FALSE;
	}

	if (NULL == m_hLogDeviceSetChangeEvent) {
		m_hLogDeviceSetChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	if (NULL == m_hLogDeviceSetChangeEvent) {
		DBGPRT_ERR_EX(_FT("Logical device set change event creation failed: "));
		return FALSE;
	}

	return CTask::Initialize();
}

VOID
CNdasEventMonitor::Attach(CNdasDevice* pDevice)
{
	ximeta::CAutoLock autolock(this);

	pDevice->AddRef();

	DBGPRT_INFO(_FT("Attaching device %s to the monitor\n"),
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	std::pair<CNdasDeviceSet::iterator,bool> ins = m_hbMonDevices.insert(pDevice);
}

VOID
CNdasEventMonitor::Detach(CNdasDevice* pDevice)
{
	ximeta::CAutoLock autolock(this);

	_ASSERTE(!m_bIterating && 
		"You must not call Detach from OnStatusCheck" &&
		"Return TRUE to detach during OnStatusCheck instead!");

	DBGPRT_INFO(_FT("Detaching device %s from the monitor\n"),
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	CNdasDeviceSet::size_type nErased = m_hbMonDevices.erase(pDevice);
	_ASSERTE(0 == nErased || 1 == nErased);

	if (nErased == 1) {
		pDevice->Release();
	}
}

VOID
CNdasEventMonitor::Attach(CNdasLogicalDevice* pLogDevice)
{
	ximeta::CAutoLock autolock(this);

	pLogDevice->AddRef();

	DBGPRT_INFO(_FT("Attaching logical device %s to the monitor\n"),
		pLogDevice->ToString());

	m_vLogDevices.push_back(pLogDevice);

	BOOL fSuccess = ::SetEvent(m_hLogDeviceSetChangeEvent);
	_ASSERT(fSuccess);
}

VOID
CNdasEventMonitor::Detach(CNdasLogicalDevice* pLogDevice)
{
	ximeta::CAutoLock autolock(this);

	DBGPRT_INFO(_FT("Detaching logical device %s from the monitor\n"),
		pLogDevice->ToString());

	for (CNdasLogicalDeviceVector::iterator itr = m_vLogDevices.begin();
		itr != m_vLogDevices.end(); ++itr)
	{
		if (pLogDevice == *itr) {
			m_vLogDevices.erase(itr);
			pLogDevice->Release();
			break;
		}
	}

	BOOL fSuccess = ::SetEvent(m_hLogDeviceSetChangeEvent);
	_ASSERT(fSuccess);
}

BOOL 
CNdasEventMonitor::OnLogicalDeviceAlarmed(DWORD nWaitIndex)
{
	ximeta::CAutoLock autolock(this);

	CNdasLogicalDevice* pLogDevice = NULL;

	if (nWaitIndex < m_vLogDevices.size()) {
		pLogDevice = m_vLogDevices[nWaitIndex];
	} else {
		_ASSERTE(FALSE);
		return FALSE;
	}

	if (NULL == pLogDevice) {
		_ASSERTE(FALSE);
		return FALSE;
	}

	CNdasScsiLocation ndasScsiLocation = pLogDevice->GetNdasScsiLocation();

	DBGPRT_INFO(_FT("Alarm Event from %s: %s\n"),
		ndasScsiLocation.ToString(),
		pLogDevice->ToString());

	if (ndasScsiLocation.IsInvalid()) {
		DBGPRT_ERR(_FT("Invalid SCSI Location\n"));
		_ASSERTE(FALSE);
		return FALSE;
	}

	//
	// reset the event to prevent consecutive same event pulse
	//
	// should return TRUE
	BOOL fSuccess = ::ResetEvent(pLogDevice->GetAlarmEvent());
	_ASSERTE(fSuccess);

	ULONG ulAdapterStatus;

	fSuccess = ::LsBusCtlQueryStatus(
		ndasScsiLocation.SlotNo,
		&ulAdapterStatus);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("(%s) query status failed on alarm, ignored: "), 
			ndasScsiLocation.ToString());
		return TRUE;
	}

	DBGPRT_INFO(_FT("(%s) alarmed %08X.\n"), 
		ndasScsiLocation.ToString(), ulAdapterStatus);

	// Determine whether an alarm will be issued.
	// Only viable alarm will be published
	if(pIsViableAlarmStatus(pLogDevice->GetAdapterStatus(), ulAdapterStatus))
	{
		CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();
		(VOID) pEventPublisher->LogicalDeviceAlarmed(
			pLogDevice->GetLogicalDeviceId(), ulAdapterStatus);
	}

	pLogDevice->SetAdapterStatus(ulAdapterStatus);

	return TRUE;
}

BOOL CNdasEventMonitor::OnLogicalDeviceDisconnected(DWORD nWaitIndex)
{
	ximeta::CAutoLock autolock(this);

	CNdasLogicalDevice* pLogDevice = NULL;

	if (nWaitIndex < m_vLogDevices.size()) {
		pLogDevice = m_vLogDevices[nWaitIndex];
	} else {
		_ASSERTE(FALSE);
		return TRUE;
	}

	if (NULL == pLogDevice) {
		_ASSERTE(FALSE);
		return TRUE;
	}

	CNdasScsiLocation location = pLogDevice->GetNdasScsiLocation();

	DBGPRT_INFO(_FT("Disconnect Event from %s: %s\n"),
		location.ToString(),
		pLogDevice->ToString());

	//
	// reset the event to prevent consecutive same event pulse
	//
	BOOL fSuccess = ::ResetEvent(pLogDevice->GetDisconnectEvent());
	_ASSERTE(fSuccess);

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();

	NDAS_LOGICALDEVICE_ID logicalDeviceId = pLogDevice->GetLogicalDeviceId();

	(VOID) pEventPublisher->LogicalDeviceDisconnected(logicalDeviceId);

	pLogDevice->OnDisconnected();

	return TRUE;
}

void NdasLogicalDeviceStatusCheck(CNdasLogicalDevice* pLogDevice)
{
	//
	// Clear the risky mount flag 
	// in LOGICALDEVICE_RISK_MOUNT_INTERVAL after mounting
	//

	if (pLogDevice->IsRiskyMount() && 0 != pLogDevice->GetMountTick()) {
		DWORD dwBiased = ::GetTickCount() - pLogDevice->GetMountTick();
		//
		// In case of rollover (in 19 days),
		// it is safe to clear that flag.
		// This is not a strict time tick check
		//
		if (dwBiased > CNdasEventMonitor::LOGICALDEVICE_RISK_MOUNT_INTERVAL) {
			pLogDevice->SetRiskyMountFlag(FALSE);
		}
	}

	//
	// During mount pending, NDAS SCSI is not available until the user
	// accept the warning message of non-signed driver
	//
	// This may cause the logical device being unmounted. 
	//

	if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == pLogDevice->GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == pLogDevice->GetStatus())
	{

		CNdasScsiLocation location = pLogDevice->GetNdasScsiLocation();

		BOOL fAlive, fAdapterError;

		BOOL fSuccess = ::LsBusCtlQueryNodeAlive(
			location.SlotNo, 
			&fAlive, 
			&fAdapterError);

		//
		// if LsBusCtlQueryNodeAlive fails, 
		// there may be no lanscsibus device instance...
		//

		if (!fSuccess) {

			DBGPRT_ERR_EX(_FT("LsBusCtlQueryNodeAlive at %s failed: "), location.ToString());

		} else {

			if (!fAlive) {
				pLogDevice->OnDeviceStatusFailure();
			}

//			if (fAdapterError) {
//				DBGPRT_ERR_EX(_FT("LsBusCtlQueryNodeAlive reported an adapter error.\n"));
//				pLogDevice->SetLastDeviceError(NDAS_LOGICALDEVICE_ERROR_FROM_DRIVER);
//			}

		}

	}

}

DWORD
CNdasEventMonitor::OnTaskStart()
{
	
	BOOL bTerminateThread(FALSE);

	// 15 sec = 10,000,000 nanosec
	// negative value means relative time
	LARGE_INTEGER liDueTime;
	liDueTime.QuadPart = - 10 * 1000 * 1000;

	BOOL fSuccess = ::SetWaitableTimer(
		m_hHeartbeatMonitorTimer, 
		&liDueTime, 
		HEARTBEAT_MONITOR_INTERVAL, 
		NULL, 
		NULL, 
		FALSE);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Setting waitable timer failed: "));
	}

	do {

		//
		// Lock against m_vLogDevice set change
		//
		this->Lock();

		DWORD dwLogDevices = m_vLogDevices.size();
		DWORD dwHandles = 3 + 2 * dwLogDevices;
		HANDLE* hWaitingHandles = new HANDLE[dwHandles];

		hWaitingHandles[0] = m_hTaskTerminateEvent;
		hWaitingHandles[1] = m_hLogDeviceSetChangeEvent;
		hWaitingHandles[2] = m_hHeartbeatMonitorTimer;

		CNdasLogicalDeviceVector::const_iterator itr = m_vLogDevices.begin();
		for (DWORD i = 3; itr != m_vLogDevices.end(); ++itr, ++i) {
			CNdasLogicalDevice* pLogDevice = *itr;
			hWaitingHandles[i] = pLogDevice->GetDisconnectEvent();
			hWaitingHandles[dwLogDevices + i] = pLogDevice->GetAlarmEvent();
		}
		this->Unlock();

		fSuccess = ::ResetEvent(m_hLogDeviceSetChangeEvent);
		_ASSERTE(fSuccess);

		BOOL bResetLogDeviceSet(FALSE);

		do {

			DWORD dwWaitResult = ::WaitForMultipleObjects(
				dwHandles, 
				hWaitingHandles, 
				FALSE, 
				INFINITE);

			if (WAIT_OBJECT_0 == dwWaitResult) {
				//
				// Terminate Thread Event
				//
				bTerminateThread = TRUE;
			} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {
				//
				// LogicalDeviceSetChange Event
				//
				bResetLogDeviceSet = TRUE;
			} else if (WAIT_OBJECT_0 + 2 == dwWaitResult) {

				ximeta::CAutoLock autolock(this);

				//
				// Heartbeat Monitor Timer Event
				//
				CNdasDeviceSet::const_iterator devitr = m_hbMonDevices.begin();
				m_bIterating = TRUE;
				for (; devitr != m_hbMonDevices.end();) {
					CNdasDevice* pDevice = *devitr;
					BOOL fDetach = pDevice->OnStatusCheck();
					if (fDetach) {
						devitr = m_hbMonDevices.erase(devitr);
						pDevice->Release();
					} else {
						++devitr;
					}
				}
				m_bIterating = FALSE;

				//
				// Check the logical devices
				//
				std::for_each(
					m_vLogDevices.begin(),
					m_vLogDevices.end(),
					NdasLogicalDeviceStatusCheck);

			} else if (WAIT_OBJECT_0 + 3 <= dwWaitResult &&
				dwWaitResult < WAIT_OBJECT_0 + 3 + dwLogDevices)
			{
				//
				// Disconnect Event
				//
				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 3);
				BOOL fHandled = OnLogicalDeviceDisconnected(n);
				if (!fHandled) {
					fSuccess = ::ResetEvent(hWaitingHandles[dwWaitResult - WAIT_OBJECT_0]);
					_ASSERTE(fSuccess);
				}

			} else if (WAIT_OBJECT_0 + 3 + dwLogDevices <= dwWaitResult &&
				dwWaitResult < WAIT_OBJECT_0 + 3 + 2 * dwLogDevices)
			{
				//
				// Alarm Event
				//
				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 3 + dwLogDevices);
				BOOL fHandled = OnLogicalDeviceAlarmed(n);
				if (!fHandled) {
					fSuccess = ::ResetEvent(hWaitingHandles[dwWaitResult - WAIT_OBJECT_0]);
					_ASSERTE(fSuccess);
				}

			} else {
//				_ASSERTE(FALSE);
				// Some handles may be already invalid.
				// LogicalDeviceSetChange Event
				//
				bResetLogDeviceSet = TRUE;
				DBGPRT_ERR_EX(_FT("Wait failed: "));
			}

		} while (!bResetLogDeviceSet && !bTerminateThread);

		delete [] hWaitingHandles;

	} while (!bTerminateThread);

	fSuccess = ::CancelWaitableTimer(m_hHeartbeatMonitorTimer);
	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Canceling waitable timer failed: "));
	}

	return 0;
}

//
// Status flags of the alarm from the NDAS SCSI controller 
// spans more flags defined in 'ndastype.h'.
// 'ndastype.h' defines the following type only at this time.
// 
// NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING
// NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT
// NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING
//
// To mitigate irrelavant alarms we supresses non-exposed alarms
// (Default configuration)
//

ULONG 
pMaskKnownAdapterStatus(ULONG ulAdapterStatus)
{
	const static ULONG VIABLE_STATUSFLAG_MASK = 
		NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING |
		NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT |
		NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING;
	return 
		((ADAPTERINFO_STATUSFLAG_MASK & VIABLE_STATUSFLAG_MASK) & ulAdapterStatus) |
		((ADAPTERINFO_STATUS_MASK) & ulAdapterStatus);
}

BOOL 
pIsViableAlarmStatus(ULONG ulOldAdapterStatus, ULONG ulNewAdapterStatus)
{
	BOOL fDontSupressAlarms = NdasServiceConfig::Get(nscDontSupressAlarms);
	if (fDontSupressAlarms)
	{
		return (ulOldAdapterStatus != ulNewAdapterStatus);
	}

	// 0 is an initial status and the first adapter status
	// will be supressed.
	if (ADAPTERINFO_STATUS_INIT == ulOldAdapterStatus && 
		ADAPTERINFO_STATUS_RUNNING == ulNewAdapterStatus)
	{
		return FALSE;
	}

	// Otherwise, compare the viability with masks
	ULONG ulOldStatus = pMaskKnownAdapterStatus(ulOldAdapterStatus);
	ULONG ulNewStatus = pMaskKnownAdapterStatus(ulNewAdapterStatus);

	return (ulOldStatus != ulNewStatus);
}

