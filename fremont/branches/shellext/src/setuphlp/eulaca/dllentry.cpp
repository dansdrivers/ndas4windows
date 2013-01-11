#include <windows.h>
#include <crtdbg.h>

BOOL 
APIENTRY 
DllMain(
	HMODULE hModule, 
	DWORD dwReason, 
	LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			BOOL fSuccess = ::DisableThreadLibraryCalls(hModule);
			_ASSERTE(fSuccess);
		}
		break;
	case DLL_THREAD_ATTACH:
		// Never called as we called DisableThreadLibraryCalls
		break;
	case DLL_THREAD_DETACH:
		// Never called as we called DisableThreadLibraryCalls
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
