/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    nbfdrvr.c

Abstract:

    This module contains code which defines the NetBIOS Frames Protocol
    transport provider's device object.

Author:

    David Beaver (dbeaver) 2-July-1991

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"

#pragma hdrstop

//
// This is a list of all the device contexts that LPX owns,
// used while unloading.
//

LIST_ENTRY LpxDeviceList = {0,0};   // initialized for real at runtime.

//
// And a lock that protects the global list of LPX devices
//
FAST_MUTEX LpxDevicesLock;

//
// Global variables this is a copy of the path in the registry for
// configuration data.
//

UNICODE_STRING LpxRegistryPath;

//
// We need the driver object to create device context structures.
//

PDRIVER_OBJECT LpxDriverObject;

//
// A handle to be used in all provider notifications to TDI layer
//
HANDLE         LpxProviderHandle;

//
// Global Configuration block for the driver ( no lock required )
//
PCONFIG_DATA   LpxConfig = NULL;

#ifdef LPX_LOCKS                    // see spnlckdb.c

extern KSPIN_LOCK LpxGlobalLock;

#endif // def LPX_LOCKS

//
// The debugging longword, containing a bitmask as defined in LPXCONST.H.
// If a bit is set, then debugging is turned on for that component.
//

#if DBG

#if __LPX__
ULONGLONG LpxDebug = 0;
#endif
BOOLEAN LpxDisconnectDebug;

PVOID * LpxConnectionTable;
PVOID * LpxAddressFileTable;
PVOID * LpxAddressTable;


LIST_ENTRY LpxGlobalRequestList;
LIST_ENTRY LpxGlobalLinkList;
LIST_ENTRY LpxGlobalConnectionList;
KSPIN_LOCK LpxGlobalInterlock;
KSPIN_LOCK LpxGlobalHistoryLock;

extern ULONG	PacketTxDropRate;
extern ULONG	PacketRxDropRate;

PVOID
TtdiSend ();

PVOID
TtdiReceive ();

PVOID
TtdiServer ();

KEVENT TdiSendEvent;
KEVENT TdiReceiveEvent;
KEVENT TdiServerEvent;

#endif

//
// This prevents us from having a bss section
//

ULONG _setjmpexused = 0;

//
// Forward declaration of various routines used in this module.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
LpxUnload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
LpxFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
LpxDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
LpxDispatchInternal(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
LpxDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
LpxDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LpxDispatchPnPPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
LpxDeallocateResources(
    IN PDEVICE_CONTEXT DeviceContext
    );

#ifdef RASAUTODIAL
VOID
LpxAcdBind();

VOID
LpxAcdUnbind();
#endif // RASAUTODIAL

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

#if (__LPX__ && defined __LPX_MUTEX_SPIN_LOCK__)

#if DBG

#define Must_test_deadlock_with_undefine___LPX_MUTEX_SPIN_LOCK__()	

#else

VOID
Must_test_deadlock_with_undefine___LPX_MUTEX_SPIN_LOCK__(
	VOID
	);	

#endif

#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine performs initialization of the NetBIOS Frames Protocol
    transport driver.  It creates the device objects for the transport
    provider and performs other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

    RegistryPath - The name of LPX's node in the registry.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    UNICODE_STRING nameString;
    NTSTATUS status;

    ASSERT (sizeof (SHORT) == 2);

#ifdef MEMPRINT
    MemPrintInitialize ();
#endif

#ifdef LPX_LOCKS
    KeInitializeSpinLock( &LpxGlobalLock );
#endif

#if (__LPX__ && defined __LPX_MUTEX_SPIN_LOCK__)
	Must_test_deadlock_with_undefine___LPX_MUTEX_SPIN_LOCK__();
#endif

#if DBG
#if __LPX__

	DbgPrint( "LPX DriverEntry %s %s\n", __DATE__, __TIME__ );

	{
		KIRQL			oldIrql;
		KSPIN_LOCK		spinLock;

		KeInitializeSpinLock( &spinLock );
		DbgPrint( "spinLock = %d\n", spinLock );
		ACQUIRE_SPIN_LOCK( &spinLock, &oldIrql );
		DbgPrint( "spinLock = %d\n", spinLock );
		RELEASE_SPIN_LOCK( &spinLock, oldIrql );
		DbgPrint( "spinLock = %d\n", spinLock );
	}

	DbgPrint( "LPX DriverEntry %s %s\n", __DATE__, __TIME__ );

	ASSERT( sizeof(TDI_ADDRESS_LPX) == sizeof(LPX_ADDRESS) );

	DbgPrint( "KeQueryTimeIncrement: Tick interval: %d ms \n", (KeQueryTimeIncrement()/(HZ/1000)) );

#if 0
	{
		LONG i = 0x7FFFFFFF;
		UINT j = 0;

		InterlockedIncrement( &i );
		DbgPrint( "SHORT_SEQNUM test  %x %x\n", i, SHORT_SEQNUM(i) );

		InterlockedIncrement( &i );
		DbgPrint( "SHORT_SEQNUM test  %x %x\n", i, SHORT_SEQNUM(i) );  

		i = 0x8FFFFFFF;
		DbgPrint( "SHORT_SEQNUM test  %x %x\n", i, SHORT_SEQNUM(i) );

		j -= 100;

		DbgPrint( "uint test j = %x\n", j );
	}
#endif

	//DbgPrint(" SocketLpxPrimaryDeviceContext = %p\n", SocketLpxPrimaryDeviceContext );

	LpxDebug = 0;
	LpxDebug |= LPX_DEBUG_PNP;
	//LpxDebug |= LPX_DEBUG_ERROR;
	//LpxDebug |= LPX_DEBUG_CURRENT_IRQL;
	//LpxDebug |= LPX_DEBUG_TEARDOWN;
	//LpxDebug |= LPX_DEBUG_TEMP;
	//LpxDebug |= LPX_DEBUG_DISPATCH;

#endif
#endif

#if DBG
    InitializeListHead (&LpxGlobalRequestList);
    InitializeListHead (&LpxGlobalLinkList);
    InitializeListHead (&LpxGlobalConnectionList);
    KeInitializeSpinLock (&LpxGlobalInterlock);
    KeInitializeSpinLock (&LpxGlobalHistoryLock);
#endif

#if __LPX__
	SocketLpxPrimaryDeviceContext = NULL;
	SocketLpxDeviceContext = NULL;
#endif

    LpxRegistryPath = *RegistryPath;
    LpxRegistryPath.Buffer = ExAllocatePoolWithTag(PagedPool,
                                                   RegistryPath->MaximumLength,
                                                   LPX_MEM_TAG_REGISTRY_PATH);

    if (LpxRegistryPath.Buffer == NULL) {
        PANIC(" Failed to allocate Registry Path!\n");
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlCopyMemory(LpxRegistryPath.Buffer, RegistryPath->Buffer,
                                                RegistryPath->MaximumLength);
    LpxDriverObject = DriverObject;
    RtlInitUnicodeString( &nameString, LPX_NAME);


    //
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction [IRP_MJ_CREATE] = LpxDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = LpxDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] = LpxDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] = LpxDispatchInternal;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = LpxDispatch;

    DriverObject->MajorFunction [IRP_MJ_PNP_POWER] = LpxDispatch;

    DriverObject->DriverUnload = LpxUnload;

    //
    // Initialize the global list of devices.
    // & the lock guarding this global list
    //

    InitializeListHead (&LpxDeviceList);

    ExInitializeFastMutex (&LpxDevicesLock);

    TdiInitialize();

    status = LpxRegisterProtocol (&nameString);

    if (!NT_SUCCESS (status)) {

        //
        // No configuration info read at startup when using PNP
        //

        ExFreePool(LpxRegistryPath.Buffer);
        PANIC ("LpxInitialize: RegisterProtocol with NDIS failed!\n");

        LpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_REGISTER_FAILED,
            607,
            status,
            NULL,
            0,
            NULL);

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    RtlInitUnicodeString( &nameString, LPX_DEVICE_NAME);

    //
    // Register as a provider with TDI
    //
    status = TdiRegisterProvider(
                &nameString,
                &LpxProviderHandle);

    if (!NT_SUCCESS (status)) {

        //
        // Deregister with the NDIS layer as TDI registration failed
        //
        LpxDeregisterProtocol();

        ExFreePool(LpxRegistryPath.Buffer);
        PANIC ("LpxInitialize: RegisterProtocol with TDI failed!\n");

        LpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_REGISTER_FAILED,
            607,
            status,
            NULL,
            0,
            NULL);

        return STATUS_INSUFFICIENT_RESOURCES;

    }

