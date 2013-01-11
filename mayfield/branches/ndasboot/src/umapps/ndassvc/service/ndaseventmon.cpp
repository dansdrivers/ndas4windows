#include "stdafx.h"
#include "ndasdev.h"
#include "ndaseventmon.h"
#include "ndastypeex.h"
#include "ndasinstman.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"

#include "lsbusctl.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_EVENTMON
#include "xdebug.h"

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
			DPWarningEx(_FT("Failed to close Heartbeat Monitor Timer Handle: "));
		}
	}

	if (NULL != m_hLogDeviceSetChangeEvent) {
		BOOL fSuccess = ::CloseHandle(m_hLogDeviceSetChangeEvent);
		if (!fSuccess) {
			DPWarningEx(_FT("Failed to close Logical device set change event: "));
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
		DPErrorEx(_FT("Timer creation failed: "));
		return FALSE;
	}

	if (NULL == m_hLogDeviceSetChangeEvent) {
		m_hLogDeviceSetChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	if (NULL == m_hLogDeviceSetChangeEvent) {
		DPErrorEx(_FT("Logical device set change event creation failed: "));
		return FALSE;
	}

	return CTask::Initialize();
}

VOID
CNdasEventMonitor::Attach(CNdasDevice* pDevice)
{
	ximeta::CAutoLock autolock(this);

	pDevice->AddRef();

	DPInfo(_FT("Attaching device %s to the monitor\n"),
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	std::pair<PCNdasDeviceSet::iterator,bool> ins = m_hbMonDevices.insert(pDevice);
}

VOID
CNdasEventMonitor::Detach(CNdasDevice* pDevice)
{
	ximeta::CAutoLock autolock(this);

	_ASSERTE(!m_bIterating && 
		"You must not call Detach from OnStatusCheck" &&
		"Return TRUE to detach during OnStatusCheck instead!");

	DPInfo(_FT("Detaching device %s from the monitor\n"),
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	PCNdasDeviceSet::size_type nErased = m_hbMonDevices.erase(pDevice);
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

	DPInfo(_FT("Attaching logical device %s to the monitor\n"),
		pLogDevice->ToString());

	m_vLogDevices.push_back(pLogDevice);

	BOOL fSuccess = ::SetEvent(m_hLogDeviceSetChangeEvent);
	_ASSERT(fSuccess);
}

VOID
CNdasEventMonitor::Detach(CNdasLogicalDevice* pLogDevice)
{
	ximeta::CAutoLock autolock(this);

	DPInfo(_FT("Detaching logical device %s from the monitor\n"),
		pLogDevice->ToString());

	for (PCNdasLogicalDeviceVector::iterator itr = m_vLogDevices.begin();
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

	PCNdasLogicalDevice pLogDevice = NULL;

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

	DPInfo(_FT("Alarm Event from %s: %s\n"),
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

	ULONG ulStatus;

	fSuccess = ::LsBusCtlQueryStatus(
		ndasScsiLocation.SlotNo,
		&ulStatus);

	if (!fSuccess) {
		DPErrorEx(_FT("Unable to get alarm status, Ignored: "));
		return TRUE;
	}

	CNdasEventPublisher* pEventPublisher = pGetNdasEventPublisher();

	(VOID) pEventPublisher->LogicalDeviceAlarmed(pLogDevice->GetLogicalDeviceId(), ulStatus);

	if(ADAPTERINFO_ISSTATUSFLAG(ulStatus, ADAPTERINFO_STATUSFLAG_MEMBER_FAULT))
		pLogDevice->SetAllUnitDevicesFault();

	return TRUE;

	// check disconnected
	if(!ADAPTERINFO_ISSTATUS(ulStatus, ADAPTERINFO_STATUS_RUNNING))
	{
		// not impossible, but ultra rare (if all disks are broken at once or power down)
		// OnLogicalDeviceDisconnected will process it
		DPWarning(_FT("Abnormal status : %d\n"), ulStatus);

		// THIS HAPPENS whenever there is an abnormal disconnection
		// _ASSERTE(FALSE);
	}
	else // ADAPTERINFO_STATUS_RUNNING
	{		
		if(ADAPTERINFO_ISSTATUSFLAG(ulStatus, ADAPTERINFO_STATUSFLAG_MEMBER_FAULT))
		{
			// emergency
			pLogDevice->SetReconnectFlag(FALSE);
			// warning on NMT_MIRROR : will not come to here
			// do nothing else NMT_RAID1, NMT_RAID4 : will not come to here


			// AING_TO_DO : raid check
			PLSMPIOCTL_ADAPTERLURINFO	lurFullInfo = NULL;
			fSuccess = ::LsBusCtlQueryMiniportFullInformation(
				ndasScsiLocation.SlotNo,
				&lurFullInfo);

			if (!fSuccess) {
				DPErrorEx(_FT("LsBusCtlQueryMiniportFullInformation failed, Ignored: "));
				return TRUE;
			}

			int i, j;
			PCNdasUnitDevice pUnitDevice;
			BOOL bUnitDeviceBroken = FALSE;

			// see LurTranslateAddTargetDataToLURDesc in lslurn.c for ordering
#define NDAS_UNITDEVICE_TO_LURN_COUNT_RAID1(CNT) (1 + ((CNT) * 3) /2)
#define NDAS_UNITDEVICE_TO_LURN_COUNT_RAID4(CNT) (1 + (CNT))
#define NDAS_UNITDEVICE_TO_LURN_INDEX_RAID1(NUM) (((NUM)/2)*3 +1 + 1 +(NUM)%2)
#define NDAS_UNITDEVICE_TO_LURN_INDEX_RAID4(NUM) (1 + (CNT))
			switch(pLogDevice->GetType())
			{
			case NDAS_LOGICALDEVICE_TYPE_DISK_RAID1:
				// check disk count
				if(NDAS_UNITDEVICE_TO_LURN_COUNT_RAID1(
					pLogDevice->GetUnitDeviceCount()) !=
					lurFullInfo->UnitDiskCnt)
				{
					DPError(_FT("PLSMPIOCTL_ADAPTERLURINFO invalid\n"));
					break;
				}

				for(i = 0; i < pLogDevice->GetUnitDeviceCount(); i++)
				{
					j = NDAS_UNITDEVICE_TO_LURN_INDEX_RAID1(i);
					pUnitDevice = pLogDevice->GetUnitDevice(i);
					// accepts only LURN_IDE_DISK for binding 
					if(lurFullInfo->UnitDisks[j].LurnType != LURN_IDE_DISK)
					{
						DPError(_FT("PLSMPIOCTL_ADAPTERLURINFO invalid\n"));
						break;
					}

					if(!LURN_IS_RUNNING(lurFullInfo->UnitDisks[j].StatusFlags))
						bUnitDeviceBroken = TRUE;
				}

				(VOID) pEventPublisher->
					LogicalDeviceEmergency(pLogDevice->GetLogicalDeviceId());
				break;
			case NDAS_LOGICALDEVICE_TYPE_DISK_RAID4:
				// check disk count
				if(NDAS_UNITDEVICE_TO_LURN_COUNT_RAID4(
					pLogDevice->GetUnitDeviceCount()) !=
					lurFullInfo->UnitDiskCnt)
				{
					DPError(_FT("PLSMPIOCTL_ADAPTERLURINFO invalid\n"));
					break;
				}
				break;
			default:
				break;
			}

			if (NULL != lurFullInfo) {
				HeapFree(GetProcessHeap(), 0, lurFullInfo);
			}
		}
		else if(
			pLogDevice->GetReconnectFlag() && // was reconnecting
			!ADAPTERINFO_ISSTATUSFLAG(ulStatus, ADAPTERINFO_STATUSFLAG_RECONNECT_PENDING)  // now is not reconnecting
			)
		{
			// reconnected
			pLogDevice->SetReconnectFlag(FALSE);

			(VOID) pEventPublisher->
				LogicalDeviceReconnected(pLogDevice->GetLogicalDeviceId());
		}
		else if(
			!pLogDevice->GetReconnectFlag() && // was not reconnecting
			ADAPTERINFO_ISSTATUSFLAG(ulStatus, ADAPTERINFO_STATUSFLAG_RECONNECT_PENDING)  // now reconnecting
			)
		{
			DPInfo(_FT("Alarm Start Reconnect\n"));

			pLogDevice->SetReconnectFlag(TRUE);

			(VOID) pEventPublisher->LogicalDeviceReconnecting(
				pLogDevice->GetLogicalDeviceId());
		}
		else
		{
			DPInfo(_FT("Healthy status: %d\n"), ulStatus);
		}
	}

	return TRUE;
}

BOOL CNdasEventMonitor::OnLogicalDeviceDisconnected(DWORD nWaitIndex)
{
	ximeta::CAutoLock autolock(this);

	PCNdasLogicalDevice pLogDevice = NULL;

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

	DPInfo(_FT("Disconnect Event from %s: %s\n"),
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
		DPErrorEx(_FT("Setting waitable timer failed: "));
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

		PCNdasLogicalDeviceVector::const_iterator itr = m_vLogDevices.begin();
		for (DWORD i = 3; itr != m_vLogDevices.end(); ++itr, ++i) {
			PCNdasLogicalDevice pLogDevice = *itr;
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
				PCNdasDeviceSet::const_iterator devitr = m_hbMonDevices.begin();
				m_bIterating = TRUE;
				for (; devitr != m_hbMonDevices.end();) {
					PCNdasDevice pDevice = *devitr;
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
				DPErrorEx(_FT("Wait failed: "));
			}

		} while (!bResetLogDeviceSet && !bTerminateThread);

		delete [] hWaitingHandles;

	} while (!bTerminateThread);

	fSuccess = ::CancelWaitableTimer(m_hHeartbeatMonitorTimer);
	if (!fSuccess) {
		DPErrorEx(_FT("Canceling waitable timer failed: "));
	}

	return 0;
}
