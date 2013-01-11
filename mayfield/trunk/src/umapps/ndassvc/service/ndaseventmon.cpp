#include "stdafx.h"
#include "ndasdev.h"
#include "ndaseventmon.h"
#include "ndas/ndastypeex.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndascfg.h"
#include <ndasbusctl.h>
#include "traceflags.h"
namespace { const DWORD XtlTraceCategory = 0x00000001; }

// anonymous namespace
namespace
{

BOOL pIsViableAlarmStatus(ULONG ulOldAdapterStatus, ULONG ulNewAdapterStatus);
ULONG pMaskKnownAdapterStatus(ULONG ulAdapterStatus);
ULONG pGetSignificantAlarm(ULONG ulOldStatus, ULONG ulNewStatus);

}
// anonymous namespace

CNdasEventMonitor::CNdasEventMonitor()
{
	XTLCALLTRACE2(TCEventMon);
}

CNdasEventMonitor::~CNdasEventMonitor()
{
	XTLCALLTRACE2(TCEventMon);
}

bool
CNdasEventMonitor::Initialize()
{
	// Initialize routine is reentrant
	if (m_hHeartbeatMonitorTimer.IsInvalid()) 
	{
		// Auto-reset waitable timer
		m_hHeartbeatMonitorTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);
		if (m_hHeartbeatMonitorTimer.IsInvalid()) 
		{
			XTLTRACE_ERR("Timer creation failed.\n");
			return false;
		}
	}
	if (m_hLogDeviceSetChangeEvent.IsInvalid()) 
	{
		m_hLogDeviceSetChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hLogDeviceSetChangeEvent.IsInvalid()) 
		{
			XTLTRACE_ERR("Logical device set change event creation failed.\n");
			return false;
		}
	}
	if (!m_deviceDataLock.Initialize())
	{
		XTLTRACE_ERR("Device ReaderWriterLock initialization failed.\n");
		return false;
	}
	if (!m_logDeviceDataLock.Initialize())
	{
		XTLTRACE_ERR("LogDevice ReaderWriterLock initialization failed.\n");
		return false;
	}

	return true;
}

void
CNdasEventMonitor::Attach(CNdasDevicePtr pDevice)
{
	XTLTRACE("Attaching device %ws to the monitor\n",
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	// DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_deviceDataLock);
		m_devices.push_back(pDevice);
	}
	// DEVICE WRITE LOCK REGION
}

void
CNdasEventMonitor::Detach(CNdasDevicePtr pDevice)
{
	XTLTRACE("Detaching device %ws from the monitor\n",
		CNdasDeviceId(pDevice->GetDeviceId()).ToString());

	// DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_deviceDataLock);
		CNdasDeviceVector::iterator itr = 
			std::find(
				m_devices.begin(), m_devices.end(), 
				pDevice);
		XTLASSERT(m_devices.end() != itr);
		if (m_devices.end() != itr)
		{
			m_devices.erase(itr);
		}
	}
	// DEVICE WRITE LOCK REGION
}

void
CNdasEventMonitor::Attach(CNdasLogicalDevicePtr pLogDevice)
{
	XTLTRACE("Attaching logical device %ws to the monitor\n", pLogDevice->ToString());

	// LOGICAL DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_logDeviceDataLock);
		m_logDevices.push_back(pLogDevice);
	}
	// LOGICAL DEVICE WRITE LOCK REGION

	XTLVERIFY( ::SetEvent(m_hLogDeviceSetChangeEvent) );
}

void
CNdasEventMonitor::Detach(CNdasLogicalDevicePtr pLogDevice)
{
	XTLTRACE("Detaching logical device %ws from the monitor\n", pLogDevice->ToString());

	// LOGICAL DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_logDeviceDataLock);
		CNdasLogicalDeviceVector::iterator itr =
			std::find(
				m_logDevices.begin(), m_logDevices.end(), 
				pLogDevice);
		XTLASSERT(m_logDevices.end() != itr);
		if (m_logDevices.end() != itr)
		{
			m_logDevices.erase(itr);
		}
	}
	// LOGICAL DEVICE WRITE LOCK REGION

	XTLVERIFY( ::SetEvent(m_hLogDeviceSetChangeEvent) );
}

