/*++

This file contains PDO routines

--*/
#include "port.h"
#include "ndasport.wmi.h"

#ifdef RUN_WPP
#include "pdo.tmh"
#endif

NTSTATUS
NdasPortPdoNotifyWmiEvent(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_PNP_NOTIFICATION_TYPE Type,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

NTSTATUS
NdasPortFdoNotifyPnpEvent(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_PNP_NOTIFICATION_TYPE Type,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

NTSTATUS
NdasPortPdoMiniportIoctl(
    __in PNDASPORT_PDO_EXTENSION PdoExtension,
    __in PIRP RequestIrp);

NTSTATUS
NdasPortPdoPnpQueryDeviceRelations(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp);

NTSTATUS
NdasPortPdoPnpQueryId(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NdasPortPdoMiniportIoctl)
#pragma alloc_text(PAGE, NdasPortPdoPnpQueryDeviceRelations)
#pragma alloc_text(PAGE, NdasPortPdoPnpQueryId)
#endif

NTSTATUS
NdasPortPdoNotifyWmiEvent(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_PNP_NOTIFICATION_TYPE Type,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	NTSTATUS status;
	PNdasPortWmiEvent wmiEventData;
	GUID wmiEventGuid = NdasPortWmiEvent_GUID;

	wmiEventData = (PNdasPortWmiEvent) ExAllocatePoolWithTag(
		NonPagedPool, NdasPortWmiEvent_SIZE, NDASPORT_TAG_WMI);

	if (NULL == wmiEventData)
	{
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_ERROR,
			"Memory allocation failed for NdasPortWmiEvent\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	wmiEventData->EventType = Type;
	wmiEventData->LogicalUnitAddress = LogicalUnitAddress.Address;

	status = WmiFireEvent(
		FdoExtension->DeviceObject,
		&wmiEventGuid,
		0,
		NdasPortWmiEvent_SIZE,
		wmiEventData);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_ERROR,
			"WmiFireEvent failed, status=0x%X\n", status);

		ExFreePoolWithTag(wmiEventData, NDASPORT_TAG_WMI);
	}

	return status;
}

NTSTATUS
NdasPortpFirePnpEvent(
	__in PDEVICE_OBJECT PhysicalDeviceObject,
	__in CONST GUID* EventGuid,
	__in_opt CONST WCHAR* EventName,
	__in_bcount_opt(CustomDataSize) PVOID CustomData,
	__in ULONG CustomDataSize)
{
	NTSTATUS status;
	PTARGET_DEVICE_CUSTOM_NOTIFICATION customNotification;
	PVOID buffer;
	size_t bufferSize;
	size_t eventNameLength;
	size_t customNotificationSize;
	LONG nameBufferOffset;
	ULONG removed;

	bufferSize = 
		FIELD_OFFSET(TARGET_DEVICE_CUSTOM_NOTIFICATION, CustomDataBuffer) + 
		CustomDataSize;

	if (NULL != EventName)
	{
		status = RtlStringCchLengthW(EventName, NTSTRSAFE_MAX_CCH, &eventNameLength);
		if (NT_SUCCESS(status))
		{
			ASSERTMSG("Invalid event name is specified!", NT_SUCCESS(status));
			return status;
		}
		nameBufferOffset = ALIGN_UP(bufferSize, USHORT);
		bufferSize = nameBufferOffset + (eventNameLength + 1) * sizeof(WCHAR);
	}
	else
	{
		nameBufferOffset = -1;
	}

	customNotificationSize = bufferSize;

	if (customNotificationSize > 0xFFFF)
	{
		//
		// Buffer size cannot be larger than the size where USHORT can hold
		//
		return STATUS_INVALID_PARAMETER;
	}

	buffer = ExAllocatePoolWithTag(
		NonPagedPool, 
		bufferSize,
		NDASPORT_TAG_PNP_EVENT);

	if (NULL == buffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(buffer, bufferSize);

	customNotification = (PTARGET_DEVICE_CUSTOM_NOTIFICATION) buffer;
	customNotification->Version = 1;
	customNotification->Size = (USHORT) customNotificationSize;
	customNotification->Event = *EventGuid;
	customNotification->NameBufferOffset = nameBufferOffset;

	if (NULL != CustomData)
	{
		RtlCopyMemory(
			customNotification->CustomDataBuffer,
			CustomData,
			CustomDataSize);
	}

	if (NULL != EventName)
	{
		RtlCopyMemory(
			NdasPortOffsetOf(customNotification, nameBufferOffset),
			EventName,
			eventNameLength * sizeof(WCHAR));
	}

	status = IoReportTargetDeviceChangeAsynchronous(
		PhysicalDeviceObject,
		customNotification, 
		NULL, 
		NULL);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_ERROR,
			"IoReportTargetDeviceChangeAsynchronous failed, status=0x%08X\n",
			status);
	}

	ExFreePoolWithTag(buffer, NDASPORT_TAG_PNP_EVENT);

	return status;
}

NTSTATUS
NdasPortFirePnpEvent(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in CONST GUID* EventGuid,
	__in_opt CONST WCHAR* EventName,
	__in_bcount_opt(CustomDataSize) PVOID CustomData,
	__in ULONG CustomDataSize)
{
	PNDASPORT_PDO_EXTENSION PdoExtension;
	PdoExtension = NdasPortLogicalUnitGetPdoExtension(LogicalUnitExtension);
	return NdasPortpFirePnpEvent(
		PdoExtension->DeviceObject,
		EventGuid,
		EventName,
		CustomData,
		CustomDataSize);
}

NTSTATUS
NdasPortFdoNotifyPnpEvent(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_PNP_NOTIFICATION_TYPE Type,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	NTSTATUS status;

	NDASPORT_PNP_NOTIFICATION portNotification;

	portNotification.Type = Type;
	portNotification.LogicalUnitAddress = LogicalUnitAddress;

	status = NdasPortpFirePnpEvent(
		FdoExtension->LowerPdo,
		&GUID_NDASPORT_PNP_NOTIFICATION,
		NULL,
		&portNotification,
		sizeof(NDASPORT_PNP_NOTIFICATION));

	return status;
}

NTSTATUS
NdasPortPdoCreatePhysicalDeviceObject(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG LogicalUnitExtensionSize,
	__in ULONG LogicalUnitSrbExtensionSize,
	__in PDEVICE_OBJECT ExternalDeviceObject,
	__in PFILE_OBJECT ExternalFileObject,
	__out PDEVICE_OBJECT* PhysicalDeviceObject)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION pdoExtension;
	PNDASPORT_COMMON_EXTENSION commonExtension;
	ULONG pdoExtensionSize;
	ULONG logicalUnitDescriptorOffset;
	ULONG pc, i;

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"NdasPortPdoCreatePhysicalDeviceObject, Fdo=%p\n", FdoExtension->DeviceObject);

	//
	// 8-byte Alignment
	//
	LogicalUnitExtensionSize = ALIGN_UP(LogicalUnitExtensionSize, 8);

	pdoExtensionSize = 
		FIELD_OFFSET(NDASPORT_PDO_EXTENSION, LogicalUnitExtensionHolder) +
		LogicalUnitExtensionSize +
		LogicalUnitDescriptor->Size;

	//
	// Logical Unit Descriptor is placed at the end
	//
	logicalUnitDescriptorOffset = 
		FIELD_OFFSET(NDASPORT_PDO_EXTENSION, LogicalUnitExtensionHolder) +
		LogicalUnitExtensionSize;

	ASSERT(pdoExtensionSize > sizeof(NDASPORT_PDO_EXTENSION));

	status = IoCreateDeviceSecure(
		FdoExtension->DeviceObject->DriverObject,
		pdoExtensionSize,
		NULL,
		FILE_DEVICE_MASS_STORAGE,
		FILE_AUTOGENERATED_DEVICE_NAME | FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&SDDL_DEVOBJ_SYS_ALL_ADM_ALL, // read wdmsec.h for more info
		NULL,
		PhysicalDeviceObject);

	if (!NT_SUCCESS (status)) 
	{
		NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_ERROR,
			"IoCreateDeviceSecure failed: Status=%08X\n", status);
		goto error1;
	}

	(*PhysicalDeviceObject)->StackSize = 1;
	(*PhysicalDeviceObject)->AlignmentRequirement = FdoExtension->DeviceObject->AlignmentRequirement;
	(*PhysicalDeviceObject)->Flags |= (DO_BUS_ENUMERATED_DEVICE | DO_DIRECT_IO);

	pdoExtension = (PNDASPORT_PDO_EXTENSION) (*PhysicalDeviceObject)->DeviceExtension;

	RtlZeroMemory(pdoExtension, pdoExtensionSize);

	pdoExtension->CommonExtension = &pdoExtension->CommonExtensionHolder;

	//
	// Initialize Common Extension Structure
	//

	commonExtension = pdoExtension->CommonExtension;

	NpInitializeCommonExtension(
		commonExtension,
		TRUE,
		*PhysicalDeviceObject,
		FdoExtension->DeviceObject,
		PdoMajorFunctionTable);

	//
	// Initialize PDO Extension Structure
	//

	pdoExtension->ParentFDOExtension = FdoExtension;
	pdoExtension->LogicalUnitType = LogicalUnitDescriptor->Type;

	RtlCopyMemory(
		&pdoExtension->LogicalUnitInterface,
		LogicalUnitInterface,
		sizeof(NDAS_LOGICALUNIT_INTERFACE));

	pdoExtension->Present = TRUE; // attached to the bus
	pdoExtension->ReportedMissing = FALSE; // not yet reported missing

	pdoExtension->IsTemporary = FALSE;
	pdoExtension->IsClaimed = FALSE;

	pdoExtension->LogicalUnitAddress.PortNumber = FdoExtension->PortNumber;
	pdoExtension->LogicalUnitAddress.PathId = LogicalUnitDescriptor->Address.PathId;
	pdoExtension->LogicalUnitAddress.TargetId = LogicalUnitDescriptor->Address.TargetId;
	pdoExtension->LogicalUnitAddress.Lun = LogicalUnitDescriptor->Address.Lun;

	pdoExtension->LogicalUnitType = LogicalUnitDescriptor->Type;
	pdoExtension->ExternalTypeGuid = LogicalUnitDescriptor->ExternalTypeGuid;
	pdoExtension->ExternalDeviceObject = ExternalDeviceObject;
	pdoExtension->ExternalFileObject = ExternalFileObject;

	pdoExtension->SrbDataExtensionSize = LogicalUnitSrbExtensionSize;

	LogicalUnitDescriptor->Address.PortNumber = FdoExtension->PortNumber;

	ExInitializeNPagedLookasideList(
		&pdoExtension->SrbDataLookasideList,
		NULL,
		NULL,
		0,
		sizeof(NDASPORT_SRB_DATA) + LogicalUnitSrbExtensionSize,
		NDASPORT_TAG_SRB_DATA,
		0);

	status = pdoExtension->LogicalUnitInterface.InitializeLogicalUnit(
		LogicalUnitDescriptor,
		NdasPortPdoGetLogicalUnitExtension(pdoExtension));

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_ERROR,
			"InitializeLogicalUnit failed: Status=%08X\n", status);
		goto error2;
	}

	//
	// Active Srb List
	//

	KeInitializeSpinLock(&pdoExtension->ActiveSrbListSpinLock);
	InitializeListHead(&pdoExtension->ActiveSrbListHead);

	//
	// Completion DPC Initialization
	//

	pc = pKeQueryActiveProcessorCount(NULL);

	pdoExtension->CompletionDpcCount = min(pc, NDASPORT_DPC_MAX_PROCESSORS);

	for (i = 0; i < pdoExtension->CompletionDpcCount; ++i)
	{
		KeInitializeDpc(
			&pdoExtension->CompletionDpc[i],
			NdasPortpCompleteRequestDpc,
			pdoExtension);

		KeSetTargetProcessorDpc(
			&pdoExtension->CompletionDpc[i],
			(CCHAR) i);

		KeInitializeSpinLock(&pdoExtension->CompletionDpcSpinLock[i]);
		InitializeListHead(&pdoExtension->CompletionSrbListHead[i]);
	}

	//
	// Set StartIoAttribute to prevent recursion in 
	// IoStartNextPacket and StartIoRoutine
	// (Applicable in Windows XP or later only)
	//

	pIoSetStartIoAttributes(
		*PhysicalDeviceObject,
		TRUE,
		TRUE);

	//
	// Copy the logical unit descriptor
	//
	pdoExtension->LogicalUnitDescriptor = NdasPortOffsetOf(
		pdoExtension, logicalUnitDescriptorOffset);

	RtlCopyMemory(
		pdoExtension->LogicalUnitDescriptor,
		LogicalUnitDescriptor,
		LogicalUnitDescriptor->Size);

	//
	// WMI Registration
	//

	if (pdoExtension->LogicalUnitWmiInfo)
	{
		status = IoWMIRegistrationControl(
			*PhysicalDeviceObject,
			WMIREG_ACTION_REGISTER);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_ERROR,
				"IoWMIRegistrationControl failed, status=%x\n", status);
			goto error3;
		}

		pdoExtension->WmiRegistered = TRUE;
	}

	return STATUS_SUCCESS;

