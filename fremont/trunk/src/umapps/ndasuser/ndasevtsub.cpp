#include "stdafx.h"
#include <ndas/ndasmsg.h>
#include <ndas/ndasuser.h>
#include <ndas/ndaseventex.h>
#include "ndasevtsub.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndasevtsub.tmh"
#endif

extern LPCRITICAL_SECTION DllGlobalCriticalSection;

namespace
{
	const HNDASEVENTCALLBACK HNDASEVTCB_BASE = (HNDASEVENTCALLBACK)0xC000;
	CNdasEventSubscriber* EventSubscriber = NULL;

	bool pIsValidHandle(HNDASEVENTCALLBACK hCallback)
	{
		return ((size_t(hCallback) >= size_t(HNDASEVTCB_BASE)) && 
			(size_t(hCallback)< size_t(HNDASEVTCB_BASE) + MAX_NDAS_EVENT_CALLBACK));
	}

	bool pIsValidEventVersion(const NDAS_EVENT_MESSAGE& Message)
	{
		if (NDAS_EVENT_TYPE_VERSION_INFO != Message.EventType) 
		{
			XTLTRACE1(
				TRACE_LEVEL_ERROR,
				"Getting Event Version Info failed: "
				"Type expected %d, received %d.\n",
				NDAS_EVENT_TYPE_VERSION_INFO, Message.EventType);
			::SetLastError(NDASUSER_ERROR_EVENT_VERSION_MISMATCH);
			return false;
		}
		if (NDAS_EVENT_VERSION_MAJOR != Message.VersionInfo.MajorVersion ||
			NDAS_EVENT_VERSION_MINOR != Message.VersionInfo.MinorVersion)
		{
			XTLTRACE1(
				TRACE_LEVEL_ERROR,
				"Event version mismatch: "
				"Version expected %d.%d, received %d.%d",
				NDAS_EVENT_VERSION_MAJOR, NDAS_EVENT_VERSION_MINOR,
				Message.VersionInfo.MajorVersion, 
				Message.VersionInfo.MinorVersion);
			::SetLastError(NDASUSER_ERROR_EVENT_VERSION_MISMATCH);
			return false;
		}
		return true;
	}

} // anonymous namespace


NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasUnregisterEventCallback(
	HNDASEVENTCALLBACK hEventCallback)
{
	__try
	{
		::EnterCriticalSection(DllGlobalCriticalSection);
		if (NULL == EventSubscriber)
		{
			::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		BOOL fSuccess = EventSubscriber->RemoveCallback(hEventCallback);
		if (!fSuccess)
		{
			return FALSE;
		}
		if (EventSubscriber->IsFinal())
		{
			XTLVERIFY(EventSubscriber->Finalize());
			delete EventSubscriber;
			EventSubscriber = NULL;
		}
		return TRUE;
	}
	__finally
	{
		::LeaveCriticalSection(DllGlobalCriticalSection);
	}
}

NDASUSER_LINKAGE
HNDASEVENTCALLBACK
NDASUSERAPI
NdasRegisterEventCallback(
	NDASEVENTPROC lpEventProc, LPVOID lpContext)
{
	if (::IsBadCodePtr(reinterpret_cast<FARPROC>(lpEventProc))) 
	{
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER);
		return NULL;
	}
	__try
	{
		::EnterCriticalSection(DllGlobalCriticalSection);
		if (NULL == EventSubscriber)
		{
			EventSubscriber = new CNdasEventSubscriber();
			if (NULL == EventSubscriber)
			{
				XTLTRACE1(TRACE_LEVEL_ERROR,
					"new CNdasEventSubscriber failed.\n");
				return NULL;
			}
		}
		if (!EventSubscriber->Initialize()) 
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"Event Subscriber initialization failed, error=0x%X\n",
				GetLastError());
			return NULL;
		}
		HNDASEVENTCALLBACK hCallback = 
			EventSubscriber->AddCallback(lpEventProc, lpContext);
		if (NULL == hCallback) 
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"Adding callback function failed, error=0x%X\n",
				GetLastError());
			return NULL;
		}
		return hCallback;
	}
	__finally
	{
		::LeaveCriticalSection(DllGlobalCriticalSection);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Register NDAS Event Callback function
//
//////////////////////////////////////////////////////////////////////////


CNdasEventSubscriber::
CNdasEventSubscriber() :
	m_cRef(0),
	m_nWorkItemStarted(0),
	m_csInit(false)
{
	::ZeroMemory(&m_cs, sizeof(CRITICAL_SECTION));
	::ZeroMemory(&m_CallbackData,
		sizeof(NDAS_EVENT_CALLBACK_DATA) * MAX_NDAS_EVENT_CALLBACK);
}

CNdasEventSubscriber::
~CNdasEventSubscriber()
{
}


bool
CNdasEventSubscriber::
Initialize()
{
	if (!m_csInit)
	{
		if (!::InitializeCriticalSectionAndSpinCount(&m_cs, 0x80000400))
		{
			return false;
		}
		m_csInit = true;
	}
	if (m_hThreadStopEvent.IsInvalid()) 
	{
		m_hThreadStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hThreadStopEvent.IsInvalid()) 
		{
			return false;
		}
	}
	if (m_hWorkItemSemaphore.IsInvalid()) 
	{
		m_hWorkItemSemaphore = ::CreateSemaphore(NULL, 0, MAX_WORKITEMS, NULL);
		if (m_hWorkItemSemaphore.IsInvalid()) 
		{
			return false;
		}
	}
	if (m_hDataEvent.IsInvalid()) 
	{
		m_hDataEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hDataEvent.IsInvalid()) 
		{
			return false;
		}
	}
	return true;
}


