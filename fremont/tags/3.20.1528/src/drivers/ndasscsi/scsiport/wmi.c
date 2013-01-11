/*++

Copyright (C) Microsoft Corporation, 1997 - 1999

Module Name:

    wmi.c

Abstract:

    This module contains the WMI support code for SCSIPORT's functional and
    physical device objects.

Authors:

    Dan Markarian

Environment:

    Kernel mode only.

Notes:

    None.

Revision History:

    19-Mar-1997, Original Writing, Dan Markarian

--*/

#include "port.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NDSC_WMI"

#ifdef __INTERRUPT__

#if DBG
static const char *__file__ = __FILE__;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ScsiPortSystemControlIrp)
#pragma alloc_text(PAGE, SpWmiIrpNormalRequest)
#pragma alloc_text(PAGE, SpWmiIrpRegisterRequest)

#pragma alloc_text(PAGE, SpWmiHandleOnMiniPortBehalf)
#pragma alloc_text(PAGE, SpWmiPassToMiniPort)

#pragma alloc_text(PAGE, SpWmiDestroySpRegInfo)
#pragma alloc_text(PAGE, SpWmiGetSpRegInfo)
#pragma alloc_text(PAGE, SpWmiInitializeSpRegInfo)

#pragma alloc_text(PAGE, SpWmiInitializeFreeRequestList)

#endif


NTSTATUS
ScsiPortSystemControlIrp(
    PDEVICE_OBJECT DeviceObject,
    PIRP           Irp
    )

/*++

Routine Description:

   Process an IRP_MJ_SYSTEM_CONTROL request packet.

Arguments:

   DeviceObject - Pointer to the functional or physical device object.

   Irp          - Pointer to the request packet.

Return Value:

   NTSTATUS result code.

--*/

