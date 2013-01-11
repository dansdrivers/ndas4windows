/*++

Copyright (c) 1991  Ximeta Technology Inc

Module Name:

    LpxNdis.c

Abstract:

	Implement Lpx & Ddis protocol handler ans Support function

Author:


Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

VOID
LpxProtoOpenAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  NdisStatus,
    IN NDIS_STATUS  OpenErrorStatus
    )
{
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

	DebugPrint(0, ("LpxNdis: LpxOpenAdapterCompleteNDIS Status: %s\n",
            lpx_GetNdisStatus (NdisStatus)));
 

    DeviceContext->NdisRequestStatus = NdisStatus;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}

VOID 
LpxProtoCloseAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    )
{
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

	DebugPrint(0, ("LpxNdis: LpxCloseAdapterCompleteNDIS Status: %s\n",
		lpx_GetNdisStatus (Status)));

    DeviceContext->NdisRequestStatus = Status;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}


VOID
LpxProtoResetComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    )
{
    UNREFERENCED_PARAMETER(ProtocolBindingContext);
    UNREFERENCED_PARAMETER(Status);


	DebugPrint(0, ("LpxNdis: LpxResetCompleteNDIS Status: %s\n",
		lpx_GetNdisStatus (Status)));


    return;
}


VOID
LpxProtoRequestComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_REQUEST  NdisRequest,
    IN NDIS_STATUS  Status
    )
{
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;


	DebugPrint(0, ("LpxNdis: LpxRequestComplete request: %i, NDIS Status: %s\n",
		NdisRequest->RequestType,lpx_GetNdisStatus (Status)));


    DeviceContext->NdisRequestStatus = Status;
    KeSetEvent(
        &DeviceContext->NdisRequestEvent,
        0L,
        FALSE);

    return;
}


VOID
LpxProtoStatusComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    )
{
	UNREFERENCED_PARAMETER (ProtocolBindingContext);
}



VOID
LpxProtoStatus(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  GeneralStatus,
    IN PVOID  StatusBuffer,
    IN UINT  StatusBufferSize
    )
{
    PDEVICE_CONTEXT DeviceContext;
    KIRQL oldirql;
 

    DeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

    KeRaiseIrql (DISPATCH_LEVEL, &oldirql);

    switch (GeneralStatus) {


        case NDIS_STATUS_CLOSING:

           
            DebugPrint(0, ("LpxStatusIndication: Device @ %08x Closing\n", DeviceContext));
          
            //
            // The adapter is shutting down. We queue a worker
            // thread to handle this.
            //

            ExInitializeWorkItem(
                &DeviceContext->StatusClosingQueueItem,
                lpx_ProcessStatusClosing,
                (PVOID)DeviceContext);
            ExQueueWorkItem(&DeviceContext->StatusClosingQueueItem, DelayedWorkQueue);

            break;

        default:
            break;

    }

    KeLowerIrql (oldirql);

}




// need change
VOID
lpx_ProcessStatusClosing(
    IN PVOID Parameter
    )
{
    PDEVICE_CONTEXT DeviceContext;
    PLIST_ENTRY p;

    NDIS_STATUS ndisStatus;
    KIRQL oldirql;
    NDIS_HANDLE NdisBindingHandle;

    DeviceContext = (PDEVICE_CONTEXT)Parameter;

    //
    // Prevent new activity on the connection.
    //

    DeviceContext->State = DEVICECONTEXT_STATE_DOWN;


    //
    // Close the NDIS binding.
    //

    NdisBindingHandle = DeviceContext->NdisBindingHandle;
    
    DeviceContext->NdisBindingHandle = NULL;
        
    if (NdisBindingHandle != NULL) {

        KeInitializeEvent(
            &DeviceContext->NdisRequestEvent,
            NotificationEvent,
            FALSE
        );

        NdisCloseAdapter(
            &ndisStatus,
            NdisBindingHandle);

        if (ndisStatus == NDIS_STATUS_PENDING) {

 
               
			DebugPrint(0,("Adapter close pended.\n"));
 

            //
            // The completion routine will set NdisRequestStatus.
            //

            KeWaitForSingleObject(
                &DeviceContext->NdisRequestEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );

            ndisStatus = DeviceContext->NdisRequestStatus;

            KeResetEvent(
                &DeviceContext->NdisRequestEvent
                );

        }
    }
    
 

    // And remove creation ref if it has not already been removed
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
   
        // Remove creation reference
        LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext);
		InterlockedDecrement(&global.NumberOfBinds);
    }


}






VOID
LpxProtoSendComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_PACKET  NdisPacket,
    IN NDIS_STATUS NdisStatus
    )
{
    KIRQL			lpxOldIrql;

	LpxSendComplete(
		ProtocolBindingContext,
		NdisPacket,
		NdisStatus
    );	

	return;
}


VOID
LpxProtoTransferDataComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_PACKET  Packet,
    IN NDIS_STATUS  Status,
    IN UINT  BytesTransferred
    )
{

    DebugPrint(2, (" LpxTransferDataComplete: Entered.\n"));	
	LpxTransferDataComplete(
		ProtocolBindingContext,
		Packet,
		Status,
		BytesTransferred
	);

	return;

}





NDIS_STATUS
LpxProtoReceiveIndicate(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_HANDLE  MacReceiveContext,
    IN PVOID  HeaderBuffer,
    IN UINT  HeaderBufferSize,
    IN PVOID  LookAheadBuffer,
    IN UINT  LookaheadBufferSize,
    IN UINT  PacketSize
    )
{
    
    PDEVICE_CONTEXT		deviceContext;
	USHORT				protocol;
	PNDIS_PACKET        packet;
	PNDIS_BUFFER		firstBuffer;	
	PUCHAR				packetData;
	NDIS_STATUS         status;
	UINT				bytesTransfered = 0;
	UINT				startOffset = 0;
	DebugPrint(2, ("LPXReceiveIndicate: Packet, Size: 0x0%lx LookaheadSize: 0x0%lx\n 00:",
		PacketSize, LookaheadBufferSize));
/*
#if DBG
{
	PUCHAR p;
	SHORT i;
        p = (PUCHAR)LookAheadBuffer;
        for (i=0;i<25;i++) {
			DbgPrint(" %2x",p[i]);
        }
        DbgPrint("\n");
}
#endif
*/
	//
	//	validation
	//
	if (HeaderBufferSize != ETHERNET_HEADER_LENGTH) {
		DebugPrint(2, ("HeaderBufferSize = %x\n", HeaderBufferSize));
		return NDIS_STATUS_NOT_ACCEPTED;
	}
	
	RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)HeaderBuffer)[12], sizeof(USHORT));


	//
	// if Ether Type less than 0x0600 ( 1536 )
	//
	// added by hootch 09242003
	//
	if(  NTOHS(protocol) < 0x0600 &&
		( protocol != HTONS(0x0060) && // LOOP: Ethernet Loopback
			protocol != HTONS(0x0200) && // PUP : Xerox PUP packet
				protocol != HTONS(0x0201)  // PUPAP: Xerox PUP address trans packet
				)) {
		RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)LookAheadBuffer)[LENGTH_8022LLCSNAP - 2], sizeof(USHORT));
		PacketSize -= LENGTH_8022LLCSNAP ;
		LookaheadBufferSize -= LENGTH_8022LLCSNAP ;
		startOffset = LENGTH_8022LLCSNAP ;
}




    if(protocol != HTONS(ETH_P_LPX)) {
		DebugPrint(2, ("Type = %x\n", protocol));

		return NDIS_STATUS_NOT_ACCEPTED;
	}

	if (HeaderBufferSize < ETHERNET_HEADER_LENGTH) {

		return NDIS_STATUS_NOT_ACCEPTED;
	}


    DebugPrint(2, ("LpxReceiveIndicate, PacketSize = %d, LookaheadBufferSize = %d, LPX_HEADER2 size = %d\n",
		PacketSize, LookaheadBufferSize, sizeof(LPX_HEADER2)));

	deviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

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
		return NDIS_STATUS_NOT_ACCEPTED;
	}
		
	RtlCopyMemory(
			&RESERVED(packet)->EthernetHeader,
			HeaderBuffer,
			ETHERNET_HEADER_LENGTH
			);
	
	if(PacketSize == LookaheadBufferSize) {
		NdisQueryPacket(
			packet,
			NULL,
			NULL,
			&firstBuffer,
			NULL
			);
	
   		packetData = MmGetMdlVirtualAddress(firstBuffer);
		RtlCopyMemory(
					packetData,
					LookAheadBuffer,
					LookaheadBufferSize
					);
		LpxTransferDataComplete(
                                deviceContext,
                                packet,
                                NDIS_STATUS_SUCCESS,
                                LookaheadBufferSize
								);
	} else {
		NdisTransferData(
			&status,
			deviceContext->NdisBindingHandle,
	        MacReceiveContext,
		    0,
			PacketSize,
	        packet,
		    &bytesTransfered
			);
	    if (status == NDIS_STATUS_PENDING) {
		    return NDIS_STATUS_SUCCESS;
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
//
//	route packets to each connection.
//	SmpDoReceive() does actual works for the Stream-like packets.
//
//	called from NbfReceiveComplete()
//
//
VOID
LpxProtoReceiveComplete(
				IN NDIS_HANDLE BindingContext
					)
{
	PLIST_ENTRY			pListEntry;
	PNDIS_PACKET		Packet;
	PNDIS_BUFFER		firstBuffer;
	KIRQL				oldIrql;
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;

	PUCHAR				bufferData;
	UINT				bufferLength;
	UINT				totalCopied;
	UINT				copied;
	UINT				bufferNumber;
    PDEVICE_CONTEXT     deviceContext = (PDEVICE_CONTEXT)BindingContext;
    PDEVICE_CONTEXT     addressDeviceContext;
	PCONTROL_DEVICE_CONTEXT	ControlContext;
    PTP_ADDRESS			address;

    PLIST_ENTRY			Flink;
	PLIST_ENTRY			listHead;
	PLIST_ENTRY			thisEntry;
	PLIST_ENTRY			irpListEntry;
	PIRP				irp;

	PUCHAR				packetData = NULL;
	PLIST_ENTRY			p;
	KIRQL				lpxOldIrql;
	USHORT				usPacketSize;
	UINT				uiBufferSize;

	PSERVICE_POINT		connectionServicePoint;
	PSERVICE_POINT		listenServicePoint;

	PSERVICE_POINT		connectingServicePoint;

	BOOLEAN				refAddress = FALSE ;
	BOOLEAN				refAddressFile = FALSE ;
	BOOLEAN				refConnection = FALSE ;
	
	DebugPrint(2, (" LpxReceiveComplete: Entered.\n"));	

	
	//
	// Process In Progress Packets.
	//

	while((pListEntry = ExInterlockedRemoveHeadList(&deviceContext->PacketInProgressList, &deviceContext->PacketInProgressQSpinLock)) != NULL) {
		reserved = CONTAINING_RECORD(pListEntry, LPX_RESERVED, ListElement);
		Packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		
		packetData = NULL;	
		lpxHeader = RESERVED(Packet)->LpxSmpHeader;
		
		DebugPrint(9,("From %02X%02X%02X%02X%02X%02X:%04X\n",
			RESERVED(Packet)->EthernetHeader.SourceAddress[0],
			RESERVED(Packet)->EthernetHeader.SourceAddress[1],
			RESERVED(Packet)->EthernetHeader.SourceAddress[2],
			RESERVED(Packet)->EthernetHeader.SourceAddress[3],
			RESERVED(Packet)->EthernetHeader.SourceAddress[4],
			RESERVED(Packet)->EthernetHeader.SourceAddress[5],
			lpxHeader->SourcePort));
		
		DebugPrint(9,("To %02X%02X%02X%02X%02X%02X:%04X\n",
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
		ControlContext = (PCONTROL_DEVICE_CONTEXT)global.ControlDeviceObject;
		ACQUIRE_SPIN_LOCK (&ControlContext->SpinLock, &lpxOldIrql);
		for (Flink = ControlContext->AddressDatabase.Flink;
			Flink != &ControlContext->AddressDatabase;
			Flink = Flink->Flink, address = NULL) {				
			address = CONTAINING_RECORD (
				Flink,
				TP_ADDRESS,
				Linkage);
			
			if ((address->State & ADDRESS_STATE_CLOSING) != 0) {
				continue;
			}
			
			if (address->NetworkName == NULL) {
				continue;
			}

			if(address->NetworkName->LpxAddress.Port == lpxHeader->DestinationPort) {
				// Broadcast?
				ULONG	ulSum, i;
				
				ulSum = 0;
				for(i = 0; i < 6; i++)
					ulSum += RESERVED(Packet)->EthernetHeader.DestinationAddress[i];

				if(ulSum == 0xFF * 6) {
					//
					//
					// added by hootch
					lpx_ReferenceAddress(address) ;
					refAddress = TRUE ;
					break;
				}

				if(!memcmp(&address->NetworkName->LpxAddress.Node, RESERVED(Packet)->EthernetHeader.DestinationAddress, ETHERNET_ADDRESS_LENGTH)) {
					//
					// added by hootch
					lpx_ReferenceAddress(address) ;
					refAddress = TRUE ;
					break;
				}
			}
		}
		
		// lock was missing @hootch@ 0825
		RELEASE_SPIN_LOCK (&ControlContext->SpinLock, lpxOldIrql);
		if(address == NULL) {
			DebugPrint(2, ("No End Point. To %02X%02X%02X%02X%02X%02X:%04X\n",
				RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
				lpxHeader->DestinationPort));
			
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
			connectingServicePoint = NULL;


			// lock was missing @hootch@ 0825
			ACQUIRE_SPIN_LOCK (&address->SpinLock, &lpxOldIrql);
			
			listHead = &address->ConnectionServicePointList;

			for(thisEntry = listHead->Flink;
				thisEntry != listHead;
				thisEntry = thisEntry->Flink, connectionServicePoint = NULL)
			{
				UCHAR	zeroNode[6] = {0, 0, 0, 0, 0, 0};


				connectionServicePoint = CONTAINING_RECORD(thisEntry, SERVICE_POINT, ServicePointListEntry);
				DebugPrint(2,("connectionServicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
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
					//	reference Connection
					//	hootch	09042003
					lpx_ReferenceConnection(connectionServicePoint->Connection) ;
					refConnection = TRUE ;



					DebugPrint(2, ("[LPX] connectionServicePoint = %p found!\n", connectionServicePoint));

					break;
				}

				if(connectionServicePoint->SmpState == SMP_LISTEN 
					&& !memcmp(&connectionServicePoint->DestinationAddress.Node, zeroNode , 6)
					&& connectionServicePoint->ListenIrp != NULL)
				{

					listenServicePoint = connectionServicePoint;
					DebugPrint(1, ("listenServicePoint = %p found!\n", listenServicePoint));
				}


				if(connectionServicePoint->SmpState == SMP_SYN_SENT 
					&& !memcmp(&connectionServicePoint->DestinationAddress.Node,  RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH)
					&& (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ))  
				{

					connectingServicePoint = connectionServicePoint;
					DebugPrint(1, ("connectingServicePoint = %p\n", connectingServicePoint));
				}


				connectionServicePoint = NULL;
			}


			
			if(connectionServicePoint == NULL) {
				if(listenServicePoint != NULL) {
					connectionServicePoint = listenServicePoint;
					//
					//	add one reference count
					//	hootch	09042003
					lpx_ReferenceConnection(connectionServicePoint->Connection) ;
					refConnection = TRUE ;
				} else {
					if(connectingServicePoint != NULL) {
						memcpy(connectingServicePoint->DestinationAddress.Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH);
						connectingServicePoint->DestinationAddress.Port = lpxHeader->SourcePort;
						connectionServicePoint = connectingServicePoint;
						//
						//	add one reference count
						//	hootch	09042003
						lpx_ReferenceConnection(connectionServicePoint->Connection) ;
						refConnection = TRUE ;
					} else {
						RELEASE_SPIN_LOCK (&address->SpinLock, lpxOldIrql);

						goto TossPacket;
					}
				}
			}
			// lock was missing @hootch@ 0825
			RELEASE_SPIN_LOCK (&address->SpinLock, lpxOldIrql);

			RESERVED(Packet)->PacketDataOffset = sizeof(LPX_HEADER2);

			//
			//	call stream-like(called LPX-Stream) routine to do more process
			//
			SmpDoReceiveRequest(&connectionServicePoint->SmpContext, Packet);

			goto loopEnd;

		//
		//	a datagram packet
		//
		} else if(lpxHeader->LpxType == LPX_TYPE_DATAGRAM) {

//			DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete: DataGram packet arrived.\n"));
//			DebugPrint(DebugLevel, ("Address = %0x \n", address));
			if (address->RegisteredReceiveDatagramHandler) {

				ULONG				indicateBytesCopied, mdlBytesCopied, bytesToCopy;
				NTSTATUS			ntStatus;
				TA_NETBIOS_ADDRESS	sourceName;
				PIRP				irp;
				PIO_STACK_LOCATION	irpSp;
				UINT				headerLength;
				UINT				bufferOffset;
				UINT				totalCopied;
				PUCHAR				bufferData;
				PNDIS_BUFFER		nextBuffer;
				ULONG				userDataLength;
				KIRQL				cancelIrql;

				userDataLength = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)) - sizeof(LPX_HEADER2);

//				DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete: call UserDataGramHandler with a DataGram packet. NTOHS(lpxHeader->PacketSize) = %d, userDataLength = %d\n",
//					NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK), userDataLength));

				packetData = ExAllocatePool (NonPagedPool, userDataLength);
				//
				//	NULL pointer check.
				//
				//	added by @hootch@ 0812
				//
				if(packetData == NULL) {
					DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete failed to allocate nonpaged pool for packetData\n"));
					goto TossPacket ;
				}


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
				//	call user-defined ReceiveDatagramHandler with copied data
				//

				indicateBytesCopied = 0;

				sourceName.TAAddressCount = 1;
				sourceName.Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
				sourceName.Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
				memcpy(((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH);
				((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Port = lpxHeader->SourcePort;

				ntStatus = (*address->ReceiveDatagramHandler)(
					address->ReceiveDatagramHandlerContext,
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
//					DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete: A datagram packet consumed. STATUS_SUCCESS, userDataLength = %d, indicateBytesCopied = %d\n", userDataLength, indicateBytesCopied));
				} else if (ntStatus == STATUS_DATA_NOT_ACCEPTED) 
				{
					//
					// The client did not accept the datagram and we need to satisfy
					// a TdiReceiveDatagram, if possible.
					//
					DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete: DataGramHandler didn't accept a datagram packet.\n"));
					
					DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete: Picking off a rcv datagram request from this address.\n"));
					

					ntStatus = STATUS_MORE_PROCESSING_REQUIRED;
					
				} else if (ntStatus == STATUS_MORE_PROCESSING_REQUIRED) 
				{
					//
					// The client returned an IRP that we should queue up to the
					// address to satisfy the request.
					//

					DebugPrint(DebugLevel, ("[LPX] LpxReceiveComplete: DataGram STATUS_MORE_PROCESSING_REQUIRED\n"));
					irp->IoStatus.Status = STATUS_PENDING;  // init status information.
					irp->IoStatus.Information = 0;
					irpSp = IoGetCurrentIrpStackLocation (irp); // get current stack loctn.
					if ((irpSp->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL) 
						|| (irpSp->MinorFunction != TDI_RECEIVE_DATAGRAM)) 
					{
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
			lpx_DereferenceAddress(address);
			refAddress = FALSE ;
		}
		
		if(refConnection) {
			lpx_DereferenceConnection(connectionServicePoint->Connection);
			refConnection = FALSE ;
		}

		continue;
	}

    return;

}




//
// PnP module for Ndis 

VOID 
LpxProtoBindAdapter(
    OUT PNDIS_STATUS NdisStatus,
    IN NDIS_HANDLE  BindContext,
    IN PNDIS_STRING  DeviceName,
    IN PVOID  SystemSpecific1,
    IN PVOID  SystemSpecific2
    )
{
    PUNICODE_STRING ExportName;
    UNICODE_STRING ExportString;
    ULONG i, j, k;
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext;
	PLIST_ENTRY		listHead;
	PLIST_ENTRY		thisEntry;
    UNICODE_STRING	deviceString;

#if DBG
    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }
#endif

   
    DebugPrint(0, ("ENTER NbfProtocolBindAdapter for %S\n", DeviceName->Buffer));
    

    if (global.Config == NULL) {
        //
        // This allocates the CONFIG_DATA structure and returns
        // it in global.Config.
        //

        status = Lpx_ConfigureTransport(&global.RegistryPath, &global.Config);

        if (!NT_SUCCESS (status)) {
            PANIC (" Failed to initialize transport, binding failed.\n");
            *NdisStatus = NDIS_STATUS_RESOURCES;
            return;
        }

    }

    //
    // Loop through all the adapters that are in the configuration
    // information structure (this is the initial cache) until we
    // find the one that NDIS is calling Protocol bind adapter for. 
    //        

    for (j = 0; j < global.Config->NumAdapters; j++ ) {

        if (NdisEqualString(DeviceName, &global.Config->Names[j], TRUE)) {
            break;
        }
    }

    if (j < global.Config->NumAdapters) {

        // We found the bind to export mapping in initial cache

        ExportName = &global.Config->Names[global.Config->DevicesOffset + j];
    }
    else {

   
        
            DebugPrint(0,("\nNot In Initial Cache = %08x\n\n", DeviceName->Buffer));

            DebugPrint(0,("Bind Names in Initial Cache: \n"));

            for (k = 0; k < global.Config->NumAdapters; k++)
            {
                DebugPrint(0,("Config[%2d]: @ %08x, %75S\n",
                           k, &global.Config->Names[k],
                           global.Config->Names[k].Buffer));
            }

            DebugPrint(0,("Export Names in Initial Cache: \n"));

            for (k = 0; k < global.Config->NumAdapters; k++)
            {
                DebugPrint(0,("Config[%2d]: @ %08x, %75S\n",
                           k, &global.Config->Names[global.Config->DevicesOffset + k],
                           global.Config->Names[global.Config->DevicesOffset + k].Buffer));
            }

            DebugPrint(0,("\n\n"));
       

        ExportName = &ExportString;

        //
        // We have not found the name in the initial registry info;
        // Read the registry and check if a new binding appeared...
        //

        *NdisStatus = Lpx_GetExportNameFromRegistry(&global.RegistryPath,
                                                   DeviceName,
                                                   ExportName
                                                  );
        if (!NT_SUCCESS (*NdisStatus))
        {
            return;
        }
    }
        
    lpx_CreateNdisDeviceContext(NdisStatus, 
                                  global.DriverObject,
                                  global.Config,
                                  DeviceName,
                                  ExportName,
                                  SystemSpecific1,
                                  SystemSpecific2
                                 );
	
	// Check if we need to de-allocate the ExportName buffer

	if (ExportName == &ExportString)
	{
		ExFreePool(ExportName->Buffer);
	}

	if (*NdisStatus == NDIS_STATUS_SUCCESS) {

		InterlockedIncrement(&global.NumberOfBinds);
	

    
		DebugPrint(0,("LEAVE NbfProtocolBindAdapter for %S with Status %08x\n", 
							DeviceName->Buffer, *NdisStatus));




		listHead = &global.NIC_DevList;
		for(deviceContext = NULL, thisEntry = listHead->Flink;
			thisEntry != listHead;
			thisEntry = thisEntry->Flink)
		{
			deviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
			RtlInitUnicodeString(&deviceString, deviceContext->DeviceName);

			if (NdisEqualString(&deviceString, ExportName, TRUE)) 
				break;
		}

		ASSERT(deviceContext);

		if(global.LpxPrimaryDeviceContext != NULL)
			return;

		global.LpxPrimaryDeviceContext = deviceContext;
	}

    return;
}


VOID
LpxProtoUnbindAdapter(
    OUT PNDIS_STATUS  NdisStatus,
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_HANDLE  UnbindContext
    )
{
    PDEVICE_CONTEXT deviceContext;
    PTP_ADDRESS Address;
    NTSTATUS status;
	NDIS_STATUS		ndisStatus;
    UINT			deviceCount;
	PLIST_ENTRY		listHead;
	PLIST_ENTRY		thisEntry;
    KIRQL oldirql;
    PLIST_ENTRY p;

#if DBG

    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }
#endif

    // Get the device context for the adapter being unbound
    deviceContext = (PDEVICE_CONTEXT) ProtocolBindingContext;



	//
	//	cause we have only Socket Lpx Device Context
	//
	if(global.ControlDeviceObject== NULL)
		goto Out;

 
    ACQUIRE_DEVICES_LIST_LOCK();

	if (IsListEmpty (&global.NIC_DevList)) {
		RELEASE_DEVICES_LIST_LOCK();

		goto Out;
	}

	listHead = &global.NIC_DevList;
	for(deviceCount = 0, thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = thisEntry->Flink)
	{
		PDEVICE_CONTEXT DeviceContext;
    
		DeviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
		if(DeviceContext->CreateRefRemoved == FALSE)
		{
			deviceCount++;
		}
	}

	RELEASE_DEVICES_LIST_LOCK();


	if(deviceCount == 1)
	{
		// destroy control device context 
		//Lpx_ControlUnbindAdapter((PCONTROL_DEVICE_CONTEXT) global.ControlDeviceObject);
		global.LpxPrimaryDeviceContext = NULL;
	}
	else{
		// change global.LpxPrimaryDeviceContext
		if(deviceContext == global.LpxPrimaryDeviceContext) 
		{
			PDEVICE_CONTEXT DeviceContext;

			deviceContext = NULL;

			ACQUIRE_DEVICES_LIST_LOCK();
			
			listHead = &global.NIC_DevList;
			for(deviceCount = 0, thisEntry = listHead->Flink;
				thisEntry != listHead;
				thisEntry = thisEntry->Flink)
			{
				DeviceContext = CONTAINING_RECORD (thisEntry, DEVICE_CONTEXT, Linkage);
				if(DeviceContext != global.LpxPrimaryDeviceContext)
					break;
				DeviceContext = NULL;
			}

			ASSERT(DeviceContext);
			ASSERT(DeviceContext->CreateRefRemoved == FALSE);
				global.LpxPrimaryDeviceContext = DeviceContext;
		
			RELEASE_DEVICES_LIST_LOCK();
		}

	}
Out:


    
    DebugPrint(0, ("ENTER LpxProtocolUnbindAdapter for %S\n", deviceContext->DeviceName));
    

  
    if (InterlockedExchange(&deviceContext->CreateRefRemoved, TRUE) == FALSE) {
        DebugPrint(0,("TdiDeregisterNetAddress\n"));
        *NdisStatus = TdiDeregisterNetAddress(deviceContext->ReservedAddressHandle);

        if (!NT_SUCCESS (*NdisStatus)) {
        

            DebugPrint(0,("No success deregistering this address,STATUS = %08X\n",*NdisStatus));


            // this can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            deviceContext->CreateRefRemoved = FALSE;
            
            return;
        }
       DebugPrint(0,("TdiDeregisterNetAddress\n"));        
        // Inform TDI (and its clients) that device is going away
        *NdisStatus = TdiDeregisterDeviceObject(deviceContext->TdiDeviceHandle);

        if (!NT_SUCCESS (*NdisStatus)) {
        
            
            DebugPrint(0,("No success deregistering device object,STATUS = %08X\n",*NdisStatus));
            

            // This can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            deviceContext->CreateRefRemoved = FALSE;

            return;
        }

        // Clear away the association with the underlying PDO object
        deviceContext->PnPContext = NULL;


        // Cleanup the Ndis Binding as it is not useful on return
        // from this function - do not try to use it after this
        LpxCloseNdis(deviceContext);
      
 
        LPX_DEREFERENCE_DEVICECONTEXT( deviceContext);

		InterlockedDecrement(&global.NumberOfBinds);

 
    }
    else {
    
        *NdisStatus = NDIS_STATUS_SUCCESS;
    }

    
        DebugPrint(0, ("LEAVE LpxProtocolUnbindAdapter for %S with Status %08x\n",
                        deviceContext->DeviceName, *NdisStatus));
    

    return;
}


VOID
Lpx_ControlUnbindAdapter(
	IN	PCONTROL_DEVICE_CONTEXT DeviceContext
    )
{
    PTP_ADDRESS Address;
    NTSTATUS status;
    KIRQL oldirql;
    PLIST_ENTRY p;
	NDIS_STATUS NdisStatus;
	
	NdisStatus = NDIS_STATUS_SUCCESS;	
	
	DebugPrint(0, ("Control device unbind function %p\n", DeviceContext));


#if DBG

    // We can never be called at DISPATCH or above
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
    {
        DbgBreakPoint();
    }

#endif


    DebugPrint(0, ("ENTER Lpx_ControlUnbindAdapter for %S\n", DeviceContext->DeviceName));
   

 
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) 
	{	
 
		UNICODE_STRING  unicodeDeviceName;
		RtlInitUnicodeString(&unicodeDeviceName, SOCKETLPX_DOSDEVICE_NAME);
		IoDeleteSymbolicLink( &unicodeDeviceName);
	
        
        // Inform TDI (and its clients) that device is going away
        NdisStatus = TdiDeregisterDeviceObject(DeviceContext->TdiDeviceHandle);

        if (!NT_SUCCESS (NdisStatus)) {
        
            
			DebugPrint(0,("No success deregistering control device object,STATUS = %08X\n",NdisStatus));
            

            // This can never happen
            ASSERT(FALSE);

            // In case it happens, this allows a redo of the unbind
            DeviceContext->CreateRefRemoved = FALSE;

            return;
        }

        // Clear away the association with the underlying PDO object
        DeviceContext->PnPContext = NULL;


        // Removing creation reference means that once all handles
        // r closed,device will automatically be garbage-collected
        LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext);
        InterlockedDecrement(&global.NumberOfBinds);
    } else {
    
        // Ignore any duplicate Unbind Indications from NDIS layer
        NdisStatus = NDIS_STATUS_SUCCESS;
    }

   
    DebugPrint(0,("LEAVE  Lpx_ControlUnbindAdapterfor \n"));
    

    return;
}







NTSTATUS
LpxInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
    IN PCONFIG_DATA LpxConfig,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine introduces this transport to the NDIS interface and sets up
    any necessary NDIS data structures (Buffer pools and such). It will be
    called for each adapter opened by this transport.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/
{
    ULONG SendPacketReservedLength;
    ULONG ReceivePacketReservedLen;
    ULONG SendPacketPoolSize;
    ULONG ReceivePacketPoolSize;
    NDIS_STATUS NdisStatus;
    NDIS_STATUS OpenErrorStatus;

	NDIS_MEDIUM NbfSupportedMedia[] = { NdisMedium802_3 };
    UINT SelectedMedium;
    NDIS_REQUEST LpxRequest;
    UCHAR LpxDataBuffer[6];
    NDIS_OID LpxOid;
    UCHAR WanProtocolId[6] = { 0x80, 0x00, 0x00, 0x00, 0x80, 0xd5 };
    ULONG WanHeaderFormat = NdisWanHeaderEthernet;
    ULONG MinimumLookahead = 128 + sizeof(ETHERNET_HEADER) + sizeof(LPX_RESERVED);
    ULONG MacOptions;


    //
    // Initialize this adapter for NBF use through NDIS
    //

    //
    // This event is used in case any of the NDIS requests
    // pend; we wait until it is set by the completion
    // routine, which also sets NdisRequestStatus.
    //

    KeInitializeEvent(
        &DeviceContext->NdisRequestEvent,
        NotificationEvent,
        FALSE
    );

    DeviceContext->NdisBindingHandle = NULL;

    NdisOpenAdapter (
        &NdisStatus,
        &OpenErrorStatus,
        &DeviceContext->NdisBindingHandle,
        &SelectedMedium,
        NbfSupportedMedia,
        sizeof (NbfSupportedMedia) / sizeof(NDIS_MEDIUM),
        global.NdisProtocolHandle,
        (NDIS_HANDLE)DeviceContext,
        AdapterString,
        0,
        NULL);

    if (NdisStatus == NDIS_STATUS_PENDING) {

  
        DebugPrint(0, ("Adapter %S open pended.\n", AdapterString));
       

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus == NDIS_STATUS_SUCCESS) {

        DebugPrint(0, ("Adapter %S successfully opened.\n", AdapterString));

    } else {


		DebugPrint(0, ("Adapter open %S failed, status: %s.\n",
			AdapterString,
			lpx_GetNdisStatus (NdisStatus)));
 
        return STATUS_INSUFFICIENT_RESOURCES;
    }



    //
    // Get the information we need about the adapter, based on
    // the media type.
    //

	DeviceContext->MacInfo.DestinationOffset = 0;
	DeviceContext->MacInfo.SourceOffset = 6;
	DeviceContext->MacInfo.SourceRouting = FALSE;
	DeviceContext->MacInfo.AddressLength = 6;
	DeviceContext->MacInfo.TransferDataOffset = 0;
	DeviceContext->MacInfo.MaxHeaderLength = 14;
	DeviceContext->MacInfo.MediumType = NdisMedium802_3;
	DeviceContext->MacInfo.MediumAsync = FALSE;
    DeviceContext->MacInfo.QueryWithoutSourceRouting = FALSE;   
    DeviceContext->MacInfo.AllRoutesNameRecognized = FALSE;
       


    //
    // Set the multicast/functional addresses first so we avoid windows where we
    // receive only part of the addresses.
    //

    MacSetNetBIOSMulticast (
            DeviceContext->MacInfo.MediumType,
            DeviceContext->NetBIOSAddress.Address);


	

 
	//
	// Fill in the data for our multicast list.
	//

	RtlCopyMemory(LpxDataBuffer, DeviceContext->NetBIOSAddress.Address, 6);

	//
	// Now fill in the NDIS_REQUEST.
	//

	LpxRequest.RequestType = NdisRequestSetInformation;
	LpxRequest.DATA.SET_INFORMATION.Oid = OID_802_3_MULTICAST_LIST;
	LpxRequest.DATA.SET_INFORMATION.InformationBuffer = &LpxDataBuffer;
	LpxRequest.DATA.SET_INFORMATION.InformationBufferLength = 6;



	NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

	if (NdisStatus != NDIS_STATUS_SUCCESS) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	



	
	LpxOid = OID_802_3_CURRENT_ADDRESS;

	LpxRequest.RequestType = NdisRequestQueryInformation;
	LpxRequest.DATA.QUERY_INFORMATION.Oid = LpxOid;
	LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = DeviceContext->LocalAddress.Address;
	LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 6;

	NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

	if (NdisStatus != NDIS_STATUS_SUCCESS) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
    //
    // Set up the reserved Netbios address.
    //

    RtlZeroMemory(DeviceContext->ReservedNetBIOSAddress, 10);
    RtlCopyMemory(&DeviceContext->ReservedNetBIOSAddress[10], DeviceContext->LocalAddress.Address, 6);



    //
    // Now query the maximum packet sizes.
    //

    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_FRAME_SIZE;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MaxReceivePacketSize);
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MaxSendPacketSize);
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DeviceContext->CurSendPacketSize = DeviceContext->MaxSendPacketSize;


    //
    // Now set the minimum lookahead size.
    //

    LpxRequest.RequestType = NdisRequestSetInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_CURRENT_LOOKAHEAD;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MinimumLookahead;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now query the link speed for non-wan media
    // 

	LpxRequest.RequestType = NdisRequestQueryInformation;
	LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_LINK_SPEED;
	LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &(DeviceContext->MediumSpeed);
	LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

	NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

	if (NdisStatus != NDIS_STATUS_SUCCESS) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	DeviceContext->MediumSpeedAccurate = TRUE;

 


  


    //
    // Now query the MAC's optional characteristics.
    //

    LpxRequest.RequestType = NdisRequestQueryInformation;
    LpxRequest.DATA.QUERY_INFORMATION.Oid = OID_GEN_MAC_OPTIONS;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBuffer = &MacOptions;
    LpxRequest.DATA.QUERY_INFORMATION.InformationBufferLength = 4;

    NdisStatus = LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }


    DeviceContext->MacInfo.CopyLookahead =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA) != 0);
    DeviceContext->MacInfo.ReceiveSerialized =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_RECEIVE_SERIALIZED) != 0);
    DeviceContext->MacInfo.TransferSynchronous =
        (BOOLEAN)((MacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND) != 0);
    DeviceContext->MacInfo.SingleReceive =
        (BOOLEAN)(DeviceContext->MacInfo.ReceiveSerialized && DeviceContext->MacInfo.TransferSynchronous);



    //
    // Now that everything is set up, we enable the filter
    // for packet reception.
    //

 
 

	RtlStoreUlong((PULONG)LpxDataBuffer,
		(NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_BROADCAST));


    //
    // Now fill in the NDIS_REQUEST.
    //

    LpxRequest.RequestType = NdisRequestSetInformation;
    LpxRequest.DATA.SET_INFORMATION.Oid = OID_GEN_CURRENT_PACKET_FILTER;
    LpxRequest.DATA.SET_INFORMATION.InformationBuffer = &LpxDataBuffer;
    LpxRequest.DATA.SET_INFORMATION.InformationBufferLength = sizeof(ULONG);

    LpxSubmitNdisRequest (DeviceContext, &LpxRequest, AdapterString);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;

}   /* NbfInitializeNdis */


