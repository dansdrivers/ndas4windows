#include "precomp.h"
#pragma hdrstop
//
//	Address managing function definition
//
VOID
lpx_ReferenceAddress(
    IN PTP_ADDRESS Address
    )
{

    ASSERT (Address->ReferenceCount > 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&Address->ReferenceCount);
DebugPrint(4,("lpx_ReferenceAddress : count %d\n", Address->ReferenceCount));
} /* __lpx_RefAddress */



VOID
lpx_DereferenceAddress(
    IN PTP_ADDRESS Address
    )
{
    LONG result;

    result = InterlockedDecrement (&Address->ReferenceCount);
DebugPrint(4,("lpx_DereferenceAddress : count %d\n", Address->ReferenceCount));
    //
    // If we have deleted all references to this address, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the address any longer.
    //

    ASSERT (result >= 0);

    if (result == 0) {

        ASSERT ((Address->State & ADDRESS_STATE_CLOSING) != 0);
        
        ExInitializeWorkItem(
            &Address->u.DestroyAddressQueueItem,
            lpx_DestroyAddress,
            (PVOID)Address);
        ExQueueWorkItem(&Address->u.DestroyAddressQueueItem, DelayedWorkQueue);
    }
} /* __lpx_DerefAddress */



VOID
lpx_DeallocateAddress(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS TransportAddress
    )
{
    ExFreePool (TransportAddress);
    --DeviceContext->AddressAllocated;


}   /* __lpx_DeallocateAddress */



VOID
lpx_AllocateAddress(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS *TransportAddress
    )
{
    PTP_ADDRESS Address;
    NDIS_STATUS NdisStatus;
    PNDIS_PACKET NdisPacket;
    PNDIS_BUFFER NdisBuffer;

 

    Address = (PTP_ADDRESS)ExAllocatePoolWithTag (
                               NonPagedPool,
                               sizeof (TP_ADDRESS),
                               LPX_MEM_TAG_TP_ADDRESS);
    if (Address == NULL) {
        PANIC("LPX: Could not allocate address: no pool\n");
        *TransportAddress = NULL;
        return;
    }
    RtlZeroMemory (Address, sizeof(TP_ADDRESS));

    ++DeviceContext->AddressAllocated;

    Address->Type = LPX_ADDRESS_SIGNATURE;
    Address->Size = sizeof (TP_ADDRESS);

    Address->Provider = DeviceContext;
	Address->PacketProvider = NULL;
    KeInitializeSpinLock (&Address->SpinLock);
    InitializeListHead (&Address->ConnectionDatabase);
	InitializeListHead (&Address->ConnectionServicePointList);
    *TransportAddress = Address;
}   /* __lpx_AllocateAddress */






