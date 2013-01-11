#include "precomp.h"
#pragma hdrstop

VOID
lpx_StopControlChannel(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN USHORT ChannelIdentifier
    )
{

   return;
}


NTSTATUS
lpx_TdiAccept(
    IN PIRP Irp
    )
{
    PTP_CONNECTION connection;
	PSERVICE_POINT	servicePoint;
    PIO_STACK_LOCATION irpSp;
    KIRQL oldIrql, cancelIrql;
    NTSTATUS status;

    //
    // Get the connection this is associated with; if there is none, get out.
    // This adds a connection reference of type BY_ID if successful.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection = irpSp->FileObject->FsContext;

    //
    // This adds a connection reference of type BY_ID if successful.
    //

    status = lpx_VerifyConnectionObject (connection);	//connection ref + : 1

    if (!NT_SUCCESS (status)) {
        return status;
    }

	servicePoint = (PSERVICE_POINT)connection;

	DebugPrint(2, ("LpxAccept servicePoint = %p, servicePoint->SmpState = 0x%x\n", 
		servicePoint, servicePoint->SmpState));

	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;			//servicePoint	+ : 1

	if(servicePoint->SmpState != SMP_ESTABLISHED) {

		status = STATUS_UNSUCCESSFUL;
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;		//servicePoint - : 0
		goto ErrorOut;
	}

	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	//servicePoint - : 0

	status = STATUS_SUCCESS;

ErrorOut:

	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    lpx_DereferenceConnection (connection);

	return STATUS_PENDING;
} /* lpxTdiAccept */


NTSTATUS
lpx_TdiAction(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    )
{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    PTDI_ACTION_HEADER ActionHeader;
    LARGE_INTEGER timeout = {0,0};
    KIRQL oldirql, cancelirql;
    ULONG BytesRequired;

    //
    // what type of status do we want?
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if ((!Irp->MdlAddress) || 
             (MmGetMdlByteCount(Irp->MdlAddress) < sizeof(TDI_ACTION_HEADER))) {
        return STATUS_INVALID_PARAMETER;
    }

    ActionHeader = (PTDI_ACTION_HEADER)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

    if (!ActionHeader) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	return STATUS_NOT_SUPPORTED;
}



NTSTATUS
lpx_TdiAssociateAddress(
    IN PIRP Irp
    )
{
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PTP_ADDRESS oldAddress, address;
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_ASSOCIATE parameters;
    PCONTROL_DEVICE_CONTEXT deviceContext;
    KIRQL			lpxOldIrql;
    KIRQL oldirql, oldirql2;
	PSERVICE_POINT	servicePoint;

    irpSp = IoGetCurrentIrpStackLocation (Irp);
DebugPrint(1,("lpx_TdiAssociateAddress \n"));
    //
    // verify that the operation is taking place on a connection. At the same
    // time we do this, we reference the connection. This ensures it does not
    // get removed out from under us. Note also that we do the connection
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
		DebugPrint(1,("Fail :: irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE \n"));
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;
    
    status = lpx_VerifyConnectionObject (connection);	//connection ref + : 1
    if (!NT_SUCCESS (status)) {

        return status;
    }

DebugPrint(1,("lpx_TdiAssociateAddress : lpx_VerifyConnectionObject \n"));
    //
    // Make sure this connection is ready to be associated.
    //

    oldAddress = (PTP_ADDRESS)NULL;

    try {

        ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql2);	//connection + : 1

        if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
DebugPrint(1,("lpx_TdiAssociateAddress step2 \n"));
            //
            // The connection is already associated with
            // an active connection...bad!
            //

            RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);	//connection - : 1
            lpx_DereferenceConnection (connection);				//connection ref - : 0

            return STATUS_INVALID_CONNECTION;

        } else {

            //
            // See if there is an old association hanging around...
            // this happens if the connection has been disassociated,
            // but not closed.
            //

            if (connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) {
DebugPrint(1,("lpx_TdiAssociateAddress step3 \n"));
                //
                // Save this; since it is non-null this address
                // will be dereferenced after the connection
                // spinlock is released.
                //

                oldAddress = connection->ServicePoint.Address;

                //
                // Remove the old association.
                //

                connection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
                RemoveEntryList (&connection->LinkList);
                InitializeListHead (&connection->LinkList);
                connection->ServicePoint.Address = NULL;

            }

        }

        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);	//connection - : 0

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DebugPrint(8, ("LPX: Got exception 1 in _lpx_TdiAssociateAddress\n"));
        DbgBreakPoint();
///??????????????
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);	// connection - : 0
        lpx_DereferenceConnection (connection);					// connection ref - : 0
        return GetExceptionCode();
    }


    //
    // If we removed an old association, dereference the
    // address.
    //

    if (oldAddress != (PTP_ADDRESS)NULL) {
DebugPrint(1,("lpx_TdiAssociateAddress step4 \n"));
        lpx_DereferenceAddress(oldAddress);	//connection ref - : 0

    }


    deviceContext = connection->Provider;

    parameters = (PTDI_REQUEST_KERNEL_ASSOCIATE)&irpSp->Parameters;

    //
    // get a pointer to the address File Object, which points us to the
    // transport's address object, which is where we want to put the
    // connection.
    //

    status = ObReferenceObjectByHandle (
                parameters->AddressHandle,
                0L,
                *IoFileObjectType,
                Irp->RequestorMode,
                (PVOID *) &fileObject,
                NULL);
DebugPrint(1,("lpx_TdiAssociateAddress step5 \n"));
    if (NT_SUCCESS(status)) {

        if (fileObject->DeviceObject == &deviceContext->DeviceObject) {

            //
            // we might have one of our address objects; verify that.
            //

            address = fileObject->FsContext;
DebugPrint(1,("lpx_TdiAssociateAddress step6 \n"));
            
            if ((fileObject->FsContext2 == (PVOID) TDI_TRANSPORT_ADDRESS_FILE) &&
                (NT_SUCCESS (lpx_VerifyAddressObject (address)))) {		//address ref + : 1

                //
                // have an address and connection object. Add the connection to the
                // address object database. Also add the connection to the address
                // file object db (used primarily for cleaning up). Reference the
                // address to account for one more reason for it staying open.
                //
DebugPrint(1,("lpx_TdiAssociateAddress step7 \n"));
                ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);		// address + ; 1
                if ((address->State & ADDRESS_STATE_CLOSING) == 0) {


                    try {

                        ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql2);	// connection + : 1

                        if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                            lpx_ReferenceAddress (address);		// address ref + : 2
DebugPrint(1,("lpx_TdiAssociateAddress step8 \n"));

                            InsertTailList (
                                &address->ConnectionDatabase,
                                &connection->Linkage);

                            connection->Address = address;
                            connection->Flags2 |= CONNECTION_FLAGS2_ASSOCIATED;
                            connection->Flags2 &= ~CONNECTION_FLAGS2_DISASSOCIATED;

                            status = STATUS_SUCCESS;

                        } else {

                            //
                            // The connection is closing, stop the
                            // association.
                            //

                            status = STATUS_INVALID_CONNECTION;

                        }
DebugPrint(1,("lpx_TdiAssociateAddress step8-2 \n"));
                        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);	// connection - : 0

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        DebugPrint(1, ("LPX: Got exception 2 in lpxTdiAssociateAddress\n"));
                        DbgBreakPoint();

                        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql2);	//connection - : 0

                        status = GetExceptionCode();
                    }

                } else {

                    status = STATUS_INVALID_HANDLE; //BUGBUG: should this be more informative?
                }
