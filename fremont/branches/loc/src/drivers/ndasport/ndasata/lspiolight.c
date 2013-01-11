#include <ndasport.h>
#include "trace.h"
#include "utils.h"
#include <socketlpx.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include "lspio.h"
#include <xtdi.h>
#include <xtdilpx.h>
#include <xtdiios.h>
#include "lspiolight.h"
#include "lspconnect.h"

FORCEINLINE
VOID
LSPIOCALL
LspIoLightSessionpCompleteLspRequest(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in PIO_STATUS_BLOCK IoStatus);

FORCEINLINE
PXTDI_IO_SESSION
LspIoLightSessionpGetNextTdiIoSession(
	PLSP_IO_LIGHT_SESSION LspIoLightSession);

VOID
LspIoLightSessionpTdiIoCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LSPIOCALL
LspIoLightSessionpProcessTransfer(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession);

enum {
	LSP_IO_SESSION_BUFFER_POOL_TAG = 'bpsL'
};

NTSTATUS
LSPIOCALL
LspIoLightSessionCreate(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject)
{
	NTSTATUS status;
	ULONG i, j;
	PVOID sessionBuffer;

	RtlZeroMemory(LspIoLightSession, sizeof(LSP_IO_LIGHT_SESSION));

	sessionBuffer = ExAllocatePoolWithTag(
		NonPagedPool, LSP_SESSION_BUFFER_SIZE, LSP_IO_SESSION_BUFFER_POOL_TAG);

	if (NULL == sessionBuffer)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto error1;
	}

	LspIoLightSession->LspSessionBuffer = sessionBuffer;

	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		status = xTdiIoSessionCreate(
			&LspIoLightSession->TdiIoSession[i], 
			ConnectionDeviceObject->StackSize + 1);
		if (!NT_SUCCESS(status))
		{
			goto error3;
		}

		xTdiIoSessionSetCompletionRoutine(
			&LspIoLightSession->TdiIoSession[i],
			LspIoLightSessionpTdiIoCompletion,
			LspIoLightSession);
	}

	LspIoLightSession->ConnectionFileObject = ConnectionFileObject;
	LspIoLightSession->ConnectionDeviceObject = ConnectionDeviceObject;

	LspIoLightSession->LspHandle = lsp_initialize_session(
		LspIoLightSession->LspSessionBuffer, LSP_SESSION_BUFFER_SIZE);

	ASSERT(NULL != LspIoLightSession->LspHandle);

	LspIoLightSession->SendTimeout.QuadPart = -5 * SECOND_IN_100_NS;
	LspIoLightSession->ReceiveTimeout.QuadPart = -30 * SECOND_IN_100_NS;

	status = STATUS_SUCCESS;

	goto success;

error3:

	for (j = 0; j < i; ++j)
	{
		xTdiIoSessionDelete(&LspIoLightSession->TdiIoSession[j]);
	}

// error2:

	ExFreePoolWithTag(sessionBuffer, LSP_IO_SESSION_BUFFER_POOL_TAG);

error1:
success:

	return status;
}

VOID
LSPIOCALL
LspIoLightSessionDelete(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession)
{
	ULONG i;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		xTdiIoSessionDelete(&LspIoLightSession->TdiIoSession[i]);
	}

	ExFreePoolWithTag(
		LspIoLightSession->LspSessionBuffer, 
		LSP_IO_SESSION_BUFFER_POOL_TAG);
}

NTSTATUS
LSPIOCALL
LspIoLightSessionRecreate(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject)
{
	NTSTATUS status;
	ULONG i;

	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		status = xTdiIoSessionRecreate(
			&LspIoLightSession->TdiIoSession[i],
			ConnectionDeviceObject->StackSize + 1);

		if (!NT_SUCCESS(status))
		{
			return status;
		}
	}

	LspIoLightSession->LspHandle = lsp_initialize_session(
		LspIoLightSession->LspSessionBuffer, LSP_SESSION_BUFFER_SIZE);

	LspIoLightSession->ConnectionFileObject = ConnectionFileObject;
	LspIoLightSession->ConnectionDeviceObject = ConnectionDeviceObject;

	return STATUS_SUCCESS;
}

