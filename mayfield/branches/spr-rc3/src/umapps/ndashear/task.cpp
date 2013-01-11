/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "task.h"
#include <process.h>
#include <crtdbg.h>
#include <strsafe.h>
#include "xdebug.h"

CTask::CTask(LPCTSTR szTaskName) :
	m_dwTaskThreadId(0), 
	m_bRunnable(FALSE),
	m_bIsRunning(FALSE),
	m_hTaskThreadHandle(NULL),
	m_hTaskTerminateEvent(NULL),
	m_szTaskName(szTaskName)
{
	DBGPRT_INFO(_FT("Task %s created.\n"), m_szTaskName);
}

CTask::~CTask() 
{
	if (NULL != m_hTaskTerminateEvent) {
		::CloseHandle(m_hTaskTerminateEvent);
	}
	if (NULL != m_hTaskThreadHandle) {
		::CloseHandle(m_hTaskThreadHandle);
	}

	DBGPRT_INFO(_FT("Task %s destroyed.\n"), m_szTaskName);
}

BOOL CTask::Initialize()
{
	m_bRunnable = TRUE;
	m_hTaskTerminateEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == m_hTaskTerminateEvent) {
		return FALSE;
	}
	return TRUE;
}

//
// Returns thread handle.
// This handle can be used to wait for
// the task to be stopped when called Stop(bAync = TRUE).
//
HANDLE 
CTask::
GetTaskHandle()
{
	return m_hTaskThreadHandle;
}

DWORD 
CTask::
GetTaskId()
{
	return m_dwTaskThreadId;
}

BOOL 
CTask::
IsRunning()
{
	return m_bIsRunning;
}

BOOL 
CTask::
Run()
{
	_ASSERTE(m_bRunnable && "Have you ever called CTask::Initialize()?");

	if (FALSE == m_bRunnable) {
		return FALSE;
	}
#if USE_WINAPI_THREAD
	m_hTaskThreadHandle = ::CreateThread(
		NULL,
		0,
		TaskThreadProcKickStart,
		this,
		NULL,
		&m_dwTaskThreadId);
#else
	m_hTaskThreadHandle = (HANDLE) _beginthreadex(
		NULL, 
		0,
		TaskThreadProcKickStart,
		this,
		NULL,
		(unsigned int*)&m_dwTaskThreadId);
#endif

	if (NULL == m_hTaskThreadHandle) {
		// TODO: Event Log Error Here
		DBGPRT_ERR_EX(_FT("Task thread creation failed: "));
		return FALSE;
	}

	m_bIsRunning = TRUE;

	DBGPRT_INFO(_FT("Task %s is running.\n"), m_szTaskName);
	return TRUE;
}

BOOL 
CTask::
Stop(BOOL bWaitUntilStopped)
{
	if (!m_bIsRunning) {
		return FALSE;
	}

	BOOL fSuccess = ::SetEvent(m_hTaskTerminateEvent);
	_ASSERT(fSuccess);

	DBGPRT_INFO(_FT("Task %s stop request queued.\n"), m_szTaskName);

	if (bWaitUntilStopped) {
		(VOID) ::WaitForSingleObject(GetTaskHandle(), INFINITE);
	}

	m_bIsRunning = FALSE;
	m_bRunnable = FALSE;

	return TRUE;
}

#if 0
DWORD WINAPI 
CTask::
TaskThreadProcKickStart(LPVOID lpParam)
#else
unsigned int __stdcall 
CTask::
TaskThreadProcKickStart(void* lpParam)
#endif
{
	CTask* pThis = reinterpret_cast<CTask*>(lpParam);
	unsigned int retval = pThis->OnTaskStart();
#if 0
	ExitThread(0);
#else
	_endthreadex(retval);
#endif
	DBGPRT_INFO(_FT("Task %s is stopped.\n"), pThis->m_szTaskName);
	return retval;
}

