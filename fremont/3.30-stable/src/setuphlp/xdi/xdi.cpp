#include "precomp.hpp"
#include "xdi.h"

class CCoInit
{
	BOOL m_init;
public:
	CCoInit() : m_init(FALSE) {}
	~CCoInit()
	{
		if (m_init) Uninitialize();
	}
	HRESULT Initialize(DWORD Flags)
	{
		ATLASSERT(!m_init);
		HRESULT hr = CoInitializeEx(NULL, Flags);
		if (SUCCEEDED(hr)) m_init = TRUE;
		return hr;
	}
	void Uninitialize()
	{
		CoUninitialize();
	}
};

HRESULT
xDipCreateRunOnceKey();

HRESULT
xDipContainsHardwareId(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in LPCWSTR HardwareId);

HRESULT
WINAPI
xDiInstallFromInfSection(
	__in_opt HWND Owner,
	__in LPCWSTR InfFile,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService)
{
	//
	// Make sure that HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce
	// registry key exists. Otherwise, installation may fail with the
	// error code ERROR_FILE_NOT_FOUND.
	//
	xDipCreateRunOnceKey();

	HRESULT hr;
	UINT errorLine;

	DWORD fullPathLength = 0;
	LPWSTR fullPath = NULL;
	DWORD n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	if (n > 0)
	{
		fullPathLength = n + 1;
		fullPath = (LPWSTR) calloc(fullPathLength, sizeof(WCHAR));
		if (NULL == fullPath)
		{
			return E_OUTOFMEMORY;
		}
		n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	}

	if (0 == n)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = (SUCCEEDED(hr)) ? E_FAIL : hr;

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"GetFullPathNameW failed, inf=%ls, hr=0x%x\n", InfFile, hr);

		return hr;
	}

	_ASSERT(NULL != fullPath);

	HINF infHandle = SetupOpenInfFileW(
		fullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (INVALID_HANDLE_VALUE == infHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupOpenInfFileW failed, path=%ls, hr=0x%x\n", fullPath, hr);

		free(fullPath);

		return hr;
	}

	hr = xDiInstallFromInfSectionEx(
		Owner, infHandle, InfSection, 
		CopyFlags, ServiceInstallFlags, 
		NeedsReboot, NeedsRebootOnService);

	SetupCloseInfFile(infHandle);
	free(fullPath);

	return hr;
}

typedef struct _XDI_SETUP_QUEUE_CALLBACK_CONTEXT {
	PVOID DefaultQueueCallbackContext;
	BOOL NeedsReboot;
} XDI_SETUP_QUEUE_CALLBACK_CONTEXT, *PXDI_SETUP_QUEUE_CALLBACK_CONTEXT;

HRESULT
xDipSetupInitQueueCallback(
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
xDipSetupTermQueueCallback(
	__in PXDI_SETUP_QUEUE_CALLBACK_CONTEXT Context)
{
	SetupTermDefaultQueueCallback(Context->DefaultQueueCallbackContext);
}

UINT
CALLBACK
xDipSetupFileCallback(
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
xDiServiceIsMarkedForDeletionFromPnpInf(
	__in LPCWSTR InfFile,
	__in LPCWSTR HardwareId,
	__deref_out_opt LPWSTR* ServiceName)
{
	HRESULT hr;
	UINT errorLine;

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Inf=%ls, HardwareId=%ls\n", InfFile, HardwareId);

	if (ServiceName) *ServiceName = NULL;

	DWORD fullPathLength = 0;
	LPWSTR fullPath = NULL;
	DWORD n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	if (n > 0)
	{
		fullPathLength = n + 1;
		fullPath = (LPWSTR) calloc(fullPathLength, sizeof(WCHAR));
		if (NULL == fullPath)
		{
			return E_OUTOFMEMORY;
		}
		n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	}

	if (0 == n)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = (SUCCEEDED(hr)) ? E_FAIL : hr;

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"GetFullPathNameW failed, inf=%ls, hr=0x%x\n", InfFile, hr);

		return hr;
	}

	_ASSERT(NULL != fullPath);

	HINF infHandle = SetupOpenInfFileW(
		fullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (INVALID_HANDLE_VALUE == infHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupOpenInfFileW failed, path=%ls, hr=0x%x\n", fullPath, hr);

		free(fullPath);

		return hr;
	}

	//
	// [Manufacturer]
	//  %MANUFACTURER% = Generic
	//

	INFCONTEXT infContext;
	for (BOOL found = SetupFindFirstLineW(infHandle, L"Manufacturer", NULL, &infContext);
		found; found = SetupFindNextLine(&infContext, &infContext))
	{
		WCHAR mfgName[512];
		BOOL success = SetupGetStringFieldW(
			&infContext, 1, 
			mfgName, RTL_NUMBER_OF(mfgName), 
			NULL);

		if (!success)
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, "SetupGetStringFieldW(Manufacturer) failed, hr=0x%x\n", 
				HRESULT_FROM_SETUPAPI(GetLastError()));
		}
		else
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, "Manufacturer=%ls\n", mfgName);

			WCHAR altMfgName[512];
			
			success = SetupDiGetActualSectionToInstallW(
				infHandle, mfgName, 
				altMfgName, RTL_NUMBER_OF(altMfgName),
				NULL, NULL);

			if (!success)
			{
				XTLTRACE1(TRACE_LEVEL_ERROR, "SetupDiGetActualSectionToInstall(Manufacturer) failed, hr=0x%x\n", 
					HRESULT_FROM_SETUPAPI(GetLastError()));
				continue;
			}

			XTLTRACE1(TRACE_LEVEL_INFORMATION, "AltManufacturer=%ls\n", altMfgName);

			//
			// [Generic.NTamd64]
			// %NDASBUS.DeviceDesc% = NDASBUS.DDI,Root\NDASBus
			//
			INFCONTEXT mfgInfContext;

			for (BOOL found2 = SetupFindFirstLineW(infHandle, altMfgName, NULL, &mfgInfContext);
				found2; found2 = SetupFindNextLine(&mfgInfContext, &mfgInfContext))
			{
				WCHAR buffer[512];
				
				//
				// Read the device id
				//

				success = SetupGetStringFieldW(
					&mfgInfContext, 2, buffer, RTL_NUMBER_OF(buffer), NULL);

				if (!success)
				{
					XTLTRACE1(TRACE_LEVEL_ERROR, "SetupGetStringField(Model) failed, hr=0x%x\n", 
						HRESULT_FROM_SETUPAPI(GetLastError()));
					continue;
				}

				XTLTRACE1(TRACE_LEVEL_INFORMATION, "DeviceId=%ls\n", buffer);

				if (0 != lstrcmpi(HardwareId, buffer))
				{
					XTLTRACE1(TRACE_LEVEL_INFORMATION, "SetupGetStringField(Model) skipped, target=%ls, inf=%ls\n", 
						HardwareId, buffer);
					continue;
				}

				//
				// Read the section to install
				//
				success = SetupGetStringFieldW(
					&mfgInfContext, 1, 
					buffer, RTL_NUMBER_OF(buffer), NULL);

				if (!success)
				{
					XTLTRACE1(TRACE_LEVEL_ERROR, "SetupGetStringField(Section) failed, hr=0x%x\n", 
						HRESULT_FROM_SETUPAPI(GetLastError()));
					continue;
				}

				hr = xDiServiceIsMarkedForDeletionFromInfSectionEx(
					infHandle, buffer, ServiceName);

				if (S_FALSE != hr)
				{
					XTLTRACE1(TRACE_LEVEL_ERROR, "xDiServiceIsMarkedForDeletionFromInfSectionEx returned, hr=0x%x\n", hr);

					SetupCloseInfFile(infHandle);
					free(fullPath);
					return hr;
				}

				XTLTRACE1(TRACE_LEVEL_INFORMATION, "xDiServiceIsMarkedForDeletionFromInfSectionEx returned, hr=0x%x\n", hr);
			}

			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			if (hr != 0x800f0102)
			{
				XTLTRACE1(TRACE_LEVEL_ERROR, "SetupFindFirstLine(Model) failed, hr=0x%x\n", hr);
			}
		}
	}

	hr = HRESULT_FROM_SETUPAPI(GetLastError());
	if (hr != 0x800f0102)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, "SetupFindFirstLine(Manufacturer) failed, hr=0x%x\n", hr);
	}

	SetupCloseInfFile(infHandle);
	free(fullPath);

	return S_FALSE;
}

