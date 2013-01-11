#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <shlwapi.h>
#include <strsafe.h>

#include "ndas/ndastypeex.h"
#include "ndas/ndasdib.h"
#include "ndas/ndassyscfg.h"
#include "ndas/ndasautoregscope.h"

#include "xs/xstrhelp.h"

//
// get the option parameters
//
typedef struct _OPTION_VALUE {
	LPCTSTR option;
	BOOL is_set;
	LPTSTR pval;
} OPTION_VALUE, *POPTION_VALUE;

//
// OPTION_VALUE is NULL terminated
//
// e.g. OPTION_VALUE optValues[] = {{ _T("id"), NULL }, {NULL, NULL}}
//
static const int GETOPT_ERROR_SUCCESS = 0;
static const int GETOPT_UNKNOWN_OPTION = 1;

void header()
{
	_tprintf(
		_T("NDAS Device Auto Registration Scope Management Version 1.01\n")
		_T("Copyright (C) 2003-2004 XIMETA, Inc.\n\n"));
}

int 
getopts(int* pArgc, TCHAR*** ppArgs, OPTION_VALUE* pOptValues);

// {C9463038-6917-4758-8AFE-3738305A86A6}
static CONST GUID NDAS_ENC_GUID = 
{ 0xc9463038, 0x6917, 0x4758, { 0x8a, 0xfe, 0x37, 0x38, 0x30, 0x5a, 0x86, 0xa6 } };

//
// ndascope set <range_begin> <range_end> <ro|rw>
//  
// ndascope view
//

static LPCTSTR pNdasContentEncryptMethodString(unsigned _int16 method);
static VOID pShowErrorMessage(DWORD dwError = ::GetLastError());
static BOOL pHexCharToByte(TCHAR ch, BYTE& bval);
static BOOL pParseNdasDeviceID(NDAS_DEVICE_ID& deviceID, LPCTSTR lpsz);

int usage()
{
	_tprintf(
		_T("usage: ndasenc <command> [options]\n")
		_T("\n")
		_T("Available commands:\n")
		_T("\n")
		_T("  set <addr_start> <addr_end> <ro|rw> ...\n")
		_T("\n")
		_T("    Set Auto Registration Address Scope to the system.\n")
		_T("    Address Format: 00AACCFFFFFF\n")
		_T("\n")
		_T("  clear\n")
		_T("\n")
		_T("    Clear Auto Registration Scopes\n")
		_T("\n")
		_T("  view\n")
		_T("\n")
		_T("    Display the Auto Registration Address Scope of the system.\n")
		_T("\n")
		);

	return 255;
}

int setscope(int argc, TCHAR** argv)
{
	if (argc < 3) {
		return usage();
	}

	NDAS_DEVICE_ID rBegin = {0}, rEnd = {0};
	ACCESS_MASK autoRegAccess = 0;
	BOOL fSuccess;

	LPCTSTR lpBegin = argv[0], lpEnd = argv[1], lpAccess = argv[2];

	fSuccess = pParseNdasDeviceID(rBegin, lpBegin);
	if (!fSuccess) {
		_tprintf(_T("error: Invalid Device Address Scope (%s).\n"), lpBegin);
		return 255;
	}

	fSuccess = pParseNdasDeviceID(rEnd, lpEnd);
	if (!fSuccess) {
		_tprintf(_T("error: Invalid Device Address Scope (%s).\n"), lpEnd);
		return 255;
	}

	if (::memcmp(rBegin.Node, rEnd.Node, sizeof(rBegin.Node)) > 0) {
		_tprintf(_T("error: Scopes are not in a non-decreasing order.\n"));
		return 255;
	}

	if (0 == lstrcmpi(_T("ro"), lpAccess)) {
		autoRegAccess = GENERIC_READ;
	} else if (0 == lstrcmpi(_T("rw"), lpAccess)) {
		autoRegAccess = GENERIC_READ | GENERIC_WRITE;
	} else {
		_tprintf(_T("error: Invalid Device Access (%s).\n"), lpAccess);
		return 255;
	}

	_tprintf(_T("Scope: %02X:%02X:%02X:%02X:%02X:%02X - %02X:%02X:%02X:%02X:%02X:%02X: %08X"), 
		rBegin.Node[0], rBegin.Node[1], rBegin.Node[2],
		rBegin.Node[3], rBegin.Node[4], rBegin.Node[5],
		rEnd.Node[0], rEnd.Node[1], rEnd.Node[2],
		rEnd.Node[3], rEnd.Node[4], rEnd.Node[5],
		autoRegAccess);

	CNdasAutoRegScopeData arsData;

	fSuccess = arsData.AddScope(rBegin, rEnd, autoRegAccess);

	if (!fSuccess) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Creating scope data failed with error %08X.\n"), dwStatus);
		return dwStatus;
	}
	
	fSuccess = arsData.SaveToSystem();

	if (!fSuccess) {
		DWORD dwStatus = ::GetLastError();
		_tprintf(_T("Saving the scope data failed with error %08X.\n"), dwStatus);
		return dwStatus;
	}

	return 0;
}

