#include "precomp.h"
#pragma hdrstop

#ifdef __LPX__

#if DBG
	
// Packet Drop Rate.
ULONG				ulPacketDropRate = 2;
ULONG				iTicks = 50;
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

void
CallUserDisconnectHandlerForDCDisable(
				  IN	PSERVICE_POINT	pServicePoint,
				  IN	ULONG			DisconnectFlags
				  )
{
	LONG called ;
	ULONG Reason;
	// Check parameter.
	if(pServicePoint == NULL) {
		return;
	}

	if(pServicePoint->Connection->AddressFile->DisconnectHandler == NULL) {
		return;
	}

	if(pServicePoint->SmpState != SMP_ESTABLISHED) {
		return;
	}

	called = InterlockedIncrement(&pServicePoint->lDisconnectHandlerCalled) ;
	if(called == 1) {
		Reason = TDI_DISCONNECT_DC_DISABLE; 
		// Perform Handler.
		(*pServicePoint->Connection->AddressFile->DisconnectHandler)(
			pServicePoint->Connection->AddressFile->DisconnectHandlerContext,
			pServicePoint->Connection->Context,
			0,
			NULL,
			0,//sizeof(ULONG),
			NULL,//&Reason,
			DisconnectFlags
			);
	} else {
		DebugPrint(1,("[LPX]CallUserDisconnectHandlerForDCDisable DisconnectHandler: Already Called\n"));
	}

	return;
}

/*
VOID 
LpxFreeDeviceContext(
					 IN PDEVICE_CONTEXT DeviceContext
					 )
{
	PTP_ADDRESS		address;
	PSERVICE_POINT	servicePoint = NULL;
	KIRQL			lpxOldIrql;
	KIRQL			addressOldIrql;
	PLIST_ENTRY			Flink = NULL;
	PLIST_ENTRY			listHead = NULL;
	PLIST_ENTRY			thisEntry = NULL;
	PLIST_ENTRY			nextEntry = NULL;
	
	ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &lpxOldIrql);

	for (Flink = DeviceContext->AddressDatabase.Flink;
		Flink != &DeviceContext->AddressDatabase;
		Flink = Flink->Flink, address = NULL) 
		{
			
			address = CONTAINING_RECORD (
										Flink,
										TP_ADDRESS,
										Linkage);
			
			if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
				continue;
			}

			ACQUIRE_SPIN_LOCK (&address->SpinLock, &addressOldIrql);
			
			listHead = &address->ConnectionServicePointList;

			thisEntry = listHead->Flink;

			while(thisEntry != listHead)
			{
				nextEntry = thisEntry->Flink;

				servicePoint = CONTAINING_RECORD(thisEntry, SERVICE_POINT, ServicePointListEntry);
				ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SpinLock);
				CallUserDisconnectHandlerForDCDisable(servicePoint,TDI_DISCONNECT_ABORT);
				RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock);
				thisEntry = nextEntry;
			}
			RELEASE_SPIN_LOCK(&address->SpinLock, addressOldIrql);
		}

		RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, lpxOldIrql);
}
*/

static VOID
LpxCancelIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

static BOOLEAN
SmpFreeServicePoint(
	IN	PSERVICE_POINT	ServicePoint
	);
//
//	IRP cancel routine
//
//	used only in LpxSend()
//
VOID
LpxCancelSend(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP Irp
			  )
{
//	KIRQL oldirql ;
//	KIRQL cancelIrql;
//    PIO_STACK_LOCATION IrpSp;
//    PTP_CONNECTION Connection;
//	PSERVICE_POINT ServicePoint;
//    PIRP SendIrp;
//    PLIST_ENTRY p;
//    BOOLEAN Found;
//	UINT	currentCount = 0;

    UNREFERENCED_PARAMETER (DeviceObject);

//
//	we cannot cancel any sending Irp.
//	Every IRPs in LPX is in progress because we don't keep Sending IRP queue in LPX.
//	See section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
//
//	hootch 02052004
//
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	DebugPrint(1, ("LpxCancelSend\n"));

/*
    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

    Connection = IrpSp->FileObject->FsContext;
	ServicePoint = (PSERVICE_POINT)Connection;
	
//
//	removed to keep the basic rule of cancel routine.
//	see section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
//
//	hootch 02052004
//	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
//	

	InterlockedIncrement (&IRP_SEND_REFCOUNT(IrpSp));

	CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);	
	SmpFreeServicePoint(ServicePoint);

//	NbfDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST) ;

	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 
	//
	// Cancel the Irp
	//
	if(InterlockedDecrement (&IRP_SEND_REFCOUNT(IrpSp)) == 0)
	{	
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = STATUS_CANCELLED;
		IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
	}
*/
}


//
//	IRP cancel routine
//
//	used only in LpxRecv()
//
VOID
LpxCancelRecv(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP			Irp
			  )
{
	KIRQL				oldirql;
	PIO_STACK_LOCATION	IrpSp;
	PTP_CONNECTION		pConnection;
	PSERVICE_POINT		pServicePoint;
//	KIRQL	cancelIrql;	

#if DBG
	DebugPrint(1, ("[LPX]LpxCancelRecv: Canceled Recv IRP %lx\n ", Irp));
#endif
	
    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //
    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

    pConnection = IrpSp->FileObject->FsContext;
	pServicePoint = &pConnection->ServicePoint;

#if DBG
	DbgPrint("[LPX] LpxCancelRecv: Cancelled receive IRP %lx on %lx\n",
		Irp, pConnection);
#endif

	//
    // Remove IRP from ReceiveIrpList.
    //
	ACQUIRE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, &oldirql);

	//
	//	Do not cancel a IRP in progress.
	//	See section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
	//
	//	patched by hootch
	//
	if(Irp->Tail.Overlay.ListEntry.Flink != Irp->Tail.Overlay.ListEntry.Blink) {

	RemoveEntryList(
		&Irp->Tail.Overlay.ListEntry
		);

		InitializeListHead(&Irp->Tail.Overlay.ListEntry) ;
	} else {
		//
		// IRP in progress. Do not cancel!
		//
		IoSetCancelRoutine(Irp, NULL);
		RELEASE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, oldirql);
		IoReleaseCancelSpinLock (Irp->CancelIrql);

		return ;
	}

	RELEASE_SPIN_LOCK (&pServicePoint->ReceiveIrpQSpinLock, oldirql);
		
	//
	// Unset Cancel Routine.
	//
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql);

	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	
	DebugPrint(1, ("[LPX]LpxCancelRecv: IRP %lx completed.\n ", Irp));

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	CallUserDisconnectHandler(pServicePoint, TDI_DISCONNECT_ABORT);	
	SmpFreeServicePoint(pServicePoint);

//	NbfDereferenceConnection("SmpTimerDpcRoutine", pServicePoint->Connection, CREF_REQUEST) ;
}
/*
VOID
LpxCancelRecv(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    
#if DBG
        DebugPrint(1, ("[LPX]LpxCancelRecv: Canceled Recv IRP %lx ", Irp));
#endif

		LpxCancelIrp(DeviceObject, Irp);
}
*/
//
//	IRP cancel routine
//
//	used only in LpxConnect()
//
VOID
LpxCancelConnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
	KIRQL oldIrql;
//	KIRQL cancelIrql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
//    PIRP SendIrp;
//    PLIST_ENTRY p;
//    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelConnect\n"));

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

    Connection = IrpSp->FileObject->FsContext;
	ServicePoint = (PSERVICE_POINT)Connection;
//	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	ASSERT(ServicePoint->Address) ;
	ACQUIRE_SPIN_LOCK (&ServicePoint->Address->SpinLock, &oldIrql);
	ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpinLock) ;
	ServicePoint->ConnectIrp = NULL;
	RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpinLock) ;
	RELEASE_SPIN_LOCK(&ServicePoint->Address->SpinLock, oldIrql) ;
	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);


//	CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);	
//	SmpFreeServicePoint(ServicePoint);
//	NbfDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST) ;

}

//
//	IRP cancel routine
//
//	used only in LpxDisconnect()
//
VOID
LpxCancelDisconnect(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
//	KIRQL oldirql, cancelIrql;
//    PIO_STACK_LOCATION IrpSp;
//    PTP_CONNECTION Connection;
//	PSERVICE_POINT ServicePoint;
//    PIRP SendIrp;
//    PLIST_ENTRY p;
//    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);
//
//	we cannot cancel any disconnecting Irp.
//	Every IRPs in LPX is in progress because we don't keep Sending IRP queue in LPX.
//	See section "Cancel Routines in Drivers without StartIo Routines" in DDK manual.
//
//	hootch 02052004
//
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

    DebugPrint(1, ("LpxCancelDisconnect\n"));
/*
    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

    Connection = IrpSp->FileObject->FsContext;
	ServicePoint = (PSERVICE_POINT)Connection;
	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	
	
	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 
	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	ServicePoint->DisconnectIrp = NULL;
	

	CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);	
	SmpFreeServicePoint(ServicePoint);

//	NbfDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST) ;
*/
}

VOID
LpxCancelListen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
	KIRQL oldIrql;
//	KIRQL cancelIrql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
	PSERVICE_POINT ServicePoint;
//    PIRP SendIrp;
//    PLIST_ENTRY p;
//    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelListen\n"));

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

    Connection = IrpSp->FileObject->FsContext;
	ServicePoint = (PSERVICE_POINT)Connection;
//	Irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;

	IoSetCancelRoutine(Irp, NULL);
	IoReleaseCancelSpinLock (Irp->CancelIrql); 

	ASSERT(ServicePoint->Address) ;
	ACQUIRE_SPIN_LOCK (&ServicePoint->Address->SpinLock, &oldIrql);
	ACQUIRE_DPC_SPIN_LOCK(&ServicePoint->SpinLock) ;
	ServicePoint->ListenIrp = NULL;
	RELEASE_DPC_SPIN_LOCK(&ServicePoint->SpinLock) ;
	RELEASE_SPIN_LOCK(&ServicePoint->Address->SpinLock, oldIrql) ;

	//
	// Cancel the Irp
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

//	CallUserDisconnectHandler(ServicePoint, TDI_DISCONNECT_ABORT);	
//	SmpFreeServicePoint(ServicePoint);
	
//	NbfDereferenceConnection("SmpTimerDpcRoutine", ServicePoint->Connection, CREF_REQUEST) ;

}

static VOID
SmpContextInit(
	IN PSERVICE_POINT	ServicePoint,
	IN PSMP_CONTEXT		SmpContext
	);

static VOID
SmpTimerDpcRoutineRequest(
	IN	PKDPC	dpc,
	IN	PVOID	Context,
	IN	PVOID	junk1,
	IN	PVOID	junk2
	);

static VOID
SmpTimerDpcRoutine(
	IN	PKDPC	dpc,
	IN	PVOID	Context,
	IN	PVOID	junk1,
	IN	PVOID	junk2
	);


static NTSTATUS
TransmitPacket(
   IN	PDEVICE_CONTEXT	DeviceContext,
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN PACKET_TYPE		PacketType,
	IN USHORT			UserDataLength
	);

static NTSTATUS
TransmitPacket_AvoidAddrSvcDeadLock(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN PACKET_TYPE		PacketType,
	IN USHORT			UserDataLength,
	IN PKSPIN_LOCK		ServicePointSpinLock,
	IN KIRQL			ServiceIrql
	) ;

static VOID
SmpDoReceiveRequest(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet
	);

static BOOLEAN
SmpDoReceive(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet
	);


static VOID
SmpWorkDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   );

static VOID
SmpPrintState(
	IN	LONG			DebugLevel,
	IN	PCHAR			Where,
	IN	PSERVICE_POINT	ServicePoint
	);


static NTSTATUS
SmpReadPacket(
	IN 	PIRP			Irp,
	IN 	PSERVICE_POINT	ServicePoint
	);

//
//	acquire SpinLock of DeviceContext before calling
//	comment by hootch 09042003
//
//	called only from NbfOpenAddress()
//
NTSTATUS
LpxAssignPort(
	IN PDEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	)
{
	BOOLEAN				notUsedPortFound;
	PLIST_ENTRY			listHead;
	PLIST_ENTRY			thisEntry;
	PTP_ADDRESS			address;
	USHORT				portNum;
	NTSTATUS			status;

	DebugPrint(1, ("Smp LPX_BIND %02x:%02x:%02x:%02x:%02x:%02x SourceAddress->Port = %x\n", 
		SourceAddress->Node[0],SourceAddress->Node[1],SourceAddress->Node[2],
		SourceAddress->Node[3],SourceAddress->Node[4],SourceAddress->Node[5],
		SourceAddress->Port));

	portNum = AddressDeviceContext->PortNum;
	listHead = &AddressDeviceContext->AddressDatabase;

	notUsedPortFound = FALSE;

	do {
		BOOLEAN	usedPort;

		portNum++;
		if(portNum == 0)
			portNum = LPX_PORTASSIGN_BEGIN;

		usedPort = FALSE;

		for(thisEntry = AddressDeviceContext->AddressDatabase.Flink;
			thisEntry != listHead;
			thisEntry = thisEntry->Flink) 
		{
	        address = CONTAINING_RECORD (thisEntry, TP_ADDRESS, Linkage);

	        if (address->NetworkName != NULL) {
                if (address->NetworkName->LpxAddress.Port == HTONS(portNum)) {
					usedPort = TRUE;
					break;
				}
			}
		}
		if(usedPort == FALSE)
			notUsedPortFound = TRUE;
		
		if(portNum == PNPMOUDLEPORT){
			usedPort = TRUE;
			notUsedPortFound = FALSE;
		}

	} while(notUsedPortFound == FALSE && portNum != AddressDeviceContext->PortNum);
	
	if(notUsedPortFound	== FALSE) {
		DebugPrint(2, ("[Lpx] LpxAssignPort: couldn't find available port number\n") );
		status = STATUS_UNSUCCESSFUL;
		goto ErrorOut;
	}
	SourceAddress->Port = HTONS(portNum) ;
	AddressDeviceContext->PortNum = portNum;
	DebugPrint(2, ("Smp LPX_BIND portNum = %x\n", portNum));

	status = STATUS_SUCCESS;

ErrorOut:
	return status;
}


//
//	acquire SpinLock of DeviceContext before calling
//	comment by hootch 09042003
//
//
//	called only from NbfOpenAddress()
//
PTP_ADDRESS
LpxLookupAddress(
    IN PDEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS		SourceAddress
    )

/*++

Routine Description:

    This routine scans the transport addresses defined for the given
    device context and compares them with the specified NETWORK
    NAME values.  If an exact match is found, then a pointer to the
    TP_ADDRESS object is returned, and as a side effect, the reference
    count to the address object is incremented.  If the address is not
    found, then NULL is returned.

    NOTE: This routine must be called with the DeviceContext
    spinlock held.

Arguments:

    DeviceContext - Pointer to the device object and its extension.
    NetworkName - Pointer to an NBF_NETBIOS_ADDRESS structure containing the
                    network name.

Return Value:

    Pointer to the TP_ADDRESS object found, or NULL if not found.

--*/

{
    PTP_ADDRESS address;
    PLIST_ENTRY p;
    ULONG i;


    p = DeviceContext->AddressDatabase.Flink;

    for (p = DeviceContext->AddressDatabase.Flink;
         p != &DeviceContext->AddressDatabase;
         p = p->Flink) {

        address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

        if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
			DebugPrint(3,("!!!! Address's STATUS is Alive %p\n", address) );
            continue;
        }

        //
        // If the network name is specified and the network names don't match,
        // then the addresses don't match.
        //

//        i = NETBIOS_NAME_LENGTH;        // length of a Netbios name
		i = sizeof(LPX_ADDRESS);

        if (address->NetworkName != NULL) {
            if (SourceAddress != NULL) {
                if (!RtlEqualMemory (
                        &address->NetworkName->LpxAddress,
                        SourceAddress,
                        i)) {
                    continue;
                }
            } else {
                continue;
            }

        } else {
            if (SourceAddress != NULL) {
                continue;
            }
        }

        //
        // We found the match.  Bump the reference count on the address, and
        // return a pointer to the address object for the caller to use.
        //

        IF_NBFDBG (NBF_DEBUG_ADDRESS) {
            NbfPrint2 ("NbfLookupAddress DC %lx: found %lx ", DeviceContext, address);
//          NbfDbgShowAddr (NetworkName);
        }

        NbfReferenceAddress ("lookup", address, AREF_LOOKUP);
        return address;

    } /* for */

    //
    // The specified address was not found.
    //

    IF_NBFDBG (NBF_DEBUG_ADDRESS) {
        NbfPrint1 ("NbfLookupAddress DC %lx: did not find ", address);
//      NbfDbgShowAddr (NetworkName);
    }

    return NULL;

} /* NbfLookupAddress */

//
//
//	called only from NbfCreateAddress()
//
VOID
LpxCreateAddress(
    IN PTP_ADDRESS Address
    )
{
	DebugPrint(2, ("LpxCreateAddress\n"));

//	InitializeListHead(&Address->ReceivePacketList);
//	InitializeListHead(&Address->ReceiveIrpList);
	InitializeListHead(&Address->ConnectionServicePointList);

	return;
}

//
//
// called only from NbfDestroyAddress()
//
VOID
LpxDestroyAddress(
    IN PTP_ADDRESS Address
    )
{
//	PNDIS_PACKET	packet;
//	PLIST_ENTRY		thisEntry;
//	PIRP			pendingIrp;
//	KIRQL			cancelIrql;

	UNREFERENCED_PARAMETER(Address) ;


	DebugPrint(2, ("LpxDestroyAddress\n"));

//	while(packet = PacketDequeue(
//					&Address->ReceivePacketList,
//					NULL
//					))
//	{
//		PacketFree(packet);
//	}

//	while(!IsListEmpty(&Address->ReceiveIrpList)) 
//	{
//		thisEntry = RemoveHeadList(
//						&Address->ReceiveIrpList
//						);
//
//		ASSERT(thisEntry != NULL);
//
//		pendingIrp = CONTAINING_RECORD(thisEntry, IRP, Tail.Overlay.ListEntry);
//
//        DebugPrint(1, ("[Lpx] LpxDestroyAddress: Cancelled IRP: 0%0x\n", pendingIrp));

//		IoAcquireCancelSpinLock(&cancelIrql);
//		IoSetCancelRoutine(pendingIrp, NULL);
//		IoReleaseCancelSpinLock(cancelIrql);

        //
        // Cancel the Irp
        //

//        pendingIrp->IoStatus.Information = 0;
//        pendingIrp->IoStatus.Status = STATUS_CANCELLED;
		
//        IoCompleteRequest(pendingIrp, IO_NETWORK_INCREMENT);
//    }

	return;
}

//
//
//
// called only from NbfTdiAssociateAddress()
//
VOID
LpxAssociateAddress(
    IN OUT	PTP_CONNECTION	Connection
    )
{
	PTP_ADDRESS		address;
	PSERVICE_POINT	servicePoint;
//	KIRQL			oldIrql ;

	DebugPrint(1, ("LpxAssociateAddress %p\n", Connection));

	address = Connection->AddressFile->Address;
	servicePoint = &Connection->ServicePoint;

    NbfReferenceAddress ("ServicePoint inserting", address, AREF_REQUEST);

	ExInterlockedInsertTailList(&address->ConnectionServicePointList,
						&servicePoint->ServicePointListEntry,
						&address->SpinLock
						);

	RtlCopyMemory(
				&servicePoint->SourceAddress,
				&address->NetworkName->LpxAddress,
				sizeof(LPX_ADDRESS)
				);

	servicePoint->Address = address;

	return;
}

//
//
//
// called only from NbfTdiDisassociateAddress() and LpxCloseConnection()
//
VOID
LpxDisassociateAddress(
    IN OUT	PTP_CONNECTION	Connection
    )
{
	PSERVICE_POINT	servicePoint;
//	KIRQL			oldIrql ;
	PTP_ADDRESS		address ;

	DebugPrint(2, ("LpxDisassociateAddress %p\n", Connection));

	servicePoint = &Connection->ServicePoint;
	address = servicePoint->Address ;
	if( address == NULL) return;

	//
	//	close Connection ( called Service Point in LPX )
	//
	if( SmpFreeServicePoint(servicePoint) == FALSE ) 
		return ;
	servicePoint->Address = NULL;
/*
	// added by hootch 08262003
	ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldIrql);

	RemoveEntryList(&servicePoint->ServicePointListEntry);

	RELEASE_SPIN_LOCK (&address->SpinLock, oldIrql);

	NbfDereferenceAddress ("ServicePoint deleting", address, AREF_REQUEST);
*/
	return;
}

//
//
//	called only from NbfTdiConnect()
//
NDIS_STATUS
LpxConnect(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   )
{   
	PSERVICE_POINT	servicePoint;
	PLPX_ADDRESS	destinationAddress;
	NDIS_STATUS     status;
//	KIRQL			cancelIrql;
	KIRQL			oldIrql ;
	LARGE_INTEGER	TimeInterval = {0,0};
	DebugPrint(2, ("LpxConnect\n"));

	servicePoint = (PSERVICE_POINT)Connection;

	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql) ;
	ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;

	if(servicePoint->SmpState != SMP_CLOSE) {
		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;
		goto ErrorOut;
	}
	
	//
	//	reference the connection
	//
	NbfReferenceConnection("LpxConnect", Connection, CREF_REQUEST) ;

	destinationAddress = &Connection->CalledAddress.LpxAddress;

	RtlCopyMemory(
		&servicePoint->DestinationAddress,
		destinationAddress,
		sizeof(LPX_ADDRESS)
		);
		
			DebugPrint(2,("servicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
					servicePoint->DestinationAddress.Node[0],
					servicePoint->DestinationAddress.Node[1],
					servicePoint->DestinationAddress.Node[2],
					servicePoint->DestinationAddress.Node[3],
					servicePoint->DestinationAddress.Node[4],
					servicePoint->DestinationAddress.Node[5],
					servicePoint->DestinationAddress.Port));

	IoMarkIrpPending(Irp);
	servicePoint->ConnectIrp = Irp;

	servicePoint->SmpState = SMP_SYN_SENT;

	status = TransmitPacket_AvoidAddrSvcDeadLock(servicePoint, NULL, CONREQ, 0, &servicePoint->SpinLock, DISPATCH_LEVEL);

	if(!NT_SUCCESS(status)) {
		DebugPrint(1, ("[LPX] LPX_CONNECT ERROR\n"));
		servicePoint->ConnectIrp = NULL;

		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

		NbfDereferenceConnection("LpxConnect", Connection, CREF_REQUEST) ;

		goto ErrorOut;
	}

