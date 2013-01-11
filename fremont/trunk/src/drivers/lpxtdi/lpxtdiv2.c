/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "lpxtdiv2.h"


#pragma warning(error:4100)     //  Enable-Unreferenced formal parameter
#pragma warning(error:4101)     //  Enable-Unreferenced local variable
#pragma warning(error:4061)     //  Eenable-missing enumeration in switch statement
#pragma warning(error:4505)     //  Enable-identify dead functions


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
	IN PDEVICE_OBJECT				DeviceObject,
	IN PIRP							Irp,
	IN PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	)
{
	ULONG	requestIdx;

	UNREFERENCED_PARAMETER( DeviceObject );

	for (requestIdx = 0; requestIdx < LPXTDIV2_MAX_REQUEST; requestIdx++) {

		if (OverlappedContext->Request[requestIdx].Irp == Irp) {

			break;
		}
	}

	NDAS_ASSERT( requestIdx < LPXTDIV2_MAX_REQUEST );

	OverlappedContext->Request[requestIdx].IoStatusBlock.Status = Irp->IoStatus.Status;
	OverlappedContext->Request[requestIdx].IoStatusBlock.Information = Irp->IoStatus.Information;

	NDAS_ASSERT( Irp->IoStatus.Status == STATUS_SUCCESS || !NT_SUCCESS(Irp->IoStatus.Status) );

	if (OverlappedContext->Request[requestIdx].IoCompleteRoutine) {

		OverlappedContext->Request[requestIdx].IoCompleteRoutine( OverlappedContext );

		if (Irp->MdlAddress != NULL) {

			IoFreeMdl( Irp->MdlAddress );
			Irp->MdlAddress = NULL;
		}

		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		OverlappedContext->Request[requestIdx].Irp = NULL;

		return STATUS_SUCCESS;

	} else {

		KeSetEvent( &OverlappedContext->Request[requestIdx].CompletionEvent, IO_NO_INCREMENT, FALSE );

		return STATUS_MORE_PROCESSING_REQUIRED;
	}
}

NTSTATUS
LpxTdiV2OpenAddress (
	IN	PTDI_ADDRESS_LPX	LpxAddress,
	OUT	PHANDLE				AddressFileHandle,
	OUT	PFILE_OBJECT		*AddressFileObject
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


	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );


	LtDebugPrint (3, ("[LpxTdi] TdiOpenAddress:  Entered\n"));

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	RtlInitUnicodeString( &nameString, TRANSPORT_NAME );

	InitializeObjectAttributes( &objectAttributes,
								&nameString,
								OBJ_KERNEL_HANDLE,
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

	NDAS_ASSERT( lpxAddress->Port < HTONS((UINT16)0x4000) );

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
	
	if (status != STATUS_SUCCESS) {

		if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/) {

			if (status == STATUS_INSUFFICIENT_RESOURCES) {

				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			
			} if (status == 0xC0010022) {
			
				NDAS_ASSERT( NDAS_ASSERT_NETWORK_FAIL );

			} else {

				NDAS_ASSERT( FALSE );
			}
		} 

		*AddressFileHandle = NULL;
		*AddressFileObject = NULL;

		return status;
	}

	NDAS_ASSERT( status == ioStatusBlock.Status );

	status = ioStatusBlock.Status;

	if (status != STATUS_SUCCESS) {

		if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/) {

			if (status == STATUS_INSUFFICIENT_RESOURCES) {

				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			
			} else {

				NDAS_ASSERT( FALSE );
			}
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

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );

		ZwClose( addressFileHandle );
		
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( AddressFileHandle && AddressFileObject );

	if (AddressFileObject) {

		ObDereferenceObject( AddressFileObject );
	}

	if (AddressFileHandle == NULL) {

		return;
	}

	status = ZwClose( AddressFileHandle );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	return;
}

