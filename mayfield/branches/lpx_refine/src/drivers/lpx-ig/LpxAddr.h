
/*++

Copyright (c) Ximeta Technology Inc

Module Name:

    LpxDrv.h

Abstract:

Author:

Revision History:

    Converted to Windows 2000 - Eliyas Yakub 

--*/

#ifndef __LPXADDR_H
#define __LPXADDR_H

static GENERIC_MAPPING AddressGenericMapping =
       { READ_CONTROL, READ_CONTROL, READ_CONTROL, READ_CONTROL };

#define ADDRESS_STATE_OPENING   0x01    // not yet open for business
#define ADDRESS_STATE_OPEN      0x02    // open for business
#define ADDRESS_STATE_CLOSING   0x04   // closing



typedef struct _TP_ADDRESS {

    USHORT Size;
    CSHORT Type;
	LONG ReferenceCount;                // number of references to this object.
    UCHAR State;

    LIST_ENTRY Linkage;                 // next address/this device object.
  
	LIST_ENTRY ConnectionDatabase;  // list of defined transport connections.
	LIST_ENTRY				ConnectionServicePointList;	
    
	//
    // The following spin lock is acquired to edit this TP_ADDRESS structure
    // or to scan down or edit the list of address files.
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this structure.

    PFILE_OBJECT FileObject;   
    PNBF_NETBIOS_ADDRESS NetworkName;    // this address


    PCONTROL_DEVICE_CONTEXT Provider;   // device context to which we are attached.
	PDEVICE_CONTEXT	PacketProvider;
	

	PIRP	CloseIrp;
    //
    // These two can be a union because they are not used
    // concurrently.
    //

    union {

        //
        // This structure is used for checking share access.
        //

        SHARE_ACCESS ShareAccess;

        //
        // Used for delaying NbfDestroyAddress to a thread so
        // we can access the security descriptor.
        //

        WORK_QUEUE_ITEM DestroyAddressQueueItem;

    } u;

    //
    // This structure is used to hold ACLs on the address.

    PSECURITY_DESCRIPTOR SecurityDescriptor;

    //
    // handler for kernel event actions. First we have a set of booleans that
    // indicate whether or not this address has an event handler of the given
    // type registered.
    //

    BOOLEAN RegisteredConnectionHandler;
    BOOLEAN RegisteredDisconnectHandler;
    BOOLEAN RegisteredReceiveHandler;
    BOOLEAN RegisteredReceiveDatagramHandler;
    BOOLEAN RegisteredExpeditedDataHandler;
    BOOLEAN RegisteredErrorHandler;

    //
    // This function pointer points to a connection indication handler for this
    // Address. Any time a connect request is received on the address, this
    // routine is invoked.
    //
    //

    PTDI_IND_CONNECT ConnectionHandler;
    PVOID ConnectionHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_DISCONNECT
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_DISCONNECT DisconnectHandler;
    PVOID DisconnectHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE
    // event handler for connections on this address.  If the NULL handler
    // is specified in a TdiSetEventHandler, then this points to an internal
    // routine which does not accept the incoming data.
    //

    PTDI_IND_RECEIVE ReceiveHandler;
    PVOID ReceiveHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE_DATAGRAM
    // event handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which does
    // not accept the incoming data.
    //

    PTDI_IND_RECEIVE_DATAGRAM ReceiveDatagramHandler;
    PVOID ReceiveDatagramHandlerContext;

    //
    // An expedited data handler. This handler is used if expedited data is
    // expected; it never is in NBF, thus this handler should always point to
    // the default handler.
    //

    PTDI_IND_RECEIVE_EXPEDITED ExpeditedDataHandler;
    PVOID ExpeditedDataHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_ERROR
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_ERROR ErrorHandler;
    PVOID ErrorHandlerContext;
    PVOID ErrorHandlerOwner;


} TP_ADDRESS, *PTP_ADDRESS;


//
//	Address managing function definition
//
VOID
lpx_ReferenceAddress(
    IN PTP_ADDRESS Address
    );


VOID
lpx_DereferenceAddress(
    IN PTP_ADDRESS Address
    );


VOID
lpx_DeallocateAddress(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS TransportAddress
    );


VOID
lpx_AllocateAddress(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS *TransportAddress
    );


NTSTATUS
lpx_CreateAddress(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PDEVICE_CONTEXT RealDeviceContext,
    IN PNBF_NETBIOS_ADDRESS NetworkName,
    OUT PTP_ADDRESS *Address
    );

VOID
lpx_DestroyAddress(
    IN PVOID Parameter
    );

VOID
lpx_StopAddress(
    IN PTP_ADDRESS Address
    );

//
//	managing Open Address / Address file operation
//


PDEVICE_CONTEXT
lpx_FindDeviceContext(
    PNBF_NETBIOS_ADDRESS networkName
	);

TDI_ADDRESS_NETBIOS *
lpx_ParseTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN BOOLEAN BroadcastAddressOk
	);

BOOLEAN
lpx_ValidateTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN ULONG TransportAddressLength
	);

NTSTATUS
lpx_OpenAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
lpx_CloseAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
lpx_VerifyAddressObject (
    IN PTP_ADDRESS AddressFile
    );

#endif // #ifndef __LPXADDR_H