bool
CNdasEventSubscriber::
Finalize()
{
	if (m_csInit)
	{
		::DeleteCriticalSection(&m_cs);
	}
	return true;
}

bool 
CNdasEventSubscriber::
IsFinal()
{
	::EnterCriticalSection(&m_cs);
	for (DWORD i = 0; i < MAX_NDAS_EVENT_CALLBACK; ++i) 
	{
		if (NULL != m_CallbackData[i].EventProc)
		{
			::LeaveCriticalSection(&m_cs);
			return false;
		}
	}
	::LeaveCriticalSection(&m_cs);
	return true;
}

HNDASEVENTCALLBACK
CNdasEventSubscriber::
AddCallback(
	NDASEVENTPROC lpEventProc, 
	LPVOID lpContext)
{
	::EnterCriticalSection(&m_cs);

	//
	// Find the empty handle placeholder
	//
	HNDASEVENTCALLBACK hCallback = NULL;
	size_t i = 0;
	for (; i < MAX_NDAS_EVENT_CALLBACK; ++i) 
	{
		if (NULL == m_CallbackData[i].EventProc) 
		{
			hCallback = reinterpret_cast<HNDASEVENTCALLBACK>(i + reinterpret_cast<size_t>(HNDASEVTCB_BASE));
			break;
		}
	}

	//
	// No more available event callback?
	//
	if (NULL == hCallback) 
	{
		::LeaveCriticalSection(&m_cs);
		::SetLastError(NDASUSER_ERROR_EVENT_CALLBACK_FULL);
		return NULL;
	}

	//
	// Start thread if there is no thread started
	//
	if (0 == m_cRef) 
	{
		XTLVERIFY(::ResetEvent(m_hThreadStopEvent));
		if (!QueueUserWorkItemParam(NULL, WT_EXECUTELONGFUNCTION))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"QueueUserWorkItem failed, error=0x%X\n", GetLastError());
			::LeaveCriticalSection(&m_cs);
			return NULL;
		}
	}

	//
	// Add callback data
	//
	XTLASSERT(NULL == m_CallbackData[i].EventProc);
	XTLASSERT(NULL == m_CallbackData[i].lpContext);
	m_CallbackData[i].EventProc = lpEventProc;
	m_CallbackData[i].lpContext = lpContext;
	//
	// Add reference
	//
	++m_cRef;
	if (1 == m_cRef)
	{
		// Increment semaphore count to let the user work item start
		XTLVERIFY(::ReleaseSemaphore(m_hWorkItemSemaphore, 1, NULL));
		++m_nWorkItemStarted;
	}
	::LeaveCriticalSection(&m_cs);
	return hCallback;
}

BOOL
CNdasEventSubscriber::
RemoveCallback(HNDASEVENTCALLBACK hCallback)
{
	if (!pIsValidHandle(hCallback)) 
	{
		::SetLastError(NDASUSER_ERROR_INVALID_EVENT_CALLBACK_HANDLE);
		return FALSE;
	}

	::EnterCriticalSection(&m_cs);

	size_t i = (size_t)hCallback - (size_t)HNDASEVTCB_BASE;
	m_CallbackData[i].EventProc = NULL;
	m_CallbackData[i].lpContext = NULL;
	--m_cRef;

	if (0 == m_cRef) 
	{
		XTLASSERT(!m_hThreadStopEvent.IsInvalid());
		XTLVERIFY(::SetEvent(m_hThreadStopEvent));
		// Wait for work item to stop (until semaphore count becomes 0)
		for (DWORD nWorkItemStopped = 0; nWorkItemStopped < m_nWorkItemStarted; )
		{
			DWORD waitResult = ::WaitForSingleObject(m_hWorkItemSemaphore, 0);
			if (WAIT_OBJECT_0 == waitResult)
			{
				++nWorkItemStopped;
			}
			else
			{
				::Sleep(0);
			}
		}
		m_nWorkItemStarted = 0;
	}

	::LeaveCriticalSection(&m_cs);

	return TRUE;
}

