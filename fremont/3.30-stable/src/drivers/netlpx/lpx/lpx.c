/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop

#if __LPX__

VOID
LpxCancelConnection (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PIRP				Irp
    );

VOID
LpxCancelSend (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp
	);

VOID
LpxCancelReceive (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp
	);

PNDIS_PACKET
PrepareSmpNonDataPacket (
	IN PTP_CONNECTION		Connection,
	IN LPX_SMP_PACKET_TYPE	PacketType
	);

PNDIS_PACKET
PrepareSmpNonDataPacket (
	IN PTP_CONNECTION		Connection,
	IN LPX_SMP_PACKET_TYPE	PacketType
	);

NDIS_STATUS
TransmitSmpUserData (
	IN	PDEVICE_CONTEXT		DeviceContext,
	IN	PTP_CONNECTION		Connection,
	IN  PIO_STACK_LOCATION	IrpSp,
	IN  PCHAR				UserData,
	IN  USHORT				UserDataLength
	);

VOID
LpxSendDataPackets (
	IN PTP_CONNECTION	Connection
	);

VOID
LpxSendPacket (
	IN	PTP_CONNECTION		Connection,
	IN PNDIS_PACKET			Packet,
	IN LPX_SMP_PACKET_TYPE	PacketType
	);

VOID
LpxSendPackets ( 
	IN	PTP_CONNECTION	Connection,
	IN  PPNDIS_PACKET	PacketArray,
	IN  UINT			NumberOfPackets
	);

BOOLEAN
SmpSendTest (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET     Packet
	);

LONGLONG
CalculateRTT (
	IN PTP_CONNECTION	Connection
	);

NTSTATUS
LpxSmpProcessReceivePacket (
	IN  PTP_CONNECTION	Connection,
	OUT PBOOLEAN		IrpCompleted
	);

VOID
LpxSmpReceiveComplete (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet
	); 

BOOLEAN
SmpDoReceive (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet
	);

BOOLEAN 
LpxStateDoReceiveWhenListen (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	);