NTSTATUS
lpx_CreateAddress(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PDEVICE_CONTEXT RealDeviceContext,
    IN PNBF_NETBIOS_ADDRESS NetworkName,
    OUT PTP_ADDRESS *Address
    )
{
    PTP_ADDRESS pAddress;
    PLIST_ENTRY p;


    p = RemoveHeadList (&DeviceContext->AddressPool);
    if (p == &DeviceContext->AddressPool) {

        if ((DeviceContext->AddressMaxAllocated == 0) ||
            (DeviceContext->AddressAllocated < DeviceContext->AddressMaxAllocated)) {
DebugPrint(1,("Get From lpx_AllocateAddress\n") );
            lpx_AllocateAddress (DeviceContext, &pAddress);


        } else {
            pAddress = NULL;

        }

        if (pAddress == NULL) {
            ++DeviceContext->AddressExhausted;
            PANIC ("LpxCreateAddress: Could not allocate address object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {
DebugPrint(1,("Get From DeviceContext->AddressPool\n") );
        pAddress = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

    }

    ++DeviceContext->AddressInUse;
    if (DeviceContext->AddressInUse > DeviceContext->AddressMaxInUse) {
         ++DeviceContext->AddressMaxInUse;
    }


    //
    // Initialize all of the static data for this address.
    //
    pAddress->Type = LPX_ADDRESS_SIGNATURE;
    pAddress->Size = sizeof (TP_ADDRESS);


    KeInitializeSpinLock (&pAddress->SpinLock);
    InitializeListHead (&pAddress->ConnectionDatabase);
	InitializeListHead (&pAddress->ConnectionServicePointList);

    pAddress->ReferenceCount = 1;

    pAddress->State = ADDRESS_STATE_OPENING;

    pAddress->NetworkName = NetworkName;
    
	ASSERT(pAddress->NetworkName);
    //
    // Now link this address into the specified device context's
    // address database.  To do this, we need to acquire the spin lock
    // on the device context.
    //

    pAddress->FileObject = NULL;
	pAddress->Provider = DeviceContext;
	pAddress->PacketProvider = RealDeviceContext;
    pAddress->CloseIrp = (PIRP)NULL;

    //
    // Initialize the request handlers.
    //

    pAddress->RegisteredConnectionHandler = FALSE;
    pAddress->ConnectionHandler = TdiDefaultConnectHandler;
    pAddress->ConnectionHandlerContext = NULL;
    pAddress->RegisteredDisconnectHandler = FALSE;
    pAddress->DisconnectHandler = TdiDefaultDisconnectHandler;
    pAddress->DisconnectHandlerContext = NULL;
    pAddress->RegisteredReceiveHandler = FALSE;
    pAddress->ReceiveHandler = TdiDefaultReceiveHandler;
    pAddress->ReceiveHandlerContext = NULL;
    pAddress->RegisteredReceiveDatagramHandler = FALSE;
    pAddress->ReceiveDatagramHandler = TdiDefaultRcvDatagramHandler;
    pAddress->ReceiveDatagramHandlerContext = NULL;
    pAddress->RegisteredExpeditedDataHandler = FALSE;
    pAddress->ExpeditedDataHandler = TdiDefaultRcvExpeditedHandler;
    pAddress->ExpeditedDataHandlerContext = NULL;
    pAddress->RegisteredErrorHandler = FALSE;
    pAddress->ErrorHandler = TdiDefaultErrorHandler;
    pAddress->ErrorHandlerContext = NULL;



    InsertTailList (&DeviceContext->AddressDatabase, &pAddress->Linkage);
   
    LPX_REFERENCE_DEVICECONTEXT (DeviceContext);   // CONTROl ref Device + : 1
	LPX_REFERENCE_DEVICECONTEXT	(RealDeviceContext);	// NDIS ref Device + : 1		
    *Address = pAddress;                // return the address.
    return STATUS_SUCCESS;              // not finished yet.
} /* __lpx_CreateAddress */


VOID
lpx_DestroyAddress(
    IN PVOID Parameter
    )
{
    KIRQL oldirql;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    PDEVICE_CONTEXT		RealDeviceContext;
    PTP_ADDRESS Address = (PTP_ADDRESS)Parameter;
	PIRP CloseIrp;

DebugPrint(DebugLevel,("lpx_DestroyAddress\n"));
    DeviceContext = Address->Provider;
	RealDeviceContext = Address->PacketProvider;
	
    SeDeassignSecurity (&Address->SecurityDescriptor);

    //
    // Delink this address from its associated device context's address
    // database.  To do this we must spin lock on the device context object,
    // not on the address.
    //

    Address->FileObject->FsContext = NULL;
    Address->FileObject->FsContext2 = NULL;
	
	CloseIrp = Address->CloseIrp;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);			//CONTROL + : 1


	ASSERT(Address->NetworkName);
// do something 
    RemoveEntryList (&Address->Linkage);

    if (Address->NetworkName != NULL) {
        ExFreePool (Address->NetworkName);
        Address->NetworkName = NULL;
    }

    //
    // Now we can deallocate the transport address object.
    //

    --DeviceContext->AddressInUse;

    if ((DeviceContext->AddressAllocated - DeviceContext->AddressInUse) >
            DeviceContext->AddressInitAllocated) {
        lpx_DeallocateAddress (DeviceContext, Address);

    } else {
        InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);			//CONTROL - : 0


    LPX_DEREFERENCE_DEVICECONTEXT (DeviceContext);   //CONTROL Device ref - : -1
    LPX_DEREFERENCE_DEVICECONTEXT (RealDeviceContext);   // Ndis Device ref - : -1

		
	if (CloseIrp != (PIRP)NULL) {
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);
    }

} /* __lpx_DestroyAddress */


