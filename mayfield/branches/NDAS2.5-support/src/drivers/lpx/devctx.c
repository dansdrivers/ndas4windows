/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop

LONG	DebugLevel = 2;

PCONTROL_CONTEXT  LpxControlDeviceContext;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,LpxCreateDeviceContext)
#endif


VOID
LpxRefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine increments the reference count on a device context.

Arguments:

    DeviceContext - Pointer to a transport device context object.

Return Value:

    none.

--*/

{
    IF_LPXDBG (LPX_DEBUG_DEVCTX) {
        LpxPrint0 ("LpxRefDeviceContext:  Entered.\n");
    }

    ASSERT (DeviceContext->ReferenceCount >= 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&DeviceContext->ReferenceCount);

} /* LpxRefDeviceContext */


VOID
LpxDerefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine dereferences a device context by decrementing the
    reference count contained in the structure.  Currently, we don't
    do anything special when the reference count drops to zero, but
    we could dynamically unload stuff then.

Arguments:

    DeviceContext - Pointer to a transport device context object.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_DEVCTX) {
        LpxPrint0 ("LpxDerefDeviceContext:  Entered.\n");
    }

    result = InterlockedDecrement (&DeviceContext->ReferenceCount);

    ASSERT (result >= 0);

    if (result == 0) {
        LpxDestroyDeviceContext (DeviceContext);
    }

} /* LpxDerefDeviceContext */


VOID
LpxRefControlContext(
    IN PCONTROL_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine increments the reference count on a device context.

Arguments:

    DeviceContext - Pointer to a transport device context object.

Return Value:

    none.

--*/

{
    IF_LPXDBG (LPX_DEBUG_DEVCTX) {
        LpxPrint0 ("LpxRefDeviceContext:  Entered.\n");
    }

    ASSERT (DeviceContext->ReferenceCount >= 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&DeviceContext->ReferenceCount);

} /* LpxRefDeviceContext */


VOID
LpxDerefControlContext(
    IN PCONTROL_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine dereferences a device context by decrementing the
    reference count contained in the structure.  Currently, we don't
    do anything special when the reference count drops to zero, but
    we could dynamically unload stuff then.

Arguments:

    DeviceContext - Pointer to a transport device context object.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_DEVCTX) {
        LpxPrint0 ("LpxDerefDeviceContext:  Entered.\n");
    }

    result = InterlockedDecrement (&DeviceContext->ReferenceCount);

    ASSERT (result >= 0);

    if (result == 0) {
        LpxDestroyControlContext (DeviceContext);
    }

} /* LpxDerefDeviceContext */



NTSTATUS
LpxCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING DeviceName,
    IN OUT PDEVICE_CONTEXT *DeviceContext
    )

/*++

Routine Description:

    This routine creates and initializes a device context structure.

Arguments:


    DriverObject - pointer to the IO subsystem supplied driver object.

    DeviceContext - Pointer to a pointer to a transport device context object.

    DeviceName - pointer to the name of the device this device object points to.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INSUFFICIENT_RESOURCES otherwise.

--*/

