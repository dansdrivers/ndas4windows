/*++

Copyright (C) Microsoft Corporation, 1996 - 1999

Module Name:

    pdo.c

Abstract:

    This module contains the dispatch routines for scsiport's physical device
    objects

Authors:

    Peter Wieland

Environment:

    Kernel mode only

Notes:

Revision History:

--*/

#include "port.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSMP_remove"

#ifdef __INTERRUPT__

#if DBG
static const char *__file__ = __FILE__;
#endif

VOID
SpWaitForRemoveLock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID LockTag
    );

VOID
SpAdapterCleanup(
    IN PADAPTER_EXTENSION DeviceExtension
    );

VOID
SpReapChildren(
    IN PADAPTER_EXTENSION Adapter
    );

BOOLEAN
SpTerminateAdapterSynchronized (
    IN PADAPTER_EXTENSION Adapter
    );

BOOLEAN
SpRemoveAdapterSynchronized(
    IN PADAPTER_EXTENSION Adapter
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SpDeleteLogicalUnit)
#pragma alloc_text(PAGE, SpRemoveLogicalUnit)
#pragma alloc_text(PAGE, SpWaitForRemoveLock)
#pragma alloc_text(PAGE, SpAdapterCleanup)
#pragma alloc_text(PAGE, SpReapChildren)

#pragma alloc_text(PAGELOCK, ScsiPortRemoveAdapter)
#endif


BOOLEAN
SpRemoveLogicalUnit(
    IN PDEVICE_OBJECT LogicalUnit,
    IN UCHAR RemoveType
    )

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension = LogicalUnit->DeviceExtension;
    PCOMMON_EXTENSION commonExtension = LogicalUnit->DeviceExtension;
    PADAPTER_EXTENSION adapterExtension = logicalUnitExtension->AdapterExtension;

    ULONG isRemoved;
    ULONG oldDebugLevel;

    PAGED_CODE();

    if(commonExtension->IsRemoved != REMOVE_COMPLETE) {

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            SpWaitForRemoveLock(LogicalUnit, UIntToPtr( 0xabcdabcd) );

            //
            // If the device was claimed we should release it now.
            //

            if(logicalUnitExtension->IsClaimed) {
                logicalUnitExtension->IsClaimed = FALSE;
                logicalUnitExtension->IsLegacyClaim = FALSE;
            }

        }

        DebugPrint((1, "SpRemoveLogicalUnit - %sremoving device %#p\n",
                    (RemoveType == IRP_MN_SURPRISE_REMOVAL) ? "surprise " : "",
                    LogicalUnit));

        //
        // If the lun isn't marked as missing yet or is marked as missing but 
        // PNP hasn't been informed yet then we cannot delete it.  Set it back 
        // to the NO_REMOVE state so that we'll be able to attempt a rescan.
        //
        // Likewise if the lun is invisible then just swallow the remove 
        // operation now that we've cleared any existing claims.
        //

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            //
            // If the device is not missing or is missing but is still 
            // enumerated then don't finish destroying it.
            //

            if((logicalUnitExtension->IsMissing == TRUE) &&
               (logicalUnitExtension->IsEnumerated == FALSE)) {

                // do nothing here - fall through and destroy the device.

            } else {

                DebugPrint((1, "SpRemoveLogicalUnit - device is not missing "
                               "and will not be destroyed\n"));

                SpAcquireRemoveLock(LogicalUnit, UIntToPtr( 0xabcdabcd ));

                logicalUnitExtension->CommonExtension.IsRemoved = NO_REMOVE;

                return FALSE;
            }

        } else if((logicalUnitExtension->IsVisible == FALSE) && 
                  (logicalUnitExtension->IsMissing == FALSE)) {

            //
            // The surprise remove came because the device is no longer 
            // visible.  We don't want to destroy it.
            //

            return FALSE;
        }

        //
        // Mark the device as uninitialized so that we'll go back and
        // recreate all the necessary stuff if it gets restarted.
        //

        commonExtension->IsInitialized = FALSE;

        //
        // Delete the device map entry for this one (if any).
        //

        SpDeleteDeviceMapEntry(LogicalUnit);

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            ASSERT(logicalUnitExtension->RequestTimeoutCounter == -1);
            ASSERT(logicalUnitExtension->ReadyLogicalUnit == NULL);
            ASSERT(logicalUnitExtension->PendingRequest == NULL);
            ASSERT(logicalUnitExtension->BusyRequest == NULL);
            ASSERT(logicalUnitExtension->QueueCount == 0);
    
            commonExtension->IsRemoved = REMOVE_COMPLETE;
            SpDeleteLogicalUnit(LogicalUnit);
        }
    }

    return TRUE;
}