void
CNdasEventMonitor::OnLogicalDeviceAlarmed(CNdasLogicalDevicePtr pLogDevice)
{
	CNdasScsiLocation ndasScsiLocation = pLogDevice->GetNdasScsiLocation();

	XTLTRACE("Alarm Event from %ws: %ws\n", ndasScsiLocation.ToString(), pLogDevice->ToString());

	if (ndasScsiLocation.IsInvalid()) 
	{
		XTLTRACE("Invalid SCSI Location\n");
		XTLASSERT(FALSE);
		return;
	}

	ULONG ulAdapterStatus;

	BOOL fSuccess = ::LsBusCtlQueryStatus(
		ndasScsiLocation.SlotNo,
		&ulAdapterStatus);

	if (!fSuccess) 
	{
		XTLTRACE_ERR("(%ws) query status failed on alarm, ignored\n", ndasScsiLocation.ToString());
		return;
	}

	XTLTRACE("(%ws) alarmed %08X.\n", ndasScsiLocation.ToString(), ulAdapterStatus);

	// Determine whether an alarm will be issued.
	// Only viable alarm will be published
	if (pIsViableAlarmStatus(pLogDevice->GetAdapterStatus(), ulAdapterStatus))
	{
		CNdasEventPublisher& epub = pGetNdasEventPublisher();
		(void) epub.LogicalDeviceAlarmed(
			pLogDevice->GetLogicalDeviceId(), 
			pGetSignificantAlarm(pLogDevice->GetAdapterStatus(), ulAdapterStatus));
	}

	pLogDevice->SetAdapterStatus(ulAdapterStatus);
}

void 
CNdasEventMonitor::OnLogicalDeviceDisconnected(CNdasLogicalDevicePtr pLogDevice)
{
	CNdasScsiLocation location = pLogDevice->GetNdasScsiLocation();

	XTLTRACE("Disconnect Event from %ws: %ws\n", location.ToString(), pLogDevice->ToString());

	NDAS_LOGICALDEVICE_ID logicalDeviceId = pLogDevice->GetLogicalDeviceId();

	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceDisconnected(logicalDeviceId);

	pLogDevice->OnDisconnected();
}

