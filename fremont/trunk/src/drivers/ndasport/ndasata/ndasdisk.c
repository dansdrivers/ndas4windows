#include "ndasdisk.h"
#include <wmistr.h>
#include <initguid.h>
#include "ndasataguid.h"
#include "ndasata.wmi.h"

#ifdef RUN_WPP
#include "ndasdisk.tmh"
#endif

#define NDASATA_BMF_RESOURCENAME L"NDASATAWMI"

enum {
	NDASATA_TAG_WMI = 'watn',
};

WMIGUIDREGINFO 
NdasAtaWmiGuidList[] =
{
	&NdasAtaWmi_Connection_Link_Event_GUID, 
		1, WMIREG_FLAG_EVENT_ONLY_GUID,
	&NdasAtaWmi_Connection_FlowControlEvent_GUID, 
		1, WMIREG_FLAG_EVENT_ONLY_GUID,
};

enum { 
	NDASATA_WMI_LINK_EVENT_INDEX,
	NDASATA_WMI_FLOWCONTROL_EVENT_INDEX,
};

typedef enum _NDAS_ATA_EVENT_TYPE {
	NDAS_ATA_EVENT_ONLINE,
	NDAS_ATA_EVENT_OFFLINE,
	NDAS_ATA_EVENT_CONNECTING,
} NDAS_ATA_EVENT_TYPE;

NTSTATUS
NdasAtapFireConnectionEvent(
	__in PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension,
	__in NDAS_ATA_EVENT_TYPE EventType);

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
NdasAtapUpdateDeviceData(
	PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension);

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

FORCEINLINE
PNDAS_ATA_DEVICE_EXTENSION
NdasAtaDeviceGetExtension(PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	return (PNDAS_ATA_DEVICE_EXTENSION) NdasPortGetLogicalUnit(DeviceExtension, 0, 0, 0);
}

NTSTATUS
NdasAtaDeviceInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	NTSTATUS status;
	PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	PLSP_IO_SESSION LspIoSession;
	PLSP_TRANSPORT_ADDRESS lspTransportAddress;

	ULONG i;

	PAGED_CODE();

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(LogicalUnitExtension);
	ndasAtaDeviceExtension->LogicalUnitExtension = LogicalUnitExtension;

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

	ndasAtaDeviceExtension->DeviceObject = NdasPortExGetWdmDeviceObject(LogicalUnitExtension);

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
		ndasAtaDeviceExtension->DeviceAddress.Type = LspOverTcpLpx;
		
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
	// Threshold
	//

	ndasAtaDeviceExtension->ResetConnectionFailThreshold = 5;
	ndasAtaDeviceExtension->ResetConnectionDelay.QuadPart = - 3 * SECOND_IN_100_NS;

	//
	// Local LPX Address List
	//

	NdasAtapUpdateLocalLpxAddresses(ndasAtaDeviceExtension);

	//
	// LSP IO Session
	//

	ASSERT(NULL != ndasAtaDeviceExtension->LspIoSession);
	LspIoSession = ndasAtaDeviceExtension->LspIoSession;

	status = LspIoInitializeSession(
		LspIoSession,
		&ndasAtaDeviceExtension->DeviceAddress,
		ndasAtaDeviceExtension->LocalAddressList,
		ndasAtaDeviceExtension->LocalAddressCount,
		&ndasAtaDeviceExtension->LspLoginInfo);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoSessionCreate failed with status=%x\n", status);
		goto error2;
	}

	//
	// Start the device
	//

	status = LspIoStartSession(
		ndasAtaDeviceExtension->LspIoSession,
		NULL,
		NULL,
		NULL);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoStartSession failed with status=%x\n", status);

		goto error3;
	}

	ndasAtaDeviceExtension->LspHandle = 
		LspIoGetLspHandle(ndasAtaDeviceExtension->LspIoSession);

	ASSERT(NULL != ndasAtaDeviceExtension->LspHandle);

	NdasAtapUpdateDeviceData(ndasAtaDeviceExtension);

	//
	// WMI Support
	//

	ndasAtaDeviceExtension->WmiLibInfo.GuidCount = countof(NdasAtaWmiGuidList);
	ndasAtaDeviceExtension->WmiLibInfo.GuidList = NdasAtaWmiGuidList;
	ndasAtaDeviceExtension->WmiLibInfo.QueryWmiRegInfo = NdasAtaWmiQueryRegInfo;
	ndasAtaDeviceExtension->WmiLibInfo.QueryWmiDataBlock = NdasAtaWmiQueryDataBlock;

	NdasPortWmiInitializeContext(
		LogicalUnitExtension,
		&ndasAtaDeviceExtension->WmiLibInfo);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"NdasAtaDeviceInitializeLogicalUnit completed.\n");

	return STATUS_SUCCESS;

error3:

	ASSERT(NULL != ndasAtaDeviceExtension->LspIoSession);
	LspIoUninitializeSession(ndasAtaDeviceExtension->LspIoSession);

error2:

	ASSERT(NULL != ndasAtaDeviceExtension->LspIoSession);
	LspIoFreeSession(ndasAtaDeviceExtension->LspIoSession);
	ndasAtaDeviceExtension->LspIoSession = NULL;

error1:

	return status;
}

