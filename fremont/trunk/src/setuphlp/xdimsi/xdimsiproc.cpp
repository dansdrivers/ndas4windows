#include "precomp.hpp"
#include <xdi.h>
#include "xmsitrace.h"
#include "xmsiutil.h"
#include "xdimsiproc.h"
#include "xdimsiprocdata.h"

HRESULT
xDiMsipGetInfPathFromRegistry(
	__in MSIHANDLE hInstall,
	__in DWORD RegRoot,
	__in LPCWSTR RegKey,
	__in LPCWSTR RegName,
	__deref_out LPWSTR* InfPath);

HRESULT
xDiMsipSetInfPathInRegistry(
	__in MSIHANDLE hInstall,
	__in LPCWSTR InfPath,
	__in DWORD RegRoot,
	__in LPCWSTR RegKey,
	__in LPCWSTR RegName);

HRESULT
xDiMsipDeleteInfPathFromRegistry(
	__in MSIHANDLE hInstall,
	__in DWORD RegRoot,
	__in LPCWSTR RegKey,
	__in LPCWSTR RegName);

typedef struct _XDIMSI_DIALOG_ACTIVATOR_CONTEXT {
	HANDLE StopEventHandle;
	HANDLE ThreadHandle;
	DWORD Interval;
} XDIMSI_DIALOG_ACTIVATOR_CONTEXT, *PXDIMSI_DIALOG_ACTIVATOR_CONTEXT;

HRESULT
xDiMsipDialogActivatorStart(
	__out PXDIMSI_DIALOG_ACTIVATOR_CONTEXT ActivatorContext);

HRESULT
xDiMsipDialogActivatorStop(
	__in PXDIMSI_DIALOG_ACTIVATOR_CONTEXT ActivatorContext);

DWORD
WINAPI
xDiMsipDialogActivatorThreadProc(
	LPVOID Context);

FORCEINLINE LPCWSTR pEmptyToNullW(LPCWSTR String)
{
	if (NULL == String) return NULL;
	if (L'\0' == String[0]) return NULL;
	return String;
}