//	IoAcquireCancelSpinLock(&cancelIrql);
    IoSetCancelRoutine(Irp, LpxCancelConnect);
//	IoReleaseCancelSpinLock(cancelIrql);

	//
	//	set connection time-out
	//

	ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;
	servicePoint->SmpContext.ConnectTimeOut.QuadPart = CurrentTime().QuadPart + MAX_CONNECT_TIME;
	servicePoint->SmpContext.SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
	RELEASE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;

	//TimeInterval.QuadPart = - SMP_TIMEOUT;
	TimeInterval.HighPart = -1;
	TimeInterval.LowPart = - SMP_TIMEOUT;
	KeSetTimer(
			&servicePoint->SmpContext.SmpTimer,
//			servicePoint->SmpContext.SmpTimerExpire,
			*(PTIME)&TimeInterval,
			&servicePoint->SmpContext.SmpTimerDpc
			);

	RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

	//
	//	dereference the connection
	//
	NbfDereferenceConnection("LpxConnect", Connection, CREF_REQUEST) ;

	return STATUS_PENDING; 

ErrorOut:

	Irp->IoStatus.Status = status;

//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
//	IoReleaseCancelSpinLock(cancelIrql);

	DebugPrint(1, ("[LPX]LpxConnect: IRP %lx completed with error: NTSTATUS:%08lx.\n ", Irp, status));

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return status;
}

//
//
//	called only from NbfTdiListen()
//
NDIS_STATUS
LpxListen(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   )
{   
	PSERVICE_POINT				servicePoint;
	NDIS_STATUS					status;
//	KIRQL						cancelIrql;
	KIRQL						oldIrql ;

	servicePoint = (PSERVICE_POINT)Connection;

	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql) ;
	ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;

	DebugPrint(1, ("LpxListen servicePoint = %p, servicePoint->SmpState = 0x%x\n", 
		servicePoint, servicePoint->SmpState));

	if(servicePoint->SmpState != SMP_CLOSE) {

		status = STATUS_UNSUCCESSFUL;

		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

		goto ErrorOut;
	}

	RtlCopyMemory(
				&servicePoint->SourceAddress,
				&servicePoint->Address->NetworkName->LpxAddress,
				sizeof(LPX_ADDRESS)
				);

	servicePoint->SmpState = SMP_LISTEN;
	servicePoint->ListenIrp = Irp;

	RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

	IoMarkIrpPending(Irp);

//	IoAcquireCancelSpinLock(&cancelIrql);
    IoSetCancelRoutine(Irp, LpxCancelListen);
//	IoReleaseCancelSpinLock(cancelIrql);

	return STATUS_PENDING; 

ErrorOut:

//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
//	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = status;

	DebugPrint(1, ("[LPX]LpxListen: IRP %lx completed with error: NTSTATUS:%08lx.\n ", Irp, status));

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return status;
}

//
//
//
//	called only from NbfTdiAccept()
//
NDIS_STATUS
LpxAccept(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   )
{   
	PSERVICE_POINT	servicePoint;
	NDIS_STATUS     status;
//	KIRQL			cancelIrql;
	KIRQL			oldIrql ;

	DebugPrint(1, ("LpxAccept\n"));

	servicePoint = (PSERVICE_POINT)Connection;

	DebugPrint(2, ("LpxAccept servicePoint = %p, servicePoint->SmpState = 0x%x\n", 
		servicePoint, servicePoint->SmpState));

	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql) ;
	ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;

	if(servicePoint->SmpState != SMP_ESTABLISHED) {

		status = STATUS_UNSUCCESSFUL;

		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

		goto ErrorOut;
	}

	RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

	status = STATUS_SUCCESS;

ErrorOut:

//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
//	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = status;

	DebugPrint(1, ("[LPX]LpxAccept: IRP %lx completed with error: NTSTATUS:%08lx.\n ", Irp, status));

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return status;
}

//
//
//	called only from NbfTdiDisconnect()
//
NDIS_STATUS
LpxDisconnect(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
    )
{

	PSERVICE_POINT	servicePoint;
	PSMP_CONTEXT	smpContext;
	NDIS_STATUS     status;
//	KIRQL			cancelIrql;
	KIRQL			oldIrql ;

	servicePoint = (PSERVICE_POINT)Connection;
	smpContext = &servicePoint->SmpContext;

	DebugPrint(1, ("LpxDisconnect servicePoint = %p, servicePoint->SmpState = 0x%x\n", 
		servicePoint, servicePoint->SmpState));

	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql) ;
	ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;

	switch(servicePoint->SmpState) {
	
	case SMP_CLOSE:
	case SMP_SYN_SENT:
		
		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

		SmpFreeServicePoint(servicePoint);
		break;

	case SMP_SYN_RECV:
	case SMP_ESTABLISHED:
	case SMP_CLOSE_WAIT:
	{
		IoMarkIrpPending(Irp);
//		IoAcquireCancelSpinLock(&cancelIrql);
	    IoSetCancelRoutine(Irp, LpxCancelDisconnect);
//		IoReleaseCancelSpinLock(cancelIrql);

		servicePoint->DisconnectIrp = Irp;

		if(servicePoint->SmpState == SMP_CLOSE_WAIT)
			servicePoint->SmpState = SMP_LAST_ACK;
		else
			servicePoint->SmpState = SMP_FIN_WAIT1;

		smpContext->FinSequence = SHORT_SEQNUM(smpContext->Sequence) ;

//		TransmitPacket_AvoidAddrSvcDeadLock(servicePoint, NULL, DISCON, 0, &servicePoint->SpinLock, DISPATCH_LEVEL);

		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

		ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;
		TransmitPacket_AvoidAddrSvcDeadLock(servicePoint, NULL, DISCON, 0, &servicePoint->SpinLock, DISPATCH_LEVEL);
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

		return STATUS_PENDING;
	 }

	 default:

		RELEASE_DPC_SPIN_LOCK(&servicePoint->SpinLock) ;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql) ;

		break;
	}


	status = STATUS_SUCCESS;

//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
//	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = status;

	DebugPrint(1, ("[LPX]LpxDisconnect: IRP %lx completed with error: NTSTATUS:%08lx.\n ", Irp, status));

	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return status;
} 

//
//
//
// called only from NbfOpenConnection()
//
VOID
LpxOpenConnection(
    IN OUT	PTP_CONNECTION	Connection
    )
{
	PSERVICE_POINT	ServicePoint;

	DebugPrint(2, ("LpxOpenConnection\n"));

	ServicePoint = &Connection->ServicePoint;
		
	RtlZeroMemory(
		ServicePoint,
		sizeof(SERVICE_POINT)
		);

	//
	//	init service point
	//
	KeInitializeSpinLock(&ServicePoint->SpinLock);

	ServicePoint->SmpState	= SMP_CLOSE;
	ServicePoint->Shutdown	= 0;

	InitializeListHead(&ServicePoint->ServicePointListEntry);
//	KeInitializeSpinLock(&ServicePoint->ServicePointQSpinLock);

	ServicePoint->DisconnectIrp = NULL;
	ServicePoint->ConnectIrp = NULL;
	ServicePoint->ListenIrp = NULL;
	
	InitializeListHead(&ServicePoint->ReceiveIrpList);
	KeInitializeSpinLock(&ServicePoint->ReceiveIrpQSpinLock);

	ServicePoint->Address = NULL;
	ServicePoint->Connection = Connection;
	// 052303 jgahn
	ServicePoint->lDisconnectHandlerCalled = 0;

	SmpContextInit(ServicePoint, &ServicePoint->SmpContext);

	return;
}

//
//
//	called only from NbfCloseConnection()
//
VOID
LpxCloseConnection(
    IN OUT	PTP_CONNECTION	Connection
    )
{
	PTP_ADDRESS		address;
	PSERVICE_POINT	servicePoint;

	DebugPrint(2, ("LpxCloseConnection\n"));

	servicePoint = &Connection->ServicePoint;
	address = servicePoint->Address;

	if(address != NULL) {
		LpxDisassociateAddress(
			Connection
		);
	}

	return;
}


NDIS_STATUS
LpxReceiveIndicate (
	IN NDIS_HANDLE	ProtocolBindingContext,
	IN NDIS_HANDLE	MacReceiveContext,
	IN PVOID		HeaderBuffer,
	IN UINT			HeaderBufferSize,
	IN PVOID		LookAheadBuffer,
	IN UINT			LookAheadBufferSize,
	IN UINT			PacketSize
	)
/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.
    This routine is time critical, so we only allocate a
    buffer and copy the packet into it. We also perform minimal
    validation on this packet. It gets queued to the device context
    to allow for processing later.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.

    ReceiveContext - A magic cookie for the MAC.

    HeaderBuffer - pointer to a buffer containing the packet header.

    HeaderBufferSize - the size of the header.

    LookaheadBuffer - pointer to a buffer containing the negotiated minimum
        amount of buffer I get to look at (not including header).

    LookaheadBufferSize - the size of the above. May be less than asked
        for, if that's all there is.

    PacketSize - Overall size of the packet (not including header).

Return Value:

    NDIS_STATUS - status of operation, one of:

                 NDIS_STATUS_SUCCESS if packet accepted,
                 NDIS_STATUS_NOT_RECOGNIZED if not recognized by protocol,
                 NDIS_any_other_thing if I understand, but can't handle.

--*/
{
    PDEVICE_CONTEXT		deviceContext;
	USHORT				protocol;
	PNDIS_PACKET        packet;
	PNDIS_BUFFER		firstBuffer;	
	PUCHAR				packetData;
	NDIS_STATUS         status;
	UINT				bytesTransfered = 0;
	UINT				startOffset = 0;

    DebugPrint(4, ("LpxReceiveIndicate, Entered\n"));
	
	// ILGU 2003_1103 support packet drop
	deviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;
	if(deviceContext->bDeviceInit == FALSE) {
		DebugPrint(1,(" Drop packet\n"));
		return NDIS_STATUS_NOT_RECOGNIZED;
	}

	//
	//	validation
	//
	if (HeaderBufferSize != ETHERNET_HEADER_LENGTH) {
		DebugPrint(4, ("HeaderBufferSize = %x\n", HeaderBufferSize));
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
	
	RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)HeaderBuffer)[12], sizeof(USHORT));

	//
	// if Ether Type less than 0x0600 ( 1536 )
	//
	// added by hootch 09242003
	//
	if(  NTOHS(protocol) < 0x0600 &&
		( protocol != HTONS(0x0060) && // LOOP: Ethernet Loopback
			protocol != HTONS(0x0200) && // PUP : Xerox PUP packet
				protocol != HTONS(0x0201)  // PUPAP: Xerox PUP address trans packet
				)) {
		RtlCopyMemory((PUCHAR)&protocol, &((PUCHAR)LookAheadBuffer)[LENGTH_8022LLCSNAP - 2], sizeof(USHORT));
		PacketSize -= LENGTH_8022LLCSNAP ;
		LookAheadBufferSize -= LENGTH_8022LLCSNAP ;
		startOffset = LENGTH_8022LLCSNAP ;
	}



    if(protocol != HTONS(ETH_P_LPX)) {
		DebugPrint(4, ("Type = %x\n", protocol));

		return NDIS_STATUS_NOT_RECOGNIZED;
	}

    DebugPrint(4, ("LpxReceiveIndicate, PacketSize = %d, LookAheadBufferSize = %d, LPX_HEADER2 size = %d\n",
		PacketSize, LookAheadBufferSize, sizeof(LPX_HEADER2)));



	status = PacketAllocate(
		NULL,
		PacketSize,
		deviceContext,
		RECEIVE_TYPE,
		NULL,
		0,
		NULL,
		&packet
		);

	if(status != NDIS_STATUS_SUCCESS) {
		return NDIS_STATUS_NOT_RECOGNIZED;
	}
		
	RtlCopyMemory(
			&RESERVED(packet)->EthernetHeader,
			HeaderBuffer,
			ETHERNET_HEADER_LENGTH
			);
	
	RESERVED(packet)->EthernetHeader.Type = protocol;

	if(PacketSize == LookAheadBufferSize) {
		NdisQueryPacket(
			packet,
			NULL,
			NULL,
			&firstBuffer,
			NULL
			);
	
   		packetData = MmGetMdlVirtualAddress(firstBuffer);
		RtlCopyMemory(
					packetData,
					(PBYTE)LookAheadBuffer + startOffset,
					LookAheadBufferSize
					);
		LpxTransferDataComplete(
                                deviceContext,
                                packet,
                                NDIS_STATUS_SUCCESS,
                                LookAheadBufferSize
								);
	} else {
		NdisTransferData(
			&status,
			deviceContext->NdisBindingHandle,
	        MacReceiveContext,
		    0,
			PacketSize,
	        packet,
		    &bytesTransfered
			);
	    if (status == NDIS_STATUS_PENDING) {
		    return NDIS_STATUS_SUCCESS;
		}

	    LpxTransferDataComplete(
                            deviceContext,
                            packet,
                            status,
                            bytesTransfered);

	}

    return NDIS_STATUS_SUCCESS;
}

//
//	queues a received packet to InProgressPacketList
//
//	called from NbfTransferDataComplete()
//		and LpxReceiveIndicate(), called from NbfReceiveIndication()
//
VOID
LpxTransferDataComplete(
						IN NDIS_HANDLE   ProtocolBindingContext,
						IN PNDIS_PACKET  Packet,
						IN NDIS_STATUS   Status,
						IN UINT          BytesTransfered
						)
{
	PDEVICE_CONTEXT     pDeviceContext;
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;	
	PUCHAR				bufferData;
	UINT				bufferLength;
	UINT				totalCopied;
	UINT				copied;
	UINT				bufferNumber;
	NDIS_STATUS			status;
	PNDIS_PACKET		pNewPacket;
	USHORT				usPacketSize;
	UINT				uiBufferSize;

	DebugPrint(3, ("[Lpx]LpxTransferDataComplete: Entered\n"));

	UNREFERENCED_PARAMETER(BytesTransfered) ;
	
	pDeviceContext = (PDEVICE_CONTEXT)ProtocolBindingContext;

	if(Status != NDIS_STATUS_SUCCESS){
		goto TossPacket;
	}
	// Mod by jgahn.
	// Original.
/*
	lpxHeader = ExAllocatePool(NonPagedPool, sizeof(LPX_HEADER2));
	if(lpxHeader == NULL) {
		DebugPrint(1, ("No memory\n"));
		goto TossPacket;
	}
*/
	// New.
	if(NdisAllocateMemory(&lpxHeader, sizeof(LPX_HEADER2)) != NDIS_STATUS_SUCCESS) {
		DebugPrint(1, ("No memory\n"));
		goto TossPacket;
	}
	// End Mod.

	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);

	// Mod by gahn.
	//Origial.
	/*
	bufferData = MmGetMdlVirtualAddress(firstBuffer);
	bufferLength = NdisBufferLength(firstBuffer);
	*/

	// New
	NdisQueryBufferSafe(
		firstBuffer,
		&bufferData,
		&bufferLength,
		HighPagePriority
		);
	// End Mod.

	DebugPrint(4, ("bufferLength = %d\n", bufferLength));
	//	if(RESERVED(Packet)->Type == DIRECT_RECEIVE_TYPE) {
	//		bufferLength -= ETHERNET_HEADER_LENGTH;
	//		bufferData += ETHERNET_HEADER_LENGTH;
	//	}
	totalCopied = 0;
	copied = bufferLength < sizeof(LPX_HEADER2) ? bufferLength : sizeof(LPX_HEADER2);
	bufferNumber = 0;
	
	while(firstBuffer) {
		PNDIS_BUFFER	nextBuffer;
		
		if(copied)
			RtlCopyMemory(
				&((PUCHAR)lpxHeader)[totalCopied],
				bufferData,
				copied
				);
		totalCopied += copied;
		if(totalCopied == sizeof(LPX_HEADER2)) {
			break;
		}
		
		NdisGetNextBuffer(firstBuffer, &nextBuffer);
		firstBuffer = nextBuffer;
		if(!firstBuffer)
			break;

		// Mod by gahn.
		//Origial.
		/*
		bufferData = MmGetMdlVirtualAddress(firstBuffer);
		bufferLength = NdisBufferLength(firstBuffer);
		*/
		
		// New
		NdisQueryBufferSafe(
			firstBuffer,
			&bufferData,
			&bufferLength,
			HighPagePriority
			);
		// End Mod.
		
		copied = bufferLength < (sizeof(LPX_HEADER2) - totalCopied) ? bufferLength 
			: (sizeof(LPX_HEADER2) - totalCopied);
		bufferNumber ++;
	}
	
	if(((NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))) < sizeof(LPX_HEADER2))
		goto TossPacket;
	
	//
	// Remove Padding.
	//
	
	// Get Real Packet Length.
	usPacketSize = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK));
	
	// Get Buffer Length.
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		NULL,
		&uiBufferSize
		);
	
	if(usPacketSize < uiBufferSize) {
		UINT  BytesCopied;

		// Create New Packet.
		status = PacketAllocate(
			NULL,
			usPacketSize,
			pDeviceContext,
			RECEIVE_TYPE,
			NULL,
			0,
			NULL,
			&pNewPacket
			);
		
		if(status != NDIS_STATUS_SUCCESS) {
			DebugPrint(1, ("LpxTransferDataComplete: Can't Make New Packet.\n"));
			goto TossPacket;
		}

		NdisCopyFromPacketToPacket(
			pNewPacket,
			0,
			usPacketSize,
			Packet,
			0,
			&BytesCopied
			);	
		
		if(usPacketSize != BytesCopied) {
			DebugPrint(1, ("LpxTransferDataComplete: Can't Copy Packets.\n"));
			
			PacketFree(pNewPacket);
			goto TossPacket;
		}

		RtlCopyMemory(
			&RESERVED(pNewPacket)->EthernetHeader,
			&RESERVED(Packet)->EthernetHeader,
			ETHERNET_HEADER_LENGTH
			);

		PacketFree(Packet);
	} else {
		pNewPacket = Packet;
	}

	RESERVED(pNewPacket)->LpxSmpHeader = lpxHeader;
	
	//
	// Append received packet to InProgress Packet list.
	//	
	ExInterlockedInsertTailList(
		&pDeviceContext->PacketInProgressList,
		&(RESERVED(pNewPacket)->ListElement),
		&pDeviceContext->PacketInProgressQSpinLock
		);
	
	return;

TossPacket:

	PacketFree(Packet);
    return;
}




//
//
//	route packets to each connection.
//	SmpDoReceive() does actual works for the Stream-like packets.
//
//	called from NbfReceiveComplete()
//
//
VOID
LpxReceiveComplete	(
					IN NDIS_HANDLE BindingContext
					)
/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a connection(less) frame has been received on the
    physical link.  We dispatch to the correct packet handler here.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.
                     Nbf uses the DeviceContext for this parameter.

Return Value:

    None

