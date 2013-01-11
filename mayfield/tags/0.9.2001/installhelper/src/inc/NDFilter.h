#ifndef NETDISK_FILTINSTALL_LIB_H
#define NETDISK_FILTINSTALL_LIB_H

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>
#include <winioctl.h>
#define STRSAFE_LIB
#include <strsafe.h>

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
//	@hootch@
//
//	Read Only Filter
//

// Variables/definitions for the driver that performs the actual monitoring.
#define	ROFILT_SYS_FILE			_T("ROFilt.sys")
#define	ROFILT_SYS_NAME			_T("ROFilt")

//
//	load device driver dynamically
//
BOOL __stdcall LoadDeviceDriver( const TCHAR * Name, const TCHAR * Path, HANDLE * lphDevice, PDWORD Error );
//
// Starts the driver service.
//
BOOL __stdcall StartDriver(IN LPCTSTR DriverName ) ;
BOOL __stdcall StopDriver(IN LPCTSTR DriverName ) ;

//
//	unload device driver dynamically
//
BOOL __stdcall UnloadDeviceDriver( const TCHAR * Name ) ;

//
// NDS API
//
int __stdcall LoadAndStartROFilter(TCHAR *DrvFilePath) ;
int __stdcall UnloadROFilter() ;


//
//	NDS API added by hootch 12192003
//
int __stdcall InstallNonPnPDriver(
		IN LPCTSTR	DrvFilePath,
		IN LPCTSTR	SysName,
		IN LPCTSTR	FileName,
		IN LPCTSTR	DisplayName,
		IN DWORD	StartType,
		IN DWORD	ErrorControl,
		IN LPCTSTR	LoadOrderGroup,
		IN LPCTSTR	Dependencies
	) ;

int __stdcall StopNonPnPDriver(
		IN LPCTSTR	SysName
	) ;

int __stdcall UninstallNonPnPDriver(
		IN LPCTSTR	SysName,
		IN LPCTSTR	FileName
	) ;

#ifdef __cplusplus
};
#endif

#endif
