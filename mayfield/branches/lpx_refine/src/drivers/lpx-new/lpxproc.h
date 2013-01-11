/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPXPROC_H_
#define _LPXPROC_H_

VOID
LpxInitServicePoint(
    IN OUT    PTP_CONNECTION    Connection
    );

BOOLEAN
LpxCloseServicePoint(
    IN OUT    PSERVICE_POINT Connection
    );

NTSTATUS
LpxAssignPort(
    IN PDEVICE_CONTEXT    AddressDeviceContext,
    IN PLPX_ADDRESS        SourceAddress
    );

PTP_ADDRESS
LpxLookupAddress(
    IN PDEVICE_CONTEXT    DeviceContext,
    IN PLPX_ADDRESS        SourceAddress
    );


VOID
LpxAssociateAddress(
    IN OUT    PTP_CONNECTION    Connection
    );

NTSTATUS
LpxConnect(
    IN         PTP_CONNECTION    Connection,
     IN OUT    PIRP            Irp
   );

NTSTATUS
LpxListen(
    IN         PTP_CONNECTION    Connection,
     IN OUT    PIRP            Irp
   );

NDIS_STATUS
LpxAccept(
    IN         PTP_CONNECTION    Connection,
     IN OUT    PIRP            Irp
   );

NDIS_STATUS
LpxDisconnect(
    IN         PTP_CONNECTION    Connection,
     IN OUT    PIRP            Irp
    );

NTSTATUS
LpxSend(
    IN         PTP_CONNECTION    Connection,
     IN OUT    PIRP            Irp
   );

VOID
LpxSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
    );

NTSTATUS
LpxRecv(
    IN         PTP_CONNECTION    Connection,
     IN OUT    PIRP            Irp
   );


NDIS_STATUS
LpxSendDatagram(
    IN         PTP_ADDRESS    Address,
     IN OUT    PIRP        Irp
   );

NDIS_STATUS
LpxReceiveIndicate(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
LpxTransferDataComplete (
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  Packet,
    IN NDIS_STATUS   Status,
    IN UINT          BytesTransfered
    );

VOID
LpxReceiveComplete (
                    IN NDIS_HANDLE BindingContext
                    );
extern LONG        NumberOfPackets;
extern ULONG    NumberOfSentComplete;

extern ULONG    NumberOfSent;
extern ULONG    NumberOfSentComplete;

PNDIS_PACKET
PacketDequeue(
    PLIST_ENTRY    PacketQueue,
    PKSPIN_LOCK    QSpinLock
    );

VOID
PacketFree(
    IN PNDIS_PACKET    Packet
    );

NTSTATUS
PacketAllocate(
    IN    PSERVICE_POINT        ServicePoint,
    IN    ULONG                PacketLength,
    IN    PDEVICE_CONTEXT        DeviceContext,
    IN    BOOLEAN                Send,
    IN    PUCHAR                CopyData,
    IN    ULONG                CopyDataLength,
    IN    PIO_STACK_LOCATION    IrpSp,
    OUT    PNDIS_PACKET        *Packet
    );

PNDIS_PACKET
PacketCopy(
    IN    PNDIS_PACKET Packet,
    OUT    PLONG    Cloned
    ) ;

PNDIS_PACKET
PacketClone(
    IN    PNDIS_PACKET Packet
    );

BOOLEAN
PacketQueueEmpty(
    PLIST_ENTRY    PacketQueue,
    PKSPIN_LOCK    QSpinLock
    );

PNDIS_PACKET
PacketPeek(
    PLIST_ENTRY    PacketQueue,
    PKSPIN_LOCK    QSpinLock
    );

void
CallUserDisconnectHandler(
    IN    PSERVICE_POINT    pServicePoint,
    IN    ULONG            DisconnectFlags
    );

VOID LpxChangeState(
    IN PTP_CONNECTION Connection,
    IN SMP_STATE NewState,
    IN BOOLEAN Locked    
);

#endif