--*/
{
	PLIST_ENTRY			pListEntry;
	PNDIS_PACKET		Packet;
	PNDIS_BUFFER		firstBuffer;
//	KIRQL				oldIrql;
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;

//	PUCHAR				bufferData;
	UINT				bufferLength;
//	UINT				totalCopied;
	UINT				copied;
//	UINT				bufferNumber;
    PDEVICE_CONTEXT     deviceContext = (PDEVICE_CONTEXT)BindingContext;
//    PDEVICE_CONTEXT     addressDeviceContext;
    PTP_ADDRESS			address;

    PLIST_ENTRY			Flink;
	PLIST_ENTRY			listHead;
	PLIST_ENTRY			thisEntry;
//	PLIST_ENTRY			irpListEntry;
//	PIRP				irp;

	PUCHAR				packetData = NULL;
	PLIST_ENTRY			p;
    PTP_ADDRESS_FILE	addressFile;
	KIRQL				lpxOldIrql;
//	USHORT				usPacketSize;
//	UINT				uiBufferSize;

	PSERVICE_POINT		connectionServicePoint;
	PSERVICE_POINT		listenServicePoint;

#ifdef __VERSION_CONTROL__
	PSERVICE_POINT		connectingServicePoint;
#endif

	BOOLEAN				refAddress = FALSE ;
	BOOLEAN				refAddressFile = FALSE ;
	BOOLEAN				refConnection = FALSE ;
		
	DebugPrint(4, ("[Lpx]LpxReceiveComplete: Entered %d\n", KeGetCurrentIrql()));
	
	//
	// Process In Progress Packets.
	//
	while((pListEntry = ExInterlockedRemoveHeadList(&deviceContext->PacketInProgressList, &deviceContext->PacketInProgressQSpinLock)) != NULL) {
		reserved = CONTAINING_RECORD(pListEntry, LPX_RESERVED, ListElement);
		Packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		packetData = NULL;
		
		lpxHeader = RESERVED(Packet)->LpxSmpHeader;
		
		DebugPrint(4,("From %02X%02X%02X%02X%02X%02X:%04X\n",
			RESERVED(Packet)->EthernetHeader.SourceAddress[0],
			RESERVED(Packet)->EthernetHeader.SourceAddress[1],
			RESERVED(Packet)->EthernetHeader.SourceAddress[2],
			RESERVED(Packet)->EthernetHeader.SourceAddress[3],
			RESERVED(Packet)->EthernetHeader.SourceAddress[4],
			RESERVED(Packet)->EthernetHeader.SourceAddress[5],
			lpxHeader->SourcePort));
		
		DebugPrint(4,("To %02X%02X%02X%02X%02X%02X:%04X\n",
			RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
			RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
			lpxHeader->DestinationPort));

		address = NULL;

		//
		//	match destination Address
		//

		// lock was missing @hootch@ 0825
		ACQUIRE_SPIN_LOCK (&deviceContext->SpinLock, &lpxOldIrql);

		for (Flink = deviceContext->AddressDatabase.Flink;
			Flink != &deviceContext->AddressDatabase;
			Flink = Flink->Flink, address = NULL) {
			
			address = CONTAINING_RECORD (
				Flink,
				TP_ADDRESS,
				Linkage);
			
			if ((address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
				continue;
			}
			
			if (address->NetworkName == NULL) {
				continue;
			}
/*			
			if(!memcmp(&address->NetworkName->LpxAddress.Node, RESERVED(Packet)->EthernetHeader.DestinationAddress, ETHERNET_ADDRESS_LENGTH)
				&& address->NetworkName->LpxAddress.Port == lpxHeader->DestinationPort) 
				*/
			if(address->NetworkName->LpxAddress.Port == lpxHeader->DestinationPort) {
				// Broadcast?
				ULONG	ulSum, i;
				
				ulSum = 0;
				for(i = 0; i < 6; i++)
					ulSum += RESERVED(Packet)->EthernetHeader.DestinationAddress[i];

				if(ulSum == 0xFF * 6) {
					//
					//
					// added by hootch
					NbfReferenceAddress("ReceiveCompletion", address, AREF_REQUEST) ;
					refAddress = TRUE ;
					break;
				}

				if(!memcmp(&address->NetworkName->LpxAddress.Node, RESERVED(Packet)->EthernetHeader.DestinationAddress, ETHERNET_ADDRESS_LENGTH)) {
					//
					// added by hootch
					NbfReferenceAddress("ReceiveCompletion", address, AREF_REQUEST) ;
					refAddress = TRUE ;
					break;
				}
			}
		}
		// lock was missing @hootch@ 0825
		RELEASE_SPIN_LOCK (&deviceContext->SpinLock, lpxOldIrql);

		if(address == NULL) {

#if DBG
			if(!(RESERVED(Packet)->EthernetHeader.DestinationAddress[0] == 0xff &&
				RESERVED(Packet)->EthernetHeader.DestinationAddress[1] == 0xff &&
				RESERVED(Packet)->EthernetHeader.DestinationAddress[2] == 0xff &&
				RESERVED(Packet)->EthernetHeader.DestinationAddress[3] == 0xff &&
				RESERVED(Packet)->EthernetHeader.DestinationAddress[4] == 0xff &&
				RESERVED(Packet)->EthernetHeader.DestinationAddress[5] == 0xff ) )
			{
			DebugPrint(2, ("No End Point. To %02X%02X%02X%02X%02X%02X:%04X\n",
				RESERVED(Packet)->EthernetHeader.DestinationAddress[0],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[1],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[2],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[3],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[4],
				RESERVED(Packet)->EthernetHeader.DestinationAddress[5],
				lpxHeader->DestinationPort));
			}
#endif
			goto TossPacket;
		}
		
		//
		//	if it is a stream-like(called LPX-Stream) packet,
		//	match destination Connection ( called service point in LPX )
		//
		if(lpxHeader->LpxType == LPX_TYPE_STREAM) 
		{
			connectionServicePoint = NULL;
			listenServicePoint = NULL;
#ifdef __VERSION_CONTROL__
			connectingServicePoint = NULL;
#endif

			// lock was missing @hootch@ 0825
			ACQUIRE_SPIN_LOCK (&address->SpinLock, &lpxOldIrql);
			
			listHead = &address->ConnectionServicePointList;

			for(thisEntry = listHead->Flink;
				thisEntry != listHead;
				thisEntry = thisEntry->Flink, connectionServicePoint = NULL)
			{
				UCHAR	zeroNode[6] = {0, 0, 0, 0, 0, 0};


				connectionServicePoint = CONTAINING_RECORD(thisEntry, SERVICE_POINT, ServicePointListEntry);
				DebugPrint(4,("connectionServicePoint %02X%02X%02X%02X%02X%02X:%04X\n",
					connectionServicePoint->DestinationAddress.Node[0],
					connectionServicePoint->DestinationAddress.Node[1],
					connectionServicePoint->DestinationAddress.Node[2],
					connectionServicePoint->DestinationAddress.Node[3],
					connectionServicePoint->DestinationAddress.Node[4],
					connectionServicePoint->DestinationAddress.Node[5],
					connectionServicePoint->DestinationAddress.Port));

//				ACQUIRE_DPC_SPIN_LOCK(&connectionServicePoint->SpinLock) ;

				if(connectionServicePoint->SmpState != SMP_CLOSE
					&& (!memcmp(connectionServicePoint->DestinationAddress.Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH)
					&& connectionServicePoint->DestinationAddress.Port == lpxHeader->SourcePort)) 
				{
					//
					//	reference Connection
					//	hootch	09042003
					NbfReferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST) ;
					refConnection = TRUE ;

//					RELEASE_DPC_SPIN_LOCK(&connectionServicePoint->SpinLock) ;

					DebugPrint(4, ("[LPX] connectionServicePoint = %p found!\n", connectionServicePoint));

					break;
				}

				if(connectionServicePoint->SmpState == SMP_LISTEN 
					&& !memcmp(&connectionServicePoint->DestinationAddress.Node, zeroNode , 6)
					&& connectionServicePoint->ListenIrp != NULL)
				{

					listenServicePoint = connectionServicePoint;
					DebugPrint(1, ("listenServicePoint = %p found!\n", listenServicePoint));
				}

#ifdef __VERSION_CONTROL__
				if(connectionServicePoint->SmpState == SMP_SYN_SENT 
					&& !memcmp(&connectionServicePoint->DestinationAddress.Node,  RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH)
					&& (NTOHS(lpxHeader->Lsctl) & LSCTL_CONNREQ))  
				{

					connectingServicePoint = connectionServicePoint;
					DebugPrint(1, ("connectingServicePoint = %p\n", connectingServicePoint));
				}
#endif
//				RELEASE_DPC_SPIN_LOCK(&connectionServicePoint->SpinLock) ;

				connectionServicePoint = NULL;
			}


			
			if(connectionServicePoint == NULL) {
				if(listenServicePoint != NULL) {
					connectionServicePoint = listenServicePoint;
					//
					//	add one reference count
					//	hootch	09042003
					NbfReferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST) ;
					refConnection = TRUE ;
				} else {
#ifdef __VERSION_CONTROL__
					if(connectingServicePoint != NULL) {
						memcpy(connectingServicePoint->DestinationAddress.Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH);
						connectingServicePoint->DestinationAddress.Port = lpxHeader->SourcePort;
						connectionServicePoint = connectingServicePoint;
						//
						//	add one reference count
						//	hootch	09042003
						NbfReferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST) ;
						refConnection = TRUE ;
					} else {
#endif
						RELEASE_SPIN_LOCK (&address->SpinLock, lpxOldIrql);

						goto TossPacket;
					}
				}
			}
			// lock was missing @hootch@ 0825
			RELEASE_SPIN_LOCK (&address->SpinLock, lpxOldIrql);

			/*
			Move to SmpDoReceive...

			if(((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(connectionServicePoint->SmpContext.Sequence))) > 0) {
				DebugPrint(1, ("abnormal Packet\n"));
				goto TossPacket;
			}
			*/

			RESERVED(Packet)->PacketDataOffset = sizeof(LPX_HEADER2);

			//
			//	call stream-like(called LPX-Stream) routine to do more process
			//
			SmpDoReceiveRequest(&connectionServicePoint->SmpContext, Packet);

			goto loopEnd;

		//
		//	a datagram packet
		//
		} else if(lpxHeader->LpxType == LPX_TYPE_DATAGRAM) {

			DebugPrint(4, ("[LPX] LpxReceiveComplete: DataGram packet arrived.\n"));

			//
			//	acquire address->SpinLock to traverse AddressFileDatabase
			//
			// lock was missing @hootch@ 0825
			ACQUIRE_DPC_SPIN_LOCK (&address->SpinLock);

			p = address->AddressFileDatabase.Flink;
			while (p != &address->AddressFileDatabase) {
				addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
				//
				//	Address File is protected by the spinlock of Address owning it.
				//
				if (addressFile->State != ADDRESSFILE_STATE_OPEN) {
					p = p->Flink;
					continue;
				}
				DebugPrint(4, ("[LPX] LpxReceiveComplete udp 2\n"));
				NbfReferenceAddressFile(addressFile);
				refAddressFile = TRUE ;
				break;
			}

			if(p == &address->AddressFileDatabase) {

				RELEASE_DPC_SPIN_LOCK (&address->SpinLock);

				DebugPrint(4, ("[LPX] LpxReceiveComplete: DataGram Packet - No addressFile matched.\n"));
				goto TossPacket;
			}

			RELEASE_DPC_SPIN_LOCK (&address->SpinLock);

			
			if (addressFile->RegisteredReceiveDatagramHandler) {

				ULONG				indicateBytesCopied, mdlBytesCopied, bytesToCopy;
				NTSTATUS			ntStatus;
				TA_NETBIOS_ADDRESS	sourceName;
				PIRP				irp;
				PIO_STACK_LOCATION	irpSp;
//				UINT				headerLength;
//				UINT				bufferOffset;
				UINT				totalCopied;
				PUCHAR				bufferData;
				PNDIS_BUFFER		nextBuffer;
				ULONG				userDataLength;
//				KIRQL				cancelIrql;

				userDataLength = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)) - sizeof(LPX_HEADER2);

				DebugPrint(4, ("[LPX] LpxReceiveComplete: call UserDataGramHandler with a DataGram packet. NTOHS(lpxHeader->PacketSize) = %d, userDataLength = %d\n",
					NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK), userDataLength));

				packetData = ExAllocatePool (NonPagedPool, userDataLength);
				//
				//	NULL pointer check.
				//
				//	added by @hootch@ 0812
				//
				if(packetData == NULL) {
					DebugPrint(0, ("[LPX] LpxReceiveComplete failed to allocate nonpaged pool for packetData\n"));
					goto TossPacket ;
				}


				//		if(RESERVED(Packet)->Type == DIRECT_RECEIVE_TYPE)
				//			headerLength = ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2);
				//		else
				
				// Bypass Header.
/*
				headerLength = sizeof(LPX_HEADER2);
				
				NdisQueryPacket(
					packet,
					NULL,
					NULL,
					&firstBuffer,
					NULL
					);
				
				while(firstBuffer) {
					if(headerLength <= NdisBufferLength(firstBuffer)) {
						bufferOffset = headerLength;
						break;
					}
					
					headerLength -= NdisBufferLength(firstBuffer);
					
					NdisGetNextBuffer(firstBuffer, &nextBuffer);
					firstBuffer = nextBuffer;
					if(!firstBuffer)
						break;
				}
*/				
				//
				// Copy User Data of the packet
				//
				NdisQueryPacket(
					Packet,
					NULL,
					NULL,
					&firstBuffer,
					NULL
					);

				totalCopied = 0;
				bufferData = MmGetMdlVirtualAddress(firstBuffer);
				bufferLength = NdisBufferLength(firstBuffer);
				bufferLength -= sizeof(LPX_HEADER2);
				bufferData += sizeof(LPX_HEADER2);
				copied = (bufferLength < userDataLength) ? bufferLength : userDataLength;
				
				while(firstBuffer) {
					if(copied)
						RtlCopyMemory(
						&packetData[totalCopied],
						bufferData,
						copied
						);
					
					totalCopied += copied;
					if(totalCopied == (USHORT)(NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) - sizeof(LPX_HEADER2)))
						break;
					
					NdisGetNextBuffer(firstBuffer, &nextBuffer);
					firstBuffer = nextBuffer;
					if(!firstBuffer)
						break;
					
					bufferData = MmGetMdlVirtualAddress(firstBuffer);
					bufferLength = NdisBufferLength(firstBuffer);
					copied = bufferLength < (userDataLength - totalCopied) ? bufferLength 
						: (userDataLength - totalCopied);
				}

				//
				//	call user-defined ReceiveDatagramHandler with copied data
				//
				indicateBytesCopied = 0;

				sourceName.TAAddressCount = 1;
				sourceName.Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
				sourceName.Address[0].AddressType = TDI_ADDRESS_TYPE_LPX;
				memcpy(((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Node, RESERVED(Packet)->EthernetHeader.SourceAddress, ETHERNET_ADDRESS_LENGTH);
				((PLPX_ADDRESS)(&sourceName.Address[0].Address))->Port = lpxHeader->SourcePort;

				DebugPrint(3, ("[LPX] LPxReceiveComplete: DATAGRAM: SourceAddress=%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
														RESERVED(Packet)->EthernetHeader.SourceAddress[0],
														RESERVED(Packet)->EthernetHeader.SourceAddress[1],
														RESERVED(Packet)->EthernetHeader.SourceAddress[2],
														RESERVED(Packet)->EthernetHeader.SourceAddress[3],
														RESERVED(Packet)->EthernetHeader.SourceAddress[4],
														RESERVED(Packet)->EthernetHeader.SourceAddress[5],
														NTOHS(lpxHeader->SourcePort)
													));

				ntStatus = (*addressFile->ReceiveDatagramHandler)(
					addressFile->ReceiveDatagramHandlerContext,
					sizeof (TA_NETBIOS_ADDRESS),
					&sourceName,
					0,
					NULL,
					TDI_RECEIVE_NORMAL,
					userDataLength,
					userDataLength,  // available
					&indicateBytesCopied,
					packetData,
					&irp
					);

				if (ntStatus == STATUS_SUCCESS) 
				{
					//
					// The client accepted the datagram. We're done.
					//
					DebugPrint(4, ("[LPX] LpxReceiveComplete: DATAGRAM: A datagram packet consumed. STATUS_SUCCESS, userDataLength = %d, indicateBytesCopied = %d\n", userDataLength, indicateBytesCopied));
				} else if (ntStatus == STATUS_DATA_NOT_ACCEPTED) 
				{
					//
					// The client did not accept the datagram and we need to satisfy
					// a TdiReceiveDatagram, if possible.
					//
					DebugPrint(4, ("[LPX] LpxReceiveComplete: DATAGRAM: DataGramHandler didn't accept a datagram packet.\n"));
					IF_NBFDBG (NBF_DEBUG_DATAGRAMS) {
						NbfPrint0 ("[LPX] LpxReceiveComplete: DATAGRAM: Picking off a rcv datagram request from this address.\n");
					}

					ntStatus = STATUS_MORE_PROCESSING_REQUIRED;
					
				} else if (ntStatus == STATUS_MORE_PROCESSING_REQUIRED) 
				{
					//
					// The client returned an IRP that we should queue up to the
					// address to satisfy the request.
					//

					DebugPrint(1, ("[LPX] LpxReceiveComplete: DATAGRAM: STATUS_MORE_PROCESSING_REQUIRED\n"));
					irp->IoStatus.Status = STATUS_PENDING;  // init status information.
					irp->IoStatus.Information = 0;
					irpSp = IoGetCurrentIrpStackLocation (irp); // get current stack loctn.
					if ((irpSp->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL) 
						|| (irpSp->MinorFunction != TDI_RECEIVE_DATAGRAM)) 
					{
						DebugPrint(1, ("[LPX] LpxReceiveComplete: DATAGRAM: Wrong IRP: Maj:%d Min:%d\n",irpSp->MajorFunction, irpSp->MinorFunction) );
						irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
						goto TossPacket;
					}

					//
					// Now copy the actual user data.
					//
					mdlBytesCopied = 0;
					bytesToCopy =  userDataLength - indicateBytesCopied;

					if ((bytesToCopy > 0) && irp->MdlAddress) 
					{
						ntStatus = TdiCopyBufferToMdl (
							packetData,
							indicateBytesCopied,
							bytesToCopy,
							irp->MdlAddress,
							0,
							&mdlBytesCopied
							);
					} else {
						ntStatus = STATUS_SUCCESS;
					}

					irp->IoStatus.Information = mdlBytesCopied;
					irp->IoStatus.Status = ntStatus;
					LEAVE_NBF;

//					IoAcquireCancelSpinLock(&cancelIrql);
					IoSetCancelRoutine(irp, NULL);
//					IoReleaseCancelSpinLock(cancelIrql);

					DebugPrint(1, ("[LPX]LpxReceiveComplete: DATAGRAM: IRP %lx completed with NTSTATUS:%08lx.\n ", irp, ntStatus));
					IoCompleteRequest (irp, IO_NETWORK_INCREMENT);
					ENTER_NBF;
				}
			}

		}

TossPacket:
		if(packetData != NULL)
			ExFreePool(packetData);

		PacketFree(Packet);

loopEnd:
		//
		//	clean up reference count
		//	added by hootch 09042003
		//
		if(refAddress) {
			NbfDereferenceAddress("ReceiveCompletion", address, AREF_REQUEST);
			refAddress = FALSE ;
		}
		if(refAddressFile) {
			NbfDereferenceAddressFile (addressFile);
			refAddressFile = FALSE ;
		}
		if(refConnection) {
			NbfDereferenceConnection("ReceiveCompletion", connectionServicePoint->Connection, CREF_REQUEST);
			refConnection = FALSE ;
		}

		continue;
	}

    return;

}

LpxSend(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   )
{
	PSERVICE_POINT				servicePoint;
    PIO_STACK_LOCATION			irpSp;
    PTDI_REQUEST_KERNEL_SEND	parameters;
    PDEVICE_CONTEXT				deviceContext;
	NDIS_STATUS					status;
    UINT						maxUserData;
	UINT						mss;
	PUCHAR						userData;
	ULONG						userDataLength;
	PNDIS_PACKET				packet;
//    KIRQL						cancelIrql;
	KIRQL						oldIrql ;
	BOOLEAN						bFailAlloc = FALSE;

	DebugPrint(4, ("PACKET_SEND\n"));

	servicePoint = (PSERVICE_POINT)Connection;
    irpSp = IoGetCurrentIrpStackLocation (Irp);
	ASSERT(irpSp);
    parameters = (PTDI_REQUEST_KERNEL_SEND)(&irpSp->Parameters);
	ASSERT(parameters);
	deviceContext = (PDEVICE_CONTEXT)servicePoint->Address->Provider;
	
    IRP_SEND_IRP(irpSp) = Irp;
    IRP_SEND_REFCOUNT(irpSp) = 1;

    Irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
    Irp->IoStatus.Information = 0;

	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;

	if(servicePoint->SmpState != SMP_ESTABLISHED) {
		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

		DebugPrint(1, ("[LPX] LpxSend: SmpState %x is not SMP_ESTABLISHED.\n", servicePoint->SmpState));

        NbfDereferenceSendIrp ("Complete", irpSp, RREF_CREATION);     // remove creation reference.

		return STATUS_PENDING; 
	}

    NbfReferenceConnection ("LpxSend", Connection, CREF_SEND_IRP);

	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

	IoMarkIrpPending(Irp);

//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, LpxCancelSend);
//	IoReleaseCancelSpinLock(cancelIrql);

    MacReturnMaxDataSize(
        &deviceContext->MacInfo,
        NULL,
        0,
        deviceContext->MaxSendPacketSize,
        TRUE,
        &maxUserData);

	mss = maxUserData - sizeof(LPX_HEADER2);
	mss >>= 2;
	mss <<= 2;
	userDataLength = parameters->SendLength;
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);

	while (userDataLength) 
	{
		USHORT			copy;

		copy = (USHORT)mss;
		if(copy > userDataLength)
			copy = (USHORT)userDataLength;

		
		status = PacketAllocate(
			servicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			deviceContext,
			SEND_TYPE,
			userData,
			copy,
			irpSp,
			&packet
			);
		
		if(!NT_SUCCESS(status)) {
			bFailAlloc = TRUE;
			DebugPrint(2, ("[LPX]LpxSend: packet == NULL\n"));
			SmpPrintState(2, "[LPX]LpxSend: PacketAlloc", servicePoint);
            //NbfCompleteSendIrp (Irp, STATUS_CANCELLED, 0);
			//return STATUS_PENDING; 
			break;
		}
				
        NbfReferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

		DebugPrint(4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy));
		userDataLength -= copy;

		status = TransmitPacket(deviceContext, servicePoint, packet, DATA, copy);

		userData += copy;
	}
	
	if((TRUE == bFailAlloc) 
		&& (parameters->SendLength == userDataLength))
	{
		NbfCompleteSendIrp(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);
	}
	else{
		NbfCompleteSendIrp (Irp, STATUS_SUCCESS, parameters->SendLength - userDataLength);
	}

	return STATUS_PENDING; 
}

