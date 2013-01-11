#include <ndasport.h>
#include "trace.h"
#include "utils.h"
#include <socketlpx.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include "lspio.h"
#include <xtdi.h>
#include <xtdilpx.h>

#ifdef RUN_WPP
#include "lspio.tmh"
#endif

// #define LSPIO_IMP_USE_EXTERNAL_ENCODE

#define LSP_IO_SESSION_TAG 'Spsl'
#define LSP_IO_DATA_TAG    'Dpsl'
#define LSP_IO_TDI_IRP_TAG 'Ipsl'

typedef enum _LSP_IOS_RESOURCE_FLAG {
	LSP_IOS_RES_ADDRESS_CREATED     = 0x00000001,
	LSP_IOS_RES_CONNECTION_CREATED  = 0x00000002,
	LSP_IOS_RES_ADDRESS_ASSOCIATED  = 0x00000004,
	LSP_IOS_RES_CONNECTED           = 0x00000008,
	LSP_IOS_RES_LOGGED_IN           = 0x00000010
} LSP_IOS_RESOURCE_FLAG;

#define LSPIO_DEFAULT_RECEIVE_TIMEOUT_SEC 30
#define LSPIO_DEFAULT_SEND_TIMEOUT_SEC 30

#define LSP_TDI_DATA_IN_USE 0x00000001

typedef enum _LSP_IOS_STATE {
	LSP_IOS_STATE_NONE,
	LSP_IOS_STATE_ALLOCATED,
	LSP_IOS_STATE_INITIALIZED,
	LSP_IOS_STATE_CONNECTION_FILE_CREATED,
	LSP_IOS_STATE_ADDRESS_FILE_CREATED,
	LSP_IOS_STATE_CONNECTING,
	LSP_IOS_STATE_CONNECTED,
	LSP_IOS_STATE_LOGGING_IN,
	LSP_IOS_STATE_READY,
	LSP_IOS_STATE_EXECUTING,
	LSP_IOS_STATE_LOGGING_OUT,
	LSP_IOS_STATE_DISCONNECTING,
	LSP_IOS_STATE_DEALLOCATED
} LSP_IOS_STATE;

//
// Get statistics about protocol operation. 
// Currently get information about packet retransmission or loss during operation.
// Should be synchronized with TRANS_STAT in socketlpx.h
//
typedef struct _LPX_TDI_PERF_COUNTER {
	ULONG RetransmitCounter;
	ULONG PacketLossCounter;
} LPX_TDI_PERF_COUNTER, *PLPX_TDI_PERF_COUNTER;

typedef struct _TDI_IO_CONTEXT {
	PLSP_IO_SESSION LspIoSession;
	PIRP Irp;
	XTDI_OVERLAPPED Overlapped;
	ULONG Flags;
#if DBG
	LARGE_INTEGER StartCounter;
#endif
} TDI_IO_CONTEXT, *PTDI_IO_CONTEXT;

typedef struct _LSP_SYNCHRONOUS_IO_CONTEXT {
	KEVENT IoCompletionEvent;
	IO_STATUS_BLOCK IoStatus;
} LSP_SYNCHRONOUS_IO_CONTEXT, *PLSP_SYNCHRONOUS_IO_CONTEXT;

typedef NTSTATUS (LSPIOCALL *PLSP_IO_SINGLE_IO_ROUTINE)(
	PLSP_IO_SESSION LspIoSession,
	PVOID Buffer,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext);

typedef struct _LSP_MULTIPLE_IO_CONTEXT {
	PLSP_IO_SINGLE_IO_ROUTINE IoRoutine;
	PUCHAR NextBuffer;
	LARGE_INTEGER NextLogicalBlockAddress;
	ULONG NextTransferLength;
	IO_STATUS_BLOCK IoStatus;
} LSP_MULTIPLE_IO_CONTEXT, *PLSP_MULTIPLE_IO_CONTEXT;

typedef struct _LSP_VERIFIED_WRITE_CONTEXT {
	PUCHAR OriginalBuffer;
	LARGE_INTEGER LogicalBlockAddress;
	ULONG TransferLength;
} LSP_VERIFIED_WRITE_CONTEXT, *PLSP_VERIFIED_WRITE_CONTEXT;

typedef struct _LSP_IO_SESSION_PERF_COUNTER {
	LARGE_INTEGER PerformanceCounterValueOnStart;
	LARGE_INTEGER LspTransactionCount;
	LARGE_INTEGER LspTdiIoCount;
	LARGE_INTEGER PduBytesSent;
	LARGE_INTEGER PduBytesReceived;
	LARGE_INTEGER DataBytesSent;
	LARGE_INTEGER DataBytesReceived;
	LARGE_INTEGER NonDataBytesSent;
	LARGE_INTEGER NonDataBytesReceived;
} LSP_IO_SESSION_PERF_COUNTER, *PLSP_IO_SESSION_PERF_COUNTER;

typedef struct _LSP_IO_SESSION {

	lsp_handle_t LspHandle;
	lsp_status_t LspStatus;
	lsp_login_info_t LspLoginInfo;

	LSP_TRANSPORT_ADDRESS LocalAddress;
	LSP_TRANSPORT_ADDRESS DeviceAddress;

	PDEVICE_OBJECT DeviceObject;

	HANDLE AddressHandle;
	PFILE_OBJECT AddressFileObject;
	PDEVICE_OBJECT AddressDeviceObject;

	HANDLE ConnectionHandle;
	PFILE_OBJECT ConnectionFileObject;
	PDEVICE_OBJECT ConnectionDeviceObject;

	ULONG LspIoResourceFlags;
	LSP_IOS_STATE PreviousState;
	LSP_IOS_STATE SessionState;
	LSP_IOS_STATE PendingState;

	LONG OutstandingTdiIO;

	ULONG MaximumTransferLength;
	ULONG CurrentMaximumTransferLength;

	KEVENT TdiTimeOutEvent;
	KEVENT TdiErrorEvent;

	IO_STATUS_BLOCK TdiStatus;
	//
	// Save the IO_STATUS for performance counter
	//
	IO_STATUS_BLOCK UnderlyingStatus;

	union {
		ULONG LspIoFlags;
		/* DEBUG ONLY, DO NOT USE THESE FIELDS */
		struct {
			BOOLEAN UseDMA : 1;
			BOOLEAN UseLBA : 1;
			BOOLEAN UseLBA48 : 1;
			BOOLEAN DirectWrite : 1;
			BOOLEAN LockedWrite : 1;
			BOOLEAN VerifyWrite : 1;
		} LspIoFlagBits;
	};

	TDI_IO_CONTEXT NonLspTdiIoContext;
	TDI_IO_CONTEXT LpxControlTdiIoContext;
	TDI_IO_CONTEXT LspTdiIoContext[4];

	LARGE_INTEGER ReceiveTimeOut;
	LARGE_INTEGER SendTimeOut;

	//
	// We have up to 4 internal completion stacks for
	// layered processing.
	//
	// [0] User Completion Routine / Synchronous IO Completion Routine
	// [1] Multiple IO Completion Routine (optional)
	// [2] Verified Write Completion Routine (optional)
	// [3] Reserved
	//
	struct {
		PLSP_IO_COMPLETION CompletionRoutine;
		PVOID CompletionContext;
	} LspIoCompletionStack[4];

	LONG LspIoCompletionStackNextIndex;

	LSP_SYNCHRONOUS_IO_CONTEXT SyncIoContext;
	LSP_MULTIPLE_IO_CONTEXT MultiIoContext;
	LSP_VERIFIED_WRITE_CONTEXT VerifiedWriteContext;

	ULONG LspIoCount;

	LPX_TDI_PERF_COUNTER LpxTdiPreviousPerfCounter;
	LPX_TDI_PERF_COUNTER LpxTdiCurrentPerfCounter;

	ULONG InternalBufferLength;

	//
	// Around 1 KB
	//
	PVOID LspSessionBuffer;

	//
	// Maximum 64 KB for buffered writing
	//
	PVOID InternalBuffer;
	PVOID OriginalBuffer;
	ULONG OriginalBufferLength;

	lsp_request_packet_t LspRequestPackets[4];

	PLSP_IO_REQUEST OriginalRequest;
	PLSP_IO_REQUEST CurrentRequest;
	LSP_IO_REQUEST InternalRequests[4];

} LSP_IO_SESSION, *PLSP_IO_SESSION;

//
//typedef enum _LSP_OPCODE {
//	LspAtaOpRead,
//	LspAtaOpWrite,
//	LspAtaOpVerify,
//	LspAtaOpIdentifyDevice,
//	LspAtaOpIdentifyPacketDevice,
//	LspAtaOpFlushCache,
//	LspAtaOpSmart,
//} LSP_OPCODE;
//
//typedef struct _LSP_REQUEST_BLOCK {
//	LSP_OPCODE OpCode;
//	PVOID Buffer;
//	LARGE_INTEGER LogicalBlockAddress;
//	ULONG TransferLength;
//	LSP_IDE_REGISTER IdeRegister;
//} LSP_REQUEST_BLOCK, *PLSP_REQUEST_BLOCK;
//

#define LSP_DATA_BUFFER_SIZE (64 * 1024) // 64 KB

ULONG LSP_IO_DEFAULT_INTERAL_BUFFER_SIZE = LSP_DATA_BUFFER_SIZE;

BOOLEAN
FORCEINLINE
LspIoTestStateFlag(PLSP_IO_SESSION LspIoSession, ULONG Bit)
{
	return TestFlag(LspIoSession->LspIoResourceFlags, Bit) ? TRUE : FALSE;
}

VOID
FORCEINLINE
LspIoSetStateFlag(PLSP_IO_SESSION LspIoSession, ULONG Bit)
{
	SetFlag(&LspIoSession->LspIoResourceFlags, Bit);
}

VOID
FORCEINLINE
LspIoClearStateFlag(PLSP_IO_SESSION LspIoSession, ULONG Bit)
{
	ClearFlag(&LspIoSession->LspIoResourceFlags, Bit);
}

