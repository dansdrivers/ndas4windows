#ifndef _XTDI_H_
#define _XTDI_H_
#include <tdikrnl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _XTDI_OVERLAPPED XTDI_OVERLAPPED, *PXTDI_OVERLAPPED;

typedef struct _XTDI_EVENT_HANDLER {
	USHORT EventId;
	PVOID  EventHandler;
} XTDI_EVENT_HANDLER, *PXTDI_EVENT_HANDLER;

typedef 
VOID 
(*PXTDI_IO_COMPLETION_ROUTINE)(
	__in PIRP Irp,
	__in PXTDI_OVERLAPPED Overlapped);

typedef struct _XTDI_OVERLAPPED {
	PXTDI_IO_COMPLETION_ROUTINE CompletionRoutine;
	PKEVENT Event;
	IO_STATUS_BLOCK IoStatus;
	PVOID UserContext;
	ULONG_PTR Internal;
	PVOID InternalBuffer;
} XTDI_OVERLAPPED, *PXTDI_OVERLAPPED;

//
// IRQL = PASSIVE_LEVEL and with APCs enabled
//
NTSTATUS
xTdiCreateAddressObject(
	__in PCWSTR DeviceName,
	__in PFILE_FULL_EA_INFORMATION EaInformation,
	__in ULONG EaInformationLength,
	__out HANDLE *AddressHandle,
	__out PFILE_OBJECT *AddressFileObject,
	__out PDEVICE_OBJECT *AddressDeviceObject);

//
// IRQL = PASSIVE_LEVEL
//
NTSTATUS
xTdiCloseAddressObject(
	__in HANDLE AddressHandle,
	__in PFILE_OBJECT AddressFileObject);

//
// IRQL = PASSIVE_LEVEL and with APCs enabled
//
NTSTATUS
xTdiCreateConnectionObject(
	__in PCWSTR DeviceName,
	__in CONNECTION_CONTEXT ConnectionContext,
	__out HANDLE *ConnectionHandle,
	__out PFILE_OBJECT *ConnectionFileObject,
	__out PDEVICE_OBJECT *ConnectionDeviceObject);

//
// IRQL = PASSIVE_LEVEL
//
NTSTATUS
xTdiCloseConnectionObject(
	__in HANDLE ConnectionHandle,
	__in PFILE_OBJECT ConnectionFileObject);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiAssociateAddressEx(
	__in PIRP Irp,
	__in HANDLE AddressHandle,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= APC_LEVEL and in a nonarbitrary thread context.
//
NTSTATUS
xTdiAssociateAddress(
	__in HANDLE AddressHandle,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiDisassociateAddressEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= APC_LEVEL and in a nonarbitrary thread context.
//
NTSTATUS
xTdiDisassociateAddress(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiConnectEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in_bcount(RemoteAddressLength) PTRANSPORT_ADDRESS RemoteAddress,
	__in LONG RemoteAddressLength,
	__in_opt PLARGE_INTEGER ConnectionTimeout,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= APC_LEVEL and in a nonarbitrary thread context.
//
NTSTATUS
xTdiConnect(
	__in PDEVICE_OBJECT TdiConnDeviceObject,
	__in PFILE_OBJECT TdiConnFileObject,
	__in PTRANSPORT_ADDRESS RemoteAddress,
	__in LONG RemoteAddressLength,
	__in_opt PLARGE_INTEGER  ConnectionTimeout);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiDisconnectEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER  DisconnectTimeout,
	__in_opt PTDI_CONNECTION_INFORMATION RequestConnectionInfo,
	__out_opt PTDI_CONNECTION_INFORMATION ReturnConnectionInfo,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= APC_LEVEL and in a nonarbitrary thread context.
//
NTSTATUS
xTdiDisconnect(
	__in PDEVICE_OBJECT TdiConnDeviceObject,
	__in PFILE_OBJECT TdiConnFileObject,
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER DisconnectTimeout,
	__in_opt PTDI_CONNECTION_INFORMATION RequestConnectionInfo,
	__out_opt PTDI_CONNECTION_INFORMATION ReturnConnectionInfo);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiQueryInformationEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT TdiDeviceObject,
	__in PFILE_OBJECT TdiFileObject,
	__in ULONG QueryType,
	__out_bcount(BufferLength) PVOID Buffer,
	__in ULONG BufferLength,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= APC_LEVEL and in a nonarbitrary thread context.
//
NTSTATUS
xTdiQueryInformation(
	__in PDEVICE_OBJECT TdiDeviceObject,
	__in PFILE_OBJECT TdiFileObject,
	__in ULONG QueryType,
	__out_bcount(BufferLength) PVOID Buffer,
	__in ULONG BufferLength);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiSetEventHandlerEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT DeviceObject,
	__in PFILE_OBJECT FileObject,
	__in USHORT EventId,
	__in PVOID EventHandler,
	__in PVOID EventContext,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= APC_LEVEL and in a nonarbitrary thread context.
//
NTSTATUS
xTdiSetEventHandler(
	__in PDEVICE_OBJECT DeviceObject,
	__in PFILE_OBJECT FileObject,
	__in_ecount(CountOfEvents) PXTDI_EVENT_HANDLER EventsToSet,
	__in ULONG CountOfEvents,
	__in PVOID EventContext);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiSendMdlEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToSend,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiSendMdl(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToSend,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiSendEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiSend(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiReceiveMdlEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToReceive,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiReceiveMdl(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToReceive,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiReceiveEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
xTdiReceive(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped);

#ifdef __cplusplus
}
#endif

#endif /* _XTDILPX_H_ */
