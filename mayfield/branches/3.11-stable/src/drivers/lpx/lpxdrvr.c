 Pointer to the request packet representing the I/O request.

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
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MacOptions);
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
#if 1
        return STATUS_INSUFFICIENT_RESOURCES;
#else
        DeviceContext->MacOptions = 0;
#endif
    }
#if DBG
	if(DeviceContext->MacOptions & NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA) {
		LpxPrint0("No CopyLookaheadData restriction\n");
	} else {
		LpxPrint0("CopyLookaheadData restriction applied\n");
	}
	if(DeviceContext->MacOptions & NDIS_MAC_OPTION_8021P_PRIORITY) {
		LpxPrint0("802.1p enabled.\n");
	} else {
		LpxPrint0("802.1p disabled.\n");
	}
#endif

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
    LpxRequest.DATA.SET_INFORMATION.InformationBuffer = LpxDataBuffer;
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
                LpxPrint1 ("LpxStatusIndication: Device @ %p Closing\n", DeviceContext);
            }

            //
            // The adapter is shutting down. We queue a worker
            // thread to handle this.
            //

            IoQueueWorkItem(DeviceContext->StatusClosingQueueItem, LpxProcessStatusClosing, DelayedWorkQueue, (PVOID)DeviceContext);

            break;

        default:
            break;

    }

    KeLowerIrql (oldirql);

}


