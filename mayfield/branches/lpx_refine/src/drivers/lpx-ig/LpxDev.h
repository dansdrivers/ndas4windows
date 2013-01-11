/*++

Copyright (c) Ximeta Technology Inc

Module Name:

    LpxDev.h

Abstract:

Author:

Revision History:

 
--*/
#ifndef __LPXDEV_H
#define __LPXDEV_H


#define DEVICECONTEXT_STATE_OPENING  0x00
#define DEVICECONTEXT_STATE_OPEN     0x01
#define DEVICECONTEXT_STATE_DOWN     0x02
#define DEVICECONTEXT_STATE_STOPPING 0x03




typedef struct _LPX_CONTEXT
{

	DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
	CSHORT	Type;
	USHORT	Size;
    LONG ReferenceCount;                // activity count/this provider.
}LPX_CONTEXT, *PLPX_CONTEXT;

typedef struct _CONTROL_DEVICE_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
	CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure
    LONG ReferenceCount;                // activity count/this provider.

	USHORT				PortNum;

    //
    // the device context state, among open, closing
    //

    UCHAR State;                                      
    PVOID PnPContext;


    LONG CreateRefRemoved;              // has unload or unbind been called ?

    //
    // This resource guards access to the ShareAccess
    // and SecurityDescriptor fields in addresses.
    //

    ERESOURCE AddressResource;
    
    KSPIN_LOCK Interlock;               // GLOBAL spinlock for reference count.
                                        //  (used in ExInterlockedXxx calls)

    //
    // Following are protected by Global Device Context SpinLock
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this object.
                                        //  (used in KeAcquireSpinLock calls)

	HANDLE TdiDeviceHandle;
	//
    // The queue of (currently receive only) IRPs waiting to complete.
    //

    LIST_ENTRY IrpCompletionQueue;

 
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


 

    //
    // The following queue holds free TP_CONNECTION objects available for allocation.
    //

    LIST_ENTRY ConnectionPool;

    //
    // These counters keep track of resources uses by TP_CONNECTION objects.
    //

    ULONG ConnectionAllocated;
    ULONG ConnectionInitAllocated;
    ULONG ConnectionMaxAllocated;
    ULONG ConnectionInUse;
    ULONG ConnectionMaxInUse;
    ULONG ConnectionExhausted;


    //
    // This is the Mac type we must build the packet header for and know the
    // offsets for.
    //
    TDI_PROVIDER_INFO Information;      // information about this provider.
    NBF_NDIS_IDENTIFICATION MacInfo;    // MAC type and other info
    ULONG MaxReceivePacketSize;         // does not include the MAC header
    ULONG MaxSendPacketSize;            // includes the MAC header
    ULONG CurSendPacketSize;            // may be smaller for async
    USHORT RecommendedSendWindow;       // used for Async lines
    BOOLEAN EasilyDisconnected;         // TRUE over wireless nets.


    //
    // The following field is a head of a list of TP_ADDRESS objects that
    // are defined for this transport provider.  To edit the list, you must
    // hold the spinlock of the device context object.
    //

    LIST_ENTRY AddressDatabase;        // list of defined transport addresses.
	LIST_ENTRY ConnectionDatabase;
 
	PWCHAR DeviceName;
    ULONG DeviceNameLength;
    //
    // This next field maintains a unique number which can next be assigned
    // as a connection identifier.  It is incremented by one each time a
    // value is allocated.
    //

    USHORT UniqueIdentifier;            // starts at 0, wraps around 2^16-1.

    //
    // This contains the next unique indentified to use as
    // the FsContext in the file object associated with an
    // open of the control channel.
    //

    USHORT ControlChannelIdentifier;
    //
    // The following structure contains statistics counters for use
    // by TdiQueryInformation and TdiSetInformation.  They should not
    // be used for maintenance of internal data structures.
    //

 




} CONTROL_DEVICE_CONTEXT, *PCONTROL_DEVICE_CONTEXT;







