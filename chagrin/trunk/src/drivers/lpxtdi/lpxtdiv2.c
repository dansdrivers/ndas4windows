/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include <ndis.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <ntintsafe.h>

#include "lpxtdiv2.h"


#pragma warning(error:4100)     //  Enable-Unreferenced formal parameter
#pragma warning(error:4101)     //  Enable-Unreferenced local variable
#pragma warning(error:4061)     //  Eenable-missing enumeration in switch statement
#pragma warning(error:4505)     //  Enable-identify dead functions


#if WINVER >= 0x0501
#define LpxTdiDbgBreakPoint()	(KD_DEBUGGER_ENABLED ? DbgBreakPoint() : TRUE)
#else
#define LpxTdiDbgBreakPoint()	((*KdDebuggerEnabled) ? DbgBreakPoint() : TRUE) 
#endif

#if DBG

#define LPXTDI_ASSERT( exp )	ASSERT( exp )

#else

#define LPXTDI_ASSERT( exp )	\
	((!(exp)) ?					\
	LpxTdiDbgBreakPoint() :		\
	TRUE)

#endif


LONG	DebugLevelV2 = 1;

#if DBG
extern LONG	DebugLevelV2;
#define LtDebugPrint(l, x)			\
		if(l <= DebugLevelV2) {		\
			DbgPrint x; 			\
		}
#else
#define LtDebugPrint(l, x)
#endif

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


NTSTATUS
LpxTdiSynchronousCompletionRoutine (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	)
{
	LtDebugPrint( 3, ("[LpxTdi] LpxTdiSynchronousCompletionRoutine: Mdl is NULL!!!\n") );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );
	
	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
LpxTdiAsynchronousCompletionRoutine (
	IN PDEVICE_OBJECT		DeviceObject,
	IN PIRP					Irp,
	IN PLPXTDI_OVERLAPPED	OverLapped
	)
{
	UNREFERENCED_PARAMETER( DeviceObject );

	OverLapped->IoStatusBlock.Status = Irp->IoStatus.Status;
	OverLapped->IoStatusBlock.Information = Irp->IoStatus.Information;

	LPXTDI_ASSERT( Irp->IoStatus.Status == STATUS_SUCCESS || !NT_SUCCESS(Irp->IoStatus.Status) );

	if (OverLapped->IoCompleteRoutine) {

		OverLapped->IoCompleteRoutine( OverLapped );

		if (Irp->MdlAddress != NULL) {

			IoFreeMdl( Irp->MdlAddress );
			Irp->MdlAddress = NULL;
		}

		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		OverLapped->Irp = NULL;

		return STATUS_SUCCESS;

	} else {

		KeSetEvent( &OverLapped->CompletionEvent, IO_NO_INCREMENT, FALSE );

		return STATUS_MORE_PROCESSING_REQUIRED;
	}
}


VOID
LpxTdiV2CancelIrpRoutine (
	IN PLPXTDI_OVERLAPPED OverLapped 
	)
{
	NTSTATUS		status;
	LARGE_INTEGER	timeOut;


	LPXTDI_ASSERT( OverLapped->Irp );

	IoCancelIrp( OverLapped->Irp );

	timeOut.QuadPart = -100*HZ;

	status = KeWaitForSingleObject( &OverLapped->CompletionEvent, Executive, KernelMode, FALSE, &timeOut );

	LPXTDI_ASSERT( status == STATUS_SUCCESS );

	return;
}


VOID
LpxTdiV2CleanupRoutine (
	IN PLPXTDI_OVERLAPPED OverLapped 
	)
{
	if (OverLapped->Irp) {

		if (OverLapped->Irp->MdlAddress != NULL) {
		
			IoFreeMdl( OverLapped->Irp->MdlAddress );
			OverLapped->Irp->MdlAddress = NULL;
		}

		IoCompleteRequest( OverLapped->Irp, IO_NO_INCREMENT );

		OverLapped->Irp = NULL;
	}

	return;
}



