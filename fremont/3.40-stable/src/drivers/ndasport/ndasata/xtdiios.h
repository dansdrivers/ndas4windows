#pragma once
#include <xtdi.h>
#include "cint.h"

#ifndef ExFreePoolWithTag
#define ExFreePoolWithTag(POINTER,TAG) ExFreePool(POINTER)
#endif

#define TDI_IO_CONTEXT_IN_USE_BIT 1
#define TDI_IO_CONTEXT_POOL_TAG 'idtL'
#define TDI_IO_CONTEXT_IRP_POOL_TAG 'pdtL'

typedef struct _XTDI_IO_SESSION {
	PIRP Irp;
	ULONG MaxIrpSize;
	LONG Flags;
	XTDI_OVERLAPPED Overlapped;
#if DBG
	LARGE_INTEGER StartCounter;
#endif
	KTIMER TdiIoTimer;
	KDPC TdiIoTimerDpc;
	KSPIN_LOCK TdiIoTimerSpinLock;
	LONG TdiIoTimerDpcRunning;
	KEVENT TdiIoTimerDpcCompletion;
} XTDI_IO_SESSION, *PXTDI_IO_SESSION;

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
NTSTATUS
xTdiIoSessionCreate(
	__out PXTDI_IO_SESSION TdiIoSession,
	__in CCHAR IrpStackSize);

//
// IRQL <= APC_LEVEL if IO is queued
// IRQL <= DISPATCH_LEVEL if IO is never queued
//
FORCEINLINE
VOID
xTdiIoSessionDelete(
	__in PXTDI_IO_SESSION TdiIoSession);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
NTSTATUS
xTdiIoSessionRecreate(
	__in PXTDI_IO_SESSION TdiIoSession,
	__in CCHAR IrpStackSize);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
VOID
xTdiIoSessionSetCompletionRoutine(
	__inout PXTDI_IO_SESSION TdiIoSession,
	__in PXTDI_IO_COMPLETION_ROUTINE CompletionRoutine,
	__in PVOID CompletionContext);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
VOID
xTdiIoSessionReset(
	__inout PXTDI_IO_SESSION TdiIoSession);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
VOID
xTdiIoSessionStartTimer(
	__in PXTDI_IO_SESSION TdiIoSession,
	__in LARGE_INTEGER Timeout);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
VOID
xTdiIoSessionStopTimer(
	__in PXTDI_IO_SESSION TdiIoSession);

//
// IRQL == DISPATCH_LEVEL
//
FORCEINLINE
VOID
xTdiIoSessionpTimerDpc(
	__in PRKDPC Dpc,
	__in PVOID DeferredContext,
	__in PVOID SystemArgument1,
	__in PVOID SystemArgument2);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
PXTDI_IO_SESSION
xTdiIoSessionRetrieveFromOverlapped(
	__in PXTDI_OVERLAPPED Overlapped);

//
// Returns 1 if previously in use, 0 otherwise
//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
BOOLEAN
xTdiIoSessionMarkInUseBit(
	__in PXTDI_IO_SESSION TdiIoSession);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
BOOLEAN
xTdiIoSessionIsInUseBitSet(
	__in PXTDI_IO_SESSION TdiIoSession);

//
// IRQL <= DISPATCH_LEVEL
//
FORCEINLINE
BOOLEAN
xTdiIoSessionResetInUseBit(
	__in PXTDI_IO_SESSION TdiIoSession);

//
// Inline codes
//

FORCEINLINE
PXTDI_IO_SESSION
xTdiIoSessionRetrieveFromOverlapped(
	__in PXTDI_OVERLAPPED Overlapped)
{
	return CONTAINING_RECORD(Overlapped, XTDI_IO_SESSION, Overlapped);
}

