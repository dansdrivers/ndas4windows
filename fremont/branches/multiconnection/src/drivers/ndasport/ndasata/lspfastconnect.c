#include <ndasport.h>
#include "trace.h"
#include "utils.h"
#include <socketlpx.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include "lspio.h"
#include <xtdi.h>
#include <xtdilpx.h>
#include "lspfastconnect.h"

#ifdef RUN_WPP
#include "lspfastconnect.tmh"
#endif

VOID
LspFastConnectpPostCompletionWorkItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context);

VOID
LSPIOCALL
LspFastConnectpConnectCompletion(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

NTSTATUS
LSPIOCALL
LspFastConnectInitialize(
	__out PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext,
	__in PDEVICE_OBJECT DeviceObject)
{
	NTSTATUS status;

	RtlZeroMemory(
		LspFastConnectContext, 
		sizeof(LSP_FAST_CONNECT_CONTEXT));

	KeInitializeEvent(
		&LspFastConnectContext->CompletionCompleted,
		NotificationEvent,
		TRUE);

	KeInitializeEvent(
		&LspFastConnectContext->QueueCompleted,
		NotificationEvent,
		FALSE);

	LspFastConnectContext->DeviceObject = DeviceObject;

	LspFastConnectContext->ConnectCompleteWorkItem = 
		IoAllocateWorkItem(LspFastConnectContext->DeviceObject);

	if (NULL == LspFastConnectContext->ConnectCompleteWorkItem)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"IoAllocateWorkItem failed, status=%x\n", status);
		return status;
	}

	return STATUS_SUCCESS;
}

VOID
LSPIOCALL
LspFastConnectUninitialize(
	__in PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext)
{
	ASSERT(NULL != LspFastConnectContext->ConnectCompleteWorkItem);
	IoFreeWorkItem(LspFastConnectContext->ConnectCompleteWorkItem);
	LspFastConnectContext->ConnectCompleteWorkItem = NULL;
}

NTSTATUS
LSPIOCALL
LspFastConnectConnect(
	__in PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext,
	__in PLSP_TRANSPORT_ADDRESS RemoteAddress,
	__in_ecount(LocalAddressCount) PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_FAST_CONNECT_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext)
{
	NTSTATUS status;
	ULONG i, count;

	ASSERT(NULL != CompletionRoutine);

	LspFastConnectContext->CompletionRoutine = CompletionRoutine;
	LspFastConnectContext->CompletionContext = CompletionContext;

	ASSERT(NULL == LspFastConnectContext->ActiveLspConnection);
	LspFastConnectContext->ActiveLspConnection = NULL;

	LspFastConnectContext->Counters.Queued = 0;
	LspFastConnectContext->Counters.Connected = 0;

	count = min(LocalAddressCount, LSP_MAX_ADDRESS_COUNT);

	if (0 == count)
	{
		IO_STATUS_BLOCK ioStatus;
		ioStatus.Status = STATUS_NO_MORE_ENTRIES;
		ioStatus.Information = 0;

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"Local address count is zero, status=%x\n", ioStatus.Status);

		(*CompletionRoutine)(LspFastConnectContext, NULL, &ioStatus, CompletionContext);

		return STATUS_NO_MORE_ENTRIES;
	}

	LspFastConnectContext->Counters.Queued = count;

	for (i = 0; i < count; ++i)
	{
		PLSP_CONNECT_CONTEXT lspConnection;

		lspConnection = &LspFastConnectContext->LspConnections[i];

		status = LspConnectInitialize(
			lspConnection,
			&LocalAddressList[i],
			RemoteAddress);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspConnectionInitialize failed, status=%x\n", status);
		}
	}

	LspFastConnectContext->Counters.Pending = count;

	KeClearEvent(&LspFastConnectContext->QueueCompleted);
	KeClearEvent(&LspFastConnectContext->CompletionCompleted);

	for (i = 0; i < count; ++i)
	{
		PLSP_CONNECT_CONTEXT lspConnection;

		lspConnection = &LspFastConnectContext->LspConnections[i];

		status = LspConnectConnect(
			lspConnection,
			NULL,
			LspFastConnectpConnectCompletion,
			LspFastConnectContext);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
				"LspConnectConnect failed, status=%x\n", status);
		}
	}

	KeSetEvent(
		&LspFastConnectContext->QueueCompleted,
		IO_NO_INCREMENT,
		FALSE);

	return STATUS_PENDING;
}

