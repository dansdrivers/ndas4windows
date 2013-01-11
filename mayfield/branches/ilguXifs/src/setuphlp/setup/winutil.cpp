#include "stdafx.h"
#include <shellapi.h>
#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>
#include <msiquery.h>
#include <msidefs.h>
#include "winutil.h"
#include "resource.h"
#define DLOAD_USE_SEH
#include "dload_kernel32.h"
#include "dload_advapi32.h"
#include "dload_dllver.h"


namespace
{
const LPCTSTR ADVAPI32_DLL = _T("advapi32.dll");
const LPCTSTR KERNEL32_DLL = _T("kernel32.dll");

//// Update command line options
//const TCHAR MSI_DELAY_REBOOT[] = _T("/c:\"msiinst.exe /delayreboot\""); 
////_T(" /norestart");
//const TCHAR MSI_DELAY_REBOOT_QUIET[] = _T("/c:\"msiinst.exe /delayrebootq\"");
////_T(" /quiet /norestart");

const LPCTSTR MSI_DELAY_REBOOT = _T("/c:\"msiinst.exe /delayreboot\""); 
const LPCTSTR MSI_DELAY_REBOOT_QUIET = _T("/c:\"msiinst.exe /delayrebootq\"");

BOOL
GetMsiDllPath(
	LPTSTR lpBuffer,
	DWORD cchBuffer)
{
	DWORD dwRet = GetSystemDirectory(lpBuffer, cchBuffer);
	if (0 == dwRet || MAX_PATH < dwRet)
	{
		return FALSE;
	}
	LPTSTR lpNext = PathAddBackslash(lpBuffer);
	if (NULL == lpNext)
	{
		return FALSE;
	}
	HRESULT hr = StringCchCat(lpBuffer, cchBuffer, _T("MSI.DLL"));
	if (FAILED(hr))
	{
		return FALSE;
	}
	return TRUE;
}

}

//////////////////////////////////////////////////////////////////////////
//
// Utility functions
//