VOID
NdasAtaDeviceCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension)
{
	NTSTATUS status;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	lsp_ide_register_param_t idereg;
	BOOLEAN supportFlushCache;
	
	PAGED_CODE();

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
		"NdasAtaDeviceCleanupLogicalUnit: DeviceObject=%p\n", 
		ndasAtaDeviceExtension->DeviceObject);

	if (!pTestFlag(ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED))
	{
		//
		// Flush synchronously if supported
		//

		supportFlushCache = FALSE;

		if (ndasAtaDeviceExtension->LspIdentifyData->
			command_set_support.flush_cache_ext)
		{
			supportFlushCache = TRUE;
		}
		else if (ndasAtaDeviceExtension->LspIdentifyData->
			command_set_support.flush_cache)
		{
			supportFlushCache = TRUE;
		}

		if (supportFlushCache)
		{
			NdasPortTrace(NDASATA_IO, TRACE_LEVEL_VERBOSE,
				"Flushing: NdasAtaDeviceExtension=%p\n", 
				ndasAtaDeviceExtension);

			status = LspIoFlushCache(
				ndasAtaDeviceExtension->LspIoSession,
				NULL,
				NULL);

			if (!NT_SUCCESS(status))
			{
				NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
					"LspIoAtaFlushCache failed during cleanup, Status=%08X\n", 
					status);
			}
		}
	}

	status = LspIoStopSession(
		ndasAtaDeviceExtension->LspIoSession, 
		NULL, 
		NULL);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
			"LspIoStopSession failed during cleanup, Status=%08X\n", status);
	}

	ASSERT(NULL != ndasAtaDeviceExtension->LspIoSession);
	LspIoUninitializeSession(ndasAtaDeviceExtension->LspIoSession);

	ASSERT(NULL != ndasAtaDeviceExtension->LspIoSession);
	LspIoFreeSession(ndasAtaDeviceExtension->LspIoSession);
	ndasAtaDeviceExtension->LspIoSession = NULL;
}

FORCEINLINE
VOID
NdasAtaDeviceInitializeInquiryData(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	static CONST UCHAR VendorId[8] = "NDAS";

	const lsp_ide_identify_device_data_t* identifyData;
	PINQUIRYDATA inquiryData;

	identifyData = NdasAtaDeviceExtension->LspIdentifyData;
	inquiryData = &NdasAtaDeviceExtension->InquiryData;

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
	inquiryData->CommandQueue = 1;
	inquiryData->LinkedCommands;
	inquiryData->Synchronous;
	inquiryData->Wide16Bit;
	inquiryData->Wide32Bit;
	inquiryData->RelativeAddressing;

	inquiryData->VendorId[8-1];
	inquiryData->ProductId[16-1];
	inquiryData->ProductRevisionLevel[4-1];

	identifyData->serial_number; 
	identifyData->model_number;
	identifyData->firmware_revision;

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

	realLength = 
		FIELD_OFFSET(STORAGE_DEVICE_DESCRIPTOR, RawDeviceProperties) +
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

SRBSTATUS
NdasAtaDeviceStartLspIo(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	SRBSTATUS srbStatus;
	NTSTATUS status;
	LARGE_INTEGER logicalBlockAddress;
	LONG64 startingBlockAddress;
	LONG64 endingBlockAddress;
	ULONG transferBlockCount;
	ULONG transferBytes;
	lsp_status_t lspStatus;
	lsp_ide_register_param_t idereg;
	PLSP_IO_REQUEST LspIoRequest;
	PCDB Cdb;
	BOOLEAN fua;

	++NdasAtaDeviceExtension->SrbIoCount;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"NdasAtaDeviceExtension=%p, Srb(%u)=%p\n", 
		NdasAtaDeviceExtension, NdasAtaDeviceExtension->SrbIoCount, Srb);

	ASSERT(NULL == NdasAtaDeviceExtension->CurrentSrb);
	ASSERT(!pTestFlag(NdasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING));

	if (pTestFlag(NdasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED))
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

		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
		return srbStatus;
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

		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		return srbStatus;
	}

	logicalBlockAddress.QuadPart = startingBlockAddress;
	endingBlockAddress = startingBlockAddress + transferBlockCount - 1;
	transferBytes = transferBlockCount * NdasAtaDeviceExtension->VirtualBytesPerBlock;

	if (endingBlockAddress > NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart)
	{
		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_READ/WRITE/VERIFY: out of bounds (%I64Xh,%I64Xh:%Xh blocks,%Xh bytes)\n", 
			startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
		return srbStatus;
	}

	NdasAtaDeviceExtension->CurrentSrb = Srb;
	LspIoRequest = &NdasAtaDeviceExtension->LspIoRequest;

	Cdb = (PCDB) Srb->Cdb;
	switch (Cdb->CDB6GENERIC.OperationCode)
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
				"SCSIOP_READ: %u,(%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n",
				NdasAtaDeviceExtension->SrbIoCount,
				startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

			LspIoRequest->RequestType = LSP_IO_READ;
			LspIoRequest->Context.ReadWriteVerify.LogicalBlockAddress = logicalBlockAddress;
			LspIoRequest->Context.ReadWriteVerify.Buffer = Srb->DataBuffer;
			LspIoRequest->Context.ReadWriteVerify.TransferBlocks = transferBlockCount;

			srbStatus = SRB_STATUS_PENDING;

			break;
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:
		case SCSIOP_WRITE6:

			fua = FALSE;

			if (Srb->CdbLength >= 10)
			{
				if (Cdb->CDB10.ForceUnitAccess)
				{
					fua = TRUE;
				}
			}

			if (fua)
			{
				NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
					"SCSIOP_WRITE(FUA): %u,(%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n",
					NdasAtaDeviceExtension->SrbIoCount,
					startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);
			}
			else
			{
				NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
					"SCSIOP_WRITE: %u,(%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n", 
					NdasAtaDeviceExtension->SrbIoCount,
					startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);
			}

			if (NdasAtaDeviceExtension->CurrentAccessMode & GENERIC_WRITE)
			{
				if (fua)
				{
					LspIoRequest->RequestType = LSP_IO_WRITE_FUA;
				}
				else
				{
					LspIoRequest->RequestType = LSP_IO_WRITE;
				}
				LspIoRequest->Context.ReadWriteVerify.LogicalBlockAddress = logicalBlockAddress;
				LspIoRequest->Context.ReadWriteVerify.Buffer = Srb->DataBuffer;
				LspIoRequest->Context.ReadWriteVerify.TransferBlocks = transferBlockCount;

				srbStatus = SRB_STATUS_PENDING;

			}
			else if (NdasAtaDeviceExtension->RequestedAccessMode & GENERIC_WRITE)
			{
				NdasAtaDeviceExtension->CurrentSrb = NULL;

				Srb->DataTransferLength = 0;
				srbStatus = Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			}
			else
			{
				//
				// Fake write
				//
				NdasAtaDeviceExtension->CurrentSrb = NULL;

				Srb->DataTransferLength = transferBytes;
				srbStatus = Srb->SrbStatus = SRB_STATUS_SUCCESS;
			}
			break;
			DEFAULT_UNREACHABLE;
		}

		break;
	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:
	case SCSIOP_VERIFY6:

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_VERIFY: %u,(%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n", 
			NdasAtaDeviceExtension->SrbIoCount,
			startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

		LspIoRequest->RequestType = LSP_IO_VERIFY;
		LspIoRequest->Context.ReadWriteVerify.LogicalBlockAddress = logicalBlockAddress;
		LspIoRequest->Context.ReadWriteVerify.Buffer = NULL;
		LspIoRequest->Context.ReadWriteVerify.TransferBlocks = transferBlockCount;

		srbStatus = SRB_STATUS_PENDING;

		break;

	case SCSIOP_SYNCHRONIZE_CACHE:
	case SCSIOP_SYNCHRONIZE_CACHE16:

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
			"SCSIOP_SYNCHRONIZE_CACHE: %u,(%I64Xh,%I64Xh:%Xh blocks, %Xh bytes)\n",
			NdasAtaDeviceExtension->SrbIoCount,
			startingBlockAddress, endingBlockAddress, transferBlockCount, transferBytes);

		LspIoRequest->RequestType = LSP_IO_FLUSH_CACHE;
		srbStatus = SRB_STATUS_PENDING;

		break;
	default:
		ASSERT(FALSE);
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		srbStatus = SRB_STATUS_INVALID_REQUEST;
	}

	if (SRB_STATUS_PENDING == srbStatus)
	{		
		LspIoRequest->CompletionRoutine = NdasAtapLspIoCompletion;
		LspIoRequest->CompletionContext = NdasAtaDeviceExtension;

		status = LspIoRequestIo(
			NdasAtaDeviceExtension->LspIoSession,
			LspIoRequest);
	}

	return srbStatus;
}

