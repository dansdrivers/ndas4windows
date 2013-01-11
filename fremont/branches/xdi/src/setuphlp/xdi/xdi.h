#ifndef _NDASDI_H_
#define _NDASDI_H_
#pragma once

#ifndef _INC_SETUPAPI
#error setupapi.h is required to include this header
#endif

#ifdef __cplusplus
extern "C" {
#endif
	
typedef enum _XDI_RESULT {
	/* NETCFG_S_REBOOT */
	XDI_S_REBOOT = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_ITF, 0xA020),
	/* NETCFG_E_NEED_REBOOT */
	XDI_E_NEED_REBOOT = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA025)
} XDI_RESULT;

#ifndef INSTALLFLAG_FORCE
#define INSTALLFLAG_FORCE           0x00000001      // Force the installation of the specified driver
#define INSTALLFLAG_READONLY        0x00000002      // Do a read-only install (no file copy)
#define INSTALLFLAG_NONINTERACTIVE  0x00000004      // No UI shown at all. API will fail if any UI must be shown.
#define INSTALLFLAG_BITS            0x00000007
#endif

HRESULT
WINAPI
xDiInstallFromInfSection(
	__in_opt HWND Owner,
	__in LPCWSTR InfFullPath,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService);

HRESULT
WINAPI
xDiInstallFromInfSectionEx(
	__in_opt HWND Owner,
	__in HINF InfHandle,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService);

HRESULT
WINAPI
xDiInstallNetComponent(
	__in LPCGUID ClassGuid,
	__in LPCWSTR ComponentId,
	__in DWORD LockTimeout,
	__in LPCWSTR ClientDescription,
	__deref_out_opt LPWSTR* LockingClientDescription);

HRESULT
WINAPI
xDiUninstallNetComponent(
	__in LPCWSTR ComponentId,
	__in DWORD LockTimeout,
	__in LPCWSTR ClientDescription,
	__deref_out_opt LPWSTR* LockingClientDescription,
	__deref_out_opt LPWSTR* OEMInfName);

HRESULT
WINAPI
xDiFindNetComponent(
	__in LPCWSTR ComponentId,
	__deref_out_opt LPWSTR* OemInfName);

HRESULT
WINAPI
xDiInstallLegacyPnpDevice(
	__in_opt HWND Owner,
	__in LPCWSTR HardwareId,
	__in LPCWSTR InfFullPath,
	__in_opt LPCWSTR DeviceName,
	__in DWORD InstallFlags);

HRESULT
WINAPI
xDiUpdateDeviceDriver(
	__in_opt HWND Owner,
	__in LPCWSTR HardwareId,
	__in LPCWSTR InfFullPath,
	__in DWORD InstallFlags);

HRESULT
WINAPI
xDiFindPnpDevice(
	__in_opt HWND Owner,
	__in_opt LPCGUID ClassGuid,
	__in_opt LPCWSTR Enumerator, 
	__in LPCWSTR HardwareId);

typedef enum _XDI_ENUM_FLAGS {
	XDI_ENUM_FALGS_PRESENT_ONLY = 1
} XDI_ENUM_FLAGS;

typedef enum _XDI_CALLBACK_TYPE {
	XDI_CALLBACK_TYPE_SUCCESS = 1,
	XDI_CALLBACK_TYPE_ERROR_REGISTRY = 2,
	XDI_CALLBACK_TYPE_ERROR_CALL_INSTALLER = 3
} XDI_CALLBACK_TYPE;

typedef BOOL (CALLBACK* XDI_ENUM_CALLBACK)(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in LPVOID Context);

//
// Returns S_OK on success.
// Otherwise E_XXX is returned
//
// For EnumFlags, see SetupDiGetClassDevsEx
// DIGCF_ALLCLASSES, DIGCF_DEVICEINTERFACE, DIGCF_DEFAULT, 
// DIGCF_PRESENT, DIGCF_PROFILE
//
HRESULT
WINAPI
xDiEnumDevices(
	__in HWND Owner,
	__in_opt LPCGUID ClassGuid,
	__in_opt LPCWSTR Enumerator,
	__in DWORD EnumFlags,
	__in XDI_ENUM_CALLBACK EnumCallback,
	__in_opt PVOID EnumCallbackContext);

