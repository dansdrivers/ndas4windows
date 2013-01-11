/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiSend
        o   TdiSendDatagram

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode

Revision History:

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
#if DBG
	PTDI_REQUEST_KERNEL_SEND parameters;
#endif

	//
    // Determine which connection this send belongs on.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    connection  = irpSp->FileObject->FsContext;

    //
    // Check that this is really a connection.
    //

    if ((irpSp->FileObject->FsContext2 == UlongToPtr(LPX_FILE_TYPE_CONTROL)) ||
        (connection->Size != sizeof (TP_CONNECTION)) ||
        (connection->Type != LPX_CONNECTION_SIGNATURE)) {
#if DBG
        LpxPrint2 ("TdiSend: Invalid Connection %p Irp %p\n", connection, Irp);
#endif
        return STATUS_INVALID_CONNECTION;
    }

#if DBG
    Irp->IoStatus.Information = 0;              // initialize it.
    Irp->IoStatus.Status = 0x01010101;          // initialize it.
#endif

    //
    // Interpret send options.
    //

#if DBG
    parameters = (PTDI_REQUEST_KERNEL_SEND)(&irpSp->Parameters);
    if ((parameters->SendFlags & TDI_SEND_PARTIAL) != 0) {
        IF_LPXDBG (LPX_DEBUG_SENDENG) {
            LpxPrint0 ("LpxTdiSend: TDI_END_OF_RECORD not found.\n");
        }
    }
#endif

#ifdef __LPX__
	
	return LpxSend( connection, Irp );

#endif

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
    PTDI_REQUEST_KERNEL_SENDDG parameters;
    UINT MaxUserData;

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    if (irpSp->FileObject->FsContext2 != (PVOID) TDI_TRANSPORT_ADDRESS_FILE) {
        return STATUS_INVALID_ADDRESS;
    }

    addressFile  = irpSp->FileObject->FsContext;

    status = LpxVerifyAddressObject (addressFile);
    if (!NT_SUCCESS (status)) {
        IF_LPXDBG (LPX_DEBUG_SENDENG) {
            LpxPrint2 ("TdiSendDG: Invalid address %p Irp %p\n",
                    addressFile, Irp);
        }
        return status;
    }

    address = addressFile->Address;
    parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);

    //
    // Check that the length is short enough.
    //

    MacReturnMaxDataSize(
        &address->Provider->MacInfo,
        NULL,
        0,
        address->Provider->MaxSendPacketSize,
        FALSE,
        &MaxUserData);

    //
    // If we are on a disconnected RAS link, then fail the datagram
    // immediately.
    //

    if ((address->Provider->MacInfo.MediumAsync) &&
        (!address->Provider->MediumSpeedAccurate)) {

        LpxDereferenceAddress("tmp send datagram", address, AREF_VERIFY);
        return STATUS_DEVICE_NOT_READY;
    }

    //
    // Check that the target address includes a Netbios component.
    //

    if (!(LpxValidateTdiAddress(
             parameters->SendDatagramInformation->RemoteAddress,
             parameters->SendDatagramInformation->RemoteAddressLength)) ||
        (LpxParseTdiAddress(parameters->SendDatagramInformation->RemoteAddress, TRUE) == NULL)) {

        LpxDereferenceAddress("tmp send datagram", address, AREF_VERIFY);
        return STATUS_BAD_NETWORK_PATH;
    }

#ifdef __LPX__

	status = LpxSendDatagram( address, Irp );

    LpxDereferenceAddress( "tmp send datagram", address, AREF_VERIFY );

	return status;

#endif

} /* LpxTdiSendDatagram */
