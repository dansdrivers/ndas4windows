#include "ActivateWarnDlg.h"

#include "NDSetup.h" // DebugPrintf
#ifndef NDS_PREBOOT_REQUIRED
#error IsWindows2000OrLater
#endif
#include <shlobj.h>

#define IsWindows2000OrLater() IsSystemEqualOrLater(5,0,0)
#define IsWindowsXPOrLater() IsSystemEqualOrLater(5,1,0)
#define IsWindowsServer2003OrLater() IsSystemEqualOrLater(5,2,0)

BOOL IsSystemEqualOrLater(DWORD dwMajorVersion, DWORD dwMinorVersion, WORD wServicePackMajor);

namespace awdlgproc {
	// DLGTEMPLATEEX is not defined at anywhere.
#define OFFSET_DLGTEMPLATEEX 26

	BOOL GetInstallationWindowCaption(WCHAR *lpszCaption)
	{
		BOOL bReturn = FALSE;
		TCHAR szSetupApiPath[MAX_PATH];
		HMODULE hLibrary = NULL;
		HRSRC hResInfo = NULL;
		BYTE *lpResLock = NULL;
		HGLOBAL hResData = NULL;
		WORD menu = 0;
		WORD windowClass = 0;
		int iResourceNumber = 0;

		int iTemp;
		int i;

		DebugPrintf(_T("IN GetInstallationWindowCaption\n"));

		// get SetupApi.dll path
		SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_DEFAULT, szSetupApiPath);
		StringCchCat(szSetupApiPath, MAX_PATH, _T("\\SetupApi.dll"));
		
		DLGTEMPLATE dlgTemplete;
		
		// Load resource
		hLibrary = LoadLibrary(szSetupApiPath);
		if(!hLibrary)
		{
			goto out;
		}
		DebugPrintf(_T("- Library load\n"));
		
		// win 2k : 5314
		// win xp : 5316
		iResourceNumber = IsWindowsXPOrLater() ? 5316 : 5314; 
		
		hResInfo = FindResource(hLibrary, MAKEINTRESOURCE(iResourceNumber), RT_DIALOG);
		if(!hResInfo)
		{
			goto out;
		}
		DebugPrintf(_T("- Resource found\n"));
		
		hResData = LoadResource(hLibrary, hResInfo);
		
		lpResLock = (BYTE *)LockResource(hResData);
		if(!lpResLock)
		{
			goto out;
		}
		DebugPrintf(_T("- Library locked\n"));
		
		// Read Dialog Templete
		CopyMemory(&dlgTemplete, lpResLock, sizeof(DLGTEMPLATE));
		if(0xFFFF == HIWORD(dlgTemplete.style)) // extended style
		{
			CopyMemory(&menu, (WCHAR *)(lpResLock + OFFSET_DLGTEMPLATEEX), sizeof(WORD));
			if(menu)
				goto out;
			
			CopyMemory(&windowClass, (WCHAR *)(lpResLock + OFFSET_DLGTEMPLATEEX + sizeof(WORD)), sizeof(WORD));
			if(windowClass)
				goto out;
			
			StringCchCopyW(lpszCaption, MAX_PATH, (WCHAR *)(lpResLock + OFFSET_DLGTEMPLATEEX + sizeof(WORD) + sizeof(WORD)));
		}
		else // standard style
		{
			CopyMemory(&menu, (WCHAR *)(lpResLock + sizeof(DLGTEMPLATE)), sizeof(WORD));
			if(menu)
				goto out;
			
			CopyMemory(&windowClass, (WCHAR *)(lpResLock + sizeof(DLGTEMPLATE) + sizeof(WORD)), sizeof(WORD));
			if(windowClass)
				goto out;
			
			StringCchCopyW(lpszCaption, MAX_PATH, (WCHAR *)(lpResLock + sizeof(DLGTEMPLATE) + sizeof(WORD) + sizeof(WORD)));
		}
		DebugPrintf(_T("- String loaded : %s"), lpszCaption);

