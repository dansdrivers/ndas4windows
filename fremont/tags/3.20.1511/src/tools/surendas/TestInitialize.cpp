#include	"StdAfx.h"

extern HANDLE	hServerStopEvent;
extern HANDLE	hUpdateEvent;

BOOL TestInitialize(void)
{
	WSADATA			wsadata;
	int				iErrorCode;

	DebugPrint(2, (TEXT("[NETDISKTEST]TestInitialize: Start Initialization.\n")));

	// Create events
	hServerStopEvent = CreateEvent(
		NULL,		// no security attibutes
		TRUE,		// manual reset event
		FALSE,		// not-signalled
		NULL		// INIT_EVENT_NAME
		);
	if(hServerStopEvent == NULL) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: Cannot create event.\n")));
		return FALSE;
	}
	
	hUpdateEvent = CreateEvent(
		NULL,		// no security attibutes
		TRUE,		// manual reset event
		FALSE,		// not-signalled
		NULL		// INIT_EVENT_NAME
		);
	if(hUpdateEvent == NULL) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: Cannot create event.\n")));
		return FALSE;
	}

	// Init socket
	iErrorCode = WSAStartup(MAKEWORD(2, 0), &wsadata);
	if(iErrorCode != 0) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: WSAStartup Failed %d\n"), iErrorCode));
		return FALSE;
	}

	return TRUE;
}