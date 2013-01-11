/*++

Copyright (c) 1990-2000  Microsoft Corporation All Rights Reserved

Module Name:

    BUSENUM.C

Abstract:

    This module contains the entry points for a toaster bus driver.

Author:


Environment:

    kernel mode only

Revision History:

    Cleaned up sample 05/05/99
    Fixed the create_close and ioctl handler to fail the request 
    sent on the child stack - 3/15/04


--*/

#include "ndasbusproc.h"

#if DBG

LONG DbgLevelBusEmum = DBG_LEVEL_BUS_EMUM;

#define NdasDbgCall(l,x,...) do {						\
	if (l <= DbgLevelBusEmum) {							\
	DbgPrint("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
	DbgPrint(x,__VA_ARGS__);							\
	} 													\
} while(0)

#else
#define NdasDbgCall(l,x,...);
#endif

//
// Global Debug Level
//

#define DEBUG_BUFFER_LENGTH 256

ULONG BusEnumDebugLevel = BUS_DEFAULT_DEBUG_OUTPUT_LEVEL;


GLOBALS Globals;

#if __NDAS_SCSI_BUS__

BOOLEAN	NdasTestBug = 1;

UCHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	DebugBufferSentinal = NULL;
UCHAR	DebugBufferWithLocation[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	DebugBufferWithLocationSentinal = NULL;

PCHAR
_KDebugPrint(
   IN PCCHAR	DebugMessage,
   ...
   )


{
    va_list ap;

    va_start(ap, DebugMessage);

	_vsnprintf(DebugBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	ASSERTMSG("_KDebugPrint overwrote sentinal byte",
		((DebugBufferSentinal == NULL) || (*DebugBufferSentinal == 0xff)));

	if(DebugBufferSentinal) {
		*DebugBufferSentinal = 0xff;
	}

//	DbgPrint(DebugBuffer);

    va_end(ap);

	return (PCHAR)DebugBuffer;
}

VOID
_KDebugPrintWithLocation(
   IN PCCHAR	DebugMessage,
   PCCHAR		ModuleName,
   UINT32		LineNumber,
   PCCHAR		FunctionName
   )
{
	_snprintf(DebugBufferWithLocation, DEBUG_BUFFER_LENGTH, 
		"[%s:%04d] %s : %s", ModuleName, LineNumber, FunctionName, DebugMessage);

	ASSERTMSG("_KDebugPrintWithLocation overwrote sentinal byte",
		((DebugBufferWithLocationSentinal == NULL) || (*DebugBufferWithLocationSentinal == 0xff)));

	if(DebugBufferWithLocationSentinal) {
		*DebugBufferWithLocationSentinal = 0xff;
	}

	DbgPrint(DebugBufferWithLocation);
}


PPDO_DEVICE_DATA
LookupPdoData(
	PFDO_DEVICE_DATA	FdoData,
	ULONG				SystemIoBusNumber);
VOID
CleanupPdoEnumRegistryUnsafe(
			PFDO_DEVICE_DATA	FdoData);

#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, Bus_DriverUnload)
#pragma alloc_text (PAGE, Bus_CreateClose)
#pragma alloc_text (PAGE, Bus_IoCtl)
#pragma alloc_text (PAGE, LookupPdoData)
#endif

NTSTATUS
DriverEntry (
    __in  PDRIVER_OBJECT  DriverObject,
    __in  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

    Initialize the driver dispatch table.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

  NT Status Code

--*/
{
	NTSTATUS	status;
	ULONG		tempUlong;

#if DBG
	DbgPrint( "\n************NdasBus %s %s %s\n", __FUNCTION__, __DATE__, __TIME__ );
#endif

    //
    // Save the RegistryPath for WMI.
    //

    Globals.RegistryPath.MaximumLength = RegistryPath->Length +
                                          sizeof(UNICODE_NULL);
    Globals.RegistryPath.Length = RegistryPath->Length;
    Globals.RegistryPath.Buffer = ExAllocatePoolWithTag(
                                       PagedPool,
                                       Globals.RegistryPath.MaximumLength,
                                       BUSENUM_POOL_TAG
                                       );

    if (!Globals.RegistryPath.Buffer) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&Globals.RegistryPath, RegistryPath);

#if __NDAS_SCSI_BUS__

	//
	// Query OS Versions
	//
	Globals.bCheckVersion = PsGetVersion(
								&Globals.MajorVersion,
								&Globals.MinorVersion,
								&Globals.BuildNumber,
								NULL
								);

	if(Globals.bCheckVersion == TRUE) {
		Bus_KdPrint_Def (BUS_DBG_SS_INFO, 
			("Checkd Build, Major Ver %d, Minor Ver %d, Build %d\n", 
				Globals.MajorVersion, Globals.MinorVersion, Globals.BuildNumber));
	} else {
		Bus_KdPrint_Def (BUS_DBG_SS_INFO, 
			("Free Build, Major Ver %d, Minor Ver %d, Build %d\n", 
				Globals.MajorVersion, Globals.MinorVersion, Globals.BuildNumber));
	}

#endif

    //
    // Set entry points into the driver
    //
    DriverObject->MajorFunction [IRP_MJ_CREATE] =
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = Bus_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_PNP] = Bus_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER] = Bus_Power;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = Bus_IoCtl;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = Bus_SystemControl;
    DriverObject->DriverUnload = Bus_DriverUnload;
    DriverObject->DriverExtension->AddDevice = Bus_AddDevice;

#if __NDAS_SCSI_BUS__

	//
	//	Init mutex
	//
	ExInitializeFastMutex(&Globals.Mutex);

	//
	//	Default setting
	//
	Globals.PersistentPdo = TRUE;
	Globals.LfsFilterInstalled = FALSE;

	//
	//	Read options in the registry
	//

	// Disable persistent PDO option
	status = DrReadKeyValueInstantly(	RegistryPath,
									BUSENUM_DRVREG_DISABLE_PERSISTENTPDO,
									REG_DWORD,
									&tempUlong,
									sizeof(tempUlong),
									NULL);
	if(NT_SUCCESS(status) && tempUlong != 0) {
		Globals.PersistentPdo = FALSE;
		Bus_KdPrint_Def (BUS_DBG_SS_INFO, 
			("Persistent PDO option disabled.\n"));
	}

	//
	//	Check to see if LFS filter is installed.
	//

	status = LfsFiltDriverServiceExist();
	if(NT_SUCCESS(status)) {
		Globals.LfsFilterInstalled = TRUE;
		Bus_KdPrint_Def (BUS_DBG_SS_INFO,
			("LFS Filter is detected.\n"));
	} else {
		Globals.LfsFilterInstalled = FALSE;
		Bus_KdPrint_Def (BUS_DBG_SS_INFO,
			("LFS Filter is not detected. STATUS=%08lx\n", status));
	}

#endif

    return STATUS_SUCCESS;
}

