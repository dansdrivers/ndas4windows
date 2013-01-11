/*++

This file contains plug and play code for the NDAS SCSI port driver.

--*/

#include "port.h"
#include <initguid.h>
#include <devguid.h>

VOID
NdasPortFdoInitializeIoScsiCapabilities(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__inout PIO_SCSI_CAPABILITIES IoScsiCapabilities);

VOID
NdasPortFdoInitializeStorageAdapterDescriptor(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__inout PSTORAGE_ADAPTER_DESCRIPTOR StorageAdapterDescriptor);

VOID
NdasPortFdoDeleteSymbolicLinks(
	__in PNDASPORT_FDO_EXTENSION FdoExtension);

NTSTATUS
NdasPortFdoCreateSymbolicLinks(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PUNICODE_STRING DeviceName);

NTSTATUS
NdasPortFdoLogicalUnitEnumCallback(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PVOID Context);

VOID
NdasPortFdopInitializeLogicalUnits(
	__in PNDASPORT_FDO_EXTENSION FdoExtension);

VOID
NdasPortFdopInitializeLogicalUnitCompletion(
	__in PNDASPORT_FDO_EXTENSION FdoExtension);

#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, NdasPortFdoAddDevice)
#pragma alloc_text(PAGE, NdasPortUnload)
#pragma alloc_text(PAGE, NdasPortFdoInitializeIoScsiCapabilities)
#pragma alloc_text(PAGE, NdasPortFdoInitializeStorageAdapterDescriptor)

#endif // ALLOC_PRAGMA

#ifdef RUN_WPP
#include "pnp.tmh"
#endif

PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY
NdasPortpCreateLogicalUnitDescriptorListEntry(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor)
{
	PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY descEntry;
	ULONG descEntrySize;
	PIO_WORKITEM initWorkItem;

	initWorkItem = IoAllocateWorkItem(FdoExtension->DeviceObject);

	if (NULL == initWorkItem)
	{
		NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
			"Allocating InitLogicalUnitWorkItem failed.\n");
		return NULL;
	}

	descEntrySize = 
		FIELD_OFFSET(NDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY, LogicalUnitDescriptor) +
		LogicalUnitDescriptor->Size;

	descEntry = (PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY) 
		ExAllocatePoolWithTag(
			PagedPool, descEntrySize, NDASPORT_TAG_LU_DESC);

	if (NULL == descEntry)
	{
		NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
			"Allocating ExAllocatePoolWithTag(%u bytes) failed.\n",
			descEntrySize);

		IoFreeWorkItem(initWorkItem);

		return NULL;
	}

	descEntry->InitWorkItem = initWorkItem;

	RtlCopyMemory(
		&descEntry->LogicalUnitDescriptor,
		LogicalUnitDescriptor,
		LogicalUnitDescriptor->Size);

	return descEntry;
}

VOID
NdasPortpDeleteLogicalUnitDescriptorListEntry(
	__in PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY DescriptorEntry)
{
	ASSERT(NULL != DescriptorEntry->InitWorkItem);
	IoFreeWorkItem(DescriptorEntry->InitWorkItem);
	ExFreePoolWithTag(DescriptorEntry, NDASPORT_TAG_LU_DESC);
}

VOID
NdasPortpDeleteLogicalUnitDescriptorList(
	__in PLIST_ENTRY LogicalUnitDescriptorListHead)
{
	PLIST_ENTRY entry;

	for (entry = RemoveHeadList(LogicalUnitDescriptorListHead);
		entry != LogicalUnitDescriptorListHead;
		entry = RemoveHeadList(LogicalUnitDescriptorListHead))
	{
		PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY descEntry;

		descEntry = CONTAINING_RECORD(
			entry, 
			NDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY,
			ListEntry);

		NdasPortpDeleteLogicalUnitDescriptorListEntry(descEntry);
	}
}

VOID
NdasPortFdoInitializeLogicalUnits(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	LONG queued, delegated;

	//
	// Debugging support to disable persistent logical units
	//
	// To disable persistent logical units, use:
	//
	//   ed ndasport!__DisablePersistentLogicalUnits 1
	//

	if (__DisablePersistentLogicalUnits)
	{
		NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_ERROR,
			"*** PersistentLogicalUnits are disabled.\n");
		return;
	}

	//
	// Check if the persistent pdo initialization is delegated,
	// 

	delegated = InterlockedCompareExchange(
		&FdoExtension->InitChildDelegated, 0, 0);

	if (delegated)
	{
		//
		// Do not queue any initialization
		//
		NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
			"NdasPortFdoInitializeLogicalUnits is now DELEGATED.\n");
		return;
	}

	queued = InterlockedIncrement(&FdoExtension->InitQueueCount);

	if (queued > 1)
	{
		//
		// Other initialization is in progress, 
		// initialization will be just queued.
		//

		NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
			"NdasPortFdoInitializeLogicalUnits queued, count=%d\n",
			queued);

		return;
	}
	else
	{
		ASSERT(KeReadStateEvent(&FdoExtension->InitChildCompletionEvent));
		KeClearEvent(&FdoExtension->InitChildCompletionEvent);

		NdasPortFdopInitializeLogicalUnits(FdoExtension);
	}
}