DebugPrint(1,("lpx_TdiAssociateAddress step8-3 \n"));
                RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);	// address - : 0
DebugPrint(1,("lpx_TdiAssociateAddress step9 \n"));
                lpx_DereferenceAddress (address);					// address 	ref - : 1

            } else {

                status = STATUS_INVALID_HANDLE;
            }
        } else {

            status = STATUS_INVALID_HANDLE;
        }

        //
        // Note that we don't keep a reference to this file object around.
        // That's because the IO subsystem manages the object for us; we simply
        // want to keep the association. We only use this association when the
        // IO subsystem has asked us to close one of the file object, and then
        // we simply remove the association.
        //

        ObDereferenceObject (fileObject);
  DebugPrint(1,("lpx_TdiAssociateAddress step10 \n"));          
    } else {
  DebugPrint(1,("lpx_TdiAssociateAddress step11 \n"));   
        status = STATUS_INVALID_HANDLE;
    }

	if(NT_SUCCESS(status))
	{
		servicePoint = &connection->ServicePoint;
		ExInterlockedInsertTailList(&address->ConnectionServicePointList,
									&servicePoint->ServicePointListEntry,
									&address->SpinLock
									);
		RtlCopyMemory(
					&servicePoint->SourceAddress,
					&address->NetworkName->LpxAddress,
					sizeof(LPX_ADDRESS)
					);

		servicePoint->Address = address;
	}

    lpx_DereferenceConnection (connection);		// connection ref -1 : 0
  DebugPrint(1,("lpx_TdiAssociateAddress step12 \n"));  
    return status;

} /* TdiAssociateAddress */





NTSTATUS
lpx_TdiDisassociateAddress(
    IN PIRP Irp
    )
{

    KIRQL oldirql;
    PIO_STACK_LOCATION irpSp;
    PTP_CONNECTION connection;
    NTSTATUS status;


 
	DebugPrint(DebugLevel,("lpx_TdiDisassociateAddress\n"));
    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference of type BY_ID.
    //

    status = lpx_VerifyConnectionObject (connection);	// connection ref + 1

    if (!NT_SUCCESS (status)) {
        return status;
    }


    

	LpxDisassociateAddress(
		connection
		);
		
    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldirql);			//connection + : 1
    if ((connection->Flags2 & CONNECTION_FLAGS2_STOPPING) == 0) {
        		//connection - : 0
        lpx_StopConnection (connection, STATUS_LOCAL_DISCONNECT);
    } 

    //
    // and now we disassociate the address. This only removes
    // the appropriate reference for the connection, the
    // actually disassociation will be done later.
    //
    // The DISASSOCIATED flag is used to make sure that
    // only one person removes this reference.
    //

    
    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);	// connection - : 0
    } else {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldirql);	// connection - : 0
    }

    

    lpx_DereferenceConnection (connection);		// connection ref - : 0

    return STATUS_SUCCESS;

} /* TdiDisassociateAddress */



NTSTATUS
lpx_TdiConnect(
    IN PIRP Irp
    )
{
    PCONTROL_DEVICE_CONTEXT	deviceContext;
    PTP_CONNECTION connection;
	PSERVICE_POINT	servicePoint;
	PLPX_ADDRESS	destinationAddress;
	PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
    TDI_ADDRESS_NETBIOS * RemoteAddress;
	NDIS_STATUS     status;
	KIRQL			cancelIrql;
	KIRQL			oldIrql, oldIrql1 ;

DebugPrint(1,("lpx_TdiConnect start\n"));
    //
    // is the file object a connection?
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference of type BY_ID.
    //

    status = lpx_VerifyConnectionObject (connection);	// connection ref +  : 1

    if (!NT_SUCCESS (status)) {
        return status;
    }
DebugPrint(1,("lpx_TdiConnect step1\n"));
    parameters = (PTDI_REQUEST_KERNEL)(&irpSp->Parameters);

    //
    // Check that the remote is a Netbios address.
    //

    if (!lpx_ValidateTdiAddress(
             parameters->RequestConnectionInformation->RemoteAddress,
             parameters->RequestConnectionInformation->RemoteAddressLength)) {

        lpx_DereferenceConnection (connection);		// connection ref - : 0
        return STATUS_BAD_NETWORK_PATH;
    }
DebugPrint(1,("lpx_TdiConnect step2\n"));
    RemoteAddress = lpx_ParseTdiAddress((PTRANSPORT_ADDRESS)(parameters->RequestConnectionInformation->RemoteAddress), FALSE);
DebugPrint(1,("lpx_TdiConnect step3\n"));
    if (RemoteAddress == NULL) {

        lpx_DereferenceConnection (connection);		// connection ref - : 0
        return STATUS_BAD_NETWORK_PATH;

    }
DebugPrint(1,("lpx_TdiConnect step4\n"));


	servicePoint = (PSERVICE_POINT)connection;	
	
	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql); // servicePoint + : 1

	if(servicePoint->SmpState != SMP_CLOSE) {
		DebugPrint(1, ("servicePoint->SmpState != SMP_CLOSE\n"));
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0
		
		lpx_DereferenceConnection (connection);				// connection ref - : 0
		return STATUS_UNSUCCESSFUL;
	}

    //
    // copy the called address someplace we can use it.
    //
    connection->CalledAddress.NetbiosNameType =
        RemoteAddress->NetbiosNameType;

    RtlCopyMemory(
        connection->CalledAddress.NetbiosName,
        RemoteAddress->NetbiosName,
        16);
	
	destinationAddress = &connection->CalledAddress.LpxAddress;

	RtlCopyMemory(
		&servicePoint->DestinationAddress,
		destinationAddress,
		sizeof(LPX_ADDRESS)
		);
		
			DebugPrint(2,("servicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
					servicePoint->DestinationAddress.Node[0],
					servicePoint->DestinationAddress.Node[1],
					servicePoint->DestinationAddress.Node[2],
					servicePoint->DestinationAddress.Node[3],
					servicePoint->DestinationAddress.Node[4],
					servicePoint->DestinationAddress.Node[5],
					servicePoint->DestinationAddress.Port));

	IoMarkIrpPending(Irp);
	servicePoint->ConnectIrp = Irp;

	servicePoint->SmpState = SMP_SYN_SENT;

	status = TransmitPacket(servicePoint, NULL, CONREQ, 0);

	if(!NT_SUCCESS(status)) {
		DebugPrint(1, ("[LPX] LPX_CONNECT ERROR\n"));
		servicePoint->ConnectIrp = NULL;
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0
		lpx_DereferenceConnection(connection) ;				// connection ref - : 0
		return STATUS_UNSUCCESSFUL;
	}

	//
	//	set connection time-out
	//

	ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;	// smpContext.timecounter + : 1
	servicePoint->SmpContext.ConnectTimeOut.QuadPart = CurrentTime().QuadPart + MAX_CONNECT_TIME;
	servicePoint->SmpContext.SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
	RELEASE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;	// smpContext.timecounter - : 0

	KeSetTimer(
			&servicePoint->SmpContext.SmpTimer,
			servicePoint->SmpContext.SmpTimerExpire,
			&servicePoint->SmpContext.SmpTimerDpc
			);

	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;		// servicePoint - : 0
	
	
	//
	//	dereference the connection
	//
	lpx_DereferenceConnection(connection) ;					// connection ref - : 0

	IoAcquireCancelSpinLock(&cancelIrql);
    IoSetCancelRoutine(Irp, LpxCancelConnect);
	IoReleaseCancelSpinLock(cancelIrql);
	return STATUS_PENDING; 
}