DWORD 
CNdasEventSubscriber::
WorkItemStart(LPVOID)
{
	XTLASSERT(!m_hDataEvent.IsInvalid());
	XTLASSERT(!m_hThreadStopEvent.IsInvalid());
	XTLASSERT(!m_hWorkItemSemaphore.IsInvalid());

	//
	// Waiting for available semaphore
	//
	HANDLE waitHandles[] = { m_hThreadStopEvent, m_hWorkItemSemaphore };
	const DWORD nWaitHandles = RTL_NUMBER_OF(waitHandles);
	DWORD waitResult = ::WaitForMultipleObjects(nWaitHandles, waitHandles, FALSE, INFINITE);
	if (WAIT_OBJECT_0 == waitResult)
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"WorkItem stopped by the the stop event.\n");
		return 0;
	}
	XTLASSERT(WAIT_OBJECT_0 + 1 == waitResult);

	//
	// We should release semaphore when done!
	//
	// const DWORD Timeout = INFINITE;
	const DWORD ConnectInterval = 5000; // 5 seconds
	while (!IsThreadStopRequested())
	{
		XTL::AutoFileHandle hPipe = WaitForEventPipe(ConnectInterval);
		NDAS_EVENT_MESSAGE message = {0};
		if (IsThreadStopRequested())
		{
			break;
		}
		if (!ReadEventData(hPipe, message) ||
			!pIsValidEventVersion(message))
		{
			PublishConnectionFailed();
			// wait for 5 seconds
			IsThreadStopRequested(5000);
			continue;
		}
		PublishConnected();
		while (!IsThreadStopRequested())
		{
			BOOL DataReceived = ReadEventData(hPipe, message);
			if (DataReceived)
			{
				PublishServiceEvent(message);
			}
			else
			{
				break;
			}
		}
	}

	//
	// I am done. Increment the semaphore
	//
	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"WorkItem stopped by the the stop event.\n");
	XTLVERIFY(::ReleaseSemaphore(m_hWorkItemSemaphore, 1, NULL));
	return 0;

}


void 
CNdasEventSubscriber::
CallEventProc(DWORD Error, const NDAS_EVENT_INFO& EventInfo)
{
	::EnterCriticalSection(&m_cs);

	for (DWORD i = 0; i < MAX_NDAS_EVENT_CALLBACK; ++i) 
	{
		if (NULL != m_CallbackData[i].EventProc) 
		{
			// We are using a copy of event info as the parameter
			// is not constant or the callee may alter the data.
			NDAS_EVENT_INFO EventInfoCopy = EventInfo;
			m_CallbackData[i].EventProc(
				Error,
				&EventInfoCopy,
				m_CallbackData[i].lpContext);
		}
	}

	::LeaveCriticalSection(&m_cs);
}


void
CNdasEventSubscriber::
PublishConnecting()
{
	NDAS_EVENT_INFO EventInfo = {0};
	EventInfo.EventType = NDAS_EVENT_TYPE_CONNECTING;
	CallEventProc(0, EventInfo);
}

void 
CNdasEventSubscriber::
PublishConnectionFailed(
	DWORD Error)
{
	NDAS_EVENT_INFO EventInfo = {0};
	EventInfo.EventType = NDAS_EVENT_TYPE_CONNECTION_FAILED;
	CallEventProc(Error, EventInfo);
}


void 
CNdasEventSubscriber::
PublishConnected()
{
	NDAS_EVENT_INFO EventInfo = {0};
	EventInfo.EventType = NDAS_EVENT_TYPE_CONNECTED;
	CallEventProc(0, EventInfo);
}

