/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    connobj.c

Abstract:

    This module contains code which implements the TP_CONNECTION object.
    Routines are provided to create, destroy, reference, and dereference,
    transport connection objects.

    A word about connection reference counts:

    With TDI version 2, connections live on even after they have been stopped.
    This necessitated changing the way LPX handles connection reference counts,
    making the stopping of a connection only another way station in the life
    of a connection, rather than its demise. Reference counts now work like
    this:

    Connection State         Reference Count     Flags
   ------------------       -----------------   --------
    Opened, no activity             1              0
    Open, Associated                2            FLAGS2_ASSOCIATED
    Open, Assoc., Connected         3            FLAGS_READY
     Above + activity              >3            varies
    Open, Assoc., Stopping         >3            FLAGS_STOPPING
    Open, Assoc., Stopped           3            FLAGS_STOPPING
    Open, Disassoc. Complete        2            FLAGS_STOPPING
                                                 FLAGS2_ASSOCIATED == 0
    Closing                         1            FLAGS2_CLOSING
    Closed                          0            FLAGS2_CLOSING

    Note that keeping the stopping flag set when the connection has fully
    stopped avoids using the connection until it is connected again; the
    CLOSING flag serves the same purpose. This allows a connection to run
    down in its own time.


Author:

    David Beaver (dbeaver) 1 July 1991

Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef RASAUTODIAL
#include <acd.h>
#include <acdapi.h>
#endif // RASAUTODIAL

#ifdef RASAUTODIAL
extern BOOLEAN fAcdLoadedG;
extern ACD_DRIVER AcdDriverG;

//
// Imported routines
//
BOOLEAN
LpxAttemptAutoDial(
    IN PTP_CONNECTION         Connection,
    IN ULONG                  ulFlags,
    IN ACD_CONNECT_CALLBACK   pProc,
    IN PVOID                  pArg
    );

VOID
LpxRetryTdiConnect(
    IN BOOLEAN fSuccess,
    IN PVOID *pArgs
    );

BOOLEAN
LpxCancelTdiConnect(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    );
#endif // RASAUTODIAL


VOID
LpxAllocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )

/*++

Routine Description:

    This routine allocates storage for a transport connection. Some
    minimal initialization is done.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - the device context for this connection to be
        associated with.

    TransportConnection - Pointer to a place where this routine will
        return a pointer to a transport connection structure. Returns
        NULL if the storage cannot be allocated.

Return Value:

    None.

--*/

{

    PTP_CONNECTION Connection;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_CONNECTION)) >
                DeviceContext->MemoryLimit)) {
        PANIC("LPX: Could not allocate connection: limit\n");
        LpxWriteResourceErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_RESOURCE_LIMIT,
            103,
            sizeof(TP_CONNECTION),
            CONNECTION_RESOURCE_ID);
        *TransportConnection = NULL;
        return;
    }

    Connection = (PTP_CONNECTION)ExAllocatePoolWithTag (
                                     NonPagedPool,
                                     sizeof (TP_CONNECTION),
                                     LPX_MEM_TAG_TP_CONNECTION);
    if (Connection == NULL) {
        PANIC("LPX: Could not allocate connection: no pool\n");
        LpxWriteResourceErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_RESOURCE_POOL,
            203,
            sizeof(TP_CONNECTION),
            CONNECTION_RESOURCE_ID);
        *TransportConnection = NULL;
        return;
    }
    RtlZeroMemory (Connection, sizeof(TP_CONNECTION));

    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint1 ("ExAllocatePool Connection %p\n", Connection);
    }

    DeviceContext->MemoryUsage += sizeof(TP_CONNECTION);
    ++DeviceContext->ConnectionAllocated;

    Connection->Type = LPX_CONNECTION_SIGNATURE;
    Connection->Size = sizeof (TP_CONNECTION);

    Connection->Provider = DeviceContext;
    Connection->ProviderInterlock = &DeviceContext->Interlock;
    KeInitializeSpinLock (&Connection->SpinLock);

    InitializeListHead (&Connection->LinkList);
    InitializeListHead (&Connection->AddressFileList);
    InitializeListHead (&Connection->AddressList);

    *TransportConnection = Connection;

}   /* LpxAllocateConnection */


VOID
LpxDeallocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine frees storage for a transport connection.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - the device context for this connection to be
        associated with.

    TransportConnection - Pointer to a transport connection structure.

Return Value:

    None.

--*/

{
    IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
        LpxPrint1 ("ExFreePool Connection: %p\n", TransportConnection);
    }

    ExFreePool (TransportConnection);
    --DeviceContext->ConnectionAllocated;
    DeviceContext->MemoryUsage -= sizeof(TP_CONNECTION);

}   /* LpxDeallocateConnection */


NTSTATUS
LpxCreateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    )