NTSTATUS
LSPIOCALL
LspIoLightSessionRequest(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PLSP_IO_LIGHT_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	ASSERT(FALSE == pTestFlag(LspIoLightSession->SessionFlags, LSP_IO_LIGHT_SESSION_OP_IN_PROGRESS));
	pSetFlag(&LspIoLightSession->SessionFlags, LSP_IO_LIGHT_SESSION_OP_IN_PROGRESS);

	ASSERT(NULL == LspIoLightSession->ActiveLspRequest);
	LspIoLightSession->CompletionRoutine = CompletionRoutine;
	LspIoLightSession->CompletionContext = CompletionContext;
	LspIoLightSession->ActiveLspRequest = LspRequestPacket;

	InterlockedIncrement(&LspIoLightSession->RequestSequence);

	LspIoLightSession->LspStatus = lsp_request(
		LspIoLightSession->LspHandle,
		LspRequestPacket);

	return LspIoLightSessionpProcessTransfer(LspIoLightSession);
}

FORCEINLINE
VOID
LspIoLightSessionpReuseTdiIoSession(
	__inout PXTDI_IO_SESSION TdiIoSession)
{
	xTdiIoSessionReset(TdiIoSession);
}

FORCEINLINE
VOID
LspIoLightSessionpResetTdiIoContext(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession)
{
	LONG i;
	ASSERT(0 == LspIoLightSession->PendingTdiIo);
	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		if (!xTdiIoSessionIsInUseBitSet(&LspIoLightSession->TdiIoSession[i])) 
		{
			break;
		}
		LspIoLightSessionpReuseTdiIoSession(
			&LspIoLightSession->TdiIoSession[i]);
	}
}

//
// Called when there is no outstanding IO left to proceed to the next
// step in lsp process. LspIoLightSession->OutstandingTdiIo must be
// zero when this function is called.
//
BOOLEAN
LSPIOCALL
LspIoLightSessionpErrorInTdiIo(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession)
{
	NTSTATUS status;
	PIO_STATUS_BLOCK ioStatus;
	ULONG i;

	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		if (!xTdiIoSessionIsInUseBitSet(&LspIoLightSession->TdiIoSession[i]))
		{
			return FALSE;
		}

		ioStatus = &LspIoLightSession->TdiIoSession[i].Overlapped.IoStatus;

		if (!NT_SUCCESS(ioStatus->Status))
		{
			NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
				"TDI function[%d] failed, status=%x\n", i, ioStatus->Status);

			return TRUE;
		}
	}

	return FALSE;
}

VOID
LspIoLightSessionpTdiIoCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PXTDI_IO_SESSION TdiData = xTdiIoSessionRetrieveFromOverlapped(Overlapped);
	PLSP_IO_LIGHT_SESSION LspIoLightSession = (PLSP_IO_LIGHT_SESSION) Overlapped->UserContext;
	LONG pendingTdiIo;

	UNREFERENCED_PARAMETER(TransferIrp);

	pendingTdiIo = InterlockedDecrement(&LspIoLightSession->PendingTdiIo);
	ASSERT(pendingTdiIo >= 0);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"TransferIrp=%p, LspIoLightSession=%p, Status=%x, TdiIoCount=%d\n", 
		TransferIrp, LspIoLightSession, Overlapped->IoStatus.Status, pendingTdiIo);

	xTdiIoSessionStopTimer(TdiData);

	if (0 == pendingTdiIo)
	{
		if (LspIoLightSessionpErrorInTdiIo(LspIoLightSession))
		{
			IO_STATUS_BLOCK completeIoStatus;
			LspIoLightSessionpResetTdiIoContext(LspIoLightSession);

			//
			// We do not use TDI status directly.
			// Use STATUS_UNEXPECTED_NETWORK_ERROR for all errors.
			//

			completeIoStatus.Status = STATUS_UNEXPECTED_NETWORK_ERROR;
			completeIoStatus.Information = 0;

			LspIoLightSessionpCompleteLspRequest(
				LspIoLightSession, &completeIoStatus);
		}
		else
		{
			LspIoLightSessionpResetTdiIoContext(LspIoLightSession);
			LspIoLightSession->LspStatus = lsp_process_next(
				LspIoLightSession->LspHandle);
			LspIoLightSessionpProcessTransfer(LspIoLightSession);
		}
	}
}