UINT
xDiMsipInitializeScheduledAction(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	XMSITRACEH(hInstall, L"pxMsiInitializeScheduledAction started...\n"); 

	pxMsiClearForceReboot();
	pxMsiClearScheduleReboot();

	XMSITRACEH(hInstall, L"pxMsiInitializeScheduledAction finished...\n"); 

	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallFromInfSection(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	//
	// InfSection: <InstallSection>[;<RollbackSection>]
	//

	WCHAR section[128];
	HRESULT hr = StringCchCopy(section, RTL_NUMBER_OF(section), ProcessRecord->InfSectionList);
	_ASSERT(SUCCEEDED(hr));

	LPWSTR rollbackSection = NULL;
	for (LPWSTR p = section; *p != NULL; ++p)
	{
		if (L';' == *p)
		{
			*p = L'\0';
			rollbackSection = p + 1;
			break;
		}
	}

	XMSITRACEH(hInstall, L"InstallFromInfSection: Section=%ls, RollbackSection=%ls\n", 
		section, rollbackSection);

	LPWSTR processingSection = section;

	if (MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK))
	{
		if (NULL == rollbackSection)
		{
			return ERROR_SUCCESS;
		}
		processingSection = rollbackSection;
	}

	while (TRUE)
	{
		BOOL needsReboot = FALSE, needsRebootForService = FALSE;

		hr = xDiInstallFromInfSection(
			NULL,
			ProcessRecord->InfPath,
			processingSection,
			0,
			0,
			&needsReboot,
			&needsRebootForService);

		if (FAILED(hr))
		{
			XMSITRACEH(hInstall, L"InstallFromInf failed, hr=0x%x\n", hr);

			if (!MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK))
			{
				UINT response = pxMsiErrorMessageBox(
					hInstall, ProcessRecord->ErrorNumber, hr);
				switch (response)
				{
				case IDRETRY:
					continue;
				case IDIGNORE:
					break;
				case 0:
				case IDABORT:
				default:
					return ERROR_INSTALL_FAILURE;
				}
			}
		}

		if (needsReboot)
		{
			XMSITRACEH(hInstall, L"Reboot required for the file operation.\n");
			if (ProcessRecord->Flags & XDIMSI_INSTALL_INF_FLAGS_IGNORE_FILE_REBOOT)
			{
				XMSITRACEH(hInstall, L"Dismissed reboot request for files.\n");
			}
			else
			{
				pxMsiQueueScheduleReboot(hInstall);
			}
		}

		if (needsRebootForService)
		{
			XMSITRACEH(hInstall, L"Reboot required for the service installation.\n");
			if (ProcessRecord->Flags & XDIMSI_INSTALL_INF_FLAGS_IGNORE_SERVICE_REBOOT)
			{
				XMSITRACEH(hInstall, L"Dismissed reboot request for the service.\n");
			}
			else
			{
				pxMsiQueueScheduleReboot(hInstall);
			}
		}

		break;
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallLegacyPnpDeviceRollback(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	XMSITRACEH(hInstall, L"Rollback in pxMsiInstallLegacyPnpDeviceRollback\n");

	LPWSTR oemInfPath = NULL;

	HRESULT hr = xDiMsipGetInfPathFromRegistry(
		hInstall,
		ProcessRecord->RegRoot,
		ProcessRecord->RegKey,
		ProcessRecord->RegName,
		&oemInfPath);
	
	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"pxMsiGetInfPathFromRegistry failed, hr=0x%x\n", hr);
		//
		// OEM INF is not copied yet. No further processing is required.
		//
		XMSITRACEH(hInstall, L"No actions are necessary for rollback, hardwareId=%ls\n",
			ProcessRecord->HardwareId);
		return ERROR_SUCCESS;
	}

	hr = xDiMsipDeleteInfPathFromRegistry(
		hInstall, 		
		ProcessRecord->RegRoot,
		ProcessRecord->RegKey,
		ProcessRecord->RegName);

	XMSITRACEH(hInstall, L"DeleteInfPathFromRegistry, hr=0x%x\n", hr);

	LPWSTR serviceList = NULL;
	LPWSTR oemInfNameList = NULL;

	hr = xDiRemoveDevices(
		NULL, 
		NULL, 
		NULL, 
		ProcessRecord->HardwareId, 
		DIGCF_ALLCLASSES,
		&serviceList,
		&oemInfNameList);

	XMSITRACEH(hInstall, L"DiRemoveDevices, hardwareId=%ls, hr=0x%x\n", 
		ProcessRecord->HardwareId, hr);

	hr = xDiDeletePnpDriverServices(serviceList);

	XMSITRACEH(hInstall, L"DiDeletePnpDriverServices, serviceList=%ls, hr=0x%x\n", 
		serviceList, hr);

	hr = xDiDeleteDriverFiles(NULL, oemInfPath, XDI_EDF_NO_CLASS_INSTALLER);

	XMSITRACEH(hInstall, L"DiDeleteDriverFiles, inf=%ls, hr=0x%x\n", 
		oemInfPath, hr);

	hr = xDiUninstallOEMInf(oemInfPath, 0);

	XMSITRACEH(hInstall, L"DiUninstallOEMInf, inf=%ls, hr=0x%x\n", 
		oemInfPath, hr);
	
	CoTaskMemFree(serviceList);
	CoTaskMemFree(oemInfNameList);

	CoTaskMemFree(oemInfPath);

	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallLegacyPnpDevice(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	BOOL success = FALSE;
	BOOL reboot = FALSE;

	HRESULT hr;

	while (TRUE) 
	{
		WCHAR oemInfPath[MAX_PATH];
		DWORD oemInfPathLength = 0;
		LPWSTR oemInfName = NULL;

		XDIMSI_DIALOG_ACTIVATOR_CONTEXT daContext;

		xDiMsipDialogActivatorStart(&daContext);

		success = SetupCopyOEMInfW(
			ProcessRecord->InfPath,
			NULL,
			SPOST_PATH,
			0,
			oemInfPath,
			RTL_NUMBER_OF(oemInfPath),
			&oemInfPathLength,
			&oemInfName);

		xDiMsipDialogActivatorStop(&daContext);

		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			XMSITRACEH(hInstall, L"SetupCopyOEMInf(%s) failed, hr=0x%x\n", hr);
		}
		else
		{
			XMSITRACEH(hInstall, L"hardwareId=%ls, oemInfPath=%ls\n", 
				ProcessRecord->HardwareId, oemInfPath);

			hr = xDiMsipSetInfPathInRegistry(
				hInstall, 
				oemInfPath, 
				ProcessRecord->RegRoot, 
				ProcessRecord->RegKey, 
				ProcessRecord->RegName);

			if (FAILED(hr))
			{
				XMSITRACEH(hInstall, L"pxMsiSetInfPathInRegistry failed, hr=0x%x\n", hr);
			}

			//
			// Specifying INSTALLFLAG_FORCE will overwrite the
			// existing device driver with the current one
			// irrespective of existence of higher version
			//
			xDiMsipDialogActivatorStart(&daContext);

			reboot = FALSE;

			hr = xDiInstallLegacyPnpDevice(
				NULL,
				ProcessRecord->HardwareId,
				ProcessRecord->InfPath,
				NULL,
				ProcessRecord->Flags,
				INSTALLFLAG_FORCE);

			xDiMsipDialogActivatorStop(&daContext);

			if (FAILED(hr))
			{
				XMSITRACEH(hInstall, L"xDiInstallLegacyPnpDevice failed, hr=0x%x\n", hr);
			}
			else if (XDI_S_REBOOT == hr)
			{
				reboot  = TRUE;
			}
		}

		if (FAILED(hr)) 
		{
			UINT response = pxMsiErrorMessageBox(hInstall, ProcessRecord->ErrorNumber, hr);
			switch (response)
			{
			case IDRETRY:
				continue;
			case IDIGNORE:
				break;
			case 0:
			case IDABORT:
			default:
				return ERROR_INSTALL_FAILURE;
			}
		}

		break;
	}

	if (reboot) 
	{
		pxMsiQueueScheduleReboot(hInstall);
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipUninstallPnpDevice(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	BOOL reboot = FALSE;
	BOOL success = FALSE;

	while (TRUE) 
	{
		LPWSTR serviceList = NULL;
		LPWSTR infNameList = NULL;

		HRESULT hr = xDiRemoveDevices(
			NULL,
			NULL,
			pEmptyToNullW(ProcessRecord->InfPath),
			ProcessRecord->HardwareId,
			DIGCF_ALLCLASSES,
			&serviceList,
			&infNameList);

		if (FAILED(hr) && 0 != ProcessRecord->ErrorNumber)
		{
			XMSITRACEH(hInstall, 
				L"NdasDiRemoveDevice failed, hardwareId=%s, hr=0x%x\n",
				ProcessRecord->HardwareId, hr);

			UINT response = pxMsiErrorMessageBox(hInstall, ProcessRecord->ErrorNumber, hr);
			switch (response)
			{
			case IDRETRY:
				continue;
			case IDIGNORE:
				break;
			case 0:
			case IDABORT:
			default:
				return ERROR_INSTALL_FAILURE;
			}
		}

		if (XDI_S_REBOOT == hr)
		{
			reboot  = TRUE;
		}

		hr = xDiDeletePnpDriverServices(serviceList);

		if (XDI_S_REBOOT == hr)
		{
			reboot = TRUE;
		}

		for (LPCWSTR infName = infNameList; 
			*infName; 
			infName += lstrlenW(infName) + 1)
		{
			WCHAR infPath[MAX_PATH];

			GetWindowsDirectoryW(infPath, RTL_NUMBER_OF(infPath));
			PathAppendW(infPath, L"INF");
			PathAppend(infPath, infName);

			xDiDeleteDriverFiles(NULL, infPath, XDI_EDF_NO_CLASS_INSTALLER);
			xDiUninstallOEMInf(infNameList, 0);
		}

		break;
	}

	if (reboot) 
	{
		pxMsiQueueScheduleReboot(hInstall);
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallNetworkComponentRollback(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	XMSITRACEH(hInstall, L"Rollback in pxMsiInstallNetworkComponentRollback\n");

	LPWSTR oemInfPath = NULL;

	HRESULT hr = xDiMsipGetInfPathFromRegistry(
		hInstall,
		ProcessRecord->RegRoot,
		ProcessRecord->RegKey,
		ProcessRecord->RegName,
		&oemInfPath);
	
	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"pxMsiGetInfPathFromRegistry failed, hr=0x%x\n", hr);
		//
		// OEM INF is not copied yet. No further processing is required.
		//
		XMSITRACEH(hInstall, L"No actions are necessary for rollback, hardwareId=%ls\n",
			ProcessRecord->HardwareId);
		return ERROR_SUCCESS;
	}

	hr = xDiMsipDeleteInfPathFromRegistry(
		hInstall, 		
		ProcessRecord->RegRoot,
		ProcessRecord->RegKey,
		ProcessRecord->RegName);

	XMSITRACEH(hInstall, L"DeleteInfPathFromRegistry, hr=0x%x\n", hr);

	hr = xDiUninstallNetComponent(
		ProcessRecord->HardwareId,
		15 * 1000,
		L"Windows Installer",
		NULL,
		NULL);

	XMSITRACEH(hInstall, L"DiUninstallNetComponent, componentId=%ls, hr=0x%x\n", 
		ProcessRecord->HardwareId, hr);

	hr = xDiDeleteDriverFiles(NULL, oemInfPath, XDI_EDF_NO_CLASS_INSTALLER);

	XMSITRACEH(hInstall, L"DiDeleteDriverFiles, inf=%ls, hr=0x%x\n", 
		oemInfPath, hr);

	hr = xDiUninstallOEMInf(oemInfPath, 0);

	XMSITRACEH(hInstall, L"DiUninstallOEMInf, inf=%ls, hr=0x%x\n", oemInfPath, hr);
	
	CoTaskMemFree(oemInfPath);

	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallNetworkComponent(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	//
	// InfPath: <inf-path>   e.g. c:\program files\ndas\drivers\netlpx.inf
	// Param: <component-id> e.g. nkc_lpx
	//

	HRESULT hr;
	GUID classGuid;
	WCHAR className[MAX_CLASS_NAME_LEN];

	BOOL success = SetupDiGetINFClassW(
		ProcessRecord->InfPath,
		&classGuid,
		className,
		MAX_CLASS_NAME_LEN,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		//
		// Invalid Class GUID in the inf file
		//
		XMSITRACEH(hInstall, L"SetupDiGetINFClass failed, hr=0x%x\n", hr);
		return ERROR_INSTALL_FAILURE;
	}

	//
	// Pre-install the OEM INF file to trigger the installation of
	// the component by the component id later (xDiInstallNetComponent)
	//

	WCHAR oemInfPath[MAX_PATH];
	DWORD oemInfPathLength;
	LPWSTR oemInfName;

	XDIMSI_DIALOG_ACTIVATOR_CONTEXT daContext;

	xDiMsipDialogActivatorStart(&daContext);

	success = SetupCopyOEMInfW(
		ProcessRecord->InfPath,
		NULL,
		SPOST_PATH,
		0,
		oemInfPath,
		RTL_NUMBER_OF(oemInfPath),
		NULL,
		NULL);

	xDiMsipDialogActivatorStop(&daContext);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XMSITRACEH(hInstall, L"SetupCopyOEMInf failed, hr=0x%x\n", hr);
		return ERROR_INSTALL_FAILURE;
	}

	hr = xDiMsipSetInfPathInRegistry(
		hInstall, 
		oemInfPath, 
		ProcessRecord->RegRoot, 
		ProcessRecord->RegKey, 
		ProcessRecord->RegName);

	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"pxMsiSetInfPathInRegistry failed, hr=0x%x\n", hr);
	}

	while (TRUE)
	{
		xDiMsipDialogActivatorStart(&daContext);

		hr = xDiInstallNetComponent(
			&classGuid,
			ProcessRecord->HardwareId, 
			15*1000,
			L"Windows Installer", 
			NULL);

		xDiMsipDialogActivatorStop(&daContext);

		if (FAILED(hr))
		{
			XMSITRACEH(hInstall, L"InstallNetComponent failed, hr=0x%x\n", hr);

			// 0, IDABORT, IDRETRY, IDIGNORE
			UINT response = pxMsiErrorMessageBox(hInstall, ProcessRecord->ErrorNumber, hr);
			switch (response)
			{
			case IDRETRY:
				continue;
			case IDIGNORE:
				break;
			case 0:
			case IDABORT:
			default:
				return ERROR_INSTALL_FAILURE;
			}
		}

		break;
	}

	//
	// The followings actions are not critical ones
	// Just log errors if any and return ERROR_SUCCESS
	//

	if (XDI_S_REBOOT == hr) 
	{
		pxMsiQueueScheduleReboot(hInstall);
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipUninstallNetworkComponent(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	while (TRUE)
	{
		XMSITRACEH(hInstall, L"Hardware Id=%s\n", ProcessRecord->HardwareId);

		LPWSTR oemInfName = NULL;
		HRESULT hr = xDiUninstallNetComponent(
			ProcessRecord->HardwareId,
			15 * 1000,
			L"Windows Installer",
			NULL,
			&oemInfName);

		if (FAILED(hr)) 
		{
			XMSITRACEH(hInstall, L"UninstallNetComponent(%s) failed, hr=0x%x\n", 
				ProcessRecord->HardwareId, hr);

			UINT response = pxMsiErrorMessageBox(hInstall, ProcessRecord->ErrorNumber, hr);
			switch (response)
			{
			case IDRETRY:
				continue;
			case IDIGNORE:
				break;
			case 0:
			case IDABORT:
			default:
				return ERROR_INSTALL_FAILURE;
			}
		}

		XMSITRACEH(hInstall, L"OEM INF=%s\n", oemInfName);

		//
		// S_FALSE may be returned in case that the component is not installed
		// at all.
		//

		if (S_FALSE != hr)
		{
			if (XDI_S_REBOOT == hr)
			{
				pxMsiQueueScheduleReboot(hInstall);
			}

			WCHAR oemInfPath[MAX_PATH];
			GetWindowsDirectoryW(oemInfPath, RTL_NUMBER_OF(oemInfPath));
			PathAppendW(oemInfPath, L"INF");
			PathAppendW(oemInfPath, oemInfName);

			//
			// See if any services in the INF file is marked for pending deletion
			//

			LPWSTR stalledServiceName = NULL;
			
			hr = xDiServiceIsMarkedForDeletionFromPnpInf(
				oemInfPath, 
				ProcessRecord->HardwareId, 
				&stalledServiceName);

			if (S_OK == hr)
			{
				XMSITRACEH(hInstall, L"Service deletion requires the reboot, service=%ls\n",
					stalledServiceName);
				pxMsiQueueScheduleReboot(hInstall);
			}

			CoTaskMemFree(stalledServiceName);

			//
			// Delete driver files
			//

			hr = xDiDeleteDriverFiles(NULL, oemInfPath, XDI_EDF_NO_CLASS_INSTALLER);
			if (SUCCEEDED(hr))
			{
				if (XDI_S_REBOOT == hr) 
				{
					pxMsiQueueScheduleReboot(hInstall);
				}

				hr = xDiUninstallOEMInf(oemInfName, 0);
				if (FAILED(hr))
				{
					XMSITRACEH(hInstall, L"UninstallOEMInf(%s) failed, hr=0x%x\n", 
						oemInfName, hr);
				}
				else
				{
					if (XDI_S_REBOOT == hr)
					{
						pxMsiQueueScheduleReboot(hInstall);
					}
				}
			}
			else
			{
				XMSITRACEH(hInstall, 
					L"DeleteDriverFiles(%s) failed, hr=0x%x\n", oemInfPath, hr);
			}
		}

		if (NULL != oemInfName)
		{
			CoTaskMemFree(oemInfName);
		}

		break;
	}
	
	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallPnpDeviceInfRollback(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	XMSITRACEH(hInstall, L"Rollback in pxMsiInstallPnpDeviceInfRollback\n");

	LPWSTR oemInfPath = NULL;

	HRESULT hr = xDiMsipGetInfPathFromRegistry(
		hInstall,
		ProcessRecord->RegRoot,
		ProcessRecord->RegKey,
		ProcessRecord->RegName,
		&oemInfPath);
	
	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"pxMsiGetInfPathFromRegistry failed, hr=0x%x\n", hr);
		//
		// OEM INF is not copied yet. No further processing is required.
		//
		XMSITRACEH(hInstall, L"No actions are necessary for rollback, hardwareId=%ls\n",
			ProcessRecord->HardwareId);
		return ERROR_SUCCESS;
	}

	hr = xDiMsipDeleteInfPathFromRegistry(
		hInstall, 		
		ProcessRecord->RegRoot,
		ProcessRecord->RegKey,
		ProcessRecord->RegName);

	XMSITRACEH(hInstall, L"DeleteInfPathFromRegistry, hr=0x%x\n", hr);

	hr = xDiUninstallOEMInf(oemInfPath, 0);

	XMSITRACEH(hInstall, L"DiUninstallOEMInf, inf=%ls, hr=0x%x\n", 
		oemInfPath, hr);
	
	CoTaskMemFree(oemInfPath);

	return ERROR_SUCCESS;
}