VOID
SpDeleteLogicalUnit(
    IN PDEVICE_OBJECT LogicalUnit
    )

/*++

Routine Description:

    This routine will release any resources held for the logical unit, mark the
    device extension as deleted, and call the io system to actually delete
    the object.  The device object will be deleted once it's reference count
    drops to zero.

Arguments:

    LogicalUnit - the device object for the logical unit to be deleted.

Return Value:

    none

--*/

{
    PLOGICAL_UNIT_EXTENSION luExtension = LogicalUnit->DeviceExtension;
    PCOMMON_EXTENSION commonExtension = LogicalUnit->DeviceExtension;

    PAGED_CODE();

    ASSERT(luExtension->ReadyLogicalUnit == NULL);
    ASSERT(luExtension->PendingRequest == NULL);
    ASSERT(luExtension->BusyRequest == NULL);
    ASSERT(luExtension->QueueCount == 0);

    //
    // Unregister with WMI.
    //

    if(commonExtension->WmiInitialized == TRUE) {

        //
        // Destroy all our WMI resources and unregister with WMI.
        //

        IoWMIRegistrationControl(LogicalUnit,
            WMIREG_ACTION_DEREGISTER);

        // BUGBUG
        // We should be asking the WmiFreeRequestList of remove some
        // free cells.

        commonExtension->WmiInitialized = FALSE;
        SpWmiDestroySpRegInfo(LogicalUnit);
    }

    SpDeleteDeviceMapEntry(luExtension->DeviceObject);

    //
    // Yank this out of the logical unit list.
    //

    SpRemoveLogicalUnitFromBin(luExtension->AdapterExtension, luExtension);

#if DBG
    // ASSERT(commonExtension->RemoveTrackingList == NULL);
    ExDeleteNPagedLookasideList(&(commonExtension->RemoveTrackingLookasideList));
#endif

    //
    // If the request sense irp still exists, delete it.
    //

    if(luExtension->RequestSenseIrp != NULL) {
        IoFreeIrp(luExtension->RequestSenseIrp);
        luExtension->RequestSenseIrp = NULL;
    }

    IoDeleteDevice(LogicalUnit);

    return;
}


VOID
ScsiPortRemoveAdapter(
    IN PDEVICE_OBJECT AdapterObject,
    IN BOOLEAN Surprise
    )
{
    PADAPTER_EXTENSION adapter = AdapterObject->DeviceExtension;
    PCOMMON_EXTENSION commonExtension = AdapterObject->DeviceExtension;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    ASSERT_FDO(AdapterObject);
    ASSERT(adapter->IsPnp);

    //
    // Set the flag PD_ADAPTER_REMOVED to keep scsiport from calling into the 
    // miniport after we've started this teardown.
    //

    if(Surprise == FALSE) {
        PVOID sectionHandle;
        KIRQL oldIrql;

        //
        // Wait until all outstanding requests have been completed.
        //
    
        SpWaitForRemoveLock(AdapterObject, AdapterObject);

        //
        // If the device is started we should uninitialize the miniport and 
        // release it's resources.  Fortunately this is exactly what stop does.  
        //

        if((commonExtension->CurrentPnpState != IRP_MN_SURPRISE_REMOVAL) &&
           ((commonExtension->CurrentPnpState == IRP_MN_START_DEVICE) ||
            (commonExtension->PreviousPnpState == IRP_MN_START_DEVICE))) {

            //
            // Okay.  If this adapter can't support remove then we're dead
            //

            ASSERT(SpIsAdapterControlTypeSupported(adapter, ScsiStopAdapter) == TRUE);

            //
            // Stop the miniport now that it's safe.
            //

            SpEnableDisableAdapter(adapter, FALSE);

            //
            // Mark the adapter as removed.
            //
    
    #ifdef ALLOC_PRAGMA
            sectionHandle = MmLockPagableCodeSection(ScsiPortRemoveAdapter);
            InterlockedIncrement(&SpPAGELOCKLockCount);
    #endif
            KeAcquireSpinLock(&(adapter->SpinLock), &oldIrql);
            adapter->SynchronizeExecution(adapter->InterruptObject,
                                          SpRemoveAdapterSynchronized,
                                          adapter);
    
            KeReleaseSpinLock(&(adapter->SpinLock), oldIrql);
    
    #ifdef ALLOC_PRAGMA
            InterlockedDecrement(&SpPAGELOCKLockCount);
            MmUnlockPagableImageSection(sectionHandle);
    #endif

        }
        SpReapChildren(adapter);
    }

    if(commonExtension->WmiInitialized == TRUE) {

        //
        // Destroy all our WMI resources and unregister with WMI.
        //

        IoWMIRegistrationControl(AdapterObject, WMIREG_ACTION_DEREGISTER);
        SpWmiRemoveFreeMiniPortRequestItems(adapter);
        commonExtension->WmiInitialized = FALSE;
    }

    //
    // If we were surprise removed then this has already been done once.  
    // In that case don't try to run the cleanup code a second time even though 
    // it's safe to do so.
    //

    SpDeleteDeviceMapEntry(AdapterObject);
    SpDestroyAdapter(adapter, Surprise);

    return;
}


