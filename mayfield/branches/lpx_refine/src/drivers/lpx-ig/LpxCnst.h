/*++

Copyright (c) Ximeta Technology Inc

Module Name:

    LpxCnst.h

Abstract:

Author:

Revision History:

    Converted to Windows 2000 - Eliyas Yakub 

--*/
#ifndef __LPXCNST_H
#define __LPXCNST_H


#define LPX_NAME                L"Lpx"          	// name for protocol chars.
#define LPX_DEVICE_NAME         L"\\Device\\SocketLpx"	// name of our driver.
#define LPX_DOSDEVICE_NAME	L"\\DosDevices\\SocketLpx"	// mane of symbolic link for driver.
	
#define LPX_MEM_TAG_TP_ADDRESS          'aXPL'
#define LPX_MEM_TAG_RCV_BUFFER          'bXPL'
#define LPX_MEM_TAG_TP_CONNECTION       'cXPL'
#define LPX_MEM_TAG_POOL_DESC           'dXPL'
#define LPX_MEM_TAG_DEVICE_EXPORT       'eXPL'
#define LPX_MEM_TAG_TP_ADDRESS_FILE     'fXPL'
#define LPX_MEM_TAG_NETBIOS_NAME		'gXPL'
#define LPX_MEM_TAG_DEVICE_PDO			'hXPL'
#define LPX_MEM_TAG_REGISTRY_PATH       'iXPL' 
#define LPX_MEM_TAG_CONFIG_DATA			'jXPL'
#define LPX_MEM_TAG_TDI_QUERY_BUFFER    'kXPL'
#define LPX_MEM_TAG_WORK_ITEM			'lXPL'

#define LPX_FILE_TYPE_CONTROL   (ULONG)0x1919   // file is type control

/*
	DebugLevel	0 --> driver install
	DebugLevel	1 --> Open / Close connection
	DebugLevel	2 --> Read / Write
	DebugLevel	8 --> Error

*/

extern LONG	DebugLevel;
#if DBG
#define DebugPrint(l, x)			\
		if(l == DebugLevel) {		\
			DbgPrint x; 			\
		}
#else	
#define DebugPrint(l, x) 
#endif


#if DBG
#define PANIC(Msg) \
    DbgPrint ((Msg))
#else
#define PANIC(Msg)
#endif


#define ACQUIRE_RESOURCE_EXCLUSIVE(Resource, Wait) \
    KeEnterCriticalRegion(); ExAcquireResourceExclusive(Resource, Wait);
    
#define RELEASE_RESOURCE(Resource) \
    ExReleaseResource(Resource); KeLeaveCriticalRegion();

#define ACQUIRE_FAST_MUTEX_UNSAFE(Mutex) \
    KeEnterCriticalRegion(); ExAcquireFastMutexUnsafe(Mutex);

#define RELEASE_FAST_MUTEX_UNSAFE(Mutex) \
    ExReleaseFastMutexUnsafe(Mutex); KeLeaveCriticalRegion();




#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)
#define ACQUIRE_DPC_SPIN_LOCK(lock) KeAcquireSpinLockAtDpcLevel(lock)
#define RELEASE_DPC_SPIN_LOCK(lock) KeReleaseSpinLockFromDpcLevel(lock)




#define			CONTROL_DEVICE_SIGNATURE		((CSHORT)0x1901)
#define			NDIS_DEVICE_SIGNATURE			((CSHORT)0x1902)
#define			LPX_ADDRESS_SIGNATURE			((CSHORT)0x1903)
#define			LPX_CONNECTION_SIGNATURE		((CSHORT)0x1904)
#define			LPX_PACKET_SIGNATURE			((CSHORT)0x1905)





typedef struct _TP_SEND_IRP_PARAMETERS {
    TDI_REQUEST_KERNEL_SEND Request;
    LONG ReferenceCount;
    PVOID Irp;
} TP_SEND_IRP_PARAMETERS, *PTP_SEND_IRP_PARAMETERS;

#define IRP_SEND_LENGTH(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.SendLength)

#define IRP_SEND_FLAGS(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.SendFlags)

#define IRP_SEND_REFCOUNT(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->ReferenceCount)

#define IRP_SEND_IRP(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Irp)

#define IRP_SEND_CONNECTION(_IrpSp) \
    ((PTP_CONNECTION)((_IrpSp)->FileObject->FsContext))

#define IRP_DEVICE_CONTEXT(_IrpSp) \
    ((PDEVICE_CONTEXT)((_IrpSp)->DeviceObject))





#define LpxReferenceSendIrp(IrpSp)\
    (VOID)InterlockedIncrement( \
        &IRP_SEND_REFCOUNT(IrpSp))


#define LpxDereferenceSendIrp(IrpSp) {\
    PIO_STACK_LOCATION _IrpSp = (IrpSp); \
    if (InterlockedDecrement( \
            &IRP_SEND_REFCOUNT(_IrpSp)) == 0) { \
        PIRP _Irp = IRP_SEND_IRP(_IrpSp); \
        IRP_SEND_REFCOUNT(_IrpSp) = 0; \
        IRP_SEND_IRP (_IrpSp) = NULL; \
		{ \
			KIRQL	ilgu_cancelIrql; \
			IoAcquireCancelSpinLock(&ilgu_cancelIrql);	\
			IoSetCancelRoutine(_Irp, NULL);	\
			IoReleaseCancelSpinLock(ilgu_cancelIrql);	\
			IoCompleteRequest (_Irp, IO_NETWORK_INCREMENT); \
		} \
	} \
}




















#endif //#ifndef __LPXCNST_H
