DI

Arguments:
    
    None


Return Value:

    NTSTATUS -- Indicates whether registration succeeded

--*/

{
    NTSTATUS                    status;
    TDI_CLIENT_INTERFACE_INFO   info;
    UNICODE_STRING              clientName;
    
    PAGED_CODE ();

 
    //
    // Setup the TDI request structure
    //

    RtlZeroMemory (&info, sizeof (info));
    RtlInitUnicodeString(&clientName, L"ndasbus");
#ifdef TDI_CURRENT_VERSION
    info.TdiVersion = TDI_CURRENT_VERSION;
#else
    info.MajorTdiVersion = 2;
    info.MinorTdiVersion = 0;
#endif
    info.Unused = 0;
    info.ClientName = &clientName;
    info.BindingHandler = Reg_NetworkPnPBindingChange;
    info.AddAddressHandlerV2 = Reg_AddAddressHandler;
    info.DelAddressHandlerV2 = Reg_DelAddressHandler;
    info.PnPPowerHandler = NULL;

    //
    // Register handlers with TDI
    //

    status = TdiRegisterPnPHandlers (&info, sizeof(info), &FdoData->TdiClient);
    if (!NT_SUCCESS (status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, (
					"Failed to register PnP handlers: %lx .\n",
					status));
        return status;
    }

    return STATUS_SUCCESS;
}


VOID
Reg_DeregisterTdiPnPHandlers (
	PFDO_DEVICE_DATA	FdoData
){

    if (FdoData->TdiClient) {
        TdiDeregisterPnPHandlers (FdoData->TdiClient);
        FdoData->TdiClient = NULL;
	}
}

VOID
Reg_AddAddressHandler ( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI add address handler

Arguments:
    
    NetworkAddress  - new network address available on the system

    DeviceName      - name of the device to which address belongs

    Context         - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

    PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, (
			"NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
			NetworkAddress->AddressType));
		return;
	}
	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
										DeviceName->Buffer,
										(ULONG)NetworkAddress->AddressType,
										(ULONG)NetworkAddress->AddressLength));

	//
	//	LPX
	//
	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);

	if(	RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) &&
		NetworkAddress->AddressType == TDI_ADDRESS_TYPE_LPX
		){
			PTDI_ADDRESS_LPX	lpxAddr;

			lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LPX: %02x:%02x:%02x:%02x:%02x:%02x\n",
									lpxAddr->Node[0],
									lpxAddr->Node[1],
									lpxAddr->Node[2],
									lpxAddr->Node[3],
									lpxAddr->Node[4],
									lpxAddr->Node[5]));
			//
			//	LPX may leave dummy values.
			//
			RtlZeroMemory(lpxAddr->Reserved, sizeof(lpxAddr->Reserved));

			//
			//	Check to see if FdoData for TdiPnP is created.
			//

			ExAcquireFastMutex(&Globals.Mutex);
			if(Globals.PersistentPdo && Globals.FdoDataTdiPnP) {
				LSBUS_QueueWorker_PlugInByRegistry(Globals.FdoDataTdiPnP, NetworkAddress, 0);
			}
#if DBG
			else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LPX: no FdoData for TdiPnP or enum disabled\n"));
			}
#endif
			ExReleaseFastMutex(&Globals.Mutex);

	//
	//	IP	address
	//
	} else if(NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]));
	} else {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType));
	}
}

VOID
Reg_DelAddressHandler ( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI delete address handler

Arguments:
    
    NetworkAddress  - network address that is no longer available on the system

    Context1        - name of the device to which address belongs

    Context2        - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

	PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, (
			"AfdDelAddressHandler: "
			"NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
			NetworkAddress->AddressType));
		return;
	}
	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
		DeviceName->Buffer,
		(ULONG)NetworkAddress->AddressType,
		(ULONG)NetworkAddress->AddressLength));

	//
	//	LPX
	//
	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);

	if(	RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE)){
		PTDI_ADDRESS_LPX	lpxAddr;

		lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LPX: %02x:%02x:%02x:%02x:%02x:%02x\n",
			lpxAddr->Node[0],
			lpxAddr->Node[1],
			lpxAddr->Node[2],
			lpxAddr->Node[3],
			lpxAddr->Node[4],
			lpxAddr->Node[5]));

		//
		//	IP	address
		//
	} else if(NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]));
	} else {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType));
	}
}


VOID
Reg_NetworkPnPBindingChange(
	IN TDI_PNP_OPCODE  PnPOpcode,
	IN PUNICODE_STRING  DeviceName,
	IN PWSTR  MultiSZBindList
){
	UNREFERENCED_PARAMETER(DeviceName);
	UNREFERENCED_PARAMETER(MultiSZBindList);

#if DBG
	if(DeviceName && DeviceName->Buffer) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DeviceName=%ws PnpOpcode=%x\n", DeviceName->Buffer, PnPOpcode));
	} else {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DeviceName=NULL PnpOpcode=%x\n", PnPOpcode));
	}
#endif

	switch(PnPOpcode) {
	case TDI_PNP_OP_ADD:
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("TDI_PNP_OP_ADD\n"));
	break;
	case TDI_PNP_OP_DEL:
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("TDI_PNP_OP_DEL\n"));
	break;
	case TDI_PNP_OP_PROVIDERREADY:
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("TDI_PNP_OP_PROVIDERREADY\n"));
	break;
	case TDI_PNP_OP_NETREADY:
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("TDI_PNP_OP_NETREADY\n"));

		//
		// Some NIC does not accept sending packets at early booting time.
		// So
		//

		ExAcquireFastMutex(&Globals.Mutex);
		if(Globals.PersistentPdo && Globals.FdoDataTdiPnP) {
			LSBUS_QueueWorker_PlugInByRegistry(Globals.FdoDataTdiPnP, NULL, 0);
		}
#if DBG
		else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NETREADY: no FdoData for TdiPnP or enum disabled\n"));
		}
#endif
		ExReleaseFastMutex(&Globals.Mutex);
	break;
	default:
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Unknown PnP code. %x\n", PnPOpcode));
	}
}


//////////////////////////////////////////////////////////////////////////
//
//	Exported functions to IOCTL.
//

//
//	Register a device by writing registry.
//

NTSTATUS
LSBus_RegisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PBUSENUM_PLUGIN_HARDWARE_EX2	Plugin
){
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				ndasDevInst;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	busDevReg = NULL;
	ndasDevRoot = NULL;
	ndasDevInst = NULL;


	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//
	KeEnterCriticalRegion();
	ExAcquireFastMutexUnsafe(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenServiceRegistry() failed.\n"));
		return status;
	}

	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {

		ZwClose(busDevReg);

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenNdasDeviceRegistry() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&ndasDevInst, Plugin->SlotNo, TRUE, ndasDevRoot);
	if(!NT_SUCCESS(status)) {

		ZwClose(busDevReg);
		ZwClose(ndasDevRoot);

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceInst() failed.\n"));
		return	status;
	}

	//
	//	Before writing information, clean up the device instance key.
	//

	DrDeleteAllSubKeys(ndasDevInst);

	//
	//	Write plug in information
	//

	status = WriteNDASDevToRegistry(ndasDevInst, Plugin);


	//
	//	Close handles
	//

	if(ndasDevInst)
		ZwClose(ndasDevInst);
	if(ndasDevRoot)
		ZwClose(ndasDevRoot);

	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
	KeLeaveCriticalRegion();

	return status;
}


NTSTATUS
LSBus_RegisterTarget(
	PFDO_DEVICE_DATA			FdoData,
	PLANSCSI_ADD_TARGET_DATA	AddTargetData
){
	NTSTATUS	status;
	HANDLE		busDevReg;
	HANDLE		ndasDevRoot;
	HANDLE		ndasDevInst;
	HANDLE		targetKey;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	busDevReg = NULL;
	ndasDevRoot = NULL;
	ndasDevInst = NULL;
	targetKey = NULL;

	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//
	KeEnterCriticalRegion();
	ExAcquireFastMutexUnsafe(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&ndasDevInst, AddTargetData->ulSlotNo, FALSE, ndasDevRoot);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ZwClose(ndasDevRoot);

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceInst() failed.\n"));
		return	status;
	}

	status = Reg_OpenTarget(&targetKey, AddTargetData->ucTargetId, TRUE, ndasDevInst);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ZwClose(ndasDevRoot);
		ZwClose(ndasDevInst);

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenTarget() failed.\n"));

		return	status;
	}

	//
	//	Before writing information, clean up the target key.
	//

	DrDeleteAllSubKeys(targetKey);

	//
	//	Write target information
	//

	status = WriteTargetToRegistry(targetKey, AddTargetData);


	//
	//	Close handles
	//
	if(targetKey)
		ZwClose(targetKey);
	if(ndasDevInst)
		ZwClose(ndasDevInst);
	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
	KeLeaveCriticalRegion();

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Adding an NDAS device into registry is completed. NTSTATUS:%08lx\n", status));

	return status;
}



