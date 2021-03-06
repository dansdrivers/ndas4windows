/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    dlc.c

Abstract:

    This module contains code which implements the data link layer for the
    transport provider.

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop


VOID
LpxReceiveComplete2(
	IN NDIS_HANDLE BindingContext,
	IN PLIST_ENTRY ReceivedPackets
	);


NDIS_STATUS
LpxReceiveIndication (
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
	PDEVICE_CONTEXT	deviceContext;
	USHORT		    protocol;
	PNDIS_PACKET	packet;
	NDIS_STATUS		status;
	UINT		    bytesTransfered = 0;
	UINT		    startOffset = 0;


	DebugPrint( 4, ("LpxReceiveIndication, Entered\n") );
	
	deviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

	//
	//	validation
	//

	if (HeaderBufferSize != ETHERNET_HEADER_LENGTH) {

		DebugPrint( 4, ("HeaderBufferSize = %x\n", HeaderBufferSize) );
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
	
	RtlCopyMemory( (PUCHAR)&protocol, &((PUCHAR)HeaderBuffer)[12], sizeof(USHORT) );

	//
	//	Discard 802.2 LLC SNAP field.
	//
	// if Ether Type less than 0x0600 ( 1536 )
	//

	if (NTOHS(protocol) < 0x0600  && 
	    protocol != HTONS(0x0060) && // LOOP: Ethernet Loopback
		protocol != HTONS(0x0200) && // PUP : Xerox PUP packet
		protocol != HTONS(0x0201)) { // PUPAP: Xerox PUP address trans packet 

#if __LPX__
		NdisCopyLookaheadData( (PUCHAR)&protocol,
								&((PUCHAR)LookAheadBuffer)[LENGTH_8022LLCSNAP - 2],
								sizeof(USHORT),
								deviceContext->MacOptions );
#endif
		PacketSize -= LENGTH_8022LLCSNAP;
		LookAheadBufferSize -= LENGTH_8022LLCSNAP;
		startOffset = LENGTH_8022LLCSNAP;
	}

	if (protocol != HTONS(ETH_P_LPX)) {
	
		DebugPrint( 4, ("Type = %x\n", protocol) );
		return NDIS_STATUS_NOT_RECOGNIZED;
	}


	//
	//	Check to see if the device context is initialized.
	//

	//ACQUIRE_DPC_SPIN_LOCK( &deviceContext->SpinLock );

	if (!FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) || FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP)) {
	
		//RELEASE_DPC_SPIN_LOCK( &deviceContext->SpinLock );
		DebugPrint( 4,("Device is not initialized. Drop packet\n") );

		return NDIS_STATUS_NOT_RECOGNIZED;
	}

	ASSERT( deviceContext->NdisBindingHandle );

	//RELEASE_DPC_SPIN_LOCK( &deviceContext->SpinLock );

	//
	// DROP PACKET for DEBUGGING!!!!
	//

#if 1 //DBG // Enabled for testing

	if (PacketRxDropRate) {

		PacketRxCountForDrop++;
				
		if ((PacketRxCountForDrop % 1000) <= PacketRxDropRate) {
			PLPX_HEADER        lpxHeader = (PLPX_HEADER)LookAheadBuffer;
#if 0
			if ((PacketRxCountForDrop % (PacketRxDropRate*20)) == 0) 
				DebugPrint( 1, ("[Drop(%x,%x,%x))]\n", 
								 NTOHS(lpxHeader->Lsctl), NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)) );
#endif			
			DebugPrint( 1, ("D") );

			return NDIS_STATUS_NOT_RECOGNIZED;
		}
	}

