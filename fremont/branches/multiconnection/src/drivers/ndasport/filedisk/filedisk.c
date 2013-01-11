#include <ndasport.h>
#include "trace.h"
#include "filedisk.h"
#include "constants.h"

#include <initguid.h>
#include "filediskguid.h"

//
// References:
//
// * Large Logical Unit Support and Windows Server 2003 SP1
//   http://www.microsoft.com/whdc/device/storage/LUN_SP1.mspx
//

#if !defined(_NTIFS_)

#define FSCTL_SET_SPARSE \
	CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 49, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

NTSYSAPI
NTSTATUS
NTAPI
ZwFsControlFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG IoControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength
    );

NTSYSAPI
NTSTATUS
NTAPI
ZwWaitForSingleObject(
    IN HANDLE Handle,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL
    );

#endif /* _NTIFS */

#ifdef RUN_WPP
#include "filedisk.tmh"
#endif

#ifndef countof
#define countof(a) (sizeof(a)/sizeof(a[0]))
#endif

#define FILEDISK_EXT_TAG 'DliF'
#define FILEDISK_PNP_TAG 'PliF'

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FileDiskInitializeLogicalUnit)
#pragma alloc_text(PAGE, FileDiskInitializeIoScsiCapabilities)
#pragma alloc_text(PAGE, FileDiskInitializeInquiryData)
#pragma alloc_text(PAGE, FileDiskCreateDataFile)
#pragma alloc_text(PAGE, FileDiskCloseDataFile)
#pragma alloc_text(PAGE, FileDiskDeleteDataFile)
#endif // ALLOC_PRAGMA

PFILEDISK_EXTENSION 
FORCEINLINE 
FileDiskGetExtension(
	PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	return (PFILEDISK_EXTENSION) NdasPortGetLogicalUnit(LogicalUnitExtension, 0, 0, 0);
}

PNDAS_LOGICALUNIT_EXTENSION
FORCEINLINE
FileDiskGetLogicalUnitExtension(
	PFILEDISK_EXTENSION FileDiskExtension)
{
	return (PNDAS_LOGICALUNIT_EXTENSION) FileDiskExtension;
}

NDAS_LOGICALUNIT_INTERFACE FileDiskInterface = {
	sizeof(NDAS_LOGICALUNIT_INTERFACE),
	NDAS_LOGICALUNIT_INTERFACE_VERSION,
	FileDiskInitializeLogicalUnit,
	FileDiskCleanupLogicalUnit,
	FileDiskLogicalUnitControl,
	FileDiskBuildIo,
	FileDiskStartIo,
	FileDiskQueryPnpID,
	FileDiskQueryPnpDeviceText,
	FileDiskQueryPnpDeviceCapabilities,
	FileDiskQueryStorageDeviceProperty,
	FileDiskQueryStorageDeviceIdProperty
};

NTSTATUS
FileDiskGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__inout PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize)
{
	BOOLEAN equal;

	if (NdasExternalType != LogicalUnitDescriptor->Type)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	equal = RtlEqualMemory(
		&LogicalUnitDescriptor->ExternalTypeGuid,
		&NDASPORT_FILEDISK_TYPE_GUID,
		sizeof(GUID));

	if (!equal)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	if (sizeof(NDAS_LOGICALUNIT_INTERFACE) != LogicalUnitInterface->Size ||
		NDAS_LOGICALUNIT_INTERFACE_VERSION != LogicalUnitInterface->Version)
	{
		return STATUS_INVALID_PARAMETER_2;
	}

	RtlCopyMemory(
		LogicalUnitInterface,
		&FileDiskInterface,
		sizeof(NDAS_LOGICALUNIT_INTERFACE));

	*LogicalUnitExtensionSize = sizeof(FILEDISK_EXTENSION);
	*SrbExtensionSize = 0;

	return STATUS_SUCCESS;
}