BOOLEAN 
LpxStateDoReceiveWhenSynSent (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

BOOLEAN 
LpxStateDoReceiveWhenSynRecv (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	);

BOOLEAN 
LpxStateDoReceiveWhenEstablished (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

BOOLEAN 
LpxStateDoReceiveWhenFinWait1 (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

BOOLEAN 
LpxStateDoReceiveWhenFinWait2 (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

BOOLEAN 
LpxStateDoReceiveWhenClosing (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

BOOLEAN 
LpxStateDoReceiveWhenCloseWait (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

BOOLEAN 
LpxStateDoReceiveWhenLastAck (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	);

BOOLEAN 
LpxStateDoReceiveWhenTimeWait (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	); 

VOID
SmpRetransmitCheck (
	IN PTP_CONNECTION	Connection,
	IN  USHORT			AckSequence,
	OUT PLIST_ENTRY		FreePacketList
	);

VOID 
LpxCallUserReceiveHandler (
	IN PTP_CONNECTION	Connection
	);

BOOLEAN	NdasTestBug = 1;

#if DBG

PKSPIN_LOCK DebugSpinLock = NULL;

#endif


//#if DBG
	
ULONG	PacketTxDropRate = 0; // Drop PacketTxDropRate out of 1000 packet
ULONG	PacketTxCountForDrop = 0;

ULONG	PacketRxDropRate = 0;
ULONG	PacketRxCountForDrop = 0;

//#endif


PCHAR LpxStateName[] = {

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
//	acquire SpinLock of DeviceContext before calling
//	comment by hootch 09042003
//
//	called only from LpxOpenAddress()
//
NTSTATUS
LpxAssignPort (
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

	ASSERT( SocketLpxDeviceContext != AddressDeviceContext );
	
	DebugPrint( 2, ("Smp LPX_BIND %02x:%02x:%02x:%02x:%02x:%02x SourceAddress->Port = %x\n", 
					SourceAddress->Node[0],SourceAddress->Node[1],SourceAddress->Node[2],
					SourceAddress->Node[3],SourceAddress->Node[4],SourceAddress->Node[5],
					SourceAddress->Port) );

	portNum		= AddressDeviceContext->LastPortNum;
	listHead	= &AddressDeviceContext->AddressDatabase;

	do {

		portNum++;
		
		if (portNum == 0) {
		
			portNum = LPX_PORTASSIGN_BEGIN;
		}

		notUsedPortFound = TRUE;

		for (thisEntry = AddressDeviceContext->AddressDatabase.Flink;
			 thisEntry != listHead;
			 thisEntry = thisEntry->Flink) {

			address = CONTAINING_RECORD (thisEntry, TP_ADDRESS, Linkage);

			if (address->NetworkName != NULL) {
			    
				if (address->NetworkName->Port == HTONS(portNum)) {
			    
					notUsedPortFound = FALSE;
			        break;
			    }
			}
		}

		//	Do not allow to assign reserved ports due to historical reason.

		if (portNum == RESERVEDPORT_NDASPNP_BCAST) {
		
			notUsedPortFound = FALSE;
		}

	} while (notUsedPortFound == FALSE && portNum != AddressDeviceContext->LastPortNum);
	
	if (notUsedPortFound == FALSE) {

		DebugPrint( 2, ("[Lpx] LpxAssignPort: couldn't find available port number\n") );

		NDAS_ASSERT(FALSE);
		status = STATUS_TRANSPORT_FULL;

		goto ErrorOut;
	}

	SourceAddress->Port = HTONS(portNum);
	AddressDeviceContext->LastPortNum = portNum;

	DebugPrint( 2, ("Smp LPX_BIND portNum = %x\n", portNum) );

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
LpxLookupAddress (
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
			
				if (RtlCompareMemory(address->NetworkName, SourceAddress, i) != i) {
			        
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



NTSTATUS 
LpxConnect (
	IN		PTP_CONNECTION	Connection,
	IN OUT	PIRP	        Irp
   )
{   
	NTSTATUS		status;
	KIRQL		    oldIrql;
	KIRQL		    oldIrql2;
	PNDIS_PACKET	packet;
	KIRQL			cancelIrql;
	BOOLEAN			result;
	PDEVICE_CONTEXT	deviceContext = Connection->AddressFile->Address->Provider;

	
	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	DebugPrint( 2, ("LpxConnect\n") );

	// IoAcquireCancelSpinLock( &cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	switch (Connection->LpxSmp.SmpState) {

	case SMP_STATE_CLOSE: {

		PLPX_ADDRESS		destinationAddress;
		LARGE_INTEGER		TimeInterval = {0,0};
#if 0
		PCONNECTION_PRIVATE	connectionPrivate;
#endif

		ASSERT( !FlagOn(Connection->Flags2, CONNECTION_FLAGS2_STOPPING) );
#if 0
		if (Irp->Cancel) {

			status = STATUS_CANCELLED;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );		
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			break;
		}
#endif
		ACQUIRE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, &oldIrql2);

		if (!(FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_CONNECTED) && 
			  !FlagOn(deviceContext->LpxMediaFlags, LPX_DEVICE_CONTEXT_MEDIA_DISCONNECTED))) {

			RELEASE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, oldIrql2);
			status = NDIS_STATUS_NO_CABLE;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );		
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			break;
		} else {
			RELEASE_SPIN_LOCK(&deviceContext->LpxMediaFlagSpinLock, oldIrql2);
		}

		ASSERT( Connection->LpxSmp.ConnectIrp == NULL );

#if 0
		connectionPrivate = ExAllocatePoolWithTag( NonPagedPool, sizeof(CONNECTION_PRIVATE), LPX_MEM_TAG_PRIVATE );

		if (connectionPrivate) {

			RtlZeroMemory( connectionPrivate, sizeof(CONNECTION_PRIVATE) );
			(PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0] = connectionPrivate;

			KeQuerySystemTime( &connectionPrivate->CurrentTime );
			connectionPrivate->Connection = Connection;
			LpxReferenceConnection( "LpxConnect", Connection, CREF_LPX_PRIVATE );
		
		}

#endif

		Connection->LpxSmp.ConnectIrp = Irp;

		IoSetCancelRoutine( Irp, LpxCancelConnection );
		
		destinationAddress = &Connection->CalledAddress;

		RtlCopyMemory( &Connection->LpxSmp.DestinationAddress,
					   destinationAddress,
					   sizeof(LPX_ADDRESS) );

		LpxChangeState( Connection, SMP_STATE_SYN_SENT, TRUE );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		DebugPrint(2,("Connecting to %02X%02X%02X%02X%02X%02X:%04X\n",
					   Connection->LpxSmp.DestinationAddress.Node[0],
					   Connection->LpxSmp.DestinationAddress.Node[1],
					   Connection->LpxSmp.DestinationAddress.Node[2],
					   Connection->LpxSmp.DestinationAddress.Node[3],
					   Connection->LpxSmp.DestinationAddress.Node[4],
					   Connection->LpxSmp.DestinationAddress.Node[5],
					   Connection->LpxSmp.DestinationAddress.Port));

		packet = PrepareSmpNonDataPacket( Connection, SMP_TYPE_CONREQ );

		if (!packet) {

			DebugPrint( 2, ("[LPX] LPX_CONNECT ERROR\n") );

			KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
			LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
			KeLowerIrql ( oldIrql );

			status = STATUS_PENDING;
			break;
		}

		ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

		if (FlagOn(Connection->Flags2, CONNECTION_FLAGS2_STOPPING)) {

			NDAS_ASSERT(FALSE);			
			
			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

			IoAcquireCancelSpinLock( &cancelIrql );
			ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

			if (Connection->LpxSmp.ConnectIrp) {

				Irp = Connection->LpxSmp.ConnectIrp;
				Connection->LpxSmp.ConnectIrp = NULL;

				IoSetCancelRoutine( Irp, NULL );

				status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Status = status;
			
			} else {

				status = STATUS_PENDING;
			}

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			IoReleaseCancelSpinLock( cancelIrql );

			PacketFree( Connection->AddressFile->Address->Provider, packet );

			break;
		}

		LpxReferenceConnection( "Connect", Connection, CREF_LPX_TIMER );
			
		ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );
		Connection->LpxSmp.MaxStallTimeOut.QuadPart		= NdasCurrentTime().QuadPart + Connection->LpxSmp.MaxConnectWaitInterval.QuadPart;
		Connection->LpxSmp.RetransmitTimeOut.QuadPart	= NdasCurrentTime().QuadPart + CalculateRTT(Connection);
		RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );

#if __LPX_STATISTICS__
		Connection->LpxSmp.StatisticsTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.StatisticsInterval.QuadPart;
#endif

		TimeInterval.QuadPart = -Connection->LpxSmp.SmpTimerInterval.QuadPart;
		result = KeSetTimer( &Connection->LpxSmp.SmpTimer,
						     TimeInterval,
							 &Connection->LpxSmp.SmpTimerDpc );

		if (result == TRUE) { // Timer is already in system queue. deference myself.
			
			ASSERT( FALSE );
			LpxDereferenceConnection( "Connect", Connection, CREF_LPX_TIMER );
		}

		ExInterlockedInsertTailList( &Connection->LpxSmp.TramsmitQueue,
									 &RESERVED(packet)->ListEntry,
									 &Connection->LpxSmp.TransmitSpinLock );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		LpxSendDataPackets( Connection );
	
		status = STATUS_PENDING; 
		break;
	}

	default: // !SMP_STATE_CLOSE

		status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Status = status;
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		DebugPrint( 2, ("[LPX]LpxConnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );

		break;
	}

	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	return status;	
}


NTSTATUS
LpxListen (
	IN		PTP_CONNECTION	Connection,
	IN OUT	PIRP	        Irp
   )
{   
	NTSTATUS	status;
	KIRQL		oldIrql;
//	KIRQL		cancelIrql;


	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	DebugPrint( 2, ("LpxListen Connection = %p, Address=%p(%02x:%02x:%02x:%02x:%02x:%02x:%x)\n", 
					 Connection, 
					 Connection->AddressFile->Address, 
					 Connection->AddressFile->Address->NetworkName->Node[0], 
					 Connection->AddressFile->Address->NetworkName->Node[1], 
					 Connection->AddressFile->Address->NetworkName->Node[2], 
					 Connection->AddressFile->Address->NetworkName->Node[3], 
					 Connection->AddressFile->Address->NetworkName->Node[4], 
					 Connection->AddressFile->Address->NetworkName->Node[5],
					 Connection->AddressFile->Address->NetworkName->Port) );	


	// IoAcquireCancelSpinLock( &cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	switch (Connection->LpxSmp.SmpState) {

	case SMP_STATE_CLOSE:

		ASSERT( !FlagOn(Connection->Flags2, CONNECTION_FLAGS2_STOPPING) );
#if 0
		if (Irp->Cancel) {

			status = STATUS_CANCELLED;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
		
			break;
		}
#endif
		Connection->LpxSmp.ListenIrp = Irp;

		IoSetCancelRoutine( Irp, LpxCancelConnection );
		
		LpxChangeState( Connection, SMP_STATE_LISTEN, TRUE );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		status = STATUS_PENDING;

		break;

	default: // !SMP_STATE_CLOSE

		status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Status = status;

		DebugPrint( 2, ("[LPX]LpxListen: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		break;
	}

	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	return status;
}

//
//
//
//	called only from LpxTdiAccept()
//
NDIS_STATUS
LpxAccept (
	IN		PTP_CONNECTION	Connection,
	IN OUT	PIRP	        Irp
   )
{   
	NDIS_STATUS	status;
	KIRQL		cancelIrql;
	KIRQL		oldIrql;

	//ASSERT( FALSE );

	DebugPrint( 2, ("LpxAccept ServicePoint = %p, State = %s\n", 
					 Connection, LpxStateName[Connection->LpxSmp.SmpState]));

	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	switch (Connection->LpxSmp.SmpState) {

	case SMP_STATE_ESTABLISHED:
		
		status = STATUS_SUCCESS;
		
		IoAcquireCancelSpinLock( &cancelIrql );
		IoSetCancelRoutine( Irp, NULL );
		IoReleaseCancelSpinLock( cancelIrql );
		Irp->IoStatus.Status = status;
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );	
		break;
	
	default: // !SMP_STATE_ESTABLISHED

		status = STATUS_UNSUCCESSFUL;
		IoAcquireCancelSpinLock( &cancelIrql );
		IoSetCancelRoutine( Irp, NULL );
		IoReleaseCancelSpinLock( cancelIrql );
		Irp->IoStatus.Status = status;

		DebugPrint( 2, ("[LPX]LpxAccept: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		break;
	}
	return status;
}



NDIS_STATUS
LpxDisconnect (
	IN		PTP_CONNECTION	Connection,
	IN OUT	PIRP	        Irp
	)
{
	NTSTATUS		status;
	KIRQL			oldIrql;
	PNDIS_PACKET	packet;
	PDEVICE_CONTEXT	deviceContext;


	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL) {

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	}

	DebugPrint( 2, ("LpxDisconnect Connection = %p, Connection->LpxSmp.state = %s\n", 
					 Connection, LpxStateName[Connection->LpxSmp.SmpState]) );

	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	Connection->LpxSmp.lDisconnectHandlerCalled++;

	SetFlag( Connection->LpxSmp.Shutdown, SMP_SEND_SHUTDOWN );
	Connection->Status = STATUS_LOCAL_DISCONNECT;

	if (Connection->LpxSmp.SmpState != SMP_STATE_CLOSE) {

		NDAS_ASSERT( Connection->AddressFile );
		deviceContext = Connection->AddressFile->Address->Provider;
	}

	switch (Connection->LpxSmp.SmpState) {

	case SMP_STATE_CLOSE:

		ASSERT( Connection->Status != STATUS_PENDING );

		status = Connection->Status;	
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
	
		DebugPrint( 2, ("return disconnect Irp = %p status = %x\n", Irp, status) );

		break;

	case SMP_STATE_SYN_RECV:		// While connected
	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_CLOSE_WAIT: {

		PCONNECTION_PRIVATE	connectionPrivate = NULL;

		Connection->LpxSmp.DisconnectIrp = Irp;
		DebugPrint(2, ("Connection = %p Connection->LpxSmp.DisconnectIrp = %p\n", Connection, Connection->LpxSmp.DisconnectIrp) );

		IoSetCancelRoutine( Irp, LpxCancelConnection );

		if (Connection->LpxSmp.SmpState == SMP_STATE_CLOSE_WAIT) {

			KIRQL		    oldIrqlTimeCnt;

			ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, &oldIrqlTimeCnt );
			Connection->LpxSmp.LastAckTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.LastAckInterval.QuadPart;
			RELEASE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, oldIrqlTimeCnt );

			LpxChangeState( Connection, SMP_STATE_LAST_ACK, TRUE );
		
		} else {

			LpxChangeState( Connection, SMP_STATE_FIN_WAIT1, TRUE );	    
		}

		Connection->LpxSmp.FinSequence = SHORT_SEQNUM( Connection->LpxSmp.Sequence );
		
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		packet = PrepareSmpNonDataPacket( Connection, SMP_TYPE_DISCON );

		if (!packet) {

			DebugPrint( 0, ("[LPX] LpxDisconnect ERROR\n") );

			KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
			LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
			KeLowerIrql ( oldIrql );

			status = STATUS_PENDING;
			break;
		}

		ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

		if (FlagOn(Connection->Flags2, CONNECTION_FLAGS2_STOPPING)) {

			if (Connection->LpxSmp.DisconnectIrp) {

				Connection->LpxSmp.DisconnectIrp = NULL;

				DebugPrint( 2, ("CONNECTION_FLAGS2_STOPPING Connection->LpxSmp.DisconnectIrp = %p\n", Connection->LpxSmp.DisconnectIrp) );

				status = STATUS_REMOTE_DISCONNECT;

				Irp->IoStatus.Status = status;
				Irp->IoStatus.Information = 0;

			} else {
				
				status = STATUS_PENDING;
			}

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			PacketFree( Connection->AddressFile->Address->Provider, packet );

			DebugPrint( 0, ("[LPX] LpxDisconnect ERROR. Already stopping.\n") );

			status = STATUS_PENDING;
			break;
		}

		ExInterlockedInsertTailList( &Connection->LpxSmp.TramsmitQueue,
									 &RESERVED(packet)->ListEntry,
									 &Connection->LpxSmp.TransmitSpinLock );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		LpxSendDataPackets( Connection );
	
		status = STATUS_PENDING; 
		break;
	}

	case SMP_STATE_LISTEN:
	case SMP_STATE_SYN_SENT:		// Disconnect while connecting...
		
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
		LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
		KeLowerIrql ( oldIrql );

		status = STATUS_SUCCESS;	
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;

		DebugPrint( 2, ("[LPX]LpxDisconnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );

		break;

	default: // Do nothing. Already disconnected or disconnection is under progress.

		ASSERT( FALSE );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
		LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
		KeLowerIrql ( oldIrql );

		status = STATUS_SUCCESS;	 
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;

		DebugPrint( 2, ("[LPX]LpxDisconnect: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );

		break;
	}

	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL) {

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	}

	return status;
}


NTSTATUS
LpxSend (
	IN		PTP_CONNECTION  Connection,
	IN OUT	PIRP	        Irp
	)
{
	NTSTATUS		status;
	KIRQL			oldIrql;
//	KIRQL			cancelIrql;
	PDEVICE_CONTEXT	deviceContext = Connection->AddressFile->Address->Provider;


	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (Connection->LpxSmp.SmpState != SMP_STATE_ESTABLISHED || FlagOn(Connection->LpxSmp.Shutdown, SMP_SEND_SHUTDOWN))
		DebugPrint( 2, ("LpxSend Connection = %p, Connection->LpxSmp.state = %s\n", 
						 Connection, LpxStateName[Connection->LpxSmp.SmpState]) );

	// IoAcquireCancelSpinLock( &cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	if (FlagOn(Connection->LpxSmp.Shutdown, SMP_SEND_SHUTDOWN)) {

		status = Connection->Status;

		if (NT_SUCCESS(status)) {

			NDAS_ASSERT(FALSE);			
			status = STATUS_LOCAL_DISCONNECT;
		}

		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		DebugPrint( 0, ("LpxSend STATUS_LOCAL_DISCONNECT status = %x\n", status) );
			
		return status;
	}

	switch (Connection->LpxSmp.SmpState) {

	case SMP_STATE_ESTABLISHED: 
	case SMP_STATE_CLOSE_WAIT: {
	
		PIO_STACK_LOCATION	irpSp;

		ULONG		        remainedUserDataLength;
		PUCHAR		        userDataOffSet;

		PNDIS_PACKET		packet;
		PCONNECTION_PRIVATE	connectionPrivate;

		LIST_ENTRY			packetQueue;

		InitializeListHead(&packetQueue);

		irpSp = IoGetCurrentIrpStackLocation( Irp );
		ASSERT(irpSp);

		if (IRP_SEND_LENGTH(irpSp) == 0) {

			status = STATUS_SUCCESS;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

			DebugPrint( 0, ("LpxSend userDataLength = 0 !!!!!!!!!!! \n") );

			break;
		}

		if (!Irp->MdlAddress) {

			NDAS_ASSERT(FALSE);

			status = STATUS_INVALID_PARAMETER;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			DebugPrint( 0, ("LpxSend Irp->MdlAddress = NULL !!!!!!!!!!! \n") );

			break;
		}

		deviceContext = (PDEVICE_CONTEXT)Connection->AddressFile->Address->Provider;
		LpxReferenceConnection( "LpxSend", Connection, CREF_SEND_IRP );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		userDataOffSet		   = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority );
		remainedUserDataLength = IRP_SEND_LENGTH( irpSp ); 

		NDAS_ASSERT( remainedUserDataLength );
		status = STATUS_SUCCESS;

		while (remainedUserDataLength) {

			USHORT copied;

			copied = (USHORT)(deviceContext->MaxUserData - sizeof(LPX_HEADER));

#if __LPX_OPTION_ADDRESSS__
			
			if (FlagOn(Connection->LpxSmp.Option, LPX_OPTION_SOURCE_ADDRESS))
				copied -= ETHERNET_ADDRESS_LENGTH;

			if (FlagOn(Connection->LpxSmp.Option, LPX_OPTION_DESTINATION_ADDRESS))
				copied -= ETHERNET_ADDRESS_LENGTH;
#endif

			copied = (USHORT)((copied < remainedUserDataLength) ? copied : remainedUserDataLength);

			status = SendPacketAlloc( deviceContext,
									  Connection->AddressFile->Address,
									  Connection->LpxSmp.DestinationAddress.Node,
									  userDataOffSet,
									  copied,
									  NULL,
									  Connection->LpxSmp.Option,
									  &packet );

			if (!NT_SUCCESS(status)) {

				DebugPrint( 2, ("[LPX]LpxSend: packet == NULL\n") );
				SmpPrintState( 1, "[LPX]LpxSend: PacketAlloc", Connection );
				break;
			}
			
			InsertTailList( &packetQueue, &RESERVED(packet)->ListEntry );

			remainedUserDataLength -= copied;
			userDataOffSet += copied;
		}

		if (IsListEmpty(&packetQueue)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			// IoAcquireCancelSpinLock( &cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			// IoSetCancelRoutine( Irp, NULL );
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

			KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
			LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
			KeLowerIrql ( oldIrql );

			LpxDereferenceConnectionMacro( "Removing Connection", Connection, CREF_SEND_IRP );  

			break;
		}

		ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

		if (FlagOn(Connection->LpxSmp.Shutdown, SMP_SEND_SHUTDOWN)) {

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

			while (!IsListEmpty(&packetQueue)) {

				PLIST_ENTRY		packetListEntry;
				PLPX_RESERVED	reserved;

				packetListEntry = RemoveHeadList(&packetQueue);
				reserved		= CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
				packet			= reserved->Packet;

				PacketFree( Connection->AddressFile->Address->Provider, packet );
			}

			status = Connection->Status;

			if (NT_SUCCESS(status)) {

				NDAS_ASSERT(FALSE);			
				status = STATUS_LOCAL_DISCONNECT;
			}

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			// IoAcquireCancelSpinLock( &cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			// IoSetCancelRoutine( Irp, NULL );
			//IoReleaseCancelSpinLock( cancelIrql );  // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

			LpxDereferenceConnectionMacro( "Removing Connection", Connection, CREF_SEND_IRP );  
			
			break;
		} 

		NDAS_ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_ESTABLISHED || Connection->LpxSmp.SmpState == SMP_STATE_CLOSE_WAIT );

		IoSetCancelRoutine( Irp, LpxCancelSend );

		connectionPrivate = ExAllocatePoolWithTag( NonPagedPool, sizeof(CONNECTION_PRIVATE), LPX_MEM_TAG_PRIVATE );

		if (connectionPrivate) {

			RtlZeroMemory( connectionPrivate, sizeof(CONNECTION_PRIVATE) );

			(PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0] = connectionPrivate;

			KeQuerySystemTime( &connectionPrivate->CurrentTime );
			connectionPrivate->Connection = Connection;
			LpxReferenceConnection( "LpxSend", Connection, CREF_LPX_PRIVATE );	
		}

		Irp->IoStatus.Information	= IRP_SEND_LENGTH(irpSp) - remainedUserDataLength;
		Irp->IoStatus.Status		= STATUS_SUCCESS;

		IRP_SEND_REFCOUNT(irpSp) = 1;
		IRP_SEND_IRP(irpSp)		 = Irp;

		while (!IsListEmpty(&packetQueue)) {

			PLPX_HEADER	lpxHeader;
			USHORT		sequence;

			PLIST_ENTRY		packetListEntry;
			PLPX_RESERVED	reserved;

			packetListEntry = RemoveHeadList(&packetQueue);
			reserved		= CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
			packet			= reserved->Packet;

			lpxHeader = &RESERVED(packet)->LpxHeader;

			lpxHeader->LpxType		    = LPX_TYPE_STREAM;
			lpxHeader->DestinationPort	= Connection->LpxSmp.DestinationAddress.Port;
			lpxHeader->SourcePort		= Connection->AddressFile->Address->NetworkName->Port;
			lpxHeader->AckSequence		= HTONS( SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence) );
			lpxHeader->ServerTag		= Connection->LpxSmp.ServerTag;

			lpxHeader->Lsctl			= HTONS( LSCTL_DATA | LSCTL_ACK );

			sequence = Connection->LpxSmp.Sequence;
			Connection->LpxSmp.Sequence ++;

			lpxHeader->Sequence			= HTONS(sequence);

			RESERVED(packet)->IrpSp = irpSp;
			LpxReferenceSendIrp( "Packetize", irpSp, RREF_PACKET );

			ExInterlockedInsertTailList( &Connection->LpxSmp.TramsmitQueue,
										 &RESERVED(packet)->ListEntry,
										 &Connection->LpxSmp.TransmitSpinLock );
		}

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		LpxSendDataPackets( Connection );

		LpxDereferenceSendIrp( "Complete", irpSp, RREF_CREATION );	 // remove creation reference.
		LpxDereferenceConnectionMacro( "Removing Connection", Connection, CREF_SEND_IRP );  
	
		status = STATUS_PENDING; 
		break;
	}

	default: // !SMP_STATE_ESTABLISHED

		status = Connection->Status;

		if (NT_SUCCESS(status)) {

			NDAS_ASSERT(FALSE);			
			status = STATUS_LOCAL_DISCONNECT;
		}

		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		DebugPrint( 2, ("[LPX] LpxSend: not ESTABLISHED state.(in %s)\n", LpxStateName[Connection->LpxSmp.SmpState]) );
		break;
	}

	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	return status;
}


NTSTATUS
LpxRecv (
	IN PTP_CONNECTION	Connection,
	IN OUT	PIRP        Irp
	)
{
	NTSTATUS	status;
	KIRQL		oldIrql;
//	KIRQL		cancelIrql;


	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	DebugPrint( 4, ("LpxRecv: Connection = %p, Connection->LpxSmp.state = %s\n", 
					 Connection, LpxStateName[Connection->LpxSmp.SmpState]) );

	// IoAcquireCancelSpinLock( &cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	switch (Connection->LpxSmp.SmpState) {

	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_CLOSE_WAIT: {

		PIO_STACK_LOCATION	irpSp;
		ULONG		        userDataLength;
		PUCHAR		        userData = NULL;
		PCONNECTION_PRIVATE	connectionPrivate;


		irpSp = IoGetCurrentIrpStackLocation(Irp);

		userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 

		if (Irp->MdlAddress) {

			userData = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, HighPagePriority );
		
		} else {

			userData = NULL;
		}

		if (userDataLength == 0) {

			status = STATUS_SUCCESS;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;
			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			DebugPrint( 0, ("LPX_RECEIVE userDataLength = 0 !!!!!!!!!!! \n") );
			break;
		}

		if (userData == NULL) {

			ASSERT( FALSE );
			status = STATUS_INVALID_PARAMETER;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;			
			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			DebugPrint(0, ("LPX_RECEIVE: userData is NULL !!!!!!!!!!!!!!!\n") );
			break;
		}
#if 0
		if (Irp->Cancel) {

			status = STATUS_CANCELLED;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;

			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

			break;
		}
#endif

#if __LPX_STATISTICS__
		Connection->LpxSmp.LastDataPacketArrived = NdasCurrentTime(); // Assume packet is lost if response is not in timeout.
#endif

		connectionPrivate = ExAllocatePoolWithTag( NonPagedPool, sizeof(CONNECTION_PRIVATE), LPX_MEM_TAG_PRIVATE );

		if (connectionPrivate) {

			RtlZeroMemory( connectionPrivate, sizeof(CONNECTION_PRIVATE) );
			(PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0] = connectionPrivate;

			KeQuerySystemTime( &connectionPrivate->CurrentTime );
			connectionPrivate->Connection = Connection;
			LpxReferenceConnection( "LpxRecv", Connection, CREF_LPX_PRIVATE );
		
		}

		Irp->IoStatus.Information = 0;

		ExInterlockedInsertTailList( &Connection->LpxSmp.ReceiveIrpQueue,
									 &Irp->Tail.Overlay.ListEntry,
									 &Connection->LpxSmp.ReceiveIrpQSpinLock );
		IoSetCancelRoutine( Irp, LpxCancelReceive );

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.

		status = LpxSmpProcessReceivePacket( Connection, NULL );

		status = STATUS_PENDING;
		
		break;
	}

	default: 

		if (Connection->Status == STATUS_PENDING) {

			NDAS_ASSERT( FlagOn(Connection->LpxSmp.Shutdown, SMP_RECEIVE_SHUTDOWN) );			

			status = STATUS_REMOTE_DISCONNECT;
			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = 0;
			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
			// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
			DebugPrint( 2, ("[LPX]LpxRecv: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );

			break;
		}

		status = Connection->Status;

		if (NT_SUCCESS(status)) {

			NDAS_ASSERT(FALSE);			
			status = STATUS_REMOTE_DISCONNECT;
		}

		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		// IoReleaseCancelSpinLock( cancelIrql ); // Refer to the OSR article "Setting A Cancel Routine" in DDK document.
		DebugPrint( 2, ("[LPX]LpxRecv: IRP %p completed with error: NTSTATUS:%08lx.\n ", Irp, status) );

		break;
	}

	IF_LPXDBG (LPX_DEBUG_CURRENT_IRQL)
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	return status;
}


NDIS_STATUS
LpxSendDatagram (
	IN		PTP_ADDRESS Address,
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


	DebugPrint( 3, ("LpxSendDatagram\n") );

	irpSp = IoGetCurrentIrpStackLocation( Irp );

	parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);

	if (parameters->SendLength > LPX_MAX_DATAGRAM_SIZE) {

		ASSERT( FALSE );
		status = STATUS_PORT_MESSAGE_TOO_LONG;
		Irp->IoStatus.Status = status;
		return status;
	}

	deviceContext = (PDEVICE_CONTEXT)Address->Provider;

	IRP_SEND_IRP(irpSp) = Irp;
	IRP_SEND_REFCOUNT(irpSp) = 1;

	userDataLength = parameters->SendLength;

	if (Irp->MdlAddress) {

		userData = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, HighPagePriority );
		
	} else {

		userData = NULL;
	}


	transportAddress = (TRANSPORT_ADDRESS UNALIGNED *)parameters->SendDatagramInformation->RemoteAddress;
	remoteAddress = (PLPX_ADDRESS)&transportAddress->Address[0].Address[0];

	irpSp = IoGetCurrentIrpStackLocation (Irp);
	parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);

	/*while (userDataLength)*/ {

		USHORT	        copy;
		PLPX_HEADER	lpxHeader;
		KIRQL			oldirql;


		copy = (USHORT)(deviceContext->MaxUserData - sizeof(LPX_HEADER));

		if(copy > userDataLength)
			copy = (USHORT)userDataLength;


		status = SendPacketAlloc( deviceContext,
								  Address,
								  remoteAddress->Node,
								  userData,
								  copy,
								  irpSp,
								  0,
								  &packet );


		if (!NT_SUCCESS(status)) {

			DebugPrint( 0, ("packet == NULL\n") );
			LpxDereferenceSendIrp ( "Packetize", irpSp, RREF_PACKET );
			return STATUS_PENDING; 
		}

		LpxReferenceSendIrp( "Packetize", irpSp, RREF_PACKET );

		DebugPrint( 4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy) );
		userDataLength -= copy;

		DebugPrint( 3,("remoteAddress %02X%02X%02X%02X%02X%02X:%04X\n",
			            remoteAddress->Node[0],
						remoteAddress->Node[1],
						remoteAddress->Node[2],
						remoteAddress->Node[3],
						remoteAddress->Node[4],
						remoteAddress->Node[5],
						NTOHS(remoteAddress->Port)) );

		lpxHeader = &RESERVED(packet)->LpxHeader;

		lpxHeader->LpxType			= LPX_TYPE_DATAGRAM;
		lpxHeader->DestinationPort	= remoteAddress->Port;
		lpxHeader->SourcePort		= Address->NetworkName->Port;
		lpxHeader->MessageId		= HTONS((USHORT)LPX_HOST_DGMSG_ID);
		lpxHeader->MessageLength	= HTONS((USHORT)copy);
		lpxHeader->FragmentId		= 0;
		lpxHeader->FragmentLength	= 0;
		lpxHeader->ResevedU1		= 0;

		INCREASE_SENDING_THREAD_COUNT( deviceContext );	
		ACQUIRE_SPIN_LOCK( &deviceContext->SpinLock, &oldirql );

		if (FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) && !FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP)) {

			ASSERT( deviceContext->NdisBindingHandle );

			NdisSend( &status, deviceContext->NdisBindingHandle, packet );
		
		} else {

			status = NDIS_STATUS_NO_CABLE;
		}

		RELEASE_SPIN_LOCK( &deviceContext->SpinLock, oldirql );
		DECREASE_SENDING_THREAD_COUNT( deviceContext );	
				
		if (status == NDIS_STATUS_SUCCESS) {

			RESERVED(packet)->NdisStatus = status;
			PacketFree( deviceContext, packet );
			userData += copy;
			//continue;
		
		} else if (status == NDIS_STATUS_PENDING) {

			status = STATUS_SUCCESS;
			userData += copy;
			//continue;
		
		} else {

			DebugPrint( 3, ("LpxSendDatagram: status = %x\n", status) );

			RESERVED(packet)->NdisStatus = status;
			PacketFree( deviceContext, packet );
			//break;
		}
	}

	ASSERT( status != STATUS_PENDING );
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = parameters->SendLength;
	LpxDereferenceSendIrp( "Packetize", irpSp, RREF_PACKET );

	return STATUS_PENDING; 
}


VOID 
LpxCallUserReceiveHandler (
	IN PTP_CONNECTION	Connection
	)
{
	if (Connection && Connection->AddressFile) {

		ULONG		ReceiveFlags;
		NTSTATUS	status;
		PIRP		irp = NULL;
		ULONG		indicateBytesTransferred;
	

		ReceiveFlags = TDI_RECEIVE_AT_DISPATCH_LEVEL | TDI_RECEIVE_ENTIRE_MESSAGE | TDI_RECEIVE_NO_RESPONSE_EXP;

		DebugPrint( 3, ("before ReceiveHandler Connection->AddressFile->RegisteredReceiveHandler=%d\n",
						 Connection->AddressFile->RegisteredReceiveHandler) );

		status = (*Connection->AddressFile->ReceiveHandler) ( 
						Connection->AddressFile->ReceiveHandlerContext,
						Connection->Context,
						ReceiveFlags,
						0,
						0,             // BytesAvailable
						&indicateBytesTransferred,
						NULL,
						NULL );

		DebugPrint( 3, ("status = %x, Irp = %p\n", status, irp) );
	}

	return;
}


