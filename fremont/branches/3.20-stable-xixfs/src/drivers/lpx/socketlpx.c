#include "precomp.h"
#pragma hdrstop


LONG	DebugLevel = 1;

PDEVICE_CONTEXT  SocketLpxDeviceContext;
PDEVICE_CONTEXT  SocketLpxPrimaryDeviceContext;


ULONG
SocketLpxInitializeOneDeviceContext(
	OUT PNDIS_STATUS NdisStatus,
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA LpxConfig,
    IN PUNICODE_STRING BindName,
    IN PUNICODE_STRING ExportName,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2,
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

    LpxConfig    - the transport configuration information from the registry.

    SystemSpecific1 - SystemSpecific1 argument to ProtocolBindAdapter

    SystemSpecific2 - SystemSpecific2 argument to ProtocolBindAdapter

Return Value:

    The number of successful binds.

--*/

{
    ULONG i;
    PDEVICE_CONTEXT DeviceContext;
#if 0
	PTP_REQUEST Request;
    PTP_LINK Link;
#endif
	PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
#if 0
    PTP_UI_FRAME UIFrame;
    PTP_PACKET Packet;
    PNDIS_PACKET NdisPacket;
    PRECEIVE_PACKET_TAG ReceiveTag;
    PBUFFER_TAG BufferTag;
    KIRQL oldIrql;
#endif
	NTSTATUS status;
    UINT MaxUserData;
#if 0
    ULONG InitReceivePackets;
#endif
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

#if __LPX__
        UniProcessor = FALSE;
#else
    if (KeNumberProcessors == 1) {
        UniProcessor = TRUE;
    } else {
        UniProcessor = FALSE;
    }
#endif

    //
    // Loop through all the adapters that are in the configuration
    // information structure. Allocate a device object for each
    // one that we find.
    //

    status = LpxCreateDeviceContext(
                                    DriverObject,
                                    ExportName,
                                    &DeviceContext
                                   );

    if (!NT_SUCCESS (status)) {

        IF_LPXDBG (LPX_DEBUG_PNP) {
            LpxPrint2 ("LpxCreateDeviceContext for %S returned error %08x\n",
                            ExportName->Buffer, status);
        }

		//
		// First check if we already have an object with this name
		// This is because a previous unbind was not done properly.
		//

    	if (status == STATUS_OBJECT_NAME_COLLISION) {

			// See if we can reuse the binding and device name
			
			LpxReInitializeDeviceContext(
                                         &status,
                                         DriverObject,
                                         LpxConfig,
                                         BindName,
                                         ExportName,
                                         SystemSpecific1,
                                         SystemSpecific2
                                        );

			if (status == STATUS_NOT_FOUND)
			{
				// Must have got deleted in the meantime
			
				return LpxInitializeOneDeviceContext(
                                                     NdisStatus,
                                                     DriverObject,
                                                     LpxConfig,
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
	        LpxWriteGeneralErrorLog(
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

    DeviceContext->UniProcessor = UniProcessor;

    //
    // Initialize our counter that records memory usage.
    //

    DeviceContext->MemoryUsage = 0;
    DeviceContext->MemoryLimit = LpxConfig->MaxMemoryUsage;

    DeviceContext->MaxConnections = LpxConfig->MaxConnections;
    DeviceContext->MaxAddressFiles = LpxConfig->MaxAddressFiles;
    DeviceContext->MaxAddresses = LpxConfig->MaxAddresses;

#if 0
    //
    // Now fire up NDIS so this adapter talks
    //

    status = LpxInitializeNdis (DeviceContext,
                                LpxConfig,
                                BindName);

    if (!NT_SUCCESS (status)) {

        //
        // Log an error if we were failed to
        // open this adapter.
        //

        LpxWriteGeneralErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_BINDING_FAILED,
            601,
            status,
            BindName->Buffer,
            0,
            NULL);

        if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
            LpxDereferenceDeviceContext ("Initialize NDIS failed", DeviceContext, DCREF_CREATION);
        }
        
        *NdisStatus = status;
        return(0);

    }

#if 0
    DbgPrint("Opened %S as %S\n", &LpxConfig->Names[j], &nameString);
#endif

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint6 ("LpxInitialize: NDIS returned: %x %x %x %x %x %x as local address.\n",
            DeviceContext->LocalAddress.Address[0],
            DeviceContext->LocalAddress.Address[1],
            DeviceContext->LocalAddress.Address[2],
            DeviceContext->LocalAddress.Address[3],
            DeviceContext->LocalAddress.Address[4],
            DeviceContext->LocalAddress.Address[5]);
    }

#endif

    //
    // Initialize our provider information structure; since it
    // doesn't change, we just keep it around and copy it to
    // whoever requests it.
    //

#if 0

    MacReturnMaxDataSize(
        &DeviceContext->MacInfo,
        NULL,
        0,
        DeviceContext->MaxSendPacketSize,
        TRUE,
        &MaxUserData);
#else
	MaxUserData = 1500;
#endif

#if 0
    DeviceContext->Information.Version = 0x0100;
    DeviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    DeviceContext->Information.MaxConnectionUserData = 0;
    DeviceContext->Information.MaxDatagramSize =
        MaxUserData - (sizeof(DLC_FRAME) + sizeof(LPX_HDR_CONNECTIONLESS));
    DeviceContext->Information.ServiceFlags = LPX_SERVICE_FLAGS;
    if (DeviceContext->MacInfo.MediumAsync) {
        DeviceContext->Information.ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
    }
    DeviceContext->Information.MinimumLookaheadData =
        240 - (sizeof(DLC_FRAME) + sizeof(LPX_HDR_CONNECTIONLESS));
    DeviceContext->Information.MaximumLookaheadData =
        DeviceContext->MaxReceivePacketSize - (sizeof(DLC_I_FRAME) + sizeof(LPX_HDR_CONNECTION));
    DeviceContext->Information.NumberOfResources = LPX_TDI_RESOURCES;
    KeQuerySystemTime (&DeviceContext->Information.StartTime);
#else
    DeviceContext->Information.Version = 0x0100;
    DeviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    DeviceContext->Information.MaxConnectionUserData = 0;
    DeviceContext->Information.MaxDatagramSize =  LPX_MAX_DATAGRAM_SIZE;
    DeviceContext->Information.ServiceFlags = LPX_SERVICE_FLAGS;
    if (DeviceContext->MacInfo.MediumAsync) {
        DeviceContext->Information.ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
    }
    DeviceContext->Information.MinimumLookaheadData =  240 - sizeof(LPX_HEADER);
    DeviceContext->Information.MaximumLookaheadData =  1500 - sizeof(LPX_HEADER);
    DeviceContext->Information.NumberOfResources = LPX_TDI_RESOURCES;
    KeQuerySystemTime (&DeviceContext->Information.StartTime);
#endif

    //
    // Allocate various structures we will need.
    //

    ENTER_LPX;

    //
    // The TP_PACKET structure has a CHAR[1] field at the end
    // which we expand upon to include all the headers needed;
    // the size of the MAC header depends on what the adapter
    // told us about its max header size. TP_PACKETs are used
    // for connection-oriented frame as well as for
    // control frames, but since DLC_I_FRAME and DLC_S_FRAME
    // are the same size, the header is the same size.
    //

#if 0

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: pre-allocating requests.\n");
    }
    for (i=0; i<LpxConfig->InitRequests; i++) {

        LpxAllocateRequest (DeviceContext, &Request);

        if (Request == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate requests.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->RequestPool, &Request->Linkage);
#if DBG
        LpxRequestTable[i+1] = (PVOID)Request;
#endif
    }
#if DBG
    LpxRequestTable[0] = UlongToPtr(LpxConfig->InitRequests);
    LpxRequestTable[LpxConfig->InitRequests + 1] = (PVOID)
                        ((LPX_REQUEST_SIGNATURE << 16) | sizeof (TP_REQUEST));
    InitializeListHead (&LpxGlobalRequestList);
#endif

    DeviceContext->RequestInitAllocated = LpxConfig->InitRequests;
    DeviceContext->RequestMaxAllocated = LpxConfig->MaxRequests;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d requests, %ld\n", LpxConfig->InitRequests, DeviceContext->MemoryUsage);
    }

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating links.\n");
    }
    for (i=0; i<LpxConfig->InitLinks; i++) {

        LpxAllocateLink (DeviceContext, &Link);

        if (Link == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate links.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->LinkPool, &Link->Linkage);
#if DBG
        LpxLinkTable[i+1] = (PVOID)Link;
#endif
    }
#if DBG
    LpxLinkTable[0] = UlongToPtr(LpxConfig->InitLinks);
    LpxLinkTable[LpxConfig->InitLinks+1] = (PVOID)
                ((LPX_LINK_SIGNATURE << 16) | sizeof (TP_LINK));
#endif

    DeviceContext->LinkInitAllocated = LpxConfig->InitLinks;
    DeviceContext->LinkMaxAllocated = LpxConfig->MaxLinks;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d links, %ld\n", LpxConfig->InitLinks, DeviceContext->MemoryUsage);
    }

