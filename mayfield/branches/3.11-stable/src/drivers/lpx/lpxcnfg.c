/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop

//
// Local functions used to access the registry.
//

VOID
LpxFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
LpxOpenParametersKey(
    IN HANDLE LpxConfigHandle,
    OUT PHANDLE ParametersHandle
    );

VOID
LpxCloseParametersKey(
    IN HANDLE ParametersHandle
    );

NTSTATUS
LpxCountEntries(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
LpxAddBind(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
LpxAddExport(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
LpxReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    );

ULONG
LpxReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    );

VOID
LpxWriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    );

UINT
LpxWstrLength(
    IN PWSTR Wstr
    );

NTSTATUS
LpxMatchBindName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
LpxExportAtIndex(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,LpxWstrLength)
#pragma alloc_text(PAGE,LpxConfigureTransport)
#pragma alloc_text(PAGE,LpxFreeConfigurationInfo)
#pragma alloc_text(PAGE,LpxOpenParametersKey)
#pragma alloc_text(PAGE,LpxCloseParametersKey)
#pragma alloc_text(PAGE,LpxCountEntries)
#pragma alloc_text(PAGE,LpxAddBind)
#pragma alloc_text(PAGE,LpxAddExport)
#pragma alloc_text(PAGE,LpxReadLinkageInformation)
#pragma alloc_text(PAGE,LpxReadSingleParameter)
#pragma alloc_text(PAGE,LpxWriteSingleParameter)
#endif

UINT
LpxWstrLength(
    IN PWSTR Wstr
    )
{
    UINT Length = 0;
    while (*Wstr++) {
        Length += sizeof(WCHAR);
    }
    return Length;
}

#define InsertAdapter(ConfigurationInfo, Subscript, Name)                \
{ \
    PWSTR _S; \
    PWSTR _N = (Name); \
    UINT _L = LpxWstrLength(_N)+sizeof(WCHAR); \
    _S = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, _L, LPX_MEM_TAG_DEVICE_EXPORT); \
    if (_S != NULL) { \
        RtlCopyMemory(_S, _N, _L); \
        RtlInitUnicodeString (&(ConfigurationInfo)->Names[Subscript], _S); \
    } \
}

#define InsertDevice(ConfigurationInfo, Subscript, Name)                \
{ \
    PWSTR _S; \
    PWSTR _N = (Name); \
    UINT _L = LpxWstrLength(_N)+sizeof(WCHAR); \
    _S = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, _L, LPX_MEM_TAG_DEVICE_EXPORT); \
    if (_S != NULL) { \
        RtlCopyMemory(_S, _N, _L); \
        RtlInitUnicodeString (&(ConfigurationInfo)->Names[(ConfigurationInfo)->DevicesOffset+Subscript], _S); \
    } \
}


#define RemoveAdapter(ConfigurationInfo, Subscript)                \
    ExFreePool ((ConfigurationInfo)->Names[Subscript].Buffer)

#define RemoveDevice(ConfigurationInfo, Subscript)                \
    ExFreePool ((ConfigurationInfo)->Names[(ConfigurationInfo)->DevicesOffset+Subscript].Buffer)



//
// These strings are used in various places by the registry.
//

#define DECLARE_STRING(_str_) WCHAR Str ## _str_[] = L#_str_


#define READ_HIDDEN_CONFIG(_Field) \
{ \
    ConfigurationInfo->_Field = \
        LpxReadSingleParameter( \
             ParametersHandle, \
             Str ## _Field, \
             ConfigurationInfo->_Field); \
}

#define WRITE_HIDDEN_CONFIG(_Field) \
{ \
    LpxWriteSingleParameter( \
        ParametersHandle, \
        Str ## _Field, \
        ConfigurationInfo->_Field); \
}



NTSTATUS
LpxConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigurationInfoPtr
    )
