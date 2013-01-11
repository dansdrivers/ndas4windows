/*++

Copyright (C) 2007 XIMETA, Inc. All rights reserved.

Abstract:

    This module contains routines to handle the function driver
    aspect of the bus driver.

--*/

#include "port.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, NdasPortEvtDeviceAdd)
#pragma alloc_text (PAGE, NdasPortEvtIoDeviceControl)
#pragma alloc_text (PAGE, NdasPortPlugInDevice)
#pragma alloc_text (PAGE, NdasPortUnPlugDevice)
#pragma alloc_text (PAGE, NdasPortEjectDevice)
#endif

NTSTATUS
NdasPortEvtDeviceAdd(
    __in WDFDRIVER        Driver,
    __in PWDFDEVICE_INIT  DeviceInit)
/*++
Routine Description:

    Bus_EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of toaster bus.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    WDF_CHILD_LIST_CONFIG      config;
    WDF_OBJECT_ATTRIBUTES      fdoAttributes;
    NTSTATUS                   status;
    WDFDEVICE                  device;
    WDF_IO_QUEUE_CONFIG        queueConfig;
    PNP_BUS_INFORMATION        busInfo;
    PNDASPORT_FDO_EXTENSION           deviceData;
    WDFQUEUE                   queue;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE ();

    KdPrint(("Bus_EvtDeviceAdd: 0x%p\n", Driver));

    //
    // Initialize all the properties specific to the device.
    // Framework has default values for the one that are not
    // set explicitly here. So please read the doc and make sure
    // you are okay with the defaults.
    //
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);
    WdfDeviceInitSetExclusive(DeviceInit, TRUE);

    //
    // Since this is pure software bus enumerator, we don't have to register for
    // any PNP/Power callbacks. Framework will take the default action for
    // all the PNP and Power IRPs.
    //


    //
    // WDF_ DEVICE_LIST_CONFIG describes how the framework should handle
    // dynamic child enumeration on behalf of the driver writer.
    // Since we are a bus driver, we need to specify identification description
    // for our child devices. This description will serve as the identity of our
    // child device. Since the description is opaque to the framework, we
    // have to provide bunch of callbacks to compare, copy, or free
    // any other resources associated with the description.
    //
    WDF_CHILD_LIST_CONFIG_INIT(
		&config,
        sizeof(PDO_IDENTIFICATION_DESCRIPTION),
        NdasPortEvtDeviceListCreatePdo // callback to create a child device.
        );
    //
    // This function pointer will be called when the framework needs to copy a
    // identification description from one location to another.  An implementation
    // of this function is only necessary if the description contains description
    // relative pointer values (like  LIST_ENTRY for instance) .
    // If set to NULL, the framework will use RtlCopyMemory to copy an identification .
    // description. In this sample, it's not required to provide these callbacks.
    // they are added just for illustration.
    //
    config.EvtChildListIdentificationDescriptionDuplicate =
		NdasPortEvtChildListIdentificationDescriptionDuplicate;

    //
    // This function pointer will be called when the framework needs to compare
    // two identificaiton descriptions.  If left NULL a call to RtlCompareMemory
    // will be used to compare two identificaiton descriptions.
    //
    config.EvtChildListIdentificationDescriptionCompare =
		NdasPortEvtChildListIdentificationDescriptionCompare;
    //
    // This function pointer will be called when the framework needs to free a
    // identification description.  An implementation of this function is only
    // necessary if the description contains dynamically allocated memory
    // (by the driver writer) that needs to be freed. The actual identification
    // description pointer itself will be freed by the framework.
    //
    config.EvtChildListIdentificationDescriptionCleanup =
		NdasPortEvtChildListIdentificationDescriptionCleanup;

    //
    // Tell the framework to use the built-in childlist to track the state
    // of the device based on the configuration we just created.
    //
    WdfFdoInitSetDefaultChildListConfig(
		DeviceInit,
        &config,
        WDF_NO_OBJECT_ATTRIBUTES);

    //
    // Initialize attributes structure to specify size and accessor function
    // for storing device context.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
		&fdoAttributes, 
		NDASPORT_FDO_EXTENSION);

    //
    // Create a framework device object. In response to this call, framework
    // creates a WDM deviceobject and attach to the PDO.
    //
    status = WdfDeviceCreate(&DeviceInit, &fdoAttributes, &device);

    if (!NT_SUCCESS(status)) 
	{
        KdPrint(("Error creating device 0x%x\n", status));
        return status;
    }

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel);

    queueConfig.EvtIoDeviceControl = NdasPortEvtIoDeviceControl;

    status = WdfIoQueueCreate(
		device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue );

    if (!NT_SUCCESS(status)) 
	{
        KdPrint(("WdfIoQueueCreate failed status 0x%x\n", status));
        return status;
    }

    //
    // Get the device context.
    //
    deviceData = NdasPortFdoGetExtension(device);

    //
    // Create device interface for this device. The interface will be
    // enabled by the framework when we return from StartDevice successfully.
    // Clients of this driver will open this interface and send ioctls.
    //
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_NDASPORT,
        NULL // No Reference String. If you provide one it will appended to the
        );   // symbolic link. Some drivers register multiple interfaces for the same device
             // and use the reference string to distinguish between them

	if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    //
    // This value is used in responding to the IRP_MN_QUERY_BUS_INFORMATION
    // for the child devices. This is an optional information provided to
    // uniquely identify the bus the device is connected.
    //
    busInfo.BusTypeGuid = GUID_BUS_TYPE_NDASPORT;
    busInfo.LegacyBusType = PNPBus;
    busInfo.BusNumber = 0;

    WdfDeviceSetBusInformationForChildren(device, &busInfo);

    status = NdasPortWmiRegistration(device);
    if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    //
    // Check the registry to see if we need to enumerate child devices during
    // start.
    //
    status = NdasPortDoStaticEnumeration(device);

    return status;
}


VOID
NdasPortEvtIoDeviceControl(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request,
    IN size_t       OutputBufferLength,
    IN size_t       InputBufferLength,
    IN ULONG        IoControlCode
    )

/*++
Routine Description:

  Handle user mode PlugIn, UnPlug and device Eject requests.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.

    Request - Handle to a framework request object. This one represents
              the IRP_MJ_DEVICE_CONTROL IRP received by the framework.

    OutputBufferLength - Length, in bytes, of the request's output buffer,
                        if an output buffer is available.

    InputBufferLength - Length, in bytes, of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - Driver-defined or system-defined I/O control code (IOCTL)
                    that is associated with the request.

Return Value:

   VOID

--*/
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    WDFDEVICE hDevice;
    size_t length = 0;

    UNREFERENCED_PARAMETER(OutputBufferLength);

    PAGED_CODE ();

    hDevice = WdfIoQueueGetDevice(Queue);

    KdPrint(("Bus_EvtIoDeviceControl: 0x%p\n", hDevice));

    switch (IoControlCode) 
	{
	case IOCTL_STORAGE_QUERY_PROPERTY:
	case IOCTL_SCSI_GET_CAPABILITIES: 

	case IOCTL_SCSI_PASS_THROUGH:
	case IOCTL_SCSI_PASS_THROUGH_DIRECT: 

	case IOCTL_SCSI_GET_DUMP_POINTERS: 

	case IOCTL_SCSI_RESCAN_BUS:
	case IOCTL_SCSI_GET_INQUIRY_DATA: 

		WdfRequestComplete(Request, STATUS_NOT_SUPPORTED);

	case IOCTL_SCSI_MINIPORT:

		break;

	case IOCTL_NDASPORT_PLUGIN_LOGICALUNIT:
		{
			PNDAS_LOGICALUNIT_DESCRIPTOR descriptor;

			status = WdfRequestRetrieveInputBuffer(
				Request,
				sizeof(NDAS_LOGICALUNIT_DESCRIPTOR),
				&descriptor,
				NULL);

			if (!NT_SUCCESS(status))
			{
				break;
			}

			if (InputBufferLength < descriptor->Size)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != descriptor->Version)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			status = NdasPortPlugInDevice(hDevice, descriptor, 0);
		}
		break;
	case IOCTL_NDASPORT_EJECT_LOGICALUNIT:
		{
			PNDASPORT_LOGICALUNIT_EJECT ejectParam;

			if (OutputBufferLength < sizeof(NDASPORT_LOGICALUNIT_EJECT))
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			status = WdfRequestRetrieveInputBuffer(
				Request,
				sizeof(NDASPORT_LOGICALUNIT_EJECT),
				&ejectParam,
				NULL);

			if (!NT_SUCCESS(status))
			{
				break;
			}

			if (sizeof(NDASPORT_LOGICALUNIT_EJECT) != ejectParam->Size)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			status = NdasPortEjectDevice(hDevice, ejectParam);
		}
		break;
	case IOCTL_NDASPORT_UNPLUG_LOGICALUNIT:
		break;
    default:
        break; // default status is STATUS_INVALID_PARAMETER
    }

    WdfRequestCompleteWithInformation(Request, status, length);
}

