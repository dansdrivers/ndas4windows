#include "precomp.h"
#pragma hdrstop


//
//	IRP cancel routine
//
//	used only in LpxSend()
//
//	Disable connection !!!!
VOID
LpxCancelSend(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP Irp
			  )
{
	KIRQL oldirql, cancelIrql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
    PIRP SendIrp;
    PLIST_ENTRY p;
    BOOLEAN Found;
	UINT	currentCount = 0;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelIrp\n"));
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
	ServicePoint = (PSERVICE_POINT)Connection;
	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;

	// change ref counter for diable irp completing 
	currentCount = IRP_SEND_REFCOUNT(IrpSp);
	currentCount ++;
	IRP_SEND_REFCOUNT(IrpSp) = currentCount;

	lpx_ReferenceConnection (Connection);	// connection ref + : 1

	SmpFreeServicePoint(ServicePoint);		

    lpx_DereferenceConnection (Connection);	// connection ref - : 0
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 
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
//	used only in LpxRecv()
//
// acquire/release servicePoint spinlock
VOID
LpxCancelRecv(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP			Irp
			  )
{
	KIRQL				oldirql;
	PIO_STACK_LOCATION	IrpSp;
	PTP_CONNECTION		pConnection;
	PSERVICE_POINT		pServicePoint;
	KIRQL	cancelIrql;	

#if DBG
	DebugPrint(1, ("[LPX]LpxCancelRecv: Canceled Recv IRP %lx\n ", Irp));
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

#if DBG
	DbgPrint("[LPX] LpxCancelRecv: Cancelled receive IRP %lx on %lx\n",
		Irp, pConnection);
#endif

	//
    // Remove IRP from ReceiveIrpList.
    //
	ACQUIRE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, &oldirql);	// serviePoint + : 1

	RemoveEntryList(
		&Irp->Tail.Overlay.ListEntry
		);

	RELEASE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, oldirql);	// servicePoint - : 0
		
	//
	// Unset Cancel Routine.
	//
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql);

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
//	used only in LpxConnect()
//
// Disable connection !!!!!!
VOID
LpxCancelConnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
	KIRQL oldirql, cancelIrql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
    PIRP SendIrp;
    PLIST_ENTRY p;
    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelIrp\n"));
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
	ServicePoint = (PSERVICE_POINT)Connection;
	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	
	
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 
	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
	
	ServicePoint->ConnectIrp = NULL;
	
	
	lpx_ReferenceConnection (Connection);	//connection ref + : 1

	SmpFreeServicePoint(ServicePoint);

    lpx_DereferenceConnection (Connection);	// connection ref - : 0
}

//
//	IRP cancel routine
//
//	used only in LpxDisconnect()
//
//	Disable connection!!!!!!!!
VOID
LpxCancelDisconnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
	KIRQL oldirql, cancelIrql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
    PIRP SendIrp;
    PLIST_ENTRY p;
    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelIrp\n"));
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
	ServicePoint = (PSERVICE_POINT)Connection;
	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 
	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	ServicePoint->DisconnectIrp = NULL;
	
	lpx_ReferenceConnection (Connection);	// connection ref + : 1

	SmpFreeServicePoint(ServicePoint);

    lpx_DereferenceConnection (Connection);	// connection ref - : 0
}

/*
VOID
LpxCancelListen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    
#if DBG
        DebugPrint(1, ("[LPX]LpxCancelListen: Canceled listen IRP %lx ", Irp));
#endif

		LpxCancelIrp(DeviceObject, Irp);
}
*/

LARGE_INTEGER CurrentTime(
	VOID
	)
{
	LARGE_INTEGER Time;
	
	KeQuerySystemTime(&Time);

	return Time;
}


// Disable connection !!!!!!!!!!!!
VOID
LpxCancelListen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
	KIRQL oldirql, cancelIrql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
    PIRP SendIrp;
    PLIST_ENTRY p;
    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelIrp\n"));
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
	ServicePoint = (PSERVICE_POINT)Connection;
	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 
	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	ServicePoint->ListenIrp = NULL;
	
	
	lpx_ReferenceConnection (Connection);	// connection ref + : 1

	SmpFreeServicePoint(ServicePoint);

    lpx_DereferenceConnection (Connection);	// connection ref - : 0
}





