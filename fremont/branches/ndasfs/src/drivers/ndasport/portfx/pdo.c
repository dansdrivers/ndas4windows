/*++

Copyright (C) 2007 XIMETA, Inc. All rights reserved.

Abstract:

    This module handles plug & play calls for the child device (PDO).

--*/

#include "port.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NdasPortCreatePdo)
#pragma alloc_text(PAGE, NdasPortEvtDeviceListCreatePdo)
#endif

NTSTATUS
NdasPortEvtChildListIdentificationDescriptionDuplicate(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SourceIdentificationDescription,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER DestinationIdentificationDescription)
/*++

Routine Description:

    It is called when the framework needs to make a copy of a description.
    This happens when a request is made to create a new child device by
    calling WdfChildListAddOrUpdateChildDescriptionAsPresent.
    If this function is left unspecified, RtlCopyMemory will be used to copy the
    source description to destination. Memory for the description is managed by the
    framework.

    NOTE:   Callback is invoked with an internal lock held.  So do not call out
    to any WDF function which will require this lock
    (basically any other WDFCHILDLIST api)

Arguments:

    DeviceList - Handle to the default WDFCHILDLIST created by the framework.
    SourceIdentificationDescription - Description of the child being created -memory in
                            the calling thread stack.
    DestinationIdentificationDescription - Created by the framework in nonpaged pool.

Return Value:

    NT Status code.

--*/
{
    PPDO_IDENTIFICATION_DESCRIPTION src, dst;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(DeviceList);

    src = CONTAINING_RECORD(
		SourceIdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);
    
	dst = CONTAINING_RECORD(
		DestinationIdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);

    dst->LogicalUnitAddress = src->LogicalUnitAddress;

	if (NULL != src->LogicalUnitDescriptor)
	{
		dst->LogicalUnitDescriptor = (PNDAS_LOGICALUNIT_DESCRIPTOR) 
			ExAllocatePoolWithTag(
			PagedPool,
			src->LogicalUnitDescriptor->Size,
			NDASPORT_TAG_DEVICE_RELATIONS);

		if (NULL == dst->LogicalUnitDescriptor)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlCopyMemory(
			dst->LogicalUnitDescriptor,
			src->LogicalUnitDescriptor,
			src->LogicalUnitDescriptor->Size);
	}

    return STATUS_SUCCESS;
}

BOOLEAN
NdasPortEvtChildListIdentificationDescriptionCompare(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER FirstIdentificationDescription,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SecondIdentificationDescription)
/*++

Routine Description:

    It is called when the framework needs to compare one description with another.
    Typically this happens whenever a request is made to add a new child device.
    If this function is left unspecified, RtlCompareMemory will be used to compare the
    descriptions.

    NOTE:   Callback is invoked with an internal lock held.  So do not call out
    to any WDF function which will require this lock
    (basically any other WDFCHILDLIST api)

Arguments:

    DeviceList - Handle to the default WDFCHILDLIST created by the framework.

Return Value:

   TRUE or FALSE.

--*/
{
    PPDO_IDENTIFICATION_DESCRIPTION lhs, rhs;

    UNREFERENCED_PARAMETER(DeviceList);

    lhs = CONTAINING_RECORD(
		FirstIdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);

    rhs = CONTAINING_RECORD(
		SecondIdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);

    return (lhs->LogicalUnitAddress.Address == rhs->LogicalUnitAddress.Address) ? 
		TRUE : FALSE;
}

VOID
NdasPortEvtChildListIdentificationDescriptionCleanup(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription)
/*++

Routine Description:

    It is called to free up any memory resources allocated as part of the description.
    This happens when a child device is unplugged or ejected from the bus.
    Memory for the description itself will be freed by the framework.

Arguments:

    DeviceList - Handle to the default WDFCHILDLIST created by the framework.

    IdentificationDescription - Description of the child being deleted

Return Value:


--*/
{
    PPDO_IDENTIFICATION_DESCRIPTION description;

	UNREFERENCED_PARAMETER(DeviceList);

    description = CONTAINING_RECORD(
		IdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);

	if (NULL != description->LogicalUnitDescriptor)
	{
		ExFreePoolWithTag(
			description->LogicalUnitDescriptor,
			NDASPORT_TAG_DEVICE_RELATIONS);
		description->LogicalUnitDescriptor = NULL;
	}
}