VOID
NdasPortFdopInitializeLogicalUnits(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	PLIST_ENTRY head, entry;
	LONG activeCount;

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
		"NdasPortFdopInitializeLogicalUnits initiated.\n");

	ExAcquireFastMutex(&FdoExtension->InitChildDescriptorListMutex);

	ASSERT(0 == FdoExtension->InitChildActiveCount);

	head = &FdoExtension->InitChildDescriptorList;

	for (entry = head->Flink; entry != head; entry = entry->Flink)
	{
		++FdoExtension->InitChildActiveCount;
	}

	if (0 == FdoExtension->InitChildActiveCount)
	{
		ExReleaseFastMutex(&FdoExtension->InitChildDescriptorListMutex);
		NdasPortFdopInitializeLogicalUnitCompletion(FdoExtension);
		return;
	}

	head = &FdoExtension->InitChildDescriptorList;

	for (entry = head->Flink; entry != head; entry = entry->Flink)
	{
		PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY descEntry;

		descEntry = CONTAINING_RECORD(
			entry,
			NDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY,
			ListEntry);

		IoQueueWorkItem(
			descEntry->InitWorkItem,
			NdasPortFdopInitializeLogicalUnitWorkItem,
			DelayedWorkQueue,
			descEntry);
	}

	ExReleaseFastMutex(&FdoExtension->InitChildDescriptorListMutex);
}

VOID
NdasPortFdopInitializeLogicalUnitWorkItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context)
{
	NTSTATUS status;
	PNDASPORT_FDO_EXTENSION FdoExtension;
	PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY TargetDescEntry;
	PLIST_ENTRY head, entry;
	LONG count;

	FdoExtension = NdasPortFdoGetExtension(DeviceObject);
	TargetDescEntry = (PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY) Context;

	status = NdasPortFdoDeviceControlPlugInLogicalUnit(
		FdoExtension, 
		&TargetDescEntry->LogicalUnitDescriptor);

	if (NT_SUCCESS(status))
	{
		ExAcquireFastMutex(&FdoExtension->InitChildDescriptorListMutex);

		RemoveEntryList(&TargetDescEntry->ListEntry);

		ExReleaseFastMutex(&FdoExtension->InitChildDescriptorListMutex);

		NdasPortpDeleteLogicalUnitDescriptorListEntry(TargetDescEntry);
	}

	count = InterlockedDecrement(&FdoExtension->InitChildActiveCount);
	ASSERT(count >= 0);

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
		" InitLogicalUnit completed, status=%x, remaining=%d\n", status, count);

	if (0 == count)
	{
		NdasPortFdopInitializeLogicalUnitCompletion(FdoExtension);
	}
}

VOID
NdasPortFdopInitializeLogicalUnitCompletion(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	LONG delegated, initQueueCount;

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
		"NdasPortFdopInitializeLogicalUnitCompletion\n");

	delegated = InterlockedCompareExchange(
		&FdoExtension->InitChildDelegated, 0, 0);

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
		" InitLogicalUnit completed, delegated=%d\n", delegated);

	if (delegated)
	{
		initQueueCount = FdoExtension->InitQueueCount = 0;
	}
	else
	{
		initQueueCount = InterlockedDecrement(&FdoExtension->InitQueueCount);
	}

	ASSERT(initQueueCount >= 0);

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
		" InitLogicalUnit completed, queued=%d\n", initQueueCount);

	if (initQueueCount > 0)
	{
		NdasPortFdopInitializeLogicalUnits(FdoExtension);
	}
	else
	{
		KeSetEvent(
			&FdoExtension->InitChildCompletionEvent,
			IO_NO_INCREMENT,
			FALSE);
	}
}

NTSTATUS
NdasPortFdoStart(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	NTSTATUS status;

	PAGED_CODE ();

	DebugPrint((3, "NdasPortFdoStart FDO=%p\n", FdoExtension->DeviceObject));

	//
	// Enable device interface. If the return status is 
	// STATUS_OBJECT_NAME_EXISTS means we are enabling the interface
	// that was already enabled, which could happen if the device 
	// is stopped and restarted for resource rebalancing.
	//

	status = IoSetDeviceInterfaceState(&FdoExtension->InterfaceName, TRUE);
	if (!NT_SUCCESS (status))
	{
		DebugPrint((1, "IoSetDeviceInterfaceState(%wZ) failed: 0x%x\n", &FdoExtension->InterfaceName, status));
		//return status;
	}

	NpSetNewPnpState(FdoExtension->CommonExtension, Started);
	ASSERT(FdoExtension->CommonExtension->DevicePnPState == Started);

	//
	// First attempt to mount the logical units
	//

	NdasPortFdoInitializeLogicalUnits(FdoExtension);

	status = STATUS_SUCCESS;

	return status;
}

VOID
NdasPortFdoStop(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	PNDASPORT_COMMON_EXTENSION CommonExtension = FdoExtension->CommonExtension;

	PAGED_CODE();

	DebugPrint((3, "NdasPortFdoStop FDO=%p\n", FdoExtension->DeviceObject));
}

