#include "kernel/TcpTdiProc.h"

LONG	TCPDebugLevel = 1;


NTSTATUS
TcpTdiOpenAddress(
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject,
	IN	PTDI_ADDRESS_IP		Address
	)
{
	HANDLE						addressFileHandle; 
	PFILE_OBJECT				addressFileObject;

    UNICODE_STRING				nameString;
    OBJECT_ATTRIBUTES			objectAttributes;
	UCHAR						eaFullBuffer[TCPADDRESS_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	PTRANSPORT_ADDRESS			transportAddress;
    PTA_ADDRESS					taAddress;
	PTDI_ADDRESS_IP				TcpAddress;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;

    TCPLtDebugPrint (3, ("[TcpTdi] TdiOpenAddress:  Entered\n"));

	//
	// Init object attributes
	//

    RtlInitUnicodeString (&nameString, TCPTRANSPORT_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	RtlZeroMemory(eaBuffer, TCPADDRESS_EA_BUFFER_LENGTH);
    eaBuffer->NextEntryOffset	= 0;
    eaBuffer->Flags				= 0;
    eaBuffer->EaNameLength		= TDI_TRANSPORT_ADDRESS_LENGTH;
	eaBuffer->EaValueLength		= FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
									+ FIELD_OFFSET(TA_ADDRESS, Address)
									+ TDI_ADDRESS_LENGTH_IP;

    for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
        eaBuffer->EaName[i] = TdiTransportAddress[i];
    }

	transportAddress = (PTRANSPORT_ADDRESS)&eaBuffer->EaName[eaBuffer->EaNameLength+1];
	transportAddress->TAAddressCount = 1;

    taAddress = (PTA_ADDRESS)transportAddress->Address;
    taAddress->AddressType		= TDI_ADDRESS_TYPE_IP;
    taAddress->AddressLength	= TDI_ADDRESS_LENGTH_IP;

    TcpAddress = (PTDI_ADDRESS_IP)taAddress->Address;

	RtlCopyMemory(
		TcpAddress,
		Address,
		sizeof(TDI_ADDRESS_IP)
		);

	//
	// Open Address File
	//
	status = ZwCreateFile(
				&addressFileHandle,
				GENERIC_READ,
				&objectAttributes,
				&ioStatusBlock,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				0,
				0,
				0,
				eaBuffer,
				TCPADDRESS_EA_BUFFER_LENGTH 
				);
	
	if (!NT_SUCCESS(status)) {
	    TCPLtDebugPrint (0,("TdiOpenAddress:  FAILURE, NtCreateFile returned status code=%x.\n", status));
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint (0, ("TdiOpenAddress:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
    }

    status = ObReferenceObjectByHandle (
                addressFileHandle,
                0L,
                NULL,
                KernelMode,
                (PVOID *) &addressFileObject,
                NULL
				);

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint(0,("\n****** Send Test:  FAILED on open of server Connection: %x ******\n", status));
		ZwClose(addressFileHandle);
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
        return status;
    }
    
	*AddressFileHandle = addressFileHandle;
	*AddressFileObject = addressFileObject;

	TCPLtDebugPrint (3, ("[TcpTdi] TdiOpenAddress:  returning\n"));

	return status;
}


NTSTATUS
TcpTdiCloseAddress (
	IN HANDLE			AddressFileHandle,
	IN	PFILE_OBJECT	AddressFileObject
	)
{
    NTSTATUS status;

	if(AddressFileObject)
		ObDereferenceObject(AddressFileObject);

	if(AddressFileHandle == NULL)
		return STATUS_SUCCESS;

    status = ZwClose (AddressFileHandle);

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint (1, ("[TcpTdi] CloseAddress:  FAILURE, NtClose returned status code=%x\n", status));
    } else {
        TCPLtDebugPrint (1, ("[TcpTdi] CloseAddress:  NT_SUCCESS.\n"));
    }

    return status;
} // TcpCloseAddress 




NTSTATUS
TcpTdiOpenConnection (
	OUT PHANDLE					ConnectionFileHandle, 
	OUT	PFILE_OBJECT			*ConnectionFileObject,
	IN PVOID					ConnectionContext
	)
{
	HANDLE						connectionFileHandle; 
	PFILE_OBJECT				connectionFileObject;

    UNICODE_STRING				nameString;
    OBJECT_ATTRIBUTES			objectAttributes;
	UCHAR						eaFullBuffer[TCPCONNECTION_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;
    

    TCPLtDebugPrint (3, ("[TcpTdi] TcpTdiOpenConnection:  Entered\n"));

	//
	// Init object attributes
	//

    RtlInitUnicodeString (&nameString, TCPTRANSPORT_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	
	RtlZeroMemory(eaBuffer, TCPCONNECTION_EA_BUFFER_LENGTH);
    eaBuffer->NextEntryOffset	= 0;
    eaBuffer->Flags				= 0;
    eaBuffer->EaNameLength		= TDI_CONNECTION_CONTEXT_LENGTH;
	eaBuffer->EaValueLength		= sizeof (PVOID);

    for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
        eaBuffer->EaName[i] = TdiConnectionContext[i];
    }
    
    RtlMoveMemory (
        &eaBuffer->EaName[TDI_CONNECTION_CONTEXT_LENGTH+1],
        &ConnectionContext,
        sizeof (PVOID)
		);

	status = ZwCreateFile(
				&connectionFileHandle,
				GENERIC_READ,
				&objectAttributes,
				&ioStatusBlock,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				0,
				0,
				0,
				eaBuffer,
				TCPCONNECTION_EA_BUFFER_LENGTH
				);

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint (0, ("[TcpTdi] TdiOpenConnection:  FAILURE, NtCreateFile returned status code=%x\n", status));
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
        return status;
    }

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint (0, ("[TcpTdi] TdiOpenConnection:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
		return status;
    }

    status = ObReferenceObjectByHandle (
                connectionFileHandle,
                0L,
                NULL,
                KernelMode,
                (PVOID *) &connectionFileObject,
                NULL
				);

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint(0, ("[TcpTdi] TdiOpenConnection:  ObReferenceObjectByHandle() FAILED %x\n", status));
		ZwClose(connectionFileHandle);
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
        return status;
    }
	 
	*ConnectionFileHandle = connectionFileHandle;
	*ConnectionFileObject = connectionFileObject;

	TCPLtDebugPrint (3, ("[TcpTdi] TcpOpenConnection:  returning\n"));

    return status;
} // TcpOpenConnection



NTSTATUS
TcpTdiCloseConnection (
	IN	HANDLE			ConnectionFileHandle,
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
    NTSTATUS status;

	if(ConnectionFileObject)
		ObDereferenceObject(ConnectionFileObject);

	if(!ConnectionFileHandle)
		return STATUS_SUCCESS;

	status = ZwClose (ConnectionFileHandle);

    if (!NT_SUCCESS(status)) {
	    TCPLtDebugPrint (1, ("[TcpTdi] TcpCloseConnection:  FAILURE, NtClose returned status code=%x\n", status));
	} else {
		TCPLtDebugPrint (1, ("[TcpTdi] TcpCloseConnection:  NT_SUCCESS.\n"));
	}

    return status;
} // TcpCloseConnection 





NTSTATUS
TcpTdiOpenCtrlChannel (
	OUT PHANDLE					CtrlChannelFileHandle, 
	OUT	PFILE_OBJECT			*CtrlChannelFileObject
	)
{
	HANDLE						ctrlChannelFileHandle; 
	PFILE_OBJECT				ctrlChannelFileObject;

    UNICODE_STRING				nameString;
    OBJECT_ATTRIBUTES			objectAttributes;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;
    

    TCPLtDebugPrint (3, ("[TcpTdi] TcpTdiOpenCtrlChannel:  Entered\n"));

	//
	// Init object attributes
	//

    RtlInitUnicodeString (&nameString, TCPTRANSPORT_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	
	status = ZwCreateFile(
				&ctrlChannelFileHandle,
				GENERIC_READ,
				&objectAttributes,
				&ioStatusBlock,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				0,
				0,
				0,
				NULL,
				0
				);

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint (0, ("[TcpTdi] TdiOpenConnection:  FAILURE, NtCreateFile returned status code=%x\n", status));
		*CtrlChannelFileHandle = NULL;
		*CtrlChannelFileObject = NULL;
        return status;
    }

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint (0, ("[TcpTdi] TdiOpenConnection:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*CtrlChannelFileHandle = NULL;
		*CtrlChannelFileObject = NULL;
		return status;
    }

    status = ObReferenceObjectByHandle (
                ctrlChannelFileHandle,
                0L,
                NULL,
                KernelMode,
                (PVOID *) &ctrlChannelFileObject,
                NULL
				);

    if (!NT_SUCCESS(status)) {
        TCPLtDebugPrint(0, ("[TcpTdi] TdiOpenConnection:  ObReferenceObjectByHandle() FAILED %x\n", status));
		ZwClose(ctrlChannelFileHandle);
		*CtrlChannelFileHandle = NULL;
		*CtrlChannelFileObject = NULL;
        return status;
    }
	 
	*CtrlChannelFileHandle = ctrlChannelFileHandle;
	*CtrlChannelFileObject = ctrlChannelFileObject;

	TCPLtDebugPrint (3, ("[TcpTdi] TcpOpenConnection:  returning\n"));

    return status;
} // TcpTdiOpenCtrlChannel



NTSTATUS
TcpTdiCloseCtrlChannel (
	IN	HANDLE			CtrlChannelFileHandle,
	IN	PFILE_OBJECT	CtrlChannelFileObject
	)
{
    NTSTATUS status;

	if(CtrlChannelFileObject)
		ObDereferenceObject(CtrlChannelFileObject);

	if(!CtrlChannelFileHandle)
		return STATUS_SUCCESS;

	status = ZwClose (CtrlChannelFileHandle);

    if (!NT_SUCCESS(status)) {
	    TCPLtDebugPrint (0, ("[TcpTdi] TcpCloseConnection:  FAILURE, NtClose returned status code=%x\n", status));
	} else {
		TCPLtDebugPrint (3, ("[TcpTdi] TcpCloseConnection:  NT_SUCCESS.\n"));
	}

    return status;
} // TcpTdiCloseCtrlChannel 



NTSTATUS
TcpTdiIoCallDriver(
    IN		PDEVICE_OBJECT		DeviceObject,
    IN OUT	PIRP				Irp,
	IN		PIO_STATUS_BLOCK	IoStatusBlock,
	IN		PKEVENT				Event
    )
{
	NTSTATUS		ntStatus;
	NTSTATUS		wait_status;

	ntStatus = IoCallDriver(
				DeviceObject,
				Irp
				);

	if(ntStatus == STATUS_PENDING) {


		wait_status = KeWaitForSingleObject(
					Event,
					Executive,
					KernelMode,
					FALSE,
					NULL
					);

		if(wait_status != STATUS_SUCCESS) {
			TCPLtDebugPrint(1, ("[TcpTdi] TcpTdiIoCallDriver: Wait for event Failed.\n"));
			return STATUS_CONNECTION_DISCONNECTED; // STATUS_TIMEOUT;
		}
	}

    ntStatus = IoStatusBlock->Status;

	return ntStatus;
} //TcpTdiIoCallDriver



NTSTATUS
TcpTdiConnect(
	IN	PFILE_OBJECT		ControlFileObject,
	IN	PTDI_ADDRESS_IP		Address ,
	OUT PTDI_ADDRESS_IP		RetAddress
	)
{
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;

	UCHAR						buffer[
										FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
										+ FIELD_OFFSET(TA_ADDRESS, Address)
										+ TDI_ADDRESS_LENGTH_IP
										];

	UCHAR						Recvbuff[
										FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
										+ FIELD_OFFSET(TA_ADDRESS, Address)
										+ TDI_ADDRESS_LENGTH_IP
										];

    PTRANSPORT_ADDRESS			serverTransportAddress;
	PTRANSPORT_ADDRESS			returnedTransportAddress;
    PTA_ADDRESS					taAddress;
	PTDI_ADDRESS_IP				addressName;

	NTSTATUS					ntStatus;
	TDI_CONNECTION_INFORMATION	connectionInfomation;
	TDI_CONNECTION_INFORMATION	returnInfomation;

    TCPLtDebugPrint (3, ("[TcpTdi] TcpTdiConnect:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ControlFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_CONNECT,
			deviceObject,
			connectionFileObject,
			&event,
			&ioStatusBlock
			);

	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiConnect: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	serverTransportAddress = (PTRANSPORT_ADDRESS)buffer;
	serverTransportAddress->TAAddressCount = 1;

    taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
    taAddress->AddressType		= TDI_ADDRESS_TYPE_IP;
    taAddress->AddressLength	= TDI_ADDRESS_LENGTH_IP;

    addressName = (PTDI_ADDRESS_IP)taAddress->Address;

	RtlCopyMemory(
		addressName,
		Address,
		TDI_ADDRESS_LENGTH_IP
		);	

	//
	// Make Connection Info...
	//

	RtlZeroMemory(
		&connectionInfomation,
		sizeof(TDI_CONNECTION_INFORMATION)
		);

	connectionInfomation.UserDataLength = 0;
	connectionInfomation.UserData = NULL;
	connectionInfomation.OptionsLength = 0;
	connectionInfomation.Options = NULL;
	connectionInfomation.RemoteAddress = serverTransportAddress;
	connectionInfomation.RemoteAddressLength = 	FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
													+ FIELD_OFFSET(TA_ADDRESS, Address)
													+ TDI_ADDRESS_LENGTH_IP;


	// Set Returned information
	returnedTransportAddress = (PTRANSPORT_ADDRESS)Recvbuff;
	returnedTransportAddress->TAAddressCount = 1;
    taAddress = (PTA_ADDRESS)returnedTransportAddress->Address;
    taAddress->AddressType		= TDI_ADDRESS_TYPE_IP;
    taAddress->AddressLength	= TDI_ADDRESS_LENGTH_IP;	
	
	RtlZeroMemory(
		&returnInfomation,
		sizeof(TDI_CONNECTION_INFORMATION)
		);
		
	returnInfomation.UserDataLength = 0;
	returnInfomation.UserData = NULL;
	returnInfomation.OptionsLength = 0;
	returnInfomation.Options = NULL;
	returnInfomation.RemoteAddress = returnedTransportAddress;
	returnInfomation.RemoteAddressLength = 	FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
													+ FIELD_OFFSET(TA_ADDRESS, Address)
													+ TDI_ADDRESS_LENGTH_IP;

	TdiBuildConnect(
		irp,
		deviceObject,
		ControlFileObject,
		NULL,
		NULL,
		NULL,
		&connectionInfomation,
		&returnInfomation
		);

	irp->MdlAddress = NULL;

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1,("[TcpTdi] TdiConnect %x\n", ntStatus));
		TCPLtDebugPrint(1, ("[TcpTdi]TdiConnect: Failed.\n"));
	}

	returnedTransportAddress = (PTRANSPORT_ADDRESS)Recvbuff;
    taAddress = (PTA_ADDRESS)returnedTransportAddress->Address;
	addressName = (PTDI_ADDRESS_IP)taAddress->Address;
	RtlCopyMemory(
		RetAddress,
		addressName,
		TDI_ADDRESS_LENGTH_IP
		);

	return ntStatus;
} // TcpTdiConnect




NTSTATUS
TcpTdiDisconnect(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	)
{
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    TCPLtDebugPrint (3, ("TcpTdiDisconnect:  Entered\n"));
	ASSERT(ConnectionFileObject);

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_DISCONNECT,
			deviceObject,
			ConnectionFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]KSTdiDisconnect: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	TdiBuildDisconnect(
		irp,
		deviceObject,
		ConnectionFileObject,
		NULL,
		NULL,
		NULL,
		Flags,
		NULL,
		NULL
		);

	irp->MdlAddress = NULL;

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiDisconnect: Failed. ntStatus = %x\n", ntStatus));
	}

	return ntStatus;
}



