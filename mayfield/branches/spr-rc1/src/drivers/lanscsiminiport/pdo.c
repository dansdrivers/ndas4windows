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

#define KEEP_COMPLETE_REQUEST

#include "port.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSMP_pdo"

#ifdef __INTERRUPT__

#if DBG
static const char *__file__ = __FILE__;
#endif

LONG SpPowerIdleTimeout = -1;      // use system default

NTSTATUS
SpPagingPathNotificationCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP LowerIrp,
    IN PDEVICE_OBJECT Fdo
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ScsiPortPdoPnp)
#pragma alloc_text(PAGE, ScsiPortPdoCreateClose)
#pragma alloc_text(PAGE, SpCreateLogicalUnit)
#pragma alloc_text(PAGE, ScsiPortStartDevice)
#endif

NTSTATUS
ScsiPortPdoDeviceControl(
    IN PDEVICE_OBJECT Pdo,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine handles device control requests for scsi target devices

Arguments:

    Pdo - a pointer to the physical device object

    Irp - a pointer to the io request packet

Return Value:

    status

--*/

{
    PLOGICAL_UNIT_EXTENSION physicalExtension = Pdo->DeviceExtension;
    PCOMMON_EXTENSION commonExtension = Pdo->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    NTSTATUS status;
    BOOLEAN completeRequest = TRUE;

    ULONG isRemoved;

    isRemoved = SpAcquireRemoveLock(Pdo, Irp);
    if(isRemoved) {

        SpReleaseRemoveLock(Pdo, Irp);
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        SpCompleteRequest(Pdo, Irp, NULL, IO_NO_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    ASSERT(commonExtension->IsPdo);

    Irp->IoStatus.Status = 0;

    switch(ioControlCode) {

        case IOCTL_STORAGE_QUERY_PROPERTY: {

            //
            // Validate the query
            //

            PSTORAGE_PROPERTY_QUERY query = Irp->AssociatedIrp.SystemBuffer;

            if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
               sizeof(STORAGE_PROPERTY_QUERY)) {

                status = STATUS_INVALID_PARAMETER;
                break;
            }

            status = ScsiPortQueryProperty(Pdo, Irp);

            return status;

            break;
        }

        case IOCTL_SCSI_GET_ADDRESS: {

            PSCSI_ADDRESS scsiAddress = Irp->AssociatedIrp.SystemBuffer;

            if(irpStack->Parameters.DeviceIoControl.OutputBufferLength <
               sizeof(SCSI_ADDRESS)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            scsiAddress->Length = sizeof(PSCSI_ADDRESS);
            scsiAddress->PortNumber = (UCHAR) physicalExtension->PortNumber;
            scsiAddress->PathId = physicalExtension->PathId;
            scsiAddress->TargetId = physicalExtension->TargetId;
            scsiAddress->Lun = physicalExtension->Lun;

            Irp->IoStatus.Information = sizeof(SCSI_ADDRESS);
            status = STATUS_SUCCESS;
            break;
        }

        //
        // XXX - need to handle
        //
        // IOCTL_STORAGE_PASS_THROUGH
        // IOCTL_STORAGE_PASS_THROUGH_DIRECT
        //

        case IOCTL_SCSI_PASS_THROUGH:
        case IOCTL_SCSI_PASS_THROUGH_DIRECT: {

            PSCSI_PASS_THROUGH srbControl = Irp->AssociatedIrp.SystemBuffer;

            if(irpStack->Parameters.DeviceIoControl.InputBufferLength <
               sizeof(SCSI_PASS_THROUGH)) {

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            srbControl->PathId = physicalExtension->PathId;
            srbControl->TargetId = physicalExtension->TargetId;
            srbControl->Lun = physicalExtension->Lun;

            //
            // Fall through to the default handler
            //

        }

        default: {

            IoSkipCurrentIrpStackLocation(Irp);
            SpReleaseRemoveLock(Pdo, Irp);
            status = IoCallDriver(commonExtension->LowerDeviceObject, Irp);
            completeRequest = FALSE;
        }

    }

    if(completeRequest) {

        Irp->IoStatus.Status = status;
        SpReleaseRemoveLock(Pdo, Irp);
        SpCompleteRequest(Pdo, Irp, NULL, IO_NO_INCREMENT);
    }
    return status;
}

NTSTATUS
ScsiPortPdoPnp(
    IN PDEVICE_OBJECT LogicalUnit,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine handles pnp-power requests.  Currently it will just be
    successful

Arguments:

    LogicalUnit - pointer to the physical device object
    Irp - pointer to the io request packet

Return Value:

    status

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension = LogicalUnit->DeviceExtension;
    PCOMMON_EXTENSION commonExtension = LogicalUnit->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    static ULONG i = 0;

    NTSTATUS status = STATUS_SUCCESS;

    ULONG isRemoved;

    PAGED_CODE();

    isRemoved = SpAcquireRemoveLock(LogicalUnit, Irp);

#if 0
    if(isRemoved != ) {

        ASSERT(isRemoved != REMOVE_PENDING);

        status = STATUS_DEVICE_DOES_NOT_EXIST;
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
#else
    ASSERT(isRemoved != REMOVE_COMPLETE);
#endif

    switch(irpStack->MinorFunction) {

        case IRP_MN_QUERY_PNP_DEVICE_STATE: {

            //
            // If the device is in the paging path then mark it as
            // not-disableable.
            //

            PPNP_DEVICE_STATE deviceState = 
                (PPNP_DEVICE_STATE) &(Irp->IoStatus.Information);

            DebugPrint((1, "ScsiPortPdoPnp: QUERY_DEVICE_STATE for PDO %#x\n", LogicalUnit));

            if(commonExtension->PagingPathCount != 0) {
                SET_FLAG((*deviceState), PNP_DEVICE_NOT_DISABLEABLE);
                DebugPrint((1, "ScsiPortPdoPnp: QUERY_DEVICE_STATE: %#x - not disableable\n",
                            LogicalUnit));
            }

            Irp->IoStatus.Status = STATUS_SUCCESS;
            SpReleaseRemoveLock(LogicalUnit, Irp);
            SpCompleteRequest(LogicalUnit, Irp, NULL, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }

        case IRP_MN_START_DEVICE: {

            if(commonExtension->CurrentPnpState == IRP_MN_START_DEVICE) {
                Irp->IoStatus.Status = STATUS_SUCCESS;
                break;
            }

            if(commonExtension->IsInitialized == FALSE) {
                status = ScsiPortInitDevice(LogicalUnit);
            }

            if(NT_SUCCESS(status)) {
                commonExtension->IsInitialized = TRUE;
                status = ScsiPortStartDevice(LogicalUnit);
            }

            if(NT_SUCCESS(status)) {
                commonExtension->CurrentPnpState = IRP_MN_START_DEVICE;
                commonExtension->PreviousPnpState = 0xff;
            }

            Irp->IoStatus.Status = status;

            break;
        }

        case IRP_MN_QUERY_ID: {

            UCHAR rawIdString[64] = "UNKNOWN ID TYPE";
            ANSI_STRING ansiIdString;
            UNICODE_STRING unicodeIdString;
            BOOLEAN multiStrings;

            PINQUIRYDATA inquiryData = &(logicalUnitExtension->InquiryData);

            //
            // We've been asked for the id of one of the physical device objects
            //

            DebugPrint((2, "ScsiPortPnp: got IRP_MN_QUERY_ID\n"));

            RtlInitUnicodeString(&unicodeIdString, NULL);
            RtlInitAnsiString(&ansiIdString, NULL);

            switch(irpStack->Parameters.QueryId.IdType) {

                case BusQueryDeviceID: {

                    status = ScsiPortGetDeviceId(LogicalUnit, &unicodeIdString);
                    multiStrings = FALSE;

                    break;
                }

                case BusQueryInstanceID: {

                    status = ScsiPortGetInstanceId(LogicalUnit, &unicodeIdString);
                    multiStrings = FALSE;

                    break;
                }

                case BusQueryHardwareIDs: {

                    status = ScsiPortGetHardwareIds(
                                &(logicalUnitExtension->InquiryData), 
                                &unicodeIdString);
                    multiStrings = TRUE;
                    break;
                }

                case BusQueryCompatibleIDs: {

                    status = ScsiPortGetCompatibleIds( 
                                &(logicalUnitExtension->InquiryData), 
                                &unicodeIdString);
                    multiStrings = TRUE;

                    break;
                }

                default: {

                    status = Irp->IoStatus.Status;
                    Irp->IoStatus.Information = 0;

                    break;

                }
            }

            Irp->IoStatus.Status = status;

            if(NT_SUCCESS(status)) {
            
                PWCHAR idString;
                
                //
                // fix up all invalid characters
                //                 
                idString = unicodeIdString.Buffer;
                while (*idString) {
                
                    if ((*idString <= L' ')  || 
                        (*idString > (WCHAR)0x7F) || 
                        (*idString == L',')) {
                        *idString = L'_';
                    }
                    idString++;
                    
                    if ((*idString == L'\0') && multiStrings) {
                        idString++;
                    }
                }            
            
                Irp->IoStatus.Information = (ULONG_PTR) unicodeIdString.Buffer;
            } else {
                Irp->IoStatus.Information = (ULONG_PTR) NULL;
            }

            SpReleaseRemoveLock(LogicalUnit, Irp);
            SpCompleteRequest(LogicalUnit, Irp, NULL, IO_NO_INCREMENT);

            return status;
            break;
        }

        case IRP_MN_QUERY_RESOURCES:
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS: {

            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = (ULONG_PTR) NULL;
            SpReleaseRemoveLock(LogicalUnit, Irp);
            SpCompleteRequest(LogicalUnit, Irp, NULL, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }

        case IRP_MN_SURPRISE_REMOVAL:
        case IRP_MN_REMOVE_DEVICE: {

            BOOLEAN destroyed;

            //
            // Release the lock for this IRP before going in.
            //

            if(commonExtension->IsRemoved == NO_REMOVE) {
                commonExtension->IsRemoved = REMOVE_PENDING;
            }

            SpReleaseRemoveLock(LogicalUnit, Irp);

            destroyed = SpRemoveLogicalUnit(LogicalUnit, 
                                            irpStack->MinorFunction);

            if(destroyed) {
                commonExtension->PreviousPnpState =
                    commonExtension->CurrentPnpState;
                commonExtension->CurrentPnpState = irpStack->MinorFunction;
            } else {
                commonExtension->CurrentPnpState = 0xff;
                commonExtension->PreviousPnpState = irpStack->MinorFunction;
            }

            status = STATUS_SUCCESS;
            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
        }

        case IRP_MN_QUERY_DEVICE_TEXT: {

            Irp->IoStatus.Status =
                SpQueryDeviceText(
                    LogicalUnit,
                    irpStack->Parameters.QueryDeviceText.DeviceTextType,
                    irpStack->Parameters.QueryDeviceText.LocaleId,
                    (PWSTR *) &Irp->IoStatus.Information
                    );

            break;
        }

        case IRP_MN_QUERY_CAPABILITIES: {

            PDEVICE_CAPABILITIES capabilities =
                irpStack->Parameters.DeviceCapabilities.Capabilities;

            PSCSIPORT_DEVICE_TYPE deviceType = NULL;

            capabilities->RawDeviceOK = 1;

            deviceType = SpGetDeviceTypeInfo(
                            logicalUnitExtension->InquiryData.DeviceType
                            );

            if((deviceType != NULL) && (deviceType->IsStorage)) {
                capabilities->SilentInstall = 1;
            }

            capabilities->Address = logicalUnitExtension->TargetId;

            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }

        case IRP_MN_QUERY_STOP_DEVICE:
        case IRP_MN_QUERY_REMOVE_DEVICE: {

            if ((commonExtension->PagingPathCount != 0) ||
                (logicalUnitExtension->IsLegacyClaim == TRUE)) {
                Irp->IoStatus.Status = STATUS_DEVICE_BUSY;
            } else {
                Irp->IoStatus.Status = STATUS_SUCCESS;

                commonExtension->PreviousPnpState =
                    commonExtension->CurrentPnpState;
                commonExtension->CurrentPnpState = irpStack->MinorFunction;
            }
            break;
        }

        case IRP_MN_CANCEL_STOP_DEVICE: {

            if(commonExtension->CurrentPnpState == IRP_MN_QUERY_STOP_DEVICE) {
                commonExtension->CurrentPnpState =
                    commonExtension->PreviousPnpState;
                commonExtension->PreviousPnpState = 0xff;
            }

            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }

        case IRP_MN_CANCEL_REMOVE_DEVICE: {

            if(commonExtension->CurrentPnpState == IRP_MN_QUERY_REMOVE_DEVICE) {
                commonExtension->CurrentPnpState =
                    commonExtension->PreviousPnpState;
                commonExtension->PreviousPnpState = 0xff;
            }

            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }

        case IRP_MN_STOP_DEVICE: {

            ASSERT(commonExtension->CurrentPnpState == IRP_MN_QUERY_STOP_DEVICE);

            status = ScsiPortStopDevice(LogicalUnit);

            ASSERT(NT_SUCCESS(status));

            Irp->IoStatus.Status = status;
            Irp->IoStatus.Information = (ULONG_PTR) NULL;

            if(NT_SUCCESS(status)) {
                commonExtension->CurrentPnpState = IRP_MN_STOP_DEVICE;
                commonExtension->PreviousPnpState = 0xff;
            }

            SpReleaseRemoveLock(LogicalUnit, Irp);
            SpCompleteRequest(LogicalUnit, Irp, NULL, IO_NO_INCREMENT);

            return status;
        }

        case IRP_MN_QUERY_DEVICE_RELATIONS: {

            PDEVICE_RELATIONS deviceRelations;

            if(irpStack->Parameters.QueryDeviceRelations.Type !=
               TargetDeviceRelation) {

                break;
            }

            //
            // DEVICE_RELATIONS definition contains one object pointer.
            //

            deviceRelations = ExAllocatePoolWithTag(
                                PagedPool,
                                sizeof(DEVICE_RELATIONS),
                                SCSIPORT_TAG_DEVICE_RELATIONS);

            if(deviceRelations == NULL) {

                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            RtlZeroMemory(deviceRelations, sizeof(DEVICE_RELATIONS));

            deviceRelations->Count = 1;
            deviceRelations->Objects[0] = LogicalUnit;

            ObReferenceObject(deviceRelations->Objects[0]);

            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;

            break;
        }

        case IRP_MN_DEVICE_USAGE_NOTIFICATION: {

            PIRP newIrp;
            PIO_STACK_LOCATION nextStack;

            DebugPrint((1, "Pdo - IRP_MN_DEVICE_USAGE_NOTIFICATION %#p received for "
                           "logical unit %#p\n",
                        Irp,
                        LogicalUnit));

            newIrp = IoAllocateIrp(
                        commonExtension->LowerDeviceObject->StackSize,
                        FALSE);

            if(newIrp == NULL) {

                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            newIrp->AssociatedIrp.MasterIrp = Irp;

            newIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;

            nextStack = IoGetNextIrpStackLocation(newIrp);
            *nextStack = *IoGetCurrentIrpStackLocation(Irp);

            IoSetCompletionRoutine(newIrp,
                                   SpPagingPathNotificationCompletion,
                                   commonExtension->LowerDeviceObject,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            status = IoCallDriver(commonExtension->LowerDeviceObject,
                                  newIrp);
            return status;
            break;
        }
    }

    SpReleaseRemoveLock(LogicalUnit, Irp);

    status = Irp->IoStatus.Status;
    SpCompleteRequest(LogicalUnit, Irp, NULL, IO_NO_INCREMENT);

    return status;
}


NTSTATUS
SpPagingPathNotificationCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP LowerIrp,
    IN PDEVICE_OBJECT Fdo
    )
{
    PIRP upperIrp = LowerIrp->AssociatedIrp.MasterIrp;

    PIO_STACK_LOCATION lowerStack = IoGetCurrentIrpStackLocation(LowerIrp);
    PIO_STACK_LOCATION upperStack = IoGetCurrentIrpStackLocation(upperIrp);

    PDEVICE_OBJECT pdo = upperStack->DeviceObject;

    PADAPTER_EXTENSION lowerExtension;
    PLOGICAL_UNIT_EXTENSION upperExtension;

    ASSERT(Fdo != NULL);
    ASSERT(pdo != NULL);

    DebugPrint((1, "Completion - IRP_MN_DEVICE_USAGE_NOTIFICATION: Completion of "
                   "paging notification irp %#p sent due to irp %#p\n",
                LowerIrp, upperIrp));

    lowerExtension = (PADAPTER_EXTENSION) Fdo->DeviceExtension;
    upperExtension = (PLOGICAL_UNIT_EXTENSION) pdo->DeviceExtension;

    ASSERT_FDO(lowerExtension->DeviceObject);
    ASSERT_PDO(upperExtension->DeviceObject);

    DebugPrint((1, "Completion - IRP_MN_DEVICE_USAGE_NOTIFICATION: irp status %#08lx\n",
                LowerIrp->IoStatus.Status));

    if(NT_SUCCESS(LowerIrp->IoStatus.Status)) {

        PUCHAR typeName = "INSERT TYPE HERE";
        PULONG lowerCount;
        PULONG upperCount;

        //
        // The parameters have already been erased from the lower irp stack
        // location - use the parameters from the upper once since they're
        // just a copy.
        //

        switch(upperStack->Parameters.UsageNotification.Type) {

            case DeviceUsageTypePaging: {

                lowerCount = &(lowerExtension->CommonExtension.PagingPathCount);
                upperCount = &(upperExtension->CommonExtension.PagingPathCount);
                typeName = "PagingPathCount";
                break;
            }

            case DeviceUsageTypeHibernation: {

                lowerCount = &(lowerExtension->CommonExtension.HibernatePathCount);
                upperCount = &(upperExtension->CommonExtension.HibernatePathCount);
                typeName = "HibernatePathCount";
                break;
            }

            case DeviceUsageTypeDumpFile: {

                lowerCount = &(lowerExtension->CommonExtension.DumpPathCount);
                upperCount = &(upperExtension->CommonExtension.DumpPathCount);
                typeName = "DumpPathCount";
                break;
            }

            default: {

                typeName = "unknown type";
                lowerCount = upperCount = NULL;
                break;
            }
        }

        if(lowerCount != NULL) {
            IoAdjustPagingPathCount(
                lowerCount,
                upperStack->Parameters.UsageNotification.InPath
                );
            DebugPrint((1, "Completion - IRP_MN_DEVICE_USAGE_NOTIFICATION: "
                           "Fdo %s count - %d\n",
                        typeName, *lowerCount));
            IoInvalidateDeviceState(lowerExtension->LowerPdo);
        }

        if(upperCount != NULL) {
            IoAdjustPagingPathCount(
                upperCount,
                upperStack->Parameters.UsageNotification.InPath
                );
            DebugPrint((1, "Completion - IRP_MN_DEVICE_USAGE_NOTIFICATION: "
                           "Pdo %s count - %d\n",
                        typeName, *upperCount));
            IoInvalidateDeviceState(upperExtension->DeviceObject);
        }
    }

    upperIrp->IoStatus = LowerIrp->IoStatus;

    SpReleaseRemoveLock(lowerExtension->CommonExtension.DeviceObject, LowerIrp);
    SpReleaseRemoveLock(upperExtension->CommonExtension.DeviceObject, upperIrp);

    IoMarkIrpPending(upperIrp);

    SpCompleteRequest(upperExtension->CommonExtension.DeviceObject,
                      upperIrp,
                      NULL,
                      IO_NO_INCREMENT);

    IoFreeIrp(LowerIrp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
ScsiPortPdoCreateClose(
    IN PDEVICE_OBJECT Pdo,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine handles creates and closes for bus device pdo's

Arguments:

    Pdo - a pointer to the physical device object
    Irp - a pointer to the io request packet

Return Value:

    status

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension = Pdo->DeviceExtension;

    ULONG isRemoved;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    ASSERT_PDO(Pdo);

    isRemoved = SpAcquireRemoveLock(Pdo, Irp);

    if(IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE) {

        if(isRemoved) {
            status = STATUS_DEVICE_DOES_NOT_EXIST;
        } else if((logicalUnitExtension->LuFlags | LU_RESCAN_ACTIVE) ==
                    LU_RESCAN_ACTIVE) {

            //
            // This device object hasn't been verified to exist.  Fail the create
            //

            // status = STATUS_DEVICE_NOT_READY;
        }
    }

    Irp->IoStatus.Status = status;
    SpReleaseRemoveLock(Pdo, Irp);
    SpCompleteRequest(Pdo, Irp, NULL, IO_NO_INCREMENT);

    return status;
}

NTSTATUS
ScsiPortPdoScsi(
    IN PDEVICE_OBJECT Pdo,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine dispatches SRB's for a particular target device.  It will fill
    in the Port, Path, Target and Lun values and then forward the request
    through to the FDO for the bus

Arguments:

    Pdo - a pointer to the physical device object
    Irp - a pointer to the io request packet

Return Value:

    status

--*/

{
    PLOGICAL_UNIT_EXTENSION lun = Pdo->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

#if DBG
    PDRIVER_OBJECT lowerDriverObject =
                        lun->CommonExtension.LowerDeviceObject->DriverObject;
#endif

    PSCSI_REQUEST_BLOCK srb = irpStack->Parameters.Scsi.Srb;

    ULONG isRemoved;

    PSRB_DATA srbData;
    BOOLEAN isLock = FALSE;

    NTSTATUS status;

    isRemoved = SpAcquireRemoveLock(Pdo, Irp);

    if(isRemoved &&
       !IS_CLEANUP_REQUEST(irpStack) &&
       (srb->Function != SRB_FUNCTION_CLAIM_DEVICE)) {

        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;

        SpReleaseRemoveLock(Pdo, Irp);
        SpCompleteRequest(Pdo, Irp, NULL, IO_NO_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    srb->PathId = lun->PathId;
    srb->TargetId = lun->TargetId;
    srb->Lun = lun->Lun;

    //
    // NOTICE:  The SCSI-II specification indicates that this field should be
    // zero; however, some target controllers ignore the logical unit number
    // in the INDENTIFY message and only look at the logical unit number field
    // in the CDB.
    //

    srb->Cdb[1] |= srb->Lun << 5;

    //
    // Queue tags should be assigned only by the StartIo routine.  Set it to
    // a benign value here so we can tell later on that we don't have to
    // clear the tag value in the bitmap.
    //

    srb->QueueTag = SP_UNTAGGED;

#if DBG
    ASSERT(lowerDriverObject->MajorFunction[IRP_MJ_SCSI] != NULL);
    ASSERT(lowerDriverObject->MajorFunction[IRP_MJ_SCSI] == ScsiPortGlobalDispatch);
#endif

    switch(srb->Function) {


        case SRB_FUNCTION_ABORT_COMMAND: {

            //
            // BUGBUG - abort is broken.  We'll need to either yank it out
            // or figure out how to recode it internally.  Abort commands
            // are going to need their own SRB_DATA structure since that
            // contains the pointer back to the original irp.
            //

            ASSERT(FALSE);
            status = STATUS_NOT_SUPPORTED;
            break;

        }

        case SRB_FUNCTION_CLAIM_DEVICE:
        case SRB_FUNCTION_REMOVE_DEVICE: {

            status = SpClaimLogicalUnit(
                        lun->CommonExtension.LowerDeviceObject->DeviceExtension,
                        lun,
                        Irp,
                        FALSE);
            break;
        }

        case SRB_FUNCTION_UNLOCK_QUEUE: 
        case SRB_FUNCTION_LOCK_QUEUE: {

            BOOLEAN lock;

            lock = (srb->Function == SRB_FUNCTION_LOCK_QUEUE);

            //
            // This srb function is only valid as part of a power up request
            // and will be ignored if the power state is D0.
            //

            DebugPrint((2, "ScsiPortPdoScsi: called to %s queue %#p\n",
                           lock ? "lock" : "unlock", 
                           lun));

            ASSERT(lun->LockRequest == NULL);

            CLEAR_FLAG(srb->SrbFlags, SRB_FLAGS_QUEUE_ACTION_ENABLE);
            SET_FLAG(srb->SrbFlags, SRB_FLAGS_BYPASS_LOCKED_QUEUE);

            isLock = TRUE;

            //
            // Throw this request down so it gets processed as a real
            // request.  We need to get the completion dpc to start
            // things running again.  there are too many flags to set
            // to do it from here.
            //

            DebugPrint((2, "ScsiPortPdoScsi: %s %#p into "
                           "queue %#p ... issuing request\n",
                        lock ? "lock" : "unlock", srb, lun));

            srbData = SpAllocateBypassSrbData(lun);
            ASSERT(srbData != NULL);

            goto RunSrb;
        }

        case SRB_FUNCTION_RELEASE_QUEUE:
        case SRB_FUNCTION_FLUSH_QUEUE: {

            srbData = SpAllocateBypassSrbData(lun);
            ASSERT(srbData != NULL);

            goto RunSrb;
        }

        default: {

            if(TEST_FLAG(srb->SrbFlags, (SRB_FLAGS_BYPASS_LOCKED_QUEUE |
                                         SRB_FLAGS_BYPASS_FROZEN_QUEUE))) {
                
                srbData = SpAllocateBypassSrbData(lun);
                ASSERT(srbData != NULL);
            } else {
                srbData = SpAllocateSrbData( lun->AdapterExtension, Irp);

                if(srbData == NULL) {
    
                    //
                    // There wasn't an SRB_DATA block available for this
                    // request so it's been queued waiting for resources -
                    // leave the logical unit remove-locked and return pending.
                    //
    
                    DebugPrint((1, "ScsiPortPdoScsi: Insufficient resources "
                                   "to allocate SRB_DATA structure\n"));
                    return STATUS_PENDING;
                }
            }
RunSrb:
            srbData->CurrentIrp = Irp;
            srbData->CurrentSrb = srb;
            srbData->LogicalUnit = lun;

            srb->OriginalRequest = srbData;
            return SpDispatchRequest(lun, Irp);
        }
    }

    Irp->IoStatus.Status = status;
    SpReleaseRemoveLock(Pdo, Irp);
    SpCompleteRequest(Pdo, Irp, NULL, IO_NO_INCREMENT);
    return status;
}


NTSTATUS
SpCreateLogicalUnit(
    IN PDEVICE_OBJECT AdapterFdo,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    OUT PDEVICE_OBJECT *NewPdo
    )

/*++

Routine Description:

    This routine will create a physical device object for the specified device

Arguments:

    AdapterFdo - the FDO this device was enumerated from

    LunInfo - the scsi port lun info structure for this device

    NewPdo - a location to store the pointer to the new device

Return Value:

    status

--*/

{
    PADAPTER_EXTENSION deviceExtension = AdapterFdo->DeviceExtension;

    PIRP senseIrp;

    PDEVICE_OBJECT pdo = NULL;

    WCHAR wideDeviceName[64];
    UNICODE_STRING unicodeDeviceName;

    ULONG extensionSize;

    NTSTATUS status;

    PAGED_CODE();

    //
    // Attempt to allocate all the persistent resources we need before we 
    // try to create the device object itself.
    //

    //
    // Allocate a request sense irp.
    //

    senseIrp = IoAllocateIrp(1, FALSE);

    if(senseIrp == NULL) {
        DebugPrint((0, "SpCreateLogicalUnit: Could not allocate request sense "
                       "irp\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Build the name for the device
    //

    swprintf(wideDeviceName,
             L"%wsPort%xPath%xTarget%xLun%x",
             deviceExtension->DeviceName,
             deviceExtension->PortNumber,
             PathId,
             TargetId,
             Lun);

    RtlInitUnicodeString(&unicodeDeviceName, wideDeviceName);

    //
    // Round the size of the Hardware logical extension to the size of a
    // PVOID and add it to the port driver's logical extension.
    //

    extensionSize =
        (deviceExtension->HwLogicalUnitExtensionSize + sizeof(LONGLONG) - 1) &
        ~(sizeof(LONGLONG) -1);
    extensionSize += sizeof(LOGICAL_UNIT_EXTENSION);

    //
    // Create a physical device object
    //

    status = IoCreateDevice(
                AdapterFdo->DriverObject,
                extensionSize,
                &unicodeDeviceName,
                FILE_DEVICE_MASS_STORAGE,
                FILE_DEVICE_SECURE_OPEN,
                FALSE,
                &pdo
                );

    if(NT_SUCCESS(status)) {

        PCOMMON_EXTENSION commonExtension;
        PLOGICAL_UNIT_EXTENSION logicalUnitExtension;
        UCHAR i;
        ULONG bin;

        UCHAR rawDeviceName[64];
        ANSI_STRING ansiDeviceName;

        //
        // Set the device object's stack size
        //

        //
        // We need one stack location for the PDO to do lock tracking and
        // one stack location to issue scsi request to the FDO.
        //

        pdo->StackSize = 1;

        pdo->Flags |= DO_BUS_ENUMERATED_DEVICE;

        pdo->Flags |= DO_DIRECT_IO;

        pdo->AlignmentRequirement = AdapterFdo->AlignmentRequirement;

        //
        // Initialize the device extension for the root device
        //

        commonExtension = pdo->DeviceExtension;
        logicalUnitExtension = pdo->DeviceExtension;

        RtlZeroMemory(logicalUnitExtension, extensionSize);

        commonExtension->DeviceObject = pdo;
        commonExtension->IsPdo = TRUE;
        commonExtension->LowerDeviceObject = AdapterFdo;
        commonExtension->MajorFunction = DeviceMajorFunctionTable;

        commonExtension->WmiInitialized            = FALSE;
        commonExtension->WmiMiniPortSupport        =
            deviceExtension->CommonExtension.WmiMiniPortSupport;

        commonExtension->WmiScsiPortRegInfoBuf     = NULL;
        commonExtension->WmiScsiPortRegInfoBufSize = 0;

        //FILE0  zC?l      8  X                —           `           H      νσO«EΜ Έ1‘ybΚνσO«EΜάοΞ+tLΜ                    
           ¶    0   p          T     —    νσO«EΜνσO«EΜνσO«EΜνσO«EΜ                        	m e n u b t n . h     €   H                        @                           Aÿvƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  "D?l      8  X                ‘—           `           H      zO«EΜ Έ1‘ybΚzO«EΜΒ;Ο+tLΜ                    
          P¶    0   p          X     —    zO«EΜzO«EΜzO«EΜzO«EΜ                       m s i p r o c . c p p €   H                         @              	      	      Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  D?l      8  X                ’—           `           H      zO«EΜ Έ1‘ybΚzO«EΜ*ΣΟ+tLΜ                    
          θ¶    0   p          T     —    zO«EΜzO«EΜzO«EΜzO«EΜ 0                      	m s i p r o c . h     €   H                        @        0      έ"      έ"      Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  ¬D?l      8  `                “—           `           H      zO«EΜ Έ1‘ybΚzO«EΜ‹Π+tLΜ                    
          ¶    0   x          Z     —    zO«EΜzO«EΜzO«EΜzO«EΜ                       n d a s e t u p . c p p       €   H                         @                          Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              FILE0  
G?l      8  Ψ                —           `           H      -O«EΜ Έ1‘ybΚ-O«EΜ¤Ò+tLΜ                    
          ψ¨¶    0   p          X     —    -O«EΜ-O«EΜ-O«EΜ-O«EΜ                        N D A S E T ~ 3 . R C 0   €          h     —    -O«EΜ-O«EΜ-O«EΜ-O«EΜ                        n d a s e t u p . l o c . e s n . r c €   H                        @               L      L      A	wƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  &G?l      8  Ψ                ™—           `           H      -O«EΜ Έ1‘ybΚ-O«EΜd7Ò+tLΜ                    
           «¶    0   p          X     —    -O«EΜ-O«EΜ-O«EΜ-O«EΜ                        N D A S E T ~ 4 . R C 0   €          h     —    -O«EΜ-O«EΜ-O«EΜ-O«EΜ                        n d a s e t u p . l o c . f r a . r c €   H                        @               ‘      ‘      Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  BG?l      8  Ψ                —           `           H      -O«EΜ Έ1‘ybΚ-O«EΜ^jÒ+tLΜ                    
          ­¶    0   p          X     —    -O«EΜ-O«EΜ-O«EΜ-O«EΜ                        N D C 5 1 4 ~ 1 . R C 0   €          h     —    -O«EΜ-O«EΜ-O«EΜ-O«EΜ                        n d a s e t u p . l o c . i t a . r c €   H                        @               —      —      Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  7H?l      8  Ψ                ›—           `           H      Ξ‡O«EΜ Έ1‘ybΚΞ‡O«EΜΨ¤Ò+tLΜ                    
          ―¶    0   p          X     —    Ξ‡O«EΜΞ‡O«EΜΞ‡O«EΜΞ‡O«EΜ                       N D 4 E B 6 ~ 1 . R C 0   €          h     —    Ξ‡O«EΜΞ‡O«EΜΞ‡O«EΜΞ‡O«EΜ                       n d a s e t u p . l o c . j p n . r c €   H                         @              !      !      Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  )I?l      8  Ψ                —           `           H      Ξ‡O«EΜ Έ1‘ybΚΞ‡O«EΜψΤÒ+tLΜ                    
          8±¶    0   p          X     —    Ξ‡O«EΜΞ‡O«EΜΞ‡O«EΜΞ‡O«EΜ                       N D 6 C 5 5 ~ 1 . R C 0   €          h     —    Ξ‡O«EΜΞ‡O«EΜΞ‡O«EΜΞ‡O«EΜ                       n d a s e t u p . l o c . k o r . r c €   H                         @                          Awƒ   ÿÿÿÿ‚yG                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      FILE0  EI?l      8  Ψ                —           `           H      Ξ‡O«EΜ Έ1‘ybΚΞ‡O«EΜr
Σ+tLΜ                    
          @³¶    0   p          X     —    Ξ‡O«EΜΞ‡O«EΜΞ‡O«EΜΞ‡O«EΜ                        N D 1 F E 3 ~ 1 . R C 0   €          h     —    Ξ‡O«EΜΞ‡O«EΜΞ‡O«EΜΞ‡O«EΜ                        n d a s e t u p . l o c . p t g . r c €   H                        @                           Awƒ   ÿÿÿÿ‚yG                                                                                               