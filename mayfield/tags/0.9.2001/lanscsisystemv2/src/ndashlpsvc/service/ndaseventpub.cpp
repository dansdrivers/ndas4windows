#include "stdafx.h"
#include "ndaseventpub.h"
#include "task.h"
#include "ndascfg.h"

#include "ndastype_str.h"
#include "ndasevent_str.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_EVENTPUB
#include "xdebug.h"

HANDLE 
CNdasEventPublisher::
CreatePipeInstance(LPOVERLAPPED lpOverlapped)
{
	HANDLE hPipe = ::CreateNamedPipe(
		NDAS_EVENT_PIPE_NAME,
		PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		MAX_NDAS_EVENT_PIPE_INSTANCES,
		sizeof(NDAS_EVENT_MESSAGE),
		0,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);

	if (INVALID_HANDLE_VALUE == hPipe) {
		return INVALID_HANDLE_VALUE;
	}

	BOOL fConnected = ::ConnectNamedPipe(hPipe, lpOverlapped);

	// overlapped ConnectNamedPipe should return 0
	if (fConnected) {
		::CloseHandle(hPipe);
		return INVALID_HANDLE_VALUE;
	}

	switch (::GetLastError()) {
	case ERROR_IO_PENDING: 
		// The overlapped connection in progess
		break;

	case ERROR_PIPE_CONNECTED:
		// client is already connected, signal an event
		{
			BOOL fSuccess = ::SetEvent(lpOverlapped->hEvent);
            _ASSERT(fSuccess);
		}
		break;
	default:
		::CloseHandle(hPipe);
		return INVALID_HANDLE_VALUE;
	}

	return hPipe;
}

CNdasEventPublisher::
CNdasEventPublisher() :
	m_hSemQueue(NULL),
	ximeta::CTask()
{
	BOOL fSuccess = _NdasServiceCfg.GetValue(
		CFG_EVENTPUB_PERIODIC_INTERVAL,
		&m_dwPeriod);
	if (!fSuccess) {
		m_dwPeriod = 60 * 1000; // default 60 sec
	}
}

CNdasEventPublisher::
~CNdasEventPublisher()
{
	if (NULL != m_hSemQueue) {
		::CloseHandle(m_hSemQueue);
	}
}

BOOL
CNdasEventPublisher::
Initialize()
{
	if (NULL == m_hSemQueue) {
		m_hSemQueue = ::CreateSemaphore(
			NULL, 
			0, 
			MAX_NDAS_EVENT_PIPE_INSTANCES, 
			NULL);
	}

	if (NULL == m_hSemQueue) {
		return FALSE;
	}

	return CTask::Initialize();
}

BOOL
CNdasEventPublisher::
AcceptNewConnection()
{
	CLIENT_DATA clientData = {0};

	clientData.overlapped.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == clientData.overlapped.hEvent) {
		// Unable to create an event
		DPErrorEx(_FT("Creating an event failed: "));
		return FALSE;
	}

	clientData.hPipe = CreatePipeInstance(&clientData.overlapped);
	if (INVALID_HANDLE_VALUE == clientData.hPipe) {
		// Unable to create a pipe instance
		DPErrorEx(_FT("Creating the pipe instance failed: "));
		::CloseHandle(clientData.overlapped.hEvent);
		return FALSE;
	}

	m_PipeData.push_back(clientData);

	return TRUE;
}

BOOL
CNdasEventPublisher::
CleanupConnection(PCLIENT_DATA pClientData)
{
	::CloseHandle(pClientData->overlapped.hEvent);

	::DisconnectNamedPipe(pClientData->hPipe);
	::CloseHandle(pClientData->hPipe);

	return TRUE;
}

void 
CNdasEventPublisher::
Publish(const PNDAS_EVENT_MESSAGE pMessage)
{
	DPInfo(_FT("Publishing Event: %s\n"), 
		NdasEventTypeString(pMessage->EventType));

	//
	// sent the message to the connected pipes
	//
	for (ClientDataVector::iterator itr = m_PipeData.begin(); 
		itr != m_PipeData.end();) 
		//
		// do not forward the iterator here when erasing some 
		// elements 
		// itr2 = v.erase(itr); 
		// itr2 has a forwarded iterator from itr
		//
	{
		if (itr->bConnected) {
			DWORD cbWritten;
			BOOL fSuccess = ::WriteFile(
				itr->hPipe,
				pMessage,
				sizeof(NDAS_EVENT_MESSAGE),
				&cbWritten,
				&itr->overlapped);

			if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) {
				
				DPErrorEx(_FT("Writing to a pipe failed: "));
				DPInfo(_FT("Detaching an event subscriber.\n"));
				
				CleanupConnection(itr);
				//
				// erasing an element will automatically
				// forward the vector 
				// (actually, itr remains the same, but the itr is
				// a next elemen)
				itr = m_PipeData.erase(itr);

			} else {
				//
				// forward the iterator if we did erase
				//
				DPInfo(_FT("Event written to a pipe %p.\n"), itr->hPipe);
				++itr;
			}
		} else {
			//
			// forward the iterator if not connected
			//
			++itr;
		}
	}	
}