//
//	acquire SpinLock of DeviceContext before calling
//	comment by hootch 09042003
//
//	called only from NbfOpenAddress()
//
NTSTATUS
LpxAssignPort(
	IN PCONTROL_DEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	)
{
	BOOLEAN				notUsedPortFound;
	PLIST_ENTRY			listHead;
	PLIST_ENTRY			thisEntry;
	PTP_ADDRESS			address;
	USHORT				portNum;
	NTSTATUS			status;

	DebugPrint(1, ("Smp LPX_BIND %02x:%02x:%02x:%02x:%02x:%02x SourceAddress->Port = %x\n", 
		SourceAddress->Node[0],SourceAddress->Node[1],SourceAddress->Node[2],
		SourceAddress->Node[3],SourceAddress->Node[4],SourceAddress->Node[5],
		SourceAddress->Port));

	portNum = AddressDeviceContext->PortNum;
	listHead = &AddressDeviceContext->AddressDatabase;

	notUsedPortFound = FALSE;

	do {
		BOOLEAN	usedPort;

		portNum++;
		if(portNum == 0)
			portNum = LPX_PORTASSIGN_BEGIN ;


		usedPort = FALSE;

		for(thisEntry = AddressDeviceContext->AddressDatabase.Flink;
			thisEntry != listHead;
			thisEntry = thisEntry->Flink) 
		{
	        address = CONTAINING_RECORD (thisEntry, TP_ADDRESS, Linkage);

	        if (address->NetworkName != NULL) {
                if (address->NetworkName->LpxAddress.Port == portNum) {
					usedPort = TRUE;
					break;
				}
			}
		}
		if(usedPort == FALSE)
			notUsedPortFound = TRUE;

	} while(notUsedPortFound == FALSE && portNum != AddressDeviceContext->PortNum);
	
	if(notUsedPortFound	== FALSE) {
		DebugPrint(1, ("[Lpx] LpxAssignPort: couldn't find available port number\n") );
		status = STATUS_UNSUCCESSFUL;
		goto ErrorOut;
	}
	SourceAddress->Port = AddressDeviceContext->PortNum = portNum;
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
//	called only from NbfOpenAddress()
//			!!!!!!!!!!ref count : address + 1
PTP_ADDRESS
LpxLookupAddress(
    IN PCONTROL_DEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS		SourceAddress
    )
{
    PTP_ADDRESS address = NULL;
    PLIST_ENTRY p;
    ULONG i;


    p = DeviceContext->AddressDatabase.Flink;

    for (p = DeviceContext->AddressDatabase.Flink;
         p != &DeviceContext->AddressDatabase;
         p = p->Flink) {

        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        if ((address->State & ADDRESS_STATE_CLOSING) != 0) {
            continue;
        }

        //
        // If the network name is specified and the network names don't match,
        // then the addresses don't match.
        //

//        i = NETBIOS_NAME_LENGTH;        // length of a Netbios name
		i = sizeof(LPX_ADDRESS);

        if (address->NetworkName != NULL) {
            if (SourceAddress != NULL) {
                if (!RtlEqualMemory (
                        &address->NetworkName->LpxAddress,
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


		DebugPrint(1, ("NbfLookupAddress DC %lx: found %lx ", DeviceContext, address));


        lpx_ReferenceAddress (address);		// address ref + : 1
        return address;

    } /* for */

    //
    // The specified address was not found.
    //

 
	DebugPrint(1, ("LpxLookupAddress DC %lx: did not find \n"));
    

    return NULL;

} /* NbfLookupAddress */



//
//
//
// called only from NbfTdiDisassociateAddress() and LpxCloseConnection()
//
//	SpinLock : address 
//	!!!!!!!!!!! ref count : address -1
VOID
LpxDisassociateAddress(
    IN OUT	PTP_CONNECTION	Connection
    )
{
	PSERVICE_POINT	servicePoint;
	KIRQL			oldIrql ;
	PTP_ADDRESS		address ;

	DebugPrint(DebugLevel, ("LpxDisassociateAddress %p\n", Connection));

	servicePoint = &Connection->ServicePoint;
	address = servicePoint->Address ;
	if( address == NULL) return;

	//
	//	close Connection ( called Service Point in LPX )
	//
	if( SmpFreeServicePoint(servicePoint) == FALSE ) 
		return ;
	servicePoint->Address = NULL;

	// added by hootch 08262003
	ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldIrql);		//address + : 1
	DebugPrint(DebugLevel,("LpxDisassociateAddress : remove servicePoint->ServicePointListEntry %0x \n", servicePoint));
	RemoveEntryList(&servicePoint->ServicePointListEntry);
	InitializeListHead(&servicePoint->ServicePointListEntry);
	RELEASE_SPIN_LOCK (&address->SpinLock, oldIrql);		// address - : 0

	DebugPrint(10, ("LpxDisassociateAddress %p\n", Connection));	
	lpx_DereferenceAddress(address);						// address ref -: -1
	return;
}




//
//
//	called only from LpxCloseConnection()
//
// call LpxDisassociateAddress --> ref count : address  -1
VOID
LpxCloseConnection(
    IN OUT	PTP_CONNECTION	Connection
    )
{
	PTP_ADDRESS		address;
	PSERVICE_POINT	servicePoint;

	DebugPrint(2, ("LpxCloseConnection\n"));

	servicePoint = &Connection->ServicePoint;
	address = servicePoint->Address;

	if(address != NULL) {
		LpxDisassociateAddress(
			Connection
		);
	}

	return;
}




//
//	queues a received packet to InProgressPacketList
//
//	called from 
//		and LpxProtoReceiveIndicate(), called from LpxProtoReceiveIndication()
//
VOID
LpxTransferDataComplete(
						IN NDIS_HANDLE   ProtocolBindingContext,
						IN PNDIS_PACKET  Packet,
						IN NDIS_STATUS   Status,
						IN UINT          BytesTransfered
						)
{
	PDEVICE_CONTEXT     pDeviceContext;
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
	
	pDeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

	if(Status != NDIS_STATUS_SUCCESS){
		goto TossPacket;
	}

	
	if(NdisAllocateMemory(&lpxHeader, sizeof(LPX_HEADER2)) != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("No memory\n"));
		goto TossPacket;
	}
	

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
	{
		NdisFreeMemory(lpxHeader);
		goto TossPacket;
	}	
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
			NdisFreeMemory(lpxHeader);
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





VOID LpxCallUserReceiveHandler(
	PSERVICE_POINT	servicePoint
	)
{
	PNDIS_PACKET	packet;

//
//	Not need packets to call User ReceiveHandler in the context at this time.
//
//	removed by hootch 09052003
//
//	packet = PacketPeek(&servicePoint->SmpContext.RcvDataQueue, &servicePoint->ServicePointQSpinLock);
//	DebugPrint(2, ("before ReceiveHandler packet = %p\n", packet));
	
	if(/*packet &&*/ servicePoint->Connection && servicePoint->Connection->Address)
	{
	    ULONG ReceiveFlags;
	    NTSTATUS status;
		PIRP	irp = NULL;
	    ULONG indicateBytesTransferred;
	
	    ReceiveFlags = TDI_RECEIVE_AT_DISPATCH_LEVEL | TDI_RECEIVE_ENTIRE_MESSAGE | TDI_RECEIVE_NO_RESPONSE_EXP;

		DebugPrint(2, ("before ReceiveHandler ServicePoint->Connection->AddressFile=%d\n",
		servicePoint->Connection->Address->RegisteredReceiveHandler));
		status = (*servicePoint->Connection->Address->ReceiveHandler)(
		                servicePoint->Connection->Address->ReceiveHandlerContext,
			            servicePoint->Connection->Context,
				        ReceiveFlags,
					    0,
						0,             // BytesAvailable
	                    &indicateBytesTransferred,
		                NULL,
			            NULL);

		DebugPrint(2, ("status = %x, Irp = %p\n", status, irp));
	}

	return;
}




void LpxCompleteIRPRequest(
	IN 		PSERVICE_POINT	servicePoint,
	IN		PIRP			Irp
	) {
	LONG	cnt ;
	KIRQL	oldIrql;
	
	ExInterlockedInsertTailList(
		&servicePoint->ReceiveIrpList,
		&Irp->Tail.Overlay.ListEntry,
		&servicePoint->ReceiveIrpQSpinLock
	);

	cnt = InterlockedIncrement(&servicePoint->SmpContext.RequestCnt) ;
	if( cnt == 1 ) {
		ACQUIRE_SPIN_LOCK(&servicePoint->SmpContext.smpDpcSpinLock,&oldIrql);	// smpDpcSpinlock + : 1
		KeInsertQueueDpc(&servicePoint->SmpContext.SmpWorkDpc, NULL, NULL) ;
		RELEASE_SPIN_LOCK(&servicePoint->SmpContext.smpDpcSpinLock,oldIrql);	// smpDpcSpinlock - : 0
	}

}



VOID
SmpPrintState(
	IN	ULONG			DebugLevel,
	IN	PCHAR			Where,
	IN	PSERVICE_POINT	ServicePoint
	)
{
	PSMP_CONTEXT	smpContext = &ServicePoint->SmpContext;

	DebugPrint(DebugLevel, (Where));
	DebugPrint(DebugLevel, (" : SP %p, Seq 0x%x, RSeq 0x%x, RAck 0x%x", 
		ServicePoint, SHORT_SEQNUM(smpContext->Sequence), SHORT_SEQNUM(smpContext->RemoteSequence),
		SHORT_SEQNUM(smpContext->RemoteAck)));

//	OBSOULTE: IntervalTime
//	removed by hootch 09062003
//
//	DebugPrint(DebugLevel, (" LRetransSeq 0x%x, TimerR %d, #ExpPac %d, #Pac %d, #Cloned %d Int %llu", 
//		smpContext->LastRetransmitSequence, smpContext->TimerReason, 
//		NumberOfExportedPackets, NumberOfPackets, NumberOfCloned,  smpContext->IntervalTime.QuadPart));

	DebugPrint(DebugLevel, (" LRetransSeq 0x%x, TimerR 0x%x, #ExpPac %ld, #Pac %ld, #Cloned %ld", 
		SHORT_SEQNUM(smpContext->LastRetransmitSequence), SHORT_SEQNUM(smpContext->TimerReason), 
		NumberOfExportedPackets, NumberOfPackets, NumberOfCloned));

	DebugPrint(DebugLevel, (" #Sent %ld, #SentCom %ld, CT %llu\n", 
				NumberOfSent, NumberOfSentComplete, CurrentTime().QuadPart));
}


//
// Callers of this routine must be running at IRQL PASSIVE_LEVEL
//
// comment by hootch 08262003
//
VOID
SmpContextInit(
	IN PSERVICE_POINT	ServicePoint,
	IN PSMP_CONTEXT		SmpContext
	)
{
	DebugPrint(2, ("SmpContextInit ServicePoint = %p\n", ServicePoint));

	RtlZeroMemory(SmpContext,
				sizeof(SMP_CONTEXT));

	SmpContext->ServicePoint = ServicePoint;

	KeInitializeSpinLock(&SmpContext->TimeCounterSpinLock) ;
	SmpContext->Sequence		= 0;
	SmpContext->RemoteSequence	= 0;
	SmpContext->RemoteAck		= 0;
	SmpContext->ServerTag		= 0;

	SmpContext->MaxFlights = SMP_MAX_FLIGHT / 2;

//	OBSOULTE: IntervalTime
//	removed by hootch 09062003
//
//	SmpContext->IntervalTime.QuadPart = INITIAL_INTERVAL_TIME;

	KeInitializeTimer(&SmpContext->SmpTimer);

	//
	// Callers of this routine must be running at IRQL PASSIVE_LEVEL
	//
	// comment by hootch
	
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	KeInitializeDpc(&SmpContext->SmpTimerDpc, SmpTimerDpcRoutineRequest, SmpContext);
	KeInitializeDpc(&SmpContext->SmpWorkDpc, SmpWorkDpcRoutine, SmpContext);
	KeInitializeSpinLock(&SmpContext->smpDpcSpinLock);
	KeInitializeSpinLock(&SmpContext->SendQSpinLock);
	InitializeListHead(&SmpContext->SendQueue);
	KeInitializeSpinLock(&SmpContext->ReceiveQSpinLock);
	InitializeListHead(&SmpContext->ReceiveQueue);
	KeInitializeSpinLock(&SmpContext->RcvDataQSpinLock);
	InitializeListHead(&SmpContext->RcvDataQueue);
	KeInitializeSpinLock(&SmpContext->RetransmitQSpinLock);
	InitializeListHead(&SmpContext->RetransmitQueue);
	KeInitializeSpinLock(&SmpContext->WriteQSpinLock);
	InitializeListHead(&SmpContext->WriteQueue);

	return;
}

// Spinlock : servicePoint
// 
BOOLEAN
SmpFreeServicePoint(
	IN	PSERVICE_POINT	ServicePoint
	)
{
	PSMP_CONTEXT	smpContext = &ServicePoint->SmpContext;
	PNDIS_PACKET	packet;
	UINT			back_log = 0, receive_queue = 0, receivedata_queue = 0,
					write_queue = 0, retransmit_queue = 0;
	KIRQL			oldIrql ;

	// added by hootch 08262003
	ACQUIRE_SPIN_LOCK (&ServicePoint->SpinLock, &oldIrql);		// servicePoint + : 1

	if(ServicePoint->SmpState == SMP_CLOSE) {
		RELEASE_SPIN_LOCK (&ServicePoint->SpinLock, oldIrql);	// servicePoint - : 0
		return FALSE ;
	}

	KeCancelTimer(&ServicePoint->SmpContext.SmpTimer);

	//
	//	change the state to SMP_CLOSE to stop I/O in this connection
	//
	ServicePoint->SmpState = SMP_CLOSE;
	ServicePoint->Shutdown	= 0;

	DebugPrint(DebugLevel,("SmpFreeServicePoint : remove servicePoint->ServicePointListEntry %0x \n", ServicePoint));
	RemoveEntryList(&ServicePoint->ServicePointListEntry);
	InitializeListHead(&ServicePoint->ServicePointListEntry);

	// added by hootch 08262003
	RELEASE_SPIN_LOCK (&ServicePoint->SpinLock, oldIrql);	// servicePoint - : 0

	SmpPrintState(1, "SmpFreeServicePoint", ServicePoint);

	DebugPrint(1, ("sequence = %x, fin_seq = %x, rmt_seq = %x, rmt_ack = %x\n", 
		SHORT_SEQNUM(smpContext->Sequence), SHORT_SEQNUM(smpContext->FinSequence),
		SHORT_SEQNUM(smpContext->RemoteSequence),SHORT_SEQNUM(smpContext->RemoteAck)));
	DebugPrint(1, ("last_retransmit_seq=%x, reason = %x\n",
		SHORT_SEQNUM(smpContext->LastRetransmitSequence), smpContext->TimerReason));

	while(packet
		= PacketDequeue(&smpContext->WriteQueue, &smpContext->WriteQSpinLock))
	{
		PIO_STACK_LOCATION	irpSp;
		PIRP				irp;

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
		= PacketDequeue(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock))
	{
		PIO_STACK_LOCATION	irpSp;
		PIRP				irp;

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
			DebugPrint(1, ("[LPX]SmpFreeServicePoint: Cloned is NOT 0. %d\n", RESERVED(packet)->Cloned));
		}

		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock))
	{
		receive_queue++;
		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&smpContext->ReceiveQueue, &smpContext->ReceiveQSpinLock))
	{
		receive_queue++;
		PacketFree(packet);
	}



	DebugPrint(1, ("back_log = %d, receive_queue = %d, receivedata_queue = %d write_queue = %d, retransmit_queue = %d\n", 
			back_log, receive_queue, receivedata_queue, write_queue, retransmit_queue));

	SmpCancelIrps(ServicePoint);

	return TRUE ;
}



// Spinlock : ServicePoint
static VOID
SmpCancelIrps(
	IN PSERVICE_POINT	ServicePoint
	)
{
	PIRP				irp;
	PLIST_ENTRY			thisEntry;
	PIRP				pendingIrp;
	KIRQL				cancelIrql;
	KIRQL				oldIrql ;
	PDRIVER_CANCEL		oldCancelRoutine;


	DebugPrint(DebugLevel, ("SmpCancelIrps\n"));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;		// servicePoint + : 1

	if(ServicePoint->ConnectIrp) {
		DebugPrint(DebugLevel, ("SmpCancelIrps ConnectIrp\n"));

		irp = ServicePoint->ConnectIrp;
		ServicePoint->ConnectIrp = NULL;
		irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	 
		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);

		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
	}

	if(ServicePoint->ListenIrp) {
		DebugPrint(DebugLevel, ("SmpCancelIrps ListenIrp\n"));

		irp = ServicePoint->ListenIrp;
		ServicePoint->ListenIrp = NULL;
		irp->IoStatus.Status = STATUS_CANCELLED;

		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
	}

	if(ServicePoint->DisconnectIrp) {
		DebugPrint(DebugLevel, ("SmpCancelIrps DisconnectIrp\n"));

		irp = ServicePoint->DisconnectIrp;
		ServicePoint->DisconnectIrp = NULL;
		irp->IoStatus.Status = STATUS_CANCELLED;

		IoAcquireCancelSpinLock(&cancelIrql);
		oldCancelRoutine = IoSetCancelRoutine(irp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
	}

	RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0

    //
    // Walk through the RcvList and cancel all read IRPs.
    //

	while(thisEntry=ExInterlockedRemoveHeadList(&ServicePoint->ReceiveIrpList, &ServicePoint->ReceiveIrpQSpinLock) )
	{
		pendingIrp = CONTAINING_RECORD(thisEntry, IRP, Tail.Overlay.ListEntry);

        DebugPrint(DebugLevel, ("[LPX] SmpCancelIrps: Cancelled Receive IRP 0x%0x\n", pendingIrp));

        //
		//  Clear the IRP¡¯s cancel routine
        //
		IoAcquireCancelSpinLock(&cancelIrql);
		oldCancelRoutine = IoSetCancelRoutine(pendingIrp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);

        pendingIrp->IoStatus.Information = 0;
        pendingIrp->IoStatus.Status = STATUS_CANCELLED;

	    IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
    }

	return;
}


NTSTATUS
TransmitPacket(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN PACKET_TYPE		PacketType,
	IN USHORT			UserDataLength
	)
{
	PSMP_CONTEXT	smpContext	= &ServicePoint->SmpContext;
	PUCHAR			packetData;
	PLPX_HEADER2	lpxHeader;
	NTSTATUS		status;
	UCHAR			raised = 0;
	PNDIS_BUFFER	firstBuffer;	
	KIRQL			oldIrql ;
	USHORT			sequence ;
	USHORT			finsequence ;
	PTP_ADDRESS		address ;

	DebugPrint(3, ("TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));

	ASSERT(ServicePoint->Address);

//
//	we don't need to make sure that Address field is not NULL
//	because we have a reference to Connection before calling
//	hootch 09012003

	address = ServicePoint->Address ;
//	if(address == NULL) {
//			return STATUS_INSUFFICIENT_RESOURCES;
//	}

	ASSERT(address->PacketProvider);

//	if(ServicePoint->SmpState == SMP_SYN_RECV)
//		return STATUS_INSUFFICIENT_RESOURCES;
	if(Packet == NULL) {
		DebugPrint(3, ("before TransmitPacket size = 0x%x \n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
		status = PacketAllocate(
			ServicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			address->PacketProvider,
			SEND_TYPE,
			NULL,
			0,
			NULL,
			&Packet
			);
		DebugPrint(3, ("after TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
		if(!NT_SUCCESS(status)) {
			DebugPrint(1, ("[LPX]TransmitPacket: packet == NULL\n"));
			SmpPrintState(1, "[LPX]TransmitPacket: PacketAlloc", ServicePoint);
			return status;
		}
	}
	
	DebugPrint(3, ("after TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if(PacketType == DATA)
		lpxHeader->PacketSize = HTONS((USHORT)(SMP_SYS_PKT_LEN + UserDataLength));
	else
		lpxHeader->PacketSize = HTONS(SMP_SYS_PKT_LEN);

	lpxHeader->LpxType					= LPX_TYPE_STREAM;
//	{
//		USHORT	dataLength;
//		DebugPrint(2, ("dataLength = 0x%x\n", lpxHeader->PacketSize));
//		dataLength = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)) - sizeof(LPX_HEADER2);
//		DebugPrint(2, ("dataLength = 0x%x\n", dataLength));
//	}
//	lpxHeader->CheckSum				= 0;
	lpxHeader->DestinationPort		= ServicePoint->DestinationAddress.Port;
	lpxHeader->SourcePort			= ServicePoint->SourceAddress.Port;

	lpxHeader->AckSequence			= HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));
//	lpxHeader->SourceId				= smpContext->SourceId;
//	lpxHeader->DestinationId			= smpContext->DestionationId;
//	lpxHeader->AllocateSequence		= HTONS(smpContext->Allocate);
	lpxHeader->ServerTag			= smpContext->ServerTag;

	switch(PacketType) {

	case CONREQ:
	case DATA:
	case DISCON:
		sequence = (USHORT)InterlockedIncrement(&smpContext->Sequence) ;
		sequence -- ;

		if(PacketType == CONREQ) {
			lpxHeader->Lsctl = HTONS(LSCTL_CONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DATA) {
			lpxHeader->Lsctl = HTONS(LSCTL_DATA | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DISCON) {
			finsequence = (USHORT)InterlockedIncrement(&smpContext->FinSequence) ;
			finsequence -- ;

			lpxHeader->Lsctl = HTONS(LSCTL_DISCONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(finsequence);
		}

		DebugPrint(2, ("CONREQ DATA DISCON : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		if(!SmpSendTest(smpContext, Packet)) {
			DebugPrint(0, ("[LPX] TransmitPacket: insert WriteQueue!!!!\n"));
			ExInterlockedInsertTailList(
									&smpContext->WriteQueue,
									&RESERVED(Packet)->ListElement,
									&smpContext->WriteQSpinLock
									);
			return STATUS_SUCCESS;
		}
		
		//
		//	added by hootch 09072003
		//	give the packet to SmpWorkDPC routine
		//
		//
		// NOTE:The packet type is restricted to DATA for now.
		//
		if(PacketType == DATA) {
			RoutePacketRequest(smpContext, Packet, PacketType);
			return STATUS_SUCCESS ;
		}

		break;

	case ACK :

		DebugPrint(2, ("ACK : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(smpContext->Sequence));

		break;

	case ACKREQ :

		DebugPrint(2, ("ACKREQ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(smpContext->Sequence));

		break;

	default:

		DebugPrint(0, ("[LPX] TransmitPacket: STATUS_NOT_SUPPORTED\n"));

		return STATUS_NOT_SUPPORTED;
	}

	RoutePacket(smpContext, Packet, PacketType);

	return STATUS_SUCCESS;
}

VOID
ProcessSentPacket(
				  IN PNDIS_PACKET	Packet
				  )
{
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;
	PSMP_CONTEXT		SmpContext;
	PUCHAR				packetData;
	KIRQL				oldIrql ;

	// Added by jgahn.
	
	//
	// Calc Timeout...
	//
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	switch(NTOHS(lpxHeader->Lsctl)) {
	case LSCTL_CONNREQ | LSCTL_ACK:
	case LSCTL_DATA | LSCTL_ACK:
	case LSCTL_DISCONNREQ | LSCTL_ACK:

		SmpContext = (PSMP_CONTEXT)(RESERVED(Packet)->pSmpContext);


//	OBSOULTE: LatestSendTime
//	removed by hootch 09062003
		// Calc Timeout...
//		if(SmpContext != NULL) {
//			ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;
//			SmpContext->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CalculateRTT(SmpContext);
//
//			SmpContext->LatestSendTime.QuadPart = CurrentTime().QuadPart;
//			RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;
//		}

		break;
	default:
		// Nothing to do..
		break;
	}

	PacketFree(Packet);
}




//
//	give a request to SmpWork DPC
//
static void
RoutePacketRequest(
	IN PSMP_CONTEXT	smpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	) {
	LONG	cnt ;
	KIRQL	oldIrql;
	//
	// The packet type is restricted to DATA for now.
	//
	if(PacketType != DATA) {
		DebugPrint(0, ("[LPX] RoutePacketRequest: bad PacketType for SmpWorkDPC\n")) ;
		return ;
	}

	ExInterlockedInsertTailList(&smpContext->SendQueue,
		&RESERVED(Packet)->ListElement,
		&smpContext->SendQSpinLock
		);

	cnt = InterlockedIncrement(&smpContext->RequestCnt) ;
	if( cnt == 1 ) {
		ACQUIRE_SPIN_LOCK(&smpContext->smpDpcSpinLock,&oldIrql);	// smpDpcSpinlock + : 1
		KeInsertQueueDpc(&smpContext->SmpWorkDpc, NULL, NULL) ;
		RELEASE_SPIN_LOCK(&smpContext->smpDpcSpinLock,oldIrql);		// smpDpcSpinlock - : 0
	}

}



static NTSTATUS
RoutePacket(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	)
{
	PSERVICE_POINT	servicePoint = SmpContext->ServicePoint;
	PNDIS_PACKET	packet2 = NULL;
	NTSTATUS		status = STATUS_PENDING;
	PDEVICE_CONTEXT	deviceContext = NULL;
	KIRQL			oldIrql ;
	PTP_ADDRESS		address = NULL;
	ULONG			result = 0;
	PNDIS_PACKET	t_packet;
	PLPX_RESERVED	reserved;
	PLIST_ENTRY p;
	
	DebugPrint(3, ("RoutePacket\n"));

	switch(PacketType) {

	case CONREQ :
	case DATA :
	case DISCON :

		RESERVED(Packet)->pSmpContext = SmpContext;

		packet2 = PacketClone(Packet);


		ExInterlockedInsertTailList(
								&SmpContext->RetransmitQueue,
								&RESERVED(packet2)->ListElement,
								&SmpContext->RetransmitQSpinLock
								);

		ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;
		SmpContext->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CalculateRTT(SmpContext);
		// Update End retransmit time
		SmpContext->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)MAX_RETRANSMIT_TIME);
		RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;

	case RETRAN:

	case ACK:
	case ACKREQ:
	default:

		t_packet = Packet;
		address = servicePoint->Address ;
		if(!address) {
			PacketFree(t_packet);
			return STATUS_NOT_SUPPORTED;
		}

		deviceContext = address->PacketProvider;
		

		ACQUIRE_SPIN_LOCK(&deviceContext->SpinLock, &oldIrql);
        if (deviceContext->NdisSendsInProgress > 0 ) {
            InsertTailList (&deviceContext->NdisSendQueue, &(RESERVED(t_packet)->SendElement));
            ++deviceContext->NdisSendsInProgress;
            RELEASE_SPIN_LOCK (&deviceContext->SpinLock, oldIrql);
            return STATUS_PENDING;

        }
				
		deviceContext->NdisSendsInProgress = 1;
		RELEASE_SPIN_LOCK (&deviceContext->SpinLock, oldIrql);

		while(1){
			

			if(deviceContext->NdisBindingHandle) {

				NdisSend(
					&status,
					deviceContext->NdisBindingHandle,
					t_packet
					);

			} else
				status = STATUS_INVALID_DEVICE_STATE;

			result = ExInterlockedAddUlong(
                         &deviceContext->NdisSendsInProgress,
                         (ULONG)-1,
                         &deviceContext->SpinLock);


			switch(status) {
			case NDIS_STATUS_SUCCESS:
				DebugPrint(1, ("[LPX]RoutePacket: NdisSend return Success\n"));
				ProcessSentPacket(t_packet);
				InterlockedIncrement( &NumberOfSentComplete );

			case NDIS_STATUS_PENDING:
				InterlockedIncrement(&NumberOfSent);
				break;

			default:
				DebugPrint(1, ("[LPX]RoutePacket: Error when NdisSend. status: 0x%x\n", status));
				PacketFree(t_packet);
			}

			if(result == 1) return status;
			
			ACQUIRE_SPIN_LOCK(&deviceContext->SpinLock, &oldIrql);
			p = RemoveHeadList(&deviceContext->NdisSendQueue);
			reserved = CONTAINING_RECORD(p, LPX_RESERVED, ListElement);
			t_packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);	
			RELEASE_SPIN_LOCK (&deviceContext->SpinLock, oldIrql);
		}
	}

	return status;
}

VOID
LpxSendComplete(
				IN NDIS_HANDLE	ProtocolBindingContext,
				IN PNDIS_PACKET	Packet,
				IN NDIS_STATUS	Status
				)
{
	PDEVICE_CONTEXT	deviceContext;
	DebugPrint(3, ("LpxSendComplete\n"));
	deviceContext = RESERVED(Packet)->PacketProvider;
	ProcessSentPacket(Packet);
	InterlockedIncrement( &NumberOfSentComplete );

	return;
}

static BOOLEAN
SmpSendTest(
			PSMP_CONTEXT	SmpContext,
			PNDIS_PACKET	Packet
			)
{
	LONG				inFlight;
	PCHAR				packetData;
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;	
	KIRQL				oldIrql ;

	NdisQueryPacket(
				Packet,
				NULL,
				NULL,
				&firstBuffer,
				NULL
				);
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	inFlight = NTOHS(SHORT_SEQNUM(lpxHeader->Sequence)) - SHORT_SEQNUM(SmpContext->RemoteAck);

	DebugPrint(4, ("lpxHeader->Sequence = %x, SmpContext->RemoteAck = %x\n",
		NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(SmpContext->RemoteAck)));
	if(inFlight >= SmpContext->MaxFlights) {
		SmpPrintState(1, "Flight overflow", SmpContext->ServicePoint);
		return 0;
	}

	return 1;	
}


static LONGLONG
CalculateRTT(
	IN	PSMP_CONTEXT	SmpContext
	)
{
	/*
	if(SmpContext->Retransmits == 0) {
		return SmpContext->IntervalTime.QuadPart * 10;
	} else if(SmpContext->Retransmits < 10) {
		return SmpContext->IntervalTime.QuadPart * SmpContext->Retransmits * 1000;
	} else
		return MAX_RETRANSMIT_DELAY;
		*/
	LARGE_INTEGER	Rtime;
	int				i;

	Rtime.QuadPart = RETRANSMIT_TIME; //(SmpContext->IntervalTime.QuadPart);

	// Exponential
	for(i = 0; i < SmpContext->Retransmits; i++) {
		Rtime.QuadPart *= 2;
		
		if(Rtime.QuadPart > MAX_RETRANSMIT_DELAY)
			return MAX_RETRANSMIT_DELAY;
	}

	if(Rtime.QuadPart > MAX_RETRANSMIT_DELAY)
		Rtime.QuadPart = MAX_RETRANSMIT_DELAY;

	return Rtime.QuadPart;
}

//
//
//
//	called only from SmpDpcWork()
//
static NTSTATUS
SmpReadPacket(
	IN 	PIRP			Irp,
	IN 	PSERVICE_POINT	ServicePoint
	)
{
	PNDIS_PACKET		packet;
	PSMP_CONTEXT		smpContext = &ServicePoint->SmpContext;
	PIO_STACK_LOCATION	irpSp;
	ULONG				userDataLength;
	ULONG				irpCopied;
	ULONG				remained;
	PUCHAR				userData;
	PLPX_HEADER2		lpxHeader;

	PNDIS_BUFFER		firstBuffer;
	PUCHAR				bufferData;
	UINT				bufferLength;
	UINT				copied;
	KIRQL				cancelIrql;
	KIRQL				oldIrql ;

	DebugPrint(4, ("SmpReadPacket\n"));

	do {
		// Mod by jgahn. See line 2206...
		//
		//packet = PacketDequeue(&smpContext->RcvDataQueue, &ServicePoint->ServicePointQSpinLock);
		// New
		packet = PacketPeek(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock);

		if(packet == NULL) {
			DebugPrint(2, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p Returned\n", ServicePoint, Irp));
			ExInterlockedInsertHeadList(
									&ServicePoint->ReceiveIrpList,
									&Irp->Tail.Overlay.ListEntry,
									&ServicePoint->ReceiveIrpQSpinLock
									);
			return STATUS_PENDING;
		}

		lpxHeader = (PLPX_HEADER2)RESERVED(packet)->LpxSmpHeader;
		if(NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
			ServicePoint->Shutdown |= SMP_RECEIVE_SHUTDOWN;	

			packet = PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock);
			PacketFree(packet);
			break;
		}

		irpSp = IoGetCurrentIrpStackLocation(Irp);
		userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 
		irpCopied = Irp->IoStatus.Information;
		remained = userDataLength - irpCopied;

		userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);
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

		Irp->IoStatus.Information += copied;
		if(RESERVED(packet)->PacketDataOffset == NdisBufferLength(firstBuffer)) {
			// Add by jgahn.
			packet = PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock);
			PacketFree(packet);
		}

		DebugPrint(4, ("userDataLength = %d, copied = %d, Irp->IoStatus.Information = %d\n",
				userDataLength, copied, Irp->IoStatus.Information));

	} while(userDataLength > Irp->IoStatus.Information);

	DebugPrint(4, ("SmpReadPacket IRP\n"));

	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DebugPrint(2, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p completed.\n", ServicePoint, Irp));

	//
	//	check to see if shutdown is in progress.
	//
	//	added by hootch 09042003
	//

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;	//servicePoint + : 1

	if(ServicePoint->Shutdown & SMP_RECEIVE_SHUTDOWN) {
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0

		DebugPrint(1, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p canceled\n", ServicePoint, Irp));

		LpxCallUserReceiveHandler(ServicePoint);
		SmpCancelIrps(ServicePoint);
	}
	else {
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0
	}

	return STATUS_SUCCESS;
}

int iTicks;



//
//	request the Smp DPC routine for the packet received
//
//
//	added by hootch 09092003
VOID
SmpDoReceiveRequest(
	IN PSMP_CONTEXT	smpContext,
	IN PNDIS_PACKET	Packet
	) {
	LONG	cnt ;
	KIRQL	oldIrql;

	ExInterlockedInsertTailList(&smpContext->ReceiveQueue,
		&RESERVED(Packet)->ListElement,
		&smpContext->ReceiveQSpinLock
		);
	cnt = InterlockedIncrement(&smpContext->RequestCnt) ;
	if( cnt == 1 ) {
		ACQUIRE_SPIN_LOCK(&smpContext->smpDpcSpinLock, &oldIrql);		//	smpDpcSpinlock + : 1
		KeInsertQueueDpc(&smpContext->SmpWorkDpc, NULL, NULL) ;
		RELEASE_SPIN_LOCK(&smpContext->smpDpcSpinLock, oldIrql);		//	smpDpcSpinlock - : 0
	}

}

//
//
//	called only from LpxReceiveComplete
//
static BOOLEAN
SmpDoReceive(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet
	)
{
	PLPX_HEADER2		lpxHeader;
	NTSTATUS			status;
	KIRQL				oldIrql;
	PSERVICE_POINT		servicePoint;
	PLIST_ENTRY			irpListEntry;
	PIRP				irp;
	PNDIS_BUFFER		firstBuffer;	
	UCHAR				dataArrived = 0;
	UCHAR				packetConsumed = 0;
	KIRQL				cancelIrql;
#if DBG
	LARGE_INTEGER		ticks;
#endif

	DebugPrint(3, ("SmpDoReceive\n"));
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	servicePoint = SmpContext->ServicePoint;

	//
	// DROP PACKET for DEBUGGING!!!!
	//
#if DBG
	/*
	KeQueryTickCount(&ticks);
	
	if(ulPacketDropRate > (ULONG)(ticks.u.LowPart % 100)) {
		
		DebugPrint(2, ("[LPX]DROP PACKET FOR DEBUGGING!!!!!\n"));
		
		PacketFree(Packet);
		
		return;
	}

	if(ulPacketDropRate > (ULONG)(++iTicks % 100)) {
		
		DebugPrint(2, ("[LPX]DROP PACKET FOR DEBUGGING!!!!!\n"));
		
		PacketFree(Packet);

		return FALSE ;
	}
	*/	
#endif

	/*
	//	Version 1
	if((NTOHS(lpxHeader->Sequence) != SmpContext->RemoteSequence)
		&& !(NTOHS(lpxHeader->Lsctl) == LSCTL_ACK && ((SHORT)(NTOHS(lpxHeader->Sequence) - SmpContext->RemoteSequence) > 0))
		&& !(NTOHS(lpxHeader->Lsctl) & LSCTL_DATA)) {
		DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet\n"));
		PacketFree(Packet);
		return;
	}
*/

	/*
	// Version 2
	if(((SHORT)(NTOHS(lpxHeader->AckSequence) - SmpContext->Sequence) < 0) &&
		((SHORT)(NTOHS(lpxHeader->Sequence) - SmpContext->RemoteSequence) < 0))
	{
		DebugPrint(1, 
			("[LPX]SmpDoReceive: Already Received packet... Drop. HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x\n", 
			NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence));
		PacketFree(Packet);
		return;
	}

	if(NTOHS(lpxHeader->Sequence) != SmpContext->RemoteSequence) {
		
		if((NTOHS(lpxHeader->AckSequence) >= SmpContext->Sequence)
			&& (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)) {
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. But has Ack process ACK\n"));
			
		} else {
			DebugPrint(1, 
				("[LPX]SmpDoReceive: Bad ACK Number Packet Loss... Drop. HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x\n", 
				NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence
				));
			
			PacketFree(Packet);
			return;
		}
	}
	*/

	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;		//servicePoint + : 1

	// Version 3
	//
	//	caution: use SHORT_SEQNUM() to compare lpxHeader->SequneceXXX
	//					with LONG type sequence number such as RemoteSequnce and Sequence
	//
	if(NTOHS(lpxHeader->Sequence) != SHORT_SEQNUM(SmpContext->RemoteSequence)) {
		
		//
		// Transmit ACK
		//
		TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

		if(( NTOHS(lpxHeader->Sequence) > SHORT_SEQNUM(SmpContext->RemoteSequence) + SMP_MAX_FLIGHT / 2 )
			|| ( NTOHS(lpxHeader->Sequence) < (SHORT_SEQNUM(SmpContext->RemoteSequence) - SMP_MAX_FLIGHT / 2) )) {
			DebugPrint(2, 
				("[LPX]SmpDoReceive: Bad Seq Number. !!! Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence,
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));
		} else {
			DebugPrint(2, 
				("[LPX]SmpDoReceive: Seq# Mismatch. Maybe RT Packet. Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence,
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));
		}
//
//		 Has Ack?
//		if((servicePoint->SmpState == SMP_ESTABLISHED)
//			&& (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)
//			&& (NTOHS(lpxHeader->AckSequence) > SmpContext->RemoteAck)) {
//
//			if((NTOHS(lpxHeader->AckSequence) <= SmpContext->Sequence)) {
//
//				DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. But has advanced Ack. Process ACK\n"));
//				
//				SmpContext->RemoteAck = NTOHS(lpxHeader->AckSequence);
//				SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);	
//			} else {
//				DebugPrint(1, ("[LPX]SmpDoReceive: BAD ACK!!!!!!!!!!!!!!! S : 0x%x, HA :0x%x\n",
//					SmpContext->Sequence, NTOHS(lpxHeader->AckSequence)));
//			}
//		}

		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;		//servicePoint - : 0

		PacketFree(Packet);

		return FALSE ;
	}


	// Receive Ack for Unsent packets.
	if(SHORT_SEQNUM(SmpContext->Sequence) >= SHORT_SEQNUM(SmpContext->RemoteAck)) {
		if((NTOHS(lpxHeader->AckSequence) > SHORT_SEQNUM(SmpContext->Sequence))
			|| (NTOHS(lpxHeader->AckSequence) < SHORT_SEQNUM(SmpContext->RemoteAck))) {
			DebugPrint(1, 
				("[LPX]SmpDoReceive: Bad ACK Number. Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x, RA: 0x%x, LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), 
				SHORT_SEQNUM(SmpContext->Sequence), SHORT_SEQNUM(SmpContext->RemoteSequence),
				SHORT_SEQNUM(SmpContext->RemoteAck),
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));

			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;		//servicePoint - : 0

			PacketFree(Packet);


			return FALSE ;
		}
	} else {
		// Wrapping...
		if(((NTOHS(lpxHeader->AckSequence) > SHORT_SEQNUM(SmpContext->Sequence)) &&
			(NTOHS(lpxHeader->AckSequence) < SHORT_SEQNUM(SmpContext->RemoteAck)))
			) {
			DebugPrint(1, 
				("[LPX]SmpDoReceive: Bad ACK Number. Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x, RA: 0x%x, LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), 
				SHORT_SEQNUM(SmpContext->Sequence), SHORT_SEQNUM(SmpContext->RemoteSequence),
				SHORT_SEQNUM(SmpContext->RemoteAck),
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));

			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;		//servicePoint - : 0

			PacketFree(Packet);

			return FALSE ;
		}
	}

	DebugPrint(4, ("SmpDoReceive NTOHS(lpxHeader->Sequence) = 0x%x, lpxHeader->Lsctl = 0x%x\n",
		NTOHS(lpxHeader->Sequence), lpxHeader->Lsctl));

	// Since receiving Vaild Packet, Reset AliveTimeOut.
	InterlockedExchange(&SmpContext->AliveRetries, 0) ;

	ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;		//TimeCountSpinlock + : 1
	SmpContext->AliveTimeOut.QuadPart = CurrentTime().QuadPart + ALIVE_INTERVAL;
	RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;		//TimeCountSpinlock - : 0

	switch(servicePoint->SmpState) {

	case SMP_CLOSE:
		break;

	case SMP_LISTEN:

		switch(NTOHS(lpxHeader->Lsctl)) 
		{

		case LSCTL_CONNREQ:
		case LSCTL_CONNREQ | LSCTL_ACK:
		
			DebugPrint(1, ("LpxDoReceive SMP_LISTEN CONREQ\n"));

			if(servicePoint->ListenIrp == NULL) {
				break;
			}

			InterlockedIncrement(&SmpContext->RemoteSequence);
	
			RtlCopyMemory(
				servicePoint->DestinationAddress.Node,
				RESERVED(Packet)->EthernetHeader.SourceAddress,
				ETHERNET_ADDRESS_LENGTH
				);
			servicePoint->DestinationAddress.Port = lpxHeader->SourcePort;
			servicePoint->SmpState = SMP_SYN_RECV;


			//
			// BUG BUG BUG!!! Assign Server Tag. But mot implemented, So ServerTag is 0.
			// 
			// servicePoint->SmpContext.ServerTag = ;

			TransmitPacket(servicePoint, NULL, CONREQ, 0);

			ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;	//timeCountSpinlock + : 1
			servicePoint->SmpContext.ConnectTimeOut.QuadPart = CurrentTime().QuadPart + MAX_CONNECT_TIME;
			servicePoint->SmpContext.SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
			RELEASE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;	//timeCountSpinlock - : 0
			
			KeSetTimer(
				&servicePoint->SmpContext.SmpTimer,
				servicePoint->SmpContext.SmpTimerExpire,
				&servicePoint->SmpContext.SmpTimerDpc
				);

			break;
		
		default:
		
			break;
		}

		break;
		
	case SMP_SYN_SENT:

		switch(NTOHS(lpxHeader->Lsctl)) 
		{

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);
			servicePoint->SmpState = SMP_SYN_SENT;

			break;

		case LSCTL_CONNREQ:
		case LSCTL_CONNREQ | LSCTL_ACK:

			DebugPrint(1, ("LpxDoReceive SMP_SYN_SENT CONREQ\n"));
			SmpPrintState(1, "LSCTL_CONNREQ", servicePoint);

			InterlockedIncrement(&SmpContext->RemoteSequence);

			SmpContext->ServerTag = lpxHeader->ServerTag;

			servicePoint->SmpState = SMP_SYN_RECV;
DebugPrint(1,("IN SmpDoReceive SMP_SYN_SENT 1\n "));
			if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) 
			{
				InterlockedExchange(&SmpContext->RemoteAck, NTOHS(lpxHeader->AckSequence));
				SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

				servicePoint->SmpState = SMP_ESTABLISHED;

				if(servicePoint->ConnectIrp) {
					PIRP	irp;

					irp = servicePoint->ConnectIrp;
					servicePoint->ConnectIrp = NULL;
					irp->IoStatus.Status = STATUS_SUCCESS;
DebugPrint(1,("IN SmpDoReceive SMP_SYN_SENT 2\n "));
					IoAcquireCancelSpinLock(&cancelIrql);
DebugPrint(1,("IN SmpDoReceive SMP_SYN_SENT 3\n" ));
					IoSetCancelRoutine(irp, NULL);
DebugPrint(1,("IN SmpDoReceive SMP_SYN_SENT 4\n "));
					IoReleaseCancelSpinLock(cancelIrql);

					IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
				}
			} 

			TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);						
		
			break;
		
		default:

			break;
		}

		break;

	case SMP_SYN_RECV:

		DebugPrint(1, ("LpxDoReceive SMP_SYN_RECV CONREQ\n"));

		if(!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK))
			break;
		
		if(NTOHS(lpxHeader->AckSequence) < 1)
			break;

		servicePoint->SmpState = SMP_ESTABLISHED;

		if(servicePoint->ConnectIrp) 
		{
			PIRP	irp;

			irp = servicePoint->ConnectIrp;
			servicePoint->ConnectIrp = NULL;

			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(irp, NULL);
			IoReleaseCancelSpinLock(cancelIrql);

			irp->IoStatus.Status = STATUS_SUCCESS;
			IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		} else if(servicePoint->ListenIrp) 
		{
			PIRP						irp;
			PIO_STACK_LOCATION			irpSp;
			PTDI_REQUEST_KERNEL_LISTEN	request;
			PTDI_CONNECTION_INFORMATION	connectionInfo;
			PTRANSPORT_ADDRESS			transportAddress;
		    PTA_ADDRESS					taAddress;
			PTDI_ADDRESS_LPX			lpxAddress;


			irp = servicePoint->ListenIrp;
			servicePoint->ListenIrp = NULL;

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
					UCHAR				addressBuffer[FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
														+ FIELD_OFFSET(TA_ADDRESS, Address)
														+ TDI_ADDRESS_LENGTH_LPX];
					PTRANSPORT_ADDRESS	transportAddress;
					ULONG				returnLength;
					PTA_ADDRESS			taAddress;
					PTDI_ADDRESS_LPX	lpxAddress;


					DebugPrint(2, ("connectionInfo->RemoteAddressLength = %d, addressLength = %d\n", 
							connectionInfo->RemoteAddressLength, (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
																+ FIELD_OFFSET(TA_ADDRESS, Address)
																+ TDI_ADDRESS_LENGTH_LPX)));
													
					transportAddress = (PTRANSPORT_ADDRESS)addressBuffer;

					transportAddress->TAAddressCount = 1;
					taAddress = (PTA_ADDRESS)transportAddress->Address;
					taAddress->AddressType		= TDI_ADDRESS_TYPE_LPX;
					taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

					lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

					RtlCopyMemory(
							lpxAddress,
							&servicePoint->DestinationAddress,
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
			IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		}

		goto established;

		break;

	case SMP_ESTABLISHED:

established:

		// 
		// Check Server Tag.
		//
		if(lpxHeader->ServerTag != SmpContext->ServerTag) {
			DebugPrint(1, ("[LPX] SmpDoReceice/SMP_ESTABLISHED: Bad Server Tag Drop. SmpContext->ServerTag 0x%x, lpxHeader->ServerTag 0x%x\n", SmpContext->ServerTag, lpxHeader->ServerTag));
			break;
		}

		if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);
		}
		if(SmpContext->Retransmits) {
			SmpPrintState(1, "remained", servicePoint);
		}

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_ACKREQ:
		case LSCTL_ACKREQ | LSCTL_ACK:

			TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

			break;

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:
			
			if(NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) <= sizeof(LPX_HEADER2)) {
				DebugPrint(1, ("[LPX] SmpDoReceice/SMP_ESTABLISHED: Data packet without Data!!!!!!!!!!!!!! SP: 0x%x\n", servicePoint));
			}

			if(NTOHS(lpxHeader->Sequence) > SHORT_SEQNUM(SmpContext->RemoteSequence)) {
				char	buffer[256];

				TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceice/SMP_ESTABLISHED: remote packet losed: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			if(NTOHS(lpxHeader->Sequence) < SHORT_SEQNUM(SmpContext->RemoteSequence)) {
				char	buffer[256];

				TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);
				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				DebugPrint(1, ("Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			InterlockedIncrement(&SmpContext->RemoteSequence);

			//if(NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {
			ExInterlockedInsertTailList(&SmpContext->RcvDataQueue,
				&RESERVED(Packet)->ListElement,
				&SmpContext->RcvDataQSpinLock
				);
			
			packetConsumed ++;
			dataArrived = 1;  // Data
			//}

			//
			//BUG BUG BUG!!!!!!
			//
			TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

			/*
			if((SmpContext->RemoteSequence % 100) != 0) 
				TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);						
			else
				DebugPrint(1, ("Can't send ACK when RemoteSeq is 0x%x\n", SmpContext->RemoteSequence));
*/

			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			if(NTOHS(lpxHeader->Sequence) > SHORT_SEQNUM(SmpContext->RemoteSequence)) {
				char	buffer[256];

				TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);
				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceice/SMP_ESTABLISHED: remote packet losed: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			if(NTOHS(lpxHeader->Sequence) < SHORT_SEQNUM(SmpContext->RemoteSequence)) {
				char	buffer[256];

				TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);
				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceice/SMP_ESTABLISHED: Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			InterlockedIncrement(&SmpContext->RemoteSequence);

			//if(NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {
			ExInterlockedInsertTailList(&SmpContext->RcvDataQueue,
				&RESERVED(Packet)->ListElement,
				&SmpContext->RcvDataQSpinLock
				);
			
			packetConsumed ++;
			dataArrived = 1;  // Data
			//}

			//
			//BUG BUG BUG!!!!!!
			//
			TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

			/*
			if((SmpContext->RemoteSequence % 100) != 0) 
				TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);						
			else
				DebugPrint(1, ("Can't send ACK when RemoteSeq is 0x%x\n", SmpContext->RemoteSequence));
*/

//			SmpContext->TimerReason |= SMP_DELAYED_ACK;

//			if(NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
				DebugPrint(10, ("[LPX] SmpDoReceice/SMP_ESTABLISHED: SmpDoReceive to SMP_CLOSE_WAIT\n"));
				servicePoint->SmpState = SMP_CLOSE_WAIT;
				
				CallUserDisconnectHandler(servicePoint, TDI_DISCONNECT_RELEASE);
				/*
				(*servicePoint->Connection->AddressFile->DisconnectHandler)(
					servicePoint->Connection->AddressFile->DisconnectHandlerContext,
					servicePoint->Connection->Context,
					0,
					NULL,
					0,
					NULL,
					TDI_DISCONNECT_RELEASE
					);
					*/
//			}

			break;


		default:

			break;
		}

		break;

	case SMP_LAST_ACK:

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck,(LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

			if(SHORT_SEQNUM(SmpContext->RemoteAck) == SHORT_SEQNUM(SmpContext->FinSequence)) {
				KIRQL	oldIrqlTimeCnt ;
//				SmpFreeServicePoint(servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceive: entering SMP_TIME_WAIT due to RemoteAck == FinSequence\n")) ;

				servicePoint->SmpState = SMP_TIME_WAIT;
				ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrqlTimeCnt) ;	//timeCountSpinlock + : 1
				SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;
				RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrqlTimeCnt) ;	//timeCountSpinlock - : 0
				break;
			}

			DebugPrint(1, ("SmpDoReceive SMP_LAST_ACK\n"));

			break;

		default:

			break;
		}

		break;

	case SMP_FIN_WAIT1:

		DebugPrint(1, ("SmpDoReceive SMP_FIN_WAIT1 lpxHeader->Lsctl = %d\n", NTOHS(lpxHeader->Lsctl)));

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);

			if(!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK))
				break;

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

			if(SHORT_SEQNUM(SmpContext->RemoteAck) == SHORT_SEQNUM(SmpContext->FinSequence)) {
				DebugPrint(1, ("[LPX] SmpDoReceice/SMP_FIN_WAIT1: SMP_FIN_WAIT1 to SMP_FIN_WAIT2\n"));
				servicePoint->SmpState = SMP_FIN_WAIT2;
			}
			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);
			servicePoint->SmpState = SMP_CLOSING;

			if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
				InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
				SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

				if(SHORT_SEQNUM(SmpContext->RemoteAck) == SHORT_SEQNUM(SmpContext->FinSequence)) {
					DebugPrint(1, ("[LPX] SmpDoReceice/SMP_FIN_WAIT1: entering SMP_TIME_WAIT due to RemoteAck == FinSequence\n")) ;
					servicePoint->SmpState = SMP_TIME_WAIT;
					SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;
				}
			}
			TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

			break;

		default:
			
			break;
		}

		break;

	case SMP_FIN_WAIT2:

		DebugPrint(4, ("SmpDoReceive SMP_FIN_WAIT2  lpxHeader->Lsctl = 0x%x\n", NTOHS(lpxHeader->Lsctl)));

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);

			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT2: entering SMP_TIME_WAIT\n"));
			servicePoint->SmpState = SMP_TIME_WAIT;
			SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;

			TransmitPacket(SmpContext->ServicePoint, NULL, ACK, 0);

			break;

		default:
			
			break;
		}

		break;

	case SMP_CLOSING:

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

			if(SHORT_SEQNUM(SmpContext->RemoteAck) != SHORT_SEQNUM(SmpContext->FinSequence))
				break;

			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_CLOSING: entering SMP_TIME_WAIT\n"));
			servicePoint->SmpState = SMP_TIME_WAIT;
			SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;

			break;

		default:

			break;
		}

	default:
			
		break;
	}

	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;	// servicePoint - : 0

	if(!packetConsumed)
		PacketFree(Packet);

	if(dataArrived)
		return TRUE ;

    return FALSE ;
}			

