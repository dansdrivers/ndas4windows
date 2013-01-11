/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

This module contains code which performs the following TDI services:

o   TdiAccept
o   TdiListen
o   TdiConnect
o   TdiDisconnect
o   TdiAssociateAddress
o   TdiDisassociateAddress
o   OpenConnection
o   CloseConnection

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef notdef // RASAUTODIAL
#include <acd.h>
#include <acdapi.h>
#endif // RASAUTODIAL

#ifdef notdef // RASAUTODIAL
extern BOOLEAN fAcdLoadedG;
extern ACD_DRIVER AcdDriverG;

//
// Imported functions.
//
VOID
LpxRetryPreTdiConnect(
    IN BOOLEAN fSuccess,
    IN PVOID *pArgs
    );

BOOLEAN
LpxAttemptAutoDial(
    IN PTP_CONNECTION         Connection,
    IN ULONG                  ulFlags,
    IN ACD_CONNECT_CALLBACK   pProc,
    IN PVOID                  pArg
    );

VOID
LpxCancelPreTdiConnect(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    );
#endif // RASAUTODIAL

NTSTATUS
LpxTdiConnectCommon(
    IN PIRP Irp
    );



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

#if __LPX__

	status = LpxAccept( connection, Irp );

	LpxDereferenceConnection( "Temp TdiAccept", connection, CREF_BY_ID );
	return status;

#endif
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
    PDEVICE_CONTEXT deviceContext;

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

        ACQUIRE_C_SPIN_LOCK (&connection->SpinLock, &oldirql2);

        if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {

            //
            // The connection is already associated with
            // an active connection...bad!
            //

            RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql2);
            LpxDereferenceConnection ("Temp Ref Associate", connection, CREF_BY_ID);

            return STATUS_INVALID_CONNECTION;

        } else {

            //
            // See if there is an old association hanging around...
            // this happens if the connection has been disassociated,
            // but not closed.
            //

            if (connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) {

#if __LPX__
				// connection reuse is not supported.
				
				NDAS_BUGON( FALSE );

				RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql2);
				LpxDereferenceConnection ("Temp Ref Associate", connection, CREF_BY_ID);

				return STATUS_INVALID_CONNECTION;
#endif
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
                RemoveEntryList (&connection->AddressList);
                RemoveEntryList (&connection->AddressFileList);
                InitializeListHead (&connection->AddressList);
                InitializeListHead (&connection->AddressFileList);
                connection->AddressFile = NULL;

            }

        }

        RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql2);

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("LPX: Got exception 1 in LpxTdiAssociateAddress\n");
        DbgBreakPoint();

        RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql2);
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

                        ACQUIRE_C_SPIN_LOCK (&connection->SpinLock, &oldirql2);

                        if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                            LpxReferenceAddress (
                                "Connection associated",
                                addressFile->Address,
                                AREF_CONNECTION);

#if DBG
                            if (!(IsListEmpty(&connection->AddressList))) {
                                DbgPrint ("LPX: C %p, new A %p, in use\n",
                                    connection, addressFile->Address);
                                DbgBreakPoint();
                            }
#endif
                            InsertTailList (
                                &addressFile->Address->ConnectionDatabase,
                                &connection->AddressList);

#if DBG
                            if (!(IsListEmpty(&connection->AddressFileList))) {
                                DbgPrint ("LPX: C %p, new AF %p, in use\n",
                                    connection, addressFile);
                                DbgBreakPoint();
                            }
#endif
                            InsertTailList (
                                &addressFile->ConnectionDatabase,
                                &connection->AddressFileList);

                            connection->AddressFile = addressFile;
                            connection->Flags2 |= CONNECTION_FLAGS2_ASSOCIATED;
                            connection->Flags2 &= ~CONNECTION_FLAGS2_DISASSOCIATED;

                            if (addressFile->ConnectIndicationInProgress) {
                                connection->Flags2 |= CONNECTION_FLAGS2_INDICATING;
                            }

							DebugPrint( 0, ("LpxTdiAssociateAddress: connection %p port %X\n",
											connection, addressFile->Address->NetworkName->Port) );

                            status = STATUS_SUCCESS;

                        } else {

                            //
                            // The connection is closing, stop the
                            // association.
                            //

                            status = STATUS_INVALID_CONNECTION;

                        }

                        RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql2);

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        DbgPrint ("LPX: Got exception 2 in LpxTdiAssociateAddress\n");
                        DbgBreakPoint();

                        RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql2);

                        status = GetExceptionCode();
                    }

                } else {

                    status = STATUS_INVALID_HANDLE; //should this be more informative?
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

    KIRQL oldirql;
    PIO_STACK_LOCATION irpSp;
    PTP_CONNECTION connection;
    NTSTATUS status;
//    PDEVICE_CONTEXT DeviceContext;

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

#if __LPX__

	if (connection->LpxSmp.SmpState != SMP_STATE_CLOSE) { // Already closed

		IF_LPXDBG (LPX_DEBUG_CONNECT) {

			LpxPrint2 ("TdiDisassociateAddress: connection->LpxSmp.SmpState = %x, KeGetCurrentIrql() = %d\n", 
						connection->LpxSmp.SmpState, KeGetCurrentIrql() );
		}
		
		if (KeGetCurrentIrql() == PASSIVE_LEVEL || KeGetCurrentIrql() == APC_LEVEL) {
			
			LARGE_INTEGER	interval;
			
			IF_LPXDBG (LPX_DEBUG_CONNECT) {

				LpxPrint0( "Wait\n" ); 
			}

			interval.QuadPart = (-1 * NANO100_PER_SEC);
			KeDelayExecutionThread( KernelMode, FALSE, &interval );
		}
	}

#endif

    KeRaiseIrql (DISPATCH_LEVEL, &oldirql);

    ACQUIRE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    if ((connection->Flags2 & CONNECTION_FLAGS2_STOPPING) == 0) {
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
        LpxStopConnection (connection, STATUS_LOCAL_DISCONNECT);
    } else {
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    }

    //
    // and now we disassociate the address. This only removes
    // the appropriate reference for the connection, the
    // actually disassociation will be done later.
    //
    // The DISASSOCIATED flag is used to make sure that
    // only one person removes this reference.
    //

    ACQUIRE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    } else {
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    }

    KeLowerIrql (oldirql);

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
#if !__LPX__
	KIRQL oldirql;
