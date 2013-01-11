/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include <ndis.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <ntintsafe.h>

#include "LSKLib.h"
#include "LSTransport.h"
#include "lpxtdi.h"

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
LpxTdiCloseAddress_LSTrans (
		IN HANDLE		AddressFileHandle,
		IN PFILE_OBJECT	AddressFileObject
	);

NTSTATUS
LpxTdiOpenConnection_LSTrans(
		IN PVOID			ConnectionContext,
		OUT PHANDLE			ConnectionFileHandle,
		OUT	PFILE_OBJECT	*ConnectionFileObject
	);

NTSTATUS
LpxTdiCloseConnection_LSTrans (
		IN	HANDLE			ConnectionFileHandle,
		IN	PFILE_OBJECT	ConnectionFileObject
	);

NTSTATUS
LpxTdiAssociateAddress_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	HANDLE		AddressFileHandle
	);

NTSTATUS
LpxTdiDisassociateAddress_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject
	);

NTSTATUS
LpxTdiConnect_LSTrans(
		IN PFILE_OBJECT			ConnectionFileObject,
		IN PTA_LSTRANS_ADDRESS	Address,
		IN PLARGE_INTEGER		TimeOut,
		IN PLSTRANS_OVERLAPPED	OverlappedData
	);

NTSTATUS
LpxTdiDisconnect_LSTrans(
		IN	PFILE_OBJECT	ConnectionFileObject,
		IN	ULONG			Flags
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
		IN PFILE_OBJECT			ConnectionFileObject,
		IN PUCHAR				SendBuffer,
		IN ULONG				SendLength,
		IN ULONG				Flags,
		IN PLARGE_INTEGER		TimeOut,
		OUT PLS_TRANS_STAT		TransStat,
		OUT PLONG				Result,
		IN PLSTRANS_OVERLAPPED	OverlappedData
	);

NTSTATUS
LpxTdiRecv_LSTrans(
		IN	PFILE_OBJECT		ConnectionFileObject,
		OUT	PUCHAR				RecvBuffer,
		IN	ULONG				RecvLength,
		IN	ULONG				Flags,
		IN PLARGE_INTEGER		TimeOut,
		OUT PLS_TRANS_STAT		TransStat,
		OUT	PLONG				Result,
		IN PLSTRANS_OVERLAPPED	OverlappedData
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
		IN    ULONG			OutputBufferLength
	);

NTSTATUS
LpxTdiSetReceiveDatagramHandler_LSTrans(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	);

NTSTATUS
LpxSetDisconnectHandler_LSTrans(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
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
			LpxTdiCloseAddress_LSTrans,
			LpxTdiOpenConnection_LSTrans,
			LpxTdiCloseConnection_LSTrans,
			LpxTdiAssociateAddress_LSTrans,
			LpxTdiDisassociateAddress_LSTrans,
			LpxTdiConnect_LSTrans,
			LpxTdiDisconnect_LSTrans,
			LpxTdiListen_LSTrans,
			LpxTdiSend_LSTrans,
			LpxTdiRecv_LSTrans,
			LpxTdiSendDataGram_LSTrans,
			LpxTdiSetReceiveDatagramHandler_LSTrans,
			LpxSetDisconnectHandler_LSTrans,
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

//////////////////////////////////////////////////////////////////////////
//
//	Common to both LSTRANS and non-LSTRANS
//

//
//	get the current system clock
//
static
__inline
LARGE_INTEGER
CurrentTime(VOID){
	LARGE_INTEGER Time;
	ULONG		Tick;

	KeQueryTickCount(&Time);
	Tick = KeQueryTimeIncrement();
	Time.QuadPart = Time.QuadPart * Tick;

	return Time;
}


//
//	Send IRP to the target device.
//	It will wait for the event instead of the caller if a user event is set.
//
//	Arguments:
//		DeviceObject - target device object
//		Irp - IRP to be sent
//		TransStat - additional return information from the target
//		TimeOut - IRP should be completed until this time-out
//		Synchronous - TRUE if the caller wants to perform synchronous IO.
//
//	Return:
//		STATUS_SUCCESS - the IRP succeeds synchronously
//		STATUS_PENDING - the target device object returns PENDING,
//						but a user event is not set
//		STATUS_INVALID_PARAMETER - If IoStatusBlock is not set
//		STATUS_CONNECTION_DISCONNECTED - event-waiting failure
//		STATUS_* - the target device return an error.
//

NTSTATUS
LpxTdiIoCallDriver(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	OUT PTRANS_STAT		TransStat,
	IN PLARGE_INTEGER	TimeOut,
	IN BOOLEAN			Synchronous
){
	NTSTATUS			ntStatus;
	PKEVENT				userEvent;
	PIO_STATUS_BLOCK	userIoStatusBlock;

	//
	//	set a expire time to IRP.
	//	LPX takes whole charge of IRP completion.
	//	LPX will measure time-out.
	//	Do not wait with time-out here to prevent BSOD.
	//
	if(TimeOut)
		SET_IRP_EXPTIME(Irp, CurrentTime().QuadPart + TimeOut->QuadPart);
	else
		SET_IRP_EXPTIME(Irp, 0);

	Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_FOR_TRANS_STAT] = (PVOID)TransStat; // This entry is used for getting flow control hint.

	LtDebugPrint(2, ("[LpxTdi] LpxTdiIoCallDriver: Irp->Tail.Overlay.DriverContext[2] == %p\n",
		Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_FOR_TRANS_STAT]));


	//
	//	Extract event and IoStatusBlock set by the caller
	//

	if(Synchronous) {
		userEvent = Irp->UserEvent;
		userIoStatusBlock = Irp->UserIosb;
		if(userIoStatusBlock == NULL) {
			return STATUS_INVALID_PARAMETER;
		}
		if(Synchronous && userEvent == NULL) {
			return STATUS_INVALID_PARAMETER;
		}
	} else {
		userEvent = NULL;
		userIoStatusBlock = NULL;
	}


	//
	//	Send IRP to the target device object.
	//

	ntStatus = IoCallDriver(
				DeviceObject,
				Irp
				);

	if(ntStatus == STATUS_PENDING) {
		NTSTATUS			wait_status;

		//
		//	Target device is doing the request asynchronously.
		//	If user event is set, wait for it instead of the caller.
		//

		if(Synchronous) {
			wait_status = KeWaitForSingleObject(
					userEvent,
					Executive,
					KernelMode,
					FALSE,
					NULL
					);

			if(wait_status == STATUS_SUCCESS) {
				return userIoStatusBlock->Status;
			} else {
				LtDebugPrint(1, ("[LpxTdi] LpxTdiIoCallDriver: Wait for event Failed.\n"));
				ASSERT(FALSE);
				return STATUS_CONNECTION_DISCONNECTED;
			}
		}

	} else {


		//
		//	the IRP is completed synchronously
		//

		//
		//	If return value from IoCallDriver() is SUCCESS,
		//	Make IoStatusBlock and return value same 
		//  by returning userIoStatusBlock.
		//	If not, forwards the return value.
		//

		if (ntStatus == STATUS_SUCCESS) {
			if(userIoStatusBlock) {
				ASSERT(userIoStatusBlock->Status != STATUS_PENDING);
				return userIoStatusBlock->Status;
			}
		} else {
			if(userIoStatusBlock) {
				userIoStatusBlock->Status = ntStatus;
			}
		}
	}

	return ntStatus;
}