/*++

Routine Description:

    This routine creates a transport connection. The reference count in the
    connection is automatically set to 1, and the reference count in the
    DeviceContext is incremented.

Arguments:

    Address - the address for this connection to be associated with.

    TransportConnection - Pointer to a place where this routine will
        return a pointer to a transport connection structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_CONNECTION Connection;
    KIRQL oldirql;
    PLIST_ENTRY p;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint0 ("LpxCreateConnection:  Entered.\n");
    }

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    p = RemoveHeadList (&DeviceContext->ConnectionPool);
    if (p == &DeviceContext->ConnectionPool) {

        if ((DeviceContext->ConnectionMaxAllocated == 0) ||
            (DeviceContext->ConnectionAllocated < DeviceContext->ConnectionMaxAllocated)) {

            LpxAllocateConnection (DeviceContext, &Connection);
            IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
                LpxPrint1 ("LPX: Allocated connection at %p\n", Connection);
            }

        } else {

            LpxWriteResourceErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_RESOURCE_SPECIFIC,
                403,
                sizeof(TP_CONNECTION),
                CONNECTION_RESOURCE_ID);
            Connection = NULL;

        }

        if (Connection == NULL) {
            ++DeviceContext->ConnectionExhausted;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
            PANIC ("LpxCreateConnection: Could not allocate connection object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        Connection = CONTAINING_RECORD (p, TP_CONNECTION, LinkList);
#if DBG
        InitializeListHead (p);
#endif

    }

    ++DeviceContext->ConnectionInUse;
    if (DeviceContext->ConnectionInUse > DeviceContext->ConnectionMaxInUse) {
        ++DeviceContext->ConnectionMaxInUse;
    }

    DeviceContext->ConnectionTotal += DeviceContext->ConnectionInUse;
    ++DeviceContext->ConnectionSamples;

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    IF_LPXDBG (LPX_DEBUG_TEARDOWN) {
        LpxPrint1 ("LpxCreateConnection:  Connection at %p.\n", Connection);
    }

    //
    // We have two references; one is for creation, and the
    // other is a temporary one so that the connection won't
    // go away before the creator has a chance to access it.
    //

    Connection->SpecialRefCount = 1;
    Connection->ReferenceCount = -1;   // this is -1 based

#if DBG
    {
        UINT Counter;
        for (Counter = 0; Counter < NUMBER_OF_CREFS; Counter++) {
            Connection->RefTypes[Counter] = 0;
        }

        // This reference is removed by LpxCloseConnection

        Connection->RefTypes[CREF_SPECIAL_CREATION] = 1;
    }
#endif

    //
    // Initialize the request queues & components of this connection.
    //

    InitializeListHead (&Connection->AddressList);
    InitializeListHead (&Connection->AddressFileList);
    Connection->Flags2 = 0;
    Connection->Context = NULL;                 // no context yet.
    Connection->Status = STATUS_PENDING;        // default LpxStopConnection status.
    Connection->CloseIrp = (PIRP)NULL;
    Connection->AddressFile = NULL;

#if DBG
    Connection->Destroyed = FALSE;
    Connection->TotalReferences = 0;
    Connection->TotalDereferences = 0;
    Connection->NextRefLoc = 0;
    ExInterlockedInsertHeadList (&LpxGlobalConnectionList, &Connection->GlobalLinkage, &LpxGlobalInterlock);
    StoreConnectionHistory (Connection, TRUE);
#endif

#ifdef __LPX__

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	RtlZeroMemory( &Connection->LpxSmp, sizeof(LPX_SMP) );

	//Connection->LpxSmp.Connection = Connection;
	//Connection->LpxSmp.SpSpinLock = &Connection->SpinLock;

	LpxChangeState( Connection, SMP_STATE_CLOSE, TRUE ); /* No lock required */
	Connection->LpxSmp.Shutdown = 0;

	Connection->LpxSmp.DisconnectIrp = NULL;
	Connection->LpxSmp.ConnectIrp = NULL;
	Connection->LpxSmp.ListenIrp = NULL;
	
	InitializeListHead( &Connection->LpxSmp.ReceiveIrpQueue );
	KeInitializeSpinLock( &Connection->LpxSmp.ReceiveIrpQSpinLock );

	Connection->LpxSmp.lDisconnectHandlerCalled = 0;
	
	KeInitializeSpinLock( &Connection->LpxSmp.TimeCounterSpinLock );

	Connection->LpxSmp.Sequence				= 0;
	Connection->LpxSmp.RemoteSequence		= 0;
	Connection->LpxSmp.RemoteAckSequence	= 0;
	Connection->LpxSmp.ServerTag			= 0;

	Connection->LpxSmp.MaxFlights = SMP_MAX_FLIGHT / 2;

	KeInitializeTimer( &Connection->LpxSmp.SmpTimer );

	KeInitializeDpc( &Connection->LpxSmp.SmpTimerDpc, SmpTimerDpcRoutine, Connection );

	KeInitializeSpinLock( &Connection->LpxSmp.ReceiveQSpinLock );
	InitializeListHead( &Connection->LpxSmp.ReceiveQueue );
	KeInitializeSpinLock( &Connection->LpxSmp.RecvDataQSpinLock );
	InitializeListHead( &Connection->LpxSmp.RecvDataQueue );
	KeInitializeSpinLock( &Connection->LpxSmp.TransmitSpinLock );
	InitializeListHead( &Connection->LpxSmp.TramsmitQueue );

	KeInitializeSpinLock( &Connection->LpxSmp.ReceiveReorderQSpinLock );
	InitializeListHead( &Connection->LpxSmp.ReceiveReorderQueue );

	Connection->LpxSmp.SmpTimerInterval.QuadPart       = LPX_DEFAULT_SMP_TIMER_INTERVAL;
	
	Connection->LpxSmp.MaxStallInterval.QuadPart       = LPX_DEFAULT_MAX_STALL_INTERVAL;
	Connection->LpxSmp.MaxConnectWaitInterval.QuadPart = LPX_DEFALUT_MAX_CONNECT_WAIT_INTERVAL;
	
	Connection->LpxSmp.RetransmitInterval.QuadPart     = LPX_DEFAULT_RETRANSMIT_INTERVAL;
	
	Connection->LpxSmp.TimeWaitInterval.QuadPart       = LPX_DEFAULT_TIME_WAIT_INTERVAL;
	Connection->LpxSmp.AliveInterval.QuadPart	       = LPX_DEFAULT_ALIVE_INTERVAL;

	//SetFlag( Connection->LpxSmp.Option, LPX_OPTION_SOURCE_ADDRESS | LPX_OPTION_SOURCE_ADDRESS_ACCEPT );
	//SetFlag( Connection->LpxSmp.Option, LPX_OPTION_DESTINATION_ADDRESS | LPX_OPTION_DESTINATION_ADDRESS_ACCEPT );

	Connection->LpxSmp.StatisticsInterval.QuadPart     = LPX_DEFAULT_STATSTICS_INTERVAL;

