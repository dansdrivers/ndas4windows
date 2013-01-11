#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>


#pragma comment(lib, "advapi32.lib")

int PrintError(DWORD dwError)
{
	LPWSTR lpBuffer;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		0,
		(LPWSTR) &lpBuffer,
		0,
		NULL);

	wprintf(L"%s\n", lpBuffer);
	LocalFree(lpBuffer);
	return 0;	
}

int wmain(int argc, LPWSTR* argv)
{
	HKEY hKey;
	LONG lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\NDAS", 0, KEY_READ, &hKey);
	if (lResult != ERROR_SUCCESS) {
		wprintf(L"Error %d (0x%08X)\n", lResult, lResult);
		PrintError(lResult);
	}
	DWORD dwDisposition;
	lResult = RegCreateKeyEx(
		HKEY_LOCAL_MACHINE, 
		L"Software\\NDAS\\Devices\\0",
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		0,
		NULL,
		&hKey,
		&dwDisposition);

	wprintf(L"Returned %d (0x%08X) - %s (%d) \n", lResult, lResult,
		(dwDisposition == REG_CREATED_NEW_KEY) ? L"CREATED_NEW_KEY" :
		(dwDisposition == REG_OPENED_EXISTING_KEY) ? L"EXISTING_KEY" : L"???",
		dwDisposition);
	PrintError(lResult);
		
	return 0;
}
