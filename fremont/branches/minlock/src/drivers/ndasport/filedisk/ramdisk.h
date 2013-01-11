#pragma once
#include "ndasport.h"
#include "ramdiskioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
RamDiskGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

NTSTATUS
RamDiskInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

VOID
RamDiskCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

NTSTATUS
RamDiskLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters);

NTSTATUS
RamDiskQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId);

NTSTATUS
RamDiskQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__out PWCHAR* DeviceText);

NTSTATUS
RamDiskQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__inout PDEVICE_CAPABILITIES Capabilities);

NTSTATUS
RamDiskQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
RamDiskQueryStorageDeviceIdProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_ID_DESCRIPTOR DeviceIdDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
RamDiskQueryStorageAdapterProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_ADAPTER_DESCRIPTOR AdapterDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

NTSTATUS
RamDiskQueryIoScsiCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out PIO_SCSI_CAPABILITIES IoScsiCapabilities,
	__in ULONG BufferLength,
	__out PULONG ResultLength);

BOOLEAN
RamDiskBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

BOOLEAN
RamDiskStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb);

#ifdef __cplusplus
}
#endif
