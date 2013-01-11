#include "AfdTcpProc.h"
//#include "public.h"

LONG	AfdDebugLevel = 1; 


/*
 typedef struct _AFD_OPEN_PACKET {
	AFD_ENDPOINT_FLAGS __f;
#define afdConnectionLess  __f.ConnectionLess
#define afdMessageMode     __f.MessageMode
#define afdRaw             __f.Raw
#define afdMultipoint      __f.Multipoint
#define afdC_Root          __f.C_Root
#define afdD_Root          __f.D_Root
#define afdEndpointFlags   __f.EndpointFlags
    LONG  GroupID;
    ULONG TransportDeviceNameLength;
    WCHAR TransportDeviceName[1];
} AFD_OPEN_PACKET, *PAFD_OPEN_PACKET;




#define	AFDTCPCREATE_EA_BUFFER_LENGTH_IN (									\
					FIELD_OFFSET(FILE_FULL_EA_INFORMATION, Address)			\
					+ AFD_OPEN_PACKET_NAME_LENGTH + 1 						\
					+ FILED_OFFSET(AFD_OPEN_PACKET, TransportDeviceName)	\
					+ TCPTRANSPORT_NAME_LENGTH + 2							\
					)

 */



//
//	Maybe Create socket (buf only generate entry - or SAP)
//


NTSTATUS
AfdTcpCreateSocket(
		OUT PHANDLE			SocketFileHandle,
		OUT PFILE_OBJECT	*SocketFileObject,
		IN	ULONG			SocketType
		)
{
	HANDLE						socketFileHandle;
	PFILE_OBJECT				socketFileObject;
	UNICODE_STRING				nameString;
	OBJECT_ATTRIBUTES			objectAttributes;
	UCHAR						eaFullBuffer[AFDTCPCREATE_EA_BUFFER_LENGTH_IN];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	PAFD_OPEN_PACKET			afdOpenPacket;
	PTRANSPORT_ADDRESS			transportAddress;
    PTA_ADDRESS					taAddress;
	PTDI_ADDRESS_IP				TcpAddress;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;

	AfdTcpDebugPrint (1, ("[TcpTdi] AfdTcpCreateSocket:  Entered\n"));

	//
	// Init object attributes
	//	
    RtlInitUnicodeString (&nameString, AFD_DEVICE_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	//
	//	Init Input Parameter
	// 
	RtlZeroMemory(eaBuffer,AFDTCPCREATE_EA_BUFFER_LENGTH_IN);
	eaBuffer->NextEntryOffset	= 0;
    eaBuffer->Flags				= 0;
	eaBuffer->EaNameLength = AFD_OPEN_PACKET_NAME_LENGTH;
	eaBuffer->EaValueLength =  FIELD_OFFSET(AFD_OPEN_PACKET, TransportDeviceName)
								+ (TCPTRANSPORT_NAME_LENGTH + 2);
	
    for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
        eaBuffer->EaName[i] = (char)AfdOpenPacket[i];
    }

	afdOpenPacket = (PAFD_OPEN_PACKET)&eaBuffer->EaName[eaBuffer->EaNameLength+1];
	if(SocketType == TCP_STREAM){
		afdOpenPacket->__f.EndpointFlags = AfdEndpointTypeStream;
	}else{
		afdOpenPacket->__f.EndpointFlags = AfdEndpointTypeDatagram;
	}
	afdOpenPacket->GroupID = 0;
	afdOpenPacket->TransportDeviceNameLength = TCPTRANSPORT_NAME_LENGTH;
	
	for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
        afdOpenPacket->TransportDeviceName[i] = (WCHAR)TCPTRANSPORT_NAME[i];
    }

	//
	// Open Socket File
	//
	status = ZwCreateFile(
				&socketFileHandle,
				GENERIC_READ | GENERIC_WRITE,
				&objectAttributes,
				&ioStatusBlock,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_WRITE,//0,
				FILE_CREATE,//0,
				0,
				eaBuffer,
				AFDTCPCREATE_EA_BUFFER_LENGTH_IN 
				);


	if (!NT_SUCCESS(status)) {
	    AfdTcpDebugPrint (1,("AfdTcpCreateSocket:  FAILURE, NtCreateFile returned status code=%x.\n", status));
		*SocketFileHandle = NULL;
		*SocketFileObject = NULL;
		return status;
	}

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        AfdTcpDebugPrint (0, ("AfdTcpCreateSocket:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*SocketFileHandle = NULL;
		*SocketFileObject = NULL;
		return status;
    }
    status = ObReferenceObjectByHandle (
                socketFileHandle,
                0L,
                NULL,
                KernelMode,
                (PVOID *) &socketFileObject,
                NULL
				);

    if (!NT_SUCCESS(status)) {
        AfdTcpDebugPrint(0,("\n****** AfdTcpCreateSocket:  FAILED get socketFileObject: %x ******\n", status));
		ZwClose(socketFileHandle);
		*SocketFileHandle = NULL;
		*SocketFileObject = NULL;
        return status;
    }
    
	*SocketFileHandle = socketFileHandle;
	*SocketFileObject = socketFileObject;

	AfdTcpDebugPrint (1, ("AfdTcpCreateSocket returning\n"));
	
	return status;

}



