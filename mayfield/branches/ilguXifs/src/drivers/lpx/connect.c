/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop


NTSTATUS
LpxTdiAccept(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiAccept request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("LpxTdiAccept: Entered.\n");
    }

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

    status = LpxVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    status = LpxAccept(connection,  Irp);

    LpxDereferenceConnection ("Temp TdiAccept", connection, CREF_BY_ID);
    return status;
} /* LpxTdiAccept */


NTSTATUS
LpxTdiAssociateAddress(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the association of the connection and the address for
    the user.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PTP_ADDRESS_FILE addressFile;
    PTP_ADDRESS oldAddress;
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_ASSOCIATE parameters;
    PCONTROL_CONTEXT deviceContext;

    KIRQL oldirql, oldirql2;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("TdiAssociateAddress: Entered.\n");
    }

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // verify that the operation is taking place on a connection. At the same
    // time we do this, we reference the connection. This ensures it does not
    // get removed out from under us. Note also that we do the connection
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;
    
    status = LpxVerifyConnectionObject (connection);
    if (!NT_SUCCESS (status)) {
        return status;
    }


    //
    // Make sure this connection is ready to be associated.
    //

    oldAddress = (PTP_ADDRESS)NULL;

    try {

        ACQUIRE_SPIN_LOCK (&connection->TpConnectionSpinLock, &oldirql2);

        if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {

            //
            // The connection is already associated with
            // an active connection...bad!
            //

            RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, oldirql2);
            LpxDereferenceConnection ("Temp Ref Associate", connection, CREF_BY_ID);

            return STATUS_INVALID_CONNECTION;

        } else {

            //
            // See if there is an old association hanging around...
            // this happens if the connection has been disassociated,
            // but not closed.
            //

            if (connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) {

                IF_LPXDBG (LPX_DEBUG_CONNECT) {
                    LpxPrint0 ("LpxTdiAssociateAddress: removing association.\n");
                }

                //
                // Save this; since it is non-null this address
                // will be dereferenced after the connection
                // spinlock is released.
                //

                oldAddress = connection->AddressFile->Address;

                //
                // Remove the old association.
                //

                connection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
                RemoveEntryList (&connection->AddressFileList);
                InitializeListHead (&connection->AddressFileList);
                connection->AddressFile = NULL;

            }

        }

        RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, oldirql2);

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("LPX: Got exception 1 in LpxTdiAssociateAddress\n");
//        DbgBreakPoint();

        RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, oldirql2);
        LpxDereferenceConnection ("Temp Ref Associate", connection, CREF_BY_ID);
        return GetExceptionCode();
    }


    //
    // If we removed an old association, dereference the
    // address.
    //

    if (oldAddress != (PTP_ADDRESS)NULL) {

        LpxDereferenceAddress("Removed old association", oldAddress, AREF_CONNECTION);

    }


    deviceContext = connection->Provider;

    ASSERT( LpxControlDeviceContext == deviceContext) ;

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

    if (NT_SUCCESS(status)) {

        if (fileObject->DeviceObject == &deviceContext->DeviceObject) {

            //
            // we might have one of our address objects; verify that.
            //

            addressFile = fileObject->FsContext;

            IF_LPXDBG (LPX_DEBUG_CONNECT) {
                LpxPrint3 ("LpxTdiAssociateAddress: Connection %p Irp %p AddressFile %p \n",
                    connection, Irp, addressFile);
            }
            
            if ((fileObject->FsContext2 == (PVOID) TDI_TRANSPORT_ADDRESS_FILE) &&
                (NT_SUCCESS (LpxVerifyAddressObject (addressFile)))) {

                //
                // have an address and connection object. Add the connection to the
                // address object database. Also add the connection to the address
                // file object db (used primarily for cleaning up). Reference the
                // address to account for one more reason for it staying open.
                //

                ACQUIRE_SPIN_LOCK (&addressFile->Address->SpinLock, &oldirql);
                if ((addressFile->Address->Flags & ADDRESS_FLAGS_STOPPING) == 0) {

                    IF_LPXDBG (LPX_DEBUG_CONNECT) {
                        LpxPrint2 ("LpxTdiAssociateAddress: Valid Address %p %p\n",
                            addressFile->Address, addressFile);
                    }

                    try {

                        ACQUIRE_SPIN_LOCK (&connection->TpConnectionSpinLock, &oldirql2);

                        if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                            LpxReferenceAddress (
                                "Connection associated",
                                addressFile->Address,
                                AREF_CONNECTION);
                             InsertTailList (
                                &addressFile->ConnectionDatabase,
                                &connection->AddressFileList);

                            connection->AddressFile = addressFile;
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

                        RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, oldirql2);

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        DbgPrint ("LPX: Got exception 2 in LpxTdiAssociateAddress\n");
//                        DbgBreakPoint();

                        RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, oldirql2);

                        status = GetExceptionCode();
                    }

                } else {

                    status = STATUS_INVALID_HANDLE; //BUGBUG: should this be more informative?
                }

                RELEASE_SPIN_LOCK (&addressFile->Address->SpinLock, oldirql);

                LpxDereferenceAddress ("Temp associate", addressFile->Address, AREF_VERIFY);

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
            
    } else {
        status = STATUS_INVALID_HANDLE;
    }

    if (NT_SUCCESS(status)) {
        PTP_ADDRESS address = connection->AddressFile->Address;

        // to do : clean up this - ServicePoint->SourceAddress, Address is just duplication. remove it! 
        LpxReferenceAddress ("ServicePoint inserting", address, AREF_REQUEST);

        ExInterlockedInsertTailList(&address->ConnectionServicePointList,
                            &connection->ServicePoint.ServicePointListEntry,
                            &address->SpinLock
                            );
        RtlCopyMemory(
                    &connection->ServicePoint.SourceAddress,
                    address->NetworkName,
                    sizeof(LPX_ADDRESS)
                    );
        connection->ServicePoint.Address = address;
    }

 
    LpxDereferenceConnection ("Temp Ref Associate", connection, CREF_BY_ID);

    return status;

} /* TdiAssociateAddress */