{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION       irpSp;
    NTSTATUS                 status          = STATUS_SUCCESS;
    WMI_PARAMETERS           wmiParameters;

    ULONG isRemoved;

    PAGED_CODE();

    isRemoved = SpAcquireRemoveLock(DeviceObject, Irp);

    if (isRemoved) {
        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        SpReleaseRemoveLock(DeviceObject, Irp);
        SpCompleteRequest(DeviceObject, Irp, NULL, IO_NO_INCREMENT);
        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    //
    // Obtain a pointer to the current IRP stack location.
    //
    irpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(irpSp->MajorFunction == IRP_MJ_SYSTEM_CONTROL);

    //
    // Determine if this WMI request was destined to us.  If not, pass the IRP
    // down.
    //
    if ( (PDEVICE_OBJECT)irpSp->Parameters.WMI.ProviderId != DeviceObject) {

        //
        // Copy parameters from our stack location to the next stack location.
        //
        IoCopyCurrentIrpStackLocationToNext(Irp);

        //
        // Pass the IRP on to the next driver.
        //
        SpReleaseRemoveLock(DeviceObject, Irp);
        return IoCallDriver(commonExtension->LowerDeviceObject, Irp);
    }

    //
    // Copy the WMI parameters into our local WMISRB structure.
    //
    wmiParameters.ProviderId = irpSp->Parameters.WMI.ProviderId;
    wmiParameters.DataPath   = irpSp->Parameters.WMI.DataPath;
    wmiParameters.Buffer     = irpSp->Parameters.WMI.Buffer;
    wmiParameters.BufferSize = irpSp->Parameters.WMI.BufferSize;

    //
    // Determine what the WMI request wants of us.
    //
    switch (irpSp->MinorFunction) {
        case IRP_MN_QUERY_ALL_DATA:
            //
            // Query for all instances of a data block.
            //
        case IRP_MN_QUERY_SINGLE_INSTANCE:
            //
            // Query for a single instance of a data block.
            //
        case IRP_MN_CHANGE_SINGLE_INSTANCE:
            //
            // Change all data items in a data block for a single instance.
            //
        case IRP_MN_CHANGE_SINGLE_ITEM:
            //
            // Change a single data item in a data block for a single instance.
            //
        case IRP_MN_ENABLE_EVENTS:
            //
            // Enable events.
            //
        case IRP_MN_DISABLE_EVENTS:
            //
            // Disable events.
            //
        case IRP_MN_ENABLE_COLLECTION:
            //
            // Enable data collection for the given GUID.
            //
        case IRP_MN_DISABLE_COLLECTION:
            //
            // Disable data collection for the given GUID.
            //
            status = SpWmiIrpNormalRequest(DeviceObject,
                                           irpSp->MinorFunction,
                                           &wmiParameters);
            break;

        case IRP_MN_EXECUTE_METHOD:
            //
            // Execute method
            //
            status = SpWmiIrpNormalRequest(DeviceObject,
                                           irpSp->MinorFunction,
                                           &wmiParameters);
            break;

        case IRP_MN_REGINFO:
            //
            // Query for registration and registration update information.
            //
            status = SpWmiIrpRegisterRequest(DeviceObject, &wmiParameters);
            break;

        default:
            //
            // Unsupported WMI request.
            //
            status = STATUS_INVALID_DEVICE_REQUEST;
            ASSERT(FALSE);
            break;
    }

    //
    // Complete this WMI IRP request.
    //
    Irp->IoStatus.Status     = status;
    Irp->IoStatus.Information= NT_SUCCESS(status) ? wmiParameters.BufferSize : 0;
    SpReleaseRemoveLock(DeviceObject, Irp);
    SpCompleteRequest(DeviceObject, Irp, NULL, IO_NO_INCREMENT);

    return status;
}


NTSTATUS
SpWmiIrpNormalRequest(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN     UCHAR           WmiMinorCode,
    IN OUT PWMI_PARAMETERS WmiParameters
    )

/*++

Routine Description:

    Process an IRP_MJ_SYSTEM_CONTROL request packet (for all requests except registration
    IRP_MN_REGINFO requests).

Arguments:

    DeviceObject  - Pointer to the functional or physical device object.

    WmiMinorCode  - WMI action to perform.

    WmiParameters - Pointer to the WMI request parameters.

Return Value:

    NTSTATUS result code to complete the WMI IRP with.

Notes:

    If this WMI request is completed with STATUS_SUCCESS, the WmiParameters
    BufferSize field will reflect the actual size of the WMI return buffer.

--*/

{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    NTSTATUS                 status          = STATUS_SUCCESS;

    PAGED_CODE();

    //
    // Determine if SCSIPORT will repond to this WMI request on behalf of
    // the miniport driver.
    //
    status = SpWmiHandleOnMiniPortBehalf(DeviceObject,
                                         WmiMinorCode,
                                         WmiParameters);

    //
    // If not, pass the request onto the miniport driver, provided the
    // miniport driver does support WMI.
    //
    if (status == STATUS_UNSUCCESSFUL && commonExtension->WmiMiniPortSupport) {

        //
        // Send off the WMI request to the miniport.
        //
        status = SpWmiPassToMiniPort(DeviceObject,
                                     WmiMinorCode,
                                     WmiParameters);

        if (NT_SUCCESS(status)) {

            //
            // Fill in fields miniport cannot fill in for itself.
            //
            if ( WmiMinorCode == IRP_MN_QUERY_ALL_DATA ||
                 WmiMinorCode == IRP_MN_QUERY_SINGLE_INSTANCE ) {
                PWNODE_HEADER wnodeHeader = WmiParameters->Buffer;

                ASSERT( WmiParameters->BufferSize >= sizeof(WNODE_HEADER) );

                KeQuerySystemTime(&wnodeHeader->TimeStamp);
            }
        } else {

            //
            // Translate SRB status into a meaningful NTSTATUS status.
            //
            status = STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    return status;
}


NTSTATUS
SpWmiIrpRegisterRequest(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN OUT PWMI_PARAMETERS WmiParameters
    )

/*++

Routine Description:

   Process an IRP_MJ_SYSTEM_CONTROL registration request.

Arguments:

   DeviceObject  - Pointer to the functional or physical device object.

   WmiParameters - Pointer to the WMI request parameters.

Return Value:

   NTSTATUS result code to complete the WMI IRP with.

Notes:

   If this WMI request is completed with STATUS_SUCCESS, the WmiParameters
   BufferSize field will reflect the actual size of the WMI return buffer.

--*/

{
    PCOMMON_EXTENSION   commonExtension = DeviceObject->DeviceExtension;
    PSCSIPORT_DRIVER_EXTENSION driverExtension = NULL;

    ULONG                      countedRegistryPathSize = 0;
    ULONG                      retSz;
    PWMIREGINFO                spWmiRegInfoBuf;
    ULONG                      spWmiRegInfoBufSize;
    NTSTATUS                   status = STATUS_SUCCESS;
    BOOLEAN                    wmiUpdateRequest;
    ULONG                      i;
    PDEVICE_OBJECT             pDO;

    WMI_PARAMETERS  paranoidBackup = *WmiParameters;

    PAGED_CODE();

    //
    // Validate our assumptions for this function's code.
    //
    ASSERT(WmiParameters->BufferSize >= sizeof(ULONG));

    //
    // Validate the registration mode.
    //
    switch ( (ULONG)(ULONG_PTR)WmiParameters->DataPath ) {
        case WMIUPDATE:
            //
            // No SCSIPORT registration information will be piggybacked
            // on behalf of the miniport for a WMIUPDATE request.
            //
            wmiUpdateRequest = TRUE;
            break;

        case WMIREGISTER:
            wmiUpdateRequest = FALSE;
            break;

        default:
            //
            // Unsupported registration mode.
            //
            ASSERT(FALSE);
            return STATUS_INVALID_PARAMETER;
    }

    //
    // Obtain the driver extension for this miniport (FDO/PDO).
    //
    driverExtension = IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                                 ScsiPortInitialize);

    ASSERT(driverExtension != NULL);

    //
    // Obtain a pointer to the SCSIPORT WMI registration information
    // buffer, which is registered on behalf of the miniport driver.
    //
    SpWmiGetSpRegInfo(DeviceObject, &spWmiRegInfoBuf,
                      &spWmiRegInfoBufSize);

    //
    // Pass the WMI registration request to the miniport.  This is not
    // necessary if we know the miniport driver does not support WMI.
    //
    if (commonExtension->WmiMiniPortSupport) {
        //
        // Note that we shrink the buffer size by the size necessary
        // to hold SCSIPORT's own registration information, which we
        // register on behalf of the miniport.   This information is
        // piggybacked into the WMI return buffer after the call  to
        // the miniport.  We ensure that the BufferSize passed to the
        // miniport is no smaller than "sizeof(ULONG)" so that it can
        // tell us the required buffer size should the buffer be too
        // small [by filling in this ULONG].
        //
        // Note that we must also make enough room for a copy of the
        // miniport registry path in the buffer, since the WMIREGINFO
        // structures from the miniport DO NOT set their registry
        // path fields.
        //
        ASSERT(WmiParameters->BufferSize >= sizeof(ULONG));

        //
        // Calculate size of required miniport registry path.
        //
        countedRegistryPathSize = driverExtension->RegistryPath.Length
                                  + sizeof(USHORT);

        //
        // Shrink buffer by the appropriate size. Note that the extra
        // 7 bytes (possibly extraneous) is subtracted to ensure that
        // the piggybacked data added later on is 8-byte aligned (if
        // any).
        //
        if (spWmiRegInfoBufSize && !wmiUpdateRequest) {
            WmiParameters->BufferSize =
                (WmiParameters->BufferSize > spWmiRegInfoBufSize + countedRegistryPathSize + 7 + sizeof(ULONG)) ?
                WmiParameters->BufferSize - spWmiRegInfoBufSize - countedRegistryPathSize - 7 :
            sizeof(ULONG);
        } else { // no data to piggyback
            WmiParameters->BufferSize =
                (WmiParameters->BufferSize > countedRegistryPathSize + sizeof(ULONG)) ?
                WmiParameters->BufferSize - countedRegistryPathSize :
            sizeof(ULONG);
        }

        //
        // Call the minidriver.
        //
        status = SpWmiPassToMiniPort(DeviceObject,
                                     IRP_MN_REGINFO,
                                     WmiParameters);

        ASSERT(WmiParameters->ProviderId == paranoidBackup.ProviderId);
        ASSERT(WmiParameters->DataPath == paranoidBackup.DataPath);
        ASSERT(WmiParameters->Buffer == paranoidBackup.Buffer);
        ASSERT(WmiParameters->BufferSize <= paranoidBackup.BufferSize);

        //
        // Assign WmiParameters->BufferSize to retSz temporarily.
        //
        // Note that on return from the above call, the wmiParameters'
        // BufferSize field has been _modified_ to reflect the current
        // size of the return buffer.
        //
        retSz = WmiParameters->BufferSize;

    } else if (WmiParameters->BufferSize < spWmiRegInfoBufSize &&
               !wmiUpdateRequest) {

        //
        // Insufficient space to hold SCSIPORT WMI registration information
        // alone.  Inform WMI appropriately of the required buffer size.
        //
        *((ULONG*)WmiParameters->Buffer) = spWmiRegInfoBufSize;
        WmiParameters->BufferSize = sizeof(ULONG);
        return STATUS_SUCCESS;

    } else { // no miniport support for WMI, sufficient space for scsiport info

        //
        // Fake having the miniport return zero WMIREGINFO structures.
        //
        retSz = 0;
    }

    //
    // Piggyback SCSIPORT's registration information into the WMI
    // registration buffer.
    //
    if (status == STATUS_BUFFER_TOO_SMALL || retSz == sizeof(ULONG)) {
        //
        // Miniport could not fit registration information into the
        // pre-shrunk buffer.
        //
        // Buffer currently contains a ULONG specifying required buffer
        // size of miniport registration info, but does not include the
        // SCSIPORT registration info's size.  Add it in.
        //
        if (spWmiRegInfoBufSize && !wmiUpdateRequest) {

            *((ULONG*)WmiParameters->Buffer) += spWmiRegInfoBufSize;

            //
            // Add an extra 7 bytes (possibly extraneous) which is used to
            // ensure that the piggybacked data structure 8-byte aligned.
            //
            *((ULONG*)WmiParameters->Buffer) += 7;
        }

        //
        // Add in size of the miniport registry path.
        //
        *((ULONG*)WmiParameters->Buffer) += countedRegistryPathSize;

        //
        // Return STATUS_SUCCESS, even though this is a BUFFER TOO
        // SMALL failure, while ensuring retSz = sizeof(ULONG), as
        // the WMI protocol calls us to do.
        //
        retSz  = sizeof(ULONG);
        status = STATUS_SUCCESS;

    } else if ( NT_SUCCESS(status) ) {
        //
        // Zero or more WMIREGINFOs exist in buffer from miniport.
        //

        //
        // Piggyback the miniport registry path transparently, if at least one
        // WMIREGINFO was returned by the minport.
        //
        if (retSz) {

            ULONG offsetToRegPath  = retSz;
            PWMIREGINFO wmiRegInfo = WmiParameters->Buffer;

            //
            // Build a counted wide-character string, containing the
            // registry path, into the WMI buffer.
            //
            *( (PUSHORT)( (PUCHAR)WmiParameters->Buffer + retSz ) ) =
                driverExtension->RegistryPath.Length,
            RtlCopyMemory( (PUCHAR)WmiParameters->Buffer + retSz + sizeof(USHORT),
                           driverExtension->RegistryPath.Buffer,
                           driverExtension->RegistryPath.Length);

            //
            // Traverse the WMIREGINFO structures returned by the mini-
            // driver and set the missing RegistryPath fields to point
            // to our registry path location. We also jam in the PDO for
            // the device stack so that the device instance name is used for
            // the wmi instance names.
            //
            pDO = commonExtension->IsPdo ? DeviceObject :
                            ((PADAPTER_EXTENSION)commonExtension)->LowerPdo;

            while (1) {
                wmiRegInfo->RegistryPath = offsetToRegPath;

                for (i = 0; i < wmiRegInfo->GuidCount; i++)
                {
                    wmiRegInfo->WmiRegGuid[i].InstanceInfo = (ULONG_PTR)pDO;
                    wmiRegInfo->WmiRegGuid[i].Flags &= ~(WMIREG_FLAG_INSTANCE_BASENAME |
                                                      WMIREG_FLAG_INSTANCE_LIST);
                    wmiRegInfo->WmiRegGuid[i].Flags |= WMIREG_FLAG_INSTANCE_PDO;
                }

                if (wmiRegInfo->NextWmiRegInfo == 0) {
                    break;
                }

                offsetToRegPath -= wmiRegInfo->NextWmiRegInfo;
                wmiRegInfo = (PWMIREGINFO)( (PUCHAR)wmiRegInfo +
                                            wmiRegInfo->NextWmiRegInfo );
            }

            //
            // Adjust retSz to reflect new size of the WMI buffer.
            //
            retSz += countedRegistryPathSize;
            wmiRegInfo->BufferSize = retSz;
        } // else, no WMIREGINFOs registered whatsoever, nothing to piggyback

        //
        // Do we have any SCSIPORT WMIREGINFOs to piggyback?
        //
        if (spWmiRegInfoBufSize && !wmiUpdateRequest) {

            //
            // Adjust retSz so that the data we piggyback is 8-byte aligned
            // (safe if retSz = 0).
            //
            retSz = (retSz + 7) & ~7;

            //
            // Piggyback SCSIPORT's registration info into the buffer.
            //
            RtlCopyMemory( (PUCHAR)WmiParameters->Buffer + retSz,
                           spWmiRegInfoBuf,
                           spWmiRegInfoBufSize);

            //
            // Was at least one WMIREGINFO returned by the minidriver?
            // Otherwise, we have nothing else to add to the WMI buffer.
            //
            if (retSz) { // at least one WMIREGINFO returned by minidriver
                PWMIREGINFO wmiRegInfo = WmiParameters->Buffer;

                //
                // Traverse to the end of the WMIREGINFO structures returned
                // by the miniport.
                //
                while (wmiRegInfo->NextWmiRegInfo) {
                    wmiRegInfo = (PWMIREGINFO)( (PUCHAR)wmiRegInfo +
                                                wmiRegInfo->NextWmiRegInfo );
                }

                //
                // Chain minidriver's WMIREGINFO structures to SCSIPORT's
                // WMIREGINFO structures.
                //
                wmiRegInfo->NextWmiRegInfo = retSz -
                                             (ULONG)((PUCHAR)wmiRegInfo - (PUCHAR)WmiParameters->Buffer);
            }

            //
            // Adjust retSz to reflect new size of the WMI buffer.
            //
            retSz += spWmiRegInfoBufSize;

        } // we had SCSIPORT REGINFO data to piggyback
    } // else, unknown error, complete IRP with this error status

    //
    // Save new buffer size to WmiParameters->BufferSize.
    //
    WmiParameters->BufferSize = retSz;

    return status;
}


NTSTATUS
SpWmiHandleOnMiniPortBehalf(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN     UCHAR           WmiMinorCode,
    IN OUT PWMI_PARAMETERS WmiParameters
    )

/*++

Routine Description:

   Handle the WMI request on the miniport's behalf, if possible.

Arguments:

   DeviceObject  - Pointer to the functional or physical device object.

   WmiMinorCode  - WMI action to perform.

   WmiParameters - WMI parameters.

Return Value:

   If STATUS_UNSUCCESSFUL is returned, SCSIPORT did not handle this WMI
   request.  It must be passed on to the miniport driver for processing.

   Otherwise, this function returns an NTSTATUS code describing the result
   of handling the WMI request.  Complete the IRP with this status.

Notes:

   If this WMI request is completed with STATUS_SUCCESS, the WmiParameters
   BufferSize field will reflect the actual size of the WMI return buffer.

--*/
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    if (commonExtension->IsPdo) {
        //
        /// Placeholder for code to check if this is a PDO-relevant GUID which
        //  SCSIPORT must handle, and handle it if so.
        //
    } else { // FDO
        //
        /// Placeholder for code to check if this is an FDO-relevant GUID which
        //  SCSIPORT must handle, and handle it if so.
        //
    }

    //
    /// Currently, the SCSIPORT driver does not handle any WMI requests on
    //  behalf of the miniport driver.
    //
    return STATUS_UNSUCCESSFUL;
}


NTSTATUS
SpWmiPassToMiniPort(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN     UCHAR           WmiMinorCode,
    IN OUT PWMI_PARAMETERS WmiParameters
    )
/*++

Routine Description:

   This function pass a WMI request to the miniport driver for processing.
   It creates an SRB which is processed normally by the port driver.  This
   call is synchronous.

   Callers of SpWmiPassToMiniPort must be running at IRQL PASSIVE_LEVEL.

Arguments:

   DeviceObject  - Pointer to the functional or physical device object.

   WmiMinorCode  - WMI action to perform.

   WmiParameters - WMI parameters.

Return Value:

   An NTSTATUS code describing the result of handling the WMI request.
   Complete the IRP with this status.

Notes:

   If this WMI request is completed with STATUS_SUCCESS, the WmiParameters
   BufferSize field will reflect the actual size of the WMI return buffer.

--*/
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    PADAPTER_EXTENSION fdoExtension;
    SCSI_WMI_REQUEST_BLOCK   srb;
    LARGE_INTEGER            startingOffset;
    PLOGICAL_UNIT_EXTENSION  logicalUnit;

    ULONG                    commonBufferSize;
    PUCHAR                   commonBuffer;
    PHYSICAL_ADDRESS         physicalAddress;
    PVOID                    removeTag = (PVOID)((ULONG_PTR)WmiParameters+3);
    PWNODE_HEADER            wnode;

    NTSTATUS status;

    PAGED_CODE();

    startingOffset.QuadPart = (LONGLONG) 1;

    //
    // Zero out the SRB.
    //
    RtlZeroMemory(&srb, sizeof(SCSI_WMI_REQUEST_BLOCK));

    //
    // Initialize the SRB for a WMI request.
    //
    if (commonExtension->IsPdo) {                                       // [PDO]

        //
        // Set the logical unit addressing from this PDO's device extension.
        //
        logicalUnit = DeviceObject->DeviceExtension;

        SpAcquireRemoveLock(DeviceObject, removeTag);

        srb.PathId      = logicalUnit->PathId;
        srb.TargetId    = logicalUnit->TargetId;
        srb.Lun         = logicalUnit->Lun;

        fdoExtension = logicalUnit->AdapterExtension;

    } else {                                                            // [FDO]

        //
        // Set the logical unit addressing to the first logical unit.  This is
        // merely used for addressing purposes for adapter requests only.
        // NOTE: SpFindSafeLogicalUnit will acquire the remove lock
        //

        logicalUnit = SpFindSafeLogicalUnit(DeviceObject,
                                            0xff,
                                            removeTag);

        if (logicalUnit == NULL) {
            return(STATUS_DEVICE_DOES_NOT_EXIST);
        }

        fdoExtension = DeviceObject->DeviceExtension;

        srb.WMIFlags    = SRB_WMI_FLAGS_ADAPTER_REQUEST;
        srb.PathId      = logicalUnit->PathId;
        srb.TargetId    = logicalUnit->TargetId;
        srb.Lun         = logicalUnit->Lun;
    }

    //
    // HACK - allocate a chunk of common buffer for the actual request to
    // get processed in. We need to determine the size of buffer to allocate
    // this is the larger of the input or output buffers
    //

    if (WmiMinorCode == IRP_MN_EXECUTE_METHOD)
    {
        wnode = (PWNODE_HEADER)WmiParameters->Buffer;
        commonBufferSize = (WmiParameters->BufferSize > wnode->BufferSize) ?
                            WmiParameters->BufferSize :
                            wnode->BufferSize;
    } else {
        commonBufferSize = WmiParameters->BufferSize;
    }
                        
    commonBuffer = HalAllocateCommonBuffer(fdoExtension->DmaAdapterObject,
                                           commonBufferSize,
                                           &physicalAddress,
                                           FALSE);

    if(commonBuffer == NULL) {
        DebugPrint((1, "SpWmiPassToMiniPort: Unable to allocate %#x bytes of "
                       "common buffer\n", commonBufferSize));
        
        SpReleaseRemoveLock(logicalUnit->DeviceObject, removeTag);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    try {
        KEVENT event;
        PIRP irp;
        PMDL mdl;
        PIO_STACK_LOCATION irpStack;

        RtlCopyMemory(commonBuffer, WmiParameters->Buffer, commonBufferSize);
    
        srb.DataBuffer         = commonBuffer;       // [already non-paged]
        srb.DataTransferLength = WmiParameters->BufferSize;
        srb.Function           = SRB_FUNCTION_WMI;
        srb.Length             = sizeof(SCSI_REQUEST_BLOCK);
        srb.WMISubFunction     = WmiMinorCode;
        srb.DataPath           = WmiParameters->DataPath;
        srb.SrbFlags           = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;
        srb.TimeOutValue       = 10;                                // [ten seconds]
    
        //
        // Note that the value in DataBuffer may be used regardless of the value
        // of the MapBuffers field.
        //
    
        //
        // Initialize the notification event.
        //

        KeInitializeEvent(&event, NotificationEvent, FALSE);
    
        //
        // Build IRP for this request.
        // Note we do this synchronously for two reasons.  If it was done
        // asynchonously then the completion code would have to make a special
        // check to deallocate the buffer.  Second if a completion routine were
        // used then an additional IRP stack location would be needed.
        //

        irp = IoAllocateIrp(logicalUnit->DeviceObject->StackSize, FALSE);

        if(irp == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            leave;
        }

        mdl = IoAllocateMdl(commonBuffer,
                            WmiParameters->BufferSize,
                            FALSE,
                            FALSE,
                            irp);

        if(mdl == NULL) {
            IoFreeIrp(irp);
            status = STATUS_INSUFFICIENT_RESOURCES;
            leave;
        }

        MmBuildMdlForNonPagedPool(mdl);

        srb.OriginalRequest = irp;
    
        irpStack = IoGetNextIrpStackLocation(irp);
    
        //
        // Set major code.
        //
        irpStack->MajorFunction = IRP_MJ_SCSI;
    
        //
        // Set SRB pointer.
        //
        irpStack->Parameters.Scsi.Srb = (PSCSI_REQUEST_BLOCK)&srb;
    
        //
        // Setup a completion routine so we know when the request has completed.
        //

        IoSetCompletionRoutine(irp,
                               SpSignalCompletion,
                               &event,
                               TRUE,
                               TRUE,
                               TRUE);

        //
        // Flush the data buffer for output.  This will insure that the data is
        // written back to memory.  Since the data-in flag is the the port driver
        // will flush the data again for input which will ensure the data is not
        // in the cache.
        //
        KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
    
        //
        // Call port driver to handle this request.
        //
        IoCallDriver(logicalUnit->CommonExtension.DeviceObject, irp);
    
        //
        // Wait for request to complete.
        //
        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
    
        status = irp->IoStatus.Status;

        //
        // Relay the return buffer's size to the caller on success.
        //
        if (NT_SUCCESS(status)) {
            WmiParameters->BufferSize = srb.DataTransferLength;
        }
    
        //
        // Copy back the correct number of bytes into the caller provided buffer.
        //
    
        RtlCopyMemory(WmiParameters->Buffer,
                      commonBuffer,
                      WmiParameters->BufferSize);

        //
        // Free the irp and MDL.
        //

        IoFreeMdl(mdl);
        IoFreeIrp(irp);

    } finally {

        HalFreeCommonBuffer(fdoExtension->DmaAdapterObject,
                            commonBufferSize,
                            physicalAddress,
                            commonBuffer,
                            FALSE);
    
        SpReleaseRemoveLock(logicalUnit->CommonExtension.DeviceObject,
                            removeTag);
    }

    //
    // Return the IRP's status.
    //
    return status;
}


VOID
SpWmiGetSpRegInfo(
    IN  PDEVICE_OBJECT DeviceObject,
    OUT PWMIREGINFO  * SpRegInfoBuf,
    OUT ULONG        * SpRegInfoBufSize
    )
/*++

Routine Description:

   This function retrieves a pointer to the WMI registration information
   buffer for the given device object.

Arguments:

   DeviceObject     - Pointer to the functional or physical device object.

Return Values:

   SpRegInfoBuf     - Pointer to the registration information buffer, which
                      will point to the WMIREGINFO structures that SCSIPORT
                      should register on behalf of the miniport driver.

   SpRegInfoBufSize - Size of the registration information buffer in bytes.

--*/
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    //
    // Retrieve a pointer to the WMI registration information buffer for the
    // given device object.
    //
    if (commonExtension->WmiScsiPortRegInfoBuf     == NULL ||
        commonExtension->WmiScsiPortRegInfoBufSize == 0) {
        *SpRegInfoBuf     = NULL;
        *SpRegInfoBufSize = 0;
    } else {
        *SpRegInfoBuf     = commonExtension->WmiScsiPortRegInfoBuf;
        *SpRegInfoBufSize = commonExtension->WmiScsiPortRegInfoBufSize;
    }

    return;
}