DWORD
CNdasEventPublisher::
OnTaskStart()
{
	_ASSERTE(NULL != m_hSemQueue && "Don't forget to call initialize().");

	// Queue Semaphore, Terminating Thread, Pipe Instances(MAX...)
	HANDLE hWaitHandles[2 + MAX_NDAS_EVENT_PIPE_INSTANCES];
	hWaitHandles[0] = m_hTaskTerminateEvent;
	hWaitHandles[1] = m_hSemQueue;

	//
	// initial pipe instance
	//
	BOOL fSuccess = AcceptNewConnection();
	if (!fSuccess) {
		DPErrorEx(_T("Creating a first pipe instance failed: "));
		return -1;
	}

	BOOL bTerminate(FALSE);

	while (FALSE == bTerminate) {

		DWORD dwWaitHandles = 2 + m_PipeData.size();
		for (DWORD i = 0; i < m_PipeData.size(); ++i) {
			hWaitHandles[i + 2] = m_PipeData[i].overlapped.hEvent;
		}

		DWORD dwWaitResult = ::WaitForMultipleObjects(
			dwWaitHandles,
			hWaitHandles,
			FALSE,
			m_dwPeriod);


		if (dwWaitResult == WAIT_OBJECT_0) 
		{
			//
			// Terminate Thread
			//
			bTerminate = TRUE;
		} 
		else if (dwWaitResult == WAIT_OBJECT_0 + 1) 
		{
			//
			// Event Message is queued
			//
			NDAS_EVENT_MESSAGE message = m_EventMessageQueue.back();
			m_EventMessageQueue.pop();
			PNDAS_EVENT_MESSAGE pMessage = &message;

			Publish(pMessage);

		} 
		else if (dwWaitResult >= WAIT_OBJECT_0 + 2 && 
			dwWaitResult < WAIT_OBJECT_0 + 2 + m_PipeData.size())
		{
			DWORD dwPipe = dwWaitResult - WAIT_OBJECT_0 - 2;

			DPInfo(_FT("Event Client %d\n"), dwPipe);

			CLIENT_DATA* pCurClientData = &m_PipeData[dwPipe];

			DPInfo(_FT("Connected: %d\n"), pCurClientData->bConnected);

			BOOL fSuccess = ::ResetEvent(pCurClientData->overlapped.hEvent);
			_ASSERT(fSuccess);

			if (!pCurClientData->bConnected) {
				
				//
				// create another instance
				//
				fSuccess = AcceptNewConnection();
				if (!fSuccess) {
					DPWarningEx(_FT("Creating another pipe instance failed: "));
					DPWarning(_FT("No more event subscribers can be accepted.\n"));
				}

				// AcceptNewConnection will invalidate pCurClientData;

				pCurClientData = &m_PipeData.at(dwPipe);

				//
				// Accepting connection
				//
				pCurClientData->bConnected = TRUE;
				BOOL fSuccess = ::ResetEvent(pCurClientData->overlapped.hEvent);
				_ASSERT(fSuccess);
				
				//
				// Send a version event for connected client
				//
				fSuccess = SendVersionInfo(
					pCurClientData->hPipe, 
					&pCurClientData->overlapped);

				//
				// Any failure will disconnect the client
				//
				if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) {
					CleanupConnection(pCurClientData);
					ClientDataVector::iterator itr = m_PipeData.begin();
					while (itr != m_PipeData.end()) {
						if (itr->hPipe == pCurClientData->hPipe) {
							m_PipeData.erase(itr);
							break;
						}
						++itr;
					}
					DPInfo(_FT("Accepted removed event subscriber.\n"));
				} else {
					DPInfo(_FT("Accepted new event subscriber.\n"));
				}

			} else {
			}
			// ignore other status
		} else if (WAIT_TIMEOUT == dwWaitResult) {
			NDAS_EVENT_MESSAGE msg = {0};
			msg.EventType = NDAS_EVENT_TYPE_PERIODIC;
			Publish(&msg);
		} else 
		{
			//
			// Error
			//
		}

	}

	//
	// TODO: Add cleanup
	//
	ClientDataVector::iterator itr = m_PipeData.begin();
	while (itr != m_PipeData.end()) {
		CleanupConnection(itr);
	}
	m_PipeData.clear();

	_tprintf(TEXT("Terminating Publisher Thread...\n"));
	return 0;
}

