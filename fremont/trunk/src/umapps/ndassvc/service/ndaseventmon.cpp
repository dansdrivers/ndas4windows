#include "stdafx.h"
#include <ntddscsi.h>
#include <ndasbusctl.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasportctl.h>
#include <ndas/ndasvolex.h>
#include "ndaseventpub.h"
#include "ndasobjs.h"
#include "ndascfg.h"

#include "ndaseventmon.h"

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

HRESULT
CNdasEventMonitor::Initialize()
{
	HRESULT hr;

	// Initialize routine is reentrant
	if (m_HeartbeatMonitorTimer.IsInvalid()) 
	{
		// Auto-reset waitable timer
		m_HeartbeatMonitorTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);
		if (m_HeartbeatMonitorTimer.IsInvalid()) 
		{
			hr = AtlHresultFromLastError();			
			XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
				"Timer creation failed, hr=0x%X\n", 
				hr);
			return hr;
		}
	}
	if (m_NdasLogicalUnitSetChanged.IsInvalid()) 
	{
		m_NdasLogicalUnitSetChanged = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_NdasLogicalUnitSetChanged.IsInvalid()) 
		{
			hr = AtlHresultFromLastError();
			XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
				"Logical device set change event creation failed, hr=0x%X\n",
				hr);
			return hr;
		}
	}
	if (!m_NdasDeviceDataLock.Initialize())
	{
		hr = AtlHresultFromLastError();
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Device ReaderWriterLock initialization failed, hr=0x%X\n",
			hr);
		return hr;
	}
	if (!m_NdasLogicalUnitDataLock.Initialize())
	{
		hr = AtlHresultFromLastError();			
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"LogDevice ReaderWriterLock initialization failed, hr=0x%X\n",
			hr);
		return hr;
	}

	return S_OK;
}

void
CNdasEventMonitor::Attach(INdasDevice* pNdasDevice)
{
	DWORD slotNo;
	COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Attaching ndas device %d to the monitor\n", slotNo);

	// DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_NdasDeviceDataLock);
		m_NdasDevices.Add(pNdasDevice);
	}
	// DEVICE WRITE LOCK REGION
}

void
CNdasEventMonitor::Detach(INdasDevice* pNdasDevice)
{
	DWORD slotNo;
	COMVERIFY(pNdasDevice->get_SlotNo(&slotNo));
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Detaching ndas device %d from the monitor\n", slotNo);

	// DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_NdasDeviceDataLock);
		size_t count = m_NdasDevices.GetCount();
		for (size_t index = 0; index < count; ++index)
		{
			INdasDevice* p = m_NdasDevices.GetAt(index);
			if (pNdasDevice == p)
			{
				m_NdasDevices.RemoveAt(index);
				break;
			}
		}
	}
	// DEVICE WRITE LOCK REGION
}

void
CNdasEventMonitor::Attach(INdasLogicalUnit* pNdasLogicalUnit)
{
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Attaching logical unit %p to the monitor\n",
		pNdasLogicalUnit);

	// LOGICAL DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_NdasLogicalUnitDataLock);
		m_NdasLogicalUnits.Add(pNdasLogicalUnit);
	}
	// LOGICAL DEVICE WRITE LOCK REGION

	XTLVERIFY( ::SetEvent(m_NdasLogicalUnitSetChanged) );
}

void
CNdasEventMonitor::Detach(INdasLogicalUnit* pNdasLogicalUnit)
{
	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION, 
		"Detaching logical unit %p from the monitor\n",
		pNdasLogicalUnit);

	// LOGICAL DEVICE WRITE LOCK REGION
	{
		XTL::CWriterLockHolder holder(m_NdasLogicalUnitDataLock);
		// CInterfaceArray
		size_t count = m_NdasLogicalUnits.GetCount();
		for (size_t index = 0; index < count; ++index)
		{
			INdasLogicalUnit* p = m_NdasLogicalUnits.GetAt(index);
			if (pNdasLogicalUnit == p)
			{
				m_NdasLogicalUnits.RemoveAt(index);
				break;
			}
		}
	}
	// LOGICAL DEVICE WRITE LOCK REGION

	XTLVERIFY( ::SetEvent(m_NdasLogicalUnitSetChanged) );
}

