/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    address.c

Abstract:

    This module contains code which implements the TP_ADDRESS object.
    Routines are provided to create, destroy, reference, and dereference,
    transport address objects.

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#if DBG
#define LpxDbgShowAddr(TNA)\
    { \
        if ((TNA) == NULL) { \
            LpxPrint0("<NetBios broadcast>\n"); \
        } else { \
            LpxPrint6("%c %c %c %c %d (%c)\n", \
                (TNA)->NetbiosName[0], \
                (TNA)->NetbiosName[1], \
                (TNA)->NetbiosName[4], \
                (TNA)->NetbiosName[6], \
                (TNA)->NetbiosName[15], \
                (TNA)->NetbiosNameType + 'A'); \
        } \
    }
#else
#define LpxDbgShowAddr(TNA)
#endif

//
// Map all generic accesses to the same one.
//

STATIC GENERIC_MAPPING AddressGenericMapping =
       { READ_CONTROL, READ_CONTROL, READ_CONTROL, READ_CONTROL };


TDI_ADDRESS_NETBIOS *
LpxParseTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN BOOLEAN BroadcastAddressOk
)

/*++

Routine Description:

    This routine scans a TRANSPORT_ADDRESS, looking for an address
    of type TDI_ADDRESS_TYPE_NETBIOS.

Arguments:

    Transport - The generic TDI address.

    BroadcastAddressOk - TRUE if we should return the broadcast
        address if found. If so, a value of (PVOID)-1 indicates
        the broadcast address.

Return Value:

    A pointer to the Netbios address, or NULL if none is found,
    or (PVOID)-1 if the broadcast address is found.

--*/

{
    TA_ADDRESS * addressName;
    INT i;

    addressName = &TransportAddress->Address[0];

    //
    // The name can be passed with multiple entries; we'll take and use only
    // the Netbios one.
    //

    for (i=0;i<TransportAddress->TAAddressCount;i++) {
        if (addressName->AddressType == TDI_ADDRESS_TYPE_NETBIOS) {
            if ((addressName->AddressLength == 0) &&
                BroadcastAddressOk) {
                return (PVOID)-1;
            } else if (addressName->AddressLength == 
                        sizeof(TDI_ADDRESS_NETBIOS)) {
                return((TDI_ADDRESS_NETBIOS *)(addressName->Address));
            }
        }

        addressName = (TA_ADDRESS *)(addressName->Address +
                                                addressName->AddressLength);
    }
    return NULL;

}   /* LpxParseTdiAddress */


BOOLEAN
LpxValidateTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN ULONG TransportAddressLength
)

/*++

Routine Description:

    This routine scans a TRANSPORT_ADDRESS, verifying that the
    components of the address do not extend past the specified
    length.

Arguments:

    TransportAddress - The generic TDI address.

    TransportAddressLength - The specific length of TransportAddress.

Return Value:

    TRUE if the address is valid, FALSE otherwise.

--*/

{
    PUCHAR AddressEnd = ((PUCHAR)TransportAddress) + TransportAddressLength;
    TA_ADDRESS * addressName;
    INT i;

    if (TransportAddressLength < sizeof(TransportAddress->TAAddressCount)) {
        LpxPrint0 ("LpxValidateTdiAddress: runt address\n");
        return FALSE;
    }

    addressName = &TransportAddress->Address[0];

    for (i=0;i<TransportAddress->TAAddressCount;i++) {
        if (addressName->Address > AddressEnd) {
            LpxPrint0 ("LpxValidateTdiAddress: address too short\n");
            return FALSE;
        }
        addressName = (TA_ADDRESS *)(addressName->Address +
                                                addressName->AddressLength);
    }

    if ((PUCHAR)addressName > AddressEnd) {
        LpxPrint0 ("LpxValidateTdiAddress: address too short\n");
        return FALSE;
    }
    return TRUE;

}   /* LpxValidateTdiAddress */


NTSTATUS
LpxOpenAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine opens a file that points to an existing address object, or, if
    the object doesn't exist, creates it (note that creation of the address
    object includes registering the address, and may take many seconds to
    complete, depending upon system configuration).

    If the address already exists, and it has an ACL associated with it, the
    ACL is checked for access rights before allowing creation of the address.

Arguments:

    DeviceObject - pointer to the device object describing the LPX transport.

    Irp - a pointer to the Irp used for the creation of the address.

    IrpSp - a pointer to the Irp stack location.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS status;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
    PLPX_ADDRESS networkName;    // Network name string.
    PFILE_FULL_EA_INFORMATION ea;
    TRANSPORT_ADDRESS UNALIGNED *name;
    TDI_ADDRESS_NETBIOS *netbiosName;
    ULONG DesiredShareAccess;
    KIRQL oldirql;
    PACCESS_STATE AccessState;
    ACCESS_MASK GrantedAccess;
    BOOLEAN AccessAllowed;
    BOOLEAN QuickAdd = FALSE;

#ifdef __LPX__
	PDEVICE_CONTEXT	providerDeviceContext;