VOID
SpWmiInitializeSpRegInfo(
    IN  PDEVICE_OBJECT  DeviceObject
    )

/*++

Routine Description:

   This function allocates space for and builds the WMI registration
   information buffer for this device object.

   The WMI registration information consists of zero or more WMIREGINFO
   structures which are used to register and identify SCSIPORT-handled
   WMI GUIDs on behalf of the miniport driver. This information is not
   the complete set of WMI GUIDs supported by for device object,  only
   the ones supported by SCSIPORT.  It is actually piggybacked onto the
   WMIREGINFO structures provided by the miniport driver during
   registration.

   The WMI registration information is allocated and stored on a
   per-device basis because, concievably, each device may support
   differing WMI GUIDs and/or instances during its lifetime.

Arguments:

   DeviceObject   - Pointer to the functional or physical device object.

Return Value:

   None.

--*/
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    ASSERT(commonExtension->WmiScsiPortRegInfoBuf     == NULL);
    ASSERT(commonExtension->WmiScsiPortRegInfoBufSize == 0);

    if (commonExtension->IsPdo) {
        //
        /// Placeholder for code to build PDO-relevant GUIDs into the
        //  registration buffer.
        //
        /// commonExtension->WmiScsiPortRegInfo     = ExAllocatePool( PagedPool, <size> );
        //  commonExtension->WmiScsiPortRegInfoSize = <size>;
        //  <code to fill in wmireginfo struct(s) into buffer>
        //
        //  * use L"SCSIPORT" as the RegistryPath
    } else { // FDO
        //
        /// Placeholder for code to build FDO-relevant GUIDs into the
        //  registration buffer.
        //
        /// commonExtension->WmiScsiPortRegInfo     = ExAllocatePool( PagedPool, <size> );
        //  commonExtension->WmiScsiPortRegInfoSize = <size>;
        //  <code to fill in wmireginfo struct(s) into buffer>
        //
        //  * use L"SCSIPORT" as the RegistryPath
    }

    return;
}


