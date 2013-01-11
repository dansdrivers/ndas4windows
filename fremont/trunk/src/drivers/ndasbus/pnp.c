/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    PNP.C

Abstract:

    This module handles plug & play calls for the toaster bus controller FDO.

Author:


Environment:

    kernel mode only

Notes:


Revision History:


--*/

#include "ndasbusproc.h"

#if DBG

LONG DbgLevelBusPnp = DBG_LEVEL_BUS_PNP;

#define NdasDbgCall(l,x,...) do {						\
	if (l <= DbgLevelBusPnp) {							\
	DbgPrint("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
	DbgPrint(x,__VA_ARGS__);							\
	} 													\
} while(0)

#else
#define NdasDbgCall(l,x,...);
#endif

#if __NDAS_SCSI_BUS__

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "BUSPNP"

NTSTATUS
Bus_PlugInDevice_PtrAlign(
    PNDASBUS_PLUGIN_HARDWARE_EX2	PlugIn,
    ULONG							PlugInSize,
    PFDO_DEVICE_DATA				FdoData,
	KPROCESSOR_MODE					RequestorMode,
	BOOLEAN							EnterFromRegistrar
    );

#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, Bus_AddDevice)
#pragma alloc_text (PAGE, Bus_PnP)
#pragma alloc_text (PAGE, Bus_PlugInDevice)
#pragma alloc_text (PAGE, Bus_PlugInDevice_PtrAlign)
#pragma alloc_text (PAGE, Bus_InitializePdo)
#pragma alloc_text (PAGE, Bus_UnPlugDevice)
#pragma alloc_text (PAGE, Bus_DestroyPdo)
#pragma alloc_text (PAGE, Bus_RemoveFdo)
#pragma alloc_text (PAGE, Bus_FDO_PnP)
#pragma alloc_text (PAGE, Bus_StartFdo)
#pragma alloc_text (PAGE, Bus_SendIrpSynchronously)
#pragma alloc_text (PAGE, Bus_EjectDevice)
#endif

NTSTATUS
Bus_AddDevice(
    __in PDRIVER_OBJECT DriverObject,
    __in PDEVICE_OBJECT PhysicalDeviceObject
    )
/*++
Routine Description.

    Our Toaster bus has been found.  Attach our FDO to it.
    Allocate any required resources.  Set things up.
    And be prepared for the ``start device''

Arguments:

    DriverObject - pointer to driver object.

    PhysicalDeviceObject  - Device object representing the bus to which we
                            will attach a new FDO.

--*/
{
    NTSTATUS            status;
    PDEVICE_OBJECT      deviceObject = NULL;
    PFDO_DEVICE_DATA    deviceData = NULL;
    PWCHAR              deviceName = NULL;
	ULONG				tempUlong;

#if DBG
    ULONG               nameLength;
#endif

    PAGED_CODE ();

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Add Device: 0x%p\n",
                                          PhysicalDeviceObject));

    status = IoCreateDevice (
                    DriverObject,               // our driver object
                    sizeof (FDO_DEVICE_DATA),   // device object extension size
                    NULL,                       // FDOs do not have names
                    FILE_DEVICE_BUS_EXTENDER,   // We are a bus
                    FILE_DEVICE_SECURE_OPEN,    //
                    TRUE,                       // our FDO is exclusive
                    &deviceObject);             // The device object created

    if (!NT_SUCCESS (status))
    {
        goto End;
    }

    deviceData = (PFDO_DEVICE_DATA) deviceObject->DeviceExtension;
    RtlZeroMemory (deviceData, sizeof (FDO_DEVICE_DATA));

    //
    // Set the initial state of the FDO
    //

    INITIALIZE_PNP_STATE(deviceData);

    deviceData->DebugLevel = BusEnumDebugLevel;

    deviceData->IsFDO = TRUE;

    deviceData->Self = deviceObject;

    ExInitializeFastMutex (&deviceData->Mutex);

	ExInitializeFastMutex (&deviceData->RegMutex);

    InitializeListHead (&deviceData->ListOfPDOs);

    // Set the PDO for use with PlugPlay functions

    deviceData->UnderlyingPDO = PhysicalDeviceObject;

#if __NDAS_SCSI_BUS__

	deviceData->PersistentPdo = Globals.PersistentPdo;
	deviceData->StartStopRegistrarEnum = Globals.PersistentPdo;
	deviceData->LfsFilterInstalled = Globals.LfsFilterInstalled;


	//
	//	Read options from the PDO's registry
	//

	// Disable persistent PDO option

	status = DrReadDevKeyValueInstantly(	PhysicalDeviceObject,
									BUSENUM_BUSPDOREG_DISABLE_PERSISTENTPDO,
									REG_DWORD,
									&tempUlong,
									sizeof(tempUlong),
									NULL);
	if(NT_SUCCESS(status) && tempUlong != 0) {
		deviceData->PersistentPdo = FALSE;
		Bus_KdPrint_Def (BUS_DBG_SS_INFO, 
			("Persistent PDO option disabled.\n"));
	}

#endif

    //
    // Set the initial powerstate of the FDO
    //

    deviceData->DevicePowerState = PowerDeviceUnspecified;
    deviceData->SystemPowerState = PowerSystemWorking;


    //
    // Biased to 1. Transition to zero during remove device
    // means IO is finished. Transition to 1 means the device
    // can be stopped.
    //

    deviceData->OutstandingIO = 1;

#if __NDAS_SCSI_BUS__

	//
	//	Set persistent PDO option.
	//

	deviceData->PersistentPdo = Globals.PersistentPdo;

#endif

    //
    // Initialize the remove event to Not-Signaled.  This event
    // will be set when the OutstandingIO will become 0.
    //

    KeInitializeEvent(&deviceData->RemoveEvent,
                  SynchronizationEvent,
                  FALSE);
    //
    // Initialize the stop event to Signaled:
    // there are no Irps that prevent the device from being
    // stopped. This event will be set when the OutstandingIO
    // will become 0.
    //

    KeInitializeEvent(&deviceData->StopEvent,
                      SynchronizationEvent,
                      TRUE);

    deviceObject->Flags |= DO_POWER_PAGABLE;

    //
    // Tell the Plug & Play system that this device will need a
    // device interface.
    //

    status = IoRegisterDeviceInterface (
                PhysicalDeviceObject,
                (LPGUID) &GUID_NDAS_BUS_ENUMERATOR_INTERFACE_CLASS,
                NULL,
                &deviceData->InterfaceName);

    if (!NT_SUCCESS (status)) {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
                      ("AddDevice: IoRegisterDeviceInterface failed (%x)", status));
        goto End;
    }

    //
    // Attach our FDO to the device stack.
    // The return value of IoAttachDeviceToDeviceStack is the top of the
    // attachment chain.  This is where all the IRPs should be routed.
    //

    deviceData->NextLowerDriver = IoAttachDeviceToDeviceStack (
                                    deviceObject,
                                    PhysicalDeviceObject);

    if (NULL == deviceData->NextLowerDriver) {

        status = STATUS_NO_SUCH_DEVICE;
        goto End;
    }


#if DBG
    //
    // We will demonstrate here the step to retrieve the name of the PDO
    //

    status = IoGetDeviceProperty (PhysicalDeviceObject,
                                  DevicePropertyPhysicalDeviceObjectName,
                                  0,
                                  NULL,
                                  &nameLength);

    if (status != STATUS_BUFFER_TOO_SMALL)
    {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
                      ("AddDevice:IoGDP failed (0x%x)\n", status));
        goto End;
    }

    deviceName = ExAllocatePoolWithTag (NonPagedPool,
                            nameLength, BUSENUM_POOL_TAG);

    if (NULL == deviceName) {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
        ("AddDevice: no memory to alloc for deviceName(0x%x)\n", nameLength));
        status =  STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    status = IoGetDeviceProperty (PhysicalDeviceObject,
                         DevicePropertyPhysicalDeviceObjectName,
                         nameLength,
                         deviceName,
                         &nameLength);

    if (!NT_SUCCESS (status)) {

        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
                      ("AddDevice:IoGDP(2) failed (0x%x)", status));
        goto End;
    }

    Bus_KdPrint (deviceData, BUS_DBG_SS_TRACE,
                  ("AddDevice: %p to %p->%p (%ws) \n",
                   deviceObject,
                   deviceData->NextLowerDriver,
                   PhysicalDeviceObject,
                   deviceName));

