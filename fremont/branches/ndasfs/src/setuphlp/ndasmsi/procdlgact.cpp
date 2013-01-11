#include "stdafx.h"
#include "procdlgact.hpp"
#include "lasterr.hpp"
#include <tchar.h>

CProcessDialogActivator::
CProcessDialogActivator() :
	m_hStopEvent(NULL)
{
}

CProcessDialogActivator::
~CProcessDialogActivator()
{
	if (m_hStopEvent)
	{
		RESERVE_LAST_ERROR();
		::CloseHandle(m_hStopEvent);
	}
}

void
CProcessDialogActivator::
ActivateProcessWindow()
{
	HWND hWndNext = NULL;
	do
	{
		hWndNext = ::FindWindowEx(
			NULL, 
			hWndNext,
			MAKEINTATOM(32770), // dialog class
			NULL); 
		// we do not know the caption (it depends on OS language)
		if (hWndNext != NULL)
		{
			DWORD dwProcessId = 0;
			DWORD dwThreadId = ::GetWindowThreadProcessId(hWndNext, &dwProcessId);
			UNREFERENCED_PARAMETER(dwThreadId);
			if (::GetCurrentProcessId() == dwProcessId)
			{
				// make the window topmost
				(void) ::SetWindowPos(
					hWndNext, 
					HWND_TOPMOST, 
					0, 0, 0, 0, 
					SWP_ASYNCWINDOWPOS | SWP_NOSIZE | SWP_NOMOVE /* | SWP_SHOWWINDOW */);
				// SetForegroundWindow(hwndNext))
				// yield to the GUI thread
				::Sleep(0); 
			}
		}
	} while (NULL != hWndNext);
}

DWORD
CProcessDialogActivator::
ThreadMain()
{
	while (TRUE)
	{
		ActivateProcessWindow();

		DWORD waitResult = ::WaitForSingleObjectEx(
			m_hStopEvent,
			m_dwInterval,
			TRUE);

		if (WAIT_OBJECT_0 == waitResult)
		{
			return 0;
		}
		else if (WAIT_TIMEOUT != waitResult)
		{
			return GetLastError();
		}
	}
}

BOOL
CProcessDialogActivator::
Start(DWORD dwInterval)
{
	if (NULL == m_hStopEvent)
	{
		m_hStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	}
	else
	{
		::ResetEvent(m_hStopEvent);
	}

	if (NULL == m_hStopEvent)
	{
		return FALSE;
	}

	m_dwInterval = dwInterval;
	BOOL fSuccess = Create(
		NULL,
		0,
		CREATE_SUSPENDED);
	if (!fSuccess)
	{
		return FALSE;
	}

	fSuccess = ::SetThreadPriority(m_hThread, THREAD_PRIORITY_IDLE);
	DWORD nSuspended = ::ResumeThread(m_hThread);
	if ((DWORD)(-1) == nSuspended)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL
CProcessDialogActivator::
Stop(BOOL fWait)
{
	if (NULL == m_hThread || NULL == m_hStopEvent)
	{
		return FALSE;
	}

	BOOL fSuccess = ::SetEvent(m_hStopEvent);
	if (!fSuccess)
	{
		return FALSE;
	}

	return (fWait) ? WAIT_OBJECT_0 ==Wait() : TRUE;
}

DWORD
CProcessDialogActivator::
Wait(DWORD dwTimeout)
{
	return ::WaitForSingleObject(m_hThread, dwTimeout);
}
