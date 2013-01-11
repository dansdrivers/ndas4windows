#include "precomp.h"
#pragma hdrstop

VOID
lpx_Control_RefDeviceContext(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    )
{
    DebugPrint(0, ("LpxControlRefDeviceContext:  Entered.\n"));

    ASSERT (DeviceContext->ReferenceCount >= 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&DeviceContext->ReferenceCount);

} /* LpxControlRefDeviceContext */


VOID
lpx_Control_DerefDeviceContext(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    )
{
    LONG result;

   DebugPrint(0, ("LpxControlDerefDeviceContext:  Entered.\n"));


    result = InterlockedDecrement (&DeviceContext->ReferenceCount);

    ASSERT (result >= 0);

    if (result == 0) {
        lpx_DestroyControlDeviceContext (DeviceContext);
    }

} /* lxpControlDerefDeviceContext */

VOID
lpx_Ndis_RefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    DebugPrint(0, ("LpxControlRefDeviceContext:  Entered.\n"));

    ASSERT (DeviceContext->ReferenceCount >= 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&DeviceContext->ReferenceCount);

} /* LpxControlRefDeviceContext */


VOID
lpx_Ndis_DerefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    LONG result;

   DebugPrint(0, ("LpxNdislDerefDeviceContext:  Entered.\n"));


    result = InterlockedDecrement (&DeviceContext->ReferenceCount);

    ASSERT (result >= 0);

    if (result == 0) {
        lpx_DestroyNdisDeviceContext (DeviceContext);
    }

} /* lxpControlDerefDeviceContext */




