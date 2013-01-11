#include "stdafx.h"
#include <boost/shared_ptr.hpp>
#include <boost/mem_fn.hpp>
#include <ndas/ndastypeex.h>
#include <ndas/ndaseventex.h>
#include <ndas/ndastype_str.h>
#include <ndas/ndasevent_str.h>
#include <xtl/xtlstr.h>
#include <xtl/xtltrace.h>

#include "ndaseventpub.h"
#include "ndaseventsub.h"
#include "ndascfg.h"
#include "traceflags.h"

typedef boost::shared_ptr<CNdasEventSubscriber> CNdasEventSubscriberPtr; 
typedef std::vector<CNdasEventSubscriberPtr> CNdasEventSubscriberVector;
typedef CNdasEventSubscriberVector::iterator CNdasEventSubscriberIterator;
typedef CNdasEventSubscriberVector::const_iterator CNdasEventSubscriberConstIterator;

namespace // anonymous namespace
{

	class BeginWriteToSubscriber : 
		public std::unary_function<CNdasEventSubscriber,bool>
	{
	public:
		BeginWriteToSubscriber(const NDAS_EVENT_MESSAGE& msg) : msg(msg) {}
		bool operator()(CNdasEventSubscriberPtr& subscriber) const {
			return subscriber->BeginWriteMessage(msg);
		}
	private:
		const NDAS_EVENT_MESSAGE& msg;
	};

	void PublishEvent(
		CNdasEventSubscriberVector& subscribers, 
		const NDAS_EVENT_MESSAGE& msg);

	bool SendInitialVersionInfo(CNdasEventSubscriberPtr subscriber);

} // anonymous namespace

CNdasEventPublisher::CNdasEventPublisher() :
	PeriodicEventInterval(NdasServiceConfig::Get(nscPeriodicEventInterval))
{
}

bool
CNdasEventPublisher::Initialize()
{
	const DWORD MaxQueueSize = 255;
	if (m_hSemQueue.IsInvalid()) 
	{
		m_hSemQueue = ::CreateSemaphore(NULL, 0, MaxQueueSize, NULL);
		if (m_hSemQueue.IsInvalid()) 
		{
			return false;
		}
	}
	return true;
}