#endif

    DeviceContext = (PDEVICE_CONTEXT)DeviceObject;

    //
    // The network name is in the EA, passed in AssociatedIrp.SystemBuffer
    //

    ea = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    if (ea == NULL) {
        LpxPrint1("OpenAddress: IRP %p has no EA\n", Irp);
        return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    //
    // this may be a valid name; parse the name from the EA and use it if OK.
    //

    name = (TRANSPORT_ADDRESS UNALIGNED *)&ea->EaName[ea->EaNameLength+1];

    if (!LpxValidateTdiAddress(name, ea->EaValueLength)) {
        return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    //
    // The name can have with multiple entries; we'll use the Netbios one.
    // This call returns NULL if not Netbios address is found, (PVOID)-1
    // if it is the broadcast address, and a pointer to a Netbios
    // address otherwise.
    //

    netbiosName = LpxParseTdiAddress(name, TRUE);

    if (netbiosName != NULL) {
        if (netbiosName != (PVOID)-1) {
            networkName = (PLPX_ADDRESS)ExAllocatePoolWithTag (
                                                NonPagedPool,
                                                sizeof (LPX_ADDRESS),
                                                LPX_MEM_TAG_NETBIOS_NAME);
            if (networkName == NULL) {
                PANIC ("LpxOpenAddress: PANIC! could not allocate networkName!\n");
                LpxWriteResourceErrorLog(
                    DeviceContext,
                    EVENT_TRANSPORT_RESOURCE_POOL,
                    1,
                    sizeof(TA_NETBIOS_ADDRESS),
                    ADDRESS_RESOURCE_ID);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // get the name to local storage
            //
#ifdef __LPX__

			QuickAdd = TRUE;

			RtlZeroMemory( networkName, sizeof(*networkName) );
            RtlCopyMemory( networkName->Node, netbiosName->NetbiosName, ETHERNET_ADDRESS_LENGTH );
			networkName->Port = netbiosName->NetbiosNameType;

#endif

		} else {
            networkName = NULL;
        }

    } else {
        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
            LpxPrint1("OpenAddress: IRP %p has no NETBIOS address\n", Irp);
        }
        return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    IF_LPXDBG (LPX_DEBUG_ADDRESS) {
        LpxPrint1 ("OpenAddress %s: ",
            ((IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
             (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                   "shared" : "exclusive");
        //LpxDbgShowAddr (networkName);
    }

#ifdef __LPX__

	//
	//	find the right device context for the networkName
	//	because we request open-address via SocketLpx device context
	//
	//	@hootch@ 09112003
	//

	ASSERT( DeviceContext == SocketLpxDeviceContext );

	providerDeviceContext = SocketLpxFindDeviceContext( networkName );
	
	if (providerDeviceContext == NULL) {

		if (networkName)
			ExFreePool( networkName );

		status = STATUS_INVALID_ADDRESS_COMPONENT;
		DebugPrint(1, ("[Lpx] LpxOpenAddress: couldn't find providerDeviceContext\n") );
		return	status;
	}

	DebugPrint( 2, ("[Lpx] LpxOpenAddress: DeviceContext is changed into providerDeviceContext\n") );

	DeviceContext = providerDeviceContext;

#endif

    //
    // get an address file structure to represent this address.
    //

    status = LpxCreateAddressFile (DeviceContext, &addressFile);

    if (!NT_SUCCESS (status)) {
        if (networkName != NULL) {
            ExFreePool (networkName);
        }
        return status;
    }

    //
    // See if this address is already established.  This call automatically
    // increments the reference count on the address so that it won't disappear
    // from underneath us after this call but before we have a chance to use it.
    //
    // To ensure that we don't create two address objects for the
    // same address, we hold the device context AddressResource until
    // we have found the address or created a new one.
    //

    ACQUIRE_RESOURCE_EXCLUSIVE (&DeviceContext->AddressResource, TRUE);

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

#ifdef __LPX__

	if (networkName->Port == 0) {

		status = LpxAssignPort( DeviceContext, networkName );
		
		if (!NT_SUCCESS(status)) {

	        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
			RELEASE_RESOURCE (&DeviceContext->AddressResource);

	        LpxDereferenceAddressFile( addressFile );

			DebugPrint( 3, ("[Lpx] LpxOpenAddress: another process has taken specified address!\n") );

			return status;
		}
	}

    address = LpxLookupAddress( DeviceContext, networkName );

	if (address != NULL) {
		
		RELEASE_SPIN_LOCK( &DeviceContext->SpinLock, oldirql );

		RELEASE_RESOURCE( &DeviceContext->AddressResource );

        LpxDereferenceAddress( "lookup", address, AREF_LOOKUP );

        LpxDereferenceAddressFile( addressFile );

        if (networkName != NULL) {

            ExFreePool( networkName );
        }

		DebugPrint( 3, ("[Lpx] !!!!!LpxOpenAddress %p: another process has taken specified address!\n", address) );

		status = STATUS_ADDRESS_ALREADY_EXISTS;
		return status;
	}

#endif

    if (address == NULL) {

        //
        // This address doesn't exist. Create it, and start the process of
        // registering it.
        //

        status = LpxCreateAddress (
                    DeviceContext,
                    networkName,
                    &address);

        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

        if (NT_SUCCESS (status)) {

            //
            // Initialize the shared access now. We use read access
            // to control all access.
            //

            DesiredShareAccess = (ULONG)
                (((IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                  (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                        FILE_SHARE_READ : 0);

            IoSetShareAccess(
                FILE_READ_DATA,
                DesiredShareAccess,
                IrpSp->FileObject,
                &address->u.ShareAccess);


            //
            // Assign the security descriptor (need to do this with
            // the spinlock released because the descriptor is not
            // mapped. Need to synchronize Assign and Access).
            //

            AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

            status = SeAssignSecurity(
                         NULL,                       // parent descriptor
                         AccessState->SecurityDescriptor,
                         &address->SecurityDescriptor,
                         FALSE,                      // is directory
                         &AccessState->SubjectSecurityContext,
                         &AddressGenericMapping,
                         PagedPool);

            IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                LpxPrint3 ("Assign security A %p AF %p, status %lx\n",
                               address,
                               addressFile,
                               status);
            }

            if (!NT_SUCCESS(status)) {

                //
                // Error, return status.
                //
                IoRemoveShareAccess (IrpSp->FileObject, &address->u.ShareAccess);

                // Mark as stopping so that someone does not ref it again
                ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);
                address->Flags |= ADDRESS_FLAGS_STOPPING;
                RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

                RELEASE_RESOURCE (&DeviceContext->AddressResource);
                LpxDereferenceAddress ("Device context stopping", address, AREF_TEMP_CREATE);
                LpxDereferenceAddressFile (addressFile);
                return status;

            }

            RELEASE_RESOURCE (&DeviceContext->AddressResource);

            //
            // if the adapter isn't ready, we can't do any of this; get out
            //

            if (DeviceContext->State != DEVICECONTEXT_STATE_OPEN) {

                IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                    LpxPrint3("OpenAddress A %p AF %p: DeviceContext %p not open\n",
                        address,
                        addressFile,
                        DeviceContext);
                }
                LpxDereferenceAddressFile (addressFile);
                status = STATUS_DEVICE_NOT_READY;

            } else {

                IrpSp->FileObject->FsContext = (PVOID)addressFile;
                IrpSp->FileObject->FsContext2 =
                                    (PVOID)TDI_TRANSPORT_ADDRESS_FILE;
                addressFile->FileObject = IrpSp->FileObject;
                addressFile->Irp = Irp;
                addressFile->Address = address;

                LpxReferenceAddress("Opened new", address, AREF_OPEN);

                IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                    LpxPrint2("OpenAddress A %p AF %p: created.\n",
                        address,
                        addressFile);
                }

                ExInterlockedInsertTailList(
                    &address->AddressFileDatabase,
                    &addressFile->Linkage,
                    &address->SpinLock);


                //
                // Begin address registration unless this is the broadcast
                // address (which is a "fake" address with no corresponding
                // Netbios address) or the reserved address, which we know
                // is unique since it is based on the adapter address.
                //
                // Also, for "quick" add names, do not register.
                //

                address->Flags &= ~ADDRESS_FLAGS_NEEDS_REG;
                addressFile->Irp = NULL;
                addressFile->State = ADDRESSFILE_STATE_OPEN;
                status = STATUS_SUCCESS;
            }

            LpxDereferenceAddress("temp create", address, AREF_TEMP_CREATE);

        } else {

            RELEASE_RESOURCE (&DeviceContext->AddressResource);

            //
            // If the address could not be created, and is not in the process of
            // being created, then we can't open up an address.
            //

            if (networkName != NULL) {
                ExFreePool (networkName);
            }

            LpxDereferenceAddressFile (addressFile);

        }

    } else {

        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

        //
        // The address already exists.  Check the ACL and see if we
        // can access it.  If so, simply use this address as our address.
        //

        AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

        AccessAllowed = SeAccessCheck(
                            address->SecurityDescriptor,
                            &AccessState->SubjectSecurityContext,
                            FALSE,                  // lock tokens
                            IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                            (ACCESS_MASK)0,         // previously granted
                            NULL,                   // privileges
                            &AddressGenericMapping,
                            Irp->RequestorMode,
                            &GrantedAccess,
                            &status);
                            
        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
            LpxPrint4 ("Access check A %p AF %p, %s (%lx)\n",
                           address,
                           addressFile,
                           AccessAllowed ? "allowed" : "not allowed",
                           status);
        }

        if (AccessAllowed) {

            //
            // Access was successful, make sure Status is right.
            //

            status = STATUS_SUCCESS;

            // Transfer the access masks from what is desired to what is granted
            AccessState->PreviouslyGrantedAccess |= GrantedAccess;
            AccessState->RemainingDesiredAccess &= ~(GrantedAccess | MAXIMUM_ALLOWED);

            //
            // Compare DesiredAccess to GrantedAccess?
            //

#if 0
            //
            // Check that the name is of the correct type (unique vs. group)
            // We don't need to check this for the broadcast address.
            //
            // This code is structured funny, the only reason
            // this is inside this if is to avoid indenting too much.
            //

            if (networkName != NULL) {
                if (address->NetworkName->NetbiosNameType !=
                    networkName->NetbiosNameType) {

                    IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                        LpxPrint2 ("Address types differ: old %c, new %c\n",
                            address->NetworkName->NetbiosNameType + 'A',
                            networkName->NetbiosNameType + 'A');
                    }

                    status = STATUS_DUPLICATE_NAME;

                }
            }
#endif
        }


        if (!NT_SUCCESS (status)) {

            RELEASE_RESOURCE (&DeviceContext->AddressResource);

            IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                LpxPrint2("OpenAddress A %p AF %p: ACL bad.\n",
                    address,
                    addressFile);
            }

            LpxDereferenceAddressFile (addressFile);

        } else {

            //
            // Now check that we can obtain the desired share
            // access. We use read access to control all access.
            //

            DesiredShareAccess = (ULONG)
                (((IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                  (IrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                        FILE_SHARE_READ : 0);

            status = IoCheckShareAccess(
                         FILE_READ_DATA,
                         DesiredShareAccess,
                         IrpSp->FileObject,
                         &address->u.ShareAccess,
                         TRUE);

            if (!NT_SUCCESS (status)) {

                RELEASE_RESOURCE (&DeviceContext->AddressResource);

                IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                    LpxPrint2("OpenAddress A %p AF %p: ShareAccess problem.\n",
                        address,
                        addressFile);
                }

                LpxDereferenceAddressFile (addressFile);

            } else {

                RELEASE_RESOURCE (&DeviceContext->AddressResource);

                ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

                //
                // now, if the address registered, we simply return success after
                // pointing the file object at the address file (which points to
                // the address). If the address registration is pending, we mark
                // the registration pending and let the registration completion
                // routine complete the open. If the address is bad, we simply
                // fail the open.
                //

                if ((address->Flags &
                       (ADDRESS_FLAGS_CONFLICT |
                        ADDRESS_FLAGS_REGISTERING |
                        ADDRESS_FLAGS_DEREGISTERING |
                        ADDRESS_FLAGS_DUPLICATE_NAME |
                        ADDRESS_FLAGS_NEEDS_REG |
                        ADDRESS_FLAGS_STOPPING |
                        ADDRESS_FLAGS_BAD_ADDRESS |
                        ADDRESS_FLAGS_CLOSED)) == 0) {

                    InsertTailList (
                        &address->AddressFileDatabase,
                        &addressFile->Linkage);

                    addressFile->Irp = NULL;
                    addressFile->Address = address;
                    addressFile->FileObject = IrpSp->FileObject;
                    addressFile->State = ADDRESSFILE_STATE_OPEN;

                    LpxReferenceAddress("open ready", address, AREF_OPEN);

                    IrpSp->FileObject->FsContext = (PVOID)addressFile;
                    IrpSp->FileObject->FsContext2 =
                                            (PVOID)TDI_TRANSPORT_ADDRESS_FILE;

                    RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

                    IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                        LpxPrint2("OpenAddress A %p AF %p: address ready.\n",
                            address,
                            addressFile);
                    }

                    status = STATUS_SUCCESS;

                } else {

                    //
                    // if the address is still registering, make the open pending.
                    //

                    if ((address->Flags & (ADDRESS_FLAGS_REGISTERING | ADDRESS_FLAGS_NEEDS_REG)) != 0) {

                        InsertTailList (
                            &address->AddressFileDatabase,
                            &addressFile->Linkage);

                        addressFile->Irp = Irp;
                        addressFile->Address = address;
                        addressFile->FileObject = IrpSp->FileObject;

                        LpxReferenceAddress("open registering", address, AREF_OPEN);

                        IrpSp->FileObject->FsContext = (PVOID)addressFile;
                        IrpSp->FileObject->FsContext2 =
                                    (PVOID)TDI_TRANSPORT_ADDRESS_FILE;

                        RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

                        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                            LpxPrint2("OpenAddress A %p AF %p: address registering.\n",
                                address,
                                addressFile);
                        }

                        status = STATUS_PENDING;

                    } else {

                        if ((address->Flags & ADDRESS_FLAGS_CONFLICT) != 0) {
                            status = STATUS_DUPLICATE_NAME;
                        } else {
                            status = STATUS_DRIVER_INTERNAL_ERROR;
                        }

                        RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

                        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
                            LpxPrint3("OpenAddress A %p AF %p: address flags %lx.\n",
                                address,
                                addressFile,
                                address->Flags);
                        }

                        LpxDereferenceAddressFile (addressFile);

                    }
                }
            }
        }


        //
        // This isn't needed since it was not used in the
        // creation of the address.
        //

        if (networkName != NULL) {
            ExFreePool (networkName);
        }

        //
        // Remove the reference from LpxLookupAddress.
        //

        LpxDereferenceAddress ("Done opening", address, AREF_LOOKUP);
    }

    return status;
} /* LpxOpenAddress */