#endif

    //
    // Now assign this connection an ID. This is used later to identify the
    // connection across multiple processes.
    //
    // The high bit of the ID is not user, it is off for connection
    // initiating NAME.QUERY frames and on for ones that are the result
    // of a FIND.NAME request.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    LpxReferenceDeviceContext ("Create Connection", DeviceContext, DCREF_CONNECTION);
    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    *TransportConnection = Connection;  // return the connection.

    return STATUS_SUCCESS;
} /* LpxCreateConnection */


NTSTATUS
LpxVerifyConnectionObject (
    IN PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called to verify that the pointer given us in a file
    object is in fact a valid connection object.

Arguments:

    Connection - potential pointer to a TP_CONNECTION object.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INVALID_CONNECTION otherwise

--*/

{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;

    //
    // try to verify the connection signature. If the signature is valid,
    // get the connection spinlock, check its state, and increment the
    // reference count if it's ok to use it. Note that being in the stopping
    // state is an OK place to be and reference the connection; we can
    // disassociate the address while running down.
    //

    try {

        if ((Connection != (PTP_CONNECTION)NULL) &&
            (Connection->Size == sizeof (TP_CONNECTION)) &&
            (Connection->Type == LPX_CONNECTION_SIGNATURE)) {

            ACQUIRE_C_SPIN_LOCK (&Connection->SpinLock, &oldirql);

            if ((Connection->Flags2 & CONNECTION_FLAGS2_CLOSING) == 0) {

                LpxReferenceConnection ("Verify Temp Use", Connection, CREF_BY_ID);

            } else {

                status = STATUS_INVALID_CONNECTION;
            }

            RELEASE_C_SPIN_LOCK (&Connection->SpinLock, oldirql);

        } else {

            status = STATUS_INVALID_CONNECTION;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         return GetExceptionCode();
    }

    return status;

}


NTSTATUS
LpxDestroyAssociation(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine destroys the association between a transport connection and
    the address it was formerly associated with. The only action taken is
    to disassociate the address and remove the connection from all address
    queues.

    This routine is only called by LpxDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql2;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
    BOOLEAN NotAssociated = FALSE;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint1 ("LpxDestroyAssociation:  Entered for connection %p.\n",
                    TransportConnection);
    }

    try {

        ACQUIRE_C_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql2);
        if ((TransportConnection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {

#if DBG
            if (!(IsListEmpty(&TransportConnection->AddressList)) ||
                !(IsListEmpty(&TransportConnection->AddressFileList))) {
                DbgPrint ("LPX: C %p, AF %p, freed while still queued\n",
                    TransportConnection, TransportConnection->AddressFile);
                DbgBreakPoint();
            }
#endif
            RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
            NotAssociated = TRUE;
        } else {
            TransportConnection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
            RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("LPX: Got exception 1 in LpxDestroyAssociation\n");
        DbgBreakPoint();

        RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
    }

    if (NotAssociated) {
        return STATUS_SUCCESS;
    }

    addressFile = TransportConnection->AddressFile;

    address = addressFile->Address;

    //
    // Delink this connection from its associated address connection
    // database.  To do this we must spin lock on the address object as
    // well as on the connection,
    //

    ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

    try {

        ACQUIRE_C_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql2);
        RemoveEntryList (&TransportConnection->AddressFileList);
        RemoveEntryList (&TransportConnection->AddressList);

        InitializeListHead (&TransportConnection->AddressList);
        InitializeListHead (&TransportConnection->AddressFileList);

        //
        // remove the association between the address and the connection.
        //

        TransportConnection->AddressFile = NULL;

        RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("LPX: Got exception 2 in LpxDestroyAssociation\n");
        DbgBreakPoint();

        RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql2);
    }

    RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

    //
    // and remove a reference to the address
    //

    LpxDereferenceAddress ("Destroy association", address, AREF_CONNECTION);


    return STATUS_SUCCESS;

} /* LpxDestroyAssociation */


NTSTATUS
LpxIndicateDisconnect(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine indicates a remote disconnection on this connection if it
    is necessary to do so. No other action is taken here.

    This routine is only called by LpxDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_ADDRESS_FILE addressFile;
    PDEVICE_CONTEXT DeviceContext;
    ULONG DisconnectReason;
    KIRQL oldirql;
    ULONG Lflags2;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint1 ("LpxIndicateDisconnect:  Entered for connection %p.\n",
                    TransportConnection);
    }

    try {

        ACQUIRE_C_SPIN_LOCK (&TransportConnection->SpinLock, &oldirql);

        if (((TransportConnection->Flags2 & CONNECTION_FLAGS2_REQ_COMPLETED) != 0)) {

            //
            // Turn off all but the still-relevant bits in the flags.
            //

            Lflags2 = TransportConnection->Flags2;
            TransportConnection->Flags2 &=
                (CONNECTION_FLAGS2_ASSOCIATED |
                 CONNECTION_FLAGS2_DISASSOCIATED |
                 CONNECTION_FLAGS2_CLOSING);
            TransportConnection->Flags2 |= CONNECTION_FLAGS2_STOPPING;

            //
            // Clean up other stuff -- basically everything gets
            // done here except for the flags and the status, since
            // they are used to block other requests. When the connection
            // is given back to us (in Accept, Connect, or Listen)
            // they are cleared.
            //

            if ((TransportConnection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) != 0) {
                addressFile = TransportConnection->AddressFile;
            } else {
                addressFile = NULL;
            }

            RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql);


            DeviceContext = TransportConnection->Provider;


            //
            // If this connection was stopped by a call to TdiDisconnect,
            // we have to complete that. We save the Irp so we can return
            // the connection to the pool before we complete the request.
            //


			if ((TransportConnection->Status != STATUS_LOCAL_DISCONNECT) &&
                    (addressFile != NULL) &&
                    (addressFile->RegisteredDisconnectHandler == TRUE)) {

                //
                // This was a remotely spawned disconnect, so indicate that
                // to our client. Note that in the comparison above we
                // check the status first, since if it is LOCAL_DISCONNECT
                // addressFile may be NULL (This is sort of a hack
                // for PDK2, we should really indicate the disconnect inside
                // LpxStopConnection, where we know addressFile is valid).
                //

                IF_LPXDBG (LPX_DEBUG_SETUP) {
                    LpxPrint1("IndicateDisconnect %p, indicate\n", TransportConnection);
                }

                //
                // if the disconnection was remotely spawned, then indicate
                // disconnect. In the case that a disconnect was issued at
                // the same time as the connection went down remotely, we
                // won't do this because DisconnectIrp will be non-NULL.
                //

                IF_LPXDBG (LPX_DEBUG_TEARDOWN) {
                    LpxPrint1 ("LpxIndicateDisconnect calling DisconnectHandler, refcnt=%ld\n",
                                TransportConnection->ReferenceCount);
                }

                //
                // Invoke the user's disconnection event handler, if any. We do this here
                // so that any outstanding sends will complete before we tear down the
                // connection.
                //

                DisconnectReason = 0;
                if (TransportConnection->Flags2 & CONNECTION_FLAGS2_ABORT) {
                    DisconnectReason |= TDI_DISCONNECT_ABORT;
                }
                if (TransportConnection->Flags2 & CONNECTION_FLAGS2_DESTROY) {
                    DisconnectReason |= TDI_DISCONNECT_RELEASE;
                }

                (*addressFile->DisconnectHandler)(
                        addressFile->DisconnectHandlerContext,
                        TransportConnection->Context,
                        0,
                        NULL,
                        0,
                        NULL,
                        TDI_DISCONNECT_ABORT);

            }

        } else {

            //
            // The client does not yet think that this connection
            // is up...generally this happens due to request count
            // fluctuation during connection setup.
            //

            RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql);

        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("LPX: Got exception in LpxIndicateDisconnect\n");
        DbgBreakPoint();

        RELEASE_C_SPIN_LOCK (&TransportConnection->SpinLock, oldirql);
    }


    return STATUS_SUCCESS;

} /* LpxIndicateDisconnect */


