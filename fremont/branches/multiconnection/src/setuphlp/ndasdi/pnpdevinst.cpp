/*++

  Copyright (c) 2003-2004 XIMETA, Inc.

  Module Name:

    pnpdevinst.c

  Abstract:

  Environment:

  Notes:

  Original Disclaimer:

  Revision History:

  10/XX/2003: cslee, Initial update
   8/16/2004: cslee, major overhauling

--*/

#include "stdafx.h"
// #include <newdev.h> // for the API UpdateDriverForPlugAndPlayDevices().
#include <regstr.h>

#include <xtl/xtlautores.h>
#include "pnpdevinst.h"

#include "trace.h"
#ifdef RUN_WPP
#include "pnpdevinst.tmh"
#endif

#define MAX_CLASS_NAME_LEN 32 // Stolen from <cfgmgr32.h>

/*++

Routine Description:
    CopyDriverInf
    CopyDriver files to the system

Arguments:

Return Value:

    EXIT_xxxx

--*/

//
// stub function for dynamic binding
//
BOOL
pUpdateDriverForPlugAndPlayDevices(
	HWND hWndParent,
	LPCTSTR HardwareId,
	LPCTSTR FullInfPath,
	DWORD InstallFlags,
	PBOOL bRebootRequired OPTIONAL);

//
// Returns the full path of the szPath
//
// If the UNICODE is defined, 
// path prefix "\\?\" is appended.
//
// Returns NULL is any error occurs, call GetLastError
// for extended information.
//
// Required to free the returned string with LocalFree 
// if not null
//

#define RFP_NO_PATH_PREFIX 0x0001

static LPTSTR ResolveFullPath(
	IN LPCTSTR szPath, 
	OUT LPDWORD pcchFullPath = NULL,
	OUT LPTSTR* ppszFilePart = NULL,
	IN DWORD Flags = 0);

static LPTSTR ResolveFullPath(
	IN LPCTSTR szPath, 
	OUT LPDWORD pcchFullPath, 
	OUT LPTSTR* ppszFilePart,
	IN DWORD Flags)
{

#ifdef UNICODE
static const TCHAR PATH_PREFIX[] = L"\\\\?\\";
static const DWORD PATH_PREFIX_LEN = RTL_NUMBER_OF(PATH_PREFIX);
#else
static const TCHAR PATH_PREFIX[] = "";
static const DWORD PATH_PREFIX_LEN = RTL_NUMBER_OF(PATH_PREFIX);
#endif

	//
	// szPathBuffer = \\?\
	// lpszPathBuffer ptr ^
	//
	LPTSTR lpszLongPathBuffer = NULL;
	BOOL fSuccess = FALSE;

	// Inf must be a full pathname
	DWORD cch = GetFullPathName(
		szPath,
		0,
		NULL,
		NULL);

	if (0 == cch) {
		return NULL;
	}
	
	// cch contains required buffer size
	lpszLongPathBuffer = (LPTSTR) LocalAlloc(
		LPTR,
		(PATH_PREFIX_LEN - 1 + cch) * sizeof(TCHAR));

	if (NULL == lpszLongPathBuffer) {
		// out of memory
		return NULL;
	}

	
	// lpsz is a path without path prefix
	LPTSTR lpsz = lpszLongPathBuffer + (PATH_PREFIX_LEN - 1);
	
	cch = GetFullPathName(
		szPath,
		cch,
		lpsz,
		ppszFilePart);

	if (0 == cch) {
		LocalFree(lpszLongPathBuffer);
		return NULL;
	}

	if (NULL != pcchFullPath) {
		*pcchFullPath = cch;
	}
	
	if (!(Flags & RFP_NO_PATH_PREFIX)) {
		if (_T('\\') != lpsz[0] || _T('\\') != lpsz[1]) {
			// UNC path does not support path prefix "\\?\" 
			// Also path with \\?\ does not need path prefix
			::CopyMemory(
				lpszLongPathBuffer, 
				PATH_PREFIX, 
				(PATH_PREFIX_LEN - 1) * sizeof(TCHAR));
			lpsz = lpszLongPathBuffer;
			if (NULL != pcchFullPath) {
				*pcchFullPath += PATH_PREFIX_LEN;
			}
		}
	}
	
	return lpsz;
}