NTSTATUS
NdasPortPlugInDevice(
    __in WDFDEVICE Device,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG InternalFlags)
/*++

Routine Description:

    The user application has told us that a new device on the bus has arrived.

    We therefore create a description structure in stack, fill in information about
    the child device and call WdfChildListAddOrUpdateChildDescriptionAsPresent
    to add the device.

--*/
{
    PDO_IDENTIFICATION_DESCRIPTION description;
    NTSTATUS         status;

    PAGED_CODE ();

    //
    // Initialize the description with the information about the newly
    // plugged in device.
    //
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
        &description.Header,
        sizeof(description));

	description.LogicalUnitDescriptor = LogicalUnitDescriptor; 

    //
    // Call the framework to add this child to the childlist. This call
    // will internally call our DescriptionCompare callback to check
    // whether this device is a new device or existing device. If
    // it's a new device, the framework will call DescriptionDuplicate to create
    // a copy of this description in non-paged pool.
    // The actual creation of the child device will happen when the framework
    // receives QUERY_DEVICE_RELATION request from the PNP manager in
    // response to InvalidateDeviceRelations call made as part of adding
    // a new child.
    //
    status = WdfChildListAddOrUpdateChildDescriptionAsPresent(
		WdfFdoGetDefaultChildList(Device), 
		&description.Header,
        NULL); // AddressDescription

    if (status == STATUS_OBJECT_NAME_EXISTS) 
	{
        //
        // The description is already present in the list, the serial number is
        // not unique, return error.
        //
        status = STATUS_INVALID_PARAMETER;
    }

    return status;
}