error3:

	pdoExtension->LogicalUnitInterface.CleanupLogicalUnit(
		NdasPortPdoGetLogicalUnitExtension(pdoExtension));

error2:

	ExDeleteNPagedLookasideList(&pdoExtension->SrbDataLookasideList);
	IoDeleteDevice(*PhysicalDeviceObject);

error1:

	*PhysicalDeviceObject = NULL;

	return status;
}

VOID
NdasPortPdoRemoveDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension)
{
	NTSTATUS status;
	NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;
	PNDASPORT_FDO_EXTENSION fdoExtension;
	LONG wmiRegistered;

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
		"RemoveDevice PDO=%p\n", PdoExtension->DeviceObject);

	PdoExtension->LogicalUnitInterface.CleanupLogicalUnit(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension));

	//
	// WMI Deregistration
	//
	wmiRegistered = InterlockedCompareExchange(
		&PdoExtension->WmiRegistered,
		FALSE,
		TRUE);

	if (wmiRegistered)
	{
		status = IoWMIRegistrationControl(
			PdoExtension->DeviceObject,
			WMIREG_ACTION_DEREGISTER);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_WARNING,
				"IoWMIRegistrationControl(DEREGISTER) failed, status=0x%X\n",
				status);
		}
	}

	//
	// Delete SrbData LookasideList
	//
	ExDeleteNPagedLookasideList(&PdoExtension->SrbDataLookasideList);

	//
	// Dereference External File Object if it is external
	//
	if (NULL != PdoExtension->ExternalFileObject)
	{
		ObDereferenceObject(PdoExtension->ExternalFileObject);
		PdoExtension->ExternalFileObject = NULL;
		PdoExtension->ExternalDeviceObject = NULL;
	}

	//
	// Make sure to copy logicalUnitAddress before deleting the device object
	//
	fdoExtension = PdoExtension->ParentFDOExtension;
	logicalUnitAddress = PdoExtension->LogicalUnitAddress;

	IoDeleteDevice(PdoExtension->DeviceObject);

	//
	// PNP Notification for device removal (it is postmortem)
	//
	status = NdasPortFdoNotifyPnpEvent(
		fdoExtension, 
		NdasPortLogicalUnitIsRemoved, 
		logicalUnitAddress);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_WARNING,
			"NdasPortFdoNotifyPnpEvent(Removed) failed, status=0x%X, "
			"LogicalUnitAddress=%08X\n",
			status,
			logicalUnitAddress.Address);
	}

	//
	// WMI Event
	//
	status = NdasPortPdoNotifyWmiEvent(
		fdoExtension,
		NdasPortLogicalUnitIsRemoved,
		logicalUnitAddress);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_WARNING,
			"NdasPortPdoNotifyWmiEvent(NdasPortLogicalUnitIsRemoved) failed, status=0x%X, "
			"LogicalUnitAddress=%08X\n",
			status,
			logicalUnitAddress.Address);
	}
}

NTSTATUS
NdasPortPdoQueryProperty(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	PIO_STACK_LOCATION irpStack;
	PSTORAGE_PROPERTY_QUERY query;
	ULONG queryLength, inputBufferLength;

	PAGED_CODE();

	CommonExtension = PdoExtension->CommonExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	query = (PSTORAGE_PROPERTY_QUERY) Irp->AssociatedIrp.SystemBuffer;
	inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	queryLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

	if (inputBufferLength < FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters))
	{
		NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_INFORMATION,
			"Incomplete query, size=%d\n", inputBufferLength);
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);
	}


	NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_INFORMATION,
		"%s(%x) %s(%x)\n",
		DbgStoragePropertyIdStringA(query->PropertyId),
		query->PropertyId,
		DbgStorageQueryTypeStringA(query->QueryType),
		query->QueryType);

	//
	// We don't actually support mask queries.
	//
	if (query->QueryType >= PropertyMaskQuery) 
	{
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);
	}

	switch (query->PropertyId) 
	{
	case StorageDeviceProperty: 
		{
			PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor;

			deviceDescriptor = (PSTORAGE_DEVICE_DESCRIPTOR) Irp->AssociatedIrp.SystemBuffer;

			status = PdoExtension->LogicalUnitInterface.QueryStorageDeviceProperty(
				NdasPortPdoGetLogicalUnitExtension(PdoExtension),
				deviceDescriptor,
				queryLength,
				&queryLength);

			if (queryLength > FIELD_OFFSET(STORAGE_DEVICE_DESCRIPTOR, RawDeviceProperties))
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
					"Version=%08X, Size=%08X. "
					"DeviceType=%02X, Modifier=%02X, "
					"RemovableMedia=%1X, CommandQueuing=%1X, "
					"BusType=%X, RawPropertiesLength=%X\n",
					deviceDescriptor->Version,
					deviceDescriptor->Size,
					deviceDescriptor->DeviceType,
					deviceDescriptor->DeviceTypeModifier,
					deviceDescriptor->RemovableMedia,
					deviceDescriptor->CommandQueueing,
					deviceDescriptor->BusType,
					deviceDescriptor->RawPropertiesLength);
			}

#if 0
			ASSERT(deviceDescriptor->VendorIdOffset < queryLength);
			ASSERT(deviceDescriptor->ProductIdOffset < queryLength);
			ASSERT(deviceDescriptor->ProductRevisionOffset < queryLength);
			ASSERT(deviceDescriptor->SerialNumberOffset < queryLength);
			ASSERT(deviceDescriptor->RawPropertiesLength < queryLength);

			if (deviceDescriptor->VendorIdOffset > 0)
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
					"VendorId(%X)=%s", deviceDescriptor->VendorIdOffset,
					(PCHAR) NdasPortOffsetOf(deviceDescriptor, deviceDescriptor->VendorIdOffset));
			}

			if (deviceDescriptor->ProductIdOffset > 0)
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
					"ProductId(%X)=%s", deviceDescriptor->ProductIdOffset,
					(PCHAR) NdasPortOffsetOf(deviceDescriptor, deviceDescriptor->ProductIdOffset));
			}

			if (deviceDescriptor->ProductRevisionOffset > 0)
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
					"ProductRevision(%X)=%s", deviceDescriptor->ProductRevisionOffset,
					(PCHAR) NdasPortOffsetOf(deviceDescriptor, deviceDescriptor->ProductRevisionOffset));
			}

			if (deviceDescriptor->SerialNumberOffset > 0)
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
					"SerialNumber(%X)=%s", deviceDescriptor->SerialNumberOffset,
					(PCHAR) NdasPortOffsetOf(deviceDescriptor, deviceDescriptor->SerialNumberOffset));
			}
