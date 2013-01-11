#include <windows.h>
#include <tchar.h>
#include <msi.h>
#include <xixfsctl.h>
#include <xtl/xtltrace.h>

extern "C"
UINT
__stdcall
XixfsMsiShutdown(MSIHANDLE hInstall)
{
	HANDLE controlDeviceHandle;

	HRESULT hr = XixfsCtlOpenControlDevice(&controlDeviceHandle);

	if (FAILED(hr))
	{
		if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr)
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"Xixfs is not available. Shutdown ignored.\n");
			return ERROR_SUCCESS;
		}
		else
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"XixfsCtlOpenControlDevice failed, hr=0x%X\n", hr);
			return ERROR_INSTALL_FAILURE;
		}
	}

	hr = XixfsCtlShutdownByHandle(controlDeviceHandle);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"XixfsCtlShutdownByHandle failed, hr=0x%X\n", hr);

		XTLVERIFY(CloseHandle(controlDeviceHandle));

		return ERROR_INSTALL_FAILURE;
	}

	XTLVERIFY(CloseHandle(controlDeviceHandle));

	return ERROR_SUCCESS;
}
