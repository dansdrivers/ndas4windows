#include <ndasport.h>
#include "trace.h"
#include "utils.h"
#include <socketlpx.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include "lspio.h"
#include <xtdi.h>
#include <xtdilpx.h>
#include "lspconnect.h"

#ifdef RUN_WPP
#include "lspconnect.tmh"
#endif

VOID
LSPIOCALL
LspConnectCleanup(
	__in PLSP_CONNECT_CONTEXT LspConnection)
{
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	ASSERT(NULL != LspConnection->ConnectIrp);

	IoFreeIrp(LspConnection->ConnectIrp);

	LspConnection->ConnectIrp = NULL;

	ASSERT(NULL != LspConnection->ConnectionHandle);
	ASSERT(NULL != LspConnection->ConnectionFileObject);
	ASSERT(NULL != LspConnection->ConnectionDeviceObject);

	xTdiDisassociateAddress(
		LspConnection->ConnectionDeviceObject,
		LspConnection->ConnectionFileObject);

	xTdiCloseConnectionObject(
		LspConnection->ConnectionHandle,
		LspConnection->ConnectionFileObject);

	LspConnection->ConnectionHandle = NULL;
	LspConnection->ConnectionFileObject = NULL;
	LspConnection->ConnectionDeviceObject = NULL;

	ASSERT(NULL != LspConnection->AddressHandle);
	ASSERT(NULL != LspConnection->AddressFileObject);
	ASSERT(NULL != LspConnection->AddressDeviceObject);

	xTdiCloseAddressObject(
		LspConnection->AddressHandle,
		LspConnection->AddressFileObject);

	LspConnection->AddressHandle = NULL;
	LspConnection->AddressFileObject = NULL;
	LspConnection->AddressDeviceObject = NULL;
}

NTSTATUS
LSPIOCALL
LspConnectInitialize(
	__out PLSP_CONNECT_CONTEXT LspConnection,
	__in PLSP_TRANSPORT_ADDRESS LocalAddress,
	__in PLSP_TRANSPORT_ADDRESS RemoteAddress)
{
	NTSTATUS status, status2;
	ULONG i;

	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	RtlZeroMemory(LspConnection, sizeof(LSP_CONNECT_CONTEXT));

	LspConnection->LocalAddress = *LocalAddress;
	LspConnection->RemoteAddress = *RemoteAddress;

	status = xLpxTdiCreateConnectionObject(
		LspConnection,
		&LspConnection->ConnectionHandle,
		&LspConnection->ConnectionFileObject,
		&LspConnection->ConnectionDeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xLpxTdiCreateConnectionObject failed, status=%x\n", status);
		goto error1;
	}

	status = xLpxTdiCreateAddressObject(
		&LspConnection->LocalAddress.LpxAddress,
		&LspConnection->AddressHandle,
		&LspConnection->AddressFileObject,
		&LspConnection->AddressDeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xLpxTdiCreateConnectionObject failed, status=%x\n", status);
		goto error2;
	}

	status = xTdiAssociateAddress(
		LspConnection->AddressHandle,
		LspConnection->ConnectionDeviceObject,
		LspConnection->ConnectionFileObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xTdiAssociateAddress failed, status=%x\n", status);
		goto error3;
	}

	LspConnection->ConnectIrp = IoAllocateIrp(
		LspConnection->ConnectionDeviceObject->StackSize + 1,
		FALSE);

	if (NULL == LspConnection->ConnectIrp)
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"IoAllocateIrp failed\n");
		goto error4;
	}

	goto success;

error4:

error3:

	status2 = xTdiCloseAddressObject(
		LspConnection->ConnectionHandle,
		LspConnection->AddressFileObject);

	if (!NT_SUCCESS(status2))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
			"xTdiCloseAddressObject failed, status=%x\n", status2);
	}

error2:

	LspConnection->AddressHandle = NULL;
	LspConnection->AddressFileObject = NULL;
	LspConnection->AddressDeviceObject = NULL;

	status2 = xTdiCloseConnectionObject(
		LspConnection->ConnectionHandle,
		LspConnection->ConnectionFileObject);

	if (!NT_SUCCESS(status2))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
			"xTdiCloseConnectionObject failed, status=%x\n", status2);
	}