NTSTATUS
LpxTdiDisassociateAddress(
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine performs the disassociation of the connection and the address
    for the user. If the connection has not been stopped, it will be stopped
    here.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PTP_CONNECTION connection;
    NTSTATUS status;
    KIRQL  irql;
    
    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("TdiDisassociateAddress: Entered.\n");
    }

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference of type BY_ID.
    //

    status = LpxVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    LpxCloseServicePoint(&connection->ServicePoint);

    ACQUIRE_SPIN_LOCK(&connection->TpConnectionSpinLock, &irql);
    connection->Flags2 |= CONNECTION_FLAGS2_STOPPING;

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
    }
    RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, irql);

    LpxDereferenceConnection ("Temp use in Associate", connection, CREF_BY_ID);

    return STATUS_SUCCESS;

} /* TdiDisassociateAddress */



NTSTATUS
LpxTdiConnect(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiConnect request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
    TDI_ADDRESS_LPX* RemoteAddress;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("LpxTdiConnect: Entered.\n");
    }

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

    status = LpxVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    parameters = (PTDI_REQUEST_KERNEL)(&irpSp->Parameters);

    //
    // Check that the remote is a LPX address.
    //

    if (!LpxValidateTdiAddress(
             parameters->RequestConnectionInformation->RemoteAddress,
             parameters->RequestConnectionInformation->RemoteAddressLength)) {

        LpxDereferenceConnection ("Invalid Address", connection, CREF_BY_ID);
        return STATUS_BAD_NETWORK_PATH;
    }

    RemoteAddress = LpxParseTdiAddress((PTRANSPORT_ADDRESS)(parameters->RequestConnectionInformation->RemoteAddress), FALSE);

    if (RemoteAddress == NULL) {
        LpxDereferenceConnection ("Not valid address", connection, CREF_BY_ID);
        return STATUS_BAD_NETWORK_PATH;
    }

    //
    // copy the called address someplace we can use it.
    //

    connection->CalledAddress.Port =
        RemoteAddress->Port;

    RtlCopyMemory(
        connection->CalledAddress.Node,
        RemoteAddress->Node,
        6);


    status = LpxConnect(
        connection,
        Irp
    );

 
    LpxDereferenceConnection ("Automatic connection", connection, CREF_BY_ID);
    return status; 
} // LpxTdiConnect




