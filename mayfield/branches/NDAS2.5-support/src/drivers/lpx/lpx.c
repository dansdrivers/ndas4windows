/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop


//
// Lock order
// Address -> TP_CONNECTION -> SERVICE_POINT
//

#if 1
	
// Packet Drop Rate.
ULONG			    ulRxPacketDropRate = 0;
ULONG			    ulRxPacketCountForDrop = 0;

ULONG			    ulTxPacketDropRate = 0;
ULONG			    ulTxPacketCountForDrop = 0;

#endif


/* Initialized in LpxConfigureTransport. No lock is required. */
/* These values time units are in 100ns(1/HZ). Pre-calcurated in lpxcnfg.c  */
LONGLONG LpxConnectionTimeout = MSEC_TO_HZ(DEFAULT_CONNECTION_TIMEOUT);
LONGLONG LpxSmpTimeout = MSEC_TO_HZ(DEFAULT_SMP_TIMEOUT);
LONGLONG LpxWaitInterval = MSEC_TO_HZ(DEFAULT_TIME_WAIT_INTERVAL);
LONGLONG LpxAliveInterval = MSEC_TO_HZ(DEFAULT_ALIVE_INTERVAL);
LONGLONG LpxRetransmitDelay = MSEC_TO_HZ(DEFAULT_RETRANSMIT_DELAY);
LONGLONG LpxMaxRetransmitDelay = MSEC_TO_HZ(DEFAULT_MAX_RETRANSMIT_DELAY);
LONG LpxMaxAliveCount = DEFAULT_MAX_ALIVE_COUNT;
LONGLONG LpxMaxRetransmitTime = MSEC_TO_HZ(DEFAULT_MAX_RETRANSMIT_TIME);
LONG LpxMaximumTransferUnit = DEFAULT_MAXIMUM_TRANSFER_UNIT;
LONG MaxPCont = 3;

#define MAX_REORDER_COUNT 64

static PCHAR LpxStateName[] = {
	"NONE",
	"ESTABLISHED",
	"SYN_SENT",
	"SYN_RECV",
	"FIN_WAIT1",
	"FIN_WAIT2",
	"TIME_WAIT",
	"CLOSE",
	"CLOSE_WAIT",
	"LAST_ACK",
	"LISTEN",
	"CLOSING" /* now a valid state */
};

//
//	get the current system clock
//	100 Nano-second unit
//
static
__inline
LARGE_INTEGER CurrentTime(
	VOID
	)
{
	LARGE_INTEGER Time;
	ULONG		Tick;
	
	KeQueryTickCount(&Time);
	Tick = KeQueryTimeIncrement();
	Time.QuadPart = Time.QuadPart * Tick;

	return Time;
}

//
//	IRP cancel routine
//
//	used only in LpxSend
//
VOID
LpxCancelSend(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP Irp
			  )
{

	UNREFERENCED_PARAMETER (DeviceObject);

//
//	we cannot cancel any sending Irp.
//	Every IRPs in LPX is in progress because we don't keep Sending IRP queue in LPX.
//	See section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
//
//	hootch 02052004
//
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	DebugPrint(1, ("LpxCancelSend\n"));

}


//
//	IRP cancel routine
//
//	used only in LpxRecv()
//
VOID
LpxCancelRecv(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP            Irp
			  )
{
	KIRQL		        oldirql;
	PIO_STACK_LOCATION	IrpSp;
	PTP_CONNECTION		pConnection;
	PSERVICE_POINT		pServicePoint;

#if DBG
	DebugPrint(1, ("[LPX]LpxCancelRecv: Canceling Recv IRP %p\n ", Irp));
#endif
	
	UNREFERENCED_PARAMETER (DeviceObject);

	//
	// Get a pointer to the current stack location in the IRP.  This is where
	// the function codes and parameters are stored.
	//
	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
			(IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

	pConnection = IrpSp->FileObject->FsContext;
	pServicePoint = &pConnection->ServicePoint;


	//
	// Remove IRP from ReceiveIrpList.
	//
	ACQUIRE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, &oldirql);

	//
	//	Do not cancel a IRP in progress.
	//	See section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
	//
	//	patched by hootch
	//
	if(Irp->Tail.Overlay.ListEntry.Flink != Irp->Tail.Overlay.ListEntry.Blink) {
		// This Irp is in ReceiveIrpList 
		// Remove from ReceiveIrpList list
		RemoveEntryList(
			&Irp->Tail.Overlay.ListEntry
			);

		InitializeListHead(&Irp->Tail.Overlay.ListEntry);
	} else {
		//
		// IRP in progress. Cannot cancel now. Set flag for other thread to cancel this irp.
		//
//		IoSetCancelRoutine(Irp, NULL);
		pServicePoint->ReserveCanceling = TRUE; 
		RELEASE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, oldirql);
		IoReleaseCancelSpinLock (Irp->CancelIrql);
		DebugPrint(1, ("[LPX]LpxCancelRecv: IRP %p in progress. Cancel cancelling.\n ", Irp));
		return;
	}

	//
	// Unset Cancel Routine.
	//
	IoSetCancelRoutine(Irp, NULL);
	RELEASE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, oldirql);
	IoReleaseCancelSpinLock (Irp->CancelIrql);

	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	
	DebugPrint(1, ("[LPX]LpxCancelRecv: IRP %p completed.\n ", Irp));

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	ACQUIRE_SPIN_LOCK (&pServicePoint->SpSpinLock, &oldirql);
	CallUserDisconnectHandler(pServicePoint, TDI_DISCONNECT_ABORT);	
	RELEASE_SPIN_LOCK (&pServicePoint->SpSpinLock, oldirql);	
	LpxCloseServicePoint(pServicePoint);


//	LpxDereferenceConnection("SmpTimerDpcRoutine", pServicePoint->Connection, CREF_REQUEST);
}

//
//	IRP cancel routine
//
//	used only in LpxConnect()
//
VOID
LpxCancelConnect(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	)
{
	KIRQL oldIrql;
//	KIRQL cancelIrql;
	PIO_STACK_LOCATION IrpSp;
	PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
//	PIRP SendIrp;
//	PLIST_ENTRY p;
//	BOOLEAN Found;

	UNREFERENCED_PARAMETER (DeviceObject);

	DebugPrint(1, ("LpxCancelConnect\n"));

	//
	// Get a pointer to the current stack location in the IRP.  This is where
	// the function codes and parameters are stored.
	//

	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
			(IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

	Connection = IrpSp->FileObject->FsContext;
	ServicePoint = &Connection->ServicePoint;
//	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	ASSERT(ServicePoint->Address);
	ACQUIRE_SPIN_LOCK (&ServicePoint->Address->SpinLock, &oldIrql);
	ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
	ServicePoint->ConnectIrp = NULL;
	RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
	RELEASE_SPIN_LOCK(&ServicePoint->Address->SpinLock, oldIrql);
	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

}

//
//	IRP cancel routine
//
//	used only in LpxDisconnect()
//
VOID
LpxCancelDisconnect(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	)
{

	UNREFERENCED_PARAMETER (DeviceObject);
//
//	we cannot cancel any disconnecting Irp.
//	Every IRPs in LPX is in progress because we don't keep Sending IRP queue in LPX.
//	See section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
//
//	hootch 02052004
//
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	DebugPrint(1, ("LpxCancelDisconnect\n"));

}

VOID
LpxCancelListen(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
	)
{
	KIRQL oldIrql;
	PIO_STACK_LOCATION IrpSp;
	PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;

	UNREFERENCED_PARAMETER (DeviceObject);

	DebugPrint(1, ("LpxCancelListen\n"));

	//
	// Get a pointer to the current stack location in the IRP.  This is where
	// the function codes and parameters are stored.
	//

	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
			(IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

	Connection = IrpSp->FileObject->FsContext;
	ServicePoint = &Connection->ServicePoint;

	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	ASSERT(ServicePoint->Address);
	ACQUIRE_SPIN_LOCK (&ServicePoint->Address->SpinLock, &oldIrql);
	ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
	ServicePoint->ListenIrp = NULL;
	RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
	RELEASE_SPIN_LOCK(&ServicePoint->Address->SpinLock, oldIrql);

	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
}

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


static NTSTATUS
TransmitDataPacket(
   IN	PDEVICE_CONTEXT	DeviceContext,
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN USHORT		    UserDataLength,
	IN BOOL				MarkOpc
	);


static PNDIS_PACKET
PrepareNonDataPacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PACKET_TYPE		PacketType
	);

static NTSTATUS
TransmitNonDataPacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PACKET_TYPE		PacketType,
	IN PNDIS_PACKET		Packet
	);

static VOID
SmpDoReceiveRequest(
	IN PSERVICE_POINT ServicePoint,
	IN PNDIS_PACKET	Packet
	);

static BOOLEAN
SmpDoReceive(
	IN PSERVICE_POINT ServicePoint,
	IN PNDIS_PACKET	Packet
	);


static VOID
SmpWorkDpcRoutine(
			       IN    PKDPC    dpc,
			       IN    PVOID    Context,
			       IN    PVOID    junk1,
			       IN    PVOID    junk2
			       );

static NTSTATUS
SmpReadPacket(
	IN	 PSERVICE_POINT	ServicePoint
	);