VOID LpxCallUserReceiveHandler(
	PSERVICE_POINT	servicePoint
	)
{
//	PNDIS_PACKET	packet;

//
//	Not need packets to call User ReceiveHandler in the context at this time.
//
//	removed by hootch 09052003
//
//	packet = PacketPeek(&servicePoint->SmpContext.RcvDataQueue, &servicePoint->ServicePointQSpinLock);
//	DebugPrint(2, ("before ReceiveHandler packet = %p\n", packet));
	
	if(/*packet &&*/ servicePoint->Connection && servicePoint->Connection->AddressFile)
	{
	    ULONG ReceiveFlags;
	    NTSTATUS status;
		PIRP	irp = NULL;
	    ULONG indicateBytesTransferred;
	
	    ReceiveFlags = TDI_RECEIVE_AT_DISPATCH_LEVEL | TDI_RECEIVE_ENTIRE_MESSAGE | TDI_RECEIVE_NO_RESPONSE_EXP;

		DebugPrint(3, ("before ReceiveHandler ServicePoint->Connection->AddressFile->RegisteredReceiveHandler=%d\n",
		servicePoint->Connection->AddressFile->RegisteredReceiveHandler));
		status = (*servicePoint->Connection->AddressFile->ReceiveHandler)(
		                servicePoint->Connection->AddressFile->ReceiveHandlerContext,
			            servicePoint->Connection->Context,
				        ReceiveFlags,
					    0,
						0,             // BytesAvailable
	                    &indicateBytesTransferred,
		                NULL,
			            NULL);

		DebugPrint(3, ("status = %x, Irp = %p\n", status, irp));
	}

	return;
}




void LpxCompleteIRPRequest(
	IN 		PSERVICE_POINT	servicePoint,
	IN		PIRP			Irp
	) {
	LONG	cnt ;
	KIRQL				oldIrql;
	BOOLEAN				raised = FALSE;



    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}


	ExInterlockedInsertTailList(
		&servicePoint->ReceiveIrpList,
		&Irp->Tail.Overlay.ListEntry,
		&servicePoint->ReceiveIrpQSpinLock
	);

	cnt = InterlockedIncrement(&servicePoint->SmpContext.RequestCnt) ;
	if( cnt == 1 ) {
		KeInsertQueueDpc(&servicePoint->SmpContext.SmpWorkDpc, NULL, NULL) ;
//		KeInsertQueueDpc(&SocketLpxDeviceContext->LpxWorkDpc, &servicePoint->SmpContext, NULL);
	}

	if(raised == TRUE)
		KeLowerIrql(oldIrql);
}



NDIS_STATUS
LpxRecv(
		IN 		PTP_CONNECTION	Connection,
		IN OUT	PIRP			Irp
		)
{
	PSERVICE_POINT	servicePoint;
	NDIS_STATUS		status;
//	PIRP			irp;
//	KIRQL			cancelIrql;
    KIRQL			lpxOldIrql;
	PIO_STACK_LOCATION	irpSp;
	ULONG				userDataLength;
	PUCHAR				userData = NULL;
	DebugPrint(4, ("LPX_RECEIVE\n"));

	servicePoint = (PSERVICE_POINT)Connection;

	//
	//	acquire Connection's SpinLock
	//
	//	added by hootch 09042003
	//	
    ACQUIRE_SPIN_LOCK (&servicePoint->SpinLock, &lpxOldIrql);

	if(servicePoint->SmpState != SMP_ESTABLISHED
		&& servicePoint->SmpState != SMP_CLOSE_WAIT) 
	{
		RELEASE_SPIN_LOCK (&servicePoint->SpinLock, lpxOldIrql);

		DebugPrint(0, ("LPX_RECEIVE OUT\n"));

		status = STATUS_UNSUCCESSFUL;

		goto ErrorOut;
	}

	if(servicePoint->Shutdown & SMP_RECEIVE_SHUTDOWN)
	{
		RELEASE_SPIN_LOCK (&servicePoint->SpinLock, lpxOldIrql);

		DebugPrint(0, ("LPX_RECEIVE OUT\n"));

		status = STATUS_UNSUCCESSFUL;

		goto ErrorOut;
	}

	//
	//	release Connection's SpinLock
	//
    RELEASE_SPIN_LOCK (&servicePoint->SpinLock, lpxOldIrql);

	irpSp = IoGetCurrentIrpStackLocation(Irp);
	userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);

	if(userDataLength == 0){
		DebugPrint(0, ("LPX_RECEIVE fail for Invalid Address\n"));
		Irp->IoStatus.Information = 0;
		status = STATUS_UNSUCCESSFUL;
		goto ErrorOut;
	}

	if(userData == NULL ){
		DebugPrint(0, ("LPX_RECEIVE fail for Invalid Address\n"));
		Irp->IoStatus.Information = 0;
		status = STATUS_UNSUCCESSFUL;
		goto ErrorOut;
	}
	
	IoMarkIrpPending(Irp);
//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, LpxCancelRecv);
//	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Information = 0;

	LpxCompleteIRPRequest(servicePoint, Irp) ;

	return STATUS_PENDING; 

ErrorOut:

//	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
//	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = status;
	DebugPrint(1, ("[LPX]LpxRecv: IRP %lx completed with error: NTSTATUS:%08lx.\n ", Irp, status));
	IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

	return status;
}


NDIS_STATUS
LpxSendDatagram(
    IN 		PTP_ADDRESS	Address,
 	IN OUT	PIRP		Irp
   )
{
    PIO_STACK_LOCATION			irpSp;
    PTDI_REQUEST_KERNEL_SENDDG	parameters;
    PDEVICE_CONTEXT				deviceContext;
	NDIS_STATUS					status;
    UINT						maxUserData;
	UINT						mss;
	PUCHAR						userData;
	ULONG						userDataLength;
	PNDIS_PACKET				packet;
    TRANSPORT_ADDRESS UNALIGNED *transportAddress;
	PLPX_ADDRESS				remoteAddress;


	DebugPrint(2, ("LpxSendDatagram\n"));

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    parameters = (PTDI_REQUEST_KERNEL_SENDDG)(&irpSp->Parameters);
	deviceContext = (PDEVICE_CONTEXT)Address->Provider;

    IRP_SEND_IRP(irpSp) = Irp;
    IRP_SEND_REFCOUNT(irpSp) = 1;

    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
    Irp->IoStatus.Information = 0;


	IoMarkIrpPending(Irp);
//    IoSetCancelRoutine(Irp, LpxCancelSend);

    MacReturnMaxDataSize(
        &deviceContext->MacInfo,
        NULL,
        0,
        deviceContext->MaxSendPacketSize,
        TRUE,
        &maxUserData);

	mss = maxUserData - sizeof(LPX_HEADER2);
	mss >>= 2;
	mss <<= 2;
	userDataLength = parameters->SendLength;
	userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);

	transportAddress = (TRANSPORT_ADDRESS UNALIGNED *)parameters->SendDatagramInformation->RemoteAddress;
	remoteAddress = (PLPX_ADDRESS)&transportAddress->Address[0].Address[0];

//    NbfReferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

	while (userDataLength) 
	{
		USHORT			copy;
		PNDIS_BUFFER	firstBuffer;	
		PUCHAR			packetData;
		USHORT			type;
		PLPX_HEADER2	lpxHeader;
		HANDLE			NDISHandle ;

		copy = (USHORT)mss;
		if(copy > userDataLength)
			copy = (USHORT)userDataLength;

		status = PacketAllocate(
			NULL,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			deviceContext,
			SEND_TYPE,
			userData,
			copy,
			irpSp,
			&packet
			);

		if(!NT_SUCCESS(status)) {
			DebugPrint(0, ("packet == NULL\n"));
//			SmpPrintState(0, "PacketAlloc", servicePoint);
//            NbfCompleteSendIrp (Irp, STATUS_CANCELLED, 0);
	        NbfDereferenceSendIrp ("Packetize", irpSp, RREF_PACKET);
			return STATUS_PENDING; 
		}

        NbfReferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

		DebugPrint(4, ("SEND_DATA userDataLength = %d, copy = %d\n", userDataLength, copy));
		userDataLength -= copy;

//		status = TransmitPacket(servicePoint, packet, DATA, copy);

		//
		//	get a NDIS packet
		//
		NdisQueryPacket(
				packet,
				NULL,
				NULL,
				&firstBuffer,
				NULL
		);
			
		packetData = MmGetMdlVirtualAddress(firstBuffer);

		//
		//	set destination and source MAC address
		//
		RtlCopyMemory(
				&packetData[0],
				remoteAddress->Node,
				ETHERNET_ADDRESS_LENGTH
			);

		DebugPrint(2,("remoteAddress %02X%02X%02X%02X%02X%02X:%04X\n",
					remoteAddress->Node[0],
					remoteAddress->Node[1],
					remoteAddress->Node[2],
					remoteAddress->Node[3],
					remoteAddress->Node[4],
					remoteAddress->Node[5],
					NTOHS(remoteAddress->Port)));

		RtlCopyMemory(
				&packetData[ETHERNET_ADDRESS_LENGTH],
				Address->NetworkName->LpxAddress.Node,
				ETHERNET_ADDRESS_LENGTH
				);

		//
		//	set LPX to ethernet type.
		//
		type = HTONS(ETH_P_LPX);
		RtlCopyMemory(
			&packetData[ETHERNET_ADDRESS_LENGTH*2],
			&type, //&ServicePoint->DestinationAddress.Port,
			2
			);

		//
		//	set LPX header for datagram
		//
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

		lpxHeader->PacketSize = HTONS(sizeof(LPX_HEADER2) + copy);
		lpxHeader->LpxType = LPX_TYPE_DATAGRAM;
//			lpxHeader->CheckSum = 0;
		lpxHeader->DestinationPort = remoteAddress->Port;
		lpxHeader->SourcePort = Address->NetworkName->LpxAddress.Port;
		lpxHeader->MessageId = HTONS((USHORT)LPX_HOST_DGMSG_ID);
		lpxHeader->MessageLength = HTONS((USHORT)copy);
		lpxHeader->FragmentId = 0;
		lpxHeader->FragmentLength = 0;
		lpxHeader->ResevedU1 = 0;

		//
		//	pass a datagram packet to NDIS.
		//
		//
		//	TODO: more precious lock needed
		//	patched by hootch
		//
//			NdisSend(
//					&status,
//					deviceContext->NdisBindingHandle,
//					packet
//					);

		NDISHandle = deviceContext->NdisBindingHandle ;
		if(NDISHandle) {
			NdisSend(
					&status,
					NDISHandle,
					packet
				);
		} else {
			PacketFree(packet);
			Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
			Irp->IoStatus.Information = parameters->SendLength - userDataLength;
//
//		hootch 03092004
//				we must not complete any IRP when we don't return STATUS_PENDING
//
//
//				NbfDereferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

			return STATUS_NO_SUCH_DEVICE; 
		}

		if(status != NDIS_STATUS_PENDING) {
			PacketFree(packet);
			status = STATUS_SUCCESS;
		} else
			continue;
		
		userData += copy;
	}

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = parameters->SendLength;
    NbfDereferenceSendIrp ("Packetize", irpSp, RREF_PACKET);

	return STATUS_PENDING; 
}

VOID
LpxCancelIrp(
			 IN PDEVICE_OBJECT DeviceObject,
			 IN PIRP Irp
			 )

/*++

Routine Description:

    This routine is called by the I/O system to cancel a send.
    The send is found on the connection's send queue; if it is the
    current request it is cancelled and the connection is torn down,
    otherwise it is silently cancelled.

    NOTE: This routine is called with the CancelSpinLock held and
    is responsible for releasing it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    none.

--*/

{
//    KIRQL oldirql, oldirql1;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
//    PIRP SendIrp;
//    PLIST_ENTRY p;
//    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    DebugPrint(1, ("LpxCancelIrp\n"));
    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_CONNECT
			|| IrpSp->MinorFunction == TDI_LISTEN
			|| IrpSp->MinorFunction == TDI_ACCEPT
			|| IrpSp->MinorFunction == TDI_DISCONNECT
			|| IrpSp->MinorFunction == TDI_SEND
			|| IrpSp->MinorFunction == TDI_RECEIVE));

    Connection = IrpSp->FileObject->FsContext;

    //
    // Since this IRP is still in the cancellable state, we know
    // that the connection is still around (although it may be in
    // the process of being torn down).
    //


//    ACQUIRE_SPIN_LOCK (Connection->LinkSpinLock, &oldirql);
    NbfReferenceConnection ("Cancelling IRP", Connection, CREF_COMPLETE_SEND);
//    RELEASE_SPIN_LOCK (Connection->LinkSpinLock, oldirql);

//	IoReleaseCancelSpinLock (Irp->CancelIrql);

#if DBG
        DbgPrint("LPX: Canceled a irp:IRP %lx on %lx\n",
                Irp, Connection);
#endif

	SmpFreeServicePoint(&Connection->ServicePoint);

    NbfDereferenceConnection ("Cancelling IRP", Connection, CREF_COMPLETE_SEND);
}


static VOID
SmpCancelIrps(
	IN PSERVICE_POINT	ServicePoint
	);


static BOOLEAN
SmpSendTest(
	PSMP_CONTEXT	SmpContext,
	PNDIS_PACKET	Packet
	);


static void
RoutePacketRequest(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	) ;

static NTSTATUS
RoutePacket(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	);


static LONGLONG
CalculateRTT(
	IN	PSMP_CONTEXT	SmpContext
	);


static INT
SmpRetransmitCheck(
				   IN PSMP_CONTEXT	SmpContext,
				   IN LONG			AckSequence,
				   IN PACKET_TYPE	PacketType
				   ) ;


#ifdef DBG
static VOID
SmpPrintState(
	IN	LONG			Debuglevel,
	IN	PCHAR			Where,
	IN	PSERVICE_POINT	ServicePoint
	)
{
	PSMP_CONTEXT	smpContext = &ServicePoint->SmpContext;

#if !DBG
	UNREFERENCED_PARAMETER(Debuglevel) ;
	UNREFERENCED_PARAMETER(Where) ;
#endif

	DebugPrint(Debuglevel, (Where));
	DebugPrint(Debuglevel, (" : SP %p, Seq 0x%x, RSeq 0x%x, RAck 0x%x", 
		ServicePoint, SHORT_SEQNUM(smpContext->Sequence), SHORT_SEQNUM(smpContext->RemoteSequence),
		SHORT_SEQNUM(smpContext->RemoteAck)));

//	OBSOULTE: IntervalTime
//	removed by hootch 09062003
//
//	DebugPrint(DebugLevel, (" LRetransSeq 0x%x, TimerR %d, #ExpPac %d, #Pac %d, #Cloned %d Int %llu", 
//		smpContext->LastRetransmitSequence, smpContext->TimerReason, 
//		NumberOfExportedPackets, NumberOfPackets, NumberOfCloned,  smpContext->IntervalTime.QuadPart));

	DebugPrint(Debuglevel, (" LRetransSeq 0x%x, TimerR 0x%x, #ExpPac %ld, #Pac %ld, #Cloned %ld", 
		SHORT_SEQNUM(smpContext->LastRetransmitSequence), SHORT_SEQNUM(smpContext->TimerReason), 
		NumberOfExportedPackets, NumberOfPackets, NumberOfCloned));

	DebugPrint(Debuglevel, (" #Sent %ld, #SentCom %ld, CT %I64d  RT %I64d\n", 
				NumberOfSent, NumberOfSentComplete, CurrentTime().QuadPart, smpContext->RetransmitTimeOut.QuadPart));
}
#else
#define SmpPrintState(l, w, s) 
#endif


//
// Callers of this routine must be running at IRQL PASSIVE_LEVEL
//
// comment by hootch 08262003
//
static VOID
SmpContextInit(
	IN PSERVICE_POINT	ServicePoint,
	IN PSMP_CONTEXT		SmpContext
	)
{
	DebugPrint(2, ("SmpContextInit ServicePoint = %p\n", ServicePoint));

	RtlZeroMemory(SmpContext,
				sizeof(SMP_CONTEXT));

	SmpContext->ServicePoint = ServicePoint;

	KeInitializeSpinLock(&SmpContext->TimeCounterSpinLock) ;
	SmpContext->Sequence		= 0;
	SmpContext->RemoteSequence	= 0;
	SmpContext->RemoteAck		= 0;
	SmpContext->ServerTag		= 0;

	SmpContext->MaxFlights = SMP_MAX_FLIGHT / 2;

	// ILGU for debuging
	SmpContext->TimeOutCount = 0;
//	OBSOULTE: IntervalTime
//	removed by hootch 09062003
//
//	SmpContext->IntervalTime.QuadPart = INITIAL_INTERVAL_TIME;

	KeInitializeTimer(&SmpContext->SmpTimer);
	//
	// Callers of this routine must be running at IRQL PASSIVE_LEVEL
	//
	// comment by hootch
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
//	SmpContext->SmpTimerSet = FALSE;

	KeInitializeDpc(&SmpContext->SmpTimerDpc, SmpTimerDpcRoutineRequest, SmpContext);
	KeInitializeDpc(&SmpContext->SmpWorkDpc, SmpWorkDpcRoutine, SmpContext);

	KeInitializeSpinLock(&SmpContext->SendQSpinLock);
	InitializeListHead(&SmpContext->SendQueue);
	KeInitializeSpinLock(&SmpContext->ReceiveQSpinLock);
	InitializeListHead(&SmpContext->ReceiveQueue);
	KeInitializeSpinLock(&SmpContext->RcvDataQSpinLock);
	InitializeListHead(&SmpContext->RcvDataQueue);
	KeInitializeSpinLock(&SmpContext->RetransmitQSpinLock);
	InitializeListHead(&SmpContext->RetransmitQueue);
	KeInitializeSpinLock(&SmpContext->WriteQSpinLock);
	InitializeListHead(&SmpContext->WriteQueue);

	return;
}


static BOOLEAN
SmpFreeServicePoint(
	IN	PSERVICE_POINT	ServicePoint
	)
{
	PSMP_CONTEXT	smpContext = &ServicePoint->SmpContext;
	PNDIS_PACKET	packet;
	UINT			back_log = 0, receive_queue = 0, receivedata_queue = 0,
					write_queue = 0, retransmit_queue = 0;
	KIRQL			oldIrql ;
//	PTP_CONNECTION  connection;
	PTP_ADDRESS		address;
	
	// added by hootch 08262003
	ACQUIRE_SPIN_LOCK (&ServicePoint->SpinLock, &oldIrql);

	if(ServicePoint->SmpState == SMP_CLOSE) {
		RELEASE_SPIN_LOCK (&ServicePoint->SpinLock, oldIrql);
		return FALSE ;
	}

	KeCancelTimer(&ServicePoint->SmpContext.SmpTimer);

	//
	//	change the state to SMP_CLOSE to stop I/O in this connection
	//
	ServicePoint->SmpState = SMP_CLOSE;
	ServicePoint->Shutdown	= 0;
	
//	connection = ServicePoint->Connection;
//	ASSERT(connection);
//	connection->IsDisconnted = TRUE;
	address = ServicePoint->Address;
	ASSERT(address);
	RemoveEntryList(&ServicePoint->ServicePointListEntry);
	NbfDereferenceAddress ("ServicePoint deleting", address, AREF_REQUEST);
	
	// added by hootch 08262003
	RELEASE_SPIN_LOCK (&ServicePoint->SpinLock, oldIrql);

	SmpPrintState(1, "SmpFreeServicePoint", ServicePoint);

	DebugPrint(2, ("sequence = %x, fin_seq = %x, rmt_seq = %x, rmt_ack = %x\n", 
		SHORT_SEQNUM(smpContext->Sequence), SHORT_SEQNUM(smpContext->FinSequence),
		SHORT_SEQNUM(smpContext->RemoteSequence),SHORT_SEQNUM(smpContext->RemoteAck)));
	DebugPrint(2, ("last_retransmit_seq=%x, reason = %x\n",
		SHORT_SEQNUM(smpContext->LastRetransmitSequence), smpContext->TimerReason));

	while(packet
		= PacketDequeue(&smpContext->WriteQueue, &smpContext->WriteQSpinLock))
	{
		PIO_STACK_LOCATION	irpSp;
		PIRP				irp;

		DebugPrint(1, ("TransmitQueue packet deleted\n"));
		irpSp = RESERVED(packet)->IrpSp;
		if(irpSp != NULL) {
			irp = IRP_SEND_IRP(irpSp);
			irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
			irp->IoStatus.Information = 0;
		}

		write_queue++;
		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock))
	{
		PIO_STACK_LOCATION	irpSp;
		PIRP				irp;

		DebugPrint(1, ("RetransmitQueue packet deleted\n"));
		irpSp = RESERVED(packet)->IrpSp;

		if(irpSp != NULL) {
			irp = IRP_SEND_IRP(irpSp);
		    irp->IoStatus.Status = STATUS_LOCAL_DISCONNECT;
			irp->IoStatus.Information = 0;
		}

		retransmit_queue++;

		// Check Cloned Packet. 
		if(RESERVED(packet)->Cloned > 0) {
			DebugPrint(1, ("[LPX]SmpFreeServicePoint: Cloned is NOT 0. %d\n", RESERVED(packet)->Cloned));
		}

		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock))
	{
		receive_queue++;
		PacketFree(packet);
	}

	while(packet 
		= PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock))
	{
		receive_queue++;
		PacketFree(packet);
	}

	DebugPrint(1, ("back_log = %d, receive_queue = %d, receivedata_queue = %d write_queue = %d, retransmit_queue = %d\n", 
			back_log, receive_queue, receivedata_queue, write_queue, retransmit_queue));

	SmpCancelIrps(ServicePoint);

	return TRUE ;
}


