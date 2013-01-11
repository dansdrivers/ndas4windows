#include "precomp.h"
#pragma hdrstop

LONG	DebugLevel = 2;

PDEVICE_CONTEXT  SocketLpxDeviceContext;
PDEVICE_CONTEXT  SocketLpxPrimaryDeviceContext;


ULONG
SocketNbfInitializeOneDeviceContext(
	OUT PNDIS_STATUS	NdisStatus,
    IN	PDRIVER_OBJECT	DriverObject,
    IN  PCONFIG_DATA	NbfConfig,
    IN  PUNICODE_STRING BindName,
    IN  PUNICODE_STRING ExportName,
    IN  PVOID			SystemSpecific1,
    IN  PVOID			SystemSpecific2,
    OUT PDEVICE_CONTEXT *SocketLpxDeviceContext
    )
/*++

Routine Description:

    This routine creates and initializes one nbf device context.  In order to
    do this it must successfully open and bind to the adapter described by
    nbfconfig->names[adapterindex].

Arguments:

    NdisStatus   - The outputted status of the operations.

    DriverObject - the nbf driver object.

    NbfConfig    - the transport configuration information from the registry.

    SystemSpecific1 - SystemSpecific1 argument to ProtocolBindAdapter

    SystemSpecific2 - SystemSpecific2 argument to ProtocolBindAdapter

Return Value:

    The number of successful binds.

--*/

{
    ULONG i;
    PDEVICE_CONTEXT DeviceContext;
    PTP_REQUEST Request;
    PTP_LINK Link;
    PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
//    PTP_UI_FRAME UIFrame;
//    PTP_PACKET Packet;
//    PNDIS_PACKET NdisPacket;
//    PRECEIVE_PACKET_TAG ReceiveTag;
//    PBUFFER_TAG BufferTag;
//    KIRQL oldIrql;
    NTSTATUS status;
    UINT MaxUserData;
//    ULONG InitReceivePackets;
    BOOLEAN UniProcessor;
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceString;
    UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_NETBIOS];
    PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
    PTDI_ADDRESS_NETBIOS NetBIOSAddress =
                                    (PTDI_ADDRESS_NETBIOS)pAddress->Address;
    struct {
        TDI_PNP_CONTEXT tdiPnPContextHeader;
        PVOID           tdiPnPContextTrailer;
    } tdiPnPContext1, tdiPnPContext2;

    pAddress->AddressLength = TDI_ADDRESS_LENGTH_NETBIOS;
    pAddress->AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    NetBIOSAddress->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;

    //
    // Determine if we are on a uniprocessor.
    //

