#include "ndasport.h"
#include "trace.h"
#include "ramdisk.h"

#if _WIN32_WINNT <= 0x0500
#define NTSTRSAFE_LIB
#endif
#include <ntstrsafe.h>

#include <initguid.h>
#include "ramdiskguid.h"

#ifdef RUN_WPP
#include "ramdisk.tmh"
#endif

#define RAMDISK_EXT_TAG 'DliF'
#define RAMDISK_PNP_TAG 'PliF'

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _RAMDISK_EXTENSION {

	PNDAS_LOGICALUNIT_EXTENSION DeviceExtension;

	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;

	ULONG SectorSize;
	ULONG SectorCount;

	PSCSI_REQUEST_BLOCK CurrentSrb;

	IO_SCSI_CAPABILITIES IoScsiCapabilities;

	STORAGE_BUS_TYPE StorageBusType;
	USHORT StorageBusMajorVersion;
	USHORT StorageBusMinorVersion;

	PVOID StorageData;

} RAMDISK_EXTENSION, *PRAMDISK_EXTENSION;

VOID
FORCEINLINE
RamDiskInitializeIoScsiCapabilities(
	__in PRAMDISK_EXTENSION RamDiskExtension,
	__inout_bcount(sizeof(IO_SCSI_CAPABILITIES)) 
		PIO_SCSI_CAPABILITIES IoScsiCapabilities);

VOID
RamDiskGetInquiryData(
	__in PRAMDISK_EXTENSION RamDiskExtension,
	__inout PINQUIRYDATA InquiryData);

VOID
RamDiskThreadRoutine(
	__in PVOID Context);

VOID
RamDiskCreateThreadWorkItemRoutine(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context);

#ifdef __cplusplus
}
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, RamDiskInitializeLogicalUnit)
#pragma alloc_text(PAGE, RamDiskInitializeIoScsiCapabilities)
#endif // ALLOC_PRAGMA

PRAMDISK_EXTENSION 
FORCEINLINE 
RamDiskGetExtension(
	PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	return (PRAMDISK_EXTENSION) NdasPortGetLogicalUnit(DeviceExtension, 0, 0, 0);
}

NDAS_LOGICALUNIT_INTERFACE RamDiskInterface = {
	sizeof(NDAS_LOGICALUNIT_INTERFACE),
	NDAS_LOGICALUNIT_INTERFACE_VERSION,
	RamDiskInitializeLogicalUnit,
	RamDiskCleanupLogicalUnit,
	RamDiskLogicalUnitControl,
	RamDiskBuildIo,
	RamDiskStartIo,
	RamDiskQueryPnpID,
	RamDiskQueryPnpDeviceText,
	RamDiskQueryPnpDeviceCapabilities,
	RamDiskQueryStorageDeviceProperty,
	RamDiskQueryStorageDeviceIdProperty
};

NTSTATUS
RamDiskGetNdasLogicalUnitInterface(
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
		&NDASPORT_RAMDISK_TYPE_GUID,
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
		&RamDiskInterface,
		sizeof(NDAS_LOGICALUNIT_INTERFACE));

	*LogicalUnitExtensionSize = sizeof(RAMDISK_EXTENSION);
	*SrbExtensionSize = 0;

	return STATUS_SUCCESS;
}