VOID
NdasPortFdoDestroy(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	NTSTATUS status;
	PNDASPORT_COMMON_EXTENSION CommonExtension = FdoExtension->CommonExtension;

	PAGED_CODE();

	DebugPrint((3, "NdasPortFdoDestroy FDO=%p\n", FdoExtension->DeviceObject));

#if defined(RUN_WPP) && defined(WPP_TRACE_W2K_COMPATABILITY)
	// 
	// Cleanup using DeviceObject on Win2K. Make sure
	// this is same deviceobject that used for initializing.
	//
	WPP_CLEANUP(FdoExtension->DeviceObject);
#endif

	NdasPortpDeleteLogicalUnitDescriptorList(
		&FdoExtension->InitChildDescriptorList);

	status = NdasPortFdoWmiDeregister(FdoExtension);
	DebugPrint((3, "NdasPortWmiDeregister: status=%x\n", status));

	NdasPortFdoDeleteSymbolicLinks(FdoExtension);

	IoSetDeviceInterfaceState(&FdoExtension->InterfaceName, FALSE);
	if (NULL != FdoExtension->InterfaceName.Buffer)
	{
		ExFreePool(FdoExtension->InterfaceName.Buffer);
		RtlZeroMemory(&FdoExtension->InterfaceName, sizeof (UNICODE_STRING)); 
	}

	IoDetachDevice(CommonExtension->LowerDeviceObject);

	//
	// Remove Fdo from the global FdoList
	//
	{
		PNDASPORT_DRIVER_EXTENSION driverExtension;
		KIRQL oldIrql;
		PLIST_ENTRY entry;

		driverExtension = IoGetDriverObjectExtension(
			FdoExtension->DriverObject,
			NdasPortDriverExtensionTag);

		ASSERT(NULL != driverExtension);

		KeAcquireSpinLock(&driverExtension->FdoListSpinLock, &oldIrql);
		RemoveEntryList(&FdoExtension->Link);
		KeReleaseSpinLock(&driverExtension->FdoListSpinLock, oldIrql);
	}

	DebugPrint((1, "Deleting FDO: %p\n", CommonExtension->DeviceObject));

	IoDeleteDevice(FdoExtension->DeviceObject);
}

VOID
NdasPortFdoDeleteSymbolicLinks(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	NTSTATUS status;
	UNICODE_STRING symbolicLinkName;
	WCHAR symbolicLinkNameBuffer[64];

	if (FdoExtension->ScsiPortSymbolicLinkCreated)
	{
		status = RtlStringCchPrintfW(
			symbolicLinkNameBuffer,
			countof(symbolicLinkNameBuffer),
			L"\\Device\\ScsiPort%d",
			FdoExtension->PortNumber);
		
		ASSERT(NT_SUCCESS(status));

		RtlInitUnicodeString(
			&symbolicLinkName,
			symbolicLinkNameBuffer);

		status = IoDeleteSymbolicLink(&symbolicLinkName);

		if (!NT_SUCCESS(status))
		{
			DebugPrint((1, "Deleting symbolic link(%ws) failed. Status=%x\n", 
				symbolicLinkNameBuffer,
				status));
		}

		status = RtlStringCchPrintfW(
			symbolicLinkNameBuffer,
			countof(symbolicLinkNameBuffer),
			L"\\DosDevice\\Scsi%d:",
			FdoExtension->PortNumber);

		ASSERT(NT_SUCCESS(status));

		RtlInitUnicodeString(
			&symbolicLinkName,
			symbolicLinkNameBuffer);

		IoDeassignArcName(&symbolicLinkName);

		FdoExtension->ScsiPortSymbolicLinkCreated = FALSE;
		--(IoGetConfigurationInformation()->ScsiPortCount);
	}

	if (FdoExtension->NdasPortSymbolicLinkCreated)
	{
		status = RtlStringCchPrintfW(
			symbolicLinkNameBuffer,
			countof(symbolicLinkNameBuffer),
			L"\\DosDevices\\NdasPort%d",
			FdoExtension->NdasPortNumber);

		ASSERT(NT_SUCCESS(status));

		RtlInitUnicodeString(
			&symbolicLinkName,
			symbolicLinkNameBuffer);

		status = IoDeleteSymbolicLink(&symbolicLinkName);

		if (!NT_SUCCESS(status))
		{
			DebugPrint((1, "Deleting symbolic link(%ws) failed. Status=%x\n", 
				symbolicLinkNameBuffer,
				status));
		}

		FdoExtension->NdasPortSymbolicLinkCreated = FALSE;
	}
}

NTSTATUS
NdasPortFdoCreateSymbolicLinks(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PUNICODE_STRING DeviceName)
{
	NTSTATUS status, status2;
	ULONG i;
	PULONG scsiPortNumber;

	UNICODE_STRING symbolicLinkName;
	WCHAR symbolicLinkNameBuffer[64];

	//
	// There are cases that previous symbolic link name may exist
	// if we did not gracefully stop the device during debugging.
	// Delete the symbolic link and recreate it!
	//

	scsiPortNumber = &IoGetConfigurationInformation()->ScsiPortCount;

	status = STATUS_SUCCESS;
	for (i = 0; i <= (*scsiPortNumber); ++i)
	{
		status2 = RtlStringCchPrintfW(
			symbolicLinkNameBuffer,
			countof(symbolicLinkNameBuffer),
			L"\\Device\\ScsiPort%d", i);
		
		ASSERT(NT_SUCCESS(status2));

		RtlInitUnicodeString(
			&symbolicLinkName,
			symbolicLinkNameBuffer);

		status = IoCreateSymbolicLink(
			&symbolicLinkName,
			DeviceName);

		if (NT_SUCCESS(status))
		{
			DebugPrint((3, "SymbolicLink: %ws\n", symbolicLinkNameBuffer));

			status2 = RtlStringCchPrintfW(
				symbolicLinkNameBuffer,
				countof(symbolicLinkNameBuffer),
				L"\\DosDevices\\Scsi%d:", i);

			ASSERT(NT_SUCCESS(status2));

			RtlInitUnicodeString(
				&symbolicLinkName,
				symbolicLinkNameBuffer);

			IoAssignArcName(
				&symbolicLinkName,
				DeviceName);

			DebugPrint((3, "ArcName: %ws\n", symbolicLinkNameBuffer));

			break;
		}
	}

	if (!NT_SUCCESS(status))
	{
		DebugPrint((1, "Failed to create a symbolic links, status=%x\n", status));
		return status;
	}

	FdoExtension->ScsiPortSymbolicLinkCreated = TRUE;
	FdoExtension->PortNumber = (UCHAR)i;
	++(*scsiPortNumber);

	status2 = RtlStringCchPrintfW(
		symbolicLinkNameBuffer,
		countof(symbolicLinkNameBuffer),
		L"\\DosDevices\\NdasPort%d", 
		FdoExtension->NdasPortNumber);

	RtlInitUnicodeString(
		&symbolicLinkName, 
		symbolicLinkNameBuffer);

	status = IoCreateSymbolicLink(
		&symbolicLinkName,
		DeviceName);

	if (!NT_SUCCESS(status))
	{
		DebugPrint((1, "Failed to create a symbolic link(%ws), status=%x\n", 
			symbolicLinkNameBuffer,
			status));
		NdasPortFdoDeleteSymbolicLinks(FdoExtension);
		return status;
	}

	FdoExtension->NdasPortSymbolicLinkCreated = TRUE;

	DebugPrint((3, "SymbolicLink: %ws\n", symbolicLinkNameBuffer));

	return STATUS_SUCCESS;
}