#if (WINVER == 0x0500)
    if (*KeNumberProcessors == 1) {
#else
    if (KeNumberProcessors == 1) {
#endif
        UniProcessor = TRUE;
    } else {
        UniProcessor = FALSE;
    }

    //
    // Loop through all the adapters that are in the configuration
    // information structure. Allocate a device object for each
    // one that we find.
    //

    status = NbfCreateDeviceContext(
				DriverObject,
                ExportName,
                &DeviceContext
                );

    if (!NT_SUCCESS (status)) {

        IF_NBFDBG (NBF_DEBUG_PNP) {
            NbfPrint2 ("NbfCreateDeviceContext for %S returned error %08x\n",
                            ExportName->Buffer, status);
        }

		//
		// First check if we already have an object with this name
		// This is because a previous unbind was not done properly.
		//

    	if (status == STATUS_OBJECT_NAME_COLLISION) {

			// See if we can reuse the binding and device name
			
			NbfReInitializeDeviceContext(
                                         &status,
                                         DriverObject,
                                         NbfConfig,
                                         BindName,
                                         ExportName,
                                         SystemSpecific1,
                                         SystemSpecific2
                                        );

			if (status == STATUS_NOT_FOUND)
			{
				// Must have got deleted in the meantime
			
				return NbfInitializeOneDeviceContext(
                                                     NdisStatus,
                                                     DriverObject,
                                                     NbfConfig,
                                                     BindName,
                                                     ExportName,
                                                     SystemSpecific1,
                                                     SystemSpecific2
                                                    );
			}
		}

	    *NdisStatus = status;

		if (!NT_SUCCESS (status))
		{
	        NbfWriteGeneralErrorLog(
    	        (PVOID)DriverObject,
        	    EVENT_TRANSPORT_BINDING_FAILED,
	            707,
    	        status,
        	    BindName->Buffer,
	            0,
    	        NULL);

            return(0);
		}
		
    	return(1);
	}


{
	UNICODE_STRING  unicodeDeviceName;

    RtlInitUnicodeString(&unicodeDeviceName, SOCKETLPX_DOSDEVICE_NAME);
    IoCreateSymbolicLink(&unicodeDeviceName, ExportName);
}

#if 0
	do
	{
	    UNICODE_STRING  unicodeDeviceName;
		NTSTATUS		ntStatus;
	    PWSTR           deviceNameStr = L"\\DosDevices\\SocketLpx";


		unicodeDeviceName.Buffer = NULL;
//		deviceNameStr = ExAllocatePool(NonPagedPool, 200);
//		if (!deviceNameStr) {
//			DebugPrint(1, ("Memory allocation for create symbolic failed\n"));
//  /          break;
//		}
        
//		swprintf(deviceNameStr, "%ws", "");
		RtlInitUnicodeString(&unicodeDeviceName, deviceNameStr);

		ntStatus = IoCreateSymbolicLink(
					(PUNICODE_STRING) &unicodeDeviceName,
					ExportName
					);

		if (ntStatus != STATUS_SUCCESS) {
			DebugPrint(1, ("Create symbolic failed\n"));
			break;
		}
       
//		ExFreePool(unicodeDeviceName.Buffer);
//		unicodeDeviceName.Buffer = NULL;

	} while(0);

#endif
	
	DeviceContext->UniProcessor = UniProcessor;

    //
    // Initialize the timer and retry values (note that the link timeouts
    // are converted from NT ticks to NBF ticks). These values may
    // be modified by NbfInitializeNdis.
    //
    DeviceContext->MinimumT1Timeout = NbfConfig->MinimumT1Timeout / SHORT_TIMER_DELTA;
    DeviceContext->DefaultT1Timeout = NbfConfig->DefaultT1Timeout / SHORT_TIMER_DELTA;
    DeviceContext->DefaultT2Timeout = NbfConfig->DefaultT2Timeout / SHORT_TIMER_DELTA;
    DeviceContext->DefaultTiTimeout = NbfConfig->DefaultTiTimeout / LONG_TIMER_DELTA;
    DeviceContext->LlcRetries = NbfConfig->LlcRetries;
    DeviceContext->LlcMaxWindowSize = NbfConfig->LlcMaxWindowSize;
    DeviceContext->MaxConsecutiveIFrames = (UCHAR)NbfConfig->MaximumIncomingFrames;
    DeviceContext->NameQueryRetries = NbfConfig->NameQueryRetries;
    DeviceContext->NameQueryTimeout = NbfConfig->NameQueryTimeout;
    DeviceContext->AddNameQueryRetries = NbfConfig->AddNameQueryRetries;
    DeviceContext->AddNameQueryTimeout = NbfConfig->AddNameQueryTimeout;
    DeviceContext->GeneralRetries = NbfConfig->GeneralRetries;
    DeviceContext->GeneralTimeout = NbfConfig->GeneralTimeout;
    DeviceContext->MinimumSendWindowLimit = NbfConfig->MinimumSendWindowLimit;

    //
    // Initialize our counter that records memory usage.
    //

    DeviceContext->MemoryUsage = 0;
    DeviceContext->MemoryLimit = NbfConfig->MaxMemoryUsage;

    DeviceContext->MaxRequests = NbfConfig->MaxRequests;
    DeviceContext->MaxLinks = NbfConfig->MaxLinks;
    DeviceContext->MaxConnections = NbfConfig->MaxConnections;
    DeviceContext->MaxAddressFiles = NbfConfig->MaxAddressFiles;
    DeviceContext->MaxAddresses = NbfConfig->MaxAddresses;

#if 0

    //
    // Now fire up NDIS so this adapter talks
    //

    status = NbfInitializeNdis (DeviceContext,
                                NbfConfig,
                                BindName);

    if (!NT_SUCCESS (status)) {

        //
        // Log an error if we were failed to
        // open this adapter.
        //

        NbfWriteGeneralErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_BINDING_FAILED,
            601,
            status,
            BindName->Buffer,
            0,
            NULL);

        if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
            NbfDereferenceDeviceContext ("Initialize NDIS failed", DeviceContext, DCREF_CREATION);
        }
        
        *NdisStatus = status;
        return(0);

    }

