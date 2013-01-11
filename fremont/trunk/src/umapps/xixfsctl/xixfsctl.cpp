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

/*++

XixfsCtlStartService with SCM handle

--*/
HRESULT
XixfsCtlStartServiceByHandle(SC_HANDLE hSCManager)
{
	HRESULT hr;

	XTL::AutoSCHandle hService = ::OpenService(
		hSCManager,
		XIXFS_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (hService.IsInvalid()) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening a service (%ls) failed, hr=0x%X\n", 
			XIXFS_SERVICE_NAME, hr);
		return hr;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Starting a service (%ls) failed, hr=0x%X\n",
			XIXFS_SERVICE_NAME, hr);
		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"%ls service started successfully.\n", XIXFS_SERVICE_NAME);
	
	return S_OK;
}

/*++

XixfsCtlQueryServiceStatus without SCM handle

--*/
HRESULT
XixfsCtlStartService()
{
	HRESULT hr;

	XTL::AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (hSCManager.IsInvalid()) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"OpenSCManager failed, hr=0x%X\n", hr);
		return hr;
	}

	return XixfsCtlStartServiceByHandle(hSCManager);
}

/*++

XixfsCtlQueryServiceStatus with SCM handle

--*/
HRESULT
XixfsCtlQueryServiceStatusByHandle(
	SC_HANDLE hSCManager,
	LPSERVICE_STATUS lpServiceStatus)
{
	HRESULT hr;

	XTL::AutoSCHandle hService = ::OpenService(
		hSCManager,
		XIXFS_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (hService.IsInvalid()) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"OpenService(%ls) failed, hr=0x%X\n",
			XIXFS_SERVICE_NAME, hr);
		return hr;
	}

	BOOL fSuccess = ::QueryServiceStatus(hService, lpServiceStatus);
	if (!fSuccess) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"QueryServiceStatus(%ls) failed, error=0x%X\n",
			XIXFS_SERVICE_NAME, hr);
		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"QueryServiceStatus(%ls) completed successfully\n",
		XIXFS_SERVICE_NAME);

	return S_OK;
}


/*++

NdasRoFilterQueryServiceStatus without SCM handle

--*/
HRESULT
XixfsCtlQueryServiceStatus(
	LPSERVICE_STATUS lpServiceStatus)
{
	HRESULT hr;

	XTL::AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (hSCManager.IsInvalid()) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening SCM Database failed, hr=0x%X\n", hr);
		return hr;
	}

	return XixfsCtlQueryServiceStatusByHandle(hSCManager, lpServiceStatus);
}

/*++

XixfsCtlOpenDevice

Create a handle for LFSFilt device object,
If failed, returns INVALID_HANDLE_VALUE

--*/
HRESULT
XixfsCtlOpenControlDevice(
	__deref_out HANDLE* ControlDeviceHandle)
{
	HRESULT hr;

	XTLASSERT(NULL != ControlDeviceHandle);
	*ControlDeviceHandle = INVALID_HANDLE_VALUE;

	//
	// Returning handle must not be AutoFileHandle
	//
	HANDLE handle = ::CreateFile(
		XIXFS_DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == handle) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, hr=0x%X\n",
			XIXFS_DEVICE_NAME, hr);

		return hr;
	}

	*ControlDeviceHandle = handle;

	return S_OK;
}

/*++

Get the version information of LFS Filter
This IOCTL supports since Rev 1853

--*/

HRESULT
XixfsCtlGetVersionByHandle(
	HANDLE hXixfsControlDevice,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor)
{
	HRESULT hr;

	DWORD cbReturned = 0;
	XIXFS_VER verInfo = {0};

	BOOL success = ::DeviceIoControl(
		hXixfsControlDevice,
		NDAS_XIXFS_VERSION,
		NULL, 0,
		&verInfo, sizeof(verInfo), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Getting Xixfs Version failed, hr=0x%X\n", hr);
		return hr;
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

	return S_OK;
}

HRESULT
WINAPI 
XixfsCtlGetVersion(
	LPWORD lpwVerMajor, 
	LPWORD lpwVerMinor,
	LPWORD lpwVerBuild,
	LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,
	LPWORD lpwNDFSVerMinor)
{
	HRESULT hr;
	XTL::AutoFileHandle controlDeviceHandle;

	hr = XixfsCtlOpenControlDevice(&controlDeviceHandle);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"XixfsCtlOpenControlDevice failed, hr=0x%X\n", hr);
		return hr;
	}

	return XixfsCtlGetVersionByHandle(
		controlDeviceHandle, 
		lpwVerMajor, lpwVerMinor,
		lpwVerBuild, lpwVerPrivate,
		lpwNDFSVerMajor, lpwNDFSVerMinor);
}

/*++

Get the version information of LFS Filter
This IOCTL supports since Rev 1853

--*/

HRESULT
XixfsCtlShutdownByHandle(
	HANDLE ControlDeviceHandle)
{
	HRESULT hr;
	DWORD bytesReturned;
	BOOL success = ::DeviceIoControl(
		ControlDeviceHandle,
		NDAS_XIXFS_UNLOAD,
		NULL, 0,
		NULL, 0, 
		&bytesReturned,
		NULL);

	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Getting Xixfs preparing for unload failed, hr=0x%X\n", hr);

		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"Xixfs is now shut down and ready to unload.\n");

	return S_OK;
}

HRESULT
XixfsCtlShutdown(VOID)
{
	HRESULT hr;

	XTL::AutoFileHandle controlDeviceHandle;
	
	hr = XixfsCtlOpenControlDevice(&controlDeviceHandle);

	if (FAILED(hr)) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"XixfsCtlOpenControlDevice failed, hr=0x%X\n", hr);
		return hr;
	}

	return XixfsCtlShutdownByHandle(controlDeviceHandle);
}