void
CNdasEventMonitor::OnLogicalDeviceAlarmed(INdasLogicalUnit* pNdasLogicalUnit)
{
	NDAS_LOCATION ndasLocation;
	COMVERIFY(pNdasLogicalUnit->get_Id(&ndasLocation));

	NDAS_LOGICALUNIT_ADDRESS ndasLogicalUnitAddress;
	COMVERIFY(pNdasLogicalUnit->get_LogicalUnitAddress(&ndasLogicalUnitAddress));

	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Alarm Event, ndasLocation=%d, logicalUnit=%p\n", 
		ndasLocation, pNdasLogicalUnit);

	if (0 == ndasLocation) 
	{
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Invalid SCSI Location\n");
		XTLASSERT(FALSE);
		return;
	}

	ULONG newAdapterStatus;

	while(TRUE)
	{
		BOOL success = ::NdasBusCtlQueryEvent(
			ndasLocation,
			&newAdapterStatus);

		if (!success) 
		{
			if (::GetLastError() == ERROR_NO_MORE_ITEMS) 
			{
				XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
					"No more events, ignored, ndasLocation=%d\n", 
					ndasLocation);
			} 
			else 
			{
				XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
					"Query status failed on alarm, ignored, ndasLocation=%d\n", 
					ndasLocation);
			}
			return;
		}

		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
			"Logical device alarmed, ndasLocation=%d, adapterStatus=%08X.\n", 
			ndasLocation, newAdapterStatus);

		// Determine whether an alarm will be issued.
		// Only viable alarm will be published
		ULONG currentAdapterStatus;
		COMVERIFY(pNdasLogicalUnit->get_AdapterStatus(&currentAdapterStatus));

		if (pIsViableAlarmStatus(currentAdapterStatus, newAdapterStatus))
		{
			CNdasEventPublisher& epub = pGetNdasEventPublisher();
			NDAS_LOGICALDEVICE_ID logicalUnitId;
			COMVERIFY(pNdasLogicalUnit->get_Id(&logicalUnitId));
			(void) epub.LogicalDeviceAlarmed(
				logicalUnitId, 
				pGetSignificantAlarm(currentAdapterStatus, newAdapterStatus));
		}

		COMVERIFY(pNdasLogicalUnit->put_AdapterStatus(newAdapterStatus));

		if (!ADAPTERINFO_ISSTATUSFLAG(
			newAdapterStatus, 
			NDASSCSI_ADAPTER_STATUSFLAG_NEXT_EVENT_EXIST)) 
		{
			break;
		}
	}
}

void
CNdasEventMonitor::OnLogicalDeviceAlarmedByPnP(
	__in INdasLogicalUnit* pNdasLogicalUnit, 
	__in ULONG NewAdapterStatus)
{
	NDAS_LOCATION ndasLocation;
	COMVERIFY(pNdasLogicalUnit->get_Id(&ndasLocation));

	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Alarm Event, ndasLocation=%08X, logicalUnit=%p\n", 
		ndasLocation, pNdasLogicalUnit);

	if (0 == ndasLocation) 
	{
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Invalid SCSI Location\n");
		XTLASSERT(FALSE);
		return;
	}

	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_INFORMATION,
		"Logical device alarmed by PnP, ndasLocation=%08X, adapterStatus=%08X.\n", 
		ndasLocation, NewAdapterStatus);

	// Determine whether an alarm will be issued.
	// Only viable alarm will be published
	ULONG currentAdapterStatus;
	COMVERIFY(pNdasLogicalUnit->get_AdapterStatus(&currentAdapterStatus));

	if (pIsViableAlarmStatus(currentAdapterStatus, NewAdapterStatus))
	{
		CNdasEventPublisher& epub = pGetNdasEventPublisher();

		if (ADAPTERINFO_ISSTATUS(NewAdapterStatus, NDASSCSI_ADAPTER_STATUS_STOPPED) &&
			ADAPTERINFO_ISSTATUSFLAG(NewAdapterStatus, NDAS_DEVICE_ALARM_STATUSFLAG_ABNORMAL_TERMINAT))
		{
			//
			// Notify abnormal removal of the LU device.
			//
			OnLogicalDeviceDisconnected(pNdasLogicalUnit);
		}
		else
		{
			NDAS_LOGICALDEVICE_ID logicalUnitId;
			COMVERIFY(pNdasLogicalUnit->get_Id(&logicalUnitId));
			(void) epub.LogicalDeviceAlarmed(
				logicalUnitId, 
				pGetSignificantAlarm(currentAdapterStatus, NewAdapterStatus));
		}
	}

	COMVERIFY(pNdasLogicalUnit->put_AdapterStatus(NewAdapterStatus));
}

