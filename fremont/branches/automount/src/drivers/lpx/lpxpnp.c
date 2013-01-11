/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    nbfpnp.c

Abstract:

    This module contains code which allocates and initializes all data 
    structures needed to activate a plug and play binding.  It also informs
    tdi (and thus nbf clients) of new devices and protocol addresses. 

Author:

    Jim McNelis (jimmcn)  1-Jan-1996

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#ifdef RASAUTODIAL

LONG NumberOfBinds = 0;

VOID
LpxAcdBind();

VOID
LpxAcdUnbind();

#endif // RASAUTODIAL

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
                      NdisOpenProtocolConfiguration.  This is not used by nbf
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
#if __LPX__
	ULONG j, k;
#endif
    NTSTATUS status;

#if __LPX__
	UNREFERENCED_PARAMETER( BindContext );
#endif

#if DBG
    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }
#endif

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("ENTER LpxProtocolBindAdapter for %S\n", DeviceName->Buffer);
    }

    if (LpxConfig == NULL) {
        //
        // This allocates the CONFIG_DATA structure and returns
        // it in LpxConfig.
        //

        status = LpxConfigureTransport(&LpxRegistryPath, &LpxConfig);

        if (!NT_SUCCESS (status)) {
            PANIC (" Failed to initialize transport, Lpx binding failed.\n");
            *NdisStatus = NDIS_STATUS_RESOURCES;
            return;
        }

#if DBG
        //
        // Allocate the debugging tables. 
        //

        LpxConnectionTable = (PVOID *)ExAllocatePoolWithTag(NonPagedPool,
                                          sizeof(PVOID) *
                                          (LpxConfig->InitConnections + 2 +
                                           LpxConfig->InitAddressFiles + 2 +
                                           LpxConfig->InitAddresses + 2),
                                           LPX_MEM_TAG_CONNECTION_TABLE);

        ASSERT (LpxConnectionTable);

        LpxAddressFileTable = LpxConnectionTable + (LpxConfig->InitConnections + 2);
        LpxAddressTable = LpxAddressFileTable + 
                                        (LpxConfig->InitAddressFiles + 2);
#endif

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
        
            LpxPrint1("\nNot In Initial Cache = %p\n\n", DeviceName->Buffer);

            LpxPrint0("Bind Names in Initial Cache: \n");

            for (k = 0; k < LpxConfig->NumAdapters; k++)
            {
                LpxPrint3("Config[%2d]: @ %p, %75S\n",
                           k, &LpxConfig->Names[k],
                           LpxConfig->Names[k].Buffer);
            }

            LpxPrint0("Export Names in Initial Cache: \n");

            for (k = 0; k < LpxConfig->NumAdapters; k++)
            {
                LpxPrint3("Config[%2d]: @ %p, %75S\n",
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

    // Check if we need to de-allocate the ExportName buffer

#ifndef __LPX__

	if (ExportName == &ExportString)
    {
        ExFreePool(ExportName->Buffer);
    }

#endif

    if (*NdisStatus == NDIS_STATUS_SUCCESS) {

#ifdef RASAUTODIAL

		if (InterlockedIncrement(&NumberOfBinds) == 1) {

            // 
            // This is the first successful open.
            //
#if DBG
            DbgPrint("Calling LpxAcdBind()\n");
#endif
            //
            // Get the automatic connection driver entry points.
            //
            
            LpxAcdBind();
        }            

#endif // RASAUTODIAL

	}

#if __LPX__
{
    PDEVICE_CONTEXT deviceContext;
	PLIST_ENTRY		listHead;
	PLIST_ENTRY		thisEntry;
    UNICODE_STRING	deviceString;

	//
	//
	//	added by hootch 03182004

	if (NDIS_STATUS_SUCCESS != *NdisStatus) {
    
		if (ExportName == &ExportString) {

	        ExFreePool( ExportName->Buffer );
	   }

		DebugPrint( 1, ("[LPX] LpxPRotocolBindAdapter: LpxInitializeOneDeviceContext() failed!\n") );
		ASSERT( FALSE );
		return;
	}

	ACQUIRE_DEVICES_LIST_LOCK();

	listHead = &LpxDeviceList;

	for (deviceContext = NULL, thisEntry = listHead->Flink;
		 thisEntry != listHead;
		 thisEntry = thisEntry->Flink) {

        deviceContext = CONTAINING_RECORD( thisEntry, DEVICE_CONTEXT, Linkage );
        RtlInitUnicodeString( &deviceString, deviceContext->DeviceName );

        if (NdisEqualString(&deviceString, ExportName, TRUE)) 
			break;
	}

	 RELEASE_DEVICES_LIST_LOCK();


	//
	//	patched by hootch 03052004
	//	found by aingoppa 03052004
	//
	// Check if we need to de-allocate the ExportName buffer

    if (ExportName == &ExportString) {

        ExFreePool( ExportName->Buffer );
    }

	ASSERT( deviceContext );

	MacReturnMaxDataSize( &deviceContext->MacInfo,
						  NULL,
						  0,
						  deviceContext->MaxSendPacketSize,
						  TRUE,
						  &deviceContext->MaxUserData );

	if (deviceContext->MaxUserData < LPX_MAX_DATAGRAM_SIZE + sizeof(LPX_HEADER)) {

		ASSERT( FALSE );
		return;
	}

	NdisAllocatePacketPool( &status,
							&deviceContext->LpxPacketPool,
							PACKET_BUFFER_POOL_SIZE,
							sizeof(LPX_RESERVED) );

	if (status != NDIS_STATUS_SUCCESS) {
		
		ASSERT(FALSE);
		return;
	}

	NdisAllocateBufferPool( &status,
							&deviceContext->LpxBufferPool,
							PACKET_BUFFER_POOL_SIZE );

	if (status != NDIS_STATUS_SUCCESS) {

		ASSERT( FALSE );
		return;
	}

	deviceContext->LastPortNum = LPX_PORTASSIGN_BEGIN;

#if DBG
	DebugSpinLock = &SocketLpxPrimaryDeviceContext->SpinLock;
#endif

	DebugPrint(1, ("deviceContext->MaxUserData = %d\n", deviceContext->MaxUserData) );

	SetFlag( deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START );
	ClearFlag( deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP );

	DebugPrint( 0, ("deviceContext = %p, deviceContext->LpxFlags = %x\n", deviceContext, deviceContext->LpxFlags) );

	if (SocketLpxPrimaryDeviceContext == NULL &&
		FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_MIDIA_CONNECTED) && 
		!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_MIDIA_DISCONNECTED)) {

		//ASSERT( FALSE );	
		SocketLpxPrimaryDeviceContext = deviceContext;
	}

	IF_LPXDBG (LPX_DEBUG_PNP) {
		LpxPrint3 ("LEAVE LpxProtocolBindAdapter for %S with Status %08x deviceContext = %p\n", 
			DeviceName->Buffer, *NdisStatus, deviceContext );
	}
}

#endif

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

#if __LPX__
	UNREFERENCED_PARAMETER( UnbindContext );
#endif

#if DBG

    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }
#endif

    // Get the device context for the adapter being unbound
    DeviceContext = (PDEVICE_CONTEXT) ProtocolBindContext;

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("ENTER LpxProtocolUnbindAdapter for %S\n", DeviceContext->DeviceName);
    }

#if __LPX__

	do {

		UINT			deviceCount;
		PLIST_ENTRY		listHead;
		PLIST_ENTRY		thisEntry;
		PDEVICE_CONTEXT deviceContext;
		KIRQL			oldirql;

        ACQUIRE_SPIN_LOCK( &DeviceContext->SpinLock, &oldirql );

		SetFlag( DeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP );

		RELEASE_SPIN_LOCK( &DeviceContext->SpinLock, oldirql );

		//
		//	'cause we have only Socket Lpx Device Context
		//

		if (SocketLpxPrimaryDeviceContext && DeviceContext != SocketLpxPrimaryDeviceContext) {

			break;
		}

		ACQUIRE_DEVICES_LIST_LOCK();

		if (IsListEmpty(&LpxDeviceList)) {

			SocketLpxPrimaryDeviceContext = NULL;

			RELEASE_DEVICES_LIST_LOCK();
			break;
		}

		listHead = &LpxDeviceList;

		for (deviceCount = 0, thisEntry = listHead->Flink;
			 thisEntry != listHead;
			 thisEntry = thisEntry->Flink) {

			deviceContext = CONTAINING_RECORD( thisEntry, DEVICE_CONTEXT, Linkage );
		
			if (deviceContext != SocketLpxDeviceContext && 
				deviceContext->CreateRefRemoved == FALSE && 
				FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) &&
				!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP) &&
				FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_MIDIA_CONNECTED) &&
				!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_MIDIA_DISCONNECTED)) {

				deviceCount++;
			}
		}

		if (deviceCount == 0) {

			ASSERT( SocketLpxPrimaryDeviceContext == NULL || DeviceContext == SocketLpxPrimaryDeviceContext );
			SocketLpxPrimaryDeviceContext = NULL;

			RELEASE_DEVICES_LIST_LOCK();

			break;
		}

		listHead = &LpxDeviceList;

		for (deviceContext = NULL, thisEntry = listHead->Flink;
			 thisEntry != listHead;
			 deviceContext = NULL, thisEntry = thisEntry->Flink) {

			deviceContext = CONTAINING_RECORD( thisEntry, DEVICE_CONTEXT, Linkage );
				
			if (deviceContext != SocketLpxDeviceContext && 
				deviceContext->CreateRefRemoved == FALSE && 
				FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) &&
				!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP) &&
				FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_MIDIA_CONNECTED) &&
				!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_MIDIA_DISCONNECTED)) {

				break;
			}
		}
		
		ASSERT( deviceContext );
		ASSERT( DeviceContext->CreateRefRemoved == FALSE );
		SocketLpxPrimaryDeviceContext = deviceContext;
		
		RELEASE_DEVICES_LIST_LOCK();

	} while (0);