DWORD
CNdasEventPublisher::ThreadStart(HANDLE hStopEvent)
{
	XTLASSERT(!m_hSemQueue.IsInvalid() && "Don't forget to call initialize().");

	CNdasEventPreSubscriber presubscriber(
		NDAS_EVENT_PIPE_NAME, MAX_NDAS_EVENT_PIPE_INSTANCES);

	if (!presubscriber.Initialize())
	{
		XTLTRACE_ERR("EventPreSubscriber initialization failed.\n");
		return 1;
	}

	if (!presubscriber.BeginWaitForConnection())
	{
		XTLTRACE_ERR("EventPreSubscriber connection failed.\n");
		return 1;
	}

	CNdasEventSubscriberVector subscribers;
	subscribers.reserve(MAX_NDAS_EVENT_PIPE_INSTANCES);

	std::vector<HANDLE> waitHandles;
	waitHandles.resize(3);
	waitHandles[0] = hStopEvent;
	waitHandles[1] = m_hSemQueue;
	waitHandles[2] = presubscriber.GetWaitHandle();
	DWORD nWaitHandles = waitHandles.size();

	DWORD nSubscribers = subscribers.size();

	while (true)
	{
		DWORD waitResult = ::WaitForMultipleObjects(
			nWaitHandles,
			&waitHandles[0],
			FALSE,
			PeriodicEventInterval);

		if (presubscriber.GetPipeHandle() == INVALID_HANDLE_VALUE &&
			subscribers.size() != nSubscribers)
		{
			// in case pre-subscriber is not available 
			// as maximum connection has been reached,
			// we should try it again.
			if (presubscriber.ResetPipeHandle())
			{
				if (!presubscriber.BeginWaitForConnection())
				{
					XTLTRACE_ERR("Presubscriber WaitForConnection failed.\n");
				}
			}
			else
			{
				XTLTRACE_ERR("Presubscriber ResetPipeHandle failed.\n");
			}
		}
		
		nSubscribers = subscribers.size();

		if (waitResult == WAIT_OBJECT_0) 
		{
			// Terminate Thread
			NDAS_EVENT_MESSAGE msg = {0};
			msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
			msg.EventType = NDAS_EVENT_TYPE_TERMINATING;
			PublishEvent(subscribers, msg);
			
			std::for_each(
				subscribers.begin(), subscribers.end(),
				boost::mem_fn(&CNdasEventSubscriber::Disconnect));

			(void) presubscriber.EndWaitForConnection();
			XTLASSERT( presubscriber.Disconnect() );

			XTLTRACE("Publisher Thread Terminated...\n");
			return 0;
		} 
		else if (waitResult == WAIT_OBJECT_0 + 1) 
		{
			// Event Message is queued
			while (TRUE) 
			{
				m_queueLock.Lock();
				if (m_EventMessageQueue.empty())
				{
					m_queueLock.Unlock();
					break;
				}
				// do not use a reference, we need a copy to pop and unlock
				NDAS_EVENT_MESSAGE msg = m_EventMessageQueue.front();
				m_EventMessageQueue.pop();
				m_queueLock.Unlock();
				PublishEvent(subscribers, msg);
			}

		} 
		else if (waitResult == WAIT_OBJECT_0 + 2)
		{
			if (!presubscriber.EndWaitForConnection())
			{
				XTLTRACE_ERR("Error in Presubscriber EndWaitForConnection.\n");

			}
			// New Connection
			HANDLE hConnectedPipe = presubscriber.DetachPipeHandle();

			CNdasEventSubscriberPtr subscriber(
				new CNdasEventSubscriber(hConnectedPipe));
			if (!subscriber->Initialize())
			{
				XTLTRACE_ERR("Subscriber initialization failed.\n");
				XTLVERIFY( ::DisconnectNamedPipe(hConnectedPipe) );
				XTLVERIFY( ::CloseHandle(hConnectedPipe) );
				continue;
			}
			// Send a version event for a new subscriber
			if (!SendInitialVersionInfo(subscriber))
			{
				XTLTRACE_ERR("Initial subscriber failed.\n");
			}
			else
			{
				subscribers.push_back(subscriber);
			}
			if (!presubscriber.ResetPipeHandle())
			{
				// maybe max connection has been reached
				XTLTRACE_ERR("Error in Presubscriber ResetPipeHandle.\n");
			}
			else
			{
				if (!presubscriber.BeginWaitForConnection())
				{
					// maybe max connection has been reached
					XTLTRACE_ERR("Error in Presubscriber BeginWaitForConnection.\n");
				}
			}
		}
		else if (WAIT_TIMEOUT == waitResult) 
		{
			NDAS_EVENT_MESSAGE msg = {0};
			msg.EventType = NDAS_EVENT_TYPE_PERIODIC;
			PublishEvent(subscribers, msg);
		}
		else 
		{
			// Error
			XTLASSERT(FALSE && "Invalid wait result.\n");
		}
	}
}

BOOL 
CNdasEventPublisher::AddEvent(
	const NDAS_EVENT_MESSAGE& eventMessage)
{
	m_queueLock.Lock();
	m_EventMessageQueue.push(eventMessage);
	m_queueLock.Unlock();
	if (!::ReleaseSemaphore(m_hSemQueue, 1, NULL)) 
	{
		// Queue Full
		XTLTRACE_ERR("Event Message Queue Full, Discarded %ws.\n", 
			NdasEventTypeString(eventMessage.EventType));
		return FALSE;
	}
	XTLTRACE("Event Message Queued: %ws\n", 
		NdasEventTypeString(eventMessage.EventType));
	return TRUE;
}