VOID
LpxCancelConnection (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PIRP				Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to cancel a connect
    or a listen. It is simple since there can only be one of these
    active on a connection; we just stop the connection, the IRP
    will get completed as part of normal session teardown.

    NOTE: This routine is called with the CancelSpinLock held and
    is responsible for releasing it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    none.

--*/

{
    KIRQL				oldirql;
    PIO_STACK_LOCATION	IrpSp;
    PTP_CONNECTION		Connection;

	BOOLEAN				cancelable = FALSE;

    UNREFERENCED_PARAMETER( DeviceObject );

	IoReleaseCancelSpinLock( Irp->CancelIrql );

    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    NDAS_ASSERT( (IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
				(IrpSp->MinorFunction == TDI_CONNECT || IrpSp->MinorFunction == TDI_LISTEN || IrpSp->MinorFunction == TDI_DISCONNECT) );

    Connection = IrpSp->FileObject->FsContext;

    // Since this IRP is still in the cancellable state, we know
    // that the connection is still around (although it may be in
    // the process of being torn down).
 
    ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldirql );

    LpxReferenceConnection( "Cancelling Send", Connection, CREF_TEMP );

	if (IrpSp->MinorFunction == TDI_CONNECT) {

        ASSERT( Connection->LpxSmp.ConnectIrp == Irp );

		if (Connection->LpxSmp.ConnectIrp) {
		
			cancelable = TRUE;
			Connection->LpxSmp.ConnectIrp = NULL;
		}
	
	} else if (IrpSp->MinorFunction == TDI_LISTEN) {

		ASSERT( Connection->LpxSmp.ListenIrp == Irp );

		if (Connection->LpxSmp.ListenIrp) {
		
			cancelable = TRUE;
			Connection->LpxSmp.ListenIrp = NULL;
		}
	
	} else if (IrpSp->MinorFunction == TDI_DISCONNECT) {

		ASSERT( Connection->LpxSmp.DisconnectIrp == Irp );

		if (Connection->LpxSmp.DisconnectIrp) {
		
			cancelable = TRUE;
			Connection->LpxSmp.DisconnectIrp = NULL;
		}
	}

	RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldirql );

//	IoSetCancelRoutine( Irp, NULL ); // Don't need it. IO manager does it.

    if (cancelable) {

        KeRaiseIrql( DISPATCH_LEVEL, &oldirql );

		Irp->IoStatus.Status = STATUS_CANCELLED;
		Irp->IoStatus.Information = 0;

		LpxIoCompleteRequest( Irp, IO_NETWORK_INCREMENT );

		//LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
       
		KeLowerIrql( oldirql );
    }

    LpxDereferenceConnection( "Cancel done", Connection, CREF_TEMP );
}


VOID
LpxCancelSend (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp
	)
{
	KIRQL	oldIrql;
	PIO_STACK_LOCATION	irpSp;
	PTP_CONNECTION		connection;
	PLIST_ENTRY			listEntry;
	PLIST_ENTRY			nextEntry;
	PLPX_RESERVED		reserved;
	PNDIS_PACKET		packet;
	PIO_STACK_LOCATION	irpSp2;
	PIRP	            irp2;

	DebugPrint( 2, ("LpxStopConnection:TramsmitQueue packet deleted\n") );

	UNREFERENCED_PARAMETER( DeviceObject );

	IoReleaseCancelSpinLock( Irp->CancelIrql ); 

	DebugPrint( 2, ("LpxCancelSend\n") );

	irpSp = IoGetCurrentIrpStackLocation( Irp );

	NDAS_ASSERT( irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL && irpSp->MinorFunction == TDI_SEND );

	connection = irpSp->FileObject->FsContext;

	// Set canceled status to the IRP
	// NOTE: the IRP status value could change by error packets

	Irp->IoStatus.Status = STATUS_CANCELLED;
	Irp->IoStatus.Information = 0;

	ACQUIRE_C_SPIN_LOCK (&connection->SpinLock, &oldIrql);

	ACQUIRE_DPC_SPIN_LOCK( &connection->LpxSmp.TransmitSpinLock);

	if (!IsListEmpty(&connection->LpxSmp.TramsmitQueue)) {
		
		connection->LpxSmp.CanceledByUser = TRUE;
	}

	for (listEntry = connection->LpxSmp.TramsmitQueue.Flink;
		 listEntry != &connection->LpxSmp.TramsmitQueue;
		 listEntry = nextEntry) {

		nextEntry = listEntry->Flink;

		reserved= CONTAINING_RECORD( listEntry, LPX_RESERVED, ListEntry );
		packet = reserved->Packet;

		irpSp2 = reserved->IrpSp;

		if (irpSp2 != NULL) {

			irp2 = IRP_SEND_IRP(irpSp2);

			// Free a packet for the IRP.
		
			if (irp2 == Irp) {

				// set canceled status.
		
				reserved->NdisStatus = STATUS_CANCELLED;
				RemoveEntryList(&reserved->ListEntry);
				PacketFree( connection->AddressFile->Address->Provider, packet );
			}
		}
	}

	RELEASE_DPC_SPIN_LOCK( &connection->LpxSmp.TransmitSpinLock );

	LpxReferenceConnection( "Canceling Send", connection, CREF_TEMP );

	RELEASE_C_SPIN_LOCK( &connection->SpinLock, oldIrql );

	//KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
	//LpxStopConnection( connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
	//KeLowerIrql( oldIrql );

	LpxDereferenceConnection( "Canceling Send Done", connection, CREF_TEMP );
}


VOID
LpxCancelReceive (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp
	)
{
    KIRQL				oldIrql;
    PIO_STACK_LOCATION	irpSp;
    PTP_CONNECTION		connection;
	PLIST_ENTRY			listEntry;
	PIRP				canceledIrp;


	UNREFERENCED_PARAMETER( DeviceObject );

	IoReleaseCancelSpinLock( Irp->CancelIrql );

	DebugPrint( 2, ("LpxCancelReceive\n") );

    irpSp = IoGetCurrentIrpStackLocation (Irp);

    NDAS_ASSERT( irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL && irpSp->MinorFunction == TDI_RECEIVE );

    connection = irpSp->FileObject->FsContext;


	ACQUIRE_C_SPIN_LOCK( &connection->SpinLock, &oldIrql );

	// Find a canceled IRP from the front of the queue
	// To know why canceled IRP is searched in the queue,
	// reference 'Supporting Cancel Processing in a Kernel Mode Driver' NTDDK document

	ACQUIRE_DPC_SPIN_LOCK( &connection->LpxSmp.ReceiveIrpQSpinLock );

	canceledIrp = NULL;

	for (listEntry = connection->LpxSmp.ReceiveIrpQueue.Flink;
		 listEntry != &connection->LpxSmp.ReceiveIrpQueue;
		 listEntry = listEntry->Flink) {

		canceledIrp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);

		if (canceledIrp->Cancel) {

			break;
		}

		canceledIrp = NULL;
	}

	if (canceledIrp) {

		DebugPrint( 2, ("Found cancelable receive IRP\n") );
		RemoveEntryList( &canceledIrp->Tail.Overlay.ListEntry );
	
	} else {
	
		DebugPrint( 2, ("Cound not find cancelable receive IRP\n") );
	}

	RELEASE_DPC_SPIN_LOCK( &connection->LpxSmp.ReceiveIrpQSpinLock );

	LpxReferenceConnection( "Cancelling Recv", connection, CREF_TEMP );

	RELEASE_C_SPIN_LOCK( &connection->SpinLock, oldIrql );

	if (canceledIrp) {

		canceledIrp->IoStatus.Status = STATUS_CANCELLED;
		canceledIrp->IoStatus.Information = 0;

		LpxIoCompleteRequest( canceledIrp, IO_NETWORK_INCREMENT );
	}

	//KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
	//LpxStopConnection( connection, STATUS_LOCAL_DISCONNECT );   // prevent indication to clients
    //KeLowerIrql( oldIrql );

    LpxDereferenceConnection( "Cancel Recv done", connection, CREF_TEMP );
}


// To do: Change NewState's type to LPX_STATE
VOID 
LpxChangeState (
	IN PTP_CONNECTION	Connection,
	IN SMP_STATE		NewState,
	IN BOOLEAN			Locked
	) 
{
	KIRQL irql;
	PCHAR prevStateName;
	
	IF_LPXDBG (LPX_DEBUG_ERROR)
		ASSERT( Locked );
	
	if (NewState >= SMP_STATE_LAST) {

		ASSERT( FALSE );
		return;
	}


	if (!Locked) {

		ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &irql);
	}
	
	prevStateName = LpxStateName[Connection->LpxSmp.SmpState];
	
	Connection->LpxSmp.SmpState = NewState;

	DebugPrint( 2, ("LPX:Con %p: %s to %s(<-> %02x:%02x:%02x:%02x:%02x:%02x:%d)\n",
					 Connection, 
					 prevStateName, 
					 LpxStateName[Connection->LpxSmp.SmpState],
					 Connection->LpxSmp.DestinationAddress.Node[0], 
					 Connection->LpxSmp.DestinationAddress.Node[1], 
					 Connection->LpxSmp.DestinationAddress.Node[2], 
					 Connection->LpxSmp.DestinationAddress.Node[3], 
					 Connection->LpxSmp.DestinationAddress.Node[4], 
					 Connection->LpxSmp.DestinationAddress.Node[5],
					 Connection->LpxSmp.DestinationAddress.Port) );

	if (!Locked) {
	
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, irql );
	}
}


VOID
LpxSendDataPackets (
	IN PTP_CONNECTION	Connection
	)
{
	KIRQL			oldIrpql;
	PLIST_ENTRY		packetListEntry;
	PNDIS_PACKET	packet;
	PLPX_RESERVED	reserved;
	LONG		    cloned;

	UINT			numberOfPackets;


	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrpql );
	ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );
	
	if (Connection->LpxSmp.PacketArrayUsed) {

		RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrpql );
		return;
	}

	Connection->LpxSmp.PacketArrayUsed = TRUE;

loop:

	numberOfPackets = 0;

	if (!IsListEmpty(&Connection->LpxSmp.TramsmitQueue)) {

		if (Connection->LpxSmp.RetransmitTimeOut.QuadPart <= NdasCurrentTime().QuadPart) {
		
			packetListEntry = Connection->LpxSmp.TramsmitQueue.Flink;
			reserved		= CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
			packet			= reserved->Packet;

			NDAS_ASSERT( RESERVED(packet) == reserved );

			if (((SHORT)(NTOHS(RESERVED(packet)->LpxHeader.Sequence) - Connection->LpxSmp.NextTransmitSequece)) < 0) {
				
				RESERVED(packet)->LpxHeader.AckSequence = HTONS( SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence) );

				// Remove continuing flag from retransmitting packet.
				RESERVED(packet)->LpxHeader.Option &= ~LPX_OPTION_PACKETS_CONTINUE_BIT;

				InterlockedIncrement( &Connection->LpxSmp.RetransmitCount );
				Connection->LpxSmp.RetransmitTimeOut.QuadPart = NdasCurrentTime().QuadPart + CalculateRTT( Connection );

				InterlockedIncrement( &Connection->LpxSmp.TransportStats.Retransmits );
				InterlockedIncrement( &RESERVED(packet)->Retransmits );
				Connection->LpxSmp.NextTransmitSequece = NTOHS(RESERVED(packet)->LpxHeader.Sequence) + 1;

				if (NTOHS(RESERVED(packet)->LpxHeader.Lsctl) & LSCTL_CONNREQ) {

					RESERVED(packet)->LpxHeader.SourcePort = Connection->AddressFile->Address->NetworkName->Port;
				}

				PacketCopy( packet, &cloned );

				Connection->LpxSmp.NumberOfSendRetransmission ++;

				if (cloned == 1) {

					Connection->LpxSmp.PacketArray[numberOfPackets++] = packet;

					DebugPrint( 2, ("[LPX] Retransmit, Connection->LpxSmp.TransportStats.Retransmits = %d\n", 
								    Connection->LpxSmp.TransportStats.Retransmits) );

				} else {

					DebugPrint( 1, ("[LPX] Not yet Sended Cloned Connection->LpxSmp.TransportStats.Retransmits = %d\n", 
									Connection->LpxSmp.TransportStats.Retransmits) );

					SmpPrintState( 1, "Not yet Sended Cloned", Connection );
					PacketFree( Connection->AddressFile->Address->Provider, packet );
				}
			}
		} 
		
		if (Connection->LpxSmp.RetransmitCount == 0) {

			SmpPrintState( 4, "Ret1", Connection );

			for (packetListEntry = Connection->LpxSmp.TramsmitQueue.Flink;
				 packetListEntry != &Connection->LpxSmp.TramsmitQueue;
				 packetListEntry = packetListEntry->Flink) {

				reserved = CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
				packet	 = reserved->Packet;

				ASSERT( RESERVED(packet) == reserved );

				if (((SHORT)(NTOHS(RESERVED(packet)->LpxHeader.Sequence) - Connection->LpxSmp.NextTransmitSequece)) < 0) {

					continue;
				}

#if 0
				if (!SmpSendTest(Connection, packet)) {

					break;
				}
#endif

				PacketCopy( packet, &cloned );

				Connection->LpxSmp.NextTransmitSequece = NTOHS(RESERVED(packet)->LpxHeader.Sequence) + 1;
	
				if (cloned == 1) {
				 
					RESERVED(packet)->LpxHeader.AckSequence = HTONS( SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence) );
					Connection->LpxSmp.PacketArray[numberOfPackets++] = packet;
					
				} else {
				
					SmpPrintState( 1, "Not yet Sended", Connection );
					PacketFree( Connection->AddressFile->Address->Provider, packet );

					break;
				}

				if (numberOfPackets == LPX_MAX_PACKET_ARRAY) {

					break;
				}
			}	

			if (numberOfPackets != 0) {

				ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );
				Connection->LpxSmp.RetransmitTimeOut.QuadPart = NdasCurrentTime().QuadPart + CalculateRTT(Connection);
				RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );
			}
		}
	} 

	if (numberOfPackets == 0) {

		Connection->LpxSmp.PacketArrayUsed = FALSE;
	}

	RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );
	RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrpql );

	if (numberOfPackets == 0) {

		return;
	}

	LpxSendPackets( Connection, Connection->LpxSmp.PacketArray, numberOfPackets );

	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrpql );
	ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );

	if (Connection->LpxSmp.RetransmitCount == 0 && Connection->LpxSmp.NextTransmitSequece != Connection->LpxSmp.Sequence) {

		DbgPrint( "goto loop\n" );
		goto loop;
	}
	
	Connection->LpxSmp.PacketArrayUsed = FALSE;

	RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );
	RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrpql );
}


VOID
SmpTimerDpcRoutine (
	IN PKDPC			Dpc,
	IN PTP_CONNECTION	Connection,
	IN PVOID			Junk1,
	IN PVOID			Junk2
	)
{
	BOOLEAN			    result;
	LARGE_INTEGER		timeInterval = {0,0};

	DebugPrint( 5, ("SmpTimerDpcRoutine ServicePoint = %p\n", Connection) );

	UNREFERENCED_PARAMETER( Dpc );
	UNREFERENCED_PARAMETER( Junk1 );
	UNREFERENCED_PARAMETER( Junk2 );


	ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

	if (Connection->LpxSmp.MaxStallTimeOut.QuadPart < NdasCurrentTime().QuadPart) {

		DebugPrint( 2,("SmpTimerDpcRoutine: no response from Remote. stop connection\n") );

		//if (Connection->LpxSmp.SmpState == SMP_STATE_ESTABLISHED)
		//	ASSERT( FALSE );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		LpxStopConnection( Connection, STATUS_IO_TIMEOUT );

		LpxDereferenceConnection( "Timer", Connection, CREF_LPX_TIMER );

		return;
	}

	if (Connection->LpxSmp.SmpState == SMP_STATE_CLOSE) {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		LpxDereferenceConnection( "SmpTimerDpcRoutineRequest", Connection, CREF_LPX_TIMER );

		return;
	}

	//
	//	do condition check
	//

	switch (Connection->LpxSmp.SmpState) {
		
	case SMP_STATE_TIME_WAIT:
		
		if (Connection->LpxSmp.TimeWaitTimeOut.QuadPart <= NdasCurrentTime().QuadPart) {

			DebugPrint( 2, ("[LPX] SmpTimerDpcRoutine: TimeWaitTimeOut Connection = %p\n", Connection) );

			NDAS_ASSERT( Connection->LpxSmp.DisconnectIrp == NULL );			

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );
			LpxDereferenceConnection( "Timer", Connection, CREF_LPX_TIMER );

			return;
		}
		
		goto out;

	case SMP_STATE_CLOSING:
	case SMP_STATE_LAST_ACK:

		if (Connection->LpxSmp.LastAckTimeOut.QuadPart <= NdasCurrentTime().QuadPart) {

			DebugPrint( 2, ("[LPX] SmpTimerDpcRoutine: LastAckTimeOut Connection = %p\n", Connection) );

			if (!Connection->LpxSmp.DisconnectIrp) {

				ASSERT( FALSE );    
			}

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			LpxStopConnection( Connection, STATUS_LOCAL_DISCONNECT );
			LpxDereferenceConnection( "Timer", Connection, CREF_LPX_TIMER );

			return;
		}

		break;

	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_SYN_SENT:
	case SMP_STATE_SYN_RECV:
	case SMP_STATE_FIN_WAIT1:
	case SMP_STATE_FIN_WAIT2:
	case SMP_STATE_CLOSE_WAIT:

		break;
		
	default:

		NDAS_ASSERT(FALSE);	
		break;
	}

	if (Connection->LpxSmp.AliveTimeOut.QuadPart < NdasCurrentTime().QuadPart) { // alive message

		PNDIS_PACKET	packet;

		DebugPrint( 3, ("[LPX]SmpTimerDpcRoutine: Send ACKREQ. SP : %p, S: 0x%x, RS: 0x%x\n", 
						 Connection, Connection->LpxSmp.Sequence, SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) );

		Connection->LpxSmp.AliveTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.AliveInterval.QuadPart;
		
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		packet = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACKREQ );
		
		if (packet)
			LpxSendPacket( Connection, packet, SMP_TYPE_ACKREQ );

		ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );	
	}

	if (FlagOn(Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK)) { // alive message

		PNDIS_PACKET	packet;

		DebugPrint( 2, ("[LPX]SmpTimerDpcRoutine: Send delayed SMP_TYPE_ACK. SP : %p, S: 0x%x, RS: 0x%x\n", 
						 Connection, Connection->LpxSmp.Sequence, SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		packet = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );
		
		if (packet)
			LpxSendPacket( Connection, packet, SMP_TYPE_ACK );

		packet = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (packet)
			LpxSendPacket( Connection, packet, SMP_TYPE_ACK );

		ClearFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

		ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );	
	}

	RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	LpxSendDataPackets( Connection );
	ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );	

out:

	if (Connection->LpxSmp.RetransmitCount) {
	
		SmpPrintState( 4, "Ret2", Connection );
		DebugPrint( 4,("NdasCurrentTime().QuadPart %I64d\n", NdasCurrentTime().QuadPart) );
	}

