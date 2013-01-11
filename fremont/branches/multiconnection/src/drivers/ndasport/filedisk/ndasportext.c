#include <ntddk.h>
#include <wdmsec.h>
#include <ndasport.h>

#define STR2(x) # x
#define STR(x) STR2(x)
#define __BUILDMACHINE_STRING__ STR(__BUILDMACHINE__)

//
// WDK replaces RtlUnicodeStringCatUnicodeString with RtlUnicodeStringCat
//
#if !defined(NTDDI_VERSION)
#define RtlUnicodeStringCat RtlUnicodeStringCatUnicodeString
#endif

#include <initguid.h>
#include <devguid.h>
#include "filediskguid.h"
#include "ramdiskguid.h"

CONST GUID* 
HostingDeviceTypes[] = {
	&NDASPORT_FILEDISK_TYPE_GUID,
	&NDASPORT_RAMDISK_TYPE_GUID
};

CONST size_t HostDeviceCount = sizeof(HostingDeviceTypes) / sizeof(HostingDeviceTypes[0]);
#define HOST_DEVICE_TYPE_COUNT (sizeof(HostingDeviceTypes) / sizeof(HostingDeviceTypes[0]))

typedef struct _CONTROL_DEVICE_EXTENSION {
	WCHAR SymbolicLinkNameBuffers[HOST_DEVICE_TYPE_COUNT][64];
	UNICODE_STRING SymbolicLinkNames[HOST_DEVICE_TYPE_COUNT];
} CONTROL_DEVICE_EXTENSION, *PCONTROL_DEVICE_EXTENSION;

NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath);

VOID
NdasPortExtDriverUnload(
    __in PDRIVER_OBJECT DriverObject);

NTSTATUS
NdasPortExtCreateClose(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortExtInternalDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortExtBuildSymbolicLinkName(
	__inout PUNICODE_STRING SymbolicLinkName,
	__in CONST GUID* ExternalTypeGuid);

NTSTATUS
NdasPortExtRegisterExternalTypes(
	__in PCUNICODE_STRING DeviceName,
	__in CONST GUID* ExternalTypeGuid,
	__inout PUNICODE_STRING SymbolicLinkName);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NdasPortExtDriverUnload)
#pragma alloc_text(PAGE, NdasPortExtBuildSymbolicLinkName)
#pragma alloc_text(PAGE, NdasPortExtRegisterExternalTypes)
#endif

NTSTATUS
NdasPortExtBuildSymbolicLinkName(
	__inout PUNICODE_STRING SymbolicLinkName,
	__in CONST GUID* ExternalTypeGuid)
{
	NTSTATUS status;
	UNICODE_STRING guidString;

	status = RtlStringFromGUID(ExternalTypeGuid, &guidString);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = RtlUnicodeStringCopyString(
		SymbolicLinkName, L"\\Device\\NdasPort_");
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	
	status = RtlUnicodeStringCat(
		SymbolicLinkName, 
		&guidString);

	RtlFreeUnicodeString(&guidString);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortExtRegisterExternalTypes(
	__in PCUNICODE_STRING DeviceName,
	__in CONST GUID* ExternalTypeGuid,
	__inout PUNICODE_STRING SymbolicLinkName)
{
	NTSTATUS status;

	status = NdasPortExtBuildSymbolicLinkName(
		SymbolicLinkName, 
		ExternalTypeGuid);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = IoCreateSymbolicLink(
		SymbolicLinkName,
		(PUNICODE_STRING) DeviceName);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT DriverObject,
	__in PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    PCONTROL_DEVICE_EXTENSION deviceExtension;
	ULONG i;

	WCHAR controlDeviceUnicodeStringBuffer[64];
	UNICODE_STRING controlDeviceUnicodeString = {
		0, 
		sizeof(controlDeviceUnicodeStringBuffer), 
		controlDeviceUnicodeStringBuffer};


    KdPrint(("NdasPortExt DriverEntry build on %s %s at %s MSVC %d\n", 
        __DATE__, __TIME__, __BUILDMACHINE_STRING__, _MSC_VER));

	i = 0;

	do
	{
		status = RtlUnicodeStringPrintf(
			&controlDeviceUnicodeString,
			L"\\Device\\NdasPortExt%d", i++);

		if (!NT_SUCCESS(status))
		{
			return status;
		}

		status = IoCreateDeviceSecure(
			DriverObject,
			sizeof(CONTROL_DEVICE_EXTENSION),
			&controlDeviceUnicodeString,
			FILE_DEVICE_UNKNOWN,
			FILE_DEVICE_SECURE_OPEN,
			FALSE,
			&SDDL_DEVOBJ_KERNEL_ONLY,
			&GUID_DEVCLASS_UNKNOWN,
			&deviceObject);

	} while (STATUS_OBJECT_NAME_COLLISION == status);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("IoCreateDevice failed, status=%x\n", status));
        return status;
    }

    KdPrint(("Control device=%p, name=%wZ\n", deviceObject, &controlDeviceUnicodeString));

	deviceExtension = (PCONTROL_DEVICE_EXTENSION) deviceObject->DeviceExtension;

	RtlZeroMemory(
		deviceExtension,
		sizeof(CONTROL_DEVICE_EXTENSION));

	for (i = 0; i < HOST_DEVICE_TYPE_COUNT; ++i)
	{
		deviceExtension->SymbolicLinkNames[i].Buffer = deviceExtension->SymbolicLinkNameBuffers[i];
		deviceExtension->SymbolicLinkNames[i].MaximumLength = sizeof(deviceExtension->SymbolicLinkNameBuffers[i]);

		status = NdasPortExtRegisterExternalTypes(
			&controlDeviceUnicodeString,
			HostingDeviceTypes[i],
			&deviceExtension->SymbolicLinkNames[i]);

		if (!NT_SUCCESS(status))
		{
			ULONG j = i;
			for (j = 0; j + 1 < i; ++j)
			{
				IoDeleteSymbolicLink(&deviceExtension->SymbolicLinkNames[i]);
			}
			return status;
		}
	}

    DriverObject->DriverUnload = NdasPortExtDriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = 
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = NdasPortExtCreateClose;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = NdasPortExtInternalDeviceControl;

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    KdPrint(("NdasPortExt loaded successfully.\n"));
    
    return STATUS_SUCCESS;
}

