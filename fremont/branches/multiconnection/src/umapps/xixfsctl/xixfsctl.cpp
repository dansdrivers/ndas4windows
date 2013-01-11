/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

2007-4-03 Chesong Lee <cslee@ximeta.com>

Initial implementation

--*/

#include <windows.h>
#include <tchar.h>
#include <winioctl.h>
#include <strsafe.h>
#include <crtdbg.h>

#include "xixfsctl.h"
#include "xixfscontrol.h"
#include <xtl/xtlautores.h>

#include "trace.h"
#ifdef RUN_WPP
#include "xixfsctl.tmh"
#endif

const LPCWSTR XIXFS_DEVICE_NAME = XIXFS_CONTROL_W32_DEVICE_NAME;
//const LPCWSTR XIXFS_SERVICE_NAME = XIXFS_SERVICE_NAME;

/*++

XixfsCtlStartService with SCM handle

--*/
BOOL 
WINAPI 
XixfsCtlStartService(SC_HANDLE hSCManager)
{
	XTL::AutoSCHandle hService = ::OpenService(
		hSCManager,
		XIXFS_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (hService.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening a service (%ls) failed, error=0x%X\n", 
			XIXFS_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Starting a service (%ls) failed, error=0x%X\n",
			XIXFS_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"%ls service started successfully.\n", XIXFS_SERVICE_NAME);
	
	return TRUE;
}

/*++

XixfsCtlQueryServiceStatus without SCM handle

--*/
BOOL
WINAPI 
XixfsCtlStartService()
{
	XTL::AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (hSCManager.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"OpenSCManager failed, error=0x%X\n", 
			GetLastError());
		return FALSE;
	}

	return XixfsCtlStartService(hSCManager);
}

/*++

XixfsCtlQueryServiceStatus with SCM handle

--*/
BOOL 
WINAPI 
XixfsCtlQueryServiceStatus(
	SC_HANDLE hSCManager,
	LPSERVICE_STATUS lpServiceStatus)
{
	XTL::AutoSCHandle hService = ::OpenService(
		hSCManager,
		XIXFS_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (hService.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"OpenService(%ls) failed, error=0x%X\n",
			XIXFS_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	BOOL fSuccess = ::QueryServiceStatus(hService, lpServiceStatus);
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"QueryServiceStatus(%ls) failed, error=0x%X\n",
			XIXFS_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"QueryServiceStatus(%ls) completed successfully\n",
		XIXFS_SERVICE_NAME);

	return TRUE;
}


/*++

NdasRoFilterQueryServiceStatus without SCM handle

--*/
BOOL 
WINAPI 
XixfsCtlQueryServiceStatus(
	LPSERVICE_STATUS lpServiceStatus)
{
	XTL::AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (hSCManager.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening SCM Database failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	return XixfsCtlQueryServiceStatus(hSCManager, lpServiceStatus);
}

/*++

XixfsCtlOpenDevice

Create a handle for LFSFilt device object,
If failed, returns INVALID_HANDLE_VALUE

--*/
HANDLE 
WINAPI 
XixfsCtlOpenDevice()
{
	//
	// Returning handle must not be AutoFileHandle
	//
	HANDLE hDevice = ::CreateFile(
		XIXFS_DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == hDevice) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			XIXFS_DEVICE_NAME,
			GetLastError());

		return INVALID_HANDLE_VALUE;
	}

	return hDevice;
}

/*++

Get the version information of LFS Filter
This IOCTL supports since Rev 1853

--*/

BOOL 
WINAPI 
XixfsCtlGetVersion(
	HANDLE hXixfsControlDevice,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor)
{
	DWORD cbReturned = 0;
	XIXFS_VER verInfo = {0};

	BOOL fSuccess = ::DeviceIoControl(
		hXixfsControlDevice,
		NDAS_XIXFS_VERSION,
		NULL, 0,
		&verInfo, sizeof(verInfo), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Getting Xixfs Version failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	if (lpwVerMajor) *lpwVerMajor = verInfo.VersionMajor;
	if (lpwVerMinor) *lpwVerMinor = verInfo.VersionMinor;
	if (lpwVerBuild) *lpwVerBuild = verInfo.VersionBuild;
	if (lpwVerPrivate) *lpwVerPrivate = verInfo.VersionPrivate;
	if (lpwNDFSVerMajor) *lpwNDFSVerMajor = verInfo.VersionNDFSMajor;
	if (lpwNDFSVerMinor) *lpwNDFSVerMinor = verInfo.VersionNDFSMinor;

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"Xixfs version is %d.%d.%d.%d, NDFS %d.%d\n",
		verInfo.VersionMajor, verInfo.VersionMinor,
		verInfo.VersionBuild, verInfo.VersionPrivate,
		verInfo.VersionNDFSMajor, verInfo.VersionNDFSMinor);

	return TRUE;
}

BOOL 
WINAPI 
XixfsCtlGetVersion(
	LPWORD lpwVerMajor, 
	LPWORD lpwVerMinor,
	LPWORD lpwVerBuild,
	LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,
	LPWORD lpwNDFSVerMinor)
{
	XTL::AutoFileHandle hDevice = XixfsCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			XIXFS_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return XixfsCtlGetVersion(
		hDevice, 
		lpwVerMajor, lpwVerMinor,
		lpwVerBuild, lpwVerPrivate,
		lpwNDFSVerMajor, lpwNDFSVerMinor);
}

/*++

Get the version information of LFS Filter
This IOCTL supports since Rev 1853

--*/

BOOL 
WINAPI 
XixfsCtlReadyToUnload(
	HANDLE hXixfsControlDevice)
{
	DWORD cbReturned = 0;
	XIXFS_VER verInfo = {0};

	BOOL fSuccess = ::DeviceIoControl(
		hXixfsControlDevice,
		NDAS_XIXFS_UNLOAD,
		NULL, 0,
		NULL, 0, &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Getting Xixfs preparing for unload failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"Xixfs version is prepared for unloading\n");

	return TRUE;
}

BOOL 
WINAPI 
XixfsCtlReadyForUnload(VOID)
{
	XTL::AutoFileHandle hDevice = XixfsCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			XIXFS_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return XixfsCtlReadyToUnload(
		hDevice);
}