NTSTATUS
LpxTdiV2OpenConnection (
	IN	PVOID						ConnectionContext,
	OUT	PHANDLE						ConnectionFileHandle,
	OUT	PFILE_OBJECT				*ConnectionFileObject,
	OUT PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
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

	ULONG						requestIdx;

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( ConnectionFileObject && ConnectionFileHandle && OverlappedContext );

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2OpenConnection: Entered\n") );

	RtlInitUnicodeString( &nameString, TRANSPORT_NAME );

	InitializeObjectAttributes( &objectAttributes,
								&nameString,
								OBJ_KERNEL_HANDLE,
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

	if (status != STATUS_SUCCESS) {

		if (status == STATUS_INSUFFICIENT_RESOURCES) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			
		} else {

			NDAS_ASSERT( FALSE );
		}

		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;

		return status;
	}

	NDAS_ASSERT( status == ioStatusBlock.Status );

	status = ioStatusBlock.Status;

	if (status != STATUS_SUCCESS) {

		if (status == STATUS_INSUFFICIENT_RESOURCES) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );

		} else {

			NDAS_ASSERT( FALSE );
		}

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

	if (status != STATUS_SUCCESS) {

		if (status == STATUS_INSUFFICIENT_RESOURCES) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );

		} else {

			NDAS_ASSERT( FALSE );
		}

		ZwClose( connectionFileHandle );

		*ConnectionFileHandle = NULL;
		*ConnectionFileObject = NULL;

		return status;
	}
	 
	*ConnectionFileHandle = connectionFileHandle;
	*ConnectionFileObject = connectionFileObject;

	NDAS_ASSERT( !FlagOn(OverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED) );

	RtlZeroMemory( OverlappedContext, sizeof(LPXTDI_OVERLAPPED_CONTEXT) );
	
	for (requestIdx = 0; requestIdx < LPXTDIV2_MAX_REQUEST; requestIdx ++) {

		KeInitializeEvent( &OverlappedContext->Request[requestIdx].CompletionEvent, NotificationEvent, FALSE );
	}

	OverlappedContext->CheckBuffer = ExAllocatePoolWithTag( KernelMode, sizeof(CHAR), LPXTDIV2_SANITY_CHECK_TAG );

	SetFlag( OverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED );

	LtDebugPrint( 3, ("[LpxTdiV2] LpxOpenConnection: returning\n") );

	return status;
}

VOID
LpxTdiV2CloseConnection (
	IN	HANDLE						ConnectionFileHandle,
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	)
{
	NTSTATUS	status;
	ULONG		requestIdx;


	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( ConnectionFileObject && ConnectionFileHandle && OverlappedContext );

	for (requestIdx = 0; requestIdx < LPXTDIV2_MAX_REQUEST; requestIdx ++) {

		NDAS_ASSERT( OverlappedContext->Request[requestIdx].Irp == NULL );
		NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[requestIdx].CompletionEvent) == 0 );
	}

	NDAS_ASSERT( FlagOn(OverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED) );

	ClearFlag( OverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED );

	if (ConnectionFileObject) {

		ObDereferenceObject( ConnectionFileObject );
	}

	if (!ConnectionFileHandle) {

		return;
	}

	status = ZwClose( ConnectionFileHandle );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	if (OverlappedContext->CheckBuffer) {
		
		ExFreePoolWithTag( OverlappedContext->CheckBuffer, LPXTDIV2_SANITY_CHECK_TAG );
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_ASSOCIATE_ADDRESS,
											deviceObject,
											ConnectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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

	NDAS_ASSERT( status == STATUS_SUCCESS );

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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_DISASSOCIATE_ADDRESS,
											deviceObject,
											ConnectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );
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

	NDAS_ASSERT( status == STATUS_SUCCESS );
	return;
}

NTSTATUS
LpxTdiV2Connect (
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN	PTDI_ADDRESS_LPX			LpxAddress,
	IN  PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext
	)
{
	NTSTATUS					status;

	LARGE_INTEGER				timeout;

	KEVENT						event;
	PDEVICE_OBJECT	    		deviceObject;
	PIRP						irp;

	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];
	PTRANSPORT_ADDRESS	    	serverTransportAddress;
	PTA_ADDRESS	    			taAddress;
	PTDI_ADDRESS_LPX			addressName;

	TDI_CONNECTION_INFORMATION	connectionInfomation;

	if (Timeout) {

		NDAS_ASSERT( Timeout->QuadPart < 0 && (-Timeout->QuadPart) >= 30*NANO100_PER_SEC && (-Timeout->QuadPart) <= 1000*NANO100_PER_SEC );
	
		timeout = *Timeout;

	} else {

		timeout.QuadPart = -LPXTDIV2_DEFAULT_TIMEOUT;
	}

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Connect_LSTrans: Entered\n") );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_CONNECT,
											deviceObject,
											connectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[0].IoCompleteRoutine) {

			OverlappedContext->Request[0].IoCompleteRoutine(OverlappedContext);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->MdlAddress = NULL;

	if (OverlappedContext == NULL) {
	
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
						 NULL,
						 &connectionInfomation,
						 NULL );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, &timeout );

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;
			
			} else {

				NDAS_ASSERT( status == STATUS_TIMEOUT );
				NDAS_ASSERT( FALSE );

				LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2Connect: Canceled Irp= %p\n", irp) );

				if (IoCancelIrp(irp) == FALSE) {

					NDAS_ASSERT( FALSE );
				}
			
				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, &timeout) != STATUS_SUCCESS) {

					NDAS_ASSERT( FALSE );
				}

				NDAS_ASSERT( irp->IoStatus.Status == STATUS_CANCELLED );
				status = STATUS_CANCELLED;
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;
		}

		NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		NDAS_ASSERT( OverlappedContext->Request[0].Irp == NULL );
		NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[0].CompletionEvent) == 0 );
		NDAS_ASSERT( AddressBuffer == NULL );
		NDAS_ASSERT( Timeout == NULL );

		OverlappedContext->Request[0].Irp = irp;
		SetFlag( OverlappedContext->Request[0].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG );

		serverTransportAddress = (PTRANSPORT_ADDRESS)OverlappedContext->Request[0].AddressBuffer;
		serverTransportAddress->TAAddressCount = 1;

		taAddress = (PTA_ADDRESS)serverTransportAddress->Address;
		taAddress->AddressType	    = TDI_ADDRESS_TYPE_LPX;
		taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

		addressName = (PTDI_ADDRESS_LPX)taAddress->Address;
		RtlCopyMemory( addressName, LpxAddress, TDI_ADDRESS_LENGTH_LPX );	

		RtlZeroMemory( &OverlappedContext->ConnectionInfomation, sizeof(TDI_CONNECTION_INFORMATION) );

		OverlappedContext->ConnectionInfomation.UserDataLength = 0;
		OverlappedContext->ConnectionInfomation.UserData = NULL;
		OverlappedContext->ConnectionInfomation.OptionsLength = 0;
		OverlappedContext->ConnectionInfomation.Options = NULL;
		OverlappedContext->ConnectionInfomation.RemoteAddress = serverTransportAddress;
		OverlappedContext->ConnectionInfomation.RemoteAddressLength =	TPADDR_LPX_LENGTH;

		TdiBuildConnect( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiAsynchronousCompletionRoutine,
						 OverlappedContext,
						 Timeout,
						 &connectionInfomation,
						 NULL );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}