VOID
SpWmiDestroySpRegInfo(
    IN  PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

   This function de-allocates the space for the WMI registration information
   buffer for this device object, if one exists.

Arguments:

   DeviceObject - Pointer to the functional or physical device object.

Return Value:

   None.

--*/
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    if (commonExtension->WmiScsiPortRegInfoBuf) {
        ExFreePool(commonExtension->WmiScsiPortRegInfoBuf);
        commonExtension->WmiScsiPortRegInfoBuf = NULL;
    }

    commonExtension->WmiScsiPortRegInfoBufSize = 0;

    return;
}


NTSTATUS
SpWmiInitializeFreeRequestList(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG          NumberOfItems
    )
/*++

Routine Description:

    Call that initializes the WmiFreeMiniPortRequestList, this call MUST
    be completed prior to any manipulatio of the WmiFreeMiniPortRequestList

    The list will be initialized with at most the number of cells requested.

    If the list has already been initialized, we raise the watermark by the number
    of Items requested.

Arguments:

    DeviceObject    - Device Object that this list belongs to
    NumberOfItems   - requested number of free cells

Return Value:

    Return the SUCESS if list was initialized succesfully

    STATUS_INSUFFICIENT_REOSOURCES  - Indicates that we could not allocate
                                      enough memory for the list header

Notes:


--*/
{
    PADAPTER_EXTENSION  fdoExtension;
    ULONG               itemsInserted;
    KIRQL               oldIrql;

    PAGED_CODE();               // Routine is paged until locked down.

    //
    // Obtain a pointer to the functional device extension (for the adapter).
    //
    if ( ((PCOMMON_EXTENSION)DeviceObject->DeviceExtension)->IsPdo ) {
        fdoExtension = ((PLOGICAL_UNIT_EXTENSION)DeviceObject->DeviceExtension)
                       ->AdapterExtension;
    } else {
        fdoExtension = DeviceObject->DeviceExtension;
    }

    // If the list has been initalized increase the watermark
    if (fdoExtension->WmiFreeMiniPortRequestInitialized) {
        DebugPrint((2, "SpWmiInitializeFreeRequestList:"
                    " Increased watermark for : %p\n", fdoExtension));

        InterlockedExchangeAdd
            (&(fdoExtension->WmiFreeMiniPortRequestWatermark),
             NumberOfItems);

        while (fdoExtension->WmiFreeMiniPortRequestCount <
            fdoExtension->WmiFreeMiniPortRequestWatermark) {

            // Add free cells until the count reaches the watermark
            SpWmiPushFreeRequestItem(fdoExtension);
        }

        return (STATUS_SUCCESS);
    }

    // Only PDO's should be calling when the list has not been initialized
    ASSERT_FDO(DeviceObject);

    // Assignt he list we just initialized to the pointer in the
    // fdoExtension (and save the lock pointer also)
    KeInitializeSpinLock(&(fdoExtension->WmiFreeMiniPortRequestLock));
    ExInitializeSListHead(&(fdoExtension->WmiFreeMiniPortRequestList));

    DebugPrint((1, "SpWmiInitializeFreeRequestList:"
                " Initialized WmiFreeRequestList for: %p\n", fdoExtension));

    // Set the initialized flag
    fdoExtension->WmiFreeMiniPortRequestInitialized = TRUE;

    // Set the watermark, and the count to 0
    fdoExtension->WmiFreeMiniPortRequestWatermark = 0;
    fdoExtension->WmiFreeMiniPortRequestCount = 0;

    // Attempt to add free cells to the free list
    for (itemsInserted = 0; itemsInserted < NumberOfItems;
         itemsInserted++) {

        // Make a request to push a NULL item, so that the
        // allocation will be done by the next function
        //
        // At this point we don't care about the return value
        // because after we set the watermark, scsiport's free-cell
        // repopulation code will try to get the free list cell count
        // back to the watermark. (So if we fail to add all the requested
        // free cells, the repopulation code will attempt again for us
        // at a later time)
        SpWmiPushFreeRequestItem(fdoExtension);
    }


    // Now set the watermark to the correct value
    fdoExtension->WmiFreeMiniPortRequestWatermark = NumberOfItems;

    return(STATUS_SUCCESS);
}


