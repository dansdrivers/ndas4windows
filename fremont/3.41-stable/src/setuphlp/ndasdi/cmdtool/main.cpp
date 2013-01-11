#include "stdafx.h"
#include "pnpdevinst.h"
#include "svcinst.h"
#include "netcomp.h"
#include "xdi.h"
#include <xtl/xtltrace.h>

/*++

wmain, library tester

--*/

#define USAGE \
	_T("usage: ndasdi components command [command-args]\n") \
	_T("\n") \
	_T("Components: device, service, network, driver, inf\n") \
	_T("\n") \
	_T("dev install <inf> <hardware id>\n") \
	_T("dev update <inf> <hardware id>\n") \
	_T("dev remove <hardware id> [all]\n") \
	_T("dev find <hardware id>\n") \
	_T("dev copyinf <inf>\n") \
	_T("dev uninstallinf <hardware id>,<hardware id>,...\n") \
	_T("\n") \
	_T("net install <inf> <protocol|adapter|client|service> <component id>\n") \
	_T("net remove <component id>\n") \
	_T("net find <component id>\n") \
	_T("\n") \
	_T("svc install <binary path> <name> <display name>\n") \
	_T("svc remove <name>\n") \
	_T("svc find <name>\n") \
	_T("drv install <source file path> <name> <display name> [load order group]\n") \
	_T("drv remove <name>\n") \
	_T("\n") \
	_T("inf <inf> <section>") \
	_T("\n")

void print_error(FILE* fp = stderr, DWORD dwError = GetLastError())
{
	DWORD lasterr = GetLastError();
	LPTSTR lpBuffer = NULL;

	DWORD cchMsg = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		0,
		(LPTSTR)&lpBuffer,
		0,
		NULL);

	if (0 == cchMsg) 
	{
		_ftprintf(fp, _T("Error %u (0x%08X): No description available.\n"), dwError, dwError);
	}
	else 
	{
		_ftprintf(fp, _T("Error %u (0x%08X): %s"), dwError, dwError, lpBuffer);
		LocalFree(lpBuffer);
	}

	SetLastError(lasterr);
}

BOOL CALLBACK 
RemoveErrorCallback(
	NDASDI_REMOVE_DEVICE_ERROR_TYPE Type,
	HDEVINFO DeviceInfoSet,
	PSP_DEVINFO_DATA DeviceInfoData,
	DWORD Error,
	LPVOID Context)
{
	switch(Type) {
	case NDIRD_ERROR_GetDeviceRegistryProperty:
		_tprintf(_T("NDIRD_ERROR_GetDeviceRegistryProperty"));
		break;
	case NDIRD_ERROR_CallRemoveToClassInstaller:
		_tprintf(_T("NDIRD_ERROR_CallRemoveToClassInstaller"));
		break;
	default:
		_tprintf(_T("NDIRD_ERROR_???"));
	}

	print_error();

	return TRUE;
}

static
BOOL 
CALLBACK
DeleteOEMInfCallback(
	DWORD dwError, 
	LPCTSTR szPath, 
	LPCTSTR szFileName, 
	LPVOID lpContext)
{
	if (ERROR_SUCCESS == dwError) {
		_tprintf(_T("%s\\%s deleted\n"), szPath, szFileName);
	} else {
		_tprintf(_T("Deleting %s\\%s failed with error %d\n"), 
			szPath, szFileName, dwError);
	}
	return TRUE;
}