#endif

	ASSERT( startOffset == 0 );

	DebugPrint( 4, ("LpxReceiveIndication, PacketSize = %d, LookAheadBufferSize = %d, LPX_HEADER size = %d\n",
					 PacketSize, LookAheadBufferSize, sizeof(LPX_HEADER)) );

	if (LookAheadBufferSize - startOffset >= sizeof(LPX_HEADER)) {

		PNDIS_BUFFER	firstBuffer;    
		PUCHAR		    packetData;
		PLPX_HEADER		lpxHeader;
		USHORT			lpxHeaderSize;

		
		lpxHeader = (PLPX_HEADER)((PBYTE)LookAheadBuffer + startOffset);
		
		lpxHeaderSize = sizeof(LPX_HEADER);

#if __LPX_OPTION_ADDRESSS__

		if (FlagOn(lpxHeader->Option, LPX_OPTION_SOURCE_ADDRESS)) {

			lpxHeaderSize += ETHERNET_ADDRESS_LENGTH;
		}

		if (FlagOn(lpxHeader->Option, LPX_OPTION_DESTINATION_ADDRESS)) {

			lpxHeaderSize += ETHERNET_ADDRESS_LENGTH;
		}

#endif

		if (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) == lpxHeaderSize) {

			status = RcvPacketAlloc( deviceContext,
									 0,
									 &packet );

			if (status == STATUS_SUCCESS) {

				NdisCopyLookaheadData( &RESERVED(packet)->EthernetHeader,
										HeaderBuffer,
										ETHERNET_HEADER_LENGTH,
										deviceContext->MacOptions );

				RESERVED(packet)->EthernetHeader.Type = protocol;
				RESERVED(packet)->RecvTime = CurrentTime();
			
				RtlCopyMemory( &RESERVED(packet)->LpxHeader, lpxHeader, lpxHeaderSize );
				RESERVED(packet)->HeaderCopied = TRUE;

				RESERVED(packet)->PacketRawDataLength = 0;
				RESERVED(packet)->PacketRawDataOffset = 0;

				LpxTransferDataComplete( deviceContext,
					                     packet,
					                     NDIS_STATUS_SUCCESS,
					                     LookAheadBufferSize );

				return NDIS_STATUS_SUCCESS;
			}

		} else if (LookAheadBufferSize - startOffset >= NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)) {
				
			status = RcvPacketAlloc( deviceContext,
									 NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) - lpxHeaderSize,
									 &packet );

			if (status == STATUS_SUCCESS) {
			
				NdisCopyLookaheadData( &RESERVED(packet)->EthernetHeader,
										HeaderBuffer,
										ETHERNET_HEADER_LENGTH,
										deviceContext->MacOptions );

				RESERVED(packet)->EthernetHeader.Type = protocol;
				RESERVED(packet)->RecvTime = CurrentTime();

				RtlCopyMemory( &RESERVED(packet)->LpxHeader, lpxHeader, lpxHeaderSize );
				RESERVED(packet)->HeaderCopied = TRUE;

				RESERVED(packet)->PacketRawDataLength = NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) - lpxHeaderSize;
				RESERVED(packet)->PacketRawDataOffset = 0;

				NdisQueryPacket( packet, NULL, NULL, &firstBuffer, NULL );
				packetData = MmGetMdlVirtualAddress( firstBuffer );

				NdisCopyLookaheadData( packetData,
									   (PBYTE)LookAheadBuffer + startOffset + lpxHeaderSize,
									   RESERVED(packet)->PacketRawDataLength,
									   deviceContext->MacOptions );

				LpxTransferDataComplete( deviceContext,
					                     packet,
					                     NDIS_STATUS_SUCCESS,
					                     LookAheadBufferSize );

				return NDIS_STATUS_SUCCESS;
			}

		} else {

			status = RcvPacketAlloc( deviceContext,
									 startOffset + NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK),
									 &packet );
			
			if (status == STATUS_SUCCESS) {
			
				NdisCopyLookaheadData( &RESERVED(packet)->EthernetHeader,
										HeaderBuffer,
										ETHERNET_HEADER_LENGTH,
										deviceContext->MacOptions );

				RESERVED(packet)->EthernetHeader.Type = protocol;
				RESERVED(packet)->RecvTime = CurrentTime();

				RtlCopyMemory( &RESERVED(packet)->LpxHeader, lpxHeader, lpxHeaderSize );
				RESERVED(packet)->HeaderCopied = TRUE;

				RESERVED(packet)->PacketRawDataLength = startOffset + NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);
				RESERVED(packet)->PacketRawDataOffset = startOffset + lpxHeaderSize;
			}
		}

	} else {

		PLPX_HEADER		lpxHeader;
		PNDIS_BUFFER	firstBuffer;	
		PUCHAR			packetData;
		UINT			packetDataLength;

		ASSERT( FALSE );

		status = RcvPacketAlloc( deviceContext, PacketSize, &packet );
		
		if (status == STATUS_SUCCESS) {
		
			RtlCopyMemory( &RESERVED(packet)->EthernetHeader,
						   HeaderBuffer,
						   ETHERNET_HEADER_LENGTH );

			RESERVED(packet)->EthernetHeader.Type = protocol;
			RESERVED(packet)->RecvTime = CurrentTime();

			RESERVED(packet)->PacketRawDataLength = PacketSize;
			RESERVED(packet)->PacketRawDataOffset = startOffset;

			NdisQueryPacket( packet, NULL, NULL, &firstBuffer, NULL );
			NdisQueryBufferSafe( firstBuffer, &packetData, &packetDataLength, HighPagePriority );

			lpxHeader = (PLPX_HEADER)(packetData + RESERVED(packet)->PacketRawDataOffset);
			RtlZeroMemory( lpxHeader, sizeof(LPX_HEADER) );

			RESERVED(packet)->HeaderCopied = FALSE;
		}
	}

	if (status != NDIS_STATUS_SUCCESS) {
	
		return NDIS_STATUS_NOT_RECOGNIZED;
	}

	ASSERT( packet->Private.NdisPacketFlags & fPACKET_ALLOCATED_BY_NDIS );
			
	if (deviceContext->NdisBindingHandle) {

		//ASSERT( FALSE );

		NdisTransferData( &status,
						  deviceContext->NdisBindingHandle,
						  MacReceiveContext,
						  0, //RESERVED(packet)->PacketRawDataOffset,
						  RESERVED(packet)->PacketRawDataLength,
						  packet,
						  &bytesTransfered );

			
		if (status == NDIS_STATUS_PENDING) {

			LPX_ASSERT( FALSE );
		    status = NDIS_STATUS_SUCCESS;
		
		} else if (status == NDIS_STATUS_SUCCESS) {
		
			LpxTransferDataComplete( deviceContext,
									 packet,
									 status,
									 bytesTransfered );
			
		} else {
	
			LPX_ASSERT( FALSE );
			DebugPrint( 1, ("NdisTransferData() failed. STATUS=%08lx\n", status) );
		}

	} else {
			
		status = NDIS_STATUS_NOT_RECOGNIZED;
		DebugPrint( 1, ("Invalid device status. STATUS=%08lx\n", status) );
	}

	return status;
}