NTSTATUS
Lpx_CreateControlDevice( 
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	DeviceName
	)
{
    NTSTATUS status;
	PDEVICE_OBJECT	DeviceObject;
    PCONTROL_DEVICE_CONTEXT deviceContext;
    PTP_CONNECTION Connection;
    PTP_ADDRESS Address;
	UNICODE_STRING		unicodeDeviceName;
	ULONG				i;
    UINT MaxUserData;
	
	// initialize out Parameter
	DeviceObject = NULL;

    if (global.Config == NULL) {
        //
        // This allocates the CONFIG_DATA structure and returns
        // it in global.Config.
        //

        status = Lpx_ConfigureTransport(&global.RegistryPath, &global.Config);

        if (!NT_SUCCESS (status)) {
            PANIC (" Failed to initialize transport, binding failed.\n");
           status = NDIS_STATUS_RESOURCES;
            return status;
        }

    }



   	//
    // Create the device object for LpxControlDevice.
    //

    status = IoCreateDevice(
                 DriverObject,
                 sizeof (CONTROL_DEVICE_CONTEXT) - sizeof (DEVICE_OBJECT) +
                     (DeviceName->Length + sizeof(UNICODE_NULL)),
                 DeviceName,
                 FILE_DEVICE_TRANSPORT,
                 FILE_DEVICE_SECURE_OPEN,
                 FALSE,
                 &DeviceObject);
	
    if (!NT_SUCCESS(status)) {
		DebugPrint(0,("Error Createing Device Object!!!!!\n"));
        return status;
    }

	DebugPrint(0,("ControlDevice %x\n",DeviceObject));
    DeviceObject->Flags |= DO_DIRECT_IO;

	global.ControlDeviceObject = DeviceObject;
	DebugPrint(0,("global.ControlDeviceObject %x\n",global.ControlDeviceObject));
	
    deviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
	
	//
    // Initialize our part of the device context.
    //

    RtlZeroMemory(
        ((PUCHAR)deviceContext) + sizeof(DEVICE_OBJECT),
        sizeof(CONTROL_DEVICE_CONTEXT) - sizeof(DEVICE_OBJECT));
	
 	
	// 
	// initialize Device Context
	// 	
	// 					NEED ADDITIONAL CODE
    
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


	deviceContext->MaxSendPacketSize = 1500;

    //
    // Get the information we need about the adapter, based on
    // the media type.
    //

	deviceContext->MacInfo.DestinationOffset = 0;
	deviceContext->MacInfo.SourceOffset = 6;
	deviceContext->MacInfo.SourceRouting = FALSE;
	deviceContext->MacInfo.AddressLength = 6;
	deviceContext->MacInfo.TransferDataOffset = 0;
	deviceContext->MacInfo.MaxHeaderLength = 14;
	deviceContext->MacInfo.MediumType = NdisMedium802_3;
	deviceContext->MacInfo.MediumAsync = FALSE;
    deviceContext->MacInfo.QueryWithoutSourceRouting = FALSE;   
    deviceContext->MacInfo.AllRoutesNameRecognized = FALSE;


    MacReturnMaxDataSize(
        &deviceContext->MacInfo,
        NULL,
        0,
        deviceContext->MaxSendPacketSize,
        TRUE,
        &MaxUserData);



	deviceContext->Information.Version = 0x0100;
    deviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    deviceContext->Information.MaxConnectionUserData = 0;
    deviceContext->Information.MaxDatagramSize =
        MaxUserData - sizeof(LPX_HEADER2); 
    deviceContext->Information.ServiceFlags = LPX_SERVICE_FLAGS;
    if (deviceContext->MacInfo.MediumAsync) {
        deviceContext->Information.ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
    }
    deviceContext->Information.MinimumLookaheadData =
        240 - sizeof(LPX_HEADER2);
    deviceContext->Information.MaximumLookaheadData =
        deviceContext->MaxReceivePacketSize - (sizeof(LPX_HEADER2)+sizeof(ETHERNET_HEADER));
    deviceContext->Information.NumberOfResources = LPX_TDI_RESOURCES;
    KeQuerySystemTime (&deviceContext->Information.StartTime);




	//
	// Set Device type
	//

	deviceContext->Type = CONTROL_DEVICE_SIGNATURE;
	deviceContext->Size = sizeof(CONTROL_DEVICE_CONTEXT);
	deviceContext->ReferenceCount  = 1;	



    deviceContext->UniqueIdentifier = 1;
    deviceContext->ControlChannelIdentifier = 1;

	//
	// Init pool list 
	//

	InitializeListHead (&deviceContext->ConnectionPool);
    InitializeListHead (&deviceContext->AddressPool);
    InitializeListHead (&deviceContext->AddressDatabase);
	InitializeListHead (&deviceContext->ConnectionDatabase);

    KeInitializeSpinLock (&deviceContext->Interlock);
    KeInitializeSpinLock (&deviceContext->SpinLock);	

   //
    // Initialize the resource that guards address ACLs.
    //

    ExInitializeResource (&deviceContext->AddressResource);


	//
	// init device state
	//
	deviceContext->State = DEVICECONTEXT_STATE_OPENING;


	deviceContext->PortNum = 0x4000;

	//
	// generate pool elements
	//

    for (i=0; i<global.Config->InitAddresses; i++) {

        lpx_AllocateAddress (deviceContext, &Address);
        if (Address == NULL) {
            PANIC ("NbfInitialize:  insufficient memory to allocate addresses.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&deviceContext->AddressPool, &Address->Linkage);
	}


	for (i=0; i<global.Config->InitConnections; i++) {

		lpx_AllocateConnection (deviceContext, &Connection);
		if (Connection == NULL) {
			PANIC ("NbfInitialize:  insufficient memory to allocate connections.\n");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto cleanup;
		}
		InsertTailList (&deviceContext->ConnectionPool, &Connection->LinkList);
	}


	// registering symbolic link
    RtlInitUnicodeString(&unicodeDeviceName, SOCKETLPX_DOSDEVICE_NAME);
    IoCreateSymbolicLink(&unicodeDeviceName, DeviceName);


	// register Tdi provider device

	DebugPrint(0, ("TdiRegisterDeviceObject for %S\n", DeviceName->Buffer));
    

    status = TdiRegisterDeviceObject(DeviceName,
                                     &deviceContext->TdiDeviceHandle);
	
	deviceContext->State = DEVICECONTEXT_STATE_OPEN;
    return STATUS_SUCCESS;

cleanup:
 
    //
    // Cleanup whatever device context we were initializing
    // when we failed.
    //
   
    ASSERT(status != STATUS_SUCCESS);
    
    if (InterlockedExchange(&deviceContext->CreateRefRemoved, TRUE) == FALSE) {

   
        // Remove creation reference
        LPX_DEREFERENCE_DEVICECONTEXT(deviceContext);
    }

    

   return status;

}




VOID
lpx_DestroyControlDeviceContext(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    )


{
    KIRQL       oldIrql;

    ACQUIRE_DEVICES_LIST_LOCK();

    // Is ref count zero - or did a new rebind happen now
    // See rebind happen in NbfReInitializeDeviceContext
    if (DeviceContext->ReferenceCount != 0)
    {
        // A rebind happened while we waited for the lock
        RELEASE_DEVICES_LIST_LOCK();
        return;
    }

    
    RELEASE_DEVICES_LIST_LOCK();

    // Mark the adapter as going away to prevent activity
    DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

 
    // Remove all the storage associated with the device.
    LpxFreeControlResources (DeviceContext);

    // Cleanup any kernel resources
    ExDeleteResource (&DeviceContext->AddressResource);
	DebugPrint(0,("lpx_DestroyControlDeviceContext2 %x\n",DeviceContext));   
    // Delete device from IO space
    IoDeleteDevice ((PDEVICE_OBJECT)DeviceContext);
  
    return;
}



VOID
LpxFreeControlResources (
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    )
{
	PTP_ADDRESS	address;
	PTP_CONNECTION connection;
    PLIST_ENTRY p;
 
	//
    // Clean up Packet In Progress List.
    //
	//
	//
	
	//
    // Clean up address pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressPool) ) {
        p = RemoveHeadList (&DeviceContext->AddressPool);
        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        lpx_DeallocateAddress (DeviceContext, address);
    }



    //
    // Clean up connection pool.
    //

    while ( !IsListEmpty (&DeviceContext->ConnectionPool) ) {
        p  = RemoveHeadList (&DeviceContext->ConnectionPool);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

        lpx_DeallocateConnection (DeviceContext, connection);
    }


}   /* NbfFreeResources */







NTSTATUS
LpxCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING DeviceName,
    IN OUT PDEVICE_CONTEXT *DeviceContext
    )
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_CONTEXT deviceContext;
    USHORT i;


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

    //
    // Initialize our part of the device context.
    //

    RtlZeroMemory(
        ((PUCHAR)deviceContext) + sizeof(DEVICE_OBJECT),
        sizeof(DEVICE_CONTEXT) - sizeof(DEVICE_OBJECT));

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
    // Initialize device context fields.
    //

 

    //
    // Initialize the reference count.
    //

    deviceContext->ReferenceCount = 1;
    deviceContext->CreateRefRemoved = FALSE;

    //
    // initialize the various fields in the device context
    //


    KeInitializeSpinLock (&deviceContext->Interlock);
    KeInitializeSpinLock (&deviceContext->SpinLock);
 

	deviceContext->NdisSendsInProgress = 0;
	InitializeListHead (&deviceContext->NdisSendQueue);
    InitializeListHead (&deviceContext->IrpCompletionQueue);

 
	InitializeListHead(&deviceContext->PacketInProgressList);
	KeInitializeSpinLock(&deviceContext->PacketInProgressQSpinLock);


    deviceContext->State = DEVICECONTEXT_STATE_OPENING;


    //
    // Initialize the resource that guards address ACLs.
    //

    ExInitializeResource (&deviceContext->AddressResource);

 
    //
    // set the netbios multicast address for this network type
    //

    for (i=0; i<HARDWARE_ADDRESS_LENGTH; i++) {
        deviceContext->LocalAddress.Address [i] = 0; // set later
        deviceContext->NetBIOSAddress.Address [i] = 0;
    }

     deviceContext->Type = NDIS_DEVICE_SIGNATURE;;
     deviceContext->Size = sizeof (DEVICE_CONTEXT);

    *DeviceContext = deviceContext;
    return STATUS_SUCCESS;
}




