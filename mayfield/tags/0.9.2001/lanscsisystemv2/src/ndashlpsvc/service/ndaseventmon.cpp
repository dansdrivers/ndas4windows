#include "stdafx.h"
#include "ndasdev.h"
#include "ndaseventmon.h"
#include "ndastypeex.h"
#include "ndasinstman.h"
#include "ndaseventpub.h"

#include "lsbusctl.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_LPXCOMM
#include "xdebug.h"

CNdasEventMonitor::
CNdasEventMonitor() :
	m_hHeartbeatMonitorTimer(INVALID_HANDLE_VALUE),
	m_bIterating(FALSE),
	m_hLogDeviceSetChangeEvent(NULL),
	CTask()
{
}

CNdasEventMonitor::
~CNdasEventMonitor()
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
CNdasEventMonitor::
Initialize()
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
CNdasEventMonitor::
Attach(const PCNdasDevice pDevice)
{
	ximeta::CAutoLock autolock(this);

	DPInfo(_FT("Attaching device %s to the monitor\n"),
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	std::pair<PCNdasDeviceSet::iterator,bool> ins = m_hbMonDevices.insert(pDevice);
}

VOID
CNdasEventMonitor::
Detach(const PCNdasDevice pDevice)
{
	ximeta::CAutoLock autolock(this);

	_ASSERTE(!m_bIterating && 
		"You must not call Detach from OnStatusCheck" &&
		"Return TRUE to detach during OnStatusCheck instead!");

	DPInfo(_FT("Detaching device %s from the monitor\n"),
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	m_hbMonDevices.erase(pDevice);
}

VOID
CNdasEventMonitor::
Attach(const PCNdasLogicalDevice pLogDevice)
{
	ximeta::CAutoLock autolock(this);
	DPInfo(_FT("Attaching logical device %s to the monitor\n"),
		pLogDevice->ToString());

	m_vLogDevices.push_back(pLogDevice);

	BOOL fSuccess = ::SetEvent(m_hLogDeviceSetChangeEvent);
	_ASSERT(fSuccess);
}

VOID
CNdasEventMonitor::
Detach(const PCNdasLogicalDevice pLogDevice)
{
	ximeta::CAutoLock autolock(this);

	DPInfo(_FT("Detaching logical device %s from the monitor\n"),
		pLogDevice->ToString());

	for (PCNdasLogicalDeviceVector::iterator itr = m_vLogDevices.begin();
		itr != m_vLogDevices.end(); ++itr)
	{
		if (pLogDevice == *itr) {
			m_vLogDevices.erase(itr);
			break;
		}
	}

	BOOL fSuccess = ::SetEvent(m_hLogDeviceSetChangeEvent);
	_ASSERT(fSuccess);
}

DWORD
CNdasEventMonitor::
OnTaskStart()
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

		BOOL fSuccess = ::ResetEvent(m_hLogDeviceSetChangeEvent);
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
				//
				// Heartbeat Monitor Timer Event
				//
				ximeta::CAutoLock autolock(this);
				PCNdasDeviceSet::const_iterator itr = m_hbMonDevices.begin();
				m_bIterating = TRUE;
				for (; itr != m_hbMonDevices.end();) {
					PCNdasDevice pDevice = *itr;
					BOOL bDetach = pDevice->OnStatusCheck();
					if (bDetach) {
						DPInfo(_FT("Detaching device %s from the monitor\n"),
							CNdasDeviceId(pDevice->GetDeviceId()).ToString());
						itr = m_hbMonDevices.erase(itr);
					} else {
						++itr;
					}
				}
				m_bIterating = FALSE;

			} else if (WAIT_OBJECT_0 + 3 <= dwWaitResult &&
				dwWaitResult < WAIT_OBJECT_0 + 3 + dwLogDevices)
			{
				//
				// Disconnect Event
				//
				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 3);
				PCNdasLogicalDevice pLogDevice = m_vLogDevices[n];

				DPInfo(_FT("Disconnect Event from slot %d: %s\n"),
					pLogDevice->GetSlot(), 
					pLogDevice->ToString());

				//
				// Publish event here
				//
				// TODO: Publish event here
				//
				CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
				_ASSERTE(NULL != pInstMan);

				CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
				_ASSERTE(NULL != pEventPublisher);

				NDAS_LOGICALDEVICE_ID logicalDeviceId = {0};
				logicalDeviceId.SlotNo = pLogDevice->GetSlot();
				logicalDeviceId.TargetId = 0;
				logicalDeviceId.LUN = 0;
				(void) pEventPublisher->LogicalDeviceDisconnected(logicalDeviceId);

				BOOL fSuccess = ::ResetEvent(pLogDevice->GetDisconnectEvent());
				_ASSERTE(fSuccess);

				//
				// Eject device
				//
				DPInfo(_FT("Ejecting disconnected logical device\n"));

				fSuccess = pLogDevice->Eject();
				if (!fSuccess) {
					DPErrorEx(_FT("Eject failed: "));
					DPError(_FT("Trying to unplug...\n"));
					fSuccess = pLogDevice->Unplug();
					if (!fSuccess) {
						DPErrorEx(_FT("Unplugging failed: "));
					}
				}

			} else if (WAIT_OBJECT_0 + 3 + dwLogDevices <= dwWaitResult &&
				dwWaitResult < WAIT_OBJECT_0 + 3 + 2 * dwLogDevices)
			{
				//
				// Alarm Event
				//
				DWORD n = dwWaitResult - (WAIT_OBJECT_0 + 3 + dwLogDevices);
				PCNdasLogicalDevice pLogDevice = m_vLogDevices[n];
				ULONG ulStatus;

				DPInfo(_FT("Alarm Event from slot %d: %s\n"),
					pLogDevice->GetSlot(),
					pLogDevice->ToString());

				LsBusCtlQueryAlarmStatus(pLogDevice->GetSlot(), &ulStatus);
				
				CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
				_ASSERTE(NULL != pInstMan);

				CNdasEventPublisher* pEventPublisher = pInstMan->GetEventPublisher();
				_ASSERTE(NULL != pEventPublisher);

				switch (ulStatus) {
				case ALARM_STATUS_NORMAL:
					{
						DPInfo(_FT("Alarm Normal\n"));
						NDAS_LOGICALDEVICE_ID logicalDeviceId = {0};
						logicalDeviceId.SlotNo = pLogDevice->GetSlot();
						logicalDeviceId.TargetId = 0;
						logicalDeviceId.LUN = 0;
						(void) pEventPublisher->
							LogicalDeviceReconnected(logicalDeviceId);
					}

					break;
				case ALARM_STATUS_START_RECONNECT:
					{
						DPInfo(_FT("Alarm Start Reconnect\n"));
						NDAS_LOGICALDEVICE_ID logicalDeviceId = {0};
						logicalDeviceId.SlotNo = pLogDevice->GetSlot();
						logicalDeviceId.TargetId = 0;
						logicalDeviceId.LUN = 0;
						(void) pEventPublisher->
							LogicalDeviceReconnecting(logicalDeviceId);
					}
					break;
				case ALARM_STATUS_FAIL_RECONNECT: // obsolete
					DPInfo(_FT("Alarm Failed Reconnecting\n"));
					break;
				default:
					DPWarning(_FT("Unknown alarm status: %d\n"), ulStatus);
				}

				//
				// TODO: Publish event here
				//

				BOOL fSuccess = ::ResetEvent(pLogDevice->GetAlarmEvent());
				_ASSERTE(fSuccess);

			} else {
				_ASSERTE(FALSE);
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