/*++

Routine Description:

    This routine is called by LPX to get information from the configuration
    management routines. We read the registry, starting at RegistryPath,
    to get the parameters. If they don't exist, we use the defaults
    set in lpxcnfg.h file.

Arguments:

    RegistryPath - The name of LPX's node in the registry.

    ConfigurationInfoPtr - A pointer to the configuration information structure.

Return Value:

    Status - STATUS_SUCCESS if everything OK, STATUS_INSUFFICIENT_RESOURCES
            otherwise.

--*/
{

    NTSTATUS OpenStatus;
    HANDLE ParametersHandle;
    HANDLE LpxConfigHandle;
    NTSTATUS Status;
    ULONG Disposition;
    PWSTR RegistryPathBuffer;
    OBJECT_ATTRIBUTES TmpObjectAttributes;
    PCONFIG_DATA ConfigurationInfo;

     DECLARE_STRING(InitConnections);
    DECLARE_STRING(InitAddressFiles);
    DECLARE_STRING(InitAddresses);

    DECLARE_STRING(MaxConnections);
    DECLARE_STRING(MaxAddressFiles);
    DECLARE_STRING(MaxAddresses);

    DECLARE_STRING(InitPackets);
    DECLARE_STRING(InitReceivePackets);
    DECLARE_STRING(InitReceiveBuffers);

    DECLARE_STRING(SendPacketPoolSize);
    DECLARE_STRING(ReceivePacketPoolSize);
 
     DECLARE_STRING(GeneralRetries);
    DECLARE_STRING(GeneralTimeout);
 
    DECLARE_STRING(UseDixOverEthernet);


    // LPX specific configurations
    DECLARE_STRING(ConnectionTimeout);
    DECLARE_STRING(SmpTimeout);
    DECLARE_STRING(WaitInterval);
    DECLARE_STRING(AliveInterval);
    DECLARE_STRING(RetransmitDelay);
    DECLARE_STRING(MaxRetransmitDelay);
    DECLARE_STRING(MaxAliveCount);
    DECLARE_STRING(MaxRetransmitTime);
    DECLARE_STRING(MTU);
    
    //
    // Open the registry.
    //

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        RegistryPath,               // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwCreateKey(
                 &LpxConfigHandle,
                 KEY_WRITE,
                 &TmpObjectAttributes,
                 0,                 // title index
                 NULL,              // class
                 0,                 // create options
                 &Disposition);     // disposition

    if (!NT_SUCCESS(Status)) {
        LpxPrint1("LPX: Could not open/create LPX key: %lx\n", Status);
        return Status;
    }

    IF_LPXDBG (LPX_DEBUG_REGISTRY) {
        LpxPrint2("%s LPX key: %p\n",
            (Disposition == REG_CREATED_NEW_KEY) ? "created" : "opened",
            LpxConfigHandle);
    }


    OpenStatus = LpxOpenParametersKey (LpxConfigHandle, &ParametersHandle);

    if (OpenStatus != STATUS_SUCCESS) {
        return OpenStatus;
    }

    //
    // Read in the NDIS binding information (if none is present
    // the array will be filled with all known drivers).
    //
    // LpxReadLinkageInformation expects a null-terminated path,
    // so we have to create one from the UNICODE_STRING.
    //

    RegistryPathBuffer = (PWSTR)ExAllocatePoolWithTag(
                                    NonPagedPool,
                                    RegistryPath->Length + sizeof(WCHAR),
                                    LPX_MEM_TAG_REGISTRY_PATH);
    if (RegistryPathBuffer == NULL) {
        LpxCloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory (RegistryPathBuffer, RegistryPath->Buffer, RegistryPath->Length);
    *(PWCHAR)(((PUCHAR)RegistryPathBuffer)+RegistryPath->Length) = (WCHAR)'\0';

    LpxReadLinkageInformation (RegistryPathBuffer, ConfigurationInfoPtr);

    if (*ConfigurationInfoPtr == NULL) {
        ExFreePool (RegistryPathBuffer);
        LpxCloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ConfigurationInfo = *ConfigurationInfoPtr;


    //
    // Configure the initial values for some LPX resources.
    //

#if 1
    ConfigurationInfo->InitConnections = 0;
    ConfigurationInfo->InitAddressFiles = 0;
    ConfigurationInfo->InitAddresses = 0;
#else
    ConfigurationInfo->InitConnections = 10;
    ConfigurationInfo->InitAddressFiles = 10;
    ConfigurationInfo->InitAddresses = 10;
#endif
    ConfigurationInfo->ConnectionTimeout = DEFAULT_CONNECTION_TIMEOUT;
    ConfigurationInfo->SmpTimeout = DEFAULT_SMP_TIMEOUT;
    ConfigurationInfo->WaitInterval = DEFAULT_TIME_WAIT_INTERVAL;
    ConfigurationInfo->AliveInterval = DEFAULT_ALIVE_INTERVAL;
    ConfigurationInfo->RetransmitDelay = DEFAULT_RETRANSMIT_DELAY;
    ConfigurationInfo->MaxRetransmitDelay = DEFAULT_MAX_RETRANSMIT_DELAY;
    
#if 1
    ConfigurationInfo->MaxAliveCount = DEFAULT_MAX_ALIVE_COUNT;
    ConfigurationInfo->MaxRetransmitTime = DEFAULT_MAX_RETRANSMIT_TIME;
#else
    ConfigurationInfo->MaxAliveCount = 2;
    ConfigurationInfo->MaxRetransmitTime = 8000;
#endif

    //
    // Now initialize the timeout etc. values.
    //
    
  
    ConfigurationInfo->UseDixOverEthernet = 0;
    ConfigurationInfo->MTU = DEFAULT_MAXIMUM_TRANSFER_UNIT;
 

    //
    // Now read the optional "hidden" parameters; if these do
    // not exist then the current values are used. Note that
    // the current values will be 0 unless they have been
    // explicitly initialized above.
    //
    // NOTE: These macros expect "ConfigurationInfo" and
    // "ParametersHandle" to exist when they are expanded.
    //

    READ_HIDDEN_CONFIG (InitConnections);
    READ_HIDDEN_CONFIG (InitAddressFiles);
    READ_HIDDEN_CONFIG (InitAddresses);

    READ_HIDDEN_CONFIG (MaxConnections);
    READ_HIDDEN_CONFIG (MaxAddressFiles);
    READ_HIDDEN_CONFIG (MaxAddresses);

    READ_HIDDEN_CONFIG (UseDixOverEthernet);

    READ_HIDDEN_CONFIG(ConnectionTimeout);
    READ_HIDDEN_CONFIG(SmpTimeout);
    READ_HIDDEN_CONFIG(WaitInterval);
    READ_HIDDEN_CONFIG(AliveInterval);
    READ_HIDDEN_CONFIG(RetransmitDelay);
    READ_HIDDEN_CONFIG(MaxRetransmitDelay);
    READ_HIDDEN_CONFIG(MaxAliveCount);
    READ_HIDDEN_CONFIG(MaxRetransmitTime);
    READ_HIDDEN_CONFIG(MTU);

    LpxConnectionTimeout = MSEC_TO_HZ(ConfigurationInfo->ConnectionTimeout);
    LpxSmpTimeout = MSEC_TO_HZ(ConfigurationInfo->SmpTimeout);
    LpxWaitInterval = MSEC_TO_HZ(ConfigurationInfo->WaitInterval);
    LpxAliveInterval = MSEC_TO_HZ(ConfigurationInfo->AliveInterval);
    LpxRetransmitDelay = MSEC_TO_HZ(ConfigurationInfo->RetransmitDelay);
    LpxMaxRetransmitDelay = MSEC_TO_HZ(ConfigurationInfo->MaxRetransmitDelay);
    LpxMaxAliveCount = ConfigurationInfo->MaxAliveCount;
    LpxMaxRetransmitTime = MSEC_TO_HZ(ConfigurationInfo->MaxRetransmitTime);
    LpxMaximumTransferUnit = ConfigurationInfo->MTU;

 
    //
    // Save the "hidden" parameters, these may not exist in
    // the registry.
    //
    // NOTE: These macros expect "ConfigurationInfo" and
    // "ParametersHandle" to exist when they are expanded.
    //

    //
    // 5/22/92 - don't write the parameters that are set
    // based on Size, since otherwise these will overwrite
    // those values since hidden parameters are set up
    // after the Size-based configuration is done.
    //

//    WRITE_HIDDEN_CONFIG (MaxConnections);
//    WRITE_HIDDEN_CONFIG (MaxAddressFiles);
//    WRITE_HIDDEN_CONFIG (MaxAddresses);

 
 
//    WRITE_HIDDEN_CONFIG (UseDixOverEthernet);
 
    // ZwFlushKey (ParametersHandle);

    ExFreePool (RegistryPathBuffer);
    LpxCloseParametersKey (ParametersHandle);
    ZwClose (LpxConfigHandle);

    return STATUS_SUCCESS;

}   /* LpxConfigureTransport */


VOID
LpxFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    )