//
//	Maybe Close socket (but on close entry or - SAP)
//

NTSTATUS
AfdTcpCloseSocket(
	IN HANDLE			SocketFileHandle,
	IN PFILE_OBJECT		SocketFileObject
	)
{
    NTSTATUS status;

	if(SocketFileObject)
		ObDereferenceObject(SocketFileObject);

	if(SocketFileHandle == NULL)
		return STATUS_SUCCESS;

    status = ZwClose (SocketFileHandle);

    if (!NT_SUCCESS(status)) {
        AfdTcpDebugPrint (1, ("AfdTcpCloseSocket:  FAILURE, NtClose returned status code=%x\n", status));
    } else {
        AfdTcpDebugPrint (1, ("AfdTcpCloseSocket:  NT_SUCCESS.\n"));
    }

    return status;
} // AfdTcpCloseSocket




NTSTATUS
AfdTcpIoCallDriver(
    IN		PDEVICE_OBJECT		DeviceObject,
    IN OUT	PIRP				Irp,
	IN		PIO_STATUS_BLOCK	IoStatusBlock,
	IN		PKEVENT				Event
    )
{
	NTSTATUS		ntStatus;
	NTSTATUS		wait_status;
	LARGE_INTEGER	AfdTimeout;

	ntStatus = IoCallDriver(
				DeviceObject,
				Irp
				);

	if(ntStatus == STATUS_PENDING) {

		AfdTimeout.QuadPart  = - AFD_TIME_OUT;

		wait_status = KeWaitForSingleObject(
					Event,
					Executive,
					KernelMode,
					FALSE,
					&AfdTimeout
//					NULL
					);

		if(wait_status != STATUS_SUCCESS) {
			AfdTcpDebugPrint(1, ("AfdTcpIoCallDriver: Wait for event Failed.\n"));
			return STATUS_CONNECTION_DISCONNECTED; // STATUS_TIMEOUT;
		}
	}

    ntStatus = IoStatusBlock->Status;

	return ntStatus;
} 



/*
#define AFDTCPBIND_INFO_LENGTH_IN	(									\
					FIELD_OFFSET(AFD_BIND_INFO, EaName)						\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

#define AFDTCPBIND_INFO_LENGTH_OUT	(
					sizeof(TDI_ADDRESS_INFO)								\
					+ TDI_TRANSPORT_ADDRESS_LENGTH 							\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

*/


