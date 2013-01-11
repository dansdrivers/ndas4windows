/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop


// PnP-Power Declarations

VOID
LpxPnPEventDispatch(
                    IN PVOID            NetPnPEvent
                   );

VOID
LpxPnPEventComplete(
                    IN PNET_PNP_EVENT   NetPnPEvent,
                    IN NTSTATUS         retVal
                   );

NTSTATUS
LpxPnPBindsComplete(
                    IN PDEVICE_CONTEXT  DeviceContext,
                    IN PNET_PNP_EVENT   NetPnPEvent
                   );

// PnP Handler Routines

VOID
LpxProtocolBindAdapter(
                OUT PNDIS_STATUS    NdisStatus,
                IN NDIS_HANDLE      BindContext,
                IN PNDIS_STRING     DeviceName,
                IN PVOID            SystemSpecific1,
                IN PVOID            SystemSpecific2
                ) 
/*++

Routine Description:

    This routine activates a transport binding and exposes the new device
    and associated addresses to transport clients.  This is done by reading
    the registry, and performing any one time initialization of the transport
    and then natching the device to bind to with the linkage information from
    the registry.  If we have a match for that device the bind will be 
    performed.

Arguments:

    NdisStatus      - The status of the bind.

    BindContext     - A context used for NdisCompleteBindAdapter() if 
                      STATUS_PENDING is returned.

    DeviceName      - The name of the device that we are binding with.

    SystemSpecific1 - Unused (a pointer to an NDIS_STRING to use with
                      NdisOpenProtocolConfiguration.  This is not used by lpx
                      since there is no adapter specific information when 
                      configuring the protocol via the registry. Passed to
                      LpxInitializeOneDeviceContext for possible future use)

    SystemSpecific2 - Passed to LpxInitializeOneDeviceContext to be used
                      in a call to TdiRegisterNetAddress

Return Value:

    None.

--*/
{
    PUNICODE_STRING ExportName;
    UNICODE_STRING ExportString;
    ULONG j, k;
    NTSTATUS status;
    KIRQL  oldirql;
#if DBG
    // Called at PASSIVE_LEVEL
    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }
#endif

    UNREFERENCED_PARAMETER(BindContext) ;

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("ENTER LpxProtocolBindAdapter for %S\n", DeviceName->Buffer);
    }


    //
    // Loop through all the adapters that are in the configuration
    // information structure (this is the initial cache) until we
    // find the one that NDIS is calling Protocol bind adapter for. 
    //        

    for (j = 0; j < LpxConfig->NumAdapters; j++ ) {

        if (NdisEqualString(DeviceName, &LpxConfig->Names[j], TRUE)) {
            break;
        }
    }

    if (j < LpxConfig->NumAdapters) {

        // We found the bind to export mapping in initial cache

        ExportName = &LpxConfig->Names[LpxConfig->DevicesOffset + j];
    }
    else {

        IF_LPXDBG (LPX_DEBUG_PNP) {
        
            LpxPrint1("\nNot In Initial Cache = %08x\n\n", DeviceName->Buffer);

            LpxPrint0("Bind Names in Initial Cache: \n");

            for (k = 0; k < LpxConfig->NumAdapters; k++)
            {
                LpxPrint3("Config[%2d]: @ %08x, %75S\n",
                           k, &LpxConfig->Names[k],
                           LpxConfig->Names[k].Buffer);
            }

            LpxPrint0("Export Names in Initial Cache: \n");

            for (k = 0; k < LpxConfig->NumAdapters; k++)
            {
                LpxPrint3("Config[%2d]: @ %08x, %75S\n",
                           k, &LpxConfig->Names[LpxConfig->DevicesOffset + k],
                           LpxConfig->Names[LpxConfig->DevicesOffset + k].Buffer);
            }

            LpxPrint0("\n\n");
        }

        ExportName = &ExportString;

        //
        // We have not found the name in the initial registry info;
        // Read the registry and check if a new binding appeared...
        //

        *NdisStatus = LpxGetExportNameFromRegistry(&LpxRegistryPath,
                                                   DeviceName,
                                                   ExportName
                                                  );
        if (!NT_SUCCESS (*NdisStatus))
        {
            return;
        }
    }

    LpxInitializeOneDeviceContext(NdisStatus, 
                                  LpxDriverObject,
                                  LpxConfig,
                                  DeviceName,
                                  ExportName,
                                  SystemSpecific1,
                                  SystemSpecific2
                                 );

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("LEAVE LpxProtocolBindAdapter for %S with Status %08x\n", 
                        DeviceName->Buffer, *NdisStatus);
    }

    {
        PDEVICE_CONTEXT deviceContext;
        PLIST_ENTRY		listHead;
        PLIST_ENTRY		thisEntry;
        UNICODE_STRING	deviceString;

        //
        //
        //	added by hootch 03182004
        if(NDIS_STATUS_SUCCESS != *NdisStatus) {
            if (ExportName == &ExportString)
            {
                ExFreePool(ExportName->Buffer);
            }
            DebugPrint(1, ("[LPX] LpxPRotocolBindAdapter: LpxInitializeOneDeviceContext() failed!\n")) ;
            return ;
        }
        ACQUIRE_DEVICES_LIST_LOCK();

        listHead = &LpxDeviceList;
        for(deviceContext = NULL, thisEntry = listHead->Flink;
            thisEntry != listHead;
            thisEntry = thisEntry->Flink)
        {
            deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
            RtlInitUnicodeString(&deviceString, deviceContext->DeviceName);

            if (NdisEqualString(&deviceString, ExportName, TRUE)) 
                break;
        }
        RELEASE_DEVICES_LIST_LOCK();

        // Check if we need to de-allocate the ExportName buffer

        if (ExportName == &ExportString)
        {
            ExFreePool(ExportName->Buffer);
        }

        ASSERT(deviceContext);

        NdisAllocatePacketPool(
            &status,
            &deviceContext->LpxPacketPool,
            TRANSMIT_PACKETS,
            sizeof(LPX_RESERVED)
        );
        NdisAllocateBufferPool(
            &status,
            &deviceContext->LpxBufferPool,
            TRANSMIT_PACKETS
        );

        deviceContext->PortNum = LPX_PORTASSIGN_BEGIN;

        ACQUIRE_DEVICES_LIST_LOCK();

        // ILGU 2003_1104 support for shutdown NIC
        if(LpxPrimaryDeviceContext == NULL);
        	LpxPrimaryDeviceContext = deviceContext;

        RELEASE_DEVICES_LIST_LOCK();
        // ILGU 2003_1103	Support packet drop flags
        ACQUIRE_SPIN_LOCK(&deviceContext->SpinLock, &oldirql);
        deviceContext->bDeviceInit = TRUE;	
        RELEASE_SPIN_LOCK(&deviceContext->SpinLock, oldirql);
    }
    return;
}


VOID
LpxProtocolUnbindAdapter(
                    OUT PNDIS_STATUS NdisStatus,
                    IN NDIS_HANDLE ProtocolBindContext,
                    IN PNDIS_HANDLE UnbindContext
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
    PDEVICE_CONTEXT DeviceContext;
    KIRQL oldIrql;

    UNREFERENCED_PARAMETER(UnbindContext) ;

#if DBG

    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }
#endif

    DebugPrint(3,("LpxProtocolUnbindAdapter ENTER \n"));

    // Get the device context for the adapter being unbound
    DeviceContext = (PDEVICE_CONTEXT) ProtocolBindContext;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;