#endif
    PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL parameters;
    TDI_ADDRESS_NETBIOS * RemoteAddress;

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
    // Check that the remote is a Netbios address.
    //

    if (!LpxValidateTdiAddress(
             parameters->RequestConnectionInformation->RemoteAddress,
             parameters->RequestConnectionInformation->RemoteAddressLength)) {

        LpxDereferenceConnection ("Invalid Address", connection, CREF_BY_ID);
        return STATUS_BAD_NETWORK_PATH;
    }

    RemoteAddress = LpxParseTdiAddress((PTRANSPORT_ADDRESS)(parameters->RequestConnectionInformation->RemoteAddress), FALSE);

    if (RemoteAddress == NULL) {

        LpxDereferenceConnection ("Not Netbios", connection, CREF_BY_ID);
        return STATUS_BAD_NETWORK_PATH;

    }

    //
    // copy the called address someplace we can use it.
    //

#if 0

    connection->CalledAddress.NetbiosNameType =
        RemoteAddress->NetbiosNameType;

    RtlCopyMemory(
        connection->CalledAddress.NetbiosName,
        RemoteAddress->NetbiosName,
        16);
#else
	
	RtlCopyMemory( &connection->CalledAddress,
				   RemoteAddress,
				   sizeof(connection->CalledAddress) );

#endif

#ifdef notdef // RASAUTODIAL
    if (fAcdLoadedG) {
        KIRQL adirql;
        BOOLEAN fEnabled;

        //
        // See if the automatic connection driver knows
        // about this address before we search the
        // network.  If it does, we return STATUS_PENDING,
        // and we will come back here via LpxRetryTdiConnect().
        //
        ACQUIRE_SPIN_LOCK(&AcdDriverG.SpinLock, &adirql);
        fEnabled = AcdDriverG.fEnabled;
        RELEASE_SPIN_LOCK(&AcdDriverG.SpinLock, adirql);
        if (fEnabled && LpxAttemptAutoDial(
                          connection,
                          ACD_NOTIFICATION_PRECONNECT,
                          LpxRetryPreTdiConnect,
                          Irp))
        {
            ACQUIRE_SPIN_LOCK(&connection->SpinLock, &oldirql);
            connection->Flags2 |= CONNECTION_FLAGS2_AUTOCONNECT;
            connection->Status = STATUS_PENDING;
            RELEASE_SPIN_LOCK(&connection->SpinLock, oldirql);
            LpxDereferenceConnection ("Automatic connection", connection, CREF_BY_ID);
            //
            // Set a special cancel routine on the irp
            // in case we get cancelled during the
            // automatic connection.
            //
            IoSetCancelRoutine(Irp, LpxCancelPreTdiConnect);
            return STATUS_PENDING;
        }
    }
#endif // RASAUTODIAL

#if __LPX__

	status = LpxConnect( connection, Irp );

	LpxDereferenceConnection ("Automatic connection", connection, CREF_BY_ID);
	return status;

#endif

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
	PIO_STACK_LOCATION irpSp;
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

#if __LPX__

	status = LpxDisconnect( connection, Irp );

	LpxDereferenceConnection ("Ignoring disconnect", connection, CREF_BY_ID);       // release our lookup reference.
	return status;

