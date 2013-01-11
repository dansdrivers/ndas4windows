#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <crtdbg.h>

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#include <ndas/ndasuser.h>
#include <ndas/ndasevent_str.h>
#include <ndas/ndastype_str.h>

#include "argp.h"
#include "ndascmd.h"
#include "hexstr.h"
#include "devstr.h"

BOOL check_ndasuser_api_version();
int pusage(LPCTSTR str);
void pheader();
int invoke_cmd_proc(int argc, TCHAR** argv);

void
CALLBACK
ndascmd_logicaldevice_status_change_callback(
	DWORD dwError,
	PNDAS_EVENT_INFO pEventInfo,
	LPVOID lpContext);

int __cdecl _tmain(int argc, TCHAR** argv)
{
	INT iPos, iRet;

    /* NDASUSER API version check */
    if (!check_ndasuser_api_version())
    {
        return 1;
    }
    
	iPos = get_named_arg_pos(argc, argv, _T("nologo"), NULL);
	if (-1 == iPos) pheader();

	iRet = invoke_cmd_proc(argc - 1, argv + 1);

#ifdef _DEBUG
	_tprintf(_T("DBG: returned '%d'\n"), iRet);
#endif

	return iRet;
}

int pusage(LPCTSTR str)
{
	_tprintf(_T("%s\n"), str);
	return 22; /* EINVAL */
}

void pheader()
{
	_tprintf(
		_T("NDAS Device Management Command Version 1.00\n")
		_T("Copyright (C) 2003-2004 XIMETA, Inc.\n\n"));
}

int invoke_cmd_proc(int argc, TCHAR** argv)
{
	CONST struct {
		LPCTSTR cmds;
		int (*cmdproc)(int argc, TCHAR** argv);
	} CommandSet[] = {
		{ _T("register"), ndascmd_register_device },
#ifdef _DEBUG
		{ _T("registerex"), ndascmd_register_device_ex },
#endif
		{ _T("unregister"), ndascmd_unregister_device },
		{ _T("list devices"), ndascmd_enum_devices },
		{ _T("list logdevices"), ndascmd_enum_logicaldevices },
		{ _T("mount"), ndascmd_mount },
		{ _T("unmount"), ndascmd_unmount },
		{ _T("activate"), ndascmd_enable },
		{ _T("deactivate"), ndascmd_disable },
#ifdef _DEBUG
		{ _T("trace events"), ndascmd_trace_events },
#endif
		{ _T("query device hosts"), ndascmd_query_unitdevice_hosts}
	};

#define COMMAND_SET_SIZE RTL_NUMBER_OF(CommandSet)

	INT nArgUsed;
	int i, j, n;
	int nCands = COMMAND_SET_SIZE;
	int cands[COMMAND_SET_SIZE] = {0};

	if (0 == argc)
	{
		_tprintf(_T("Type 'ndascmd help' for usage.\n"));
		return EXIT_FAILURE;
	}

	if (1 == argc &&
		(0 == lstrcmpi(_T("help"), argv[0]) ||
		0 == lstrcmpi(_T("/help"), argv[0]) ||
		0 == lstrcmpi(_T("-help"), argv[0]) ||
		0 == lstrcmpi(_T("/?"), argv[0]) ||
		0 == lstrcmpi(_T("-?"), argv[0]) ||
		0 == lstrcmpi(_T("/h"), argv[0]) ||
		0 == lstrcmpi(_T("-h"), argv[0])))
	{
		_tprintf(_T("Available commands:\n"));
		for (i = 0; i < COMMAND_SET_SIZE; ++i)
		{
			_tprintf(_T("  "));
			_tprintf(_T("%s "), CommandSet[i].cmds);
			(void) CommandSet[i].cmdproc(-1, NULL);
		}
		return EXIT_SUCCESS;
	}

	for (i = 0; i < COMMAND_SET_SIZE; ++i) 
	{
		cands[i] = i;
	}

	for (i = 0; i < argc; ++i)
	{
		if (!argp_is_arg_cmd_type(argv[i])) break;

		n = 0;
		for (j = 0; j < nCands; ++j)
		{
			if (argp_is_arg_in_cmds(CommandSet[cands[j]].cmds, i, argv[i]))
			{
				cands[n++] = cands[j];
			}
		}
		nCands = n;

		if (0 == nCands) break;
		if (1 == nCands) break;
	}

	nArgUsed = i + 1;

	if (0 == nCands)
	{
		_tprintf(_T("Unknown command: %s.\n"), argv[0]);
		_tprintf(_T("Type 'ndascmd help' for usage.\n"));
		return EXIT_FAILURE;
	}
	else if (nCands > 1)
	{
		_tprintf(_T("Ambiguous command.\n"));
		_tprintf(_T("Type 'ndascmd help' for usage.\n"));
		return EXIT_FAILURE;
	}
	/* if the command part is not used up, we should consume those */
	else if (1 == nCands)
	{
		INT nParts = arpg_cmd_part_count(CommandSet[cands[0]].cmds);
		for (i = nArgUsed; i < nParts; ++i)
		{
			if (i >= argc ||
				!argp_is_arg_cmd_type(argv[i]))
			{
				_tprintf(_T("Unknown command: %s.\n"), argv[0]);
				_tprintf(_T("Type 'ndascmd help' for usage.\n"));
				return EXIT_FAILURE;
			}
			if (!argp_is_arg_in_cmds(CommandSet[cands[0]].cmds, i, argv[i]))
			{
				_tprintf(_T("Unknown command: %s.\n"), argv[0]);
				_tprintf(_T("Type 'ndascmd help' for usage.\n"));
				return EXIT_FAILURE;
			}
		}
		nArgUsed = nParts;
	}

#ifdef _DEBUG
	_tprintf(_T("DBG: invoking '%s'"), CommandSet[cands[0]].cmds);
	if (argc - nArgUsed > 0)
	{
		_tprintf(_T(" -"));
		for (j = nArgUsed; j < argc; ++j)
		{
			_tprintf(_T(" '%s'"), argv[j]);
		}
	}
	_tprintf(_T("\n"));
#endif
	return CommandSet[cands[0]].cmdproc(argc - (nArgUsed), argv + nArgUsed);
}