//
//	acquire ServicePoint->SpinLock before calling
//
//	called only from SmpDoReceive()
static INT
SmpRetransmitCheck(
				   IN PSMP_CONTEXT	SmpContext,
				   IN LONG			AckSequence,
				   IN PACKET_TYPE	PacketType
				   )
{
	PNDIS_PACKET		packet;
	PLIST_ENTRY			packetListEntry;
	PLPX_HEADER2		lpxHeader;
	PSERVICE_POINT		servicePoint;
	PUCHAR				packetData;
	PNDIS_BUFFER		firstBuffer;	
	PLPX_RESERVED		reserved;
	
	servicePoint = SmpContext->ServicePoint;

	DebugPrint(3, ("[LPX]SmpRetransmitCheck: Entered.\n"));

	packet = PacketPeek(&SmpContext->RetransmitQueue, &SmpContext->RetransmitQSpinLock);
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
						
	while((packet = PacketPeek(&SmpContext->RetransmitQueue, &SmpContext->RetransmitQSpinLock)) != NULL)
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
			DebugPrint(2, ("[LPX] SmpRetransmitCheck: deleted a packet to be  retransmitted.\n")) ;
			packet = PacketDequeue(&SmpContext->RetransmitQueue, &SmpContext->RetransmitQSpinLock);
			if(packet) PacketFree(packet);
		} else
			break;
	}

