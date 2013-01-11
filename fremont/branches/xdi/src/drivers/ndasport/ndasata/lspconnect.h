#pragma once
#ifndef LSP_CONNECT_H_INCLUDED
#define LSP_CONNECT_H_INCLUDED

typedef
VOID
LSPIOCALL
LSP_CONNECT_COMPLETION(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

typedef LSP_CONNECT_COMPLETION *PLSP_CONNECT_COMPLETION;

typedef struct _LSP_CONNECT_CONTEXT {

	HANDLE AddressHandle;
	PFILE_OBJECT AddressFileObject;
	PDEVICE_OBJECT AddressDeviceObject;

	HANDLE ConnectionHandle;
	PFILE_OBJECT ConnectionFileObject;
	PDEVICE_OBJECT ConnectionDeviceObject;

	PLSP_CONNECT_COMPLETION ConnectionCompletionRoutine;
	PVOID ConnectionCompletionContext;

	PIRP ConnectIrp;
	XTDI_OVERLAPPED Overlapped;

	LSP_TRANSPORT_ADDRESS LocalAddress;
	LSP_TRANSPORT_ADDRESS RemoteAddress;

	LONG PendingIo;

} LSP_CONNECT_CONTEXT, *PLSP_CONNECT_CONTEXT;

//
// <remarks>
// IRQL = PASSIVE_LEVEL and with APCs enabled
// </remarks>
//
NTSTATUS
LSPIOCALL
LspConnectInitialize(
	__out PLSP_CONNECT_CONTEXT LspConnection,
	__in PLSP_TRANSPORT_ADDRESS LocalAddress,
	__in PLSP_TRANSPORT_ADDRESS RemoteAddress);

//
// <remarks>
// IRQL == PASSIVE_LEVEL
// </remarks>
//
VOID
LSPIOCALL
LspConnectCleanup(
	__in PLSP_CONNECT_CONTEXT LspConnection);

//
// <remarks>
// IRQL <= DISPATCH_LEVEL
// </remarks>
//
NTSTATUS
LSPIOCALL
LspConnectConnect(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_CONNECT_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

//
// <returns>
// LspConnectCancel returns TRUE if the connection request is canceled
// and FALSE if the connection request is not cancelable.
// </returns>
//
// <remarks>
// IRQL <= DISPATCH_LEVEL
// </remarks>
//
BOOLEAN
LSPIOCALL
LspConnectCancel(
	__in PLSP_CONNECT_CONTEXT LspConnection);

//
// <remarks>
// IRQL <= DISPATCH_LEVEL
// </remarks>
//
NTSTATUS
LSPIOCALL
LspConnectDisconnect(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_CONNECT_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

//
// <remarks>
// Callers of LspConnectDuplicate must be running at IRQL = PASSIVE_LEVEL 
// and with APCs enabled.
// </remarks>
//
NTSTATUS
LSPIOCALL
LspConnectDuplicate(
	__out PLSP_CONNECT_CONTEXT DuplicatedLspConnection,
	__in PLSP_CONNECT_CONTEXT SourceLspConnection);

#endif /* LSP_CONNECT_H_INCLUDED */
