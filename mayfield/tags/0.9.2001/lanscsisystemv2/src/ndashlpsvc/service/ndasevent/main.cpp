#include "stdafx.h"
#include "ndasevent.h"

void DumpMessage(const PNDAS_EVENT_MESSAGE pMessage)
{
	switch (pMessage->EventType) {
	case NDAS_EVENT_TYPE_VERSION_INFO:
		_tprintf(_T("NDAS_EVENT_TYPE_VERSION_EVENT:"));
		_tprintf(_T("%d.%d\n"), 
			pMessage->VersionInfo.MajorVersion, 
			pMessage->VersionInfo.MinorVersion);
		break;
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED: "));
		_tprintf(_T("(%d, %s): %s -> %s.\n"), 
			pMessage->DeviceEventInfo.SlotNo,
			CNdasDeviceId(pMessage->DeviceEventInfo.DeviceId).ToString(),
			NdasDeviceStatusString(pMessage->DeviceEventInfo.OldStatus),
			NdasDeviceStatusString(pMessage->DeviceEventInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
		_tprintf(_T("NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED: "));
		_tprintf(_T("(%d,%d,%d): %s -> %s.\n"), 
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.SlotNo,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.TargetId,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.LUN,
			NdasLogicalDeviceStatusString(pMessage->LogicalDeviceEventInfo.OldStatus),
			NdasLogicalDeviceStatusString(pMessage->LogicalDeviceEventInfo.NewStatus));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
		_tprintf(_T("NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED: "));
		_tprintf(_T("(%d,%d,%d)\n"), 
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.SlotNo,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.TargetId,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.LUN);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING:
		_tprintf(_T("NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING: "));
		_tprintf(_T("(%d,%d,%d)\n"), 
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.SlotNo,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.TargetId,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.LUN);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ALIVE:
		_tprintf(_T("NDAS_EVENT_TYPE_LOGICALDEVICE_ALIVE: "));
		_tprintf(_T("(%d,%d,%d)\n"), 
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.SlotNo,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.TargetId,
			pMessage->LogicalDeviceEventInfo.LogicalDeviceId.LUN);
		break;
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
		_tprintf(_T("NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGE\n"));
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		_tprintf(_T("NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED\n"));
		break;
	case NDAS_EVENT_TYPE_TERMINATING:
		_tprintf(_T("NDAS_EVENT_TYPE_TERMINATION\n"));
		break;
	default:
		_tprintf(_T("Unknown event type: %04X.\n"), pMessage->EventType);
	}
}

int __cdecl wmain(int argc, LPWSTR* argv)
{
	HANDLE hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	_ASSERTE(NULL != hEvent);

	OVERLAPPED overlapped = {0};
	overlapped.hEvent = hEvent;

	HANDLE hPipe = ::CreateFile(
		NDAS_EVENT_PIPE_NAME,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	_tprintf(TEXT("hPipe: %p\n"), hPipe);

	if (INVALID_HANDLE_VALUE == hPipe) {
		_tprintf(TEXT("CreateFile failed with error %d.\n"), ::GetLastError());
		return -1;
	}
	
	DWORD cbRead(0);
	NDAS_EVENT_MESSAGE message = {0};
	DWORD cbMessage = sizeof(NDAS_EVENT_MESSAGE);
	DWORD cMessage(0);

	for (DWORD i = 0; i < 1000; ++i) {

		BOOL fSuccess = ::ReadFile(
			hPipe, 
			&message, 
			cbMessage,
			&cbRead, 
			&overlapped);

		if (!fSuccess && ERROR_IO_PENDING != ::GetLastError()) {
			_tprintf(TEXT("ReadFile failed with error %d\n"), ::GetLastError());
			break;
		}

		if (fSuccess) {
			::SetEvent(overlapped.hEvent);
		}

		BOOL bPending(TRUE);

		do {
			DWORD dwTimeout = 3000;
			DWORD dwResult = ::WaitForSingleObject(hEvent, dwTimeout);
			if (dwResult == WAIT_OBJECT_0) {
				fSuccess = ::GetOverlappedResult(hPipe, &overlapped, &cbRead, TRUE);
				_ASSERTE(fSuccess && "GetOverlappedResult");
				DumpMessage(&message);
				bPending = FALSE;
			} else if (dwResult == WAIT_TIMEOUT) {
				_tprintf(TEXT("No message in %d seconds.\n"), dwTimeout / 1000);
			} else {
				_tprintf(TEXT("Wait failed with error %d.\n"), ::GetLastError());
				bPending = FALSE;
			}
		} while (bPending);

	}

	::CloseHandle(hEvent);
	::CloseHandle(hPipe);

	return 0;
}