int pnpdevinst(int argc, LPTSTR *argv)
{
	typedef enum {
		DEV_INSTALL,
		DEV_UPDATE,
		DEV_REMOVE,
		DEV_COPYINF,
		DEV_FIND,
		DEV_UNINSTALLINF
	} DEV_COMMAND_TYPE;

	struct {
		DEV_COMMAND_TYPE type;
		LPCTSTR cmd; 
		int argc; 
		BOOL op; 
	} opts[] = { 
		DEV_INSTALL, L"install", 2, FALSE, 
		DEV_UPDATE, L"update", 2, FALSE,
		DEV_REMOVE, L"remove", 1, TRUE, 
		DEV_COPYINF, L"copyinf", 1, FALSE,
		DEV_FIND, L"find", 1, TRUE,
		DEV_UNINSTALLINF, L"uninstallinf", 1, FALSE
	};

	DEV_COMMAND_TYPE CmdType;

	const DWORD nopts = RTL_NUMBER_OF(opts);

	DWORD i = 0;
	for (; i < nopts; ++i) {
		if (lstrcmpi(opts[i].cmd, argv[0]) == 0) {
			if ((argc - 1) == opts[i].argc ||
				(opts[i].op && (argc - 1) >= opts[i].argc)) 
			{
				CmdType = opts[i].type;
			} else {
				_ftprintf(stderr, _T("Invalid command arguments.\n"));
				return 254;
			}
			break;
		}
	}

	if (i == nopts) {
		_ftprintf(stderr, _T("Invalid command.\n"));
		return 254;
	}

	BOOL bRebootRequired;
	BOOL bPresentOnly = TRUE;

	if (DEV_INSTALL == CmdType) {

		BOOL fSuccess = NdasDiInstallRootEnumeratedDevice(
			NULL, 
			argv[1], 
			argv[2],
			INSTALLFLAG_FORCE,
			&bRebootRequired);
		if (!fSuccess) {
			print_error();
			return 255;
		} else {
			_ftprintf(stdout, _T("Installation is done.%s"),
				(bRebootRequired) ? _T(" Reboot required.") : _T(""));
			return (bRebootRequired) ? 1 : 0;
		}

	} else if (DEV_UPDATE == CmdType) {

		BOOL fSuccess = NdasDiUpdateDeviceDriver(
			NULL,
			argv[1],
			argv[2],
			INSTALLFLAG_FORCE,
			&bRebootRequired);
		if (!fSuccess) {
			print_error();
			return 255;
		} else {
			_ftprintf(stdout, _T("Update is done.%s"),
				(bRebootRequired) ? _T(" Reboot required.") :  _T(""));
			return (bRebootRequired) ? 1 : 0;
		}

	} else if (DEV_REMOVE == CmdType) {

		if (argc > 2 && 0 == lstrcmpi(argv[2], _T("all"))) {
			bPresentOnly = FALSE;
		}

		DWORD dwRemoved = 0;
		BOOL fSuccess = NdasDiRemoveDevice(
			NULL,
			argv[1], 
			bPresentOnly,
			&dwRemoved,
			&bRebootRequired,
			RemoveErrorCallback,
			NULL);
		if (!fSuccess) {
			print_error();
			return 255;
		} else {
			print_error();
			_ftprintf(stdout, _T("Removal is done for %d devices.%s"),
				dwRemoved,
				(bRebootRequired) ? _T(" Reboot required.") : _T(""));
			return 0;
		}

	} else if (DEV_COPYINF == CmdType) {

		TCHAR szOemInf[MAX_PATH + 1];

		BOOL fSuccess = NdasDiCopyOEMInf(
			argv[1], 
			0,
			szOemInf,
			MAX_PATH,
			NULL,
			NULL);

		if (!fSuccess) {
			print_error();
			return 255;
		} else {
			_ftprintf(stdout, _T("Copied to %s\n"), szOemInf);
			return 0;
		}

	} else if (DEV_FIND == CmdType) {

		LPCTSTR szHardwareID = argv[1];

		if (argc > 2 && 0 == lstrcmpi(argv[2], _T("all"))) {
			bPresentOnly = FALSE;
		}

		BOOL fSuccess = NdasDiFindExistingDevice(
			NULL,
			szHardwareID,
			bPresentOnly);

		if (!fSuccess) {
			if (ERROR_NO_MORE_ITEMS == GetLastError()) {
				_ftprintf(stdout, _T("Device not found.\n"));
				return 1;
			} else {
				print_error();
				return 255;
			}
		} else {
			_ftprintf(stdout, _T("Device found.\n"));
			return 0;
		}

	} else if (DEV_UNINSTALLINF == CmdType) {
		
		size_t cchHardwareIDs = ::lstrlen(argv[1]) + 2; // plus additional null

		LPTSTR mszHardwareIDs = (LPTSTR) ::HeapAlloc(
			::GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			cchHardwareIDs * sizeof(TCHAR));
		_ASSERTE(NULL != mszHardwareIDs);

		HRESULT hr = ::StringCchCopy(mszHardwareIDs, cchHardwareIDs, argv[1]);
		_ASSERTE(SUCCEEDED(hr));

		// replaces , to _T('\0')
		PTCHAR pch = mszHardwareIDs;
		for (size_t i = 0; i < cchHardwareIDs; ++i, ++pch) {
			if (_T(',') == *pch) {
				*pch = _T('\0');
			}
		}

		_tprintf(_T("%s\n"), mszHardwareIDs);

		BOOL fSuccess = NdasDiDeleteOEMInf(
			_T("oem*.inf"),
			mszHardwareIDs,
			0x0001,
			DeleteOEMInfCallback,
			NULL);

		::HeapFree(::GetProcessHeap(), 0, mszHardwareIDs);

		return 0;

	} else {
		_ftprintf(stderr, _T("Unknown command.\n"));
		return 254;
	}
}