//
//	OBSOULTE: We don't update RetransmitTimeOut here for now.
//	removed by hootch 09062003
//
//	if(!PacketQueueEmpty(&SmpContext->RetransmitQueue, &servicePoint->ServicePointQSpinLock))
//	{
//		SmpContext->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CaculateRTT(SmpContext);
//	}

//
//	OBSOULTE: LatestSendTime, IntervalTime
//	removed by hootch 09062003
//
//	else {
//
//
//		if(SmpContext->LatestSendTime.QuadPart != 0) {
//			SmpContext->IntervalTime.QuadPart = ((SmpContext->IntervalTime.QuadPart * 99)
//											+ ((CurrentTime().QuadPart - SmpContext->LatestSendTime.QuadPart) * 1));
//			if(SmpContext->IntervalTime.QuadPart > 100)
//				SmpContext->IntervalTime.QuadPart /= 100;
//			else
//				SmpContext->IntervalTime.QuadPart = 1;
//
//			SmpContext->LatestSendTime.QuadPart = 0;
//		}else
//			SmpPrintState(1, "LatestSendTime = 0", servicePoint);			
//	}


	if(SmpContext->Retransmits) {
		KIRQL	oldIrql ;

		InterlockedExchange(&SmpContext->Retransmits, 0);

		ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;		//timeCounterSpinlock + : 1
		SmpContext->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)MAX_RETRANSMIT_TIME);
		RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;		//timeCounterSpinlock - : 0

		SmpContext->TimerReason |= SMP_RETRANSMIT_ACKED;
	} 
	else if(!(SmpContext->TimerReason & SMP_RETRANSMIT_ACKED)
		&& !(SmpContext->TimerReason & SMP_SENDIBLE)
		&& ((packet = PacketPeek(&SmpContext->WriteQueue, &SmpContext->WriteQSpinLock)) != NULL)
		&& SmpSendTest(SmpContext, packet))
	{
		SmpContext->TimerReason |= SMP_SENDIBLE;
	}

	return 0;
}