HRESULT
WINAPI
xDiServiceIsMarkedForDeletionFromInfSection(
	__in LPCWSTR InfFile,
	__in LPCWSTR InfSection,
	__deref_out_opt LPWSTR* ServiceName)
{
	HRESULT hr;
	UINT errorLine;

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Inf=%ls, Section=%ls\n", InfFile, InfSection);

	if (ServiceName) *ServiceName = NULL;

	DWORD fullPathLength = 0;
	LPWSTR fullPath = NULL;
	DWORD n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	if (n > 0)
	{
		fullPathLength = n + 1;
		fullPath = (LPWSTR) calloc(fullPathLength, sizeof(WCHAR));
		if (NULL == fullPath)
		{
			return E_OUTOFMEMORY;
		}
		n = GetFullPathNameW(InfFile, fullPathLength, fullPath, NULL);
	}

	if (0 == n)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		hr = (SUCCEEDED(hr)) ? E_FAIL : hr;

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"GetFullPathNameW failed, inf=%ls, hr=0x%x\n", InfFile, hr);

		return hr;
	}

	_ASSERT(NULL != fullPath);

	HINF infHandle = SetupOpenInfFileW(
		fullPath, NULL, INF_STYLE_WIN4, &errorLine);

	if (INVALID_HANDLE_VALUE == infHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		hr = (SUCCEEDED(hr)) ? E_FAIL : hr;

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupOpenInfFileW failed, path=%ls, hr=0x%x\n", fullPath, hr);

		free(fullPath);

		return hr;
	}

	hr = xDiServiceIsMarkedForDeletionFromInfSectionEx(
		infHandle, InfSection, ServiceName);

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"xDiServiceIsMarkedForDeletionFromInfSectionEx returned, hr=0x%x\n", hr);

	SetupCloseInfFile(infHandle);

	free(fullPath);

	return hr;
}

HRESULT
WINAPI
xDipServiceIsMarkedForDeletionInfContext(
	__inout PINFCONTEXT InfContext,
	__deref_out_opt LPWSTR* ServiceName)
{
	HRESULT hr;
	BOOL success;
	//
	// SetupGetStringField returns the required size 
	// including the null terminator
	//
	LPWSTR serviceName = NULL;
	DWORD serviceNameLength = 0;

	success = SetupGetStringFieldW(
		InfContext, 1,
		serviceName, serviceNameLength,
		&serviceNameLength);

	//
	// Unlike other API functions, SetupGetStringField returns non-zero
	// for getting buffer length only
	//
	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupGetStringField for DelService failed, hr=0x%x\n", hr);
		return hr;
	}

	serviceName = (LPWSTR) CoTaskMemAlloc(serviceNameLength * sizeof(WCHAR));
	if (NULL == serviceName)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"Memory allocation failure, %d bytes\n", serviceNameLength * sizeof(WCHAR));

		return E_OUTOFMEMORY;
	}

	success = SetupGetStringFieldW(
		InfContext, 1,
		serviceName, serviceNameLength, 
		&serviceNameLength);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		CoTaskMemFree(serviceName);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupGetStringField(2) for DelService failed, hr=0x%x\n", hr);

		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, "\tService=%ls\n", serviceName);

	if (success)
	{
		hr = xDiServiceIsMarkedForDeletion(serviceName);
		if (S_OK == hr)
		{
			if (ServiceName)
			{
				*ServiceName = serviceName;
			}
			else
			{
				CoTaskMemFree(serviceName);
			}
			return S_OK;
		}
	}

	return S_FALSE;
}

HRESULT
WINAPI
xDiServiceIsMarkedForDeletionFromInfSectionEx(
	__in HINF InfHandle,
	__in LPCWSTR InfSection,
	__deref_out_opt LPWSTR* ServiceName)
{
	HRESULT hr;
	WCHAR actionalSection[MAX_PATH] = {0};
	DWORD requiredLength;

	if (ServiceName) *ServiceName = NULL;

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

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupDiGetActualSectionToInstallW failed, hr=0x%x\n", hr);

		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, "ActualSection=%ls\n", actionalSection);

	//
	// Process .Services section
	//

	WCHAR serviceSection[MAX_PATH] = {0};

	hr = StringCchCopyW(
		serviceSection,
		RTL_NUMBER_OF(serviceSection),
		actionalSection);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"StringCchCopy failed, hr=0x%x\n", hr);

		return hr;
	}

	hr = StringCchCatW(
		serviceSection, 
		RTL_NUMBER_OF(serviceSection), 
		L".Services");

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"StringCchCat failed, hr=0x%x\n", hr);

		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, "ActualServicesSection=%ls\n", serviceSection);

	//
	// [DefaultUninstall.Services]
	// DelService=<service-name>,<flags>
	//

	XTLTRACE1(TRACE_LEVEL_INFORMATION, "Inspecting DelService\n");

	INFCONTEXT infContext;

	BOOL found = SetupFindFirstLineW(
		InfHandle, 
		serviceSection, 
		L"DelService", 
		&infContext);

	while (found)
	{
		hr = xDipServiceIsMarkedForDeletionInfContext(&infContext, ServiceName);
		if (S_FALSE != hr)
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, "xDipServiceIsMarkedForDeletionInfContext returned, hr=0x%x\n", hr);
			return hr;
		}
		found = SetupFindNextLine(&infContext, &infContext);
	}

	//
	// [DefaultInstall.Services]
	// AddService=<service-name>,,<service-section>
	//

	XTLTRACE1(TRACE_LEVEL_INFORMATION, "Inspecting AddService\n");

	found = SetupFindFirstLineW(
		InfHandle,
		serviceSection,
		L"AddService",
		&infContext);

	while (found)
	{
		hr = xDipServiceIsMarkedForDeletionInfContext(&infContext, ServiceName);
		if (S_FALSE != hr)
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, "xDipServiceIsMarkedForDeletionInfContext returned, hr=0x%x\n", hr);
			return hr;
		}
		found = SetupFindNextLine(&infContext, &infContext);
	}

	return S_FALSE;
}

HRESULT
WINAPI
xDiInstallFromInfSectionEx(
	__in_opt HWND Owner,
	__in HINF InfHandle,
	__in LPCWSTR InfSection,
	__in UINT CopyFlags,
	__in UINT ServiceInstallFlags,
	__out_opt LPBOOL NeedsReboot,
	__out_opt LPBOOL NeedsRebootOnService)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"Owner=%p, InfHandle=%p, InfSection=%ls, CopyFlags=%08X, SvcFlags=%08X\n",
		Owner, InfHandle, InfSection, CopyFlags, ServiceInstallFlags);

	//
	// Make sure that HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce
	// registry key exists. Otherwise, installation may fail with the
	// error code ERROR_FILE_NOT_FOUND.
	//
	xDipCreateRunOnceKey();

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

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupDiGetActualSectionToInstallW failed, hr=0x%x\n", hr);

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

	hr = xDipSetupInitQueueCallback(&callbackContext, Owner);
	if (FAILED(hr))
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"xDipSetupInitQueueCallback failed, hr=0x%x\n", hr);

		return hr;
	}

	//
	// Create a file queue handle
	//
	HSPFILEQ fileQueueHandle = SetupOpenFileQueue();

	if (INVALID_HANDLE_VALUE == fileQueueHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupOpenFileQueue failed, hr=0x%x\n", hr);

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
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupInstallFilesFromInfSectionW failed, hr=0x%x\n", hr);

		return hr;
	}

	success = SetupCommitFileQueueW(
		Owner, 
		fileQueueHandle, 
		xDipSetupFileCallback,
		static_cast<PVOID>(&callbackContext));

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupCloseFileQueue(fileQueueHandle);
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupCommitFileQueueW failed, hr=0x%x\n", hr);

		return hr;
	}

	//
	// Process .Services section
	//

	WCHAR serviceSection[MAX_PATH] = {0};

	hr = StringCchCopyW(
		serviceSection,
		RTL_NUMBER_OF(serviceSection),
		actionalSection);

	if (FAILED(hr))
	{
		SetupCloseFileQueue(fileQueueHandle);
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"StringCchCopy failed, hr=0x%x\n", hr);

		return hr;
	}

	hr = StringCchCatW(
		serviceSection, 
		RTL_NUMBER_OF(serviceSection), 
		L".Services");

	if (FAILED(hr))
	{
		SetupCloseFileQueue(fileQueueHandle);
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"StringCchCat failed, hr=0x%x\n", hr);

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
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupInstallServicesFromInfSectionW failed, hr=0x%x\n", hr);

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
		xDipSetupFileCallback,
		static_cast<PVOID>(&callbackContext),
		NULL,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupCloseFileQueue(fileQueueHandle);
		xDipSetupTermQueueCallback(&callbackContext);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupInstallFromInfSectionW failed, hr=0x%x\n", hr);

		return hr;
	}

	requireReboot |= callbackContext.NeedsReboot;

	SetupCloseFileQueue(fileQueueHandle);
	xDipSetupTermQueueCallback(&callbackContext);

	if (NeedsReboot) *NeedsReboot = requireReboot;

	return hr;
}

HRESULT
WINAPI
xDiInstallNetComponent(
	__in LPCGUID ClassGuid,
	__in LPCWSTR ComponentId,
	__in DWORD LockTimeout,
	__in LPCWSTR ClientDescription,
	__deref_out_opt LPWSTR* LockingClientDescription)
{
#ifdef _DEBUG

	OLECHAR classGuidString[64];

	StringFromGUID2(*ClassGuid, classGuidString, RTL_NUMBER_OF(classGuidString));

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"xDiInstallNetComponent(ClassGuid=%ls, ComponentId=%ls)\n",
		OLE2W(classGuidString), ComponentId);

