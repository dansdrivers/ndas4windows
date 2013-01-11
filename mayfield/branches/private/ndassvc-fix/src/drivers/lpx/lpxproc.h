/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    LpxProc.h

Abstract:

    About LPX protocol.

Revision History:

--*/

#ifndef _LPXPROC_H_
#define _LPXPROC_H_


NTSTATUS
LpxAssignPort(
	IN PDEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	);

PTP_ADDRESS
LpxLookupAddress(
    IN PDEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS		SourceAddress
    );

VOID
LpxCreateAddress(
    IN PTP_ADDRESS Address
    );

VOID
LpxDestroyAddress(
    IN PTP_ADDRESS Address
    );

VOID
LpxAssociateAddress(
    IN OUT	PTP_CONNECTION	Connection
    );

VOID
LpxDisassociateAddress(
    IN OUT	PTP_CONNECTION	Connection
    );

NDIS_STATUS
LpxConnect(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

NDIS_STATUS
LpxListen(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

NDIS_STATUS
LpxAccept(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

NDIS_STATUS
LpxDisconnect(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
    );

VOID
LpxOpenConnection(
    IN OUT	PTP_CONNECTION	Connection
    );

VOID
LpxCloseConnection(
    IN OUT	PTP_CONNECTION	Connection
    );

NDIS_STATUS
LpxSend(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

VOID
LpxSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
    );

NDIS_STATUS
LpxRecv(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );


NDIS_STATUS
LpxSendDatagram(
    IN 		PTP_ADDRESS	Address,
 	IN OUT	PIRP		Irp
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
//@ILGU@ <2003_1120>
VOID 
LpxFreeDeviceContext(
					 IN PDEVICE_CONTEXT DeviceContext
					 );
//@ILGU@ <2003_1120>
extern LONG		NumberOfExportedPackets;
extern LONG		NumberOfPackets;
extern LONG		NumberOfCloned;
extern ULONG	NumberOfSentComplete;

extern ULONG	NumberOfSent;
extern ULONG	NumberOfSentComplete;

PNDIS_PACKET
PacketDequeue(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

VOID
PacketFree(
	IN PNDIS_PACKET	Packet
	);

NTSTATUS
PacketAllocate(
	IN	PSERVICE_POINT		ServicePoint,
	IN	ULONG				PacketLength,
	IN	PDEVICE_CONTEXT		DeviceContext,
	IN	BOOLEAN				Send,
	IN	PUCHAR				CopyData,
	IN	ULONG				CopyDataLength,
	IN	PIO_STACK_LOCATION	IrpSp,
	OUT	PNDIS_PACKET		*Packet
	);

PNDIS_PACKET
PacketCopy(
	IN	PNDIS_PACKET Packet,
	OUT	PLONG	Cloned
	) ;

PNDIS_PACKET
PacketClone(
	IN	PNDIS_PACKET Packet
	);

BOOLEAN
PacketQueueEmpty(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

PNDIS_PACKET
PacketPeek(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

void
CallUserDisconnectHandler(
				  IN	PSERVICE_POINT	pServicePoint,
				  IN	ULONG			DisconnectFlags
				  );
#endif

#if 0
///////////////////////////////////////
//
// Defines.
//

// 
// Protocol Type
//
#define LPX_TYPE_LPX	0
#define LPX_TYPE_SMP	5

//
// Constants
//
#define ETHERNET_ADDRESS_LENGTH	6

///////////////////////////////////////
//
// Structures.
//

#include <pshpack1.h>

//
// LPX Address
//
typedef struct _LPX_ADDRESS {
	USHORT	Port;
	union {
		UCHAR	Node[ETHERNET_ADDRESS_LENGTH];
	};
} LPX_ADDRESS, *PLPX_ADDRESS;

//
// LPX Header
//
typedef struct _LPX_HEADER {
	USHORT		PacketSize;
	UCHAR		Type;
	USHORT		CheckSum;
	LPX_ADDRESS	SourceAddress;
	LPX_ADDRESS	DestinationAddress;
} LPX_HEADER, *PLPX_HEADER;

#include <poppack.h>


#endif // def _LPX_H_
