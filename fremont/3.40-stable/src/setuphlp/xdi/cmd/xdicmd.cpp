#include <stdio.h>
#include <windows.h>
#include <objbase.h>
#include <setupapi.h>
#include <shlwapi.h>
#include <devguid.h>
#include <regstr.h>

#include <cfg.h>
#include <cfgmgr32.h>
#include <strsafe.h>

#include "../xdi.h"

typedef struct _DISPATCH_ENTRY {
	LPCWSTR Command;
	int (*Function)(int, WCHAR**);
} DISPATCH_ENTRY, PDISPATCH_ENTRY;

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

void report_error(FILE* out, DWORD ecode)
{
	LPWSTR description = NULL;
	DWORD n = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		ecode,
		0,
		(LPWSTR) &description,
		0,
		NULL);
	if (n > 0) 
	{
		fwprintf(out, L"0x%x: %s\n", ecode, description);
	}
	else 
	{
		fwprintf(out, L"0x%x: (no description)\n", ecode);
	}
	if (NULL != description) LocalFree(description);
}

enum {
	XDICMD_GENERIC,
	XDICMD_INSTALL_NET_USAGE,
	XDICMD_INVALID_INF_PATH,
	XDICMD_INF_COPY_FAILURE,
	XDICMD_INVALID_DEVICE_CLASS,
	XDICMD_INSTALL_NET_FAILURE,
	XDICMD_UNINSTALL_OEM_INF_FAILED
};

struct {
	DWORD MessageId;
	LPCWSTR MessageFormat;
} MessageTable[] = {
	XDICMD_GENERIC, L"%s\n",
	XDICMD_INSTALL_NET_USAGE,    L"usage: instnet <component-id> <inf-path>\n",
	XDICMD_INVALID_INF_PATH,     L"INF Path is invalid.\n",
	XDICMD_INF_COPY_FAILURE,     L"Preinstalling INF failed.\n",
	XDICMD_INVALID_DEVICE_CLASS, L"Reading a device class from the INF failed.\n",
	XDICMD_INSTALL_NET_FAILURE,  L"Installing a network component failed.\n",
	XDICMD_UNINSTALL_OEM_INF_FAILED, L"Uninstalling OEM INF failed.\n"
};

void msgv(DWORD MessageId, va_list ap)
{
	vfwprintf(stdout, MessageTable[MessageId].MessageFormat, ap);
}

void msgf(DWORD MessageId, ...)
{
	va_list ap;
	va_start(ap, MessageId);
	msgv(MessageId, ap);
	va_end(ap);
}

void msgfe(DWORD ErrorCode, DWORD MessageId, ...)
{
	va_list ap;
	va_start(ap, MessageId);
	msgv(MessageId, ap);
	va_end(ap);
	report_error(stdout, ErrorCode);
}

void msgff(DWORD ErrorCode, LPCWSTR Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	vfwprintf(stdout, Format, ap);
	va_end(ap);
	report_error(stdout, ErrorCode);
}

LPCWSTR 
resolve_infpath(
	__in LPCWSTR InfPath, 
	__out_ecount(MAX_PATH) LPWSTR InfFullPath)
{
	DWORD n = GetFullPathNameW(
		InfPath, 
		MAX_PATH, 
		InfFullPath, 
		NULL);

	if (0 == n)
	{
		DWORD se = GetLastError();
		msgfe(se, XDICMD_INVALID_INF_PATH);
		return NULL;
	}

	return InfFullPath;
}