VOID
FORCEINLINE
LspIoAddCompletionRoutine(
	PLSP_IO_SESSION LspIoSession,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	LONG i;

	i = LspIoSession->LspIoCompletionStackNextIndex;
	++LspIoSession->LspIoCompletionStackNextIndex;

	ASSERT(i >= 0 && i < countof(LspIoSession->LspIoCompletionStack));

	ASSERT(NULL == LspIoSession->LspIoCompletionStack[i].CompletionRoutine);
	ASSERT(NULL == LspIoSession->LspIoCompletionStack[i].CompletionContext);

	LspIoSession->LspIoCompletionStack[i].CompletionRoutine = CompletionRoutine;
	LspIoSession->LspIoCompletionStack[i].CompletionContext = CompletionContext;

}

PLSP_IO_COMPLETION
FORCEINLINE
LspIoPeekNextCompletionRoutine(
	PLSP_IO_SESSION LspIoSession)
{
	LONG i;
	PLSP_IO_COMPLETION CompletionRoutine;
	i = LspIoSession->LspIoCompletionStackNextIndex - 1;
	if (i < 0) 
	{
		ASSERT(FALSE);
		return NULL;
	}
	CompletionRoutine = LspIoSession->LspIoCompletionStack[i].CompletionRoutine;
	return CompletionRoutine;
}

NTSTATUS
FORCEINLINE
LspIoInvokeNextCompletionRoutine(
	PLSP_IO_SESSION LspIoSession,
	PIO_STATUS_BLOCK IoStatus)
{
	LONG i;
	PLSP_IO_COMPLETION CompletionRoutine;
	PVOID CompletionContext;
	
	i = (--LspIoSession->LspIoCompletionStackNextIndex);
	ASSERT(i >= 0 && i < countof(LspIoSession->LspIoCompletionStack));

	CompletionRoutine = LspIoSession->LspIoCompletionStack[i].CompletionRoutine;
	CompletionContext = LspIoSession->LspIoCompletionStack[i].CompletionContext;

	LspIoSession->LspIoCompletionStack[i].CompletionRoutine = NULL;
	LspIoSession->LspIoCompletionStack[i].CompletionContext = NULL;

	return (*CompletionRoutine)(LspIoSession, IoStatus, CompletionContext);
}

NTSTATUS
FORCEINLINE
TdiIoContextInitialize(
	__inout PTDI_IO_CONTEXT TdiData,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PLSP_IO_SESSION LspIoSession,
	__in PXTDI_IO_COMPLETION_ROUTINE CompletionRoutine);

PTDI_IO_CONTEXT
FORCEINLINE
LspIoLspTdiDataGetNext(
	__in PLSP_IO_SESSION LspIoSession);

VOID
LspIoTdiDataTdiCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LSPIOCALL
LspIoProcessNext(
	__in PLSP_IO_SESSION LspIoSession);

NTSTATUS
LspIoSynchronousIoCompletion(
	PLSP_IO_SESSION LspIoSession,
	PIO_STATUS_BLOCK IoStatus,
	PVOID Context)
{
	PLSP_SYNCHRONOUS_IO_CONTEXT SyncIoContext;
	
	SyncIoContext = (PLSP_SYNCHRONOUS_IO_CONTEXT) Context;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, Status=%x\n",
		LspIoSession, IoStatus->Status);

	RtlCopyMemory(
		&SyncIoContext->IoStatus,
		IoStatus,
		sizeof(IO_STATUS_BLOCK));

	KeSetEvent(
		&SyncIoContext->IoCompletionEvent, 
		IO_NO_INCREMENT, 
		FALSE);

	return STATUS_SUCCESS;
}

VOID
FORCEINLINE
LspIoSetCompletionRoutine(
	PLSP_IO_SESSION LspIoSession,
	PLSP_IO_COMPLETION LspIoCompletionRoutine,
	PVOID LspIoCompletionContext)
{
	if (NULL == LspIoCompletionRoutine)
	{
		LspIoAddCompletionRoutine(
			LspIoSession,
			LspIoSynchronousIoCompletion,
			&LspIoSession->SyncIoContext);

		KeClearEvent(&LspIoSession->SyncIoContext.IoCompletionEvent);
	}
	else
	{
		LspIoAddCompletionRoutine(
			LspIoSession,
			LspIoCompletionRoutine,
			LspIoCompletionContext);
	}
}

NTSTATUS
FORCEINLINE
LspIoWaitForCompletion(
	PLSP_IO_SESSION LspIoSession,
	NTSTATUS status)
{
	NTSTATUS status2;

	if (STATUS_PENDING != status)
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
			"Not pending, LspIoSession=%p, Status=%x\n",
			LspIoSession, status);
		return status;
	}

	if (KeGetCurrentIrql() > APC_LEVEL) 
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"EX: Waitable code called at IRQL %d\n", KeGetCurrentIrql());
		ASSERTMSG("EX: Waitable code called at IRQL higher than APC_LEVEL", FALSE);
	}

	ASSERT(STATUS_PENDING == status);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"Pending Completion, LspIoSession=%p\n",
		LspIoSession);

	status2 = KeWaitForSingleObject(
		&LspIoSession->SyncIoContext.IoCompletionEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	ASSERT(STATUS_SUCCESS == status2);

	status = LspIoSession->SyncIoContext.IoStatus.Status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"Completed, LspIoSession=%p, Status=%x\n",
		LspIoSession, status);

	return status;
}

PTDI_IO_CONTEXT
FORCEINLINE
LspIoSessionGetNextTdiIoContext(
	PLSP_IO_SESSION LspIoSession)
{
	LONG i;
	for (i = 0; i < countof(LspIoSession->LspTdiIoContext); ++i)
	{
		PTDI_IO_CONTEXT tdiIoContext = &LspIoSession->LspTdiIoContext[i];
		if (!InterlockedBitTestAndSet(&tdiIoContext->Flags,LSP_TDI_DATA_IN_USE))
		{
			return tdiIoContext;
		}
	}
	ASSERTMSG("Too many concurrent TDI transfers...", FALSE);
	return NULL;
}

VOID
FORCEINLINE
LspIoSetLpxPerfCounter(
	PTDI_IO_CONTEXT TdiData,
	PLSP_IO_SESSION LspIoSession)
{
	//
	// LPX does not make use of DriverContext anymore
	//
#if 0
#ifdef _WIN64
	// On x64, only DriverContext[0] is used for expiration time.
	// And DriverContext[2] is used by someone who is not identified yet.(To do..)
	// So use DriverContext[1] for time being.
#define LPX_TDI_PERF_COUNTER_DRIVER_CONTEXT_INDEX 1
#else
	// LPX uses DriverContext[0] and [1]  to store expiration time.
	// DriverContext[3] is used by someone else. To do: Find who is using this value.
#define LPX_TDI_PERF_COUNTER_DRIVER_CONTEXT_INDEX 2
#endif
	TdiData->Irp->Tail.Overlay.DriverContext[LPX_TDI_PERF_COUNTER_DRIVER_CONTEXT_INDEX] = 
		&LspIoSession->LpxTdiPerfCounter;
#endif
}

VOID
FORCEINLINE
LspIoSessionResetLpxPerfCounter(
	PLSP_IO_SESSION LspIoSession)
{
	RtlZeroMemory(
		&LspIoSession->LpxTdiPreviousPerfCounter,
		sizeof(LPX_TDI_PERF_COUNTER));

	RtlZeroMemory(
		&LspIoSession->LpxTdiCurrentPerfCounter,
		sizeof(LPX_TDI_PERF_COUNTER));
}

NTSTATUS
FORCEINLINE
TdiIoContextInitialize(
	__inout PTDI_IO_CONTEXT TdiData, 
	__in PDEVICE_OBJECT ConnectionDeviceObject, 
	__in PLSP_IO_SESSION LspIoSession,
	__in PXTDI_IO_COMPLETION_ROUTINE CompletionRoutine)
{
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	RtlZeroMemory(TdiData, sizeof(TDI_IO_CONTEXT));

	TdiData->LspIoSession = LspIoSession;

	TdiData->Irp = IoAllocateIrp(
		ConnectionDeviceObject->StackSize + 1,
		FALSE);

	if (NULL == TdiData->Irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (LspOverLpxStream == LspIoSession->DeviceAddress.Type)
	{
		LspIoSetLpxPerfCounter(TdiData, LspIoSession);
	}

	TdiData->Overlapped.CompletionRoutine = CompletionRoutine;
	TdiData->Overlapped.UserContext = TdiData;

	return STATUS_SUCCESS;
}

VOID
TdiIoContextCleanup(
	__in PTDI_IO_CONTEXT TdiIoContext)
{
	if (TdiIoContext->Irp)
	{
		IoFreeIrp(TdiIoContext->Irp);
#if DBG
		TdiIoContext->Irp = NULL;
#endif
	}
}

//
// This function must be called without any outstanding IO's
//
VOID
FORCEINLINE
TdiIoContextReuse(
	__inout PTDI_IO_CONTEXT TdiIoContext)
{
	ASSERT(NULL != TdiIoContext->Irp);
	IoReuseIrp(TdiIoContext->Irp, STATUS_SUCCESS);

	ASSERT(NULL == TdiIoContext->Overlapped.Event);
	TdiIoContext->Overlapped.Internal = 0;
	TdiIoContext->Overlapped.InternalBuffer = 0;
	TdiIoContext->Flags = 0;
	RtlZeroMemory(
		&TdiIoContext->Overlapped.IoStatus, 
		sizeof(IO_STATUS_BLOCK));
}

VOID
FORCEINLINE
LspIoSessionReuseTdiIoContext(
	__inout PTDI_IO_CONTEXT LspTdiIoContext)
{
	//NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
	//	("LspIoLspTdiDataReuse: "
	//	"LspIoSession=%p, LspTdiData=%p, TdiIrp=%p\n",
	//	TdiData->LspIoSession, TdiData, TdiData->Irp);

	TdiIoContextReuse(LspTdiIoContext);
}

VOID
FORCEINLINE
LspIoSessionResetTdiIoContext(
	__in PLSP_IO_SESSION LspIoSession)
{
	LONG i;
	ASSERT(0 == LspIoSession->OutstandingTdiIO);
	for (i = 0; i < countof(LspIoSession->LspTdiIoContext); ++i)
	{
		if (0 == LspIoSession->LspTdiIoContext[i].Flags) 
		{
			break;
		}
		LspIoSessionReuseTdiIoContext(
			&LspIoSession->LspTdiIoContext[i]);
	}
}

NTSTATUS
LspIoClientEventDisconnect(
	__in PVOID TdiEventContext,
	__in CONNECTION_CONTEXT ConnectionContext,
	__in LONG DisconnectDataLength,
	__in PVOID DisconnectData,
	__in LONG DisconnectInformationLength,
	__in PVOID DisconnectInformation,
	__in ULONG DisconnectFlags)
{
	PLSP_IO_SESSION LspIoSession = (PLSP_IO_SESSION) TdiEventContext;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"*** LspIoClientEventDisconnect: LspIoSession=%p\n", 
		LspIoSession);

	DebugPrint((1, "*** LspIoClientEventDisconnect: LspIoSession=%p\n", LspIoSession));

	KeSetEvent(&LspIoSession->TdiErrorEvent, IO_NO_INCREMENT, FALSE);

	return STATUS_SUCCESS;
}