error1:

	LspConnection->ConnectionHandle = NULL;
	LspConnection->ConnectionFileObject = NULL;
	LspConnection->ConnectionDeviceObject = NULL;

success:

	return status;
}

VOID
LspConnectpTdiIoComplete(
	__in PIRP Irp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	PLSP_CONNECT_CONTEXT lspConnection;

	lspConnection = CONTAINING_RECORD(Overlapped, LSP_CONNECT_CONTEXT, Overlapped);

	ASSERT(NULL != lspConnection);
	ASSERT(NULL != lspConnection->ConnectionCompletionRoutine);

	(*lspConnection->ConnectionCompletionRoutine)(
		lspConnection,
		&Overlapped->IoStatus,
		lspConnection->ConnectionCompletionContext);
}

BOOLEAN
LSPIOCALL
LspConnectCancel(
	__in PLSP_CONNECT_CONTEXT LspConnection)
{
	if (NULL == LspConnection->ConnectIrp)
	{
		return FALSE;
	}
	return IoCancelIrp(LspConnection->ConnectIrp);
}

NTSTATUS
LSPIOCALL
LspConnectConnect(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_CONNECT_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	NTSTATUS status;

	ASSERT(NULL != LspConnection);
	ASSERT(NULL != CompletionRoutine);
	ASSERT(NULL != CompletionContext);

	if (NULL == LspConnection->ConnectIrp)
	{
		//
		// Non-active connection is specified, halt the processing and
		// call the completion routine.
		//

		LspConnection->Overlapped.IoStatus.Information = 0;
		LspConnection->Overlapped.IoStatus.Status = STATUS_INVALID_PARAMETER;

		(*CompletionRoutine)(
			LspConnection, 
			&LspConnection->Overlapped.IoStatus, 
			CompletionContext);

		return STATUS_INVALID_PARAMETER;
	}

	LspConnection->Overlapped.CompletionRoutine = LspConnectpTdiIoComplete;
	LspConnection->ConnectionCompletionRoutine = CompletionRoutine;
	LspConnection->ConnectionCompletionContext = CompletionContext;

	status = xLpxTdiConnectEx(
		LspConnection->ConnectIrp,
		LspConnection->ConnectionDeviceObject,
		LspConnection->ConnectionFileObject,
		&LspConnection->RemoteAddress.LpxAddress,
		Timeout,
		&LspConnection->Overlapped);

	return status;
}

NTSTATUS
LSPIOCALL
LspConnectDisconnect(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_CONNECT_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	NTSTATUS status;

	ASSERT(NULL != LspConnection);
	ASSERT(NULL != CompletionRoutine);
	ASSERT(NULL != CompletionContext);

	if (NULL == LspConnection->ConnectIrp)
	{
		//
		// Non-active connection is specified, halt the processing and
		// call the completion routine.
		//

		IO_STATUS_BLOCK ioStatus;
		ioStatus.Information = 0;
		ioStatus.Status = STATUS_INVALID_PARAMETER;

		(*CompletionRoutine)(LspConnection, &ioStatus, CompletionContext);

		return STATUS_INVALID_PARAMETER;
	}

	LspConnection->Overlapped.CompletionRoutine = LspConnectpTdiIoComplete;
	LspConnection->ConnectionCompletionRoutine = CompletionRoutine;
	LspConnection->ConnectionCompletionContext = CompletionContext;

	status = xTdiDisconnectEx(
		LspConnection->ConnectIrp,
		LspConnection->ConnectionDeviceObject,
		LspConnection->ConnectionFileObject,
		DisconnectFlags,
		Timeout,
		NULL,
		NULL, 
		&LspConnection->Overlapped);

	return status;
}

NTSTATUS
LSPIOCALL
LspConnectDuplicate(
	__out PLSP_CONNECT_CONTEXT DuplicatedLspConnection,
	__in PLSP_CONNECT_CONTEXT SourceLspConnection)
{
	NTSTATUS status;

	status = LspConnectInitialize(
		DuplicatedLspConnection,
		&SourceLspConnection->LocalAddress,
		&SourceLspConnection->RemoteAddress);

	return status;
}
