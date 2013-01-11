#ifndef _NDASDI_DEVINST_H_
#define _NDASDI_DEVINST_H_

#pragma once

#ifdef NDASDI_DLL_EXPORTS
#define NDASDI_API __declspec(dllexport)
#elif NDASDI_DLL_IMPORTS
#define NDASDI_API __declspec(dllimport)
#else
#define NDASDI_API		
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* InstallFlags */
#ifndef _INC_NEWDEV
#define INSTALLFLAG_FORCE           0x00000001      // Force the installation of the specified driver
/* The following sets are available for Windows XP or later only */
#define INSTALLFLAG_READONLY        0x00000002      // Do a read-only install (no file copy)
#define INSTALLFLAG_NONINTERACTIVE  0x00000004      // No UI shown at all. API will fail if any UI must be shown.
#endif

/*

Possible error values returned by GetLastError are:

ERROR_NO_MORE_ITEMS

*/

NDASDI_API BOOL WINAPI
NdasDiFindExistingDevice(
	IN HWND hwndParent,
	IN LPCTSTR HardwareId,
	IN BOOL PresentOnly);


/*

Possible error values returned by GetLastError are:

ERROR_FILE_NOT_FOUND
ERROR_IN_WOW64
ERROR_INVALID_FLAGS
ERROR_NO_SUCH_DEVINST 0xE000020B
NO_ERROR

*/

/*

The function returns TRUE if a device was upgraded to the specified driver. 

Otherwise, it returns FALSE and the logged error can be retrieved with a 
call to GetLastError. Possible error values returned by GetLastError are 
included in the following table.

Possible error values returned by GetLastError are:

ERROR_FILE_NOT_FOUND 
The path specified for FullInfPath does not exist. 
ERROR_IN_WOW64 
The calling application is a 32-bit application attempting to execute 
in a 64-bit environment, which is not allowed. 
ERROR_INVALID_FLAGS 
The value specified for InstallFlags is invalid. 
ERROR_NO_SUCH_DEVINST 
The value specified for HardwareId does not match any device on the 
system. That is, the device is not plugged in. 
NO_ERROR 
The routine found a match for the HardwareId value, but the specified 
driver was not better than the current driver and the caller did not 
specify the INSTALLFLAG_FORCE flag. 

*/

NDASDI_API BOOL WINAPI 
NdasDiInstallRootEnumeratedDevice(
	IN HWND hwndParent,
	IN LPCTSTR szInfPath, 
	IN LPCTSTR szHardwareId,
	IN DWORD InstallFlags,
	OUT LPBOOL pbRebootRequired OPTIONAL);

/*

The function returns TRUE if a device was upgraded to the specified driver. 

Otherwise, it returns FALSE and the logged error can be retrieved with a 
call to GetLastError. Possible error values returned by GetLastError are 
included in the following table.

Possible error values returned by GetLastError are:

ERROR_FILE_NOT_FOUND 
The path specified for FullInfPath does not exist. 
ERROR_IN_WOW64 
The calling application is a 32-bit application attempting to execute 
in a 64-bit environment, which is not allowed. 
ERROR_INVALID_FLAGS 
The value specified for InstallFlags is invalid. 
ERROR_NO_SUCH_DEVINST 
The value specified for HardwareId does not match any device on the 
system. That is, the device is not plugged in. 
NO_ERROR 
The routine found a match for the HardwareId value, but the specified 
driver was not better than the current driver and the caller did not 
specify the INSTALLFLAG_FORCE flag. 

*/

NDASDI_API BOOL WINAPI 
NdasDiUpdateDeviceDriver(
	IN HWND hwnd,
	IN LPCTSTR szInfPath, 
	IN LPCTSTR szHardwareId,
	IN DWORD InstallFlags,
	OUT PBOOL pbRebootRequired OPTIONAL);