#endif

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating connections.\n");
    }
    for (i=0; i<LpxConfig->InitConnections; i++) {

        LpxAllocateConnection (DeviceContext, &Connection);

        if (Connection == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate connections.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->ConnectionPool, &Connection->LinkList);
#if DBG
        LpxConnectionTable[i+1] = (PVOID)Connection;
#endif
    }
#if DBG
    LpxConnectionTable[0] = UlongToPtr(LpxConfig->InitConnections);
    LpxConnectionTable[LpxConfig->InitConnections+1] = (PVOID)
                ((LPX_CONNECTION_SIGNATURE << 16) | sizeof (TP_CONNECTION));
#endif

    DeviceContext->ConnectionInitAllocated = LpxConfig->InitConnections;
    DeviceContext->ConnectionMaxAllocated = LpxConfig->MaxConnections;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d connections, %ld\n", LpxConfig->InitConnections, DeviceContext->MemoryUsage);
    }


    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating AddressFiles.\n");
    }
    for (i=0; i<LpxConfig->InitAddressFiles; i++) {

        LpxAllocateAddressFile (DeviceContext, &AddressFile);

        if (AddressFile == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate Address Files.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
#if DBG
        LpxAddressFileTable[i+1] = (PVOID)AddressFile;
#endif
    }
#if DBG
    LpxAddressFileTable[0] = UlongToPtr(LpxConfig->InitAddressFiles);
    LpxAddressFileTable[LpxConfig->InitAddressFiles + 1] = (PVOID)
                            ((LPX_ADDRESSFILE_SIGNATURE << 16) |
                                 sizeof (TP_ADDRESS_FILE));
#endif

    DeviceContext->AddressFileInitAllocated = LpxConfig->InitAddressFiles;
    DeviceContext->AddressFileMaxAllocated = LpxConfig->MaxAddressFiles;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d address files, %ld\n", LpxConfig->InitAddressFiles, DeviceContext->MemoryUsage);
    }


    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating addresses.\n");
    }
    for (i=0; i<LpxConfig->InitAddresses; i++) {

        LpxAllocateAddress (DeviceContext, &Address);
        if (Address == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate addresses.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
#if DBG
        LpxAddressTable[i+1] = (PVOID)Address;
#endif
    }
#if DBG
    LpxAddressTable[0] = UlongToPtr(LpxConfig->InitAddresses);
    LpxAddressTable[LpxConfig->InitAddresses + 1] = (PVOID)
                        ((LPX_ADDRESS_SIGNATURE << 16) | sizeof (TP_ADDRESS));
#endif

    DeviceContext->AddressInitAllocated = LpxConfig->InitAddresses;
    DeviceContext->AddressMaxAllocated = LpxConfig->MaxAddresses;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d addresses, %ld\n", LpxConfig->InitAddresses, DeviceContext->MemoryUsage);
    }