#endif
		}
		break;
	case StorageAdapterProperty:

		if (NULL == PdoExtension->LogicalUnitInterface.QueryStorageAdapterProperty)
		{
			return NpReleaseRemoveLockAndForwardIrp(
				CommonExtension,
				Irp);
		}
		else
		{
			PSTORAGE_ADAPTER_DESCRIPTOR adapterDescriptor;

			adapterDescriptor = (PSTORAGE_ADAPTER_DESCRIPTOR) 
				Irp->AssociatedIrp.SystemBuffer;

			status = PdoExtension->LogicalUnitInterface.QueryStorageAdapterProperty(
				NdasPortPdoGetLogicalUnitExtension(PdoExtension),
				adapterDescriptor,
				queryLength,
				&queryLength);
		}

		break;

	case StorageDeviceIdProperty:  /* Windows XP or later */

		if (NULL != PdoExtension->LogicalUnitInterface.QueryStorageDeviceIdProperty)
		{
			PSTORAGE_DEVICE_ID_DESCRIPTOR deviceIdDescriptor;

			deviceIdDescriptor = (PSTORAGE_DEVICE_ID_DESCRIPTOR) Irp->AssociatedIrp.SystemBuffer;

			status = PdoExtension->LogicalUnitInterface.QueryStorageDeviceIdProperty(
				NdasPortPdoGetLogicalUnitExtension(PdoExtension),
				deviceIdDescriptor,
				queryLength,
				&queryLength);
		}
		else
		{
			status = STATUS_NOT_SUPPORTED;
		}

		break;
	default: 
		//
		// If this is a target device then some filter beneath us may
		// handle this property. Call the lower device.
		//
		status = STATUS_NOT_SUPPORTED;
	}

	if (status == STATUS_NOT_SUPPORTED)
	{
		//
		// We do not send it down to the port driver
		// 
		//return NpReleaseRemoveLockAndForwardIrp(
		//	CommonExtension,
		//	Irp);
	}

	return NpReleaseRemoveLockAndCompleteIrpEx(
		CommonExtension,
		Irp,
		status,
		queryLength,
		IO_NO_INCREMENT);
}

/*++

Routine Description:

    This function sends a miniport ioctl to the miniport driver.
    It creates an srb which is processed normally by the port driver.
    This call is synchronous.

Arguments:

    DeviceExtension - Supplies a pointer the SCSI adapter device extension.

    RequestIrp - Supplies a pointe to the Irp which made the original request.

Return Value:

    Returns a status indicating the success or failure of the operation.

--*/

NTSTATUS
NdasPortPdoIsValidMiniportIoctl(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
    PIO_STACK_LOCATION irpStack;
    PSRB_IO_CONTROL srbControl;
	ULONG inputLength;
    ULONG outputLength;
    ULONG length;

    PAGED_CODE();

    //
    // Get a pointer to the control block.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    srbControl = Irp->AssociatedIrp.SystemBuffer;

	inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

    //
    // Validate the user buffer.
    //

    if (inputLength < sizeof(SRB_IO_CONTROL))
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT DeviceIoControl input buffer too small "
			"(InputBufferLength=0x%X < 0x%X in bytes)\n",
			inputLength,
			sizeof(SRB_IO_CONTROL));

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    if (srbControl->HeaderLength != sizeof(SRB_IO_CONTROL)) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT SrbControl->HeaderLength is invalid, "
			"(HeaderLength=0x%X != 0x%X in bytes)\n",
			srbControl->HeaderLength,
			sizeof(SRB_IO_CONTROL));

		Irp->IoStatus.Status = STATUS_REVISION_MISMATCH;
        return STATUS_REVISION_MISMATCH;
    }

    length = srbControl->HeaderLength + srbControl->Length;

	if ((length < srbControl->HeaderLength) || (length < srbControl->Length))
	{
		//
		// total length overflows a ULONG
		//
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT SrbControl->(HeaderLength + Length) overflows ULONG "
			"(HeaderLength=0x%X, Length=0x%X\n",
			srbControl->HeaderLength,
			srbControl->Length);

		return STATUS_INVALID_PARAMETER;
    }

    outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (outputLength < length && inputLength < length) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT BufferLengths are less than HeaderLength + Length "
			"(OutputBufferLength 0x%X or InputBufferLength 0x%X < "
			"HeaderLength=0x%X + Length=0x%X = 0x%X\n",
			outputLength,
			inputLength,
			srbControl->HeaderLength,
			srbControl->Length,
			length);

		Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        return STATUS_BUFFER_TOO_SMALL;
    }

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortPdoRedirectMiniportIoCtl(
	__in PNDASPORT_PDO_EXTENSION PdoExtension, 
	__in PIRP Irp,
	__in PSRB_IO_CONTROL SrbControl)
{
	NTSTATUS status;
	KEVENT event;
	PIRP redirectedIrp;
	LARGE_INTEGER startingOffset;
	SCSI_REQUEST_BLOCK srb;
	PIO_STACK_LOCATION irpStack;
	IO_STATUS_BLOCK ioStatus;
	ULONG length;
	ULONG outputLength;

	PAGED_CODE();

	irpStack = IoGetCurrentIrpStackLocation(Irp);
	outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
	length = SrbControl->HeaderLength + SrbControl->Length;

	startingOffset.QuadPart = (LONGLONG) 1;

    //
    // Must be at PASSIVE_LEVEL to use synchronous FSD.
    //

    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchronously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    redirectedIrp = IoBuildSynchronousFsdRequest(
		IRP_MJ_SCSI,
		PdoExtension->DeviceObject,
        SrbControl,
        length,
        &startingOffset,
        &event,
        &ioStatus);

	if (NULL == redirectedIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

    irpStack = IoGetNextIrpStackLocation(redirectedIrp);

    //
    // Set major and minor codes.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = IRP_MN_SCSI_CLASS;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    srb.PathId = PdoExtension->LogicalUnitAddress.PathId;
    srb.TargetId = PdoExtension->LogicalUnitAddress.TargetId;
    srb.Lun = PdoExtension->LogicalUnitAddress.Lun;

    srb.Function = SRB_FUNCTION_IO_CONTROL;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;

    srb.OriginalRequest = redirectedIrp;

    //
    // Set timeout to requested value.
    //

    srb.TimeOutValue = SrbControl->Timeout;

    //
    // Set the data buffer.
    //

    srb.DataBuffer = SrbControl;
    srb.DataTransferLength = length;

    //
    // Call port driver to handle this request.
    //

    status = IoCallDriver(PdoExtension->DeviceObject, redirectedIrp);

    //
    // Wait for request to complete.
    //

    if (STATUS_PENDING == status) 
	{
        KeWaitForSingleObject(
			&event,
            Executive,
            KernelMode,
            FALSE,
            NULL);
    }

    //
    // Set the information length to the smaller of the output buffer length
    // and the length returned in the srb.
    //

	status = ioStatus.Status;

    Irp->IoStatus.Information = srb.DataTransferLength > outputLength ?
        outputLength : srb.DataTransferLength;

    Irp->IoStatus.Status = status;

    return status;
}

NTSTATUS
NdasPortPdoMiniportIoctl(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStack;
	PSRB_IO_CONTROL srbControl;
	ULONG inputLength;
	ULONG outputLength;
	ULONG length;

	PAGED_CODE();

	Irp->IoStatus.Information = 0;

	status = NdasPortPdoIsValidMiniportIoctl(PdoExtension, Irp);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	srbControl = Irp->AssociatedIrp.SystemBuffer;

	return NdasPortPdoRedirectMiniportIoCtl(PdoExtension, Irp, srbControl);
}

NTSTATUS
NdasPortPdoScsiPassThrough(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP RequestIrp,
	__inout_bcount(max(InputBufferLength,OutputBufferLength)) 
		PSCSI_PASS_THROUGH ScsiPassThroughData,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength)
{
	PIRP irp;
	PIO_STACK_LOCATION irpStack;
	SCSI_REQUEST_BLOCK srb;
	KEVENT event;
	LARGE_INTEGER startingOffset;
	IO_STATUS_BLOCK ioStatusBlock;
	KIRQL currentIrql;
	ULONG length;
	ULONG bufferOffset;
	PVOID buffer;
	PVOID senseBuffer;
	UCHAR majorCode;
	NTSTATUS status;

	PAGED_CODE();

	irpStack = IoGetCurrentIrpStackLocation(RequestIrp);

	startingOffset.QuadPart = (LONGLONG) 1;

	NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"Enter routine\n");

	//
	// Validate the user buffer.
	//

#if defined (_WIN64)

	if (IoIs32bitProcess(RequestIrp)) 
	{
		PSCSI_PASS_THROUGH32 ScsiPassThroughData32;
		ULONG32 dataBufferOffset;
		ULONG   senseInfoOffset;

		ScsiPassThroughData32 = (PSCSI_PASS_THROUGH32) ScsiPassThroughData;

		//
		// copy the fields that follow the ULONG_PTR
		//

		dataBufferOffset = (ULONG32) (ScsiPassThroughData32->DataBufferOffset);
		senseInfoOffset = ScsiPassThroughData32->SenseInfoOffset;
		ScsiPassThroughData->DataBufferOffset = (ULONG_PTR) dataBufferOffset;
		ScsiPassThroughData->SenseInfoOffset = senseInfoOffset;

		RtlCopyMemory(
			ScsiPassThroughData->Cdb,
			ScsiPassThroughData32->Cdb,
			16*sizeof(UCHAR));

		if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(SCSI_PASS_THROUGH32))
		{
			return(STATUS_INVALID_PARAMETER);
		}

		if (ScsiPassThroughData->Length != sizeof(SCSI_PASS_THROUGH32) &&
			ScsiPassThroughData->Length != sizeof(SCSI_PASS_THROUGH_DIRECT32)) 
		{
			return(STATUS_REVISION_MISMATCH);
		}

	}
	else 
	{
#endif
		if (InputBufferLength < sizeof(SCSI_PASS_THROUGH))
		{
			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR,
				"Input buffer length %#08lx too small\n",
				InputBufferLength);

			return STATUS_INVALID_PARAMETER;
		}

		if (ScsiPassThroughData->Length != sizeof(SCSI_PASS_THROUGH) &&
			ScsiPassThroughData->Length != sizeof(SCSI_PASS_THROUGH_DIRECT)) 
		{
			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR,
				"SrbControl length %#08lx incorrect\n",
				ScsiPassThroughData->Length);
			return STATUS_REVISION_MISMATCH;
		}