NDASDI_API
BOOL 
WINAPI
NdasDiFindExistingDevice(
	IN HWND hwndParent,
	IN LPCTSTR HardwareId,
	IN BOOL PresentOnly)
{
	BOOL Found = FALSE;
	BOOL fReturn = FALSE;
	BOOL fSuccess = FALSE;

	HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
	SP_DEVINFO_DATA DeviceInfoData = {0};
	DWORD Flags = DIGCF_ALLCLASSES;

	if (PresentOnly) {
		Flags |= DIGCF_PRESENT;
	}

	DeviceInfoSet = SetupDiGetClassDevs(
		NULL, 
		0, 
		hwndParent,
		Flags);

	if (INVALID_HANDLE_VALUE == DeviceInfoSet) {
		return FALSE;
	}

	for (DWORD i = 0; ; ++i) {
		
		DeviceInfoData.cbSize = sizeof(DeviceInfoData);
		fSuccess = SetupDiEnumDeviceInfo(
			DeviceInfoSet,
			i,
			&DeviceInfoData);

		if (!fSuccess) {
			break;
		}

		DWORD DataT;
		LPTSTR buffer = NULL;
		DWORD buffersize = 0;

		//
		// We won't know the size of the HardwareID buffer until we call
		// this function. So call it with a null to begin with, and then 
		// use the required buffer size to Alloc the nessicary space.
		// Keep calling we have success or an unknown failure.
		//
		while (TRUE) {

			fSuccess = SetupDiGetDeviceRegistryProperty(
				DeviceInfoSet,
				&DeviceInfoData,
				SPDRP_HARDWAREID,
				&DataT,
				(PBYTE)buffer,
				buffersize,
				&buffersize);

			if (fSuccess) {
				break;
			}

			if (ERROR_INVALID_DATA == GetLastError()) {
				//
				// May be a Legacy Device with no HardwareID. Continue.
				//
				break;
			} else if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
				//
				// We need to change the buffer size.
				//
				if (buffer) {
					LocalFree(buffer);
				}
				buffer = (LPTSTR) LocalAlloc(LPTR,buffersize);

			} else {
				//
				// Unknown Failure.
				//
				goto cleanup;
			}            
		}

		//
		// May be a Legacy Device with no HardwareID. Continue.
		//
		if (GetLastError() == ERROR_INVALID_DATA) {
			continue;
		}

		//
		// Compare each entry in the buffer multi-sz list with our HardwareID.
		//
		for (LPTSTR p = buffer; 
			*p && (p < & buffer[buffersize]);
			p += lstrlen(p) + 1) 
		{
			if (0 == lstrcmpi(HardwareId,p)) {
				Found = TRUE;
				break;
			}
		}

		if (buffer) {
			LocalFree(buffer);
		}

		if (Found) {
			break;
		}
	}

	if (Found) {
		SetLastError(ERROR_SUCCESS);
		fReturn = TRUE;
	}

cleanup:

	DWORD err = GetLastError();

	if (INVALID_HANDLE_VALUE != DeviceInfoSet) {
		(VOID) SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}

	SetLastError(err);
	return fReturn;
}

/*++

Routine Description:

    InstallDevice
    install a device manually

Arguments:

  szInfPath    - relative or absolute path to INF file
  szHardwareId - hardware ID to install device

Return Value:

    EXIT_xxxx

--*/