VOID
MacReturnMaxDataSize(
    IN PNBF_NDIS_IDENTIFICATION MacInfo,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    IN UINT DeviceMaxFrameSize,
    IN BOOLEAN AssumeWorstCase,
    OUT PUINT MaxFrameSize
    )
{
     *MaxFrameSize = DeviceMaxFrameSize - 14;
}



VOID
MacSetNetBIOSMulticast (
    IN NDIS_MEDIUM Type,
    IN PUCHAR Buffer
    )
{
    switch (Type) {
    case NdisMedium802_3:
    case NdisMediumDix:
        Buffer[0] = 0x03;
        Buffer[ETHERNET_ADDRESS_LENGTH-1] = 0x01;
        break;

    default:
        PANIC ("MacSetNetBIOSAddress: PANIC! called with unsupported Mac type.\n");
    }
}


NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest2,
    IN PNDIS_STRING AdapterString
    )
{
    NDIS_STATUS NdisStatus;

    if (DeviceContext->NdisBindingHandle) {
        NdisRequest(
            &NdisStatus,
            DeviceContext->NdisBindingHandle,
            NdisRequest2);
	}
    else {
        NdisStatus = STATUS_INVALID_DEVICE_STATE;
    }
    
    if (NdisStatus == NDIS_STATUS_PENDING) {

  
        DebugPrint(0, ("OID %lx pended.\n",
                NdisRequest2->DATA.QUERY_INFORMATION.Oid));
       

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus != STATUS_SUCCESS) {


        if (NdisRequest2->RequestType == NdisRequestSetInformation) {
            DebugPrint(0, ("Nbfdrvr: Set OID %lx failed: %s.\n",
                NdisRequest2->DATA.SET_INFORMATION.Oid, lpx_GetNdisStatus(NdisStatus)));
        } else {
            DebugPrint(0,("Nbfdrvr: Query OID %lx failed: %s.\n",
                NdisRequest2->DATA.QUERY_INFORMATION.Oid, lpx_GetNdisStatus(NdisStatus)));
        }

        
    }

    return NdisStatus;
}