NTSTATUS
Bus_CreateClose (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
/*++
Routine Description:

    Some outside source is trying to create a file against us.

    If this is for the FDO (the bus itself) then the caller
    is trying to open the proprietary connection to tell us
    to enumerate or remove a device.

    If this is for the PDO (an object on the bus) then this
    is a client that wishes to use the toaster device.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PFDO_DEVICE_DATA    fdoData;
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;
    //
    // We only allow create/close requests for the FDO.
    // That is the bus itself.
    //

    if (!commonData->IsFDO) {
        Irp->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    Bus_IncIoCount (fdoData);

    //
    // Check to see whether the bus is removed
    //

    if (fdoData->DevicePnPState == Deleted) {
        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }


    irpStack = IoGetCurrentIrpStackLocation (Irp);

    switch (irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Create \n"));
        status = STATUS_SUCCESS;
        break;

    case IRP_MJ_CLOSE:
        Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Close \n"));
        status = STATUS_SUCCESS;
        break;
     default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    Bus_DecIoCount (fdoData);
    return status;
}

#if __NDAS_SCSI_BUS__

//
//	Increment the reference count to a PDO.
//
PPDO_DEVICE_DATA
LookupPdoData (
	PFDO_DEVICE_DATA	FdoData,
	ULONG				SystemIoBusNumber
	)
{
	PPDO_DEVICE_DATA	pdoData = NULL;
    PLIST_ENTRY         entry;

    PAGED_CODE ();

    KeEnterCriticalRegion();
	ExAcquireFastMutex( &FdoData->Mutex );

    for (entry = FdoData->ListOfPDOs.Flink; entry != &FdoData->ListOfPDOs; entry = entry->Flink) {

		 pdoData = CONTAINING_RECORD( entry, PDO_DEVICE_DATA, Link );

		 if (pdoData->SerialNo == SystemIoBusNumber && pdoData->Present) {

			 break;
		 }

		pdoData = NULL;
	}

	if (pdoData) {

		 //	increment the reference count to the PDO.

		 ObReferenceObject( pdoData->Self );
		 InterlockedIncrement( &pdoData->ReferenceCount );
	}

	ExReleaseFastMutex( &FdoData->Mutex );
    KeLeaveCriticalRegion();

	return pdoData;
}


//////////////////////////////////////////////////////////////////////////
//
//	Worker to unplug NDAS devices at late time.
//

typedef struct _NDBUS_UNPLUGWORKER {

	PIO_WORKITEM		IoWorkItem;
	PFDO_DEVICE_DATA	FdoData;
	ULONG				SlotNo;

} NDBUS_UNPLUGWORKER, *PNDBUS_UNPLUGWORKER;

//	Unplugging worker function

VOID
UnplugWorker (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PVOID			Context
	)
{
	NTSTATUS				status;
	PNDBUS_UNPLUGWORKER		ctx = (PNDBUS_UNPLUGWORKER)Context;
	NDASBUS_UNPLUG_HARDWARE	unplug;

	UNREFERENCED_PARAMETER(DeviceObject);

	NdasDbgCall( 1, "in\n" );

	//	IO_WORKITEM is rare resource, give it back to the system now.

	IoFreeWorkItem(ctx->IoWorkItem);

	//	Start unplug

	unplug.Size = sizeof(unplug);
	unplug.SerialNo = ctx->SlotNo;

	status = Bus_UnPlugDevice( &unplug, ctx->FdoData );

	NDAS_ASSERT( NT_SUCCESS(status) );

	ExFreePool(Context);

	NdasDbgCall( 1, "out\n" );
}


//
//	Queue a workitem to unplug a NDAS device.
//


NTSTATUS
QueueUnplugWorker(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo
	) {
	NTSTATUS			status;
	PNDBUS_UNPLUGWORKER	workItemCtx;

	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("entered.\n"));


	//
	//	Parameter check
	//

	if(!FdoData) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FdoData NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}


	//
	//	Allocate worker's context
	//

	workItemCtx = (PNDBUS_UNPLUGWORKER)ExAllocatePoolWithTag(
								NonPagedPool,
								sizeof(NDBUS_UNPLUGWORKER),
								NDBUS_POOLTAG_UNPLUGWORKITEM);
	if(!workItemCtx) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	workItemCtx->FdoData		= FdoData;
	workItemCtx->SlotNo			= SlotNo;


	//
	//	Allocate IO work item for NDASBUS's Functional device object.
	//

	workItemCtx->IoWorkItem = IoAllocateWorkItem(FdoData->Self);
	if(workItemCtx->IoWorkItem == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}


	//
	//	Queue the work item.
	//

	IoQueueWorkItem(
		workItemCtx->IoWorkItem,
		UnplugWorker,
		DelayedWorkQueue,
		workItemCtx
		);

	return STATUS_SUCCESS;

cleanup:
	if(workItemCtx) {
		ExFreePool(workItemCtx);
	}

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	BUS IOCTL
//

#ifndef _WIN64

//
//	Dummy for 32bit Windows
//	Disallow thunk by returning FALSE
//

__inline
BOOLEAN
IoIs32bitProcess(PIRP Irp) {
	UNREFERENCED_PARAMETER(Irp);
	return FALSE;
}

#endif

#endif

NTSTATUS
Bus_IoCtl (
    __in  PDEVICE_OBJECT  DeviceObject,
    __in  PIRP            Irp
    )
/*++
Routine Description:

    Handle user mode PlugIn, UnPlug and device Eject requests.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    ULONG                   inlen;
    ULONG                   outlen;
    PFDO_DEVICE_DATA        fdoData;
    PVOID                   buffer;
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

	//
	// It is not safe to call IOCTL in raised IRQL.

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
    
    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;
    //
    // We only allow create/close requests for the FDO.
    // That is the bus itself.
    //
    if (!commonData->IsFDO) {
        Irp->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    Bus_IncIoCount (fdoData);

    //
    // Check to see whether the bus is removed
    //

    if (fdoData->DevicePnPState == Deleted) {
        status = STATUS_NO_SUCH_DEVICE;
        goto END;
    }

    irpStack = IoGetCurrentIrpStackLocation (Irp);

    buffer = Irp->AssociatedIrp.SystemBuffer;
    inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outlen			= irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    status = STATUS_INVALID_PARAMETER;
    
	Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("%d called\n", irpStack->Parameters.DeviceIoControl.IoControlCode));

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

#if !__NDAS_SCSI_BUS__

    case IOCTL_BUSENUM_PLUGIN_HARDWARE:
        if (
            //
            // Make sure it has at least two nulls and the size
            // field is set to the declared size of the struct
            //
            ((sizeof (BUSENUM_PLUGIN_HARDWARE) + sizeof(UNICODE_NULL) * 2) <=
             inlen) &&

            //
            // The size field should be set to the sizeof the struct as declared
            // and *not* the size of the struct plus the multi_sz
            //
            (sizeof (BUSENUM_PLUGIN_HARDWARE) ==
             ((PBUSENUM_PLUGIN_HARDWARE) buffer)->Size)) {

            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("PlugIn called\n"));

            status= Bus_PlugInDevice((PBUSENUM_PLUGIN_HARDWARE)buffer,
                                inlen, fdoData);


        }
        break;

    case IOCTL_BUSENUM_UNPLUG_HARDWARE:

        if ((sizeof (BUSENUM_UNPLUG_HARDWARE) == inlen) &&
              (((PBUSENUM_UNPLUG_HARDWARE)buffer)->Size == inlen)) {

            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UnPlug called\n"));

            status= Bus_UnPlugDevice(
                    (PBUSENUM_UNPLUG_HARDWARE)buffer, fdoData);

        }
        break;

    case IOCTL_BUSENUM_EJECT_HARDWARE:

        if ((sizeof (BUSENUM_EJECT_HARDWARE) == inlen) &&
            (((PBUSENUM_EJECT_HARDWARE)buffer)->Size == inlen)) {

            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("Eject called\n"));

            status= Bus_EjectDevice((PBUSENUM_EJECT_HARDWARE)buffer, fdoData);

        }

#else

	case IOCTL_NDASBUS_PLUGIN_HARDWARE_EX2: {

		BOOLEAN						accepted;
		PNDASBUS_ADD_TARGET_DATA	ndasBusAddTargetData = &((PNDASBUS_PLUGIN_HARDWARE_EX2)buffer)->NdasBusAddTargetData;

		NdasDbgCall( 2, "IOCTL_NDASBUS_PLUGIN_HARDWARE_EX2\n" );

		// Make sure it has at least two nulls and the size
        // field is set to the declared size of the struct

		if ((sizeof(NDASBUS_PLUGIN_HARDWARE_EX2) + sizeof(UNICODE_NULL) * 2) > inlen) {
			
			break;
		}

		// The size field should be set to the sizeof the struct as declared
        // and *not* the size of the struct plus the multi_sz
        
        if (sizeof(NDASBUS_PLUGIN_HARDWARE_EX2) != ((PNDASBUS_PLUGIN_HARDWARE_EX2) buffer)->Size) {

			break;
		}

#ifdef _WIN64
			
		if (IoIs32bitProcess(Irp)) {

			((PNDASBUS_PLUGIN_HARDWARE_EX2)buffer)->DisconEvent = (HANDLE)((PNDASBUS_PLUGIN_HARDWARE_EX2)buffer)->DisconEvent32;
			((PNDASBUS_PLUGIN_HARDWARE_EX2)buffer)->AlarmEvent = (HANDLE)((PNDASBUS_PLUGIN_HARDWARE_EX2)buffer)->AlarmEvent32;
		}

#endif

		accepted = TRUE;

		switch (ndasBusAddTargetData->ucTargetType) {

		case NDASSCSI_TYPE_DISK_NORMAL: 
		case NDASSCSI_TYPE_VDVD: 
		case NDASSCSI_TYPE_ATAPI_CDROM: 
		case NDASSCSI_TYPE_ATAPI_DIRECTACC:
		case NDASSCSI_TYPE_ATAPI_SEQUANACC:
		case NDASSCSI_TYPE_ATAPI_PRINTER:
		case NDASSCSI_TYPE_ATAPI_PROCESSOR:
		case NDASSCSI_TYPE_ATAPI_WRITEONCE:
		case NDASSCSI_TYPE_ATAPI_SCANNER:
		case NDASSCSI_TYPE_ATAPI_OPTMEM:
		case NDASSCSI_TYPE_ATAPI_MEDCHANGER:
		case NDASSCSI_TYPE_ATAPI_COMM:
		case NDASSCSI_TYPE_ATAPI_ARRAYCONT:
		case NDASSCSI_TYPE_ATAPI_ENCLOSURE:
		case NDASSCSI_TYPE_ATAPI_RBC:
		case NDASSCSI_TYPE_ATAPI_OPTCARD: {

			if (ndasBusAddTargetData->ulNumberOfUnitDiskList != 1) {

				accepted = FALSE;
			}

			break;
		}

		case NDASSCSI_TYPE_DISK_MIRROR: {

			if (ndasBusAddTargetData->ulNumberOfUnitDiskList != 2) {

				accepted = FALSE;
			}

			break;
		}

		case NDASSCSI_TYPE_DISK_AGGREGATION: {

			if (ndasBusAddTargetData->ulNumberOfUnitDiskList < 2 || 
				ndasBusAddTargetData->ulNumberOfUnitDiskList > MAX_NR_UNITDISK_FOR_AGGR) {

				accepted = FALSE;
			}

			break;
		}

		case NDASSCSI_TYPE_DISK_RAID0: {
				
			switch (ndasBusAddTargetData->ulNumberOfUnitDiskList) {

			case 2:
			case 4:
			case 8:

				break;
				
			default: // do not accept
				
				accepted = FALSE;
				break;
			}

			break;
		}

		case NDASSCSI_TYPE_DISK_RAID1R3: {

			ULONG ulDiskCount;
			
			ulDiskCount = ndasBusAddTargetData->ulNumberOfUnitDiskList - ndasBusAddTargetData->RAID_Info.SpareDiskCount;
					
			if (2 != ulDiskCount) {
				
				accepted = FALSE;
			}
			
			break;
		}

		case NDASSCSI_TYPE_DISK_RAID4R3:
		case NDASSCSI_TYPE_DISK_RAID5: {

			ULONG ulDiskCount;
			
			ulDiskCount = ndasBusAddTargetData->ulNumberOfUnitDiskList - ndasBusAddTargetData->RAID_Info.SpareDiskCount;
			
			switch(ulDiskCount) {

			case 3: // 2 + 1
			case 5: // 4 + 1
			case 9: // 8 + 1
					
				break;
				
			default: // do not accept
				
				accepted = FALSE;
				break;
			}

			break;
		}				

		default:

			NDAS_ASSERT(FALSE);
			accepted = FALSE;
			break;
		}

		if (accepted == FALSE) {

			NDAS_ASSERT(FALSE);
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = Bus_PlugInDevice( (PNDASBUS_PLUGIN_HARDWARE_EX2)buffer,
								   inlen,
								   fdoData,
								   Irp->RequestorMode, 
								   FALSE );

		Irp->IoStatus.Information = outlen;
		break;
	}

#if 0
	case IOCTL_NDASBUS_PLUGIN_HARDWARE_EX2:
		{
			ULONG	structLen;		// Without variable length field
			ULONG	wholeStructLen; // With variable length field
			ULONG	inputWholeStructLen;

			//
			// Check 32 bit thunking request
            //
			if(IoIs32bitProcess(Irp)) {
				structLen = FIELD_OFFSET(NDASBUS_PLUGIN_HARDWARE_EX2_32, HardwareIDs);
				wholeStructLen = sizeof(NDASBUS_PLUGIN_HARDWARE_EX2_32);
				inputWholeStructLen = ((PNDASBUS_PLUGIN_HARDWARE_EX2_32) buffer)->Size;
			} else {
				structLen = FIELD_OFFSET(NDASBUS_PLUGIN_HARDWARE_EX2, HardwareIDs);
				wholeStructLen = sizeof(NDASBUS_PLUGIN_HARDWARE_EX2);
				inputWholeStructLen = ((PNDASBUS_PLUGIN_HARDWARE_EX2) buffer)->Size;
			}

			if ((inlen == outlen) &&
				//
				// Make sure it has at least two nulls and the size 
				// field is set to the declared size of the struct
				//
				((structLen + sizeof(UNICODE_NULL) * 2) <=
				inlen) &&

				//
				// The size field should be set to the sizeof the struct as declared
				// and *not* the size of the struct plus the multi_sz
				//
				(wholeStructLen == inputWholeStructLen)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("PlugIn called\n"));

				status= Bus_PlugInDevice((PNDASBUS_PLUGIN_HARDWARE_EX2)buffer,
											inlen,
											fdoData,
											IoIs32bitProcess(Irp),
											Irp->RequestorMode, FALSE);

				Irp->IoStatus.Information = outlen;

			}
		}
        break;
#endif

    case IOCTL_NDASBUS_UNPLUG_HARDWARE:

		NdasDbgCall( 2, "IOCTL_NDASBUS_EJECT_HARDWARE\n" );

        if (sizeof(NDASBUS_UNPLUG_HARDWARE) != inlen || ((PNDASBUS_UNPLUG_HARDWARE)buffer)->Size != inlen) {

			NDAS_ASSERT(FALSE);
			break;
		}

		status= Bus_UnPlugDevice( (PNDASBUS_UNPLUG_HARDWARE)buffer, fdoData );

		break;

	case IOCTL_NDASBUS_EJECT_HARDWARE:

		NdasDbgCall( 2, "IOCTL_NDASBUS_EJECT_HARDWARE\n" );

		if (sizeof(NDASBUS_EJECT_HARDWARE) != inlen || ((PNDASBUS_EJECT_HARDWARE)buffer)->Size != inlen) {

			NDAS_ASSERT(FALSE);
			break;
		}

		status= Bus_EjectDevice( (PNDASBUS_EJECT_HARDWARE)buffer, fdoData );

	case IOCTL_NDASBUS_STARTSTOP_REGISTRARENUM:{
		
		PULONG	onOff = (PULONG)buffer;

		NdasDbgCall( 2, "IOCTL_NDASBUS_STARTSTOP_REGISTRARENUM: inlen %d, outlen %d OnOff %u\n", inlen, outlen, *onOff );

		KeEnterCriticalRegion();
		ExAcquireFastMutexUnsafe(&fdoData->RegMutex);

		if (*onOff != 0) {

			//
			//	Save old state.
			//	Activate the registrar's enumeration
			//

			*onOff = fdoData->StartStopRegistrarEnum;
			fdoData->StartStopRegistrarEnum = TRUE;
		
		} else {

			//
			//	Save old state.
			//	Deactivate the registrar's enumeration
			//

			*onOff = fdoData->StartStopRegistrarEnum;
			fdoData->StartStopRegistrarEnum = FALSE;
		}

		//
		//	Clean up non-enumerated entries.
		//
		LSBus_CleanupNDASDeviceRegistryUnsafe(fdoData);

		ExReleaseFastMutexUnsafe(&fdoData->RegMutex);
		KeLeaveCriticalRegion();

		Irp->IoStatus.Information = sizeof(ULONG);
		status = STATUS_SUCCESS;

		break;
	}

	case IOCTL_NDASBUS_REGISTER_DEVICE:
	{

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
							"REGISTER_DEVICE: inlen %d, outlen %d,"
							" sizeof(NDASBUS_PLUGIN_HARDWARE_EX2) %d\n",
							inlen, outlen, sizeof(NDASBUS_PLUGIN_HARDWARE_EX2)));
		if ((inlen == outlen)) {

			PNDASBUS_PLUGIN_HARDWARE_EX2	PlugIn = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("REGISTER_DEVICE: entered\n"));

			status = LSBus_RegisterDevice(fdoData, PlugIn);

			Irp->IoStatus.Information = 0;
		}
		else
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR,
									("REGISTER_DEVICE: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(NDASBUS_PLUGIN_HARDWARE_EX2) %d\n",
									inlen, outlen, sizeof(NDASBUS_PLUGIN_HARDWARE_EX2)));

	}
		break;
	case IOCTL_NDASBUS_REGISTER_TARGET:
	{

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"REGISTER_TARGET: inlen %d, outlen %d,"
									" sizeof(NDASBUS_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(NDASBUS_ADD_TARGET_DATA)));
		if ((inlen == outlen)) {

			PNDASBUS_ADD_TARGET_DATA	AddTargetData = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("REGISTER_TARGET: entered\n"));

			status = LSBus_RegisterTarget(fdoData, AddTargetData);

			Irp->IoStatus.Information = 0;
		}
		else {
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"REGISTER_TARGET: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(NDASBUS_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(NDASBUS_ADD_TARGET_DATA)));
		}

		break;
	}

	case IOCTL_NDASBUS_UNREGISTER_DEVICE: {

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_DEVICE: inlen %d, outlen %d,"
									" sizeof(NDASBUS_UNREGISTER_NDASDEV) %d\n",
									inlen, outlen, sizeof(NDASBUS_UNREGISTER_NDASDEV)));
		if ((inlen == outlen)) {

			PNDASBUS_UNREGISTER_NDASDEV	UnregDev = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UNREGISTER_DEVICE: entered\n"));

			status = LSBus_UnregisterDevice(fdoData, UnregDev->SlotNo);

			Irp->IoStatus.Information = 0;
		}
		else {
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_DEVICE: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(NDASBUS_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(NDASBUS_UNREGISTER_NDASDEV)));
		}
	}
	break;
	case IOCTL_NDASBUS_UNREGISTER_TARGET:
	{
		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_TARGET: inlen %d, outlen %d,"
									" sizeof(NDASBUS_UNREGISTER_TARGET) %d\n",
									inlen, outlen, sizeof(NDASBUS_UNREGISTER_TARGET)));
		if ((inlen == outlen)) {

			PNDASBUS_UNREGISTER_TARGET	UnregTarget = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UNREGISTER_TARGET: entered\n"));

			status = LSBus_UnregisterTarget(fdoData, UnregTarget->SlotNo, UnregTarget->TargetId);

			Irp->IoStatus.Information = 0;
		}
		else {
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_TARGET: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(NDASBUS_UNREGISTER_TARGET) %d\n",
									inlen, outlen, sizeof(NDASBUS_UNREGISTER_TARGET)));
		}
	}
	break;

	case IOCTL_NDASBUS_SETPDOINFO: {

	    PPDO_DEVICE_DATA	pdoData;
		PNDASBUS_SETPDOINFO	SetPdoInfo;
		KIRQL				oldIrql;
		PVOID				sectionHandle;
		BOOLEAN				acceptStatus;

        NdasDbgCall( 2, "IOCTL_NDASBUS_SETPDOINFO\n" );

		if (sizeof (NDASBUS_SETPDOINFO) != inlen) {

			break;
		}

		acceptStatus = TRUE;
		SetPdoInfo = (PNDASBUS_SETPDOINFO)buffer;
		pdoData = LookupPdoData(fdoData, SetPdoInfo->SlotNo);
		
		if (pdoData == NULL) {

			NDAS_ASSERT( SetPdoInfo->AdapterStatus == NDASSCSI_ADAPTER_STATUS_STOPPED );

			Bus_KdPrint_Cont ( fdoData, BUS_DBG_IOCTL_ERROR, ("no pdo\n") );

			status = STATUS_UNSUCCESSFUL;

			NDBusIoctlLogError(	fdoData->Self, NDASBUS_IO_PDO_NOT_FOUND, IOCTL_NDASBUS_SETPDOINFO, SetPdoInfo->SlotNo );

			break;
		}

		//	lock the code section of this function to acquire spinlock in raised IRQL.

		sectionHandle = MmLockPagableCodeSection(Bus_IoCtl);

		KeAcquireSpinLock(&pdoData->LanscsiAdapterPDO.LSDevDataSpinLock, &oldIrql);

		NdasDbgCall( 2, "SETPDOINFO: PDO %p: %08lx %08lx %08lx\n",
						 pdoData->Self, SetPdoInfo->AdapterStatus, 
						 SetPdoInfo->SupportedFeatures, 
						 SetPdoInfo->EnabledFeatures );

		// Deny the status change if the current status is STATUS_STOPPED except for RESETSTATUS flag.

		if (ADAPTERINFO_ISSTATUS(pdoData->LanscsiAdapterPDO.LastAdapterStatus, NDASSCSI_ADAPTER_STATUS_STOPPED)) {

			NdasDbgCall( 2, "SETPDOINFO: 'An event occured after 'Stopped' event\n" );


			if (ADAPTERINFO_ISSTATUSFLAG(SetPdoInfo->AdapterStatus, NDASSCSI_ADAPTER_STATUSFLAG_RESETSTATUS)) {
			
				NdasDbgCall( 2, "SETPDOINFO: Reset-status event accepted\n" );

				acceptStatus = TRUE;

			} else {
			
				acceptStatus = FALSE;
			}

		} else {

			if (pdoData->LanscsiAdapterPDO.AddDevInfoLength == 0) {

				NDAS_ASSERT(FALSE);
				acceptStatus = FALSE;

				NdasDbgCall( 2, "SETPDOINFO: AddTarget is not occured. Too early to set status.\n" );
			}
		}

		// Mask off RESETSTATUS.
		// NDAS service does not need to know it.

		SetPdoInfo->AdapterStatus &= ~NDASSCSI_ADAPTER_STATUSFLAG_RESETSTATUS;

		// Set status values to the corresponding physical device object.
		// Save to the extension

		if (acceptStatus) {

			PNDASBUS_PDOEVENT_ENTRY	pdoEventEntry;

			pdoEventEntry = NdasBusCreatePdoStatusItem(SetPdoInfo->AdapterStatus);

			if (pdoEventEntry) {

				NdasBusQueuePdoStatusItem(&pdoData->LanscsiAdapterPDO, pdoEventEntry);
			}
#if DBG
			else {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("SETPDOINFO: Could not allocate PDO status entry.\n"));
			}
#endif

			pdoData->LanscsiAdapterPDO.LastAdapterStatus = SetPdoInfo->AdapterStatus;
			pdoData->LanscsiAdapterPDO.SupportedFeatures = SetPdoInfo->SupportedFeatures;
			pdoData->LanscsiAdapterPDO.EnabledFeatures = SetPdoInfo->EnabledFeatures;

			// Queue plugout worker if the NDAS SCSI stop abnormally.
			// Notify the NDAS service of abnormal termination

			if (ADAPTERINFO_ISSTATUS(SetPdoInfo->AdapterStatus, NDASSCSI_ADAPTER_STATUS_STOPPED) &&
				ADAPTERINFO_ISSTATUSFLAG(SetPdoInfo->AdapterStatus, NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT)) {

				NdasDbgCall( 2, "SETPDOINFO: Queueing Unplug worker!!!!!!!!\n" );

				status = QueueUnplugWorker(fdoData, SetPdoInfo->SlotNo);

				// Set disconnection event

				KeSetEvent(pdoData->LanscsiAdapterPDO.DisconEventToService, IO_DISK_INCREMENT, FALSE);

			} else {

				// Notify the adapter status change

				KeSetEvent(pdoData->LanscsiAdapterPDO.AlarmEventToService, IO_DISK_INCREMENT, FALSE);
			}
		}

		KeReleaseSpinLock(&pdoData->LanscsiAdapterPDO.LSDevDataSpinLock, oldIrql);

		//	Release the code section.

	    MmUnlockPagableImageSection(sectionHandle);

		status = STATUS_SUCCESS;

		InterlockedDecrement( &pdoData->ReferenceCount );
		ObDereferenceObject(pdoData->Self);

        Irp->IoStatus.Information = outlen;

		break;
	}

	case IOCTL_NDASBUS_QUERY_NODE_ALIVE: {

		PPDO_DEVICE_DATA		pdoData;
		BOOLEAN					bAlive;
		PNDASBUS_NODE_ALIVE_IN	pNodeAliveIn;
		NDASBUS_NODE_ALIVE_OUT	nodeAliveOut;

		// Check Parameter.
		if(inlen != sizeof(NDASBUS_NODE_ALIVE_IN) || 
			outlen != sizeof(NDASBUS_NODE_ALIVE_OUT)) {
			status = STATUS_UNKNOWN_REVISION;
			break;
		}
		
		pNodeAliveIn = (PNDASBUS_NODE_ALIVE_IN)Irp->AssociatedIrp.SystemBuffer;  
		
		Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_NOISE,
			("FDO: IOCTL_NDASBUS_QUERY_NODE_ALIVE SlotNumber = %d\n",
			pNodeAliveIn->SlotNo));
		
		pdoData = LookupPdoData(fdoData, pNodeAliveIn->SlotNo);

		if(pdoData == NULL) {

			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_TRACE,
							  ("[LanScsiBus]Bus_IoCtl: IOCTL_NDASBUS_QUERY_NODE_ALIVE No pdo\n"));
			
			bAlive = FALSE;

		} else {

			// Check this PDO would be removed...

			if (pdoData->Present == TRUE) {

				bAlive = TRUE;
			
			} else {

				bAlive = FALSE;
			}
		}

		// For Result...

		nodeAliveOut.SlotNo = pNodeAliveIn->SlotNo;
		nodeAliveOut.bAlive = bAlive;

		// Get Adapter Status.

		if (bAlive == TRUE) {

			if (ADAPTERINFO_ISSTATUS(pdoData->LanscsiAdapterPDO.LastAdapterStatus, NDASSCSI_ADAPTER_STATUS_STOPPING)) {

				nodeAliveOut.bHasError = TRUE;
				Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
					("IOCTL_NDASBUS_QUERY_NODE_ALIVE Adapter has Error 0x%x\n", nodeAliveOut.bHasError));
			
			} else {
			
				nodeAliveOut.bHasError = FALSE;
			}
		}

		if (pdoData) {

			InterlockedDecrement( &pdoData->ReferenceCount );
			ObDereferenceObject(pdoData->Self);
		}

		RtlCopyMemory(
			Irp->AssociatedIrp.SystemBuffer,
			&nodeAliveOut,
			sizeof(NDASBUS_NODE_ALIVE_OUT)
			);
		
		Irp->IoStatus.Information = sizeof(NDASBUS_NODE_ALIVE_OUT);
		status = STATUS_SUCCESS;
		}
		break;

	//
	//	added by hootch 01172004
	//
	case IOCTL_NDASBUS_UPGRADETOWRITE:
		{
		PPDO_DEVICE_DATA				pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_NDASBUS_UPGRADETOWRITE called\n"));
		// Check Parameter.
		if(inlen != sizeof(NDASBUS_UPGRADE_TO_WRITE)) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_NDASBUS_UPGRADETOWRITE: Invalid input buffer length\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		pdoData = LookupPdoData(fdoData, ((PNDASBUS_UPGRADE_TO_WRITE)buffer)->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_NDASBUS_UPGRADETOWRITE: No pdo for Slotno:%d\n", ((PNDASBUS_UPGRADE_TO_WRITE)buffer)->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_NDASBUS_UPGRADETOWRITE,
				((PNDASBUS_UPGRADE_TO_WRITE)buffer)->SlotNo);
		} else {
			//
			//	redirect to the NDASSCSI Device
			//
			status = LSBus_IoctlToNdasScsiDevice(
					pdoData,
					NDASSCSI_IOCTL_UPGRADETOWRITE,
					buffer,
					inlen,
					buffer,
					outlen
				);

			InterlockedDecrement( &pdoData->ReferenceCount );
			ObDereferenceObject(pdoData->Self);
		}
		Irp->IoStatus.Information = 0;
	}
		break;

	case IOCTL_NDASBUS_REDIRECT_NDASSCSI:
		{
		PPDO_DEVICE_DATA				pdoData;
		PNDASBUS_REDIRECT_NDASSCSI		redirectIoctl;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_NDASBUS_REDIRECT_NDASSCSI called\n"));
		// Check Parameter.
		if(inlen < sizeof(NDASBUS_REDIRECT_NDASSCSI)) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_NDASBUS_REDIRECT_NDASSCSI: Invalid input buffer length\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		redirectIoctl = (PNDASBUS_REDIRECT_NDASSCSI)buffer;

		pdoData = LookupPdoData(fdoData, redirectIoctl->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_NDASBUS_REDIRECT_NDASSCSI: No pdo for Slotno:%d\n", redirectIoctl->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
		} else {
			//
			//	redirect to the NDASSCSI Device
			//
			status = LSBus_IoctlToNdasScsiDevice(
					pdoData,
					redirectIoctl->IoctlCode,
					redirectIoctl->IoctlData,
					redirectIoctl->IoctlDataSize,
					redirectIoctl->IoctlData,
					redirectIoctl->IoctlDataSize
				);

			InterlockedDecrement( &pdoData->ReferenceCount );
			ObDereferenceObject(pdoData->Self);
		}
		Irp->IoStatus.Information = 0;
	}
		break;

	case IOCTL_NDASBUS_QUERY_NDASSCSIINFO:
		{
		PPDO_DEVICE_DATA				pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_NDASBUS_QUERY_NDASSCSIINFO called\n"));
		// Check Parameter.
		if(inlen < FIELD_OFFSET(NDASSCSI_QUERY_INFO_DATA, QueryData) ) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_NDASBUS_QUERY_NDASSCSIINFO: Invalid input buffer length too small.\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}
		pdoData = LookupPdoData(fdoData, ((PNDASSCSI_QUERY_INFO_DATA)buffer)->NdasScsiAddress.SlotNo);

		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_NDASBUS_QUERY_NDASSCSIINFO No pdo\n"));
			status = STATUS_NO_SUCH_DEVICE;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_NDASBUS_QUERY_NDASSCSIINFO,
				((PNDASSCSI_QUERY_INFO_DATA)buffer)->NdasScsiAddress.SlotNo);
		} else {
			//
			//	redirect to the NDASSCSI Device
			//
			status = LSBus_IoctlToNdasScsiDevice(
					pdoData,
					NDASSCSI_IOCTL_QUERYINFO_EX,
					buffer,
					inlen,
					buffer,
					outlen
				);

			InterlockedDecrement( &pdoData->ReferenceCount );
			ObDereferenceObject(pdoData->Self);
		}
        Irp->IoStatus.Information = outlen;
		}
		break;

	case IOCTL_NDASBUS_QUERY_INFORMATION: {

		NDASBUS_QUERY_INFORMATION	Query;
		PNDASBUS_INFORMATION		Information;
		LONG						BufferLenNeeded;

		NdasDbgCall( 2, "IOCTL_NDASBUS_QUERY_INFORMATION\n" );

		// Check Parameter.
		
		if (inlen < sizeof(NDASBUS_QUERY_INFORMATION) /*|| outlen < sizeof(NDASBUS_INFORMATION) */) {
			
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		RtlCopyMemory( &Query, buffer, sizeof(NDASBUS_QUERY_INFORMATION) );

		Bus_KdPrint_Cont( fdoData, BUS_DBG_IOCTL_TRACE,
						  ("FDO: IOCTL_NDASBUS_QUERY_INFORMATION QueryType : %d  SlotNumber = %d\n",
						  Query.InfoClass, Query.SlotNo) );

		Information = (PNDASBUS_INFORMATION)buffer;

		NDAS_ASSERT(Information);

		Information->InfoClass = Query.InfoClass;
		
		status = LSBus_QueryInformation( fdoData, IoIs32bitProcess(Irp), &Query, Information, outlen, &BufferLenNeeded );
		
		if (NT_SUCCESS(status)) {

			Information->Size = BufferLenNeeded;
			Irp->IoStatus.Information = BufferLenNeeded;

		} else {

			Irp->IoStatus.Information = BufferLenNeeded;
		}

		break;
	}

	case IOCTL_NDASBUS_GETVERSION:
		{
			if (outlen >= sizeof(NDASBUS_GET_VERSION)) {
				PNDASBUS_GET_VERSION version = (PNDASBUS_GET_VERSION)buffer;

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_NDASBUS_GETVERSION: called\n"));

			try {
				version->VersionMajor = VER_FILEMAJORVERSION;
				version->VersionMinor = VER_FILEMINORVERSION;
				version->VersionBuild = VER_FILEBUILD;
				version->VersionPrivate = VER_FILEBUILD_QFE;

					Irp->IoStatus.Information = sizeof(NDASBUS_GET_VERSION);
					status = STATUS_SUCCESS;

				} except (EXCEPTION_EXECUTE_HANDLER) {

					status = GetExceptionCode();
					Irp->IoStatus.Information = 0;
				}

			}
		}			
		break;

	case IOCTL_NDASBUS_DVD_GET_STATUS: {

		PPDO_DEVICE_DATA		pdoData;
		PNDASBUS_DVD_STATUS		pDvdStatusData;


		// Check Parameter.
	
		if ((inlen != outlen) || (sizeof(NDASBUS_DVD_STATUS) >  inlen)) {

			status = STATUS_UNSUCCESSFUL ;
			break;
		}
			
		pDvdStatusData = (PNDASBUS_DVD_STATUS)Irp->AssociatedIrp.SystemBuffer;  
			
		Bus_KdPrint_Cont( fdoData, BUS_DBG_IOCTL_ERROR, 
						  ("FDO: IOCTL_NDASBUS_DVD_GET_STATUS SlotNumber = %d\n", pDvdStatusData->SlotNo) );	

		pdoData = LookupPdoData( fdoData, pDvdStatusData->SlotNo );
			
		if (pdoData == NULL) {

			Bus_KdPrint_Cont( fdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_NDASBUS_DVD_GET_STATUS No pdo\n") );

			status = STATUS_UNSUCCESSFUL;

			NDBusIoctlLogError(	fdoData->Self, 
								NDASBUS_IO_PDO_NOT_FOUND, 
								IOCTL_NDASBUS_DVD_GET_STATUS, 
								pDvdStatusData->SlotNo );
			break;
		}

		//	ADD_TARGET_DATA is set.

		if (pdoData->LanscsiAdapterPDO.AddDevInfo.ucTargetType != NDASSCSI_TYPE_ATAPI_CDROM) {

			Bus_KdPrint_Cont( fdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_NDASBUS_DVD_GET_STATUS  No DVD Device\n") );

			InterlockedDecrement( &pdoData->ReferenceCount );
			ObDereferenceObject(pdoData->Self);

			status = STATUS_UNSUCCESSFUL;
			break;
		}
	
		//	redirect to the NDASSCSI Device
				
		status = LSBus_IoctlToNdasScsiDevice( pdoData,
											  NDASSCSI_IOCTL_GET_DVD_STATUS,
											  buffer,
											  inlen,
											  buffer,
											  outlen );

		InterlockedDecrement( &pdoData->ReferenceCount );
		ObDereferenceObject( pdoData->Self );

		status = STATUS_SUCCESS;
		Irp->IoStatus.Information = outlen;

		break;
	}

#endif

    default:

        break; // default status is STATUS_INVALID_PARAMETER
    }

#if !__NDAS_SCSI_BUS__
    Irp->IoStatus.Information = 0;
#endif

END:
    Irp->IoStatus.Status = status;

	if(Irp->UserIosb)
		*Irp->UserIosb = Irp->IoStatus;

    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    Bus_DecIoCount (fdoData);
    return status;
}


