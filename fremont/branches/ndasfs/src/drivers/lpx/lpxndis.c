/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

This module contains code which implements the routines used to interface
LPX and NDIS. All callback routines (except for Transfer Data,
Send Complete, and ReceiveIndication) are here, as well as those routines
called to initialize NDIS.

--*/
#include "precomp.h"
#pragma hdrstop

#ifdef LPX_LOCKS                // see spnlckdb.c

VOID
LpxFakeSendCompletionHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    );

VOID
LpxFakeTransferDataComplete (
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus,
    IN UINT BytesTransferred
    );

#endif


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

	// NDIS 3.0 field
    ProtChars.MajorNdisVersion = 4;
    ProtChars.MinorNdisVersion = 0;
    
    ProtChars.Name.Length = NameString->Length;
    ProtChars.Name.MaximumLength = NameString->MaximumLength;
    ProtChars.Name.Buffer = NameString->Buffer;

    ProtChars.OpenAdapterCompleteHandler = LpxOpenAdapterComplete;
    ProtChars.CloseAdapterCompleteHandler = LpxCloseAdapterComplete;
    ProtChars.ResetCompleteHandler = LpxResetComplete;
    ProtChars.RequestCompleteHandler = LpxRequestComplete;

#ifdef LPX_LOCKS
    ProtChars.SendCompleteHandler = LpxFakeSendCompletionHandler;
    ProtChars.TransferDataCompleteHandler = LpxFakeTransferDataComplete;
#else
    ProtChars.SendCompleteHandler = LpxSendCompletionHandler;
    ProtChars.TransferDataCompleteHandler = LpxTransferDataComplete;
#endif

    ProtChars.ReceiveHandler = LpxReceiveIndication;
    ProtChars.ReceiveCompleteHandler = LpxReceiveComplete;
    ProtChars.StatusHandler = LpxStatusIndication;
    ProtChars.StatusCompleteHandler = LpxStatusComplete;

	// NDIS 4.0 fields
	ProtChars.ReceivePacketHandler = LpxProtocolReceivePacket;
	ProtChars.BindAdapterHandler = LpxProtocolBindAdapter;
	ProtChars.UnbindAdapterHandler = LpxProtocolUnbindAdapter;
	ProtChars.PnPEventHandler = LpxProtocolPnPEventHandler;

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
    IN PNDIS_REQUEST Request,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine passed an NDIS_REQUEST to the MAC and waits
    until it has completed before returning the final status.

Arguments:

    DeviceContext - Pointer to the device context for this driver.

    Request - Pointer to the NDIS_REQUEST to submit.

    AdapterString - The name of the adapter, in case an error needs
        to be logged.

Return Value:

    The function value is the status of the operation.