static VOID
SmpCancelIrps(
	IN PSERVICE_POINT	ServicePoint
	)
{
	PIRP				irp;
	PLIST_ENTRY			thisEntry;
	PIRP				pendingIrp;
//	KIRQL				cancelIrql;
	KIRQL				oldIrql ;
	PDRIVER_CANCEL		oldCancelRoutine;


	DebugPrint(2, ("SmpCancelIrps\n"));

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;

	if(ServicePoint->ConnectIrp) {
		DebugPrint(1, ("SmpCancelIrps ConnectIrp\n"));

		irp = ServicePoint->ConnectIrp;
		ServicePoint->ConnectIrp = NULL;
		irp->IoStatus.Status = STATUS_NETWORK_UNREACHABLE; // Mod by jgahn. STATUS_CANCELLED;
	 
//		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
//		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql);
		DebugPrint(1, ("[LPX]SmpCancelIrps: Connect IRP %lx completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_NETWORK_UNREACHABLE));
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;
	}

	if(ServicePoint->ListenIrp) {
		DebugPrint(1, ("SmpCancelIrps ListenIrp\n"));

		irp = ServicePoint->ListenIrp;
		ServicePoint->ListenIrp = NULL;
		irp->IoStatus.Status = STATUS_CANCELLED;

//		IoAcquireCancelSpinLock(&cancelIrql);
		IoSetCancelRoutine(irp, NULL);
//		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql);
		DebugPrint(1, ("[LPX]SmpCancelIrps: Listen IRP %lx completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_CANCELLED));
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;
	}

	if(ServicePoint->DisconnectIrp) {
		DebugPrint(1, ("SmpCancelIrps DisconnectIrp\n"));

		irp = ServicePoint->DisconnectIrp;
		ServicePoint->DisconnectIrp = NULL;
		irp->IoStatus.Status = STATUS_CANCELLED;

//		IoAcquireCancelSpinLock(&cancelIrql);
		oldCancelRoutine = IoSetCancelRoutine(irp, NULL);
//		IoReleaseCancelSpinLock(cancelIrql);
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql);
		DebugPrint(1, ("[LPX]SmpCancelIrps: Disconnect IRP %lx completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_CANCELLED));
		IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;
	}

	RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql) ;

    //
    // Walk through the RcvList and cancel all read IRPs.
    //

	while(thisEntry=ExInterlockedRemoveHeadList(&ServicePoint->ReceiveIrpList, &ServicePoint->ReceiveIrpQSpinLock) )
	{
		pendingIrp = CONTAINING_RECORD(thisEntry, IRP, Tail.Overlay.ListEntry);

        DebugPrint(1, ("[LPX] SmpCancelIrps: Cancelled Receive IRP 0x%0x\n", pendingIrp));

        //
		//  Clear the IRP¡¯s cancel routine
        //
//		IoAcquireCancelSpinLock(&cancelIrql);
		oldCancelRoutine = IoSetCancelRoutine(pendingIrp, NULL);
//		IoReleaseCancelSpinLock(cancelIrql);

        pendingIrp->IoStatus.Information = 0;
        pendingIrp->IoStatus.Status = STATUS_CANCELLED;

		DebugPrint(1, ("[LPX]SmpCancelIrps: IRP %lx completed with error: NTSTATUS:%08lx.\n ", irp, STATUS_CANCELLED));
	    IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
    }

	return;
}


static NTSTATUS
TransmitPacket(
   IN	PDEVICE_CONTEXT	DeviceContext,
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN PACKET_TYPE		PacketType,
	IN USHORT			UserDataLength
	)
{
	PSMP_CONTEXT	smpContext	= &ServicePoint->SmpContext;
	PUCHAR			packetData;
	PLPX_HEADER2	lpxHeader;
	NTSTATUS		status;
	UCHAR			raised = 0;
	PNDIS_BUFFER	firstBuffer;	
	USHORT			sequence ;
	USHORT			finsequence ;
	PTP_ADDRESS		address ;
	UINT			TotalPacketLength;
	UINT			BufferCount;

	DebugPrint(3, ("TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));

	ASSERT(ServicePoint->Address);

//
//	we don't need to make sure that Address field is not NULL
//	because we have a reference to Connection before calling
//	hootch 09012003

	address = ServicePoint->Address ;
//	if(address == NULL) {
//			return STATUS_INSUFFICIENT_RESOURCES;
//	}

	ASSERT(address->Provider);

	if(Packet == NULL) {
		DebugPrint(3, ("before TransmitPacket size = 0x%x \n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
		status = PacketAllocate(
			ServicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			address->Provider,
			SEND_TYPE,
			NULL,
			0,
			NULL,
			&Packet
			);
		DebugPrint(3, ("after TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
		if(!NT_SUCCESS(status)) {
			DebugPrint(1, ("[LPX]TransmitPacket: packet == NULL\n"));
			SmpPrintState(1, "[LPX]TransmitPacket: PacketAlloc", ServicePoint);
			return status;
		}
	}
	
	DebugPrint(3, ("after TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
	//
	//	Initialize packet header.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		&BufferCount,
		&firstBuffer,
		&TotalPacketLength
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if(PacketType == DATA)
		lpxHeader->PacketSize = HTONS((USHORT)(SMP_SYS_PKT_LEN + UserDataLength));
	else
		lpxHeader->PacketSize = HTONS(SMP_SYS_PKT_LEN);

	lpxHeader->LpxType					= LPX_TYPE_STREAM;
	lpxHeader->DestinationPort		= ServicePoint->DestinationAddress.Port;
	lpxHeader->SourcePort			= ServicePoint->SourceAddress.Port;
	lpxHeader->AckSequence			= HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));
	lpxHeader->ServerTag			= smpContext->ServerTag;

	switch(PacketType) {

	case CONREQ:
	case DATA:
	case DISCON:
		sequence = (USHORT)InterlockedIncrement(&smpContext->Sequence) ;
		sequence -- ;

		if(PacketType == CONREQ) {
			lpxHeader->Lsctl = HTONS(LSCTL_CONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DATA) {
			lpxHeader->Lsctl = HTONS(LSCTL_DATA | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DISCON) {
			finsequence = (USHORT)InterlockedIncrement(&smpContext->FinSequence) ;
			finsequence -- ;

			lpxHeader->Lsctl = HTONS(LSCTL_DISCONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(finsequence);
		}

		DebugPrint(3, ("CONREQ DATA DISCON : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		if(!SmpSendTest(smpContext, Packet)) {
			DebugPrint(0, ("[LPX] TransmitPacket: insert WriteQueue!!!!\n"));
			ExInterlockedInsertTailList(
									&smpContext->WriteQueue,
									&RESERVED(Packet)->ListElement,
									&smpContext->WriteQSpinLock
									);
			return STATUS_SUCCESS;
		}
		
		//
		//	added by hootch 09072003
		//	give the packet to SmpWorkDPC routine
		//
		//
		// NOTE:The packet type is restricted to DATA for now.
		//
		if(PacketType == DATA) {

//////////////////////////////////////////////////////////////////////////
//
//	Add padding to fix Under-60byte bug of NDAS chip 2.0.
//
			if(	
				TotalPacketLength >= ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2) + 4 &&
				TotalPacketLength <= 56
			) {

				LONG			PaddingLen = 60 - TotalPacketLength;
				PUCHAR			PaddingData;
				PNDIS_BUFFER	PaddingBuffDesc;
				PUCHAR			LastData;
				UINT			LastDataLen;
				PNDIS_BUFFER	LastBuffDesc;
				PNDIS_BUFFER	CurrentBuffDesc;

				DebugPrint(1, ("[LpxSmp]TransmitPacket: Adding padding to support NDAS chip 2.0\n"));
				//
				//	Allocate memory for padding.
				//
				status = NdisAllocateMemory(
						&PaddingData,
						PaddingLen
					);
				if(status != NDIS_STATUS_SUCCESS) {
					DebugPrint(1, ("[LpxSmp]TransmitPacket: Can't Allocate Memory packet.\n"));
					return status;
				}
				RtlZeroMemory(PaddingData, PaddingLen);

				//
				//	Copy the last 4 bytes to the end of packet.
				//
				CurrentBuffDesc = firstBuffer;
				do {
					LastBuffDesc = CurrentBuffDesc;
					NdisGetNextBuffer(LastBuffDesc, &CurrentBuffDesc);
				} while(CurrentBuffDesc);

				NdisQueryBufferSafe(
							LastBuffDesc,
							&LastData,
							&LastDataLen,
							HighPagePriority
				);
				if(!LastData) {
					NdisFreeMemory(PaddingData);
					DebugPrint(1, ("[LPX]TransmitPacket: Can't NdisQueryBufferSafe()!!!\n"));
					return STATUS_INSUFFICIENT_RESOURCES;
				}

				if(LastDataLen >= 4 && PaddingLen >= 4) {
					RtlCopyMemory(PaddingData + PaddingLen - 4, LastData + LastDataLen - 4, 4);
				} else {
					DebugPrint(1, ("[LPX]TransmitPacket: Data length too small. LastDataLen:%d PaddingLen:%d!!!\n",
												LastDataLen, PaddingLen));
					ASSERT(FALSE);
				}

				//
				//	Create the Ndis buffer desc from header memory for LPX packet header.
				//
				NdisAllocateBuffer(
						&status,
						&PaddingBuffDesc,
						DeviceContext->LpxBufferPool,
						PaddingData,
						PaddingLen
					);
				if(!NT_SUCCESS(status)) {
					NdisFreeMemory(PaddingData);
					DebugPrint(1, ("[LPX]TransmitPacket: Can't Allocate Buffer!!!\n"));
					return status;
				}

				NdisChainBufferAtBack(Packet, PaddingBuffDesc);
			}
//
//	End of padding routine.
//
//////////////////////////////////////////////////////////////////////////

			RoutePacketRequest(smpContext, Packet, PacketType);
			return STATUS_SUCCESS ;
		}

		break;

	case ACK :

		DebugPrint(4, ("ACK : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(smpContext->Sequence));

		break;

	case ACKREQ :

		DebugPrint(4, ("ACKREQ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(smpContext->Sequence));

		break;

	default:

		DebugPrint(0, ("[LPX] TransmitPacket: STATUS_NOT_SUPPORTED\n"));

		return STATUS_NOT_SUPPORTED;
	}

	RoutePacket(smpContext, Packet, PacketType);

	return STATUS_SUCCESS;
}

//	
//	Introduced to avoid the dead lock between ServicePoint and Address
//	when packets take a path of loop back.
//	It requires the connection reference count larger than one.
//
//	NOTE: Do not call this function with DATA packet type.
//
//	hootch 02062004
//
//	STACK_TEXT:  
//	f88dac2c 8063a85a 000000c4 00001001 83242010 nt!KeBugCheckEx+0x19
//	f88dac48 8063b1a4 00001001 83242010 824e3cf8 nt!ViDeadlockReportIssue+0x55
//	f88dac70 8063b78a 83242010 014e3cf8 00000001 nt!ViDeadlockAnalyze+0x188
//	f88daccc 80635144 83242010 00000004 82526cd0 nt!VfDeadlockAcquireResource+0x19a
//	f88dacf0 f1270ef6 82908ad8 828834a0 8243fe98 nt!VerifierKfAcquireSpinLock+0x66
//	f88dad68 f1261181 024de030 82908ad8 8243fe98 lpx!LpxReceiveComplete+0x132 [d:\workspc\ndfs\lanscsisystemv2\src\lpx\lpx.c @ 1930]
//	f88dad90 baea1695 824de030 c000009a 82908ad8 lpx!NbfReceiveComplete+0x11 [d:\workspc\ndfs\lanscsisystemv2\src\lpx\dlc.c @ 2823]
//	f88dadec bac32ddd 015e8d9c f88dae10 00000001 NDIS!ethFilterDprIndicateReceivePacket+0x586
//	f88dae18 baea19ea 0188b690 828834c0 82883550 psched!ClReceiveIndication+0x95
//	f88dae8c bae9582b c000009a f88daeac 00000001 NDIS!ethFilterDprIndicateReceivePacket+0x1dd
//	=> f88daeb4 bae829f2 00000000 00000000 8290cd60 NDIS!ndisMLoopbackPacketX+0x150
//	f88daec8 bac341bf 8290cd60 82a0e9b8 00000103 NDIS!ndisMSendX+0xd2
//	f88daefc bae82922 8288b690 82a0e9b8 00000000 psched!MpSend+0xab1
//	f88daf1c f1271492 825a35d8 82a0e9b8 83574040 NDIS!ndisMSendX+0x125
//	f88daf40 f1271718 02000002 8357403c 83574000 lpx!RoutePacket+0xcc [d:\workspc\ndfs\lanscsisystemv2\src\lpx\lpx.c @ 3562]
//	f88daf58 f1271df9 82a0e9b8 00000002 00000000 lpx!TransmitPacket+0xc0 [d:\workspc\ndfs\lanscsisystemv2\src\lpx\lpx.c @ 3380]
//	f88dafa8 f1272844 82a0ed78 806341b2 f87479c0 lpx!SmpDoReceive+0x63d [d:\workspc\ndfs\lanscsisystemv2\src\lpx\lpx.c @ 4118]
//	f88dafcc 80534b9f 83574128 00574040 00000000 lpx!SmpWorkDpcRoutine+0xac [d:\workspc\ndfs\lanscsisystemv2\src\lpx\lpx.c @ 4819]
//	f88daff4 8053470b f1db3afc 00000000 00000000 nt!KiRetireDpcList+0x61
//
// Thread 0: A B 
// Thread 1: B C D 
// Thread 2: D E 
// Thread 3: E A 
// 
// Where:
// Thread 0 = 00000000
// Thread 1 = 00000000
// Thread 2 = 00000000
// Thread 3 = 822f4788
// Lock A =   83242010 Type 'Spinlock' Address
// Lock B =   8351817c Type 'Spinlock' 
// Lock C =   83518000 Type 'Spinlock'
// Lock D =   8357417c Type 'Spinlock' Connection 
// Lock E =   83574000 Type 'Spinlock' ServicePoint
//
static NTSTATUS
TransmitPacket_AvoidAddrSvcDeadLock(
	IN PSERVICE_POINT	ServicePoint,
	IN PNDIS_PACKET		Packet,
	IN PACKET_TYPE		PacketType,
	IN USHORT			UserDataLength,
	IN PKSPIN_LOCK		ServicePointSpinLock,
	IN KIRQL			ServiceIrql
	)
{
	PSMP_CONTEXT	smpContext	= &ServicePoint->SmpContext;
	PUCHAR			packetData;
	PLPX_HEADER2	lpxHeader;
	NTSTATUS		status;
	UCHAR			raised = 0;
	PNDIS_BUFFER	firstBuffer;	
	KIRQL			oldIrql ;
	USHORT			sequence ;
	USHORT			finsequence ;
	PTP_ADDRESS		address ;

	DebugPrint(3, ("TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));

	ASSERT(ServicePoint->Address);
	ASSERT(ServicePointSpinLock) ;

//
//	we don't need to make sure that Address field is not NULL
//	because we have a reference to Connection before calling
//	hootch 09012003

	address = ServicePoint->Address ;
//	if(address == NULL) {
//			return STATUS_INSUFFICIENT_RESOURCES;
//	}

	ASSERT(address->Provider);

//	if(ServicePoint->SmpState == SMP_SYN_RECV)
//		return STATUS_INSUFFICIENT_RESOURCES;
	if(Packet == NULL) {
		DebugPrint(3, ("before TransmitPacket size = 0x%x \n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
		status = PacketAllocate(
			ServicePoint,
			ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2),
			address->Provider,
			SEND_TYPE,
			NULL,
			0,
			NULL,
			&Packet
			);
		DebugPrint(3, ("after TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
		if(!NT_SUCCESS(status)) {
			DebugPrint(1, ("[LPX]TransmitPacket: packet == NULL\n"));
			SmpPrintState(1, "[LPX]TransmitPacket: PacketAlloc", ServicePoint);
			return status;
		}
	}
	
	DebugPrint(3, ("after TransmitPacket size = 0x%x\n", ETHERNET_HEADER_LENGTH + sizeof(LPX_HEADER2)));
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if(PacketType == DATA)
		lpxHeader->PacketSize = HTONS((USHORT)(SMP_SYS_PKT_LEN + UserDataLength));
	else
		lpxHeader->PacketSize = HTONS(SMP_SYS_PKT_LEN);

	lpxHeader->LpxType					= LPX_TYPE_STREAM;
//	{
//		USHORT	dataLength;
//		DebugPrint(2, ("dataLength = 0x%x\n", lpxHeader->PacketSize));
//		dataLength = (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK)) - sizeof(LPX_HEADER2);
//		DebugPrint(2, ("dataLength = 0x%x\n", dataLength));
//	}
//	lpxHeader->CheckSum				= 0;
	lpxHeader->DestinationPort		= ServicePoint->DestinationAddress.Port;
	lpxHeader->SourcePort			= ServicePoint->SourceAddress.Port;

	lpxHeader->AckSequence			= HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));
//	lpxHeader->SourceId				= smpContext->SourceId;
//	lpxHeader->DestinationId			= smpContext->DestionationId;
//	lpxHeader->AllocateSequence		= HTONS(smpContext->Allocate);
	lpxHeader->ServerTag			= smpContext->ServerTag;

	//
	//	To avoid the dead lock, release the lock.
	//	It requires the connection reference count larger than one.
	//
	ASSERT(DATA != PacketType) ;
	RELEASE_SPIN_LOCK(ServicePointSpinLock, ServiceIrql) ;

	switch(PacketType) {

	case CONREQ:
	case DISCON:
		sequence = (USHORT)InterlockedIncrement(&smpContext->Sequence) ;
		sequence -- ;

		if(PacketType == CONREQ) {
			lpxHeader->Lsctl = HTONS(LSCTL_CONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DATA) {
			lpxHeader->Lsctl = HTONS(LSCTL_DATA | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(sequence);
		} else if(PacketType == DISCON) {
			finsequence = (USHORT)InterlockedIncrement(&smpContext->FinSequence) ;
			finsequence -- ;

			lpxHeader->Lsctl = HTONS(LSCTL_DISCONNREQ | LSCTL_ACK);
			lpxHeader->Sequence = HTONS(finsequence);
		}

		DebugPrint(2, ("CONREQ DISCON : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		if(!SmpSendTest(smpContext, Packet)) {
			DebugPrint(0, ("[LPX] TransmitPacket: insert WriteQueue!!!!\n"));

			ACQUIRE_SPIN_LOCK(ServicePointSpinLock, &oldIrql) ;

			ExInterlockedInsertTailList(
									&smpContext->WriteQueue,
									&RESERVED(Packet)->ListElement,
									&smpContext->WriteQSpinLock
									);
			return STATUS_SUCCESS;
		}
		
		break;

	case ACK :

		DebugPrint(4, ("ACK : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(smpContext->Sequence));

		break;

	case ACKREQ :

		DebugPrint(4, ("ACKREQ : Sequence 0x%x, ACk 0x%x\n", NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence)));

		lpxHeader->Lsctl = HTONS(LSCTL_ACKREQ | LSCTL_ACK);
		lpxHeader->Sequence = HTONS(SHORT_SEQNUM(smpContext->Sequence));

		break;

	default:

		ACQUIRE_SPIN_LOCK(ServicePointSpinLock, &oldIrql) ;

		DebugPrint(0, ("[LPX] TransmitPacket: STATUS_NOT_SUPPORTED\n"));

		return STATUS_NOT_SUPPORTED;
	}

	RoutePacket(smpContext, Packet, PacketType);

	ACQUIRE_SPIN_LOCK(ServicePointSpinLock, &oldIrql) ;

	return STATUS_SUCCESS;
}


VOID
ProcessSentPacket(
				  IN PNDIS_PACKET	Packet
				  )
{
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;
//	PSMP_CONTEXT		SmpContext;
	PUCHAR				packetData;
//	KIRQL				oldIrql ;

	// Added by jgahn.
	
	//
	// Calc Timeout...
	//
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
		);
	   
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	switch(NTOHS(lpxHeader->Lsctl)) {
	case LSCTL_CONNREQ | LSCTL_ACK:
	case LSCTL_DATA | LSCTL_ACK:
	case LSCTL_DISCONNREQ | LSCTL_ACK:

//	SmpContext = (PSMP_CONTEXT)(RESERVED(Packet)->pSmpContext);


//	OBSOULTE: LatestSendTime
//	removed by hootch 09062003
		// Calc Timeout...
//		if(SmpContext != NULL) {
//			ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;
//			SmpContext->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CalculateRTT(SmpContext);

//			SmpContext->LatestSendTime.QuadPart = CurrentTime().QuadPart;
//			RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;
//		}

		break;
	default:
		// Nothing to do..
		break;
	}

	PacketFree(Packet);
}

//
//	give a request to SmpWork DPC
//
static void
RoutePacketRequest(
	IN PSMP_CONTEXT	smpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	) {
	LONG	cnt ;
	KIRQL				oldIrql;
	BOOLEAN				raised = FALSE;



    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}
	//
	// The packet type is restricted to DATA for now.
	//
	if(PacketType != DATA) {
		DebugPrint(0, ("[LPX] RoutePacketRequest: bad PacketType for SmpWorkDPC\n")) ;
		return ;
	}

	ExInterlockedInsertTailList(&smpContext->SendQueue,
		&RESERVED(Packet)->ListElement,
		&smpContext->SendQSpinLock
		);

	cnt = InterlockedIncrement(&smpContext->RequestCnt) ;
	if( cnt == 1 ) {
		KeInsertQueueDpc(&smpContext->SmpWorkDpc, NULL, NULL) ;
//		KeInsertQueueDpc(&SocketLpxDeviceContext->LpxWorkDpc, smpContext, NULL) ;
	}

	if(raised == TRUE)
		KeLowerIrql(oldIrql);

}


static NTSTATUS
RoutePacket(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet,
	IN PACKET_TYPE	PacketType
	)
{
	PSERVICE_POINT	servicePoint = SmpContext->ServicePoint;
	PNDIS_PACKET	packet2;
	NTSTATUS		status;
	PDEVICE_CONTEXT	deviceContext;
	KIRQL			oldIrql ;
	PTP_ADDRESS		address ;
#ifdef DBG
//	LARGE_INTEGER	ticks;
#endif

	DebugPrint(3, ("RoutePacket\n"));


	switch(PacketType) {

	case CONREQ :
	case DATA :
	case DISCON :

		RESERVED(Packet)->pSmpContext = SmpContext;

		packet2 = PacketClone(Packet);


		ExInterlockedInsertTailList(
								&SmpContext->RetransmitQueue,
								&RESERVED(packet2)->ListElement,
								&SmpContext->RetransmitQSpinLock
								);

		ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;
//		if(IsListEmpty(&SmpContext->RetransmitQueue)) {
			SmpContext->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CalculateRTT(SmpContext);
//		}
		// Update End retransmit time
		SmpContext->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)MAX_RETRANSMIT_TIME);
		RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;

	case RETRAN:

#if 0
		if(ulPacketDropRate > (ULONG)(++iTicks % 1000)) {
			DebugPrint(1, ("[LPX]DROP PACKET FOR DEBUGGING!!!!!\n"));
			ProcessSentPacket(Packet);
			InterlockedIncrement( &NumberOfSentComplete );
			return 	NDIS_STATUS_SUCCESS;
		}
		
#endif

//	OBSOULTE: LatestSendTime
//	removed by hootch 09062003
//
//		ACQUIRE_SPIN_LOCK(&SmpContext->ServicePoint->SpinLock, &oldIrql) ;
//		SmpContext->LatestSendTime.QuadPart = CurrentTime().QuadPart;
//		RELEASE_SPIN_LOCK(&SmpContext->ServicePoint->SpinLock, oldIrql) ;

	case ACK:
	case ACKREQ:
	default:
		address = servicePoint->Address ;
		if(!address) {
			PacketFree(Packet);
			return STATUS_NOT_SUPPORTED;
		}

		deviceContext = address->Provider;

		if((deviceContext->bDeviceInit == TRUE)  && (deviceContext->NdisBindingHandle)) {
			NdisSend(
				&status,
				deviceContext->NdisBindingHandle,
				Packet
				);
		} else
            status = STATUS_INVALID_DEVICE_STATE;

		switch(status) {
		case NDIS_STATUS_SUCCESS:
			DebugPrint(2, ("[LPX]RoutePacket: NdisSend return Success\n"));
			ProcessSentPacket(Packet);
			InterlockedIncrement( &NumberOfSentComplete );

		case NDIS_STATUS_PENDING:
			InterlockedIncrement(&NumberOfSent);
			break;

		default:
			DebugPrint(1, ("[LPX]RoutePacket: Error when NdisSend. status: 0x%x\n", status));
			PacketFree(Packet);
		}
	}

	return status;
}


VOID
LpxSendComplete(
				IN NDIS_HANDLE	ProtocolBindingContext,
				IN PNDIS_PACKET	Packet,
				IN NDIS_STATUS	Status
				)
{
	DebugPrint(3, ("LpxSendComplete\n"));

	UNREFERENCED_PARAMETER(ProtocolBindingContext) ;
	UNREFERENCED_PARAMETER(Status) ;

	ProcessSentPacket(Packet);

	InterlockedIncrement( &NumberOfSentComplete );

	return;
}

static BOOLEAN
SmpSendTest(
			PSMP_CONTEXT	SmpContext,
			PNDIS_PACKET	Packet
			)
{
	LONG				inFlight;
	PCHAR				packetData;
	PLPX_HEADER2		lpxHeader;
	PNDIS_BUFFER		firstBuffer;	
//	KIRQL				oldIrql ;

	NdisQueryPacket(
				Packet,
				NULL,
				NULL,
				&firstBuffer,
				NULL
				);
	packetData = MmGetMdlVirtualAddress(firstBuffer);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	inFlight = NTOHS(SHORT_SEQNUM(lpxHeader->Sequence)) - SHORT_SEQNUM(SmpContext->RemoteAck);

	DebugPrint(4, ("lpxHeader->Sequence = %x, SmpContext->RemoteAck = %x\n",
		NTOHS(lpxHeader->Sequence), SHORT_SEQNUM(SmpContext->RemoteAck)));
	if(inFlight >= SmpContext->MaxFlights) {
		SmpPrintState(1, "Flight overflow", SmpContext->ServicePoint);
		return 0;
	}

	return 1;	
}


static LONGLONG
CalculateRTT(
	IN	PSMP_CONTEXT	SmpContext
	)
{
	/*
	if(SmpContext->Retransmits == 0) {
		return SmpContext->IntervalTime.QuadPart * 10;
	} else if(SmpContext->Retransmits < 10) {
		return SmpContext->IntervalTime.QuadPart * SmpContext->Retransmits * 1000;
	} else
		return MAX_RETRANSMIT_DELAY;
		*/
	LARGE_INTEGER	Rtime;
	int				i;

	Rtime.QuadPart = RETRANSMIT_TIME; //(SmpContext->IntervalTime.QuadPart);

	// Exponential
	for(i = 0; i < SmpContext->Retransmits; i++) {
		Rtime.QuadPart *= 2;
		
		if(Rtime.QuadPart > MAX_RETRANSMIT_DELAY)
			return MAX_RETRANSMIT_DELAY;
	}

	if(Rtime.QuadPart > MAX_RETRANSMIT_DELAY)
		Rtime.QuadPart = MAX_RETRANSMIT_DELAY;

	return Rtime.QuadPart;
}

//
//
//
//	called only from SmpDpcWork()
//
static NTSTATUS
SmpReadPacket(
	IN 	PIRP			Irp,
	IN 	PSERVICE_POINT	ServicePoint
	)
{
	PNDIS_PACKET		packet;
	PSMP_CONTEXT		smpContext = &ServicePoint->SmpContext;
	PIO_STACK_LOCATION	irpSp;
	ULONG				userDataLength;
	ULONG				irpCopied;
	ULONG				remained;
	PUCHAR				userData = NULL;
	PLPX_HEADER2		lpxHeader;

	PNDIS_BUFFER		firstBuffer;
	PUCHAR				bufferData;
	UINT				bufferLength;
	UINT				copied;
//	KIRQL				cancelIrql;
	KIRQL				oldIrql ;

	DebugPrint(4, ("SmpReadPacket\n"));

	do {
		// Mod by jgahn. See line 2206...
		//
		//packet = PacketDequeue(&smpContext->RcvDataQueue, &ServicePoint->ServicePointQSpinLock);
		// New
		packet = PacketPeek(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock);

		if(packet == NULL) {
			DebugPrint(4, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p Returned\n", ServicePoint, Irp));
			ExInterlockedInsertHeadList(
									&ServicePoint->ReceiveIrpList,
									&Irp->Tail.Overlay.ListEntry,
									&ServicePoint->ReceiveIrpQSpinLock
									);
			return STATUS_PENDING;
		}

		lpxHeader = (PLPX_HEADER2)RESERVED(packet)->LpxSmpHeader;
		if(NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
			ServicePoint->Shutdown |= SMP_RECEIVE_SHUTDOWN;	

			packet = PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock);
			PacketFree(packet);
			break;
		}

		irpSp = IoGetCurrentIrpStackLocation(Irp);
		userDataLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength; 
		irpCopied = Irp->IoStatus.Information;
		remained = userDataLength - irpCopied;

		userData = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);
		userData += irpCopied;
		
		NdisQueryPacket(
				packet,
				NULL,
				NULL,
				&firstBuffer,
				NULL
				);

		bufferData = MmGetMdlVirtualAddress(firstBuffer);
		bufferLength = NdisBufferLength(firstBuffer);
		bufferLength -= RESERVED(packet)->PacketDataOffset;
		bufferData += RESERVED(packet)->PacketDataOffset;
		copied = (bufferLength < remained) ? bufferLength : remained;
		RESERVED(packet)->PacketDataOffset += copied;

		if(copied)
			RtlCopyMemory(
				userData,
				bufferData,
				copied
			);

		Irp->IoStatus.Information += copied;
		if(RESERVED(packet)->PacketDataOffset == NdisBufferLength(firstBuffer)) {
			// Add by jgahn.
			packet = PacketDequeue(&smpContext->RcvDataQueue, &smpContext->RcvDataQSpinLock);
			PacketFree(packet);
		}

		DebugPrint(4, ("userDataLength = %d, copied = %d, Irp->IoStatus.Information = %d\n",
				userDataLength, copied, Irp->IoStatus.Information));

	} while(userDataLength > Irp->IoStatus.Information);

	DebugPrint(4, ("SmpReadPacket IRP\n"));

  //  IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(Irp, NULL);
//	IoReleaseCancelSpinLock(cancelIrql);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DebugPrint(2, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p completed.\n", ServicePoint, Irp));

	//
	//	check to see if shutdown is in progress.
	//
	//	added by hootch 09042003
	//

	ACQUIRE_SPIN_LOCK(&ServicePoint->SpinLock, &oldIrql) ;

	if(ServicePoint->Shutdown & SMP_RECEIVE_SHUTDOWN) {
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql) ;

		DebugPrint(1, ("[LPX] SmpReadPacket: ServicePoint:%p IRP:%p canceled\n", ServicePoint, Irp));

		LpxCallUserReceiveHandler(ServicePoint);
		SmpCancelIrps(ServicePoint);
	}
	else {
		RELEASE_SPIN_LOCK(&ServicePoint->SpinLock, oldIrql) ;
	}

	return STATUS_SUCCESS;
}




//
//	request the Smp DPC routine for the packet received
//
//
//	added by hootch 09092003
static VOID
SmpDoReceiveRequest(
	IN PSMP_CONTEXT	smpContext,
	IN PNDIS_PACKET	Packet
	) {
	LONG	cnt ;
	KIRQL				oldIrql;
	BOOLEAN				raised = FALSE;



    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}

	ExInterlockedInsertTailList(&smpContext->ReceiveQueue,
		&RESERVED(Packet)->ListElement,
		&smpContext->ReceiveQSpinLock
		);
	cnt = InterlockedIncrement(&smpContext->RequestCnt) ;
	if( cnt == 1 ) {
		KeInsertQueueDpc(&smpContext->SmpWorkDpc, NULL, NULL) ;
//		KeInsertQueueDpc(&SocketLpxDeviceContext->LpxWorkDpc, smpContext, NULL) ;
	}

	if(raised == TRUE)
		KeLowerIrql(oldIrql);
}

//
//
//	called only from LpxReceiveComplete
//
static BOOLEAN
SmpDoReceive(
	IN PSMP_CONTEXT	SmpContext,
	IN PNDIS_PACKET	Packet
	)
{
	PLPX_HEADER2		lpxHeader;
//	NTSTATUS			status;
	KIRQL				oldIrql;
	PSERVICE_POINT		servicePoint;
//	PLIST_ENTRY			irpListEntry;
//	PIRP				irp;
//	PNDIS_BUFFER		firstBuffer;	
	UCHAR				dataArrived = 0;
	UCHAR				packetConsumed = 0;
//	KIRQL				cancelIrql;
#if DBG
//	LARGE_INTEGER		ticks;
#endif
	LARGE_INTEGER		TimeInterval = {0,0};
	DebugPrint(3, ("SmpDoReceive\n"));
	lpxHeader = (PLPX_HEADER2) RESERVED(Packet)->LpxSmpHeader;
	servicePoint = SmpContext->ServicePoint;

	//
	// DROP PACKET for DEBUGGING!!!!
	//
#if DBG
	/*
	KeQueryTickCount(&ticks);
	
	if(ulPacketDropRate > (ULONG)(ticks.u.LowPart % 100)) {
		
		DebugPrint(2, ("[LPX]DROP PACKET FOR DEBUGGING!!!!!\n"));
		
		PacketFree(Packet);
		
		return;
	}
	*/
#if 0
	if(ulPacketDropRate > (ULONG)(++iTicks % 1000)) {
		
		DebugPrint(1, ("[LPX]DROP PACKET FOR DEBUGGING!!!!!\n"));
		
		PacketFree(Packet);

		return FALSE;
	}
#endif

#endif
	ACQUIRE_SPIN_LOCK(&servicePoint->SpinLock, &oldIrql) ;

/*
	if(NTOHS(lpxHeader->Sequence) != SHORT_SEQNUM(SmpContext->RemoteSequence)) {
		if(!((NTOHS(lpxHeader->Lsctl) == LSCTL_ACK 
			 && ((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(SmpContext->RemoteSequence)) > 0))
			|| NTOHS(lpxHeader->Lsctl) & LSCTL_DATA)) 
		{
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet\n"));
			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;
			PacketFree(Packet);
			return FALSE;
		}
	}
*/


	if(NTOHS(lpxHeader->Sequence) != SHORT_SEQNUM(SmpContext->RemoteSequence)) {
		if(!(  ( ( NTOHS(lpxHeader->Lsctl) == LSCTL_ACK )  
			 && ( (SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(SmpContext->RemoteSequence)) > 0 ) )   
			|| ( NTOHS(lpxHeader->Lsctl) & LSCTL_DATA )   ))    
		{
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet\n"));
			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;
			PacketFree(Packet);
			return FALSE;
		}
	}

	
	if( (SHORT)( NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(SmpContext->RemoteSequence) ) > MAX_ALLOWED_SEQ )
	{
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet\n"));
			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;
			PacketFree(Packet);
			return FALSE;
	}

	if( NTOHS(lpxHeader->Lsctl) & LSCTL_ACK ) {
		if(((SHORT)(SHORT_SEQNUM(SmpContext->RemoteAck) - NTOHS(lpxHeader->AckSequence))) > MAX_ALLOWED_SEQ){
			//
			//	Who was missing release lock. -_-;
			//	patched by hootch 02252004
			//
			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;
			ASSERT(FALSE);
				//DbgBreakPoint ();
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. Drop packet\n"));
			PacketFree(Packet);
			return FALSE;
		}
	}
	/*
	// Version 2
	if(((SHORT)(NTOHS(lpxHeader->AckSequence) - SmpContext->Sequence) < 0) &&
		((SHORT)(NTOHS(lpxHeader->Sequence) - SmpContext->RemoteSequence) < 0))
	{
		DebugPrint(1, 
			("[LPX]SmpDoReceive: Already Received packet... Drop. HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x\n", 
			NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence));
		PacketFree(Packet);
		return;
	}

	if(NTOHS(lpxHeader->Sequence) != SmpContext->RemoteSequence) {
		
		if((NTOHS(lpxHeader->AckSequence) >= SmpContext->Sequence)
			&& (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)) {
			DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. But has Ack process ACK\n"));
			
		} else {
			DebugPrint(1, 
				("[LPX]SmpDoReceive: Bad ACK Number Packet Loss... Drop. HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x\n", 
				NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence
				));
			
			PacketFree(Packet);
			return;
		}
	}
	*/


	// Version 3
	//
	//	caution: use SHORT_SEQNUM() to compare lpxHeader->SequneceXXX
	//					with LONG type sequence number such as RemoteSequnce and Sequence
	//
/*
	if(NTOHS(lpxHeader->Sequence) != SHORT_SEQNUM(SmpContext->RemoteSequence)) {
		
		//
		// Transmit ACK
		//
		TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);

		if(( NTOHS(lpxHeader->Sequence) > SHORT_SEQNUM(SmpContext->RemoteSequence) + SMP_MAX_FLIGHT / 2 )
			|| ( NTOHS(lpxHeader->Sequence) < (SHORT_SEQNUM(SmpContext->RemoteSequence) - SMP_MAX_FLIGHT / 2) )) {
			DebugPrint(2, 
				("[LPX]SmpDoReceive: Bad Seq Number. !!! Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence,
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));
		} else {
			DebugPrint(2, 
				("[LPX]SmpDoReceive: Seq# Mismatch. Maybe RT Packet. Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), SmpContext->Sequence, SmpContext->RemoteSequence,
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));
		}
//
//		 Has Ack?
//		if((servicePoint->SmpState == SMP_ESTABLISHED)
//			&& (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK)
//			&& (NTOHS(lpxHeader->AckSequence) > SmpContext->RemoteAck)) {
//
//			if((NTOHS(lpxHeader->AckSequence) <= SmpContext->Sequence)) {
//
//				DebugPrint(1, ("[LPX]SmpDoReceive: bad ACK number. But has advanced Ack. Process ACK\n"));
//				
//				SmpContext->RemoteAck = NTOHS(lpxHeader->AckSequence);
//				SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);	
//			} else {
//				DebugPrint(1, ("[LPX]SmpDoReceive: BAD ACK!!!!!!!!!!!!!!! S : 0x%x, HA :0x%x\n",
//					SmpContext->Sequence, NTOHS(lpxHeader->AckSequence)));
//			}
//		}

		RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

		PacketFree(Packet);

		return FALSE ;
	}
*/

/*	// Receive Ack for Unsent packets.
	if((SHORT(SHORT_SEQNUM(SmpContext->Sequence) >= SHORT_SEQNUM(SmpContext->RemoteAck)) {
		if((NTOHS(lpxHeader->AckSequence) > SHORT_SEQNUM(SmpContext->Sequence))
			|| (NTOHS(lpxHeader->AckSequence) < SHORT_SEQNUM(SmpContext->RemoteAck))) {
			DebugPrint(1, 
				("[LPX]SmpDoReceive: Bad ACK Number. Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x, RA: 0x%x, LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), 
				SHORT_SEQNUM(SmpContext->Sequence), SHORT_SEQNUM(SmpContext->RemoteSequence),
				SHORT_SEQNUM(SmpContext->RemoteAck),
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));

			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

			PacketFree(Packet);


			return FALSE ;
		}
	} else {
		// Wrapping...
		if(((NTOHS(lpxHeader->AckSequence) > SHORT_SEQNUM(SmpContext->Sequence)) &&
			(NTOHS(lpxHeader->AckSequence) < SHORT_SEQNUM(SmpContext->RemoteAck)))
			) {
			DebugPrint(1, 
				("[LPX]SmpDoReceive: Bad ACK Number. Drop. SP : 0x%x HS: 0x%x HA: 0x%x, S: 0x%x, RS: 0x%x, RA: 0x%x, LSCTR 0x%x size %d\n", 
				servicePoint, NTOHS(lpxHeader->Sequence), NTOHS(lpxHeader->AckSequence), 
				SHORT_SEQNUM(SmpContext->Sequence), SHORT_SEQNUM(SmpContext->RemoteSequence),
				SHORT_SEQNUM(SmpContext->RemoteAck),
				NTOHS(lpxHeader->Lsctl), (NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK))
				));

			RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

			PacketFree(Packet);

			return FALSE ;
		}
	}
*/
	DebugPrint(4, ("SmpDoReceive NTOHS(lpxHeader->Sequence) = 0x%x, lpxHeader->Lsctl = 0x%x\n",
		NTOHS(lpxHeader->Sequence), lpxHeader->Lsctl));

	// Since receiving Vaild Packet, Reset AliveTimeOut.
	InterlockedExchange(&SmpContext->AliveRetries, 0) ;

	ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;
	SmpContext->AliveTimeOut.QuadPart = CurrentTime().QuadPart + ALIVE_INTERVAL;
	RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;

	switch(servicePoint->SmpState) {

	case SMP_CLOSE:
		break;

	case SMP_LISTEN:

		switch(NTOHS(lpxHeader->Lsctl)) 
		{

		case LSCTL_CONNREQ:
		case LSCTL_CONNREQ | LSCTL_ACK:
		
			DebugPrint(1, ("LpxDoReceive SMP_LISTEN CONREQ\n"));

			if(servicePoint->ListenIrp == NULL) {
				break;
			}

			InterlockedIncrement(&SmpContext->RemoteSequence);
	
			RtlCopyMemory(
				servicePoint->DestinationAddress.Node,
				RESERVED(Packet)->EthernetHeader.SourceAddress,
				ETHERNET_ADDRESS_LENGTH
				);
			servicePoint->DestinationAddress.Port = lpxHeader->SourcePort;
			servicePoint->SmpState = SMP_SYN_RECV;


			//
			// BUG BUG BUG!!! Assign Server Tag. But not implemented, So ServerTag is 0.
			// 
			// servicePoint->SmpContext.ServerTag = ;

			TransmitPacket_AvoidAddrSvcDeadLock(servicePoint, NULL, CONREQ, 0, &servicePoint->SpinLock, oldIrql);

			ACQUIRE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;
			servicePoint->SmpContext.ConnectTimeOut.QuadPart = CurrentTime().QuadPart + MAX_CONNECT_TIME;
			servicePoint->SmpContext.SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
			RELEASE_DPC_SPIN_LOCK(&servicePoint->SmpContext.TimeCounterSpinLock) ;
			
//			TimeInterval.QuadPart = - SMP_TIMEOUT;
			TimeInterval.HighPart = -1;
			TimeInterval.LowPart = - SMP_TIMEOUT;
			KeSetTimer(
				&servicePoint->SmpContext.SmpTimer,
//				servicePoint->SmpContext.SmpTimerExpire,
				*(PTIME)&TimeInterval,
				&servicePoint->SmpContext.SmpTimerDpc
				);

			break;
		
		default:
		
			break;
		}

		break;
		
	case SMP_SYN_SENT:

		switch(NTOHS(lpxHeader->Lsctl)) 
		{

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);
			servicePoint->SmpState = SMP_SYN_SENT;

			break;

		case LSCTL_CONNREQ:
		case LSCTL_CONNREQ | LSCTL_ACK:

			DebugPrint(1, ("LpxDoReceive SMP_SYN_SENT CONREQ\n"));
			SmpPrintState(1, "LSCTL_CONNREQ", servicePoint);

			InterlockedIncrement(&SmpContext->RemoteSequence);

			SmpContext->ServerTag = lpxHeader->ServerTag;

			servicePoint->SmpState = SMP_SYN_RECV;

			if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) 
			{
				InterlockedExchange(&SmpContext->RemoteAck, NTOHS(lpxHeader->AckSequence));
				SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

				servicePoint->SmpState = SMP_ESTABLISHED;

				if(servicePoint->ConnectIrp) {
					PIRP	irp;

					irp = servicePoint->ConnectIrp;
					servicePoint->ConnectIrp = NULL;
					irp->IoStatus.Status = STATUS_SUCCESS;

//					IoAcquireCancelSpinLock(&cancelIrql);
					IoSetCancelRoutine(irp, NULL);
//					IoReleaseCancelSpinLock(cancelIrql);

					DebugPrint(1, ("[LPX]SmpDoReceive: Connect IRP %lx completed.\n ", irp));
					IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
				}
			} 

			TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);
		
			break;
		
		default:

			break;
		}

		break;

	case SMP_SYN_RECV:

		DebugPrint(1, ("LpxDoReceive SMP_SYN_RECV CONREQ\n"));

		if(!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK))
			break;
		
		if(NTOHS(lpxHeader->AckSequence) < 1)
			break;

		servicePoint->SmpState = SMP_ESTABLISHED;

		if(servicePoint->ConnectIrp) 
		{
			PIRP	irp;

			irp = servicePoint->ConnectIrp;
			servicePoint->ConnectIrp = NULL;

//			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(irp, NULL);
//			IoReleaseCancelSpinLock(cancelIrql);

			irp->IoStatus.Status = STATUS_SUCCESS;
			DebugPrint(1, ("[LPX]SmpDoReceive: Connect IRP %lx completed.\n ", irp));
			IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		} else if(servicePoint->ListenIrp) 
		{
			PIRP						irp;
			PIO_STACK_LOCATION			irpSp;
			PTDI_REQUEST_KERNEL_LISTEN	request;
			PTDI_CONNECTION_INFORMATION	connectionInfo;
//			PTRANSPORT_ADDRESS			transportAddress;
//		    PTA_ADDRESS					taAddress;
//			PTDI_ADDRESS_LPX			lpxAddress;


			irp = servicePoint->ListenIrp;
			servicePoint->ListenIrp = NULL;

			irpSp = IoGetCurrentIrpStackLocation(irp);
			request = (PTDI_REQUEST_KERNEL_LISTEN)&irpSp->Parameters;
			connectionInfo = request->ReturnConnectionInformation;

			if(connectionInfo != NULL) 
			{	
				connectionInfo->UserData = NULL;
				connectionInfo->UserDataLength = 0;
				connectionInfo->Options = NULL;
				connectionInfo->OptionsLength = 0;

				if(connectionInfo->RemoteAddressLength != 0)
				{
					UCHAR				addressBuffer[FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
														+ FIELD_OFFSET(TA_ADDRESS, Address)
														+ TDI_ADDRESS_LENGTH_LPX];
					PTRANSPORT_ADDRESS	transportAddress;
					ULONG				returnLength;
					PTA_ADDRESS			taAddress;
					PTDI_ADDRESS_LPX	lpxAddress;


					DebugPrint(2, ("connectionInfo->RemoteAddressLength = %d, addressLength = %d\n", 
							connectionInfo->RemoteAddressLength, (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
																+ FIELD_OFFSET(TA_ADDRESS, Address)
																+ TDI_ADDRESS_LENGTH_LPX)));
													
					transportAddress = (PTRANSPORT_ADDRESS)addressBuffer;

					transportAddress->TAAddressCount = 1;
					taAddress = (PTA_ADDRESS)transportAddress->Address;
					taAddress->AddressType		= TDI_ADDRESS_TYPE_LPX;
					taAddress->AddressLength	= TDI_ADDRESS_LENGTH_LPX;

					lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

					RtlCopyMemory(
							lpxAddress,
							&servicePoint->DestinationAddress,
							sizeof(LPX_ADDRESS)
							);


					returnLength = (connectionInfo->RemoteAddressLength <=  (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
																		+ FIELD_OFFSET(TA_ADDRESS, Address)
																		+ TDI_ADDRESS_LENGTH_LPX)) 
											? connectionInfo->RemoteAddressLength
												: (FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
													+ FIELD_OFFSET(TA_ADDRESS, Address)
													+ TDI_ADDRESS_LENGTH_LPX);

					RtlCopyMemory(
						connectionInfo->RemoteAddress,
						transportAddress,
						returnLength
					);
				}
			}		
			
//			IoAcquireCancelSpinLock(&cancelIrql);
			IoSetCancelRoutine(irp, NULL);
//			IoReleaseCancelSpinLock(cancelIrql);

			irp->IoStatus.Status = STATUS_SUCCESS;
			DebugPrint(1, ("[LPX]SmpDoReceive: Listen IRP %lx completed.\n ", irp));
			IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
		}

		goto established;

		break;

	case SMP_ESTABLISHED:

established:

		// 
		// Check Server Tag.
		//
		if(lpxHeader->ServerTag != SmpContext->ServerTag) {
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Bad Server Tag Drop. SmpContext->ServerTag 0x%x, lpxHeader->ServerTag 0x%x\n", SmpContext->ServerTag, lpxHeader->ServerTag));
			break;
		}

		if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);
		}

		if(SmpContext->Retransmits) {
			SmpPrintState(1, "remained", servicePoint);
		}

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_ACKREQ:
		case LSCTL_ACKREQ | LSCTL_ACK:

			TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql );

			break;

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:
			
			if(NTOHS(lpxHeader->PacketSize & ~LPX_TYPE_MASK) <= sizeof(LPX_HEADER2)) {
				DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Data packet without Data!!!!!!!!!!!!!! SP: 0x%x\n", servicePoint));
			}

			if(((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(SmpContext->RemoteSequence))) > 0) {
//				char	buffer[256];

				TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);

				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				//DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: remote packet losed: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			if(((SHORT)(NTOHS(lpxHeader->Sequence) - SHORT_SEQNUM(SmpContext->RemoteSequence))) < 0) {
//				char	buffer[256];

				TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);
				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				//DebugPrint(1, ("Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			InterlockedIncrement(&SmpContext->RemoteSequence);

			if(NTOHS(lpxHeader->Lsctl) & LSCTL_DATA 
				|| NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) 
			{
				ExInterlockedInsertTailList(&SmpContext->RcvDataQueue,
				&RESERVED(Packet)->ListElement,
				&SmpContext->RcvDataQSpinLock
				);
			
				packetConsumed ++;
				dataArrived = 1;  // Data
			}

			//
			//BUG BUG BUG!!!!!!
			//
			TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);

			/*
			if((SmpContext->RemoteSequence % 100) != 0) 
				TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);
			else
				DebugPrint(1, ("Can't send ACK when RemoteSeq is 0x%x\n", SmpContext->RemoteSequence));
*/

			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			if(NTOHS(lpxHeader->Sequence) > SHORT_SEQNUM(SmpContext->RemoteSequence)) {
//				char	buffer[256];

				TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);
				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: remote packet losed: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			if(NTOHS(lpxHeader->Sequence) < SHORT_SEQNUM(SmpContext->RemoteSequence)) {
//				char	buffer[256];

				TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);
				//sprintf(buffer, "remote packet losed: HeaderSeq 0x%x ", NTOHS(lpxHeader->Sequence));
				//SmpPrintState(1, buffer, servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: Already Received packet: HeaderSeq 0x%x, RS: 0%x\n", NTOHS(lpxHeader->Sequence), SmpContext->RemoteSequence));
				break;
			}

			InterlockedIncrement(&SmpContext->RemoteSequence);

			//if(NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {
			ExInterlockedInsertTailList(&SmpContext->RcvDataQueue,
				&RESERVED(Packet)->ListElement,
				&SmpContext->RcvDataQSpinLock
				);
			
			packetConsumed ++;
			dataArrived = 1;  // Data
			//}

			//
			//BUG BUG BUG!!!!!!
			//
			TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);

			/*
			if((SmpContext->RemoteSequence % 100) != 0) 
				TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);
			else
				DebugPrint(1, ("Can't send ACK when RemoteSeq is 0x%x\n", SmpContext->RemoteSequence));
*/

//			SmpContext->TimerReason |= SMP_DELAYED_ACK;

//			if(NTOHS(lpxHeader->Lsctl) & LSCTL_DISCONNREQ) {
				DebugPrint(1, ("[LPX] Receive : LSCTL_DISCONNREQ | LSCTL_ACK: SmpDoReceive from establish to SMP_CLOSE_WAIT\n"));
				DebugPrint(1, ("[LPX] SmpDoReceive/SMP_ESTABLISHED: SmpDoReceive to SMP_CLOSE_WAIT\n"));
				servicePoint->SmpState = SMP_CLOSE_WAIT;
				
				CallUserDisconnectHandler(servicePoint, TDI_DISCONNECT_RELEASE);
				/*
				(*servicePoint->Connection->AddressFile->DisconnectHandler)(
					servicePoint->Connection->AddressFile->DisconnectHandlerContext,
					servicePoint->Connection->Context,
					0,
					NULL,
					0,
					NULL,
					TDI_DISCONNECT_RELEASE
					);
					*/
//			}

			break;


		default:

			break;
		}

		break;

	case SMP_LAST_ACK:

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck,(LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

			if(SHORT_SEQNUM(SmpContext->RemoteAck) == SHORT_SEQNUM(SmpContext->FinSequence)) {
				KIRQL	oldIrqlTimeCnt ;
//				SmpFreeServicePoint(servicePoint);
				DebugPrint(1, ("[LPX] SmpDoReceive: entering SMP_TIME_WAIT due to RemoteAck == FinSequence\n")) ;

				servicePoint->SmpState = SMP_TIME_WAIT;
				ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrqlTimeCnt) ;
				SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;
				RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrqlTimeCnt) ;
				break;
			}

			DebugPrint(1, ("SmpDoReceive SMP_LAST_ACK\n"));

			break;

		default:

			break;
		}

		break;

	case SMP_FIN_WAIT1:

		DebugPrint(1, ("SmpDoReceive SMP_FIN_WAIT1 lpxHeader->Lsctl = %d\n", NTOHS(lpxHeader->Lsctl)));

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);

			if(!(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK))
				break;

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

			if(SHORT_SEQNUM(SmpContext->RemoteAck) == SHORT_SEQNUM(SmpContext->FinSequence)) {
				DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1: SMP_FIN_WAIT1 to SMP_FIN_WAIT2\n"));
				servicePoint->SmpState = SMP_FIN_WAIT2;
			}
			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);
			servicePoint->SmpState = SMP_CLOSING;

			if(NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
				InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
				SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

				if(SHORT_SEQNUM(SmpContext->RemoteAck) == SHORT_SEQNUM(SmpContext->FinSequence)) {
					DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT1: entering SMP_TIME_WAIT due to RemoteAck == FinSequence\n")) ;
					servicePoint->SmpState = SMP_TIME_WAIT;
					SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;
				}
			}
			TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);

			break;

		default:
			
			break;
		}

		break;

	case SMP_FIN_WAIT2:

		DebugPrint(4, ("SmpDoReceive SMP_FIN_WAIT2  lpxHeader->Lsctl = 0x%x\n", NTOHS(lpxHeader->Lsctl)));

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);

			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			InterlockedIncrement(&SmpContext->RemoteSequence);
			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_FIN_WAIT2: entering SMP_TIME_WAIT\n"));
			servicePoint->SmpState = SMP_TIME_WAIT;
			SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;

			TransmitPacket_AvoidAddrSvcDeadLock(SmpContext->ServicePoint, NULL, ACK, 0, &servicePoint->SpinLock, oldIrql);

			break;

		default:
			
			break;
		}

		break;

	case SMP_CLOSING:

		switch(NTOHS(lpxHeader->Lsctl)) {

		case LSCTL_ACK:

			InterlockedExchange(&SmpContext->RemoteAck, (LONG)NTOHS(lpxHeader->AckSequence));
			SmpRetransmitCheck(SmpContext, SmpContext->RemoteAck, ACK);

			if(SHORT_SEQNUM(SmpContext->RemoteAck) != SHORT_SEQNUM(SmpContext->FinSequence))
				break;

			DebugPrint(1, ("[LPX] SmpDoReceive/SMP_CLOSING: entering SMP_TIME_WAIT\n"));
			servicePoint->SmpState = SMP_TIME_WAIT;
			SmpContext->TimeWaitTimeOut.QuadPart = CurrentTime().QuadPart + TIME_WAIT_INTERVAL;

			break;

		default:

			break;
		}

	default:
			
		break;
	}

	RELEASE_SPIN_LOCK(&servicePoint->SpinLock, oldIrql) ;

	if(!packetConsumed)
		PacketFree(Packet);

	if(dataArrived)
		return TRUE ;

    return FALSE ;
}			