VOID
LpxAllocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS *TransportAddress
    )

/*++

Routine Description:

    This routine allocates storage for a transport address. Some minimal
    initialization is done on the address.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    Address - Pointer to a place where this routine will return a pointer
        to a transport address structure. Returns NULL if no storage
        can be allocated.

Return Value:

    None.

--*/

{
    PTP_ADDRESS Address;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_ADDRESS)) >
                DeviceContext->MemoryLimit)) {
        PANIC("LPX: Could not allocate address: limit\n");
        LpxWriteResourceErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_RESOURCE_LIMIT,
            101,
            sizeof(TP_ADDRESS),
            ADDRESS_RESOURCE_ID);
        *TransportAddress = NULL;
        return;
    }

    Address = (PTP_ADDRESS)ExAllocatePoolWithTag (
                               NonPagedPool,
                               sizeof (TP_ADDRESS),
                               LPX_MEM_TAG_TP_ADDRESS);
    if (Address == NULL) {
        PANIC("LPX: Could not allocate address: no pool\n");
        LpxWriteResourceErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_RESOURCE_POOL,
            201,
            sizeof(TP_ADDRESS),
            ADDRESS_RESOURCE_ID);
        *TransportAddress = NULL;
        return;
    }
    RtlZeroMemory (Address, sizeof(TP_ADDRESS));

    DeviceContext->MemoryUsage += sizeof(TP_ADDRESS);

    ++DeviceContext->AddressAllocated;

    Address->Type = LPX_ADDRESS_SIGNATURE;
    Address->Size = sizeof (TP_ADDRESS);

    Address->Provider = DeviceContext;
    KeInitializeSpinLock (&Address->SpinLock);
