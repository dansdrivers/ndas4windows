#include "stdafx.h"

#define XDBG_FILENAME "dllentry.cpp"
#define XDBG_MAIN_MODULE
#include "xdebug.h"

BOOL 
APIENTRY 
DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		XDbgInit(_T("NDMSICA"));
		XDbgLoadSettingsFromRegistry(
			_T("Software\\NDAS\\NDMSICA"),
			HKEY_CURRENT_USER);

		DBGPRT_INFO(_FT("NDMSICA.DLL Process Attach\n"));
		break;

	case DLL_THREAD_ATTACH:

		DBGPRT_INFO(_FT("NDMSICA.DLL Thread Attach\n"));
		break;

	case DLL_THREAD_DETACH:

		DBGPRT_INFO(_FT("NDMSICA.DLL Thread Detach\n"));
		break;

	case DLL_PROCESS_DETACH:

		DBGPRT_INFO(_FT("NDMSICA.DLL Process Detach\n"));
		XDbgCleanup();
		break;
	}
	return TRUE;
}