{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_CONTEXT deviceContext;
    USHORT i;
	ULONG LookasideMemsizes[] = LOOK_ASIDE_PACKET_MEM_SIZES; 

    //
    // Create the device object for NETBEUI.
    //

    status = IoCreateDevice(
                 DriverObject,
                 sizeof (DEVICE_CONTEXT) - sizeof (DEVICE_OBJECT) +
                     (DeviceName->Length + sizeof(UNICODE_NULL)),
                 DeviceName,
                 FILE_DEVICE_TRANSPORT,
                 FILE_DEVICE_SECURE_OPEN,
                 FALSE,
                 &deviceObject);

    if (!NT_SUCCESS(status)) {
        return status;
    }


    deviceObject->Flags |= DO_DIRECT_IO;

    deviceContext = (PDEVICE_CONTEXT)deviceObject;
    deviceContext->bDeviceInit = FALSE;

    //
    // Initialize our part of the device context.
    //

    RtlZeroMemory(
        ((PUCHAR)deviceContext) + sizeof(DEVICE_OBJECT),
        sizeof(DEVICE_CONTEXT) - sizeof(DEVICE_OBJECT));

    deviceContext->StatusClosingQueueItem = IoAllocateWorkItem(deviceObject);
    if (deviceContext->StatusClosingQueueItem == NULL) {
		IoDeleteDevice (deviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Copy over the device name.
    //

    deviceContext->DeviceNameLength = DeviceName->Length + sizeof(WCHAR);
    deviceContext->DeviceName = (PWCHAR)(deviceContext+1);
    RtlCopyMemory(
        deviceContext->DeviceName,
        DeviceName->Buffer,
        DeviceName->Length);
    deviceContext->DeviceName[DeviceName->Length/sizeof(WCHAR)] = UNICODE_NULL;

    //
    // Initialize the reference count.
    //

    InterlockedExchange(&deviceContext->ReferenceCount, 1);

#if DBG
    {
        UINT Counter;
        for (Counter = 0; Counter < NUMBER_OF_DCREFS; Counter++) {
            deviceContext->RefTypes[Counter] = 0;
        }

        // This reference is removed by the caller.

        deviceContext->RefTypes[DCREF_CREATION] = 1;
    }
#endif

    deviceContext->CreateRefRemoved = FALSE;

    //
    // initialize the various fields in the device context
    //

    InitializeListHead(&deviceContext->DeviceListLinkage);

    KeInitializeSpinLock (&deviceContext->SpinLock);

    InitializeListHead (&deviceContext->AddressPool);
    InitializeListHead (&deviceContext->AddressFilePool);
    InitializeListHead (&deviceContext->AddressDatabase);

    //
    // Initialize In Progress Packet List.
    //

    InitializeListHead(&deviceContext->PacketInProgressList);
    KeInitializeSpinLock(&deviceContext->PacketInProgressQSpinLock);


	for(i=0;i<NUMBER_OF_PACKET_MEM_LOOKASIDE_LIST;i++) {
		NdisInitializeNPagedLookasideList(
			&deviceContext->PacketMemLookAsideList[i],
			NULL,
			NULL,
			0, 
			LookasideMemsizes[i],	
			'PxpL',
			0
		);
	}

    //
    // Initialize provider statistics.
    //

    deviceContext->Statistics.Version = 0x0100;
    deviceContext->State = DEVICECONTEXT_STATE_OPENING;
  
    //
    // Initialize the resource that guards address ACLs.
    //

    ExInitializeResource (&deviceContext->AddressResource);

    //
    // set the multicast address for this network type
    //

    for (i=0; i<HARDWARE_ADDRESS_LENGTH; i++) {
        deviceContext->LocalAddress.Address [i] = 0; // set later
    }

     deviceContext->Type = LPX_DEVICE_CONTEXT_SIGNATURE;
     deviceContext->Size = sizeof (DEVICE_CONTEXT);

    *DeviceContext = deviceContext;
    return STATUS_SUCCESS;
}


VOID
LpxDestroyDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine destroys a device context structure.

Arguments:

    DeviceContext - Pointer to a pointer to a transport device context object.

Return Value:

    None.

--*/

{
 	UINT32 i;
 	
    ACQUIRE_DEVICES_LIST_LOCK();

    // Is ref count zero - or did a new rebind happen now
    // See rebind happen in LpxReInitializeDeviceContext
    if (DeviceContext->ReferenceCount != 0)
    {
        // A rebind happened while we waited for the lock
        RELEASE_DEVICES_LIST_LOCK();
        return;
    }
	DebugPrint(3,("CALL DESTROY DEVICE_CONTEXT %p !!!!!!!!\n",DeviceContext));
    // Splice this adapter of the list of device contexts
    RemoveEntryList (&DeviceContext->DeviceListLinkage);
    
    RELEASE_DEVICES_LIST_LOCK();

    // Mark the adapter as going away to prevent activity
    DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

     // Free the packet pools, etc. and close the adapter.
     LpxCloseNdis (DeviceContext);
    
    // Remove all the storage associated with the device.
    LpxFreeResources (DeviceContext);

    // Cleanup any kernel resources
    ExDeleteResource (&DeviceContext->AddressResource);

    IoFreeWorkItem(DeviceContext->StatusClosingQueueItem);

	for(i=0;i<NUMBER_OF_PACKET_MEM_LOOKASIDE_LIST;i++) {
		NdisDeleteNPagedLookasideList(
			&DeviceContext->PacketMemLookAsideList[i]
		);
	}

    // Delete device from IO space
    IoDeleteDevice ((PDEVICE_OBJECT)DeviceContext);
        
    return;
}

VOID
LpxDestroyControlContext(
    IN PCONTROL_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine destroys a device context structure.

Arguments:

    DeviceContext - Pointer to a pointer to a transport device context object.

Return Value:

    None.

--*/

{
//    PLIST_ENTRY p;
//    PTP_CONNECTION connection;
    
    // Is ref count zero - or did a new rebind happen now
    // See rebind happen in LpxReInitializeDeviceContext
    if (DeviceContext->ReferenceCount != 0)
    {
        // A rebind happened while we waited for the lock
        return;
    }

    // Mark the adapter as going away to prevent activity
    DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

#if 0    
    // Remove all the storage associated with the device.
    //
    // Clean up connection pool.
    //

    while ( !IsListEmpty (&DeviceContext->ConnectionPool) ) {
        p  = RemoveHeadList (&DeviceContext->ConnectionPool);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

        LpxDeallocateConnection (DeviceContext, connection);
    }
#endif

    // Delete device from IO space
    IoDeleteDevice ((PDEVICE_OBJECT)DeviceContext);
        
    return;
}

VOID
LpxCreateControlDevice(
                IN PDRIVER_OBJECT	DriverObject
                ) 
{
    UNICODE_STRING  lpxDeviceName;
    ULONG i;
    PCONTROL_CONTEXT deviceContext;
//     PTP_CONNECTION Connection;
     NTSTATUS status;
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceString;
    KIRQL  irql;
    
    DebugPrint(2, ("LpxCreateControlDevice %p\n", LpxControlDeviceContext));

    //
    //	we create only one socket device context.
    //
    if(LpxControlDeviceContext != NULL)
        return;

    RtlInitUnicodeString(&lpxDeviceName, SOCKETLPX_DEVICE_NAME);

    status = IoCreateDevice(
                 DriverObject,
                 sizeof (CONTROL_CONTEXT) - sizeof (DEVICE_OBJECT) +
                     (lpxDeviceName.Length + sizeof(UNICODE_NULL)),
                 &lpxDeviceName,
                 FILE_DEVICE_TRANSPORT,
                 FILE_DEVICE_SECURE_OPEN,
                 FALSE,
                 &DeviceObject);

    if (!NT_SUCCESS(status)) {
        return;
    }
	
    DeviceObject->Flags |= DO_DIRECT_IO;

    deviceContext = (PCONTROL_CONTEXT)DeviceObject;
    deviceContext->bDeviceInit = FALSE;
    //
    // Initialize our part of the device context.
    //

    RtlZeroMemory(
        ((PUCHAR)deviceContext) + sizeof(DEVICE_OBJECT),
        sizeof(CONTROL_CONTEXT) - sizeof(DEVICE_OBJECT));

    //
    // Copy over the device name.
    //

    deviceContext->DeviceNameLength = lpxDeviceName.Length + sizeof(WCHAR);
    deviceContext->DeviceName = (PWCHAR)(deviceContext+1);
    RtlCopyMemory(
        deviceContext->DeviceName,
        lpxDeviceName.Buffer,
        lpxDeviceName.Length);
    deviceContext->DeviceName[lpxDeviceName.Length/sizeof(WCHAR)] = UNICODE_NULL;

    //
    // Initialize the reference count.
    //

    InterlockedExchange(&deviceContext->ReferenceCount, 1);

#if DBG
    {
        UINT Counter;
        for (Counter = 0; Counter < NUMBER_OF_DCREFS; Counter++) {
            deviceContext->RefTypes[Counter] = 0;
        }

        // This reference is removed by the caller.

        deviceContext->RefTypes[DCREF_CREATION] = 1;
    }
#endif

    deviceContext->CreateRefRemoved = FALSE;

    //
    // initialize the various fields in the device context
    //

    KeInitializeSpinLock (&deviceContext->Interlock);
    KeInitializeSpinLock (&deviceContext->SpinLock);

#if 0 /* disable connection pool */
    InitializeListHead (&deviceContext->ConnectionPool);
#endif

    deviceContext->State = DEVICECONTEXT_STATE_OPENING;
    
    //
    // set the netbios multicast address for this network type
    //

    for (i=0; i<HARDWARE_ADDRESS_LENGTH; i++) {
        deviceContext->LocalAddress.Address [i] = 0; // set later
    }

     deviceContext->Type = LPX_CONTROL_CONTEXT_SIGNATURE;
     deviceContext->Size = sizeof (CONTROL_CONTEXT);


    {
        UNICODE_STRING  unicodeDeviceName;

        RtlInitUnicodeString(&unicodeDeviceName, SOCKETLPX_DOSDEVICE_NAME);
        IoCreateSymbolicLink(&unicodeDeviceName, &lpxDeviceName);
    }

    deviceContext->Information.Version = 0x0100;
    deviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    deviceContext->Information.MaxConnectionUserData = 0;
    deviceContext->Information.MaxDatagramSize =   deviceContext->MaxUserData;
    deviceContext->Information.ServiceFlags = LPX_SERVICE_FLAGS;
    deviceContext->Information.MinimumLookaheadData =  240 - sizeof(LPX_HEADER2);
    deviceContext->Information.MaximumLookaheadData =
        deviceContext->MaxReceivePacketSize - sizeof(LPX_HEADER2);
    deviceContext->Information.NumberOfResources = LPX_TDI_RESOURCES;
    KeQuerySystemTime (&deviceContext->Information.StartTime);


    //
    // Allocate various structures we will need.
    //


 #if 0 // disable connection pool
    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating connections.\n");
    }
    for (i=0; i<LpxConfig->InitConnections; i++) {

        LpxAllocateConnection (deviceContext, &Connection);

        if (Connection == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate connections.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&deviceContext->ConnectionPool, &Connection->LinkList);
    }

    deviceContext->ConnectionInitAllocated = LpxConfig->InitConnections;
    deviceContext->ConnectionMaxAllocated = LpxConfig->MaxConnections;
#else
    deviceContext->ConnectionInitAllocated = 0;
    deviceContext->ConnectionMaxAllocated = LpxConfig->MaxConnections;
#endif

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint1("%d connections\n", LpxConfig->InitConnections);
    }

    deviceContext->State = DEVICECONTEXT_STATE_OPEN;


    DeviceObject = (PDEVICE_OBJECT) deviceContext;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    RtlInitUnicodeString(&DeviceString, deviceContext->DeviceName);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("TdiRegisterDeviceObject for %S\n", DeviceString.Buffer);
    }

    status = TdiRegisterDeviceObject(&DeviceString,
                                     &deviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
       // RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }

    LpxReferenceControlContext ("Load Succeeded", deviceContext, DCREF_CREATION);

    LpxControlDeviceContext = deviceContext;

    ACQUIRE_SPIN_LOCK(&deviceContext->SpinLock, &irql);
    LpxControlDeviceContext->bDeviceInit = TRUE;
    RELEASE_SPIN_LOCK(&deviceContext->SpinLock, irql);
    
    return;

cleanup:

    ASSERT(status != STATUS_SUCCESS);
    
    if (InterlockedExchange(&deviceContext->CreateRefRemoved, TRUE) == FALSE) {
        // Remove creation reference
        LpxDereferenceControlContext ("Load failed", deviceContext, DCREF_CREATION);
    }
    return;
}


VOID
LpxDestroyControlDevice(
    PCONTROL_CONTEXT DeviceContext
    )
/*++

Routine Description:

    This routine deactivates a transport binding. Before it does this, it
    indicates to all clients above, that the device is going away. Clients
    are expected to close all open handles to the device.

    Then the device is pulled out of the list of LPX devices, and all
    resources reclaimed. Any connections, address files etc, that the
    client has cleaned up are forcibly cleaned out at this point. Any
    outstanding requests are completed (with a status). Any future
    requests are automatically invalid as they use obsolete handles.

Arguments:

    NdisStatus              - The status of the bind.

    ProtocolBindContext     - the context from the openadapter call 

    UnbindContext           - A context for async unbinds.


Return Value:

    None.
    
--*/
{
    NTSTATUS Status;
    KIRQL   irql;
	DebugPrint(2, ("LpxDestroyControlDevice %p\n", LpxControlDeviceContext));
    ASSERT( LpxControlDeviceContext == DeviceContext) ;
#if DBG

    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }

#endif

    // Get the device context for the adapter being unbound
    ACQUIRE_SPIN_LOCK(&DeviceContext->SpinLock, &irql);
    DeviceContext->bDeviceInit = FALSE;
    RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, irql);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("ENTER LpxDestroyControlDevice for %S\n", DeviceContext->DeviceName);
    }

    // Remove creation ref if it has not already been removed,
    // after telling TDI and its clients that we'r going away.
    // This flag also helps prevent any more TDI indications
    // of deregister addr/devobj - after the 1st one succeeds.
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

        {
            UNICODE_STRING  unicodeDeviceName;
            RtlInitUnicodeString(&unicodeDeviceName, SOCKETLPX_DOSDEVICE_NAME);
            IoDeleteSymbolicLink( &unicodeDeviceName);
        }

        // Inform TDI (and its clients) that device is going away
        Status = TdiDeregisterDeviceObject(DeviceContext->TdiDeviceHandle);

        if (!NT_SUCCESS (Status)) {
        
            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1("No success deregistering device object,STATUS = %08X\n",Status);
            }

            // This can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            DeviceContext->CreateRefRemoved = FALSE;

            return;
        }

        // BUG BUG -- probable race condition with timer callbacks
        // Do we wait for some time in case a timer func gets in ?

        // Removing creation reference means that once all handles
        // r closed,device will automatically be garbage-collected
        LpxDereferenceControlContext ("Unload", DeviceContext, DCREF_CREATION);

    } else {
        // Ignore any duplicate Unbind Indications from NDIS layer
        Status = STATUS_DEVICE_REMOVED; // Already removed.
    }

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("LEAVE LpxDestroyControlDevice for %S with Status %08x\n",
                        DeviceContext->DeviceName, Status);
    }

    return;
}

