#include <ndis.h>
#include <tdi.h>
#include <tdikrnl.h>

#include "LSKLib.h"
#include "LSTransport.h"
#include "LpxTdiProc.h"

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

//////////////////////////////////////////////////////////////////////////
//
//	Support for Lanscsi transport interface.
//
NTSTATUS
LpxTdiOpenAddress_LSTrans(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PHANDLE			AddressFileHandle,
		OUT	PFILE_OBJECT	*AddressFileObject
	);

NTSTATUS
LpxTdiOpenConnection_LSTrans (
		IN PVOID			ConnectionContext,
		OUT PHANDLE			ConnectionFileHandle,
		OUT	PFILE_OBJECT	*ConnectionFileObject
	);

NTSTATUS
LpxTdiConnect_LSTrans(
		IN	PFILE_OBJECT		ConnectionFileObject,
		IN	PTA_LSTRANS_ADDRESS	Address,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LpxTdiListen_LSTrans(
		IN	PFILE_OBJECT		ConnectionFileObject,
		IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
		IN  PULONG				Flags,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LpxTdiSend_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	PUCHAR			SendBuffer,
		IN 	ULONG			SendLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext,
		IN PLARGE_INTEGER	TimeOut

	);

NTSTATUS
LpxTdiRecv_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject,
		OUT	PUCHAR			RecvBuffer,
		IN	ULONG			RecvLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
LpxTdiSendDataGram_LSTrans(
		IN	PFILE_OBJECT	AddressFileObject,
		PTA_LSTRANS_ADDRESS	RemoteAddress,
		IN	PUCHAR			SendBuffer,
		IN 	ULONG			SendLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext
	);

NTSTATUS
LpxTdiIoControl_LSTRans (
		IN	ULONG			IoControlCode,
		IN	PVOID			InputBuffer OPTIONAL,
		IN	ULONG			InputBufferLength,
		OUT PVOID			OutputBuffer OPTIONAL,
	    IN	ULONG			OutputBufferLength
	);

NTSTATUS
LpxTdiGetAddressList_LSTrans(
		IN	ULONG				AddressListLen,
		IN	PTA_LSTRANS_ADDRESS	AddressList,
		OUT	PULONG				OutLength
	);

LANSCSITRANSPORT_INTERFACE	LstransLPXV10Interface = {
		sizeof(LANSCSITRANSPORT_INTERFACE),
		LSSTRUC_TYPE_TRANSPORT_INTERFACE,
		TDI_ADDRESS_TYPE_LPX,
		TDI_ADDRESS_LENGTH_LPX,
		LSTRANS_LPX_V1,				// version 1.0
		0,
		{
			LpxTdiOpenAddress_LSTrans,
			LpxTdiCloseAddress,
			LpxTdiOpenConnection_LSTrans,
			LpxTdiCloseConnection,
			LpxTdiAssociateAddress,
			LpxTdiDisassociateAddress,
			LpxTdiConnect_LSTrans,
			LpxTdiDisconnect,
			LpxTdiListen_LSTrans,
			LpxTdiSend_LSTrans,
			LpxTdiRecv_LSTrans,
			LpxTdiSendDataGram_LSTrans,
			LpxTdiSetReceiveDatagramHandler,
			LpxSetDisconnectHandler,
			LpxTdiIoControl_LSTRans,
			LpxTdiGetAddressList_LSTrans
		}
	};

//////////////////////////////////////////////////////////////////////////
//
//
//
#define TRANSPORT_NAME	SOCKETLPX_DEVICE_NAME
#define	ADDRESS_EA_BUFFER_LENGTH	(								\
					FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)  \
					+ TDI_TRANSPORT_ADDRESS_LENGTH + 1				\
					+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)		\
					+ FIELD_OFFSET(TA_ADDRESS, Address)				\
					+ TDI_ADDRESS_LENGTH_LPX						\
					)

#define CONNECTION_EA_BUFFER_LENGTH	(								\
					FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)	\
					+ TDI_CONNECTION_CONTEXT_LENGTH + 1				\
					+ sizeof(PVOID)									\
					)							


LONG	DebugLevel = 1;

#if DBG
extern LONG	DebugLevel;
#define LtDebugPrint(l, x)			\
		if(l <= DebugLevel) {		\
			DbgPrint x; 			\
		}
#else	
#define LtDebugPrint(l, x) 
#endif

//
//	get the current system clock
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

