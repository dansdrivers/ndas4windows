#include <windows.h>
#include <regstr.h>
#include <tchar.h>
#include "dsig.h"
#include "osverify.h"

#define SIGTAB_REG_KEY      TEXT("Software\\Microsoft\\Driver Signing")
#define SIGTAB_REG_VALUE    TEXT("Policy")

#define REGSTR_PATH_WPA_PNP	TEXT("System\\CurrentControlSet\\Control\\Session Manager\\WPA\\PnP")
#define REGSTR_PATH_WPA_PNP_2003 TEXT("System\\WPA\\PnP")

typedef VOID (WINAPI *SetupGetRealSystemTimePtr)(VOID*);

static HMODULE g_hSetupApiDll = NULL;

#define pSetupGetRealSystemTime DSAIMP##__COUNTER__
#define GetDriverSigningPolicyInSystemXP DSAIMP##__COUNTER__
#define ApplyDriverSigningPolicyXP DSAIMP##__COUNTER__

static BOOL pSetupGetRealSystemTime(OUT LPSYSTEMTIME RealSystemTime);
DWORD GetDriverSigningPolicyInSystemXP();
BOOL ApplyDriverSigningPolicyXP(DWORD dwPolicy, BOOL bGlobal);

static BOOL pSetupGetRealSystemTime(OUT LPSYSTEMTIME RealSystemTime)
{
	if (NULL == g_hSetupApiDll) {
		g_hSetupApiDll = LoadLibrary(_T("setupapi.dll"));
		if (NULL == g_hSetupApiDll) {
			// _tprintf(_T("Unable to load setupapi.dll"));
			return FALSE;
		}
	}

	SetupGetRealSystemTimePtr fn = (SetupGetRealSystemTimePtr)
		GetProcAddress(g_hSetupApiDll, "pSetupGetRealSystemTime");

	if (NULL == fn) {
		FreeLibrary(g_hSetupApiDll);
		g_hSetupApiDll = NULL;
		return FALSE;
	}

	fn(RealSystemTime);

	if (NULL != g_hSetupApiDll) {
		FreeLibrary(g_hSetupApiDll);
		g_hSetupApiDll = NULL;
	}

	return TRUE;
}

BOOL ApplyDriverSigningPolicyXP(DWORD dwPolicy, BOOL bGlobal)
{
	HKEY    hKey;
	LONG    lRes;
	DWORD   dwSize, dwType, dwDisposition;

	if (bGlobal && !IsUserAdmin()) {
		SetLastError(ERROR_NO_ADMINISTRATORS);
		return FALSE;
	}

	lRes = RegCreateKeyEx(
		bGlobal ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, 
		SIGTAB_REG_KEY, 
		NULL, 
		NULL, 
		REG_OPTION_NON_VOLATILE, 
		KEY_ALL_ACCESS, 
		NULL, 
		&hKey, 
		&dwDisposition);

	if (ERROR_SUCCESS != lRes) {
		SetLastError(lRes);
		return FALSE;
	}

	dwType = REG_DWORD;
	dwSize = sizeof(dwPolicy);

	lRes = RegSetValueEx(
		hKey, 
		SIGTAB_REG_VALUE, 
		0, 
		dwType, 
		(CONST BYTE *) &dwPolicy, 
		dwSize);

	RegCloseKey(hKey);

	if (ERROR_SUCCESS != lRes) {
		SetLastError(lRes);
		return FALSE;
	}

	if (!bGlobal) {
		return TRUE;
	}

	SYSTEMTIME RealSystemTime;

	if (IS_WINDOWS_SERVER_2003_OR_LATER()) {
		lRes = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			REGSTR_PATH_WPA_PNP_2003,
			0,
			KEY_READ,
			&hKey);
	} else {
		lRes = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			REGSTR_PATH_WPA_PNP,
			0,
			KEY_READ,
			&hKey);
	}

	if (ERROR_SUCCESS != lRes) {
		return FALSE;
	}

	dwSize = sizeof(dwPolicy);

	DWORD dwSeed;
	lRes = RegQueryValueEx(hKey,
		TEXT("seed"),
		NULL,
		&dwType,
		(PBYTE)&dwSeed,
		&dwSize);

	if (ERROR_SUCCESS != lRes || 
		(dwType != REG_DWORD) || (dwSize != sizeof(dwPolicy)))
	{
		dwPolicy = 0;
	}

	RegCloseKey(hKey);

	RealSystemTime.wDayOfWeek = LOWORD(&hKey) | 4;
	RealSystemTime.wMinute = LOWORD(dwSeed);
	RealSystemTime.wYear = HIWORD(dwSeed);
	RealSystemTime.wMilliseconds = (LOWORD(&lRes)&~3072)|(WORD)((dwPolicy&3)<<10);

	return pSetupGetRealSystemTime(&RealSystemTime);
}