#if __LPX_STATISTICS__
	if (Connection->LpxSmp.StatisticsTimeOut.QuadPart <= NdasCurrentTime().QuadPart) { 

		PrintStatistics( 3, Connection );
		Connection->LpxSmp.StatisticsTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.StatisticsInterval.QuadPart;
	}
#endif

	if (Connection->LpxSmp.SmpState != SMP_STATE_CLOSE) {

		timeInterval.QuadPart = -Connection->LpxSmp.SmpTimerInterval.QuadPart;	
		LpxReferenceConnection( "Timer", Connection, CREF_LPX_TIMER );
		
		result = KeSetTimer( &Connection->LpxSmp.SmpTimer,
							 timeInterval,    
							 &Connection->LpxSmp.SmpTimerDpc );

		if (result == TRUE) { // Timer is already in system queue. deference myself.
	
			ASSERT( FALSE );
			LpxDereferenceConnection( "Timer", Connection, CREF_LPX_TIMER );
		}
	}

	RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

	LpxDereferenceConnection( "SmpTimerDpcRoutineRequest", Connection, CREF_LPX_TIMER );

	return;
}


PNDIS_PACKET
PrepareSmpNonDataPacket (
	IN PTP_CONNECTION		Connection,
	IN LPX_SMP_PACKET_TYPE	PacketType
	)
{
	KIRQL		    oldIrql;
	PLPX_HEADER		lpxHeader;
	NTSTATUS		status;
	USHORT		    sequence;
	USHORT		    finsequence;
	PNDIS_PACKET	packet;


	DebugPrint( 3, ("PrepareNonDataPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER)) );

	ASSERT( Connection->AddressFile->Address );
	ASSERT( Connection->AddressFile->Address->Provider );

	status = SendPacketAlloc( Connection->AddressFile->Address->Provider,
							  Connection->AddressFile->Address,
							  Connection->LpxSmp.DestinationAddress.Node,
							  NULL,
							  0,
							  NULL,
							  Connection->LpxSmp.Option,
							  &packet );

	if (!NT_SUCCESS(status)) {
		
		DebugPrint( 2, ("[LPX]PrepareNonDataPacket: packet == NULL\n") );
		SmpPrintState( 1, "[LPX]PrepareNonDataPacket: PacketAlloc", Connection );
		return NULL;
	}

	lpxHeader = &RESERVED(packet)->LpxHeader;

	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	lpxHeader->LpxType			= LPX_TYPE_STREAM;
	lpxHeader->DestinationPort	= Connection->LpxSmp.DestinationAddress.Port;
	lpxHeader->SourcePort		= Connection->AddressFile->Address->NetworkName->Port;

	lpxHeader->AckSequence		= HTONS(SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence));
	lpxHeader->ServerTag		= Connection->LpxSmp.ServerTag;

	switch (PacketType) {

	case SMP_TYPE_CONREQ:
	case SMP_TYPE_DISCON:
		
		sequence = Connection->LpxSmp.Sequence;
		Connection->LpxSmp.Sequence ++;

		if (PacketType == SMP_TYPE_CONREQ) {

			lpxHeader->Lsctl = HTONS(LSCTL_CONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		
		} else if(PacketType == SMP_TYPE_DISCON) {

			finsequence = Connection->LpxSmp.FinSequence;
			Connection->LpxSmp.FinSequence ++;

			lpxHeader->Lsctl = HTONS(LSCTL_DISCONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(finsequence);
		}

		DebugPrint( 2, ("CONREQ DISCON : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)) );
		
		break;

	case SMP_TYPE_ACK:

		DebugPrint( 4, ("ACK : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)) );

		lpxHeader->Lsctl = HTONS(LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(Connection->LpxSmp.Sequence));

		break;

	case SMP_TYPE_ACKREQ:

		DebugPrint( 4, ("ACKREQ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)) );

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(Connection->LpxSmp.Sequence));

		break;

	default:

		ASSERT( FALSE );
		DebugPrint( 0, ("[LPX] PrepareNonDataPacket: STATUS_NOT_SUPPORTED\n") );
		PacketFree( Connection->AddressFile->Address->Provider, packet );
		packet = NULL;
		break;
	}

	RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

	return packet;
}


VOID
LpxSendPacket ( 
	IN	PTP_CONNECTION		Connection,
	IN PNDIS_PACKET			Packet,
	IN LPX_SMP_PACKET_TYPE	PacketType
	)
{
	PNDIS_PACKET		PacketArray[1];

	UNREFERENCED_PARAMETER( PacketType );

	PacketArray[0]		= Packet;

	LpxSendPackets( Connection, PacketArray, 1 );
	
	return;
}


VOID
LpxSendPackets ( 
	IN	PTP_CONNECTION	Connection,
	IN  PPNDIS_PACKET	PacketArray,
	IN  UINT			NumberOfPackets
	)
{
	PDEVICE_CONTEXT	deviceContext;
	PTP_ADDRESS		address;
	UINT			index;
	KIRQL			oldirql;

	DebugPrint( 3, ("LpxSendPackets\n") );

	if (NumberOfPackets == 0) {

		NDAS_ASSERT(FALSE);
		return;
	}

	NDAS_ASSERT( NumberOfPackets <= LPX_MAX_PACKET_ARRAY );

	if (NumberOfPackets > 1) {
	
		for (index = 0; index < (NumberOfPackets-1); index ++) {

			SetFlag( RESERVED(PacketArray[index])->LpxHeader.Option, LPX_OPTION_PACKETS_CONTINUE_BIT );

			if (Connection->LpxSmp.DestinationSmallLengthDelayedAck == TRUE) {

				if ((index+1) % Connection->LpxSmp.PcbWindow == 0) {
					
					ClearFlag( RESERVED(PacketArray[index])->LpxHeader.Option, LPX_OPTION_PACKETS_CONTINUE_BIT );
				}
			}
		}	
	}

	address = Connection->AddressFile->Address;
	deviceContext = address->Provider;

	for (index = 0; index < NumberOfPackets;) {

		if (PacketArray[index] == NULL ) {

			UINT index2;

			NDAS_ASSERT(FALSE);

			for (index2 = index; index2 < NumberOfPackets; index2 ++) {

				PacketArray[index] = PacketArray[index2];
			}

			NumberOfPackets --;
			continue;
		}

		index ++;
	}

	for (index = 0; index < NumberOfPackets; index ++) {

		KeQuerySystemTime( &RESERVED(PacketArray[index])->SendTime );
#if __LPX_STATISTICS__
		RESERVED(PacketArray[index])->DeviceContext = deviceContext;
#endif
		InterlockedIncrement( &NumberOfSent );
	}

	if (NumberOfPackets) {

		INCREASE_SENDING_THREAD_COUNT( deviceContext );	
		ACQUIRE_SPIN_LOCK( &deviceContext->SpinLock, &oldirql );

		if (FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) && !FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP)) {

			ASSERT( deviceContext->NdisBindingHandle );

			Connection->LpxSmp.NumberOfSendPackets += NumberOfPackets;

			if (PacketTxDropRate) {

				PacketTxCountForDrop++;
				
				if ((PacketTxCountForDrop % 1000) <= PacketTxDropRate) {
			
					RESERVED(PacketArray[NumberOfPackets-1])->NdisStatus = NDIS_STATUS_SUCCESS;
					PacketFree( deviceContext, PacketArray[NumberOfPackets-1] );
					NumberOfPackets --;

					InterlockedIncrement( &NumberOfSentComplete );
				}
			}

			if (NumberOfPackets) {

				NdisSendPackets( deviceContext->NdisBindingHandle, PacketArray, NumberOfPackets );
			}
	
		} else {

			for (index = 0; index < NumberOfPackets; index ++) {

				RESERVED(PacketArray[index])->NdisStatus = NDIS_STATUS_NO_CABLE;
				PacketFree( deviceContext, PacketArray[index] );

				InterlockedIncrement( &NumberOfSentComplete );
			}	
		}

		RELEASE_SPIN_LOCK( &deviceContext->SpinLock, oldirql );
		DECREASE_SENDING_THREAD_COUNT( deviceContext );
	}

	return;
}


VOID
LpxSendComplete (
	IN NDIS_HANDLE	ProtocolBindingContext,
	IN PNDIS_PACKET Packet,
	IN NDIS_STATUS  Status
	)
{
	DebugPrint( 3, ("LpxSendComplete\n") );

	UNREFERENCED_PARAMETER( ProtocolBindingContext );

	if (Status != NDIS_STATUS_SUCCESS) {

		DebugPrint( 2, ("LpxSendComplete: status = %x\n", Status) );

		if (Status != NDIS_STATUS_NO_CABLE && NDIS_STATUS_ADAPTER_NOT_READY) {

			DebugPrint( 1, ("LpxSendComplete status: %x\n", Status) );
		}
	}

#if __LPX_STATISTICS__
	{
		LARGE_INTEGER systemTime;

		KeQuerySystemTime( &systemTime );

		RESERVED(Packet)->DeviceContext->NumberOfCompleteSendPackets ++;
		RESERVED(Packet)->DeviceContext->CompleteTimeOfSendPackets.QuadPart += systemTime.QuadPart - RESERVED(Packet)->SendTime.QuadPart;
		RESERVED(Packet)->DeviceContext->BytesOfCompleteSendPackets.QuadPart += NTOHS(RESERVED(Packet)->LpxHeader.PacketSize & ~LPX_TYPE_MASK);

		if (NTOHS(RESERVED(Packet)->LpxHeader.PacketSize & ~LPX_TYPE_MASK) > (sizeof(LPX_HEADER) + 12)) {

			RESERVED(Packet)->DeviceContext->NumberOfCompleteLargeSendPackets ++;
			RESERVED(Packet)->DeviceContext->CompleteTimeOfLargeSendPackets.QuadPart += systemTime.QuadPart - RESERVED(Packet)->SendTime.QuadPart;
			RESERVED(Packet)->DeviceContext->BytesOfCompleteLargeSendPackets.QuadPart += NTOHS(RESERVED(Packet)->LpxHeader.PacketSize & ~LPX_TYPE_MASK);

		} else {

			RESERVED(Packet)->DeviceContext->NumberOfCompleteSmallSendPackets ++;
			RESERVED(Packet)->DeviceContext->CompleteTimeOfSmallSendPackets.QuadPart += systemTime.QuadPart - RESERVED(Packet)->SendTime.QuadPart;
			RESERVED(Packet)->DeviceContext->BytesOfCompleteSmallSendPackets.QuadPart += NTOHS(RESERVED(Packet)->LpxHeader.PacketSize & ~LPX_TYPE_MASK);
		}
	}
#endif

	RESERVED(Packet)->NdisStatus = Status;
	PacketFree( (PDEVICE_CONTEXT)ProtocolBindingContext, Packet );

	InterlockedIncrement( &NumberOfSentComplete );

	return;
}

#if 0

BOOLEAN
SmpSendTest (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET    Packet
	)
{
	USHORT	inFlight;


	if (Connection->LpxSmp.RetransmitCount)
		return 0;

	inFlight = NTOHS(SHORT_SEQNUM(RESERVED(Packet)->LpxHeader.Sequence)) - SHORT_SEQNUM( Connection->LpxSmp.RemoteAckSequence );

	DebugPrint( 4, ("lpxHeader->Sequence = %x, Connection->LpxSmp.RemoteAckSequence = %x\n",
					 NTOHS(RESERVED(Packet)->LpxHeader.Sequence), SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence)) );

	if (inFlight >= Connection->LpxSmp.MaxFlights) {
	
		SmpPrintState( 1, "Flight overflow", Connection );
		return 0;
	}

	return 1;	
}

#endif

LONGLONG
CalculateRTT (
	IN PTP_CONNECTION	Connection
	)
{
	return Connection->LpxSmp.RetransmitInterval.QuadPart;

#if 0

	LARGE_INTEGER	Rtime;
	int		        i;

	Rtime.QuadPart = LpxRetransmitDelay; //(Connection->LpxSmp.IntervalTime.QuadPart);

	// Exponential
	for(i = 0; i < Connection->LpxSmp.Retransmits; i++) {

		Rtime.QuadPart *= 2;
		
		if(Rtime.QuadPart > LpxMaxRetransmitDelay)
			return LpxMaxRetransmitDelay;
	}

	if(Rtime.QuadPart > LpxMaxRetransmitDelay)
		Rtime.QuadPart = LpxMaxRetransmitDelay;

	return Rtime.QuadPart;
#endif
}


// Called with Connection->SpinLock locked


NTSTATUS
LpxSmpProcessReceivePacket (
	IN	PTP_CONNECTION	Connection,
	OUT PBOOLEAN		IrpCompleted
	)
{
	KIRQL				oldIrql;

	PLIST_ENTRY			packetListEntry;
	PLPX_RESERVED		reserved;
	PNDIS_PACKET		packet;

	PLIST_ENTRY		    irpListEntry;
	PIRP				irp;

	PIO_STACK_LOCATION	irpSp;
	ULONG		        userDataLength;

	ULONG		        irpCopied;
	ULONG		        remained;
	PUCHAR		        userData = NULL;
	PLPX_HEADER			lpxHeader;

	PNDIS_BUFFER		firstBuffer;
	PUCHAR		        bufferData;
	UINT		        bufferLength;
	UINT		        copied;
	KIRQL				cancelIrql;
	LIST_ENTRY			tempQueue;


	InitializeListHead( &tempQueue );


	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.RecvDataQSpinLock );
	ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.ReceiveIrpQSpinLock );

	while (!IsListEmpty( &Connection->LpxSmp.RecvDataQueue)) {

		packetListEntry = Connection->LpxSmp.RecvDataQueue.Flink;
		reserved		= CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
//		packet			= CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
		packet = reserved->Packet;

		lpxHeader = &RESERVED(packet)->LpxHeader;

		if (NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {

			SetFlag( Connection->LpxSmp.Shutdown, SMP_RECEIVE_SHUTDOWN );    

			RemoveEntryList( packetListEntry );
			InitializeListHead( packetListEntry );

			PacketFree( Connection->AddressFile->Address->Provider, packet );
			break;
		}

		if (IsListEmpty(&Connection->LpxSmp.ReceiveIrpQueue))
			break;

        irpListEntry = Connection->LpxSmp.ReceiveIrpQueue.Flink;

		irp = CONTAINING_RECORD( irpListEntry, IRP, Tail.Overlay.ListEntry );

		//
		//	check to see if the IRP is expired.
		//	If it is, complete the IRP with TIMEOUT.
		//	If expiration time is zero, do not check it.
		//
		//	added by hootch 02092004

#if 0 
		if (GET_IRP_EXPTIME(irp) && GET_IRP_EXPTIME(irp) <= NdasCurrentTime().QuadPart) {

			DebugPrint( 2, ("[LPX] SmpWorkDpcRoutine IRP expired!! %I64d CurrentTime:%I64d.\n",
							 GET_IRP_EXPTIME(irp), NdasCurrentTime().QuadPart) );

			RemoveEntryList( irpListEntry );
			InitializeListHead( irpListEntry );

			irp->IoStatus.Status = STATUS_REQUEST_ABORTED;
			InsertTailList( &tempQueue, irpListEntry );

			continue;
		}
#endif

		irpSp = IoGetCurrentIrpStackLocation( irp );
		userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 

#if __LPX_STATISTICS__

		// Check packet loss based on packet interval only if caller requested

		if (Connection->LpxSmp.LastDataPacketArrived.QuadPart != 0) {

			LONG TimeDiff; // time interval between two packet in 100nano sec unit.
			TimeDiff = (LONG)(RESERVED(packet)->RecvTime.QuadPart - Connection->LpxSmp.LastDataPacketArrived.QuadPart);

			if (TimeDiff > 0 && TimeDiff > Connection->LpxSmp.RetransmitInterval.QuadPart * 3/4) {

			    // Assume packet is lost if interval between packet is 75% of Retransmission delay.
			    //DebugPrint( 2, ("Rx packet lost based on interval check.\n") );
			    //INC_IRP_PACKET_LOSS( irp, 1 );
				InterlockedIncrement(&Connection->LpxSmp.TransportStats.PacketLoss);
			}
		}

		Connection->LpxSmp.LastDataPacketArrived = RESERVED(packet)->RecvTime;
#endif

		irpCopied = (ULONG)irp->IoStatus.Information;
		remained = userDataLength - irpCopied;

		userData = MmGetSystemAddressForMdlSafe( irp->MdlAddress, HighPagePriority );
		userData += irpCopied;
		
		NdisQueryPacket( packet, NULL, NULL, &firstBuffer, NULL );
		if(firstBuffer == NULL)
		{
			DebugPrint( 2, ("[LPX] SmpReadPacket: Connection=%p Irp=%p No first buffer!\n", Connection, irp) );

			RemoveEntryList( packetListEntry );
			InitializeListHead( packetListEntry );
			PacketFree( Connection->AddressFile->Address->Provider, packet );
			continue;
		}

		bufferData		= MmGetMdlVirtualAddress( firstBuffer );
		bufferData	   += RESERVED(packet)->PacketRawDataOffset;

		bufferLength	= RESERVED(packet)->PacketRawDataLength;
		bufferLength   -= RESERVED(packet)->PacketRawDataOffset;
		
		copied			= (bufferLength < remained) ? bufferLength : remained;
		
		RESERVED(packet)->PacketRawDataOffset += copied;

		RtlCopyMemory( userData, bufferData, copied );

		irp->IoStatus.Information += copied;

		DebugPrint( 4, ("userDataLength = %d, copied = %d, Irp->IoStatus.Information = %d\n",
						 userDataLength, copied, irp->IoStatus.Information) );
		
		if (irp->IoStatus.Information == userDataLength) {

			DebugPrint( 3, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p completed.\n", Connection, irp) );

#if __LPX_STATISTICS__
			Connection->LpxSmp.LastDataPacketArrived.QuadPart = 0;
#endif

			RemoveEntryList( irpListEntry );
			InitializeListHead( irpListEntry );

			irp->IoStatus.Status = STATUS_SUCCESS;
			InsertTailList( &tempQueue, irpListEntry );
		}

		if (RESERVED(packet)->PacketRawDataOffset == RESERVED(packet)->PacketRawDataLength) {

			RemoveEntryList( packetListEntry );
			InitializeListHead( packetListEntry );
			PacketFree( Connection->AddressFile->Address->Provider, packet );
		
		} 
	} 

	if (!IsListEmpty(&Connection->LpxSmp.RecvDataQueue)		&& 
		IsListEmpty(&Connection->LpxSmp.ReceiveIrpQueue)	&&
		!FlagOn(Connection->LpxSmp.Shutdown, SMP_RECEIVE_SHUTDOWN)) {

		 LpxCallUserReceiveHandler( Connection );
	}

	RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.ReceiveIrpQSpinLock );
	RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.RecvDataQSpinLock );

	RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

	if (ARGUMENT_PRESENT(IrpCompleted)) {

		if (IsListEmpty(&tempQueue)) {

			*IrpCompleted = FALSE;

		} else {

			*IrpCompleted = TRUE;
		}
	}

	while (!IsListEmpty(&tempQueue)) {

		irpListEntry = tempQueue.Flink;
		irp = CONTAINING_RECORD( irpListEntry, IRP, Tail.Overlay.ListEntry );
		
		RemoveEntryList( irpListEntry );
		InitializeListHead( irpListEntry );

#if __LPX_STATISTICS__

		if (irp->Tail.Overlay.DriverContext[0]) {
			
			LARGE_INTEGER systemTime;

			ASSERT( ((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection );

			KeQuerySystemTime( &systemTime );

			if( irp->IoStatus.Information < 1500 ) {

				((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.NumberofSmallRecvRequests ++;

				((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.ResponseTimeOfSmallRecvRequests.QuadPart +=
					systemTime.QuadPart - ((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->CurrentTime.QuadPart;					

				((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.BytesOfSmallRecvRequests.QuadPart += 
					irp->IoStatus.Information;

			} else {
			
				((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.NumberofLargeRecvRequests ++;

				((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.ResponseTimeOfLargeRecvRequests.QuadPart +=
					systemTime.QuadPart - ((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->CurrentTime.QuadPart;					

				((PCONNECTION_PRIVATE)irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.BytesOfLargeRecvRequests.QuadPart += irp->IoStatus.Information;
			}

			LpxDereferenceConnection( "LpxSmpProcessReceivePacket", Connection, CREF_LPX_PRIVATE );

			ExFreePool( irp->Tail.Overlay.DriverContext[0] );
			irp->Tail.Overlay.DriverContext[0] = 0;
		}

#endif

		IoAcquireCancelSpinLock( &cancelIrql );
		IoSetCancelRoutine( irp, NULL );
		IoReleaseCancelSpinLock( cancelIrql );

		LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );
	}

	ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

	//
	//	check to see if shutdown is in progress.
	//
	//	added by hootch 09042003
	//

	if (Connection->LpxSmp.Shutdown & SMP_RECEIVE_SHUTDOWN) {
		
		KIRQL			cancelirql;
		PLIST_ENTRY		thisEntry;
		PIRP		    pendingIrp;
		PDRIVER_CANCEL	oldCancelRoutine;

		ASSERT( IsListEmpty( &Connection->LpxSmp.RecvDataQueue) );

		DebugPrint( 2, ("[LPX] LpxSmpProcessReceivePacket: Remotely DisConnected\n") );
	
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		IoAcquireCancelSpinLock( &cancelirql );
		ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

		while (thisEntry = ExInterlockedRemoveHeadList( &Connection->LpxSmp.ReceiveIrpQueue, 
														&Connection->LpxSmp.ReceiveIrpQSpinLock)) {
	
			RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

			pendingIrp = CONTAINING_RECORD( thisEntry, IRP, Tail.Overlay.ListEntry );

			oldCancelRoutine = IoSetCancelRoutine( pendingIrp, NULL );
			IoReleaseCancelSpinLock( cancelirql );

			pendingIrp->IoStatus.Information = 0;
			pendingIrp->IoStatus.Status = STATUS_REMOTE_DISCONNECT;
			LpxIoCompleteRequest( pendingIrp, IO_NO_INCREMENT );

			DebugPrint( 2, ("[LPX] LpxSmpProcessReceivePacket: Cancelled Receive IRP %p\n", pendingIrp) );

			IoAcquireCancelSpinLock( &cancelirql );
			ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );
		}

		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );
		IoReleaseCancelSpinLock( cancelirql );

		ACQUIRE_C_SPIN_LOCK( &Connection->SpinLock, &oldIrql );
		LpxCallUserReceiveHandler( Connection );
		RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

		return STATUS_REMOTE_DISCONNECT;	
	} 

	RELEASE_C_SPIN_LOCK( &Connection->SpinLock, oldIrql );

	return STATUS_SUCCESS;
}


VOID
LpxReceiveComplete2 (
	IN NDIS_HANDLE BindingContext,
	IN PLIST_ENTRY	ReceivedPackets
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
	PLPX_HEADER			lpxHeader;

	UINT		        bufferLength;
	PDEVICE_CONTEXT		deviceContext = (PDEVICE_CONTEXT)BindingContext;
	PTP_ADDRESS		    address;
	PTP_CONNECTION		connection;

	PLIST_ENTRY		    Flink;
	PLIST_ENTRY		    listHead;
	PLIST_ENTRY		    thisEntry;

	PUCHAR		        packetData = NULL;
	PLIST_ENTRY		    p;
	PTP_ADDRESS_FILE	addressFile;
	KIRQL		        irql;
	KIRQL				cancelIrql;

	BOOLEAN		        refAddress = FALSE;
	BOOLEAN		        refAddressFile = FALSE;
	BOOLEAN		        refConnection = FALSE;
	
	const UCHAR			BroadcastAddr[ETHERNET_ADDRESS_LENGTH]  = {0xff, 0xff,0xff,0xff,0xff,0xff};


	DebugPrint(4, ("[Lpx]LpxReceiveComplete: Entered %d\n", KeGetCurrentIrql()));

	//
	// Process In Progress Packets.
	//

	while (TRUE) {
		pListEntry = RemoveHeadList(ReceivedPackets);
		if(pListEntry == ReceivedPackets)
			break;

		reserved = CONTAINING_RECORD( pListEntry, LPX_RESERVED, ListEntry );
//		Packet = CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
		Packet = reserved->Packet;
		packetData = NULL;
		
		lpxHeader = &RESERVED(Packet)->LpxHeader;

		if (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ) {

			DebugPrint( 4, ("*** Recevied %s from %02X%02X%02X%02X%02X%02X:%04X\n",
							(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)?"CONNREQ|ACK":"CONNREQ",
							RESERVED(Packet)->EthernetHeader.SourceAddress[0],
							RESERVED(Packet)->EthernetHeader.SourceAddress[1],
							RESERVED(Packet)->EthernetHeader.SourceAddress[2],
							RESERVED(Packet)->EthernetHeader.SourceAddress[3],
							RESERVED(Packet)->EthernetHeader.SourceAddress[4],
							RESERVED(Packet)->EthernetHeader.SourceAddress[5],
							lpxHeader->SourcePort ));
		}

		if (RESERVED(Packet)->EthernetHeader.DestinationAddress[0] != 0xFF) {

			DebugPrint( 4,("From %02X%02X%02X%02X%02X%02X:%04X\n",
							RESERVED(Packet)->EthernetHeader.SourceAddress[0],
							RESERVED(Packet)->EthernetHeader.SourceAddress[1],
							RESERVED(Packet)->EthernetHeader.SourceAddress[2],
							RESERVED(Packet)->EthernetHeader.SourceAddress[3],
							RESERVED(Packet)->EthernetHeader.SourceAddress[4],
							RESERVED(Packet)->EthernetHeader.SourceAddress[5],
							lpxHeader->SourcePort) );
		
			DebugPrint( 4, ("To %02X%02X%02X%02X%02X%02X:%04X\n",
							RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
							RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
							RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
							RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
							RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
							RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
							lpxHeader->DestinationPort) );
		}

		//
		//	match destination Address
		//

		ACQUIRE_SPIN_LOCK ( &deviceContext->SpinLock, &irql );

		for (address = NULL, Flink = deviceContext->AddressDatabase.Flink;
			 Flink != &deviceContext->AddressDatabase;
			 Flink = Flink->Flink, address = NULL) {
			
			 address = CONTAINING_RECORD( Flink, TP_ADDRESS, Linkage );
			
			if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
			
				continue;
			}
			
			if (address->NetworkName == NULL) {

				continue;
			}

			if (address->NetworkName->Port == lpxHeader->DestinationPort) {
			    // Broadcast?
			    if (RtlCompareMemory(RESERVED(Packet)->EthernetHeader.DestinationAddress,
									 BroadcastAddr, 
									 ETHERNET_ADDRESS_LENGTH) == ETHERNET_ADDRESS_LENGTH) {

					LpxReferenceAddress( "ReceiveCompletion", address, AREF_REQUEST );
			        refAddress = TRUE;
				    break;
				}
			
				if (RtlCompareMemory(&address->NetworkName->Node, 
									 RESERVED(Packet)->EthernetHeader.DestinationAddress, 
									 ETHERNET_ADDRESS_LENGTH) == ETHERNET_ADDRESS_LENGTH) {
	
					LpxReferenceAddress( "ReceiveCompletion", address, AREF_REQUEST );
				    refAddress = TRUE;
					break;
				}
			}
		}

		RELEASE_SPIN_LOCK( &deviceContext->SpinLock, irql );

		
		if (address == NULL) {
		
			// No matching address. 
#if DBG
			if (RtlCompareMemory(RESERVED(Packet)->EthernetHeader.DestinationAddress,
								 BroadcastAddr, 
								 ETHERNET_ADDRESS_LENGTH) != ETHERNET_ADDRESS_LENGTH) {

			DebugPrint( 2, ("No End Point. To %02X%02X%02X%02X%02X%02X:%04X\n",
							 RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
							 RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
							 RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
							 RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
							 RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
							 RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
							 lpxHeader->DestinationPort) );
			}
#endif
			goto TossPacket;
		}
		
		//
		//	if it is a stream-like(called LPX-Stream) packet,
		//	match destination Connection ( called service point in LPX )
		//

		if (lpxHeader->LpxType == LPX_TYPE_STREAM) {
		
			ACQUIRE_SPIN_LOCK( &address->SpinLock, &irql );
			
			listHead = &address->ConnectionDatabase;

			for (connection = NULL, thisEntry = listHead->Flink;
			     thisEntry != listHead;
			     thisEntry = thisEntry->Flink, connection = NULL) {

				connection = CONTAINING_RECORD( thisEntry, TP_CONNECTION, AddressList );

				DebugPrint( 4,("connectionServicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
							    connection->LpxSmp.DestinationAddress.Node[0],
								connection->LpxSmp.DestinationAddress.Node[1],
								connection->LpxSmp.DestinationAddress.Node[2],
								connection->LpxSmp.DestinationAddress.Node[3],
								connection->LpxSmp.DestinationAddress.Node[4],
								connection->LpxSmp.DestinationAddress.Node[5],
								connection->LpxSmp.DestinationAddress.Port) );

			    if (connection->LpxSmp.SmpState != SMP_STATE_CLOSE							&& 
					RtlCompareMemory(connection->LpxSmp.DestinationAddress.Node, 
									 RESERVED(Packet)->EthernetHeader.SourceAddress, 
									 ETHERNET_ADDRESS_LENGTH) == ETHERNET_ADDRESS_LENGTH	&& 
					connection->LpxSmp.DestinationAddress.Port == lpxHeader->SourcePort) {

			        //
			        //    reference Connection
			        //    hootch    09042003
			        
					LpxReferenceConnection( "ReceiveCompletion", connection, CREF_REQUEST );
			        refConnection = TRUE;

			        DebugPrint( 4, ("[LPX] connectionServicePoint = %p found!\n", connection) );
			        break;
			    }
			}

			if (connection == NULL) {
		
				for (connection = NULL, thisEntry = listHead->Flink;
					 thisEntry != listHead;
					 thisEntry = thisEntry->Flink, connection = NULL) {

					UCHAR    zeroNode[6] = {0, 0, 0, 0, 0, 0};
				    
					connection = CONTAINING_RECORD( thisEntry, TP_CONNECTION, AddressList );

					if (connection->LpxSmp.SmpState == SMP_STATE_LISTEN									&& 
						RtlCompareMemory(&connection->LpxSmp.DestinationAddress.Node, zeroNode, 6) == 6 && 
						connection->LpxSmp.ListenIrp != NULL) {

						LpxReferenceConnection( "ReceiveCompletion", connection, CREF_REQUEST );
						refConnection = TRUE;

						DebugPrint( 2, ("listenConnection = %p found!\n", connection) );
						break;
				    }
				}
			}
			
			if (connection == NULL) {

	            RELEASE_SPIN_LOCK( &address->SpinLock, irql );
	            goto TossPacket;
	        }

			RELEASE_SPIN_LOCK ( &address->SpinLock, irql );

			InterlockedExchange( &RESERVED(Packet)->ReorderCount, 0 );

			LpxSmpReceiveComplete( connection, Packet );
			//SmpDoReceiveRequest( &connection->Sp, Packet );

			goto loopEnd;

		} else if (lpxHeader->LpxType == LPX_TYPE_DATAGRAM) {

			DebugPrint( 4, ("[LPX] LpxReceiveComplete: DataGram packet arrived.\n") );

			ACQUIRE_DPC_SPIN_LOCK ( &address->SpinLock );

			p = address->AddressFileDatabase.Flink;

			while (p != &address->AddressFileDatabase) {
			
				addressFile = CONTAINING_RECORD( p, TP_ADDRESS_FILE, Linkage );

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

			if (p == &address->AddressFileDatabase) {

			    RELEASE_DPC_SPIN_LOCK( &address->SpinLock );

			    DebugPrint( 4, ("[LPX] LpxReceiveComplete: DataGram Packet - No addressFile matched.\n") );
			    goto TossPacket;
			}

			RELEASE_DPC_SPIN_LOCK( &address->SpinLock );

			if (addressFile->RegisteredReceiveDatagramHandler) {

			    ULONG				indicateBytesCopied, mdlBytesCopied, bytesToCopy;
			    NTSTATUS			ntStatus;
			    TA_NETBIOS_ADDRESS  sourceName;
			    PIRP                irp;
			    PIO_STACK_LOCATION  irpSp;
			    UINT                totalCopied;
			    PUCHAR              bufferData;
			    ULONG               userDataLength;

			    userDataLength = RESERVED(Packet)->PacketRawDataLength - RESERVED(Packet)->PacketRawDataOffset;
				
			    DebugPrint( 4, ("[LPX] LpxReceiveComplete: call UserDataGramHandler with a DataGram packet. NTOHS(lpxHeader->PacketSize) = %d, userDataLength = %d\n",
								 NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK), userDataLength) );

			    packetData = ExAllocatePoolWithTag( NonPagedPool, userDataLength, LPX_MEM_TAG_DGRAM_DATA );
			    //
			    //    NULL pointer check.
			    //
			    //    added by @hootch@ 0812
			    //
			    if (packetData == NULL) {

			        DebugPrint( 0, ("[LPX] LpxReceiveComplete failed to allocate nonpaged pool for packetData\n") );
			        goto TossPacket;
			    }

			    //
			    // Copy User Data of the packet
			    //

				totalCopied = 0;

				NdisQueryPacket( Packet, NULL, NULL, &firstBuffer, NULL );

				if (firstBuffer == NULL) {

					totalCopied = 0;
				
				} else {
								
					bufferData		= MmGetMdlVirtualAddress( firstBuffer );
					bufferData	   += RESERVED(Packet)->PacketRawDataOffset;

					bufferLength	= RESERVED(Packet)->PacketRawDataLength;
					bufferLength   -= RESERVED(Packet)->PacketRawDataOffset;
				
				    totalCopied = (bufferLength < userDataLength) ? bufferLength : userDataLength;
			    
					RtlCopyMemory( packetData, bufferData, totalCopied );
				}
			        
			    //
			    //    call user-defined ReceiveDatagramHandler with copied data
			    //

			    indicateBytesCopied = 0;

			    sourceName.TAAddressCount = 1;
			    sourceName.Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
			    sourceName.Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
			    
				memcpy( ((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Node, 
						RESERVED(Packet)->EthernetHeader.SourceAddress, 
						ETHERNET_ADDRESS_LENGTH );

				((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Port = lpxHeader->SourcePort;

			    DebugPrint( 3, ("[LPX] LPxReceiveComplete: DATAGRAM: SourceAddress=%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
	                             RESERVED(Packet)->EthernetHeader.SourceAddress[0],
			                     RESERVED(Packet)->EthernetHeader.SourceAddress[1],
			                     RESERVED(Packet)->EthernetHeader.SourceAddress[2],
			                     RESERVED(Packet)->EthernetHeader.SourceAddress[3],
			                     RESERVED(Packet)->EthernetHeader.SourceAddress[4],
			                     RESERVED(Packet)->EthernetHeader.SourceAddress[5],
			                     NTOHS(lpxHeader->SourcePort)) );

			    ntStatus = (*addressFile->ReceiveDatagramHandler)( addressFile->ReceiveDatagramHandlerContext,
																   sizeof (TA_NETBIOS_ADDRESS),
																   &sourceName,
																   0,
																   NULL,
																   TDI_RECEIVE_NORMAL,
																   userDataLength,
																   userDataLength,  // available
																   &indicateBytesCopied,
																   packetData,
																   &irp );

			    if (ntStatus == STATUS_SUCCESS) {

			        DebugPrint( 4, ("[LPX] LpxReceiveComplete: DATAGRAM: A datagram packet consumed. STATUS_SUCCESS, "
									"userDataLength = %d, indicateBytesCopied = %d\n", 
						             userDataLength, indicateBytesCopied) );
			    
				} else if (ntStatus == STATUS_DATA_NOT_ACCEPTED) {

					DebugPrint( 4, ("[LPX] LpxReceiveComplete: DATAGRAM: DataGramHandler didn't accept a datagram packet.\n") );
			        
					IF_LPXDBG (LPX_DEBUG_DATAGRAMS) {

			            LpxPrint0( "[LPX] LpxReceiveComplete: DATAGRAM: Picking off a rcv datagram request from this address.\n" );
			        }

			        ntStatus = STATUS_MORE_PROCESSING_REQUIRED;
			        
			    } else if (ntStatus == STATUS_MORE_PROCESSING_REQUIRED) {

			        DebugPrint( 2, ("[LPX] LpxReceiveComplete: DATAGRAM: STATUS_MORE_PROCESSING_REQUIRED\n") );

					irp->IoStatus.Status = STATUS_PENDING;  // init status information.
			        irp->IoStatus.Information = 0;
			        irpSp = IoGetCurrentIrpStackLocation (irp); // get current stack loctn.
			        
					if ((irpSp->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL) || (irpSp->MinorFunction != TDI_RECEIVE_DATAGRAM)) {

			            DebugPrint( 2, ("[LPX] LpxReceiveComplete: DATAGRAM: Wrong IRP: Maj:%d Min:%d\n",
							             irpSp->MajorFunction, irpSp->MinorFunction) );

			            irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
			            goto TossPacket;
			        }

			        //
			        // Now copy the actual user data.
			        //
			        mdlBytesCopied = 0;
			        bytesToCopy =  userDataLength - indicateBytesCopied;

			        if ((bytesToCopy > 0) && irp->MdlAddress) {

			            ntStatus = TdiCopyBufferToMdl( packetData,
													   indicateBytesCopied,
													   bytesToCopy,
													   irp->MdlAddress,
													   0,
													   &mdlBytesCopied );
			        
					} else {
			        
						ntStatus = STATUS_SUCCESS;
			        }

			        irp->IoStatus.Information = mdlBytesCopied;
			        irp->IoStatus.Status = ntStatus;

			        IoAcquireCancelSpinLock( &cancelIrql );
			        IoSetCancelRoutine( irp, NULL );
			        IoReleaseCancelSpinLock( cancelIrql );

			        DebugPrint( 2, ("[LPX]LpxReceiveComplete: DATAGRAM: IRP %p completed with NTSTATUS:%08lx.\n ", irp, ntStatus) );
			        LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );
			    }
			}
		}

TossPacket:

		if (packetData != NULL) {

			ExFreePool(packetData);
			packetData = NULL;
		}

		PacketFree( deviceContext, Packet );

loopEnd:
		//
		//	clean up reference count
		//	added by hootch 09042003
		//
		if(refAddress) {

			LpxDereferenceAddress( "ReceiveCompletion", address, AREF_REQUEST );
			refAddress = FALSE;
		}
		if(refAddressFile) {

			LpxDereferenceAddressFile( addressFile );
			refAddressFile = FALSE;
		}
		if(refConnection) {
			LpxDereferenceConnection( "ReceiveCompletion", connection, CREF_REQUEST );
			refConnection = FALSE;
		}

		continue;
	}

	return;
}


VOID
LpxSmpReceiveComplete (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet
	) 
{
	BOOLEAN		result;

	LpxReferenceConnection( "DoReceiveReq", Connection, CREF_LPX_RECEIVE );

	result = SmpDoReceive( Connection, Packet );
	
	if (result == FALSE) {

		NDAS_ASSERT(FALSE);			

		PacketFree( Connection->AddressFile->Address->Provider, Packet );
	}

	LpxDereferenceConnection( "DoReceiveReq", Connection, CREF_LPX_RECEIVE );
}


BOOLEAN
SmpDoReceive (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet
	)
{
	PLPX_HEADER		lpxHeader;
	KIRQL		    oldIrql2;
	BOOLEAN			PacketHandled;
	LIST_ENTRY		freePacketList;
	LARGE_INTEGER	systemTime;


	DebugPrint( 3, ("SmpDoReceive\n") );

	InitializeListHead( &freePacketList );

	lpxHeader = &RESERVED(Packet)->LpxHeader;
	lpxHeader->Lsctl = HTONS( NTOHS(lpxHeader->Lsctl) & LSCTL_MASK );

	ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

	KeQuerySystemTime( &systemTime );

	Connection->LpxSmp.NumberofRecvPackets ++;

#if __LPX_STATISTICS__

	Connection->LpxSmp.BytesOfRecvPackets.QuadPart += NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);

	if ((NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)+14) <= 60) {

		Connection->LpxSmp.NumberofSmallRecvPackets ++;
		Connection->LpxSmp.BytesOfSmallRecvPackets.QuadPart += NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);
			
	} else {

		Connection->LpxSmp.NumberofLargeRecvPackets ++;
		Connection->LpxSmp.BytesOfLargeRecvPackets.QuadPart += NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);
	}

#endif

	if ((SHORT)( NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) > MAX_ALLOWED_SEQ) {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		PacketFree( Connection->AddressFile->Address->Provider, Packet );
		DebugPrint( 2, ("[LPX]SmpDoReceive: bad ACK number. Drop packet (2)\n") );

		return TRUE;
	}

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {

		if (((SHORT)(SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence) - NTOHS(lpxHeader->AckSequence))) > MAX_ALLOWED_SEQ) {
		
			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			DebugPrint( 2, ("[LPX]SmpDoReceive: bad ACK number. Drop packet (3)\n") );
			PacketFree( Connection->AddressFile->Address->Provider, Packet );
			
			return TRUE;
		}
	}

	DebugPrint( 4, ("SmpDoReceive NTOHS(lpxHeader->Sequence) = 0x%x, lpxHeader->Lsctl = 0x%x\n",
					 NTOHS(lpxHeader->Sequence), lpxHeader->Lsctl));

	ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, &oldIrql2 );

	if (Connection->LpxSmp.RetransmitCount == 0) {

		Connection->LpxSmp.MaxStallTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.MaxStallInterval.QuadPart;
	}

	Connection->LpxSmp.AliveTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.AliveInterval.QuadPart;

	RELEASE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, oldIrql2 );
	
	switch(Connection->LpxSmp.SmpState) {
	
	case SMP_STATE_LISTEN:
		
		PacketHandled = LpxStateDoReceiveWhenListen( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_SYN_SENT:
		
		PacketHandled = LpxStateDoReceiveWhenSynSent( Connection, Packet, &freePacketList );
		break;
	
	case SMP_STATE_SYN_RECV:
	
		PacketHandled = LpxStateDoReceiveWhenSynRecv( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_ESTABLISHED:
		
		PacketHandled = LpxStateDoReceiveWhenEstablished( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_FIN_WAIT1:
		
		PacketHandled = LpxStateDoReceiveWhenFinWait1( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_FIN_WAIT2:
		
		PacketHandled = LpxStateDoReceiveWhenFinWait2( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_CLOSING:

		PacketHandled = LpxStateDoReceiveWhenClosing( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_CLOSE_WAIT:

		PacketHandled = LpxStateDoReceiveWhenCloseWait( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_LAST_ACK:

		PacketHandled = LpxStateDoReceiveWhenLastAck( Connection, Packet, &freePacketList );
		break;

	case SMP_STATE_TIME_WAIT:

		PacketHandled = LpxStateDoReceiveWhenTimeWait( Connection, Packet, &freePacketList );
		break;


	case SMP_STATE_CLOSE:
	default:

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	
		DebugPrint( 2, ("[LPX] Dropping packet in %s state. ", LpxStateName[Connection->LpxSmp.SmpState]) );
		DebugPrint( 2, ("src=%02x:%02x:%02x:%02x:%02x:%02x   lsctl=%04x\n", 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[0], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[1], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[2], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[3], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[4], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[5], 
						 NTOHS(lpxHeader->Lsctl) ));
		
		PacketFree( Connection->AddressFile->Address->Provider, Packet );
		PacketHandled = TRUE;
		break;
	}

	while (!IsListEmpty(&freePacketList)) {

		PLIST_ENTRY		packetListEntry;
		PLPX_RESERVED	reserved;
		PNDIS_PACKET	packet;

		packetListEntry = freePacketList.Flink;
		reserved		= CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
//		packet			= CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
		packet = reserved->Packet;
		
		RemoveEntryList( &reserved->ListEntry );
		InitializeListHead( &reserved->ListEntry );

		PacketFree( Connection->AddressFile->Address->Provider, packet );
	}

	return PacketHandled;
}			


//////////////////////////////////////////
//
//  LPX State machine refactoring
// 


BOOLEAN 
LpxStateDoReceiveWhenListen (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	UCHAR		    dataArrived = 0;
	LARGE_INTEGER	timeInterval = {0,0};
	PNDIS_PACKET	replyPacket;
	BOOL			result;

	UNREFERENCED_PARAMETER( FreePacketList );

	lpxHeader = &RESERVED(Packet)->LpxHeader;

	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_LISTEN );
	
	switch (NTOHS(lpxHeader->Lsctl) & LSCTL_MASK) {

	case LSCTL_CONNREQ:
	case LSCTL_CONNREQ | LSCTL_ACK:
	
		DebugPrint( 2, ("SmpDoReceive SMP_STATE_LISTEN CONREQ\n") );

#if __LPX_OPTION_ADDRESSS__

		if (!FlagOn(lpxHeader->Option, LPX_OPTION_SOURCE_ADDRESS_ACCEPT))
			ClearFlag( Connection->LpxSmp.Option, LPX_OPTION_SOURCE_ADDRESS );

		if (!FlagOn(lpxHeader->Option, LPX_OPTION_DESTINATION_ADDRESS_ACCEPT))
			ClearFlag( Connection->LpxSmp.Option, LPX_OPTION_DESTINATION_ADDRESS );

#endif

		if (FlagOn(lpxHeader->Option, LPX_OPTION_SMALL_LENGTH_DELAYED_ACK)) {

			Connection->LpxSmp.DestinationSmallLengthDelayedAck = 1;
			Connection->LpxSmp.PcbWindow = DEFAULT_PCB_WINDOW;
		}

		if (Connection->LpxSmp.ListenIrp == NULL) {

			ASSERT( FALSE );
			DebugPrint( 2, ("SmpDoReceive ERROR. No ListenIrp. Dropping packet\n") );
			break;
		}

		RtlCopyMemory( Connection->LpxSmp.DestinationAddress.Node,
					   RESERVED(Packet)->EthernetHeader.SourceAddress,
					   ETHERNET_ADDRESS_LENGTH );

		Connection->LpxSmp.DestinationAddress.Port = lpxHeader->SourcePort;

		Connection->LpxSmp.RemoteSequence ++;
		Connection->LpxSmp.ServerTag =lpxHeader->ServerTag;

		LpxChangeState( Connection, SMP_STATE_SYN_RECV, TRUE );

		ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );
		Connection->LpxSmp.MaxStallTimeOut.QuadPart		= NdasCurrentTime().QuadPart + Connection->LpxSmp.MaxConnectWaitInterval.QuadPart;
		Connection->LpxSmp.RetransmitTimeOut.QuadPart	= NdasCurrentTime().QuadPart + CalculateRTT(Connection);
		RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );

#if __LPX_STATISTICS__
		Connection->LpxSmp.StatisticsTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.StatisticsInterval.QuadPart;
#endif

		LpxReferenceConnection( "Listen", Connection, CREF_LPX_TIMER );
		timeInterval.QuadPart = -Connection->LpxSmp.SmpTimerInterval.QuadPart;
		
		result = KeSetTimer( &Connection->LpxSmp.SmpTimer,
							 timeInterval,
							 &Connection->LpxSmp.SmpTimerDpc );

		if (result == TRUE) { // Timer is already in system queue. deference myself.

			ASSERT( FALSE );
			LpxDereferenceConnection( "Listen", Connection, CREF_LPX_TIMER );
		}

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		replyPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_CONREQ );

		if (replyPacket) {

			ExInterlockedInsertTailList( &Connection->LpxSmp.TramsmitQueue,
										 &RESERVED(replyPacket)->ListEntry,
										 &Connection->LpxSmp.TransmitSpinLock );

			LpxSendDataPackets( Connection );
		}

		break;

	default:
		
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		DebugPrint( 2, ("Dropping non CONNREQ packet(%x) for listening socket.\n", NTOHS(lpxHeader->Lsctl)) );
		DebugPrint( 2, ("src=%02x:%02x:%02x:%02x:%02x:%02x, src port=%d, des port=%d\n", 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[0], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[1], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[2], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[3], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[4], 
						 RESERVED(Packet)->EthernetHeader.SourceAddress[5], 
						 NTOHS(lpxHeader->SourcePort),
						 NTOHS(lpxHeader->DestinationPort) ));
		break;
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );

	return TRUE; // Not a data packet
}


BOOLEAN 
LpxStateDoReceiveWhenSynSent (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	UCHAR		    dataArrived = 0;
	PNDIS_PACKET	ackPacket;
	KIRQL			cancelIrql;


	DebugPrint( 3, ("SmpDoReceive\n") );

	lpxHeader = &RESERVED(Packet)->LpxHeader;

	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_SYN_SENT );

	if (!(NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ)) {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		NDAS_ASSERT( Connection->AddressFile->Address->PortAssignedByLpx == TRUE );

		if (Connection->AddressFile->Address->PortAssignedByLpx == TRUE) {

			PTP_CONNECTION		connection;
			ULONG				connectionCount = 0;

			PLIST_ENTRY		    listHead;
			PLIST_ENTRY		    thisEntry;

			ACQUIRE_DPC_SPIN_LOCK( &Connection->AddressFile->Address->SpinLock );
			
			listHead = &Connection->AddressFile->Address->ConnectionDatabase;

			for (connection = NULL, thisEntry = listHead->Flink;
			     thisEntry != listHead;
				 thisEntry = thisEntry->Flink, connection = NULL) {

				connection = CONTAINING_RECORD( thisEntry, TP_CONNECTION, AddressList );

				DebugPrint( 4, ("connectionServicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
							    connection->LpxSmp.DestinationAddress.Node[0],
								connection->LpxSmp.DestinationAddress.Node[1],
								connection->LpxSmp.DestinationAddress.Node[2],
								connection->LpxSmp.DestinationAddress.Node[3],
								connection->LpxSmp.DestinationAddress.Node[4],
								connection->LpxSmp.DestinationAddress.Node[5],
								connection->LpxSmp.DestinationAddress.Port) );

				if (connection->LpxSmp.SmpState != SMP_STATE_CLOSE) {
				
					connectionCount ++;
				}
			}

			NDAS_ASSERT( connectionCount == 1 );

			if (connectionCount == 1) {

				ACQUIRE_DPC_SPIN_LOCK( &Connection->AddressFile->Address->Provider->SpinLock );

				LpxAssignPort( Connection->AddressFile->Address->Provider, Connection->AddressFile->Address->NetworkName ); 

				RELEASE_DPC_C_SPIN_LOCK( &Connection->AddressFile->Address->Provider->SpinLock );
			}

			RELEASE_DPC_C_SPIN_LOCK ( &Connection->AddressFile->Address->SpinLock );
		}

		PacketFree( Connection->AddressFile->Address->Provider, Packet );

		return TRUE;
	}

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {

		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {

			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}
	}

	if (Connection->LpxSmp.ConnectIrp == NULL) {

		NDAS_ASSERT(FALSE);			
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		PacketFree( Connection->AddressFile->Address->Provider, Packet );
		
		return TRUE;
	}

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ) {

		DebugPrint( 2, ("SmpDoReceive SMP_STATE_SYN_SENT CONREQ\n") );

#if __LPX_OPTION_ADDRESSS__

		if (!FlagOn(lpxHeader->Option, LPX_OPTION_SOURCE_ADDRESS_ACCEPT))
			ClearFlag( Connection->LpxSmp.Option, LPX_OPTION_SOURCE_ADDRESS );

		if (!FlagOn(lpxHeader->Option, LPX_OPTION_DESTINATION_ADDRESS_ACCEPT))
			ClearFlag( Connection->LpxSmp.Option, LPX_OPTION_DESTINATION_ADDRESS );

#endif

		SmpPrintState( 2, "LSCTL_CONNREQ", Connection );
		
		Connection->LpxSmp.RemoteSequence ++;
		Connection->LpxSmp.ServerTag = lpxHeader->ServerTag;

		LpxChangeState( Connection, SMP_STATE_SYN_RECV, TRUE );

		if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {

			PIRP    irp;

			LpxChangeState( Connection, SMP_STATE_ESTABLISHED, TRUE );

			SetFlag( Connection->Flags2, CONNECTION_FLAGS2_REQ_COMPLETED );
			
			irp = Connection->LpxSmp.ConnectIrp;
		    Connection->LpxSmp.ConnectIrp = NULL;
		    irp->IoStatus.Status = STATUS_SUCCESS;
		
			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			IoAcquireCancelSpinLock( &cancelIrql );
			IoSetCancelRoutine( irp, NULL );
		    IoReleaseCancelSpinLock( cancelIrql );
	
			LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );

			DebugPrint(2, ("[LPX]SmpDoReceive: Connect IRP %p completed.\n ", irp));
			
		} else {
		
			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		} 
		
		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );
		
		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

	} else {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		DebugPrint(2, ("SmpDoReceive Unexpected packet in SYN_SENT state(%x)\n", NTOHS(lpxHeader->Lsctl)));
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );

	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenSynRecv (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER	lpxHeader;
	KIRQL		cancelIrql;
	
	
	lpxHeader = &RESERVED(Packet)->LpxHeader;
		
	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_SYN_RECV );	

	DebugPrint( 2, ("LpxDoReceive SMP_STATE_SYN_RECV CONREQ\n") );

	// Accept ACK only in this state

	if (!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)) {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		PacketFree( Connection->AddressFile->Address->Provider, Packet );

		DebugPrint(2, ("LpxDoReceive Unexpected packet when SYN_RECV %x\n", NTOHS(lpxHeader->Lsctl)));
		return TRUE;
	}

	if (NTOHS(lpxHeader->AckSequence) < 1) {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		PacketFree( Connection->AddressFile->Address->Provider, Packet );

		DebugPrint(2, ("LpxDoReceive Invalid acksequence when SYN_RECV\n"));
		return TRUE;
	}

	if (Connection->LpxSmp.ConnectIrp == NULL && Connection->LpxSmp.ListenIrp == NULL) {

		NDAS_ASSERT(FALSE);			
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		PacketFree( Connection->AddressFile->Address->Provider, Packet );

		return TRUE;
	}

	// 
	// Check Server Tag.
	//

	if (lpxHeader->ServerTag != Connection->LpxSmp.ServerTag) {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		PacketFree( Connection->AddressFile->Address->Provider, Packet );

		DebugPrint( 2, ("[LPX] SmpDoReceive/SynRecv: Bad Server Tag. Dropping. Connection->LpxSmp.ServerTag 0x%x, lpxHeader->ServerTag 0x%x\n", 
			             Connection->LpxSmp.ServerTag, lpxHeader->ServerTag) );
		return TRUE;	    
	}

	if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {

		Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
		SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
	}

	LpxChangeState( Connection, SMP_STATE_ESTABLISHED, TRUE );

	if (Connection->LpxSmp.ConnectIrp) {

		PIRP	irp;

		irp = Connection->LpxSmp.ConnectIrp;
		Connection->LpxSmp.ConnectIrp = NULL;

		SetFlag( Connection->Flags2, CONNECTION_FLAGS2_REQ_COMPLETED );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		IoAcquireCancelSpinLock( &cancelIrql );
		IoSetCancelRoutine( irp, NULL );
		IoReleaseCancelSpinLock( cancelIrql );
		irp->IoStatus.Status = STATUS_SUCCESS;

		LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );

		DebugPrint( 2, ("[LPX]SmpDoReceive: Connect IRP %p completed.\n ", irp) );

	} else if (Connection->LpxSmp.ListenIrp) {

		PIRP	                    irp;
		PIO_STACK_LOCATION	        irpSp;
		PTDI_REQUEST_KERNEL_LISTEN	request;
		PTDI_CONNECTION_INFORMATION	connectionInfo;

		irp = Connection->LpxSmp.ListenIrp;
		Connection->LpxSmp.ListenIrp = NULL;

		SetFlag( Connection->Flags2, CONNECTION_FLAGS2_REQ_COMPLETED );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		irpSp = IoGetCurrentIrpStackLocation( irp );
		request = (PTDI_REQUEST_KERNEL_LISTEN)&irpSp->Parameters;
		connectionInfo = request->ReturnConnectionInformation;

		if (connectionInfo != NULL) {

			connectionInfo->UserData = NULL;
			connectionInfo->UserDataLength = 0;
			connectionInfo->Options = NULL;
			connectionInfo->OptionsLength = 0;

			if (connectionInfo->RemoteAddressLength != 0) {

			    UCHAR				addressBuffer[FIELD_OFFSET(TRANSPORT_ADDRESS, Address) + FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX];
			    PTRANSPORT_ADDRESS  transportAddress;
			    ULONG               returnLength;
			    PTA_ADDRESS         taAddress;
			    PTDI_ADDRESS_LPX    lpxAddress;


			    DebugPrint(2, ("connectionInfo->RemoteAddressLength = %d, addressLength = %d\n", 
								connectionInfo->RemoteAddressLength, 
								(FIELD_OFFSET(TRANSPORT_ADDRESS, Address) + FIELD_OFFSET(TA_ADDRESS, Address)+ TDI_ADDRESS_LENGTH_LPX)) );
			                                    
			    transportAddress = (PTRANSPORT_ADDRESS)addressBuffer;

			    transportAddress->TAAddressCount = 1;
			    taAddress					= (PTA_ADDRESS)transportAddress->Address;
			    taAddress->AddressType      = TDI_ADDRESS_TYPE_LPX;
			    taAddress->AddressLength    = TDI_ADDRESS_LENGTH_LPX;

			    lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

			    RtlCopyMemory( lpxAddress,
							   &Connection->LpxSmp.DestinationAddress,
							   sizeof(LPX_ADDRESS) );

			    returnLength = (connectionInfo->RemoteAddressLength <=  
									(FIELD_OFFSET(TRANSPORT_ADDRESS, Address) + FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)) 
			                      ? connectionInfo->RemoteAddressLength
			                        : (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
			                           + FIELD_OFFSET(TA_ADDRESS, Address)
			                           + TDI_ADDRESS_LENGTH_LPX);

			    RtlCopyMemory( connectionInfo->RemoteAddress,
							   transportAddress,
							   returnLength );
			}
		}	    

		IoAcquireCancelSpinLock( &cancelIrql );
		IoSetCancelRoutine( irp, NULL );
		IoReleaseCancelSpinLock( cancelIrql );
		irp->IoStatus.Status = STATUS_SUCCESS;

		LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );

		DebugPrint( 2, ("[LPX]SmpDoReceive: Listen IRP %p completed.\n ", irp) );

	} else {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		DebugPrint( 2, ("[LPX]SmpDoReceive: No IRP to handle. Why do we in SYN_RECV state??????\n") );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );	
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenEstablished (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	BOOLEAN		    packetConsumed = FALSE;
	PNDIS_PACKET	ackPacket;
	KIRQL		    oldIrql;
	PLIST_ENTRY		reorderPacketList;
	BOOLEAN			sendAck = FALSE;
	BOOLEAN			irpCompleted;


	lpxHeader = &RESERVED(Packet)->LpxHeader;

	NDAS_ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_ESTABLISHED );	
	
	// Check Server Tag.

	if (lpxHeader->ServerTag != Connection->LpxSmp.ServerTag) {

		DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_ESTABLISHED: Bad Server Tag. Dropping. Connection->LpxSmp.ServerTag 0x%x, lpxHeader->ServerTag 0x%x\n", 
			             Connection->LpxSmp.ServerTag, lpxHeader->ServerTag) );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		Connection->LpxSmp.DropOfReceivePacket ++;
		PacketFree( Connection->AddressFile->Address->Provider, Packet );
		
		return TRUE;	    // Packet is consumed.
	}

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {

		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {
			
			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}
	}

	if (Connection->LpxSmp.RetransmitCount) {
	
		DebugPrint( 2, ("LPX: %d Retransmits left\n", Connection->LpxSmp.RetransmitCount) );
	}

	switch (NTOHS(lpxHeader->Lsctl) & LSCTL_MASK) {
	
	case LSCTL_ACKREQ:
	case LSCTL_ACKREQ | LSCTL_ACK:
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

		sendAck = TRUE;

		PacketFree( Connection->AddressFile->Address->Provider, Packet );
		packetConsumed = TRUE;
		break;

	case LSCTL_DATA:
	case LSCTL_DATA | LSCTL_ACK:

		if (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) <= sizeof(LPX_HEADER)) {

			NDAS_ASSERT(FALSE);

			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_ESTABLISHED: Data packet without Data!!!!!!!!!!!!!! SP: %p\n", 
							Connection) );
		}

		if (((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence))) > 0) {

			DebugPrint( 2, ("[LPX] Remote packet lost: HeaderSeq 0x%x, RS: 0x%x\n", 
							NTOHS(lpxHeader->Sequence), Connection->LpxSmp.RemoteSequence) );

			DebugPrint( 2, ("link: %02X%02X%02X%02X%02X%02X:%04X\n",
							Connection->LpxSmp.DestinationAddress.Node[0],
					        Connection->LpxSmp.DestinationAddress.Node[1],
							Connection->LpxSmp.DestinationAddress.Node[2],
					        Connection->LpxSmp.DestinationAddress.Node[3],
					        Connection->LpxSmp.DestinationAddress.Node[4],
							Connection->LpxSmp.DestinationAddress.Node[5],
						    Connection->LpxSmp.DestinationAddress.Port) );

			ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.ReceiveReorderQSpinLock );

			for (reorderPacketList = Connection->LpxSmp.ReceiveReorderQueue.Flink;
				 reorderPacketList != &Connection->LpxSmp.ReceiveReorderQueue;
				 reorderPacketList = reorderPacketList->Flink) {

				PLPX_RESERVED	reorderReserved;
				PLPX_HEADER		reorderLpxHeader;

				reorderReserved		= CONTAINING_RECORD( reorderPacketList, LPX_RESERVED, ListEntry );
				reorderLpxHeader	= &reorderReserved->LpxHeader;

				if (reorderLpxHeader->Sequence == lpxHeader->Sequence) {

					Connection->LpxSmp.DropOfReceivePacket ++;
					PacketFree( Connection->AddressFile->Address->Provider, Packet );
					packetConsumed = TRUE;

					break;
				}

				if (((SHORT)(NTOHS(reorderLpxHeader->Sequence) - NTOHS(lpxHeader->Sequence))) > 0) {

					InsertTailList( &reorderReserved->ListEntry, &RESERVED(Packet)->ListEntry );
					packetConsumed = TRUE;
					
					break;
				}
			}

			if (packetConsumed == FALSE) {

				InsertTailList( &Connection->LpxSmp.ReceiveReorderQueue, &RESERVED(Packet)->ListEntry );
				packetConsumed = TRUE;
			}

			RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.ReceiveReorderQSpinLock );

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			
			SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

			sendAck = TRUE;

			break;
		}

		if (((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence))) < 0) {

			DebugPrint( 2, ("Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", 
							NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) );
			
			// Netdisk retransmitted packet, which is possibly caused by packet loss.

			ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.ReceiveIrpQSpinLock, &oldIrql );

			if (!IsListEmpty(&Connection->LpxSmp.ReceiveIrpQueue)) {
			    
				PLIST_ENTRY	listEntry;
			    PIRP		FirstIrp;
			    
				listEntry = Connection->LpxSmp.ReceiveIrpQueue.Flink;
			    FirstIrp = CONTAINING_RECORD( listEntry, IRP, Tail.Overlay.ListEntry );
				InterlockedIncrement(&Connection->LpxSmp.TransportStats.PacketLoss);

			    DebugPrint( 2, ("Duplicated packet rxed. Increasing rx packet loss count\n") );
			}

			RELEASE_SPIN_LOCK( &Connection->LpxSmp.ReceiveIrpQSpinLock, oldIrql );

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

			sendAck = TRUE;

			Connection->LpxSmp.DropOfReceivePacket ++;
			PacketFree( Connection->AddressFile->Address->Provider, Packet );
			packetConsumed = TRUE;

			break;
		}

		NDAS_ASSERT( ((SHORT)(NTOHS(lpxHeader->Sequence) == SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence))) );
		
		Connection->LpxSmp.RemoteSequence ++;

		if (FlagOn(lpxHeader->Option, LPX_OPTION_PACKETS_CONTINUE_BIT)) {
			
			if (Connection->LpxSmp.DestinationSmallLengthDelayedAck == TRUE &&
				Connection->LpxSmp.RemoteSequence % Connection->LpxSmp.PcbWindow == 0) {

				sendAck = TRUE;
			
			} else {
			
				sendAck = FALSE;
			}

		} else {

			sendAck = TRUE;
		}

		ExInterlockedInsertTailList( &Connection->LpxSmp.RecvDataQueue,
									 &RESERVED(Packet)->ListEntry,
									 &Connection->LpxSmp.RecvDataQSpinLock );

		ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.ReceiveReorderQSpinLock );

		for (reorderPacketList = Connection->LpxSmp.ReceiveReorderQueue.Flink;
			 reorderPacketList != &Connection->LpxSmp.ReceiveReorderQueue;
			 ) {

			PLPX_RESERVED	reorderReserved;
			PLPX_HEADER		reorderLpxHeader;

			reorderReserved		= CONTAINING_RECORD( reorderPacketList, LPX_RESERVED, ListEntry );
			reorderLpxHeader	= &reorderReserved->LpxHeader;

			reorderPacketList = reorderPacketList->Flink;

			if (NTOHS(reorderLpxHeader->Sequence) == SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) {

				RemoveEntryList( &reorderReserved->ListEntry );
				InitializeListHead( &reorderReserved->ListEntry );

				Connection->LpxSmp.RemoteSequence ++;

				if (sendAck == FALSE) {

					if (FlagOn(lpxHeader->Option, LPX_OPTION_PACKETS_CONTINUE_BIT)) {

						sendAck = FALSE;

					} else {

						sendAck = TRUE;
					}
				}

				ExInterlockedInsertTailList( &Connection->LpxSmp.RecvDataQueue,
											 &reorderReserved->ListEntry,
											 &Connection->LpxSmp.RecvDataQSpinLock );

				continue;
			}

			NDAS_ASSERT( ((SHORT)(NTOHS(reorderLpxHeader->Sequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence))) > 0 );

			break;
		}

		SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

		RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.ReceiveReorderQSpinLock );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );


		if (sendAck == TRUE) {

			if (Connection->LpxSmp.DestinationAddress.Port == HTONS(10000)) { // ndas chip below 2.0

				if (Connection->LpxSmp.PcbWindow == 0) {

					sendAck = FALSE;
				}
			} 
		}