NTSTATUS
RamDiskInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	NTSTATUS status;
	PRAMDISK_EXTENSION ramDiskExtension;
	PRAMDISK_DESCRIPTOR ramDiskDescriptor;
	size_t dataFilePathLength;
	BOOLEAN newDataFileCreated;

	PAGED_CODE();

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);
	ramDiskExtension->DeviceExtension = DeviceExtension;

	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		NdasPortTrace(RAMDISK_INIT, TRACE_LEVEL_FATAL,
			"LogicalUnitDescriptor version is invalid. Version=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Version,
			sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER;
	}
	if (sizeof(RAMDISK_DESCRIPTOR) != LogicalUnitDescriptor->Size)
	{
		NdasPortTrace(RAMDISK_INIT, TRACE_LEVEL_FATAL,
			"RamDiskDescriptor Size is invalid. Size=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Size,
			sizeof(RAMDISK_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER;
	}

	NdasPortTrace(RAMDISK_INIT, TRACE_LEVEL_FATAL,
		"Initializing RamDisk Logical Unit\n");

	ramDiskDescriptor = (PRAMDISK_DESCRIPTOR) LogicalUnitDescriptor;

	ramDiskExtension->LogicalUnitAddress = LogicalUnitDescriptor->Address;
	ramDiskExtension->SectorCount = ramDiskDescriptor->SectorCount;
	ramDiskExtension->SectorSize = ramDiskDescriptor->SectorSize;

	RamDiskInitializeIoScsiCapabilities(
		ramDiskExtension,
		&ramDiskExtension->IoScsiCapabilities);

	ramDiskExtension->StorageBusType = BusTypeScsi;
	ramDiskExtension->StorageBusMajorVersion = 2;
	ramDiskExtension->StorageBusMinorVersion = 0;

	ramDiskExtension->StorageData = ExAllocatePoolWithTag(
		NonPagedPool,
		ramDiskExtension->SectorSize * ramDiskExtension->SectorCount,
		RAMDISK_EXT_TAG);

	if (NULL == ramDiskExtension->StorageData)
	{
		NdasPortTrace(RAMDISK_INIT, TRACE_LEVEL_WARNING,
			"RamDisk failed to allocate 0x%X bytes\n",
			ramDiskExtension->SectorSize * ramDiskExtension->SectorCount);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NdasPortTrace(RAMDISK_INIT, TRACE_LEVEL_INFORMATION,
		"RamDisk created successfully at LogicalUnitAddress=%08X.\n",
		ramDiskExtension->LogicalUnitAddress.Address);

	return STATUS_SUCCESS;
}

VOID
RamDiskCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	PRAMDISK_EXTENSION ramDiskExtension;

	PAGED_CODE();

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	ASSERT(NULL != ramDiskExtension);
	ASSERT(NULL != ramDiskExtension->StorageData);

	ExFreePoolWithTag(
		ramDiskExtension->StorageData,
		RAMDISK_EXT_TAG);

	ramDiskExtension->StorageData = NULL;
}

VOID
FORCEINLINE
RamDiskInitializeIoScsiCapabilities(
	__in PRAMDISK_EXTENSION RamDiskExtension,
	__inout_bcount(sizeof(IO_SCSI_CAPABILITIES)) 
		PIO_SCSI_CAPABILITIES IoScsiCapabilities)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(RamDiskExtension);

	RtlZeroMemory(IoScsiCapabilities, sizeof(IO_SCSI_CAPABILITIES));
	//
	// For now, we are going to hard code most of the values
	//
	IoScsiCapabilities->Length = sizeof(IO_SCSI_CAPABILITIES);

	// IoScsiCapabilities->MaximumTransferLength = 0x10000; /* 64 KB */
	// IoScsiCapabilities->MaximumTransferLength = 0x20000; /* 128 KB */
	IoScsiCapabilities->MaximumTransferLength = 0x1000000; /* 1 MB */

	//
	// Scatter and gather support
	//
	// Software driver does not need any physical page restrictions.
	// So we supports arbitrarily physical pages
	//
	IoScsiCapabilities->MaximumPhysicalPages = (ULONG)(~0);
	IoScsiCapabilities->SupportedAsynchronousEvents = 0;
	IoScsiCapabilities->AlignmentMask = FILE_LONG_ALIGNMENT;
	IoScsiCapabilities->TaggedQueuing = TRUE;
	// IoScsiCapabilities->TaggedQueuing = FALSE;
	IoScsiCapabilities->AdapterScansDown = FALSE;
	IoScsiCapabilities->AdapterUsesPio = FALSE;
}

NTSTATUS
RamDiskLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters)
{
	return STATUS_SUCCESS;
}

//
// PNP ID Routines
//

#ifndef countof
#define countof(a) (sizeof(a)/sizeof(a[0]))
#endif