NTSTATUS
FileDiskInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	NTSTATUS status;
	PFILEDISK_EXTENSION fileDiskExtension;
	PFILEDISK_DESCRIPTOR fileDiskDescriptor;
	size_t dataFilePathLength;
	BOOLEAN newDataFileCreated;

	PAGED_CODE();

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_FATAL,
			"LogicalUnitDescriptor version is invalid. Version=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Version,
			sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER;
	}
	if (LogicalUnitDescriptor->Size < FIELD_OFFSET(FILEDISK_DESCRIPTOR, FilePath))
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_FATAL,
			"FileDiskDescriptor Size is invalid. Size=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Size,
			sizeof(FILEDISK_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER;
	}

	NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_FATAL,
		"Initializing FileDisk Logical Unit\n");

	fileDiskDescriptor = (PFILEDISK_DESCRIPTOR) LogicalUnitDescriptor;

	fileDiskExtension->FileDiskFlags = fileDiskDescriptor->FileDiskFlags;

	status = RtlStringCbLengthW(
		fileDiskDescriptor->FilePath,
		LogicalUnitDescriptor->Size - FIELD_OFFSET(FILEDISK_DESCRIPTOR, FilePath),
		&dataFilePathLength);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_FATAL,
			"FileDiskDescriptor FilePath length is invalid. Status=%08X\n", 
			status);

		return status;
	}

	NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_INFORMATION,
		"FilePath=%ws\n", fileDiskDescriptor->FilePath);

	dataFilePathLength += sizeof(WCHAR); // additional NULL

	fileDiskExtension->FilePath.Buffer = (PWSTR) ExAllocatePoolWithTag(
		NonPagedPool,
		dataFilePathLength,
		FILEDISK_EXT_TAG);

	if (NULL == fileDiskExtension->FilePath.Buffer)
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_WARNING,
			"Memory allocation failed for data file path (%d bytes).\n", 
			dataFilePathLength);

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlCopyMemory(
		fileDiskExtension->FilePath.Buffer,
		fileDiskDescriptor->FilePath,
		dataFilePathLength);

	fileDiskExtension->FilePath.Length = dataFilePathLength - sizeof(WCHAR);
	fileDiskExtension->FilePath.MaximumLength = dataFilePathLength;

	fileDiskExtension->LogicalUnitAddress = LogicalUnitDescriptor->Address.Address;
	fileDiskExtension->LogicalBlockAddress.QuadPart = fileDiskDescriptor->LogicalBlockAddress.QuadPart;
	fileDiskExtension->BytesPerBlock = fileDiskDescriptor->BytesPerBlock;

	fileDiskExtension->ThreadShouldStop = FALSE;

	KeInitializeEvent(
		&fileDiskExtension->ThreadNotificationEvent,
		NotificationEvent,
		FALSE);

	KeInitializeEvent(
		&fileDiskExtension->ThreadCompleteEvent,
		NotificationEvent,
		FALSE);

	FileDiskInitializeIoScsiCapabilities(fileDiskExtension);
	FileDiskInitializeInquiryData(fileDiskExtension);

	fileDiskExtension->StorageBusType = BusTypeScsi;
	fileDiskExtension->StorageBusMajorVersion = 2;
	fileDiskExtension->StorageBusMinorVersion = 0;

	status = FileDiskCreateDataFile(
		fileDiskExtension,
		&fileDiskExtension->FilePath,
		&newDataFileCreated);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_WARNING,
			"FileDiskCreateDataFile failed, Status=%08X\n", 
			status);

		goto error1;
	}

	//
	// For Windows 2000, PsCreateSystemThread should be called
	// from the system process context.
	//
	if (IoIsWdmVersionAvailable(0x01, 0x20))
	{
		FileDiskCreateThreadWorkItemRoutine(NULL, fileDiskExtension);
	}
	else
	{
		PIO_WORKITEM workItem;
		workItem = NdasPortExAllocateWorkItem(LogicalUnitExtension);
		if (NULL == workItem)
		{
			NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_WARNING,
				"NdasPortExAllocateWorkItem failed with out of resource error.\n");

			status = STATUS_INSUFFICIENT_RESOURCES;

			goto error2;
		}
		
		IoQueueWorkItem(
			workItem, 
			FileDiskCreateThreadWorkItemRoutine,
			DelayedWorkQueue,
			fileDiskExtension);

		KeWaitForSingleObject(
			&fileDiskExtension->ThreadCompleteEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		IoFreeWorkItem(workItem);
	}

	if (!NT_SUCCESS(fileDiskExtension->ThreadStatus))
	{
		status = fileDiskExtension->ThreadStatus;

		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_INFORMATION,
			"FileDisk file creation failed, Status=%08X\n",
			status);

		goto error3;
	}

	NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_INFORMATION,
		"FileDisk created successfully at LogicalUnitAddress=%08X.\n",
		fileDiskExtension->LogicalUnitAddress);

	return STATUS_SUCCESS;

error3:
error2:

	FileDiskCloseDataFile(fileDiskExtension);

	if (newDataFileCreated)
	{
		FileDiskDeleteDataFile(&fileDiskExtension->FilePath);
	}

error1:

	ExFreePoolWithTag(
		fileDiskExtension->FilePath.Buffer, 
		FILEDISK_EXT_TAG);

	fileDiskExtension->FilePath.Buffer = NULL;
	fileDiskExtension->FilePath.Length = 0;
	fileDiskExtension->FilePath.MaximumLength = 0;

	return status;
}

