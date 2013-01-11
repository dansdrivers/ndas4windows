#include "precomp.h"
#pragma hdrstop



LONG	NumberOfPackets = 0;
LONG	NumberOfExportedPackets = 0;
LONG	NumberOfCloned = 0;

ULONG	NumberOfSent = 0;
ULONG	NumberOfSentComplete = 0;

PNDIS_PACKET
PacketDequeue(
			  PLIST_ENTRY	PacketQueue,
			  PKSPIN_LOCK	QSpinLock
			  )
{
	PLIST_ENTRY		packetListEntry = NULL;
	PLPX_RESERVED	reserved;
	PNDIS_PACKET	packet;
	
	DebugPrint(4, ("PacketDequeue\n"));
	
	if(QSpinLock) {
		packetListEntry = ExInterlockedRemoveHeadList(
			PacketQueue,
			QSpinLock
			);
		
	} else {
		if(IsListEmpty(PacketQueue))
			packetListEntry = NULL;
		else
			packetListEntry = RemoveHeadList(PacketQueue);
	}
	
	if(packetListEntry) {
		reserved = CONTAINING_RECORD(packetListEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
	} else
		packet = NULL;
	
	return packet;
}

VOID
PacketFree(
		   IN PNDIS_PACKET	Packet
		   )
{
	PLPX_RESERVED	reserved = RESERVED(Packet);
	PUCHAR			packetData;
	PNDIS_BUFFER	pNdisBuffer;
	UINT			uiLength;
	LONG			clone ;

	DebugPrint(3, ("PacketFree reserved->type = %d\n", reserved->Type));

	switch(reserved->Type) {

	case SEND_TYPE:

		clone = InterlockedDecrement(&reserved->Cloned);
		if(clone >= 0) {
			return;
		}
	
		pNdisBuffer = NULL;
		NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		if(pNdisBuffer) {
			
			NdisQueryBufferSafe(
				pNdisBuffer,
				&packetData,
				&uiLength,
				HighPagePriority 
				);
		
			NdisFreeMemory(packetData);
			NdisFreeBuffer(pNdisBuffer);
		}
		pNdisBuffer = NULL;
		NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		while(pNdisBuffer) {
			NdisFreeBuffer(pNdisBuffer);
			pNdisBuffer = NULL;
			NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		}
		if(reserved->IrpSp != NULL) {
            LpxDereferenceSendIrp(reserved->IrpSp);
		} else {
			DebugPrint(2, ("[LPX] PacketFree: No IrpSp\n")) ;
		}
		break;

	case RECEIVE_TYPE:

		if(reserved->LpxSmpHeader)
			//ExFreePool(reserved->LpxSmpHeader);
			NdisFreeMemory(reserved->LpxSmpHeader);

		pNdisBuffer = NULL;	
		NdisUnchainBufferAtFront(Packet, &pNdisBuffer);
		if(pNdisBuffer) {
			NdisQueryBufferSafe(
				pNdisBuffer,
				&packetData,
				&uiLength,
				HighPagePriority 
				);
			
			NdisFreeMemory(packetData);
			NdisFreeBuffer(pNdisBuffer);
		}
		reserved->PacketDataOffset = 0;

		break;

	}

	NdisFreePacket(Packet);
	
	InterlockedDecrement(&NumberOfPackets);

	DebugPrint(2, ("Packet REALLY Freed Numberofpackets = %d\n", NumberOfPackets));
}

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
			   )
{
	NTSTATUS		status;
	PUCHAR			packetData;
	PNDIS_BUFFER	pNdisBuffer;
	PNDIS_BUFFER	pNdisBufferData;
	PNDIS_PACKET	packet;
	USHORT			port;
	
	DebugPrint(3, ("PacketAllocate, PacketLength = %d, Numberofpackets = %d\n", PacketLength, NumberOfPackets));
	
	//	if(ServicePoint && ServicePoint->SmpState == SMP_SYN_RECV)
	//		return STATUS_INSUFFICIENT_RESOURCES;
	
	if(DeviceContext == NULL) {
		DebugPrint(1, ("[LPX]PacketAllocate: DeviceContext is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	
	if(DeviceContext->LpxPacketPool == NULL) {
		DebugPrint(1, ("[LPX]PacketAllocate: DeviceContext->LpxPacketPool is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	
	NdisAllocatePacket(&status,	&packet, DeviceContext->LpxPacketPool);
	
	if(status != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("[LPX]PacketAllocate: NdisAllocatePacket Failed!!!\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	status = NdisAllocateMemory(
		&packetData,
		PacketLength
		);
	if(status != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("[LpxSmp]PacketAllocate: Can't Allocate Memory packet.\n"));
		
		NdisFreePacket(packet);
		*Packet = NULL;
		
		return status;
	}
	
	NdisAllocateBuffer(
		&status,
		&pNdisBuffer,
		DeviceContext->LpxBufferPool,
		packetData,
		PacketLength
		);
	if(!NT_SUCCESS(status)) {
		NdisFreePacket(packet);
		*Packet = NULL;
		NdisFreeMemory(packetData);
		DebugPrint(1, ("[LPX]PacketAllocate: Can't Allocate Buffer!!!\n"));
		
		return status;
	}
	
	switch(Type) {
		
	case SEND_TYPE:

		if(ServicePoint && &ServicePoint->SmpContext) {
			RtlCopyMemory(&packetData[0],
				ServicePoint->DestinationAddress.Node,
				ETHERNET_ADDRESS_LENGTH
				);
			RtlCopyMemory(&packetData[ETHERNET_ADDRESS_LENGTH],
				ServicePoint->SourceAddress.Node,
				ETHERNET_ADDRESS_LENGTH
				);
			port = HTONS(ETH_P_LPX);
			RtlCopyMemory(&packetData[ETHERNET_ADDRESS_LENGTH*2],
				&port, //&ServicePoint->DestinationAddress.Port,
				2
				);
		}
		
		if(CopyDataLength) {
			
			NdisAllocateBuffer(
				&status,
				&pNdisBufferData,
				DeviceContext->LpxBufferPool,
				CopyData,
				CopyDataLength
				);
			if(!NT_SUCCESS(status)) {
				NdisFreePacket(packet);
				*Packet = NULL;
				NdisFreeMemory(packetData);
				DebugPrint(1, ("[LPX]PacketAllocate: Can't Allocate Buffer For CopyData!!!\n"));
				
				return status;
			}
			
			NdisChainBufferAtFront(packet, pNdisBufferData);
		}
		break;
		
	case RECEIVE_TYPE:
		
		NdisMoveMappedMemory(
			packetData,
			CopyData,
			CopyDataLength
			);
		
		break;
	}
		
	//	RESERVED(packet)->ServicePoint = ServicePoint;
	RESERVED(packet)->Cloned = 0;
	RESERVED(packet)->IrpSp = IrpSp;
	RESERVED(packet)->Type = Type;
	RESERVED(packet)->LpxSmpHeader = NULL;
	
	if(IrpSp == NULL) {
		DebugPrint(2, ("[LPX] PacketAllocate: No IrpSp\n")) ;
	}

	NdisChainBufferAtFront(packet, pNdisBuffer);
	
	InterlockedIncrement(&NumberOfPackets);

	*Packet = packet;
	return STATUS_SUCCESS;
}

PNDIS_PACKET
PacketClone(
	IN	PNDIS_PACKET Packet
	)
{
	InterlockedIncrement(&(RESERVED(Packet)->Cloned));
	InterlockedIncrement(&NumberOfCloned);

	return Packet;
}

PNDIS_PACKET
PacketCopy(
	IN	PNDIS_PACKET Packet,
	OUT	PLONG	Cloned
	)
{
	ASSERT(Cloned) ;

	*Cloned = InterlockedIncrement(&(RESERVED(Packet)->Cloned));
	InterlockedIncrement(&NumberOfCloned);

	return Packet;
}


BOOLEAN
PacketQueueEmpty(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	)
{
	PLIST_ENTRY		packetListEntry;
	KIRQL			oldIrql;

	if(QSpinLock) {
		ACQUIRE_SPIN_LOCK(QSpinLock, &oldIrql);
		packetListEntry = PacketQueue->Flink;
		RELEASE_SPIN_LOCK(QSpinLock, oldIrql);
	}else
		packetListEntry = PacketQueue->Flink;

	return (packetListEntry == PacketQueue);
}

PNDIS_PACKET
PacketPeek(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	)	
{
	PLIST_ENTRY		packetListEntry;
	PLPX_RESERVED	reserved;
	PNDIS_PACKET	packet;
	KIRQL			oldIrql;

	if(QSpinLock) {
		KeAcquireSpinLock(QSpinLock, &oldIrql);
		packetListEntry = PacketQueue->Flink;
		KeReleaseSpinLock(QSpinLock, oldIrql);
	}else
		packetListEntry = PacketQueue->Flink;

	if(packetListEntry == PacketQueue)
		packetListEntry = NULL;

	if(packetListEntry) {
		reserved = CONTAINING_RECORD(packetListEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
	} else
		packet = NULL;

	return packet;
}