--*/
{
    NDIS_STATUS NdisStatus;

    if (DeviceContext->NdisBindingHandle) {
        NdisRequest(
            &NdisStatus,
            DeviceContext->NdisBindingHandle,
            Request);
    }
    else {
        NdisStatus = STATUS_INVALID_DEVICE_STATE;
    }
    
    if (NdisStatus == NDIS_STATUS_PENDING) {

        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint1 ("OID %lx pended.\n",
                Request->DATA.QUERY_INFORMATION.Oid);
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
            if (Request->RequestType == NdisRequestSetInformation) {
                LpxPrint1 ("Lpxdrvr: Set OID %lx succeeded.\n",
                    Request->DATA.SET_INFORMATION.Oid);
            } else {
                LpxPrint1 ("Lpxdrvr: Query OID %lx succeeded.\n",
                    Request->DATA.QUERY_INFORMATION.Oid);
            }
        }

    } else {
#if DBG
        if (Request->RequestType == NdisRequestSetInformation) {
            LpxPrint2 ("Lpxdrvr: Set OID %lx failed: %s.\n",
                Request->DATA.SET_INFORMATION.Oid, LpxGetNdisStatus(NdisStatus));
        } else {
            LpxPrint2 ("Lpxdrvr: Query OID %lx failed: %s.\n",
                Request->DATA.QUERY_INFORMATION.Oid, LpxGetNdisStatus(NdisStatus));
        }
#endif

#if __LPX__
		if (NdisStatus != STATUS_INVALID_DEVICE_STATE && AdapterString) {
#endif        
        
            LpxWriteOidErrorLog(
                DeviceContext,
                Request->RequestType == NdisRequestSetInformation ?
                    EVENT_TRANSPORT_SET_OID_FAILED : EVENT_TRANSPORT_QUERY_OID_FAILED,
                NdisStatus,
                AdapterString->Buffer,
                Request->DATA.QUERY_INFORMATION.Oid);
        }
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
#if __LPX__
	NDIS_MEDIUM LpxSupportedMedia[] = { NdisMedium802_3 };
#endif
	UINT SelectedMedium;
    NDIS_REQUEST LpxRequest;
    UCHAR LpxDataBuffer[6];
    NDIS_OID LpxOid;
    UCHAR WanProtocolId[6] = { 0x80, 0x00, 0x00, 0x00, 0x80, 0xd5 };
    ULONG WanHeaderFormat = NdisWanHeaderEthernet;
	ULONG MinimumLookahead = 128;
    ULONG MacOptions;

#if __LPX__
	NDIS_MEDIA_STATE	ndisMediaState;
#endif

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
#if DBG
        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint2 ("Adapter open %S failed, status: %s.\n",
                AdapterString,
                LpxGetNdisStatus (NdisStatus));
        }