//
// Bind call --> Set local address information
//
NTSTATUS
AfdTcpBindSocket(
	IN 	PFILE_OBJECT		SocketFileObject,
	IN	PTDI_ADDRESS_IP		Address,
	IN  ULONG				AddressType,
	OUT PTDI_ADDRESS_IP		RetAddress
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;

	UCHAR					InBuffer[AFDTCPBIND_INFO_LENGTH_IN];
	UCHAR					OutBuffer[AFDTCPBIND_INFO_LENGTH_OUT];
	PAFD_BIND_INFO			pBindInfo;
	PTDI_ADDRESS_INFO		pTdiAddressInfo;
	PTA_ADDRESS				pAdress;
	PTDI_ADDRESS_IP			pIPAddress;
	PTRANSPORT_ADDRESS		pTransPortAddress;
	PIO_STACK_LOCATION		IrpSp;


	AfdTcpDebugPrint (1, ("AfdTcpBindSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	// Setup InPut Parameter
	//
	RtlZeroMemory(InBuffer,AFDTCPBIND_INFO_LENGTH_IN);
	pBindInfo = (PAFD_BIND_INFO)InBuffer;
	pBindInfo->ShareAccess = AddressType;
	pTransPortAddress = (PTRANSPORT_ADDRESS)&(pBindInfo->Address);
	pTransPortAddress->TAAddressCount = 1;
	pAdress = (PTA_ADDRESS)pTransPortAddress->Address;
    pAdress->AddressType = TDI_ADDRESS_TYPE_IP;
    pAdress->AddressLength = TDI_ADDRESS_LENGTH_IP;

    pIPAddress = (PTDI_ADDRESS_IP)pAdress->Address;

	RtlCopyMemory(
		pIPAddress,
		Address,
		sizeof(TDI_ADDRESS_IP)
		);

	//
	//	Build Irp
	//

	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_BIND,
				deviceObject,
				InBuffer,
				AFDTCPBIND_INFO_LENGTH_IN,
				OutBuffer,
				AFDTCPBIND_INFO_LENGTH_OUT,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpBindSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	// Set Device Object and File object
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   


	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpBindSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpBindSocket: Failed.\n"));
	}

	//
	//	Copy Result 
	//
	
	pTdiAddressInfo = (PTDI_ADDRESS_INFO)OutBuffer;
	AfdTcpDebugPrint(1,("Active Address Count %d\n",pTdiAddressInfo->ActivityCount));
    pTransPortAddress = (PTRANSPORT_ADDRESS)&(pTdiAddressInfo->Address);
	pAdress = (PTA_ADDRESS)pTransPortAddress->Address;
	pIPAddress = (PTDI_ADDRESS_IP)pAdress->Address;
	RtlCopyMemory(
		RetAddress,
		pIPAddress,
		TDI_ADDRESS_LENGTH_IP
		);

	return ntStatus;
}





/*
#define AFDTCPCONNECT_INFO_LENGTH_IN (								\
					FIELD_OFFSET(AFD_CONNECT_JOIN_INFO, RemoteAddress)		\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

#define AFDTCPCONNECT_INFO_LENGTH_OUT (								\
					sizeof(IO_STATUS_BLOCK)									\
					)
*/




/*
 typedef struct _AFD_CONNECT_JOIN_INFO {
    BOOLEAN     SanActive;
    HANDLE  RootEndpoint;       // Root endpoint for joins
    HANDLE  ConnectEndpoint;    // Connect/leaf endpoint for async connects
    TRANSPORT_ADDRESS   RemoteAddress; // Remote address
} AFD_CONNECT_JOIN_INFO, *PAFD_CONNECT_JOIN_INFO;	
 */


//
//	Connect Call
//
NTSTATUS
AfdTcpConnectSocket(
	IN  HANDLE				SocketFileHandle,
	IN	PFILE_OBJECT		SocketFileObject,
	IN	PTDI_ADDRESS_IP		Address					
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPCONNECT_INFO_LENGTH_IN];
	
	PAFD_CONNECT_JOIN_INFO		pAfdConnectJoinInfo;
	PTA_ADDRESS					pAdress;
	PTDI_ADDRESS_IP				pIPAddress;
	PTRANSPORT_ADDRESS			pTransPortAddress;
	IO_STATUS_BLOCK				OutBuffer;


	AfdTcpDebugPrint (1, ("AfdTcpConnectSocket:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);



	//
	//	Setup Input Parameter
	//
	RtlZeroMemory(InBuffer,AFDTCPCONNECT_INFO_LENGTH_IN);
	pAfdConnectJoinInfo = (PAFD_CONNECT_JOIN_INFO)InBuffer;
	pAfdConnectJoinInfo->SanActive = TRUE;
	pAfdConnectJoinInfo->ConnectEndpoint = SocketFileHandle;
	pTransPortAddress = (PTRANSPORT_ADDRESS)&(pAfdConnectJoinInfo->RemoteAddress);
	pTransPortAddress->TAAddressCount = 1;
	pAdress = (PTA_ADDRESS)pTransPortAddress->Address;
    pAdress->AddressType = TDI_ADDRESS_TYPE_IP;
    pAdress->AddressLength = TDI_ADDRESS_LENGTH_IP;

    pIPAddress = (PTDI_ADDRESS_IP)pAdress->Address;

	RtlCopyMemory(
		pIPAddress,
		Address,
		sizeof(TDI_ADDRESS_IP)
		);	


  	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_CONNECT,
				deviceObject,
				InBuffer,
				AFDTCPCONNECT_INFO_LENGTH_IN,
				&OutBuffer,
				sizeof(IO_STATUS_BLOCK),
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpConnectSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;

	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   

	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpConnectSocket Error code 0x%x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpConnectSocket: Failed.\n"));
	}
	
//	AfdTcpDebugPrint(1,("AfdTcpConnectSocket : Status %x", OutBuffer.Status));

//	return OutBuffer.Status;
	return ntStatus;
}


/*
 // Disconnect
#define AFDTCPDISCONNECT_INFO_LENGTH_IN (									\
					sizeof(AFD_PARTIAL_DISCONNECT_INFO)						\
					)

 */
NTSTATUS
AfdTcpDisConnectSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  ULONG				SocketType		
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPDISCONNECT_INFO_LENGTH_IN];
	PAFD_PARTIAL_DISCONNECT_INFO	pAfdPartialDisconnectInfo;


	AfdTcpDebugPrint (1, ("AfdTcpDisConnectSocket:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);


	//
	//	Setup Input Parameter
	//
	RtlZeroMemory(InBuffer,AFDTCPDISCONNECT_INFO_LENGTH_IN);
	pAfdPartialDisconnectInfo = (PAFD_PARTIAL_DISCONNECT_INFO)InBuffer;
	if(SocketType == TCP_STREAM){
		pAfdPartialDisconnectInfo->DisconnectMode = AFD_ABORTIVE_DISCONNECT;
	}else{
		pAfdPartialDisconnectInfo->DisconnectMode = AFD_UNCONNECT_DATAGRAM;
	}


	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_PARTIAL_DISCONNECT,
				deviceObject,
				InBuffer,
				AFDTCPDISCONNECT_INFO_LENGTH_IN,
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);

	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpDisConnectSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   



	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpDisConnectSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpDisConnectSocket: Failed.\n"));
	}

	return ntStatus;

}

