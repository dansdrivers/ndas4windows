#include "precomp.h"
#pragma hdrstop
//
//	managing TP_CONNECTION instance
//
VOID
lpx_DereferenceConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    )
{
    KIRQL oldirql;

 
    DebugPrint(DebugLevel,("lpx_DereferenceConnectionSpecial: entered for connection %lx, "
                    "current level=%ld (%ld).\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount,
                    TransportConnection->SpecialRefCount) );
    

    ACQUIRE_SPIN_LOCK (TransportConnection->ProviderInterlock, &oldirql);	//connection + : 1

    --TransportConnection->SpecialRefCount;

    if ((TransportConnection->SpecialRefCount == 0) &&
        (TransportConnection->ReferenceCount == -1)) {

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);	//connection - : 0

        lpx_DestroyConnection (TransportConnection);

    } else {

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);	//connection - : 0

    }

} /* NbfDerefConnectionSpecial */


VOID
lpx_ReferenceConnection(
		PTP_CONNECTION Connection
		)
{
    if ((Connection->ReferenceCount == -1) &&  (Connection->SpecialRefCount == 0))     
        DbgBreakPoint();                         
    if (InterlockedIncrement(&Connection->ReferenceCount) == 0) { 
DebugPrint(4, ("lpx_ReferenceConnection : count %d\n", Connection->ReferenceCount));
        ExInterlockedAddUlong( 
			(PULONG)(&Connection->SpecialRefCount), 
			1, 
            Connection->ProviderInterlock); 
    }
}

VOID
lpx_DereferenceConnection(
		PTP_CONNECTION Connection
		)
{
    if ((Connection->ReferenceCount == -1) &&  (Connection->SpecialRefCount == 0))     
			DbgBreakPoint();                          
    lpx_DerefConnection (Connection);
}


VOID
lpx_DerefConnection(
    IN PTP_CONNECTION TransportConnection
    )
{
    LONG result;


    result = InterlockedDecrement (&TransportConnection->ReferenceCount);
DebugPrint(4, ("lpx_DerefConnection : count %d\n", TransportConnection->ReferenceCount));
    if (result < 0) {
	    
		//
        // Now it is OK to let the connection go away.
        //

        lpx_DereferenceConnectionSpecial (TransportConnection);

    }

} /* NbfDerefConnection */



VOID
lpx_AllocateConnection(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )
{

    PTP_CONNECTION Connection;

    Connection = (PTP_CONNECTION)ExAllocatePoolWithTag (
                                     NonPagedPool,
                                     sizeof (TP_CONNECTION),
                                     LPX_MEM_TAG_TP_CONNECTION);
    if (Connection == NULL) {
        PANIC("LPX: Could not allocate connection: no pool\n");
        *TransportConnection = NULL;
        return;
    }

    RtlZeroMemory (Connection, sizeof(TP_CONNECTION));

    ++DeviceContext->ConnectionAllocated;

    Connection->Type = LPX_CONNECTION_SIGNATURE;
    Connection->Size = sizeof (TP_CONNECTION);

    Connection->Provider = DeviceContext;
    Connection->ProviderInterlock = &DeviceContext->Interlock;
    KeInitializeSpinLock (&Connection->SpinLock);

    InitializeListHead (&Connection->Linkage);
	InitializeListHead (&Connection->LinkList);
    *TransportConnection = Connection;

}   /* __lpx_AllocateConnection */


VOID
lpx_DeallocateConnection(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    )
{

    ExFreePool (TransportConnection);
    --DeviceContext->ConnectionAllocated;

}   /* __lpx_DeallocateConnection */