VOID
NdasPortFdoInitializeIoScsiCapabilities(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__inout PIO_SCSI_CAPABILITIES IoScsiCapabilities)
{
	PSTORAGE_ADAPTER_DESCRIPTOR adapterDescriptor;

	PAGED_CODE();

	//
	// StorageAdapterDescriptor must be initialized before
	// initializing IoScsiCapabilities
	//

	adapterDescriptor = &FdoExtension->StorageAdapterDescriptor;
	ASSERT(adapterDescriptor->Version == sizeof(STORAGE_ADAPTER_DESCRIPTOR));

	IoScsiCapabilities->Length = sizeof(IO_SCSI_CAPABILITIES);
	IoScsiCapabilities->MaximumTransferLength = adapterDescriptor->MaximumTransferLength;
	IoScsiCapabilities->MaximumPhysicalPages = adapterDescriptor->MaximumPhysicalPages;
	IoScsiCapabilities->SupportedAsynchronousEvents = 0;
	IoScsiCapabilities->AlignmentMask = adapterDescriptor->AlignmentMask;
	IoScsiCapabilities->TaggedQueuing = adapterDescriptor->CommandQueueing;
	IoScsiCapabilities->AdapterScansDown = adapterDescriptor->AdapterScansDown;
	IoScsiCapabilities->AdapterUsesPio = adapterDescriptor->AdapterUsesPio;
}

VOID
NdasPortFdoInitializeStorageAdapterDescriptor(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__inout PSTORAGE_ADAPTER_DESCRIPTOR StorageAdapterDescriptor)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(FdoExtension);

	StorageAdapterDescriptor->Version = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
	StorageAdapterDescriptor->Size = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
	//
	// For the sake of RAID, we extends the MaximumTransferLength to 1MB
	//
	// IoScsiCapabilities->MaximumTransferLength = 128 * 1024; // 128 KB
	StorageAdapterDescriptor->MaximumTransferLength = 1024 * 1024; // 1 MB
	//
	// Scatter and gather support
	//
	// Software driver does not need any physical page restrictions.
	// So we supports arbitrary physical pages
	//
	StorageAdapterDescriptor->MaximumPhysicalPages = 0xFFFFFFFF;
	//
	// We do not have any alignment restriction
	//
	StorageAdapterDescriptor->AlignmentMask = FILE_BYTE_ALIGNMENT;
	StorageAdapterDescriptor->AdapterUsesPio = FALSE;
	StorageAdapterDescriptor->AdapterScansDown = FALSE;
	StorageAdapterDescriptor->CommandQueueing = TRUE;
	StorageAdapterDescriptor->AcceleratedTransfer = FALSE;
	StorageAdapterDescriptor->BusType = BusTypeScsi;
	StorageAdapterDescriptor->BusMajorVersion = 0x04;
	StorageAdapterDescriptor->BusMinorVersion = 0x00;
}