static VOID
SmpWorkDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   ) {
	PSMP_CONTEXT		smpContext = (PSMP_CONTEXT)Context;
	PSERVICE_POINT		servicePoint = smpContext->ServicePoint ;
	LONG				cnt ;
	PNDIS_PACKET		packet;
	PLPX_RESERVED		reserved;
	PLIST_ENTRY			listEntry;
	PIRP				irp ;
	NTSTATUS			status ;

do_more:
	//
	//	Send
	//
	while(listEntry = ExInterlockedRemoveHeadList(&smpContext->SendQueue, &smpContext->SendQSpinLock)) {
		reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		if(KeGetCurrentIrql() > DISPATCH_LEVEL){
			DebugPrint(10,("KeGetCurrentIrql() > DISPATCH_LEVEL\n"));
			PacketFree(packet);
			continue;
		}
		RoutePacket(smpContext, packet, DATA) ;
	}

	//
	//	Receive completion
	//
	while(listEntry = ExInterlockedRemoveHeadList(&smpContext->ReceiveQueue, &smpContext->ReceiveQSpinLock)) {
		reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		SmpDoReceive(smpContext, packet) ;
	}

	//
	//	Receive IRP completion
	//
	while(listEntry
		= ExInterlockedRemoveHeadList(&servicePoint->ReceiveIrpList, &servicePoint->ReceiveIrpQSpinLock))
	{
		irp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);
		status = SmpReadPacket(irp, servicePoint);
		if(status != STATUS_SUCCESS)
			break;
		else
			LpxCallUserReceiveHandler(servicePoint);
	}

	//
	//	Timer expiration
	//
	SmpTimerDpcRoutine(dpc, smpContext, junk1, junk2) ;

	cnt = InterlockedDecrement(&smpContext->RequestCnt) ;
	if(cnt) {
		//
		// Reset the counter to one.
		// Do work more then.
		//
		InterlockedExchange(&smpContext->RequestCnt, 1) ;
		goto do_more ;
	}

}

