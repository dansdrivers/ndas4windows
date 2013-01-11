#include "stdafx.h"
#include "xdi.h"

#if _DEBUG
#define xDiTrace _xDiTrace
#else
#define xDiTrace __noop
#endif

void _xDiTrace(ULONG Level, LPCSTR Format, ...)
{
	CHAR buffer[256];
	va_list ap;
	va_start(ap, Format);
	StringCchVPrintfA(
		buffer, RTL_NUMBER_OF(buffer), 
		Format, ap);
	OutputDebugStringA(buffer);
	va_end(ap);
}

HRESULT
WINAPI
xDiInstallFromInfW(
	__in_opt HWND Owner,
	__in LPCWSTR InfFile,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService)
{
	HRESULT hr;
	UINT errorLine;

	DWORD fullPathLength = 0;
	LPWSTR fullPath = NULL;
	DWORD n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	if (n > 0)
	{
		fullPathLength = n + 1;
		fullPath = (LPWSTR) HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY, 
			fullPathLength * sizeof(WCHAR));
		if (NULL == fullPath)
		{
			return E_OUTOFMEMORY;
		}
		n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	}

	if (0 == n)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		xDiTrace(4, "GetFullPathNameW failed, inffile=%ls, hr=0x%x\n", InfFile);

		return hr;
	}

	_ASSERT(NULL != fullPath);

	HINF infHandle = SetupOpenInfFileW(
		fullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (INVALID_HANDLE_VALUE == infHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		HeapFree(GetProcessHeap(), 0, fullPath);

		xDiTrace(4, "SetupOpenInfFileW failed, path=%ls, hr=0x%x\n", fullPath, hr);

		return hr;
	}

	hr = xDiInstallFromInfHandleW(
		Owner, infHandle, InfSection, 
		CopyFlags, ServiceInstallFlags, 
		NeedsReboot, NeedsRebootOnService);

	SetupCloseInfFile(infHandle);
	HeapFree(GetProcessHeap(), 0, fullPath);

	return hr;
}

typedef struct _XDI_SETUP_QUEUE_CALLBACK_CONTEXT {
	PVOID DefaultQueueCallbackContext;
	BOOL NeedsReboot;
} XDI_SETUP_QUEUE_CALLBACK_CONTEXT, *PXDI_SETUP_QUEUE_CALLBACK_CONTEXT;

HRESULT
pxDiSetupInitQueueCallback(
	__inout PXDI_SETUP_QUEUE_CALLBACK_CONTEXT Context,
	__in HWND OwnerWindow)
{
	HRESULT hr;
	ZeroMemory(Context, sizeof(XDI_SETUP_QUEUE_CALLBACK_CONTEXT));
	Context->DefaultQueueCallbackContext = SetupInitDefaultQueueCallback(OwnerWindow);
	if (NULL == Context->DefaultQueueCallbackContext)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		return hr;
	}
	return S_OK;
}

VOID
pxDiSetupTermQueueCallback(
	__in PXDI_SETUP_QUEUE_CALLBACK_CONTEXT Context)
{
	SetupTermDefaultQueueCallback(Context->DefaultQueueCallbackContext);
}

UINT
CALLBACK
pxDiSetupFileCallback(
	__in PVOID Context,
	__in UINT Notification,
	__in UINT_PTR Param1,
	__in UINT_PTR Param2)
{
	PXDI_SETUP_QUEUE_CALLBACK_CONTEXT qc = 
		reinterpret_cast<PXDI_SETUP_QUEUE_CALLBACK_CONTEXT>(Context);

	switch (Notification)
	{
	case SPFILENOTIFY_COPYERROR:
	case SPFILENOTIFY_RENAMEERROR:
		return FILEOP_ABORT;
	case SPFILENOTIFY_DELETEERROR:
		return FILEOP_SKIP;
	case SPFILENOTIFY_TARGETEXISTS:
	case SPFILENOTIFY_TARGETNEWER:
		return TRUE; /* overwrite the file */
	case SPFILENOTIFY_FILEOPDELAYED:
		{
			qc->NeedsReboot = TRUE;
			PFILEPATHS filePaths = reinterpret_cast<PFILEPATHS>(Param1);
		}
		return 0;
	default:
		return SetupDefaultQueueCallbackW(
			qc->DefaultQueueCallbackContext,
			Notification,
			Param1,
			Param2);
	}
}

