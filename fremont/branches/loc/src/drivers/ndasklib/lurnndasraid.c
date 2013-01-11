#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasdraid"

VOID
NdasRaidTdiBindingHandler (
	IN TDI_PNP_OPCODE	PnPOpcode,
	IN PUNICODE_STRING  DeviceName,
	IN PWSTR			MultiSZBindList
	);

VOID
NdasRaidTdiDelAddressHandlerV2 ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING	DeviceName,
	IN PTDI_PNP_CONTEXT Context
    );

VOID
NdasRaidTdiAddAddressHandlerV2 ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
    );

NTSTATUS
NdasRaidTdPnPPowerHandler (
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
	);

VOID
NdasRaidFlushAll (
	IN PNDASR_GLOBALS NdasrGlobals
	); 

VOID 
NdasRaidListenerThreadProc (
	PNDASR_GLOBALS NdasrGlobals
	); 

VOID
NdasRaidListnerAddAddressHandler (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PTDI_ADDRESS_LPX Addr
	);

PNDASR_LISTEN_CONTEXT 
NdasRaidCreateListenContext (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PLPX_ADDRESS		Addr
	); 

VOID
NdasRaidListnerDelAddress (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PLPX_ADDRESS		Addr
	);

NTSTATUS 
NdasRaidStartListenAddress (
	PNDASR_GLOBALS			NdasrGlobals,
	PNDASR_LISTEN_CONTEXT	ListenContext
	);

NTSTATUS 
NdasRaidStopListenAddress (
	PNDASR_GLOBALS			NdasrGlobals,
	PNDASR_LISTEN_CONTEXT	ListenContext
	); 

NTSTATUS 
NdasRaidListenConnection (
	PNDASR_LISTEN_CONTEXT ListenContext
	);

NTSTATUS
NdasRaidAcceptConnection (
	PNDASR_GLOBALS			NdasrGlobals,
	PNDASR_LISTEN_CONTEXT	ListenContext
	);

VOID
NdasRaidReceptionThreadProc (
	IN PVOID Param
	);

NTSTATUS
NdasRaidLurnSynchronousStopCcbCompletion (
	IN PCCB Ccb,
	IN PCCB ChildCcb
	);

VOID
NdasRaidLurnShutDownWorker (
	IN PDEVICE_OBJECT				DeviceObject,
	IN PLURN_NDASR_SHUT_DOWN_PARAM	Parameter
	); 

ULONG
NdasRaidLurnGetChildBlockBytes (
	PLURELATION_NODE Lurn
	);

NTSTATUS
NdasRaidLurnGetChildMinTransferLength (
	IN	PLURELATION_NODE	Lurn,
	OUT PUINT32				ChildMinDataSendLength,
	OUT PUINT32				ChildMinDataRecvLength
	);


struct _NDASR_GLOBALS NdasrGlobalData = { 0 };

NTSTATUS 
NdasRaidStart (
	PNDASR_GLOBALS NdasrGlobals
	) 
{
	NTSTATUS					status;
	OBJECT_ATTRIBUTES			objectAttributes;

	TDI_CLIENT_INTERFACE_INFO   tdiClientInterfaceInfo;
	UNICODE_STRING				svcName;


	if (FlagOn(NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE)) {

		InterlockedIncrement( &NdasrGlobals->ReferenceCount );
		return STATUS_SUCCESS;
	}

#if !DBG
	NdasTestBug = ( RtlCheckRegistryKey( RTL_REGISTRY_SERVICES, 
									     L"ndasscsi" ) == STATUS_SUCCESS );
#endif

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Starting\n") );

	KeInitializeEvent( &NdasrGlobals->StartEvent, NotificationEvent, FALSE );

	InitializeListHead( &NdasrGlobals->IdeDiskQueue );
	KeInitializeSpinLock( &NdasrGlobals->IdeDiskQSpinlock );

	InitializeListHead( &NdasrGlobals->ArbitratorQueue );
	KeInitializeSpinLock( &NdasrGlobals->ArbitratorQSpinlock );

	InitializeListHead( &NdasrGlobals->ClientQueue );
	KeInitializeSpinLock( &NdasrGlobals->ClientQSpinlock );

	KeInitializeEvent( &NdasrGlobals->DraidExitEvent, NotificationEvent, FALSE );	
	KeInitializeEvent( &NdasrGlobals->NetChangedEvent, NotificationEvent, FALSE );		

	InitializeListHead( &NdasrGlobals->ListenContextList );
	KeInitializeSpinLock( &NdasrGlobals->ListenContextSpinlock );

	SetFlag( NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	KeInitializeEvent( &NdasrGlobals->DraidThreadReadyEvent, NotificationEvent, FALSE );
	
	status = PsCreateSystemThread( &NdasrGlobals->DraidThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   NdasRaidListenerThreadProc,
								   NdasrGlobals );

	if (!NT_SUCCESS(status)) {
	
		NDAS_BUGON( FALSE );
	
		NdasrGlobals->DraidThreadHandle = NULL;
		NdasrGlobals->DraidThreadObject = NULL;

		TdiDeregisterPnPHandlers( NdasrGlobals->TdiClientBindingHandle );
		NdasrGlobals->TdiClientBindingHandle = NULL;

		ClearFlag( NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE );

		return STATUS_UNSUCCESSFUL;
	}

	status = ObReferenceObjectByHandle( NdasrGlobals->DraidThreadHandle,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&NdasrGlobals->DraidThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {
	
		NDAS_BUGON( FALSE );

		NdasrGlobals->DraidThreadObject = NULL;
		NdasrGlobals->DraidThreadHandle = NULL;

		NdasrGlobals->TdiClientBindingHandle = NULL;
		TdiDeregisterPnPHandlers( NdasrGlobals->TdiClientBindingHandle );

		ClearFlag( NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE );
		
		return STATUS_UNSUCCESSFUL;
	}
	
	status = KeWaitForSingleObject( &NdasrGlobals->DraidThreadReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									NULL );

	InterlockedIncrement(&NdasrGlobals->ReferenceCount);

    // Setup the TDI request structure

    RtlZeroMemory ( &tdiClientInterfaceInfo, sizeof (tdiClientInterfaceInfo) );

#ifdef TDI_CURRENT_VERSION
    tdiClientInterfaceInfo.TdiVersion = TDI_CURRENT_VERSION;
#else
    tdiClientInterfaceInfo.MajorTdiVersion = 2;
    tdiClientInterfaceInfo.MinorTdiVersion = 0;
#endif

	RtlInitUnicodeString( &svcName, L"NdasRaid" );

    tdiClientInterfaceInfo.Unused			   = 0;
    tdiClientInterfaceInfo.ClientName		   = &svcName;
    tdiClientInterfaceInfo.BindingHandler	   = NdasRaidTdiBindingHandler;
    tdiClientInterfaceInfo.AddAddressHandlerV2 = NdasRaidTdiAddAddressHandlerV2;
    tdiClientInterfaceInfo.DelAddressHandlerV2 = NdasRaidTdiDelAddressHandlerV2;
    tdiClientInterfaceInfo.PnPPowerHandler	   = NdasRaidTdPnPPowerHandler;

    // Register handlers with TDI

    status = TdiRegisterPnPHandlers( &tdiClientInterfaceInfo, 
									 sizeof(tdiClientInterfaceInfo), 
									 &NdasrGlobals->TdiClientBindingHandle );
    
	if (!NT_SUCCESS (status)) {
	
		NDAS_BUGON( FALSE );
		
		ClearFlag( NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE );
		return status;
    }

	NDAS_BUGON( NdasrGlobals->TdiClientBindingHandle );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("TDI PnP registered.\n") );

	return STATUS_SUCCESS;
}

NTSTATUS 
NdasRaidClose (
	PNDASR_GLOBALS NdasrGlobals
	) 
{
	NTSTATUS status;
	LARGE_INTEGER Timeout;
	LONG	refCnt;

	NDAS_BUGON( FlagOn(NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE) );

	refCnt = InterlockedDecrement(&NdasrGlobals->ReferenceCount);
	
	NDAS_BUGON( refCnt >= 0 );
	
	if (refCnt) {

		return STATUS_SUCCESS;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Stopping\n") );
	
	if (NdasrGlobals->TdiClientBindingHandle) {

		TdiDeregisterPnPHandlers( NdasrGlobals->TdiClientBindingHandle );
		NdasrGlobals->TdiClientBindingHandle = NULL;
	}

	if (NdasrGlobals->DraidThreadHandle && NdasrGlobals->DraidThreadObject) {
	
		KeSetEvent( &NdasrGlobals->DraidExitEvent, IO_NO_INCREMENT, FALSE );

		status = KeWaitForSingleObject( NdasrGlobals->DraidThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ObDereferenceObject( NdasrGlobals->DraidThreadObject );
		ZwClose( NdasrGlobals->DraidThreadHandle );
		
		NdasrGlobals->DraidThreadHandle = NULL;
		NdasrGlobals->DraidThreadObject = NULL;
	}

	while(TRUE) {

		if (InterlockedCompareExchange(&NdasrGlobals->ReceptionThreadCount, 0, 0) == 0) {

			break;
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Reception thread is still running.. Waiting..\n") );

		Timeout.QuadPart = - NANO100_PER_SEC/2; // 500ms
		
		KeDelayExecutionThread( KernelMode, FALSE, &Timeout );		
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Stopped\n") );

	return STATUS_SUCCESS;
}

VOID
NdasRaidTdiBindingHandler (
	IN TDI_PNP_OPCODE	PnPOpcode,
	IN PUNICODE_STRING  DeviceName,
	IN PWSTR			MultiSZBindList
	)
{
	UNREFERENCED_PARAMETER( DeviceName );
	UNREFERENCED_PARAMETER( MultiSZBindList);

#if DBG
	if (DeviceName && DeviceName->Buffer) {
	
		DebugTrace( NDASSCSI_DBG_LUR_ERROR, 
					("DeviceName=%ws PnpOpcode=%x\n", DeviceName->Buffer, PnPOpcode) );

	} else {

		DebugTrace( NDASSCSI_DBG_LUR_ERROR, 
					("DeviceName=NULL PnpOpcode=%x\n", PnPOpcode) );
	}
#endif

	switch (PnPOpcode) {

	case TDI_PNP_OP_ADD:
		
		DebugTrace( DBG_OTHER_INFO, ("TDI_PNP_OP_ADD\n") );
		break;
	
	case TDI_PNP_OP_DEL:
	
		DebugTrace( DBG_OTHER_INFO, ("TDI_PNP_OP_DEL\n") );
		break;
	
	case TDI_PNP_OP_PROVIDERREADY:
		
		DebugTrace( DBG_OTHER_INFO, ("TDI_PNP_OP_PROVIDERREADY\n") );
		break;
	
	case TDI_PNP_OP_NETREADY:
		
		DebugTrace( DBG_OTHER_INFO, ("TDI_PNP_OP_NETREADY\n") );
		break;
	
	default:

		DebugTrace( NDASSCSI_DBG_LUR_ERROR, ("Unknown PnP code. %x\n", PnPOpcode) );
	}
}

VOID
NdasRaidTdiAddAddressHandlerV2 ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI add address handler

Arguments:
    
    NetworkAddress  - new network address available on the system

    DeviceName      - name of the device to which address belongs

    Context         - PDO to which address belongs

Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

	PAGED_CODE ();

	UNREFERENCED_PARAMETER( Context );

	if (DeviceName == NULL) {

		NDAS_BUGON( FALSE );
		return;
	}

	if (!FlagOn(NdasrGlobalData.Flags, NDASR_GLOBALS_FLAG_INITIALIZE)) {

		NDAS_BUGON( FALSE );
		return; 
	}

	DebugTrace( NDASSCSI_DBG_LUR_INFO, 
				("DeviceName=%ws AddrType=%u AddrLen=%u\n",
				 DeviceName->Buffer,
				 (ULONG)NetworkAddress->AddressType,
				 (ULONG)NetworkAddress->AddressLength) );

	//	LPX

	RtlInitUnicodeString( &lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX );

	if (RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) &&
		NetworkAddress->AddressType == TDI_ADDRESS_TYPE_LPX) {

		PTDI_ADDRESS_LPX	lpxAddr;
		KIRQL				oldIrql;
		PLIST_ENTRY			listEntry;

		lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
		
		DebugTrace( NDASSCSI_DBG_LUR_ERROR, 
					("LPX address added: %02x:%02x:%02x:%02x:%02x:%02x\n",
					 lpxAddr->Node[0],
					 lpxAddr->Node[1],
					 lpxAddr->Node[2],
					 lpxAddr->Node[3],
					 lpxAddr->Node[4],
					 lpxAddr->Node[5]) );

		//	LPX may leave dummy values.

		RtlZeroMemory( lpxAddr->Reserved, sizeof(lpxAddr->Reserved) );

		NdasRaidListnerAddAddressHandler( &NdasrGlobalData, lpxAddr );

		ACQUIRE_SPIN_LOCK( &NdasrGlobalData.IdeDiskQSpinlock, &oldIrql );

		for (listEntry = NdasrGlobalData.IdeDiskQueue.Flink;
			 listEntry != &NdasrGlobalData.IdeDiskQueue;
			 listEntry = listEntry->Flink) {

			PLURNEXT_IDE_DEVICE ideDisk;

			ideDisk = CONTAINING_RECORD( listEntry, LURNEXT_IDE_DEVICE, AllListEntry );

			IdeDiskAddAddressHandler( ideDisk, NetworkAddress );
		}

		RELEASE_SPIN_LOCK( &NdasrGlobalData.IdeDiskQSpinlock, oldIrql );

	} else if (NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
	
		//	IP	address
	
		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		
		DebugTrace( NDASSCSI_DBG_LUR_INFO, 
				   ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]) );
	
	} else {
	
		DebugTrace( NDASSCSI_DBG_LUR_INFO, 
				   ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType) );
	}
}