NTSTATUS
LpxTdiDisconnect(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiDisconnect request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION connection;
//    LARGE_INTEGER timeout;
    PIO_STACK_LOCATION irpSp;
//    PTDI_REQUEST_KERNEL parameters;
//    KIRQL oldirql;
    NTSTATUS status;


    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("TdiDisconnect: Entered.\n");
    }

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {
        return STATUS_INVALID_CONNECTION;
    }

    connection  = irpSp->FileObject->FsContext;

    //
    // If successful this adds a reference of type BY_ID.
    //

    status = LpxVerifyConnectionObject (connection);
    if (!NT_SUCCESS (status)) {
        return status;
    }

    status = LpxDisconnect(
		connection,
		Irp
		);


    LpxDereferenceConnection ("Ignoring disconnect", connection, CREF_BY_ID);       // release our lookup reference.
	return status;
 } /* TdiDisconnect */


NTSTATUS
LpxTdiListen(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiListen request for the transport provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("TdiListen: Entered.\n");
    }

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

    status = LpxVerifyConnectionObject (connection);

    if (!NT_SUCCESS (status)) {
        return status;
    }

 
    status = LpxListen(
        connection,
        Irp
        );
 
    LpxDereferenceConnection ("Automatic connection", connection, CREF_BY_ID);
    return status;
} /* TdiListen */


NTSTATUS
LpxOpenConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to open a connection. Note that the connection that
    is open is of little use until associated with an address; until then,
    the only thing that can be done with it is close it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PCONTROL_CONTEXT DeviceContext;
    NTSTATUS status;
    PTP_CONNECTION connection;
    PFILE_FULL_EA_INFORMATION ea;

    UNREFERENCED_PARAMETER (Irp);

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("LpxOpenConnection: Entered.\n");
    }

    DeviceContext = (PCONTROL_CONTEXT)DeviceObject;
    ASSERT( LpxControlDeviceContext == DeviceContext) ;

    // Make sure we have a connection context specified in the EA info
    ea = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    if (ea->EaValueLength < sizeof(PVOID)) {
        return STATUS_INVALID_PARAMETER;
    }

    // Then, try to make a connection object to represent this pending
    // connection.  Then fill in the relevant fields.
    // In addition to the creation, if successful LpxCreateConnection
    // will create a second reference which is removed once the request
    // references the connection, or if the function exits before that.

    status = LpxCreateConnection (DeviceContext, &connection);
    if (!NT_SUCCESS (status)) {
        DebugPrint(1, ("LpxOpenConnection: error LpxCreateConnection\n") );
        return status;                          // sorry, we couldn't make one.
    }

    LpxInitServicePoint(connection);

    //
    // set the connection context so we can connect the user to this data
    // structure
    //

    RtlCopyMemory (
        &connection->Context,
        &ea->EaName[ea->EaNameLength+1],
        sizeof (PVOID));

    //
    // let file object point at connection and connection at file object
    //

    IrpSp->FileObject->FsContext = (PVOID)connection;
    IrpSp->FileObject->FsContext2 = (PVOID)TDI_CONNECTION_FILE;
    connection->FileObject = IrpSp->FileObject;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint1 ("LpxOpenConnection: Opened Connection %p.\n",
              connection);
    }

    return status;

} /* LpxOpenConnection */


