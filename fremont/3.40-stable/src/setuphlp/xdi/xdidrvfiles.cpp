#include "precomp.hpp"
#include "xdi.h"

typedef struct _XDI_ENUM_FQS_CONTEXT {
	XDI_ENUM_DRIVER_FILE_CALLBACK Callback;
	LPVOID CallbackContext;
} XDI_ENUM_FQS_CONTEXT, *PXDI_ENUM_FQS_CONTEXT;

BOOL
xDipWindowsXPOrLater();

UINT
CALLBACK
xDipEnumFileQueueScanCallback(
	PVOID Context,
	UINT Notification,
	UINT_PTR Param1,
	UINT_PTR Param2);

BOOL
xDipWindowsXPOrLater()
{
	OSVERSIONINFOEXW osvi = {sizeof(OSVERSIONINFOEXW)};
	GetVersionExW((LPOSVERSIONINFOW)&osvi);
	return osvi.dwMajorVersion > 5 ||
		(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion >= 1);
}

UINT
CALLBACK
xDipEnumFileQueueScanCallback(
	PVOID Context,
	UINT Notification,
	UINT_PTR Param1,
	UINT_PTR Param2)
{
	PXDI_ENUM_FQS_CONTEXT fqsContext = (PXDI_ENUM_FQS_CONTEXT) Context;

	if (SPFILENOTIFY_QUEUESCAN_EX == Notification)
	{
		PFILEPATHS filePaths = (PFILEPATHS) Param1;

		//fwprintf(stderr, L"%s -> %s, error=%x, flags=%x\n", 
		//	filePaths->Source, filePaths->Target,
		//	filePaths->Win32Error, filePaths->Flags);

		BOOL cont = (*fqsContext->Callback)(
			filePaths->Target, 
			filePaths->Flags, 
			fqsContext->CallbackContext);

		return cont ? NO_ERROR : 1;
	}
	else if (SPFILENOTIFY_QUEUESCAN == Notification)
	{
		LPCWSTR targetPath = (LPCWSTR) Param1;
		DWORD delayFlags = (DWORD) Param2;

		BOOL cont = (*fqsContext->Callback)(
			targetPath, 
			delayFlags, 
			fqsContext->CallbackContext);

		return cont ? NO_ERROR : 1;
	}

	return NO_ERROR;
}

HRESULT
xDiEnumDriverFiles(
	__in_opt HWND Owner,
	__in LPCWSTR OemInfFullPath,
	__in DWORD Flags,
	__in XDI_ENUM_DRIVER_FILE_CALLBACK EnumCallback,
	__in LPVOID EnumContext)
{
	HRESULT hr;
	BOOL success;

	WCHAR infFullPath[MAX_PATH];

	DWORD n = GetFullPathNameW(OemInfFullPath, MAX_PATH, infFullPath, NULL);
	if (0 == n)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = (SUCCEEDED(hr)) ? E_FAIL : hr;
		goto error0;
	}

	HDEVINFO devInfoSet = SetupDiCreateDeviceInfoList(NULL, Owner);
	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		goto error0;
	}

	SP_DEVINFO_DATA devInfoData = { sizeof(SP_DEVINFO_DATA) };

	success = SetupDiCreateDeviceInfoW(
		devInfoSet,
		L"XDI_Temporary_Enumerator",
		&GUID_NULL,
		NULL,
		NULL,
		DICD_GENERATE_ID,
		&devInfoData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		goto error1;
	}

	HSPFILEQ fileQueueHandle = SetupOpenFileQueue();

	if (INVALID_HANDLE_VALUE == fileQueueHandle)
	{
		// Error from SetupOpenFileQueue is only from out-of-memory situation
		// without the last error set
		hr = E_OUTOFMEMORY;
		goto error2;
	}

	SP_DEVINSTALL_PARAMS devInstallParams = {sizeof(SP_DEVINSTALL_PARAMS)};

	success = SetupDiGetDeviceInstallParamsW(
		devInfoSet,
		&devInfoData,
		&devInstallParams);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		goto error3;
	}

	//
	// Specify the search path
	//

	hr = StringCchCopyW(
		devInstallParams.DriverPath,
		MAX_PATH,
		infFullPath);

	if (FAILED(hr))
	{
		goto error3;
	}

	devInstallParams.FlagsEx |= DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
	devInstallParams.FileQueue = fileQueueHandle;
	devInstallParams.Flags |= DI_NOVCP;
	devInstallParams.Flags |= DI_ENUMSINGLEINF;

	success = SetupDiSetDeviceInstallParamsW(
		devInfoSet,
		&devInfoData,
		&devInstallParams);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		goto error3;
	}

	//
	// use DI_FLAGSEX_NO_CLASSLIST_NODE_MERGE if possible 
	// to ensure we look at duplicate nodes (which is broken in itself)
	//
