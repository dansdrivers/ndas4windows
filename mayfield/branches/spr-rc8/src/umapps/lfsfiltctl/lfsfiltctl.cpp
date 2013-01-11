/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

Initial implementation:

2004-8-15 Chesong Lee <cslee@ximeta.com>

Revisons:

--*/

#define STRICT
#include <windows.h>
#include <tchar.h>
#include <winioctl.h>
#include <strsafe.h>
#include <crtdbg.h>

#include "lfsfiltctl.h"

#include "lfsfilterpublic.h"
#include "autores.h"

#define XDBG_LIBRARY_MODULE XDF_LFSFILTCTL
#include "xdebug.h"

static LPCTSTR LFSFILT_DEVICE_NAME = _T("\\\\.\\Global\\lfsfilt");
static LPCTSTR LFSFILT_DRIVER_NAME = _T("lfsfilt.sys");
static LPCTSTR LFSFILT_SERVICE_NAME = _T("lfsfilt");

/*++

LfsFiltCtlStartService with SCM handle

--*/
BOOL WINAPI LfsFiltCtlStartService(SC_HANDLE hSCManager)
{
	AutoSCHandle hService = ::OpenService(
		hSCManager,
		LFSFILT_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (NULL == (SC_HANDLE) hService) {
		DPErrorEx(_FT("Opening a service (%s) failed: "), LFSFILT_SERVICE_NAME);
		return FALSE;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess) {
		DPErrorEx(_FT("Starting a service (%s) failed: "), LFSFILT_SERVICE_NAME);
		return FALSE;
	}

	DPInfo(_FT("LFS Filter service started successfully.\n"));
	return TRUE;
}

/*++

LfsFiltCtlQueryServiceStatus without SCM handle

--*/
BOOL WINAPI LfsFiltCtlStartService()
{
	AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (NULL == (SC_HANDLE) hSCManager) {
		DPErrorEx(_FT("Failed to open SCM Database.\n"));
		return FALSE;
	}

	return LfsFiltCtlStartService(hSCManager);
}

/*++

LfsFiltCtlQueryServiceStatus with SCM handle

--*/
BOOL WINAPI LfsFiltCtlQueryServiceStatus(
	SC_HANDLE hSCManager,
	LPSERVICE_STATUS lpServiceStatus)
{
	AutoSCHandle hService = ::OpenService(
		hSCManager,
		LFSFILT_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (NULL == (SC_HANDLE) hService) {
		DPErrorEx(_FT("Opening Service (%s) failed: "), LFSFILT_SERVICE_NAME);
		return FALSE;
	}

	BOOL fSuccess = ::QueryServiceStatus(hService, lpServiceStatus);
	if (!fSuccess) {
		DPErrorEx(_FT("Querying Service Status (%s) failed: "), LFSFILT_SERVICE_NAME);
		return FALSE;
	}

	DPInfo(_FT("QueryServiceStatus succeeded.\n"));
	return TRUE;
}


/*++

NdasRoFilterQueryServiceStatus without SCM handle

--*/
BOOL WINAPI LfsFiltCtlQueryServiceStatus(
	LPSERVICE_STATUS lpServiceStatus)
{
	AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (NULL == (SC_HANDLE) hSCManager) {
		DPErrorEx(_FT("Opening SCM Database failed: "));
		return FALSE;
	}

	return LfsFiltCtlQueryServiceStatus(hSCManager, lpServiceStatus);
}

/*++

LfsFiltCtlOpenDevice

Create a handle for LFSFilt device object,
If failed, returns INVALID_HANDLE_VALUE

--*/
HANDLE WINAPI LfsFiltCtlOpenDevice()
{
	//
	// Returning handle must not be AutoFileHandle
	//
	HANDLE hDevice = ::CreateFile(
		LFSFILT_DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == hDevice) {
		DPErrorEx(_FT("Opening %s failed: "), LFSFILT_DEVICE_NAME);
		return INVALID_HANDLE_VALUE;
	}

	return hDevice;
}

/*++

Get the version information of LFS Filter
This IOCTL supports since Rev 1853

--*/

BOOL WINAPI LfsFiltCtlGetVersion(
	HANDLE hFilter,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor)
{
	DWORD cbReturned = 0;
	LFSFILT_VER verInfo = {0};

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_GETVERSION,
		NULL, 0,
		&verInfo, sizeof(verInfo), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("Getting LfsFilter Version failed: "));
		return FALSE;
	}

	if (lpwVerMajor) *lpwVerMajor = verInfo.VersionMajor;
	if (lpwVerMinor) *lpwVerMinor = verInfo.VersionMinor;
	if (lpwVerBuild) *lpwVerBuild = verInfo.VersionBuild;
	if (lpwVerPrivate) *lpwVerPrivate = verInfo.VersionPrivate;
	if (lpwNDFSVerMajor) *lpwNDFSVerMajor = verInfo.VersionNDFSMajor;
	if (lpwNDFSVerMinor) *lpwNDFSVerMinor = verInfo.VersionNDFSMinor;

	DPInfo(_FT("LfsFilter version is %d.%d.%d.%d, NDFS %d.%d\n"), 
		verInfo.VersionMajor, verInfo.VersionMinor,
		verInfo.VersionBuild, verInfo.VersionPrivate,
		verInfo.VersionNDFSMajor, verInfo.VersionNDFSMinor);

	return TRUE;
}

BOOL WINAPI LfsFiltCtlGetVersion(
	LPWORD lpwVerMajor, 
	LPWORD lpwVerMinor,
	LPWORD lpwVerBuild,
	LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,
	LPWORD lpwNDFSVerMinor)
{
	AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (INVALID_HANDLE_VALUE == hDevice) {
		DPErrorEx(_FT("Opening %s failed: "), LFSFILT_DEVICE_NAME);
		return FALSE;
	}

	return LfsFiltCtlGetVersion(
		hDevice, 
		lpwVerMajor, lpwVerMinor,
		lpwVerBuild, lpwVerPrivate,
		lpwNDFSVerMajor, lpwNDFSVerMinor);
}
/*++

Shutdown LfsFilter

Explicit call to shut down LfsFilter when shutting down the system.

--*/
BOOL WINAPI LfsFiltCtlShutdown(HANDLE hFilter)
{
	DWORD cbReturned = 0;
	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_SHUTDOWN,
		NULL, 0,
		NULL, 0, &cbReturned,
		(LPOVERLAPPED) NULL);
	
	if (!fSuccess) {
		DPErrorEx(_FT("Shutting down LfsFilter failed: "));
		return FALSE;
	}

	DPInfo(_FT("LfsFilter is shut down successfully.\n"));
	return TRUE;
}

/*++

Shutdown LfsFilter without a device file handle

--*/
BOOL WINAPI LfsFiltCtlShutdown()
{
	AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (INVALID_HANDLE_VALUE == hDevice) {
		DPErrorEx(_FT("Opening %s failed: "), LFSFILT_DEVICE_NAME);
		return FALSE;
	}

	return LfsFiltCtlShutdown(hDevice);
}
