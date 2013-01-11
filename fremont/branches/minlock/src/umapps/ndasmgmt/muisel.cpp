#include "stdafx.h"
#include <tchar.h>
#include <xtl/xtlautores.h>
#include "muisel.h"

// This function works on all version of 32-bit Windows
// returns 0 when cannot determine the language
LANGID 
WINAPI
NuiGetCurrentUserUILanguage()
{
	WORD wLangId;

	typedef LANGID (*Kernel32_GetUserDefaultUILanguage)(VOID);
	Kernel32_GetUserDefaultUILanguage GetUserUILanguage;

	HINSTANCE hKernel = LoadLibrary(_T("kernel32.dll"));
	if (NULL != hKernel)
	{
		// GetProcAddress uses ANSI, not Unicode at this time
		GetUserUILanguage = (Kernel32_GetUserDefaultUILanguage) 
			GetProcAddress(hKernel, "GetUserDefaultUILanguage");
		if (NULL == GetUserUILanguage)
		{
			GetUserUILanguage = (Kernel32_GetUserDefaultUILanguage)
				GetProcAddress(hKernel, "GetUserDefaultLangID");
		}
		if (NULL != GetUserUILanguage)
		{
			wLangId = GetUserUILanguage();
		}
	}

	return wLangId;
}

// returns 0 if no language matches from primary supporting languages
LANGID 
WINAPI
NuiFindMatchingLanguage(
	IN LANGID UserLangID, 
	IN DWORD nSupportingLangIDs, 
	IN LANGID* SupportingLangIDs )
{
	WORD primaryLangID, subLangID;
	DWORD i, nMatch, nFirstMatch;

	// we don't have to consider if LangID == 0
	if (UserLangID == 0)
		return 0;

	primaryLangID = PRIMARYLANGID(UserLangID);
	subLangID = SUBLANGID(UserLangID);

	for (i = 0, nMatch = 0, nFirstMatch = -1; i < nSupportingLangIDs; i++)
	{
		// exact match, no further consideration
		if (SupportingLangIDs[i] == UserLangID)
			return SupportingLangIDs[i];
		if (PRIMARYLANGID(SupportingLangIDs[i]) ==  primaryLangID)
		{
			nMatch++;
			if (nFirstMatch == -1)
				nFirstMatch = i;
		}
	}

	// if there is no match, return 0
	if (nFirstMatch == -1)
		return 0;
	else
	// two possible cases, however, one stmt covers both
	// - if there is only one supporing language found, that is the best match!
	// - if there are more than one possible languages and there is no exact match,
	//   return the first match
		return SupportingLangIDs[nFirstMatch];
}

LANGID 
WINAPI
NuiFindMatchingLanguage(
	IN CONST LANGID LangID, 
	IN CONST SUPPORTINGLANGUAGES SupportingLangs)
{
	WORD primaryLangID, subLangID;
	DWORD i, nMatch, nFirstMatch;

	// we don't have to consider if LangID == 0
	if (LangID == 0)
		return 0;

	primaryLangID = PRIMARYLANGID(LangID);
	subLangID = SUBLANGID(LangID);

	for (i = 0, nMatch = 0, nFirstMatch = -1; i < SupportingLangs.nLangs; i++)
	{
		// exact match, no further consideration
		if (SupportingLangs.idLangs[i] == LangID)
			return SupportingLangs.idLangs[i];
		if (PRIMARYLANGID(SupportingLangs.idLangs[i]) ==  primaryLangID)
		{
			nMatch++;
			if (nFirstMatch == -1)
				nFirstMatch = i;
		}
	}

	// if there is no match, return 0
	if (nFirstMatch == -1)
		return 0;
	else
	// two possible cases, however, one stmt covers both
	// - if there is only one supporing language found, that is the best match!
	// - if there are more than one possible languages and there is no exact match,
	//   return the first match
		return SupportingLangs.idLangs[nFirstMatch];
}