BOOLEAN LpxStateDoReceiveWhenListen(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenSynSent(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenSynRecv(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenEstablished(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenLastAck(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenFinWait1(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenFinWait2(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);
BOOLEAN LpxStateDoReceiveWhenClosing(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
);


#if DBG
static VOID
SmpPrintState(
	IN	LONG	        Debuglevel,
	IN	PCHAR	        Where,
	IN	PSERVICE_POINT	ServicePoint
	)
{
#if !DBG
	UNREFERENCED_PARAMETER(Debuglevel);
	UNREFERENCED_PARAMETER(Where);
	UNREFERENCED_PARAMETER(ServicePoint);
#endif

	DebugPrint(Debuglevel, (Where));
	DebugPrint(Debuglevel, (" : SP %p, Seq 0x%x, RSeq 0x%x, RAck 0x%x", 
		ServicePoint, SHORT_SEQNUM(ServicePoint->Sequence), SHORT_SEQNUM(ServicePoint->RemoteSequence),
		SHORT_SEQNUM(ServicePoint->RemoteAck)));

	DebugPrint(Debuglevel, (" LRetransSeq 0x%x, TimerR 0x%x, #Pac %ld", 
		SHORT_SEQNUM(ServicePoint->LastRetransmitSequence), ServicePoint->TimerReason, 
		NumberOfPackets));

	DebugPrint(Debuglevel, (" #Sent %ld, #SentCom %ld, CT %I64d  RT %I64d\n", 
			    NumberOfSent, NumberOfSentComplete, CurrentTime().QuadPart, ServicePoint->RetransmitTimeOut.QuadPart));
}
#else
#define SmpPrintState(l, w, s) 
#endif




//
//	acquire SpinLock of DeviceContext before calling
//	comment by hootch 09042003
//
//	called only from LpxOpenAddress()
//
NTSTATUS
LpxAssignPort(
	IN PDEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	)
{
	BOOLEAN		        notUsedPortFound;
	PLIST_ENTRY		    listHead;
	PLIST_ENTRY		    thisEntry;
	PTP_ADDRESS		    address;
	USHORT		        portNum;
	NTSTATUS		    status;
//	ASSERT( LpxControlDeviceContext != AddressDeviceContext);
	DebugPrint(2, ("Smp LPX_BIND %02x:%02x:%02x:%02x:%02x:%02x SourceAddress->Port = %x\n", 
		SourceAddress->Node[0],SourceAddress->Node[1],SourceAddress->Node[2],
		SourceAddress->Node[3],SourceAddress->Node[4],SourceAddress->Node[5],
		SourceAddress->Port));

	portNum = AddressDeviceContext->LastPortNum;
	listHead = &AddressDeviceContext->AddressDatabase;

	notUsedPortFound = FALSE;

	do {
		BOOLEAN	usedPort;

		portNum++;
		if(portNum == 0)
			portNum = LPX_PORTASSIGN_BEGIN;

		usedPort = FALSE;

		for(thisEntry = AddressDeviceContext->AddressDatabase.Flink;
			thisEntry != listHead;
			thisEntry = thisEntry->Flink) 
		{
			address = CONTAINING_RECORD (thisEntry, TP_ADDRESS, Linkage);

			if (address->NetworkName != NULL) {
			    if (address->NetworkName->Port == HTONS(portNum)) {
			        usedPort = TRUE;
			        break;
			    }
			}
		}
		if(usedPort == FALSE)
			notUsedPortFound = TRUE;

		//
		//	Do not allow to assign reserved ports due to historical reason.
		//

		if(portNum == RESERVEDPORT_NDASPNP_BCAST){
			usedPort = TRUE;
			notUsedPortFound = FALSE;
		}

	} while(notUsedPortFound == FALSE && portNum != AddressDeviceContext->LastPortNum);
	
	if(notUsedPortFound	== FALSE) {
		DebugPrint(2, ("[Lpx] LpxAssignPort: couldn't find available port number\n") );
		status = STATUS_UNSUCCESSFUL;
		goto ErrorOut;
	}
	SourceAddress->Port = HTONS(portNum);
	AddressDeviceContext->LastPortNum = portNum;
	DebugPrint(2, ("Smp LPX_BIND portNum = %x\n", portNum));

	status = STATUS_SUCCESS;

ErrorOut:
	return status;
}


//
//	acquire SpinLock of DeviceContext before calling
//	comment by hootch 09042003
//
//
//	called only from LpxOpenAddress()
//
PTP_ADDRESS
LpxLookupAddress(
	IN PDEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	)

/*++

Routine Description:

	This routine scans the transport addresses defined for the given
	device context and compares them with the specified NETWORK
	NAME values.  If an exact match is found, then a pointer to the
	TP_ADDRESS object is returned, and as a side effect, the reference
	count to the address object is incremented.  If the address is not
	found, then NULL is returned.

	NOTE: This routine must be called with the DeviceContext
	spinlock held.

Arguments:

	DeviceContext - Pointer to the device object and its extension.
	NetworkName - Pointer to an LPX_ADDRESS structure containing the
			        network name.

Return Value:

	Pointer to the TP_ADDRESS object found, or NULL if not found.

--*/

{
	PTP_ADDRESS address = NULL;
	PLIST_ENTRY p;
	ULONG i;

//	ASSERT( LpxControlDeviceContext != DeviceContext) ;
	p = DeviceContext->AddressDatabase.Flink;

	for (p = DeviceContext->AddressDatabase.Flink;
		 p != &DeviceContext->AddressDatabase;
		 p = p->Flink) {

		address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

		if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
			DebugPrint(3,("!!!! Address's STATUS is Alive %p\n", address) );
			continue;
		}

		//
		// If the network name is specified and the network names don't match,
		// then the addresses don't match.
		//

		i = sizeof(LPX_ADDRESS);

		if (address->NetworkName != NULL) {
			if (SourceAddress != NULL) {
			    if (!RtlEqualMemory (
			            address->NetworkName,
			            SourceAddress,
			            i)) {
			        continue;
			    }
			} else {
			    continue;
			}

		} else {
			if (SourceAddress != NULL) {
			    continue;
			}
		}

		//
		// We found the match.  Bump the reference count on the address, and
		// return a pointer to the address object for the caller to use.
		//

		IF_LPXDBG (LPX_DEBUG_ADDRESS) {
			LpxPrint2 ("LpxLookupAddress DC %p: found %p ", DeviceContext, address);
		}

		LpxReferenceAddress ("lookup", address, AREF_LOOKUP);
		return address;

	} /* for */

	//
	// The specified address was not found.
	//

	IF_LPXDBG (LPX_DEBUG_ADDRESS) {
		LpxPrint1 ("LpxLookupAddress DC %p: did not find ", address);
	}

	return NULL;

} /* LpxLookupAddress */


//
//
//	called only from LpxTdiConnect()
//
NTSTATUS 
LpxConnect(
	IN		 PTP_CONNECTION    Connection,
	 IN OUT	PIRP	        Irp
   )
{   
	PSERVICE_POINT	ServicePoint;
	NTSTATUS	 status;
	KIRQL		    oldIrql;
	PNDIS_PACKET Packet;
	KIRQL		cancelIrql;
	BOOLEAN bRet;
	DebugPrint(2, ("LpxConnect\n"));
	ServicePoint = &Connection->ServicePoint;
	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	switch(ServicePoint->SmpState) {
	case SMP_CLOSE: 
		{
			PLPX_ADDRESS	destinationAddress;
			LARGE_INTEGER	TimeInterval = {0,0};
			ASSERT(Connection->ServicePoint.SmpState == SMP_CLOSE);
			destinationAddress = &Connection->CalledAddress;

			RtlCopyMemory(
				&Connection->ServicePoint.DestinationAddress,
				destinationAddress,
				sizeof(LPX_ADDRESS)
				);
				
			DebugPrint(1,("Connecting to %02X%02X%02X%02X%02X%02X:%04X\n",
					Connection->ServicePoint.DestinationAddress.Node[0],
					Connection->ServicePoint.DestinationAddress.Node[1],
					Connection->ServicePoint.DestinationAddress.Node[2],
					Connection->ServicePoint.DestinationAddress.Node[3],
					Connection->ServicePoint.DestinationAddress.Node[4],
					Connection->ServicePoint.DestinationAddress.Node[5],
					Connection->ServicePoint.DestinationAddress.Port));

			IoMarkIrpPending(Irp);
			Connection->ServicePoint.ConnectIrp = Irp;
			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(Irp, LpxCancelConnect);
			IoReleaseCancelSpinLock(cancelIrql);
			LpxChangeState(Connection, SMP_SYN_SENT, TRUE);

			Packet = PrepareNonDataPacket(&Connection->ServicePoint, CONREQ);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			status = TransmitNonDataPacket(&Connection->ServicePoint, CONREQ, Packet);
			if(!NT_SUCCESS(status)) {
				DebugPrint(1, ("[LPX] LPX_CONNECT ERROR\n"));
				Connection->ServicePoint.ConnectIrp = NULL;
				Irp->IoStatus.Status = status;
				IoAcquireCancelSpinLock(&cancelIrql);
				IoSetCancelRoutine(Irp, NULL);
				IoReleaseCancelSpinLock(cancelIrql);
				DebugPrint(1, ("[LPX]LpxConnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		//		IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
			} else {
				//
				//	set connection time-out
				//
				ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
				
				ACQUIRE_DPC_SPIN_LOCK(&Connection->ServicePoint.TimeCounterSpinLock);
				Connection->ServicePoint.ConnectTimeOut.QuadPart = CurrentTime().QuadPart + LpxConnectionTimeout;
				Connection->ServicePoint.SmpTimerExpire.QuadPart = CurrentTime().QuadPart + LpxSmpTimeout;
				RELEASE_DPC_SPIN_LOCK(&Connection->ServicePoint.TimeCounterSpinLock);

				if (ServicePoint->SmpState != SMP_CLOSE) {
					LpxReferenceConnection("Connect", Connection, CREF_TIMERDPC);
					TimeInterval.QuadPart = - LpxSmpTimeout;
					bRet = KeSetTimer(
							&Connection->ServicePoint.SmpTimer,
							*(PTIME)&TimeInterval,
							&Connection->ServicePoint.SmpTimerDpc
							);
					if (bRet ==TRUE) { // Timer is already in system queue. deference myself.
						LpxDereferenceConnection("Connect", Connection, CREF_TIMERDPC);
					}
				}
				RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
				status = STATUS_PENDING; 
			}
			break;
		}
	default: // !SMP_CLOSE
		status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Status = status;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		DebugPrint(1, ("[LPX]LpxConnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		break;
	}
	return status;	
}

//
//
//	called only from LpxTdiListen()
//
NTSTATUS
LpxListen(
	IN		 PTP_CONNECTION    Connection,
	 IN OUT	PIRP	        Irp
   )
{   
	PSERVICE_POINT		        ServicePoint;
	NTSTATUS		            status;
	KIRQL		                oldIrql;
	KIRQL				cancelIrql;
	ServicePoint = &Connection->ServicePoint;

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	DebugPrint(1, ("LpxListen ServicePoint = %p, Address=%p(%02x:%02x:%02x:%02x:%02x:%02x:%x)\n", 
		ServicePoint, 
		Connection->ServicePoint.Address, 
		Connection->ServicePoint.Address->NetworkName->Node[0], 
		Connection->ServicePoint.Address->NetworkName->Node[1], 
		Connection->ServicePoint.Address->NetworkName->Node[2], 
		Connection->ServicePoint.Address->NetworkName->Node[3], 
		Connection->ServicePoint.Address->NetworkName->Node[4], 
		Connection->ServicePoint.Address->NetworkName->Node[5],
		Connection->ServicePoint.Address->NetworkName->Port

	));	

	switch(ServicePoint->SmpState) {
	case SMP_CLOSE:
		RtlCopyMemory(
				    &ServicePoint->SourceAddress,
				    ServicePoint->Address->NetworkName,
				    sizeof(LPX_ADDRESS)
				    );
		LpxChangeState(Connection, SMP_LISTEN, TRUE);
		ServicePoint->ListenIrp = Irp;
		IoMarkIrpPending(Irp);
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, LpxCancelListen);
		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		status =  STATUS_PENDING;
		break;
	default: // !SMP_CLOSE
		status = STATUS_UNSUCCESSFUL;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		Irp->IoStatus.Status = status;
		DebugPrint(1, ("[LPX]LpxListen: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		break;
	}
	return status;
}

//
//
//
//	called only from LpxTdiAccept()
//
NDIS_STATUS
LpxAccept(
	IN		 PTP_CONNECTION    Connection,
	 IN OUT	PIRP	        Irp
   )
{   
	PSERVICE_POINT	ServicePoint;
	NDIS_STATUS	 status;
	KIRQL		    cancelIrql;
	KIRQL		    oldIrql;

	ServicePoint = &Connection->ServicePoint;

	DebugPrint(2, ("LpxAccept ServicePoint = %p, State = %s\n", 
		ServicePoint, LpxStateName[ServicePoint->SmpState]));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	switch(ServicePoint->SmpState) {
	case SMP_ESTABLISHED:
		status = STATUS_SUCCESS;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		Irp->IoStatus.Status = status;
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);	
		break;
	default: // !SMP_ESTABLISHED
		status = STATUS_UNSUCCESSFUL;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		Irp->IoStatus.Status = status;
		DebugPrint(1, ("[LPX]LpxAccept: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		break;
	}
	return status;
}

//
//
//	called only from LpxTdiDisconnect()
//
NDIS_STATUS
LpxDisconnect(
	IN		 PTP_CONNECTION    Connection,
	 IN OUT	PIRP	        Irp
	)
{
	PSERVICE_POINT	ServicePoint;
	NTSTATUS	 status;
	KIRQL		oldIrql;
	KIRQL		cancelIrql;
	PNDIS_PACKET Packet;
	ServicePoint = &Connection->ServicePoint;

	DebugPrint(2, ("LpxDisconnect ServicePoint = %p, ServicePoint->State = %s\n", 
		ServicePoint, LpxStateName[ServicePoint->SmpState]));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	switch(ServicePoint->SmpState) {
	case SMP_SYN_SENT: // Disconnect while connecting...
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		//
		// Just close protocol
		//
		LpxCloseServicePoint(ServicePoint);
		status = STATUS_SUCCESS;	
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		Irp->IoStatus.Status = status;
		DebugPrint(1, ("[LPX]LpxDisconnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		break;
	case SMP_SYN_RECV:		// While connected
	case SMP_ESTABLISHED:
	case SMP_CLOSE_WAIT: 
		IoMarkIrpPending(Irp);
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, LpxCancelDisconnect);
		IoReleaseCancelSpinLock(cancelIrql);
		ServicePoint->DisconnectIrp = Irp;

		if(ServicePoint->SmpState == SMP_CLOSE_WAIT)
			LpxChangeState(Connection, SMP_LAST_ACK, TRUE);
		else
			LpxChangeState(Connection, SMP_FIN_WAIT1, TRUE);	    

		ServicePoint->FinSequence = SHORT_SEQNUM(ServicePoint->Sequence);

		Packet = PrepareNonDataPacket(&Connection->ServicePoint, DISCON);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(&Connection->ServicePoint, DISCON, Packet);

		status = STATUS_PENDING;
		break;
	default: // Do nothing. Already disconnected or disconnection is under progress.
		status = STATUS_SUCCESS;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		Irp->IoStatus.Status = status;
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX]LpxDisconnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		break;
	}
	return status;
}

NTSTATUS
LpxSend(
	IN		 PTP_CONNECTION    Connection,
	 IN OUT	PIRP	        Irp
   )
{
	PSERVICE_POINT	ServicePoint;
	NTSTATUS	 status;
	KIRQL		    oldIrql;
	KIRQL		cancelIrql;
	ServicePoint = &Connection->ServicePoint;

	DebugPrint(3, ("LpxSend ServicePoint = %p, ServicePoint->State = %s\n", 
		ServicePoint, LpxStateName[ServicePoint->SmpState]));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	switch(ServicePoint->SmpState) {
	case SMP_ESTABLISHED:
		{
			PIO_STACK_LOCATION		    irpSp;
			PTDI_REQUEST_KERNEL_SEND	parameters;
			PDEVICE_CONTEXT		        deviceContext;
			PUCHAR		                userData;
			ULONG		                userDataLength;
			PNDIS_PACKET		        packet;
			BOOLEAN		                bFailAlloc = FALSE;
			int NrOpc = 0;
			BOOLEAN bMarkOpc = FALSE;
			
			irpSp = IoGetCurrentIrpStackLocation (Irp);
			ASSERT(irpSp);
			parameters = (PTDI_REQUEST_KERNEL_SEND)(&irpSp->Parameters);
			ASSERT(parameters);

			IRP_SEND_IRP(irpSp) = Irp;
			IRP_SEND_REFCOUNT(irpSp) = 1;

			Irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
			Irp->IoStatus.Information = 0;

			LpxReferenceConnection ("LpxSend", Connection, CREF_SEND_IRP);

			deviceContext = (PDEVICE_CONTEXT)ServicePoint->Address->Provider;

			IoMarkIrpPending(Irp);
			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(Irp, LpxCancelSend);
			IoReleaseCancelSpinLock(cancelIrql);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			// 
			// From this point, we must check state is changed to SMP_CLOSE before accessing 
			//		      ServicePpint->SmpTimerDpc, SmpTimer, ReceiveReorderQueue, ReceiveQueue, WriteQueue, RetransmitQueue
			//		                       Address, ConnectIrp, ListenIrp, DisconnectIrp, ReceiveIrpList

			userDataLength = parameters->SendLength;
			if (Irp->MdlAddress != NULL) {
				userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);
			} else {
				ASSERT(0);
			}
			// User supply no input buffer. What is correct behavior for this?
			//		return error for now..
			if (userDataLength == 0 || Irp->MdlAddress==NULL) {
				status = STATUS_SUCCESS; 
				Irp->IoStatus.Status = status;
				Irp->IoStatus.Information = 0;
				IoAcquireCancelSpinLock(&cancelIrql);
				IoSetCancelRoutine(Irp, NULL);
				IoReleaseCancelSpinLock(cancelIrql);
				LpxDereferenceSendIrp ("Complete", irpSp, RREF_CREATION);	 // remove creation reference.
				LpxDereferenceConnectionMacro ("Removing Connection", Connection, CREF_SEND_IRP);  
				DebugPrint(1, ("[LPX] LpxSend: userData is NULL\n"));
				break;
			}

			while (userDataLength) 
			{
				USHORT	        copy;

				if (ServicePoint->SendJumbo && deviceContext->MaxJumboUserData) {
					copy = (USHORT)deviceContext->MaxJumboUserData;
				} else {
					copy = (USHORT)deviceContext->MaxUserData;
				}
				if(copy > userDataLength)
					copy = (USHORT)userDataLength;

				
				status = PacketAllocate(
					ServicePoint,
					ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
					deviceContext,
					SEND_TYPE,
					userData,
					copy,
					irpSp,
					&packet
					);
				
				if(!NT_SUCCESS(status)) {
					bFailAlloc = TRUE;
					DebugPrint(2, ("[LPX]LpxSend: packet == NULL\n"));
					SmpPrintState(2, "[LPX]LpxSend: PacketAlloc", ServicePoint);
					break;
				}
					    
				LpxReferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

				DebugPrint(4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy));
				userDataLength -= copy;

				if (MaxPCont != 0 && deviceContext->MaxJumboUserData) {
					// We've marked MaxPCont, so it's time not to mark OPC
					//	or we should not mark OPC for the last packet
					//  Do not use OPC for jumbo packet because retransmission penalty for jumbo packet is high.
					if (NrOpc >= MaxPCont || userDataLength == 0) {
						NrOpc = 0;
						bMarkOpc = FALSE;
					} else {
						NrOpc++;
						bMarkOpc = TRUE;
					}
				} 

				status = TransmitDataPacket(deviceContext, ServicePoint, packet, copy, bMarkOpc);
				if (status != STATUS_SUCCESS) {
					// SendIrp reference is decreasde by Packetfree in TransmitDataPacket
					break;
				}
				userData += copy;
			}

			DebugPrint(4, ("LpxSend Complete Irp:  Entered IRP %p, connection %p\n",
				Irp, Connection));
			
			if((TRUE == bFailAlloc) 
				&& (parameters->SendLength == userDataLength))
			{
				//
				// Failed alloc packet and no data is sent.
				//
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Irp->IoStatus.Information = 0;
				status = STATUS_INSUFFICIENT_RESOURCES;
				IoAcquireCancelSpinLock(&cancelIrql);
				IoSetCancelRoutine(Irp, NULL);
				IoReleaseCancelSpinLock(cancelIrql);
			} else{
				Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = parameters->SendLength - userDataLength;
				status = STATUS_PENDING;
			}

			LpxDereferenceSendIrp ("Complete", irpSp, RREF_CREATION);	 // remove creation reference.
			LpxDereferenceConnectionMacro ("Removing Connection", Connection, CREF_SEND_IRP);  
	
			break;
		}
	default: // !SMP_ESTABLISHED
		status = STATUS_INVALID_CONNECTION; 
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX] LpxSend: not ESTABLISHED state.(in %s)\n", LpxStateName[ServicePoint->SmpState]));
		break;
	}
	return status;
}


NTSTATUS
LpxRecv(
		IN	     PTP_CONNECTION    Connection,
		IN OUT	PIRP            Irp
		)
{
	PSERVICE_POINT	ServicePoint;
	NTSTATUS	 status;
	KIRQL		    oldIrql;
	KIRQL		cancelIrql;
	BOOLEAN bRet;

	ServicePoint = &Connection->ServicePoint;
	DebugPrint(3, ("LpxSend ServicePoint = %p, ServicePoint->State = %s\n", 
		ServicePoint, LpxStateName[ServicePoint->SmpState]));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	switch(ServicePoint->SmpState) {
	case SMP_ESTABLISHED:
	case SMP_CLOSE_WAIT:
		{
			PIO_STACK_LOCATION	irpSp;
			ULONG		        userDataLength;
			PUCHAR		        userData = NULL;
			LONG	cnt;
			BOOLEAN	            raised = FALSE;

			if(ServicePoint->Shutdown & SMP_RECEIVE_SHUTDOWN)
			{
				status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
				goto ErrorOut;
			}

			RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);

			// From this point, we must check state is changed to SMP_CLOSE before accessing 
			//		      ServicePpint->SmpTimerDpc, SmpTimer, ReceiveReorderQueue, ReceiveQueue, WriteQueue, RetransmitQueue
			//		                       Address, ConnectIrp, ListenIrp, DisconnectIrp, ReceiveIrpList

			irpSp = IoGetCurrentIrpStackLocation(Irp);
			userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 

			if(userDataLength == 0){
				DebugPrint(0, ("LPX_RECEIVE fail for Invalid Address\n"));
				Irp->IoStatus.Information = 0;
				status = STATUS_UNSUCCESSFUL;
				ASSERT(0);
				goto ErrorOut;
			}

			if(Irp->MdlAddress == NULL ){
				DebugPrint(0, ("LPX_RECEIVE fail for Invalid Address\n"));
				Irp->IoStatus.Information = 0;
				status = STATUS_UNSUCCESSFUL;
				ASSERT(0);
				goto ErrorOut;
			}

			userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);

			if(userData == NULL ){
				DebugPrint(0, ("LPX_RECEIVE fail for Invalid Address\n"));
				Irp->IoStatus.Information = 0;
				status = STATUS_UNSUCCESSFUL;
				ASSERT(0);
				goto ErrorOut;
			}
			
			IoMarkIrpPending(Irp);
			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(Irp, LpxCancelRecv);
			IoReleaseCancelSpinLock(cancelIrql);

			Irp->IoStatus.Information = 0;

			// Insert irp to ReceiveIrpList and let SmpWorkDpc handle.

			ACQUIRE_SPIN_LOCK (&ServicePoint->SpSpinLock, &oldIrql);

			if (ServicePoint->SmpState == SMP_CLOSE) { // Status may have changed.
				status =  STATUS_REQUEST_ABORTED;
				RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
				goto ErrorOut;
			} else {
				ServicePoint->LastRecvDataArrived = CurrentTime(); // Assume packet is lost if response is not in timeout. 
				ExInterlockedInsertTailList(
				    &ServicePoint->ReceiveIrpList,
				    &Irp->Tail.Overlay.ListEntry,
				    &ServicePoint->ReceiveIrpQSpinLock
				);
				RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
				cnt = InterlockedIncrement(&ServicePoint->RequestCnt);
				if( cnt == 1 ) {
				    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
				        oldIrql = KeRaiseIrqlToDpcLevel();
				        raised = TRUE;
				    }
				    LpxReferenceConnection("LpxRecv", Connection, CREF_WORKDPC);
				    ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
				    bRet = KeInsertQueueDpc(&ServicePoint->SmpWorkDpc, NULL, NULL);
				    RELEASE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
				    if (!bRet) { // DPC is already in queue. Dereference myself.
					    LpxDereferenceConnection("LpxRecv", Connection, CREF_WORKDPC);
				    }
				    if(raised == TRUE)
				        KeLowerIrql(oldIrql);
				}
			}
			status = STATUS_PENDING;
			break;
		ErrorOut:
			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(Irp, NULL);
			IoReleaseCancelSpinLock(cancelIrql);
			DebugPrint(1, ("[LPX]LpxRecv: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
			break;
		}
	default: // !(SMP_ESTABLISHED|SMP_CLOSE_WAIT)
		status = STATUS_UNSUCCESSFUL;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(Irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
	//	Irp->IoStatus.Status = status;
		RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX]LpxRecv: Not established or close_wait status. IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status));
		break;
	}
	return status;
}


NDIS_STATUS
LpxReceiveIndicate (
	IN NDIS_HANDLE	ProtocolBindingContext,
	IN NDIS_HANDLE	MacReceiveContext,
	IN PVOID		HeaderBuffer,
	IN UINT		    HeaderBufferSize,
	IN PVOID		LookAheadBuffer,
	IN UINT		    LookAheadBufferSize,
	IN UINT		    PacketSize
	)
/*++

Routine Description:

	This routine receives control from the physical provider as an
	indication that a frame has been received on the physical link.
	This routine is time critical, so we only allocate a
	buffer and copy the packet into it. We also perform minimal
	validation on this packet. It gets queued to the device context
	to allow for processing later.

Arguments:

	BindingContext - The Adapter Binding specified at initialization time.

	ReceiveContext - A magic cookie for the MAC.

	HeaderBuffer - pointer to a buffer containing the packet header.

	HeaderBufferSize - the size of the header.

	LookaheadBuffer - pointer to a buffer containing the negotiated minimum
		amount of buffer I get to look at (not including header).

	LookaheadBufferSize - the size of the above. May be less than asked
		for, if that's all there is.

	PacketSize - Overall size of the packet (not including header).

Return Value:

	NDIS_STATUS - status of operation, one of:

			     NDIS_STATUS_SUCCESS if packet accepted,
			     NDIS_STATUS_NOT_RECOGNIZED if not recognized by protocol,
			     NDIS_any_other_thing if I understand, but can't handle.

--*/
{
	PDEVICE_CONTEXT		deviceContext;
	USHORT		        protocol;
	PNDIS_PACKET		packet;
	PNDIS_BUFFER		firstBuffer;    
	PUCHAR		        packetData;
	NDIS_STATUS		 status;
	UINT		        bytesTransfered = 0;
	UINT		        startOffset = 0;

	DebugPrint(4, ("LpxReceiveIndicate, Entered\n"));
	
	deviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;
	//
	//	validation
	//
	if (HeaderBufferSize != ETHERNET_HEADER_LENGTH) {
		DebugPrint(4, ("HeaderBufferSize = %x\n", HeaderBufferSize));
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
	
	RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)HeaderBuffer)[12], sizeof(USHORT));

	//
	//	Discard 802.2 LLC SNAP field.
	//
	// if Ether Type less than 0x0600 ( 1536 )
	//
	if(  NTOHS(protocol) < 0x0600 &&
		( protocol != HTONS(0x0060) && // LOOP: Ethernet Loopback
			protocol != HTONS(0x0200) && // PUP : Xerox PUP packet
			    protocol != HTONS(0x0201)  // PUPAP: Xerox PUP address trans packet
			    )) {
		NdisCopyLookaheadData(
					(PUCHAR)&protocol,
					&((PUCHAR)LookAheadBuffer)[LENGTH_8022LLCSNAP - 2],
					sizeof(USHORT),
					deviceContext->MacOptions);
		PacketSize -= LENGTH_8022LLCSNAP;
		LookAheadBufferSize -= LENGTH_8022LLCSNAP;
		startOffset = LENGTH_8022LLCSNAP;
	}

#if 0
	if (protocol == HTONS(0x8100)) {
		DebugPrint(1, ("Tagged packet\n"));
		DbgBreakPoint();
/*		RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)LookAheadBuffer)[18], sizeof(USHORT));
		PacketSize -= 6;
		LookAheadBufferSize -= LENGTH_8022LLCSNAP;
		startOffset = LENGTH_8022LLCSNAP;

		RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)HeaderBuffer)[12], sizeof(USHORT));*/
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
#endif
	if(protocol != HTONS(ETH_P_LPX)) {
		DebugPrint(4, ("Type = %x\n", protocol));

		return NDIS_STATUS_NOT_RECOGNIZED;
	}


	//
	//	Check to see if the device context is initialized.
	//

	ACQUIRE_DPC_SPIN_LOCK(&deviceContext->SpinLock);
	if(deviceContext->bDeviceInit == FALSE) {
		RELEASE_DPC_SPIN_LOCK(&deviceContext->SpinLock);
		DebugPrint(4,("Device is not initialized. Drop packet\n"));
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
	RELEASE_DPC_SPIN_LOCK(&deviceContext->SpinLock);

	//
	// DROP PACKET for DEBUGGING!!!!
	//
#if 1
	if (ulRxPacketDropRate) {
		LONG DropCount;
		DropCount = InterlockedIncrement(&ulRxPacketCountForDrop);	
		if(DropCount % 1000 < (LONG)ulRxPacketDropRate ) {
			PLPX_HEADER2        lpxHeader = (PLPX_HEADER2)LookAheadBuffer;
#if 0
			DebugPrint(1, ("[Drop(%x,%x,%x))]\n", 
			    NTOHS(lpxHeader->Lsctl), NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)
			));
#else
			DebugPrint(1, ("D"));
#endif
			return NDIS_STATUS_NOT_RECOGNIZED;
		}
	}
#endif

	DebugPrint(4, ("LpxReceiveIndicate, PacketSize = %d, LookAheadBufferSize = %d, LPX_HEADER2 size = %d\n",
		PacketSize, LookAheadBufferSize, sizeof(LPX_HEADER2)));

	status = PacketAllocate(
		NULL,
		PacketSize,
		deviceContext,
		RECEIVE_TYPE,
		NULL,
		0,
		NULL,
		&packet
		);

	if(status != NDIS_STATUS_SUCCESS) {
		return NDIS_STATUS_NOT_RECOGNIZED;
	}

	ASSERT(packet->Private.NdisPacketFlags & fPACKET_ALLOCATED_BY_NDIS);
	RtlCopyMemory(
			&RESERVED(packet)->EthernetHeader,
			HeaderBuffer,
			ETHERNET_HEADER_LENGTH
			);
	
	RESERVED(packet)->EthernetHeader.Type = protocol;
	RESERVED(packet)->RecvTime = CurrentTime();
		
	if(PacketSize == LookAheadBufferSize) {
		NdisQueryPacket(
			packet,
			NULL,
			NULL,
			&firstBuffer,
			NULL
			);
	
		   packetData = MmGetMdlVirtualAddress(firstBuffer);
		NdisCopyLookaheadData(
			        packetData,
			        (PBYTE)LookAheadBuffer + startOffset,
			        LookAheadBufferSize,
					deviceContext->MacOptions
			        );
		LpxTransferDataComplete(
			                    deviceContext,
			                    packet,
			                    NDIS_STATUS_SUCCESS,
			                    LookAheadBufferSize
			                    );
	} else {
		ACQUIRE_DPC_SPIN_LOCK(&deviceContext->SpinLock);
		if (deviceContext->NdisBindingHandle) {
			NdisTransferData(
			    &status,
			    deviceContext->NdisBindingHandle,
			    MacReceiveContext,
			    0,
			    PacketSize,
			    packet,
			    &bytesTransfered
			    );
			RELEASE_DPC_SPIN_LOCK(&deviceContext->SpinLock);
			if (status == NDIS_STATUS_PENDING) {
			    return NDIS_STATUS_SUCCESS;
			} else if(status != NDIS_STATUS_SUCCESS) {
				DebugPrint(1, ("NdisTransferData() failed. STATUS=%08lx\n", status));
			}
		} else {
			RELEASE_DPC_SPIN_LOCK(&deviceContext->SpinLock);
			status = STATUS_INVALID_DEVICE_STATE;
			DebugPrint(1, ("Invalid device status. STATUS=%08lx\n", status));
		}

		LpxTransferDataComplete(
			                deviceContext,
			                packet,
			                status,
			                bytesTransfered);

	}

	return NDIS_STATUS_SUCCESS;
}

//
//	queues a received packet to InProgressPacketList
//
//	called from LpxReceiveIndicate() and NDIS
//
VOID
LpxTransferDataComplete(
			            IN NDIS_HANDLE   ProtocolBindingContext,
			            IN PNDIS_PACKET  Packet,
			            IN NDIS_STATUS   Status,
			            IN UINT          BytesTransfered
			            )
{
	PDEVICE_CONTEXT		pDeviceContext;
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;	
	PUCHAR				bufferData;
	UINT				bufferLength;
	UINT				totalCopied;
	UINT				copied;
	UINT				bufferNumber;
	NDIS_STATUS			status;
	PNDIS_PACKET		pNewPacket;
	USHORT				usPacketSize;
	UINT				uiBufferSize;

	DebugPrint(3, ("[Lpx]LpxTransferDataComplete: Entered\n"));

	UNREFERENCED_PARAMETER(BytesTransfered);
	
	pDeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;
	if(Status != NDIS_STATUS_SUCCESS){
		DebugPrint(1,  ("[LPX] LpxTransferDataComplete error %x\n", Status));
		goto TossPacket;
	}

	if(LpxAllocatePacketMemory(pDeviceContext, &lpxHeader, sizeof(LPX_HEADER2)) != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("No memory\n"));
		goto TossPacket;
	}
	// End Mod.

	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);

	NdisQueryBufferSafe(
		firstBuffer,
		&bufferData,
		&bufferLength,
		HighPagePriority
		);

	DebugPrint(4, ("bufferLength = %d\n", bufferLength));
	totalCopied = 0;
	copied = bufferLength < sizeof(LPX_HEADER2) ? bufferLength : sizeof(LPX_HEADER2);
	bufferNumber = 0;
	
	while(firstBuffer) {
		PNDIS_BUFFER	nextBuffer;
		
		if(copied)
			RtlCopyMemory(
			    &((PUCHAR)lpxHeader)[totalCopied],
			    bufferData,
			    copied
			    );
		totalCopied += copied;
		if(totalCopied == sizeof(LPX_HEADER2)) {
			break;
		}
		
		NdisGetNextBuffer(firstBuffer, &nextBuffer);
		firstBuffer = nextBuffer;
		if(!firstBuffer)
			break;

		NdisQueryBufferSafe(
			firstBuffer,
			&bufferData,
			&bufferLength,
			HighPagePriority
			);
		
		copied = bufferLength < (sizeof(LPX_HEADER2) - totalCopied) ? bufferLength 
			: (sizeof(LPX_HEADER2) - totalCopied);
		bufferNumber ++;
	}
	
	if(((NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))) < sizeof(LPX_HEADER2))
		goto TossPacket;
	
	//
	// Remove Padding.
	//
	
	// Get Real Packet Length.
	usPacketSize = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK));
	
	// Get Buffer Length.
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		NULL,
		&uiBufferSize
		);
	
	if(usPacketSize < uiBufferSize) {
		UINT  BytesCopied;

		// Create New Packet.
		status = PacketAllocate(
			NULL,
			usPacketSize,
			pDeviceContext,
			RECEIVE_TYPE,
			NULL,
			0,
			NULL,
			&pNewPacket
			);
		
		if(status != NDIS_STATUS_SUCCESS) {
			DebugPrint(1, ("LpxTransferDataComplete: Can't Make New Packet.\n"));
			goto TossPacket;
		}

		NdisCopyFromPacketToPacket(
			pNewPacket,
			0,
			usPacketSize,
			Packet,
			0,
			&BytesCopied
			);    
		
		if(usPacketSize != BytesCopied) {
			DebugPrint(1, ("LpxTransferDataComplete: Can't Copy Packets.\n"));
			
			PacketFree(pNewPacket);
			goto TossPacket;
		}

		RtlCopyMemory(
			&RESERVED(pNewPacket)->EthernetHeader,
			&RESERVED(Packet)->EthernetHeader,
			ETHERNET_HEADER_LENGTH
			);

		PacketFree(Packet);
	} else {
		pNewPacket = Packet;
	}

	RESERVED(pNewPacket)->LpxSmpHeader = lpxHeader;
	
	//
	// Append received packet to InProgress Packet list.
	//	
	ExInterlockedInsertTailList(
		&pDeviceContext->PacketInProgressList,
		&(RESERVED(pNewPacket)->ListElement),
		&pDeviceContext->PacketInProgressQSpinLock
		);
	
	return;