NTSTATUS
lpx_TdiDisconnect(
    IN PIRP Irp
    )
{
    PTP_CONNECTION connection;
    PCONTROL_DEVICE_CONTEXT deviceContext;
    LARGE_INTEGER timeout;
	PSERVICE_POINT	servicePoint;
	PSMP_CONTEXT	smpContext;
	PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
	NDIS_STATUS     status;
	KIRQL			cancelIrql;
	KIRQL			oldIrql ;

 
    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference of type BY_ID.
    //

    status = lpx_VerifyConnectionObject (connection);	//connection ref  + : 1
    if (!NT_SUCCESS (status)) {
        return status;
    }


	servicePoint = (PSERVICE_POINT)connection;
	smpContext = &servicePoint->SmpContext;

	DebugPrint(2, ("LpxDisconnect servicePoint = %p, servicePoint->SmpState = 0x%x\n", 
		servicePoint, servicePoint->SmpState));


	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;		// servicePoint + : 1

	switch(servicePoint->SmpState) {
	
	case SMP_CLOSE:
	case SMP_SYN_SENT:
		
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0

		SmpFreeServicePoint(servicePoint);
		lpx_DereferenceConnection (connection);       // connection ref - : 0
		return STATUS_SUCCESS;
		break;

	case SMP_SYN_RECV:
	case SMP_ESTABLISHED:
	case SMP_CLOSE_WAIT:
	{
		IoMarkIrpPending(Irp);
		IoAcquireCancelSpinLock(&cancelIrql);
	    IoSetCancelRoutine(Irp, LpxCancelDisconnect);
		IoReleaseCancelSpinLock(cancelIrql);

		servicePoint->DisconnectIrp = Irp;

		if(servicePoint->SmpState == SMP_CLOSE_WAIT)
			servicePoint->SmpState = SMP_LAST_ACK;
		else
			servicePoint->SmpState = SMP_FIN_WAIT1;

		smpContext->FinSequence = SHORT_SEQNUM(smpContext->Sequence) ;

		TransmitPacket(servicePoint, NULL, DISCON, 0);

		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0
		lpx_DereferenceConnection (connection);       // connection ref - : 0
		break;
	 }

	 default:

		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0
		lpx_DereferenceConnection (connection);       // connection ref - : 0
		return STATUS_SUCCESS;
		break;
	}

	return STATUS_PENDING;
}




NTSTATUS
lpx_TdiListen(
    IN PIRP Irp
    )
{
    PTP_CONNECTION connection;
	PSERVICE_POINT				servicePoint;
	PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_LISTEN parameters;
    PTDI_CONNECTION_INFORMATION ListenInformation;
	NDIS_STATUS					status;
	KIRQL						cancelIrql;
	KIRQL						oldIrql ;
	LARGE_INTEGER timeout = {0,0};
    TDI_ADDRESS_NETBIOS * ListenAddress;
    PVOID RequestBuffer2;
    ULONG RequestBuffer2Length;
   

 
    //
    // validate this connection

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;
	
    //
    // If successful this adds a reference of type BY_ID.
    //

    status = lpx_VerifyConnectionObject (connection);	// connection ref + : 1

    if (!NT_SUCCESS (status)) {
        return status;
    }


    parameters = (PTDI_REQUEST_KERNEL_LISTEN)&irpSp->Parameters;

    //
    // Record the remote address if there is one.
    //

    ListenInformation = parameters->RequestConnectionInformation;

    if ((ListenInformation != NULL) &&
        (ListenInformation->RemoteAddress != NULL)) {

        if ((lpx_ValidateTdiAddress(
             ListenInformation->RemoteAddress,
             ListenInformation->RemoteAddressLength)) &&
            ((ListenAddress = lpx_ParseTdiAddress(ListenInformation->RemoteAddress, FALSE)) != NULL)) {

            RequestBuffer2 = (PVOID)ListenAddress->NetbiosName,
            RequestBuffer2Length = NETBIOS_NAME_LENGTH;

        } else {

            lpx_DereferenceConnection (connection);		// connection ref - : 0
            return STATUS_BAD_NETWORK_PATH;
        }

    } else {

        RequestBuffer2 = NULL;
        RequestBuffer2Length = 0;
    }

	servicePoint = (PSERVICE_POINT)connection;


	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;		// servicePoint + : 1

	DebugPrint(2, ("LpxListen servicePoint = %p, servicePoint->SmpState = 0x%x\n", 
		servicePoint, servicePoint->SmpState));

	if(servicePoint->SmpState != SMP_CLOSE) {

		status = STATUS_UNSUCCESSFUL;

		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// connection - : 0
		lpx_DereferenceConnection (connection);				// connection ref - : 0
		return status;
	}

	RtlCopyMemory(
				&servicePoint->SourceAddress,
				&servicePoint->Address->NetworkName->LpxAddress,
				sizeof(LPX_ADDRESS)
				);

	servicePoint->SmpState = SMP_LISTEN;

	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;		// connection - : 0
	
	servicePoint->ListenIrp = Irp;
	DebugPrint(DebugLevel,("ListenIrp: %0x\n", servicePoint->ListenIrp));
	DebugPrint(DebugLevel,("Irq : %0x SevicePoint : %0x connection %0x\n", Irp, servicePoint, connection));
	Irp->IoStatus.Information = 2;
	IoMarkIrpPending(Irp);

	IoAcquireCancelSpinLock(&cancelIrql);
    IoSetCancelRoutine(Irp, LpxCancelListen);
	IoReleaseCancelSpinLock(cancelIrql);
    lpx_DereferenceConnection (connection);					// connection ref - : 0
	return STATUS_PENDING; 

}




