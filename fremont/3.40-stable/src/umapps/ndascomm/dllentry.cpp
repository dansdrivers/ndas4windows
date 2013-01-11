#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <xtl/xtltrace.h>

#ifndef NDASCOMM_SLIB

BOOL APIENTRY DllMain(HMODULE ModuleHandle, DWORD Reason, LPVOID Reserved)
{
	switch (Reason)
	{
	case DLL_PROCESS_ATTACH:

		XTL::XtlTraceInitModuleName(ModuleHandle);
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Process Attach\n");
		break;

	case DLL_THREAD_ATTACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Thread Attach\n");
		break;

	case DLL_THREAD_DETACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Thread Detach\n");
		break;

	case DLL_PROCESS_DETACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASCOMM.DLL Process Detach\n");
		break;
	}
    return TRUE;
}

#endif