#endif
        LpxWriteGeneralErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_ADAPTER_NOT_FOUND,
            807,
            NdisStatus,
            AdapterString->Buffer,
            0,
            NULL);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Get the information we need about the adapter, based on
    // the media type.
    //

    MacInitializeMacInfo(
        LpxSupportedMedia[SelectedMedium],
        (BOOLEAN)(LpxConfig->UseDixOverEthernet != 0),
        &DeviceContext->MacInfo);
    DeviceContext->MacInfo.QueryWithoutSourceRouting =
        LpxConfig->QueryWithoutSourceRouting ? TRUE : FALSE;
    DeviceContext->MacInfo.AllRoutesNameRecognized =
        LpxConfig->AllRoutesNameRecognized ? TRUE : FALSE;


    //
    // Set the multicast/functional addresses first so we avoid windows where we
    // receive only part of the addresses.
    //

    MacSetNetBIOSMulticast (
            DeviceContext->MacInfo.MediumType,
            DeviceContext->NetBIOSAddress.Address);


    switch (DeviceContext->MacInfo.MediumType) {

    case NdisMedium802_3:
    case NdisMediumDix:

        //
        // Fill in the data for our multicast list.
        //

        RtlCopyMemory(LpxDataBuffer, DeviceContext->NetBIOSAddress.Address, 6);

        //
        // Now fill in the NDIS_REQUEST.
        //

        LpxRequest.RequestType = NdisRequestSetInformation;
        LpxRequest.DATA.SET_INFORMATION.Oid = OID_802_3_MULTICAST_LIST;
        LpxRequest.DATA.SET_INFORMATION.InformationBuffer = LpxDataBuffer;
        LpxRequest.DATA.SET_INFORMATION.InformationBufferLength = 6;

        break;

    case NdisMedium802_5:

        //
        // For token-ring, we pass the last four bytes of the
        // Netbios functional address.
        //

        //
        // Fill in the OVB for our functional address.
        //

        RtlCopyMemory(LpxDataBuffer, ((PUCHAR)(DeviceContext->NetBIOSAddress.Address)) + 2, 4);

        //
        // Now fill in the NDIS_REQUEST.
        //

        LpxRequest.RequestType = NdisRequestSetInformation;
        LpxRequest.DATA.SET_INFORMATION.Oid = OID_802_5_CURRENT_FUNCTIONAL;
        LpxRequest.DATA.SET_INFORMATION.InformationBuffer = LpxDataBuffer;
        LpxRequest.DATA.SET_INFORMATION.InformationBufferLength = 4;

        break;

    case NdisMediumFddi:

        //
        // Fill in the data for our multicast list.
        //

        RtlCopyMemory(LpxDataBuffer, DeviceContext->NetBIOSAddress.Address, 6);

        //
        // Now fill in the NDIS_REQUEST.
        //

        LpxRequest.RequestType = NdisRequestSetInformation;
        LpxRequest.DATA.SET_INFORMATION.Oid = OID_FDDI_LONG_MULTICAST_LIST;
        LpxRequest.DATA.SET_INFORMATION.InformationBuffer = LpxDataBuffer;
        LpxRequest.DATA.SET_INFORMATION.InformationBufferLength = 6;

        break;

    }

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }



    switch (DeviceContext->MacInfo.MediumType) {

    case NdisMedium802_3:
    case NdisMediumDix:

        if (DeviceContext->MacInfo.MediumAsync) {
            LpxOid = OID_WAN_CURRENT_ADDRESS;
        } else {
            LpxOid = OID_802_3_CURRENT_ADDRESS;
        }
        break;

    case NdisMedium802_5:

        LpxOid = OID_802_5_CURRENT_ADDRESS;
        break;

    case NdisMediumFddi:

        LpxOid = OID_FDDI_LONG_CURRENT_ADDR;
        break;

    default:

        NdisStatus = NDIS_STATUS_FAILURE;
        return STATUS_INVALID_PARAMETER;

    }
    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = LpxOid;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = DeviceContext->LocalAddress.Address;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set up the reserved Netbios address.
    //

    RtlZeroMemory(DeviceContext->ReservedNetBIOSAddress, 10);
    RtlCopyMemory(&DeviceContext->ReservedNetBIOSAddress[10], DeviceContext->LocalAddress.Address, 6);



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

    DeviceContext->CurSendPacketSize = DeviceContext->MaxSendPacketSize;


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

	//
    // Now query the link speed for non-wan media
    //

	LpxRequest.RequestType = NdisRequestQueryInformation;
	LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MEDIA_CONNECT_STATUS;
	LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &ndisMediaState;
	LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = sizeof(ndisMediaState);

	NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

	if (NdisStatus != NDIS_STATUS_SUCCESS) {

		ASSERT( FALSE );
		//SetFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED );
		//return STATUS_INSUFFICIENT_RESOURCES;

	} else {

		if (ndisMediaState == NdisMediaStateConnected) {

			SetFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED );
			ClearFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED );
		
		} else {

			ASSERT( ndisMediaState == NdisMediaStateDisconnected );

			SetFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED );
			ClearFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED );
		}

		DebugPrint( 0, ("DeviceContext = %p, ndisMediaState = %d\n", DeviceContext, ndisMediaState) );
	}

    if (!DeviceContext->MacInfo.MediumAsync) {

        LpxRequest.RequestType = NdisRequestQueryInformation;
        LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_LINK_SPEED;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MediumSpeed);
        LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        DeviceContext->MediumSpeedAccurate = TRUE;

        // Initialized MinimumT1Timeout in nbfdrvr.c
        // For a non-WAN media, this value is picked
        // from the registry, and remains constant.

        // DeviceContext->MinimumT1Timeout = 8;

    } else {

        //
        // On an wan media, this isn't valid until we get an
        // WAN_LINE_UP indication. Set the timeouts to
        // low values for now.
        //

        DeviceContext->MediumSpeedAccurate = FALSE;

        //
        // Use this until we know better.
        //

        DeviceContext->RecommendedSendWindow = 1;

    }

    //
    // For wan, specify our protocol ID and header format.
    // We don't query the medium subtype because we don't
    // case (since we require ethernet emulation).
    //

    if (DeviceContext->MacInfo.MediumAsync) {

        LpxRequest.RequestType = NdisRequestSetInformation;
        LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_PROTOCOL_TYPE;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = WanProtocolId;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

        NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }


        LpxRequest.RequestType = NdisRequestSetInformation;
        LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_WAN_HEADER_FORMAT;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &WanHeaderFormat;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
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

#if __LPX__

	DeviceContext->MacOptions = MacOptions;