#if 0
		// Force to send an ack packet.
		// Some NIC drivers require frequent ack packets to boost packet receiving.
		// Maybe the NIC drivers are optimized for TCP/IP packet exchange.
		// Currently, this symtom is identified on Widows Vista 32.
		// Ex> Broadcom Giga ehternet 3788. driver name = b57nd60x.sys 11/1/2005/11:30 164KB

		if (sendAck == FALSE) {

			if (Connection->AddressFile->Address->Provider->ForcedAckPerPackets &&
				(Connection->LpxSmp.RemoteSequence % Connection->AddressFile->Address->Provider->ForcedAckPerPackets) == 0) {

				sendAck = TRUE;
			}
		}
#endif

		LpxSmpProcessReceivePacket( Connection, &irpCompleted );

		if (irpCompleted == TRUE) {

			sendAck = TRUE;
		}

		packetConsumed = TRUE;

		break;

	case LSCTL_DISCONNREQ:
	case LSCTL_DISCONNREQ | LSCTL_ACK:

		if (((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence))) > 0) {
		
			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

			sendAck = TRUE;

			PacketFree( Connection->AddressFile->Address->Provider, Packet );
			packetConsumed = TRUE;

			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_ESTABLISHED: remote packet lost: HeaderSeq 0x%x, RS: 0x%x\n", 
							NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) );

			break;
		}

		if (((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence))) < 0) {

			NDAS_ASSERT(FALSE);

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

			sendAck = TRUE;

			PacketFree( Connection->AddressFile->Address->Provider, Packet );
			packetConsumed = TRUE;

			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_ESTABLISHED: Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", 
							NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) );

			break;
		}

		Connection->LpxSmp.RemoteSequence ++;

		ExInterlockedInsertTailList( &Connection->LpxSmp.RecvDataQueue,
									 &RESERVED(Packet)->ListEntry,
									 &Connection->LpxSmp.RecvDataQSpinLock );

		LpxChangeState( Connection, SMP_STATE_CLOSE_WAIT, TRUE );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		while (!IsListEmpty(FreePacketList)) {

			PLIST_ENTRY		packetListEntry;
			PLPX_RESERVED	reserved;
			PNDIS_PACKET	packet;

			packetListEntry = FreePacketList->Flink;
			reserved		= CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
			packet			= reserved->Packet;
		
			RemoveEntryList( &reserved->ListEntry );
			InitializeListHead( &reserved->ListEntry );

			PacketFree( Connection->AddressFile->Address->Provider, packet );
		}

		LpxSmpProcessReceivePacket( Connection, NULL );

		SetFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );

		sendAck = TRUE;

		DebugPrint( 2, ("[LPX] Receive : LSCTL_DISCONNREQ | LSCTL_ACK: SmpDoReceive from establish to SMP_STATE_CLOSE_WAIT\n") );

		packetConsumed = TRUE;

		break;

	default:

		if (!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)) {

			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_ESTABLISHED: Unexpected packet received %x\n", 
							NTOHS(lpxHeader->Lsctl)) );
		}

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		PacketFree( Connection->AddressFile->Address->Provider, Packet );
		packetConsumed = TRUE;	    

		break;
	}

	if (sendAck) {

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket) {

			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

			ClearFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_DELAYED_ACK );
		}
	}

	return packetConsumed;
}