NTSTATUS
LpxTdiV2OpenAddress (
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject,
	IN	PTDI_ADDRESS_LPX	LpxAddress
	)
{
	NTSTATUS	    			status;
	IO_STATUS_BLOCK				ioStatusBlock;

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


	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );


	LtDebugPrint (3, ("[LpxTdi] TdiOpenAddress:  Entered\n"));

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	RtlInitUnicodeString( &nameString, TRANSPORT_NAME );

	InitializeObjectAttributes( &objectAttributes,
								&nameString,
								0,
								NULL,
								NULL );

	RtlZeroMemory( eaBuffer, ADDRESS_EA_BUFFER_LENGTH );
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

	RtlCopyMemory( lpxAddress,
				   LpxAddress,
				   sizeof(TDI_ADDRESS_LPX) );

	status = ZwCreateFile( &addressFileHandle,
						   GENERIC_READ,
						   &objectAttributes,
						   &ioStatusBlock,
						   NULL,
						   FILE_ATTRIBUTE_NORMAL,
						   0,
						   0,
						   0,
						   eaBuffer,
						   ADDRESS_EA_BUFFER_LENGTH );
	
	if (!NT_SUCCESS(status)) {

		if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/) {

			LtDebugPrint (0,("[LpxTdi] TdiOpenAddress: FAILURE, NtCreateFile returned status code=%x.\n", status));
		}
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}

	LPXTDI_ASSERT( status == ioStatusBlock.Status );
	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {

		if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/) {

			LtDebugPrint (0, ("[LpxTdi] TdiOpenAddress: FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		}
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}

	status = ObReferenceObjectByHandle( addressFileHandle,
										0L,
										NULL,
										KernelMode,
										(PVOID *) &addressFileObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint(0,("[LpxTdi] TdiOpenAddress: ObReferenceObjectByHandle() failed. STATUS=%08lx\n", status));
		ZwClose(addressFileHandle);
		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;
		return status;
	}
	
	*AddressFileHandle = addressFileHandle;
	*AddressFileObject = addressFileObject;

	LtDebugPrint( 3, ("[LpxTdi] TdiOpenAddress: returning\n") );

	return status;
}


VOID
LpxTdiV2CloseAddress (
	IN HANDLE		AddressFileHandle,
	IN PFILE_OBJECT	AddressFileObject
	)
{
	NTSTATUS status;

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	LPXTDI_ASSERT( AddressFileHandle && AddressFileObject );

	if (AddressFileObject) {

		ObDereferenceObject( AddressFileObject );
	}

	if (AddressFileHandle == NULL) {

		return;
	}

	status = ZwClose( AddressFileHandle );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint( 0, ("[LpxTdiV2] CloseAddress: FAILURE, NtClose returned status code=%x\n", status) );
	
	} else {
	
		LtDebugPrint( 3, ("[LpxTdiV2] CloseAddress: NT_SUCCESS.\n") );
	}

	return;
}


NTSTATUS
LpxTdiV2OpenConnection (
	OUT PHANDLE			ConnectionFileHandle,
	OUT	PFILE_OBJECT	*ConnectionFileObject,
	IN  PVOID			ConnectionContext
	) 
{
	NTSTATUS	    			status;
	IO_STATUS_BLOCK				ioStatusBlock;

	HANDLE						connectionFileHandle; 
	PFILE_OBJECT				connectionFileObject;

	UNICODE_STRING	    		nameString;
	OBJECT_ATTRIBUTES	    	objectAttributes;
	UCHAR						eaFullBuffer[CONNECTION_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION	eaBuffer = (PFILE_FULL_EA_INFORMATION)eaFullBuffer;
	INT							i;


	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2OpenConnection: Entered\n") );

	RtlInitUnicodeString( &nameString, TRANSPORT_NAME );

	InitializeObjectAttributes( &objectAttributes,
								&nameString,
								0,
								NULL,
								NULL );

	
	RtlZeroMemory( eaBuffer, CONNECTION_EA_BUFFER_LENGTH );

	eaBuffer->NextEntryOffset	= 0;
	eaBuffer->Flags	    		= 0;
	eaBuffer->EaNameLength	    = TDI_CONNECTION_CONTEXT_LENGTH;
	eaBuffer->EaValueLength		= sizeof (PVOID);

	for (i=0;i<(int)eaBuffer->EaNameLength;i++) {
	
		eaBuffer->EaName[i] = TdiConnectionContext[i];
	}
	
	RtlMoveMemory( &eaBuffer->EaName[TDI_CONNECTION_CONTEXT_LENGTH+1],
				   &ConnectionContext,
				   sizeof (PVOID) );

	status = ZwCreateFile( &connectionFileHandle,
						   GENERIC_READ,
						   &objectAttributes,
						   &ioStatusBlock,
						   NULL,
						   FILE_ATTRIBUTE_NORMAL,
						   0,
						   0,
						   0,
						   eaBuffer,
						   CONNECTION_EA_BUFFER_LENGTH );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint (0, ("[LpxTdiV2] TdiOpenConnection: FAILURE, NtCreateFile returned status code=%x\n", status));
		
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
		
		return status;
	}

	LPXTDI_ASSERT( status == ioStatusBlock.Status );
	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {

		LtDebugPrint (0, ("[LpxTdiV2] TdiOpenConnection: FAILURE, NtCreateFile returned status code=%x\n", status));
		
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;
		
		return status;
	}

	status = ObReferenceObjectByHandle( connectionFileHandle,
										0L,
										NULL,
										KernelMode,
										(PVOID *) &connectionFileObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint( 0, ("[LpxTdiV2] TdiOpenConnection: ObReferenceObjectByHandle() FAILED %x\n", status) );

		ZwClose( connectionFileHandle );
		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;

		return status;
	}
	 
	*ConnectionFileHandle = connectionFileHandle;
	*ConnectionFileObject = connectionFileObject;

	LtDebugPrint( 3, ("[LpxTdiV2] LpxOpenConnection: returning\n") );

	return status;
}