BOOL check_ndasuser_api_version()
{
    DWORD apiver = NdasGetAPIVersion();
    if (NDASUSER_API_VERSION_MAJOR != LOWORD(apiver) ||
        NDASUSER_API_VERSION_MINOR != HIWORD(apiver))
    {
        _tprintf(
            _T("Loaded NDASUSER.DLL API Version %d.%d does not comply with this program.")
            _T(" Expecting API Version %d.%d.\n"),
            LOWORD(apiver), HIWORD(apiver),
            NDASUSER_API_VERSION_MAJOR, NDASUSER_API_VERSION_MINOR);
        return FALSE;
    }
    return TRUE;
}

int ndascmd_register_device(int argc, TCHAR** argv)
{
#ifdef _DEBUG
	static LPCTSTR USAGE = _T("[/hide] [/volatile] <device name> <device id> [write key]");
#else
	static LPCTSTR USAGE = _T("<device name> <device id> [write key]");
#endif
	INT pos = -1;
	LPCTSTR lpDeviceName = NULL;
	LPCTSTR lpDeviceID = NULL;
	LPCTSTR lpDeviceWriteKey = NULL;
	/* for normalized device string id */
	TCHAR szDeviceID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	DWORD dwRegFlags = NDAS_DEVICE_REG_FLAG_NONE;

	if (-1 == argc) return pusage(USAGE);

	/* Device Name */
	pos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 != pos) lpDeviceName = argv[pos];
	/* Device Name is a mandatory argument */
	else return pusage(USAGE);

	/* Device ID */
	pos = get_unnamed_arg_pos(argc, argv, 1);
	if (-1 != pos) lpDeviceID = argv[pos];
	/* Device ID is a mandatory argument */
	else return pusage(USAGE); 

	/* Normalize Device ID */
	if (NDAS_DEVICE_STRING_ID_LEN != 
		normalize_device_string_id(
			szDeviceID, 
			NDAS_DEVICE_STRING_ID_LEN, 
			lpDeviceID))
	{
		return pusage(USAGE);
	}

	/* Write Key */
	pos = get_unnamed_arg_pos(argc, argv, 2);
	if (-1 != pos) lpDeviceWriteKey = argv[pos];
	/* Write Key is an optional argument */