VOID
FileDiskCreateThreadWorkItemRoutine(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context)
{
	PFILEDISK_EXTENSION fileDiskExtension;
	OBJECT_ATTRIBUTES threadAttributes;

	InitializeObjectAttributes(
		&threadAttributes,
		NULL,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	fileDiskExtension = (PFILEDISK_EXTENSION) Context;

	fileDiskExtension->ThreadStatus = PsCreateSystemThread(
		&fileDiskExtension->ThreadHandle,
		0,
		&threadAttributes,
		NULL,
		NULL,
		FileDiskThreadRoutine,
		fileDiskExtension);

	if (NT_SUCCESS(fileDiskExtension->ThreadStatus))
	{
		ObReferenceObjectByHandle(
			fileDiskExtension->ThreadHandle,
			THREAD_ALL_ACCESS,
			NULL,
			KernelMode,
			(PVOID*)&fileDiskExtension->ThreadObject,
			NULL);
	}

	KeSetEvent(
		&fileDiskExtension->ThreadCompleteEvent,
		IO_NO_INCREMENT,
		FALSE);
}


VOID
FileDiskCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	PFILEDISK_EXTENSION fileDiskExtension;

	PAGED_CODE();

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	ASSERT(NULL != fileDiskExtension);

	ASSERT(NULL == fileDiskExtension->CurrentSrb);

	fileDiskExtension->ThreadShouldStop = TRUE;

	KeSetEvent(
		&fileDiskExtension->ThreadNotificationEvent,
		IO_NO_INCREMENT,
		FALSE);

	KeWaitForSingleObject(
		&fileDiskExtension->ThreadCompleteEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	ObDereferenceObject(fileDiskExtension->ThreadObject);
	ZwClose(fileDiskExtension->ThreadHandle);

	fileDiskExtension->ThreadObject = NULL;
	fileDiskExtension->ThreadHandle = NULL;

	FileDiskCloseDataFile(fileDiskExtension);

	ExFreePoolWithTag(
		fileDiskExtension->FilePath.Buffer,
		FILEDISK_EXT_TAG);

	fileDiskExtension->FilePath.Buffer = NULL;
	fileDiskExtension->FilePath.Length = 0;
	fileDiskExtension->FilePath.MaximumLength = 0;
}

VOID
FileDiskCloseDataFile(
	__in PFILEDISK_EXTENSION FileDiskExtension)
{
	PAGED_CODE();

	ASSERT(NULL != FileDiskExtension->FileHandle);
	ASSERT(NULL != FileDiskExtension->FileObject);

	ObDereferenceObject(FileDiskExtension->FileObject);
	ZwClose(FileDiskExtension->FileHandle);

	FileDiskExtension->FileObject = NULL;
	FileDiskExtension->FileHandle = NULL;
}

NTSTATUS
FileDiskCreateDataFile(
	__in PFILEDISK_EXTENSION FileDiskExtension,
	__in PUNICODE_STRING DataFilePath,
	__in PBOOLEAN NewDataFileCreated)
{
	NTSTATUS status;
	OBJECT_ATTRIBUTES fileAttributes;
	HANDLE fileHandle;
	IO_STATUS_BLOCK ioStatus;
	LARGE_INTEGER allocationSize;
	FILE_END_OF_FILE_INFORMATION eof;

	PAGED_CODE();

	InitializeObjectAttributes(
		&fileAttributes,
		DataFilePath,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	allocationSize.QuadPart = 1; // initial size

	//
	// Open the existing file if any
	//

	status = ZwCreateFile(
		&fileHandle,
		GENERIC_ALL,
		&fileAttributes,
		&ioStatus,
		NULL, 
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN_IF,
		FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_ERROR,
			"ZwCreateFile(%wZ) failed: Status=%08X\n", DataFilePath, status);

		goto error1;
	}

	*NewDataFileCreated = (ioStatus.Information == FILE_CREATED);

	//
	// Set sparse file
	//
	if (FileDiskExtension->FileDiskFlags & FILEDISK_FLAG_USE_SPARSE_FILE)
	{
		status = ZwFsControlFile(
			fileHandle,
			NULL,
			NULL,
			NULL,
			&ioStatus,
			FSCTL_SET_SPARSE,
			NULL, 0,
			NULL, 0);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_ERROR,
				"FSCTL_SET_SPARSE failed, Status=%08X\n, IoStatus.Information=%X", 
				status,
				(ULONG)ioStatus.Information);

			goto error2;
		}

		ZwWaitForSingleObject(fileHandle, FALSE, NULL);
	}

	//
	// Set the data file size (truncate or extend)
	//
	eof.EndOfFile.QuadPart = (FileDiskExtension->LogicalBlockAddress.QuadPart + 1) * 
		FileDiskExtension->BytesPerBlock;

	status = ZwSetInformationFile(
		fileHandle,
		NULL,
		&eof,
		sizeof(FILE_END_OF_FILE_INFORMATION),
		FileEndOfFileInformation);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_ERROR,
			"ZwSetInformationFile failed, Status=%08X\n", status);

		goto error2;
	}

	FileDiskExtension->FileHandle = fileHandle;

	ObReferenceObjectByHandle(
		fileHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		(PVOID*)&FileDiskExtension->FileObject,
		NULL);

	NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_INFORMATION,
		"FileDisk Data File %wZ created.\n", DataFilePath);

	return STATUS_SUCCESS;

error2:

	ZwClose(fileHandle);

	if (*NewDataFileCreated)
	{
		FileDiskDeleteDataFile(DataFilePath);
	}

error1:

	return status;
}

VOID
FORCEINLINE
FileDiskInitializeIoScsiCapabilities(
	__in PFILEDISK_EXTENSION FileDiskExtension)
{
	PIO_SCSI_CAPABILITIES ioScsiCapabilities;

	PAGED_CODE();

	ioScsiCapabilities = &FileDiskExtension->IoScsiCapabilities;

	RtlZeroMemory(ioScsiCapabilities, sizeof(IO_SCSI_CAPABILITIES));

	ioScsiCapabilities->Length = sizeof(IO_SCSI_CAPABILITIES);
	// IoScsiCapabilities->MaximumTransferLength = 0x10000; /* 64 KB */
	// IoScsiCapabilities->MaximumTransferLength = 0x20000; /* 128 KB */
	ioScsiCapabilities->MaximumTransferLength = 0x1000000; /* 1 MB */
	//
	// Scatter and gather support
	//
	// Software driver does not need any physical page restrictions.
	// So we supports arbitrarily physical pages
	//
	ioScsiCapabilities->MaximumPhysicalPages = ((ULONG)(~0)) >> 12;
	ioScsiCapabilities->SupportedAsynchronousEvents = 0;
	ioScsiCapabilities->AlignmentMask = FILE_LONG_ALIGNMENT;
	ioScsiCapabilities->TaggedQueuing = TRUE;
	ioScsiCapabilities->AdapterScansDown = FALSE;
	ioScsiCapabilities->AdapterUsesPio = FALSE;
}