TossPacket:

	PacketFree(Packet);
	return;
}


//
//	route packets to each connection.
//	SmpDoReceive() does actual works for the Stream-like packets.
//
VOID
LpxReceiveComplete(
			        IN NDIS_HANDLE BindingContext
			        )
/*++

Routine Description:

	This routine receives control from the physical provider as an
	indication that a connection(less) frame has been received on the
	physical link.  We dispatch to the correct packet handler here.

Arguments:

	BindingContext - The Adapter Binding specified at initialization time.
			         LPX uses the DeviceContext for this parameter.

Return Value:

	None

--*/
{
	PLIST_ENTRY		    pListEntry;
	PNDIS_PACKET		Packet;
	PNDIS_BUFFER		firstBuffer;
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;

	UINT		        bufferLength;
	UINT		        copied;
	PDEVICE_CONTEXT	 deviceContext = (PDEVICE_CONTEXT)BindingContext;
	PTP_ADDRESS		    address;

	PLIST_ENTRY		    Flink;
	PLIST_ENTRY		    listHead;
	PLIST_ENTRY		    thisEntry;

	PUCHAR		        packetData = NULL;
	PLIST_ENTRY		    p;
	PTP_ADDRESS_FILE	addressFile;
	KIRQL		        irql;
	KIRQL			cancelIrql;
	PSERVICE_POINT		connectionServicePoint;
	PSERVICE_POINT		listenServicePoint;

#ifdef __VERSION_CONTROL__
	PSERVICE_POINT		connectingServicePoint;
#endif

	BOOLEAN		        refAddress = FALSE;
	BOOLEAN		        refAddressFile = FALSE;
	BOOLEAN		        refConnection = FALSE;
	const UCHAR		   BroadcastAddr[ETHERNET_ADDRESS_LENGTH]  = {0xff, 0xff,0xff,0xff,0xff,0xff};

	DebugPrint(4, ("[Lpx]LpxReceiveComplete: Entered %d\n", KeGetCurrentIrql()));
	
	//
	// Process In Progress Packets.
	//
	while((pListEntry = ExInterlockedRemoveHeadList(
		&deviceContext->PacketInProgressList, 
		&deviceContext->PacketInProgressQSpinLock)) != NULL) 
	{	   
		reserved = CONTAINING_RECORD(pListEntry, LPX_RESERVED, ListElement);
		Packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		packetData = NULL;
		
		lpxHeader = RESERVED(Packet)->LpxSmpHeader;
#if 0
		if (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ) {
			DebugPrint(1, ("*** Recevied %s from %02X%02X%02X%02X%02X%02X:%04X\n",
			   (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)?"CONNREQ|ACK":"CONNREQ",
			    RESERVED(Packet)->EthernetHeader.SourceAddress[0],
			    RESERVED(Packet)->EthernetHeader.SourceAddress[1],
			    RESERVED(Packet)->EthernetHeader.SourceAddress[2],
			    RESERVED(Packet)->EthernetHeader.SourceAddress[3],
			    RESERVED(Packet)->EthernetHeader.SourceAddress[4],
			    RESERVED(Packet)->EthernetHeader.SourceAddress[5],
			    lpxHeader->SourcePort
			));
		}
#endif		
		DebugPrint(4,("From %02X%02X%02X%02X%02X%02X:%04X\n",
			RESERVED(Packet)->EthernetHeader.SourceAddress[0],
			RESERVED(Packet)->EthernetHeader.SourceAddress[1],
			RESERVED(Packet)->EthernetHeader.SourceAddress[2],
			RESERVED(Packet)->EthernetHeader.SourceAddress[3],
			RESERVED(Packet)->EthernetHeader.SourceAddress[4],
			RESERVED(Packet)->EthernetHeader.SourceAddress[5],
			lpxHeader->SourcePort));
		
		DebugPrint(4,("To %02X%02X%02X%02X%02X%02X:%04X\n",
			RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
			lpxHeader->DestinationPort));

		address = NULL;

		//
		//	match destination Address
		//

		// lock was missing @hootch@ 0825
		ACQUIRE_SPIN_LOCK (&deviceContext->SpinLock, &irql);

		for (Flink = deviceContext->AddressDatabase.Flink;
			Flink != &deviceContext->AddressDatabase;
			Flink = Flink->Flink, address = NULL) {
			
			address = CONTAINING_RECORD (
			    Flink,
			    TP_ADDRESS,
			    Linkage);
			
			if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
			    continue;
			}
			
			if (address->NetworkName == NULL) {
			    continue;
			}

			if(address->NetworkName->Port == lpxHeader->DestinationPort) {
			    // Broadcast?
			    if (RtlCompareMemory(
			            RESERVED(Packet)->EthernetHeader.DestinationAddress,
			            BroadcastAddr, ETHERNET_ADDRESS_LENGTH) == ETHERNET_ADDRESS_LENGTH) {
			        LpxReferenceAddress("ReceiveCompletion", address, AREF_REQUEST);
			        refAddress = TRUE;
			        break;
			    }
			    if(!memcmp(&address->NetworkName->Node, RESERVED(Packet)->EthernetHeader.DestinationAddress, ETHERNET_ADDRESS_LENGTH)) {
			        //
			        // added by hootch
			        LpxReferenceAddress("ReceiveCompletion", address, AREF_REQUEST);
			        refAddress = TRUE;
			        break;
			    }
			}
		}
		// lock was missing @hootch@ 0825
		RELEASE_SPIN_LOCK (&deviceContext->SpinLock, irql);

		if(address == NULL) {
			// No matching address. 
#if DBG
			if(!(RtlCompareMemory(
			            RESERVED(Packet)->EthernetHeader.DestinationAddress,
			            BroadcastAddr, ETHERNET_ADDRESS_LENGTH) == ETHERNET_ADDRESS_LENGTH))
			{
			DebugPrint(1, ("No End Point. To %02X%02X%02X%02X%02X%02X:%04X\n",
			    RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
			    RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
			    RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
			    RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
			    RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
			    RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
			    lpxHeader->DestinationPort));
			}
#endif
			goto TossPacket;
		}
		
		//
		//	if it is a stream-like(called LPX-Stream) packet,
		//	match destination Connection ( called service point in LPX )
		//
		if(lpxHeader->LpxType == LPX_TYPE_STREAM) 
		{
			connectionServicePoint = NULL;
			listenServicePoint = NULL;
#ifdef __VERSION_CONTROL__
			connectingServicePoint = NULL;
#endif

			// lock was missing @hootch@ 0825
			ACQUIRE_SPIN_LOCK (&address->SpinLock, &irql);
			
			listHead = &address->ConnectionServicePointList;

			for(thisEntry = listHead->Flink;
			    thisEntry != listHead;
			    thisEntry = thisEntry->Flink, connectionServicePoint = NULL)
			{
			    UCHAR    zeroNode[6] = {0, 0, 0, 0, 0, 0};

			    connectionServicePoint = CONTAINING_RECORD(thisEntry, SERVICE_POINT, ServicePointListEntry);
			    DebugPrint(4,("connectionServicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
			        connectionServicePoint->DestinationAddress.Node[0],
			        connectionServicePoint->DestinationAddress.Node[1],
			        connectionServicePoint->DestinationAddress.Node[2],
			        connectionServicePoint->DestinationAddress.Node[3],
			        connectionServicePoint->DestinationAddress.Node[4],
			        connectionServicePoint->DestinationAddress.Node[5],
			        connectionServicePoint->DestinationAddress.Port));

			    if(connectionServicePoint->SmpState != SMP_CLOSE
			        && (!memcmp(connectionServicePoint->DestinationAddress.Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH)
			        && connectionServicePoint->DestinationAddress.Port == lpxHeader->SourcePort)) 
			    {
			        //
			        //    reference Connection
			        //    hootch    09042003
			        LpxReferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST);
			        refConnection = TRUE;

			        DebugPrint(4, ("[LPX] connectionServicePoint = %p found!\n", connectionServicePoint));

			        break;
			    }

			    if(connectionServicePoint->SmpState == SMP_LISTEN 
			        && !memcmp(&connectionServicePoint->DestinationAddress.Node, zeroNode , 6)
			        && connectionServicePoint->ListenIrp != NULL)
			    {

			        listenServicePoint = connectionServicePoint;
			        DebugPrint(1, ("listenServicePoint = %p found!\n", listenServicePoint));
			    }

#ifdef __VERSION_CONTROL__
			    if(connectionServicePoint->SmpState == SMP_SYN_SENT 
			        && !memcmp(&connectionServicePoint->DestinationAddress.Node,  RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH)
			        && (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ))  
			    {

			        connectingServicePoint = connectionServicePoint;
			        DebugPrint(1, ("connectingServicePoint = %p\n", connectingServicePoint));
			    }
#endif
			    connectionServicePoint = NULL;
			}
			
			if(connectionServicePoint == NULL) {
			    if(listenServicePoint != NULL) {
			        connectionServicePoint = listenServicePoint;
			        //
			        //    add one reference count
			        //    hootch    09042003
			        LpxReferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST);
			        refConnection = TRUE;
			    } else {
#ifdef __VERSION_CONTROL__
			        if(connectingServicePoint != NULL) {
			            memcpy(connectingServicePoint->DestinationAddress.Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH);
			            connectingServicePoint->DestinationAddress.Port = lpxHeader->SourcePort;
			            connectionServicePoint = connectingServicePoint;
			            //
			            //    add one reference count
			            //    hootch    09042003
			            LpxReferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST);
			            refConnection = TRUE;
			        } else {
#endif
			            RELEASE_SPIN_LOCK (&address->SpinLock, irql);

			            goto TossPacket;
			        }
			    }
			}
			// lock was missing @hootch@ 0825
			RELEASE_SPIN_LOCK (&address->SpinLock, irql);

			RESERVED(Packet)->PacketDataOffset = sizeof(LPX_HEADER2);
			InterlockedExchange(&RESERVED(Packet)->ReorderCount, 0);

			//
			//    call stream-like(called LPX-Stream) routine to do more process
			//
			SmpDoReceiveRequest(connectionServicePoint, Packet);

			goto loopEnd;
		} else if(lpxHeader->LpxType == LPX_TYPE_DATAGRAM) {
			//
			//    a datagram packet
			//
			DebugPrint(4, ("[LPX] LpxReceiveComplete: DataGram packet arrived.\n"));

			//
			//    acquire address->SpinLock to traverse AddressFileDatabase
			//
			// lock was missing @hootch@ 0825
			ACQUIRE_DPC_SPIN_LOCK (&address->SpinLock);

			p = address->AddressFileDatabase.Flink;
			while (p != &address->AddressFileDatabase) {
			    addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
			    //
			    //    Address File is protected by the spinlock of Address owning it.
			    //
			    if (addressFile->State != ADDRESSFILE_STATE_OPEN) {
			        p = p->Flink;
			        continue;
			    }
			    DebugPrint(4, ("[LPX] LpxReceiveComplete udp 2\n"));
			    LpxReferenceAddressFile(addressFile);
			    refAddressFile = TRUE;
			    break;
			}

			if(p == &address->AddressFileDatabase) {

			    RELEASE_DPC_SPIN_LOCK (&address->SpinLock);

			    DebugPrint(4, ("[LPX] LpxReceiveComplete: DataGram Packet - No addressFile matched.\n"));
			    goto TossPacket;
			}

			RELEASE_DPC_SPIN_LOCK (&address->SpinLock);

			
			if (addressFile->RegisteredReceiveDatagramHandler) {

			    ULONG                indicateBytesCopied, mdlBytesCopied, bytesToCopy;
			    NTSTATUS            ntStatus;
			    TA_NETBIOS_ADDRESS    sourceName;
			    PIRP                irp;
			    PIO_STACK_LOCATION    irpSp;
//			    UINT                headerLength;
//			    UINT                bufferOffset;
			    UINT                totalCopied;
			    PUCHAR                bufferData;
			    PNDIS_BUFFER        nextBuffer;
			    ULONG                userDataLength;
//			    KIRQL                cancelIrql;

			    userDataLength = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)) - sizeof(LPX_HEADER2);

			    DebugPrint(4, ("[LPX] LpxReceiveComplete: call UserDataGramHandler with a DataGram packet. NTOHS(lpxHeader->PacketSize) = %d, userDataLength = %d\n",
			        NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK), userDataLength));

			    packetData = ExAllocatePoolWithTag(NonPagedPool, userDataLength, LPX_MEM_TAG_DGRAM_DATA);
			    //
			    //    NULL pointer check.
			    //
			    //    added by @hootch@ 0812
			    //
			    if(packetData == NULL) {
			        DebugPrint(0, ("[LPX] LpxReceiveComplete failed to allocate nonpaged pool for packetData\n"));
			        goto TossPacket;
			    }


			    //
			    // Copy User Data of the packet
			    //
			    NdisQueryPacket(
			        Packet,
			        NULL,
			        NULL,
			        &firstBuffer,
			        NULL
			        );

			    totalCopied = 0;
			    bufferData = MmGetMdlVirtualAddress(firstBuffer);
			    bufferLength = NdisBufferLength(firstBuffer);
			    bufferLength -= sizeof(LPX_HEADER2);
			    bufferData += sizeof(LPX_HEADER2);
			    copied = (bufferLength < userDataLength) ? bufferLength : userDataLength;
			    
			    while(firstBuffer) {
			        if(copied)
			            RtlCopyMemory(
			            &packetData[totalCopied],
			            bufferData,
			            copied
			            );
			        
			        totalCopied += copied;
			        if(totalCopied == (USHORT)(NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) - sizeof(LPX_HEADER2)))
			            break;
			        
			        NdisGetNextBuffer(firstBuffer, &nextBuffer);
			        firstBuffer = nextBuffer;
			        if(!firstBuffer)
			            break;
			        
			        bufferData = MmGetMdlVirtualAddress(firstBuffer);
			        bufferLength = NdisBufferLength(firstBuffer);
			        copied = bufferLength < (userDataLength - totalCopied) ? bufferLength 
			            : (userDataLength - totalCopied);
			    }

			    //
			    //    call user-defined ReceiveDatagramHandler with copied data
			    //
			    indicateBytesCopied = 0;

			    sourceName.TAAddressCount = 1;
			    sourceName.Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
			    sourceName.Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
			    memcpy(((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH);
			    ((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Port = lpxHeader->SourcePort;

			    DebugPrint(3, ("[LPX] LPxReceiveComplete: DATAGRAM: SourceAddress=%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
			                                            RESERVED(Packet)->EthernetHeader.SourceAddress[0],
			                                            RESERVED(Packet)->EthernetHeader.SourceAddress[1],
			                                            RESERVED(Packet)->EthernetHeader.SourceAddress[2],
			                                            RESERVED(Packet)->EthernetHeader.SourceAddress[3],
			                                            RESERVED(Packet)->EthernetHeader.SourceAddress[4],
			                                            RESERVED(Packet)->EthernetHeader.SourceAddress[5],
			                                            NTOHS(lpxHeader->SourcePort)
			                                        ));

			    ntStatus = (*addressFile->ReceiveDatagramHandler)(
			        addressFile->ReceiveDatagramHandlerContext,
			        sizeof (TA_NETBIOS_ADDRESS),
			        &sourceName,
			        0,
			        NULL,
			        TDI_RECEIVE_NORMAL,
			        userDataLength,
			        userDataLength,  // available
			        &indicateBytesCopied,
			        packetData,
			        &irp
			        );

			    if (ntStatus == STATUS_SUCCESS) 
			    {
			        //
			        // The client accepted the datagram. We're done.
			        //
			        DebugPrint(4, ("[LPX] LpxReceiveComplete: DATAGRAM: A datagram packet consumed. STATUS_SUCCESS, userDataLength = %d, indicateBytesCopied = %d\n", userDataLength, indicateBytesCopied));
			    } else if (ntStatus == STATUS_DATA_NOT_ACCEPTED) 
			    {
			        //
			        // The client did not accept the datagram and we need to satisfy
			        // a TdiReceiveDatagram, if possible.
			        //
			        DebugPrint(4, ("[LPX] LpxReceiveComplete: DATAGRAM: DataGramHandler didn't accept a datagram packet.\n"));
			        IF_LPXDBG (LPX_DEBUG_DATAGRAMS) {
			            LpxPrint0 ("[LPX] LpxReceiveComplete: DATAGRAM: Picking off a rcv datagram request from this address.\n");
			        }

			        ntStatus = STATUS_MORE_PROCESSING_REQUIRED;
			        
			    } else if (ntStatus == STATUS_MORE_PROCESSING_REQUIRED) 
			    {
			        //
			        // The client returned an IRP that we should queue up to the
			        // address to satisfy the request.
			        //

			        DebugPrint(1, ("[LPX] LpxReceiveComplete: DATAGRAM: STATUS_MORE_PROCESSING_REQUIRED\n"));
			        irp->IoStatus.Status = STATUS_PENDING;  // init status information.
			        irp->IoStatus.Information = 0;
			        irpSp = IoGetCurrentIrpStackLocation (irp); // get current stack loctn.
			        if ((irpSp->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL) 
			            || (irpSp->MinorFunction != TDI_RECEIVE_DATAGRAM)) 
			        {
			            DebugPrint(1, ("[LPX] LpxReceiveComplete: DATAGRAM: Wrong IRP: Maj:%d Min:%d\n",irpSp->MajorFunction, irpSp->MinorFunction) );
			            irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
			            goto TossPacket;
			        }

			        //
			        // Now copy the actual user data.
			        //
			        mdlBytesCopied = 0;
			        bytesToCopy =  userDataLength - indicateBytesCopied;

			        if ((bytesToCopy > 0) && irp->MdlAddress) 
			        {
			            ntStatus = TdiCopyBufferToMdl (
			                packetData,
			                indicateBytesCopied,
			                bytesToCopy,
			                irp->MdlAddress,
			                0,
			                &mdlBytesCopied
			                );
			        } else {
			            ntStatus = STATUS_SUCCESS;
			        }

			        irp->IoStatus.Information = mdlBytesCopied;
			        irp->IoStatus.Status = ntStatus;

			        IoAcquireCancelSpinLock(&cancelIrql);
			        IoSetCancelRoutine(irp, NULL);
			        IoReleaseCancelSpinLock(cancelIrql);

			        DebugPrint(1, ("[LPX]LpxReceiveComplete: DATAGRAM: IRP %p completed with NTSTATUS:%08lx.\n ", irp, ntStatus));
			        IoCompleteRequest (irp, IO_NETWORK_INCREMENT);
			    }
			}

		}

TossPacket:
		if(packetData != NULL)
			ExFreePool(packetData);

		PacketFree(Packet);

loopEnd:
		//
		//	clean up reference count
		//	added by hootch 09042003
		//
		if(refAddress) {
			LpxDereferenceAddress("ReceiveCompletion", address, AREF_REQUEST);
			refAddress = FALSE;
		}
		if(refAddressFile) {
			LpxDereferenceAddressFile (addressFile);
			refAddressFile = FALSE;
		}
		if(refConnection) {
			LpxDereferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST);
			refConnection = FALSE;
		}

		continue;
	}
	return;
}


