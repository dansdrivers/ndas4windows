#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>
#include <strsafe.h>
#include "lfsfiltctl.h"

#define XDBG_MAIN_MODULE
#include "xdebug.h"

void usage()
{
	_tprintf(TEXT("lfscmd [version | listenevent | ndasusage slotno | stopsecvol physicaldriveno]\n"));
}

VOID
PrintOutEvent(
	PXEVENT_ITEM EventItem
){
	_tprintf(	TEXT("EventClass          =%u\n")
				TEXT("EventLength         =%u\n")
				TEXT("PhysicalDriveNumber =%u\n")
				TEXT("VParamLength        =%u\n\n"),
				EventItem->EventClass,
				EventItem->EventLength,
				EventItem->DiskVolumeNumber,
				EventItem->VParamLength);

	switch(EventItem->EventClass) {
		case XEVTCLS_PRIMARY_VOLUME_INVALID_OR_LOCKED:
			_tprintf(TEXT("Phsical drive %u of Primary host of Slot %u Unit %u is locked.\n"),
				EventItem->DiskVolumeNumber,
				EventItem->VolumeLocked.SlotNumber,
				EventItem->VolumeLocked.UnitNumber
				);
			break;
		case XEVTCLS_PRIMARY_DISCONNECTED_ABNORMALLY:
			//
			//	NOTE: SuspectedFileNameList is in multi-string format.
			//			print the first file name in this example.
			//
			_tprintf(TEXT("Phsical drive %u is disconnected abnormally from the primary\n"),
				EventItem->DiskVolumeNumber,
				EventItem->AbnormalDiscon.SuspectedFileNameList
				);
			break;
		case XEVTCLS_LFSFILT_SHUTDOWN:
			_tprintf(TEXT("LFSFILT shut down\n"));
			break;
		default:
		_tprintf(TEXT("Unknown event class\n"));
	}
}


BOOL
DequeueEvent(LFS_EVTQUEUE_HANDLE EvtqHandle){
	BOOL	fSuccess;
	UINT32	EventLength;
	UINT32	EventClass;
	PXEVENT_ITEM eventItem;

	while(TRUE) {

		//
		//	Retrieve an event header
		//

		fSuccess = LfsFiltGetEventHeader(
						EvtqHandle,
						&EventLength,
						&EventClass
						);
		if(!fSuccess) {
			if(GetLastError() == ERROR_NO_MORE_ITEMS) {
				fSuccess = TRUE;
				break;
			}
			break;
		}


		//
		//	Allocate memory for the event
		//	

		eventItem = (PXEVENT_ITEM)HeapAlloc(GetProcessHeap(), 0, EventLength);
		if(eventItem == NULL) {
			_tprintf(TEXT("Heap allocation failed.\n"));
			fSuccess = FALSE;
			break;
		}


		//
		//	Retrieve the event
		//

		fSuccess = LfsFiltGetEvent(
				EvtqHandle,
				EventLength,
				eventItem
			);
		if(!fSuccess) {
			HeapFree(GetProcessHeap(), 0, eventItem);

			if(GetLastError() == ERROR_NO_MORE_ITEMS) {
				fSuccess = TRUE;
				break;
			}
			break;
		}


		//
		//	Print out
		//

		PrintOutEvent(eventItem);


		//
		//	Free the memory for the event
		//

		HeapFree(GetProcessHeap(), 0, eventItem);
	}

	return fSuccess;
}

BOOL
ListenEvent(
){
	BOOL	fSuccess;
	DWORD	result;
	BOOL	exitLoop;
	LFS_EVTQUEUE_HANDLE	evtqHandle;
	HANDLE				eventWait;

	_tprintf(TEXT("Commands: 'q' to quit\n           others continue\n"));

	//
	//	Create an event queue in LfsFilt
	//

	fSuccess = LfsFiltCtlCreateEventQueue(
							&evtqHandle,
							&eventWait);
	if(!fSuccess)
		return FALSE;

	_tprintf(TEXT("EventQueue=%p EventWait:%p\n"),evtqHandle,eventWait);

	exitLoop = TRUE;
	while(exitLoop) {

		result = WaitForSingleObject(eventWait, 1000);
		if(result == WAIT_OBJECT_0) {
			_tprintf(TEXT("Event arrived\n"));

			ResetEvent(eventWait);

			DequeueEvent(evtqHandle);

		} else if(result == WAIT_TIMEOUT) {
			INT	input;

			_tprintf(TEXT("Enter command: \n"));
			input = _gettchar();
			if(input == 'q' || input == 'Q') {
				_tprintf(TEXT("Quit\n"));
				break;
			}
		} else {
			_tprintf(TEXT("Waiting for events failed %x\n"), result);
			fSuccess = FALSE;
			break;
		}
	}


	//
	//	Close the event queue
	//

	LfsFiltCtlCloseEventQueue(evtqHandle);

	return fSuccess;
}