/*++

Routine Description:
st types to different handlers based
	on the minor IOCTL function code in the IRP's current stack location.
	In addition to cracking the minor function code, this routine also
	reaches into the IRP and passes the packetized parameters stored there
	as parameters to the various TDI request handlers so that they are
	not IRP-dependent.

Arguments:

	DeviceObject - Pointer to the device object for this driver.

	Irp - Pointer to the request packet representing the I/O request.

Return Value:

	The function value is the status of the operation.

--*/

{
	NTSTATUS Status;
	PCONTROL_CONTEXT DeviceContext;
	PIO_STACK_LOCATION IrpSp;
#if DBG
	KIRQL IrqlOnEnter = KeGetCurrentIrql();
#endif


	IF_LPXDBG (LPX_DEBUG_DISPATCH) {
		LpxPrint0 ("LpxInternalDeviceControl: Entered.\n");
	}

	//
	// Get a pointer to the current stack location in the IRP.  This is where
	// the function codes and parameters are stored.
	//

	IrpSp = IoGetCurrentIrpStackLocation (Irp);

	DeviceContext = (PCONTROL_CONTEXT)DeviceObject;
	ASSERT( LpxControlDeviceContext == DeviceContext) ;
	try {
		if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
			Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
			IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
			return STATUS_INVALID_DEVICE_STATE;
		}
	
		// Reference the device so that it does not go away from under us
		LpxReferenceControlContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);
		
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


	IF_LPXDBG (LPX_DEBUG_DISPATCH) {
		{
			PULONG Temp=(PULONG)&IrpSp->Parameters;
			LpxPrint5 ("Got IrpSp %lx %lx %lx %lx %lx\n", *(Temp++),  *(Temp++),
				*(Temp++), *(Temp++), *(Temp++));
		}
	}

	//
	// Branch to the appropriate request handler.  Preliminary checking of
	// the size of the request block is performed here so that it is known
	// in the handlers that the minimum input parameters are readable.  It
	// is *not* determined here whether variable length input fields are
	// passed correctly; this is a check which must be made within each routine.
	//

	switch (IrpSp->MinorFunction) {

		case TDI_ACCEPT:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiAccept request.\n");
			}

			Status = LpxTdiAccept (Irp);
			break;

		case TDI_ACTION:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiAction request.\n");
			}
			Status = STATUS_NOT_SUPPORTED; //LpxTdiAction
			break;

		case TDI_ASSOCIATE_ADDRESS:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiAccept request.\n");
			}
			DebugPrint(2,("LpxDispatchInternal: LpxTdiAssociateAddress request.\n"));
			Status = LpxTdiAssociateAddress (Irp);
			break;

		case TDI_DISASSOCIATE_ADDRESS:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiDisassociateAddress request.\n");
			}
			DebugPrint(3,("LpxDispatchInternal: LpxTdiDisassociateAddress request.\n"));
			Status = LpxTdiDisassociateAddress (Irp);
			break;

		case TDI_CONNECT:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiConnect request\n");
			}

			Status = LpxTdiConnect (Irp);
			break;

		case TDI_DISCONNECT:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiDisconnect request.\n");
			}

			Status = LpxTdiDisconnect (Irp);
			break;

		case TDI_LISTEN:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiListen request.\n");
			}

			Status = LpxTdiListen (Irp);
			break;

		case TDI_QUERY_INFORMATION:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiQueryInformation request.\n");
			}

			Status = LpxTdiQueryInformation (DeviceContext, Irp);
			break;

		case TDI_RECEIVE:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiReceive request.\n");
			}

			Status =  LpxTdiReceive (Irp);
			break;

		case TDI_RECEIVE_DATAGRAM:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiReceiveDatagram request.\n");
			}

			Status =  LpxTdiReceiveDatagram (Irp);
			break;

		case TDI_SEND:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiSend request.\n");
			}

			Status =  LpxTdiSend (Irp);
			break;

		case TDI_SEND_DATAGRAM:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiSendDatagram request.\n");
		   }

		   Status = LpxTdiSendDatagram (Irp);
			break;

		case TDI_SET_EVENT_HANDLER:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiSetEventHandler request.\n");
			}

			//
			// Because this request will enable direct callouts from the
			// transport provider at DISPATCH_LEVEL to a client-specified
			// routine, this request is only valid in kernel mode, denying
			// access to this request in user mode.
			//

			Status = LpxTdiSetEventHandler (Irp);
			break;

		case TDI_SET_INFORMATION:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint0 ("LpxDispatchInternal: TdiSetInformation request.\n");
			}

			Status = LpxTdiSetInformation (Irp);
			break;

		//
		// Something we don't know about was submitted.
		//

		default:
			IF_LPXDBG (LPX_DEBUG_DISPATCH) {
				LpxPrint1 ("LpxDispatchInternal: invalid request type %lx\n",
				IrpSp->MinorFunction);
			}
			Status = STATUS_INVALID_DEVICE_REQUEST;

			break;
	}

	if (Status == STATUS_PENDING) {
		IF_LPXDBG (LPX_DEBUG_DISPATCH) {
			LpxPrint0 ("LpxDispatchInternal: request PENDING from handler.\n");
		}
	} else {
		IF_LPXDBG (LPX_DEBUG_DISPATCH) {
			LpxPrint0 ("LpxDispatchInternal: request COMPLETED by handler.\n");
		}

		IrpSp->Control &= ~SL_PENDING_RETURNED;
		Irp->IoStatus.Status = Status;
		IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
	}

	IF_LPXDBG (LPX_DEBUG_DISPATCH) {
		LpxPrint1 ("LpxDispatchInternal: exiting, status: %lx\n",Status);
	}

	// Remove the temp use reference on device context added above
	LpxDereferenceControlContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);

	//
	// Return the immediate status code to the caller.
	//