#endif

#if __NDAS_SCSI_BUS__
    
	// Register with WMI
    
    status = Bus_WmiRegistration(deviceData);
    if (!NT_SUCCESS (status)) {
        Bus_KdPrint (deviceData, BUS_DBG_SS_ERROR,
        ("Bus_WmiRegistration failed (%x)\n", status));
        goto End;
    }

#endif


End:
	//
	// We are done with initializing, so let's indicate that and return.
	// This should be the final step in the AddDevice process.
	//
	deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	if (deviceName) {

		ExFreePool(deviceName);
    }

    if (!NT_SUCCESS(status) && deviceObject) {

        if (deviceData && deviceData->NextLowerDriver){
        
			IoDetachDevice (deviceData->NextLowerDriver);
        }

		IoDeleteDevice (deviceObject);
    }

    return status;

}

NTSTATUS
Bus_PnP (
    __in PDEVICE_OBJECT   DeviceObject,
    __in PIRP             Irp
    )
/*++
Routine Description:
    Handles PnP Irps sent to both FDO and child PDOs.
Arguments:
    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.

Return Value:

    NT Status is returned.
--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    PCOMMON_DEVICE_DATA     commonData;

    PAGED_CODE ();

    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_PNP == irpStack->MajorFunction);

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // If the device has been removed, the driver should
    // not pass the IRP down to the next lower driver.
    //

    if (commonData->DevicePnPState == Deleted) {
        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE ;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

	NdasDbgCall( 2, "%s IRP:0x%p\n", PnPMinorFunctionString(irpStack->MinorFunction), Irp );

    if (commonData->IsFDO) {
        Bus_KdPrint (commonData, BUS_DBG_PNP_TRACE,
                      ("FDO %s IRP:0x%p\n",
                      PnPMinorFunctionString(irpStack->MinorFunction),
                      Irp));
        //
        // Request is for the bus FDO
        //
        status = Bus_FDO_PnP (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PFDO_DEVICE_DATA) commonData);
    } else {
        Bus_KdPrint (commonData, BUS_DBG_PNP_TRACE,
                      ("PDO %s IRP: 0x%p\n",
                      PnPMinorFunctionString(irpStack->MinorFunction),
                      Irp));
        //
        // Request is for the child PDO.
        //
        status = Bus_PDO_PnP (
                    DeviceObject,
                    Irp,
                    irpStack,
                    (PPDO_DEVICE_DATA) commonData);
    }

    return status;
}

NTSTATUS
Bus_FDO_PnP (
    __in PDEVICE_OBJECT       DeviceObject,
    __in PIRP                 Irp,
    __in PIO_STACK_LOCATION   IrpStack,
    __in PFDO_DEVICE_DATA     DeviceData
    )
/*++
Routine Description:

    Handle requests from the Plug & Play system for the BUS itself

--*/
{
    NTSTATUS            status;
    ULONG               length, prevcount, numPdosPresent;
    PLIST_ENTRY         entry, listHead, nextEntry;
    PPDO_DEVICE_DATA    pdoData;
    PDEVICE_RELATIONS   relations, oldRelations;

    PAGED_CODE ();

    Bus_IncIoCount (DeviceData);

    switch (IrpStack->MinorFunction) {

    case IRP_MN_START_DEVICE:

		NdasDbgCall( 2, "IRP_MN_START_DEVICE\n" );

        //
        // Send the Irp down and wait for it to come back.
        // Do not touch the hardware until then.
        //
        status = Bus_SendIrpSynchronously (DeviceData->NextLowerDriver, Irp);

        if (NT_SUCCESS(status)) {

            //
            // Initialize your device with the resources provided
            // by the PnP manager to your device.
            //
            status = Bus_StartFdo (DeviceData, Irp);
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completion routine with MORE_PROCESSING_REQUIRED.
        //

        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        Bus_DecIoCount (DeviceData);

        return status;

    case IRP_MN_QUERY_STOP_DEVICE:

		NdasDbgCall( 2, "IRP_MN_QUERY_STOP_DEVICE\n" );

        //
        // The PnP manager is trying to stop the device
        // for resource rebalancing. Fail this now if you
        // cannot stop the device in response to STOP_DEVICE.
        //
        SET_NEW_PNP_STATE(DeviceData, StopPending);
        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;

    case IRP_MN_CANCEL_STOP_DEVICE:

		NdasDbgCall( 2, "IRP_MN_CANCEL_STOP_DEVICE\n" );

        //
        // The PnP Manager sends this IRP, at some point after an
        // IRP_MN_QUERY_STOP_DEVICE, to inform the drivers for a
        // device that the device will not be stopped for
        // resource reconfiguration.
        //
        //
        // First check to see whether you have received cancel-stop
        // without first receiving a query-stop. This could happen if
        //  someone above us fails a query-stop and passes down the subsequent
        // cancel-stop.
        //

        if (StopPending == DeviceData->DevicePnPState)
        {
            //
            // We did receive a query-stop, so restore.
            //
            RESTORE_PREVIOUS_PNP_STATE(DeviceData);
            ASSERT(DeviceData->DevicePnPState == Started);
        }
        Irp->IoStatus.Status = STATUS_SUCCESS; // We must not fail the IRP.
        break;

    case IRP_MN_STOP_DEVICE:

		NdasDbgCall( 2, "IRP_MN_STOP_DEVICE\n" );

        //
        // Stop device means that the resources given during Start device
        // are now revoked. Note: You must not fail this Irp.
        // But before you relieve resources make sure there are no I/O in
        // progress. Wait for the existing ones to be finished.
        // To do that, first we will decrement this very operation.
        // When the counter goes to 1, Stop event is set.
        //

        Bus_DecIoCount(DeviceData);

        KeWaitForSingleObject(
                   &DeviceData->StopEvent,
                   Executive, // Waiting reason of a driver
                   KernelMode, // Waiting in kernel mode
                   FALSE, // No allert
                   NULL); // No timeout

        //
        // Increment the counter back because this IRP has to
        // be sent down to the lower stack.
        //

        Bus_IncIoCount (DeviceData);

        //
        // Free resources given by start device.
        //

        SET_NEW_PNP_STATE(DeviceData, Stopped);

        //
        // We don't need a completion routine so fire and forget.
        //
        // Set the current stack location to the next stack location and
        // call the next device object.
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:

		NdasDbgCall( 2, "IRP_MN_QUERY_REMOVE_DEVICE\n" );

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        SET_NEW_PNP_STATE(DeviceData, RemovePending);

        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:

		NdasDbgCall( 2, "IRP_MN_CANCEL_REMOVE_DEVICE\n" );

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        //
        // First check to see whether you have received cancel-remove
        // without first receiving a query-remove. This could happen if
        // someone above us fails a query-remove and passes down the
        // subsequent cancel-remove.
        //

        if (RemovePending == DeviceData->DevicePnPState)
        {
            //
            // We did receive a query-remove, so restore.
            //
            RESTORE_PREVIOUS_PNP_STATE(DeviceData);
        }
        Irp->IoStatus.Status = STATUS_SUCCESS;// You must not fail the IRP.
        break;

    case IRP_MN_SURPRISE_REMOVAL:

		NdasDbgCall( 2, "IRP_MN_SURPRISE_REMOVAL\n" );

        //
        // The device has been unexpectedly removed from the machine
        // and is no longer available for I/O. Bus_RemoveFdo clears
        // all the resources, frees the interface and de-registers
        // with WMI, but it doesn't delete the FDO. That's done
        // later in Remove device query.
        //

        SET_NEW_PNP_STATE(DeviceData, SurpriseRemovePending);
        Bus_RemoveFdo(DeviceData);

        ExAcquireFastMutex (&DeviceData->Mutex);

        listHead = &DeviceData->ListOfPDOs;

        for(entry = listHead->Flink,nextEntry = entry->Flink;
            entry != listHead;
            entry = nextEntry,nextEntry = entry->Flink) {

            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            RemoveEntryList (&pdoData->Link);
            InitializeListHead (&pdoData->Link);
            pdoData->ParentFdo  = NULL;
            pdoData->ReportedMissing = TRUE;
        }

        ExReleaseFastMutex (&DeviceData->Mutex);

        Irp->IoStatus.Status = STATUS_SUCCESS; // You must not fail the IRP.
        break;

    case IRP_MN_REMOVE_DEVICE:

		NdasDbgCall( 2, "IRP_MN_REMOVE_DEVICE\n" );

        //
        // The Plug & Play system has dictated the removal of this device.
        // We have no choice but to detach and delete the device object.
        //

        //
        // Check the state flag to see whether you are surprise removed
        //

        if (DeviceData->DevicePnPState != SurpriseRemovePending)
        {
            Bus_RemoveFdo(DeviceData);
        }

        SET_NEW_PNP_STATE(DeviceData, Deleted);

        //
        // Wait for all outstanding requests to complete.
        // We need two decrements here, one for the increment in
        // the beginning of this function, the other for the 1-biased value of
        // OutstandingIO.
        //

        Bus_DecIoCount (DeviceData);

        //
        // The requestCount is at least one here (is 1-biased)
        //

        Bus_DecIoCount (DeviceData);

        KeWaitForSingleObject (
                &DeviceData->RemoveEvent,
                Executive,
                KernelMode,
                FALSE,
                NULL);

        //
        // Typically the system removes all the  children before
        // removing the parent FDO. If for any reason child Pdos are
        // still present we will destroy them explicitly, with one exception -
        // we will not delete the PDOs that are in SurpriseRemovePending state.
        //

        ExAcquireFastMutex (&DeviceData->Mutex);

        listHead = &DeviceData->ListOfPDOs;

        for(entry = listHead->Flink,nextEntry = entry->Flink;
            entry != listHead;
            entry = nextEntry,nextEntry = entry->Flink) {

            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            RemoveEntryList (&pdoData->Link);
            if (SurpriseRemovePending == pdoData->DevicePnPState)
            {
                //
                // We will reinitialize the list head so that we
                // wouldn't barf when we try to delink this PDO from
                // the parent's PDOs list, when the system finally
                // removes the PDO. Let's also not forget to set the
                // ReportedMissing flag to cause the deletion of the PDO.
                //
                Bus_KdPrint_Cont(DeviceData, BUS_DBG_PNP_INFO,
                ("\tFound a surprise removed device: 0x%p\n", pdoData->Self));
                InitializeListHead (&pdoData->Link);
                pdoData->ParentFdo  = NULL;
                pdoData->ReportedMissing = TRUE;
                continue;
            }
            DeviceData->NumPDOs--;
            Bus_DestroyPdo (pdoData->Self, pdoData);
        }

        ExReleaseFastMutex (&DeviceData->Mutex);

        //
        // We need to send the remove down the stack before we detach,
        // but we don't need to wait for the completion of this operation
        // (and to register a completion routine).
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->NextLowerDriver, Irp);
        //
        // Detach from the underlying devices.
        //

        IoDetachDevice (DeviceData->NextLowerDriver);

        Bus_KdPrint_Cont(DeviceData, BUS_DBG_PNP_INFO,
                        ("\tDeleting FDO: 0x%p\n", DeviceObject));

        IoDeleteDevice (DeviceObject);

        return status;

    case IRP_MN_QUERY_DEVICE_RELATIONS:

        Bus_KdPrint_Cont (DeviceData, BUS_DBG_PNP_TRACE,
                   ("\tQueryDeviceRelation Type: %s\n",
                    DbgDeviceRelationString(\
                    IrpStack->Parameters.QueryDeviceRelations.Type)));

        if (BusRelations != IrpStack->Parameters.QueryDeviceRelations.Type) {
            //
            // We don't support any other Device Relations
            //
            break;
        }

        //
        // Tell the plug and play system about all the PDOs.
        //
        // There might also be device relations below and above this FDO,
        // so, be sure to propagate the relations from the upper drivers.
        //
        // No Completion routine is needed so long as the status is preset
        // to success.  (PDOs complete plug and play irps with the current
        // IoStatus.Status and IoStatus.Information as the default.)
        //

        ExAcquireFastMutex (&DeviceData->Mutex);

        oldRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information;
        if (oldRelations) {
            prevcount = oldRelations->Count;
            if (!DeviceData->NumPDOs) {
                //
                // There is a device relations struct already present and we have
                // nothing to add to it, so just call IoSkip and IoCall
                //
                ExReleaseFastMutex (&DeviceData->Mutex);
                break;
            }
        }
        else  {
            prevcount = 0;
        }

        //
        // Calculate the number of PDOs actually present on the bus
        //
        numPdosPresent = 0;
        for (entry = DeviceData->ListOfPDOs.Flink;
             entry != &DeviceData->ListOfPDOs;
             entry = entry->Flink) {
            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            if (pdoData->Present)
                numPdosPresent++;
        }

        //
        // Need to allocate a new relations structure and add our
        // PDOs to it.
        //

        length = sizeof(DEVICE_RELATIONS) +
                ((numPdosPresent + prevcount) * sizeof (PDEVICE_OBJECT)) -1;

        relations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag (PagedPool,
                                        length, BUSENUM_POOL_TAG);

        if (NULL == relations) {
            //
            // Fail the IRP
            //
            ExReleaseFastMutex (&DeviceData->Mutex);
            Irp->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
            IoCompleteRequest (Irp, IO_NO_INCREMENT);
            Bus_DecIoCount (DeviceData);
            return status;

        }

        //
        // Copy in the device objects so far
        //
        if (prevcount) {
            RtlCopyMemory (relations->Objects, oldRelations->Objects,
                                      prevcount * sizeof (PDEVICE_OBJECT));
        }

        relations->Count = prevcount + numPdosPresent;

        //
        // For each PDO present on this bus add a pointer to the device relations
        // buffer, being sure to take out a reference to that object.
        // The Plug & Play system will dereference the object when it is done
        // with it and free the device relations buffer.
        //

        for (entry = DeviceData->ListOfPDOs.Flink;
             entry != &DeviceData->ListOfPDOs;
             entry = entry->Flink) {

            pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
            if (pdoData->Present) {
                relations->Objects[prevcount] = pdoData->Self;
                ObReferenceObject (pdoData->Self);
                prevcount++;
            } else {
                pdoData->ReportedMissing = TRUE;
            }
        }

        Bus_KdPrint_Cont (DeviceData, BUS_DBG_PNP_TRACE,
                           ("\t#PDOS present = %d\n\t#PDOs reported = %d\n",
                             DeviceData->NumPDOs, relations->Count));

        //
        // Replace the relations structure in the IRP with the new
        // one.
        //
        if (oldRelations) {
            ExFreePool (oldRelations);
        }
        Irp->IoStatus.Information = (ULONG_PTR) relations;

        ExReleaseFastMutex (&DeviceData->Mutex);

        //
        // Set up and pass the IRP further down the stack
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    default:

        //
        // In the default case we merely call the next driver.
        // We must not modify Irp->IoStatus.Status or complete the IRP.
        //

        break;
    }

    IoSkipCurrentIrpStackLocation (Irp);
    status = IoCallDriver (DeviceData->NextLowerDriver, Irp);
    Bus_DecIoCount (DeviceData);
    return status;
}