/*+++

Routine Description:

	This routine handles add-device requests for the SCSI port driver

Arguments:

	DriverObject - a pointer to the driver object for this device

	PhysicalDeviceObject - a pointer to the PDO we are being added to

Return Value:

	STATUS_SUCCESS if successful
	Appropriate NTStatus code on error

--*/
NTSTATUS
NdasPortFdoAddDevice(
    __in PDRIVER_OBJECT DriverObject,
    __in PDEVICE_OBJECT PhysicalDeviceObject)
{
	NTSTATUS status;
	PNDASPORT_DRIVER_EXTENSION driverExtension;
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	PNDASPORT_FDO_EXTENSION FdoExtension;
	PDEVICE_OBJECT deviceObject;
	PDEVICE_OBJECT newDeviceObject;

	LONG ndasPortNumber;

	UNICODE_STRING deviceName;
	WCHAR deviceNameBuffer[64];

	driverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		DriverObject, 
		NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

	ndasPortNumber = InterlockedIncrement(&driverExtension->NextNdasPortNumber);
	--ndasPortNumber;

	status = RtlStringCchPrintfW(
		deviceNameBuffer,
		countof(deviceNameBuffer),
		L"\\Device\\NdasPort%d",
		ndasPortNumber);

	ASSERT(NT_SUCCESS(status));

	RtlInitUnicodeString(
		&deviceName, 
		deviceNameBuffer);

	DebugPrint((3, "DeviceName=%ws\n", deviceNameBuffer));

	status = IoCreateDeviceSecure(
		DriverObject,
        sizeof(NDASPORT_FDO_EXTENSION),
        &deviceName,
        FILE_DEVICE_NETWORK,
        0,
        FALSE,
		&SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R,
		&GUID_DEVCLASS_SCSIADAPTER,
        &deviceObject);

    if (!NT_SUCCESS(status)) 
	{
        DebugPrint((1, "NdasPortFdoAddDevice failed. Status %lx\n", status));
        return status;
    }

	//
	// FDO Extension Initialization
	//

	FdoExtension = (PNDASPORT_FDO_EXTENSION) deviceObject->DeviceExtension;
	RtlZeroMemory(FdoExtension, sizeof(NDASPORT_FDO_EXTENSION));
	FdoExtension->CommonExtension = &FdoExtension->CommonExtensionHolder;

	status = IoRegisterDeviceInterface(
		PhysicalDeviceObject,
		&GUID_NDASPORT_INTERFACE_CLASS,
		NULL,
		&FdoExtension->InterfaceName);

	if (!NT_SUCCESS(status))
	{
		DebugPrint((1, "IoRegisterDeviceInterface failed. Status %lx\n", status));
		IoDeleteDevice(deviceObject);
		return status;
	}

	DebugPrint((3, "Interface registered: %wZ\n", &FdoExtension->InterfaceName));

	//
	// Create symbolic links
	//

	FdoExtension->NdasPortNumber = ndasPortNumber;

	status = NdasPortFdoCreateSymbolicLinks(FdoExtension, &deviceName);
	if (!NT_SUCCESS(status))
	{
		DebugPrint((1, "NdasPortFdoCreateSymbolicLinks failed. Status %lx\n", status));
		RtlFreeUnicodeString(&FdoExtension->InterfaceName);
		IoDeleteDevice(deviceObject);
		return status;
	}

	//
	// Attach our FDO to the device stack.
	// The return value of IoAttachDeviceToDeviceStack is the top of the
	// attachment chain.  This is where all the IRPs should be routed.
	//

	newDeviceObject = IoAttachDeviceToDeviceStack(
		deviceObject, 
		PhysicalDeviceObject);

	if (NULL == newDeviceObject) 
	{
        DebugPrint((0, 
			"IoAttachDeviceToDeviceStack failed in NdasPortFdoAddDevice\n"));

		NdasPortFdoDeleteSymbolicLinks(FdoExtension);
		RtlFreeUnicodeString(&FdoExtension->InterfaceName);
        IoDeleteDevice(deviceObject);

        return STATUS_UNSUCCESSFUL;
    }

    deviceObject->Flags |= DO_DIRECT_IO;

	CommonExtension = FdoExtension->CommonExtension;

	NpInitializeCommonExtension(
		CommonExtension,
		FALSE,
		deviceObject,
		newDeviceObject,
		FdoMajorFunctionTable);

	//
	// FDO Specific Initialization
	//

	FdoExtension->Link;
	// Set the PDO for use with PlugPlay functions
    FdoExtension->LowerPdo = PhysicalDeviceObject;

	// NdasPortFdoGetIoScsiCapabilities(&FdoExtension->IoScsiCapabilities);
	FdoExtension->InterfaceName;
	InitializeListHead (&FdoExtension->ListOfPDOs);
	FdoExtension->NumberOfPDOs = 0;
	// Port number is assigned from NdasPortPdoCreateSymbolicLinks
	FdoExtension->PortNumber; 
	FdoExtension->DriverObject = DriverObject;

	ExInitializeFastMutex (&FdoExtension->Mutex);

	//
	// Add the FDO to the driver's global FDO list
	//
	ExInterlockedInsertTailList(
		&driverExtension->FdoList,
		&FdoExtension->Link,
		&driverExtension->FdoListSpinLock);

	status = NdasPortFdoWmiRegister(FdoExtension);

	DebugPrint((3, "NdasPortWmiRegister: status=%x\n", status));

#if defined(RUN_WPP) && defined(WPP_TRACE_W2K_COMPATABILITY)
	NdasPortWppInitTracing2K(PhysicalDeviceObject, NULL);
#endif

	//
	// Initialize Adapter Descriptors
	//

	NdasPortFdoInitializeStorageAdapterDescriptor(
		FdoExtension,
		&FdoExtension->StorageAdapterDescriptor);

	NdasPortFdoInitializeIoScsiCapabilities(
		FdoExtension,
		&FdoExtension->IoScsiCapabilities);

	//
	// Power state
	//
	CommonExtension->DevicePowerState = PowerDeviceD0;
	CommonExtension->SystemPowerState = PowerSystemWorking;

	//
	// Initialize Logical Unit Descriptor List Head
	//

	InitializeListHead(
		&FdoExtension->InitChildDescriptorList);

	ExInitializeFastMutex(
		&FdoExtension->InitChildDescriptorListMutex);

	//
	// InitChildCompletionEvent, initially set as TRUE
	//

	KeInitializeEvent(
		&FdoExtension->InitChildCompletionEvent, 
		NotificationEvent, 
		TRUE);

	//
	// Enumerate persistent logical units
	//

	NdasPortFdoRegEnumerateLogicalUnits(
		FdoExtension,
		NdasPortFdoLogicalUnitEnumCallback,
		NULL);

	//
	// We are done with initializing, so let's indicate that and return.
	// This should be the final step in the AddDevice process.
	//
	deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	NdasPortTrace(NDASPORT_PDO_PNP, TRACE_LEVEL_INFORMATION,
		"AddDevice for Pdo=%p\n", PhysicalDeviceObject);

    return STATUS_SUCCESS;
}