NTSTATUS
NdasPortEvtDeviceListCreatePdo(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
    PWDFDEVICE_INIT ChildInit)
/*++

Routine Description:

    Called by the framework in response to Query-Device relation when
    a new PDO for a child device needs to be created.

Arguments:

    DeviceList - Handle to the default WDFCHILDLIST created by the framework as part
                        of FDO.

    IdentificationDescription - Description of the new child device.

    ChildInit - It's a opaque structure used in collecting device settings
                    and passed in as a parameter to CreateDevice.

Return Value:

    NT Status code.

--*/
{
    PPDO_IDENTIFICATION_DESCRIPTION pdoDescription;

    PAGED_CODE();

    pdoDescription = CONTAINING_RECORD(
		IdentificationDescription,
        PDO_IDENTIFICATION_DESCRIPTION,
        Header);

    return NdasPortCreatePdo(
		WdfChildListGetDevice(DeviceList),
        ChildInit,
		pdoDescription->LogicalUnitDescriptor);
}

NTSTATUS
NdasPortLuGetInquiryData(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out PINQUIRYDATA InquiryData)
{
	// InquiryData->
	return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
NdasPortCreatePdo(
	__in WDFDEVICE Device,
	__in PWDFDEVICE_INIT DeviceInit,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor)
/*++

Routine Description:

    This routine creates and initialize a PDO.

Arguments:

Return Value:

    NT Status code.

--*/
{
    NTSTATUS status;
    PNDASPORT_PDO_EXTENSION pdoData = NULL;
    WDFDEVICE hChild = NULL;
    WDF_QUERY_INTERFACE_CONFIG qiConfig;
    WDF_OBJECT_ATTRIBUTES pdoAttributes;
    WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
    WDF_DEVICE_POWER_CAPABILITIES powerCaps;

	WDF_IO_QUEUE_CONFIG ioQueueConfig;
	WDFQUEUE ioQueue;
	WDFQUEUE srbIoQueue;
	WDF_OBJECT_ATTRIBUTES ioQueueAttributes;

    DECLARE_CONST_UNICODE_STRING(compatId, L"gendisk\0");
    DECLARE_CONST_UNICODE_STRING(deviceLocation, L"NDAS Port Bus 0");
    DECLARE_UNICODE_STRING_SIZE(buffer, MAX_INSTANCE_ID_LEN);
    DECLARE_UNICODE_STRING_SIZE(deviceId, MAX_INSTANCE_ID_LEN);

	WCHAR hardwareIds[] = NDASPORT_ENUMERATOR L"\\ndaslu\0";

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Device);

    KdPrint(("Entered %s \n", __FUNCTION__));

    //
    // Set DeviceType
    //

	/* Depends on the logical unit type */
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_MASS_STORAGE);

    //
    // Provide DeviceID, HardwareIDs, CompatibleIDs and InstanceId
    //

	/* Depends on the logical unit type */
    RtlInitUnicodeString(&deviceId, hardwareIds);

    status = WdfPdoInitAssignDeviceID(DeviceInit, &deviceId);
    
	if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    //
    // NOTE: same string  is used to initialize hardware id too
    //

    status = WdfPdoInitAddHardwareID(DeviceInit, &deviceId);

	if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    status = WdfPdoInitAddCompatibleID(DeviceInit, &compatId);

	if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    status =  RtlUnicodeStringPrintf(
		&buffer, 
		L"%08Xd", 
		LogicalUnitDescriptor->Address);

    if (!NT_SUCCESS(status))
	{
        return status;
    }

    status = WdfPdoInitAssignInstanceID(DeviceInit, &buffer);

	if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    //
    // Provide a description about the device. This text is usually read from
    // the device. In the case of USB device, this text comes from the string
    // descriptor. This text is displayed momentarily by the PnP manager while
    // it's looking for a matching INF. If it finds one, it uses the Device
    // Description from the INF file or the friendly name created by
    // coinstallers to display in the device manager. FriendlyName takes
    // precedence over the DeviceDesc from the INF file.
    //
    status = RtlUnicodeStringPrintf(
		&buffer,
        L"Port %d, PathId %d, TargetId %d, LUN %d",
        LogicalUnitDescriptor->Address.PortNumber,
		LogicalUnitDescriptor->Address.PathId,
		LogicalUnitDescriptor->Address.TargetId,
		LogicalUnitDescriptor->Address.Lun);
    
	if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    //
    // You can call WdfPdoInitAddDeviceText multiple times, adding device
    // text for multiple locales. When the system displays the text, it
    // chooses the text that matches the current locale, if available.
    // Otherwise it will use the string for the default locale.
    // The driver can specify the driver's default locale by calling
    // WdfPdoInitSetDefaultLocale.
    //
    status = WdfPdoInitAddDeviceText(
		DeviceInit,
        &buffer,
        &deviceLocation,
        0x409);

    if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    WdfPdoInitSetDefaultLocale(DeviceInit, 0x409);

    //
    // Initialize the attributes to specify the size of PDO device extension.
    // All the state information private to the PDO will be tracked here.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
		&pdoAttributes, 
		NDASPORT_PDO_EXTENSION);
	
	pdoAttributes.EvtCleanupCallback = NdasPortPdoEvtCleanup;

    status = WdfDeviceCreate(&DeviceInit, &pdoAttributes, &hChild);

	if (!NT_SUCCESS(status)) 
	{
		KdPrint(("WdfDeviceCreate failed, status=%x\n", status));
        return status;
    }

    //
    // Get the device context.
    //
    pdoData = NdasPortPdoGetExtention(hChild);

    pdoData->LogicalUnitAddress = LogicalUnitDescriptor->Address;

    //
    // Set some properties for the child device.
    //
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.Removable         = WdfTrue;
    pnpCaps.EjectSupported    = WdfTrue;
    pnpCaps.SurpriseRemovalOK = WdfFalse;

    pnpCaps.Address  = LogicalUnitDescriptor->Address.Address;
    pnpCaps.UINumber = LogicalUnitDescriptor->Address.Address;

    WdfDeviceSetPnpCapabilities(hChild, &pnpCaps);

    WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCaps);

    powerCaps.DeviceState[PowerSystemWorking]   = PowerDeviceD1;
    powerCaps.DeviceState[PowerSystemSleeping1] = PowerDeviceD3;
    powerCaps.DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
    powerCaps.DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
    powerCaps.DeviceState[PowerSystemHibernate] = PowerDeviceD3;
    powerCaps.DeviceState[PowerSystemShutdown]  = PowerDeviceD3;

    WdfDeviceSetPowerCapabilities(hChild, &powerCaps);

	//
	// Create a device I/O queue
	//

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&ioQueueConfig, 
		WdfIoQueueDispatchParallel);

	// This ensures that SRB_FUNCTION_CLAIM is processed in PowerDeviceD3 state.
	ioQueueConfig.PowerManaged = WdfFalse;
	ioQueueConfig.EvtIoDeviceControl = NdasPortPdoEvtIoDeviceControl;
	ioQueueConfig.EvtIoInternalDeviceControl = NdasPortPdoEvtIoScsi;

	status = WdfIoQueueCreate(
		hChild,
		&ioQueueConfig,
		NULL,
		NULL);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("WdfIoQueueCreate failed, status=%x\n", status));
		return status;
	}

	WDF_IO_QUEUE_CONFIG_INIT(
		&ioQueueConfig,
		WdfIoQueueDispatchManual);

	ioQueueConfig.EvtIoInternalDeviceControl;

	//
	// Create a lockable device I/O queue (srb queue)
	//
	status = WdfIoQueueCreate(
		hChild,
		&ioQueueConfig,
		NULL,
		&srbIoQueue);
	
	if (!NT_SUCCESS(status))
	{
		KdPrint(("WdfIoQueueCreate (srbQueue) failed, status=%x\n", status));
		return status;
	}

    return status;
}

VOID
NdasPortPdoEvtCleanup(
	__in WDFDEVICE Device)
{
	KdPrint(("Entered %s\n", __FUNCTION__));
}