#if __LPX__

    status = LpxConfigureTransport(&LpxRegistryPath, &LpxConfig);

    if (!NT_SUCCESS (status)) {
    
		PANIC (" Failed to initialize transport, Lpx binding failed.\n");

		TdiDeregisterProvider(LpxProviderHandle);

		LpxDeregisterProtocol();

        ExFreePool(LpxRegistryPath.Buffer);

        LpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_REGISTER_FAILED,
            607,
            status,
            NULL,
            0,
            NULL);

        return STATUS_INSUFFICIENT_RESOURCES;

    }

#if DBG

	LpxConnectionTable = (PVOID *)ExAllocatePoolWithTag( NonPagedPool,
														 sizeof(PVOID) *
														 (LpxConfig->InitConnections + 2 +
														  LpxConfig->InitAddressFiles + 2 +
														  LpxConfig->InitAddresses + 2),
														  LPX_MEM_TAG_CONNECTION_TABLE);

	ASSERT (LpxConnectionTable);

    LpxAddressFileTable = LpxConnectionTable + (LpxConfig->InitConnections + 2);
    LpxAddressTable = LpxAddressFileTable + 
                                    (LpxConfig->InitAddressFiles + 2);

#endif

	SocketLpxProtocolBindAdapter( NULL,
								  NULL,
								  NULL,
								  NULL,
								  NULL );
#endif

    return(status);

}

VOID
LpxUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine unloads the NetBIOS Frames Protocol transport driver.
    It unbinds from any NDIS drivers that are open and frees all resources
    associated with the transport. The I/O system will not call us until
    nobody above has LPX open.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None. When the function returns, the driver is unloaded.

--*/

{

    PDEVICE_CONTEXT DeviceContext;
    PLIST_ENTRY p;

    UNREFERENCED_PARAMETER (DriverObject);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint0 ("ENTER LpxUnload\n");
    }

/*

#ifdef RASAUTODIAL

    //
    // Unbind from the automatic connection driver.
    //

#if DBG
        DbgPrint("Calling LpxAcdUnbind()\n");
#endif

    LpxAcdUnbind();
#endif // RASAUTODIAL

*/

    //
    // Walk the list of device contexts.
    //

    ACQUIRE_DEVICES_LIST_LOCK();

    while (!IsListEmpty (&LpxDeviceList)) {

        // Remove an entry from list and reset its
        // links (as we might try to remove from
        // the list again - when ref goes to zero)
        p = RemoveHeadList (&LpxDeviceList);

        InitializeListHead(p);

        DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, Linkage);

        DeviceContext->State = DEVICECONTEXT_STATE_STOPPING;

        // Remove creation ref if it has not already been removed
        if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

            RELEASE_DEVICES_LIST_LOCK();

#if __LPX__
			InterlockedExchange( &DeviceContext->CreateRefRemoved, FALSE );
#endif

            // Remove creation reference
            LpxDereferenceDeviceContext ("Unload", DeviceContext, DCREF_CREATION);

            ACQUIRE_DEVICES_LIST_LOCK();
        }
    }

    RELEASE_DEVICES_LIST_LOCK();

#if __LPX__

	if (SocketLpxDeviceContext) {

		NDIS_STATUS		ndisStatus;

		ACQUIRE_DEVICES_LIST_LOCK();
		RemoveHeadList( &SocketLpxDeviceContext->Linkage );
		InitializeListHead( &SocketLpxDeviceContext->Linkage );
		RELEASE_DEVICES_LIST_LOCK();

		SocketLpxProtocolUnbindAdapter( &ndisStatus,
										(PNDIS_HANDLE)SocketLpxDeviceContext,
										NULL );
		SocketLpxDeviceContext = NULL;
	}

#endif

    //
    // Deregister from TDI layer as a network provider
    //
    TdiDeregisterProvider(LpxProviderHandle);

    //
    // Then remove ourselves as an NDIS protocol.
    //

    LpxDeregisterProtocol();

    //
    // Finally free any memory allocated for config info
    //
    if (LpxConfig != NULL) {

        // Free configuration block
        LpxFreeConfigurationInfo(LpxConfig);

#if DBG
        // Free debugging tables
        ExFreePool(LpxConnectionTable);
#endif
    }

    //
    // Free memory allocated in DriverEntry for reg path
    //
    
    ExFreePool(LpxRegistryPath.Buffer);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint0 ("LEAVE LpxUnload\n");
    }

    return;
}


VOID
LpxFreeResources (
    IN PDEVICE_CONTEXT DeviceContext
    )
/*++

Routine Description:

    This routine is called by LPX to clean up the data structures associated
    with a given DeviceContext. When this routine exits, the DeviceContext
    should be deleted as it no longer has any assocaited resources.

Arguments:

    DeviceContext - Pointer to the DeviceContext we wish to clean up.

Return Value:

    None.

--*/
{
    PLIST_ENTRY p;
	PTP_ADDRESS address;
    PTP_CONNECTION connection;
	PTP_ADDRESS_FILE addressFile;

	//
    // Clean up address pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressPool) ) {
        p = RemoveHeadList (&DeviceContext->AddressPool);
        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        LpxDeallocateAddress (DeviceContext, address);
    }

    //
    // Clean up address file pool.
    //

    while ( !IsListEmpty (&DeviceContext->AddressFilePool) ) {
        p = RemoveHeadList (&DeviceContext->AddressFilePool);
        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

        LpxDeallocateAddressFile (DeviceContext, addressFile);
    }

    //
    // Clean up connection pool.
    //

    while ( !IsListEmpty (&DeviceContext->ConnectionPool) ) {
        p  = RemoveHeadList (&DeviceContext->ConnectionPool);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

        LpxDeallocateConnection (DeviceContext, connection);
    }

#if __LPX__

	//
    // Clean up Packet In Progress List.
    //
	//
	//
	//	modified by hootch 09042003
    
	while (p = ExInterlockedRemoveHeadList(&DeviceContext->PacketInProgressList,
										   &DeviceContext->PacketInProgressQSpinLock)) {

		PNDIS_PACKET	Packet;
		PLPX_RESERVED	reserved;

		reserved = CONTAINING_RECORD( p, LPX_RESERVED, ListEntry );
		Packet = CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
		PacketFree( DeviceContext, Packet );
    }

	if (DeviceContext->LpxPacketPool) {

	    NdisFreePacketPool( DeviceContext->LpxPacketPool );
		DeviceContext->LpxPacketPool = NULL;
		NdisFreeBufferPool( DeviceContext->LpxBufferPool );
		DeviceContext->LpxBufferPool = NULL;
	}