VOID
LpxTdiV2CloseConnection (
	IN	HANDLE			ConnectionFileHandle,
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
	NTSTATUS status;


	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	LPXTDI_ASSERT( ConnectionFileObject && ConnectionFileHandle );

	if (ConnectionFileObject) {

		ObDereferenceObject( ConnectionFileObject );
	}

	if (!ConnectionFileHandle) {

		return;
	}

	status = ZwClose( ConnectionFileHandle );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint( 0, ("[LpxTdiV2] LpxCloseConnection: FAILURE, NtClose returned status code=%x\n", status) );
	
	} else {
	
		LtDebugPrint( 3, ("[LpxTdiV2] LpxCloseConnection: NT_SUCCESS.\n") );
	}

	return;
}


NTSTATUS
LpxTdiV2AssociateAddress (
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	HANDLE			AddressFileHandle
	)
{
	NTSTATUS		status;

	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2AssociateAddress: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_ASSOCIATE_ADDRESS,
											deviceObject,
											ConnectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiBuildAssociateAddress( irp,
							  deviceObject,
							  ConnectionFileObject,
							  NULL,
							  NULL,
							  AddressFileHandle );

	irp->MdlAddress = NULL;

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = ioStatusBlock.Status;

		}

	} else if (status == STATUS_SUCCESS) {

		status = ioStatusBlock.Status;
	}

	LPXTDI_ASSERT( status == STATUS_SUCCESS );

	return status;
}



VOID
LpxTdiV2DisassociateAddress (
	IN	PFILE_OBJECT	ConnectionFileObject
	)
{
	NTSTATUS		status;

	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2DisassociateAddress: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_DISASSOCIATE_ADDRESS,
											deviceObject,
											ConnectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );
		return;
	}

	TdiBuildDisassociateAddress( irp,
								 deviceObject,
								 ConnectionFileObject,
								 NULL,
								 NULL );

	irp->MdlAddress = NULL;

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = ioStatusBlock.Status;

		}

	} else if (status == STATUS_SUCCESS) {

		status = ioStatusBlock.Status;
	}

	LPXTDI_ASSERT( status == STATUS_SUCCESS );

	return;
}

NTSTATUS
LpxTdiV2SetReceiveDatagramHandler (
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
	)
{
	NTSTATUS		status;

	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2SetReceiveDatagramHandler: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( AddressFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_SET_EVENT_HANDLER,
											deviceObject,
											AddressFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxSetReceiveDatagramHandler: Can't Build IRP.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiBuildSetEventHandler (irp,
							 deviceObject,
							 AddressFileObject,
							 NULL,
							 NULL,
							 TDI_EVENT_RECEIVE_DATAGRAM,
							 InEventHandler,
							 InEventContext );

	irp->MdlAddress = NULL;

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = ioStatusBlock.Status;
		}

	} else if (status == STATUS_SUCCESS) {

		status = ioStatusBlock.Status;
	}

	LPXTDI_ASSERT( status == STATUS_SUCCESS );

	return status;
}

NTSTATUS
LpxTdiV2SetDisconnectHandler (
	IN	PFILE_OBJECT	AddressFileObject,
	IN	PVOID			InEventHandler,
	IN	PVOID			InEventContext
	)
{
	NTSTATUS		status;

	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2SetReceiveDatagramHandler: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( AddressFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_SET_EVENT_HANDLER,
											deviceObject,
											AddressFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxSetDisconnectHandler: Can't Build IRP.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiBuildSetEventHandler (irp,
							 deviceObject,
							 AddressFileObject,
							 NULL,
							 NULL,
							 TDI_EVENT_DISCONNECT,
							 InEventHandler,
							 InEventContext );

	irp->MdlAddress = NULL;

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = ioStatusBlock.Status;

		}

	} else if (status == STATUS_SUCCESS) {

		status = ioStatusBlock.Status;
	}

	LPXTDI_ASSERT( status == STATUS_SUCCESS );

	return status;
}