VOID
LspIoTdiNonLspCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	PTDI_IO_CONTEXT TdiData = (PTDI_IO_CONTEXT) Overlapped->UserContext;
	PLSP_IO_SESSION LspIoSession = (PLSP_IO_SESSION) TdiData->LspIoSession;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"TransferIrp=%p, Overlapped=%p, Status=%x\n", 
		TransferIrp, Overlapped, Overlapped->IoStatus.Status);

	if (NT_SUCCESS(Overlapped->IoStatus.Status))
	{
		LspIoSession->SessionState = LspIoSession->PendingState;
	}
	else
	{
		LspIoSession->SessionState = LspIoSession->PreviousState;
	}

	LspIoInvokeNextCompletionRoutine(LspIoSession, &Overlapped->IoStatus);
}

VOID
FORCEINLINE
LspIoHandleTdiError(
	__in PLSP_IO_SESSION LspIoSession)
{
	BOOLEAN requireResetConnection;

	requireResetConnection = IsDispatchObjectSignaled(&LspIoSession->TdiTimeOutEvent);
}

VOID
FORCEINLINE
LspIoLogLspError(
	__in PLSP_IO_SESSION LspIoSession)
{
	LSP_IDE_REGISTER ideRegister = {0};

	LspIoGetLastIdeOutputRegister(LspIoSession, &ideRegister);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"Error LspIoSession=%p, LspStatus=0x%x "
		"IdeRegister(ER,SC,LL,LM,LH,DE,CM)=(%02X %02X %02X %02X %02X %02X %02X)\n",
		LspIoSession, 
		LspIoSession->LspStatus,
		ideRegister.reg.named.features,
		ideRegister.reg.named.sector_count,
		ideRegister.reg.named.lba_low,
		ideRegister.reg.named.lba_mid,
		ideRegister.reg.named.lba_high,
		ideRegister.device.device,
		ideRegister.command.command);
}

VOID
LspIoTdiPerfCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	LONG outstandingIO;
	PTDI_IO_CONTEXT TdiData = (PTDI_IO_CONTEXT) Overlapped->UserContext;
	PLSP_IO_SESSION LspIoSession = (PLSP_IO_SESSION) TdiData->LspIoSession;
	PLPX_TDI_PERF_COUNTER previousPC, currentPC;
	
	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LSPIO: LspIoTdiPerfCompletion, TransferIrp=%p, Overlapped=%p, Status=%x\n", 
		TransferIrp, Overlapped, Overlapped->IoStatus.Status);

	outstandingIO = InterlockedDecrement(&LspIoSession->OutstandingTdiIO);
	ASSERT(0 == outstandingIO);

	//
	// We do not care about the performance query completion status
	// but the underlying actual status
	//
	if (NT_SUCCESS(LspIoSession->UnderlyingStatus.Status))
	{
		LspIoSession->SessionState = LspIoSession->PendingState;
	}
	else
	{
		LspIoSession->SessionState = LspIoSession->PreviousState;
	}

	currentPC = &LspIoSession->LpxTdiCurrentPerfCounter;
	previousPC = &LspIoSession->LpxTdiPreviousPerfCounter;

	previousPC->PacketLossCounter += currentPC->PacketLossCounter;
	previousPC->RetransmitCounter += currentPC->RetransmitCounter;

	if (currentPC->PacketLossCounter != 0)
	{
		NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_WARNING,
			"  [%d] Packet Loss: Current=%d, Total=%d, MaxTransfer=0x%x bytes\n",
			LspIoSession->LspIoCount,
			currentPC->PacketLossCounter,
			previousPC->PacketLossCounter,
			LspIoSession->CurrentMaximumTransferLength);

	}
	if (currentPC->RetransmitCounter != 0)
	{
		NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_WARNING,
			"  [%d] Retransmit: Current=%d, Total=%d, MaxTransfer=0x%x bytes\n",
			LspIoSession->LspIoCount,
			currentPC->RetransmitCounter,
			previousPC->RetransmitCounter,
			LspIoSession->CurrentMaximumTransferLength);
	}

	if (currentPC->PacketLossCounter > 0 ||
		currentPC->RetransmitCounter > 0)
	{
		if (LspIoSession->CurrentMaximumTransferLength > 512)
		{
			LspIoSession->CurrentMaximumTransferLength /= 2;
		}
	}

	NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_INFORMATION,
		"PacketLoss=(%d->%d), Retransmit=(%d->%d)\n",
		previousPC->PacketLossCounter,
		currentPC->PacketLossCounter,
		previousPC->RetransmitCounter,
		currentPC->RetransmitCounter);

	LspIoInvokeNextCompletionRoutine(
		LspIoSession, 
		&LspIoSession->UnderlyingStatus);
}

NTSTATUS
LspIoQueryPerformanceData(
	__in PLSP_IO_SESSION LspIoSession,
	__in PIO_STATUS_BLOCK UnderlyingIoStatus)
{
	NTSTATUS status;
	ULONG outstandingIO;
	PTDI_IO_CONTEXT tdiIoContext;

	tdiIoContext = &LspIoSession->LpxControlTdiIoContext;
	TdiIoContextReuse(tdiIoContext);

	outstandingIO = InterlockedIncrement(&LspIoSession->OutstandingTdiIO);
	ASSERT(outstandingIO >= 1);

	status = xTdiQueryInformationEx(
		tdiIoContext->Irp,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject,
		LPXTDI_QUERY_CONNECTION_TRANSSTAT,
		&LspIoSession->LpxTdiCurrentPerfCounter,
		sizeof(LPX_TDI_PERF_COUNTER),
		&tdiIoContext->Overlapped);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"LPXTDI_QUERY_CONNECTION_TRANSSTAT failed, status=%x\n",
			status);

		//
		// completion routine will be invoked at any case
		//
	}

	return status;
}

VOID
LSPIOCALL
LspIoCompleteIoRequest(
	__in PLSP_IO_SESSION LspIoSession,
	__in NTSTATUS Status)
{
	ASSERT(0 == LspIoSession->OutstandingTdiIO);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, Status=0x%x\n",
		LspIoSession, Status);

	++(LspIoSession->LspIoCount);
	LspIoSession->UnderlyingStatus.Status = Status;
	LspIoSession->UnderlyingStatus.Information = 0;

	LspIoQueryPerformanceData(LspIoSession, &LspIoSession->UnderlyingStatus);
}

//
// Called when there is no outstanding IO left to proceed to the next
// step in lsp process. LspIoSession->OutstandingTdiIO must be
// zero when this function is called.
//
NTSTATUS
LspIoTdiDataTdiPostCompletion(
	__in PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;
	PIO_STATUS_BLOCK ioStatus;
	ULONG i;

	//
	// If there are any TDI errors in the previous transfers,
	// halt the entire LSP IO.
	//

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"Completing Transfers LspIoSession=%p\n", 
		LspIoSession);

	ASSERT(0 == LspIoSession->OutstandingTdiIO);

	for (i = 0; i < countof(LspIoSession->LspTdiIoContext); ++i)
	{

		if (0 == LspIoSession->LspTdiIoContext[i].Flags)
		{
			//
			// No errors are found
			//
			break;
		}

		ioStatus = &LspIoSession->LspTdiIoContext[i].Overlapped.IoStatus;

		if (!NT_SUCCESS(ioStatus->Status))
		{
			NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
				"TDI function[%d] failed, status=%x\n", i, ioStatus->Status);

			//
			// Save the last TDI error
			//
			LspIoSession->TdiStatus = *ioStatus;

			//
			// Clear all TDI data for later use
			//
			LspIoSessionResetTdiIoContext(LspIoSession);

			//
			// We do not use TDI status directly.
			// Use STATUS_UNEXPECTED_NETWORK_ERROR for all errors.
			//

			status = STATUS_UNEXPECTED_NETWORK_ERROR;
			LspIoCompleteIoRequest(LspIoSession, status);

			return status;
		}
	}

	LspIoSessionResetTdiIoContext(LspIoSession);

	LspIoSession->LspStatus = lsp_process_next(LspIoSession->LspHandle);
	status = LspIoProcessNext(LspIoSession);

	return status;
}

VOID
LspIoTdiDataTdiCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PTDI_IO_CONTEXT TdiData = (PTDI_IO_CONTEXT) Overlapped->UserContext;
	PLSP_IO_SESSION LspIoSession = (PLSP_IO_SESSION) TdiData->LspIoSession;
	LONG outstandingIO;

	UNREFERENCED_PARAMETER(TransferIrp);

	outstandingIO = InterlockedDecrement(&LspIoSession->OutstandingTdiIO);
	ASSERT(outstandingIO >= 0);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"TransferIrp=%p, Overlapped=%p, Status=%x, TdiIoCount=%d\n", 
		TransferIrp, Overlapped, Overlapped->IoStatus.Status, outstandingIO);

	if (0 == outstandingIO)
	{
		LspIoTdiDataTdiPostCompletion(LspIoSession);
	}
}