#ifndef DI_FLAGSEX_NO_CLASSLIST_NODE_MERGE
#define DI_FLAGSEX_NO_CLASSLIST_NODE_MERGE  0x08000000L  // Don't remove identical driver nodes from the class list
#endif

	if (xDipWindowsXPOrLater())
	{
		devInstallParams.FlagsEx |= DI_FLAGSEX_NO_CLASSLIST_NODE_MERGE;

		success = SetupDiSetDeviceInstallParamsW(
			devInfoSet,
			&devInfoData,
			&devInstallParams);

		if (!success)
		{
			devInstallParams.FlagsEx &= ~DI_FLAGSEX_NO_CLASSLIST_NODE_MERGE;
		}
	}

	//
	// Build a class driver list with every driver
	//

	success = SetupDiBuildDriverInfoList(
		devInfoSet,
		&devInfoData,
		SPDIT_CLASSDRIVER);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		goto error3;
	}

	SP_DRVINFO_DATA drvInfoData = { sizeof(SP_DRVINFO_DATA) };

	for (DWORD index = 0; ; ++index)
	{
		success = SetupDiEnumDriverInfoW(
			devInfoSet, 
			&devInfoData, 
			SPDIT_CLASSDRIVER, 
			index, 
			&drvInfoData);

		if (!success)
		{
			break;
		}

		SP_DRVINFO_DETAIL_DATA drvInfoDetail = { sizeof(SP_DRVINFO_DETAIL_DATA) };

		success = SetupDiGetDriverInfoDetailW(
			devInfoSet, 
			&devInfoData,
			&drvInfoData,
			&drvInfoDetail,
			sizeof(SP_DRVINFO_DETAIL_DATA),
			NULL);

		if (!success && ERROR_INSUFFICIENT_BUFFER != GetLastError())
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			goto error4;
		}

		success = SetupDiSetSelectedDriverW(
			devInfoSet,
			&devInfoData,
			&drvInfoData);

		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			goto error4;
		}

		if (Flags & XDI_EDF_NO_CLASS_INSTALLER)
		{
			success = SetupDiInstallDriverFiles(
				devInfoSet, &devInfoData);

			if (!success)
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				goto error4;
			}
		}
		else
		{
			success = SetupDiCallClassInstaller(
				DIF_INSTALLDEVICEFILES, devInfoSet, &devInfoData);

			if (!success)
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				goto error4;
			}
		}
	}

	//
	// SPQ_SCAN_USE_CALLBACK_EX checks the digital signature of the file
	// We do not want to check the signature here.
	//
	// SPQ_SCAN_FILE_PRESENCE avoids checking the digital signature of the file
	// in Windows XP or later. (Not in Windows 2000) when used 
	// with SPQ_SCAN_USE_CALLBACK_EX
	//

	XDI_ENUM_FQS_CONTEXT fqsContext = {0};
	fqsContext.Callback = EnumCallback;
	fqsContext.CallbackContext = EnumContext;

	DWORD scanResult;
	success = SetupScanFileQueueW(
		fileQueueHandle, 
		SPQ_SCAN_USE_CALLBACK,
		// SPQ_SCAN_USE_CALLBACKEX | SPQ_SCAN_FILE_PRESENCE,
		Owner,
		xDipEnumFileQueueScanCallback,
		&fqsContext,
		&scanResult);

	if (!success)
	{
		//
		// SetupScanFileQueue may fail using SPQ_SCAN_FILE_PRESENSE flag
		// Try again without SPQ_SCAN_FILE_PRESENSE
		// (when using SPQ_SCAN_USE_CALLBACKEX)
		//
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		goto error4;
	}

	hr = S_OK;

#pragma warning(disable: 4533) // to use goto in cpp

error4:

	SetupDiDestroyDriverInfoList(
		devInfoSet, &devInfoData, SPDIT_CLASSDRIVER);

error3:

	SetupCloseFileQueue(fileQueueHandle);

error2:

	SetupDiDeleteDeviceInfo(devInfoSet, &devInfoData);

error1:

	SetupDiDestroyDeviceInfoList(devInfoSet);

error0:

#pragma warning(default: 4533)

	return hr;
}