NTSTATUS
lpx_CreateConnection(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )
{
    PTP_CONNECTION Connection;
    PSERVICE_POINT	ServicePoint;
    KIRQL oldirql;
    PLIST_ENTRY p;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);	//Device + : 1

    p = RemoveHeadList (&DeviceContext->ConnectionPool);
    if (p == &DeviceContext->ConnectionPool) {

        if ((DeviceContext->ConnectionMaxAllocated == 0) ||
            (DeviceContext->ConnectionAllocated < DeviceContext->ConnectionMaxAllocated)) {

            lpx_AllocateConnection (DeviceContext, &Connection);
  
        } else {
            Connection = NULL;
        }

        if (Connection == NULL) {
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);	//Device - : 0
            PANIC ("LPXCreateConnection: Could not allocate connection object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        Connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);

    }

    ++DeviceContext->ConnectionInUse;
    if (DeviceContext->ConnectionInUse > DeviceContext->ConnectionMaxInUse) {
        ++DeviceContext->ConnectionMaxInUse;
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);	//Device - : 0

	DebugPrint(2, ("LpxOpenConnection\n"));
	
    Connection->Type = LPX_CONNECTION_SIGNATURE;
    Connection->Size = sizeof (TP_CONNECTION);
    KeInitializeSpinLock (&Connection->SpinLock);
    InitializeListHead (&Connection->Linkage);
 	InitializeListHead (&Connection->LinkList);
    //
    // We have two references; one is for creation, and the
    // other is a temporary one so that the connection won't
    // go away before the creator has a chance to access it.
    //

    Connection->SpecialRefCount = 1;
    Connection->ReferenceCount = -1;
    //
    // Initialize the request queues & components of this connection.
    //
    Connection->Provider = DeviceContext;
    Connection->ProviderInterlock = &DeviceContext->Interlock;
    Connection->Flags = 0;
    Connection->Flags2 = 0;
 
    Connection->Context = NULL;                 // no context yet.
    
 
 
	// initialization ServicePoint

	ServicePoint = &Connection->ServicePoint;
	KeInitializeSpinLock(&ServicePoint->SpinLock);

	ServicePoint->SmpState	= SMP_CLOSE;
	ServicePoint->Shutdown	= 0;

	InitializeListHead(&ServicePoint->ServicePointListEntry);

	ServicePoint->DisconnectIrp = NULL;
	ServicePoint->ConnectIrp = NULL;
	ServicePoint->ListenIrp = NULL;
	ServicePoint->CloseIrp = NULL;
	
	InitializeListHead(&ServicePoint->ReceiveIrpList);
	KeInitializeSpinLock(&ServicePoint->ReceiveIrpQSpinLock);

	ServicePoint->Address = NULL;
	ServicePoint->Connection = Connection;
	ServicePoint->lDisconnectHandlerCalled = 0;

	SmpContextInit(ServicePoint, &ServicePoint->SmpContext);

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);		//Device + : 1

    Connection->ConnectionId = DeviceContext->UniqueIdentifier;
    ++DeviceContext->UniqueIdentifier;
    if (DeviceContext->UniqueIdentifier == 0x8000) {
        DeviceContext->UniqueIdentifier = 1;
    }

    LPX_REFERENCE_DEVICECONTEXT (DeviceContext);				//Device ref + : 1
    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);		// Device - : 0

    *TransportConnection = Connection;  // return the connection.

    return STATUS_SUCCESS;
} /* __lpx_CreateConnection */



NTSTATUS
lpx_OpenConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )
{
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    NTSTATUS status;
    PTP_CONNECTION connection;
    PFILE_FULL_EA_INFORMATION ea;
    KIRQL			oldIrql;

    UNREFERENCED_PARAMETER (Irp);

    DeviceContext = (PCONTROL_DEVICE_CONTEXT)DeviceObject;

DebugPrint(1,("ENTER lpx_OpenConnection\n"));  
    // Make sure we have a connection context specified in the EA info
    ea = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    if (ea->EaValueLength < sizeof(PVOID)) {
        return STATUS_INVALID_PARAMETER;
    }

    // Then, try to make a connection object to represent this pending
    // connection.  Then fill in the relevant fields.
    // In addition to the creation, if successful NbfCreateConnection
    // will create a second reference which is removed once the request
    // references the connection, or if the function exits before that.

    status = lpx_CreateConnection (DeviceContext, &connection);
    if (!NT_SUCCESS (status)) {
        return status;                          // sorry, we couldn't make one.
    }
    
	
	ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldIrql);	//Device + : 1
	InsertTailList (&DeviceContext->ConnectionDatabase, &connection->LinkList);
    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldIrql);	//Device - : 0


    //
    // set the connection context so we can connect the user to this data
    // structure
    //

    RtlCopyMemory (
        &connection->Context,
        &ea->EaName[ea->EaNameLength+1],
        sizeof (PVOID));

    //
    // let file object point at connection and connection at file object
    //

    IrpSp->FileObject->FsContext = (PVOID)connection;
    IrpSp->FileObject->FsContext2 = (PVOID)TDI_CONNECTION_FILE;
    connection->FileObject = IrpSp->FileObject;