NTSTATUS
lpx_TdiQueryInformation(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    )
{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    PVOID adapterStatus;
    PTDI_REQUEST_KERNEL_QUERY_INFORMATION query;
    PTA_NETBIOS_ADDRESS broadcastAddress;
    PTDI_PROVIDER_STATISTICS ProviderStatistics;
    PTDI_CONNECTION_INFO ConnectionInfo;
    ULONG TargetBufferLength;
    PFIND_NAME_HEADER FindNameHeader;
    LARGE_INTEGER timeout = {0,0};
    PTP_CONNECTION Connection;
    PTP_ADDRESS Address;
    ULONG NamesWritten, TotalNameCount, BytesWritten;
    BOOLEAN Truncated;
    BOOLEAN RemoteAdapterStatus;
    TDI_ADDRESS_NETBIOS * RemoteAddress;
    struct {
        ULONG ActivityCount;
        TA_NETBIOS_ADDRESS TaAddressBuffer;
    } AddressInfo;
    PTRANSPORT_ADDRESS TaAddress;
    TDI_DATAGRAM_INFO DatagramInfo;
    BOOLEAN UsedConnection;
    PLIST_ENTRY p;
    KIRQL oldirql;
    ULONG BytesCopied;

    //
    // what type of status do we want?
    //
DebugPrint(1,("lpx_TdiQueryInformation \n"));
    irpSp = IoGetCurrentIrpStackLocation (Irp);

    query = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&irpSp->Parameters;

    switch (query->QueryType) {

    case TDI_QUERY_ADDRESS_INFO:
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO \n "));
        if (irpSp->FileObject->FsContext2 == (PVOID)TDI_TRANSPORT_ADDRESS_FILE) {

            Address = irpSp->FileObject->FsContext;

            status = lpx_VerifyAddressObject(Address);			// address ref + : 1
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO 1 \n "));
            if (!NT_SUCCESS (status)) {

                DebugPrint(8, ("TdiQueryInfo: Invalid Address %lx Irp %lx\n", Address, Irp));
                return status;
            }

            UsedConnection = FALSE;

        } else if (irpSp->FileObject->FsContext2 == (PVOID)TDI_CONNECTION_FILE) {

            Connection = irpSp->FileObject->FsContext;

            status = lpx_VerifyConnectionObject (Connection);	// connection ref + : 1
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO 2 \n "));
            if (!NT_SUCCESS (status)) {

                DebugPrint(8, ("TdiQueryInfo: Invalid Connection %lx Irp %lx\n", Connection, Irp));

                return status;
            }

            Address = Connection->ServicePoint.Address;

            UsedConnection = TRUE;

        } else {

            return STATUS_INVALID_ADDRESS;

        }


        TdiBuildNetbiosAddress(
            Address->NetworkName->NetbiosName,
            FALSE,
            &AddressInfo.TaAddressBuffer);
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO 3 \n "));
        //
        // Count the active addresses.
        //

        AddressInfo.ActivityCount = 1;

        status = TdiCopyBufferToMdl (
                    &AddressInfo,
                    0,
                    sizeof(ULONG) + sizeof(TA_NETBIOS_ADDRESS),
                    Irp->MdlAddress,
                    0,                    
                    &BytesCopied);

        Irp->IoStatus.Information = BytesCopied;
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO 4 \n "));
        if (UsedConnection) {

            lpx_DereferenceConnection (Connection);		// connection ref - : 0
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO 5 \n "));
        } else {

            lpx_DereferenceAddress (Address);			// address ref - : 0	
DebugPrint(1,("TDI_QUERY_ADDRESS_INFO 6 \n "));
        }

        break;

    case TDI_QUERY_BROADCAST_ADDRESS:
DebugPrint(1,("TDI_QUERY_BROADCAST_ADDRESS \n "));
        //
        // for this provider, the broadcast address is a zero byte name,
        // contained in a Transport address structure.
        //

        broadcastAddress = ExAllocatePoolWithTag (
                                NonPagedPool,
                                sizeof (TA_NETBIOS_ADDRESS),
                                LPX_MEM_TAG_TDI_QUERY_BUFFER);
        if (broadcastAddress == NULL) {

            PANIC ("LpxQueryInfo: Cannot allocate broadcast address!\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
        } else {

            broadcastAddress->TAAddressCount = 1;
            broadcastAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
            broadcastAddress->Address[0].AddressLength = 0;

            Irp->IoStatus.Information =
                    sizeof (broadcastAddress->TAAddressCount) +
                    sizeof (broadcastAddress->Address[0].AddressType) +
                    sizeof (broadcastAddress->Address[0].AddressLength);

            BytesCopied = (ULONG)Irp->IoStatus.Information;

            status = TdiCopyBufferToMdl (
                            (PVOID)broadcastAddress,
                            0L,
                            BytesCopied,
                            Irp->MdlAddress,
                            0,
                            &BytesCopied);
                            
            Irp->IoStatus.Information = BytesCopied;

            ExFreePool (broadcastAddress);
        }

        break; 

    case TDI_QUERY_SESSION_STATUS:
DebugPrint(1,("TDI_QUERY_SESSION_STATUS \n "));
        status = STATUS_NOT_IMPLEMENTED;
        break;


    case TDI_QUERY_PROVIDER_INFO:
DebugPrint(1,("TDI_QUERY_SESSION_STATUS \n "));
        status = TdiCopyBufferToMdl (
                    &(DeviceContext->Information),
                    0,
                    sizeof (TDI_PROVIDER_INFO),
                    Irp->MdlAddress,
                    0,
                    &BytesCopied);

        Irp->IoStatus.Information = BytesCopied;

        break;

    case TDI_QUERY_DATA_LINK_ADDRESS:
    case TDI_QUERY_NETWORK_ADDRESS:
DebugPrint(1,("TDI_QUERY_DATA_LINK_ADDRESS \n "));
        TaAddress = (PTRANSPORT_ADDRESS)&AddressInfo.TaAddressBuffer;
        TaAddress->TAAddressCount = 1;
        TaAddress->Address[0].AddressLength = 6;
        TaAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_UNSPEC;
  
        RtlCopyMemory (TaAddress->Address[0].Address, global.LpxPrimaryDeviceContext->LocalAddress.Address, 6);

        status = TdiCopyBufferToMdl (
                    &AddressInfo.TaAddressBuffer,
                    0,
                    sizeof(TRANSPORT_ADDRESS)+5,
                    Irp->MdlAddress,
                    0,
                    &BytesCopied);
                        
        Irp->IoStatus.Information = BytesCopied;
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return status;

} /* TdiQueryInformation */


NTSTATUS
lpx_TdiReceive(
    IN PIRP Irp
    )
{
    PTP_CONNECTION connection;
	PSERVICE_POINT	servicePoint;
	NDIS_STATUS		status;
	PIO_STACK_LOCATION irpSp;
	ULONG				userDataLength;
	ULONG				irpCopied;
	PUCHAR				userData;
	KIRQL			cancelIrql;
    KIRQL oldIrql;


    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection = irpSp->FileObject->FsContext;


    //
    // Check that this is really a connection.
    //

    if ((irpSp->FileObject->FsContext2 == (PVOID)LPX_FILE_TYPE_CONTROL) ||
        (connection->Size != sizeof (TP_CONNECTION)) ||
        (connection->Type != LPX_CONNECTION_SIGNATURE)) {

        return STATUS_INVALID_CONNECTION;
    }

	servicePoint = (PSERVICE_POINT)connection;

	//
	//	acquire Connection's SpinLock
	//
	//	added by hootch 09042003
	//	
    ACQUIRE_SPIN_LOCK (&servicePoint->SpinLock, &oldIrql);		// servicePoint + : 1

	if(servicePoint->SmpState != SMP_ESTABLISHED
		&& servicePoint->SmpState != SMP_CLOSE_WAIT) 
	{
		RELEASE_SPIN_LOCK (&servicePoint->SpinLock, oldIrql);	// servicePoint - : 0

		DebugPrint(0, ("LPX_RECEIVE OUT\n"));

		return STATUS_UNSUCCESSFUL;
	}

	if(servicePoint->Shutdown & SMP_RECEIVE_SHUTDOWN)
	{
		RELEASE_SPIN_LOCK (&servicePoint->SpinLock, oldIrql);	// servicePoint - : 0

		DebugPrint(0, ("LPX_RECEIVE OUT\n"));

		return STATUS_UNSUCCESSFUL;
	}

	//
	//	release Connection's SpinLock
	//
    RELEASE_SPIN_LOCK (&servicePoint->SpinLock, oldIrql);	// servicePoint - : 0


	irpSp = IoGetCurrentIrpStackLocation(Irp);
	userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 
	irpCopied = Irp->IoStatus.Information;
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);

	if((userData == NULL) || (userDataLength == 0))
	{
		status = STATUS_CANCELLED;
		IoAcquireCancelSpinLock(&cancelIrql);	\
		IoSetCancelRoutine(Irp, NULL);	\
		IoReleaseCancelSpinLock(cancelIrql);	\
		IoCompleteRequest (Irp, IO_NETWORK_INCREMENT); \
		return STATUS_PENDING;
	}



	IoMarkIrpPending(Irp);
	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, LpxCancelRecv);
	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Information = 0;

	LpxCompleteIRPRequest(servicePoint, Irp) ;

	return STATUS_PENDING; 
}

 




