#ifndef __DRAID_EXPORT_H__
#define __DRAID_EXPORT_H__

// 
// Exports to ndasscsi.c 
//

typedef struct _DRAID_GLOBALS {
	//
	// DRAID listening thread.
	//
	HANDLE			DraidThreadHandle;
	PVOID			DraidThreadObject;
	KEVENT			DraidExitEvent;

	LIST_ENTRY		ArbiterList;
	KSPIN_LOCK		ArbiterListSpinlock;

	LIST_ENTRY		ClientList;
	KSPIN_LOCK		ClientListSpinlock;

	KEVENT			NetChangedEvent;

	LIST_ENTRY		ListenContextList;	// List of DRAID_LISTEN_CONTEXT
	KSPIN_LOCK		ListenContextSpinlock;

	LONG			ReceptionThreadCount;	// Required to check all reception thread is finished.
	
} DRAID_GLOBALS, *PDRAID_GLOBALS;


NTSTATUS
DraidStart(
	PDRAID_GLOBALS DraidGlobals
);

NTSTATUS 
DraidStop(
	PDRAID_GLOBALS DraidGlobals
);

VOID
DraidPnpAddAddressHandler ( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
);

VOID
DraidPnpDelAddressHandler( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING DeviceName,
	IN PTDI_PNP_CONTEXT Context
);

NTSTATUS
DraidTdiClientPnPPowerChange(
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
);

#endif /* __DRAID_EXPORT_H__ */