#endif

    DeviceContext->MacInfo.CopyLookahead =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA) != 0);
    DeviceContext->MacInfo.ReceiveSerialized =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_RECEIVE_SERIALIZED) != 0);
    DeviceContext->MacInfo.TransferSynchronous =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND) != 0);
    DeviceContext->MacInfo.SingleReceive =
        (BOOLEAN)(DeviceContext->MacInfo.ReceiveSerialized && DeviceContext->MacInfo.TransferSynchronous);


#if 0
    //
    // Now set our options if needed.
    //
    // Don't allow early indications because we can't determine
    // if the CRC has been checked yet.
    //

    if ((DeviceContext->MacInfo.MediumType == NdisMedium802_3) ||
        (DeviceContext->MacInfo.MediumType == NdisMediumDix)) {

        ULONG ProtocolOptions = NDIS_PROT_OPTION_ESTIMATED_LENGTH;

        LpxRequest.RequestType = NdisRequestSetInformation;
        LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_PROTOCOL_OPTIONS;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &ProtocolOptions;
        LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

        NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    }
#endif

    //
    // Now that everything is set up, we enable the filter
    // for packet reception.
    //

    //
    // Fill in the OVB for packet filter.
    //

    switch (DeviceContext->MacInfo.MediumType) {

    case NdisMedium802_3:
    case NdisMediumDix:
    case NdisMediumFddi:

#if __LPX__
        RtlStoreUlong((PULONG)LpxDataBuffer,
            (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_BROADCAST));
#endif
		break;

    case NdisMedium802_5:

        RtlStoreUlong((PULONG)LpxDataBuffer,
            (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_FUNCTIONAL));
        break;

    default:

        NdisStatus = NDIS_STATUS_FAILURE;
        break;

    }

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
    
    //
    // Close the NDIS binding.
    //
    
    NdisBindingHandle = DeviceContext->NdisBindingHandle;
    
    DeviceContext->NdisBindingHandle = NULL;
        
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

#if __LPX__
    UNREFERENCED_PARAMETER(OpenErrorStatus);
#endif

#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint1 ("Lpxdrvr: LpxOpenAdapterCompleteNDIS Status: %s\n",
            LpxGetNdisStatus (NdisStatus));
    }
#endif

    ENTER_LPX;

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    LEAVE_LPX;
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

#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint1 ("Lpxdrvr: LpxCloseAdapterCompleteNDIS Status: %s\n",
            LpxGetNdisStatus (NdisStatus));
    }
#endif

    ENTER_LPX;

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    LEAVE_LPX;
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

#if __LPX__
	UNREFERENCED_PARAMETER(NdisRequest);
#endif

#if DBG
    IF_LPXDBG (LPX_DEBUG_NDIS) {
        LpxPrint2 ("Lpxdrvr: LpxRequestComplete request: %i, NDIS Status: %s\n",
            NdisRequest->RequestType,LpxGetNdisStatus (NdisStatus));
    }
#endif

    ENTER_LPX;

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    LEAVE_LPX;
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
    PNDIS_WAN_LINE_UP LineUp;
    KIRQL oldirql;

#if __LPX__
	KIRQL oldirql2;
   UNREFERENCED_PARAMETER(StatusBufferSize);
#endif

    DeviceContext = (PDEVICE_CONTEXT)NdisBindingContext;

	IF_LPXDBG (LPX_DEBUG_PNP) {
		LpxPrint1 ("LpxStatusIndication: NdisStatus %x\n", NdisStatus);
	}

#if !__LPX__
    KeRaiseIrql (DISPATCH_LEVEL, &oldirql);
