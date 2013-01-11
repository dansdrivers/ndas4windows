#include "stdafx.h"
#include "winutil.h"
#include "resource.h"
#include "setupui.h"
#include "autores.h"
#include <msiquery.h>
#include <msidefs.h>
#include "msiproc.h"
#include <shellapi.h>

#include "xdebug.h"

//--------------------------------------------------------------------------------------
// ADVAPI32 API -- delay load
//--------------------------------------------------------------------------------------

#define ADVAPI32_DLL _T("advapi32.dll")

#define ADVAPI32API_CheckTokenMembership "CheckTokenMembership"
typedef BOOL (WINAPI* PFnCheckTokenMembership)(HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember);

#define ADVAPI32API_AdjustTokenPrivileges "AdjustTokenPrivileges"
typedef BOOL (WINAPI* PFnAdjustTokenPrivileges)(HANDLE TokenHandle, BOOL DisableAllPrivileges, PTOKEN_PRIVILEGES NewState, DWORD BufferLength, PTOKEN_PRIVILEGES PreviousState, PDWORD ReturnLength);

#define ADVAPI32API_OpenProcessToken "OpenProcessToken"
typedef BOOL (WINAPI* PFnOpenProcessToken)(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle);

#define ADVAPI32API_LookupPrivilegeValueA "LookupPrivilegeValueA"
typedef BOOL (WINAPI* PFnLookupPrivilegeValueA)(LPCSTR lpSystemName, LPCSTR lpName, PLUID lpLuid);

#define ADVAPI32API_LookupPrivilegeValueW "LookupPrivilegeValueW"
typedef BOOL (WINAPI* PFnLookupPrivilegeValueW)(LPCWSTR lpSystemName, LPCWSTR lpName, PLUID lpLuid);

#ifdef UNICODE
#define ADVAPI32API_LookupPrivilegeValue ADVAPI32API_LookupPrivilegeValueW
#define PFnLookupPrivilegeValue PFnLookupPrivilegeValueW
#else
#define ADVAPI32API_LookupPrivilegeValue ADVAPI32API_LookupPrivilegeValueA
#define PFnLookupPrivilegeValue PFnLookupPrivilegeValueA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Utility functions
//

static DWORD pWaitForProcess(HANDLE handle);
static DWORD pExecuteUpgradeMsi(LPTSTR szUpgradeMsi);
static BOOL pIsUpdateRequiredVersion(LPTSTR szFilename, ULONG ulMinVer);

static LPTSTR pGetMsiPackageCode(LPCTSTR szMsiFile);

/////////////////////////////////////////////////////////////////////////////
//
// Update command line options
//
//
/////////////////////////////////////////////////////////////////////////////
CONST TCHAR MSI_DELAY_REBOOT[] = _T("/c:\"msiinst.exe /delayreboot\""); 
//_T(" /norestart");
CONST TCHAR MSI_DELAY_REBOOT_QUIET[] = _T("/c:\"msiinst.exe /delayrebootq\"");
//_T(" /quiet /norestart");