#if 0

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating UI frames.\n");
    }

    for (i=0; i<LpxConfig->InitUIFrames; i++) {

        LpxAllocateUIFrame (DeviceContext, &UIFrame);

        if (UIFrame == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate UI frames.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&(DeviceContext->UIFramePool), &UIFrame->Linkage);
#if DBG
        LpxUiFrameTable[i+1] = UIFrame;
#endif
    }
#if DBG
        LpxUiFrameTable[0] = UlongToPtr(LpxConfig->InitUIFrames);
#endif

    DeviceContext->UIFrameInitAllocated = LpxConfig->InitUIFrames;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d UI frames, %ld\n", LpxConfig->InitUIFrames, DeviceContext->MemoryUsage);
    }


    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating I frames.\n");
        LpxPrint1 ("LPXDRVR: Packet pool header: %lx\n",&DeviceContext->PacketPool);
    }

    for (i=0; i<LpxConfig->InitPackets; i++) {

        LpxAllocateSendPacket (DeviceContext, &Packet);
        if (Packet == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate packets.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        PushEntryList (&DeviceContext->PacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
#if DBG
        LpxSendPacketTable[i+1] = Packet;
#endif
    }
#if DBG
        LpxSendPacketTable[0] = UlongToPtr(LpxConfig->InitPackets);
        LpxSendPacketTable[LpxConfig->InitPackets+1] = (PVOID)
                    ((LPX_PACKET_SIGNATURE << 16) | sizeof (TP_PACKET));
#endif

    DeviceContext->PacketInitAllocated = LpxConfig->InitPackets;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d I-frame send packets, %ld\n", LpxConfig->InitPackets, DeviceContext->MemoryUsage);
    }


    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating RR frames.\n");
        LpxPrint1 ("LPXDRVR: Packet pool header: %lx\n",&DeviceContext->RrPacketPool);
    }

    for (i=0; i<10; i++) {

        LpxAllocateSendPacket (DeviceContext, &Packet);
        if (Packet == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate packets.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        Packet->Action = PACKET_ACTION_RR;
        PushEntryList (&DeviceContext->RrPacketPool, (PSINGLE_LIST_ENTRY)&Packet->Linkage);
    }

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d RR-frame send packets, %ld\n", 10, DeviceContext->MemoryUsage);
    }


    // Allocate receive Ndis packets

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating Ndis Receive packets.\n");
    }
    if (DeviceContext->MacInfo.SingleReceive) {
        InitReceivePackets = 2;
    } else {
        InitReceivePackets = LpxConfig->InitReceivePackets;
    }
    for (i=0; i<InitReceivePackets; i++) {

        LpxAllocateReceivePacket (DeviceContext, &NdisPacket);

        if (NdisPacket == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate packet MDLs.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        ReceiveTag = (PRECEIVE_PACKET_TAG)NdisPacket->ProtocolReserved;
        PushEntryList (&DeviceContext->ReceivePacketPool, &ReceiveTag->Linkage);

        IF_LPXDBG (LPX_DEBUG_RESOURCE) {
            PNDIS_BUFFER NdisBuffer;
            NdisQueryPacket(NdisPacket, NULL, NULL, &NdisBuffer, NULL);
            LpxPrint2 ("LpxInitialize: Created NDIS Pkt: %x Buffer: %x\n",
                NdisPacket, NdisBuffer);
        }
    }

    DeviceContext->ReceivePacketInitAllocated = InitReceivePackets;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d receive packets, %ld\n", InitReceivePackets, DeviceContext->MemoryUsage);
    }

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating Ndis Receive buffers.\n");
    }

    for (i=0; i<LpxConfig->InitReceiveBuffers; i++) {

        LpxAllocateReceiveBuffer (DeviceContext, &BufferTag);

        if (BufferTag == NULL) {
            PANIC ("LpxInitialize: Unable to allocate receive packet.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        PushEntryList (&DeviceContext->ReceiveBufferPool, (PSINGLE_LIST_ENTRY)&BufferTag->Linkage);

    }

    DeviceContext->ReceiveBufferInitAllocated = LpxConfig->InitReceiveBuffers;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d receive buffers, %ld\n", LpxConfig->InitReceiveBuffers, DeviceContext->MemoryUsage);
    }

#endif

    // Store away the PDO for the underlying object
    DeviceContext->PnPContext = SystemSpecific2;

    DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

    //
    // Start the link-level timers running.
    //

    LpxInitializeTimerSystem (DeviceContext);

#if 0
    //
    // Now link the device into the global list.
    //

    ACQUIRE_DEVICES_LIST_LOCK();
    InsertTailList (&LpxDeviceList, &DeviceContext->Linkage);
    RELEASE_DEVICES_LIST_LOCK();
#endif

    DeviceObject = (PDEVICE_OBJECT) DeviceContext;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("TdiRegisterDeviceObject for %S\n", DeviceString.Buffer);
    }

    status = TdiRegisterDeviceObject(&DeviceString,
                                     &DeviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
#if 0
        RemoveEntryList(&DeviceContext->Linkage);
#endif
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

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("TdiRegisterNetAddress on %S ", DeviceString.Buffer);
        LpxPrint6 ("for %02x%02x%02x%02x%02x%02x\n",
                            NetBIOSAddress->NetbiosName[10],
                            NetBIOSAddress->NetbiosName[11],
                            NetBIOSAddress->NetbiosName[12],
                            NetBIOSAddress->NetbiosName[13],
                            NetBIOSAddress->NetbiosName[14],
                            NetBIOSAddress->NetbiosName[15]);
    }