//
//	request the Smp DPC routine for the time-expire
//
//
//	added by hootch 09092003
static VOID
SmpTimerDpcRoutineRequest(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   ) {
	PSMP_CONTEXT		smpContext = (PSMP_CONTEXT)Context;
	LONG	cnt ;
	KIRQL	oldIrql;

	cnt = InterlockedIncrement(&smpContext->RequestCnt) ;
	if( cnt == 1 ) {
		ACQUIRE_SPIN_LOCK(&smpContext->smpDpcSpinLock, &oldIrql);		//smpDpcSpinlock + : 1
		KeInsertQueueDpc(&smpContext->SmpWorkDpc, NULL, NULL) ;
		RELEASE_SPIN_LOCK(&smpContext->smpDpcSpinLock, oldIrql);		//smpDpcSpinlock - : 0
	}
}

static VOID
SmpTimerDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   )
{
	PSMP_CONTEXT		smpContext = (PSMP_CONTEXT)Context;
	PSERVICE_POINT		servicePoint = smpContext->ServicePoint;
	PNDIS_PACKET		packet;
	PNDIS_PACKET		packet2;
	PUCHAR				packetData;
	PNDIS_BUFFER		firstBuffer;	
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;
	LIST_ENTRY			tempQueue;
	KSPIN_LOCK			tempSpinLock;
	KIRQL				cancelIrql;
	LONG				cloned ;

	DebugPrint(5, ("SmpTimerDpcRoutine ServicePoint = %x\n", servicePoint));

	KeInitializeSpinLock(&tempSpinLock) ;

	// added by hootch 08262003
	ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);		// servicePoint + : 1
	if(servicePoint->SmpState == SMP_CLOSE) {
		RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);	// servicePoint - : 0

		DebugPrint(1, ("[LPX] SmpTimerDpcRoutine: ServicePoint closed\n", servicePoint));
		return ;
	}

	//
	//	reference Connection
	//
	lpx_ReferenceConnection(servicePoint->Connection) ;		// connection ref + : 1

	KeCancelTimer(&smpContext->SmpTimer);

	//
	//	do condition check
	//
	switch(servicePoint->SmpState) {
		
	case SMP_TIME_WAIT:
		
		if(smpContext->TimeWaitTimeOut.QuadPart <= CurrentTime().QuadPart) 
		{
			DebugPrint(1, ("[LPX] SmpTimerDpcRoutine: TimeWaitTimeOut ServicePoint = %x\n", servicePoint));

			if(servicePoint->DisconnectIrp) {
				PIRP	irp;
				
				irp = servicePoint->DisconnectIrp;
				servicePoint->DisconnectIrp = NULL;
				
				IoAcquireCancelSpinLock(&cancelIrql);
				IoSetCancelRoutine(irp, NULL);
				IoReleaseCancelSpinLock(cancelIrql);
				
				irp->IoStatus.Status = STATUS_SUCCESS;
				IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
			}

			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);	//servicePoint - : 0

			SmpFreeServicePoint(servicePoint);

			lpx_DereferenceConnection(servicePoint->Connection) ;	//connection ref - : 0

			return;
		}
		
		goto out;
		
	case SMP_CLOSE: 
	case SMP_LISTEN:
		
		goto out;
		
	case SMP_SYN_SENT:
		
		if(smpContext->ConnectTimeOut.QuadPart <= CurrentTime().QuadPart) 
		{
			// added by hootch 08262003
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);		//servicePoint - : 0

			SmpFreeServicePoint(servicePoint);

			lpx_DereferenceConnection(servicePoint->Connection) ;	//connection ref - : 0

			return;
		}
		
		break;
		
	default:
		
		break;
	}

	//
	//	we need to check retransmission?
	//
	if(!PacketQueueEmpty(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock)
		&& smpContext->RetransmitTimeOut.QuadPart <= CurrentTime().QuadPart) 
	{
		DebugPrint(1, ("smp_retransmit_timeout smpContext->retransmits = %d CurrentTime().QuadPart = %lx\n", 
			smpContext->Retransmits, CurrentTime().QuadPart));

		//
		//	retransmission time-out
		//
		//if(smpContext->Retransmits > MAX_RETRANSMIT_COUNT) 
		if(smpContext->RetransmitEndTime.QuadPart < CurrentTime().QuadPart) 
		{
//#if(SMP_DEBUG)
			SmpPrintState(10, "Retransmit Time Out", servicePoint);

			CallUserDisconnectHandler(servicePoint, TDI_DISCONNECT_ABORT);
			/*
			(*servicePoint->Connection->AddressFile->DisconnectHandler)(
				servicePoint->Connection->AddressFile->DisconnectHandlerContext,
				servicePoint->Connection->Context,
				0,
				NULL,
				0,
				NULL,
				TDI_DISCONNECT_ABORT
				);
*/
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);		//servicePoint - : 0

			SmpFreeServicePoint(servicePoint);
			lpx_DereferenceConnection(servicePoint->Connection) ;	// connection ref - : 0

			return;
//#endif
		}

		//
		//	retransmit.
		//
		// Need to leave packet on the queue, aye the fear
		//
		packet = PacketPeek(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock);
		if(!packet) {
			DebugPrint(0, ("no packet - is not stable\n"));
			goto out;
		}
		
		if(RESERVED(packet)->Cloned == 1)
		{
			InterlockedIncrement(&smpContext->Retransmits);	
			smpContext->RetransmitTimeOut.QuadPart 
					= CurrentTime().QuadPart + CalculateRTT(smpContext);
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);		//servicePoint - : 0
			return;				
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
		lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));

		InterlockedIncrement(&smpContext->Retransmits);
		smpContext->RetransmitTimeOut.QuadPart 
			= CurrentTime().QuadPart + CalculateRTT(smpContext);
		InterlockedExchange(&smpContext->LastRetransmitSequence, (ULONG)NTOHS(lpxHeader->Sequence));

		SmpPrintState(2, "Ret", servicePoint);
		