//      KeInitializeSpinLock (&Address->Interlock);

    InitializeListHead (&Address->ConnectionDatabase);
    InitializeListHead (&Address->AddressFileDatabase);
    InitializeListHead (&Address->SendDatagramQueue);

    *TransportAddress = Address;

}   /* LpxAllocateAddress */


VOID
LpxDeallocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS TransportAddress
    )

/*++

Routine Description:

    This routine frees storage for a transport address.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    Address - Pointer to a transport address structure.

Return Value:

    None.

--*/

{
    ExFreePool (TransportAddress);
    --DeviceContext->AddressAllocated;

    DeviceContext->MemoryUsage -= sizeof(TP_ADDRESS);

}   /* LpxDeallocateAddress */


NTSTATUS
LpxCreateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PLPX_ADDRESS NetworkName,
    OUT PTP_ADDRESS *Address
    )

/*++

Routine Description:

    This routine creates a transport address and associates it with
    the specified transport device context.  The reference count in the
    address is automatically set to 1, and the reference count of the
    device context is incremented.

    NOTE: This routine must be called with the DeviceContext
    spinlock held.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    NetworkName - Pointer to an LPX_ADDRESS type containing the network
        name to be associated with this address, if any.
        NOTE: This has only the basic NetbiosNameType values, not the
              QUICK_ ones.

    Address - Pointer to a place where this routine will return a pointer
        to a transport address structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_ADDRESS pAddress;
    PLIST_ENTRY p;


    p = RemoveHeadList (&DeviceContext->AddressPool);
    if (p == &DeviceContext->AddressPool) {

        if ((DeviceContext->AddressMaxAllocated == 0) ||
            (DeviceContext->AddressAllocated < DeviceContext->AddressMaxAllocated)) {

            LpxAllocateAddress (DeviceContext, &pAddress);
            IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
                LpxPrint1 ("LPX: Allocated address at %p\n", pAddress);
            }

        } else {

            LpxWriteResourceErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_RESOURCE_SPECIFIC,
                401,
                sizeof(TP_ADDRESS),
                ADDRESS_RESOURCE_ID);
            pAddress = NULL;

        }

        if (pAddress == NULL) {
            ++DeviceContext->AddressExhausted;
            PANIC ("LpxCreateAddress: Could not allocate address object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        pAddress = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);

    }

    ++DeviceContext->AddressInUse;
    if (DeviceContext->AddressInUse > DeviceContext->AddressMaxInUse) {
        ++DeviceContext->AddressMaxInUse;
    }

    DeviceContext->AddressTotal += DeviceContext->AddressInUse;
    ++DeviceContext->AddressSamples;


    IF_LPXDBG (LPX_DEBUG_ADDRESS | LPX_DEBUG_UFRAMES) {
        LpxPrint1 ("LpxCreateAddress %p: ", pAddress);
        //LpxDbgShowAddr (NetworkName);
    }

    //
    // Initialize all of the static data for this address.
    //

    pAddress->ReferenceCount = 1;

#if DBG
    {
        UINT Counter;
        for (Counter = 0; Counter < NUMBER_OF_AREFS; Counter++) {
            pAddress->RefTypes[Counter] = 0;
        }

        // This reference is removed by the caller.

        pAddress->RefTypes[AREF_TEMP_CREATE] = 1;
    }
#endif

    pAddress->Flags = ADDRESS_FLAGS_NEEDS_REG;
    InitializeListHead (&pAddress->AddressFileDatabase);

    pAddress->NetworkName = NetworkName;

#if 0
	if ((NetworkName != (PLPX_ADDRESS)NULL) &&
        (NetworkName->NetbiosNameType ==
           TDI_ADDRESS_NETBIOS_TYPE_GROUP)) {

        pAddress->Flags |= ADDRESS_FLAGS_GROUP;

    }
    if (NetworkName != (PLPX_ADDRESS)NULL) {
        ++DeviceContext->AddressCounts[NetworkName->NetbiosName[0]];
    }
#endif
    //
    // Now link this address into the specified device context's
    // address database.  To do this, we need to acquire the spin lock
    // on the device context.
    //

    InsertTailList (&DeviceContext->AddressDatabase, &pAddress->Linkage);
    pAddress->Provider = DeviceContext;
    LpxReferenceDeviceContext ("Create Address", DeviceContext, DCREF_ADDRESS);   // count refs to the device context.

    *Address = pAddress;                // return the address.
    return STATUS_SUCCESS;              // not finished yet.
} /* LpxCreateAddress */