NTSTATUS
LpxTdiV2Listen (
	IN  PFILE_OBJECT				ConnectionFileObject,
	IN  PULONG						RequestOptions,
	IN  PULONG						ReturnOptions,
	IN  ULONG						Flags,
	IN  PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	OUT PUCHAR						AddressBuffer
	)
{
	NTSTATUS					status;

	PDEVICE_OBJECT				deviceObject;
	PIRP						irp;
	
	KEVENT						event;
	TDI_CONNECTION_INFORMATION  requestConnectionInfo;
	TDI_CONNECTION_INFORMATION  returnConnectionInfo;

	if (Timeout) {

		NDAS_ASSERT( Timeout->QuadPart < 0 && (-Timeout->QuadPart) >= 30*NANO100_PER_SEC && (-Timeout->QuadPart) <= 1000*NANO100_PER_SEC );
	
	}

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Send: Entered\n") );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_LISTEN,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[0].IoCompleteRoutine) {

			OverlappedContext->Request[0].IoCompleteRoutine(OverlappedContext);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->MdlAddress = NULL;

	if (OverlappedContext == NULL) {
	
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

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, Timeout );

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;

			} else {

				NDAS_ASSERT( status == STATUS_TIMEOUT );

				LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2Listen: Canceled Irp= %p\n", irp) );

				if (IoCancelIrp(irp) == FALSE) {

					NDAS_ASSERT( FALSE );
				}
			
				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, Timeout) != STATUS_SUCCESS) {

					NDAS_ASSERT( FALSE );
				}

				NDAS_ASSERT( irp->IoStatus.Status == STATUS_CANCELLED );
				status = STATUS_CANCELLED;
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;
		}

		NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		NDAS_ASSERT( OverlappedContext->Request[0].Irp == NULL );
		NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[0].CompletionEvent) == 0 );
		NDAS_ASSERT( AddressBuffer == NULL );
		NDAS_ASSERT( Timeout == NULL );

		OverlappedContext->Request[0].Irp = irp;
		SetFlag( OverlappedContext->Request[0].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG );

		OverlappedContext->RequestConnectionInfo.UserData = NULL;
		OverlappedContext->RequestConnectionInfo.UserDataLength = 0;
		OverlappedContext->RequestConnectionInfo.Options =  RequestOptions;
		OverlappedContext->RequestConnectionInfo.OptionsLength = sizeof(ULONG);
		OverlappedContext->RequestConnectionInfo.RemoteAddress = NULL;
		OverlappedContext->RequestConnectionInfo.RemoteAddressLength = 0;

		OverlappedContext->ReturnConnectionInfo.UserData = NULL;
		OverlappedContext->ReturnConnectionInfo.UserDataLength = 0;
		OverlappedContext->ReturnConnectionInfo.Options =  ReturnOptions;
		OverlappedContext->ReturnConnectionInfo.OptionsLength = sizeof(ULONG);
		OverlappedContext->ReturnConnectionInfo.RemoteAddress = OverlappedContext->Request[0].AddressBuffer;
		OverlappedContext->ReturnConnectionInfo.RemoteAddressLength = TPADDR_LPX_LENGTH;

		TdiBuildListen( irp,
						deviceObject,
						ConnectionFileObject,
						LpxTdiAsynchronousCompletionRoutine,
						OverlappedContext,
						Flags,
						&OverlappedContext->RequestConnectionInfo,
						&OverlappedContext->ReturnConnectionInfo );

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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( ConnectionFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_DISCONNECT,
											deviceObject,
											ConnectionFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );
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

	NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

	return;
}