NTSTATUS
LpxDestroyConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine destroys a transport connection and removes all references
    made by it to other objects in the transport.  The connection structure
    is returned to our lookaside list.  It is assumed that the caller
    has removed all IRPs from the connections's queues first.

    This routine is only called by LpxDereferenceConnection.  The reason for
    this is that there may be multiple streams of execution which are
    simultaneously referencing the same connection object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    TransportConnection - Pointer to a transport connection structure to
        be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    PIRP CloseIrp;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint1 ("LpxDestroyConnection:  Entered for connection %p.\n",
                    TransportConnection);
    }

#if DBG
    if (TransportConnection->Destroyed) {
        LpxPrint1 ("attempt to destroy already-destroyed connection 0x%p\n", TransportConnection);
        DbgBreakPoint ();
    }
    if (!(TransportConnection->Flags2 & CONNECTION_FLAGS2_STOPPING)) {
        LpxPrint1 ("attempt to destroy unstopped connection 0x%p\n", TransportConnection);
        DbgBreakPoint ();
    }
    TransportConnection->Destroyed = TRUE;
    ACQUIRE_SPIN_LOCK (&LpxGlobalInterlock, &oldirql);
    RemoveEntryList (&TransportConnection->GlobalLinkage);
    RELEASE_SPIN_LOCK (&LpxGlobalInterlock, oldirql);