NDASDI_API
BOOL 
WINAPI 
NdasDiInstallRootEnumeratedDevice(
	IN HWND hwndParent,
	IN LPCTSTR szInfPath, 
	IN LPCTSTR szHardwareId,
	IN DWORD InstallFlags,
	IN LPBOOL pbRebootRequired OPTIONAL)
{
	_ASSERTE(NULL == pbRebootRequired || !IsBadWritePtr(pbRebootRequired, sizeof(BOOL)));

	BOOL fReturn = FALSE;
	BOOL fSuccess = FALSE;

    DWORD len;

    SP_DEVINFO_DATA DeviceInfoData;
    GUID ClassGuid;
    TCHAR ClassName[MAX_CLASS_NAME_LEN];
	
	LPTSTR lpszFullInfPath = ResolveFullPath(szInfPath, NULL, NULL, 0);
	if (NULL == lpszFullInfPath) {
		goto cleanup;
	}

    //
    // List of hardware ID's must be double zero-terminated
    //
    TCHAR hardwareIdList[LINE_LEN+4] = {0};
	size_t cchHardwareIdList = RTL_NUMBER_OF(hardwareIdList);
	size_t cchRemaining = cchHardwareIdList;
	
	HRESULT hr = StringCchCopyNEx(
		hardwareIdList, 
		LINE_LEN + 4, 
		szHardwareId, 
		LINE_LEN,
		NULL,
		&cchRemaining,
		STRSAFE_FILL_BEHIND_NULL);

	_ASSERTE(SUCCEEDED(hr));

	cchHardwareIdList -= cchRemaining - 1;

    //
    // Use the INF File to extract the Class GUID.
    //
    fSuccess = SetupDiGetINFClass(
		lpszFullInfPath,
		&ClassGuid,
		ClassName,
		RTL_NUMBER_OF(ClassName),
		NULL);

	if (!fSuccess) {
		goto cleanup;
	}
	
    //
    // Create the container for the to-be-created Device Information Element.
    //
    HDEVINFO DeviceInfoSet = SetupDiCreateDeviceInfoList(
		&ClassGuid,
		hwndParent);

    if(INVALID_HANDLE_VALUE == DeviceInfoSet) {
		goto cleanup;
    }

    //
    // Now create the element.
    // Use the Class GUID and Name from the INF file.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    fSuccess = SetupDiCreateDeviceInfo(
		DeviceInfoSet,
        ClassName,
        &ClassGuid,
        NULL,
        hwndParent,
        DICD_GENERATE_ID,
        &DeviceInfoData);

	if (!fSuccess) {
		goto cleanup;
	}

    //
    // Add the HardwareID to the Device's HardwareID property.
    //
    fSuccess = SetupDiSetDeviceRegistryProperty(
		DeviceInfoSet,
        &DeviceInfoData,
        SPDRP_HARDWAREID,
        (LPBYTE)hardwareIdList,
        cchHardwareIdList * sizeof(TCHAR));
	
	if (!fSuccess) {
		goto cleanup;
	}

    //
    // Transform the registry element into an actual devnode
    // in the PnP HW tree.
    //
	fSuccess = SetupDiCallClassInstaller(
		DIF_REGISTERDEVICE,
        DeviceInfoSet,
        &DeviceInfoData);
	
	if (!fSuccess) {
		goto cleanup;
	}

	//
	// The element is now registered.
	// You must explicitly remove the device using DIF_REMOVE,
	// if any failure is encountered from now on.
	//

	//
    // update the driver for the device we just created
    //

	fSuccess = pUpdateDriverForPlugAndPlayDevices(
		hwndParent,
		szHardwareId,
		lpszFullInfPath,
		InstallFlags,
		pbRebootRequired);

	if (!fSuccess) {
		DWORD err = GetLastError();
		fSuccess = SetupDiCallClassInstaller(
			DIF_REMOVE,
			DeviceInfoSet,
			&DeviceInfoData);
		//
		// BUGBUG: If it may still require reboot?
		//
		SetLastError(err);
		goto cleanup;
	}

	fReturn = TRUE;
	
cleanup:

	// save the last error
	DWORD err = GetLastError();

	if (INVALID_HANDLE_VALUE != DeviceInfoSet) {
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}

	if (NULL != lpszFullInfPath) {
		LocalFree(lpszFullInfPath);
	}

	// restore the last error
	SetLastError(err);

	return fReturn;
}

/*++

Routine Description:
    UPDATE
    update driver for existing device(s)

Arguments:

Return Value:

    EXIT_xxxx

--*/