FORCEINLINE
VOID
xTdiIoSessionReset(
	__inout PXTDI_IO_SESSION TdiIoSession)
{
	ASSERT(NULL != TdiIoSession->Irp);
	IoReuseIrp(TdiIoSession->Irp, STATUS_SUCCESS);
	TdiIoSession->Flags = 0;
	TdiIoSession->Overlapped.Internal = 0;
	TdiIoSession->Overlapped.InternalBuffer = 0;
	RtlZeroMemory(&TdiIoSession->Overlapped.IoStatus, sizeof(IO_STATUS_BLOCK));
}

FORCEINLINE
BOOLEAN
xTdiIoSessionMarkInUseBit(
	__in PXTDI_IO_SESSION TdiIoSession)
{
	return InterlockedBitTestAndSet(&TdiIoSession->Flags,TDI_IO_CONTEXT_IN_USE_BIT);
}

FORCEINLINE
BOOLEAN
xTdiIoSessionIsInUseBitSet(
	__in PXTDI_IO_SESSION TdiIoSession)
{
	return (InterlockedOr(&TdiIoSession->Flags, 0) & (1 << TDI_IO_CONTEXT_IN_USE_BIT)) != 0;
}

FORCEINLINE
BOOLEAN
xTdiIoSessionResetInUseBit(
	__in PXTDI_IO_SESSION TdiIoSession)
{
	return InterlockedBitTestAndReset(&TdiIoSession->Flags,TDI_IO_CONTEXT_IN_USE_BIT);
}

