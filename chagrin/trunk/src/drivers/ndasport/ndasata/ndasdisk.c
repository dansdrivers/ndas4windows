#include "ndasdisk.h"

#ifdef RUN_WPP
#include "ndasdisk.tmh"
#endif

//
// implementation flags
//

// #define NDASATA_ENABLE_IDENTIFY_CHECKSUM

ATA_DEVICE_TYPE 
AtaDeviceTypes[] = {
	"Disk",       "GenDisk",       "DiskPeripheral",
	"Sequential", "GenSequential", "TapePeripheral",
	"Printer",    "GenPrinter",    "PrinterPeripheral",
	"Processor",  "GenProcessor",  "ProcessorPeripheral",
	"Worm",       "GenWorm",       "WormPeripheral",
	"CdRom",      "GenCdRom",      "CdRomPeripheral",
	"Scanner",    "GenScanner",    "ScannerPeripheral",
	"Optical",    "GenOptical",    "OpticalDiskPeripheral",
	"Changer",    "GenChanger",    "MediumChangerPeripheral",
	"Net",        "GenNet",        "CommunicationPeripheral"
};

NDAS_LOGICALUNIT_INTERFACE 
NdasAtaDeviceInterface = {
	sizeof(NDAS_LOGICALUNIT_INTERFACE),
	NDAS_LOGICALUNIT_INTERFACE_VERSION,
	NdasAtaDeviceInitializeLogicalUnit,
	NdasAtaDeviceCleanupLogicalUnit,
	NdasAtaDeviceLogicalUnitControl,
	NdasAtaDeviceBuildIo,
	NdasAtaDeviceStartIo,
	NdasAtaDeviceQueryPnpID,
	NdasAtaDeviceQueryPnpDeviceText,
	NdasAtaDeviceQueryPnpDeviceCapabilities,
	NdasAtaDeviceQueryStorageDeviceProperty,
	NdasAtaDeviceQueryStorageDeviceIdProperty
};

NTSTATUS
NdasAtaDeviceGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize)
{
	if (NdasDiskDevice != LogicalUnitDescriptor->Type &&
		NdasAtaDevice != LogicalUnitDescriptor->Type)
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
		&NdasAtaDeviceInterface,
		sizeof(NDAS_LOGICALUNIT_INTERFACE));

	*LogicalUnitExtensionSize = sizeof(NDAS_ATA_DEVICE_EXTENSION);
	*SrbExtensionSize = 0;

	return STATUS_SUCCESS;
}

PNDAS_ATA_DEVICE_EXTENSION
FORCEINLINE
NdasAtaDeviceGetExtension(PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	return (PNDAS_ATA_DEVICE_EXTENSION) NdasPortGetLogicalUnit(DeviceExtension, 0, 0, 0);
}

NTSTATUS
NdasAtaDeviceInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	NTSTATUS status;
	PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	PLSP_IO_SESSION LspIoSession;
	PLSP_TRANSPORT_ADDRESS lspTransportAddress;

	ULONG i;

	PAGED_CODE();

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);
	ndasAtaDeviceExtension->DeviceExtension = DeviceExtension;

	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_FATAL,
			"LogicalUnitDescriptor version is invalid. Version=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Version,
			sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER_1;
	}
	if (LogicalUnitDescriptor->Size < sizeof(NDAS_ATA_DEVICE_DESCRIPTOR))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_FATAL,
			"NdasAtaDeviceDescriptor Size is invalid. Size=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Size,
			sizeof(NDAS_ATA_DEVICE_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER_2;
	}

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_FATAL,
		"Initializing NDAS ATA Device Logical Unit\n");

	ndasAtaDeviceDescriptor = (PNDAS_ATA_DEVICE_DESCRIPTOR) LogicalUnitDescriptor;

	//
	// The actual value will be initialized at startup
	//

	ndasAtaDeviceExtension->LogicalUnitAddress = 
		ndasAtaDeviceDescriptor->Header.Address;

	ndasAtaDeviceExtension->VirtualBytesPerBlock = 
		ndasAtaDeviceDescriptor->VirtualBytesPerBlock;

	ndasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart = 
		ndasAtaDeviceDescriptor->VirtualLogicalBlockAddress.QuadPart;

	ndasAtaDeviceExtension->DeviceObject = NdasPortExGetWdmDeviceObject(DeviceExtension);

	ndasAtaDeviceExtension->ResetConnectionWorkItem = 
		IoAllocateWorkItem(ndasAtaDeviceExtension->DeviceObject);

	if (NULL == ndasAtaDeviceExtension->ResetConnectionWorkItem)
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"IoAllocateWorkItem failed.\n");

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = LspIoAllocateSession(
		ndasAtaDeviceExtension->DeviceObject,
		LSP_IO_DEFAULT_INTERAL_BUFFER_SIZE,
		&ndasAtaDeviceExtension->LspIoSession);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoSessionAllocate failed with status=%x\n", status);

		goto error1;
	}

	//
	// NDAS Device Address Field
	//

	if (ndasAtaDeviceDescriptor->DeviceAddressOffset > 0)
	{
		if (ndasAtaDeviceDescriptor->DeviceAddressOffset < sizeof(NDAS_ATA_DEVICE_DESCRIPTOR) ||
			ndasAtaDeviceDescriptor->DeviceAddressOffset +
			sizeof(LSP_TRANSPORT_ADDRESS) > ndasAtaDeviceDescriptor->Header.Size)
		{
			status = STATUS_INVALID_PARAMETER_3;
			goto error2;
		}

		lspTransportAddress = (PLSP_TRANSPORT_ADDRESS) NdasPortOffsetOf(
			ndasAtaDeviceDescriptor,
			ndasAtaDeviceDescriptor->DeviceAddressOffset);

		RtlCopyMemory(
			&ndasAtaDeviceExtension->DeviceAddress,
			lspTransportAddress,
			sizeof(LSP_TRANSPORT_ADDRESS));
	}
	else
	{
		C_ASSERT(
			sizeof(ndasAtaDeviceExtension->DeviceAddress.LpxAddress.Node) == 
			sizeof(ndasAtaDeviceDescriptor->NdasDeviceId.Identifier));
		//
		// Default Fallback (LSP over LPX Stream)
		//
		ndasAtaDeviceExtension->DeviceAddress.Type = LspOverLpxStream;
		
		RtlCopyMemory(
			ndasAtaDeviceExtension->DeviceAddress.LpxAddress.Node,
			ndasAtaDeviceDescriptor->NdasDeviceId.Identifier,
			sizeof(ndasAtaDeviceDescriptor->NdasDeviceId.Identifier));

		ndasAtaDeviceExtension->DeviceAddress.LpxAddress.Port =
			RtlUshortByteSwap(NDAS_DEVICE_DEFAULT_LPX_STREAM_LISTEN_PORT);
	}

	//
	// LSP Local Address Fields
	//

	if (ndasAtaDeviceDescriptor->LocalAddressOffset > 0)
	{
		if (ndasAtaDeviceDescriptor->LocalAddressOffset < sizeof(NDAS_ATA_DEVICE_DESCRIPTOR) ||
			ndasAtaDeviceDescriptor->LocalAddressOffset + 
			sizeof(LSP_TRANSPORT_ADDRESS) * ndasAtaDeviceDescriptor->LocalAddressCount >
			ndasAtaDeviceDescriptor->Header.Size)
		{
			status = STATUS_INVALID_PARAMETER_4;
			goto error2;
		}

		lspTransportAddress = (PLSP_TRANSPORT_ADDRESS) NdasPortOffsetOf(
			ndasAtaDeviceDescriptor,
			ndasAtaDeviceDescriptor->LocalAddressOffset);

		for (i = 0; 
			i < ndasAtaDeviceDescriptor->LocalAddressCount && 
			i < countof(ndasAtaDeviceExtension->LocalAddressList);
			++i)
		{
			RtlCopyMemory(
				&ndasAtaDeviceExtension->LocalAddressList[i],
				lspTransportAddress,
				sizeof(LSP_TRANSPORT_ADDRESS));
			++lspTransportAddress;
		}

		ndasAtaDeviceExtension->LocalAddressCount = i;
	}
	else
	{
		ndasAtaDeviceExtension->LocalAddressCount = 0;
	}

	//
	// Access Mode
	//

	ndasAtaDeviceExtension->RequestedAccessMode = ndasAtaDeviceDescriptor->AccessMode;
	ndasAtaDeviceExtension->CurrentAccessMode = ndasAtaDeviceExtension->RequestedAccessMode;

	//
	// Default Login
	//

	ndasAtaDeviceExtension->LspLoginInfo.login_type = LSP_LOGIN_TYPE_NORMAL;
	
	ndasAtaDeviceExtension->LspLoginInfo.unit_no = 
		ndasAtaDeviceDescriptor->NdasDeviceId.UnitNumber;

	ndasAtaDeviceExtension->LspLoginInfo.write_access = 
		(ndasAtaDeviceExtension->CurrentAccessMode & GENERIC_WRITE) ? TRUE : FALSE;
	
	RtlCopyMemory(
		&ndasAtaDeviceExtension->LspLoginInfo.password,
		ndasAtaDeviceDescriptor->DevicePassword,
		8) ; // 0x1F4A50731530EABB;

	//
	// LspIoSession
	//

	status = NdasAtaDeviceLspCreateConnection(ndasAtaDeviceExtension);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"NdasAtaDeviceLspCreateConnection failed with status=%x\n", status);

		goto error2;
	}

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"NdasAtaDeviceInitializeLogicalUnit completed.\n");

	return STATUS_SUCCESS;