VOID
NdasRaidTdiDelAddressHandlerV2 ( 
	IN PTA_ADDRESS		NetworkAddress,
	IN PUNICODE_STRING	DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI delete address handler

Arguments:
    
    NetworkAddress  - network address that is no longer available on the system

    Context1        - name of the device to which address belongs

    Context2        - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

	PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {

		DebugTrace( NDASSCSI_DBG_LUR_ERROR, 
				   ("AfdDelAddressHandler: "
					"NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
					NetworkAddress->AddressType) );
		return;
	}

	if (!FlagOn(NdasrGlobalData.Flags, NDASR_GLOBALS_FLAG_INITIALIZE)) {

		NDAS_BUGON( FALSE );
		return; 
	}
	
	DebugTrace(NDASSCSI_DBG_LUR_INFO,
		       ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
				DeviceName->Buffer,
				(ULONG)NetworkAddress->AddressType,
				(ULONG)NetworkAddress->AddressLength) );

	//	LPX

	RtlInitUnicodeString( &lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX );

	if (RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE)) {
		
		PTDI_ADDRESS_LPX	lpxAddr;

		lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
		
		DebugTrace( NDASSCSI_DBG_LUR_ERROR, 
				    ("LPX address deleted: %02x:%02x:%02x:%02x:%02x:%02x\n",
					 lpxAddr->Node[0], lpxAddr->Node[1],
					 lpxAddr->Node[2], lpxAddr->Node[3],
					 lpxAddr->Node[4], lpxAddr->Node[5]) );

		RtlZeroMemory( lpxAddr->Reserved, sizeof(lpxAddr->Reserved) );		

		NdasRaidListnerDelAddress( &NdasrGlobalData, lpxAddr );
	
	} else if (NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {

		//	IP	address

		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;

		DebugTrace( NDASSCSI_DBG_LUR_INFO, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]) );

	} else {

		DebugTrace( NDASSCSI_DBG_LUR_INFO, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType) );
	}
}

NTSTATUS
NdasRaidTdPnPPowerHandler (
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
	)
{
	NTSTATUS				status;
	UNICODE_STRING			lpxPrefix;
	NET_DEVICE_POWER_STATE	powerState;


	UNREFERENCED_PARAMETER( Context1 );
	UNREFERENCED_PARAMETER( Context2 );

	if (DeviceName == NULL) {

		DebugTrace( NDASSCSI_DBG_LUR_ERROR, 
					("NO DEVICE NAME SUPPLIED when power event of type %x.\n",
					 PowerEvent->NetEvent) );

		return STATUS_SUCCESS;
	}

	if (PowerEvent == NULL) {
	
		return STATUS_SUCCESS;
	}

	if (PowerEvent->Buffer == NULL || PowerEvent->BufferLength == 0) {
		
		powerState = NetDeviceStateUnspecified;
	
	} else {
	
		powerState = *((PNET_DEVICE_POWER_STATE) PowerEvent->Buffer);
	}

	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);

	if (DeviceName == NULL || RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) == FALSE) {

		DebugTrace( NDASSCSI_DBG_LUR_ERROR, ("Not LPX binding device.\n") );

		return STATUS_SUCCESS;
	}

	status = STATUS_SUCCESS;

	switch (PowerEvent->NetEvent) {

		case NetEventSetPower:
		
			DebugTrace( DBG_OTHER_INFO, ("SetPower\n") );

			if(powerState != NetDeviceStateD0) {

				// Flush all RAID instances if exist.

				NdasRaidFlushAll( &NdasrGlobalData );;				
			}

			break;

		default:

			break;
	}

	// Call default power change handler

	return LsuClientPnPPowerChange( DeviceName, PowerEvent, Context1, Context2 );
}

VOID
NdasRaidFlushAll (
	IN PNDASR_GLOBALS NdasrGlobals
	) 
{
	KIRQL			oldIrql;
	PLIST_ENTRY		listEntry;
	PNDASR_CLIENT	ndasrClient;

	if (!FlagOn(NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE)) {

		NDAS_BUGON( FALSE );
		return; 
	}

	DebugTrace( DBG_LURN_INFO, ("DRAID flush all\n") );
	
	// Flush request to all client 
	
	ACQUIRE_SPIN_LOCK( &NdasrGlobals->ClientQSpinlock, &oldIrql );
	
	for (listEntry = NdasrGlobals->ClientQueue.Flink;
		 listEntry != &NdasrGlobals->ClientQueue;
		 listEntry = listEntry->Flink) {

		ndasrClient = CONTAINING_RECORD( listEntry, NDASR_CLIENT, AllListEntry );
		ndasrClient->Flush( ndasrClient );
	}

	RELEASE_SPIN_LOCK( &NdasrGlobals->ClientQSpinlock, oldIrql );

	// Send flush request to arbiter. (Not needed if multi-write is not used.)
}

INT32 
NdasRaidReallocEventArray (
	PKEVENT**		Events,
	PKWAIT_BLOCK*	WaitBlocks,
	INT32			CurrentCount
	) 
{
	INT32 newCount;
	
	newCount = CurrentCount + 4;
	
	DebugTrace( DBG_LURN_TRACE, ("Allocating event array to count %d\n", newCount) );
	
	if (*Events) {

		ExFreePoolWithTag( *Events, NDASR_EVENT_ARRAY_POOL_TAG );
	}
	
	if (*WaitBlocks) {

		ExFreePoolWithTag( *WaitBlocks, NDASR_EVENT_ARRAY_POOL_TAG );
	}
	
	*Events = ExAllocatePoolWithTag( NonPagedPool, sizeof(PKEVENT) * newCount, NDASR_EVENT_ARRAY_POOL_TAG );
	*WaitBlocks = ExAllocatePoolWithTag( NonPagedPool, sizeof(KWAIT_BLOCK) * newCount, NDASR_EVENT_ARRAY_POOL_TAG );
	
	return newCount;
}

VOID
NdasRaidFreeEventArray (
	PKEVENT*		Events,
	PKWAIT_BLOCK	WaitBlocks
	) 
{
	if (Events) {

		ExFreePoolWithTag( Events, NDASR_EVENT_ARRAY_POOL_TAG );
	}

	if (WaitBlocks) {
	
		ExFreePoolWithTag( WaitBlocks, NDASR_EVENT_ARRAY_POOL_TAG );
	}
}

NTSTATUS 
NdasRaidRegisterArbitrator (
	IN PNDASR_GLOBALS NdasrGlobals,
	IN PNDASR_ARBITRATOR NdasrArbitrator
	) 
{
	ExInterlockedInsertTailList( &NdasrGlobals->ArbitratorQueue, 
								 &NdasrArbitrator->AllListEntry, 
								 &NdasrGlobals->ArbitratorQSpinlock );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidRegisterIdeDisk (
	IN PNDASR_GLOBALS		NdasrGlobals,
	IN PLURNEXT_IDE_DEVICE  IdeDisk
	) 
{
	ExInterlockedInsertTailList( &NdasrGlobals->IdeDiskQueue, 
								 &IdeDisk->AllListEntry, 
								 &NdasrGlobals->IdeDiskQSpinlock );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidUnregisterIdeDisk (
	IN PNDASR_GLOBALS		NdasrGlobals,
	IN PLURNEXT_IDE_DEVICE  IdeDisk
	) 
{
	KIRQL oldIrql;

	ACQUIRE_SPIN_LOCK( &NdasrGlobals->IdeDiskQSpinlock, &oldIrql );
	RemoveEntryList( &IdeDisk->AllListEntry );
	RELEASE_SPIN_LOCK( &NdasrGlobals->IdeDiskQSpinlock, oldIrql );
	
	return STATUS_SUCCESS;	
}

NTSTATUS
NdasRaidUnregisterArbitrator (
	IN PNDASR_GLOBALS		NdasrGlobals,
	IN PNDASR_ARBITRATOR	NdasrArbitrator
	) 
{
	KIRQL oldIrql;

	ACQUIRE_SPIN_LOCK( &NdasrGlobals->ArbitratorQSpinlock, &oldIrql );
	RemoveEntryList( &NdasrArbitrator->AllListEntry );
	RELEASE_SPIN_LOCK( &NdasrGlobals->ArbitratorQSpinlock, oldIrql );

	return STATUS_SUCCESS;	
}

NTSTATUS
NdasRaidRegisterClient (
	IN PNDASR_GLOBALS NdasrGlobals,
	IN PNDASR_CLIENT NdasrClient
	) 
{
	ExInterlockedInsertTailList( &NdasrGlobals->ClientQueue, 
								 &NdasrClient->AllListEntry, 
								 &NdasrGlobals->ClientQSpinlock );

	return STATUS_SUCCESS;
}

NTSTATUS
NdasRaidUnregisterClient (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PNDASR_CLIENT	NdasrClient
	) 
{
	KIRQL oldIrql;

	ACQUIRE_SPIN_LOCK( &NdasrGlobals->ClientQSpinlock, &oldIrql );
	RemoveEntryList( &NdasrClient->AllListEntry );
	RELEASE_SPIN_LOCK( &NdasrGlobals->ClientQSpinlock, oldIrql );
	
	return STATUS_SUCCESS;	
}

VOID 
NdasRaidListenerThreadProc (
	IN PNDASR_GLOBALS NdasrGlobals
	) 
{
	NTSTATUS				status;

	BOOLEAN					terminateThread = FALSE;

	PLIST_ENTRY				listEntry;
	PNDASR_LISTEN_CONTEXT	listenContext;

	KIRQL					oldIrql;
	
	INT32 					maxEventCount = 0;
	PKEVENT					*events = NULL;
	PKWAIT_BLOCK			waitBlocks = NULL;
	INT32					eventCount;

	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	INT32					i;
	

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasRaidListenerThreadProc Starting\n") );

	maxEventCount = NdasRaidReallocEventArray( &events, &waitBlocks, maxEventCount );
	
	status = LpxTdiV2GetAddressList( &socketLpxAddressList );

	if (status != STATUS_SUCCESS) {

		KeSetEvent( &NdasrGlobals->DraidThreadReadyEvent, IO_DISK_INCREMENT, FALSE );
	
		NDAS_BUGON( FALSE );
		goto out;
	}

	for (i = 0; i < socketLpxAddressList.iAddressCount; i++) {
		
		listenContext = NdasRaidCreateListenContext( NdasrGlobals, &socketLpxAddressList.SocketLpx[i].LpxAddress );

		if (listenContext == NULL) {

			NDAS_BUGON( FALSE );
			continue;
		}
			
		status = NdasRaidStartListenAddress( NdasrGlobals, listenContext );
			
		if (status != STATUS_PENDING) {

			ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );					
			RemoveEntryList( &listenContext->Link );
			RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );					

			NdasRaidStopListenAddress( NdasrGlobals, listenContext );
		}
	}

	KeSetEvent( &NdasrGlobals->DraidThreadReadyEvent, IO_DISK_INCREMENT, FALSE );
	
	do {

		if (events == NULL || waitBlocks == NULL) {

			NDAS_BUGON( FALSE );

			terminateThread = TRUE;
			continue;
		}

		eventCount = 0;
		
		//	Wait exit event, net change event, connect request.
		
		events[0] = &NdasrGlobals->DraidExitEvent;
		eventCount++;
		events[1] = &NdasrGlobals->NetChangedEvent;
		eventCount++;

		// Add listening event 
		
		ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );
		
		for (listEntry = NdasrGlobals->ListenContextList.Flink;
			 listEntry != &NdasrGlobals->ListenContextList;
			 listEntry = listEntry->Flink) {

			listenContext = CONTAINING_RECORD (listEntry, NDASR_LISTEN_CONTEXT, Link);
			
			if (!listenContext->Destroy) {

				if (maxEventCount < eventCount+1) {
				
					maxEventCount = NdasRaidReallocEventArray( &events, &waitBlocks, maxEventCount );
					break;
				}

				events[eventCount] = &listenContext->ListenOverlapped.Request[0].CompletionEvent;
				eventCount++;
			}
		}

		RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );

		if (listEntry != &NdasrGlobals->ListenContextList) {

			continue;
		}

		status = KeWaitForMultipleObjects( eventCount, 
										   events,
										   WaitAny, 
										   Executive,
										   KernelMode,
										   TRUE,
										   NULL, //&TimeOut,
										   waitBlocks );

		if (KeReadStateEvent(&NdasrGlobals->DraidExitEvent)) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Exit requested\n") );

			terminateThread = TRUE;
			continue;
		}

		if (KeReadStateEvent(&NdasrGlobals->NetChangedEvent)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NIC status has changed\n") );

			KeClearEvent( &NdasrGlobals->NetChangedEvent );

			// Start if not listening and stop if destorying request is on.

			ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );

			listEntry = NdasrGlobals->ListenContextList.Flink;

			while (listEntry != &NdasrGlobals->ListenContextList) {
				
				listenContext = CONTAINING_RECORD(listEntry, NDASR_LISTEN_CONTEXT, Link);
				listEntry = listEntry->Flink;
				
				if (listenContext->Destroy) {

					RemoveEntryList( &listenContext->Link );

					RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );					
					NdasRaidStopListenAddress( NdasrGlobals, listenContext );
					ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );					

					continue;
				}
								
				if (listenContext->Started == FALSE) {
				
					RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );	
					status = NdasRaidStartListenAddress( NdasrGlobals, listenContext );
					ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );
				
					if (status != STATUS_PENDING) {
				
						RemoveEntryList( &listenContext->Link );
					
						RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );					
						NdasRaidStopListenAddress( NdasrGlobals, listenContext );
						ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );					

						continue;
					}
				}
			}

			RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );

			continue; // Reset listen event.
		}

		// Check whether listen event has been signaled.

		for (i = 2; i < eventCount; i++) {

			if (KeReadStateEvent(events[i])) {

				// Find listencontext related to this event.

				ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );
				
				for (listEntry = NdasrGlobals->ListenContextList.Flink;
					 listEntry != &NdasrGlobals->ListenContextList;
					 listEntry = listEntry->Flink) {

					listenContext = CONTAINING_RECORD (listEntry, NDASR_LISTEN_CONTEXT, Link);
					
					if (&listenContext->ListenOverlapped.Request[0].CompletionEvent == events[i]) {
					
						break;
					}

					listenContext = NULL;
				}

				RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );	
				
				if (listenContext == NULL) {

					NDAS_BUGON( FALSE );
					KeClearEvent( events[i] );

					continue;
				}

				LpxTdiV2CompleteRequest( &listenContext->ListenOverlapped, 0 );

				if (listenContext->ListenOverlapped.Request[0].IoStatusBlock.Status == STATUS_SUCCESS) {

					status = NdasRaidAcceptConnection( NdasrGlobals, listenContext );
					NDAS_BUGON( status == STATUS_SUCCESS );

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed to listen. Maybe network is down.\n") );	
					NDAS_BUGON( NDAS_BUGON_NETWORK_FAIL ); // May be this can be happen if multiple instances of this function is called with same address
			
					LpxTdiV2DisassociateAddress( listenContext->ListenFileObject );
					LpxTdiV2CloseConnection( listenContext->ListenFileHandle, 
											 listenContext->ListenFileObject, 
											 &listenContext->ListenOverlapped );

					listenContext->ListenFileHandle = NULL;
					listenContext->ListenFileObject = NULL;
				}

				status = NdasRaidListenConnection( listenContext );

				if (status != STATUS_PENDING) {

					NDAS_BUGON( FALSE );
				}
			}
		}
	
	} while (terminateThread == FALSE);