#endif

    //
    // Cleanup list of ndis buffers
    //
    if (DeviceContext->NdisBufferPool != NULL) {
        NdisFreeBufferPool (DeviceContext->NdisBufferPool);
        DeviceContext->NdisBufferPool = NULL;
    }

    return;

}   /* LpxFreeResources */


NTSTATUS
LpxDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the LPX device driver.
    It accepts an I/O Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    BOOL DeviceControlIrp = FALSE;
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PDEVICE_CONTEXT DeviceContext;

    ENTER_LPX;

	//Irp->Tail.Overlay.DriverContext[0] = 0;
	//Irp->Tail.Overlay.DriverContext[1] = 0;

    //
    // Check to see if LPX has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    try {
        DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
            LEAVE_LPX;
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        }

        // Reference the device so that it does not go away from under us
        LpxReferenceDeviceContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);
        
    } except(EXCEPTION_EXECUTE_HANDLER) {
        LEAVE_LPX;
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


    switch (IrpSp->MajorFunction) {

        case IRP_MJ_DEVICE_CONTROL:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDispatch: IRP_MJ_DEVICE_CONTROL.\n");
            }

            DeviceControlIrp = TRUE;

            Status = LpxDeviceControl (DeviceObject, Irp, IrpSp);
            break;

    case IRP_MJ_PNP:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDispatch: IRP_MJ_PNP.\n");
            }

            Status = LpxDispatchPnPPower (DeviceObject, Irp, IrpSp);
            break;

        default:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDispatch: OTHER (DEFAULT).\n");
            }
            Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status == STATUS_PENDING) {
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: request PENDING from handler.\n");
        }
    } else {
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: request COMPLETED by handler.\n");
        }

        //
        // LpxDeviceControl would have completed this IRP already
        //

        if (!DeviceControlIrp)
        {
            LEAVE_LPX;
            IrpSp->Control &= ~SL_PENDING_RETURNED;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            ENTER_LPX;
        }
    }

    // Remove the temp use reference on device context added above
    LpxDereferenceDeviceContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);
    
    //
    // Return the immediate status code to the caller.
    //

    LEAVE_LPX;
    return Status;
} /* LpxDispatch */


NTSTATUS
LpxDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the LPX device driver.
    It accepts an I/O Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    PFILE_FULL_EA_INFORMATION openType;
    USHORT i;
    BOOLEAN found;
    PTP_ADDRESS_FILE AddressFile;
    PTP_CONNECTION Connection;

    ENTER_LPX;

	//Irp->Tail.Overlay.DriverContext[0] = 0;
	//Irp->Tail.Overlay.DriverContext[1] = 0;

    //
    // Check to see if LPX has been initialized; if not, don't allow any use.
    // Note that this only covers any user mode code use; kernel TDI clients
    // will fail on their creation of an endpoint.
    //

    try {
        DeviceContext = (PDEVICE_CONTEXT)DeviceObject;
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
            LEAVE_LPX;
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        }

        // Reference the device so that it does not go away from under us
        LpxReferenceDeviceContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);
        
    } except(EXCEPTION_EXECUTE_HANDLER) {
        LEAVE_LPX;
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


    switch (IrpSp->MajorFunction) {

    //
    // The Create function opens a transport object (either address or
    // connection).  Access checking is performed on the specified
    // address to ensure security of transport-layer addresses.
    //

    case IRP_MJ_CREATE:
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: IRP_MJ_CREATE.\n");
        }

        openType =
            (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        if (openType != NULL) {

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
                Status = LpxOpenAddress (DeviceObject, Irp, IrpSp);
                break;
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
                Status = LpxOpenConnection (DeviceObject, Irp, IrpSp);
                break;
            }

            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint2 ("LpxDispatchOpenClose: IRP_MJ_CREATE on invalid type, len: %3d, name: %s\n",
                            (USHORT)openType->EaNameLength, openType->EaName);
            }

        } else {

            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDispatchOpenClose: IRP_MJ_CREATE on control channel!\n");
            }

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

            IrpSp->FileObject->FsContext = (PVOID)(DeviceContext->ControlChannelIdentifier);
            ++DeviceContext->ControlChannelIdentifier;
            if (DeviceContext->ControlChannelIdentifier == 0) {
                DeviceContext->ControlChannelIdentifier = 1;
            }

            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

            IrpSp->FileObject->FsContext2 = UlongToPtr(LPX_FILE_TYPE_CONTROL);
            Status = STATUS_SUCCESS;
        }

        break;

    case IRP_MJ_CLOSE:

        //
        // The Close function closes a transport endpoint, terminates
        // all outstanding transport activity on the endpoint, and unbinds
        // the endpoint from its transport address, if any.  If this
        // is the last transport endpoint bound to the address, then
        // the address is removed from the provider.
        //

        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: IRP_MJ_CLOSE.\n");
        }

        switch (PtrToUlong(IrpSp->FileObject->FsContext2)) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PTP_ADDRESS_FILE)IrpSp->FileObject->FsContext;

            //
            // This creates a reference to AddressFile->Address
            // which is removed by LpxCloseAddress.
            //

            Status = LpxVerifyAddressObject(AddressFile);

            if (!NT_SUCCESS (Status)) {
                Status = STATUS_INVALID_HANDLE;
            } else {
                Status = LpxCloseAddress (DeviceObject, Irp, IrpSp);
            }

            break;

        case TDI_CONNECTION_FILE:

            //
            // This is a connection
            //

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;

            Status = LpxVerifyConnectionObject (Connection);
            if (NT_SUCCESS (Status)) {

                Status = LpxCloseConnection (DeviceObject, Irp, IrpSp);
                LpxDereferenceConnection ("Temporary Use",Connection, CREF_BY_ID);

            }

            break;

        case LPX_FILE_TYPE_CONTROL:

            //
            // this always succeeds
            //

            Status = STATUS_SUCCESS;
            break;

        default:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint1 ("LpxDispatch: IRP_MJ_CLOSE on unknown file type %p.\n",
                    IrpSp->FileObject->FsContext2);
            }

            Status = STATUS_INVALID_HANDLE;
        }

        break;

    case IRP_MJ_CLEANUP:

        //
        // Handle the two stage IRP for a file close operation. When the first
        // stage hits, run down all activity on the object of interest. This
        // do everything to it but remove the creation hold. Then, when the
        // CLOSE irp hits, actually close the object.
        //

        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: IRP_MJ_CLEANUP.\n");
        }

        switch (PtrToUlong(IrpSp->FileObject->FsContext2)) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PTP_ADDRESS_FILE)IrpSp->FileObject->FsContext;
            Status = LpxVerifyAddressObject(AddressFile);
            if (!NT_SUCCESS (Status)) {

                Status = STATUS_INVALID_HANDLE;

            } else {

                LpxStopAddressFile (AddressFile, AddressFile->Address);
                LpxDereferenceAddress ("IRP_MJ_CLEANUP", AddressFile->Address, AREF_VERIFY);
                Status = STATUS_SUCCESS;
            }

            break;

        case TDI_CONNECTION_FILE:
            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;
            Status = LpxVerifyConnectionObject (Connection);
            if (NT_SUCCESS (Status)) {
                KeRaiseIrql (DISPATCH_LEVEL, &oldirql);
                LpxStopConnection (Connection, STATUS_LOCAL_DISCONNECT);
                KeLowerIrql (oldirql);
                Status = STATUS_SUCCESS;
                LpxDereferenceConnection ("Temporary Use",Connection, CREF_BY_ID);
            }

            break;

        case LPX_FILE_TYPE_CONTROL:

            Status = STATUS_SUCCESS;
            break;

        default:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint1 ("LpxDispatch: IRP_MJ_CLEANUP on unknown file type %p.\n",
                    IrpSp->FileObject->FsContext2);
            }

            Status = STATUS_INVALID_HANDLE;
        }

        break;

    default:
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: OTHER (DEFAULT).\n");
        }

        Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status == STATUS_PENDING) {
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: request PENDING from handler.\n");
        }
    } else {
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatch: request COMPLETED by handler.\n");
        }

        LEAVE_LPX;
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        ENTER_LPX;
    }

    // Remove the temp use reference on device context added above
    LpxDereferenceDeviceContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);

    //
    // Return the immediate status code to the caller.
    //

    LEAVE_LPX;
    return Status;
} /* LpxDispatchOpenClose */


