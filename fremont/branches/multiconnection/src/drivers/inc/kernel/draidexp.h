#ifndef __DRAID_EXPORT_H__
#define __DRAID_EXPORT_H__


// 
// Exports to ndasscsi.c 
//


#define DRAID_GLOBALS_FLAG_INITIALIZE	0x00000001

typedef struct _DRAID_GLOBALS {

	// DRAID listening thread.

	ULONG			Flags;
	ULONG			ReferenceCount;

	KEVENT			DraidThreadReadyEvent;

	HANDLE			DraidThreadHandle;
	PVOID			DraidThreadObject;
	KEVENT			DraidExitEvent;

	LIST_ENTRY		IdeDiskQueue;
	KSPIN_LOCK		IdeDiskQSpinlock;

	LIST_ENTRY		ArbiterQueue;
	KSPIN_LOCK		ArbiterQSpinlock;

	LIST_ENTRY		ClientQueue;
	KSPIN_LOCK		ClientQSpinlock;

	KEVENT			NetChangedEvent;

	LIST_ENTRY		ListenContextList;	// List of DRAID_LISTEN_CONTEXT
	KSPIN_LOCK		ListenContextSpinlock;

	LONG			ReceptionThreadCount;	// Required to check all reception thread is finished.
	
	HANDLE			TdiClientBindingHandle;

} DRAID_GLOBALS, *PDRAID_GLOBALS;

#if __NDAS_SCSI_OLD_VERSION__

NTSTATUS 
DraidStart (
	IN PDRAID_GLOBALS DraidGlobals
	); 

NTSTATUS 
DraidStop (
	PDRAID_GLOBALS DraidGlobals
	);

VOID
DraidPnpDelAddressHandler ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING	DeviceName,
	IN PTDI_PNP_CONTEXT Context
    );

VOID
DraidPnpAddAddressHandler ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
    );

NTSTATUS
DraidTdiClientPnPPowerChange (
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
	);

#else

__forceinline
NTSTATUS 
DraidStart (
	IN PDRAID_GLOBALS DraidGlobals
	) 
{
	UNREFERENCED_PARAMETER( DraidGlobals );

	// If you wanna use old version, set macro __NDAS_SCSI_OLD_VERSION__ and include draid.c ... in macro SOURCE

	DbgBreakPoint ();

	return STATUS_UNSUCCESSFUL;
} 

__forceinline
NTSTATUS 
DraidStop (
	PDRAID_GLOBALS DraidGlobals
	) 
{
	UNREFERENCED_PARAMETER( DraidGlobals );

	// If you wanna use old version, set macro __NDAS_SCSI_OLD_VERSION__ and include draid.c ... in macro SOURCE

	DbgBreakPoint ();

	return STATUS_UNSUCCESSFUL;
} 

__forceinline
VOID
DraidPnpDelAddressHandler ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING	DeviceName,
	IN PTDI_PNP_CONTEXT Context
	) 
{
	UNREFERENCED_PARAMETER( NetworkAddress );
	UNREFERENCED_PARAMETER( DeviceName );
	UNREFERENCED_PARAMETER( Context );

	// If you wanna use old version, set macro __NDAS_SCSI_OLD_VERSION__ and include draid.c ... in macro SOURCE

	DbgBreakPoint ();
} 

__forceinline
VOID
DraidPnpAddAddressHandler ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
	) 
{
	UNREFERENCED_PARAMETER( NetworkAddress );
	UNREFERENCED_PARAMETER( DeviceName );
	UNREFERENCED_PARAMETER( Context );

	// If you wanna use old version, set macro __NDAS_SCSI_OLD_VERSION__ and include draid.c ... in macro SOURCE

	DbgBreakPoint ();
} 

__forceinline
NTSTATUS
DraidTdiClientPnPPowerChange (
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
	) 
{
	UNREFERENCED_PARAMETER( DeviceName );
	UNREFERENCED_PARAMETER( PowerEvent );
	UNREFERENCED_PARAMETER( Context1 );
	UNREFERENCED_PARAMETER( Context2 );

	// If you wanna use old version, set macro __NDAS_SCSI_OLD_VERSION__ and include draid.c ... in macro SOURCE

	DbgBreakPoint ();

	return STATUS_SUCCESS;
} 

#endif

#endif /* __DRAID_EXPORT_H__ */