#if defined (_WIN64)
	}
#endif

	//
	// Validate the rest of the buffer parameters.
	//

	if (ScsiPassThroughData->CdbLength > 16) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR,
			"CdbLength %#x is incorrect\n", ScsiPassThroughData->CdbLength);
		return STATUS_INVALID_PARAMETER;
	}

	//
	// If there's a sense buffer then its offset cannot be shorter than the
	// length of the srbControl block, nor can it be located after the data
	// buffer (if any)
	//

	if (ScsiPassThroughData->SenseInfoLength != 0 &&
		(ScsiPassThroughData->Length > ScsiPassThroughData->SenseInfoOffset ||
		(ScsiPassThroughData->SenseInfoOffset + ScsiPassThroughData->SenseInfoLength >
		ScsiPassThroughData->DataBufferOffset && ScsiPassThroughData->DataTransferLength != 0))) 
	{
			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR,
				"Bad sense info offset\n");

			return STATUS_INVALID_PARAMETER;
	}

	majorCode = !ScsiPassThroughData->DataIn ? IRP_MJ_WRITE : IRP_MJ_READ;

	if (ScsiPassThroughData->DataTransferLength == 0) 
	{
		length = 0;
		buffer = NULL;
		bufferOffset = 0;
		majorCode = IRP_MJ_FLUSH_BUFFERS;
	} 
	else if (ScsiPassThroughData->DataBufferOffset > OutputBufferLength &&
		ScsiPassThroughData->DataBufferOffset > InputBufferLength) 
	{
		//
		// The data buffer offset is greater than system buffer.  Assume this
		// is a user mode address.
		//

		if (ScsiPassThroughData->SenseInfoOffset + 
			ScsiPassThroughData->SenseInfoLength > OutputBufferLength
			&& ScsiPassThroughData->SenseInfoLength) 
		{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
					"Sense buffer is not in ioctl buffer\n");

				return STATUS_INVALID_PARAMETER;
		}

		//
		// Make sure the buffer is properly aligned.
		//

		if (ScsiPassThroughData->DataBufferOffset &
			PdoExtension->DeviceObject->AlignmentRequirement)
		{
			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
				"Data buffer not aligned "
				"[%#p doesn't have alignment of %#0x\n",
				ScsiPassThroughData->DataBufferOffset,
				PdoExtension->DeviceObject->AlignmentRequirement);

			return STATUS_INVALID_PARAMETER;
		}

		length = ScsiPassThroughData->DataTransferLength;
		buffer = (PCHAR) ScsiPassThroughData->DataBufferOffset;
		bufferOffset = 0;

		//
		// make sure the user buffer is valid.  The last byte must be at or 
		// below the highest possible user address.  Additionally the end of 
		// the buffer must not wrap around in memory (taking care to ensure that
		// a one-byte length buffer is okay)
		//

		if (KernelMode == RequestIrp->RequestorMode != KernelMode) 
		{
			if (length) 
			{
				ULONG_PTR endByte = (ULONG_PTR) buffer + length - 1;

				if ((endByte > (ULONG_PTR) MM_HIGHEST_USER_ADDRESS) ||
					((ULONG_PTR) buffer >= endByte + 1)) 
				{
					NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
						"User buffer invalid\n");
					return STATUS_INVALID_USER_BUFFER;
				}
			}
		}

	}
	else 
	{
		if (ScsiPassThroughData->DataIn != SCSI_IOCTL_DATA_IN) 
		{
			if ((ScsiPassThroughData->SenseInfoOffset + 
				ScsiPassThroughData->SenseInfoLength > OutputBufferLength
				&& ScsiPassThroughData->SenseInfoLength != 0) ||
				ScsiPassThroughData->DataBufferOffset + 
				ScsiPassThroughData->DataTransferLength > InputBufferLength ||
				ScsiPassThroughData->Length > ScsiPassThroughData->DataBufferOffset) 
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
					"Sense or data buffer not in ioctl buffer\n");

				return STATUS_INVALID_PARAMETER;
			}
		}

		//
		// Make sure the buffer is properly aligned.
		//

		if (ScsiPassThroughData->DataBufferOffset &
			PdoExtension->DeviceObject->AlignmentRequirement)
		{
			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
				"Data buffer not aligned [%#p doesn't have alignment of %#0x\n",
				ScsiPassThroughData->DataBufferOffset,
				PdoExtension->DeviceObject->AlignmentRequirement);

			return STATUS_INVALID_PARAMETER;
		}

		if (ScsiPassThroughData->DataIn) 
		{
			if (ScsiPassThroughData->DataBufferOffset + 
				ScsiPassThroughData->DataTransferLength > OutputBufferLength ||
				ScsiPassThroughData->Length > ScsiPassThroughData->DataBufferOffset) 
			{
				NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
					"Data buffer not in ioctl buffer or offset too small\n");
				return STATUS_INVALID_PARAMETER;
			}
		}

		length = (ULONG)ScsiPassThroughData->DataBufferOffset +
			ScsiPassThroughData->DataTransferLength;
		buffer = (PUCHAR) ScsiPassThroughData;
		bufferOffset = (ULONG)ScsiPassThroughData->DataBufferOffset;

	}

	//
	// Validate that the request isn't too large for the miniport.
	//

#if 0
	if (ScsiPassThroughData->DataTransferLength &&
		((ADDRESS_AND_SIZE_TO_SPAN_PAGES(
			(PUCHAR)buffer+bufferOffset,
			ScsiPassThroughData->DataTransferLength) > DeviceExtension->Capabilities.MaximumPhysicalPages) ||
		(DeviceExtension->Capabilities.MaximumTransferLength < ScsiPassThroughData->DataTransferLength))) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
			"Request is too large for this miniport\n");
		return STATUS_INVALID_PARAMETER;
	}
#endif

	if (ScsiPassThroughData->TimeOutValue == 0 ||
		ScsiPassThroughData->TimeOutValue > 30 * 60 * 60) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
			"Timeout value %d is invalid\n", ScsiPassThroughData->TimeOutValue);
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Check for illegal command codes.
	//

	if (ScsiPassThroughData->Cdb[0] == SCSIOP_COPY ||
		ScsiPassThroughData->Cdb[0] == SCSIOP_COMPARE ||
		ScsiPassThroughData->Cdb[0] == SCSIOP_COPY_COMPARE) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
			"Failing attempt to send restricted "
			"SCSI command %#x\n", ScsiPassThroughData->Cdb[0]);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	//
	// If this request came through a normal device control rather than from
	// class driver then the device must exist and be unclaimed. Class drivers
	// will set the minor function code for the device control.  It is always
	// zero for a user request.
	//

	if ((irpStack->MinorFunction == 0) && (PdoExtension->IsClaimed)) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
			"Pass through request to claimed "
			"device must come through the driver which claimed it\n");
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	//
	// Allocate an aligned request sense buffer.
	//

	if (ScsiPassThroughData->SenseInfoLength != 0) 
	{
		senseBuffer = ExAllocatePoolWithTag(
			NonPagedPool,
			ScsiPassThroughData->SenseInfoLength,
			NDASPORT_TAG_SENSE);

		if (senseBuffer == NULL) 
		{
			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
				"Unable to allocate sense buffer\n");

			return STATUS_INSUFFICIENT_RESOURCES;
		}

	}
	else 
	{
		senseBuffer = NULL;
	}

	//
	// Must be at PASSIVE_LEVEL to use synchronous FSD.
	//

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	//
	// Initialize the notification event.
	//

	KeInitializeEvent(
		&event,
		NotificationEvent,
		FALSE);

	//
	// Build IRP for this request.
	// Note we do this synchronously for two reasons.  If it was done
	// asynchonously then the completion code would have to make a special
	// check to deallocate the buffer.  Second if a completion routine were
	// used then an addation stack locate would be needed.
	//

	__try 
	{
		irp = IoBuildSynchronousFsdRequest(
			majorCode,
			PdoExtension->DeviceObject,
			buffer,
			length,
			&startingOffset,
			&event,
			&ioStatusBlock);

	}
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{

		NTSTATUS exceptionCode;

		//
		// An exception was incurred while attempting to probe the
		// caller's parameters.  Dereference the file object and return
		// an appropriate error status code.
		//

		if (senseBuffer != NULL) 
		{
			ExFreePool(senseBuffer);
		}

		exceptionCode = GetExceptionCode();

		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
			"Exception %#08lx building irp\n", exceptionCode);

		return exceptionCode;

	}

	if (irp == NULL)
	{
		if (senseBuffer != NULL) 
		{
			ExFreePool(senseBuffer);
		}

		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_ERROR, 
			"Couldn't allocate irp\n");

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irpStack = IoGetNextIrpStackLocation(irp);

	//
	// Set major code.
	//

	irpStack->MajorFunction = IRP_MJ_SCSI;
	irpStack->MinorFunction = 1;

	//
	// Fill in SRB fields.
	//

	irpStack->Parameters.Others.Argument1 = &srb;

	//
	// Zero out the srb.
	//

	RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

	//
	// Fill in the srb.
	//

	srb.Length = SCSI_REQUEST_BLOCK_SIZE;
	srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
	srb.SrbStatus = SRB_STATUS_PENDING;
	srb.PathId = ScsiPassThroughData->PathId;
	srb.TargetId = ScsiPassThroughData->TargetId;
	srb.Lun = ScsiPassThroughData->Lun;
	srb.CdbLength = ScsiPassThroughData->CdbLength;
	srb.SenseInfoBufferLength = ScsiPassThroughData->SenseInfoLength;

	switch (ScsiPassThroughData->DataIn) 
	{
	case SCSI_IOCTL_DATA_OUT:
		if (ScsiPassThroughData->DataTransferLength) 
		{
			srb.SrbFlags = SRB_FLAGS_DATA_OUT;
		}
		break;

	case SCSI_IOCTL_DATA_IN:
		if (ScsiPassThroughData->DataTransferLength) 
		{
			srb.SrbFlags = SRB_FLAGS_DATA_IN;
		}
		break;

	default:
		srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT;
		break;
	}

	if (ScsiPassThroughData->DataTransferLength == 0) 
	{
		srb.SrbFlags = 0;
	}
	else 
	{
		//
		// Flush the data buffer for output. This will insure that the data is
		// written back to memory.
		//

		KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
	}