NTSTATUS
LSPIOCALL
LspIoLightSessionpProcessTransfer(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession)
{
	NTSTATUS status, status2;
	LONG pendingTdiIo;

	ASSERT(0 == LspIoLightSession->PendingTdiIo);
	pendingTdiIo = InterlockedIncrement(&LspIoLightSession->PendingTdiIo);
	ASSERT(1 == pendingTdiIo);

	while (TRUE)
	{
		//
		// Initial Count is 1.
		// When this counter reaches 0, it indicates that all outstanding 
		// TDI IO requests are completed.
		//

		switch (LspIoLightSession->LspStatus)
		{
		case LSP_REQUIRES_SEND:
			{
				PXTDI_IO_SESSION tdiIoSession;
				PVOID dataBuffer;
				ULONG bytesTransferred;
				UINT32 dataBufferLength;

				tdiIoSession = LspIoLightSessionpGetNextTdiIoSession(LspIoLightSession);
				ASSERT(NULL != tdiIoSession);

				xTdiIoSessionStartTimer(
					tdiIoSession, 
					LspIoLightSession->SendTimeout);

				dataBuffer = lsp_get_buffer_to_send(
					LspIoLightSession->LspHandle, 
					&dataBufferLength);

				pendingTdiIo = InterlockedIncrement(&LspIoLightSession->PendingTdiIo);
				ASSERT(pendingTdiIo > 1);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					"LspSend[%d]: tdiSession=%p, bytes=%d\n", 
					pendingTdiIo, tdiIoSession, dataBufferLength);

#if DBG
				tdiIoSession->StartCounter = KeQueryPerformanceCounter(NULL);
#endif
				status = xTdiSendEx(
					tdiIoSession->Irp,
					LspIoLightSession->ConnectionDeviceObject,
					LspIoLightSession->ConnectionFileObject,
					dataBuffer,
					dataBufferLength,
					&bytesTransferred,
					0,
					&tdiIoSession->Overlapped);

				if (!NT_SUCCESS(status))
				{
					//
					// completion routine will be invoked regardless
					// of the error status
					//
					NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
						"Send: ***** xTdiSend failed, status=%x\n", status);
				}
			}
			break;
		case LSP_REQUIRES_RECEIVE:
			{
				PXTDI_IO_SESSION tdiIoSession;
				PVOID dataBuffer;
				ULONG bytesTransferred;
				UINT32 dataBufferLength;

				tdiIoSession = LspIoLightSessionpGetNextTdiIoSession(LspIoLightSession);
				ASSERT(NULL != tdiIoSession);

				xTdiIoSessionStartTimer(
					tdiIoSession, 
					LspIoLightSession->ReceiveTimeout);

				dataBuffer = lsp_get_buffer_to_receive(
					LspIoLightSession->LspHandle, 
					&dataBufferLength);

				pendingTdiIo = InterlockedIncrement(&LspIoLightSession->PendingTdiIo);
				ASSERT(pendingTdiIo > 1);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					"LspReceive[%d]: tdiSession=%p, bytes=%d\n", 
					pendingTdiIo, tdiIoSession, dataBufferLength);

				status = xTdiReceiveEx(
					tdiIoSession->Irp,
					LspIoLightSession->ConnectionDeviceObject,
					LspIoLightSession->ConnectionFileObject,
					dataBuffer,
					dataBufferLength,
					&bytesTransferred,
					0,
					&tdiIoSession->Overlapped);

				if (!NT_SUCCESS(status))
				{
					//
					// completion routine will be invoked regardless
					// of the error status
					//
					NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
						"Receive: ***** xTdiReceiveEx failed, status=%x\n", status);
				}
			}
			break;
		case LSP_REQUIRES_SYNCHRONIZE:
			{
				pendingTdiIo = InterlockedDecrement(&LspIoLightSession->PendingTdiIo);
				ASSERT(pendingTdiIo >= 0);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					"LspSynchronize[%d]: LspIoLightSession=%p\n", 
					pendingTdiIo, LspIoLightSession);

				if (pendingTdiIo > 0)
				{
					//
					// Completion routines are not called yet
					// Last-called completion routine will proceed to
					// the next step instead.
					//
					return STATUS_PENDING;
				}

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
					"SynchronizedCompletion: LspIoLightSession=%p, TdiIoCount=%d\n", 
					LspIoLightSession, pendingTdiIo);

				if (LspIoLightSessionpErrorInTdiIo(LspIoLightSession))
				{
					IO_STATUS_BLOCK completeIoStatus;
					LspIoLightSessionpResetTdiIoContext(LspIoLightSession);

					//
					// We do not use TDI status directly.
					// Use STATUS_UNEXPECTED_NETWORK_ERROR for all errors.
					//

					completeIoStatus.Status = STATUS_UNEXPECTED_NETWORK_ERROR;
					completeIoStatus.Information = 0;

					LspIoLightSessionpCompleteLspRequest(
						LspIoLightSession, &completeIoStatus);

					return STATUS_UNEXPECTED_NETWORK_ERROR;
				}

				LspIoLightSessionpResetTdiIoContext(LspIoLightSession);

				//
				// Reset the transfer count to 1
				//
				pendingTdiIo = InterlockedIncrement(&LspIoLightSession->PendingTdiIo);
				ASSERT(1 == pendingTdiIo);
			}
			break;
		case LSP_REQUIRES_DATA_ENCODE:
			{
#ifdef LSPIO_IMP_USE_EXTERNAL_ENCODE
				lsp_encrypt_send_data(
					LspIoLightSession->LspHandle,
					LspIoLightSession->InternalBuffer,
					LspIoLightSession->OriginalBuffer,
					LspIoLightSession->OriginalBufferLength);
#else
				ASSERTMSG("We do not use external encode, "
					"we should never get LSP_REQUIRES_DATA_ENCODE", FALSE);
#endif
			}
			break;
		default:
			{
				IO_STATUS_BLOCK completeIoStatus;

				pendingTdiIo = InterlockedDecrement(&LspIoLightSession->PendingTdiIo);
				ASSERT(0 == pendingTdiIo);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
					"LspComplete[%d]: LspIoLightSession=%p, LspStatus=0x%x\n",
					pendingTdiIo,
					LspIoLightSession,
					LspIoLightSession->LspStatus);

				status = (LspIoLightSession->LspStatus == LSP_STATUS_SUCCESS) ?
					STATUS_SUCCESS : IO_ERR_LSP_FAILURE;

				//
				// Complete the LSP IO request
				//
				completeIoStatus.Status = status;
				completeIoStatus.Information = 0;

				LspIoLightSessionpCompleteLspRequest(
					LspIoLightSession, &completeIoStatus);

				return status;
			}
		}

		LspIoLightSession->LspStatus = lsp_process_next(
			LspIoLightSession->LspHandle);
	}
}