/*
// Stat for listen
#define AFDTCPLISTEN_START_LISTEN_INFO_IN	(								\
					sizeof(AFD_LISTEN_INFO)									\
					)
*/

NTSTATUS
AfdTcpListenStartSocket(
	IN	PFILE_OBJECT		SocketFileObject				
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPLISTEN_START_LISTEN_INFO_IN];
	PAFD_LISTEN_INFO			pListenInfo;

	
	AfdTcpDebugPrint (1, ("AfdTcpListenStartSocket:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	//	Setup Input Parameter
	//
	RtlZeroMemory(InBuffer,AFDTCPLISTEN_START_LISTEN_INFO_IN);

	pListenInfo = (PAFD_LISTEN_INFO)InBuffer;
	pListenInfo->SanActive = FALSE;
	pListenInfo->MaximumConnectionQueue = 5;
	pListenInfo->UseDelayedAcceptance = TRUE;


  	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_START_LISTEN,
				deviceObject,
				InBuffer,
				AFDTCPLISTEN_START_LISTEN_INFO_IN,
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpListenStartSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	// Set Device Object and File object
	//
//	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   



	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpListenStartSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpListenStartSocket: Failed.\n"));
	}

	return ntStatus;
}


/*
// Wait for listen			
#define AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT_ELEMENT (						\
					FIELD_OFFSET(AFD_LISTEN_RESPONSE_INFO, RemoteAddress)	\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)

#define AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT (								\
					(AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT_ELEMENT) * 5       \
					)
*/


