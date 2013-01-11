/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include "ndassvcworkitem.h"
#include <xtl/xtlthread.h>

class CNdasService;

class CNdasCommandServer : 
	public CNdasServiceWorkItem<CNdasCommandServer,HANDLE>
{
protected:

	TCHAR PipeName[MAX_PATH];
	const DWORD PipeTimeout;
	const DWORD MaxPipeInstances;

	HANDLE m_hServerPipe;
	volatile DWORD m_nClients;

	XTL::AutoObjectHandle m_hProcSemaphore;

public:

	CNdasCommandServer();
	~CNdasCommandServer();

	HRESULT Initialize();

	DWORD ThreadStart(LPVOID hStopEvent);
	DWORD CommandProcessStart(HANDLE hStopEvent);
	DWORD ServiceCommand(HANDLE hStopEvent);

};