NTSTATUS
LpxTdiV2Send (
	IN  PFILE_OBJECT				ConnectionFileObject,
	IN  PUCHAR						SendBuffer,
	IN  ULONG						SendLength,
	IN  ULONG						Flags,
	IN  PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT PLONG						Result
	)
{
	NTSTATUS		status;

	LARGE_INTEGER	timeout;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;

	PMDL			mdl;
	KEVENT			event;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Send: Entered\n") );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( SendBuffer && SendLength );

	if (Timeout) {

		NDAS_ASSERT( Timeout->QuadPart < 0 && (-Timeout->QuadPart) >= 30*NANO100_PER_SEC && (-Timeout->QuadPart) <= 1000*NANO100_PER_SEC );
	
		timeout = *Timeout;

	} else {

		timeout.QuadPart = -LPXTDIV2_DEFAULT_TIMEOUT;
	}

	if (OverlappedContext && RequestIdx >= LPXTDIV2_MAX_REQUEST) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;		
	}		

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_SEND,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[RequestIdx].IoCompleteRoutine) {

			OverlappedContext->Request[RequestIdx].IoCompleteRoutine(OverlappedContext);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( SendBuffer,
						 SendLength,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[RequestIdx].IoCompleteRoutine) {

			OverlappedContext->Request[RequestIdx].IoCompleteRoutine(OverlappedContext);
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	if (OverlappedContext == NULL) {
	
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

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, Timeout );

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;

				if (status == STATUS_SUCCESS) {

					status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
				}
			
			} else {

				BOOLEAN	result;

				NDAS_ASSERT( status == STATUS_TIMEOUT );

				LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2Send: Canceled Irp= %p\n", irp) );

				result = IoCancelIrp(irp);
				
				if (result == FALSE) {
					
					NDAS_ASSERT( irp->IoStatus.Status == 0xC0010011 ); // NDIS_STATUS_ADAPTER_NOT_READY
					status = irp->IoStatus.Status;
				
				} else {
			
					if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, Timeout) != STATUS_SUCCESS) {

						NDAS_ASSERT( FALSE );
					}

					NDAS_ASSERT( irp->IoStatus.Status == STATUS_CANCELLED );
					status = STATUS_CANCELLED;
				}
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			if (status == STATUS_SUCCESS) {

				status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			}
		}

		NDAS_ASSERT( irp->MdlAddress != NULL );
		
		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;

		NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		if (status == STATUS_SUCCESS) {

			NDAS_ASSERT( *Result > 0 );
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		NDAS_ASSERT( OverlappedContext->Request[RequestIdx].Irp == NULL );
		NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[RequestIdx].CompletionEvent) == 0 );
		NDAS_ASSERT( Result == NULL );
		NDAS_ASSERT( Timeout == NULL );

		OverlappedContext->Request[RequestIdx].Irp = irp;
		SetFlag( OverlappedContext->Request[RequestIdx].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG );

		TdiBuildSend( irp,
					  deviceObject,
					  ConnectionFileObject,
					  LpxTdiAsynchronousCompletionRoutine,
					  OverlappedContext,
					  mdl,
					  Flags,
					  SendLength );

		status = IoCallDriver( deviceObject, irp );

		// User expects returned status is STATUS_PENDING, but status can be STATUS_SUCCESS.
		// User is expected more cautious to use this asynchronous mode.
	}

	return status;
}

