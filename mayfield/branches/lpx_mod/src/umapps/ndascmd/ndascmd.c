// ndascmd.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <crtdbg.h>

#include "ndas/ndasuser.h"
#include "ndas/ndasevent_str.h"
#include "ndas/ndastype_str.h"

#define STRIP_PREFIX(prefix, str) \
	((LPCTSTR)(((LPCTSTR)(str)) + ((sizeof(prefix) - 1) / sizeof(TCHAR))))

#define NDAS_MSG_DLL	_T("ndasmsg.dll")

__inline void PrintNdasErrorMessage(DWORD dwError)
{
	HMODULE hModule = NULL;
	BOOL fSuccess = FALSE;
	LPTSTR lpszErrorMessage;

	hModule = LoadLibraryEx(
		NDAS_MSG_DLL,
		NULL,
		LOAD_LIBRARY_AS_DATAFILE);

	if (NULL == hModule) {
		_tprintf(_T("NDAS Error: %u (0x%08X)\n"), dwError, dwError);
		return;
	}

	fSuccess = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
		hModule,
		dwError,
		0,
		(LPTSTR) &lpszErrorMessage,
		0,
		NULL);

	if (fSuccess) {
		_tprintf(_T("NDAS Error: %u (0x%08X) : %s\n"),
			dwError,
			dwError,
			lpszErrorMessage);
		LocalFree(lpszErrorMessage);
	} else {
		_tprintf(_T("NDAS Error: %u (0x%08X)\n"), dwError, dwError);
	}

	FreeLibrary(hModule);

	return;
}