int install_net(int argc, WCHAR** argv)
{
	if (argc < 2)
	{
		msgf(XDICMD_INSTALL_NET_USAGE);
		return 1;
	}

	LPCWSTR componentId = argv[0];
	LPCWSTR infPath = argv[1];

	LPCWSTR client = L"XDI - Device Driver Installer";
	LPWSTR lockedBy = NULL;

	WCHAR infFullPath[MAX_PATH];

	infPath = resolve_infpath(infPath, infFullPath);
	if (NULL == infPath) return 1;

	WCHAR oemInfPath[MAX_PATH];
	LPWSTR oemInfFileName = NULL;
	
	BOOL success = SetupCopyOEMInfW(
		infPath,
		NULL,
		SPOST_PATH,
		0,
		oemInfPath,
		MAX_PATH,
		NULL,
		&oemInfFileName);

	if (!success)
	{
		HRESULT hr = HRESULT_FROM_SETUPAPI(GetLastError());
		msgfe(hr, XDICMD_INF_COPY_FAILURE);
		return 1;
	}

	fwprintf(stdout, L"OEM INF=%s, %s\n", oemInfPath, oemInfFileName);

	GUID classGuid;
	WCHAR className[MAX_CLASS_NAME_LEN];

	success = SetupDiGetINFClassW(
		oemInfPath,
		&classGuid,
		className,
		RTL_NUMBER_OF(className),
		NULL);

	if (!success)
	{
		HRESULT hr = HRESULT_FROM_SETUPAPI(GetLastError());
		msgfe(hr, XDICMD_INVALID_DEVICE_CLASS);
		return 1;
	}

	HRESULT hr = xDiInstallNetComponent(&classGuid, componentId, 15000, client, &lockedBy);

	if (FAILED(hr))
	{
		msgfe(hr, XDICMD_INSTALL_NET_FAILURE);

		if (NULL != lockedBy)
		{
			CoTaskMemFree(lockedBy);
		}

		return 1;
	}

	if (XDI_S_REBOOT == hr)
	{
		fwprintf(stdout, L"Reboot is required to complete the operation.\n");
	}

	if (NULL != lockedBy)
	{
		CoTaskMemFree(lockedBy);
	}

	return 0;
}

//
// http://support.microsoft.com/default.aspx?scid=kb;[LN];822445
//

int uninstall_net(int argc, WCHAR** argv)
{
	HRESULT hr;

	if (argc < 1)
	{
		fwprintf(stderr, L"usage: uninstnet <component-id>\n");
		return 1;
	}

	LPCWSTR componentId = argv[0];
	DWORD timeout = 15 * 1000;
	LPCWSTR client = L"XDI - Device Driver Installer";
	LPWSTR lockedBy = NULL;
	LPWSTR oemInfName = NULL;

	hr = xDiUninstallNetComponent(componentId, timeout, client, &lockedBy, &oemInfName);

	if (FAILED(hr))
	{
		fwprintf(stderr, L"Uninstalling network component failed, hr=0x%x\n", hr);
		report_error(stderr, hr);

		if (NULL != lockedBy)
		{
			fwprintf(stderr, L"Network installation is locked by %s", lockedBy);
			CoTaskMemFree(lockedBy);
		}

		return 1;
	}

	if (XDI_S_REBOOT == hr)
	{
		fwprintf(stdout, L"Reboot is required to complete the operation.\n");
	}
	else if (S_FALSE == hr)
	{
		fwprintf(stdout, L"'%s' is not installed.\n", componentId);
		return 1;
	}

	WCHAR oemInfPath[MAX_PATH];
	GetWindowsDirectoryW(oemInfPath, MAX_PATH);
	PathAppendW(oemInfPath, L"INF");
	PathAppendW(oemInfPath, oemInfName);

	hr = xDiDeleteDriverFiles(NULL, oemInfPath, XDI_EDF_NO_CLASS_INSTALLER);

	if (FAILED(hr))
	{
		fwprintf(stdout, L"Warning! Deleting driver files failed, hr=0x%x\n", hr);
	}

	hr = xDiUninstallOEMInf(oemInfName, 0);

	if (FAILED(hr))
	{
		fwprintf(stdout, L"Warning! Deleting OEM INF file failed, inf=%s, error=0x%x\n",
			oemInfName, hr);
	}

	if (NULL != oemInfName)
	{
		CoTaskMemFree(oemInfName);
	}

	if (NULL != lockedBy)
	{
		CoTaskMemFree(lockedBy);
	}

	return 0;
}

int update_device(int argc, WCHAR** argv)
{
	HRESULT hr;

	if (argc < 2)
	{
		fwprintf(stderr, L"usage: update <hardware-id> <inf-path>\n");
		return 1;
	}

	LPCWSTR hardwareId = argv[0];
	LPCWSTR infPath = argv[1];

	WCHAR infFullPath[MAX_PATH];

	infPath = resolve_infpath(infPath, infFullPath);
	if (NULL == infPath) return 1;

	WCHAR oemInfPath[MAX_PATH];
	LPWSTR oemInfFileName = NULL;

	BOOL success = SetupCopyOEMInfW(
		infPath,
		NULL,
		SPOST_PATH,
		0,
		oemInfPath,
		MAX_PATH,
		NULL,
		&oemInfFileName);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		msgfe(hr, XDICMD_INF_COPY_FAILURE);
		return 1;
	}

	fwprintf(stdout, L"OEM INF=%s, %s\n", oemInfPath, oemInfFileName);

	hr = xDiUpdateDeviceDriver(
		NULL, 
		hardwareId, 
		infFullPath,
		INSTALLFLAG_FORCE);

	if (SUCCEEDED(hr))
	{
		if (XDI_S_REBOOT == hr)
		{
			fwprintf(stdout, L"Reboot is required to complete the operation.\n");
		}
	}
	else
	{
		msgfe(hr, XDICMD_GENERIC, L"Installation failed\n");
	}

	return 0;
}