//
//	Completion routine with overlapped data
//

NTSTATUS
IrpCompletionForUserNotification(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
){
	PLSTRANS_OVERLAPPED	overlapped = (PLSTRANS_OVERLAPPED)Context;

	UNREFERENCED_PARAMETER(DeviceObject);

	//
	//	Clean up MDLs
	//

	if(Irp->MdlAddress != NULL) {
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		LtDebugPrint(1, ("[LpxTdi] IrpCompletionForUserNotification: Mdl is NULL!!!\n"));
	}

	//
	//	Copy user status block
	//

	if(Irp->UserIosb) {
		*Irp->UserIosb = Irp->IoStatus;
	}

	//
	//	Call the user's routine if overlapped data exists
	//

	if(overlapped) {
		//
		// Free LPXTDI's buffer
		//

		if(overlapped->SystemUse) {
			ExFreePoolWithTag(overlapped->SystemUse, LPXTDI_COMPBUFF_POOLTAG);
		}

		//
		// Call user-supplied completion routine.
		//

		overlapped->IoStatusBlock = Irp->IoStatus;
		if(overlapped->IoCompleteRoutine)
			overlapped->IoCompleteRoutine(overlapped);
	}

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
//	LSTRANS support functions
//


NTSTATUS
LpxTdiIoControl_LSTRans (
		IN	ULONG			IoControlCode,
		IN	PVOID			InputBuffer OPTIONAL,
		IN	ULONG			InputBufferLength,
		OUT PVOID			OutputBuffer OPTIONAL,
		IN    ULONG			OutputBufferLength
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
LpxTdiOpenAddress_LSTrans(
		IN	PTA_LSTRANS_ADDRESS		LstransAddress,
		OUT	PHANDLE					AddressFileHandle,
		OUT	PFILE_OBJECT			*AddressFileObject
	)
{
	HANDLE						addressFileHandle; 
	PFILE_OBJECT				addressFileObject;

	UNICODE_STRING	    		nameString;
	OBJECT_ATTRIBUTES	    	objectAttributes;
	UCHAR						eaFullBuffer[ADDRESS_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	PTRANSPORT_ADDRESS			transportAddress;
	PTA_ADDRESS	    			taAddress;
	PTDI_ADDRESS_LPX			lpxAddress;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
	NTSTATUS	    			status;

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
	eaBuffer->Flags	    		= 0;
	eaBuffer->EaNameLength	    = TDI_TRANSPORT_ADDRESS_LENGTH;
	eaBuffer->EaValueLength		= FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
									+ FIELD_OFFSET(TA_ADDRESS, Address)
									+ TDI_ADDRESS_LENGTH_LPX;

	for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
		eaBuffer->EaName[i] = TdiTransportAddress[i];
	}

	transportAddress = (PTRANSPORT_ADDRESS)&eaBuffer->EaName[eaBuffer->EaNameLength+1];
	transportAddress->TAAddressCount = 1;

	taAddress = (PTA_ADDRESS)transportAddress->Address;
	taAddress->AddressType	    = TDI_ADDRESS_TYPE_LPX;
	taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

	lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

	RtlCopyMemory(
		lpxAddress,
		&LstransAddress->Address[0].Address,
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

		if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
			LtDebugPrint (0,("[LpxTdi] TdiOpenAddress: FAILURE, NtCreateFile returned status code=%x.\n", status));
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}

	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {

		if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
			LtDebugPrint (0, ("[LpxTdi] TdiOpenAddress: FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
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
		LtDebugPrint(0,("[LpxTdi] TdiOpenAddress: ObReferenceObjectByHandle() failed. STATUS=%08lx\n", status));
		ZwClose(addressFileHandle);
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}
	
	*AddressFileHandle = addressFileHandle;
	*AddressFileObject = addressFileObject;

	LtDebugPrint (3, ("[LpxTdi] TdiOpenAddress: returning\n"));

	return status;
}

NTSTATUS
LpxTdiCloseAddress_LSTrans (
	IN HANDLE		AddressFileHandle,
	IN PFILE_OBJECT	AddressFileObject
	)
{
	NTSTATUS status;

	if(AddressFileObject)
		ObDereferenceObject(AddressFileObject);

	if(AddressFileHandle == NULL)
		return STATUS_SUCCESS;

	status = ZwClose (AddressFileHandle);

	if (!NT_SUCCESS(status)) {
		LtDebugPrint (0, ("[LpxTdi] CloseAddress: FAILURE, NtClose returned status code=%x\n", status));
	} else {
		LtDebugPrint (3, ("[LpxTdi] CloseAddress: NT_SUCCESS.\n"));
	}

	return status;
}

NTSTATUS
LpxTdiOpenConnection_LSTrans (
		IN PVOID			ConnectionContext,
		OUT PHANDLE			ConnectionFileHandle,
		OUT	PFILE_OBJECT	*ConnectionFileObject
	)
{
	HANDLE						connectionFileHandle; 
	PFILE_OBJECT				connectionFileObject;

	UNICODE_STRING	    		nameString;
	OBJECT_ATTRIBUTES	    	objectAttributes;
	UCHAR						eaFullBuffer[CONNECTION_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	INT							i;
	IO_STATUS_BLOCK				ioStatusBlock;
	NTSTATUS	    			status;
	

	LtDebugPrint (3, ("[LpxTdi] LpxTdiOpenConnection: Entered\n"));

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
	eaBuffer->Flags	    		= 0;
	eaBuffer->EaNameLength	    = TDI_CONNECTION_CONTEXT_LENGTH;
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
		LtDebugPrint (0, ("[LpxTdi] TdiOpenConnection: FAILURE, NtCreateFile returned status code=%x\n", status));
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
		return status;
	}

	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {
		LtDebugPrint (0, ("[LpxTdi] TdiOpenConnection: FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
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
		LtDebugPrint(0, ("[LpxTdi] TdiOpenConnection: ObReferenceObjectByHandle() FAILED %x\n", status));
		ZwClose(connectionFileHandle);
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
		return status;
	}
	 
	*ConnectionFileHandle = connectionFileHandle;
	*ConnectionFileObject = connectionFileObject;

	LtDebugPrint (3, ("[LpxTdi] LpxOpenConnection: returning\n"));

	return status;
}

NTSTATUS
LpxTdiCloseConnection_LSTrans (
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
		LtDebugPrint (0, ("[LpxTdi] LpxCloseConnection: FAILURE, NtClose returned status code=%x\n", status));
	} else {
		LtDebugPrint (3, ("[LpxTdi] LpxCloseConnection: NT_SUCCESS.\n"));
	}

	return status;
}

NTSTATUS
LpxTdiAssociateAddress_LSTrans(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE		AddressFileHandle
	)
{
	KEVENT			event;
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

	LtDebugPrint (3, ("[LpxTdi] LpxTdiAssociateAddress: Entered\n"));

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
				NULL,
				NULL,
				TRUE
				);

#if DBG
	if(ntStatus != STATUS_SUCCESS) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiAssociateAddress: Failed. STATUS=%08lx\n", ntStatus));
	}
#endif

	return ntStatus;
}


NTSTATUS
LpxTdiDisassociateAddress_LSTrans(
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

	LtDebugPrint (3, ("[LpxTdi] LpxTdiDisassociateAddress: Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi] TdiDisassociateAddress: Can't Build IRP.\n"));
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
				NULL,
				NULL,
				TRUE);

#if DBG
	if(ntStatus != STATUS_SUCCESS) {
		LtDebugPrint(1, ("[LpxTdi] TdiDisassociateAddress: Failed. STATUS=%08lx\n", ntStatus));
	}
#endif

	return ntStatus;
}


NTSTATUS
LpxTdiConnect_LSTrans(
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PTA_LSTRANS_ADDRESS	DestAddress,
	IN PLARGE_INTEGER		TimeOut,
	IN PLSTRANS_OVERLAPPED	OverlappedData
){
	KEVENT						event;
	PDEVICE_OBJECT	    		deviceObject;
	PIRP						irp;
	IO_STATUS_BLOCK				ioStatusBlock;

	UCHAR						buffer[
		FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
			+ FIELD_OFFSET(TA_ADDRESS, Address)
			+ TDI_ADDRESS_LENGTH_LPX
	];

	PTRANSPORT_ADDRESS	    	serverTransportAddress;
	PTA_ADDRESS	    			taAddress;
	PTDI_ADDRESS_LPX			addressName;

	NTSTATUS					ntStatus;
	PTDI_CONNECTION_INFORMATION	connectionInfomation;

	LtDebugPrint (3, ("[LpxTdi] LpxTdiConnect_LSTrans: Entered\n"));

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Clear field for LPXTDI usage because users might not initialize to zero.
	//

	if(OverlappedData)
		OverlappedData->SystemUse = NULL;


	//
	// Allocate memory for connection information
	//

	connectionInfomation = ExAllocatePoolWithTag(
				NonPagedPool,
				sizeof(TDI_CONNECTION_INFORMATION),
				LPXTDI_COMPBUFF_POOLTAG);
	if(connectionInfomation == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	//
	// Build an IRP for connect IRP sub-function
	//
	if(OverlappedData == NULL) {
		//
		// Synchronous connect
		//
		// Init an completion event
		//
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		irp = TdiBuildInternalDeviceControlIrp(
			TDI_CONNECT,
			deviceObject,
			connectionFileObject,
			&event,
			&ioStatusBlock
			);
	} else {
		//
		// Asynchronous connect
		//

		irp = TdiBuildInternalDeviceControlIrp(
			TDI_CONNECT,
			deviceObject,
			connectionFileObject,
			NULL,
			NULL
			);

		//
		// Save the connection information buffer to free
		// in the completion routine.
		//

		OverlappedData->SystemUse = (PVOID)connectionInfomation;
	}


	if(irp == NULL) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiConnect_LSTrans: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	serverTransportAddress = (PTRANSPORT_ADDRESS)buffer;
	serverTransportAddress->TAAddressCount = 1;

	taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
	taAddress->AddressType	    = TDI_ADDRESS_TYPE_LPX;
	taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

	addressName = (PTDI_ADDRESS_LPX)taAddress->Address;
	RtlCopyMemory(
		addressName,
		&DestAddress->Address[0].Address,
		TDI_ADDRESS_LENGTH_LPX
		);	

	//
	// Make Connection Info...
	//

	RtlZeroMemory(
		connectionInfomation,
		sizeof(TDI_CONNECTION_INFORMATION)
		);

	connectionInfomation->UserDataLength = 0;
	connectionInfomation->UserData = NULL;
	connectionInfomation->OptionsLength = 0;
	connectionInfomation->Options = NULL;
	connectionInfomation->RemoteAddress = serverTransportAddress;
	connectionInfomation->RemoteAddressLength =	 FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
		+ FIELD_OFFSET(TA_ADDRESS, Address)
		+ TDI_ADDRESS_LENGTH_LPX;

	if(OverlappedData == NULL) {
		TdiBuildConnect(
			irp,
			deviceObject,
			ConnectionFileObject,
			NULL,
			NULL,
			TimeOut,
			connectionInfomation,
			NULL
			);

		//
		// Send the IRP
		//

		ntStatus = LpxTdiIoCallDriver(
			deviceObject,
			irp,
			NULL,
			NULL,
			TRUE);

		//
		// Synchronous connect does not have completion routine.
		// Free the connection information buffer here.
		//
		ExFreePoolWithTag(connectionInfomation, LPXTDI_COMPBUFF_POOLTAG);
	} else {
		TdiBuildConnect(
			irp,
			deviceObject,
			ConnectionFileObject,
			IrpCompletionForUserNotification,
			OverlappedData,
			TimeOut,
			connectionInfomation,
			NULL
			);

		//
		// Send the IRP
		//

		ntStatus = LpxTdiIoCallDriver(
			deviceObject,
			irp,
			NULL,
			NULL,
			FALSE);

		//
		// Connection information buffer will be free in the completion routine.
		//
	}


#if DBG
	if(ntStatus != STATUS_SUCCESS &&
		ntStatus != STATUS_PENDING) {
		LtDebugPrint(1, ("[LpxTdi]LpxTdiConnect_LSTrans: Failed. STATUS=%08lx\n", ntStatus));
	}
#endif

	return ntStatus;
}

NTSTATUS
LpxTdiDisconnect_LSTrans(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	)
{
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

	LtDebugPrint (3, ("LpxTdiDisconnect: Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiDisconnect: Can't Build IRP.\n"));
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
				NULL,
				NULL,
				TRUE);
#if DBG
	if(ntStatus != STATUS_SUCCESS) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiDisconnect: Failed. STATUS = %08lx\n", ntStatus));
	}
#endif

	return ntStatus;
}

// You must wait Completion Event If not, It can be crashed !!!
NTSTATUS
LpxTdiListen_LSTrans(
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG				Flags,
	IN	PLARGE_INTEGER	TimeOut
	) {

	UNREFERENCED_PARAMETER(TimeOut);

	return	LpxTdiListenWithCompletionEvent(ConnectionFileObject, TdiListenContext, Flags);
}


//
// Send data
//

NTSTATUS
LpxTdiSend_LSTrans(
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PUCHAR				SendBuffer,
	IN ULONG				SendLength,
	IN ULONG				Flags,
	IN PLARGE_INTEGER		TimeOut,
	OUT PLS_TRANS_STAT		TransStat,
	OUT PLONG				Result,
	IN PLSTRANS_OVERLAPPED	OverlappedData
){
	PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
	NTSTATUS			ntStatus;
	PMDL				mdl;
	BOOLEAN				synch;
	KEVENT				event;
	IO_STATUS_BLOCK		ioStatusBlock;
	PTRANS_STAT			transStat;

	LtDebugPrint (3, ("[LPXTDI] LpxTdiSend_LSTrans:  Entered\n"));

	//
	//	Parameter check
	//

	if(OverlappedData) {
		if(TransStat || Result) {
			return STATUS_INVALID_PARAMETER;
		}
		transStat = (PTRANS_STAT)OverlappedData->TransStat;
		synch = FALSE;

		//
		// Clear field for LPXTDI usage because users might not initialize to zero.
		//

		OverlappedData->SystemUse = NULL;

	} else {
		transStat = (PTRANS_STAT)TransStat;
		synch = TRUE;
	}

	if((SendBuffer == NULL) || (SendLength == 0))
	{
		LtDebugPrint(1, ("[LpxTdi] LpxTdiSend_LSTrans: SendBuffer == NULL or SendLen == 0.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Get the related device ( LPX's control device )
	//

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);


	//
	// Make IRP.
	//

	if(synch) {
		//	Synchronous
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		irp = TdiBuildInternalDeviceControlIrp(
					TDI_SEND,
					deviceObject,
					ConnectionFileObject,
					&event,
					&ioStatusBlock
					);
	} else {
		irp = TdiBuildInternalDeviceControlIrp(
					TDI_SEND,
					deviceObject,
					ConnectionFileObject,
					NULL,
					NULL
					);
	}
	if(irp == NULL) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiSend_LSTrans: Can't Build IRP.\n"));
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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiSend_LSTrans: Can't Allocate MDL.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	MmBuildMdlForNonPagedPool(mdl);

	TdiBuildSend(
			irp,
			deviceObject,
			ConnectionFileObject,
			IrpCompletionForUserNotification,
			OverlappedData,
			mdl,
			Flags,
			SendLength);

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				transStat,
				TimeOut,
				synch
				);
	if(synch) {
		if(ntStatus == STATUS_SUCCESS) {
			ntStatus = RtlULongPtrToLong(ioStatusBlock.Information, Result);
			ASSERT(NT_SUCCESS(ntStatus));
		} else {
			LtDebugPrint(1, ("[LpxTdi] LpxTdiSend_LSTrans: Failed. STATUS=%08lx\n", ntStatus));
			*Result = 0;
		}
	}
#if DBG
	else {
		if(ntStatus != STATUS_SUCCESS && ntStatus != STATUS_PENDING) {
			LtDebugPrint(1, ("[LpxTdi] LpxTdiSend_LSTrans: Failed. STATUS=%08lx\n", ntStatus));
		}
	}
#endif

	return ntStatus;
}


//
// Receive data
//

NTSTATUS
LpxTdiRecv_LSTrans(
		IN	PFILE_OBJECT		ConnectionFileObject,
		OUT	PUCHAR				RecvBuffer,
		IN	ULONG				RecvLength,
		IN	ULONG				Flags,
		IN PLARGE_INTEGER		TimeOut,
		OUT PLS_TRANS_STAT		TransStat,
		OUT	PLONG				Result,
		IN PLSTRANS_OVERLAPPED	OverlappedData
){
	KEVENT				event;
	PDEVICE_OBJECT	  deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			ntStatus;
	PMDL				mdl;
	BOOLEAN				synch;
	PTRANS_STAT			transStat;
	
	LtDebugPrint (3, ("LpxTdiRecv_LSTrans:  Entered\n"));

	//
	//	Parameter check
	//

	if(OverlappedData) {
		if(TransStat || Result) {
			return STATUS_INVALID_PARAMETER;
		}
		transStat = (PTRANS_STAT)OverlappedData->TransStat;
		synch = FALSE;

		//
		// Clear field for LPXTDI usage because users might not initialize to zero.
		//

		OverlappedData->SystemUse = NULL;
	} else {
		transStat = (PTRANS_STAT)TransStat;
		synch = TRUE;
	}

	if((RecvBuffer == NULL) || (RecvLength == 0))
	{
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecv_LSTrans: Rcv buffer == NULL or RcvLen == 0.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	//
	// Make IRP.
	//
	if(synch) {
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		irp = TdiBuildInternalDeviceControlIrp(
				TDI_RECEIVE,
				deviceObject,
				connectionFileObject,
				&event,
				&ioStatusBlock
				);
	} else {
		irp = TdiBuildInternalDeviceControlIrp(
				TDI_RECEIVE,
				deviceObject,
				connectionFileObject,
				NULL,
				NULL
				);
	}
	if(irp == NULL) {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecv_LSTrans: Can't Build IRP.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl(
				RecvBuffer,
				RecvLength,
				FALSE,
				FALSE,
				irp
				);
		if(mdl == NULL) {
			LtDebugPrint(1, ("[LpxTdi] LpxTdiRecv_LSTrans: Can't Allocate MDL.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

	MmBuildMdlForNonPagedPool(mdl);

	TdiBuildReceive(
			irp,
			deviceObject,
			ConnectionFileObject,
			IrpCompletionForUserNotification,
			OverlappedData,
			mdl,
			Flags,
			RecvLength
			);

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				transStat,
				TimeOut,
				synch
				);

	if(synch) {
		if(ntStatus == STATUS_SUCCESS) {
			ntStatus = RtlULongPtrToLong(ioStatusBlock.Information, Result);
			ASSERT(NT_SUCCESS(ntStatus));
		} else {
			LtDebugPrint(1, ("[LpxTdi] LpxTdiRecv_LSTrans: Failed. STATUS=%08lx\n", ntStatus));
			*Result = 0;
		}
	}
#if DBG
	else {
		if(ntStatus != STATUS_SUCCESS && ntStatus != STATUS_PENDING) {
			LtDebugPrint(1, ("[LpxTdi] LpxTdiSend_LSTrans: Failed. STATUS=%08lx\n", ntStatus));
		}
	}
#endif

	return ntStatus;
}

NTSTATUS
LpxTdiSetReceiveDatagramHandler_LSTrans(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	) {
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

	LtDebugPrint (3, ("[LxpTdi] LpxSetReceiveDatagramHandler:  Entered\n"));

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
		LtDebugPrint(1, ("[LpxTdi] LpxSetReceiveDatagramHandler: Can't Build IRP.\n"));
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
				NULL,
				NULL,
				TRUE
				);
#if DBG
	if(ntStatus != STATUS_SUCCESS) {
		LtDebugPrint(1, ("[LpxTdi] LpxSetReceiveDatagramHandler: "
			"TdiBuildSetEventHandler() failed. STATUS=%08lx\n", ntStatus));
	}
#endif

	return ntStatus;
}

NTSTATUS
LpxSetDisconnectHandler_LSTrans(
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
){
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		ntStatus;

	LtDebugPrint (3, ("[LpxTdi] LpxSetDisconnectHandler:  Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi] LpxSetDisconnectHandler: Can't Build IRP.\n"));
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
				NULL,
				NULL,
				TRUE
				);
#if DBG
	if(ntStatus != STATUS_SUCCESS) {
		LtDebugPrint(1, ("[LpxTdi] LpxSetDisconnectHandler: Failed.\n"));
	}
#endif

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


//////////////////////////////////////////////////////////////////////////
//
//	LPXTDI function for non-LSTRANS applications
//

NTSTATUS
LpxTdiSendDataGramCompletionRoutine(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context					
	)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);


	if(Irp->MdlAddress != NULL) {
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiSendDataGramCompletionRoutine: Mdl is NULL!!!\n"));
	}

	if(Irp->UserIosb) {
		*Irp->UserIosb = Irp->IoStatus;
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

	UNICODE_STRING	    		nameString;
	OBJECT_ATTRIBUTES	    	objectAttributes;
	IO_STATUS_BLOCK				ioStatusBlock;
	NTSTATUS	    			status;
	
	UNREFERENCED_PARAMETER(ControlContext);

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
				NULL,	// Open as control
				0		// 
				);

	if (!NT_SUCCESS(status)) {
		LtDebugPrint (0, ("[LpxTdi] TdiOpenControl: FAILURE, ZwCreateFile returned status code=%x\n", status));
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}

	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {
		LtDebugPrint (0, ("[LpxTdi] TdiOpenControl: FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
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
		LtDebugPrint(0, ("[LpxTdi] LpxOpenControl: ObReferenceObjectByHandle() failed. STATUS=%08lx\n", status));
		ZwClose(controlFileHandle);
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}
	 
	*ControlFileHandle = controlFileHandle;
	*ControlFileObject = controlFileObject;

	LtDebugPrint (3, ("[LpxTdi] LpxOpenControl:  returning\n"));

	return status;
}


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
		LtDebugPrint (0, ("[LpxTdi] LpxCloseControl: FAILURE, NtClose returned status code=%x\n", status));
	} else {
		LtDebugPrint (3, ("[LpxTdi] LpxCloseControl: NT_SUCCESS.\n"));
	}

	return status;
}


NTSTATUS
LpxTdiIoControl (
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject,
	IN	ULONG    		IoControlCode,
	IN	PVOID    		InputBuffer OPTIONAL,
	IN	ULONG    		InputBufferLength,
	OUT PVOID	    	OutputBuffer OPTIONAL,
	IN	ULONG    		OutputBufferLength
	)
{
	NTSTATUS		ntStatus;
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT	    	event;
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
LpxTdiOpenAddress(
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject,
	IN	PTDI_ADDRESS_LPX	Address
	)
{
	TA_LSTRANS_ADDRESS	lstransAddr;

	RtlZeroMemory(&lstransAddr, sizeof(TA_LSTRANS_ADDRESS));
	lstransAddr.TAAddressCount = 1;
	lstransAddr.Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
	lstransAddr.Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
	RtlCopyMemory(lstransAddr.Address[0].Address.Address, Address, TDI_ADDRESS_LENGTH_LPX);

	return LpxTdiOpenAddress_LSTrans(&lstransAddr, AddressFileHandle, AddressFileObject);
}

NTSTATUS
LpxTdiCloseAddress (
	IN HANDLE			AddressFileHandle,
	IN	PFILE_OBJECT	AddressFileObject
	)
{
	return LpxTdiCloseAddress_LSTrans(AddressFileHandle, AddressFileObject);
}


NTSTATUS
LpxTdiOpenConnection (
		OUT PHANDLE			ConnectionFileHandle,
		OUT	PFILE_OBJECT	*ConnectionFileObject,
		IN PVOID			ConnectionContext
	) {

	return LpxTdiOpenConnection_LSTrans(ConnectionContext, ConnectionFileHandle, ConnectionFileObject);
}


NTSTATUS
LpxTdiCloseConnection (
	IN	HANDLE			ConnectionFileHandle,
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
	return LpxTdiCloseConnection_LSTrans (
		ConnectionFileHandle,
		ConnectionFileObject);
}

NTSTATUS
LpxTdiAssociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE		AddressFileHandle
	){
		return LpxTdiAssociateAddress_LSTrans(ConnectionFileObject, AddressFileHandle);
}

NTSTATUS
LpxTdiDisassociateAddress(
	IN	PFILE_OBJECT	ConnectionFileObject
	){
		return LpxTdiDisassociateAddress_LSTrans(ConnectionFileObject);
}


NTSTATUS
LpxTdiConnect(
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN	PTDI_ADDRESS_LPX	Address
){
	TA_LSTRANS_ADDRESS	lstransAddr;

	RtlZeroMemory(&lstransAddr, sizeof(TA_LSTRANS_ADDRESS));
	lstransAddr.TAAddressCount = 1;
	lstransAddr.Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
	lstransAddr.Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
	RtlCopyMemory(lstransAddr.Address[0].Address.Address, Address, TDI_ADDRESS_LENGTH_LPX);

	return LpxTdiConnect_LSTrans(ConnectionFileObject, &lstransAddr, NULL, NULL);
}

NTSTATUS
LpxTdiDisconnect(
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
){
	return LpxTdiDisconnect_LSTrans(ConnectionFileObject, Flags);
}

NTSTATUS
LpxTdiSend(
		   IN PFILE_OBJECT		ConnectionFileObject,
		   IN PUCHAR			SendBuffer,
		   IN ULONG			SendLength,
		   IN ULONG			Flags,
		   IN PLARGE_INTEGER	TimeOut,
		   OUT PTRANS_STAT		TransStat,
		   OUT PLONG			Result
		   ){
   return LpxTdiSend_LSTrans(ConnectionFileObject, SendBuffer, SendLength, Flags, TimeOut, (PLS_TRANS_STAT)TransStat, Result, NULL);
}

NTSTATUS
LpxTdiRecv(
	IN	PFILE_OBJECT	ConnectionFileObject,
	OUT	PUCHAR			RecvBuffer,
	IN	ULONG			RecvLength,
	IN	ULONG			Flags,
	IN	PLARGE_INTEGER	TimeOut,
	OUT PTRANS_STAT	 TransStat,
	OUT	PLONG			Result
){
	return LpxTdiRecv_LSTrans(ConnectionFileObject, RecvBuffer, RecvLength, Flags, TimeOut, (PLS_TRANS_STAT)TransStat, Result, NULL);
}


NTSTATUS
LpxSetDisconnectHandler(
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
){
	return LpxSetDisconnectHandler_LSTrans(
					AddressFileObject,
					InEventHandler,
					InEventContext
				);

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
						IOCTL_LPX_QUERY_ADDRESS_LIST,
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


	LtDebugPrint(1, ("[LpxTdi] LpxTdiListenCompletionRoutine\n"));

	TdiListenContext->Status = Irp->IoStatus.Status;

	if(Irp->UserIosb) {
		*Irp->UserIosb = Irp->IoStatus;
	}

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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiListenCompletionRoutine: Listen IRP completed with an error.\n"));
	}

	KeSetEvent(&TdiListenContext->CompletionEvent, IO_NETWORK_INCREMENT, FALSE);	
	TdiListenContext->Irp = NULL;
	
	return STATUS_SUCCESS;
}


NTSTATUS
LpxTdiListenWithCompletionEvent(
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN  PTDI_LISTEN_CONTEXT	TdiListenContext,
	IN  PULONG				Flags
	)
{
	PDEVICE_OBJECT	    deviceObject;
	PIRP				irp;
	NTSTATUS			ntStatus;

	LtDebugPrint (1, ("[LpxTdi] LpxTdiListenWithCompletionEvent: Entered.\n"));

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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiListenWithCompletionEvent: Can't Build IRP.\n"));
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
        LtDebugPrint(1, ("[LpxTdi] LpxTdiListenWithCompletionEvent: Failed. STATUS=%08lx\n", ntStatus));
        return ntStatus;
    }
    TdiListenContext->Irp = irp;
    
    return ntStatus;
}