NTSTATUS
LpxDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine dispatches TDI request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various TDI request handlers so that they are
    not IRP-dependent.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    BOOL InternalIrp = FALSE;
    NTSTATUS Status;
    PDEVICE_CONTEXT DeviceContext = (PDEVICE_CONTEXT)DeviceObject;

    IF_LPXDBG (LPX_DEBUG_DISPATCH) {
        LpxPrint0 ("LpxDeviceControl: Entered.\n");
    }

    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly;this is a check which must be made within each routine.
    //

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

#if __LPX__

		case IOCTL_LPX_GET_VERSION: {

			LPXDRV_VER	version;
		    PVOID		outputBuffer;
			ULONG		outputBufferLength;

			outputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
	        outputBuffer = Irp->UserBuffer;

			DebugPrint( 3, ("LPX: IOCTLLPX_GET_VERSION, outputBufferLength= %d, outputBuffer = %p\n", 
							 outputBufferLength, outputBuffer) );

			version.VersionMajor = VER_FILEMAJORVERSION;
			version.VersionMinor = VER_FILEMINORVERSION;
			version.VersionBuild = VER_FILEBUILD;
			version.VersionPrivate = VER_FILEBUILD_QFE;

			Irp->IoStatus.Information = sizeof( LPXDRV_VER );
			Status = STATUS_SUCCESS;

            try {

				RtlCopyMemory( outputBuffer, &version, sizeof(LPXDRV_VER) );

            } except (EXCEPTION_EXECUTE_HANDLER) {

				Status = GetExceptionCode();
                Irp->IoStatus.Information = 0;
            }

			break;
		}

		case IOCTL_LPX_QUERY_ADDRESS_LIST: {

		    PVOID					outputBuffer;
			ULONG					outputBufferLength;
			SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;

			PLIST_ENTRY		listHead;
			PLIST_ENTRY		thisEntry;

			outputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
	        outputBuffer = Irp->UserBuffer;

            DebugPrint( 3, ("LPX: IOCTL_TCP_QUERY_INFORMATION_EX, outputBufferLength= %d, outputBuffer = %p\n", 
							 outputBufferLength, outputBuffer) );

			if (outputBufferLength < sizeof(SOCKETLPX_ADDRESS_LIST)) {

				Status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				break;
			}

			ACQUIRE_DEVICES_LIST_LOCK();

			RtlZeroMemory( &socketLpxAddressList,
						   sizeof(SOCKETLPX_ADDRESS_LIST) );

			socketLpxAddressList.iAddressCount = 0;

			if (IsListEmpty(&LpxDeviceList)) {

				RELEASE_DEVICES_LIST_LOCK();

				RtlCopyMemory( outputBuffer,
							   &socketLpxAddressList,
							   sizeof(SOCKETLPX_ADDRESS_LIST) );

				Irp->IoStatus.Information = sizeof( SOCKETLPX_ADDRESS_LIST );
				Status = STATUS_SUCCESS;

				break;
			}

			listHead = &LpxDeviceList;
			for (thisEntry = listHead->Flink;
				 thisEntry != listHead;
				 thisEntry = thisEntry->Flink) {

				PDEVICE_CONTEXT deviceContext;
    
				deviceContext = CONTAINING_RECORD( thisEntry, DEVICE_CONTEXT, Linkage );

				if (deviceContext != SocketLpxDeviceContext && 
					deviceContext->CreateRefRemoved == FALSE && 
					FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_START) && !FlagOn(deviceContext->LpxFlags, LPX_DEVICE_CONTEXT_STOP)) {

					socketLpxAddressList.SocketLpx[socketLpxAddressList.iAddressCount].sin_family = TDI_ADDRESS_TYPE_LPX;
					socketLpxAddressList.SocketLpx[socketLpxAddressList.iAddressCount].LpxAddress.Port = 0;
					
					RtlCopyMemory( &socketLpxAddressList.SocketLpx[socketLpxAddressList.iAddressCount].LpxAddress.Node,
								   deviceContext->LocalAddress.Address,
								   HARDWARE_ADDRESS_LENGTH );
						
					socketLpxAddressList.iAddressCount++;
					ASSERT( socketLpxAddressList.iAddressCount <= MAX_SOCKETLPX_INTERFACE );

					if (socketLpxAddressList.iAddressCount == MAX_SOCKETLPX_INTERFACE)
						break;
				}
			}

			RELEASE_DEVICES_LIST_LOCK();
	
			RtlCopyMemory( outputBuffer,
						   &socketLpxAddressList,
						   sizeof(SOCKETLPX_ADDRESS_LIST) );

			Irp->IoStatus.Information = sizeof( SOCKETLPX_ADDRESS_LIST );
			Status = STATUS_SUCCESS;

			break;
		}

		case IOCTL_LPX_GET_RX_DROP_RATE: {

			ULONG	outputBufferLength;
			PULONG	pulData;

			outputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
			pulData = (PULONG)Irp->AssociatedIrp.SystemBuffer;

			if (outputBufferLength < sizeof(ULONG)) {
			
				Status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
				
			*pulData = PacketRxDropRate;

			DebugPrint( 1, ("[LPX]IOCTL_LPX_GET_RX_DROP_RATE: %d\n", *pulData) );

			Irp->IoStatus.Information = sizeof(ULONG);
			Status = STATUS_SUCCESS;

			break;
		}

		case IOCTL_LPX_SET_RX_DROP_RATE: {

			ULONG	inputBufferLength;
			PULONG	pulData;

			inputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
			pulData = (PULONG)Irp->AssociatedIrp.SystemBuffer;

			if (inputBufferLength < sizeof(ULONG)) {
			
				Status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			DebugPrint( 1, ("[LPX]IOCTL_LPX_SET_RX_DROP_RATE: %d -> %d\n", PacketRxDropRate, *pulData) );

			PacketRxDropRate = *pulData;

			Irp->IoStatus.Information = 0;
			Status = STATUS_SUCCESS;
		
			break;
		}

		case IOCTL_LPX_GET_TX_DROP_RATE: {

			ULONG	outputBufferLength;
			PULONG	pulData;

			outputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
			pulData = (PULONG)Irp->AssociatedIrp.SystemBuffer;

			if (outputBufferLength < sizeof(ULONG)) {
			
				Status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
				
			*pulData = PacketTxDropRate;

			DebugPrint( 1, ("[LPX]IOCTL_LPX_GET_TX_DROP_RATE: %d\n", *pulData) );

			Irp->IoStatus.Information = sizeof(ULONG);
			Status = STATUS_SUCCESS;

			break;
		}

		case IOCTL_LPX_SET_TX_DROP_RATE: {

			ULONG	inputBufferLength;
			PULONG	pulData;

			inputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
			pulData = (PULONG)Irp->AssociatedIrp.SystemBuffer;

			if (inputBufferLength < sizeof(ULONG)) {
			
				Status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}

			DebugPrint( 1, ("[LPX]IOCTL_LPX_SET_TX_DROP_RATE: %d -> %d\n", PacketRxDropRate, *pulData) );

			PacketTxDropRate = *pulData;

			Irp->IoStatus.Information = 0;
			Status = STATUS_SUCCESS;
		
			break;
		}
	
#endif  /*  __LPX__ */

#if DBG
        case IOCTL_TDI_SEND_TEST:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDeviceControl: Internal IOCTL: start send side test\n");
            }

            (VOID) KeSetEvent( &TdiSendEvent, 0, FALSE );

            break;

        case IOCTL_TDI_RECEIVE_TEST:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDeviceControl: Internal IOCTL: start receive side test\n");
            }

            (VOID) KeSetEvent( &TdiReceiveEvent, 0, FALSE );

            break;

        case IOCTL_TDI_SERVER_TEST:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDeviceControl: Internal IOCTL: start receive side test\n");
            }

            (VOID) KeSetEvent( &TdiServerEvent, 0, FALSE );

            break;
