#include "precomp.hpp"
#include "xdi.h"

#define MAX_DEVICE_ID_LEN   200     // size in chars

BOOL 
WINAPI
pSetupUninstallOEMInfW(
    __in LPCWSTR InfFileName,
    __in DWORD Flags,
	__in PVOID Reserved);

//
// global system catalog guid
//
// {F750E6C3-38EE-11d1-85E5-00C04FC295EE}
const GUID DriverVerifyGuid = {
	0xf750e6c3,0x38ee,0x11d1,
	0x85,0xe5,0x0,0xc0,0x4f,0xc2,0x95,0xee};

const LPCWSTR INFPATH_VALUE_NAME = REGSTR_VAL_INFPATH;

typedef struct _DLL_HANDLE_DATA {
	LPCWSTR ModuleName;
	HMODULE ModuleHandle;
} DLL_HANDLE_DATA;

DLL_HANDLE_DATA WintrustLibrary = { L"wintrust.dll" };
DLL_HANDLE_DATA SetupapiLibrary = { L"setupapi.dll" };
DLL_HANDLE_DATA NewdevLibrary = { L"newdev.dll" };

typedef HANDLE HCATADMIN;

typedef BOOL (WINAPI* CryptCATAdminAcquireContext_PROC)(HCATADMIN*, LPCGUID, DWORD);
typedef BOOL (WINAPI* CryptCATAdminReleaseContext_PROC)(HCATADMIN, DWORD);
typedef BOOL (WINAPI* CryptCATAdminRemoveCatalog_PROC)(HCATADMIN, LPCWSTR, DWORD);
typedef BOOL (WINAPI* SetupUninstallOEMInfW_PROC)(LPCWSTR, DWORD, PVOID);
typedef BOOL (WINAPI* UpdateDriverForPlugAndPlayDevicesW_PROC)(HWND, LPCWSTR, LPCWSTR, DWORD, PBOOL);

template <typename T>
void pGetProcAddress(T& pfn, DLL_HANDLE_DATA& DllHandleData, LPCSTR ProcName)
{
	if (NULL == DllHandleData.ModuleHandle)
	{
		HMODULE h = LoadLibraryW(DllHandleData.ModuleName);
		if (h == NULL) 
		{
			pfn = NULL;
			return;
		}
		InterlockedExchangePointer((VOID**)&DllHandleData.ModuleHandle, h);
	}
	pfn = (T) GetProcAddress(DllHandleData.ModuleHandle, ProcName);
}

BOOL 
WINAPI
Mscat_CryptCATAdminAcquireContext(
	__out HCATADMIN* phCatAdmin,
	__in const GUID* pgSubsystem,
	__in DWORD dwFlags)
{
	CryptCATAdminAcquireContext_PROC pfn;
	pGetProcAddress(pfn, WintrustLibrary, "CryptCATAdminAcquireContext");
	if (NULL == pfn) return FALSE;
	return (*pfn)(phCatAdmin, pgSubsystem, dwFlags);
}

BOOL 
WINAPI
Mscat_CryptCATAdminReleaseContext(
	__in HCATADMIN hCatAdmin,
	__in DWORD dwFlags)
{
	CryptCATAdminReleaseContext_PROC pfn;
	pGetProcAddress(pfn, WintrustLibrary, "CryptCATAdminReleaseContext");
	if (NULL == pfn) return FALSE;
	return (*pfn)(hCatAdmin, dwFlags);
}

BOOL WINAPI
Mscat_CryptCATAdminRemoveCatalog(
	__in HCATADMIN hCatAdmin,
	__in LPCWSTR szCatalogFile,
	__in DWORD dwFlags)
{
	CryptCATAdminRemoveCatalog_PROC pfn;
	pGetProcAddress(pfn, WintrustLibrary, "CryptCATAdminRemoveCatalog");
	if (NULL == pfn) return FALSE;
	return (*pfn)(hCatAdmin, szCatalogFile, dwFlags);
}