VOID
lpx_StopAddress(
    IN PTP_ADDRESS Address
    )
{
    KIRQL oldirql, oldirql1;
    PTP_CONNECTION connection;
    PLIST_ENTRY p;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
	BOOL fStopping;
	
    DeviceContext = Address->Provider;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);	//Address + : 1

    //
    // If we're already stopping this address, then don't try to do it again.
    //

    if (!(Address->State & ADDRESS_STATE_CLOSING)) {

        lpx_ReferenceAddress (Address);		//address ref + : 1

        //
        // Run down all addressfiles on this address. This
        // will leave the address with no references
        // potentially, but we don't need a temp one
        // because every place that calls NbfStopAddress
        // already has a temp reference.
        //

        while (!IsListEmpty (&Address->ConnectionDatabase)) {
            p = RemoveHeadList (&Address->ConnectionDatabase);
            connection = CONTAINING_RECORD (p, TP_CONNECTION, Linkage);
 
			ACQUIRE_DPC_SPIN_LOCK (&connection->SpinLock);	//Connection + : 1

			if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {

				//
				// It is in the process of being disassociated already.
				//

				RELEASE_DPC_SPIN_LOCK (&connection->SpinLock);	//connection - : 0
				continue;
			}

			connection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
			connection->Flags2 |= CONNECTION_FLAGS2_DESTROY;    // BUGBUG: Is this needed?

			RemoveEntryList (&connection->Linkage);
			InitializeListHead (&connection->Linkage); 
			connection->Address = NULL;

			fStopping = connection->Flags2 & CONNECTION_FLAGS2_STOPPING;



			if (!fStopping) {

				lpx_ReferenceConnection (connection);	//connection ref + : 1

			}

			RELEASE_DPC_SPIN_LOCK (&connection->SpinLock);	//connection - : 0
				
			RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);		//Address - : 0



			if (!fStopping) {

					ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);
					lpx_StopConnection (connection, STATUS_LOCAL_DISCONNECT);
					RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);
					lpx_DereferenceConnection (connection);		//connection ref - : 0
			}

			lpx_DereferenceAddress (Address);		//address ref - : -1 // accumuated
 
			ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);	//address + : 1
        }

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);	// address - : 0

        lpx_DereferenceAddress (Address);	//address ref - : 0

    } else {

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);	//address - : 0

    }

} /* __lpx_StopAddress */






//
//	managing Open Address / Address file operation
//



PDEVICE_CONTEXT
lpx_FindDeviceContext(
    PNBF_NETBIOS_ADDRESS networkName
	)
{

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

	ACQUIRE_DEVICES_LIST_LOCK();	// Global NDIS Device + : 1

	if (IsListEmpty (&(global.NIC_DevList)) ){
		RELEASE_DEVICES_LIST_LOCK();	// Global NDIS Device - : 0
		return NULL;
	}

	listHead = &(global.NIC_DevList);
	for(deviceContext = NULL, thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = thisEntry->Flink)
	{

        deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
		if (RtlEqualMemory (
				deviceContext->LocalAddress.Address,
				&networkName->LpxAddress.Node,
				HARDWARE_ADDRESS_LENGTH
				))
		{
			break;
		}
		
		deviceContext = NULL;
	}

	if(deviceContext == NULL 
		&& RtlEqualMemory (
				notAssigned,
				&networkName->LpxAddress.Node,
				HARDWARE_ADDRESS_LENGTH
				)
		&& global.LpxPrimaryDeviceContext) 
	{
		deviceContext = global.LpxPrimaryDeviceContext;
		ASSERT(deviceContext);
	    RtlCopyMemory (
			&networkName->LpxAddress.Node,
			deviceContext->LocalAddress.Address,
			HARDWARE_ADDRESS_LENGTH
			);
	}

	RELEASE_DEVICES_LIST_LOCK();	// Global NDIS Device - : 0

	return deviceContext;

}



