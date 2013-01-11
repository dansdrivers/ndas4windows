/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    info.c

Abstract:

    This module contains code which performs the following TDI services:

        o   TdiQueryInformation
        o   TdiSetInformation

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop


//
// Only the following routine is active in this module. All is is commented
// out waiting for the definition of Get/Set info in TDI version 2.
//

//
// Useful macro to obtain the total length of an MDL chain.
//

#define LpxGetMdlChainLength(Mdl, Length) { \
    PMDL _Mdl = (Mdl); \
    *(Length) = 0; \
    while (_Mdl) { \
        *(Length) += MmGetMdlByteCount(_Mdl); \
        _Mdl = _Mdl->Next; \
    } \
}


//
// Local functions used to satisfy various requests.
//

VOID
LpxStoreProviderStatistics(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTDI_PROVIDER_STATISTICS ProviderStatistics
    );

VOID
LpxStoreAdapterStatus(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    IN PVOID StatusBuffer
    );

VOID
LpxStoreNameBuffers(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PVOID Buffer,
    IN ULONG BufferLength,
    IN ULONG NamesToSkip,
    OUT PULONG NamesWritten,
    OUT PULONG TotalNameCount OPTIONAL,
    OUT PBOOLEAN Truncated
    );


NTSTATUS
LpxTdiQueryInformation(
    IN PDEVICE_CONTEXT DeviceContext,
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
    PTDI_PROVIDER_STATISTICS ProviderStatistics;
    PTDI_CONNECTION_INFO ConnectionInfo;
    ULONG TargetBufferLength;
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

    //
    // what type of status do we want?
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    query = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&irpSp->Parameters;

    switch (query->QueryType) {

#if 0
    case 0x12345678:

        {
            typedef struct _LPX_CONNECTION_STATUS {
                UCHAR LocalName[16];
                UCHAR RemoteName[16];
                BOOLEAN SendActive;
                BOOLEAN ReceiveQueued;
                BOOLEAN ReceiveActive;
                BOOLEAN ReceiveWakeUp;
                ULONG Flags;
                ULONG Flags2;
            } LPX_CONNECTION_STATUS, *PLPX_CONNECTION_STATUS;

            PLPX_CONNECTION_STATUS CurStatus;
            ULONG TotalStatus;
            ULONG AllowedStatus;
            PLIST_ENRY q;

            CurStatus = MmGetSystemAddressForMdl (Irp->MdlAddress);
            TotalStatus = 0;
            AllowedStatus = MmGetMdlByteCount (Irp->MdlAddress) / sizeof(LPX_CONNECTION_STATUS);

            for (p = DeviceContext->AddressDatabase.Flink;
                 p != &DeviceContext->AddressDatabase;
                 p = p->Flink) {

                Address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

                if ((Address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
                    continue;
                }

                for (q = Address->ConnectionDatabase.Flink;
                     q != &Address->ConnectionDatabase;
                     q = q->Flink) {

                    Connection = CONTAINING_RECORD (q, TP_CONNECTION, AddressList);

                    if ((Connection->Flags & CONNECTION_FLAGS_READY) == 0) {
                        continue;
                    }

                    if (TotalStatus >= AllowedStatus) {
                        continue;
                    }

                    RtlMoveMemory (CurStatus->LocalName, Address->NetworkName->NetbiosName, 16);
                    RtlMoveMemory (CurStatus->RemoteName, Connection->RemoteName, 16);

                    CurStatus->Flags = Connection->Flags;
                    CurStatus->Flags2 = Connection->Flags2;
                    CurStatus->SendActive = (BOOLEAN)(!IsListEmpty(&Connection->SendQueue));
                    CurStatus->ReceiveQueued = (BOOLEAN)(!IsListEmpty(&Connection->ReceiveQueue));
                    CurStatus->ReceiveActive = (BOOLEAN)((Connection->Flags & CONNECTION_FLAGS_ACTIVE_RECEIVE) != 0);
                    CurStatus->ReceiveWakeUp = (BOOLEAN)((Connection->Flags & CONNECTION_FLAGS_RECEIVE_WAKEUP) != 0);

                    ++CurStatus;
                    ++TotalStatus;

                }
            }

            Irp->IoStatus.Information = TotalStatus * sizeof(LPX_CONNECTION_STATUS);
            status = STATUS_SUCCESS;

        }

        break;
#endif

    case TDI_QUERY_CONNECTION_INFO:

        //
        // Connection info is queried on a connection,
        // verify this.
        //

        if (irpSp->FileObject->FsContext2 != (PVOID) TDI_CONNECTION_FILE) {

			ASSERT( FALSE );
            return STATUS_INVALID_CONNECTION;
        }

        Connection = irpSp->FileObject->FsContext;

        status = LpxVerifyConnectionObject (Connection);

        if (!NT_SUCCESS (status)) {

			ASSERT( FALSE );
#if DBG
            LpxPrint2 ("TdiQueryInfo: Invalid Connection %p Irp %p\n", Connection, Irp);
#endif
            return status;
        }

        ConnectionInfo = ExAllocatePoolWithTag (
                             NonPagedPool,
                             sizeof (TDI_CONNECTION_INFO),
                             LPX_MEM_TAG_TDI_CONNECTION_INFO);

        if (ConnectionInfo == NULL) {

            PANIC ("LpxQueryInfo: Cannot allocate connection info!\n");
            LpxWriteResourceErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_RESOURCE_POOL,
                6,
                sizeof(TDI_CONNECTION_INFO),
                0);
            status = STATUS_INSUFFICIENT_RESOURCES;

#ifdef __LPX__

		} else {

			LARGE_INTEGER	delay;

            RtlZeroMemory ((PVOID)ConnectionInfo, sizeof(TDI_CONNECTION_INFO));

			ConnectionInfo->Event;
			ConnectionInfo->TransmittedTsdus	= Connection->LpxSmp.NumberOfSendPackets; 
			ConnectionInfo->ReceivedTsdus		= Connection->LpxSmp.NumberofRecvPackets; 
			ConnectionInfo->TransmissionErrors	= Connection->LpxSmp.NumberOfSendRetransmission; 
			ConnectionInfo->ReceiveErrors		= Connection->LpxSmp.DropOfReceivePacket; 

			if (Connection->LpxSmp.ResponseTimeOfLargeSendRequests.QuadPart) {

				ConnectionInfo->Throughput.QuadPart	= 
					Connection->LpxSmp.BytesOfLargeSendRequests.QuadPart * 1000 * 1000 * 10 / 
					Connection->LpxSmp.ResponseTimeOfLargeSendRequests.QuadPart; 			
			
			} else if (Connection->LpxSmp.ResponseTimeOfSmallSendRequests.QuadPart) {

				ConnectionInfo->Throughput.QuadPart	= 
					Connection->LpxSmp.BytesOfSmallSendRequests.QuadPart * 1000 * 1000 * 10 / 
					Connection->LpxSmp.ResponseTimeOfSmallSendRequests.QuadPart; 			
			}			

			if (Connection->LpxSmp.NumberofSmallSendRequests) {
		
				delay.QuadPart = Connection->LpxSmp.ResponseTimeOfSmallSendRequests.QuadPart / Connection->LpxSmp.NumberofSmallSendRequests;	
				ConnectionInfo->Delay.HighPart = -1L;
				ConnectionInfo->Delay.LowPart = (ULONG) - ((LONG)(delay.LowPart));	// 100 ns
			}
			ConnectionInfo->SendBufferSize; 
			ConnectionInfo->ReceiveBufferSize; 
			ConnectionInfo->Unreliable; 

			status = TdiCopyBufferToMdl (
                            (PVOID)ConnectionInfo,
                            0L,
                            sizeof(TDI_CONNECTION_INFO),
                            Irp->MdlAddress,
                            0,
                            &BytesCopied);

            Irp->IoStatus.Information = BytesCopied;

            ExFreePool (ConnectionInfo);
		}

#endif

        LpxDereferenceConnection ("query connection info", Connection, CREF_BY_ID);

        break;

    case TDI_QUERY_ADDRESS_INFO:

        if (irpSp->FileObject->FsContext2 == (PVOID)TDI_TRANSPORT_ADDRESS_FILE) {

            AddressFile = irpSp->FileObject->FsContext;

            status = LpxVerifyAddressObject(AddressFile);

            if (!NT_SUCCESS (status)) {
#if DBG
                LpxPrint2 ("TdiQueryInfo: Invalid AddressFile %p Irp %p\n", AddressFile, Irp);
#endif
                return status;
            }

            UsedConnection = FALSE;

        } else if (irpSp->FileObject->FsContext2 == (PVOID)TDI_CONNECTION_FILE) {

            Connection = irpSp->FileObject->FsContext;

            status = LpxVerifyConnectionObject (Connection);

            if (!NT_SUCCESS (status)) {
#if DBG
                LpxPrint2 ("TdiQueryInfo: Invalid Connection %p Irp %p\n", Connection, Irp);
#endif
                return status;
            }

            AddressFile = Connection->AddressFile;

            UsedConnection = TRUE;

        } else {

            return STATUS_INVALID_ADDRESS;

        }

        Address = AddressFile->Address;
#if 0
        TdiBuildNetbiosAddress(
            Address->NetworkName->NetbiosName,
            (BOOLEAN)(Address->Flags & ADDRESS_FLAGS_GROUP ? TRUE : FALSE),
            &AddressInfo.TaAddressBuffer);
#else
		TdiBuildNetbiosAddress(
            Address->NetworkName->Node,
            FALSE,
            &AddressInfo.TaAddressBuffer);
#endif
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
            LpxWriteResourceErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_RESOURCE_POOL,
                2,
                sizeof(TA_NETBIOS_ADDRESS),
                0);
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

        //
        // This information is probablt available somewhere else.
        //

        LpxGetMdlChainLength (Irp->MdlAddress, &TargetBufferLength);

        if (TargetBufferLength < sizeof(TDI_PROVIDER_STATISTICS) + ((LPX_TDI_RESOURCES-1) * sizeof(TDI_PROVIDER_RESOURCE_STATS))) {

            Irp->IoStatus.Information = 0;
            status = STATUS_BUFFER_OVERFLOW;

        } else {

            ProviderStatistics = ExAllocatePoolWithTag(
                                   NonPagedPool,
                                   sizeof(TDI_PROVIDER_STATISTICS) +
                                     ((LPX_TDI_RESOURCES-1) * sizeof(TDI_PROVIDER_RESOURCE_STATS)),
                                   LPX_MEM_TAG_TDI_PROVIDER_STATS);

            if (ProviderStatistics == NULL) {

                PANIC ("LpxQueryInfo: Cannot allocate provider statistics!\n");
                LpxWriteResourceErrorLog(
                    DeviceContext,
                    EVENT_TRANSPORT_RESOURCE_POOL,
                    7,
                    sizeof(TDI_PROVIDER_STATISTICS),
                    0);
                status = STATUS_INSUFFICIENT_RESOURCES;

            } else {

                LpxStoreProviderStatistics (DeviceContext, ProviderStatistics);

                status = TdiCopyBufferToMdl (
                                (PVOID)ProviderStatistics,
                                0L,
                                sizeof(TDI_PROVIDER_STATISTICS) +
                                  ((LPX_TDI_RESOURCES-1) * sizeof(TDI_PROVIDER_RESOURCE_STATS)),
                                Irp->MdlAddress,
                                0,
                                &BytesCopied);

                Irp->IoStatus.Information = BytesCopied;

                ExFreePool (ProviderStatistics);
            }

        }

        break;

    case TDI_QUERY_SESSION_STATUS:

        status = STATUS_NOT_IMPLEMENTED;
        break;

    case TDI_QUERY_ADAPTER_STATUS:

		ASSERT( FALSE );
		status = STATUS_NOT_IMPLEMENTED;

		break;

    case TDI_QUERY_FIND_NAME:

		ASSERT( FALSE );
		status = STATUS_NOT_IMPLEMENTED;

        break;

    case TDI_QUERY_DATA_LINK_ADDRESS:
    case TDI_QUERY_NETWORK_ADDRESS:

        TaAddress = (PTRANSPORT_ADDRESS)&AddressInfo.TaAddressBuffer;
        TaAddress->TAAddressCount = 1;
        TaAddress->Address[0].AddressLength = 6;
        if (query->QueryType == TDI_QUERY_DATA_LINK_ADDRESS) {
            TaAddress->Address[0].AddressType =
                DeviceContext->MacInfo.MediumAsync ?
                    NdisMediumWan : DeviceContext->MacInfo.MediumType;
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

//
// Quick macros, assumes DeviceContext and ProviderStatistics exist.
//

#define STORE_RESOURCE_STATS_1(_ResourceNum,_ResourceId,_ResourceName) \
{ \
    PTDI_PROVIDER_RESOURCE_STATS RStats = &ProviderStatistics->ResourceStats[_ResourceNum]; \
    RStats->ResourceId = (_ResourceId); \
    RStats->MaximumResourceUsed = DeviceContext->_ResourceName ## MaxInUse; \
    if (DeviceContext->_ResourceName ## Samples > 0) { \
        RStats->AverageResourceUsed = DeviceContext->_ResourceName ## Total / DeviceContext->_ResourceName ## Samples; \
    } else { \
        RStats->AverageResourceUsed = 0; \
    } \
    RStats->ResourceExhausted = DeviceContext->_ResourceName ## Exhausted; \
}

#define STORE_RESOURCE_STATS_2(_ResourceNum,_ResourceId,_ResourceName) \
{ \
    PTDI_PROVIDER_RESOURCE_STATS RStats = &ProviderStatistics->ResourceStats[_ResourceNum]; \
    RStats->ResourceId = (_ResourceId); \
    RStats->MaximumResourceUsed = DeviceContext->_ResourceName ## Allocated; \
    RStats->AverageResourceUsed = DeviceContext->_ResourceName ## Allocated; \
    RStats->ResourceExhausted = DeviceContext->_ResourceName ## Exhausted; \
}


VOID
LpxStoreProviderStatistics(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTDI_PROVIDER_STATISTICS ProviderStatistics
    )

/*++

Routine Description:

    This routine writes the TDI_PROVIDER_STATISTICS structure
    from the device context into ProviderStatistics.

Arguments:

    DeviceContext - a pointer to the device context.

    ProviderStatistics - The buffer that holds the result. It is assumed
        that it is long enough.

Return Value:

    None.

--*/

{

    //
    // Copy all the statistics up to NumberOfResources
    // in one move.
    //

    RtlCopyMemory(
        ProviderStatistics,
        &DeviceContext->Statistics,
        FIELD_OFFSET (TDI_PROVIDER_STATISTICS, NumberOfResources));

    //
    // Copy the resource statistics.
    //

    ProviderStatistics->NumberOfResources = LPX_TDI_RESOURCES;

    STORE_RESOURCE_STATS_1 (1, ADDRESS_RESOURCE_ID, Address);
    STORE_RESOURCE_STATS_1 (2, ADDRESS_FILE_RESOURCE_ID, AddressFile);
    STORE_RESOURCE_STATS_1 (3, CONNECTION_RESOURCE_ID, Connection);

}   /* LpxStoreProviderStatistics */


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