DSIGAPI BOOL WINAPI ApplyDriverSigningPolicy(DWORD dwPolicy, BOOL bGlobal)
{
	if (IS_WINDOWS_XP_OR_LATER()) {
		return ApplyDriverSigningPolicyXP(dwPolicy, bGlobal);
	}

	HKEY    hKey;
	LONG    lResult;
	DWORD   dwData, dwSize, dwType, dwDisposition;

	if (bGlobal && !IsUserAdmin()) {
		SetLastError(ERROR_NO_ADMINISTRATORS);
		return TRUE;
	}

	if (DRIVERSIGN_NONE != dwPolicy &&
		DRIVERSIGN_WARNING != dwPolicy &&
		DRIVERSIGN_BLOCKING != dwPolicy)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	lResult = RegCreateKeyEx(
		(bGlobal) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, 
		SIGTAB_REG_KEY, 
		NULL, 
		NULL, 
		REG_OPTION_NON_VOLATILE, 
		KEY_ALL_ACCESS, 
		NULL, 
		&hKey, 
		&dwDisposition);

	if (ERROR_SUCCESS != lResult) {
		SetLastError(lResult);
		return FALSE;
	}

	dwType = REG_DWORD;
	dwSize = sizeof(dwData);

	lResult = RegSetValueEx(
		hKey,
		SIGTAB_REG_VALUE, 
		0, 
		dwType, 
		(CONST BYTE *) &dwPolicy, 
		dwSize);

	RegCloseKey(hKey);

	if (ERROR_SUCCESS != lResult) {
		return FALSE;
	}

	return TRUE;
}

/*
    For driver signing, there are actually 3 sources of policy:

        1.  HKLM\Software\Microsoft\Driver Signing : Policy : REG_BINARY (REG_DWORD also supported)
            This is a Windows 98-compatible value that specifies the default
            behavior which applies to all users of the machine.

        2.  HKCU\Software\Microsoft\Driver Signing : Policy : REG_DWORD
            This specifies the user's preference for what behavior to employ
            upon verification failure.

        3.  HKCU\Software\Policies\Microsoft\Windows NT\Driver Signing : BehaviorOnFailedVerify : REG_DWORD
            This specifies the administrator-mandated policy on what behavior
            to employ upon verification failure.  This policy, if specified,
            overrides the user's preference.

    The algorithm for deciding on the behavior to employ is as follows:

        if (3) is specified {
            policy = (3)
        } else {
            policy = (2)
        }
        policy = MAX(policy, (1))

    Value indicating the policy in effect.  May be one of the following three values:

        DRIVERSIGN_NONE    -  silently succeed installation of unsigned/
                              incorrectly-signed files.  A PSS log entry will
                              be generated, however (as it will for all 3 types)
        DRIVERSIGN_WARNING -  warn the user, but let them choose whether or not
                              they still want to install the problematic file
        DRIVERSIGN_BLOCKING - do not allow the file to be installed
*/

DSIGAPI DWORD WINAPI CalcEffectiveDriverSigningPolicy(
	DWORD dwUser, DWORD dwPolicy, DWORD dwSystem)
{
	DWORD dwEffective;

	if (DRIVERSIGN_NOT_SET != dwPolicy) {
		dwEffective = dwPolicy;
	} else {
		dwEffective = dwUser;
	}

	if (DRIVERSIGN_NOT_SET != dwEffective) {
		dwEffective = max(dwEffective, dwSystem);
	} else {
		dwEffective = dwSystem;
	}

	return dwEffective;
}

DSIGAPI DWORD WINAPI GetEffectiveDriverSigningPolicy()
{
	DWORD dwEffective;
	DWORD dwSystem = GetDriverSigningPolicyInSystem();
	DWORD dwPolicy = GetDriverSigningPolicyInUserPolicy();

	if (DRIVERSIGN_NOT_SET != dwPolicy) {
		dwEffective = dwPolicy;
	} else {
		dwEffective = GetDriverSigningPolicyInUserPreference();
	}

	if (DRIVERSIGN_NOT_SET != dwEffective) {
		dwEffective = max(dwEffective, dwSystem);
	} else {
		dwEffective = dwSystem;
	}

	return dwEffective;
}

DWORD GetDriverSigningPolicyInSystemXP()
{
    SYSTEMTIME RealSystemTime;
    DWORD dwSize, dwType;
    DWORD dwDefault, dwPolicy, dwPreference;
	HKEY hKey;

    dwPolicy = dwPreference = (DWORD) -1;
	RealSystemTime.wDayOfWeek = LOWORD(&hKey) | 4;

	BOOL fSuccess = pSetupGetRealSystemTime(&RealSystemTime);
	if (!fSuccess) {
		return DRIVERSIGN_NOT_SET;
	}

	dwDefault = (((RealSystemTime.wMilliseconds+2)&15)^8)/4;

	UNREFERENCED_PARAMETER(dwSize);
	UNREFERENCED_PARAMETER(dwType);

	return dwDefault;
}