NTSTATUS
LpxTdiV2Connect (
	IN	PFILE_OBJECT		ConnectionFileObject,
	IN	PTDI_ADDRESS_LPX	LpxAddress,
	IN  PLARGE_INTEGER		TimeOut,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	)
{
	NTSTATUS					status;

	KEVENT						event;
	PDEVICE_OBJECT	    		deviceObject;
	PIRP						irp;

	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];
	PTRANSPORT_ADDRESS	    	serverTransportAddress;
	PTA_ADDRESS	    			taAddress;
	PTDI_ADDRESS_LPX			addressName;

	TDI_CONNECTION_INFORMATION	connectionInfomation;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Connect_LSTrans: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_CONNECT,
											deviceObject,
											connectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->MdlAddress = NULL;

	if (OverlappedData == NULL) {
	
		// Synchronous Mode

		KeInitializeEvent( &event, NotificationEvent, FALSE );

		serverTransportAddress = (PTRANSPORT_ADDRESS)AddressBuffer;
		serverTransportAddress->TAAddressCount = 1;

		taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
		taAddress->AddressType	    = TDI_ADDRESS_TYPE_LPX;
		taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

		addressName = (PTDI_ADDRESS_LPX)taAddress->Address;
		RtlCopyMemory( addressName, LpxAddress, TDI_ADDRESS_LENGTH_LPX );	

		RtlZeroMemory( &connectionInfomation, sizeof(TDI_CONNECTION_INFORMATION) );

		connectionInfomation.UserDataLength = 0;
		connectionInfomation.UserData = NULL;
		connectionInfomation.OptionsLength = 0;
		connectionInfomation.Options = NULL;
		connectionInfomation.RemoteAddress = serverTransportAddress;
		connectionInfomation.RemoteAddressLength =	TPADDR_LPX_LENGTH;

		TdiBuildConnect( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiSynchronousCompletionRoutine,
						 &event,
						 TimeOut,
						 &connectionInfomation,
						 NULL );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;
		}

		LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		LPXTDI_ASSERT( OverlappedData->Irp == NULL );
		LPXTDI_ASSERT( AddressBuffer == NULL );
		LPXTDI_ASSERT( TimeOut == NULL );

		OverlappedData->Irp = irp;

		serverTransportAddress = (PTRANSPORT_ADDRESS)OverlappedData->AddressBuffer;
		serverTransportAddress->TAAddressCount = 1;

		taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
		taAddress->AddressType	    = TDI_ADDRESS_TYPE_LPX;
		taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

		addressName = (PTDI_ADDRESS_LPX)taAddress->Address;
		RtlCopyMemory( addressName, LpxAddress, TDI_ADDRESS_LENGTH_LPX );	

		RtlZeroMemory( &OverlappedData->ConnectionInfomation, sizeof(TDI_CONNECTION_INFORMATION) );

		OverlappedData->ConnectionInfomation.UserDataLength = 0;
		OverlappedData->ConnectionInfomation.UserData = NULL;
		OverlappedData->ConnectionInfomation.OptionsLength = 0;
		OverlappedData->ConnectionInfomation.Options = NULL;
		OverlappedData->ConnectionInfomation.RemoteAddress = serverTransportAddress;
		OverlappedData->ConnectionInfomation.RemoteAddressLength =	TPADDR_LPX_LENGTH;

		TdiBuildConnect( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiAsynchronousCompletionRoutine,
						 OverlappedData,
						 TimeOut,
						 &connectionInfomation,
						 NULL );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}


VOID
LpxTdiV2Disconnect (
	IN	PFILE_OBJECT	ConnectionFileObject,
	IN	ULONG			Flags
	)
{
	NTSTATUS		status;

	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Disconnect: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_DISCONNECT,
											deviceObject,
											ConnectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );
		return;
	}

	TdiBuildDisconnect( irp,
						deviceObject,
						ConnectionFileObject,
						NULL,
						NULL,
						NULL,
						Flags,
						NULL,
						NULL );

	irp->MdlAddress = NULL;

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = ioStatusBlock.Status;

		}

	} else if (status == STATUS_SUCCESS) {

		status = ioStatusBlock.Status;

	}

	LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

	return;
}

