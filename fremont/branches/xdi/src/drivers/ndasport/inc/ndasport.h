#ifndef _NDASPORT_H_
#define _NDASPORT_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <ntddk.h>
#include <scsi.h>
#include <ntddstor.h>
#include <ntddscsi.h>
#if _WIN32_WINNT <= 0x500
#define NTSTRSAFE_LIB
#endif
#include <ntstrsafe.h>

#include <ndas/ndasportioctl.h>

#define NDASPORT_ENUMERATOR_GUID_PREFIX L"{56552ED5-F57A-4A9D-B13E-251A838E5D7B}\\"
#define NDASPORT_ENUMERATOR_NAMED_PREFIX L"NDAS\\"

#ifndef countof
#define countof(A) (sizeof(A)/sizeof(A[0]))
#endif

//
//  Data structure used in PlugIn and UnPlug ioctls
//

typedef enum _NDAS_LOGICALUNIT_CONTROL_TYPE {
	NdasStopLogicalUnit,
	NdasRestartLogicalUnit,
} NDAS_LOGICALUNIT_CONTROL_TYPE, *PNDAS_LOGICALUNIT_CONTROL_TYPE;

#if _WIN32_WINNT <= 0x500
typedef struct _STORAGE_DEVICE_ID_DESCRIPTOR 
STORAGE_DEVICE_ID_DESCRIPTOR, *PSTORAGE_DEVICE_ID_DESCRIPTOR;
#endif

typedef struct _NDAS_LOGICALUNIT_EXTENSION {
	ULONG_PTR DontUseThisStructure;
} NDAS_LOGICALUNIT_EXTENSION, *PNDAS_LOGICALUNIT_EXTENSION;

typedef struct _NDAS_LOGICALUNIT_INTERFACE
NDAS_LOGICALUNIT_INTERFACE, *PNDAS_LOGICALUNIT_INTERFACE;

PVOID
NdasPortGetLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in UCHAR Reserved,
	__in UCHAR Reserved2,
	__in UCHAR Reserved3);

VOID
NdasPortCompleteRequest(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in UCHAR PathId,
	__in UCHAR TargetId,
	__in UCHAR Lun,
	__in UCHAR SrbStatus);

VOID
NdasPortNotification(
	__in SCSI_NOTIFICATION_TYPE NotificationType, 
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	...);

//
// Extended support routines (not in ScsiPort)
//
PIO_WORKITEM
NdasPortExAllocateWorkItem(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

PDEVICE_OBJECT
NdasPortExGetWdmDeviceObject(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

NTSTATUS
NdasPortFirePnpEvent(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in CONST GUID* EventGuid,
	__in_opt CONST WCHAR* EventName,
	__in_bcount_opt(CustomDataSize) PVOID CustomData,
	__in ULONG CustomDataSize);


//BOOLEAN
//NdasPortExIsWindowsXPOrLater();

typedef
NTSTATUS
(*PNDAS_LU_QUERY_NDAS_LOGICALUNIT_INTERFACE)(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

#define NDASPORT_EXTERNAL_TYPE_GET_INTERFACE_VERSION 0x00000002

typedef struct _NDASPORT_EXTERNAL_TYPE_GET_INTERFACE {
	ULONG Size;    /* sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE) */
	ULONG Version; /* NDASPORT_EXTERNAL_TYPE_GET_INTERFACE_VERSION */
	GUID ExternalTypeGuid;
	PNDAS_LU_QUERY_NDAS_LOGICALUNIT_INTERFACE GetInterfaceFunction;
} NDASPORT_EXTERNAL_TYPE_GET_INTERFACE, *PNDASPORT_EXTERNAL_TYPE_GET_INTERFACE;

#define NDASPORTEXT_IOCTL_GET_LOGICALUNIT_INTERFACE \
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef
NTSTATUS
(*PNDAS_LU_INITIALIZE)(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

typedef
VOID
(*PNDAS_LU_CLEANUP)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

typedef
NTSTATUS
(*PNDAS_LU_LOGICALUNIT_CONTROL)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters);

typedef
NTSTATUS
(*PNDAS_LU_QUERY_PNP_ID)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING DeviceId);

typedef
NTSTATUS
(*PNDAS_LU_QUERY_PNP_DEVICE_TEXT)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__out PWCHAR *DeviceText);

typedef
NTSTATUS
(*PNDAS_LU_QUERY_PNP_DEVICE_CAPABILITIES)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PDEVICE_CAPABILITIES Capabilities);

