/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/


#include "precomp.h"
#pragma hdrstop



//
// This is a one-per-driver variable used in binding
// to the NDIS interface.
//

NDIS_HANDLE LpxNdisProtocolHandle = (NDIS_HANDLE)NULL;


NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest,
    IN PNDIS_STRING AdapterName
    );

VOID
LpxOpenAdapterComplete (
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus,
    IN NDIS_STATUS OpenErrorStatus
    );

VOID
LpxCloseAdapterComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    );

VOID
LpxResetComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    );

VOID
LpxRequestComplete (
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS NdisStatus
    );

VOID
LpxStatusIndication (
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferLength
    );

VOID
LpxProcessStatusClosing(
    IN PVOID Parameter
    );

VOID
LpxStatusComplete (
    IN NDIS_HANDLE NdisBindingContext
    );

VOID
LpxProtocolBindAdapter(
                OUT PNDIS_STATUS    NdisStatus,
                IN NDIS_HANDLE      BindContext,
                IN PNDIS_STRING     DeviceName,
                IN PVOID            SystemSpecific1,
                IN PVOID            SystemSpecific2
                );
VOID
LpxProtocolUnbindAdapter(
                OUT PNDIS_STATUS    NdisStatus,
                IN NDIS_HANDLE      ProtocolBindContext,
                IN PNDIS_HANDLE     UnbindContext
                );

NDIS_STATUS
LpxProtocolPnPEventHandler(
                IN  NDIS_HANDLE     ProtocolBindingContext,
                IN  PNET_PNP_EVENT  NetPnPEvent
                );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,LpxProtocolBindAdapter)
#pragma alloc_text(PAGE,LpxRegisterProtocol)
#pragma alloc_text(PAGE,LpxSubmitNdisRequest)
#pragma alloc_text(PAGE,LpxInitializeNdis)
#endif


