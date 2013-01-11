/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once

namespace ximeta {
	class CTask;
};

class CNdasCommandServer;

class CNdasCommandServer
	: public ximeta::CTask
{
protected:

	TCHAR m_szPipeName[MAX_PATH];
	const DWORD m_dwPipeTimeout;
	const DWORD m_dwMaxPipeInstances;

	HANDLE m_hServerPipe;
	volatile DWORD m_nClients;

public:

	CNdasCommandServer();
	virtual ~CNdasCommandServer();
	virtual BOOL Initialize();

	virtual DWORD OnTaskStart();

#ifdef USE_WINAPI_THREAD
	static DWORD WINAPI PipeInstanceThreadProc(LPVOID lpParam);
#else
	static unsigned int __stdcall PipeInstanceThreadProc(void* lpParam);
#endif

};