#if 0
	srb.SrbFlags |= logicalUnit->CommonExtension.SrbFlags;
	srb.SrbFlags |= (SRB_FLAGS_DISABLE_SYNCH_TRANSFER & DeviceExtension->CommonExtension.SrbFlags);
#endif
	srb.SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
	srb.SrbFlags |= SRB_FLAGS_NO_QUEUE_FREEZE;
	srb.DataTransferLength = ScsiPassThroughData->DataTransferLength;
	srb.TimeOutValue = ScsiPassThroughData->TimeOutValue;
	srb.DataBuffer = (PCHAR) buffer + bufferOffset;
	srb.SenseInfoBuffer = senseBuffer;

	srb.OriginalRequest = irp;

	RtlCopyMemory(
		srb.Cdb, 
		ScsiPassThroughData->Cdb, 
		ScsiPassThroughData->CdbLength);

	//
	// Disable autosense if there's no sense buffer to put the data in.
	//

	if (senseBuffer == NULL) 
	{
		srb.SrbFlags |= SRB_FLAGS_DISABLE_AUTOSENSE;
	}

	//
	// Call port driver to handle this request.
	//

	status = IoCallDriver(PdoExtension->DeviceObject, irp);

	//
	// Wait for request to complete.
	//

	if (status == STATUS_PENDING) 
	{
		KeWaitForSingleObject(&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);
	}
	else 
	{
		ioStatusBlock.Status = status;
	}

	//
	// Copy the returned values from the srb to the control structure.
	//

	ScsiPassThroughData->ScsiStatus = srb.ScsiStatus;

	if (srb.SrbStatus  & SRB_STATUS_AUTOSENSE_VALID) 
	{
		//
		// Set the status to success so that the data is returned.
		//

		ioStatusBlock.Status = STATUS_SUCCESS;
		ScsiPassThroughData->SenseInfoLength = srb.SenseInfoBufferLength;

		//
		// Copy the sense data to the system buffer.
		//

		RtlCopyMemory(
			(PUCHAR) ScsiPassThroughData + ScsiPassThroughData->SenseInfoOffset,
			senseBuffer,
			srb.SenseInfoBufferLength);
	}
	else 
	{
		ScsiPassThroughData->SenseInfoLength = 0;
	}

	//
	// Free the sense buffer.
	//

	if (senseBuffer != NULL) 
	{
		ExFreePool(senseBuffer);
	}

	//
	// If the srb status is buffer underrun then set the status to success.
	// This insures that the data will be returned to the caller.
	//

	if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) 
	{
		ioStatusBlock.Status = STATUS_SUCCESS;
	}

	ScsiPassThroughData->DataTransferLength = srb.DataTransferLength;

	//
	// Set the information length
	//

	if (!ScsiPassThroughData->DataIn || bufferOffset == 0) 
	{
		RequestIrp->IoStatus.Information = 
			ScsiPassThroughData->SenseInfoOffset +
			ScsiPassThroughData->SenseInfoLength;

	}
	else 
	{
		RequestIrp->IoStatus.Information = 
			ScsiPassThroughData->DataBufferOffset +
			ScsiPassThroughData->DataTransferLength;
	}

	RequestIrp->IoStatus.Status = ioStatusBlock.Status;

	ASSERT(pTestFlag(srb.SrbStatus, SRB_STATUS_QUEUE_FROZEN) == 0);

	return ioStatusBlock.Status;
}

//NTSTATUS
//NdasPortPdoScsiPassThrough(
//	__in PNDASPORT_PDO_EXTENSION PdoExtension,
//	__inout_bcount(max(InputBufferLength,OutputBufferLength)) 
//		PSCSI_PASS_THROUGH ScsiPassThroughData,
//	__in ULONG InputBufferLength,
//	__in ULONG OutputBufferLength)
//{
//	PIRP                    irp;
//	PIO_STACK_LOCATION      irpStack;
//	PSCSI_PASS_THROUGH      srbControl;
//	SCSI_REQUEST_BLOCK      srb;
//	KEVENT                  event;
//	LARGE_INTEGER           startingOffset;
//	IO_STATUS_BLOCK         ioStatusBlock;
//	KIRQL                   currentIrql;
//	ULONG                   length;
//	ULONG                   bufferOffset;
//	PVOID                   buffer;
//	PVOID                   senseBuffer;
//	UCHAR                   majorCode;
//	NTSTATUS                status;
//
//	PAGED_CODE();
//
//	startingOffset.QuadPart = 1LL;
//
//	if (InputBufferLength < sizeof(SCSI_PASS_THROUGH))
//	{
//		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
//			"Input buffer length is too small, length=%#08lx\n", 
//			InputBufferLength);
//		return STATUS_BUFFER_TOO_SMALL;
//	}
//
//	if (ScsiPassThroughData->Length != sizeof(SCSI_PASS_THROUGH) &&
//		ScsiPassThroughData->Length != sizeof(SCSI_PASS_THROUGH_DIRECT))
//	{
//		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
//			"SCSI_PASS_THROUGH.Length is invalid, Length=%#08lx\n", 
//			ScsiPassThroughData->Length);
//		return STATUS_INVALID_PARAMETER;
//	}
//
//	//
//	// Cdb length must be less than or equal to 16
//	//
//	if (ScsiPassThroughData->CdbLength > 16)
//	{
//		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
//			"CdbLength is invalid, CdbLength=%#08lx\n", 
//			ScsiPassThroughData->CdbLength);
//		return STATUS_BUFFER_TOO_SMALL;
//	}
//
//	//
//	// Validate the sense buffer.
//	// Sense buffer should reside after ScsiPassThroughData->Length
//	// and before the data buffer
//	//
//	if (ScsiPassThroughData->SenseInfoLength > 0)
//	{
//		if (ScsiPassThroughData->SenseInfoOffset < ScsiPassThroughData->Length)
//		{
//			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
//				"SenseInfoOffset is within SCSI_PASSTHROUGH_DATA, Offset=%%08lx\n", 
//				ScsiPassThroughData->SenseInfoOffset);
//			return STATUS_INVALID_PARAMETER;
//		}
//
//		if (ScsiPassThroughData->DataTransferLength > 0 &&
//			ScsiPassThroughData->SenseInfoOffset + ScsiPassThroughData->SenseInfoLength >
//			ScsiPassThroughData->DataBufferOffset)
//		{
//			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
//				"SenseInfoOffset is after DataBufferOffset, Offset=%%08lx\n", 
//				ScsiPassThroughData->SenseInfoOffset);
//		}
//	}
//
//	switch (ScsiPassThroughData->DataIn)
//	{
//	case SCSI_IOCTL_DATA_IN:
//	case SCSI_IOCTL_DATA_OUT:
//	case SCSI_IOCTL_DATA_UNSPECIFIED:
//	default:
//		return STATUS_INVALID_PARAMETER;
//	}
//
//	majorCode = !ScsiPassThroughData->DataIn ? IRP_MJ_WRITE : IRP_MJ_READ;
//
//	if (0 == ScsiPassThroughData->DataTransferLength)
//	{
//		length = 0;
//		buffer = NULL;
//		bufferOffset = 0;
//		majorCode = IRP_MJ_FLUSH_BUFFERS;
//	}
//	else if (ScsiPassThroughData->DataBufferOffset > OutputBufferLength &&
//		ScsiPassThroughData->DataBufferOffset > InputBufferLength)
//	{
//		//
//		// Data buffer offset is greater than system buffer.
//		// Assume this is a user-mode address
//		//
//
//		if (ScsiPassThroughData->SenseInfoOffset + 
//			ScsiPassThroughData->SenseInfoLength > OutputBufferLength &&
//			0 != ScsiPassThroughData->SenseInfoLength)
//		{
//			NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
//				"Sense buffer is not in IOCTL buffer\n");
//			return STATUS_INVALID_PARAMETER;
//		}
//
//		//
//		// Buffer alignment
//		//
//
//		if (ScsiPassThroughData->DataBufferOffset &
//			PdoExtension->DeviceObject->AlignmentRequirement)
//		{
//
//		}
//	}
//
//	KeInitializeEvent(&event, NotificationEvent, FALSE);
//
//	__try
//	{
//		irp = IoBuildSynchronousFsdRequest(
//			IRP_MJ_READ,
//			PdoExtension->DeviceObject,
//			)
//	}
//	__except (EXCEPTION_EXECUTE_HANDLER)
//	{
//		NTSTATUS exceptionCode;
//
//		exceptionCode = GetExceptionCode();
//
//		return exceptionCode;
//	}
//
//	if (NULL == irp)
//	{
//		if (NULL != senseBuffer)
//		{
//			ExFreePoolWithTag(senseBuffer, 0);
//		}
//
//		return STATUS_INSUFFICIENT_RESOURCES;
//	}
//
//	NTSTATUS status;
//	PIO_STACK_LOCATION irpStack;
//	SCSI_REQUEST_BLOCK srb;
//
//	PVOID buffer;
//	ULONG bufferOffset;
//	PVOID senseBuffer;
//
//	irpStack = IoGetNextIrpStackLocation(irp);
//
//	irpStack->MajorFunction = IRP_MJ_SCSI;
//	irpStack->MinorFunction = IRP_MN_SCSI_CLASS;
//	irpStack->Parameters.Others.Argument1 = &srb;
//	RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));
//	srb.Length = SCSI_REQUEST_BLOCK_SIZE;
//	srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
//	srb.PathId = ScsiPassThroughData->PathId;
//	srb.TargetId = ScsiPassThroughData->TargetId;
//	srb.Lun = ScsiPassThroughData->Lun;
//	srb.CdbLength = ScsiPassThroughData->CdbLength;
//	srb.SenseInfoBufferLength = ScsiPassThroughData->SenseInfoLength;
//
//	switch (ScsiPassThroughData->DataIn)
//	{
//	case SCSI_IOCTL_DATA_OUT:
//		if (ScsiPassThroughData->DataTransferLength)
//		{
//			srb.SrbFlags = SRB_FLAGS_DATA_OUT;
//		}
//		break;
//	case SCSI_IOCTL_DATA_IN:
//		if (ScsiPassThroughData->DataTransferLength)
//		{
//			srb.SrbFlags = SRB_FLAGS_DATA_IN;
//		}
//		break;
//	default:
//		srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT;
//		break;
//	}
//
//	if (0 == ScsiPassThroughData->DataTransferLength)
//	{
//		srb.SrbFlags = 0;
//	}
//	else
//	{
//		//
//		// We do not actually perform DMA transfer at the host's side
//		// So we don't have to flush io buffers here
//		//
//		// KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
//	}
//
//	srb.SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
//	srb.SrbFlags |= SRB_FLAGS_NO_QUEUE_FREEZE;
//	srb.DataTransferLength = ScsiPassThroughData->DataTransferLength;
//	srb.TimeOutValue = ScsiPassThroughData->TimeOutValue;
//	srb.DataBuffer = (PCHAR) buffer + bufferOffset;
//	srb.SenseInfoBuffer = senseBuffer;
//	srb.OriginalRequest = irp;
//	RtlCopyMemory(
//		srb.Cdb, 
//		ScsiPassThroughData->Cdb, 
//		ScsiPassThroughData->CdbLength);
//
//	if (NULL == senseBuffer)
//	{
//		srb.SrbFlags |= SRB_FLAGS_DISABLE_AUTOSENSE;
//	}
//
//	status = IoCallDriver(PdoExtension->DeviceObject, irp);
//
//	if (STATUS_PENDING == status)
//	{
//		KeWaitForSingleObject(
//			&event,
//			Executive,
//			KernelMode,
//			FALSE,
//			NULL);
//	}
//	else
//	{
//		
//	}
//}