#if DBG
	ASSERT (KeGetCurrentIrql() == IrqlOnEnter);
#endif
	return Status;
} /* LpxDispatchInternal */

ULONG
LpxInitializeOneDeviceContext(
						        OUT PNDIS_STATUS NdisStatus,
						        IN PDRIVER_OBJECT DriverObject,
						        IN PCONFIG_DATA LpxConfig,
						        IN PUNICODE_STRING BindName,
						        IN PUNICODE_STRING ExportName,
						        IN PVOID SystemSpecific1,
						        IN PVOID SystemSpecific2
						     )
/*++

Routine Description:

	This routine creates and initializes one lpx device context.  In order to
	do this it must successfully open and bind to the adapter described by
	lpxconfig->names[adapterindex].

Arguments:

	NdisStatus   - The outputted status of the operations.

	DriverObject - the lpx driver object.

	LpxConfig	- the transport configuration information from the registry.

	SystemSpecific1 - SystemSpecific1 argument to ProtocolBindAdapter

	SystemSpecific2 - SystemSpecific2 argument to ProtocolBindAdapter

Return Value:

	The number of successful binds.

--*/

{
	ULONG i;
	PDEVICE_CONTEXT DeviceContext;
//	 PTP_CONNECTION Connection;
	PTP_ADDRESS_FILE AddressFile;
	PTP_ADDRESS Address;
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING DeviceString;
	UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_LPX];
	PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
	struct {
		TDI_PNP_CONTEXT tdiPnPContextHeader;
		PVOID		   tdiPnPContextTrailer;
	} tdiPnPContext2;
	pAddress->AddressLength = TDI_ADDRESS_LENGTH_LPX;
	pAddress->AddressType = TDI_ADDRESS_TYPE_LPX;


	//
	// Loop through all the adapters that are in the configuration
	// information structure. Allocate a device object for each
	// one that we find.
	//

	status = LpxCreateDeviceContext(
						            DriverObject,
						            ExportName,
						            &DeviceContext
						           );

	if (!NT_SUCCESS (status)) {

		IF_LPXDBG (LPX_DEBUG_PNP) {
			LpxPrint2 ("LpxCreateDeviceContext for %S returned error %08x\n",
						    ExportName->Buffer, status);
		}

		//
		// First check if we already have an object with this name
		// This is because a previous unbind was not done properly.
		//

		if (status == STATUS_OBJECT_NAME_COLLISION) {

			// See if we can reuse the binding and device name

			LpxReInitializeDeviceContext(
						                 &status,
						                 DriverObject,
						                 LpxConfig,
						                 BindName,
						                 ExportName,
						                 SystemSpecific1,
						                 SystemSpecific2
						                );

			if (status == STATUS_NOT_FOUND)
			{
				// Must have got deleted in the meantime

				return LpxInitializeOneDeviceContext(
						                             NdisStatus,
						                             DriverObject,
						                             LpxConfig,
						                             BindName,
						                             ExportName,
						                             SystemSpecific1,
						                             SystemSpecific2
						                            );
			}
		}

		*NdisStatus = status;

		if (!NT_SUCCESS (status))
		{
			// error 
			return(0);
		}
		return(1);
	}

  
	//
	// Initialize our counter that records memory usage.
	//
 
	DeviceContext->MaxConnections = LpxConfig->MaxConnections;
	DeviceContext->MaxAddressFiles = LpxConfig->MaxAddressFiles;
	DeviceContext->MaxAddresses = LpxConfig->MaxAddresses;

	//
	// Now fire up NDIS so this adapter talks
	//

	status = LpxInitializeNdis (DeviceContext,
						        LpxConfig,
						        BindName);

	if (!NT_SUCCESS (status)) {

		if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
			LpxDereferenceDeviceContext ("Initialize NDIS failed", DeviceContext, DCREF_CREATION);
		}
		
		*NdisStatus = status;
		return(0);

	}