error2:

	LspIoFreeSession(ndasAtaDeviceExtension->LspIoSession);

error1:

	IoFreeWorkItem(ndasAtaDeviceExtension->ResetConnectionWorkItem);
	ndasAtaDeviceExtension->ResetConnectionWorkItem = NULL;

	return status;
}

VOID
NdasAtaDeviceCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	NTSTATUS status;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	
	PAGED_CODE();

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
		"NdasAtaDeviceCleanupLogicalUnit: DeviceObject=%p\n", ndasAtaDeviceExtension->DeviceObject);

	if (!TestFlag(ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED))
	{
		//
		// Flush synchronously
		//
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
			"Flushing: NdasAtaDeviceExtension=%p\n", ndasAtaDeviceExtension);

		status = LspIoAtaFlushCache(
			ndasAtaDeviceExtension->LspIoSession,
			NULL,
			NULL);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
				"LspIoAtaFlushCache failed during cleanup, Status=%08X\n", status);
		}

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
			"Logging out: NdasAtaDeviceExtension=%p\n", ndasAtaDeviceExtension);

		status = LspIoLogout(
			ndasAtaDeviceExtension->LspIoSession,
			NULL,
			NULL);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
				"LspIoLogout failed during cleanup, Status=%08X\n", status);
		}

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
			"Closing Connection: NdasAtaDeviceExtension=%p\n", ndasAtaDeviceExtension);

		NdasAtaDeviceLspCloseConnection(ndasAtaDeviceExtension);

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
			"Closing Connection: NdasAtaDeviceExtension=%p\n", ndasAtaDeviceExtension);
	}


	ASSERT(NULL != ndasAtaDeviceExtension->LspIoSession);
	LspIoFreeSession(ndasAtaDeviceExtension->LspIoSession);
	ndasAtaDeviceExtension->LspIoSession = NULL;

	ASSERT(NULL != ndasAtaDeviceExtension->ResetConnectionWorkItem);
	IoFreeWorkItem(ndasAtaDeviceExtension->ResetConnectionWorkItem);
	ndasAtaDeviceExtension->ResetConnectionWorkItem = NULL;
}

VOID
FORCEINLINE
NdasAtaDeviceInitializeInquiryData(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	static CONST UCHAR VendorId[8] = "NDAS";
	// static CONST UCHAR ProductId[16] = "RAM Disk";
	// static CONST UCHAR ProductRevisionLevel[4] = "1.0";

	CONST ATA_IDENTIFY_DATA* identifyData = &NdasAtaDeviceExtension->IdentifyData;
	PINQUIRYDATA inquiryData = &NdasAtaDeviceExtension->InquiryData;

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

	identifyData->serial_no;
	identifyData->model;
	identifyData->fw_rev;

	RtlCopyMemory(
		inquiryData->VendorId, 
		VendorId, 
		sizeof(inquiryData->VendorId));

	RtlCopyMemory(
		inquiryData->ProductId, 
		NdasAtaDeviceExtension->DeviceModel, 
		sizeof(inquiryData->ProductId));

	RtlCopyMemory(
		inquiryData->ProductRevisionLevel, 
		NdasAtaDeviceExtension->DeviceFirmwareRevision,
		sizeof(inquiryData->ProductRevisionLevel));

	inquiryData->VendorSpecific[20-1];
}


NTSTATUS
NdasAtaDeviceLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters)
{
	switch (ControlType)
	{
	case NdasStopLogicalUnit:
	case NdasRestartLogicalUnit:
	default:
		;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasAtaDeviceBuildCompatId(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId)
{
	NTSTATUS status;
	PTSTR deviceTypeCompatIdString = "GenDisk";
	switch (Index)
	{
	case 0:
		status = RtlUnicodeStringPrintf(
			UnicodeStringId,
			L"%hs",
			deviceTypeCompatIdString);
		break;
	default:
		status = STATUS_NO_MORE_ENTRIES;
	}

	//
	// Fix-up ID's
	//
	if (STATUS_NO_MORE_ENTRIES != status)
	{
		NdasPortFixDeviceId(
			UnicodeStringId->Buffer,
			UnicodeStringId->Length);
	}

	return status;
}

NTSTATUS
NdasAtaDeviceBuildHardwareId(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId)
{
	NTSTATUS status;
	PSTR deviceTypeIdString = "Disk";

	// 0: GUID Enumerator + Device Type + [Vendor + Product] + Revision
	// 1: Enumerator + Device Type + [Vendor + Product] + Revision
	// 2: Enumerator + [Vendor + Product] + Revision[0]
	// 3: Enumerator + Device Type + [Vendor + Product]
	// 4: [Vendor + Product] + Revision[0] (Win9X format)
	// 5: Compatible Id String (e.g. GenDisk)
	switch (Index)
	{
	case 0:
		status = RtlUnicodeStringPrintf(
			UnicodeStringId,
			NDASPORT_ENUMERATOR_GUID_PREFIX L"%hs%40hs%8hs",
			deviceTypeIdString,
			NdasAtaDeviceExtension->DeviceModel,
			NdasAtaDeviceExtension->DeviceFirmwareRevision);
		break;
	case 1:
		status = RtlUnicodeStringPrintf(
			UnicodeStringId,
			NDASPORT_ENUMERATOR_NAMED_PREFIX L"%hs%40hs%8hs",
			deviceTypeIdString,
			NdasAtaDeviceExtension->DeviceModel,
			NdasAtaDeviceExtension->DeviceFirmwareRevision);
		break;
	case 2:
		status = RtlUnicodeStringPrintf(
			UnicodeStringId,
			NDASPORT_ENUMERATOR_NAMED_PREFIX L"%40hs%8hs",
			NdasAtaDeviceExtension->DeviceModel,
			NdasAtaDeviceExtension->DeviceFirmwareRevision);
		break;
	case 3:
		status = RtlUnicodeStringPrintf(
			UnicodeStringId,
			NDASPORT_ENUMERATOR_NAMED_PREFIX L"%hs%40hs",
			deviceTypeIdString,
			NdasAtaDeviceExtension->DeviceModel);
		break;
	case 4:
		status = RtlUnicodeStringPrintf(
			UnicodeStringId,
			L"%40hs%8hs",
			NdasAtaDeviceExtension->DeviceModel,
			NdasAtaDeviceExtension->DeviceFirmwareRevision);
		break;
	default:
		// Append Device Compatible ID
		status = NdasAtaDeviceBuildCompatId(
			NdasAtaDeviceExtension,
			Index - 5,
			UnicodeStringId);
		break;
	}
	
	//
	// Fix-up ID's
	//
	if (STATUS_NO_MORE_ENTRIES != status)
	{
		NdasPortFixDeviceId(
			UnicodeStringId->Buffer,
			UnicodeStringId->Length);
	}

	return status;
}

NTSTATUS
NdasAtaDeviceQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId)
{
	NTSTATUS status;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	switch (QueryType)
	{
	case BusQueryDeviceID:
		status = (Index > 0) ? STATUS_NO_MORE_ENTRIES : 
			NdasAtaDeviceBuildHardwareId(
				ndasAtaDeviceExtension,
				Index,
				UnicodeStringId);
		break;
	case BusQueryHardwareIDs:
		status = NdasAtaDeviceBuildHardwareId(
			ndasAtaDeviceExtension,
			Index,
			UnicodeStringId);
		break;
	case BusQueryCompatibleIDs:
		status = NdasAtaDeviceBuildCompatId(
			ndasAtaDeviceExtension,
			Index,
			UnicodeStringId);
		break;
	case BusQueryInstanceID:
		status = (Index > 0) ? STATUS_NO_MORE_ENTRIES :
			RtlUnicodeStringPrintf(
				UnicodeStringId,
				L"%08X", 
				RtlUlongByteSwap(ndasAtaDeviceExtension->LogicalUnitAddress.Address));
		break;
	case 4 /*BusQueryDeviceSerialNumber*/:
	default:
		return STATUS_NOT_SUPPORTED;
	}

	ASSERT(STATUS_NO_MORE_ENTRIES == status || NT_SUCCESS(status));

	return status;
}

NTSTATUS
NdasAtaDeviceQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__out PWCHAR* DeviceText)
{
	NTSTATUS status;
	CHAR ansiBuffer[256];
	ANSI_STRING ansiText;
	UNICODE_STRING unicodeText;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;

	PAGED_CODE();

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	RtlInitUnicodeString(&unicodeText, NULL);

	switch (DeviceTextType)
	{
	case DeviceTextDescription:
		{
			PCHAR c;
			LONG i;

			RtlZeroMemory(
				ansiBuffer, 
				sizeof(ansiBuffer));

			RtlCopyMemory(
				ansiBuffer,
				ndasAtaDeviceExtension->DeviceModel,
				sizeof(ndasAtaDeviceExtension->DeviceModel));

			c = ansiBuffer;

			for (i = sizeof(ndasAtaDeviceExtension->DeviceModel); i >= 0; --i) 
			{
				if ((c[i] != '\0') &&
					(c[i] != ' ')) 
				{
					break;
				}
				c[i] = '\0';
			}
			c = &(c[i + 1]);
			*c = '\0';

			// TODO: c is not as large as countof(ansiBuffer) here!!
			status = RtlStringCchCatA(
				c, countof(ansiBuffer),
				" NDAS Disk Device");
		}
		break;
	case DeviceTextLocationInformation:
		{
			status = RtlStringCchPrintfA(
				ansiBuffer, countof(ansiBuffer),
				"Bus Number %d, Target ID %d, LUN %d",
				ndasAtaDeviceExtension->LogicalUnitAddress.PathId,
				ndasAtaDeviceExtension->LogicalUnitAddress.TargetId,
				ndasAtaDeviceExtension->LogicalUnitAddress.Lun);
		}
		break;
	default:
		return STATUS_NOT_SUPPORTED;
	}

	RtlInitAnsiString(&ansiText, ansiBuffer);
	status = RtlAnsiStringToUnicodeString(&unicodeText, &ansiText, TRUE);

	*DeviceText = unicodeText.Buffer;
	return status;
}