//
// Resource DLL should contains the following information
// in Version Resource
//
// \\VarFileInfo\\ResLangID 0x0409,0x0000
//                         (LOWORD)(HIWORD)
// The lower word is a LANGID and the HIWORD is reserved
// and must be 0
//
// Arguments:
//
//	szFilename - Filename to be searched
//
// Return value:
//
//	If the function succeeds, the return value is the LANGID specified,
//	in the resource file.
//
//  If the function fails, the return value is zero. To get extended error
//  information, call GetLastError.
//
LANGID 
WINAPI
NuiGetResLangIDFromFile(LPCTSTR szFilename)
{
	static LPCTSTR VF_RESLANGID = _T("\\VarFileInfo\\ResLangID");

	DWORD dwHandle = 0;
	DWORD cbFileVer = ::GetFileVersionInfoSize(
		(LPTSTR)szFilename,
		&dwHandle);

	if (0 == cbFileVer) {
		return 0;
	}

	LPVOID lpFileVerData = ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbFileVer);

	if (NULL == lpFileVerData) {
		return 0;
	}

	XTL::AutoProcessHeap hHeap = lpFileVerData;

	BOOL fSuccess = ::GetFileVersionInfo(
		(LPTSTR)szFilename,
		0, 
		cbFileVer, 
		lpFileVerData);

	if (!fSuccess) {
		return 0;
	}

	UINT cbResLangID;
	LPDWORD lpdwResLangID;

	fSuccess = ::VerQueryValue(
		lpFileVerData,
		(LPTSTR) VF_RESLANGID,
		(LPVOID*)&lpdwResLangID,
		&cbResLangID);

	if (!fSuccess) {
		return 0;
	}

	WORD wLangID = (WORD)*lpdwResLangID;

	return wLangID;
}

//
// Returns the list of Resource DLLs containing 
// Version Information of ResLangID in the specified directory
// of files in specified pattern
//

DWORD_PTR 
WINAPI
NuiCreateAvailResourceDLLsInModuleDirectory(
	IN LPCTSTR szSubDirectory OPTIONAL, 
	IN LPCTSTR szFileSpec OPTIONAL,
	IN DWORD_PTR cbList,
	IN OUT PNUI_RESDLL_INFO pRdiList)
{
	//
	// First to find the available resources
	// from the application directory
	//
	BOOL fSuccess;
	HRESULT hr;
	TCHAR szModulePathBuffer[MAX_PATH];
	LPTSTR szModulePath = szModulePathBuffer;
	DWORD cchModulePath = ::GetModuleFileName(
		NULL, 
		szModulePath, 
		MAX_PATH);

#ifdef UNICODE
	XTL::AutoProcessHeap autoModulePath;
	if (cchModulePath > MAX_PATH) {

		//
		// handling very long path up to 32,767 chars
		//

		szModulePath = (LPTSTR) ::HeapAlloc(
			::GetProcessHeap(), 
			HEAP_ZERO_MEMORY,
			(cchModulePath + 1) * sizeof(TCHAR));

		if (NULL == szModulePath) {
			return 0;
		}

		autoModulePath = (LPVOID) szModulePath;

		cchModulePath = ::GetModuleFileName(
			NULL, 
			szModulePath,
			cchModulePath + 1);

		ATLASSERT(cchModulePath > 0);
	}
#endif

	size_t cchSubDirectory = 0;
	if (NULL != szSubDirectory) {
		hr = ::StringCchLength(szSubDirectory, MAX_PATH, &cchSubDirectory);
		ATLASSERT(SUCCEEDED(hr));
	}

	LPTSTR szModuleDirectory = (LPTSTR) ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		(cchModulePath + 1 + cchSubDirectory + 1) * sizeof(TCHAR));

	if (NULL == szModuleDirectory) {
		return 0;
	}

	XTL::AutoProcessHeap autoModuleDirectory = szModuleDirectory;

	::CopyMemory(szModuleDirectory, szModulePath, 
		(cchModulePath + 1) * sizeof(TCHAR));

	fSuccess = ::PathRemoveFileSpec(szModuleDirectory);
	ATLASSERT(fSuccess);

	if (NULL != szSubDirectory) {
		fSuccess = ::PathAppend(szModuleDirectory, szSubDirectory);
		ATLASSERT(fSuccess);
	}

	TCHAR szModuleName[MAX_PATH];
	LPCTSTR lpFilePart = ::PathFindFileName(szModulePath);
	hr = ::StringCchCopy(szModuleName, MAX_PATH, lpFilePart);
	ATLASSERT(SUCCEEDED(hr));
	::PathRemoveExtension(szModuleName);

	//
	// we are only searching ndasmgmt*
	//
	TCHAR szResFileSpec[MAX_PATH];

	if (NULL == szFileSpec) {
		hr = ::StringCchCopy(szResFileSpec, MAX_PATH, szModuleName);
		ATLASSERT(SUCCEEDED(hr));
		hr = ::StringCchCat(szResFileSpec, MAX_PATH, _T("*"));
		ATLASSERT(SUCCEEDED(hr));
		szFileSpec = szResFileSpec;
	}

	return ::NuiCreateAvailResourceDLLs(
		szModuleDirectory, 
		szFileSpec,
		cbList, 
		pRdiList);
}