BOOLEAN 
LpxStateDoReceiveWhenFinWait1 (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	PNDIS_PACKET	ackPacket;


	lpxHeader	 = &RESERVED(Packet)->LpxHeader;
	
	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_FIN_WAIT1 );

	DebugPrint( 2, ("SmpDoReceive SMP_STATE_FIN_WAIT1 lpxHeader->Lsctl = %d\n", NTOHS(lpxHeader->Lsctl)) );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_DATA | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT1:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 
	
	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
	
		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {

			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}

		if (SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence) == SHORT_SEQNUM(Connection->LpxSmp.FinSequence)) {
		
			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT1: SMP_STATE_FIN_WAIT1 to SMP_STATE_FIN_WAIT2\n") );
			LpxChangeState( Connection, SMP_STATE_FIN_WAIT2, TRUE );

			if (Connection->LpxSmp.DisconnectIrp == NULL) {

				NDAS_ASSERT(FALSE);			
			
			} else {
			    
				PIRP	irp;
				KIRQL	cancelIrql;

				irp = Connection->LpxSmp.DisconnectIrp;
			    Connection->LpxSmp.DisconnectIrp = NULL;
			    
				IoAcquireCancelSpinLock( &cancelIrql );
			    IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelIrql );
			    
			    irp->IoStatus.Status = STATUS_SUCCESS;
				DebugPrint( 2, ("[LPX]LpxStateDoReceiveWhenFinWait1: Disconnect IRP %p completed.\n ", irp) );

				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );
			} 
		}
	} 

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {

		KIRQL		    oldIrqlTimeCnt;

		Connection->LpxSmp.RemoteSequence ++;

		ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, &oldIrqlTimeCnt );
		Connection->LpxSmp.LastAckTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.LastAckInterval.QuadPart;
		RELEASE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, oldIrqlTimeCnt );

		LpxChangeState( Connection, SMP_STATE_CLOSING, TRUE );
		
		if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {

			if (SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence) == SHORT_SEQNUM(Connection->LpxSmp.FinSequence)) {

			    Connection->LpxSmp.TimeWaitTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.TimeWaitInterval.QuadPart;

				DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT1: entering SMP_STATE_TIME_WAIT due to RemoteAckSequence == FinSequence\n") );
				LpxChangeState( Connection, SMP_STATE_TIME_WAIT, TRUE );

			} else {
			
				DebugPrint(2, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT1: RemoteAckSequence != FinSequence\n"));
			}
		}

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );
	
	} else if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACKREQ) {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

	} else if (NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {

		if (NTOHS(lpxHeader->Sequence) == SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) {

			Connection->LpxSmp.RemoteSequence ++;

			ExInterlockedInsertTailList( &Connection->LpxSmp.RecvDataQueue,
										 &RESERVED(Packet)->ListEntry,
										 &Connection->LpxSmp.RecvDataQSpinLock );
		}

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		LpxSmpProcessReceivePacket( Connection, NULL );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

		return TRUE;

	} else {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );		
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenFinWait2 (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	KIRQL		    oldIrqlTimeCnt;
	PNDIS_PACKET	ackPacket;
	
	UNREFERENCED_PARAMETER( FreePacketList );
	
	lpxHeader	 = &RESERVED(Packet)->LpxHeader;
	
	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_FIN_WAIT2 );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_DATA | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT2:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 

	DebugPrint( 4, ("SmpDoReceive SMP_STATE_FIN_WAIT2  lpxHeader->Lsctl = 0x%x\n", NTOHS(lpxHeader->Lsctl)) );

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {

		Connection->LpxSmp.RemoteSequence ++;
		
		ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, &oldIrqlTimeCnt );
		Connection->LpxSmp.TimeWaitTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.TimeWaitInterval.QuadPart;
		RELEASE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, oldIrqlTimeCnt );

		DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT2: entering SMP_STATE_TIME_WAIT\n") );
		LpxChangeState( Connection, SMP_STATE_TIME_WAIT, TRUE );

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );
	
	} else if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACKREQ) {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

	} else if (NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {

		if (NTOHS(lpxHeader->Sequence) == SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) {

			Connection->LpxSmp.RemoteSequence ++;

			ExInterlockedInsertTailList( &Connection->LpxSmp.RecvDataQueue,
										 &RESERVED(Packet)->ListEntry,
										 &Connection->LpxSmp.RecvDataQSpinLock );
		}

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		LpxSmpProcessReceivePacket( Connection, NULL );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

		return TRUE;

	} else {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenClosing (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	PNDIS_PACKET	ackPacket;


	lpxHeader	 = &RESERVED(Packet)->LpxHeader;
	
	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_CLOSING );
	
	DebugPrint( 2, ("SmpDoReceive SMP_STATE_CLOSING lpxHeader->Lsctl = %d\n", NTOHS(lpxHeader->Lsctl)) );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_CLOSING:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 
	
	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
	
		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {

			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}

		if (SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence) == SHORT_SEQNUM(Connection->LpxSmp.FinSequence)) {
		
			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT1: SMP_STATE_CLOSING to SMP_STATE_TIME_WAIT\n") );
			LpxChangeState( Connection, SMP_STATE_TIME_WAIT, TRUE );

			if (Connection->LpxSmp.DisconnectIrp == NULL) {

				NDAS_ASSERT(FALSE);			
			
			} else {
			    
				PIRP	irp;
				KIRQL	cancelIrql;

				irp = Connection->LpxSmp.DisconnectIrp;
			    Connection->LpxSmp.DisconnectIrp = NULL;
			    
				IoAcquireCancelSpinLock( &cancelIrql );
			    IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelIrql );
			    
			    irp->IoStatus.Status = STATUS_SUCCESS;
				DebugPrint( 2, ("[LPX]LpxStateDoReceiveWhenClosing: Disconnect IRP %p completed.\n ", irp) );

				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );
			} 
		}
	} 

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACKREQ) {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );
	
	} else {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );		
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenCloseWait (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER	lpxHeader;


	lpxHeader = &RESERVED(Packet)->LpxHeader;

	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_CLOSE_WAIT );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_CLOSE_WAIT:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 

	if (NTOHS(lpxHeader->Lsctl) == LSCTL_ACK) {

		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {

			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}
	}

	RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

	PacketFree( Connection->AddressFile->Address->Provider, Packet );
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenLastAck (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER	lpxHeader;


	lpxHeader	 = &RESERVED(Packet)->LpxHeader;


	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_LAST_ACK );	

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_LAST_ACK:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 

	if (NTOHS(lpxHeader->Lsctl) == LSCTL_ACK) {
		
		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) > 0) {

			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}

		if (SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence) == SHORT_SEQNUM(Connection->LpxSmp.FinSequence)) {
		
			KIRQL    oldIrqlTimeCnt;

			DebugPrint( 2, ("[LPX] SmpDoReceive: entering SMP_STATE_TIME_WAIT due to RemoteAckSequence == FinSequence\n") );
			
			LpxChangeState( Connection, SMP_STATE_TIME_WAIT, TRUE );

			ACQUIRE_SPIN_LOCK(&Connection->LpxSmp.TimeCounterSpinLock, &oldIrqlTimeCnt);
			Connection->LpxSmp.TimeWaitTimeOut.QuadPart = NdasCurrentTime().QuadPart + Connection->LpxSmp.TimeWaitInterval.QuadPart;
			RELEASE_SPIN_LOCK(&Connection->LpxSmp.TimeCounterSpinLock, oldIrqlTimeCnt);

			if (Connection->LpxSmp.DisconnectIrp == NULL) {

				NDAS_ASSERT(FALSE);			
			
			} else {
			    
				PIRP	irp;
				KIRQL	cancelIrql;

				irp = Connection->LpxSmp.DisconnectIrp;
			    Connection->LpxSmp.DisconnectIrp = NULL;
			    
				IoAcquireCancelSpinLock( &cancelIrql );
			    IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelIrql );
			    
			    irp->IoStatus.Status = STATUS_SUCCESS;
				DebugPrint( 0, ("[LPX]LpxStateDoReceiveWhenLastAck: Disconnect IRP %p completed.\n ", irp) );

				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );
			} 
		}
	
	} else {

		DebugPrint( 2, ("SmpDoReceive Unexpected packet %x\n", NTOHS(lpxHeader->Lsctl) == LSCTL_ACK) );
	}

	RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

	PacketFree( Connection->AddressFile->Address->Provider, Packet );		
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenTimeWait (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	PNDIS_PACKET	ackPacket;

	UNREFERENCED_PARAMETER( FreePacketList );

	lpxHeader = &RESERVED(Packet)->LpxHeader;

	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_TIME_WAIT );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_TIME_WAIT:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 

	if (NTOHS(lpxHeader->Lsctl) == (LSCTL_DISCONNREQ | LSCTL_ACK)) {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );
	
	} else {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );
	return TRUE;
}