NTSTATUS
NdasPortUnPlugDevice(
    __in WDFDEVICE Device,
	__in PNDASPORT_LOGICALUNIT_UNPLUG UnplugParam)
/*++

Routine Description:

    The application has told us a device has departed from the bus.

    We therefore need to flag the PDO as no longer present.

Arguments:


Returns:

    STATUS_SUCCESS upon successful removal from the list
    STATUS_INVALID_PARAMETER if the removal was unsuccessful

--*/

{
    NTSTATUS       status;
    WDFCHILDLIST   list;

    PAGED_CODE ();

    list = WdfFdoGetDefaultChildList(Device);

    if (-1 == UnplugParam->LogicalUnitAddress.Address) 
	{
        //
        // Unplug everybody.  We do this by starting a scan and then not reporting
        // any children upon its completion
        //
        status = STATUS_SUCCESS;

        WdfChildListBeginScan(list);
        //
        // A call to WdfChildListBeginScan indicates to the framework that the
        // driver is about to scan for dynamic children. After this call has
        // returned, all previously reported children associated with this will be
        // marked as potentially missing.  A call to either
        // WdfChildListUpdateChildDescriptionAsPresent  or
        // WdfChildListMarkAllChildDescriptionsPresent will mark all previuosly
        // reported missing children as present.  If any children currently
        // present are not reported present by calling
        // WdfChildListUpdateChildDescriptionAsPresent at the time of
        // WdfChildListEndScan, they will be reported as missing to the PnP subsystem
        // After WdfChildListEndScan call has returned, the framework will
        // invalidate the device relations for the FDO associated with the list
        // and report the changes
        //
        WdfChildListEndScan(list);

    }
    else 
	{
        PDO_IDENTIFICATION_DESCRIPTION description;

        WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
            &description.Header,
            sizeof(description));

		description.LogicalUnitAddress = UnplugParam->LogicalUnitAddress;
        description.LogicalUnitDescriptor = NULL;

        //
        // WdfFdoUpdateChildDescriptionAsMissing indicates to the framework that a
        // child device that was previously detected is no longer present on the bus.
        // This API can be called by itself or after a call to WdfChildListBeginScan.
        // After this call has returned, the framework will invalidate the device
        // relations for the FDO associated with the list and report the changes.
        //
        status = WdfChildListUpdateChildDescriptionAsMissing(
			list,
            &description.Header);

        if (status == STATUS_NO_SUCH_DEVICE) 
		{
            //
            // serial number didn't exist. Remap it to a status that user
            // application can understand when it gets translated to win32
            // error code.
            //
            status = STATUS_INVALID_PARAMETER;
        }
    }

    return status;
}

NTSTATUS
NdasPortEjectDevice(
    __in WDFDEVICE Device,
	__in PNDASPORT_LOGICALUNIT_EJECT EjectParam)

