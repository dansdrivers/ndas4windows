#include <windows.h>
#include <tchar.h>
#include <msi.h>
#include <ndas/ndasfsctl.h>
#include <xtl/xtltrace.h>

UINT
NdasfsMsipShutdown(MSIHANDLE hInstall, LPCWSTR ControlDeviceName)
{
	HANDLE controlDeviceHandle;

	HRESULT hr = NdasFsCtlOpenControlDevice(
		ControlDeviceName, GENERIC_READ | GENERIC_WRITE,
		&controlDeviceHandle);

	if (FAILED(hr))
	{
		if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr)
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Control device(%ls) is not available. Shutdown ignored.\n",
				ControlDeviceName);
			return ERROR_SUCCESS;
		}
		else
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"OpenControlDevice(%ls) failed, hr=0x%X\n",
				ControlDeviceName, hr);
			return ERROR_INSTALL_FAILURE;
		}
	}

	hr = NdasFsCtlShutdownEx(controlDeviceHandle);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"NdasFsCtlShutdownEx failed, hr=0x%X\n", hr);

		XTLVERIFY(CloseHandle(controlDeviceHandle));

		return ERROR_INSTALL_FAILURE;
	}

	XTLVERIFY(CloseHandle(controlDeviceHandle));

	return ERROR_SUCCESS;
}

extern "C"
UINT
__stdcall
NdasNtfsMsiShutdown(MSIHANDLE hInstall)
{
	return NdasfsMsipShutdown(hInstall, NdasNtfsControlDeviceName);
}

extern "C"
UINT
__stdcall
NdasFatMsiShutdown(MSIHANDLE hInstall)
{
	return NdasfsMsipShutdown(hInstall, NdasFatControlDeviceName);
}