out:

	// to do: clean-up pending client

	// Close and free pending listen contexts

	while (listEntry = ExInterlockedRemoveHeadList(&NdasrGlobals->ListenContextList, 
												   &NdasrGlobals->ListenContextSpinlock)) {

		listenContext = CONTAINING_RECORD( listEntry, NDASR_LISTEN_CONTEXT, Link );
		NdasRaidStopListenAddress( NdasrGlobals, listenContext );
	}

	NdasRaidFreeEventArray( events, waitBlocks );
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasRaidListenerThreadProc Exiting\n") );

	PsTerminateSystemThread( STATUS_SUCCESS );	
}

VOID
NdasRaidListnerAddAddressHandler (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PTDI_ADDRESS_LPX Addr
	) 
{
	if (!FlagOn(NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE)) {

		NDAS_BUGON( FALSE );
		return; 
	}

	NdasRaidCreateListenContext( NdasrGlobals, Addr );

	KeSetEvent( &NdasrGlobals->NetChangedEvent, IO_NO_INCREMENT, FALSE );	
}

PNDASR_LISTEN_CONTEXT 
NdasRaidCreateListenContext (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PLPX_ADDRESS		Addr
	) 
{
	KIRQL					oldIrql;
	BOOLEAN					alreadyExist;
	PLIST_ENTRY				listEntry;
	PNDASR_LISTEN_CONTEXT	listenContext;
		
	// Check address is already in the listen context list

	ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );

	alreadyExist = FALSE;
	
	for (listEntry = NdasrGlobals->ListenContextList.Flink;
		 listEntry != &NdasrGlobals->ListenContextList;
		 listEntry = listEntry->Flink) {

		listenContext = CONTAINING_RECORD (listEntry, NDASR_LISTEN_CONTEXT, Link);
		
		if (!listenContext->Destroy && 
			RtlCompareMemory(listenContext->Addr.Node, Addr->Node, 6) == 6) {
			
			DebugTrace(DBG_LURN_INFO, ("New LPX address already exist.Ignoring.\n"));
			alreadyExist = TRUE;
			break;
		}
	}

	RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );
	
	if (alreadyExist) {

		return NULL;
	}

	// Alloc listen context

	listenContext = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_LISTEN_CONTEXT), NDASR_LISTEN_CONTEXT_POOL_TAG );
	
	if (!listenContext) {
	
		NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("created listenContext = %p\n", listenContext) );

	RtlZeroMemory( listenContext, sizeof(NDASR_LISTEN_CONTEXT) );

	InitializeListHead( &listenContext->Link );

	RtlCopyMemory( listenContext->Addr.Node, Addr->Node, 6 );
	listenContext->Addr.Port = HTONS(LPXRP_NRMX_ARBITRRATOR_PORT);

	ExInterlockedInsertTailList( &NdasrGlobals->ListenContextList, 
								 &listenContext->Link, 
								 &NdasrGlobals->ListenContextSpinlock );
	
	return listenContext;
}

VOID
NdasRaidListnerDelAddress (
	IN PNDASR_GLOBALS	NdasrGlobals,
	IN PLPX_ADDRESS		Addr
	) 
{
	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
	PNDASR_LISTEN_CONTEXT	listenContext;
	
	if (!FlagOn(NdasrGlobals->Flags, NDASR_GLOBALS_FLAG_INITIALIZE)) {

		NDAS_BUGON( FALSE );
		return; 
	}
	
	// Find matching address and just mark active flag false because Wait event may be in use.	
	
	ACQUIRE_SPIN_LOCK( &NdasrGlobalData.ListenContextSpinlock, &oldIrql );

	for (listEntry = NdasrGlobals->ListenContextList.Flink;
		 listEntry != &NdasrGlobals->ListenContextList;
		 listEntry = listEntry->Flink) {

		listenContext = CONTAINING_RECORD( listEntry, NDASR_LISTEN_CONTEXT, Link );
		
		if (RtlCompareMemory(listenContext->Addr.Node, Addr->Node, 6) == 6) {
			
			DebugTrace( DBG_LURN_INFO, ("Found matching address\n") );

			listenContext->Destroy = TRUE;

			KeSetEvent( &NdasrGlobals->NetChangedEvent, IO_NO_INCREMENT, FALSE );
			break;
		}
	}

	RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );
}

NTSTATUS 
 NdasRaidStartListenAddress (
	PNDASR_GLOBALS			NdasrGlobals,
	PNDASR_LISTEN_CONTEXT	ListenContext
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;

	DebugTrace( DBG_LURN_INFO, 
				("Starting to listen address %02x:%02x:%02x:%02x:%02x:%02x\n",
				 ListenContext->Addr.Node[0], ListenContext->Addr.Node[1], ListenContext->Addr.Node[2],
				 ListenContext->Addr.Node[3], ListenContext->Addr.Node[4], ListenContext->Addr.Node[5]) );

	ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );
	ListenContext->Started = TRUE;
	RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );

	// Start listen.

	// 1. Open address

	status = LpxTdiV2OpenAddress( &ListenContext->Addr,
								  &ListenContext->AddressFileHandle,
								  &ListenContext->AddressFileObject );

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( NDAS_BUGON_NETWORK_FAIL );
		goto errout;
	}
	
	// 2. Open connection

	status = NdasRaidListenConnection( ListenContext );

	if (status != STATUS_PENDING) {

		NDAS_BUGON( FALSE );
		goto errout;
	}

	return status;
	
errout:

	if (ListenContext->AddressFileHandle && ListenContext->AddressFileObject) {

		LpxTdiV2CloseAddress( ListenContext->AddressFileHandle, ListenContext->AddressFileObject );
		ListenContext->AddressFileHandle = NULL;
		ListenContext->AddressFileObject = NULL;
	}

	ACQUIRE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, &oldIrql );
	ListenContext->Started = FALSE;
	RELEASE_SPIN_LOCK( &NdasrGlobals->ListenContextSpinlock, oldIrql );

	return status;
}

NTSTATUS 
NdasRaidStopListenAddress (
	PNDASR_GLOBALS			NdasrGlobals,
	PNDASR_LISTEN_CONTEXT	ListenContext
	) 
{
	UNREFERENCED_PARAMETER( NdasrGlobals );
	
	DebugTrace( DBG_LURN_INFO, 
				("Stop listening to address %02x:%02x:%02x:%02x:%02x:%02x\n",
				 ListenContext->Addr.Node[0], ListenContext->Addr.Node[1], ListenContext->Addr.Node[2],
				 ListenContext->Addr.Node[3], ListenContext->Addr.Node[4], ListenContext->Addr.Node[5]) );
	
	if (ListenContext->ListenFileHandle) {

		NDAS_BUGON( ListenContext->ListenFileHandle );
		NDAS_BUGON( ListenContext->ListenFileObject );

		if (LpxTdiV2IsRequestPending(&ListenContext->ListenOverlapped, 0)) {

			LpxTdiV2CancelRequest( ListenContext->ListenFileObject, &ListenContext->ListenOverlapped, 0, FALSE, 0 );
		}
	
		LpxTdiV2Disconnect( ListenContext->ListenFileObject, 0 );
		LpxTdiV2DisassociateAddress( ListenContext->ListenFileObject );
		LpxTdiV2CloseConnection( ListenContext->ListenFileHandle, ListenContext->ListenFileObject, &ListenContext->ListenOverlapped );
	
	} else {

		NDAS_BUGON( ListenContext->ListenFileHandle == NULL );
		NDAS_BUGON( ListenContext->ListenFileObject == NULL );
	}

	if (ListenContext->AddressFileHandle && ListenContext->AddressFileObject) {
	
		LpxTdiV2CloseAddress( ListenContext->AddressFileHandle, ListenContext->AddressFileObject );
	}

	ExFreePoolWithTag( ListenContext, NDASR_LISTEN_CONTEXT_POOL_TAG );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Freed listenContext = %p\n", ListenContext) );
	
	return STATUS_SUCCESS;
}