//
//	acquire ServicePoint->SpinLock before calling
//
//	called only from SmpDoReceive()
static INT
SmpRetransmitCheck(
				   IN PSMP_CONTEXT	SmpContext,
				   IN LONG			AckSequence,
				   IN PACKET_TYPE	PacketType
				   )
{
	PNDIS_PACKET		packet;
//	PLIST_ENTRY			packetListEntry;
	PLPX_HEADER2		lpxHeader;
	PSERVICE_POINT		servicePoint;
	PUCHAR				packetData;
	PNDIS_BUFFER		firstBuffer;	
	PLPX_RESERVED		reserved;
	
	servicePoint = SmpContext->ServicePoint;

	DebugPrint(3, ("[LPX]SmpRetransmitCheck: Entered.\n"));

	UNREFERENCED_PARAMETER(PacketType) ;

	packet = PacketPeek(&SmpContext->RetransmitQueue, &SmpContext->RetransmitQSpinLock);
	if(!packet)
		return -1;

	NdisQueryPacket(
		packet,
		NULL,
		NULL,
		&firstBuffer,
		NULL
	);
   	packetData = MmGetMdlVirtualAddress(firstBuffer);

	reserved = RESERVED(packet);
	lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

	if(((SHORT)(SHORT_SEQNUM(AckSequence) - NTOHS(lpxHeader->Sequence))) <= 0)
		return -1;
						
	while((packet = PacketPeek(&SmpContext->RetransmitQueue, &SmpContext->RetransmitQSpinLock)) != NULL)
	{
		reserved = RESERVED(packet);
		NdisQueryPacket(
						packet,
						NULL,
						NULL,
						&firstBuffer,
						NULL
						);
		packetData = MmGetMdlVirtualAddress(firstBuffer);
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];

		DebugPrint(4, ("AckSequence = %x, lpxHeader->Sequence = %x\n",
						AckSequence, NTOHS(lpxHeader->Sequence)));
			
		if((SHORT)(SHORT_SEQNUM(AckSequence) - NTOHS(lpxHeader->Sequence)) > 0)
		{
			DebugPrint(3, ("[LPX] SmpRetransmitCheck: deleted a packet to be  retransmitted.\n")) ;
			packet = PacketDequeue(&SmpContext->RetransmitQueue, &SmpContext->RetransmitQSpinLock);
			if(packet) PacketFree(packet);
		} else
			break;
	}