VOID
FileDiskInitializeInquiryData(
	__in PFILEDISK_EXTENSION FileDiskExtension)
{
	static CONST UCHAR VendorId[8] = "NDAS";
	static CONST UCHAR ProductId[16] = "File Disk";
	static CONST UCHAR ProductRevisionLevel[4] = "1.0";

	PINQUIRYDATA inquiryData;

	PAGED_CODE();

	inquiryData = &FileDiskExtension->InquiryData;

	//
	// REFERENCES: SPC-2 R20
	//
	RtlZeroMemory(inquiryData, sizeof(INQUIRYDATA));

	inquiryData->DeviceType = DIRECT_ACCESS_DEVICE;
	inquiryData->DeviceTypeQualifier = DEVICE_CONNECTED;
	inquiryData->RemovableMedia = FALSE;
	inquiryData->AdditionalLength = 0;
	inquiryData->DeviceTypeModifier;
	//
	// VERSION
	//
	// 0x02: ANSI X3.131:1994.
	// 0x03: ANSI X3.301:1997.
	// 0x04: SPC-2 T10/1236-D Revision 20
	//
	inquiryData->Versions = 0x02;
	inquiryData->ResponseDataFormat = 2;
	inquiryData->HiSupport;
	inquiryData->NormACA;
	inquiryData->AERC;
	inquiryData->AdditionalLength;
	inquiryData->SoftReset;
	inquiryData->CommandQueue;
	inquiryData->LinkedCommands;
	inquiryData->Synchronous;
	inquiryData->Wide16Bit;
	inquiryData->Wide32Bit;
	inquiryData->RelativeAddressing;

	inquiryData->VendorId[8-1];
	inquiryData->ProductId[16-1];
	inquiryData->ProductRevisionLevel[4-1];

	RtlCopyMemory(inquiryData->VendorId, VendorId, sizeof(VendorId));
	RtlCopyMemory(inquiryData->ProductId, ProductId, sizeof(ProductId));
	RtlCopyMemory(inquiryData->ProductRevisionLevel, ProductRevisionLevel, 
		sizeof(ProductRevisionLevel));

	inquiryData->VendorSpecific[20-1];
}

NTSTATUS
FileDiskLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters)
{
	return STATUS_SUCCESS;
}

//
// PNP ID Routines
//

NTSTATUS
FileDiskQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId)
{
	static CONST WCHAR* HARDWARE_IDS[] = { 
		NDASPORT_ENUMERATOR_GUID_PREFIX L"FileDisk",
		NDASPORT_ENUMERATOR_NAMED_PREFIX L"FileDisk", 
		L"GenDisk", };
	static CONST WCHAR* COMPATIBLE_IDS[] = { L"gendisk" };
	WCHAR instanceId[20];
	WCHAR* instanceIdList[] = { instanceId };

	NTSTATUS status;
	PFILEDISK_EXTENSION fileDiskExtension;

	CONST WCHAR** idList;
	ULONG idListCount;

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	switch (QueryType)
	{
	case BusQueryDeviceID:
		idList = HARDWARE_IDS;
		idListCount = 1;
		break;
	case BusQueryHardwareIDs:
		idList = HARDWARE_IDS;
		idListCount = countof(HARDWARE_IDS);
		break;
	case BusQueryCompatibleIDs:
		idList = COMPATIBLE_IDS;
		idListCount = countof(COMPATIBLE_IDS);
		break;
	case BusQueryInstanceID:
		idList = (const WCHAR**) instanceIdList;
		idListCount = 1;
		status = RtlStringCchPrintfW(
			instanceId, 
			countof(instanceId),
			L"%08X", 
			RtlUlongByteSwap(fileDiskExtension->LogicalUnitAddress));
		ASSERT(NT_SUCCESS(status));
		break;
	case 4 /*BusQueryDeviceSerialNumber*/:
	default:
		return STATUS_NOT_SUPPORTED;
	}

	if (Index >= idListCount)
	{
		return STATUS_NO_MORE_ENTRIES;
	}

	status = RtlUnicodeStringCopyString(
		UnicodeStringId,
		idList[Index]);

	return status;
}

NTSTATUS
FileDiskQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__in PWCHAR* DeviceText)
{
	NTSTATUS status;
	PFILEDISK_EXTENSION fileDiskExtension;
	size_t filePathLength;
	static CONST WCHAR FILEDISK_DEVICE_TEXT[] = L"NDAS Virtual File Disk";

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	switch (DeviceTextType)
	{
	case DeviceTextDescription:
		
		*DeviceText = (PWCHAR) ExAllocatePoolWithTag(
			PagedPool, 
			sizeof(FILEDISK_DEVICE_TEXT), 
			FILEDISK_PNP_TAG);
		
		if (NULL == *DeviceText)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		
		status = RtlStringCchCopyW(
			*DeviceText,
			countof(FILEDISK_DEVICE_TEXT),
			FILEDISK_DEVICE_TEXT);

		ASSERT(NT_SUCCESS(status));
		return status;

	case DeviceTextLocationInformation:

		filePathLength = fileDiskExtension->FilePath.Length;
		filePathLength += sizeof(WCHAR); // additional NULL

		*DeviceText = ExAllocatePoolWithTag(
			PagedPool,
			filePathLength,
			FILEDISK_PNP_TAG);

		if (NULL == *DeviceText)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		status = RtlStringCbCopyW(
			*DeviceText,
			filePathLength, 
			fileDiskExtension->FilePath.Buffer);

		ASSERT(NT_SUCCESS(status));

		return status;
	default:
		return STATUS_NOT_SUPPORTED;
	}
}