NTSTATUS
NdasAtaDeviceQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__inout PDEVICE_CAPABILITIES Capabilities)
{
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	DEVICE_CAPABILITIES deviceCaps;

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	RtlZeroMemory(&deviceCaps, sizeof(DEVICE_CAPABILITIES));
	deviceCaps.Version = 1;
	deviceCaps.Size = sizeof(DEVICE_CAPABILITIES);
	deviceCaps.Removable = TRUE;
	deviceCaps.EjectSupported = TRUE;
	deviceCaps.SurpriseRemovalOK = FALSE;
	//
	// masks Port Number
	//
	deviceCaps.Address = 
	deviceCaps.UINumber = (0x00FFFFFF & ndasAtaDeviceExtension->LogicalUnitAddress.Address);

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

NTSTATUS
NdasAtaDeviceQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	STORAGE_DEVICE_DESCRIPTOR tmp;
	ULONG offset;
	ULONG realLength;
	ULONG remainingBufferLength;
	ULONG usedBufferLength;
	PINQUIRYDATA inquiryData;
	ULONG inquiryDataLength;

	//
	// Fill the storage device descriptor as much as possible
	//

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	inquiryData = &ndasAtaDeviceExtension->InquiryData;
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
	tmp.BusType = ndasAtaDeviceExtension->StorageBusType;
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
NdasAtaDeviceQueryStorageDeviceIdProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	return STATUS_NOT_SUPPORTED;
}

BOOLEAN
NdasAtaDeviceBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	return TRUE;
}

BOOLEAN
NdasAtaDeviceStartLspIo(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;
	LARGE_INTEGER logicalBlockAddress;
	LONG64 startingBlockAddress;
	LONG64 endingBlockAddress;
	ULONG transferBlockCount;
	ULONG transferBytes;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"NdasAtaDeviceExtension=%p, Srb=%p\n", 
		NdasAtaDeviceExtension, Srb);

	ASSERT(NULL == NdasAtaDeviceExtension->CurrentSrb);
	ASSERT(!TestFlag(NdasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING));

	if (TestFlag(NdasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED))
	{
		//
		// Queue is resumed in disconnected state
		// 
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
			"Failing SRB queued for disconnection, Srb=%p\n", 
			Srb);

		//
		// Unlike NdasPortNotification, NdasPortCompleteRequest
		// completes all pending SRB's with the SRB_STATUS=SRB_STATUS_UNEXPECTED_BUS_FREE
		// 

		NdasPortCompleteRequest(
			NdasAtaDeviceExtension->DeviceExtension,
			NdasAtaDeviceExtension->LogicalUnitAddress.PathId,
			NdasAtaDeviceExtension->LogicalUnitAddress.TargetId,
			NdasAtaDeviceExtension->LogicalUnitAddress.Lun,
			SRB_STATUS_UNEXPECTED_BUS_FREE);

		return TRUE;
	}

	status = NdasPortRetrieveCdbBlocks(
		Srb,
		&startingBlockAddress,
		&transferBlockCount);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"Invalid CDB Length: CDB ["
			"%02X %02X %02X %02X %02X %02X | "
			"%02X %02X %02X %02X | "
			"%02X %02X %02X %02X %02X %02X]\n", 
			Srb->Cdb[0], Srb->Cdb[1], Srb->Cdb[2], Srb->Cdb[3], Srb->Cdb[4], Srb->Cdb[5],
			Srb->Cdb[6], Srb->Cdb[7], Srb->Cdb[8], Srb->Cdb[9],
			Srb->Cdb[10], Srb->Cdb[11], Srb->Cdb[12], Srb->Cdb[13], Srb->Cdb[14], Srb->Cdb[15]);

		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;

		NdasPortNotification(
			RequestComplete,
			NdasAtaDeviceExtension->DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			NdasAtaDeviceExtension->DeviceExtension);

		return TRUE;
	}

	logicalBlockAddress.QuadPart = startingBlockAddress;
	endingBlockAddress = startingBlockAddress + transferBlockCount - 1;
	transferBytes = transferBlockCount * NdasAtaDeviceExtension->VirtualBytesPerBlock;

	if (endingBlockAddress > NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart)
	{
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_READ/WRITE/VERIFY: out of bounds (%I64Xh,%I64Xh:%Xh blocks,%Xh bytes)\n", 
			startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

		Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		Srb->DataTransferLength = 0;

		NdasPortNotification(
			RequestComplete,
			NdasAtaDeviceExtension->DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			NdasAtaDeviceExtension->DeviceExtension);

		return TRUE;
	}

	NdasAtaDeviceExtension->CurrentSrb = Srb;

	switch (Srb->Cdb[0])
	{
	case SCSIOP_READ:
	case SCSIOP_READ16:
	case SCSIOP_READ6:
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:
	case SCSIOP_WRITE6:
		switch (Srb->Cdb[0])
		{
		case SCSIOP_READ:
		case SCSIOP_READ16:
		case SCSIOP_READ6:

			NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
				"SCSIOP_READ: (%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n", 
				startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

			status = LspIoRead(
				NdasAtaDeviceExtension->LspIoSession,
				Srb->DataBuffer,
				&logicalBlockAddress,
				transferBytes,
				NdasAtaDeviceLspIoCompletion,
				NdasAtaDeviceExtension);

			break;
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:
		case SCSIOP_WRITE6:

			NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
				"SCSIOP_WRITE: (%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n", 
				startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

			if (NdasAtaDeviceExtension->CurrentAccessMode & GENERIC_WRITE)
			{
				status = LspIoWrite(
					NdasAtaDeviceExtension->LspIoSession,
					Srb->DataBuffer,
					&logicalBlockAddress,
					transferBytes,
					NdasAtaDeviceLspIoCompletion,
					NdasAtaDeviceExtension);
			}
			else if (NdasAtaDeviceExtension->RequestedAccessMode & GENERIC_WRITE)
			{
				Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
				Srb->DataTransferLength = 0;

				NdasAtaDeviceExtension->CurrentSrb = NULL;

				NdasPortNotification(
					RequestComplete,
					NdasAtaDeviceExtension->DeviceExtension,
					Srb);

				NdasPortNotification(
					NextLuRequest,
					NdasAtaDeviceExtension->DeviceExtension);

				return TRUE;				
			}
			else
			{
				//
				// Fake write
				//
				Srb->SrbStatus = SRB_STATUS_SUCCESS;
				Srb->DataTransferLength = transferBytes;

				NdasAtaDeviceExtension->CurrentSrb = NULL;

				NdasPortNotification(
					RequestComplete,
					NdasAtaDeviceExtension->DeviceExtension,
					Srb);

				NdasPortNotification(
					NextLuRequest,
					NdasAtaDeviceExtension->DeviceExtension);

				return TRUE;				
			}
			break;
			DEFAULT_UNREACHABLE;
		}

		break;
	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:
	case SCSIOP_VERIFY6:

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_VERIFY: (%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n", 
			startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

		status = LspIoVerify(
			NdasAtaDeviceExtension->LspIoSession,
			NULL,
			&logicalBlockAddress,
			transferBytes,
			NdasAtaDeviceLspIoCompletion,
			NdasAtaDeviceExtension);

		break;

	case SCSIOP_SYNCHRONIZE_CACHE:
	case SCSIOP_SYNCHRONIZE_CACHE16:

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_SYNCHRONIZE_CACHE: (%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n", 
			startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

		status = LspIoAtaFlushCache(
			NdasAtaDeviceExtension->LspIoSession,
			NdasAtaDeviceLspIoCompletion,
			NdasAtaDeviceExtension);

		break;
	default:
		ASSERT(FALSE);
	}

	return TRUE;
}

BOOLEAN
NdasAtaDeviceStartIoPassthrough(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

	NdasPortNotification(
		RequestComplete,
		NdasAtaDeviceExtension->DeviceExtension,
		Srb);

	NdasPortNotification(
		NextLuRequest,
		NdasAtaDeviceExtension->DeviceExtension);

	return TRUE;
}

BOOLEAN
NdasAtaDeviceStartIoFlush(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"Flush Command Queued, NdasAtaDeviceExtension=%p, Srb=%p\n", 
		NdasAtaDeviceExtension, Srb);

	NdasAtaDeviceExtension->CurrentSrb = Srb;

	status = LspIoAtaFlushCache(
		NdasAtaDeviceExtension->LspIoSession,
		NdasAtaDeviceLspIoCompletion,
		NdasAtaDeviceExtension);

	//
	// ignore status here, all errors are processed at completion
	//

	return TRUE;
}