BOOL WINAPI
Setupapi_SetupUninstallOEMInfW(
	__in LPCWSTR szInfFileName,
	__in DWORD Flags,
	__in PVOID Reserved)
{
	SetupUninstallOEMInfW_PROC pfn;
	pGetProcAddress(pfn, SetupapiLibrary, "SetupUninstallOEMInfW");
	if (NULL == pfn) pfn = pSetupUninstallOEMInfW;
	return (*pfn)(szInfFileName, Flags, Reserved);
}

BOOL
WINAPI
Newdev_UpdateDriverForPlugAndPlayDevicesW(
	__in HWND hwndParent,
	__in LPCWSTR HardwareId,
	__in LPCWSTR FullInfPath,
	__in DWORD InstallFlags,
	__in_opt PBOOL bRebootRequired)
{
	UpdateDriverForPlugAndPlayDevicesW_PROC pfn;
	pGetProcAddress(pfn, NewdevLibrary, "UpdateDriverForPlugAndPlayDevicesW");
	if (NULL == pfn) return FALSE;
	return (*pfn)(hwndParent, HardwareId, FullInfPath, InstallFlags, bRebootRequired);
}

BOOL WINAPI
pSetupUninstallCatalogW(
    IN LPCWSTR szCatalogFile)
{
    HCATADMIN hCatAdmin;

    BOOL success = Mscat_CryptCATAdminAcquireContext(
		&hCatAdmin, 
		&DriverVerifyGuid, 
		0);

	if (!success) 
	{
		return FALSE;
	}

	success = Mscat_CryptCATAdminRemoveCatalog(
		hCatAdmin, 
		szCatalogFile, 
		0);

	DWORD se = ::GetLastError();
    (VOID) Mscat_CryptCATAdminReleaseContext(hCatAdmin, 0);
	::SetLastError(se);

    return success;
}