VOID
SpWaitForRemoveLock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID LockTag
    )
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    //
    // Mark the thing as removing
    //

    commonExtension->IsRemoved = REMOVE_PENDING;

    //
    // Release our outstanding lock.
    //

    SpReleaseRemoveLock(DeviceObject, LockTag);

    DebugPrint((1, "SpWaitForRemoveLock - Reference count is now %d\n",
                commonExtension->RemoveLock));

    KeWaitForSingleObject(&(commonExtension->RemoveEvent),
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    DebugPrint((1, "SpWaitForRemoveLock - removing device %#p\n",
                DeviceObject));

    return;
}


VOID
SpDestroyAdapter(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Surprise
    )
{
    SpReleaseAdapterResources(Adapter, Surprise);
    SpAdapterCleanup(Adapter);
    return;
}


VOID
SpAdapterCleanup(
    IN PADAPTER_EXTENSION Adapter
    )

/*++

Routine Description:

    This routine cleans up the names associated with the specified adapter
    and the i/o system counts.

Arguments:

    Adapter - Supplies a pointer to the device extension to be deleted.

Return Value:

    None.

--*/

{
    PCOMMON_EXTENSION commonExtension = &(Adapter->CommonExtension);

    PAGED_CODE();

    //
    // If we assigned a port number to this adapter then attempt to delete the
    // symbolic links we created to it.
    //

    if(Adapter->PortNumber != -1) {

        PWCHAR wideNameStrings[] = {L"\\Device\\ScsiPort%d",
                                    L"\\DosDevices\\Scsi%d:"};
        ULONG i;

        for(i = 0; i < (sizeof(wideNameStrings) / sizeof(PWCHAR)); i++) {
            WCHAR wideLinkName[64];
            UNICODE_STRING unicodeLinkName;

            swprintf(wideLinkName, wideNameStrings[i], Adapter->PortNumber);
            RtlInitUnicodeString(&unicodeLinkName, wideLinkName);
            IoDeleteSymbolicLink(&unicodeLinkName);
        }

        Adapter->PortNumber = -1;

        //
        // Decrement the scsiport count.
        //

        IoGetConfigurationInformation()->ScsiPortCount--;
    }

    return;
}


VOID
SpReleaseAdapterResources(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Surprise
    )

/*++

Routine Description:

    This function deletes all of the storage associated with a device
    extension, disconnects from the timers and interrupts and then deletes the
    object.   This function can be called at any time during the initialization.

Arguments:

    Adapter - Supplies a pointer to the device extesnion to be deleted.

Return Value:

    None.

--*/