__inline void PrintErrorMessage(DWORD dwError)
{
	if (dwError & APPLICATION_ERROR_MASK) {
		PrintNdasErrorMessage(dwError);
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

static LPCTSTR
pConvertToDeviceIdString(LPCTSTR szDeviceID)
{
	__declspec(thread) static TCHAR DEVICE_ID_BUFFER[25] = {0};
	HRESULT hr;

	hr = StringCchPrintf(
		DEVICE_ID_BUFFER,
		RTL_NUMBER_OF(DEVICE_ID_BUFFER),
		_T("%C%C%C%C%C-%C%C%C%C%C-%C%C%C%C%C-*****"),
		szDeviceID[0], szDeviceID[1], szDeviceID[2],
		szDeviceID[3], szDeviceID[4], szDeviceID[5],
		szDeviceID[6], szDeviceID[7], szDeviceID[8],
		szDeviceID[9], szDeviceID[10], szDeviceID[11],
		szDeviceID[12], szDeviceID[13], szDeviceID[14]);

	return SUCCEEDED(hr) ? DEVICE_ID_BUFFER : _T("");
}

int CpNull(int argc, _TCHAR* argv[])
{
	return 0;
}

//
// name, id, <key>
//
int CpRegisterDevice(int argc, _TCHAR* argv[])
{
	LPTSTR szName = NULL;
	LPTSTR szID = NULL;
	LPTSTR szKey = NULL;
	DWORD dwSlotNo = 0;

	if (argc == 2) {
	} else if (argc == 3) {
		szKey = argv[2];
	} else {
		_tprintf(_T("usage: register <device name> <device id> [device key]\n"));
		return -1;
	}

	szName = argv[0];
	szID = argv[1];

	dwSlotNo = NdasRegisterDevice(szID, szKey, szName);
	if (0 == dwSlotNo) {
		PrintLastErrorMessage();
		return -1;
	}

	NdasEnableDevice(dwSlotNo, TRUE);

	_tprintf(_T("NDAS device is registered successfully.\n"));
	return 0;
}

//
// slot no
//
int CpUnregisterDevice(int argc, _TCHAR* argv[])
{
	DWORD dwSlotNo;
	BOOL fSuccess;

	if (argc != 1) {
		_tprintf(_T("usage: unregister <device number>\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasUnregisterDevice(dwSlotNo);

	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	_tprintf(_T("NDAS device is unregistered successfully.\n"));
	return 0;
}

static
LPCTSTR
pNdasUnitDeviceTypeString(NDAS_UNITDEVICE_TYPE type)
{
	switch (type) {
	case NDAS_UNITDEVICE_TYPE_DISK: return _T("Disk Drive");
	case NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK: return _T("CF Card Reader");
	case NDAS_UNITDEVICE_TYPE_CDROM: return _T("CD/DVD Drive");
	case NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY: return _T("MO Drive");
	default: return _T("Unknown Type");
	}
}

BOOL CALLBACK EnumUnitDeviceProc(
	NDASUSER_UNITDEVICE_ENUM_ENTRY* pEntry,
	LPVOID lpContext)
{
	BOOL fSuccess = FALSE;
	NDASUSER_UNITDEVICE_INFORMATION unitDevInfo = {0};
	DWORD dwSlotNo = *((DWORD*)lpContext);
	NDAS_LOGICALDEVICE_ID ldid = 0;

	fSuccess = NdasQueryUnitDeviceInformation(
		dwSlotNo,
		pEntry->UnitNo,
		&unitDevInfo);

	if (!fSuccess) {
		return TRUE;
	}

	unitDevInfo.UnitDeviceSubType;
	unitDevInfo.HardwareInfo.SectorCountLowPart;
	unitDevInfo.HardwareInfo.SectorCountHighPart;
	unitDevInfo.HardwareInfo.MediaType;

	_tprintf(
		_T("    Unit Device %d: %s"),
		pEntry->UnitNo,
		pNdasUnitDeviceTypeString(unitDevInfo.UnitDeviceType));

	fSuccess = NdasFindLogicalDeviceOfUnitDevice(
		dwSlotNo,
		pEntry->UnitNo,
		&ldid);

	if (fSuccess) {
		_tprintf(_T(" (Logical Device Number: %d)"), ldid);
	}

	_tprintf(_T("\n"));

	return TRUE;
}

BOOL CALLBACK EnumDeviceProc(
	NDASUSER_DEVICE_ENUM_ENTRY* pEntry,
	LPVOID lpContext)
{
	LPDWORD lpCount = (LPDWORD)(lpContext);
	++(*lpCount);

	_tprintf(
		_T("%02d: Name     : %s\n"),
		pEntry->SlotNo,
		pEntry->szDeviceName);

	_tprintf(
		_T("    Device ID: %s (%s)\n"),
		pConvertToDeviceIdString(pEntry->szDeviceStringId),
		(pEntry->GrantedAccess& GENERIC_WRITE) ? _T("RW") : _T("RO"));

	NdasEnumUnitDevices(
		pEntry->SlotNo,
		EnumUnitDeviceProc,
		&pEntry->SlotNo);

	_tprintf(_T("\n"));

	return TRUE;
}

int CpEnumDevices(int argc, _TCHAR* argv[])
{
	DWORD nCount = 0;
	BOOL fSuccess;

	if (0 != argc) {
		_tprintf(_T("usage: enumerate logicaldevices\n"));
		return -1;
	}

	fSuccess = NdasEnumDevicesW(EnumDeviceProc, &nCount);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}
	return 0;
}

int CpPlugIn(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	BOOL bWritable = FALSE;
	NDAS_LOGICALDEVICE_ID ldid = 0;

	if (argc != 1 && argc != 2) {
		_tprintf(_T("usage: mount <logical device number> [rw|ro]\n"));
		return -1;
	}

	if (argc == 2) {
		if (lstrcmpi(argv[1], _T("rw")) == 0) {
			bWritable = TRUE;
		}
	}

	ldid = _ttoi(argv[0]);

	_tprintf(_T("Mounting a logical device (%d) with %s mode...\n"),
		ldid, bWritable ? _T("RW") : _T("RO"));

	fSuccess = NdasPlugInLogicalDevice(bWritable, ldid);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpEject(int argc, _TCHAR* argv[])
{
	BOOL fSuccess;
	NDAS_LOGICALDEVICE_ID ldid = 0;

	if (argc != 1) {
		_tprintf(_T("usage: unmount <logical device number>\n"));
		return -1;
	}

	ldid = _ttoi(argv[0]);

	_tprintf(_T("Unmounting a logical device (%d)...\n"), ldid);
	fSuccess = NdasEjectLogicalDevice(ldid);
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
	NDAS_LOGICALDEVICE_ID ldid = 0;

	if (argc != 1) {
		_tprintf(_T("usage: unplug <logical device number>\n"));
		return -1;
	}

	ldid = _ttoi(argv[0]);

	_tprintf(_T("Unplugging a logical device (%d)...\n"), ldid);
	fSuccess = NdasUnplugLogicalDevice(ldid);
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
		_tprintf(_T("usage: activate <device number>\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasEnableDevice(dwSlotNo, TRUE);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	} else {
		_tprintf(_T("Device %d is activated successfully.\n"), dwSlotNo);
	}

	return 0;
}

int CpDisable(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;

	if (argc != 1) {
		_tprintf(_T("usage: deactivate <device number>\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasEnableDevice(dwSlotNo, FALSE);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	} else {
		_tprintf(_T("Device %d is deactivated successfully.\n"), dwSlotNo);
	}

	return 0;
}

static LPCTSTR
pGetSizeString(DWORD dwBlocks)
{
	__declspec(thread) static TCHAR szBuffer[30];
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

BOOL CALLBACK
EnumLogicalDevices(
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

	_tprintf(_T("%02d: %s"),
			pEntry->LogicalDeviceId,
			STRIP_PREFIX(
				_T("NDAS_LOGICALDEVICE_TYPE_"),
				NdasLogicalDeviceTypeString(pEntry->Type)));

	fSuccess = NdasQueryLogicalDeviceInformation(
		pEntry->LogicalDeviceId, &info);

	if (!fSuccess) {
		_tprintf(_T(" (Information Unavailable)\n"));
		PrintLastErrorMessage();
		// return TRUE;
	} else {
		if (IS_NDAS_LOGICALDEVICE_TYPE_DISK_GROUP(info.LogicalDeviceType)) {
			_tprintf(_T(" (%s)"), pGetSizeString(info.LogicalDiskInformation.Blocks));
		}

		_tprintf(_T(" (%s)"), // Granted Access
			(info.GrantedAccess & GENERIC_WRITE) ? _T("RW") : _T("RO"));
	}


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
		_tprintf(_T("usage: enumerate logicaldevices\n"));
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
		_tprintf(_T("usage: query device information <device number>\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasQueryDeviceInformation(dwSlotNo, &di);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	_tprintf(_T("Slot No   : %d\n"), di.SlotNo);
	_tprintf(_T("Name      : %s\n"), di.szDeviceName);
	_tprintf(_T("Device ID : %s\n"), pConvertToDeviceIdString(di.szDeviceId));
	_tprintf(_T("Writable  : %s\n"), (di.GrantedAccess & GENERIC_WRITE) ? _T("Yes") : _T("No"));

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
		_tprintf(_T("usage: query device status <device number>\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	fSuccess = NdasQueryDeviceStatus(dwSlotNo, &status, &lastError);
	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	_tprintf(_T("Status    : 0x%08X (%s)\n"), status, NdasDeviceStatusString(status));
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

static LPCTSTR pGuidString(CONST GUID* lpGuid)
{
	static TCHAR szBuffer[256];
	HRESULT hr = StringCchPrintf(
		szBuffer,
		256,
		_T("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"),
		lpGuid->Data1,
		lpGuid->Data2,
		lpGuid->Data3,
		lpGuid->Data4[0], lpGuid->Data4[1],
		lpGuid->Data4[2], lpGuid->Data4[3],
		lpGuid->Data4[4], lpGuid->Data4[5],
		lpGuid->Data4[6], lpGuid->Data4[7]);
	return szBuffer;
}
BOOL CALLBACK QueryHostEnumProc(
	CONST GUID* lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext)
{
	BOOL fSuccess;
	NDAS_HOST_INFO hostInfo;

	_tprintf(_T("%s - %s: "),
		pGuidString(lpHostGuid),
		(Access & GENERIC_WRITE) ? _T("RW") : _T("RO"));

	fSuccess = NdasQueryHostInfo(lpHostGuid, &hostInfo);
	if (!fSuccess) {
		_tprintf(_T("no info"));
		PrintLastErrorMessage();
	} else {
		_tprintf(_T("%s"), hostInfo.szHostname);
	}

	_tprintf(_T("\n"));

	return TRUE;
}

int CpQueryUnitDeviceHost(int argc, _TCHAR* argv[])
{
	BOOL fSuccess;
	DWORD dwSlotNo;
	DWORD dwUnitNo;

	if (1 != argc && 2 != argc) {
		_tprintf(_T("usage: query unitdevice hosts <device number> [unit number]\n"));
		return -1;
	}

	dwSlotNo = _ttoi(argv[0]);
	if (2 == argc) {
		dwUnitNo = _ttoi(argv[1]);
	} else {
		dwUnitNo = 0;
	}

	fSuccess = NdasQueryHostsForUnitDevice(
		dwSlotNo,
		dwUnitNo,
		QueryHostEnumProc,
		NULL);

	if (!fSuccess) {
		PrintLastErrorMessage();
		return -1;
	}

	return 0;
}

int CpQueryLogicalDeviceInfo(int argc, _TCHAR* argv[])
{
	return 0;
}


int CpQueryLogicalDeviceStatus(int argc, _TCHAR* argv[])
{
	BOOL fSuccess = FALSE;

	NDAS_LOGICALDEVICE_ID logDevId = 0;
	NDAS_LOGICALDEVICE_STATUS status = 0;
	NDAS_LOGICALDEVICE_ERROR lastError = 0;

	if (argc != 1) {
		_tprintf(_T("usage: query logicaldevice status <logical device number>\n"));
		return -1;
	}

	logDevId = _ttoi(argv[0]);

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

	if (ERROR_SUCCESS != dwError && ERROR_IO_PENDING != dwError) {
		PrintLastErrorMessage();
		SetEvent(hErrorEvent);
		return;
	}

	switch (pEventInfo->EventType) {
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS Device (%d) status changed: %s -> %s.\n"),
			pEventInfo->DeviceInfo.SlotNo,
			NdasDeviceStatusString(pEventInfo->DeviceInfo.OldStatus),
			NdasDeviceStatusString(pEventInfo->DeviceInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d) status changed: %s -> %s.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId,
			NdasLogicalDeviceStatusString(pEventInfo->LogicalDeviceInfo.OldStatus),
			NdasLogicalDeviceStatusString(pEventInfo->LogicalDeviceInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
		_tprintf(_T("NDAS Logical Device (%d) is disconnected.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING:
		_tprintf(_T("NDAS Logical Device (%d) is being reconnected.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTED:
		_tprintf(_T("NDAS Logical Device (%d) is reconnected.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_EMERGENCY:
		_tprintf(_T("NDAS Logical Device (%d) is running under emergency mode.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d) Relation Changed.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d) Property Changed.\n"),
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
		_tprintf(_T("NDAS Device Entry Changed.\n"));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		_tprintf(_T("NDAS Logical Device Entry Changed.\n"));
		break;
	case NDAS_EVENT_TYPE_TERMINATING:
		_tprintf(_T("Termination.\n"));
		SetEvent(hErrorEvent);
		break;
	case NDAS_EVENT_TYPE_CONNECTED:
		_tprintf(_T("Connected.\n"));
		break;
	case NDAS_EVENT_TYPE_RETRYING_CONNECTION:
		_tprintf(_T("Reconnecting.\n"));
		break;
	case NDAS_EVENT_TYPE_CONNECTION_FAILED:
		_tprintf(_T("Connection Failure.\n"));
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

DEF_COMMAND_1(_c_reg, "register", "<name> <device id without dash> [write key]")
DEF_COMMAND_1(_c_unreg, "unregister", "<device number>")
DEF_COMMAND_2(_c_enum_dev, "list", "devices", "")
// DEF_COMMAND_2(_c_enum_unitdev, "list", "unitdevices", "")
DEF_COMMAND_2(_c_enum_logdev, "list", "logicaldevices", "")
DEF_COMMAND_1(_c_plugin, "mount", "<logical device number> [ro|rw]")
DEF_COMMAND_1(_c_unplug, "unplug", "<logical device number>")
DEF_COMMAND_1(_c_eject, "unmount", "<logical device number>")
DEF_COMMAND_1(_c_enable, "activate", "<device number>")
DEF_COMMAND_1(_c_disable, "deactivate", "<device number>")
DEF_COMMAND_3(_c_query_dev_st, "query","device","status", "<device number>")
DEF_COMMAND_3(_c_query_dev_info, "query","device","information", "<device number>")
DEF_COMMAND_3(_c_query_unitdev_st, "query", "unitdevice", "status", "<device number> <un>")
DEF_COMMAND_3(_c_query_unitdev_info, "query", "unitdevice", "information", "<device number> <un>")
DEF_COMMAND_3(_c_query_unitdev_host, "query", "unitdevice", "hosts", "<device number> <un>")
DEF_COMMAND_3(_c_query_logdev_st, "query", "logicaldevice", "status", "<logical device number>")
DEF_COMMAND_3(_c_query_logdev_info, "query", "logicaldevice", "information", "<logical device number>")
DEF_COMMAND_2(_c_show_events, "track", "events", "")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_reg, CpRegisterDevice, 2, 3},
	{ _c_unreg, CpUnregisterDevice, 1, 1},
	{ _c_enum_dev, CpEnumDevices, 0, 0},
//	{ _c_enum_unitdev, CpEnumUnitDevices, 1, 1},
	{ _c_enum_logdev, CpEnumLogicalDevices, 0, 0},
	{ _c_plugin, CpPlugIn, 1, 1},
	{ _c_unplug, CpUnplug, 1, 1},
	{ _c_eject, CpEject, 1, 1},
	{ _c_enable, CpEnable, 1, 1},
	{ _c_disable, CpDisable, 1, 1},
//	{ _c_query_dev_st, CpQueryDeviceStatus, 1, 1},
//	{ _c_query_dev_info, CpQueryDeviceInfo, 1, 1},
//	{ _c_query_unitdev_st, CpQueryUnitDeviceStatus, 1, 1},
//	{ _c_query_unitdev_info, CpQueryUnitDeviceInfo, 1, 1},
//	{ _c_query_unitdev_host, CpQueryUnitDeviceHost, 1, 1},
//	{ _c_query_logdev_st, CpQueryLogicalDeviceStatus, 1, 1},
//	{ _c_query_logdev_info, CpQueryLogicalDeviceInfo, 1, 1},
#ifdef _DEBUG
	{ _c_show_events, CpShowEvents, 0, 1},
#endif
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
_T(" usage: ndascmd <command> [parameters]\n")
_T("\n"));

	for (i = 0; i < nCommands; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(i);
		_tprintf(_T(" "));
		PrintOpt(i);
		_tprintf(_T("\n"));
	}
}

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define MAX_CANDIDATES RTL_NUMBER_OF(_cproc_table)

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	SIZE_T candIndices[MAX_CANDIDATES] = {0};
	SIZE_T cCandIndices = MAX_CANDIDATES;
	DWORD dwLevel = 0;
	SIZE_T i;

	_tprintf(
		_T("NDAS Device Management Command Version 0.8\n")
		_T("Copyright (C) 2003-2004 XIMETA, Inc.\n\n"));

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

			int j;
			int cpArgc = argc - dwLevel - 2;
			_TCHAR** cpArgv = &argv[dwLevel+2];

#if _DEBUG
			_tprintf(_T("Running: "));
			PrintCand(candIndices, cCandIndices);
			_tprintf(_T("argc: %d, argv: "), cpArgc);
			for (j = 0; j < cpArgc; ++j) {
				_tprintf(_T("{%d:%s} "), j, cpArgv[j]);
			}
			_tprintf(_T("\n"));
#endif

			return _cproc_table[candIndices[0]].
				proc(cpArgc, cpArgv);

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