NTSTATUS 
NdasRaidListenConnection (
	PNDASR_LISTEN_CONTEXT ListenContext
	) 
{
	NTSTATUS status;

	NDAS_BUGON( ListenContext->ListenFileHandle == NULL );
	NDAS_BUGON( ListenContext->ListenFileObject == NULL );

	status = LpxTdiV2OpenConnection( NULL,
									 &ListenContext->ListenFileHandle,
								     &ListenContext->ListenFileObject,
									 &ListenContext->ListenOverlapped );

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );
		return status;
	}

	status = LpxTdiV2AssociateAddress( ListenContext->ListenFileObject,
									   ListenContext->AddressFileHandle );


	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );

		LpxTdiV2CloseConnection( ListenContext->ListenFileHandle, ListenContext->ListenFileObject, &ListenContext->ListenOverlapped );
		return status;
	}

	ListenContext->Flags = TDI_QUERY_ACCEPT; 

	status = LpxTdiV2Listen( ListenContext->ListenFileObject,
							 NULL,
							 NULL,
							 ListenContext->Flags,
							 NULL,
							 &ListenContext->ListenOverlapped,
							 NULL );

	if (status != STATUS_PENDING) {
	
		NDAS_BUGON( FALSE ); // May be this can be happen if multiple instances of this function is called with same address
		
		LpxTdiV2CompleteRequest( &ListenContext->ListenOverlapped, 0 );
		
		LpxTdiV2DisassociateAddress( ListenContext->ListenFileObject );
		LpxTdiV2CloseConnection( ListenContext->ListenFileHandle, ListenContext->ListenFileObject, &ListenContext->ListenOverlapped );

		return status;
	}

	return status;
}

NTSTATUS
NdasRaidAcceptConnection (
	PNDASR_GLOBALS			NdasrGlobals,
	PNDASR_LISTEN_CONTEXT	ListenContext
	) 
{
	PNDASR_ARBITRATOR_CONNECTION	connection;
	NTSTATUS					status;
	OBJECT_ATTRIBUTES			objectAttributes;
	HANDLE						threadHandle;

	PTRANSPORT_ADDRESS			tpAddr;
	PTA_ADDRESS					taAddr;
	PLPX_ADDRESS				lpxAddr;


	connection = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_ARBITRATOR_CONNECTION), NDASR_REMOTE_CLIENT_CHANNEL_POOL_TAG );

	if (!connection) {

		NDAS_BUGON( FALSE );

		LpxTdiV2CompleteRequest( &ListenContext->ListenOverlapped, 0 );

		LpxTdiV2DisassociateAddress( ListenContext->ListenFileObject );
		LpxTdiV2CloseConnection( ListenContext->ListenFileHandle, 
								 ListenContext->ListenFileObject, 
								 &ListenContext->ListenOverlapped );

		ListenContext->ListenFileHandle = NULL;
		ListenContext->ListenFileObject = NULL;

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Get information about new connection and prepare for new listening.

	RtlZeroMemory( connection, sizeof(NDASR_ARBITRATOR_CONNECTION) );

	connection->ConnectionFileHandle = ListenContext->ListenFileHandle;
	connection->ConnectionFileObject = ListenContext->ListenFileObject;
	
	LpxTdiV2MoveOverlappedContext( &connection->ReceiveOverlapped, &ListenContext->ListenOverlapped );

	ListenContext->ListenFileHandle = NULL;
	ListenContext->ListenFileObject = NULL;

	tpAddr = (PTRANSPORT_ADDRESS)connection->ReceiveOverlapped.Request[0].AddressBuffer;
	taAddr = tpAddr->Address;
	lpxAddr = (PLPX_ADDRESS)taAddr->Address;

	RtlCopyMemory( &connection->RemoteAddr, lpxAddr, sizeof(LPX_ADDRESS) );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("connection = %p Connected from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
				 connection, connection->RemoteAddr.Node[0], connection->RemoteAddr.Node[1],
				 connection->RemoteAddr.Node[2], connection->RemoteAddr.Node[3],
				 connection->RemoteAddr.Node[4], connection->RemoteAddr.Node[5],
				 NTOHS(connection->RemoteAddr.Port)) );
		
	// Start reception thread.

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );
	
	status = PsCreateSystemThread( &threadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   NdasRaidReceptionThreadProc,
								   (PVOID) connection );

	if (!NT_SUCCESS(status)) {

		NDAS_BUGON( FALSE );

		LpxTdiV2DisassociateAddress( connection->ConnectionFileObject );
		LpxTdiV2CloseConnection( connection->ConnectionFileHandle,
								 connection->ConnectionFileObject, 
								 &connection->ReceiveOverlapped );
		
		ExFreePoolWithTag( connection, NDASR_REMOTE_CLIENT_CHANNEL_POOL_TAG );

		return status;
	}

	ZwClose( threadHandle );  // Reception thread will be exited by itself. Close now.

	InterlockedIncrement( &NdasrGlobals->ReceptionThreadCount );

	return STATUS_SUCCESS;
}

VOID
NdasRaidReceptionThreadProc (
	PNDASR_ARBITRATOR_CONNECTION Connection
	) 
{
	NTSTATUS			status;
	NRMX_REGISTER		registerMsg;
	NRMX_HEADER			registerReply = {0};
	KIRQL				oldIrql;
	PLIST_ENTRY			listEntry;
	ULONG				result;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("start Connection = %p\n", Connection) );

	do {
	
		// Wait for network event or short timeout

		status = LpxTdiV2Recv( Connection->ConnectionFileObject,
							   (PUCHAR)&registerMsg,
							   sizeof(NRMX_REGISTER),
							   0,
							   NULL,
							   NULL,
							   0,
							   &result );

		if (result != sizeof(NRMX_REGISTER)) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiRecvWithCompletionEvent returned %d.\n", status) );

			status = STATUS_CONNECTION_DISCONNECTED;
			break;
		}

		// Data received. Check validity and forward channel to arbiter.

		if (NTOHL(registerMsg.Header.Signature) != NRMX_SIGNATURE	||
			registerMsg.Header.ReplyFlag							||
			registerMsg.Header.Command != NRMX_CMD_REGISTER			||
			NTOHS(registerMsg.Header.Length) !=  sizeof(NRMX_REGISTER)) {

			NDAS_BUGON( FALSE );
		
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		status = STATUS_UNSUCCESSFUL;

		ACQUIRE_SPIN_LOCK( &NdasrGlobalData.ArbitratorQSpinlock, &oldIrql );

		for (listEntry = NdasrGlobalData.ArbitratorQueue.Flink;
			 listEntry != &NdasrGlobalData.ArbitratorQueue;
			 listEntry = listEntry->Flink) {

			PNDASR_ARBITRATOR ndasArbitrator;

			ndasArbitrator = CONTAINING_RECORD( listEntry, NDASR_ARBITRATOR, AllListEntry );
		
			if (RtlCompareMemory(&ndasArbitrator->Lurn->NdasrInfo->Rmd.RaidSetId, &registerMsg.RaidSetId, sizeof(GUID)) == sizeof(GUID) &&
				RtlCompareMemory(&ndasArbitrator->Lurn->NdasrInfo->Rmd.ConfigSetId, &registerMsg.ConfigSetId, sizeof(GUID)) == sizeof(GUID)) {
			
				status = ndasArbitrator->AcceptClient( ndasArbitrator, &registerMsg, &Connection );
	
				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("accept client %x Connection = %p\n", status, Connection) );

				break;
			}
		}

		RELEASE_SPIN_LOCK( &NdasrGlobalData.ArbitratorQSpinlock, oldIrql );
		
	} while (0);

	if (status != STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Closing connection to client.\n") );

		// Close connection.

		if (Connection) {

			registerReply.Signature = NTOHL(NRMX_SIGNATURE);
			registerReply.Command	= NRMX_CMD_REGISTER;
			registerReply.Length	= NTOHS((UINT16)sizeof(NRMX_HEADER));
			registerReply.ReplyFlag = 1;
			registerReply.Sequence	= registerMsg.Header.Sequence;
			registerReply.Result	= (status == STATUS_SUCCESS) ? NRMX_RESULT_SUCCESS : NRMX_RESULT_RAID_SET_NOT_FOUND;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("DRAID Sending registration reply(result=%x) to remote client\n", registerReply.Result) );

			status = LpxTdiV2Send( Connection->ConnectionFileObject, 
								   (PUCHAR)&registerReply, 
								   sizeof(NRMX_HEADER), 
								   0, 
								   NULL,
								   NULL,
								   0,
								   &result );

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("LpxTdiSend status=%x, result=%x.\n", status, result) );

			if (result != sizeof(NRMX_HEADER)) {

				status = STATUS_CONNECTION_DISCONNECTED;
			}

			LpxTdiV2DisassociateAddress( Connection->ConnectionFileObject );
			LpxTdiV2CloseConnection( Connection->ConnectionFileHandle,
									 Connection->ConnectionFileObject, 
									 &Connection->ReceiveOverlapped );
		
			ExFreePoolWithTag( Connection, NDASR_REMOTE_CLIENT_CHANNEL_POOL_TAG );
		}
	}
	
	DebugTrace( DBG_LURN_INFO, ("Exiting reception thread.\n") );

	// Decrease counter
	
	InterlockedDecrement( &NdasrGlobalData.ReceptionThreadCount );
}

LURN_INTERFACE LurnNdasAggregationInterface = {
	
	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_NDAS_AGGREGATION,
	LURN_DEVICE_INTERFACE_LURN,
	0,

	{
		NdasRaidLurnCreate,
		NdasRaidLurnClose,
		NdasRaidLurnStop,
		NdasRaidLurnRequest
	}
};

LURN_INTERFACE LurnNdasRaid0Interface = {
	
	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_NDAS_RAID0,
	LURN_DEVICE_INTERFACE_LURN,
	0,

	{
		NdasRaidLurnCreate,
		NdasRaidLurnClose,
		NdasRaidLurnStop,
		NdasRaidLurnRequest
	}
};

LURN_INTERFACE LurnNdasRaid1Interface = {
	
	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_NDAS_RAID1,
	LURN_DEVICE_INTERFACE_LURN,
	0,

	{
		NdasRaidLurnCreate,
		NdasRaidLurnClose,
		NdasRaidLurnStop,
		NdasRaidLurnRequest
	}
};

LURN_INTERFACE LurnNdasRaid4Interface = {
	
	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_NDAS_RAID4,
	LURN_DEVICE_INTERFACE_LURN,
	0,

	{
		NdasRaidLurnCreate,
		NdasRaidLurnClose,
		NdasRaidLurnStop,
		NdasRaidLurnRequest
	}
};

LURN_INTERFACE LurnNdasRaid5Interface = {
	
	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_NDAS_RAID5,
	LURN_DEVICE_INTERFACE_LURN,
	0,

	{
		NdasRaidLurnCreate,
		NdasRaidLurnClose,
		NdasRaidLurnStop,
		NdasRaidLurnRequest
	}
};


NTSTATUS
NdasRaidLurnSendStopCcbToChildrenSyncronouslyCompletionRoutine (
	IN PCCB		Ccb,
	IN PKEVENT	Event
	)
{
	UNREFERENCED_PARAMETER( Ccb );

	KeSetEvent( Event, 0, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}
	 
NTSTATUS
NdasRaidLurnSendStopCcbToChildrenSyncronously (
	IN PLURELATION_NODE		Lurn,
	IN PCCB					Ccb
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	UINT32				nidx;
	PCCB				nextCcb[LUR_MAX_LURNS_PER_LUR] = {NULL};

	NDAS_BUGON( !LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN) );

	try {

		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {  // idx_client is index of role

			status = LsCcbAllocate( &nextCcb[nidx] );

			if (!NT_SUCCESS(status)) {

				LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
				leave;
			}

			status = LsCcbInitializeByCcb( Ccb, Lurn->LurnChildren[nidx], nextCcb[nidx] );
		
			if (!NT_SUCCESS(status)) {

				LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
				leave;
			}

			ASSERT( Ccb->DataBuffer == nextCcb[nidx]->DataBuffer );
	
			nextCcb[nidx]->AssociateID = (USHORT)nidx;

			LsCcbSetFlag( nextCcb[nidx], CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED );
			LsCcbSetFlag( nextCcb[nidx], Ccb->Flags & CCB_FLAG_SYNCHRONOUS );
		}	 

		LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );

		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {	

			KEVENT	completionEvent;

			if (nextCcb[nidx] == NULL) {

				NDAS_BUGON( FALSE );
				continue;
			}

			KeInitializeEvent( &completionEvent, SynchronizationEvent, FALSE );

			LsCcbSetCompletionRoutine( nextCcb[nidx], 
									   NdasRaidLurnSendStopCcbToChildrenSyncronouslyCompletionRoutine, 
									   &completionEvent );

			status = LurnRequest( Lurn->LurnChildren[nidx], nextCcb[nidx] );
		
			DebugTrace( DBG_LURN_NOISE, ("LurnRequest status : %08x\n", status) );

			if (!NT_SUCCESS(status)) {

				NDAS_BUGON( FALSE );
			}

			if (STATUS_PENDING == status) {

				status = KeWaitForSingleObject( &completionEvent,
												Executive,
												KernelMode,
												FALSE,
												NULL );

				NDAS_BUGON( status == STATUS_SUCCESS );
			}

			NdasRaidLurnSynchronousStopCcbCompletion( Ccb, nextCcb[nidx] );
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_NOISE, 
					("Completing Ccb %s with CcbStatus %x, status = %x\n", 
					 CdbOperationString(Ccb->Cdb[0]), Ccb->CcbStatus, status) );

		if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );
		}

	} finally {

		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {

			if (nextCcb[nidx]) {

				LsCcbFree( nextCcb[nidx] );
				nextCcb[nidx] = NULL;
			}
		}

		LsCcbCompleteCcb( Ccb );
	}

	return status;
}