#endif

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

        // Stop all the internal timers - this'll clear timer refs
        LpxStopTimerSystem(DeviceContext);

        // Cleanup the Ndis Binding as it is not useful on return
        // from this function - do not try to use it after this
        LpxCloseNdis(DeviceContext);

        // BUG BUG -- probable race condition with timer callbacks
        // Do we wait for some time in case a timer func gets in ?

        // Removing creation reference means that once all handles
        // r closed,device will automatically be garbage-collected
        LpxDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);

#ifdef RASAUTODIAL

		if (InterlockedDecrement(&NumberOfBinds) == 0) {

			// 
            // This is a successful close of last adapter
            //
#if DBG
            DbgPrint("Calling LpxAcdUnbind()\n");
#endif

            //
            // Unbind from the automatic connection driver.
            //  

            LpxAcdUnbind();
        }

#endif // RASAUTODIAL

	}
    else {
    
        // Ignore any duplicate Unbind Indications from NDIS layer
        *NdisStatus = NDIS_STATUS_SUCCESS;
    }

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("LEAVE LpxProtocolUnbindAdapter for %S with Status %08x\n",
                        DeviceContext->DeviceName, *NdisStatus);
    }

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

	IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ( "LpxProtocolPnPEventHandler for %p %p\n", NetPnPEvent, ProtocolBindContext );
    }

    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)NetPnPEvent->TransportReserved;
    NetPnPReserved->PnPWorkItem = PnPWorkItem;
    NetPnPReserved->DeviceContext = (PDEVICE_CONTEXT) ProtocolBindContext;

#pragma warning(disable: 4995 4996)
    ExInitializeWorkItem(
            PnPWorkItem,
            LpxPnPEventDispatch,
            NetPnPEvent);
            
    ExQueueWorkItem(PnPWorkItem, CriticalWorkQueue);
#pragma warning(default: 4995 4996)

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

    // In case everything goes ok, we return an NDIS_SUCCESS
    retVal = STATUS_SUCCESS;

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint3 ("ENTER LpxPnPEventDispatch DeviceContext = %p, ((PNET_PNP_EVENT)NetPnPEvent)->NetEvent = %d NetEventQueryRemoveDevice = %d\n", 
					DeviceContext, ((PNET_PNP_EVENT)NetPnPEvent)->NetEvent, NetEventQueryRemoveDevice);
    }

    // Dispatch the PnP Event to the appropriate PnP handler
    switch (((PNET_PNP_EVENT)NetPnPEvent)->NetEvent)
    {
#if 0 //def __LPX__	
        case NetEventQueryRemoveDevice:
		SetFlag( DeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP );
		break;
        case NetEventCancelRemoveDevice:
		ClearFlag( DeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP );
		break;
#endif
        case NetEventCancelRemoveDevice:
        case NetEventQueryRemoveDevice:
		case NetEventReconfigure:
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

#if __LPX__
	UNREFERENCED_PARAMETER( DeviceContext );
	UNREFERENCED_PARAMETER( NetPnPEvent );

	IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ( "LpxPnPBindsComplete for %p %p\n", NetPnPEvent, ((PNET_PNP_EVENT_RESERVED)(NetPnPEvent->TransportReserved))->DeviceContext );
    }

#endif

    ASSERT(DeviceContext == NULL);

    retVal = TdiProviderReady(LpxProviderHandle);

    ASSERT(retVal == STATUS_SUCCESS);

    return retVal;
}