NTSTATUS
lpx_TdiReceiveDatagram(
    IN PIRP Irp
    )
{
    NTSTATUS status;
    KIRQL oldirql;
    PTP_ADDRESS address;
    PIO_STACK_LOCATION irpSp;
    KIRQL cancelIrql;

    //
    // verify that the operation is taking place on an address. At the same
    // time we do this, we reference the address. This ensures it does not
    // get removed out from under us. Note also that we do the address
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_TRANSPORT_ADDRESS_FILE) {
        return STATUS_INVALID_ADDRESS;
    }

    address = irpSp->FileObject->FsContext;

    status = lpx_VerifyAddressObject (address);		// address ref + : 1

    if (!NT_SUCCESS (status)) {
        return status;
    }

	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

    lpx_DereferenceAddress (address);				// address ref - : 0

    return STATUS_PENDING;

} /* TdiReceiveDatagram */



NTSTATUS
lpx_TdiSend(
    IN PIRP Irp
    )
{
	PSERVICE_POINT				servicePoint;
    PTP_CONNECTION				connection;
	PDEVICE_CONTEXT				deviceContext;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_SEND parameters;
	NDIS_STATUS					status;
    UINT						maxUserData;
	UINT						mss;
	PUCHAR						userData;
	ULONG						userDataLength;
	PNDIS_PACKET				packet;
    KIRQL						cancelIrql;
	KIRQL						oldIrql ;


    //
    // Determine which connection this send belongs on.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection  = irpSp->FileObject->FsContext;

    //
    // Check that this is really a connection.
    //

    if ((irpSp->FileObject->FsContext2 == (PVOID)LPX_FILE_TYPE_CONTROL) ||
        (connection->Size != sizeof (TP_CONNECTION)) ||
        (connection->Type != LPX_CONNECTION_SIGNATURE)) {
        return STATUS_INVALID_CONNECTION;
    }


	servicePoint = (PSERVICE_POINT)connection;
    irpSp = IoGetCurrentIrpStackLocation (Irp);
    parameters = (PTDI_REQUEST_KERNEL_SEND)(&irpSp->Parameters);
	deviceContext = (PDEVICE_CONTEXT)servicePoint->Address->PacketProvider;
	
    IRP_SEND_IRP(irpSp) = Irp;
    IRP_SEND_REFCOUNT(irpSp) = 1;

    Irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
    Irp->IoStatus.Information = 0;

	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;	// servicePoint + : 1

	if(servicePoint->SmpState != SMP_ESTABLISHED) {
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0

		DebugPrint(1, ("[LPX] LpxSend: SmpState %x is not SMP_ESTABLISHED.\n", servicePoint->SmpState));

        LpxDereferenceSendIrp (irpSp);     // remove creation reference.

		return STATUS_PENDING; 
	}
	
	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0

    lpx_ReferenceConnection (connection);	// connection ref + : 1

	

	IoMarkIrpPending(Irp);

	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, LpxCancelSend);
	IoReleaseCancelSpinLock(cancelIrql);

    MacReturnMaxDataSize(
        &deviceContext->MacInfo,
        NULL,
        0,
        deviceContext->MaxSendPacketSize,
        TRUE,
        &maxUserData);

	mss = maxUserData - sizeof(LPX_HEADER2);

	userDataLength = parameters->SendLength;
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);


	if((userData == NULL) || (userDataLength == 0))
	{
			DebugPrint(0, ("[LPX]LpxSend: userData == NULL || userDataLength == 0 \n"));
            LpxCompleteSendIrp (Irp, STATUS_CANCELLED, 0);	//	connection ref - : 0 
			return STATUS_PENDING; 	
	}

	while (userDataLength) 
	{
		USHORT			copy;

		copy = (USHORT)mss;
		if(copy > userDataLength)
			copy = (USHORT)userDataLength;

		status = PacketAllocate(
			servicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			deviceContext,
			SEND_TYPE,
			userData,
			copy,
			irpSp,
			&packet
			);
		
		if(!NT_SUCCESS(status)) {
			DebugPrint(0, ("[LPX]LpxSend: packet == NULL\n"));
			SmpPrintState(0, "[LPX]LpxSend: PacketAlloc", servicePoint);
            LpxCompleteSendIrp (Irp, STATUS_CANCELLED, 0);	//	connection ref - : 0 
			return STATUS_PENDING; 
		}

        LpxReferenceSendIrp (irpSp);

		DebugPrint(4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy));
		userDataLength -= copy;

		status = TransmitPacket(servicePoint, packet, DATA, copy);

		userData += copy;
	}
		
    LpxCompleteSendIrp (Irp, STATUS_SUCCESS, parameters->SendLength);	// connection ref - : 0
	return STATUS_PENDING; 
}
 