int viewscope(int argc, TCHAR** argv)
{
	CNdasAutoRegScopeData arsData;

	BOOL fSuccess = arsData.LoadFromSystem();

	if (!fSuccess) {
		DWORD dwStatus = ::GetLastError();
		if (ERROR_FILE_NOT_FOUND == dwStatus) {
			_tprintf(_T("No Auto Registration Scopes are registered.\n"));
		} else {
			_tprintf(_T("Getting scope data failed with error %08X.\n"), dwStatus);
		}
		return dwStatus;
	}

	for (DWORD i = 0; i < arsData.GetCount(); ++i) {
		NDAS_DEVICE_ID rBegin, rEnd;
		ACCESS_MASK access;
		BOOL fSuccess = arsData.GetScope(i, rBegin, rEnd, access);
		_ASSERTE(fSuccess);
		_tprintf(_T("%d: %s - %s (%s)\n"),
			i,
			xs::CXSByteString(RTL_FIELD_SIZE(NDAS_DEVICE_ID, Node), rBegin.Node, _T(':')).ToString(),
			xs::CXSByteString(RTL_FIELD_SIZE(NDAS_DEVICE_ID, Node), rEnd.Node, _T(':')).ToString(),
			(access & GENERIC_WRITE) ? _T("rw") : _T("ro"));
	}

	return 0;
}

int clearscope(int argc, TCHAR** argv)
{
	BOOL fSuccess = ::NdasSysDeleteConfigValue(
		NDASSYS_SERVICE_REGKEY,
		NDASSYS_SERVICE_ARFLAGS_REGVAL);

	if (!fSuccess) {
		DWORD dwStatus = ::GetLastError();
		if (ERROR_FILE_NOT_FOUND == dwStatus) {
			_tprintf(_T("No Auto Registration Scopes are registered.\n"));
		} else {
			_tprintf(_T("Deleting Auto Registration Scope failed with error %08X\n"), dwStatus);
		}
		return dwStatus;
	}

	_tprintf(_T("Auto Registration Scopes are cleared successfully.\n"));

	return 0;
}

int __cdecl _tmain(int argc, TCHAR** argv)
{
	typedef int (*SUBCMDPROC)(int argc, TCHAR** argv);

	typedef struct _SUBCMDSPEC {
		LPCTSTR szCommandString;
		SUBCMDPROC pfn;
	} SUBCMDSPEC, *PSUBCMDSPEC;

	SUBCMDSPEC SubCmds[] = {
		{ _T("set"), setscope },
		{ _T("view"), viewscope },
		{ _T("clear"), clearscope },
	};

	static const size_t nSubCmds = sizeof(SubCmds) / sizeof(SubCmds[0]);

	header();
	
	BOOL fSuccess = FALSE;

	if (argc < 2) {
		return usage();
	}

	for (size_t i = 0; i < nSubCmds; ++i) {
		if (0 == ::lstrcmpi(SubCmds[i].szCommandString, argv[1])) {
			int iRet = SubCmds[i].pfn(argc - 2, argv + 2);
#ifdef _DEBUG
			_tprintf(_T("\nDEBUG: command returned: %d\n"), iRet);
#endif
			return iRet;
		}
	}

	return usage();
}