NTSTATUS
LpxTdiV2OpenControl (
	OUT PHANDLE			ControlFileHandle, 
	OUT	PFILE_OBJECT	*ControlFileObject,
	IN PVOID			ControlContext
	)
{
	NTSTATUS	    	status;

	HANDLE				controlFileHandle; 
	PFILE_OBJECT		controlFileObject;

	UNICODE_STRING	    nameString;
	OBJECT_ATTRIBUTES	objectAttributes;
	IO_STATUS_BLOCK		ioStatusBlock;
	
	UNREFERENCED_PARAMETER( ControlContext );


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2OpenControl: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	RtlInitUnicodeString( &nameString, TRANSPORT_NAME );

	InitializeObjectAttributes( &objectAttributes,
								&nameString,
								0,
								NULL,
								NULL );

	status = ZwCreateFile( &controlFileHandle,
						   GENERIC_READ,
						   &objectAttributes,
						   &ioStatusBlock,
						   NULL,
						   FILE_ATTRIBUTE_NORMAL,
						   0,
						   0,
						   0,
						   NULL,	// Open as control
						   0 );
	
	if (!NT_SUCCESS(status)) {
	
		LtDebugPrint( 0, ("[LpxTdiV2] TdiOpenControl: FAILURE, ZwCreateFile returned status code=%x\n", status) );
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}

	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {

		LtDebugPrint (0, ("[LpxTdiV2] TdiOpenControl: FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}

	status = ObReferenceObjectByHandle( controlFileHandle,
										0L,
										NULL,
										KernelMode,
										(PVOID *) &controlFileObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint(0, ("[LpxTdiV2] LpxOpenControl: ObReferenceObjectByHandle() failed. STATUS=%08lx\n", status));
		ZwClose(controlFileHandle);
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}
	 
	*ControlFileHandle = controlFileHandle;
	*ControlFileObject = controlFileObject;

	LtDebugPrint( 3, ("[LpxTdiV2] LpxOpenControl:  returning\n") );

	return status;
}


VOID
LpxTdiV2CloseControl (
	IN	HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject
	)
{
	NTSTATUS status;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2OpenControl: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (ControlFileObject) {

		ObDereferenceObject( ControlFileObject );
	}

	if (!ControlFileHandle) {

		return;
	}

	status = ZwClose( ControlFileHandle );

	if (!NT_SUCCESS(status)) {

		LtDebugPrint (0, ("[LpxTdiV2] LpxCloseControl: FAILURE, NtClose returned status code=%x\n", status));
	
	} else {
	
		LtDebugPrint (3, ("[LpxTdiV2] LpxCloseControl: NT_SUCCESS.\n"));
	}

	return;
}


NTSTATUS
LpxTdiV2IoControl (
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject,
	IN	ULONG    		IoControlCode,
	IN	PVOID    		InputBuffer OPTIONAL,
	IN	ULONG    		InputBufferLength,
	OUT PVOID	    	OutputBuffer OPTIONAL,
	IN	ULONG    		OutputBufferLength
	)
{
	NTSTATUS		status;

	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;


	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	UNREFERENCED_PARAMETER( ControlFileHandle );

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Disconnect: Entered\n") );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject(ControlFileObject);
	
	irp = IoBuildDeviceIoControlRequest( IoControlCode,
										 deviceObject,
										 InputBuffer,
										 InputBufferLength,
										 OutputBuffer,
										 OutputBufferLength,
										 FALSE,
										 &event,
										 &ioStatusBlock );
	
	if (irp == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->MdlAddress = NULL;

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = ioStatusBlock.Status;

		}

	} else if (status == STATUS_SUCCESS) {

		status = ioStatusBlock.Status;
	}	
	
	LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );
	
	return status;
}


NTSTATUS
LpxTdiV2Listen (
	IN  PFILE_OBJECT		ConnectionFileObject,
	IN  PULONG				RequestOptions,
	IN  PULONG				ReturnOptions,
	IN  ULONG				Flags,
	IN  PLARGE_INTEGER		TimeOut,
	OUT PUCHAR				AddressBuffer,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	)
{
	NTSTATUS					status;

	PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	
	KEVENT						event;
	TDI_CONNECTION_INFORMATION  requestConnectionInfo;
	TDI_CONNECTION_INFORMATION  returnConnectionInfo;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Send: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_LISTEN,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->MdlAddress = NULL;

	if (OverlappedData == NULL) {
	
		// Synchronous Mode

		KeInitializeEvent( &event, NotificationEvent, FALSE );

		requestConnectionInfo.UserData = NULL;
		requestConnectionInfo.UserDataLength = 0;
		requestConnectionInfo.Options =  RequestOptions;
		requestConnectionInfo.OptionsLength = sizeof(ULONG);
		requestConnectionInfo.RemoteAddress = NULL;
		requestConnectionInfo.RemoteAddressLength = 0;

		returnConnectionInfo.UserData = NULL;
		returnConnectionInfo.UserDataLength = 0;
		returnConnectionInfo.Options =  ReturnOptions;
		returnConnectionInfo.OptionsLength = sizeof(ULONG);
		returnConnectionInfo.RemoteAddress = AddressBuffer;
		returnConnectionInfo.RemoteAddressLength = TPADDR_LPX_LENGTH;

		TdiBuildListen( irp,
						deviceObject,
						ConnectionFileObject,
						LpxTdiSynchronousCompletionRoutine,
						&event,
						Flags,
						&requestConnectionInfo,
						&returnConnectionInfo );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, TimeOut );

			if (status == STATUS_TIMEOUT) {
	
				IoCancelIrp( irp );

				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, TimeOut) != STATUS_SUCCESS) {

					LPXTDI_ASSERT( FALSE );

					return status;
				}
			}

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;
		}

		LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		LPXTDI_ASSERT( OverlappedData->Irp == NULL );
		LPXTDI_ASSERT( AddressBuffer == NULL );
		LPXTDI_ASSERT( TimeOut == NULL );

		OverlappedData->Irp = irp;

		OverlappedData->RequestConnectionInfo.UserData = NULL;
		OverlappedData->RequestConnectionInfo.UserDataLength = 0;
		OverlappedData->RequestConnectionInfo.Options =  RequestOptions;
		OverlappedData->RequestConnectionInfo.OptionsLength = sizeof(ULONG);
		OverlappedData->RequestConnectionInfo.RemoteAddress = NULL;
		OverlappedData->RequestConnectionInfo.RemoteAddressLength = 0;

		OverlappedData->ReturnConnectionInfo.UserData = NULL;
		OverlappedData->ReturnConnectionInfo.UserDataLength = 0;
		OverlappedData->ReturnConnectionInfo.Options =  ReturnOptions;
		OverlappedData->ReturnConnectionInfo.OptionsLength = sizeof(ULONG);
		OverlappedData->ReturnConnectionInfo.RemoteAddress = OverlappedData->AddressBuffer;
		OverlappedData->ReturnConnectionInfo.RemoteAddressLength = TPADDR_LPX_LENGTH;

		TdiBuildListen( irp,
						deviceObject,
						ConnectionFileObject,
						LpxTdiAsynchronousCompletionRoutine,
						OverlappedData,
						Flags,
						&OverlappedData->RequestConnectionInfo,
						&OverlappedData->ReturnConnectionInfo );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}