//
//	OBSOULTE: We don't update RetransmitTimeOut here for now.
//	removed by hootch 09062003
//
//	if(!PacketQueueEmpty(&SmpContext->RetransmitQueue, &servicePoint->ServicePointQSpinLock))
//	{
//		SmpContext->RetransmitTimeOut.QuadPart = CurrentTime().QuadPart + CaculateRTT(SmpContext);
//	}

//
//	OBSOULTE: LatestSendTime, IntervalTime
//	removed by hootch 09062003
//
//	else {
//
//
//		if(SmpContext->LatestSendTime.QuadPart != 0) {
//			SmpContext->IntervalTime.QuadPart = ((SmpContext->IntervalTime.QuadPart * 99)
//											+ ((CurrentTime().QuadPart - SmpContext->LatestSendTime.QuadPart) * 1));
//			if(SmpContext->IntervalTime.QuadPart > 100)
//				SmpContext->IntervalTime.QuadPart /= 100;
//			else
//				SmpContext->IntervalTime.QuadPart = 1;
//
//			SmpContext->LatestSendTime.QuadPart = 0;
//		}else
//			SmpPrintState(1, "LatestSendTime = 0", servicePoint);			
//	}

	{
		KIRQL	oldIrql ;

		ACQUIRE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, &oldIrql) ;
		SmpContext->RetransmitEndTime.QuadPart = (CurrentTime().QuadPart + (LONGLONG)MAX_RETRANSMIT_TIME);
		RELEASE_SPIN_LOCK(&SmpContext->TimeCounterSpinLock, oldIrql) ;
	}

	if(SmpContext->Retransmits) {

		InterlockedExchange(&SmpContext->Retransmits, 0);
		SmpContext->TimerReason |= SMP_RETRANSMIT_ACKED;
	} 
	else if(!(SmpContext->TimerReason & SMP_RETRANSMIT_ACKED)
		&& !(SmpContext->TimerReason & SMP_SENDIBLE)
		&& ((packet = PacketPeek(&SmpContext->WriteQueue, &SmpContext->WriteQSpinLock)) != NULL)
		&& SmpSendTest(SmpContext, packet))
	{
		SmpContext->TimerReason |= SMP_SENDIBLE;
	}

	return 0;
}


static VOID
SmpDoMoreWorkDpcRequest(
				   IN	PSMP_CONTEXT	SmpContext
				   ) {
	KeInsertQueueDpc(&SmpContext->SmpWorkDpc, NULL, NULL) ;
}


static VOID
SmpWorkDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   ) {
	PSMP_CONTEXT		smpContext = (PSMP_CONTEXT)Context;
	PSERVICE_POINT		servicePoint = smpContext->ServicePoint ;
	LONG				cnt ;
	PNDIS_PACKET		packet;
	PLPX_RESERVED		reserved;
	PLIST_ENTRY			listEntry;
	PIRP				irp ;
	NTSTATUS			status ;
	BOOLEAN				timeOut;	
	LARGE_INTEGER		start_time;
	KIRQL				oldIrql ;

	start_time = CurrentTime();

	NbfReferenceConnection("SmpDoRecv", servicePoint->Connection, CREF_REQUEST) ;

do_more:
	smpContext->WorkDpcCall = CurrentTime();
	timeOut = FALSE;

	//
	//	Send
	//
	while(listEntry = ExInterlockedRemoveHeadList(&smpContext->SendQueue, &smpContext->SendQSpinLock)) {
		reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		RoutePacket(smpContext, packet, DATA) ;
		if(smpContext->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		{
			//smpContext->SmpTimerSet = TRUE;
			SmpTimerDpcRoutine(dpc, smpContext, junk1, junk2);
			timeOut = TRUE;
			break;
		}
		if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
	DebugPrint(3,("[LPX] SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
					start_time.QuadPart, CurrentTime().QuadPart));
			goto TIMEOUT;
		}
	}

	//
	//	Receive completion
	//
	while(listEntry = ExInterlockedRemoveHeadList(&smpContext->ReceiveQueue, &smpContext->ReceiveQSpinLock)) {
		reserved = CONTAINING_RECORD(listEntry, LPX_RESERVED, ListElement);
		packet = CONTAINING_RECORD(reserved, NDIS_PACKET, ProtocolReserved);
		SmpDoReceive(smpContext, packet) ;
		if(smpContext->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		{
			//smpContext->SmpTimerSet = TRUE;
			SmpTimerDpcRoutine(dpc, smpContext, junk1, junk2);
			timeOut = TRUE;
			break;
		}
		if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
	DebugPrint(3,("[LPX] SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
					start_time.QuadPart, CurrentTime().QuadPart));
			goto TIMEOUT;
		}
	}

	//
	//	Receive IRP completion
	//
	while(1)
	{
		//
		//	Initialize ListEntry to show the IRP is in progress.
		//	See LpxCancelRecv().
		//
		//	patched by hootch 02052004
		ACQUIRE_SPIN_LOCK(&servicePoint->ReceiveIrpQSpinLock, &oldIrql) ;
		listEntry = RemoveHeadList(&servicePoint->ReceiveIrpList) ;
		if(listEntry == &servicePoint->ReceiveIrpList) {
			RELEASE_SPIN_LOCK(&servicePoint->ReceiveIrpQSpinLock, oldIrql) ;
			break ;
		}
		InitializeListHead(listEntry) ;
		RELEASE_SPIN_LOCK(&servicePoint->ReceiveIrpQSpinLock, oldIrql) ;
 
		irp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);

		//
		//	check to see if the IRP is expired.
		//	If it is, complete the IRP with TIMEOUT.
		//	If expiration time is zero, do not check it.
		//
		//	added by hootch 02092004
		if( GET_IRP_EXPTIME(irp) && GET_IRP_EXPTIME(irp) <= CurrentTime().QuadPart) {

			DebugPrint(1,("[LPX] SmpWorkDpcRoutine IRP expired!! %I64d CurrentTime:%I64d.\n",
					(*((PLARGE_INTEGER)irp->Tail.Overlay.DriverContext)).QuadPart,
					CurrentTime().QuadPart
				));

            IoSetCancelRoutine(irp, NULL);
            irp->IoStatus.Status = STATUS_REQUEST_ABORTED ;
            IoCompleteRequest (irp, IO_NETWORK_INCREMENT);

			continue ;
		}

 
		status = SmpReadPacket(irp, servicePoint);
		if(status != STATUS_SUCCESS)
			break;
		else
			LpxCallUserReceiveHandler(servicePoint);
		if(smpContext->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		{
			//smpContext->SmpTimerSet = TRUE;
			SmpTimerDpcRoutine(dpc, smpContext, junk1, junk2);
			timeOut = TRUE;
			break;
		}
		if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
	DebugPrint(3,("[LPX] SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
					start_time.QuadPart, CurrentTime().QuadPart));
			goto TIMEOUT;
		}
	}

	//
	//	Timer expiration
	//
	//		smpContext->SmpTimerSet = TRUE;


	if(smpContext->SmpTimerExpire.QuadPart <= CurrentTime().QuadPart)
		SmpTimerDpcRoutine(dpc, smpContext, junk1, junk2) ;


	if(start_time.QuadPart + HZ <= CurrentTime().QuadPart){
	DebugPrint(3,("[LPX] SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
					start_time.QuadPart, CurrentTime().QuadPart));
		goto TIMEOUT;
	}


	if(timeOut == TRUE)
		goto do_more;