void 
CNdasEventMonitor::OnLogicalDeviceDisconnected(
	INdasLogicalUnit* pNdasLogicalUnit)
{
	NDAS_LOCATION location;
	COMVERIFY(pNdasLogicalUnit->get_Id(&location));

	XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
		"Disconnect Event, ndasLocation=%d, logicalUnit=%p\n", 
		location, pNdasLogicalUnit);

	NDAS_LOGICALDEVICE_ID logicalDeviceId;
	COMVERIFY(pNdasLogicalUnit->get_Id(&logicalDeviceId));

	CNdasEventPublisher& epub = pGetNdasEventPublisher();
	(void) epub.LogicalDeviceDisconnected(logicalDeviceId);

	CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
	pNdasLogicalUnitPnpSink->OnDisconnected();
}

struct NdasLogicalUnitDisconnectEvent : 
	public std::unary_function<INdasLogicalUnit*, HANDLE>
{
	result_type operator()(argument_type pNdasLogicalUnit) const
	{
		HANDLE h;
		COMVERIFY(pNdasLogicalUnit->get_DisconnectEvent(&h));
		return h;
	}
};

struct NdasLogicalUnitAlarmEvent : 
	public std::unary_function<INdasLogicalUnit*, HANDLE>
{
	result_type operator()(argument_type pNdasLogicalUnit) const
	{
		HANDLE h;
		COMVERIFY(pNdasLogicalUnit->get_AlarmEvent(&h));
		return h;
	}
};

template <typename T>
struct InvokeTimerEventSink :
	public std::unary_function<T*, void>
{
	result_type operator()(argument_type pUnk) const
	{
		CComQIPtr<INdasTimerEventSink> pSink(pUnk);
		ATLASSERT(pSink.p);
		pSink->OnTimer();
	}
};