#endif

	//
	// Make sure that HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce
	// registry key exists. Otherwise, installation may fail with the
	// error code ERROR_FILE_NOT_FOUND.
	//
	xDipCreateRunOnceKey();

	HRESULT hr, hr2;
	CCoInit coinit;

	hr = coinit.Initialize(COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CoInitializeEx failed, hr=0x%x\n", hr);
		return hr;
	}

	CComPtr<INetCfg> pNetCfg;
	hr = pNetCfg.CoCreateInstance(CLSID_CNetCfg);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CoCreateInstance(CLSID_CNetCfg) failed, hr=0x%x\n", hr);
		return hr;
	}

	CComQIPtr<INetCfgLock> pNetCfgLock = pNetCfg;
	
	if (NULL == pNetCfgLock.p)
	{
		hr = E_NOINTERFACE;
		return hr;
	}

	LPWSTR lockedBy = NULL;

	hr = pNetCfgLock->AcquireWriteLock(
		LockTimeout, 
		ClientDescription, 
		&lockedBy);

	if (LockingClientDescription)
	{
		*LockingClientDescription = lockedBy;
	}
	else
	{
		CoTaskMemFree(lockedBy);
	}

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.AcquireWriteLock failed, hr=0x%x\n", hr);
		return hr;
	}
	else if (S_FALSE == hr)
	{
		hr = NETCFG_E_NO_WRITE_LOCK;

		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.AcquireWriteLock failed, hr=0x%x\n", hr);

		return hr;
	}
	
	hr = pNetCfg->Initialize(NULL);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.Initialize failed, hr=0x%x\n", hr);

		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	CComPtr<INetCfgClassSetup> pNetCfgClassSetup;
	hr = pNetCfg->QueryNetCfgClass(
		ClassGuid, 
		IID_INetCfgClassSetup, 
		(void**) &pNetCfgClassSetup);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.QueryNetCfgClass failed, hr=0x%x\n", hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));

		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));

		return hr;
	}

	OBO_TOKEN oboToken = {OBO_USER};

	CComPtr<INetCfgComponent> pNetCfgComponent;
	hr = pNetCfg->FindComponent(ComponentId, &pNetCfgComponent);
	if (S_FALSE != hr)
	{
		if (S_OK == hr) hr = S_FALSE;

		XTLTRACE1(TRACE_LEVEL_WARNING,
			"Component is already installed or failed, componentId=%ls, hr=0x%x\n", ComponentId, hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));

		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));

		return hr;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"Component is not installed, componentId=%ls, hr=0x%x\n", ComponentId, hr);

	hr = pNetCfgClassSetup->Install(
		ComponentId,
		&oboToken,
		NSF_POSTSYSINSTALL,
		0,
		NULL,
		NULL,
		&pNetCfgComponent);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfgClassSetup.Install failed, hr=0x%x\n", hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));

		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));

		return hr;
	}

	hr = pNetCfg->Apply();

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.Apply failed, hr=0x%x\n", hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));

		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));

		return hr;
	}

	hr2 = pNetCfg->Uninitialize();
	ATLASSERT(SUCCEEDED(hr2));

	hr2 = pNetCfgLock->ReleaseWriteLock();
	ATLASSERT(SUCCEEDED(hr2));

	return hr;
}

HRESULT
xDipGetNetComponentInfPath(
	__in LPCGUID ClassGuid,
	__in LPCGUID InstanceGuid,
	__deref_out LPWSTR* InfPath)
{
	HRESULT hr;

	*InfPath = NULL;

	OLECHAR classGuidString[64];
	OLECHAR instanceGuidString[64];

	StringFromGUID2(
		*InstanceGuid, 
		instanceGuidString, 
		RTL_NUMBER_OF(instanceGuidString));
	
	StringFromGUID2(
		*ClassGuid, 
		classGuidString, 
		RTL_NUMBER_OF(classGuidString));

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"xDiInstallNetComponent(ClassGuid=%ls,InstanceguId=%ls)\n",
		OLE2W(classGuidString), OLE2W(instanceGuidString));

	WCHAR deviceRegPath[MAX_PATH];

	StringCchPrintfW(
		deviceRegPath,
		RTL_NUMBER_OF(deviceRegPath),
		L"SYSTEM\\CurrentControlSet\\Control\\Network\\%s\\%s",
		OLE2W(classGuidString),
		OLE2W(instanceGuidString));

	HKEY deviceKeyHandle;

	LONG result = RegOpenKeyExW(
		HKEY_LOCAL_MACHINE, 
		deviceRegPath,
		0,
		KEY_READ,
		&deviceKeyHandle);

	if (ERROR_SUCCESS != result)
	{
		hr = HRESULT_FROM_WIN32(result);
		return hr;
	}

	WCHAR infName[MAX_PATH];
	DWORD infNameSize = sizeof(infName);

	result = RegQueryValueExW(
		deviceKeyHandle,
		L"InfPath",
		NULL,
		NULL,
		(LPBYTE) infName,
		&infNameSize);

	if (ERROR_SUCCESS != result)
	{
		hr = HRESULT_FROM_WIN32(result);
		RegCloseKey(deviceKeyHandle);
		return hr;
	}

	RegCloseKey(deviceKeyHandle);

	*InfPath = (LPWSTR) CoTaskMemAlloc(infNameSize);
	if (NULL == *InfPath)
	{
		return E_OUTOFMEMORY;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"infName=%ls\n", infName);

	CopyMemory(*InfPath, infName, infNameSize);

	return S_OK;
}

HRESULT
WINAPI
xDiUninstallNetComponent(
	__in LPCWSTR ComponentId,
	__in DWORD LockTimeout,
	__in LPCWSTR ClientDescription,
	__deref_out_opt LPWSTR* LockingClientDescription,
	__deref_out_opt LPWSTR* OEMInfName)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"ComponentId=%ls\n", ComponentId);

	//
	// Make sure that HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce
	// registry key exists. Otherwise, installation may fail with the
	// error code ERROR_FILE_NOT_FOUND.
	//
	xDipCreateRunOnceKey();

	HRESULT hr, hr2;
	CCoInit coinit;

	hr = coinit.Initialize(COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CoInitializeEx failed, hr=0x%x\n", hr);
		return hr;
	}

	CComPtr<INetCfg> pNetCfg;
	hr = pNetCfg.CoCreateInstance(CLSID_CNetCfg);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CoCreateInstance(CLSID_CNetCfg) failed, hr=0x%x\n", hr);
		return hr;
	}

	CComQIPtr<INetCfgLock> pNetCfgLock = pNetCfg;
	ATLASSERT(pNetCfgLock.p);

	LPWSTR lockedBy = NULL;

	hr = pNetCfgLock->AcquireWriteLock(
		LockTimeout, 
		ClientDescription, 
		&lockedBy);

	if (LockingClientDescription)
	{
		*LockingClientDescription = lockedBy;
	}
	else
	{
		CoTaskMemFree(lockedBy);
	}

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.AcquireWriteLock failed, hr=0x%x\n", hr);
		return hr;
	}
	else if (S_FALSE == hr)
	{
		hr = NETCFG_E_NO_WRITE_LOCK;

		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.AcquireWriteLock failed, hr=0x%x\n", hr);

		return hr;
	}

	hr = pNetCfg->Initialize(NULL);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.Initialize failed, hr=0x%x\n", hr);

		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	CComPtr<INetCfgComponent> pNetCfgComponent;
	hr = pNetCfg->FindComponent(ComponentId, &pNetCfgComponent);

	if (FAILED(hr) || S_FALSE == hr)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.FindComponent failed, ComponentId=%ls, hr=0x%x\n", 
			ComponentId, hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	GUID classGuid;
	hr = pNetCfgComponent->GetClassGuid(&classGuid);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.GetClassGuid failed, hr=0x%x\n", hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	if (OEMInfName)
	{
		GUID instanceGuid;
		hr = pNetCfgComponent->GetInstanceGuid(&instanceGuid);
		if (FAILED(hr))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"NetCfgComponent.GetInstanceGuid failed, hr=0x%x\n", hr);

			hr2 = pNetCfg->Uninitialize();
			ATLASSERT(SUCCEEDED(hr2));
			hr2 = pNetCfgLock->ReleaseWriteLock();
			ATLASSERT(SUCCEEDED(hr2));
			return hr;
		}

		hr = xDipGetNetComponentInfPath(&classGuid, &instanceGuid, OEMInfName);
		if (FAILED(hr))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"xDipGetNetComponentInfPath failed, hr=0x%x\n", hr);

			hr2 = pNetCfg->Uninitialize();
			ATLASSERT(SUCCEEDED(hr2));
			hr2 = pNetCfgLock->ReleaseWriteLock();
			ATLASSERT(SUCCEEDED(hr2));
			return hr;
		}
	}

	CComPtr<INetCfgClassSetup> pNetCfgClassSetup;
	hr = pNetCfg->QueryNetCfgClass(
		&classGuid, 
		IID_INetCfgClassSetup, 
		(void**) &pNetCfgClassSetup);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.QueryNetCfgClass failed, hr=0x%x\n", hr);

		if (OEMInfName)
		{
			CoTaskMemFree(*OEMInfName);
			*OEMInfName = NULL;
		}

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	OBO_TOKEN oboToken = {OBO_USER};

	CComHeapPtr<WCHAR> referenceCount;

	hr = pNetCfgClassSetup->DeInstall(
		pNetCfgComponent,
		&oboToken,
		&referenceCount);

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"NetCfgClassSetup.Deinstall returns hr=0x%X\n", hr);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfgClassSetup.DeInstall failed, hr=0x%x\n", hr);

		if (OEMInfName)
		{
			CoTaskMemFree(*OEMInfName);
			*OEMInfName = NULL;
		}

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	hr = pNetCfg->Apply();

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"NetCfgClassSetup.Apply returns hr=0x%X\n", hr);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.Apply failed, hr=0x%x\n", hr);

		if (OEMInfName)
		{
			CoTaskMemFree(*OEMInfName);
			*OEMInfName = NULL;
		}

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		hr2 = pNetCfgLock->ReleaseWriteLock();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	hr2 = pNetCfg->Uninitialize();
	ATLASSERT(SUCCEEDED(hr2));
	hr2 = pNetCfgLock->ReleaseWriteLock();
	ATLASSERT(SUCCEEDED(hr2));

	return hr;
}