#endif

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
    LARGE_INTEGER timeout = {0,0};
	PIO_STACK_LOCATION irpSp;
    PTDI_REQUEST_KERNEL_LISTEN parameters;
    PTDI_CONNECTION_INFORMATION ListenInformation;
    TDI_ADDRESS_NETBIOS * ListenAddress;
    PVOID RequestBuffer2;
    ULONG RequestBuffer2Length;

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

    parameters = (PTDI_REQUEST_KERNEL_LISTEN)&irpSp->Parameters;

    //
    // Record the remote address if there is one.
    //

    ListenInformation = parameters->RequestConnectionInformation;

    if ((ListenInformation != NULL) &&
        (ListenInformation->RemoteAddress != NULL)) {

        if ((LpxValidateTdiAddress(
             ListenInformation->RemoteAddress,
             ListenInformation->RemoteAddressLength)) &&
            ((ListenAddress = LpxParseTdiAddress(ListenInformation->RemoteAddress, FALSE)) != NULL)) {

            RequestBuffer2 = (PVOID)ListenAddress->NetbiosName,
            RequestBuffer2Length = NETBIOS_NAME_LENGTH;

        } else {

            IF_LPXDBG (LPX_DEBUG_CONNECT) {
                LpxPrint0 ("TdiListen: Create Request Failed, bad address.\n");
            }

            LpxDereferenceConnection ("Bad address", connection, CREF_BY_ID);
            return STATUS_BAD_NETWORK_PATH;
        }

    } else {

        RequestBuffer2 = NULL;
        RequestBuffer2Length = 0;
    }

#if __LPX__

	if (ListenInformation)
		DebugPrint( 2, ("ListenInformation->RemoteAddressLength = 0x%x\n", ListenInformation->RemoteAddressLength) );

	status = LpxListen( connection, Irp );

	LpxDereferenceConnection( "Automatic connection", connection, CREF_BY_ID );
	return status;

#endif

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
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS status;
    PTP_CONNECTION connection;
    PFILE_FULL_EA_INFORMATION ea;

    UNREFERENCED_PARAMETER (Irp);

    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint0 ("LpxOpenConnection: Entered.\n");
    }

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;

#if __LPX__
	ASSERT( DeviceContext == SocketLpxDeviceContext );
#endif

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
        return status;                          // sorry, we couldn't make one.
    }

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
        LpxPrint1 ("LPXOpenConnection: Opened Connection %p.\n",
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
    KIRQL oldirql;
    PTP_CONNECTION connection;

    UNREFERENCED_PARAMETER (DeviceObject);
    UNREFERENCED_PARAMETER (Irp);

    //
    // is the file object a connection?
    //

    connection  = IrpSp->FileObject->FsContext;

#if __LPX__
    IF_LPXDBG (LPX_DEBUG_CONNECT) {
        LpxPrint1 ("LpxCloseConnection CO %p:\n",connection);
    }
#endif

    KeRaiseIrql (DISPATCH_LEVEL, &oldirql);

    //
    // We duplicate the code from VerifyConnectionObject,
    // although we don't actually call that since it does
    // a reference, which we don't want (to avoid bouncing
    // the reference count up from 0 if this is a dead
    // link).
    //

    try {

        if ((connection->Size == sizeof (TP_CONNECTION)) &&
            (connection->Type == LPX_CONNECTION_SIGNATURE)) {

            ACQUIRE_DPC_C_SPIN_LOCK (&connection->SpinLock);

            if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                status = STATUS_SUCCESS;

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        KeLowerIrql (oldirql);
        return GetExceptionCode();
    }

    if (!NT_SUCCESS (status)) {
        KeLowerIrql (oldirql);
        return status;
    }

    //
    // We recognize it; is it closing already?
    //

    ACQUIRE_DPC_C_SPIN_LOCK (&connection->SpinLock);

    if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) != 0) {
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
        KeLowerIrql (oldirql);
#if DBG
        LpxPrint1("Closing already-closing connection %p\n", connection);
#endif
        return STATUS_INVALID_CONNECTION;
    }

    connection->Flags2 |= CONNECTION_FLAGS2_CLOSING;

    //
    // if there is activity on the connection, tear it down.
    //

    if ((connection->Flags2 & CONNECTION_FLAGS2_STOPPING) == 0) {
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
        LpxStopConnection (connection, STATUS_LOCAL_DISCONNECT);
        ACQUIRE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    }

    //
    // If the connection is still associated, disassociate it.
    //

    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    } else {
        RELEASE_DPC_C_SPIN_LOCK (&connection->SpinLock);
    }

    //
    // Save this to complete the IRP later.
    //

    connection->CloseIrp = Irp;

#if 0
    //
    // make it impossible to use this connection from the file object
    //

    IrpSp->FileObject->FsContext = NULL;
    IrpSp->FileObject->FsContext2 = NULL;
    connection->FileObject = NULL;
#endif

    KeLowerIrql (oldirql);

    //
    // dereference for the creation. Note that this dereference
    // here won't have any effect until the regular reference count
    // hits zero.
    //

    LpxDereferenceConnectionSpecial (" Closing Connection", connection, CREF_SPECIAL_CREATION);

    return STATUS_PENDING;

} /* LpxCloseConnection */


