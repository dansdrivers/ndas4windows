#include "stdafx.h"
#include "ndas/ndasmsg.h"
#include "ndas/ndasuser.h"
#include "ndas/ndaseventex.h"
#include "ndasevtsub.h"

#define XDBG_FILENAME "ndasevtsub.cpp"
#include "xdebug.h"

const HNDASEVENTCALLBACK CNdasEventSubscriber::HNDASEVTCB_BASE = (HNDASEVENTCALLBACK)0xC000;
CNdasEventSubscriber* _pEventSubscriber = NULL;

NDASUSER_API
BOOL
WINAPI
NdasUnregisterEventCallback(
	HNDASEVENTCALLBACK hEventCallback)
{
	return _pEventSubscriber->RemoveCallback(hEventCallback);
}


NDASUSER_API
HNDASEVENTCALLBACK
WINAPI
NdasRegisterEventCallback(
	NDASEVENTPROC lpEventProc, LPVOID lpContext)
{
	if (IsBadCodePtr(reinterpret_cast<FARPROC>(lpEventProc))) {
		::SetLastError(NDASUSER_ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	BOOL fSuccess = _pEventSubscriber->Initialize();
	if (!fSuccess) {
		DPErrorEx(_FT("Event Subscriber initialization failed: "));
		return FALSE;
	}
	
	HNDASEVENTCALLBACK hCallback = 
		_pEventSubscriber->AddCallback(lpEventProc, lpContext);

	if (NULL == hCallback) {
		DPErrorEx(_FT("Adding callback function failed: "));
		return FALSE;
	}

	return hCallback;
}

//////////////////////////////////////////////////////////////////////////
//
// Register NDAS Event Callback function
//
//////////////////////////////////////////////////////////////////////////

static HANDLE CreatePipeConnection(LPOVERLAPPED lpOverlapped)
{
	HANDLE hPipe = ::CreateFile(
			NDAS_EVENT_PIPE_NAME,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);

	if (INVALID_HANDLE_VALUE == hPipe)
	{
		DPErrorEx(_FT("Connecting to %s failed: "), NDAS_EVENT_PIPE_NAME);
		return INVALID_HANDLE_VALUE;
	}

	//
	// We should read the event version info immediately
	//
	NDAS_EVENT_MESSAGE message = {0};
	DWORD cbMessage = sizeof(NDAS_EVENT_MESSAGE);
	DWORD cbRead(0);

	BOOL fSuccess = ::ReadFile(
		hPipe,
		&message,
		cbMessage,
		&cbRead,
		lpOverlapped);

	if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) {
		DPErrorEx(_FT("Retrieving version information failed on ReadFile: "));
		::CloseHandle(hPipe);
		return INVALID_HANDLE_VALUE;
	}

	if (fSuccess) {
		fSuccess = ::SetEvent(lpOverlapped->hEvent);
		_ASSERTE(fSuccess);
	}

	//
	// If we cannot retrieve the version information
	// in a timeout interval or the version information
	// is mismatch, an error is returned
	//

	const DWORD dwTimeout = 3000; // 3 sec timeout
	DWORD dwWaitResult = ::WaitForSingleObject(
		lpOverlapped->hEvent,
		dwTimeout);

	if (dwWaitResult == WAIT_TIMEOUT) {
		DPError(_FT("Retrieving version information timed out."));
		::CloseHandle(hPipe);
		return INVALID_HANDLE_VALUE;
	}

	if (dwWaitResult != WAIT_OBJECT_0) {
		DPErrorEx(_FT("Retrieving version information failed: "));
		::CloseHandle(hPipe);
		return INVALID_HANDLE_VALUE;
	}

	fSuccess = ::GetOverlappedResult(
		hPipe,
		lpOverlapped,
		&cbRead,
		TRUE);

	if (!fSuccess) {
		DPErrorEx(_FT("Getting overlapped result failed: "));
		::CloseHandle(hPipe);
		return INVALID_HANDLE_VALUE;
	}

	if (NDAS_EVENT_TYPE_VERSION_INFO != message.EventType) {
		DPError(_FT("Getting Event Version Info failed: ")
			_T("Type expected %d, received %d.\n"),
			NDAS_EVENT_TYPE_VERSION_INFO, message.EventType);
		::CloseHandle(hPipe);
		::SetLastError(NDASUSER_ERROR_EVENT_VERSION_MISMATCH);
		return INVALID_HANDLE_VALUE;
	}

	if (NDAS_EVENT_VERSION_MAJOR != message.VersionInfo.MajorVersion ||
		NDAS_EVENT_VERSION_MINOR != message.VersionInfo.MinorVersion)
	{
		DPErrorEx(_FT("Event version mismatch: ")
			_T("Version expected %d.%d, received %d.%d"),
			NDAS_EVENT_VERSION_MAJOR, NDAS_EVENT_VERSION_MINOR,
			message.VersionInfo.MajorVersion, message.VersionInfo.MinorVersion);
		::CloseHandle(hPipe);
		::SetLastError(NDASUSER_ERROR_EVENT_VERSION_MISMATCH);
		return INVALID_HANDLE_VALUE;
	}

	return hPipe;
}


CNdasEventSubscriber::
CNdasEventSubscriber() :
	m_cRef(0),
	m_hThread(INVALID_HANDLE_VALUE),
	m_hThreadStopEvent(NULL),
	m_dwThreadId(0),
	m_hDataEvent(NULL)
{
	::ZeroMemory(
		&m_cs, 
		sizeof(CRITICAL_SECTION));

	::InitializeCriticalSection(&m_cs);

	::ZeroMemory(
		&m_CallbackData,
		sizeof(NDAS_EVENT_CALLBACK_DATA) * MAX_NDAS_EVENT_CALLBACK);
}


CNdasEventSubscriber::
~CNdasEventSubscriber()
{
	::DeleteCriticalSection(&m_cs);

	if (INVALID_HANDLE_VALUE != m_hThread) {
		if (NULL != m_hThreadStopEvent) {
			::SetEvent(m_hThreadStopEvent);
		}
		::WaitForSingleObject(m_hThread, 10000);
		::CloseHandle(m_hThread);
	}

	if (NULL != m_hThreadStopEvent) {
		BOOL fSuccess = ::CloseHandle(m_hThreadStopEvent);
		_ASSERT(fSuccess);
	}
}

BOOL 
CNdasEventSubscriber::
Initialize()
{
	if (NULL == m_hThreadStopEvent) {
		m_hThreadStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hThreadStopEvent) {
			return FALSE;
		}
	}

	if (NULL == m_hDataEvent) {
		m_hDataEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL == m_hDataEvent) {
			return FALSE;
		}
	}
	return TRUE;
}

