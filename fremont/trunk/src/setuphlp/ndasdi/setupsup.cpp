#include "stdafx.h"
#include <regstr.h>
#include <xtl/xtlautores.h>
#include "setupsup.h"

#include "trace.h"
#ifdef RUN_WPP
#include "setupsup.tmh"
#endif

#define MAX_DEVICE_ID_LEN   200     // size in chars

namespace XTL 
{

struct AutoHDevInfoConfig {
	static HDEVINFO GetInvalidValue() { return (HDEVINFO) NULL; }
	static void Release(HDEVINFO h)
	{
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::SetupDiDestroyDeviceInfoList(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HDEVINFO, AutoHDevInfoConfig> AutoHDevInfo;

} // XTL

//
// global system catalog guid
//
// {F750E6C3-38EE-11d1-85E5-00C04FC295EE}
CONST GUID DriverVerifyGuid = {
	0xf750e6c3,0x38ee,0x11d1,
	0x85,0xe5,0x0,0xc0,0x4f,0xc2,0x95,0xee};

CONST LPCTSTR INFPATH_VALUE_NAME = REGSTR_VAL_INFPATH;

LPCTSTR WINTRUST_MODULE_NAME = _T("wintrust.dll");
LPCTSTR SETUPAPI_MODULE_NAME = _T("setupapi.dll");

typedef HANDLE HCATADMIN;

BOOL WINAPI
Mscat_CryptCATAdminAcquireContext(
	OUT HCATADMIN* phCatAdmin,
	IN const GUID* pgSubsystem,
	IN DWORD dwFlags)
{
	LPCSTR szProcName = "CryptCATAdminAcquireContext";
	typedef BOOL (WINAPI* DLL_PROC)(HCATADMIN*, const GUID*, DWORD);

	XTL::AutoModuleHandle hModule = ::LoadLibrary(WINTRUST_MODULE_NAME);
	if (NULL == (HMODULE)hModule) {
		return NULL;
	}
	
	DLL_PROC fn = reinterpret_cast<DLL_PROC>(
		::GetProcAddress(hModule, szProcName));

	if (NULL == fn) {
		return FALSE;
	}

	return fn(phCatAdmin, pgSubsystem, dwFlags);
}

BOOL WINAPI
Mscat_CryptCATAdminReleaseContext(
	HCATADMIN hCatAdmin,
	DWORD dwFlags)
{
	LPCSTR szProcName = "CryptCATAdminReleaseContext";
	typedef BOOL (WINAPI* DLL_PROC)(HCATADMIN, DWORD);

	XTL::AutoModuleHandle hModule = ::LoadLibrary(WINTRUST_MODULE_NAME);
	if (NULL == (HMODULE) hModule) {
		return NULL;
	}

	DLL_PROC fn = reinterpret_cast<DLL_PROC>(
		::GetProcAddress(hModule, szProcName));

	if (NULL == fn) {
		return FALSE;
	}

	return fn(hCatAdmin, dwFlags);
}

BOOL WINAPI
Mscat_CryptCATAdminRemoveCatalog(
	HCATADMIN hCatAdmin,
	LPCTSTR szCatalogFile,
	DWORD dwFlags)
{
#ifdef UNICODE
	LPCWSTR wszCatalogFile = szCatalogFile;
#else
	LPWSTR wszCatalogFile[MAX_PATH];
	if (!::MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED,
		szCatalogFile,
		-1,
		wszCatalogFile,
		RTL_NUMBER_OF(wszCatalogFile)))
	{
			return FALSE;
	}

#endif

	LPCSTR szProcName = "CryptCATAdminRemoveCatalog";
	typedef BOOL (WINAPI* DLL_PROC)(HCATADMIN, LPCWSTR, DWORD);

	XTL::AutoModuleHandle hModule = ::LoadLibrary(WINTRUST_MODULE_NAME);
	if (NULL == (HMODULE) hModule) {
		return NULL;
	}

	DLL_PROC fn = reinterpret_cast<DLL_PROC>(
		::GetProcAddress(hModule, szProcName));

	if (NULL == fn) {
		return FALSE;
	}

	return fn(hCatAdmin, wszCatalogFile, dwFlags);
}

BOOL WINAPI
pSetupUninstallCatalog(
    IN LPCTSTR szCatalogFile)
{
    DWORD Err;
    HCATADMIN hCatAdmin;

    BOOL fSuccess = Mscat_CryptCATAdminAcquireContext(
		&hCatAdmin, 
		&DriverVerifyGuid, 
		0);

	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = Mscat_CryptCATAdminRemoveCatalog(
		hCatAdmin, 
		szCatalogFile, 
		0);

	Err = ::GetLastError();
    (VOID) Mscat_CryptCATAdminReleaseContext(hCatAdmin, 0);
	::SetLastError(Err);

    return fSuccess;
}


PCTSTR WINAPI
pGetFilePart(
    IN PCTSTR FilePath)
{
    PCTSTR LastComponent = FilePath;
    TCHAR  CurChar;

    while(CurChar = *FilePath) {
        FilePath = CharNext(FilePath);
        if((CurChar == TEXT('\\')) || (CurChar == TEXT('/')) || (CurChar == TEXT(':'))) {
            LastComponent = FilePath;
        }
    }

    return LastComponent;
}

/*++

Routine Description:

This routine checks if any device, live or phantom, is using this INF file,
and logs if they are.

Arguments:

InfFullPath - supplies the full path of the INF.

Return Value:

TRUE if any device is still using this INF, FALSE if no devices are using
this INF.

--*/
BOOL WINAPI
pAnyDeviceUsingInf(
	IN  LPCTSTR szInfFullPath)
{
    //
    // If we are passed a NULL InfFullPath or an empty string then just
    // return FALSE since nobody is using this.
    //
    if (!szInfFullPath || (szInfFullPath[0] == TEXT('\0'))) {
        return FALSE;
    }

	PTSTR pInfFile;

	pInfFile = (PTSTR) pGetFilePart(szInfFullPath);

	XTL::AutoHDevInfo DeviceInfoSet = ::SetupDiGetClassDevs(
		NULL, 
		NULL, 
		NULL, 
		DIGCF_ALLCLASSES);

	if (INVALID_HANDLE_VALUE == DeviceInfoSet) {
		return FALSE;
	}

	TCHAR CurrentDeviceInfFile[MAX_PATH];
	TCHAR DeviceId[MAX_DEVICE_ID_LEN];

	DWORD dwIndex = 0;
	SP_DEVINFO_DATA DeviceInfoData;
	DeviceInfoData.cbSize = sizeof(DeviceInfoData);

	DWORD Err = ERROR_SUCCESS;

	while (TRUE) {

		BOOL fSuccess = ::SetupDiEnumDeviceInfo(
			DeviceInfoSet,
			dwIndex++,
			&DeviceInfoData);

		if (!fSuccess) {
			break;
		}

		//
		// Open the 'driver' key for this device.
		//
		XTL::AutoKeyHandle hKey = ::SetupDiOpenDevRegKey(
			DeviceInfoSet,
			&DeviceInfoData,
			DICS_FLAG_GLOBAL,
			0,
			DIREG_DRV,
			KEY_READ);

		if (INVALID_HANDLE_VALUE == hKey) {
			continue;
		}

		DWORD cbSize = sizeof(CurrentDeviceInfFile);
		DWORD dwType = REG_SZ;

		LONG lRet = RegQueryValueEx(hKey,
			INFPATH_VALUE_NAME,
			NULL,
			&dwType,
			(LPBYTE)CurrentDeviceInfFile,
			&cbSize);

		if (ERROR_SUCCESS == lRet &&
			0 == lstrcmpi(CurrentDeviceInfFile, pInfFile))
		{
			//
			// This key is using this INF file so the INF can't be
			// deleted.
			//
			Err = ERROR_SHARING_VIOLATION;
			::SetLastError(Err);
		}
	} 

    return (Err != ERROR_SUCCESS);
}


BOOL WINAPI
pFileExists(LPCTSTR szFilePath)
{
	HANDLE hFindFile;
	WIN32_FIND_DATA ffData;
	UINT uiOldErrorMode;
	DWORD Error;

	uiOldErrorMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);