NTSTATUS
LpxVerifyAddressObject (
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine is called to verify that the pointer given us in a file
    object is in fact a valid address file object. We also verify that the
    address object pointed to by it is a valid address object, and reference
    it to keep it from disappearing while we use it.

Arguments:

    AddressFile - potential pointer to a TP_ADDRESS_FILE object

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INVALID_ADDRESS otherwise

--*/

{
    KIRQL oldirql;
    NTSTATUS status = STATUS_SUCCESS;
    PTP_ADDRESS address;

    //
    // try to verify the address file signature. If the signature is valid,
    // verify the address pointed to by it and get the address spinlock.
    // check the address's state, and increment the reference count if it's
    // ok to use it. Note that the only time we return an error for state is
    // if the address is closing.
    //

    try {

        if ((AddressFile != (PTP_ADDRESS_FILE)NULL) &&
            (AddressFile->Size == sizeof (TP_ADDRESS_FILE)) &&
            (AddressFile->Type == LPX_ADDRESSFILE_SIGNATURE) ) {
//            (AddressFile->State != ADDRESSFILE_STATE_CLOSING) ) {

            address = AddressFile->Address;

            if ((address != (PTP_ADDRESS)NULL) &&
                (address->Size == sizeof (TP_ADDRESS)) &&
                (address->Type == LPX_ADDRESS_SIGNATURE)    ) {

                ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

                if ((address->Flags & ADDRESS_FLAGS_STOPPING) == 0) {

                    LpxReferenceAddress ("verify", address, AREF_VERIFY);

                } else {

                    LpxPrint1("LpxVerifyAddress: A %p closing\n", address);
                    status = STATUS_INVALID_ADDRESS;
                }

                RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

            } else {

                LpxPrint1("LpxVerifyAddress: A %p bad signature\n", address);
                status = STATUS_INVALID_ADDRESS;
            }

        } else {

            LpxPrint1("LpxVerifyAddress: AF %p bad signature\n", AddressFile);
            status = STATUS_INVALID_ADDRESS;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

         LpxPrint1("LpxVerifyAddress: AF %p exception\n", address);
         return GetExceptionCode();
    }

    return status;

}

VOID
LpxDestroyAddress(
    IN PVOID Parameter
    )

/*++

Routine Description:

    This routine destroys a transport address and removes all references
    made by it to other objects in the transport.  The address structure
    is returned to nonpaged system pool or our lookaside list. It is assumed
    that the caller has already removed all addressfile structures associated
    with this address.

    The routine is called from a worker thread so that the security
    descriptor can be accessed.

    This worked thread is only queued by LpxDerefAddress.  The reason
    for this is that there may be multiple streams of execution which are
    simultaneously referencing the same address object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    Address - Pointer to a transport address structure to be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PDEVICE_CONTEXT DeviceContext;
    PTP_ADDRESS Address = (PTP_ADDRESS)Parameter;

    IF_LPXDBG (LPX_DEBUG_ADDRESS) {
        LpxPrint1 ("LpxDestroyAddress %p:.\n", Address);
    }

    DeviceContext = Address->Provider;

    SeDeassignSecurity (&Address->SecurityDescriptor);

    //
    // Delink this address from its associated device context's address
    // database.  To do this we must spin lock on the device context object,
    // not on the address.
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);
#if 0
    if (Address->NetworkName) {
        --DeviceContext->AddressCounts[Address->NetworkName->NetbiosName[0]];
    }
#endif
    RemoveEntryList (&Address->Linkage);

    if (Address->NetworkName != NULL) {
        ExFreePool (Address->NetworkName);
        Address->NetworkName = NULL;
    }

    //
    // Now we can deallocate the transport address object.
    //

    DeviceContext->AddressTotal += DeviceContext->AddressInUse;
    ++DeviceContext->AddressSamples;
    --DeviceContext->AddressInUse;

    if ((DeviceContext->AddressAllocated - DeviceContext->AddressInUse) >
            DeviceContext->AddressInitAllocated) {
        LpxDeallocateAddress (DeviceContext, Address);
        IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
            LpxPrint1 ("LPX: Deallocated address at %p\n", Address);
        }
    } else {
        InsertTailList (&DeviceContext->AddressPool, &Address->Linkage);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
    LpxDereferenceDeviceContext ("Destroy Address", DeviceContext, DCREF_ADDRESS);  // just housekeeping.

} /* LpxDestroyAddress */


#if DBG
VOID
LpxRefAddress(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine increments the reference count on a transport address.

Arguments:

    Address - Pointer to a transport address object.

Return Value:

    none.

--*/

{

    ASSERT (Address->ReferenceCount > 0);    // not perfect, but...

    (VOID)InterlockedIncrement (&Address->ReferenceCount);

} /* LpxRefAddress */
#endif


VOID
LpxDerefAddress(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine dereferences a transport address by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    LpxDestroyAddress to remove it from the system.

Arguments:

    Address - Pointer to a transport address object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&Address->ReferenceCount);

    //
    // If we have deleted all references to this address, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the address any longer.
    //

    ASSERT (result >= 0);

    if (result == 0) {

        ASSERT ((Address->Flags & ADDRESS_FLAGS_STOPPING) != 0);
        
        ExInitializeWorkItem(
            &Address->u.DestroyAddressQueueItem,
            LpxDestroyAddress,
            (PVOID)Address);
        ExQueueWorkItem(&Address->u.DestroyAddressQueueItem, DelayedWorkQueue);
    }
} /* LpxDerefAddress */



VOID
LpxAllocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE *TransportAddressFile
    )

/*++

Routine Description:

    This routine allocates storage for an address file. Some
    minimal initialization is done on the object.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    TransportAddressFile - Pointer to a place where this routine will return
        a pointer to a transport address file structure. It returns NULL if no
        storage can be allocated.

Return Value:

    None.

--*/

{

    PTP_ADDRESS_FILE AddressFile;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + sizeof(TP_ADDRESS_FILE)) >
                DeviceContext->MemoryLimit)) {
        PANIC("LPX: Could not allocate address file: limit\n");
        LpxWriteResourceErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_RESOURCE_LIMIT,
            102,
            sizeof(TP_ADDRESS_FILE),
            ADDRESS_FILE_RESOURCE_ID);
        *TransportAddressFile = NULL;
        return;
    }

    AddressFile = (PTP_ADDRESS_FILE)ExAllocatePoolWithTag (
                                        NonPagedPool,
                                        sizeof (TP_ADDRESS_FILE),
                                        LPX_MEM_TAG_TP_ADDRESS_FILE);
    if (AddressFile == NULL) {
        PANIC("LPX: Could not allocate address file: no pool\n");
        LpxWriteResourceErrorLog(
            DeviceContext,
            EVENT_TRANSPORT_RESOURCE_POOL,
            202,
            sizeof(TP_ADDRESS_FILE),
            ADDRESS_FILE_RESOURCE_ID);
        *TransportAddressFile = NULL;
        return;
    }
    RtlZeroMemory (AddressFile, sizeof(TP_ADDRESS_FILE));

    DeviceContext->MemoryUsage += sizeof(TP_ADDRESS_FILE);
    ++DeviceContext->AddressFileAllocated;

    AddressFile->Type = LPX_ADDRESSFILE_SIGNATURE;
    AddressFile->Size = sizeof (TP_ADDRESS_FILE);

    InitializeListHead (&AddressFile->ReceiveDatagramQueue);
    InitializeListHead (&AddressFile->ConnectionDatabase);

    *TransportAddressFile = AddressFile;

}   /* LpxAllocateAddressFile */