NTSTATUS
Bus_StartFdo (
    __in  PFDO_DEVICE_DATA            FdoData,
    __in  PIRP   Irp )
/*++

Routine Description:

    Initialize and start the bus controller. Get the resources
    by parsing the list and map them if required.

Arguments:

    DeviceData - Pointer to the FDO's device extension.
    Irp          - Pointer to the irp.

Return Value:

    NT Status is returned.

--*/
{
    NTSTATUS status;
    POWER_STATE powerState;

    PAGED_CODE ();

	UNREFERENCED_PARAMETER(Irp);

    //
    // Check the function driver source to learn
    // about parsing resource list.
    //

    //
    // Enable device interface. If the return status is
    // STATUS_OBJECT_NAME_EXISTS means we are enabling the interface
    // that was already enabled, which could happen if the device
    // is stopped and restarted for resource rebalancing.
    //

    status = IoSetDeviceInterfaceState(&FdoData->InterfaceName, TRUE);
    if (!NT_SUCCESS (status)) {
        Bus_KdPrint (FdoData, BUS_DBG_PNP_TRACE,
                ("IoSetDeviceInterfaceState failed: 0x%x\n", status));
        return status;
    }

#if __NDAS_SCSI_BUS__

	//
	//	Initialize the registrar
	//

	status = LSBus_RegInitialize(FdoData);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint (FdoData, BUS_DBG_PNP_TRACE, 
			("IoSetDeviceInterfaceState failed: 0x%x\n", status));
	}

