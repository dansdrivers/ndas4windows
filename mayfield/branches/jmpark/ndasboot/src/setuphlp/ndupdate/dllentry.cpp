#include "stdafx.h"
#define XDBG_FILENAME "dllentry.cpp"
#define XDBG_MAIN_MODULE
#include "xdebug.h"

#define DLL_NAME "NDUPDATE.DLL"

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

		XDbgInit(_T("NDUPDATE"));
		XDbgLoadSettingsFromRegistry(
			_T("Software\\NDAS\\NDUpdate"),
			HKEY_CURRENT_USER);

		DBGPRT_INFO(_FT(DLL_NAME) _T(" Process Attach\n"));

		_NdasUpdateDllInstance = hModule;

		break;

	case DLL_THREAD_ATTACH:

		DBGPRT_INFO(_FT(DLL_NAME) _T(" Thread Attach\n"));
		break;

	case DLL_THREAD_DETACH:

		DBGPRT_INFO(_FT(DLL_NAME) _T(" Thread Detach\n"));
		break;

	case DLL_PROCESS_DETACH:
		DBGPRT_INFO(_FT(DLL_NAME) _T(" Process Detach\n"));

		XDbgCleanup();
		break;
	}
    return TRUE;
}