VOID
LpxReInitializeDeviceContext(
                                OUT PNDIS_STATUS NdisStatus,
                                IN PDRIVER_OBJECT DriverObject,
                                IN PCONFIG_DATA NbfConfig,
                                IN PUNICODE_STRING BindName,
                                IN PUNICODE_STRING ExportName,
                                IN PVOID SystemSpecific1,
                                IN PVOID SystemSpecific2
                            )
{
    PDEVICE_CONTEXT DeviceContext;
    KIRQL oldIrql;
	PLIST_ENTRY p;
    NTSTATUS status;
    UNICODE_STRING DeviceString;
    UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_NETBIOS];
    PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
    PTDI_ADDRESS_NETBIOS NetBIOSAddress =
                                    (PTDI_ADDRESS_NETBIOS)pAddress->Address;
    struct {
        TDI_PNP_CONTEXT tdiPnPContextHeader;
        PVOID           tdiPnPContextTrailer;
    } tdiPnPContext1, tdiPnPContext2;


  
    DebugPrint(1, ("ENTER NbfReInitializeDeviceContext for %S\n",
                    ExportName->Buffer));
   

	//
	// Search the list of NBF devices for a matching device name
	//
	
    ACQUIRE_DEVICES_LIST_LOCK();

    for (p = global.NIC_DevList.Flink ; p != &global.NIC_DevList; p = p->Flink)
    {
        DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, Linkage);

        RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

        if (NdisEqualString(&DeviceString, ExportName, TRUE)) {
        					
            // This has to be a rebind - otherwise something wrong

        	ASSERT(DeviceContext->CreateRefRemoved == TRUE);

            // Reference within lock so that it is not cleaned up

            LPX_REFERENCE_DEVICECONTEXT(DeviceContext);

            break;
        }
	}

    RELEASE_DEVICES_LIST_LOCK();

	if (p == &global.NIC_DevList)
	{
        
            DebugPrint(0, ("LEAVE NbfReInitializeDeviceContext for %S with Status %08x\n",
                            ExportName->Buffer,
                            STATUS_NOT_FOUND));
       

        *NdisStatus = STATUS_NOT_FOUND;

	    return;
	}

    //
    // Fire up NDIS again so this adapter talks
    //

    status = LpxInitializeNdis (DeviceContext,
					            NbfConfig,
					            BindName);

    if (!NT_SUCCESS (status)) {
		goto Cleanup;
	}

    // Store away the PDO for the underlying object
    DeviceContext->PnPContext = SystemSpecific2;

    DeviceContext->State = DEVICECONTEXT_STATE_OPEN;
	
  
	//
	// Re-Indicate to TDI that new binding has arrived
	//

    status = TdiRegisterDeviceObject(&DeviceString,
                                     &DeviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
        goto Cleanup;
	}


    pAddress->AddressLength = TDI_ADDRESS_LENGTH_NETBIOS;
    pAddress->AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    NetBIOSAddress->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;

    RtlCopyMemory(NetBIOSAddress->NetbiosName,
                  DeviceContext->ReservedNetBIOSAddress, 16);

    tdiPnPContext1.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext1.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_IF_NAME;
    *(PVOID UNALIGNED *) &tdiPnPContext1.tdiPnPContextHeader.ContextData = &DeviceString;

    tdiPnPContext2.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext2.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_PDO;
    *(PVOID UNALIGNED *) &tdiPnPContext2.tdiPnPContextHeader.ContextData = SystemSpecific2;

    
	DebugPrint(0, ("TdiRegisterNetAddress on %S ", DeviceString.Buffer));
	DebugPrint(0, ("for %02x%02x%02x%02x%02x%02x\n",
					NetBIOSAddress->NetbiosName[10],
					NetBIOSAddress->NetbiosName[11],
					NetBIOSAddress->NetbiosName[12],
					NetBIOSAddress->NetbiosName[13],
					NetBIOSAddress->NetbiosName[14],
					NetBIOSAddress->NetbiosName[15]));
   


