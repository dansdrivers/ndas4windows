#include "stdafx.h"
#include "pnpdevinst.h"
#include "setupsup.h"
#include "xdebug.h"

BOOL
pCompareHardwareID(
	LPCTSTR szHardwareID, 
	LPCTSTR mszFindingHardwareIDs)
{
	LPCTSTR lpszCur = mszFindingHardwareIDs;

	while (TRUE) 
	{
		if (_T('\0') == *lpszCur) 
		{
			DBGPRT_INFO(_T("Hardware ID %s not found.\n"), mszFindingHardwareIDs);
			return FALSE;
		}

		DBGPRT_INFO(_T("Comparing Hardware ID %s against %s\n"), szHardwareID, lpszCur);

		if (0 == lstrcmpi(szHardwareID, lpszCur)) 
		{
			DBGPRT_INFO(_T("Found %s\n"), lpszCur);
			return TRUE;
		}

		// next hardware ID
		_ASSERTE(!::IsBadStringPtr(lpszCur, -1));
		while (_T('\0') != *lpszCur) 
		{
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
	LPCTSTR szPlatform,
	LPCTSTR mszHardwareIDs)
{
	DBGPRT_INFO(_T("[%s.%s]\n"), szModelSection, szPlatform ? szPlatform : _T("none"));

	TCHAR Section[255] = {0};

	HRESULT hr = ::StringCchCopy(Section, RTL_NUMBER_OF(Section), szModelSection);
	_ASSERTE(SUCCEEDED(hr));

	// If szPlatform is given, append to the section
	if (szPlatform && szPlatform[0] != 0)
	{
		hr = ::StringCchCat(Section, RTL_NUMBER_OF(Section), _T("."));
		_ASSERTE(SUCCEEDED(hr));
		hr = ::StringCchCat(Section, RTL_NUMBER_OF(Section), szPlatform);
		_ASSERTE(SUCCEEDED(hr));
	}

	INFCONTEXT infContext = {0};
	BOOL fSuccess = ::SetupFindFirstLine(
		hInfFile, 
		Section,
		NULL,
		&infContext);

	if (!fSuccess) 
	{
		return FALSE;
	}

	do 
	{
		TCHAR szHardwareID[MAX_PATH] = {0};
		fSuccess = ::SetupGetStringField(
			&infContext, 2, szHardwareID, MAX_PATH, NULL);
		
		if (!fSuccess) 
		{
			return FALSE;
		}

		fSuccess = pCompareHardwareID(szHardwareID, mszHardwareIDs);

		if (fSuccess) 
		{
			return TRUE;
		}

		fSuccess = ::SetupFindNextLine(&infContext, &infContext);

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

	if (!fSuccess) 
	{
		DBGPRT_WARN(_T("No Manufacturer section found.\n"));
		return FALSE;
	}

	do 
	{
		// Model Section Fields
		fSuccess = ::SetupGetStringField(
			&infContext, 1, szModel, MAX_PATH, NULL);
		if (!fSuccess) 
		{
			DBGPRT_ERR_EX(_T("No Model section found.\n"));
			return FALSE;
		}
		
		DBGPRT_INFO(_T("Model: %s\n"), szModel);

		DWORD fieldCount = ::SetupGetFieldCount(&infContext);
		DBGPRT_INFO(_T("Decorations: %d\n"), fieldCount);
		if (fieldCount < 2)
		{
			DBGPRT_INFO(_T("TargetOS: (none)\n"));
			fSuccess = pProcessModelSection(
				hInfFile, szModel, NULL, mszHardwareIDs);
			if (fSuccess) 
			{
				return TRUE;
			}
		}
		else
		{
			// String Field is 1-based index
			for (DWORD i = 2; i <= fieldCount; ++i)
			{
				// Decoration: e.g. NTamd64
				TCHAR szDecoration[50] = {0};
				fSuccess = ::SetupGetStringField(
					&infContext, i, 
					szDecoration, RTL_NUMBER_OF(szDecoration), 
					NULL);
				// Ignore if there is no decoration
				if (!fSuccess)
				{
					continue;
				}
				DBGPRT_INFO(_T("TargetOS: %s\n"), szDecoration);
				fSuccess = pProcessModelSection(
					hInfFile, szModel, szDecoration, mszHardwareIDs);
				if (fSuccess) 
				{
					return TRUE;
				}
			}
		}

		fSuccess = ::SetupFindNextLine(&infContext, &infContext);

	} while (fSuccess);

	return FALSE;
}

BOOL 
pCompareINF(
	LPCTSTR szInfFile, 
	LPCTSTR mszHardwareIDs)
{
	DBGPRT_INFO(_T("Inspecting INF %s for %s...\n"), szInfFile, mszHardwareIDs);

	HINF hInf = ::SetupOpenInfFile(
		szInfFile,
		NULL,
		INF_STYLE_WIN4,
		0);

	if (INVALID_HANDLE_VALUE == hInf) 
	{
		DBGPRT_ERR_EX(_T("SetupOpenInfFile failed: "));
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
	if (!fSuccess)
	{
		return FALSE;
	}

	WIN32_FIND_DATA ffData = {0};

	// prevent file open error dialog from the system
	UINT oldErrorMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);

	HANDLE hFindFile = ::FindFirstFile(szInfFindSpec, &ffData);
	if (INVALID_HANDLE_VALUE == hFindFile) 
	{
		::SetErrorMode(oldErrorMode);
		return FALSE;
	}

	do 
	{

		if (pCompareINF(ffData.cFileName, mszHardwareIDs)) 
		{
			BOOL fSuccess = Setupapi_SetupUninstallOEMInf(
				ffData.cFileName, 
				dwFlags, 
				NULL);

			if (pfnCallback) 
			{
				BOOL fCont = pfnCallback(
					fSuccess ? 0 : ::GetLastError(),
					szInfPath,
					ffData.cFileName,
					lpContext);
				if (!fCont) 
				{
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
