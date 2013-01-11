#pragma once
#include <ndasport.h>
#include <ndasportwmi.h>
#include "ndasdiskioctl.h"
#include "lspio.h"
#include "trace.h"
#include "utils.h"
#include "cint.h"
#include "constants.h"
#include <ntdddisk.h>

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NdasAtaDeviceGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

NTSTATUS
NdasAtaDeviceInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

VOID
NdasAtaDeviceCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

NTSTATUS
NdasAtaDeviceLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters);

NTSTATUS
NdasAtaDeviceQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId);

NTSTATUS
NdasAtaDeviceQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__out PWCHAR* DeviceText);

NTSTATUS
NdasAtaDeviceQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__inout PDEVICE_CAPABILITIES Capabilities);

NTSTATUS
NdasAtaDeviceQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
NdasAtaDeviceQueryStorageDeviceIdProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

BOOLEAN
NdasAtaDeviceBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

BOOLEAN
NdasAtaDeviceStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

typedef enum _NDAS_ATA_DEVICE_STATE {
	NDASATA_STATE_NOT_INITIALIZED,
	NDASATA_STATE_STARTING,
	NDASATA_STATE_STARTED,
	NDASATA_STATE_DESTROYED,
	NDASATA_STATE_RECONNECTING
} NDAS_ATA_DEVICE_STATE;

//
// Status Flags
//
typedef enum _NDAS_ATA_DEVICE_FLAG {
	NDASATA_FLAG_RECONNECTING = 0x00000001,
	NDASATA_FLAG_DISCONNECTED = 0x00000002
} NDAS_ATA_DEVICE_FLAG;

typedef struct _NDAS_ATA_DEVICE_EXTENSION {

	PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension;
	PDEVICE_OBJECT DeviceObject;

	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;

	ULONG VirtualBytesPerBlock;
	LARGE_INTEGER StartLogicalBlockAddress;
	LARGE_INTEGER VirtualLogicalBlockAddress;

	PSCSI_REQUEST_BLOCK CurrentSrb;

	NDAS_ATA_DEVICE_STATE LuState;

	ULONG LuFlags;

	PLSP_IO_SESSION LspIoSession;
	lsp_handle_t LspHandle;

	ACCESS_MASK RequestedAccessMode;
	ACCESS_MASK CurrentAccessMode;

	LSP_TRANSPORT_ADDRESS DeviceAddress;
	LSP_TRANSPORT_ADDRESS ConnectedLocalAddress;
	LSP_TRANSPORT_ADDRESS LocalAddressList[8];
	ULONG LocalAddressCount;

	LSP_LOGIN_INFO LspLoginInfo;

	struct _LPX_SPECIFIC {
		TDI_ADDRESS_LPX LocalAddressList[8];
		ULONG LocalAddressCount;
		ULONG LocalAddressUpdateCounter;
	} LPX;

	ULONG ResetConnectionErrorCount;
	ULONG ResetConnectionFailThreshold;
	LARGE_INTEGER ResetConnectionDelay;

	IO_SCSI_CAPABILITIES IoScsiCapabilities;

	const lsp_ide_identify_device_data_t* LspIdentifyData;
	const lsp_ata_handshake_data_t* LspHandshakeData;

	// SCSI-II Reservation Support
	// NDAS_RESERVATION_DATA Reservation;

	//
	// Identify Data contains this information in word-sized format.
	// Model, SerialNumber and FirmwareRevision is adjusted to BYTE format
	// and normalized with no trailing spaces
	//
	CHAR DeviceModel[48];
	CHAR DeviceSerialNumber[24];
	CHAR DeviceFirmwareRevision[16];

	INQUIRYDATA InquiryData;
	STORAGE_BUS_TYPE StorageBusType;
	USHORT StorageBusMajorVersion;
	USHORT StorageBusMinorVersion;

	//
	// Session State Variables
	//
	UCHAR SmartCommand;
	ULONG SmartDataLength;

	ULONG SrbIoCount;
	LSP_IO_REQUEST LspIoRequest;

	NDASPORT_LOGICALUNIT_WMILIB_CONTEXT WmiLibInfo;

} NDAS_ATA_DEVICE_EXTENSION, *PNDAS_ATA_DEVICE_EXTENSION;

typedef struct _ATA_DEVICE_TYPE {
	PCSTR CompatibleIdString;
	PCSTR DeviceTypeString;
	PCSTR PeripheralIdString;
} ATA_DEVICE_TYPE, *PATA_DEVICE_TYPE;

//FORCEINLINE
//ULONG
//AtaGetBytesPerBlock(const lsp_ide_identify_device_data_t* IdentifyData);
//
//FORCEINLINE
//LONGLONG
//AtaGetLogicalBlockAddress(const lsp_ide_identify_device_data_t* IdentifyData);

FORCEINLINE
VOID
NdasAtaDeviceInitializeInquiryData(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension);

FORCEINLINE
VOID
NdasAtapUpdateLocalLpxAddresses(
	__in PNDAS_ATA_DEVICE_EXTENSION NdasAtaDeviceExtension);

NTSTATUS
LSPIOCALL
NdasAtapLspIoCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

VOID
LSPIOCALL
NdasAtapRestartSessionCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

typedef UCHAR SRBSTATUS;

NTSTATUS
NdasAtaWmiQueryRegInfo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_opt PUNICODE_STRING *RegistryPath,
	__out PUNICODE_STRING MofResourceName);

NTSTATUS
NdasAtaWmiQueryDataBlock(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG InstanceCount,
	__inout PULONG InstanceLengthArray,
	__in ULONG BufferAvail,
	__out PUCHAR Buffer);

#ifdef __cplusplus
}
#endif
