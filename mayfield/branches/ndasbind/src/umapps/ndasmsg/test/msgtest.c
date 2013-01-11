#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "ndasmsg.h"

#define CUSTOMER_MESSAGE_FILE _T("ndasmsg.dll")
#define IS_CUSTOMER_CODE(x) ((0x20000000 & (x)) == (0x20000000))

/*
 * Returns FALSE if unable to load a message
 * Caller is responsible to free the buffer with LocalFree(lpBuffer)
 */
BOOL ResolveMessage(IN DWORD dwMessageId, 
					IN DWORD dwLanguageId, 
					OUT LPVOID lplpBuffer)
{
	DWORD fmtFlags;
	DWORD nMessage;
	LPVOID lpSource;
	HMODULE hMessageFile;

	fmtFlags = 0;

	if (IS_CUSTOMER_CODE(dwMessageId)) {
		fmtFlags |= FORMAT_MESSAGE_FROM_HMODULE;
		hMessageFile = LoadLibrary(CUSTOMER_MESSAGE_FILE);
		if (hMessageFile == NULL) {
			return FALSE;
		}
		lpSource = (LPVOID) hMessageFile;
	} else {
		fmtFlags |= FORMAT_MESSAGE_FROM_SYSTEM;
		lpSource = NULL;
	}

	fmtFlags |= FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_IGNORE_INSERTS;

	nMessage = FormatMessage(
		fmtFlags, 
		lpSource, 
		dwMessageId, 
		dwLanguageId, 
		(LPTSTR) lplpBuffer, 
		0, 
		NULL);

	if (IS_CUSTOMER_CODE(dwMessageId)) {
		FreeLibrary(hMessageFile);
	}

	/* FormatMessage returns 0 if there is an error */
	return (nMessage != 0);
}

int ErrorMessageBox()
{
	LPTSTR lpBuffer = NULL;
	int iResult;

	if (!ResolveMessage(GetLastError(), 0, &lpBuffer)) {
		/* Unable to load message from HMODULE or other */
		/* customer error */
		if (ResolveMessage(GetLastError(), 0, &lpBuffer)) {
		}
	}

	iResult = MessageBox(
		GetDesktopWindow(), 
		lpBuffer, 
		_T("Error"), 
		MB_OK | MB_ICONERROR);

	LocalFree(lpBuffer);

	return iResult;
}

int __cdecl _tmain(int argc, LPTSTR* argv)
{
	// DebugBreak();
	SetLastError(NDAS_ERROR_UNKNOWN); // NDAS_ERROR_UNKNOWN);
	ErrorMessageBox();
	return 0;
}