NTSTATUS
NdasRaidLurnSynchronousStopCcbCompletion (
	IN PCCB	Ccb,
	IN PCCB	ChildCcb
	)
{
	KIRQL		oldIrql;
	NTSTATUS	status;

	NDAS_BUGON( ChildCcb->OperationCode == CCB_OPCODE_STOP );

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &Ccb->CcbSpinLock, &oldIrql );

	if (ChildCcb->OperationCode == CCB_OPCODE_STOP) {

		switch (ChildCcb->CcbStatus) {
	
		case CCB_STATUS_SUCCESS:	// prority 0
	
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
			
			break;

		case CCB_STATUS_NOT_EXIST:

			if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

				Ccb->CcbStatus = ChildCcb->CcbStatus;
			}

			break;

		default:

			NDAS_BUGON( FALSE );

			if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

				Ccb->CcbStatus = ChildCcb->CcbStatus;
			}

			break;
		}
	
		LsCcbSetStatusFlag(	Ccb, ChildCcb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

		RELEASE_SPIN_LOCK( &Ccb->CcbSpinLock, oldIrql );

		status = STATUS_SUCCESS;
	}

	return status;
}

NTSTATUS
NdasRaidLurnReadRmd (
	IN  PLURELATION_NODE		Lurn,
	OUT PNDAS_RAID_META_DATA	Rmd,
	OUT PBOOLEAN				NodeIsUptoDate,
	OUT PUCHAR					UpToDateNode
	)
{
	NTSTATUS			status;

	UCHAR				nidx;
	UCHAR				ridx;
	NDAS_RAID_META_DATA rmd_tmp;
	UCHAR				upToDateNode;
	BOOLEAN				nodeIsUptoDate[MAX_NDASR_MEMBER_DISK] = {FALSE};

	// Update NodeFlags if it's RMD is missing or invalid.
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("IN\n") );

	upToDateNode = 0xff;
	status = STATUS_UNSUCCESSFUL;

	for (nidx =  0; nidx < Lurn->LurnChildrenCnt; nidx++) { // i is node flag

		if (!FlagOn(Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_RUNNING) ||
			FlagOn(Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE)) {
	
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Lurn is not running. Skip reading node %d.\n", nidx) );
			
			nodeIsUptoDate[nidx] = FALSE;
			continue;
		}

		status = NdasRaidLurnExecuteSynchrously( Lurn->LurnChildren[nidx], 
												 SCSIOP_READ16,
												 FALSE,
												 FALSE,
												 (PUCHAR)&rmd_tmp, 
												 (-1*NDAS_BLOCK_LOCATION_RMD), 
												 1,
												 TRUE );

		if (status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("read rmd fail status = %x node %d\n", status, nidx) );

			Lurn->NdasrInfo->LocalNodeFlags[nidx] = NdasRaidLurnStatusToNodeFlag( Lurn->LurnChildren[nidx]->LurnStatus );

			NDAS_BUGON( !FlagOn(Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_RUNNING) ||
						 FlagOn(Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE) );

			break;
		}

		if (!IS_RMD_CRC_VALID(crc32_calc, rmd_tmp)) {

			NDAS_BUGON( FALSE );

			status = NdasRaidLurnExecuteSynchrously( Lurn->LurnChildren[nidx], 
													 SCSIOP_READ16,
													 FALSE,
													 FALSE,
													 (PUCHAR)&rmd_tmp, 
													 (-1*NDAS_BLOCK_LOCATION_RMD_T), 
													 1,
													 TRUE );

			if (status != STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("read rmd fail status = %x node %d\n", status, nidx) );

				Lurn->NdasrInfo->LocalNodeFlags[nidx] = NdasRaidLurnStatusToNodeFlag( Lurn->LurnChildren[nidx]->LurnStatus );

				NDAS_BUGON( !FlagOn(Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_RUNNING) ||
								 FlagOn(Lurn->NdasrInfo->LocalNodeFlags[nidx], NRMX_NODE_FLAG_DEFECTIVE) );

				break;
			}

			if (!IS_RMD_CRC_VALID(crc32_calc, rmd_tmp)) {

				NDAS_BUGON( FALSE );
				break;
			}
		}

		if (NDAS_RAID_META_DATA_SIGNATURE != rmd_tmp.Signature) {

			NDAS_BUGON( FALSE );

			status = STATUS_UNSUCCESSFUL;
			break;
		} 

		if (Lurn->LurnType == LURN_NDAS_RAID0 || Lurn->LurnType == LURN_NDAS_AGGREGATION) {

			RtlCopyMemory( &Lurn->NdasrInfo->NdasRaidId, &rmd_tmp.RaidSetId, sizeof(GUID) );
			RtlCopyMemory( &Lurn->NdasrInfo->ConfigSetId, &rmd_tmp.ConfigSetId, sizeof(GUID) );
			
		} else {

			if (RtlCompareMemory(&Lurn->NdasrInfo->NdasRaidId, &rmd_tmp.RaidSetId, sizeof(GUID)) != sizeof(GUID)) {
		
				NDAS_BUGON( FALSE );

				status = STATUS_UNSUCCESSFUL;
				break;
			}
		
			if (RtlCompareMemory(&Lurn->NdasrInfo->ConfigSetId, &rmd_tmp.ConfigSetId, sizeof(GUID)) != sizeof(GUID)) {
	
				NDAS_BUGON( FALSE );

				status = STATUS_UNSUCCESSFUL;
				break;	
			}
		}

		for (ridx = 0; ridx < Lurn->LurnChildrenCnt; ridx++) {
					
			if (rmd_tmp.UnitMetaData[ridx].iUnitDeviceIdx == nidx) {
					
				if (FlagOn(rmd_tmp.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_SPARE)) {

					if (upToDateNode != 0xff && ((signed _int32)(rmd_tmp.uiUSN - Rmd->uiUSN)) > 0) {
						
						NDAS_BUGON( FALSE );
						status = STATUS_UNSUCCESSFUL;
					}

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Spare disk has newer RMD USN %x but ignore it\n", rmd_tmp.uiUSN) );
					break;
				}

				if (FlagOn(rmd_tmp.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

					if (upToDateNode != 0xff && ((signed _int32)(rmd_tmp.uiUSN - Rmd->uiUSN)) > 0) {

						NDAS_BUGON( FALSE );
						status = STATUS_UNSUCCESSFUL;
					}

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Disk has newer RMD USN %x but ignore it\n", rmd_tmp.uiUSN) );
					break;
				}
			}
		}

		if (status != STATUS_SUCCESS) {

			break;
		}
	
		if (ridx != Lurn->LurnChildrenCnt) {

			nodeIsUptoDate[nidx] = FALSE;
			continue;
		}

		if (upToDateNode != 0xff) {

			if (FlagOn(rmd_tmp.state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {

				if (FlagOn(Rmd->state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {

					//if (rmd_tmp.uiUSN != Rmd->uiUSN) {

					//	NDAS_BUGON( FALSE );
					//	status = STATUS_UNSUCCESSFUL;
					//	break;
					//}
				
				} else { 
				
					if (!(((signed _int32)(rmd_tmp.uiUSN - Rmd->uiUSN)) > 0)) {

						NDAS_BUGON( FALSE );
						status = STATUS_UNSUCCESSFUL;
						break;
					}
				}
			
			} else if (FlagOn(Rmd->state, NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)) {

				if (!(((signed _int32)(Rmd->uiUSN - rmd_tmp.uiUSN)) > 0)) {

					NDAS_BUGON( FALSE );
					status = STATUS_UNSUCCESSFUL;
					break;
				}
			}
		}

		if (upToDateNode == 0xff) { 

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Found newer RMD USN %x from node %d upToDateNode =%d\n", rmd_tmp.uiUSN, nidx, upToDateNode) );

			RtlCopyMemory( Rmd, &rmd_tmp, sizeof(NDAS_RAID_META_DATA) );
			upToDateNode = nidx;

			nodeIsUptoDate[nidx] = TRUE;
		
		} else if (((signed _int32)(rmd_tmp.uiUSN - Rmd->uiUSN)) > 0) {

			UCHAR	nidx2;

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("Found newer RMD USN %x from node %d upToDateNode =%d\n", rmd_tmp.uiUSN, nidx, upToDateNode) );

			RtlCopyMemory( Rmd, &rmd_tmp, sizeof(NDAS_RAID_META_DATA) );
			upToDateNode = nidx;
		
			for (nidx2 = 0; nidx2 < nidx; nidx2++) {

				nodeIsUptoDate[nidx2] = FALSE;
			}

			nodeIsUptoDate[nidx] = TRUE;

		} else if (((signed _int32)(rmd_tmp.uiUSN - Rmd->uiUSN)) == 0) {
		
			nodeIsUptoDate[nidx] = TRUE;
		
		} else {

			nodeIsUptoDate[nidx] = FALSE;
		}
	}

	if (UpToDateNode) {

		*UpToDateNode = upToDateNode;
	}

	if (NodeIsUptoDate) {

		RtlCopyMemory( NodeIsUptoDate, nodeIsUptoDate, sizeof(BOOLEAN)*MAX_NDASR_MEMBER_DISK );
		*UpToDateNode = upToDateNode;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("OUT status = %x, upToDataNode = %x\n", status, upToDateNode) );

	return status;
}

NTSTATUS
NdasRaidLurnExecuteSynchrouslyCompletionRoutine (
	IN PCCB		Ccb,
	IN PKEVENT	Event
	)
{
	UNREFERENCED_PARAMETER( Ccb );

	KeSetEvent( Event, 0, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
NdasRaidLurnUpdateSynchrously (
	IN  PLURELATION_NODE	ChildLurn,
	IN  UINT16				UpdateClass,
	OUT PUCHAR				CcbStatus
	)
{
	NTSTATUS			status;

	CCB					ccb;
	LURN_UPDATE			lurnUpdate;
	KEVENT				completionEvent;

	LARGE_INTEGER		interval;

retry:

	LSCCB_INITIALIZE( &ccb );

	LsCcbSetFlag( &ccb, CCB_FLAG_CALLED_INTERNEL );

	ccb.OperationCode = CCB_OPCODE_UPDATE;
	ccb.DataBuffer = &lurnUpdate;
	ccb.DataBufferLength = sizeof(LURN_UPDATE);		

	RtlZeroMemory( &lurnUpdate, sizeof(LURN_UPDATE) );

	lurnUpdate.UpdateClass = UpdateClass;

	KeInitializeEvent( &completionEvent, SynchronizationEvent, FALSE );

	LsCcbSetCompletionRoutine( &ccb, 
							   NdasRaidLurnExecuteSynchrouslyCompletionRoutine, 
							   &completionEvent );

	status = LurnRequest( ChildLurn, &ccb );
		
	DebugTrace( DBG_LURN_NOISE, ("LurnRequest status : %08x\n", status) );

	if (!NT_SUCCESS(status)) {

		NDAS_BUGON( FALSE );
	}

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &completionEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		NDAS_BUGON( status == STATUS_SUCCESS );
	}

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed : Status %08lx\n", status) );
		status = STATUS_CLUSTER_NODE_UNREACHABLE;

		return status;
	
	} 
	
	if (ccb.CcbStatus == CCB_STATUS_SUCCESS ||
		ccb.OperationCode == CCB_OPCODE_UPDATE && ccb.CcbStatus == CCB_STATUS_NO_ACCESS) {

		if (CcbStatus) {

			*CcbStatus = ccb.CcbStatus;
		}

		return status;
	}

	if (ccb.CcbStatus == CCB_STATUS_BUSY) {
		
		interval.QuadPart = (-5*NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;
	}

	if (ccb.CcbStatus == CCB_STATUS_RESET) {

		NDAS_BUGON( ccb.OperationCode != CCB_OPCODE_RESETBUS );

		interval.QuadPart = (-5*NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;		
	}

	if (ccb.OperationCode == CCB_OPCODE_DEVLOCK && ccb.CcbStatus == CCB_STATUS_LOST_LOCK) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );
		
		status = STATUS_LOCK_NOT_GRANTED;
	
	} else {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );

		if (ccb.CcbStatus != CCB_STATUS_STOP															&&
			!(ccb.OperationCode == CCB_OPCODE_DEVLOCK && ccb.CcbStatus == CCB_STATUS_COMMAND_FAILED)	&&
			ccb.CcbStatus != CCB_STATUS_NOT_EXIST) {
		
			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );
			NDAS_BUGON( FALSE );
		}

		status = STATUS_CLUSTER_NODE_UNREACHABLE;
	}

	if (CcbStatus) {

		*CcbStatus = ccb.CcbStatus;
	}

	return status;
}

