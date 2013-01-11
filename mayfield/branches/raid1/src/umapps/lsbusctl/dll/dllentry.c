#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#define DBGPRN_USE_EXTERN_LEVEL
#include "xdbgprn.h"

static DWORD _DbgPrintLevel2 = 3;

BOOL 
APIENTRY 
DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:

		DebugPrint(3, _T("LSBUSCTL.DLL Attached to the thread PID %d (0x%08X).\n"),
			GetCurrentProcessId(),
			GetCurrentProcessId());

		break;

	case DLL_THREAD_ATTACH:

		DebugPrint(3, _T("LSBUSCTL.DLL Attached to the thread PID %d (0x%08X), TID %d (0x%08X).\n"), 
			GetCurrentProcessId(), GetCurrentProcessId(),
			GetCurrentThreadId(), GetCurrentThreadId());

		break;

	case DLL_THREAD_DETACH:

		DebugPrint(3, _T("LSBUSCTL.DLL Detached the thread PID %d (0x%08X), TID %d (0x%08X).\n"), 
			GetCurrentProcessId(), GetCurrentProcessId(),
			GetCurrentThreadId(), GetCurrentThreadId());

		break;

	case DLL_PROCESS_DETACH:

		DebugPrint(3, _T("LSBUSCTL.DLL Detached from the process PID %d (0x%08X).\n"),
			GetCurrentProcessId(),
			GetCurrentProcessId());

		break;
	}
	return TRUE;
}
