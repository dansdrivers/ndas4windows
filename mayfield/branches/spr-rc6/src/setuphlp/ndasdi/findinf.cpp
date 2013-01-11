#include "stdafx.h"
#include "pnpdevinst.h"
#include "setupsup.h"
#include "xdebug.h"

/*
BOOL 
WINAPI
pIsNdasHardwareID(LPCTSTR szHardwareID)
{
	if (0 == lstrcmpi(szHardwareID, _T("nkc_lpx")) ||
		0 == lstrcmpi(szHardwareID, _T("root\\lanscsibus")) ||
		0 == lstrcmpi(szHardwareID, _T("LANSCSIBus\\NetDisk_V0")) ||
		0 == lstrcmpi(szHardwareID, _T("root\\ndasbus")) ||
		0 == lstrcmpi(szHardwareID, _T("ndas\\scsiadapter_r01")))
	{
		return TRUE;
	}

	return FALSE;
}
*/

BOOL
pCompareHardwareID(
	LPCTSTR szHardwareID, 
	LPCTSTR mszFindingHardwareIDs)
{
	LPCTSTR lpszCur = mszFindingHardwareIDs;

	while (TRUE) {
		if (_T('\0') == *lpszCur) {
			return FALSE;
		}

		DBGPRT_INFO(_T("Comparing %s\n"), lpszCur);
		if (0 == lstrcmpi(szHardwareID, lpszCur)) {
			DBGPRT_INFO(_T("Found %s\n"), lpszCur);
			return TRUE;
		}
		// next hardware ID
		_ASSERTE(!::IsBadStringPtr(lpszCur, -1));
		while (_T('\0') != *lpszCur) {
			++lpszCur;
		}
		_ASSERTE(!::IsBadStringPtr(lpszCur, -1));
		++lpszCur;
	}
}

BOOL 
pProcessModelSection(
	HINF hInfFile, 
	LPCTSTR szModelSection,
	LPCTSTR mszHardwareIDs)
{
	INFCONTEXT infContext = {0};
	TCHAR szHardwareID[MAX_PATH] = {0};

	BOOL fSuccess = ::SetupFindFirstLine(
		hInfFile, 
		szModelSection,
		NULL,
		&infContext);

	if (!fSuccess) {
		return FALSE;
	}

	do {

		fSuccess = ::SetupGetStringField(
			&infContext, 2, szHardwareID, MAX_PATH, NULL);
		
		if (!fSuccess) {
			return FALSE;
		}

		fSuccess = pCompareHardwareID(szHardwareID, mszHardwareIDs);
		if (fSuccess) {
			return TRUE;
		}

		fSuccess = ::SetupFindNextLine(
			&infContext, &infContext);

	} while (fSuccess);

	return FALSE;
	
}

BOOL 
pProcessManufacturerSection(
	HINF hInfFile,
	LPCTSTR mszHardwareIDs)
{
	INFCONTEXT infContext = {0};
	TCHAR szModel[MAX_PATH] = {0};

	BOOL fSuccess = ::SetupFindFirstLine(
		hInfFile,
		_T("Manufacturer"),
		NULL,
		&infContext);

	if (!fSuccess) {
		return FALSE;
	}

	do {

		fSuccess = ::SetupGetStringField(
			&infContext, 1, szModel, MAX_PATH, NULL);
		
		if (!fSuccess) {
			return FALSE;
		}
		
		fSuccess = pProcessModelSection(hInfFile, szModel, mszHardwareIDs);
		if (fSuccess) {
			return TRUE;
		}
		
		fSuccess = ::SetupFindNextLine(
			&infContext, &infContext);

	} while (fSuccess);

	return FALSE;
}

BOOL 
pCompareINF(
	LPCTSTR szInfFile, 
	LPCTSTR mszHardwareIDs)
{
	HINF hInf = ::SetupOpenInfFile(
		szInfFile,
		NULL,
		INF_STYLE_WIN4,
		0);

	if (INVALID_HANDLE_VALUE == hInf) {
		return FALSE;
	}
	
	BOOL fMatch = pProcessManufacturerSection(hInf, mszHardwareIDs);

	::SetupCloseInfFile(hInf);

	return fMatch;
}

NDASDI_API
BOOL 
WINAPI
NdasDiDeleteOEMInf(
	IN LPCTSTR szFindSpec OPTIONAL, /* if null, oem*.inf */
	IN LPCTSTR mszHardwareIDs, /* should be double _T('\0') terminated */
	IN DWORD dwFlags,
	IN NDASDI_DELETE_OEM_INF_CALLBACK_PROC pfnCallback OPTIONAL,
	IN LPVOID lpContext OPTIONAL)
{
	_ASSERTE(NULL == pfnCallback || !::IsBadCodePtr((FARPROC)pfnCallback));

	BOOL fSuccess = FALSE;

	TCHAR szInfPath[MAX_PATH];
	TCHAR szInfFindSpec[MAX_PATH];

	UINT uichInfPath = ::GetSystemWindowsDirectory(szInfPath, MAX_PATH);
	_ASSERTE(0 != uichInfPath);

	fSuccess = ::PathAppend(szInfPath, _T("INF"));
	_ASSERTE(fSuccess);

	HRESULT hr = ::StringCchCopy(szInfFindSpec, MAX_PATH, szInfPath);
	_ASSERTE(SUCCEEDED(hr));

	fSuccess = ::PathAppend(szInfFindSpec, szFindSpec ? szFindSpec : _T("oem*.inf"));
	_ASSERTE(fSuccess);
	if (!fSuccess){
		return FALSE;
	}

	WIN32_FIND_DATA ffData = {0};

	// prevent file open error dialog from the system
	UINT oldErrorMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);

	HANDLE hFindFile = ::FindFirstFile(szInfFindSpec, &ffData);
	if (INVALID_HANDLE_VALUE == hFindFile) {
		::SetErrorMode(oldErrorMode);
		return FALSE;
	}

	do {

		if (pCompareINF(ffData.cFileName, mszHardwareIDs)) {

			BOOL fSuccess = Setupapi_SetupUninstallOEMInf(
				ffData.cFileName, 
				dwFlags, 
				NULL);

			if (pfnCallback) {
				BOOL fCont = pfnCallback(
					fSuccess ? 0 : ::GetLastError(),
					szInfPath,
					ffData.cFileName,
					lpContext);
				if (!fCont) {
					break;
				}
			}
		}

		fSuccess = ::FindNextFile(hFindFile, &ffData);

	} while (fSuccess);

	::FindClose(hFindFile);
	::SetErrorMode(oldErrorMode);

	return TRUE;
}

/*
BOOL
WINAPI
pDeleteCallback(DWORD dwError, LPCTSTR szPath, LPCTSTR szFileName, LPVOID lpContext)
{
	_tprintf(_T("Error %d, Path %s, File %s\n"), dwError, szPath, szFileName);
	return TRUE;
}
*/

/*
int __cdecl _tmain(int argc, TCHAR** argv)
{
	DeleteOEMInf(NULL, pIsNdasHardwareID, pDeleteCallback, NULL);
	return 0;
}

*/
