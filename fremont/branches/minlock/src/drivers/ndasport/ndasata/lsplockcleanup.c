#include <ntddk.h>
#include "lsplockcleanup.h"
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include "lspio.h"
#include "lspiolight.h"
#include "lspconnect.h"

#ifdef countof
#undef countof
#endif
#define countof(x) (sizeof(x) / sizeof((x)[0]))

#ifndef ExFreePoolWithTag
#define ExFreePoolWithTag(POINTER, TAG) ExFreePool(POINTER)
#endif

typedef enum _LSP_LOCK_CLEANUP_PHASE {
	LCU_PHASE_NONE,
	LCU_PHASE_TO_LOGIN,
	LCU_PHASE_TO_RELEASE_LOCK,
	LCU_PHASE_TO_LOGOUT,
	LCU_PHASE_TO_DISCONNECT,
	LCU_PHASE_TO_NEXT_CONNECTION,
	LCU_PHASE_TO_CLEANUP
} LSP_LOCK_CLEANUP_PHASE;

typedef struct _LSP_LOCK_CLEANUP_CONTEXT {
	LSP_IO_LIGHT_SESSION LspIoLightSession;
	lsp_request_packet_t LspRequestPacket;
	lsp_login_info_t LspLoginInfo;
	LSP_CONNECT_CONTEXT LspConnections[63];
	LONG PendingIo;
	ULONG LspCurrentConnectionIndex;
	LSP_LOCK_CLEANUP_PHASE NextPhase;
	PIO_WORKITEM CleanupWorkItem;
	PLSP_LOCK_CLEANUP_COMPLETION CompletionRoutine;
	PVOID CompletionContext;
} LSP_LOCK_CLEANUP_CONTEXT, *PLSP_LOCK_CLEANUP_CONTEXT;

