#include "RebootFlag.h"
#include "NDSetup.h"

//++
//
// Reboot Flag functions
//

BOOL SetRebootFlag(BOOL bReboot)
{
	LONG lRet;
	HKEY hKey;
	DWORD dwDisp;
	BOOL bOldData = FALSE; // default is FALSE
	DWORD dwData;
	DWORD cbData;

	DebugPrintf(TEXT("Entering SetRebootFlag(%d)...\n"), bReboot);

	lRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, NDSETUP_REBOOTFLAG_KEY, 
		0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, NULL, &hKey, &dwDisp);
	
	if (ERROR_SUCCESS != lRet)
	{
		LogPrintfErr(TEXT("Error opening HKLM\\%s"), NDSETUP_REBOOTFLAG_KEY);
		DebugPrintf(TEXT("Leaving SetRebootFlag()... %d\n"), FALSE);
		return FALSE;
	}

	dwData = (bReboot) ? 1 : 0;
	cbData = sizeof(DWORD);

	lRet = RegSetValueEx(hKey, NDSETUP_REBOOTFLAG_VALUE, 0, 
		REG_DWORD, (const BYTE*) &dwData, cbData);

	RegCloseKey(hKey);
	
	if (ERROR_SUCCESS != lRet)
	{
		LogPrintfErr(TEXT("Error set the flag"));
		DebugPrintf(TEXT("Leaving SetRebootFlag()... %d\n"), FALSE);
		return FALSE;
	}

	DebugPrintf(TEXT("Leaving SetRebootFlag()... %d\n"), TRUE);
	return TRUE;
}

BOOL GetRebootFlag(PBOOL pbReboot)
{
	LONG lRet;
	HKEY hKey;
	BOOL  bData = FALSE; // default is FALSE
	DWORD dwData;
	DWORD cbData;
	DWORD dwType;

	DebugPrintf(TEXT("Entering GetRebootFlag()...\n"));
	//
	// by default, *pbReboot is set to FALSE
	//

	if (NULL != pbReboot) 
		*pbReboot = FALSE;

	lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NDSETUP_REBOOTFLAG_KEY, 0, KEY_ALL_ACCESS, &hKey);
	
	if (ERROR_SUCCESS != lRet)
	{
		LogPrintfErr(TEXT("Error opening HKLM\\%s"), NDSETUP_REBOOTFLAG_KEY);
		DebugPrintf(TEXT("Leaving GetRebootFlag()... %d\n"), FALSE);
		return FALSE;
	}

	lRet = RegQueryValueEx(hKey, NDSETUP_REBOOTFLAG_VALUE, NULL, &dwType, (LPBYTE) &dwData, &cbData);

	if (ERROR_SUCCESS != lRet)
	{
		LogPrintfErr(TEXT("Error query value HKLM\\%s, %s"), NDSETUP_REBOOTFLAG_KEY, NDSETUP_REBOOTFLAG_VALUE);
		RegCloseKey(hKey);
		DebugPrintf(TEXT("Leaving GetRebootFlag()... %d\n"), FALSE);
		return FALSE;
	}

	RegCloseKey(hKey);

	if (REG_DWORD == dwType && 1 == dwData)
	{
		if (NULL != pbReboot)
			*pbReboot = TRUE;
	}

	DebugPrintf(TEXT("Leaving GetRebootFlag()... %d\n"), TRUE);
	return TRUE;
}

BOOL ClearRebootFlag()
{
	LONG lRet;
	HKEY hKey;

	DebugPrintf(TEXT("Entering ClearRebootFlag()...\n"));

	// try to open the existing key, when it fails just ignore to clear the flag
	lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NDSETUP_REBOOTFLAG_KEY, 0, KEY_ALL_ACCESS, &hKey);
	if (ERROR_SUCCESS != lRet)
	{
		DebugPrintf(TEXT("Leaving ClearRebootFlag()... %d : RegOpenKeyEx Error\n"), TRUE);
		return TRUE;
	}
	RegCloseKey(hKey);

	// when there are an existing key, try to delete it
	lRet = RegDeleteKey(HKEY_LOCAL_MACHINE, NDSETUP_REBOOTFLAG_KEY);
	if (ERROR_SUCCESS != lRet)
	{
		LogPrintfErr(TEXT("Error deleting HKLM\\%s"), NDSETUP_REBOOTFLAG_KEY);

		// if failed to delete the key itself, just clear the value
		lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NDSETUP_REBOOTFLAG_KEY, 0, KEY_ALL_ACCESS, &hKey);
		if (ERROR_SUCCESS != lRet)
		{
			LogPrintfErr(TEXT("Error opening the key HKLM\\%s"), NDSETUP_REBOOTFLAG_KEY);
			DebugPrintf(TEXT("Leaving ClearRebootFlag()... %d\n"), FALSE);
			return FALSE;
		}

		lRet = RegDeleteValue(hKey, NDSETUP_REBOOTFLAG_VALUE);
		if (ERROR_SUCCESS != lRet)
		{
			LogPrintfErr(TEXT("Error delete the value HKLM\\%s, %s"), NDSETUP_REBOOTFLAG_KEY, NDSETUP_REBOOTFLAG_VALUE);
			RegCloseKey(hKey);
			DebugPrintf(TEXT("Leaving ClearRebootFlag()... %d\n"), FALSE);
			return FALSE;
		}
		RegCloseKey(hKey);
		DebugPrintf(TEXT("Leaving ClearRebootFlag()... %d\n"), TRUE);
		return TRUE;
	}
	DebugPrintf(TEXT("Leaving ClearRebootFlag()... %d\n"), TRUE);
	return TRUE;
}
