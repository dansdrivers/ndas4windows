/*++

Copyright (c) 1990-2000  Microsoft Corporation All Rights Reserved

Module Name:

    BUSENUM.C

Abstract:

    This module contains the entry points for a toaster bus driver.

Author:

    Eliyas Yakub    Sep 10, 1998
    
Environment:

    kernel mode only

Revision History:

    Cleaned up sample 05/05/99


--*/

#include <ntddk.h>
#include "devreg.h"
#include "ndasbus.h"
#include "ndasscsiioctl.h"
#include "busenum.h"
#include <stdarg.h>
#include "stdio.h"
#include "public\ndas\ndasiomsg.h"
#include "ndasbuspriv.h"

// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions
#include "ndasbus.ver"
#include <ndasverp.h>

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "BusEnum"

//
// Global Debug Level
//

#define DEBUG_BUFFER_LENGTH 256
ULONG BusEnumDebugLevel = BUS_DEFAULT_DEBUG_OUTPUT_LEVEL;

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


//
//	Driver-wide variable
//

GLOBALS Globals;

PPDO_DEVICE_DATA
LookupPdoData(
	PFDO_DEVICE_DATA	FdoData,
	ULONG				SystemIoBusNumber);
VOID
CleanupPdoEnumRegistryUnsafe(
			PFDO_DEVICE_DATA	FdoData);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, Bus_DriverUnload)
#pragma alloc_text (PAGE, Bus_CreateClose)
#pragma alloc_text (PAGE, Bus_IoCtl)
#pragma alloc_text (PAGE, LookupPdoData)
#endif

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
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

    Bus_KdPrint_Def (BUS_DBG_SS_INFO, ("%s, %s\n", __DATE__, __TIME__));

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

    return STATUS_SUCCESS;
}

NTSTATUS
Bus_CreateClose (
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
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

    PAGED_CODE ();

	Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Entered. \n"));

    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;
    
    Bus_IncIoCount (fdoData);

    //
    // If it's not for the FDO. We don't allow create/close on PDO
    // 
    if (fdoData->IsFDO) {
          
        //
        // Check to see whether the bus is removed
        //
       
        if (fdoData->DevicePnPState == Deleted){         
            status = STATUS_NO_SUCH_DEVICE;
        } else {

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
                break;
            } 
        }
        
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    Bus_DecIoCount (fdoData);

    return status;
}


//
//	Increment the reference count to a PDO.
//
PPDO_DEVICE_DATA
LookupPdoData(
	PFDO_DEVICE_DATA	FdoData,
	ULONG				SystemIoBusNumber
	)
{
	PPDO_DEVICE_DATA	pdoData = NULL;
    PLIST_ENTRY         entry;

    PAGED_CODE ();

    KeEnterCriticalRegion();
	ExAcquireFastMutex (&FdoData->Mutex);

    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink) {

			 pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
             if(pdoData->SlotNo == SystemIoBusNumber)
				 break;
			pdoData = NULL;
		 }
	 if(pdoData) {
		 //
		 //	increment the reference count to the PDO.
		 //
		 ObReferenceObject(pdoData->Self);
	 }

	ExReleaseFastMutex (&FdoData->Mutex);
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


//
//	Unplugging worker function
//

VOID
UnplugWorker(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PVOID			Context
){
	NTSTATUS				status;
	PNDBUS_UNPLUGWORKER		ctx = (PNDBUS_UNPLUGWORKER)Context;
	BUSENUM_UNPLUG_HARDWARE	unplug;


	UNREFERENCED_PARAMETER(DeviceObject);


	//
	//	IO_WORKITEM is rare resource, give it back to the system now.
	//

	IoFreeWorkItem(ctx->IoWorkItem);


	//
	//	Start unplug
	//

	unplug.Size = sizeof(unplug);
	unplug.SlotNo = ctx->SlotNo;
	status = Bus_UnPlugDevice(&unplug, ctx->FdoData);
	ASSERT(NT_SUCCESS(status));

	ExFreePool(Context);

}


//
//	Queue a workitem to unplug a NDAS device.
//

#define NDBUS_POOLTAG_UNPLUGWORKITEM	'iwSL'

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