VOID
NdasPortExtDriverUnload(
    __in PDRIVER_OBJECT DriverObject)
{
    NTSTATUS status;
    PDEVICE_OBJECT nextDeviceObject;
	ULONG i;

    //
    // Loop through each devices and delete them
    //
    nextDeviceObject = DriverObject->DeviceObject;
    while (NULL != nextDeviceObject)
    {
        PDEVICE_OBJECT deviceObject;
        PCONTROL_DEVICE_EXTENSION deviceExtension;

		deviceObject = nextDeviceObject;
		deviceExtension = (PCONTROL_DEVICE_EXTENSION) deviceObject->DeviceExtension;

		for (i = 0; i < HOST_DEVICE_TYPE_COUNT; ++i)
		{
			IoDeleteSymbolicLink(
				&deviceExtension->SymbolicLinkNames[i]);
		}
        
        nextDeviceObject = deviceObject->NextDevice;
        IoDeleteDevice(deviceObject);
    }

    KdPrint(("NdasPortExt unloaded successfully.\n"));
}

NTSTATUS
NdasPortExtCreateClose(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

extern
NTSTATUS
FileDiskGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__inout PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

extern
NTSTATUS
RamDiskGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__inout PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

NTSTATUS
NdasPortExtInternalDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
    NTSTATUS status;
    PIO_STACK_LOCATION irpStack;
    ULONG outputBufferLength;
    PNDASPORT_EXTERNAL_TYPE_GET_INTERFACE getInterface;
    
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    Irp->IoStatus.Information = 0;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
    case NDASPORTEXT_IOCTL_GET_LOGICALUNIT_INTERFACE:
     
        outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    
        if (outputBufferLength < sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE))
        {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            getInterface = (PNDASPORT_EXTERNAL_TYPE_GET_INTERFACE) Irp->AssociatedIrp.SystemBuffer;
            if (getInterface->Size != sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE))
            {
                status = STATUS_INVALID_PARAMETER_1;
            }
            else if (getInterface->Version != NDASPORT_EXTERNAL_TYPE_GET_INTERFACE_VERSION)
            {
                status = STATUS_NOT_SUPPORTED;
            }
            else if (RtlEqualMemory(&getInterface->ExternalTypeGuid, &NDASPORT_FILEDISK_TYPE_GUID, sizeof(GUID)))
            {
                status = STATUS_SUCCESS;
                getInterface->GetInterfaceFunction = FileDiskGetNdasLogicalUnitInterface;
                Irp->IoStatus.Information = sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE);
            }
            else if (RtlEqualMemory(&getInterface->ExternalTypeGuid, &NDASPORT_RAMDISK_TYPE_GUID, sizeof(GUID)))
			{
                status = STATUS_SUCCESS;
                getInterface->GetInterfaceFunction = RamDiskGetNdasLogicalUnitInterface;
                Irp->IoStatus.Information = sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE);
			}
            else
            {
                status = STATUS_NOT_SUPPORTED;
            }
        }
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }
  
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
