/*++

Copyright (C) 2007 XIMETA, Inc. All rights reserved.

Abstract:

    This module handles all the WMI Irps.

--*/

#include "port.h"
#include <ndas/ndasportguid.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NdasPortWmiRegistration)
#pragma alloc_text(PAGE,NdasPortEvtStdDataSetItem)
#pragma alloc_text(PAGE,NdasPortEvtStdDataSetInstance)
#pragma alloc_text(PAGE,NdasPortEvtStdDataQueryInstance)
#endif

NTSTATUS
NdasPortWmiRegistration(
    WDFDEVICE      Device
    )
/*++
Routine Description

    Registers with WMI as a data provider for this
    instance of the device

--*/
{
    WDF_WMI_PROVIDER_CONFIG providerConfig;
    WDF_WMI_INSTANCE_CONFIG instanceConfig;
    PNDASPORT_FDO_EXTENSION deviceData;
    NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(busRsrcName, NDASPORT_MOF_RESOURCENAME);

    PAGED_CODE();

    deviceData = NdasPortFdoGetExtension(Device);

    //
    // Register WMI classes.
    // First specify the resource name which contain the binary mof resource.
    //
    status = WdfDeviceAssignMofResourceName(Device, &busRsrcName);
    if (!NT_SUCCESS(status)) 
	{
        return status;
    }

    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &NDASPORT_STD_DATA_GUID);
    providerConfig.MinInstanceBufferSize = sizeof(NDASPORT_WMI_STD_DATA);

    //
    // You would want to create a WDFWMIPROVIDER handle separately if you are
    // going to dynamically create instances on the provider.  Since we are
    // statically creating one instance, there is no need to create the provider
    // handle.
    //

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);

    //
    // By setting Register to TRUE, we tell the framework to create a provider
    // as part of the Instance creation call. This eliminates the need to
    // call WdfWmiProviderRegister.
    //
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = NdasPortEvtStdDataQueryInstance;
    instanceConfig.EvtWmiInstanceSetInstance = NdasPortEvtStdDataSetInstance;
    instanceConfig.EvtWmiInstanceSetItem = NdasPortEvtStdDataSetItem;

    status = WdfWmiInstanceCreate(
        Device,
        &instanceConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE);

    if (NT_SUCCESS(status)) 
	{
        deviceData->FdoWmiStdData.ErrorCount = 0;
    }

    return status;
}

//
// WMI System Call back functions
//
NTSTATUS
NdasPortEvtStdDataSetItem(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG DataItemId,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to set for the contents of
    an instance.

Arguments:

    WmiInstance is the instance being set

    DataItemId has the id of the data item being set

    InBufferSize has the size of the data item passed

    InBuffer has the new values for the data item

Return Value:

    status

--*/
{
    PNDASPORT_FDO_EXTENSION    fdoData;

    PAGED_CODE();

    fdoData = NdasPortFdoGetExtension(WdfWmiInstanceGetDevice(WmiInstance));

    //
    // TODO: Use generated header's #defines for constants and sizes
    // (for the remainder of the file)
    //

    if (DataItemId == 2) 
	{
        if (InBufferSize < sizeof(ULONG)) 
		{
            return STATUS_BUFFER_TOO_SMALL;
        }

        //BusEnumDebugLevel = fdoData->StdToasterBusData.DebugPrintLevel =
        //    *((PULONG)InBuffer);

        return STATUS_SUCCESS;
    }

    //
    // All other fields are read only
    //
    return STATUS_WMI_READ_ONLY;
}

NTSTATUS
NdasPortEvtStdDataSetInstance(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    )
/*++

Routine Description:

    This routine is a callback into the driver to set for the contents of
    an instance.

Arguments:

    WmiInstance is the instance being set

    BufferSize has the size of the data block passed

    Buffer has the new values for the data block

Return Value:

    status

--*/
{
    PNDASPORT_FDO_EXTENSION   fdoData;

    UNREFERENCED_PARAMETER(InBufferSize);

    PAGED_CODE();

    fdoData = NdasPortFdoGetExtension(WdfWmiInstanceGetDevice(WmiInstance));

    //
    // We will update only writable elements.
    //
    fdoData->FdoWmiStdData.DebugPrintLevel =
        ((PNDASPORT_WMI_STD_DATA)InBuffer)->DebugPrintLevel;

    return STATUS_SUCCESS;
}

NTSTATUS
NdasPortEvtStdDataQueryInstance(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG OutBufferSize,
    IN  PVOID OutBuffer,
    OUT PULONG BufferUsed
    )
/*++

Routine Description:

    This routine is a callback into the driver to set for the contents of
    a wmi instance

Arguments:

    WmiInstance is the instance being set

    OutBufferSize on has the maximum size available to write the data
        block.

    OutBuffer on return is filled with the returned data block

    BufferUsed pointer containing how many bytes are required (upon failure) or
        how many bytes were used (upon success)

Return Value:

    status

--*/
{
    PNDASPORT_FDO_EXTENSION fdoData;

    UNREFERENCED_PARAMETER(OutBufferSize);

    PAGED_CODE();

    fdoData = NdasPortFdoGetExtension(WdfWmiInstanceGetDevice(WmiInstance));

    *BufferUsed = sizeof (NDASPORT_WMI_STD_DATA);
    * (PNDASPORT_WMI_STD_DATA) OutBuffer = fdoData->FdoWmiStdData;

    return STATUS_SUCCESS;
}