#endif

    //
    // Set the device power state to fully on. Also if this Start
    // is due to resource rebalance, you should restore the device
    // to the state it was before you stopped the device and relinquished
    // resources.
    //

    FdoData->DevicePowerState = PowerDeviceD0;
    powerState.DeviceState = PowerDeviceD0;
    PoSetPowerState ( FdoData->Self, DevicePowerState, powerState );

    SET_NEW_PNP_STATE(FdoData, Started);

    //
    // Register with WMI
    //
    status = Bus_WmiRegistration(FdoData);
    if (!NT_SUCCESS (status)) {
        Bus_KdPrint (FdoData, BUS_DBG_SS_ERROR,
        ("StartFdo: Bus_WmiRegistration failed (%x)\n", status));
    }

    return status;
}

void
Bus_RemoveFdo (
    __in PFDO_DEVICE_DATA FdoData
    )
/*++
Routine Description:

    Frees any memory allocated by the FDO and unmap any IO mapped as well.
    This function does not the delete the deviceobject.

Arguments:

    DeviceData - Pointer to the FDO's device extension.

Return Value:

    NT Status is returned.

--*/
{
    PAGED_CODE ();

    //
    // Stop all access to the device, fail any outstanding I/O to the device,
    // and free all the resources associated with the device.
    //

    //
    // Disable the device interface and free the buffer
    //

    if (FdoData->InterfaceName.Buffer != NULL) {

        IoSetDeviceInterfaceState (&FdoData->InterfaceName, FALSE);

        ExFreePool (FdoData->InterfaceName.Buffer);
        RtlZeroMemory (&FdoData->InterfaceName,
                       sizeof (UNICODE_STRING));
    }

    //
    // Inform WMI to remove this DeviceObject from its
    // list of providers.
    //

    Bus_WmiDeRegistration(FdoData);

#if __NDAS_SCSI_BUS__
	//
	//	Destroy the registrar
	//
	LSBus_RegDestroy(FdoData);
#endif
}

NTSTATUS
Bus_SendIrpSynchronously (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
/*++
Routine Description:

    Sends the Irp down to lower driver and waits for it to
    come back by setting a completion routine.

Arguments:
    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.

Return Value:

    NT Status is returned.

--*/
{
    KEVENT   event;
    NTSTATUS status;

    PAGED_CODE();

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);

    IoSetCompletionRoutine(Irp,
                           Bus_CompletionRoutine,
                           &event,
                           TRUE,
                           TRUE,
                           TRUE
                           );

    status = IoCallDriver(DeviceObject, Irp);

    //
    // Wait for lower drivers to be done with the Irp.
    // Important thing to note here is when you allocate
    // the memory for an event in the stack you must do a
    // KernelMode wait instead of UserMode to prevent
    // the stack from getting paged out.
    //

    if (status == STATUS_PENDING) {
       KeWaitForSingleObject(&event,
                             Executive,
                             KernelMode,
                             FALSE,
                             NULL
                             );
       status = Irp->IoStatus.Status;
    }

    return status;
}

NTSTATUS
Bus_CompletionRoutine(
    __in PDEVICE_OBJECT   DeviceObject,
    __in PIRP             Irp,
    __in PVOID            Context
    )
/*++
Routine Description:
    A completion routine for use when calling the lower device objects to
    which our bus (FDO) is attached.

Arguments:

    DeviceObject - Pointer to deviceobject
    Irp          - Pointer to a PnP Irp.
    Context      - Pointer to an event object
Return Value:

    NT Status is returned.

--*/
{
    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // If the lower driver didn't return STATUS_PENDING, we don't need to
    // set the event because we won't be waiting on it.
    // This optimization avoids grabbing the dispatcher lock and improves perf.
    //
    if (Irp->PendingReturned == TRUE) {

        KeSetEvent ((PKEVENT) Context, IO_NO_INCREMENT, FALSE);
    }
    return STATUS_MORE_PROCESSING_REQUIRED; // Keep this IRP
}

NTSTATUS
Bus_DestroyPdo (
    PDEVICE_OBJECT      Device,
    PPDO_DEVICE_DATA    PdoData
    )
/*++
Routine Description:
    The Plug & Play subsystem has instructed that this PDO should be removed.

    We should therefore
    - Complete any requests queued in the driver
    - If the device is still attached to the system,
    - then complete the request and return.
    - Otherwise, cleanup device specific allocations, memory, events...
    - Call IoDeleteDevice
    - Return from the dispatch routine.

    Note that if the device is still connected to the bus (IE in this case
    the enum application has not yet told us that the toaster device has disappeared)
    then the PDO must remain around, and must be returned during any
    query Device relations IRPS.

--*/

{
	NTSTATUS			ntStatus;
	PFDO_DEVICE_DATA	FdoData;

	PAGED_CODE ();

    // BusEnum does not queue any irps at this time so we have nothing to do.
    //
    //
    // Free any resources.
    
#if __NDAS_SCSI_BUS__

	Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("KeGetCurrentIrql() = 0x%x\n",	KeGetCurrentIrql()) );

	FdoData = (PFDO_DEVICE_DATA)PdoData->ParentFdo->DeviceExtension;

	NDAS_ASSERT(FdoData);

	//	Unregister from the persistent NDAS registry

	if (PdoData->Persistent) {

		ntStatus = LSBus_UnregisterDevice( FdoData, PdoData->SerialNo );

		if (!NT_SUCCESS(ntStatus)) {

			NDAS_ASSERT(FALSE);

			Bus_KdPrint (PdoData, BUS_DBG_PNP_ERROR,
				("LSBus_UnregisterDevice() failed. SlotNo=%u 0x%08lx\n", PdoData->SerialNo, ntStatus));
		}
	}

	// Close NDAS specific resources
		
	ntStatus = LSBus_CloseLanscsiAdapter( &PdoData->LanscsiAdapterPDO );
	
	if (!NT_SUCCESS(ntStatus)) {

		NDAS_ASSERT(FALSE);

		Bus_KdPrint( PdoData, BUS_DBG_PNP_ERROR,
					 ("LSBus_CloseLanscsiAdapter() failed. SlotNo=%u 0x%08lx\n", PdoData->SerialNo, ntStatus) );
	}
	