NTSTATUS
LpxTdiV2Send (
	IN  PFILE_OBJECT		ConnectionFileObject,
	IN  PUCHAR				SendBuffer,
	IN  ULONG				SendLength,
	IN  ULONG				Flags,
	IN  PLARGE_INTEGER		TimeOut,
	OUT PLONG				Result,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	)
{
	NTSTATUS		status;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;

	PMDL			mdl;
	KEVENT			event;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Send: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	LPXTDI_ASSERT( SendBuffer && SendLength );

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_SEND,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( SendBuffer,
						 SendLength,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	if (OverlappedData == NULL) {
	
		// Synchronous Mode

		KeInitializeEvent( &event, NotificationEvent, FALSE );

		TdiBuildSend( irp,
					  deviceObject,
					  ConnectionFileObject,
					  LpxTdiSynchronousCompletionRoutine,
					  &event,
					  mdl,
					  Flags,
					  SendLength );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

			if (status == STATUS_TIMEOUT) {
	
				IoCancelIrp( irp );

				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, TimeOut) != STATUS_SUCCESS) {

					LPXTDI_ASSERT( FALSE );

					return status;
				}
			}

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;

				if (status == STATUS_SUCCESS) {

					status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
				}
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			if (status == STATUS_SUCCESS) {

				status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			}
		}

		LPXTDI_ASSERT( irp->MdlAddress != NULL );
		
		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;

		LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		if (status == STATUS_SUCCESS) {

			LPXTDI_ASSERT( *Result > 0 );
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		LPXTDI_ASSERT( OverlappedData->Irp == NULL );
		LPXTDI_ASSERT( Result == NULL );
		LPXTDI_ASSERT( TimeOut == NULL );

		OverlappedData->Irp = irp;

		TdiBuildSend( irp,
					  deviceObject,
					  ConnectionFileObject,
					  LpxTdiAsynchronousCompletionRoutine,
					  OverlappedData,
					  mdl,
					  Flags,
					  SendLength );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}


NTSTATUS
LpxTdiV2Recv (
	IN	PFILE_OBJECT		ConnectionFileObject,
	OUT	PUCHAR				RecvBuffer,
	IN	ULONG				RecvLength,
	IN	ULONG				Flags,
	IN	PLARGE_INTEGER		TimeOut,
	OUT	PLONG				Result,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	)
{
	NTSTATUS		status;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;

	PMDL			mdl;
	KEVENT			event;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Recv: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	LPXTDI_ASSERT( RecvBuffer && RecvLength );

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_RECEIVE,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( RecvBuffer,
						 RecvLength,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	if (OverlappedData == NULL) {
	
		// Synchronous Mode

		KeInitializeEvent( &event, NotificationEvent, FALSE );

		TdiBuildReceive( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiSynchronousCompletionRoutine,
						 &event,
						 mdl,
						 Flags,
						 RecvLength );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

			if (status == STATUS_TIMEOUT) {

				IoCancelIrp( irp );

				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, TimeOut) != STATUS_SUCCESS) {

					LPXTDI_ASSERT( FALSE );

					return status;
				}
			}

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;

				if (status == STATUS_SUCCESS) {

					status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
				}
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			if (status == STATUS_SUCCESS) {

				status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			}
		}

		LPXTDI_ASSERT( irp->MdlAddress != NULL );
		
		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;

		LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		if (status == STATUS_SUCCESS) {

			LPXTDI_ASSERT( *Result > 0 );
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		LPXTDI_ASSERT( OverlappedData->Irp == NULL );
		LPXTDI_ASSERT( Result == NULL );
		LPXTDI_ASSERT( TimeOut == NULL );

		OverlappedData->Irp = irp;

		TdiBuildReceive( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiAsynchronousCompletionRoutine,
						 OverlappedData,
						 mdl,
						 Flags,
						 RecvLength );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}