NTSTATUS
NdasPortFdoLogicalUnitEnumCallback(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PVOID Context)
{
	PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY descEntry;
	ULONG descEntrySize;

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
		"Enumerated Pdo: LogicalUnitAddress=0x%X\n", 
		LogicalUnitDescriptor->Address.Address);

	descEntry = NdasPortpCreateLogicalUnitDescriptorListEntry(
		FdoExtension, LogicalUnitDescriptor);

	if (NULL == descEntry)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	InsertTailList(
		&FdoExtension->InitChildDescriptorList,
		&descEntry->ListEntry);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortFdoPnpQueryDeviceRelations(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PIRP Irp)
{
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	PDEVICE_RELATIONS oldRelations;
	PDEVICE_RELATIONS deviceRelations;
	PDEVICE_OBJECT lowerDevice; 

	PIO_STACK_LOCATION irpStack;

	ULONG prevCount;
	ULONG relationSize;
	DEVICE_RELATION_TYPE relationType;

	ULONG numPDOsPresent;
	PLIST_ENTRY entry;

	CommonExtension = FdoExtension->CommonExtension;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_INFORMATION,
		"QueryDeviceRelation Type: %s\n", 
		DbgDeviceRelationStringA(irpStack->Parameters.QueryDeviceRelations.Type));

	//
	// We only handle BusRelations for now
	//
	relationType = irpStack->Parameters.QueryDeviceRelations.Type;
	if (BusRelations != relationType) 
	{
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);
	}

	ExAcquireFastMutex (&FdoExtension->Mutex);

	prevCount = 0;
	oldRelations = (PDEVICE_RELATIONS) Irp->IoStatus.Information;
	if (oldRelations) 
	{
		prevCount = oldRelations->Count; 
		if (!FdoExtension->NumberOfPDOs) 
		{
			//
			// There is a device relations struct already present and we have
			// nothing to add to it, so just call IoSkip and IoCall
			//
			ExReleaseFastMutex (&FdoExtension->Mutex);

			return NpReleaseRemoveLockAndForwardIrp(
				CommonExtension, 
				Irp);
		}
	}

	// 
	// Calculate the number of PDOs actually present on the bus
	// 
	numPDOsPresent = 0;
	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		PNDASPORT_PDO_EXTENSION pdoExtension;
		pdoExtension = CONTAINING_RECORD (entry, NDASPORT_PDO_EXTENSION, Link);
		if (pdoExtension->Present)
		{
			numPDOsPresent++;
		}
	}

	//
	// Need to allocate a new relations structure and add our
	// PDOs to it.
	//

	relationSize = sizeof(DEVICE_RELATIONS) +
		((numPDOsPresent + prevCount) * sizeof (PDEVICE_OBJECT)) -1;

	deviceRelations = (PDEVICE_RELATIONS) ExAllocatePoolWithTag(
		PagedPool, 
		relationSize, 
		NDASPORT_TAG_DEVICE_RELATIONS);

	if (NULL == deviceRelations) 
	{
		//
		// Fail the IRP
		//
		ExReleaseFastMutex(&FdoExtension->Mutex);

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			STATUS_INSUFFICIENT_RESOURCES);
	}

	//
	// Copy in the device objects so far
	//
	if (prevCount > 0) 
	{
		RtlCopyMemory(
			deviceRelations->Objects, 
			oldRelations->Objects,
			prevCount * sizeof (PDEVICE_OBJECT));
	}

	deviceRelations->Count = prevCount + numPDOsPresent;

	//
	// For each PDO present on this bus add a pointer to the device relations
	// buffer, being sure to take out a reference to that object.
	// The Plug & Play system will dereference the object when it is done 
	// with it and free the device relations buffer.
	//

	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		PNDASPORT_PDO_EXTENSION pdoExtension;
		pdoExtension = CONTAINING_RECORD (entry, NDASPORT_PDO_EXTENSION, Link);
		if (pdoExtension->Present) 
		{
			if (!pdoExtension->IsDeviceRelationReported)
			{
				pdoExtension->IsDeviceRelationReported = TRUE;
			}
			deviceRelations->Objects[prevCount] = pdoExtension->DeviceObject;
			ObReferenceObject (pdoExtension->DeviceObject);
			++prevCount;
		} 
		else 
		{
			pdoExtension->ReportedMissing = TRUE;
		}
	}

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_INFORMATION,
		"\tPDOs Present=%d, Reported=%d\n", 
		FdoExtension->NumberOfPDOs, deviceRelations->Count);

	ExReleaseFastMutex(&FdoExtension->Mutex);

	//
	// Replace the relations structure in the IRP with the new
	// one.
	//
	if (oldRelations) 
	{
		ExFreePool(oldRelations);
	}

	Irp->IoStatus.Information = (ULONG_PTR) deviceRelations;

	//
	// Set up and pass the IRP further down the stack
	//

	Irp->IoStatus.Status = STATUS_SUCCESS;

	IoCopyCurrentIrpStackLocationToNext(Irp);

	lowerDevice = CommonExtension->LowerDeviceObject;

	NpReleaseRemoveLock(CommonExtension, Irp);

	return IoCallDriver(lowerDevice, Irp);
}

