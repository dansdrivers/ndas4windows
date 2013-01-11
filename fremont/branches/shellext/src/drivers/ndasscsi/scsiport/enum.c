/*++

Copyright (C) Microsoft Corporation, 1996 - 1999

Module Name:

    enum.c

Abstract:

    This module contains device enumeration code for the scsi port driver

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
#define __MODULE__ "NDSC_Enum"

#ifdef __INTERRUPT__

#if DBG
static const char *__file__ = __FILE__;
#endif

#define MINIMUM_BUS_SCAN_INTERVAL ((ULONGLONG) (30 * SECONDS))

ULONG RescanStartTarget = -1;
ULONG RescanEndTarget = -1;
ULONG BreakOnTarget = (ULONG) -1;

ULONG DisconnectedPortId = -1;

NTSTATUS
SpInquireDevice(
    IN PDEVICE_OBJECT FunctionalDeviceObject,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN BOOLEAN ExposeDisconnectedLuns,
    OUT PDEVICE_OBJECT *LogicalUnit,
    OUT PBOOLEAN CheckNextLun
    );

BOOLEAN
SpRemoveLogicalUnitFromBinSynchronized(
    IN PVOID ServiceContext                 // PLOGICAL_UNIT_EXTENSION
    );

BOOLEAN
SpAddLogicalUnitToBinSynchronized(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnitExtension
    );

ULONG
SpCountLogicalUnits(
    IN PADAPTER_EXTENSION Adapter
    );

NTSTATUS
IssueReportLuns(
    IN PDEVICE_OBJECT LogicalUnit,
    OUT PLUN_LIST *LunList
    );

PLUN_LIST
AdjustReportLuns(
    IN PLUN_LIST RawList
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SpEnumerateAdapter)
#pragma alloc_text(PAGE, SpInquireDevice)
#pragma alloc_text(PAGE, SpExtractDeviceRelations)

#pragma alloc_text(PAGELOCK, SpCountLogicalUnits)
#pragma alloc_text(PAGELOCK, GetNextLuRequestWithoutLock)
#pragma alloc_text(PAGELOCK, IssueReportLuns)

#pragma alloc_text(PAGE, SpGetInquiryData)
#pragma alloc_text(PAGE, IssueInquiry)

#pragma alloc_text(PAGE, AdjustReportLuns)

LONG SpPAGELOCKLockCount = 0;
#endif

NTSTATUS
SpEnumerateAdapter(
    IN PDEVICE_OBJECT Adapter,
    IN BOOLEAN Force
    )

/*++

Routine Description:

    This routine enumerates the scsi devices attached to the adapter.  It
    requests inquiry data from the underlying driver then walks through it
    to create and destroy device objects as necessary.

    This routine is very much non-reenterant and should not be called outside
    of the enumeration mutex (ie. outside of an enumeration request).

Arguments:

    Adapter - a pointer to the functional device object being enumerated.

    Alertable - indicates whether the synchronization wait should be cancelable.

    Force - indicates whether the rescan must occur, or if it can be ignored
            because one has been done recently enough.

Return Value:

    STATUS_SUCCESS

--*/