DWORD_PTR 
WINAPI
NuiCreateAvailResourceDLLs(
	LPCTSTR szDirectory, 
	LPCTSTR szFindSpec,
	DWORD_PTR cbList,
	PNUI_RESDLL_INFO pRdiList)
{
#ifdef UNICODE
	const static size_t MAX_LONG_PATH = 32767;
#else
	const static size_t MAX_LONG_PATH = MAX_PATH;
#endif

	WIN32_FIND_DATA ffData;

	HANDLE hFind = INVALID_HANDLE_VALUE;
	BOOL fSuccess = FALSE;
	DWORD_PTR cbUsed = 0, cbAvail = cbList;

	PNUI_RESDLL_INFO pPrevRdi = NULL;
	PNUI_RESDLL_INFO pCurRdi = pRdiList;

	size_t cchDir;
	HRESULT hr = ::StringCchLength(szDirectory, MAX_LONG_PATH, &cchDir);
	if (!SUCCEEDED(hr)) {
		::SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}

	size_t cchFindSpec;
	hr = ::StringCchLength(szFindSpec, MAX_PATH, &cchFindSpec);
	if (!SUCCEEDED(hr)) {
		::SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}

	size_t cchBuffer = cchDir + 1 + MAX_PATH; // append "\\"
	LPVOID lpBuffer = ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		(cchBuffer + 1) * sizeof(TCHAR)); 
	// we need additional 1 chars to hold NULL

	if (NULL == lpBuffer) {
		::SetLastError(ERROR_OUTOFMEMORY);
		return 0;
	}

	XTL::AutoProcessHeap autoHeap = lpBuffer;

	LPTSTR szDirFindSpec = (LPTSTR) lpBuffer;

	hr = ::StringCchPrintf(
		szDirFindSpec,
		cchBuffer + 1,
		_T("%s\\%s"), 
		szDirectory, 
		szFindSpec);
	_ASSERTE(SUCCEEDED(hr));

	while (TRUE) {

		if (INVALID_HANDLE_VALUE == hFind) {

			hFind = ::FindFirstFile(szDirFindSpec, &ffData);
			if (INVALID_HANDLE_VALUE == hFind) {
				return 0;
			}

		} else {

			fSuccess = ::FindNextFile(hFind,&ffData);
			if (!fSuccess) {
				break;
			}

		}

		INT iLen = ::lstrlen(ffData.cFileName);

		if (iLen < 1) {
			continue;
		}

		// reusing the buffer
		LPTSTR szResFilePath = static_cast<LPTSTR>(lpBuffer);
		hr = ::StringCchPrintf(
			szResFilePath, 
			cchBuffer + 1,
			_T("%s\\%s"),
			szDirectory,
			ffData.cFileName);
		_ASSERTE(SUCCEEDED(hr));

		size_t cchResFilePath;
		hr = ::StringCchLength(szResFilePath, MAX_LONG_PATH, &cchResFilePath);
		_ASSERTE(SUCCEEDED(hr));

		size_t cbResFilePath = sizeof(TCHAR) * (cchResFilePath + 1);

		LANGID langId = ::NuiGetResLangIDFromFile(szResFilePath);
		if (0 == langId) {
			continue;
		}

		cbUsed += sizeof(NUI_RESDLL_INFO) + cbResFilePath;

		if (cbAvail < cbUsed) {
			continue;
		}

		if (NULL != pPrevRdi) {
			pPrevRdi->pNext = pCurRdi;
		}

		pCurRdi->wLangID = langId;
		pCurRdi->wReserved = 0;
		pCurRdi->lpszFilePath = (LPTSTR)(
			(LPBYTE)(pCurRdi) + sizeof(NUI_RESDLL_INFO));

		hr = ::StringCbCopy(pCurRdi->lpszFilePath, cbResFilePath, szResFilePath);
		_ASSERTE(SUCCEEDED(hr));

		pCurRdi->pNext = NULL;
		pPrevRdi = pCurRdi;
		pCurRdi = (PNUI_RESDLL_INFO)(
			(LPBYTE)(pCurRdi) + sizeof(NUI_RESDLL_INFO) + cbResFilePath);

	};

	fSuccess = ::FindClose(hFind);
	_ASSERTE(fSuccess);

	return cbUsed;
}