NTSTATUS
NdasPortPdoDispatchDeviceControl(
	__in PDEVICE_OBJECT Pdo,
	__in PIRP Irp)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION PdoExtension;
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	PIO_STACK_LOCATION irpStack;
	ULONG ioControlCode;
	ULONG isRemoved;

	CommonExtension = (PNDASPORT_COMMON_EXTENSION) Pdo->DeviceExtension;

	isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);
	if (isRemoved)
	{		
		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension, 
			Irp, 
			STATUS_DEVICE_DOES_NOT_EXIST);
	}

	PdoExtension = (PNDASPORT_PDO_EXTENSION) Pdo->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

	NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"PDO: IOCTL %s(%08x) (f%04X,a%04X,c%04X,m%04X), Pdo=%p, Irp=%p\n", 
		DbgIoControlCodeStringA(ioControlCode),
		ioControlCode, 
		ioControlCode>>16,
		(ioControlCode>>14&0x0003),
		(ioControlCode>>2&0x0FFF),
		(ioControlCode&0x0003),
		Pdo,
		Irp);

	switch (ioControlCode) 
	{
	case IOCTL_SCSI_GET_CAPABILITIES:
		{
			//
			// Fails this IRP sent to the PDO.
			// FDO handles this IOCTL when it is sent to FDO directly.
			//
			Irp->IoStatus.Information = 0;
			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				STATUS_NOT_SUPPORTED);
		}
	case IOCTL_STORAGE_QUERY_PROPERTY: 
		{
			//
			// Validate the query
			//
			PSTORAGE_PROPERTY_QUERY query;
			query = (PSTORAGE_PROPERTY_QUERY) Irp->AssociatedIrp.SystemBuffer;

			if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
				FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters))
			{
				return NpReleaseRemoveLockAndCompleteIrp(
					CommonExtension,
					Irp,
					STATUS_INVALID_PARAMETER);
			}

			return NdasPortPdoQueryProperty(PdoExtension, Irp);
		}
	case IOCTL_SCSI_GET_ADDRESS: 
		{
			PSCSI_ADDRESS scsiAddress;

			scsiAddress = (PSCSI_ADDRESS) Irp->AssociatedIrp.SystemBuffer;

			if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(SCSI_ADDRESS)) 
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			else
			{
				scsiAddress->Length = sizeof(SCSI_ADDRESS);
				scsiAddress->PortNumber = PdoExtension->LogicalUnitAddress.PortNumber;
				scsiAddress->PathId = PdoExtension->LogicalUnitAddress.PathId;
				scsiAddress->TargetId = PdoExtension->LogicalUnitAddress.TargetId;
				scsiAddress->Lun = PdoExtension->LogicalUnitAddress.Lun;
				Irp->IoStatus.Information = sizeof(SCSI_ADDRESS);
				status = STATUS_SUCCESS;
			}
		}

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			status);

	case IOCTL_SCSI_MINIPORT:

		status = NdasPortPdoMiniportIoctl(PdoExtension, Irp);

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			status);

	case IOCTL_NDASPORT_LOGICALUNIT_GET_ADDRESS:
		{
			PNDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;

			logicalUnitAddress = (PNDAS_LOGICALUNIT_ADDRESS) Irp->AssociatedIrp.SystemBuffer;

			if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(NDAS_LOGICALUNIT_ADDRESS))
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			else
			{
				*logicalUnitAddress = PdoExtension->LogicalUnitAddress;
				Irp->IoStatus.Information = sizeof(NDAS_LOGICALUNIT_ADDRESS);
				status = STATUS_SUCCESS;
			}
		}

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			status);

	case IOCTL_NDASPORT_LOGICALUNIT_EXIST:
		{
			PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
			ULONG outputBufferLength = 
				irpStack->Parameters.DeviceIoControl.OutputBufferLength;

			ULONG_PTR information = 0;

			//
			// Optionally, if the output buffer is available,
			// we provides the IDENTIFY GUID. Callers can verify
			// the identify more accurately.
			//
			if (outputBufferLength >= sizeof(GUID))
			{
				RtlCopyMemory(
					outputBuffer,
					&GUID_NDASPORT_LOGICALUNIT_IDENTITY,
					sizeof(GUID));
				information = sizeof(GUID);
			}

			return NpReleaseRemoveLockAndCompleteIrpEx(
				CommonExtension,
				Irp,
				STATUS_SUCCESS,
				information,
				IO_NO_INCREMENT);
		}

	case IOCTL_NDASPORT_LOGICALUNIT_GET_DESCRIPTOR:
		{
			PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
			ULONG outputBufferLength = 
				irpStack->Parameters.DeviceIoControl.OutputBufferLength;

			if (outputBufferLength < sizeof(NDAS_LOGICALUNIT_DESCRIPTOR))
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			else
			{
				outputBufferLength = min(
					outputBufferLength,
					PdoExtension->LogicalUnitDescriptor->Size);
				RtlCopyMemory(
					outputBuffer,
					PdoExtension->LogicalUnitDescriptor,
					outputBufferLength);
				Irp->IoStatus.Information = outputBufferLength;
				status = STATUS_SUCCESS;
			}
		}
		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			status);

	case IOCTL_SCSI_PASS_THROUGH:
	case IOCTL_SCSI_PASS_THROUGH_DIRECT:
		{
			PSCSI_PASS_THROUGH scsiPassThrough;
			ULONG inputBufferLength, outputBufferLength;

			scsiPassThrough = (PSCSI_PASS_THROUGH) Irp->AssociatedIrp.SystemBuffer;
			inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
			outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

			status = NdasPortPdoScsiPassThrough(
				PdoExtension,
				Irp,
				scsiPassThrough,
				inputBufferLength, 
				outputBufferLength);

			if (STATUS_PENDING == status)
			{
				IoMarkIrpPending(Irp);
				return STATUS_PENDING;
			}

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}

	case IOCTL_ATA_PASS_THROUGH:
	case IOCTL_ATA_PASS_THROUGH_DIRECT:

	default:

		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
			"PDO DeviceControl: Passing down to the device object=%p\n",
			CommonExtension->LowerDeviceObject);

		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);
	}
}

NTSTATUS
NdasPortPdoPnpDefault(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStack;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_WARNING,
		"Not handling PDO: MinorFunctionCode=%x(%s) Status=0x%08x\n",
		(irpStack->MinorFunction),
		DbgPnPMinorFunctionStringA(irpStack->MinorFunction),
		Irp->IoStatus.Status);

	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension,
		Irp,
		Irp->IoStatus.Status,
		Irp->IoStatus.Information,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpStartDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	KIRQL oldIrql;

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
		"NdasPortPdoPnpStartDevice: PDO=%p\n", PdoExtension->DeviceObject);

	//            
	// Here we do what ever initialization and ``turning on'' that is
	// required to allow others to access this device.
	// Power up the device.
	//

	PdoExtension->CommonExtension->IsInitialized = TRUE;

	NpSetNewPnpState(PdoExtension->CommonExtension, Started);

	KeAcquireSpinLock(&PdoExtension->CommonExtension->PowerStateSpinLock, &oldIrql);
	PdoExtension->CommonExtension->DevicePowerState = PowerDeviceD0;
	PdoExtension->CommonExtension->SystemPowerState = PowerSystemWorking;
	KeReleaseSpinLock(&PdoExtension->CommonExtension->PowerStateSpinLock, oldIrql);

	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension,
		Irp,
		STATUS_SUCCESS,
		0,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpQueryRemoveDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// Check to see whether the device can be removed safely.
	// If not fail this request. This is the last opportunity
	// to do so.
	//

	//if(DeviceData->ToasterInterfaceRefCount)
	//{
	//	//
	//	// Somebody is still using our interface. 
	//	// We must fail remove.
	//	//
	//	status = STATUS_UNSUCCESSFUL;
	//	break;
	//}

	NpSetNewPnpState(PdoExtension->CommonExtension, RemovePending);

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);

}