HRESULT
WINAPI
xDiFindNetComponent(
	__in LPCWSTR ComponentId,
	__deref_out_opt LPWSTR* OEMInfName)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"ComponentId=%ls\n", ComponentId);

	HRESULT hr, hr2;
	CCoInit coinit;

	hr = coinit.Initialize(COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CoInitializeEx failed, hr=0x%x\n", hr);
		return hr;
	}

	CComPtr<INetCfg> pNetCfg;
	hr = pNetCfg.CoCreateInstance(CLSID_CNetCfg);
	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CoCreateInstance(CLSID_CNetCfg) failed, hr=0x%x\n", hr);
		return hr;
	}

	hr = pNetCfg->Initialize(NULL);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.Initialize failed, hr=0x%x\n", hr);
		return hr;
	}

	CComPtr<INetCfgComponent> pNetCfgComponent;
	hr = pNetCfg->FindComponent(ComponentId, &pNetCfgComponent);

	if (FAILED(hr) || S_FALSE == hr)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.FindComponent failed, ComponentId=%ls, hr=0x%x\n", 
			ComponentId, hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	GUID classGuid;
	hr = pNetCfgComponent->GetClassGuid(&classGuid);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"NetCfg.GetClassGuid failed, hr=0x%x\n", hr);

		hr2 = pNetCfg->Uninitialize();
		ATLASSERT(SUCCEEDED(hr2));
		return hr;
	}

	if (OEMInfName)
	{
		GUID instanceGuid;
		hr = pNetCfgComponent->GetInstanceGuid(&instanceGuid);
		if (FAILED(hr))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"NetCfgComponent.GetInstanceGuid failed, hr=0x%x\n", hr);

			hr2 = pNetCfg->Uninitialize();
			ATLASSERT(SUCCEEDED(hr2));
			return hr;
		}

		hr = xDipGetNetComponentInfPath(&classGuid, &instanceGuid, OEMInfName);
		if (FAILED(hr))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"xDipGetNetComponentInfPath failed, hr=0x%x\n", hr);

			hr2 = pNetCfg->Uninitialize();
			ATLASSERT(SUCCEEDED(hr2));
			return hr;
		}

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "OemInf=%ls\n", OEMInfName);
	}

	hr2 = pNetCfg->Uninitialize();
	ATLASSERT(SUCCEEDED(hr2));

	return hr;
}

typedef struct _XDI_FIND_DEVICE_INSTANCE_ENUM_CONTEXT {
	BOOL Found;
	LPCWSTR HardwareId;
} XDI_FIND_DEVICE_INSTANCE_ENUM_CONTEXT;

BOOL
CALLBACK
xDipFindPnpDeviceInstanceEnumCallback(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in LPVOID Context);

HRESULT
WINAPI
xDiFindPnpDeviceInstance(
	__in HWND Owner,
	__in_opt LPCGUID ClassGuid,
	__in_opt LPCWSTR Enumerator,
	__in LPCWSTR HardwareId,
	__in DWORD EnumFlags)
{
	XDI_FIND_DEVICE_INSTANCE_ENUM_CONTEXT findContext = {0};
	findContext.HardwareId = HardwareId;

	HRESULT hr = xDiEnumDevices(
		Owner, 
		ClassGuid, 
		Enumerator, 
		EnumFlags, 
		xDipFindPnpDeviceInstanceEnumCallback, 
		&findContext);

	if (FAILED(hr))
	{
		return hr;
	}

	return findContext.Found ? S_OK : S_FALSE;
}

BOOL
xDipFindPnpDeviceInstanceEnumCallback(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in LPVOID Context)
{
	XDI_FIND_DEVICE_INSTANCE_ENUM_CONTEXT* findContext = 
		static_cast<XDI_FIND_DEVICE_INSTANCE_ENUM_CONTEXT*>(Context);

	HRESULT hr = xDipContainsHardwareId(
		DeviceInfoSet, 
		DeviceInfoData, 
		findContext->HardwareId);

	if (S_OK == hr)
	{
		findContext->Found = TRUE;
		return FALSE; // Stop enumeration
	}

	return TRUE; // continue enumeration
}