#endif

#ifdef __LPX__

	PrintStatistics( 3, TransportConnection );

	ASSERT( TransportConnection->LpxSmp.SmpState == SMP_STATE_CLOSE );

	ASSERT( TransportConnection->LockAcquired == 0);

	ASSERT( KeCancelTimer(&TransportConnection->LpxSmp.SmpTimer) == FALSE );
	ASSERT( KeRemoveQueueDpc(&TransportConnection->LpxSmp.SmpTimerDpc) == FALSE );
	
	ASSERT( IsListEmpty(&TransportConnection->LpxSmp.ReceiveReorderQueue) );
	ASSERT( IsListEmpty(&TransportConnection->LpxSmp.ReceiveQueue) );
	ASSERT( IsListEmpty(&TransportConnection->LpxSmp.TramsmitQueue) );
	ASSERT( IsListEmpty(&TransportConnection->LpxSmp.RecvDataQueue) );
	
	ASSERT( TransportConnection->LpxSmp.ConnectIrp == NULL );
	ASSERT( TransportConnection->LpxSmp.ListenIrp == NULL );
	ASSERT( TransportConnection->LpxSmp.DisconnectIrp == NULL );

	ASSERT( IsListEmpty(&TransportConnection->LpxSmp.ReceiveIrpQueue) );

	//if (FlagOn(TransportConnection->Flags2, CONNECTION_FLAGS2_REQ_COMPLETED))
	//	ASSERT( TransportConnection->LpxSmp.lDisconnectHandlerCalled != 0 );

#endif

    DeviceContext = TransportConnection->Provider;

    //
    // Destroy any association that this connection has.
    //

    LpxDestroyAssociation (TransportConnection);

    //
    // Clear out any associated nasties hanging around the connection. Note
    // that the current flags are set to STOPPING; this way anyone that may
    // maliciously try to use the connection after it's dead and gone will
    // just get ignored.
    //

    TransportConnection->Flags2 = CONNECTION_FLAGS2_CLOSING;

    //
    // Now complete the close IRP. This will be set to non-null
    // when CloseConnection was called.
    //

    CloseIrp = TransportConnection->CloseIrp;

    if (CloseIrp != (PIRP)NULL) {

        TransportConnection->CloseIrp = (PIRP)NULL;
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);

    } else {

#if DBG
        LpxPrint1("Connection %p destroyed, no CloseIrp!!\n", TransportConnection);
#endif

    }

    //
    // Return the connection to the provider's pool.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    DeviceContext->ConnectionTotal += DeviceContext->ConnectionInUse;
    ++DeviceContext->ConnectionSamples;
    --DeviceContext->ConnectionInUse;

    if ((DeviceContext->ConnectionAllocated - DeviceContext->ConnectionInUse) >
            DeviceContext->ConnectionInitAllocated) {
        LpxDeallocateConnection (DeviceContext, TransportConnection);
        IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
            LpxPrint1 ("LPX: Deallocated connection at %p\n", TransportConnection);
        }
    } else {
        InsertTailList (&DeviceContext->ConnectionPool, &TransportConnection->LinkList);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    LpxDereferenceDeviceContext ("Destroy Connection", DeviceContext, DCREF_CONNECTION);

    return STATUS_SUCCESS;

} /* LpxDestroyConnection */


#if DBG
VOID
LpxRefConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine increments the reference count on a transport connection.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint2 ("LpxReferenceConnection: entered for connection %p, "
                    "current level=%ld.\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount);
    }