//		if(!RESERVED(packet)->Cloned) {
//			packet2 = PacketCopy(packet);

//			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
//			RoutePacket(smpContext, packet2, RETRAN);
//			ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
//		} else
//			SmpPrintState(2, "Cloned", servicePoint);
//
//		alternated by hootch	09132003
//
		
		packet2 = PacketCopy(packet, &cloned);
		if(cloned == 1) {
			RoutePacket(smpContext, packet2, RETRAN);
		} else {
			SmpPrintState(1, "Cloned", servicePoint);
			PacketFree(packet2) ;
		}
		
	} 
	else if(smpContext->TimerReason & SMP_RETRANSMIT_ACKED) 
	{
		BOOLEAN	YetSent = 0;
		
		SmpPrintState(2, "RetA", servicePoint);
		if(PacketQueueEmpty(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock))
		{
			smpContext->TimerReason &= ~SMP_RETRANSMIT_ACKED;
			goto send_packet;
		}

		InitializeListHead(&tempQueue);
		
		while((packet = PacketDequeue(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock)) != NULL)
		{
			ExInterlockedInsertHeadList(&tempQueue,
				&RESERVED(packet)->ListElement,
				&tempSpinLock
				);

			if(RESERVED(packet)->Cloned == 1)
			{
				ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
				smpContext->RetransmitTimeOut.QuadPart 
						= CurrentTime().QuadPart + CalculateRTT(smpContext);
				RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
				break;				
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
			lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));

/*			
			if(!RESERVED(packet)->Cloned) {
				packet2 = PacketClone(packet);

				ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
				smpContext->RetransmitTimeOut.QuadPart 
					= CurrentTime().QuadPart + CalculateRTT(smpContext);
				RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;

//				RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
				RoutePacket(smpContext, packet2, RETRAN);
//				ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
			} else {
				YetSent = 1;
				break;
			}

//		alternated by hootch	09132003
*/

			packet2 = PacketCopy(packet, &cloned) ;
			if(cloned == 1) {
				ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;		//	timeCountlock  + : 1
				smpContext->RetransmitTimeOut.QuadPart 
					= CurrentTime().QuadPart + CalculateRTT(smpContext);
				RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;		//	timeCounterlock - : 0

				RoutePacket(smpContext, packet2, RETRAN);
			} else {
				PacketFree(packet2) ;
				YetSent = 1;
				break;
			}
		}
		
		while((packet = PacketDequeue(&tempQueue, &tempSpinLock)) != NULL)
		{
			ExInterlockedInsertHeadList(
				&smpContext->RetransmitQueue,
				&RESERVED(packet)->ListElement,
				&smpContext->RetransmitQSpinLock
				);
		}
		if(!YetSent)	
			smpContext->TimerReason &= ~SMP_RETRANSMIT_ACKED;
	} 
	else if(smpContext->TimerReason & SMP_SENDIBLE) 
	{
		SmpPrintState(2, "Send", servicePoint);
send_packet:
		while((packet = PacketDequeue(&smpContext->WriteQueue, &smpContext->WriteQSpinLock)) != NULL)
		{
			SmpPrintState(1, "RealSend", servicePoint);
			if(SmpSendTest(smpContext, packet)) 
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
				lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));
				