DebugPrint(1,("LEAVE lpx_OpenConnection\n"));  
    return status;

} /* lpx_OpenConnection */



NTSTATUS
lpx_CloseConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
	)
{
    NTSTATUS status;
    KIRQL oldirql;
    PTP_CONNECTION connection;
    KIRQL			oldIrql;

    UNREFERENCED_PARAMETER (DeviceObject);
    UNREFERENCED_PARAMETER (Irp);

    //
    // is the file object a connection?
    //

    connection  = IrpSp->FileObject->FsContext;

	LpxCloseConnection(
		connection
		);

    

    //
    // We duplicate the code from VerifyConnectionObject,
    // although we don't actually call that since it does
    // a reference, which we don't want (to avoid bouncing
    // the reference count up from 0 if this is a dead
    // link).
    //

    try {

        if ((connection->Size == sizeof (TP_CONNECTION)) &&
            (connection->Type == LPX_CONNECTION_SIGNATURE)) {

            ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldIrql);	//connection + : 1

            if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                status = STATUS_SUCCESS;

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_SPIN_LOCK (&connection->SpinLock, oldIrql);	//connection  - : 0

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        return GetExceptionCode();
    }



    if (!NT_SUCCESS (status)) {
        return status;
    }

    //
    // We recognize it; is it closing already?
    //

    ACQUIRE_SPIN_LOCK (&connection->SpinLock, &oldIrql);	//connection  + : 1

    if ((connection->Flags2 & CONNECTION_FLAGS2_CLOSING) != 0) {
        RELEASE_SPIN_LOCK (&connection->SpinLock, oldIrql);	//connection - : 0
        return STATUS_INVALID_CONNECTION;
    }

    connection->Flags2 |= CONNECTION_FLAGS2_CLOSING;

    //
    // if there is activity on the connection, tear it down.
    //

    if ((connection->Flags2 & CONNECTION_FLAGS2_STOPPING) == 0) {
        lpx_StopConnection (connection, STATUS_LOCAL_DISCONNECT);
    }

    //
    // If the connection is still associated, disassociate it.
    //

    if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) &&
            ((connection->Flags2 & CONNECTION_FLAGS2_DISASSOCIATED) == 0)) {
        connection->Flags2 |= CONNECTION_FLAGS2_DISASSOCIATED;
    } 

    //
    // Save this to complete the IRP later.
    //

    connection->ServicePoint.CloseIrp = Irp;

	RELEASE_SPIN_LOCK (&connection->SpinLock, oldIrql);	// connection - : 0
 

    //
    // dereference for the creation. Note that this dereference
    // here won't have any effect until the regular reference count
    // hits zero.
    //

    lpx_DereferenceConnectionSpecial (connection);	//connection ref ?

    return STATUS_PENDING;

} /* __lpx_CloseConnection */



VOID
lpx_StopConnection(
    IN PTP_CONNECTION Connection,
    IN NTSTATUS Status
    )
{
    KIRQL cancelirql;
    PLIST_ENTRY p;
    PIRP Irp;
    BOOLEAN TimerWasCleared;
    ULONG DisconnectReason;
    PULONG StopCounter;
    PCONTROL_DEVICE_CONTEXT DeviceContext;

    DebugPrint(1, ("NbfStopConnection: Entered for connection %lx \n",Connection));



    DeviceContext = Connection->Provider;
	  

    if (!(Connection->Flags2 & CONNECTION_FLAGS2_STOPPING)) {

        Connection->Flags2 |= CONNECTION_FLAGS2_STOPPING;
  
    } 
} /* __lpx_StopConnection */