LPCWSTR WINAPI
pGetFilePart(
    IN LPCWSTR FilePath)
{
    LPCWSTR LastComponent = FilePath;
    WCHAR CurChar;

    while(CurChar = *FilePath) 
	{
        FilePath = CharNextW(FilePath);
        if((CurChar == L'\\') || (CurChar == L'/') || (CurChar == L':')) 
		{
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
pAnyDeviceUsingInfW(
	IN LPCWSTR InfFullPath)
{
    //
    // If we are passed a NULL InfFullPath or an empty string then just
    // return FALSE since nobody is using this.
    //
    if (!InfFullPath || (InfFullPath[0] == L'\0')) 
	{
        return FALSE;
    }

	LPCWSTR infFilePart = pGetFilePart(InfFullPath);

	HDEVINFO devInfoSet = SetupDiGetClassDevsW(
		NULL,
		NULL,
		NULL,
		DIGCF_ALLCLASSES);

	if (INVALID_HANDLE_VALUE == devInfoSet) 
	{
		return FALSE;
	}

	WCHAR currentDeviceInfFile[MAX_PATH];
	WCHAR deviceId[MAX_DEVICE_ID_LEN];

	DWORD index = 0;
	SP_DEVINFO_DATA devInfoData = { sizeof(SP_DEVINFO_DATA) };

	DWORD se = ERROR_SUCCESS;

	while (TRUE) 
	{
		BOOL success = SetupDiEnumDeviceInfo(
			devInfoSet,
			index++,
			&devInfoData);

		if (!success) 
		{
			break;
		}

		//
		// Open the 'driver' key for this device.
		//
		HKEY keyHandle = SetupDiOpenDevRegKey(
			devInfoSet,
			&devInfoData,
			DICS_FLAG_GLOBAL,
			0,
			DIREG_DRV,
			KEY_READ);

		if (INVALID_HANDLE_VALUE == keyHandle) 
		{
			continue;
		}

		DWORD valueDataSize = sizeof(currentDeviceInfFile);
		DWORD type = REG_SZ;

		LONG ret = RegQueryValueExW(
			keyHandle,
			INFPATH_VALUE_NAME,
			NULL,
			&type,
			(LPBYTE)currentDeviceInfFile,
			&valueDataSize);

		if (ERROR_SUCCESS == ret &&
			0 == lstrcmpiW(currentDeviceInfFile, infFilePart))
		{
			//
			// This key is using this INF file so the INF can't be
			// deleted.
			//
			se = ERROR_SHARING_VIOLATION;
			RegCloseKey(keyHandle);
			SetupDiDestroyDeviceInfoList(devInfoSet);
			SetLastError(se);
			return FALSE;
		}

		RegCloseKey(keyHandle);
	} 

	SetupDiDestroyDeviceInfoList(devInfoSet);
	if (ERROR_SUCCESS != se) SetLastError(se);

    return (se != ERROR_SUCCESS);
}


BOOL WINAPI
pFileExists(LPCWSTR FilePath)
{
	DWORD errorCode;
	UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	WIN32_FIND_DATAW findFileData;
	HANDLE findFileHandle = FindFirstFileW(FilePath, &findFileData);

	if (INVALID_HANDLE_VALUE == findFileHandle) 
	{
		errorCode = GetLastError();
	}
	else 
	{
		FindClose(findFileHandle);
		errorCode = ERROR_SUCCESS;
	}

	SetErrorMode(oldErrorMode);
	SetLastError(errorCode);

	return (errorCode == ERROR_SUCCESS);
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
BOOL 
WINAPI
pSetupUninstallOEMInfW(
    __in LPCWSTR InfFileName,
    __in DWORD Flags,
	__in PVOID Reserved)
{
	WCHAR infFullPath[MAX_PATH];
	UINT n = GetSystemWindowsDirectoryW(infFullPath, MAX_PATH);
	_ASSERTE(n != 0);
	if (0 == n) return FALSE;

	HRESULT hr = StringCchCatW(infFullPath, MAX_PATH, L"\\INF\\");
	_ASSERTE(SUCCEEDED(hr));

	hr = StringCchCatW(infFullPath, MAX_PATH, pGetFilePart(InfFileName));
	_ASSERTE(SUCCEEDED(hr));

	if (!pFileExists(infFullPath))
	{
		return FALSE;
	}

	LPCWSTR CAT_SUFFIX = L".cat";
	LPCWSTR INF_SUFFIX = L".inf";
	LPCWSTR PNF_SUFFIX = L".pnf";

	BOOL success = FALSE;

	WCHAR fileNameBuffer[MAX_PATH+4]; // +4 in case filename is aaa. not aaa.inf
    LPWSTR extension = NULL;

    //
    // Unless the caller passed in the SUOI_FORCEDELETE flag first check that
    // no devices are using this INF file.
    //
    if (!(Flags & SUOI_FORCEDELETE) && pAnyDeviceUsingInfW(infFullPath)) 
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
	hr = StringCchCopyW(fileNameBuffer, RTL_NUMBER_OF(fileNameBuffer), infFullPath);
	_ASSERTE(SUCCEEDED(hr));

    //
    // Uninstall the catalog (if any) first, because as soon as we delete the
    // INF, that slot is "open" for use by another INF, and we wouldn't want to
    // inadvertently delete someone else's catalog due to a race condition.
    //
    extension = wcsrchr(fileNameBuffer, L'.');
    if (!extension) 
	{
        //
        // not xxx.inf, so we know there is no catalog to delete
        //
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
    }

	hr = StringCchCopyW(extension, MAX_PATH, CAT_SUFFIX);
	_ASSERTE(SUCCEEDED(hr));

	success = pSetupUninstallCatalogW(pGetFilePart(fileNameBuffer));

    //
    // Now delete the PNF (we don't care so much if this succeeds or fails)...
    //
    hr = StringCchCopyW(wcsrchr(fileNameBuffer, L'.'), MAX_PATH, PNF_SUFFIX);
	_ASSERTE(SUCCEEDED(hr));
	(void) DeleteFileW(fileNameBuffer);

    //
    // and finally the INF itself...
    //
	hr = StringCchCopyW(wcsrchr(fileNameBuffer, L'.'), MAX_PATH, INF_SUFFIX);
	_ASSERTE(SUCCEEDED(hr));

	success = DeleteFileW(fileNameBuffer);
	if (!success) 
	{
		return FALSE;
	}

	return TRUE;
}