NTSTATUS
LpxRegisterProtocol (
    IN PUNICODE_STRING NameString
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface.

Arguments:

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.
    STATUS_SUCCESS if all goes well,
    Failure status if we tried to register and couldn't,
    STATUS_INSUFFICIENT_RESOURCES if we couldn't even try to register.

--*/

{
    NDIS_STATUS ndisStatus;
    NDIS_PROTOCOL_CHARACTERISTICS ProtChars;

    RtlZeroMemory(&ProtChars, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    //
    // Set up the characteristics of this protocol
    //
    ProtChars.MajorNdisVersion = 4;
    ProtChars.MinorNdisVersion = 0;
    
    ProtChars.BindAdapterHandler = LpxProtocolBindAdapter;
    ProtChars.UnbindAdapterHandler = LpxProtocolUnbindAdapter;
    ProtChars.PnPEventHandler = LpxProtocolPnPEventHandler;
    
    ProtChars.Name.Length = NameString->Length;
    ProtChars.Name.MaximumLength = NameString->MaximumLength;
    ProtChars.Name.Buffer = NameString->Buffer;

    ProtChars.OpenAdapterCompleteHandler = LpxOpenAdapterComplete;
    ProtChars.CloseAdapterCompleteHandler = LpxCloseAdapterComplete;
    ProtChars.ResetCompleteHandler = LpxResetComplete;
    ProtChars.RequestCompleteHandler = LpxRequestComplete;

    ProtChars.SendCompleteHandler = LpxSendComplete;
    ProtChars.TransferDataCompleteHandler = LpxTransferDataComplete;

    ProtChars.ReceiveHandler = LpxReceiveIndicate;
    ProtChars.ReceiveCompleteHandler = LpxReceiveComplete;
    ProtChars.StatusHandler = LpxStatusIndication;
    ProtChars.StatusCompleteHandler = LpxStatusComplete;

    NdisRegisterProtocol (
        &ndisStatus,
        &LpxNdisProtocolHandle,
        &ProtChars,
        sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
#if DBG
        IF_LPXDBG (LPX_DEBUG_RESOURCE) {
            LpxPrint1("LpxInitialize: NdisRegisterProtocol failed: %s\n",
                        LpxGetNdisStatus(ndisStatus));
        }
#endif
        return (NTSTATUS)ndisStatus;
    }

    return STATUS_SUCCESS;
}


VOID
LpxDeregisterProtocol (
    VOID
    )

/*++

Routine Description:

    This routine removes this transport to the NDIS interface.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NDIS_STATUS ndisStatus;

    if (LpxNdisProtocolHandle != (NDIS_HANDLE)NULL) {
        NdisDeregisterProtocol (
            &ndisStatus,
            LpxNdisProtocolHandle);
        LpxNdisProtocolHandle = (NDIS_HANDLE)NULL;
    }
}


NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest2,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine passed an NDIS_REQUEST to the MAC and waits
    until it has completed before returning the final status.

Arguments:

    DeviceContext - Pointer to the device context for this driver.

    NdisRequest - Pointer to the NDIS_REQUEST to submit.

    AdapterString - The name of the adapter, in case an error needs
        to be logged.

Return Value:

    The function value is the status of the operation.

--*/
{
    NDIS_STATUS NdisStatus;
//    KIRQL   irql;
    AdapterString;
//    ASSERT( LpxControlDeviceContext != DeviceContext) ;
//    ACQUIRE_SPIN_LOCK(&DeviceContext->SpinLock, &irql);
    if (DeviceContext->NdisBindingHandle) {
        NdisRequest(
            &NdisStatus,
            DeviceContext->NdisBindingHandle,
            NdisRequest2);
//        RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, irql);            
    } else {
//        RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, irql);    
        NdisStatus = STATUS_INVALID_DEVICE_STATE;
    }
    
    if (NdisStatus == NDIS_STATUS_PENDING) {

        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint1 ("OID %lx pended.\n",
                NdisRequest2->DATA.QUERY_INFORMATION.Oid);
        }

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus == STATUS_SUCCESS) {

        IF_LPXDBG (LPX_DEBUG_NDIS) {
            if (NdisRequest2->RequestType == NdisRequestSetInformation) {
                LpxPrint1 ("Lpxdrvr: Set OID %lx succeeded.\n",
                    NdisRequest2->DATA.SET_INFORMATION.Oid);
            } else {
                LpxPrint1 ("Lpxdrvr: Query OID %lx succeeded.\n",
                    NdisRequest2->DATA.QUERY_INFORMATION.Oid);
            }
        }

    } else {
#if DBG
        if (NdisRequest2->RequestType == NdisRequestSetInformation) {
            LpxPrint2 ("Lpxdrvr: Set OID %lx failed: %s.\n",
                NdisRequest2->DATA.SET_INFORMATION.Oid, LpxGetNdisStatus(NdisStatus));
        } else {
            LpxPrint2 ("Lpxdrvr: Query OID %lx failed: %s.\n",
                NdisRequest2->DATA.QUERY_INFORMATION.Oid, LpxGetNdisStatus(NdisStatus));
        }
#endif
    }

    return NdisStatus;
}


NTSTATUS
LpxInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
    IN PCONFIG_DATA LpxConfig,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface and sets up
    any necessary NDIS data structures (Buffer pools and such). It will be
    called for each adapter opened by this transport.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/
{
    NDIS_STATUS NdisStatus;
    NDIS_STATUS OpenErrorStatus;
    // LPX support NdisMedium802_3 only
    NDIS_MEDIUM LpxSupportedMedia[] = { NdisMedium802_3 };
    //NDIS_MEDIUM LpxSupportedMedia[] = { NdisMedium802_3, NdisMedium802_5, NdisMediumFddi, NdisMediumWan };

    UINT SelectedMedium;
    NDIS_REQUEST LpxRequest;
    UCHAR LpxDataBuffer[6];
    NDIS_OID LpxOid;
    ULONG MinimumLookahead = 128;
    ULONG MacOptions;
    LpxConfig;
//    ASSERT( LpxControlDeviceContext != DeviceContext) ;
    //
    // Initialize this adapter for LPX use through NDIS
    //

    //
    // This event is used in case any of the NDIS requests
    // pend; we wait until it is set by the completion
    // routine, which also sets NdisRequestStatus.
    //

    KeInitializeEvent(
        &DeviceContext->NdisRequestEvent,
        NotificationEvent,
        FALSE
    );

    DeviceContext->NdisBindingHandle = NULL;

    NdisOpenAdapter (
        &NdisStatus,
        &OpenErrorStatus,
        &DeviceContext->NdisBindingHandle,
        &SelectedMedium,
        LpxSupportedMedia,
        sizeof (LpxSupportedMedia) / sizeof(NDIS_MEDIUM),
        LpxNdisProtocolHandle,
        (NDIS_HANDLE)DeviceContext,
        AdapterString,
        0,
        NULL);

    if (NdisStatus == NDIS_STATUS_PENDING) {

        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint1 ("Adapter %S open pended.\n", AdapterString);
        }

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus == NDIS_STATUS_SUCCESS) {
#if DBG
        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint1 ("Adapter %S successfully opened.\n", AdapterString);
        }
#endif
    } else {

//		DbgPrint("Bind Error!!!!\n", SelectedMedium);

#if DBG
        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint2 ("Adapter open %S failed, status: %s.\n",
                AdapterString,
                LpxGetNdisStatus (NdisStatus));
        }
