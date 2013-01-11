/*++

Copyright (c) Ximeta Technology Inc

Module Name:

    LpxDrv.h

Abstract:

Author:

Revision History:

    Converted to Windows 2000 - Eliyas Yakub 

--*/
#ifndef __LPXDRIV_H
#define __LPXDRIV_H

//#undef  ExAllocatePool
//#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a, b, 'PxpL')



/* 
 * define Lpx Driver structure 
 */

typedef struct _GLOBAL
{

	//
	//	Driver object pointer
	//

    PDRIVER_OBJECT DriverObject;

	// 
	// ndis protocol handle
	//
    NDIS_HANDLE    NdisProtocolHandle;


	//
	//	tdi provider handle
	//
	HANDLE		   TdiProviderHandle;

    // 
    // Path to the driver's Services Key in the registry
    //
    UNICODE_STRING RegistryPath;

	PCONFIG_DATA	Config;
	LONG NumberOfBinds ;
    //
    // List of deviceobjecs that are created for every
    // adapter we bind to.
    //
    LIST_ENTRY  NIC_DevList;
    KSPIN_LOCK  GlobalLock; // To synchronize access to the list.
	FAST_MUTEX	DevicesLock;
    //
    // Control deviceObject for the driver.
    //

    PDEVICE_OBJECT				ControlDeviceObject;
    PDEVICE_CONTEXT			 LpxPrimaryDeviceContext;

} GLOBAL, *PGLOBAL;


/*	
 *	Instance of global variable 
*/

extern GLOBAL	global;  // Instance of global infomation sturcture

#define ACQUIRE_DEVICES_LIST_LOCK()                                     \
    ACQUIRE_FAST_MUTEX_UNSAFE(&global.DevicesLock)

#define RELEASE_DEVICES_LIST_LOCK()                                     \
   RELEASE_FAST_MUTEX_UNSAFE(&global.DevicesLock)


/* 
 * declaration for Exported Lpx driver function 
 */ 
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
LpxOpen(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    );

NTSTATUS
LpxClose(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    );

NTSTATUS
LpxCleanup(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    );

NTSTATUS
LpxInternalDevControl(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    );

NTSTATUS
LpxDevControl(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    );

NTSTATUS
LpxPnPPower(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
    );

VOID
LpxUnload(
	IN PDRIVER_OBJECT  DriverObject 
    );












VOID
LpxCompleteSendIrp(
    IN PIRP Irp,
    IN NTSTATUS Status,
    IN ULONG Information
    );

VOID
lpx_StopControlChannel(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN USHORT ChannelIdentifier
    );

NTSTATUS
lpx_TdiAccept(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiAction(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiAssociateAddress(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiDisassociateAddress(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiConnect(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiDisconnect(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiListen(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiQueryInformation(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiReceive(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiReceiveDatagram(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiSend(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiSendDatagram(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiSetEventHandler(
    IN PIRP Irp
    );

NTSTATUS
lpx_TdiSetInformation(
    IN PIRP Irp
    );

NTSTATUS
lpx_DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
lpx_DispatchPnPPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );






#endif //#ifndef __LPXDRIV_H













