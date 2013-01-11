/*++

Copyright (C) 2007 XIMETA, Inc. All rights reserved.

Abstract:

    This module contains routines to handle the function driver
    aspect of the bus driver.

--*/

#include "port.h"
#include <initguid.h>
#include <ndas/ndasportguid.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
/*++
Routine Description:

    Initialize the call backs structure of Driver Framework.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

  NT Status Code

--*/
{
    WDF_DRIVER_CONFIG   config;
    NTSTATUS            status;
    WDFDRIVER driver;

    KdPrint(("NDASPORTFX DriverEntry - \n"));
    KdPrint(("Built %s %s\n", __DATE__, __TIME__));

    //
    // Initialize driver config to control the attributes that
    // are global to the driver. Note that framework by default
    // provides a driver unload routine. If you create any resources
    // in the DriverEntry and want to be cleaned in driver unload,
    // you can override that by specifying one in the Config structure.
    //

    WDF_DRIVER_CONFIG_INIT(
        &config,
        NdasPortEvtDeviceAdd);

    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(
		DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        &driver);

    if (!NT_SUCCESS(status)) 
	{
        KdPrint( ("WdfDriverCreate failed with status 0x%x\n", status));
    }

    return status;
}