TDI_ADDRESS_NETBIOS *
lpx_ParseTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN BOOLEAN BroadcastAddressOk
)

/*++

Routine Description:

    This routine scans a TRANSPORT_ADDRESS, looking for an address
    of type TDI_ADDRESS_TYPE_NETBIOS.

Arguments:

    Transport - The generic TDI address.

    BroadcastAddressOk - TRUE if we should return the broadcast
        address if found. If so, a value of (PVOID)-1 indicates
        the broadcast address.

Return Value:

    A pointer to the Netbios address, or NULL if none is found,
    or (PVOID)-1 if the broadcast address is found.

--*/

{
    TA_ADDRESS * addressName;
    INT i;

    addressName = &TransportAddress->Address[0];

    //
    // The name can be passed with multiple entries; we'll take and use only
    // the Netbios one.
    //

    for (i=0;i<TransportAddress->TAAddressCount;i++) {
        if (addressName->AddressType == TDI_ADDRESS_TYPE_LPX) {
            if ((addressName->AddressLength == 0) &&
                BroadcastAddressOk) {
                return (PVOID)-1;
            } else if (addressName->AddressLength == 
                        sizeof(TDI_ADDRESS_NETBIOS)) {
                return((TDI_ADDRESS_NETBIOS *)(addressName->Address));
            }
        }

        addressName = (TA_ADDRESS *)(addressName->Address +
                                                addressName->AddressLength);
    }
    return NULL;

}   /* __lpx_ParseTdiAddress */



BOOLEAN
lpx_ValidateTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN ULONG TransportAddressLength
)
{
    PUCHAR AddressEnd = ((PUCHAR)TransportAddress) + TransportAddressLength;
    TA_ADDRESS * addressName;
    INT i;

    if (TransportAddressLength < sizeof(TransportAddress->TAAddressCount)) {
        return FALSE;
    }

    addressName = &TransportAddress->Address[0];

    for (i=0;i<TransportAddress->TAAddressCount;i++) {
        if (addressName->Address > AddressEnd) {
           DebugPrint(8, ("LPXValidateTdiAddress: address too short\n"));
            return FALSE;
        }
        addressName = (TA_ADDRESS *)(addressName->Address +
                                                addressName->AddressLength);
    }

    if ((PUCHAR)addressName > AddressEnd) {
           DebugPrint(8, ("LPXValidateTdiAddress: address too short\n"));
        return FALSE;
    }
    return TRUE;

}   /* __lpx_ValidateTdiAddress */



NTSTATUS
lpx_OpenAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PCONTROL_DEVICE_CONTEXT DeviceContext;
	PDEVICE_CONTEXT	RealDeviceContext;
    NTSTATUS status;
    PTP_ADDRESS address;
    PNBF_NETBIOS_ADDRESS networkName;    // Network name string.
    PFILE_FULL_EA_INFORMATION ea;
    TRANSPORT_ADDRESS UNALIGNED *name;
    TDI_ADDRESS_NETBIOS *netbiosName;
    ULONG DesiredShareAccess;
    KIRQL oldirql;
    PACCESS_STATE AccessState;
    ACCESS_MASK GrantedAccess;
    BOOLEAN AccessAllowed;
    BOOLEAN QuickAdd = FALSE;
	KIRQL			addressOldIrql;


	
	DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