	hFindFile = ::FindFirstFile(szFilePath,&ffData);
	if(INVALID_HANDLE_VALUE == hFindFile) {
		Error = GetLastError();
	} else {
		::FindClose(hFindFile);
		Error = ERROR_SUCCESS;
	}

	::SetErrorMode(uiOldErrorMode);

	::SetLastError(Error);
	return (Error == ERROR_SUCCESS);
}

/*++

Routine Description:

This routine uninstalls a 3rd-party INF, PNF, and CAT (if one exists).

By default this routine will first verify that there aren't any other
device's, live and phantom, that are pointing their InfPath to this
INF. This behavior can be overridden by the SUOI_FORCEDELETE flag.

Arguments:

InfFullPath - supplies the full path of the INF to be uninstalled.

LogContext - optionally, supplies the log context to be used if we
encounter an error when attempting to delete the catalog.

Flags - the following flags are supported:
SUOI_FORCEDELETE - delete the INF even if other driver keys are
have their InfPath pointing to it.

InfDeleteErr - optionally, supplies the address of a variable that receives
the error (if any) encountered when attempting to delete the INF.
Note that we delete the INF last (to avoid race conditions), so the
corresponding CAT and PNF may have already been deleted at this point.

Return Value:

None.

--*/
BOOL WINAPI
SetupUninstallOEMInf_Impl(
    IN LPCTSTR	InfFileName,
    IN DWORD	Flags,
	IN PVOID	Reserved)
{

	TCHAR szInfFullPath[MAX_PATH];
	UINT uiRet = ::GetSystemWindowsDirectory(szInfFullPath, MAX_PATH);
	_ASSERTE(0 != uiRet);
	HRESULT hr;

	hr = ::StringCchCat(szInfFullPath, MAX_PATH, _T("\\INF\\"));
	_ASSERTE(SUCCEEDED(hr));

	hr = ::StringCchCat(szInfFullPath, MAX_PATH, pGetFilePart(InfFileName));
	_ASSERTE(SUCCEEDED(hr));

	if (!pFileExists(szInfFullPath)) {
		return FALSE;
	}

	LPCTSTR CAT_SUFFIX = _T(".cat");
	LPCTSTR INF_SUFFIX = _T(".inf");
	LPCTSTR PNF_SUFFIX = _T(".pnf");

	BOOL fSuccess = FALSE;

    TCHAR FileNameBuffer[MAX_PATH+4]; // +4 in case filename is aaa. not aaa.inf
    LPTSTR ExtPtr= NULL;

    //
    // Unless the caller passed in the SUOI_FORCEDELETE flag first check that
    // no devices are using this INF file.
    //
    if (!(Flags & SUOI_FORCEDELETE) &&
        pAnyDeviceUsingInf(szInfFullPath)) 
	{
        //
        // Some device is still using this INF so we can't delete it. 
        //
		// ERROR_SHARING_VIOLATION will be set by pAnyDeviceUsingInf
		return FALSE;
    }

    //
    // Copy the caller-supplied INF name into a local buffer, so we can modify
    // it when deleting the various files (INF, PNF, and CAT).
    //
    lstrcpyn(FileNameBuffer, szInfFullPath, RTL_NUMBER_OF(FileNameBuffer));

    //
    // Uninstall the catalog (if any) first, because as soon as we delete the
    // INF, that slot is "open" for use by another INF, and we wouldn't want to
    // inadvertently delete someone else's catalog due to a race condition.
    //
    ExtPtr = _tcsrchr(FileNameBuffer, TEXT('.'));
    if(!ExtPtr) {
        //
        // not xxx.inf, so we know there is no catalog to delete
        //
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
    }

	hr = ::StringCchCopy(ExtPtr, MAX_PATH, CAT_SUFFIX);
	_ASSERTE(SUCCEEDED(hr));

	fSuccess = pSetupUninstallCatalog(pGetFilePart(FileNameBuffer));

    //
    // Now delete the PNF (we don't care so much if this succeeds or fails)...
    //
    hr = ::StringCchCopy(_tcsrchr(FileNameBuffer, TEXT('.')), MAX_PATH, PNF_SUFFIX);
	_ASSERTE(SUCCEEDED(hr));
	(VOID) ::DeleteFile(FileNameBuffer);

    //
    // and finally the INF itself...
    //
	hr = ::StringCchCopy(_tcsrchr(FileNameBuffer, TEXT('.')), MAX_PATH, INF_SUFFIX);
	_ASSERTE(SUCCEEDED(hr));

	fSuccess = ::DeleteFile(FileNameBuffer);
	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}


BOOL WINAPI
Setupapi_SetupUninstallOEMInf(
	LPCTSTR szInfFileName,
	DWORD Flags,
	PVOID Reserved)
{
	typedef BOOL (WINAPI *DLL_PROC)(PCWSTR, DWORD, PVOID);

	HMODULE hSetupApiDll = ::LoadLibrary(SETUPAPI_MODULE_NAME);
	LPCSTR szProcName = "SetupUninstallOEMInfW";

	if (NULL == hSetupApiDll) {
		return FALSE;
	}

	DLL_PROC pProc = (DLL_PROC)::GetProcAddress(hSetupApiDll, szProcName);
	if (NULL == pProc) {
		// fallback for manual uninstallation
		return SetupUninstallOEMInf_Impl(szInfFileName, Flags, Reserved);
	}

	return pProc(szInfFileName, Flags, Reserved);
}