#endif

    if (PdoData->HardwareIDs) {

        ExFreePool( PdoData->HardwareIDs );
        PdoData->HardwareIDs = NULL;
    }

	NdasDbgCall( 2, "Device = %p, PdoData->ReferenceCount = %d\n", Device, PdoData->ReferenceCount );

	NDAS_ASSERT( PdoData->ReferenceCount == 1 );
	NDAS_ASSERT( PdoData->Self == Device );

	InterlockedDecrement( &PdoData->ReferenceCount );
	ObDereferenceObject( PdoData->Self );

	PdoData->Self = NULL;

	IoDeleteDevice (Device);
    return STATUS_SUCCESS;
}


NTSTATUS
Bus_InitializePdo (
    PDEVICE_OBJECT      Pdo,
    PFDO_DEVICE_DATA    FdoData
    )
/*++
Routine Description:
    Set the PDO into a known good starting state

--*/
{
    PPDO_DEVICE_DATA	pdoData;
	PPDO_DEVICE_DATA	tmpPdoData;
	PLIST_ENTRY         entry;
	BOOLEAN             unique;

    PAGED_CODE ();

    pdoData = (PPDO_DEVICE_DATA) Pdo->DeviceExtension;

    NdasDbgCall( 2, "Pdo = %p, pdoData->ReferenceCount = %d extension = %p\n", Pdo, pdoData->ReferenceCount, pdoData );

	ObReferenceObject(Pdo);
	InterlockedIncrement( &pdoData->ReferenceCount );

	NdasDbgCall( 2, "Pdo = %p, pdoData->ReferenceCount = %d extension = %p\n", Pdo, pdoData->ReferenceCount, pdoData );

    // Initialize the rest

	pdoData->IsFDO = FALSE;
    pdoData->Self =  Pdo;
    pdoData->DebugLevel = BusEnumDebugLevel;

    pdoData->ParentFdo = FdoData->Self;

    pdoData->Present = TRUE; // attached to the bus
    pdoData->ReportedMissing = FALSE; // not yet reported missing

    INITIALIZE_PNP_STATE(pdoData);

    // PDO's usually start their life at D3

    pdoData->DevicePowerState = PowerDeviceD3;
    pdoData->SystemPowerState = PowerSystemWorking;

    Pdo->Flags |= DO_POWER_PAGABLE;
	
	// To support paging file in the device, you have to turn off DO_POWER_PAGABLE flags.
	// But, the device will go to power saving mode after the network layers.
	// It makes serious obstacles for the device to enter power saving mode.
	// The device could fail to perform system IOs when entering power saving mode.
	// If the flag is on, Windows give the device notification earlier than non-pagable devices.
	//
	// NOTE: NDASSCSI's FDO should also have pagable flag.

	Pdo->Flags |= DO_POWER_PAGABLE;

	RtlZeroMemory( &pdoData->InterfaceName, sizeof (UNICODE_STRING) );
	pdoData->bRegisterInterface = FALSE;

    // Make sure the thread in which we execute cannot get
    // suspended in APC while we own the global resource.

    KeEnterCriticalRegion();

    ExAcquireFastMutex (&FdoData->Mutex);

	unique = TRUE;

	for (entry = FdoData->ListOfPDOs.Flink; entry != &FdoData->ListOfPDOs; entry = entry->Flink) {

		tmpPdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link); 

		if (pdoData->SerialNo == tmpPdoData->SerialNo && tmpPdoData->DevicePnPState != SurpriseRemovePending) {

			unique = FALSE;
			break;
		}
	}

	if (unique == FALSE) {

		ExReleaseFastMutex (&FdoData->Mutex);
		KeLeaveCriticalRegion();

		NDAS_ASSERT(FALSE);

		Bus_KdPrint( pdoData, BUS_DBG_SS_ERROR,
					("Another device exsists in the same slot. pdo 0x%p, extension 0x%p\n", Pdo, pdoData) );

		return STATUS_INVALID_PARAMETER;
	}

    InsertTailList(&FdoData->ListOfPDOs, &pdoData->Link);
    FdoData->NumPDOs++;

	ExReleaseFastMutex (&FdoData->Mutex);
  
	KeLeaveCriticalRegion();

    // This should be the last step in initialization.

	Pdo->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

#if 0

NTSTATUS
Bus_PlugInDevice (
    PNDASBUS_PLUGIN_HARDWARE_EX2	PlugIn,
    ULONG                       PlugInSize,
    PFDO_DEVICE_DATA            FdoData,
	BOOLEAN						Request32Bit,
	KPROCESSOR_MODE				RequestorMode,
	BOOLEAN						EnterFromRegistrar
    )
/*++
Routine Description:
    The user application has told us that a new device on the bus has arrived.
    
    We therefore need to create a new PDO, initialize it, add it to the list
    of PDOs for this FDO bus, and then tell Plug and Play that all of this
    happened so that it will start sending prodding IRPs.
--*/
{

	PAGED_CODE ();

	if(Request32Bit) {
		NTSTATUS	status;
		ULONG		length;
		PNDASBUS_PLUGIN_HARDWARE_EX2_32 plugIn32 = (PNDASBUS_PLUGIN_HARDWARE_EX2_32)PlugIn;
		PNDASBUS_PLUGIN_HARDWARE_EX2 thunkPlugIn;

		length = FIELD_OFFSET(NDASBUS_PLUGIN_HARDWARE_EX2, HardwareIDs) +
							plugIn32->HardwareIDLen  * sizeof(WCHAR);
		thunkPlugIn = ExAllocatePoolWithTag(NonPagedPool, length, BUSENUM_POOL_TAG);
		if(thunkPlugIn == NULL) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		//
		//	Convert 32 fields to pointer-aligned field.
		//

		thunkPlugIn->Size				= sizeof(NDASBUS_PLUGIN_HARDWARE_EX2);
		thunkPlugIn->SerialNo				= plugIn32->SlotNo;
		thunkPlugIn->MaxOsDataTransferLength	= plugIn32->MaxOsDataTransferLength;
		thunkPlugIn->DisconEvent		= (PHANDLE)plugIn32->DisconEvent;
		thunkPlugIn->AlarmEvent			= (PHANDLE)plugIn32->AlarmEvent;
		thunkPlugIn->Flags				= plugIn32->Flags;
		thunkPlugIn->HardwareIDLen		= plugIn32->HardwareIDLen;
		RtlCopyMemory(thunkPlugIn->HardwareIDs, plugIn32->HardwareIDs, plugIn32->HardwareIDLen * sizeof(WCHAR));

		status = Bus_PlugInDevice_PtrAlign(thunkPlugIn, PlugInSize, FdoData, RequestorMode, EnterFromRegistrar);

		ExFreePool(thunkPlugIn);

		return status;
	} else {
		return Bus_PlugInDevice_PtrAlign(PlugIn, PlugInSize, FdoData, RequestorMode, EnterFromRegistrar);
	}
}

#endif

NTSTATUS
Bus_PlugInDevice (
    PNDASBUS_PLUGIN_HARDWARE_EX2	PlugIn,
    ULONG							PlugInSize,
    PFDO_DEVICE_DATA				FdoData,
	KPROCESSOR_MODE					RequestorMode,
	BOOLEAN							EnterFromRegistrar
    )