#ifdef _DEBUG
	/* /hide option */
	pos = get_named_arg_pos(argc, argv, _T("hide"), NULL);
	if (-1 != pos) dwRegFlags |= NDAS_DEVICE_REG_FLAG_HIDDEN;

	/* /volatile option */
	pos = get_named_arg_pos(argc, argv, _T("volatile"), NULL);
	if (-1 != pos) dwRegFlags |= NDAS_DEVICE_REG_FLAG_VOLATILE;
#endif

	{
		/* Register a device */
		DWORD dwSlotNo = NdasRegisterDevice(
			szDeviceID, 
			lpDeviceWriteKey, 
			lpDeviceName,
			dwRegFlags);
		/* 0 means a failure. Slot number is always larger than 0. */
		if (0 == dwSlotNo)
		{
			return NC_PrintLastErrMsg(), EXIT_FAILURE;
		}
		/* When a device is registered, its initial status is "DISABLED" */
		(VOID) NdasEnableDevice(dwSlotNo, TRUE);
	}

	_tprintf(_T("NDAS device is registered successfully.\n"));

	return EXIT_SUCCESS;
}

int ndascmd_register_device_ex(int argc, TCHAR** argv)
{
#ifdef _DEBUG
	static LPCTSTR USAGE = _T("[/hide] [/volatile] [/oemcode:<oemcode>] <device name> <device id> [write key]");
#else
	static LPCTSTR USAGE = _T("<device name> <device id> [write key]");
#endif
	INT pos = -1;
	LPCTSTR lpDeviceName = NULL;
	LPCTSTR lpDeviceID = NULL;
	LPCTSTR lpDeviceWriteKey = NULL;
#ifdef _DEBUG
	LPCTSTR lpOemCode = NULL;
#endif
	/* for normalized device string id */
	TCHAR szDeviceID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	DWORD dwRegFlags = NDAS_DEVICE_REG_FLAG_NONE;
	NDAS_OEM_CODE regOemCode = {0};

	if (-1 == argc) return pusage(USAGE);

	/* Device Name */
	pos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 != pos) lpDeviceName = argv[pos];
	/* Device Name is a mandatory argument */
	else return pusage(USAGE);

	/* Device ID */
	pos = get_unnamed_arg_pos(argc, argv, 1);
	if (-1 != pos) lpDeviceID = argv[pos];
	/* Device ID is a mandatory argument */
	else return pusage(USAGE); 

	/* Normalize Device ID */
	if (NDAS_DEVICE_STRING_ID_LEN != 
		normalize_device_string_id(
			szDeviceID, 
			NDAS_DEVICE_STRING_ID_LEN, 
			lpDeviceID))
	{
		return pusage(USAGE);
	}

#ifdef _DEBUG
	_tprintf(_T("DeviceID: %s\n"), szDeviceID);
#endif

	/* Write Key */
	pos = get_unnamed_arg_pos(argc, argv, 2);
	if (-1 != pos) lpDeviceWriteKey = argv[pos];
	/* Write Key is an optional argument */

#ifdef _DEBUG
	/* /hide option */
	pos = get_named_arg_pos(argc, argv, _T("hide"), NULL);
	if (-1 != pos) dwRegFlags |= NDAS_DEVICE_REG_FLAG_HIDDEN;

	/* /volatile option */
	pos = get_named_arg_pos(argc, argv, _T("volatile"), NULL);
	if (-1 != pos) dwRegFlags |= NDAS_DEVICE_REG_FLAG_VOLATILE;

	/* oem code */
	pos = get_named_arg_pos(argc, argv, _T("oemcode"), &lpOemCode);
	if (-1 != pos)
	{
		DWORD nConverted;
		if (NULL == lpOemCode)
		{
			return pusage(USAGE);
		}
		dwRegFlags |= NDAS_DEVICE_REG_FLAG_USE_OEM_CODE;
		nConverted = string_to_hex(
			lpOemCode, 
			(BYTE*)&regOemCode, 
			sizeof(NDAS_OEM_CODE));
		if (sizeof(NDAS_OEM_CODE) != nConverted)
		{
			return pusage(USAGE);
		}
	}