int install_device(int argc, WCHAR** argv)
{
	HRESULT hr;

	if (argc < 2)
	{
		fwprintf(stderr, L"usage: instdev [/multiple] <hardware-id> <inf-path> [device-name]\n");
		return 1;
	}

	int index = 0;
	DWORD installFlags = 0;

	if (0 == lstrcmpiW(L"/multiple", argv[index]))
	{
		++index;
	}
	else
	{
		installFlags |= XDI_INSTALL_SINGLE_INSTANCE;
	}

	LPCWSTR hardwareId = argv[index++];
	LPCWSTR infPath = argv[index++];
	LPCWSTR deviceName = index < argc ? argv[index++] : NULL;

	WCHAR infFullPath[MAX_PATH];

	infPath = resolve_infpath(infPath, infFullPath);
	if (NULL == infPath) return 1;
	
	WCHAR oemInfPath[MAX_PATH];
	LPWSTR oemInfFileName = NULL;

	BOOL success = SetupCopyOEMInfW(
		infPath,
		NULL,
		SPOST_PATH,
		0,
		oemInfPath,
		MAX_PATH,
		NULL,
		&oemInfFileName);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		msgfe(hr, XDICMD_INF_COPY_FAILURE);
		return 1;
	}

	fwprintf(stdout, L"OEM INF=%s, %s\n", oemInfPath, oemInfFileName);

	hr = xDiInstallLegacyPnpDevice(
		NULL, 
		hardwareId, 
		infFullPath,
		deviceName,
		installFlags,
		INSTALLFLAG_FORCE);

	if (SUCCEEDED(hr))
	{
		if (XDI_S_REBOOT == hr)
		{
			fwprintf(stdout, L"Reboot is required to complete the operation.\n");
		}
	}
	else
	{
		msgfe(hr, XDICMD_GENERIC, L"Installation failed\n");
	}

	return 0;
}

BOOL CALLBACK
remove_device_callback(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVINFO_DATA DeviceInfoData,
	__in LPVOID CallbackContext);

HRESULT
UninstallOEMInfFilesFromList(__in LPCWSTR InfNameList)
{
	WCHAR infPath[MAX_PATH];

	GetWindowsDirectoryW(infPath, MAX_PATH);
	PathAppendW(infPath, L"INF");

	LPWSTR infPathTerm = infPath + lstrlenW(infPath);

	HRESULT hr;
	for (LPCWSTR infName = InfNameList; 
		*infName; 
		infName += lstrlenW(infName) + 1)
	{
		*infPathTerm = L'\0';
		PathAppendW(infPath, infName);

		hr = xDiDeleteDriverFiles(NULL, infPath, XDI_EDF_NO_CLASS_INSTALLER);
		if (FAILED(hr))
		{
			msgff(hr, L"xDiDeleteDriverFiles(%s) failed.\n", infPath);
		}

		BOOL success = Setupapi_SetupUninstallOEMInfW(infName, 0, NULL);
		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			msgff(hr, L"SetupUninstallOEMInf(%s) failed.\n", infName);
		}

		fwprintf(stdout, L"INF(%s) deleted successfully.\n", infName);
	}
	return S_OK;
}

HRESULT
DeletePnpDriverServicesFromList(__in LPCWSTR ServiceList)
{
	HRESULT hr;
	SC_HANDLE scManagerHandle = OpenSCManagerW(NULL, NULL, 
		SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_LOCK);

	if (NULL == scManagerHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		msgff(hr, L"OpenSCManager failed.");
		return hr;
	}

	SC_LOCK scLock = LockServiceDatabase(scManagerHandle);
	if (NULL == scLock)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		msgff(hr, L"LockServiceDatabase failed.");
		CloseServiceHandle(scManagerHandle);
		return hr;
	}

	for (LPCWSTR serviceName = ServiceList; 
		*serviceName;
		serviceName += lstrlenW(serviceName) + 1)
	{
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
			msgff(hr, L"OpenService failed.\n");
			continue;
		}

		BOOL success = DeleteService(serviceHandle);

		if (!success)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			msgff(hr, L"DeleteService failed.\n");
			CloseServiceHandle(serviceHandle);
			continue;
		}

		CloseServiceHandle(serviceHandle);

		fwprintf(stdout, L"Service(%s) deleted successfully.\n", serviceName);

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

	}

	UnlockServiceDatabase(scLock);
	CloseServiceHandle(scManagerHandle);

	return S_OK;
}