#if 0
    DbgPrint("Opened %S as %S\n", &NbfConfig->Names[j], &nameString);
#endif

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint6 ("NbfInitialize: NDIS returned: %x %x %x %x %x %x as local address.\n",
            DeviceContext->LocalAddress.Address[0],
            DeviceContext->LocalAddress.Address[1],
            DeviceContext->LocalAddress.Address[2],
            DeviceContext->LocalAddress.Address[3],
            DeviceContext->LocalAddress.Address[4],
            DeviceContext->LocalAddress.Address[5]);
    }

    //
    // Initialize our provider information structure; since it
    // doesn't change, we just keep it around and copy it to
    // whoever requests it.
    //


#endif

    MacReturnMaxDataSize(
        &DeviceContext->MacInfo,
        NULL,
        0,
        DeviceContext->MaxSendPacketSize,
        TRUE,
        &MaxUserData);

    DeviceContext->Information.Version = 0x0100;
    DeviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    DeviceContext->Information.MaxConnectionUserData = 0;
    DeviceContext->Information.MaxDatagramSize =
        MaxUserData - (sizeof(DLC_FRAME) + sizeof(NBF_HDR_CONNECTIONLESS));
    DeviceContext->Information.ServiceFlags = NBF_SERVICE_FLAGS;
    if (DeviceContext->MacInfo.MediumAsync) {
        DeviceContext->Information.ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
    }
    DeviceContext->Information.MinimumLookaheadData =
        240 - (sizeof(DLC_FRAME) + sizeof(NBF_HDR_CONNECTIONLESS));
    DeviceContext->Information.MaximumLookaheadData =
        DeviceContext->MaxReceivePacketSize - (sizeof(DLC_I_FRAME) + sizeof(NBF_HDR_CONNECTION));
    DeviceContext->Information.NumberOfResources = NBF_TDI_RESOURCES;
    KeQuerySystemTime (&DeviceContext->Information.StartTime);


    //
    // Allocate various structures we will need.
    //

    ENTER_NBF;

    //
    // The TP_UI_FRAME structure has a CHAR[1] field at the end
    // which we expand upon to include all the headers needed;
    // the size of the MAC header depends on what the adapter
    // told us about its max header size.
    //

    DeviceContext->UIFrameHeaderLength =
        DeviceContext->MacInfo.MaxHeaderLength +
        sizeof(DLC_FRAME) +
        sizeof(NBF_HDR_CONNECTIONLESS);

    DeviceContext->UIFrameLength =
        FIELD_OFFSET(TP_UI_FRAME, Header[0]) +
        DeviceContext->UIFrameHeaderLength;


    //
    // The TP_PACKET structure has a CHAR[1] field at the end
    // which we expand upon to include all the headers needed;
    // the size of the MAC header depends on what the adapter
    // told us about its max header size. TP_PACKETs are used
    // for connection-oriented frame as well as for
    // control frames, but since DLC_I_FRAME and DLC_S_FRAME
    // are the same size, the header is the same size.
    //

    ASSERT (sizeof(DLC_I_FRAME) == sizeof(DLC_S_FRAME));

    DeviceContext->PacketHeaderLength =
        DeviceContext->MacInfo.MaxHeaderLength +
        sizeof(DLC_I_FRAME) +
        sizeof(NBF_HDR_CONNECTION);

    DeviceContext->PacketLength =
        FIELD_OFFSET(TP_PACKET, Header[0]) +
        DeviceContext->PacketHeaderLength;


    //
    // The BUFFER_TAG structure has a CHAR[1] field at the end
    // which we expand upong to include all the frame data.
    //

    DeviceContext->ReceiveBufferLength =
        DeviceContext->MaxReceivePacketSize +
        FIELD_OFFSET(BUFFER_TAG, Buffer[0]);


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: pre-allocating requests.\n");
    }
    for (i=0; i<NbfConfig->InitRequests; i++) {

        NbfAllocateRequest (DeviceContext, &Request);

        if (Request == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate requests.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->RequestPool, &Request->Linkage);
#if DBG
        NbfRequestTable[i+1] = (PVOID)Request;
#endif
    }