{

    PCOMMON_EXTENSION commonExtension = &(Adapter->CommonExtension);
    ULONG j;
    PVOID tempPointer;

    PAGED_CODE();

#if DBG

    if(!Surprise) {

        //
        // Free the Remove tracking lookaside list.
        //

        ExDeleteNPagedLookasideList(&(commonExtension->RemoveTrackingLookasideList));
    }
#endif

    //
    // Stop the time and disconnect the interrupt if they have been
    // initialized.  The interrupt object is connected after
    // timer has been initialized, and the interrupt object is connected, but
    // before the timer is started.
    //

    if(Adapter->DeviceObject->Timer != NULL) {
        IoStopTimer(Adapter->DeviceObject);
        KeCancelTimer(&(Adapter->MiniPortTimer));
    }

    if(Adapter->SynchronizeExecution != SpSynchronizeExecution) {

        if (Adapter->InterruptObject) {
            IoDisconnectInterrupt(Adapter->InterruptObject);
        }

        if (Adapter->InterruptObject2) {
            IoDisconnectInterrupt(Adapter->InterruptObject2);
            Adapter->InterruptObject2 = NULL;
        }

        //
        // SpSynchronizeExecution expects to get a pointer to the 
        // adapter extension as the "interrupt" parameter.
        //

        Adapter->InterruptObject = (PVOID) Adapter;
        Adapter->SynchronizeExecution = SpSynchronizeExecution;
    }

    //
    // Delete the miniport's device extension
    //

    if (Adapter->HwDeviceExtension != NULL) {

        PHW_DEVICE_EXTENSION devExt =
            CONTAINING_RECORD(Adapter->HwDeviceExtension,
                              HW_DEVICE_EXTENSION,
                              HwDeviceExtension);

        ExFreePool(devExt);
        Adapter->HwDeviceExtension = NULL;
    }

    //
    // Free the configuration information structure.
    //

    if (Adapter->PortConfig) {
        ExFreePool(Adapter->PortConfig);
        Adapter->PortConfig = NULL;
    }

    //
    // Deallocate SCSIPORT WMI REGINFO information, if any.
    //

    SpWmiDestroySpRegInfo(Adapter->DeviceObject);

    //
    // Free the common buffer.
    //

    if (Adapter->SrbExtensionBuffer != NULL &&
        Adapter->CommonBufferSize != 0) {

        if (Adapter->DmaAdapterObject == NULL) {

            //
            // Since there is no adapter just free the non-paged pool.
            //

            ExFreePool(Adapter->SrbExtensionBuffer);

        } else {

#if defined(_X86_)
            if(Adapter->UncachedExtensionIsCommonBuffer == FALSE) {
                MmFreeContiguousMemorySpecifyCache(Adapter->SrbExtensionBuffer,
                                                   Adapter->CommonBufferSize,
                                                   MmNonCached);
            } else 
#endif
            {

                HalFreeCommonBuffer(
                    Adapter->DmaAdapterObject,
                    Adapter->CommonBufferSize,
                    Adapter->PhysicalCommonBuffer,
                    Adapter->SrbExtensionBuffer,
                    FALSE);
            }

        }
        Adapter->SrbExtensionBuffer = NULL;
    }

    //
    // Free the SRB data array.
    //

    if (Adapter->SrbDataListInitialized) {

        if(Adapter->EmergencySrbData != NULL) {

            ExFreeToNPagedLookasideList(
                &Adapter->SrbDataLookasideList,
                Adapter->EmergencySrbData);
            Adapter->EmergencySrbData = NULL;
        }

        ExDeleteNPagedLookasideList(&Adapter->SrbDataLookasideList);

        Adapter->SrbDataListInitialized = FALSE;
    }

    if (Adapter->InquiryBuffer != NULL) {
        ExFreePool(Adapter->InquiryBuffer);
        Adapter->InquiryBuffer = NULL;
    }

    if (Adapter->InquirySenseBuffer != NULL) {
        ExFreePool(Adapter->InquirySenseBuffer);
        Adapter->InquirySenseBuffer = NULL;
    }

    if (Adapter->InquiryIrp != NULL) {
        IoFreeIrp(Adapter->InquiryIrp);
        Adapter->InquiryIrp = NULL;
    }

    if (Adapter->InquiryMdl != NULL) {
        IoFreeMdl(Adapter->InquiryMdl);
        Adapter->InquiryMdl = NULL;
    }

    //
    // Free the Scatter Gather lookaside list.
    //

    if (Adapter->MediumScatterGatherListInitialized) {

        ExDeleteNPagedLookasideList(
            &Adapter->MediumScatterGatherLookasideList);

        Adapter->MediumScatterGatherListInitialized = FALSE;
    }

    //
    // Unmap any mapped areas.
    //

    SpReleaseMappedAddresses(Adapter);

    //
    // If we've got any resource lists allocated still we should free them
    // now.
    //

    if(Adapter->AllocatedResources != NULL) {
        ExFreePool(Adapter->AllocatedResources);
        Adapter->AllocatedResources = NULL;
    }

    if(Adapter->TranslatedResources != NULL) {
        ExFreePool(Adapter->TranslatedResources);
        Adapter->TranslatedResources = NULL;
    }

    Adapter->CommonExtension.IsInitialized = FALSE;

    return;
}