HRESULT
WINAPI
xDiInstallLegacyPnpDevice(
	__in HWND Owner,
	__in LPCWSTR HardwareId,
	__in LPCWSTR InfPath,
	__in_opt LPCWSTR DeviceName,
	__in DWORD InstallFlags,
	__in DWORD UpdateFlags)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Owner=%p, HardwareId=%ls, InfPath=%ls, DeviceName=%ls, InstallFlags=%08X, UpdateFlags=%08X\n",
		Owner, HardwareId, InfPath, DeviceName, InstallFlags, UpdateFlags);

	HRESULT hr;
	BOOL success;
	GUID classGuid;
	WCHAR className[MAX_PATH];

	//
	// Make sure that HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce
	// registry key exists. Otherwise, installation may fail with the
	// error code ERROR_FILE_NOT_FOUND.
	//
	xDipCreateRunOnceKey();

	success = SetupDiGetINFClassW(
		InfPath,
		&classGuid,
		className,
		RTL_NUMBER_OF(className),
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		return hr;
	}

	//
	// Search for existing instances if single instance is specified
	//
	if (InstallFlags & XDI_INSTALL_SINGLE_INSTANCE)
	{
		hr = xDiFindPnpDeviceInstance(
			Owner, 
			&classGuid,
			NULL,
			HardwareId,
			DIGCF_PRESENT);

		if (FAILED(hr))
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"xDiFindPnpDeviceInstance failed, hr=0x%X\n", hr);
			return hr;
		}

		if (S_OK == hr)
		{
			BOOL rebootRequired = FALSE;

			success = Newdev_UpdateDriverForPlugAndPlayDevicesW(
				Owner,
				HardwareId,
				InfPath,
				UpdateFlags,
				&rebootRequired);

			if (!success)
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());

				XTLTRACE1(TRACE_LEVEL_ERROR, 
					"UpdateDriverForPlugAndPlayDevicesW failed, hr=0x%X\n", hr);

				return hr;
			}

			return rebootRequired ? XDI_S_REBOOT : S_OK;
		}
	}

	//
	// Create the container for the to-be-created Device Information Element.
	//

	HDEVINFO devInfoSet = SetupDiCreateDeviceInfoList(&classGuid, Owner);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		return hr;
	}

	//
	// Now create the element.
	// Use the Class GUID and Name from the INF file.
	//
	SP_DEVINFO_DATA devInfoData = { sizeof(SP_DEVINFO_DATA) };

	success = SetupDiCreateDeviceInfoW(
		devInfoSet,
		DeviceName ? DeviceName : className,
		&classGuid,
		NULL,
		Owner,
		DICD_GENERATE_ID,
		&devInfoData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	//
	// List of hardware ID's must be double zero-terminated
	//

	WCHAR hardwareIdList[LINE_LEN + 2] = {0};
	
	LPWSTR buffer = hardwareIdList;
	size_t bufferLength = RTL_NUMBER_OF(hardwareIdList);

	hr = StringCchCopyNExW(
		buffer, bufferLength, 
		HardwareId, LINE_LEN, 
		&buffer, &bufferLength,
		STRSAFE_FILL_BEHIND_NULL);

	if (FAILED(hr))
	{
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	// additional terminating null character
	++buffer; --bufferLength;

	// additional terminating null character
	*buffer = L'\0';
	++buffer; --bufferLength;

	size_t hardwareIdListLength = RTL_NUMBER_OF(hardwareIdList) - bufferLength;

	//
	// Add the HardwareID to the Device's HardwareID property.
	//

	success = SetupDiSetDeviceRegistryProperty(
		devInfoSet,
		&devInfoData,
		SPDRP_HARDWAREID,
		(LPBYTE)hardwareIdList,
		hardwareIdListLength * sizeof(WCHAR));

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	//
	// Transform the registry element into an actual devnode
	// in the PnP HW tree.
	//

	success = SetupDiCallClassInstaller(
		DIF_REGISTERDEVICE,
		devInfoSet,
		&devInfoData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	//
	// The element is now registered.
	// You must explicitly remove the device using DIF_REMOVE,
	// if any failure is encountered from now on.
	//

	BOOL rebootRequired = FALSE;

	success = Newdev_UpdateDriverForPlugAndPlayDevicesW(
		Owner,
		HardwareId,
		InfPath,
		UpdateFlags,
		&rebootRequired);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		//
		// We have to remove the registered device on failure
		//
		SetupDiCallClassInstaller(DIF_REMOVE, devInfoSet, &devInfoData);

		SetupDiDestroyDeviceInfoList(devInfoSet);

		return hr;
	}

	ULONG cmStatus, cmProblem;
	CONFIGRET cmRet = CM_Get_DevNode_Status(
		&cmStatus, 
		&cmProblem,
		devInfoData.DevInst,
		0);

	if (CR_SUCCESS == cmRet)
	{
		XTLTRACE1(TRACE_LEVEL_WARNING,
			"DevNodeStatus, status=0x%08X, problem=0x%08X\n",
			cmStatus, cmProblem);

		if (DN_NEED_RESTART & cmStatus)
		{
			rebootRequired = TRUE;
		}
		else if (DN_HAS_PROBLEM & cmStatus)
		{
			rebootRequired = TRUE;
		}
	}

	SetupDiDestroyDeviceInfoList(devInfoSet);

	hr = rebootRequired ? XDI_S_REBOOT : S_OK;

	XTLTRACE1(TRACE_LEVEL_WARNING,
		"Completed, rebootRequired=%d, hr=0x%X\n", rebootRequired, hr);

	return hr;
}

HRESULT
WINAPI
xDiUpdateDeviceDriver(
	__in_opt HWND Owner,
	__in LPCWSTR HardwareId,
	__in LPCWSTR InfFullPath,
	__in DWORD UpdateFlags)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Owner=%p, HardwareId=%ls, InfPath=%ls, Flags=%08X\n",
		Owner, HardwareId, InfFullPath, UpdateFlags);

	HRESULT hr;
	BOOL rebootRequired = FALSE;

	BOOL success = Newdev_UpdateDriverForPlugAndPlayDevicesW(
		Owner,
		HardwareId,
		InfFullPath,
		UpdateFlags,
		&rebootRequired);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		return hr;
	}

	return rebootRequired ? XDI_S_REBOOT : S_OK;
}

typedef struct _XDI_ENUM_CALLBACK_CONTEXT_FOR_REMOVE {
	LPCWSTR TargetHardwareId;
	LPWSTR HardwareIdListBuffer;
	DWORD HardwareIdListBufferSize;
	BOOL PendingReboot;
	LPWSTR AssociatedServiceList;
	DWORD AssociatedServiceListBytes;
	LPWSTR AssociatedInfNameList;
	DWORD AssociatedInfNameListBytes;
} XDI_ENUM_CALLBACK_CONTEXT_FOR_REMOVE, *PXDI_ENUM_CALLBACK_CONTEXT_FOR_REMOVE;

//
// returns S_OK if found, otherwise returns S_FALSE
//
HRESULT
Multisz_FindOneIgnoreCase(
	__in LPCWSTR TargetString,
	__in_bcount(StringListLength) LPCWSTR StringList,
	__in DWORD StringListLength)
{
	for (LPCWSTR p = StringList; 
		*p && p < (StringList + StringListLength); 
		p += lstrlenW(p) + 1)
	{
		if (0 == lstrcmpiW(TargetString, p))
		{
			return S_OK;
		}
	}
	return S_FALSE;
}

SIZE_T
Multisz_Len(
	__in LPCWSTR Multisz)
{
	DWORD len = 0;
	for (LPCWSTR p = Multisz; *p; )
	{
		DWORD l = lstrlenW(p);
		len += l;
		p += l + 1;
	}
	return len;
}

HRESULT
Multisz_Append(
	__deref_inout LPWSTR* Multisz,
	__inout DWORD* AllocatedBytes,
	__in LPCWSTR Value)
{
	LPWSTR p = *Multisz;
	DWORD requiredBytes = *AllocatedBytes;

	//
	// Initially, multisz has a single null characters
	//
	_ASSERT(*AllocatedBytes >= 1 * sizeof(WCHAR));

	DWORD valueLength = lstrlenW(Value);
	requiredBytes += (valueLength + 1) * sizeof(WCHAR);
	PVOID reallocated = CoTaskMemRealloc(p, requiredBytes);
	if (NULL == reallocated)
	{
		return E_OUTOFMEMORY;
	}

	p = (LPWSTR) reallocated;
	*Multisz = p;
	*AllocatedBytes = requiredBytes;

	//
	// Append the string at the final null position
	//
	while (*p)
	{
		p += lstrlenW(p) + 1;
	}

	CopyMemory(p, Value, valueLength * sizeof(WCHAR));
	p += valueLength;
	*p = L'\0'; ++p; // terminate value
	*p = L'\0'; // terminate multisz

	return S_OK;
}

//typedef struct _XDI_REMOVE_HARDWARE_INFO {
//	WCHAR DeviceInstanceId[MAX_DEVICE_ID_LEN];
//	WCHAR ServiceName[MAX_SERVICE_NAME_LEN];
//	WCHAR InfFileName[MAX_PATH];
//} XDI_REMOVE_HARDWARE_INFO, *PXDI_REMOVE_HARDWARE_INFO;

BOOL
CALLBACK
xDipEnumCallbackForRemove(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in PVOID CallbackContext)
{
	BOOL success;
	HRESULT hr;
	DWORD requiredSize = 0;

	PXDI_ENUM_CALLBACK_CONTEXT_FOR_REMOVE context = 
		(PXDI_ENUM_CALLBACK_CONTEXT_FOR_REMOVE) CallbackContext;

	while (TRUE)
	{
		success = SetupDiGetDeviceRegistryPropertyW(
			DeviceInfoSet,
			DeviceInfoData,
			SPDRP_HARDWAREID,
			NULL,
			(PBYTE) context->HardwareIdListBuffer,
			context->HardwareIdListBufferSize,
			&requiredSize);

		if (success)
		{
			break;
		}

		DWORD e = GetLastError();
		switch (e)
		{
		case ERROR_INVALID_DATA:
			// Legacy devices may not have hardware ids
			// We may ignore this error
			return TRUE;
		case ERROR_INSUFFICIENT_BUFFER:
			{
				PVOID p = CoTaskMemRealloc(
					context->HardwareIdListBuffer, requiredSize);
				if (NULL == p)
				{
					// out of memory error, ignore
					return TRUE;
				}
				context->HardwareIdListBuffer = (LPWSTR) p;
				context->HardwareIdListBufferSize = requiredSize;
			}
			continue;
		default:
			// other errors, ignore
			return TRUE;
		}
	}

	//
	// Now we get the hardware id. Compare with the target hardware id
	//

	//
	// Compare each entry in the buffer multi-sz list with our HardwareID.
	//
	
	hr = Multisz_FindOneIgnoreCase(
		context->TargetHardwareId, 
		context->HardwareIdListBuffer,
		context->HardwareIdListBufferSize / sizeof(WCHAR));

	//
	// Multisz_FindOneIgnoreCase returns S_OK or S_FALSE
	// Do not test using SUCCEEDED(hr) as both are evaluated as true
	//

	if (S_OK != hr)
	{
		return TRUE;
	}

	if (context->AssociatedServiceList)
	{
		WCHAR serviceName[MAX_SERVICE_NAME_LEN];
		success = SetupDiGetDeviceRegistryPropertyW(
			DeviceInfoSet,
			DeviceInfoData,
			SPDRP_SERVICE,
			NULL,
			(LPBYTE) serviceName,
			sizeof(serviceName),
			NULL);

		if (success)
		{
			hr = Multisz_FindOneIgnoreCase(
				serviceName, 
				context->AssociatedServiceList, 
				context->AssociatedServiceListBytes / sizeof(WCHAR));
			if (S_OK != hr)
			{
				hr = Multisz_Append(
					&context->AssociatedServiceList,
					&context->AssociatedServiceListBytes,
					serviceName);
			}
		}
	}

	if (context->AssociatedInfNameList)
	{
		HKEY driverKeyHandle = SetupDiOpenDevRegKey(
			DeviceInfoSet,
			DeviceInfoData,
			DICS_FLAG_GLOBAL,
			0,
			DIREG_DRV,
			KEY_READ);

		if (INVALID_HANDLE_VALUE != driverKeyHandle)
		{
			WCHAR infFileName[MAX_PATH];
			DWORD infFileNameSize = sizeof(infFileName);

			ULONG result = RegQueryValueExW(
				driverKeyHandle,
				L"InfPath",
				NULL,
				NULL,
				(LPBYTE) infFileName,
				&infFileNameSize);

			RegCloseKey(driverKeyHandle);

			if (ERROR_SUCCESS == result)
			{
				hr = Multisz_FindOneIgnoreCase(
					infFileName, 
					context->AssociatedInfNameList, 
					context->AssociatedInfNameListBytes / sizeof(WCHAR));
				if (S_OK != hr)
				{
					hr = Multisz_Append(
						&context->AssociatedInfNameList,
						&context->AssociatedInfNameListBytes,
						infFileName);
				}				
			}
		}
	}

	success = SetupDiCallClassInstaller(
		DIF_REMOVE,
		DeviceInfoSet,
		DeviceInfoData);

	if (!success)
	{
		return TRUE;
	}

	SP_DEVINSTALL_PARAMS_W diParams = {sizeof(SP_DEVINSTALL_PARAMS_W)};

	success = SetupDiGetDeviceInstallParamsW(
		DeviceInfoSet,
		DeviceInfoData,
		&diParams);

	if (!success)
	{
		return TRUE;
	}

	context->PendingReboot |= 
		(diParams.Flags & DI_NEEDREBOOT) ||
		(diParams.Flags & DI_NEEDRESTART);

	return TRUE;
}