BOOL 
CNdasEventPublisher::DeviceEntryChanged()
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED;

	XTLTRACE("DeviceEntryChanged.\n");

	return AddEvent(msg);
}

BOOL  
CNdasEventPublisher::DeviceStatusChanged(
	const DWORD slotNo,
	const NDAS_DEVICE_STATUS oldStatus,
	const NDAS_DEVICE_STATUS newStatus)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED;
	msg.DeviceEventInfo.SlotNo = slotNo;
//	msg.DeviceEventInfo.DeviceId = deviceId;
	msg.DeviceEventInfo.OldStatus = oldStatus;
	msg.DeviceEventInfo.NewStatus = newStatus;

	XTLTRACE("DeviceStatusChanged (%d): %ws -> %ws.\n",
		slotNo, 
		NdasDeviceStatusString(oldStatus),
		NdasDeviceStatusString(newStatus));

	return AddEvent(msg);
}

BOOL
CNdasEventPublisher::DevicePropertyChanged(
	DWORD slotNo)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED;
	msg.DeviceEventInfo.SlotNo = slotNo;

	XTLTRACE("DevicePropertyChanged (%d).\n", slotNo);

	return AddEvent(msg);
}

BOOL
CNdasEventPublisher::UnitDevicePropertyChanged(
	DWORD slotNo, DWORD unitNo)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED;
	msg.UnitDeviceEventInfo.SlotNo = slotNo;
	msg.UnitDeviceEventInfo.UnitNo = unitNo;

	XTLTRACE("UnitDevicePropertyChanged (%d,%d).\n", slotNo, unitNo);

	return AddEvent(msg);
}

BOOL  
CNdasEventPublisher::LogicalDeviceEntryChanged()
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED;

	XTLTRACE("LogicalDeviceEntryChanged.\n");

	return AddEvent(msg);
}

BOOL  
CNdasEventPublisher::LogicalDeviceStatusChanged(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	NDAS_LOGICALDEVICE_STATUS oldStatus,
	NDAS_LOGICALDEVICE_STATUS newStatus)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;
	msg.LogicalDeviceEventInfo.OldStatus = oldStatus;
	msg.LogicalDeviceEventInfo.NewStatus = newStatus;

	XTLTRACE("LogicalDeviceStatusChanged (%d): %ws->%ws.\n",
		logicalDeviceId,
		NdasLogicalDeviceStatusString(oldStatus),
		NdasLogicalDeviceStatusString(newStatus));

	return AddEvent(msg);
}

BOOL
CNdasEventPublisher::LogicalDevicePropertyChanged(
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;

	XTLTRACE("LogicalDevicePropertyChanged (%d).\n", logicalDeviceId);

	return AddEvent(msg);
}

BOOL
CNdasEventPublisher::LogicalDeviceRelationChanged(
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;

	XTLTRACE("LogicalDeviceRelationChanged (%d).\n", logicalDeviceId);

	return AddEvent(msg);
}


BOOL  
CNdasEventPublisher::LogicalDeviceDisconnected(
	NDAS_LOGICALDEVICE_ID logicalDeviceId)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;

	XTLTRACE("LogicalDeviceDisconnected (%d).\n", logicalDeviceId);

	return AddEvent(msg);
}

BOOL 
CNdasEventPublisher::LogicalDeviceAlarmed(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	ULONG ulStatus)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;
	msg.LogicalDeviceEventInfo.AdapterStatus = ulStatus;

	XTLTRACE("LogicalDeviceAlarmed (%d): %08X.\n", logicalDeviceId, ulStatus);

	return AddEvent(msg);
}