ULONG
FORCEINLINE
LspIoGetTdiDataIndex(
	__in PLSP_IO_SESSION LspIoSession, 
	__in PTDI_IO_CONTEXT TdiData)
{
	ASSERT(TdiData < &LspIoSession->LspTdiIoContext[countof(LspIoSession->LspTdiIoContext)]);
	ASSERT(TdiData >= &LspIoSession->LspTdiIoContext[0]);
	return (ULONG)(((ULONG_PTR)TdiData) - ((ULONG_PTR)&LspIoSession->LspTdiIoContext[0])) / sizeof(TDI_IO_CONTEXT);
}

//
// LspIoRead/Write -> LspIoProcessNext or
// LspIoTdiDataTdiCompletion -> LspIoProcessNext
//
NTSTATUS
LSPIOCALL
LspIoProcessNext(
	PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status, status2;
	LONG outstandingIO;

	//
	// Initial Count is 1.
	// When this counter reaches 0, it indicates that all outstanding 
	// TDI IO requests are completed.
	//
	//NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
	//	("LspIoSession=%p\n", LspIoSession);

	ASSERT(0 == LspIoSession->OutstandingTdiIO);
	outstandingIO = InterlockedIncrement(&LspIoSession->OutstandingTdiIO);
	ASSERT(1 == outstandingIO);

	while (TRUE)
	{
		switch (LspIoSession->LspStatus)
		{
		case LSP_REQUIRES_SEND:
			{
				PTDI_IO_CONTEXT tdiIoContext;
				PVOID dataBuffer;
				ULONG bytesTransferred;
				UINT32 dataBufferLength;
				
				tdiIoContext = LspIoSessionGetNextTdiIoContext(LspIoSession);

				dataBuffer = lsp_get_buffer_to_send(
					LspIoSession->LspHandle, 
					&dataBufferLength);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					"Send[%d]: %d bytes (within %I64d us)\n", 
					LspIoGetTdiDataIndex(LspIoSession, tdiIoContext),
					dataBufferLength, 
					LspIoSession->SendTimeOut.QuadPart / 10);

				outstandingIO = InterlockedIncrement(&LspIoSession->OutstandingTdiIO);
				ASSERT(outstandingIO > 1);

#if DBG
				tdiIoContext->StartCounter = KeQueryPerformanceCounter(NULL);
#endif
				status = xTdiSendEx(
					tdiIoContext->Irp,
					LspIoSession->ConnectionDeviceObject,
					LspIoSession->ConnectionFileObject,
					dataBuffer,
					dataBufferLength,
					&bytesTransferred,
					0,
					&tdiIoContext->Overlapped);

				if (!NT_SUCCESS(status))
				{
					//
					// completion routine will be invoked regardless
					// of the error status
					//
					NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
						"Send: ***** xTdiSend failed, status=%x\n", status);
				}

				outstandingIO = InterlockedDecrement(&LspIoSession->OutstandingTdiIO);
				ASSERT(outstandingIO >= 0);

				if (outstandingIO > 0)
				{
					return STATUS_PENDING;
				}
				
				return LspIoTdiDataTdiPostCompletion(LspIoSession);
			}
		case LSP_REQUIRES_RECEIVE:
			{
				PTDI_IO_CONTEXT tdiIoContext;
				PVOID dataBuffer;
				ULONG bytesTransferred;
				UINT32 dataBufferLength;

				tdiIoContext = LspIoSessionGetNextTdiIoContext(LspIoSession);

				dataBuffer = lsp_get_buffer_to_receive(
					LspIoSession->LspHandle, 
					&dataBufferLength);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					"Receive[%d]: %d bytes (within %I64d us)\n", 
					LspIoGetTdiDataIndex(LspIoSession, tdiIoContext),
					dataBufferLength, 
					LspIoSession->SendTimeOut.QuadPart / 10);

				outstandingIO = InterlockedIncrement(&LspIoSession->OutstandingTdiIO);
				ASSERT(outstandingIO > 1);

				status = xTdiReceiveEx(
					tdiIoContext->Irp,
					LspIoSession->ConnectionDeviceObject,
					LspIoSession->ConnectionFileObject,
					dataBuffer,
					dataBufferLength,
					&bytesTransferred,
					0,
					&tdiIoContext->Overlapped);

				if (!NT_SUCCESS(status))
				{
					//
					// completion routine will be invoked regardless
					// of the error status
					//
					NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
						"Receive: ***** xTdiReceiveEx failed, status=%x\n", status);
				}

				outstandingIO = InterlockedDecrement(&LspIoSession->OutstandingTdiIO);
				ASSERT(outstandingIO >= 0);

				if (outstandingIO > 0)
				{
					return STATUS_PENDING;
				}

				return LspIoTdiDataTdiPostCompletion(LspIoSession);
			}
		case LSP_REQUIRES_SYNCHRONIZE:
			{
				outstandingIO = InterlockedDecrement(&LspIoSession->OutstandingTdiIO);
				ASSERT(outstandingIO >= 0);

				if (outstandingIO > 0)
				{
					//NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					//	("----------- LspSynchronize (Pending) -----------\n");

					//
					// Completion routines are not called yet
					// Last-called completion routine will proceed to
					// the next step instead.
					//
					return STATUS_PENDING;
				}
				else
				{
					//NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
					//	("----------- LspSynchronize (Completed) -----------\n");

					NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
						"SynchronizedCompletion: LspIoSession=%p, TdiIoCount=%d\n", 
						LspIoSession, outstandingIO);

					//
					// All completion routines are already called.
					// Proceed to the next step.
					// Otherwise, 
					//
					LspIoSessionResetTdiIoContext(LspIoSession);

					//
					// Reset the transfer count to 1
					//
					outstandingIO = InterlockedIncrement(&LspIoSession->OutstandingTdiIO);
					ASSERT(1 == outstandingIO);
				}
			}
			break;
		case LSP_REQUIRES_DATA_ENCODE:
			{
#ifdef LSPIO_IMP_USE_EXTERNAL_ENCODE
				lsp_encrypt_send_data(
					LspIoSession->LspHandle,
					LspIoSession->InternalBuffer,
					LspIoSession->OriginalBuffer,
					LspIoSession->OriginalBufferLength);
#else
				ASSERTMSG("We do not use external encode, "
					"we should never get LSP_REQUIRES_DATA_ENCODE", FALSE);
#endif
			}
			break;
		default:
			{
				outstandingIO = InterlockedDecrement(&LspIoSession->OutstandingTdiIO);

				ASSERT(0 == outstandingIO);

				//NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
				//	("----------- Lsp Status=0x%x(Completed) -----------\n",
				//	LspIoSession->LspStatus);

				NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
					"LspCompletion: LspIoSession=%p, LspStatus=0x%x\n",
					LspIoSession,
					LspIoSession->LspStatus);

				status = (LspIoSession->LspStatus == LSP_STATUS_SUCCESS) ?
					STATUS_SUCCESS : IO_ERR_LSP_FAILURE;

				if (STATUS_SUCCESS == status)
				{
					if (LSP_IOS_STATE_LOGGING_IN == LspIoSession->SessionState)
					{
						lsp_status_t lspStatus;
						lspStatus = lsp_get_handle_info(
							LspIoSession->LspHandle,
							LSP_PROP_HW_MAX_TRANSFER_BLOCKS,
							&LspIoSession->MaximumTransferLength,
							sizeof(LspIoSession->MaximumTransferLength));
						
						ASSERT(LSP_STATUS_SUCCESS == lspStatus);
						
						/* convert it to bytes */
						LspIoSession->MaximumTransferLength *= 512;

						ASSERT(
							LspIoSession->MaximumTransferLength <=
							LspIoSession->InternalBufferLength);

						LspIoSession->CurrentMaximumTransferLength = 
							LspIoSession->MaximumTransferLength;

						ASSERT(LSP_STATUS_SUCCESS == lspStatus);
					}
				}
				else
				{
					ASSERT(IO_ERR_LSP_FAILURE == status);
					LspIoLogLspError(LspIoSession);
				}

				//
				// Complete the LSP IO request
				//
				LspIoCompleteIoRequest(LspIoSession, status);

				return status;
			}
		}

		LspIoSession->LspStatus = lsp_process_next(LspIoSession->LspHandle);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Session Allocation Functions
//
//////////////////////////////////////////////////////////////////////////

PVOID
FORCEINLINE
LspIoOffsetOf(PVOID Buffer, ULONG Length)
{
	return ((PUCHAR)Buffer) + Length;
}

LSP_STATUS
LSPIOCALL
LspIoGetLastLspStatus(
	__in PLSP_IO_SESSION LspIoSession)
{
	return LspIoSession->LspStatus;
}

NTSTATUS
LSPIOCALL
LspIoGetLastTdiError(
	__in PLSP_IO_SESSION LspIoSession)
{
	return LspIoSession->TdiStatus.Status;
}

VOID
LSPIOCALL
LspIoSetReceiveTimeout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLARGE_INTEGER DueTime)
{
	LspIoSession->ReceiveTimeOut = *DueTime;
}

VOID
LSPIOCALL
LspIoSetSendTimeout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLARGE_INTEGER DueTime)
{
	LspIoSession->SendTimeOut = *DueTime;
}

LARGE_INTEGER
LSPIOCALL
LspIoGetReceiveTimeout(
	__in PLSP_IO_SESSION LspIoSession)
{
	return LspIoSession->ReceiveTimeOut;
}

LARGE_INTEGER
LSPIOCALL
LspIoGetSendTimeout(
	__in PLSP_IO_SESSION LspIoSession)
{
	return LspIoSession->SendTimeOut;
}