{
	PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;
    
	RtlCopyMemory( &LpxAddress->Node,
				   DeviceContext->LocalAddress.Address,
				   HARDWARE_ADDRESS_LENGTH );
}

#if 0
    status = TdiRegisterNetAddress(pAddress,
                                   &DeviceString,
                                   (TDI_PNP_CONTEXT *) &tdiPnPContext2,
                                   &DeviceContext->ReservedAddressHandle);

    if (!NT_SUCCESS (status)) {
        RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }
#endif

    LpxReferenceDeviceContext ("Load Succeeded", DeviceContext, DCREF_CREATION);

    LEAVE_LPX;

	*SocketLpxDeviceContext = DeviceContext;

	ACQUIRE_DEVICES_LIST_LOCK();
	InsertTailList (&LpxDeviceList, &DeviceContext->Linkage);
    RELEASE_DEVICES_LIST_LOCK();

    *NdisStatus = NDIS_STATUS_SUCCESS;

    return(1);

cleanup:

    LpxWriteResourceErrorLog(
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
        LpxStopTimerSystem(DeviceContext);

        // Remove creation reference
        LpxDereferenceDeviceContext ("Load failed", DeviceContext, DCREF_CREATION);
    }

    LEAVE_LPX;

    return (0);
}


VOID
SocketLpxProtocolBindAdapter(
                OUT PNDIS_STATUS    NdisStatus,
                IN NDIS_HANDLE      BindContext,
                IN PNDIS_STRING     DeviceName,
                IN PVOID            SystemSpecific1,
                IN PVOID            SystemSpecific2
                ) 
{
	NDIS_STATUS		ndisStatus;
    UNICODE_STRING  socketLpxDeviceName;

	DebugPrint(2, ("SocketLpxProtocolBindAdapter %p\n", SocketLpxDeviceContext));

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

#if __LPX__
    RtlInitUnicodeString(&socketLpxDeviceName, SOCKETLPX_DEVICE_NAME);
#else
    RtlInitUnicodeString(&socketLpxDeviceName, SOCKETLPX_DEVICE_NAME);
#endif

    SocketLpxInitializeOneDeviceContext( &ndisStatus, 
										 LpxDriverObject,
										 LpxConfig,
										 NULL,
										 &socketLpxDeviceName,
										 NULL,
										 NULL,
										 &SocketLpxDeviceContext );

    if (!NT_SUCCESS(ndisStatus)) {

		SocketLpxDeviceContext = NULL;
        return;
    }

	SetFlag( SocketLpxDeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START );
	DebugPrint( 1, ("SocketLpxDeviceContext is Created\n") );

	return;
}