//
// Returns zero (S_OK) if successful, and returns XDI_S_REBOOT 
// if the applied changes required a system reboot.
// E_OUTOFMEMORY is returned when memory allocation failed.
//
// For EnumFlags, see SetupDiGetClassDevsEx
// DIGCF_ALLCLASSES, DIGCF_DEVICEINTERFACE, DIGCF_DEFAULT, 
// DIGCF_PRESENT, DIGCF_PROFILE
//
HRESULT
WINAPI
xDiRemoveDevices(
	__in_opt HWND Owner,
	__in_opt LPCGUID ClassGuid,
	__in_opt LPCWSTR Enumerator,
	__in LPCWSTR HardwareId,
	__in DWORD EnumFlags,
	__out_opt LPWSTR* ServiceList,
	__out_opt LPWSTR* InfNameList);

HRESULT
WINAPI
xDiDeletePnpDriverServices(
	__in LPCWSTR ServiceList);

typedef BOOL (CALLBACK* XDI_REMOVE_INF_CALLBACK)(
	__in DWORD Error, 
	__in LPCWSTR InfPath, 
	__in LPCWSTR FileName, 
	__in LPVOID CallbackContext);

HRESULT 
WINAPI
xDiUninstallHardwareOEMInf(
	__in_opt LPCWSTR FileFindSpec, /* if zero, oem*.inf */
	__in LPCWSTR HardwareIdList,   /* MULTI_SZ, should be terminated with two nulls */
	__in DWORD Flags,
	__in_opt XDI_REMOVE_INF_CALLBACK Callback,
	__in_opt LPVOID CallbackContext);

#ifndef SUOI_FORCEDELETE
#define SUOI_FORCEDELETE   0x00000001
#endif

HRESULT
WINAPI
xDiUninstallOEMInf(
	__in LPCWSTR InfFileName,
	__in DWORD Flags);

typedef enum _XDI_ENUM_DRIVER_FILES_FLAGS { 
	XDI_EDF_NO_CLASS_INSTALLER = 0x1 
} XDI_ENUM_DRIVER_FILES_FLAGS;

typedef BOOL (CALLBACK* XDI_ENUM_DRIVER_FILE_CALLBACK)(
	__in LPCWSTR TargetFilePath,
	__in DWORD Flags,
	__in LPVOID Context);

HRESULT
WINAPI
xDiEnumDriverFiles(
	__in_opt HWND Owner,
	__in LPCWSTR OemInfFullPath,
	__in DWORD Flags,
	__in XDI_ENUM_DRIVER_FILE_CALLBACK EnumCallback,
	__in LPVOID EnumContext);

HRESULT
WINAPI
xDiDeleteDriverFiles(
	__in_opt HWND Owner,
	__in LPCWSTR InfPath,
	__in DWORD Flags);

//
// S_OK if the service is marked for deletion
// S_FALSE if it is not marked for deletion
// E_XXX for other errors
//
HRESULT
WINAPI
xDiServiceIsMarkedForDeletion(
	__in LPCWSTR ServiceName);

//
// S_OK if any services to install or to delete from INF section is marked for deletion
// S_FALSE if it is not marked for deletion
// E_XXX for other errors
//
// Use CoTaskMemFree to free ServiceName if not null
//

HRESULT
WINAPI
xDiServiceIsMarkedForDeletionFromInfSection(
	__in LPCWSTR InfFile,
	__in LPCWSTR InfSection,
	__deref_out_opt LPWSTR* ServiceName);

HRESULT
WINAPI
xDiServiceIsMarkedForDeletionFromInfSectionEx(
	__in HINF InfHandle,
	__in LPCWSTR InfSection,
	__deref_out_opt LPWSTR* ServiceName);

HRESULT
WINAPI
xDiServiceIsMarkedForDeletionFromPnpInf(
	__in LPCWSTR InfFile,
	__in LPCWSTR HardwareId,
	__deref_out_opt LPWSTR* ServiceName);

HRESULT
WINAPI
xDiStartService(
	__in SC_HANDLE SCManagerHandle,
	__in SC_HANDLE ServiceHandle,
	__in DWORD Timeout);

HRESULT
WINAPI
xDiStopService(
	__in SC_HANDLE SCManagerHandle,
	__in SC_HANDLE ServiceHandle,
	__in BOOL StopDependencies,
	__in DWORD Timeout);

//
// Use xDiUninstallOEMInf instead
//

BOOL 
WINAPI
Setupapi_SetupUninstallOEMInfW(
	__in LPCWSTR InfFileName,
	__in DWORD Flags,
	__reserved PVOID Reserved);

BOOL
WINAPI
Newdev_UpdateDriverForPlugAndPlayDevicesW(
	__in HWND hwndParent,
	__in LPCWSTR HardwareId,
	__in LPCWSTR FullInfPath,
	__in DWORD InstallFlags,
	__in_opt PBOOL bRebootRequired);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _NDASDI_H_ */
