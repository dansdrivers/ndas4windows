#include <windows.h>
#include <tchar.h>
#include <winioctl.h>
#include <strsafe.h>
#include <crtdbg.h>

#include "ndasfsctl.h"
#include <xtl/xtlautores.h>

#include "trace.h"
#ifdef RUN_WPP
#include "ndasfsctl.tmh"
#endif

#include "ndasfs.h"

const LPCWSTR NdasFatControlDeviceName = L"\\\\.\\NdasFatControl";
const LPCWSTR NdasNtfsControlDeviceName = L"\\\\.\\NdasNtfsControl";
const LPCWSTR NdasRofsControlDeviceName = L"\\\\.\\NdasRofsControl";


HRESULT
NdasFsCtlOpenControlDevice(
	__in LPCWSTR ControlDeviceName,
	__in DWORD DesiredAccess,
	__deref_out HANDLE* ControlDeviceHandle)
{
	HRESULT hr;

	XTLASSERT(NULL != ControlDeviceHandle);
	*ControlDeviceHandle = INVALID_HANDLE_VALUE;

	//
	// Returning handle must not be AutoFileHandle
	//

	HANDLE handle = ::CreateFile(
		ControlDeviceName,
		DesiredAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	if (INVALID_HANDLE_VALUE == handle) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CreateFile(%ls) failed, hr=0x%X\n",
			ControlDeviceName, hr);

		return hr;
	}

	*ControlDeviceHandle = handle;

	return S_OK;
}

HRESULT
NdasFsCtlShutdownEx(
	__in HANDLE ControlDeviceHandle)
{
	HRESULT hr;
	DWORD bytesReturned;
	BOOL success = ::DeviceIoControl(
		ControlDeviceHandle,
		FSCTL_NDAS_FS_UNLOAD,
		NULL, 0,
		NULL, 0, 
		&bytesReturned,
		NULL);

	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NdasFsCtlShutdown failed, handle=%p, hr=0x%X\n", 
			ControlDeviceHandle, hr);

		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"NdasFs is now shut down and ready to unload, handle=%p\n", 
		ControlDeviceHandle);

	return S_OK;
}

HRESULT
NdasFsCtlShutdown(__in LPCWSTR ControlDeviceName)
{
	HRESULT hr;

	XTL::AutoFileHandle controlDeviceHandle;

	hr = NdasFsCtlOpenControlDevice(
		ControlDeviceName, 
		GENERIC_READ | GENERIC_WRITE, 
		&controlDeviceHandle);

	if (FAILED(hr))
	{
		return hr;
	}

	return NdasFsCtlShutdownEx(controlDeviceHandle);
}