HRESULT
WINAPI
xDiInstallFromInfHandleW(
	__in_opt HWND Owner,
	__in HINF InfHandle,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService)
{
	HRESULT hr;
	WCHAR actionalSection[MAX_PATH] = {0};
	DWORD requiredLength;

	if (NeedsReboot) *NeedsReboot = FALSE;
	if (NeedsRebootOnService) *NeedsRebootOnService = FALSE;

	//
	// Find the actual section to install (.NT, .NTx86, etc)
	//
	BOOL success = SetupDiGetActualSectionToInstallW(
		InfHandle,
		InfSection,
		actionalSection,
		RTL_NUMBER_OF(actionalSection),
		NULL,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		xDiTrace(4, "SetupDiGetActualSectionToInstallW failed, hr=0x%x\n", hr);

		return hr;
	}

	//
	// See if 'reboot' is specified in the section
	//
	BOOL requireReboot = FALSE;

	INFCONTEXT infContext;
	success = SetupFindFirstLineW(
		InfHandle, actionalSection, L"reboot", &infContext);

	if (success)
	{
		requireReboot = TRUE;
	}

	//
	// Initialize default queue callback 
	//
	XDI_SETUP_QUEUE_CALLBACK_CONTEXT callbackContext;

	hr = pxDiSetupInitQueueCallback(&callbackContext, Owner);
	if (FAILED(hr))
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		xDiTrace(4, "pxDiSetupInitQueueCallback failed, hr=0x%x\n", hr);

		return hr;
	}

	//
	// Create a file queue handle
	//
	HSPFILEQ fileQueueHandle = SetupOpenFileQueue();

	if (INVALID_HANDLE_VALUE == fileQueueHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "SetupOpenFileQueue failed, hr=0x%x\n", hr);

		return hr;
	}

	//
	// Install files first
	//
	// recommended to use SP_COPY_FORCE_IN_USE;
	// SetupCommitFileQueueW(NULL, fileQueueHandle, SetupDefaultQueueCallback, );
	success = SetupInstallFilesFromInfSectionW(
		InfHandle, 
		NULL, 
		fileQueueHandle,
		actionalSection,
		NULL,
		CopyFlags);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupCloseFileQueue(fileQueueHandle);
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "SetupInstallFilesFromInfSectionW failed, hr=0x%x\n", hr);

		return hr;
	}

	success = SetupCommitFileQueueW(
		Owner, 
		fileQueueHandle, 
		pxDiSetupFileCallback,
		static_cast<PVOID>(&callbackContext));

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupCloseFileQueue(fileQueueHandle);
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "SetupCommitFileQueueW failed, hr=0x%x\n", hr);

		return hr;
	}

	//
	// Process .Services section
	//

	WCHAR serviceSection[MAX_PATH] = {0};

	hr = StringCchCopy(
		serviceSection,
		RTL_NUMBER_OF(serviceSection),
		actionalSection);

	if (FAILED(hr))
	{
		SetupCloseFileQueue(fileQueueHandle);
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "StringCchCopy failed, hr=0x%x\n", hr);

		return hr;
	}

	hr = StringCchCat(
		serviceSection, 
		RTL_NUMBER_OF(serviceSection), 
		L".Services");

	if (FAILED(hr))
	{
		SetupCloseFileQueue(fileQueueHandle);
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "StringCchCat failed, hr=0x%x\n", hr);

		return hr;
	}

	success = SetupInstallServicesFromInfSectionW(
		InfHandle,
		serviceSection,
		ServiceInstallFlags);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupCloseFileQueue(fileQueueHandle);
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "SetupInstallServicesFromInfSectionW failed, hr=0x%x\n", hr);

		return hr;
	}

	if (ERROR_SUCCESS_REBOOT_REQUIRED == GetLastError())
	{
		if (NeedsRebootOnService)
		{
			*NeedsRebootOnService = TRUE;
		}
	}

	success = SetupInstallFromInfSectionW(
		Owner,
		InfHandle,
		actionalSection,
		SPINST_ALL & ~(SPINST_FILES),
		HKEY_LOCAL_MACHINE,
		NULL,
		CopyFlags,
		pxDiSetupFileCallback,
		static_cast<PVOID>(&callbackContext),
		NULL,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupCloseFileQueue(fileQueueHandle);
		pxDiSetupTermQueueCallback(&callbackContext);

		xDiTrace(4, "SetupInstallFromInfSectionW failed, hr=0x%x\n", hr);

		return hr;
	}

	requireReboot |= callbackContext.NeedsReboot;

	SetupCloseFileQueue(fileQueueHandle);
	pxDiSetupTermQueueCallback(&callbackContext);

	if (NeedsReboot) *NeedsReboot = requireReboot;

	return hr;
}