{
//	NDIS_STATUS		ndisStatus;
    UINT			deviceCount;
	PLIST_ENTRY		listHead;
	PLIST_ENTRY		thisEntry;

	//
	//	'cause we have only Socket Lpx Device Context
	//
	if(LpxControlDeviceContext == NULL)
		goto Out;

    ACQUIRE_DEVICES_LIST_LOCK();

	if (IsListEmpty (&LpxDeviceList)) {
		RELEASE_DEVICES_LIST_LOCK();

		goto Out;
	}

	listHead = &LpxDeviceList;
	for(deviceCount = 0, thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = thisEntry->Flink)
	{
		PDEVICE_CONTEXT deviceContext;
    
		deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
		if(deviceContext->CreateRefRemoved == FALSE)
		{
			deviceCount++;
		}
	}
	
	
	RELEASE_DEVICES_LIST_LOCK();

	if(deviceCount == 1) {
	        // The last device is being removed
		LpxPrimaryDeviceContext = NULL;
	} else {
		if(DeviceContext == LpxPrimaryDeviceContext) 
		{
			PDEVICE_CONTEXT deviceContext;

			deviceContext = NULL;
			
			ACQUIRE_DEVICES_LIST_LOCK();

			listHead = &LpxDeviceList;
			for(deviceCount = 0, thisEntry = listHead->Flink;
				thisEntry != listHead;
				thisEntry = thisEntry->Flink)
			{
				deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
				if(deviceContext != LpxPrimaryDeviceContext)
					break;
				deviceContext = NULL;
			}
			ASSERT(deviceContext);
			ASSERT(DeviceContext->CreateRefRemoved == FALSE);
			LpxPrimaryDeviceContext = deviceContext;
			RELEASE_DEVICES_LIST_LOCK();
		}
	}
}
Out:

    ACQUIRE_SPIN_LOCK(&DeviceContext->SpinLock, &oldIrql);
    DeviceContext->bDeviceInit = FALSE;
    RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, oldIrql);
    
    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("ENTER LpxProtocolUnbindAdapter for %S\n", DeviceContext->DeviceName);
    }

    // Remove creation ref if it has not already been removed,
    // after telling TDI and its clients that we'r going away.
    // This flag also helps prevent any more TDI indications
    // of deregister addr/devobj - after the 1st one succeeds.
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

        // Assume upper layers clean up by closing connections
        // when we deregister all addresses and device object,
        // but this can happen asynchronously, after we return
        // from the (asynchronous) TdiDeregister.. calls below 

        // Inform TDI by deregistering the reserved netbios address
        *NdisStatus = TdiDeregisterNetAddress(DeviceContext->ReservedAddressHandle);

        if (!NT_SUCCESS (*NdisStatus)) {
        
            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1("No success deregistering this address,STATUS = %08X\n",*NdisStatus);
            }

            // this can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            DeviceContext->CreateRefRemoved = FALSE;
            
            return;
        }
        
        // Inform TDI (and its clients) that device is going away
        *NdisStatus = TdiDeregisterDeviceObject(DeviceContext->TdiDeviceHandle);

        if (!NT_SUCCESS (*NdisStatus)) {
        
            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1("No success deregistering device object,STATUS = %08X\n",*NdisStatus);
            }

            // This can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            DeviceContext->CreateRefRemoved = FALSE;

            return;
        }

        // Clear away the association with the underlying PDO object
        DeviceContext->PnPContext = NULL;

        // Cleanup the Ndis Binding as it is not useful on return
        // from this function - do not try to use it after this
        LpxCloseNdis(DeviceContext);

        // BUG BUG -- probable race condition with timer callbacks
        // Do we wait for some time in case a timer func gets in ?

        // Removing creation reference means that once all handles
        // r closed,device will automatically be garbage-collected
        LpxDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);

/*
		LpxDereferenceDeviceContext ("Free DeviceContext", DeviceContext, DCREF_CREATION);
*/
	}
	else {
    
		DebugPrint(3,(" PREVIOUS DISABLE DEVICE CONTEXT %p\n",DeviceContext));
        // Ignore any duplicate Unbind Indications from NDIS layer
        *NdisStatus = NDIS_STATUS_SUCCESS;
/*
		LpxDereferenceDeviceContext ("Free DeviceContext", DeviceContext, DCREF_CREATION);
*/
	}

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("LEAVE LpxProtocolUnbindAdapter for %S with Status %08x\n",
                        DeviceContext->DeviceName, *NdisStatus);
    }
	DebugPrint(3,("LpxProtocolUnbindAdapter END \n"));
    return;
}

NDIS_STATUS
LpxProtocolPnPEventHandler(
                    IN NDIS_HANDLE ProtocolBindContext,
                    IN PNET_PNP_EVENT NetPnPEvent
                          )
/*++

Routine Description:

    This routine queues a work item to invoke the actual PnP
    event dispatcher. This asyncronous mechanism is to allow
    NDIS to signal PnP events to other bindings in parallel.

Arguments:

    ProtocolBindContext - the context from the openadapter call 

    NetPnPEvent         - kind of PnP event and its parameters

Return Value:

    STATUS_PENDING (or) an error code
    
--*/

{
    PNET_PNP_EVENT_RESERVED NetPnPReserved;
    PWORK_QUEUE_ITEM PnPWorkItem;

    PnPWorkItem = (PWORK_QUEUE_ITEM)ExAllocatePoolWithTag(
                                        NonPagedPool,
                                        sizeof (WORK_QUEUE_ITEM),
                                        LPX_MEM_TAG_WORK_ITEM);

    if (PnPWorkItem == NULL) 
    {
        return NDIS_STATUS_RESOURCES;
    }

    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)NetPnPEvent->TransportReserved;
    NetPnPReserved->PnPWorkItem = PnPWorkItem;
    NetPnPReserved->DeviceContext = (PDEVICE_CONTEXT) ProtocolBindContext;