SRBSTATUS
NdasAtaDeviceStartIoPassthrough(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
	return Srb->SrbStatus;
}

SRBSTATUS
NdasAtaDeviceStartIoFlush(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;
	lsp_ide_register_param_t idereg;
	PLSP_IO_REQUEST LspIoRequest;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"Flush Command Queued, NdasAtaDeviceExtension=%p, Srb=%p\n", 
		NdasAtaDeviceExtension, Srb);

	if (!NdasAtaDeviceExtension->LspIdentifyData->command_set_support.flush_cache &&
		!NdasAtaDeviceExtension->LspIdentifyData->command_set_support.flush_cache_ext)
	{
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		return SRB_STATUS_INVALID_REQUEST;
	}

	NdasAtaDeviceExtension->CurrentSrb = Srb;

	LspIoRequest = &NdasAtaDeviceExtension->LspIoRequest;
	RtlZeroMemory(LspIoRequest, sizeof(LSP_IO_REQUEST));
	LspIoRequest->RequestType = LSP_IO_FLUSH_CACHE;
	LspIoRequest->CompletionRoutine = NdasAtapLspIoCompletion;
	LspIoRequest->CompletionContext = NdasAtaDeviceExtension;

	status = LspIoRequestIo(
		NdasAtaDeviceExtension->LspIoSession,
		LspIoRequest);

	//
	// ignore status here, all errors are processed at completion
	//

	return SRB_STATUS_PENDING;
}

SRBSTATUS
NdasAtaDeviceStartIoSmart(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	NTSTATUS status;
	PSRB_IO_CONTROL srbIoControl;
	SENDCMDINPARAMS sendCmdIn; /* a copy of SENDCMDINPARAMS */
	PSENDCMDOUTPARAMS sendCmdOut;
	ULONG dataLength;
	lsp_io_data_buffer_t iobuf;
	lsp_ide_register_param_t idereg;
	PLSP_IO_REQUEST LspIoRequest;

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

		return Srb->SrbStatus;
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

	RtlZeroMemory(&idereg, sizeof(lsp_ide_register_param_t));
	idereg.reg.named.features = sendCmdIn.irDriveRegs.bFeaturesReg;
	idereg.reg.named.sector_count = sendCmdIn.irDriveRegs.bSectorCountReg;
	idereg.reg.named.lba_low = sendCmdIn.irDriveRegs.bSectorNumberReg;
	idereg.reg.named.lba_mid = sendCmdIn.irDriveRegs.bCylLowReg;
	idereg.reg.named.lba_high = sendCmdIn.irDriveRegs.bCylHighReg;
	idereg.device.device = sendCmdIn.irDriveRegs.bDriveHeadReg;
	idereg.command.command = sendCmdIn.irDriveRegs.bCommandReg;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"Smart Command Queued, NdasAtaDeviceExtension=%p, Srb=%p\n", 
		NdasAtaDeviceExtension, Srb);

	//
	// Sanity Check and adjustment for DEV bit in Device Register
	// As we do not report DEV bit, we should manually adjust the bit
	//
	idereg.device.s.dev = NdasAtaDeviceExtension->LspLoginInfo.unit_no;

	NdasAtaDeviceExtension->CurrentSrb = Srb;

	//
	// There is no data to send in SMART command
	//

	RtlZeroMemory(&iobuf, sizeof(lsp_io_data_buffer_t));
	iobuf.recv_buffer = sendCmdOut->bBuffer;
	iobuf.recv_size = dataLength;
	
	LspIoRequest = &NdasAtaDeviceExtension->LspIoRequest;
	RtlZeroMemory(LspIoRequest, sizeof(LSP_IO_REQUEST));
	LspIoRequest->RequestType = LSP_IO_LSP_COMMAND;
	LspIoRequest->CompletionRoutine = NdasAtapLspIoCompletion;
	LspIoRequest->CompletionContext = NdasAtaDeviceExtension;

	lsp_build_ide_command(
		&NdasAtaDeviceExtension->LspIoRequest.LspRequestPacket,
		NdasAtaDeviceExtension->LspHandle,
		&idereg,
		&iobuf,
		0);

	status = LspIoRequestIo(
		NdasAtaDeviceExtension->LspIoSession,
		LspIoRequest);

	// ASSERT(STATUS_PENDING == status);

	//
	// ignore status here, all errors are processed at completion
	//

	return SRB_STATUS_PENDING;
}