NTSTATUS
RamDiskQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId)
{
	static CONST WCHAR* HARDWARE_IDS[] = { 
		NDASPORT_ENUMERATOR_GUID_PREFIX L"RamDisk",
		NDASPORT_ENUMERATOR_NAMED_PREFIX L"RamDisk", 
		L"GenDisk", };
	static CONST WCHAR* COMPATIBLE_IDS[] = { L"GenDisk" };
	WCHAR instanceId[20];
	WCHAR* instanceIdList[] = { instanceId };

	NTSTATUS status;
	PRAMDISK_EXTENSION ramDiskExtension;

	CONST WCHAR** idList;
	ULONG idListCount;

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

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
			RtlUlongByteSwap(ramDiskExtension->LogicalUnitAddress.Address));
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
RamDiskQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__in PWCHAR* DeviceText)
{
	NTSTATUS status;
	PRAMDISK_EXTENSION ramDiskExtension;
	size_t length;
	static CONST WCHAR RAMDISK_DEVICE_TEXT[] = L"NDAS RAM Disk";

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	switch (DeviceTextType)
	{
	case DeviceTextDescription:
		
		*DeviceText = (PWCHAR) ExAllocatePoolWithTag(
			PagedPool, 
			sizeof(RAMDISK_DEVICE_TEXT), 
			RAMDISK_PNP_TAG);
		
		if (NULL == *DeviceText)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		
		status = RtlStringCchCopyW(
			*DeviceText,
			countof(RAMDISK_DEVICE_TEXT),
			RAMDISK_DEVICE_TEXT);

		ASSERT(NT_SUCCESS(status));
		return status;

	case DeviceTextLocationInformation:
		{
			WCHAR buffer[128];
			size_t remaining;
			
			status = RtlStringCchPrintfExW(
				buffer, 
				countof(buffer),
				NULL, &remaining, 0, 
				L"VirtualAddress=%p", ramDiskExtension->StorageData);

			ASSERT(NT_SUCCESS(status));

			length = sizeof(buffer) + sizeof(WCHAR) - remaining;

			*DeviceText = ExAllocatePoolWithTag(
				PagedPool,
				length,
				RAMDISK_PNP_TAG);

			if (NULL == *DeviceText)
			{
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			status = RtlStringCbCopyW(
				*DeviceText,
				length,
				buffer);

			ASSERT(NT_SUCCESS(status));
		}
		return status;
	default:
		return STATUS_NOT_SUPPORTED;
	}
}

NTSTATUS
RamDiskQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__inout PDEVICE_CAPABILITIES Capabilities)
{
	PRAMDISK_EXTENSION ramDiskExtension;
	DEVICE_CAPABILITIES deviceCaps;

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	RtlZeroMemory(&deviceCaps, sizeof(DEVICE_CAPABILITIES));
	deviceCaps.Version = 1;
	deviceCaps.Size = sizeof(DEVICE_CAPABILITIES);
	deviceCaps.Removable = TRUE;
	deviceCaps.EjectSupported = TRUE;
	deviceCaps.SurpriseRemovalOK = FALSE;
	deviceCaps.Address = -1; // (0x00FFFFFF) | ramDiskExtension->LogicalUnitAddress;
	deviceCaps.UINumber = -1; // (0x00FFFFFF) | ramDiskExtension->LogicalUnitAddress;

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
RamDiskQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PRAMDISK_EXTENSION ramDiskExtension;
	STORAGE_DEVICE_DESCRIPTOR tmp;
	ULONG offset;
	ULONG realLength;
	ULONG remainingBufferLength;
	ULONG usedBufferLength;
	INQUIRYDATA inquiryData;
	ULONG inquiryDataLength;

	// static CONST CHAR RawPropertyData[] = "DUMMY";
	//static CONST CHAR VendorId[] = "NDAS";
	//static CONST CHAR ProductId[] = "VIRTUAL FILE DISK";
	//static CONST CHAR ProductRevision[] = "1.0";
	//static CONST CHAR SerialNumber[] = "101";

	//
	// Fill the storage device descriptor as much as possible
	//

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	RamDiskGetInquiryData(ramDiskExtension, &inquiryData);
	inquiryDataLength = sizeof(INQUIRYDATA);

	//
	// Zero out the provided buffer
	//
	RtlZeroMemory(DeviceDescriptor, BufferLength);

	realLength = sizeof(STORAGE_DEVICE_DESCRIPTOR) - 1 +
		inquiryDataLength +
		sizeof(inquiryData.VendorId) +
		sizeof(inquiryData.ProductId) +
		sizeof(inquiryData.ProductRevisionLevel);
		// sizeof(inquiryData.)
		//sizeof(VendorId) + 
		//sizeof(ProductId) +
		//sizeof(ProductRevision) + 
		// sizeof(SerialNumber);

	RtlZeroMemory(&tmp, sizeof(STORAGE_DEVICE_DESCRIPTOR));
	tmp.Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
	tmp.Size = realLength;
	tmp.DeviceType = inquiryData.DeviceType;
	tmp.DeviceTypeModifier = inquiryData.DeviceTypeModifier;
	tmp.RemovableMedia = inquiryData.RemovableMedia;
	tmp.CommandQueueing = inquiryData.CommandQueue;
	tmp.VendorIdOffset;
	tmp.ProductIdOffset;
	tmp.ProductRevisionOffset;
	tmp.SerialNumberOffset;
	tmp.BusType = ramDiskExtension->StorageBusType;
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
			&inquiryData, inquiryDataLength,
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
			inquiryData.VendorId, sizeof(inquiryData.VendorId), 
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
			inquiryData.ProductId, sizeof(inquiryData.ProductId), 
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
			inquiryData.ProductRevisionLevel, sizeof(inquiryData.ProductRevisionLevel), 
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
RamDiskQueryStorageDeviceIdProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	*ResultLength = 0;
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS
RamDiskQueryStorageAdapterProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__out_bcount(BufferLength) PSTORAGE_ADAPTER_DESCRIPTOR AdapterDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PRAMDISK_EXTENSION ramDiskExtension;
	PIO_SCSI_CAPABILITIES capabilities;
	STORAGE_ADAPTER_DESCRIPTOR tmp;

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	capabilities = &ramDiskExtension->IoScsiCapabilities;

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

	tmp.BusType = ramDiskExtension->StorageBusType;
	tmp.BusMajorVersion = ramDiskExtension->StorageBusMajorVersion;
	tmp.BusMinorVersion = ramDiskExtension->StorageBusMinorVersion;

	*ResultLength = min(BufferLength, sizeof(STORAGE_ADAPTER_DESCRIPTOR));

	RtlCopyMemory(
		AdapterDescriptor,
		&tmp,
		*ResultLength);

	return STATUS_SUCCESS;
}