typedef struct _DEVICE_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure
    LONG ReferenceCount;                // activity count/this provider.

    LIST_ENTRY Linkage;                   // links them on NbfDeviceList;

    KSPIN_LOCK Interlock;               // GLOBAL spinlock for reference count.
                                        //  (used in ExInterlockedXxx calls)
                                        
 
    LONG CreateRefRemoved;              // has unload or unbind been called ?


    //
    // The queue of (currently receive only) IRPs waiting to complete.
    //

    LIST_ENTRY IrpCompletionQueue;

   

    BOOLEAN IndicationQueuesInUse;

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

    WORK_QUEUE_ITEM StatusClosingQueueItem;

   	ULONG NdisSendsInProgress;
	LIST_ENTRY NdisSendQueue; 




 
    //
    // This queue contains receives that are in progress
    //

    

    //
    // NDIS/TDI fields
    //

 

    UCHAR ReservedNetBIOSAddress[NETBIOS_NAME_LENGTH];
    NDIS_HANDLE NdisBindingHandle;

    //
    // The following fields are used for talking to NDIS. They keep information
    // for the NDIS wrapper to use when determining what pool to use for
    // allocating storage.
    //



   
    ULONG MaxConnections;
    ULONG MaxAddresses;
    PWCHAR DeviceName;
    ULONG DeviceNameLength;

    //
    // This is the Mac type we must build the packet header for and know the
    // offsets for.
    //
    TDI_PROVIDER_INFO Information;      // information about this provider.
    NBF_NDIS_IDENTIFICATION MacInfo;    // MAC type and other info
    ULONG MaxReceivePacketSize;         // does not include the MAC header
    ULONG MaxSendPacketSize;            // includes the MAC header
    ULONG CurSendPacketSize;            // may be smaller for async
    USHORT RecommendedSendWindow;       // used for Async lines
    BOOLEAN EasilyDisconnected;         // TRUE over wireless nets.

    //
    // some MAC addresses we use in the transport
    //

    HARDWARE_ADDRESS LocalAddress;      // our local hardware address.
    HARDWARE_ADDRESS NetBIOSAddress;    // NetBIOS functional address, used for TR

    //
    // The reserved Netbios address; consists of 10 zeroes
    // followed by LocalAddress;
    //

    
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
    BOOLEAN MediumSpeedAccurate;          // if FALSE, can't use the link.

    //
    // This is TRUE if we are on a UP system.
    //

    BOOLEAN UniProcessor;

 

    ERESOURCE AddressResource;

  
    //
    // This is to hold the underlying PDO of the device so
    // that we can answer DEVICE_RELATION IRPs from above
    //

    PVOID PnPContext;

    //
    // The following structure contains statistics counters for use
    // by TdiQueryInformation and TdiSetInformation.  They should not
    // be used for maintenance of internal data structures.
    //

 

 

   

    NDIS_HANDLE         LpxPacketPool;

	// Mod by jgahn.
	NDIS_HANDLE			LpxBufferPool;

	// Received packet.
	KSPIN_LOCK			PacketInProgressQSpinLock;
	LIST_ENTRY			PacketInProgressList;



} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
   




VOID
lpx_Control_RefDeviceContext(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    );

VOID
lpx_Control_DerefDeviceContext(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    );

VOID
lpx_Ndis_RefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

VOID
lpx_Ndis_DerefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

NTSTATUS
Lpx_CreateControlDevice( 
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	DeviceName
	);

VOID
lpx_DestroyControlDeviceContext(
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    );

VOID
LpxFreeControlResources (
    IN PCONTROL_DEVICE_CONTEXT DeviceContext
    );

NTSTATUS
LpxCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING DeviceName,
    IN OUT PDEVICE_CONTEXT *DeviceContext
    );

VOID
LpxReInitializeDeviceContext(
	OUT PNDIS_STATUS NdisStatus,
	IN PDRIVER_OBJECT DriverObject,
	IN PCONFIG_DATA NbfConfig,
	IN PUNICODE_STRING BindName,
	IN PUNICODE_STRING ExportName,
	IN PVOID SystemSpecific1,
	IN PVOID SystemSpecific2
	);

ULONG
lpx_CreateNdisDeviceContext(	    
   OUT PNDIS_STATUS NdisStatus,
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA NbfConfig,
    IN PUNICODE_STRING BindName,
    IN PUNICODE_STRING ExportName,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2
	);

VOID
lpx_DestroyNdisDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

VOID
LpxFreeNdisResources (
    IN PDEVICE_CONTEXT DeviceContext
    );



#define LPX_REFERENCE_DEVICECONTEXT(DeviceContext)            \
{															  \
	PLPX_CONTEXT pDeviceContext = (PLPX_CONTEXT)DeviceContext; \
	if ((pDeviceContext)->ReferenceCount == 0)                \
		DbgBreakPoint();                                      \
	if((pDeviceContext)->Type == CONTROL_DEVICE_SIGNATURE )	{ \
		lpx_Control_RefDeviceContext ((PCONTROL_DEVICE_CONTEXT)pDeviceContext);\
	} else if((pDeviceContext)->Type == NDIS_DEVICE_SIGNATURE){\
		lpx_Ndis_RefDeviceContext ((PDEVICE_CONTEXT)pDeviceContext);\
	}else {                                                   \
		DbgBreakPoint();                                     \
	}														  \
}
			
#define LPX_DEREFERENCE_DEVICECONTEXT(DeviceContext)          \
{															  \
	PLPX_CONTEXT pDeviceContext = (PLPX_CONTEXT)DeviceContext; \
	if ((pDeviceContext)->ReferenceCount == 0)                \
		DbgBreakPoint();                                      \
	if((pDeviceContext)->Type == CONTROL_DEVICE_SIGNATURE )	{ \
		lpx_Control_DerefDeviceContext ((PCONTROL_DEVICE_CONTEXT)pDeviceContext);\
	} else if((pDeviceContext)->Type == NDIS_DEVICE_SIGNATURE){\
		lpx_Ndis_DerefDeviceContext ((PDEVICE_CONTEXT)pDeviceContext);\
	}else {                                                   \
		DbgBreakPoint();                                     \
	}														  \
}

#endif //#ifndef __LPXDEV_H