#endif

	{
		DWORD dwSlotNo;
		NDAS_DEVICE_REGISTRATION reg;

		reg.Size = sizeof(NDAS_DEVICE_REGISTRATION);
		reg.RegFlags = dwRegFlags;
		reg.DeviceStringId = szDeviceID;
		reg.DeviceStringKey = lpDeviceWriteKey;
		reg.DeviceName = lpDeviceName;
		CopyMemory(
			reg.OEMCode.Bytes, 
			regOemCode.Bytes, 
			sizeof(NDAS_OEM_CODE));

		/* Register a device */
		dwSlotNo = NdasRegisterDeviceEx(&reg);

		/* 0 means a failure. Slot number is always larger than 0. */
		if (0 == dwSlotNo)
		{
			return NC_PrintLastErrMsg(), EXIT_FAILURE;
		}
		/* When a device is registered, its initial status is "DISABLED" */
		(VOID) NdasEnableDevice(dwSlotNo, TRUE);
	}

	_tprintf(_T("NDAS device is registered successfully.\n"));

	return EXIT_SUCCESS;
}

int ndascmd_unregister_device(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("<device number>");

	INT pos;
	BOOL fSuccess;
	DWORD dwSlotNo;

	if (-1 == argc) return pusage(USAGE);

	/* Device Slot Number  */
	pos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 != pos) dwSlotNo = _ttoi(argv[pos]);
	/* Device Slot Number is a mandatory argument */
	else return pusage(USAGE);

	/* Unregister */
	fSuccess = NdasUnregisterDevice(dwSlotNo);
	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	_tprintf(_T("NDAS device is unregistered successfully.\n"));

	return EXIT_SUCCESS;
}

BOOL
CALLBACK
ndascmd_enum_unitdevices_proc(
	NDASUSER_UNITDEVICE_ENUM_ENTRY* pEntry,
	LPVOID lpContext)
{
	BOOL fSuccess = FALSE;
	NDASUSER_UNITDEVICE_INFORMATION udinfo = {0};
	DWORD dwSlotNo = *((DWORD*)lpContext);
	NDAS_LOGICALDEVICE_ID ldid = 0;

	fSuccess = NdasQueryUnitDeviceInformation(
		dwSlotNo,
		pEntry->UnitNo,
		&udinfo);

	if (!fSuccess) 
	{
		/* returns TRUE to continue enumeration */
		return TRUE;
	}

	udinfo.UnitDeviceSubType;
	udinfo.HardwareInfo.SectorCountLowPart;
	udinfo.HardwareInfo.SectorCountHighPart;
	udinfo.HardwareInfo.MediaType;

	_tprintf(_T("    Unit Device %d: %s"),
		pEntry->UnitNo,
		NC_UnitDeviceTypeString(udinfo.UnitDeviceType));

	fSuccess = NdasFindLogicalDeviceOfUnitDevice(
		dwSlotNo,
		pEntry->UnitNo,
		&ldid);

	if (fSuccess) 
	{
		_tprintf(_T(" (Logical Device Number: %d)"), ldid);
	}

	_tprintf(_T("\n"));

	return TRUE;
}

