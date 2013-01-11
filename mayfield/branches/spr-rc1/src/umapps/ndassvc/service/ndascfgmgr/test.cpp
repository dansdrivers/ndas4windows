#include <windows.h>
#include <stdio.h>
#include <wincrypt.h>
#include "confmgr.h"

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

int wmain()
{
	BOOL fSuccess(FALSE);
	CConfigurationManager cfgmgr(L"Software\\NDAS");
	BYTE entropy[] = {0x00,0x10,0x02};
//	cfgmgr.SetEntropy(entropy, 3);
//	DebugBreak();
#if 0
	fSuccess = cfgmgr.SetSecureValueEx(L"Devices\\0", L"DeviceID", L"00:0B:D0:00:86:EB", sizeof(WCHAR) * 19);
	if (!fSuccess) {
		DWORD dwError = GetLastError();
		wprintf(L"Error %d (0x%08X)\n", dwError, dwError);
		PrintError(dwError);
		return -1;
	}
#endif
	WCHAR buffer[19];
	DWORD cbUsed;
	fSuccess = cfgmgr.GetSecureValueEx(L"Devices\\0", L"DeviceID", &buffer, sizeof(WCHAR) * 19, &cbUsed);
	if (!fSuccess) {
		DWORD dwError = GetLastError();
		wprintf(L"Error %d (0x%08X)\n", dwError, dwError);
		PrintError(dwError);
		return -1;
	}
	wprintf(L"Device ID: %s\n", buffer);

	return 0;
}