NTSTATUS
LSPIOCALL
LspIoAllocateSession(
	__in PDEVICE_OBJECT DeviceObject,
	__in ULONG InternalDataBufferLength,
	__out PLSP_IO_SESSION* LspIoSession)
{
	ULONG allocateLength;

	ASSERT(NULL != LspIoSession);

	allocateLength = 
		sizeof(LSP_IO_SESSION) + LSP_SESSION_BUFFER_SIZE;

	*LspIoSession = (PLSP_IO_SESSION) ExAllocatePoolWithTag(
		NonPagedPool, 
		allocateLength, 
		LSP_IO_SESSION_TAG);

	if (NULL == *LspIoSession)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(*LspIoSession, allocateLength);

	(*LspIoSession)->DeviceObject = DeviceObject;
	(*LspIoSession)->LspSessionBuffer = LspIoOffsetOf(*LspIoSession, sizeof(LSP_IO_SESSION));

	if (InternalDataBufferLength > 0)
	{
		(*LspIoSession)->InternalBuffer = ExAllocatePoolWithTag(
			NonPagedPool,
			InternalDataBufferLength,
			LSP_IO_DATA_TAG);
		if (NULL == (*LspIoSession)->InternalBuffer)
		{
			ExFreePoolWithTag((*LspIoSession), LSP_IO_SESSION_TAG);
			(*LspIoSession) = NULL;
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		(*LspIoSession)->InternalBufferLength = InternalDataBufferLength;
	}

	//
	// Synchronous IO Context
	//
	KeInitializeEvent(
		&(*LspIoSession)->SyncIoContext.IoCompletionEvent, 
		NotificationEvent, 
		FALSE);

	//
	// Tdi Timeout Context
	//
	KeInitializeEvent(
		&(*LspIoSession)->TdiTimeOutEvent,
		NotificationEvent,
		FALSE);

	//
	// Tdi Error Context
	//
	KeInitializeEvent(
		&(*LspIoSession)->TdiErrorEvent,
		NotificationEvent,
		FALSE);

	(*LspIoSession)->SessionState = LSP_IOS_STATE_ALLOCATED;
	(*LspIoSession)->PreviousState = LSP_IOS_STATE_NONE;
	(*LspIoSession)->PendingState = LSP_IOS_STATE_NONE;

	return STATUS_SUCCESS;
}

VOID
LSPIOCALL
LspIoFreeSession(
	__in PLSP_IO_SESSION LspIoSession)
{
	//
	// Ensure that all resources are freed already.
	//
	ASSERT(0 == LspIoSession->LspIoResourceFlags);

	if (LspIoSession->InternalBuffer)
	{
		ExFreePoolWithTag(LspIoSession->InternalBuffer, LSP_IO_DATA_TAG);
#if DBG
		LspIoSession->InternalBufferLength = 0;
#endif
	}

	//
	// Explicitly mark the memory as deallocated to help debugging
	//
	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_DEALLOCATED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;

	ExFreePoolWithTag(LspIoSession, LSP_IO_SESSION_TAG);
}

//////////////////////////////////////////////////////////////////////////
//
// Session Create/Close Functions
//
//////////////////////////////////////////////////////////////////////////

NTSTATUS
LSPIOCALL
LspIoInitializeSession(
	__in PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	ASSERT(LSP_IOS_STATE_ALLOCATED == LspIoSession->SessionState);

	LspIoSession->LspHandle = lsp_create_session(
		LspIoSession->LspSessionBuffer,
		NULL);

#ifdef LSPIO_IMP_USE_EXTERNAL_ENCODE
	lsp_set_options(
		LspIoSession->LspHandle, 
		LSP_SO_USE_EXTERNAL_DATA_ENCODE);
#endif

	SetRelativeDueTimeInSecond(
		&LspIoSession->ReceiveTimeOut,
		LSPIO_DEFAULT_RECEIVE_TIMEOUT_SEC);

	SetRelativeDueTimeInSecond(
		&LspIoSession->SendTimeOut,
		LSPIO_DEFAULT_SEND_TIMEOUT_SEC);

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_INITIALIZED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;

	return STATUS_SUCCESS;
}

VOID
LSPIOCALL
LspIoCleanupSession(
	PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	ASSERT(LSP_IOS_STATE_INITIALIZED == LspIoSession->SessionState);
	ASSERT(0 == LspIoSession->LspIoResourceFlags);

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_ALLOCATED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;

	//
	// LSP Session
	//

	lsp_destroy_session(LspIoSession->LspHandle);
	LspIoSession->LspHandle = NULL;
}

NTSTATUS
LSPIOCALL
LspIoCreateConnectionFile(
	__in PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;
	ULONG i;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	ASSERT(NULL == LspIoSession->ConnectionHandle);
	ASSERT(NULL == LspIoSession->ConnectionFileObject);
	ASSERT(NULL == LspIoSession->ConnectionDeviceObject);

	ASSERT(!LspIoTestStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED));

	status = xLpxTdiCreateConnectionObject(
		LspIoSession,
		&LspIoSession->ConnectionHandle,
		&LspIoSession->ConnectionFileObject,
		&LspIoSession->ConnectionDeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xLpxTdiCreateConnectionObject failed with status %x\n", status);
		return status;
	}

	LspIoSetStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"Connection Handle=%p, DeviceObject=%p, FileObject=%p\n",
		LspIoSession->ConnectionHandle,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject);

	//
	// TdiIoContext Initialize
	//
	status = TdiIoContextInitialize(
		&LspIoSession->NonLspTdiIoContext,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession,
		LspIoTdiNonLspCompletion);

	if (!NT_SUCCESS(status))
	{
		xTdiCloseConnectionObject(
			LspIoSession->ConnectionHandle,
			LspIoSession->ConnectionFileObject);

		LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED);

		LspIoSession->ConnectionHandle = NULL;
		LspIoSession->ConnectionFileObject = NULL;
		LspIoSession->ConnectionDeviceObject = NULL;
	}

	status = TdiIoContextInitialize(
		&LspIoSession->LpxControlTdiIoContext,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession,
		LspIoTdiPerfCompletion);

	if (!NT_SUCCESS(status))
	{
		TdiIoContextCleanup(&LspIoSession->NonLspTdiIoContext);

		xTdiCloseConnectionObject(
			LspIoSession->ConnectionHandle,
			LspIoSession->ConnectionFileObject);

		LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED);

		LspIoSession->ConnectionHandle = NULL;
		LspIoSession->ConnectionFileObject = NULL;
		LspIoSession->ConnectionDeviceObject = NULL;
	}

	for (i = 0; i < countof(LspIoSession->LspTdiIoContext); ++i)
	{
		status = TdiIoContextInitialize(
			&LspIoSession->LspTdiIoContext[i],
			LspIoSession->ConnectionDeviceObject,
			LspIoSession,
			LspIoTdiDataTdiCompletion);

		if (!NT_SUCCESS(status))
		{
			ULONG j;

			for (j = 0; j < i; ++j)
			{
				TdiIoContextCleanup(&LspIoSession->LspTdiIoContext[j]);
			}

			TdiIoContextCleanup(&LspIoSession->LpxControlTdiIoContext);
			TdiIoContextCleanup(&LspIoSession->NonLspTdiIoContext);

			xTdiCloseConnectionObject(
				LspIoSession->ConnectionHandle,
				LspIoSession->ConnectionFileObject);

			LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED);

			LspIoSession->ConnectionHandle = NULL;
			LspIoSession->ConnectionFileObject = NULL;
			LspIoSession->ConnectionDeviceObject = NULL;

			return status;
		}
	}

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_CONNECTION_FILE_CREATED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;

	return STATUS_SUCCESS;
}

VOID
LSPIOCALL
LspIoCloseConnectionFile(
	__in PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;
	ULONG i;

	//
	// LspIoTdiData
	//

	for (i = 0; i < countof(LspIoSession->LspTdiIoContext); ++i)
	{
		TdiIoContextCleanup(&LspIoSession->LspTdiIoContext[i]);
	}

	TdiIoContextCleanup(&LspIoSession->LpxControlTdiIoContext);
	TdiIoContextCleanup(&LspIoSession->NonLspTdiIoContext);

	//
	// Connection Object
	//

	ASSERT(LspIoSession->ConnectionHandle != NULL);
	ASSERT(LspIoSession->ConnectionFileObject != NULL);
	ASSERT(LspIoSession->ConnectionDeviceObject != NULL);

	ASSERT(LspIoTestStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED));

	status = xTdiCloseConnectionObject(
		LspIoSession->ConnectionHandle,
		LspIoSession->ConnectionFileObject);

	ASSERT(NT_SUCCESS(status));

	LspIoSession->ConnectionHandle = NULL;
	LspIoSession->ConnectionFileObject = NULL;
	LspIoSession->ConnectionDeviceObject = NULL;

	LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_CONNECTION_CREATED);

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_INITIALIZED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;
}

NTSTATUS
LSPIOCALL
LspIoCreateAddressFile(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS LspIoLocalAddress)
{
	NTSTATUS status, status2;

	XTDI_EVENT_HANDLER eventHandlers[] = {
		TDI_EVENT_DISCONNECT, LspIoClientEventDisconnect
	};

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, LspIoLocalAddress(LPX)=%02X:%02X:%02X-%02X:%02X:%02X.\n", 
		LspIoSession, 
		LspIoLocalAddress->LpxAddress.Node[0],
		LspIoLocalAddress->LpxAddress.Node[1],
		LspIoLocalAddress->LpxAddress.Node[2],
		LspIoLocalAddress->LpxAddress.Node[3],
		LspIoLocalAddress->LpxAddress.Node[4],
		LspIoLocalAddress->LpxAddress.Node[5]);

	ASSERT(NULL == LspIoSession->AddressHandle);
	ASSERT(NULL == LspIoSession->AddressFileObject);
	ASSERT(NULL == LspIoSession->AddressDeviceObject);

	ASSERT(!LspIoTestStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_CREATED));

	status = xLpxTdiCreateAddressObject(
		&LspIoLocalAddress->LpxAddress,
		&LspIoSession->AddressHandle,
		&LspIoSession->AddressFileObject,
		&LspIoSession->AddressDeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xLpxTdiCreateAddressObject failed with status %x\n", status);

		goto error_1;
	}

	LspIoSetStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_CREATED);

	ASSERT(!LspIoTestStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_ASSOCIATED));

	status = xTdiAssociateAddress(
		LspIoSession->AddressHandle,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xTdiAssociateAddress failed with status %x\n", status);

		goto error_2;
	}

	LspIoSetStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_ASSOCIATED);

	status = xTdiSetEventHandler(
		LspIoSession->AddressDeviceObject,
		LspIoSession->AddressFileObject,
		eventHandlers,
		countof(eventHandlers),
		LspIoSession);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"xTdiSetEventHandler failed with status %x\n", status);

		goto error_3;
	}

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSessionSetupLocalAddress completed.\n");

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_ADDRESS_FILE_CREATED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;

	return STATUS_SUCCESS;