VOID LpxCallUserReceiveHandler(
	PSERVICE_POINT	ServicePoint
	)
{
//	PNDIS_PACKET	packet;

//
//	Not need packets to call User ReceiveHandler in the context at this time.
//
//	removed by hootch 09052003
//
//	packet = PacketPeek(&ServicePoint->RcvDataQueue, &ServicePoint->ServicePointQSpinLock);
//	DebugPrint(2, ("before ReceiveHandler packet = %p\n", packet));
	
	if(/*packet &&*/ ServicePoint->Connection && ServicePoint->Connection->AddressFile)
	{
		ULONG ReceiveFlags;
		NTSTATUS status;
		PIRP	irp = NULL;
		ULONG indicateBytesTransferred;
	
		ReceiveFlags = TDI_RECEIVE_AT_DISPATCH_LEVEL | TDI_RECEIVE_ENTIRE_MESSAGE | TDI_RECEIVE_NO_RESPONSE_EXP;

		DebugPrint(3, ("before ReceiveHandler ServicePoint->Connection->AddressFile->RegisteredReceiveHandler=%d\n",
		ServicePoint->Connection->AddressFile->RegisteredReceiveHandler));
		status = (*ServicePoint->Connection->AddressFile->ReceiveHandler)(
			            ServicePoint->Connection->AddressFile->ReceiveHandlerContext,
			            ServicePoint->Connection->Context,
			            ReceiveFlags,
			            0,
			            0,             // BytesAvailable
			            &indicateBytesTransferred,
			            NULL,
			            NULL);

		DebugPrint(3, ("status = %x, Irp = %p\n", status, irp));
	}

	return;
}

NDIS_STATUS
LpxSendDatagram(
	IN		 PTP_ADDRESS    Address,
	 IN OUT	PIRP	    Irp
   )
{
	PIO_STACK_LOCATION		    irpSp;
	PTDI_REQUEST_KERNEL_SENDDG	parameters;
	PDEVICE_CONTEXT		        deviceContext;
	NDIS_STATUS		            status;
	PUCHAR		                userData;
	ULONG		                userDataLength;
	PNDIS_PACKET		        packet;
	TRANSPORT_ADDRESS UNALIGNED *transportAddress;
	PLPX_ADDRESS		        remoteAddress;
	KIRQL irql;

	DebugPrint(3, ("LpxSendDatagram\n"));

	irpSp = IoGetCurrentIrpStackLocation (Irp);
	parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);
	deviceContext = (PDEVICE_CONTEXT)Address->Provider;
//		ASSERT( LpxControlDeviceContext != deviceContext) ;
		
	IRP_SEND_IRP(irpSp) = Irp;
	IRP_SEND_REFCOUNT(irpSp) = 1;

	Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	Irp->IoStatus.Information = 0;


	IoMarkIrpPending(Irp);
//	IoSetCancelRoutine(Irp, LpxCancelSend);

	userDataLength = parameters->SendLength;
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);

	transportAddress = (TRANSPORT_ADDRESS UNALIGNED *)parameters->SendDatagramInformation->RemoteAddress;
	remoteAddress = (PLPX_ADDRESS)&transportAddress->Address[0].Address[0];

	while (userDataLength) 
	{
		USHORT	        copy;
		PNDIS_BUFFER	firstBuffer;    
		PUCHAR	        packetData;
		USHORT	        type;
		PLPX_HEADER2	lpxHeader;
		HANDLE	        NDISHandle;

		// Do not check jumbo. No jumbo frame negotiation exists for dgram.
		copy = (USHORT)deviceContext->MaxUserData;
		if(copy > userDataLength)
			copy = (USHORT)userDataLength;

		status = PacketAllocate(
			NULL,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			deviceContext,
			SEND_TYPE,
			userData,
			copy,
			irpSp,
			&packet
			);

		if(!NT_SUCCESS(status)) {
			DebugPrint(0, ("packet == NULL\n"));
//			SmpPrintState(0, "PacketAlloc", ServicePoint);
//			LpxCompleteSendIrp (Irp, STATUS_CANCELLED, 0);
			LpxDereferenceSendIrp ("Packetize", irpSp, RREF_PACKET);
			return STATUS_PENDING; 
		}

		LpxReferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

		DebugPrint(4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy));
		userDataLength -= copy;

//		status = TransmitDataPacket(ServicePoint, packet, copy);

		//
		//	get a NDIS packet
		//
		NdisQueryPacket(
			    packet,
			    NULL,
			    NULL,
			    &firstBuffer,
			    NULL
		);
			
		packetData = MmGetMdlVirtualAddress(firstBuffer);

		//
		//	set destination and source MAC address
		//
		RtlCopyMemory(
			    &packetData[0],
			    remoteAddress->Node,
			    ETHERNET_ADDRESS_LENGTH
			);

		DebugPrint(3,("remoteAddress %02X%02X%02X%02X%02X%02X:%04X\n",
			        remoteAddress->Node[0],
			        remoteAddress->Node[1],
			        remoteAddress->Node[2],
			        remoteAddress->Node[3],
			        remoteAddress->Node[4],
			        remoteAddress->Node[5],
			        NTOHS(remoteAddress->Port)));

		RtlCopyMemory(
			    &packetData[ETHERNET_ADDRESS_LENGTH],
			    Address->NetworkName->Node,
			    ETHERNET_ADDRESS_LENGTH
			    );

		//
		//	set LPX to ethernet type.
		//
		type = HTONS(ETH_P_LPX);
		RtlCopyMemory(
			&packetData[ETHERNET_ADDRESS_LENGTH*2],
			&type, //&ServicePoint->DestinationAddress.Port,
			2
			);

		//
		//	set LPX header for datagram
		//
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

		lpxHeader->PacketSize = HTONS(sizeof(LPX_HEADER2) + copy);
		lpxHeader->LpxType = LPX_TYPE_DATAGRAM;
//			lpxHeader->CheckSum = 0;
		lpxHeader->DestinationPort = remoteAddress->Port;
		lpxHeader->SourcePort = Address->NetworkName->Port;
		lpxHeader->MessageId = HTONS((USHORT)LPX_HOST_DGMSG_ID);
		lpxHeader->MessageLength = HTONS((USHORT)copy);
		lpxHeader->FragmentId = 0;
		lpxHeader->FragmentLength = 0;
		lpxHeader->ResevedU1 = 0;

		//
		//	pass a datagram packet to NDIS.
		//
		//
		//	TODO: more precious lock needed
		//	patched by hootch
		//
		ACQUIRE_SPIN_LOCK(&deviceContext->SpinLock, &irql);
		NDISHandle = deviceContext->NdisBindingHandle;
		if((deviceContext->bDeviceInit == TRUE)  && NDISHandle) {
			RELEASE_SPIN_LOCK(&deviceContext->SpinLock, irql);
			NdisSend(
			        &status,
			        NDISHandle,
			        packet
			    );
		} else {
			RELEASE_SPIN_LOCK(&deviceContext->SpinLock, irql);
			PacketFree(packet);
			Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
			Irp->IoStatus.Information = parameters->SendLength - userDataLength;
//
//		hootch 03092004
//			    we must not complete any IRP when we don't return STATUS_PENDING
//
//
//			    LpxDereferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

			return STATUS_NO_SUCH_DEVICE; 
		}

		if(status != NDIS_STATUS_PENDING) {
			PacketFree(packet);
			status = STATUS_SUCCESS;
		} else
			continue;
		
		userData += copy;
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = parameters->SendLength;
	LpxDereferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

	return STATUS_PENDING; 
}

static VOID
SmpCancelIrps(
	IN PSERVICE_POINT	ServicePoint
	);


static BOOLEAN
SmpSendTest(
	IN PSERVICE_POINT	ServicePoint,
	PNDIS_PACKET	Packet
	);


static void
RoutePacketRequest(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	);

static void
RoutePacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	);


static LONGLONG
CalculateRTT(
	IN PSERVICE_POINT	ServicePoint
	);


static INT
SmpRetransmitCheck(
	IN PSERVICE_POINT	ServicePoint,
	IN LONG		    AckSequence,
	IN PACKET_TYPE	PacketType
	);

//
// Callers of this routine must be running at IRQL PASSIVE_LEVEL
//
// comment by hootch 08262003
//
VOID
LpxInitServicePoint(
	IN OUT	PTP_CONNECTION	Connection
	)
{
	PSERVICE_POINT ServicePoint = &Connection->ServicePoint;
	DebugPrint(2, ("LpxInitServicePoint ServicePoint = %p\n", ServicePoint));

	RtlZeroMemory(
		ServicePoint,
		sizeof(SERVICE_POINT)
		);

	//
	//	init service point
	//
	KeInitializeSpinLock(&ServicePoint->SpSpinLock);

	LpxChangeState(Connection, SMP_CLOSE, TRUE); /* No lock required */
	ServicePoint->Shutdown	= 0;

	InitializeListHead(&ServicePoint->ServicePointListEntry);

	ServicePoint->DisconnectIrp = NULL;
	ServicePoint->ConnectIrp = NULL;
	ServicePoint->ListenIrp = NULL;
	
	InitializeListHead(&ServicePoint->ReceiveIrpList);
	KeInitializeSpinLock(&ServicePoint->ReceiveIrpQSpinLock);

	ServicePoint->Address = NULL;
	ServicePoint->Connection = Connection;
	// 052303 jgahn
	ServicePoint->lDisconnectHandlerCalled = 0;


	//
	// Init ServicePoint->SmpContext 
	//
	
	KeInitializeSpinLock(&ServicePoint->TimeCounterSpinLock);
	ServicePoint->Sequence		= 0;
	ServicePoint->RemoteSequence	= 0;
	ServicePoint->RemoteAck		= 0;
	ServicePoint->ServerTag		= 0;

	ServicePoint->MaxFlights = SMP_MAX_FLIGHT / 2;

	ServicePoint->SendJumbo = FALSE;
	ServicePoint->WaitingSetJumbo = FALSE;
	ServicePoint->PeerWithoutDelayedAck = FALSE;
	ServicePoint->UnsentAckCount = 0;
	
	KeInitializeTimer(&ServicePoint->SmpTimer);
	//
	// Callers of this routine must be running at IRQL PASSIVE_LEVEL
	//
	// comment by hootch
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	KeInitializeDpc(&ServicePoint->SmpTimerDpc, SmpTimerDpcRoutineRequest, ServicePoint);
	KeInitializeDpc(&ServicePoint->SmpWorkDpc, SmpWorkDpcRoutine, ServicePoint);
	KeInitializeSpinLock(&ServicePoint->SmpWorkDpcLock);

	KeInitializeSpinLock(&ServicePoint->ReceiveQSpinLock);
	InitializeListHead(&ServicePoint->ReceiveQueue);
	KeInitializeSpinLock(&ServicePoint->RcvDataQSpinLock);
	InitializeListHead(&ServicePoint->RcvDataQueue);
	KeInitializeSpinLock(&ServicePoint->RetransmitQSpinLock);
	InitializeListHead(&ServicePoint->RetransmitQueue);
	KeInitializeSpinLock(&ServicePoint->WriteQSpinLock);
	InitializeListHead(&ServicePoint->WriteQueue);

#ifndef DISABLE_PACKET_REORDERING
	KeInitializeSpinLock(&ServicePoint->ReceiveReorderQSpinLock);
	InitializeListHead(&ServicePoint->ReceiveReorderQueue);
#endif
	return;
}


BOOLEAN
LpxCloseServicePoint(
	IN	PSERVICE_POINT	ServicePoint
	)
{
	PNDIS_PACKET	packet;
	UINT		    back_log = 0, receive_queue = 0, receivedata_queue = 0,
			        write_queue = 0, retransmit_queue = 0;
	KIRQL		    oldIrql;
	PTP_ADDRESS		address;
	PLIST_ENTRY  listEntry;
	PLPX_RESERVED reserved;
	BOOL bRet;
	ULONG count;
	ACQUIRE_SPIN_LOCK (&ServicePoint->SpSpinLock, &oldIrql);

	if(ServicePoint->SmpState == SMP_CLOSE) { // Already closed
		RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
		return FALSE;
	}

	//
	//	change the state to SMP_CLOSE to stop I/O in this connection
	//
	LpxChangeState(ServicePoint->Connection, SMP_CLOSE, TRUE); 
	ServicePoint->Shutdown	= 0;

	// Do not cancel or remove dpc. It will stop by themselves by checking SMP_CLOSE state.
	bRet = KeCancelTimer(&ServicePoint->SmpTimer);
	if (bRet) { // Timer is in queue. If we canceled it, we need to dereference it.
		LpxDereferenceConnection("Close", ServicePoint->Connection, CREF_TIMERDPC);
	}

	bRet = KeRemoveQueueDpc(&ServicePoint->SmpTimerDpc); 
	if (bRet) { // Timer is in queue. If we canceled it, we need to dereference it.
		LpxDereferenceConnection("Close", ServicePoint->Connection, CREF_TIMERDPC);
	}
	bRet = KeRemoveQueueDpc(&ServicePoint->SmpWorkDpc);
	if (bRet) {
		LpxDereferenceConnection("Close", ServicePoint->Connection, CREF_WORKDPC);
	}	
	address = ServicePoint->Address;
	ASSERT(address);
	ServicePoint->Address = NULL;
	RemoveEntryList(&ServicePoint->ServicePointListEntry);
	LpxDereferenceAddress ("ServicePoint deleting", address, AREF_REQUEST);
	
	// added by hootch 08262003
	RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);

	SmpPrintState(2, "LpxCloseServicePoint", ServicePoint);

	DebugPrint(2, ("sequence = %x, fin_seq = %x, rmt_seq = %x, rmt_ack = %x\n", 
		SHORT_SEQNUM(ServicePoint->Sequence), SHORT_SEQNUM(ServicePoint->FinSequence),
		SHORT_SEQNUM(ServicePoint->RemoteSequence),SHORT_SEQNUM(ServicePoint->RemoteAck)));
	DebugPrint(2, ("last_retransmit_seq=%x, reason = %x\n",
		SHORT_SEQNUM(ServicePoint->LastRetransmitSequence), ServicePoint->TimerReason));


#ifndef DISABLE_PACKET_REORDERING
	count =0;
	while(listEntry = ExInterlockedRemoveHeadList(&ServicePoint->ReceiveReorderQueue, &ServicePoint->ReceiveReorderQSpinLock)) {
		reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		PacketFree(packet);
		count++;
	}
	if (count) {
		DebugPrint(1, ("Removing packet from ReorderQueue\n"));
	}
#endif	
	count = 0;
	while(listEntry = ExInterlockedRemoveHeadList(&ServicePoint->ReceiveQueue, &ServicePoint->ReceiveQSpinLock)) {
		reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		PacketFree(packet);
		count++;
	}
	if (count) {
		DebugPrint(1, ("Removed %d packet from ReceiveQueue\n", count));
	}

	while(packet
		= PacketDequeue(&ServicePoint->WriteQueue, &ServicePoint->WriteQSpinLock))
	{
		PIO_STACK_LOCATION	irpSp;
		PIRP	            irp;

		DebugPrint(1, ("TransmitQueue packet deleted\n"));
		irpSp = RESERVED(packet)->IrpSp;
		if(irpSp != NULL) {
			irp = IRP_SEND_IRP(irpSp);
			irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
			irp->IoStatus.Information = 0;
		}

		write_queue++;
		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock))
	{
		PIO_STACK_LOCATION	irpSp;
		PIRP	            irp;

		DebugPrint(1, ("RetransmitQueue packet deleted\n"));
		irpSp = RESERVED(packet)->IrpSp;

		if(irpSp != NULL) {
			irp = IRP_SEND_IRP(irpSp);
			irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
			irp->IoStatus.Information = 0;
		}

		retransmit_queue++;

		// Check Cloned Packet. 
		if(RESERVED(packet)->Cloned > 0) {
			DebugPrint(1, ("[LPX]LpxCloseServicePoint: Cloned is NOT 0. %d\n", RESERVED(packet)->Cloned));
		}

		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&ServicePoint->RcvDataQueue, &ServicePoint->RcvDataQSpinLock))
	{
		receive_queue++;
		PacketFree(packet);
	}

	DebugPrint(2, ("back_log = %d, receive_queue = %d, receivedata_queue = %d write_queue = %d, retransmit_queue = %d\n", 
			back_log, receive_queue, receivedata_queue, write_queue, retransmit_queue));

	SmpCancelIrps(ServicePoint);

	return TRUE;
}