		for(i = 0; lpszCaption[i] != 0; i++)
		{
			iTemp = (int)lpszCaption[i];
			DebugPrintf(_T("%x "), iTemp);
		}
		DebugPrintf(_T("\n"));
		
//		wprintf(L"%s\n", lpszCaption);
		
		bReturn = TRUE;
out:
		if(hLibrary)
			FreeLibrary(hLibrary);
		
		
		return bReturn;
	}
	
	BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
	{
		WCHAR wszCaption[MAX_PATH];
		WCHAR *pszCompare = (WCHAR *)lParam;
		
		GetWindowTextW(hwnd, wszCaption, MAX_PATH);
		if(0 == wcslen(wszCaption))
		{
			return TRUE;
		}
		
		if(0 == wcscmp(wszCaption, pszCompare))
		{
			DebugPrintf(_T("Window Found %s\n"), wszCaption);
			
//			SendMessage(hwnd, WM_COMMAND, 0x000014B7, NULL); // send BN_CLICK message
			DebugPrintf(_T("SetWindowPos - HWND_TOPMOST\n"));
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
			return FALSE;
		}
		
		return TRUE;
		
		
//		wprintf(wszCaption);
		
/*
		for(int i = 0; 0 != wszCaption[i]; i++)
		{
			printf("%x ", (int)(wszCaption[i]));
		}
		
		printf("\n");
*/
		
		return TRUE;
	}

	//
	// Thread synchronization variable
	//
	volatile LONG lThreadCount = 0;
	
	DWORD WINAPI ActivateWarnDlgProc(LPVOID lvParam)
	{
		DWORD	dwInterval = 1000;
		HWND	hDlg;
		int		i;
		const int nCaption = 2, nMaxString = 255;
		TCHAR	szCaption[nCaption][nMaxString + 1];
		BOOL	bFound;
		WCHAR	wszCaption[MAX_PATH];

		DebugPrintf(_T("ActivateWarnDlgProc()\n"));

		StringCchPrintf(szCaption[0], nMaxString, TEXT("Digital Signature Not Found"));
		StringCchPrintf(szCaption[1], nMaxString, TEXT("Software Installation"));
		// LoadString(ghInst, IDS_DLGCAP_01, szCaption[0], nMaxString);
		// LoadString(ghInst, IDS_DLGCAP_02, szCaption[1], nMaxString);
		DebugPrintf(TEXT("Caption: %s\n"), szCaption[0]);
		DebugPrintf(TEXT("Caption: %s\n"), szCaption[1]);
		
		bFound = FALSE;

		// I'm not sure if this cheat still works for 2003
		if(IsWindowsServer2003OrLater() || !GetInstallationWindowCaption(wszCaption))
			bFound = TRUE;

		DebugPrintf(TEXT("Caption : %s\n"), wszCaption);

		while (bFound == FALSE)
		{
			if (0 != lThreadCount) break;
			Sleep(dwInterval);	
			DebugPrintf(_T("\nSeeking Window\n"));
			EnumWindows(EnumWindowsProc, (LPARAM)wszCaption);
			DebugPrintf(_T("Complete\n\n\n"));

/*
			for (i = 0; i < nCaption; i++)
			{
				DebugPrintf(TEXT("Trying to find a dialog box captioned %s\n"), szCaption[i]);
				hDlg = FindWindowEx(NULL, NULL, TEXT("#32770"), szCaption[i]);
				if (NULL != hDlg)
				{
					DebugPrintf(TEXT("Found and activate %X\n"), hDlg);
					SetForegroundWindow(hDlg);
					SetActiveWindow(hDlg);
					SetFocus(hDlg);
					bFound = TRUE;
					break;
				}
			}
*/
		}

		InterlockedExchange(&lThreadCount, 0);
		DebugPrintf(TEXT("ThActivateWarnDlg is done.\n"));
		return ERROR_SUCCESS;
	}
}