DWORD
CNdasEventMonitor::ThreadStart(HANDLE hStopEvent)
{
	CCoInitialize coinit(COINIT_MULTITHREADED);

	// 15 sec = 10,000,000 nanosec
	// negative value means relative time
	LARGE_INTEGER liDueTime;
	liDueTime.QuadPart = - 10 * 1000 * 1000;

	BOOL success = ::SetWaitableTimer(
		m_HeartbeatMonitorTimer, 
		&liDueTime, 
		HEARTBEAT_MONITOR_INTERVAL, 
		NULL, 
		NULL, 
		FALSE);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_EVENTMON, TRACE_LEVEL_ERROR,
			"Setting waitable timer failed, error=0x%X\n", GetLastError());
	}

	XTLVERIFY( ::ResetEvent(m_NdasLogicalUnitSetChanged) );

	std::vector<HANDLE> waitHandles;
	waitHandles.reserve(20);

	// Lock-free copy of the devices and logDevices
	CInterfaceArray<INdasDevice> ndasDevices;
	CInterfaceArray<INdasLogicalUnit> ndasLogicalUnits;

	while (true)
	{
		//
		// Copy m_NdasDevices and m_NdasLogicalUnits to devices and logDevices
		// for lock free accesses
		//
		// DEVICE READER LOCK REGION
		{
			XTL::CReaderLockHolder holder(m_NdasDeviceDataLock);
			ndasDevices.Copy(m_NdasDevices);
		}
		{
			XTL::CReaderLockHolder holder(m_NdasLogicalUnitDataLock);
			ndasLogicalUnits.Copy(m_NdasLogicalUnits);
		}
		// DEVICE READER LOCK REGION

		//
		// Recreate wait handles
		//
		size_t ndasLogicalUnitCount = ndasLogicalUnits.GetCount();
		waitHandles.resize(3 + ndasLogicalUnitCount * 2);

		waitHandles[0] = hStopEvent;
		waitHandles[1] = m_NdasLogicalUnitSetChanged;
		waitHandles[2] = m_HeartbeatMonitorTimer;

		// Disconnect events i=[3 ...3+nLogDevices)
		// Alarm Events events i=[3+nLogDevices ... 3+2*nLogDevices)
		for (size_t index = 0; index < ndasLogicalUnitCount; ++index)
		{
			INdasLogicalUnit* pNdasLogicalUnit = ndasLogicalUnits.GetAt(index);
			waitHandles[3 + index] = 
				NdasLogicalUnitDisconnectEvent()(pNdasLogicalUnit);
			waitHandles[3 + index + ndasLogicalUnitCount] = 
				NdasLogicalUnitAlarmEvent()(pNdasLogicalUnit);
		}

		DWORD nWaitHandles = waitHandles.size();

		DWORD waitResult = ::WaitForMultipleObjects(
			nWaitHandles, 
			&waitHandles[0], 
			FALSE, 
			INFINITE);

		if (WAIT_OBJECT_0 == waitResult)
		{
			// Terminate Thread Event
			XTLVERIFY( ::CancelWaitableTimer(m_HeartbeatMonitorTimer) );
			return 0;
		}
		else if (WAIT_OBJECT_0 + 1 == waitResult) 
		{
			// Logical device set change event
			XTLVERIFY( ::ResetEvent(m_NdasLogicalUnitSetChanged) );
			continue;
		} 
		else if (WAIT_OBJECT_0 + 2 == waitResult) 
		{
			// Heartbeat Monitor Timer Event
			AtlForEach(ndasDevices, InvokeTimerEventSink<INdasDevice>());
			AtlForEach(ndasLogicalUnits, InvokeTimerEventSink<INdasLogicalUnit>());
		}
		else if (
			waitResult >= WAIT_OBJECT_0 + 3 &&
			waitResult < WAIT_OBJECT_0 + 3 + ndasLogicalUnitCount)
		{
			XTLVERIFY( ::ResetEvent(waitHandles[waitResult - WAIT_OBJECT_0]) );
			// Disconnect Event
			DWORD n = waitResult - (WAIT_OBJECT_0 + 3);
			OnLogicalDeviceDisconnected(ndasLogicalUnits[n]);
		} 
		else if (
			waitResult >= WAIT_OBJECT_0 + 3 + ndasLogicalUnitCount &&
			waitResult < WAIT_OBJECT_0 + 3 + 2 * ndasLogicalUnitCount)
		{
			XTLVERIFY( ::ResetEvent(waitHandles[waitResult - WAIT_OBJECT_0]) );
			// Alarm Event
			DWORD n = waitResult - (WAIT_OBJECT_0 + 3 + ndasLogicalUnitCount);
			OnLogicalDeviceAlarmed(ndasLogicalUnits[n]);
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
// NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING
// NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT
// NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING
// NDAS_DEVICE_ALARM_STATUSFLAG_ABNORMAL_TERMINAT
// NDAS_DEVICE_ALARM_STATUSFLAG_RAID_FAILURE
// NDAS_DEVICE_ALARM_STATUSFLAG_RAID_NORMAL 
//
//
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
		NDAS_DEVICE_ALARM_STATUSFLAG_ABNORMAL_TERMINAT |
		NDAS_DEVICE_ALARM_STATUSFLAG_RAID_FAILURE |
		NDAS_DEVICE_ALARM_STATUSFLAG_RAID_NORMAL 
		;
	return 
		((NDASSCSI_ADAPTER_STATUSFLAG_MASK & VIABLE_STATUSFLAG_MASK) & ulAdapterStatus) |
		((NDASSCSI_ADAPTER_STATUS_MASK) & ulAdapterStatus);
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
	if (NDASSCSI_ADAPTER_STATUS_INIT == ulOldAdapterStatus && 
		NDASSCSI_ADAPTER_STATUS_RUNNING == ulNewAdapterStatus)
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
	if (FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECOVERING;
	}
	else if (FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_MEMBER_FAULT, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_MEMBER_FAULT;
	}
	else if (FlagIsOn(NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING, ulOldStatus, ulNewStatus))
	{
		return NDAS_DEVICE_ALARM_RECONNECTING;
	}
	else if (FlagIsOff(NDAS_DEVICE_ALARM_STATUSFLAG_RECOVERING, ulOldStatus, ulNewStatus) &&
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
	else if (FlagIsOff(NDAS_DEVICE_ALARM_STATUSFLAG_RECONNECT_PENDING, ulOldStatus, ulNewStatus) &&
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
