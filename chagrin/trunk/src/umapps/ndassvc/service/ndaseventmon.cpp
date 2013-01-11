#include "stdafx.h"
#include "ndasdev.h"
#include "ndaseventmon.h"
#include "ndas/ndastypeex.h"
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndascfg.h"
#include <ndasbusctl.h>
#include <ndas/ndasportctl.h>
#include <ntddscsi.h>
#include <ndas/ndasvolex.h>

#include "trace.h"
#ifdef RUN_WPP
#include "ndaseventmon.tmh"
#endif

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
}

CNdasEventMonitor::~CNdasEventMonitor()
{
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
			XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
				"Timer creation failed, error=0x%X\n", 
				GetLastError());
			return false;
		}
	}
	if (m_hLogDeviceSetChangeEvent.IsInvalid()) 
	{
		m_hLogDeviceSetChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hLogDeviceSetChangeEvent.IsInvalid()) 
		{
			XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
				"Logical device set change event creation failed, error=0x%X\n",
				GetLastError());
			return false;
		}
	}
	if (!m_deviceDataLock.Initialize())
	{
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Device ReaderWriterLock initialization failed, error=0x%X\n",
			GetLastError());
		return false;
	}
	if (!m_logDeviceDataLock.Initialize())
	{
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"LogDevice ReaderWriterLock initialization failed, error=0x%X\n",
			GetLastError());
		return false;
	}

	return true;
}

void
CNdasEventMonitor::Attach(CNdasDevicePtr pDevice)
{
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Attaching device %s to the monitor\n",
		CNdasDeviceId(pDevice->GetDeviceId()).ToStringA());

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
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Detaching device %s from the monitor\n",
		CNdasDeviceId(pDevice->GetDeviceId()).ToStringA());

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
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Attaching logical device %s to the monitor\n",
		pLogDevice->ToStringA());

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
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION, 
		"Detaching logical device %s from the monitor\n",
		pLogDevice->ToStringA());

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
	NDAS_LOCATION ndasLocation = pLogDevice->GetNdasLocation();
	NDAS_LOGICALUNIT_ADDRESS	ndasLogicalUnitAddress;

	ndasLogicalUnitAddress.Address = pLogDevice->GetNdasLogicalUnitAddress();


	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Alarm Event, ndasLocation=%d, logDevice=%s\n", 
		ndasLocation, pLogDevice->ToStringA());

	if (0 == ndasLocation) 
	{
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Invalid SCSI Location\n");
		XTLASSERT(FALSE);
		return;
	}

	ULONG ulAdapterStatus;
	BOOL fSuccess;

	DWORD deviceNumber = pLogDevice->GetDeviceNumberHint();


	XTL::AutoFileHandle storageDeviceHandle;

	if (-1 != deviceNumber)
	{
		storageDeviceHandle = pOpenStorageDeviceByNumber(
			deviceNumber,
			GENERIC_WRITE|GENERIC_READ);
	}

	while(TRUE)
	{
		if(storageDeviceHandle.IsInvalid())
		{
			fSuccess = ::NdasBusCtlQueryEvent(
				ndasLocation,
				&ulAdapterStatus);
		}
		else
		{
		    fSuccess = ::NdasPortCtlQueryEvent(
			    storageDeviceHandle,
			    ndasLogicalUnitAddress,
			    &ulAdapterStatus);
		}

		if (!fSuccess) 
		{
			if(::GetLastError() == ERROR_NO_MORE_ITEMS) {
				XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
					"No more events, ignored, ndasLocation=%d\n", 
					ndasLocation);
			} else {
				XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
					"Query status failed on alarm, ignored, ndasLocation=%d\n", 
					ndasLocation);
			}
			return;
		}

		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
			"Logical device alarmed, ndasLocation=%d, adapterStatus=%08X.\n", 
			ndasLocation, ulAdapterStatus);

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

		if(!ADAPTERINFO_ISSTATUSFLAG(ulAdapterStatus, NDASSCSI_ADAPTERINFO_STATUSFLAG_NEXT_EVENT_EXIST)) {
			break;
		}
	}
}

void 
CNdasEventMonitor::OnLogicalDeviceDisconnected(CNdasLogicalDevicePtr pLogDevice)
{
	NDAS_LOCATION location = pLogDevice->GetNdasLocation();

	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
		"Disconnect Event, ndasLocation=%d, logDevice=%s\n", 
		location, pLogDevice->ToStringA());

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
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Setting waitable timer failed, error=0x%X\n", GetLastError());
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
		NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING |
		NDAS_DEVICE_ALARM_STATUSFLAG_RAID_FAILURE |
		NDAS_DEVICE_ALARM_STATUSFLAG_RAID_NORMAL 
		;
	return 
		((NDASSCSI_ADAPTERINFO_STATUSFLAG_MASK & VIABLE_STATUSFLAG_MASK) & ulAdapterStatus) |
		((NDASSCSI_ADAPTERINFO_STATUS_MASK) & ulAdapterStatus);
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
	if (NDASSCSI_ADAPTERINFO_STATUS_INIT == ulOldAdapterStatus && 
		NDASSCSI_ADAPTERINFO_STATUS_RUNNING == ulNewAdapterStatus)
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
		!(NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING & ulNewStatus) &&
		!(NDAS_DEVICE_ALARM_STATUSFLAG_RAID_FAILURE & ulNewStatus) &&
		(NDAS_DEVICE_ALARM_STATUSFLAG_RAID_NORMAL & ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECOVERED;
	} 
	else if (FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_RAID_FAILURE, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RAID_FAILURE;
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