NDASDI_API
BOOL 
WINAPI 
NdasDiUpdateDeviceDriver(
	IN HWND hwnd,
	IN LPCTSTR szInfPath, 
	IN LPCTSTR szHardwareId,
	IN DWORD InstallFlags,
	IN PBOOL pbRebootRequired OPTIONAL)
{
	_ASSERTE(NULL == pbRebootRequired || !IsBadWritePtr(pbRebootRequired, sizeof(BOOL)));

	BOOL fReturn = FALSE;
	BOOL fSuccess = FALSE;

	LPTSTR lpszFullInfPath = ResolveFullPath(szInfPath);

	if (NULL == lpszFullInfPath) {
		goto cleanup;
	}

    fSuccess = pUpdateDriverForPlugAndPlayDevices(
		hwnd,
		szHardwareId,
		lpszFullInfPath,
		InstallFlags,
		pbRebootRequired);
	
	if (!fSuccess) {
		goto cleanup;
	}

	fReturn = TRUE;

cleanup:

	// save the last error
	DWORD err = GetLastError();

	if (NULL != lpszFullInfPath) {
		LocalFree(lpszFullInfPath);
	}
	
	// restore the last error
	SetLastError(err);
	
	return fReturn;
}


/*++
Routine Discription:

Arguments:
    
    szHardwareId - PnP HardwareID of devices to remove.

Return Value:
    
    Standard Console ERRORLEVEL values:

    0 - Remove Successfull
    2 - Remove Failure.
    
--*/