NTSTATUS
lpx_DestroyConnection(
    IN PTP_CONNECTION TransportConnection
    )
{
    KIRQL oldirql;
    PCONTROL_DEVICE_CONTEXT DeviceContext;
    PIRP CloseIrp;



    DebugPrint(DebugLevel, ("LpxDestroyConnection:  Entered for connection %lx.\n",
                    TransportConnection));
    

    DeviceContext = TransportConnection->Provider;

    //
    // Destroy any association that this connection has.
    //

    lpx_DestroyAssociation (TransportConnection);

    //
    // Clear out any associated nasties hanging around the connection. Note
    // that the current flags are set to STOPPING; this way anyone that may
    // maliciously try to use the connection after it's dead and gone will
    // just get ignored.
    //



    TransportConnection->Flags = 0;
    TransportConnection->Flags2 = CONNECTION_FLAGS2_CLOSING;

    //
    // Now complete the close IRP. This will be set to non-null
    // when CloseConnection was called.
    //

    CloseIrp = TransportConnection->ServicePoint.CloseIrp;
	
    if (CloseIrp != (PIRP)NULL) {


		KIRQL	cancelIrql;

		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(CloseIrp, NULL);
		IoReleaseCancelSpinLock(cancelIrql);
		
        TransportConnection->ServicePoint.CloseIrp = (PIRP)NULL;
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);

    } else {


        DebugPrint(1,("Connection %x destroyed, no CloseIrp!!\n", TransportConnection));


    }

    //
    // Return the connection to the provider's pool.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);		//device + : 1

    --DeviceContext->ConnectionInUse;

	RemoveEntryList (&TransportConnection->LinkList);
    InitializeListHead(&TransportConnection->LinkList);

	if ((DeviceContext->ConnectionAllocated - DeviceContext->ConnectionInUse) >
            DeviceContext->ConnectionInitAllocated) {
        lpx_DeallocateConnection (DeviceContext, TransportConnection);	
   
        DebugPrint(1, ("NBF: Deallocated connection at %lx\n",TransportConnection));
    } else {

        InsertTailList (&DeviceContext->ConnectionPool, &TransportConnection->LinkList);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);		// Device - : 0
	
	LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext);				// Device ref - : -1

    return STATUS_SUCCESS;

} /* lpxDestroyConnection */






NTSTATUS
lpx_DestroyAssociation(
    IN PTP_CONNECTION TransportConnection
    )
{
    KIRQL oldirql, oldirql2;
    PTP_ADDRESS address;
    BOOLEAN NotAssociated = FALSE;


    DebugPrint(1, ("NbfDestroyAssociation:  Entered for connection %lx.\n",
                    TransportConnection));
    

    try {

        ACQUIRE_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql2);	//connection + : 1
        if ((TransportConnection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {

            RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);	//connection - : 0
            NotAssociated = TRUE;
        } else {
            TransportConnection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
            RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);	//connection - : 0
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DebugPrint(1, ("Lpx: Got exception 1 in LpxDestroyAssociation\n"));
        DbgBreakPoint();

        //RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
    }

    if (NotAssociated) {
        return STATUS_SUCCESS;
    }

    address = TransportConnection->ServicePoint.Address;

    //
    // Delink this connection from its associated address connection
    // database.  To do this we must spin lock on the address object as
    // well as on the connection,
    //

    ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);	//address + : 1

    try {

        ACQUIRE_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql2);	//connection + : 1
        RemoveEntryList (&TransportConnection->Linkage);
        InitializeListHead (&TransportConnection->Linkage); 
        //
        // remove the association between the address and the connection.
        //

        TransportConnection->ServicePoint.Address = NULL;

        RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);	//connection - : 0

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("NBF: Got exception 2 in NbfDestroyAssociation\n");
        DbgBreakPoint();

        //RELEASE_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);	//connection - : 0
    }

    RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);	//address - : 0

    //
    // and remove a reference to the address
    //

    lpx_DereferenceAddress (address);		// address ref - : -1

    return STATUS_SUCCESS;

} /* LpxDestroyAssociation */




NTSTATUS
lpx_VerifyConnectionObject (
    IN PTP_CONNECTION Connection
    )
{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;

    try {

        if ((Connection != (PTP_CONNECTION)NULL) &&
            (Connection->Size == sizeof (TP_CONNECTION)) &&
            (Connection->Type == LPX_CONNECTION_SIGNATURE)) {

            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);		//connection + : 1

            if ((Connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                lpx_ReferenceConnection (Connection);					// connection ref + : 1

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);			//connection - : 0

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         return GetExceptionCode();
    }

    return status;

}