static VOID 
pShowErrorMessage(DWORD dwError)
{
	HMODULE hModule = ::LoadLibraryEx(
		_T("ndasmsg.dll"),
		NULL,
		LOAD_LIBRARY_AS_DATAFILE);

	LPTSTR lpszErrorMessage = NULL;

	if (dwError & APPLICATION_ERROR_MASK) {
		if (NULL != (HMODULE) hModule) {
			INT iChars = ::FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				(HMODULE) hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);
		}
	} else {
		INT iChars = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwError,
			0,
			(LPTSTR) &lpszErrorMessage,
			0,
			NULL);
	}

	if (NULL != lpszErrorMessage) {
		_tprintf(_T("Error : %u(%08X) : %s\n"), dwError, dwError, lpszErrorMessage);
		::LocalFree(lpszErrorMessage);
	} else {
		_tprintf(_T("Unknown error: %u(%08X)\n"), dwError, dwError);
	}

	FreeLibrary(hModule);
}

static BOOL 
pParseNdasDeviceID(NDAS_DEVICE_ID& deviceID, LPCTSTR lpsz)
{
	size_t cch = 0;
	HRESULT hr = ::StringCchLength(lpsz, STRSAFE_MAX_CCH, &cch);
	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr)) {
		return FALSE;
	}
	if (cch != 12) {
		return FALSE;
	}

	NDAS_DEVICE_ID tmpID = {0};

	for (DWORD i = 0; i < 6; ++i) {
		BYTE b_low = 0, b_high = 0;
		BOOL fSuccess = 
			pHexCharToByte(lpsz[i*2], b_high) &&
			pHexCharToByte(lpsz[i*2+1], b_low);
		if (!fSuccess) {
			return FALSE;
		}
		tmpID.Node[i] = (b_high << 4) + b_low;
	}

	deviceID = tmpID;
	return TRUE;
}

static BOOL
pHexCharToByte(TCHAR ch, BYTE& bval)
{
	if (ch >= _T('0') && ch <= '9') { 
		bval = ch - _T('0'); return TRUE; 
	}
	else if (ch >= _T('A') && ch <= 'F') { 
		bval = ch - _T('A') + 0xA; return TRUE; 
	}
	else if (ch >= _T('a') && ch <= 'f') { 
		bval = ch - _T('a') + 0xA; return TRUE; 
	}
	else return FALSE;
}

POPTION_VALUE
findoptptr(TCHAR* szOption, POPTION_VALUE pOptValues)
{
	for (POPTION_VALUE p = pOptValues; NULL != p->option; ++p) {
		if (0 == ::lstrcmpi(p->option, szOption)) {
			return p;
		}
	}
	return NULL;
}

int 
getopts(int* pArgc, TCHAR*** ppArgs, OPTION_VALUE* pOptValues)
{
	POPTION_VALUE pCurOpt = NULL;
	while (*pArgc > 0) {
		if (_T('-') == (**ppArgs)[0] || _T('/') == (**ppArgs)[0]) {
			POPTION_VALUE popt = findoptptr(&(**ppArgs)[1], pOptValues);
			if (NULL == popt) {
				return GETOPT_UNKNOWN_OPTION;
			}
			popt->is_set = TRUE;
			popt->pval = NULL;
			pCurOpt = popt;
		} else if (NULL != pCurOpt) {
			pCurOpt->pval = **ppArgs;
			pCurOpt = NULL;
		} else {
			return GETOPT_ERROR_SUCCESS;
		}
		--(*pArgc);
		++(*ppArgs);
	}
	return GETOPT_ERROR_SUCCESS;
}


