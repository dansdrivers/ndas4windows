// ndascmd.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ndasuser.h"
#include "ndasevent_str.h"
#include "ndastype_str.h"

#define STRIP_PREFIX(prefix, str) \
	((LPCTSTR)(((LPCTSTR)(str)) + ((sizeof(prefix) - 1) / sizeof(TCHAR))))

__inline void PrintErrorMessage(DWORD dwError)
{
	if (dwError & APPLICATION_ERROR_MASK) {
		_tprintf(_T("NDAS Error: %d (0x%08X)\n"), dwError, dwError);
		return;
	} else {
		LPTSTR lpszErrorMessage;
		BOOL fSuccess = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, dwError, 0, (LPTSTR) &lpszErrorMessage, 0, NULL);
		if (!fSuccess) {
			_tprintf(_T("System Error: %d (0x%08X): (not available)\n"), dwError, dwError);
			return;
		}
		_tprintf(_T("System Error: %d (0x%08X): %s"), dwError, dwError, lpszErrorMessage);
		(VOID) LocalFree(lpszErrorMessage);
	}
}

__inline void PrintLastErrorMessage()
{
	PrintErrorMessage(GetLastError());
}

int CpNull(int argc, _TCHAR* argv[])
{
	return 0;
}

int CpRegisterDevice(int argc, _TCHAR* argv[])
{
	LPTSTR szKey = NULL;
	BOOL fSuccess = FALSE;

	if (argc == 2) {
	} else if (argc == 3) {
		szKey = argv[2];
	} else {
		_tprintf(_T("register <device id> <device name> [device key]\n"));
		return -1;
	}

	fSuccess = NdasRegisterDevice(argv[0], szKey, argv[1]);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpUnregisterDevice(int argc, _TCHAR* argv[])
{
	DWORD dwSlotNo;
	BOOL fSuccess;

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasUnregisterDevice(dwSlotNo);

	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

BOOL CALLBACK EnumDeviceProc(
	NDASUSER_DEVICE_ENUM_ENTRY* pEntry, 
	LPVOID lpContext)
{
	LPDWORD lpCount = (LPDWORD)(lpContext);
	++(*lpCount);

	_tprintf(_T("[%d] %s (%s, %s)\n"),
		pEntry->SlotNo, 
		pEntry->szDeviceName,
		pEntry->szDeviceStringId, 
		(pEntry->GrantedAccess& GENERIC_WRITE) ? _T("RW") : _T("RO"));

	return TRUE;
}

int CpEnumDevices(int argc, _TCHAR* argv[])
{
	DWORD nCount = 0;
	BOOL fSuccess;
	
	fSuccess = NdasEnumDevicesW(EnumDeviceProc, &nCount);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}
	return 0;
}

int CpEject(int argc, _TCHAR* argv[])
{
	DWORD dwSlotNo;
	BOOL fSuccess;
	NDAS_LOGICALDEVICE_ID id = {0};

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	id.SlotNo = dwSlotNo;

	fSuccess = NdasEjectLogicalDevice(id);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpPlugIn(int argc, _TCHAR* argv[])
{
	BOOL fSuccess;
	DWORD dwSlotNo;
	BOOL bWritable;
	NDAS_LOGICALDEVICE_ID id = {0};

	if (argc != 1 && argc != 2) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	bWritable = FALSE;
	if (argc == 2) {
		if (lstrcmpi(argv[1], _T("rw")) == 0) {
			bWritable = TRUE;
		}
	}

	dwSlotNo = _ttoi(argv[0]);
	_tprintf(_T("Plugging in logical device at slot %d with %s access...\n"),
		dwSlotNo, bWritable ? _T("RW") : _T("RO"));

	id.SlotNo = dwSlotNo;

	fSuccess = NdasPlugInLogicalDevice(bWritable, id);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpUnplug(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;
	NDAS_LOGICALDEVICE_ID id = {0};

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	id.SlotNo = dwSlotNo;

	fSuccess = NdasUnplugLogicalDevice(id);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpEnable(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasEnableDevice(dwSlotNo, TRUE);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpDisable(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasEnableDevice(dwSlotNo, FALSE);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

//
// Not MT-aware function
//
static LPCTSTR GetSizeString(DWORD dwBlocks)
{
	static TCHAR szBuffer[30];
	DWORD dwKB = dwBlocks / 2; // 1 BLOCK = 512 Bytes = 1/2 KB

	static TCHAR* szSuffixes[] = { 
		_T("KB"), _T("MB"), _T("GB"),
		_T("TB"), _T("PB"), _T("EB"), 
		NULL };

	DWORD dwBase = dwKB;
	DWORD dwSub = 0;
	TCHAR ** ppszSuffix = szSuffixes; // KB

	while (dwBase > 1024 && NULL != *(ppszSuffix + 1)) {
		//
		// e.g. 1536 bytes
		// 1536 bytes % 1024 = 512 Bytes
		// 512 bytes * 1000 / 1024 = 500
		// 500 / 100 = 5 -> 0.5
		dwSub = MulDiv(dwBase % 1024, 1000, 1024) / 100;
		dwBase = dwBase / 1024;
		ppszSuffix++;
	}

	if (dwSub == 0) {
		HRESULT hr = StringCchPrintf(
			szBuffer, 
			RTL_NUMBER_OF(szBuffer),
			_T("%d %s"),
			dwBase,
			*ppszSuffix);
		_ASSERTE(SUCCEEDED(hr));
	} else {
		HRESULT hr = StringCchPrintf(
			szBuffer, 
			RTL_NUMBER_OF(szBuffer),
			_T("%d.%d %s"),
			dwBase,
			dwSub,
			*ppszSuffix);
		_ASSERTE(SUCCEEDED(hr));
	}

	return szBuffer;
}

BOOL CALLBACK EnumLogicalDevices(
	PNDASUSER_LOGICALDEVICE_ENUM_ENTRY pEntry, 
	LPVOID lpContext)
{
	BOOL fSuccess = FALSE;
	NDAS_LOGICALDEVICE_STATUS status = 0;
	NDAS_LOGICALDEVICE_ERROR lastError = 0;
	NDASUSER_LOGICALDEVICE_INFORMATION info = {0};

	//
	// (1, 0, 0) DISK_SINGLE (80 GB) (RW) - MOUNTED (RW)
	// (1, 0, 0) DISK_AGGREGATED (72 GB) (RO) - MOUNT_PENDING
	// (1, 0, 0) DISK_MIRRORED (10.1 MB) (RO) - UNMOUNTED (ERROR)
	// (1, 0, 0) DISK_MIRRORED (10.1 MB) (RO) - UNMOUNTED (ERROR)
	// (2, 0, 0) DVD (RO) - MOUNTED (RW)

	_tprintf(_T("(%d, %d, %d) %s"), 
			pEntry->LogicalDeviceId.SlotNo,
			pEntry->LogicalDeviceId.TargetId,
			pEntry->LogicalDeviceId.LUN,
			STRIP_PREFIX(
				_T("NDAS_LOGICALDEVICE_TYPE_"),
				NdasLogicalDeviceTypeString(pEntry->Type)));

	fSuccess = NdasQueryLogicalDeviceInformation(
		pEntry->LogicalDeviceId, &info);

	if (!fSuccess) {
		_tprintf(_T(" (Information Unavailable)\n"));
		return TRUE;
	}

	if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(info.LogicalDeviceType)) {
		_tprintf(_T(" (%s)"),
			GetSizeString(info.LogicalDiskInformation.Blocks));
	}

	_tprintf(_T(" (%s)"), // Granted Access
		(info.GrantedAccess & GENERIC_WRITE) ? _T("RW") : _T("RO"));

	fSuccess = NdasQueryLogicalDeviceStatus(
		pEntry->LogicalDeviceId, &status, &lastError);

	if (!fSuccess) {
		_tprintf(_T("- (Status Unknown)\n"));
		return TRUE;
	}

	_tprintf(_T(" - %s"),
		STRIP_PREFIX(
		_T("NDAS_LOGICALDEVICE_STATUS_"),
		NdasLogicalDeviceStatusString(status)));

	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status) {
		_tprintf(_T(" (%s)"),  // Mounted Access
            (info.MountedAccess & GENERIC_WRITE) ?
			_T("RW") : _T("RO"));
	}

	if (NDAS_LOGICALDEVICE_ERROR_NONE != lastError) {
		_tprintf(_T(" (%s"), // Last Error
			STRIP_PREFIX(
			_T("NDAS_LOGICALDEVICE_ERROR_"),
			NdasLogicalDeviceErrorString(lastError)));
	}

	_tprintf(_T("\n"));

	return TRUE;
}

int CpEnumLogicalDevices(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;

	if (argc != 0) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	fSuccess = NdasEnumLogicalDevices(EnumLogicalDevices, NULL);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpEnumUnitDevices(int argc, _TCHAR* argv[])
{
	return 0;
}

int CpQueryDeviceInfo(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;
	NDASUSER_DEVICE_INFORMATION di = {0};

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasQueryDeviceInformation(dwSlotNo, &di);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	_tprintf(_T("Slot No  : %d\n"), di.SlotNo);
	_tprintf(_T("Device ID: %s\n"), di.szDeviceId);
	_tprintf(_T("Name     : %s\n"), di.szDeviceName);
	_tprintf(_T("Writable : %d\n"), (di.GrantedAccess & GENERIC_WRITE));

//	di.HardwareInfo.dwHwType

	return 0;
}

int CpQueryDeviceStatus(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;
	NDASUSER_DEVICE_INFORMATION di = {0};
	NDAS_DEVICE_STATUS status = 0;
	NDAS_DEVICE_ERROR lastError = 0;

	if (argc != 1) {
		_tprintf(_T("Invalid parameter!\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasQueryDeviceStatus(dwSlotNo, &status, &lastError);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	_tprintf(_T("Status   : 0x%08X (%s)\n"), status, NdasDeviceStatusString(status));
	_tprintf(_T("Last Error: 0x%08X (%s)\n"), lastError, NdasDeviceErrorString(lastError));

	return 0;
}

int CpQueryUnitDeviceInfo(int argc, _TCHAR* argv[])
{
	return 0;
}

int CpQueryUnitDeviceStatus(int argc, _TCHAR* argv[])
{
	return 0;
}

int CpQueryLogicalDeviceInfo(int argc, _TCHAR* argv[])
{
	return 0;
}


int CpQueryLogicalDeviceStatus(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;

	DWORD dwSlotNo = 0;
	DWORD dwTargetId = 0;
	DWORD dwLUN = 0;

	NDAS_LOGICALDEVICE_ID logDevId = {0};
	NDAS_LOGICALDEVICE_STATUS status = 0;
	NDAS_LOGICALDEVICE_ERROR lastError = 0;

	if (argc != 3) {
		_tprintf(_T("usage: ndas query logicaldevice status")
			_T("slot targetid lun\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	dwTargetId = _ttoi(argv[1]);
	dwLUN = _ttoi(argv[2]);

	logDevId.SlotNo = dwSlotNo;
	logDevId.TargetId = dwTargetId;
	logDevId.LUN = dwLUN;

	fSuccess = NdasQueryLogicalDeviceStatus(logDevId, &status, &lastError);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	_tprintf(_T("Status: %s\n"), 
		STRIP_PREFIX(_T("NDAS_LOGICALDEVICE_STATUS_"),
		NdasLogicalDeviceStatusString(status)));

	return 0;
}

VOID CALLBACK EventProc(
	DWORD dwError, 
	PNDAS_EVENT_INFO pEventInfo, 
	LPVOID lpContext)
{
	HANDLE hErrorEvent = (HANDLE)(lpContext);

	if (ERROR_SUCCESS != dwError) {
		PrintLastErrorMessage();
		SetEvent(hErrorEvent);
		return;
	}

	switch (pEventInfo->EventType) {
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS Device %d status changed: %s -> %s.\n"),
			pEventInfo->DeviceInfo.SlotNo,
			NdasDeviceStatusString(pEventInfo->DeviceInfo.OldStatus),
			NdasDeviceStatusString(pEventInfo->DeviceInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d, %d, %d) status changed: %s -> %s.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN,
			NdasLogicalDeviceStatusString(pEventInfo->LogicalDeviceInfo.OldStatus),
			NdasLogicalDeviceStatusString(pEventInfo->LogicalDeviceInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
		_tprintf(_T("NDAS Logical Device (%d, %d, %d) is disconnected.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING:
		_tprintf(_T("NDAS Logical Device (%d, %d, %d) is being reconnected.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTED:
		_tprintf(_T("NDAS Logical Device (%d, %d, %d) is reconnected.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN);
		break;
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
		_tprintf(_T("NDAS Device Entry Changed.\n"));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		_tprintf(_T("NDAS Logical Device Entry Changed.\n"));
		break;
	case NDAS_EVENT_TYPE_TERMINATING:
		_tprintf(_T("Service termination.\n"));
		SetEvent(hErrorEvent);
		break;
	default:
		_tprintf(_T("Unknown Event: 0x%04X.\n"), pEventInfo->EventType);
	}

	_flushall();

	return;
}

int CpShowEvents(int argc, _TCHAR* argv[])
{
	HANDLE hErrorEvent = NULL;
	HNDASEVENTCALLBACK hCallback = NULL;
	INPUT_RECORD inputRecord[128];
	HANDLE hStdIn = INVALID_HANDLE_VALUE;
	BOOL bTerminate = FALSE;

	hErrorEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == hErrorEvent) {
		PrintLastErrorMessage();
		return -1;
	}

	hCallback = NdasRegisterEventCallback(
		EventProc, 
		(LPVOID)(hErrorEvent));
	if (NULL == hCallback) {
		PrintLastErrorMessage();
		return -1;
	}

	hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	if (INVALID_HANDLE_VALUE == hStdIn) {
		PrintLastErrorMessage();
		return -1;
	}

	bTerminate = FALSE;
	while (!bTerminate) {
		DWORD dwWaitResult = WaitForSingleObjectEx(hErrorEvent, 5000, TRUE);

		if (WAIT_OBJECT_0 == dwWaitResult) {
			_tprintf(_T("Event subscription error. Terminating.\n"));
			break;
		} else if (WAIT_IO_COMPLETION == dwWaitResult) {
		} else if (WAIT_TIMEOUT == dwWaitResult) {
			DWORD dwEvents = 0;
			PeekConsoleInput(hStdIn, inputRecord, 128, &dwEvents);
			if (dwEvents > 0) {
				DWORD i;
				ReadConsoleInput(hStdIn, inputRecord, 128, &dwEvents);
				for (i = 0; i < dwEvents; ++i) {
					if (_T('q') == inputRecord[i].Event.KeyEvent.uChar.UnicodeChar) {
						bTerminate = TRUE;
					}
				}
			}
		} else {
			PrintLastErrorMessage();
		}
	}

	NdasUnregisterEventCallback(hCallback);

	return 0;
}

//
// command line client for NDAS device management
//
// usage:
// ndascmd 
//
// help or ?
// register ABCDE12345ABCDE12345 <name> [WriteKey]
// unregister <slot>
// enum devices
// enum logicaldevices
// plugin <slot>
// e]ect <slot>
// unplug <slot>
// en]able <slot>
// disable <slot>
// query device status <slot>
// query device information <slot>
// query unitdevice status <slot> <unitno>
// query unitdevice information <slot> <unitno>
// query logicaldevice status <slot>
// query logicaldevice information <slot>

typedef int (*CMDPROC)(int argc, _TCHAR* argv[]);

typedef struct _CPROC_ENTRY {
	LPCTSTR* szCommands;
	CMDPROC proc;
	DWORD nParamMin;
	DWORD nParamMax;
} CPROC_ENTRY, *PCPROC_ENTRY, *CPROC_TABLE;

#define DEF_COMMAND_1(c,x,h) LPCTSTR c [] = {_T(x), NULL, _T(h), NULL};
#define DEF_COMMAND_2(c,x,y,h) LPCTSTR c [] = {_T(x), _T(y), NULL, _T(h), NULL};
#define DEF_COMMAND_3(c,x,y,z,h) LPCTSTR c [] = {_T(x), _T(y), _T(z), NULL, _T(h), NULL};
#define DEF_COMMAND_4(c,x,y,z,w,h) LPCTSTR c[] = {_T(x), _T(y), _T(z), _T(w), NULL, _T(h), NULL};

DEF_COMMAND_2(_c_reg, "register", "device", "<device id> <name> [write key]")
DEF_COMMAND_2(_c_unreg, "unregister", "device", "<dsn>")
DEF_COMMAND_2(_c_enum_dev, "enumerate", "devices", "")
DEF_COMMAND_2(_c_enum_unitdev, "enumerate", "unitdevices", "")
DEF_COMMAND_2(_c_enum_logdev, "enumerate", "logicaldevices", "")
DEF_COMMAND_2(_c_plugin, "plugin", "logicaldevice", "<lsn> <tid> <lun>")
DEF_COMMAND_2(_c_unplug, "unplug", "logicaldevice", "<lsn> <tid> <lun>")
DEF_COMMAND_2(_c_eject, "eject", "logicaldevice", "<lsn> <tid> <lun>")
DEF_COMMAND_2(_c_enable, "enable", "device", "<dsn>")
DEF_COMMAND_2(_c_disable, "disable", "device", "<dsn>")
DEF_COMMAND_3(_c_query_dev_st, "query","device","status", "<dsn>")
DEF_COMMAND_3(_c_query_dev_info, "query","device","information", "<dsn>")
DEF_COMMAND_3(_c_query_unitdev_st, "query", "unitdevice", "status", "<dsn> <un>")
DEF_COMMAND_3(_c_query_unitdev_info, "query", "unitdevice", "information", "<dsn> <un>")
DEF_COMMAND_3(_c_query_logdev_st, "query", "logicaldevice", "status", "<lsn> <tid> <lun>")
DEF_COMMAND_3(_c_query_logdev_info, "query", "logicaldevice", "information", "<lsn> <tid> <lun>")
DEF_COMMAND_2(_c_show_events, "track", "events", "")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_reg, CpRegisterDevice, 2, 3},
	{ _c_unreg, CpUnregisterDevice, 1},
	{ _c_enum_dev, CpEnumDevices, 0},
	{ _c_enum_unitdev, CpEnumUnitDevices, 1},
	{ _c_enum_logdev, CpEnumLogicalDevices, 0},
	{ _c_plugin, CpPlugIn, 1},
	{ _c_unplug, CpUnplug, 1},
	{ _c_enable, CpEnable, 1},
	{ _c_eject, CpEject, 1},
	{ _c_disable, CpDisable, 1},
	{ _c_query_dev_st, CpQueryDeviceStatus, 1},
	{ _c_query_dev_info, CpQueryDeviceInfo, 1},
	{ _c_query_unitdev_st, CpQueryUnitDeviceStatus, 1},
	{ _c_query_unitdev_info, CpQueryUnitDeviceInfo, 1},
	{ _c_query_logdev_st, CpQueryLogicalDeviceStatus, 1},
	{ _c_query_logdev_info, CpQueryLogicalDeviceInfo, 1},
	{ _c_show_events, CpShowEvents, 0},
};

DWORD GetStringMatchLength(LPCTSTR szToken, LPCTSTR szCommand)
{
	DWORD i = 0;
	for (; szToken[i] != _T('\0') && szCommand[i] != _T('\0'); ++i) {
		TCHAR x[2] = { szToken[i], 0};
		TCHAR y[2] = { szCommand[i], 0};
		if (*CharLower(x) != *CharLower(y))
			if (szToken[i] == _T('\0')) return i;
			else return 0;
	}
	if (szToken[i] != _T('\0')) {
		return 0;
	}
	return i;
}

void FindPartialMatchEntries(
	LPCTSTR szToken, DWORD dwLevel, 
	SIZE_T* pCand, SIZE_T* pcCand)
{
	DWORD maxMatchLen = 1, matchLen = 0;
	LPCTSTR szCommand = NULL;
	SIZE_T* pNewCand = pCand;
	SIZE_T* pCurNewCand = pCand;
	SIZE_T cNewCand = 0;
	SIZE_T i;

	for (i = 0; i < *pcCand; ++i) {
		szCommand = _cproc_table[pCand[i]].szCommands[dwLevel];
		matchLen = GetStringMatchLength(szToken, szCommand);
		if (matchLen > maxMatchLen) {
			maxMatchLen = matchLen;
			pCurNewCand = pNewCand;
			*pCurNewCand = pCand[i];
			++pCurNewCand;
			cNewCand = 1;
		} else if (matchLen == maxMatchLen) {
			*pCurNewCand = pCand[i];
			++pCurNewCand;
			++cNewCand;
		} 
	}

	*pcCand = cNewCand;

	return;
}

void PrintCmd(SIZE_T index)
{
	SIZE_T j = 0;
	_tprintf(_T("%s"), _cproc_table[index].szCommands[j]);
	for (j = 1; _cproc_table[index].szCommands[j]; ++j) {
		_tprintf(_T(" %s"), _cproc_table[index].szCommands[j]);
	}
}

void PrintOpt(SIZE_T index)
{
	LPCTSTR* ppsz = _cproc_table[index].szCommands;
	
	for (; NULL != *ppsz; ++ppsz) {
		__noop;
	}

	_tprintf(_T("%s"), *(++ppsz));

}

void PrintCand(const SIZE_T* pCand, SIZE_T cCand)
{
	SIZE_T i;

	for (i = 0; i < cCand; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(pCand[i]);
		_tprintf(_T("\n"));
	}
}

void usage()
{
	const SIZE_T nCommands = 
		sizeof(_cproc_table) / sizeof(_cproc_table[0]);
	SIZE_T i;

	_tprintf(
_T("ndascmd: command line client for NDAS device management\n")
_T("\n")
_T(" usage: ndascmd <command> [options]\n")
_T("\n"));

	for (i = 0; i < nCommands; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(i);
		_tprintf(_T(" "));
		PrintOpt(i);
		_tprintf(_T("\n"));
	}
}

#define MAX_CANDIDATES RTL_NUMBER_OF(_cproc_table)

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	SIZE_T candIndices[MAX_CANDIDATES] = {0};
	SIZE_T cCandIndices = MAX_CANDIDATES;
	DWORD dwLevel = 0;
	SIZE_T i;

	if (argc < 2) {
		usage();
		return -1;
	}

	for (i = 0; i < cCandIndices; ++i) {
		candIndices[i] = i;
	}

	for (dwLevel = 0; dwLevel + 1 < (DWORD) argc; ++dwLevel) {

		FindPartialMatchEntries(
			argv[1 + dwLevel], 
			dwLevel, 
			candIndices,
			&cCandIndices);
	
		if (cCandIndices == 1) {

#if _DEBUG
			_tprintf(_T("Running: "));
			PrintCand(candIndices, cCandIndices);
#endif

			return _cproc_table[candIndices[0]].
				proc(argc - dwLevel - 2, &argv[dwLevel + 2]);

		} else if (cCandIndices == 0) {

			_tprintf(_T("Error: Unknown command.\n\n"));
			usage();
			break;

		} else if (cCandIndices > 1) {

			BOOL fStop = FALSE;
			SIZE_T i;

			// if the current command parts are same, proceed,
			// otherwise emit error
			for ( i = 1; i < cCandIndices; ++i) {
				if (0 != lstrcmpi(
					_cproc_table[candIndices[0]].szCommands[dwLevel],
					_cproc_table[candIndices[i]].szCommands[dwLevel]))
				{
					_tprintf(_T("Error: Ambiguous command:\n\n"));
					PrintCand(candIndices, cCandIndices);
					_tprintf(_T("\n"));
					fStop = TRUE;
					break;
				}
			}

			if (fStop) {
				break;
			}
		}

		// more search
		
		if (dwLevel + 2 >= (DWORD) argc) {
			_tprintf(_T("Error: Incomplete command:\n\n"));
			PrintCand(candIndices, cCandIndices);
			_tprintf(_T("\n"));
		}
	}

	return -1;
}