NDASDI_API
BOOL
WINAPI 
NdasDiRemoveDevice(
	IN HWND hwndParent,
	IN LPCTSTR szHardwareId, 
	IN BOOL PresentOnly,
	OUT LPDWORD pdwRemovedDeviceCount OPTIONAL,
	OUT LPBOOL pbRebootRequired OPTIONAL,
	IN NDASDIREMOVEDEVICEERRCALLBACKPROC ErrorCallbackProc OPTIONAL,
	IN LPVOID Context OPTIONAL)
{
	_ASSERTE(!IsBadStringPtr(szHardwareId, LINE_LEN));
	_ASSERTE(NULL == pbRebootRequired || !IsBadWritePtr(pbRebootRequired, sizeof(BOOL)));
	_ASSERTE(NULL == ErrorCallbackProc || !IsBadCodePtr((FARPROC)ErrorCallbackProc));

	BOOL fReturn = FALSE;
	BOOL fSuccess = FALSE;

    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData = {0};
	DWORD Flags = DIGCF_ALLCLASSES;
	
	if (PresentOnly) {
		Flags |= DIGCF_PRESENT;
	}
	
    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(
		NULL, // All Classes
        0,
        hwndParent, 
        Flags); // All devices present on system

    if (INVALID_HANDLE_VALUE == DeviceInfoSet) {
		goto cleanup;
    }
    
	if (NULL != pbRebootRequired) {
		*pbRebootRequired = FALSE;
	}

	//
    //  Enumerate through all Devices.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	if (NULL != pdwRemovedDeviceCount) {
		*pdwRemovedDeviceCount = 0;
	}

	for (DWORD i = 0;
		SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData);
		++i)
    {
        DWORD DataT;
        LPTSTR buffer = NULL;
        DWORD buffersize = 0;
        
        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the nessicary space.
        // Keep calling we have success or an unknown failure.
        //
        while (TRUE) {

			fSuccess = SetupDiGetDeviceRegistryProperty(
				DeviceInfoSet,
				&DeviceInfoData,
				SPDRP_HARDWAREID,
				&DataT,
				(PBYTE)buffer,
				buffersize,
				&buffersize);
			
			if (fSuccess) {
				break;
			}

            if (ERROR_INVALID_DATA == GetLastError()) {
                //
                // May be a Legacy Device with no HardwareID. Continue.
                //
                break;
            } else if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
                //
                // We need to change the buffer size.
                //
                if (buffer) {
                    LocalFree(buffer);
				}
                buffer = (LPTSTR) LocalAlloc(LPTR,buffersize);

			} else {
                //
                // Unknown Failure.
                //
				if (ErrorCallbackProc) {
					BOOL fCont = ErrorCallbackProc(
						NDIRD_ERROR_GetDeviceRegistryProperty,
						DeviceInfoSet,
						&DeviceInfoData,
						GetLastError(),
						Context);
					if (fCont) {
					}
					fReturn = FALSE;
					goto cleanup;
				}
            }            
        }

		//
		// May be a Legacy Device with no HardwareID. Continue.
		//
        if (GetLastError() == ERROR_INVALID_DATA) {
            continue;
		}
        
        //
        // Compare each entry in the buffer multi-sz list with our HardwareID.
        //
        for (LPTSTR p = buffer; 
			*p && (p < & buffer[buffersize]);
			p += lstrlen(p) + 1) 
		{

			XTLTRACE1(TRACE_LEVEL_VERBOSE, _T("%s\n"), p);

			if (0 == lstrcmpi(szHardwareId,p)) 
			{
				XTLTRACE1(TRACE_LEVEL_VERBOSE, _T("\n>>> %s <<<\n"), p);

/*				XTLTRACE1(TRACE_LEVEL_INFORMATION, _T("Deleting related registry key.\n"));
				fSuccess = SetupDiDeleteDevRegKey(
					DeviceInfoSet, 
					&DeviceInfoData, 
					DICS_FLAG_GLOBAL, 
					0xFFFFFFFF, 
					DIREG_DRV);
				if (!fSuccess) {
					DWORD err = GetLastError();
					DPErrorEx(_T("SetupDiDeleteDevRegKey failed: 0x%08X"), err);
				}
*/
				//
                // Worker function to remove device.
                //
				 fSuccess = SetupDiCallClassInstaller(
					DIF_REMOVE, 
					DeviceInfoSet, 
					&DeviceInfoData);

				if (!fSuccess && ErrorCallbackProc) {
					BOOL fCont = ErrorCallbackProc(
						NDIRD_ERROR_CallRemoveToClassInstaller,
						DeviceInfoSet, 
						&DeviceInfoData, 
						GetLastError(), 
						Context);

					if (buffer) {
						LocalFree(buffer);
					}
					fReturn = FALSE;
					goto cleanup;
				}

				//
				// Get the installation param
				//
				if (NULL != pbRebootRequired) {

					SP_DEVINSTALL_PARAMS diParams = {0};
					diParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

					fSuccess = SetupDiGetDeviceInstallParams(
						DeviceInfoSet,
						&DeviceInfoData,
						&diParams);

					_ASSERTE(fSuccess);

					*pbRebootRequired |= 
						(diParams.Flags & DI_NEEDREBOOT) ||
						(diParams.Flags & DI_NEEDRESTART);

				}

				if (NULL != pdwRemovedDeviceCount) {
					++(*pdwRemovedDeviceCount);
				}

                break;
            }
        }

        if (buffer) {
			LocalFree(buffer);
		}
    }

    if ((NO_ERROR != GetLastError()) && 
		(ERROR_NO_MORE_ITEMS != GetLastError())) 
	{
		fReturn = FALSE;
	} else {
		fReturn = TRUE;
	}
    
    //
    //  Cleanup.
    //    
cleanup:

    DWORD err = GetLastError();

	if (INVALID_HANDLE_VALUE != DeviceInfoSet) {
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}
	
	SetLastError(err);

	return fReturn;
}