NTSTATUS
Bus_IoCtl (
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
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
    ULONG                   inlen, outlen;
    PFDO_DEVICE_DATA        fdoData;
    PVOID                   buffer;

    PAGED_CODE ();
    
    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // We only take Device Control requests for the FDO.
    // That is the bus itself.
    //

    if (!fdoData->IsFDO) {
    
        //
        // These commands are only allowed to go to the FDO.
        //   
        status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;

    }

    //
    // Check to see whether the bus is removed
    //
    
    if (fdoData->DevicePnPState == Deleted) {
        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }
    
    Bus_IncIoCount (fdoData);
    
    irpStack = IoGetCurrentIrpStackLocation (Irp);

    buffer			= Irp->AssociatedIrp.SystemBuffer;  
    inlen			= irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outlen			= irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    status = STATUS_INVALID_PARAMETER;

	Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("%d called\n", irpStack->Parameters.DeviceIoControl.IoControlCode));
    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) 
	{

	case IOCTL_LANSCSI_ADD_TARGET:
	{
		PPDO_DEVICE_DATA	pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_LANSCSI_ADD_TARGET inlen %d, outlen %d, sizeof(LANSCSI_ADD_TARGET_DATA) %d\n", inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));

		if ((inlen == outlen) &&
			(sizeof(LANSCSI_ADD_TARGET_DATA) <= inlen)) 
		{
			ULONG						ulSize;
			PLANSCSI_ADD_TARGET_DATA	addTargetData = buffer;
			BOOLEAN						accepted;
			
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_ADD_TARGET called\n"));
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_LANSCSI_ADD_TARGET Target Type %d\n", addTargetData->ucTargetType));

			status = STATUS_SUCCESS;
			
			//
			// Check structure size
			//
			if(VerifySizeOfAddTargetData(addTargetData, &ulSize) == 0) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if(ulSize != inlen) 
			{
				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("ADD_TARGET: Size mismatch. Req %d, in %d\n", ulSize, inlen));
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			//
			//	Check to see if acceptable structure.
			//

			accepted = TRUE;
			switch(addTargetData->ucTargetType) 
			{
			case NDASSCSI_TYPE_DISK_NORMAL: 
			case NDASSCSI_TYPE_DVD: 
			case NDASSCSI_TYPE_VDVD: 
			case NDASSCSI_TYPE_MO:

				if(addTargetData->ulNumberOfUnitDiskList != 1)
					accepted = FALSE;
				break;

			case NDASSCSI_TYPE_DISK_MIRROR:

				if(2 != addTargetData->ulNumberOfUnitDiskList)
					accepted = FALSE;

				break;

			case NDASSCSI_TYPE_DISK_AGGREGATION:

				if (addTargetData->ulNumberOfUnitDiskList < 2 || 
					addTargetData->ulNumberOfUnitDiskList > MAX_NR_TC_PER_TARGET) 
					accepted = FALSE;
				
				break;

			case NDASSCSI_TYPE_DISK_RAID0:
				switch(addTargetData->ulNumberOfUnitDiskList)
				{
				case 2:
				case 4:
				case 8:
					break;
				default: // do not accept
					accepted = FALSE;
					break;
				}
				break;
			case NDASSCSI_TYPE_DISK_RAID1R:
				{
					ULONG						ulDiskCount;
					ulDiskCount = addTargetData->ulNumberOfUnitDiskList - 
						addTargetData->RAID_Info.nSpareDisk;
					if (2 != ulDiskCount) 
						accepted = FALSE;
				}
				break;
			case NDASSCSI_TYPE_DISK_RAID4R:
				{
					ULONG						ulDiskCount;
					ulDiskCount = addTargetData->ulNumberOfUnitDiskList - 
						addTargetData->RAID_Info.nSpareDisk;
					switch(ulDiskCount)
				{
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
				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("ADD_TARGET: Bad Disk Type.\n"));
				accepted = FALSE;
				break;
			}
			if(accepted == FALSE) {
				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("ADD_TARGET: Invaild type.\n"));
				status = STATUS_UNSUCCESSFUL;
				break;
			}
						
#if DBG
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_TRY_TO_ADDTARGET,
				IOCTL_LANSCSI_ADD_TARGET,
				addTargetData->ulSlotNo);
#endif
			// Find Pdo Data...
			pdoData = LookupPdoData(fdoData, addTargetData->ulSlotNo);
			if(pdoData == NULL) 
			{
				Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
					("no pdo\n"));
				status = STATUS_UNSUCCESSFUL;
				NDBusIoctlLogError(	fdoData->Self,
									NDASBUS_IO_PDO_NOT_FOUND,
									IOCTL_LANSCSI_ADD_TARGET,
									addTargetData->ulSlotNo);
				break;
			}

			pdoData->LanscsiAdapterPDO.AddDevInfo = ExAllocatePool(NonPagedPool, inlen);
			
			if(pdoData->LanscsiAdapterPDO.AddDevInfo == NULL) 
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
			else 
			{
				RtlCopyMemory(pdoData->LanscsiAdapterPDO.AddDevInfo, addTargetData, inlen);
				status = STATUS_SUCCESS;
			}

			pdoData->LanscsiAdapterPDO.AddDevInfoLength = inlen;

			//
			//	Notify to LanscsiMiniport
			//
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_LANSCSI_ADD_TARGET SetEvent AddTargetEvent!\n"));
			KeSetEvent(&pdoData->LanscsiAdapterPDO.AddTargetEvent, IO_NO_INCREMENT, FALSE);

			//
			//	Register Target
			//
			if(pdoData->Persistent) {
				status = LSBus_RegisterTarget(fdoData, addTargetData);
				if(!NT_SUCCESS(status)) {
					Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("ADD_TARGET: LSBus_RegisterTarget() failed. STATUS=%08lx\n", status));
					status = STATUS_INTERNAL_DB_ERROR;
					NDBusIoctlLogError(	fdoData->Self,
										NDASBUS_IO_REGISTER_TARGET_FAIL,
										IOCTL_LANSCSI_ADD_TARGET,
										addTargetData->ulSlotNo);
				} else {
					Bus_KdPrint(fdoData, BUS_DBG_IOCTL_INFO, ("ADD_TARGET: Successfully registered.\n"));
				}
			}

			ObDereferenceObject(pdoData->Self);

			Irp->IoStatus.Information = outlen;
		}        
		else
		{
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR,
								("IOCTL_LANSCSI_ADD_TARGET length mismatch!!!"
								" inlen %d, outlen %d, sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
								inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		}

	}
		break;

	case IOCTL_LANSCSI_REMOVE_TARGET:
	{
	    PPDO_DEVICE_DATA	pdoData;
		PLANSCSI_REMOVE_TARGET_DATA	removeTarget;

        Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_REMOVE_TARGET called\n"));

        if (sizeof (LANSCSI_REMOVE_TARGET_DATA) != inlen)
			break;

		removeTarget = (PLANSCSI_REMOVE_TARGET_DATA)buffer;
		pdoData = LookupPdoData(fdoData, removeTarget->ulSlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
						("no pdo\n"));
			status = STATUS_UNSUCCESSFUL;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_LANSCSI_REMOVE_TARGET,
				removeTarget->ulSlotNo);
			break;
		}

		//
		//	redirect to the NDAS SCSI Device
		//
		status = LSBus_IoctlToLSMPDevice(
				pdoData,
				LANSCSIMINIPORT_IOCTL_REMOVE_TARGET,
				buffer,
				sizeof(LANSCSI_REMOVE_TARGET_DATA),
				NULL,
				0
			);

		if(NT_SUCCESS(status) && pdoData->Persistent) {

			status = LSBus_UnregisterTarget(fdoData, removeTarget->ulSlotNo, removeTarget->ulTargetId);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_INFO, (	"REMOVE_TARGET: Removed  Target instance,"
															" but LSBus_UnregisterTarget() failed.\n"));
				status = STATUS_INTERNAL_DB_ERROR;
				NDBusIoctlLogError(	fdoData->Self,
					NDASBUS_IO_UNREGISTER_TARGET_FAIL,
					IOCTL_LANSCSI_REMOVE_TARGET,
					removeTarget->ulSlotNo);
			}