NTSTATUS
LSBus_UnregisterDevice(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo
) {
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				devInstTobeDeleted;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = STATUS_SUCCESS;
	busDevReg = NULL;
	ndasDevRoot = NULL;
	devInstTobeDeleted = NULL;


	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//
	KeEnterCriticalRegion();
	ExAcquireFastMutexUnsafe(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);

		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	if(SlotNo != NDASBUS_SLOT_ALL) {
		status = Reg_OpenDeviceInst(&devInstTobeDeleted, SlotNo, FALSE, ndasDevRoot);
		if(NT_SUCCESS(status)) {

			//
			//	Delete a NDAS device instance.
			//
			status = DrDeleteAllSubKeys(devInstTobeDeleted);
			if(NT_SUCCESS(status)) {
				status = ZwDeleteKey(devInstTobeDeleted);
			}
#if DBG
			else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DrDeleteAllSubkeys() failed. SlotNo:%u NTSTATUS:%08lx\n", SlotNo, status));
			}
#endif

			ZwClose(devInstTobeDeleted);

#if DBG
			if(NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("A device(Slot %d) is deleted.\n", SlotNo));
			} else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwDeleteKey() failed. SlotNo:%u NTSTATUS:%08lx\n", SlotNo, status));
			}
#endif
		}
	} else {
		status = DrDeleteAllSubKeys(ndasDevRoot);
	}


	//
	//	Close handles
	//

	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
	KeLeaveCriticalRegion();

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Removing a DNAS device from registry is completed. NTSTATUS:%08lx\n", status));

	return status;
}


NTSTATUS
LSBus_UnregisterTarget(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo,
		ULONG				TargetId
) {
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				ndasDevInst;
	HANDLE				targetIdTobeDeleted;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = STATUS_SUCCESS;
	busDevReg = NULL;
	ndasDevRoot = NULL;
	ndasDevInst = NULL;
	targetIdTobeDeleted = NULL;

	KeEnterCriticalRegion();
	ExAcquireFastMutexUnsafe(&FdoData->RegMutex);

	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&ndasDevInst, SlotNo, FALSE, ndasDevRoot);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ZwClose(ndasDevInst);
		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceInst() failed.\n"));
		return status;
	}

	status = Reg_OpenTarget(&targetIdTobeDeleted, TargetId, FALSE, ndasDevInst);
	if(NT_SUCCESS(status)) {

		//
		//	Delete an NDAS device instance.
		//
		status = DrDeleteAllSubKeys(targetIdTobeDeleted);
		if(NT_SUCCESS(status)) {
			status = ZwDeleteKey(targetIdTobeDeleted);
		}
#if DBG
		else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DrDeleteAllSubKeys() failed. SlotNo:%u Target %u NTSTATUS:%08lx\n", SlotNo, TargetId, status));
		}
#endif
		ZwClose(targetIdTobeDeleted);
#if DBG
		if(NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("A device(Slot %d Target %u) is deleted.\n", SlotNo, TargetId));
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwDeleteKey() failed. SlotNo:%u Target %u NTSTATUS:%08lx\n", SlotNo, TargetId, status));
		}
#endif
	}


	//
	//	Close handles
	//
	if(ndasDevInst)
		ZwClose(ndasDevInst);
	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ntenance parts, such as a reference count,
// ACL, and so on. All outstanding connection-oriented and connectionless
// data transfer requests are queued here.
//

#if DBG
#define AREF_TEMP_CREATE        0
#define AREF_OPEN               1
#define AREF_VERIFY            2
#define AREF_LOOKUP             3
#define AREF_CONNECTION         4
#define AREF_REQUEST            5

#define NUMBER_OF_AREFS        6
#endif

typedef struct _TP_ADDRESS {

#if DBG
    ULONG RefTypes[NUMBER_OF_AREFS];
#endif

    USHORT Size;
    CSHORT Type;

    LIST_ENTRY Linkage;                 // next address/this device object.
    LONG ReferenceCount;                // number of references to this object.

    //
    // The following spin lock is acquired to edit this TP_ADDRESS structure
    // or to scan down or edit the list of address files.
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this structure.

    //
    // The following fields comprise the actual address itself.
    //

    PLPX_ADDRESS NetworkName;    // this address

    //
    // The following fields are used to maintain state about this address.
    //

    ULONG Flags;                        // attributes of the address.
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per active, connecting, or disconnecting connections on this
    // address.  By definition, if a connection is on this list, then
    // it is visible to the client in terms of receiving events and being
    // able to post requests by naming the ConnectionId.  If the connection
    // is not on this list, then it is not valid, and it is guaranteed that
    // no indications to the client will be made with reference to it, and
    // no requests specifying its ConnectionId will be accepted by the transport.
    //

    LIST_ENTRY AddressFileDatabase; // list of defined address file objects

    //
    // This structure is used for checking share access.
    //

    SHARE_ACCESS ShareAccess;

    //
    // Used for delaying LpxDestroyAddress to a thread so
    // we can access the security descriptor.
    //

    PIO_WORKITEM  DestroyAddressQueueItem;


    //
    // This structure is used to hold ACLs on the address.

    PSECURITY_DESCRIPTOR SecurityDescriptor;

    LIST_ENTRY				ConnectionServicePointList;	 // added for lpx

} TP_ADDRESS, *PTP_ADDRESS;

#define ADDRESS_FLAGS_STOPPING          0x00000040 // TpStopAddress is in progress.


//
// This structure defines the DEVICE_OBJECT and its extension allocated at
// the time the transport provider creates its device object.
//

#if DBG
#define DCREF_CREATION    0
#define DCREF_ADDRESS     1
#define DCREF_CONNECTION  2
#define DCREF_TEMP_USE    3

#define NUMBER_OF_DCREFS 4
#endif


typedef struct _DEVICE_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
#if 1 /* Added for lpx */
	USHORT				LastPortNum;

	//
	// Packet descriptor pool handle
	//

	NDIS_HANDLE         LpxPacketPool;

	//
	// Packet buffer descriptor pool handle
	//

	NDIS_HANDLE			LpxBufferPool;

	// Received packet.
	KSPIN_LOCK			PacketInProgressQSpinLock;
	LIST_ENTRY			PacketInProgressList;

	BOOL				bDeviceInit;
#endif

#if DBG
    ULONG RefTypes[NUMBER_OF_DCREFS];
#endif

    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure

    LIST_ENTRY DeviceListLinkage;                   // links them on LpxDeviceList
                                        
    LONG ReferenceCount;                // activity count/this provider.
    LONG CreateRefRemoved;              // has unload or unbind been called ?


    //
    // Following are protected by Global Device Context SpinLock
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this object.
                                        //  (used in KeAcquireSpinLock calls)

    //
    // the device context state, among open, closing
    //

    UCHAR State;

    //
    // Used when processing a STATUS_CLOSING indication.
    //

    PIO_WORKITEM StatusClosingQueueItem;
    
    //
    // The following queue holds free TP_ADDRESS objects available for allocation.
    //

    LIST_ENTRY AddressPool;

    //
    // These counters keep track of resources uses by TP_ADDRESS objects.
    //

    ULONG AddressAllocated;
    ULONG AddressInitAllocated;
    ULONG AddressMaxAllocated;
    ULONG AddressInUse;
    ULONG AddressMaxInUse;
    ULONG AddressExhausted;
    ULONG AddressTotal;


    //
    // The following queue holds free TP_ADDRESS_FILE objects available for allocation.
    //

    LIST_ENTRY AddressFilePool;

    //
    // These counters keep track of resources uses by TP_ADDRESS_FILE objects.
    //

    ULONG AddressFileAllocated;
    ULONG AddressFileInitAllocated;
    ULONG AddressFileMaxAllocated;
    ULONG AddressFileInUse;
    ULONG AddressFileMaxInUse;
    ULONG AddressFileTotal;

    //
    // The following field is a head of a list of TP_ADDRESS objects that
    // are defined for this transport provider.  To edit the list, you must
    // hold the spinlock of the device context object.
    //

    LIST_ENTRY AddressDatabase;        // list of defined transport addresses.
 
    //
    // following is used to keep adapter information.
    //

    NDIS_HANDLE NdisBindingHandle;

    ULONG MaxConnections;
    ULONG MaxAddressFiles;
    ULONG MaxAddresses;
    PWCHAR DeviceName;
    ULONG DeviceNameLength;

    //
    // This is the Mac type we must build the packet header for and know the
    // offsets for.
    //

    ULONG MaxReceivePacketSize;         // does not include the MAC header
    ULONG MaxSendPacketSize;            // includes the MAC header

    ULONG MaxUserData;
    //
    // some MAC addresses we use in the transport
    //

    HARDWARE_ADDRESS LocalAddress;      // our local hardware address.

    HANDLE TdiDeviceHandle;
    HANDLE ReservedAddressHandle;

    //
    // These are used while initializing the MAC driver.
    //

    KEVENT NdisRequestEvent;            // used for pended requests.
    NDIS_STATUS NdisRequestStatus;      // records request status.

    //
    // This information is used to keep track of the speed of
    // the underlying medium.
    //

    ULONG MediumSpeed;                    // in units of 100 bytes/sec

	//
	//	General Mac options supplied by underlying NIC drivers
	//

	ULONG	MacOptions;

	//
    // Counters for most of the statistics that LPX maintains;
    // some of these are kept elsewhere. Including the structure
    // itself wastes a little space but ensures that the alignment
    // inside the structure is correct.
    //

    TDI_PROVIDER_STATISTICS Statistics;

    //
    // This resource guards access to the ShareAccess
    // and SecurityDescriptor fields in addresses.
    //

    ERESOURCE AddressResource;

    //
    // This is to hold the underlying PDO of the device so
    // that we can answer DEVICE_RELATION IRPs from above
    //

    PVOID PnPContext;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;