static VOID
SmpCancelIrps(
	IN PSERVICE_POINT	ServicePoint
	)
{
	PIRP		        irp;
	PLIST_ENTRY		    thisEntry;
	PIRP		        pendingIrp;
	KIRQL		        cancelIrql;
	KIRQL		        oldIrql;
	PDRIVER_CANCEL		oldCancelRoutine;


	DebugPrint(2, ("SmpCancelIrps\n"));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);

	if(ServicePoint->ConnectIrp) {
		DebugPrint(1, ("SmpCancelIrps ConnectIrp\n"));

		irp = ServicePoint->ConnectIrp;
		ServicePoint->ConnectIrp = NULL;
		irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	 
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX]SmpCancelIrps: Connect IRP %p completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_NETWORK_UNREACHABLE));
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	}

	if(ServicePoint->ListenIrp) {
		DebugPrint(1, ("SmpCancelIrps ListenIrp\n"));

		irp = ServicePoint->ListenIrp;
		ServicePoint->ListenIrp = NULL;
		irp->IoStatus.Status = STATUS_CANCELLED;

		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX]SmpCancelIrps: Listen IRP %p completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_CANCELLED));
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	}

	if(ServicePoint->DisconnectIrp) {
		DebugPrint(1, ("SmpCancelIrps DisconnectIrp\n"));

		irp = ServicePoint->DisconnectIrp;
		ServicePoint->DisconnectIrp = NULL;
		irp->IoStatus.Status = STATUS_CANCELLED;

		IoAcquireCancelSpinLock(&cancelIrql);
		oldCancelRoutine = IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX]SmpCancelIrps: Disconnect IRP %p completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_CANCELLED));
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	}

	RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);

	//
	// Walk through the RcvList and cancel all read IRPs.
	//

	while(thisEntry=ExInterlockedRemoveHeadList(&ServicePoint->ReceiveIrpList, &ServicePoint->ReceiveIrpQSpinLock) )
	{
		pendingIrp = CONTAINING_RECORD(thisEntry, IRP, Tail.Overlay.ListEntry);

		DebugPrint(1, ("[LPX] SmpCancelIrps: Cancelled Receive IRP %p\n", pendingIrp));

		//
		//  Clear the IRP¡¯s cancel routine
		//
		IoAcquireCancelSpinLock(&cancelIrql);
		oldCancelRoutine = IoSetCancelRoutine(pendingIrp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);

		pendingIrp->IoStatus.Information = 0;
		pendingIrp->IoStatus.Status = STATUS_CANCELLED;

		DebugPrint(1, ("[LPX]SmpCancelIrps: IRP %p completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_CANCELLED));
		IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
	}

	return;
}

	
//
//	Add padding to fix Under-60byte bug of NDAS chip 2.0.
//
static NTSTATUS
PatchPacketForNdas2Bug(
	IN	PDEVICE_CONTEXT	DeviceContext,
	IN PNDIS_PACKET		Packet
	)
{
    PUCHAR			PaddingData;
    PNDIS_BUFFER	PaddingBuffDesc;
    PUCHAR          LastData;
    UINT            LastDataLen;
    PNDIS_BUFFER    LastBuffDesc;
    PNDIS_BUFFER    CurrentBuffDesc;
	NTSTATUS		status;
	PNDIS_BUFFER	firstBuffer;	
	UINT		    TotalPacketLength;
	UINT		    BufferCount;
    LONG			PaddingLen;

	NdisQueryPacket(
		Packet,
		NULL,
		&BufferCount,
		&firstBuffer,
		&TotalPacketLength
		);
	
	if (TotalPacketLength < ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2) + 4 ||
		TotalPacketLength > 56)
		return STATUS_SUCCESS; // Don't need patch.

	// if
	//	    TotalPacketLength >= ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2) + 4 &&
	//	    TotalPacketLength <= 56

	PaddingLen = 60 - TotalPacketLength;

    DebugPrint(4, ("[LpxSmp]TransmitDataPacket: Adding padding to support NDAS chip 2.0\n"));
    //
    //    Allocate memory for padding.
    //
    status = LpxAllocatePacketMemory(
    		DeviceContext,
            &PaddingData,
            PaddingLen
	);
    if(status != NDIS_STATUS_SUCCESS) {
        DebugPrint(1, ("[LpxSmp]TransmitDataPacket: Can't Allocate Memory packet.\n"));
        return status;
    }
    RtlZeroMemory(PaddingData, PaddingLen);

	NdisQueryPacket(
		Packet,
		NULL,
		&BufferCount,
		&firstBuffer,
		&TotalPacketLength
		);
    //
    //    Copy the last 4 bytes to the end of packet.
    //
    CurrentBuffDesc = firstBuffer;
    do {
        LastBuffDesc = CurrentBuffDesc;
        NdisGetNextBuffer(LastBuffDesc, &CurrentBuffDesc);
    } while(CurrentBuffDesc);

    NdisQueryBufferSafe(
                LastBuffDesc,
                &LastData,
                &LastDataLen,
                HighPagePriority
    );
    if(!LastData) {
        LpxFreePacketMemory(DeviceContext, PaddingLen, PaddingData);
        DebugPrint(1, ("[LPX]TransmitDataPacket: Can't NdisQueryBufferSafe()!!!\n"));
        PacketFree(Packet);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if(LastDataLen >= 4 && PaddingLen >= 4) {
        RtlCopyMemory(PaddingData + PaddingLen - 4, LastData + LastDataLen - 4, 4);
    } else {
        DebugPrint(1, ("[LPX]TransmitDataPacket: Data length too small. LastDataLen:%d PaddingLen:%d!!!\n",
                                    LastDataLen, PaddingLen));
        ASSERT(FALSE);
    }

    //
    //    Create the Ndis buffer desc from header memory for LPX packet header.
    //
    NdisAllocateBuffer(
            &status,
            &PaddingBuffDesc,
            DeviceContext->LpxBufferPool,
            PaddingData,
            PaddingLen
        );
    if(!NT_SUCCESS(status)) {
        LpxFreePacketMemory(DeviceContext, PaddingLen, PaddingData);
        PacketFree(Packet);
        DebugPrint(1, ("[LPX]TransmitDataPacket: Can't Allocate Buffer!!!\n"));
        return status;
    }

    NdisChainBufferAtBack(Packet, PaddingBuffDesc);
	return STATUS_SUCCESS;
}

static NTSTATUS
TransmitDataPacket(
   IN	PDEVICE_CONTEXT	DeviceContext,
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN USHORT		    UserDataLength,
	IN BOOL				MarkOpc
	)
{
	PUCHAR		    packetData;
	PLPX_HEADER2	lpxHeader;
	NTSTATUS		status;
	UCHAR		    raised = 0;
	PNDIS_BUFFER	firstBuffer;	
	USHORT		    sequence;
	UINT		    TotalPacketLength;
	UINT		    BufferCount;
	KIRQL	   irql;
	PDEVICE_CONTEXT Provider;

	ASSERT(Packet!=NULL);
	DebugPrint(3, ("TransmitDataPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));

// to do: fix this...
// This is not enough. Need to make it sure that transition to CLOSE state is not occur
//
	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &irql);
	if(ServicePoint->SmpState == SMP_CLOSE) {
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);    
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	Provider = ServicePoint->Address->Provider;
	ASSERT(Provider);
	RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);	

	DebugPrint(3, ("after TransmitDataPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
	//
	//	Initialize packet header.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		&BufferCount,
		&firstBuffer,
		&TotalPacketLength
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	lpxHeader->PacketSize = HTONS((USHORT)(SMP_SYS_PKT_LEN + UserDataLength));

	lpxHeader->LpxType		            = LPX_TYPE_STREAM;
	lpxHeader->DestinationPort		= ServicePoint->DestinationAddress.Port;
	lpxHeader->SourcePort		    = ServicePoint->SourceAddress.Port;
	lpxHeader->AckSequence		    = HTONS(SHORT_SEQNUM(ServicePoint->RemoteSequence));
	lpxHeader->ServerTag		    = ServicePoint->ServerTag;

	sequence = (USHORT)InterlockedIncrement(&ServicePoint->Sequence);
	sequence --;

	lpxHeader->Lsctl = HTONS(LSCTL_DATA | LSCTL_ACK);
	lpxHeader->Sequence = HTONS(sequence);
	if (MarkOpc)
		lpxHeader->PCont = 1;
	else
		lpxHeader->PCont = 0;
	DebugPrint(3, ("DATA DISCON : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

	status = PatchPacketForNdas2Bug(DeviceContext, Packet);
	if (status!=STATUS_SUCCESS) {
		// packet already is freed by PatchPacketForNdas2Bug
		return status;
	}

	if(!SmpSendTest(ServicePoint, Packet)) {
		DebugPrint(0, ("[LPX] TransmitDataPacket: insert WriteQueue!!!!\n"));
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &irql);
		if(ServicePoint->SmpState == SMP_CLOSE) {
		        RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);    
		        PacketFree(Packet);
		        return STATUS_INVALID_CONNECTION;
		} else {
		    ExInterlockedInsertTailList(
		                            &ServicePoint->WriteQueue,
		                            &RESERVED(Packet)->ListElement,
		                            &ServicePoint->WriteQSpinLock
		                            );
		    RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);    
		    return STATUS_SUCCESS;
		}
	}

	RoutePacketRequest(ServicePoint, Packet, DATA);
	return STATUS_SUCCESS;
}

static UCHAR JumboDummy[NDAS_JUMBO_FRAME_LENGTH] = {0};

// Called with ServicePoint->SpSpinLock locked
static PNDIS_PACKET
PrepareNonDataPacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PACKET_TYPE		PacketType
	)
{
	PUCHAR		    packetData;
	PLPX_HEADER2	lpxHeader;
	NTSTATUS		status;
	UCHAR		    raised = 0;
	PNDIS_BUFFER	firstBuffer;	
	USHORT		    sequence;
	USHORT		    finsequence;
	PTP_ADDRESS		address;
	PNDIS_PACKET Packet;
	
	DebugPrint(3, ("PrepareNonDataPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));

	ASSERT(ServicePoint->Address);

//
//	we don't need to make sure that Address field is not NULL
//	because we have a reference to Connection before calling
//	hootch 09012003

	address = ServicePoint->Address;
//	if(address == NULL) {
//			return STATUS_INSUFFICIENT_RESOURCES;
//	}

	ASSERT(address->Provider);
	if (PacketType != ACKREQJ && PacketType != ACKJ) {
		status = PacketAllocate(
			ServicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			address->Provider,
			SEND_TYPE,
			NULL,
			0,
			NULL,
			&Packet
			);
	} else {
		status = PacketAllocate(
			ServicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			address->Provider,
			SEND_TYPE,
			JumboDummy,
			NDAS_JUMBO_FRAME_LENGTH - sizeof(LPX_HEADER2),
			NULL,
			&Packet
			);
	}
	if(!NT_SUCCESS(status)) {
		DebugPrint(1, ("[LPX]PrepareNonDataPacket: packet == NULL\n"));
		SmpPrintState(1, "[LPX]PrepareNonDataPacket: PacketAlloc", ServicePoint);
		return NULL;
	}
	
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if (PacketType != ACKREQJ && PacketType != ACKJ) {
		lpxHeader->PacketSize = HTONS(SMP_SYS_PKT_LEN);
	} else {
		lpxHeader->PacketSize = NDAS_JUMBO_FRAME_LENGTH;
	}

	lpxHeader->LpxType		            = LPX_TYPE_STREAM;
	lpxHeader->DestinationPort		= ServicePoint->DestinationAddress.Port;
	lpxHeader->SourcePort		    = ServicePoint->SourceAddress.Port;

	lpxHeader->AckSequence		    = HTONS(SHORT_SEQNUM(ServicePoint->RemoteSequence));
	lpxHeader->ServerTag		    = ServicePoint->ServerTag;

	switch(PacketType) {

	case CONREQ:
	case DISCON:
		sequence = (USHORT)InterlockedIncrement(&ServicePoint->Sequence);
		sequence --;

		if(PacketType == CONREQ) {
			lpxHeader->Lsctl = HTONS(LSCTL_CONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DATA) {
			lpxHeader->Lsctl = HTONS(LSCTL_DATA | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DISCON) {
			finsequence = (USHORT)InterlockedIncrement(&ServicePoint->FinSequence);
			finsequence --;

			lpxHeader->Lsctl = HTONS(LSCTL_DISCONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(finsequence);
		}

		DebugPrint(2, ("CONREQ DISCON : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));
		
		break;

	case ACK :

		DebugPrint(4, ("ACK : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(ServicePoint->Sequence));

		break;

	case ACKJ :

		DebugPrint(4, ("ACKJ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACK | LSCTL_SET_JUMBO);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(ServicePoint->Sequence));

		break;

	case ACKNJ :

		DebugPrint(4, ("ACKNJ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACK | LSCTL_CLEAR_JUMBO);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(ServicePoint->Sequence));

		break;

	case ACKREQ :

		DebugPrint(4, ("ACKREQ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(ServicePoint->Sequence));

		break;



	case ACKREQJ :

		DebugPrint(4, ("ACKREQJ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK | LSCTL_SET_JUMBO);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(ServicePoint->Sequence));
		
		break;

	case ACKREQNJ :

		DebugPrint(4, ("ACKREQNJ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK | LSCTL_CLEAR_JUMBO);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(ServicePoint->Sequence));
		
		break;


	default:
		DebugPrint(0, ("[LPX] PrepareNonDataPacket: STATUS_NOT_SUPPORTED\n"));
		return NULL;
	}
	return Packet;
}

static NTSTATUS
TransmitNonDataPacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PACKET_TYPE		PacketType,
	IN PNDIS_PACKET		Packet
	)
{
	KIRQL Irql;
	if (Packet==NULL)
		return STATUS_UNSUCCESSFUL;
	if (PacketType == ACKREQJ) {
		DebugPrint(0, ("[LPX] TransmitNonDataPacket: Sending Jumbo ACKREQ!!!!\n"));
	}
	if (PacketType == ACKJ) {
		DebugPrint(0, ("[LPX] TransmitNonDataPacket: Replying to Jumbo ACKREQ!!!!\n"));
	}
	
	if(!SmpSendTest(ServicePoint, Packet)) { // Not needed??
		DebugPrint(0, ("[LPX] TransmitNonDataPacket: insert WriteQueue!!!!\n"));
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &Irql);
		ExInterlockedInsertTailList(
		                        &ServicePoint->WriteQueue,
		                        &RESERVED(Packet)->ListElement,
		                        &ServicePoint->WriteQSpinLock
		                        );
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, Irql);
		return STATUS_SUCCESS;
	}
	RoutePacket(ServicePoint, Packet, PacketType);
	return STATUS_SUCCESS;
}

static void
RoutePacketRequest(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	) 
{
	KIRQL		        oldIrql;
	BOOLEAN		        raised = FALSE;

	if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}
	//
	// The packet type is restricted to DATA for now.
	//
	if(PacketType != DATA) {
		DebugPrint(0, ("[LPX] RoutePacketRequest: bad PacketType for SmpWorkDPC\n"));
		return;
	}

	RoutePacket(ServicePoint, Packet, DATA);

	if(raised == TRUE)
		KeLowerIrql(oldIrql);

}

//
// Called at DISPATCH_LEVEL
//
static void
RoutePacket(
	IN PSERVICE_POINT ServicePoint,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	)
{
	PNDIS_PACKET	packet2;
	NTSTATUS		status;
	PDEVICE_CONTEXT	deviceContext;
	KIRQL		    oldIrql;
	PTP_ADDRESS		address;

	DebugPrint(3, ("RoutePacket\n"));

	switch(PacketType) {

	case CONREQ :
	case DATA :
	case DISCON :
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
		if (ServicePoint->SmpState == SMP_CLOSE) {
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			break;
		}
		packet2 = PacketClone(Packet);

		ExInterlockedInsertTailList(
			                    &ServicePoint->RetransmitQueue,
			                    &RESERVED(packet2)->ListElement,
			                    &ServicePoint->RetransmitQSpinLock
			                    );
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);

		ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrql);
		ServicePoint->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CalculateRTT(ServicePoint);
		ServicePoint->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)LpxMaxRetransmitTime);
		RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrql);

	case RETRAN:
	case ACK:
	case ACKREQ:
	case ACKREQJ:
	case ACKJ:
	case ACKREQNJ:
	case ACKNJ:
	default:
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
		if (ServicePoint->SmpState == SMP_CLOSE) {
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			// No need to route
			return;
		}
		address = ServicePoint->Address;
		deviceContext = address->Provider;
		if((deviceContext->bDeviceInit == TRUE)  && (deviceContext->NdisBindingHandle)) {
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);

			// Check tx packet drop is enabled
			if (ulTxPacketDropRate) {
				LONG DropCount;
				DropCount = InterlockedIncrement(&ulTxPacketCountForDrop);
				if(DropCount % 1000 < (LONG)ulTxPacketDropRate ) {
					// Drop tx packet
					DebugPrint(0, ("Dropping tx packet\n"));
					status = NDIS_STATUS_FAILURE;
				} else {
					NdisSend(
					    &status,
					    deviceContext->NdisBindingHandle,
					    Packet
					);
				}
			} else {
				NdisSend(
				    &status,
				    deviceContext->NdisBindingHandle,
				    Packet
				);
			}
		} else {
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			status = STATUS_INVALID_DEVICE_STATE;
		}
		switch(status) {
		case NDIS_STATUS_SUCCESS:
			DebugPrint(2, ("[LPX]RoutePacket: NdisSend return Success\n"));
			PacketFree(Packet);
#if DBG
			InterlockedIncrement(&NumberOfSent);
			InterlockedIncrement( &NumberOfSentComplete );
#endif
			break;
		case NDIS_STATUS_PENDING:
#if DBG		
			InterlockedIncrement(&NumberOfSent);
#endif			
			break;
		default:
			DebugPrint(1, ("[LPX]RoutePacket: Error when NdisSend. status: 0x%x\n", status));
			PacketFree(Packet);
			break;
		}
	}
}


VOID
LpxSendComplete(
			    IN NDIS_HANDLE    ProtocolBindingContext,
			    IN PNDIS_PACKET    Packet,
			    IN NDIS_STATUS    Status
			    )
{
	DebugPrint(3, ("LpxSendComplete\n"));

	UNREFERENCED_PARAMETER(ProtocolBindingContext);
//	UNREFERENCED_PARAMETER(Status);
	if (Status != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("LpxSendComplete status: %x\n", Status));
	}
	PacketFree(Packet);
#if DBG
	InterlockedIncrement( &NumberOfSentComplete );
#endif
	return;
}

static BOOLEAN
SmpSendTest(
			PSERVICE_POINT ServicePoint,
			PNDIS_PACKET    Packet
			)
{
	LONG		        inFlight;
	PCHAR		        packetData;
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;    
//	KIRQL		        oldIrql;

	NdisQueryPacket(
			    Packet,
			    NULL,
			    NULL,
			    &firstBuffer,
			    NULL
			    );
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	inFlight = NTOHS(SHORT_SEQNUM(lpxHeader->Sequence)) - SHORT_SEQNUM(ServicePoint->RemoteAck);

	DebugPrint(4, ("lpxHeader->Sequence = %x, ServicePoint->RemoteAck = %x\n",
		NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(ServicePoint->RemoteAck)));
	if(inFlight >= ServicePoint->MaxFlights) {
		SmpPrintState(1, "Flight overflow", ServicePoint);
		return 0;
	}

	return 1;	
}


static LONGLONG
CalculateRTT(
	IN	PSERVICE_POINT ServicePoint
	)
{
	LARGE_INTEGER	Rtime;
	int		        i;

	Rtime.QuadPart = LpxRetransmitDelay; //(ServicePoint->IntervalTime.QuadPart);

	// Exponential
	for(i = 0; i < ServicePoint->Retransmits; i++) {
		Rtime.QuadPart *= 2;
		
		if(Rtime.QuadPart > LpxMaxRetransmitDelay)
			return LpxMaxRetransmitDelay;
	}

	if(Rtime.QuadPart > LpxMaxRetransmitDelay)
		Rtime.QuadPart = LpxMaxRetransmitDelay;

	return Rtime.QuadPart;
}

//
//
//
//	called only from SmpDpcWork()
static NTSTATUS
SmpReadPacket(
	IN	 PSERVICE_POINT	ServicePoint
	)
{
	PNDIS_PACKET		packet;
	PIO_STACK_LOCATION	irpSp;
	ULONG		        userDataLength;
	ULONG		        irpCopied;
	ULONG		        remained;
	PUCHAR		        userData = NULL;
	PLPX_HEADER2		lpxHeader;

	PNDIS_BUFFER		firstBuffer;
	PUCHAR		        bufferData;
	UINT		        bufferLength;
	UINT		        copied;
	KIRQL		        oldIrql;
	KIRQL			cancelIrql;
	PIRP	        irp;
	PLIST_ENTRY		    listEntry;
	
	DebugPrint(4, ("SmpReadPacket\n"));

	//
	//	Initialize ListEntry to show the IRP is in progress.
	//	See LpxCancelRecv().
	//
	//	patched by hootch 02052004

	ACQUIRE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, &oldIrql);
	listEntry = RemoveHeadList(&ServicePoint->ReceiveIrpList);
	if(listEntry == &ServicePoint->ReceiveIrpList) {
		RELEASE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, oldIrql);
		return  STATUS_UNSUCCESSFUL; // No receive request pending right now.
	}
	InitializeListHead(listEntry);

	irp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);

	//
	//	check to see if the IRP is expired.
	//	If it is, complete the IRP with TIMEOUT.
	//	If expiration time is zero, do not check it.
	//
	//	added by hootch 02092004
	if( GET_IRP_EXPTIME(irp) && GET_IRP_EXPTIME(irp) <= CurrentTime().QuadPart) {

		DebugPrint(1,("[LPX] SmpWorkDpcRoutine IRP expired!! %I64d CurrentTime:%I64d.\n",
		        GET_IRP_EXPTIME(irp),
		        CurrentTime().QuadPart
		    ));

		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		irp->IoStatus.Status = STATUS_REQUEST_ABORTED;
		RELEASE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, oldIrql);
		IoCompleteRequest (irp, IO_NETWORK_INCREMENT);
		return STATUS_REQUEST_ABORTED; // continue to next irp
	}


	do {
		packet = PacketPeek(&ServicePoint->RcvDataQueue, &ServicePoint->RcvDataQSpinLock);

		if(packet == NULL) {
			DebugPrint(4, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p Returned\n", ServicePoint, irp));
			if (ServicePoint->ReserveCanceling) {
			    RELEASE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, oldIrql);
			    irp->IoStatus.Information = 0;
			    irp->IoStatus.Status = STATUS_CANCELLED;
			    IoAcquireCancelSpinLock(&cancelIrql);
			    IoSetCancelRoutine(irp, NULL);
			    IoReleaseCancelSpinLock(cancelIrql);
			    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);

			    ACQUIRE_SPIN_LOCK (&ServicePoint->SpSpinLock, &oldIrql);
			    CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);    
			    RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);    
			    LpxCloseServicePoint(ServicePoint);
			    return STATUS_CANCELLED;
			} else {
			    RELEASE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, oldIrql);
			    ACQUIRE_SPIN_LOCK (&ServicePoint->SpSpinLock, &oldIrql);
			    if (ServicePoint->SmpState == SMP_CLOSE) {
				    RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
//				    DbgBreakPoint();
				    irp->IoStatus.Information = 0;
				    irp->IoStatus.Status = STATUS_CONNECTION_DISCONNECTED;
				    IoAcquireCancelSpinLock(&cancelIrql);
				    IoSetCancelRoutine(irp, NULL);
				    IoReleaseCancelSpinLock(cancelIrql);
				    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);

				    ACQUIRE_SPIN_LOCK (&ServicePoint->SpSpinLock, &oldIrql);
				    CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);    
				    RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);
				    LpxCloseServicePoint(ServicePoint);
					return STATUS_CONNECTION_DISCONNECTED; // Connection is closed while waiting rx packet.
			    } else {
					ExInterlockedInsertHeadList(
                        &ServicePoint->ReceiveIrpList,
                        &irp->Tail.Overlay.ListEntry,
						&ServicePoint->ReceiveIrpQSpinLock
	                );
				    RELEASE_SPIN_LOCK (&ServicePoint->SpSpinLock, oldIrql);    
					return STATUS_PENDING; // No data arrived.
	            }
			}
		}

		lpxHeader = (PLPX_HEADER2)RESERVED(packet)->LpxSmpHeader;
		if(NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
			ServicePoint->Shutdown |= SMP_RECEIVE_SHUTDOWN;    

			packet = PacketDequeue(&ServicePoint->RcvDataQueue, &ServicePoint->RcvDataQSpinLock);
			PacketFree(packet);
			break;
		}

		// Check packet loss based on packet interval only if caller requested
		if (IRP_TRANS_STAT(irp) && ServicePoint->LastRecvDataArrived.QuadPart !=0) {
			LONGLONG TimeDiff; // time interval between two packet in 100nano sec unit.
			TimeDiff = (LONGLONG)(RESERVED(packet)->RecvTime.QuadPart - ServicePoint->LastRecvDataArrived.QuadPart);

//			if (TimeDiff > LpxRetransmitDelay * 3/4) { 
			if (TimeDiff > MSEC_TO_HZ(45)) { 
				// Some old netdisk has retransmission time of 50ms. 
				//		- But link which is slower than 216k will always record packet loss
			    // Assume packet is lost if interval between packet is 75% of Retransmission delay.
			    DebugPrint(1, ("Rx packet lost based on interval check.\n"));
			    INC_IRP_PACKET_LOSS(irp, 1);
			}
		}
		ServicePoint->LastRecvDataArrived = RESERVED(packet)->RecvTime;

		irpSp = IoGetCurrentIrpStackLocation(irp);
		userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 
		irpCopied = (ULONG)irp->IoStatus.Information;
		remained = userDataLength - irpCopied;

		userData = MmGetSystemAddressForMdlSafe(irp->MdlAddress, HighPagePriority);
		userData += irpCopied;
		
		NdisQueryPacket(
			    packet,
			    NULL,
			    NULL,
			    &firstBuffer,
			    NULL
			    );

		bufferData = MmGetMdlVirtualAddress(firstBuffer);
		bufferLength = NdisBufferLength(firstBuffer);
		bufferLength -= RESERVED(packet)->PacketDataOffset;
		bufferData += RESERVED(packet)->PacketDataOffset;
		copied = (bufferLength < remained) ? bufferLength : remained;
		RESERVED(packet)->PacketDataOffset += copied;

		if(copied)
			RtlCopyMemory(
			    userData,
			    bufferData,
			    copied
			);

		irp->IoStatus.Information += copied;
		if(RESERVED(packet)->PacketDataOffset == NdisBufferLength(firstBuffer)) {
			// Add by jgahn.
			packet = PacketDequeue(&ServicePoint->RcvDataQueue, &ServicePoint->RcvDataQSpinLock);
			PacketFree(packet);
		}

		DebugPrint(4, ("userDataLength = %d, copied = %d, Irp->IoStatus.Information = %d\n",
			    userDataLength, copied, irp->IoStatus.Information));

	} while(userDataLength > irp->IoStatus.Information);

	DebugPrint(4, ("SmpReadPacket IRP\n"));

	ServicePoint->LastRecvDataArrived.QuadPart = 0;
	
	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(irp, NULL);
	IoReleaseCancelSpinLock(cancelIrql);
	RELEASE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, oldIrql);

	irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	DebugPrint(3, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p completed.\n", ServicePoint, irp));

	//
	//	check to see if shutdown is in progress.
	//
	//	added by hootch 09042003
	//

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);

	if(ServicePoint->Shutdown & SMP_RECEIVE_SHUTDOWN) {
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);

		DebugPrint(1, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p canceled\n", ServicePoint, irp));

		LpxCallUserReceiveHandler(ServicePoint);
		SmpCancelIrps(ServicePoint);
	}
	else {
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
	}

	return STATUS_SUCCESS;
}