#endif
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    LpxOid = OID_802_3_CURRENT_ADDRESS;

    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = LpxOid;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = DeviceContext->LocalAddress.Address;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now query the maximum packet sizes.
    //

    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_FRAME_SIZE;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MaxReceivePacketSize);
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MaxSendPacketSize);
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now set the minimum lookahead size.
    //

    LpxRequest.RequestType = NdisRequestSetInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_LOOKAHEAD;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MinimumLookahead;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }



    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_LINK_SPEED;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MediumSpeed);
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now query the MAC's optional characteristics.
    //

    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAC_OPTIONS;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MacOptions;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
#if 1
        return STATUS_INSUFFICIENT_RESOURCES;
#else
        MacOptions = 0;
#endif
    }


    //
    // Fill in the OVB for packet filter.
    //


    RtlStoreUlong((PULONG)LpxDataBuffer,
        (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_BROADCAST));

    //
    // Now fill in the NDIS_REQUEST.
    //

    LpxRequest.RequestType = NdisRequestSetInformation;
    LpxRequest.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    LpxRequest.DATA.SET_INFORMATION.InformationBuffer = &LpxDataBuffer;
    LpxRequest.DATA.SET_INFORMATION.InformationBufferLength = sizeof(ULONG);

    LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;

}   /* LpxInitializeNdis */


VOID
LpxCloseNdis (
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine unbinds the transport from the NDIS interface and does
    any other work required to undo what was done in LpxInitializeNdis.
    It is written so that it can be called from within LpxInitializeNdis
    if it fails partway through.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

Return Value:

    The function value is the status of the operation.

--*/
{
    NDIS_STATUS ndisStatus;
    NDIS_HANDLE NdisBindingHandle;
    KIRQL oldIrql;
//    ASSERT( LpxControlDeviceContext != DeviceContext);
    
    //
    // Close the NDIS binding.
    //
    
    NdisBindingHandle = DeviceContext->NdisBindingHandle;

    ACQUIRE_SPIN_LOCK(&DeviceContext->SpinLock, &oldIrql);
    DeviceContext->NdisBindingHandle = NULL;
    RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, oldIrql);
    
    if (NdisBindingHandle != NULL) {
    
        //
        // This event is used in case any of the NDIS requests
        // pend; we wait until it is set by the completion
        // routine, which also sets NdisRequestStatus.
        //

        KeInitializeEvent(
            &DeviceContext->NdisRequestEvent,
            NotificationEvent,
            FALSE
        );

        NdisCloseAdapter(
            &ndisStatus,
            NdisBindingHandle);

        if (ndisStatus == NDIS_STATUS_PENDING) {

            IF_LPXDBG (LPX_DEBUG_NDIS) {
                LpxPrint0 ("Adapter close pended.\n");
            }

            //
            // The completion routine will set NdisRequestStatus.
            //

            KeWaitForSingleObject(
                &DeviceContext->NdisRequestEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );

            ndisStatus = DeviceContext->NdisRequestStatus;

            KeResetEvent(
                &DeviceContext->NdisRequestEvent
                );

        }

        //
        // We ignore ndisStatus.
        //

    }
}   /* LpxCloseNdis */


VOID
LpxOpenAdapterComplete (
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus,
    IN NDIS_STATUS OpenErrorStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that an open adapter
    is complete. Since we only ever have one outstanding, and then only
    during initialization, all we do is record the status and set
    the event to signalled to unblock the initialization thread.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

    OpenErrorStatus - More status information.

Return Value:

    None.

--*/

{
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)BindingContext;
//    ASSERT( LpxControlDeviceContext != DeviceContext) ;
#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint1 ("Lpxdrvr: LpxOpenAdapterCompleteNDIS Status: %s\n",
            LpxGetNdisStatus (NdisStatus));
    }