BOOL 
CALLBACK 
ndascmd_enum_devices_proc(
	NDASUSER_DEVICE_ENUM_ENTRY* pEntry,
	LPVOID lpContext)
{
	BOOL fSuccess = FALSE;
	NDASUSER_DEVICE_INFORMATIONW ndi = {0};
	TCHAR szDisplayID[30];

	UNREFERENCED_PARAMETER(lpContext);

	fSuccess = NdasQueryDeviceInformationW(pEntry->SlotNo, &ndi);
	_ASSERTE(fSuccess);
	if (!fSuccess)
	{
		return TRUE;
	}

	_tprintf(_T("%2d: Name: %s\n"), 
		pEntry->SlotNo, 
		pEntry->szDeviceName);

	fSuccess = NC_NdasDeviceIDToString(szDisplayID, RTL_NUMBER_OF(szDisplayID), 
		pEntry->szDeviceStringId);
	_ASSERTE(fSuccess);

	_tprintf(_T("    ID  : %s (%s)"),
		szDisplayID,
		(pEntry->GrantedAccess& GENERIC_WRITE) ? _T("RW") : _T("RO"));

	if (ndi.DeviceParams.RegFlags)
	{
		_tprintf(_T(" (Flag: %08x)"), ndi.DeviceParams.RegFlags);
	}

	_tprintf(_T("\n"));

	fSuccess = NdasEnumUnitDevices(
		pEntry->SlotNo,
		ndascmd_enum_unitdevices_proc,
		&pEntry->SlotNo);
	_ASSERTE(fSuccess);

	_tprintf(_T("\n"));

	return TRUE;
}

int ndascmd_enum_devices(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("");

	BOOL fSuccess;

	UNREFERENCED_PARAMETER(argv);

	if (-1 == argc) return pusage(USAGE);

	fSuccess = NdasEnumDevices(ndascmd_enum_devices_proc, NULL);

	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


BOOL
CALLBACK
ndascmd_enum_logicaldevice_member_proc(
	PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry, LPVOID lpContext)
{
	BOOL fSuccess;
	BOOL fMultiple = *((BOOL*) lpContext);
	NDASUSER_DEVICE_INFORMATION ndasDeviceInfo;

	if (fMultiple)
	{
		_tprintf(_T("     %d: "), (lpEntry->Index + 1));
	}
	else
	{
		_tprintf(_T("     "));
	}

	fSuccess = NdasQueryDeviceInformationById(
		lpEntry->szDeviceStringId,
		&ndasDeviceInfo);

	if (fSuccess)
	{
		_tprintf(_T("%s"), ndasDeviceInfo.szDeviceName);
	}
	else
	{
		TCHAR szDisplayID[30];
		fSuccess = NC_NdasDeviceIDToString(
			szDisplayID, 
			RTL_NUMBER_OF(szDisplayID), 
			lpEntry->szDeviceStringId);
		_ASSERTE(fSuccess);
		_tprintf(_T("%s"), szDisplayID);
	}

	if (lpEntry->UnitNo > 0)
	{
		_tprintf(_T(":%d"), lpEntry->UnitNo);
	}

	{
		NDAS_UNITDEVICE_STATUS status;
		NDAS_UNITDEVICE_ERROR error;
		fSuccess = NdasQueryUnitDeviceStatusById(
			lpEntry->szDeviceStringId,
			lpEntry->UnitNo,
			&status,
			&error);
		if (!fSuccess)
		{
			_tprintf(_T(" (N/A)"));
		}
	}
	
	_tprintf(_T("\n"));

	return TRUE;
}

BOOL
CALLBACK
ndascmd_enum_logicaldevices_proc(
	PNDASUSER_LOGICALDEVICE_ENUM_ENTRY pEntry,
	LPVOID lpContext)
{
	BOOL fSuccess = FALSE;
	NDAS_LOGICALDEVICE_STATUS status = 0;
	NDAS_LOGICALDEVICE_ERROR lastError = 0;
	NDASUSER_LOGICALDEVICE_INFORMATION info = {0};

	UNREFERENCED_PARAMETER(lpContext);

	/*
	 (1, 0, 0) DISK_SINGLE (80 GB) (RW) - MOUNTED (RW)
	 (1, 0, 0) DISK_AGGREGATED (72 GB) (RO) - MOUNT_PENDING
	 (1, 0, 0) DISK_MIRRORED (10.1 MB) (RO) - UNMOUNTED (ERROR)
	 (1, 0, 0) DISK_MIRRORED (10.1 MB) (RO) - UNMOUNTED (ERROR)
	 (2, 0, 0) DVD (RO) - MOUNTED (RW)
	*/

	_tprintf(_T("%2d: %s"),
		pEntry->LogicalDeviceId,
		NdasLogicalDeviceTypeString(pEntry->Type));

	fSuccess = NdasQueryLogicalDeviceInformation(
		pEntry->LogicalDeviceId, &info);

	if (!fSuccess) 
	{
		_tprintf(_T(" (Information Unavailable)\n"));
		NC_PrintLastErrMsg();
		// return TRUE;
	} 
	else 
	{
		if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(info.LogicalDeviceType)) 
		{
			TCHAR szBlockSize[30];
			fSuccess = NC_BlockSizeToString(
				szBlockSize, 
				RTL_NUMBER_OF(szBlockSize), 
				info.SubType.LogicalDiskInformation.Blocks);
			_ASSERTE(fSuccess);
			_tprintf(_T(" (%s)"), szBlockSize);
		}
		_tprintf(_T(" (%s)"), /* Granted Access (Available) */
			(info.GrantedAccess & GENERIC_WRITE) ? _T("RW") : _T("RO"));

	}

	fSuccess = NdasQueryLogicalDeviceStatus(
		pEntry->LogicalDeviceId, &status, &lastError);

	if (!fSuccess) 
	{
		_tprintf(_T("- (Status Unknown)\n"));
		return TRUE;
	}

	_tprintf(_T(" - %s"), NdasLogicalDeviceStatusString(status));

	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status) 
	{
		_tprintf(_T(" (%s)"),  /* Mounted Access (Current) */
			(info.MountedAccess & GENERIC_WRITE) ?
			_T("RW") : _T("RO"));
	}

	if (NDAS_LOGICALDEVICE_ERROR_NONE != lastError) 
	{
		_tprintf(_T(" (%s)"), NdasLogicalDeviceErrorString(lastError));
	}

	_tprintf(_T("\n"));

	{
		BOOL fMultiple = (info.nUnitDeviceEntries > 1);
		fSuccess = NdasEnumLogicalDeviceMembers(
			pEntry->LogicalDeviceId, 
			ndascmd_enum_logicaldevice_member_proc,
			&fMultiple);
	}

	_tprintf(_T("\n"));

	return TRUE;
}