int netcomp(int argc, LPTSTR *argv)
{
	typedef enum {
		NET_INSTALL,
		NET_REMOVE,
		NET_FIND
	} COMMAND_TYPE;

	struct {
		COMMAND_TYPE type;
		LPCTSTR cmd; 
		int argc; 
		BOOL op; 
	} opts[] = 
	{ 
		{ NET_INSTALL, L"install", 3, FALSE}, 
		{ NET_REMOVE, L"remove", 1, FALSE},
		{ NET_FIND, L"find", 1, FALSE},
	};

	COMMAND_TYPE CmdType;

	const DWORD nopts = RTL_NUMBER_OF(opts);
	DWORD i = 0;

	for (; i < nopts; ++i) {
		if (lstrcmpi(opts[i].cmd, argv[0]) == 0) {
			if ((argc - 1) != opts[i].argc) {
				_ftprintf(stderr, _T("Invalid command arguments.\n"));
				return 254;
			}
			CmdType = opts[i].type;
			break;
		}
	}

	if (i == nopts) {
		_ftprintf(stderr, _T("Invalid command.\n"));
		return 254;
	}

	if (NET_INSTALL == CmdType) {

		NetClass nc;

		if (0 == lstrcmpi(_T("protocol"), argv[2])) {
			nc = NC_NetProtocol;
		} else if (0 == lstrcmpi(_T("adapter"), argv[2])) {
			nc = NC_NetAdapter;
		} else if (0 == lstrcmpi(_T("client"), argv[2])) {
			nc = NC_NetClient;
		} else if (0 == lstrcmpi(_T("service"), argv[2])) {
			nc = NC_NetService;
		} else {
			_ftprintf(stderr, _T("Invalid network component.\n"));
			return 253;
		}

		TCHAR szCopiedInfPath[MAX_PATH] = {0};
		
		HRESULT hr = HrInstallNetComponent(
			argv[3],
			nc,
			argv[1],
			szCopiedInfPath,
			MAX_PATH,
			NULL,
			NULL);

		if (FAILED(hr)) {
			_tprintf(_T("Installation of %s failed with error %s\n"), 
				argv[3],
				pNetCfgHrString(hr));
			print_error(stderr, hr);
			return 255;
		} else if (S_OK != hr) {
			_tprintf(_T("%s installed successfully. INF=%s\n"), argv[3], szCopiedInfPath);
			_tprintf(_T("HResult = %s\n"), pNetCfgHrString(hr));
			print_error(stderr, hr);
			return 1;
		} else {
			_tprintf(_T("%s installed successfully. INF=%s\n"), argv[3], szCopiedInfPath);
			return 0;
		}

	} else if (NET_REMOVE == CmdType) {

		HRESULT hr = HrUninstallNetComponent(argv[1]);

		if (FAILED(hr)) {
			_tprintf(_T("Uninstallation of %s failed with error %s\n"), 
				argv[3],
				pNetCfgHrString(hr));
			return 255;
		} else if (S_OK != hr) {
			_tprintf(_T("%s uninstalled successfully.\n"), argv[1]);
			_tprintf(_T("HResult = %s\n"), pNetCfgHrString(hr));
			print_error(stderr, hr);
			return 1;
		} else {
			_tprintf(_T("%s uninstalled successfully.\n"), argv[1]);
			return 0;
		}

	} else if (NET_FIND == CmdType) {

		HRESULT hr = HrIsNetComponentInstalled(argv[1]);

		if (FAILED(hr)) {
			_tprintf(_T("Finding %s failed with error %s\n"), argv[1], pNetCfgHrString(hr));
			print_error(stderr, hr);
			return 255;
		} else {
			if (S_FALSE == hr) {
				_tprintf(_T("Component %s not found.\n"), argv[1]);
				return 1;
			} else if (S_OK == hr) {
				_tprintf(_T("Component %s is installed.\n"), argv[1]);
				return 0;
			} else {
				_tprintf(_T("Component %s not found. (%s)\n"), argv[1], pNetCfgHrString(hr));
				print_error(stderr, hr);
				return 1;
			}
		}

	} else {
		_ftprintf(stderr, _T("Unknown command.\n"));
		return 254;
	}

	return 0;
}

