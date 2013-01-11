#pragma once
#include "lspconnect.h"

typedef VOID (LSPIOCALL *PLSP_FAST_CONNECT_COMPLETION)(
	__in PLSP_FAST_CONNECT_CONTEXT FastConnectContext,
	__in PLSP_CONNECT_CONTEXT ActiveConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

typedef struct _LSP_FAST_CONNECT_CONTEXT {

	PDEVICE_OBJECT DeviceObject; /* for IoAllocateWorkItem */
	PLSP_CONNECT_CONTEXT ActiveLspConnection;
	LSP_CONNECT_CONTEXT LspConnections[LSP_MAX_ADDRESS_COUNT];

	struct {
		LONG Queued;
		LONG Pending;
		LONG Connected;
	} Counters;

	KEVENT QueueCompleted;
	KEVENT CompletionCompleted;
	PIO_WORKITEM ConnectCompleteWorkItem;

	IO_STATUS_BLOCK IoStatus;
	PLSP_FAST_CONNECT_COMPLETION CompletionRoutine;
	PVOID CompletionContext;

} LSP_FAST_CONNECT_CONTEXT, *PLSP_FAST_CONNECT_CONTEXT;

//
// IRQL == PASSIVE_LEVEL
//
NTSTATUS
LSPIOCALL
LspFastConnectInitialize(
	__out PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext,
	__in PDEVICE_OBJECT DeviceObject);

//
// IRQL <= DISPATCH_LEVEL
//
VOID
LSPIOCALL
LspFastConnectUninitialize(
	__in PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext);

//
// IRQL == PASSIVE_LEVEL
//
NTSTATUS
LSPIOCALL
LspFastConnectConnect(
	__in PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext,
	__in PLSP_TRANSPORT_ADDRESS RemoteAddress,
	__in_ecount(LocalAddressCount) PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_FAST_CONNECT_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);