DSIGAPI DWORD WINAPI GetDriverSigningPolicyInSystem()
{
	if (IS_WINDOWS_XP_OR_LATER()) {
		return GetDriverSigningPolicyInSystemXP();
	}

	CONST TCHAR pszDrvSignPath[] = REGSTR_PATH_DRIVERSIGN;
	CONST TCHAR pszDrvSignPolicyValue[] = REGSTR_VAL_POLICY;

	HKEY hKey;
	LONG lRes;
	DWORD dwType, dwSize, dwDefault;

	//
	// First, retrieve the system default policy (under HKLM).
	//
	lRes = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		pszDrvSignPath,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lRes) {
		return DRIVERSIGN_NOT_SET;
	}

	dwSize = sizeof(dwDefault);
	lRes = RegQueryValueEx(hKey,
		pszDrvSignPolicyValue,
		NULL,
		&dwType,
		(PBYTE)&dwDefault,
		&dwSize);
	
	if (ERROR_SUCCESS != lRes) {
		RegCloseKey(hKey);
		return DRIVERSIGN_NOT_SET;
	}

	//
	// To be compatible with Win98's value, we support both REG_BINARY
	// and REG_DWORD.
	//
	if((dwType == REG_BINARY) && (dwSize >= sizeof(BYTE))) {
		//
		// Use the value contained in the first byte of the buffer.
		//
		dwDefault = (DWORD)*((PBYTE)&dwDefault);

	} else if((dwType != REG_DWORD) || (dwSize != sizeof(DWORD))) {
		//
		// Bogus entry for system default policy--ignore it.
		//
		dwDefault = DRIVERSIGN_NONE;
	}

	//
	// Finally, make sure a valid policy value was specified.
	//
	if((dwDefault != DRIVERSIGN_NONE) &&
		(dwDefault != DRIVERSIGN_WARNING) &&
		(dwDefault != DRIVERSIGN_BLOCKING)) 
	{
			//
			// Bogus entry for system default policy--ignore it.
			//
			dwDefault = DRIVERSIGN_NOT_SET; // DRIVERSIGN_NONE;
	}

	RegCloseKey(hKey);
	return dwDefault;
}

DSIGAPI DWORD WINAPI GetDriverSigningPolicyInUserPreference()
{
	CONST TCHAR pszDrvSignPath[]                     = REGSTR_PATH_DRIVERSIGN;
	CONST TCHAR pszDrvSignPolicyValue[]              = REGSTR_VAL_POLICY;

	DWORD dwPreference;

	//
	// Retrieve the user preference.
	//

	DWORD dwSize, dwType;
	HKEY hKey;
	LONG lRes;

	lRes = RegOpenKeyEx(
		HKEY_CURRENT_USER,
		pszDrvSignPath,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lRes) {
		return DRIVERSIGN_NOT_SET;
	}

	dwSize = sizeof(dwPreference);

	lRes = RegQueryValueEx(hKey,
		pszDrvSignPolicyValue,
		NULL,
		&dwType,
		(PBYTE)&dwPreference,
		&dwSize);

	if (ERROR_SUCCESS != lRes) {
		RegCloseKey(hKey);
		return DRIVERSIGN_NOT_SET;
	}

	if((dwType != REG_DWORD) ||
		(dwSize != sizeof(DWORD)) ||
		!((dwPreference == DRIVERSIGN_NONE) || 
		(dwPreference == DRIVERSIGN_WARNING) || 
		(dwPreference == DRIVERSIGN_BLOCKING)))
	{
		//
		// Bogus entry for user preference--ignore it.
		//
		dwPreference = DRIVERSIGN_NOT_SET; // DRIVERSIGN_NONE;
	}

	RegCloseKey(hKey);

	return dwPreference;
}

DSIGAPI DWORD WINAPI GetDriverSigningPolicyInUserPolicy()
{
	CONST TCHAR pszDrvSignPolicyPath[]               = REGSTR_PATH_DRIVERSIGN_POLICY;
	CONST TCHAR pszDrvSignBehaviorOnFailedVerifyDS[] = REGSTR_VAL_BEHAVIOR_ON_FAILED_VERIFY;

	//
	// Retrieve the user policy.
	//

	DWORD dwPolicy, dwType, dwSize;
	HKEY hKey;
	LONG lRes;

	lRes = RegOpenKeyEx(
		HKEY_CURRENT_USER,
		pszDrvSignPolicyPath,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lRes) {
		return DRIVERSIGN_NOT_SET;
	}

	dwSize = sizeof(dwPolicy);

	lRes = RegQueryValueEx(hKey,
		pszDrvSignBehaviorOnFailedVerifyDS,
		NULL,
		&dwType,
		(PBYTE)&dwPolicy,
		&dwSize);

	if (ERROR_SUCCESS != lRes) {
		RegCloseKey(hKey);
		return DRIVERSIGN_NOT_SET;
	}

	if((dwType != REG_DWORD) ||
		(dwSize != sizeof(DWORD)) ||
		!((dwPolicy == DRIVERSIGN_NONE) || 
		(dwPolicy == DRIVERSIGN_WARNING) || 
		(dwPolicy == DRIVERSIGN_BLOCKING)))
	{
		//
		// Bogus entry for user preference--ignore it.
		//
		dwPolicy = DRIVERSIGN_NOT_SET; // DRIVERSIGN_NONE;
	}

	RegCloseKey(hKey);

	return dwPolicy;

}

