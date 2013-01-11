/*++

Copyright (C) Microsoft Corporation, 1990 - 1998

Module Name:

    prop.c

Abstract:

    This is the NT SCSI port driver.  This module contains code relating to
    property queries

Authors:

    Peter Wieland

Environment:

    kernel mode only

Notes:

Revision History:

--*/
#include "port.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSMP_Prop"

#ifdef __INTERRUPT__

NTSTATUS
SpBuildDeviceDescriptor(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSTORAGE_DEVICE_DESCRIPTOR Descriptor,
    IN OUT PULONG DescriptorLength
    );

NTSTATUS
SpBuildAdapterDescriptor(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSTORAGE_ADAPTER_DESCRIPTOR Descriptor,
    IN OUT PULONG DescriptorLength
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SpBuildDeviceDescriptor)
#pragma alloc_text(PAGE, SpBuildAdapterDescriptor)
#pragma alloc_text(PAGE, ScsiPortQueryProperty)
#pragma alloc_text(PAGE, SpQueryDeviceText)
#endif


NTSTATUS
ScsiPortQueryProperty(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP QueryIrp
    )

/*++

Routine Description:

    This routine will handle a property query request.  It will build the
    descriptor on it's own if possible, or it may forward the request down
    to lower level drivers.

    Since this routine may forward the request downwards the caller should
    not complete the irp

    This routine is asynchronous.
    This routine must be called at <= IRQL_DISPATCH
    This routine must be called with the remove lock held

Arguments:

    DeviceObject - a pointer to the device object being queried

    QueryIrp - a pointer to the irp for the query

Return Value:

    STATUS_PENDING if the request cannot be completed yet
    STATUS_SUCCESS if the query was successful

    STATUS_INVALID_PARAMETER_1 if the property id does not exist
    STATUS_INVALID_PARAMETER_2 if the query type is invalid
    STATUS_INVALID_PARAMETER_3 if an invalid optional parameter was passed

    STATUS_INVALID_DEVICE_REQUEST if this request cannot be handled by this
    device

    other error values as applicable

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(QueryIrp);

    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PSTORAGE_PROPERTY_QUERY query = QueryIrp->AssociatedIrp.SystemBuffer;
    ULONG queryLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    NTSTATUS status;

    PAGED_CODE();

    //
    // BUGBUG - don't handle mask queries yet
    //

    if(query->QueryType >= PropertyMaskQuery) {

        status = STATUS_INVALID_PARAMETER_1;
        QueryIrp->IoStatus.Status = status;
        QueryIrp->IoStatus.Information = 0;
        SpReleaseRemoveLock(DeviceObject, QueryIrp);
        SpCompleteRequest(DeviceObject, QueryIrp, NULL, IO_NO_INCREMENT);
        return status;
    }

    switch(query->PropertyId) {

        case StorageDeviceProperty: {

            //
            // Make sure this is a target device.
            //

            if(!commonExtension->IsPdo) {

                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }

            if(query->QueryType == PropertyExistsQuery) {

                status = STATUS_SUCCESS;

            } else {

                status = SpBuildDeviceDescriptor(
                            DeviceObject,
                            QueryIrp->AssociatedIrp.SystemBuffer,
                            &queryLength);

                QueryIrp->IoStatus.Information = queryLength;
            }
            break;
        }

        case StorageAdapterProperty: {

            PDEVICE_OBJECT adapterObject = DeviceObject;

            //
            // if this is a target device then forward it down to the
            // underlying device object.  This lets filters do their magic
            //

            if(commonExtension->IsPdo) {

                //
                // Call the lower device
                //

                IoSkipCurrentIrpStackLocation(QueryIrp);
                SpReleaseRemoveLock(DeviceObject, QueryIrp);
                status = IoCallDriver(commonExtension->LowerDeviceObject, QueryIrp);

                return status;
            }

            if(query->QueryType == PropertyExistsQuery) {

                status = STATUS_SUCCESS;

            } else {

                status = SpBuildAdapterDescriptor(
                            DeviceObject,
                            QueryIrp->AssociatedIrp.SystemBuffer,
                            &queryLength);

                QueryIrp->IoStatus.Information = queryLength;
            }
            break;
        }

        default: {

            //
            // If this is a target device then some filter beneath us may
            // handle this property.
            //

            if(commonExtension->IsPdo) {

                //
                // Call the lower device.
                //

                IoSkipCurrentIrpStackLocation(QueryIrp);
                SpReleaseRemoveLock(DeviceObject, QueryIrp);
                status = IoCallDriver(commonExtension->LowerDeviceObject, QueryIrp);

                return status;
            }

            //
            // Nope, this property really doesn't exist
            //

            status = STATUS_INVALID_PARAMETER_1;
            QueryIrp->IoStatus.Information = 0;
            break;
        }
    }

    if(status != STATUS_PENDING) {
        QueryIrp->IoStatus.Status = status;
        SpReleaseRemoveLock(DeviceObject, QueryIrp);
        SpCompleteRequest(DeviceObject, QueryIrp, NULL, IO_DISK_INCREMENT);
    }

    return status;
}

NTSTATUS
SpBuildDeviceDescriptor(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSTORAGE_DEVICE_DESCRIPTOR Descriptor,
    IN OUT PULONG DescriptorLength
    )

/*++

Routine Description:

    This routine will create a device descriptor based on the information in
    it's device extension.  It will copy as much data as possible into
    the Descriptor and will update the DescriptorLength to indicate the
    number of bytes copied

Arguments:

    DeviceObject - a pointer to the PDO we are building a descriptor for

    Descriptor - a buffer to store the descriptor in

    DescriptorLength - the length of the buffer and the number of bytes
                       returned

    QueryIrp - unused

Return Value:

    status

--*/

