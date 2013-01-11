/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "task.h"
#include <process.h>

#define XDEBUG_MODULE_FLAG 0x00100000
#include "xdebug.h"

namespace ximeta {

CTask::CTask() :
	m_dwTaskThreadId(0), 
	m_bRunnable(FALSE),
	m_bIsRunning(FALSE),
	m_hTaskThreadHandle(INVALID_HANDLE_VALUE),
	m_hTaskTerminateEvent(NULL)
{
}

CTask::~CTask() 
{
	if (NULL != m_hTaskTerminateEvent) {
		::CloseHandle(m_hTaskTerminateEvent);
	}
	if (INVALID_HANDLE_VALUE != m_hTaskThreadHandle) {
		::CloseHandle(m_hTaskThreadHandle);
	}
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
		DPErrorEx(_T("Task thread creation failed!\n"));
		return FALSE;
	}

	m_bIsRunning = TRUE;
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
	unsigned int retval = reinterpret_cast<CTask*>(lpParam)->OnTaskStart();
	_endthreadex(retval);
	return retval;
}

} // ximeta