int ndascmd_enum_logicaldevices(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("");

	BOOL fSuccess;

	UNREFERENCED_PARAMETER(argv);

	if (-1 == argc) return pusage(USAGE);

	fSuccess = NdasEnumLogicalDevices(
		ndascmd_enum_logicaldevices_proc, 
		NULL);

	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int 
ndascmd_wait_for_logical_device_status_change_imp3(
	HANDLE status_change_event,
	HNDASEVENTCALLBACK callback_handle,
	NDAS_LOGICALDEVICE_ID logical_device_id,
	NDAS_LOGICALDEVICE_STATUS initial_status)
{
	UNREFERENCED_PARAMETER(callback_handle);

#pragma warning(disable: 4127) /* conditional expression is constant */

	while (TRUE)
	{
		DWORD last_error;
		DWORD waitResult;
		NDAS_LOGICALDEVICE_STATUS status;

		BOOL fSuccess = NdasQueryLogicalDeviceStatus(
			logical_device_id, 
			&status, 
			&last_error);

		if (!fSuccess)
		{
			return NC_PrintLastErrMsg(), EXIT_FAILURE;
		}

		if (initial_status != status)
		{
			break;
		}

		waitResult = WaitForSingleObject(
			status_change_event, 
			INFINITE);

		if (WAIT_OBJECT_0 != waitResult)
		{
			return NC_PrintLastErrMsg(), EXIT_FAILURE;
		}
	}

	return ERROR_SUCCESS;

#pragma warning(default: 4127)
}

int 
ndascmd_wait_for_logical_device_status_change_imp2(
	HANDLE status_change_event,
	NDAS_LOGICALDEVICE_ID logical_device_id,
	NDAS_LOGICALDEVICE_STATUS initial_status)
{
	int result;
	BOOL success;

	HNDASEVENTCALLBACK callback_handle = NdasRegisterEventCallback(
		ndascmd_logicaldevice_status_change_callback,
		status_change_event);

	if (NULL == callback_handle)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	result = ndascmd_wait_for_logical_device_status_change_imp3(
		status_change_event,
		callback_handle,
		logical_device_id,
		initial_status);

	success = NdasUnregisterEventCallback(callback_handle);
	_ASSERTE(success);

	return result;

}

int 
ndascmd_wait_for_logical_device_status_change(
	NDAS_LOGICALDEVICE_ID logical_device_id,
	NDAS_LOGICALDEVICE_STATUS initial_status)
{
	int result;
	BOOL success;
	HANDLE status_change_event;
		
	status_change_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	if (NULL == status_change_event)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	result = ndascmd_wait_for_logical_device_status_change_imp2(
		status_change_event, 
		logical_device_id,
		initial_status);

	success = CloseHandle(status_change_event);
	_ASSERTE(success);

	return result;
}

int ndascmd_mount(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("[/wait] {<ldn> | /device <dn>} {ro|rw}");

	BOOL wait_for_completion;
	NDAS_LOGICALDEVICE_ID ld_id;
	BOOL rw_mode;
	int pos;
	BOOL fSuccess;

	if (-1 == argc) return pusage(USAGE);

	pos = get_unnamed_arg_pos(argc, argv, 1);
	if (-1 == pos)
	{
		return pusage(USAGE);
	}

	if (0 == lstrcmpi(_T("rw"), argv[pos]))
	{
		rw_mode = TRUE;
	}
	else if (0 == lstrcmpi(_T("ro"), argv[pos]))
	{
		rw_mode = FALSE;
	}
	else
	{
		return pusage(USAGE);
	}

	pos = get_named_arg_pos(argc, argv, _T("device"), NULL);
	if (-1 != pos)
	{
		pos = get_unnamed_arg_pos(argc, argv, 0);
		fSuccess = NC_ResolveLDIDFromSlotUnit(argv[pos], &ld_id);
		if (!fSuccess) 
		{
			return EXIT_FAILURE;
		}
	}
	else
	{
		pos = get_unnamed_arg_pos(argc, argv, 0);
		ld_id = _ttoi(argv[pos]);
	}

	pos = get_named_arg_pos(argc, argv, _T("wait"), NULL);
	wait_for_completion = (-1 != pos) ? TRUE : FALSE;

	fSuccess = NdasPlugInLogicalDevice(rw_mode, ld_id);
	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	if (wait_for_completion)
	{
		int result = ndascmd_wait_for_logical_device_status_change(
			ld_id,
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING);
		return result;
	}

	return EXIT_SUCCESS;
}

int ndascmd_unmount(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("[/force] [/wait] {<ldn> | /device <dn>}");

	BOOL wait_for_completion;
	BOOL to_unplug;
	NDAS_LOGICALDEVICE_ID ld_id;
	int pos;
	BOOL fSuccess;

	if (-1 == argc) return pusage(USAGE);

	pos = get_named_arg_pos(argc, argv, _T("force"), NULL);
	to_unplug = (-1 != pos) ? TRUE : FALSE;

	pos = get_named_arg_pos(argc, argv, _T("wait"), NULL);
	wait_for_completion = (-1 != pos) ? TRUE : FALSE;

	pos = get_named_arg_pos(argc, argv, _T("device"), NULL);
	if (-1 != pos)
	{
		pos = get_unnamed_arg_pos(argc, argv, 0);
		fSuccess = NC_ResolveLDIDFromSlotUnit(argv[pos], &ld_id);
		if (!fSuccess) 
		{
			return EXIT_FAILURE;
		}
	}
	else
	{
		pos = get_unnamed_arg_pos(argc, argv, 0);
		ld_id = _ttoi(argv[pos]);
	}

	fSuccess = (to_unplug) ? 
		NdasUnplugLogicalDevice(ld_id) :
		NdasEjectLogicalDevice(ld_id);

	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	if (wait_for_completion)
	{
		int result = ndascmd_wait_for_logical_device_status_change(
			ld_id,
			NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING);
		return result;
	}

	return EXIT_SUCCESS;
}

int ndascmd_enable(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("<device-number>");

	INT iPos;
	DWORD dwSlotNo;
	BOOL fSuccess;

	if (-1 == argc) return pusage(USAGE);

	iPos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 == iPos) return pusage(USAGE);

	dwSlotNo = _ttoi(argv[iPos]);

	fSuccess = NdasEnableDevice(dwSlotNo, TRUE);

	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int ndascmd_disable(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("<device-number>");

	INT iPos;
	DWORD dwSlotNo;
	BOOL fSuccess;

	if (-1 == argc) return pusage(USAGE);

	iPos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 == iPos) return pusage(USAGE);

	dwSlotNo = _ttoi(argv[iPos]);

	fSuccess = NdasEnableDevice(dwSlotNo, FALSE);

	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int ndascmd_query_device_status(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("<device-number>");

	INT iPos;
	BOOL fSuccess;
	DWORD dwSlotNo;
	NDAS_DEVICE_STATUS status;
	NDAS_DEVICE_ERROR lastError;

	if (-1 == argc) return pusage(USAGE);

	iPos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 == iPos) return pusage(USAGE);

	dwSlotNo = _ttoi(argv[iPos]);

	fSuccess = NdasQueryDeviceStatus(dwSlotNo, &status, &lastError);

	if (!fSuccess) 
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	_tprintf(_T("Status    : 0x%08X (%s)\n"), status, NdasDeviceStatusString(status));
	_tprintf(_T("Last Error: 0x%08X (%s)\n"), lastError, NdasDeviceErrorString(lastError));

	return EXIT_SUCCESS;
}