NTSTATUS
LpxCloseConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to close a connection. There may be actions in
    progress on this connection, so we note the closing IRP, mark the
    connection as closing, and complete it somewhere down the road (when all
    references have been removed).

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS status;
    PTP_CONNECTION connection;
    KIRQL irql;
    UNREFERENCED_PARAMETER (DeviceObject);
    UNREFERENCED_PARAMETER (Irp);

    //
    // is the file object a connection?
    //

    connection  = IrpSp->FileObject->FsContext;

    if ((connection->Size == sizeof (TP_CONNECTION)) &&
        (connection->Type == LPX_CONNECTION_SIGNATURE)) {
        ACQUIRE_SPIN_LOCK (&connection->TpConnectionSpinLock, &irql);
        if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_CONNECTION;
        }
        RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, irql);
    } else {
        status = STATUS_INVALID_CONNECTION;
    }

    if (!NT_SUCCESS (status)) {
        return status;
    }

    ACQUIRE_SPIN_LOCK (&connection->TpConnectionSpinLock, &irql);
    if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) != 0) {
        RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, irql);
        LpxPrint1("Closing already-closing connection %p\n", connection);
        return STATUS_INVALID_CONNECTION;
    }
    RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, irql);

    LpxCloseServicePoint(&connection->ServicePoint);

    ACQUIRE_SPIN_LOCK (&connection->TpConnectionSpinLock, &irql);


    connection->Flags2 |= CONNECTION_FLAGS2_CLOSING;

    connection->Flags2 |= CONNECTION_FLAGS2_STOPPING;

    //
    // If the connection is still associated, disassociate it.
    //

    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
    }

    //
    // Save this to complete the IRP later.
    //

    connection->CloseIrp = Irp;

    RELEASE_SPIN_LOCK (&connection->TpConnectionSpinLock, irql);

    //
    // dereference for the creation. Note that this dereference
    // here won't have any effect until the regular reference count
    // hits zero.
    //

    LpxDereferenceConnectionSpecial (" Closing Connection", connection, CREF_SPECIAL_CREATION);

    return STATUS_PENDING;

} /* LpxCloseConnection */


VOID
LpxAllocateConnection(
    IN PCONTROL_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )

/*++

Routine Description:

    This routine allocates storage for a transport connection. Some
    minimal initialization is done.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - the device context for this connection to be
        associated with.

    TransportConnection - Pointer to a place where this routine will
        return a pointer to a transport connection structure. Returns
        NULL if the storage cannot be allocated.

Return Value:

    None.

--*/

{

    PTP_CONNECTION Connection;

    Connection = (PTP_CONNECTION)ExAllocatePoolWithTag (
                                     NonPagedPool,
                                     sizeof (TP_CONNECTION),
                                     LPX_MEM_TAG_TP_CONNECTION);
    if (Connection == NULL) {
        PANIC("LPX: Could not allocate connection: no pool\n");
        *TransportConnection = NULL;
        return;
    }
    RtlZeroMemory (Connection, sizeof(TP_CONNECTION));

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint1 ("ExAllocatePool Connection %p\n", Connection);
    }

     ++DeviceContext->ConnectionAllocated;

    Connection->Type = LPX_CONNECTION_SIGNATURE;
    Connection->Size = sizeof (TP_CONNECTION);

    Connection->Provider = DeviceContext;
    Connection->ProviderInterlock = &DeviceContext->Interlock;
    KeInitializeSpinLock (&Connection->TpConnectionSpinLock);


//    InitializeListHead (&Connection->LinkList);
    InitializeListHead (&Connection->AddressFileList);


    *TransportConnection = Connection;

}   /* LpxAllocateConnection */