#endif

	UNREFERENCED_PARAMETER(OpenErrorStatus) ;


    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID
LpxCloseAdapterComplete (
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a close adapter
    is complete. Currently we don't close adapters, so this is not
    a problem.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)BindingContext;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;
#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint1 ("Lpxdrvr: LpxCloseAdapterCompleteNDIS Status: %s\n",
            LpxGetNdisStatus (NdisStatus));
    }
#endif


    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID
LpxResetComplete (
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a reset adapter
    is complete. Currently we don't reset adapters, so this is not
    a problem.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER(BindingContext);
    UNREFERENCED_PARAMETER(NdisStatus);

#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint1 ("Lpxdrvr: LpxResetCompleteNDIS Status: %s\n",
            LpxGetNdisStatus (NdisStatus));
    }
#endif

    return;
}

VOID
LpxRequestComplete (
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by NDIS to indicate that a request is complete.
    Since we only ever have one request outstanding, and then only
    during initialization, all we do is record the status and set
    the event to signalled to unblock the initialization thread.

Arguments:

    BindingContext - Pointer to the device object for this driver.

    NdisRequest - The object describing the request.

    NdisStatus - The request completion code.

Return Value:

    None.

--*/

{
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)BindingContext;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;
#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint2 ("Lpxdrvr: LpxRequestComplete request: %i, NDIS Status: %s\n",
            NdisRequest->RequestType,LpxGetNdisStatus (NdisStatus));
    }
#endif

	UNREFERENCED_PARAMETER(NdisRequest) ;


    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID
LpxStatusIndication (
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )

{
    PDEVICE_CONTEXT DeviceContext;
    KIRQL oldirql;

    DeviceContext = (PDEVICE_CONTEXT)NdisBindingContext;
//    ASSERT( LpxControlDeviceContext != DeviceContext) ;
    UNREFERENCED_PARAMETER(StatusBufferSize) ;
    UNREFERENCED_PARAMETER(StatusBuffer);
    
    KeRaiseIrql (DISPATCH_LEVEL, &oldirql);

    switch (NdisStatus) {
        case NDIS_STATUS_CLOSING:

            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1 ("LpxStatusIndication: Device @ %08x Closing\n", DeviceContext);
            }

            //
            // The adapter is shutting down. We queue a worker
            // thread to handle this.
            //

            ExInitializeWorkItem(
                &DeviceContext->StatusClosingQueueItem,
                LpxProcessStatusClosing,
                (PVOID)DeviceContext);
            ExQueueWorkItem(&DeviceContext->StatusClosingQueueItem, DelayedWorkQueue);

            break;

        default:
            break;

    }

    KeLowerIrql (oldirql);

}


VOID
LpxProcessStatusClosing(
    IN PVOID Parameter
    )

/*++

Routine Description:

    This is the thread routine which restarts packetizing
    that has been delayed on WAN to allow RRs to come in.
    This is very similar to PacketizeConnections.

Arguments:

    Parameter - A pointer to the device context.

Return Value:

    None.

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    NDIS_STATUS ndisStatus;
    NDIS_HANDLE NdisBindingHandle;
    KIRQL irql;
    DeviceContext = (PDEVICE_CONTEXT)Parameter;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;
    //
    // Prevent new activity on the connection.
    //

    DeviceContext->State = DEVICECONTEXT_STATE_DOWN;
 
    //
    // Close the NDIS binding.
    //

    NdisBindingHandle = DeviceContext->NdisBindingHandle;
    ACQUIRE_SPIN_LOCK(&DeviceContext->SpinLock, &irql);    
    DeviceContext->NdisBindingHandle = NULL;
    RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, irql);
    
    if (NdisBindingHandle != NULL) {

        KeInitializeEvent(
            &DeviceContext->NdisRequestEvent,
            NotificationEvent,
            FALSE
        );

        NdisCloseAdapter(
            &ndisStatus,
            NdisBindingHandle);

        if (ndisStatus == NDIS_STATUS_PENDING) {

            IF_LPXDBG (LPX_DEBUG_NDIS) {
                LpxPrint0 ("Adapter close pended.\n");
            }

            //
            // The completion routine will set NdisRequestStatus.
            //

            KeWaitForSingleObject(
                &DeviceContext->NdisRequestEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );

            ndisStatus = DeviceContext->NdisRequestStatus;

            KeResetEvent(
                &DeviceContext->NdisRequestEvent
                );

        }
    }
    
    //
    // We ignore ndisStatus.
    //

#if 0
    //
    // Remove all the storage associated with the device.
    //

    LpxFreeResources (DeviceContext);

    NdisFreePacketPool (DeviceContext->SendPacketPoolHandle);
    NdisFreePacketPool (DeviceContext->ReceivePacketPoolHandle);
    NdisFreeBufferPool (DeviceContext->NdisBufferPoolHandle);
#endif

    // And remove creation ref if it has not already been removed
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
        // Remove creation reference
        LpxDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);
    }

}   /* LpxProcessStatusClosing */


