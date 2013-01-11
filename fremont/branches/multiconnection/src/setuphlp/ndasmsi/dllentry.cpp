#include "stdafx.h"
#include "trace.h"
#ifdef RUN_WPP
#include "dllentry.tmh"
#endif

BOOL 
APIENTRY 
DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, _T("NDMSICA.DLL Process Attach\n"));
		break;

	case DLL_THREAD_ATTACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, _T("NDMSICA.DLL Thread Attach\n"));
		break;

	case DLL_THREAD_DETACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, _T("NDMSICA.DLL Thread Detach\n"));
		break;

	case DLL_PROCESS_DETACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, _T("NDMSICA.DLL Process Detach\n"));
		break;
	}
	return TRUE;
}