SRBSTATUS
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

		return Srb->SrbStatus;
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

		return Srb->SrbStatus;
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
				NdasAtaDeviceExtension->LspIdentifyData,
				sizeof(lsp_ide_identify_device_data_t));

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

	return Srb->SrbStatus;
}

SRBSTATUS
NdasAtaDeviceModeSense(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	//
	// This is used to determine of the media is write-protected.
	// Since IDE does not support mode sense then we will modify just the portion we need
	// so the higher level driver can determine if media is protected.
	//
	// MODE_PAGE_CACHING;
	// MODE_SENSE_CURRENT_VALUES;
	PCDB cdb = (PCDB) &Srb->Cdb[0];
	ULONG requestLength;
	PMODE_PARAMETER_HEADER header;
	PMODE_PARAMETER_BLOCK blockDescriptor;
	PMODE_CACHING_PAGE cachePage; 
	UCHAR modeDsp;

	ASSERT(SCSIOP_MODE_SENSE == Srb->Cdb[0] || SCSIOP_MODE_SENSE10 == Srb->Cdb[0]);
	ASSERT(Srb->SrbFlags & SRB_FLAGS_DATA_IN);

	switch (cdb->MODE_SENSE.PageCode)
	{
	case MODE_PAGE_CACHING:
	case MODE_SENSE_RETURN_ALL:
	case 0:
		break;
	default:
		//
		// Not supported
		//
		Srb->DataTransferLength = 0;
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		return SRB_STATUS_INVALID_REQUEST;
	}

	requestLength = Srb->DataTransferLength;

	//
	// Ensure that we have room for the header at least
	//
	if (requestLength < sizeof(MODE_PARAMETER_HEADER))
	{
		Srb->DataTransferLength = 0;
		Srb->SrbStatus = SRB_STATUS_ERROR;
		return SRB_STATUS_ERROR;
	}

	header = (PMODE_PARAMETER_HEADER) Srb->DataBuffer;

	//
	// Device Specific Parameters
	// e.g. WRITE_PROTECT, FUA
	//

	modeDsp = 0;

	if (!(NdasAtaDeviceExtension->RequestedAccessMode & GENERIC_WRITE))
	{
		modeDsp |= MODE_DSP_WRITE_PROTECT;
	}

	//
	// TODO: BY_OPTIONS
	//
	if (NdasAtaDeviceExtension->LspIdentifyData->command_set_support.write_fua)
	{
		modeDsp |= MODE_DSP_FUA_SUPPORTED;
	}

	if (MODE_PAGE_CACHING == cdb->MODE_SENSE.PageCode ||
		MODE_SENSE_RETURN_ALL == cdb->MODE_SENSE.PageCode)
	{
		header->ModeDataLength = 0;
		header->MediumType = 0;
		header->DeviceSpecificParameter = modeDsp;
		header->BlockDescriptorLength = 0;

		requestLength -= sizeof(MODE_PARAMETER_HEADER);
		Srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER);

		if (requestLength < sizeof(MODE_CACHING_PAGE))
		{
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			return SRB_STATUS_SUCCESS;
		}

		//
		// Data length is (buffer - ModeDataLength byte)
		//
		header->ModeDataLength = sizeof(MODE_CACHING_PAGE) + 3;

		cachePage = (PMODE_CACHING_PAGE) NdasPortOffsetOf(
			header, sizeof(MODE_PARAMETER_HEADER));

		cachePage->PageCode = MODE_PAGE_CACHING;
		cachePage->Reserved = 0;
		cachePage->PageSavable = 0;
		cachePage->PageLength = sizeof(MODE_CACHING_PAGE) - 
			RTL_SIZEOF_THROUGH_FIELD(MODE_CACHING_PAGE, PageLength);
		cachePage->ReadDisableCache = 0;
		cachePage->MultiplicationFactor = 0;

		//
		// Cache control
		//
		//
		// TODO: should be based on mediaInfo.
		//
		cachePage->WriteCacheEnable = 1;
		cachePage->Reserved2 = 0;
		cachePage->WriteRetensionPriority = 0;
		cachePage->ReadRetensionPriority = 0;
		cachePage->DisablePrefetchTransfer[0] = 0;
		cachePage->DisablePrefetchTransfer[1] = 0;
		cachePage->MinimumPrefetch[0] = 0;
		cachePage->MinimumPrefetch[1] = 0;
		cachePage->MaximumPrefetch[0] = 0;
		cachePage->MaximumPrefetch[1] = 0;
		cachePage->MaximumPrefetchCeiling[0] = 0;
		cachePage->MaximumPrefetchCeiling[1] = 0;

		Srb->DataTransferLength += sizeof(MODE_CACHING_PAGE);
		Srb->SrbStatus = SRB_STATUS_SUCCESS;

		return SRB_STATUS_SUCCESS;
	}
	else if (0 == cdb->MODE_SENSE.PageCode)
	{
		//
		// Header and block descriptor are requested, but no mode page
		//

		header->ModeDataLength = 0;
		header->MediumType = 0;
		header->DeviceSpecificParameter = modeDsp;
		header->BlockDescriptorLength = 0;

		requestLength -= sizeof(MODE_PARAMETER_HEADER);
		Srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER);

		if (requestLength < sizeof(MODE_PARAMETER_BLOCK))
		{
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			return SRB_STATUS_SUCCESS;
		}

		// 
		// We have enough room for MODE_PARAMETER_BLOCK now
		//
		blockDescriptor = (PMODE_PARAMETER_BLOCK) NdasPortOffsetOf(
			header, sizeof(MODE_PARAMETER_HEADER));

		//
		// 'default' media type
		//
		blockDescriptor->DensityCode = 0;

		//
		// Indicate that this applies to ALL blocks.
		//
		blockDescriptor->NumberOfBlocks[0] = 0;
		blockDescriptor->NumberOfBlocks[1] = 0;
		blockDescriptor->NumberOfBlocks[2] = 0;
		blockDescriptor->Reserved = 0;

		//
		// TODO: should be based on mediaInfo.
		//
		blockDescriptor->BlockLength[0] = 0;
		blockDescriptor->BlockLength[1] = 1;
		blockDescriptor->BlockLength[2] = 0;

		Srb->DataTransferLength += sizeof(MODE_PARAMETER_BLOCK);
		Srb->SrbStatus = SRB_STATUS_SUCCESS;

		return SRB_STATUS_SUCCESS;
	}
	return SRB_STATUS_INVALID_REQUEST;
}

