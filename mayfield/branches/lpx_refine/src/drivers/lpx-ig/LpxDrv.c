/*++

Copyright (c) Ximeta Technology Inc.

Module Name:

    LpxDrv.c

Abstract:


Author:


Environment:

    Kernel mode only.

Notes:


Future:



Revision History: 
  
--*/




#include "precomp.h"
#pragma hdrstop


LONG	DebugLevel = 10;
GLOBAL	global;


/* 
 *	definition lpx driver function
 */

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{

    NDIS_PROTOCOL_CHARACTERISTICS   protocolChar;
    NTSTATUS                        status = STATUS_SUCCESS;
    UNICODE_STRING					protoName;
    UNICODE_STRING                  ntDeviceName;
    UNICODE_STRING                  win32DeviceName;
    PDEVICE_OBJECT                  deviceObject;
	NDIS_STATUS						ndisStatus;

	deviceObject = NULL;
DebugPrint(1, ("\n\n Starting Lpx DriverEntry\n"));

	RtlZeroMemory(&global, sizeof(GLOBAL) );

    global.DriverObject = DriverObject;
    
    //
    // Save the RegistryPath.
    //

	global.RegistryPath.MaximumLength = RegistryPath->Length +
                                          sizeof(UNICODE_NULL);
    global.RegistryPath.Length = RegistryPath->Length;
    global.RegistryPath.Buffer = ExAllocatePool(
                                       PagedPool,
                                       global.RegistryPath.MaximumLength
                                       );    

    if (!global.RegistryPath.Buffer) {

        DebugPrint (8,("Couldn't allocate pool for registry path."));

        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlCopyUnicodeString(&global.RegistryPath, RegistryPath);


    //
    // Now set only the dispatch points we would like to handle.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = LpxOpen;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = LpxClose;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]   = LpxCleanup;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL]  = LpxInternalDevControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = LpxDevControl;
    DriverObject->MajorFunction[IRP_MJ_PNP_POWER] = LpxPnPPower;
    DriverObject->DriverUnload = LpxUnload;

    //
    // Initialize the protocol characterstic structure
    //

	RtlInitUnicodeString(&protoName, LPX_NAME);   
    NdisZeroMemory(&protocolChar,sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    protocolChar.MajorNdisVersion            = 4;
    protocolChar.MinorNdisVersion            = 0;
    
    protocolChar.Name.Length = protoName.Length;
    protocolChar.Name.MaximumLength = protoName.MaximumLength;
    protocolChar.Name.Buffer = protoName.Buffer;
    
    protocolChar.OpenAdapterCompleteHandler  = LpxProtoOpenAdapterComplete;
    protocolChar.CloseAdapterCompleteHandler = LpxProtoCloseAdapterComplete;
    protocolChar.SendCompleteHandler         = LpxProtoSendComplete;
    protocolChar.TransferDataCompleteHandler = LpxProtoTransferDataComplete;
    protocolChar.ResetCompleteHandler        = LpxProtoResetComplete;
    protocolChar.RequestCompleteHandler      = LpxProtoRequestComplete;
    protocolChar.ReceiveHandler              = LpxProtoReceiveIndicate;
    protocolChar.ReceiveCompleteHandler      = LpxProtoReceiveComplete;
    protocolChar.StatusHandler = LpxProtoStatus;
    protocolChar.StatusCompleteHandler       = LpxProtoStatusComplete;
    protocolChar.BindAdapterHandler          = LpxProtoBindAdapter;
    protocolChar.UnbindAdapterHandler        = LpxProtoUnbindAdapter;
    protocolChar.UnloadHandler               = NULL;
    protocolChar.ReceivePacketHandler        = NULL; 
    protocolChar.PnPEventHandler             = LpxProtoPnPEventHandler;


	// init Global information
	InitializeListHead(&global.NIC_DevList);
    KeInitializeSpinLock(&global.GlobalLock);
	ExInitializeFastMutex(&global.DevicesLock);

	//
	// Init driver for Tdi provider
	//
	TdiInitialize();

    //
    // Register as a  protocol driver
    //
    
    NdisRegisterProtocol(
        &status,
        &global.NdisProtocolHandle,
        &protocolChar,
        sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    
    if (status != NDIS_STATUS_SUCCESS) {
DebugPrint(8,("Failed to register protocol with NDIS\n"));
		DebugPrint(0, ("DriverEntry: NdisRegisterProtocol Status: %s\n",
            lpx_GetNdisStatus (status)));
        status = STATUS_UNSUCCESSFUL;	
        goto ERROR;        
    }
  
	//
    // Register as a Tdi provider
	//
	RtlInitUnicodeString( &ntDeviceName, LPX_DEVICE_NAME);

    status = TdiRegisterProvider(
                &ntDeviceName,
                &global.TdiProviderHandle);
	
	if( !NT_SUCCESS ( status ))
	{
		DebugPrint(8,("Failed to create Tdi provider\n"));
		status = STATUS_UNSUCCESSFUL;	
		goto ERROR;
	}

	RtlInitUnicodeString( &ntDeviceName, SOCKETLPX_DEVICE_NAME);

    //
    // Create a control device object for this driver.
    // Application can send an IOCTL to this device to get 
    // bound adapter information.
    //
    status = Lpx_CreateControlDevice( DriverObject,
		    							&ntDeviceName
										);

DebugPrint(0,("global.ControlDeviceObject %x\n",global.ControlDeviceObject));

    if (!NT_SUCCESS (status)) {
        //
        // Either not enough memory to create a deviceobject or another
        // deviceobject with the same name exits. This could happen
        // if you install another instance of this device.
        //
		DebugPrint(0,("Failed to register generate Control device \n"));
        goto ERROR;
    }


	DebugPrint(0, ("\n\n Exiting Lpx DriverEntry\n"));
    return(STATUS_SUCCESS);

ERROR:
 
    if(global.RegistryPath.Buffer) {       
        ExFreePool(global.RegistryPath.Buffer);
		global.RegistryPath.Buffer = NULL;
	}
	if(global.NdisProtocolHandle){
		NdisDeregisterProtocol(
						&ndisStatus,
						global.NdisProtocolHandle);
		global.NdisProtocolHandle = (NDIS_HANDLE)NULL;
	}

	if(global.TdiProviderHandle){
		TdiDeregisterProvider(global.TdiProviderHandle);
		global.TdiProviderHandle = NULL;
	}
DebugPrint(0, ("\n\n ERROR DriverEntry\n"));
    return status;


}



NTSTATUS
LpxOpen(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    )
{
    KIRQL oldirql;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_FULL_EA_INFORMATION openType;
    USHORT i;
    BOOLEAN found;
    PTP_ADDRESS		Address;
    PTP_CONNECTION Connection;

DebugPrint(1,("ENTER LpxOpen\n"));
    try {

        DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
			DebugPrint(1,("DeviceContext->State != DEVICECONTEXT_STATE_OPEN\n"));
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        
		} else if (DeviceContext->Type != CONTROL_DEVICE_SIGNATURE){
			DebugPrint(1,("DeviceContext->Type != CONTROL_DEVICE_SIGNATURE\n"));
			Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_DEVICE_DOES_NOT_EXIST;	
        }

		// Reference the device so that it does not go away from under us
		LPX_REFERENCE_DEVICECONTEXT(DeviceContext); //CONTRL ref + :1
        
    } except(EXCEPTION_EXECUTE_HANDLER) {
		DebugPrint(1,("EXCEPTION CALLED\n"));		
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

	//
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

		
    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //
	if(IrpSp->MajorFunction == IRP_MJ_CREATE) {
        openType = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

		if(openType != NULL)
		{

            //
            // Address?
            //

            found = TRUE;

            if ((USHORT)openType->EaNameLength == TDI_TRANSPORT_ADDRESS_LENGTH) {
                for (i = 0; i < TDI_TRANSPORT_ADDRESS_LENGTH; i++) {
                    if (openType->EaName[i] != TdiTransportAddress[i]) {
                        found = FALSE;
                        break;
                    }
                }
            }
            else {
                found = FALSE;
            }

            if (found) {
DebugPrint(1,("CALL lpx_OpenAddress\n")); 
DebugPrint(1,("irpsp ==> %0x\n",IrpSp));
                Status = lpx_OpenAddress (DeviceObject, Irp, IrpSp);
                goto DONE;
            }

			 //
            // Connection?
            //

            found = TRUE;

            if ((USHORT)openType->EaNameLength == TDI_CONNECTION_CONTEXT_LENGTH) {
                for (i = 0; i < TDI_CONNECTION_CONTEXT_LENGTH; i++) {
                    if (openType->EaName[i] != TdiConnectionContext[i]) {
                        found = FALSE;
                        break;
                    }
                }
            }
            else {
                found = FALSE;
            }

            if (found) {
DebugPrint(1,("CALL lpx_OpenConnection\n"));  
                Status = lpx_OpenConnection (DeviceObject, Irp, IrpSp);
                goto DONE;
            }
			
			goto ERROR;
		} else 
		{
			

            IrpSp->FileObject->FsContext = (PVOID)(DeviceContext->ControlChannelIdentifier);
            ++DeviceContext->ControlChannelIdentifier;

            if (DeviceContext->ControlChannelIdentifier == 0) {
                DeviceContext->ControlChannelIdentifier = 1;
            }

			
DebugPrint(1,("CALL LPX_FILE_TYPE_CONTROL\n")); 
            IrpSp->FileObject->FsContext2 = (PVOID)LPX_FILE_TYPE_CONTROL;
            Status = STATUS_SUCCESS;
		}

	} else {
ERROR:
		DebugPrint(1,("ERROR NOT SUPPORTED\n"));
		Status = STATUS_INVALID_DEVICE_REQUEST;

	}

DONE:	
	if(Status == STATUS_PENDING){

	}else {
		IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
	}

   LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); //CONTROL ref - : 0
DebugPrint(1,("LEAVE LpxOpen\n"));   
   return Status;
}


NTSTATUS
LpxClose(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    )
{
    KIRQL oldirql;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_FULL_EA_INFORMATION openType;
    USHORT i;
    BOOLEAN found;
	PTP_ADDRESS		Address;
    PTP_CONNECTION Connection;

DebugPrint(0,("ENTER LpxClose\n"));
    try {

        DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {

            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        
		} else if (DeviceContext->Type != CONTROL_DEVICE_SIGNATURE){

			Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_DEVICE_DOES_NOT_EXIST;	
        }

        // Reference the device so that it does not go away from under us
		LPX_REFERENCE_DEVICECONTEXT(DeviceContext); //CONTROL ref + :1
		
    } except(EXCEPTION_EXECUTE_HANDLER) {
		
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

   //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //
	if(IrpSp->MajorFunction == IRP_MJ_CLOSE) {
		switch (PtrToUlong(IrpSp->FileObject->FsContext2)) {
        
		case TDI_TRANSPORT_ADDRESS_FILE:
            Address = (PTP_ADDRESS)IrpSp->FileObject->FsContext;

            //
            // This creates a reference to AddressFile->Address
            // which is removed by NbfCloseAddress.
            //
			DebugPrint(DebugLevel,("LpxClose : Address\n"));
            Status = lpx_VerifyAddressObject(Address); //Address ref + : 1

            if (!NT_SUCCESS (Status)) {
				//lpx_DereferenceAddress (Address);
                Status = STATUS_INVALID_HANDLE;
            } else {
				DebugPrint(DebugLevel,("LpxClose : Address : valid Address object\n"));
                Status = lpx_CloseAddress (DeviceObject, Irp, IrpSp);
				lpx_DereferenceAddress (Address); //Address ref - : 0
            }

            break;

        case TDI_CONNECTION_FILE:

            //
            // This is a connection
            //

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
			DebugPrint(DebugLevel,("LpxClose : Connection\n"));
            Status = lpx_VerifyConnectionObject (Connection); // connection ref + : 1
            if (NT_SUCCESS (Status)) {

                Status = lpx_CloseConnection (DeviceObject, Irp, IrpSp);
                lpx_DereferenceConnection (Connection);	 //connection ref - : 0
            }

            break;

        case LPX_FILE_TYPE_CONTROL:

            //
            // this always succeeds
            //

            Status = STATUS_SUCCESS;
            break;

        default:

            Status = STATUS_INVALID_HANDLE;
        }


	} else {

		Status = STATUS_INVALID_DEVICE_REQUEST;
	}


	if(Status == STATUS_PENDING){

	}else {
		IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
	}

   LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); //CONTROL ref - : 0 
DebugPrint(0,("LEAVE LpxClose\n"));
   return Status;
	
}

NTSTATUS
LpxCleanup(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    )
{
    KIRQL oldirql;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_FULL_EA_INFORMATION openType;
    USHORT i;
    BOOLEAN found;
	PTP_ADDRESS		Address;
    PTP_CONNECTION Connection;

DebugPrint(0,("ENTER LpxCleanup\n"));
    try {

        DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {

            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        
		} else if (DeviceContext->Type != CONTROL_DEVICE_SIGNATURE){

			Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_DEVICE_DOES_NOT_EXIST;	
        }

        // Reference the device so that it does not go away from under us
		LPX_REFERENCE_DEVICECONTEXT(DeviceContext); //CONTROL ref + : 1
        
    } except(EXCEPTION_EXECUTE_HANDLER) {
		
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

   //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //
	if(IrpSp->MajorFunction == IRP_MJ_CLEANUP) {
        switch (PtrToUlong(IrpSp->FileObject->FsContext2)) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            Address = (PTP_ADDRESS)IrpSp->FileObject->FsContext;
            Status = lpx_VerifyAddressObject(Address); //address ref + : 1
            if (!NT_SUCCESS (Status)) {

                Status = STATUS_INVALID_HANDLE;

            } else {
				DebugPrint(DebugLevel,("LpxCleanup : Address\n"));
                lpx_StopAddress(Address);
                lpx_DereferenceAddress (Address);  // address ref - : 0
                Status = STATUS_SUCCESS;
            }

            break;

        case TDI_CONNECTION_FILE:
            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
            Status = lpx_VerifyConnectionObject (Connection); // connection ref + : 1
            if (NT_SUCCESS (Status)) {
            	
                ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
                lpx_StopConnection (Connection, STATUS_LOCAL_DISCONNECT);
                RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
                Status = STATUS_SUCCESS;
                lpx_DereferenceConnection (Connection); // connection ref - : 0
            }

            break;

        case LPX_FILE_TYPE_CONTROL:

            lpx_StopControlChannel(
                (PCONTROL_DEVICE_CONTEXT)DeviceObject,
                (USHORT)IrpSp->FileObject->FsContext
                );

            Status = STATUS_SUCCESS;
            break;

        default:

            Status = STATUS_INVALID_HANDLE;
        }


	} else {

		Status = STATUS_INVALID_DEVICE_REQUEST;
	}

	if(Status == STATUS_PENDING){

	}else {
		IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
	}

   LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); // CONTROl ref - : 0
DebugPrint(0,("LEAVE LpxCleanup\n"));
   return Status;	
}

NTSTATUS
LpxInternalDevControl(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    )
{
    NTSTATUS Status;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    PIO_STACK_LOCATION IrpSp;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //
DebugPrint(1,("LpxInternalDevControl start\n "));
    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;

    try {
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {

            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        
		} else if (DeviceContext->Type != CONTROL_DEVICE_SIGNATURE){

			Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_DEVICE_DOES_NOT_EXIST;	
        }
    
        // Reference the device so that it does not go away from under us
        LPX_REFERENCE_DEVICECONTEXT(DeviceContext); // CONTROl ref + : 1
        
    } except(EXCEPTION_EXECUTE_HANDLER) {
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;


    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    switch (IrpSp->MinorFunction) {

        case TDI_ACCEPT:
DebugPrint(1,("CALL TDI_ACCEPT \n "));
            Status = lpx_TdiAccept (Irp);
            break;

        case TDI_ACTION:
DebugPrint(1,("CALL TDI_ACTION \n "));
            Status = lpx_TdiAction (DeviceContext, Irp);
            break;

        case TDI_ASSOCIATE_ADDRESS:
DebugPrint(1,("CALL TDI_ASSOCIATE_ADDRESS \n "));
            Status = lpx_TdiAssociateAddress (Irp);
            break;

        case TDI_DISASSOCIATE_ADDRESS:
DebugPrint(1,("CALL TDI_DISASSOCIATE_ADDRESS \n "));
            Status = lpx_TdiDisassociateAddress (Irp);
            break;

        case TDI_CONNECT:
DebugPrint(1,("CALL TDI_CONNECT \n "));  
            Status = lpx_TdiConnect (Irp);

            break;

        case TDI_DISCONNECT:
DebugPrint(1,("CALL TDI_DISCONNECT \n "));  
            Status = lpx_TdiDisconnect (Irp);
            break;

        case TDI_LISTEN:
DebugPrint(1,("CALL TDI_LISTEN \n "));  
            Status = lpx_TdiListen (Irp);
            break;

        case TDI_QUERY_INFORMATION:
DebugPrint(1,("CALL TDI_QUERY_INFORMATION \n "));  
            Status = lpx_TdiQueryInformation (DeviceContext, Irp);
            break;

        case TDI_RECEIVE:
DebugPrint(1,("CALL TDI_RECEIVE \n ")); 
            Status =  lpx_TdiReceive (Irp);
            break;

        case TDI_RECEIVE_DATAGRAM:
DebugPrint(1,("CALL TDI_RECEIVE_DATAGRAM \n ")); 
            Status =  lpx_TdiReceiveDatagram (Irp);
            break;

        case TDI_SEND:
DebugPrint(1,("CALL TDI_SEND \n "));
            Status =  lpx_TdiSend (Irp);
            break;

        case TDI_SEND_DATAGRAM:
DebugPrint(1,("CALL TDI_SEND_DATAGRAM \n "));
           Status = lpx_TdiSendDatagram (Irp);
            break;

        case TDI_SET_EVENT_HANDLER:
DebugPrint(1,("CALL TDI_SET_EVENT_HANDLER \n ")); 
            Status = lpx_TdiSetEventHandler (Irp);
            break;

        case TDI_SET_INFORMATION:
DebugPrint(1,("CALL TDI_SET_INFORMATION \n "));    
            Status = lpx_TdiSetInformation (Irp);
            break;

        default:

            Status = STATUS_INVALID_DEVICE_REQUEST;
    }

    if (Status == STATUS_PENDING) {
   
    } else {
 
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
 
    }


    // Remove the temp use reference on device context added above
	LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); // CONTROL ref - : 0

    //
    // Return the immediate status code to the caller.
    //

    return Status;

} /* LpxDispatchInternal */

NTSTATUS
LpxDevControl(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    )
{
    BOOL DeviceControlIrp = FALSE;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PCONTROL_DEVICE_CONTEXT DeviceContext;



    //
    // Check to see if NBF has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    try {
        DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
   
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        
		} else if (DeviceContext->Type != CONTROL_DEVICE_SIGNATURE){

			Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_DEVICE_DOES_NOT_EXIST;	
        }

        // Reference the device so that it does not go away from under us
        LPX_REFERENCE_DEVICECONTEXT(DeviceContext);	//CONTROL ref + : 0

        
    } except(EXCEPTION_EXECUTE_HANDLER) {
    
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    
    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //


    if(IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

		DeviceControlIrp = TRUE;
		Status = lpx_DeviceControl (DeviceObject, Irp, IrpSp);
	
	}else{
		
		Status = STATUS_INVALID_DEVICE_REQUEST;
	}


    if (Status == STATUS_PENDING) {
  
    } else {

        //
        // LpxDeviceControl would have completed this IRP already
        //

        if (!DeviceControlIrp)
        {
           
            IrpSp->Control &= ~SL_PENDING_RETURNED;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
          
        }
    }

    // Remove the temp use reference on device context added above
 	LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); // CONTROL ref - : 0

    
    //
    // Return the immediate status code to the caller.
    //

   
    return Status;
} /* NbfDispatch */