FORCEINLINE
NTSTATUS
xTdiIoSessionCreate(
	__out PXTDI_IO_SESSION TdiIoSession,
	__in CCHAR IrpStackSize)
{
	SIZE_T allocSize;
	USHORT irpSize;

	irpSize = IoSizeOfIrp(IrpStackSize);

	RtlZeroMemory(TdiIoSession, sizeof(XTDI_IO_SESSION));
	TdiIoSession->Irp = ExAllocatePoolWithTag(
		NonPagedPool, irpSize, TDI_IO_CONTEXT_IRP_POOL_TAG);

	if (NULL == TdiIoSession->Irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	TdiIoSession->MaxIrpSize = irpSize;
	IoInitializeIrp(TdiIoSession->Irp, irpSize, IrpStackSize);

	KeInitializeTimer(
		&TdiIoSession->TdiIoTimer);

	KeInitializeDpc(
		&TdiIoSession->TdiIoTimerDpc, 
		xTdiIoSessionpTimerDpc, 
		TdiIoSession);

	KeInitializeSpinLock(
		&TdiIoSession->TdiIoTimerSpinLock);

	KeInitializeEvent(
		&TdiIoSession->TdiIoTimerDpcCompletion,
		NotificationEvent,
		TRUE);

	return STATUS_SUCCESS;
}

FORCEINLINE
VOID
xTdiIoSessionDelete(
	__in PXTDI_IO_SESSION TdiIoSession)
{
	LARGE_INTEGER zero = {0};
	NTSTATUS status;

	status = KeWaitForSingleObject(
		&TdiIoSession->TdiIoTimerDpcCompletion,
		Executive,
		KernelMode,
		FALSE,
		&zero);

	if (STATUS_SUCCESS != status)
	{
		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

		KeWaitForSingleObject(
			&TdiIoSession->TdiIoTimerDpcCompletion,
			Executive,
			KernelMode,
			FALSE,
			NULL);
	}

	ExFreePoolWithTag(TdiIoSession->Irp, TDI_IO_CONTEXT_IRP_POOL_TAG);
	TdiIoSession->Irp = NULL;
}

FORCEINLINE
NTSTATUS
xTdiIoSessionRecreate(
	__in PXTDI_IO_SESSION TdiIoSession,
	__in CCHAR IrpStackSize)
{
	USHORT irpSize;
	CCHAR irpStackSize;
	PIRP irp;
	ULONG maxIrpSize;

	maxIrpSize = TdiIoSession->MaxIrpSize;
	irpSize = IoSizeOfIrp(IrpStackSize);
	if (irpSize > maxIrpSize)
	{
		irp = (PIRP) ExAllocatePoolWithTag(
			NonPagedPool, irpSize, TDI_IO_CONTEXT_IRP_POOL_TAG);

		if (NULL == irp)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		ExFreePoolWithTag(TdiIoSession->Irp, TDI_IO_CONTEXT_IRP_POOL_TAG);
		maxIrpSize = irpSize;
	}
	else
	{
		irp = TdiIoSession->Irp;
	}

	RtlZeroMemory(TdiIoSession, sizeof(XTDI_IO_SESSION));
	TdiIoSession->Irp = irp;
	TdiIoSession->MaxIrpSize = maxIrpSize;

	//
	// As the stack size may be changed, we cannot use IoReuseIrp,
	// instead we just initialize the raw memory with IoInitializeIrp
	//
	IoInitializeIrp(irp, irpSize, IrpStackSize);

	return STATUS_SUCCESS;
}

FORCEINLINE
VOID
xTdiIoSessionSetCompletionRoutine(
	__inout PXTDI_IO_SESSION TdiIoSession,
	__in PXTDI_IO_COMPLETION_ROUTINE CompletionRoutine,
	__in PVOID CompletionContext)
{
	TdiIoSession->Overlapped.CompletionRoutine = CompletionRoutine;
	TdiIoSession->Overlapped.UserContext = CompletionContext;
}

FORCEINLINE
VOID
xTdiIoSessionStartTimer(
	__in PXTDI_IO_SESSION TdiIoSession,
	__in LARGE_INTEGER Timeout)
{
	BOOLEAN queuedAlready;

	TdiIoSession->TdiIoTimerDpcRunning = 0;

	KeClearEvent(&TdiIoSession->TdiIoTimerDpcCompletion);

	queuedAlready = KeSetTimer(
		&TdiIoSession->TdiIoTimer, 
		Timeout, 
		&TdiIoSession->TdiIoTimerDpc);

	ASSERT(!queuedAlready);
}

FORCEINLINE
VOID
xTdiIoSessionStopTimer(
	__in PXTDI_IO_SESSION TdiIoSession)
{
	BOOLEAN stillQueued;
	KIRQL oldIrql;
	LONG dpcIsRunning;

	dpcIsRunning = 0;
	stillQueued = KeCancelTimer(&TdiIoSession->TdiIoTimer);
	if (stillQueued)
	{
		stillQueued = KeRemoveQueueDpc(&TdiIoSession->TdiIoTimerDpc);
		if (stillQueued)
		{
			dpcIsRunning = InterlockedCompareExchange(
				&TdiIoSession->TdiIoTimerDpcRunning, 1, 0);
			KdPrint(("TxIrp timer dpc may be running, irp=%p\n", TdiIoSession->Irp));
		}
	}
	if (!dpcIsRunning)
	{
		KeSetEvent(
			&TdiIoSession->TdiIoTimerDpcCompletion,
			IO_NO_INCREMENT,
			FALSE);
	}
}

FORCEINLINE
VOID
xTdiIoSessionpTimerDpc(
	__in PRKDPC Dpc,
	__in PVOID DeferredContext,
	__in PVOID SystemArgument1,
	__in PVOID SystemArgument2)
{
	PXTDI_IO_SESSION TdiIoSession;
	LONG haltDpc;

	TdiIoSession = (PXTDI_IO_SESSION) DeferredContext;

	haltDpc = InterlockedCompareExchange(
		&TdiIoSession->TdiIoTimerDpcRunning, 1, 0);

	if (!haltDpc)
	{
		BOOLEAN cancelled;
		KdPrint(("TxIrp timed out, irp=%p\n", TdiIoSession->Irp));
		cancelled = IoCancelIrp(TdiIoSession->Irp);
		if (!cancelled)
		{
			KdPrint(("TxIrp cancel failed, irp=%p\n", TdiIoSession->Irp));
		}
	}

	KeSetEvent(
		&TdiIoSession->TdiIoTimerDpcCompletion,
		IO_NO_INCREMENT,
		FALSE);

	return;
}
