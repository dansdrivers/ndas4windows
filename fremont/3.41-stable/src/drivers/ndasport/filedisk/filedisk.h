#pragma once
#include "ndasport.h"
#include "filediskioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Logical Unit Interface Query
//

NTSTATUS
FileDiskGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

//
// Logical Unit Interface Implementation
//

NTSTATUS
FileDiskInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

VOID
FileDiskCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

NTSTATUS
FileDiskLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters);

NTSTATUS
FileDiskQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId);

NTSTATUS
FileDiskQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__out PWCHAR* DeviceText);

NTSTATUS
FileDiskQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__inout PDEVICE_CAPABILITIES Capabilities);

NTSTATUS
FileDiskQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
FileDiskQueryStorageDeviceIdProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
FileDiskQueryStorageAdapterProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_ADAPTER_DESCRIPTOR AdapterDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
FileDiskQueryIoScsiCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out PIO_SCSI_CAPABILITIES IoScsiCapabilities,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

BOOLEAN
FileDiskBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

BOOLEAN
FileDiskStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

//
// Implementation
//

typedef struct _FILEDISK_EXTENSION {

	union {
		struct {
			UCHAR PortNumber;
			UCHAR PathId;
			UCHAR TargetId;
			UCHAR Lun;
		};
		ULONG LogicalUnitAddress;
	};

	ULONG FileDiskFlags;
	UNICODE_STRING FilePath;

	LARGE_INTEGER LogicalBlockAddress;
	ULONG BytesPerBlock;

	HANDLE FileHandle;
	PFILE_OBJECT FileObject;

	HANDLE ThreadHandle;
	PETHREAD ThreadObject;
	NTSTATUS ThreadStatus;

	//BOOLEAN LuReserved;
	//BOOLEAN ThirdPartyReservation;
	//UCHAR ThirdPartyDeviceId;
	//UCHAR LuReservedInitiator[8];

	BOOLEAN ThreadShouldStop;
	KEVENT ThreadNotificationEvent;
	KEVENT ThreadCompleteEvent;

	PSCSI_REQUEST_BLOCK CurrentSrb;

	//
	// Property Data
	//
	INQUIRYDATA InquiryData;
	IO_SCSI_CAPABILITIES IoScsiCapabilities;
	STORAGE_BUS_TYPE StorageBusType;
	USHORT StorageBusMajorVersion;
	USHORT StorageBusMinorVersion;

} FILEDISK_EXTENSION, *PFILEDISK_EXTENSION;

VOID
FORCEINLINE
FileDiskInitializeIoScsiCapabilities(
	__in PFILEDISK_EXTENSION FileDiskExtension);

VOID
FileDiskInitializeInquiryData(
	__in PFILEDISK_EXTENSION FileDiskExtension);

VOID
FileDiskGetInquiryData(
	__in PFILEDISK_EXTENSION FileDiskExtension,
	__inout PINQUIRYDATA InquiryData);

NTSTATUS
FileDiskCreateDataFile(
	__in PFILEDISK_EXTENSION FileDiskExtension,
	__in PUNICODE_STRING DataFilePath,
	__in PBOOLEAN NewDataFileCreated);

VOID
FileDiskCloseDataFile(
	__in PFILEDISK_EXTENSION FileDiskExtension);

NTSTATUS
FileDiskDeleteDataFile(
	__in PUNICODE_STRING FilePath);

VOID
FileDiskThreadRoutine(
	__in PVOID Context);

VOID
FileDiskCreateThreadWorkItemRoutine(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context);

#ifdef __cplusplus
}
#endif