NTSTATUS
LpxPnPPower(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    )
{
    BOOL DeviceControlIrp = FALSE;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PCONTROL_DEVICE_CONTEXT DeviceContext;



    //
    // Check to see if NBF has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    try {
        DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
   
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        
		} else if (DeviceContext->Type != CONTROL_DEVICE_SIGNATURE){

			Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_DEVICE_DOES_NOT_EXIST;	
        }

        // Reference the device so that it does not go away from under us
        LPX_REFERENCE_DEVICECONTEXT(DeviceContext); //CONTROL ref + : 0

        
    } except(EXCEPTION_EXECUTE_HANDLER) {
    
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    
    //
    // Make sure status information is consistent every time.
    //

    IoMarkIrpPending (Irp);
    Irp->IoStatus.Status = STATUS_PENDING;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //


    if(IrpSp->MajorFunction == IRP_MJ_PNP) {

		Status = lpx_DispatchPnPPower (DeviceObject, Irp, IrpSp);
	
	}else{
		
		Status = STATUS_INVALID_DEVICE_REQUEST;
	}


    if (Status == STATUS_PENDING) {
  
    } else {

      
           
            IrpSp->Control &= ~SL_PENDING_RETURNED;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
          
        
    }

    // Remove the temp use reference on device context added above
 	LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); //CONTROL ref - : 0

    
    //
    // Return the immediate status code to the caller.
    //

   
    return Status;
}

