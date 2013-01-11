#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>
#include <strsafe.h>
#include "rofiltctl.h"

void usage()
{
	_tprintf(TEXT("rfctl <start,stop,query> | <enable,disable> [drive]\n"));
}

int __cdecl _tmain(int argc, LPTSTR* argv)
{
	BOOL fSuccess(FALSE);

	HANDLE hFilter = NdasRoFilterOpenDevice();

	while (TRUE) {

	TCHAR szCommand[80];
	_tprintf(TEXT("Command: "));
	_tscanf(TEXT("%s"), szCommand);

	if (lstrcmpi(szCommand,TEXT("query")) == 0) {
		DWORD dwDriveMask;
		fSuccess= NdasRoFilterQueryFilteredDrives(hFilter, &dwDriveMask);
		if (fSuccess) {

		TCHAR szBuffer[26 * 2 + 1] = {0};
		TCHAR* pszBuffer = szBuffer;
		for (DWORD i = 0; i < 'Z' - 'A' + 1; ++i) {
			if (dwDriveMask & (1 << i)) {
				pszBuffer[0] = (TCHAR)i + TEXT('A');
				pszBuffer[1] = TEXT(' ');
				pszBuffer[2] = TEXT('\0');
				pszBuffer += 2;
			}
		}
		_ASSERTE(pszBuffer <= &szBuffer[26 * 2 + 1]);
		_tprintf(TEXT("Filtered on drive(s): %s.\n"), szBuffer);

		}		
	} else if (lstrcmpi(szCommand,TEXT("start")) == 0) {
		fSuccess = NdasRoFilterStartFilter(hFilter);
	} else if (lstrcmpi(szCommand,TEXT("stop")) == 0) {
		fSuccess = NdasRoFilterStopFilter(hFilter);
	} else if (lstrcmpi(szCommand,TEXT("enable")) == 0) {

		TCHAR szDrive[256];
		_tscanf(TEXT("%s"), szDrive);
		DWORD dwDriveNumber = szDrive[0] - TEXT('A');
		if (dwDriveNumber > 25)
			dwDriveNumber = szDrive[0] - TEXT('a');

		if (dwDriveNumber > 25) {
			usage();
			break;
		}

		_tprintf(TEXT("Enabling on drive %c.\n"), dwDriveNumber + TEXT('A'));
		fSuccess = NdasRoFilterEnableFilter(hFilter, dwDriveNumber, TRUE);
	
	} else if (lstrcmpi(szCommand,TEXT("disable")) == 0) {

		TCHAR szDrive[256];
		_tscanf(TEXT("%s"), szDrive);
		DWORD dwDriveNumber = szDrive[0] - TEXT('A');
		if (dwDriveNumber > 25)
			dwDriveNumber = szDrive[0] - TEXT('a');

		if (dwDriveNumber > 25) {
			usage();
			break;
		}

		_tprintf(TEXT("Enabling on drive %c.\n"), dwDriveNumber + TEXT('A'));
		fSuccess = NdasRoFilterEnableFilter(hFilter, dwDriveNumber, FALSE);

	} else if (lstrcmpi(szCommand,TEXT("quit")) == 0) {
		break;
	} else {
		usage();
	}

	if (!fSuccess) {
		_tprintf(TEXT("Error %d.\n"), ::GetLastError());
	}

	}

	::CloseHandle(hFilter);
	return fSuccess ? 0 : 1;
}
