#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include "init.h"
#include <xtl/xtltrace.h>

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

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Process Attach\n");

		__try
		{
			DllCreateInitSync();
		}
		__except(GetExceptionCode() == STATUS_NO_MEMORY ?
			EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, "Out of memory exception\n");
			return FALSE;
		}

		break;

	case DLL_THREAD_ATTACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Thread Attach\n");
		break;

	case DLL_THREAD_DETACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Thread Detach\n");
		break;

	case DLL_PROCESS_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Process Detach\n");
		DllDestroyInitSync();
		break;
	}
    return TRUE;
}

#endif
