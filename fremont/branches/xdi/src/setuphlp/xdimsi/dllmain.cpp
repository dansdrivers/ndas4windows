#include "precomp.hpp"

BOOL 
APIENTRY 
DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

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