VOID
LpxCloseNdis (
    IN PDEVICE_CONTEXT DeviceContext
    )
{
    NDIS_STATUS ndisStatus;
    NDIS_HANDLE NdisBindingHandle;
    
    //
    // Close the NDIS binding.
    //
    
    NdisBindingHandle = DeviceContext->NdisBindingHandle;
    
    DeviceContext->NdisBindingHandle = NULL;
        
    if (NdisBindingHandle != NULL) {
    
        //
        // This event is used in case any of the NDIS requests
        // pend; we wait until it is set by the completion
        // routine, which also sets NdisRequestStatus.
        //

        KeInitializeEvent(
            &DeviceContext->NdisRequestEvent,
            NotificationEvent,
            FALSE
        );

        NdisCloseAdapter(
            &ndisStatus,
            NdisBindingHandle);

        if (ndisStatus == NDIS_STATUS_PENDING) {

  
            DebugPrint(0, ("Adapter close pended.\n"));
            

            //
            // The completion routine will set NdisRequestStatus.
            //

            KeWaitForSingleObject(
                &DeviceContext->NdisRequestEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );

            ndisStatus = DeviceContext->NdisRequestStatus;
			DebugPrint(0,("NdisCloseAdapter Status %s\n",lpx_GetNdisStatus(ndisStatus)));
            KeResetEvent(
                &DeviceContext->NdisRequestEvent
                );

        }

  

    }
}   /* NbfCloseNdis */



