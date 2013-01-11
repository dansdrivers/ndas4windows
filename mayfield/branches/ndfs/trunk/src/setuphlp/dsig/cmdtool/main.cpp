#include <windows.h>
#include <tchar.h>
// #ifndef _UNICODE
#include <stdio.h>
// #endif
#include "dsig.h"

#define USAGE_CHECK _T("dsig check")
#define USAGE_SET _T("dsig set <ignore|warn|block> <user|system>")

LPCTSTR SigningPolicyString(DWORD dwPolicy)
{
	switch(dwPolicy) {
	case DRIVERSIGN_NONE: return _T("IGNORE");
	case DRIVERSIGN_BLOCKING: return _T("BLOCK");
	case DRIVERSIGN_WARNING: return _T("WARN");
	case DRIVERSIGN_NOT_SET: return _T("(none)");
	default: return _T("(Invalid Value)");
	}
}

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2) {
		_tprintf(
			_T("usage: ") USAGE_CHECK _T("\n")
			_T("       ") USAGE_SET _T("\n"));
		return 255;
	}

	--argc; ++argv;

	if (0 == lstrcmpi(argv[0], _T("check"))) {
		
		DWORD dwDefault = GetDriverSigningPolicyInSystem();
		DWORD dwPolicy = GetDriverSigningPolicyInUserPolicy();
		DWORD dwUserPref = GetDriverSigningPolicyInUserPreference();
		DWORD dwEffective = CalcEffectiveDriverSigningPolicy(
			dwUserPref, 
			dwPolicy, 
			dwDefault);

		_tprintf(_T("System Default  : %s\n"), SigningPolicyString(dwDefault));
		_tprintf(_T("DS Policy       : %s\n"), SigningPolicyString(dwPolicy));
		_tprintf(_T("User Preference : %s\n"), SigningPolicyString(dwUserPref));
		_tprintf(_T("Effective Policy: %s\n"), SigningPolicyString(dwEffective));

	} else if (0 == lstrcmpi(argv[0], _T("set"))) {

		if (argc != 3) {
			_tprintf(USAGE_SET _T("\n"));
			return 255;
		}

		DWORD dwPolicy = 0;
		if (0 == lstrcmpi(argv[1], _T("ignore"))) {
			dwPolicy = DRIVERSIGN_NONE;
		} else if (0 == lstrcmpi(argv[1], _T("warn"))) {
			dwPolicy = DRIVERSIGN_WARNING;
		} else if (0 == lstrcmpi(argv[1], _T("block"))) {
			dwPolicy = DRIVERSIGN_BLOCKING;
		} else {
			_tprintf(USAGE_SET _T("\n"));
			return 255;
		}

		BOOL bGlobal = FALSE;
		if (0 == lstrcmpi(argv[2], _T("system"))) {
			bGlobal = TRUE;
		} else if (0 == lstrcmpi(argv[2], _T("user"))){
			bGlobal = FALSE;
		} else {
			_tprintf(USAGE_SET _T("\n"));
			return 255;
		}

		BOOL fSuccess = ApplyDriverSigningPolicy(dwPolicy, bGlobal);

		if (!fSuccess) {
			_tprintf(_T("Applying failed with error %d (%08X).\n"),
				GetLastError(), GetLastError());
			return 1;
		} else {
			_tprintf(_T("Applied successfully.\n"));
		}

	} else {
		_tprintf(
			_T("usage: ") USAGE_CHECK _T("\n")
			_T("       ") USAGE_SET _T("\n"));
		return 255;
	}

	return 0;
}