//
//	request the Smp DPC routine for the packet received
//
//
//	added by hootch 09092003
static VOID
SmpDoReceiveRequest(
	IN PSERVICE_POINT ServicePoint,
	IN PNDIS_PACKET	Packet
	) {
	LONG	cnt;
	KIRQL		        oldIrql;
	BOOLEAN		        raised = FALSE;
	BOOLEAN			bRet;


	if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}

	ExInterlockedInsertTailList(&ServicePoint->ReceiveQueue,
		&RESERVED(Packet)->ListElement,
		&ServicePoint->ReceiveQSpinLock
		);
	cnt = InterlockedIncrement(&ServicePoint->RequestCnt);
	if( cnt == 1 ) {
		LpxReferenceConnection("DoReceiveReq", ServicePoint->Connection, CREF_WORKDPC);
		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
		bRet = KeInsertQueueDpc(&ServicePoint->SmpWorkDpc, NULL, NULL);
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);	    
		if (!bRet) { // DPC is already in queue. Deference myself.
			LpxDereferenceConnection("DoReceiveReq", ServicePoint->Connection, CREF_WORKDPC);
		}
	}

	if(raised == TRUE)
		KeLowerIrql(oldIrql);
}

//
//
//	called only from LpxReceiveComplete
//
//	  Return TRUE if packet is handled.(whether packet is used or dropped)
//			  or FALSE. Packet will be processed again to check reodering, so Packet should not be freed.
//
static BOOLEAN
SmpDoReceive(
	IN PSERVICE_POINT ServicePoint,
	IN PNDIS_PACKET	Packet
	)
{
	PLPX_HEADER2		lpxHeader;
	KIRQL		        oldIrql;
	KIRQL		        oldIrql2;
	USHORT RemoteSeq;
	BOOLEAN PacketHandled;
	DebugPrint(3, ("SmpDoReceive\n"));
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	RemoteSeq = SHORT_SEQNUM(ServicePoint->RemoteSequence);

	if( (SHORT)( NTOHS(lpxHeader->Sequence) - RemoteSeq) > MAX_ALLOWED_SEQ )
	{
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet (2)\n"));
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			PacketFree(Packet);
			return TRUE;
	}

	if( NTOHS(lpxHeader->Lsctl) & LSCTL_ACK ) {
		if(((SHORT)(SHORT_SEQNUM(ServicePoint->RemoteAck) - NTOHS(lpxHeader->AckSequence))) > MAX_ALLOWED_SEQ){
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet (3)\n"));
			PacketFree(Packet);
			return TRUE;
		}
	}



	DebugPrint(4, ("SmpDoReceive NTOHS(lpxHeader->Sequence) = 0x%x, lpxHeader->Lsctl = 0x%x\n",
		NTOHS(lpxHeader->Sequence), lpxHeader->Lsctl));

	// Since we received vaild Packet, reset AliveTimeOut.
	InterlockedExchange(&ServicePoint->AliveRetries, 0);

	ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrql2);
	ServicePoint->AliveTimeOut.QuadPart = CurrentTime().QuadPart + LpxAliveInterval;
	RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrql2);
	switch(ServicePoint->SmpState) {
	case SMP_LISTEN:
		PacketHandled = LpxStateDoReceiveWhenListen(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_SYN_SENT:
		PacketHandled = LpxStateDoReceiveWhenSynSent(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_SYN_RECV:
		PacketHandled = LpxStateDoReceiveWhenSynRecv(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_ESTABLISHED:
		PacketHandled = LpxStateDoReceiveWhenEstablished(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_LAST_ACK:
		PacketHandled = LpxStateDoReceiveWhenLastAck(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_FIN_WAIT1:
		PacketHandled = LpxStateDoReceiveWhenFinWait1(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_FIN_WAIT2:
		PacketHandled = LpxStateDoReceiveWhenFinWait2(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_CLOSING:
		PacketHandled = LpxStateDoReceiveWhenClosing(ServicePoint->Connection, Packet, oldIrql);
		break;
	case SMP_CLOSE:
	case SMP_CLOSE_WAIT:
	case SMP_TIME_WAIT:
	default:
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		DebugPrint(1, ("[LPX] Dropping packet in %s state. ", LpxStateName[ServicePoint->SmpState]));
		DebugPrint(1, ("   src=%02x:%02x:%02x:%02x:%02x:%02x   lsctl=%04x\n", 
			RESERVED(Packet)->EthernetHeader.SourceAddress[0], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[1], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[2], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[3], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[4], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[5], 
			NTOHS(lpxHeader->Lsctl) ));
		PacketFree(Packet);
		PacketHandled = TRUE;
		break;
	}
	return PacketHandled;
}			

//
//	acquire ServicePoint->SpinLock before calling
//
//	called only from SmpDoReceive()
static INT
SmpRetransmitCheck(
			       IN PSERVICE_POINT    ServicePoint,
			       IN LONG            AckSequence,
			       IN PACKET_TYPE    PacketType
			       )
{
	PNDIS_PACKET		packet;
//	PLIST_ENTRY		    packetListEntry;
	PLPX_HEADER2		lpxHeader;
	PUCHAR		        packetData;
	PNDIS_BUFFER		firstBuffer;    
	PLPX_RESERVED		reserved;
	

	DebugPrint(3, ("[LPX]SmpRetransmitCheck: Entered.\n"));

	UNREFERENCED_PARAMETER(PacketType);

	packet = PacketPeek(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock);
	if(!packet)
		return -1;

	NdisQueryPacket(
		packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
	);
	   packetData = MmGetMdlVirtualAddress(firstBuffer);

	reserved = RESERVED(packet);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if(((SHORT)(SHORT_SEQNUM(AckSequence) - NTOHS(lpxHeader->Sequence))) <= 0)
		return -1;
			            
	while((packet = PacketPeek(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock)) != NULL)
	{
		reserved = RESERVED(packet);
		NdisQueryPacket(
			            packet,
			            NULL,
			            NULL,
			            &firstBuffer,
			            NULL
			            );
		packetData = MmGetMdlVirtualAddress(firstBuffer);
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

		DebugPrint(4, ("AckSequence = %x, lpxHeader->Sequence = %x\n",
			            AckSequence, NTOHS(lpxHeader->Sequence)));
			
		if((SHORT)(SHORT_SEQNUM(AckSequence) - NTOHS(lpxHeader->Sequence)) > 0)
		{
			DebugPrint(3, ("[LPX] SmpRetransmitCheck: deleted a packet to be  retransmitted.\n"));
			packet = PacketDequeue(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock);
			if(packet) PacketFree(packet);
		} else
			break;
	}


	{
		KIRQL	oldIrql;

		ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrql);
		ServicePoint->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)LpxMaxRetransmitTime);
		RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrql);
	}

	if(ServicePoint->Retransmits) {

		InterlockedExchange(&ServicePoint->Retransmits, 0);
		ServicePoint->TimerReason |= SMP_RETRANSMIT_ACKED;
	} 
	else if(!(ServicePoint->TimerReason & SMP_RETRANSMIT_ACKED)
		&& !(ServicePoint->TimerReason & SMP_SENDIBLE)
		&& ((packet = PacketPeek(&ServicePoint->WriteQueue, &ServicePoint->WriteQSpinLock)) != NULL)
		&& SmpSendTest(ServicePoint, packet))
	{
		ServicePoint->TimerReason |= SMP_SENDIBLE;
	}

	return 0;
}

static VOID
SmpWorkDpcRoutine(
			       IN    PKDPC    dpc,
			       IN    PVOID    Context,
			       IN    PVOID    junk1,
			       IN    PVOID    junk2
			       ) {
	PSERVICE_POINT		ServicePoint = (PSERVICE_POINT)Context;
	LONG		        cnt;
	PNDIS_PACKET		packet;
	PLPX_RESERVED		reserved;
	PLIST_ENTRY		    listEntry;
	NTSTATUS		    status;
	BOOLEAN		        timeOut;    
	LARGE_INTEGER		start_time;
	BOOLEAN		     Ret;
	BOOLEAN		     Matched;
#ifndef DISABLE_PACKET_REORDERING
	ULONG		       ReorderCount;
	PLIST_ENTRY	 lastEntry;	
#endif

	LpxReferenceConnection("SmpWorkDpcRoutine", ServicePoint->Connection, CREF_PROCESS_DATA);

do_more:
	start_time = CurrentTime();

	timeOut = FALSE;

	//
	//	Receive completion
	//	Assume packets in ReceiveQueue can be non-ordered.
	//
	//	  Check reorder queue first
	do {
		Matched = FALSE;
#ifndef DISABLE_PACKET_REORDERING
		ReorderCount = 0;
		// Assume this is not executed concurrently because this is run by single DPC.
		lastEntry = ServicePoint->ReceiveReorderQueue.Blink; 
		while(listEntry = ExInterlockedRemoveHeadList(&ServicePoint->ReceiveReorderQueue, &ServicePoint->ReceiveReorderQSpinLock)) {
			ReorderCount++;
			reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
			packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
			Ret = SmpDoReceive(ServicePoint, packet);
			if (Ret==TRUE) {
			    DebugPrint(3, ("Resolved out of order packet\n"));
			    Matched = TRUE;
			} else {
			    InterlockedIncrement(&reserved->ReorderCount);
			    if (reserved->ReorderCount > MAX_REORDER_COUNT) {
			        PLPX_HEADER2  lpxHeader = (PLPX_HEADER2) RESERVED(packet)->LpxSmpHeader;
			        DebugPrint(1, ("Dropping out-of-order packet Type=%x%s, Seq=%x, Ack=%x, RS=%x\n",
			            NTOHS(lpxHeader->Lsctl),
			            (NTOHS(lpxHeader->Lsctl)&LSCTL_DATA)?"(DATA)":"",
			            NTOHS(lpxHeader->Sequence),
			            NTOHS(lpxHeader->AckSequence),
			            SHORT_SEQNUM(ServicePoint->RemoteSequence)
			        ));
			        PacketFree(packet);
			    } else {
			        // Queue to reorder queue again.
			        ExInterlockedInsertTailList(&ServicePoint->ReceiveReorderQueue, listEntry, &ServicePoint->ReceiveReorderQSpinLock);
			    }
			}
			if(ServicePoint->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
			{
			    //ServicePoint->SmpTimerSet = TRUE;
			    SmpTimerDpcRoutine(dpc, ServicePoint, junk1, junk2);
			    timeOut = TRUE;
			    break;
			}
			if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
			    DebugPrint(3,("[LPX] Timeout while handling ReceiveQueue!!!! start_time : %I64d , CurrentTime : %I64d \n",
			            start_time.QuadPart, CurrentTime().QuadPart));
			    goto TIMEOUT;
			}
			if (listEntry==lastEntry)
			    break;  // We looped one time.
		}
#endif

		while(listEntry = ExInterlockedRemoveHeadList(&ServicePoint->ReceiveQueue, &ServicePoint->ReceiveQSpinLock)) {
			reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
			packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
			Ret = SmpDoReceive(ServicePoint, packet);
			if (Ret==TRUE) {
			    Matched = TRUE;
			} else {
#ifndef DISABLE_PACKET_REORDERING			
			    DebugPrint(4, ("Queuing to Reorder queue\n"));
			    // Queue to Reorder queue.
			    ExInterlockedInsertTailList(
			        &ServicePoint->ReceiveReorderQueue,
			        listEntry,
			        &ServicePoint->ReceiveReorderQSpinLock
			        );
#endif				
			}
			if(ServicePoint->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
			{
			    //ServicePoint->SmpTimerSet = TRUE;
			    SmpTimerDpcRoutine(dpc, ServicePoint, junk1, junk2);
			    timeOut = TRUE;
			    break;
			}
			if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
			    DebugPrint(3,("[LPX] Timeout while handling ReceiveQueue!!!! start_time : %I64d , CurrentTime : %I64d \n",
			            start_time.QuadPart, CurrentTime().QuadPart));
			    goto TIMEOUT;
			}
		}
	} while(Matched);  // Check reorder queue and ReceiveQueue again and again if any packet in queue was usable.
	
	//
	//	Receive IRP completion
	//
	while(1)
	{
		status = SmpReadPacket(ServicePoint);
		if (status == STATUS_REQUEST_ABORTED) {
			continue; // continue to next irp
		} else if(status == STATUS_CANCELLED || status == STATUS_CONNECTION_DISCONNECTED) {
			goto CancelOut;
		} else if (status !=STATUS_SUCCESS) {
			break; // no receive irp or no data arrived yet.
		} else {
			LpxCallUserReceiveHandler(ServicePoint);
		}
		if(ServicePoint->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		{
			//ServicePoint->SmpTimerSet = TRUE;
			SmpTimerDpcRoutine(dpc, ServicePoint, junk1, junk2);
			timeOut = TRUE;
			break;
		}
		if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
			DebugPrint(1,("[LPX] Timeout while handling ReceiveIrp!!!! start_time : %I64d , CurrentTime : %I64d \n",
			        start_time.QuadPart, CurrentTime().QuadPart));
			goto TIMEOUT;
		}
	}

	//
	//	Timer expiration
	//
	//		ServicePoint->SmpTimerSet = TRUE;


	if(ServicePoint->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		SmpTimerDpcRoutine(dpc, ServicePoint, junk1, junk2);


	if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
		DebugPrint(1,("[LPX] Timeout at SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
			        start_time.QuadPart, CurrentTime().QuadPart));
		goto TIMEOUT;
	}


	if(timeOut == TRUE)
		goto do_more;

TIMEOUT:	
	cnt = InterlockedDecrement(&ServicePoint->RequestCnt);
	if(cnt) {
		// New request is received during this call. 
		// Reset the counter to one and do work more then.
		//
		InterlockedExchange(&ServicePoint->RequestCnt, 1);
		goto do_more;
	}

CancelOut:
	LpxDereferenceConnection("SmpWorkDpcRoutine", ServicePoint->Connection, CREF_PROCESS_DATA);
	LpxDereferenceConnection("SmpWorkDpcRoutine", ServicePoint->Connection, CREF_WORKDPC);
	return;
}

//
//	request the Smp DPC routine for the time-expire
//
//
//	added by hootch 09092003
static VOID
SmpTimerDpcRoutineRequest(
			       IN    PKDPC    dpc,
			       IN    PVOID    Context,
			       IN    PVOID    junk1,
			       IN    PVOID    junk2
			       ) 
{
	PSERVICE_POINT		ServicePoint = (PSERVICE_POINT)Context;
	LONG	cnt;
	KIRQL		        oldIrql;
	BOOLEAN		        raised = FALSE;
	LARGE_INTEGER		TimeInterval;
	BOOLEAN bRet;
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(junk1);
	UNREFERENCED_PARAMETER(junk2);

	// Prevent ServicePoint is freed during this function
	LpxReferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TEMP);

	if(ServicePoint->SmpTimerExpire.QuadPart > CurrentTime().QuadPart) {
		
		if(ServicePoint->Retransmits)
			DebugPrint(3,("SmpTimerDpcRoutineRequest\n"));

		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
		
		bRet = KeCancelTimer(&ServicePoint->SmpTimer);
		if (bRet)  { // Timer is in queue. If we canceled it, we need to dereference it.
			LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
		}
		if (ServicePoint->SmpState == SMP_CLOSE) {
			RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
			LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
			return;
		}
		TimeInterval.QuadPart = - LpxSmpTimeout;
		LpxReferenceConnection("TimerRequest", ServicePoint->Connection, CREF_TIMERDPC);
		bRet = KeSetTimer(
			&ServicePoint->SmpTimer,
			*(PTIME)&TimeInterval,    
			&ServicePoint->SmpTimerDpc
			);
		if (bRet ==TRUE) { // Timer is already in system queue. deference myself.
			LpxDereferenceConnection("TimerRequest", ServicePoint->Connection, CREF_TIMERDPC);
		}
			
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
		LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TEMP);	
		LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
		return;
	}

	if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}

	cnt = InterlockedIncrement(&ServicePoint->RequestCnt);

	if( cnt == 1 ) {
		LpxReferenceConnection("DoReceiveReq", ServicePoint->Connection, CREF_WORKDPC);
		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
		bRet = KeInsertQueueDpc(&ServicePoint->SmpWorkDpc, NULL, NULL);
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->SmpWorkDpcLock);
		if (!bRet) { // DPC is already in queue. Deference myself.
			LpxDereferenceConnection("DoReceiveReq", ServicePoint->Connection, CREF_WORKDPC);
		}
	}
	
	if(raised == TRUE)
		KeLowerIrql(oldIrql);
	LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TEMP);
	LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
}