//
//	queues a received packet to InProgressPacketList
//
//	called from LpxReceiveIndication() and NDIS
//

VOID
LpxTransferDataComplete(
	IN NDIS_HANDLE   ProtocolBindingContext,
	IN PNDIS_PACKET  Packet,
	IN NDIS_STATUS   Status,
	IN UINT          BytesTransfered
	)
{
	PDEVICE_CONTEXT	pDeviceContext;
	PLPX_HEADER	lpxHeader;
	PNDIS_BUFFER	firstBuffer;	
	PUCHAR			packetData;
	UINT			packetDataLength;
	USHORT			lpxHeaderSize;


	UNREFERENCED_PARAMETER( BytesTransfered );

	pDeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

	if (Status != NDIS_STATUS_SUCCESS) {

		ASSERT( FALSE );
		DebugPrint( 1,  ("[LPX] LpxTransferDataComplete error %x\n", Status) );
		PacketFree( pDeviceContext, Packet );
		return;
	}

	if (RESERVED(Packet)->HeaderCopied == FALSE) {
	
		NdisQueryPacket( Packet, NULL, NULL, &firstBuffer, NULL );
		NdisQueryBufferSafe( firstBuffer, &packetData, &packetDataLength, HighPagePriority );

		lpxHeader = (PLPX_HEADER)(packetData + RESERVED(Packet)->PacketRawDataOffset);

		lpxHeaderSize = sizeof(LPX_HEADER);

#if __LPX_OPTION_ADDRESSS__

		if (FlagOn(lpxHeader->Option, LPX_OPTION_SOURCE_ADDRESS)) {

			lpxHeaderSize += ETHERNET_ADDRESS_LENGTH;
		}

		if (FlagOn(lpxHeader->Option, LPX_OPTION_DESTINATION_ADDRESS)) {

			lpxHeaderSize += ETHERNET_ADDRESS_LENGTH;
		}

#endif

		RtlCopyMemory( &RESERVED(Packet)->LpxHeader, lpxHeader, lpxHeaderSize );
		RESERVED(Packet)->HeaderCopied = TRUE;

		RESERVED(Packet)->PacketRawDataLength = RESERVED(Packet)->PacketRawDataOffset + NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK);
		RESERVED(Packet)->PacketRawDataOffset += lpxHeaderSize;
	}
	
	lpxHeaderSize = sizeof(LPX_HEADER);