#if DBG
    NbfRequestTable[0] = UlongToPtr(NbfConfig->InitRequests);
    NbfRequestTable[NbfConfig->InitRequests + 1] = (PVOID)
                        ((NBF_REQUEST_SIGNATURE << 16) | sizeof (TP_REQUEST));
    InitializeListHead (&NbfGlobalRequestList);
#endif

    DeviceContext->RequestInitAllocated = NbfConfig->InitRequests;
    DeviceContext->RequestMaxAllocated = NbfConfig->MaxRequests;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d requests, %ld\n", NbfConfig->InitRequests, DeviceContext->MemoryUsage);
    }

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating links.\n");
    }
    for (i=0; i<NbfConfig->InitLinks; i++) {

        NbfAllocateLink (DeviceContext, &Link);

        if (Link == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate links.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->LinkPool, &Link->Linkage);
#if DBG
        NbfLinkTable[i+1] = (PVOID)Link;
#endif
    }
#if DBG
    NbfLinkTable[0] = UlongToPtr(NbfConfig->InitLinks);
    NbfLinkTable[NbfConfig->InitLinks+1] = (PVOID)
                ((NBF_LINK_SIGNATURE << 16) | sizeof (TP_LINK));
#endif

    DeviceContext->LinkInitAllocated = NbfConfig->InitLinks;
    DeviceContext->LinkMaxAllocated = NbfConfig->MaxLinks;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d links, %ld\n", NbfConfig->InitLinks, DeviceContext->MemoryUsage);
    }

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating connections.\n");
    }
    for (i=0; i<NbfConfig->InitConnections; i++) {

        NbfAllocateConnection (DeviceContext, &Connection);

        if (Connection == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate connections.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->ConnectionPool, &Connection->LinkList);
#if DBG
        NbfConnectionTable[i+1] = (PVOID)Connection;
#endif
    }
#if DBG
    NbfConnectionTable[0] = UlongToPtr(NbfConfig->InitConnections);
    NbfConnectionTable[NbfConfig->InitConnections+1] = (PVOID)
                ((NBF_CONNECTION_SIGNATURE << 16) | sizeof (TP_CONNECTION));
#endif

    DeviceContext->ConnectionInitAllocated = NbfConfig->InitConnections;
    DeviceContext->ConnectionMaxAllocated = NbfConfig->MaxConnections;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d connections, %ld\n", NbfConfig->InitConnections, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating AddressFiles.\n");
    }
    for (i=0; i<NbfConfig->InitAddressFiles; i++) {

        NbfAllocateAddressFile (DeviceContext, &AddressFile);

        if (AddressFile == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate Address Files.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
#if DBG
        NbfAddressFileTable[i+1] = (PVOID)AddressFile;
#endif
    }
#if DBG
    NbfAddressFileTable[0] = UlongToPtr(NbfConfig->InitAddressFiles);
    NbfAddressFileTable[NbfConfig->InitAddressFiles + 1] = (PVOID)
                            ((NBF_ADDRESSFILE_SIGNATURE << 16) |
                                 sizeof (TP_ADDRESS_FILE));
#endif

    DeviceContext->AddressFileInitAllocated = NbfConfig->InitAddressFiles;
    DeviceContext->AddressFileMaxAllocated = NbfConfig->MaxAddressFiles;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d address files, %ld\n", NbfConfig->InitAddressFiles, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating addresses.\n");
    }
    for (i=0; i<NbfConfig->InitAddresses; i++) {

        NbfAllocateAddress (DeviceContext, &Address);
        if (Address == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate addresses.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
#if DBG
        NbfAddressTable[i+1] = (PVOID)Address;
#endif
    }
#if DBG
    NbfAddressTable[0] = UlongToPtr(NbfConfig->InitAddresses);
    NbfAddressTable[NbfConfig->InitAddresses + 1] = (PVOID)
                        ((NBF_ADDRESS_SIGNATURE << 16) | sizeof (TP_ADDRESS));
#endif

    DeviceContext->AddressInitAllocated = NbfConfig->InitAddresses;
    DeviceContext->AddressMaxAllocated = NbfConfig->MaxAddresses;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d addresses, %ld\n", NbfConfig->InitAddresses, DeviceContext->MemoryUsage);
    }