VOID
LSPIOCALL
LspFastConnectpConnectCompletion(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context)
{
	PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext;
	LONG pending;
	LONG connected;

	LspFastConnectContext = (PLSP_FAST_CONNECT_CONTEXT) Context;

	if (STATUS_SUCCESS == IoStatus->Status)
	{
		connected = InterlockedIncrement(&LspFastConnectContext->Counters.Connected);

		if (1 == connected)
		{
			LspFastConnectContext->ActiveLspConnection = LspConnection;
			LspFastConnectContext->IoStatus = *IoStatus;

			IoQueueWorkItem(
				LspFastConnectContext->ConnectCompleteWorkItem,
				LspFastConnectpPostCompletionWorkItem,
				DelayedWorkQueue,
				LspFastConnectContext);
		}
	}

	pending = InterlockedDecrement(&LspFastConnectContext->Counters.Pending);

	if (0 == pending)
	{
		if (0 == LspFastConnectContext->Counters.Connected)
		{
			//
			// All connect requests are failed
			//
			ASSERT(IoStatus->Status != STATUS_SUCCESS);
			LspFastConnectContext->ActiveLspConnection = LspConnection;
			LspFastConnectContext->IoStatus = *IoStatus;

			IoQueueWorkItem(
				LspFastConnectContext->ConnectCompleteWorkItem,
				LspFastConnectpPostCompletionWorkItem,
				DelayedWorkQueue,
				LspFastConnectContext);
		}

		KeSetEvent(
			&LspFastConnectContext->CompletionCompleted, 
			IO_NO_INCREMENT,
			FALSE);
	}
}

VOID
LspFastConnectpPostCompletionWorkItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context)
{
	PLSP_FAST_CONNECT_CONTEXT lspFastConnectContext;
	PLSP_CONNECT_CONTEXT ActiveLspConnection;
	ULONG i, count;

	lspFastConnectContext = (PLSP_FAST_CONNECT_CONTEXT) Context;

	//
	// Wait for all connect request are queued
	//

	KeWaitForSingleObject(
		&lspFastConnectContext->QueueCompleted,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"Queue Connect Completed\n");

	//
	// Now all connection requests are queued
	// Cancel Connection IRPs that is not completed yet
	//

	count = lspFastConnectContext->Counters.Queued;

	for (i = 0; i < count; ++i)
	{
		BOOLEAN cancelled;
		PLSP_CONNECT_CONTEXT lspConnection;
		lspConnection = &lspFastConnectContext->LspConnections[i];
		if (lspConnection != lspFastConnectContext->ActiveLspConnection)
		{
			cancelled = LspConnectCancel(lspConnection);
			if (!cancelled)
			{
				NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
					"LspConnectCancel failed, LspConnection=%p\n", lspConnection);
			}
		}
	}

	//
	// Wait for all connection requests are completed
	//

	KeWaitForSingleObject(
		&lspFastConnectContext->CompletionCompleted,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"Connect Completion Completed\n");

	for (i = 0; i < count; ++i)
	{
		PLSP_CONNECT_CONTEXT lspConnection;
		lspConnection = &lspFastConnectContext->LspConnections[i];
		if (lspConnection != lspFastConnectContext->ActiveLspConnection)
		{
			LspConnectCleanup(lspConnection);
		}
	}

	//
	// If the ActiveLspConnection is not null, we have a connection.
	// Otherwise, every connection attempt is failed.
	//

	//
	// Call the completion routine
	//

	ActiveLspConnection = lspFastConnectContext->ActiveLspConnection;
	lspFastConnectContext->ActiveLspConnection = NULL;

	(*lspFastConnectContext->CompletionRoutine)(
		lspFastConnectContext,
		ActiveLspConnection,
		&lspFastConnectContext->IoStatus,
		lspFastConnectContext->CompletionContext);
}