typedef struct _CONTROL_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
    BOOL				bDeviceInit;

#if DBG
    ULONG RefTypes[NUMBER_OF_DCREFS];
#endif

    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure

    KSPIN_LOCK Interlock;               // GLOBAL spinlock for reference count.
                                        //  (used in ExInterlockedXxx calls)
                                        
    LONG ReferenceCount;                // activity count/this provider.
    LONG CreateRefRemoved;              // has unload or unbind been called ?

    //
    // Following are protected by Global Device Co °32 ˆÿÿ       Ñ &} ¾¨ # +@   @Öš …Â         # ,@   zÖš —  ½ } # -@       Ï  )} } # .@    ­Öš ú  *} zx # /@       „
  +} x # 0@        &  ,} Š] # 1@       &      x] # 2@    åÖš —  .} '} # 3@   éÖš f%  /} ‡| # 4@   öÖš u1  0} Ê{ # 5@       ˜F 1} ï{ # 6@    úÖš   2} } # 7@       _   3}  } # 8@        z  ½ ä\ # 9@    2×š —  5} -} # :@       ŠÂ 6}     # ;@    \×š ‹Â "½     # <@   —×š —  8} 4} # =@   ›×š ˜  9} N| # >@   ¡×š ü  :} ƒ| # ?@       AÎ  ;} d # @@    ª×š ¬
  <} [{ # A@   °×š BÎ  =} Í[ # B@   ·×š ã  >} | # C@   ¿×š £¯  ?} §O # D@       ŒÂ @}     # E@    Ş×š DÎ  A} .Ô # F@   ä×š œ;  B} ×c # G@   ê×š uX C} @| # H@   í×š   D} D| # I@       h!  E} ©O # J@        KÎ  F} ×{ # K@    ö×š ç  G} ‘| # L@   ü×š À  H} ³{ # M@       MÎ  I} b‚ # N@        NÎ  J} JÔ # O@        à3     ]‚ # P@    Øš —  L} 7} # Q@   Øš 	  M} } # R@       
  N} } # S@        ]  O} Ùo # T@       9  =½ Ty # U@        9  Q} O} # V@        oÎ  R} o # W@        pÎ  B½  o # X@    €Øš —  T} K} # Y@       FÎ  U} ®Õ # Z@       GÎ  G½ ¯Õ # [@    °Øš —  W} S} # \@       ¾  X} T| # ]@        h;  Y} Î% # ^@        ‡
  Z} „{ # _@        2  N½ i] # `@    çØš —  \} V} # a@       Â ]}     # b@        ƒ  S½ ù| # c@        ²
  _} Ñ{ # d@       ¸  `} §D # e@        ;c     ‘ÿ # f@        ü1  b} ,{ # g@        ¤;  c} ­{ # h@        ÒÜ  d} ÎY # i@        ÖÜ      { # j@        ²
  f} ^} # k@        ”Â g}     # l@        ¯  h} )\ # m@        •Â         # n@        ²
  j} e} !# o@        &  k} äp !# p@        –b l} µ/ !# q@       ÈÜ      ¾q !# r@        ™Â n}     "# s@        šÂ o}     "# t@        ›Â p}     "# u@        œÂ q}     "# v@        Â r}     "# w@        Â s}     "# x@        hI t} Ùª "# y@        ŸÂ u}     "# z@         Â         "# {@        ²
  w} i} ## |@        Ì¯ x} ] ## }@        m     ] ## ~@        Ç  z} Ç{ $# @        M   {} ˜X $# €@        £Â         $# @        …   }} Rr %# ‚@        ¦Â ~}     %# ƒ@        Ï  } (} %# „@        +  €} ¦q %# …@        ¹   } 	Y %# †@        K¹  ‚} r %# ‡@        !  ƒ} nw %# ˆ@        £Â „} {} %# ‰@        §Â …}     %# Š@        ¨Â †}     %# ‹@        §Â ‡} „} %# Œ@        ¨Â     …} %# @    éÛš ôƒ ‰} [| &# @   ğÛš õƒ Š} \| &# @   ÷Ûš öƒ ‹} ]| &# @   şÛš ÷ƒ Œ} ^| &# ‘@   Üš —  } [} &# ’@   	Üš ã  } =} &# “@   Üš øƒ } a| &# ”@   Üš ùƒ } b| &# •@   Üš zu ‘} c| &# –@       ‹ _½ ¨X &# —@    `Üš —  “} Œ} '# ˜@   dÜš î   ”} …| '# ™@   oÜš H  •} vd '# š@   uÜš   –}  } '# ›@        3 —} Nx '# œ@          ˜} š| '# @        «Â ™}     '# @        ¬Â š}     '# Ÿ@        ­Â ›}     '#  @        ®Â k½     '# ¡@    ÅÜš —  } ’} (# ¢@   ËÜš ä‚  o½ šB (# £@    İš —  Ÿ} œ} )# ¤@   İš PÎ   } ãw )# ¥@       QÎ  ¡} „± )# ¦@       RÎ  ¢} …± )# §@   İš SÎ  £} †± )# ¨@       TÎ  ¤} ‡± )# ©@        ï3     £n )# ª@        SÎ  ¦} ¢} *# «@        ó3 àÀ Øw *# ¬@    iİš —  ¨} } +# ­@   mİš PÎ  ©} Ÿ} +# ®@       õ3     §n +# ¯@    ´İš —  «} §} ,# °@       ¿Î  ¬} ¤ ,# ±@        Ï  èÀ ~} ,# ²@        °Â ®}     -# ³@        ¿Î  ¯} «} -# ´@        Ï  íÀ ¬} -# µ@          ±} —} .# ¶@    MŞš   ²} 1} .# ·@       ³Â ³}     .# ¸@        ´Â óÀ     .# ¹@          µ} °} /# º@    ¡Şš   ¶} ±} /# »@       ²Â ·}     /# ¼@  à32 ˆÿÿ           /# ½@        ·Â ¹}     0# ¾@        ¸Â º}     0# ¿@        ¹Â         0# À@    Sßš —  ¼} ª} 1# Á@   Xßš ã  ½} } 1# Â@   `ßš ç  ¾} F} 1# Ã@       ¿Î  ¿} ®} 1# Ä@        ¶Â À}     1# Å@        Ï¥ Á} "Y 1# Æ@        ºÂ Á     1# Ç@        D˜ Ã} c2 2# È@        ³Â Ä} ²} 2# É@       ²Â Å} ¶} 2# Ê@        ·Â Æ} ¸} 2# Ë@        ï‚  Ç} gX 2# Ì@        ¶Â 