HRESULT
xDiDeletePnpDriverServices(
	__in LPCWSTR ServiceList)
{
	HRESULT hr;
	BOOL reboot = FALSE;

	SC_HANDLE scManagerHandle = OpenSCManagerW(NULL, NULL, 
		SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_LOCK);

	if (NULL == scManagerHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	SC_LOCK scLock = LockServiceDatabase(scManagerHandle);
	if (NULL == scLock)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		CloseServiceHandle(scManagerHandle);
		return hr;
	}

	for (LPCWSTR serviceName = ServiceList; 
		*serviceName;
		serviceName += lstrlenW(serviceName) + 1)
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION, 
			"Service=%ls\n", serviceName);

		WCHAR serviceEnumKeyPath[MAX_PATH];
		
		hr = StringCchPrintfW(
			serviceEnumKeyPath,
			RTL_NUMBER_OF(serviceEnumKeyPath),
			L"System\\CurrentControlSet\\Services\\%s\\Enum", /* REGSTR_PATH_SERVICES */
			serviceName);

		if (FAILED(hr))
		{
			continue;
		}

		HKEY serviceEnumKeyHandle = (HKEY) INVALID_HANDLE_VALUE;

		LONG result = RegOpenKeyExW(
			HKEY_LOCAL_MACHINE,
			serviceEnumKeyPath,
			0,
			KEY_READ,
			&serviceEnumKeyHandle);

		//
		// If the HKLM\SYSTEM\CurrentControlSet\Services\<servicename>\Enum
		// key does not exist, we assume that the service is safe to delete
		//
		if (ERROR_SUCCESS == result)
		{
			//
			// If the Enum key exists, delete the service if the
			// instance count is zero.
			//

			DWORD instanceCount = 0;
			DWORD instanceCountSize = sizeof(instanceCount);

			result = RegQueryValueExW(
				serviceEnumKeyHandle,
				L"Count",
				NULL,
				NULL,
				(LPBYTE) &instanceCount,
				&instanceCountSize);

			//
			// If failed, we will assume that it is safe to delete
			//
			if (ERROR_SUCCESS == result)
			{
				if (instanceCount != 0)
				{
					//
					// Instance count is not zero. 
					// Do not delete the service
					//
					RegCloseKey(serviceEnumKeyHandle);
					continue;
				}
			}

			RegCloseKey(serviceEnumKeyHandle);
		}

		//
		// Now it is safe to delete the service
		//

		SC_HANDLE serviceHandle = OpenServiceW(scManagerHandle, serviceName, DELETE);

		if (NULL == serviceHandle)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			continue;
		}

		BOOL success = DeleteService(serviceHandle);

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			CloseServiceHandle(serviceHandle);
			continue;
		}

		CloseServiceHandle(serviceHandle);

		//
		// Strangely enough, DeleteService retains the service entry
		// in the database until QueryServiceStatus is called.
		// The following will ensure that the service is deleted,
		// if possible.
		//
		serviceHandle = OpenServiceW(scManagerHandle, serviceName, SERVICE_QUERY_STATUS);
		if (NULL != serviceHandle)
		{
			SERVICE_STATUS serviceStatus;
			QueryServiceStatus(serviceHandle, &serviceStatus);
			CloseServiceHandle(serviceHandle);
		}

		Sleep(0);
		// Sleep(10000);

		if (S_OK == xDiServiceIsMarkedForDeletion(serviceName))
		{
			reboot = TRUE;
		}
	}

	UnlockServiceDatabase(scLock);
	CloseServiceHandle(scManagerHandle);

	return reboot ? XDI_S_REBOOT : S_OK;
}

HRESULT
WINAPI
xDiRemoveDevices(
	__in HWND Owner,
	__in_opt LPCGUID ClassGuid,
	__in_opt LPCWSTR Enumerator,
	__in LPCWSTR HardwareId,
	__in DWORD EnumFlags,
	__out_opt LPWSTR* AssociatedServiceList,
	__out_opt LPWSTR* AssociatedInfNameList)
{
#ifdef _DEBUG

	OLECHAR classGuidString[64] = {0};

	if (ClassGuid)
	{
		StringFromGUID2(*ClassGuid, classGuidString, RTL_NUMBER_OF(classGuidString));
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Owner=%p, ClassGuid=%ls, Enumerator=%ls, "
		"HardwareId=%ls, Flags=%08X)\n",
		Owner, OLE2W(classGuidString), Enumerator, HardwareId, EnumFlags);

#endif

	if (AssociatedServiceList) *AssociatedServiceList = NULL;
	if (AssociatedInfNameList) *AssociatedInfNameList = NULL;

	//
	// Safe-guard to prevent ERROR_FILE_NOT_FOUND error in SETUPAPI
	//
	xDipCreateRunOnceKey();

	XDI_ENUM_CALLBACK_CONTEXT_FOR_REMOVE context = {0};
	context.TargetHardwareId = HardwareId;
	context.HardwareIdListBufferSize = 128;
	context.HardwareIdListBuffer = (LPWSTR) 
		CoTaskMemAlloc(context.HardwareIdListBufferSize);

	if (NULL == context.HardwareIdListBuffer)
	{
		return E_OUTOFMEMORY;
	}

	context.AssociatedServiceList = NULL;
	context.AssociatedServiceListBytes = 0;
	context.AssociatedInfNameList = NULL;
	context.AssociatedInfNameListBytes = 0;

	if (AssociatedServiceList)
	{
		context.AssociatedServiceListBytes = 1 * sizeof(WCHAR);
		context.AssociatedServiceList = (LPWSTR) 
			CoTaskMemAlloc(context.AssociatedServiceListBytes);
		if (NULL == context.AssociatedServiceList)
		{
			CoTaskMemFree(context.HardwareIdListBuffer);
			return E_OUTOFMEMORY;
		}
		ZeroMemory(
			context.AssociatedServiceList,
			context.AssociatedServiceListBytes);
	}

	if (AssociatedInfNameList)
	{
		context.AssociatedInfNameListBytes = 1 * sizeof(WCHAR);
		context.AssociatedInfNameList = (LPWSTR) 
			CoTaskMemAlloc(context.AssociatedInfNameListBytes);
		if (NULL == context.AssociatedInfNameList)
		{
			CoTaskMemFree(context.AssociatedServiceList);
			CoTaskMemFree(context.HardwareIdListBuffer);
			return E_OUTOFMEMORY;
		}
		ZeroMemory(
			context.AssociatedInfNameList,
			context.AssociatedInfNameListBytes);
	}

	HRESULT hr = xDiEnumDevices(
		Owner, ClassGuid, Enumerator, EnumFlags, 
		xDipEnumCallbackForRemove, &context);

	if (SUCCEEDED(hr))
	{
		hr = context.PendingReboot ? XDI_S_REBOOT : S_OK;
	}

	//
	// caller now owns the memory for the service list and the inf name list
	//
	if (AssociatedServiceList) *AssociatedServiceList = context.AssociatedServiceList;
	if (AssociatedInfNameList) *AssociatedInfNameList = context.AssociatedInfNameList;

	//
	// hardware id list buffer is local to this function
	//
	CoTaskMemFree(context.HardwareIdListBuffer);

	return hr;
}