NTSTATUS
NdasRaidLurnLockSynchrously (
	IN  PLURELATION_NODE	ChildLurn,
	IN  UINT32				LockId,
	IN  BYTE				LockOpCode,
	OUT PUCHAR				CcbStatus
	)
{
	NTSTATUS				status;

	CCB						ccb;
	LURN_DEVLOCK_CONTROL	lurnDeviceLock;
	KEVENT					completionEvent;

	LARGE_INTEGER			interval;

retry:

	LSCCB_INITIALIZE( &ccb );

	LsCcbSetFlag( &ccb, CCB_FLAG_CALLED_INTERNEL );

	ccb.OperationCode = CCB_OPCODE_DEVLOCK;
	ccb.DataBuffer = &lurnDeviceLock;
	ccb.DataBufferLength = sizeof(LURN_DEVLOCK_CONTROL);		

	ccb.CcbStatus = CCB_STATUS_UNKNOWN_STATUS;

	RtlZeroMemory( &lurnDeviceLock, sizeof(LURN_DEVLOCK_CONTROL) );

	lurnDeviceLock.LockId		= LockId;
	lurnDeviceLock.LockOpCode	= LockOpCode;

	lurnDeviceLock.AdvancedLock;				// advanced GP lock operation. ADV_GPLOCK feature required.
	lurnDeviceLock.AddressRangeValid;
	lurnDeviceLock.RequireLockAcquisition;
	lurnDeviceLock.StartingAddress;
	lurnDeviceLock.EndingAddress;

	//KeQueryTickCount( (PLARGE_INTEGER)&lurnDeviceLock.ContentionTimeOut );
	lurnDeviceLock.ContentionTimeOut *= KeQueryTimeIncrement();
	lurnDeviceLock.ContentionTimeOut += -60 * NANO100_PER_SEC;
	
	lurnDeviceLock.LockData;

	RtlZeroMemory( lurnDeviceLock.LockData, NDSCLOCK_LOCKDATA_LENGTH );

	KeInitializeEvent( &completionEvent, SynchronizationEvent, FALSE );

	LsCcbSetCompletionRoutine( &ccb, 
							   NdasRaidLurnExecuteSynchrouslyCompletionRoutine, 
							   &completionEvent );

	status = LurnRequest( ChildLurn, &ccb );
		
	DebugTrace( DBG_LURN_NOISE, ("LurnRequest status : %08x\n", status) );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &completionEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		NDAS_BUGON( status == STATUS_SUCCESS );
	}

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed : Status %08lx\n", status) );
		status = STATUS_CLUSTER_NODE_UNREACHABLE;

		return status;
	
	} 
	
	if (ccb.CcbStatus == CCB_STATUS_SUCCESS) {

		return status;
	}

	if (ccb.CcbStatus == CCB_STATUS_BUSY) {
		
		interval.QuadPart = (-5*NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;
	}

	if (ccb.CcbStatus == CCB_STATUS_RESET) {

		NDAS_BUGON( ccb.OperationCode != CCB_OPCODE_RESETBUS );

		interval.QuadPart = (-5 * NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;		
	}

	if (ccb.OperationCode == CCB_OPCODE_DEVLOCK && ccb.CcbStatus == CCB_STATUS_LOST_LOCK) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );
		
		status = STATUS_LOCK_NOT_GRANTED;
	
	} else {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );

		if (ccb.CcbStatus != CCB_STATUS_STOP															&&
			!(ccb.OperationCode == CCB_OPCODE_DEVLOCK && ccb.CcbStatus == CCB_STATUS_COMMAND_FAILED)	&&
			ccb.CcbStatus != CCB_STATUS_NOT_EXIST) {
		
			NDAS_BUGON( FALSE );
		}

		status = STATUS_CLUSTER_NODE_UNREACHABLE;
	}

	if (CcbStatus) {

		*CcbStatus = ccb.CcbStatus;
	}

	return status;
}