VOID
SpReapChildren(
    IN PADAPTER_EXTENSION Adapter
    )
{
    ULONG j;

    PAGED_CODE();

    //
    // Run through the logical unit bins and remove any child devices which
    // remain.
    //

    for(j = 0; j < NUMBER_LOGICAL_UNIT_BINS; j++) {

        while(Adapter->LogicalUnitList[j].List != NULL) {

            PLOGICAL_UNIT_EXTENSION lun =
                Adapter->LogicalUnitList[j].List;

            lun->IsMissing = TRUE;
            lun->IsEnumerated = FALSE;

            SpRemoveLogicalUnit(lun->DeviceObject, IRP_MN_REMOVE_DEVICE);
        }
    }
    return;
}


VOID
SpTerminateAdapter(
    IN PADAPTER_EXTENSION Adapter
    )
/*++

Routine Description:

    This routine will terminate the miniport's control of the adapter.  It
    does not cleanly shutdown the miniport and should only be called when
    scsiport is notified that the adapter has been surprise removed.

    This works by synchronizing with the miniport and setting flags to
    disable any new calls into the miniport.  Once this has been done it can
    run through and complete any i/o requests which may still be inside
    the miniport.

Arguments:

    Adapter - the adapter to terminate.

Return Value:

    none

--*/

{
    KIRQL oldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

    KeAcquireSpinLockAtDpcLevel(&(Adapter->SpinLock));

    if(Adapter->CommonExtension.PreviousPnpState == IRP_MN_START_DEVICE) {

        //
        // TA synchronized will stop all calls into the miniport and complete
        // all active requests.
        //

        Adapter->SynchronizeExecution(Adapter->InterruptObject,
                                      SpTerminateAdapterSynchronized,
                                      Adapter);

        Adapter->CommonExtension.PreviousPnpState = 0xff;

        KeReleaseSpinLockFromDpcLevel(&(Adapter->SpinLock));

        //
        // Stop the miniport timer
        //

        KeCancelTimer(&(Adapter->MiniPortTimer));

        //
        // We keep the device object timer running so that any held, busy or
        // otherwise deferred requests will have a chance to get flushed out.
        // We can give the whole process a boost by setting the adapter timeout
        // counter to 1 (it will go to zero in the tick handler) and running
        // the tick handler by hand here.
        //

        // IoStopTimer(Adapter->DeviceObject);
        Adapter->PortTimeoutCounter = 1;

        ScsiPortTickHandler(Adapter->DeviceObject, NULL);

    } else {
        KeReleaseSpinLockFromDpcLevel(&(Adapter->SpinLock));
    }

    KeLowerIrql(oldIrql);

    return;
}


BOOLEAN
SpTerminateAdapterSynchronized(
    IN PADAPTER_EXTENSION Adapter
    )
{
    //
    // Disable the interrupt from coming in.
    //

    SET_FLAG(Adapter->InterruptData.InterruptFlags, PD_ADAPTER_REMOVED);
    CLEAR_FLAG(Adapter->InterruptData.InterruptFlags, PD_RESET_HOLD);

    ScsiPortCompleteRequest(Adapter->HwDeviceExtension,
                            0xff,
                            0xff,
                            0xff,
                            SRB_STATUS_NO_HBA);

    //
    // Run the completion DPC.
    //

    if(TEST_FLAG(Adapter->InterruptData.InterruptFlags,
                 PD_NOTIFICATION_REQUIRED)) {
        SpRequestCompletionDpc(Adapter->DeviceObject);
    }

    return TRUE;
}

BOOLEAN
SpRemoveAdapterSynchronized(
    PADAPTER_EXTENSION Adapter
    )
{
    //
    // Disable the interrupt from coming in.
    //

    SET_FLAG(Adapter->InterruptData.InterruptFlags, PD_ADAPTER_REMOVED);
    CLEAR_FLAG(Adapter->InterruptData.InterruptFlags, PD_RESET_HOLD);

    return TRUE;
}

#endif // __INTERRUPT__