SRBSTATUS
NdasAtaDeviceModeSelect(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	Srb->DataTransferLength = 0;
	Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
	return Srb->SrbStatus;
}

#ifdef NDASPORT_IMP_SCSI_WMI 

SRBSTATUS
NdasAtapStartWmi(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_WMI_REQUEST_BLOCK Srb)
{
	BOOLEAN pending;
	NDASPORT_WMI_REQUEST_CONTEXT wmiRequest;

	switch (Srb->WMISubFunction)
	{
	case IRP_MN_REGINFO:
		;
	}

	wmiRequest.BufferSize = Srb->DataTransferLength;
	wmiRequest.Buffer = Srb->DataBuffer;
	wmiRequest.MinorFunction = Srb->WMISubFunction;
	wmiRequest.ReturnSize;
	wmiRequest.ReturnStatus;
	wmiRequest.UserContext = NdasAtaDeviceExtension;

	pending = NdasPortWmiDispatchFunction(
		&NdasAtaDeviceExtension->WmiLibInfo,
		Srb->WMISubFunction,
		NdasAtaDeviceExtension,
		&wmiRequest,
		Srb->DataPath,
		Srb->DataTransferLength,
		Srb->DataBuffer);

	if (pending)
	{
		return SRB_STATUS_PENDING;
	}

	NdasPortWmiPostProcess(&wmiRequest, SRB_STATUS_SUCCESS, 0);
	Srb->DataTransferLength = NdasPortWmiGetReturnSize(&wmiRequest);
	Srb->SrbStatus = NdasPortWmiGetReturnStatus(&wmiRequest);

	return Srb->SrbStatus;
}
#endif /* NDASPORT_IMP_SCSI_WMI */

SRBSTATUS
NdasAtaDeviceStartIoExecuteScsi(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	UCHAR srbStatus;

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
		
		srbStatus = NdasAtaDeviceStartLspIo(NdasAtaDeviceExtension, Srb);
		break;

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
				&NdasAtaDeviceExtension->InquiryData,
				readDataLength);

			Srb->DataTransferLength = readDataLength;
			srbStatus = Srb->SrbStatus = SRB_STATUS_SUCCESS;
		}
		break;

	case SCSIOP_MODE_SENSE:

		srbStatus = NdasAtaDeviceModeSense(NdasAtaDeviceExtension, Srb);
		break;

	case SCSIOP_START_STOP_UNIT:
	case SCSIOP_TEST_UNIT_READY:

		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_SUCCESS;

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

			if (NdasAtaDeviceExtension->VirtualLogicalBlockAddress.HighPart > 0)
			{
				logicalBlockAddress = 0xFFFFFFFF;
			}
			else
			{
				logicalBlockAddress = NdasAtaDeviceExtension->VirtualLogicalBlockAddress.LowPart;
			}

			readCapacityData->LogicalBlockAddress = 
				RtlUlongByteSwap(logicalBlockAddress);

			readCapacityData->BytesPerBlock =
				RtlUlongByteSwap(NdasAtaDeviceExtension->VirtualBytesPerBlock);

			Srb->DataTransferLength = sizeof(READ_CAPACITY_DATA);
			srbStatus = Srb->SrbStatus = SRB_STATUS_SUCCESS;
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
					NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart);

			readCapacityDataEx->BytesPerBlock = 
				RtlUlongByteSwap(NdasAtaDeviceExtension->VirtualBytesPerBlock);
			
		}
		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_SUCCESS;

		break;
	case SCSIOP_MODE_SELECT:
		// TODO: supports SCSIOP_MODE_SELECT
	default:
		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
	}

	return srbStatus;
}