static VOID
SmpTimerDpcRoutine(
			       IN    PKDPC    dpc,
			       IN    PVOID    Context,
			       IN    PVOID    junk1,
			       IN    PVOID    junk2
			       )
{
	PSERVICE_POINT		ServicePoint = (PSERVICE_POINT)Context;
	PNDIS_PACKET		packet;
	PNDIS_PACKET		packet2;
	PUCHAR		        packetData;
	PNDIS_BUFFER		firstBuffer;    
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;
	LIST_ENTRY		    tempQueue;
	KSPIN_LOCK		    tempSpinLock;
	KIRQL		        cancelIrql;
	LONG		        cloned;
	BOOLEAN			bRet;
	LARGE_INTEGER		current_time;
	LARGE_INTEGER		TimeInterval = {0,0};
	DebugPrint(5, ("SmpTimerDpcRoutine ServicePoint = %p\n", ServicePoint));

	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(junk1);
	UNREFERENCED_PARAMETER(junk2);

	KeInitializeSpinLock(&tempSpinLock);

	// added by hootch 08262003
	ACQUIRE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
	
	current_time = CurrentTime();
	if(ServicePoint->Retransmits) {
		SmpPrintState(4, "Ret3", ServicePoint);
		DebugPrint(4,("current_time.QuadPart %I64d\n", current_time.QuadPart));
	}

	if(ServicePoint->SmpState == SMP_CLOSE) {
		RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
		DebugPrint(1, ("[LPX] SmpTimerDpcRoutine: ServicePoint %p closed\n", ServicePoint));
		return;
	}

	//
	//	reference Connection
	//
	LpxReferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

	bRet = KeCancelTimer(&ServicePoint->SmpTimer);
	if (bRet)  { // Timer is in queue. If we canceled it, we need to dereference it.
		LpxDereferenceConnection("SmpTimerDpcRoutineRequest", ServicePoint->Connection, CREF_TIMERDPC);
	}

	//
	//	do condition check
	//
	switch(ServicePoint->SmpState) {
		
	case SMP_TIME_WAIT:
		
		if(ServicePoint->TimeWaitTimeOut.QuadPart <= current_time.QuadPart) 
		{
			DebugPrint(2, ("[LPX] SmpTimerDpcRoutine: TimeWaitTimeOut ServicePoint = %p\n", ServicePoint));

			if(ServicePoint->DisconnectIrp) {
			    PIRP    irp;
			    
			    irp = ServicePoint->DisconnectIrp;
			    ServicePoint->DisconnectIrp = NULL;
			    
			    IoAcquireCancelSpinLock(&cancelIrql);
			    IoSetCancelRoutine(irp, NULL);
			    IoReleaseCancelSpinLock(cancelIrql);
			    
			    irp->IoStatus.Status = STATUS_SUCCESS;
			    DebugPrint(1, ("[LPX]SmpTimerDpcRoutine: Disconnect IRP %p completed.\n ", irp));
			    RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
			    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
			} else {
			 RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);
			}

			LpxCloseServicePoint(ServicePoint);

			LpxDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

			return;
		}
		
		goto out;
		
	case SMP_CLOSE: 
	case SMP_LISTEN:
		
		goto out;
		
	case SMP_SYN_SENT:
		
		if(ServicePoint->ConnectTimeOut.QuadPart <= current_time.QuadPart) 
		{
			RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);

			LpxCloseServicePoint(ServicePoint);

			LpxDereferenceConnection("SmpTimerDpcRoutine: ", ServicePoint->Connection, CREF_REQUEST);

			return;
		}
		
		break;
		
	default:
		
		break;
	}

	//
	//	we need to check retransmission?
	//
	if((!PacketQueueEmpty(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock))
		&& ServicePoint->RetransmitTimeOut.QuadPart <= current_time.QuadPart) 
	{
//		DebugPrint(DebugLevel, ("smp_retransmit_timeout ServicePoint->retransmits = %d, ",ServicePoint->Retransmits));
//		DebugPrint(DebugLevel, ("CurrentTime().QuadPart = %I64d\n", current_time.QuadPart));
//		DebugPrint(DebugLevel, ("ServicePoint->RetransmitTimeOut.QuadPart = %I64d\n", 
//			ServicePoint->RetransmitTimeOut.QuadPart));
//		DebugPrint(DebugLevel, ("delayed = %I64d\n", 
//			CurrentTime().QuadPart - ServicePoint->RetransmitTimeOut.QuadPart));

#if 0   // To do
		// 1.0, 1.1 conreq packet loss bug fix: Netdisk ignore multiple connection request from same host. 
		//	    So if host lose first connection ack, host cannot connect the disk.
		//	Work around: connect with different source port number.
		if (ServicePoint->SmpState == SMP_SYN_SENT) {
			USHORT port;
			// find another port number
			port = lpx_get_free_port_num();
			// Change port
			lpx_unreg_bind(sk);
			LPX_OPT(sk)->source_addr.port = g_htons(port);
			lpx_reg_bind(sk);
			lpxhdr->source_port = LPX_OPT(sk)->source_addr.port;
			debug_lpx(2, "Conreq fix: changing port number to %d", port);
		}
#endif

		//
		//	retransmission time-out
		//
		//if(ServicePoint->Retransmits > 100) 
		if(ServicePoint->RetransmitEndTime.QuadPart < current_time.QuadPart) 
		{
			DebugPrint(1,("Retransmit Time Out\n"));
			CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);
			RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);

			LpxCloseServicePoint(ServicePoint);
			LpxDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

			return;
//#endif
		}

		//
		//	retransmit.
		//
		// Need to leave packet on the queue, aye the fear
		//
		packet = PacketPeek(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock);
		if(!packet) {
			DebugPrint(0, ("no packet - is not stable\n"));
			goto out;
		}
		NdisQueryPacket(
			packet,
			NULL,
			NULL,
			&firstBuffer,
			NULL
			);
		packetData = MmGetMdlVirtualAddress(firstBuffer);
		reserved = RESERVED(packet);
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];
		lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(ServicePoint->RemoteSequence));

		InterlockedIncrement(&ServicePoint->Retransmits);
		ServicePoint->RetransmitTimeOut.QuadPart 
			= CurrentTime().QuadPart + CalculateRTT(ServicePoint);
		InterlockedExchange(&ServicePoint->LastRetransmitSequence, (ULONG)NTOHS(lpxHeader->Sequence));

		SmpPrintState(4, "Ret0", ServicePoint);

		// AING : raise reserved->Retransmits to record retransmits for this packet
		//	    retransmits for one send command will sum at LpxDereference in PacketFree
		if(LPX_TYPE_STREAM == lpxHeader->LpxType &&
			SEND_TYPE == reserved->Type)
		{
			InterlockedIncrement(&reserved->Retransmits);
		}
		
		packet2 = PacketCopy(packet, &cloned);
		if(cloned == 1) {
			// Packet is copied. So safe to release lock.
			RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
			DebugPrint(1,("[LPX] Retransmit\n"));
			RoutePacket(ServicePoint, packet2, RETRAN);
			ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
		} else {
			// If cloned is not 1, another copy of this packet is alreay in the transmitting state.
			SmpPrintState(1, "Cloned", ServicePoint);
			PacketFree(packet2);
		}
		
	} 
	else if(ServicePoint->TimerReason & SMP_RETRANSMIT_ACKED) 
	{
		BOOLEAN	YetSent = 0;
		
		SmpPrintState(4, "Ret1", ServicePoint);
		if(PacketQueueEmpty(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock))
		{
			ServicePoint->TimerReason &= ~SMP_RETRANSMIT_ACKED;
			goto send_packet;
		}

		InitializeListHead(&tempQueue);
		
		while((packet = PacketDequeue(&ServicePoint->RetransmitQueue, &ServicePoint->RetransmitQSpinLock)) != NULL)
		{
			ExInterlockedInsertHeadList(&tempQueue,
			    &RESERVED(packet)->ListElement,
			    &tempSpinLock
			    );
			
			NdisQueryPacket(
			    packet,
			    NULL,
			    NULL,
			    &firstBuffer,
			    NULL
			    );
			packetData = MmGetMdlVirtualAddress(firstBuffer);
			reserved = RESERVED(packet);
			lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];
			lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(ServicePoint->RemoteSequence));

			packet2 = PacketCopy(packet, &cloned);
			if(cloned == 1) {
			    ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock);
			    ServicePoint->RetransmitTimeOut.QuadPart 
			        = CurrentTime().QuadPart + CalculateRTT(ServicePoint);
			    RELEASE_DPC_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock);

			    RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
			    DebugPrint(1,("[R2]"));
			    RoutePacket(ServicePoint, packet2, RETRAN);
			    ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
			} else {
			    PacketFree(packet2);
			    YetSent = 1;
			    break;
			}
		}
		
		while((packet = PacketDequeue(&tempQueue, &tempSpinLock)) != NULL)
		{
			ExInterlockedInsertHeadList(
			    &ServicePoint->RetransmitQueue,
			    &RESERVED(packet)->ListElement,
			    &ServicePoint->RetransmitQSpinLock
			    );
		}
		if(!YetSent)	
			ServicePoint->TimerReason &= ~SMP_RETRANSMIT_ACKED;
	} 
	else if(ServicePoint->TimerReason & SMP_SENDIBLE) 
	{
		SmpPrintState(2, "Send", ServicePoint);
send_packet:
		while((packet = PacketDequeue(&ServicePoint->WriteQueue, &ServicePoint->WriteQSpinLock)) != NULL)
		{
			SmpPrintState(1, "RealSend", ServicePoint);
			if(SmpSendTest(ServicePoint, packet)) 
			{
			    NdisQueryPacket(
			        packet,
			        NULL,
			        NULL,
			        &firstBuffer,
			        NULL
			        );
			    packetData = MmGetMdlVirtualAddress(firstBuffer);
			    reserved = RESERVED(packet);
			    lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];
			    lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(ServicePoint->RemoteSequence));
			    
//			    RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpinLock);
			    RoutePacket(ServicePoint, packet, DATA);
//			    ACQUIRE_DPC_SPIN_LOCK (&ServicePoint->SpinLock);
			} else 
			{
			    ExInterlockedInsertHeadList(
			        &ServicePoint->WriteQueue,
			        &RESERVED(packet)->ListElement,
			        &ServicePoint->WriteQSpinLock
			        );
			    break;
			}
		}
		ServicePoint->TimerReason &= ~SMP_SENDIBLE;
	} else if (ServicePoint->AliveTimeOut.QuadPart <= current_time.QuadPart)	 {// alive message
		LONG alive;
		PNDIS_PACKET Packet;
		alive = InterlockedIncrement(&ServicePoint->AliveRetries);
		if(( alive % 2) == 0) {
			DebugPrint(100, ("alive_retries = %d, smp_alive CurrentTime().QuadPart = %llx\n", 
			    ServicePoint->AliveRetries, current_time.QuadPart));
		}
		if(ServicePoint->AliveRetries > LpxMaxAliveCount) {
			SmpPrintState(100, "Alive Max", ServicePoint);
			
			DebugPrint(2, ("!!!!!!!!!!! ServicePoint->Connection->AddressFile->RegisteredDisconnectHandler 0x%x\n",
			    ServicePoint->Connection->AddressFile->RegisteredDisconnectHandler));

			CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);
			RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);

			LpxCloseServicePoint(ServicePoint);

			LpxDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

			return;
		}

		DebugPrint(3, ("[LPX]SmpTimerDpcRoutine: Send ACKREQ. SP : %p, S: 0x%x, RS: 0x%x\n", 
			ServicePoint, ServicePoint->Sequence, SHORT_SEQNUM(ServicePoint->RemoteSequence)));

		ServicePoint->AliveTimeOut.QuadPart = CurrentTime().QuadPart + LpxAliveInterval;
		
		Packet = PrepareNonDataPacket(ServicePoint, ACKREQ);
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
		TransmitNonDataPacket(ServicePoint, ACKREQ, Packet);
		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpSpinLock);
	}

out:
	ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock);
	ServicePoint->SmpTimerExpire.QuadPart = CurrentTime().QuadPart + LpxSmpTimeout;
	RELEASE_DPC_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock);

	if(ServicePoint->Retransmits) {
		SmpPrintState(4, "Ret2", ServicePoint);
		DebugPrint(4,("current_time.QuadPart %I64d\n", current_time.QuadPart));
	}
	
	if (ServicePoint->SmpState != SMP_CLOSE) {
		TimeInterval.QuadPart = - LpxSmpTimeout;	
		LpxReferenceConnection("Timer", ServicePoint->Connection, CREF_TIMERDPC);
		bRet = KeSetTimer(
			&ServicePoint->SmpTimer,
			*(PTIME)&TimeInterval,    
			&ServicePoint->SmpTimerDpc
			);
		if (bRet ==TRUE) { // Timer is already in system queue. deference myself.
			LpxDereferenceConnection("Timer", ServicePoint->Connection, CREF_TIMERDPC);
		}
	 }

	RELEASE_DPC_SPIN_LOCK (&ServicePoint->SpSpinLock);

	//
	//	dereference the connection
	//
	LpxDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST);

	return;
}
//
//
//	acquire SpinLock of connection before calling
//
void
CallUserDisconnectHandler(
			      IN    PSERVICE_POINT    pServicePoint,
			      IN    ULONG            DisconnectFlags
			      )
{
	LONG called;
	// Check parameter.
	if(pServicePoint == NULL) {
		return;
	}

	if(pServicePoint->SmpState != SMP_ESTABLISHED) {
		return;
	}

	if(pServicePoint->Connection == NULL) {
		// This should not happen..
		return;
	}

	if(pServicePoint->Connection->AddressFile == NULL) {
		// This should not happen..	
		return;
	}
	
	if(pServicePoint->Connection->AddressFile->DisconnectHandler == NULL) {
		return;
	}

//	ACQUIRE_DPC_SPIN_LOCK(&(pServicePoint->Address->SpinLock));
	called = InterlockedIncrement(&pServicePoint->lDisconnectHandlerCalled);
	if(called == 1) {

		// Perform Handler.
		(*pServicePoint->Connection->AddressFile->DisconnectHandler)(
			pServicePoint->Connection->AddressFile->DisconnectHandlerContext,
			pServicePoint->Connection->Context,
			0,
			NULL,
			0,
			NULL,
			DisconnectFlags
			);
	} else {
		DebugPrint(1,("[LPX]DisconnectHandler: Already Called\n"));
	}
//	RELEASE_DPC_SPIN_LOCK(&(pServicePoint->Address->SpinLock));
	return;
}

//////////////////////////////////////////
//
//  LPX State machine refactoring
// 


BOOLEAN LpxStateDoReceiveWhenListen(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PLPX_HEADER2		lpxHeader;
	UCHAR		        dataArrived = 0;
	LARGE_INTEGER		TimeInterval = {0,0};
	PSERVICE_POINT ServicePoint;
	PNDIS_PACKET ReplyPacket;
	KIRQL oldIrql = irql;
	USHORT lsctl;
	BOOL bRet;
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;
	ASSERT(Connection->ServicePoint.SmpState == SMP_LISTEN);
	lsctl = NTOHS(lpxHeader->Lsctl) & LSCTL_MASK; // Mask lsctl to support futre usage of lsctl field
	switch(lsctl) 
	{
	case LSCTL_CONNREQ:
	case LSCTL_CONNREQ | LSCTL_ACK:
		DebugPrint(1, ("SmpDoReceive SMP_LISTEN CONREQ\n"));

		if(ServicePoint->ListenIrp == NULL) {
			DebugPrint(1, ("SmpDoReceive ERROR. No ListenIrp. Dropping packet\n"));
			break;
		}

		InterlockedIncrement(&ServicePoint->RemoteSequence);
		RtlCopyMemory(
			ServicePoint->DestinationAddress.Node,
			RESERVED(Packet)->EthernetHeader.SourceAddress,
			ETHERNET_ADDRESS_LENGTH
			);
		ServicePoint->DestinationAddress.Port = lpxHeader->SourcePort;
		LpxChangeState(ServicePoint->Connection, SMP_SYN_RECV, TRUE);

		ServicePoint->ServerTag =lpxHeader->ServerTag;

		ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock);
		ServicePoint->ConnectTimeOut.QuadPart = CurrentTime().QuadPart + LpxConnectionTimeout;
		ServicePoint->SmpTimerExpire.QuadPart = CurrentTime().QuadPart + LpxSmpTimeout;
		RELEASE_DPC_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock);

		LpxReferenceConnection("Listen", ServicePoint->Connection, CREF_TIMERDPC);
		TimeInterval.QuadPart = - LpxSmpTimeout;
		bRet = KeSetTimer(
			&ServicePoint->SmpTimer,
			*(PTIME)&TimeInterval,
			&ServicePoint->SmpTimerDpc
			);
		if (bRet ==TRUE) { // Timer is already in system queue. deference myself.
			LpxDereferenceConnection("Listen", ServicePoint->Connection, CREF_TIMERDPC);
		}

		ReplyPacket = PrepareNonDataPacket(&Connection->ServicePoint, CONREQ);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(&Connection->ServicePoint, CONREQ, ReplyPacket);
		break;
	default:
		DebugPrint(1, ("Dropping non CONNREQ packet(%x) for listening socket.\n", NTOHS(lpxHeader->Lsctl)));
		DebugPrint(1, ("   src=%02x:%02x:%02x:%02x:%02x:%02x, src port=%d, des port=%d\n", 
			RESERVED(Packet)->EthernetHeader.SourceAddress[0], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[1], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[2], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[3], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[4], 
			RESERVED(Packet)->EthernetHeader.SourceAddress[5], 
			NTOHS(lpxHeader->SourcePort),
			NTOHS(lpxHeader->DestinationPort) ));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		break;
	}
	PacketFree(Packet);
	return TRUE; // Not a data packet
}

BOOLEAN LpxStateDoReceiveWhenSynSent(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	KIRQL		        oldIrql = irql;
	UCHAR		        dataArrived = 0;
	PNDIS_PACKET AckPacket;
	PNDIS_PACKET AckReqJPacket;
	KIRQL	cancelIrql;
	DebugPrint(3, ("SmpDoReceive\n"));
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;
	ASSERT(Connection->ServicePoint.SmpState == SMP_SYN_SENT);

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
		InterlockedExchange(&ServicePoint->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
		SmpRetransmitCheck(ServicePoint, ServicePoint->RemoteAck, ACK);
	}

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ) {
		DebugPrint(1, ("SmpDoReceive SMP_SYN_SENT CONREQ\n"));
//		SmpPrintState(1, "LSCTL_CONNREQ", ServicePoint);
		InterlockedIncrement(&ServicePoint->RemoteSequence);

		ServicePoint->ServerTag = lpxHeader->ServerTag;
		LpxChangeState(ServicePoint->Connection, SMP_SYN_RECV, TRUE);

		AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);

		if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) 
		{
			PDEVICE_CONTEXT deviceContext;
			deviceContext = (PDEVICE_CONTEXT)ServicePoint->Address->Provider;
			if (deviceContext->MaxJumboUserData) { // This NIC can send jumbo packet
				ServicePoint->WaitingSetJumbo = TRUE;
				AckReqJPacket = PrepareNonDataPacket(ServicePoint, ACKREQJ);
				RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
				TransmitNonDataPacket(ServicePoint, ACKREQJ, AckReqJPacket);
				ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &irql);
			}
			if (lpxHeader->NoDack) {
				DebugPrint(1, ("Peer does not support delayed ack\n"));			
				ServicePoint->PeerWithoutDelayedAck = TRUE;
			}
			LpxChangeState(ServicePoint->Connection, SMP_ESTABLISHED, TRUE);

			if(ServicePoint->ConnectIrp) {
			    PIRP    irp;
			    irp = ServicePoint->ConnectIrp;
			    ServicePoint->ConnectIrp = NULL;
			    irp->IoStatus.Status = STATUS_SUCCESS;
			    IoAcquireCancelSpinLock(&cancelIrql);
			    IoSetCancelRoutine(irp, NULL);
			    IoReleaseCancelSpinLock(cancelIrql);
			    DebugPrint(1, ("[LPX]SmpDoReceive: Connect IRP %p completed.\n ", irp));
			    RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			    IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
			} else {
			    DebugPrint(1, ("[LPX]SmpDoReceive: Entered ESTABLISH state without ConnectIrp!!!!\n "));
			    RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			}
		} 
	} else {
		DebugPrint(1, ("SmpDoReceive Unexpected packet in SYN_SENT state(%x)\n", NTOHS(lpxHeader->Lsctl)));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);	    
	}
	PacketFree(Packet);
	return TRUE;
}

// Transitted from listen state.
BOOLEAN LpxStateDoReceiveWhenSynRecv(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	KIRQL		cancelIrql;
//	PNDIS_PACKET AckReqJPacket;
//	PDEVICE_CONTEXT deviceContext;
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;	
	ASSERT(ServicePoint->SmpState == SMP_SYN_RECV);	

	DebugPrint(1, ("LpxDoReceive SMP_SYN_RECV CONREQ\n"));

	// Accept ACK only in this state
	if(!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)) {
		DebugPrint(1, ("LpxDoReceive Unexpected packet when SYN_RECV %x\n", NTOHS(lpxHeader->Lsctl)));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		PacketFree(Packet);
		return TRUE;
	}
	
	if(NTOHS(lpxHeader->AckSequence) < 1) {
		DebugPrint(1, ("LpxDoReceive Invalid acksequence when SYN_RECV\n"));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		PacketFree(Packet);
		return TRUE;
	}

	// 
	// Check Server Tag.
	//
	if(lpxHeader->ServerTag != ServicePoint->ServerTag) {
		DebugPrint(1, ("[LPX] SmpDoReceive/SynRecv: Bad Server Tag. Dropping. ServicePoint->ServerTag 0x%x, lpxHeader->ServerTag 0x%x\n", ServicePoint->ServerTag, lpxHeader->ServerTag));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		PacketFree(Packet);
		return TRUE;	    
	}

