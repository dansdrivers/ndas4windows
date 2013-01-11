#include "precomp.hpp"
#include <xtl/xtltrace.h>

BOOL 
APIENTRY 
DllMain(HMODULE ModuleHandle, DWORD Reason, LPVOID Reserved)
{
	switch (Reason)
	{
	case DLL_PROCESS_ATTACH:
		XTL::XtlTraceInitModuleName(ModuleHandle);
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