NTSTATUS
SpWmiPushFreeRequestItem(
    IN PADAPTER_EXTENSION           fdoExtension
    )
/*++

Routine Description:

    Inserts the Entry into the interlocked SLIST.  (Of Free items)

Arguments:

    fdoExtension        - The extension on the adapter

Return Value:

    STATUS_SUCESS                   - If succesful
    STATUS_INSUFFICIENT_RESOURCES   - If memory allocation fails
    STATUS_UNSUCCESSFUL             - Free List not initialized

Notes:

    This code cannot be marked as pageable since it will be called from
    DPC level

    Theoricatlly this call can fail, but no one should call this function
    before we've been initialized

--*/
{
    PWMI_MINIPORT_REQUEST_ITEM      Entry = NULL;

    if (!fdoExtension->WmiFreeMiniPortRequestInitialized) {
        return (STATUS_UNSUCCESSFUL);
    }

    Entry = ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(WMI_MINIPORT_REQUEST_ITEM),
        SCSIPORT_TAG_WMI_EVENT);

    if (!Entry) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    Entry->NextRequest = NULL;

    // Insert Cell into interlocked list
    ExInterlockedPushEntrySList(
        &(fdoExtension->WmiFreeMiniPortRequestList),
        (PSINGLE_LIST_ENTRY)Entry,
        &(fdoExtension->WmiFreeMiniPortRequestLock));

    // Increment the value of the free count
    InterlockedIncrement(&(fdoExtension->WmiFreeMiniPortRequestCount));

    return(STATUS_SUCCESS);
}