NTSTATUS
NdasPortFdoDispatchPnp(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
	NTSTATUS status;
    PNDASPORT_COMMON_EXTENSION CommonExtension;
    PNDASPORT_FDO_EXTENSION fdoExtension;
	PIO_STACK_LOCATION irpStack;
	ULONG isRemoved;

	PLIST_ENTRY entry, nextEntry;
	LIST_ENTRY pdosToRemoveListHead;

    PAGED_CODE();

	fdoExtension = (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;
	CommonExtension = fdoExtension->CommonExtension;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

	NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_INFORMATION,
		"FDO: PNP Fdo=%p, Irp=%p, MN=%s(0x%02X)\n",
		DeviceObject, 
		Irp, 
		DbgPnPMinorFunctionStringA(irpStack->MinorFunction),
		irpStack->MinorFunction);

    isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);
    if (isRemoved)
	{
		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension, 
			Irp, 
			STATUS_DEVICE_DOES_NOT_EXIST);
    }

	switch (irpStack->MinorFunction)
	{
    case IRP_MN_START_DEVICE: 

		status = NdasPortSendIrpSynchronous(
			CommonExtension->LowerDeviceObject,
            Irp);

        if (!NT_SUCCESS(status)) 
		{
			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}

        //
        // Register for notification from network interface
        // to be notified of network ready state
        //

        //
        // Initialize various fields in the 
        // FDO Extension. 
        //
		status = NdasPortFdoStart(fdoExtension);
		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_INFORMATION,
				"NdasPortFdoStart failed: status=%x\n", status);
			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			STATUS_SUCCESS);

    case IRP_MN_QUERY_STOP_DEVICE: 
		//
		// The PnP manager is trying to stop the device
		// for resource rebalancing. Fail this now if you 
		// cannot stop the device in response to STOP_DEVICE.
		// 
		NpSetNewPnpState(CommonExtension, StopPending);
		
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);

    case IRP_MN_CANCEL_STOP_DEVICE: 
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
		if (StopPending == CommonExtension->DevicePnPState)
		{
			//
			// We did receive a query-stop, so restore.
			//
			NpRestorePreviousPnpState(CommonExtension);
			ASSERT(CommonExtension->DevicePnPState == Started);
		}

		//
		// We must not fail the IRP.
		//
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);

    case IRP_MN_STOP_DEVICE: 
		//
		// Stop device means that the resources given during Start device
		// are now revoked. Note: You must not fail this Irp.
		// But before you relieve resources make sure there are no I/O in 
		// progress. Wait for the existing ones to be finished.
		// To do that, first we will decrement this very operation.
		// When the counter goes to 1, Stop event is set.
		//

		NpReleaseRemoveLock(CommonExtension, CommonExtension->DeviceObject);

		KeWaitForSingleObject(
			&CommonExtension->StopEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		//
		// Increment the counter back because this IRP has to
		// be sent down to the lower stack.
		//
		
		NpAcquireRemoveLock(CommonExtension, CommonExtension->DeviceObject);

		//
		// Free resources given by start device.
		//

		NpSetNewPnpState(CommonExtension, Stopped);

		//
		// We don't need a completion routine so fire and forget.
		//
		// Set the current stack location to the next stack location and
		// call the next device object.
		//
#if 0
		//
		// If network node hasn't been released yet,
		// release it now
		//
		if ((fdoExtension->LocalNodesInitialized) == TRUE) 
		{
			PLIST_ENTRY entry;
			PNDASPORT_PDO_EXTENSION pdoExtension;

			ExAcquireFastMutex(&fdoExtension->Mutex);
			
			for (entry = fdoExtension->ListOfPDOs.Flink;
				entry != &fdoExtension->ListOfPDOs;
				entry = entry->Flink)
			{
				pdoExtension = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);
				NdasPortPdoStopDevice(pdoExtension);
			}

			fdoExtension->LocalNodesInitialized = FALSE;
			fdoExtension->NumberOfPDOs = 0;

			ExReleaseFastMutex(&fdoExtension->Mutex);
		}

		fdoExtension->LocalNodesInitialized = FALSE;