VOID
LpxProcessStatusClosing(
    IN PDEVICE_OBJECT DeviceObject,
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
    UNREFERENCED_PARAMETER(DeviceObject);
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
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    /*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop

LONG	NumberOfPackets = 0;

ULONG	NumberOfSent = 0;
ULONG	NumberOfSentComplete = 0;

//////////////////////////////////////////////////////////////////////////
//
//	Packet allocation functions
//


NTSTATUS
PacketAllocate(
			   IN	PSERVICE_POINT		ServicePoint,
			   IN	ULONG				PacketLength,
			   IN	PDEVICE_CONTEXT		DeviceContext,
			   IN	UCHAR				Type,
			   IN	PUCHAR				CopyData,
			   IN	ULONG				CopyDataLength,
			   IN	PIO_STACK_LOCATION	IrpSp,
			   OUT	PNDIS_PACKET		*Packet
			   )
{
	NTSTATUS		status;
	PUCHAR			packetData;
	PNDIS_BUFFER	pNdisBuffer;
	PNDIS_BUFFER	pNdisBufferData;
	PNDIS_PACKET	packet;
	USHORT			port;
//        ASSERT( LpxControlDeviceContext != DeviceContext) ;
	DebugPrint(3, ("PacketAllocate, PacketLength = %d, Numberofpackets = %d\n", PacketLength, NumberOfPackets));
	
	//
	//	Check the prerequisite
	//
	if(DeviceContext == NULL) {
		DebugPrint(1, ("[LPX]PacketAllocate: DeviceContext is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(DeviceContext->LpxPacketPool == NULL) {
		DebugPrint(1, ("[LPX]PacketAllocate: DeviceContext->LpxPacketPool is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Allocate packet descriptor
	//
	NdisAllocatePacket(&status,	&packet, DeviceContext->LpxPacketPool);

	if(status != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("[LPX]PacketAllocate: NdisAllocatePacket Failed!!!\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ASSERT( packet->Private.NdisPacketFlags & fPACKET_ALLOCATED_BY_NDIS);

	//
	//	Allocate memory for packet header
	//
	status = LpxAllocateMemoryWithLpxTag(
		&packetData,
		PacketLength
		);
	if(status != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("[LpxSmp]PacketAllocate: Can't Allocate Memory packet.\n"));

		NdisFreePacket(packet);
		*Packet = NULL;

		return status;
	}
	
	//
	//	Create the Ndis buffer desc from header memory for LPX packet header.
	//
	NdisAllocateBuffer(
		&status,
		&pNdisBuffer,
		DeviceContext->LpxBufferPool,
		packetData,
		PacketLength
		);
	if(!NT_SUCCESS(status)) {
		NdisFreePacket(packet);
		*Packet = NULL;
		LpxFreeMemoryWithLpxTag(packetData);
		DebugPrint(1, ("[LPX]PacketAllocate: Can't Allocate Buffer!!!\n"));

		return status;
	}

	//
	//	Create the Ndis buffer desc from data memory for LPX packet body.
	//
	switch(Type) {
		
	case SEND_TYPE:

		//
		//	Initialize Ethernet header in Xerox spec.
		//
		if(ServicePoint) {
			RtlCopyMemory(&packetData[0],
				ServicePoint->DestinationAddress.Node,
				ETHERNET_ADDRESS_LENGTH
				);
			RtlCopyMemory(&packetData[ETHERNET_ADDRESS_LENGTH],
				ServicePoint->SourceAddress.Node,
				ETHERNET_ADDRESS_LENGTH
				);
			port = HTONS(ETH_P_LPX);
			RtlCopyMemory(&packetData[ETHERNET_ADDRESS_LENGTH*2],
				&port, //&ServicePoint->DestinationAddress.Port,
				2
				);
			// Clear header field to 0 to prevent reserved field is not initialized.
			RtlZeroMemory(&packetData[ETHERNET_HEADER_LENGTH], sizeof(LPX_HEADER2));
		}
		
		if(CopyDataLength) {
			
			NdisAllocateBuffer(
				&status,
				&pNdisBufferData,
				DeviceContext->LpxBufferPool,
				CopyData,
				CopyDataLength
				);
			if(!NT_SUCCESS(status)) {
				NdisFreePacket(packet);
				*Packet = NULL;
				LpxFreeMemoryWithLpxTag(packetData);
				DebugPrint(1, ("[LPX]PacketAllocate: Can't Allocate Buffer For CopyData!!!\n"));

				return status;
			}

			NdisChainBufferAtFront(packet, pNdisBufferData);
		}
		break;

	case RECEIVE_TYPE:

		NdisMoveMappedMemory(
			packetData,
			CopyData,
			CopyDataLength
			);

		break;
	}

	//
	//	Initialize packet descriptor
	//

	//	RESERVED(packet)->ServicePoint = ServicePoint;
	RtlZeroMemory(RESERVED(packet), sizeof(LPX_RESERVED));
	RESERVED(packet)->Cloned = 0;
	RESERVED(packet)->IrpSp = IrpSp;
	RESERVED(packet)->Type = Type;
	RESERVED(packet)->LpxSmpHeader = NULL;

	if(IrpSp == NULL) {
		DebugPrint(3, ("[LPX] PacketAllocate: No IrpSp\n")) ;
	}

	//
	//	Insert packet header to the front.
	//
	NdisChainBufferAtFront(packet, pNdisBuffer);

	InterlockedIncrement(&NumberOfPackets);

	*Packet = packet;
	return STATUS_SUCCESS;
}


VOID
PacketFree(
		   IN PNDIS_PACKET	Packet
		   )
{
	PLPX_RESERVED	reserved = RESERVED(Packet);
	PUCHAR			packetData;
	PNDIS_BUFFER	pNdisBuffer;
	UINT			uiLength;
	LONG			clone ;
	LONG			BufferSeq;

	DebugPrint(3, ("PacketFree reserved->type = %d\n", reserved->Type));
	ASSERT( Packet->Private.NdisPacketFlags & fPACKET_ALLOCATED_BY_NDIS);

	switch(reserved->Type) {

	case SEND_TYPE:

		clone = InterlockedDecrement(&reserved->Cloned);
		if(clone >= 0) {
			return;
		}
	
		pNdisBuffer = NULL;
		NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		if(pNdisBuffer) {
			
			NdisQueryBufferSafe(
				pNdisBuffer,
				&packetData,
				&uiLength,
				HighPagePriority 
				);
		
			LpxFreeMemoryWithLpxTag(packetData);
			NdisFreeBuffer(pNdisBuffer);
		}
		pNdisBuffer = NULL;
		BufferSeq = 0;
		NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		while(pNdisBuffer) {

			//
			//	Assuming the first data buffer comes from user application
			//			the others are created in LPX for padding, etc.
			//	Free the memory of the others.
			//
			if(BufferSeq > 0) {
				NdisQueryBufferSafe(
					pNdisBuffer,
					&packetData,
					&uiLength,
					HighPagePriority 
				);
				LpxFreeMemoryWithLpxTag(packetData);
			}

			NdisFreeBuffer(pNdisBuffer);
			pNdisBuffer = NULL;
			NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
			BufferSeq ++;
		}
		if(reserved->IrpSp != NULL) {
			{
				PIRP _Irp = IRP_SEND_IRP(reserved->IrpSp);
				INC_IRP_RETRANSMITS(_Irp, reserved->Retransmits);
			}

            LpxDereferenceSendIrp("Destroy packet", reserved->IrpSp, RREF_PACKET);
		} else {
			DebugPrint(3, ("[LPX] PacketFree: No IrpSp\n")) ;
		}
		break;

	case RECEIVE_TYPE:

		if(reserved->LpxSmpHeader)
			LpxFreeMemoryWithLpxTag(reserved->LpxSmpHeader);

		pNdisBuffer = NULL;	
		NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		if(pNdisBuffer) {
			NdisQueryBufferSafe(
				pNdisBuffer,
				&packetData,
				&uiLength,
				HighPagePriority 
				);
			
			LpxFreeMemoryWithLpxTag(packetData);
			NdisFreeBuffer(pNdisBuffer);
		}
		reserved->PacketDataOffset = 0;

		break;
	}

	ASSERT( Packet->Private.NdisPacketFlags & fPACKET_ALLOCATED_BY_NDIS);
	NdisFreePacket(Packet);

	InterlockedDecrement(&NumberOfPackets);

	DebugPrint(3, ("Packet REALLY Freed Numberofpackets = %d\n", NumberOfPackets));
}


PNDIS_PACKET
PacketClone(
	IN	PNDIS_PACKET Packet
	)
{
	InterlockedIncrement(&(RESERVED(Packet)->Cloned));

	return Packet;
}

PNDIS_PACKET
PacketCopy(
	IN	PNDIS_PACKET Packet,
	OUT	PLONG	Cloned
	)
{
	*Cloned = InterlockedIncrement(&(RESERVED(Packet)->Cloned));

	return Packet;
}

//////////////////////////////////////////////////////////////////////////
//
//	Packet queue utility functions
//
//

PNDIS_PACKET
PacketDequeue(
			  PLIST_ENTRY	PacketQueue,
			  PKSPIN_LOCK	QSpinLock
			  )
{
	PLIST_ENTRY		packetListEntry = NULL;
	PLPX_RESERVED	reserved;
	PNDIS_PACKET	packet;
	
	DebugPrint(4, ("PacketDequeue\n"));
	
	if(QSpinLock) {
		packetListEntry = ExInterlockedRemoveHeadList(
			PacketQueue,
			QSpinLock
			);
		
	} else {
		if(IsListEmpty(PacketQueue))
			packetListEntry = NULL;
		else
			packetListEntry = RemoveHeadList(PacketQueue);
	}
	
	if(packetListEntry) {
		reserved = CONTAINING_RECORD(packetListEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
	} else
		packet = NULL;
	
	return packet;
}


BOOLEAN
PacketQueueEmpty(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	)
{
	PLIST_ENTRY		packetListEntry;
	KIRQL			oldIrql;

	if(QSpinLock) {
		ACQUIRE_SPIN_LOCK(QSpinLock, &oldIrql);
		packetListEntry = PacketQueue->Flink;
		RELEASE_SPIN_LOCK(QSpinLock, oldIrql);
	}else
		packetListEntry = PacketQueue->Flink;

	return (packetListEntry == PacketQueue);
}

PNDIS_PACKET
PacketPeek(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	)	
{
	PLIST_ENTRY		packetListEntry;
	PLPX_RESERVED	reserved;
	PNDIS_PACKET	packet;
	KIRQL			oldIrql;

	if(QSpinLock) {
		KeAcquireSpinLock(QSpinLock, &oldIrql);
		packetListEntry = PacketQueue->Flink;
		KeReleaseSpinLock(QSpinLock, oldIrql);
	}else
		packetListEntry = PacketQueue->Flink;

	if(packetListEntry == PacketQueue)
		packetListEntry = NULL;

	if(packetListEntry) {
		reserved = CONTAINING_RECORD(packetListEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		ASSERT(packet->Private.NdisPacketFlags & fPACKET_ALLOCATED_BY_NDIS);
	} else
		packet = NULL;

	return packet;
}


                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         ò    0       ÄÕP           òÅ  0    ˆ  ÕP           òÅ  0   ˆ  ÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP           ò    0       ÄÕP               0       ÄG            ò    0       ÄÂ|            ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ©    0       ÄÂ|           ô    0       ÄÂ|          ò    0       ÄÂ|            ò    0       ÄÂ|            ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ò    0       ÄÂ|           ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ©    0       ÄÂ|         ( ô    0       ÄÂ|        ( ©    0            22 àˇˇ∞755 àˇˇ                                     @ 22 àˇˇ@ 22 àˇˇ`≈!Åˇˇˇˇ                –q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          22 àˇˇh22 àˇˇ ;55 àˇˇ                                     ®22 àˇˇ®22 àˇˇ`≈!Åˇˇˇˇ                †q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        h22 àˇˇ–22 àˇˇ†x
 àˇˇ                                     	22 àˇˇ	22 àˇˇ`≈!Åˇˇˇˇ                ∞q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        –22 àˇˇ822 àˇˇ#x
 àˇˇ                                     x22 àˇˇx22 àˇˇ`≈!Åˇˇˇˇ                ` q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        822 àˇˇ†22 àˇˇ)x
 àˇˇ                                     ‡22 àˇˇ‡22 àˇˇ`≈!Åˇˇˇˇ                Äq4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        †22 àˇˇ22 àˇˇ∞7x
 àˇˇ                                     H22 àˇˇH22 àˇˇ`≈!Åˇˇˇˇ                @ q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        22 àˇˇp22 àˇˇ ;x
 àˇˇ                                     ∞22 àˇˇ∞22 àˇˇ`≈!Åˇˇˇˇ                p q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        p22 àˇˇÿ22 àˇˇ`-x
 àˇˇ                                      22 àˇˇ22 àˇˇ`≈!Åˇˇˇˇ                –q4 àˇˇ`q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ÿ22 àˇˇ@#22 àˇˇ–0x
 àˇˇ                                      Ä#22 àˇˇÄ#22 àˇˇ`≈!Åˇˇˇˇ                pq4 àˇˇêq4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                @#22 àˇˇ®'22 àˇˇ@4x
 àˇˇ                                     Ë'22 àˇˇË'22 àˇˇ`≈!Åˇˇˇˇ                –q4 àˇˇ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        ®'22