BOOLEAN
NdasAtaDeviceStartIoSmart(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;
	PSRB_IO_CONTROL srbIoControl;
	SENDCMDINPARAMS sendCmdIn; /* a copy of SENDCMDINPARAMS */
	PSENDCMDOUTPARAMS sendCmdOut;
	LSP_IDE_REGISTER lspIdeRegister;
	ULONG dataLength;

	srbIoControl = (PSRB_IO_CONTROL) Srb->DataBuffer;

	//
	// Copy of the SENDCMDINPARAMS
	//
	sendCmdIn = *(PSENDCMDINPARAMS)NdasPortOffsetOf(
		Srb->DataBuffer, sizeof(SRB_IO_CONTROL));

	//
	// Now SendCmdOut hold the pointer to the buffer
	//
	sendCmdOut = (PSENDCMDOUTPARAMS) NdasPortOffsetOf(
		Srb->DataBuffer, sizeof(SRB_IO_CONTROL));

	if (sendCmdIn.irDriveRegs.bCommandReg != SMART_CMD)
	{
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;

		NdasPortNotification(
			RequestComplete,
			NdasAtaDeviceExtension->DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			NdasAtaDeviceExtension->DeviceExtension);

		return TRUE;
	}

#define SMART_READ_LOG          0xD5
#define SMART_WRITE_LOG         0xd6
#define SMART_LOG_SECTOR_SIZE 512

	//
	// We look into the registers not the IO control code itself!
	//
	switch (sendCmdIn.irDriveRegs.bFeaturesReg)
	{
		//
		// SMART_RCV_DRIVE_DATA
		//
	case READ_ATTRIBUTES:
	case READ_THRESHOLDS:
		dataLength = READ_ATTRIBUTE_BUFFER_SIZE;
		break;
	case SMART_READ_LOG:
		dataLength = sendCmdIn.irDriveRegs.bSectorCountReg * SMART_LOG_SECTOR_SIZE;
		break;
		//
		// SMART_SEND_DRIVE_COMMAND
		//
	case ENABLE_SMART:
	case DISABLE_SMART:
	case RETURN_SMART_STATUS:
	case ENABLE_DISABLE_AUTOSAVE:
	case EXECUTE_OFFLINE_DIAGS:
	case SAVE_ATTRIBUTE_VALUES:
	case ENABLE_DISABLE_AUTO_OFFLINE:
	case SMART_WRITE_LOG:
	default:
		dataLength = 0;
		break;
	}

	//
	// Save the data length
	//
	NdasAtaDeviceExtension->SmartDataLength = dataLength;

	RtlZeroMemory(&lspIdeRegister, sizeof(LSP_IDE_REGISTER));
	lspIdeRegister.use_48 = 0;
	lspIdeRegister.use_dma = 0;
	lspIdeRegister.reg.named.features = sendCmdIn.irDriveRegs.bFeaturesReg;
	lspIdeRegister.reg.named.sector_count = sendCmdIn.irDriveRegs.bSectorCountReg;
	lspIdeRegister.reg.named.lba_low = sendCmdIn.irDriveRegs.bSectorNumberReg;
	lspIdeRegister.reg.named.lba_mid = sendCmdIn.irDriveRegs.bCylLowReg;
	lspIdeRegister.reg.named.lba_high = sendCmdIn.irDriveRegs.bCylHighReg;
	lspIdeRegister.device.device = sendCmdIn.irDriveRegs.bDriveHeadReg;
	lspIdeRegister.command.command = sendCmdIn.irDriveRegs.bCommandReg;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"Smart Command Queued, NdasAtaDeviceExtension=%p, Srb=%p\n", 
		NdasAtaDeviceExtension, Srb);

	//
	// Sanity Check and adjustment for DEV bit in Device Register
	// As we do not report DEV bit, we should manually adjust the bit
	//
	lspIdeRegister.device.dev = NdasAtaDeviceExtension->LspLoginInfo.unit_no;

	NdasAtaDeviceExtension->CurrentSrb = Srb;

	//
	// There is no data to send in SMART command
	//
	status = LspIoIdeCommand(
		NdasAtaDeviceExtension->LspIoSession,
		&lspIdeRegister,
		NULL, 0,
		sendCmdOut->bBuffer, dataLength,
		NdasAtaDeviceLspIoCompletion, 
		NdasAtaDeviceExtension);

	//
	// ignore status here, all errors are processed at completion
	//

	return TRUE;
}

BOOLEAN
NdasAtaDeviceStartIoSrbIoControl(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	//
	// The request is an I/O control request. The SRB DataBuffer points 
	// to an SRB_IO_CONTROL header followed by the data area. The value 
	// in DataBuffer can be used by the driver, regardless of the value 
	// of MapBuffers field. If the HBA miniport driver supports this 
	// request, it should execute the request and notify the OS-specific 
	// port driver when it has completed it, using the normal mechanism 
	// of ScsiPortNotification with RequestComplete and NextRequest. 
	// Only the Function, SrbFlags, TimeOutValue, DataBuffer, 
	// DataTransferLength and SrbExtension are valid. 
	//
	PSRB_IO_CONTROL srbIoControl;
	BOOLEAN diskClassRequest;
	BOOLEAN ndasScsiRequest;

	srbIoControl = (PSRB_IO_CONTROL) Srb->DataBuffer;

	//
	// Supporting downlevel clients
	//
	if (RtlEqualMemory(srbIoControl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8))
	{
		switch (srbIoControl->ControlCode)
		{
		case NDASSCSI_IOCTL_GET_SLOT_NO:

			if (srbIoControl->Length < sizeof(ULONG))
			{
				C_ASSERT(sizeof(ULONG) == sizeof(NDAS_LOGICALUNIT_ADDRESS));
				Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
				srbIoControl->ReturnCode = SRB_STATUS_DATA_OVERRUN;
				Srb->DataTransferLength = 0;
			}
			else
			{
				PULONG slotNumber = (PULONG) NdasPortOffsetOf(
					srbIoControl, sizeof(SRB_IO_CONTROL));

				*slotNumber = NdasAtaDeviceExtension->LogicalUnitAddress.Address;
				Srb->SrbStatus = SRB_STATUS_SUCCESS;
				Srb->DataTransferLength = sizeof(ULONG);
			}
			break;

		case NDASSCSI_IOCTL_UPGRADETOWRITE:

			// TODO: not implemented yet.

		default:

			Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			Srb->DataTransferLength = 0;
			break;
		}

		NdasPortNotification(
			RequestComplete,
			NdasAtaDeviceExtension->DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			NdasAtaDeviceExtension->DeviceExtension);

		return TRUE;
	}

	diskClassRequest = RtlEqualMemory(srbIoControl->Signature, "SCSIDISK", 8);

	if (!diskClassRequest)
	{
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"IOCTL_SCSI_MINIPORT from '%c%c%c%c%c%c%c%c' other than SCSIDISK, ignored!\n",
			srbIoControl->Signature[0], srbIoControl->Signature[1],
			srbIoControl->Signature[2], srbIoControl->Signature[3],
			srbIoControl->Signature[4], srbIoControl->Signature[5],
			srbIoControl->Signature[6], srbIoControl->Signature[7]);

		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;

		NdasPortNotification(
			RequestComplete,
			NdasAtaDeviceExtension->DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			NdasAtaDeviceExtension->DeviceExtension);

		return TRUE;
	}

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"IOCTL_SCSI_MINIPORT from SCSIDISK!\n");

	switch (srbIoControl->ControlCode)
	{
	case IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS:
	case IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS:
	case IOCTL_SCSI_MINIPORT_ENABLE_SMART:
	case IOCTL_SCSI_MINIPORT_DISABLE_SMART:
	case IOCTL_SCSI_MINIPORT_RETURN_STATUS:
	case IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE:
	case IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES:
	case IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS:
	case IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE:
	case IOCTL_SCSI_MINIPORT_READ_SMART_LOG:
	case IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG:
		return NdasAtaDeviceStartIoSmart(NdasAtaDeviceExtension, Srb);
	case IOCTL_SCSI_MINIPORT_IDENTIFY:
		{
			SENDCMDINPARAMS sendCmdIn;
			PSENDCMDOUTPARAMS sendCmdOut;

			//
			// Copy the buffer to input parameter
			//
			sendCmdIn = *((PSENDCMDINPARAMS) NdasPortOffsetOf(
				srbIoControl, sizeof(SRB_IO_CONTROL)));
			//
			// Output parameter now points to the data buffer
			//
			sendCmdOut = (PSENDCMDOUTPARAMS) NdasPortOffsetOf(
				srbIoControl, sizeof(SRB_IO_CONTROL));

			//
			// For now, we only supports ATA, not ATAPI
			// TODO: ATAPI (ATAPI_ID_CMD) or ATA (ID_CMD)
			//
			if (sendCmdIn.irDriveRegs.bCommandReg != ID_CMD)
			{
				Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
				Srb->DataTransferLength = 0;
				break;
			}

			RtlZeroMemory(
				sendCmdOut,
				FIELD_OFFSET(SENDCMDOUTPARAMS, bBuffer) +
				IDENTIFY_BUFFER_SIZE);

			sendCmdOut->cBufferSize = IDENTIFY_BUFFER_SIZE;
			sendCmdOut->DriverStatus.bDriverError = 0;
			sendCmdOut->DriverStatus.bIDEError = 0;

			RtlCopyMemory(
				sendCmdOut->bBuffer,
				&NdasAtaDeviceExtension->IdentifyData,
				sizeof(ATA_IDENTIFY_DATA));

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = 
				sizeof(SRB_IO_CONTROL) +
				FIELD_OFFSET(SENDCMDOUTPARAMS, bBuffer) +
				IDENTIFY_BUFFER_SIZE;
		}
		break;
	case IOCTL_SCSI_MINIPORT_SMART_VERSION:
		{
			PGETVERSIONINPARAMS getVersion;

			getVersion = (PGETVERSIONINPARAMS) NdasPortOffsetOf(
				srbIoControl, sizeof(SRB_IO_CONTROL));

			//
			// SMART 1.03
			//
			getVersion->bVersion = 1;
			getVersion->bRevision = 1;
			getVersion->bReserved = 0;

			//
			// TODO: Add CAP_ATAPI_ID_CMD
			//
			getVersion->fCapabilities = CAP_ATA_ID_CMD | CAP_SMART_CMD;
			//
			// Regardless of unit number, we exposes the logical unit
			// as a pseudo ATA primary master
			//
			getVersion->bIDEDeviceMap = 1;

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = 
				sizeof(SRB_IO_CONTROL) +
				sizeof(GETVERSIONINPARAMS);
		}
		break;
		//
		// Cluster Support
		//
	case IOCTL_SCSI_MINIPORT_NOT_QUORUM_CAPABLE:
	case IOCTL_SCSI_MINIPORT_NOT_CLUSTER_CAPABLE:
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;
		break;
	}

	NdasPortNotification(
		RequestComplete,
		NdasAtaDeviceExtension->DeviceExtension,
		Srb);

	NdasPortNotification(
		NextLuRequest,
		NdasAtaDeviceExtension->DeviceExtension);

	return TRUE;
}

