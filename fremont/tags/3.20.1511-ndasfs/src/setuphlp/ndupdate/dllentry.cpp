#include "stdafx.h"
#define DLL_NAME "NDUPDATE.DLL"

#include "trace.h"
#ifdef RUN_WPP
#include "dllentry.tmh"
#endif

HINSTANCE _NdasUpdateDllInstance = NULL;

BOOL 
APIENTRY 
DllMain(
	HINSTANCE hModule, 
	DWORD  dwReason, 
	LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDUPDATE Process Attach\n");
		_NdasUpdateDllInstance = hModule;
		break;

	case DLL_THREAD_ATTACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDUPDATE Thread Attach\n");
		break;

	case DLL_THREAD_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDUPDATE Thread Detach\n");
		break;

	case DLL_PROCESS_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDUPDATE Process Detach\n");
		break;
	}
    return TRUE;
}