{
    PSCSIPORT_DRIVER_EXTENSION driverExtension;
    PADAPTER_EXTENSION adapterExtension = Adapter->DeviceExtension;

    NTSTATUS status = STATUS_SUCCESS;

    UCHAR i;
    UCHAR pathId;
    UCHAR lun;

    ULONG forceNext;
    BOOLEAN scanDisconnectedDevices = FALSE;

#ifdef ALLOC_PRAGMA
    PVOID sectionHandle;
#endif

    PAGED_CODE();

    status = KeWaitForSingleObject(
                &adapterExtension->EnumerationSynchronization,
                UserRequest,
                UserMode,
                FALSE,
                NULL);

    //
    // If we were woken up by a user-mode APC then it's because the thread is 
    // being terminated (termination will make a thread alertable even though
    // we specify that it's not) ... abort the reenumeration attempt and let 
    // this thread die.
    //

    if(status == STATUS_USER_APC) {
        status = STATUS_REQUEST_ABORTED;
    }

    if(!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Swap out the ForceNextBusScan value for FALSE.
    //

    forceNext = InterlockedExchange(&(adapterExtension->ForceNextBusScan),
                                    FALSE);

    //
    // Force the bus scan to happen either way.
    //

    Force |= forceNext;

    if(Force == FALSE) {

        //
        // If the last bus enumeration was recent enough then don't enumerate
        // again.
        //

        LARGE_INTEGER currentSystemTime;
        LONGLONG lastTime;
        LONGLONG difference;

        KeQuerySystemTime(&currentSystemTime);

        lastTime = adapterExtension->LastBusScanTime.QuadPart;

        difference = currentSystemTime.QuadPart - lastTime;

        if(difference <= MINIMUM_BUS_SCAN_INTERVAL) {

            //
            // Skip this enumeration.
            //

            KeSetEvent(&adapterExtension->EnumerationSynchronization,
                       IO_NO_INCREMENT,
                       FALSE);

            return STATUS_SUCCESS;
        }
    }

    //
    // Lock down the PAGELOCK section - we'll need it in order to call
    // IssueInquiry.
    //

#ifdef ALLOC_PRAGMA
    sectionHandle = MmLockPagableCodeSection(GetNextLuRequestWithoutLock);
    InterlockedIncrement(&SpPAGELOCKLockCount);
#endif

    //
    // Check to see if we should be exposing disconnected LUNs.
    //

    for(i = 0; i < 3; i++) {
        
        PWCHAR locations[] = {
            L"Scsiport",
            SCSIPORT_CONTROL_KEY,
            DISK_SERVICE_KEY
        };

        UNICODE_STRING unicodeString;
        OBJECT_ATTRIBUTES objectAttributes;
        HANDLE instanceHandle = NULL;
        HANDLE handle;
        PKEY_VALUE_FULL_INFORMATION key = NULL;

        if(i == 0) {
            status = IoOpenDeviceRegistryKey(adapterExtension->LowerPdo,
                                             PLUGPLAY_REGKEY_DEVICE,
                                             KEY_READ,
                                             &instanceHandle);

            if(!NT_SUCCESS(status)) {
                DebugPrint((2, "SpEnumerateAdapter: Error %#08lx opening device registry key\n", status));
                continue;
            }
        }

        RtlInitUnicodeString(&unicodeString, locations[i]);

        InitializeObjectAttributes(
            &objectAttributes,
            &unicodeString,
            OBJ_CASE_INSENSITIVE,
            instanceHandle,
            NULL);

        status = ZwOpenKey(&handle,
                           KEY_READ,
                           &objectAttributes);

        if(!NT_SUCCESS(status)) {
            DebugPrint((2, "SpEnumerateAdapter: Error %#08lx opening %wZ key\n", status, &unicodeString));
            if(instanceHandle != NULL) {
                ZwClose(instanceHandle);
                instanceHandle = NULL;
            }
            continue;
        }

        status = SpGetRegistryValue(handle,
                                    L"ScanDisconnectedDevices",
                                    &key);

        ZwClose(handle);
        if(instanceHandle != NULL) {
            ZwClose(instanceHandle);
            instanceHandle = NULL;
        }

        if(NT_SUCCESS(status)) {
            if(key->Type == REG_DWORD) {
                PULONG value;
                value = (PULONG) ((PUCHAR) key + key->DataOffset);
                if(*value) {
                    scanDisconnectedDevices = TRUE;
                }
            }
            ExFreePool(key);
            break;
        } else {
            DebugPrint((2, "SpEnumerateAdapter: Error %#08lx opening %wZ\\ScanDisconnectedDevices value\n", status, &unicodeString));
        }
    }

    //
    // We need to be powered up in order to do a bus enumeration - make
    // sure that we are.  This is because we create new PDO's and new
    // PDO's are assumed to be at D0.
    //

    status = SpRequestValidAdapterPowerStateSynchronous(adapterExtension);

    if(NT_SUCCESS(status)) {

        driverExtension = IoGetDriverObjectExtension(
                                Adapter->DriverObject,
                                ScsiPortInitialize);

        for (pathId = 0;
             pathId < adapterExtension->NumberOfBuses;
             pathId++) {

            ULONG targetIndex;
            UCHAR startingTarget = 0;
            UCHAR endingTarget = adapterExtension->MaximumTargetIds;

            if(RescanStartTarget != -1) {
                startingTarget = (UCHAR) RescanStartTarget;
            }

            if(RescanEndTarget != -1) {
                endingTarget = (UCHAR) RescanEndTarget;
            }

            targetIndex = startingTarget;
            while (1) {

                UCHAR targetId;
                BOOLEAN sparseLun;
                UCHAR maxLuCount;
                ULONG lunListIndex;
                PLUN_LIST lunList;
                PULONGLONG nextLunEntry;
                ULONG numLunsReported;

                //
                // pick the next target id to scan
                //
                if (!(targetIndex < endingTarget)) {
                    break;
                }

                if(adapterExtension->Capabilities.AdapterScansDown) {

                    targetId = (UCHAR) ((endingTarget - 1) -
                               targetIndex);
                } else {

                    targetId = (UCHAR) targetIndex;
                }
                targetIndex++;

                ASSERT(targetId != 255);

                //
                // BUGBUG - we shouldn't just skip the adapter here - make it
                // fall out of the inquiry routine.
                //

                ASSERT(adapterExtension->PortConfig);
                if(targetId == adapterExtension->PortConfig->InitiatorBusId[pathId]) {
                    continue;
                }

                sparseLun = FALSE;

                lun = 0;
                lunList = NULL;
                nextLunEntry = NULL;
                lunListIndex = 0;
                maxLuCount = adapterExtension->MaxLuCount - 1;
                while (1) {

                    PDEVICE_OBJECT logicalUnit;
                    BOOLEAN checkNextLun = TRUE;

                    //
                    // Issue an inquiry to each logical unit in the system.
                    //

                    status = SpInquireDevice(Adapter,
                                             pathId,
                                             targetId,
                                             lun,
                                             scanDisconnectedDevices,
                                             &logicalUnit,
                                             &checkNextLun);

                    if(NT_SUCCESS(status)) {

                        PLOGICAL_UNIT_EXTENSION luExtension =
                            logicalUnit->DeviceExtension;

                        //
                        // See if we should only be processing the first lun.
                        //

                        if((lun == 0) && 
                           (luExtension->InquiryData.HiSupport ||
                            luExtension->SpecialFlags.LargeLuns)) {

                            if(luExtension->SpecialFlags.OneLun) {

                                DebugPrint((1, "SpEnumerateAdapter: target is "
                                               "listed as having only one lun\n"));
                                break;

                            } else if (luExtension->SpecialFlags.SparseLun) {

                                sparseLun = TRUE;
                            }

                            if (NT_SUCCESS(IssueReportLuns(logicalUnit, &lunList))) {

                                numLunsReported  = lunList->LunListLength[3] <<  0;
                                numLunsReported |= lunList->LunListLength[2] <<  8;
                                numLunsReported |= lunList->LunListLength[1] << 16;
                                numLunsReported |= lunList->LunListLength[0] << 24;
                                numLunsReported /= sizeof (lunList->Lun[0]);

                                nextLunEntry = (PULONGLONG) lunList->Lun;

                                sparseLun = TRUE;

                                maxLuCount = SCSI_MAXIMUM_LUNS_PER_TARGET;
                            }
                        }

                        luExtension->SpecialFlags.SparseLun = sparseLun;

                    } else {

                        DebugPrint((1, "SpEnumerateAdapter: SpInquireDevice(%#p, "
                                       "[%#x, %#x, %#x]) returned %#08lx\n",
                                       Adapter,
                                       pathId,
                                       targetId,
                                       lun,
                                       status));

                        if((sparseLun == FALSE)&&(checkNextLun == FALSE)) {
                            break;
                        }

                        if(sparseLun) {
                            DebugPrint((1, "SpEnumerateAdapter: target is to be "
                                           "scanned for sparse luns, continuing\n"));
                        }
                    }

                    //
                    // pick the next lun the scan
                    //
                    if (nextLunEntry) {

                        //
                        // set it to a bad lun #
                        //

                        lun = maxLuCount;

                        //
                        // look for the next good lun
                        //
                        while (lunListIndex < numLunsReported) {       // loop until we run out

                            USHORT nextLun;

                            nextLun  = lunList->Lun[lunListIndex][1] << 0;
                            nextLun |= lunList->Lun[lunListIndex][0] << 8;
                            nextLun &= 0x3fff;

                            lunListIndex++;

                            if ((nextLun >= maxLuCount) ||
                                (nextLun == 0)) {


                                //
                                // we already got lun 0, skip all reported lun 0.
                                //
                                // also, skip all lun numbers that are too large
                                //
                                continue;
                            }

                            //
                            // got a good lun
                            //
                            lun = (UCHAR) nextLun;
                            break;
                        }

                        if ((lun >= maxLuCount) ||
                            (lun == 0)) {

                            //
                            // no more lun
                            //
                            break;
                        }

                    } else {

                        if (lun < maxLuCount) {

                            lun++;
                        } else {

                            //
                            // no more lun
                            //
                            break;
                        }
                    }
                }

                if (lunList) {

                    ExFreePool (lunList);
                }
            }
        }

        status = STATUS_SUCCESS;
    }

#ifdef ALLOC_PRAGMA
    InterlockedDecrement(&SpPAGELOCKLockCount);
    MmUnlockPagableImageSection(sectionHandle);
#endif

    KeQuerySystemTime(&(adapterExtension->LastBusScanTime));

    KeSetEvent(&adapterExtension->EnumerationSynchronization,
               IO_DISK_INCREMENT,
               FALSE);

    return STATUS_SUCCESS;
}


NTSTATUS
SpInquireDevice(
    IN PDEVICE_OBJECT FunctionalDeviceObject,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN BOOLEAN ExposeDisconnectedLuns,
    OUT PDEVICE_OBJECT *LogicalUnit,
    OUT PBOOLEAN CheckNextLun
    )

/*++

Routine Description:

    This routine will issue an inquiry to the logical unit at the specified
    address.  If there is not already a device object allocated for that
    logical unit, it will create one.  If it turns out the device does not
    exist, the logical unit can be destroyed before returning.

    If the logical unit exists, this routine will clear the PD_RESCAN_ACTIVE
    flag in the LuFlags to indicate that the unit is safe.

    If it does not respond, the IsMissing flag will be set to indicate that the
    unit should not be reported during enumeration.  If the IsRemoved flag has
    already been set on the logical unit extension, the device object will be
    destroyed.  Otherwise the device object will not be destroyed until a
    remove can be issued.

Arguments:

    FunctionalDeviceObject - the adapter which this device would exist on

    DeviceAddress - the address of the device we are to inquire.

Return Value:

    STATUS_NO_SUCH_DEVICE if the device does not exist.

    STATUS_SUCCESS if the device does exist.

    error description otherwise.

--*/

{
    PADAPTER_EXTENSION fdoExtension = FunctionalDeviceObject->DeviceExtension;

    PDEVICE_OBJECT logicalUnit;
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension;

    INQUIRYDATA inquiryData;

    BOOLEAN newDevice = FALSE;
    BOOLEAN deviceMismatch = FALSE;

    ULONG bytesReturned;

    NTSTATUS status;

    *LogicalUnit = NULL;
    *CheckNextLun = TRUE;

    PAGED_CODE();

    ASSERT(TargetId != BreakOnTarget);

    //
    // Find or create the device object for this address.
    //

    logicalUnitExtension = GetLogicalUnitExtension(fdoExtension,
                                                   PathId,
                                                   TargetId,
                                                   Lun,
                                                   SpInquireDevice,
                                                   TRUE);

    if(logicalUnitExtension == NULL) {

        //
        // We'll need to (temporarily) create a new device object for this
        // logical unit.  The creation routine will have marked it as a
        // RESCAN_ACTIVE device.
        //

        status = SpCreateLogicalUnit(FunctionalDeviceObject,
                                     PathId,
                                     TargetId,
                                     Lun,
                                     &logicalUnit);

        if(!NT_SUCCESS(status)) {
            return status;
        }

        logicalUnitExtension = logicalUnit->DeviceExtension;

        //
        // Acquire the lock for this device so it doesn't drop to zero
        // until we're ready.
        //

        SpAcquireRemoveLock(logicalUnit, SpInquireDevice);

        //
        // Acquire the single lock which is held until a remove request comes
        // down
        //

        SpAcquireRemoveLock(logicalUnit, UIntToPtr( 0xabcdabcd ) );

        newDevice = TRUE;

    } else {

        logicalUnit = logicalUnitExtension->CommonExtension.DeviceObject;

    }

    if(logicalUnitExtension->IsMissing) {

        DebugPrint((1, "SpInquireDevice: logical unit @ (%d,%d,%d) (%#p) is "
                       "marked as missing and will not be rescanned\n",
                       PathId, TargetId, Lun,
                       logicalUnitExtension->CommonExtension.DeviceObject));

        SpReleaseRemoveLock(logicalUnit, SpInquireDevice);

        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Issue an inquiry to the potential logical unit.
    //

    DebugPrint((2, "SpInquireTarget: Try %s device @ Bus %d, Target %d, "
                   "Lun %d\n",
                   (newDevice ? "new" : "existing"),
                   PathId,
                   TargetId,
                   Lun));

    status = IssueInquiry(logicalUnit, &inquiryData, &bytesReturned);

    if(NT_SUCCESS(status)) {

        UCHAR qualifier;
        BOOLEAN present = FALSE;

        //
        // Check in the registry for special device flags for this lun.
        // If this is disconnected then set the qualifier to be 0 so that we 
        // use the normal hardware ids instead of the "disconnected" ones.
        //

        qualifier = inquiryData.DeviceTypeQualifier;

        SpCheckSpecialDeviceFlags(logicalUnitExtension, &(inquiryData));

        //
        // The inquiry was successful.  Determine whether a device is present.
        //

        switch(qualifier) {
            case DEVICE_QUALIFIER_ACTIVE: {

                //
                // Active devices are always present.
                //

                present = TRUE;
                break;
            }

            case DEVICE_QUALIFIER_NOT_ACTIVE: {

                //
                // If we are to expose disconnected luns then inactive devices
                // are present.
                //

                if(ExposeDisconnectedLuns == TRUE) {
                    present = TRUE;
                    break;
                }

                //
                // If we're using REPORT_LUNS commands for LUN 0 of a target
                // then we should always indicate that LUN 0 is present.
                //

                if((Lun == 0) &&
                   ((inquiryData.HiSupport == TRUE) ||
                    (logicalUnitExtension->SpecialFlags.LargeLuns == TRUE))) {
                    present = TRUE;
                    break;
                }

                break;
            }

            case DEVICE_QUALIFIER_NOT_SUPPORTED: {
                present = FALSE;
                break;
            }

            default: {
                present = TRUE;
                break;
            }
        };

        if(present == FALSE) {

            //
            // setup an error value so we'll clean up the logical unit.
            //

            status =  STATUS_NO_SUCH_DEVICE;

        } else if(newDevice == FALSE) {

            //
            // Verify that the inquiry data hasn't changed since the last time
            // we did a rescan.  Ignore the device type qualifier in this 
            // check.
            //

            deviceMismatch = FALSE;

            if(inquiryData.DeviceType != 
               logicalUnitExtension->InquiryData.DeviceType) {

                DebugPrint((1, "SpInquireTarget: Found different type of "
                               "device @ (%d,%d,%d)\n", 
                            PathId,
                            TargetId,
                            Lun));

                deviceMismatch = TRUE;
                status = STATUS_NO_SUCH_DEVICE;

            } else if(inquiryData.DeviceTypeQualifier != 
                      logicalUnitExtension->InquiryData.DeviceTypeQualifier) {

                DebugPrint((1, "SpInquireDevice: Device @ (%d,%d,%d) type "
                               "qualifier was %d is now %d\n", 
                            PathId,
                            TargetId,
                            Lun,
                            logicalUnitExtension->InquiryData.DeviceTypeQualifier,
                            inquiryData.DeviceTypeQualifier
                            ));

                //
                // If the device was offline but no longer is then we 
                // treat this as a device mismatch.  If the device has gone 
                // offline then we pretend it's the same device.
                // 
                // the goal is to provide PNP with a new device object when 
                // bringing a device online, but to reuse the same device 
                // object when bringing the device offline.
                //

                if(logicalUnitExtension->InquiryData.DeviceTypeQualifier == 
                   DEVICE_QUALIFIER_NOT_ACTIVE) {

                    DebugPrint((1, "SpInquireDevice: device mismatch\n"));
                    deviceMismatch = TRUE;
                    status = STATUS_NO_SUCH_DEVICE;

                } else {

                    DebugPrint((1, "SpInquireDevice: device went offline\n"));
                    deviceMismatch = FALSE;
                    status = STATUS_SUCCESS;
                }
            } 

            if((deviceMismatch == FALSE) && 
               (RtlEqualMemory(
                    (((PUCHAR) &(inquiryData)) + 1), 
                    (((PUCHAR) &(logicalUnitExtension->InquiryData)) + 1), 
                    (INQUIRYDATABUFFERSIZE - 1)) == FALSE)) {

                //
                // Despite the fact that the device type & qualifier are 
                // compatible a mismatch still occurred.
                //

                deviceMismatch = TRUE;
                status = STATUS_NO_SUCH_DEVICE;

                DebugPrint((1, "SpInquireDevice: Device @ (%d,%d,%d) has "
                               "changed\n", 
                            PathId,
                            TargetId,
                            Lun));
            }

        } else {

            if(inquiryData.RemovableMedia) {
                logicalUnit->Characteristics |= FILE_REMOVABLE_MEDIA;
            }

            DebugPrint((1, "SpInquireTarget: Found new %sDevice at address "
                           "(%d,%d,%d)\n",
                           (inquiryData.RemovableMedia ? "Removable " : ""),
                           PathId,
                           TargetId,
                           Lun));

        }
    } else {
        *CheckNextLun = FALSE;
    }

    if(!NT_SUCCESS(status)) {

        //
        // No SCSI device was found for this PDO.  If the device has been
        // enumerated already then mark it as missing so it won't get reported
        // the next time (and will be removed).  If the device object is
        // shiny and new just go ahead and delete it.
        //

        logicalUnitExtension->IsMissing = TRUE;

        if((newDevice) || (logicalUnitExtension->IsEnumerated == FALSE)) {

            //
            // It's safe to destroy this device object ourself since it's not 
            // a device PNP is aware of.  However we may have outstanding i/o 
            // due to pass-through requests or legacy class driver so we need 
            // to properly wait for all i/o to complete.
            //

            logicalUnitExtension->CommonExtension.CurrentPnpState =
                IRP_MN_REMOVE_DEVICE;

            SpReleaseRemoveLock(logicalUnit, SpInquireDevice);

            //
            // Mark this device temporarily as visible so that 
            // SpRemoveLogicalUnit will do the right thing.  Since the rescan
            // active bit is set the enumeration code won't return this device.
            //

            logicalUnitExtension->IsVisible = TRUE;

            ASSERT(logicalUnitExtension->IsEnumerated == FALSE);
            ASSERT(logicalUnitExtension->IsMissing == TRUE);
            ASSERT(logicalUnitExtension->IsVisible == TRUE);

            SpRemoveLogicalUnit(logicalUnitExtension->DeviceObject,
                                IRP_MN_REMOVE_DEVICE);
            
            if(deviceMismatch) {

                //
                // Call this routine again.  This is the only recursion and 
                // since we've deleted the device object there should be no 
                // cause for a mismatch there.  
                //

                status = SpInquireDevice(FunctionalDeviceObject,
                                         PathId,
                                         TargetId,
                                         Lun,
                                         ExposeDisconnectedLuns,
                                         LogicalUnit,
                                         CheckNextLun);
            }

            return status;

        } else {

            //
            // BUGBUG - freeze and flush the queue.  This way we don't need
            // to deal with handling get next request calls
            //

            //
            // Mark the device as being mismatched so that it's destruction 
            // will cause us to rescan the bus (and pickup the new device).
            //

            if(deviceMismatch) {
                logicalUnitExtension->IsMismatched = TRUE;
            }
        }

    } else {

        logicalUnitExtension->IsMissing = FALSE;

        if(newDevice) {
            RtlCopyMemory(&(logicalUnitExtension->InquiryData),
                          &(inquiryData),
                          bytesReturned);

        } else {

            //
            // Update the state of the device and the device map entry if 
            // necessary.
            //

            if(logicalUnitExtension->InquiryData.DeviceTypeQualifier != 
               inquiryData.DeviceTypeQualifier) {

                logicalUnitExtension->InquiryData.DeviceTypeQualifier = 
                    inquiryData.DeviceTypeQualifier;

                SpUpdateLogicalUnitDeviceMapEntry(logicalUnitExtension);
            }
        }

        if(logicalUnitExtension->InquiryData.DeviceTypeQualifier == 
           DEVICE_QUALIFIER_NOT_ACTIVE) {
            logicalUnitExtension->IsVisible = FALSE;

            //
            // Scsiport won't create a device-map entry for this device since 
            // it's never been exposed to PNP (and definately won't be now).
            // Create one here.  If the init-device routine tries to generate
            // one later on down the road it will deal with this case just fine.
            //

            SpBuildDeviceMapEntry(logicalUnitExtension->DeviceObject);
        } else {
            logicalUnitExtension->IsVisible = TRUE;
        }

        *LogicalUnit = logicalUnitExtension->DeviceObject;

        CLEAR_FLAG(logicalUnitExtension->LuFlags, LU_RESCAN_ACTIVE);
    }

    //
    // Release the temporary lock we grabbed for this operation.
    //

    SpReleaseRemoveLock(logicalUnit, SpInquireDevice);

    return status;
}


NTSTATUS
SpExtractDeviceRelations(
    PDEVICE_OBJECT Fdo,
    DEVICE_RELATION_TYPE RelationType,
    PDEVICE_RELATIONS *DeviceRelations
    )

/*++

Routine Description:

    This routine will allocate a device relations structure and fill in the
    count and object array with referenced object pointers

Arguments:

    Fdo - a pointer to the functional device object being enumerated

    RelationType - what type of relationship is being retrieved

    DeviceRelations - a place to store the relationships

--*/

{
    PADAPTER_EXTENSION fdoExtension = Fdo->DeviceExtension;
    ULONG count = 0;

    ULONG relationsSize;
    PDEVICE_RELATIONS deviceRelations = NULL;

    UCHAR bus, target, lun;
    PLOGICAL_UNIT_EXTENSION luExtension;

    ULONG i;

    NTSTATUS status;

    PAGED_CODE();

    status = KeWaitForSingleObject(
                &fdoExtension->EnumerationSynchronization,
                Executive,
                KernelMode,
                FALSE,
                NULL);

    if(status == STATUS_USER_APC) {
        status = STATUS_REQUEST_ABORTED;
    }

    if(!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Find out how many devices there are
    //

    for(bus = 0; bus < fdoExtension->NumberOfBuses; bus++) {
        for(target = 0; target < fdoExtension->MaximumTargetIds; target++) {
            for(lun = 0; lun < SCSI_MAXIMUM_LUNS_PER_TARGET; lun++) {

                luExtension = GetLogicalUnitExtension(
                                fdoExtension,
                                bus,
                                target,
                                lun,
                                FALSE,
                                TRUE);

                if(luExtension == NULL) {
                    continue;
                }

                if(luExtension->IsMissing) {
                    continue;
                }

                if(luExtension->IsVisible == FALSE) {
                    continue;
                }

                if(luExtension->CommonExtension.IsRemoved >= REMOVE_COMPLETE) {
                    ASSERT(FALSE);
                    continue;
                }

                if(luExtension->LuFlags & LU_RESCAN_ACTIVE) {
                    continue;
                }

                count++;
            }
        }
    }

    //
    // Allocate the structure
    //

    relationsSize = sizeof(DEVICE_RELATIONS) + (count * sizeof(PDEVICE_OBJECT));

    deviceRelations = ExAllocatePoolWithTag(PagedPool,
                                            relationsSize,
                                            SCSIPORT_TAG_DEVICE_RELATIONS);

    if(deviceRelations == NULL) {

        DebugPrint((1, "SpExtractDeviceRelations: unable to allocate "
                       "%d bytes for device relations\n", relationsSize));

        KeSetEvent(&fdoExtension->EnumerationSynchronization,
                   IO_NO_INCREMENT,
                   FALSE);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(deviceRelations, relationsSize);

    i = 0;

    for(bus = 0; bus < fdoExtension->NumberOfBuses; bus++) {
        for(target = 0; target < fdoExtension->MaximumTargetIds; target++) {
            for(lun = 0; lun < SCSI_MAXIMUM_LUNS_PER_TARGET; lun++) {

                luExtension = GetLogicalUnitExtension(
                                fdoExtension,
                                bus,
                                target,
                                lun,
                                FALSE,
                                TRUE);

                if(luExtension == NULL) {

                    continue;

                } else if(luExtension->IsMissing) {

                    DebugPrint((1, "SpExtractDeviceRelations: logical unit "
                                   "(%d,%d,%d) is missing and will not be "
                                   "returned\n",
                                bus, target, lun));

                    luExtension->IsEnumerated = FALSE;
                    continue;

                } else if(luExtension->CommonExtension.IsRemoved >= REMOVE_COMPLETE) {

                    ASSERT(FALSE);
                    luExtension->IsEnumerated = FALSE;
                    continue;

                } else if (TEST_FLAG(luExtension->LuFlags, LU_RESCAN_ACTIVE)) {
                    luExtension->IsEnumerated = FALSE;
                    continue;

                } else if(luExtension->IsVisible == FALSE) {
                    luExtension->IsEnumerated = FALSE;
                    continue;
                }

                status = ObReferenceObjectByPointer(
                            luExtension->CommonExtension.DeviceObject,
                            0,
                            NULL,
                            KernelMode);

                if(!NT_SUCCESS(status)) {

                    DebugPrint((1, "SpFdoExtractDeviceRelations: status %#08lx "
                                   "while referenceing object %#p\n",
                                   status,
                                   deviceRelations->Objects[i]));
                    continue;
                }

                deviceRelations->Objects[i] =
                    luExtension->CommonExtension.DeviceObject;

                i++;
                luExtension->IsEnumerated = TRUE;
            }
        }
    }

    deviceRelations->Count = i;
    *DeviceRelations = deviceRelations;

    KeSetEvent(&fdoExtension->EnumerationSynchronization,
               IO_NO_INCREMENT,
               FALSE);

    return STATUS_SUCCESS;
}


NTSTATUS
IssueReportLuns(
    IN PDEVICE_OBJECT LogicalUnit,
    OUT PLUN_LIST *LunList
    )

/*++

Routine Description:

    Build IRP, SRB and CDB for SCSI REPORT LUNS command.

Arguments:

    LogicalUnit - address of target's device object extension.
    LunList - address of buffer for LUN_LIST information.

Return Value:

    NTSTATUS

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension =
        (PLOGICAL_UNIT_EXTENSION) LogicalUnit->DeviceExtension;

    PMDL mdl;
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    KEVENT event;
    KIRQL currentIrql;
    PLUN_LIST lunListDataBuffer;
    PSENSE_DATA senseInfoBuffer = NULL;
    NTSTATUS status;
    ULONG retryCount = 0;
    ULONG lunListSize;
    ULONG i;

    PAGED_CODE();

    if ((logicalUnitExtension->InquiryData.Versions & 7) < 3) {

        //
        // make sure the device supports scsi3 commands
        // without this check, we may hang some scsi2 devices
        //
// BUG BUG BUG
//        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // start with the minilun of 16 byte for the lun list
    //
    lunListSize = 16;

    status = STATUS_INVALID_DEVICE_REQUEST;

    senseInfoBuffer = 
        logicalUnitExtension->AdapterExtension->InquirySenseBuffer;
    irp = logicalUnitExtension->AdapterExtension->InquiryIrp;
    mdl = NULL;

    KeInitializeEvent(&event,
                      SynchronizationEvent,
                      FALSE);

    //
    // This is a two pass operation - for the first pass we just try to figure
    // out how large the list should be.  On the second pass we'll actually 
    // reallocate the buffer and try to get the entire lun list.
    //
    // BUGBUG - we may want to set an arbitrary limit here so we don't soak 
    // up all of non-paged pool when some device hands us back a buffer filled
    // with 0xff.
    // 

    for (i=0; i<2; i++) {

        //
        // Allocate a cache aligned LUN_LIST structure.  
        //

        lunListDataBuffer = ExAllocatePoolWithTag(NonPagedPoolCacheAligned,
                                                  lunListSize,
                                                  SCSIPORT_TAG_REPORT_LUNS);

        if (lunListDataBuffer == NULL) {
            DebugPrint((1,"IssueReportLuns: Can't allocate report luns data buffer\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        mdl = IoAllocateMdl(lunListDataBuffer,
                            lunListSize,
                            FALSE,
                            FALSE,
                            NULL);

        if(mdl == NULL) {
            DebugPrint((1,"IssueReportLuns: Can't allocate data buffer MDL\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;

        }

        MmBuildMdlForNonPagedPool(mdl);

        //
        // number of retry
        //
        retryCount = 3;
        while (retryCount--) {

            //
            // Build IRP for this request.
            //

            IoInitializeIrp(irp, 
                            IoSizeOfIrp(INQUIRY_STACK_LOCATIONS),
                            INQUIRY_STACK_LOCATIONS);

            irp->MdlAddress = mdl;

            irpStack = IoGetNextIrpStackLocation(irp);

            //
            // Fill in SRB fields.
            //

            RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

            //
            // Mark the minor function to indicate that this is an internal scsiport
            // request and that the start state of the device can be ignored.
            //

            irpStack->MajorFunction = IRP_MJ_SCSI;
            irpStack->MinorFunction = 1;

            irpStack->Parameters.Scsi.Srb = &srb;

            IoSetCompletionRoutine(irp, 
                                   SpSignalCompletion,
                                   &event,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            srb.PathId = logicalUnitExtension->PathId;
            srb.TargetId = logicalUnitExtension->TargetId;
            srb.Lun = logicalUnitExtension->Lun;

            srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
            srb.Length = sizeof(SCSI_REQUEST_BLOCK);

            //
            // Set flags to disable synchronous negociation.
            //

            srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

            srb.SrbStatus = srb.ScsiStatus = 0;

            srb.NextSrb = 0;

            srb.OriginalRequest = irp;

            //
            // Set timeout to 2 seconds.
            //

            srb.TimeOutValue = 4;

            srb.CdbLength = 12;

            //
            // Enable auto request sense.
            //

            srb.SenseInfoBuffer = senseInfoBuffer;
            srb.SenseInfoBufferLength = SENSE_BUFFER_SIZE;

            srb.DataBuffer = MmGetMdlVirtualAddress(irp->MdlAddress);
            srb.DataTransferLength = lunListSize;

            cdb = (PCDB)srb.Cdb;

            //
            // Set CDB operation code.
            //

            cdb->REPORT_LUNS.OperationCode = SCSIOP_REPORT_LUNS;
            cdb->REPORT_LUNS.AllocationLength[0] = (UCHAR) ((lunListSize >> 24) & 0xff);
            cdb->REPORT_LUNS.AllocationLength[1] = (UCHAR) ((lunListSize >> 16) & 0xff);
            cdb->REPORT_LUNS.AllocationLength[2] = (UCHAR) ((lunListSize >>  8) & 0xff);
            cdb->REPORT_LUNS.AllocationLength[3] = (UCHAR) ((lunListSize >>  0) & 0xff);

            //
            // Call port driver to handle this request.
            //

            status = IoCallDriver(LogicalUnit, irp);

            //
            // Wait for request to complete.
            //

            KeWaitForSingleObject(&event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);

            status = irp->IoStatus.Status;

            if (SRB_STATUS(srb.SrbStatus) != SRB_STATUS_SUCCESS) {

                DebugPrint((2,"IssueReportLuns: failed SRB status %x\n",
                    srb.SrbStatus));

                //
                // Unfreeze queue if necessary
                //

                if (srb.SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

                    DebugPrint((3, "IssueInquiry: Unfreeze Queue TID %d\n",
                        srb.TargetId));

                    logicalUnitExtension->LuFlags &= ~LU_QUEUE_FROZEN;

                    KeAcquireSpinLock(
                        &(logicalUnitExtension->AdapterExtension->SpinLock),
                        &currentIrql);

                    GetNextLuRequest(logicalUnitExtension);
                    KeLowerIrql(currentIrql);
                }

                if ((srb.SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&
                     senseInfoBuffer->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST){

                     //
                     // A sense key of illegal request was recieved.  This indicates
                     // that the logical unit number of not valid but there is a
                     // target device out there.
                     //

                     status = STATUS_INVALID_DEVICE_REQUEST;
                     break;

                } else if ((SRB_STATUS(srb.SrbStatus) == SRB_STATUS_SELECTION_TIMEOUT) ||
                           (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_NO_DEVICE)) {

                    //
                    // If the selection times out then give up
                    //
                    status = STATUS_NO_SUCH_DEVICE;
                    break;
                }

                //
                // retry...
                //

            } else {

                status = STATUS_SUCCESS;
                break;
            }
        }

        IoFreeMdl(mdl);

        if (NT_SUCCESS(status)) {

            ULONG listLength;

            listLength  = lunListDataBuffer->LunListLength[3] <<  0;
            listLength |= lunListDataBuffer->LunListLength[2] <<  8;
            listLength |= lunListDataBuffer->LunListLength[1] << 16;
            listLength |= lunListDataBuffer->LunListLength[0] << 24;

            if (lunListSize < (listLength + sizeof (LUN_LIST))) {

                lunListSize = listLength + sizeof (LUN_LIST);

                //
                // try report lun with a bigger buffer
                //

                ExFreePool(lunListDataBuffer);
                lunListDataBuffer = NULL;
                status = STATUS_INVALID_DEVICE_REQUEST;

            } else {

                //
                // lun list is good
                //
                break;
            }
        }
    }

    //
    // Return the lun list
    //

    if(NT_SUCCESS(status)) {
        *LunList = AdjustReportLuns(lunListDataBuffer);

        if(*LunList == NULL) {
            *LunList = lunListDataBuffer;
        } else {
            ExFreePool(lunListDataBuffer);
        }
    } else {
        *LunList = NULL;
        if (lunListDataBuffer) {
            ExFreePool(lunListDataBuffer);
        }
    }

    return status;

} // end IssueReportLuns()


NTSTATUS
IssueInquiry(
    IN PDEVICE_OBJECT LogicalUnit,
    OUT PINQUIRYDATA InquiryData,
    OUT PULONG BytesReturned
    )

/*++

Routine Description:

    Build IRP, SRB and CDB for SCSI INQUIRY command.

    This routine MUST be called while holding the enumeration lock.

Arguments:

    LogicalUnit - address of the logical unit's device object

    InquiryData - the location to store the inquiry data for the LUN.

    BytesReturned - the number of bytes of inquiry data returned.
    

Return Value:

    NTSTATUS

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension =
        (PLOGICAL_UNIT_EXTENSION) LogicalUnit->DeviceExtension;

    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    KEVENT event;
    IO_STATUS_BLOCK ioStatusBlock;
    KIRQL currentIrql;
    PINQUIRYDATA inquiryDataBuffer;
    PSENSE_DATA senseInfoBuffer;
    NTSTATUS status;
    ULONG retryCount = 0;

    PAGED_CODE();

    //
    // Allocate properly aligned INQUIRY buffer.
    //
    //
    // ALLOCATION
    //


    inquiryDataBuffer = logicalUnitExtension->AdapterExtension->InquiryBuffer;
    senseInfoBuffer = logicalUnitExtension->AdapterExtension->InquirySenseBuffer;
    ASSERT(inquiryDataBuffer != NULL);
    ASSERT(senseInfoBuffer != NULL);

inquiryRetry:

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    //
    // Build IRP for this request.
    //

    irp = logicalUnitExtension->AdapterExtension->InquiryIrp;

    IoInitializeIrp(irp, 
                    IoSizeOfIrp(INQUIRY_STACK_LOCATIONS),
                    INQUIRY_STACK_LOCATIONS);

    irp->MdlAddress = logicalUnitExtension->AdapterExtension->InquiryMdl;

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Fill in SRB fields.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Mark the minor function to indicate that this is an internal scsiport
    // request and that the start state of the device can be ignored.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = 1;

    irpStack->Parameters.Scsi.Srb = &srb;

    srb.PathId = logicalUnitExtension->PathId;
    srb.TargetId = logicalUnitExtension->TargetId;
    srb.Lun = logicalUnitExtension->Lun;

    srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    //
    // Set flags to disable synchronous negociation.
    //

    srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    srb.SrbStatus = srb.ScsiStatus = 0;

    srb.NextSrb = 0;

    srb.OriginalRequest = irp;

    //
    // Set timeout to 2 seconds.
    //

    srb.TimeOutValue = 4;

    srb.CdbLength = 6;

    //
    // Enable auto request sense.
    //

    srb.SenseInfoBuffer = senseInfoBuffer;
    srb.SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    srb.DataBuffer = MmGetMdlVirtualAddress(irp->MdlAddress);
    srb.DataTransferLength = INQUIRYDATABUFFERSIZE;

    RtlZeroMemory(srb.DataBuffer, srb.DataTransferLength);

    cdb = (PCDB)srb.Cdb;

    //
    // Set CDB operation code.
    //

    cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;

    //
    // Set CDB LUN.
    //

    cdb->CDB6INQUIRY.LogicalUnitNumber = logicalUnitExtension->Lun;
    cdb->CDB6INQUIRY.Reserved1 = 0;

    //
    // Set allocation length to inquiry data buffer size.
    //

    cdb->CDB6INQUIRY.AllocationLength = INQUIRYDATABUFFERSIZE;

    //
    // Zero reserve field and
    // Set EVPD Page Code to zero.
    // Set Control field to zero.
    // (See SCSI-II Specification.)
    //

    cdb->CDB6INQUIRY.PageCode = 0;
    cdb->CDB6INQUIRY.IReserved = 0;
    cdb->CDB6INQUIRY.Control = 0;

    //
    // Call port driver to handle this request.
    //

    IoSetCompletionRoutine(irp, 
                           SpSignalCompletion,
                           &event, 
                           TRUE,
                           TRUE,
                           TRUE);

    status = IoCallDriver(LogicalUnit, irp);

    //
    // Wait for request to complete.
    //

    KeWaitForSingleObject(&event,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    status = irp->IoStatus.Status;

    if (SRB_STATUS(srb.SrbStatus) != SRB_STATUS_SUCCESS) {

        DebugPrint((2,"IssueInquiry: Inquiry failed SRB status %x\n",
            srb.SrbStatus));

        //
        // Unfreeze queue if necessary
        //

        if (srb.SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

            DebugPrint((3, "IssueInquiry: Unfreeze Queue TID %d\n",
                srb.TargetId));

            logicalUnitExtension->LuFlags &= ~LU_QUEUE_FROZEN;

            GetNextLuRequestWithoutLock(logicalUnitExtension);
        }

        //
        // NOTE: if INQUIRY fails with a data underrun,
        //      indicate success and let the class drivers
        //      determine whether the inquiry information
        //      is useful.
        //

        if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

            //
            // Copy INQUIRY buffer to LUNINFO.
            //

            DebugPrint((1,"IssueInquiry: Data underrun at TID %d\n",
                        logicalUnitExtension->TargetId));

            status = STATUS_SUCCESS;

        } else if ((srb.SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&
             senseInfoBuffer->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST){

             //
             // A sense key of illegal request was recieved.  This indicates
             // that the logical unit number of not valid but there is a
             // target device out there.
             //

             status = STATUS_INVALID_DEVICE_REQUEST;

        } else {
            //
            // If the selection did not time out then retry the request.
            //

            if ((SRB_STATUS(srb.SrbStatus) != SRB_STATUS_SELECTION_TIMEOUT) &&
                (SRB_STATUS(srb.SrbStatus) != SRB_STATUS_NO_DEVICE) &&
                (retryCount++ < INQUIRY_RETRY_COUNT)) {

                DebugPrint((2,"IssueInquiry: Retry %d\n", retryCount));
                goto inquiryRetry;
            }

            status = SpTranslateScsiStatus(&srb);
        }

    } else {

        status = STATUS_SUCCESS;
    }

    //
    // Return the inquiry data for the device if the call was successful.
    // Otherwise cleanup.
    //

    if(NT_SUCCESS(status)) {
        RtlCopyMemory(InquiryData, inquiryDataBuffer, irp->IoStatus.Information);

        if((logicalUnitExtension->AdapterExtension->PortNumber == DisconnectedPortId) && 
           (InquiryData->DeviceTypeQualifier == DEVICE_QUALIFIER_ACTIVE)) {
            InquiryData->DeviceTypeQualifier = DEVICE_QUALIFIER_NOT_ACTIVE;
        }

        *BytesReturned = (ULONG) irp->IoStatus.Information;
    }

    return status;

} // end IssueInquiry()


VOID
GetNextLuRequestWithoutLock(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    )
{
    KIRQL oldIrql;

    PAGED_CODE();
    ASSERT(SpPAGELOCKLockCount != 0);
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
    KeAcquireSpinLockAtDpcLevel(&(LogicalUnit->AdapterExtension->SpinLock));
    GetNextLuRequest(LogicalUnit);
    KeLowerIrql(oldIrql);
    PAGED_CODE();
    return;
}


ULONG
SpCountLogicalUnits(
    IN PADAPTER_EXTENSION Adapter
    )
{
    ULONG numberOfLus = 0;
    PLOGICAL_UNIT_EXTENSION luExtension;
    KIRQL oldIrql;

    ULONG bin;

#ifdef ALLOC_PRAGMA
    PVOID sectionHandle;
#endif
    //
    // Code is paged until locked down.
    //

    PAGED_CODE();

    //
    // Lock this routine down before grabbing the spinlock.
    //

#ifdef ALLOC_PRAGMA
    sectionHandle = MmLockPagableCodeSection(SpCountLogicalUnits);
#endif

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

    for(bin = 0; bin < NUMBER_LOGICAL_UNIT_BINS; bin++) {

        KeAcquireSpinLockAtDpcLevel(&(Adapter->LogicalUnitList[bin].Lock));

        for(luExtension = Adapter->LogicalUnitList[bin].List;
            luExtension != NULL;
            luExtension = luExtension->NextLogicalUnit) {

            if(luExtension->IsMissing == FALSE) {
                numberOfLus++;
            }
        }

        KeReleaseSpinLockFromDpcLevel(&(Adapter->LogicalUnitList[bin].Lock));
    }

    KeLowerIrql(oldIrql);

#ifdef ALLOC_PRAGMA
    MmUnlockPagableImageSection(sectionHandle);
#endif

    return numberOfLus;
}


NTSTATUS
SpGetInquiryData(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PIRP Irp
    )

/*++

Routine Description:

    This functions copies the inquiry data to the system buffer.  The data
    is translate from the port driver's internal format to the user mode
    format.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    Irp - Supplies a pointer to the Irp which made the original request.

Return Value:

    Returns a status indicating the success or failure of the operation.

--*/

{
    PUCHAR bufferStart;
    PIO_STACK_LOCATION irpStack;

    UCHAR bin;
    PLOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PSCSI_INQUIRY_DATA inquiryData;
    ULONG inquiryDataSize;
    ULONG length;
    PLOGICAL_UNIT_INFO lunInfo;
    ULONG numberOfBuses;
    ULONG numberOfLus;
    ULONG j;
    UCHAR pathId;
    UCHAR targetId;
    UCHAR lun;

    NTSTATUS status;

    PAGED_CODE();

    ASSERT_FDO(DeviceExtension->CommonExtension.DeviceObject);

    status = KeWaitForSingleObject(&DeviceExtension->EnumerationSynchronization,
                                   UserRequest,
                                   UserMode,
                                   FALSE,
                                   NULL);

    if(status == STATUS_USER_APC) {
        status = STATUS_REQUEST_ABORTED;
    }

    if(!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        return status;
    }

    DebugPrint((3,"SpGetInquiryData: Enter routine\n"));

    //
    // Get a pointer to the control block.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    bufferStart = Irp->AssociatedIrp.SystemBuffer;

    //
    // Determine the number of SCSI buses and logical units.
    //

    numberOfBuses = DeviceExtension->NumberOfBuses;
    numberOfLus = 0;

    numberOfLus = SpCountLogicalUnits(DeviceExtension);

    //
    // Caculate the size of the logical unit structure and round it to a word
    // alignment.
    //

    inquiryDataSize = ((sizeof(SCSI_INQUIRY_DATA) - 1 + INQUIRYDATABUFFERSIZE +
        sizeof(ULONG) - 1) & ~(sizeof(ULONG) - 1));

    // Based on the number of buses and logical unit, determine the minimum
    // buffer length to hold all of the data.
    //

    length = sizeof(SCSI_ADAPTER_BUS_INFO) +
        (numberOfBuses - 1) * sizeof(SCSI_BUS_DATA);
    length += inquiryDataSize * numberOfLus;

    if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < length) {

        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        KeSetEvent(&DeviceExtension->EnumerationSynchronization,
                   IO_NO_INCREMENT,
                   FALSE);
        return(STATUS_BUFFER_TOO_SMALL);
    }

    //
    // Set the information field.
    //

    Irp->IoStatus.Information = length;

    //
    // Fill in the bus information.
    //

    adapterInfo = (PSCSI_ADAPTER_BUS_INFO) bufferStart;

    adapterInfo->NumberOfBuses = (UCHAR) numberOfBuses;
    inquiryData = (PSCSI_INQUIRY_DATA)(bufferStart +
                                       sizeof(SCSI_ADAPTER_BUS_INFO) +
                                       ((numberOfBuses - 1) *
                                        sizeof(SCSI_BUS_DATA)));

    for (pathId = 0; pathId < numberOfBuses; pathId++) {

        PSCSI_BUS_DATA busData;

        busData = &adapterInfo->BusData[pathId];
        busData->InitiatorBusId = DeviceExtension->PortConfig->InitiatorBusId[pathId];
        busData->NumberOfLogicalUnits = 0;
        busData->InquiryDataOffset = (ULONG)((PUCHAR) inquiryData - bufferStart);

        for(targetId = 0;
            targetId < DeviceExtension->MaximumTargetIds;
            targetId++) {
            for(lun = 0;
                lun < SCSI_MAXIMUM_LUNS_PER_TARGET;
                lun++) {

                PLOGICAL_UNIT_EXTENSION luExtension;

                luExtension = GetLogicalUnitExtension(DeviceExtension,
                                                      pathId,
                                                      targetId,
                                                      lun,
                                                      Irp,
                                                      TRUE);

                if(luExtension == NULL) {
                    continue;
                }


                if((luExtension->IsMissing) ||
                   (luExtension->CommonExtension.IsRemoved)) {

                    SpReleaseRemoveLock(
                        luExtension->CommonExtension.DeviceObject,
                        Irp);

                    continue;
                }

                busData->NumberOfLogicalUnits++;

                DebugPrint((1, "InquiryData for (%d, %d, %d) - ",
                               pathId,
                               targetId,
                               lun));
                DebugPrint((1, "%d units found\n", busData->NumberOfLogicalUnits));

                inquiryData->PathId = pathId;
                inquiryData->TargetId = targetId;
                inquiryData->Lun = lun;
                inquiryData->DeviceClaimed = luExtension->IsClaimed;
                inquiryData->InquiryDataLength = INQUIRYDATABUFFERSIZE;
                inquiryData->NextInquiryDataOffset = (ULONG)((PUCHAR) inquiryData + inquiryDataSize - bufferStart);

                RtlCopyMemory(inquiryData->InquiryData,
                              &(luExtension->InquiryData),
                              INQUIRYDATABUFFERSIZE);

                inquiryData = (PSCSI_INQUIRY_DATA) ((PUCHAR) inquiryData + inquiryDataSize);

                SpReleaseRemoveLock(luExtension->CommonExtension.DeviceObject,
                                    Irp);
            }
        }

        if(busData->NumberOfLogicalUnits == 0) {
            busData->InquiryDataOffset = 0;
        } else {
            ((PSCSI_INQUIRY_DATA) ((PCHAR) inquiryData - inquiryDataSize))->NextInquiryDataOffset = 0;
        }

    }

    Irp->IoStatus.Status = STATUS_SUCCESS;

    KeSetEvent(&DeviceExtension->EnumerationSynchronization,
               IO_NO_INCREMENT,
               FALSE);
    return(STATUS_SUCCESS);
}


VOID
SpAddLogicalUnitToBin (
    IN PADAPTER_EXTENSION AdapterExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnitExtension
    )

/*++

Routine Description:

    This routine will synchronize with any interrupt or miniport routines and
    add the specified logical unit to the appropriate logical unit list.
    The logical unit must not already be in the list.

    This routine acquires the bin spinlock and calls the SynchronizeExecution
    routine.  It cannot be called when the bin spinlock is held or from a
    miniport API.

Arguments:

    AdapterExtension - the adapter to add this logical unit to.

    LogicalUnitExtension - the logical unit to be added.

Return Value:

    none

--*/

{
    UCHAR hash = ADDRESS_TO_HASH(LogicalUnitExtension->PathId,
                                 LogicalUnitExtension->TargetId,
                                 LogicalUnitExtension->Lun);

    PLOGICAL_UNIT_BIN bin = &AdapterExtension->LogicalUnitList[hash];

    PLOGICAL_UNIT_EXTENSION lun;

    KIRQL oldIrql;

    KeAcquireSpinLock(&AdapterExtension->SpinLock, &oldIrql);
    KeAcquireSpinLockAtDpcLevel(&bin->Lock);

    //
    // Run through the list quickly and make sure this lun isn't already there
    //

    lun = bin->List;

    while(lun != NULL) {

        if(lun == LogicalUnitExtension) {
            break;
        }
        lun = lun->NextLogicalUnit;
    }

    ASSERTMSG("Logical Unit already in list: ", lun == NULL);

    ASSERTMSG("Logical Unit not properly initialized: ",
              (LogicalUnitExtension->AdapterExtension == AdapterExtension));

    ASSERTMSG("Logical Unit is already on a list: ",
              LogicalUnitExtension->NextLogicalUnit == NULL);

    LogicalUnitExtension->NextLogicalUnit = bin->List;

    bin->List = LogicalUnitExtension;

    KeReleaseSpinLockFromDpcLevel(&bin->Lock);
    KeReleaseSpinLock(&AdapterExtension->SpinLock, oldIrql);
    return;
}


VOID
SpRemoveLogicalUnitFromBin (
    IN PADAPTER_EXTENSION AdapterExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnitExtension
    )

/*++

Routine Description:

    This routine will synchronize with any interrupt or miniport routines and
    remove the specified logical unit from the appropriate logical unit list.
    The logical unit MUST be in the logical unit list.

    This routine acquires the bin spinlock and calls the SynchronizeExecution
    routine.  It cannot be called when the bin spinlock is held or from
    a miniport exported routine.

Arguments:

    AdapterExtension - The adapter from which to remove this logical unit

    LogicalUnitExtension - the logical unit to be removed

Return Value:

    none

--*/

{
    KIRQL oldIrql;
    PLOGICAL_UNIT_BIN bin =
        &AdapterExtension->LogicalUnitList[ADDRESS_TO_HASH(
                                                LogicalUnitExtension->PathId,
                                                LogicalUnitExtension->TargetId,
                                                LogicalUnitExtension->Lun)];

    KeAcquireSpinLock(&AdapterExtension->SpinLock, &oldIrql);
    KeAcquireSpinLockAtDpcLevel(&bin->Lock);

    AdapterExtension->SynchronizeExecution(
        AdapterExtension->InterruptObject,
        SpRemoveLogicalUnitFromBinSynchronized,
        LogicalUnitExtension
        );

    KeReleaseSpinLockFromDpcLevel(&bin->Lock);
    KeReleaseSpinLock(&AdapterExtension->SpinLock, oldIrql);

    if(LogicalUnitExtension->IsMismatched) {
        DebugPrint((1, "SpRemoveLogicalUnitFromBin: Signalling for rescan "
                       "after removal of mismatched lun %#p\n", 
                    LogicalUnitExtension));
        IoInvalidateDeviceRelations(AdapterExtension->LowerPdo,
                                    BusRelations);
    }
}


BOOLEAN
SpRemoveLogicalUnitFromBinSynchronized(
    IN PVOID ServiceContext
    )

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension =
        (PLOGICAL_UNIT_EXTENSION) ServiceContext;
    PADAPTER_EXTENSION adapterExtension =
        logicalUnitExtension->AdapterExtension;

    UCHAR hash = ADDRESS_TO_HASH(
                    logicalUnitExtension->PathId,
                    logicalUnitExtension->TargetId,
                    logicalUnitExtension->Lun);

    PLOGICAL_UNIT_BIN  bin;

    PLOGICAL_UNIT_EXTENSION *lun;

    ASSERT(hash < NUMBER_LOGICAL_UNIT_BINS);

    adapterExtension->CachedLogicalUnit = NULL;

    bin = &adapterExtension->LogicalUnitList[hash];

    lun = &bin->List;

    while(*lun != NULL) {

        if(*lun == logicalUnitExtension) {

            //
            // Found a match - unlink it from the list.
            //

            *lun = logicalUnitExtension->NextLogicalUnit;
            logicalUnitExtension->NextLogicalUnit = NULL;
            return TRUE;
        }

        lun = &((*lun)->NextLogicalUnit);
    }

    // ASSERTMSG("Lun not found in bin\n", FALSE);

    return TRUE;
}


PLUN_LIST
AdjustReportLuns(
    IN PLUN_LIST RawList
    )
{
    ULONG newLength;
    ULONG numberOfEntries;
    ULONG maxLun = 8;

    PLUN_LIST newList;

    //
    // Derive the length of the list and the number of entries currently in 
    // the list.
    //

    newLength  = RawList->LunListLength[3] <<  0;
    newLength |= RawList->LunListLength[2] <<  8;
    newLength |= RawList->LunListLength[1] << 16;
    newLength |= RawList->LunListLength[0] << 24;

    numberOfEntries = newLength / sizeof (RawList->Lun[0]);

    newLength += sizeof(LUN_LIST);
    newLength += maxLun * sizeof(RawList->Lun[0]);

    //
    // Allocate a list with "maxLun" extra entries in it.  This might waste 
    // some space if we have duplicates but it's easy.
    //
    //
    // ALLOCATION
    //


    newList = ExAllocatePoolWithTag(NonPagedPool, 
                                    newLength,
                                    SCSIPORT_TAG_REPORT_LUNS);

    if(newList == NULL){
        newList = RawList;
    } else {

        UCHAR lunNumber;
        ULONG entry;
        ULONG newEntryCount = 0;

        RtlZeroMemory(newList, newLength);

        //
        // First make a fake entry for each of the luns from 0 to maxLun - 1
        //

        for(lunNumber = 0; lunNumber < maxLun; lunNumber++) {
            newList->Lun[lunNumber][1] = lunNumber;
            newEntryCount++;
        };

        //
        // Now iterate through the entries in the remaining list.  For each 
        // one copy it over iff it's not already a lun 0 -> (maxLun - 1)
        //

        for(entry = 0; entry < numberOfEntries; entry++) {
            USHORT l;

            l = (RawList->Lun[entry][0] << 8);
            l |= RawList->Lun[entry][1];
            l &= 0x3fff;

            if(l >= maxLun) {
                RtlCopyMemory(&(newList->Lun[lunNumber]), 
                              &(RawList->Lun[entry]),
                              sizeof(newList->Lun[0]));
                lunNumber++;
                newEntryCount++;
            }
        }

        //
        // Copy over the reserved bytes for the cases where they aren't all 
        // that reserved.
        //

        RtlCopyMemory(newList->Reserved, 
                      RawList->Reserved, 
                      sizeof(RawList->Reserved));

        //
        // Subtract out the number of duplicate entries we found.
        //

        newLength = newEntryCount * sizeof(RawList->Lun[0]); 

        newList->LunListLength[0] = (UCHAR) ((newLength >> 24) & 0xff);
        newList->LunListLength[1] = (UCHAR) ((newLength >> 16) & 0xff);
        newList->LunListLength[2] = (UCHAR) ((newLength >> 8) & 0xff);
        newList->LunListLength[3] = (UCHAR) ((newLength >> 0) & 0xff);
    }

    return newList;
}

#endif __INTERRUPT__