NTSTATUS
TcpTdiAssociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE			AddressFileHandle
	)
{
	KEVENT			event;
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    TCPLtDebugPrint (3, ("[TcpTdi] TcpTdiAssociateAddress:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(
		&event, 
		NotificationEvent, 
		FALSE
		);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_ASSOCIATE_ADDRESS,
			deviceObject,
			ConnectionFileObject,
			&event,
			&ioStatusBlock
			);

	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi] TcpTdiAssociateAddress: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiBuildAssociateAddress(
		irp,
		deviceObject,
		ConnectionFileObject,
		NULL,
		NULL,
		AddressFileHandle
		);

	irp->MdlAddress = NULL;

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi] TcpTdiAssociateAddress: Failed.\n"));
	}

	return ntStatus;
}



NTSTATUS
TcpTdiDisassociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    TCPLtDebugPrint (1, ("[TcpTdi] TcpTdiDisassociateAddress:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_DISASSOCIATE_ADDRESS,
			deviceObject,
			ConnectionFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiDisassociateAddress: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiBuildDisassociateAddress(
		irp,
		deviceObject,
		ConnectionFileObject,
		NULL,
		NULL
		);

	irp->MdlAddress = NULL;

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiDisassociateAddress: Failed.\n"));
	}

	return ntStatus;
}