#endif

        default:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDeviceControl: invalid request type.\n");
            }

            //
            // Convert the user call to the proper internal device call.
            //

            Status = TdiMapUserRequest (DeviceObject, Irp, IrpSp);

            if (Status == STATUS_SUCCESS) {

                //
                // If TdiMapUserRequest returns SUCCESS then the IRP
                // has been converted into an IRP_MJ_INTERNAL_DEVICE_CONTROL
                // IRP, so we dispatch it as usual. The IRP will be
                // completed by this call to LpxDispatchInternal, so we dont
                //

                InternalIrp = TRUE;

                Status = LpxDispatchInternal (DeviceObject, Irp);
            }
    }

    //
    // If this IRP got converted to an internal IRP,
    // it will be completed by LpxDispatchInternal.
    //

    if ((!InternalIrp) && (Status != STATUS_PENDING))
    {
        LEAVE_LPX;
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        ENTER_LPX;
    }

    return Status;
} /* LpxDeviceControl */

NTSTATUS
LpxDispatchPnPPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine dispatches PnP request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to current IRP stack frame.

Return Value:

    The function value is the status of the operation.

--*/

{
    PDEVICE_RELATIONS DeviceRelations = NULL;
    PTP_CONNECTION Connection;
    PVOID PnPContext;
    NTSTATUS Status;

#if __LPX__
	UNREFERENCED_PARAMETER( DeviceObject );
#endif

    IF_LPXDBG (LPX_DEBUG_DISPATCH) {
        LpxPrint0 ("LpxDispatchPnPPower: Entered.\n");
    }

    Status = STATUS_INVALID_DEVICE_REQUEST;

    switch (IrpSp->MinorFunction) {

    case IRP_MN_QUERY_DEVICE_RELATIONS:

      if (IrpSp->Parameters.QueryDeviceRelations.Type == TargetDeviceRelation){

        switch (PtrToUlong(IrpSp->FileObject->FsContext2))
        {
        case TDI_CONNECTION_FILE:

            // Get the connection object and verify
            Connection = IrpSp->FileObject->FsContext;

            //
            // This adds a connection reference of type BY_ID if successful.
            //

            Status = LpxVerifyConnectionObject (Connection);

            if (NT_SUCCESS (Status)) {

                //
                // Get the PDO associated with conn's device object
                //

                PnPContext = Connection->Provider->PnPContext;
                if (PnPContext) {

                    DeviceRelations = 
                        ExAllocatePoolWithTag(NonPagedPool,
                                              sizeof(DEVICE_RELATIONS),
                                              LPX_MEM_TAG_DEVICE_PDO);
                    if (DeviceRelations) {

                        //
                        // TargetDeviceRelation allows exactly 1 PDO. fill it.
                        //
                        DeviceRelations->Count = 1;
                        DeviceRelations->Objects[0] = PnPContext;
                        ObReferenceObject(PnPContext);

                    } else {
                        Status = STATUS_NO_MEMORY;
                    }
                } else {
                    Status = STATUS_INVALID_DEVICE_STATE;
                }
            
                LpxDereferenceConnection ("Temp Rel", Connection, CREF_BY_ID);
            }
            break;
            
        case TDI_TRANSPORT_ADDRESS_FILE:

            Status = STATUS_UNSUCCESSFUL;
            break;
        }
      }
    }

    //
    // Invoker of this irp will free the information buffer.
    //

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = (ULONG_PTR) DeviceRelations;

    IF_LPXDBG (LPX_DEBUG_DISPATCH) {
        LpxPrint1 ("LpxDispatchPnPPower: exiting, status: %lx\n",Status);
    }

    return Status;
} /* LpxDispatchPnPPower */


NTSTATUS
LpxDispatchInternal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine dispatches TDI request types to different handlers based
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
    PDEVICE_CONTEXT DeviceContext;
    PIO_STACK_LOCATION IrpSp;
#if DBG
    KIRQL IrqlOnEnter = KeGetCurrentIrql();
#endif

    ENTER_LPX;

	//Irp->Tail.Overlay.DriverContext[0] = 0;
	//Irp->Tail.Overlay.DriverContext[1] = 0;

    IF_LPXDBG (LPX_DEBUG_DISPATCH) {
        LpxPrint0 ("LpxInternalDeviceControl: Entered.\n");
    }

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;

    try {
        if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {
            LEAVE_LPX;
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INVALID_DEVICE_STATE;
        }
    
        // Reference the device so that it does not go away from under us
        LpxReferenceDeviceContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);
        
    } except(EXCEPTION_EXECUTE_HANDLER) {
        LEAVE_LPX;
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
            LpxPrint5 ("Got IrpSp %p %p %p %p %p\n", Temp++,  Temp++,
                Temp++, Temp++, Temp++);
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

			ASSERT( FALSE );
			Status = STATUS_NOT_IMPLEMENTED;
            break;

        case TDI_ASSOCIATE_ADDRESS:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDispatchInternal: TdiAccept request.\n");
            }

            Status = LpxTdiAssociateAddress (Irp);
            break;

        case TDI_DISASSOCIATE_ADDRESS:
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint0 ("LpxDispatchInternal: TdiDisassociateAddress request.\n");
            }

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

			ASSERT( FALSE );
            IF_LPXDBG (LPX_DEBUG_DISPATCH) {
                LpxPrint1 ("LpxDispatchInternal: invalid request type %lx\n",
                IrpSp->MinorFunction);
            }
            Status = STATUS_INVALID_DEVICE_REQUEST;
    }

    if (Status == STATUS_PENDING) {
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatchInternal: request PENDING from handler.\n");
        }
    } else {
        IF_LPXDBG (LPX_DEBUG_DISPATCH) {
            LpxPrint0 ("LpxDispatchInternal: request COMPLETED by handler.\n");
        }

        LEAVE_LPX;
        IrpSp->Control &= ~SL_PENDING_RETURNED;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        ENTER_LPX;
    }

    IF_LPXDBG (LPX_DEBUG_DISPATCH) {
        LpxPrint1 ("LpxDispatchInternal: exiting, status: %lx\n",Status);
    }

    // Remove the temp use reference on device context added above
    LpxDereferenceDeviceContext ("Temp Use Ref", DeviceContext, DCREF_TEMP_USE);

    //
    // Return the immediate status code to the caller.
    //

    LEAVE_LPX;
#if DBG
    ASSERT (KeGetCurrentIrql() == IrqlOnEnter);
#endif

    return Status;

} /* LpxDispatchInternal */