typedef enum _NDASDI_REMOVE_DEVICE_ERROR_TYPE {
	NDIRD_ERROR_GetDeviceRegistryProperty,
	NDIRD_ERROR_CallRemoveToClassInstaller
} NDASDI_REMOVE_DEVICE_ERROR_TYPE;

//#ifdef _INC_SETUPAPI
//
// You may not need this callback function prototype.
// When you need, include <setupapi.h> 
// prior to include this file.
//
typedef BOOL (CALLBACK* NDASDIREMOVEDEVICEERRCALLBACKPROC)(
	NDASDI_REMOVE_DEVICE_ERROR_TYPE Type,
	HDEVINFO DeviceInfoSet,
	PSP_DEVINFO_DATA DeviceInfoData,
	DWORD Error,
	LPVOID Context);

//#endif // _INC_SETUPAPI

NDASDI_API BOOL WINAPI 
NdasDiRemoveDevice(
	IN HWND hwndParent,
	IN LPCTSTR szHardwareId, 
	IN BOOL PresentOnly,
	OUT LPDWORD pdwRemovedDeviceCount OPTIONAL,
	OUT LPBOOL pbRebootRequired OPTIONAL,
	IN NDASDIREMOVEDEVICEERRCALLBACKPROC ErrorCallbackProc OPTIONAL,
	IN LPVOID Context OPTIONAL);

NDASDI_API BOOL WINAPI 
NdasDiRemoveLegacyDevice(
	IN HWND hwndParent,
	IN LPCTSTR ServiceName,
	IN BOOL PresentOnly,
	OUT LPDWORD pdwRemovedDeviceCount OPTIONAL,
	OUT LPBOOL pbRebootRequired OPTIONAL,
	IN NDASDIREMOVEDEVICEERRCALLBACKPROC ErrorCallbackProc OPTIONAL,
	IN LPVOID Context OPTIONAL);

#ifndef _INC_SETUPAPI
//
// CopyStyle values for copy and queue-related APIs
//
#define SP_COPY_DELETESOURCE        0x0000001   // delete source file on successful copy
#define SP_COPY_REPLACEONLY         0x0000002   // copy only if target file already present
#define SP_COPY_NOOVERWRITE         0x0000008   // copy only if target doesn't exist
#define SP_COPY_OEMINF_CATALOG_ONLY 0x0040000   // (SetupCopyOEMInf only) don't copy INF--just catalog

#endif // _INC_SETUPAPI

/*

Possible error values returned by GetLastError are:

ERROR_NO_CATALOG_FOR_OEM_INF 0xE000020B 
(User clicks STOP Installation for digital signature warning)
ERROR_FILE_NOT_FOUND

*/

NDASDI_API BOOL WINAPI 
NdasDiCopyOEMInf(
	IN LPCTSTR szInfPath,
	IN DWORD CopyStyle,
	OUT LPTSTR lpszOemInfPath OPTIONAL,
	IN DWORD cchOemInfPath OPTIONAL,
	OUT LPDWORD pchOemInfPath OPTIONAL,
	OUT LPTSTR* lplpszOemInfFilePart OPTIONAL);

//
// This function fails during initialization
// from functions such as FindFirstFile, etc.
// Actual processing (deletion) error is reported
// through the callback function
//

typedef BOOL (CALLBACK* NDASDI_DELETE_OEM_INF_CALLBACK_PROC)(
	DWORD dwError, LPCTSTR szPath, LPCTSTR szFileName, LPVOID lpContext);

NDASDI_API
BOOL 
WINAPI
NdasDiDeleteOEMInf(
	IN LPCTSTR szFindSpec OPTIONAL, /* if null, oem*.inf */
	IN LPCTSTR multiszHardwareIDs, /* must be terminated with two-NULLs */
	IN NDASDI_DELETE_OEM_INF_CALLBACK_PROC pfnCallback OPTIONAL,
	IN LPVOID lpContext OPTIONAL);
	
#ifdef __cplusplus
}
#endif

#endif /* _NDASDI_DEVINST_H_ */
