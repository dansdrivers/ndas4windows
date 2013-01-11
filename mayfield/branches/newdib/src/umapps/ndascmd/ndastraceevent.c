#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <ndas/ndasuser.h>
#include <ndas/ndastype_str.h>
#include "ndascmd.h"

int pusage(LPCTSTR str);

void 
CALLBACK
ndas_event_callback(
	DWORD dwError,
	PNDAS_EVENT_INFO pEventInfo,
	LPVOID lpContext);

int ndascmd_trace_events(int argc, TCHAR** argv)
{
	static LPCTSTR USAGE = _T("");

	BOOL fSuccess;
	HANDLE hErrorEvent = NULL;
	HNDASEVENTCALLBACK hCallback = NULL;
	INPUT_RECORD inputRecord[128];
	HANDLE hStdIn = INVALID_HANDLE_VALUE;
	BOOL bTerminate = FALSE;

	UNREFERENCED_PARAMETER(argv);

	if (-1 == argc) return pusage(USAGE);

	hErrorEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == hErrorEvent) 
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	hCallback = NdasRegisterEventCallback(
		ndas_event_callback, 
		(LPVOID)hErrorEvent);

	if (NULL == hCallback)
	{
		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	hStdIn = GetStdHandle(STD_INPUT_HANDLE);

	if (INVALID_HANDLE_VALUE == hStdIn) 
	{
		fSuccess = NdasUnregisterEventCallback(hCallback);
		_ASSERTE(fSuccess);

		return NC_PrintLastErrMsg(), EXIT_FAILURE;
	}

	bTerminate = FALSE;
	while (!bTerminate) 
	{
		DWORD dwWaitResult = WaitForSingleObject(hErrorEvent, 500);

		if (WAIT_OBJECT_0 == dwWaitResult) 
		{
			_tprintf(_T("Event subscription error. Terminating.\n"));
			bTerminate = TRUE;
		} 
		else if (WAIT_TIMEOUT == dwWaitResult) 
		{
			DWORD dwEvents = 0;
			PeekConsoleInput(hStdIn, inputRecord, 128, &dwEvents);
			if (dwEvents > 0) 
			{
				DWORD i;
				ReadConsoleInput(hStdIn, inputRecord, 128, &dwEvents);
				for (i = 0; i < dwEvents; ++i) 
				{
#ifdef UNICODE
#define TCHAR_IN_UCHAR(x) x.UnicodeChar
#else
#define TCHAR_IN_UCHAR(x) x.AsciiChar
#endif
					if (_T('q') == TCHAR_IN_UCHAR(inputRecord[i].Event.KeyEvent.uChar)) 
					{
						bTerminate = TRUE;
					}
				}
			}
		}
		else 
		{
			NC_PrintLastErrMsg();
		}
	}

	fSuccess = NdasUnregisterEventCallback(hCallback);
	_ASSERTE(fSuccess);

	return EXIT_SUCCESS;
}

void CALLBACK
ndas_event_callback(
	DWORD dwError,
	PNDAS_EVENT_INFO pEventInfo,
	LPVOID lpContext)
{
	BOOL fSuccess;
	HANDLE hErrorEvent = (HANDLE)(lpContext);

	if (ERROR_SUCCESS != dwError && ERROR_IO_PENDING != dwError) 
	{
		NC_PrintErrMsg(dwError);
		fSuccess = SetEvent(hErrorEvent);
		_ASSERTE(fSuccess);
		return;
	}

	switch (pEventInfo->EventType) 
	{
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS Device (%d) status changed: %s -> %s.\n"),
			pEventInfo->EventInfo.DeviceInfo.SlotNo,
			NdasDeviceStatusString(pEventInfo->EventInfo.DeviceInfo.OldStatus),
			NdasDeviceStatusString(pEventInfo->EventInfo.DeviceInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d) status changed: %s -> %s.\n"),
			pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId,
			NdasLogicalDeviceStatusString(pEventInfo->EventInfo.LogicalDeviceInfo.OldStatus),
			NdasLogicalDeviceStatusString(pEventInfo->EventInfo.LogicalDeviceInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
		_tprintf(_T("NDAS Logical Device (%d) is disconnected.\n"),
			pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED:
		_tprintf(_T("NDAS Logical Device (%d) is alarmed(%08lx).\n"),
			pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId,
			pEventInfo->EventInfo.LogicalDeviceInfo.AdapterStatus);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d) Relation Changed.\n"),
			pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED:
		_tprintf(_T("NDAS Logical Device (%d) Property Changed.\n"),
			pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId);
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