error_3:

	xTdiDisassociateAddress(
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject);

	LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_ASSOCIATED);

error_2:

	xTdiCloseAddressObject(
		LspIoSession->AddressHandle,
		LspIoSession->AddressFileObject);

	LspIoSession->AddressHandle = NULL;
	LspIoSession->AddressFileObject = NULL;
	LspIoSession->AddressDeviceObject = NULL;

	LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_CREATED);

error_1:

	return status;
}

VOID
LSPIOCALL
LspIoCloseAddressFile(
	__in PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;

	XTDI_EVENT_HANDLER eventHandlers[] = {
		TDI_EVENT_DISCONNECT, NULL
	};

	ASSERT(NULL != LspIoSession->AddressDeviceObject);
	ASSERT(NULL != LspIoSession->AddressFileObject);

	//
	// Event Handler
	//
	
	status = xTdiSetEventHandler(
		LspIoSession->AddressDeviceObject,
		LspIoSession->AddressFileObject,
		eventHandlers,
		countof(eventHandlers),
		LspIoSession);

	ASSERT(NT_SUCCESS(status));

	//
	// Address - Connection Association
	//

	ASSERT(LspIoTestStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_ASSOCIATED));

	status = xTdiDisassociateAddress(
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject);

	ASSERT(NT_SUCCESS(status));

	LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_ASSOCIATED);

	//
	// Address Object
	//

	ASSERT(LspIoTestStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_CREATED));

	ASSERT(NULL != LspIoSession->AddressHandle);
	ASSERT(NULL != LspIoSession->AddressFileObject);
	ASSERT(NULL != LspIoSession->AddressDeviceObject);

	status = xTdiCloseAddressObject(
		LspIoSession->AddressHandle,
		LspIoSession->AddressFileObject);

	ASSERT(NT_SUCCESS(status));

	LspIoSession->AddressHandle = NULL;
	LspIoSession->AddressFileObject = NULL;
	LspIoSession->AddressDeviceObject = NULL;

	LspIoClearStateFlag(LspIoSession, LSP_IOS_RES_ADDRESS_CREATED);

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = LSP_IOS_STATE_CONNECTION_FILE_CREATED;
	LspIoSession->PendingState = LSP_IOS_STATE_NONE;
}

//////////////////////////////////////////////////////////////////////////
//
// TDI Connections
//
//////////////////////////////////////////////////////////////////////////

VOID
FORCEINLINE
LspIoStartNewIo(
	__in PLSP_IO_SESSION LspIoSession,
	__in LSP_IOS_STATE SessionState,
	__in LSP_IOS_STATE PendingState,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext)
{

	LspIoSession->LspStatus = 0;
	ASSERT(0 == LspIoSession->OutstandingTdiIO);
	LspIoSession->OutstandingTdiIO = 0;

	RtlZeroMemory(&LspIoSession->TdiStatus, sizeof(IO_STATUS_BLOCK));

	KeClearEvent(&LspIoSession->TdiErrorEvent);
	KeClearEvent(&LspIoSession->TdiTimeOutEvent);

	LspIoSession->PreviousState = LspIoSession->SessionState;
	LspIoSession->SessionState = SessionState;
	LspIoSession->PendingState = PendingState;

	LspIoSetCompletionRoutine(
		LspIoSession,
		CompletionRoutine,
		CompletionContext);
}

NTSTATUS
LSPIOCALL
LspIoConnect(
	PLSP_IO_SESSION LspIoSession, 
	PLSP_TRANSPORT_ADDRESS LspIoDeviceAddress,
	PLARGE_INTEGER Timeout,
	PLSP_IO_COMPLETION CompletionRoutine, 
	PVOID CompletionContext)
{
	NTSTATUS status;
	PTDI_IO_CONTEXT tdiIoContext;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	if (LspIoSession->LocalAddress.Type != LspIoDeviceAddress->Type)
	{
		ASSERTMSG("Different LspTransportAddresses are specified!", FALSE);
		return STATUS_INVALID_PARAMETER_2;
	}

	ASSERT(LSP_IOS_STATE_ADDRESS_FILE_CREATED == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_CONNECTING,
		LSP_IOS_STATE_CONNECTED,
		CompletionRoutine,
		CompletionContext);

	tdiIoContext = &LspIoSession->NonLspTdiIoContext;
	TdiIoContextReuse(tdiIoContext);

	status = xLpxTdiConnectEx(
		tdiIoContext->Irp,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject,
		&LspIoDeviceAddress->LpxAddress,
		Timeout,
		&tdiIoContext->Overlapped);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoDisconnect(
	PLSP_IO_SESSION LspIoSession, 
	ULONG DisconnectFlags,
	PLARGE_INTEGER Timeout,
	PLSP_IO_COMPLETION CompletionRoutine, 
	PVOID CompletionContext)
{
	NTSTATUS status;
	PTDI_IO_CONTEXT tdiIoContext;
	LONG outstandingIO;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p, SessionState=%d\n", 
		LspIoSession, LspIoSession->SessionState);

	// ASSERT(LspSessionConnected == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_DISCONNECTING,
		LSP_IOS_STATE_CONNECTION_FILE_CREATED,
		CompletionRoutine,
		CompletionContext);

	LspIoSession->PreviousState = LSP_IOS_STATE_CONNECTION_FILE_CREATED; // LspIoSession->SessionState;

	tdiIoContext = &LspIoSession->NonLspTdiIoContext;
	TdiIoContextReuse(tdiIoContext);

	status = xTdiDisconnectEx(
		tdiIoContext->Irp,
		LspIoSession->ConnectionDeviceObject,
		LspIoSession->ConnectionFileObject,
		DisconnectFlags,
		Timeout,
		NULL,
		NULL, 
		&tdiIoContext->Overlapped);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Lsp Login/Logout
//
//////////////////////////////////////////////////////////////////////////

NTSTATUS
LSPIOCALL
LspIoLogin(
	PLSP_IO_SESSION LspIoSession,
	CONST LSP_LOGIN_INFO* LspLoginInfo,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_LOGGING_IN,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	LspIoSession->LspStatus = lsp_login(
		LspIoSession->LspHandle,
		LspLoginInfo);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoLogout(
	PLSP_IO_SESSION LspIoSession,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_LOGGING_OUT,
		LSP_IOS_STATE_CONNECTED,
		CompletionRoutine,
		CompletionContext);

	LspIoSession->LspStatus = lsp_logout(LspIoSession->LspHandle);
	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}


ULONG
LSPIOCALL
LspIoGetIoFlags(
	__in PLSP_IO_SESSION LspIoSession)
{
	return LspIoSession->LspIoFlags;
}

VOID
LSPIOCALL
LspIoSetIoFlags(
	__in PLSP_IO_SESSION LspIoSession,
	__in ULONG Flags)
{
	LspIoSession->LspIoFlags = Flags;
}

VOID
LSPIOCALL
LspIoAppendIoFlag(
	__in PLSP_IO_SESSION LspIoSession,
	__in ULONG Flag)
{
	LspIoSession->LspIoFlags |= Flag;
}

VOID
LSPIOCALL
LspIoClearIoFlag(
	__in PLSP_IO_SESSION LspIoSession,
	__in ULONG Flag)
{
	LspIoSession->LspIoFlags &= ~(Flag);
}

NDAS_HARDWARE_VERSION_INFO
LSPIOCALL
LspIoGetHardwareVersionInfo(
	__in PLSP_IO_SESSION LspIoSession)
{
	lsp_status_t lspStatus;

	NDAS_HARDWARE_VERSION_INFO versionInfo = {0};

	lspStatus = lsp_get_handle_info(
		LspIoSession->LspHandle,
		LSP_PROP_HW_VERSION,
		&versionInfo.Version,
		sizeof(UCHAR));

	ASSERT(LSP_STATUS_SUCCESS == lspStatus);

	lspStatus = lsp_get_handle_info(
		LspIoSession->LspHandle,
		LSP_PROP_HW_REVISION,
		&versionInfo.Revision,
		sizeof(USHORT));

	ASSERT(LSP_STATUS_SUCCESS == lspStatus);

	return versionInfo;
}