TIMEOUT:	
	cnt = InterlockedDecrement(&smpContext->RequestCnt) ;
	if(cnt) {
		//
		// Reset the counter to one.
		// Do work more then.
		//
		InterlockedExchange(&smpContext->RequestCnt, 1) ;
		goto do_more ;
	}

	NbfDereferenceConnection("SmpDoRecv", servicePoint->Connection, CREF_REQUEST) ;

	return;

//TIMEOUT:
//	DebugPrint(DebugLevel,("[LPX] SmpWorkDpcRoutine !!!! start_time : %I64d , CurrentTime : %I64d \n",
//					start_time.QuadPart, CurrentTime().QuadPart));
	
//	SmpDoMoreWorkDpcRequest(smpContext);
//	return;
}



//
//	request the Smp DPC routine for the time-expire
//
//
//	added by hootch 09092003
static VOID
SmpTimerDpcRoutineRequest(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   ) {
	PSMP_CONTEXT		smpContext = (PSMP_CONTEXT)Context;
	LONG	cnt ;
	KIRQL				oldIrql;
	BOOLEAN				raised = FALSE;
	LARGE_INTEGER		TimeInterval;

	//smpContext->SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
	//smpContext->Lsmptimercall = CurrentTime();

	UNREFERENCED_PARAMETER(dpc) ;
	UNREFERENCED_PARAMETER(junk1) ;
	UNREFERENCED_PARAMETER(junk2) ;

	if(smpContext->SmpTimerExpire.QuadPart > CurrentTime().QuadPart) {
		
		if(smpContext->Retransmits)
			DebugPrint(3,("SmpTimerDpcRoutineRequest\n"));

		ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
		
		KeCancelTimer(&smpContext->SmpTimer);
	
		TimeInterval.HighPart = -1;
		TimeInterval.LowPart = - SMP_TIMEOUT;

		KeSetTimer(
			&smpContext->SmpTimer,
//			smpContext->SmpTimerExpire,
			*(PTIME)&TimeInterval,	
			&smpContext->SmpTimerDpc
			);
		RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
	
		return;
	}



    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
		oldIrql = KeRaiseIrqlToDpcLevel();
		raised = TRUE;
	}

	if(smpContext->Retransmits)
		DebugPrint(1,("smpContext->SmpTimerExpire.QuadPart = %I64d, CurrentTime().QuadPart = %I64d\n",
			smpContext->SmpTimerExpire.QuadPart, CurrentTime().QuadPart));


//    ASSERT (KeGetCurrentIrql() == DISPATCH_LEVEL);
//smpContext->SmpTimerSet = TRUE;

	cnt = InterlockedIncrement(&smpContext->RequestCnt) ;
	if(smpContext->Retransmits) 
		DebugPrint(1,("cnt = %d\n", cnt));

	if( cnt == 1 ) {
		KeInsertQueueDpc(&smpContext->SmpWorkDpc, NULL, NULL) ;
//		KeInsertQueueDpc(&SocketLpxDeviceContext->LpxWorkDpc, smpContext, NULL) ;
	}
	if(raised == TRUE)
		KeLowerIrql(oldIrql);
}

static VOID
SmpTimerDpcRoutine(
				   IN	PKDPC	dpc,
				   IN	PVOID	Context,
				   IN	PVOID	junk1,
				   IN	PVOID	junk2
				   )
{
	PSMP_CONTEXT		smpContext = (PSMP_CONTEXT)Context;
	PSERVICE_POINT		servicePoint = smpContext->ServicePoint;
	PNDIS_PACKET		packet;
	PNDIS_PACKET		packet2;
	PUCHAR				packetData;
	PNDIS_BUFFER		firstBuffer;	
	PLPX_RESERVED		reserved;
	PLPX_HEADER2		lpxHeader;
	LIST_ENTRY			tempQueue;
	KSPIN_LOCK			tempSpinLock;
//	KIRQL				cancelIrql;
	LONG				cloned ;
	LARGE_INTEGER		current_time;
	LARGE_INTEGER		TimeInterval = {0,0};
	DebugPrint(5, ("SmpTimerDpcRoutine ServicePoint = %x\n", servicePoint));

	UNREFERENCED_PARAMETER(dpc) ;
	UNREFERENCED_PARAMETER(junk1) ;
	UNREFERENCED_PARAMETER(junk2) ;

//	if(smpContext->SmpTimerSet == FALSE){
//		smpContext->Lsmptimercall.QuadPart = CurrentTime().QuadPart;
//		return;
//	}
	InterlockedIncrement(&smpContext->TimeOutCount);
//	if(smpContext->TimeOutCount % 50)
//			SmpPrintState(DebugLevel, "Remember  ME!!!!!", servicePoint);


	KeInitializeSpinLock(&tempSpinLock) ;

	// added by hootch 08262003
	ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
	
	current_time = CurrentTime();
	if(smpContext->Retransmits) {
		SmpPrintState(1, "Ret", servicePoint);
		DebugPrint(1,("current_time.QuadPart %I64d\n", current_time.QuadPart));
	}

	if(servicePoint->SmpState == SMP_CLOSE) {
		smpContext->Lsmptimercall = CurrentTime();
		RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
		DebugPrint(1, ("[LPX] SmpTimerDpcRoutine: ServicePoint closed\n", servicePoint));
		return ;
	}

	//
	//	reference Connection
	//
	NbfReferenceConnection("SmpTimerDpcRoutine", servicePoint->Connection, CREF_REQUEST) ;

	KeCancelTimer(&smpContext->SmpTimer);

	//
	//	do condition check
	//
	switch(servicePoint->SmpState) {
		
	case SMP_TIME_WAIT:
		
		if(smpContext->TimeWaitTimeOut.QuadPart <= current_time.QuadPart) 
		{
			DebugPrint(1, ("[LPX] SmpTimerDpcRoutine: TimeWaitTimeOut ServicePoint = %x\n", servicePoint));

			if(servicePoint->DisconnectIrp) {
				PIRP	irp;
				
				irp = servicePoint->DisconnectIrp;
				servicePoint->DisconnectIrp = NULL;
				
//				IoAcquireCancelSpinLock(&cancelIrql);
				IoSetCancelRoutine(irp, NULL);
//				IoReleaseCancelSpinLock(cancelIrql);
				
				irp->IoStatus.Status = STATUS_SUCCESS;
				DebugPrint(1, ("[LPX]SmpTimerDpcRoutine: Disconnect IRP %lx completed.\n ", irp));
				IoCompleteRequest(irp, IO_NETWORK_INCREMENT);
			}
			smpContext->Lsmptimercall = CurrentTime();
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

			SmpFreeServicePoint(servicePoint);

			NbfDereferenceConnection("SmpTimerDpcRoutine", servicePoint->Connection, CREF_REQUEST) ;

			return;
		}
		
		goto out;
		
	case SMP_CLOSE: 
	case SMP_LISTEN:
		
		goto out;
		
	case SMP_SYN_SENT:
		
		if(smpContext->ConnectTimeOut.QuadPart <= current_time.QuadPart) 
		{
			// added by hootch 08262003
			smpContext->Lsmptimercall = CurrentTime();
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

			SmpFreeServicePoint(servicePoint);

			NbfDereferenceConnection("SmpTimerDpcRoutine: ", servicePoint->Connection, CREF_REQUEST) ;

			return;
		}
		
		break;
		
	default:
		
		break;
	}

	//
	//	we need to check retransmission?
	//
	if((!PacketQueueEmpty(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock))
		&& smpContext->RetransmitTimeOut.QuadPart <= current_time.QuadPart) 
	{
//		DebugPrint(DebugLevel, ("smp_retransmit_timeout smpContext->retransmits = %d, ",smpContext->Retransmits));
//		DebugPrint(DebugLevel, ("CurrentTime().QuadPart = %I64d\n", current_time.QuadPart));
//		DebugPrint(DebugLevel, ("smpContext->RetransmitTimeOut.QuadPart = %I64d\n", 
//			smpContext->RetransmitTimeOut.QuadPart));
//		DebugPrint(DebugLevel, ("delayed = %I64d\n", 
//			CurrentTime().QuadPart - smpContext->RetransmitTimeOut.QuadPart));

		//
		//	retransmission time-out
		//
		//if(smpContext->Retransmits > 100) 
		if(smpContext->RetransmitEndTime.QuadPart < current_time.QuadPart) 
		{
//#if(SMP_DEBUG)
			//DbgBreakPoint();
			//ASSERT(FALSE);
//			DebugPrint(DebugLevel,("Retransmits %d\n", smpContext->Retransmits));
//			DebugPrint(DebugLevel, ("delayed = %I64d\n", current_time.QuadPart - smpContext->RetransmitTimeOut.QuadPart));
//			DebugPrint(DebugLevel, ("CurrentTime().QuadPart = %I64d\n", current_time.QuadPart));
//			DebugPrint(DebugLevel, ("smpContext->RetransmitTimeOut.QuadPart = %I64d\n", 
//					smpContext->RetransmitTimeOut.QuadPart));
//			DebugPrint(DebugLevel, ("smpContext->Lsmptimercall.QuadPart = %I64d\n", 
//					smpContext->Lsmptimercall.QuadPart));
//			DebugPrint(DebugLevel, ("smpContext->RetransmitEndTime.QuadPart = %I64d\n", 
//					smpContext->RetransmitEndTime.QuadPart));
			//SmpPrintState(DebugLevel, "Retransmit Time Out", servicePoint);
			DebugPrint(1,("Retransmit Time Out\n"));
//			SmpFreeServicePoint(servicePoint);
			CallUserDisconnectHandler(servicePoint, TDI_DISCONNECT_ABORT);
			/*
			(*servicePoint->Connection->AddressFile->DisconnectHandler)(
				servicePoint->Connection->AddressFile->DisconnectHandlerContext,
				servicePoint->Connection->Context,
				0,
				NULL,
				0,
				NULL,
				TDI_DISCONNECT_ABORT
				);
*/			smpContext->Lsmptimercall = CurrentTime();
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

			SmpFreeServicePoint(servicePoint);
			NbfDereferenceConnection("SmpTimerDpcRoutine", servicePoint->Connection, CREF_REQUEST) ;

			return;
//#endif
		}

		//
		//	retransmit.
		//
		// Need to leave packet on the queue, aye the fear
		//
		packet = PacketPeek(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock);
		if(!packet) {
			DebugPrint(0, ("no packet - is not stable\n"));
			goto out;
		}
		NdisQueryPacket(
			packet,
			NULL,
			NULL,
			&firstBuffer,
			NULL
			);
		packetData = MmGetMdlVirtualAddress(firstBuffer);
		reserved = RESERVED(packet);
		lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];
		lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));

		InterlockedIncrement(&smpContext->Retransmits);
		smpContext->RetransmitTimeOut.QuadPart 
			= CurrentTime().QuadPart + CalculateRTT(smpContext);
		InterlockedExchange(&smpContext->LastRetransmitSequence, (ULONG)NTOHS(lpxHeader->Sequence));

		SmpPrintState(1, "Ret", servicePoint);
		
//		if(!RESERVED(packet)->Cloned) {
//			packet2 = PacketCopy(packet);

//			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
//			RoutePacket(smpContext, packet2, RETRAN);
//			ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
//		} else
//			SmpPrintState(2, "Cloned", servicePoint);
//
//		alternated by hootch	09132003
//
		
		packet2 = PacketCopy(packet, &cloned);
		if(cloned == 1) {
			RoutePacket(smpContext, packet2, RETRAN);
		} else {
			SmpPrintState(1, "Cloned", servicePoint);
			PacketFree(packet2) ;
		}
		
	} 
	else if(smpContext->TimerReason & SMP_RETRANSMIT_ACKED) 
	{
		BOOLEAN	YetSent = 0;
		
		SmpPrintState(1, "RetA", servicePoint);
		if(PacketQueueEmpty(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock))
		{
			smpContext->TimerReason &= ~SMP_RETRANSMIT_ACKED;
			goto send_packet;
		}

		InitializeListHead(&tempQueue);
		
		while((packet = PacketDequeue(&smpContext->RetransmitQueue, &smpContext->RetransmitQSpinLock)) != NULL)
		{
			ExInterlockedInsertHeadList(&tempQueue,
				&RESERVED(packet)->ListElement,
				&tempSpinLock
				);
			
			NdisQueryPacket(
				packet,
				NULL,
				NULL,
				&firstBuffer,
				NULL
				);
			packetData = MmGetMdlVirtualAddress(firstBuffer);
			reserved = RESERVED(packet);
			lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];
			lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));

/*			
			if(!RESERVED(packet)->Cloned) {
				packet2 = PacketClone(packet);

				ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
				smpContext->RetransmitTimeOut.QuadPart 
					= CurrentTime().QuadPart + CalculateRTT(smpContext);
				RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;

//				RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
				RoutePacket(smpContext, packet2, RETRAN);
//				ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
			} else {
				YetSent = 1;
				break;
			}

//		alternated by hootch	09132003
*/

			packet2 = PacketCopy(packet, &cloned) ;
			if(cloned == 1) {
				ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
				smpContext->RetransmitTimeOut.QuadPart 
					= CurrentTime().QuadPart + CalculateRTT(smpContext);
				RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;

				RoutePacket(smpContext, packet2, RETRAN);
			} else {
				PacketFree(packet2) ;
				YetSent = 1;
				break;
			}
		}
		
		while((packet = PacketDequeue(&tempQueue, &tempSpinLock)) != NULL)
		{
			ExInterlockedInsertHeadList(
				&smpContext->RetransmitQueue,
				&RESERVED(packet)->ListElement,
				&smpContext->RetransmitQSpinLock
				);
		}
		if(!YetSent)	
			smpContext->TimerReason &= ~SMP_RETRANSMIT_ACKED;
	} 
	else if(smpContext->TimerReason & SMP_SENDIBLE) 
	{
		SmpPrintState(2, "Send", servicePoint);
send_packet:
		while((packet = PacketDequeue(&smpContext->WriteQueue, &smpContext->WriteQSpinLock)) != NULL)
		{
			SmpPrintState(1, "RealSend", servicePoint);
			if(SmpSendTest(smpContext, packet)) 
			{
				NdisQueryPacket(
					packet,
					NULL,
					NULL,
					&firstBuffer,
					NULL
					);
				packetData = MmGetMdlVirtualAddress(firstBuffer);
				reserved = RESERVED(packet);
				lpxHeader = (PLPX_HEADER2)&packetData[ETHERNET_HEADER_LENGTH];
				lpxHeader->AckSequence = HTONS(SHORT_SEQNUM(smpContext->RemoteSequence));
				
//				RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
				RoutePacket(smpContext, packet, DATA);
//				ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
			} else 
			{
				ExInterlockedInsertHeadList(
					&smpContext->WriteQueue,
					&RESERVED(packet)->ListElement,
					&smpContext->WriteQSpinLock
					);
				break;
			}
		}
		smpContext->TimerReason &= ~SMP_SENDIBLE;
	} 
	//	else if(smpContext->TimerReason & SMP_DELAYED_ACK)
	//	{
	//		TransmitPacket_AvoidAddrSvcDeadLock(servicePoint, NULL, ACK, 0, &servicePoint->SpinLock, DISPATCH_LEVEL);
	//		smpContext->TimerReason &= ~SMP_DELAYED_ACK;
	//	}
	else if (smpContext->AliveTimeOut.QuadPart <= current_time.QuadPart) // alive message
	{
		LONG alive ;

		alive = InterlockedIncrement(&smpContext->AliveRetries) ;
		if(( alive % 2) == 0) {
			DebugPrint(100, ("alive_retries = %d, smp_alive CurrentTime().QuadPart = %llx\n", 
				smpContext->AliveRetries, current_time.QuadPart));
		}
		if(smpContext->AliveRetries > MAX_ALIVE_COUNT) {
			SmpPrintState(100, "Alive Max", servicePoint);
			
			DebugPrint(1, ("!!!!!!!!!!! servicePoint->Connection->AddressFile->RegisteredDisconnectHandler 0x%x\n",
				servicePoint->Connection->AddressFile->RegisteredDisconnectHandler));

			CallUserDisconnectHandler(servicePoint, TDI_DISCONNECT_ABORT);
			smpContext->Lsmptimercall = CurrentTime();
			// added by hootch 08262003
			RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

			SmpFreeServicePoint(servicePoint);

			NbfDereferenceConnection("SmpTimerDpcRoutine", servicePoint->Connection, CREF_REQUEST) ;

			/*
			if(servicePoint->SmpState == SMP_ESTABLISHED) {
				
				(*servicePoint->Connection->AddressFile->DisconnectHandler)(
					servicePoint->Connection->AddressFile->DisconnectHandlerContext,
					servicePoint->Connection->Context,
					0,
					NULL,
					0,
					NULL,
					TDI_DISCONNECT_ABORT
					);
			} else {
				//DbgPrint("\n\n\n!!!!!!!!!!!!!!!!!!!!!Alive Max NOT SMP_ESTABLISHED!!!!!\n\n\n");
				SmpPrintState(1, "Alive Max NOT SMP_ESTABLISHED!!!!!", servicePoint);
			}
			// End Mod.
			*/

			return;
		}

		DebugPrint(3, ("[LPX]SmpTimerDpcRoutine: Send ACKREQ. SP : 0x%x, S: 0x%x, RS: 0x%x\n", 
			servicePoint, smpContext->Sequence, smpContext->RemoteSequence));

		smpContext->AliveTimeOut.QuadPart = CurrentTime().QuadPart + ALIVE_INTERVAL;
		
//		RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);
		TransmitPacket_AvoidAddrSvcDeadLock(servicePoint, NULL, ACKREQ, 0, &servicePoint->SpinLock, DISPATCH_LEVEL);
//		ACQUIRE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

		InterlockedIncrement(&smpContext->AliveRetries);
	}

out:
	ACQUIRE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;
	smpContext->SmpTimerExpire.QuadPart = CurrentTime().QuadPart + SMP_TIMEOUT;
	smpContext->Lsmptimercall = CurrentTime();
	RELEASE_DPC_SPIN_LOCK(&smpContext->TimeCounterSpinLock) ;

	// added by hootch 08262003
	RELEASE_DPC_SPIN_LOCK (&servicePoint->SpinLock);

//	smpContext->SmpTimerSet = FALSE;
//	TimeInterval.QuadPart = - SMP_TIMEOUT;
	if(smpContext->Retransmits) {
		SmpPrintState(1, "Retaaa", servicePoint);
		DebugPrint(1,("current_time.QuadPart %I64d\n", current_time.QuadPart));
	}

	
	TimeInterval.HighPart = -1;
	TimeInterval.LowPart = - SMP_TIMEOUT;
	KeSetTimer(
		&smpContext->SmpTimer,
//		smpContext->SmpTimerExpire,
		*(PTIME)&TimeInterval,	
		&smpContext->SmpTimerDpc
		);

	//
	//	dereference the connection
	//
	NbfDereferenceConnection("SmpTimerDpcRoutine", servicePoint->Connection, CREF_REQUEST) ;

	return;
}
//
//
//	acquire SpinLock of connection before calling
//
void
CallUserDisconnectHandler(
				  IN	PSERVICE_POINT	pServicePoint,
				  IN	ULONG			DisconnectFlags
				  )
{
	LONG called ;
	// Check parameter.
	if(pServicePoint == NULL) {
		return;
	}

	if(pServicePoint->Connection->AddressFile->DisconnectHandler == NULL) {
		return;
	}

	if(pServicePoint->SmpState != SMP_ESTABLISHED) {
		return;
	}
//	ACQUIRE_DPC_SPIN_LOCK(&(pServicePoint->Address->SpinLock));
	called = InterlockedIncrement(&pServicePoint->lDisconnectHandlerCalled) ;
	if(called == 1) {

		// Perform Handler.
		(*pServicePoint->Connection->AddressFile->DisconnectHandler)(
			pServicePoint->Connection->AddressFile->DisconnectHandlerContext,
			pServicePoint->Connection->Context,
			0,
			NULL,
			0,
			NULL,
			DisconnectFlags
			);
	} else {
		DebugPrint(1,("[LPX]DisconnectHandler: Already Called\n"));
	}
//	RELEASE_DPC_SPIN_LOCK(&(pServicePoint->Address->SpinLock));
	return;
}

#endif