NTSTATUS
LpxTdiSendDataGram(
	IN	PFILE_OBJECT	AddressFileObject,
	PLPX_ADDRESS		LpxRemoteAddress,
	IN	PUCHAR			SendBuffer,
	IN	 ULONG			SendLength,
	IN	ULONG			Flags,
	OUT	PLONG			Result
	)
{
	KEVENT				event;
	PDEVICE_OBJECT	    deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	NTSTATUS			ntStatus;
	PMDL				mdl;
	TDI_CONNECTION_INFORMATION  SendDatagramInfo;
	UCHAR				AddrBuffer[256];
	PTRANSPORT_ADDRESS	RemoteAddress = (PTRANSPORT_ADDRESS)AddrBuffer;
	
	UNREFERENCED_PARAMETER(Flags);

	LtDebugPrint (3, ("LpxTdiSendDataGram: Entered\n"));
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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiSendDataGram: Can't Build IRP.\n"));
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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiSendDataGram: Can't Allocate MDL.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	MmBuildMdlForNonPagedPool(mdl);

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
		LpxTdiSendDataGramCompletionRoutine,
		NULL,
		mdl,
		SendLength,
		&SendDatagramInfo
	);

	ntStatus = LpxTdiIoCallDriver(
				deviceObject,
				irp,
				NULL,
				NULL,
				TRUE
				);

	if(ntStatus == STATUS_SUCCESS) {
		*Result = (ULONG)ioStatusBlock.Information;
	} else {
		LtDebugPrint(3, ("[LpxTdi] LpxTdiSendDataGram: Failed. STATUS=%08lx\n", ntStatus));
		*Result = 0;
	}

	return ntStatus;
}