NTSTATUS
FileDiskQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__inout PDEVICE_CAPABILITIES Capabilities)
{
	PFILEDISK_EXTENSION fileDiskExtension;
	DEVICE_CAPABILITIES deviceCaps;

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	RtlZeroMemory(&deviceCaps, sizeof(DEVICE_CAPABILITIES));
	deviceCaps.Version = 1;
	deviceCaps.Size = sizeof(DEVICE_CAPABILITIES);
	deviceCaps.Removable = TRUE;
	deviceCaps.EjectSupported = TRUE;
	deviceCaps.SurpriseRemovalOK = FALSE;
	deviceCaps.Address = -1; // (0x00FFFFFF) | fileDiskExtension->LogicalUnitAddress;
	deviceCaps.UINumber = -1; // (0x00FFFFFF) | fileDiskExtension->LogicalUnitAddress;

	if (Capabilities->Version != 1)
	{
		return STATUS_NOT_SUPPORTED;
	}
	if (Capabilities->Size < sizeof(DEVICE_CAPABILITIES))
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlCopyMemory(
		Capabilities,
		&deviceCaps,
		min(sizeof(DEVICE_CAPABILITIES), Capabilities->Size));

	return STATUS_SUCCESS;
}

//
// Storage Device Property
//

NTSTATUS
FileDiskQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PFILEDISK_EXTENSION fileDiskExtension;
	STORAGE_DEVICE_DESCRIPTOR tmp;
	ULONG offset;
	ULONG realLength;
	ULONG remainingBufferLength;
	ULONG usedBufferLength;
	PINQUIRYDATA inquiryData;
	ULONG inquiryDataLength;

	// static CONST CHAR RawPropertyData[] = "DUMMY";
	//static CONST CHAR VendorId[] = "NDAS";
	//static CONST CHAR ProductId[] = "VIRTUAL FILE DISK";
	//static CONST CHAR ProductRevision[] = "1.0";
	//static CONST CHAR SerialNumber[] = "101";

	//
	// Fill the storage device descriptor as much as possible
	//

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	inquiryData = &fileDiskExtension->InquiryData;
	inquiryDataLength = sizeof(INQUIRYDATA);

	//
	// Zero out the provided buffer
	//
	RtlZeroMemory(DeviceDescriptor, BufferLength);

	realLength = sizeof(STORAGE_DEVICE_DESCRIPTOR) - 1 +
		inquiryDataLength +
		sizeof(inquiryData->VendorId) +
		sizeof(inquiryData->ProductId) +
		sizeof(inquiryData->ProductRevisionLevel);

	RtlZeroMemory(&tmp, sizeof(STORAGE_DEVICE_DESCRIPTOR));
	tmp.Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
	tmp.Size = realLength;
	tmp.DeviceType = inquiryData->DeviceType;
	tmp.DeviceTypeModifier = inquiryData->DeviceTypeModifier;
	tmp.RemovableMedia = inquiryData->RemovableMedia;
	tmp.CommandQueueing = inquiryData->CommandQueue;
	tmp.VendorIdOffset;
	tmp.ProductIdOffset;
	tmp.ProductRevisionOffset;
	tmp.SerialNumberOffset;
	tmp.BusType = fileDiskExtension->StorageBusType;
	// INQUIRYDATA or ATAIDENTIFYDATA is stored in RawDeviceProperties
	tmp.RawPropertiesLength;
	tmp.RawDeviceProperties[0];

	offset = 0;

	//
	// Copies up to sizeof(STORAGE_DEVICE_DESCRIPTOR),
	// excluding RawDeviceProperties[0]
	//

	NdasPortSetStoragePropertyData(
		&tmp, sizeof(STORAGE_DEVICE_DESCRIPTOR) - 1,
		DeviceDescriptor, BufferLength,
		&offset, NULL);

	//
	// Set Raw Device Properties
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyData(
			inquiryData, inquiryDataLength,
			DeviceDescriptor, BufferLength,
			&offset, 
			NULL);
		tmp.RawPropertiesLength  = inquiryDataLength;
	}

	//
	// Set Vendor Id
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyString(
			inquiryData->VendorId, sizeof(inquiryData->VendorId), 
			DeviceDescriptor, BufferLength, 
			&offset,
			&DeviceDescriptor->VendorIdOffset);
	}

	//
	// Set Product Id
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyString(
			inquiryData->ProductId, sizeof(inquiryData->ProductId), 
			DeviceDescriptor, BufferLength,
			&offset, 
			&DeviceDescriptor->ProductIdOffset);
	}

	//
	// Set Product Revision
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyString(
			inquiryData->ProductRevisionLevel, sizeof(inquiryData->ProductRevisionLevel), 
			DeviceDescriptor, BufferLength,
			&offset, 
			&DeviceDescriptor->ProductRevisionOffset);
	}

	//
	// Set SerialNumber
	//
	//if (offset < BufferLength)
	//{
	//	NdasPortSetStoragePropertyString(
	//		SerialNumber, sizeof(SerialNumber), 
	//		DeviceDescriptor, BufferLength,
	//		&offset, 
	//		&DeviceDescriptor->SerialNumberOffset);
	//}

	*ResultLength = offset;

	return STATUS_SUCCESS;
}