BOOL
CNdasEventPublisher::SurrenderRequest(
	DWORD SlotNo, 
	DWORD UnitNo,
	LPCGUID lpRequestHostGuid,
	DWORD RequestFlags)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_SURRENDER_REQUEST;
	msg.SurrenderRequestInfo.RequestFlags = RequestFlags;
	msg.SurrenderRequestInfo.SlotNo = SlotNo;
	msg.SurrenderRequestInfo.UnitNo = UnitNo;
	msg.SurrenderRequestInfo.RequestHostGuid = *lpRequestHostGuid;

	XTLTRACE("SurrenderRequest (%d,%d): HOSTID=%s, FLAG=%08X.\n", 
		SlotNo, UnitNo,
		XTL::CStaticStringBufferA<40>().Append(*lpRequestHostGuid).ToString(), 
		RequestFlags);

	return AddEvent(msg);
}

BOOL
CNdasEventPublisher::SuspendRejected()
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_SUSPEND_REJECTED;

	XTLTRACE("SuspendRejected.\n");

	return AddEvent(msg);
}


namespace // anonymous namespace
{

void 
PublishEvent(
	CNdasEventSubscriberVector& subscribers, 
	const NDAS_EVENT_MESSAGE& msg)
{
	XTLTRACE("Publishing Event: %ws\n", NdasEventTypeString(msg.EventType));

	if (subscribers.empty()) return;

	//
	// Begin Write to Subscribers for in m_subscribers
	//
	// [begin() -- itr) contains valid subscribers
	// [itr ---- end()) contains invalid ones
	//
	CNdasEventSubscriberIterator valid_end = 
		std::partition(
			subscribers.begin(), subscribers.end(),
			BeginWriteToSubscriber(msg));

	size_t handles = std::distance(subscribers.begin(), valid_end);

	// Get Wait Handles
	std::vector<HANDLE> waitHandles(handles);

	std::transform(
		subscribers.begin(), valid_end,
		waitHandles.begin(),
		boost::mem_fn(&CNdasEventSubscriber::GetWaitHandle));

	const DWORD PUBLISH_TIMEOUT = 5000;

	if (!waitHandles.empty())
	{
		DWORD waitResult = ::WaitForMultipleObjects(
			waitHandles.size(),
			&waitHandles[0],
			TRUE,
			PUBLISH_TIMEOUT);
		XTLVERIFY(WAIT_TIMEOUT == waitResult ||
			(/*waitResult >= WAIT_OBJECT_0 && */ waitResult < WAIT_OBJECT_0 + waitHandles.size()));
	}
	else
	{
		XTLTRACE2(TCEventPub, TLInfo, "No valid subscribers.\n");
	}

	// EndWriteMessage
	CNdasEventSubscriberIterator connected_end = 
		std::partition(
			subscribers.begin(), valid_end,
			boost::mem_fn(&CNdasEventSubscriber::EndWriteMessage));

	//
	// itr2 is the last valid subscribers
	//
	// begin() <- valid -> itr2 <- write error -> itr <- init error -> end()
	//

	// Disconnect subscribers in error
	std::for_each(
		connected_end, subscribers.end(),
		boost::mem_fn(&CNdasEventSubscriber::Disconnect));

	// Remove subscribers in error
	subscribers.erase(
		connected_end, subscribers.end());
}

bool
SendInitialVersionInfo(CNdasEventSubscriberPtr subscriber)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_VERSION_INFO;
	msg.VersionInfo.MajorVersion = NDAS_EVENT_VERSION_MAJOR;
	msg.VersionInfo.MinorVersion = NDAS_EVENT_VERSION_MINOR;

	if (!subscriber->BeginWriteMessage(msg))
	{
		XTLVERIFY( subscriber->Disconnect() );
		XTLTRACE("Initial event message failed. Disconnecting %x.\n",
			subscriber->GetPipeHandle());
		return false;
	}
	::WaitForSingleObject(subscriber->GetWaitHandle(), 1000);
	if (!subscriber->EndWriteMessage())
	{
		XTLVERIFY( subscriber->Disconnect() );
		XTLTRACE("Finalize initial event message failed. Disconnecting %x.\n",
			subscriber->GetPipeHandle());
		return false;
	}

	return true;
}

}  // anonymous namespace