#if 0
	DbgPrint("Opened %S as %S\n", &LpxConfig->Names[j], &nameString);
#endif

	IF_LPXDBG (LPX_DEBUG_RESOURCE) {
		LpxPrint6 ("LpxInitialize: NDIS returned: %x %x %x %x %x %x as local address.\n",
			DeviceContext->LocalAddress.Address[0],
			DeviceContext->LocalAddress.Address[1],
			DeviceContext->LocalAddress.Address[2],
			DeviceContext->LocalAddress.Address[3],
			DeviceContext->LocalAddress.Address[4],
			DeviceContext->LocalAddress.Address[5]);
	}

	//
	// Initialize our provider information structure; since it
	// doesn't change, we just keep it around and copy it to
	// whoever requests it.
	//


	MacReturnMaxDataSize(
		DeviceContext->MaxSendPacketSize,
		&DeviceContext->MaxUserData);

	IF_LPXDBG (LPX_DEBUG_RESOURCE) {
		LpxPrint0 ("LPXDRVR: allocating AddressFiles.\n");
	}
	for (i=0; i<LpxConfig->InitAddressFiles; i++) {

		LpxAllocateAddressFile (DeviceContext, &AddressFile);

		if (AddressFile == NULL) {
			PANIC ("LpxInitialize:  insufficient memory to allocate Address Files.\n");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto cleanup;
		}

		InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
	}

	DeviceContext->AddressFileInitAllocated = LpxConfig->InitAddressFiles;
	DeviceContext->AddressFileMaxAllocated = LpxConfig->MaxAddressFiles;

	IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
		LpxPrint1("%d address files\n", LpxConfig->InitAddressFiles );
	}


	IF_LPXDBG (LPX_DEBUG_RESOURCE) {
		LpxPrint0 ("LPXDRVR: allocating addresses.\n");
	}
	for (i=0; i<LpxConfig->InitAddresses; i++) {

		LpxAllocateAddress (DeviceContext, &Address);
		if (Address == NULL) {
			PANIC ("LpxInitialize:  insufficient memory to allocate addresses.\n");
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto cleanup;
		}

		InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
	}

	DeviceContext->AddressInitAllocated = LpxConfig->InitAddresses;
	DeviceContext->AddressMaxAllocated = LpxConfig->MaxAddresses;

	IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
		LpxPrint1("%d addresses\n", LpxConfig->InitAddresses );
	}

	// Store away the PDO for the underlying object
	DeviceContext->PnPContext = SystemSpecific2;

	DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

	//
	// Now link the device into the global list.
	//

	ACQUIRE_DEVICES_LIST_LOCK();
	InsertTailList (&LpxDeviceList, &DeviceContext->DeviceListLinkage);
	RELEASE_DEVICES_LIST_LOCK();

	DeviceObject = (PDEVICE_OBJECT) DeviceContext;
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

	IF_LPXDBG (LPX_DEBUG_PNP) {
		LpxPrint1 ("TdiRegisterDeviceObject for %S\n", DeviceString.Buffer);
	}

	status = TdiRegisterDeviceObject(&DeviceString,
						             &DeviceContext->TdiDeviceHandle);

	if (!NT_SUCCESS (status)) {
		ACQUIRE_DEVICES_LIST_LOCK();
		RemoveEntryList(&DeviceContext->DeviceListLinkage);
		RELEASE_DEVICES_LIST_LOCK();
		goto cleanup;
	}

	tdiPnPContext2.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
	tdiPnPContext2.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_PDO;
	*(PVOID UNALIGNED *) &tdiPnPContext2.tdiPnPContextHeader.ContextData = SystemSpecific2;

