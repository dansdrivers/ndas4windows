#include "stdafx.h"
#include "ndasevtsub.h"

#include "trace.h"
#ifdef RUN_WPP
#include "dllentry.tmh"
#endif

extern CNdasEventSubscriber* _pEventSubscriber;

LPCRITICAL_SECTION DllGlobalCriticalSection = NULL;

BOOL 
APIENTRY 
DllMain(
	HMODULE hModule, 
	DWORD  dwReason, 
	LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASUSER.DLL Process Attach\n");

		DllGlobalCriticalSection = reinterpret_cast<LPCRITICAL_SECTION>(
			::HeapAlloc(
				::GetProcessHeap(),
				HEAP_ZERO_MEMORY,
				sizeof(CRITICAL_SECTION)));

		if (NULL == DllGlobalCriticalSection)
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"HeapAlloc failed, bytes=%d\n",
				sizeof(CRITICAL_SECTION));
		}

		if (!::InitializeCriticalSectionAndSpinCount(DllGlobalCriticalSection, 0x80000400))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"InitializeCriticalSectionAndSpinCount failed.\n");
			XTLVERIFY(::HeapFree(::GetProcessHeap(), 0, DllGlobalCriticalSection));
			return FALSE;
		}

		XTLVERIFY(::DisableThreadLibraryCalls(hModule));

		break;
	case DLL_PROCESS_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASUSER.DLL Process Detach\n");
		::DeleteCriticalSection(DllGlobalCriticalSection);
		XTLVERIFY(::HeapFree(::GetProcessHeap(), 0, DllGlobalCriticalSection));
		break;
	case DLL_THREAD_ATTACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASUSER.DLL Thread Attach\n");
		break;
	case DLL_THREAD_DETACH:
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "NDASUSER.DLL Thread Detach\n");
		break;
	}
    return TRUE;
}