NTSTATUS
RamDiskQueryIoScsiCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__out PIO_SCSI_CAPABILITIES IoScsiCapabilities,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PRAMDISK_EXTENSION ramDiskExtension;
	PIO_SCSI_CAPABILITIES capabilities;

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	capabilities = &ramDiskExtension->IoScsiCapabilities;

	ASSERT(sizeof(IO_SCSI_CAPABILITIES) == capabilities->Length);

	*ResultLength = min(BufferLength, sizeof(IO_SCSI_CAPABILITIES));

	RtlCopyMemory(
		IoScsiCapabilities,
		&capabilities,
		*ResultLength);

	return STATUS_SUCCESS;
}

VOID
RamDiskGetInquiryData(
	__in PRAMDISK_EXTENSION RamDiskExtension,
	__inout PINQUIRYDATA InquiryData)
{
	static CONST UCHAR VendorId[8] = "NDAS";
	static CONST UCHAR ProductId[16] = "RAM Disk";
	static CONST UCHAR ProductRevisionLevel[4] = "1.0";

	//
	// REFERENCES: SPC-2 R20
	//
	RtlZeroMemory(InquiryData, sizeof(INQUIRYDATA));

	InquiryData->DeviceType = DIRECT_ACCESS_DEVICE;
	InquiryData->DeviceTypeQualifier = DEVICE_CONNECTED;
	InquiryData->RemovableMedia = FALSE;
	InquiryData->AdditionalLength = 0;
	InquiryData->DeviceTypeModifier;
	//
	// VERSION
	//
	// 0x02: ANSI X3.131:1994.
	// 0x03: ANSI X3.301:1997.
	// 0x04: SPC-2 T10/1236-D Revision 20
	//
	InquiryData->Versions = 0x02;
	InquiryData->ResponseDataFormat = 2;
	InquiryData->HiSupport;
	InquiryData->NormACA;
	InquiryData->AERC;
	InquiryData->AdditionalLength;
	InquiryData->SoftReset;
	InquiryData->CommandQueue;
	InquiryData->LinkedCommands;
	InquiryData->Synchronous;
	InquiryData->Wide16Bit;
	InquiryData->Wide32Bit;
	InquiryData->RelativeAddressing;

	InquiryData->VendorId[8-1];
	InquiryData->ProductId[16-1];
	InquiryData->ProductRevisionLevel[4-1];

	RtlCopyMemory(InquiryData->VendorId, VendorId, sizeof(VendorId));
	RtlCopyMemory(InquiryData->ProductId, ProductId, sizeof(ProductId));
	RtlCopyMemory(InquiryData->ProductRevisionLevel, ProductRevisionLevel, 
		sizeof(ProductRevisionLevel));

	InquiryData->VendorSpecific[20-1];
}

//
// Scsi IO Routines
//

BOOLEAN
RamDiskBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	return TRUE;
}