NTSTATUS
AfdTcpListenWaitSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	OUT	PTDI_ADDRESS_IP		Address,
	OUT PULONG				pSequence
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						OutBuffer[AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT_ELEMENT];	
	
	PAFD_LISTEN_RESPONSE_INFO	pAfdListenResponseInfo;
	PTA_ADDRESS					pAdress;
	PTDI_ADDRESS_IP				pIPAddress;
	PTRANSPORT_ADDRESS			pTransPortAddress;

	AfdTcpDebugPrint (1, ("AfdTcpListenWaitSocket:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);


  	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_WAIT_FOR_LISTEN,
				deviceObject,
				NULL,
				0,
				OutBuffer,
				AFDTCPLISTEN_WAIT_RESPONSE_INFO_OUT_ELEMENT,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpListenWaitSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   



	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpListenWaitSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpListenWaitSocket: Failed.\n"));
	}


//
//	Process Output
//
	pAfdListenResponseInfo = (PAFD_LISTEN_RESPONSE_INFO)OutBuffer;
	AfdTcpDebugPrint(1,("AfdTcpListenWaitSocket Sequence %d", pAfdListenResponseInfo->Sequence));
    pTransPortAddress = (PTRANSPORT_ADDRESS)&(pAfdListenResponseInfo->RemoteAddress);
	pAdress = (PTA_ADDRESS)pTransPortAddress->Address;
	pIPAddress = (PTDI_ADDRESS_IP)pAdress->Address;
	RtlCopyMemory(
		Address,
		pIPAddress,
		TDI_ADDRESS_LENGTH_IP
		);
	*pSequence = pAfdListenResponseInfo->Sequence;

	return ntStatus;	
}

/*
#define AFDTCPACCEPT_IN	(												\
					sizeof(AFD_ACCEPT_INFO)									\
					)

*/

NTSTATUS
AfdTcpAcceptSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN	HANDLE				AcceptFileHanlde,
	IN  ULONG				Sequence
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPACCEPT_IN];
	PAFD_ACCEPT_INFO			pAfdAcceptInfo;


	AfdTcpDebugPrint (1, ("AfdTcpAcceptSocket:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	//	Setup Input Parameter
	//

	RtlZeroMemory(InBuffer,AFDTCPACCEPT_IN);
	pAfdAcceptInfo = (PAFD_ACCEPT_INFO)InBuffer;
	pAfdAcceptInfo->SanActive = TRUE;
	pAfdAcceptInfo->AcceptHandle = AcceptFileHanlde;
	pAfdAcceptInfo->Sequence = Sequence;


 	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_ACCEPT,
				deviceObject,
				InBuffer,
				AFDTCPACCEPT_IN,
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpAcceptSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpAcceptSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpAcceptSocket: Failed.\n"));
	}

	return ntStatus;

}
/*
#define AFDTCPREAD_IN	(													\
					sizeof(AFD_RECV_INFO)									\
					)
*/

/*
 *		SocketFileObject --> this File Object of Connected EndPoint
 */
NTSTATUS
AfdTcpRecvSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	OUT PULONG				TotalRecved
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPREAD_IN];
	PAFD_RECV_INFO				pAfdRecvInfo;
	
	
	AfdTcpDebugPrint (2, ("AfdTcpRecvSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	//	Setup Input Parameter
	//

	RtlZeroMemory(InBuffer,AFDTCPREAD_IN);
	pAfdRecvInfo = (PAFD_RECV_INFO)InBuffer;
	pAfdRecvInfo->BufferCount = bufferCount;
	pAfdRecvInfo->BufferArray = bufferArray;
	pAfdRecvInfo->AfdFlags = AFD_OVERLAPPED;
	pAfdRecvInfo->TdiFlags = TDI_RECEIVE_NORMAL ;

	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_RECEIVE,
				deviceObject,
				InBuffer,
				AFDTCPREAD_IN,
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpRecvSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpRecvSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpRecvSocket: Failed.\n"));
	}

	*TotalRecved = ioStatusBlock.Information;
	return ntStatus;

}


/*
 #define AFDTCPWRITE_IN  (													\
					sizeof(AFD_SEND_INFO)									\
					)
 */

NTSTATUS
AfdTcpSendSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	OUT PULONG				TotalSent
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPWRITE_IN];
	PAFD_SEND_INFO				pAfdSendInfo;
	
	
	AfdTcpDebugPrint (2, ("AfdTcpSendSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	//	Setup Input Parameter
	//

	RtlZeroMemory(InBuffer,AFDTCPWRITE_IN);
	pAfdSendInfo = (PAFD_SEND_INFO)InBuffer;
	pAfdSendInfo->BufferCount = bufferCount;
	pAfdSendInfo->BufferArray = bufferArray;
	pAfdSendInfo->AfdFlags = AFD_OVERLAPPED ;
	pAfdSendInfo->TdiFlags = 0;

	//
	//	Build Irp
	//
	ioStatusBlock.Information = 0;
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_SEND,
				deviceObject,
				InBuffer,
				AFDTCPWRITE_IN,
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpSendSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Set Device Object and File object
	//

	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpSendSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpSendSocket: Failed.\n"));
	}

	*TotalSent = ioStatusBlock.Information;
	return ntStatus;	
}