BOOLEAN
NdasAtaDeviceStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	if (TestFlag(ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED))
	{
		//
		// SRB may be in-queue even after disconnected.
		//
		Srb->SrbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
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

	switch (Srb->Function)
	{
	case 0xC7: /* SRB_FUNCTION_ATA_POWER_PASS_THROUGH */
	case 0xC8: /* SRB_FUNCTION_ATA_PASS_THROUGH: */
		// Pass-through
		return NdasAtaDeviceStartIoPassthrough(ndasAtaDeviceExtension, Srb);
	case SRB_FUNCTION_FLUSH:
	case SRB_FUNCTION_SHUTDOWN:
		return NdasAtaDeviceStartIoFlush(ndasAtaDeviceExtension, Srb);
		// we need to send 
	case SRB_FUNCTION_IO_CONTROL:
		return NdasAtaDeviceStartIoSrbIoControl(ndasAtaDeviceExtension, Srb);
	case SRB_FUNCTION_EXECUTE_SCSI:
		/* processed at the next section */
		break;
	default:
		//
		// Unsupported command
		//
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

		NdasPortNotification(
			RequestComplete,
			DeviceExtension,
			Srb);

		NdasPortNotification(
			NextLuRequest,
			DeviceExtension);

		return TRUE;
	}

	ASSERT(SRB_FUNCTION_EXECUTE_SCSI == Srb->Function);

	switch (Srb->Cdb[0])
	{
	case SCSIOP_SYNCHRONIZE_CACHE:
	case SCSIOP_SYNCHRONIZE_CACHE16:
	case SCSIOP_READ6:
	case SCSIOP_READ:
	case SCSIOP_READ16:
	case SCSIOP_WRITE6:
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:
	case SCSIOP_VERIFY6:
	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:
		
		return NdasAtaDeviceStartLspIo(ndasAtaDeviceExtension, Srb);

	case SCSIOP_INQUIRY:
		{
			ULONG readDataLength;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_OUT);

			readDataLength = min(Srb->DataTransferLength, sizeof(INQUIRYDATA));

			//
			// Fill as much as possible
			//

			RtlCopyMemory(
				Srb->DataBuffer,
				&ndasAtaDeviceExtension->InquiryData,
				readDataLength);

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = readDataLength;
		}
		break;
	case SCSIOP_MODE_SENSE:
#if 0
		{
			PCDB cdb;
			PMODE_PARAMETER_HEADER parameterHeader;
			// PMODE_PARAMETER_BLOCK parameterBlock;

			cdb = (PCDB) Srb->Cdb;
			parameterHeader = (PMODE_PARAMETER_HEADER) Srb->DataBuffer;
			// parameterBlock = NdasPortOffsetOf(Srb->DataBuffer, sizeof(MODE_PARAMETER_HEADER));

			if (MODE_SENSE_RETURN_ALL == cdb->MODE_SENSE.PageCode ||
				MODE_SENSE_CURRENT_VALUES == cdb->MODE_SENSE.PageCode)
			{
				RtlZeroMemory(
					parameterHeader,
					sizeof(MODE_PARAMETER_HEADER));

				parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER);
				parameterHeader->MediumType = FixedMedia;
				if (ndasAtaDeviceExtension->RequestedAccessMode & GENERIC_WRITE)
				{
					// none
				}
				else
				{
					parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;
				}
				parameterHeader->BlockDescriptorLength = 0;
				Srb->SrbStatus = SRB_STATUS_SUCCESS;
				Srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER);
			}
			else
			{
				Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
				Srb->DataTransferLength = 0;
			}

		}
#endif
		//
		// This is used to determine of the media is write-protected.
		// Since IDE does not support mode sense then we will modify just the portion we need
		// so the higher level driver can determine if media is protected.
		//
		// MODE_PAGE_CACHING;
		// MODE_SENSE_CURRENT_VALUES;
		{
			PMODE_PARM_READ_WRITE_DATA mode;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_IN);
			RtlZeroMemory(Srb->DataBuffer, Srb->DataTransferLength);

			mode = (PMODE_PARM_READ_WRITE_DATA) Srb->DataBuffer;
			mode->ParameterListHeader.ModeDataLength = sizeof(MODE_PARM_READ_WRITE_DATA) - 1;
			mode->ParameterListHeader.MediumType = FixedMedia;
			if (ndasAtaDeviceExtension->RequestedAccessMode & GENERIC_WRITE)
			{
				mode->ParameterListHeader.DeviceSpecificParameter = 0;
			}
			else
			{
				mode->ParameterListHeader.DeviceSpecificParameter = MODE_DSP_WRITE_PROTECT;
			}
			mode->ParameterListHeader.BlockDescriptorLength = sizeof(mode->ParameterListBlock);

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			// Srb->DataTransferLength = Srb->DataTransferLength;
		}
		break;
	case SCSIOP_START_STOP_UNIT:
	case SCSIOP_TEST_UNIT_READY:
		Srb->SrbStatus = SRB_STATUS_SUCCESS;
		break;
	case SCSIOP_READ_CAPACITY:
		{
			//
			// Claim 512 byte blocks (big-endian).
			//
			PREAD_CAPACITY_DATA readCapacityData;
			ULONG logicalBlockAddress;

			ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_IN);

			readCapacityData = (PREAD_CAPACITY_DATA) Srb->DataBuffer;

			if (ndasAtaDeviceExtension->VirtualLogicalBlockAddress.HighPart > 0)
			{
				logicalBlockAddress = 0xFFFFFFFF;
			}
			else
			{
				logicalBlockAddress = ndasAtaDeviceExtension->VirtualLogicalBlockAddress.LowPart;
			}

			readCapacityData->LogicalBlockAddress = 
				RtlUlongByteSwap(logicalBlockAddress);

			readCapacityData->BytesPerBlock =
				RtlUlongByteSwap(ndasAtaDeviceExtension->VirtualBytesPerBlock);

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);
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
			
			readCapacityDataEx = (PREAD_CAPACITY_DATA_EX) Srb->DataBuffer;

			readCapacityDataEx->LogicalBlockAddress.QuadPart = (LONGLONG)
				RtlUlonglongByteSwap((LONGLONG)
					ndasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart);

			readCapacityDataEx->BytesPerBlock = 
				RtlUlongByteSwap(ndasAtaDeviceExtension->VirtualBytesPerBlock);
			
		}
		break;
	case SCSIOP_MODE_SELECT:
		// TODO: supports SCSIOP_MODE_SELECT
	default:
		{
			Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			Srb->DataTransferLength = 0;
		}
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


VOID
NdasAtaDeviceResetConnectionWorkItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context)
{
	NTSTATUS status;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	PSCSI_REQUEST_BLOCK srb;
	LARGE_INTEGER ConnectionInterval;
	ULONG i;
	ULONG ConnectionErrorThreshold;
	ULONG ConnectionAttemptIntervalMS;

	UNREFERENCED_PARAMETER(DeviceObject);

	//
	// IRP to restart after reconnection
	//
	ndasAtaDeviceExtension = (PNDAS_ATA_DEVICE_EXTENSION) Context;

	DebugPrint((0, "Reconnect process started.\n"));

	ConnectionErrorThreshold = 5;
	ConnectionAttemptIntervalMS = 1500;

	status = STATUS_UNEXPECTED_NETWORK_ERROR;

	NdasAtaDeviceLspCloseConnection(ndasAtaDeviceExtension);

	for (i = 0; i < ConnectionErrorThreshold; ++i)
	{
		status = NdasAtaDeviceLspCreateConnection(ndasAtaDeviceExtension);

		if (NT_SUCCESS(status))
		{
			break;
		}

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
			"Reconnect failed (%d / %d)\n",
				i+1, ConnectionErrorThreshold);

		DebugPrint((0, "Reconnect failed (%d/%d).\n", i+1, ConnectionErrorThreshold));

		SetRelativeDueTimeInMillisecond(&ConnectionInterval, ConnectionAttemptIntervalMS);
		KeDelayExecutionThread(KernelMode, FALSE, &ConnectionInterval);
	}

	if (!NT_SUCCESS(status))
	{
		ClearFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING);
		SetFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED);

		DebugPrint((0, "Reconnect failed, Pdo=%p\n", DeviceObject));
		DebugPrint((0, "Unplug the device, Pdo=%p\n", DeviceObject));

		NdasPortNotification(
			BusChangeDetected,
			ndasAtaDeviceExtension->DeviceExtension, 
			ndasAtaDeviceExtension->LogicalUnitAddress.PathId);

		NdasPortCompleteRequest(
			ndasAtaDeviceExtension->DeviceExtension,
			ndasAtaDeviceExtension->LogicalUnitAddress.PathId,
			ndasAtaDeviceExtension->LogicalUnitAddress.TargetId,
			ndasAtaDeviceExtension->LogicalUnitAddress.Lun,
			SRB_STATUS_UNEXPECTED_BUS_FREE);

		return;
	}
	else 
	{
		ClearFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING);
		DebugPrint((0, "Connection established! Pdo=%p\n", DeviceObject));
	}

	srb = ndasAtaDeviceExtension->CurrentSrb;
	ndasAtaDeviceExtension->CurrentSrb = NULL;

	DebugPrint((0, "Restarting Srb=%p\n", srb));

	NdasAtaDeviceStartIo(
		ndasAtaDeviceExtension->DeviceExtension,
		srb);
}