#ifdef __LPX__
{ 
	PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;
	RtlZeroMemory(LpxAddress, sizeof(TDI_ADDRESS_LPX));
	RtlCopyMemory (
		&LpxAddress->Node,
		DeviceContext->LocalAddress.Address,
		HARDWARE_ADDRESS_LENGTH
		);
} 
#endif

	status = TdiRegisterNetAddress(pAddress,
						           &DeviceString,
						           (TDI_PNP_CONTEXT *) &tdiPnPContext2,
						           &DeviceContext->ReservedAddressHandle);

	if (!NT_SUCCESS (status)) {
		RemoveEntryList(&DeviceContext->DeviceListLinkage);
		goto cleanup;
	}

	LpxReferenceDeviceContext ("Load Succeeded", DeviceContext, DCREF_CREATION);

	*NdisStatus = NDIS_STATUS_SUCCESS;

	return(1);

cleanup:

	 //
	// Cleanup whatever device context we were initializing
	// when we failed.
	//
	*NdisStatus = status;
	ASSERT(status != STATUS_SUCCESS);
	
	if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {
		// Remove creation reference
		LpxDereferenceDeviceContext ("Load failed", DeviceContext, DCREF_CREATION);
	}


	return (0);
}


VOID
LpxReInitializeDeviceContext(
						        OUT PNDIS_STATUS NdisStatus,
						        IN PDRIVER_OBJECT DriverObject,
						        IN PCONFIG_DATA LpxConfig,
						        IN PUNICODE_STRING BindName,
						        IN PUNICODE_STRING ExportName,
						        IN PVOID SystemSpecific1,
						        IN PVOID SystemSpecific2
						    )