NTSTATUS
TcpTdiListenCompletionRoutine(
	IN	PDEVICE_OBJECT				DeviceObject,
	IN	PIRP						Irp,
	IN	PTCP_TDI_LISTEN_CONTEXT		TdiListenContext					
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);


	TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiListenCompletionRoutine\n"));

	TdiListenContext->Status = Irp->IoStatus.Status ;

	if(Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		PTRANSPORT_ADDRESS	tpAddr ;
		PTA_ADDRESS			taAddr ;
		PTDI_ADDRESS_IP		tcpAddr ;

		
		tpAddr = (PTRANSPORT_ADDRESS)TdiListenContext->AddressBuffer ;
		taAddr = tpAddr->Address ;
		tcpAddr = (PTDI_ADDRESS_IP)taAddr->Address ;
		
		RtlCopyMemory(&TdiListenContext->RemoteAddress, tcpAddr, sizeof(TDI_ADDRESS_IP) ) ;
	} else {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiListenCompletionRoutine: Listen IRP completed with an error.\n"));
	}

	KeSetEvent(&TdiListenContext->CompletionEvent, IO_NETWORK_INCREMENT, FALSE) ;	
	TdiListenContext->Irp = NULL;
	
	return STATUS_SUCCESS;
}





NTSTATUS
TcpTdiListenWithCompletionEvent(
	IN	PFILE_OBJECT			ConnectionFileObject,
	IN  PTCP_TDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG					Flags
	)
{
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	NTSTATUS			ntStatus;

    TCPLtDebugPrint (1, ("[TcpTdi]TcpTdiListen: Entered.\n"));

	//
	// Make Event.
	//
	//KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_LISTEN,
			deviceObject,
			ConnectionFileObject,
			NULL,
			NULL
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiListen: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiListenContext->RequestConnectionInfo.UserData = NULL ;
	TdiListenContext->RequestConnectionInfo.UserDataLength = 0 ;
	TdiListenContext->RequestConnectionInfo.Options =  Flags ;
	TdiListenContext->RequestConnectionInfo.OptionsLength = sizeof(ULONG) ;
	TdiListenContext->RequestConnectionInfo.RemoteAddress = NULL ;
	TdiListenContext->RequestConnectionInfo.RemoteAddressLength = 0 ;

	TdiListenContext->ReturnConnectionInfo.UserData = NULL ;
	TdiListenContext->ReturnConnectionInfo.UserDataLength = 0 ;
	TdiListenContext->ReturnConnectionInfo.Options =  Flags ;
	TdiListenContext->ReturnConnectionInfo.OptionsLength = sizeof(ULONG) ;
	TdiListenContext->ReturnConnectionInfo.RemoteAddress = TdiListenContext->AddressBuffer ;
	TdiListenContext->ReturnConnectionInfo.RemoteAddressLength = TPADDR_IP_LENGTH ;

	TdiBuildListen(
			irp,
			deviceObject,
			ConnectionFileObject,
			TcpTdiListenCompletionRoutine,
			TdiListenContext,
			*Flags,
			&TdiListenContext->RequestConnectionInfo,
			&TdiListenContext->ReturnConnectionInfo
		);

	ntStatus = IoCallDriver(
				deviceObject,
				irp
				);

	
	if(!NT_SUCCESS(ntStatus)) {
		TdiListenContext->Irp = NULL;
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiListen: Failed.\n"));
		return ntStatus;
	}
	TdiListenContext->Irp = irp;
	
	return ntStatus;
}