/////////////////////////////////////////////////////////////////////////////
// MimimumWindowsPlatform
//
//  Returns TRUE if running on a platform whose major version, minor version
//  and service pack major are greater than or equal to the ones specifed
//  while making this function call
//
BOOL
MinimumWindowsPlatform(
	DWORD dwMajorVersion, 
	DWORD dwMinorVersion, 
	WORD wServicePackMajor)
{
	OSVERSIONINFOEXA osvi;
	DWORDLONG dwlConditionMask = 0;

	// Initialize the OSVERSIONINFOEX structure.
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
	osvi.dwMajorVersion = dwMajorVersion;
	osvi.dwMinorVersion = dwMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;
   
#define KERNEL32_DLL _T("kernel32.dll")

	AutoHModule hModule = ::LoadLibrary(KERNEL32_DLL);

	if (NULL == (HMODULE) hModule) {
		return FALSE;
	}

	typedef BOOL (WINAPI* PfnVerifyVersionInfoA)(
		LPOSVERSIONINFOEXA lpVersionInfo,
		DWORD dwTypeMask,
		DWORDLONG dwlConditionMask);

	typedef ULONGLONG (WINAPI* PfnVerSetConditionMask)(
		ULONGLONG dwlConditionMask,
		DWORD dwTypeBitMask,
		BYTE dwConditionMask);

	PfnVerifyVersionInfoA fnVerifyVersionInfoA = (PfnVerifyVersionInfoA) 
		GetProcAddress(hModule, "VerifyVersionInfoA");

	PfnVerSetConditionMask fnVerSetConditionMask = (PfnVerSetConditionMask)
		GetProcAddress(hModule, "VerSetConditionMask");

	if (NULL == fnVerifyVersionInfoA ||
		NULL == fnVerSetConditionMask)
	{
		return FALSE;
	}

#define VER_SET_CONDITION_DELAYED(_m_,_t_,_c_)  \
	((_m_)=fnVerSetConditionMask((_m_),(_t_),(_c_)))

	// Initialize the condition mask.
	VER_SET_CONDITION_DELAYED(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION_DELAYED(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION_DELAYED(dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
 
   // Perform the test.
   return fnVerifyVersionInfoA(
	   &osvi, 
       VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
       dwlConditionMask) ? TRUE : FALSE;
}

//
// VER_PLATFORM_WIN32s (Win32s on Windows 3.1)
// VER_PLATFORM_WIN32_WINDOWS (Windows 95, 98, Me)
// VER_PLATFORM_WIN32_NT (Windows NT, 2000, XP, 2003, ...)
//
BOOL 
IsPlatform(DWORD dwPlatformId)
{
	OSVERSIONINFO osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	return (osvi.dwPlatformId == dwPlatformId);
}


BOOL
IsMsiOSSupported(DWORD dwMSIVersion)
{
	//	OSVERSIONINFO sInfoOS = {0};
	//    sInfoOS.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	//    ::GetVersionEx(&sInfoOS);

	// We do no support any platform prior to Windows 2000
	//    if (5 > sInfoOS.dwMajorVersion)
	//      return FALSE;

	if (120 >= dwMSIVersion || 110 >= dwMSIVersion || 100 >= dwMSIVersion) {
		if (MinimumWindowsPlatform(4, 0, 3) ||
			IsPlatform(VER_PLATFORM_WIN32_WINDOWS))
		{
			return TRUE;
		}
	} else if (200 >= dwMSIVersion) {
		// Windows 95, 98, NT4 SP6, 2000, Me
		if (MinimumWindowsPlatform(5, 0, 0) || // Windows 2000 and above
			MinimumWindowsPlatform(4, 0, 6) || // Windows NT 4.0 SP6 and above
			IsPlatform(VER_PLATFORM_WIN32_WINDOWS)) // Windows 95, 98, Me
		{
			return TRUE;
		}
	} else if (300 >= dwMSIVersion) {
		// We support:
		if (MinimumWindowsPlatform(5, 2, 0) ||   // Windows 2003 and above
			MinimumWindowsPlatform(5, 1, 0) ||   // Windows XP and above
			MinimumWindowsPlatform(5, 0, 3))     // Windows 2000 SP3 and above
		{
			return TRUE;
		}
	} 
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// IsAdmin
//
//  Returns TRUE if current user is an administrator (or if on Win9X)
//  Returns FALSE if current user is not an adminstrator
//
//  implemented as per KB Q118626
//

BOOL IsAdmin()
{
	// get the administrator sid
	PSID psidAdministrators;
	SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
	if(!AllocateAndInitializeSid(
		&siaNtAuthority, 
		2, 
		SECURITY_BUILTIN_DOMAIN_RID, 
		DOMAIN_ALIAS_RID_ADMINS, 
		0, 0, 0, 0, 0, 0, &psidAdministrators))
		return FALSE;

	// on NT5, use the CheckTokenMembershipAPI to correctly handle cases where
	// the Adiminstrators group might be disabled. bIsAdmin is BOOL for 
	BOOL bIsAdmin = FALSE;
	// CheckTokenMembership checks if the SID is enabled in the token. NULL for
	// the token means the token of the current thread. Disabled groups, restricted
	// SIDS, and SE_GROUP_USE_FOR_DENY_ONLY are all considered. If the function
	// returns FALSE, ignore the result.

	HMODULE hAdvapi32 = LoadLibrary(ADVAPI32_DLL);
	if (!hAdvapi32)
		bIsAdmin = FALSE;
	else
	{
		PFnCheckTokenMembership pfnCheckTokenMembership = (PFnCheckTokenMembership)GetProcAddress(hAdvapi32, ADVAPI32API_CheckTokenMembership);
		if (!pfnCheckTokenMembership || !pfnCheckTokenMembership(NULL, psidAdministrators, &bIsAdmin))
			bIsAdmin = FALSE;
	}
	FreeLibrary(hAdvapi32);
	hAdvapi32 = 0;

	::FreeSid(psidAdministrators);
	return bIsAdmin ? TRUE : FALSE;

}

DWORD 
GetFileVersionNumber(
	LPCTSTR szFilename, 
	DWORD * pdwMSVer, 
	DWORD * pdwLSVer)
{
	DWORD             dwResult = NOERROR;
	unsigned          uiSize;
	DWORD             dwVerInfoSize;
	DWORD             dwHandle;
	BYTE              *prgbVersionInfo = NULL;
	VS_FIXEDFILEINFO  *lpVSFixedFileInfo = NULL;

	DWORD dwMSVer = 0xffffffff;
	DWORD dwLSVer = 0xffffffff;

	dwVerInfoSize = ::GetFileVersionInfoSize(szFilename, &dwHandle);
	if (0 != dwVerInfoSize)
	{
		prgbVersionInfo = (LPBYTE) ::GlobalAlloc(GPTR, dwVerInfoSize);
		if (NULL == prgbVersionInfo)
		{
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			goto Finish;
		}

		// Read version stamping info
		if (::GetFileVersionInfo(szFilename, dwHandle, dwVerInfoSize, prgbVersionInfo))
		{
			// get the value for Translation
			if (::VerQueryValue(prgbVersionInfo, _T("\\"), (LPVOID*)&lpVSFixedFileInfo, &uiSize) && (uiSize != 0))
			{
				dwMSVer = lpVSFixedFileInfo->dwFileVersionMS;
				dwLSVer = lpVSFixedFileInfo->dwFileVersionLS;
			}
		}
		else
		{
			dwResult = GetLastError();
			goto Finish;
		}
	}
	else
	{
		dwResult = GetLastError();
	}

#ifdef DEBUG
	TCHAR szVersion[255];
	StringCchPrintf(szVersion, sizeof(szVersion), _T("%s is version %d.%d.%d.%d\n"), szFilename, HIWORD(dwMSVer), LOWORD(dwMSVer), HIWORD(dwLSVer), LOWORD(dwLSVer));
//	DebugMsg("[INFO] %s", szVersion);
#endif // DEBUG

Finish:
	if (NULL != prgbVersionInfo)
		::GlobalFree(prgbVersionInfo);
	if (pdwMSVer)
		*pdwMSVer = dwMSVer;
	if (pdwLSVer)
		*pdwLSVer = dwLSVer;

	return dwResult;
}


BOOL 
IsMsiUpgradeNecessary(ULONG ulReqMsiMinVer)
{
	// attempt to load msi.dll in the system directory

	HRESULT hr;

	TCHAR szSysMsiDll[MAX_PATH] = {0};
	TCHAR szSystemFolder[MAX_PATH] = {0};

	DWORD dwRet = ::GetSystemDirectory(szSystemFolder, MAX_PATH);
	if (0 == dwRet || MAX_PATH < dwRet)
	{
		// failure or buffer too small; assume upgrade is necessary
		DBGPRT_INFO(_FT("Can't obtain system directory; assuming upgrade is necessary"));
		return TRUE;
	}

	hr = ::StringCchCopy(
		szSysMsiDll, 
		sizeof(szSysMsiDll)/sizeof(szSysMsiDll[0]),
		szSystemFolder);

	if (FAILED(hr)) {
		// failure to get path to msi.dll; assume upgrade is necessary
		DBGPRT_INFO(_FT("Can't obtain msi.dll path; assuming upgrade is necessary\n"));
		return TRUE;
	}

	hr = ::StringCchCat(
		szSysMsiDll, 
		sizeof(szSysMsiDll)/sizeof(szSysMsiDll[0]), 
		_T("\\MSI.DLL"));

	if (FAILED(hr)) {
		// failure to get path to msi.dll; assume upgrade is necessary
		DBGPRT_INFO(_FT("Can get path to msi.dll; assuming upgrade is necessary\n"));
		return TRUE;
	}

	HINSTANCE hinstMsiSys = ::LoadLibrary(szSysMsiDll);
	if (NULL == hinstMsiSys)
	{
		// can't load msi.dll; assume upgrade is necessary
		DBGPRT_INFO(_FT("Can't load msi.dll; assuming upgrade is necessary\n"));
		return TRUE;
	}
	::FreeLibrary(hinstMsiSys);

	// get version on msi.dll
	DWORD dwInstalledMSVer;
	dwRet = GetFileVersionNumber(szSysMsiDll, &dwInstalledMSVer, NULL);
	if (ERROR_SUCCESS != dwRet)
	{
		// can't obtain version information; assume upgrade is necessary
		DBGPRT_INFO(_FT("Can't obtain version information; assuming upgrade is necessary\n"));
		return TRUE;
	}

	// compare version in system to the required minimum
	ULONG ulInstalledVer = HIWORD(dwInstalledMSVer) * 100 + LOWORD(dwInstalledMSVer);
	if (ulInstalledVer < ulReqMsiMinVer)
	{
		// upgrade is necessary
		DBGPRT_INFO(_FT("Windows Installer upgrade is required.  System Version = %d, Minimum Version = %d.\n"), ulInstalledVer, ulReqMsiMinVer);
		return TRUE;
	}

	// no upgrade is necessary
	DBGPRT_INFO(_FT("No upgrade is necessary. System version meets minimum requirements\n"));
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// UpgradeMsi
//

UINT ValidateUpdate(
	ISetupUI* pSetupUI,
	LPTSTR szUpdatePath, 
	LPCTSTR szModuleFile, 
	ULONG ulMinVer)
{
    UINT uiRet = ERROR_SUCCESS;

    TCHAR szShortPath[MAX_PATH] = {0};

    // ensure Update is right version for Windows Installer upgrade
    if (!pIsUpdateRequiredVersion(szUpdatePath, ulMinVer))
    {
		// Update won't get us the right upgrade

		uiRet = ERROR_INVALID_PARAMETER;
		CString str; str.FormatMessage(IDS_INCORRECT_UPDATE_FMT, szUpdatePath);
		pSetupUI->PostErrorMessageBox(uiRet, str);

        return uiRet;
    }

	pSetupUI->SetProgressBar(40);

    // upgrade msi
    uiRet = pExecuteUpgradeMsi(szUpdatePath);

	pSetupUI->SetProgressBar(90);

    switch (uiRet)
    {
    case ERROR_SUCCESS:
    case ERROR_SUCCESS_REBOOT_REQUIRED:    
        {
            // nothing required at this time
            break;
        }
    case ERROR_FILE_NOT_FOUND:
        {
            // Update executable not found
			CString str; str.FormatMessage(IDS_NOUPDATE_FMT, szUpdatePath);
			pSetupUI->PostErrorMessageBox(uiRet, str);
            break;
        }
    default: // failure
        {
			// report error
			CString str; str.LoadString(IDS_FAILED_TO_UPGRADE_MSI);
			pSetupUI->PostErrorMessageBox(uiRet, str);
            break;
        }
    }
    return uiRet;
}

UINT 
UpgradeMsi(
	ISetupUI* pSetupUI,
	LPCTSTR szBase, 
	LPCTSTR szUpdate, 
	ULONG ulMinVer)
{
	TCHAR *szTempPath    = 0;
	TCHAR *szUpdatePath = 0;
	TCHAR *szFilePart    = 0;

	DWORD cchTempPath    = 0;
	DWORD cchUpdatePath = 0;
	DWORD cchReturn      = 0;
	DWORD dwLastError    = 0;
	DWORD dwFileAttrib   = 0;
	UINT  uiRet          = 0;

	HRESULT hr           = S_OK;

	// generate the path to the MSI update file =  szBase + szUpdate
	//   note: szUpdate is a relative path

	cchTempPath = lstrlen(szBase) + lstrlen(szUpdate) + 2; // 1 for null terminator, 1 for back slash
	szTempPath = new TCHAR[cchTempPath];
	if (!szTempPath)
	{
		uiRet = ERROR_OUTOFMEMORY;

		pSetupUI->PostErrorMessageBox(
			uiRet, 
			IDS_ERR_OUTOFMEM, 
			MB_OK | MB_ICONERROR);

		goto CleanUp;
	}
	::ZeroMemory(szTempPath, cchTempPath*sizeof(TCHAR));

	// find 'setup.exe' in the path so we can remove it -- this is an already expanded path, that represents
	//  our current running location.  It includes our executable name -- we want to find that and get rid of it
	if (0 == GetFullPathName(szBase, cchTempPath, szTempPath, &szFilePart))
	{
		uiRet = GetLastError();

		CString str; 
		str.FormatMessage(IDS_INVALID_PATH_FMT, szTempPath);
		pSetupUI->PostErrorMessageBox(uiRet, str, MB_OK | MB_ICONERROR);

		goto CleanUp;
	}
	if (szFilePart) {
		*szFilePart = '\0';
	}

	hr = StringCchCat(szTempPath, cchTempPath, szUpdate);
	if (FAILED(hr))
	{
		uiRet = HRESULT_CODE(hr);

		CString str; 
		str.FormatMessage(IDS_INVALID_PATH_FMT, szTempPath);
		pSetupUI->PostErrorMessageBox(uiRet, str, MB_OK | MB_ICONERROR);

		goto CleanUp;
	}

	cchUpdatePath = 2*cchTempPath;
	szUpdatePath = new TCHAR[cchUpdatePath];
	if (!szUpdatePath)
	{
		uiRet = ERROR_OUTOFMEMORY;

		pSetupUI->PostErrorMessageBox(
			uiRet, 
			IDS_ERR_OUTOFMEM, 
			MB_OK | MB_ICONERROR);

		goto CleanUp;
	}

	// normalize the path
	cchReturn = GetFullPathName(
		szTempPath, 
		cchUpdatePath, 
		szUpdatePath, 
		&szFilePart);

	if (cchReturn > cchUpdatePath)
	{
		// try again, with larger buffer
		delete [] szUpdatePath;
		cchUpdatePath = cchReturn;
		szUpdatePath = new TCHAR[cchUpdatePath];
		if (!szUpdatePath)
		{
			uiRet = ERROR_OUTOFMEMORY;
			pSetupUI->PostErrorMessageBox(
				uiRet, 
				IDS_ERR_OUTOFMEM, 
				MB_OK | MB_ICONERROR);

			goto CleanUp;
		}
		cchReturn = GetFullPathName(
			szTempPath, 
			cchUpdatePath, 
			szUpdatePath, 
			&szFilePart);
	}

	if (0 == cchReturn)
	{
		uiRet = GetLastError();

		CString str; 
		str.FormatMessage(IDS_INVALID_PATH_FMT, szTempPath);
		pSetupUI->PostErrorMessageBox(uiRet, str, MB_OK | MB_ICONERROR);

		goto CleanUp;
	}

	// no download is necessary -- but we can check for the file's existence
	dwFileAttrib = GetFileAttributes(szUpdatePath);
	if (0xFFFFFFFF == dwFileAttrib)
	{
		uiRet = ERROR_FILE_NOT_FOUND;

		// Update executable is missing
		CString str; 
		str.FormatMessage(IDS_NOUPDATE_FMT, szUpdatePath);
		pSetupUI->PostErrorMessageBox(uiRet, str, MB_OK | MB_ICONERROR);
		goto CleanUp;
	}

	pSetupUI->SetProgressBar(20);

	uiRet = ValidateUpdate(
		pSetupUI,
		szUpdatePath, 
		szUpdatePath, 
		ulMinVer);

	pSetupUI->SetProgressBar(100);

CleanUp:
	if (szTempPath)
		delete [] szTempPath;
	if (szUpdatePath)
		delete [] szUpdatePath;

	return uiRet;
}


/////////////////////////////////////////////////////////////////////////////
// WaitForProcess
//

DWORD 
WaitForHandle(HANDLE handle, DWORD dwTimeout)
{
/*    DWORD dwWaitResult = NOERROR;

	dwWaitResult = WaitForSingleObject(handle, INFINITE);
	if (WAIT_OBJECT_0 != dwWaitResult) {
		return ::GetLastError();
	}
	return ERROR_SUCCESS;
*/

	MSG msg = {0};
    
    //loop forever to wait
	while (TRUE) {

		//wait for object
		DWORD dwWaitResult = ::MsgWaitForMultipleObjects(
			1, 
			&handle, 
			FALSE, 
			dwTimeout, 
			QS_ALLINPUT);

		switch (dwWaitResult) {
        //success!
        case WAIT_OBJECT_0:
			return ERROR_SUCCESS;
        //not the process that we're waiting for
        case (WAIT_OBJECT_0 + 1):
			if (::PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
			break; // break the switch
        //did not return an OK; return error status
        default:
			return ::GetLastError();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// IsUpdateRequiredVersion
//
//  update package version is stamped as rmj.rmm.rup.rin
//

static 
BOOL 
pIsUpdateRequiredVersion(
	LPTSTR szFilename, 
	ULONG ulMinVer)
{
    // get version of update package
    DWORD dwUpdateMSVer;
    DWORD dwRet = GetFileVersionNumber(szFilename, &dwUpdateMSVer, NULL);
    if (ERROR_SUCCESS != dwRet)
    {
        // can't obtain version information; assume not proper version
        DBGPRT_INFO(_FT("Can't obtain version information for update package;")
			_T("assuming it is not the proper version\n"));
        return FALSE;
    }

    // compare version at source to required minimum
    ULONG ulSourceVer = HIWORD(dwUpdateMSVer) * 100 + LOWORD(dwUpdateMSVer);
    if (ulSourceVer < ulMinVer)
    {
        // source version won't get us to our minimum version
        DBGPRT_INFO(_FT("The update package is improper version for upgrade.")
			_T("Update package Version = %d, Minimum Version = %d.\n"), 
			ulSourceVer, ulMinVer);
        return FALSE;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// ExecuteUpgradeMsi
//

static
DWORD 
pExecuteUpgradeMsi(LPTSTR szUpgradeMsi)
{
    DBGPRT_INFO(_FT("Running update package from --> %s\n"), szUpgradeMsi);

    DWORD dwResult = 0;

    // build up CreateProcess structures
	STARTUPINFO          sui = {0};
	PROCESS_INFORMATION  pi = {0};

    sui.cb          = sizeof(STARTUPINFO);
    sui.dwFlags     = STARTF_USESHOWWINDOW;
    sui.wShowWindow = SW_SHOW;

    //
    // build command line and specify delayreboot option to Update
    //  three acounts for terminating null plus quotes for module
	//
    DWORD cchCommandLine = lstrlen(szUpgradeMsi) + lstrlen(MSI_DELAY_REBOOT_QUIET) + 3;
    TCHAR *szCommandLine = new TCHAR[cchCommandLine];

	if (!szCommandLine) {
        return ERROR_OUTOFMEMORY;
	}
    
    if (FAILED(StringCchCopy(szCommandLine, cchCommandLine, _T("\"")))
        || FAILED(StringCchCat(szCommandLine, cchCommandLine, szUpgradeMsi))
        || FAILED(StringCchCat(szCommandLine, cchCommandLine, _T("\"")))
        || FAILED(StringCchCat(szCommandLine, cchCommandLine, MSI_DELAY_REBOOT_QUIET)))
    {
        delete [] szCommandLine;
        return ERROR_INSTALL_FAILURE;
    }

    //
    // run update process
	//
	BOOL fSuccess = ::CreateProcess(
		NULL, 
		szCommandLine, 
		NULL, 
		NULL, 
		FALSE, 
		CREATE_DEFAULT_ERROR_MODE, 
		NULL, 
		NULL, 
		&sui, 
		&pi);

    if(!fSuccess)
    {
        // failed to launch.
		dwResult = ::GetLastError();
        delete [] szCommandLine;
        return dwResult;
    }

    dwResult = WaitForHandle(pi.hProcess);
    if(ERROR_SUCCESS != dwResult)
    {
        delete [] szCommandLine;
        return dwResult;
    }

    DWORD dwExitCode = 0;
    ::GetExitCodeProcess(pi.hProcess, &dwExitCode);

    ::CloseHandle(pi.hProcess);

    delete [] szCommandLine;

    return dwExitCode;
}

//
// Copy msi files to the system or user's temporal location:
//
// C:\Windows\Downloaded Installations\{PackageCode}\database.msi
// or 
// C:\Documents and Settings\{username}\Local Settings\Application Data\
// Downloaded Installations\{Package Code}\database.msi
// 

static CONST TCHAR MSICACHE_DIRECTORY[] = _T("Downloaded Installations");

UINT
pCacheMsiToUser(LPCTSTR szMsiFile, LPTSTR szCachedMsiFie, DWORD_PTR cchMax)
{
	return ERROR_SUCCESS;
}

UINT
pCacheMsiToSystem(
	ISetupUI* pSetupUI, 
	LPCTSTR szMsiFile, 
	LPTSTR szCachedMsiFile,
	DWORD cchMax)
{
	TCHAR szMsiFileFullPath[MAX_PATH] = {0};
	LPTSTR lpFilePart = NULL;
	
	DWORD nChars = ::GetFullPathName(
		szMsiFile, 
		MAX_PATH, 
		szMsiFileFullPath, 
		&lpFilePart);
	
	if (0 == nChars) {
		DWORD dwErr = GetLastError();
		DBGPRT_ERR_EX(_FT("GetFullPathName %s failed: "), szMsiFile);
		return dwErr;
	}

	TCHAR szCacheFile[MAX_PATH] = {0};
	// nChars = GetSystemWindowsDirectory(szCacheFile, MAX_PATH);
	nChars = GetWindowsDirectory(szCacheFile, MAX_PATH);
	if (0 == nChars) {
		DWORD dwErr = GetLastError();
		DBGPRT_ERR_EX(_FT("GetSystemWindowsDirectory failed: "));
		return dwErr;
	}

	HRESULT hr = StringCchCat(szCacheFile, MAX_PATH, _T("\\"));
	if (FAILED(hr)) {
		DBGPRT_ERR_EX(_FT("StringCchCat failed: "));
		return HRESULT_CODE(hr);
	}

	hr = StringCchCat(szCacheFile, MAX_PATH, MSICACHE_DIRECTORY);
	if (FAILED(hr)) {
		DBGPRT_ERR_EX(_FT("StringCchCat failed: "));
		return HRESULT_CODE(hr);
	}

	LPTSTR szPackageCode = pGetMsiPackageCode(szMsiFile);

	if (NULL == szPackageCode) {
		DWORD dwErr = GetLastError();
		return dwErr;
	}

	if (szPackageCode[0] != _T('\0')) {

		hr = StringCchCat(szCacheFile, MAX_PATH, _T("\\"));
		if (FAILED(hr)) {
			DBGPRT_ERR_EX(_FT("StringCchCat failed: "));
			GlobalFree((HGLOBAL)szPackageCode);
			return HRESULT_CODE(hr);
		}

		hr = StringCchCat(szCacheFile, MAX_PATH, szPackageCode);
		if (FAILED(hr)) {
			DBGPRT_ERR_EX(_FT("StringCchCat failed: "));
			GlobalFree((HGLOBAL)szPackageCode);
			return HRESULT_CODE(hr);
		}

		GlobalFree((HGLOBAL)szPackageCode);
	}

	hr = StringCchCat(szCacheFile, MAX_PATH, _T("\\"));
	if (FAILED(hr)) {
		DBGPRT_ERR_EX(_FT("StringCchCat failed: "));
		return HRESULT_CODE(hr);
	}

	hr = StringCchCat(szCacheFile, MAX_PATH, lpFilePart);
	if (FAILED(hr)) {
		DBGPRT_ERR_EX(_FT("StringCchCat failed: "));
		return HRESULT_CODE(hr);
	}

	SHFILEOPSTRUCT shop;
	shop.hwnd = pSetupUI->GetCurrentWindow();
	shop.wFunc = FO_COPY;
	shop.pFrom = szMsiFileFullPath; // doubly NULL-terminated!!
	shop.pTo = szCacheFile;
	shop.fFlags = FOF_FILESONLY | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION |
		FOF_NOCOPYSECURITYATTRIBS |
		FOF_SILENT;

	INT iRet = SHFileOperation(&shop);

	if (ERROR_SUCCESS != iRet) {
		DWORD dwErr = GetLastError();
		CString str;
		str.FormatMessage(IDS_ERR_CACHING_TO_SYSTEM_FMT,szCacheFile);
		pSetupUI->PostErrorMessageBox(GetLastError(),str);
		return dwErr;
	}

	hr = StringCchCopy(szCachedMsiFile, cchMax, szCacheFile);
	ATLASSERT(SUCCEEDED(hr));
	return ERROR_SUCCESS;
}

PMSIAPI _pMsiApi = NULL;

//
// Caller should free the resource for non-NULL return values
// with GlobalFree
//
static
LPTSTR
pGetMsiPackageCode(LPCTSTR szMsiFile)
{
	MSIHANDLE hSummaryInfo = 0;

	CMsiApi msiApi;
	BOOL fSuccess = msiApi.Initialize();
	if (!fSuccess) {
		DBGPRT_ERR(_FT("MSIAPI functions are not available.\n"));
		SetLastError(ERROR_PROC_NOT_FOUND);
		return NULL;
	}

	UINT uiRet = msiApi.GetSummaryInformation(0, szMsiFile, 0, &hSummaryInfo);
	if (ERROR_SUCCESS != uiRet) {
		SetLastError(uiRet);
		DBGPRT_ERR_EX(_FT("MsiGetSummaryInformation failed: "));
		return NULL;
	}

	UINT uiDataType;
	INT iValue;
	FILETIME ftValue;
	DWORD cchMax = 0;

	//
	// Get Package Code (PID_REVNUMBER)
	//
	uiRet = msiApi.SummmaryInfoGetProperty(
		hSummaryInfo, 
		PID_REVNUMBER,
		&uiDataType,
		&iValue, 
		&ftValue,
		_T(""), // do not pass NULL
		&cchMax);

	ATLASSERT(VT_LPSTR == uiDataType);

	if (ERROR_SUCCESS == uiRet) {
		ATLASSERT(FALSE);
		return _T("");
	} else if (ERROR_MORE_DATA != uiRet) {
		SetLastError(uiRet);
		DBGPRT_ERR_EX(_FT("MsiSummaryInfoGetProperty PID_REVNUMBER failed: "));
		return NULL;
	}

	++cchMax; // Add NULL
	LPTSTR szPackageCode = (LPTSTR)
		::GlobalAlloc(GPTR, (cchMax) * sizeof(TCHAR));

	if (NULL == szPackageCode) {
		DBGPRT_ERR_EX(_FT("GlobalAlloc %d chars failed: "), cchMax );
		return NULL;
	}

	uiRet = msiApi.SummmaryInfoGetProperty(
		hSummaryInfo,
		PID_REVNUMBER,
		&uiDataType,
		&iValue,
		&ftValue,
		szPackageCode,
		&cchMax);

	ATLASSERT(VT_LPSTR == uiDataType);

	if (ERROR_SUCCESS != uiRet) {
		DBGPRT_ERR_EX(_FT("SummmaryInfoGetProperty PID_REVNUMBER failed for %d chars: "), cchMax);
		::GlobalFree((HGLOBAL)szPackageCode);
		return NULL;
	}

	return szPackageCode;
}

UINT
CacheMsi(ISetupUI* pSetupUI, LPCTSTR szMsiFile, LPTSTR szCachedMsiFile, DWORD_PTR cchMax)
{
	return pCacheMsiToSystem(pSetupUI, szMsiFile, szCachedMsiFile, cchMax);
	//UINT uiRet = ERROR_SUCCESS;
	//if (IsAdmin()) {
	//	uiRet = 
	//}
//	if (ERROR_SUCCESS != uiRet) {
//		uiRet = pCacheMsiToUser(szMsiFile, szCachedMsiFile, cchMax);
//	}
	//return ERROR_SUCCESS;
}