//    ASSERT( LpxControlDeviceContext != NetPnPReserved->DeviceContext);
    ExInitializeWorkItem(
            PnPWorkItem,
            LpxPnPEventDispatch,
            NetPnPEvent);
            
    ExQueueWorkItem(PnPWorkItem, CriticalWorkQueue);

    return NDIS_STATUS_PENDING;
}

VOID
LpxPnPEventDispatch(
                    IN PVOID NetPnPEvent
                   )
/*++

Routine Description:

    This routine dispatches all PnP events for the LPX transport.
    The event is dispatched to the proper PnP event handler, and
    the events are indicated to the transport clients using TDI.

    These PnP events can trigger state changes that affect the
    device behavior ( like transitioning to low power state ).

Arguments:

    NetPnPEvent         - kind of PnP event and its parameters

Return Value:

    None

--*/

{
    PNET_PNP_EVENT_RESERVED NetPnPReserved;
    PDEVICE_CONTEXT  DeviceContext;
    UNICODE_STRING   DeviceString;
    PTDI_PNP_CONTEXT tdiPnPContext1;
    PTDI_PNP_CONTEXT tdiPnPContext2;
    NDIS_STATUS      retVal;

    // Retrieve the transport information block in event
    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)((PNET_PNP_EVENT)NetPnPEvent)->TransportReserved;

    // Free the memory allocated for this work item itself
    ExFreePool(NetPnPReserved->PnPWorkItem);
     
    // Get the device context for the adapter being unbound
    DeviceContext = NetPnPReserved->DeviceContext;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;

    // In case everything goes ok, we return an NDIS_SUCCESS
    retVal = STATUS_SUCCESS;
    
    // Dispatch the PnP Event to the appropriate PnP handler
    switch (((PNET_PNP_EVENT)NetPnPEvent)->NetEvent)
    {
        case NetEventReconfigure:
        case NetEventCancelRemoveDevice:
        case NetEventQueryRemoveDevice:
        case NetEventQueryPower:
        case NetEventSetPower:
        case NetEventPnPCapabilities:
            break;

        case NetEventBindsComplete:
            retVal = LpxPnPBindsComplete(DeviceContext, NetPnPEvent);
            break;

        default:
            ASSERT( FALSE );
    }

    if ( retVal == STATUS_SUCCESS ) 
    {
        if (DeviceContext != NULL)
        {
            RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);
            tdiPnPContext1 = tdiPnPContext2 = NULL;

            //  Notify our TDI clients about this PNP event
            retVal = TdiPnPPowerRequest(&DeviceString,
                                         NetPnPEvent,
                                         tdiPnPContext1, 
                                         tdiPnPContext2,
                                         LpxPnPEventComplete);
        }
    }

    if (retVal != STATUS_PENDING)
    {
        NdisCompletePnPEvent(retVal, (NDIS_HANDLE)DeviceContext, NetPnPEvent);
    }
}

//
// PnP Complete Handler
//
VOID
LpxPnPEventComplete(
                    IN PNET_PNP_EVENT   NetPnPEvent,
                    IN NTSTATUS         retVal
                   )
{
    PNET_PNP_EVENT_RESERVED NetPnPReserved;
    PDEVICE_CONTEXT  DeviceContext;

    // Retrieve the transport information block in event
    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)NetPnPEvent->TransportReserved;

    // Get the device context for the adapter being unbound
    DeviceContext = NetPnPReserved->DeviceContext;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;

    NdisCompletePnPEvent(retVal, (NDIS_HANDLE)DeviceContext, NetPnPEvent);
}

//
// PnP Handler Dispatches
//

NTSTATUS
LpxPnPBindsComplete(
                    IN PDEVICE_CONTEXT  DeviceContext,
                    IN PNET_PNP_EVENT   NetPnPEvent
                   )
{
    NDIS_STATUS retVal;

    ASSERT(DeviceContext == NULL);

	UNREFERENCED_PARAMETER(DeviceContext) ;
	UNREFERENCED_PARAMETER(NetPnPEvent) ;

    retVal = TdiProviderReady(LpxProviderHandle);

    ASSERT(retVal == STATUS_SUCCESS);

    return retVal;
}