int __cdecl _tmain(int argc, LPTSTR* argv)
{
	BOOL	fSuccess(FALSE);
	BOOL	bret;

	if(argc < 2) {
		usage();
		return 1;
	}

	if (lstrcmpi(argv[1],TEXT("version")) == 0) {
		WORD VersionMajor;
		WORD VersionMinor;
		WORD VersionBuild;
		WORD VersionPrivate;
		WORD NdfsMajor;
		WORD NdfsMinor;

		bret = LfsFiltCtlGetVersion(
					&VersionMajor,
					&VersionMinor,
					&VersionBuild,
					&VersionPrivate,
					&NdfsMajor,
					&NdfsMinor
				);
		if(bret == FALSE) {
			_tprintf(TEXT("LfsFilt control failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(TEXT("- LfsFilt version\n"));
			_tprintf(	TEXT("Major   : %u\n")
						TEXT("Minor   : %u\n")
						TEXT("Build   : %u\n")
						TEXT("Private : %u\n")
						TEXT("NDFS Maj: %u\n")
						TEXT("NDFS Min: %u\n"),
						VersionMajor, VersionMinor, VersionBuild, VersionPrivate,
						NdfsMajor, NdfsMinor);
		}

	} else if(lstrcmpi(argv[1],TEXT("listenevent")) == 0) {

		fSuccess = ListenEvent();
		if(!fSuccess) {
			_tprintf(TEXT("LfsFilt control failed. LastError:%lu\n"), GetLastError());
		}

	} else if(lstrcmpi(argv[1],TEXT("ndasusage")) == 0) {
		INT32	slotNo;
		LFSCTL_NDAS_USAGE	ndasUsage;

		if(argc < 3) {
			usage();
			return 1;
		}

		slotNo = _tstoi(argv[2]);

		_tprintf(TEXT("SlotNo:%u\n"), slotNo);

		fSuccess = LfsFiltQueryNdasUsage(slotNo, &ndasUsage);
		if(fSuccess) {
			if(ndasUsage.NoDiskVolume)
				_tprintf(TEXT("No disk volume.\n"));
			if(ndasUsage.Attached)
				_tprintf(TEXT("LFS filter is attatched to the specified disk volumes.\n"));
			if(ndasUsage.ActPrimary)
				_tprintf(TEXT("Acts a primary host.\n"));
			if(ndasUsage.ActSecondary)
				_tprintf(TEXT("Acts a secondary host.\n"));
			if(ndasUsage.ActReadOnly)
				_tprintf(TEXT("Acts a read-only host.\n"));
			if(ndasUsage.HasLockedVolume)
				_tprintf(TEXT("Has a locked volume.\n"));

			_tprintf(TEXT("Mounted file system volumes: %u\n"), ndasUsage.MountedFSVolumeCount);
		} else {
			_tprintf(TEXT("LfsFilt control failed. LastError:%lu\n"), GetLastError());
		}

	} else if(lstrcmpi(argv[1],TEXT("stopsecvol")) == 0) {
		UINT32	phyDrvNo;

		if(argc < 3) {
			usage();
			return 1;
		}

		phyDrvNo = _tstoi(argv[2]);
		fSuccess = LfsFiltStopSecondaryVolume(phyDrvNo);
		if(fSuccess) {
			_tprintf(TEXT("LfsFilt control failed. LastError:%lu\n"), GetLastError());
		}

	} else {
		usage();
	}


	return fSuccess ? 0 : 1;
}