NTSTATUS
LSPIOCALL
LspIoGetLastIdeOutputRegister(
	PLSP_IO_SESSION LspIoSession,
	PLSP_IDE_REGISTER IdeRegister)
{
	lsp_status_t LspStatus;

	LspStatus = lsp_get_ide_command_output_register(
		LspIoSession->LspHandle,
		IdeRegister);

	return (LSP_STATUS_SUCCESS == LspStatus) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

//////////////////////////////////////////////////////////////////////////
//
// Lsp IO
//
//////////////////////////////////////////////////////////////////////////

NTSTATUS
LspIoVerifiedWritePostReadCompletion(
	PLSP_IO_SESSION LspIoSession,
	PIO_STATUS_BLOCK IoStatus,
	PVOID Context)
{
	PLSP_VERIFIED_WRITE_CONTEXT verifiedWriteContext;
	SIZE_T verifiedLength;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, Status=%x\n",
		LspIoSession, IoStatus->Status);

	verifiedWriteContext = (PLSP_VERIFIED_WRITE_CONTEXT) Context;

	if (!NT_SUCCESS(IoStatus->Status))
	{
		return LspIoInvokeNextCompletionRoutine(LspIoSession, IoStatus);
	}

	//
	// We have the internal buffer, which is read from the disk again
	//
	verifiedLength = RtlCompareMemory(
		verifiedWriteContext->OriginalBuffer,
		LspIoSession->InternalBuffer,
		verifiedWriteContext->TransferLength);

	if (verifiedLength < verifiedWriteContext->TransferLength)
	{
		DebugPrint((0, "!!!Corruption Detected from %Xh/%dXh", verifiedLength, verifiedWriteContext->TransferLength));
		return LspIoInvokeNextCompletionRoutine(LspIoSession, IoStatus);
	}
	else
	{
		return LspIoInvokeNextCompletionRoutine(LspIoSession, IoStatus);
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LspIoVerifiedWritePostWriteCompletion(
	PLSP_IO_SESSION LspIoSession,
	PIO_STATUS_BLOCK IoStatus,
	PVOID Context)
{
	NTSTATUS status;

	PLSP_VERIFIED_WRITE_CONTEXT verifiedWriteContext;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, Status=%x\n",
		LspIoSession, IoStatus->Status);

	verifiedWriteContext = (PLSP_VERIFIED_WRITE_CONTEXT) Context;

	if (!NT_SUCCESS(IoStatus->Status))
	{
		return LspIoInvokeNextCompletionRoutine(LspIoSession, IoStatus);
	}

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		LspIoVerifiedWritePostReadCompletion,
		verifiedWriteContext);

	LspIoSession->LspStatus = lsp_ide_read(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_DMA) ? 1 : 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_LBA48) ? 1 : 0,
		(lsp_large_integer_t*) &verifiedWriteContext->LogicalBlockAddress,
		(lsp_uint16)(verifiedWriteContext->TransferLength >> 9),
		LspIoSession->InternalBuffer,
		verifiedWriteContext->TransferLength);

	status = LspIoProcessNext(LspIoSession);

	return STATUS_SUCCESS;
}

NTSTATUS
LSPIOCALL
LspIoSingleWrite(
	PLSP_IO_SESSION LspIoSession,
	PVOID Buffer,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, Buffer=%p, LBA=%I64d, TransferLength=%d\n",
		LspIoSession, Buffer, LogicalBlockAddress->QuadPart, TransferLength);

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoSession->VerifiedWriteContext.OriginalBuffer = Buffer;
	LspIoSession->VerifiedWriteContext.LogicalBlockAddress = *LogicalBlockAddress;
	LspIoSession->VerifiedWriteContext.TransferLength = TransferLength;

	if (TestFlag(LspIoSession->LspIoFlags, LSP_IOF_VERIFY_WRITE))
	{
		LspIoAddCompletionRoutine(
			LspIoSession,
			CompletionRoutine, 
			CompletionContext);

		LspIoStartNewIo(
			LspIoSession,
			LSP_IOS_STATE_EXECUTING,
			LSP_IOS_STATE_READY,
			LspIoVerifiedWritePostWriteCompletion,
			&LspIoSession->VerifiedWriteContext);
	}
	else
	{
		LspIoStartNewIo(
			LspIoSession,
			LSP_IOS_STATE_EXECUTING,
			LSP_IOS_STATE_READY,
			CompletionRoutine,
			CompletionContext);
	}

	ASSERT(0 == (TransferLength % 512));
	ASSERT(TransferLength <= LspIoSession->InternalBufferLength);

	LspIoSession->OriginalBuffer = Buffer;
	LspIoSession->OriginalBufferLength = TransferLength;

#ifdef LSPIO_IMP_USE_EXTERNAL_ENCODE
#else
	RtlCopyMemory(
		LspIoSession->InternalBuffer,
		LspIoSession->OriginalBuffer,
		TransferLength);
#endif

	LspIoSession->LspStatus = lsp_ide_write(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_DMA) ? 1 : 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_LBA48) ? 1 : 0,
		(lsp_large_integer_t*) LogicalBlockAddress,
		(lsp_uint16)(TransferLength >> 9),
		LspIoSession->InternalBuffer,
		TransferLength);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoSingleRead(
	PLSP_IO_SESSION LspIoSession,
	PVOID Buffer,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, Buffer=%p, LBA=%I64d, TransferLength=%d\n",
		LspIoSession, Buffer, LogicalBlockAddress->QuadPart, TransferLength);

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	ASSERT(0 == (TransferLength % 512));

	LspIoSession->LspStatus = lsp_ide_read(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_DMA) ? 1 : 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_LBA48) ? 1 : 0,
		(lsp_large_integer_t*) LogicalBlockAddress,
		(lsp_uint16)(TransferLength >> 9),
		Buffer,
		TransferLength);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoSingleVerify(
	PLSP_IO_SESSION LspIoSession,
	PVOID Reserved,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(Reserved);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, LBA=%I64Xh, TransferLength=%Xh\n",
		LspIoSession, LogicalBlockAddress->QuadPart, TransferLength);

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	ASSERT(0 == (TransferLength % 512));

	LspIoSession->LspStatus = lsp_ide_verify(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		(LspIoSession->LspIoFlags & LSP_IOF_USE_LBA48) ? 1 : 0,
		(lsp_large_integer_t*) LogicalBlockAddress,
		(lsp_uint16)(TransferLength >> 9));

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoIdeCommand(
	__in PLSP_IO_SESSION LspIoSession,
	__in_bcount(sizeof(LSP_IDE_REGISTER)) PLSP_IDE_REGISTER IdeRegister,
	__in_bcount(InBufferLength) PVOID InBuffer,
	__in ULONG InBufferLength,
	__out_bcount(OutBufferLength) PVOID OutBuffer,
	__in ULONG OutBufferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext)
{
	NTSTATUS status;
	lsp_io_data_buffer_t lsp_data_buffer;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, "
		"IdeRegister(FE,SC,LL,LM,LH,DE,CM)=(%02X %02X %02X %02X %02X %02X %02X)\n",
		LspIoSession, 
		IdeRegister->reg.named.features,
		IdeRegister->reg.named.sector_count,
		IdeRegister->reg.named.lba_low,
		IdeRegister->reg.named.lba_mid,
		IdeRegister->reg.named.lba_high,
		IdeRegister->device.device,
		IdeRegister->command.command);

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	//
	// Device Register DEV bit should be adjusted.
	//
	ASSERT(IdeRegister->device.dev == LspIoSession->LspLoginInfo.unit_no);
	if (IdeRegister->device.dev != LspIoSession->LspLoginInfo.unit_no)
	{
		IdeRegister->device.dev = LspIoSession->LspLoginInfo.unit_no;
	}

	//
	// We do not support simultaneous IN/OUT data
	//
	ASSERT(InBuffer == NULL || OutBuffer == NULL);
	ASSERT(InBuffer != NULL || 0 == InBufferLength);
	ASSERT(OutBuffer != NULL || 0 == OutBufferLength);

	lsp_data_buffer.recv_buffer = OutBuffer;
	lsp_data_buffer.recv_size = OutBufferLength;
	lsp_data_buffer.send_buffer = InBuffer;
	lsp_data_buffer.send_size = InBufferLength;

	LspIoSession->LspStatus = lsp_ide_command(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		IdeRegister,
		&lsp_data_buffer,
		NULL);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoAtaIdentifyDevice(
	__in PLSP_IO_SESSION LspIoSession,
	__out_bcount(TransferLength) PVOID Buffer,
	__in PLARGE_INTEGER Reserved,
	__in ULONG TransferLength,
	__in PLSP_IO_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p, Buffer=%p, TransferLength=%d\n",
		LspIoSession, Buffer, TransferLength);

	UNREFERENCED_PARAMETER(Reserved);

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	ASSERT(TransferLength == 512);

	LspIoSession->LspStatus = lsp_ide_identify(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no, 0, 0,
		Buffer);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LSPIOCALL
LspIoAtaSetFeature(
	__in PLSP_IO_SESSION LspIoSession,
	__in ATA_SETFEATURE_SUBCOMMAND Feature,
	__in UCHAR SectorCountRegister,
	__in UCHAR LBALowRegister,
	__in UCHAR LBAMidRegister,
	__in UCHAR LBAHighRegister,
	__in PLSP_IO_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	NTSTATUS status;
	LSP_IDE_REGISTER ideRegister = {0};

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, "
		"IdeRegister(F,SC,LL,LM,LH,C)=(%02X %02X %02X %02X %02X %02X)\n",
		LspIoSession, 
		Feature,
		SectorCountRegister,
		LBALowRegister,
		LBAMidRegister,
		LBAHighRegister,
		0xEF);

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	ideRegister.reg.named.features = Feature;
	ideRegister.reg.named.sector_count = SectorCountRegister;
	ideRegister.reg.named.lba_low = LBALowRegister;
	ideRegister.reg.named.lba_mid = LBAMidRegister;
	ideRegister.reg.named.lba_high = LBAHighRegister;
	ideRegister.command.command = 0xEF; /* Set feature */

	LspIoSession->LspStatus = lsp_ide_command(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		&ideRegister,
		NULL,
		NULL);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