NTSTATUS
LpxTdiSendCompletionRoutine(
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendCompletionRoutine: Mdl is NULL!!!\n"));
	}

	if((LONG)Irp->Tail.Overlay.DriverContext[2] > 0)
	{
		LtDebugPrint(3, ("[LpxTdi]LpxTdiSendCompletionRoutine: (LONG)Irp->Tail.Overlay.DriverContext[2] = %d. %p %p!!!\n",
			(LONG)Irp->Tail.Overlay.DriverContext[2], DeviceObject, Context));

		// retransmits occurred
		if(Context && TDI_FAKE_CONTEXT != Context)
		{
			PTDI_SEND_RESULT SendResult = (PTDI_SEND_RESULT)Context;
			SendResult->Retransmits = (LONG)Irp->Tail.Overlay.DriverContext[2];
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LpxTdiRcvCompletionRoutine(
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRcvCompletionRoutine: Mdl is NULL!!!\n"));
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LpxTdiOpenControl (
	OUT PHANDLE					ControlFileHandle, 
	OUT	PFILE_OBJECT			*ControlFileObject,
	IN PVOID					ControlContext
	)
{
	HANDLE						controlFileHandle; 
	PFILE_OBJECT				controlFileObject;

    UNICODE_STRING				nameString;
    OBJECT_ATTRIBUTES			objectAttributes;
	UCHAR						eaFullBuffer[CONNECTION_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;
    

    LtDebugPrint (3, ("LpxTdiOpenControl:  Entered\n"));

	//
	// Init object attributes
	//

    RtlInitUnicodeString (&nameString, TRANSPORT_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	
	RtlZeroMemory(eaBuffer, CONNECTION_EA_BUFFER_LENGTH);
    eaBuffer->NextEntryOffset	= 0;
    eaBuffer->Flags				= 0;
    eaBuffer->EaNameLength		= TDI_CONNECTION_CONTEXT_LENGTH;
	eaBuffer->EaValueLength		= sizeof (PVOID);

    for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
        eaBuffer->EaName[i] = TdiConnectionContext[i];
    }
    
    RtlMoveMemory (
        &eaBuffer->EaName[TDI_CONNECTION_CONTEXT_LENGTH+1],
        &ControlContext,
        sizeof (PVOID)
		);

	status = ZwCreateFile(
				&controlFileHandle,
				GENERIC_READ,
				&objectAttributes,
				&ioStatusBlock,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				0,
				0,
				0,
				NULL,	// eaBuffer,
				0		// CONNECTION_EA_BUFFER_LENGTH
				);

    if (!NT_SUCCESS(status)) {
        LtDebugPrint (0, ("TdiOpenControl:  FAILURE, ZwCreateFile returned status code=%x\n", status));
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
        return status;
    }

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        LtDebugPrint (0, ("TdiOpenControl:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
    }

    status = ObReferenceObjectByHandle (
                controlFileHandle,
                0L,
                NULL,
                KernelMode,
                (PVOID *) &controlFileObject,
                NULL
				);

    if (!NT_SUCCESS(status)) {
        LtDebugPrint(0, ("\n****** Send Test:  FAILED on open of server Control: %x ******\n", status));
		ZwClose(controlFileHandle);
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
        return status;
    }
	 
	*ControlFileHandle = controlFileHandle;
	*ControlFileObject = controlFileObject;

	LtDebugPrint (3, ("LpxOpenControl:  returning\n"));

    return status;
} /* LpxOpenControl */


NTSTATUS
LpxTdiCloseControl (
	IN	HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject
	)
{
    NTSTATUS status;

	if(ControlFileObject)
		ObDereferenceObject(ControlFileObject);

	if(!ControlFileHandle)
		return STATUS_SUCCESS;

	status = ZwClose (ControlFileHandle);

    if (!NT_SUCCESS(status)) {
	    LtDebugPrint (0, ("LpxCloseControl:  FAILURE, NtClose returned status code=%x\n", status));
	} else {
		LtDebugPrint (3, ("LpxCloseControl:  NT_SUCCESS.\n"));
	}

    return status;
} /* LpxCloseControl */


NTSTATUS
LpxTdiIoControl_LSTRans (
		IN	ULONG			IoControlCode,
		IN	PVOID			InputBuffer OPTIONAL,
		IN	ULONG			InputBufferLength,
		OUT PVOID			OutputBuffer OPTIONAL,
	    IN	ULONG			OutputBufferLength
	){
	NTSTATUS		status;
	HANDLE			ControlFileHandle;
	PFILE_OBJECT	ControlFileObject;

	status = LpxTdiOpenControl(&ControlFileHandle, &ControlFileObject, NULL);
	if(!NT_SUCCESS(status))
		goto error_out;

	status = LpxTdiIoControl(
					ControlFileHandle,
					ControlFileObject,
					IoControlCode,
					InputBuffer,
					InputBufferLength,
					OutputBuffer,
					OutputBufferLength);

	LpxTdiCloseControl (
					ControlFileHandle,
					ControlFileObject
				);
error_out:
	return status;
}


NTSTATUS
LpxTdiIoControl (
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject,
    IN	ULONG			IoControlCode,
    IN	PVOID			InputBuffer OPTIONAL,
    IN	ULONG			InputBufferLength,
    OUT PVOID			OutputBuffer OPTIONAL,
    IN	ULONG			OutputBufferLength
	)
{
	NTSTATUS		ntStatus;
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
    KEVENT			event;
    IO_STATUS_BLOCK	ioStatus;

	UNREFERENCED_PARAMETER(ControlFileHandle);

    LtDebugPrint (3, ("LpxTdiIoControl:  Entered\n"));

    deviceObject = IoGetRelatedDeviceObject(ControlFileObject);
    
	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp = IoBuildDeviceIoControlRequest(
			IoControlCode,
			deviceObject,
			InputBuffer,
			InputBufferLength,
			OutputBuffer,
			OutputBufferLength,
			FALSE,
			&event,
			&ioStatus
			);
	
    if (irp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
    if (ntStatus == STATUS_PENDING)
    {
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        ntStatus = ioStatus.Status;
    }
	
	return ntStatus;
}

NTSTATUS
LpxTdiOpenAddress_LSTrans(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PHANDLE			AddressFileHandle,
		OUT	PFILE_OBJECT	*AddressFileObject
	)
{
	ASSERT(Address->TAAddressCount == 1);
	ASSERT(Address->Address[0].AddressType == TDI_ADDRESS_TYPE_LPX);

	return LpxTdiOpenAddress(AddressFileHandle, AddressFileObject, (PTDI_ADDRESS_LPX)&Address->Address[0].Address);
}

NTSTATUS
LpxTdiOpenAddress(
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject,
	IN	PTDI_ADDRESS_LPX	Address
	)
{
	HANDLE						addressFileHandle; 
	PFILE_OBJECT				addressFileObject;

    UNICODE_STRING				nameString;
    OBJECT_ATTRIBUTES			objectAttributes;
	UCHAR						eaFullBuffer[ADDRESS_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	PTRANSPORT_ADDRESS			transportAddress;
    PTA_ADDRESS					taAddress;
	PTDI_ADDRESS_LPX			lpxAddress;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;

    LtDebugPrint (3, ("[LpxTdi] TdiOpenAddress:  Entered\n"));

	//
	// Init object attributes
	//

    RtlInitUnicodeString (&nameString, TRANSPORT_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	RtlZeroMemory(eaBuffer, ADDRESS_EA_BUFFER_LENGTH);
    eaBuffer->NextEntryOffset	= 0;
    eaBuffer->Flags				= 0;
    eaBuffer->EaNameLength		= TDI_TRANSPORT_ADDRESS_LENGTH;
	eaBuffer->EaValueLength		= FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
									+ FIELD_OFFSET(TA_ADDRESS, Address)
									+ TDI_ADDRESS_LENGTH_LPX;

    for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
        eaBuffer->EaName[i] = TdiTransportAddress[i];
    }

	transportAddress = (PTRANSPORT_ADDRESS)&eaBuffer->EaName[eaBuffer->EaNameLength+1];
	transportAddress->TAAddressCount = 1;

    taAddress = (PTA_ADDRESS)transportAddress->Address;
    taAddress->AddressType		= TDI_ADDRESS_TYPE_LPX;
    taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

    lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

	RtlCopyMemory(
		lpxAddress,
		Address,
		sizeof(TDI_ADDRESS_LPX)
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
				ADDRESS_EA_BUFFER_LENGTH 
				);
	
	if (!NT_SUCCESS(status)) {
	    LtDebugPrint (0,("TdiOpenAddress:  FAILURE, NtCreateFile returned status code=%x.\n", status));
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        LtDebugPrint (0, ("TdiOpenAddress:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
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
        LtDebugPrint(0,("\n****** Send Test:  FAILED on open of server Connection: %x ******\n", status));
		ZwClose(addressFileHandle);
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
        return status;
    }
    
	*AddressFileHandle = addressFileHandle;
	*AddressFileObject = addressFileObject;

	LtDebugPrint (3, ("[LpxTdi] TdiOpenAddress:  returning\n"));

	return status;
}

NTSTATUS
LpxTdiCloseAddress (
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
        LtDebugPrint (0, ("[LpxTdi] CloseAddress:  FAILURE, NtClose returned status code=%x\n", status));
    } else {
        LtDebugPrint (3, ("[LpxTdi] CloseAddress:  NT_SUCCESS.\n"));
    }

    return status;
} /* CloseAddress */

NTSTATUS
LpxTdiOpenConnection_LSTrans (
		IN PVOID			ConnectionContext,
		OUT PHANDLE			ConnectionFileHandle,
		OUT	PFILE_OBJECT	*ConnectionFileObject
	) {

	return LpxTdiOpenConnection(ConnectionFileHandle, ConnectionFileObject, ConnectionContext);
}

NTSTATUS
LpxTdiOpenConnection (
	OUT PHANDLE					ConnectionFileHandle, 
	OUT	PFILE_OBJECT			*ConnectionFileObject,
	IN PVOID					ConnectionContext
	)
{
	HANDLE						connectionFileHandle; 
	PFILE_OBJECT				connectionFileObject;

    UNICODE_STRING				nameString;
    OBJECT_ATTRIBUTES			objectAttributes;
	UCHAR						eaFullBuffer[CONNECTION_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
    NTSTATUS					status;
    

    LtDebugPrint (3, ("[LpxTdi] LpxTdiOpenConnection:  Entered\n"));

	//
	// Init object attributes
	//

    RtlInitUnicodeString (&nameString, TRANSPORT_NAME);
    InitializeObjectAttributes (
        &objectAttributes,
        &nameString,
        0,
        NULL,
        NULL
		);

	
	RtlZeroMemory(eaBuffer, CONNECTION_EA_BUFFER_LENGTH);
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
				CONNECTION_EA_BUFFER_LENGTH
				);

    if (!NT_SUCCESS(status)) {
        LtDebugPrint (0, ("[LpxTdi] TdiOpenConnection:  FAILURE, NtCreateFile returned status code=%x\n", status));
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
        return status;
    }

    status = ioStatusBlock.Status;

    if (!NT_SUCCESS(status)) {
        LtDebugPrint (0, ("[LpxTdi] TdiOpenConnection:  FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
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
        LtDebugPrint(0, ("[LpxTdi] TdiOpenConnection:  ObReferenceObjectByHandle() FAILED %x\n", status));
		ZwClose(connectionFileHandle);
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
        return status;
    }
	 
	*ConnectionFileHandle = connectionFileHandle;
	*ConnectionFileObject = connectionFileObject;

	LtDebugPrint (3, ("[LpxTdi] LpxOpenConnection:  returning\n"));

    return status;
} /* LpxOpenConnection */


NTSTATUS
LpxTdiCloseConnection (
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
	    LtDebugPrint (0, ("[LpxTdi] LpxCloseConnection:  FAILURE, NtClose returned status code=%x\n", status));
	} else {
		LtDebugPrint (3, ("[LpxTdi] LpxCloseConnection:  NT_SUCCESS.\n"));
	}

    return status;
} /* LpxCloseConnection */


NTSTATUS
LpxTdiIoCallDriver(
    IN		PDEVICE_OBJECT		DeviceObject,
    IN OUT	PIRP				Irp,
	IN		PIO_STATUS_BLOCK	IoStatusBlock,
	IN		PKEVENT				Event,
	IN		PLARGE_INTEGER		TimeOut
    )
{
	NTSTATUS		ntStatus;
	NTSTATUS		wait_status;
//	LARGE_INTEGER	timeout;

	//
	//	set a expire time to IRP.
	//	LPX takes whole charge of IRP completion.
	//	LPX will measure time-out.
	//	Do not wait with time-out here.
	//	BSOD may occur if you do.
	//
	//	added by hootch 02092004
	if(TimeOut)
		SET_IRP_EXPTIME(Irp, CurrentTime().QuadPart + TimeOut->QuadPart);
	else
		SET_IRP_EXPTIME(Irp, 0);

	Irp->Tail.Overlay.DriverContext[2] = (PVOID)0;
	
	LtDebugPrint(2, ("[LpxTdi] Irp->Tail.Overlay.DriverContext[2] == %p\n", Irp->Tail.Overlay.DriverContext[2]));
	

	ntStatus = IoCallDriver(
				DeviceObject,
				Irp
				);

	if(ntStatus == STATUS_PENDING) {
		if(Event) {
			wait_status = KeWaitForSingleObject(
					Event,
					Executive,
					KernelMode,
					FALSE,
					NULL
					);

			if(wait_status != STATUS_SUCCESS) {
				LtDebugPrint(1, ("[LpxTdi] LpxTdiIoCallDriver: Wait for event Failed.\n"));
				return STATUS_CONNECTION_DISCONNECTED; // STATUS_TIMEOUT;
			}
		} else {
			return ntStatus;
		}
	}

    ntStatus = IoStatusBlock->Status;

	return ntStatus;
}


NTSTATUS
LpxTdiAssociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE			AddressFileHandle
	)
{
	KEVENT			event;
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    LtDebugPrint (3, ("[LpxTdi] LpxTdiAssociateAddress:  Entered\n"));

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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiAssociateAddress: Can't Build IRP.\n"));
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

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiAssociateAddress: Failed.\n"));
	}

	return ntStatus;
}


NTSTATUS
LpxTdiDisassociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    LtDebugPrint (3, ("[LpxTdi] LpxTdiDisassociateAddress:  Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi]TdiDisassociateAddress: Can't Build IRP.\n"));
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

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]TdiDisassociateAddress: Failed.\n"));
	}

	return ntStatus;
}

NTSTATUS
LpxTdiConnect_LSTrans(
		IN	PFILE_OBJECT		ConnectionFileObject,
		IN	PTA_LSTRANS_ADDRESS	Address,
		IN PLARGE_INTEGER		TimeOut
	) {
	ASSERT(Address->TAAddressCount == 1);
	ASSERT(Address->Address[0].AddressType == TDI_ADDRESS_TYPE_LPX);

	UNREFERENCED_PARAMETER(TimeOut);

	return LpxTdiConnect(ConnectionFileObject, (PTDI_ADDRESS_LPX)&Address->Address[0].Address);
}

NTSTATUS
LpxTdiConnect(
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN	PTDI_ADDRESS_LPX	Address
	)
{
	KEVENT						event;
    PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;

	UCHAR						buffer[
										FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
										+ FIELD_OFFSET(TA_ADDRESS, Address)
										+ TDI_ADDRESS_LENGTH_LPX
										];

    PTRANSPORT_ADDRESS			serverTransportAddress;
    PTA_ADDRESS					taAddress;
	PTDI_ADDRESS_LPX			addressName;

	NTSTATUS					ntStatus;
	TDI_CONNECTION_INFORMATION	connectionInfomation;

    LtDebugPrint (3, ("[LpxTdi] LpxTdiConnect:  Entered\n"));

	//
	// Make Event.
	//
	KeInitializeEvent(&event, NotificationEvent, FALSE);

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

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
		LtDebugPrint(1, ("[LpxTdi]TdiConnect: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	serverTransportAddress = (PTRANSPORT_ADDRESS)buffer;
	serverTransportAddress->TAAddressCount = 1;

    taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
    taAddress->AddressType		= TDI_ADDRESS_TYPE_LPX;
    taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

    addressName = (PTDI_ADDRESS_LPX)taAddress->Address;

	RtlCopyMemory(
		addressName,
		Address,
		TDI_ADDRESS_LENGTH_LPX
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
													+ TDI_ADDRESS_LENGTH_LPX;

	TdiBuildConnect(
		irp,
		deviceObject,
		ConnectionFileObject,
		NULL,
		NULL,
		NULL,
		&connectionInfomation,
		NULL
		);

	irp->MdlAddress = NULL;

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]TdiConnect: Failed.\n"));
	}

	return ntStatus;
}


NTSTATUS
LpxTdiDisconnect(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	)
{
    PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    LtDebugPrint (3, ("LpxTdiDisconnect:  Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi]KSTdiDisconnect: Can't Build IRP.\n"));
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

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiDisconnect: Failed. ntStatus = %x\n", ntStatus));
	}

	return ntStatus;
}

//
//	Send data.
//
NTSTATUS
LpxTdiSend_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	PUCHAR			SendBuffer,
		IN 	ULONG			SendLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext,
		IN PLARGE_INTEGER	TimeOut
	) {
	UNREFERENCED_PARAMETER(CompletionContext);

	if(CompletionContext) {
		//IO_STATUS_BLOCK	ioStatusBlock;
		NTSTATUS		ntStatus;

		ntStatus = LpxTdiSendEx(
							ConnectionFileObject,
							SendBuffer,
							SendLength,
							Flags,
							NULL,
							TimeOut,
							CompletionContext,
							NULL
						);
		if(ntStatus == STATUS_SUCCESS) {
			if(Result) *Result = SendLength;
			return ntStatus;
		} else {
			return ntStatus;
		}
	} else {
		return LpxTdiSend_TimeOut(ConnectionFileObject, SendBuffer, SendLength, Flags, Result, TimeOut);
	}
}

NTSTATUS
LpxTdiSend(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	) {
	return LpxTdiSend_TimeOut(ConnectionFileObject, SendBuffer, SendLength, Flags, Result, NULL);
}


NTSTATUS
LpxTdiSend_TimeOut(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result,
	IN	PLARGE_INTEGER	TimeOut
	)
{
	KEVENT				event;
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			ntStatus;
	PMDL				mdl;
	
    LtDebugPrint (3, ("LpxTdiSend:  Entered\n"));

	//
	//	Send bytes is restricted below 64 KBytes.
	//	Removed.
	//	Now NDAS service will control request data size.
	//	Write size will be controlled by retransmits
//	SendLength = SendLength > LPXTDI_BYTEPERPACKET?LPXTDI_BYTEPERPACKET:SendLength;

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
		LtDebugPrint(1, ("[LpxTdi]TdiSend: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
/*
	try {	
		//
		// Make MDL.
		//
*/
		mdl = IoAllocateMdl(
				SendBuffer,
				SendLength,
				FALSE,
				FALSE,
				irp
				);
		if(mdl == NULL) {
			LtDebugPrint(1, ("[LpxTdi]TdiSend: Can't Allocate MDL.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
//		mdl->Next = NULL;
		MmBuildMdlForNonPagedPool(mdl);


/*

  		MmProbeAndLockPages(
			mdl,
			KernelMode,
			IoReadAccess
			);
			
    
	} except (EXCEPTION_EXECUTE_HANDLER) {
		LtDebugPrint(1, ("[LpxTdi]TdiSend: Can't Convert Non-Paged Memory MDL.\n"));
		if(mdl){
			IoFreeMdl(mdl);
		}
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if(!MmIsNonPagedSystemAddressValid(MmGetMdlVirtualAddress(mdl)))
	{
		if(mdl){
			IoFreeMdl(mdl);
		};		 
		return STATUS_INSUFFICIENT_RESOURCES;
	}
*/
	TdiBuildSend(
		irp,
		deviceObject,
		ConnectionFileObject,
		LpxTdiSendCompletionRoutine,
		NULL,
		mdl,
		Flags,
		SendLength
		);
	
	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				TimeOut
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSend: Failed.\n"));
		*Result = -1;
		return ntStatus;
	}
	*Result = ioStatusBlock.Information;

	return ntStatus;
}


NTSTATUS
LpxTdiSendEx(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	PUCHAR			SendBuffer,
	IN 	ULONG			SendLength,
	IN	ULONG			Flags,
	IN	PKEVENT			CompEvent,
	IN	PLARGE_INTEGER	TimeOut,
	IN OUT PVOID		CompletionContext,
	OUT	PIO_STATUS_BLOCK	IoStatusBlock
	)
{
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	NTSTATUS			ntStatus;
	PMDL				mdl;

    LtDebugPrint (3, ("[LPXTDI]LpxTdiSendEx:  Entered\n"));

    deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	irp = TdiBuildInternalDeviceControlIrp(
				TDI_SEND,
				deviceObject,
				ConnectionFileObject,
				CompEvent,
				IoStatusBlock
				);
	if(irp == NULL) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendEx: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	mdl = IoAllocateMdl(
				SendBuffer,
				SendLength,
				FALSE,
				FALSE,
				irp
				);
	if(mdl == NULL) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendEx: Can't Allocate MDL.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
//	mdl->Next = NULL;
	MmBuildMdlForNonPagedPool(mdl);


	if(CompletionContext) {
		TdiBuildSend(
			irp,
			deviceObject,
			ConnectionFileObject,
			LpxTdiSendCompletionRoutine,
			CompletionContext,
			mdl,
			Flags,
			SendLength
			);
	} else {
		TdiBuildSend(
			irp,
			deviceObject,
			ConnectionFileObject,
			NULL,
			NULL,
			mdl,
			Flags,
			SendLength
			);
	}
	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				IoStatusBlock,
				CompEvent,
				TimeOut
				);
	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendEx: Failed.\n"));
	}

	return ntStatus;
}


NTSTATUS
LpxTdiRecv_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject,
		OUT	PUCHAR			RecvBuffer,
		IN	ULONG			RecvLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext,
		IN PLARGE_INTEGER	TimeOut
	) {

	UNREFERENCED_PARAMETER(TimeOut);

	if(CompletionContext)
		return LpxTdiRecvWithCompletionEvent_TimeOut(ConnectionFileObject, CompletionContext,RecvBuffer,RecvLength, Flags, TimeOut);
	else
		return LpxTdiRecv_TimeOut(ConnectionFileObject, RecvBuffer,RecvLength, Flags, Result, TimeOut);
}


NTSTATUS
LpxTdiRecv(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLONG			Result
	) {
	return LpxTdiRecv_TimeOut(ConnectionFileObject, RecvBuffer, RecvLength, Flags, Result, NULL);
}


NTSTATUS
LpxTdiRecv_TimeOut(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLONG			Result,
	IN	PLARGE_INTEGER	TimeOut
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
		LtDebugPrint(1, ("[LpxTdi]TdiReceive: Rcv buffer == NULL or RcvLen == 0.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
    LtDebugPrint (3, ("LpxTdiRecv:  Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecv_TimeOut: Can't Build IRP.\n"));
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
			LtDebugPrint(1, ("[LpxTdi]LpxTdiRecv_TimeOut: Can't Allocate MDL.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

//		mdl->Next = NULL;
		MmBuildMdlForNonPagedPool(mdl);
/*
		MmProbeAndLockPages(
			mdl,
			KernelMode,
			IoWriteAccess
			);
	} except (EXCEPTION_EXECUTE_HANDLER) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecv_TimeOut: Can't Convert Non-Paged Memory MDL.\n"));
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
		LpxTdiRcvCompletionRoutine,
		NULL,
		mdl,
		Flags,
		RecvLength
		);
	
	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				TimeOut
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecv_TimeOut: Failed.\n"));
		*Result = -1;
		return ntStatus;
	}

	*Result = ioStatusBlock.Information;

	return ntStatus;
}

NTSTATUS
LpxSetDisconnectHandler(
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

    LtDebugPrint (3, ("[LpxTdi]LpxSetDisconnectHandler:  Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi]LpxSetDisconnectHandler: Can't Build IRP.\n"));
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

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxSetDisconnectHandler: Failed.\n"));
	}

	return ntStatus;
}

NTSTATUS
LpxTdiGetAddressList_LSTrans(
		IN	ULONG				AddressListLen,
		IN	PTA_LSTRANS_ADDRESS	AddressList,
		OUT	PULONG				OutLength
	) {
	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	NTSTATUS				status;
	LONG					idx_addr;
	ULONG					length;

	status = LpxTdiGetAddressList(&socketLpxAddressList);
	if(!NT_SUCCESS(status))
		goto error_out;

	length = FIELD_OFFSET(TA_LSTRANS_ADDRESS, Address) +
		sizeof(struct  _AddrLstrans) * socketLpxAddressList.iAddressCount;
	if(AddressListLen < length) {
		if(OutLength) {
			*OutLength = length;
		}
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	translate LPX address list into LSTrans address list.
	//
	AddressList->TAAddressCount = socketLpxAddressList.iAddressCount;
	for(idx_addr = 0; idx_addr < socketLpxAddressList.iAddressCount; idx_addr ++) {
		AddressList->Address[idx_addr].AddressType = TDI_ADDRESS_TYPE_LPX;
		AddressList->Address[idx_addr].AddressLength = TDI_ADDRESS_LENGTH_LPX;
		RtlCopyMemory(&AddressList->Address[idx_addr].Address, &socketLpxAddressList.SocketLpx[idx_addr].LpxAddress, TDI_ADDRESS_LENGTH_LPX);
	}

	if(OutLength) {
		*OutLength = length;
	}

error_out:

	return status;
}

NTSTATUS
LpxTdiGetAddressList(
	IN OUT	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList
    ) 
{
	NTSTATUS				ntStatus;
	HANDLE					ControlFileHandle;
	PFILE_OBJECT			ControlFileObject;
	int						addr_idx;

	socketLpxAddressList->iAddressCount = 0;

	ntStatus = LpxTdiOpenControl (
			&ControlFileHandle, 
			&ControlFileObject,
			NULL
	);
	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiGetAddressList: LpxTdiOpenControl() failed\n"));
		return ntStatus;
	}

	ntStatus = LpxTdiIoControl (
						ControlFileHandle,
						ControlFileObject,
						IOCTL_TCP_QUERY_INFORMATION_EX,
	                    NULL,
						0,
						socketLpxAddressList,
				        sizeof(SOCKETLPX_ADDRESS_LIST)
	);
	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiGetAddressList: LpxTdiIoControl() failed\n"));
		return ntStatus;
	}

	if(socketLpxAddressList->iAddressCount == 0) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiGetAddressList: No address.\n"));
		goto out;
	}

	LtDebugPrint(2, ("[LpxTdi] LpxTdiGetAddressList: count = %ld\n", socketLpxAddressList->iAddressCount));
	for(addr_idx = 0; addr_idx < socketLpxAddressList->iAddressCount; addr_idx ++) {

		LtDebugPrint(2, ("\t%d. %02x:%02x:%02x:%02x:%02x:%02x/%d\n",
				addr_idx,
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[0],
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[1],
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[2],
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[3],
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[4],
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[5],
				socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Port
			) );
	}

out:
	LpxTdiCloseControl(ControlFileHandle, ControlFileObject);

	return STATUS_SUCCESS;

}


NTSTATUS
LpxTdiListenCompletionRoutine(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp,
	IN	PTDI_LISTEN_CONTEXT		TdiListenContext					
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);


	LtDebugPrint(1, ("[LpxTdi]LpxTdiListenCompletionRoutine\n"));

	TdiListenContext->Status = Irp->IoStatus.Status;

	if(Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		PTRANSPORT_ADDRESS	tpAddr;
		PTA_ADDRESS			taAddr;
		PLPX_ADDRESS		lpxAddr;

		
		tpAddr = (PTRANSPORT_ADDRESS)TdiListenContext->AddressBuffer;
		taAddr = tpAddr->Address;
		lpxAddr = (PLPX_ADDRESS)taAddr->Address;
		
		RtlCopyMemory(&TdiListenContext->RemoteAddress, lpxAddr, sizeof(LPX_ADDRESS) );
	} else {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiListenCompletionRoutine: Listen IRP completed with an error.\n"));
	}

	KeSetEvent(&TdiListenContext->CompletionEvent, IO_NETWORK_INCREMENT, FALSE);	
	TdiListenContext->Irp = NULL;
	
	return STATUS_SUCCESS;
}


// You must wait Completion Event If not, It can be crashed !!!
NTSTATUS
LpxTdiListen_LSTrans(
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG				Flags,
	IN	PLARGE_INTEGER		TimeOut
	) {

	UNREFERENCED_PARAMETER(TimeOut);

	return	LpxTdiListenWithCompletionEvent(ConnectionFileObject, TdiListenContext, Flags);
}

NTSTATUS
LpxTdiListenWithCompletionEvent(
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG				Flags
	)
{
    PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	NTSTATUS			ntStatus;

    LtDebugPrint (1, ("[LpxTdi]LpxTdiListen: Entered.\n"));

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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiListen: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiListenContext->RequestConnectionInfo.UserData = NULL;
	TdiListenContext->RequestConnectionInfo.UserDataLength = 0;
	TdiListenContext->RequestConnectionInfo.Options =  Flags;
	TdiListenContext->RequestConnectionInfo.OptionsLength = sizeof(ULONG);
	TdiListenContext->RequestConnectionInfo.RemoteAddress = NULL;
	TdiListenContext->RequestConnectionInfo.RemoteAddressLength = 0;

	TdiListenContext->ReturnConnectionInfo.UserData = NULL;
	TdiListenContext->ReturnConnectionInfo.UserDataLength = 0;
	TdiListenContext->ReturnConnectionInfo.Options =  Flags;
	TdiListenContext->ReturnConnectionInfo.OptionsLength = sizeof(ULONG);
	TdiListenContext->ReturnConnectionInfo.RemoteAddress = TdiListenContext->AddressBuffer;
	TdiListenContext->ReturnConnectionInfo.RemoteAddressLength = TPADDR_LPX_LENGTH;

	TdiBuildListen(
			irp,
			deviceObject,
			ConnectionFileObject,
			LpxTdiListenCompletionRoutine,
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiListen: Failed.\n"));
		return ntStatus;
	}
	TdiListenContext->Irp = irp;
	
	return ntStatus;
}



NTSTATUS
LpxTdiRecvWithCompletionEventCompletionRoutine(
	IN	PDEVICE_OBJECT			DeviceObject,
	IN	PIRP					Irp,
	IN	PTDI_RECEIVE_CONTEXT	TdiReceiveContext
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	if(Irp->MdlAddress != NULL) {
//		MmUnlockPages(Irp->MdlAddress);
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		LtDebugPrint(1, ("[LpxTdi]KSCompletionRoutine: Mdl is NULL!!!\n"));
	}

	TdiReceiveContext->Result = Irp->IoStatus.Information;
	if(Irp->IoStatus.Status != STATUS_SUCCESS)
		TdiReceiveContext->Result = -1;

	KeSetEvent(&TdiReceiveContext->CompletionEvent, IO_NETWORK_INCREMENT, FALSE);
	TdiReceiveContext->Irp = NULL;

	return STATUS_SUCCESS;
}

NTSTATUS
LpxTdiRecvWithCompletionEvent(
	IN	PFILE_OBJECT			ConnectionFileObject,
	IN  PTDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT	PUCHAR					RecvBuffer,
	IN	ULONG					RecvLength,
	IN	ULONG					Flags
	) {

	return LpxTdiRecvWithCompletionEvent_TimeOut(ConnectionFileObject, TdiReceiveContext, RecvBuffer, RecvLength, Flags, NULL);
}

NTSTATUS
LpxTdiRecvWithCompletionEvent_TimeOut(
	IN	PFILE_OBJECT			ConnectionFileObject,
	IN  PTDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT	PUCHAR					RecvBuffer,
	IN	ULONG					RecvLength,
	IN	ULONG					Flags,
	IN	PLARGE_INTEGER			TimeOut
	)
{
    PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
	NTSTATUS				ntStatus;
	PMDL					mdl;


	ASSERT(ConnectionFileObject);
    LtDebugPrint (3, ("LpxTdiRecvWithCompletionEvent_TimeOut:  Entered\n"));

	if((RecvBuffer == NULL) || (RecvLength == 0))
	{
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecvWithCompletionEvent_TimeOut: Rcv buffer == NULL or RcvLen == 0.\n"));
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecvWithCompletionEvent_TimeOut: Can't Build IRP.\n"));
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecvWithCompletionEvent_TimeOut: Can't Allocate MDL.\n"));
		IoFreeIrp(irp);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

//	mdl->Next = NULL;
	MmBuildMdlForNonPagedPool(mdl);
	
	TdiBuildReceive(
		irp,
		deviceObject,
		ConnectionFileObject,
		LpxTdiRecvWithCompletionEventCompletionRoutine,
		TdiReceiveContext,
		mdl,
		Flags,
		RecvLength
		);
	

	if(TimeOut) 
		SET_IRP_EXPTIME(irp, CurrentTime().QuadPart + TimeOut->QuadPart);
	else
		SET_IRP_EXPTIME(irp, 0);

	ntStatus = IoCallDriver(
				deviceObject,
				irp
				);

	if(!NT_SUCCESS(ntStatus)) 
	{
		TdiReceiveContext->Irp = NULL;
		LtDebugPrint(1, ("[LpxTdi]LpxTdiRecvWithCompletionEvent_TimeOut: Failed.\n"));
	
		return ntStatus;
	}

	TdiReceiveContext->Irp = irp;
	
	return ntStatus;
}

NTSTATUS
LpxTdiSendDataGram_LSTrans(
		IN	PFILE_OBJECT	AddressFileObject,
		PTA_LSTRANS_ADDRESS	RemoteAddress,
		IN	PUCHAR			SendBuffer,
		IN 	ULONG			SendLength,
		IN	ULONG			Flags,
		OUT	PLONG			Result,
		IN OUT PVOID		CompletionContext
	) {

	UNREFERENCED_PARAMETER(CompletionContext);

	return LpxTdiSendDataGram(AddressFileObject, (PLPX_ADDRESS)&RemoteAddress->Address[0].Address, SendBuffer, SendLength, Flags, Result);
}

NTSTATUS
LpxTdiSendDataGram(
	IN	PFILE_OBJECT	AddressFileObject,
	PLPX_ADDRESS		LpxRemoteAddress,
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
    TDI_CONNECTION_INFORMATION  SendDatagramInfo;
	UCHAR				AddrBuffer[256];
	PTRANSPORT_ADDRESS	RemoteAddress = (PTRANSPORT_ADDRESS)AddrBuffer;
	
	UNREFERENCED_PARAMETER(Flags);

    LtDebugPrint (3, ("LpxTdiSendDataGram:  Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendDataGram: Can't Build IRP.\n"));
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
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendDataGram: Can't Allocate MDL.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	MmBuildMdlForNonPagedPool(mdl);
//	mdl->Next = NULL;
//	MmProbeAndLockPages(
//		mdl,
//		KernelMode,
//		IoReadAccess
//		);

	RemoteAddress->TAAddressCount = 1;
    RemoteAddress->Address[0].AddressType	= TDI_ADDRESS_TYPE_LPX;
    RemoteAddress->Address[0].AddressLength	= TDI_ADDRESS_LENGTH_LPX;
	RtlCopyMemory(RemoteAddress->Address[0].Address, LpxRemoteAddress, sizeof(LPX_ADDRESS));


	SendDatagramInfo.UserDataLength = 0;
	SendDatagramInfo.UserData = NULL;
	SendDatagramInfo.OptionsLength = 0;
	SendDatagramInfo.Options = NULL;
	SendDatagramInfo.RemoteAddressLength =	TPADDR_LPX_LENGTH;
	SendDatagramInfo.RemoteAddress = RemoteAddress;
	

	TdiBuildSendDatagram(
		irp,
		deviceObject,
		AddressFileObject,
		LpxTdiSendCompletionRoutine,
		NULL,
		mdl,
		SendLength,
		&SendDatagramInfo
    );

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiSendDataGram: Failed.\n"));
		*Result = -1;
		return ntStatus;
	}
	*Result = ioStatusBlock.Information;

	return ntStatus;
}


NTSTATUS
LpxTdiSetReceiveDatagramHandler(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	) {
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

    LtDebugPrint (1, ("[LxpTdi]LpxSetReceiveDatagramHandler:  Entered\n"));

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
		LtDebugPrint(1, ("[LpxTdi]LpxSetReceiveDatagramHandler: Can't Build IRP.\n"));
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

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				&ioStatusBlock,
				&event,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) {
		LtDebugPrint(1, ("[LpxTdi]LpxSetReceiveDatagramHandler: Can't Build IRP.\n"));
	}

	LtDebugPrint(3, ("[LpxTdi] Leave LpxSetReceiveDatagramHandler\n"));

	return ntStatus;
}