VOID
LpxDeallocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS_FILE TransportAddressFile
    )

/*++

Routine Description:

    This routine frees storage for an address file.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    TransportAddressFile - Pointer to a transport address file structure.

Return Value:

    None.

--*/

{

    ExFreePool (TransportAddressFile);
    --DeviceContext->AddressFileAllocated;
    DeviceContext->MemoryUsage -= sizeof(TP_ADDRESS_FILE);

}   /* LpxDeallocateAddressFile */


NTSTATUS
LpxCreateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE * AddressFile
    )

/*++

Routine Description:

    This routine creates an address file from the pool of ther
    specified device context. The reference count in the
    address is automatically set to 1.

Arguments:

    DeviceContext - Pointer to the device context (which is really just
        the device object with its extension) to be associated with the
        address.

    AddressFile - Pointer to a place where this routine will return a pointer
        to a transport address file structure.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PLIST_ENTRY p;
    PTP_ADDRESS_FILE addressFile;

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    p = RemoveHeadList (&DeviceContext->AddressFilePool);
    if (p == &DeviceContext->AddressFilePool) {

        if ((DeviceContext->AddressFileMaxAllocated == 0) ||
            (DeviceContext->AddressFileAllocated < DeviceContext->AddressFileMaxAllocated)) {

            LpxAllocateAddressFile (DeviceContext, &addressFile);
            IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
                LpxPrint1 ("LPX: Allocated address file at %p\n", addressFile);
            }

        } else {

            LpxWriteResourceErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_RESOURCE_SPECIFIC,
                402,
                sizeof(TP_ADDRESS_FILE),
                ADDRESS_FILE_RESOURCE_ID);
            addressFile = NULL;

        }

        if (addressFile == NULL) {
            ++DeviceContext->AddressFileExhausted;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
            PANIC ("LpxCreateAddressFile: Could not allocate address file object!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

    } else {

        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

    }

    ++DeviceContext->AddressFileInUse;
    if (DeviceContext->AddressFileInUse > DeviceContext->AddressFileMaxInUse) {
        ++DeviceContext->AddressFileMaxInUse;
    }

    DeviceContext->AddressFileTotal += DeviceContext->AddressFileInUse;
    ++DeviceContext->AddressFileSamples;

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    InitializeListHead (&addressFile->ConnectionDatabase);
    addressFile->Address = NULL;
    addressFile->FileObject = NULL;
    addressFile->Provider = DeviceContext;
    addressFile->State = ADDRESSFILE_STATE_OPENING;
    addressFile->ConnectIndicationInProgress = FALSE;
    addressFile->ReferenceCount = 1;
    addressFile->CloseIrp = (PIRP)NULL;

    //
    // Initialize the request handlers.
    //

    addressFile->RegisteredConnectionHandler = FALSE;
    addressFile->ConnectionHandler = TdiDefaultConnectHandler;
    addressFile->ConnectionHandlerContext = NULL;
    addressFile->RegisteredDisconnectHandler = FALSE;
    addressFile->DisconnectHandler = TdiDefaultDisconnectHandler;
    addressFile->DisconnectHandlerContext = NULL;
    addressFile->RegisteredReceiveHandler = FALSE;
    addressFile->ReceiveHandler = TdiDefaultReceiveHandler;
    addressFile->ReceiveHandlerContext = NULL;
    addressFile->RegisteredReceiveDatagramHandler = FALSE;
    addressFile->ReceiveDatagramHandler = TdiDefaultRcvDatagramHandler;
    addressFile->ReceiveDatagramHandlerContext = NULL;
    addressFile->RegisteredExpeditedDataHandler = FALSE;
    addressFile->ExpeditedDataHandler = TdiDefaultRcvExpeditedHandler;
    addressFile->ExpeditedDataHandlerContext = NULL;
    addressFile->RegisteredErrorHandler = FALSE;
    addressFile->ErrorHandler = TdiDefaultErrorHandler;
    addressFile->ErrorHandlerContext = NULL;


    *AddressFile = addressFile;
    return STATUS_SUCCESS;

} /* LpxCreateAddress */


NTSTATUS
LpxDestroyAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine destroys an address file and removes all references
    made by it to other objects in the transport.

    This routine is only called by LpxDereferenceAddressFile. The reason
    for this is that there may be multiple streams of execution which are
    simultaneously referencing the same address file object, and it should
    not be deleted out from under an interested stream of execution.