/*++

Routine Description:

	This routine re-initializes an existing lpx device context. In order to
	do this, we need to undo whatever is done in the Unbind handler exposed
	to NDIS - recreate the NDIS binding, and re-start the LPX timer system.

Arguments:

	NdisStatus   - The outputted status of the operations.

	DriverObject - the driver object.

	LpxConfig	- the transport configuration information from the registry.

	SystemSpecific1 - SystemSpecific1 argument to ProtocolBindAdapter

	SystemSpecific2 - SystemSpecific2 argument to ProtocolBindAdapter

Return Value:

	None

--*/

{
	PDEVICE_CONTEXT DeviceContext;
//	KIRQL oldIrql;
	PLIST_ENTRY p;
	NTSTATUS status;
	UNICODE_STRING DeviceString;
	UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_LPX];
	PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
	struct {
		TDI_PNP_CONTEXT tdiPnPContextHeader;
		PVOID		   tdiPnPContextTrailer;
	} tdiPnPContext2;
	UNREFERENCED_PARAMETER(DriverObject) ;
	UNREFERENCED_PARAMETER(SystemSpecific1) ;
	UNREFERENCED_PARAMETER(SystemSpecific2) ;

	IF_LPXDBG (LPX_DEBUG_PNP) {
		LpxPrint1 ("ENTER LpxReInitializeDeviceContext for %S\n",
						ExportName->Buffer);
	}

	//
	// Search the list of LPX devices for a matching device name
	//
	
	ACQUIRE_DEVICES_LIST_LOCK();

	for (p = LpxDeviceList.Flink ; p != &LpxDeviceList; p = p->Flink)
	{
		DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, DeviceListLinkage);

		RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

		if (NdisEqualString(&DeviceString, ExportName, TRUE)) {
						    
			// This has to be a rebind - otherwise something wrong

			ASSERT(DeviceContext->CreateRefRemoved == TRUE);

			// Reference within lock so that it is not cleaned up

			LpxReferenceDeviceContext ("Reload Temp Use", DeviceContext, DCREF_TEMP_USE);
			DebugPrint(0,("[LPX]Found diabled Device Context for resuse !!  %p\n", DeviceContext));
			break;
		}
	}

	if (p == &LpxDeviceList)
	{
		RELEASE_DEVICES_LIST_LOCK();
		IF_LPXDBG (LPX_DEBUG_PNP) {
			LpxPrint2 ("LEAVE LpxReInitializeDeviceContext for %S with Status %08x\n",
						    ExportName->Buffer,
						    STATUS_NOT_FOUND);
		}

		*NdisStatus = STATUS_NOT_FOUND;

		return;
	}
	RELEASE_DEVICES_LIST_LOCK();

	//
	// Fire up NDIS again so this adapter talks
	//

	status = LpxInitializeNdis (DeviceContext,
						        LpxConfig,
						        BindName);

	if (!NT_SUCCESS (status)) {
		goto Cleanup;
	}

	// Store away the PDO for the underlying object
	DeviceContext->PnPContext = SystemSpecific2;

	DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

	//
	// Re-Indicate to TDI that new binding has arrived
	//

	status = TdiRegisterDeviceObject(&DeviceString,
						             &DeviceContext->TdiDeviceHandle);

	if (!NT_SUCCESS (status)) {
		goto Cleanup;
	}


	pAddress->AddressLength = TDI_ADDRESS_LENGTH_LPX;
	pAddress->AddressType = TDI_ADDRESS_TYPE_LPX;

	tdiPnPContext2.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
	tdiPnPContext2.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_PDO;
	*(PVOID UNALIGNED *) &tdiPnPContext2.tdiPnPContextHeader.ContextData = SystemSpecific2;