VOID
LpxStatusComplete (
    IN NDIS_HANDLE NdisBindingContext
    )
{
    UNREFERENCED_PARAMETER (NdisBindingContext);
}

#if DBG

PUCHAR
LpxGetNdisStatus(
    NDIS_STATUS GeneralStatus
    )
/*++

Routine Description:

    This routine returns a pointer to the string describing the NDIS error
    denoted by GeneralStatus.

Arguments:

    GeneralStatus - the status you wish to make readable.

Return Value:

    None.

--*/
{
    static NDIS_STATUS Status[] = {
        NDIS_STATUS_SUCCESS,
        NDIS_STATUS_PENDING,

        NDIS_STATUS_ADAPTER_NOT_FOUND,
        NDIS_STATUS_ADAPTER_NOT_OPEN,
        NDIS_STATUS_ADAPTER_NOT_READY,
        NDIS_STATUS_ADAPTER_REMOVED,
        NDIS_STATUS_BAD_CHARACTERISTICS,
        NDIS_STATUS_BAD_VERSION,
        NDIS_STATUS_CLOSING,
        NDIS_STATUS_DEVICE_FAILED,
        NDIS_STATUS_FAILURE,
        NDIS_STATUS_INVALID_DATA,
        NDIS_STATUS_INVALID_LENGTH,
        NDIS_STATUS_INVALID_OID,
        NDIS_STATUS_INVALID_PACKET,
        NDIS_STATUS_MULTICAST_FULL,
        NDIS_STATUS_NOT_INDICATING,
        NDIS_STATUS_NOT_RECOGNIZED,
        NDIS_STATUS_NOT_RESETTABLE,
        NDIS_STATUS_NOT_SUPPORTED,
        NDIS_STATUS_OPEN_FAILED,
        NDIS_STATUS_OPEN_LIST_FULL,
        NDIS_STATUS_REQUEST_ABORTED,
        NDIS_STATUS_RESET_IN_PROGRESS,
        NDIS_STATUS_RESOURCES,
        NDIS_STATUS_UNSUPPORTED_MEDIA
    };
    static PUCHAR String[] = {
        "SUCCESS",
        "PENDING",

        "ADAPTER_NOT_FOUND",
        "ADAPTER_NOT_OPEN",
        "ADAPTER_NOT_READY",
        "ADAPTER_REMOVED",
        "BAD_CHARACTERISTICS",
        "BAD_VERSION",
        "CLOSING",
        "DEVICE_FAILED",
        "FAILURE",
        "INVALID_DATA",
        "INVALID_LENGTH",
        "INVALID_OID",
        "INVALID_PACKET",
        "MULTICAST_FULL",
        "NOT_INDICATING",
        "NOT_RECOGNIZED",
        "NOT_RESETTABLE",
        "NOT_SUPPORTED",
        "OPEN_FAILED",
        "OPEN_LIST_FULL",
        "REQUEST_ABORTED",
        "RESET_IN_PROGRESS",
        "RESOURCES",
        "UNSUPPORTED_MEDIA"
    };

    static UCHAR BadStatus[] = "UNDEFINED";
#define StatusCount (sizeof(Status)/sizeof(NDIS_STATUS))
    INT i;

    for (i=0; i<StatusCount; i++)
        if (GeneralStatus == Status[i])
            return String[i];
    return BadStatus;
#undef StatusCount
}
#endif