VOID
LpxWriteResourceErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN ULONG BytesNeeded,
    IN ULONG ResourceId
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    an out of resources condition. It will handle event codes
    RESOURCE_POOL, RESOURCE_LIMIT, and RESOURCE_SPECIFIC.

Arguments:

    DeviceContext - Pointer to the device context.

    ErrorCode - The transport event code.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet.

    BytesNeeded - If applicable, the number of bytes that could not
        be allocated.

    ResourceId - The resource ID of the allocated structure.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    PWSTR SecondString;
    ULONG SecondStringSize;
    PUCHAR StringLoc;
    WCHAR ResourceIdBuffer[3];
    WCHAR SizeBuffer[2];
    WCHAR SpecificMaxBuffer[11];
    ULONG SpecificMax;
    INT i;

#if __LPX__
	UNREFERENCED_PARAMETER( UniqueErrorValue );
#endif

    switch (ErrorCode) {

    case EVENT_TRANSPORT_RESOURCE_POOL:
        SecondString = NULL;
        SecondStringSize = 0;
        break;

    case EVENT_TRANSPORT_RESOURCE_LIMIT:
        SecondString = SizeBuffer;
        SecondStringSize = sizeof(SizeBuffer);

        switch (DeviceContext->MemoryLimit) {
            case 100000: SizeBuffer[0] = L'1'; break;
            case 250000: SizeBuffer[0] = L'2'; break;
            case 0: SizeBuffer[0] = L'3'; break;
            default: SizeBuffer[0] = L'0'; break;
        }
        SizeBuffer[1] = 0;
        break;

    case EVENT_TRANSPORT_RESOURCE_SPECIFIC:
        switch (ResourceId) {
            case ADDRESS_RESOURCE_ID: SpecificMax = DeviceContext->MaxAddresses; break;
            case ADDRESS_FILE_RESOURCE_ID: SpecificMax = DeviceContext->MaxAddressFiles; break;
            case CONNECTION_RESOURCE_ID: SpecificMax = DeviceContext->MaxConnections; break;
        }

        for (i=9; i>=0; i--) {
            SpecificMaxBuffer[i] = (WCHAR)((SpecificMax % 10) + L'0');
            SpecificMax /= 10;
            if (SpecificMax == 0) {
                break;
            }
        }
        SecondString = SpecificMaxBuffer + i;
        SecondStringSize = sizeof(SpecificMaxBuffer) - (i * sizeof(WCHAR));
        SpecificMaxBuffer[10] = 0;
        break;

    default:
        ASSERT (FALSE);
        SecondString = NULL;
        SecondStringSize = 0;
        break;
    }

    EntrySize = (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                        DeviceContext->DeviceNameLength +
                        sizeof(ResourceIdBuffer) +
                        SecondStringSize);

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        EntrySize
    );

    //
    // Convert the resource ID into a buffer.
    //

    ResourceIdBuffer[1] = (WCHAR)((ResourceId % 10) + L'0');
    ResourceId /= 10;
    ASSERT(ResourceId <= 9);
    ResourceIdBuffer[0] = (WCHAR)((ResourceId % 10) + L'0');
    ResourceIdBuffer[2] = 0;

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = sizeof(ULONG);
        errorLogEntry->NumberOfStrings = (SecondString == NULL) ? 2 : 3;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->DumpData[0] = BytesNeeded;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);
        StringLoc += DeviceContext->DeviceNameLength;

        RtlCopyMemory (StringLoc, ResourceIdBuffer, sizeof(ResourceIdBuffer));
        StringLoc += sizeof(ResourceIdBuffer);

        if (SecondString) {
            RtlCopyMemory (StringLoc, SecondString, SecondStringSize);
        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* LpxWriteResourceErrorLog */


VOID
LpxWriteGeneralErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PWSTR SecondString,
    IN ULONG DumpDataCount,
    IN ULONG DumpData[]
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    a general problem as indicated by the parameters. It handles
    event codes REGISTER_FAILED, BINDING_FAILED, ADAPTER_NOT_FOUND,
    TRANSFER_DATA, TOO_MANY_LINKS, and BAD_PROTOCOL. All these
    events have messages with one or two strings in them.

Arguments:

    DeviceContext - Pointer to the device context, or this may be
        a driver object instead.

    ErrorCode - The transport event code.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet.

    FinalStatus - Used as the FinalStatus in the error log packet.

    SecondString - If not NULL, the string to use as the %3
        value in the error log packet.

    DumpDataCount - The number of ULONGs of dump data.

    DumpData - Dump data for the packet.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    ULONG SecondStringSize;
    PUCHAR StringLoc;
    PWSTR DriverName;

    EntrySize = (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                       (DumpDataCount * sizeof(ULONG)));

    if (DeviceContext->Type == IO_TYPE_DEVICE) {
        EntrySize += (UCHAR)DeviceContext->DeviceNameLength;
    } else {
        DriverName = L"Lpx";
        EntrySize += 4 * sizeof(WCHAR);
    }

    if (SecondString) {
        SecondStringSize = (wcslen(SecondString)*sizeof(WCHAR)) + sizeof(UNICODE_NULL);
        EntrySize += (UCHAR)SecondStringSize;
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        EntrySize
    );

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = (USHORT)(DumpDataCount * sizeof(ULONG));
        errorLogEntry->NumberOfStrings = (SecondString == NULL) ? 1 : 2;
        errorLogEntry->StringOffset =
            (USHORT)(sizeof(IO_ERROR_LOG_PACKET) + ((DumpDataCount-1) * sizeof(ULONG)));
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;

        if (DumpDataCount) {
            RtlCopyMemory(errorLogEntry->DumpData, DumpData, DumpDataCount * sizeof(ULONG));
        }

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        if (DeviceContext->Type == IO_TYPE_DEVICE) {
            RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);
            StringLoc += DeviceContext->DeviceNameLength;
        } else {
            RtlCopyMemory (StringLoc, DriverName, 4 * sizeof(WCHAR));
            StringLoc += 4 * sizeof(WCHAR);
        }
        if (SecondString) {
            RtlCopyMemory (StringLoc, SecondString, SecondStringSize);
        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* LpxWriteGeneralErrorLog */


VOID
LpxWriteOidErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS FinalStatus,
    IN PWSTR AdapterString,
    IN ULONG OidValue
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    a problem querying or setting an OID on an adapter. It handles
    event codes SET_OID_FAILED and QUERY_OID_FAILED.

Arguments:

    DeviceContext - Pointer to the device context.

    ErrorCode - Used as the ErrorCode in the error log packet.

    FinalStatus - Used as the FinalStatus in the error log packet.

    AdapterString - The name of the adapter we were bound to.

    OidValue - The OID which could not be set or queried.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG EntrySize;
    PUCHAR StringLoc;
    WCHAR OidBuffer[9];
    INT i;
    UINT CurrentDigit;

#if __LPX__
	UNREFERENCED_PARAMETER( AdapterString );
#endif

    EntrySize = (sizeof(IO_ERROR_LOG_PACKET) -
                 sizeof(ULONG) +
                 DeviceContext->DeviceNameLength +
                 sizeof(OidBuffer));

    if (EntrySize > ERROR_LOG_LIMIT_SIZE) {
        return;
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        (PDEVICE_OBJECT)DeviceContext,
        (UCHAR) EntrySize
    );

    //
    // Convert the OID into a buffer.
    //

    for (i=7; i>=0; i--) {
        CurrentDigit = OidValue & 0xf;
        OidValue >>= 4;
        if (CurrentDigit >= 0xa) {
            OidBuffer[i] = (WCHAR)(CurrentDigit - 0xa + L'A');
        } else {
            OidBuffer[i] = (WCHAR)(CurrentDigit + L'0');
        }
    }
    OidBuffer[8] = 0;

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = 0;
        errorLogEntry->NumberOfStrings = 3;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET) - sizeof(ULONG);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, DeviceContext->DeviceName, DeviceContext->DeviceNameLength);
        StringLoc += DeviceContext->DeviceNameLength;

        RtlCopyMemory (StringLoc, OidBuffer, sizeof(OidBuffer));

        IoWriteErrorLogEntry(errorLogEntry);
    }

}   /* LpxWriteOidErrorLog */

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

    This routine creates and initializes one nbf device context.  In order to
    do this it must successfully open and bind to the adapter described by
    nbfconfig->names[adapterindex].