int svcinst(int argc, LPTSTR *argv)
{
	typedef enum { SVC_INSTALL, SVC_REMOVE, SVC_FIND } COMMAND_TYPE;

	struct { COMMAND_TYPE type; LPCTSTR cmd; int argc; BOOL op; } opts[] = 
	{ 
		{ SVC_INSTALL, L"install", 3, FALSE}, 
		{ SVC_REMOVE, L"remove", 1, FALSE},
		{ SVC_FIND, L"find", 1, FALSE},
	};

	COMMAND_TYPE CmdType;

	const DWORD nopts = RTL_NUMBER_OF(opts);
	DWORD i = 0;

	for (; i < nopts; ++i) {
		if (lstrcmpi(opts[i].cmd, argv[0]) == 0) {
			if ((argc - 1) != opts[i].argc) {
				_ftprintf(stderr, _T("Invalid command arguments.\n"));
				return 254;
			}
			CmdType = opts[i].type;
			break;
		}
	}

	if (i == nopts) {
		_ftprintf(stderr, _T("Invalid command.\n"));
		return 254;
	}

	if (SVC_INSTALL == CmdType) {

		LPTSTR szBinaryPath = argv[1];
		LPTSTR szServiceName = argv[2];
		LPTSTR szDisplayName = argv[3];

		BOOL fSuccess = NdasDiInstallService(
			szServiceName,
			szDisplayName,
			_T(""),
			SERVICE_WIN32_OWN_PROCESS,
			SERVICE_DEMAND_START,
			SERVICE_ERROR_NORMAL,
			szBinaryPath,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);

		if (!fSuccess) {
			print_error();
			return 254;
		} else {
			_ftprintf(stdout, 
				_T("Service %s installed successfully.\n"), szServiceName);
			return 0;
		}

	} else if (SVC_REMOVE == CmdType) {

		LPTSTR szServiceName = argv[1];

		BOOL fSuccess = NdasDiDeleteService(szServiceName);

		if (!fSuccess) {
			print_error();
			return 254;
		} else {
			_ftprintf(stdout, 
				_T("Service %s deleted successfully.\n"), szServiceName);
			return 0;
		}

	} else if (SVC_FIND == CmdType) {

		LPTSTR szServiceName = argv[1];

		BOOL fPendingDeletion;
		BOOL fSuccess = NdasDiFindService(szServiceName, &fPendingDeletion);

		if (!fSuccess) {
			print_error();
			return 254;
		} else {
			_ftprintf(stdout, 
				_T("Service %s exists.%s\n"), 
				szServiceName,
				fPendingDeletion ? _T(" (Marked for deletion)") : _T(""));
			return 0;
		}

	} else {
		_ftprintf(stderr, _T("Unknown command.\n"));
		return 254;
	}
}