NDIS_STATUS
LpxProtoPnPEventHandler(
                    IN NDIS_HANDLE ProtocolBindContext,
                    IN PNET_PNP_EVENT NetPnPEvent
                          )
{
    PNET_PNP_EVENT_RESERVED NetPnPReserved;
    PWORK_QUEUE_ITEM PnPWorkItem;

    PnPWorkItem = (PWORK_QUEUE_ITEM)ExAllocatePoolWithTag(
                                        NonPagedPool,
                                        sizeof (WORK_QUEUE_ITEM),
                                        LPX_MEM_TAG_WORK_ITEM);

    if (PnPWorkItem == NULL) 
    {
        return NDIS_STATUS_RESOURCES;
    }

    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)NetPnPEvent->TransportReserved;
    NetPnPReserved->PnPWorkItem = PnPWorkItem;
    NetPnPReserved->DeviceContext = (PDEVICE_CONTEXT) ProtocolBindContext;

    ExInitializeWorkItem(
            PnPWorkItem,
            LpxPnPEventDispatch,
            NetPnPEvent);
            
    ExQueueWorkItem(PnPWorkItem, CriticalWorkQueue);

    return NDIS_STATUS_PENDING;
}

VOID
LpxPnPEventDispatch(
                    IN PVOID NetPnPEvent
                   )
{
    PNET_PNP_EVENT_RESERVED NetPnPReserved;
    PDEVICE_CONTEXT  DeviceContext;
    UNICODE_STRING   DeviceString;
    PTDI_PNP_CONTEXT tdiPnPContext1;
    PTDI_PNP_CONTEXT tdiPnPContext2;
    NDIS_STATUS      retVal;

    // Retrieve the transport information block in event
    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)((PNET_PNP_EVENT)NetPnPEvent)->TransportReserved;

    // Free the memory allocated for this work item itself
    ExFreePool(NetPnPReserved->PnPWorkItem);
     
    // Get the device context for the adapter being unbound
    DeviceContext = NetPnPReserved->DeviceContext;

    // In case everything goes ok, we return an NDIS_SUCCESS
    retVal = STATUS_SUCCESS;
    
    // Dispatch the PnP Event to the appropriate PnP handler
    switch (((PNET_PNP_EVENT)NetPnPEvent)->NetEvent)
    {
        case NetEventReconfigure:
        case NetEventCancelRemoveDevice:
        case NetEventQueryRemoveDevice:
        case NetEventQueryPower:
        case NetEventSetPower:
        case NetEventPnPCapabilities:
            break;

        case NetEventBindsComplete:
            retVal = LpxPnPBindsComplete(DeviceContext, NetPnPEvent);
            break;

        default:
            ASSERT( FALSE );
    }

    if ( retVal == STATUS_SUCCESS ) 
    {
        if (DeviceContext != NULL)
        {
            RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);
            tdiPnPContext1 = tdiPnPContext2 = NULL;

            //  Notify our TDI clients about this PNP event
            retVal = TdiPnPPowerRequest(&DeviceString,
                                         NetPnPEvent,
                                         tdiPnPContext1, 
                                         tdiPnPContext2,
                                         LpxPnPEventComplete);
        }
    }

    if (retVal != STATUS_PENDING)
    {
        NdisCompletePnPEvent(retVal, (NDIS_HANDLE)DeviceContext, NetPnPEvent);
    }
}