FORCEINLINE
VOID
LSPIOCALL
LspIoLightSessionpCompleteLspRequest(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in PIO_STATUS_BLOCK IoStatus)
{
	lsp_request_packet_t* LspRequestPacket;
	PLSP_IO_LIGHT_COMPLETION completionRoutine;
	PVOID completionContext;

	ASSERT(0 == LspIoLightSession->PendingTdiIo);
	ASSERT(NULL != LspIoLightSession->CompletionRoutine);

	LspRequestPacket = LspIoLightSession->ActiveLspRequest;

	if (NT_SUCCESS(IoStatus->Status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
			"LspIoRequest completed, LspIoLightSession=%p, status=0x%x\n",
			LspIoLightSession, IoStatus->Status);
	}
	else
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"LspIoRequest failed, type=%d, lspStatus=0x%x, LspIoLightSession=%p, status=0x%x\n",
			LspRequestPacket->type,
			LspRequestPacket->status,
			LspIoLightSession, 
			IoStatus->Status);
	}

	completionRoutine = LspIoLightSession->CompletionRoutine;
	completionContext = LspIoLightSession->CompletionContext;

	LspIoLightSession->CompletionRoutine = NULL;
	LspIoLightSession->CompletionContext = NULL;
	LspIoLightSession->ActiveLspRequest = NULL;

	ASSERT(pTestFlag(LspIoLightSession->SessionFlags, LSP_IO_LIGHT_SESSION_OP_IN_PROGRESS));
	pClearFlag(&LspIoLightSession->SessionFlags, LSP_IO_LIGHT_SESSION_OP_IN_PROGRESS);

	InterlockedIncrement(&LspIoLightSession->RequestSequence);

	(*completionRoutine)(LspIoLightSession, LspRequestPacket, IoStatus, completionContext);
}

FORCEINLINE
PXTDI_IO_SESSION
LspIoLightSessionpGetNextTdiIoSession(
	PLSP_IO_LIGHT_SESSION LspIoLightSession)
{
	LONG i;
	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		PXTDI_IO_SESSION tdiIoSession = &LspIoLightSession->TdiIoSession[i];
		if (FALSE == xTdiIoSessionMarkInUseBit(tdiIoSession))
		{
			return tdiIoSession;
		}
	}
	ASSERTMSG("Too many concurrent TDI transfers...", FALSE);
	return NULL;
}

VOID
LSPIOCALL
LspIoLightSessionCancelPendingIrps(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession)
{
	ULONG i;
	for (i = 0; i < countof(LspIoLightSession->TdiIoSession); ++i)
	{
		PXTDI_IO_SESSION TdiIoSession = &LspIoLightSession->TdiIoSession[i];
		if (xTdiIoSessionIsInUseBitSet(TdiIoSession))
		{
			BOOLEAN canceled = IoCancelIrp(TdiIoSession->Irp);
			if (!canceled)
			{
				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
					"CancelIrp failed, irp=%p\n", TdiIoSession->Irp);
			}
		}
	}
}