NTSTATUS
LpxTdiV2Recv (
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN	PUCHAR						RecvBuffer,
	IN	ULONG						RecvLength,
	IN	ULONG						Flags,
	IN	PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT	PLONG						Result
	)
{
	NTSTATUS		status;

	LARGE_INTEGER	timeout;

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;

	PMDL			mdl;
	KEVENT			kevent;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2Recv: Entered\n") );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( RecvBuffer && RecvLength );

	if (Timeout) {

		NDAS_ASSERT( Timeout->QuadPart < 0 && (-Timeout->QuadPart) >= 30*NANO100_PER_SEC && (-Timeout->QuadPart) <= 1000*NANO100_PER_SEC );

		timeout = *Timeout;

	} else {

		timeout.QuadPart = -LPXTDIV2_DEFAULT_TIMEOUT;
	}

	if (OverlappedContext && RequestIdx >= LPXTDIV2_MAX_REQUEST) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;		
	}		

	deviceObject = IoGetRelatedDeviceObject(ConnectionFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_RECEIVE,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[RequestIdx].IoCompleteRoutine) {

			OverlappedContext->Request[RequestIdx].IoCompleteRoutine(OverlappedContext);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( RecvBuffer, RecvLength, FALSE, FALSE, irp );

	if (mdl == NULL) {

		NDAS_ASSERT(FALSE);

		if (OverlappedContext && OverlappedContext->Request[RequestIdx].IoCompleteRoutine) {

			OverlappedContext->Request[RequestIdx].IoCompleteRoutine(OverlappedContext);
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool(mdl);

	if (OverlappedContext == NULL) {
	
		// Synchronous Mode

		KeInitializeEvent( &kevent, NotificationEvent, FALSE );

		TdiBuildReceive( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiSynchronousCompletionRoutine,
						 &kevent,
						 mdl,
						 Flags,
						 RecvLength );

		status = IoCallDriver( deviceObject, irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &kevent, Executive, KernelMode, FALSE, Timeout );

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;

				if (status == STATUS_SUCCESS) {

					status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
				}
			
			} else {

				NDAS_ASSERT( status == STATUS_TIMEOUT );
				NDAS_ASSERT( FALSE );

				LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2Recv: Canceled Irp= %p\n", irp) );

				if (IoCancelIrp(irp) == FALSE) {
					
					NDAS_ASSERT( FALSE );
				}
			
				if (KeWaitForSingleObject(&kevent, Executive, KernelMode, FALSE, Timeout) != STATUS_SUCCESS) {

					NDAS_ASSERT( FALSE );
				}

				NDAS_ASSERT( irp->IoStatus.Status == STATUS_CANCELLED );
				status = STATUS_CANCELLED;
			}
						 
		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			if (status == STATUS_SUCCESS) {

				status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			}
		}

		NDAS_ASSERT( irp->MdlAddress != NULL );
		
		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;

		NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		if (status == STATUS_SUCCESS) {

			NDAS_ASSERT( *Result > 0 );
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		NDAS_ASSERT( OverlappedContext->Request[RequestIdx].Irp == NULL );
		NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[RequestIdx].CompletionEvent) == 0 );
		NDAS_ASSERT( Result == NULL );
		NDAS_ASSERT( Timeout == NULL );

		OverlappedContext->Request[RequestIdx].Irp = irp;
		SetFlag( OverlappedContext->Request[RequestIdx].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG );

		TdiBuildReceive( irp,
						 deviceObject,
						 ConnectionFileObject,
						 LpxTdiAsynchronousCompletionRoutine,
						 OverlappedContext,
						 mdl,
						 Flags,
						 RecvLength );

		status = IoCallDriver( deviceObject, irp );

		// User expects returned status is STATUS_PENDING, but status can be STATUS_SUCCESS.
		// User is expected more cautious to use this asynchronous mode.
	}

	return status;
}