VOID
LSPIOCALL
LspLockCleanuppLspConnectCompletion(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

VOID
LSPIOCALL
LspLockCleanuppLspRequestCompletion(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

VOID
LSPIOCALL
LspLockCleanuppPostCompletion(
	__in PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext);

VOID
LspLockCleanuppCleanupWorkItem(
    __in PDEVICE_OBJECT DeviceObject,
    __in_opt PVOID Context);

#define  LSP_LOCK_CONTEXT_POOL_TAG 'kclL'

PLSP_LOCK_CLEANUP_CONTEXT
LSPIOCALL
LspLockCleanupAllocate(
	__in PDEVICE_OBJECT DeviceObject)
{
	PLSP_LOCK_CLEANUP_CONTEXT p;
	ULONG size;
	
	size = sizeof(LSP_LOCK_CLEANUP_CONTEXT); 
	p = (PLSP_LOCK_CLEANUP_CONTEXT) ExAllocatePoolWithTag(
		NonPagedPool, size, LSP_LOCK_CONTEXT_POOL_TAG);

	if (NULL == p) 
	{
		return NULL;
	}

	RtlZeroMemory(p, size);
	p->CleanupWorkItem = IoAllocateWorkItem(DeviceObject);
	if (NULL == p->CleanupWorkItem)
	{
		ExFreePoolWithTag(p, LSP_LOCK_CONTEXT_POOL_TAG);
		return NULL;
	}

	return p;
}

VOID
LSPIOCALL
LspLockCleanupFree(
	__in PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext)
{
	IoFreeWorkItem(LockCleanupContext->CleanupWorkItem);
	ExFreePoolWithTag(LockCleanupContext, LSP_LOCK_CONTEXT_POOL_TAG);
}

NTSTATUS
LSPIOCALL
LspLockCleanup(
	__in PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext,
	__in const lsp_login_info_t* LspLoginInfo,
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PLSP_LOCK_CLEANUP_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	NTSTATUS status, status2;
	ULONG i;
	LONG pendingIo;
	IO_STATUS_BLOCK ioStatus;

	ASSERT(NULL == LockCleanupContext->CompletionRoutine);
	ASSERT(NULL == LockCleanupContext->CompletionContext);

	LockCleanupContext->LspLoginInfo = *LspLoginInfo;
	LockCleanupContext->LspLoginInfo.write_access = 0;

	status = LspIoLightSessionCreate(
		&LockCleanupContext->LspIoLightSession,
		LspConnection->ConnectionFileObject,
		LspConnection->ConnectionDeviceObject);

	if (!NT_SUCCESS(status))
	{
		ioStatus.Status = status;
		ioStatus.Information = 0;
		(*CompletionRoutine)(LockCleanupContext, &ioStatus, CompletionContext);
		return status;
	}

	for (i = 0; i < countof(LockCleanupContext->LspConnections); ++i)
	{
		status = LspConnectDuplicate(
			&LockCleanupContext->LspConnections[i],
			LspConnection);

		if (!NT_SUCCESS(status))
		{
			ULONG j;
			for (j = 0; j < i; ++j)
			{
				LspConnectCleanup(&LockCleanupContext->LspConnections[j]);
			}

			ioStatus.Status = status;
			ioStatus.Information = 0;
			(*CompletionRoutine)(LockCleanupContext, &ioStatus, CompletionContext);
			return status;
		}
	}

	LockCleanupContext->PendingIo = 1;
	LockCleanupContext->NextPhase = LCU_PHASE_TO_NEXT_CONNECTION;

	for (i = 0; i < countof(LockCleanupContext->LspConnections); ++i)
	{
		status2 = LspConnectConnect(
			&LockCleanupContext->LspConnections[i],
			NULL,
			LspLockCleanuppLspConnectCompletion,
			LockCleanupContext);
	}

	pendingIo = InterlockedDecrement(&LockCleanupContext->PendingIo);
	if (0 == pendingIo)
	{
		LspLockCleanuppPostCompletion(LockCleanupContext);
	}

	return STATUS_PENDING;
}

VOID
LSPIOCALL
LspLockCleanuppLspRequestCompletion(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context)
{
	NTSTATUS status;
	PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext;

	LockCleanupContext = (PLSP_LOCK_CLEANUP_CONTEXT) Context;

phase_reset:

	switch (LockCleanupContext->NextPhase)
	{
	case LCU_PHASE_TO_LOGIN:

		LockCleanupContext->NextPhase = LCU_PHASE_TO_RELEASE_LOCK;

		lsp_build_login(
			LspRequestPacket,
			LockCleanupContext->LspIoLightSession.LspHandle,
			&LockCleanupContext->LspLoginInfo);

		status = LspIoLightSessionRequest(
			&LockCleanupContext->LspIoLightSession,
			LspRequestPacket,
			LspLockCleanuppLspRequestCompletion,
			LockCleanupContext);

		break;

	case LCU_PHASE_TO_RELEASE_LOCK:

		if (!NT_SUCCESS(IoStatus->Status))
		{
			//
			// Login failure, continue with next connection
			// 
			LockCleanupContext->NextPhase = LCU_PHASE_TO_NEXT_CONNECTION;

			goto phase_reset;
		}

		LockCleanupContext->NextPhase = LCU_PHASE_TO_LOGOUT;

		lsp_build_release_lock(
			LspRequestPacket,
			LockCleanupContext->LspIoLightSession.LspHandle,
			LockCleanupContext->LspLoginInfo.unit_no);

		status = LspIoLightSessionRequest(
			&LockCleanupContext->LspIoLightSession,
			LspRequestPacket,
			LspLockCleanuppLspRequestCompletion,
			LockCleanupContext);

		break;

	case LCU_PHASE_TO_LOGOUT:

		//
		// Regardless of failure of release_lock (IO_ERR_LSP_FAILURE), 
		// we should logout. In most cases (99.99%), release_lock will fail.
		//

		if (!NT_SUCCESS(IoStatus->Status) && IoStatus->Status != IO_ERR_LSP_FAILURE)
		{
			//
			// TDI IO failure, continue with next connection
			// 
			LockCleanupContext->NextPhase = LCU_PHASE_TO_NEXT_CONNECTION;

			goto phase_reset;
		}

		LockCleanupContext->NextPhase = LCU_PHASE_TO_NEXT_CONNECTION;

		lsp_build_logout(
			LspRequestPacket,
			LockCleanupContext->LspIoLightSession.LspHandle);

		status = LspIoLightSessionRequest(
			&LockCleanupContext->LspIoLightSession,
			LspRequestPacket,
			LspLockCleanuppLspRequestCompletion,
			LockCleanupContext);

		break;

	case LCU_PHASE_TO_NEXT_CONNECTION:

		LspLockCleanuppPostCompletion(LockCleanupContext);

		break;

	default:
		ASSERTMSG("Invalid LockCleanup Phase", FALSE);
	}
}

VOID
LSPIOCALL
LspLockCleanuppPostCompletion(
	__in PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext)
{
	NTSTATUS status;
	PLSP_CONNECT_CONTEXT LspCurrentConnection;
	ULONG index;

reset_phase:

	switch (LockCleanupContext->NextPhase)
	{
	case LCU_PHASE_TO_NEXT_CONNECTION:
		{
			IO_STATUS_BLOCK ioStatus;

			index = LockCleanupContext->LspCurrentConnectionIndex++;

			if (index == countof(LockCleanupContext->LspConnections))
			{
				LockCleanupContext->NextPhase = LCU_PHASE_TO_DISCONNECT;
				LspCurrentConnection = NULL;
				goto reset_phase;
			}

			LspCurrentConnection = &LockCleanupContext->LspConnections[index];

			if (0 == index)
			{
				status = LspIoLightSessionCreate(
					&LockCleanupContext->LspIoLightSession,
					LspCurrentConnection->ConnectionFileObject,
					LspCurrentConnection->ConnectionDeviceObject);
			}
			else if (index < countof(LockCleanupContext->LspConnections))
			{
				status = LspIoLightSessionRecreate(
					&LockCleanupContext->LspIoLightSession,
					LspCurrentConnection->ConnectionFileObject,
					LspCurrentConnection->ConnectionDeviceObject);
			}


			LockCleanupContext->NextPhase = LCU_PHASE_TO_LOGIN;

			ioStatus.Status = STATUS_SUCCESS;
			ioStatus.Information = 0;

			LspLockCleanuppLspRequestCompletion(
				&LockCleanupContext->LspIoLightSession,
				LockCleanupContext->LspIoLightSession.ActiveLspRequest,
				&ioStatus,
				LockCleanupContext);

		}
		break;

	case LCU_PHASE_TO_DISCONNECT:
		{
			LONG pendingIo;
			PLSP_CONNECT_CONTEXT lspConnection;

			LockCleanupContext->NextPhase = LCU_PHASE_TO_CLEANUP;
			LockCleanupContext->PendingIo = 0;

			for (index = 0; index < countof(LockCleanupContext->LspConnections); ++index)
			{
				lspConnection = &LockCleanupContext->LspConnections[index];
				LspConnectDisconnect(
					lspConnection,
					TDI_DISCONNECT_RELEASE,
					NULL,
					LspLockCleanuppLspConnectCompletion,
					LockCleanupContext);
			}

			pendingIo = InterlockedDecrement(&LockCleanupContext->PendingIo);
			if (0 == pendingIo)
			{
				goto reset_phase;
			}
		}
		break;

	case LCU_PHASE_TO_CLEANUP:

		IoQueueWorkItem(
			LockCleanupContext->CleanupWorkItem, 
			LspLockCleanuppCleanupWorkItem,
			DelayedWorkQueue,
			LockCleanupContext);

		break;

	default:
		ASSERTMSG("LspLockCleanup session phase is invalid!", FALSE);
	}
}

VOID
LspLockCleanuppCleanupWorkItem(
    __in PDEVICE_OBJECT DeviceObject,
    __in_opt PVOID Context)
{
	PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext;
	PLSP_CONNECT_CONTEXT lspConnection;
	ULONG index;

	IO_STATUS_BLOCK ioStatus;
	PLSP_LOCK_CLEANUP_COMPLETION completionRoutine;
	PVOID completionContext;

	UNREFERENCED_PARAMETER(DeviceObject);

	LockCleanupContext = (PLSP_LOCK_CLEANUP_CONTEXT) Context;

	for (index = 0; index < countof(LockCleanupContext->LspConnections); ++index)
	{
		lspConnection = &LockCleanupContext->LspConnections[index];
		LspConnectCleanup(lspConnection);
	}

	LspIoLightSessionDelete(&LockCleanupContext->LspIoLightSession);

	ioStatus.Status = STATUS_SUCCESS;
	ioStatus.Information = 0;

	completionRoutine = LockCleanupContext->CompletionRoutine;
	completionContext = LockCleanupContext->CompletionContext;

	LockCleanupContext->NextPhase = LCU_PHASE_NONE;
	LockCleanupContext->CompletionRoutine = NULL;
	LockCleanupContext->CompletionContext = NULL;

	(*completionRoutine)(LockCleanupContext, &ioStatus, completionContext);
}

VOID
LSPIOCALL
LspLockCleanuppLspConnectCompletion(
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context)
{
	PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext;
	LONG pendingIo;

	LockCleanupContext = (PLSP_LOCK_CLEANUP_CONTEXT) Context;
	pendingIo = InterlockedDecrement(&LockCleanupContext->PendingIo);
	if (0 == pendingIo)
	{
		LspLockCleanuppPostCompletion(LockCleanupContext);
	}
}