PWMI_MINIPORT_REQUEST_ITEM
SpWmiPopFreeRequestItem(
    IN PADAPTER_EXTENSION           fdoExtension
    )
/*++

Routine Description:

    Pops an Entry from the interlocked SLIST.  (Of Free items)

Arguments:

    fdoExtension     - The extension on the adapter

Return Value:

    A pointer to a REQUEST_ITEM or NULL if none are available

Notes:

    This code cannot be paged, it will be called a DIRLQL

--*/
{
    PWMI_MINIPORT_REQUEST_ITEM              requestItem;

    if (!fdoExtension->WmiFreeMiniPortRequestInitialized) {
        return (NULL);
    }

    // Pop Cell from interlocked list
    requestItem = (PWMI_MINIPORT_REQUEST_ITEM)
        ExInterlockedPopEntrySList(
            &(fdoExtension->WmiFreeMiniPortRequestList),
            &(fdoExtension->WmiFreeMiniPortRequestLock));


    if (requestItem) {
        // Decrement the count of free cells
        InterlockedDecrement(&(fdoExtension->WmiFreeMiniPortRequestCount));

    }

    return (requestItem);
}



BOOLEAN
SpWmiRemoveFreeMiniPortRequestItems(
    IN PADAPTER_EXTENSION   fdoExtension
    )
/*++

Routine Description:

   This function removes WMI_MINIPORT_REQUEST_ITEM structures from the "free"
   queue of the adapter extension.

   It removed all the free cells.

Arguments:

    fdoExtension    - The device_extension

Return Value:

   TRUE always.

--*/

{
    PWMI_MINIPORT_REQUEST_ITEM   tmpRequestItem;
    PWMI_MINIPORT_REQUEST_ITEM   wmiRequestItem;

    //
    // Set the watermark to 0
    // No need to grab a lock we're just setting it
    fdoExtension->WmiFreeMiniPortRequestWatermark = 0;

    DebugPrint((1, "SpWmiRemoveFreeMiniPortRequestItems: Removing %p", fdoExtension));


    //
    // Walk the queue of items and de-allocate as many as we need to.
    //
    for (;;) {
        // Pop
        wmiRequestItem = SpWmiPopFreeRequestItem(fdoExtension);
        if (wmiRequestItem == NULL) {
            break;
        } else {
            ExFreePool(wmiRequestItem);
        }
    }

    // BUGBUG
    // Later we should add a check and make sure that the count of
    // cells is 0

    return TRUE;
}

#endif // __INTERRUPT__