/*++
Routine Description:
    The user application has told us that a new device on the bus has arrived.  
    
    We therefore need to create a new PDO, initialize it, add it to the list
    of PDOs for this FDO bus, and then tell Plug and Play that all of this
    happened so that it will start sending prodding IRPs.
--*/
{
    PDEVICE_OBJECT      pdo;
    PPDO_DEVICE_DATA    pdoData;
    NTSTATUS            status;
    ULONG               length;
    BOOLEAN             unique;
    PLIST_ENTRY         entry;
	PKEVENT				pEvent;
	PKEVENT				pAlarmEvent;

    PAGED_CODE ();

	UNREFERENCED_PARAMETER(PlugInSize);

    length = (PlugInSize - sizeof (PNDASBUS_PLUGIN_HARDWARE_EX2))/sizeof(WCHAR);
    length = PlugIn->HardwareIDLen;

	NdasDbgCall( 2, "Exposing PDO\n" );

    NdasDbgCall( 2, "==SlotNo:      %d\n"
				    "==MaxOsTrans:  %d bytes\n"
                    "==HardwareId:  %ws\n"
                    "==Length:      %d\n"
				    "==DisconEvent: 0x%p\n"
				    "==AlarmEvent:  0x%p\n"
				    "==Flags:       0x%x\n",
					PlugIn->SerialNo,
					PlugIn->MaxOsDataTransferLength,
					PlugIn->HardwareIDs,
					length,
					PlugIn->DisconEvent,
					PlugIn->AlarmEvent,
					PlugIn->Flags );


    if ( L'\0' != PlugIn->HardwareIDs[length - 1] || L'\0' != PlugIn->HardwareIDs[length - 2]) {

		NDAS_ASSERT(FALSE);
        return STATUS_INVALID_PARAMETER;
    }

	// Get Event Object.

	status = ObReferenceObjectByHandle( PlugIn->DisconEvent,
										EVENT_MODIFY_STATE,
										*ExEventObjectType,
										RequestorMode,
										(PVOID*) &pEvent,
										NULL );


	if (!NT_SUCCESS(status)) {

		NDAS_ASSERT(FALSE);
		Bus_KdPrint_Def( BUS_DBG_PNP_ERROR, ("Can't Get Event Object by Handle.\n") );
		
		return STATUS_UNSUCCESSFUL;
	}

	// Get Event Object.

	status = ObReferenceObjectByHandle( PlugIn->AlarmEvent,
										EVENT_MODIFY_STATE,
										*ExEventObjectType,
										RequestorMode,
										(PVOID*) &pAlarmEvent,
										NULL );
	
	if (!NT_SUCCESS(status)) {

		NDAS_ASSERT(FALSE);
		Bus_KdPrint_Def(BUS_DBG_PNP_ERROR, ("Can't Get Event Object by Handle.\n"));
		
		ObDereferenceObject(pEvent);
		return STATUS_UNSUCCESSFUL;
	}

    // Following code walks the PDO list to check whether
    // the input serial number is unique.
    // Let's first assume it's unique

    unique = TRUE;

    // Make sure the thread in which we execute cannot get
    // suspended in APC while we own the global resource.

    KeEnterCriticalRegion();
    ExAcquireFastMutex (&FdoData->Mutex);

	NDAS_ASSERT( PlugIn->SerialNo );

#if !__NDAS_SCSI_BUS__

    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink) {

        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);
        if (PlugIn->SerialNo == pdoData->SerialNo &&
            pdoData->DevicePnPState != SurpriseRemovePending)
        {
            unique = FALSE;
            break;
        }
    }

#else

	if (PlugIn->SerialNo) {

		for (entry = FdoData->ListOfPDOs.Flink; entry != &FdoData->ListOfPDOs; entry = entry->Flink) {

	        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);

			if (PlugIn->SerialNo == pdoData->SerialNo && pdoData->DevicePnPState != SurpriseRemovePending) {

				unique = FALSE;
	            break;
			}
		}

	} else {
		
		ULONG	slotno;

		// Assign a slot number.

		NDAS_ASSERT(FALSE);

		Bus_KdPrint_Def( BUS_DBG_PNP_INFO, ("Assign a slot number automatically.\n") );

		for (slotno = 1; slotno <= NDASBUS_MAX_SLOTNUMBER_FOR_AUTOASSIGN; slotno ++) {

			for(entry = FdoData->ListOfPDOs.Flink; entry != &FdoData->ListOfPDOs; entry = entry->Flink) {

				pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);

				if (slotno == pdoData->SerialNo && pdoData->DevicePnPState != SurpriseRemovePending) {

			        unique = FALSE;
					break;
				}
			}

			if (unique) {

				PlugIn->SerialNo = slotno;
				Bus_KdPrint_Def(BUS_DBG_PNP_INFO, ("Assigned slot number: %d.\n", slotno));

				break;
			}

			unique = TRUE;
		}
	}

	NDAS_ASSERT( PlugIn->SerialNo );

#endif

	NDAS_ASSERT( PlugIn->SerialNo == PlugIn->NdasBusAddTargetData.ulSlotNo );

    ExReleaseFastMutex (&FdoData->Mutex); 
    KeLeaveCriticalRegion();

    // Just be aware: If a user unplugs and plugs in the device
    // immediately, in a split second, even before we had a chance to
    // clean up the previous PDO completely, we might fail to
    // accept the device.

    if (!unique) {

		NdasDbgCall( 1, "Using Slot Number %d\n", PlugIn->SerialNo );
		NDAS_ASSERT(FALSE);

		ObDereferenceObject(pEvent);
		ObDereferenceObject(pAlarmEvent);

        return STATUS_INVALID_PARAMETER;
	}

    // Create the PDO

    length *= sizeof(WCHAR);

    Bus_KdPrint( FdoData, BUS_DBG_PNP_NOISE, ("FdoData->NextLowerDriver = 0x%p\n", FdoData->NextLowerDriver) );

    // PDO must have a name. You should let the system auto generate a
    // name by specifying FILE_AUTOGENERATED_DEVICE_NAME in the
    // DeviceCharacteristics parameter. Let us create a secure deviceobject,
    // in case the child gets installed as a raw device (RawDeviceOK), to prevent
    // an unpriviledged user accessing our device. This function is avaliable
    // in a static WDMSEC.LIB and can be used in Win2k, XP, and Server 2003
    // Just make sure that  the GUID specified here is not a setup class GUID.
    // If you specify a setup class guid, you must make sure that class is
    // installed before enumerating the PDO.

#if !__NDAS_SCSI_BUS__
    status = IoCreateDeviceSecure(FdoData->Self->DriverObject,
                sizeof (PDO_DEVICE_DATA),
                NULL,
                FILE_DEVICE_BUS_EXTENDER,
                FILE_AUTOGENERATED_DEVICE_NAME |FILE_DEVICE_SECURE_OPEN,
                FALSE,
                &SDDL_DEVOBJ_SYS_ALL_ADM_ALL, // read wdmsec.h for more info
                (LPCGUID)&GUID_SD_BUSENUM_PDO,
                &pdo);
#else

	status = IoCreateDevice( FdoData->Self->DriverObject,
							 sizeof (PDO_DEVICE_DATA),
							 NULL, 
							 FILE_DEVICE_BUS_EXTENDER,
							 FILE_DEVICE_SECURE_OPEN | FILE_AUTOGENERATED_DEVICE_NAME,
							 FALSE,
							 &pdo );