int drvinst(int argc, LPTSTR *argv)
{
	typedef enum { SVC_INSTALL, SVC_REMOVE, SVC_REMOVE_PNP } COMMAND_TYPE;

	struct { COMMAND_TYPE type; LPCTSTR cmd; int argc; BOOL op; } opts[] = 
	{ 
		{ SVC_INSTALL, L"install", 3, TRUE}, 
		{ SVC_REMOVE, L"remove", 1, FALSE},
		{ SVC_REMOVE_PNP, L"removepnp", 1, FALSE}
	};

	COMMAND_TYPE CmdType;

	const DWORD nopts = RTL_NUMBER_OF(opts);
	DWORD i = 0;

	for (; i < nopts; ++i) {
		if (lstrcmpi(opts[i].cmd, argv[0]) == 0) {
			if ((argc - 1) == opts[i].argc ||
				(opts[i].op && (argc - 1) >= opts[i].argc)) 
			{
				CmdType = opts[i].type;
			} else {
				_ftprintf(stderr, _T("Invalid command arguments.\n"));
				return 254;
			}
			break;
		}
	}

	if (i == nopts) {
		_ftprintf(stderr, _T("Invalid command.\n"));
		return 254;
	}

	if (SVC_INSTALL == CmdType) {

		LPTSTR szSourceFilePath = argv[1];
		LPTSTR szServiceName = argv[2];
		LPTSTR szDisplayName = argv[3];

		LPTSTR szLoadOrderGroup = NULL;
		if (argc > 4) {
			szLoadOrderGroup = argv[4];
		}

		XTLTRACE1(TRACE_LEVEL_INFORMATION, 
			_T("Source: %s, ServiceName: %s, DisplayName: %s, LoadOrder: %s"),
			szSourceFilePath, szServiceName, szDisplayName, szLoadOrderGroup);

		BOOL bUpdated;
		BOOL fSuccess = NdasDiInstallOrUpdateDriverService(
			szSourceFilePath,
			szServiceName,
			szDisplayName,
			_T(""),
			SERVICE_KERNEL_DRIVER,
			SERVICE_SYSTEM_START,
			SERVICE_ERROR_NORMAL,
			szLoadOrderGroup,
			NULL,
			NULL,
			&bUpdated);

		if (!fSuccess) {
			print_error();
			return 254;
		} else {
			if (bUpdated)
			{
				_ftprintf(stdout, 
					_T("Driver Service %s updated successfully.\n"), szServiceName);
			}
			else
			{
				_ftprintf(stdout, 
					_T("Driver Service %s installed successfully.\n"), szServiceName);
			}
			return 0;
		}

	} else if (SVC_REMOVE == CmdType) {

		LPTSTR szServiceName = argv[1];

		BOOL fSuccess = NdasDiDeleteService(szServiceName);

		if (!fSuccess) {
			print_error();
			return 254;
		} else {
			_ftprintf(stdout, 
				_T("Driver Service %s deleted successfully.\n"), szServiceName);
		}

	} else if (SVC_REMOVE_PNP == CmdType) {

		LPTSTR szServiceName = argv[1];

		DWORD cRemoved;
		BOOL fRebootRequired;
		BOOL fSuccess = NdasDiRemoveLegacyDevice(
			NULL,
			szServiceName,
			FALSE,
			&cRemoved,
			&fRebootRequired,
			NULL,
			NULL);

		if (!fSuccess) {
			print_error();
			return 254;
		}

		_ftprintf(stdout,
			_T("Driver Enum Key (%d) removed successfully.%s\n"),
			cRemoved,
			fRebootRequired ? _T(" (Reboot required)") : _T(""));

	} else {

		_ftprintf(stderr, _T("Unknown command.\n"));
		return 254;

	}

	return 0;
}

