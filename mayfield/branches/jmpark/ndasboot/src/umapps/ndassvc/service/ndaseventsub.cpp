#include "stdafx.h"
#include "ndaseventsub.h"

//////////////////////////////////////////////////////////////////////////
//
// CNdasEventPreSubscriber implementation
//

CNdasEventPreSubscriber::
CNdasEventPreSubscriber(
	LPCTSTR PipeName, DWORD MaxInstances) :
	PipeName(PipeName), MaxInstances(MaxInstances)
{
}

bool 
CNdasEventPreSubscriber::Initialize()
{
	if (m_hEvent.IsInvalid())
	{
		m_hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hEvent.IsInvalid())
		{
			return false;
		}
	}
	if (!ResetPipeHandle())
	{
		return false;
	}
	XTLENSURE_RETURN_T(ResetOverlapped(), false);
	return true;
}

HANDLE 
CNdasEventPreSubscriber::GetWaitHandle()
{
	XTLASSERT(!m_hEvent.IsInvalid());
	return m_hEvent;
}

HANDLE 
CNdasEventPreSubscriber::GetPipeHandle()
{
	XTLASSERT(!m_hPipe.IsInvalid());
	return m_hPipe;
}

bool 
CNdasEventPreSubscriber::Disconnect()
{
	XTLENSURE_RETURN_T( ::DisconnectNamedPipe(m_hPipe), false);
	return true;
}

bool 
CNdasEventPreSubscriber::BeginWaitForConnection()
{
	XTLENSURE_RETURN_T(!m_hPipe.IsInvalid() && "Not initialized", false);
	XTLENSURE_RETURN_T(!m_hEvent.IsInvalid() && "Not initialized", false);
	XTLENSURE_RETURN_T( ResetOverlapped(), false);
	BOOL fConnected = ::ConnectNamedPipe(m_hPipe, &m_overlapped);
	if (fConnected) 
	{
		XTLENSURE_RETURN_T(::SetEvent(m_hEvent), false);
		return true;
	}
	DWORD error = ::GetLastError();
	switch (error)
	{
	case ERROR_IO_PENDING: 
		// The overlapped connection in progress
		break;
	case ERROR_PIPE_CONNECTED:
		// client is already connected, signal an event
		XTLENSURE_RETURN_T(::SetEvent(m_hEvent), false);
		break;
	default:
		return true;
	}
	return true;
}

bool 
CNdasEventPreSubscriber::EndWaitForConnection()
{
	DWORD cbTransferred;
	BOOL fSuccess = ::GetOverlappedResult(m_hPipe, &m_overlapped,&cbTransferred,FALSE);
	if (!fSuccess)
	{
		XTLASSERT(ERROR_IO_INCOMPLETE == ::GetLastError());
		XTLVERIFY( ::CancelIo(m_hPipe) );
	}
	return fSuccess ? true : false;
}

HANDLE
CNdasEventPreSubscriber::DetachPipeHandle()
{
	return m_hPipe.Detach();
}

bool
CNdasEventPreSubscriber::ResetPipeHandle()
{
	XTLASSERT(m_hPipe.IsInvalid());
	XTLENSURE_RETURN_T(ResetOverlapped(), false);
	m_hPipe = ::CreateNamedPipe(
		PipeName,
		PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		MaxInstances,
		sizeof(NDAS_EVENT_MESSAGE),
		0,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);
	if (m_hPipe.IsInvalid())
	{
		return false;
	}
	return true;
}

bool
CNdasEventPreSubscriber::ResetOverlapped()
{
	::ZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
	m_overlapped.hEvent = m_hEvent;
	XTLENSURE_RETURN_T(::ResetEvent(m_hEvent), false);
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// CNdasEventSubscriber implementation
//

CNdasEventSubscriber::
CNdasEventSubscriber(HANDLE hPipe) :
	m_hPipe(hPipe)
{

}

bool 
CNdasEventSubscriber::Initialize()
{
	if (m_hEvent.IsInvalid())
	{
		m_hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hEvent.IsInvalid())
		{
			return false;
		}
	}
	return true;
}

HANDLE 
CNdasEventSubscriber::GetWaitHandle()
{
	XTLASSERT(!m_hEvent.IsInvalid());
	return m_hEvent;
}

HANDLE 
CNdasEventSubscriber::GetPipeHandle()
{
	XTLASSERT(!m_hPipe.IsInvalid());
	return m_hPipe;
}

bool 
CNdasEventSubscriber::Disconnect()
{
	XTLENSURE_RETURN_T( ::DisconnectNamedPipe(m_hPipe), false);
	return true;
}

bool
CNdasEventSubscriber::BeginWriteMessage(const NDAS_EVENT_MESSAGE& msg)
{
	XTLENSURE_RETURN_T(ResetOverlapped(), false);
	const DWORD cbToWrite = sizeof(NDAS_EVENT_MESSAGE);
	DWORD cbWritten;
	BOOL fSuccess = ::WriteFile(m_hPipe, &msg, cbToWrite, &cbWritten, &m_overlapped);
	if (!fSuccess && ::GetLastError() != ERROR_IO_PENDING)
	{
		return false;
	}
	return true;
}

bool
CNdasEventSubscriber::EndWriteMessage()
{
	DWORD cbTransferred;
	BOOL fSuccess = ::GetOverlappedResult(m_hPipe, &m_overlapped,&cbTransferred,FALSE);
	if (!fSuccess)
	{
		XTLASSERT(ERROR_IO_INCOMPLETE == ::GetLastError());
		XTLVERIFY( ::CancelIo(m_hPipe) );
	}
	return fSuccess ? true : false;
}

bool 
CNdasEventSubscriber::ResetOverlapped()
{
	::ZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
	m_overlapped.hEvent = m_hEvent;
	XTLENSURE_RETURN_T(::ResetEvent(m_hEvent), false);
	return true;
}