BOOLEAN
NdasAtaDeviceStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION DeviceExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	UCHAR srbStatus;
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;

	ndasAtaDeviceExtension = NdasAtaDeviceGetExtension(DeviceExtension);

	if (pTestFlag(ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED))
	{
		//
		// SRB may be in-queue even after disconnected.
		//
		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;

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
		srbStatus = NdasAtaDeviceStartIoPassthrough(ndasAtaDeviceExtension, Srb);
		break;
	case SRB_FUNCTION_FLUSH:
	case SRB_FUNCTION_SHUTDOWN:
		srbStatus = NdasAtaDeviceStartIoFlush(ndasAtaDeviceExtension, Srb);
		break;
	case SRB_FUNCTION_IO_CONTROL:
		srbStatus = NdasAtaDeviceStartIoSrbIoControl(ndasAtaDeviceExtension, Srb);
		break;
	case SRB_FUNCTION_EXECUTE_SCSI:
		/* processed at the next section */
		srbStatus = NdasAtaDeviceStartIoExecuteScsi(ndasAtaDeviceExtension, Srb);
		break;
#if 0
	case SRB_FUNCTION_WMI:
		srbStatus = NdasAtadeviceStartWmi(
			ndasAtaDeviceExtension, 
			(PSCSI_WMI_REQUEST_BLOCK) Srb);
		break;
#endif
	default:
		//
		// Unsupported command
		//
		Srb->DataTransferLength = 0;
		srbStatus = Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
	}

	if (SRB_STATUS_PENDING != srbStatus)
	{
		if (SRB_STATUS_UNEXPECTED_BUS_FREE == srbStatus)
		{
			NdasPortCompleteRequest(
				DeviceExtension,
				ndasAtaDeviceExtension->LogicalUnitAddress.PathId,
				ndasAtaDeviceExtension->LogicalUnitAddress.TargetId,
				ndasAtaDeviceExtension->LogicalUnitAddress.Lun,
				SRB_STATUS_UNEXPECTED_BUS_FREE);
		}
		else
		{
			NdasPortNotification(
				RequestComplete,
				DeviceExtension,
				Srb);

			NdasPortNotification(
				NextLuRequest,
				DeviceExtension);
		}
	}

	return TRUE;
}

NTSTATUS
LSPIOCALL
NdasAtapLspIoCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context)
{
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	PIRP Irp;
	PIO_STACK_LOCATION IrpStack;
	PSCSI_REQUEST_BLOCK srb;
	KIRQL oldIrql;
	PIO_STATUS_BLOCK ioStatus;

	ndasAtaDeviceExtension = (PNDAS_ATA_DEVICE_EXTENSION) Context;
	srb = ndasAtaDeviceExtension->CurrentSrb;

	ioStatus = &LspIoRequest->IoStatus;

	NdasPortTrace(NDASATA_IO, TRACE_LEVEL_INFORMATION,
		"LspIoCompletion: NdasAtaDeviceExtension=%p, Srb=%p, Status=%x\n", 
			ndasAtaDeviceExtension, srb, ioStatus->Status);

	ASSERT(
		STATUS_UNEXPECTED_NETWORK_ERROR == ioStatus->Status ||
		STATUS_LSP_ERROR == ioStatus->Status ||
		STATUS_SUCCESS == ioStatus->Status);

	if (ioStatus->Status == STATUS_UNEXPECTED_NETWORK_ERROR)
	{
		//
		// Network is disrupted. Reset the session
		//

		pSetFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING);

		NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING,
			"Network Error, Queuing reconnection...\n");

		ndasAtaDeviceExtension->ResetConnectionErrorCount = 0;

		NdasAtapFireConnectionEvent(
			ndasAtaDeviceExtension, 
			NDAS_ATA_EVENT_OFFLINE);

		LspIoRestartSession(
			ndasAtaDeviceExtension->LspIoSession,
			NULL,
			NULL,
			NULL,
			NdasAtapRestartSessionCompletion,
			ndasAtaDeviceExtension);

		return STATUS_SUCCESS;
	}

	//
	// SRB_IO_CONTROL requires more processing
	//
	if (SRB_FUNCTION_IO_CONTROL == srb->Function)
	{
		NTSTATUS status;
		PLSP_IDE_REGISTER idereg;
		PSENDCMDOUTPARAMS sendCmdOut;

		sendCmdOut = (PSENDCMDOUTPARAMS) NdasPortOffsetOf(
			srb->DataBuffer, sizeof(SRB_IO_CONTROL));

		idereg = &ndasAtaDeviceExtension->LspIoRequest.
			LspRequestPacket.u.ide_command.response.reg;

		sendCmdOut->cBufferSize = ndasAtaDeviceExtension->SmartDataLength;
		sendCmdOut->DriverStatus.bDriverError = 
			idereg->command.status.err ? SMART_IDE_ERROR : 0;
		sendCmdOut->DriverStatus.bIDEError = 
			idereg->reg.ret.err.err_na;

		//
		// RETURN_SMART_STATUS
		//
		if (RETURN_SMART_STATUS == ndasAtaDeviceExtension->SmartCommand)
		{
			PIDEREGS returnIdeRegs;
			returnIdeRegs = (PIDEREGS) sendCmdOut->bBuffer;
			returnIdeRegs->bFeaturesReg = RETURN_SMART_STATUS;
			returnIdeRegs->bSectorCountReg = idereg->reg.named.sector_count;
			returnIdeRegs->bSectorNumberReg = idereg->reg.named.lba_low;
			returnIdeRegs->bCylLowReg =  idereg->reg.named.lba_mid;
			returnIdeRegs->bCylHighReg = idereg->reg.named.lba_high;;
			returnIdeRegs->bDriveHeadReg = idereg->device.device;
			returnIdeRegs->bCommandReg = SMART_CMD;
			sendCmdOut->cBufferSize = sizeof(IDEREGS); // 7 + reserved = 8
		}
	}

	if (NT_SUCCESS(ioStatus->Status))
	{
		srb->SrbStatus = SRB_STATUS_SUCCESS;
		srb->ScsiStatus = SCSISTAT_GOOD;
	}
	else
	{
		ASSERT(STATUS_LSP_ERROR == ioStatus->Status);

		// NdasPortTrace(NDASATA_IO, TRACE_LEVEL_WARNING, "LSP Error...\n");

		srb->SrbStatus = SRB_STATUS_ERROR;
		srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
	}

	ndasAtaDeviceExtension->CurrentSrb = NULL;

	NdasPortNotification(
		RequestComplete,
		ndasAtaDeviceExtension->LogicalUnitExtension,
		srb);

	NdasPortNotification(
		NextLuRequest,
		ndasAtaDeviceExtension->LogicalUnitExtension);

	return STATUS_SUCCESS;
}