int uninstall_device(int argc, WCHAR** argv)
{
	HRESULT hr;

	if (argc < 1)
	{
		fwprintf(stderr, L"usage: uninstdev <hardware-id> [enum-flags]\n");
		return 1;
	}

	LPCWSTR hardwareId = argv[0];
	DWORD enumFlags = argc > 1 ? _wtoi(argv[1]) : DIGCF_ALLCLASSES;

	LPWSTR serviceList = NULL;
	LPWSTR infList = NULL;

	hr = xDiRemoveDevices(
		NULL,
		NULL,
		NULL,
		hardwareId,
		enumFlags,
		&serviceList,
		&infList);

	if (SUCCEEDED(hr))
	{
		if (XDI_S_REBOOT == hr)
		{
			fwprintf(stdout, L"Reboot is required to complete the operation.\n");
		}
		for (LPCWSTR p = serviceList; *p; p += lstrlenW(p) + 1)
		{
			fwprintf(stdout, L"Service: %s\n", p);
		}
		for (LPCWSTR p = infList; *p; p += lstrlenW(p) + 1)
		{
			fwprintf(stdout, L"INF: %s\n", p);
		}

		DeletePnpDriverServicesFromList(serviceList);

		UninstallOEMInfFilesFromList(infList);
	}
	else
	{
		msgfe(hr, XDICMD_GENERIC, L"Operation failed.\n");
	}

	if (serviceList) CoTaskMemFree(serviceList);
	if (infList) CoTaskMemFree(infList);

	return 0;
}

int uninstall_inf(int argc, WCHAR** argv)
{
	if (argc < 1)
	{
		fwprintf(stderr, L"usage: uninstinf <inf-name>\n");
		return 1;
	}

	LPCWSTR infName = argv[0];
	LPCWSTR infFileName = PathFindFileNameW(infName);
	
	BOOL success = Setupapi_SetupUninstallOEMInfW(
		infFileName, 0, NULL);

	if (!success)
	{
		HRESULT hr = HRESULT_FROM_SETUPAPI(GetLastError());
		msgfe(hr, XDICMD_UNINSTALL_OEM_INF_FAILED);
		return 1;
	}

	return 0;
}

BOOL 
CALLBACK 
enum_driver_file_callback(
	__in LPCWSTR targetpath,
	__in DWORD flags,
	__in LPVOID context)
{
	if (flags)
	{
		fwprintf(stdout, L"%s (0x%x)\n", targetpath, flags);
	}
	else
	{
		fwprintf(stdout, L"%s\n", targetpath);
	}
	return TRUE;
}

int enum_driver_files(int argc, WCHAR** argv)
{
	if (argc < 1)
	{
		fwprintf(stderr, L"usage: enumdrvfiles <inf-path>\n");
		return 1;
	}

	LPCWSTR infPath = argv[0];
	DWORD flags = argc > 1 ? _wtoi(argv[1]) : 0;

	HRESULT hr = xDiEnumDriverFiles(
		NULL, infPath, flags, 
		enum_driver_file_callback, NULL);

	if (FAILED(hr))
	{
		msgfe(hr, XDICMD_GENERIC, L"Enumeration failed\n");
		return 1;
	}

	return 0;
}

int service_is_marked_for_pending_deletion(int argc, WCHAR** argv)
{
	if (argc < 1)
	{
		fwprintf(stderr, L"usage: serviceis <service-name>\n");
		return 1;
	}

	HRESULT hr = xDiServiceIsMarkedForDeletion(argv[0]);

	if (S_OK == hr)
	{
		fwprintf(stdout, L"Service is marked for deletion\n");
	}
	else
	{
		fwprintf(stdout, L"hr=0x%x\n", hr);
	}

	return 0;
}