{ 
	PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;
    RtlCopyMemory (
		&LpxAddress->Node,
		DeviceContext->LocalAddress.Address,
		HARDWARE_ADDRESS_LENGTH
		);
} 

    status = TdiRegisterNetAddress(pAddress,
                                   &DeviceString,
                                   (TDI_PNP_CONTEXT *) &tdiPnPContext2,
                                   &DeviceContext->ReservedAddressHandle);

    if (!NT_SUCCESS (status)) {
        goto Cleanup;
    }

    // Put the creation reference back again
    LPX_REFERENCE_DEVICECONTEXT (DeviceContext);

    DeviceContext->CreateRefRemoved = FALSE;

    status = NDIS_STATUS_SUCCESS;

Cleanup:

    if (status != NDIS_STATUS_SUCCESS)
    {
        // Stop all internal timers
       
    }

    LPX_DEREFERENCE_DEVICECONTEXT (DeviceContext);

	*NdisStatus = status;

   
	DebugPrint(0,("LEAVE NbfReInitializeDeviceContext for %S with Status %08x\n",
				ExportName->Buffer,
				status));
    

	return;
}





ULONG
lpx_CreateNdisDeviceContext(	    
           OUT PNDIS_STATUS NdisStatus,
            IN PDRIVER_OBJECT DriverObject,
            IN PCONFIG_DATA NbfConfig,
            IN PUNICODE_STRING BindName,
            IN PUNICODE_STRING ExportName,
            IN PVOID SystemSpecific1,
            IN PVOID SystemSpecific2
	)
{
    ULONG i;
    PDEVICE_CONTEXT DeviceContext;
	PDEVICE_OBJECT	DeviceObject;
    PTP_CONNECTION Connection;
    PTP_ADDRESS Address;
    PNDIS_PACKET NdisPacket;
    KIRQL oldIrql;
    NTSTATUS status;
    UINT MaxUserData;
    ULONG InitReceivePackets;
    BOOLEAN UniProcessor;
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

#if !defined(__XP__)
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

    status = LpxCreateDeviceContext(
                                    DriverObject,
                                    ExportName,
                                    &DeviceContext
                                   );

    if (!NT_SUCCESS (status)) {

  
          DebugPrint(0, ("LpxCreateNdisDeviceContext for %S returned error %08x\n",
                            ExportName->Buffer, status));
      

		//
		// First check if we already have an object with this name
		// This is because a previous unbind was not done properly.
		//

    	if (status == STATUS_OBJECT_NAME_COLLISION) {

			// See if we can reuse the binding and device name
			
			LpxReInitializeDeviceContext(
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
			
				return lpx_CreateNdisDeviceContext(
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
            return(0);
		}
		
    	return(1);
	}

    DeviceContext->UniProcessor = UniProcessor;
    DeviceContext->MaxConnections = global.Config->MaxConnections;
    DeviceContext->MaxAddresses = global.Config->MaxAddresses;

    //
    // Now fire up NDIS so this adapter talks
    //

    status = LpxInitializeNdis (DeviceContext,
                                global.Config,
                                BindName);

    if (!NT_SUCCESS (status)) {

        //
        // Log an error if we were failed to
        // open this adapter.
        //

        if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
            LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext);
        }
        
        *NdisStatus = status;
        return(0);

    }


    
    DebugPrint(0,("NbfInitialize: NDIS returned: %x %x %x %x %x %x as local address.\n",
            DeviceContext->LocalAddress.Address[0],
            DeviceContext->LocalAddress.Address[1],
            DeviceContext->LocalAddress.Address[2],
            DeviceContext->LocalAddress.Address[3],
            DeviceContext->LocalAddress.Address[4],
            DeviceContext->LocalAddress.Address[5]));
    

    //
    // Initialize our provider information structure; since it
    // doesn't change, we just keep it around and copy it to
    // whoever requests it.
    //


    MacReturnMaxDataSize(
        &DeviceContext->MacInfo,
        NULL,
        0,
        DeviceContext->MaxSendPacketSize,
        TRUE,
        &MaxUserData);

   
    // Store away the PDO for the underlying object
    DeviceContext->PnPContext = SystemSpecific2;

    DeviceContext->State = DEVICECONTEXT_STATE_OPEN;


    ACQUIRE_DEVICES_LIST_LOCK();
    InsertTailList (&global.NIC_DevList, &DeviceContext->Linkage);
    RELEASE_DEVICES_LIST_LOCK();

    DeviceObject = (PDEVICE_OBJECT) DeviceContext;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

    
    DebugPrint(0, ("TdiRegisterDeviceObject for %S\n", DeviceString.Buffer));
    

    status = TdiRegisterDeviceObject(&DeviceString,
                                     &DeviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
        RemoveEntryList(&DeviceContext->Linkage);
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

    
    DebugPrint(0, ("TdiRegisterNetAddress on %S ", DeviceString.Buffer));
    DebugPrint(0, ("for %02x%02x%02x%02x%02x%02x\n",
                            NetBIOSAddress->NetbiosName[10],
                            NetBIOSAddress->NetbiosName[11],
                            NetBIOSAddress->NetbiosName[12],
                            NetBIOSAddress->NetbiosName[13],
                            NetBIOSAddress->NetbiosName[14],
                            NetBIOSAddress->NetbiosName[15]));
   

{
	PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;
    RtlCopyMemory (
		&LpxAddress->Node,
		DeviceContext->LocalAddress.Address,
		HARDWARE_ADDRESS_LENGTH
		);
} 


    status = TdiRegisterNetAddress(pAddress,
                                   &DeviceString,
                                   (TDI_PNP_CONTEXT *) &tdiPnPContext2,
                                   &DeviceContext->ReservedAddressHandle);

    if (!NT_SUCCESS (status)) {
        RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }

	NdisAllocatePacketPool(
		&status,
        &DeviceContext->LpxPacketPool,
        TRANSMIT_PACKETS,
        sizeof(LPX_RESERVED)
		);
	NdisAllocateBufferPool(
		&status,
        &DeviceContext->LpxBufferPool,
		TRANSMIT_PACKETS
		);


    LPX_REFERENCE_DEVICECONTEXT(DeviceContext);

   
    *NdisStatus = NDIS_STATUS_SUCCESS;

    return(1);

cleanup:


    *NdisStatus = status;
    ASSERT(status != STATUS_SUCCESS);
    
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

        // Stop all internal timers
        // do something

        // Remove creation reference
        LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext);
    }

    

    return (0);
}