NTSTATUS
lpx_TdiSendDatagram(
    IN PIRP Irp
    )
{
    PTP_ADDRESS address;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_SENDDG parameters;
    PDEVICE_CONTEXT				deviceContext;
	NDIS_STATUS					status;
    UINT						maxUserData;
	UINT						mss;
	PUCHAR						userData;
	ULONG						userDataLength;
	PNDIS_PACKET				packet;
    TRANSPORT_ADDRESS UNALIGNED *transportAddress;
	PLPX_ADDRESS				remoteAddress;
    UINT MaxUserData;
    KIRQL oldirql;
    KIRQL			lpxOldIrql;

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_TRANSPORT_ADDRESS_FILE) {
        return STATUS_INVALID_ADDRESS;
    }

    address  = irpSp->FileObject->FsContext;

    status = lpx_VerifyAddressObject (address);		// address ref + : 1

    if (!NT_SUCCESS (status)) {
        return status;
    }

    parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);
	deviceContext = (PDEVICE_CONTEXT)address->Provider;

    //
    // Check that the length is short enough.
    //

    MacReturnMaxDataSize(
        &address->PacketProvider->MacInfo,
        NULL,
        0,
        address->PacketProvider->MaxSendPacketSize,
        FALSE,
        &MaxUserData);

    if (parameters->SendLength >
        (MaxUserData - sizeof(LPX_HEADER2))) {

        lpx_DereferenceAddress(address);			// address ref - : 0
        return STATUS_INVALID_PARAMETER;

    }

   
    //
    // Check that the target address includes a Netbios component.
    //

    if (!(lpx_ValidateTdiAddress(
             parameters->SendDatagramInformation->RemoteAddress,
             parameters->SendDatagramInformation->RemoteAddressLength)) ||
        (lpx_ParseTdiAddress(parameters->SendDatagramInformation->RemoteAddress, TRUE) == NULL)) {

        lpx_DereferenceAddress(address);			// addres ref - : 0
        return STATUS_BAD_NETWORK_PATH;
    }


    IRP_SEND_IRP(irpSp) = Irp;
    IRP_SEND_REFCOUNT(irpSp) = 1;

    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
    Irp->IoStatus.Information = 0;


	IoMarkIrpPending(Irp);


    MacReturnMaxDataSize(
        &deviceContext->MacInfo,
        NULL,
        0,
        deviceContext->MaxSendPacketSize,
        TRUE,
        &maxUserData);

	mss = maxUserData - sizeof(LPX_HEADER2);

	userDataLength = parameters->SendLength;
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);


	if((userData == NULL) || (userDataLength == 0))
	{
		Irp->IoStatus.Status = STATUS_CANCELLED;
	    LpxDereferenceSendIrp (irpSp);
		lpx_DereferenceAddress(address);		// address ref - : 0
		return STATUS_PENDING; 
	}

	transportAddress = (TRANSPORT_ADDRESS UNALIGNED *)parameters->SendDatagramInformation->RemoteAddress;
	remoteAddress = (PLPX_ADDRESS)&transportAddress->Address[0].Address[0];

	

	while (userDataLength) 
	{
		USHORT			copy;
		PNDIS_BUFFER	firstBuffer;	
		PUCHAR			packetData;
		USHORT			type;
		PLPX_HEADER2	lpxHeader;

		copy = (USHORT)mss;
		if(copy > userDataLength)
			copy = (USHORT)userDataLength;

		status = PacketAllocate(
			NULL,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			deviceContext,
			SEND_TYPE,
			userData,
			copy,
			irpSp,
			&packet
			);

		if(!NT_SUCCESS(status)) {
			DebugPrint(0, ("packet == NULL\n"));
	        LpxDereferenceSendIrp (irpSp);
			lpx_DereferenceAddress(address);		// address ref - : 0
			return STATUS_PENDING; 
		}

        LpxReferenceSendIrp (irpSp);

		DebugPrint(4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy));
		userDataLength -= copy;


			NdisQueryPacket(
				packet,
				NULL,
				NULL,
				&firstBuffer,
				NULL
			);
			
			packetData = MmGetMdlVirtualAddress(firstBuffer);
			RtlCopyMemory(
				&packetData[0],
				remoteAddress->Node,
				ETHERNET_ADDRESS_LENGTH
				);

			DebugPrint(2,("remoteAddress %02X%02X%02X%02X%02X%02X:%04X\n",
					remoteAddress->Node[0],
					remoteAddress->Node[1],
					remoteAddress->Node[2],
					remoteAddress->Node[3],
					remoteAddress->Node[4],
					remoteAddress->Node[5],
					remoteAddress->Port));

			RtlCopyMemory(
				&packetData[ETHERNET_ADDRESS_LENGTH],
				address->NetworkName->LpxAddress.Node,
				ETHERNET_ADDRESS_LENGTH
				);

			type = HTONS(ETH_P_LPX);
			RtlCopyMemory(
				&packetData[ETHERNET_ADDRESS_LENGTH*2],
				&type, //&ServicePoint->DestinationAddress.Port,
				2
				);
			
			lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

			lpxHeader->PacketSize = HTONS(sizeof(LPX_HEADER2) + copy);
			lpxHeader->LpxType = LPX_TYPE_RAW;
			lpxHeader->DestinationPort = remoteAddress->Port;
			lpxHeader->SourcePort = address->NetworkName->LpxAddress.Port;
		
			NdisSend(
					&status,
					deviceContext->NdisBindingHandle,
					packet
					);
			if(status != NDIS_STATUS_PENDING) {
				PacketFree(packet);
				status = STATUS_SUCCESS;
			} else
				continue;
		
		userData += copy;
	}
		
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = parameters->SendLength;
    LpxDereferenceSendIrp (irpSp);
    lpx_DereferenceAddress(address);	// address ref - : 0
	return STATUS_PENDING; 

}


