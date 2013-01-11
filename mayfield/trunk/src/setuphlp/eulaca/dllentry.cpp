#include <windows.h>
#include <crtdbg.h>

BOOL 
APIENTRY 
DllMain(
	HMODULE hModule, 
	DWORD Reason, 
	LPVOID Reserved)
{
	UNREFERENCED_PARAMETER(Reserved);

	switch (Reason)
	{
	case DLL_PROCESS_ATTACH:
		{
			BOOL success = ::DisableThreadLibraryCalls(hModule);
			_ASSERTE(success); success;
		}
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		//
		// Never gets here as we called DisableThreadLibraryCalls
		//
		break;
	}
	return TRUE;
}