UINT
xDiMsipInstallPnpDeviceInf(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	WCHAR oemInfPath[MAX_PATH];
	DWORD oemInfPathLength = 0;
	LPWSTR oemInfName = NULL;
	
	HRESULT hr;

	while (TRUE)
	{
		XDIMSI_DIALOG_ACTIVATOR_CONTEXT daContext;

		xDiMsipDialogActivatorStart(&daContext);

		BOOL success = SetupCopyOEMInfW(
			ProcessRecord->InfPath,
			NULL,
			SPOST_PATH,
			0,
			oemInfPath,
			RTL_NUMBER_OF(oemInfPath),
			&oemInfPathLength,
			&oemInfName);

		xDiMsipDialogActivatorStop(&daContext);

		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			
			XMSITRACEH(hInstall, L"SetupCopyOEMInf(%s) failed, hr=0x%x\n", 
				ProcessRecord->InfPath, hr);

			// 0, IDABORT, IDRETRY, IDIGNORE
			UINT response = pxMsiErrorMessageBox(hInstall, ProcessRecord->ErrorNumber, hr);
			switch (response)
			{
			case IDRETRY:
				continue;
			case IDIGNORE:
				break;
			case 0:
			case IDABORT:
			default:
				return ERROR_INSTALL_FAILURE;
			}
		}
		else
		{
			XMSITRACEH(hInstall, L"Copied %s to %s\n", ProcessRecord->InfPath, oemInfPath);

			hr = xDiMsipSetInfPathInRegistry(
				hInstall, 
				oemInfPath, 
				ProcessRecord->RegRoot, 
				ProcessRecord->RegKey, 
				ProcessRecord->RegName);

			if (FAILED(hr))
			{
				XMSITRACEH(hInstall, L"pxMsiSetInfPathInRegistry failed, hr=0x%x\n", hr);
			}
		}
		break;
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipCleanupOEMInf(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	int len = lstrlenW(ProcessRecord->HardwareId);
	len += 2; // two terminating null characters

	LPWSTR hardwareIdList = (LPWSTR) calloc(len, sizeof(WCHAR));
	if (NULL == hardwareIdList)
	{
		XMSITRACEH(hInstall, L"Out of memory for %d bytes\n", len * sizeof(WCHAR));
		return ERROR_INSTALL_FAILURE;
	}

	memcpy(hardwareIdList, ProcessRecord->HardwareId, len * sizeof(WCHAR));

	//
	// Now there are two nulls in harwareIdList as we used calloc
	//
	for (LPWSTR p = hardwareIdList; *p; ++p)
	{
		// Substitute ; with \0 
		if (L';' == *p) *p = L'\0';
		++p;
	}

	HRESULT hr = xDiUninstallHardwareOEMInf(
		NULL, hardwareIdList, SUOI_FORCEDELETE, NULL, NULL);

	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"xDiUninstallHardwareOEMInf failed, hr=0x%x\n", hr);
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipCheckServicesInfSection(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	LPWSTR stalledServiceName = NULL;

	HRESULT hr;
	
	XMSITRACEH(hInstall, L"Inspecting services in INF file, inf=%ls, section=%ls, flags=%x\n", 
		ProcessRecord->InfPath, ProcessRecord->InfSectionList, ProcessRecord->Flags);

	if (ProcessRecord->Flags & XDIMSI_CHECK_SERVICES_USE_HARDWARE_ID)
	{
		hr = xDiServiceIsMarkedForDeletionFromPnpInf(
			ProcessRecord->InfPath,
			ProcessRecord->HardwareId, 
			&stalledServiceName); 
	}
	else
	{
		//
		// InfSection: <InstallSection>[;<RollbackSection]
		//

		WCHAR section[128];
		hr = StringCchCopy(section, RTL_NUMBER_OF(section), ProcessRecord->InfSectionList);
		_ASSERT(SUCCEEDED(hr));

		//
		// Remove rollback section
		//
		for (LPWSTR p = section; *p != NULL; ++p)
		{
			if (L';' == *p)
			{
				*p = L'\0';
				break;
			}
		}

		XMSITRACEH(hInstall, L"xDiServiceIsMarkedForDeletionFromInfSection: Section=%ls\n", 
			section);

		hr = xDiServiceIsMarkedForDeletionFromInfSection(
			ProcessRecord->InfPath, 
			section, 
			&stalledServiceName);

		XMSITRACEH(hInstall, L"xDiServiceIsMarkedForDeletionFromInfSection: hr=0x%x\n", hr);
	}

	if (S_OK == hr)
	{
		XMSITRACEH(hInstall, L"A service is marked for deletion, InfPath=%s, Service=%s\n", 
			ProcessRecord->InfPath, stalledServiceName);

		CoTaskMemFree(stalledServiceName);

		if (ProcessRecord->Flags & XDIMSI_CHECK_SERVICES_USE_FORCEREBOOT)
		{
			pxMsiQueueForceReboot(hInstall);
			return ERROR_NO_MORE_ITEMS;
		}
		else
		{
			pxMsiQueueScheduleReboot(hInstall);
			return ERROR_SUCCESS;
		}
	}

	CoTaskMemFree(stalledServiceName);

	return ERROR_SUCCESS;
}

HRESULT
pxDiStartStopService(
	__in LPCWSTR ServiceName, 
	__in BOOL Start)
{
	HRESULT hr;
	
	SC_HANDLE scManagerHandle = OpenSCManagerW(
		NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
	
	if (NULL == scManagerHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}
	
	DWORD serviceAccess = Start ? SERVICE_START : SERVICE_STOP;
	serviceAccess |= SERVICE_QUERY_STATUS;
	if (!Start) serviceAccess |= SERVICE_ENUMERATE_DEPENDENTS;

	SC_HANDLE serviceHandle = OpenServiceW(
		scManagerHandle, ServiceName, 
		serviceAccess);
	
	if (NULL == serviceHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		CloseServiceHandle(scManagerHandle);
		return hr;
	}

	if (Start)
	{
		hr = xDiStartService(scManagerHandle, serviceHandle, 30*1000);
	}
	else
	{
		hr = xDiStopService(scManagerHandle, serviceHandle, TRUE, 30*1000);
	}


	CloseServiceHandle(serviceHandle);
	CloseServiceHandle(scManagerHandle);

	return hr;
}

UINT
xDiMsipStartService(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	if (pxMsiIsForceRebootQueued(hInstall))
	{
		XMSITRACEH(hInstall, L"XDIMSI_STARTSERVICE: Starting service skipped (ForceReboot is queued)\n");
		return ERROR_SUCCESS;
	}
	if (pxMsiIsScheduleRebootQueued(hInstall))
	{
		XMSITRACEH(hInstall, L"XDIMSI_STARTSERVICE: Starting service skipped (ScheduleReboot is queued)\n");
		return ERROR_SUCCESS;
	}

	XMSITRACEH(hInstall, L"XDIMSI_STARTSERVICE: Starting service %ls\n", ProcessRecord->ServiceName);

	HRESULT hr = pxDiStartStopService(ProcessRecord->ServiceName, TRUE);

	XMSITRACEH(hInstall, L"XDIMSI_STARTSERVICE: Starting service returned, hr=0x%x\n", hr);

	if (FAILED(hr))
	{
		//
		// XDIMSI_START_SERVICE_FLAGS_IGNORE_FAILURE
		//
		if (XDIMSI_START_SERVICE_FLAGS_IGNORE_FAILURE & ProcessRecord->Flags)
		{
			XMSITRACEH(hInstall, L"XDIMSI_STARTSERVICE: Ignoring failure\n");
		}
		else
		{
			XMSITRACEH(hInstall, L"XDIMSI_STARTSERVICE: Queue ScheduleReboot on failure\n");
			pxMsiQueueScheduleReboot(hInstall);
		}
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipStopService(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	XMSITRACEH(hInstall, L"XDIMSI_STOPSERVICE: Stopping service %ls\n", ProcessRecord->ServiceName);

	HRESULT hr = pxDiStartStopService(ProcessRecord->ServiceName, FALSE);

	XMSITRACEH(hInstall, L"XDIMSI_STOPSERVICE: Stopping service returned, hr=0x%x\n", hr);

	ATLASSERT( SUCCEEDED(hr) );

	return ERROR_SUCCESS;
}

UINT
xDiMsipQueueScheduleReboot(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	XMSITRACEH(hInstall, L"XDIMSI_QUEUESCHEDULEREBOOT: Queue ScheduleReboot.\n");
	pxMsiQueueScheduleReboot(hInstall);
	return ERROR_SUCCESS;
}

HRESULT
xDiMsipGetInfPathFromRegistry(
	__in MSIHANDLE hInstall,
	__in DWORD RegRoot,
	__in LPCWSTR RegKey,
	__in LPCWSTR RegName,
	__deref_out LPWSTR* OemInfPath)
{
	*OemInfPath = NULL;

	HKEY rootKeyHandle;
	HKEY keyHandle;

	switch (RegRoot)
	{
	case 0: rootKeyHandle = HKEY_CLASSES_ROOT; break;
	case 1: rootKeyHandle = HKEY_CURRENT_USER; break;
	case 2: rootKeyHandle = HKEY_LOCAL_MACHINE; break;
	case 3: rootKeyHandle = HKEY_USERS; break;
	default:
		return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
	}

	LONG result = RegOpenKeyExW(
		rootKeyHandle,
		RegKey,
		0,
		KEY_READ,
		&keyHandle);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegOpenKeyExW failed, root=%p, key=%ls, hr=0x%x\n",
			rootKeyHandle, RegKey, HRESULT_FROM_WIN32(result));

		return HRESULT_FROM_WIN32(result);
	}

	DWORD valueBytes = 0;
	DWORD valueType;

	result = RegQueryValueExW(
		keyHandle,
		RegName,
		NULL,
		&valueType,
		NULL,
		&valueBytes);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegQueryValueExW failed, valueName=%ls, hr=0x%x\n",
			RegName, HRESULT_FROM_WIN32(result));

		return HRESULT_FROM_WIN32(result);
	}

	if (REG_SZ != valueType)
	{
		XMSITRACEH(hInstall, L"RegQueryValueExW failed, valueName=%ls, hr=0x%x\n",
			RegName, TYPE_E_TYPEMISMATCH);

		return TYPE_E_TYPEMISMATCH;
	}

	LPWSTR valueData = (LPWSTR) CoTaskMemAlloc(valueBytes);

	if (NULL == valueData)
	{
		XMSITRACEH(hInstall, L"RegQueryValueExW failed, valueName=%ls, hr=0x%x\n",
			RegName, E_OUTOFMEMORY);

		return E_OUTOFMEMORY;
	}

	result = RegQueryValueExW(
		keyHandle,
		RegName,
		NULL,
		&valueType,
		(LPBYTE)valueData,
		&valueBytes);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegQueryValueExW failed, valueName=%ls, hr=0x%x\n",
			RegName, HRESULT_FROM_WIN32(result));

		return HRESULT_FROM_WIN32(result);
	}

	*OemInfPath = valueData;

	RegCloseKey(keyHandle);

	return S_OK;
}