//
// FLUSH CACHE
// FLUSH CACHE EXT
//
NTSTATUS
LSPIOCALL
LspIoAtaFlushCache(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	static CONST UCHAR FLUSH_CACHE = 0xE7;
	static CONST UCHAR FLUSH_CACHE_EXT = 0xEA;

	NTSTATUS status;
	LSP_IDE_REGISTER ideRegister = {0};

	ASSERT(LSP_IOS_STATE_READY == LspIoSession->SessionState);

	LspIoStartNewIo(
		LspIoSession,
		LSP_IOS_STATE_EXECUTING,
		LSP_IOS_STATE_READY,
		CompletionRoutine,
		CompletionContext);

	ideRegister.use_48 = TestFlag(LspIoSession->LspIoFlags, LSP_IOF_USE_LBA48);
	ideRegister.command.command = ideRegister.use_48 ? FLUSH_CACHE_EXT : FLUSH_CACHE;

	LspIoSession->LspStatus = lsp_ide_command(
		LspIoSession->LspHandle,
		LspIoSession->LspLoginInfo.unit_no,
		0, 0,
		&ideRegister,
		NULL,
		NULL);

	status = LspIoProcessNext(LspIoSession);

	if (NULL == CompletionRoutine)
	{
		return LspIoWaitForCompletion(LspIoSession, status);
	}
	else
	{
		return status;
	}
}

NTSTATUS
LspIoMultipleIoCompletion(
	PLSP_IO_SESSION LspIoSession,
	PIO_STATUS_BLOCK IoStatus,
	PVOID Context)
{
	PLSP_MULTIPLE_IO_CONTEXT MultiIoContext;
	PUCHAR buffer;
	ULONG transferLength;
	LARGE_INTEGER logicalBlockAddress;

	MultiIoContext = (PLSP_MULTIPLE_IO_CONTEXT) Context;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LspIoSession=%p, NextBuffer=%p, NextLBA=%I64Xh, NextTransferLength=%Xh\n",
		LspIoSession,
		MultiIoContext->NextBuffer,
		MultiIoContext->NextLogicalBlockAddress.QuadPart,
		MultiIoContext->NextTransferLength);

	if (!NT_SUCCESS(IoStatus->Status))
	{
		MultiIoContext->IoStatus.Status = IoStatus->Status;
		MultiIoContext->IoStatus.Information += IoStatus->Information;
		
		return LspIoInvokeNextCompletionRoutine(
			LspIoSession, 
			&MultiIoContext->IoStatus);
	}

	if (0 == MultiIoContext->NextTransferLength)
	{
		MultiIoContext->IoStatus.Status = IoStatus->Status;
		MultiIoContext->IoStatus.Information += IoStatus->Information;

		return LspIoInvokeNextCompletionRoutine(
			LspIoSession,
			&MultiIoContext->IoStatus);
	}

	transferLength = min(
		MultiIoContext->NextTransferLength,
		LspIoSession->CurrentMaximumTransferLength);

	logicalBlockAddress = MultiIoContext->NextLogicalBlockAddress;
	buffer = MultiIoContext->NextBuffer;

	ASSERT(MultiIoContext->NextTransferLength >= transferLength);

	MultiIoContext->NextLogicalBlockAddress.QuadPart += transferLength >> 9;
	MultiIoContext->NextTransferLength -= transferLength;
	MultiIoContext->NextBuffer += transferLength;

	return (*MultiIoContext->IoRoutine)(
		LspIoSession,
		buffer,
		&logicalBlockAddress,
		transferLength,
		LspIoMultipleIoCompletion,
		MultiIoContext);
}


NTSTATUS
LSPIOCALL
LspIoWrite(
	PLSP_IO_SESSION LspIoSession,
	PVOID Buffer,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"WRITE LspIoSession=%p, Buffer=%p, LBA=%I64Xh, TransferLength=%Xh\n",
		LspIoSession, Buffer, LogicalBlockAddress->QuadPart, TransferLength);

	ASSERT(0 == (TransferLength % 512));

	if (TransferLength <= LspIoSession->CurrentMaximumTransferLength)
	{
		return LspIoSingleWrite(
			LspIoSession, 
			Buffer, 
			LogicalBlockAddress, 
			TransferLength, 
			CompletionRoutine, 
			CompletionContext);
	}
	else
	{
		LspIoAddCompletionRoutine(
			LspIoSession, 
			CompletionRoutine, 
			CompletionContext);

		LspIoSession->MultiIoContext.IoRoutine = LspIoSingleWrite;
		RtlZeroMemory(
			&LspIoSession->MultiIoContext.IoStatus, 
			sizeof(IO_STATUS_BLOCK));
		LspIoSession->MultiIoContext.NextBuffer = Buffer;
		LspIoSession->MultiIoContext.NextLogicalBlockAddress = *LogicalBlockAddress;
		LspIoSession->MultiIoContext.NextTransferLength = TransferLength;

		return LspIoMultipleIoCompletion(
			LspIoSession,
			&LspIoSession->MultiIoContext.IoStatus,
			&LspIoSession->MultiIoContext);
	}
}

NTSTATUS
LSPIOCALL
LspIoRead(
	PLSP_IO_SESSION LspIoSession,
	PVOID Buffer,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"READ LspIoSession=%p, Buffer=%p, LBA=%I64Xh, TransferLength=%Xh\n",
		LspIoSession, Buffer, LogicalBlockAddress->QuadPart, TransferLength);

	ASSERT(0 == (TransferLength % 512));

	if (TransferLength <= LspIoSession->CurrentMaximumTransferLength)
	{
		return LspIoSingleRead(
			LspIoSession, 
			Buffer, 
			LogicalBlockAddress, 
			TransferLength, 
			CompletionRoutine, 
			CompletionContext);
	}
	else
	{
		LspIoAddCompletionRoutine(
			LspIoSession, 
			CompletionRoutine, 
			CompletionContext);

		LspIoSession->MultiIoContext.IoRoutine = LspIoSingleRead;
		RtlZeroMemory(
			&LspIoSession->MultiIoContext.IoStatus, 
			sizeof(IO_STATUS_BLOCK));
		LspIoSession->MultiIoContext.NextBuffer = Buffer;
		LspIoSession->MultiIoContext.NextLogicalBlockAddress = *LogicalBlockAddress;
		LspIoSession->MultiIoContext.NextTransferLength = TransferLength;
		return LspIoMultipleIoCompletion(
			LspIoSession,
			&LspIoSession->MultiIoContext.IoStatus,
			&LspIoSession->MultiIoContext);
	}
}

//
// READ VERIFY SECTOR(S)
// READ VERIFY SECTOR(S) EXT
//
NTSTATUS
LSPIOCALL
LspIoVerify(
	PLSP_IO_SESSION LspIoSession,
	PVOID Reserved,
	PLARGE_INTEGER LogicalBlockAddress,
	ULONG TransferLength,
	PLSP_IO_COMPLETION CompletionRoutine,
	PVOID CompletionContext)
{
	UNREFERENCED_PARAMETER(Reserved);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"VERIFY LspIoSession=%p, LBA=%I64Xh, TransferLength=%Xh\n",
		LspIoSession, LogicalBlockAddress->QuadPart, TransferLength);

	ASSERT(0 == (TransferLength % 512));

	// Not CurrentMaximumTransferLength but the hard limit!
	if (TransferLength <= LspIoSession->MaximumTransferLength)
	{
		return LspIoSingleVerify(
			LspIoSession, 
			0, 
			LogicalBlockAddress, 
			TransferLength, 
			CompletionRoutine, 
			CompletionContext);
	}
	else
	{
		LspIoAddCompletionRoutine(
			LspIoSession, 
			CompletionRoutine, 
			CompletionContext);

		LspIoSession->MultiIoContext.IoRoutine = LspIoSingleVerify;
		RtlZeroMemory(
			&LspIoSession->MultiIoContext.IoStatus, 
			sizeof(IO_STATUS_BLOCK));
		LspIoSession->MultiIoContext.NextBuffer = 0;
		LspIoSession->MultiIoContext.NextLogicalBlockAddress = *LogicalBlockAddress;
		LspIoSession->MultiIoContext.NextTransferLength = TransferLength;
		return LspIoMultipleIoCompletion(
			LspIoSession,
			&LspIoSession->MultiIoContext.IoStatus,
			&LspIoSession->MultiIoContext);
	}
}

NTSTATUS
LSPIOCALL
LspIoRequest(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	lsp_request_packet_t* request = &LspIoSession->LspRequestPackets[0];

	switch (IoRequest->OperationCode)
	{
	case LspIoOpAcquireBufferLock:
		lsp_build_acquire_lock(
			request, 
			LspIoSession->LspHandle, 
			LspIoSession->LspLoginInfo.unit_no);
		break;
	case LspIoOpReleaseBufferLock:
		lsp_build_acquire_lock(
			request, 
			LspIoSession->LspHandle, 
			LspIoSession->LspLoginInfo.unit_no);
		break;
	case LspIoOpRead:
		lsp_build_ide_read(
			request, 
			LspIoSession->LspHandle,
			LspIoSession->LspLoginInfo.unit_no, 0, 0,
			(LspIoSession->LspIoFlags & LSP_IOF_USE_DMA) ? 1 : 0,
			(LspIoSession->LspIoFlags & LSP_IOF_USE_LBA48) ? 1 : 0,
			(lsp_large_integer_t*) &IoRequest->READ.LogicalBlockAddress,
			(lsp_uint16)(IoRequest->READ.TransferLength >> 9),
			IoRequest->READ.Buffer,
			IoRequest->READ.TransferLength);
		break;
	case LspIoOpWrite:
		lsp_build_ide_write(
			request,
			LspIoSession->LspHandle,
			LspIoSession->LspLoginInfo.unit_no, 0, 0,
			(LspIoSession->LspIoFlags & LSP_IOF_USE_DMA) ? 1 : 0,
			(LspIoSession->LspIoFlags & LSP_IOF_USE_LBA48) ? 1 : 0,
			(lsp_large_integer_t*) &IoRequest->WRITE.LogicalBlockAddress,
			(lsp_uint16)(IoRequest->WRITE.TransferLength >> 9),
			LspIoSession->InternalBuffer,
			IoRequest->WRITE.TransferLength);
		break;
	case LspIoOpLockedWrite:
		break;
	case LspIoOpVerify:
		break;
	case LspIoOpIdentify:
		break;
	case LspIoOpFlushCache:
		break;
	default:
		;
	}

	lsp_request(LspIoSession->LspHandle, request);

	return STATUS_PENDING;
}