NTSTATUS
NdasPortPdoPnpRemoveDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	//
	// Present is set to true when the pdo is exposed via PlugIn IOCTL.
	// It is set to FALSE when a UnPlug IOCTL is received. 
	// We will delete the PDO only after we have reported to the 
	// Plug and Play manager that it's missing.
	//

	if (PdoExtension->ReportedMissing)
	{
		NpSetNewPnpState(PdoExtension->CommonExtension, Deleted);

		//
		// Remove the PDO from the list and decrement the count of PDO.
		// Don't forget to synchronize access to the FDO data.
		// If the parent FDO is deleted before child PDOs, the ParentFdo
		// pointer will be NULL. This could happen if the child PDO
		// is in a SurpriseRemovePending state when the parent FDO
		// is removed.
		//

		if(PdoExtension->ParentFDOExtension)
		{
			PNDASPORT_FDO_EXTENSION fdoExtension;
			fdoExtension = PdoExtension->ParentFDOExtension;
			ExAcquireFastMutex (&fdoExtension->Mutex);
			RemoveEntryList (&PdoExtension->Link);
			--fdoExtension->NumberOfPDOs;
			ExReleaseFastMutex (&fdoExtension->Mutex);
		}

		//
		// Free up resources associated with PDO and delete it.
		//

		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
			"Already reported missing! Destroying...\n");

		NdasPortPdoRemoveDevice(PdoExtension);

	} 
	else if (PdoExtension->Present)
	{
		//
		// When the device is disabled, the PDO transitions from 
		// RemovePending to NotStarted. We shouldn't delete
		// the PDO because a) the device is still present on the bus,
		// b) we haven't reported missing to the PnP manager.
		//
		PdoExtension->IsClaimed = FALSE;
		NpSetNewPnpState(PdoExtension->CommonExtension, NotStarted);

		//
		// TODO: Report that the device is not started!
		//
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
			"Still present. Changed to NotStarted...\n");
	}
	else
	{
		ASSERT(FALSE);
	}

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);

}

NTSTATUS
NdasPortPdoPnpCancelRemoveDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// Clean up a remove that did not go through.
	//

	//
	// First check to see whether you have received cancel-remove
	// without first receiving a query-remove. This could happen if 
	// someone above us fails a query-remove and passes down the 
	// subsequent cancel-remove.
	//

	if(RemovePending == PdoExtension->CommonExtension->DevicePnPState)
	{
		//
		// We did receive a query-remove, so restore.
		//             
		NpRestorePreviousPnpState(PdoExtension->CommonExtension);
	}

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);

}

NTSTATUS
NdasPortPdoPnpStopDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// Here we shut down the device and give up and unmap any resources
	// we acquired for the device.
	//

	NpSetNewPnpState(PdoExtension->CommonExtension, Stopped);

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension,
		Irp, 
		STATUS_SUCCESS);
}

NTSTATUS
NdasPortPdoPnpQueryStopDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// No reason here why we can't stop the device.
	// If there were a reason we should speak now, because answering success
	// here may result in a stop device irp.
	//

	NpSetNewPnpState(PdoExtension->CommonExtension, StopPending);

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);
}

NTSTATUS
NdasPortPdoPnpCancelStopDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// The stop was canceled.  Whatever state we set, or resources we put
	// on hold in anticipation of the forthcoming STOP device IRP should be
	// put back to normal.  Someone, in the long list of concerned parties,
	// has failed the stop device query.
	//

	//
	// First check to see whether you have received cancel-stop
	// without first receiving a query-stop. This could happen if someone
	// above us fails a query-stop and passes down the subsequent
	// cancel-stop.
	//

	if (StopPending == PdoExtension->CommonExtension->DevicePnPState)
	{
		//
		// We did receive a query-stop, so restore.
		//             
		NpRestorePreviousPnpState(PdoExtension->CommonExtension);
	}

	//
	// We must not fail this IRP.
	//
	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);
}

NTSTATUS
NdasPortPdoPnpQueryDeviceRelations(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	PDEVICE_RELATIONS deviceRelations;
	PIO_STACK_LOCATION irpStack;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	if (TargetDeviceRelation != irpStack->Parameters.QueryDeviceRelations.Type)
	{
		//
		// BusRelation : Not handled by PDO
		// EjectionRelations: Optional for PDO
		// RemovalRelations: Optional for PDO
		//

		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
			"Not TargetDevicesRelations for PDO\n");

		return NpReleaseRemoveLockAndCompleteIrp(
			PdoExtension->CommonExtension, 
			Irp, 
			Irp->IoStatus.Status);
	}

	//
	// DEVICE_RELATIONS definition contains one object pointer.
	//

	deviceRelations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag(
		PagedPool,
		sizeof(DEVICE_RELATIONS),
		NDASPORT_TAG_DEVICE_RELATIONS);

	if (NULL == deviceRelations)
	{
		NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_FATAL,
			"Out of memory for allocating deviceRelations (%d byets)\n",
			sizeof(DEVICE_RELATIONS));

		return NpReleaseRemoveLockAndCompleteIrp(
			PdoExtension->CommonExtension, 
			Irp, 
			STATUS_INSUFFICIENT_RESOURCES);
	}

	RtlZeroMemory(
		deviceRelations, 
		sizeof(DEVICE_RELATIONS));

	deviceRelations->Count = 1;
	deviceRelations->Objects[0] = PdoExtension->DeviceObject;

	ObReferenceObject(deviceRelations->Objects[0]);

	Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);
}


NTSTATUS
NdasPortPdoPnpQueryInterface(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NdasPortPdoPnpDefault(PdoExtension, Irp);
}

NTSTATUS
NdasPortPdoPnpQueryCapabilities(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStack;
	PDEVICE_CAPABILITIES deviceCapabilities;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	deviceCapabilities = irpStack->Parameters.DeviceCapabilities.Capabilities;

	status = PdoExtension->LogicalUnitInterface.QueryPnpDeviceCapabilities(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		deviceCapabilities);

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
		"QueryPnpDeviceCapabilities returned Status=%08X\n", status);

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension,
		Irp,
		status);

}

NTSTATUS
NdasPortPdoPnpQueryResources(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension,
		Irp,
		Irp->IoStatus.Status,
		Irp->IoStatus.Information,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpQueryResourceRequirements(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension,
		Irp,
		Irp->IoStatus.Status,
		Irp->IoStatus.Information,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpQueryDeviceText(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStack;
	PWCHAR deviceText = NULL;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
		"QueryDeviceText Type=%s(%X), Locale=0x%X\n",
		DbgDeviceTextTypeStringA(irpStack->Parameters.QueryDeviceText.DeviceTextType),
		irpStack->Parameters.QueryDeviceText.DeviceTextType,
		irpStack->Parameters.QueryDeviceText.LocaleId);

	status = PdoExtension->LogicalUnitInterface.QueryPnpDeviceText(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		irpStack->Parameters.QueryDeviceText.DeviceTextType, 
		irpStack->Parameters.QueryDeviceText.LocaleId,
		&deviceText);

	if (WPP_FLAG_LEVEL_ENABLED(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION))
	{
		if (NT_SUCCESS(status))
		{
			NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
				"DeviceText: %ws\n", deviceText);
		}
		else
		{
			NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_ERROR,
				"QueryPnpDeviceText failed, Status=%08X\n",
				status);
		}
	}

	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension, 
		Irp, 
		status,
		(ULONG_PTR) deviceText,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpFilterResourceRequirements(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NdasPortPdoPnpDefault(PdoExtension, Irp);
}

NTSTATUS
NdasPortPdoPnpReadConfig(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NdasPortPdoPnpDefault(PdoExtension, Irp);
}

NTSTATUS
NdasPortPdoPnpWriteConfig(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NdasPortPdoPnpDefault(PdoExtension, Irp);
}