#if 0

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating UI frames.\n");
    }

    for (i=0; i<NbfConfig->InitUIFrames; i++) {

        NbfAllocateUIFrame (DeviceContext, &UIFrame);

        if (UIFrame == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate UI frames.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&(DeviceContext->UIFramePool), &UIFrame->Linkage);
#if DBG
        NbfUiFrameTable[i+1] = UIFrame;
#endif
    }
#if DBG
        NbfUiFrameTable[0] = (PVOID)NbfConfig->InitUIFrames;
#endif

    DeviceContext->UIFrameInitAllocated = NbfConfig->InitUIFrames;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d UI frames, %ld\n", NbfConfig->InitUIFrames, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating I frames.\n");
        NbfPrint1 ("NBFDRVR: Packet pool header: %lx\n",&DeviceContext->PacketPool);
    }

    for (i=0; i<NbfConfig->InitPackets; i++) {

        NbfAllocateSendPacket (DeviceContext, &Packet);
        if (Packet == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate packets.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        PushEntryList (&DeviceContext->PacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
#if DBG
        NbfSendPacketTable[i+1] = Packet;
#endif
    }
#if DBG
        NbfSendPacketTable[0] = (PVOID)NbfConfig->InitPackets;
        NbfSendPacketTable[NbfConfig->InitPackets+1] = (PVOID)
                    ((NBF_PACKET_SIGNATURE << 16) | sizeof (TP_PACKET));
#endif

    DeviceContext->PacketInitAllocated = NbfConfig->InitPackets;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d I-frame send packets, %ld\n", NbfConfig->InitPackets, DeviceContext->MemoryUsage);
    }


    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating RR frames.\n");
        NbfPrint1 ("NBFDRVR: Packet pool header: %lx\n",&DeviceContext->RrPacketPool);
    }

    for (i=0; i<10; i++) {

        NbfAllocateSendPacket (DeviceContext, &Packet);
        if (Packet == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate packets.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        Packet->Action = PACKET_ACTION_RR;
        PushEntryList (&DeviceContext->RrPacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
    }

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d RR-frame send packets, %ld\n", 10, DeviceContext->MemoryUsage);
    }


    // Allocate receive Ndis packets

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating Ndis Receive packets.\n");
    }
    if (DeviceContext->MacInfo.SingleReceive) {
        InitReceivePackets = 2;
    } else {
        InitReceivePackets = NbfConfig->InitReceivePackets;
    }
    for (i=0; i<InitReceivePackets; i++) {

        NbfAllocateReceivePacket (DeviceContext, &NdisPacket);

        if (NdisPacket == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate packet MDLs.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        ReceiveTag = (PRECEIVE_PACKET_TAG)NdisPacket->ProtocolReserved;
        PushEntryList (&DeviceContext->ReceivePacketPool, &ReceiveTag->Linkage);

        IF_NBFDBG (NBF_DEBUG_RESOURCE) {
            PNDIS_BUFFER NdisBuffer;
            NdisQueryPacket(NdisPacket, NULL, NULL, &NdisBuffer, NULL);
            NbfPrint2 ("NbfInitialize: Created NDIS Pkt: %x Buffer: %x\n",
                NdisPacket, NdisBuffer);
        }
    }

    DeviceContext->ReceivePacketInitAllocated = InitReceivePackets;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d receive packets, %ld\n", InitReceivePackets, DeviceContext->MemoryUsage);
    }

    IF_NBFDBG (NBF_DEBUG_RESOURCE) {
        NbfPrint0 ("NBFDRVR: allocating Ndis Receive buffers.\n");
    }

    for (i=0; i<NbfConfig->InitReceiveBuffers; i++) {

        NbfAllocateReceiveBuffer (DeviceContext, &BufferTag);

        if (BufferTag == NULL) {
            PANIC ("NbfInitialize: Unable to allocate receive packet.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        PushEntryList (&DeviceContext->ReceiveBufferPool, (PSINGLE_LIST_ENTRY)&BufferTag->Linkage);

    }

    DeviceContext->ReceiveBufferInitAllocated = NbfConfig->InitReceiveBuffers;

    IF_NBFDBG (NBF_DEBUG_DYNAMIC) {
        NbfPrint2 ("%d receive buffers, %ld\n", NbfConfig->InitReceiveBuffers, DeviceContext->MemoryUsage);
    }

#endif

    // Store away the PDO for the underlying object
    DeviceContext->PnPContext = SystemSpecific2;

    DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

    //
    // Start the link-level timers running.
    //

    NbfInitializeTimerSystem (DeviceContext);

    //
    // Now link the device into the global list.
    //

 //   ACQUIRE_DEVICES_LIST_LOCK();
 //   InsertTailList (&NbfDeviceList, &DeviceContext->Linkage);
 //   RELEASE_DEVICES_LIST_LOCK();

    DeviceObject = (PDEVICE_OBJECT) DeviceContext;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

    IF_NBFDBG (NBF_DEBUG_PNP) {
        NbfPrint1 ("TdiRegisterDeviceObject for %S\n", DeviceString.Buffer);
    }

    status = TdiRegisterDeviceObject(&DeviceString,
                                     &DeviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
       // RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }

    RtlCopyMemory(NetBIOSAddress->NetbiosName,
                  DeviceContext->ReservedNetBIOSAddress, 16);

    tdiPnPContext1.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext1.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_IF_NAME;
    *(PVOID UNALIGNED *) &tdiPnPContext1.tdiPnPContextHeader.ContextData = &DeviceString;

    tdiPnPContext2.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext2.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_PDO;
    *(PVOID UNALIGNED *) &tdiPnPContext2.tdiPnPContextHeader.ContextData = SystemSpecific2;

    IF_NBFDBG (NBF_DEBUG_PNP) {
        NbfPrint1 ("TdiRegisterNetAddress on %S ", DeviceString.Buffer);
        NbfPrint6 ("for %02x%02x%02x%02x%02x%02x\n",
                            NetBIOSAddress->NetbiosName[10],
                            NetBIOSAddress->NetbiosName[11],
                            NetBIOSAddress->NetbiosName[12],
                            NetBIOSAddress->NetbiosName[13],
                            NetBIOSAddress->NetbiosName[14],
                            NetBIOSAddress->NetbiosName[15]);
    }

{
	PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;
    RtlCopyMemory (
		&LpxAddress->Node,
		DeviceContext->LocalAddress.Address,
		HARDWARE_ADDRESS_LENGTH
		);
}

#if 0
    status = TdiRegisterNetAddress(pAddress,
                                   &DeviceString,
                                   (TDI_PNP_CONTEXT *) &tdiPnPContext2,
                                   &DeviceContext->ReservedAddressHandle);

    if (!NT_SUCCESS (status)) {
        //RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }
#endif

    NbfReferenceDeviceContext ("Load Succeeded", DeviceContext, DCREF_CREATION);

    LEAVE_NBF;
	
	*SocketLpxDeviceContext = DeviceContext;

	ACQUIRE_DEVICES_LIST_LOCK();
	InsertTailList (&NbfDeviceList, &DeviceContext->Linkage);
    RELEASE_DEVICES_LIST_LOCK();
    
	*NdisStatus = NDIS_STATUS_SUCCESS;

    return(1);

cleanup:

    NbfWriteResourceErrorLog(
        DeviceContext,
        EVENT_TRANSPORT_RESOURCE_POOL,
        501,
        DeviceContext->MemoryUsage,
        0);

    //
    // Cleanup whatever device context we were initializing
    // when we failed.
    //
    *NdisStatus = status;
    ASSERT(status != STATUS_SUCCESS);
    
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

        // Stop all internal timers
        NbfStopTimerSystem(DeviceContext);

        // Remove creation reference
        NbfDereferenceDeviceContext ("Load failed", DeviceContext, DCREF_CREATION);
    }

    LEAVE_NBF;

    return (0);
}


VOID
SocketNbfProtocolBindAdapter(
                OUT PNDIS_STATUS    NdisStatus,
                IN NDIS_HANDLE      BindContext,
                IN PNDIS_STRING     DeviceName,
                IN PVOID            SystemSpecific1,
                IN PVOID            SystemSpecific2
                ) 
{
	NDIS_STATUS		ndisStatus;
    UNICODE_STRING  socketNbfDeviceName;

	DebugPrint(2, ("SocketNbfProtocolBindAdapter %p\n", SocketLpxDeviceContext));

	UNREFERENCED_PARAMETER(NdisStatus) ;
	UNREFERENCED_PARAMETER(BindContext) ;
	UNREFERENCED_PARAMETER(DeviceName) ;
	UNREFERENCED_PARAMETER(SystemSpecific1) ;
	UNREFERENCED_PARAMETER(SystemSpecific2) ;

	//
	//	we create only one socket device context.
	//
	//
	//	comment by hootch 08262003
	if(SocketLpxDeviceContext != NULL)
		return;

#ifdef __LPX__
    RtlInitUnicodeString(&socketNbfDeviceName, SOCKETLPX_DEVICE_NAME);
#else
    RtlInitUnicodeString(&socketNbfDeviceName, SOCKETNBF_DEVICE_NAME);
#endif

    SocketNbfInitializeOneDeviceContext(
		&ndisStatus, 
		NbfDriverObject,
        NbfConfig,
        NULL,
        &socketNbfDeviceName,
        NULL,
        NULL,
		&SocketLpxDeviceContext
        );

    if(!NT_SUCCESS(ndisStatus)) {
		SocketLpxDeviceContext = NULL;

        goto ErrorOut;
    }



ErrorOut:
	
	// ILGU 2003_1103	Support packet drop flags
	SocketLpxDeviceContext->bDeviceInit = TRUE;
	return;
}


VOID
SocketNbfProtocolUnbindAdapter(
	OUT PNDIS_STATUS	NdisStatus,
    IN	NDIS_HANDLE		ProtocolBindContext,
    IN	PNDIS_HANDLE	UnbindContext
    )
/*++

Routine Description:

    This routine deactivates a transport binding. Before it does this, it
    indicates to all clients above, that the device is going away. Clients
    are expected to close all open handles to the device.

    Then the device is pulled out of the list of NBF devices, and all
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
//    PTP_ADDRESS Address;
//    NTSTATUS status;
//    KIRQL oldirql;
//    PLIST_ENTRY p;

	DebugPrint(2, ("SocketNbfProtocolUnBindAdapter %p\n", SocketLpxDeviceContext));

	UNREFERENCED_PARAMETER(UnbindContext) ;

//#if DBG	
//	NbfDebug = 0xFFFFFFFF;
//	DebugLevel = 10;
//#endif

#if DBG

    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }

#endif

    // Get the device context for the adapter being unbound
    DeviceContext = (PDEVICE_CONTEXT) ProtocolBindContext;
	DeviceContext->bDeviceInit = FALSE;
    IF_NBFDBG (NBF_DEBUG_PNP) {
        NbfPrint1 ("ENTER SocketNbfProtocolUnbindAdapter for %S\n", DeviceContext->DeviceName);
    }

    // Remove creation ref if it has not already been removed,
    // after telling TDI and its clients that we'r going away.
    // This flag also helps prevent any more TDI indications
    // of deregister addr/devobj - after the 1st one succeeds.
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

{
//	#define DOS_DEVICE_NAME L"\\DosDevices\\SocketLpx"
	
	UNICODE_STRING  unicodeDeviceName;

    RtlInitUnicodeString(&unicodeDeviceName, SOCKETLPX_DOSDEVICE_NAME);
    IoDeleteSymbolicLink( &unicodeDeviceName);
}

        // Assume upper layers clean up by closing connections
        // when we deregister all addresses and device object,
        // but this can happen asynchronously, after we return
        // from the (asynchronous) TdiDeregister.. calls below 

        // Inform TDI by deregistering the reserved netbios address
#if 0
        *NdisStatus = TdiDeregisterNetAddress(DeviceContext->ReservedAddressHandle);

        if (!NT_SUCCESS (*NdisStatus)) {
        
            IF_NBFDBG (NBF_DEBUG_PNP) {
                NbfPrint1("No success deregistering this address,STATUS = %08X\n",*NdisStatus);
            }

            // this can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            DeviceContext->CreateRefRemoved = FALSE;
            
            return;
        }
#endif
        
        // Inform TDI (and its clients) that device is going away
        *NdisStatus = TdiDeregisterDeviceObject(DeviceContext->TdiDeviceHandle);

        if (!NT_SUCCESS (*NdisStatus)) {
        
            IF_NBFDBG (NBF_DEBUG_PNP) {
                NbfPrint1("No success deregistering device object,STATUS = %08X\n",*NdisStatus);
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
        NbfStopTimerSystem(DeviceContext);

#if 0
        // Cleanup the Ndis Binding as it is not useful on return
        // from this function - do not try to use it after this
        NbfCloseNdis(DeviceContext);
#endif

        // BUG BUG -- probable race condition with timer callbacks
        // Do we wait for some time in case a timer func gets in ?

        // Removing creation reference means that once all handles
        // r closed,device will automatically be garbage-collected
        NbfDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);

#if 0
        if (InterlockedDecrement(&NumberOfBinds) == 0) {

#ifdef RASAUTODIAL

            // 
            // This is a successful close of last adapter
            //
#if DBG
            DbgPrint("Calling NbfAcdUnbind()\n");
#endif

            //
            // Unbind from the automatic connection driver.
            //  

            NbfAcdUnbind();

#endif // RASAUTODIAL

        }
#endif
    }
    else {
    
        // Ignore any duplicate Unbind Indications from NDIS layer
        *NdisStatus = NDIS_STATUS_SUCCESS;
    }

    IF_NBFDBG (NBF_DEBUG_PNP) {
        NbfPrint2 ("LEAVE SocketNbfProtocolUnbindAdapter for %S with Status %08x\n",
                        DeviceContext->DeviceName, *NdisStatus);
    }

    return;
}

PDEVICE_CONTEXT
SocketLpxFindDeviceContext(
    PNBF_NETBIOS_ADDRESS networkName
	)
{
#ifdef __LPX__
    PDEVICE_CONTEXT deviceContext;
	PLIST_ENTRY		listHead;
	PLIST_ENTRY		thisEntry;
	CHAR			notAssigned[HARDWARE_ADDRESS_LENGTH] = {0, 0, 0, 0, 0, 0};

	DebugPrint(2, ("SocketLpxFindDeviceContext\n"));

	DebugPrint(2,("%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X:%04X\n",
					networkName->NetbiosName[0],
					networkName->NetbiosName[1],
					networkName->NetbiosName[2],
					networkName->NetbiosName[3],
					networkName->NetbiosName[4],
					networkName->NetbiosName[5],
					networkName->NetbiosName[6],
					networkName->NetbiosName[7],
					networkName->NetbiosName[8],
					networkName->NetbiosName[9],
					networkName->NetbiosName[10],
					networkName->NetbiosName[11],
					networkName->NetbiosName[12],
					networkName->NetbiosName[13],
					networkName->NetbiosName[14],
					networkName->NetbiosName[15],
					networkName->NetbiosNameType));

	DebugPrint(2,("networkName = %p, networkName->LpxAddress = %p %02X%02X%02X%02X%02X%02X:%04X\n",
					networkName, &networkName->LpxAddress,
					networkName->LpxAddress.Node[0],
					networkName->LpxAddress.Node[1],
					networkName->LpxAddress.Node[2],
					networkName->LpxAddress.Node[3],
					networkName->LpxAddress.Node[4],
					networkName->LpxAddress.Node[5],
					networkName->LpxAddress.Port));

	ACQUIRE_DEVICES_LIST_LOCK();

	if (IsListEmpty (&NbfDeviceList)) {
		RELEASE_DEVICES_LIST_LOCK();
		return NULL;
	}

	listHead = &NbfDeviceList;
	for(deviceContext = NULL, thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = thisEntry->Flink)
	{

        deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
		if((deviceContext != SocketLpxDeviceContext)
			&&(deviceContext->CreateRefRemoved == FALSE)){
            if (RtlEqualMemory (
                    deviceContext->LocalAddress.Address,
					&networkName->LpxAddress.Node,
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
				&networkName->LpxAddress.Node,
				HARDWARE_ADDRESS_LENGTH
				)
		&& SocketLpxPrimaryDeviceContext) 
	{
		deviceContext = SocketLpxPrimaryDeviceContext;
		ASSERT(deviceContext);
	    RtlCopyMemory (
			&networkName->LpxAddress.Node,
			deviceContext->LocalAddress.Address,
			HARDWARE_ADDRESS_LENGTH
			);
	}

	RELEASE_DEVICES_LIST_LOCK();

	return deviceContext;
#else
	return SocketLpxPrimaryDeviceContext;
#endif
}