HNDASEVENTCALLBACK
CNdasEventSubscriber::
AddCallback(NDASEVENTPROC lpEventProc, LPVOID lpContext)
{
	::EnterCriticalSection(&m_cs);
	//
	// Find the empty handle placeholder
	//
	HNDASEVENTCALLBACK hCallback = NULL;
	DWORD i(0);
	for (; i < MAX_NDAS_EVENT_CALLBACK; ++i) {
		if (NULL == m_CallbackData[i].EventProc) {
			//
			// Handle 0 is an invalid value. 
			// So we use
			// 
			// 
			hCallback = reinterpret_cast<HNDASEVENTCALLBACK>(
				i + reinterpret_cast<DWORD>(HNDASEVTCB_BASE));
			break;
		}
	}

	//
	// No more available event callback?
	//
	if (NULL == hCallback) {
		::LeaveCriticalSection(&m_cs);
		::SetLastError(NDASUSER_ERROR_EVENT_CALLBACK_FULL);
		return NULL;
	}

	//
	// Start thread if there is no thread started
	//
	if (m_cRef == 0 || m_hThread == INVALID_HANDLE_VALUE) {

		m_hThread = ::CreateThread(
			NULL, 
			0, 
			ThreadProc, 
			reinterpret_cast<LPVOID>(this),
			0, 
			&m_dwThreadId);

		if (NULL == m_hThread) {
			::LeaveCriticalSection(&m_cs);
			return NULL;
		}

		BOOL fSuccess = ::ResetEvent(m_hThreadStopEvent);
		_ASSERT(fSuccess);

	}

	//
	// Add callback data
	//
	_ASSERTE(NULL == m_CallbackData[i].EventProc);
	_ASSERTE(NULL == m_CallbackData[i].lpContext);

	m_CallbackData[i].EventProc = lpEventProc;
	m_CallbackData[i].lpContext = lpContext;

	//
	// Add reference
	//
	++m_cRef;

	::LeaveCriticalSection(&m_cs);
	return hCallback;
}