NTSTATUS
NdasPortPdoPnpEject(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// For the device to be ejected, the device must be in the D3 
	// device power state (off) and must be unlocked 
	// (if the device supports locking). Any driver that returns success 
	// for this IRP must wait until the device has been ejected before 
	// completing the IRP.
	//
	PdoExtension->Present = FALSE;

	//
	// There are two passes for logical unit removal.
	//
	// 1. EJECT -> REMOVE
	// 2. REMOVE
	// 
	// The first case is the user request for ejection.
	// The latter case happens also when the ndasport is disabled,
	// where we want to retain the entires of the logical units.
	// Hence, we only delete the entry for IRP_MN_EJECT request.
	//
	// Note that UnplugDevice will also delete the entry.
	//

	NdasPortFdoRegDeleteLogicalUnit(
		PdoExtension->ParentFDOExtension,
		PdoExtension->LogicalUnitAddress);

	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension,
		Irp,
		STATUS_SUCCESS,
		0,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpSetLock(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NdasPortPdoPnpDefault(PdoExtension, Irp);
}

NTSTATUS
NdasPortPdoPnpQueryID(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStack;
	BUS_QUERY_ID_TYPE idType;
	DECLARE_UNICODE_STRING_SIZE(unicodeStringId, 128);
	size_t length, requiredBytes;
	PWCHAR deviceIds, nextDeviceId;
	ULONG index, deviceIdCount;

	irpStack = IoGetCurrentIrpStackLocation(Irp);
	idType = irpStack->Parameters.QueryId.IdType;
	length = 0;
	requiredBytes = 0;
	for (index = 0; ; ++index)
	{
		status = PdoExtension->LogicalUnitInterface.QueryPnpID(
			NdasPortPdoGetLogicalUnitExtension(PdoExtension),
			idType,
			index,
			&unicodeStringId);
		if (STATUS_NO_MORE_ENTRIES == status)
		{
			break;
		}
		else if (!NT_SUCCESS(status))
		{
			return NpReleaseRemoveLockAndCompleteIrpEx(
				PdoExtension->CommonExtension,
				Irp,
				status,
				0,
				IO_NO_INCREMENT);
		}
		requiredBytes += unicodeStringId.Length;
		requiredBytes += sizeof(WCHAR);
	}
	requiredBytes += sizeof(WCHAR);

	deviceIdCount = index;

	if (BusQueryCompatibleIDs == idType || 
		BusQueryHardwareIDs == idType)
	{
		requiredBytes += sizeof(WCHAR); // MULTI_SZ
	}
	deviceIds = (PWCHAR) ExAllocatePoolWithTag(
		PagedPool,
		requiredBytes,
		NDASPORT_TAG_PNP_ID);
	if (NULL == deviceIds)
	{
		return NpReleaseRemoveLockAndCompleteIrpEx(
			PdoExtension->CommonExtension,
			Irp,
			STATUS_INSUFFICIENT_RESOURCES,
			0,
			IO_NO_INCREMENT);
	}
	RtlZeroMemory(deviceIds, requiredBytes);
	nextDeviceId = deviceIds;
	for (index = 0; index < deviceIdCount; ++index)
	{
		status = PdoExtension->LogicalUnitInterface.QueryPnpID(
			NdasPortPdoGetLogicalUnitExtension(PdoExtension),
			idType,
			index,
			&unicodeStringId);
		ASSERT(NT_SUCCESS(status));
		RtlCopyMemory(
			nextDeviceId,
			unicodeStringId.Buffer,
			unicodeStringId.Length);
		NdasPortFixDeviceId(
			nextDeviceId,
			unicodeStringId.Length);
		nextDeviceId = (PWCHAR) NdasPortOffsetOf(
			nextDeviceId, 
			unicodeStringId.Length);
		++nextDeviceId; /* null character */
		ASSERT((PVOID)nextDeviceId <= NdasPortOffsetOf(deviceIds, requiredBytes));
	}
	ASSERT((PVOID)nextDeviceId <= NdasPortOffsetOf(deviceIds, requiredBytes));

	if (WPP_FLAG_LEVEL_ENABLED(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION))
	{
		if (NT_SUCCESS(status))
		{
			switch (irpStack->Parameters.QueryId.IdType)
			{
			case BusQueryDeviceID:
				NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
					"BusQueryDeviceID: %ws\n", deviceIds);
				break;
			case BusQueryInstanceID:
				NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
					"BusQueryInstanceID: %ws\n", deviceIds);
				break;
			case BusQueryHardwareIDs:
				{
					PWCHAR next = deviceIds;
					while (L'\0' != *next)
					{
						NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
							"BusQueryHardwareID: %ws\n", next);
						while (L'\0' != *next) ++next;
						++next;
					}
				}
				break;
			case BusQueryCompatibleIDs:
				{
					PWCHAR next = deviceIds;
					while (*next)
					{
						NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
							"BusQueryCompatibleID: %ws\n", next);
						while (*next) ++next;
						++next;
					}
				}
				break;
			}
		}
		else
		{
			NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_ERROR,
				"QueryPnpID failed, Status=%08X\n",
				status);
		}
	}

	return NpReleaseRemoveLockAndCompleteIrpEx(
		PdoExtension->CommonExtension, 
		Irp, 
		status,
		(ULONG_PTR) deviceIds,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasPortPdoPnpQueryPnpDeviceState(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	//
	// OPTIONAL for bus drivers.
	// The PnP Manager sends this IRP after the drivers for 
	// a device return success from the IRP_MN_START_DEVICE 
	// request. The PnP Manager also sends this IRP when a 
	// driver for the device calls IoInvalidateDeviceState.
	//
	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);
}

NTSTATUS
NdasPortPdoPnpQueryBusInformation(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NpReleaseRemoveLockAndForwardIrp(
		PdoExtension->CommonExtension,
		Irp);
}

NTSTATUS
NdasPortPdoPnpDeviceUsageNotification(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{

	//
	// OPTIONAL for bus drivers.
	// This bus drivers any of the bus's descendants 
	// (child device, child of a child device, etc.) do not 
	// contain a memory file namely paging file, dump file, 
	// or hibernation file. So we fail this Irp.
	//

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension,
		Irp,
		STATUS_UNSUCCESSFUL);
}

NTSTATUS
NdasPortPdoPnpSurpriseRemoval(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	NTSTATUS status;
	LONG wmiRegistered;

	//
	// We should stop all access to the device and relinquish all the
	// resources. Let's just mark that it happened and we will do 
	// the cleanup later in IRP_MN_REMOVE_DEVICE.
	//


	//
	// If a device is removed suddenly (for example, in a surprise removal), 
	// causing the PnP manager to send an IRP_MN_SURPRISE_REMOVAL IRP, 
	// the driver must call IoWMIRegistrationControl and specify 
	// WMIREG_ACTION_DEREGISTER in Action in the call.
	//

	wmiRegistered = InterlockedCompareExchange(
		&PdoExtension->WmiRegistered,
		FALSE,
		TRUE);

	if (wmiRegistered)
	{
		status = IoWMIRegistrationControl(
			PdoExtension->DeviceObject,
			WMIREG_ACTION_DEREGISTER);
	}

	NpSetNewPnpState(PdoExtension->CommonExtension, SurpriseRemovePending);

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension, 
		Irp, 
		STATUS_SUCCESS);
}

NTSTATUS
NdasPortPdoPnpQueryLegacyBusInformation(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp)
{
	return NdasPortPdoPnpDefault(PdoExtension, Irp);
}

typedef
NTSTATUS
(*PNDASPORT_PDO_DISPATCH)(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP Irp);

PNDASPORT_PDO_DISPATCH PdoPnpDispatch[] = {
	NdasPortPdoPnpStartDevice,         /* IRP_MN_START_DEVICE */ 
	NdasPortPdoPnpQueryRemoveDevice,   /* IRP_MN_QUERY_REMOVE_DEVICE */
	NdasPortPdoPnpRemoveDevice,        /* IRP_MN_REMOVE_DEVICE */
	NdasPortPdoPnpCancelRemoveDevice,  /* IRP_MN_CANCEL_REMOVE_DEVICE */
	NdasPortPdoPnpStopDevice,          /* IRP_MN_STOP_DEVICE */
	NdasPortPdoPnpQueryStopDevice,     /* IRP_MN_QUERY_STOP_DEVICE */
	NdasPortPdoPnpCancelStopDevice,    /* IRP_MN_CANCEL_STOP_DEVICE */
	NdasPortPdoPnpQueryDeviceRelations, /* IRP_MN_QUERY_DEVICE_RELATIONS */
	NdasPortPdoPnpQueryInterface,      /* IRP_MN_QUERY_INTERFACE */
	NdasPortPdoPnpQueryCapabilities,   /* IRP_MN_QUERY_CAPABILITIES */
	NdasPortPdoPnpQueryResources,      /* IRP_MN_QUERY_RESOURCES */
	NdasPortPdoPnpQueryResourceRequirements,/* IRP_MN_QUERY_RESOURCE_REQUIREMENTS */
	NdasPortPdoPnpQueryDeviceText,     /* IRP_MN_QUERY_DEVICE_TEXT */
	NdasPortPdoPnpFilterResourceRequirements,/* IRP_MN_FILTER_RESOURCE_REQUIREMENTS */
	NdasPortPdoPnpDefault,             /* -- HOLE -- */
	NdasPortPdoPnpReadConfig,          /* IRP_MN_READ_CONFIG */
	NdasPortPdoPnpWriteConfig,         /* IRP_MN_WRITE_CONFIG */
	NdasPortPdoPnpEject,               /* IRP_MN_EJECT */
	NdasPortPdoPnpSetLock,             /* IRP_MN_SET_LOCK */
	NdasPortPdoPnpQueryID,             /* IRP_MN_QUERY_ID */
	NdasPortPdoPnpQueryPnpDeviceState, /* IRP_MN_QUERY_PNP_DEVICE_STATE */
	NdasPortPdoPnpQueryBusInformation, /* IRP_MN_QUERY_BUS_INFORMATION */
	NdasPortPdoPnpDeviceUsageNotification,/*IRP_MN_DEVICE_USAGE_NOTIFICATION*/
	NdasPortPdoPnpSurpriseRemoval,      /* IRP_MN_SURPRISE_REMOVAL */
	NdasPortPdoPnpQueryLegacyBusInformation /* IRP_MN_QUERY_LEGACY_BUS_INFORMATION */
};

C_ASSERT(countof(PdoPnpDispatch) == (IRP_MN_QUERY_LEGACY_BUS_INFORMATION + 1));

NTSTATUS
NdasPortPdoDispatchPnp(
	__in PDEVICE_OBJECT LogicalUnit,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpStack;
	PNDASPORT_PDO_EXTENSION PdoExtension;
	ULONG isRemoved;

	PdoExtension = NdasPortPdoGetExtension(LogicalUnit);
	
	isRemoved = NpdoAcquireRemoveLock(PdoExtension, Irp);
	if (isRemoved)
	{
		return NpReleaseRemoveLockAndCompleteIrpEx(
			PdoExtension->CommonExtension,
			Irp,
			STATUS_DEVICE_DOES_NOT_EXIST,
			0,
			IO_NO_INCREMENT);
	}

	irpStack = IoGetCurrentIrpStackLocation(Irp);
	status = STATUS_SUCCESS;

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
		"PDO: PNP %s(%02Xh), Pdo=%p, Irp=%p\n",
		DbgPnPMinorFunctionStringA(irpStack->MinorFunction),
		(irpStack->MinorFunction),
		LogicalUnit,
		Irp);

	if (irpStack->MinorFunction < countof(PdoPnpDispatch))
	{
		return PdoPnpDispatch[irpStack->MinorFunction](PdoExtension, Irp);
	}
	else
	{
		// ASSERT(FALSE);
		return NdasPortPdoPnpDefault(PdoExtension, Irp);
	}
}

NTSTATUS
NdasPortPdoDispatchCreateClose(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{
	NTSTATUS status;
	PNDASPORT_COMMON_EXTENSION CommonExtension = 
		(PNDASPORT_COMMON_EXTENSION) DeviceObject->DeviceExtension;

	PIO_STACK_LOCATION irpStack;
	ULONG isRemoved;

	NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_INFORMATION,
		"NdasPortPdoDispatchCreateClose: Pdo=%p, Irp=%p\n",
		DeviceObject, Irp);

	status = STATUS_SUCCESS;
	irpStack = IoGetCurrentIrpStackLocation(Irp);

	isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);

	if (IRP_MJ_CREATE == irpStack->MajorFunction)
	{
		if (isRemoved)
		{
			status = STATUS_DEVICE_DOES_NOT_EXIST;
		}
	}

	return NpReleaseRemoveLockAndCompleteIrp(
		CommonExtension, 
		Irp, 
		status);
}