//
// Completion Routines
//

NTSTATUS
NdasAtaDeviceLspIoCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context)
{
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	PIRP Irp;
	PIO_STACK_LOCATION IrpStack;
	PSCSI_REQUEST_BLOCK srb;
	KIRQL oldIrql;

	ndasAtaDeviceExtension = (PNDAS_ATA_DEVICE_EXTENSION) Context;
	srb = ndasAtaDeviceExtension->CurrentSrb;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"LspIoCompletion: NdasAtaDeviceExtension=%p, Srb=%p, Status=%x\n", 
			ndasAtaDeviceExtension, srb, IoStatus->Status);

	ASSERT(
		STATUS_UNEXPECTED_NETWORK_ERROR == IoStatus->Status ||
		STATUS_LSP_ERROR == IoStatus->Status ||
		STATUS_SUCCESS == IoStatus->Status);

	if (IoStatus->Status == STATUS_UNEXPECTED_NETWORK_ERROR)
	{
		//
		// Network is disrupted. Reset the session
		//

		SetFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING);

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
			"Network Error, Queuing reconnection...\n");

		IoQueueWorkItem(
			ndasAtaDeviceExtension->ResetConnectionWorkItem,
			NdasAtaDeviceResetConnectionWorkItem,
			DelayedWorkQueue,
			ndasAtaDeviceExtension);

		return STATUS_SUCCESS;
	}

	//
	// SRB_IO_CONTROL requires more processing
	//
	if (SRB_FUNCTION_IO_CONTROL == srb->Function)
	{
		NTSTATUS status;
		LSP_IDE_REGISTER lspIdeRegs;
		PSENDCMDOUTPARAMS sendCmdOut;

		sendCmdOut = (PSENDCMDOUTPARAMS) NdasPortOffsetOf(
			srb->DataBuffer, sizeof(SRB_IO_CONTROL));

		status = LspIoGetLastIdeOutputRegister(
			ndasAtaDeviceExtension->LspIoSession,
			&lspIdeRegs);
		
		ASSERT(NT_SUCCESS(status));

		sendCmdOut->cBufferSize = ndasAtaDeviceExtension->SmartDataLength;
		sendCmdOut->DriverStatus.bDriverError = 
			lspIdeRegs.command.status.err ? SMART_IDE_ERROR : 0;
		sendCmdOut->DriverStatus.bIDEError = 
			lspIdeRegs.reg.ret.err.err_na;

		//
		// RETURN_SMART_STATUS
		//
		if (RETURN_SMART_STATUS == ndasAtaDeviceExtension->SmartCommand)
		{
			PIDEREGS returnIdeRegs;
			returnIdeRegs = (PIDEREGS) sendCmdOut->bBuffer;
			returnIdeRegs->bFeaturesReg = RETURN_SMART_STATUS;
			returnIdeRegs->bSectorCountReg = lspIdeRegs.reg.named.sector_count;
			returnIdeRegs->bSectorNumberReg = lspIdeRegs.reg.named.lba_low;
			returnIdeRegs->bCylLowReg =  lspIdeRegs.reg.named.lba_mid;
			returnIdeRegs->bCylHighReg = lspIdeRegs.reg.named.lba_high;;
			returnIdeRegs->bDriveHeadReg = lspIdeRegs.device.device;
			returnIdeRegs->bCommandReg = SMART_CMD;
			sendCmdOut->cBufferSize = sizeof(IDEREGS); // 7 + reserved = 8
		}
	}

	if (NT_SUCCESS(IoStatus->Status))
	{
		srb->SrbStatus = SRB_STATUS_SUCCESS;
		srb->ScsiStatus = SCSISTAT_GOOD;
	}
	else
	{
		ASSERT(STATUS_LSP_ERROR == IoStatus->Status);

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING, "LSP Error...\n");

		srb->SrbStatus = SRB_STATUS_ERROR;
		srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
	}

	ndasAtaDeviceExtension->CurrentSrb = NULL;

	NdasPortNotification(
		RequestComplete,
		ndasAtaDeviceExtension->DeviceExtension,
		srb);

	NdasPortNotification(
		NextLuRequest,
		ndasAtaDeviceExtension->DeviceExtension);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasAtaDeviceLspCreateConnection(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	NTSTATUS status;
	PLSP_IO_SESSION LspIoSession;

	LspIoSession = NdasAtaDeviceExtension->LspIoSession;

	PAGED_CODE();

	//
	// LspIoSession
	//

	status = LspIoInitializeSession(LspIoSession);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoSessionCreate failed with status=%x\n", status);

		goto error1;
	}

	if (0 == NdasAtaDeviceExtension->LocalAddressCount)
	{
		//
		// Update Local LPX Addresses
		// 
		NdasAtaDeviceUpdateLocalLpxAddresses(NdasAtaDeviceExtension);

		status = NdasAtaDeviceConnectLpx(
			NdasAtaDeviceExtension, 
			NdasAtaDeviceExtension->LPX.LocalAddressList,
			NdasAtaDeviceExtension->LPX.LocalAddressCount);
	}
	else
	{
		status = NdasAtaDeviceConnectLpx(
			NdasAtaDeviceExtension,
			&NdasAtaDeviceExtension->LocalAddressList[0].LpxAddress,
			1);
	}

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoConnect failed with status=%x\n", status);

		goto error2;
	}

	status = LspIoLogin(
		LspIoSession,
		&NdasAtaDeviceExtension->LspLoginInfo,
		NULL,
		NULL);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoLogIn failed with status=%x\n", status);
		goto error3;
	}

	status = NdasAtaDeviceLspNegotiate(
		NdasAtaDeviceExtension,
		LspIoSession);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"NdasAtaDiskLspNegotiate failed with status=%x\n", status);

		goto error4;
	}

	return STATUS_SUCCESS;

error4:

	LspIoLogout(LspIoSession, NULL, NULL);

error3:

	NdasAtaDeviceDisconnectLpx(NdasAtaDeviceExtension);

error2:

	LspIoCleanupSession(LspIoSession);

error1:

	return status;
}

VOID
NdasAtaDeviceLspCloseConnection(
	PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	NTSTATUS status;
	PLSP_IO_SESSION LspIoSession = NdasAtaDeviceExtension->LspIoSession;

	PAGED_CODE();

	LspIoDisconnect(
		LspIoSession,
		TDI_DISCONNECT_RELEASE,
		NULL,
		NULL,
		NULL);

	LspIoCloseAddressFile(LspIoSession);
	LspIoCloseConnectionFile(LspIoSession);
	LspIoCleanupSession(LspIoSession);
}

NTSTATUS
NdasAtaDeviceLspNegotiate(
	PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;
	PATA_IDENTIFY_DATA IdentifyData = &NdasAtaDeviceExtension->IdentifyData;
	NDAS_HARDWARE_VERSION_INFO NdasHardwareVersion;

	BOOLEAN supported;
	ULONG highestSupportMode;
	ULONG selectedMWordDMAMode;
	ULONG selectedUDMAMode;
	BOOLEAN requireNewTransferMode;
	ATA_SETFEATURE_TRANSFER_MODE optimalTransferMode;

	ULONG checksumErrorRetry = 0;

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"NdasAtaDiskLspNegotiate: DeviceObject=%p, LspIoSession=%p\n",
		NdasAtaDeviceExtension->DeviceObject,
		LspIoSession);

#ifdef NDASATA_ENABLE_IDENTIFY_CHECKSUM
checksum_error_retry:
#endif

	status = LspIoAtaIdentifyDevice(
		LspIoSession,
		IdentifyData,
		NULL,
		sizeof(ATA_IDENTIFY_DATA),
		NULL,
		NULL);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoAtaIdentify failed. Status %x\n", status);
		return status;
	}

#ifdef NDASATA_ENABLE_IDENTIFY_CHECKSUM
	// The use of this word is optional. If bits (7:0) of this word contain 
	// the signature A5h, bits (15:8) contain the data structure checksum. 
	// The data structure checksum is the two’s complement of the sum of 
	// all bytes in words (254:0) and the byte consisting of bits (7:0) 
	// in word 255. Each byte shall be added with unsigned arithmetic, 
	// and overflow shall be ignored. The sum of all 512 bytes is zero 
	// when the checksum is correct.
	if ((IdentifyData->integrity_word & 0xFF) == 0xA5)
	{
		ULONG i, c;
		PUCHAR p;

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"Identify Data contains checksum (Integrity Word=%04Xh)\n", 
			IdentifyData->integrity_word);

		p = (PUCHAR) IdentifyData;
		c = 0;
		for (i = 0; i < 512; ++i)
		{
			c += *p;
		}

		if (c != 0)
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
				"Identify Data Checksum is not valid. (Retry=%d) Checksum=%02X\n",
				checksumErrorRetry,
				(IdentifyData->integrity_word & 0xFF00) >> 16);

			if (checksumErrorRetry)
			{
				return STATUS_CRC_ERROR;
			}

			++checksumErrorRetry;

			goto checksum_error_retry;
		}

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"Identify Data is good. Checksum=%02X, Signature=%02Xh\n",
			(IdentifyData->integrity_word & 0xFF00) >> 16,
			(IdentifyData->integrity_word & 0xFF));
	}
	else
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"Identify Data DOES NOT contain checksum (Integrity Word=%04Xh)\n", 
			IdentifyData->integrity_word);
	}