BOOL
CNdasEventSubscriber::
RemoveCallback(HNDASEVENTCALLBACK hCallback)
{
	if (!IsValidHandle(hCallback)) {
		::SetLastError(NDASUSER_ERROR_INVALID_EVENT_CALLBACK_HANDLE);
		return FALSE;
	}

	::EnterCriticalSection(&m_cs);

	DWORD i = (DWORD)hCallback - (DWORD)HNDASEVTCB_BASE;
	m_CallbackData[i].EventProc = NULL;
	m_CallbackData[i].lpContext = NULL;

	--m_cRef;
	::LeaveCriticalSection(&m_cs);

	if (0 == m_cRef) {
		::SetEvent(m_hThreadStopEvent);
		::WaitForSingleObject(m_hThread, 10*1000);
		::CloseHandle(m_hThread);
		m_hThread = INVALID_HANDLE_VALUE;
	}

	return TRUE;
}

HANDLE
CNdasEventSubscriber::
WaitServer(BOOL& bStopThread)
{
	OVERLAPPED ov = {0};
	ov.hEvent = m_hDataEvent;

	HANDLE hPipe = CreatePipeConnection(&ov);

	if (INVALID_HANDLE_VALUE == hPipe) {
		NDAS_EVENT_INFO ei = {0};
		ei.EventType = NDAS_EVENT_TYPE_CONNECTION_FAILED;
		CallEventProc(::GetLastError(), NULL);
	}

	//
	// Wait for 5 seconds and try again
	//

	while (INVALID_HANDLE_VALUE == hPipe) {
		
		{
			NDAS_EVENT_INFO ei = {0};
			ei.EventType = NDAS_EVENT_TYPE_CONNECTION_FAILED;
			CallEventProc(::GetLastError(), &ei);
		}

		DWORD dwWaitResult = ::WaitForSingleObject(
			m_hThreadStopEvent, 
			5 * 1000);

		if (WAIT_OBJECT_0 == dwWaitResult) {

			bStopThread = TRUE;
			return INVALID_HANDLE_VALUE;

		} else if (WAIT_TIMEOUT == dwWaitResult) {

			{
				NDAS_EVENT_INFO ei = {0};
				ei.EventType = NDAS_EVENT_TYPE_RETRYING_CONNECTION;
				CallEventProc(::GetLastError(), &ei);
			}

			hPipe = CreatePipeConnection(&ov);

		} else {
			DPErrorEx(_FT("Wait failed: \n"));
			return INVALID_HANDLE_VALUE;
		}
	}

	{
		NDAS_EVENT_INFO ei = {0};
		ei.EventType = NDAS_EVENT_TYPE_CONNECTED;
		CallEventProc(0, &ei);
	}

	return hPipe;
}