VOID
SocketLpxProtocolUnbindAdapter(
	OUT PNDIS_STATUS	NdisStatus,
    IN	NDIS_HANDLE		ProtocolBindContext,
    IN	PNDIS_HANDLE	UnbindContext
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
//    PTP_ADDRESS Address;
//    NTSTATUS status;
//    KIRQL oldirql;
//    PLIST_ENTRY p;

	DebugPrint(2, ("SocketLpxProtocolUnBindAdapter %p\n", SocketLpxDeviceContext));

	UNREFERENCED_PARAMETER(UnbindContext) ;

//#if DBG	
//	LpxDebug = 0xFFFFFFFF;
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

	SetFlag( DeviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP );

	IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("ENTER SocketLpxProtocolUnbindAdapter for %S\n", DeviceContext->DeviceName);
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
        
            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1("No success deregistering this address,STATUS = %08X\n",*NdisStatus);
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

#if 0
        // Cleanup the Ndis Binding as it is not useful on return
        // from this function - do not try to use it after this
        LpxCloseNdis(DeviceContext);
#endif

        // BUG BUG -- probable race condition with timer callbacks
        // Do we wait for some time in case a timer func gets in ?

        // Removing creation reference means that once all handles
        // r closed,device will automatically be garbage-collected
        LpxDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);

