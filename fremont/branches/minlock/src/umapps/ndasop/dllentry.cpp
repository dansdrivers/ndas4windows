#include "stdafx.h"
#include <xtl/xtltrace.h>

#ifdef RUN_WPP
#include "dllentry.tmh"
#endif

HMODULE NDASOPModule = NULL;

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
		NDASOPModule = (HMODULE)hModule;
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASOP.DLL Process Attach\n");
		break;

	case DLL_THREAD_ATTACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASOP.DLL Thread Attach\n");
		break;

	case DLL_THREAD_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASOP.DLL Thread Detach\n");
		break;

	case DLL_PROCESS_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASOP.DLL Process Detach\n");
		break;
	}

	return TRUE;
}