#if 0	// Connecting initiator will send jumbo ackreq.
	deviceContext = (PDEVICE_CONTEXT)ServicePoint->Address->Provider;
	if (deviceContext->MaxJumboUserData) { // This NIC can send jumbo packet
		ServicePoint->WaitingSetJumbo = TRUE;
		AckReqJPacket = PrepareNonDataPacket(ServicePoint, ACKREQJ);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		TransmitNonDataPacket(ServicePoint, ACKREQJ, AckReqJPacket);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &irql);
	}
#endif	
	
	LpxChangeState(ServicePoint->Connection, SMP_ESTABLISHED, TRUE);


	// Handle ACK.
	InterlockedExchange(&ServicePoint->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
	SmpRetransmitCheck(ServicePoint, ServicePoint->RemoteAck, ACK);
	
	if(ServicePoint->ConnectIrp) 
	{
		PIRP	irp;
		irp = ServicePoint->ConnectIrp;
		ServicePoint->ConnectIrp = NULL;
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		irp->IoStatus.Status = STATUS_SUCCESS;
		DebugPrint(1, ("[LPX]SmpDoReceive: Connect IRP %p completed.\n ", irp));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
	} else if(ServicePoint->ListenIrp) 
	{
		PIRP	                    irp;
		PIO_STACK_LOCATION	        irpSp;
		PTDI_REQUEST_KERNEL_LISTEN	request;
		PTDI_CONNECTION_INFORMATION	connectionInfo;

		irp = ServicePoint->ListenIrp;
		ServicePoint->ListenIrp = NULL;

		irpSp = IoGetCurrentIrpStackLocation(irp);
		request = (PTDI_REQUEST_KERNEL_LISTEN)&irpSp->Parameters;
		connectionInfo = request->ReturnConnectionInformation;

		if(connectionInfo != NULL) 
		{	
			connectionInfo->UserData = NULL;
			connectionInfo->UserDataLength = 0;
			connectionInfo->Options = NULL;
			connectionInfo->OptionsLength = 0;

			if(connectionInfo->RemoteAddressLength != 0)
			{
			    UCHAR                addressBuffer[FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
			                                        + FIELD_OFFSET(TA_ADDRESS, Address)
			                                        + TDI_ADDRESS_LENGTH_LPX];
			    PTRANSPORT_ADDRESS    transportAddress;
			    ULONG                returnLength;
			    PTA_ADDRESS            taAddress;
			    PTDI_ADDRESS_LPX    lpxAddress;


			    DebugPrint(2, ("connectionInfo->RemoteAddressLength = %d, addressLength = %d\n", 
			            connectionInfo->RemoteAddressLength, (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
			                                                + FIELD_OFFSET(TA_ADDRESS, Address)
			                                                + TDI_ADDRESS_LENGTH_LPX)));
			                                    
			    transportAddress = (PTRANSPORT_ADDRESS)addressBuffer;

			    transportAddress->TAAddressCount = 1;
			    taAddress = (PTA_ADDRESS)transportAddress->Address;
			    taAddress->AddressType        = TDI_ADDRESS_TYPE_LPX;
			    taAddress->AddressLength    = TDI_ADDRESS_LENGTH_LPX;

			    lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

			    RtlCopyMemory(
			            lpxAddress,
			            &ServicePoint->DestinationAddress,
			            sizeof(LPX_ADDRESS)
			            );
			    returnLength = (connectionInfo->RemoteAddressLength <=  (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
			                                                        + FIELD_OFFSET(TA_ADDRESS, Address)
			                                                        + TDI_ADDRESS_LENGTH_LPX)) 
			                            ? connectionInfo->RemoteAddressLength
			                                : (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
			                                    + FIELD_OFFSET(TA_ADDRESS, Address)
			                                    + TDI_ADDRESS_LENGTH_LPX);

			    RtlCopyMemory(
			        connectionInfo->RemoteAddress,
			        transportAddress,
			        returnLength
			    );
			}
		}	    

		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		irp->IoStatus.Status = STATUS_SUCCESS;
		DebugPrint(1, ("[LPX]SmpDoReceive: Listen IRP %p completed.\n ", irp));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
	}else {
		DebugPrint(1, ("[LPX]SmpDoReceive: No IRP to handle. Why do we in SYN_RECV state??????\n"));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
	}
	PacketFree(Packet);	
	return TRUE;
}

BOOLEAN LpxStateDoReceiveWhenEstablished(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	KIRQL		        oldIrql = irql;
	UCHAR		        dataArrived = 0;
	BOOLEAN		        packetConsumed = FALSE;
	PNDIS_PACKET AckPacket;
	USHORT lsctl;
	PACKET_TYPE PacketType;
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;	
	ASSERT(ServicePoint->SmpState == SMP_ESTABLISHED);	
	
	// 
	// Check Server Tag.
	//
	if(lpxHeader->ServerTag != ServicePoint->ServerTag) {
		DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Bad Server Tag. Dropping. ServicePoint->ServerTag 0x%x, lpxHeader->ServerTag 0x%x\n", ServicePoint->ServerTag, lpxHeader->ServerTag));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
		PacketFree(Packet);
		return TRUE;	    // Packet is consumed.
	}

	if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
		InterlockedExchange(&ServicePoint->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
		SmpRetransmitCheck(ServicePoint, ServicePoint->RemoteAck, ACK);
		if((NTOHS(lpxHeader->Lsctl) & LSCTL_SET_JUMBO)) {
			DebugPrint(0, ("LPX: Setting jumbo mode for this connection\n"));	
			ServicePoint->SendJumbo = TRUE;
			ServicePoint->WaitingSetJumbo = FALSE;
		} else if((NTOHS(lpxHeader->Lsctl) & LSCTL_CLEAR_JUMBO)) {
			DebugPrint(0, ("LPX: Unsetting jumbo mode for this connection\n"));	
			ServicePoint->SendJumbo = FALSE;
			ServicePoint->WaitingSetJumbo = FALSE;
		} 
	}

	if(ServicePoint->Retransmits) {
		DebugPrint(1, ("LPX: %d Retransmits left\n", ServicePoint->Retransmits));
//		SmpPrintState(1, "remained", ServicePoint);
	}

	lsctl = NTOHS(lpxHeader->Lsctl) & LSCTL_MASK;
	switch(lsctl) {
	case LSCTL_ACKREQ:
	case LSCTL_ACKREQ | LSCTL_ACK:
		PacketType = ACK;
		if (NTOHS(lpxHeader->Lsctl) & LSCTL_SET_JUMBO) {
			// Jumbo ackreq is received. Reply jumbo ack if this NIC also supports it
			PDEVICE_CONTEXT deviceContext;
			deviceContext = (PDEVICE_CONTEXT)ServicePoint->Address->Provider;
			if (deviceContext->MaxJumboUserData) { // This NIC can send jumbo packet
				PacketType = ACKJ;
			}
		} else if (NTOHS(lpxHeader->Lsctl) & LSCTL_CLEAR_JUMBO) {
			// Peer or network does not support jumbo packet.
			PacketType = ACKNJ;
		}
		AckPacket = PrepareNonDataPacket(ServicePoint, PacketType);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(ServicePoint, PacketType, AckPacket);
		PacketFree(Packet);
		packetConsumed = TRUE;
		break;

	case LSCTL_DATA:
	case LSCTL_DATA | LSCTL_ACK:
		if(NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) <= sizeof(LPX_HEADER2)) {
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Data packet without Data!!!!!!!!!!!!!! SP: %p\n", ServicePoint));
		}
		if(((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(ServicePoint->RemoteSequence))) > 0) {
#if 0
			DebugPrint(1, ("[LPX] Remote packet lost: HeaderSeq 0x%x, RS: 0x%x\n", NTOHS(lpxHeader->Sequence), ServicePoint->RemoteSequence));
			DebugPrint(1,("     link: %02X%02X%02X%02X%02X%02X:%04X\n",
			        ServicePoint->DestinationAddress.Node[0],
			        ServicePoint->DestinationAddress.Node[1],
			        ServicePoint->DestinationAddress.Node[2],
			        ServicePoint->DestinationAddress.Node[3],
			        ServicePoint->DestinationAddress.Node[4],
			        ServicePoint->DestinationAddress.Node[5],
			        ServicePoint->DestinationAddress.Port));
#endif
//			TransmitNonDataPacket(ServicePoint, ACK, &oldIrql);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			// Do not free packet. Packet may be out of order. 
			break;
		}

		if(((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(ServicePoint->RemoteSequence))) < 0) {
			KIRQL Irql;
			DebugPrint(2, ("Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(ServicePoint->RemoteSequence)));
			AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
			ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
			
			// Netdisk retransmitted packet, which is possibly caused by packet loss.
			ACQUIRE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, &Irql);
			if (!IsListEmpty(&ServicePoint->ReceiveIrpList)) {
				PLIST_ENTRY listEntry;
				PIRP FirstIrp;
				listEntry = ServicePoint->ReceiveIrpList.Flink;
				FirstIrp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);
				INC_IRP_PACKET_LOSS(FirstIrp, 1);
				if (GET_IRP_PACKET_LOSS(FirstIrp) == 1) {
					DebugPrint(2, ("Setting rx packet loss count\n"));
				}
			}
			RELEASE_SPIN_LOCK(&ServicePoint->ReceiveIrpQSpinLock, Irql);
		  
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);            
			PacketFree(Packet);
			packetConsumed = TRUE;
			break;
		}

		InterlockedIncrement(&ServicePoint->RemoteSequence);

		ExInterlockedInsertTailList(&ServicePoint->RcvDataQueue,
			&RESERVED(Packet)->ListElement,
			&ServicePoint->RcvDataQSpinLock
		);

		if (lpxHeader->PCont) {
			// Peer don't want ack.

			ServicePoint->UnsentAckCount++;
			if (ServicePoint->UnsentAckCount > (UINT32)MaxPCont || ServicePoint->SendJumbo) {
				// We've not sent ack for a long time. Send ACK anyway.
				//	Or we're sending jumbo packet. Do not use OPC.
				AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
				RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
				TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
				ServicePoint->UnsentAckCount =0;
			} else {
				// Skip sending ACK for DATA.
				RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			}
		} else {
			AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
		}
		// No need to free packet
		packetConsumed = TRUE;
		break;
	case LSCTL_DISCONNREQ:
	case LSCTL_DISCONNREQ | LSCTL_ACK:
		if(NTOHS(lpxHeader->Sequence) > SHORT_SEQNUM(ServicePoint->RemoteSequence)) {
			AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: remote packet lost: HeaderSeq 0x%x, RS: 0x%x\n", NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(ServicePoint->RemoteSequence)));
			PacketFree(Packet);
			packetConsumed = TRUE;
			break;
		}

		if(NTOHS(lpxHeader->Sequence) < SHORT_SEQNUM(ServicePoint->RemoteSequence)) {
			AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(ServicePoint->RemoteSequence)));
			PacketFree(Packet);
			packetConsumed = TRUE;
			break;
		}

		InterlockedIncrement(&ServicePoint->RemoteSequence);

		ExInterlockedInsertTailList(&ServicePoint->RcvDataQueue,
			&RESERVED(Packet)->ListElement,
			&ServicePoint->RcvDataQSpinLock
			);
		
		//
		//BUG BUG BUG!!!!!!
		//
		AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);

		DebugPrint(1, ("[LPX] Receive : LSCTL_DISCONNREQ | LSCTL_ACK: SmpDoReceive from establish to SMP_CLOSE_WAIT\n"));
		CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_RELEASE);
		LpxChangeState(ServicePoint->Connection, SMP_CLOSE_WAIT, TRUE);
		   
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		packetConsumed = TRUE;
		// No need to free packet
		break;
	default:

		if ((NTOHS(lpxHeader->Lsctl) == LSCTL_ACK) && ServicePoint->WaitingSetJumbo) { 
			// Normal ack received since I sent jumbo ackreq.
			// Jumbo packet may be lost. unset jumbo mode.
			ServicePoint->WaitingSetJumbo = FALSE;
			DebugPrint(0, ("LPX: Sending unset jumbo ACKREQ\n"));	

			AckPacket = PrepareNonDataPacket(ServicePoint, ACKREQNJ);
			RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
			TransmitNonDataPacket(ServicePoint, ACKREQNJ, AckPacket);
			ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
		}
	
		 if(!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)) 
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Unexpected packet received %x\n", NTOHS(lpxHeader->Lsctl)));
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);

		PacketFree(Packet);
		packetConsumed = TRUE;	    
		break;
	}
	return packetConsumed;
}

BOOLEAN LpxStateDoReceiveWhenLastAck(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;	
	ASSERT(ServicePoint->SmpState == SMP_LAST_ACK);	
	
	if ((NTOHS(lpxHeader->Lsctl) & LSCTL_MASK) == LSCTL_ACK) {
		InterlockedExchange(&ServicePoint->RemoteAck,(LONG)NTOHS(lpxHeader->AckSequence));
		SmpRetransmitCheck(ServicePoint, ServicePoint->RemoteAck, ACK);

		if(SHORT_SEQNUM(ServicePoint->RemoteAck) == SHORT_SEQNUM(ServicePoint->FinSequence)) {
			KIRQL    oldIrqlTimeCnt;
			DebugPrint(1, ("[LPX] SmpDoReceive: entering SMP_TIME_WAIT due to RemoteAck == FinSequence\n"));
			LpxChangeState(ServicePoint->Connection, SMP_TIME_WAIT, TRUE);
			ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrqlTimeCnt);
			ServicePoint->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + LpxWaitInterval;
			RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrqlTimeCnt);
		}
	} else {
		DebugPrint(1, ("SmpDoReceive Unexpected packet %x\n", NTOHS(lpxHeader->Lsctl)));
	}
	RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
	PacketFree(Packet);		
	return TRUE;
}

BOOLEAN LpxStateDoReceiveWhenFinWait1(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	PNDIS_PACKET	AckPacket;
	KIRQL		        oldIrql = irql;    
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;	
	ASSERT(ServicePoint->SmpState == SMP_FIN_WAIT1);

	DebugPrint(2, ("SmpDoReceive SMP_FIN_WAIT1 lpxHeader->Lsctl = %d\n", NTOHS(lpxHeader->Lsctl)));

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {
		DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1:  Unexpected data packet\n"));
		InterlockedIncrement(&ServicePoint->RemoteSequence);
	} else if (NTOHS(lpxHeader->Lsctl) & (LSCTL_CONNREQ | LSCTL_ACKREQ)) {
		DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1:  Unexpected packet %x\n", NTOHS(lpxHeader->Lsctl)));
	} 
	
	if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
		InterlockedExchange(&ServicePoint->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
		SmpRetransmitCheck(ServicePoint, ServicePoint->RemoteAck, ACK);

		if(SHORT_SEQNUM(ServicePoint->RemoteAck) == SHORT_SEQNUM(ServicePoint->FinSequence)) {
			DebugPrint(2, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1: SMP_FIN_WAIT1 to SMP_FIN_WAIT2\n"));
			LpxChangeState(ServicePoint->Connection, SMP_FIN_WAIT2, TRUE);
		}
	} 

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
		InterlockedIncrement(&ServicePoint->RemoteSequence);

		LpxChangeState(ServicePoint->Connection, SMP_CLOSING, TRUE);
		
		if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
			if(SHORT_SEQNUM(ServicePoint->RemoteAck) == SHORT_SEQNUM(ServicePoint->FinSequence)) {
			    DebugPrint(2, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1: entering SMP_TIME_WAIT due to RemoteAck == FinSequence\n"));
			    LpxChangeState(ServicePoint->Connection, SMP_TIME_WAIT, TRUE);
			    ServicePoint->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + LpxWaitInterval;
			} else {
			    DebugPrint(2, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1: RemoteAck != FinSequence\n"));
			}
		}
		AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	}
	
	RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
	PacketFree(Packet);		
	return TRUE;
}


BOOLEAN LpxStateDoReceiveWhenFinWait2(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	KIRQL		        oldIrql = irql;    
	KIRQL		       oldIrqlTimeCnt;
	PNDIS_PACKET	AckPacket;
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;	
	ASSERT(ServicePoint->SmpState == SMP_FIN_WAIT2);

	DebugPrint(4, ("SmpDoReceive SMP_FIN_WAIT2  lpxHeader->Lsctl = 0x%x\n", NTOHS(lpxHeader->Lsctl)));
	if (NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {
		DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1:  Unexpected data packet\n"));
		InterlockedIncrement(&ServicePoint->RemoteSequence);
	} else if (NTOHS(lpxHeader->Lsctl) & (LSCTL_CONNREQ | LSCTL_ACKREQ)) {
		DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1:  Unexpected packet %x\n", NTOHS(lpxHeader->Lsctl)));
	}
	
	if (NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
		InterlockedIncrement(&ServicePoint->RemoteSequence);
		DebugPrint(2, ("[LPX] SmpDoReceive/SMP_FIN_WAIT2: entering SMP_TIME_WAIT\n"));
		LpxChangeState(ServicePoint->Connection, SMP_TIME_WAIT, TRUE);
		ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrqlTimeCnt);
		ServicePoint->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + LpxWaitInterval;
		RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrqlTimeCnt);

		AckPacket = PrepareNonDataPacket(ServicePoint, ACK);
		RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
		TransmitNonDataPacket(ServicePoint, ACK, AckPacket);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpSpinLock, &oldIrql);
	}
	RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, oldIrql);
	PacketFree(Packet);
	return TRUE;
}

BOOLEAN LpxStateDoReceiveWhenClosing(
		IN PTP_CONNECTION Connection,
		IN PNDIS_PACKET	Packet,
		IN KIRQL	            irql
) {
	PSERVICE_POINT ServicePoint;
	PLPX_HEADER2		lpxHeader;
	KIRQL		       oldIrqlTimeCnt;
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	ServicePoint = &Connection->ServicePoint;	
	ASSERT(ServicePoint->SmpState == SMP_CLOSING);

	if ((NTOHS(lpxHeader->Lsctl) & LSCTL_MASK) == LSCTL_ACK) {
		InterlockedExchange(&ServicePoint->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
		SmpRetransmitCheck(ServicePoint, ServicePoint->RemoteAck, ACK);

		if(SHORT_SEQNUM(ServicePoint->RemoteAck) == SHORT_SEQNUM(ServicePoint->FinSequence)) {
			DebugPrint(2, ("[LPX] SmpDoReceive/SMP_CLOSING: entering SMP_TIME_WAIT\n"));
			LpxChangeState(ServicePoint->Connection, SMP_TIME_WAIT, TRUE);
			ACQUIRE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, &oldIrqlTimeCnt);
			ServicePoint->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + LpxWaitInterval;
			RELEASE_SPIN_LOCK(&ServicePoint->TimeCounterSpinLock, oldIrqlTimeCnt);
		} 
	} else {
		DebugPrint(1, ("[LPX] SmpDoReceive/SMP_CLOSING: Unexpected packet %x\n", NTOHS(lpxHeader->Lsctl)));
	}

	RELEASE_SPIN_LOCK(&ServicePoint->SpSpinLock, irql);
	PacketFree(Packet);
	return TRUE;
}


VOID LpxChangeState(
	IN PTP_CONNECTION Connection,
	IN SMP_STATE NewState,
	IN BOOLEAN Locked
) {
	KIRQL irql;
	PCHAR PrevStateName;
	if (NewState>=SMP_LAST) {
		ASSERT(FALSE);
		return;
	}

	if (!Locked) {
		ACQUIRE_SPIN_LOCK(&Connection->ServicePoint.SpSpinLock, &irql);
	}
	
	PrevStateName = LpxStateName[Connection->ServicePoint.SmpState];
	
	Connection->ServicePoint.SmpState = NewState;
	DebugPrint(2,("LPX:Con %p: %s to %s(<-> %02x:%02x:%02x:%02x:%02x:%02x:%d)\n",
		Connection, 
		PrevStateName, LpxStateName[Connection->ServicePoint.SmpState],
		Connection->ServicePoint.DestinationAddress.Node[0], 
		Connection->ServicePoint.DestinationAddress.Node[1], 
		Connection->ServicePoint.DestinationAddress.Node[2], 
		Connection->ServicePoint.DestinationAddress.Node[3], 
		Connection->ServicePoint.DestinationAddress.Node[4], 
		Connection->ServicePoint.DestinationAddress.Node[5],
		Connection->ServicePoint.DestinationAddress.Port
	));	
	if (!Locked) {
		RELEASE_SPIN_LOCK(&Connection->ServicePoint.SpSpinLock, irql);
	}
}