#endif

	if (!NT_SUCCESS (status)) {
	
		ObDereferenceObject(pEvent);
		ObDereferenceObject(pAlarmEvent);

		return status;
    }

    pdoData = (PPDO_DEVICE_DATA) pdo->DeviceExtension;

    // Copy the hardware IDs

    pdoData->HardwareIDs = ExAllocatePoolWithTag(NonPagedPool, length, BUSENUM_POOL_TAG);

    if (pdoData->HardwareIDs == NULL) {

        IoDeleteDevice(pdo);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	pdoData->HardwareIDLen = PlugIn->HardwareIDLen;

    RtlCopyMemory( pdoData->HardwareIDs, PlugIn->HardwareIDs, length );
    
	pdoData->SerialNo = PlugIn->SerialNo;
   
	status = LSBus_OpenLanscsiAdapter( &pdoData->LanscsiAdapterPDO, PlugIn->MaxOsDataTransferLength, pEvent, pAlarmEvent );

	if (!NT_SUCCESS (status)) {

		NDAS_ASSERT(FALSE);
		Bus_KdPrint_Cont (FdoData, BUS_DBG_PNP_ERROR, ("LSBus_OpenLanscsiAdapter() failed. STATUS=%08lx\n", status));

		ExFreePool (pdoData->HardwareIDs);
		pdoData->HardwareIDs = NULL;
		IoDeleteDevice(pdo);

		ObDereferenceObject(pEvent);
		ObDereferenceObject(pAlarmEvent);

		return status;
	}

	NdasDbgCall( 2, "PlugIn->MaxOsDataTransferLength %d bytes\n", PlugIn->MaxOsDataTransferLength );

	RtlCopyMemory( &pdoData->LanscsiAdapterPDO.AddDevInfo,
				   &PlugIn->NdasBusAddTargetData,
				   PlugIn->NdasBusAddTargetData.ulSize );

	NDAS_ASSERT( PlugIn->NdasBusAddTargetData.ulSize );

	pdoData->LanscsiAdapterPDO.AddDevInfoLength = PlugIn->NdasBusAddTargetData.ulSize;

	// Init PDO status

	pdoData->LanscsiAdapterPDO.LastAdapterStatus = NDASSCSI_ADAPTER_STATUS_INIT;
	pdoData->LanscsiAdapterPDO.DeviceMode = PlugIn->NdasBusAddTargetData.DeviceMode;

	//	Notify to NDASSCSI

	Bus_KdPrint( FdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_NDASBUS_ADD_TARGET SetEvent AddTargetEvent!\n") );
	KeSetEvent( &pdoData->LanscsiAdapterPDO.AddTargetEvent, IO_NO_INCREMENT, FALSE );

	//	Register NDAS device
	
	pdoData->Persistent = FdoData->PersistentPdo;

	if (pdoData->Persistent) {

		if (PlugIn->Flags & PLUGINFLAG_NOT_REGISTER) {

			Bus_KdPrint_Cont ( FdoData, BUS_DBG_PNP_INFO, ("The PDO is not set persistent temporarly.\n") );
			pdoData->Persistent = FALSE;

		} else {

			//	If this is auto-enumerated by the registrar,
			//	Do not try to register again.

			if (EnterFromRegistrar == FALSE) {

				status = LSBus_RegisterDevice( FdoData, PlugIn );

				if (!NT_SUCCESS(status)) {

					NDAS_ASSERT( FALSE );
					Bus_KdPrint_Cont( FdoData, BUS_DBG_PNP_ERROR, ("LSBus_RegisterDevice() failed. STATUS=%08lx\n", status) );

					ExFreePool( pdoData->HardwareIDs );
					pdoData->HardwareIDs = NULL;
					IoDeleteDevice( pdo );

					ObDereferenceObject( pEvent );
					ObDereferenceObject( pAlarmEvent );

					return status;
				}

				status = LSBus_RegisterTarget(FdoData, &pdoData->LanscsiAdapterPDO.AddDevInfo );

				if (!NT_SUCCESS(status)) {

					NDAS_ASSERT( FALSE );
					Bus_KdPrint_Cont( FdoData, BUS_DBG_PNP_ERROR, ("LSBus_RegisterDevice() failed. STATUS=%08lx\n", status) );

					NDBusIoctlLogError(	FdoData->Self,
										NDASBUS_IO_REGISTER_TARGET_FAIL,
										IOCTL_NDASBUS_PLUGIN_HARDWARE_EX2,
										PlugIn->SerialNo );

					LSBus_UnregisterDevice( FdoData, PlugIn->SerialNo );

					ExFreePool( pdoData->HardwareIDs );
					pdoData->HardwareIDs = NULL;
					IoDeleteDevice( pdo );

					ObDereferenceObject( pEvent );
					ObDereferenceObject( pAlarmEvent );

					return status;
				}
			}

			Bus_KdPrint_Cont (FdoData, BUS_DBG_PNP_INFO, ("The PDO set persistent.\n"));
		}
	}
#if DBG
	else {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_PNP_INFO, ("The PDO is not set persistent.\n"));
	}
#endif

	//
	//	Initialize PDO.
	//

#if !__NDAS_SCSI_BUS__
    Bus_InitializePdo (pdo, FdoData);
#else
	status = Bus_InitializePdo (pdo, FdoData);

	if (!NT_SUCCESS(status)) {

		LSBus_UnregisterTarget(FdoData, PlugIn->SerialNo, pdoData->LanscsiAdapterPDO.AddDevInfo.ucTargetId);
		LSBus_UnregisterDevice(FdoData, PlugIn->SerialNo);

		ExFreePool (pdoData->HardwareIDs);
		pdoData->HardwareIDs = NULL;
		IoDeleteDevice(pdo);

		ObDereferenceObject(pEvent);
		ObDereferenceObject(pAlarmEvent);

		return status;
	}

#endif

    // Device Relation changes if a new pdo is created. So let
    // the PNP system now about that. This forces it to send bunch of pnp
    // queries and cause the function driver to be loaded.

    IoInvalidateDeviceRelations( FdoData->UnderlyingPDO, BusRelations );

	NdasDbgCall( 2, "out\n" );

    return status;
}

NTSTATUS
Bus_UnPlugDevice (
    PNDASBUS_UNPLUG_HARDWARE	UnPlug,
    PFDO_DEVICE_DATA            FdoData
    )
/*++
Routine Description:
    The application has told us a device has departed from the bus.

    We therefore need to flag the PDO as no longer present
    and then tell Plug and Play about it.

Arguments:

    Remove   - pointer to remove hardware structure.
    FdoData - contains the list to iterate over

Returns:

    STATUS_SUCCESS upon successful removal from the list
    STATUS_INVALID_PARAMETER if the removal was unsuccessful

--*/
{
    PLIST_ENTRY         entry;
    PPDO_DEVICE_DATA    pdoData;
    BOOLEAN             found = FALSE, plugOutAll;

    PAGED_CODE ();

    plugOutAll = (0 == UnPlug->SerialNo);

#if __NDAS_SCSI_BUS__
	if (plugOutAll) {

		NDAS_ASSERT(FALSE);
		return STATUS_INVALID_PARAMETER;
	}
#endif

    ExAcquireFastMutex (&FdoData->Mutex);

    if (plugOutAll) {

        Bus_KdPrint( FdoData, BUS_DBG_IOCTL_NOISE, ("Plugging out all the devices!\n") );
    
	} else {
    
		Bus_KdPrint( FdoData, BUS_DBG_IOCTL_NOISE, ("Plugging out %d\n", UnPlug->SerialNo) );
    }

    if (FdoData->NumPDOs == 0) {

		// We got a 2nd plugout...somebody in user space isn't playing nice!!!
 
		NDAS_ASSERT(FALSE);

		Bus_KdPrint( FdoData, BUS_DBG_IOCTL_ERROR, ("BAD BAD BAD...2 removes!!! Send only one!\n") );
        ExReleaseFastMutex (&FdoData->Mutex);

		return STATUS_NO_SUCH_DEVICE;
    }

    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink) {

        pdoData = CONTAINING_RECORD( entry, PDO_DEVICE_DATA, Link );

		NdasDbgCall( 1, "found device %d\n", pdoData->SerialNo );

        if (plugOutAll || UnPlug->SerialNo == pdoData->SerialNo) {

            NdasDbgCall( 1, "Plugged out %d\n", pdoData->SerialNo );

            pdoData->Present = FALSE;
            found = TRUE;

			if (!plugOutAll) {
            
				break;
            }
        }
    }

    ExReleaseFastMutex (&FdoData->Mutex);

    if (found) {

#if __NDAS_SCSI_BUS__

		if (pdoData->Persistent) {

			NTSTATUS status;

			status = LSBus_UnregisterTarget( FdoData, 
											 pdoData->SerialNo, 
											 pdoData->LanscsiAdapterPDO.AddDevInfo.ucTargetId );
		
			if (!NT_SUCCESS(status)) {
	
				NDAS_ASSERT(FALSE);
							
				NdasDbgCall( 1, "REMOVE_TARGET: Removed  Target instance, but LSBus_UnregisterTarget() failed.\n" );

				NDBusIoctlLogError(	FdoData->Self,
									NDASBUS_IO_UNREGISTER_TARGET_FAIL,
									IOCTL_NDASBUS_UNPLUG_HARDWARE,
									pdoData->SerialNo );

			} else {

				NdasDbgCall( 1, "REMOVE_TARGET: LSBus_UnregisterTarget() succeeded.\n" );
			}
		}
				
		//	Set Disconnection event if not set

		if (KeReadStateEvent(pdoData->LanscsiAdapterPDO.DisconEventToService) == 0) {

			KeSetEvent( pdoData->LanscsiAdapterPDO.DisconEventToService, IO_NO_INCREMENT, FALSE );
		}
#endif

		IoInvalidateDeviceRelations( FdoData->UnderlyingPDO, BusRelations );
        return STATUS_SUCCESS;
    }

    NdasDbgCall( 1, "Device %d is not present\n", UnPlug->SerialNo );

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
Bus_EjectDevice (
    PNDASBUS_EJECT_HARDWARE     Eject,
    PFDO_DEVICE_DATA            FdoData
    )