HRESULT
xDiMsipSetInfPathInRegistry(
	__in MSIHANDLE hInstall,
	__in LPCWSTR InfPath,
	__in DWORD RegRoot,
	__in LPCWSTR RegKey,
	__in LPCWSTR RegName)
{
	HKEY rootKeyHandle;
	HKEY keyHandle;

	switch (RegRoot)
	{
	case 0: rootKeyHandle = HKEY_CLASSES_ROOT; break;
	case 1: rootKeyHandle = HKEY_CURRENT_USER; break;
	case 2: rootKeyHandle = HKEY_LOCAL_MACHINE; break;
	case 3: rootKeyHandle = HKEY_USERS; break;
	default:
		return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
	}

	LONG result = RegCreateKeyExW(
		rootKeyHandle,
		RegKey,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_SET_VALUE,
		NULL,
		&keyHandle,
		NULL);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegCreateKey failed, root=%p, key=%ls, hr=0x%x\n",
			rootKeyHandle, RegKey, HRESULT_FROM_WIN32(result));

		return HRESULT_FROM_WIN32(result);
	}

	DWORD valueBytes = (lstrlenW(InfPath) + 1) * sizeof(WCHAR);

	result = RegSetValueExW(
		keyHandle, 
		RegName, 
		0, 
		REG_SZ, 
		(const BYTE*) InfPath,
		valueBytes);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegSetValueExW failed, name=%ls, hr=0x%x\n",
			RegName, HRESULT_FROM_WIN32(result));
	}

	RegCloseKey(keyHandle);

	return HRESULT_FROM_WIN32(result);
}