DWORD
CNdasEventMonitor::ThreadStart(HANDLE hStopEvent)
{
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

	if (!fSuccess) 
	{
		XTLTRACE_ERR("Setting waitable timer failed.\n");
	}

	XTLVERIFY( ::ResetEvent(m_hLogDeviceSetChangeEvent) );

	std::vector<HANDLE> waitHandles;
	waitHandles.reserve(20);

	// Lock-free copy of the devices and logDevices
	CNdasDeviceVector devices;
	CNdasLogicalDeviceVector logDevices;

	while (true)
	{
		//
		// Copy m_devices and m_logDevices to devices and logDevices
		// for lock free accesses
		//
		// DEVICE READER LOCK REGION
		{
			XTL::CReaderLockHolder holder(m_deviceDataLock);

			devices.resize(m_devices.size());
			std::copy(
				m_devices.begin(), m_devices.end(), 
				devices.begin());

			logDevices.resize(m_logDevices.size());
			std::copy(
				m_logDevices.begin(), m_logDevices.end(),
				logDevices.begin());
		}
		// DEVICE READER LOCK REGION

		//
		// Recreate wait handles
		//
		DWORD nLogDevices = logDevices.size();
		waitHandles.resize(3 + nLogDevices * 2);

		waitHandles[0] = hStopEvent;
		waitHandles[1] = m_hLogDeviceSetChangeEvent;
		waitHandles[2] = m_hHeartbeatMonitorTimer;

		// Disconnect events i=[3 ...3+nLogDevices)
		std::transform(
			m_logDevices.begin(), m_logDevices.end(),
			waitHandles.begin() + 3,
			boost::mem_fn(&CNdasLogicalDevice::GetDisconnectEvent));

		// Alarm Events events i=[3+nLogDevices ... 3+2*nLogDevices)
		std::transform(
			m_logDevices.begin(), m_logDevices.end(),
			waitHandles.begin() + 3 + nLogDevices,
			boost::mem_fn(&CNdasLogicalDevice::GetAlarmEvent));

		DWORD nWaitHandles = waitHandles.size();

		DWORD waitResult = ::WaitForMultipleObjects(
			nWaitHandles, 
			&waitHandles[0], 
			FALSE, 
			INFINITE);

		if (WAIT_OBJECT_0 == waitResult)
		{
			// Terminate Thread Event
			XTLVERIFY( ::CancelWaitableTimer(m_hHeartbeatMonitorTimer) );
			return 0;
		}
		else if (WAIT_OBJECT_0 + 1 == waitResult) 
		{
			// Logical device set change event
			XTLVERIFY( ::ResetEvent(m_hLogDeviceSetChangeEvent) );
			continue;
		} 
		else if (WAIT_OBJECT_0 + 2 == waitResult) 
		{
			// Heartbeat Monitor Timer Event
			std::for_each(
				devices.begin(), devices.end(),
				boost::mem_fn(&CNdasDevice::OnPeriodicCheckup));

			// Check the logical devices
			std::for_each(
				logDevices.begin(), logDevices.end(),
				boost::mem_fn(&CNdasLogicalDevice::OnPeriodicCheckup));
		}
		else if (
			waitResult >= WAIT_OBJECT_0 + 3 &&
			waitResult < WAIT_OBJECT_0 + 3 + nLogDevices)
		{
			XTLVERIFY( ::ResetEvent(waitHandles[waitResult - WAIT_OBJECT_0]) );
			// Disconnect Event
			DWORD n = waitResult - (WAIT_OBJECT_0 + 3);
			OnLogicalDeviceDisconnected(logDevices.at(n));
		} 
		else if (
			waitResult >= WAIT_OBJECT_0 + 3 + nLogDevices &&
			waitResult < WAIT_OBJECT_0 + 3 + 2 * nLogDevices)
		{
			XTLVERIFY( ::ResetEvent(waitHandles[waitResult - WAIT_OBJECT_0]) );
			// Alarm Event
			DWORD n = waitResult - (WAIT_OBJECT_0 + 3 + nLogDevices);
			OnLogicalDeviceAlarmed(logDevices.at(n));
		}
		else 
		{
			XTLASSERT(FALSE);
		}
	}
}

namespace
{


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
	// will be suppressed.
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

//#define IS_TURNED_ON(FLAG, OLD_STATUS, NEW_STATUS)  ( !((FLAG) & (OLD_STATUS)) &&  ((FLAG) & NEW_STATUS) )
//#define IS_TURNED_OFF(FLAG, OLD_STATUS, NEW_STATUS) (  ((FLAG) & (OLD_STATUS)) && !((FLAG) & NEW_STATUS) )

__forceinline 
bool FlagIsOn(ULONG Flag, ULONG OldStatus, ULONG NewStatus)
{
	return !(Flag & OldStatus) && (Flag & NewStatus);
}

__forceinline 
bool FlagIsOff(ULONG Flag, ULONG OldStatus, ULONG NewStatus)
{
	return (Flag & OldStatus) && !(Flag & NewStatus);
}

ULONG 
pGetSignificantAlarm(ULONG ulOldStatus, ULONG ulNewStatus)
{
	if(FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECOVERING;
	}
	else if(FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_MEMBER_FAULT;
	}
	else if(FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECONNECTING;
	}
	else if(FlagIsOff(NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING, ulOldStatus, ulNewStatus) &&
		!(NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT & ulNewStatus) &&
		!(NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING & ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECOVERRED;
	}
	else if(FlagIsOff(NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING, ulOldStatus, ulNewStatus) &&
		!(NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING & ulNewStatus) &&
		!(NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT & ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECONNECTED;
	}
	else
	{
		return NDAS_DEVICE_ALARM_NORMAL;
	}
}

} // anonymous namespace