PDEVICE_CONTEXT
LpxFindDeviceContext(
    PLPX_ADDRESS networkName
	)
{
    PDEVICE_CONTEXT deviceContext;
    PLIST_ENTRY		listHead;
    PLIST_ENTRY		thisEntry;
    CHAR			notAssigned[HARDWARE_ADDRESS_LENGTH] = {0, 0, 0, 0, 0, 0};
    
    DebugPrint(2, ("LpxFindDeviceContext\n"));

    DebugPrint(2,("networkName = %p, networkName= %p %02X%02X%02X%02X%02X%02X:%04X\n",
    				networkName, networkName,
    				networkName->Node[0],
    				networkName->Node[1],
    				networkName->Node[2],
    				networkName->Node[3],
    				networkName->Node[4],
    				networkName->Node[5],
    				networkName->Port));

    ACQUIRE_DEVICES_LIST_LOCK();

    if (IsListEmpty (&LpxDeviceList)) {
        RELEASE_DEVICES_LIST_LOCK();
        return NULL;
    }

    listHead = &LpxDeviceList;
    for(deviceContext = NULL, thisEntry = listHead->Flink;
        thisEntry != listHead;
        thisEntry = thisEntry->Flink)
    {

        deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, DeviceListLinkage);
        if(deviceContext->CreateRefRemoved == FALSE && deviceContext->bDeviceInit == TRUE){
            if (RtlEqualMemory (
                    deviceContext->LocalAddress.Address,
                    &networkName->Node,
                    HARDWARE_ADDRESS_LENGTH
                ))
            {
                break;
            }
        }
        deviceContext = NULL;
    }

    if(deviceContext == NULL 
    	&& RtlEqualMemory (
    			notAssigned,
    			&networkName->Node,
    			HARDWARE_ADDRESS_LENGTH
    			)
    	&& LpxPrimaryDeviceContext) 
    {
    	deviceContext = LpxPrimaryDeviceContext;
    	ASSERT(deviceContext);
        RtlCopyMemory (
    		&networkName->Node,
    		deviceContext->LocalAddress.Address,
    		HARDWARE_ADDRESS_LENGTH
    		);
    }

    RELEASE_DEVICES_LIST_LOCK();
    return deviceContext;
}