typedef
NTSTATUS
(*PNDAS_LU_QUERY_STORAGE_DEVICE_PROPERTY)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

typedef
NTSTATUS
(*PNDAS_LU_QUERY_STORAGE_DEVICE_ID_PROPERTY)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

typedef
BOOLEAN
(*PNDAS_LU_BUILD_IO)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

typedef
BOOLEAN
(*PNDAS_LU_START_IO)(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

#define NDAS_LOGICALUNIT_INTERFACE_VERSION 0x00000401

typedef struct _NDAS_LOGICALUNIT_INTERFACE {
	ULONG Size;
	ULONG Version;
	// Passive level
	PNDAS_LU_INITIALIZE InitializeLogicalUnit;
	// Passive level
	PNDAS_LU_CLEANUP CleanupLogicalUnit;
	// Arbitrary level
	PNDAS_LU_LOGICALUNIT_CONTROL LogicalUnitControl;
	// Arbitrary level
	PNDAS_LU_BUILD_IO BuildIo;
	// Dispatch level
	PNDAS_LU_START_IO StartIo;
	// Passive level
	PNDAS_LU_QUERY_PNP_ID QueryPnpID;
	// Passive level
	PNDAS_LU_QUERY_PNP_DEVICE_TEXT QueryPnpDeviceText;
	// Passive level
	PNDAS_LU_QUERY_PNP_DEVICE_CAPABILITIES QueryPnpDeviceCapabilities;
	// Passive level
	PNDAS_LU_QUERY_STORAGE_DEVICE_PROPERTY QueryStorageDeviceProperty;
	// Passive level
	PNDAS_LU_QUERY_STORAGE_DEVICE_ID_PROPERTY QueryStorageDeviceIdProperty;
} NDAS_LOGICALUNIT_INTERFACE, *PNDAS_LOGICALUNIT_INTERFACE;

//////////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
//////////////////////////////////////////////////////////////////////////


#include <socketlpx.h>


//
// device type table to build id's from
//
typedef struct _NDASPORT_SCSI_DEVICE_TYPE {

	PCWSTR DeviceTypeString;
	PCWSTR GenericTypeString;
	PCWSTR DeviceMapString;
	BOOLEAN IsStorage;

} NDASPORT_SCSI_DEVICE_TYPE, *PNDASPORT_SCSI_DEVICE_TYPE;

FORCEINLINE
CONST NDASPORT_SCSI_DEVICE_TYPE*
NdasPortExGetScsiDeviceTypeInfo(
	__in UCHAR DeviceType);

NTSTATUS
NdasPortGetLpxLocalAddressList(
	__inout PTDI_ADDRESS_LPX TdiLpxAddressBuffer,
	__inout PULONG TdiLpxAddressCount,
	__out PULONG TotalAddressCount,
	__out PULONG UpdateCounter);

FORCEINLINE
PVOID
NdasPortOffsetOf(PVOID Buffer, ULONG Offset);

FORCEINLINE
VOID
NdasPortAddPnpDeviceIdLength(
	PULONG BufferByteLength, 
	CONST WCHAR* DeviceId);

FORCEINLINE
VOID
NdasPortAddPnpDeviceId(
	PWCHAR* NextBuffer,
	PULONG NextBufferByteLength,
	CONST WCHAR* DeviceId);

FORCEINLINE
VOID
NdasPortFixDeviceId(
	PWCHAR DeviceId,
	ULONG DeviceIdLength);

FORCEINLINE
VOID
NdasPortSetStoragePropertyData(
	__in CONST VOID* Data,
	__in ULONG DataLength,
	__inout PVOID Buffer,
	__in ULONG BufferLength,
	__inout PULONG CurrentOffset,
	__out_opt PULONG DataOffsetField);

FORCEINLINE
VOID
NdasPortSetStoragePropertyString(
	__in CONST VOID* Data,
	__in ULONG DataLength,
	__inout PVOID Buffer,
	__in ULONG BufferLength,
	__inout PULONG CurrentOffset,
	__out_opt PULONG DataOffsetField);

FORCEINLINE
VOID
NdasPortReverseBytes(
	PFOUR_BYTE Destination,
	CONST FOUR_BYTE* Source);

FORCEINLINE
VOID
NdasPortWordByteSwap(
	PVOID Buffer,
	ULONG BufferLength);

#ifndef ExFreePoolWithTag
#define ExFreePoolWithTag(POINTER,TAG) ExFreePool(POINTER)
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

FORCEINLINE
PVOID
NdasPortOffsetOf(PVOID Buffer, ULONG Offset)
{
	PUCHAR ByteBuffer = (PUCHAR) Buffer;
	return ByteBuffer + Offset;
}


#ifndef MAX_INSTANCE_ID_LEN
#define MAX_INSTANCE_ID_LEN 200
#endif

FORCEINLINE
VOID
NdasPortAddPnpDeviceIdLength(
	PULONG BufferByteLength, 
	CONST WCHAR* DeviceId)
{
	NTSTATUS status;
	size_t length;
	
	status = RtlStringCbLengthW(
		DeviceId, 
		MAX_INSTANCE_ID_LEN * sizeof(WCHAR),
		&length);

	ASSERT(NT_SUCCESS(status));

	if (!NT_SUCCESS(status))
	{
		length = MAX_INSTANCE_ID_LEN * sizeof(WCHAR);
	}

	length += sizeof(WCHAR); // additional null

	*BufferByteLength += (ULONG) length;
}

FORCEINLINE
VOID
NdasPortAddPnpDeviceId(
	PWCHAR* NextBuffer,
	PULONG NextBufferByteLength,
	CONST WCHAR* DeviceId)
{
	NTSTATUS status;
	size_t remainingLength;

	status = RtlStringCbCopyNExW(
		*NextBuffer,
		*NextBufferByteLength,
		DeviceId,
		MAX_INSTANCE_ID_LEN * sizeof(WCHAR),
		NextBuffer,
		&remainingLength,
		STRSAFE_NULL_ON_FAILURE);

	ASSERT(NT_SUCCESS(status));
	
	*NextBufferByteLength = (ULONG) remainingLength;

	++(*NextBuffer);
	*NextBufferByteLength -= sizeof(WCHAR);
}

FORCEINLINE
VOID
NdasPortFixDeviceId(
	PWCHAR DeviceId,
	ULONG DeviceIdByteLength)
{
	ULONG i;
	for (i = 0; i < DeviceIdByteLength / sizeof(WCHAR); ++i)
	{
		if (DeviceId[i] < 0x20 || 
			DeviceId[i] > 0x7F ||
			DeviceId[i] == 0x2C)
		{
			DeviceId[i] = L'_';
		}
	}
}

FORCEINLINE
VOID
NdasPortSetStoragePropertyData(
	__in CONST VOID* Data,
	__in ULONG DataLength,
	__inout PVOID Buffer,
	__in ULONG BufferLength,
	__inout PULONG CurrentOffset,
	__out_opt PULONG DataOffsetField)
{
	ULONG remainingBufferLength;
	ULONG usedBufferLength;
	
	ASSERT(*CurrentOffset < BufferLength);

	remainingBufferLength = BufferLength - *CurrentOffset;
	usedBufferLength = min(DataLength, remainingBufferLength);
	
	RtlCopyMemory(
		NdasPortOffsetOf(Buffer, *CurrentOffset),
		Data,
		usedBufferLength);

	if (NULL != DataOffsetField)
	{
		*DataOffsetField = *CurrentOffset;
	}

	*CurrentOffset += usedBufferLength;

	ASSERT(*CurrentOffset <= BufferLength);
}

FORCEINLINE
VOID
NdasPortSetStoragePropertyString(
	__in CONST VOID* Data,
	__in ULONG DataLength,
	__inout PVOID Buffer,
	__in ULONG BufferLength,
	__inout PULONG CurrentOffset,
	__out_opt PULONG DataOffsetField)
{
	NdasPortSetStoragePropertyData(
		Data, DataLength, 
		Buffer, BufferLength, 
		CurrentOffset, DataOffsetField);

	//
	// Terminate with NULL in case of insufficient data buffer
	//
	((PUCHAR)Buffer)[*CurrentOffset - 1] = 0;
}

FORCEINLINE
VOID
NdasPortReverseBytes(
	PFOUR_BYTE Destination,
	CONST FOUR_BYTE* Source)
{
	PFOUR_BYTE d = (PFOUR_BYTE)(Destination);
	PFOUR_BYTE s = (PFOUR_BYTE)(Source);
	d->Byte3 = s->Byte0;
	d->Byte2 = s->Byte1;
	d->Byte1 = s->Byte2;
	d->Byte0 = s->Byte3;
}

FORCEINLINE
VOID
NdasPortWordByteSwap(
	PVOID Buffer,
	ULONG BufferLength)
{
	PUSHORT wordBuffer = (PUSHORT) Buffer;
	ULONG i, wordLength = BufferLength >> 1;

	ASSERT(BufferLength % 2 == 0);

	for (i = 0; i < wordLength; ++i)
	{
		wordBuffer[i] = RtlUshortByteSwap(wordBuffer[i]);
	}
}

#ifndef _CDBEXT_DEFINED
#define _CDBEXT_DEFINED
#pragma pack(push, cdb, 1)
typedef union _CDBEXT {
	//
	// Standard 16-byte CDB
	//

	struct _EXT_CDB16 {
		UCHAR OperationCode;
		UCHAR Reserved1        : 3;
		UCHAR ForceUnitAccess  : 1;
		UCHAR DisablePageOut   : 1;
		UCHAR Protection       : 3;
		UCHAR LogicalBlock[8];
		UCHAR TransferLength[4];
		UCHAR Reserved2;
		UCHAR Control;
	} CDB16;

	//
	// 16-byte CDBs
	//

	struct _EXT_READ16 {
		UCHAR OperationCode;      // 0x88 - SCSIOP_READ16
		UCHAR Reserved1         : 3;
		UCHAR ForceUnitAccess   : 1;
		UCHAR DisablePageOut    : 1;
		UCHAR ReadProtect       : 3;
		UCHAR LogicalBlock[8];
		UCHAR TransferLength[4];
		UCHAR Reserved2         : 7;
		UCHAR Streaming         : 1;
		UCHAR Control;
	} READ16;

	struct _EXT_WRITE16 {
		UCHAR OperationCode;      // 0x8A - SCSIOP_WRITE16
		UCHAR Reserved1         : 3;
		UCHAR ForceUnitAccess   : 1;
		UCHAR DisablePageOut    : 1;
		UCHAR WriteProtect      : 3;
		UCHAR LogicalBlock[8];
		UCHAR TransferLength[4];
		UCHAR Reserved2         : 7;
		UCHAR Streaming         : 1;
		UCHAR Control;
	} WRITE16;

	struct _EXT_VERIFY16 {
		UCHAR OperationCode;      // 0x8F - SCSIOP_VERIFY16
		UCHAR Reserved1         : 1;
		UCHAR ByteCheck         : 1;
		UCHAR BlockVerify       : 1;
		UCHAR Reserved2         : 1;
		UCHAR DisablePageOut    : 1;
		UCHAR VerifyProtect     : 3;
		UCHAR LogicalBlock[8];
		UCHAR VerificationLength[4];
		UCHAR Reserved3         : 7;
		UCHAR Streaming         : 1;
		UCHAR Control;
	} VERIFY16;

	struct _EXT_SYNCHRONIZE_CACHE16 {
		UCHAR OperationCode;      // 0x91 - SCSIOP_SYNCHRONIZE_CACHE16
		UCHAR Reserved1         : 1;
		UCHAR Immediate         : 1;
		UCHAR Reserved2         : 6;
		UCHAR LogicalBlock[8];
		UCHAR BlockCount[4];
		UCHAR Reserved3;
		UCHAR Control;
	} SYNCHRONIZE_CACHE16;

	struct _EXT_READ_CAPACITY16 {
		UCHAR OperationCode;      // 0x9E - SCSIOP_READ_CAPACITY16
		UCHAR ServiceAction     : 5;
		UCHAR Reserved1         : 3;
		UCHAR LogicalBlock[8];
		UCHAR BlockCount[4];
		UCHAR PMI               : 1;
		UCHAR Reserved2         : 7;
		UCHAR Control;
	} READ_CAPACITY16;

} CDBEXT, *PCDBEXT;

#pragma pack(pop, cdb)
#endif /* _CDBEXT_DEFINED */

FORCEINLINE
LONGLONG
NdasPortGetLogicalBlockAddressCDB16(PCDB Cdb2)
{
	PCDBEXT Cdb = (PCDBEXT) Cdb2;
	return (LONGLONG)(
		((ULONGLONG)Cdb->CDB16.LogicalBlock[0] << (8 * 7)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[1] << (8 * 6)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[2] << (8 * 5)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[3] << (8 * 4)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[4] << (8 * 3)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[5] << (8 * 2)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[6] << (8 * 1)) |
		((ULONGLONG)Cdb->CDB16.LogicalBlock[7] << (8 * 0)));
}

FORCEINLINE
LONGLONG
NdasPortGetLogicalBlockAddressCDB10(PCDB Cdb)
{
	return (LONGLONG)(
		((ULONGLONG)Cdb->CDB10.LogicalBlockByte0 << (8 * 3)) |
		((ULONGLONG)Cdb->CDB10.LogicalBlockByte1 << (8 * 2)) |
		((ULONGLONG)Cdb->CDB10.LogicalBlockByte2 << (8 * 1)) |
		((ULONGLONG)Cdb->CDB10.LogicalBlockByte3 << (8 * 0)));
}

FORCEINLINE
LONGLONG
NdasPortGetLogicalBlockAddressCDB6(PCDB Cdb)
{
	return (LONGLONG)(
		((ULONGLONG)Cdb->CDB6READWRITE.LogicalBlockMsb0 << (8 * 2)) |
		((ULONGLONG)Cdb->CDB6READWRITE.LogicalBlockMsb1 << (8 * 1)) |
		((ULONGLONG)Cdb->CDB6READWRITE.LogicalBlockLsb << (8 * 0)));
}

FORCEINLINE
ULONG
NdasPortGetTransferBlockCountCDB6(PCDB Cdb)
{
	return
		((ULONG)Cdb->CDB6READWRITE.TransferBlocks << (8 * 0));
}

FORCEINLINE
ULONG
NdasPortGetTransferBlockCountCDB10(PCDB Cdb)
{
	return 
		((ULONG)Cdb->CDB10.TransferBlocksMsb << (8 * 1)) |
		((ULONG)Cdb->CDB10.TransferBlocksLsb << (8 * 0));
}

FORCEINLINE
ULONG
NdasPortGetTransferBlockCountCDB16(PCDB Cdb2)
{
	PCDBEXT Cdb = (PCDBEXT) Cdb2;
	return
		((ULONG)Cdb->CDB16.TransferLength[0] << (8 * 3)) |
		((ULONG)Cdb->CDB16.TransferLength[1] << (8 * 2)) |
		((ULONG)Cdb->CDB16.TransferLength[2] << (8 * 1)) |
		((ULONG)Cdb->CDB16.TransferLength[3] << (8 * 0));
}

FORCEINLINE
NTSTATUS
NdasPortRetrieveCdbBlocks(
	__in PSCSI_REQUEST_BLOCK Srb,
	__out PLONGLONG LogicalBlockAddress,
	__out PULONG TransferBlocks)
{
	PCDB cdb = (PCDB) Srb->Cdb;
	switch (Srb->CdbLength)
	{
	case 6:
		*LogicalBlockAddress = NdasPortGetLogicalBlockAddressCDB6(cdb);
		*TransferBlocks = NdasPortGetTransferBlockCountCDB6(cdb);
		return STATUS_SUCCESS;
	case 10:
		*LogicalBlockAddress = NdasPortGetLogicalBlockAddressCDB10(cdb);
		*TransferBlocks = NdasPortGetTransferBlockCountCDB10(cdb);
		return STATUS_SUCCESS;
	case 16:
		*LogicalBlockAddress = NdasPortGetLogicalBlockAddressCDB16(cdb);
		*TransferBlocks = NdasPortGetTransferBlockCountCDB16(cdb);
		return STATUS_SUCCESS;
	default:
		return STATUS_NOT_SUPPORTED;
	}
}

FORCEINLINE
CONST NDASPORT_SCSI_DEVICE_TYPE*
NdasPortExGetScsiDeviceTypeInfo(
	__in UCHAR DeviceType)
{
	static const NDASPORT_SCSI_DEVICE_TYPE DeviceTypeInfo[] = {
		{L"Disk",       L"GenDisk",        L"DiskPeripheral",                TRUE},
		{L"Sequential", L"",               L"TapePeripheral",                TRUE},
		{L"Printer",    L"GenPrinter",     L"PrinterPeripheral",             FALSE},
		{L"Processor",  L"",               L"OtherPeripheral",               FALSE},
		{L"Worm",       L"GenWorm",        L"WormPeripheral",                TRUE},
		{L"CdRom",      L"GenCdRom",       L"CdRomPeripheral",               TRUE},
		{L"Scanner",    L"GenScanner",     L"ScannerPeripheral",             FALSE},
		{L"Optical",    L"GenOptical",     L"OpticalDiskPeripheral",         TRUE},
		{L"Changer",    L"ScsiChanger",    L"MediumChangerPeripheral",       TRUE},
		{L"Net",        L"ScsiNet",        L"CommunicationsPeripheral",      FALSE},
		{L"ASCIT8",     L"ScsiASCIT8",     L"ASCPrePressGraphicsPeripheral", FALSE},
		{L"ASCIT8",     L"ScsiASCIT8",     L"ASCPrePressGraphicsPeripheral", FALSE},
		{L"Array",      L"ScsiArray",      L"ArrayPeripheral",               FALSE},
		{L"Enclosure",  L"ScsiEnclosure",  L"EnclosurePeripheral",           FALSE},
		{L"RBC",        L"ScsiRBC",        L"RBCPeripheral",                 TRUE},
		{L"CardReader", L"ScsiCardReader", L"CardReaderPeripheral",          FALSE},
		{L"Bridge",     L"ScsiBridge",     L"BridgePeripheral",              FALSE},
		{L"Other",      L"ScsiOther",      L"OtherPeripheral",               FALSE}
	};

	static const SIZE_T NumberOfDeviceTypeInfo = countof(DeviceTypeInfo);
	C_ASSERT(countof(DeviceTypeInfo) == 18);
	// #define NUM_DEVICE_TYPE_INFO_ENTRIES 18

	PAGED_CODE();

	if (DeviceType >= NumberOfDeviceTypeInfo) 
	{
		return &(DeviceTypeInfo[NumberOfDeviceTypeInfo - 1]);
	}
	else 
	{
		return &(DeviceTypeInfo[DeviceType]);
	}
}


#ifdef __cplusplus
}
#endif

#endif /* _NDASPORT_H_ */