Arguments:

    NdisStatus   - The outputted status of the operations.

    DriverObject - the nbf driver object.

    LpxConfig    - the transport configuration information from the registry.

    SystemSpecific1 - SystemSpecific1 argument to ProtocolBindAdapter

    SystemSpecific2 - SystemSpecific2 argument to ProtocolBindAdapter

Return Value:

    The number of successful binds.

--*/

{
    ULONG i;
    PDEVICE_CONTEXT DeviceContext;
	PTP_CONNECTION Connection;
    PTP_ADDRESS_FILE AddressFile;
    PTP_ADDRESS Address;
    NTSTATUS status;
    UINT MaxUserData;
	BOOLEAN UniProcessor;
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceString;
    UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_NETBIOS];
    PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
    PTDI_ADDRESS_NETBIOS NetBIOSAddress =
                                    (PTDI_ADDRESS_NETBIOS)pAddress->Address;
    struct {
        TDI_PNP_CONTEXT tdiPnPContextHeader;
        PVOID           tdiPnPContextTrailer;
    } tdiPnPContext1, tdiPnPContext2;

    pAddress->AddressLength = TDI_ADDRESS_LENGTH_NETBIOS;
    pAddress->AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    NetBIOSAddress->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;

    //
    // Determine if we are on a uniprocessor.
    //

#if __LPX__
        UniProcessor = FALSE;
#endif

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
	        LpxWriteGeneralErrorLog(
    	        (PVOID)DriverObject,
        	    EVENT_TRANSPORT_BINDING_FAILED,
	            707,
    	        status,
        	    BindName->Buffer,
	            0,
    	        NULL);

            return(0);
		}
		
    	return(1);
	}

    DeviceContext->UniProcessor = UniProcessor;

    //
    // Initialize our counter that records memory usage.
    //

    DeviceContext->MemoryUsage = 0;
    DeviceContext->MemoryLimit = LpxConfig->MaxMemoryUsage;

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

        //
        // Log an error if we were failed to
        // open this adapter.
        //

        LpxWriteGeneralErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_BINDING_FAILED,
            601,
            status,
            BindName->Buffer,
            0,
            NULL);

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
        &DeviceContext->MacInfo,
        NULL,
        0,
        DeviceContext->MaxSendPacketSize,
        TRUE,
        &MaxUserData);

#if __LPX__

	DeviceContext->Information.Version = 0x0100;
    DeviceContext->Information.MaxSendSize = 0x1fffe;   // 128k - 2
    DeviceContext->Information.MaxConnectionUserData = 0;
    DeviceContext->Information.MaxDatagramSize =  LPX_MAX_DATAGRAM_SIZE;
    DeviceContext->Information.ServiceFlags = LPX_SERVICE_FLAGS;
    if (DeviceContext->MacInfo.MediumAsync) {
        DeviceContext->Information.ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
    }
    DeviceContext->Information.MinimumLookaheadData =  240 - sizeof(LPX_HEADER);
    DeviceContext->Information.MaximumLookaheadData =  1500 - sizeof(LPX_HEADER);
    DeviceContext->Information.NumberOfResources = LPX_TDI_RESOURCES;
    KeQuerySystemTime (&DeviceContext->Information.StartTime);

#endif


    //
    // Allocate various structures we will need.
    //

    ENTER_LPX;

    //
    // The TP_PACKET structure has a CHAR[1] field at the end
    // which we expand upon to include all the headers needed;
    // the size of the MAC header depends on what the adapter
    // told us about its max header size. TP_PACKETs are used
    // for connection-oriented frame as well as for
    // control frames, but since DLC_I_FRAME and DLC_S_FRAME
    // are the same size, the header is the same size.
    //

    IF_LPXDBG (LPX_DEBUG_RESOURCE) {
        LpxPrint0 ("LPXDRVR: allocating connections.\n");
    }
    for (i=0; i<LpxConfig->InitConnections; i++) {

        LpxAllocateConnection (DeviceContext, &Connection);

        if (Connection == NULL) {
            PANIC ("LpxInitialize:  insufficient memory to allocate connections.\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        InsertTailList (&DeviceContext->ConnectionPool, &Connection->LinkList);
#if DBG
        LpxConnectionTable[i+1] = (PVOID)Connection;
#endif
    }
#if DBG
    LpxConnectionTable[0] = UlongToPtr(LpxConfig->InitConnections);
    LpxConnectionTable[LpxConfig->InitConnections+1] = (PVOID)
                ((LPX_CONNECTION_SIGNATURE << 16) | sizeof (TP_CONNECTION));
#endif

    DeviceContext->ConnectionInitAllocated = LpxConfig->InitConnections;
    DeviceContext->ConnectionMaxAllocated = LpxConfig->MaxConnections;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d connections, %ld\n", LpxConfig->InitConnections, DeviceContext->MemoryUsage);
    }


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
#if DBG
        LpxAddressFileTable[i+1] = (PVOID)AddressFile;
#endif
    }
#if DBG
    LpxAddressFileTable[0] = UlongToPtr(LpxConfig->InitAddressFiles);
    LpxAddressFileTable[LpxConfig->InitAddressFiles + 1] = (PVOID)
                            ((LPX_ADDRESSFILE_SIGNATURE << 16) |
                                 sizeof (TP_ADDRESS_FILE));
#endif

    DeviceContext->AddressFileInitAllocated = LpxConfig->InitAddressFiles;
    DeviceContext->AddressFileMaxAllocated = LpxConfig->MaxAddressFiles;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d address files, %ld\n", LpxConfig->InitAddressFiles, DeviceContext->MemoryUsage);
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
#if DBG
        LpxAddressTable[i+1] = (PVOID)Address;
#endif
    }
#if DBG
    LpxAddressTable[0] = UlongToPtr(LpxConfig->InitAddresses);
    LpxAddressTable[LpxConfig->InitAddresses + 1] = (PVOID)
                        ((LPX_ADDRESS_SIGNATURE << 16) | sizeof (TP_ADDRESS));