VOID
LSPIOCALL
NdasAtapRestartSessionCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context)
{
	PNDAS_ATA_DEVICE_EXTENSION ndasAtaDeviceExtension;
	PSCSI_REQUEST_BLOCK srb;

	ndasAtaDeviceExtension = (PNDAS_ATA_DEVICE_EXTENSION) Context;

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"NdasAtapRestartSessionCompletion, status=0x%x, errorCount=%d.\n",
		LspIoRequest->IoStatus.Status,
		ndasAtaDeviceExtension->ResetConnectionErrorCount);

	if (!NT_SUCCESS(LspIoRequest->IoStatus.Status))
	{
		++ndasAtaDeviceExtension->ResetConnectionErrorCount;

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoRestartSession failed, status=0x%x, errorCount=%d.\n",
			LspIoRequest->IoStatus.Status,
			ndasAtaDeviceExtension->ResetConnectionErrorCount);

		if (ndasAtaDeviceExtension->ResetConnectionErrorCount >
			ndasAtaDeviceExtension->ResetConnectionFailThreshold)
		{
			pClearFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING);
			pSetFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_DISCONNECTED);

			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"*** Link failed, NdasAta=%p -> SRB_STATUS_UNEXPECTED_BUS_FREE\n", 
				ndasAtaDeviceExtension);

			NdasPortNotification(
				BusChangeDetected,
				ndasAtaDeviceExtension->LogicalUnitExtension, 
				ndasAtaDeviceExtension->LogicalUnitAddress.PathId);

			NdasPortCompleteRequest(
				ndasAtaDeviceExtension->LogicalUnitExtension,
				ndasAtaDeviceExtension->LogicalUnitAddress.PathId,
				ndasAtaDeviceExtension->LogicalUnitAddress.TargetId,
				ndasAtaDeviceExtension->LogicalUnitAddress.Lun,
				SRB_STATUS_UNEXPECTED_BUS_FREE);
		}
		else
		{
			NdasAtapFireConnectionEvent(
				ndasAtaDeviceExtension, 
				NDAS_ATA_EVENT_CONNECTING);

			LspIoRestartSession(
				ndasAtaDeviceExtension->LspIoSession,
				&ndasAtaDeviceExtension->ResetConnectionDelay,
				NULL,
				NULL,
				NdasAtapRestartSessionCompletion,
				ndasAtaDeviceExtension);
		}
	}
	else
	{
		pClearFlag(&ndasAtaDeviceExtension->LuFlags, NDASATA_FLAG_RECONNECTING);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"*** Link established, NdasAta=%p\n", 
			ndasAtaDeviceExtension);

		ndasAtaDeviceExtension->LspHandle = LspIoGetLspHandle(
			ndasAtaDeviceExtension->LspIoSession);

		NdasAtapUpdateDeviceData(ndasAtaDeviceExtension);

		srb = ndasAtaDeviceExtension->CurrentSrb;
		ndasAtaDeviceExtension->CurrentSrb = NULL;

		NdasAtapFireConnectionEvent(
			ndasAtaDeviceExtension, 
			NDAS_ATA_EVENT_ONLINE);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"Restarting NdasAta=%p, Srb=%p\n", 
			ndasAtaDeviceExtension, srb);

		NdasAtaDeviceStartIo(
			ndasAtaDeviceExtension->LogicalUnitExtension,
			srb);
	}
}

NTSTATUS
NdasAtapUpdateDeviceData(
	PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	NTSTATUS status;
	const lsp_ide_identify_device_data_t* IdentifyData;
	const lsp_ata_handshake_data_t* LspHandshakeData;

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"NdasAtapUpdateDeviceData: NdasAta=%p\n", NdasAtaDeviceExtension);

	ASSERT(NULL != NdasAtaDeviceExtension->LspHandle);

	IdentifyData = lsp_get_ide_identify_device_data(
		NdasAtaDeviceExtension->LspHandle);

	ASSERT(NULL != IdentifyData);

	LspHandshakeData = lsp_get_ata_handshake_data(
		NdasAtaDeviceExtension->LspHandle);

	ASSERT(NULL != LspHandshakeData);

	NdasAtaDeviceExtension->LspIdentifyData = IdentifyData;
	NdasAtaDeviceExtension->LspHandshakeData = LspHandshakeData;

	//
	// Set values for virtual LogicalBlockAddress and virtual BytesPerBlock
	//

	if (0 == NdasAtaDeviceExtension->VirtualBytesPerBlock)
	{
		NdasAtaDeviceExtension->VirtualBytesPerBlock = LspHandshakeData->logical_block_size;
	}

	if (0 == NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart)
	{
		NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart = 
			LspHandshakeData->lba_capacity.quad - 1;
	}
	else if (NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart < 0)
	{
		NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart =
			LspHandshakeData->lba_capacity.quad - 1 +
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
		&IdentifyData->model_number,
		sizeof(IdentifyData->model_number));

	RtlCopyMemory(
		NdasAtaDeviceExtension->DeviceFirmwareRevision,
		&IdentifyData->firmware_revision,
		sizeof(IdentifyData->firmware_revision));

	RtlCopyMemory(
		NdasAtaDeviceExtension->DeviceSerialNumber,
		&IdentifyData->serial_number,
		sizeof(IdentifyData->serial_number));

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
			IdentifyData->minor_revision,
			AtaMinorVersionNumberString(IdentifyData->minor_revision));

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tBytesPerBlock=%Xh\n", LspHandshakeData->logical_block_size);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tLBA Capacity=%I64Xh\n", LspHandshakeData->lba_capacity.quad);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tVirtualBytesPerBlock=%Xh\n", NdasAtaDeviceExtension->VirtualBytesPerBlock);

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
			"\tVirtualLogicalBlockAddress=%I64Xh\n", NdasAtaDeviceExtension->VirtualLogicalBlockAddress.QuadPart);

	}

	NdasAtaDeviceInitializeInquiryData(NdasAtaDeviceExtension);

	return STATUS_SUCCESS;
}