NTSTATUS
LpxTdiV2SendDatagram (
	IN  PFILE_OBJECT				AddressFileObject,
	IN  PLPX_ADDRESS				LpxRemoteAddress,
	IN  PUCHAR						SendBuffer,
	IN  ULONG						SendLength,
	IN  PLARGE_INTEGER				Timeout,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN  ULONG						RequestIdx,
	OUT PLONG						Result
	)
{
	NTSTATUS					status;

	LARGE_INTEGER				timeout;

	PDEVICE_OBJECT				deviceObject;
	PIRP						irp;

	PMDL						mdl;
	KEVENT						event;

	TDI_CONNECTION_INFORMATION  SendDatagramInfo;
	UCHAR						AddressBuffer[TPADDR_LPX_LENGTH];
	PTRANSPORT_ADDRESS			RemoteAddress;


	NDAS_ASSERT( Timeout == NULL );

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2SendDatagram: Entered\n") );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( LpxRemoteAddress && SendBuffer && SendLength );

	if (Timeout) {

		NDAS_ASSERT( Timeout->QuadPart < 0 && (-Timeout->QuadPart) >= 30*NANO100_PER_SEC && (-Timeout->QuadPart) <= 1000*NANO100_PER_SEC );

		timeout = *Timeout;

	} else {

		timeout.QuadPart = -LPXTDIV2_DEFAULT_TIMEOUT;
	}

	if (OverlappedContext && RequestIdx >= LPXTDIV2_MAX_REQUEST) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;		
	}		

	deviceObject = IoGetRelatedDeviceObject(AddressFileObject);

	irp = TdiBuildInternalDeviceControlIrp( TDI_SEND_DATAGRAM,
											deviceObject,
											ConnectionFileObject,
											NULL,
											NULL );

	if (irp == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[RequestIdx].IoCompleteRoutine) {

			OverlappedContext->Request[RequestIdx].IoCompleteRoutine(OverlappedContext);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl( SendBuffer,
						 SendLength,
						 FALSE,
						 FALSE,
						 irp );

	if (mdl == NULL) {

		NDAS_ASSERT( FALSE );

		if (OverlappedContext && OverlappedContext->Request[RequestIdx].IoCompleteRoutine) {

			OverlappedContext->Request[RequestIdx].IoCompleteRoutine(OverlappedContext);
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool( mdl );

	if (OverlappedContext == NULL) {
	
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

			status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, Timeout );

			if (status == STATUS_SUCCESS) {
		
				status = irp->IoStatus.Status;

				if (status == STATUS_SUCCESS) {

					status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
				}
			
			} else {

				NDAS_ASSERT( status == STATUS_TIMEOUT );
				NDAS_ASSERT( FALSE );

				LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2Recv: Canceled Irp= %p\n", irp) );

				if (IoCancelIrp(irp) == FALSE) {

					NDAS_ASSERT( FALSE );
				}
			
				if (KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, Timeout) != STATUS_SUCCESS) {

					NDAS_ASSERT( FALSE );
				}

				NDAS_ASSERT( irp->IoStatus.Status == STATUS_CANCELLED );
				status = STATUS_CANCELLED;
			}

		} else if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;

			if (status == STATUS_SUCCESS) {

				status = RtlULongPtrToLong( irp->IoStatus.Information, Result );
			}
		}

		NDAS_ASSERT( irp->MdlAddress != NULL );

		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;

		NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

		if (status == STATUS_SUCCESS) {

			NDAS_ASSERT( *Result > 0 );
		}

		IoCompleteRequest( irp, IO_NO_INCREMENT );

	} else {

		// Asynchronous Mode

		NDAS_ASSERT( OverlappedContext->Request[RequestIdx].Irp == NULL );
		NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[RequestIdx].CompletionEvent) == 0 );
		NDAS_ASSERT( Result == NULL );
		NDAS_ASSERT( Timeout == NULL );

		OverlappedContext->Request[RequestIdx].Irp = irp;
		SetFlag( OverlappedContext->Request[RequestIdx].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG );

		RemoteAddress = (PTRANSPORT_ADDRESS)OverlappedContext->Request[RequestIdx].AddressBuffer;

		RemoteAddress->TAAddressCount = 1;
		RemoteAddress->Address[0].AddressType	= TDI_ADDRESS_TYPE_LPX;
		RemoteAddress->Address[0].AddressLength	= TDI_ADDRESS_LENGTH_LPX;

		RtlCopyMemory( RemoteAddress->Address[0].Address, LpxRemoteAddress, sizeof(LPX_ADDRESS) );

		OverlappedContext->Request[RequestIdx].SendDatagramInfo.UserDataLength = 0;
		OverlappedContext->Request[RequestIdx].SendDatagramInfo.UserData = NULL;
		OverlappedContext->Request[RequestIdx].SendDatagramInfo.OptionsLength = 0;
		OverlappedContext->Request[RequestIdx].SendDatagramInfo.Options = NULL;
		OverlappedContext->Request[RequestIdx].SendDatagramInfo.RemoteAddressLength =	TPADDR_LPX_LENGTH;
		OverlappedContext->Request[RequestIdx].SendDatagramInfo.RemoteAddress = RemoteAddress;

		TdiBuildSendDatagram( irp,
							  deviceObject,
							  AddressFileObject,
							  LpxTdiAsynchronousCompletionRoutine,
							  OverlappedContext,
							  mdl,
							  SendLength,
							  &OverlappedContext->Request[RequestIdx].SendDatagramInfo );

		status = IoCallDriver( deviceObject, irp );
	}

	return status;
}