#endif

		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension, 
			Irp);

    case IRP_MN_QUERY_REMOVE_DEVICE: 
		//
		// If we were to fail this call then we would need to complete the
		// IRP here.  Since we are not, set the status to SUCCESS and
		// call the next driver.
		//
        DebugPrint((0, "Query remove received.\n"));

		NpSetNewPnpState(CommonExtension, RemovePending);

		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension, 
			Irp);
    
    case IRP_MN_CANCEL_REMOVE_DEVICE: 

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

		if (RemovePending == CommonExtension->DevicePnPState)
		{
			//
			// We did receive a query-remove, so restore.
			//
			NpRestorePreviousPnpState(CommonExtension);
		}

		//
		// You must not fail the IRP.
		//
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);

	case IRP_MN_SURPRISE_REMOVAL:
		//
		// The device has been unexpectedly removed from the machine 
		// and is no longer available for I/O. Bus_RemoveFdo clears
		// all the resources, frees the interface and de-registers
		// with WMI, but it doesn't delete the FDO. That's done
		// later in Remove device query.
		//

		NpSetNewPnpState(CommonExtension, SurpriseRemovePending);
		
		NdasPortFdoStop(fdoExtension);

		ExAcquireFastMutex(&fdoExtension->Mutex);

		for (entry = fdoExtension->ListOfPDOs.Flink, nextEntry = entry->Flink;
			entry != &fdoExtension->Link;
			entry = nextEntry, nextEntry = entry->Flink)
		{
			PNDASPORT_PDO_EXTENSION pdoExtension;
			pdoExtension = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);
			// Ignore temporary ones
			if (pdoExtension->IsTemporary) continue;
			RemoveEntryList(&pdoExtension->Link);
			InitializeListHead(&pdoExtension->Link);
			pdoExtension->ParentFDOExtension = NULL;
			pdoExtension->ReportedMissing = TRUE;
		}

		ExReleaseFastMutex(&fdoExtension->Mutex);

		//
		// You must not fail the IRP.
		//
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);

	case IRP_MN_REMOVE_DEVICE: 

		//
		// The Plug & Play system has dictated the removal of this device.  
		// We have no choice but to detach and delete the device object.
		//

		if (SurpriseRemovePending != CommonExtension->DevicePnPState)
		{
			NdasPortFdoStop(fdoExtension);
		}

		NpSetNewPnpState(CommonExtension, Deleted);

		//
		// Wait for all outstanding requests to complete.
		// We need two decrements here, one for the increment in 
		// the beginning of this function, the other for the 1-biased value of
		// OutstandingIO.
		// 

		NpReleaseRemoveLock(CommonExtension, Irp);

		//
		// The requestCount is at least one here (is 1-biased)
		//

		NpReleaseRemoveLock(CommonExtension, Irp);

		KeWaitForSingleObject(
			&CommonExtension->RemoveEvent,
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

		//
		// NdasPortPdoRemoveDevice should be called from the PASSIVE_LEVEL,
		// without locking Mutex, so we make a copy of the removed pdo's
		//

		InitializeListHead(&pdosToRemoveListHead);

		ExAcquireFastMutex(&fdoExtension->Mutex);

		for(entry = fdoExtension->ListOfPDOs.Flink ,nextEntry = entry->Flink;
			entry != &fdoExtension->ListOfPDOs;
			entry = nextEntry,nextEntry = entry->Flink) {

				PNDASPORT_PDO_EXTENSION pdoExtension;
				
				pdoExtension = CONTAINING_RECORD (entry, NDASPORT_PDO_EXTENSION, Link);

				RemoveEntryList (&pdoExtension->Link);

				if(SurpriseRemovePending == CommonExtension->DevicePnPState)
				{
					//
					// We will reinitialize the list head so that we
					// wouldn't barf when we try to delink this PDO from 
					// the parent's PDOs list, when the system finally
					// removes the PDO. Let's also not forget to set the
					// ReportedMissing flag to cause the deletion of the PDO.
					//
					NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_WARNING,
						"Found a surprise removed device: Pdo=%p\n", pdoExtension->DeviceObject);
					InitializeListHead (&pdoExtension->Link);
					pdoExtension->ParentFDOExtension  = NULL;
					pdoExtension->ReportedMissing = TRUE;                
					continue;
				}

				--fdoExtension->NumberOfPDOs;
				InsertTailList(&pdosToRemoveListHead, &pdoExtension->Link);
		}

		ExReleaseFastMutex (&fdoExtension->Mutex);

		//
		// Now, removes the pdo's
		//
		for (entry = pdosToRemoveListHead.Flink;
			entry != &pdosToRemoveListHead;
			entry = entry->Flink)
		{
			PNDASPORT_PDO_EXTENSION pdoExtension;

			pdoExtension = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);
			
			NdasPortPdoRemoveDevice(pdoExtension);
		}

		//
		// We need to send the remove down the stack before we detach,
		// but we don't need to wait for the completion of this operation
		// (and to register a completion routine).
		//

		// Irp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation (Irp);
		status = IoCallDriver (CommonExtension->LowerDeviceObject, Irp);

		//
		// Detach from the underlying devices.
		//

		NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_INFORMATION,
			"Destroying FDO: %p\n", DeviceObject);

		NdasPortFdoDestroy(fdoExtension);

        //
        // If network node hasn't been released yet,
        // release it now
        //

        return status;
    
    case IRP_MN_QUERY_DEVICE_RELATIONS: 

		//
		// NdasPortFdoPnpQueryDeviceRelations will release
		// the remove lock
		//

		return NdasPortFdoPnpQueryDeviceRelations(fdoExtension, Irp);

	case IRP_MN_QUERY_BUS_INFORMATION:
		{
			PPNP_BUS_INFORMATION busInformation;

			busInformation = (PPNP_BUS_INFORMATION) ExAllocatePoolWithTag(
				PagedPool,
				sizeof(PNP_BUS_INFORMATION),
				NDASPORT_TAG_PNP_ID);

			if (NULL == busInformation)
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
			else
			{
				status = STATUS_SUCCESS;
				busInformation->BusTypeGuid = GUID_BUS_TYPE_NDASPORT;
				busInformation->LegacyBusType = PNPBus;
				busInformation->BusNumber = fdoExtension->PortNumber;
			}
			return NpReleaseRemoveLockAndCompleteIrpEx(
				CommonExtension,
				Irp,
				status,
				(ULONG_PTR) busInformation,
				IO_NO_INCREMENT);
		}
    default:   
		//
		// In the default case we merely call the next driver.
		// We must not modify Irp->IoStatus.Status or complete the IRP.
		//
		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);
    }
}

NTSTATUS
NdasPortSendIrpSynchronous(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    if (DeviceObject == NULL) 
	{
        NdasPortTrace(NDASPORT_FDO_PNP, TRACE_LEVEL_ERROR,
			"DeviceObject NULL. Irp=%p\n", Irp);
        return Irp->IoStatus.Status;
    }

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(
		Irp,
        NpSetEvent,
        &event,
        TRUE, 
        TRUE,
        TRUE);

    status = IoCallDriver(DeviceObject, Irp);

    if (status == STATUS_PENDING) 
	{
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = Irp->IoStatus.Status;
    }

    return status;
}

/*++

Routine Description:

This routine will copy Count string bytes from Source to Destination.  If
it finds a nul byte in the Source it will translate that and any subsequent
bytes into Change.  It will also replace spaces with the specified character.

Arguments:

Destination - the location to copy bytes

Source - the location to copy bytes from

Count - the number of bytes to be copied

Return Value:

none

--*/
void
NpCopyField(
    __in PSTR Destination,
    __in PCSTR Source,
    __in ULONG Count,
    __in UCHAR Change)
{
	BOOLEAN pastEnd;
	ULONG i;

    PAGED_CODE();

	pastEnd = FALSE;
    for (i = 0; i < Count; i++) 
	{
        if (!pastEnd) 
		{
            if (Source[i] == 0) 
			{
                pastEnd = TRUE;
                Destination[i] = Change;
            }
			else if (Source[i] == ' ') 
			{
                Destination[i] = Change;
            } 
			else 
			{
                Destination[i] = Source[i];
            }
        }
		else 
		{
            Destination[i] = Change;
        }
    }
    return;
}