NTSTATUS
lpx_TdiSetEventHandler(
    IN PIRP Irp
    )
{
    NTSTATUS rc=STATUS_SUCCESS;
    KIRQL oldirql;
    PTDI_REQUEST_KERNEL_SET_EVENT parameters;
    PIO_STACK_LOCATION irpSp;
    PTP_ADDRESS address;
    NTSTATUS status;

    //
    // Get the Address this is associated with; if there is none, get out.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_TRANSPORT_ADDRESS_FILE) {
        return STATUS_INVALID_ADDRESS;
    }

    address  = irpSp->FileObject->FsContext;
    status = lpx_VerifyAddressObject (address);		// address ref + 1
    if (!NT_SUCCESS (status)) {
        return status;
    }


    ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);	// address + : 1

    parameters = (PTDI_REQUEST_KERNEL_SET_EVENT)&irpSp->Parameters;

    switch (parameters->EventType) {

    case TDI_EVENT_RECEIVE:

        if (parameters->EventHandler == NULL) {
            address->ReceiveHandler =
                (PTDI_IND_RECEIVE)TdiDefaultReceiveHandler;
            address->ReceiveHandlerContext = NULL;
            address->RegisteredReceiveHandler = FALSE;
        } else {
            address->ReceiveHandler =
                (PTDI_IND_RECEIVE)parameters->EventHandler;
            address->ReceiveHandlerContext = parameters->EventContext;
            address->RegisteredReceiveHandler = TRUE;
        }

        break;

    case TDI_EVENT_RECEIVE_EXPEDITED:

        if (parameters->EventHandler == NULL) {
            address->ExpeditedDataHandler =
                (PTDI_IND_RECEIVE_EXPEDITED)TdiDefaultRcvExpeditedHandler;
            address->ExpeditedDataHandlerContext = NULL;
            address->RegisteredExpeditedDataHandler = FALSE;
        } else {
            address->ExpeditedDataHandler =
                (PTDI_IND_RECEIVE_EXPEDITED)parameters->EventHandler;
            address->ExpeditedDataHandlerContext = parameters->EventContext;
            address->RegisteredExpeditedDataHandler = TRUE;
        }

        break;

    case TDI_EVENT_RECEIVE_DATAGRAM:

        if (parameters->EventHandler == NULL) {
            address->ReceiveDatagramHandler =
                (PTDI_IND_RECEIVE_DATAGRAM)TdiDefaultRcvDatagramHandler;
            address->ReceiveDatagramHandlerContext = NULL;
            address->RegisteredReceiveDatagramHandler = FALSE;
        } else {
            address->ReceiveDatagramHandler =
                (PTDI_IND_RECEIVE_DATAGRAM)parameters->EventHandler;
            address->ReceiveDatagramHandlerContext = parameters->EventContext;
            address->RegisteredReceiveDatagramHandler = TRUE;
        }

        break;

    case TDI_EVENT_ERROR:

        if (parameters->EventHandler == NULL) {
            address->ErrorHandler =
                (PTDI_IND_ERROR)TdiDefaultErrorHandler;
            address->ErrorHandlerContext = NULL;
            address->RegisteredErrorHandler = FALSE;
        } else {
            address->ErrorHandler =
                (PTDI_IND_ERROR)parameters->EventHandler;
            address->ErrorHandlerContext = parameters->EventContext;
            address->RegisteredErrorHandler = TRUE;
        }

        break;

    case TDI_EVENT_DISCONNECT:

        if (parameters->EventHandler == NULL) {
            address->DisconnectHandler =
                (PTDI_IND_DISCONNECT)TdiDefaultDisconnectHandler;
            address->DisconnectHandlerContext = NULL;
            address->RegisteredDisconnectHandler = FALSE;
        } else {
            address->DisconnectHandler =
                (PTDI_IND_DISCONNECT)parameters->EventHandler;
            address->DisconnectHandlerContext = parameters->EventContext;
            address->RegisteredDisconnectHandler = TRUE;
        }

        break;

    case TDI_EVENT_CONNECT:

        if (parameters->EventHandler == NULL) {
            address->ConnectionHandler =
                (PTDI_IND_CONNECT)TdiDefaultConnectHandler;
            address->ConnectionHandlerContext = NULL;
            address->RegisteredConnectionHandler = FALSE;
        } else {
            address->ConnectionHandler =
                (PTDI_IND_CONNECT)parameters->EventHandler;
            address->ConnectionHandlerContext = parameters->EventContext;
            address->RegisteredConnectionHandler = TRUE;
        }
            break;

    default:

        rc = STATUS_INVALID_PARAMETER;

    } /* switch */

    RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);	// address - : 0

    lpx_DereferenceAddress (address);	//	address ref - : 0

    return rc;
} /* TdiSetEventHandler */



NTSTATUS
lpx_TdiSetInformation(
    IN PIRP Irp
    )
{
    UNREFERENCED_PARAMETER (Irp);    // prevent compiler warnings

    return STATUS_NOT_IMPLEMENTED;

} /* TdiQueryInformation */