HRESULT
WINAPI
xDiEnumDevices(
	__in HWND Owner,
	__in_opt LPCGUID ClassGuid,
	__in_opt LPCWSTR Enumerator,
	__in DWORD EnumFlags,
	__in XDI_ENUM_CALLBACK EnumCallback,
	__in_opt PVOID EnumCallbackContext)
{
#ifdef _DEBUG

	OLECHAR classGuidString[64] = {0};

	if (ClassGuid)
	{
		StringFromGUID2(*ClassGuid, classGuidString, RTL_NUMBER_OF(classGuidString));
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"xDiRemoveDevices(Owner=%p, ClassGuid=%ls, Enumerator=%ls, "
		"Flags=%08X)\n",
		Owner, OLE2W(classGuidString), Enumerator, EnumFlags);

#endif

	HRESULT hr;
	//
	// Create a Device Information Set with all present devices.
	//
	HDEVINFO devInfoSet = SetupDiGetClassDevsW(
		ClassGuid,
		Enumerator,
		Owner,
		EnumFlags);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		return hr;
	}

	SP_DEVINFO_DATA devInfoData = {sizeof(SP_DEVINFO_DATA)};

	for (DWORD index = 0; 
		SetupDiEnumDeviceInfo(devInfoSet, index, &devInfoData);
		++index)
	{
		BOOL cont = (*EnumCallback)(devInfoSet, &devInfoData, EnumCallbackContext);
		if (!cont)
		{
			SetLastError(ERROR_NO_MORE_ITEMS);
			break;
		}
	}

	DWORD se = GetLastError();
	hr = (ERROR_NO_MORE_ITEMS == se) ? S_OK : HRESULT_FROM_SETUPAPI(se);

	SetupDiDestroyDeviceInfoList(devInfoSet);

	return hr;
}

HRESULT
xDiServiceIsMarkedForDeletion(
	__in LPCWSTR ServiceName)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"ServiceName=%ls\n", ServiceName);

	WCHAR keyName[MAX_PATH];

	HRESULT hr = StringCchPrintfW(
		keyName, RTL_NUMBER_OF(keyName), 
		REGSTR_PATH_SERVICES _T("\\%s"), 
		ServiceName);

	if (FAILED(hr))
	{
		XTLTRACE1(TRACE_LEVEL_ERROR, "StringCchPrintf failed, hr=0x%x\n", hr);
		return hr;
	}

	HKEY keyHandle = (HKEY) INVALID_HANDLE_VALUE;

	LONG result = RegOpenKeyExW(
		HKEY_LOCAL_MACHINE, 
		keyName, 
		0, 
		KEY_QUERY_VALUE,
		&keyHandle);

	if (ERROR_SUCCESS != result) 
	{
		if (ERROR_FILE_NOT_FOUND == result)
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, 
				"Service key does not exist, service=%ls\n", ServiceName);
			//
			// If the key does not exist, 
			// the service is not marked for deletion
			// Hence we returns S_FALSE
			//
			return S_FALSE;
		}
		hr = HRESULT_FROM_WIN32(result);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"RegOpenKeyExW failed, hr=0x%x, service=%ls\n", hr, ServiceName);

		return hr;
	}

	DWORD flags;
	DWORD flagsSize = sizeof(DWORD);

	result = RegQueryValueExW(
		keyHandle,
		L"DeleteFlag",
		0, 
		NULL, 
		(LPBYTE) &flags, 
		&flagsSize);

	if (ERROR_SUCCESS != result)
	{
		if (ERROR_FILE_NOT_FOUND == result)
		{
			//
			// If the value does not exist, 
			// the service is not marked for deletion
			// Hence we returns S_FALSE
			//
			XTLTRACE1(TRACE_LEVEL_INFORMATION, 
				"DeleteFlag value does not exist, service=%ls\n", ServiceName);

			return S_FALSE;
		}
		RegCloseKey(keyHandle);

		hr = HRESULT_FROM_WIN32(result);

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"RegQueryValueExW failed, hr=0x%x, service=%ls\n", hr, ServiceName);

		return hr;
	}

	RegCloseKey(keyHandle);

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"DeleteFlag=0x%x, service=%ls\n", flags, ServiceName);

	return (flags != 0) ? S_OK : S_FALSE;
}

BOOL CALLBACK
xDipEnumDriverFilesCallbackForDelete(
	__in LPCWSTR TargetFilePath,
	__in DWORD Flags,
	__in LPVOID Context)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"TargetFilePath=%ls, Flags=%08X\n",
		TargetFilePath, Flags);

	UNREFERENCED_PARAMETER(Flags);

	LPBOOL reboot = (LPBOOL) Context;
	LPCWSTR fileName = PathFindFileNameW(TargetFilePath);
	
	//
	// We should not delete WDF co-installers (special case)
	//

	WCHAR WdfCoinstallerPrefix[] = L"wdfcoinstaller";

	if (0 == _wcsnicmp(
		WdfCoinstallerPrefix, 
		fileName, 
		RTL_NUMBER_OF(WdfCoinstallerPrefix) - 1))
	{
		return TRUE;
	}

	if (!DeleteFileW(TargetFilePath))
	{
		XTLTRACE1(TRACE_LEVEL_WARNING, 
			"DeleteFile(%ls) failed->Delay, error=0x%X\n", TargetFilePath, GetLastError());

		if (!MoveFileExW(TargetFilePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT))
		{
			XTLTRACE1(TRACE_LEVEL_WARNING, 
				"MoveFileExW failed, error=0x%X\n", GetLastError());
		}

		*reboot = TRUE;
	}

	return TRUE;
}

HRESULT
xDiDeleteDriverFiles(
	__in_opt HWND Owner,
	__in LPCWSTR InfPath,
	__in DWORD Flags)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Owner=%p, Inf=%ls, Flags=%08X\n",
		Owner, InfPath, Flags);

	BOOL reboot = FALSE;
	HRESULT hr = xDiEnumDriverFiles(
		Owner,
		InfPath,
		Flags,
		xDipEnumDriverFilesCallbackForDelete,
		&reboot);
	if (FAILED(hr))
	{
		return hr;
	}
	return (reboot) ? XDI_S_REBOOT : S_OK;
}

HRESULT
xDiUninstallOEMInf(
	__in LPCWSTR InfFileName,
	__in DWORD Flags)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Inf=%ls, Flags=%08X\n",
		InfFileName, Flags);

	LPCWSTR fileNameSpec = PathFindFileNameW(InfFileName);
	if (!Setupapi_SetupUninstallOEMInfW(fileNameSpec, Flags, 0))
	{
		return HRESULT_FROM_SETUPAPI(GetLastError());
	}
	return S_OK;
}

HRESULT
xDiStartService(
	__in SC_HANDLE SCManagerHandle,
	__in SC_HANDLE ServiceHandle,
	__in DWORD Timeout)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"SCManagerHandle=%p, ServiceHandle=%p, Timeout=%d\n",
		SCManagerHandle, ServiceHandle, Timeout);

	HRESULT hr;
	BOOL success;

	DWORD bytesNeeded;
	SERVICE_STATUS_PROCESS ssp;

	success = QueryServiceStatusEx(
		ServiceHandle,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE) &ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&bytesNeeded);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"QueryServiceStatusEx failed, scHandle=%p, hr=0x%x\n", ServiceHandle, hr);
		return hr;
	}

	if (SERVICE_START_PENDING != ssp.dwCurrentState &&
		SERVICE_RUNNING != ssp.dwCurrentState)
	{
		success = StartServiceW(ServiceHandle, 0, NULL);
		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"StartServiceW failed, scHandle=%p, hr=0x%x\n", ServiceHandle, hr);
			return hr;
		}
	}

	//
	// Check the status until the service is no longer start pending
	//

	success = QueryServiceStatusEx(
		ServiceHandle,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE) &ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&bytesNeeded);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"QueryServiceStatusEx failed, scHandle=%p, hr=0x%x\n", ServiceHandle, hr);
		return hr;
	}

	//
	// Save the tick count and initial checkpoint.
	//

	DWORD startTickCount = GetTickCount();
	DWORD oldCheckPoint = ssp.dwCheckPoint;

	while (SERVICE_START_PENDING == ssp.dwCurrentState) 
	{ 
		//
		// Do not wait longer than the wait hint. A good interval is 
		// one tenth the wait hint, but no less than 1 second and no 
		// more than 10 seconds. 
		//

		DWORD waitTime = ssp.dwWaitHint / 10;

		if( waitTime < 1000 )
		{
			waitTime = 1000;
		}
		else if ( waitTime > 10000 )
		{
			waitTime = 10000;
		}

		Sleep(waitTime);

		//
		// Check the status again. 
		//
		success = QueryServiceStatusEx(
			ServiceHandle,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE) &ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&bytesNeeded);

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE1(TRACE_LEVEL_ERROR, 
				"QueryServiceStatusEx failed, scHandle=%p, hr=0x%x\n", ServiceHandle, hr);
			break;
		}

		if (ssp.dwCheckPoint > oldCheckPoint)
		{
			//
			// The service is making progress.
			//
			startTickCount = GetTickCount();
			oldCheckPoint = ssp.dwCheckPoint;
		}
		else
		{
			if (GetTickCount() - startTickCount > ssp.dwWaitHint)
			{
				//
				// No progress made within the wait hint
				//
				hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
				XTLTRACE1(TRACE_LEVEL_ERROR,
					"Timed out waiting for the service started, scHandle=%p\n", ServiceHandle);
				break;
			}
		}
	} 

	if (ssp.dwCurrentState == SERVICE_RUNNING) 
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"Service is now running, scHandle=%p\n", ServiceHandle);

		return S_OK;
	}
	else
	{
		XTLTRACE1(TRACE_LEVEL_WARNING,
			"Service is still not running, scHandle=%p\n", ServiceHandle);

		return hr;
	}
}