VOID
Bus_DriverUnload (
    __in PDRIVER_OBJECT DriverObject
    )
/*++
Routine Description:
    Clean up everything we did in driver entry.

Arguments:

   DriverObject - pointer to this driverObject.


Return Value:

--*/
{
    PAGED_CODE();

	UNREFERENCED_PARAMETER(DriverObject);

#if DBG
	DbgPrint( "\n************NdasBus %s %s %s\n", __FUNCTION__, __DATE__, __TIME__ );
#endif

    // All the device objects should be gone.

    NDAS_ASSERT( DriverObject->DeviceObject == NULL );

    // Here we free all the resources allocated in the DriverEntry

	if (Globals.RegistryPath.Buffer) {

        ExFreePool(Globals.RegistryPath.Buffer);
	}

#if DBG
	DbgPrint( "\n************NdasBus %s %s %s return\n", __FUNCTION__, __DATE__, __TIME__ );
#endif

    return;
}

VOID
Bus_IncIoCount (
    __in  PFDO_DEVICE_DATA   FdoData
    )

/*++

Routine Description:

    This routine increments the number of requests the device receives


Arguments:

    FdoData - pointer to the FDO device extension.

Return Value:

    VOID

--*/

{

    LONG            result;


    result = InterlockedIncrement(&FdoData->OutstandingIO);

    ASSERT(result > 0);
    //
    // Need to clear StopEvent (when OutstandingIO bumps from 1 to 2)
    //
    if (result == 2) {
        //
        // We need to clear the event
        //
        KeClearEvent(&FdoData->StopEvent);
    }

    return;
}

VOID
Bus_DecIoCount(
    __in  PFDO_DEVICE_DATA  FdoData
    )

/*++

Routine Description:

    This routine decrements as it complete the request it receives

Arguments:

    FdoData - pointer to the FDO device extension.

Return Value:

    VOID

--*/
{

    LONG            result;

    result = InterlockedDecrement(&FdoData->OutstandingIO);

    ASSERT(result >= 0);

    if (result == 1) {
        //
        // Set the stop event. Note that when this happens
        // (i.e. a transition from 2 to 1), the type of requests we
        // want to be processed are already held instead of being
        // passed away, so that we can't "miss" a request that
        // will appear between the decrement and the moment when
        // the value is actually used.
        //

        KeSetEvent (&FdoData->StopEvent, IO_NO_INCREMENT, FALSE);

    }

    if (result == 0) {

        //
        // The count is 1-biased, so it can be zero only if an
        // extra decrement is done when a remove Irp is received
        //

        ASSERT(FdoData->DevicePnPState == Deleted);

        //
        // Set the remove event, so the device object can be deleted
        //

        KeSetEvent (&FdoData->RemoveEvent, IO_NO_INCREMENT, FALSE);

    }

    return;
}