NTSTATUS
FileDiskQueryStorageDeviceIdProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	*ResultLength = 0;
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS
FileDiskQueryStorageAdapterProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_ADAPTER_DESCRIPTOR AdapterDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PFILEDISK_EXTENSION fileDiskExtension;
	PIO_SCSI_CAPABILITIES capabilities;
	STORAGE_ADAPTER_DESCRIPTOR tmp;

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	capabilities = &fileDiskExtension->IoScsiCapabilities;

	ASSERT(sizeof(IO_SCSI_CAPABILITIES) == capabilities->Length);

	tmp.Version = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
	tmp.Size = sizeof(STORAGE_ADAPTER_DESCRIPTOR);

	tmp.MaximumTransferLength = capabilities->MaximumTransferLength;
	tmp.MaximumPhysicalPages = capabilities->MaximumPhysicalPages;

	tmp.AlignmentMask = capabilities->AlignmentMask;

	tmp.AdapterUsesPio = capabilities->AdapterUsesPio;
	tmp.AdapterScansDown = capabilities->AdapterScansDown;
	tmp.CommandQueueing = capabilities->TaggedQueuing;
	tmp.AcceleratedTransfer = TRUE;

	tmp.BusType = fileDiskExtension->StorageBusType;
	tmp.BusMajorVersion = fileDiskExtension->StorageBusMajorVersion;
	tmp.BusMinorVersion = fileDiskExtension->StorageBusMinorVersion;

	*ResultLength = min(BufferLength, sizeof(STORAGE_ADAPTER_DESCRIPTOR));

	RtlCopyMemory(
		AdapterDescriptor,
		&tmp,
		*ResultLength);

	return STATUS_SUCCESS;
}

NTSTATUS
FileDiskQueryIoScsiCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out PIO_SCSI_CAPABILITIES IoScsiCapabilities,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PFILEDISK_EXTENSION fileDiskExtension;
	PIO_SCSI_CAPABILITIES capabilities;

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	capabilities = &fileDiskExtension->IoScsiCapabilities;

	ASSERT(sizeof(IO_SCSI_CAPABILITIES) == capabilities->Length);

	*ResultLength = min(BufferLength, sizeof(IO_SCSI_CAPABILITIES));

	RtlCopyMemory(
		IoScsiCapabilities,
		&capabilities,
		*ResultLength);

	return STATUS_SUCCESS;
}

//
// Scsi IO Routines
//

BOOLEAN
FileDiskBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	return TRUE;
}

