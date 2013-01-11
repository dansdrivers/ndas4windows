/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop


NTSTATUS
LpxTdiReceive(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiReceive request for the transport provider.

Arguments:

    Irp - I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION connection;
     PIO_STACK_LOCATION irpSp;
    NTSTATUS status;
    //
    // verify that the operation is taking place on a connection. At the same
    // time we do this, we reference the connection. This ensures it does not
    // get removed out from under us. Note also that we do the connection
    // lookup within a try/except clause, thus protecting ourselves against
    // really bogus handles
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection = irpSp->FileObject->FsContext;

    IF_LPXDBG (LPX_DEBUG_RCVENG) {
        LpxPrint2 ("LpxTdiReceive: Received IRP %p on connection %p\n", 
                        Irp, connection);
    }

    //
    // Check that this is really a connection.
    //

    if ((irpSp->FileObject->FsContext2 == (PVOID)LPX_FILE_TYPE_CONTROL) ||
        (connection->Size != sizeof (TP_CONNECTION)) ||
        (connection->Type != LPX_CONNECTION_SIGNATURE) ||
        (connection->Flags2 & CONNECTION_FLAGS2_STOPPING)
        ) {//|| 
//		(connection->IsDisconnted == TRUE)) {
#if DBG
        LpxPrint2 ("TdiReceive: Invalid Connection %p Irp %p\n", connection, Irp);
#endif
        return STATUS_INVALID_CONNECTION;
    }

    status = LpxRecv(connection, Irp);

    return status;
 } /* TdiReceive */



VOID
LpxCancelReceiveDatagram(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to cancel a receive
    datagram. The receive is looked for on the address file's
    receive datagram queue; if it is found it is cancelled.

    NOTE: This routine is called with the CancelSpinLock held and
    is responsible for releasing it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    PIO_STACK_LOCATION IrpSp;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
    PLIST_ENTRY p;
    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_RECEIVE_DATAGRAM));

    AddressFile = IrpSp->FileObject->FsContext;
    Address = AddressFile->Address;

    //
    // Since this IRP is still in the cancellable state, we know
    // that the address file is still around (although it may be in
    // the process of being torn down). See if the IRP is on the list.
    //

    Found = FALSE;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    for (p = AddressFile->ReceiveDatagramQueue.Flink;
        p != &AddressFile->ReceiveDatagramQueue;
        p = p->Flink) {

        if (CONTAINING_RECORD(p, IRP, Tail.Overlay.ListEntry) == Irp) {
            RemoveEntryList (p);
            Found = TRUE;
            break;
        }
    }

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
    IoReleaseCancelSpinLock (Irp->CancelIrql);

    if (Found) {

#if DBG
        DbgPrint("LPX: Canceled receive datagram %p on %p\n",
                Irp, AddressFile);
#endif

        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

        LpxDereferenceAddress ("Receive DG cancelled", Address, AREF_REQUEST);

    } else {

#if DBG
        DbgPrint("LPX: Tried to cancel receive datagram %p on %p, not found\n",
                Irp, AddressFile);
#endif

    }

}   /* LpxCancelReceiveDatagram */


NTSTATUS
LpxTdiReceiveDatagram(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiReceiveDatagram request for the transport
    provider. Receive datagrams just get queued up to an address, and are
    completed when a DATAGRAM or DATAGRAM_BROADCAST frame is received at
    the address.

Arguments:

    Irp - I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    KIRQL oldirql;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
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

    addressFile = irpSp->FileObject->FsContext;

    status = LpxVerifyAddressObject (addressFile);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    address = addressFile->Address;

    IoAcquireCancelSpinLock(&cancelIrql);
    ACQUIRE_SPIN_LOCK (&address->SpinLock,&oldirql);

    if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {

        RELEASE_SPIN_LOCK (&address->SpinLock,oldirql);
        IoReleaseCancelSpinLock(cancelIrql);

        Irp->IoStatus.Information = 0;
        status =  (address->Flags & ADDRESS_FLAGS_STOPPING) ?
                    STATUS_NETWORK_NAME_DELETED : STATUS_DUPLICATE_NAME;
    } else {

        //
        // If this IRP has been cancelled, then call the
        // cancel routine.
        //

        if (Irp->Cancel) {

            RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);
            IoReleaseCancelSpinLock(cancelIrql);

            Irp->IoStatus.Information = 0;
            status =  STATUS_CANCELLED;
//            Irp->IoStatus.Status = STATUS_CANCELLED;
//            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        } else {
            IoSetCancelRoutine(Irp, LpxCancelReceiveDatagram);
            LpxReferenceAddress ("Receive datagram", address, AREF_REQUEST);
            InsertTailList (&addressFile->ReceiveDatagramQueue,&Irp->Tail.Overlay.ListEntry);
            RELEASE_SPIN_LOCK (&address->SpinLock,oldirql);
            IoReleaseCancelSpinLock(cancelIrql);
            status = STATUS_PENDING;
        }
    }

    LpxDereferenceAddress ("Temp rcv datagram", address, AREF_VERIFY);

    return status;

} /* TdiReceiveDatagram */