NTSTATUS
LpxTdiSetReceiveDatagramHandler(
		IN	PFILE_OBJECT	AddressFileObject,
		IN	PVOID			InEventHandler,
		IN	PVOID			InEventContext
	)
{
	return LpxTdiSetReceiveDatagramHandler_LSTrans(
					AddressFileObject,
					InEventHandler,
					InEventContext);
}

//
// Completion routine for LpxTdiRecvWithCompletionEvent()
//

NTSTATUS
LpxTdiRecvWithCompletionEventCompletionRoutine(
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp,
	IN PTDI_RECEIVE_CONTEXT	TdiReceiveContext
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	if(Irp->MdlAddress != NULL) {
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecvWithCompletionEventCompletionRoutine: Mdl is NULL!!!\n"));
	}

	TdiReceiveContext->Result = (ULONG)Irp->IoStatus.Information;
	if(Irp->IoStatus.Status != STATUS_SUCCESS)
		TdiReceiveContext->Result = -1;

	KeSetEvent(&TdiReceiveContext->CompletionEvent, IO_NETWORK_INCREMENT, FALSE);
	TdiReceiveContext->Irp = NULL;

	return STATUS_SUCCESS;
}

//
//	OBSOLETE: used only by LfsFilt
//	Receive data asynchronous and set the event to signal at completion
//