int infinst(int argc, TCHAR** argv)
{
	if (argc < 2)
	{
		_ftprintf(stderr, _T("usage: inf <inf-file> <section>\n"));
		return 1;
	}

	LPCTSTR infFile = argv[0];
	LPCTSTR infSection = argv[1];
	BOOL needsReboot = FALSE, needsRebootOnService = FALSE;

	HRESULT hr = xDiInstallFromInfW(
		NULL,
		infFile, 
		infSection,
		0,
		0, 
		&needsReboot,
		&needsRebootOnService);

	if (FAILED(hr))
	{
		_ftprintf(stderr, _T("xDiInstallFromInfW failed.\n\n"));
		print_error(stderr, hr);
		return 1;
	}

	if (needsReboot)
	{
		_ftprintf(stdout, _T("Reboot is required to finalize.\n"));
	}

	if (needsRebootOnService)
	{
		_ftprintf(stdout, _T("Reboot is required for the service to take effect.\n"));
	}

	return 0;
}

int __cdecl _tmain(int argc, TCHAR** argv)
{
	// remove argv 0
	argc--;
	argv++;

	_tprintf(
		_T("NDAS Device Installer\n")
		_T("Copyright (C) 2003-2007 XIMETA, Inc. All rights reserved.\n\n"));

	if (!IsUserAnAdmin())
	{
		_ftprintf(stderr,
			_T("This program requires administrative privileges in most cases.\n")
			_T("You may have to invoke this program running in elevated mode\n")
			_T("\n"));
	}

	if (argc == 0) 
	{
		_ftprintf(stderr, _T("%s"), USAGE);
		return 254;
	}

	static const struct {
		LPCTSTR Command1;
		LPCTSTR Command2;
		int (*Function)(int, TCHAR**);
	} CommandDefitions[] = {
		_T("dev"), _T("device"), pnpdevinst,
		_T("net"), _T("network"), netcomp,
		_T("svc"), _T("service"), svcinst,
		_T("drv"), _T("driver"), drvinst,
		_T("inf"), NULL, infinst
	};

	int ret = 0;
	LPCTSTR command = argv[0];
	size_t i;
	for (i = 0; i < RTL_NUMBER_OF(CommandDefitions); ++i)
	{
		if (0 == lstrcmpi(CommandDefitions[i].Command1, command) ||
			0 == lstrcmpi(CommandDefitions[i].Command2, command))
		{
			--argc; ++argv;
			 ret = (*CommandDefitions[i].Function)(argc, argv);
			 break;
		}
	}
	if (i == RTL_NUMBER_OF(CommandDefitions))
	{
		_ftprintf(stderr, _T("%s"), USAGE);
		return 254;
	}

	return ret;
}

#if 0
#if (WINVER >= 0x0501)
DWORD Flags = 0; // SUOI_FORCEDELETE;
fSuccess = SetupUninstallOEMInf(
								argv[2],
								Flags,
								NULL);

if (!fSuccess) {
	_ftprintf(stderr, _T("Error %d (%08X)\n"), 
		GetLastError(),
		GetLastError());
} else {
	_ftprintf(stdout, _T("OEM Inf %d is deleted successfully.\n"), argv[2]);
}
#else
_ftprintf(stderr, _T("This feature is only available on Windows XP"));
#endif
#endif
