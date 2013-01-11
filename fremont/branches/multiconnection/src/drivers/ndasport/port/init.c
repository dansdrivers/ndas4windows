/*++

Copyright (C) 2006 XIMETA, Inc.

This file contains the initialization code for SCSI port driver.

--*/
#include "port.h"
#include <initguid.h>
#include "ndasportguid.h"

PDRIVER_DISPATCH FdoMajorFunctionTable[IRP_MJ_MAXIMUM_FUNCTION + 1];
PDRIVER_DISPATCH PdoMajorFunctionTable[IRP_MJ_MAXIMUM_FUNCTION + 1];

BOOLEAN IsWindowsXPOrLater;

// just the placeholder for the pointer and the actual value does not matter
PVOID NdasPortDriverExtensionTag = NULL;
PDRIVER_OBJECT NdasPortDriverObject = NULL;

#ifdef RUN_WPP
#include "init.tmh"
#endif

EXTERN_C
NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NdasPortUnload)
#pragma alloc_text(PAGE, NdasPortInitializeDispatchTables)
#endif

/*+++

Routine Description:

	Entry point for the driver. 

Arguments:

	DriverObject - Pointer to the driver object created by the system.
	RegistryPath - Registry path for the driver

Return Value :

	STATUS_SUCCESS if initialization was successful
	Appropriate NTStatus code on failure 

--*/
NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath)
{
	NTSTATUS status;
	PNDASPORT_DRIVER_EXTENSION driverExtension = NULL;

	//
	// Initialize system routines for DbgPrintEx
	//

	INIT_DBGPRINTEX();

	KdbgPrintEx(DPFLTR_NDASPORT, DPFLTR_ERROR_LEVEL, 
		">>> NdasPort DriverEntry built at %s %s from %s using MSC %d \n",
		__DATE__, __TIME__, __BUILDMACHINE_STR__, _MSC_VER);

	ASSERT(NULL == NdasPortDriverObject);
	NdasPortDriverObject = DriverObject;
	NdasPortDriverExtensionTag = DriverEntry;

	status = IoAllocateDriverObjectExtension(
		DriverObject,
		NdasPortDriverExtensionTag,
		sizeof(NDASPORT_DRIVER_EXTENSION),
		(PVOID*)&driverExtension);

	if (NT_SUCCESS(status)) 
	{
		RtlZeroMemory(
			driverExtension, 
			sizeof(NDASPORT_DRIVER_EXTENSION));
		driverExtension->DriverObject = DriverObject;
		driverExtension->RegistryPath.Length = RegistryPath->Length;
		driverExtension->RegistryPath.MaximumLength = RegistryPath->MaximumLength;
		driverExtension->RegistryPath.Buffer = (PWSTR)
			ExAllocatePoolWithTag(
				PagedPool,
				RegistryPath->MaximumLength,
				NDASPORT_TAG_REGPATH);

		if (driverExtension->RegistryPath.Buffer == NULL) 
		{
			return STATUS_NO_MEMORY;
		}

		RtlCopyUnicodeString(
			&(driverExtension->RegistryPath),
			RegistryPath);

		//
		// Hard code bus type for now
		//
		// driverExtension->BusType = BusTypeScsi;
		driverExtension->BusType = BusTypeAta;

		//
		// Next NdasPort number
		//
		driverExtension->NextNdasPortNumber = 0;

		//
		// FDO List
		//
		KeInitializeSpinLock(&driverExtension->FdoListSpinLock);
		InitializeListHead(&driverExtension->FdoList);

		//
		// Initialize LPX Local Address List
		//
		KeInitializeSpinLock(&driverExtension->LpxLocalAddressListSpinLock);
		InitializeListHead(&driverExtension->LpxLocalAddressList);

		//
		// Register for driver-wide network notification
		//
		driverExtension->TdiNotificationHandle = NULL;

		status = NdasPortRegisterForNetworkNotification(
			&driverExtension->TdiNotificationHandle);

		if (!NT_SUCCESS(status))
		{
			ExFreePool(driverExtension->RegistryPath.Buffer);

			DebugPrint((1,
				"DriverEntry: NdasPortRegisterForNetworkNotification failed, status=%X\n",
				status));

			return status;
		}
	}
	else if (status == STATUS_OBJECT_NAME_COLLISION) 
	{
		//
		// Extension already exists. Get a pointer to it
		//
		driverExtension = (PNDASPORT_DRIVER_EXTENSION)
			IoGetDriverObjectExtension(
				DriverObject,
				NdasPortDriverExtensionTag);
		ASSERT(driverExtension != NULL);
	}
	else 
	{
		DebugPrint((1, 
			"DriverEntry: Could not allocate driver extension, status=%X\n", status));
		return status;
	}

	//
	// Update the driver object with the entry points
	//
	DriverObject->MajorFunction[IRP_MJ_SCSI] = NdasPortDispatch;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = NdasPortDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = NdasPortDispatch;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdasPortDispatch;
	DriverObject->MajorFunction[IRP_MJ_PNP] = NdasPortDispatch;
	DriverObject->MajorFunction[IRP_MJ_POWER] = NdasPortDispatch;
	DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = NdasPortDispatch;

	DriverObject->DriverUnload = NdasPortUnload;
	DriverObject->DriverExtension->AddDevice = NdasPortFdoAddDevice;
	
	//
	// Only Pdos make use of the StartIo routine
	//
	DriverObject->DriverStartIo = NdasPortPdoStartIo;

	//
	// FDO and PDO Dispatch Tables
	//
	NdasPortInitializeDispatchTables();

	IsWindowsXPOrLater = IoIsWdmVersionAvailable(0x01, 0x20);

#ifdef TARGETING_Win2K
	//
	// You need to include this macro only on Win2K.
	// But the following article says that we cannot use this!
	// http://www.osronline.com/article.cfm?article=376
	//
	// But we need to add a handler in System Control Dispatch routine
	//
	// WPP_SYSTEMCONTROL(DriverObject);

#endif 

#if defined(RUN_WPP) && !defined(WPP_TRACE_W2K_COMPATABILITY)
	//
	// This macro is required to initialize software tracing on XP and beyond
	// For XP and beyond use the DriverObject as the first argument.
	// 
	WPP_INIT_TRACING(DriverObject,RegistryPath);

#endif

	return STATUS_SUCCESS;
}