void 
CNdasEventSubscriber::
PublishServiceEvent(const NDAS_EVENT_MESSAGE& Message)
{
	NDAS_EVENT_INFO EventInfo = {0};
	EventInfo.EventType = Message.EventType;
	switch (EventInfo.EventType) 
	{
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
	case NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED:
		EventInfo.EventInfo.DeviceInfo = Message.DeviceEventInfo;
		break;
	case NDAS_EVENT_TYPE_UNITDEVICE_PROPERTY_CHANGED:
		EventInfo.EventInfo.UnitDeviceInfo = Message.UnitDeviceEventInfo;
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED:
		EventInfo.EventInfo.LogicalDeviceInfo = Message.LogicalDeviceEventInfo;
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
	case NDAS_EVENT_TYPE_TERMINATING:
		break;
	case NDAS_EVENT_TYPE_SURRENDER_REQUEST:
		EventInfo.EventInfo.SurrenderRequestInfo = Message.SurrenderRequestInfo;
		break;
	case NDAS_EVENT_TYPE_SUSPEND_REJECTED:
		break;

	case NDAS_EVENT_TYPE_AUTO_PNP:

		XTLASSERT( sizeof(EventInfo.EventInfo.AutoPnpInfo) == sizeof(Message.AutoPnpInfo) ); 

		EventInfo.EventInfo.AutoPnpInfo = Message.AutoPnpInfo;

		XTLASSERT( !memcmp(EventInfo.EventInfo.AutoPnpInfo.NdasStringId,
						  Message.AutoPnpInfo.NdasStringId,
						  sizeof(TCHAR) * (NDAS_DEVICE_STRING_ID_LEN+1)) );

		break;

		//
		// Filter Out periodic event
		//
	case NDAS_EVENT_TYPE_PERIODIC:
		return;
	default:
		XTLASSERT(FALSE && "Unknown Event Type");
		break;
		return;
	}
	DWORD Error = ::GetLastError();
	CallEventProc(Error, EventInfo);
}

HANDLE
CNdasEventSubscriber::
WaitForEventPipe(DWORD Interval)
{
	do 
	{
		PublishConnecting();
		XTL::AutoFileHandle hPipe = ::CreateFile(
			NDAS_EVENT_PIPE_NAME,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);
		if (!hPipe.IsInvalid())
		{
			return hPipe.Detach();
		}
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Connecting to %ls failed, error=0x%X\n", 
			NDAS_EVENT_PIPE_NAME, GetLastError());

		PublishConnectionFailed();

		XTLTRACE1(TRACE_LEVEL_INFORMATION, 
			"Wait for %d ms...\n", Interval);
	} while (!IsThreadStopRequested(Interval));
	return INVALID_HANDLE_VALUE;
}

bool 
CNdasEventSubscriber::
ReadEventData(
	HANDLE hPipe, 
	NDAS_EVENT_MESSAGE& message)
{
	XTLVERIFY(!m_hThreadStopEvent.IsInvalid());
	XTLVERIFY(!m_hDataEvent.IsInvalid());

	OVERLAPPED overlapped = {0};
	overlapped.hEvent = m_hDataEvent;

	const DWORD cbMessage = sizeof(NDAS_EVENT_MESSAGE);
	DWORD cbRead;

	BOOL fSuccess = ::ReadFile(
		hPipe,
		&message,
		cbMessage,
		&cbRead,
		&overlapped);

	if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"ReadFile failed, error=0x%X\n", GetLastError());
		return false;
	}

	if (fSuccess) 
	{
		XTLVERIFY(::SetEvent(overlapped.hEvent));
	}

	//
	// If we cannot retrieve the version information
	// in a timeout interval or the version information
	// is mismatch, an error is returned
	//

	HANDLE waitHandles[] = {m_hThreadStopEvent, overlapped.hEvent };
	DWORD nWaitHandles = RTL_NUMBER_OF(waitHandles);
	DWORD waitResult = ::WaitForMultipleObjects(nWaitHandles, waitHandles, FALSE, INFINITE);

	if (WAIT_OBJECT_0 == waitResult)
	{
		XTLVERIFY(::CancelIo(hPipe));
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"Stopped by the stop event.\n");
		return false;
	}
	XTLASSERT(WAIT_OBJECT_0 + 1 == waitResult);
	if (WAIT_OBJECT_0 + 1 != waitResult) 
	{
		XTLVERIFY(::CancelIo(hPipe));
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Retrieving version information failed, error=0x%X\n",
			GetLastError());
		return false;
	}

	fSuccess = ::GetOverlappedResult(hPipe, &overlapped, &cbRead, TRUE);
	if (!fSuccess) 
	{
		XTLVERIFY(::CancelIo(hPipe));
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"Getting overlapped result failed, error=0x%X\n",
			GetLastError());
		return false;
	}

	if (cbRead < cbMessage)
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"Event message is too small: read %d, expected %d bytes.\n", 
			cbRead, cbMessage);
		::SetLastError(NDASUSER_ERROR_EVENT_VERSION_MISMATCH);
		return false;
	}

	return true;

}
