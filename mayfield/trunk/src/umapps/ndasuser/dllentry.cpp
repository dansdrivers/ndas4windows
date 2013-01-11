#include "stdafx.h"
#include <xtl/xtltrace.h>
#include "ndasevtsub.h"

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

	const LPCTSTR DllRegKey = _T("Software\\NDAS\\ndasuser");

	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		if (!(XTL::XtlTraceLoadSettingsFromRegistry(DllRegKey, HKEY_LOCAL_MACHINE) ||
			XTL::XtlTraceLoadSettingsFromRegistry(DllRegKey, HKEY_CURRENT_USER)))
		{
#if _DEBUG
			XTL::XtlTraceEnableDebuggerTrace();
			XTL::XtlTraceSetTraceCategory(0xFFFFFFFF);
			XTL::XtlTraceSetTraceLevel(5);
#endif
		}

		XTLTRACE("NDASUSER.DLL Process Attach\n");

		DllGlobalCriticalSection = reinterpret_cast<LPCRITICAL_SECTION>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CRITICAL_SECTION)));

		if (NULL == DllGlobalCriticalSection)
		{
			XTLTRACE_ERR("Memory allocation failed.\n");
		}

		if (!::InitializeCriticalSectionAndSpinCount(DllGlobalCriticalSection, 0x80000400))
		{
			XTLTRACE_ERR("InitializeCriticalSectionAndSpinCount failed.\n");
			XTLVERIFY(::HeapFree(::GetProcessHeap(), 0, DllGlobalCriticalSection));
			return FALSE;
		}

		XTLVERIFY(::DisableThreadLibraryCalls(hModule));

		break;
	case DLL_PROCESS_DETACH:
		XTLTRACE("NDASUSER.DLL Process Detach\n");
		::DeleteCriticalSection(DllGlobalCriticalSection);
		XTLVERIFY(::HeapFree(::GetProcessHeap(), 0, DllGlobalCriticalSection));
		break;
	case DLL_THREAD_ATTACH:
		XTLTRACE("NDASUSER.DLL Thread Attach\n");
		break;
	case DLL_THREAD_DETACH:
		XTLTRACE("NDASUSER.DLL Thread Detach\n");
		break;
	}
    return TRUE;
}