NTSTATUS
LpxTdiRecvWithCompletionEvent(
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PTDI_RECEIVE_CONTEXT	TdiReceiveContext,
	OUT PUCHAR				RecvBuffer,
	IN ULONG				RecvLength,
	IN ULONG				Flags,
	OUT PTRANS_STAT	     TransStat,
	IN PLARGE_INTEGER		TimeOut
){
	PDEVICE_OBJECT	    	deviceObject;
	PIRP					irp;
	NTSTATUS				ntStatus;
	PMDL					mdl;


	ASSERT(ConnectionFileObject);
	LtDebugPrint (3, ("LpxTdiRecvWithCompletionEvent:  Entered\n"));

	if((RecvBuffer == NULL) || (RecvLength == 0))
	{
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecvWithCompletionEvent: Rcv buffer == NULL or RcvLen == 0.\n"));
		return STATUS_INVALID_PARAMETER;
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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecvWithCompletionEvent: Can't Build IRP.\n"));
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
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecvWithCompletionEvent: Can't Allocate MDL.\n"));
		IoFreeIrp(irp);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

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

	irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_FOR_TRANS_STAT] = (PVOID)TransStat; 
	ntStatus = IoCallDriver(
				deviceObject,
				irp
				);

	if(!NT_SUCCESS(ntStatus)) 
	{
		TdiReceiveContext->Irp = NULL;
		LtDebugPrint(1, ("[LpxTdi] LpxTdiRecvWithCompletionEvent: Failed.\n"));
	
		return ntStatus;
	}

	TdiReceiveContext->Irp = irp;
	
	return ntStatus;
}


