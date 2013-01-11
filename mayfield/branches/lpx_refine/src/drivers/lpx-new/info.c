/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop

//
// Local functions used to satisfy various requests.
//

  

NTSTATUS
LpxTdiQueryInformation(
    IN PCONTROL_CONTEXT DeviceContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiQueryInformation request for the transport
    provider.

Arguments:

    Irp - the Irp for the requested operation.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
     PTDI_REQUEST_KERNEL_QUERY_INFORMATION query;
    PTA_NETBIOS_ADDRESS broadcastAddress;
     LARGE_INTEGER timeout = {0,0};
     PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
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
    ASSERT( LpxControlDeviceContext == DeviceContext) ;
    //
    // what type of status do we want?
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    query = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&irpSp->Parameters;

    switch (query->QueryType) {
    case TDI_QUERY_CONNECTION_INFO:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    case TDI_QUERY_ADDRESS_INFO:

        if (irpSp->FileObject->FsContext2 == (PVOID)TDI_TRANSPORT_ADDRESS_FILE) {

            AddressFile = irpSp->FileObject->FsContext;

            status = LpxVerifyAddressObject(AddressFile);

            if (!NT_SUCCESS (status)) {
#if DBG
                LpxPrint2 ("TdiQueryInfo: Invalid AddressFile %lx Irp %lx\n", AddressFile, Irp);
#endif
                return status;
            }

            UsedConnection = FALSE;

        } else if (irpSp->FileObject->FsContext2 == (PVOID)TDI_CONNECTION_FILE) {

            Connection = irpSp->FileObject->FsContext;

            status = LpxVerifyConnectionObject (Connection);

            if (!NT_SUCCESS (status)) {
#if DBG
                LpxPrint2 ("TdiQueryInfo: Invalid Connection %lx Irp %lx\n", Connection, Irp);
#endif
                return status;
            }

            AddressFile = Connection->AddressFile;

            UsedConnection = TRUE;

        } else {

            return STATUS_INVALID_ADDRESS;

        }

        Address = AddressFile->Address;

        TdiBuildNetbiosAddress(
            Address->NetworkName->Node,
            FALSE,
            &AddressInfo.TaAddressBuffer);

        //
        // Count the active addresses.
        //

        AddressInfo.ActivityCount = 0;

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

        for (p = Address->AddressFileDatabase.Flink;
             p != &Address->AddressFileDatabase;
             p = p->Flink) {
            ++AddressInfo.ActivityCount;
        }

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        status = TdiCopyBufferToMdl (
                    &AddressInfo,
                    0,
                    sizeof(ULONG) + sizeof(TA_NETBIOS_ADDRESS),
                    Irp->MdlAddress,
                    0,                    
                    &BytesCopied);

        Irp->IoStatus.Information = BytesCopied;

        if (UsedConnection) {

            LpxDereferenceConnection ("query address info", Connection, CREF_BY_ID);

        } else {

            LpxDereferenceAddress ("query address info", Address, AREF_VERIFY);

        }

        break;

    case TDI_QUERY_BROADCAST_ADDRESS:

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
            broadcastAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
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

    case TDI_QUERY_PROVIDER_INFO:

        status = TdiCopyBufferToMdl (
                    &(DeviceContext->Information),
                    0,
                    sizeof (TDI_PROVIDER_INFO),
                    Irp->MdlAddress,
                    0,
                    &BytesCopied);

        Irp->IoStatus.Information = BytesCopied;

        break;

    case TDI_QUERY_PROVIDER_STATISTICS:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    case TDI_QUERY_SESSION_STATUS:

        status = STATUS_NOT_IMPLEMENTED;
        break;

    case TDI_QUERY_ADAPTER_STATUS:
        status = STATUS_NOT_IMPLEMENTED;
        break;
     case TDI_QUERY_FIND_NAME:
        status = STATUS_NOT_IMPLEMENTED;
         break;
    case TDI_QUERY_DATA_LINK_ADDRESS:
    case TDI_QUERY_NETWORK_ADDRESS:

        TaAddress = (PTRANSPORT_ADDRESS)&AddressInfo.TaAddressBuffer;
        TaAddress->TAAddressCount = 1;
        TaAddress->Address[0].AddressLength = 6;
        if (query->QueryType == TDI_QUERY_DATA_LINK_ADDRESS) {
            TaAddress->Address[0].AddressType = NdisMedium802_3; // support 802.3 only
        } else {
            TaAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_UNSPEC;
        }
        RtlCopyMemory (TaAddress->Address[0].Address, DeviceContext->LocalAddress.Address, 6);

        status = TdiCopyBufferToMdl (
                    &AddressInfo.TaAddressBuffer,
                    0,
                    sizeof(TRANSPORT_ADDRESS)+5,
                    Irp->MdlAddress,
                    0,
                    &BytesCopied);
                        
        Irp->IoStatus.Information = BytesCopied;
        break;

    case TDI_QUERY_DATAGRAM_INFO:

        DatagramInfo.MaximumDatagramBytes = 0;
        DatagramInfo.MaximumDatagramCount = 0;

        status = TdiCopyBufferToMdl (
                    &DatagramInfo,
                    0,
                    sizeof(DatagramInfo),
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

} /* LpxTdiQueryInformation */


NTSTATUS
LpxTdiSetInformation(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the TdiSetInformation request for the transport
    provider.

Arguments:

    Irp - the Irp for the requested operation.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    UNREFERENCED_PARAMETER (Irp);    // prevent compiler warnings

    return STATUS_NOT_IMPLEMENTED;

} /* LpxTdiQueryInformation */