/*++
Routine Description:
    The user application has told us to eject the device from the bus.
    In a real situation the driver gets notified by an interrupt when the
    user presses the Eject button on the device.

Arguments:

    Eject   - pointer to Eject hardware structure.
    FdoData - contains the list to iterate over

Returns:

    STATUS_SUCCESS upon successful removal from the list
    STATUS_INVALID_PARAMETER if the removal was unsuccessful

--*/
{
    PLIST_ENTRY         entry;
    PPDO_DEVICE_DATA    pdoData;
    BOOLEAN             found = FALSE, ejectAll;

    PAGED_CODE ();

    ejectAll = (0 == Eject->SerialNo);

    //
    // Make sure the thread in which we execute cannot get
    // suspended in APC while we own the global resource.
    //
    KeEnterCriticalRegion();
    ExAcquireFastMutex (&FdoData->Mutex);

    if (ejectAll) {
        Bus_KdPrint (FdoData, BUS_DBG_IOCTL_NOISE,
                      ("Ejecting all the pdos!\n"));
    } else {
        Bus_KdPrint (FdoData, BUS_DBG_IOCTL_NOISE,
                      ("Ejecting %d\n", Eject->SerialNo));
    }

    if (FdoData->NumPDOs == 0) {
        //
        // Somebody in user space isn't playing nice!!!
        //
        Bus_KdPrint (FdoData, BUS_DBG_IOCTL_ERROR,
                      ("No devices to eject!\n"));
        ExReleaseFastMutex (&FdoData->Mutex);
        KeLeaveCriticalRegion();
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Scan the list to find matching PDOs
    //
    for (entry = FdoData->ListOfPDOs.Flink;
         entry != &FdoData->ListOfPDOs;
         entry = entry->Flink) {

        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);

        Bus_KdPrint (FdoData, BUS_DBG_IOCTL_NOISE,
                      ("found device %d\n", pdoData->SerialNo));

        if (ejectAll || Eject->SerialNo == pdoData->SerialNo) {
            Bus_KdPrint (FdoData, BUS_DBG_IOCTL_INFO,
                          ("Ejected %d\n", pdoData->SerialNo));
            found = TRUE;
            IoRequestDeviceEject(pdoData->Self);
            if (!ejectAll) {
                break;
            }
        }
    }
    ExReleaseFastMutex (&FdoData->Mutex);
    KeLeaveCriticalRegion();

    if (found) {
        return STATUS_SUCCESS;
    }

    Bus_KdPrint (FdoData, BUS_DBG_IOCTL_ERROR,
                  ("Device %d is not present\n", Eject->SerialNo));

    return STATUS_INVALID_PARAMETER;
}

#if DBG

PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
)
{
    switch (MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            return "IRP_MN_START_DEVICE";
        case IRP_MN_QUERY_REMOVE_DEVICE:
            return "IRP_MN_QUERY_REMOVE_DEVICE";
        case IRP_MN_REMOVE_DEVICE:
            return "IRP_MN_REMOVE_DEVICE";
        case IRP_MN_CANCEL_REMOVE_DEVICE:
            return "IRP_MN_CANCEL_REMOVE_DEVICE";
        case IRP_MN_STOP_DEVICE:
            return "IRP_MN_STOP_DEVICE";
        case IRP_MN_QUERY_STOP_DEVICE:
            return "IRP_MN_QUERY_STOP_DEVICE";
        case IRP_MN_CANCEL_STOP_DEVICE:
            return "IRP_MN_CANCEL_STOP_DEVICE";
        case IRP_MN_QUERY_DEVICE_RELATIONS:
            return "IRP_MN_QUERY_DEVICE_RELATIONS";
        case IRP_MN_QUERY_INTERFACE:
            return "IRP_MN_QUERY_INTERFACE";
        case IRP_MN_QUERY_CAPABILITIES:
            return "IRP_MN_QUERY_CAPABILITIES";
        case IRP_MN_QUERY_RESOURCES:
            return "IRP_MN_QUERY_RESOURCES";
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
            return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
        case IRP_MN_QUERY_DEVICE_TEXT:
            return "IRP_MN_QUERY_DEVICE_TEXT";
        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
            return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
        case IRP_MN_READ_CONFIG:
            return "IRP_MN_READ_CONFIG";
        case IRP_MN_WRITE_CONFIG:
            return "IRP_MN_WRITE_CONFIG";
        case IRP_MN_EJECT:
            return "IRP_MN_EJECT";
        case IRP_MN_SET_LOCK:
            return "IRP_MN_SET_LOCK";
        case IRP_MN_QUERY_ID:
            return "IRP_MN_QUERY_ID";
        case IRP_MN_QUERY_PNP_DEVICE_STATE:
            return "IRP_MN_QUERY_PNP_DEVICE_STATE";
        case IRP_MN_QUERY_BUS_INFORMATION:
            return "IRP_MN_QUERY_BUS_INFORMATION";
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
            return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
        case IRP_MN_SURPRISE_REMOVAL:
            return "IRP_MN_SURPRISE_REMOVAL";
        case IRP_MN_QUERY_LEGACY_BUS_INFORMATION:
            return "IRP_MN_QUERY_LEGACY_BUS_INFORMATION";
        default:
            return "unknown_pnp_irp";
    }
}

PCHAR
DbgDeviceRelationString(
    __in DEVICE_RELATION_TYPE Type
    )
{
    switch (Type)
    {
        case BusRelations:
            return "BusRelations";
        case EjectionRelations:
            return "EjectionRelations";
        case RemovalRelations:
            return "RemovalRelations";
        case TargetDeviceRelation:
            return "TargetDeviceRelation";
        default:
            return "UnKnown Relation";
    }
}

PCHAR
DbgDeviceIDString(
    BUS_QUERY_ID_TYPE Type
    )
{
    switch (Type)
    {
        case BusQueryDeviceID:
            return "BusQueryDeviceID";
        case BusQueryHardwareIDs:
            return "BusQueryHardwareIDs";
        case BusQueryCompatibleIDs:
            return "BusQueryCompatibleIDs";
        case BusQueryInstanceID:
            return "BusQueryInstanceID";
        case BusQueryDeviceSerialNumber:
            return "BusQueryDeviceSerialNumber";
        default:
            return "UnKnown ID";
    }
}

#endif