Arguments:

    AddressFile Pointer to a transport address file structure to be destroyed.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql1;
    PTP_ADDRESS address;
    PDEVICE_CONTEXT DeviceContext;
    PIRP CloseIrp;


    address = AddressFile->Address;
    DeviceContext = AddressFile->Provider;

    if (address) {

        //
        // This addressfile was associated with an address.
        //

        ACQUIRE_SPIN_LOCK (&address->SpinLock, &oldirql);

        //
        // remove this addressfile from the address list and disassociate it from
        // the file handle.
        //

        RemoveEntryList (&AddressFile->Linkage);
        InitializeListHead (&AddressFile->Linkage);

        if (address->AddressFileDatabase.Flink == &address->AddressFileDatabase) {

            //
            // This is the last open of this address, it will close
            // due to normal dereferencing but we have to set the
            // CLOSING flag too to stop further references.
            //

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql1);
            address->Flags |= ADDRESS_FLAGS_STOPPING;
            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql1);

        }

        AddressFile->Address = NULL;

        AddressFile->FileObject->FsContext = NULL;
        AddressFile->FileObject->FsContext2 = NULL;

        RELEASE_SPIN_LOCK (&address->SpinLock, oldirql);

        //
        // We will already have been removed from the ShareAccess
        // of the owning address.
        //

        //
        // Now dereference the owning address.
        //

        LpxDereferenceAddress ("Close", address, AREF_OPEN);    // remove the creation hold

    }

    //
    // Save this for later completion.
    //

    CloseIrp = AddressFile->CloseIrp;

    //
    // return the addressFile to the pool of address files
    //

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    DeviceContext->AddressFileTotal += DeviceContext->AddressFileInUse;
    ++DeviceContext->AddressFileSamples;
    --DeviceContext->AddressFileInUse;

    if ((DeviceContext->AddressFileAllocated - DeviceContext->AddressFileInUse) >
            DeviceContext->AddressFileInitAllocated) {
        LpxDeallocateAddressFile (DeviceContext, AddressFile);
        IF_LPXDBG (LPX_DEBUG_DYNAMIC) {
            LpxPrint1 ("LPX: Deallocated address file at %p\n", AddressFile);
        }
    } else {
        InsertTailList (&DeviceContext->AddressFilePool, &AddressFile->Linkage);
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);


    if (CloseIrp != (PIRP)NULL) {
        CloseIrp->IoStatus.Information = 0;
        CloseIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest (CloseIrp, IO_NETWORK_INCREMENT);
    }

    return STATUS_SUCCESS;

} /* LpxDestroyAddressFile */


VOID
LpxReferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine increments the reference count on an address file.

Arguments:

    AddressFile - Pointer to a transport address file object.

Return Value:

    none.

--*/

{

    ASSERT (AddressFile->ReferenceCount > 0);   // not perfect, but...

    (VOID)InterlockedIncrement (&AddressFile->ReferenceCount);

} /* LpxReferenceAddressFile */


VOID
LpxDereferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    )

/*++

Routine Description:

    This routine dereferences an address file by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    LpxDestroyAddressFile to remove it from the system.

Arguments:

    AddressFile - Pointer to a transport address file object.

Return Value:

    none.

--*/

{
    LONG result;

    result = InterlockedDecrement (&AddressFile->ReferenceCount);

    //
    // If we have deleted all references to this address file, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the address any longer.
    //

    ASSERT (result >= 0);

    if (result == 0) {
        LpxDestroyAddressFile (AddressFile);
    }
} /* LpxDerefAddressFile */


VOID
LpxStopAddress(
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine is called to terminate all activity on an address and
    destroy the object.  This is done in a graceful manner; i.e., all
    outstanding addressfiles are removed from the addressfile database, and
    all their activities are shut down.

Arguments:

    Address - Pointer to a TP_ADDRESS object.

Return Value:

    none.

--*/

{
    KIRQL oldirql, oldirql1;
    PTP_ADDRESS_FILE addressFile;
    PLIST_ENTRY p;
    PDEVICE_CONTEXT DeviceContext;

    DeviceContext = Address->Provider;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    //
    // If we're already stopping this address, then don't try to do it again.
    //

    if (!(Address->Flags & ADDRESS_FLAGS_STOPPING)) {

        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
            LpxPrint1 ("LpxStopAddress %p: stopping\n", Address);
        }

        LpxReferenceAddress ("Stopping", Address, AREF_TEMP_STOP);

        ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql1);
        Address->Flags |= ADDRESS_FLAGS_STOPPING;
        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql1);

        //
        // Run down all addressfiles on this address. This
        // will leave the address with no references
        // potentially, but we don't need a temp one
        // because every place that calls LpxStopAddress
        // already has a temp reference.
        //

        while (!IsListEmpty (&Address->AddressFileDatabase)) {
            p = RemoveHeadList (&Address->AddressFileDatabase);
            addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);

            addressFile->Address = NULL;
#if 0
            addressFile->FileObject->FsContext = NULL;
            addressFile->FileObject->FsContext2 = NULL;
#endif

            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

            //
            // Run-down this addressFile without the lock on.
            // We don't care about removing ourselves from
            // the address' ShareAccess because we are
            // tearing it down.
            //

            LpxStopAddressFile (addressFile, Address);

            //
            // return the addressFile to the pool of address files
            //

            LpxDereferenceAddressFile (addressFile);

            LpxDereferenceAddress ("stop address", Address, AREF_OPEN);

            ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
        }

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        LpxDereferenceAddress ("Stopping", Address, AREF_TEMP_STOP);

    } else {

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
            LpxPrint1 ("LpxStopAddress %p: already stopping\n", Address);
        }

    }

} /* LpxStopAddress */


NTSTATUS
LpxStopAddressFile(
    IN PTP_ADDRESS_FILE AddressFile,
    IN PTP_ADDRESS Address
    )

/*++

Routine Description:

    This routine is called to terminate all activity on an AddressFile and
    destroy the object.  We remove every connection and datagram associated
    with this addressfile from the address database and terminate their
    activity. Then, if there are no other outstanding addressfiles open on
    this address, the address will go away.

Arguments:

    AddressFile - pointer to the addressFile to be stopped

    Address - the owning address for this addressFile (we do not depend upon
        the pointer in the addressFile because we want this routine to be safe)

Return Value:

    STATUS_SUCCESS if all is well, STATUS_INVALID_HANDLE if the Irp does not
    point to a real address.

--*/