#if __LPX_OPTION_ADDRESSS__

	if (FlagOn(RESERVED(Packet)->LpxHeader.Option, LPX_OPTION_SOURCE_ADDRESS)) {

		if (!FlagOn(RESERVED(Packet)->LpxHeader.Option, LPX_OPTION_DESTINATION_ADDRESS)) {
		
			RtlCopyMemory( RESERVED(Packet)->OptionSourceAddress,
						   RESERVED(Packet)->OptionDestinationAddress,
						   ETHERNET_ADDRESS_LENGTH );	
		}

		lpxHeaderSize += ETHERNET_ADDRESS_LENGTH;

		ASSERT( RtlEqualMemory(RESERVED(Packet)->EthernetHeader.SourceAddress,
							   RESERVED(Packet)->OptionSourceAddress,
							   ETHERNET_ADDRESS_LENGTH) );
	}

	if (FlagOn(RESERVED(Packet)->LpxHeader.Option, LPX_OPTION_DESTINATION_ADDRESS)) {

		lpxHeaderSize += ETHERNET_ADDRESS_LENGTH;

		ASSERT( RtlEqualMemory(RESERVED(Packet)->EthernetHeader.DestinationAddress,
							   RESERVED(Packet)->OptionDestinationAddress,
							   ETHERNET_ADDRESS_LENGTH) );
	}

#endif

	if (NTOHS(RESERVED(Packet)->LpxHeader.PacketSize & ~LPX_TYPE_MASK) - lpxHeaderSize != 
		RESERVED(Packet)->PacketRawDataLength - RESERVED(Packet)->PacketRawDataOffset) {

		ASSERT( FALSE );
		PacketFree( pDeviceContext, Packet );
		return;
	}		

	ExInterlockedInsertTailList( &pDeviceContext->PacketInProgressList,
								 &(RESERVED(Packet)->ListEntry),
								 &pDeviceContext->PacketInProgressQSpinLock );
	return;
}


VOID
DeferredRxCompleteDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

//
// Deferred receive complete routine
//

#if 0