DebugPrint(1,("ENTER lpx_OpenAddress\n"));
DebugPrint(1,("irpSp ==> %0x\n",IrpSp));
	//
    // The network name is in the EA, passed in AssociatedIrp.SystemBuffer
    //

    ea = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    if (ea == NULL) {
        return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    //
    // this may be a valid name; parse the name from the EA and use it if OK.
    //

    name = (TRANSPORT_ADDRESS UNALIGNED *)&ea->EaName[ea->EaNameLength+1];

    if (!lpx_ValidateTdiAddress(name, ea->EaValueLength)) {		
        return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    //
    // The name can have with multiple entries; we'll use the Netbios one.
    // This call returns NULL if not Netbios address is found, (PVOID)-1
    // if it is the broadcast address, and a pointer to a Netbios
    // address otherwise.
    //

    netbiosName = lpx_ParseTdiAddress(name, TRUE);

    if (netbiosName != NULL) {
        if (netbiosName != (PVOID)-1) {
            networkName = (PNBF_NETBIOS_ADDRESS)ExAllocatePoolWithTag (
                                                NonPagedPool,
                                                sizeof (NBF_NETBIOS_ADDRESS),
                                                LPX_MEM_TAG_NETBIOS_NAME);
            if (networkName == NULL) {
NET_ERROR:
                PANIC ("LpxOpenAddress: PANIC! could not allocate networkName!\n");
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // get the name to local storage
            //

			QuickAdd = TRUE;
			RtlZeroMemory(networkName, sizeof(*networkName));
            RtlCopyMemory (networkName->LpxAddress.Node, netbiosName->NetbiosName, ETHERNET_ADDRESS_LENGTH);
			networkName->LpxAddress.Port = netbiosName->NetbiosNameType;

        } else {
        	// broadcast Address
            networkName = NULL;
			goto NET_ERROR;
        }

    } else {
        return STATUS_INVALID_ADDRESS_COMPONENT;
    }


    //
    // See if this address is already established.  This call automatically
    // increments the reference count on the address so that it won't disappear
    // from underneath us after this call but before we have a chance to use it.
    //
    // To ensure that we don't create two address objects for the
    // same address, we hold the device context AddressResource until
    // we have found the address or created a new one.
    //

    ACQUIRE_RESOURCE_EXCLUSIVE (&DeviceContext->AddressResource, TRUE);	

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);		//Device + : 1


	RealDeviceContext = lpx_FindDeviceContext(networkName);
	if(RealDeviceContext == NULL) {
		status = STATUS_INVALID_ADDRESS_COMPONENT;
ErrorOut:


        //
        // If the address could not be created, and is not in the process of
        // being created, then we can't open up an address.
        //

        if (networkName != NULL) {
            ExFreePool (networkName);
        }

       
        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);	//Device - : 0
		RELEASE_RESOURCE (&DeviceContext->AddressResource);
		return	status;
	}


	if(networkName->LpxAddress.Port == 0) {
		status = LpxAssignPort(DeviceContext, &networkName->LpxAddress);
		if(!NT_SUCCESS(status)) {
			goto ErrorOut;
		}
	}

    address = LpxLookupAddress (DeviceContext, &networkName->LpxAddress);	//Address ref + : 1

	if(address != NULL) {

        lpx_DereferenceAddress (address);	//Address ref - : 0
		status = STATUS_ADDRESS_ALREADY_EXISTS;
		goto ErrorOut;
	}else{

        //
        // This address doesn't exist. Create it, and start the process of
        // registering it.
        //

        status = lpx_CreateAddress (
                    DeviceContext,
                    RealDeviceContext,
                    networkName,
                    &address);

DebugPrint(1,("step 3-2 irpSp ==> %0x\n",IrpSp));		
        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);		//Device - : 0

        if (NT_SUCCESS (status)) {
            //
            // Initialize the shared access now. We use read access
            // to control all access.
            //
DebugPrint(1,("step 4 irpSp ==> %0x\n",IrpSp));
DebugPrint(1,("step 4 IrpSp->Parameters.Create.ShareAccess ==> %x\n",IrpSp->Parameters.Create.ShareAccess ));
            DesiredShareAccess = (ULONG)
				(((IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                  (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                        FILE_SHARE_READ : 0);

            IoSetShareAccess(
                FILE_READ_DATA,
                DesiredShareAccess,
                IrpSp->FileObject,
                &address->u.ShareAccess);

            //
            // Assign the security descriptor (need to do this with
            // the spinlock released because the descriptor is not
            // mapped. BUGBUG: Need to synchronize Assign and Access).
            //

            AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

            status = SeAssignSecurity(
                         NULL,                       // parent descriptor
                         AccessState->SecurityDescriptor,
                         &address->SecurityDescriptor,
                         FALSE,                      // is directory
                         &AccessState->SubjectSecurityContext,
                         &AddressGenericMapping,
                         PagedPool);


            if (!NT_SUCCESS(status)) {

                //
                // Error, return status.
                //
                IoRemoveShareAccess (IrpSp->FileObject, &address->u.ShareAccess);

                // Mark as stopping so that someone does not ref it again

                ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);		//Device + : 1
				DebugPrint(DebugLevel, ("lpx_OpenAddress : address->State |= ADDRESS_STATE_CLOSING\n") );
				address->State |= ADDRESS_STATE_CLOSING;
		        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);		//Device - : 0

				RELEASE_RESOURCE (&DeviceContext->AddressResource);
                lpx_DereferenceAddress (address);		//address ref - : -1
                return status;

            }

            RELEASE_RESOURCE (&DeviceContext->AddressResource);

            //
            // if the adapter isn't ready, we can't do any of this; get out
            //

            if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {

                status = STATUS_DEVICE_NOT_READY;

            } else {

                IrpSp->FileObject->FsContext = (PVOID)address;
                IrpSp->FileObject->FsContext2 =
                                    (PVOID)TDI_TRANSPORT_ADDRESS_FILE;
                address->FileObject = IrpSp->FileObject;

                lpx_ReferenceAddress(address);	//address ref + : 1


				address->State &= ~ADDRESS_STATE_OPENING;
				address->State = ADDRESS_STATE_OPEN;
				status = STATUS_SUCCESS;
           

            }

			lpx_DereferenceAddress(address);	//address ref - : 0 / -1
        } else {

            RELEASE_RESOURCE (&DeviceContext->AddressResource);

            //
            // If the address could not be created, and is not in the process of
            // being created, then we can't open up an address.
            //

            if (networkName != NULL) {
                ExFreePool (networkName);
            }

        }

    } 
	DebugPrint(1,("LEAVE lpx_OpenAddress\n"));
    return status;
} /* __Lpx_OpenAddress */



