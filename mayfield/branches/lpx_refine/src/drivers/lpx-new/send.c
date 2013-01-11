/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop


NTSTATUS
LpxTdiSend(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiSend request for the transport provider.

    NOTE: THIS FUNCTION MUST BE CALLED AT DPC LEVEL.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION connection;
    PIO_STACK_LOCATION irpSp;
 
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
        (connection->Type != LPX_CONNECTION_SIGNATURE) ||
		(connection->Flags2 & CONNECTION_FLAGS2_STOPPING)
		){//||
//		(connection->IsDisconnted == TRUE)) {
#if DBG
        LpxPrint2 ("TdiSend: Invalid Connection %lx Irp %lx\n", connection, Irp);
#endif
        return STATUS_INVALID_CONNECTION;
    }
 
	LpxSend(
		connection,
		Irp
		);


	return STATUS_PENDING;
} /* TdiSend */


NTSTATUS
LpxTdiSendDatagram(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiSendDatagram request for the transport
    provider.

Arguments:

    Irp - Pointer to the I/O Request Packet for this request.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PTP_ADDRESS_FILE addressFile;
    PTP_ADDRESS address;
    PIO_STACK_LOCATION irpSp;

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_TRANSPORT_ADDRESS_FILE) {
        return STATUS_INVALID_ADDRESS;
    }

    addressFile  = irpSp->FileObject->FsContext;

    status = LpxVerifyAddressObject (addressFile);
    if (!NT_SUCCESS (status)) {
        IF_LPXDBG (LPX_DEBUG_SENDENG) {
            LpxPrint2 ("TdiSendDG: Invalid address %lx Irp %lx\n",
                    addressFile, Irp);
        }
        return status;
    }

    address = addressFile->Address;

    status = LpxSendDatagram(
        address,
        Irp
        );

    LpxDereferenceAddress("tmp send datagram", address, AREF_VERIFY);

    return status;
} /* LpxTdiSendDatagram */