VOID
lpx_DestroyNdisDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    KIRQL       oldIrql;

    ACQUIRE_DEVICES_LIST_LOCK();

    // Is ref count zero - or did a new rebind happen now
    // See rebind happen in NbfReInitializeDeviceContext
    if (DeviceContext->ReferenceCount != 0)
    {
        // A rebind happened while we waited for the lock
        RELEASE_DEVICES_LIST_LOCK();
        return;
    }

    // Splice this adapter of the list of device contexts
    RemoveEntryList (&DeviceContext->Linkage);
    
    RELEASE_DEVICES_LIST_LOCK();

    // Mark the adapter as going away to prevent activity
    DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

    // Free the packet pools, etc. and close the adapter.
    LpxCloseNdis (DeviceContext);

    // Remove all the storage associated with the device.
    LpxFreeNdisResources (DeviceContext);

    // Cleanup any kernel resources
    ExDeleteResource (&DeviceContext->AddressResource);
        DebugPrint(0, ("LEAVE LpxProtocolUnbindAdapter for %S \n",
                        DeviceContext->DeviceName));
    // Delete device from IO space
    IoDeleteDevice ((PDEVICE_OBJECT)DeviceContext);
        
    return;
}


VOID
LpxFreeNdisResources (
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    PLIST_ENTRY p;
 
	//
    // Clean up Packet In Progress List.
    //
	//
	//
	//	modified by hootch 09042003
    while ( p = ExInterlockedRemoveHeadList(&DeviceContext->PacketInProgressList,
				&DeviceContext->PacketInProgressQSpinLock) ) {
		PNDIS_PACKET	Packet;
		PLPX_RESERVED	reserved;

		reserved = CONTAINING_RECORD(p, LPX_RESERVED, ListElement);
		Packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);

		PacketFree(Packet);
    }

	if(DeviceContext->LpxPacketPool) {
	    NdisFreePacketPool(DeviceContext->LpxPacketPool);
		DeviceContext->LpxPacketPool = NULL;
		NdisFreeBufferPool(DeviceContext->LpxBufferPool);
		DeviceContext->LpxBufferPool = NULL;
	}

    return;

}   /* NbfFreeResources */
