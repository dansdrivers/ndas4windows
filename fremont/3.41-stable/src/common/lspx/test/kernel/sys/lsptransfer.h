#pragma once

NTSTATUS
LspInitializeConnection(
	__in PDEVICE_OBJECT DeviceObject);

NTSTATUS
LspCleanupConnection(
	__in PDEVICE_OBJECT DeviceObject);

NTSTATUS
LspProcessSynchronous(
	__in PDEVICE_OBJECT DeviceObject,
	__inout lsp_status_t* LspStatus);

NTSTATUS
LspProcessNext(
	__in PDEVICE_OBJECT DeviceObject,
	__inout lsp_status_t* LspStatus);

VOID
LspTransferCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LspDataInitialize(
	__in PDEVICE_OBJECT DeviceObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject, 
	__inout PLSP_TRANSFER_DATA LspTransferData);

VOID
LspDataCleanup(
	__in PLSP_TRANSFER_DATA LspTransferData);

VOID
LspDataReset(
	__in PDEVICE_OBJECT DeviceObject,
	__in PLSP_TRANSFER_DATA LspTransferData);

PLSP_TRANSFER_DATA
LspDataSelect(
	__in PDEVICE_OBJECT DeviceObject);