VOID
DeferredRxCompleteDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
){
	PRX_COMP_DPC_CTX     rxCompCtx = DeferredContext;
	PDEVICE_CONTEXT		deviceContext = rxCompCtx->DeviceContext;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	LpxReceiveComplete2(deviceContext, &rxCompCtx->ReceivedPackets);

	ExFreeToNPagedLookasideList(&deviceContext->DeferredRxCompContext, rxCompCtx);

	ACQUIRE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );
	ClearFlag( deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_DEFFERED_DPC_SET );
	RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );

	LpxReceiveComplete( deviceContext );

	LpxDereferenceDeviceContext("RxCompDpc", deviceContext, DCREF_REQUEST);
}

#else

VOID
DeferredLpxReceiveComplete (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
	)
{
	PDEVICE_CONTEXT		deviceContext = DeferredContext;

	UNREFERENCED_PARAMETER( Dpc );
	UNREFERENCED_PARAMETER( SystemArgument1 );
	UNREFERENCED_PARAMETER( SystemArgument2 );

	LpxReceiveComplete2( deviceContext, &deviceContext->DeferredLpxReceiveCompleteDpcPacketList );

	ACQUIRE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );
	deviceContext->DeferredLpxReceiveCompleteDpcRun = FALSE;
	RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );

	LpxReceiveComplete( deviceContext );

	LpxDereferenceDeviceContext( "DeferredLpxReceiveComplete", deviceContext, DCREF_REQUEST );
}

#endif

//
// ProtocolReceiveComplete completes postprocessing of one or more
// preceding receive indications from a NIC driver. 
// Runs at IRQL = DISPATCH_LEVEL.
//

VOID
LpxReceiveComplete (
	IN NDIS_HANDLE BindingContext
	)
{
#if 1

	PDEVICE_CONTEXT		deviceContext = (PDEVICE_CONTEXT)BindingContext;
	BOOLEAN				queued;


	ACQUIRE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );

	IF_LPXDBG(LPX_DEBUG_TEST)
		ASSERT( FALSE );

	if (IsListEmpty(&deviceContext->PacketInProgressList)) {

		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );
		return;
	}

	if (deviceContext->DeferredLpxReceiveCompleteDpcRun == TRUE) {

		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );
		return;
	}

	LPX_ASSERT( IsListEmpty(&deviceContext->DeferredLpxReceiveCompleteDpcPacketList) );

	deviceContext->DeferredLpxReceiveCompleteDpcPacketList.Flink = deviceContext->PacketInProgressList.Flink;
	deviceContext->DeferredLpxReceiveCompleteDpcPacketList.Blink = deviceContext->PacketInProgressList.Blink;
	deviceContext->DeferredLpxReceiveCompleteDpcPacketList.Flink->Blink = &deviceContext->DeferredLpxReceiveCompleteDpcPacketList;
	deviceContext->DeferredLpxReceiveCompleteDpcPacketList.Blink->Flink = &deviceContext->DeferredLpxReceiveCompleteDpcPacketList;

	InitializeListHead( &deviceContext->PacketInProgressList );

	//
	// Perform actual LPX receive completion
	// If the completion is deferred, queue the DPC.
	// Some NIC drivers show receiving performance degrade
	// when receive complete routine consumes much CPU time.
	// Ex> Broadcom Giga ehternet 3788. driver name = b57nd60x.sys 11/1/2005/11:30 164KB
	//

	LpxReferenceDeviceContext( "LpxReceiveComplete", deviceContext, DCREF_REQUEST );

	deviceContext->DeferredLpxReceiveCompleteDpcRun = TRUE;

	queued = KeInsertQueueDpc( &deviceContext->DeferredLpxReceiveCompleteDpc, NULL, NULL );
	
	if (queued) {

	} else {

		LPX_ASSERT( FALSE );
		deviceContext->DeferredLpxReceiveCompleteDpcRun = FALSE;
		LpxDereferenceDeviceContext( "LpxReceiveComplete", deviceContext, DCREF_REQUEST );		
	}

	if (queued == FALSE) {

		deviceContext->DeferredLpxReceiveCompleteDpcRun = TRUE;
	}

	RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );

	if (queued == FALSE) {

		LpxReceiveComplete2( BindingContext, &deviceContext->DeferredLpxReceiveCompleteDpcPacketList );

		ACQUIRE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );
		deviceContext->DeferredLpxReceiveCompleteDpcRun = FALSE;
		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock );
	}