//
// PnP Complete Handler
//
VOID
LpxPnPEventComplete(
                    IN PNET_PNP_EVENT   NetPnPEvent,
                    IN NTSTATUS         retVal
                   )
{
    PNET_PNP_EVENT_RESERVED NetPnPReserved;
    PDEVICE_CONTEXT  DeviceContext;

    // Retrieve the transport information block in event
    NetPnPReserved = (PNET_PNP_EVENT_RESERVED)NetPnPEvent->TransportReserved;

    // Get the device context for the adapter being unbound
    DeviceContext = NetPnPReserved->DeviceContext;

    NdisCompletePnPEvent(retVal, (NDIS_HANDLE)DeviceContext, NetPnPEvent);
}

//
// PnP Handler Dispatches
//

NTSTATUS
LpxPnPBindsComplete(
                    IN PDEVICE_CONTEXT  DeviceContext,
                    IN PNET_PNP_EVENT   NetPnPEvent
                   )
{
    NDIS_STATUS retVal;

    ASSERT(DeviceContext == NULL);

    retVal = TdiProviderReady(global.TdiProviderHandle);

    ASSERT(retVal == STATUS_SUCCESS);

    return retVal;
}


PUCHAR
lpx_GetNdisStatus(
    NDIS_STATUS GeneralStatus
    )
{
    static NDIS_STATUS Status[] = {
        NDIS_STATUS_SUCCESS,
        NDIS_STATUS_PENDING,

        NDIS_STATUS_ADAPTER_NOT_FOUND,
        NDIS_STATUS_ADAPTER_NOT_OPEN,
        NDIS_STATUS_ADAPTER_NOT_READY,
        NDIS_STATUS_ADAPTER_REMOVED,
        NDIS_STATUS_BAD_CHARACTERISTICS,
        NDIS_STATUS_BAD_VERSION,
        NDIS_STATUS_CLOSING,
        NDIS_STATUS_DEVICE_FAILED,
        NDIS_STATUS_FAILURE,
        NDIS_STATUS_INVALID_DATA,
        NDIS_STATUS_INVALID_LENGTH,
        NDIS_STATUS_INVALID_OID,
        NDIS_STATUS_INVALID_PACKET,
        NDIS_STATUS_MULTICAST_FULL,
        NDIS_STATUS_NOT_INDICATING,
        NDIS_STATUS_NOT_RECOGNIZED,
        NDIS_STATUS_NOT_RESETTABLE,
        NDIS_STATUS_NOT_SUPPORTED,
        NDIS_STATUS_OPEN_FAILED,
        NDIS_STATUS_OPEN_LIST_FULL,
        NDIS_STATUS_REQUEST_ABORTED,
        NDIS_STATUS_RESET_IN_PROGRESS,
        NDIS_STATUS_RESOURCES,
        NDIS_STATUS_UNSUPPORTED_MEDIA
    };
    static PUCHAR String[] = {
        "SUCCESS",
        "PENDING",

        "ADAPTER_NOT_FOUND",
        "ADAPTER_NOT_OPEN",
        "ADAPTER_NOT_READY",
        "ADAPTER_REMOVED",
        "BAD_CHARACTERISTICS",
        "BAD_VERSION",
        "CLOSING",
        "DEVICE_FAILED",
        "FAILURE",
        "INVALID_DATA",
        "INVALID_LENGTH",
        "INVALID_OID",
        "INVALID_PACKET",
        "MULTICAST_FULL",
        "NOT_INDICATING",
        "NOT_RECOGNIZED",
        "NOT_RESETTABLE",
        "NOT_SUPPORTED",
        "OPEN_FAILED",
        "OPEN_LIST_FULL",
        "REQUEST_ABORTED",
        "RESET_IN_PROGRESS",
        "RESOURCES",
        "UNSUPPORTED_MEDIA"
    };

    static UCHAR BadStatus[] = "UNDEFINED";
#define StatusCount (sizeof(Status)/sizeof(NDIS_STATUS))
    INT i;

    for (i=0; i<StatusCount; i++)
        if (GeneralStatus == Status[i])
            return String[i];
    return BadStatus;
#undef StatusCount
}