//
//	acquire Connection->LpxSmp.SpinLock before calling
//
//	called only from SmpDoReceive()


VOID
SmpRetransmitCheck (
	IN PTP_CONNECTION	Connection,
	IN  USHORT			AckSequence,
	OUT PLIST_ENTRY		FreePacketList
	)
{
	PLIST_ENTRY			packetListEntry;
	PLPX_RESERVED		reserved;
	PNDIS_PACKET		packet;

	PLPX_HEADER			lpxHeader;
	KIRQL				oldIrql;


	DebugPrint( 3, ("[LPX]SmpRetransmitCheck: Entered.\n") );

	ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );

	while (!IsListEmpty(&Connection->LpxSmp.TramsmitQueue)) {

		LARGE_INTEGER systemTime;

		packetListEntry = Connection->LpxSmp.TramsmitQueue.Flink;
		 
		reserved = CONTAINING_RECORD( packetListEntry, LPX_RESERVED, ListEntry );
//		packet	= CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
		packet = reserved->Packet;

		lpxHeader = &RESERVED(packet)->LpxHeader;

		DebugPrint( 3, ("AckSequence = %x, lpxHeader->Sequence = %x\n",
			             AckSequence, NTOHS(lpxHeader->Sequence)) );

		if ((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(AckSequence)) >= 0) {

			break;
		}

		KeQuerySystemTime( &systemTime );

#if __LPX_STATISTICS__

		Connection->LpxSmp.ResponseTimeOfSendPackets.QuadPart += systemTime.QuadPart - RESERVED(packet)->SendTime.QuadPart;					
		Connection->LpxSmp.BytesOfSendPackets.QuadPart += NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);

		if (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)+14 <= 60) {

			Connection->LpxSmp.NumberofSmallSendPackets ++;
			Connection->LpxSmp.ResponseTimeOfSmallSendPackets.QuadPart += systemTime.QuadPart - RESERVED(packet)->SendTime.QuadPart;					
			Connection->LpxSmp.BytesOfSmallSendPackets.QuadPart += NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);
			
		} else {

			Connection->LpxSmp.NumberofLargeSendPackets ++;
			Connection->LpxSmp.ResponseTimeOfLargeSendPackets.QuadPart += systemTime.QuadPart - RESERVED(packet)->SendTime.QuadPart;					
			Connection->LpxSmp.BytesOfLargeSendPackets.QuadPart += NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);
		}