NTSTATUS
lpx_DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    BOOL InternalIrp = FALSE;
    NTSTATUS Status;
    PCONTROL_DEVICE_CONTEXT DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {


		case IOCTL_TCP_QUERY_INFORMATION_EX:
		{
		    PVOID					outputBuffer;
			ULONG					outputBufferLength;
			SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;

			PLIST_ENTRY		listHead;
			PLIST_ENTRY		thisEntry;

			outputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
	        outputBuffer = Irp->UserBuffer;

            DebugPrint(2, ("LPX: IOCTL_TCP_QUERY_INFORMATION_EX, outputBufferLength= %d, outputBuffer = %p\n", 
				outputBufferLength, outputBuffer));

			if(outputBufferLength < sizeof(SOCKETLPX_ADDRESS_LIST)) {
				Status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				break;
			}

			ACQUIRE_DEVICES_LIST_LOCK();	// Global Ndis Device + : 1

			RtlZeroMemory(
				&socketLpxAddressList,
				sizeof(SOCKETLPX_ADDRESS_LIST)
				);

			socketLpxAddressList.iAddressCount = 0;

			if(IsListEmpty (&global.NIC_DevList)) {
				RELEASE_DEVICES_LIST_LOCK();	//	Global Ndis Device - : 0

				RtlCopyMemory(
					outputBuffer,
					&socketLpxAddressList,
					sizeof(SOCKETLPX_ADDRESS_LIST)
					);

				Irp->IoStatus.Information = sizeof(SOCKETLPX_ADDRESS_LIST);
				Status = STATUS_SUCCESS;
				break;
			}

			listHead = &global.NIC_DevList;
			for(thisEntry = listHead->Flink;
				thisEntry != listHead;
				thisEntry = thisEntry->Flink)
			{
				PDEVICE_CONTEXT deviceContext;
    
				deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
				if(deviceContext->CreateRefRemoved == FALSE)
				{
					socketLpxAddressList.SocketLpx[socketLpxAddressList.iAddressCount].sin_family 
						= TDI_ADDRESS_TYPE_LPX;
					socketLpxAddressList.SocketLpx[socketLpxAddressList.iAddressCount].LpxAddress.Port = 0;
					RtlCopyMemory(
						&socketLpxAddressList.SocketLpx[socketLpxAddressList.iAddressCount].LpxAddress.Node,
						deviceContext->LocalAddress.Address,
						HARDWARE_ADDRESS_LENGTH
						);
						
					socketLpxAddressList.iAddressCount++;
					ASSERT(socketLpxAddressList.iAddressCount <= MAX_SOCKETLPX_INTERFACE);
					if(socketLpxAddressList.iAddressCount == MAX_SOCKETLPX_INTERFACE)
						break;
				}
			}

			RELEASE_DEVICES_LIST_LOCK();	//	Global Ndis Device - : 0
	
			RtlCopyMemory(
				outputBuffer,
				&socketLpxAddressList,
				sizeof(SOCKETLPX_ADDRESS_LIST)
				);

			Irp->IoStatus.Information = sizeof(SOCKETLPX_ADDRESS_LIST);
			Status = STATUS_SUCCESS;

			break;
		}
		


        default:
 
            //
            // Convert the user call to the proper internal device call.
            //

            Status = TdiMapUserRequest (DeviceObject, Irp, IrpSp);

            if (Status == STATUS_SUCCESS) {

                //
                // If TdiMapUserRequest returns SUCCESS then the IRP
                // has been converted into an IRP_MJ_INTERNAL_DEVICE_CONTROL
                // IRP, so we dispatch it as usual. The IRP will be
                // completed by this call to NbfDispatchInternal, so we dont
                //

                InternalIrp = TRUE;

                Status = LpxInternalDevControl(DeviceObject, Irp);
            }
    }

    //
    // If this IRP got converted to an internal IRP,
    // it will be completed by NbfDispatchInternal.
    //

    if ((!InternalIrp) && (Status != STATUS_PENDING))
    {
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
    }

    return Status;
} /* DeviceControl */


NTSTATUS
lpx_DispatchPnPPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PDEVICE_RELATIONS DeviceRelations = NULL;
    PTP_CONNECTION Connection;
    PVOID PnPContext;
    NTSTATUS Status;

 

    Status = STATUS_INVALID_DEVICE_REQUEST;

    switch (IrpSp->MinorFunction) {

    case IRP_MN_QUERY_DEVICE_RELATIONS:

      if (IrpSp->Parameters.QueryDeviceRelations.Type == TargetDeviceRelation){

        switch (PtrToUlong(IrpSp->FileObject->FsContext2))
        {
        case TDI_CONNECTION_FILE:

            // Get the connection object and verify
            Connection = IrpSp->FileObject->FsContext;

            //
            // This adds a connection reference of type BY_ID if successful.
            //

            Status = lpx_VerifyConnectionObject(Connection);	// connection  ref + : 1

            if (NT_SUCCESS (Status)) {

                //
                // Get the PDO associated with conn's device object
                //

                PnPContext = Connection->Provider->PnPContext;
                if (PnPContext) {

                    DeviceRelations = 
                        ExAllocatePoolWithTag(NonPagedPool,
                                              sizeof(DEVICE_RELATIONS),
                                              LPX_MEM_TAG_DEVICE_PDO);
                    if (DeviceRelations) {

                        //
                        // TargetDeviceRelation allows exactly 1 PDO. fill it.
                        //
                        DeviceRelations->Count = 1;
                        DeviceRelations->Objects[0] = PnPContext;
                        ObReferenceObject(PnPContext);

                    } else {
                        Status = STATUS_NO_MEMORY;
                    }
                } else {
                    Status = STATUS_INVALID_DEVICE_STATE;
                }
            
                lpx_DereferenceConnection (Connection);		// connection ref - : 0
            }
            break;
            
        case TDI_TRANSPORT_ADDRESS_FILE:

            Status = STATUS_UNSUCCESSFUL;
            break;
        }
      }
    }

    //
    // Invoker of this irp will free the information buffer.
    //

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = (ULONG_PTR) DeviceRelations;


    return Status;
} /* lpxDispatchPnPPower */

VOID
LpxCompleteSendIrp(
    IN PIRP Irp,
    IN NTSTATUS Status,
    IN ULONG Information
    )
{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PTP_CONNECTION Connection;

    ASSERT (Status != STATUS_PENDING);

    Connection = IRP_SEND_CONNECTION(IrpSp);

 
    DebugPrint(2, ("NbfCompleteSendIrp:  Entered IRP %lx, connection %lx\n",
            Irp, Connection));
   

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;

    LpxDereferenceSendIrp (IrpSp);     // remove creation reference.

    lpx_DereferenceConnection(Connection);		// connection - : -1

} /* NbfCompleteSendIrp */
