/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once

namespace ximeta {
	class CTask;
};

class CNdasCommandServer;
typedef CNdasCommandServer *PCNdasCommandServer;

class CNdasCommandServer
	: public ximeta::CTask
{
public:

	static const DWORD PIPE_MAX_INSTANCES_VALUE_MIN = 1;
	static const DWORD PIPE_MAX_INSTANCES_VALUE_MAX = 10; 

protected:

	TCHAR m_szPipeName[_MAX_PATH + 1];
	DWORD m_dwPipeTimeout;
	DWORD m_dwMaxPipeInstances;
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