VOID
RamDiskProcessIo(
	__in PRAMDISK_EXTENSION RamDiskExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;

	PCDB cdb = (PCDB) Srb->Cdb;
	LONG64 startingBlockAddress;
	LONG64 endBlockAddress;
	ULONG transferBlockCount;
	LONG64 startingByteAddress;
	ULONG transferBytes;

	LARGE_INTEGER offset;
	IO_STATUS_BLOCK ioStatusBlock;

	status = NdasPortRetrieveCdbBlocks(Srb, &startingBlockAddress, &transferBlockCount);
	if (!NT_SUCCESS(status))
	{
		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;
		return;
	}

	endBlockAddress = startingBlockAddress + transferBlockCount - 1;

	if (endBlockAddress >= RamDiskExtension->SectorCount)
	{
		NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_WARNING,
			"SCSIOP_READ/WRITE/VERIFY out of bounds (%I64Xh->%I64Xh,%Xh)\n", 
			startingBlockAddress, endBlockAddress, transferBlockCount);
		
		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;
		return;
	}

	startingByteAddress = startingBlockAddress * RamDiskExtension->SectorSize;
	transferBytes = transferBlockCount * RamDiskExtension->SectorSize;

	if (cdb->CDB10.OperationCode != SCSIOP_VERIFY &&
		transferBytes != Srb->DataTransferLength)
	{
		NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_WARNING,
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
	case SCSIOP_WRITE:

		NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_%s: (%I64Xh->%I64Xh,%Xh) ByteOffset=%I64Xh (%Xh bytes)\n",
			(SCSIOP_READ == Srb->Cdb[0]) ? "READ" : "WRITE",
			startingBlockAddress, endBlockAddress, transferBlockCount,
			startingByteAddress, transferBytes);

		offset.QuadPart = startingByteAddress;

		switch (Srb->Cdb[0])
		{
		case SCSIOP_READ:
			{
				NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_ERROR,
					"Read: Offset=%08I64Xh, Length=%Xh\n", 
					offset.QuadPart, transferBytes);
				RtlCopyMemory(
					Srb->DataBuffer,
					NdasPortOffsetOf(RamDiskExtension->StorageData, offset.LowPart),
					transferBytes);
			}
			break;
		case SCSIOP_WRITE:
			{
				NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_ERROR,
					"Write: Offset=%08I64Xh, Length=%Xh\n", 
					offset.QuadPart, transferBytes);
				RtlCopyMemory(
					NdasPortOffsetOf(RamDiskExtension->StorageData, offset.LowPart),
					Srb->DataBuffer,
					transferBytes);
			}
			break;
		DEFAULT_UNREACHABLE;
		}

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_ERROR,
				"Read/Write failed: Status=%08x\n", status);
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
		NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_INFORMATION,
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

BOOLEAN
RamDiskStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	PRAMDISK_EXTENSION ramDiskExtension;

	ramDiskExtension = RamDiskGetExtension(DeviceExtension);

	if (SRB_FUNCTION_EXECUTE_SCSI != Srb->Function)
	{
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;

		NdasPortNotification(
			RequestComplete,
			DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			DeviceExtension);

		return TRUE;
	}

	switch (Srb->Cdb[0])
	{
	case SCSIOP_READ:
	case SCSIOP_WRITE:
	case SCSIOP_VERIFY:
		{
			ASSERT(NULL == ramDiskExtension->CurrentSrb);
			ramDiskExtension->CurrentSrb = Srb;

			RamDiskProcessIo(ramDiskExtension, ramDiskExtension->CurrentSrb);

			ramDiskExtension->CurrentSrb = NULL;
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

				//
				// last sector 
				//
				logicalBlockAddress = ramDiskExtension->SectorCount - 1;

				NdasPortReverseBytes(
					(PFOUR_BYTE)&readCapacityData->LogicalBlockAddress, 
					(PFOUR_BYTE)&logicalBlockAddress);

				NdasPortReverseBytes(
					(PFOUR_BYTE)&readCapacityData->BytesPerBlock, 
					(PFOUR_BYTE)&ramDiskExtension->SectorSize);

				Srb->SrbStatus = SRB_STATUS_SUCCESS;
				Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);

				NdasPortTrace(RAMDISK_IO, TRACE_LEVEL_INFORMATION,
					"Capacity Reported (Big-Endian): LBA=%08X, BytesPerBlock=%08X\n",
					readCapacityData->LogicalBlockAddress,
					readCapacityData->BytesPerBlock);
			}
		}
		break;
	case SCSIOP_INQUIRY:
		{
			INQUIRYDATA inquiryData = {0};
			ULONG readDataLength;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_OUT);

			RamDiskGetInquiryData(
				ramDiskExtension,
				&inquiryData);

			readDataLength = min(Srb->DataTransferLength, sizeof(INQUIRYDATA));

			//
			// Fill as much as possible
			//

			RtlCopyMemory(
				Srb->DataBuffer,
				&inquiryData,
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
		DeviceExtension,
		Srb);

	NdasPortNotification(
		NextLuRequest,
		DeviceExtension);

	return TRUE;
}