/*
#define AFDTCP_REMOTEADDRESS	(											\
					FIELD_OFFSET(TRANSPORT_ADDRESS, Address)				\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\		
					)


#define AFDTCPRECVDATAGRAM_IN	(											\
					sizeof(AFD_RECV_DATAGRAM_INFO)							\
					+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)				\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)
*/

NTSTATUS
AfdTcpRecvDataGramSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	IN	PTDI_ADDRESS_IP		Address,	
	OUT PULONG				TotalRecved
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPRECVDATAGRAM_IN];
	PAFD_RECV_DATAGRAM_INFO		pAfdRecvDataGramInfo;
	
	PTA_ADDRESS					pAdress;
	PTDI_ADDRESS_IP				pIPAddress;
	PTRANSPORT_ADDRESS			pTransPortAddress;
	
	
	ULONG						TransPortAddressLen;
	AfdTcpDebugPrint (1, ("AfdTcpRecvDataGramSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	//	Setup Input Parameter
	//

	RtlZeroMemory(InBuffer,AFDTCPRECVDATAGRAM_IN);
	pAfdRecvDataGramInfo = (PAFD_RECV_DATAGRAM_INFO)InBuffer;
	pAfdRecvDataGramInfo->BufferArray = bufferArray;
	pAfdRecvDataGramInfo->BufferCount = bufferCount;
	pAfdRecvDataGramInfo->TdiFlags = TDI_RECEIVE_NORMAL;
	pAfdRecvDataGramInfo->AfdFlags = AFD_OVERLAPPED;
	
	pTransPortAddress = (PTRANSPORT_ADDRESS)((char *)InBuffer + sizeof(AFD_RECV_DATAGRAM_INFO));
	pTransPortAddress->TAAddressCount = 1;
	pAdress = (PTA_ADDRESS)pTransPortAddress->Address;
    pAdress->AddressType = TDI_ADDRESS_TYPE_IP;
    pAdress->AddressLength = TDI_ADDRESS_LENGTH_IP;
    pIPAddress = (PTDI_ADDRESS_IP)pAdress->Address;
	
	RtlCopyMemory(
		pIPAddress,
		Address,
		sizeof(TDI_ADDRESS_IP)
		);	

	pAfdRecvDataGramInfo->Address = pTransPortAddress;
	TransPortAddressLen = AFDTCP_REMOTEADDRESS;
	pAfdRecvDataGramInfo->AddressLength = &TransPortAddressLen;

	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_RECEIVE_DATAGRAM,
				deviceObject,
				InBuffer,
				sizeof(AFD_RECV_DATAGRAM_INFO),
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpRecvDataGramSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpRecvDataGramSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpRecvDataGramSocket: Failed.\n"));
	}

	*TotalRecved = ioStatusBlock.Information;
	return ntStatus;	
	
}

/*
// SendDataGram
#define AFDTCPWRITEDATAGRAM_IN	(											\
					sizeof(AFD_SEND_DATAGRAM_INFO)							\
					+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)				\
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1						\
					+ FIELD_OFFSET(TA_ADDRESS, Address)						\
					+ TDI_ADDRESS_LENGTH_IP									\
					)
 */