NDASDI_API BOOL WINAPI 
NdasDiRemoveLegacyDevice(
	IN HWND hwndParent,
	IN LPCTSTR ServiceName,
	IN BOOL PresentOnly,
	OUT LPDWORD pdwRemovedDeviceCount OPTIONAL,
	OUT LPBOOL pbRebootRequired OPTIONAL,
	IN NDASDIREMOVEDEVICEERRCALLBACKPROC ErrorCallbackProc OPTIONAL,
	IN LPVOID Context OPTIONAL)
{
	_ASSERTE(!IsBadStringPtr(ServiceName, LINE_LEN));
	_ASSERTE(NULL == pbRebootRequired || !IsBadWritePtr(pbRebootRequired, sizeof(BOOL)));
	_ASSERTE(NULL == ErrorCallbackProc || !IsBadCodePtr((FARPROC)ErrorCallbackProc));

	BOOL fReturn = FALSE;
	BOOL fSuccess = FALSE;

    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData = {0};
	DWORD Flags = DIGCF_ALLCLASSES;
	
	if (PresentOnly) {
		Flags |= DIGCF_PRESENT;
	}
	
    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(
		NULL, // All Classes
        0,
        hwndParent, 
        Flags); // All devices present on system

    if (INVALID_HANDLE_VALUE == DeviceInfoSet) {
		goto cleanup;
    }
    
	if (NULL != pbRebootRequired) {
		*pbRebootRequired = FALSE;
	}

	//
    //  Enumerate through all Devices.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	if (NULL != pdwRemovedDeviceCount) {
		*pdwRemovedDeviceCount = 0;
	}

	for (DWORD i = 0;
		SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData);
		++i)
    {
        DWORD DataT;
        LPTSTR buffer = NULL;
        DWORD buffersize = 0;
        
        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the nessicary space.
        // Keep calling we have success or an unknown failure.
        //
        while (TRUE) {

			fSuccess = SetupDiGetDeviceRegistryProperty(
				DeviceInfoSet,
				&DeviceInfoData,
				SPDRP_SERVICE,
				&DataT,
				(PBYTE)buffer,
				buffersize,
				&buffersize);
			
			if (fSuccess) {
				break;
			}

            if (ERROR_INVALID_DATA == GetLastError()) {
                //
                // May be a Legacy Device with no HardwareID. Continue.
                //
                break;
            } else if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
                //
                // We need to change the buffer size.
                //
                if (buffer) {
                    LocalFree(buffer);
				}
                buffer = (LPTSTR) LocalAlloc(LPTR,buffersize);

			} else {
                //
                // Unknown Failure.
                //
				if (ErrorCallbackProc) {
					BOOL fCont = ErrorCallbackProc(
						NDIRD_ERROR_GetDeviceRegistryProperty,
						DeviceInfoSet,
						&DeviceInfoData,
						GetLastError(),
						Context);
					if (fCont) {
					}
					fReturn = FALSE;
					goto cleanup;
				}
            }            
        }

		//
		// May be a Legacy Device with no HardwareID. Continue.
		//
        if (GetLastError() == ERROR_INVALID_DATA) {
            continue;
		}
        
        //
        // Service Name is REG_SZ (Not MultiSZ)
        //
		LPCTSTR p = buffer;
		
		XTLTRACE1(TRACE_LEVEL_VERBOSE, _T("%s\n"), p);

		if (0 == lstrcmpi(ServiceName,p)) {

			XTLTRACE1(TRACE_LEVEL_VERBOSE, _T("\n>>> %s <<<\n"), p);

			//
            // Worker function to remove device.
            //
				fSuccess = SetupDiCallClassInstaller(
				DIF_REMOVE, 
				DeviceInfoSet, 
				&DeviceInfoData);

			if (!fSuccess && ErrorCallbackProc) {
				BOOL fCont = ErrorCallbackProc(
					NDIRD_ERROR_CallRemoveToClassInstaller,
					DeviceInfoSet, 
					&DeviceInfoData, 
					GetLastError(), 
					Context);

				if (buffer) {
					LocalFree(buffer);
				}
				fReturn = FALSE;
				goto cleanup;
			}

			//
			// Get the installation param
			//
			if (NULL != pbRebootRequired) {

				SP_DEVINSTALL_PARAMS diParams = {0};
				diParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

				fSuccess = SetupDiGetDeviceInstallParams(
					DeviceInfoSet,
					&DeviceInfoData,
					&diParams);

				_ASSERTE(fSuccess);

				*pbRebootRequired |= 
					(diParams.Flags & DI_NEEDREBOOT) ||
					(diParams.Flags & DI_NEEDRESTART);

			}

			if (NULL != pdwRemovedDeviceCount) {
				++(*pdwRemovedDeviceCount);
			}

            break;

		}

        if (buffer) {
			LocalFree(buffer);
		}
    }

    if ((NO_ERROR != GetLastError()) && 
		(ERROR_NO_MORE_ITEMS != GetLastError())) 
	{
		fReturn = FALSE;
	} else {
		fReturn = TRUE;
	}
    
    //
    //  Cleanup.
    //    