#endif

    DeviceContext->AddressInitAllocated = LpxConfig->InitAddresses;
    DeviceContext->AddressMaxAllocated = LpxConfig->MaxAddresses;

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint2 ("%d addresses, %ld\n", LpxConfig->InitAddresses, DeviceContext->MemoryUsage);
    }

    // Store away the PDO for the underlying object
    DeviceContext->PnPContext = SystemSpecific2;

    DeviceContext->State = DEVICECONTEXT_STATE_OPEN;

    //
    // Start the link-level timers running.
    //

    LpxInitializeTimerSystem (DeviceContext);

    //
    // Now link the device into the global list.
    //

    ACQUIRE_DEVICES_LIST_LOCK();
    InsertTailList (&LpxDeviceList, &DeviceContext->Linkage);
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
        RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }

    RtlCopyMemory(NetBIOSAddress->NetbiosName,
                  DeviceContext->ReservedNetBIOSAddress, 16);

    tdiPnPContext1.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext1.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_IF_NAME;
    *(PVOID UNALIGNED *) &tdiPnPContext1.tdiPnPContextHeader.ContextData = &DeviceString;

    tdiPnPContext2.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext2.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_PDO;
    *(PVOID UNALIGNED *) &tdiPnPContext2.tdiPnPContextHeader.ContextData = SystemSpecific2;

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("TdiRegisterNetAddress on %S ", DeviceString.Buffer);
        LpxPrint6 ("for %02x%02x%02x%02x%02x%02x\n",
                            NetBIOSAddress->NetbiosName[10],
                            NetBIOSAddress->NetbiosName[11],
                            NetBIOSAddress->NetbiosName[12],
                            NetBIOSAddress->NetbiosName[13],
                            NetBIOSAddress->NetbiosName[14],
                            NetBIOSAddress->NetbiosName[15]);
    }

#if __LPX__
	{ 
		PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;

		RtlCopyMemory( &LpxAddress->Node,
					   DeviceContext->LocalAddress.Address,
					   HARDWARE_ADDRESS_LENGTH );
	} 
#endif

    status = TdiRegisterNetAddress(pAddress,
                                   &DeviceString,
                                   (TDI_PNP_CONTEXT *) &tdiPnPContext2,
                                   &DeviceContext->ReservedAddressHandle);

    if (!NT_SUCCESS (status)) {
        RemoveEntryList(&DeviceContext->Linkage);
        goto cleanup;
    }

    LpxReferenceDeviceContext ("Load Succeeded", DeviceContext, DCREF_CREATION);

    LEAVE_LPX;
    *NdisStatus = NDIS_STATUS_SUCCESS;

    return(1);

cleanup:

    LpxWriteResourceErrorLog(
        DeviceContext,
        EVENT_TRANSPORT_RESOURCE_POOL,
        501,
        DeviceContext->MemoryUsage,
        0);

    //
    // Cleanup whatever device context we were initializing
    // when we failed.
    //
    *NdisStatus = status;
    ASSERT(status != STATUS_SUCCESS);
    
    if (InterlockedExchange(&DeviceContext->CreateRefRemoved, TRUE) == FALSE) {

        // Stop all internal timers
        LpxStopTimerSystem(DeviceContext);

        // Remove creation reference
        LpxDereferenceDeviceContext ("Load failed", DeviceContext, DCREF_CREATION);
    }

    LEAVE_LPX;

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

    This routine re-initializes an existing nbf device context. In order to
    do this, we need to undo whatever is done in the Unbind handler exposed
    to NDIS - recreate the NDIS binding, and re-start the LPX timer system.

Arguments:

    NdisStatus   - The outputted status of the operations.

    DriverObject - the nbf driver object.

    LpxConfig    - the transport configuration information from the registry.

    SystemSpecific1 - SystemSpecific1 argument to ProtocolBindAdapter

    SystemSpecific2 - SystemSpecific2 argument to ProtocolBindAdapter

Return Value:

    None

--*/

{
    PDEVICE_CONTEXT DeviceContext;
	PLIST_ENTRY p;
    NTSTATUS status;
    UNICODE_STRING DeviceString;
    UCHAR PermAddr[sizeof(TA_ADDRESS)+TDI_ADDRESS_LENGTH_NETBIOS];
    PTA_ADDRESS pAddress = (PTA_ADDRESS)PermAddr;
    PTDI_ADDRESS_NETBIOS NetBIOSAddress =
                                    (PTDI_ADDRESS_NETBIOS)pAddress->Address;
    struct {
        TDI_PNP_CONTEXT tdiPnPContextHeader;
        PVOID           tdiPnPContextTrailer;
    } tdiPnPContext1, tdiPnPContext2;

#if __LPX__
	UNREFERENCED_PARAMETER( DriverObject );
	UNREFERENCED_PARAMETER( SystemSpecific1 );
#endif

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
        DeviceContext = CONTAINING_RECORD (p, DEVICE_CONTEXT, Linkage);

        RtlInitUnicodeString(&DeviceString, DeviceContext->DeviceName);

        if (NdisEqualString(&DeviceString, ExportName, TRUE)) {
        					
            // This has to be a rebind - otherwise something wrong

        	ASSERT(DeviceContext->CreateRefRemoved == TRUE);

            // Reference within lock so that it is not cleaned up

            LpxReferenceDeviceContext ("Reload Temp Use", DeviceContext, DCREF_TEMP_USE);

            break;
        }
	}

    RELEASE_DEVICES_LIST_LOCK();

	if (p == &LpxDeviceList)
	{
        IF_LPXDBG (LPX_DEBUG_PNP) {
            LpxPrint2 ("LEAVE LpxReInitializeDeviceContext for %S with Status %08x\n",
                            ExportName->Buffer,
                            STATUS_NOT_FOUND);
        }

        *NdisStatus = STATUS_NOT_FOUND;

	    return;
	}

	DeviceContext->LpxFlags = 0;

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
    // Restart the link-level timers on device
    //

    LpxInitializeTimerSystem (DeviceContext);

	//
	// Re-Indicate to TDI that new binding has arrived
	//

    status = TdiRegisterDeviceObject(&DeviceString,
                                     &DeviceContext->TdiDeviceHandle);

    if (!NT_SUCCESS (status)) {
        goto Cleanup;
	}


    pAddress->AddressLength = TDI_ADDRESS_LENGTH_NETBIOS;
    pAddress->AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    NetBIOSAddress->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;

    RtlCopyMemory(NetBIOSAddress->NetbiosName,
                  DeviceContext->ReservedNetBIOSAddress, 16);

    tdiPnPContext1.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext1.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_IF_NAME;
    *(PVOID UNALIGNED *) &tdiPnPContext1.tdiPnPContextHeader.ContextData = &DeviceString;

    tdiPnPContext2.tdiPnPContextHeader.ContextSize = sizeof(PVOID);
    tdiPnPContext2.tdiPnPContextHeader.ContextType = TDI_PNP_CONTEXT_TYPE_PDO;
    *(PVOID UNALIGNED *) &tdiPnPContext2.tdiPnPContextHeader.ContextData = SystemSpecific2;

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint1 ("TdiRegisterNetAddress on %S ", DeviceString.Buffer);
        LpxPrint6 ("for %02x%02x%02x%02x%02x%02x\n",
                            NetBIOSAddress->NetbiosName[10],
                            NetBIOSAddress->NetbiosName[11],
                            NetBIOSAddress->NetbiosName[12],
                            NetBIOSAddress->NetbiosName[13],
                            NetBIOSAddress->NetbiosName[14],
                            NetBIOSAddress->NetbiosName[15]);
    }

#if __LPX__
	{ 
		PTDI_ADDRESS_LPX LpxAddress = (PTDI_ADDRESS_LPX)pAddress->Address;

		RtlCopyMemory( &LpxAddress->Node,
					   DeviceContext->LocalAddress.Address,
					   HARDWARE_ADDRESS_LENGTH );
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

    if (status != NDIS_STATUS_SUCCESS)
    {
        // Stop all internal timers
        LpxStopTimerSystem (DeviceContext);
    }

    LpxDereferenceDeviceContext ("Reload Temp Use", DeviceContext, DCREF_TEMP_USE);

	*NdisStatus = status;

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("LEAVE LpxReInitializeDeviceContext for %S with Status %08x\n",
                        ExportName->Buffer,
                        status);
    }

	return;
}
