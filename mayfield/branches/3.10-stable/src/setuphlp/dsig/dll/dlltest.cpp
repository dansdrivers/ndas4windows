#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tchar.h>

typedef DWORD (WINAPI* GetEffectiveDriverSigningPolicyProc)(VOID);

void main()
{
	HMODULE hModule = LoadLibrary(_T("dsig.dll"));
	if (NULL == hModule) {
		_tprintf(_T("Unable to load dsig.exe\n"));
		return;
	}
	GetEffectiveDriverSigningPolicyProc proc = 
		(GetEffectiveDriverSigningPolicyProc)GetProcAddress(hModule, "_DSAAF__");
	if (NULL == proc) {
		_tprintf(_T("Unable to get fptr.\n"));
		FreeLibrary(hModule);
		return;
	}

	DWORD ef = proc();

	_tprintf(_T("ef=%d\n"), ef);
	
	FreeLibrary(hModule);
	return;
}