#endif

    switch (NdisStatus) {

		case NDIS_STATUS_MEDIA_CONNECT:

			IF_LPXDBG (LPX_DEBUG_PNP) {

				LpxPrint0 ("LpxStatusIndication: NdisStatus = NDIS_STATUS_MEDIA_CONNECT\n");
			}

			ACQUIRE_SPIN_LOCK( &DeviceContext->LpxMediaFlagSpinLock, &oldirql2 );
			SetFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED );
			ClearFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED );
			RELEASE_SPIN_LOCK( &DeviceContext->LpxMediaFlagSpinLock, oldirql2 );

			if (SocketLpxPrimaryDeviceContext == NULL &&
				FlagOn(DeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) && 
				!FlagOn(DeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP)) {

				SocketLpxPrimaryDeviceContext = DeviceContext;
			}

		break;

		case NDIS_STATUS_MEDIA_DISCONNECT:

			IF_LPXDBG (LPX_DEBUG_PNP) {

				LpxPrint0 ("LpxStatusIndication: NdisStatus = NDIS_STATUS_MEDIA_DISCONNECT\n");
			}

			do {

				UINT			deviceCount;
				PLIST_ENTRY		listHead;
				PLIST_ENTRY		thisEntry;
				PDEVICE_CONTEXT deviceContext;

		        ACQUIRE_SPIN_LOCK( &DeviceContext->LpxMediaFlagSpinLock, &oldirql2 );
				ClearFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED );
				SetFlag( DeviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED );
				RELEASE_SPIN_LOCK( &DeviceContext->LpxMediaFlagSpinLock, oldirql2 );

				//
				//	'cause we have only Socket Lpx Device Context
				//

				if (SocketLpxPrimaryDeviceContext && DeviceContext != SocketLpxPrimaryDeviceContext) {

					break;
				}

				ACQUIRE_DEVICES_LIST_LOCK();

				if (IsListEmpty(&LpxDeviceList)) {

					SocketLpxPrimaryDeviceContext = NULL;
					RELEASE_DEVICES_LIST_LOCK ();
					break;
				}

				listHead = &LpxDeviceList;

				for (deviceCount = 0, thisEntry = listHead->Flink;
					 thisEntry != listHead;
					 thisEntry = thisEntry->Flink) {

					deviceContext = CONTAINING_RECORD( thisEntry, DEVICE_CONTEXT, Linkage );
		
					ACQUIRE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, &oldirql2);

					if (deviceContext != SocketLpxDeviceContext && 
						deviceContext->CreateRefRemoved == FALSE && 
						FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) &&
						!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP) &&
						FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED) &&
						!FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED)) {

						deviceCount++;
					}

					RELEASE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, oldirql2);
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

					ACQUIRE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, &oldirql2);

					if (deviceContext != SocketLpxDeviceContext && 
						deviceContext->CreateRefRemoved == FALSE && 
						FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) &&
						!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP) &&
						FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED) &&
						!FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED)) {

						RELEASE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, oldirql2);
						break;
					}

					RELEASE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, oldirql2);

				}
		
				ASSERT( deviceContext );
				ASSERT( DeviceContext->CreateRefRemoved == FALSE );
				SocketLpxPrimaryDeviceContext = deviceContext;
		
				RELEASE_DEVICES_LIST_LOCK();

			} while (0);

		break;

        case NDIS_STATUS_WAN_LINE_UP:

#if __LPX__
			KeRaiseIrql (DISPATCH_LEVEL, &oldirql);