NTSTATUS
lpx_CloseAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PTP_ADDRESS address;

    address  = IrpSp->FileObject->FsContext;

    address->CloseIrp = Irp;

    //
    // We assume that addressFile has already been verified
    // at this point.
    //
    ASSERT (address);

    //
    // Remove us from the access info for this address.
    //

    ACQUIRE_RESOURCE_EXCLUSIVE (&address->Provider->AddressResource, TRUE);
	address->State |= ADDRESS_STATE_CLOSING;
    IoRemoveShareAccess (address->FileObject, &address->u.ShareAccess);
    RELEASE_RESOURCE (&address->Provider->AddressResource);
	
	DebugPrint(DebugLevel,("lpx_CloseAddress1\n"));

    lpx_StopAddress (address);
	DebugPrint(DebugLevel,("lpx_CloseAddress2\n"));
    //
    // This removes a reference added by our caller.
    //
	DebugPrint(DebugLevel,("lpx_CloseAddress3\n"));
    lpx_DereferenceAddress (address);	//address ref - : -1

    return STATUS_PENDING;

} /* __lpx_CloseAddress */



NTSTATUS
lpx_VerifyAddressObject (
    IN PTP_ADDRESS Address
    )
{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;
    PTP_ADDRESS address;

 
    try {

        if ((Address != (PTP_ADDRESS)NULL) &&
            (Address->Size == sizeof (TP_ADDRESS)) &&
            (Address->Type == LPX_ADDRESS_SIGNATURE) ) 
		{   
			ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);	//address + : 1

			if ((Address->State & ADDRESS_STATE_CLOSING) == 0) {

				lpx_ReferenceAddress (Address);	//address ref + : 1

			} else {
				DebugPrint(DebugLevel,("Address->State & ADDRESS_STATE_CLOSING != NULL\n"));
				status = STATUS_INVALID_ADDRESS;
			}

			RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);	//address - : 0

		} else {
			DebugPrint(DebugLevel,(" ADDRESS : IN VALID ADDRESS\n"));
			status = STATUS_INVALID_ADDRESS;
		}
	
    } except(EXCEPTION_EXECUTE_HANDLER) {

         return GetExceptionCode();
    }

    return status;

}