#if DBG
    StoreConnectionHistory( TransportConnection, TRUE );
#endif

    result = InterlockedIncrement (&TransportConnection->ReferenceCount);

    if (result == 0) {

        //
        // The first increment causes us to increment the
        // "ref count is not zero" special ref.
        //

        ExInterlockedAddUlong(
            (PULONG)(&TransportConnection->SpecialRefCount),
            1,
            TransportConnection->ProviderInterlock);

#if DBG
        ++TransportConnection->RefTypes[CREF_SPECIAL_TEMP];
#endif

    }

    ASSERT (result >= 0);

} /* LpxRefConnection */
#endif


VOID
LpxDerefConnection(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routine dereferences a transport connection by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    LpxDestroyConnection to remove it from the system.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint2 ("LpxDereferenceConnection: entered for connection %p, "
                    "current level=%ld.\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount);
    }

#if DBG
    StoreConnectionHistory( TransportConnection, FALSE );
#endif

    result = InterlockedDecrement (&TransportConnection->ReferenceCount);

    //
    // If all the normal references to this connection are gone, then
    // we can remove the special reference that stood for
    // "the regular ref count is non-zero".
    //

    if (result < 0) {

        //
        // If the refcount is -1, then we need to disconnect from
        // the link and indicate disconnect. However, we need to
        // do this before we actually do the special deref, since
        // otherwise the connection might go away while we
        // are doing that.
        //
        // Note that both these routines are protected in that if they
        // are called twice, the second call will have no effect.
        //

        //
        // Now it is OK to let the connection go away.
        //

        LpxDereferenceConnectionSpecial ("Regular ref gone", TransportConnection, CREF_SPECIAL_TEMP);

    }

} /* LpxDerefConnection */


VOID
LpxDerefConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    )

/*++

Routine Description:

    This routines completes the dereferencing of a connection.
    It may be called any time, but it does not do its work until
    the regular reference count is also 0.

Arguments:

    TransportConnection - Pointer to a transport connection object.

Return Value:

    none.

--*/

{
    KIRQL oldirql;

    IF_LPXDBG (LPX_DEBUG_CONNOBJ) {
        LpxPrint3 ("LpxDereferenceConnectionSpecial: entered for connection %p, "
                    "current level=%ld (%ld).\n",
                    TransportConnection,
                    TransportConnection->ReferenceCount,
                    TransportConnection->SpecialRefCount);
    }

#if DBG
    StoreConnectionHistory( TransportConnection, FALSE );
#endif


    ACQUIRE_SPIN_LOCK (TransportConnection->ProviderInterlock, &oldirql);

    --TransportConnection->SpecialRefCount;

    if ((TransportConnection->SpecialRefCount == 0) &&
        (TransportConnection->ReferenceCount == -1)) {

        //
        // If we have deleted all references to this connection, then we can
        // destroy the object.  It is okay to have already released the spin
        // lock at this point because there is no possible way that another
        // stream of execution has access to the connection any longer.
        //

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);

        LpxDestroyConnection (TransportConnection);

    } else {

        RELEASE_SPIN_LOCK (TransportConnection->ProviderInterlock, oldirql);

    }

} /* LpxDerefConnectionSpecial */


VOID
LpxStopConnection(
    IN PTP_CONNECTION Connection,
    IN NTSTATUS Status
    )

/*++

Routine Description:

    This routine is called to terminate all activity on a connection and
    destroy the object.  This is done in a graceful manner; i.e., all
    outstanding requests are terminated by cancelling them, etc.  It is
    assumed that the caller has a reference to this connection object,
    but this routine will do the dereference for the one issued at creation
    time.

    Orderly release is a function of this routine, but it is not a provided
    service of this transport provider, so there is no code to do it here.

    NOTE: THIS ROUTINE MUST BE CALLED AT DPC LEVEL.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

    Status - The status that caused us to stop the connection. This
        will determine what status pending requests are aborted with,
        and also how we proceed during the stop (whether to send a
        session end, and whether to indicate disconnect).

Return Value:

    none.

--*/