BOOL CALLBACK 
ndascmd_query_unitdevice_hosts_enum_proc(
	CONST GUID* lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext)
{
	BOOL fSuccess;
	TCHAR szHostID[30];
	NDAS_HOST_INFO hostInfo;

	UNREFERENCED_PARAMETER(lpContext);

	fSuccess = NC_GuidToString(szHostID, RTL_NUMBER_OF(szHostID), lpHostGuid);
	_ASSERTE(fSuccess);

	_tprintf(_T("%s - %s: "), szHostID,
		(Access & GENERIC_WRITE) ? _T("RW") : _T("RO"));

	fSuccess = NdasQueryHostInfo(lpHostGuid, &hostInfo);

	if (!fSuccess) 
	{
		_tprintf(_T("no information available"));
		NC_PrintLastErrMsg();
	} 
	else 
	{
		_tprintf(_T("%s"), hostInfo.szHostname);
	}
	_tprintf(_T("\n"));

	return TRUE;
}

int ndascmd_query_unitdevice_hosts(int argc, TCHAR** argv)
{
	/* Although querying is for a unit device, the actual
	   command is shown as device-number, in which case
	   <device-number> will be translated as <device-number>:0.
	   For other than 0 as a unit number, we also accept
	   <device-number>:<unit-number> format even if not explicit. */

	static LPCTSTR USAGE = _T("<device-number>");

	INT iPos;
	BOOL fSuccess;
	DWORD dwSlotNo;
	DWORD dwUnitNo;

	if (-1 == argc) return pusage(USAGE);

	iPos = get_unnamed_arg_pos(argc, argv, 0);
	if (-1 == iPos) return pusage(USAGE);

	fSuccess = NC_ParseSlotUnitNo(argv[iPos], &dwSlotNo, &dwUnitNo);
	if (!fSuccess) return EXIT_FAILURE;

	fSuccess = NdasQueryHostsForUnitDevice(
		dwSlotNo,
		dwUnitNo,
		ndascmd_query_unitdevice_hosts_enum_proc,
		NULL);

	if (!fSuccess) 
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void
CALLBACK
ndascmd_logicaldevice_status_change_callback(
	DWORD dwError,
	PNDAS_EVENT_INFO pEventInfo,
	LPVOID lpContext)
{
	HANDLE callback_event = (HANDLE) lpContext;
	_ASSERTE(NULL != callback_event);

	UNREFERENCED_PARAMETER(dwError);

	if (NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED == pEventInfo->EventType)
	{
		SetEvent(callback_event);
	}
}