BOOL 
CNdasEventPublisher::
AddEvent(
	PNDAS_EVENT_MESSAGE pEventMessage)
{
	_ASSERTE(!::IsBadReadPtr(pEventMessage, sizeof(PNDAS_EVENT_MESSAGE)));
	m_EventMessageQueue.push(*pEventMessage);
	BOOL fSuccess = ::ReleaseSemaphore(m_hSemQueue, 1, NULL);
	if (!fSuccess) {
		// Queue Full
		return FALSE;
	}
	DPInfo(_FT("Event Message Queued: %s\n"), 
		NdasEventTypeString(pEventMessage->EventType));
	return TRUE;
}

BOOL
CNdasEventPublisher::
SendVersionInfo(HANDLE hPipe, LPOVERLAPPED lpOverlapped)
{
	const DWORD cbMessage = sizeof(NDAS_EVENT_MESSAGE);
	NDAS_EVENT_MESSAGE msgVersion = {0};

	msgVersion.MessageSize = cbMessage;
	msgVersion.EventType = NDAS_EVENT_TYPE_VERSION_INFO;
	msgVersion.VersionInfo.MajorVersion = NDAS_EVENT_VERSION_MAJOR;
	msgVersion.VersionInfo.MinorVersion = NDAS_EVENT_VERSION_MINOR;

	DWORD cbWritten(0);
	
	BOOL fSuccess = ::WriteFile(
		hPipe, 
		&msgVersion,
		cbMessage,
		&cbWritten,
		lpOverlapped);

	if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) {
		DPErrorEx(_FT("Writing an initial version event failed: "));
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasEventPublisher::
DeviceEntryChanged()
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED;

	DPInfo(_FT("Device Entry Changed\n"));

	return AddEvent(&msg);
}

BOOL  
CNdasEventPublisher::
DeviceStatusChanged(
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

	DPInfo(_FT("Device (%d) Status Changed: %s -> %s.\n"),
		slotNo, 
		NdasDeviceStatusString(oldStatus),
		NdasDeviceStatusString(newStatus));

	return AddEvent(&msg);
}

BOOL  
CNdasEventPublisher::
LogicalDeviceEntryChanged()
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED;

	DPInfo(_FT("Logical Device Entry Changed\n"));

	return AddEvent(&msg);
}

BOOL  
CNdasEventPublisher::
LogicalDeviceStatusChanged(
	const NDAS_LOGICALDEVICE_ID& logicalDeviceId,
	NDAS_LOGICALDEVICE_STATUS oldStatus,
	NDAS_LOGICALDEVICE_STATUS newStatus)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;
	msg.LogicalDeviceEventInfo.OldStatus = oldStatus;
	msg.LogicalDeviceEventInfo.NewStatus = newStatus;

	DPInfo(_FT("Logical Device (%d,%d,%d) Status Changed: %s -> %s.\n"),
		logicalDeviceId.SlotNo,
		logicalDeviceId.TargetId,
		logicalDeviceId.LUN,
		NdasLogicalDeviceStatusString(oldStatus),
		NdasLogicalDeviceStatusString(newStatus));

	return AddEvent(&msg);
}

BOOL  
CNdasEventPublisher::
LogicalDeviceDisconnected(const NDAS_LOGICALDEVICE_ID& logicalDeviceId)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;

	DPInfo(_FT("Logical Device (%d,%d,%d) is disconnected.\n"),
		logicalDeviceId.SlotNo,
		logicalDeviceId.TargetId,
		logicalDeviceId.LUN);

	return AddEvent(&msg);
}

BOOL  
CNdasEventPublisher::
LogicalDeviceReconnecting(const NDAS_LOGICALDEVICE_ID& logicalDeviceId)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;

	DPInfo(_FT("Logical Device (%d,%d,%d) is being reconnected.\n"),
		logicalDeviceId.SlotNo,
		logicalDeviceId.TargetId,
		logicalDeviceId.LUN);

	return AddEvent(&msg);
}

BOOL 
CNdasEventPublisher::
LogicalDeviceReconnected(const NDAS_LOGICALDEVICE_ID& logicalDeviceId)
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTED;
	msg.LogicalDeviceEventInfo.LogicalDeviceId = logicalDeviceId;

	DPInfo(_FT("Logical Device (%d,%d,%d) is alive.\n"),
		logicalDeviceId.SlotNo,
		logicalDeviceId.TargetId,
		logicalDeviceId.LUN);

	return AddEvent(&msg);
}

BOOL 
CNdasEventPublisher::
ServiceTerminating()
{
	NDAS_EVENT_MESSAGE msg = {0};
	msg.MessageSize = sizeof(NDAS_EVENT_MESSAGE);
	msg.EventType = NDAS_EVENT_TYPE_TERMINATING;

	DPInfo(_FT("Service Termination Event.\n"));

	return AddEvent(&msg);
}