cleanup:

    DWORD err = GetLastError();

	if (INVALID_HANDLE_VALUE != DeviceInfoSet) {
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}
	
	SetLastError(err);

	return fReturn;
}

NDASDI_API BOOL WINAPI 
NdasDiCopyOEMInf(
	IN LPCTSTR szInfPath,
	IN DWORD CopyStyle,
	OUT LPTSTR lpszOemInfPath OPTIONAL,
	IN DWORD cchOemInfPath OPTIONAL,
	OUT LPDWORD pchOemInfPathUsed OPTIONAL,
	OUT LPTSTR* lplpszOemInfFilePart OPTIONAL)
{
	BOOL fReturn = FALSE;
	BOOL fSuccess = FALSE;
	size_t	cchPathLen = 0;
	DWORD cchFullPath;
	
	LPTSTR lpszFullInfPath = ResolveFullPath(
		szInfPath, 
		&cchFullPath,
		NULL,
		RFP_NO_PATH_PREFIX);

	if (NULL == lpszFullInfPath) {
		goto cleanup;
	}

    fSuccess = SetupCopyOEMInf(
		lpszFullInfPath,
		NULL,               // other files are in the
							// same dir. as primary INF
		SPOST_PATH,         // first param. contains path to INF
		CopyStyle,          // ?? default copy style
		lpszOemInfPath,		// receives the name of the INF
							// after it is copied to %windir%\inf
		cchOemInfPath,          // max buf. size for the above
		pchOemInfPathUsed,      // receives required size if non-null
		lplpszOemInfFilePart);	// optionally retrieves filename
								// component of szInfNameAfterCopy

	if (!fSuccess) {
		goto cleanup;
	}

	fReturn = TRUE;
	
cleanup:

	DWORD err = GetLastError();

	if (lpszFullInfPath) {
		LocalFree(lpszFullInfPath);
	}
	
	SetLastError(err);
	
	return fReturn;
}

BOOL
pUpdateDriverForPlugAndPlayDevices(
	HWND hWndParent,
	LPCTSTR HardwareId,
	LPCTSTR FullInfPath,
	DWORD InstallFlags,
	PBOOL bRebootRequired OPTIONAL)
{
#ifdef UNICODE
	static LPCSTR procName = "UpdateDriverForPlugAndPlayDevicesW";
#else
	static LPCSTR procName = "UpdateDriverForPlugAndPlayDevicesA";
#endif
	typedef BOOL (WINAPI *UpdateDriverForPlugAndPlayDevicesProc)(
		HWND hwndParent,
		LPCTSTR HardwareId,
		LPCTSTR FullInfPath,
		DWORD InstallFlags,
		PBOOL bRebootRequired OPTIONAL
		);

	HMODULE hModule = ::LoadLibrary(_T("newdev.dll"));
	if (NULL == hModule)
	{
		return FALSE;
	}

	UpdateDriverForPlugAndPlayDevicesProc pfn = 
		(UpdateDriverForPlugAndPlayDevicesProc) ::GetProcAddress(hModule, procName);
	if (NULL == pfn)
	{
		DWORD se = ::GetLastError();
		::FreeLibrary(hModule);
		::SetLastError(se);
		return FALSE;
	}

	BOOL fSuccess = pfn(hWndParent, HardwareId, FullInfPath, InstallFlags, bRebootRequired);

	DWORD se = ::GetLastError();
	::FreeLibrary(hModule);
	::SetLastError(se);

	return fSuccess;
}