#endif

		RemoveEntryList( &reserved->ListEntry );
		InitializeListHead( &reserved->ListEntry );
		InsertTailList( FreePacketList, &reserved->ListEntry );
	}
	
	RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TransmitSpinLock );

	if (IsListEmpty(FreePacketList))  {

		if (Connection->LpxSmp.CanceledByUser == FALSE) {

			NDAS_ASSERT(FALSE);
		}

		return;
	}

	InterlockedExchange( &Connection->LpxSmp.RetransmitCount, 0 );

	ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, &oldIrql );
	Connection->LpxSmp.RetransmitTimeOut.QuadPart = NdasCurrentTime().QuadPart + CalculateRTT(Connection);
	RELEASE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, oldIrql );

#if 0
	ACQUIRE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );
	Connection->LpxSmp.RetransmitEndTime.QuadPart = NdasCurrentTime().QuadPart + (LONGLONG)LpxMaxRetransmitTime;
	RELEASE_DPC_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock );
#else
	ACQUIRE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, &oldIrql );
	Connection->LpxSmp.MaxStallTimeOut.QuadPart = NdasCurrentTime().QuadPart + (LONGLONG)Connection->LpxSmp.MaxStallInterval.QuadPart;
	RELEASE_SPIN_LOCK( &Connection->LpxSmp.TimeCounterSpinLock, oldIrql );
#endif

	return;
}


VOID 
LpxIoCompleteRequest (
    IN PIRP  Irp,
    IN CCHAR  PriorityBoost
    )
{

	if (Irp->Tail.Overlay.DriverContext[0]) {

		if (((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection) {

			LpxDereferenceConnection( "LpxIoCompleteRequest", 
									  ((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection, 
									  CREF_LPX_PRIVATE );
		
		} else {

			NDAS_ASSERT(FALSE);			
		}

		ExFreePool( Irp->Tail.Overlay.DriverContext[0] );
		Irp->Tail.Overlay.DriverContext[0] = 0;
	}

	IoCompleteRequest( Irp, PriorityBoost );
	return;
}

#if DBG
VOID
SmpPrintState (
	IN	LONG	        Debuglevel,
	IN	PCHAR	        Where,
	IN PTP_CONNECTION	Connection
	)
{
	DebugPrint( Debuglevel, (Where) );
	
	DebugPrint( Debuglevel, (" : SP %p, Seq 0x%x, RSeq 0x%x, RAck 0x%x", 
							  Connection, SHORT_SEQNUM(Connection->LpxSmp.Sequence), SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence),
							  SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence)) );

	DebugPrint(	Debuglevel, (" #Sent %ld, #SentCom %ld, CT %I64d  RT %I64d\n", 
							   NumberOfSent, NumberOfSentComplete, NdasCurrentTime().QuadPart, Connection->LpxSmp.RetransmitTimeOut.QuadPart) );
}
#else
#define SmpPrintState(l, w, s) 
#endif

#if DBG

VOID
PrintStatistics ( 
	LONG			DebugLevel2,
	PTP_CONNECTION  Connection
	)
{
	if (Connection->LpxSmp.NumberofSmallSendRequests > 100 ) {

#if __LPX_STATISTICS__

		DebugPrint( DebugLevel2, ("\nConnection->LpxSmp.NumberOfSendPackets = %u\n", Connection->LpxSmp.NumberOfSendPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfSendPackets = %I64u MiliSecond\n", Connection->LpxSmp.ResponseTimeOfSendPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfSendPackets = %I64u Kbytes\n", Connection->LpxSmp.BytesOfSendPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofSmallSendPackets = %u\n", Connection->LpxSmp.NumberofSmallSendPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfSmallSendPackets = %I64u MiliSecond\n", Connection->LpxSmp.ResponseTimeOfSmallSendPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfSmallSendPackets = %I64u Kbytes \n", Connection->LpxSmp.BytesOfSmallSendPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofLargeSendPackets = %d\n", Connection->LpxSmp.NumberofLargeSendPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfLargeSendPackets = %I64u MiliSecond\n", Connection->LpxSmp.ResponseTimeOfLargeSendPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfLargeSendPackets = %I64u Kbytes\n", Connection->LpxSmp.BytesOfLargeSendPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Throughput = %I64u Bytes\n", Connection->LpxSmp.BytesOfSendPackets.QuadPart * 1000 * 1000 * 10 / Connection->LpxSmp.ResponseTimeOfSendPackets.QuadPart) );
		DebugPrint( DebugLevel2, ("Delay = %I64u NanoSecond\n", Connection->LpxSmp.ResponseTimeOfSmallSendPackets.QuadPart * 10 / Connection->LpxSmp.NumberofSmallSendPackets) );

		DebugPrint( DebugLevel2, ("\nConnection->LpxSmp.NumberofRecvPackets = %u\n", Connection->LpxSmp.NumberofRecvPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfRecvPackets = %I64u Kbytes\n", Connection->LpxSmp.BytesOfRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofSmallRecvPackets = %u\n", Connection->LpxSmp.NumberofSmallRecvPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfSmallRecvPackets = %I64u Kbytes \n", Connection->LpxSmp.BytesOfSmallRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofLargeRecvPackets = %d\n", Connection->LpxSmp.NumberofLargeRecvPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfLargeRecvPackets = %I64u Kbytes\n", Connection->LpxSmp.BytesOfLargeRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("\nConnection->LpxSmp.NumberofRecvPackets = %u\n", Connection->LpxSmp.NumberofRecvPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfRecvPackets = %I64u Kbytes\n", Connection->LpxSmp.BytesOfRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofSmallRecvPackets = %u\n", Connection->LpxSmp.NumberofSmallRecvPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfSmallRecvPackets = %I64u Kbytes \n", Connection->LpxSmp.BytesOfSmallRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofLargeRecvPackets = %d\n", Connection->LpxSmp.NumberofLargeRecvPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfLargeRecvPackets = %I64u Kbytes\n", Connection->LpxSmp.BytesOfLargeRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofLargeRecvRequests = %d\n", Connection->LpxSmp.NumberofLargeRecvRequests) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfLargeRecvRequests = %I64u MiliSecond\n", 
								   Connection->LpxSmp.ResponseTimeOfLargeRecvRequests.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfLargeRecvRequests = %I64u Kbytes\n", 
								   Connection->LpxSmp.BytesOfLargeRecvRequests.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofSmallRecvRequests = %u\n", Connection->LpxSmp.NumberofSmallRecvRequests) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfSmallRecvRequests = %I64u MiliSecond\n", 
								   Connection->LpxSmp.ResponseTimeOfSmallRecvRequests.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfSmallRecvRequests = %I64u Kbytes \n", 
								   Connection->LpxSmp.BytesOfSmallRecvRequests.QuadPart / 1000) );

#else
		
		DebugPrint( DebugLevel2, ("\nConnection->LpxSmp.NumberOfSendPackets = %u\n", Connection->LpxSmp.NumberOfSendPackets) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofRecvPackets = %u\n\n", Connection->LpxSmp.NumberofRecvPackets) );

#endif

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofLargeSendRequests = %d\n", Connection->LpxSmp.NumberofLargeSendRequests) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfLargeSendRequests = %I64u MiliSecond\n", Connection->LpxSmp.ResponseTimeOfLargeSendRequests.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfLargeSendRequests = %I64u Kbytes\n", Connection->LpxSmp.BytesOfLargeSendRequests.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberofSmallSendRequests = %u\n", Connection->LpxSmp.NumberofSmallSendRequests) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.ResponseTimeOfSmallSendRequests = %I64u MiliSecond\n", Connection->LpxSmp.ResponseTimeOfSmallSendRequests.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.BytesOfSmallSendRequests = %I64u Kbytes \n", Connection->LpxSmp.BytesOfSmallSendRequests.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("Connection->LpxSmp.NumberOfSendRetransmission = %u\n", Connection->LpxSmp.NumberOfSendRetransmission) );
		DebugPrint( DebugLevel2, ("Connection->LpxSmp.DropOfReceivePacket = %u\n\n", Connection->LpxSmp.DropOfReceivePacket) );

		if (Connection->LpxSmp.ResponseTimeOfLargeSendRequests.QuadPart)
			DebugPrint( DebugLevel2, ("Throughput = %I64u Bytes\n", Connection->LpxSmp.BytesOfLargeSendRequests.QuadPart * 1000 * 1000 * 10 / Connection->LpxSmp.ResponseTimeOfLargeSendRequests.QuadPart) );

		if (Connection->LpxSmp.NumberofSmallSendRequests)
			DebugPrint( DebugLevel2, ("Delay = %I64u NanoSecond\n", Connection->LpxSmp.ResponseTimeOfSmallSendRequests.QuadPart * 10 / Connection->LpxSmp.NumberofSmallSendRequests) );
	}
}

#endif

#if __LPX_STATISTICS__

VOID
PrintDeviceContextStatistics ( 
	LONG			DebugLevel2,
	PDEVICE_CONTEXT	DeviceContext
	)
{
	if (DeviceContext->NumberOfRecvPackets > 100) {

		DebugPrint( DebugLevel2, ("\nDeviceContext->NumberOfRecvPackets = %u\n", DeviceContext->NumberOfRecvPackets) );
		DebugPrint( DebugLevel2, ("DeviceContext->FreeTimeOfRecvPackets = %I64u MiliSecond\n", DeviceContext->FreeTimeOfRecvPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("DeviceContext->BytesOfRecvPackets = %I64u Kbytes\n", DeviceContext->BytesOfRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("\nDeviceContext->NumberOfSmallRecvPackets = %u\n", DeviceContext->NumberOfSmallRecvPackets) );
		DebugPrint( DebugLevel2, ("DeviceContext->FreeTimeOfSmallRecvPackets = %I64u MiliSecond\n", DeviceContext->FreeTimeOfSmallRecvPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("DeviceContext->BytesOfSmallRecvPackets = %I64u Kbytes\n", DeviceContext->BytesOfSmallRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("\nDeviceContext->NumberOfLargeRecvPackets = %u\n", DeviceContext->NumberOfLargeRecvPackets) );
		DebugPrint( DebugLevel2, ("DeviceContext->FreeTimeOfLargeRecvPackets = %I64u MiliSecond\n", DeviceContext->FreeTimeOfLargeRecvPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("DeviceContext->BytesOfLargeRecvPackets = %I64u Kbytes\n", DeviceContext->BytesOfLargeRecvPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("\nDeviceContext->NumberOfCompleteSendPackets = %u\n", DeviceContext->NumberOfCompleteSendPackets) );
		DebugPrint( DebugLevel2, ("DeviceContext->CompleteTimeOfSendPackets = %I64u MiliSecond\n", DeviceContext->CompleteTimeOfSendPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("DeviceContext->BytesOfCompleteSendPackets = %I64u Kbytes\n", DeviceContext->BytesOfCompleteSendPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("\nDeviceContext->NumberOfCompleteSmallSendPackets = %u\n", DeviceContext->NumberOfCompleteSmallSendPackets) );
		DebugPrint( DebugLevel2, ("DeviceContext->CompleteTimeOfSmallSendPackets = %I64u MiliSecond\n", DeviceContext->CompleteTimeOfSmallSendPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("DeviceContext->BytesOfCompleteSmallSendPackets = %I64u Kbytes\n", DeviceContext->BytesOfCompleteSmallSendPackets.QuadPart / 1000) );

		DebugPrint( DebugLevel2, ("\nDeviceContext->NumberOfCompleteLargeSendPackets = %u\n", DeviceContext->NumberOfCompleteLargeSendPackets) );
		DebugPrint( DebugLevel2, ("DeviceContext->CompleteTimeOfLargeSendPackets = %I64u MiliSecond\n", DeviceContext->CompleteTimeOfLargeSendPackets.QuadPart / NANO100_PER_MSEC) );
		DebugPrint( DebugLevel2, ("DeviceContext->BytesOfCompleteLargeSendPackets = %I64u Kbytes\n\n", DeviceContext->BytesOfCompleteLargeSendPackets.QuadPart / 1000) );
	}
}

#endif

#endif