{
    KIRQL cancelirql;
    PULONG StopCounter;
    PDEVICE_CONTEXT DeviceContext;

    IF_LPXDBG (LPX_DEBUG_TEARDOWN | LPX_DEBUG_PNP) {
        LpxPrint1 ("LpxStopConnection: Entered for connection %p\n",
                    Connection);
    }

    ASSERT (KeGetCurrentIrql() == DISPATCH_LEVEL);

    DeviceContext = Connection->Provider;

    ACQUIRE_DPC_C_SPIN_LOCK (&Connection->SpinLock);

    if (!(Connection->Flags2 & CONNECTION_FLAGS2_STOPPING)) {

        //
        // We are stopping the connection, record statistics
        // about it.
        //

        Connection->Flags2 |= CONNECTION_FLAGS2_STOPPING;
        Connection->Flags2 &= ~CONNECTION_FLAGS2_REMOTE_VALID;
        Connection->Status = Status;

        switch (Status) {

        case STATUS_LOCAL_DISCONNECT:
            StopCounter = &DeviceContext->Statistics.LocalDisconnects;
            break;
        case STATUS_REMOTE_DISCONNECT:
            StopCounter = &DeviceContext->Statistics.RemoteDisconnects;
            break;
        case STATUS_LINK_FAILED:
            StopCounter = &DeviceContext->Statistics.LinkFailures;
            break;
        case STATUS_IO_TIMEOUT:
            StopCounter = &DeviceContext->Statistics.SessionTimeouts;
            break;
        case STATUS_CANCELLED:
            StopCounter = &DeviceContext->Statistics.CancelledConnections;
            break;
        case STATUS_REMOTE_RESOURCES:
            StopCounter = &DeviceContext->Statistics.RemoteResourceFailures;
            break;
        case STATUS_INSUFFICIENT_RESOURCES:
            StopCounter = &DeviceContext->Statistics.LocalResourceFailures;
            break;
        case STATUS_BAD_NETWORK_PATH:
            StopCounter = &DeviceContext->Statistics.NotFoundFailures;
            break;
        case STATUS_REMOTE_NOT_LISTENING:
            StopCounter = &DeviceContext->Statistics.NoListenFailures;
            break;

        default:
            StopCounter = NULL;
            break;

        }

        if (StopCounter != NULL) {
            (*StopCounter)++;
        }

        RELEASE_DPC_C_SPIN_LOCK (&Connection->SpinLock);

#ifdef __LPX__	

		ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		if (Connection->LpxSmp.SmpState == SMP_STATE_CLOSE) { // Already closed
		
			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	
		} else {

			PIRP			irp;
			PLIST_ENTRY		thisEntry;
			PIRP		    pendingIrp;
			PDRIVER_CANCEL	oldCancelRoutine;

			PNDIS_PACKET	packet;
			UINT		    back_log = 0, receive_queue = 0, receivedata_queue = 0, write_queue = 0, retransmit_queue = 0;
			PLIST_ENTRY		listEntry;
			PLPX_RESERVED	reserved;
			BOOL			result;
		

			DebugPrint( 1, ("[LPX]LpxStopConnection: Connection->LpxSmp.SmpState = %s, Connection->Status = %x\n", 
							 LpxStateName[Connection->LpxSmp.SmpState], Connection->Status) );	

			LpxChangeState( Connection, SMP_STATE_CLOSE, TRUE ); 
			SetFlag( Connection->LpxSmp.Shutdown, SMP_SEND_SHUTDOWN );

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			IoAcquireCancelSpinLock( &cancelirql );
			ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			if (Connection->LpxSmp.ConnectIrp) {

				irp = Connection->LpxSmp.ConnectIrp;
				Connection->LpxSmp.ConnectIrp = NULL;

				RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

				oldCancelRoutine = IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelirql );

				irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );

				DebugPrint( 1, ("[LPX]LpxStopConnection: ConnectIrp %p canceled\n", irp) );	

				IoAcquireCancelSpinLock( &cancelirql );
				ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			}

			if (Connection->LpxSmp.ListenIrp) {

				irp = Connection->LpxSmp.ListenIrp;
				Connection->LpxSmp.ListenIrp = NULL;

				RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

				oldCancelRoutine = IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelirql );

				irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );

				DebugPrint( 1, ("[LPX]LpxStopConnection: ListenIrp %p canceled\n", irp) );	

				IoAcquireCancelSpinLock( &cancelirql );
				ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			}


			if (Connection->LpxSmp.DisconnectIrp) {

				irp = Connection->LpxSmp.DisconnectIrp;
				Connection->LpxSmp.DisconnectIrp = NULL;

				RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

				oldCancelRoutine = IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelirql );

				irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );

				DebugPrint( 1, ("[LPX]LpxStopConnection: DisconnectIrp %p canceled\n", irp) );	

				IoAcquireCancelSpinLock( &cancelirql );
				ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			}

			ClearFlag( Connection->LpxSmp.SmpTimerFlag, SMP_TIMER_RECEIVE_TIME_OUT );

			while (thisEntry = ExInterlockedRemoveHeadList( &Connection->LpxSmp.ReceiveIrpQueue, 
															&Connection->LpxSmp.ReceiveIrpQSpinLock)) {
	
				RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

				pendingIrp = CONTAINING_RECORD( thisEntry, IRP, Tail.Overlay.ListEntry );

				oldCancelRoutine = IoSetCancelRoutine( pendingIrp, NULL );
				IoReleaseCancelSpinLock( cancelirql );
		
				pendingIrp->IoStatus.Information = 0;
				pendingIrp->IoStatus.Status = STATUS_CANCELLED;
				LpxIoCompleteRequest( pendingIrp, IO_NO_INCREMENT );

				DebugPrint( 1, ("[LPX] SmpCancelIrps: Cancelled Receive IRP %p\n", pendingIrp) );

				IoAcquireCancelSpinLock( &cancelirql );
				ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			}

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			IoReleaseCancelSpinLock( cancelirql );

			ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
		
			result = KeCancelTimer( &Connection->LpxSmp.SmpTimer );

			if (result) { // Timer is in queue. If we canceled it, we need to dereference it.
			
				LpxDereferenceConnection( "Close", Connection, CREF_LPX_TIMER );
			}

			result = KeRemoveQueueDpc( &Connection->LpxSmp.SmpTimerDpc );

			if (result) { // Timer is in queue. If we canceled it, we need to dereference it.
		
				LpxDereferenceConnection( "Close", Connection, CREF_LPX_TIMER );
			}

			SmpPrintState( 2, "LpxStopConnection", Connection );

			DebugPrint(2, ("sequence = %x, fin_seq = %x, rmt_seq = %x, rmt_ack = %x\n", 
							SHORT_SEQNUM(Connection->LpxSmp.Sequence), SHORT_SEQNUM(Connection->LpxSmp.FinSequence),
							SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence),SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence)));
	
			while (listEntry = ExInterlockedRemoveHeadList(&Connection->LpxSmp.ReceiveReorderQueue, 
														   &Connection->LpxSmp.ReceiveReorderQSpinLock)) {
		
				DebugPrint( 1, ("LpxStopConnection: Removing packet from ReorderQueue\n") );
				reserved = CONTAINING_RECORD( listEntry, LPX_RESERVED, ListEntry );
				packet = CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
				PacketFree( Connection->AddressFile->Address->Provider, packet );
			}

			while (listEntry = ExInterlockedRemoveHeadList( &Connection->LpxSmp.ReceiveQueue, 
														    &Connection->LpxSmp.ReceiveQSpinLock)) {
		
				DebugPrint( 1, ("LpxStopConnection: Removing packet from ReceiveQueue\n") );
				reserved = CONTAINING_RECORD( listEntry, LPX_RESERVED, ListEntry );
				packet = CONTAINING_RECORD( reserved, NDIS_PACKET, ProtocolReserved );
				PacketFree( Connection->AddressFile->Address->Provider, packet );
			}

			while (packet = PacketDequeue(&Connection->LpxSmp.TramsmitQueue, &Connection->LpxSmp.TransmitSpinLock)) {

				PIO_STACK_LOCATION	irpSp;
				PIRP	            irp2;

				DebugPrint( 1, ("LpxStopConnection:TramsmitQueue packet deleted\n") );
		
				irpSp = RESERVED(packet)->IrpSp;

				if (irpSp != NULL) {
			
					irp2 = IRP_SEND_IRP(irpSp);
					irp2->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
					irp2->IoStatus.Information = 0;
				}

				retransmit_queue++;

				// Check Cloned Packet. 
				if (RESERVED(packet)->Cloned > 0) {

					DebugPrint( 1, ("[LPX]LpxCloseServicePoint: Cloned is NOT 0. %d\n", RESERVED(packet)->Cloned) );
				}

				RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
				PacketFree( Connection->AddressFile->Address->Provider, packet );
				ACQUIRE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
			}

			while (packet = PacketDequeue(&Connection->LpxSmp.RecvDataQueue, &Connection->LpxSmp.RecvDataQSpinLock)) {

				receive_queue++;
				PacketFree( Connection->AddressFile->Address->Provider, packet );
			}

			RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

			if (Connection->Status != STATUS_LOCAL_DISCONNECT) {

				(*Connection->AddressFile->DisconnectHandler)( Connection->AddressFile->DisconnectHandlerContext,
															   Connection->Context,
															   0,
															   NULL,
															   0,
															   NULL,
															   TDI_DISCONNECT_ABORT );

			}

			DebugPrint( 2, ("back_log = %d, receive_queue = %d, receivedata_queue = %d write_queue = %d, retransmit_queue = %d\n", 
							 back_log, receive_queue, receivedata_queue, write_queue, retransmit_queue) );

		}

#endif

        //
        // Run down all TdiConnect/TdiDisconnect/TdiListen requests.
        //

		IoAcquireCancelSpinLock(&cancelirql);
        ACQUIRE_DPC_C_SPIN_LOCK (&Connection->SpinLock);

        //
        // We are stopping early on.
        //

        RELEASE_DPC_C_SPIN_LOCK (&Connection->SpinLock);
        IoReleaseCancelSpinLock (cancelirql);

        return;

    } else {

        //
        // The connection was already stopping; it may have a
        // SESSION_END pending in which case we should kill
        // it.
        //

        if ((Status != STATUS_LOCAL_DISCONNECT) &&
            (Status != STATUS_CANCELLED)) {

        }

        RELEASE_DPC_C_SPIN_LOCK (&Connection->SpinLock);
    }
} /* LpxStopConnection */