VOID
LpxDeallocateConnection(
    IN PCONTROL_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine frees storage for a transport connection.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - the device context for this connection to be
        associated with.

    TransportConnection - Pointer to a transport connection structure.

Return Value:

    None.

--*/

{
    ASSERT( LpxControlDeviceContext == DeviceContext) ;
    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint1 ("ExFreePool Connection: %p\n", TransportConnection);
    }

    ExFreePool (TransportConnection);
    --DeviceContext->ConnectionAllocated;
 
}   /* LpxDeallocateConnection */


NTSTATUS
LpxCreateConnection(
    IN PCONTROL_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )

/*++

Routine Description:

    This routine creates a transport connection. The reference count in the
    connection is automatically set to 1, and the reference count in the
    DeviceContext is incremented.

Arguments:

    Address - the address for this connection to be associated with.

    TransportConnection - Pointer to a place where this routine will
        return a pointer to a transport connection structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION Connection;
    KIRQL oldirql;
//    PLIST_ENTRY p;
    ASSERT( LpxControlDeviceContext == DeviceContext) ;
    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("LpxCreateConnection:  Entered.\n");
    }

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

#if 0 // disable connection pool
    p = RemoveHeadList (&DeviceContext->ConnectionPool);
    if (p == &DeviceContext->ConnectionPool) {

        if ((DeviceContext->ConnectionMaxAllocated == 0) ||
            (DeviceContext->ConnectionAllocated < DeviceContext->ConnectionMaxAllocated)) {

            LpxAllocateConnection (DeviceContext, &Connection);
            IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
                LpxPrint1 ("LPX: Allocated connection at %lx\n", Connection);
            }

        } else {
            // out of resource
            Connection = NULL;

        }

        if (Connection == NULL) {
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
            PANIC ("LpxCreateConnection: Could not allocate connection object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {
        Connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);
        InitializeListHead (p);
    }
#else 
    LpxAllocateConnection (DeviceContext, &Connection);
    if (Connection == NULL) {
        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
        PANIC ("LpxCreateConnection: Could not allocate connection object!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
#endif
    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint1 ("LpxCreateConnection:  Connection at %p.\n", Connection);
    }

    //
    // We have two references; one is for creation, and the
    // other is a temporary one so that the connection won't
    // go away before the creator has a chance to access it.
    //

    InterlockedExchange(&Connection->SpecialRefCount, 1);
    InterlockedExchange(&Connection->ReferenceCount, -1);   // this is -1 based

#if DBG
    {
        UINT Counter;
        for (Counter = 0; Counter < NUMBER_OF_CREFS; Counter++) {
            Connection->RefTypes[Counter] = 0;
        }

        // This reference is removed by LpxCloseConnection

        Connection->RefTypes[CREF_SPECIAL_CREATION] = 1;
    }
#endif

    //
    // Initialize the request queues & components of this connection.
    //
 
    InitializeListHead (&Connection->AddressFileList);

    Connection->Flags2 = 0;
     Connection->Context = NULL;                 // no context yet.
     Connection->DisconnectIrp = (PIRP)NULL;
    Connection->CloseIrp = (PIRP)NULL;
    Connection->AddressFile = NULL;

 
    //
    // Now assign this connection an ID. This is used later to identify the
    // connection across multiple processes.
    //
    // The high bit of the ID is not user, it is off for connection
    // initiating NAME.QUERY frames and on for ones that are the result
    // of a FIND.NAME request.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);
    DeviceContext->ConnectionInUse++;
    LpxReferenceControlContext ("Create Connection", DeviceContext, DCREF_CONNECTION);
    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    *TransportConnection = Connection;  // return the connection.

    return STATUS_SUCCESS;
} /* LpxCreateConnection */


