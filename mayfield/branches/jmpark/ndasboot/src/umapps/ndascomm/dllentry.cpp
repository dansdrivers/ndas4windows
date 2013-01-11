#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#define XDBG_FILENAME "dllentry.cpp"
#ifdef NDASCOMM_SLIB
#define NO_XDEBUG
#else
#define XDBG_MAIN_MODULE
#endif
#include "xdebug.h"
#include "init.h"

#ifndef NDASCOMM_SLIB

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

		XDbgInit(_T("NDASCOMM"));
		XDbgLoadSettingsFromRegistry(
			_T("Software\\NDAS\\NDASCOMM"),
			HKEY_CURRENT_USER);

		DBGPRT_INFO(_FT("NDASCOMM.DLL Process Attach\n"));

		__try
		{
			DllCreateInitSync();
		}
		__except(GetExceptionCode() == STATUS_NO_MEMORY ?
			EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			DBGPRT_ERR(_FT("Out of memory exception\n"));
			return FALSE;
		}

		break;

	case DLL_THREAD_ATTACH:

		DBGPRT_INFO(_FT("NDASCOMM.DLL Thread Attach\n"));
		break;

	case DLL_THREAD_DETACH:

		DBGPRT_INFO(_FT("NDASCOMM.DLL Thread Detach\n"));
		break;

	case DLL_PROCESS_DETACH:
		DBGPRT_INFO(_FT("NDASCOMM.DLL Process Detach\n"));

		DllDestroyInitSync();

		XDbgCleanup();
		break;
	}
    return TRUE;
}

#endif