FORCEINLINE
VOID
NdasAtapUpdateLocalLpxAddresses(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension)
{
	ULONG totalCount;
	ULONG i, count;

	NdasAtaDeviceExtension->LPX.LocalAddressCount = 
		countof(NdasAtaDeviceExtension->LPX.LocalAddressList);

	NdasPortGetLpxLocalAddressList(
		NdasAtaDeviceExtension->LPX.LocalAddressList,
		&NdasAtaDeviceExtension->LPX.LocalAddressCount,
		&totalCount,
		&NdasAtaDeviceExtension->LPX.LocalAddressUpdateCounter);

	NdasAtaDeviceExtension->LPX.LocalAddressCount = totalCount;

	for (i = 0; i < NdasAtaDeviceExtension->LPX.LocalAddressCount; ++i)
	{
		NdasAtaDeviceExtension->LocalAddressList[i].Type = LspOverTcpLpx;
		NdasAtaDeviceExtension->LocalAddressList[i].LpxAddress =
			NdasAtaDeviceExtension->LPX.LocalAddressList[i];
	}

	NdasAtaDeviceExtension->LocalAddressCount = 
		NdasAtaDeviceExtension->LPX.LocalAddressCount;

	//
	// No address is in use now.
	//
}

#if 0
FORCEINLINE
ULONG
AtaGetBytesPerBlock(
	__in const lsp_ide_identify_device_data_t*  IdentifyData)
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
		&IdentifyData->physical_logical_sector_size;

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
				(PUCHAR) &IdentifyData->words_per_logical_sector,
				&bytesPerBlock);
		}
	}

	return bytesPerBlock;
}
#endif

NTSTATUS
NdasAtaWmiQueryRegInfo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_opt PUNICODE_STRING *RegistryPath,
	__out PUNICODE_STRING MofResourceName)
{
	PNDAS_ATA_DEVICE_EXTENSION NdasAtaExtension;

	NdasAtaExtension = NdasAtaDeviceGetExtension(LogicalUnitExtension);

	*RegistryPath = NULL;
	RtlInitUnicodeString(MofResourceName, NDASATA_BMF_RESOURCENAME);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasAtaWmiQueryDataBlock(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG InstanceCount,
	__inout PULONG InstanceLengthArray,
	__in ULONG BufferAvail,
	__out PUCHAR Buffer)
{
	NTSTATUS status;
	ULONG bufferUsed;
	PNDAS_ATA_DEVICE_EXTENSION NdasAtaExtension;

	NdasPortTrace(NDASATA_PROP, TRACE_LEVEL_WARNING, 
		__FUNCTION__ ": GuidIndex=%u, InstanceIndex=%u, InstanceCount=%u, "
		"BufferAvail=%u, Buffer=%p\n",
		GuidIndex, InstanceIndex, InstanceCount, BufferAvail, Buffer);

	NdasAtaExtension = NdasAtaDeviceGetExtension(LogicalUnitExtension);

	//
	// Only ever registers 1 instance per guid
	//

	ASSERT((InstanceIndex == 0) && (InstanceCount == 1));

	bufferUsed = 0;

	switch (GuidIndex) 
	{
	case NDASATA_WMI_LINK_EVENT_INDEX:
	case NDASATA_WMI_FLOWCONTROL_EVENT_INDEX:
		*InstanceLengthArray = 0;
		status = STATUS_SUCCESS;
		break;
	default:
		status = STATUS_WMI_GUID_NOT_FOUND;
		break;
	}

	return NdasPortWmiCompleteRequest(
		LogicalUnitExtension,
		Irp,
		status,
		bufferUsed,
		IO_NO_INCREMENT);
}

NTSTATUS
NdasAtapFireConnectionEvent(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension,
	__in NDAS_ATA_EVENT_TYPE EventType)
{
	NTSTATUS status;
	NDAS_ATA_LINK_EVENT linkEventData;
	GUID wmiEventGuid = NdasAtaWmi_Connection_Link_Event_GUID;
	PNdasAtaWmi_Connection_Link_Event wmiEventData;

	linkEventData.EventType = EventType;
	linkEventData.LogicalUnitAddress = 
		NdasAtaDeviceExtension->LogicalUnitAddress.Address;

	status = NdasPortFirePnpEvent(
		NdasAtaDeviceExtension->LogicalUnitExtension,
		&GUID_NDAS_ATA_LINK_EVENT,
		NULL, // L"NdasAtaLinkEvent",
		&linkEventData,
		sizeof(NDAS_ATA_LINK_EVENT));

	wmiEventData = (PNdasAtaWmi_Connection_Link_Event) ExAllocatePoolWithTag(
		NonPagedPool,
		NdasAtaWmi_Connection_Link_Event_SIZE,
		NDASATA_TAG_WMI);

	if (NULL == wmiEventData)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	wmiEventData->EventType = EventType;

	status = NdasPortWmiFireEvent(
		NdasAtaDeviceExtension->LogicalUnitExtension,
		&wmiEventGuid,
		0,
		NdasAtaWmi_Connection_Link_Event_SIZE,
		wmiEventData);

	if (!NT_SUCCESS(status))
	{
		ExFreePoolWithTag(wmiEventData, NDASATA_TAG_WMI);
	}

	return status;
}