NTSTATUS
LpxTdiV2SendDataGram (
	IN  PFILE_OBJECT		AddressFileObject,
	IN  PLPX_ADDRESS		LpxRemoteAddress,
	IN  PUCHAR				SendBuffer,
	IN  ULONG				SendLength,
	IN  PLARGE_INTEGER		TimeOut,
	OUT PLONG				Result,
	IN  PLPXTDI_OVERLAPPED	OverlappedData
	)
{
	NTSTATUS					status;

	PDEVICE_OBJECT				deviceObject;
	PIRP						irp;

	PMDL						mdl;
	KEVENT						event;

	TDI_CONNECTION_INFORMATION  SendDatagramInfo;
	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];
	PTRANSPORT_ADDRESS			RemoteAddress;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2SendDataGram: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	LPXTDI_ASSERT( LpxRemoteAddress && SendBuffer && SendLength );

	deviceObject = IoGetRelatedDeviceObject(AddressFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_SEND_DATAGRAM,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( SendBuffer,
						 SendLength,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		LPXTDI_ASSERT( FALSE );

		if (OverlappedData && OverlappedData->IoCompleteRoutine) {

			OverlappedData->IoCompleteRoutine(OverlappedData);
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	if (OverlappedData == NULL) {
	
		// Synchronous Mode

		KeInitializeEvent( &event, NotificationEvent, FALSE );

		RemoteAddress = (PTRANSPORT_ADDRESS)AddressBuffer;

		RemoteAddress->TAAddressCount = 1;
		RemoteAddress->Address[0].AddressType	= TDI_ADDRESS_TYPE_LPX;
		RemoteAddress->Address[0].AddressLength	= TDI_ADDRESS_LENGTH_LPX;

		RtlCopyMemory( RemoteAddress->Address[0].Address, LpxRemoteAddress, sizeof(LPX_ADDRESS) );

		SendDatagramInfo.UserDataLength = 0;
		SendDatagramInfo.UserData = NULL;
		SendDatagramInfo.OptionsLength = 0;
		SendDatagramInfo.Options = NULL;
		SendDatagramInfo.RemoteAddressLength =	TPADDR_LPX_LENGTH;
		SendDatagramInfo.RemoteAddress = RemoteAddress;

		TdiBuildSendDatagram( irp,
							  deviceObject,
							  AddressFileObject,
							  LpxTdiSynchronousCompletionRoutine,
							  &event,
							  mdl,
							  SendLength,
							  &SendDatagramInfo );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

			if (status == STATUS_TIMEOUT) {

				IoCancelIrp( irp );

				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, TimeOut) != STATUS_SUCCESS) {

					LPXTDI_ASSERT( FALSE );

					return status;
				}
			}

			if (status == STATUS_SUCCESS) {

				status = irp->IoStatus.Status;

				if (status == STATUS_SUCCESS) {

					status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
				}
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			if (status == STATUS_SUCCESS) {

				status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			}
		}

		LPXTDI_ASSERT( irp->MdlAddress != NULL );

		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;

		LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		if (status == STATUS_SUCCESS) {

			LPXTDI_ASSERT( *Result > 0 );
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		LPXTDI_ASSERT( OverlappedData->Irp == NULL );
		LPXTDI_ASSERT( Result == NULL );
		LPXTDI_ASSERT( TimeOut == NULL );

		OverlappedData->Irp = irp;

		RemoteAddress = (PTRANSPORT_ADDRESS)OverlappedData->AddressBuffer;

		RemoteAddress->TAAddressCount = 1;
		RemoteAddress->Address[0].AddressType	= TDI_ADDRESS_TYPE_LPX;
		RemoteAddress->Address[0].AddressLength	= TDI_ADDRESS_LENGTH_LPX;

		RtlCopyMemory( RemoteAddress->Address[0].Address, LpxRemoteAddress, sizeof(LPX_ADDRESS) );

		OverlappedData->SendDatagramInfo.UserDataLength = 0;
		OverlappedData->SendDatagramInfo.UserData = NULL;
		OverlappedData->SendDatagramInfo.OptionsLength = 0;
		OverlappedData->SendDatagramInfo.Options = NULL;
		OverlappedData->SendDatagramInfo.RemoteAddressLength =	TPADDR_LPX_LENGTH;
		OverlappedData->SendDatagramInfo.RemoteAddress = RemoteAddress;

		TdiBuildSendDatagram( irp,
							  deviceObject,
							  AddressFileObject,
							  LpxTdiAsynchronousCompletionRoutine,
							  OverlappedData,
							  mdl,
							  SendLength,
							  &OverlappedData->SendDatagramInfo );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}


NTSTATUS
LpxTdiV2GetAddressList (
	IN OUT	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList
	) 
{
	NTSTATUS		status;

	HANDLE			controlFileHandle;
	PFILE_OBJECT	controlFileObject;
	int				addr_idx;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2GetAddressList: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	socketLpxAddressList->iAddressCount = 0;

	status = LpxTdiV2OpenControl( &controlFileHandle, &controlFileObject, NULL );
	
	if (!NT_SUCCESS(status)) {

		LtDebugPrint(1, ("[LpxTdiV2] LpxTdiV2GetAddressList: LpxTdiV2OpenControl() failed\n"));
		return status;
	}

	status = LpxTdiV2IoControl( controlFileHandle,
								controlFileObject,
								IOCTL_LPX_QUERY_ADDRESS_LIST,
								NULL,
								0,
								socketLpxAddressList,
								sizeof(SOCKETLPX_ADDRESS_LIST) );
	
	if (status != STATUS_SUCCESS) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2GetAddressList: LpxTdiV2IoControl() failed\n") );
		return status;
	}

	if (socketLpxAddressList->iAddressCount == 0) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2GetAddressList: No address.\n") );
		goto out;
	}

	LtDebugPrint( 2, ("[LpxTdiV2] LpxTdiV2GetAddressList: count = %ld\n", socketLpxAddressList->iAddressCount) );

	for (addr_idx = 0; addr_idx < socketLpxAddressList->iAddressCount; addr_idx ++) {

		LtDebugPrint( 2, ("\t%d. %02x:%02x:%02x:%02x:%02x:%02x/%d\n",
						   addr_idx,
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[0],
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[1],
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[2],
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[3],
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[4],
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Node[5],
						   socketLpxAddressList->SocketLpx[addr_idx].LpxAddress.Port) );
	}

out:
	LpxTdiV2CloseControl( controlFileHandle, controlFileObject );

	return STATUS_SUCCESS;
}