HRESULT
xDiStopService(
	__in SC_HANDLE SCManagerHandle,
	__in SC_HANDLE ServiceHandle,
	__in BOOL StopDependencies,
	__in DWORD Timeout)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"SCManagerHandle=%p, ServiceHandle=%p, StopDeps=%d, Timeout=%d\n",
		SCManagerHandle, ServiceHandle, StopDependencies, Timeout);

	HRESULT hr;
	BOOL success;
	SERVICE_STATUS_PROCESS ssp;
	DWORD startTime = GetTickCount();
	DWORD bytesNeeded;

	//
	// Make sure the service is not already stopped
	//

	success = QueryServiceStatusEx(
		ServiceHandle,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&bytesNeeded);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"QueryServiceStatusEx failed, scHandle=%p, hr=0x%x\n", ServiceHandle, hr);
		return hr;
	}

	if (SERVICE_STOPPED == ssp.dwCurrentState)
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"Service is already stopped, scHandle=%p\n", ServiceHandle);
		return S_OK;
	}

	//
	// If a stop is pending, just wait for it
	//
	while (SERVICE_STOP_PENDING == ssp.dwCurrentState) 
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"Service is already stopping, scHandle=%p\n", ServiceHandle);

		DWORD wait = min(max(ssp.dwWaitHint, 500), 5000);

		Sleep(wait);
		
		success = QueryServiceStatusEx(
			ServiceHandle,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&bytesNeeded);

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"QueryServiceStatusEx failed, scHandle=%p, hr=0x%x\n", ServiceHandle, hr);
			return hr;
		}
		
		if (SERVICE_STOPPED == ssp.dwCurrentState)
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION,
				"Service stopped, scHandle=%p\n", ServiceHandle);
			return S_OK;
		}

		if (GetTickCount() - startTime > Timeout)
		{
			hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"Stop service timed out in %d ms, scHandle=%p\n", Timeout, ServiceHandle);
			return hr;
		}
	}

	//
	// If the service is running, dependencies must be stopped first
	//
	if ( StopDependencies ) 
	{
		DWORD depCount;
		LPENUM_SERVICE_STATUS dependencies = NULL;
		ENUM_SERVICE_STATUS ess;

		//
		// Pass a zero-length buffer to get the required buffer size
		//
		success = EnumDependentServicesW(
			ServiceHandle,
			SERVICE_ACTIVE,
			dependencies, 
			0, 
			&bytesNeeded, 
			&depCount);

		//
		// If the Enum call succeeds, then there are no dependent
		// services so do nothing
		//
		if (!success)
		{
			if (ERROR_MORE_DATA != GetLastError())
			{
				//
				// Unexpected error
				//
				hr = HRESULT_FROM_WIN32(GetLastError());
				XTLTRACE1(TRACE_LEVEL_ERROR,
					"EnumDependentServicesW failed, hr=0x%x.\n", hr);
				return hr;
			}

			// Allocate a buffer for the dependencies
			dependencies = (LPENUM_SERVICE_STATUS) calloc(bytesNeeded, sizeof(BYTE));

			if (NULL == dependencies)
			{
				hr = E_OUTOFMEMORY;
				XTLTRACE1(TRACE_LEVEL_ERROR,
					"Out of memory, %d bytes\n", bytesNeeded);
				return hr;
			}

			__try {
				//
				// Enumerate the dependencies
				//
				success = EnumDependentServicesW(
					ServiceHandle,
					SERVICE_ACTIVE,
					dependencies,
					bytesNeeded,
					&bytesNeeded,
					&depCount);

				if (!success)
				{
					//
					// Unexpected error
					//
					hr = HRESULT_FROM_WIN32(GetLastError());
					XTLTRACE1(TRACE_LEVEL_ERROR,
						"EnumDependentServicesW failed, hr=0x%x.\n", hr);
					return hr;
				}

				for (DWORD i = 0; i < depCount; i++ ) 
				{
					ess = *(dependencies + i);

					//
					// Open the service
					//
					SC_HANDLE depServiceHandle = OpenServiceW(
						SCManagerHandle,
						ess.lpServiceName,
						SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);

					if (NULL == depServiceHandle)
					{
						hr = HRESULT_FROM_WIN32(GetLastError());
						XTLTRACE1(TRACE_LEVEL_ERROR,
							"OpenServiceW(%ls) failed, hr=0x%x.\n", ess.lpServiceName, hr);
						return hr;
					}

					__try 
					{
						hr = xDiStopService(SCManagerHandle, depServiceHandle, TRUE, Timeout);

						if (FAILED(hr))
						{
							XTLTRACE1(TRACE_LEVEL_ERROR,
								"xDipStopService(%ls) failed, hr=0x%x.\n", ess.lpServiceName, hr);
							return hr;
						}
					} 
					__finally 
					{
						//
						// Always release the service handle
						//
						CloseServiceHandle( depServiceHandle );
					}
				}

			} 
			__finally 
			{
				// Always free the enumeration buffer
				free(dependencies);
			}
		} 
	}

	//
	// Send a stop code to the main service
	//
	success = ControlService(
		ServiceHandle, 
		SERVICE_CONTROL_STOP, 
		(LPSERVICE_STATUS) &ssp);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"ControlService failed, scHandle=%p, hr=0x%x.\n", ServiceHandle, hr);
		return hr;
	}

	//
	// Wait for the service to stop
	//

	while (SERVICE_STOP_PENDING == ssp.dwCurrentState) 
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION,
			"Service is stopping, scHandle=%p\n", ServiceHandle);

		Sleep( ssp.dwWaitHint );

		success = QueryServiceStatusEx( 
			ServiceHandle, 
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp, 
			sizeof(SERVICE_STATUS_PROCESS),
			&bytesNeeded);

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"QueryServiceStatusEx failed, scHandle=%p, hr=0x%x.\n", ServiceHandle, hr);
			return hr;
		}

		if (ssp.dwCurrentState == SERVICE_STOPPED)
		{
			break;
		}

		if (GetTickCount() - startTime > Timeout)
		{
			hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
			XTLTRACE1(TRACE_LEVEL_ERROR,
				"Stop service timed out in %d ms, scHandle=%p\n", Timeout, ServiceHandle);
			return hr;
		}
	}

	return S_OK;
}

HRESULT
xDipCreateRunOnceKey()
{
	HRESULT hr;
	HKEY keyHandle;
	LONG result = RegCreateKeyExW(
		HKEY_LOCAL_MACHINE,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ,
		NULL,
		&keyHandle,
		NULL);

	if (ERROR_SUCCESS != result)
	{
		hr = HRESULT_FROM_WIN32(result);
		return hr;
	}
	RegCloseKey(keyHandle);
	return S_OK;
}

HRESULT
xDipContainsHardwareId(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in LPCWSTR HardwareId)
{
	LPWSTR hardwareIdList = NULL;
	DWORD hardwareIdListSize = 0;

realloced:

	BOOL success = SetupDiGetDeviceRegistryPropertyW(
		DeviceInfoSet,
		DeviceInfoData,
		SPDRP_HARDWAREID,
		NULL,
		(PBYTE) hardwareIdList,
		hardwareIdListSize,
		&hardwareIdListSize);

	if (!success && ERROR_INSUFFICIENT_BUFFER == GetLastError())
	{
		PVOID p = realloc(hardwareIdList, hardwareIdListSize);
		if (NULL == p)
		{
			// out of memory error, ignore
			free(hardwareIdList);
			return E_OUTOFMEMORY;
		}
		hardwareIdList = (LPWSTR) p;
		goto realloced;
	}

	HRESULT hr = Multisz_FindOneIgnoreCase(
		HardwareId, 
		hardwareIdList, 
		hardwareIdListSize / sizeof(WCHAR));

	free(hardwareIdList);

	return hr;
}