{
    PLOGICAL_UNIT_EXTENSION physicalExtension = DeviceObject->DeviceExtension;
    PSCSIPORT_DRIVER_EXTENSION driverExtension = 
        IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                   ScsiPortInitialize);

    LONG maxLength = *DescriptorLength;
    LONG bytesRemaining = maxLength;
    LONG realLength = sizeof(STORAGE_DEVICE_DESCRIPTOR);

    PUCHAR currentOffset = (PUCHAR) Descriptor;

    LONG inquiryLength;

    PINQUIRYDATA inquiryData = &(physicalExtension->InquiryData);

    STORAGE_DEVICE_DESCRIPTOR tmp;

    PAGED_CODE();

    ASSERT(physicalExtension->CommonExtension.IsPdo);
    ASSERT(Descriptor);

    //
    // Figure out what the total size of this structure is going to be
    //

    inquiryLength = 4 + inquiryData->AdditionalLength;

    if(inquiryLength > INQUIRYDATABUFFERSIZE) {
        inquiryLength = INQUIRYDATABUFFERSIZE;
    }

    realLength += inquiryLength + 31;   // 31 = length of the 3 id strings +
                                        // 3 nuls

    // BUGBUG - need to deal with serial numbers

    RtlZeroMemory(Descriptor, *DescriptorLength);

    //
    // Build the device descriptor structure on the stack then copy as much as
    // can be copied over
    //

    RtlZeroMemory(&tmp, sizeof(STORAGE_DEVICE_DESCRIPTOR));

    tmp.Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
    tmp.Size = realLength;

    tmp.DeviceType = inquiryData->DeviceType;
    tmp.DeviceTypeModifier = inquiryData->DeviceTypeModifier;

    tmp.RemovableMedia = inquiryData->RemovableMedia;

    tmp.CommandQueueing = inquiryData->CommandQueue;

    tmp.SerialNumberOffset = 0xffffffff;

    tmp.BusType = driverExtension->BusType;

    RtlCopyMemory(currentOffset, &tmp, min(sizeof(STORAGE_DEVICE_DESCRIPTOR), bytesRemaining));

    bytesRemaining -= sizeof(STORAGE_DEVICE_DESCRIPTOR);

    if(bytesRemaining <= 0) {
        *DescriptorLength = maxLength;
        return STATUS_SUCCESS;
    }

    currentOffset = ((PUCHAR) Descriptor) + (maxLength - bytesRemaining);

    //
    // Copy over as much inquiry data as we can and update the raw byte count
    //

    RtlCopyMemory(currentOffset, inquiryData, min(inquiryLength, bytesRemaining));

    bytesRemaining -= inquiryLength;

    if(bytesRemaining <= 0) {

        *DescriptorLength = maxLength;

        Descriptor->RawPropertiesLength = maxLength -
                                          sizeof(STORAGE_DEVICE_DESCRIPTOR);

        return STATUS_SUCCESS;
    }

    Descriptor->RawPropertiesLength = inquiryLength;

    currentOffset = ((PUCHAR) Descriptor) + (maxLength - bytesRemaining);

    //
    // Now we need to start copying inquiry strings
    //

    //
    // first the vendor id
    //

    RtlCopyMemory(currentOffset,
                  inquiryData->VendorId,
                  min(bytesRemaining, sizeof(UCHAR) * 8));

    bytesRemaining -= sizeof(UCHAR) * 9;     // include trailing null

    if(bytesRemaining >= 0) {

        Descriptor->VendorIdOffset = (ULONG)((ULONG_PTR) currentOffset -
                                      (ULONG_PTR) Descriptor);

    }

    if(bytesRemaining <= 0) {
        *DescriptorLength = maxLength;
        return STATUS_SUCCESS;
    }

    currentOffset = ((PUCHAR) Descriptor) + (maxLength - bytesRemaining);

    //
    // now the product id
    //

    RtlCopyMemory(currentOffset,
                  inquiryData->ProductId,
                  min(bytesRemaining, 16));
    bytesRemaining -= 17;                   // include trailing null

    if(bytesRemaining >= 0) {

        Descriptor->ProductIdOffset = (ULONG)((ULONG_PTR) currentOffset -
                                       (ULONG_PTR) Descriptor);
    }

    if(bytesRemaining <= 0) {
        *DescriptorLength = maxLength;
        return STATUS_SUCCESS;
    }

    currentOffset = ((PUCHAR) Descriptor) + (maxLength - bytesRemaining);

    //
    // And the product revision
    //

    RtlCopyMemory(currentOffset,
                  inquiryData->ProductRevisionLevel,
                  min(bytesRemaining, 4));
    bytesRemaining -= 5;

    if(bytesRemaining >= 0) {
        Descriptor->ProductRevisionOffset = (ULONG)((ULONG_PTR) currentOffset -
                                             (ULONG_PTR) Descriptor);
    }

    if(bytesRemaining <= 0) {
        *DescriptorLength = maxLength;
        return STATUS_SUCCESS;
    }

    // currentOffset = ((PUCHAR) Descriptor) + (maxLength - bytesRemaining);

    //
    // And finally the device serial number
    //

    // BUGBUG - copy the device serial number

    *DescriptorLength = maxLength - bytesRemaining;
    return STATUS_SUCCESS;
}