#if defined(RUN_WPP) && defined(WPP_TRACE_W2K_COMPATABILITY)
VOID 
NdasPortWppInitTracing2K(
	__in PDEVICE_OBJECT DeviceObject, 
	__in PUNICODE_STRING RegistryPath)
{
	//
	// This macro is required to initialize software tracing. 
	// For Win2K use the deviceobject as the first argument.
	// 
	// RegistryPath is not used in W2K
	//
	WPP_INIT_TRACING(DeviceObject,RegistryPath);
}
#endif

//
// Just a stub for dll export
//
#if !defined(RUN_WPP)
NTSTATUS
W2kTraceMessage(
	IN TRACEHANDLE  LoggerHandle, 
	IN ULONG TraceOptions, 
	IN LPGUID MessageGuid, 
	IN USHORT MessageNumber, ...)
{
	return STATUS_SUCCESS;
}
#endif

extern
VOID
NdasPortClearLpxLocalAddressList(
	PNDASPORT_DRIVER_EXTENSION DriverExtension);

VOID
NdasPortUnload(
    __in PDRIVER_OBJECT DriverObject)
{
	NTSTATUS status;
    PNDASPORT_DRIVER_EXTENSION driverExtension;
	
	driverExtension = (PNDASPORT_DRIVER_EXTENSION)
		IoGetDriverObjectExtension( 
			DriverObject,
			NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

	ASSERT(NULL != driverExtension->RegistryPath.Buffer);
    ExFreePool(driverExtension->RegistryPath.Buffer);
    driverExtension->RegistryPath.Buffer = NULL;
    driverExtension->RegistryPath.Length = 0;
    driverExtension->RegistryPath.MaximumLength = 0;

	ASSERT(NULL != driverExtension->TdiNotificationHandle);
	
	status = TdiDeregisterPnPHandlers(
			driverExtension->TdiNotificationHandle);

	ASSERT(NT_SUCCESS(status));

	NdasPortClearLpxLocalAddressList(driverExtension);

#if defined(RUN_WPP) && !defined(WPP_TRACE_W2K_COMPATABILITY)
	// 
	// Cleanup using DriverObject on XP and beyond.
	//
	WPP_CLEANUP(DriverObject);
#endif

	KdbgPrintEx(DPFLTR_NDASPORT, DPFLTR_ERROR_LEVEL, 
		"<<< NdasPort unloaded successfully.\n");

	return;
}

VOID
NdasPortInitializeDispatchTables()
{
	ULONG i;

    //
    // Initialize FDO dispatch table
    //
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) 
	{
        FdoMajorFunctionTable[i] = NdasPortDispatchUnsupported;
    }

    FdoMajorFunctionTable[IRP_MJ_DEVICE_CONTROL] = NdasPortFdoDispatchDeviceControl;
    FdoMajorFunctionTable[IRP_MJ_SCSI] = NdasPortFdoDispatchScsi;
    FdoMajorFunctionTable[IRP_MJ_PNP] = NdasPortFdoDispatchPnp;
    FdoMajorFunctionTable[IRP_MJ_POWER] = NdasPortDispatchPower;
    FdoMajorFunctionTable[IRP_MJ_CREATE] = 
    FdoMajorFunctionTable[IRP_MJ_CLOSE] = NdasPortFdoDispatchCreateClose;
    FdoMajorFunctionTable[IRP_MJ_SYSTEM_CONTROL] = NdasPortFdoDispatchSystemControl;

    //
    // Initialize PDO dispatch table
    //
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) 
	{
        PdoMajorFunctionTable[i] = NdasPortDispatchUnsupported;
    }

    PdoMajorFunctionTable[IRP_MJ_DEVICE_CONTROL] = NdasPortPdoDispatchDeviceControl;
    PdoMajorFunctionTable[IRP_MJ_SCSI] = NdasPortPdoDispatchScsi;
    PdoMajorFunctionTable[IRP_MJ_PNP] = NdasPortPdoDispatchPnp;
    PdoMajorFunctionTable[IRP_MJ_POWER] = NdasPortDispatchPower;
    PdoMajorFunctionTable[IRP_MJ_CREATE] = 
    PdoMajorFunctionTable[IRP_MJ_CLOSE] = NdasPortPdoDispatchCreateClose;
    PdoMajorFunctionTable[IRP_MJ_SYSTEM_CONTROL] = NdasPortPdoDispatchSystemControl;

    return;
}

NTSTATUS
NdasPortDispatch(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
    PNDASPORT_COMMON_EXTENSION commonExtension;
    PIO_STACK_LOCATION irpStack;

	commonExtension = (PNDASPORT_COMMON_EXTENSION) DeviceObject->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);

    return (commonExtension->MajorFunction[irpStack->MajorFunction])(
		DeviceObject, Irp);
}


NTSTATUS
NdasPortDispatchUnsupported(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}