NTSTATUS
LpxVerifyConnectionObject (
    IN PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called to verify that the pointer given us in a file
    object is in fact a valid connection object.

Arguments:

    Connection - potential pointer to a TP_CONNECTION object.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INVALID_CONNECTION otherwise

--*/

{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // try to verify the connection signature. If the signature is valid,
    // get the connection spinlock, check its state, and increment the
    // reference count if it's ok to use it. Note that being in the stopping
    // state is an OK place to be and reference the connection; we can
    // disassociate the address while running down.
    //

    try {

        if ((Connection != (PTP_CONNECTION)NULL) &&
            (Connection->Size == sizeof (TP_CONNECTION)) &&
            (Connection->Type == LPX_CONNECTION_SIGNATURE)) {

            ACQUIRE_SPIN_LOCK (&Connection->TpConnectionSpinLock, &oldirql);

            if ((Connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                LpxReferenceConnection ("Verify Temp Use", Connection, CREF_BY_ID);

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_SPIN_LOCK (&Connection->TpConnectionSpinLock, oldirql);

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         return GetExceptionCode();
    }

    return status;

}


NTSTATUS
LpxDestroyAssociation(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine destroys the association between a transport connection and
    the address it was formerly associated with. The only action taken is
    to disassociate the address and remove the connection from all address
    queues.

    This routine is only called by LpxDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql2;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
    BOOLEAN NotAssociated = FALSE;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint1 ("LpxDestroyAssociation:  Entered for connection %p.\n",
                    TransportConnection);
    }

    ACQUIRE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, &oldirql2);
    if ((TransportConnection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {
         RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql2);
        NotAssociated = TRUE;
    } else {
        TransportConnection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
        RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql2);
    }

    if (NotAssociated) {
        return STATUS_SUCCESS;
    }

    addressFile = TransportConnection->AddressFile;

    address = addressFile->Address;

    //
    // Delink this connection from its associated address connection
    // database.  To do this we must spin lock on the address object as
    // well as on the connection,
    //

    ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

    ACQUIRE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, &oldirql2);
    RemoveEntryList (&TransportConnection->AddressFileList);

     InitializeListHead (&TransportConnection->AddressFileList);

    //
    // remove the association between the address and the connection.
    //

    TransportConnection->AddressFile = NULL;

    RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql2);

    RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

    //
    // and remove a reference to the address
    //

    LpxDereferenceAddress ("Destroy association", address, AREF_CONNECTION);


    return STATUS_SUCCESS;

} /* LpxDestroyAssociation */


NTSTATUS
LpxDestroyConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine destroys a transport connection and removes all references
    made by it to other objects in the transport.  The connection structure
    is returned to our lookaside list.  It is assumed that the caller
    has removed all IRPs from the connections's queues first.

    This routine is only called by LpxDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PCONTROL_CONTEXT DeviceContext;
    PIRP CloseIrp;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint1 ("LpxDestroyConnection:  Entered for connection %p.\n",
                    TransportConnection);
    }
    DebugPrint(3, ("!!!!!!CALL LpxDestroyConnection:   for connection %p.\n",
                    TransportConnection));

#if DBG
    ACQUIRE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, &oldirql);
     if (!(TransportConnection->Flags2 & CONNECTION_FLAGS2_STOPPING)) {
        LpxPrint1 ("attempt to destroy unstopped connection %p\n", TransportConnection);
        DbgBreakPoint ();
        RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql);        
        return STATUS_UNSUCCESSFUL; // sjcho temporary : need to find the real reason..
    }
    RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql);
 #endif

    DeviceContext = TransportConnection->Provider;
    ASSERT( LpxControlDeviceContext == DeviceContext) ;
    
    //
    // Destroy any association that this connection has.
    //

    LpxDestroyAssociation (TransportConnection);

    //
    // Clear out any associated nasties hanging around the connection. Note
    // that the current flags are set to STOPPING; this way anyone that may
    // maliciously try to use the connection after it's dead and gone will
    // just get ignored.
    //
    ACQUIRE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, &oldirql);
    TransportConnection->Flags2 |= CONNECTION_FLAGS2_CLOSING;

    //
    // Now complete the close IRP. This will be set to non-null
    // when CloseConnection was called.
    //

    CloseIrp = TransportConnection->CloseIrp;

    if (CloseIrp != (PIRP)NULL) {
        KIRQL	cancelIrql;
        RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql);

        IoAcquireCancelSpinLock(&cancelIrql);
        IoSetCancelRoutine(CloseIrp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);
        TransportConnection->CloseIrp = (PIRP)NULL;
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);
    } else {
        RELEASE_SPIN_LOCK (&TransportConnection->TpConnectionSpinLock, oldirql);    
        LpxPrint1("Connection %p destroyed, no CloseIrp!!\n", TransportConnection);
    }

    //
    // Return the connection to the provider's pool.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    --DeviceContext->ConnectionInUse;

