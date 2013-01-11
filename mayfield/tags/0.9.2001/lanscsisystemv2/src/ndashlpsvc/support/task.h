/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once

namespace ximeta {

class CTask {
protected:
	BOOL m_bRunnable;
	BOOL m_bIsRunning;
	DWORD m_dwTaskThreadId;
	HANDLE m_hTaskThreadHandle;
	HANDLE m_hTaskTerminateEvent;
	// LPTHREAD_START_ROUTINE m_lpTaskProc;

public:
	// CTask(LPTHREAD_START_ROUTINE lpStartAddress = NULL);
	CTask();
	virtual ~CTask();
	virtual BOOL Initialize();

	//
	// Returns thread handle.
	// This handle can be used to wait for
	// the task to be stopped when called Stop(bAync = TRUE).
	//
	virtual HANDLE GetTaskHandle();
    virtual DWORD GetTaskId();
	virtual BOOL IsRunning();
	virtual BOOL Run();
	virtual BOOL Stop(BOOL bWaitUntilStopped = TRUE);

	//
	// User-supplied task start routine which is started
	// on a newly created thread
	//
	virtual DWORD OnTaskStart() = 0;

#if 0
	static DWORD WINAPI TaskThreadProcKickStart(LPVOID lpParam);
#else
	static unsigned int __stdcall TaskThreadProcKickStart(void* lpParam);
#endif

};

} // ximeta