#if 0
        if (InterlockedDecrement(&NumberOfBinds) == 0) {

#ifdef RASAUTODIAL

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

#endif // RASAUTODIAL

        }
#endif
    }
    else {
    
        // Ignore any duplicate Unbind Indications from NDIS layer
        *NdisStatus = NDIS_STATUS_SUCCESS;
    }

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("LEAVE SocketLpxProtocolUnbindAdapter for %S with Status %08x\n",
                        DeviceContext->DeviceName, *NdisStatus);
    }

    return;
}


NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
//#ifndef __XP__
//    IN PNDIS_REQUEST NdisRequest,
//#else
    IN PNDIS_REQUEST NdisRequest2,
//#endif
    IN PNDIS_STRING AdapterString
    );

NTSTATUS
SocketLpxFindDeviceContext(
    IN  PLPX_ADDRESS	NetworkName,
	OUT PDEVICE_CONTEXT *DeviceContext
	)
{
	NTSTATUS		status;
    PDEVICE_CONTEXT deviceContext;
	PLIST_ENTRY		listHead;
	PLIST_ENTRY		thisEntry;
	CHAR			notAssigned[HARDWARE_ADDRESS_LENGTH] = {0, 0, 0, 0, 0, 0};
	KIRQL			oldIrql;

	DebugPrint( 2, ("SocketLpxFindDeviceContext\n") );

	DebugPrint( 2,("NetworkName = %p, %02X%02X%02X%02X%02X%02X:%04X\n",
					NetworkName, 
					NetworkName->Node[0],
					NetworkName->Node[1],
					NetworkName->Node[2],
					NetworkName->Node[3],
					NetworkName->Node[4],
					NetworkName->Node[5],
					NetworkName->Port) );

	ACQUIRE_DEVICES_LIST_LOCK ();

	if (IsListEmpty(&LpxDeviceList)) {

		RELEASE_DEVICES_LIST_LOCK ();
		return STATUS_NO_MEDIA;
	}

	if (RtlCompareMemory(notAssigned, &NetworkName->Node, HARDWARE_ADDRESS_LENGTH) == HARDWARE_ADDRESS_LENGTH && 
		SocketLpxPrimaryDeviceContext) {

		deviceContext = SocketLpxPrimaryDeviceContext;
		ASSERT( deviceContext );

	    RtlCopyMemory( &NetworkName->Node,
					   deviceContext->LocalAddress.Address,
					   HARDWARE_ADDRESS_LENGTH );

		*DeviceContext = deviceContext;
		
		RELEASE_DEVICES_LIST_LOCK ();

		return NDIS_STATUS_SUCCESS;
	}

	status = NDIS_STATUS_INVALID_ADDRESS;
	deviceContext = NULL;
	listHead = &LpxDeviceList;

	for (thisEntry = listHead->Flink;
		 thisEntry != listHead;
		 thisEntry = thisEntry->Flink) {

		deviceContext = CONTAINING_RECORD( thisEntry, DEVICE_CONTEXT, Linkage );
		
		if (deviceContext != SocketLpxDeviceContext && 
			deviceContext->CreateRefRemoved == FALSE) {

			ASSERT( deviceContext->NdisBindingHandle );

			if (RtlCompareMemory(deviceContext->LocalAddress.Address,
							     &NetworkName->Node,
							     HARDWARE_ADDRESS_LENGTH) == HARDWARE_ADDRESS_LENGTH) {

			   ACQUIRE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, &oldIrql);
			   if (!(FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) && 
				     !FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP))) {

					status = NDIS_STATUS_ADAPTER_NOT_OPEN;
					deviceContext = NULL;
#if 0			   
			   } else if (!(FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED) && 
						    !FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED))) {

				   status = NDIS_STATUS_NO_CABLE;
				   deviceContext = NULL;
#endif			   
			   } else {
					status = NDIS_STATUS_SUCCESS;
					*DeviceContext = deviceContext;
			   }
			   RELEASE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, oldIrql);

			   break;
			}
		}

		deviceContext = NULL;
	}

	RELEASE_DEVICES_LIST_LOCK ();

	return status;
}
