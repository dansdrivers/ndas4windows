#pragma once

#if   defined NDASDI_DLL_EXPORTS
#define NDASDI_API __declspec(dllexport)
#elif defined NDASDI_DLL_IMPORTS
#define NDASDI_API __declspec(dllimport)
#else
#define NDASDI_API	
#endif

#ifdef __cplusplus
extern "C" {
#endif

NDASDI_API 
BOOL 
WINAPI
NdasDiServiceExistsSCH(
	SC_HANDLE schSCManager,
	LPCTSTR ServiceName);

NDASDI_API 
BOOL 
WINAPI
NdasDiInstallServiceSCH(
	SC_HANDLE schSCManager,
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR BinaryPathName,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies,
	LPCTSTR AccountName,
	LPCTSTR Password);

NDASDI_API 
BOOL 
WINAPI
NdasDiInstallService(
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR BinaryPathName,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies,
	LPCTSTR AccountName,
	LPCTSTR Password);

NDASDI_API 
BOOL 
WINAPI
NdasDiInstallOrUpdateDriverService(
	IN LPCTSTR SourceFilePath,
	IN LPCTSTR ServiceName,
	IN LPCTSTR DisplayName,
	IN LPCTSTR Description,
	IN DWORD ServiceType,
	IN DWORD StartType,
	IN DWORD ErrorControl,
	IN LPCTSTR LoadOrderGroup,
	OUT LPDWORD lpdwTagId,
	IN LPCTSTR Dependencies,
	OUT LPBOOL lpbUpdated);

NDASDI_API 
BOOL 
WINAPI
NdasDiStartServiceSCH(
	IN SC_HANDLE schSCManager,
	IN LPCTSTR ServiceName,
	IN DWORD argc,
	IN LPCTSTR* argv);

NDASDI_API 
BOOL 
WINAPI
NdasDiStartService(
	IN LPCTSTR ServiceName,
	IN DWORD argc,
	IN LPCTSTR* argv);

NDASDI_API 
BOOL 
WINAPI
NdasDiStopServiceSCH(
	IN SC_HANDLE schSCManager,
	IN LPCTSTR ServiceName);

NDASDI_API 
BOOL 
WINAPI
NdasDiStopService(
	IN LPCTSTR ServiceName);

NDASDI_API 
BOOL 
WINAPI 
NdasDiDeleteServiceSCH(
	IN SC_HANDLE schSCManager,
	IN LPCTSTR	ServiceName);

NDASDI_API 
BOOL 
WINAPI 
NdasDiDeleteService(
	IN LPCTSTR	ServiceName);

NDASDI_API 
BOOL 
WINAPI
NdasDiFindService(LPCTSTR ServiceName, LPBOOL pbPendingDeletion);

NDASDI_API 
BOOL 
WINAPI
NdasDiIsServiceMarkedForDeletion(
	IN LPCTSTR ServiceName);

NDASDI_API 
BOOL 
WINAPI
NdasDiUpdateServiceConfigSCH(
	SC_HANDLE schSCManager,
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR BinaryPathName,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies,
	LPCTSTR AccountName,
	LPCTSTR Password);

NDASDI_API
BOOL
WINAPI
NdasDiUpdateServiceConfig(
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR BinaryPathName,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies,
	LPCTSTR AccountName,
	LPCTSTR Password);

/* CopiedFilePath source contain a buffer more than MAX_PATH */
NDASDI_API
BOOL
WINAPI
NdasDiCopyDriverFileToSystem(
	LPCTSTR SourceFilePath,
	LPTSTR CopiedFilePath);

#ifdef __cplusplus
}
#endif