HRESULT
xDiMsipDeleteInfPathFromRegistry(
	__in MSIHANDLE hInstall,
	__in DWORD RegRoot,
	__in LPCWSTR RegKey,
	__in LPCWSTR RegName)
{
	HKEY rootKeyHandle;
	HKEY keyHandle;

	switch (RegRoot)
	{
	case 0: rootKeyHandle = HKEY_CLASSES_ROOT; break;
	case 1: rootKeyHandle = HKEY_CURRENT_USER; break;
	case 2: rootKeyHandle = HKEY_LOCAL_MACHINE; break;
	case 3: rootKeyHandle = HKEY_USERS; break;
	default:
		return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
	}

	LONG result = RegOpenKeyExW(
		rootKeyHandle,
		RegKey,
		0,
		KEY_READ | KEY_WRITE,
		&keyHandle);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegOpenKeyExW failed, root=%p, key=%ls, hr=0x%x\n",
			rootKeyHandle, RegKey, HRESULT_FROM_WIN32(result));

		return HRESULT_FROM_WIN32(result);
	}

	result = RegDeleteValueW(keyHandle, RegName);

	if (ERROR_SUCCESS != result)
	{
		XMSITRACEH(hInstall, L"RegDeleteValueW failed, valueName=%ls, hr=0x%x\n",
			RegName, HRESULT_FROM_WIN32(result));

		return HRESULT_FROM_WIN32(result);
	}

	RegCloseKey(keyHandle);

	return S_OK;
}