NTSTATUS
TcpTdiAccept(
	IN	PFILE_OBJECT		ControlFileObject,
	IN	PTDI_ADDRESS_IP		Address 
	)
{
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;

	UCHAR						buffer[
										FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
										+ FIELD_OFFSET(TA_ADDRESS, Address)
										+ TDI_ADDRESS_LENGTH_IP
										];

    PTRANSPORT_ADDRESS			serverTransportAddress;
    PTA_ADDRESS					taAddress;
	PTDI_ADDRESS_IP				addressName;

	NTSTATUS					ntStatus;
	TDI_CONNECTION_INFORMATION	connectionInfomation;

    TCPLtDebugPrint (3, ("[TcpTdi] TcpTdiConnect:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ControlFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_CONNECT,
			deviceObject,
			connectionFileObject,
			&event,
			&ioStatusBlock
			);

	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiConnect: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	serverTransportAddress = (PTRANSPORT_ADDRESS)buffer;
	serverTransportAddress->TAAddressCount = 1;

    taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
    taAddress->AddressType		= TDI_ADDRESS_TYPE_IP;
    taAddress->AddressLength	= TDI_ADDRESS_LENGTH_IP;

    addressName = (PTDI_ADDRESS_IP)taAddress->Address;

	RtlCopyMemory(
		addressName,
		Address,
		TDI_ADDRESS_LENGTH_IP
		);	

	//
	// Make Connection Info...
	//

	RtlZeroMemory(
		&connectionInfomation,
		sizeof(TDI_CONNECTION_INFORMATION)
		);

	connectionInfomation.UserDataLength = 0;
	connectionInfomation.UserData = NULL;
	connectionInfomation.OptionsLength = 0;
	connectionInfomation.Options = NULL;
	connectionInfomation.RemoteAddress = serverTransportAddress;
	connectionInfomation.RemoteAddressLength = 	FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
													+ FIELD_OFFSET(TA_ADDRESS, Address)
													+ TDI_ADDRESS_LENGTH_IP;



	TdiBuildAccept(
		irp,
		deviceObject,
		ControlFileObject,
		NULL,
		NULL,
		&connectionInfomation,
		NULL
		);

	irp->MdlAddress = NULL;

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiConnect: Failed.\n"));
	}



	return ntStatus;
} // TcpTdiConnect