#if DBG
			else {
				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_INFO, ("REMOVE_TARGET: LSBus_UnregisterTarget() succeeded.\n"));
			}
#endif
		}

		ObDereferenceObject(pdoData->Self);

        Irp->IoStatus.Information = 0;
		break;
	}

	case IOCTL_LANSCSI_STARTSTOP_REGISTRARENUM:{
		PULONG	onOff = (PULONG)buffer;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
			"STARTSTOP_REGISTRARENUM: inlen %d, outlen %d OnOff %u\n", inlen, outlen, *onOff));

		KeEnterCriticalRegion();
		ExAcquireFastMutexUnsafe(&fdoData->RegMutex);

		if(*onOff != 0) {

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

	case IOCTL_LANSCSI_REGISTER_DEVICE:
	{

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
							"REGISTER_DEVICE: inlen %d, outlen %d,"
							" sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
							inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		if ((inlen == outlen)) {

			PBUSENUM_PLUGIN_HARDWARE_EX2	PlugIn = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("REGISTER_DEVICE: entered\n"));

			status = LSBus_RegisterDevice(fdoData, PlugIn);

			Irp->IoStatus.Information = 0;
		}
		else
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR,
									("REGISTER_DEVICE: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));

	}
		break;
	case IOCTL_LANSCSI_REGISTER_TARGET:
	{

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"REGISTER_TARGET: inlen %d, outlen %d,"
									" sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		if ((inlen == outlen)) {

			PLANSCSI_ADD_TARGET_DATA	AddTargetData = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("REGISTER_TARGET: entered\n"));

			status = LSBus_RegisterTarget(fdoData, AddTargetData);

			Irp->IoStatus.Information = 0;
		}
		else {
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"REGISTER_TARGET: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		}

	}
		break;
	case IOCTL_LANSCSI_UNREGISTER_DEVICE:
	{
		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_DEVICE: inlen %d, outlen %d,"
									" sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		if ((inlen == outlen)) {

			PLANSCSI_UNREGISTER_NDASDEV	UnregDev = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UNREGISTER_DEVICE: entered\n"));

			status = LSBus_UnregisterDevice(fdoData, UnregDev->SlotNo);

			Irp->IoStatus.Information = 0;
		}
		else {
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_DEVICE: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		}
	}
	break;
	case IOCTL_LANSCSI_UNREGISTER_TARGET:
	{
		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_TARGET: inlen %d, outlen %d,"
									" sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		if ((inlen == outlen)) {

			PLANSCSI_UNREGISTER_TARGET	UnregTarget = buffer;

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UNREGISTER_TARGET: entered\n"));

			status = LSBus_UnregisterTarget(fdoData, UnregTarget->SlotNo, UnregTarget->TargetId);

			Irp->IoStatus.Information = 0;
		}
		else {
			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, (
									"UNREGISTER_TARGET: length mismatch!!!"
									" inlen %d, outlen %d, sizeof(LANSCSI_ADD_TARGET_DATA) %d\n",
									inlen, outlen, sizeof(LANSCSI_ADD_TARGET_DATA)));
		}
	}
	break;
	case IOCTL_LANSCSI_SETPDOINFO:
		{
	    PPDO_DEVICE_DATA	pdoData;
		PBUSENUM_SETPDOINFO	SetPdoInfo;
		KIRQL				oldIrql;
		PVOID				sectionHandle;
		BOOLEAN				acceptStatus;

        Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_SETPDOINFO called\n"));

        if (sizeof (BUSENUM_SETPDOINFO) != inlen)
			break;

		acceptStatus = TRUE;
		SetPdoInfo = (PBUSENUM_SETPDOINFO)buffer;
		pdoData = LookupPdoData(fdoData, SetPdoInfo->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
						("no pdo\n"));
			status = STATUS_UNSUCCESSFUL;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_LANSCSI_SETPDOINFO,
				SetPdoInfo->SlotNo);
			break;
		}

		//
		//	lock the code section of this function to acquire spinlock in raised IRQL.
		//
	    sectionHandle = MmLockPagableCodeSection(Bus_IoCtl);

		KeAcquireSpinLock(&pdoData->LanscsiAdapterPDO.LSDevDataSpinLock, &oldIrql);

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("!!!!!!!!!!!!!!!!!!  SETPDOINFO: PDO %p: %08lx %08lx %08lx %08lx\n",
										pdoData->Self, SetPdoInfo->AdapterStatus, SetPdoInfo->DeviceMode,
										SetPdoInfo->SupportedFeatures,
										SetPdoInfo->EnabledFeatures));


		//
		//	Set status values to the corresponding physical device object.
		//

		if(ADAPTERINFO_ISSTATUS(pdoData->LanscsiAdapterPDO.AdapterStatus, ADAPTERINFO_STATUS_STOPPED)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("SETPDOINFO: 'An event occured after 'Stopped' event\n"));

				if(ADAPTERINFO_ISSTATUSFLAG(SetPdoInfo->AdapterStatus, ADAPTERINFO_STATUSFLAG_RESETSTATUS)) {
					Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("SETPDOINFO: Reset-status event accepted.\n"));
				} else {
					acceptStatus = FALSE;
				}

		}

		if(acceptStatus) {
			pdoData->LanscsiAdapterPDO.AdapterStatus = SetPdoInfo->AdapterStatus;
			pdoData->LanscsiAdapterPDO.DeviceMode = SetPdoInfo->DeviceMode;
			pdoData->LanscsiAdapterPDO.SupportedFeatures = SetPdoInfo->SupportedFeatures;
			pdoData->LanscsiAdapterPDO.EnabledFeatures = SetPdoInfo->EnabledFeatures;
		}

		//
		//	Queue plugout worker if the NDAS SCSI stop abnormally.
		//
		if(ADAPTERINFO_ISSTATUS(SetPdoInfo->AdapterStatus, ADAPTERINFO_STATUS_STOPPED) &&
			ADAPTERINFO_ISSTATUSFLAG(SetPdoInfo->AdapterStatus, ADAPTERINFO_STATUSFLAG_ABNORMAL_TERMINAT)) {

			Bus_KdPrint(fdoData, BUS_DBG_IOCTL_ERROR, ("SETPDOINFO: Queueing Unplug worker!!!!!!!!\n"));

			status = QueueUnplugWorker(fdoData, SetPdoInfo->SlotNo);
		}

		KeReleaseSpinLock(&pdoData->LanscsiAdapterPDO.LSDevDataSpinLock, oldIrql);


		//
		//	Release the code section.
		//

	    MmUnlockPagableImageSection(sectionHandle);

		status = STATUS_SUCCESS;
		ObDereferenceObject(pdoData->Self);

        Irp->IoStatus.Information = outlen;
	}
		break;

	case IOCTL_BUSENUM_QUERY_NODE_ALIVE: 
		{

		PPDO_DEVICE_DATA		pdoData;
		BOOLEAN					bAlive;
		PBUSENUM_NODE_ALIVE_IN	pNodeAliveIn;
		BUSENUM_NODE_ALIVE_OUT	nodeAliveOut;

		// Check Parameter.
		if(inlen != sizeof(BUSENUM_NODE_ALIVE_IN) || 
			outlen != sizeof(BUSENUM_NODE_ALIVE_OUT)) {
			status = STATUS_UNKNOWN_REVISION;
			break;
		}
		
		pNodeAliveIn = (PBUSENUM_NODE_ALIVE_IN)Irp->AssociatedIrp.SystemBuffer;  
		
		Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_NOISE,
			("FDO: IOCTL_BUSENUM_QUERY_NODE_ALIVE SlotNumber = %d\n",
			pNodeAliveIn->SlotNo));
		
		pdoData = LookupPdoData(fdoData, pNodeAliveIn->SlotNo);

		if(pdoData == NULL) {
//			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_TRACE,
//				("[LanScsiBus]Bus_IoCtl: IOCTL_BUSENUM_QUERY_NODE_ALIVE No pdo\n"));
			
			bAlive = FALSE;
		} else {
			//
			// Check this PDO would be removed...
			//
			if(pdoData->Present == TRUE) 
				bAlive = TRUE;
			else
				bAlive = FALSE;
		}

		// For Result...
		nodeAliveOut.SlotNo = pNodeAliveIn->SlotNo;
		nodeAliveOut.bAlive = bAlive;
		// Get Adapter Status.
		if(bAlive == TRUE)
		{
			if(	ADAPTERINFO_ISSTATUS(pdoData->LanscsiAdapterPDO.AdapterStatus, ADAPTERINFO_STATUS_IN_ERROR) ||
				ADAPTERINFO_ISSTATUS(pdoData->LanscsiAdapterPDO.AdapterStatus, ADAPTERINFO_STATUS_STOPPING) /*||
				ADAPTERINFO_ISSTATUSFLAG(pdoData->LanscsiAdapterPDO.AdapterStatus, ADAPTERINFO_STATUSFLAG_MEMBER_FAULT) */
				) {

				nodeAliveOut.bHasError = TRUE;
				Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
					("IOCTL_BUSENUM_QUERY_NODE_ALIVE Adapter has Error 0x%x\n", nodeAliveOut.bHasError));
			} else {
				nodeAliveOut.bHasError = FALSE;
			}

		}

		if(pdoData)
			ObDereferenceObject(pdoData->Self);

		RtlCopyMemory(
			Irp->AssociatedIrp.SystemBuffer,
			&nodeAliveOut,
			sizeof(BUSENUM_NODE_ALIVE_OUT)
			);
		
		Irp->IoStatus.Information = sizeof(BUSENUM_NODE_ALIVE_OUT);
		status = STATUS_SUCCESS;
		}
		break;

	//
	//	added by hootch 01172004
	//
	case IOCTL_LANSCSI_UPGRADETOWRITE:
		{
		PPDO_DEVICE_DATA				pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_UPGRADETOWRITE called\n"));
		// Check Parameter.
		if(inlen != sizeof(BUSENUM_UPGRADE_TO_WRITE)) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_UPGRADETOWRITE: Invalid input buffer length\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		pdoData = LookupPdoData(fdoData, ((PBUSENUM_UPGRADE_TO_WRITE)buffer)->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_UPGRADETOWRITE: No pdo for Slotno:%d\n", ((PBUSENUM_UPGRADE_TO_WRITE)buffer)->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_LANSCSI_UPGRADETOWRITE,
				((PBUSENUM_UPGRADE_TO_WRITE)buffer)->SlotNo);
		} else {
			//
			//	redirect to the LanscsiMiniport Device
			//
			status = LSBus_IoctlToLSMPDevice(
					pdoData,
					LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE,
					buffer,
					inlen,
					buffer,
					outlen
				);

			ObDereferenceObject(pdoData->Self);
		}
		Irp->IoStatus.Information = 0;
	}
		break;

	case IOCTL_LANSCSI_REDIRECT_NDASSCSI:
		{
		PPDO_DEVICE_DATA				pdoData;
		PBUSENUM_REDIRECT_NDASSCSI		redirectIoctl;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_REDIRECT_NDASSCSI called\n"));
		// Check Parameter.
		if(inlen < sizeof(BUSENUM_REDIRECT_NDASSCSI)) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_REDIRECT_NDASSCSI: Invalid input buffer length\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		redirectIoctl = (PBUSENUM_REDIRECT_NDASSCSI)buffer;

		pdoData = LookupPdoData(fdoData, redirectIoctl->SlotNo);
		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_REDIRECT_NDASSCSI: No pdo for Slotno:%d\n", redirectIoctl->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
		} else {
			//
			//	redirect to the LanscsiMiniport Device
			//
			status = LSBus_IoctlToLSMPDevice(
					pdoData,
					redirectIoctl->IoctlCode,
					redirectIoctl->IoctlData,
					redirectIoctl->IoctlDataSize,
					redirectIoctl->IoctlData,
					redirectIoctl->IoctlDataSize
				);

			ObDereferenceObject(pdoData->Self);
		}
		Irp->IoStatus.Information = 0;
	}
		break;

	case IOCTL_LANSCSI_QUERY_LSMPINFORMATION:
		{
		PPDO_DEVICE_DATA				pdoData;

		Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_QUERY_LSMPINFORMATION called\n"));
		// Check Parameter.
		if(inlen < FIELD_OFFSET(LSMPIOCTL_QUERYINFO, QueryData) ) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_QUERY_LSMPINFORMATION: Invalid input buffer length too small.\n"));
			status = STATUS_UNKNOWN_REVISION;
			break;
		}
		pdoData = LookupPdoData(fdoData, ((PLSMPIOCTL_QUERYINFO)buffer)->SlotNo);

		if(pdoData == NULL) {
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("IOCTL_LANSCSI_QUERY_LSMPINFORMATION No pdo\n"));
			status = STATUS_NO_SUCH_DEVICE;
			NDBusIoctlLogError(	fdoData->Self,
				NDASBUS_IO_PDO_NOT_FOUND,
				IOCTL_LANSCSI_QUERY_LSMPINFORMATION,
				((PLSMPIOCTL_QUERYINFO)buffer)->SlotNo);
		} else {
			//
			//	redirect to the LanscsiMiniport Device
			//
			status = LSBus_IoctlToLSMPDevice(
					pdoData,
					LANSCSIMINIPORT_IOCTL_QUERYINFO_EX,
					buffer,
					inlen,
					buffer,
					outlen
				);

			ObDereferenceObject(pdoData->Self);
		}
        Irp->IoStatus.Information = outlen;
		}
		break;

	case IOCTL_BUSENUM_QUERY_INFORMATION:
		{

//		PPDO_DEVICE_DATA				pdoData;
		BUSENUM_QUERY_INFORMATION		Query;
		PBUSENUM_INFORMATION			Information;
		LONG							BufferLenNeeded;

		// Check Parameter.
		if(	inlen < sizeof(BUSENUM_QUERY_INFORMATION) /*|| 
			outlen < sizeof(BUSENUM_INFORMATION) */) {
			status = STATUS_UNKNOWN_REVISION;
			break;
		}

		RtlCopyMemory(&Query, buffer, sizeof(BUSENUM_QUERY_INFORMATION));
		Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_TRACE,
			("FDO: IOCTL_BUSENUM_QUERY_INFORMATION QueryType : %d  SlotNumber = %d\n",
			Query.InfoClass, Query.SlotNo));

		Information = (PBUSENUM_INFORMATION)buffer;
		ASSERT(Information);
		Information->InfoClass = Query.InfoClass;
		status = LSBus_QueryInformation(fdoData, IoIs32bitProcess(Irp), &Query, Information, outlen, &BufferLenNeeded);
		if(NT_SUCCESS(status)) {
			Information->Size = BufferLenNeeded;
			Irp->IoStatus.Information = BufferLenNeeded;
		} else {
			Irp->IoStatus.Information = BufferLenNeeded;
		}
		}
		break;

	case IOCTL_BUSENUM_PLUGIN_HARDWARE_EX2:
		{
			ULONG	structLen;		// Without variable length field
			ULONG	wholeStructLen; // With variable length field
			ULONG	inputWholeStructLen;

			//
			// Check 32 bit thunking request
            //
			if(IoIs32bitProcess(Irp)) {
				structLen = FIELD_OFFSET(BUSENUM_PLUGIN_HARDWARE_EX2_32, HardwareIDs);
				wholeStructLen = sizeof(BUSENUM_PLUGIN_HARDWARE_EX2_32);
				inputWholeStructLen = ((PBUSENUM_PLUGIN_HARDWARE_EX2_32) buffer)->Size;
			} else {
				structLen = FIELD_OFFSET(BUSENUM_PLUGIN_HARDWARE_EX2, HardwareIDs);
				wholeStructLen = sizeof(BUSENUM_PLUGIN_HARDWARE_EX2);
				inputWholeStructLen = ((PBUSENUM_PLUGIN_HARDWARE_EX2) buffer)->Size;
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

				status= Bus_PlugInDeviceEx2((PBUSENUM_PLUGIN_HARDWARE_EX2)buffer,
											inlen,
											fdoData,
											IoIs32bitProcess(Irp),
											Irp->RequestorMode, FALSE);

				Irp->IoStatus.Information = outlen;

			}
		}
        break;

	case IOCTL_LANSCSI_GETVERSION:
		{
			if (outlen >= sizeof(BUSENUM_GET_VERSION)) {
				PBUSENUM_GET_VERSION version = (PBUSENUM_GET_VERSION)buffer;

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("IOCTL_LANSCSI_GETVERSION: called\n"));

			try {
				version->VersionMajor = VER_FILEMAJORVERSION;
				version->VersionMinor = VER_FILEMINORVERSION;
				version->VersionBuild = VER_FILEBUILD;
				version->VersionPrivate = VER_FILEBUILD_QFE;

					Irp->IoStatus.Information = sizeof(BUSENUM_GET_VERSION);
					status = STATUS_SUCCESS;

				} except (EXCEPTION_EXECUTE_HANDLER) {

					status = GetExceptionCode();
					Irp->IoStatus.Information = 0;
				}

			}
		}			
		break;

    case IOCTL_BUSENUM_UNPLUG_HARDWARE:
		{
			if ((sizeof (BUSENUM_UNPLUG_HARDWARE) == inlen) &&
				(inlen == outlen) &&
				(((PBUSENUM_UNPLUG_HARDWARE)buffer)->Size == inlen)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UnPlug called\n"));

				status= Bus_UnPlugDevice(
						(PBUSENUM_UNPLUG_HARDWARE)buffer, fdoData);
				Irp->IoStatus.Information = outlen;

			}
		}
        break;

    case IOCTL_BUSENUM_EJECT_HARDWARE:
		{
			if ((sizeof (BUSENUM_EJECT_HARDWARE) == inlen) &&
				(inlen == outlen) &&
				(((PBUSENUM_EJECT_HARDWARE)buffer)->Size == inlen)) {

				Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("Eject called\n"));

				status= Bus_EjectDevice((PBUSENUM_EJECT_HARDWARE)buffer, fdoData);

				Irp->IoStatus.Information = outlen;
			}
		}
		break;

	case IOCTL_DVD_GET_STATUS:
		{
			PPDO_DEVICE_DATA		pdoData;
			PBUSENUM_DVD_STATUS		pDvdStatusData;


			// Check Parameter.
			if((inlen != outlen)
				|| (sizeof(BUSENUM_DVD_STATUS) >  inlen))
			{
				status = STATUS_UNSUCCESSFUL ;
				break;
			}
			
			pDvdStatusData = (PBUSENUM_DVD_STATUS)Irp->AssociatedIrp.SystemBuffer;  
			
			Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
				("FDO: IOCTL_DVD_GET_STATUS SlotNumber = %d\n",
				pDvdStatusData->SlotNo));	

			pdoData = LookupPdoData(fdoData, pDvdStatusData->SlotNo);
			
			if(pdoData == NULL) {
				Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
					("IOCTL_DVD_GET_STATUS No pdo\n"));
				status = STATUS_UNSUCCESSFUL;
				NDBusIoctlLogError(	fdoData->Self,
					NDASBUS_IO_PDO_NOT_FOUND,
					IOCTL_DVD_GET_STATUS,
					pDvdStatusData->SlotNo);
				break;	
			} else {

				if(pdoData->LanscsiAdapterPDO.Flags & LSDEVDATA_FLAG_LURDESC) {
					//
					//	A LUR descriptor is set.
					//
					if(((PLURELATION_DESC)pdoData->LanscsiAdapterPDO.AddDevInfo)->DevType != NDASSCSI_TYPE_DVD)
				{
					Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
						("IOCTL_DVD_GET_STATUS  No DVD Device\n"));
					status = STATUS_UNSUCCESSFUL;
					break;
				}
				} else {
					//
					//	ADD_TARGET_DATA is set.
					//
					if(((PLANSCSI_ADD_TARGET_DATA)pdoData->LanscsiAdapterPDO.AddDevInfo)->ucTargetType != NDASSCSI_TYPE_DVD)
					{
						Bus_KdPrint_Cont (fdoData, BUS_DBG_IOCTL_ERROR,
							("IOCTL_DVD_GET_STATUS  No DVD Device\n"));
						status = STATUS_UNSUCCESSFUL;
						break;
					}
				}
				//
				//	redirect to the LanscsiMiniport Device
				//
				status = LSBus_IoctlToLSMPDevice(
						pdoData,
						LANSCSIMINIPORT_IOCTL_GET_DVD_STATUS,
						buffer,
						inlen,
						buffer,
						outlen
					);

				ObDereferenceObject(pdoData->Self);
				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = outlen;
			}					
		}
		break;
    default:
        break; // default status is STATUS_INVALID_PARAMETER
    }

	Irp->IoStatus.Status = status;
	if(Irp->UserIosb)
		*Irp->UserIosb = Irp->IoStatus;

	IoCompleteRequest (Irp, IO_NO_INCREMENT);
    Bus_DecIoCount (fdoData);
    return status;
}


VOID
Bus_DriverUnload (
    IN PDRIVER_OBJECT DriverObject
    )
/*++
Routine Description:
    Clean up everything we did in driver entry.

Arguments:

   DriverObject - pointer to this driverObject.


Return Value:

--*/
{
    PAGED_CODE ();

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Unload\n"));
    
    //
    // All the device objects should be gone.
    //

    ASSERT (NULL == DriverObject->DeviceObject);
	UNREFERENCED_PARAMETER(DriverObject);

    //
    // Here we free all the resources allocated in the DriverEntry
    //

    if(Globals.RegistryPath.Buffer)
        ExFreePool(Globals.RegistryPath.Buffer);   
        
    return;
}

VOID
Bus_IncIoCount (
    IN  PFDO_DEVICE_DATA   FdoData
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
    IN  PFDO_DEVICE_DATA  FdoData
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