/*
	Generate control device driver and extension. 
	And do initialization procedure
		- AddressFile structure
		- Address	structure

*/
VOID
LpxUnload(
	IN PDRIVER_OBJECT  DriverObject 
    )
{

    PDEVICE_CONTEXT DeviceContext;
    PLIST_ENTRY p;
    KIRQL       oldIrql;
    UNREFERENCED_PARAMETER (DriverObject);

 


    //
    // Walk the list of device contexts.
    //

    ACQUIRE_DEVICES_LIST_LOCK(); // Global Ndis Device + : 1

    while (!IsListEmpty (&global.NIC_DevList)) {

        // Remove an entry from list and reset its
        // links (as we might try to remove from
        // the list again - when ref goes to zero)
        p = RemoveHeadList (&global.NIC_DevList);

        InitializeListHead(p);

        DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, Linkage);

        DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

        // Remove creation ref if it has not already been removed
        if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

            RELEASE_DEVICES_LIST_LOCK();	//Global Ndis Device - : 0
			// do something


            // Remove creation reference
            LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext); // NDIS Device ref - : -1

            ACQUIRE_DEVICES_LIST_LOCK();	// Global Ndis Device + : 1

        } else {
            RELEASE_DEVICES_LIST_LOCK();	//Global Ndis Device - : 0

            // Remove creation reference
            LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext);	// NDIS Device ref - : -1

            ACQUIRE_DEVICES_LIST_LOCK();	// Global Ndis Device + : 1

        }
    }

    RELEASE_DEVICES_LIST_LOCK();	//Global Ndis Device - : 0
	
    //
    // Then remove ourselves as an NDIS protocol.
    //

	if(global.NdisProtocolHandle){
		NDIS_STATUS ndisStatus;
		NdisDeregisterProtocol(
						&ndisStatus,
						global.NdisProtocolHandle);
		global.NdisProtocolHandle = (NDIS_HANDLE)NULL;
	}
    //
    // Deregister from TDI layer as a network provider
    //
	if(global.TdiProviderHandle){
		TdiDeregisterProvider(global.TdiProviderHandle);
		global.TdiProviderHandle = NULL;
	}

    //
    // Finally free any memory allocated for config info
    //
    if (global.Config != NULL) {

        // Free configuration block
        Lpx_FreeConfigurationInfo(global.Config);

    }

    //
    // Free memory allocated in DriverEntry for reg path
    //
    
    ExFreePool(global.RegistryPath.Buffer);


    return;

}