NTSTATUS
NdasRaidLurnExecuteSynchrously (
	IN PLURELATION_NODE	ChildLurn,
	IN UCHAR			CdbOperationCode,
	IN BOOLEAN			AcquireBufferLock,
	IN BOOLEAN			ForceUnitAccess,
	IN PCHAR			DataBuffer,
	IN UINT64			BlockAddress,		// Child block addr
	IN UINT32			TransferBlocks,		// Child block count
	IN BOOLEAN			RelativeAddress
	)
{
	NTSTATUS		status;

	CCB				ccb;
	CMD_BYTE_OP		ext_cmd;
	KEVENT			completionEvent;

	UINT32			dataBufferLength = TransferBlocks * ChildLurn->BlockBytes; // taken from the first LURN.

	LARGE_INTEGER	interval;
	BOOLEAN			lockAquired = FALSE;

retry:

	NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (CdbOperationCode == SCSIOP_WRITE || CdbOperationCode == SCSIOP_WRITE16) {
			
		if (!FlagOn( ChildLurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
			return STATUS_CLUSTER_NODE_UNREACHABLE;
		}
	}

	DebugTrace( DBG_LURN_NOISE, 
				("Lurn : %p, DataBuffer : %08x\n", ChildLurn, DataBuffer) );

	LSCCB_INITIALIZE( &ccb );

	LsCcbSetFlag( &ccb, CCB_FLAG_CALLED_INTERNEL );

	ccb.OperationCode = CCB_OPCODE_EXECUTE;
	ccb.DataBuffer = DataBuffer;
	ccb.DataBufferLength = dataBufferLength;		

	((PCDB)(ccb.Cdb))->CDB10.OperationCode = CdbOperationCode; // OperationCode is in same place for CDB10 and CDB16
	
	LsCcbSetLogicalAddress( (PCDB)(ccb.Cdb), BlockAddress );
	LsCcbSetTransferLength( (PCDB)(ccb.Cdb), TransferBlocks );

	// Set ccb flags

	status = STATUS_SUCCESS;

	if (AcquireBufferLock) {
			
		if (FlagOn(ChildLurn->Lur->EnabledNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {

			UCHAR				ccbStatus;
			PLURNEXT_IDE_DEVICE ideDisk;

			for (;;) {

				status = NdasRaidLurnLockSynchrously( ChildLurn,
													  LURNDEVLOCK_ID_BUFFLOCK,
													  NDSCLOCK_OPCODE_ACQUIRE,
													  &ccbStatus );

				if (status == STATUS_LOCK_NOT_GRANTED) {

					if (ccbStatus == CCB_STATUS_LOST_LOCK) {

						status = NdasRaidLurnLockSynchrously( ChildLurn,
															  LURNDEVLOCK_ID_BUFFLOCK,
															  NDSCLOCK_OPCODE_RELEASE,
															  &ccbStatus );

						if (status == STATUS_SUCCESS) {

							continue;
						}
					}

					NDAS_BUGON( FALSE );
				}

				if (status != STATUS_SUCCESS) {

					NDAS_BUGON( status == STATUS_CLUSTER_NODE_UNREACHABLE );
					break;
				}
			
				lockAquired = TRUE;

				ideDisk = (PLURNEXT_IDE_DEVICE)ChildLurn->LurnExtension;
				NDAS_BUGON( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == TRUE );

				break;
			}

		} else {

			NDAS_BUGON( ChildLurn->Lur->LockImpossible );
		}
	}

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( status == STATUS_CLUSTER_NODE_UNREACHABLE );
		return STATUS_CLUSTER_NODE_UNREACHABLE;
	}

	if (ForceUnitAccess) {

		if (((PCDB)(ccb.Cdb))->CDB10.OperationCode == SCSIOP_WRITE) {
	
			((PCDB)(ccb.Cdb))->CDB10.ForceUnitAccess = TRUE;

		} else if (((PCDBEXT)(ccb.Cdb))->CDB16.OperationCode == SCSIOP_WRITE16) {

			((PCDBEXT)(ccb.Cdb))->CDB16.ForceUnitAccess = TRUE;
		}
	}

	if (RelativeAddress) {

		RtlZeroMemory( &ext_cmd, sizeof(CMD_BYTE_OP) );

		LsCcbSetFlag( &ccb, CCB_FLAG_BACKWARD_ADDRESS );
	}

	if (ChildLurn->Lur->LurFlags & LURFLAG_WRITE_CHECK_REQUIRED) {
		
		LsCcbSetFlag( &ccb, CCB_FLAG_WRITE_CHECK );

	} else {

		LsCcbResetFlag( &ccb, CCB_FLAG_WRITE_CHECK );
	}

	//LsCcbSetFlag( &ccb, CCB_FLAG_SYNCHRONOUS );

	KeInitializeEvent( &completionEvent, SynchronizationEvent, FALSE );

	LsCcbSetCompletionRoutine( &ccb, 
							   NdasRaidLurnExecuteSynchrouslyCompletionRoutine, 
							   &completionEvent );

	status = LurnRequest( ChildLurn, &ccb );
		
	DebugTrace( DBG_LURN_NOISE, ("LurnRequest status : %08x\n", status) );

	if (!NT_SUCCESS(status)) {

		NDAS_BUGON( FALSE );
	}

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &completionEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		NDAS_BUGON( status == STATUS_SUCCESS );
	}

	if (lockAquired == TRUE) {

		NTSTATUS			status2;
		UCHAR				ccbStatus;
		PLURNEXT_IDE_DEVICE ideDisk;

		status2 = NdasRaidLurnLockSynchrously( ChildLurn,
											   LURNDEVLOCK_ID_BUFFLOCK,
											   NDSCLOCK_OPCODE_RELEASE,
											   &ccbStatus );
	
		if (status2 == STATUS_CLUSTER_NODE_UNREACHABLE) {

			//status = STATUS_CLUSTER_NODE_UNREACHABLE;
		}

		NDAS_BUGON( status2 == STATUS_SUCCESS || status2 == STATUS_CLUSTER_NODE_UNREACHABLE );

		if (status2 == STATUS_SUCCESS) {

			ideDisk = (PLURNEXT_IDE_DEVICE)ChildLurn->LurnExtension;

			NDAS_BUGON( ideDisk->LuHwData.DevLockStatus[LURNDEVLOCK_ID_BUFFLOCK].Acquired == FALSE );
		}
	}

	if (status != STATUS_SUCCESS) {

		NDAS_BUGON( FALSE );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Failed : Status %08lx\n", status) );
		status = STATUS_CLUSTER_NODE_UNREACHABLE;

		return status;
	
	} 
	
	if (ccb.CcbStatus == CCB_STATUS_SUCCESS) {

		return status;
	}

	if (ccb.CcbStatus == CCB_STATUS_BUSY) {
		
		interval.QuadPart = (-5 * NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;
	}

	if (ccb.CcbStatus == CCB_STATUS_RESET) {

		NDAS_BUGON( ccb.OperationCode != CCB_OPCODE_RESETBUS );

		interval.QuadPart = (-5 * NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;		
	}

	if (ccb.CcbStatus != CCB_STATUS_STOP		&&
		ccb.CcbStatus != CCB_STATUS_NOT_EXIST) {
		
		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );
		NDAS_BUGON( FALSE );
	}

	status = STATUS_CLUSTER_NODE_UNREACHABLE;

	return status;
}

ULONG
NdasRaidLurnGetChildBlockBytes (
	PLURELATION_NODE Lurn
	)
{
	ULONG	ndix;
	ULONG	childBlockBytes;

	if (Lurn->LurnChildrenCnt == 0) {
	
		NDAS_BUGON( FALSE );	
		return 0;
	}

	// Verify

	childBlockBytes = 0;

	for (ndix = 0; ndix < Lurn->LurnChildrenCnt; ndix ++) {

		if (!LURN_IS_RUNNING(Lurn->LurnChildren[ndix]->LurnStatus)) {

			continue;
		}

		if (childBlockBytes == 0) {

			childBlockBytes = Lurn->LurnChildren[ndix]->BlockBytes;
		}

		if (childBlockBytes != Lurn->LurnChildren[ndix]->BlockBytes) {

			NDAS_BUGON( FALSE );
			return 0;
		}
	}

	return childBlockBytes;
}

NTSTATUS
NdasRaidLurnGetChildMinTransferLength (
	IN	PLURELATION_NODE	Lurn,
	OUT PUINT32				ChildMinDataSendLength,
	OUT PUINT32				ChildMinDataRecvLength
	)
{
	ULONG	ndix;
	UINT32	childMinDataSendLength;
	UINT32	childMinDataRecvLength;

	if (Lurn->LurnChildrenCnt == 0) {
	
		NDAS_BUGON( FALSE );	
		return STATUS_UNSUCCESSFUL;
	}

	// Verify

	childMinDataSendLength = (UINT32)(-1);
	childMinDataRecvLength = (UINT32)(-1);

	for (ndix = 0; ndix < Lurn->LurnChildrenCnt; ndix ++) {

		if (!LURN_IS_RUNNING(Lurn->LurnChildren[ndix]->LurnStatus)) {

			continue;
		}

		if (childMinDataSendLength > Lurn->LurnChildren[ndix]->MaxDataSendLength) {

			childMinDataSendLength = Lurn->LurnChildren[ndix]->MaxDataSendLength;
		}

		if (childMinDataRecvLength > Lurn->LurnChildren[ndix]->MaxDataRecvLength) {

			childMinDataRecvLength = Lurn->LurnChildren[ndix]->MaxDataRecvLength;
		}
	}

	if (childMinDataSendLength < 32*1024 || childMinDataSendLength > 64*1024) {

		NDAS_BUGON( FALSE );
		return STATUS_UNSUCCESSFUL;
	}

	if (childMinDataRecvLength < 32*1024 || childMinDataRecvLength > 64*1024) {

		NDAS_BUGON( FALSE );
		return STATUS_UNSUCCESSFUL;
	}

	*ChildMinDataSendLength = childMinDataSendLength;
	*ChildMinDataRecvLength = childMinDataRecvLength;

	return STATUS_SUCCESS;
}


UINT32 
NdasRaidGetOverlappedRange (
	UINT64	Start1, 
	UINT32	Length1,
	UINT64	Start2, 
	UINT32	Length2,
	UINT64	*OverlapStart, 
	UINT32	*OverlapLength
	) 
{
	UINT32	result;

	UINT64	overlapStart;
	UINT32	overlapLength;

	
	if (Start2 <= Start1) {

		if (Start2 + Length2 <= Start1) {

			result = NDASR_RANGE_NO_OVERLAP;
		
		} else {

			if (Start2 + Length2 <= Start1 + Length1) {

				overlapStart = Start1;
				overlapLength = (UINT32)(Start2 + Length2 - Start1);

				result = NDASR_RANGE_SRC1_HEAD_OVERLAP;
			
			} else {

				overlapStart = Start1;
				overlapLength = Length1;

				result = NDASR_RANGE_SRC2_CONTAINS_SRC1;
			}
		}
	
	} else {  // Start1 < Start2 

		if (Start1 + Length1 <= Start2) {

			result = NDASR_RANGE_NO_OVERLAP;
		
		} else {

			if (Start1 + Length1 <= Start2 + Length2) {

				overlapStart = Start2;
				overlapLength = (UINT32)(Start1 + Length1 - Start2);

				result = NDASR_RANGE_SRC1_TAIL_OVERLAP;
			
			} else {

				overlapStart = Start2;
				overlapLength = Length2;

				result = NDASR_RANGE_SRC1_CONTAINS_SRC2;
			}
		}
	}

	if (OverlapStart) {

		*OverlapStart = overlapStart;
	}

	if (OverlapLength) {

		*OverlapLength = overlapLength;
	}

	return result;
}

UCHAR 
NdasRaidLurnStatusToNodeFlag (
	UINT32 LurnStatus
	)
{
	UCHAR flags;

	switch (LurnStatus) {

	case LURN_STATUS_RUNNING:
	case LURN_STATUS_STALL:
		
		flags = NRMX_NODE_FLAG_RUNNING;
		break;
	
	case LURN_STATUS_STOP_PENDING:
	case LURN_STATUS_STOP:
	case LURN_STATUS_DESTROYING:
	case LURN_STATUS_INIT:
	
		flags = NRMX_NODE_FLAG_STOP;
		break;
	
	default:
		
		flags = NRMX_NODE_FLAG_UNKNOWN;
		break;
	}

	return flags;
}

NTSTATUS
NdasRaidLurnCreate (
	IN PLURELATION_NODE			Lurn,
	IN PLURELATION_NODE_DESC	LurnDesc
	) 
{
	NTSTATUS	status;
	PNDASR_INFO	ndasrInfo;

	UCHAR		nidx, ridx;
	UINT64		accumulateBlockSize;

	UINT32		ChildMinDataSendLength;
	UINT32		ChildMinDataRecvLength;

	UCHAR		runningChildCount;


	NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_BUGON( Lurn->NdasrInfo == NULL );

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasRaidLurnCreate, Lurn->LurnType = %x\n", Lurn->LurnType) ); 

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->LurnType = %d\n", LurnDesc->LurnType) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->LurnId = %d\n", LurnDesc->LurnId) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->StartBlockAddr = 0x%I64x\n", LurnDesc->StartBlockAddr) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->EndBlockAddr = 0x%I64x\n", LurnDesc->EndBlockAddr) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->UnitBlocks = 0x%I64x\n", LurnDesc->UnitBlocks) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->MaxDataSendLength = %d bytes\n", LurnDesc->MaxDataSendLength) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->MaxDataRecvLength = %d bytes\n", LurnDesc->MaxDataRecvLength) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->LurnOptions = %d\n", LurnDesc->LurnOptions) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->LurnParent = %d\n", LurnDesc->LurnParent) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->LurnChildrenCnt = %d\n", LurnDesc->LurnChildrenCnt) );
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("LurnDesc->LurnChildren = 0x%p\n", LurnDesc->LurnChildren) );

	// sanity check

	if (Lurn->LurnType != LURN_NDAS_RAID0 && Lurn->LurnType != LURN_NDAS_AGGREGATION) {

		if (LurnDesc->LurnInfoRAID.BlocksPerBit == 0) {

			NDAS_BUGON( FALSE );
			return STATUS_INVALID_PARAMETER;
		}
	}

	// Determine block bytes.

	Lurn->ChildBlockBytes = NdasRaidLurnGetChildBlockBytes( Lurn );

	if (Lurn->ChildBlockBytes == 0) {

		NDAS_BUGON( FALSE );

		return STATUS_DEVICE_NOT_READY;
	}

	NDAS_BUGON( Lurn->ChildBlockBytes == 0x200 );

	status = NdasRaidLurnGetChildMinTransferLength( Lurn,
													&ChildMinDataSendLength,
													&ChildMinDataRecvLength );

	NDAS_BUGON( Lurn->ChildBlockBytes == SECTOR_SIZE );

	ndasrInfo = ExAllocatePoolWithTag( NonPagedPool, sizeof(NDASR_INFO), NDASR_INFO_POOL_TAG );

	if (ndasrInfo == NULL) {

		NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	do {

		RtlZeroMemory( ndasrInfo, sizeof(NDASR_INFO) );

		KeInitializeSpinLock( &ndasrInfo->SpinLock );
		ExInitializeResourceLite( &ndasrInfo->BufLockResource );

		//ndasrInfo->MaxDataSendLength = LurnDesc->MaxDataSendLength;
		//ndasrInfo->MaxDataRecvLength = LurnDesc->MaxDataRecvLength;

		status = STATUS_SUCCESS;

		switch (Lurn->LurnType) {

		case LURN_NDAS_AGGREGATION:
			
			ndasrInfo->Striping = FALSE;

			ndasrInfo->ParityDiskCount = 0;	
			ndasrInfo->DistributedParity = FALSE;

			break;

		case LURN_NDAS_RAID0:

			ndasrInfo->Striping = TRUE;

			ndasrInfo->ParityDiskCount = 0;	
			ndasrInfo->DistributedParity = FALSE;

			break;

		case LURN_NDAS_RAID1:

			ndasrInfo->Striping = TRUE;

			ndasrInfo->ParityDiskCount = 1;	
			ndasrInfo->DistributedParity = FALSE;

			break;

		case LURN_NDAS_RAID4:

			if (!LUR_IS_READONLY(Lurn->Lur)) {

				NDAS_BUGON( FlagOn(Lurn->Lur->EnabledNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE) );
			}

			ndasrInfo->Striping = TRUE;

			ndasrInfo->ParityDiskCount = 1;	
			ndasrInfo->DistributedParity = FALSE;

			break;

		case LURN_NDAS_RAID5:

			if (!LUR_IS_READONLY(Lurn->Lur)) {

				NDAS_BUGON( FlagOn(Lurn->Lur->EnabledNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE) );
			}

			ndasrInfo->Striping = TRUE;

			ndasrInfo->ParityDiskCount = 1;	
			ndasrInfo->DistributedParity = TRUE;

			break;

		default:

			status = STATUS_INVALID_PARAMETER;
			break;
		}

		if (status != STATUS_SUCCESS) {

			break;
		}

		status = STATUS_SUCCESS;
		accumulateBlockSize = 0;

		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx ++) {

			if (ndasrInfo->Striping == TRUE) {

				if (Lurn->UnitBlocks != Lurn->LurnChildren[nidx]->UnitBlocks) {
					
					NDAS_BUGON( FALSE );
					status = STATUS_INVALID_PARAMETER;
					break;
				}
			}

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
						("Lurn->LurnChildren[nidx]->UnitBlocks = %I64d\n", Lurn->LurnChildren[nidx]->UnitBlocks) );

			accumulateBlockSize += Lurn->LurnChildren[nidx]->UnitBlocks;
		}

		if (status != STATUS_SUCCESS) {

			break;
		}
		
		if (ndasrInfo->Striping == FALSE) {

			if (accumulateBlockSize != Lurn->UnitBlocks) {

				NDAS_BUGON( FALSE );
				status = STATUS_INVALID_PARAMETER;
				break;
			}
		}

		if (ndasrInfo->ParityDiskCount == 0) {

			ndasrInfo->ActiveDiskCount	= Lurn->LurnChildrenCnt;
			ndasrInfo->SpareDiskCount	= 0;

		} else {

			ndasrInfo->ActiveDiskCount	= (UCHAR)(Lurn->LurnChildrenCnt - LurnDesc->LurnInfoRAID.SpareDiskCount);
			ndasrInfo->SpareDiskCount	= (UCHAR)LurnDesc->LurnInfoRAID.SpareDiskCount;

			ndasrInfo->BlocksPerBit		= LurnDesc->LurnInfoRAID.BlocksPerBit;
		}

		if (ndasrInfo->Striping == FALSE) {

			ndasrInfo->MaxDataSendLength = ChildMinDataSendLength;
			ndasrInfo->MaxDataRecvLength = ChildMinDataRecvLength;

			Lurn->MaxDataSendLength = ndasrInfo->MaxDataSendLength;
			Lurn->MaxDataRecvLength = ndasrInfo->MaxDataRecvLength;
		
		} else {

			ndasrInfo->MaxDataSendLength = ChildMinDataSendLength * (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount);
			ndasrInfo->MaxDataRecvLength = ChildMinDataRecvLength * (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount);

			Lurn->MaxDataSendLength = ndasrInfo->MaxDataSendLength;
			Lurn->MaxDataRecvLength = ndasrInfo->MaxDataRecvLength;
		}

		ndasrInfo->MaxDataSendLength = NDAS_MAX_TRANSFER_LENGTH;
		ndasrInfo->MaxDataRecvLength = NDAS_MAX_TRANSFER_LENGTH;

		Lurn->MaxDataSendLength = ndasrInfo->MaxDataSendLength;
		Lurn->MaxDataRecvLength = ndasrInfo->MaxDataRecvLength;

		ndasrInfo->NdasRaidId	= LurnDesc->LurnInfoRAID.NdasRaidId;
		ndasrInfo->ConfigSetId	= LurnDesc->LurnInfoRAID.ConfigSetId;

		NDAS_BUGON( ndasrInfo->MaxDataSendLength == ndasrInfo->MaxDataRecvLength );
		
		if (ndasrInfo->Striping) {

			NDAS_BUGON( ndasrInfo->MaxDataSendLength / Lurn->ChildBlockBytes % (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) == 0 );
			NDAS_BUGON( ndasrInfo->MaxDataRecvLength / Lurn->ChildBlockBytes % (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) == 0 );
			
			NDAS_BUGON( (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) == 1 ||
						 (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) == 2 ||
						 (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) == 4 ||
						 (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) == 8 );
		}

		NDAS_BUGON( ndasrInfo->SpareDiskCount == 0 ||  ndasrInfo->SpareDiskCount == 1 );

		runningChildCount = 0;
	
		for (nidx = 0; nidx < Lurn->LurnChildrenCnt; nidx++) {

			ndasrInfo->LocalNodeFlags[nidx]	  = NdasRaidLurnStatusToNodeFlag(Lurn->LurnChildren[nidx]->LurnStatus);
			ndasrInfo->LocalNodeChanged[nidx] = 0;

			if (ndasrInfo->LocalNodeFlags[nidx] == NRMX_NODE_FLAG_RUNNING) {

				runningChildCount ++;
			}
		}

		if (runningChildCount < ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount) {

			NDAS_BUGON( FALSE );
			
			status = STATUS_DEVICE_NOT_READY;
			break;
		}

		// Set Lurn

		Lurn->NdasrInfo = ndasrInfo;

		if (ndasrInfo->Striping == FALSE) {

			Lurn->BlockBytes = Lurn->ChildBlockBytes;
		
		} else {

			Lurn->BlockBytes = Lurn->ChildBlockBytes * (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount);
		}

