/*++

Copyright (c) Ximeta Technology Inc

Module Name:

    LpxConn.h

Abstract:

Author:

Revision History:

    Converted to Windows 2000 - Eliyas Yakub 

--*/

#ifndef __LPXCONN_H
#define __LPXCONN_H


#define CONNECTION_FLAGS2_STOPPING      0x00000001 // connection is running down.
#define CONNECTION_FLAGS2_CLOSING       0x00000002 // connection is closing
#define CONNECTION_FLAGS2_ASSOCIATED    0x00000004 // associated with address
#define CONNECTION_FLAGS2_DISCONNECT    0x00000008 // disconnect done on connection
#define CONNECTION_FLAGS2_DISASSOCIATED 0x00000010 // associate CRef has been removed
#define CONNECTION_FLAGS2_DISCONNECTED  0x00000020 // disconnect has been indicated
#define CONNECTION_FLAGS2_DESTROY       0x00040040 // destroy this connection.



typedef struct _TP_CONNECTION {

	SERVICE_POINT	ServicePoint;

    CSHORT Type;
    USHORT Size;

	LIST_ENTRY LinkList;	
    LIST_ENTRY Linkage;                 
    KSPIN_LOCK SpinLock;                // spinlock for connection protection.
   

    LONG ReferenceCount;                // number of references to this object.
    LONG SpecialRefCount;               // controls freeing of connection.

    ULONG Flags;                        // attributes guarded by LinkSpinLock
    ULONG Flags2;                       // attributes guarded by SpinLock
	
    PCONTROL_DEVICE_CONTEXT Provider;       // device context to which we are attached.
	PDEVICE_CONTEXT PacketProvider;

    PKSPIN_LOCK ProviderInterlock;          // &Provider->Interlock
    PFILE_OBJECT FileObject;                // easy backlink to file object.
	PTP_ADDRESS  Address;
    //
    // The following field contains the actual ID we expose to the TDI client
    // to represent this connection.  A unique one is created from the address.
    //

    USHORT ConnectionId;                    // unique identifier.

    CONNECTION_CONTEXT Context;         // client-specified value.
	NBF_NETBIOS_ADDRESS CalledAddress;  
    
    CHAR RemoteName[16];

} TP_CONNECTION, *PTP_CONNECTION;




VOID
lpx_DerefConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    );

VOID
lpx_ReferenceConnection(
	PTP_CONNECTION Connection
	);

VOID
lpx_DereferenceConnection(
	PTP_CONNECTION Connection
	);

VOID
lpx_DerefConnection(
    IN PTP_CONNECTION TransportConnection
    );

VOID
lpx_AllocateConnection(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    );

VOID
lpx_DeallocateConnection(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    );

NTSTATUS
lpx_CreateConnection(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    );
NTSTATUS
lpx_OpenConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );
NTSTATUS
lpx_CloseConnection (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
lpx_StopConnection(
    IN PTP_CONNECTION Connection,
    IN NTSTATUS Status
    );

NTSTATUS
lpx_DestroyConnection(
    IN PTP_CONNECTION TransportConnection
    );

NTSTATUS
lpx_DestroyAssociation(
    IN PTP_CONNECTION TransportConnection
    );

NTSTATUS
lpx_VerifyConnectionObject (
    IN PTP_CONNECTION Connection
    );


#endif //#ifndef __LPXCONN_H