DWORD
WINAPI
xDiMsipDialogActivatorThreadProc(
	LPVOID Context)
{
	PXDIMSI_DIALOG_ACTIVATOR_CONTEXT activatorContext = 
		(PXDIMSI_DIALOG_ACTIVATOR_CONTEXT) Context;

	_ASSERT(NULL != activatorContext);
	_ASSERT(NULL != activatorContext->StopEventHandle);
	_ASSERT(NULL != activatorContext->ThreadHandle);

	do 
	{
		DWORD waitResult = WaitForSingleObject(
			activatorContext->StopEventHandle,
			activatorContext->Interval);

		if (WAIT_OBJECT_0 == waitResult)
		{
			return 0;
		}
		else if (WAIT_TIMEOUT != waitResult)
		{
			return GetLastError();
		}

		HWND hWndNext = NULL;
		do
		{
			hWndNext = FindWindowEx(
				NULL, 
				hWndNext,
				MAKEINTATOM(32770), // dialog class
				NULL); 
			//
			// we do not know the caption (varies by the platform and the language)
			//
			if (hWndNext != NULL)
			{
				DWORD processId = 0;
				DWORD threadId = GetWindowThreadProcessId(hWndNext, &processId);

				XMSITRACE(L"\tWindowHandle=%p, ProcessId=0x%x, ThreadId=0x%x\n", hWndNext, processId, threadId);

				if (GetCurrentProcessId() == processId)
				{
					XMSITRACE(L"\tActivating Window, Handle=%p\n", hWndNext);

					//
					// make the window topmost
					//
					(void) SetWindowPos(
						hWndNext, 
						HWND_TOPMOST, 
						0, 0, 0, 0, 
						SWP_ASYNCWINDOWPOS | SWP_NOSIZE | SWP_NOMOVE /* | SWP_SHOWWINDOW */);
					//
					// yield to the GUI thread
					//
					Sleep(0); 
				}
			}
		} while (NULL != hWndNext);

	} while(TRUE);

	return 0;
}

HRESULT
xDiMsipDialogActivatorStart(
	__out PXDIMSI_DIALOG_ACTIVATOR_CONTEXT ActivatorContext)
{
	HRESULT hr;

	ActivatorContext->Interval = 10*1000;
	ActivatorContext->ThreadHandle = NULL;
	ActivatorContext->StopEventHandle = NULL;

	HANDLE stopEventHandle = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (NULL == stopEventHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	HANDLE threadHandle = CreateThread(
		NULL, 
		0, 
		xDiMsipDialogActivatorThreadProc, 
		ActivatorContext, 
		0, 
		NULL);
	if (NULL == threadHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		CloseHandle(stopEventHandle);
		return hr;
	}

	ActivatorContext->ThreadHandle = threadHandle;
	ActivatorContext->StopEventHandle = stopEventHandle;

	return S_OK;
}

HRESULT
xDiMsipDialogActivatorStop(
	__in PXDIMSI_DIALOG_ACTIVATOR_CONTEXT ActivatorContext)
{
	SetEvent(ActivatorContext->StopEventHandle);
	WaitForSingleObject(ActivatorContext->ThreadHandle, INFINITE);
	return S_OK;
}