BOOLEAN
FileDiskStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	PFILEDISK_EXTENSION fileDiskExtension;

	fileDiskExtension = FileDiskGetExtension(LogicalUnitExtension);

	if (SRB_FUNCTION_EXECUTE_SCSI != Srb->Function)
	{
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;

		NdasPortNotification(
			RequestComplete, 
			LogicalUnitExtension, 
			Srb);

		NdasPortNotification(
			NextLuRequest, 
			LogicalUnitExtension);

		return TRUE;
	}

	switch (Srb->Cdb[0])
	{
		//Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		//Srb->DataTransferLength = 0;
		//break;
	case SCSIOP_READ16:
	case SCSIOP_WRITE16:
	case SCSIOP_VERIFY16:
	case SCSIOP_READ:
	case SCSIOP_WRITE:
	case SCSIOP_VERIFY:
		{
			ASSERT(NULL == fileDiskExtension->CurrentSrb);
			fileDiskExtension->CurrentSrb = Srb;

			KeSetEvent(
				&fileDiskExtension->ThreadNotificationEvent,
				IO_NO_INCREMENT,
				FALSE);

			return TRUE;
		}
		break;
	case SCSIOP_READ_CAPACITY:
		{
			//
			// Claim 512 byte blocks (big-endian).
			//
			PREAD_CAPACITY_DATA readCapacityData;
			ULONG logicalBlockAddress;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_IN);

			if (Srb->DataTransferLength < sizeof(READ_CAPACITY_DATA))
			{
				Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
				Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);
				break;
			}
			else
			{
				readCapacityData = (PREAD_CAPACITY_DATA) Srb->DataBuffer;

				if (fileDiskExtension->LogicalBlockAddress.QuadPart < 0xFFFFFFFF)
				{
					readCapacityData->LogicalBlockAddress = 
						RtlUlongByteSwap(
							fileDiskExtension->LogicalBlockAddress.LowPart);
				}
				else
				{
					readCapacityData->LogicalBlockAddress = 0xFFFFFFFF;
				}

				readCapacityData->BytesPerBlock = RtlUlongByteSwap(
					fileDiskExtension->BytesPerBlock);

				Srb->SrbStatus = SRB_STATUS_SUCCESS;
				Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);

				NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_INFORMATION,
					"Capacity Reported (Big-Endian): LBA=%08X, BytesPerBlock=%08X\n",
					readCapacityData->LogicalBlockAddress,
					readCapacityData->BytesPerBlock);
			}
		}
		break;
	case SCSIOP_READ_CAPACITY16:
		{
#pragma pack(push, read_capacity_ex, 1)
			typedef struct _READ_CAPACITY_DATA_EX {
				LARGE_INTEGER LogicalBlockAddress;
				ULONG BytesPerBlock;
			} READ_CAPACITY_DATA_EX, *PREAD_CAPACITY_DATA_EX;
#pragma pack(pop, read_capacity_ex)

			PREAD_CAPACITY_DATA_EX readCapacityDataEx;

			if (Srb->DataTransferLength < sizeof(PREAD_CAPACITY_DATA_EX))
			{
				Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
				Srb->DataTransferLength = sizeof(PREAD_CAPACITY_DATA_EX);
				break;
			}

			readCapacityDataEx = (PREAD_CAPACITY_DATA_EX) Srb->DataBuffer;

			readCapacityDataEx->LogicalBlockAddress.QuadPart = (LONGLONG)
				RtlUlonglongByteSwap((ULONGLONG)
				fileDiskExtension->LogicalBlockAddress.QuadPart);

			readCapacityDataEx->BytesPerBlock = 
				RtlUlongByteSwap(fileDiskExtension->BytesPerBlock);

			NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_INFORMATION,
				"Capacity Reported (Big-Endian): LBA=%I64X, BytesPerBlock=%08X\n",
				readCapacityDataEx->LogicalBlockAddress.QuadPart,
				readCapacityDataEx->BytesPerBlock);

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA_EX);
		}
		break;
	case SCSIOP_INQUIRY:
		{
			PINQUIRYDATA inquiryData;
			ULONG readDataLength;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_OUT);

			inquiryData = &fileDiskExtension->InquiryData;

			readDataLength = min(Srb->DataTransferLength, sizeof(INQUIRYDATA));

			//
			// Fill as much as possible
			//

			RtlCopyMemory(
				Srb->DataBuffer,
				inquiryData,
				readDataLength);

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = readDataLength;
		}
		break;
	case SCSIOP_START_STOP_UNIT:
	case SCSIOP_TEST_UNIT_READY:
		Srb->SrbStatus = SRB_STATUS_SUCCESS;
		Srb->DataTransferLength = 0;
		break;
	case SCSIOP_MODE_SENSE:
		//
		// This is used to determine of the media is write-protected.
		// Since IDE does not support mode sense then we will modify just the portion we need
		// so the higher level driver can determine if media is protected.
		//
		{
			PMODE_PARM_READ_WRITE_DATA mode;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_IN);
			RtlZeroMemory(Srb->DataBuffer, Srb->DataTransferLength);

			mode = (PMODE_PARM_READ_WRITE_DATA) Srb->DataBuffer;
			mode->ParameterListHeader.ModeDataLength = sizeof(MODE_PARM_READ_WRITE_DATA) - 1;
			mode->ParameterListHeader.BlockDescriptorLength = sizeof(mode->ParameterListBlock);

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = Srb->DataTransferLength;
		}
		break;
	default:
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;
	}

	NdasPortNotification(
		RequestComplete,
		LogicalUnitExtension,
		Srb);

	NdasPortNotification(
		NextLuRequest,
		LogicalUnitExtension);

	return TRUE;
}

//
// Internal Implementations
//