NTSTATUS
AfdTcpSendDataGramSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  LPWSABUF			bufferArray,
	IN  ULONG				bufferCount,
	IN	PTDI_ADDRESS_IP		Address,	
	OUT PULONG				TotalSent
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	UCHAR						InBuffer[AFDTCPWRITEDATAGRAM_IN];
	PAFD_SEND_DATAGRAM_INFO		pAfdSendDataGramInfo;
	
	PTA_ADDRESS					pAdress;
	PTDI_ADDRESS_IP				pIPAddress;
	PTRANSPORT_ADDRESS			pTransPortAddress;	

	AfdTcpDebugPrint (1, ("AfdTcpSendDataGramSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);

	//
	//	Setup Input Parameter
	//

	RtlZeroMemory(InBuffer,AFDTCPWRITEDATAGRAM_IN);
	pAfdSendDataGramInfo = (PAFD_SEND_DATAGRAM_INFO)InBuffer;
	pAfdSendDataGramInfo->AfdFlags = AFD_OVERLAPPED;
	pAfdSendDataGramInfo->BufferArray = bufferArray;
	pAfdSendDataGramInfo->BufferCount = bufferCount;
	pAfdSendDataGramInfo->TdiRequest.SendDatagramInformation = &(pAfdSendDataGramInfo->TdiConnInfo);
	pAfdSendDataGramInfo->TdiConnInfo.UserDataLength = 0;
	pAfdSendDataGramInfo->TdiConnInfo.UserData = NULL;
	pAfdSendDataGramInfo->TdiConnInfo.OptionsLength = 0;
	pAfdSendDataGramInfo->TdiConnInfo.Options = NULL;
	
	pTransPortAddress = (PTRANSPORT_ADDRESS)((char *)InBuffer + sizeof(PAFD_SEND_DATAGRAM_INFO));
	pTransPortAddress->TAAddressCount = 1;
	pAdress = (PTA_ADDRESS)pTransPortAddress->Address;
    pAdress->AddressType = TDI_ADDRESS_TYPE_IP;
    pAdress->AddressLength = TDI_ADDRESS_LENGTH_IP;
    pIPAddress = (PTDI_ADDRESS_IP)pAdress->Address;
	
	RtlCopyMemory(
		pIPAddress,
		Address,
		sizeof(TDI_ADDRESS_IP)
		);	

	pAfdSendDataGramInfo->TdiConnInfo.RemoteAddress = pTransPortAddress;
	pAfdSendDataGramInfo->TdiConnInfo.RemoteAddressLength = AFDTCP_REMOTEADDRESS;
	
	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_SEND_DATAGRAM,
				deviceObject,
				InBuffer,
				sizeof(PAFD_SEND_DATAGRAM_INFO),
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpSendDataGramSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpSendDataGramSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpSendDataGramSocket: Failed.\n"));
	}

	*TotalSent = ioStatusBlock.Information;
	return ntStatus;		

}


/*
// SetInformation
#define AFDTCPSETINFORMATION_IN	(											\
					sizeof(AFD_INFORMATION)									\
					)

*/


NTSTATUS
AfdTcpSetInformationSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN  PAFD_INFORMATION	AfdInformation
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;

	AfdTcpDebugPrint (1, ("AfdTcpSetInformationSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);


	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_SET_INFORMATION,
				deviceObject,
				AfdInformation,
				sizeof(AFD_INFORMATION),
				NULL,
				0,
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpSetInformationSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpSetInformationSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpSetInformationSocket: Failed.\n"));
	}

	return ntStatus;		
}


/*
 // QueryInformation
#define AFDTCPQUERYINFORMATION_IN (											\
					sizeof(AFD_INFORMATION)									\
					)

*/
NTSTATUS
AfdTcpGetInformationSocket(
	IN	PFILE_OBJECT		SocketFileObject,
	IN OUT  PAFD_INFORMATION	AfdInformation
	)
{
	NTSTATUS					ntStatus;
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;
	PIO_STACK_LOCATION			IrpSp;
	AFD_INFORMATION				OutAfdInformation;

	AfdTcpDebugPrint (1, ("AfdTcpGetInformationSocket:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(SocketFileObject);


	//
	//	Build Irp
	//
	
	irp = IoBuildDeviceIoControlRequest(
				IOCTL_AFD_SET_INFORMATION,
				deviceObject,
				AfdInformation,
				sizeof(AFD_INFORMATION),
				(char *)&OutAfdInformation,
				sizeof(AFD_INFORMATION),
				FALSE,
				&event,
				&ioStatusBlock
				);
	
	if(irp == NULL) {
		AfdTcpDebugPrint(1, ("AfdTcpGetInformationSocket: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Set Device Object and File object
	//
	irp->RequestorMode = KernelMode;
	IrpSp = IoGetNextIrpStackLocation( irp );
	IrpSp->DeviceObject = deviceObject;
	IrpSp->FileObject = SocketFileObject;   
	
	
	ntStatus = AfdTcpIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);



	if(!NT_SUCCESS(ntStatus)) {
		AfdTcpDebugPrint(1,("AfdTcpGetInformationSocket %x\n", ntStatus));
		AfdTcpDebugPrint(1, ("AfdTcpGetInformationSocket: Failed.\n"));
	}

	RtlCopyMemory((char *)AfdInformation, (char *)&OutAfdInformation, sizeof(AFD_INFORMATION));

	return ntStatus;		
}