int query_service(int argc, WCHAR** argv)
{
	if (argc < 1) return 1;

	LPCWSTR serviceName = argv[0];
	SC_HANDLE scManagerHandle = OpenSCManagerW(NULL, NULL, 
		SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_LOCK);

	if (NULL != scManagerHandle)
	{
		SC_HANDLE serviceHandle = OpenServiceW(
			scManagerHandle, serviceName, SERVICE_QUERY_STATUS);
		if (NULL != serviceHandle)
		{
			SERVICE_STATUS serviceStatus;
			if (QueryServiceStatus(serviceHandle, &serviceStatus))
			{
				fwprintf(stderr, L"State=%x\n", serviceStatus.dwCurrentState);
			}
			else
			{
				msgff(GetLastError(), L"QueryServiceStatus failed.\n");
			}
			CloseServiceHandle(serviceHandle);
		}
		else
		{
			msgff(GetLastError(), L"OpenService failed.\n");
		}
		CloseServiceHandle(scManagerHandle);
	}
	else
	{
		msgff(GetLastError(), L"OpenSCManagerW failed.\n");
	}
	return 0;
}

int install_from_inf(int argc, WCHAR** argv)
{
	if (argc < 2)
	{
		fwprintf(stderr, L"usage: instinf <inf-file> <section>\n");
		return 1;
	}

	LPCWSTR infPath = argv[0];
	LPCWSTR infSection = argv[1];

	BOOL reboot = FALSE, rebootForService = FALSE;

	HRESULT hr = xDiInstallFromInfSection(
		NULL, 
		infPath, 
		infSection,
		0,
		0,
		&reboot,
		&rebootForService);

	if (FAILED(hr))
	{
		msgff(hr, L"InstallInfSection failed.\n");
	}

	if (reboot)
	{
		fwprintf(stdout, L"Reboot is required to complete the operation (File).\n");
	}

	if (rebootForService)
	{
		fwprintf(stdout, L"Reboot is required to complete the operation (Service).\n");
	}

	return 0;
}

int service_is_marked_for_pending_deletion_in_inf(int argc, WCHAR** argv)
{
	if (argc < 2)
	{
		fwprintf(stderr, L"usage: infservice <inf-file> <hardwareid>\n");
		return 1;
	}

	LPCWSTR infPath = argv[0];
	LPCWSTR hardwareId = argv[1];

	LPWSTR serviceName = NULL;

	HRESULT hr = xDiServiceIsMarkedForDeletionFromPnpInf(infPath, hardwareId, &serviceName);

	if (FAILED(hr))
	{
		fwprintf(stdout, L"xDiServiceIsMarkedForDeletionFromPnpInf failed, hr=0x%x\n", hr);
	}
	else if (S_OK == hr) 
	{
		fwprintf(stdout, L"Service is marked for pending deletion, service=%ls\n", serviceName);
	}

	CoTaskMemFree(serviceName);

	serviceName = NULL;

	hr = xDiServiceIsMarkedForDeletionFromInfSection(infPath, hardwareId, &serviceName);

	if (FAILED(hr))
	{
		fwprintf(stdout, L"xDiServiceIsMarkedForDeletionFromInfSection failed, hr=0x%x\n", hr);
	}
	else if (S_OK == hr) 
	{
		fwprintf(stdout, L"Service is marked for pending deletion, service=%ls\n", serviceName);
	}

	CoTaskMemFree(serviceName);

	return 0;
}

DISPATCH_ENTRY DispatchTable[] = {
	L"instnet", install_net,
	L"uninstnet", uninstall_net,
	L"instdev", install_device,
	L"uninstdev", uninstall_device,
	L"uninstinf", uninstall_inf,
	L"enumdrvfiles", enum_driver_files,
	L"serviceis", service_is_marked_for_pending_deletion,
	L"instinf", install_from_inf,
	L"infservice", service_is_marked_for_pending_deletion_in_inf,
	L"update", update_device,
};

int __cdecl wmain(int argc, WCHAR** argv)
{
	if (argc < 2)
	{
		fwprintf(stderr, L"usage: xdi <command>\n");
		for (int i = 0; i < RTL_NUMBER_OF(DispatchTable); ++i)
		{
			fwprintf(stderr, L" %s\n", DispatchTable[i].Command);
		}
		return 1;
	}
	LPCWSTR command = argv[1];
	for (int i = 0; i < RTL_NUMBER_OF(DispatchTable); ++i)
	{
		if (0 == lstrcmpiW(command, DispatchTable[i].Command))
		{
			return DispatchTable[i].Function(argc - 2, argv + 2);
		}
	}
	fwprintf(stderr, L"usage: xdi <command>\n");
	for (int i = 0; i < RTL_NUMBER_OF(DispatchTable); ++i)
	{
		fwprintf(stderr, L" %s\n", DispatchTable[i].Command);
	}
	return 1;
}

