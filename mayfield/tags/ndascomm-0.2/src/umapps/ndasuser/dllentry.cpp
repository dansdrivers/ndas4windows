#include "stdafx.h"
#include "procvar.h"
#include "ndasevtsub.h"

#define XDBG_FILENAME "dllentry.cpp"
#define XDBG_MAIN_MODULE
#include "xdebug.h"

extern CNdasEventSubscriber* _pEventSubscriber;

BOOL 
APIENTRY 
DllMain(
	HANDLE hModule, 
	DWORD  dwReason, 
	LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		InitProcessData();

		XDbgInit(_T("NDASUSER"));
		XDbgLoadSettingsFromRegistry(
			_T("Software\\NDAS\\NDASUSER"),
			HKEY_CURRENT_USER);

		DBGPRT_INFO(_FT("NDASUSER.DLL Process Attach\n"));

		_pEventSubscriber = new CNdasEventSubscriber();

		break;

	case DLL_THREAD_ATTACH:
		DBGPRT_INFO(_FT("NDASUSER.DLL Thread Attach\n"));
		break;
	case DLL_THREAD_DETACH:
		DBGPRT_INFO(_FT("NDASUSER.DLL Thread Detach\n"));
		break;
	case DLL_PROCESS_DETACH:

		if (NULL != _pxdbgSystemDebugOutput) {
			delete _pxdbgSystemDebugOutput;
		}

		XDbgCleanup();
		CleanupProcessData();
		DBGPRT_INFO(_FT("NDASUSER.DLL Process Detach\n"));

		delete _pEventSubscriber;

		break;
	}
    return TRUE;
}