#else

	PDEVICE_CONTEXT		deviceContext = (PDEVICE_CONTEXT)BindingContext;
	PRX_COMP_DPC_CTX	rxCompDpcCtx;
	LIST_ENTRY			receivedPackets;

	ACQUIRE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);

	IF_LPXDBG(LPX_DEBUG_TEST)
		ASSERT( FALSE );

	if (FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_DEFFERED_DPC_SET)) {

		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);
		return;
	}

	if (deviceContext->SendingThreadCount) {

		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);
		return;
	}

	if (IsListEmpty(&deviceContext->PacketInProgressList)) {

		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);
		return;
	}

	//
	// Allocate a DPC context
	//
	rxCompDpcCtx = ExAllocateFromNPagedLookasideList(&deviceContext->DeferredRxCompContext);
	
	if (rxCompDpcCtx == NULL) {

		LPX_ASSERT( FALSE );
		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);
		return;
	}

	//
	// Move received packets from the in-progress list to the temporary list.
	// LpxReceiveComplete2() will process only packets in this temporary list.
	// By doing this, we can spread out time-consume in LpxReceiveComplete2().
	// Especially, deferred LpxReceiveComplete2() in DPC routine does not
	// need to loop and timeout-check because LpxReceiveComplete2() will process
	// only packets in the temporary list.
	//
	if(rxCompDpcCtx) {
		KeInitializeDpc(&rxCompDpcCtx->Dpc, DeferredRxCompleteDpc, rxCompDpcCtx);
		rxCompDpcCtx->DeviceContext = deviceContext;
		rxCompDpcCtx->ReceivedPackets.Flink = deviceContext->PacketInProgressList.Flink;
		rxCompDpcCtx->ReceivedPackets.Blink = deviceContext->PacketInProgressList.Blink;
		deviceContext->PacketInProgressList.Flink->Blink = &rxCompDpcCtx->ReceivedPackets;
		deviceContext->PacketInProgressList.Blink->Flink = &rxCompDpcCtx->ReceivedPackets;
	} else {
		receivedPackets.Flink = deviceContext->PacketInProgressList.Flink;
		receivedPackets.Blink = deviceContext->PacketInProgressList.Blink;
		deviceContext->PacketInProgressList.Flink->Blink = &receivedPackets;
		deviceContext->PacketInProgressList.Blink->Flink = &receivedPackets;
	}
	InitializeListHead(&deviceContext->PacketInProgressList);

	//
	// Perform actual LPX receive completion
	// If the completion is deferred, queue the DPC.
	// Some NIC drivers show receiving performance degrade
	// when receive complete routine consumes much CPU time.
	// Ex> Broadcom Giga ehternet 3788. driver name = b57nd60x.sys 11/1/2005/11:30 164KB
	//

	if(rxCompDpcCtx) {
		BOOLEAN	queued;
		LpxReferenceDeviceContext("RxCompDpc", deviceContext, DCREF_REQUEST);
		queued = KeInsertQueueDpc(&rxCompDpcCtx->Dpc, NULL, NULL);
		ASSERT(queued);
		if(!queued) {
			LpxDereferenceDeviceContext("RxCompDpc", deviceContext, DCREF_REQUEST);
		
		} else {

			SetFlag( deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_DEFFERED_DPC_SET );
		}

		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);

	} else {
	
		RELEASE_DPC_SPIN_LOCK( &deviceContext->PacketInProgressQSpinLock);

		// Call actual LPX receive completion directly.
		LpxReceiveComplete2( BindingContext, &receivedPackets );
	}

#endif
}
