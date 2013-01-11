/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    LpxProc.h

Abstract:

    About LPX protocol.

Revision History:

--*/

#ifndef _LPXPROTOCOL_H_
#define _LPXPROTOCOL_H_

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
			   IN	UCHAR				Type,
			   IN	PUCHAR				CopyData,
			   IN	ULONG				CopyDataLength,
			   IN	PIO_STACK_LOCATION	IrpSp,
			   OUT	PNDIS_PACKET		*Packet
			   );


PNDIS_PACKET
PacketClone(
	IN	PNDIS_PACKET Packet
	);


PNDIS_PACKET
PacketCopy(
	IN	PNDIS_PACKET Packet,
	OUT	PLONG	Cloned
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

VOID
LpxCancelSend(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP Irp
			  );



VOID
LpxCancelRecv(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP			Irp
			  );

VOID
LpxCancelConnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
LpxCancelDisconnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


VOID
LpxCancelListen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

LARGE_INTEGER CurrentTime(
	VOID
	);




NTSTATUS
LpxAssignPort(
	IN PCONTROL_DEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	);

PTP_ADDRESS
LpxLookupAddress(
    IN PCONTROL_DEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS		SourceAddress
    );


VOID
LpxDisassociateAddress(
    IN OUT	PTP_CONNECTION	Connection
    );




VOID
LpxCloseConnection(
    IN OUT	PTP_CONNECTION	Connection
    );

VOID
LpxTransferDataComplete(
						IN NDIS_HANDLE   ProtocolBindingContext,
						IN PNDIS_PACKET  Packet,
						IN NDIS_STATUS   Status,
						IN UINT          BytesTransfered
						);


VOID LpxCallUserReceiveHandler(
	PSERVICE_POINT	servicePoint
	);




void LpxCompleteIRPRequest(
	IN 		PSERVICE_POINT	servicePoint,
	IN		PIRP			Irp
	);





VOID
SmpPrintState(
	IN	ULONG			DebugLevel,
	IN	PCHAR			Where,
	IN	PSERVICE_POINT	ServicePoint
	);




VOID
SmpContextInit(
	IN PSERVICE_POINT	ServicePoint,
	IN PSMP_CONTEXT		SmpContext
	);



BOOLEAN
SmpFreeServicePoint(
	IN	PSERVICE_POINT	ServicePoint
	);


static VOID
SmpCancelIrps(
	IN PSERVICE_POINT	ServicePoint
	);



NTSTATUS
TransmitPacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN PACKET_TYPE		PacketType,
	IN USHORT			UserDataLength
	);

VOID
ProcessSentPacket(
				  IN PNDIS_PACKET	Packet
				  );

static void
RoutePacketRequest(
	IN PSMP_CONTEXT	smpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	);

static NTSTATUS
RoutePacket(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	);

VOID
LpxSendComplete(
				IN NDIS_HANDLE	ProtocolBindingContext,
				IN PNDIS_PACKET	Packet,
				IN NDIS_STATUS	Status
				);


static BOOLEAN
SmpSendTest(
			PSMP_CONTEXT	SmpContext,
			PNDIS_PACKET	Packet
			);


static LONGLONG
CalculateRTT(
	IN	PSMP_CONTEXT	SmpContext
	);

static NTSTATUS
SmpReadPacket(
	IN 	PIRP			Irp,
	IN 	PSERVICE_POINT	ServicePoint
	);

VOID
SmpDoReceiveRequest(
	IN PSMP_CONTEXT	smpContext,
	IN PNDIS_PACKET	Packet
	);

static BOOLEAN
SmpDoReceive(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet
	);

static INT
SmpRetransmitCheck(
				   IN PSMP_CONTEXT	SmpContext,
				   IN LONG			AckSequence,
				   IN PACKET_TYPE	PacketType
				   );


static VOID
SmpWorkDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   ); 
static VOID
SmpTimerDpcRoutineRequest(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   );
static VOID
SmpTimerDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   );

void
CallUserDisconnectHandler(
				  IN	PSERVICE_POINT	pServicePoint,
				  IN	ULONG			DisconnectFlags
				  );

#endif	//#ifndef _LPXPROTOCOL_H_