NTSTATUS
LpxTdiV2QueryInformation (
	IN	PFILE_OBJECT	ConnectionFileObject,
    IN  ULONG			QueryType,
	IN  PVOID			Buffer,
	IN  ULONG			BufferLen
	)
{
	NTSTATUS		status;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;

	PMDL			mdl;
	KEVENT			event;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2QueryInformation: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	switch (QueryType) {

	case TDI_QUERY_CONNECTION_INFO:
		
		if (BufferLen != sizeof(TDI_CONNECTION_INFO))
			return STATUS_INVALID_PARAMETER;

		break;

	case LPXTDI_QUERY_CONNECTION_TRANSSTAT:

		if (BufferLen != sizeof(TRANS_STAT))
			return STATUS_INVALID_PARAMETER;

		break;

	default:

		return STATUS_INVALID_PARAMETER;
	}

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_QUERY_INFORMATION,
											deviceObject,
											connectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {
	
		LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2QueryInformation: Can't Build IRP.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( Buffer,
						 BufferLen,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2QueryInformation: Can't Allocate MDL.\n") );
		IoFreeIrp( irp );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	TdiBuildQueryInformation( irp,
							  deviceObject,
							  ConnectionFileObject, 
							  LpxTdiSynchronousCompletionRoutine,
							  &event,
							  QueryType,
							  mdl );

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			//if (status == STATUS_SUCCESS) {

			//	status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			//}
		}

	} else if (status == STATUS_SUCCESS) {

		status = irp->IoStatus.Status;

		//if (status == STATUS_SUCCESS) {

		//	status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
		//}
	}

	LPXTDI_ASSERT( irp->MdlAddress != NULL );

	IoFreeMdl( irp->MdlAddress );
	irp->MdlAddress = NULL;

	LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

	IoCompleteRequest( irp, IO_NO_INCREMENT );

	return status;
}

NTSTATUS
LpxTdiV2SetInformation (
	IN	PFILE_OBJECT	ConnectionFileObject,
    IN  ULONG			SetType,
	IN  PVOID			Buffer,
	IN  ULONG			BufferLen
	)
{
	NTSTATUS		status;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;

	PMDL			mdl;
	KEVENT			event;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2SetInformation: Entered\n") );

	LPXTDI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );


#if 0
	switch(SetType) {
	
	case TDI_QUERY_CONNECTION_INFO:

		if (BufferLen != sizeof(TDI_CONNECTION_INFO))
			return STATUS_INVALID_PARAMETER;

		break;

	case LPXTDI_QUERY_CONNECTION_TRANSSTAT:

		if (BufferLen != sizeof(TRANS_STAT))
			return STATUS_INVALID_PARAMETER;

		break;
	
	default:
		return STATUS_INVALID_PARAMETER;
	}
#endif

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_SET_INFORMATION,
											deviceObject,
											connectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {
	
		LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2QueryInformation: Can't Build IRP.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( Buffer,
						 BufferLen,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2QueryInformation: Can't Allocate MDL.\n") );
		IoFreeIrp( irp );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	TdiBuildSetInformation( irp,
							deviceObject,
							ConnectionFileObject, 
							LpxTdiSynchronousCompletionRoutine,
							&event,
							SetType,
							mdl );

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			//if (status == STATUS_SUCCESS) {

			//	status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			//}
		}

	} else if (status == STATUS_SUCCESS) {

		status = irp->IoStatus.Status;

		//if (status == STATUS_SUCCESS) {

		//	status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
		//}
	}

	LPXTDI_ASSERT( irp->MdlAddress != NULL );

	IoFreeMdl( irp->MdlAddress );
	irp->MdlAddress = NULL;

	LPXTDI_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

	IoCompleteRequest( irp, IO_NO_INCREMENT );

	return status;
}