DWORD
CNdasEventSubscriber::
Run()
{
	_ASSERTE(NULL != m_hDataEvent);
	_ASSERTE(INVALID_HANDLE_VALUE != m_hThread);
	_ASSERTE(NULL != m_hThreadStopEvent);
	// Why?
	// _ASSERTE(m_hThread == ::GetCurrentThread());

	OVERLAPPED ov = {0};
	ov.hEvent = m_hDataEvent;

	BOOL bStopThread = FALSE;
	HANDLE hPipe = WaitServer(bStopThread);

	while (!bStopThread) {

		NDAS_EVENT_MESSAGE message = {0};
		DWORD cbMessage = sizeof(NDAS_EVENT_MESSAGE);
		DWORD cbRead(0);

		BOOL fRead = ::ReadFile(
			hPipe, 
			&message, 
			cbMessage, 
			&cbRead, 
			&ov);

		if (!fRead && ERROR_IO_PENDING != ::GetLastError()) {
			// failure!
			DPErrorEx(_FT("ReadFile failed: "));
			
			::CloseHandle(hPipe);
			hPipe = WaitServer(bStopThread);

			continue;
		}

		if (fRead) {
			::SetEvent(m_hDataEvent);
		}

		HANDLE hWaitHandles[2] = { m_hThreadStopEvent, m_hDataEvent };

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			2,
			hWaitHandles,
			FALSE,
			INFINITE);

		if (WAIT_OBJECT_0 == dwWaitResult) {
			//
			// Thread stop event
			//
			bStopThread = TRUE;

		} else if (WAIT_OBJECT_0 + 1 == dwWaitResult) {

			DWORD cbRead(0);
			BOOL fData = ::GetOverlappedResult(hPipe, &ov, &cbRead, TRUE);

			NDAS_EVENT_INFO eventInfo = {0};
			eventInfo.EventType = message.EventType;
			switch (eventInfo.EventType) {
			case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
			case NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED:
				eventInfo.DeviceInfo = message.DeviceEventInfo;
				break;
			case NDAS_EVENT_TYPE_UNITDEVICE_PROPERTY_CHANGED:
				eventInfo.UnitDeviceInfo = message.UnitDeviceEventInfo;
				break;
			case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
			case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
			//case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING:
			//case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTED:
			//case NDAS_EVENT_TYPE_LOGICALDEVICE_EMERGENCY:
			case NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED:
				eventInfo.LogicalDeviceInfo = message.LogicalDeviceEventInfo;
				break;
			case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
			case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
			case NDAS_EVENT_TYPE_TERMINATING:
				break;
			case NDAS_EVENT_TYPE_SURRENDER_REQUEST:
				eventInfo.SurrenderRequestInfo = message.SurrenderRequestInfo;
				break;
			case NDAS_EVENT_TYPE_PERIODIC:
			default:
			break;
			}

			if (NDAS_EVENT_TYPE_PERIODIC != message.EventType) {
				CallEventProc(::GetLastError(), &eventInfo);
			}

		} else {

			DPWarningEx(_FT("Wait failed: "));
		}

	}

	if (INVALID_HANDLE_VALUE != hPipe) {
		::CloseHandle(hPipe);
	}

	return 0;
}

void 
CNdasEventSubscriber::
CallEventProc(DWORD dwError, PNDAS_EVENT_INFO pEventInfo)
{
	for (DWORD i = 0; i < MAX_NDAS_EVENT_CALLBACK; ++i) {
		if (NULL != m_CallbackData[i].EventProc) {
			m_CallbackData[i].EventProc(
				dwError,
				pEventInfo,
				m_CallbackData[i].lpContext);
		}
	}
}

DWORD
CNdasEventSubscriber::
ThreadProc(LPVOID lpParam)
{
	CNdasEventSubscriber* pThis = 
		reinterpret_cast<CNdasEventSubscriber*>(lpParam);
	DWORD dwResult = pThis->Run();
	::ExitThread(dwResult);
	return dwResult;
}

BOOL
CNdasEventSubscriber::
IsValidHandle(HNDASEVENTCALLBACK hCallback)
{
	return ((DWORD(hCallback) >= DWORD(HNDASEVTCB_BASE)) && 
		(DWORD(hCallback)< DWORD(HNDASEVTCB_BASE) + MAX_NDAS_EVENT_CALLBACK));
}