VOID
FileDiskProcessIo(
	__in PFILEDISK_EXTENSION FileDiskExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;

	PCDB cdb = (PCDB) Srb->Cdb;
	LONGLONG startingBlockAddress;
	LONGLONG endBlockAddress;
	ULONG transferBlockCount;
	LONGLONG startingByteAddress;
	ULONG transferBytes;

	FILE_POSITION_INFORMATION filePositionInformation;
	LARGE_INTEGER offset;
	IO_STATUS_BLOCK ioStatus;

	status = NdasPortRetrieveCdbBlocks(Srb, &startingBlockAddress, &transferBlockCount);
	if (!NT_SUCCESS(status))
	{
		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;
		return;
	}

	endBlockAddress = startingBlockAddress + (LONGLONG)transferBlockCount - 1;

	if (endBlockAddress > FileDiskExtension->LogicalBlockAddress.QuadPart)
	{
		NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_WARNING,
			"SCSIOP_READ/WRITE/VERIFY out of bounds (%I64Xh->%I64Xh,%Xh)\n", 
			startingBlockAddress, endBlockAddress, transferBlockCount);
		
		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;
		return;
	}

	startingByteAddress = startingBlockAddress * FileDiskExtension->BytesPerBlock;
	transferBytes = transferBlockCount * FileDiskExtension->BytesPerBlock;

	if (cdb->CDB10.OperationCode != SCSIOP_VERIFY &&
		transferBytes != Srb->DataTransferLength)
	{
		NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_WARNING,
			"SCSIOP_READ/WRITE transfer length is invalid (%I64Xh->%I64Xh,%Xh) TransferBytes=%Xh, SrbTransferLength=%Xh\n",
			startingBlockAddress, endBlockAddress, transferBlockCount,
			transferBytes,
			Srb->DataTransferLength);

		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;
		return;
	}

	switch (Srb->Cdb[0])
	{
	case SCSIOP_READ:
	case SCSIOP_READ16:
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:

		NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_%s: (%I64Xh->%I64Xh,%Xh) ByteOffset=%I64Xh (%Xh bytes)\n",
			(SCSIOP_READ == Srb->Cdb[0]) ? "READ" : "WRITE",
			startingBlockAddress, endBlockAddress, transferBlockCount,
			startingByteAddress, transferBytes);

		offset.QuadPart = startingByteAddress;

		switch (Srb->Cdb[0])
		{
		case SCSIOP_READ:
		case SCSIOP_READ16:
			{
				NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_ERROR,
					"ZwReadFile: Offset=%08I64Xh, Length=%Xh\n", 
					offset.QuadPart, transferBytes);

				status = ZwReadFile(
					FileDiskExtension->FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatus,
					Srb->DataBuffer,
					transferBytes,
					&offset,
					NULL);

			}
			break;
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:
			{
				NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_ERROR,
					"ZwWriteFile: Offset=%08I64Xh, Length=%Xh\n", 
					offset.QuadPart, transferBytes);

				status = ZwWriteFile(
					FileDiskExtension->FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatus,
					Srb->DataBuffer,
					transferBytes,
					&offset,
					NULL);
			}
			break;
		DEFAULT_UNREACHABLE;
		}

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_ERROR,
				"ZwRead/WriteFile failed: Status=%08x\n", status);
			Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			Srb->SrbStatus = SRB_STATUS_ERROR;
			Srb->DataTransferLength = 0;
		}
		else
		{
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = transferBytes;
		}

		break;

	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:
		NdasPortTrace(FILEDISK_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_VERIFY: (%I64Xh->%I64Xh,%Xh) ByteOffset=%I64Xh (%Xh bytes)\n", 
			startingBlockAddress, endBlockAddress, transferBlockCount,
			startingByteAddress, transferBytes);
		Srb->SrbStatus = SRB_STATUS_SUCCESS;
		Srb->DataTransferLength = 0;
		break;
	default:
		ASSERT(FALSE);
	}

	return;
}

VOID
FileDiskThreadRoutine(
	__in PVOID Context)
{
	PFILEDISK_EXTENSION fileDiskExtension;
	PSCSI_REQUEST_BLOCK srb;

	fileDiskExtension = (PFILEDISK_EXTENSION) Context;

	KeClearEvent(&fileDiskExtension->ThreadCompleteEvent);

	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

	//
	// Now enter the main IRP-processing loop
	//
	while( TRUE )
	{
		//
		// Wait indefinitely for an IRP to appear in the work queue or for
		// the Unload routine to stop the thread. Every successful return 
		// from the wait decrements the semaphore count by 1.
		//

		KeWaitForSingleObject(
			&fileDiskExtension->ThreadNotificationEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		//
		// See if thread was awakened because driver is unloading itself...
		//

		if (fileDiskExtension->ThreadShouldStop)
		{
			KeSetEvent(
				&fileDiskExtension->ThreadCompleteEvent,
				IO_NO_INCREMENT,
				FALSE);
			PsTerminateSystemThread(STATUS_SUCCESS);
			return;
		}

		KeClearEvent(&fileDiskExtension->ThreadNotificationEvent);

		//
		// Perform I/O
		//
		FileDiskProcessIo(fileDiskExtension, fileDiskExtension->CurrentSrb);

		srb = fileDiskExtension->CurrentSrb;
		fileDiskExtension->CurrentSrb = NULL;

		NdasPortNotification(
			RequestComplete,
			FileDiskGetLogicalUnitExtension(fileDiskExtension),
			srb);

		NdasPortNotification(
			NextLuRequest,
			FileDiskGetLogicalUnitExtension(fileDiskExtension));

		//
		// Go back to the top of the loop to see if there's another request waiting.
		//
	} // end of while-loop

}

NTSTATUS
FileDiskDeleteDataFile(
	__in PUNICODE_STRING FilePath)
{
	NTSTATUS status;
	OBJECT_ATTRIBUTES attributes;
	NTSTATUS (*zwDeleteFileRoutine)(POBJECT_ATTRIBUTES);
	UNICODE_STRING zwDeleteFileRoutineName;
	// UNICODE_STRING filePathCopy;

	PAGED_CODE();

	RtlInitUnicodeString(&zwDeleteFileRoutineName, L"ZwDeleteFile");

	zwDeleteFileRoutine = (NTSTATUS(*)(POBJECT_ATTRIBUTES))MmGetSystemRoutineAddress(&zwDeleteFileRoutineName);
	if (!zwDeleteFileRoutine)
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_ERROR,
			"ZwDeleteFile not available. File retained: %wZ\n", FilePath);
		return STATUS_NOT_IMPLEMENTED;
	}

	InitializeObjectAttributes(
		&attributes,
		FilePath,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE | OBJ_OPENIF,
		NULL,
		NULL);

	status = zwDeleteFileRoutine(&attributes);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_ERROR,
			"Deleting %wZ failed, Status=%08X\n",
			FilePath, status);
	}
	else
	{
		NdasPortTrace(FILEDISK_INIT, TRACE_LEVEL_INFORMATION,
			"File %wZ deleted.\n", FilePath);
	}

	return status;
}