/*++

Routine Description:

    The user application has told us to eject the device from the bus.
    In a real situation the driver gets notified by an interrupt when the
    user presses the Eject button on the device.

Arguments:


Returns:

    STATUS_SUCCESS upon successful removal from the list
    STATUS_INVALID_PARAMETER if the ejection was unsuccessful

--*/

{
    WDFDEVICE        hChild;
    NTSTATUS         status = STATUS_INVALID_PARAMETER;
    WDFCHILDLIST     list;

    PAGED_CODE ();

    list = WdfFdoGetDefaultChildList(Device);

    //
    // 0xFFFFFFFF means to eject all child devices
    //
    if (-1 == EjectParam->LogicalUnitAddress.Address) 
	{
        WDF_CHILD_LIST_ITERATOR iterator;

        WDF_CHILD_LIST_ITERATOR_INIT(
			&iterator,
            WdfRetrievePresentChildren );

        WdfChildListBeginIteration(list, &iterator);

        for ( ; ; )
		{
            WDF_CHILD_RETRIEVE_INFO childInfo;
            PDO_IDENTIFICATION_DESCRIPTION description;
            BOOLEAN ret;

            //
            // Init the structures.
            //
            WDF_CHILD_RETRIEVE_INFO_INIT(&childInfo, &description.Header);

            WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
				&description.Header,
                sizeof(description));
            //
            // Get the device identification description
            //
            status = WdfChildListRetrieveNextDevice(
				list,
                &iterator,
                &hChild,
                &childInfo);

            if (!NT_SUCCESS(status) || status == STATUS_NO_MORE_ENTRIES) 
			{
                break;
            }

            ASSERT(childInfo.Status == WdfChildListRetrieveDeviceSuccess);

            //
            // Use that description to request an eject.
            //
            ret = WdfChildListRequestChildEject(list, &description.Header);
            if (!ret) 
			{
                WDFVERIFY(ret);
            }

        }

        WdfChildListEndIteration(list, &iterator);

        if (status == STATUS_NO_MORE_ENTRIES) 
		{
            status = STATUS_SUCCESS;
        }

    }
    else 
	{
        PDO_IDENTIFICATION_DESCRIPTION description;

        WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
            &description.Header,
            sizeof(description));

        description.LogicalUnitAddress = EjectParam->LogicalUnitAddress;

        if (WdfChildListRequestChildEject(list, &description.Header)) 
		{
            status = STATUS_SUCCESS;
        }
    }

    return status;
}

NTSTATUS
NdasPortDoStaticEnumeration(
    IN WDFDEVICE Device
    )
/*++
Routine Description:

    The routine enables you to statically enumerate child devices
    during start instead of running the enum.exe/notify.exe to
    enumerate toaster devices.

    In order to statically enumerate, user must specify the number
    of toasters in the Toaster Bus driver's device registry. The
    default value is zero.

    HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\Root\SYSTEM\0002\
                    Device Parameters
                        NumberOfToasters:REG_DWORD:2

    You can also configure this value in the Toaster Bus Inf file.

--*/

{
    WDFKEY      hKey = NULL;
    NTSTATUS    status;
    ULONG       value, i;
    DECLARE_CONST_UNICODE_STRING(valueName, L"NumberOfToasters");

#if 0
    //
    // If the registry value doesn't exist, we will use the
    // hardcoded default number.
    //
    value = DEF_STATICALLY_ENUMERATED_TOASTERS;

    //
    // Open the device registry and read the "NumberOfToasters" value.
    //
    status = WdfDeviceOpenRegistryKey(
		Device,
        PLUGPLAY_REGKEY_DEVICE,
        STANDARD_RIGHTS_ALL,
        NULL, // PWDF_OBJECT_ATTRIBUTES
        &hKey);

    if (NT_SUCCESS (status)) 
	{
        status = WdfRegistryQueryULong(hKey,
                                  &valueName,
                                  &value);

        if (NT_SUCCESS (status)) {
            //
            // Make sure it doesn't exceed the max. This is required to prevent
            // denial of service by enumerating large number of child devices.
            //
            value = min(value, MAX_STATICALLY_ENUMERATED_TOASTERS);
        }else {
            return STATUS_SUCCESS; // This is an optional property.
        }

        WdfRegistryClose(hKey);
    }

    KdPrint(("Enumerating %d toaster devices\n", value));

    for (i = 1; i<= value; i++) 
	{
        //
        // Value of i is used as serial number.
        //
        status = NdasPortPlugInDevice(
			Device,);
    }

    return status;
#else
	return STATUS_SUCCESS;
#endif
}