NTSTATUS
LpxTdiV2RegisterDisconnectHandler (
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

	deviceObject = IoGetRelatedDeviceObject( AddressFileObject );

	irp = TdiBuildInternalDeviceControlIrp( TDI_SET_EVENT_HANDLER,
											deviceObject,
											AddressFileObject,
											&event,
											&ioStatusBlock );

	if (irp == NULL) {

		LtDebugPrint( 1, ("[LpxTdiV2] LpxRegisterDisconnectHandler: Can't Build IRP.\n") );
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

	NDAS_ASSERT( status == STATUS_SUCCESS );

	return status;
}

NTSTATUS
LpxTdiV2RegisterRecvDatagramHandler (
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

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

	NDAS_ASSERT( status == STATUS_SUCCESS );

	return status;
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	switch (QueryType) {

	case TDI_QUERY_CONNECTION_INFO:
		
		if (BufferLen != sizeof(TDI_CONNECTION_INFO)) {

			return STATUS_INVALID_PARAMETER;
		}

		break;

	case LPXTDI_QUERY_CONNECTION_TRANSSTAT:

		if (BufferLen != sizeof(TRANS_STAT)) {

			return STATUS_INVALID_PARAMETER;
		}

		break;

	case LPX_NDIS_QUERY_GLOBAL_STATS:

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

	NDAS_ASSERT( irp->MdlAddress != NULL );

	IoFreeMdl( irp->MdlAddress );
	irp->MdlAddress = NULL;

	NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );


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

	NDAS_ASSERT( irp->MdlAddress != NULL );

	IoFreeMdl( irp->MdlAddress );
	irp->MdlAddress = NULL;

	NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

	IoCompleteRequest( irp, IO_NO_INCREMENT );

	return status;
}

NTSTATUS
LpxTdiV2OpenControl (
	IN  PVOID			ControlContext,
	OUT PHANDLE			ControlFileHandle, 
	OUT	PFILE_OBJECT	*ControlFileObject
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	RtlInitUnicodeString( &nameString, TRANSPORT_NAME );

	InitializeObjectAttributes( &objectAttributes,
								&nameString,
								OBJ_KERNEL_HANDLE,
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
	NTSTATUS	status;


	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2OpenControl: Entered\n") );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

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

	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	PMDL			mdl;

	UINT32			bufferMethod = IoControlCode & 0x3;;

	
	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

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
										 NULL,
										 NULL );
	
	if (irp == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	IoSetCompletionRoutine( irp,
							LpxTdiSynchronousCompletionRoutine,
							&event,
							TRUE,
							TRUE,
							TRUE );


	if (bufferMethod == METHOD_IN_DIRECT) {

		mdl = IoAllocateMdl( InputBuffer,
							 InputBufferLength,
							 FALSE,
							 FALSE,
							 irp );

		if (mdl == NULL) {

			LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2IoControl: Can't Allocate MDL.\n") );
			IoFreeIrp( irp );
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		MmBuildMdlForNonPagedPool( mdl );
		
	} else if (bufferMethod == METHOD_OUT_DIRECT) {

		mdl = IoAllocateMdl( OutputBuffer,
							 OutputBufferLength,
							 FALSE,
							 FALSE,
							 irp );

		if (mdl == NULL) {

			LtDebugPrint( 1, ("[LpxTdiV2] LpxTdiV2IoControl: Can't Allocate MDL.\n") );
			IoFreeIrp( irp );
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		MmBuildMdlForNonPagedPool( mdl );

	} else {

		irp->MdlAddress = NULL;
	}

	status = IoCallDriver( deviceObject, irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );

		if (status == STATUS_SUCCESS) {

			status = irp->IoStatus.Status;
		}

	} else if (status == STATUS_SUCCESS) {

		status = irp->IoStatus.Status;
	}	
	
	if (irp->MdlAddress) {

		IoFreeMdl( irp->MdlAddress );
		irp->MdlAddress = NULL;
	}

	NDAS_ASSERT( status == STATUS_SUCCESS || !NT_SUCCESS(status) );

	IoCompleteRequest( irp, IO_NO_INCREMENT );
	
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

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	socketLpxAddressList->iAddressCount = 0;

	status = LpxTdiV2OpenControl( NULL, &controlFileHandle, &controlFileObject );
	
	if (!NT_SUCCESS(status)) {

		LtDebugPrint(1, ("[LpxTdiV2] LpxTdiV2GetAddressList: LpxTdiV2OpenControl() failed\n"));
		return status;
	}

	status = LpxTdiV2IoControl( controlFileObject,
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

		LtDebugPrint( 2, ("[LpxTdiV2] LpxTdiV2GetAddressList: No address.\n") );
		goto out;
	}

	LtDebugPrint( 3, ("[LpxTdiV2] LpxTdiV2GetAddressList: count = %ld\n", socketLpxAddressList->iAddressCount) );

	for (addr_idx = 0; addr_idx < socketLpxAddressList->iAddressCount; addr_idx ++) {

		LtDebugPrint( 3, ("\t%d. %02x:%02x:%02x:%02x:%02x:%02x/%d\n",
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
LpxTdiV2GetTransportAddress (
	IN	ULONG				AddressListLen,
	IN	PTRANSPORT_ADDRESS	AddressList,
	OUT	PULONG				OutLength
	) 
{
	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	NTSTATUS				status;
	LONG					idx_addr;
	ULONG					length;
	PTA_ADDRESS				taAddress;


	status = LpxTdiV2GetAddressList( &socketLpxAddressList );

	if (!NT_SUCCESS(status)) {
	
		return status;
	}

	length = FIELD_OFFSET( TRANSPORT_ADDRESS, Address );
	length += (FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX) * socketLpxAddressList.iAddressCount;
		
	if (AddressListLen < length) {

		NDAS_ASSERT( FALSE );

		if (OutLength) {

			*OutLength = length;
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//	translate LPX address list into LSTrans address list.

	AddressList->TAAddressCount = socketLpxAddressList.iAddressCount;

	taAddress = NULL;

	for (idx_addr = 0; idx_addr < socketLpxAddressList.iAddressCount; idx_addr ++) {
	
		if (taAddress == NULL) {

			taAddress = AddressList->Address;

		} else {

			taAddress = (PTA_ADDRESS)(((PCHAR)taAddress) + 
									  (FIELD_OFFSET(TA_ADDRESS, Address)) + 
									  taAddress->AddressLength);
		}

		taAddress->AddressType = TDI_ADDRESS_TYPE_LPX;
		taAddress->AddressLength = TDI_ADDRESS_LENGTH_LPX;
		RtlCopyMemory( taAddress->Address, 
					   &socketLpxAddressList.SocketLpx[idx_addr].LpxAddress, 
					   TDI_ADDRESS_LENGTH_LPX );
	}

	if (OutLength) {
	
		*OutLength = length;
	}

	return status;
}

VOID
LpxTdiV2CompleteRequest (
	IN PLPXTDI_OVERLAPPED_CONTEXT OverlappedContext, 
	IN ULONG					  RequestIdx
	)
{

	if (OverlappedContext == NULL || RequestIdx >= LPXTDIV2_MAX_REQUEST) {

		NDAS_ASSERT( FALSE );
		return;		
	}		

	if (OverlappedContext->Request[RequestIdx].Irp == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}

	if (OverlappedContext->Request[RequestIdx].Irp->MdlAddress != NULL) {
		
		IoFreeMdl( OverlappedContext->Request[RequestIdx].Irp->MdlAddress );
		OverlappedContext->Request[RequestIdx].Irp->MdlAddress = NULL;
	}

	NDAS_ASSERT( KeReadStateEvent(&OverlappedContext->Request[RequestIdx].CompletionEvent) == 1 );
	IoCompleteRequest( OverlappedContext->Request[RequestIdx].Irp, IO_NO_INCREMENT );

	OverlappedContext->Request[RequestIdx].Irp = NULL;

	ClearFlag( OverlappedContext->Request[RequestIdx].Flags2, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG2_REQUEST_PENDIG );

	KeClearEvent( &OverlappedContext->Request[RequestIdx].CompletionEvent );

	return;
}

VOID
LpxTdiV2CancelRequest (
	IN	PFILE_OBJECT				ConnectionFileObject,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedContext,
	IN	ULONG						RequestIdx,
	IN  BOOLEAN						DisConnect,
	IN	ULONG						DisConnectFlags
	)
{
	NTSTATUS		status;
	LARGE_INTEGER	timeOut;

	
	if (OverlappedContext == NULL || RequestIdx >= LPXTDIV2_MAX_REQUEST) {

		NDAS_ASSERT( FALSE );
		return;		
	}		

	if (OverlappedContext->Request[RequestIdx].Irp == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}
	
	IoCancelIrp( OverlappedContext->Request[RequestIdx].Irp );

	timeOut.QuadPart = -100*NANO100_PER_SEC;

	status = KeWaitForSingleObject( &OverlappedContext->Request[RequestIdx].CompletionEvent, 
									Executive, 
									KernelMode, 
									FALSE, 
									&timeOut );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	LpxTdiV2CompleteRequest( OverlappedContext, RequestIdx );

	if (DisConnect) {

		LpxTdiV2Disconnect( ConnectionFileObject, DisConnectFlags );
	}

	return;
}

NTSTATUS
LpxTdiV2MoveOverlappedContext (
	IN PLPXTDI_OVERLAPPED_CONTEXT DestOverlappedContext,
	IN PLPXTDI_OVERLAPPED_CONTEXT SourceOverlappedContext
	)
{
	ULONG requestIdx;

	NDAS_ASSERT( FlagOn(SourceOverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED) );

	RtlZeroMemory( DestOverlappedContext, sizeof(LPXTDI_OVERLAPPED_CONTEXT) );
	RtlCopyMemory( DestOverlappedContext, SourceOverlappedContext, sizeof(LPXTDI_OVERLAPPED_CONTEXT) );
	
	for (requestIdx = 0; requestIdx < LPXTDIV2_MAX_REQUEST; requestIdx++) {
	
		KeInitializeEvent( &DestOverlappedContext->Request[requestIdx].CompletionEvent, NotificationEvent, FALSE );
	}

	ClearFlag( SourceOverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_INITIALIZED );
	SetFlag( SourceOverlappedContext->Flags1, LPXTDIV2_OVERLAPPED_CONTEXT_FLAG1_MOVED );

	return STATUS_SUCCESS;
}