NTSTATUS
SpBuildAdapterDescriptor(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSTORAGE_ADAPTER_DESCRIPTOR Descriptor,
    IN OUT PULONG DescriptorLength
    )
{
    PADAPTER_EXTENSION functionalExtension = DeviceObject->DeviceExtension;
    PSCSIPORT_DRIVER_EXTENSION driverExtension = 
        IoGetDriverObjectExtension(DeviceObject->DriverObject,
                                   ScsiPortInitialize);

    PIO_SCSI_CAPABILITIES capabilities = &(functionalExtension->Capabilities);

    STORAGE_ADAPTER_DESCRIPTOR tmp;

    PAGED_CODE();

    ASSERT(!functionalExtension->CommonExtension.IsPdo);

    tmp.Version = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
    tmp.Size = sizeof(STORAGE_ADAPTER_DESCRIPTOR);

    tmp.MaximumTransferLength = capabilities->MaximumTransferLength;
    tmp.MaximumPhysicalPages = capabilities->MaximumPhysicalPages;

    tmp.AlignmentMask = capabilities->AlignmentMask;

    tmp.AdapterUsesPio = capabilities->AdapterUsesPio;
    tmp.AdapterScansDown = capabilities->AdapterScansDown;
    tmp.CommandQueueing = capabilities->TaggedQueuing;
    tmp.AcceleratedTransfer = TRUE;        // BUGBUG - how do i specifiy this

    tmp.BusType = (BOOLEAN) driverExtension->BusType;
    tmp.BusMajorVersion = 2;
    tmp.BusMinorVersion = 0;

    RtlCopyMemory(Descriptor,
                  &tmp,
                  min(*DescriptorLength, sizeof(STORAGE_ADAPTER_DESCRIPTOR)));

    *DescriptorLength = min(*DescriptorLength, sizeof(STORAGE_ADAPTER_DESCRIPTOR));

    return STATUS_SUCCESS;
}


NTSTATUS
SpQueryDeviceText(
    IN PDEVICE_OBJECT LogicalUnit,
    IN DEVICE_TEXT_TYPE TextType,
    IN LCID LocaleId,
    IN OUT PWSTR *DeviceText
    )

{
    PLOGICAL_UNIT_EXTENSION luExtension = LogicalUnit->DeviceExtension;

    UCHAR ansiBuffer[256];
    ANSI_STRING ansiText;

    UNICODE_STRING unicodeText;
    
    NTSTATUS status;

    PAGED_CODE();
    
    RtlInitUnicodeString(&unicodeText, NULL);

    if(TextType == DeviceTextDescription) {

        PSCSIPORT_DEVICE_TYPE deviceInfo = 
            SpGetDeviceTypeInfo(luExtension->InquiryData.DeviceType);

        PUCHAR c;
        LONG i;

        RtlZeroMemory(ansiBuffer, sizeof(ansiBuffer));
        RtlCopyMemory(ansiBuffer, 
                      luExtension->InquiryData.VendorId,
                      sizeof(luExtension->InquiryData.VendorId));
        c = ansiBuffer;

        for(i = sizeof(luExtension->InquiryData.VendorId); i >= 0; i--) {
            if((c[i] != '\0') &&
               (c[i] != ' ')) {
                break;
            }
            c[i] = '\0';
        }
        c = &(c[i + 1]);

        sprintf(c, " ");
        c++;

        RtlCopyMemory(c,
                      luExtension->InquiryData.ProductId,
                      sizeof(luExtension->InquiryData.ProductId));

        for(i = sizeof(luExtension->InquiryData.ProductId); i >= 0; i--) {
            if((c[i] != '\0') &&
               (c[i] != ' ')) {
                break;
            }
            c[i] = '\0';
        }
        c = &(c[i + 1]);

        sprintf(c, " SCSI %s Device", deviceInfo->DeviceTypeString);

    } else if (TextType == DeviceTextLocationInformation) {

        sprintf(ansiBuffer, "Bus Number %d, Target ID %d, LUN %d", 
                luExtension->PathId,
                luExtension->TargetId,
                luExtension->Lun);

    } else {

        return STATUS_NOT_SUPPORTED;
    }

    RtlInitAnsiString(&ansiText, ansiBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeText,
                                          &ansiText,
                                          TRUE);

    *DeviceText = unicodeText.Buffer;
    return status;
}
 
#endif // __INTERRUPT__