#if __NDAS_SCSI_VARIABLE_BLOCK_SIZE_TEST__
		if (ndasrInfo->Striping == FALSE) {
			
			Lurn->UnitBlocks /= __TEST_BLOCK_SIZE__;
			Lurn->BlockBytes *= __TEST_BLOCK_SIZE__;
		}
#endif

		Lurn->Lur->BlockBytes = Lurn->BlockBytes;

		if (ndasrInfo->Striping) {

			Lurn->DataUnitBlocks = Lurn->UnitBlocks - Lurn->Lur->StartOffset;

		} else {

			Lurn->DataUnitBlocks 
				= Lurn->UnitBlocks - Lurn->Lur->StartOffset * (ndasrInfo->ActiveDiskCount - ndasrInfo->ParityDiskCount);
		}

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
					("Lurn->UnitBlocks = %I64d, Lurn->DataUnitBlocks = %I64d, ndasrInfo->MaxDataSendLength = %x, Lurn->ChildBlockBytes = %x, Lurn->BlockBytes = %x\n", 
					 Lurn->UnitBlocks, Lurn->DataUnitBlocks, ndasrInfo->MaxDataSendLength, Lurn->ChildBlockBytes, Lurn->BlockBytes) );

		// Read Rmd

		if (ndasrInfo->ParityDiskCount) {

			status = NdasRaidLurnReadRmd( Lurn, &ndasrInfo->Rmd, ndasrInfo->NodeIsUptoDate, &ndasrInfo->UpToDateNode );

			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( NDAS_BUGON_NETWORK_FAIL );
				break;
			}

			for (ridx = 0; ridx < Lurn->LurnChildrenCnt; ridx++) { // i : role index. 
		
				UCHAR unitDeviceStatus = ndasrInfo->Rmd.UnitMetaData[ridx].UnitDeviceStatus;
		
				if (Lurn->Lur->EmergencyMode == FALSE) {

					if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

						if (FlagOn(ndasrInfo->LocalNodeFlags[(UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx], NRMX_NODE_FLAG_RUNNING)) {

							//NDAS_BUGON( FALSE );
	
							LurnSendStopCcb( Lurn->LurnChildren[ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx] );
							LurnClose( Lurn->LurnChildren[ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx] );
						}

						if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)  {

							Lurn->LurnChildren[ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx]->FaultInfo.FaultCause |= 
								LURN_FCAUSE_BAD_SECTOR;
						}

						if (unitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK)  {

							Lurn->LurnChildren[ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx]->FaultInfo.FaultCause |= 
								LURN_FCAUSE_BAD_DISK;
						}

						ndasrInfo->LocalNodeFlags[ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx] = 
								NdasRaidLurnStatusToNodeFlag( Lurn->LurnChildren[ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx]->LurnStatus );

						NDAS_BUGON( FlagOn(ndasrInfo->LocalNodeFlags[(UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx], 
											NRMX_NODE_FLAG_STOP) );

						SetFlag( ndasrInfo->LocalNodeFlags[(UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx], 
								 NRMX_NODE_FLAG_DEFECTIVE );

						continue;
					}
				}

				if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_OFFLINE)) {

					NDAS_BUGON( FlagOn(ndasrInfo->LocalNodeFlags[(UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx], 
										NRMX_NODE_FLAG_STOP) );
	
					SetFlag( ndasrInfo->LocalNodeFlags[(UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx], 
							 NRMX_NODE_FLAG_OFFLINE );
					
					DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO,
								("Offlien Node %d is enabled\n", (UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx) );
				
					continue;
				}

				if (!FlagOn(ndasrInfo->LocalNodeFlags[(UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx], NRMX_NODE_FLAG_RUNNING)) {

					DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR,
								("WANNING !!!! normal Node %d is not enabled\n", (UCHAR)ndasrInfo->Rmd.UnitMetaData[ridx].iUnitDeviceIdx) );

					NDAS_BUGON( FALSE );

					status = STATUS_CLUSTER_NODE_UNREACHABLE;
					break;
				}
			}

			if (status != STATUS_SUCCESS) {

				break;
			}
		}

		NdasrLocalChannelInitialize( &ndasrInfo->RequestChannel );
		NdasrLocalChannelInitialize( &ndasrInfo->NotitifyChannel );

		status = NdasRaidClientStart( Lurn ); 
	
		if (status != STATUS_SUCCESS) {

			break;
		}

	} while (0);
	
	if (status != STATUS_SUCCESS) {

		if (ndasrInfo) {

			ExDeleteResourceLite( &ndasrInfo->BufLockResource );

			ExFreePoolWithTag( ndasrInfo, NDASR_INFO_POOL_TAG );
			Lurn->NdasrInfo = NULL;
		}
	}

	NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );	
	return status;
}


VOID
NdasRaidLurnClose (
	IN PLURELATION_NODE Lurn
	) 
{
	NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (Lurn->NdasrInfo == NULL) {

		NDAS_BUGON( Lurn->LurnStatus == LURN_STATUS_INIT );
		return;
	}

	ExDeleteResourceLite( &Lurn->NdasrInfo->BufLockResource );

	ExFreePoolWithTag( Lurn->NdasrInfo, NDASR_INFO_POOL_TAG );

	Lurn->NdasrInfo = NULL;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("NdasRaidLurnClose\n") ); 

	return;
}

VOID
NdasRaidLurnStop (
	PLURELATION_NODE Lurn
	)
{
	UNREFERENCED_PARAMETER( Lurn );

	NDAS_BUGON( FALSE );
	return;
}

NTSTATUS
NdasRaidLurnRequest (
	IN	PLURELATION_NODE	Lurn,
	IN	PCCB				Ccb
	)
{
	NTSTATUS		status;
	KIRQL			oldIrql;

	if (Lurn->NdasrInfo == NULL || Lurn->NdasrInfo->NdasrClient == NULL) {

		NDAS_BUGON( Lurn->LurnStatus == LURN_STATUS_INIT );

		LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		LsCcbCompleteCcb( Ccb );

		return STATUS_SUCCESS;
	}

	if (Ccb->OperationCode == CCB_OPCODE_SHUTDOWN) {

		PLURN_NDASR_SHUT_DOWN_PARAM param;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("CCB_OPCODE_SHUTDOWN\n") );

		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

		if (FlagOn(Lurn->LurnStatus, (LURN_STATUS_STOP_PENDING | LURN_STATUS_STOP))) {

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );		
		
			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;

		}

		if (FlagOn(Lurn->LurnStatus, (LURN_STATUS_SHUTDOWN_PENDING | LURN_STATUS_SHUTDOWN))) {

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );		

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;

		}
	
		SetFlag( Lurn->LurnStatus, LURN_STATUS_SHUTDOWN_PENDING );
		
		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

		// This code may be running at DPC level.
		// Run stop operation asynchrously.
		// Alloc work item and call LurnRAID1ShutDownWorker

		param = ExAllocatePoolWithTag( NonPagedPool, sizeof(LURN_NDASR_SHUT_DOWN_PARAM), NDASR_SHUTDOWN_POOL_TAG );
		
		if (param == NULL) {

			NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;
		}

		NDAS_BUGON( Lurn && Lurn->Lur && Lurn->Lur->AdapterFunctionDeviceObject );
		
		param->IoWorkItem = IoAllocateWorkItem( Lurn->Lur->AdapterFunctionDeviceObject );

		if (!param->IoWorkItem) {

			NDAS_BUGON( FALSE );
				
			ExFreePoolWithTag( param, NDASR_SHUTDOWN_POOL_TAG );

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;
		}

		param->Lurn = Lurn;
		param->Ccb = Ccb;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Queuing shutdown work to IoWorkItem\n") );
			
		IoQueueWorkItem( param->IoWorkItem, NdasRaidLurnShutDownWorker, DelayedWorkQueue, param );
	
		return STATUS_SUCCESS;
	
	}
	
	if (Ccb->OperationCode == CCB_OPCODE_STOP) {

		NDAS_BUGON( KeGetCurrentIrql() ==  PASSIVE_LEVEL );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("CCB_OPCODE_STOP\n") );

		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );
		
		if (FlagOn(Lurn->LurnStatus, (LURN_STATUS_STOP_PENDING | LURN_STATUS_STOP))) {

			NDAS_BUGON( FALSE );

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );		

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;	
		}
	
		SetFlag( Lurn->LurnStatus, LURN_STATUS_STOP_PENDING );

		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

		NdasRaidClientStop( Lurn );

		if (Lurn->NdasrInfo->NdasrArbitrator) {

			NdasRaidArbitratorStop( Lurn );
		}

		//	Check to see if the CCB is coming for only this LURN.
		
		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
		
			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;
		}

		status = NdasRaidLurnSendStopCcbToChildrenSyncronously( Lurn, Ccb );

		NDAS_BUGON( status == STATUS_SUCCESS );

		SetFlag( Lurn->LurnStatus, LURN_STATUS_STOP );

		return status;

	}

	ACQUIRE_SPIN_LOCK( &Lurn->NdasrInfo->SpinLock, &oldIrql );
	ACQUIRE_DPC_SPIN_LOCK( &Lurn->NdasrInfo->NdasrClient->SpinLock );

	if (!LURN_IS_RUNNING(Lurn->LurnStatus)) {

		NDAS_BUGON( FALSE );
	}

	if (Lurn->NdasrInfo->NdasrClient->NdasrState == NRMX_RAID_STATE_TERMINATED) {

		NDAS_BUGON( FALSE );
	}

	InsertTailList( &Lurn->NdasrInfo->NdasrClient->CcbQueue, &Ccb->ListEntry );
	KeSetEvent( &Lurn->NdasrInfo->NdasrClient->ThreadEvent, IO_NO_INCREMENT, FALSE );

	LsCcbMarkCcbAsPending( Ccb );

	RELEASE_DPC_SPIN_LOCK( &Lurn->NdasrInfo->NdasrClient->SpinLock );
	RELEASE_SPIN_LOCK( &Lurn->NdasrInfo->SpinLock, oldIrql );

	return STATUS_PENDING;	
}

VOID
NdasRaidLurnShutDownWorker (
	IN PDEVICE_OBJECT				DeviceObject,
	IN PLURN_NDASR_SHUT_DOWN_PARAM	Parameter
	) 
{
	UNREFERENCED_PARAMETER( DeviceObject );

	// Is it possible that LURN is destroyed already?
	
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_TRACE, ("Shutdowning DRAID\n") );
	
	NdasRaidClientShutdown( Parameter->Lurn );

	if (Parameter->Lurn->NdasrInfo->NdasrArbitrator) {

		NdasRaidArbitratorShutdown( Parameter->Lurn );
	}

	SetFlag( Parameter->Lurn->LurnStatus, LURN_STATUS_SHUTDOWN );

	// Don't need to pass to child. lslurnide does nothing about CCB_OPCODE_SHUTDOWN 
	// Or we can set synchronous flag to ccb
	// status = LurnAssocSendCcbToAllChildren(Params->Lurn, Params->Ccb, LurnRAID1RCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);

	LsCcbSetStatus( Parameter->Ccb, CCB_STATUS_SUCCESS );
	LsCcbCompleteCcb( Parameter->Ccb );
	
	IoFreeWorkItem( Parameter->IoWorkItem );

	ExFreePoolWithTag( Parameter, NDASR_SHUTDOWN_POOL_TAG );
}