{
    KIRQL oldirql, oldirql1;
    LIST_ENTRY localIrpList;
    PLIST_ENTRY p, pFlink;
    PIRP irp;
    PTP_CONNECTION connection;
    ULONG fStopping;

    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    if (AddressFile->State == ADDRESSFILE_STATE_CLOSING) {
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
        IF_LPXDBG (LPX_DEBUG_ADDRESS) {
            LpxPrint1 ("LpxStopAddressFile %p: already closing.\n", AddressFile);
        }
        return STATUS_SUCCESS;
    }

#ifdef __LPX__
    IF_LPXDBG (LPX_DEBUG_ADDRESS) {
        LpxPrint1 ("LpxStopAddressFile %p: closing.\n", AddressFile);
    }
#endif

    AddressFile->State = ADDRESSFILE_STATE_CLOSING;
    InitializeListHead (&localIrpList);

    //
    // Run down all connections on this addressfile, and
    // preform the equivalent of LpxDestroyAssociation
    // on them.
    //

    while (!IsListEmpty (&AddressFile->ConnectionDatabase)) {
    
        p = RemoveHeadList (&AddressFile->ConnectionDatabase);
        connection = CONTAINING_RECORD (p, TP_CONNECTION, AddressFileList);

        ACQUIRE_C_SPIN_LOCK (&connection->SpinLock, &oldirql1);

        if ((connection->Flags2 & CONNECTION_FLAGS2_ASSOCIATED) == 0) {

            //
            // It is in the process of being disassociated already.
            //

            RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql1);
            continue;
        }

        connection->Flags2 &= ~CONNECTION_FLAGS2_ASSOCIATED;
        connection->Flags2 |= CONNECTION_FLAGS2_DESTROY;    // Is this needed?
        RemoveEntryList (&connection->AddressList);
        InitializeListHead (&connection->AddressList);
        InitializeListHead (&connection->AddressFileList);
        connection->AddressFile = NULL;

        fStopping = connection->Flags2 & CONNECTION_FLAGS2_STOPPING;

#if _DBG_
        DbgPrint("conn = %p, Flags2 = %08x, fStopping = %08x\n", 
                        connection, 
                        connection->Flags2,
                        fStopping);
#endif

        if (!fStopping) {

#if _DBG_
            DbgPrint("Refing BEG\n");
#endif
            LpxReferenceConnection ("Close AddressFile", connection, CREF_STOP_ADDRESS);
#if _DBG_
            DbgPrint("Refing END\n");
#endif
        }

        RELEASE_C_SPIN_LOCK (&connection->SpinLock, oldirql1);
            
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        if (!fStopping) {
#if _DBG_
            DbgPrint("Stopping BEG\n");
#endif
            KeRaiseIrql (DISPATCH_LEVEL, &oldirql1);
            LpxStopConnection (connection, STATUS_LOCAL_DISCONNECT);
            KeLowerIrql (oldirql1);
#if _DBG_
            DbgPrint("Stopping END\n");
            DbgPrint("Derefing BEG\n");
#endif
            LpxDereferenceConnection ("Close AddressFile", connection, CREF_STOP_ADDRESS);
#if _DBG_
            DbgPrint("Derefing END\n");
#endif
        }

        LpxDereferenceAddress ("Destroy association", Address, AREF_CONNECTION);

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
    }

    //
    // now remove all of the datagrams owned by this addressfile
    //

    //
    // If the address has a datagram send in progress, skip the
    // first one, it will complete when the NdisSend completes.
    //

    p = Address->SendDatagramQueue.Flink;
    if (Address->Flags & ADDRESS_FLAGS_SEND_IN_PROGRESS) {
        ASSERT (p != &Address->SendDatagramQueue);
        p = p->Flink;
    }

    for ( ;
         p != &Address->SendDatagramQueue;
         p = pFlink ) {

        pFlink = p->Flink;
        irp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);
        if (IoGetCurrentIrpStackLocation(irp)->FileObject->FsContext == AddressFile) {
            RemoveEntryList (p);
            InitializeListHead (p);
            InsertTailList (&localIrpList, p);
        }

    }

    for (p = AddressFile->ReceiveDatagramQueue.Flink;
         p != &AddressFile->ReceiveDatagramQueue;
         p = pFlink ) {

         pFlink = p->Flink;
         RemoveEntryList (p);
         InitializeListHead (p);
         InsertTailList (&localIrpList, p);
    }

    //
    // and finally, signal failure if the address file was waiting for a
    // registration to complete (Irp is set to NULL when this succeeds).
    //

    if (AddressFile->Irp != NULL) {
        PIRP irp2=AddressFile->Irp;
#if DBG
        if ((Address->Flags & ADDRESS_FLAGS_DUPLICATE_NAME) == 0) {
            DbgPrint ("LPX: AddressFile %p closed while opening!!\n", AddressFile);
            DbgBreakPoint();
        }
#endif
        AddressFile->Irp = NULL;
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
        irp2->IoStatus.Information = 0;
        irp2->IoStatus.Status = STATUS_DUPLICATE_NAME;

        LEAVE_LPX;
        IoCompleteRequest (irp2, IO_NETWORK_INCREMENT);
        ENTER_LPX;

    } else {

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);
    }

    //
    // cancel all the datagrams on this address file
    //

    while (!IsListEmpty (&localIrpList)) {
        KIRQL cancelIrql;

        p = RemoveHeadList (&localIrpList);
        irp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);

        IoAcquireCancelSpinLock(&cancelIrql);
        IoSetCancelRoutine(irp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);
        irp->IoStatus.Information = 0;
        irp->IoStatus.Status = STATUS_NETWORK_NAME_DELETED;
        IoCompleteRequest (irp, IO_NETWORK_INCREMENT);

        LpxDereferenceAddress ("Datagram aborted", Address, AREF_REQUEST);
    }

    return STATUS_SUCCESS;
    
} /* LpxStopAddressFile */


NTSTATUS
LpxCloseAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is called to close the addressfile pointed to by a file
    object. If there is any activity to be run down, we will run it down
    before we terminate the addressfile. We remove every connection and
    datagram associated with this addressfile from the address database
    and terminate their activity. Then, if there are no other outstanding
    addressfiles open on this address, the address will go away.

Arguments:

    Irp - the Irp Address - Pointer to a TP_ADDRESS object.

Return Value:

    STATUS_SUCCESS if all is well, STATUS_INVALID_HANDLE if the Irp does not
    point to a real address.

--*/

{
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;

#ifdef __LPX__
	UNREFERENCED_PARAMETER( DeviceObject );
#endif

    addressFile  = IrpSp->FileObject->FsContext;

#ifdef __LPX__
    IF_LPXDBG (LPX_DEBUG_ADDRESS) {
        LpxPrint1 ("LpxCloseAddress AF %p:\n", addressFile);
    }
#endif

	addressFile->CloseIrp = Irp;

    //
    // We assume that addressFile has already been verified
    // at this point.
    //

    address = addressFile->Address;
    ASSERT (address);

    //
    // Remove us from the access info for this address.
    //

    ACQUIRE_RESOURCE_EXCLUSIVE (&addressFile->Provider->AddressResource, TRUE);
    IoRemoveShareAccess (addressFile->FileObject, &address->u.ShareAccess);
    RELEASE_RESOURCE (&addressFile->Provider->AddressResource);


    LpxStopAddressFile (addressFile, address);
    LpxDereferenceAddressFile (addressFile);

    //
    // This removes a reference added by our caller.
    //

    LpxDereferenceAddress ("IRP_MJ_CLOSE", address, AREF_VERIFY);

    return STATUS_PENDING;

} /* LpxCloseAddress */