#if 0
    if ((DeviceContext->ConnectionAllocated - DeviceContext->ConnectionInUse) >
            DeviceContext->ConnectionInitAllocated) {
        LpxDeallocateConnection (DeviceContext, TransportConnection);
        IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
            LpxPrint1 ("LPX: Deallocated connection at %lx\n", TransportConnection);
        }
    } else {
        InsertTailList (&DeviceContext->ConnectionPool, &TransportConnection->LinkList);
    }
#else
	LpxDeallocateConnection (DeviceContext, TransportConnection);
#endif

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    LpxDereferenceControlContext ("Destroy Connection", DeviceContext, DCREF_CONNECTION);

    return STATUS_SUCCESS;

} /* LpxDestroyConnection */


#if DBG
VOID
LpxRefConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine increments the reference count on a transport connection.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint2 ("LpxReferenceConnection: entered for connection %p, "
                    "current level=%ld.\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount);
    }


    result = InterlockedIncrement (&TransportConnection->ReferenceCount);

    if (result == 0) {

        //
        // The first increment causes us to increment the
        // "ref count is not zero" special ref.
        //
        InterlockedIncrement(&TransportConnection->SpecialRefCount);

#if DBG
	InterlockedIncrement (&TransportConnection->RefTypes[CREF_SPECIAL_TEMP]);
#endif

    }

    ASSERT (result >= 0);

} /* LpxRefConnection */
#endif


VOID
LpxDerefConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine dereferences a transport connection by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    LpxDestroyConnection to remove it from the system.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint2 ("LpxDereferenceConnection: entered for connection %p, "
                    "current level=%ld.\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount);
    }

    result = InterlockedDecrement (&TransportConnection->ReferenceCount);

    //
    // If all the normal references to this connection are gone, then
    // we can remove the special reference that stood for
    // "the regular ref count is non-zero".
    //

    if (result < 0) {

        //
        // Now it is OK to let the connection go away.
        //

        LpxDereferenceConnectionSpecial ("Regular ref gone", TransportConnection, CREF_SPECIAL_TEMP);

    }


} /* LpxDerefConnection */


VOID
LpxDerefConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routines completes the dereferencing of a connection.
    It may be called any time, but it does not do its work until
    the regular reference count is also 0.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    LONG SpRefCount;
    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint3 ("LpxDereferenceConnectionSpecial: entered for connection %p, "
                    "current level=%ld (%ld).\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount,
                    TransportConnection->SpecialRefCount);
    }
 

    ACQUIRE_SPIN_LOCK (TransportConnection->ProviderInterlock, &oldirql);

    SpRefCount = InterlockedDecrement(&TransportConnection->SpecialRefCount);

    if (SpRefCount == 0 &&
        (TransportConnection->ReferenceCount == -1)) {

        //
        // If we have deleted all references to this connection, then we can
        // destroy the object.  It is okay to have already released the spin
        // lock at this point because there is no possible way that another
        // stream of execution has access to the connection any longer.
        //


        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);

        LpxDestroyConnection (TransportConnection);

    } else {

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);

    }

} /* LpxDerefConnectionSpecial */