#endif

	//
	// DMA/PIO Mode.
	//

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"\tATA Major %Xh, Minor %Xh, Capability %04Xh\n",
		IdentifyData->major_rev_num,
		IdentifyData->minor_rev_num,
		IdentifyData->capability);

#define ATA_IDENTIFY_DATA_UDMA_MODE_MASK 0x7F
#define ATA_IDENTIFY_DATA_MWDMA_MODE_MASK 0x07

	//
	// High Byte of dma_ultra & 0x7F is the bit set of the selected mode for ultra DMA
	// Low Byte of dma_ultra & 0x7F is the bit set of the supported mode for ultra DMA
	//
	// High Byte of dma_mword & 0x07 is the bit set of the selected mode for mword DMA
	// Low Byte of dma_mword & 0x07 is the bit set of the supported mode for mword DMA
	//
	// in the select bit set, each index is the mode
	// index 0 is UDMA 0 and index 6 is UDMA 6
	// index 0 is MWDMA 0 and index 2 is MWDMA 2
	//

	selectedUDMAMode = -1;
	selectedMWordDMAMode = -1;

	supported = BitScanForward(
		&selectedUDMAMode, 
		(GetHighByte(IdentifyData->dma_ultra) & ATA_IDENTIFY_DATA_UDMA_MODE_MASK));
	if (!supported)
	{
		selectedUDMAMode = -1;
		supported = BitScanForward(
			&selectedMWordDMAMode, 
			(GetHighByte(IdentifyData->dma_mword) & ATA_IDENTIFY_DATA_MWDMA_MODE_MASK));
		if (!supported)
		{
			selectedMWordDMAMode = -1;
			// No DMA is selected
		}
	}

	ASSERT(-1 == selectedUDMAMode || selectedUDMAMode <= 6);
	ASSERT(-1 == selectedMWordDMAMode || selectedMWordDMAMode <= 2);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"\tMultiword DMA: %02Xh %02Xh (Selected,Supported) (Current Mode: %d)\n",
		GetHighByte(IdentifyData->dma_mword),
		GetLowByte(IdentifyData->dma_mword),
		selectedMWordDMAMode);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"\tUltra DMA: %02Xh %02Xh (Selected,Supported) (Current Mode: %d)\n",
		GetHighByte(IdentifyData->dma_ultra), 
		GetLowByte(IdentifyData->dma_ultra),
		selectedUDMAMode);

	NdasHardwareVersion = LspIoGetHardwareVersionInfo(LspIoSession);

	requireNewTransferMode = FALSE;
	supported = FALSE;

	//
	// Transfer Mode Selection
	//
	// PIO or MDMA or UDMA
	// 
	// optimal = none
	// if NDAS >= 2.0 rev 1
	//   if UDMA is supported
	//     optimal = the highest UDMA
	// if optimal == none
	//   if MDMA is supported
	//     optimal = the highest MDMA
	//   else
	//     optimal = PIO
	//

	optimalTransferMode = ATA_TRANSFER_MODE_UNSPECIFIED;

	//
	// Find the optimal UDMA mode except "NDAS hardware 2.0 rev 0"
	//
	if (ATA_TRANSFER_MODE_UNSPECIFIED == optimalTransferMode &&
		!(LSP_HARDWARE_VERSION_2_0 == NdasHardwareVersion.Version &&
		0 == NdasHardwareVersion.Revision))
	{
		//
		// e.g. 0000 0000 0000 0000 0000 0000 0011 1111 (UDMA6)
		// BitScanReverse returns 6
		// 
		supported = BitScanReverse(
			&highestSupportMode, 
			GetLowByte(IdentifyData->dma_ultra) & ATA_IDENTIFY_DATA_UDMA_MODE_MASK);

		if (supported)
		{
			optimalTransferMode = ATA_TRANSFER_MODE_ULTRA_DMA_0 + highestSupportMode;

			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
				"\tOptimal Transfer Mode: Ultra DMA Mode %d\n", highestSupportMode);

			if (-1 == selectedUDMAMode || selectedUDMAMode < highestSupportMode)
			{
				requireNewTransferMode = TRUE;
			}
			else
			{
				ASSERT(selectedUDMAMode == highestSupportMode);
			}
		}
	}

	//
	// Multiword DMA Mode (any NDAS hardware)
	//
	if (ATA_TRANSFER_MODE_UNSPECIFIED == optimalTransferMode)
	{
		supported = BitScanReverse(
			&highestSupportMode,
			GetLowByte(IdentifyData->dma_mword) & ATA_IDENTIFY_DATA_MWDMA_MODE_MASK);

		if (supported)
		{
			highestSupportMode = 16 - highestSupportMode + 1;
			optimalTransferMode = ATA_TRANSFER_MODE_MULTIWORD_DMA_0 + highestSupportMode;

			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
				"\tOptimal Transfer Mode: Multiword DMA Mode %d\n", highestSupportMode);

			if (-1 == selectedMWordDMAMode || selectedMWordDMAMode < highestSupportMode)
			{
				requireNewTransferMode = TRUE;
			}
			else
			{
				ASSERT(selectedMWordDMAMode == highestSupportMode);
			}
		}
	}

	//
	// PIO Mode, applicable to 1.0 or later (all NDAS hardware)
	//
	if (ATA_TRANSFER_MODE_UNSPECIFIED == optimalTransferMode)
	{
		optimalTransferMode = ATA_TRANSFER_MODE_PIO_DEFAULT_MODE;

		//
		// If neither Multiword DMA or Ultra DMA is selected,
		// the current mode is PIO mode. 
		// If PIO mode is not selected, select PIO mode using SetFeature.
		//

		if (!(-1 != selectedMWordDMAMode || -1 != selectedUDMAMode))
		{
			requireNewTransferMode = TRUE;			
		}

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tOptimal Transfer Mode: PIO Default Mode\n");
	}

	if (requireNewTransferMode)
	{
		status = LspIoAtaSetFeature(
			LspIoSession,
			AtaSfcSetTransferMode,
			optimalTransferMode, 0, 0, 0, 
			NULL, NULL);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspIoAtaSetFeature failed. Status %x\n", status);
			return status;
		}

		status = LspIoAtaIdentifyDevice(
			LspIoSession,
			IdentifyData,
			NULL,
			sizeof(ATA_IDENTIFY_DATA),
			NULL,
			NULL);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspIoAtaIdentify(2nd) failed. Status %x\n", status);
			return status;
		}

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tDMA %08Xh, UDMA %08Xh\n",
			IdentifyData->dma_mword,
			IdentifyData->dma_ultra);

		//
		// TODO: We should check if the settings are really changed!
		//
	}

	//
	// We have to mark that DMA mode is required for the future data transfer
	//
	if (optimalTransferMode >= ATA_TRANSFER_MODE_MULTIWORD_DMA_0 &&
		optimalTransferMode <= ATA_TRANSFER_MODE_ULTRA_DMA_6)
	{
		LspIoAppendIoFlag(LspIoSession, LSP_IOF_USE_DMA);
	}

	//
	// LBA
	//
	if (IdentifyData->capability & 0x02)
	{
		LspIoAppendIoFlag(LspIoSession, LSP_IOF_USE_LBA);
	}
	else 
	{
		// We do not support non-LBA mode
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"Non-LBA mode is not supported!\n");
		ASSERT(FALSE);
		return STATUS_UNSUCCESSFUL;
	}

	//
	// LBA48
	//
	if((IdentifyData->command_set_2 & 0x0400) && 
		(IdentifyData->cfs_enable_2 & 0x0400)) 
	{
		LspIoAppendIoFlag(LspIoSession, LSP_IOF_USE_LBA48);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tLBA48 Mode Enabled\n");
	}

	//
	// Set values for virtual LogicalBlockAddress and virtual BytesPerBlock
	//

	if (0 == NdasAtaDeviceExtension->VirtualBytesPerBlock)
	{
		NdasAtaDeviceExtension->VirtualBytesPerBlock = AtaGetBytesPerBlock(IdentifyData);
	}

	if (0 == NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart)
	{
		NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart = 
			AtaGetLogicalBlockAddress(IdentifyData);
	}
	else if (NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart < 0)
	{
		NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart =
			AtaGetLogicalBlockAddress(IdentifyData) +
			NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart;
	}

	//
	// Build normalized Identification Data
	//
	RtlZeroMemory(
		NdasAtaDeviceExtension->DeviceModel,
		sizeof(NdasAtaDeviceExtension->DeviceModel));

	RtlZeroMemory(
		NdasAtaDeviceExtension->DeviceFirmwareRevision,
		sizeof(NdasAtaDeviceExtension->DeviceFirmwareRevision));

	RtlZeroMemory(
		NdasAtaDeviceExtension->DeviceSerialNumber,
		sizeof(NdasAtaDeviceExtension->DeviceSerialNumber));

	RtlCopyMemory(
		NdasAtaDeviceExtension->DeviceModel,
		IdentifyData->model,
		sizeof(IdentifyData->model));

	RtlCopyMemory(
		NdasAtaDeviceExtension->DeviceFirmwareRevision,
		IdentifyData->fw_rev,
		sizeof(IdentifyData->fw_rev));

	RtlCopyMemory(
		NdasAtaDeviceExtension->DeviceSerialNumber,
		IdentifyData->serial_no,
		sizeof(IdentifyData->serial_no));

	//
	// Adjusting Identity String Data
	//

	NdasPortWordByteSwap(
		NdasAtaDeviceExtension->DeviceModel, 
		sizeof(NdasAtaDeviceExtension->DeviceModel));

	NdasPortWordByteSwap(
		NdasAtaDeviceExtension->DeviceFirmwareRevision, 
		sizeof(NdasAtaDeviceExtension->DeviceFirmwareRevision));

	NdasPortWordByteSwap(
		NdasAtaDeviceExtension->DeviceSerialNumber, 
		sizeof(NdasAtaDeviceExtension->DeviceSerialNumber));

	if (WPP_FLAG_LEVEL_ENABLED(NDASATA_INIT, TRACE_LEVEL_INFORMATION))
	{
		//
		// We should not use printf("%s", model) as those may not be null-terminated!
		//

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tModel: \"%s\"\n", NdasAtaDeviceExtension->DeviceModel);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tFirmware Revision: \"%s\"\n", NdasAtaDeviceExtension->DeviceFirmwareRevision);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tSerial Number: \"%s\"\n", NdasAtaDeviceExtension->DeviceSerialNumber);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tATA Minor Version: %02Xh (%s)\n", 
			IdentifyData->minor_rev_num,
			AtaMinorVersionNumberString(IdentifyData->minor_rev_num));

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tBytesPerBlock=%Xh\n", AtaGetBytesPerBlock(IdentifyData));

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tLogicalBlockAddress=%I64Xh\n", AtaGetLogicalBlockAddress(IdentifyData));

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tVirtualBytesPerBlock=%Xh\n", NdasAtaDeviceExtension->VirtualBytesPerBlock);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tVirtualLogicalBlockAddress=%I64Xh\n", NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart);

	}

	NdasAtaDeviceInitializeInquiryData(NdasAtaDeviceExtension);

	return STATUS_SUCCESS;
}