#endif

            //
            // A wan line is connected.
            //

            ACQUIRE_DPC_SPIN_LOCK (&DeviceContext->SpinLock);

            //
            // If this happens before we are ready, then make
            // a note of it, otherwise make the device ready.
            //

            DeviceContext->MediumSpeedAccurate = TRUE;

			LineUp = (PNDIS_WAN_LINE_UP)StatusBuffer;

			//
			// See if this is a new lineup for this protocol type
			//
			if (LineUp->ProtocolType == 0x80D5) {
				NDIS_HANDLE	TransportHandle;

				*((ULONG UNALIGNED *)(&TransportHandle)) =
				*((ULONG UNALIGNED *)(&LineUp->LocalAddress[2]));

				//
				// See if this is a new lineup
				//
				if (TransportHandle == NULL) {
					*((ULONG UNALIGNED *)(&LineUp->LocalAddress[2])) = *((ULONG UNALIGNED *)(&DeviceContext));
//					ETH_COPY_NETWORK_ADDRESS(DeviceContext->LocalAddress.Address, LineUp->LocalAddress);
//					ETH_COPY_NETWORK_ADDRESS(&DeviceContext->ReservedNetBIOSAddress[10], DeviceContext->LocalAddress.Address);
				}

				//
				// Calculate minimum link timeouts based on the speed,
				// which is passed in StatusBuffer.
				//
				// The formula is (max_frame_size * 2) / speed + 0.4 sec.
				// This expands to
				//
				//   MFS (bytes) * 2       8 bits
				// -------------------  x  ------   == timeout (sec),
				// speed (100 bits/sec)     byte
				//
				// which is (MFS * 16 / 100) / speed. We then convert it into
				// the 50 ms units that LPX uses and add 8 (which is
				// 0.4 seconds in 50 ms units).
				//
				// As a default timeout we use the min + 0.2 seconds
				// unless the configured default is more.
				//
		
				if (LineUp->LinkSpeed > 0) {
					DeviceContext->MediumSpeed = LineUp->LinkSpeed;
				}
		
				if (LineUp->MaximumTotalSize > 0) {
#if DBG
					if (LineUp->MaximumTotalSize > DeviceContext->MaxSendPacketSize) {
						DbgPrint ("Lpx: Bad LINE_UP size, %d (> %d)\n",
							LineUp->MaximumTotalSize, DeviceContext->MaxSendPacketSize);
					}
					if (LineUp->MaximumTotalSize < 128) {
						DbgPrint ("LPX: Bad LINE_UP size, %d (< 128)\n",
							LineUp->MaximumTotalSize);
					}
#endif
					DeviceContext->CurSendPacketSize = LineUp->MaximumTotalSize;
				}
		
				if (LineUp->SendWindow == 0) {
					DeviceContext->RecommendedSendWindow = 3;
				} else {
					DeviceContext->RecommendedSendWindow = LineUp->SendWindow + 1;
				}
			}

            RELEASE_DPC_SPIN_LOCK (&DeviceContext->SpinLock);

#if __LPX__
			KeLowerIrql (oldirql);
#endif

            break;

        case NDIS_STATUS_WAN_LINE_DOWN:

#if __LPX__
			KeRaiseIrql (DISPATCH_LEVEL, &oldirql);
#endif

            //
            // An wan line is disconnected.
            //

            ACQUIRE_DPC_SPIN_LOCK (&DeviceContext->SpinLock);

            DeviceContext->MediumSpeedAccurate = FALSE;

            RELEASE_DPC_SPIN_LOCK (&DeviceContext->SpinLock);

#if __LPX__
			KeLowerIrql (oldirql);
#endif

            break;

        case NDIS_STATUS_WAN_FRAGMENT:

            break;

        case NDIS_STATUS_CLOSING:

#if __LPX__
			KeRaiseIrql (DISPATCH_LEVEL, &oldirql);
#endif

            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1 ("LpxStatusIndication: Device @ %08p Closing\n", DeviceContext);
            }

            //
            // The adapter is shutting down. We queue a worker
            // thread to handle this.
            //

#pragma warning(disable: 4995 4996)
            ExInitializeWorkItem(
                &DeviceContext->StatusClosingQueueItem,
                LpxProcessStatusClosing,
                (PVOID)DeviceContext);
            ExQueueWorkItem(&DeviceContext->StatusClosingQueueItem, DelayedWorkQueue);
#pragma warning(default: 4995 4996)

#if __LPX__
			KeLowerIrql (oldirql);
#endif

            break;

        default:
            break;

    }
#if !__LPX__
    KeLowerIrql (oldirql);
#endif
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

    DeviceContext = (PDEVICE_CONTEXT)Parameter;

    //
    // Prevent new activity on the connection.
    //

    DeviceContext->State = DEVICECONTEXT_STATE_DOWN;

	//
    // Close the NDIS binding.
    //

    NdisBindingHandle = DeviceContext->NdisBindingHandle;
    
    DeviceContext->NdisBindingHandle = NULL;
        
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
    
    // And remove creation ref if it has not already been removed
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
    
        // Stop all internal timers
        LpxStopTimerSystem(DeviceContext);

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