LPTSTR
GetErrorMessage(
	DWORD ErrorCode,
	DWORD LanguageId)
{
	DWORD flags = 
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM;

	LPVOID lpSource = NULL;
	LPTSTR lpBuffer = NULL;

	DWORD nChars = FormatMessage(
		flags,
		lpSource,
		ErrorCode,
		LanguageId,
		(LPTSTR)&lpBuffer,
		0,
		NULL);

	if (0 == nChars)
	{
		if (GetLastError() == ERROR_RESOURCE_LANG_NOT_FOUND)
		{
			nChars = FormatMessage(
				flags,
				lpSource,
				ErrorCode,
				0,
				(LPTSTR)&lpBuffer,
				0,
				NULL);
			if (0 == nChars)
			{
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
	}

	return lpBuffer;
}

//static DWORD pWaitForProcess(HANDLE handle);
//static DWORD pExecuteUpgradeMsi(LPTSTR szUpgradeMsi);
//static BOOL pIsUpdateRequiredVersion(LPTSTR szFilename, ULONG ulMinVer);

//static LPTSTR pGetMsiPackageCode(LPCTSTR szMsiFile);


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
	XTL::AutoModuleHandle hModule = LoadLibrary(KERNEL32_DLL);

	if (hModule.IsInvalid()) 
	{
		ATLTRACE("kernel32.dll not available, assumed platform does not meet the requirement\n");
		return FALSE;
	}

	Kernel32Dll kernel32(hModule);
	
	if (!(kernel32.IsProcAvailable("VerSetConditionMask") &&
		  kernel32.IsProcAvailable("VerifyVersionInfoA")))
	{
		ATLTRACE("Proc not available, assumed platform does not meet the requirement\n");
		return FALSE;
	}
			
	// Initialize the condition mask.
	DWORDLONG dwlConditionMask = 0;
	dwlConditionMask = kernel32.VerSetConditionMask(
		dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	dwlConditionMask = kernel32.VerSetConditionMask(
		dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
	dwlConditionMask = kernel32.VerSetConditionMask(
		dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
	
	// Initialize the OSVERSIONINFOEX structure.
	OSVERSIONINFOEXA osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
	osvi.dwMajorVersion = dwMajorVersion;
	osvi.dwMinorVersion = dwMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;
	
	// Perform the test.
	return kernel32.VerifyVersionInfoA(
		&osvi,
		VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
		dwlConditionMask);
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
	//    GetVersionEx(&sInfoOS);

	// We do no support any platform prior to Windows 2000
	//    if (5 > sInfoOS.dwMajorVersion)
	//      return FALSE;

	if (300 >= dwMSIVersion) 
	{
		// We support:
		if (MinimumWindowsPlatform(5, 2, 0) ||   // Windows 2003 and above
			MinimumWindowsPlatform(5, 1, 0) ||   // Windows XP and above
			MinimumWindowsPlatform(5, 0, 3))     // Windows 2000 SP3 and above
		{
			return TRUE;
		}
	} 
	else if (200 >= dwMSIVersion) 
	{
		// Windows 95, 98, NT4 SP6, 2000, Me
		if (MinimumWindowsPlatform(5, 0, 0) || // Windows 2000 and above
			MinimumWindowsPlatform(4, 0, 6) || // Windows NT 4.0 SP6 and above
			IsPlatform(VER_PLATFORM_WIN32_WINDOWS)) // Windows 95, 98, Me
		{
			return TRUE;
		}
	} 
	else if (120 >= dwMSIVersion || 
			 110 >= dwMSIVersion ||
			 100 >= dwMSIVersion) 
	{
		if (MinimumWindowsPlatform(4, 0, 3) ||
			IsPlatform(VER_PLATFORM_WIN32_WINDOWS))
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
//  Returns FALSE if current user is not an administrator
//
//  implemented as per KB Q118626
//

BOOL IsAdmin()
{
	XTL::AutoModuleHandle hAdvapi32 = LoadLibrary(ADVAPI32_DLL);
	if (hAdvapi32.IsInvalid())
	{
		return FALSE;
	}

	Advapi32Dll advapi32(hAdvapi32);
	if (!advapi32.IsProcAvailable("AllocateAndInitializeSid"))
	{
		// If Sid related functions are not available we assume Windows 9x
		return TRUE;
	}

	//
	// get the administrator sid
	PSID psidAdministrators = NULL;
	SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
	if(!advapi32.AllocateAndInitializeSid(
		&siaNtAuthority, 
		2, 
		SECURITY_BUILTIN_DOMAIN_RID, 
		DOMAIN_ALIAS_RID_ADMINS, 
		0, 0, 0, 0, 0, 0, &psidAdministrators))
	{
		return FALSE;
	}

	//
	// on NT5, use the CheckTokenMembershipAPI to correctly handle cases where
	// the Adiministrators group might be disabled. bIsAdmin is BOOL for 
	//
	BOOL bIsAdmin = FALSE;

	//
	// CheckTokenMembership checks if the SID is enabled in the token. NULL for
	// the token means the token of the current thread. Disabled groups, restricted
	// SIDS, and SE_GROUP_USE_FOR_DENY_ONLY are all considered. If the function
	// returns FALSE, ignore the result.
	//

	if (!advapi32.IsProcAvailable("CheckTokenMembership") ||
		!advapi32.CheckTokenMembership(NULL, psidAdministrators, &bIsAdmin))
	{
		bIsAdmin = FALSE;
	}

	FreeSid(psidAdministrators);
	return bIsAdmin;
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

	dwVerInfoSize = GetFileVersionInfoSize(szFilename, &dwHandle);
	if (0 != dwVerInfoSize)
	{
		prgbVersionInfo = (LPBYTE) GlobalAlloc(GPTR, dwVerInfoSize);
		if (NULL == prgbVersionInfo)
		{
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			goto Finish;
		}

		// Read version stamping info
		if (GetFileVersionInfo(szFilename, dwHandle, dwVerInfoSize, prgbVersionInfo))
		{
			// get the value for Translation
			if (VerQueryValue(prgbVersionInfo, _T("\\"), (LPVOID*)&lpVSFixedFileInfo, &uiSize) && (uiSize != 0))
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
		GlobalFree(prgbVersionInfo);
	if (pdwMSVer)
		*pdwMSVer = dwMSVer;
	if (pdwLSVer)
		*pdwLSVer = dwLSVer;

	return dwResult;
}

/////////////////////////////////////////////////////////////////////////////
// WaitForProcess
//

//DWORD 
//WaitForHandle(HANDLE handle, DWORD dwTimeout)
//{
//	MSG msg = {0};
//    
//    //loop forever to wait
//	while (TRUE) {
//
//		//wait for object
//		DWORD dwWaitResult = MsgWaitForMultipleObjects(
//			1, 
//			&handle, 
//			FALSE, 
//			dwTimeout, 
//			QS_ALLINPUT);
//
//		switch (dwWaitResult) {
//        //success!
//        case WAIT_OBJECT_0:
//			return ERROR_SUCCESS;
//        //not the process that we're waiting for
//        case (WAIT_OBJECT_0 + 1):
//			if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) 
//			{
//				TranslateMessage(&msg);
//				DispatchMessage(&msg);
//			}
//			break; // break the switch
//        //did not return an OK; return error status
//        default:
//			return ::GetLastError();
//        }
//    }
//}


/////////////////////////////////////////////////////////////////////////////
// IsUpdateRequiredVersion
//
//  update package version is stamped as rmj.rmm.rup.rin
//
//
//static 
//BOOL 
//pIsUpdateRequiredVersion(
//	LPTSTR szFilename, 
//	ULONG ulMinVer)
//{
//    // get version of update package
//    DWORD dwUpdateMSVer;
//    DWORD dwRet = GetFileVersionNumber(szFilename, &dwUpdateMSVer, NULL);
//    if (ERROR_SUCCESS != dwRet)
//    {
//        // can't obtain version information; assume not proper version
//        XTLTRACE(
//			"Can't obtain version information for update package;"
//			"assuming it is not the proper version\n");
//        return FALSE;
//    }
//
//    // compare version at source to required minimum
//    ULONG ulSourceVer = HIWORD(dwUpdateMSVer) * 100 + LOWORD(dwUpdateMSVer);
//    if (ulSourceVer < ulMinVer)
//    {
//        // source version won't get us to our minimum version
//        XTLTRACE(
//			"The update package is improper version for upgrade."
//			"Update package Version = %d, Minimum Version = %d.\n", 
//			ulSourceVer, ulMinVer);
//        return FALSE;
//    }
//
//    return TRUE;
//}

/////////////////////////////////////////////////////////////////////////////
// ExecuteUpgradeMsi
//


//static
//DWORD 
//pExecuteUpgradeMsi(LPTSTR szUpgradeMsi)
//{
//    XTLTRACE(_T("Running update package from --> %s\n"), szUpgradeMsi);
//
//    DWORD dwResult = 0;
//
//    // build up CreateProcess structures
//	STARTUPINFO          sui = {0};
//	PROCESS_INFORMATION  pi = {0};
//
//    sui.cb          = sizeof(STARTUPINFO);
//    sui.dwFlags     = STARTF_USESHOWWINDOW;
//    sui.wShowWindow = SW_SHOW;
//
//    //
//    // build command line and specify delayreboot option to Update
//    //  three acounts for terminating null plus quotes for module
//	//
//    DWORD cchCommandLine = lstrlen(szUpgradeMsi) + lstrlen(MSI_DELAY_REBOOT_QUIET) + 3;
//    TCHAR *szCommandLine = new TCHAR[cchCommandLine];
//
//	if (!szCommandLine) {
//        return ERROR_OUTOFMEMORY;
//	}
//    
//    if (FAILED(StringCchCopy(szCommandLine, cchCommandLine, _T("\"")))
//        || FAILED(StringCchCat(szCommandLine, cchCommandLine, szUpgradeMsi))
//        || FAILED(StringCchCat(szCommandLine, cchCommandLine, _T("\"")))
//        || FAILED(StringCchCat(szCommandLine, cchCommandLine, MSI_DELAY_REBOOT_QUIET)))
//    {
//        delete [] szCommandLine;
//        return ERROR_INSTALL_FAILURE;
//    }
//
//    //
//    // run update process
//	//
//	BOOL fSuccess = ::CreateProcess(
//		NULL, 
//		szCommandLine, 
//		NULL, 
//		NULL, 
//		FALSE, 
//		CREATE_DEFAULT_ERROR_MODE, 
//		NULL, 
//		NULL, 
//		&sui, 
//		&pi);
//
//    if(!fSuccess)
//    {
//        // failed to launch.
//		dwResult = ::GetLastError();
//        delete [] szCommandLine;
//        return dwResult;
//    }
//
//    dwResult = WaitForHandle(pi.hProcess);
//    if(ERROR_SUCCESS != dwResult)
//    {
//        delete [] szCommandLine;
//        return dwResult;
//    }
//
//    DWORD dwExitCode = 0;
//    ::GetExitCodeProcess(pi.hProcess, &dwExitCode);
//
//    ::CloseHandle(pi.hProcess);
//
//    delete [] szCommandLine;
//
//    return dwExitCode;
//}

BOOL 
IsMsiUpgradeNecessary(ULONG ulReqMsiMinVer)
{
	//
	// attempt to load msi.dll in the system directory
	//
	TCHAR szSysMsiDll[MAX_PATH] = {0};
	if (!GetMsiDllPath(szSysMsiDll, MAX_PATH))
	{
		// failure to get path to msi.dll; assume upgrade is necessary
		ATLTRACE("Can't obtain msi.dll path; assuming upgrade is necessary\n");
		return TRUE;
	}

	XTL::AutoModuleHandle hModule = LoadLibrary(szSysMsiDll);
	if (hModule.IsInvalid())
	{
		// can't load msi.dll; assume upgrade is necessary
		ATLTRACE("Can't load msi.dll; assuming upgrade is necessary\n");
		return TRUE;
	}

	GenericDllGetVersion msidll(hModule);

	if (!msidll.IsProcAvailable("DllGetVersion"))
	{
		// can't load DllGetVersion, it is not a true MSI.DLL
		ATLTRACE("DllGetVersion is not available in msi.dll; "
			"assuming upgrade is necessary\n");
		return TRUE;
	}

	DLLVERSIONINFO dvi = {0};
	dvi.cbSize = sizeof(DLLVERSIONINFO);
	HRESULT hr = msidll.DllGetVersion(&dvi);
	if (FAILED(hr))
	{
		// can't load msi.dll; assume upgrade is necessary
		ATLTRACE("DllGetVersion failed; assuming upgrade is necessary\n");
		return TRUE;
	}
	// compare version in system to the required minimum
	ULONG ulInstalledVer = dvi.dwMajorVersion * 100 + dvi.dwMinorVersion;
	if (ulInstalledVer < ulReqMsiMinVer)
	{
		// upgrade is necessary
		ATLTRACE("Windows Installer upgrade is required."
			" System Version = %d, Minimum Version = %d.\n", 
			ulInstalledVer, ulReqMsiMinVer);
		return TRUE;
	}

	// no upgrade is necessary
	ATLTRACE("No upgrade is necessary. (%d)"
		" System version meets minimum requirements.\n", ulInstalledVer);

	return FALSE;
}

// returns the handle of the process invoked
HANDLE
RunMsiInstaller(
	LPCTSTR InstallCommand)
{
	ATLTRACE("Running update package from --> %ls\n", InstallCommand);

	// build up CreateProcess structures
	STARTUPINFO          si = {0};
	PROCESS_INFORMATION  pi = {0};

	si.cb          = sizeof(STARTUPINFO);
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;

	//
	// build command line and specify delayreboot option to Update
	//  three acounts for terminating null plus quotes for module
	//

	DWORD cchCommandLine = lstrlen(InstallCommand) + 1;
	XTL::AutoProcessHeapPtr<TCHAR> szCommandLine = 
		static_cast<TCHAR*>(
		HeapAlloc(
		GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		sizeof(TCHAR) * cchCommandLine));

	if (szCommandLine.IsInvalid()) 
	{
		return NULL; // ERROR_OUTOFMEMORY;
	}

	if (FAILED(StringCchCopy(szCommandLine, cchCommandLine, InstallCommand)))
	{
		return NULL; // ERROR_INSTALL_FAILURE;
	}

	ATLTRACE("instmsi=%ls\n", szCommandLine);

	//
	// run update process
	//
	BOOL fSuccess = CreateProcess(
		NULL, 
		szCommandLine, 
		NULL, 
		NULL, 
		FALSE, 
		CREATE_DEFAULT_ERROR_MODE, 
		NULL, 
		NULL, 
		&si, 
		&pi);

	if(!fSuccess)
	{
		// failed to launch.
		return NULL;
	}

	return pi.hProcess;
}