VOID
FORCEINLINE
NdasAtaDeviceUpdateLocalLpxAddresses(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	ULONG totalCount;

	NdasAtaDeviceExtension->LPX.LocalAddressCount = 
		countof(NdasAtaDeviceExtension->LPX.LocalAddressList);
	
	NdasPortGetLpxLocalAddressList(
		NdasAtaDeviceExtension->LPX.LocalAddressList,
		&NdasAtaDeviceExtension->LPX.LocalAddressCount,
		&totalCount,
		&NdasAtaDeviceExtension->LPX.LocalAddressUpdateCounter);
}

VOID
NdasAtaDeviceDisconnectLpx(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	PLSP_IO_SESSION LspIoSession = NdasAtaDeviceExtension->LspIoSession;
	
	LspIoDisconnect(
		LspIoSession,
		TDI_DISCONNECT_RELEASE,
		NULL,
		NULL,
		NULL);

	LspIoCloseAddressFile(LspIoSession);
	LspIoCloseConnectionFile(LspIoSession);
}

NTSTATUS
NdasAtaDeviceConnectLpx(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PTDI_ADDRESS_LPX LocalAddressList,
	__in ULONG LocalAddressCount)
{
	NTSTATUS status;
	PLSP_IO_SESSION LspIoSession;
	ULONG i;

	LspIoSession = NdasAtaDeviceExtension->LspIoSession;

	//
	// Make connections using any available local addresses
	//
	status = STATUS_INVALID_PARAMETER;

	for (i = 0; i < LocalAddressCount; ++i)
	{
		LSP_TRANSPORT_ADDRESS lspLocalAddress;

		status = LspIoCreateConnectionFile(LspIoSession);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspIoCreateConnectionFile failed with status=%x\n", status);
			continue;
		}

		lspLocalAddress.Type = LspOverLpxStream;

		RtlCopyMemory(
			&lspLocalAddress.LpxAddress,
			&LocalAddressList[i],
			sizeof(TDI_ADDRESS_LPX));

		status = LspIoCreateAddressFile(
			LspIoSession,
			&lspLocalAddress);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspIoCreateAddressFile failed with status=%x\n", status);

			LspIoCloseConnectionFile(LspIoSession);

			continue;
		}

		status = LspIoConnect(
			LspIoSession,
			&NdasAtaDeviceExtension->DeviceAddress,
			NULL,
			NULL,
			NULL);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspIoConnect failed with status=%x\n", status);

			LspIoCloseAddressFile(LspIoSession);
			LspIoCloseConnectionFile(LspIoSession);

			continue;
		}

		RtlCopyMemory(
			&NdasAtaDeviceExtension->ConnectedLocalAddress,
			&lspLocalAddress,
			sizeof(LSP_TRANSPORT_ADDRESS));

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoConnect LPX connection established.\n");
		
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"Local Address : %02X:%02X:%02X:%02X:%02X:%02X.%04X.\n", 
			NdasAtaDeviceExtension->ConnectedLocalAddress.LpxAddress.Node[0],
			NdasAtaDeviceExtension->ConnectedLocalAddress.LpxAddress.Node[1],
			NdasAtaDeviceExtension->ConnectedLocalAddress.LpxAddress.Node[2],
			NdasAtaDeviceExtension->ConnectedLocalAddress.LpxAddress.Node[3],
			NdasAtaDeviceExtension->ConnectedLocalAddress.LpxAddress.Node[4],
			NdasAtaDeviceExtension->ConnectedLocalAddress.LpxAddress.Node[5],
			RtlUshortByteSwap(lspLocalAddress.LpxAddress.Port));

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"Device Address: %02X:%02X:%02X:%02X:%02X:%02X.%04X.\n", 
			NdasAtaDeviceExtension->DeviceAddress.LpxAddress.Node[0],
			NdasAtaDeviceExtension->DeviceAddress.LpxAddress.Node[1],
			NdasAtaDeviceExtension->DeviceAddress.LpxAddress.Node[2],
			NdasAtaDeviceExtension->DeviceAddress.LpxAddress.Node[3],
			NdasAtaDeviceExtension->DeviceAddress.LpxAddress.Node[4],
			NdasAtaDeviceExtension->DeviceAddress.LpxAddress.Node[5],
			RtlUshortByteSwap(lspLocalAddress.LpxAddress.Port));

		break;
	}

	return status;
}

LONGLONG
FORCEINLINE
AtaGetLogicalBlockAddress(PATA_IDENTIFY_DATA IdentifyData)
{
	LONGLONG totalsectors;
	/* 48-bit address feature set is available and enabled? */
	if ((IdentifyData->command_set_2 & 0x0400) && 
		(IdentifyData->cfs_enable_2 & 0x0400))
	{
		totalsectors = IdentifyData->lba_capacity_2;
		return (totalsectors - 1);
	}
	else
	{
		totalsectors = IdentifyData->lba_capacity;
		return (totalsectors - 1);
	}
}

ULONG
FORCEINLINE
AtaGetBytesPerBlock(PATA_IDENTIFY_DATA IdentifyData)
{
	/* word 106, 107
	106 O Physical sector size / Logical Sector Size
	F 15 Shall be cleared to zero
	F 14 Shall be set to one
	F 13 1 = Device has multiple logical sectors per physical sector.
	  12 1 = Device Logical Sector Longer than 256 Words
	F 11-4 Reserved
	F 3-0 2^X logical sectors per physical sector
	*/

	typedef struct _ATA_SECTOR_SIZE_DATA {
		UCHAR Reserved : 4;              /* 8 - 11 */
		UCHAR LargeLogicalSector    : 1; /* 12 */
		UCHAR MultipleLogicalSector : 1; /* 13 */
		UCHAR IsOne  : 1;                /* 14 */
		UCHAR IsZero : 1;                /* 15 */
		UCHAR LogicalSectorsPerPhysicalSectorInPowerOfTwo : 4; /* 0 - 3 */
		UCHAR Reserved2 : 4;             /* 4 - 7 */
	} ATA_SECTOR_SIZE_DATA, *PATA_SECTOR_SIZE_DATA;

	C_ASSERT(sizeof(ATA_SECTOR_SIZE_DATA) == 2);
	//
	// If 15 is zero and 14 is one, this information is valid
	//
	PATA_SECTOR_SIZE_DATA sectorSizeData = (PATA_SECTOR_SIZE_DATA)
		&IdentifyData->sector_info;
	// &IdentifyData->words104_125[106-104];

	ULONG bytesPerBlock = 512;
	
	if (1 == sectorSizeData->IsOne && 0 == sectorSizeData->IsZero)
	{
		if (sectorSizeData->LargeLogicalSector)
		{
			// Words 117,118 indicate the size of device logical sectors in words. 
			// The value of words 117,118 shall be equal to or greater than 256. 
			// The value in words 117,118 shall be valid when word 106 bit 12 
			// is set to 1. All logical sectors on a device shall be 
			// 117,118 words long.
			GetUlongFromArray(
				// (PUCHAR)&IdentifyData->words104_125[117-104],
				(PUCHAR) &IdentifyData->logical_sector_size,
				&bytesPerBlock);
		}
	}

	return bytesPerBlock;
}