#ifdef __LPX__
{ 
	PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;
	RtlZeroMemory(LpxAddress, sizeof(TDI_ADDRESS_LPX));
	RtlCopyMemory (
		&LpxAddress->Node,
		DeviceContext->LocalAddress.Address,
		HARDWARE_ADDRESS_LENGTH
		);
} 
#endif

	status = TdiRegisterNetAddress(pAddress,
						           &DeviceString,
						           (TDI_PNP_CONTEXT *) &tdiPnPContext2,
						           &DeviceContext->ReservedAddressHandle);

	if (!NT_SUCCESS (status)) {
		goto Cleanup;
	}

	// Put the creation reference back again
	LpxReferenceDeviceContext ("Reload Succeeded", DeviceContext, DCREF_CREATION);

	DeviceContext->CreateRefRemoved = FALSE;

	status = NDIS_STATUS_SUCCESS;

Cleanup:

	LpxDereferenceDeviceContext ("Reload Temp Use", DeviceContext, DCREF_TEMP_USE);

	*NdisStatus = status;

	IF_LPXDBG (LPX_DEBUG_PNP) {
		LpxPrint2 ("LEAVE LpxReInitializeDeviceContext for %S with Status %08x\n",
						ExportName->Buffer,
						status);
	}

	return;
}

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            /*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop



NTSTATUS
MacReturnMaxDataSize(
    IN UINT DeviceMaxFrameSize,
    OUT PUINT MaxFrameSize
    )
{
    //
    // For 802.3, we always have a 14-byte MAC header.
    //
    if(DeviceMaxFrameSize <= 14) {
        *MaxFrameSize = 1300;
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: CAUTION!!!!!!! DeviceMaxFrameSize=%u. Setting to 1300\n", DeviceMaxFrameSize));
        //
		// To do: re-scan MTU again if scanned MTU value is invalid.
		//
    } else {
        *MaxFrameSize = DeviceMaxFrameSize - 14;
    }
    
#if 0
    //
    //	should be 1500 because header size( Ethernet 14 ) is subtracted from MaxFrameSize.
    //	To allow jumbo packet, cut off 1501~1514 size to block malicious NIC drivers that reports wrong size.
    //
    if(*MaxFrameSize > 1500 && *MaxFrameSize <= 1514) {

        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: NdisMedium802_3: MaxFrameSize will be adjusted: %u\n", *MaxFrameSize));
        *MaxFrameSize = 1500;
    }
#endif

    if (*MaxFrameSize > (UINT)LpxMaximumTransferUnit) { // default LpxMaximumTransferUnit is 1500
       *MaxFrameSize = LpxMaximumTransferUnit;
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: Setting Max frame size to configuration value %d\n", *MaxFrameSize));
    }

    if(*MaxFrameSize > 1500) { // Currently lpx does not support jumbo frame
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: NdisMedium802_3: MaxFrameSize will be adjusted: %u\n", *MaxFrameSize));    
        *MaxFrameSize = 1500;
    }

    *MaxFrameSize -= sizeof(LPX_HEADER2);
    *MaxFrameSize >>= 2;
    *MaxFrameSize <<= 2;

    return STATUS_SUCCESS;
}

                                                                                                                                            