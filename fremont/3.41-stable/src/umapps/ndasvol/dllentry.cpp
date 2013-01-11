#include "precomp.hpp"

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
		::DisableThreadLibraryCalls(hModule);
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	}
    return TRUE;
}