//				RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
				RoutePacket(smpContext, packet, DATA);
//				ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
			} else 
			{
				ExInterlockedInsertHeadList(
					&smpContext->WriteQueue,
					&RESERVED(packet)->ListElement,
					&smpContext->WriteQSpinLock
					);
				break;
			}
		}
		smpContext->TimerReason &= ~SMP_SENDIBLE;
	} 
	//	else if(smpContext->TimerReason & SMP_DELAYED_ACK)
	//	{
	//		TransmitPacket(servicePoint, NULL, ACK, 0);
	//		smpContext->TimerReason &= ~SMP_DELAYED_ACK;
	//	}
	else if (smpContext->AliveTimeOut.QuadPart <= CurrentTime().QuadPart) // alive message
	{
		LONG alive ;

		alive = InterlockedIncrement(&smpContext->AliveRetries) ;
		if(( alive % 10) == 0) {
			DebugPrint(2, ("alive_retries = %d, smp_alive CurrentTime().QuadPart = %llx\n", 
				smpContext->AliveRetries, CurrentTime().QuadPart));
		}
		if(smpContext->AliveRetries > MAX_ALIVE_COUNT) {
			SmpPrintState(1, "Alive Max", servicePoint);
			
			DebugPrint(10, ("!!!!!!!!!!! servicePoint->Connection->Address->RegisteredDisconnectHandler 0x%x\n",
				servicePoint->Connection->Address->RegisteredDisconnectHandler));

			CallUserDisconnectHandler(servicePoint, TDI_DISCONNECT_ABORT);

			// added by hootch 08262003
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);		//servicePoint - : 0

			SmpFreeServicePoint(servicePoint);

			lpx_DereferenceConnection(servicePoint->Connection) ;	//connection ref - : 0

			/*
			if(servicePoint->SmpState == SMP_ESTABLISHED) {
				
				(*servicePoint->Connection->AddressFile->DisconnectHandler)(
					servicePoint->Connection->AddressFile->DisconnectHandlerContext,
					servicePoint->Connection->Context,
					0,
					NULL,
					0,
					NULL,
					TDI_DISCONNECT_ABORT
					);
			} else {
				//DbgPrint("\n\n\n!!!!!!!!!!!!!!!!!!!!!Alive Max NOT SMP_ESTABLISHED!!!!!\n\n\n");
				SmpPrintState(1, "Alive Max NOT SMP_ESTABLISHED!!!!!", servicePoint);
			}
			// End Mod.
			*/

			return;
		}

		DebugPrint(3, ("[LPX]SmpTimerDpcRoutine: Send ACKREQ. SP : 0x%x, S: 0x%x, RS: 0x%x\n", 
			servicePoint, smpContext->Sequence, smpContext->RemoteSequence));

		smpContext->AliveTimeOut.QuadPart = CurrentTime().QuadPart + ALIVE_INTERVAL;
		
//		RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
		TransmitPacket(servicePoint, NULL, ACKREQ, 0);
//		ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

		InterlockedIncrement(&smpContext->AliveRetries);
	}

out:
	ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;		//timeCountSpinlock + : 1
	smpContext->SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
	RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;		//timeCountSpinlock - : 0

	// added by hootch 08262003
	RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);				//servicePoint - : 0

	KeSetTimer(
		&smpContext->SmpTimer,
		smpContext->SmpTimerExpire,	
		&smpContext->SmpTimerDpc
		);

	//
	//	dereference the connection
	//
	lpx_DereferenceConnection(servicePoint->Connection) ;			//connection ref -1 : 0

	return;
}
//
//
//	acquire SpinLock of connection before calling
//
void
CallUserDisconnectHandler(
				  IN	PSERVICE_POINT	pServicePoint,
				  IN	ULONG			DisconnectFlags
				  )
{
	LONG called ;
	// Check parameter.
	if(pServicePoint == NULL) {
		return;
	}

	if(pServicePoint->Connection->Address->DisconnectHandler == NULL) {
		return;
	}

	if(pServicePoint->SmpState != SMP_ESTABLISHED) {
		return;
	}

	called = InterlockedIncrement(&pServicePoint->lDisconnectHandlerCalled) ;
	if(called == 1) {
DebugPrint(10,("[LPX]DisconnectHandler: Called\n"));
		// Perform Handler.
		(*pServicePoint->Connection->Address->DisconnectHandler)(
			pServicePoint->Connection->Address->DisconnectHandlerContext,
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

	return;
}