NTSTATUS
LpxTdiQueryInformationCompletionRoutine(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context					
	)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);


	if(Irp->MdlAddress != NULL) {
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	} else {
		LtDebugPrint(1, ("[LpxTdi] LpxTdiQueryInformationCompletionRoutine: Mdl is NULL!!!\n"));
	}

	if(Irp->UserIosb) {
		*Irp->UserIosb = Irp->IoStatus;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
LpxTdiQueryInformation(
	IN	PFILE_OBJECT		ConnectionFileObject,
    IN  ULONG				QType,
	IN  PVOID				Buffer,
	IN  ULONG				BufferLen
	)
{
	NTSTATUS			status;
	KEVENT				event;
	PDEVICE_OBJECT	    deviceObject;
	PIRP				irp;
	IO_STATUS_BLOCK		ioStatusBlock;
	PMDL				mdl;


	if (QType != TDI_QUERY_CONNECTION_INFO)
		return STATUS_NOT_IMPLEMENTED;

	if (BufferLen != sizeof(TDI_CONNECTION_INFO))
		return STATUS_INVALID_PARAMETER;

	LtDebugPrint (3, ("[LpxTdi] LpxTdiQueryInformation: Entered\n"));

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_QUERY_INFORMATION,
											deviceObject,
											connectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {
	
		LtDebugPrint( 1, ("[LpxTdi] LpxTdiQueryInformation: Can't Build IRP.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( Buffer,
						 BufferLen,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		LtDebugPrint( 1, ("[LpxTdi] LpxTdiQueryInformation: Can't Allocate MDL.\n") );
		IoFreeIrp( irp );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	TdiBuildQueryInformation( irp,
							  deviceObject,
							  ConnectionFileObject, 
							  LpxTdiQueryInformationCompletionRoutine,
							  NULL,
							  QType,
							  mdl );

	status = LpxTdiIoCallDriver( deviceObject, irp, NULL, NULL, TRUE );

#if DBG

	if (status != STATUS_SUCCESS) {
	
		LtDebugPrint(1, ("[LpxTdi]LpxTdiQueryInformation: Failed. STATUS=%08lx\n", status));
	}

#endif

	return status;
}