NTSTATUS
TcpTdiSendCompletionRoutine(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context					
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);

	if(Irp->MdlAddress != NULL) {
//		MmUnlockPages(Irp->MdlAddress);
//		ExFreePool(MmGetMdlVirtualAddress(Irp->MdlAddress));
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiSendCompletionRoutine: Mdl is NULL!!!\n"));
	}

	return STATUS_SUCCESS;
}



NTSTATUS
TcpTdiSend(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	)
{
	KEVENT				event;
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			ntStatus;
	PMDL				mdl;
	
    TCPLtDebugPrint (3, ("TcpTdiSend:  Entered\n"));

	SendLength = SendLength > TCPTDI_BYTEPERPACKET?TCPTDI_BYTEPERPACKET:SendLength ;
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_SEND,
			deviceObject,
			ConnectionFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiSend: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

//	try {	
		//
		// Make MDL.
		//

		mdl = IoAllocateMdl(
				SendBuffer,
				SendLength,
				FALSE,
				FALSE,
				irp
				);
		if(mdl == NULL) {
			TCPLtDebugPrint(1, ("[TcpTdi]TdiSend: Can't Allocate MDL.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		mdl->Next = NULL;
		MmBuildMdlForNonPagedPool(mdl);




//		MmProbeAndLockPages(
//		mdl,
//			KernelMode,
//			IoReadAccess
//			);
			
    
//	} except (EXCEPTION_EXECUTE_HANDLER) {
//		TCPLtDebugPrint(1, ("[TcpTdi]TdiSend: Can't Convert Non-Paged Memory MDL.\n"));
//		if(mdl){
//			IoFreeMdl(mdl);
//		}
//		return STATUS_INSUFFICIENT_RESOURCES;
//	}

//	if(!MmIsNonPagedSystemAddressValid(MmGetMdlVirtualAddress(mdl)))
//	{
//		if(mdl){
//			IoFreeMdl(mdl);
//		};		 
//		return STATUS_INSUFFICIENT_RESOURCES;
//	}

	TdiBuildSend(
		irp,
		deviceObject,
		ConnectionFileObject,
		TcpTdiSendCompletionRoutine,
		NULL,
		mdl,
		Flags,
		SendLength
		);
	
	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiSend: Failed.\n"));
		*Result = -1;
		return ntStatus;
	}
	*Result = ioStatusBlock.Information;

	return ntStatus;
}



NTSTATUS
TcpTdiRcvCompletionRoutine(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context					
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);

	if(Irp->MdlAddress != NULL) {
//		MmUnlockPages(Irp->MdlAddress);
//		ExFreePool(MmGetMdlVirtualAddress(Irp->MdlAddress));
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiRcvCompletionRoutine: Mdl is NULL!!!\n"));
	}

	return STATUS_SUCCESS;
}



NTSTATUS
TcpTdiRecv(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLONG			Result
	)
{
	KEVENT				event;
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			ntStatus;
	PMDL				mdl;
	

	if((RecvBuffer == NULL) || (RecvLength == 0))
	{
		TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Rcv buffer == NULL or RcvLen == 0.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
    TCPLtDebugPrint (3, ("TcpTdiRecv:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_RECEIVE,
			deviceObject,
			connectionFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

/*
	try {		
		//
		// Make MDL.
		//
*/
		mdl = IoAllocateMdl(
				RecvBuffer,
				RecvLength,
				FALSE,
				FALSE,
				irp
				);
		if(mdl == NULL) {
			TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Can't Allocate MDL.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		mdl->Next = NULL;

		MmBuildMdlForNonPagedPool(mdl);
/*
		MmProbeAndLockPages(
			mdl,
			KernelMode,
			IoWriteAccess
			);
	} except (EXCEPTION_EXECUTE_HANDLER) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Can't Convert Non-Paged Memory MDL.\n"));
		if(mdl){
			IoFreeMdl(mdl);
			//IoFreeIrp(irp);
		}
		return STATUS_INSUFFICIENT_RESOURCES;
	}
*/

	
	TdiBuildReceive(
		irp,
		deviceObject,
		ConnectionFileObject,
		TcpTdiRcvCompletionRoutine,
		NULL,
		mdl,
		Flags,
		RecvLength
		);
	
	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiRecv: Failed.\n"));
		*Result = -1;
		return ntStatus;
	}

	*Result = ioStatusBlock.Information;

	return ntStatus;
}




NTSTATUS
TcpTdiRecvWithCompletionEventCompletionRoutine(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp,
	IN	PTCP_TDI_RECEIVE_CONTEXT	TdiReceiveContext
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	if(Irp->MdlAddress != NULL) {
//		MmUnlockPages(Irp->MdlAddress);
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		TCPLtDebugPrint(1, ("[TcpTdi]KSCompletionRoutine: Mdl is NULL!!!\n"));
	}

	TdiReceiveContext->Result = Irp->IoStatus.Information;
	if(Irp->IoStatus.Status != STATUS_SUCCESS)
		TdiReceiveContext->Result = -1;

	KeSetEvent(&TdiReceiveContext->CompletionEvent, IO_NETWORK_INCREMENT, FALSE) ;
	TdiReceiveContext->Irp = NULL;

	return STATUS_SUCCESS;
}


NTSTATUS
TcpTdiRecvWithCompletionEvent(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN  PTCP_TDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG					Flags
	)
{
    PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
	NTSTATUS				ntStatus;
	PMDL					mdl;


	ASSERT(ConnectionFileObject);
    TCPLtDebugPrint (3, ("TcpTdiRecvWithCompletionEvent:  Entered\n"));

	if((RecvBuffer == NULL) || (RecvLength == 0))
	{
		TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Rcv buffer == NULL or RcvLen == 0.\n"));
		return STATUS_NOT_IMPLEMENTED;
	}

 	
    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_RECEIVE,
			deviceObject,
			connectionFileObject,
			NULL,
			NULL
			);
	
	if(irp == NULL) 
	{
		TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl(
				RecvBuffer,
				RecvLength,
				FALSE,
				FALSE,
				irp
				);
	if(mdl == NULL) 
	{
		TCPLtDebugPrint(1, ("[TcpTdi]TdiReceive: Can't Allocate MDL.\n"));
		IoFreeIrp(irp);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl->Next = NULL;
	MmBuildMdlForNonPagedPool(mdl);
	
	TdiBuildReceive(
		irp,
		deviceObject,
		ConnectionFileObject,
		TcpTdiRecvWithCompletionEventCompletionRoutine,
		TdiReceiveContext,
		mdl,
		Flags,
		RecvLength
		);
	
//	SET_IRP_EXPTIME(irp, CurrentTime().QuadPart + TDI_TIME_OUT);
//	SET_IRP_EXPTIME(irp, 0);
	ntStatus = IoCallDriver(
				deviceObject,
				irp
				);

	if(!NT_SUCCESS(ntStatus)) 
	{
		TdiReceiveContext->Irp = NULL;
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiRecv: Failed.\n"));
	
		return ntStatus;
	}

	TdiReceiveContext->Irp = irp;
	
	return ntStatus;
}

NTSTATUS
TcpTdiSendDataGram(
	IN	PFILE_OBJECT	AddressFileObject,
	IN  PTDI_ADDRESS_IP	DestAddress,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	)
{
	KEVENT				event;
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			ntStatus;
	PMDL				mdl;
    TDI_CONNECTION_INFORMATION  SendDatagramInfo ;
	UCHAR				AddrBuffer[256] ;
	PTRANSPORT_ADDRESS	RemoteAddress = (PTRANSPORT_ADDRESS)AddrBuffer ;
	
	UNREFERENCED_PARAMETER(Flags);

    TCPLtDebugPrint (3, ("TcpTdiSend:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(AddressFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_SEND_DATAGRAM,
			deviceObject,
			AddressFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiSend: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	//
	// Make MDL.
	//
	mdl = IoAllocateMdl(
			SendBuffer,
			SendLength,
			FALSE,
			FALSE,
			irp
			);
	if(mdl == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TdiSend: Can't Allocate MDL.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	MmBuildMdlForNonPagedPool(mdl);
	mdl->Next = NULL;
//	MmProbeAndLockPages(
//		mdl,
//		KernelMode,
//		IoReadAccess
//		);

	RemoteAddress->TAAddressCount = 1 ;
    RemoteAddress->Address[0].AddressType	= TDI_ADDRESS_TYPE_IP;
    RemoteAddress->Address[0].AddressLength	= TDI_ADDRESS_LENGTH_IP;
	RtlCopyMemory(RemoteAddress->Address[0].Address, DestAddress, sizeof(TDI_ADDRESS_IP)) ;


	SendDatagramInfo.UserDataLength = 0 ;
	SendDatagramInfo.UserData = NULL ;
	SendDatagramInfo.OptionsLength = 0 ;
	SendDatagramInfo.Options = NULL ;
	SendDatagramInfo.RemoteAddressLength =	TPADDR_IP_LENGTH ;
	SendDatagramInfo.RemoteAddress = RemoteAddress ;
	

	TdiBuildSendDatagram(
		irp,
		deviceObject,
		AddressFileObject,
		TcpTdiSendCompletionRoutine,
		NULL,
		mdl,
		SendLength,
		&SendDatagramInfo
    );

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpTdiSend: Failed.\n"));
		*Result = -1;
		return ntStatus;
	}
	*Result = ioStatusBlock.Information;

	return ntStatus;
}


NTSTATUS
TcpTdiSetReceiveDatagramHandler(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	) {
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    TCPLtDebugPrint (1, ("[LxpTdi]TcpSetReceiveDatagramHandler:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(AddressFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
			TDI_SET_EVENT_HANDLER,
			deviceObject,
			AddressFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpSetReceiveDatagramHandler: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	TdiBuildSetEventHandler(
		irp,
		deviceObject,
		AddressFileObject,
		NULL,
		NULL,
		TDI_EVENT_RECEIVE_DATAGRAM,
		InEventHandler,
		InEventContext
		);

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpSetReceiveDatagramHandler: Can't Build IRP.\n"));
	}

	TCPLtDebugPrint(3, ("[TcpTdi] Leave TcpSetReceiveDatagramHandler\n"));

	return ntStatus;
}


NTSTATUS
TcpSetDisconnectHandler(
						IN	PFILE_OBJECT	AddressFileObject,
						IN	PVOID			InEventHandler,
						IN	PVOID			InEventContext
						)
{
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    TCPLtDebugPrint (3, ("[TcpTdi]TcpSetDisconnectHandler:  Entered\n"));
	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(AddressFileObject);

	//
	// Make IRP.
	//
	irp =  TdiBuildInternalDeviceControlIrp(
			TDI_SET_EVENT_HANDLER,
			deviceObject,
			AddressFileObject,
			&event,
			&ioStatusBlock
			);
	if(irp == NULL) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpSetDisconnectHandler: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	TdiBuildSetEventHandler(
		irp,
		deviceObject,
		AddressFileObject,
		NULL,
		NULL,
		TDI_EVENT_DISCONNECT,
		InEventHandler,
		InEventContext
		);

	ntStatus = TcpTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event
				);

	if(!NT_SUCCESS(ntStatus)) {
		TCPLtDebugPrint(1, ("[TcpTdi]TcpSetDisconnectHandler: Failed.\n"));
	}

	return ntStatus;
}


