#include "stdafx.h"
// #include <cfg.h>
// #include <cfgmgr32.h>

#pragma comment(lib, "setupapi.lib")

typedef DWORD 
(WINAPI* CMP_WaitNoPendingInstallEvents_Proc)(IN DWORD dwTimeout);

//
// For Windows XP and earlier versions of the operating system, 
// this function must be called from session zero, with administrator 
// privileges. For Windows XP SP1 and later versions, 
// the function can be called from any session, 
// and administrator privileges are not required.
//
BOOL
pIsDeviceInstallInProgress (VOID)
{
	HMODULE hModule;
	CMP_WaitNoPendingInstallEvents_Proc pCMP_WaitNoPendingInstallEvents;

	hModule = GetModuleHandle(TEXT("setupapi.dll"));
	if(!hModule)
	{
		// Should never happen since we're linked to SetupAPI, but...
		return FALSE;
	}

	pCMP_WaitNoPendingInstallEvents =
		(CMP_WaitNoPendingInstallEvents_Proc)GetProcAddress(hModule,
		"CMP_WaitNoPendingInstallEvents");
	if(!pCMP_WaitNoPendingInstallEvents)
	{
		// We're running on a release of the operating system that doesn't supply this function.
		// Trust the operating system to suppress Autorun when appropriate.
		return FALSE;
	}
	return (pCMP_WaitNoPendingInstallEvents(0) == WAIT_TIMEOUT);
}