Á ¿} 2# Í@                0.5  fd40e6a6ff4e3029fe3f4e4ab98e52a6 caspar 20101204-1 caspar-doc 20091115-1 20091115-1  06daae8a7ffe33cb7597844be4c911ed 20101204-1  b4d0af6863bd65c41ebb6d661937ead8 castle-combat                                                                       èš 0.    S     S şz tº \™     ø¶      `+     ¦d  ñ<         Xš ®±-     S     S { wº         ˜B            ¾C  ò<         ”š %.     S     S { 5¾          ”W	      @&     ÌÆ  ó<         Ãš œ¾     S     S { &N         8Û       Ü     ,  ô<         ùš ´	.    !S     "S { xº         6y       à     'Ø  õ<         gdš %.     …k     †k { {º         jr      ô,     Å  ö<         ëdš ¥«-     ˆk     ‰k -{ }º         |½      0	     Ï‰  ÷<         Deš ¥«-     ‹k     Œk 2{ ~º         R9            ·›  ø<         eš d²-    k     k 4{ €º         °            s'  ù<         Ğeš ½.    ‘k     ’k 5{ ‚º         ÔH      ˜     ¹  ú<         fš ´	.     ”k     •k 7{ …º         ˜O       ,     S±  û<         Vfš ¥«-     —k     ˜k <{ †º         ¸÷D       ¥     ßÆ  ü<         Ÿfš ..     šk     ›k A{ ˆº         x–       à     µm  ı<         gš ´	.     k     k L{ ‰º         È‚       0     ¾)  ş<         9gš ®±-      k     ¡k N{ æ         ‚      ˆ     šÕ  ÿ<         „gš ®±-     £k     ¤k T{ Šº         j      ”     `   =         Ågš ,´-     ¦k     §k W{ ‹º         ”‘       P     —­  =         4hš pÄ     ©k     ªk `{ Œº                      S  =         qhš 0®-     ¬k     ­k a{ º         8^       @     éâ  =         ¤hš ¥«-    ¯k     °k b{ º         h       È      ¯ñ  =         îhš *ü1     ²k     ³k e{ •ƒ         œĞ      ô     ¯/  =         iiš ®±-     µk     ¶k o{ ‘º         N‡             	¾  =         ®iš %.     ¸k     ¹k x{ “º         "           Çt  =         éiš ´	.     »k     ¼k z{ •º         lø       @     Ùœ  =         "jš d²-    ¾k     ¿k {{ –º         üâ      €     ø  	=         hjš ´	.     Ák     Âk |{ ˜º         '      p     ´e  
=         Êjš %.    Äk     Åk { ›º         |E      °     ¨Ş  =         ôjš ,´-     Çk     Èk †{ ˜ƒ         <              –  =         )kš ¥«-     Êk     Ëk ‡{ œº         œX
           Me  =         ikš ´	.     Ík     Îk ‹{ º         Î       Ì      œû  =         œkš ´	.     Ğk     Ñk { º         ^®            şg  =         ğkš 0®-     Ók     Ôk •{  º         |             
¥  =         $lš à­-     Ök     ×k ™{ ¡º         ä†      ğ     ½Ü  =         ‘lš ,´-     Ùk     Úk ¡{ ¢º         (Ì             ªĞ  =         ìlš ®±-     Ük     İk ¦{ ¤º         T¬             ^«  =         $mš ´	.     ßk     àk §{ ğ‚         –¬            28  =         bmš %.     âk     ãk ª{ ¥º         Òk       œ     Ä  =         ªmš ½.    åk     æk ®{ ¦º         Ø       ¤      Z  =         âmš ®±-    èk     ék ¯{ §º         è       t      ¹»  =         nš ®±-    ëk     ìk °{ °         ª            æ&  =         ?nš ´	.     îk     ïk ²{ ¨º         :;       `     Òl  =         tnš 0®-     ñk     òk ´{ >O         8€       „     3¼  =         Ìnš 0®-     ôk     õk ½{ õ·          Ğ|       Ø     ÅR  =         3oš 0®-     ÷k     øk Æ{ Î2         8Î      @       =         hoš ®±-     úk     ûk É{ ©º         &s       °     ~Ç  =          oš ½.    ık     şk Ë{ ªº         ˜K      ğ       =         Óoš ´	.      l     l Ğ{ «º         GGf9uêf‰ëfƒù:t
GGf‹f;ÊuğUSPhÔ ÿ5hü ÿÖƒÄ…À|Cƒø	W|hÌ ëhÀ ÿ5hü ÿÖƒÄ…À|"Wÿœ fƒ|Gş
tÿ5hü h€ ÿ¼ YY¡ú ™‹ê‹ØÅ„Ã   h  D$ Ph,  ÿ5|õ ÿ˜ ‹L$D$ëfƒù/t
@@f‹f…Éuğ3Òf9‹øtf‹fƒùmtfƒùntGGf9uêf‰ëfƒù:t
GGf‹f;ÊuğUSPhÔ ÿ5hü ÿÖƒÄ…À|Cƒø	W|hÌ ëhÀ ÿ5hü ÿÖƒÄ…À|"Wÿœ fƒ|Gş
tÿ5hü h€ ÿ¼ YY¡ú ™‹ê‹ØÅ„Ã   h  D$ Ph-  ÿ5|õ ÿ˜ ‹L$D$ëfƒù/t
@@f‹f…Éuğ3Òf9‹øtf‹fƒùmtfƒùntGGf9uêf‰ëfƒù:t
GGf‹f;ÊuğUSPhÔ ÿ5hü ÿÖƒÄ…À|Cƒø	W|hÌ ëhÀ ÿ5hü ÿÖƒÄ…À|"Wÿœ fƒ|Gş
tÿ5hü h€ ÿ¼ YY¡ú ™‰D$¡ú ‰T$™‹ê‹ØÅu‹D$D$„Ñ   h  D$ Ph.  ÿ5|õ ÿ˜ ‹L$D$ëfƒù/t
@@f‹f…Éuğ3Òf9‹øtf‹fƒùmtfƒùntfƒùhtGGf9uäf‰ëfƒù:t
GGf‹f;Êuğÿt$ÿt$USPhè ÿ5hü ÿÖƒÄ…À|Cƒø	W|hÌ ëhÀ ÿ5hü ÿÖƒÄ…À|"Wÿœ fƒ|Gş
tÿ5hü h€ ÿ¼ YYh/  ÿ5ú è"îÿÿ3Û9ğ t/9ğ tG9ğ t?  ú ¨t6¨t2¨t.h   jèëíÿÿë%9ğ u9ğ uö ú uh!  ë×èğÿÿh(  ÿ5Ğù è´íÿÿh"  ÿ5,ú è¤íÿÿ¡(ú ;Ãth%  Pëh$  ÿ5$ú èƒíÿÿh#  ÿ5ú èsíÿÿh)  ÿ5¸ú ècíÿÿh*  ÿ5Äù èSíÿÿh+  ÿ5xú èCíÿÿh&  ÿ5|ú è1îÿÿh'  ÿ5€ú è!îÿÿ¡°ú ™‹èÂ‰T$„Ä   h  D$ Ph0  ÿ5|õ ÿ˜ ‹L$D$ëfƒù/t
@@f‹f;Ëuğf9‹øtf‹fƒùmtfƒùntGGf9uêf‰ëfƒù:t
GGf‹f;Ëuğÿt$UPhÔ ÿ5hü ÿÖƒÄ;Ã|Cƒø	W|hÌ ëhÀ ÿ5hü ÿÖƒÄ;Ã|"Wÿœ fƒ|Gş
tÿ5hü h€ ÿ¼ YY¡0ú ;Ã]t94ú t	h  jëh  Pè%ìÿÿh  ÿ54ú èìÿÿhİ  ènğÿÿ¡<ğ S3öFV™hŞ  RPèåğÿÿ¡8ğ ™¹è  ÷ùSVhß  ™RPèÉğÿÿhá  ÿ5´ú èÈëÿÿhA  è!ğÿÿhD  ÿ5Ôù è®ëÿÿhC  ÿ5Øù èëÿÿhE  ÿ5Üù èëÿÿhF  ÿ5àù è~ëÿÿhG  ÿ5äù ènëÿÿhH  ÿ5èù è^ëÿÿhJ  ÿ5ìù èNëÿÿhI  ÿ5ğù è>ëÿÿhP  ÿ5øù è.ëÿÿhQ  ÿ5üù èëÿÿ3À9ğ hK  ”ÀPèëÿÿhL  ÿ58ú èøêÿÿèvêÿÿhO  ÿ5¤ù èãêÿÿÿ5hü ÿ¨ Y[ƒŒ$(  ÿL$è_  ‹Œ$   _d‰    ‹Œ$  ‹Æ^è°d  ÉÂ ¸-Ê èú`  ì   ¡ ñ SVØûÿÿ‰EğèÓ^  ÿu…Øûÿÿ3ÛP‰]üèåÿÿh@ ÿµØûÿÿÿÈ ;ÃYY£hü uSÿµØûÿÿh¤#  è9  ‹ğéë  W…ØûÿÿP¹ü Ç ù    èi^  ÿ5ü j]èä  ‰`ú ‰]éb  ÿE3Òf9Üûÿÿt)Üûÿÿ‹Áfƒ9:ufƒx:tB@@f9‹Èuéëf‰œUÜûÿÿ…ÜûÿÿPÿœ ;Ãt,ŒEÚûÿÿf‹fƒú tfƒú	tfƒútfƒú
u
f‰HII;ÃuÛ3ÿf9Üûÿÿt!Üûÿÿ‹Áf‹	fƒù tfƒù	u
G@@f9‹Èuç´}Üûÿÿf‹f;Ó„¯   f;Ó‰Ôûÿÿt"‹Î‹Æf‹	fƒù/tfƒù\uÿ…Ôûÿÿ@@f9‹Èuâfƒú-tkjh4 Vÿ¸ ƒÄ…ÀtVjh$ Vÿ¸ ƒÄ…ÀtAjh Vÿ¸ ƒÄ…Àt,jh Vÿ¸ ƒÄ…Àtfƒ>/u	ƒ½ÔûÿÿtVèráÿÿë„}ŞûÿÿPè¡   …Àt{ÿ5hü …Üûÿÿh  PÿÌ ƒÄ…À…{şÿÿÿ5hü ÿ¨ 3öY‰Xú ‰\ú ‰`ú ‰dú ‰hú F_ƒMüÿØûÿÿè…\  ‹Mô‹Æ^d‰    ‹Mğ[è/b  ÉÂ ÿ5hü ÿ¨ YS„}ÜûÿÿPÿuè"  ‹ğë´¸GÊ èW^  QQƒeì fƒ%pü  SV‹5| W‹}hğ WÿÖ3ÛC…Àu‰Ìù éÅ  hä WÿÖ…Àu‰Xó é®  hÔ WÿÖ…Àu‰Ğù é—  hÌ WÿÖ…Àu‰Äú é€  hÄ WÿÖ…Àu‰´ú éi  h¼ WÿÖ…Àu‰¸ú éR  h´ WÿÖ…Àu‰Äù é;  h¬ WÿÖ…Àu‰xú é$  h° WÿÖ…Àu!@ú ‰<ú é  h¨ WÿÖ…Àu!<ú ‰@ú éê  h¤ WÿÖ…Àu‰Ôù éÓ  h  WÿÖ…Àu‰Øù é¼  h˜ WÿÖ…Àu‰Üù é¥  h WÿÖ…Àu‰àù é  hˆ WÿÖ…Àu‰äù éw  h€ WÿÖ…Àu‰èù é`  hx WÿÖ…Àu‰ìù éI  hp WÿÖ…Àu‰ğù é2  hh WÿÖ…Àu‰øù é  h` WÿÖ…Àu‰üù é  h\ WÿÖ…Àu‰ôù éí  hT WÿÖ…Àu‰¤ù éÖ  h¬ WÿÖ…Àu!ú ‰ ú é¹  hœ WÿÖ…Àu‰ ú ‰ú éœ  hD WÿÖ…Àu£ğ £ğ £ğ £ ú éw  h4 WÿÖ…Àuƒ ú ‰ğ ‰ğ ‰ğ éM  h, WÿÖ…Àu!‰ğ ‰ğ ‰ğ Ç ú    é   h  WÿÖ…Àu‰(ú ‰$ú é  h WÿÖ…Àtéh WÿÖ…Àu‰,ú éà
  h WÿÖ…Àu‰ú ‰ ú ‰ú ëÕh  WÿÖ…Àu!4ú ‰0ú é£
  hü WÿÖ…Àu!0ú ‰4ú é†
  hô WÿÖ…Àu‰0ú ëáhì WÿÖ…Àu!ğ é[
  hä WÿÖ…Àu‰8ú éD
  hÜ WÿÖ…Àu! ğ é-
  hÔ WÿÖ…Àu!$ğ é
  hÌ WÿÖ…Àu‰Dú éÿ	  hÄ WÿÖ…Àu‰Hú éè	  h¼ WÿÖ…Àu‰Lú éÑ	  h´ WÿÖ…Àu‰Pú éº	  h¬ WÿÖ…Àu‰Tú é£	  h  WÿÖ…Àu‰Xú éŒ	  h” WÿÖ…Àu‰\ú éu	  hˆ WÿÖ…À„e	  h| WÿÖ…À„U	  ht WÿÖ…ÀuÇtú    é:	  hl WÿÖ…ÀuÇú    é	  hd WÿÖ…Àu!dú Ç`ú    ƒ%hú  é÷  h\ WÿÖ…Àu!`ú Çdú    ëÖhT WÿÖ…Àu!`ú !dú Çhú    é²  ‹5¸ jhH WÿÖƒÄ…À…í   3ÉƒÇ
‰ ú ‰ğ ‰ğ ‰ğ f‹f;Á„n  f=  „d  f=	 „Z  3ÛCSh¼ WÿÖƒÄ…Àu‰ğ ‰ğ ë|Sh° WÿÖƒÄ…Àu‰ğ ëdSh˜ WÿÖƒÄ…Àu‰ğ ëLSh¬ WÿÖƒÄ…Àu	ƒ ú ë3Sh” WÿÖƒÄ…Àu	 ú ëSh¸ WÿÖƒÄ…À…©  ƒ ú GGf‹f…À…Aÿÿÿéª  j[Sh@ WÿÖƒÄ…ÀuƒÇWèlÕÿÿ…Àk  	lú é{  Sh8 WÿÖƒÄ…ÀuƒÇWè@Õÿÿ…À?  	pú éO                     ‰<0W1            Ğ¢ T,     0                     ‰<0Y1            Ğ¢ Œ,     0                     ‰<0[1            Ğ¢ Ä,     0                     ‰<0]1            Ğ¢ ü,     0                     ‰<0_1            Ğ¢ 4-     0                     ‰<0a1            Ğ¢ l-     0                     ‰<0c1            Ğ¢ ¤-     0                     ‰<0e1            Ğ¢ Ü-     0                     ‰<0g1            Ğ¢ .     0                     ‰<0i1            Ğ¢ L.     0                     ‰<0k1            Ğ¢ „.     0                     ‰<0m1            Ğ¢ ¼.     0                     ‰<0o1            Ğ¢ ô.     0                     ‰<0q1            Ğ¢ ,/     0                     ‰<0s1            Ğ¢ d/     0                     ‰<0u1            Ğ¢ œ/     0                     ‰<0w1            Ğ¢ Ô/     0                     ‰<0y1            Ğ¢ 0     0                     ‰<0{1            Ğ¢ D0     0                     ‰<0}1            Ğ¢ |0     0                     ‰<01            Ğ¢ ´0     0                     ‰<01            Ğ¢ ì0     0                     ‰<0ƒ1            Ğ¢ $1     0                     ‰<0…1            Ğ¢ \1     0                     ‰<0‡1            Ğ¢ ”1     0                     ‰<0‰1            Ğ¢ Ì1     0                     ‰<0‹1            Ğ¢ 2     0                     ‰<01            Ğ¢ <2     0                     ‰<01            Ğ¢ t2     0                     ‰<0‘1            Ğ¢ ¬2     0                     ‰<0“1            Ğ¢ ä2     0                     ‰<0•1            Ğ¢ 3     0                     ‰<0—1            Ğ¢ T3     0                     œ?0y2            ¸­ ´[œ?0y2                  œ?0y2                  œ?0#2                   œ?0#2                   œ?0#2                   œ?0#2                   œ?0#2                  œ?0#2                  œ?0#2                  œ?0#2                  œ?0#2                  œ?0#2                  ‰<0™1                  œ?0#2                  œ?0#2                  ‰<0™1                       0                   ‰<0›1                       0                   œ?0'2                  ‰<01                       0                   ‰<0Ÿ1                       0                   œ?0+2                  ‰<0¡1                       0                   ‰<0£1                       0                   ‰<0¥1                       0                   ‰<0§1                       0                   ‰<0©1                       0                   ‰<0«1                  ‰<0«1                       0                   ‰<0­1                  œ?0F2                  ‰<0­1                       0                   ‰<0¯1                       0                   ‰<0±1                       0                   ‰<0³1            Ğ¢ 8     0                     M90µ1            ,¬ ˆ@ M90µ1                  M90µ1                  M90µ1                  M90¹1                  M90µ1                  M90µ1                  M90µ1            ¬ `@ M90µ1                  M90µ1                  M90µ1                  ‰<0µ1                   ˜@   0       €   €         ˜@   0       €   €         ˜@   0       €   €         ‰<0µ1                   M90Á1            ,¬ ØA M90Á1                  M90Á1                       0                     M90Å1            ,¬ HB M90Å1                  M90Å1                       0                     M90É1            ,¬ ¸B M90É1                  M90É1                       0                     M90Ğ1                  M90Ğ1                       0        ÿƒø!ÿÿÿƒûÿÿÿ»   éÿÿÿÇ€rB    éÙşÿÿÇ€rB    éğşÿÿ¡ä:C jhÓA PhˆÒA h¼zB èyË  ƒÄPh¼zB h rB ÿø<C ƒÄ¸   _^][ƒÄ<Ã_^]¸3   [ƒÄ<Ã‹D$(…Àt	Pè Õ  ƒÄ‹D$ …À„9  PèÕ  ƒÄ‹Ã_^][ƒÄ<Ãèƒ›  ‹|$(…ÿt`¡trB 3ö…ÀvLƒ<· u<‹ rB j‹±RhÄÒA h¼zB èßÊ  ƒÄPh¼zB h rB ÿø<C ƒÄƒû»   ¡trB F;ğr´Wè”Ô  ƒÄ‹|$ …ÿtX¡xrB 3ö…ÀvDƒ<· u4¡¤rB h  ‹°QhèÒA h¼zB èuÊ  ƒÄPh¼zB h rB ÿø<C ƒÄ¡xrB F;ğr¼Wè4Ô  ƒÄ‹D$<…Àt_h  hğÑA h¼zB è/Ê  ƒÄPh¼zB h rB ÿø<C ƒÄh  hÓA h¼zB èÊ  ƒÄPh¼zB h rB ÿø<C ƒÄ…Ûu»   ¡@rB ‹t$,‹|$…À„y  ‹4rB ‹Ç+Æƒù,  …Ût-ƒû¸|0B t¸ mB ‹ä:C j RPh°çA h¼zB èÉ  ƒÄë{…ÀuP¡ä:C PhÜçA h¼zB èqÉ  ƒÄë\‹|rB …Ét&‹L$…Òu¡ä:C RPhèèA h¼zB èAÉ  ƒÄë,ƒø¹ mB t¹x0B j Q‹ä:C PQhéA h¼zB èÉ  ƒÄPh¼zB h rB ÿø<C ‹L$(ƒÄ…Év9ƒù¸ mB t¸x0B j PQhPéA h¼zB èÒÈ  ƒÄPh¼zB h rB ÿø<C ƒÄ…övaƒş¸ mB t¸x0B j PVhˆèA h¼zB è•È  ƒÄë&…Éu6…Ûu2…Àu.‹ä:C PRhÜçA h¼zB èmÈ  ƒÄPh¼zB h rB ÿø<C ƒÄ…ÿuƒû‹T$3À;Â_Û^ƒãF]ƒÃ‹Ã[ƒÄ<Ã;şuƒû»R   _^‹Ã][ƒÄ<Ã‹D$…Àvƒû»Q   _^‹Ã][ƒÄ<Ã…öv	…Ûu»   _^‹Ã][ƒÄ<ÃP@ ,P@ pN@ O@ +O@ BN@  ¡¸zB ‹ ;C ƒáV‹PƒâşÊ3Ò‰HŠ ;C ‹5¸zB €á€ù‹F”ÂÒ3Ğƒâ3Ğ‰V‹¸zB ‹<;C ^‹AƒâÁâ$û3Ğ‰Q¡¸zB ‹(;C ‰H‹¸zB ¡,;C ‰B‹¸zB ‹0;C ‰J¡rB ƒè t(Ht¡¸zB ‹HƒÉë#‹¸zB ‹A 3Ğƒâ3Ğ‰Që¡¸zB ‹Hƒá÷‰H ;C < ;C …ğ   <*v9‹@rB …É‹4rB …É…³  3Éh  ŠÈj‹Á¹
   ™÷ùjRPh´0B éë   ¡@rB …Àufƒ=€rB t]h  h¼ºB hÀ;C èã  Ph èA h¼zB èCÆ  ƒÄPh¼zB h rB ÿø<C h0<B j	h¬rB èùĞ   ¬rB ƒÄ<yt<Y…!  f¡";C f= r
f= †   f= „ƒ   f= t}f= wwè¹¥  ¡¸zB ‹D;C ‰¸   Ã<v¸‹@rB …É‹4rB …É…Ã   3Ò¹
   ŠĞh  ‹Âj ™÷ùjRPh°0B h¼ºB hÀ;C è  ƒÄPhxäA h¼zB èlÅ  ƒÄ ëj‹@rB …É‹4rB …Éul3Éf= h  f‹Ès"‹L0B Rh¼ºB hÀ;C è¾  ƒÄPhìäA ëQh¼ºB hÀ;C è£  ƒÄPh´äA h¼zB è Å  ƒÄPh¼zB h rB ÿø<C ƒÄ3ÀÃ¡@rB SUVW3Û3ö¿   ;Ã‰Ô:C ‰Ğ:C ‰Ø:C ‰=”;C ‰À:C „Á   94rB uISh mB h mB h¼ºB hÀ;C è  ƒÄPh,1B hØåA h¼zB ènÄ  ƒÄPh¼zB h rB ÿø<C ƒÄè²  ¡;C %ÿÿ  +Ã„I  ƒè„ğ  ƒè„   h  h¼ºB hÀ;C èª  Ph$êA h¼zB è
Ä  ƒÄPh¼zB h rB ÿø<C ƒÄèş  ‹Ç_^][Ã9rB t*h €  hP<B Ç¨;C P<B èœX ƒÄPèX ƒÄé^ÿÿÿè   …À„Qÿÿÿ_^]¸2   [Ã9@rB …‚   94rB uz¡rB ¹(1B ;Ãu¹ mB 9=rB t¸ mB ë¡¸zB ŠP¸1B öÂu¸1B SQPh¼ºB hÀ;C èÏ  ƒÄPh1B hØåA h¼zB è'Ã  ƒÄPh¼zB h rB ÿø<C ƒÄèû%  ‹ø;û„O  ƒÿ2ª   ¡@rB ;Ã¡4rB tZ;ÃuZƒÿ¸¼éA t¸ÔéA h  hğéA Ph¨éA h¼zB è½Â  ƒÄPh¼zB h rB ÿø<C ƒÄ3Éƒÿ”Á   ‹ñéŞ  ;Ãt¦ƒÿ¾¼éA t¾ÔéA h  h¼ºB hÀ;C èù  PhğéA Vh”éA h¼zB èSÂ  ƒÄë”‹÷é•  9@rB …   94rB uy¡rB ¹(1B ;Ãu¹ mB 9=rB t¸ mB ë‹¸zB öB¸1B u¸1B SQPh¼ºB hÀ;C èy  ƒÄPh 1B hØåA h¼zB èÑÁ  ƒÄPh¼zB h rB ÿø<C ƒÄè…Ïÿÿ‹ø;û„»   ƒÿ„»   ƒÿ2§   ¡@rB ;Ã¡4rB tW;ÃuWƒÿ¸¼éA t¸ÔéA h  høéA Ph¨éA h¼zB è^Á  ƒÄPh¼zB h rB ÿø<C ƒÄ3Àƒÿ”À…   ‹ğëD;Ãt©ƒÿ¾¼éA t¾ÔéA h  h¼ºB hÀ;C è  PhøéA Vh”éA h¼zB è÷À  ƒÄë—‹÷ƒÿ…5  ‹;C ‹=œrB ¡@rB ;×öF;Ã¡4rB t;Ãu;óti¹ü0B ¸ô0B ëg;Ãtì;ót½ü0B ¿ô0B ë
½ mB ¿ì0B h  hè0B h¼ºB hÀ;C è  ‹;C ‹;C ƒÄP¡œrB hä0B QURPWh mB ë.¹ mB ¸ì0B h  hà0B h mB h mB RQ‹;C QWPhÜ0B hğåA h¼zB èÀ  ƒÄ,Ph¼zB h rB ÿø<C ƒÄ÷ŞöƒÆéE  9@rB …”   94rB …ˆ   ¡rB ¹(1B ;Ãu¹ mB 9=rB t¸ mB ë%9;C u¸Ğ0B ë‹¸zB öB¸1B u¸1B SQPh¼ºB hÀ;C è  ƒÄPhÈ0B hØåA h¼zB èn¿  ƒÄPh¼zB h rB ÿø<C ƒÄÇ¸;C ¼zB ‰¼;C ¿¼zB ¡Ì:C H£Ì:C x‹È:C 3ÀŠA‰È:C ëè½  ƒøÿtN‹¸;C ˆ‹-¸;C ¡¼;C E@‰-¸;C = €  £¼;C u¯SPWè™  ‹ğƒÄ;ó‰=¸;C ‰¼;C u"9;C tŠë¡¼;C ;ÃtSPh¼zB èe  ƒÄ9@rB u9rB uèı  ¡;C ;ÃtXƒø~Lh!  h¼ºB hÀ;C è
  Ph êA h¼zB èk¾  ƒÄPh¼zB h rB ÿø<C ƒÄ¾2   èZ  ‹Æ_^][Ã¾   ë	ƒş[  ‹À:C ¡;C ;Ğ¡@rB „Î   ;Ã¡4rB t;Ãuë?;Ãt;h  h¼ºB hÀ;C è„	  PhÀ0B h¼zB èä½  ƒÄPh¼zB h rB ÿø<C ƒÄ¡;C ‹À:C h  PQhHêA h¼zB è¬½  ƒÄPh¼zB h rB ÿø<C ‹¸zB ƒÄöBt+h  h¼èA h¼zB èu½  ƒÄPh¼zB h rB ÿø<C ƒÄ¾   èd
  ‹Æ_^][Ã;Ãt>¡ø:C ;Ãt%3Éf‹;C QPèp   ƒÄ;Æ~Q‹ğè2
  ‹Æ_^][Ã94rB u;Sh¸0B ë94rB u+;óu'ShÜ0B h¼zB èï¼  ƒÄPh¼zB h rB ÿø<C ƒÄèã	  ‹Æ_^][Ã‹D$SU3íVƒøW‚Ì  ‹|$Wè3  ‹ØGPè(  ‹L$ f‹ğæÿÿ  ƒÁüƒÄ;ñ‡è  ‹Ã%ÿÿ  =SD  Æ   tƒø	„Í   =M3  „Â   éQ  ƒş‚  ŠG„À‡  hğì@ jVWè  ‹ØƒÄ…Û„"  ¡4rB …Àt8jh¼ºB hÀ;C è—  PhÀ0B h¼zB è÷»  ƒÄPh¼zB h rB ÿø<C ƒÄƒûğ  ƒûÎ=âÜ…÷÷èz¸ ÿ <•ışØFû:H1zÄØ9ñ6öaòÜ®ä"à
|ı¡«O#ô‚0‚şLc6çò3²€<d‚=O&¨!œgšd dÿ}~-aü² xûóÔ5û\Ân7r³Ïaà\$–Ê­ÈZ|ÓÒvÙ×ëıòY¿¬û8çÕ©®~9(s¬[È¿O,ì˜ÿãO².pH¶ãóF¾£ı3×ßÏ¯šd&¡¯>bwÜû<p(VÛ:ŸSòóÖ52Oó¹ï%ŞLß%I7ë"±9 }ª 'ÜnõcMÄã|©ş+£”tO¸)İƒ·ä95Bª:JEbÿø´DÔ¿$b7Ë/ìNÖ´i/a5øÛ*ÕÖNÉ
Ò/xU–Ú©`ŸéÜQO8}Dˆ­juVªí_IU€~H,»†şîĞvJ^çİI¨‡	½È'Ku`r<$‡c‡¬çdû'Yİ—Åj¶Ì	n“H›q²æüà–pÎÅÚ»ı0=_Û¤ÿëÌ',ÕÜ)Yzîz]öFôƒR§JİÓ¹c\—(û>j| +Õ1)çì/g/g%ã´B¢	Å…%Ì‘|ËñPõœ•ƒ$ª±¿”$«3}Vğ.í+c‚¿áı£Û*ù^S ÑÄ­”/3—NŒõ!ú»öYªö£o’…-KùÖ}©aQgv~b­÷-{!ª’Ğ?åLmÆvœLaÍ#9ëz³%S='IÆTù	´3¾p6©¹Î&ã`¼Xm‰³bÌ‚½%É>é<-I*_Æ`_,·¿ÙšÔÌz3¹ö†Úû9{öËLôĞ}4¡¿ƒ\¡us¥†±¯Š¡ˆ¹
>#CÈ‹B¥ˆ³h”º½}†qGæ"=rÌİÄOLºpOçÍS¯P…‘ô…Mb¡”§Q.dFñ{¾ÏyşS>6“Ñ}ØÏEj¢Ïl¾YÂ\>’pÂi)âıQ®"Œ•>‰3a<û¾ŒròÆı{¤ÄùØ8à\#×¹f|ì4«½N³Ù^ÒT'yÑ˜ëªIN¡¹Û)u
C&ze|ŸñsŞ8Â·ı¥·šÂ~¨fÌs2Šûc‡4(…¹PÙPdÜ”IF…„/áĞiÎ£A~b'óúuo±Ç¾øëåRçuÆñç¾?ol&6ABÚ9'"™‘KGD‚"©'’	"Xq"AD$'VŠ!„[W6qâ²LDº m±Z)ı£%%H:$” ¥ÖwŸç}Ï÷ŞÜh »ğá{~<çœçœ÷œsŸ#­¬eÇ¡›ÒzèÒš¿B]	súe ~SŞôÿ…îJkŞ—Ò¤¶ñby-Ø§­¯ü»÷™ãÎÿˆœ?ô6ılc[ÊX>Ü’w9‹~<¤5şKê/Q¿HÖŞı¹Lú×YŸŸ‘ïÅ—	y—oüÖ®ÕÇOşË[ãÿ¥ÍïÑs×|*oÚçEì·‰9ş!MŠ}G
ØŸsË{À«ÌsJø—ı!étóæ`ñOñıCú|K^ó¾¤Ïß1î¯çcyëĞßHWQ§cş‚ûõ#é!~©u›¥Ö¹Ïú‚¦í5şçúˆAj¨k‚>©õFÙÃŸHç¥6äùÊ§¥Ë/¢üíï‚öYÕk{ú!.úSÈé	ÓÇ,KÒ±hO;ç|Òò2)#NérëIëº½aÚŒò½¯ËS‚ïI«ŞWö÷Rm¿«±D~&»º.¿FÿÉ^´ºgR’üÖp×ÔB¡¶å.'¶Ş™Æn{"Áˆßv«¢x[ûİı-ßlù²ş—xï$¹Ó’î)ÒJ¡”z+äÇ9×eèE©õ·¥Ù«".—nW×¬’÷Ğˆ4ºİ¬ÉMø€µ[„jióÏa·MÙ
a¹ò…Aû¡w
’Ò² —ÃvÉ°¼Û=-=v?1Q½ôÛŸñ¿ı™X^"v<ë2ºåPÁ™íµÛx»õaß+	â”kXu’²„õ	6'á,é:tIÛ»k‘ïÜõcÌ£Õ"?dü=ùé×IO^…ô*Ä]†£{‘îŞq6bÅ¤g¡V#¬BÊGPlbÃĞÃûä.u'H?/B>Aëıc¨%7¶ùfX‡3Şi¨„~˜§ÿÁ—#öyäƒÌ1®ö»„ªoƒ>xlü,%}Z Â0Ì‡¸.J¯6ª|£l#-¢6«/¥Î´Ko{ÓhYVyWä_l!šGˆ–ë|†2‰EÈX„®·]™V?aõGsŠQfEÉß@YÿyÎà†¼×£ï*[fnhìˆÉß3ùi8£¬È”mí•é÷û¹•U¶e¾ÿ–™[ªŸ|ßm”Ş,mœ§6w’ïôµ´yÍü7q~ˆ‰=È¨wŠúiÎp\Fİ~â„Ù`×ƒ¯Ù·[ìµUÊ{)ïÑ:ìJ)Ûa}w¸´M4xåØª]#ÿ=Zv!p÷Ocûßá.í:å²×Âyäë“FşkZ¼§¤K¹ÓÊxG*½'ôS§¸gf©ë"=BKè¸.Íî ÚƒßœmoP*¼òıa_ÄKî¾Qgœñ»B¿NÑG=éQ÷1éæQÏ™Bî®“ö½pŒóŞkrX–â‹ÇE¾Ag` *á"¼o(§ş< =Œ~w#ÍÁÕ¨<VeYTfA?±£å™È&|·BìuCÊNÛMA^²‡ôG„éoa;³>äå«q±.F}[9löã^Ô¯¬Gı„ªìL>æÀ{¤7PõK"ìvÆü=Œ²¾VõCè<ªól¤†Lo#ãĞ0eæ:F?|C66ÃiÌ›TúJÃû0nè7¶3ie)ÒËg#Ìa†2Ç·öĞ2kfööİ³½—Ú3fÿeïtöÛ›9öe8N2ÇşLÛ£2ı|Z5°l6Ë—’Ld1«,{¯ß{~ï¦ö ~×ô}ö,ÚK3‰õ»-Á.÷ËYŞ×Ñ±çtw˜H:KşÌÚ"3hÓAêlÊ¬•/ÁİÈİdÚåĞÔIå­Dğql=x›¶>¼B×ğo4[±MÆÖù
5ø«Ñ¿ı»· Ó0’­Œ£ş¿Hñ÷Úğ"eü÷Bû”â·µ W³ô¸ÑW±kÃßºƒ”õª`¾%™|cò?1ş¨-¼YE¥ÔGŠ¨Ë;H±ÏÃ>?K‹¿-NÃo%¶¶íØcú½÷ö[.>Ü¿şeôÀıW“»OVuÿìÿ½ƒ_X_ıršı}(Ÿáİs ¦ÖÿEJ,Ñ°º^!½AÒ0Kîí`^±ä\ëÌÃœw-˜7ôÀ°îõıpŸÒîi0ç{Á¼¡†ı‹¤µ€nÎæCÃÃŠbuó0g¨o!=0lAt-sá%‚`É/cœ²àX†§¿öÁ«Á¾
a„yÓ×~0NA°äíb«<oi{&"µî©uL­sn0÷Täsj|Óïı?ö»ü¿æ½¯ïipînÂf¤ÁwJN¿Çñ{vïºşŸGß±gnÁ"L4Ûô¹iõ°Ÿ”´6ÏíƒIşG“×³ÂüÜïa\Ûw"d"çú$‚û°ÍİTº°]„eÇÄt)åÿ@,4ÖBYé™¸$?ìN)Ü?ÛÄ’èU©¶»eUóì§«Ğ'¹Šíy8chÿõ®ÛNPVÈ¯hÌ«éÔ8äKóf¥”>Ú­§ÒîTH{lNj­¯xoF‡ş@c§Çöï»>ìğÁŸOQo‚ûc-»	ú×{A^‡£ é#Æ·
(…Æ±´»!?%İiÚtA‘i÷ÒíñKÿ“:¡4–8¦ÿ÷è	%^)¥şñH=oÃ>=Á&ïÃ\äız\q“Ä)IÖ=*oãİxby-~•\ÛpDÓØ'üãÒ®°wã¬ÿ¦œ#ß­8ïÎŞoí”~œôa».eì‹÷Š8İRÍÛô¢uNòøñÜõàQüšÌ¸EÜ-|ìÃn]ı.™ôÚÂ±ÊRè›èP‡¾#d›BÆ?ê´È%¯TŠ{MÖ$1±“Aú¸ëÀ7î#»ØcK’øĞ@»ñ-9c×I#ù«0f} Uè”·(]P”¦ÅpÔä‹ÓT¹¦yıuzeÕ›—Uÿ²¬jLâÈáx8y¥â{h™Æ yñş×ˆİ4Õ³®ñšŞ›ö;Äˆç É7Ó¸&ILsA*#½–å£ùô›Ş³îı3úíıûrAû
uYšİåØtİ@½Mkù3šr·K‘^]'ÍŞ•ê®“ÿmHÓõ(ÍÜ–™÷²Ì…m.É ²×µÙwF¾-m¼¶4mJ•ûÉ‹Sê=z–×9W6rø>ı‚uËÙ¯Â9©Y{;<K[
sjw“*rŞ°†5¸ã]“*…şJœEéMaî±Ëi»JFú=÷ŒIByzåì—H\ÙÇÏ›™÷fBlƒ-i	’†‚RW†ét+³"6È0ˆ""d°"C!ˆH`	!¤"DDD$KH—%$	R$„¥¤,Ë¶„4«ı{¿o|™¨Iº¥ÙÒşñáşxç{Î¹¿Î“%ì¡¥pöâ„Œ¥ÃpH†½>ü¯>•aÔëQ¯G½^ë%­„Û„©Í š ÷“dö·ïwÜ†ÔùN¢÷,‘§ìß­ÛØİı~Öú3¿ûgà=ÈÁ~PÊ|Âå’SØopÛ÷l—íEpÏ³o~v°¾ï±¬S‡w¿òn¹Ôº%È³‹à¸2Åm‘>2¬k»q-ybygü3oò.yÍ4GğaLp6˜EÆ-Îåğuy$’…"Ù@)Y0í¯ê 
TºRæÕÂ/+ÓTş9ôÖÊCN¯M½ÍUÈ]ŸĞò¤	©E½ãúA)X)rÄ‰½gVáÓªsÂ–oñ„å˜Şã{)hİCşÕ>úøæÄS4Ş©¶æëè;G=Á2û‚z;Ù—ByTÍ¯íAÓ~ˆœï¡üŠü&ÀQğiä¡s”üíM–Cù#‹Ê¼Em@ßÏ9†íÂx_¯¯ãø#Ú%‘'ğë‰¬±ôëk¼	útÜşaêç@ğÀYğ3Ğ
*Àù˜8)pÔ’†}È}|ş{‰¬î¼°„àÜ}w[K¿ü”ûïÅËÿP	ÆÇÉ°±¼%÷ıï;w°.²K!ÆË»rfİ–ö•ß+¸Såá›óG*PŸ}?œ+–·Úä¥ğÿù	™³tÉI¼"d$@Ğ–¹÷³W1>Kì7ßò÷ê>ßî8»üÎçE”7ÁºUï²$ñ?Äİˆ#—İÅ¹rÕ½'g›ÈˆwÜ‰Eg¥
$¢y™‰æC)”ÍZBw¹¾‹(ûÀ…È¤´…g¥ÿ	qä&óZzeÒİŞªŒº2ºÊæ•h…Œdr¾,¹fÀ¢»åT¸[°Ù’s«1÷r¢ÃÒ†œè\$'ÿ         ‚   ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ@ƒÿÿÿÿ  d8 ˆÿÿ@0: ˆÿÿ        x•            †20N    8{G    8&úJ            Ğ$N    €W¨    	 	            Ş!      `              ¨ 0: ˆÿÿ¨ 0: ˆÿÿ                                Ø 0: ˆÿÿØ 0: ˆÿÿè 0: ˆÿÿè 0: ˆÿÿø 0: ˆÿÿø 0: ˆÿÿ0: ˆÿÿ0: ˆÿÿ                       `ƒÿÿÿÿ          0: ˆÿÿ                                    p0: ˆÿÿp0: ˆÿÿ       ˆ0: ˆÿÿˆ0: ˆÿÿ                        @ƒÿÿÿÿÚ      89† ˆÿÿ        Ğ0: ˆÿÿĞ0: ˆÿÿ                        ø0: ˆÿÿø0: ˆÿÿ                                S,                            ÿ  Ğö  x•     ÷Ï     `0: ˆÿÿ`0: ˆÿÿp0: ˆÿÿp0: ˆÿÿ                0: ˆÿÿ0: ˆÿÿ 0: ˆÿÿ 0: ˆÿÿ@0: ˆÿÿC% h
	 ÿ         ‚   ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ@ƒÿÿÿÿ  d8 ˆÿÿ 0: ˆÿÿ        v•            †20N    €\â    8&úJ            Ğ$N    €W¨    	 	                  `              h0: ˆÿÿh0: ˆÿÿ                                ˜0: ˆÿÿ˜0: ˆÿÿ¨0: ˆÿÿ¨0: ˆÿÿ¸0: ˆÿÿ¸0: ˆÿÿÈ0: ˆÿÿÈ0: ˆÿÿ                       `ƒÿÿÿÿ        À0: ˆÿÿ                                    00: ˆÿÿ00: ˆÿÿ       H0: ˆÿÿH0: ˆÿÿ                        @ƒÿÿÿÿÚ      89† ˆÿÿ        0: ˆÿÿ0: ˆÿÿ                        ¸0: ˆÿÿ¸0: ˆÿÿ                                Q,                            ÿ      v•     óÏ      0: ˆÿÿ 0: ˆÿÿ00: ˆÿÿ00: ˆÿÿ    l
	         P0: ˆÿÿP0: ˆÿÿ`0: ˆÿÿ`0: ˆÿÿ€0: ˆÿÿ    & ÿ         ‚   ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ@ƒÿÿÿÿ  d8 ˆÿÿÀ0: ˆÿÿ        w•            †20N    @Ñó    8&úJ            Ğ$N    €W¨    	 	            m	      `              (0: ˆÿÿ(0: ˆÿÿ                                X0: ˆÿÿX0: ˆÿÿh0: ˆÿÿh0: ˆÿÿx0: ˆÿÿx0: ˆÿÿˆ0: ˆÿÿˆ0: ˆÿÿ                       `ƒÿÿÿÿ        €0: ˆÿÿ                                    ğ0: ˆÿÿğ0: ˆÿÿ       0: ˆÿÿ0: ˆÿÿ                        @ƒÿÿÿÿÚ      89† ˆÿÿ        P0: ˆÿÿP0: ˆÿÿ                        x0: ˆÿÿx0: ˆÿÿ                                R,                            ÿ  u
	 w•     õÏ     à0: ˆÿÿà0: ˆÿÿğ0: ˆÿÿğ0: ˆÿÿ    /     ˜, 0: ˆÿÿ0: ˆÿÿ 0: ˆÿÿ 0: ˆÿÿ  0: ˆÿÿ       ÿ         ‚   ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ@ƒÿÿÿÿ  d8 ˆÿÿ€	0: ˆÿÿ        y•            †20N    0±Z    8&úJ            Ğ$N    €W¨          @       Wp      `              è0: ˆÿÿè0: ˆÿÿ                                	0: ˆÿÿ	0: ˆÿÿ(	0: ˆÿÿ(	0: ˆÿÿ8	0: ˆÿÿ8	0: ˆÿÿH	0: ˆÿÿH	0: ˆÿÿ                       `ƒÿÿÿÿ        @0: ˆÿÿ                                    °	0: ˆÿÿ°	0: ˆÿÿ       È	0: ˆÿÿÈ	0: ˆÿÿ                        @ƒÿÿÿÿÚ      89† ˆÿÿ        
0: ˆÿÿ
0: ˆÿÿ                        8
0: ˆÿÿ8
0: ˆÿÿ                                T,                            ÿ  8 y•     ùÏ      
0: ˆÿÿ 
0: ˆÿÿ°
0: ˆÿÿ°
0: ˆÿÿ            à­- Ğ
0: ˆÿÿĞ
0: ˆÿÿà
0: ˆÿÿà
0: ˆÿÿ€0: ˆÿÿ Ø     ÿA         ‚   ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ ƒÿÿÿÿ  d8 ˆÿÿ@0: ˆÿÿ        B–            )¿…N    :0    Ğ$N    Xt/    Ğ$N    Xt/                        `              ¨0: ˆÿÿ¨0: ˆÿÿ                                Ø0: ˆÿÿØ0: ˆÿÿè0: ˆÿÿè0: ˆÿÿø0: ˆÿÿø0: ˆÿÿ0: ˆÿÿ0: ˆÿÿ                       Àƒÿÿÿÿ         0: ˆÿÿ                                    p0: ˆÿÿp0: ˆÿÿ       ˆ0: ˆÿÿˆ0: ˆÿÿ                         «ÿÿÿÿÚ      89† ˆÿÿ        Ğ0: ˆÿÿĞ0: ˆÿÿ                        ø0: ˆÿÿø0: ˆÿÿ                                U,                            ÿA      B–     ÁÒ     `0: ˆÿÿ`0: ˆÿÿp0: ˆÿÿp0: ˆÿÿ            øö  0: ˆÿÿ0: ˆÿÿ 0: ˆÿÿ 0: ˆÿÿÀ0: ˆÿÿ        ÿA         ‚   ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ ƒÿÿÿÿ  d8 ˆÿÿ 0: ˆÿÿ        Y–            )¿…N     Sè0    Ğ$N    <e2    Ğ$N    <e2                        `              h0: ˆÿÿh0: ˆÿÿ                                ˜0: ˆÿÿ˜0: ˆÿÿ¨0: ˆÿÿ¨0: ˆÿÿ¸0: ˆÿÿ¸0: ˆÿÿÈ0: ˆÿÿÈ0: ˆÿÿ                       Àƒÿÿÿÿ        À0: ˆÿÿ                                    00: ˆÿÿ00: ˆÿÿ       H0: ˆÿÿH0: ˆÿÿ                         «ÿÿÿÿÚ      89† ˆÿÿ        0: ˆÿÿ0: ˆÿÿ                        ¸0: ˆÿÿ¸0: ˆÿÿ                                V,                    >2  .   <2 ô..                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       