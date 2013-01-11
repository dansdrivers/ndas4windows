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
#include "lspconnect.h"
#include "lspfastconnect.h"
#include "lspiolight.h"
#include "lsplockcleanup.h"

#ifdef RUN_WPP
#include "lspio.tmh"
#endif

#define LSPIO_IMP_USE_EXTERNAL_ENCODE

#define LSP_IO_SESSION_TAG 'Spsl'
#define LSP_IO_DATA_TAG    'Dpsl'
#define LSP_IO_TDI_IRP_TAG 'Ipsl'

#define LSPIO_DEFAULT_RECEIVE_TIMEOUT_SEC 30
#define LSPIO_DEFAULT_SEND_TIMEOUT_SEC 30

#define LSP_TDI_DATA_IN_USE 0x00000001

typedef enum _LSP_IOS_STATE {
	LSP_IOS_STATE_NONE,
	LSP_IOS_STATE_DEALLOCATED,
	LSP_IOS_STATE_ALLOCATED,
	LSP_IOS_STATE_INITIALIZED,
	LSP_IOS_TRANSITION_CONNECTING,
	LSP_IOS_TRANSITION_DISCONNECTING,
	LSP_IOS_STATE_CONNECTED,
	LSP_IOS_TRANSITION_LOGGING_IN,
	LSP_IOS_TRANSITION_LOGGING_OUT,
	LSP_IOS_STATE_LOGGED_IN,
	LSP_IOS_TRANSITION_HANDSHAKING,
	LSP_IOS_STATE_IO_READY,
	LSP_IOS_TRANSITION_EXECUTING_IO,
} LSP_IOS_STATE;

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

	lsp_login_info_t LspLoginInfo;

	// LSP_TRANSPORT_ADDRESS LocalAddress;
	LSP_TRANSPORT_ADDRESS DeviceAddress;

	LSP_TRANSPORT_ADDRESS LocalAddressList[LSP_MAX_ADDRESS_COUNT];
	ULONG LocalAddressCount;

	//
	//  Associated Device Object (Required for WorkItems)
	//
	PDEVICE_OBJECT DeviceObject;


	//
	// Connection Data
	//
	PLSP_CONNECT_CONTEXT LspActiveConnection;
	LSP_FAST_CONNECT_CONTEXT LspFastConnectContext;

	PIO_WORKITEM ConnectWorkItem;

	//
	// Lock Cleanup Data
	//
	PLSP_LOCK_CLEANUP_CONTEXT LspLockCleanupContext;

	//
	// Main LSP Io Light Session
	//
	LSP_IO_LIGHT_SESSION LspIoLightSession;

	//
	// Internal state variables
	//
	LSP_IOS_STATE SessionState;

	ULONG MaximumLspTransferBytes;
	ULONG MaximumLspReadBytes;
	ULONG MaximumLspWriteBytes;

	ULONG NoLossReadBytes;
	ULONG NoLossWriteBytes;

	KEVENT TdiTimeOutEvent;
	KEVENT TdiErrorEvent;

	//
	// Save the IO_STATUS for performance counter
	//
	IO_STATUS_BLOCK UnderlyingStatus;

	XTDI_IO_SESSION NonLspTdiIoSession;
	XTDI_IO_SESSION LpxControlTdiIoSession;

	LARGE_INTEGER ReceiveTimeOut;
	LARGE_INTEGER SendTimeOut;

	ULONG LspIoCount;

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

	LSP_IO_REQUEST InternalFrontEndRequest;
	LSP_IO_REQUEST InternalRequests[5];

} LSP_IO_SESSION, *PLSP_IO_SESSION;

FORCEINLINE
PLSP_IO_REQUEST
LSPIOCALL
LspIopAcquireInternalIoRequest(
	__in PLSP_IO_SESSION LspIoSession)
{
	PLSP_IO_REQUEST InternalIoRequest;
	ULONG index;
	for (index = 0; index < countof(LspIoSession->InternalRequests); ++index)
	{
		InternalIoRequest = &LspIoSession->InternalRequests[index];
		if (LSP_IO_RESERVED == InternalIoRequest->RequestType)
		{
			InternalIoRequest->RequestType = LSP_IO_NONE;
			return InternalIoRequest;
		}
	}
	ASSERTMSG("Internal Request Overflow", FALSE);
	return NULL;
}

FORCEINLINE
VOID
LSPIOCALL
LspIopReleaseInternalIoRequest(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest)
{
	ASSERT(InternalIoRequest >= &LspIoSession->InternalRequests[0]);
	ASSERT(InternalIoRequest < &LspIoSession->InternalRequests[countof(LspIoSession->InternalRequests)]);
	InternalIoRequest->RequestType = LSP_IO_RESERVED;
}

#define LSP_DATA_BUFFER_SIZE (64 * 1024) // 64 KB

ULONG LSP_IO_DEFAULT_INTERAL_BUFFER_SIZE = LSP_DATA_BUFFER_SIZE;

VOID
LSPIOCALL
LspIopLspIoLightCompletion(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

NTSTATUS
LSPIOCALL
LspIopSetTdiEventHandler(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

VOID
LSPIOCALL
LspIopSetDisconnectEventHandlerCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

VOID
LSPIOCALL
LspIopFastConnectCompletion(
	__in PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext,
	__in PLSP_CONNECT_CONTEXT LspActiveConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

NTSTATUS
LSPIOCALL
LspIopPostConnectInitialization(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_CONNECT_CONTEXT LspActiveConnection);

VOID
LSPIOCALL
LspIopPostConnectUninitialize(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

FORCEINLINE
VOID
LSPIOCALL
LspIopCompleteRequest(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest);

NTSTATUS
LSPIOCALL
LspIopFastConnect(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopDisconnect(
	PLSP_IO_SESSION LspIoSession,
	PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopLogin(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopLogout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopStartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopStopSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopRestartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopPauseSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopUpdateConnectionStats(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopReadWriteVerify(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

VOID
LSPIOCALL
LspIopPseudoWriteFuaCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context);

VOID
LSPIOCALL
LspIopReadWriteVerifyCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

NTSTATUS
LSPIOCALL
LspIopFlushCache(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIopHandshake(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest);

FORCEINLINE
PLARGE_INTEGER
LspIopGetTimeoutZeroAsNull(
	__in PLARGE_INTEGER Timeout)
{
	if (Timeout->QuadPart != 0)
	{
		return Timeout;
	}
	else 
	{
		return NULL;
	}
}

FORCEINLINE
LARGE_INTEGER
LspIopGetTimeoutNullAsZero(
	__in_opt PLARGE_INTEGER ReferenceTimeout)
{
	if (ReferenceTimeout)
	{
		return *ReferenceTimeout;
	}
	else
	{
		static LARGE_INTEGER zero = {0};
		return zero;
	}
}

NTSTATUS
LspIopClientEventDisconnect(
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

FORCEINLINE
VOID
LspIoHandleTdiError(
	__in PLSP_IO_SESSION LspIoSession)
{
	BOOLEAN requireResetConnection;

	requireResetConnection = IsDispatchObjectSignaled(&LspIoSession->TdiTimeOutEvent);
}

//////////////////////////////////////////////////////////////////////////
//
// Session Allocation Functions
//
//////////////////////////////////////////////////////////////////////////

FORCEINLINE
PVOID
LspIopOffsetOf(PVOID Buffer, ULONG Length)
{
	return ((PUCHAR)Buffer) + Length;
}

NTSTATUS
LSPIOCALL
LspIoAllocateSession(
	__in PDEVICE_OBJECT DeviceObject,
	__in ULONG InternalDataBufferLength,
	__out PLSP_IO_SESSION* LspIoSession)
{
	PLSP_IO_SESSION p;
	ULONG allocateLength;

	ASSERT(NULL != LspIoSession);
	*LspIoSession = NULL;

	allocateLength = 
		sizeof(LSP_IO_SESSION) + LSP_SESSION_BUFFER_SIZE;

	p = (PLSP_IO_SESSION) ExAllocatePoolWithTag(
		NonPagedPool, 
		allocateLength, 
		LSP_IO_SESSION_TAG);

	if (NULL == p)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(p, allocateLength);

	p->DeviceObject = DeviceObject;
	p->LspSessionBuffer = LspIopOffsetOf(*LspIoSession, sizeof(LSP_IO_SESSION));

	p->LspLockCleanupContext = LspLockCleanupAllocate(DeviceObject);
	if (NULL == p->LspLockCleanupContext)
	{
		ExFreePoolWithTag(p, LSP_IO_SESSION_TAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (InternalDataBufferLength > 0)
	{
		p->InternalBuffer = ExAllocatePoolWithTag(
			NonPagedPool,
			InternalDataBufferLength,
			LSP_IO_DATA_TAG);
		if (NULL == p->InternalBuffer)
		{
			LspLockCleanupFree(p->LspLockCleanupContext);
			ExFreePoolWithTag(p, LSP_IO_SESSION_TAG);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		p->InternalBufferLength = InternalDataBufferLength;
	}

	p->SessionState = LSP_IOS_STATE_ALLOCATED;

	*LspIoSession = p;

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

	if (LspIoSession->InternalBuffer)
	{
		ExFreePoolWithTag(LspIoSession->InternalBuffer, LSP_IO_DATA_TAG);
		LspIoSession->InternalBuffer = NULL;
		LspIoSession->InternalBufferLength = 0;
	}

	//
	// Explicitly mark the memory as deallocated to help debugging
	//
	LspIoSession->SessionState = LSP_IOS_STATE_DEALLOCATED;

	LspLockCleanupFree(LspIoSession->LspLockCleanupContext);
	ExFreePoolWithTag(LspIoSession, LSP_IO_SESSION_TAG);
}

//////////////////////////////////////////////////////////////////////////
//
// Session Initialization Functions
//
//////////////////////////////////////////////////////////////////////////

NTSTATUS
LSPIOCALL
LspIoInitializeSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS DeviceAddress,
	__in_ecount(LocalAddressCount) PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount,
	__in const lsp_login_info_t* LspLoginInfo)
{
	NTSTATUS status;
	ULONG index;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	ASSERT(LSP_IOS_STATE_ALLOCATED == LspIoSession->SessionState);

	//
	// Tdi Timeout Context
	//
	KeInitializeEvent(
		&LspIoSession->TdiTimeOutEvent,
		NotificationEvent,
		FALSE);

	//
	// Tdi Error Context
	//
	KeInitializeEvent(
		&LspIoSession->TdiErrorEvent,
		NotificationEvent,
		FALSE);

//////#ifdef LSPIO_IMP_USE_EXTERNAL_ENCODE
//////	lsp_set_options(
//////		LspIoSession->LspHandle, 
//////		LSP_SO_USE_EXTERNAL_DATA_ENCODE);
//////#endif

	SetRelativeDueTimeInSecond(
		&LspIoSession->ReceiveTimeOut,
		LSPIO_DEFAULT_RECEIVE_TIMEOUT_SEC);

	SetRelativeDueTimeInSecond(
		&LspIoSession->SendTimeOut,
		LSPIO_DEFAULT_SEND_TIMEOUT_SEC);

	LspIoSession->ConnectWorkItem = IoAllocateWorkItem(
		LspIoSession->DeviceObject);

	if (NULL == LspIoSession->ConnectWorkItem)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	status = LspFastConnectInitialize(
		&LspIoSession->LspFastConnectContext,
		LspIoSession->DeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"LspFastConnectInitialize failed, status=%X\n", status);

		IoFreeWorkItem(LspIoSession->ConnectWorkItem);
		LspIoSession->ConnectWorkItem = NULL;

		return status;
	}

	LspIoSession->DeviceAddress = *DeviceAddress;

	LspIoResetLoginInfo(
		LspIoSession,
		LspLoginInfo);

	LspIoResetLocalAddressList(
		LspIoSession, 
		LocalAddressList, 
		LocalAddressCount);

	for (index = 0; index < countof(LspIoSession->InternalRequests); ++index)
	{
		LspIoSession->InternalRequests[index].RequestType = LSP_IO_RESERVED;
	}

	LspIoSession->SessionState = LSP_IOS_STATE_INITIALIZED;

	return STATUS_SUCCESS;
}

VOID
LSPIOCALL
LspIoUninitializeSession(
	PLSP_IO_SESSION LspIoSession)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p\n", LspIoSession);

	ASSERT(LSP_IOS_STATE_INITIALIZED == LspIoSession->SessionState);
	ASSERT(NULL == LspIoSession->LspActiveConnection);

	IoFreeWorkItem(LspIoSession->ConnectWorkItem);
	LspIoSession->ConnectWorkItem = NULL;

	LspIoSession->SessionState = LSP_IOS_STATE_ALLOCATED;
}

VOID
LSPIOCALL
LspIoResetLoginInfo(
	__in PLSP_IO_SESSION IoSession,
	__in const lsp_login_info_t* LoginInfo)
{
	IoSession->LspLoginInfo = *LoginInfo;
}

VOID
LSPIOCALL
LspIoResetLocalAddressList(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount)
{
	ULONG index;

	LspIoSession->LocalAddressCount = min(LocalAddressCount, LSP_MAX_ADDRESS_COUNT);

	for (index = 0; index < LspIoSession->LocalAddressCount; ++index)
	{
		LspIoSession->LocalAddressList[index] = LocalAddressList[index];
	}
}

FORCEINLINE
VOID
LSPIOCALL
LspIopCompleteRequest(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest)
{
	PLSP_IO_REQUEST_COMPLETION completionRoutine;
	PVOID completionContext;

	completionRoutine = LspIoRequest->CompletionRoutine;
	completionContext = LspIoRequest->CompletionContext;

	(*completionRoutine)(LspIoSession, LspIoRequest, completionContext);
}

NTSTATUS
LSPIOCALL
LspIoRequestIo(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;

	switch (IoRequest->RequestType)
	{
	case LSP_IO_START_SESSION:
		status = LspIopStartSession(LspIoSession, IoRequest);
		break;

	case LSP_IO_STOP_SESSION:
		status = LspIopStopSession(LspIoSession, IoRequest);
		break;

	case LSP_IO_PAUSE_SESSION:
		status = LspIopPauseSession(LspIoSession, IoRequest);
		break;

	case LSP_IO_RESTART_SESSION:
		status = LspIopRestartSession(LspIoSession, IoRequest);
		break;

	case LSP_IO_CONNECT:
		status = LspIopFastConnect(LspIoSession, IoRequest);
		break;

	case LSP_IO_DISCONNECT:
		status = LspIopDisconnect(LspIoSession, IoRequest);
		break;

	case LSP_IO_SET_TDI_EVENT_HANDLER:
		status = LspIopSetTdiEventHandler(LspIoSession, IoRequest);
		break;

	case LSP_IO_UPDATE_CONNECTION_STATS:
		status = LspIopUpdateConnectionStats(LspIoSession, IoRequest);
		break;

	case LSP_IO_LOGIN:
		status = LspIopLogin(LspIoSession, IoRequest);
		break;

	case LSP_IO_LOGOUT:
		status = LspIopLogout(LspIoSession, IoRequest);
		break;

	case LSP_IO_READ:
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:
	case LSP_IO_VERIFY:
		status = LspIopReadWriteVerify(LspIoSession, IoRequest);
		break;

	case LSP_IO_FLUSH_CACHE:
		status = LspIopFlushCache(LspIoSession, IoRequest);
		break;

	case LSP_IO_HANDSHAKE:
		status = LspIopHandshake(LspIoSession, IoRequest);
		break;

	case LSP_IO_LSP_COMMAND:

		LspIoSession->SessionState = LSP_IOS_TRANSITION_EXECUTING_IO;

		status = LspIoLightSessionRequest(
			&LspIoSession->LspIoLightSession,
			&IoRequest->LspRequestPacket,
			LspIopLspIoLightCompletion,
			IoRequest);

		break;
	default:
		ASSERTMSG("Invalid Request Type", FALSE);
	}

	return status;
}

VOID
LspIopFastConnectWorkItem(
    __in PDEVICE_OBJECT  DeviceObject,
    __in PVOID Context)
{
	PLSP_IO_REQUEST IoRequest = (PLSP_IO_REQUEST) Context;
	PLSP_IO_SESSION LspIoSession = IoRequest->Internal.LspIoSession;
	LspIopFastConnect(LspIoSession, IoRequest);
}

NTSTATUS
LSPIOCALL
LspIopFastConnect(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	LspIoSession->SessionState = LSP_IOS_TRANSITION_CONNECTING;
	IoRequest->Internal.LspIoSession = LspIoSession;

	//
	// LspFastConnectConnect must be called at PASSIVE_LEVEL
	//

	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
	{
		IoQueueWorkItem(
			LspIoSession->ConnectWorkItem,
			LspIopFastConnectWorkItem,
			DelayedWorkQueue,
			IoRequest);
		return STATUS_PENDING;
	}
	else
	{
		return LspFastConnectConnect(
			&LspIoSession->LspFastConnectContext,
			&LspIoSession->DeviceAddress,
			LspIoSession->LocalAddressList,
			LspIoSession->LocalAddressCount,
			LspIopGetTimeoutZeroAsNull(&IoRequest->Context.Connect.Timeout),
			LspIopFastConnectCompletion,
			IoRequest);
	}
}

VOID
LSPIOCALL
LspIopFastConnectCompletion(
	__in PLSP_FAST_CONNECT_CONTEXT LspFastConnectContext,
	__in PLSP_CONNECT_CONTEXT LspActiveConnection,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context)
{
	NTSTATUS status;
	IO_STATUS_BLOCK ioStatus;
	PLSP_IO_SESSION LspIoSession;
	PLSP_IO_REQUEST IoRequest;
	PLSP_IO_REQUEST InternalIoRequest;

	IoRequest = (PLSP_IO_REQUEST) Context;
	LspIoSession = CONTAINING_RECORD(
		LspFastConnectContext,
		LSP_IO_SESSION,
		LspFastConnectContext);

	ASSERT(NULL != LspIoSession);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"LspIoFastConnectCompletion, LspIoSession=%p, IoStatus=%x\n", 
		LspIoSession, IoStatus->Status);

	//
	// After Post Initialization, iostatus may be changed to fail status
	//

	ioStatus = *IoStatus;

	if (!NT_SUCCESS(IoStatus->Status))
	{
		IoRequest->IoStatus = *IoStatus;
		LspIoSession->SessionState = LSP_IOS_STATE_INITIALIZED;
		LspIopCompleteRequest(LspIoSession, IoRequest);

		return;
	}

	//
	// Post connect initialization
	//
	status = LspIopPostConnectInitialization(
		LspIoSession,
		LspActiveConnection);

	if (!NT_SUCCESS(status))
	{
		IoRequest->IoStatus.Status = status;
		IoRequest->IoStatus.Information = 0;

		LspIoSession->SessionState = LSP_IOS_STATE_INITIALIZED;
		LspIopCompleteRequest(LspIoSession, IoRequest);

		return;
	}

	//
	// Set Disconnect Handler
	//

	InternalIoRequest = LspIopAcquireInternalIoRequest(LspIoSession);
	RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
	InternalIoRequest->OriginalRequest = IoRequest;
	InternalIoRequest->RequestType = LSP_IO_SET_TDI_EVENT_HANDLER;
	InternalIoRequest->CompletionRoutine = LspIopSetDisconnectEventHandlerCompletion;
	InternalIoRequest->CompletionContext = NULL;
	InternalIoRequest->Context.SetTdiEventHandler.EventId = TDI_EVENT_DISCONNECT;
	InternalIoRequest->Context.SetTdiEventHandler.EventHandler = LspIopClientEventDisconnect;
	InternalIoRequest->Context.SetTdiEventHandler.EventContext = LspIoSession;

	status = LspIopSetTdiEventHandler(
		LspIoSession,
		InternalIoRequest);
}

VOID
LSPIOCALL
LspIopSetDisconnectEventHandlerCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalIoRequest;

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"LspIoSetDisconnectEventHandlerCompletion, LspIoSession=%p, IoStatus=%x\n", 
		LspIoSession, InternalIoRequest->IoStatus.Status);

	LspIoSession->SessionState = LSP_IOS_STATE_CONNECTED;

	ASSERT(NULL != InternalIoRequest->OriginalRequest);
	OriginalIoRequest = InternalIoRequest->OriginalRequest;
	OriginalIoRequest->IoStatus = InternalIoRequest->IoStatus;
	LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
	LspIopCompleteRequest(LspIoSession, OriginalIoRequest);
}

NTSTATUS
LSPIOCALL
LspIopPostConnectInitialization(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_CONNECT_CONTEXT LspActiveConnection)
{
	NTSTATUS status;
	ULONG i;
	PXTDI_IO_SESSION tdiIoSession; 

	LspIoSession->LspActiveConnection = LspActiveConnection;

	status = LspIoLightSessionCreate(
		&LspIoSession->LspIoLightSession,
		LspIoSession->LspActiveConnection->ConnectionFileObject,
		LspIoSession->LspActiveConnection->ConnectionDeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"LspIoLightSessionCreate failed, status=0x%x\n", status);

		LspConnectCleanup(LspActiveConnection);
		LspIoSession->LspActiveConnection = NULL;

		return status;
	}

	//
	// NonLspTdiIoSession Initialize
	//

	status = xTdiIoSessionCreate(
		&LspIoSession->NonLspTdiIoSession,
		LspActiveConnection->ConnectionDeviceObject->StackSize + 1);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"TdiIoSessionInitialize failed, status=0x%x\n", status);

		LspIoLightSessionDelete(&LspIoSession->LspIoLightSession);
		LspConnectCleanup(LspActiveConnection);
		LspIoSession->LspActiveConnection = NULL;

		return status;
	}

	//
	// LPX Control TDI Io Session
	//

	status = xTdiIoSessionCreate(
		&LspIoSession->LpxControlTdiIoSession,
		LspActiveConnection->ConnectionDeviceObject->StackSize + 1);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_ERROR,
			"TdiIoSessionInitialize failed, status=0x%x\n", status);
		
		xTdiIoSessionDelete(&LspIoSession->NonLspTdiIoSession);

		LspIoLightSessionDelete(&LspIoSession->LspIoLightSession);
		LspConnectCleanup(LspActiveConnection);
		LspIoSession->LspActiveConnection = NULL;

		return status;
	}

	//xTdiIoSessionSetCompletionRoutine(
	//	&LspIoSession->LpxControlTdiIoSession,
	//	LspIopQueryPerformanceDataCompletion,
	//	LspIoSession);

	return STATUS_SUCCESS;;
}

VOID
LspIopPostConnectUninitializeWorkItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context)
{
	PLSP_IO_REQUEST AssociatedIoRequest;
	PLSP_IO_SESSION LspIoSession;

	UNREFERENCED_PARAMETER(DeviceObject);

	AssociatedIoRequest = (PLSP_IO_REQUEST) Context;
	LspIoSession = AssociatedIoRequest->Internal.LspIoSession;

	xTdiIoSessionDelete(&LspIoSession->LpxControlTdiIoSession);
	xTdiIoSessionDelete(&LspIoSession->NonLspTdiIoSession);

	LspIoLightSessionDelete(&LspIoSession->LspIoLightSession);

	ASSERT(NULL != LspIoSession->LspActiveConnection);
	LspConnectCleanup(LspIoSession->LspActiveConnection);
	LspIoSession->LspActiveConnection = NULL;

	LspIopCompleteRequest(LspIoSession, AssociatedIoRequest);
}

VOID
LSPIOCALL
LspIopPostConnectUninitialize(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST AssociatedIoRequest)
{
	AssociatedIoRequest->Internal.LspIoSession = LspIoSession;
	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
	{
		IoQueueWorkItem(
			LspIoSession->ConnectWorkItem,
			LspIopPostConnectUninitializeWorkItem,
			DelayedWorkQueue,
			AssociatedIoRequest);
	}
	else
	{
		LspIopPostConnectUninitializeWorkItem(
			NULL,
			AssociatedIoRequest);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// TDI Connections
//
//////////////////////////////////////////////////////////////////////////

VOID
LspIopSetTdiEventHandlerCompletion(
	__in PIRP Irp,
	__in PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LSPIOCALL
LspIopSetTdiEventHandler(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;
	PXTDI_IO_SESSION tdiIoSession;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p, SessionState=%d\n", 
		LspIoSession, LspIoSession->SessionState);

	IoRequest->Internal.LspIoSession = LspIoSession;
	tdiIoSession = &LspIoSession->NonLspTdiIoSession;
	xTdiIoSessionReset(tdiIoSession);
	xTdiIoSessionSetCompletionRoutine(
		tdiIoSession,
		LspIopSetTdiEventHandlerCompletion,
		IoRequest);

	status = xTdiSetEventHandlerEx(
		LspIoSession->NonLspTdiIoSession.Irp,
		LspIoSession->LspActiveConnection->AddressDeviceObject,
		LspIoSession->LspActiveConnection->AddressFileObject,
		IoRequest->Context.SetTdiEventHandler.EventId,
		IoRequest->Context.SetTdiEventHandler.EventHandler,
		IoRequest->Context.SetTdiEventHandler.EventContext,
		&LspIoSession->NonLspTdiIoSession.Overlapped);

	return status;
}

VOID
LspIopSetTdiEventHandlerCompletion(
	__in PIRP Irp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	PLSP_IO_SESSION LspIoSession;
	PLSP_IO_REQUEST IoRequest;

	IoRequest = (PLSP_IO_REQUEST) Overlapped->UserContext;
	LspIoSession = IoRequest->Internal.LspIoSession;

	LspIopCompleteRequest(LspIoSession, IoRequest);
}

VOID
LspIopTdiDisconnectCompletion(
	__in PIRP Irp,
	__in PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LSPIOCALL
LspIopDisconnect(
	PLSP_IO_SESSION LspIoSession,
	PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;
	PXTDI_IO_SESSION tdiIoSession;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_VERBOSE,
		"LspIoSession=%p, SessionState=%d\n", 
		LspIoSession, LspIoSession->SessionState);

	LspIoSession->SessionState = LSP_IOS_TRANSITION_DISCONNECTING;

	IoRequest->Internal.LspIoSession = LspIoSession;

	tdiIoSession = &LspIoSession->NonLspTdiIoSession;
	xTdiIoSessionReset(tdiIoSession);
	xTdiIoSessionSetCompletionRoutine(
		tdiIoSession,
		LspIopTdiDisconnectCompletion,
		IoRequest);

	status = xTdiDisconnectEx(
		tdiIoSession->Irp,
		LspIoSession->LspActiveConnection->ConnectionDeviceObject,
		LspIoSession->LspActiveConnection->ConnectionFileObject,
		IoRequest->Context.Disconnect.DisconnectFlags,
		LspIopGetTimeoutZeroAsNull(&IoRequest->Context.Disconnect.Timeout),
		NULL,
		NULL, 
		&tdiIoSession->Overlapped);

	return status;
}

VOID
LspIopTdiDisconnectCompletion(
	__in PIRP Irp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	PLSP_IO_SESSION LspIoSession;
	PLSP_IO_REQUEST IoRequest;

	IoRequest = (PLSP_IO_REQUEST) Overlapped->UserContext;
	LspIoSession = IoRequest->Internal.LspIoSession;

	//
	// Regardless of success of Disconnect, state becomes LSP_IOS_STATE_INITIALIZED
	//

	LspIoSession->SessionState = LSP_IOS_STATE_INITIALIZED;

	LspIopPostConnectUninitialize(
		LspIoSession, IoRequest);
}

VOID
LspIopUpdateConnectionStatsTdiIoCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LSPIOCALL
LspIopUpdateConnectionStats(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;
	PXTDI_IO_SESSION tdiIoSession;

#ifdef LSPIO_USE_FAST_IO
	//
	// This function is mostly called at DISPATCH_LEVEL.
	// FastIoDispatch routines are supported to be called at PASSIVE_LEVEL,
	// so most drivers allocate text section in PAGED_SECTION
	// which will result page fault when we call FastIoDeviceControl
	// routine.
	//
	// So we cannot make use of FastIoDispatch routines here
	// if we follow this rule.
	//
	// However, it is still possible. Assume that we only use LPX,
	// and we assign LpxFastIoDispatch routines in non-paged section,
	// it is okay then.
	//
	PFAST_IO_DISPATCH fastIoDispatch;
#endif

	tdiIoSession = &LspIoSession->LpxControlTdiIoSession;
	xTdiIoSessionReset(tdiIoSession);

	IoRequest->Internal.LspIoSession = LspIoSession;

	xTdiIoSessionSetCompletionRoutine(
		tdiIoSession,
		LspIopUpdateConnectionStatsTdiIoCompletion,
		IoRequest);

#ifdef LSPIO_USE_FAST_IO

	fastIoDispatch = LspIoSession->LspActiveConnection->
		ConnectionDeviceObject->DriverObject->FastIoDispatch;

	if (fastIoDispatch && fastIoDispatch->FastIoDeviceControl)
	{
		BOOLEAN processed;
		TDI_REQUEST_KERNEL_QUERY_INFORMATION query;

		query.QueryType = LPXTDI_QUERY_CONNECTION_TRANSSTAT;
		query.RequestConnectionInformation = NULL;

		processed = (*fastIoDispatch->FastIoDeviceControl)(
			LspIoSession->LspActiveConnection->ConnectionFileObject,
			FALSE,
			&query,
			sizeof(TDI_REQUEST_KERNEL_QUERY_INFORMATION),
			&IoRequest->Internal.UpdateConnectionStats.PerfCounter,
			sizeof(LPX_TDI_PERF_COUNTER),
			TDI_QUERY_INFORMATION,
			&IoRequest->IoStatus,
			LspIoSession->LspActiveConnection->ConnectionDeviceObject);

		if (processed)
		{
			if (tdiIoSession->Overlapped.CompletionRoutine)
			{
				(*tdiIoSession->Overlapped.CompletionRoutine)(
					tdiIoSession->Irp,
					&tdiIoSession->Overlapped);
			}
			return IoRequest->IoStatus.Status;
		}
	}
#endif

	status = xTdiQueryInformationEx(
		tdiIoSession->Irp,
		LspIoSession->LspActiveConnection->ConnectionDeviceObject,
		LspIoSession->LspActiveConnection->ConnectionFileObject,
		LPXTDI_QUERY_CONNECTION_TRANSSTAT,
		&IoRequest->Internal.UpdateConnectionStats.PerfCounter,
		sizeof(LPX_TDI_PERF_COUNTER),
		&tdiIoSession->Overlapped);

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
LspIopUpdateConnectionStatsTdiIoCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	static const ULONG MaxWriteByteChunks[] = {
		128 * 512, 64 * 512, 43 * 512, 32 * 512,
		 26 * 512, 22 * 512, 19 * 512, 16 * 512,
		 13 * 512, 11 * 512, 10 * 512,  8 * 512,
		  6 * 512,  4 * 512,  2 * 512
	};

	PXTDI_IO_SESSION TdiData;
	PLSP_IO_SESSION LspIoSession;
	PLSP_IO_REQUEST IoRequest;
	PLPX_TDI_PERF_COUNTER perfCounter;
	
	TdiData = xTdiIoSessionRetrieveFromOverlapped(Overlapped);
	IoRequest = (PLSP_IO_REQUEST) Overlapped->UserContext;
	LspIoSession = IoRequest->Internal.LspIoSession;
	perfCounter = &IoRequest->Internal.UpdateConnectionStats.PerfCounter;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_INFORMATION,
		"LSPIO: LspIopUpdateConnectionStatsTdiIoCompletion, "
		"TransferIrp=%p, Overlapped=%p, Status=%x\n", 
		TransferIrp, Overlapped, Overlapped->IoStatus.Status);

	if (!NT_SUCCESS(Overlapped->IoStatus.Status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
			"LSPIO: LspIopUpdateConnectionStatsTdiIoCompletion, completed with error: "
			"TransferIrp=%p, Overlapped=%p, Status=%x\n", 
			TransferIrp, Overlapped, Overlapped->IoStatus.Status);

		IoRequest->IoStatus = Overlapped->IoStatus;
		LspIopCompleteRequest(LspIoSession, IoRequest);
		return;
	}

	ASSERT(LSP_IO_READ == IoRequest->Context.UpdateConnectionStats.IoType ||
		LSP_IO_WRITE == IoRequest->Context.UpdateConnectionStats.IoType ||
		LSP_IO_WRITE_FUA == IoRequest->Context.UpdateConnectionStats.IoType);

	switch (IoRequest->Context.UpdateConnectionStats.IoType)
	{
	case LSP_IO_READ:
		// Consider only PacketLossCounter
		if (perfCounter->PacketLossCounter > 0)
		{
			ULONG maxReadBytes = LspIoSession->MaximumLspReadBytes;

			LspIoSession->NoLossReadBytes = 0;
			LspIoSession->MaximumLspReadBytes = max(
				LSP_IO_MINIMUM_READ_BYTES, 
				maxReadBytes * 3 / 4);

			NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_WARNING,
				"  LspReadBytes,--,%u,%u,%u,-,%u, "
				"%d->%d blocks, PacketLoss=%d\n",
				LspIoSession->LspIoCount,
				maxReadBytes / 512,
				LspIoSession->MaximumLspReadBytes / 512,
				perfCounter->PacketLossCounter,
				maxReadBytes / 512,
				LspIoSession->MaximumLspReadBytes / 512,
				perfCounter->PacketLossCounter);
		}
		else
		{
			LspIoSession->NoLossReadBytes += 
				IoRequest->Context.UpdateConnectionStats.IoBytes;

			if (LspIoSession->NoLossReadBytes > 
				LspIoSession->MaximumLspReadBytes * LSP_IO_STABLE_RX_COUNT_THRESHOLD)
			{
				ULONG oldMaxLspReadBytes = LspIoSession->MaximumLspReadBytes;

				if (LspIoSession->MaximumLspReadBytes < LspIoSession->MaximumLspTransferBytes)
				{
					LspIoSession->MaximumLspReadBytes = min(
						oldMaxLspReadBytes + LSP_IO_RX_INCREMENT_BYTES,
						LspIoSession->MaximumLspTransferBytes);

					NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_WARNING,
						"  LspReadBytes,++,%u,%u,%u,%u,-, "
						"%u->%u blocks, NoLossReadBlocks=%u\n",
						LspIoSession->LspIoCount,
						oldMaxLspReadBytes / 512,
						LspIoSession->MaximumLspReadBytes / 512,
						LspIoSession->NoLossReadBytes / 512,
						oldMaxLspReadBytes / 512,
						LspIoSession->MaximumLspReadBytes / 512,
						LspIoSession->NoLossReadBytes / 512);
				}

				LspIoSession->NoLossReadBytes = 0;
			}
		}
		break;
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:
		// Consider only RetransmitCounter
		if (perfCounter->RetransmitCounter > 0)
		{
			ULONG i, oldMaxWriteBytes;

			LspIoSession->NoLossWriteBytes= 0;
			oldMaxWriteBytes = LspIoSession->MaximumLspWriteBytes;

			for (i = 0; i + 1 < countof(MaxWriteByteChunks); ++i)
			{
				if (oldMaxWriteBytes >= MaxWriteByteChunks[i])
				{
					LspIoSession->MaximumLspWriteBytes = MaxWriteByteChunks[i+1];
					break;
				}
			}

			NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_WARNING,
				"  LspWriteBytes,--,%u,%u,%u,,%u, "
				"%u->%u blocks, Retransmits=%u\n",
				LspIoSession->LspIoCount,
				oldMaxWriteBytes / 512,
				LspIoSession->MaximumLspWriteBytes / 512,
				perfCounter->RetransmitCounter,
				oldMaxWriteBytes / 512,
				LspIoSession->MaximumLspWriteBytes / 512,
				perfCounter->RetransmitCounter);

			ASSERT(LspIoSession->MaximumLspWriteBytes <= 
				LspIoSession->MaximumLspTransferBytes);
		}
		else
		{
			LspIoSession->NoLossWriteBytes += 
				IoRequest->Context.UpdateConnectionStats.IoBytes;

			if (LspIoSession->NoLossWriteBytes > 
				LspIoSession->MaximumLspWriteBytes * LSP_IO_STABLE_TX_COUNT_THRESHOLD)
			{
				ULONG i, oldMaxLspWriteBytes;
				
				oldMaxLspWriteBytes = LspIoSession->MaximumLspWriteBytes;

				if (oldMaxLspWriteBytes < LspIoSession->MaximumLspTransferBytes)
				{
					for (i = 1; i < countof(MaxWriteByteChunks); ++i)
					{
						if (oldMaxLspWriteBytes >= MaxWriteByteChunks[i])
						{
							LspIoSession->MaximumLspWriteBytes = MaxWriteByteChunks[i-1];
							break;
						}
					}

					NdasPortTrace(NDASPORT_LSPIO_PERF, TRACE_LEVEL_WARNING,
						"  LspWriteBytes,++,%u,%u,%u,%u,, "
						"%u->%u blocks, NoLossWrite=%u blocks\n",
						LspIoSession->LspIoCount,
						oldMaxLspWriteBytes / 512,
						LspIoSession->MaximumLspWriteBytes / 512,
						LspIoSession->NoLossWriteBytes / 512,
						oldMaxLspWriteBytes / 512,
						LspIoSession->MaximumLspWriteBytes / 512,
						LspIoSession->NoLossWriteBytes / 512);
				}

				LspIoSession->NoLossWriteBytes = 0;
			}
		}
		break;
	DEFAULT_UNREACHABLE;
	}
	
	IoRequest->IoStatus.Status = STATUS_SUCCESS;
	IoRequest->IoStatus.Information = 0;
	LspIopCompleteRequest(LspIoSession, IoRequest);
}

lsp_handle_t
LSPIOCALL
LspIoGetLspHandle(
	__in PLSP_IO_SESSION IoSession)
{
	return IoSession->LspIoLightSession.LspHandle;
}

VOID
LSPIOCALL
LspIopLspIoLightCompletion(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context)
{
	PLSP_IO_SESSION LspIoSession;
	PLSP_IO_REQUEST LspIoRequest;

	LspIoSession = CONTAINING_RECORD(
		LspIoLightSession, LSP_IO_SESSION, LspIoLightSession);

	LspIoRequest = (PLSP_IO_REQUEST) Context;
	LspIoRequest->IoStatus = *IoStatus;

	//
	// State Transition
	//
	switch (LspIoSession->SessionState)
	{
	case LSP_IOS_TRANSITION_LOGGING_IN:
		LspIoSession->SessionState = NT_SUCCESS(IoStatus->Status) ? 
			LSP_IOS_STATE_LOGGED_IN : LSP_IOS_STATE_CONNECTED;
		{
			const lsp_hardware_data_t* LspHardwareData = lsp_get_hardware_data(
				LspIoSession->LspIoLightSession.LspHandle);
			LspIoSession->MaximumLspTransferBytes = 
				LspHardwareData->maximum_transfer_blocks * 512;
			if (0 == LspIoSession->MaximumLspReadBytes)
			{
				LspIoSession->MaximumLspReadBytes = 
					LspIoSession->MaximumLspTransferBytes;
			}
			if (0 == LspIoSession->MaximumLspWriteBytes)
			{
				LspIoSession->MaximumLspWriteBytes = 
					LspIoSession->MaximumLspTransferBytes;
			}
		}
		break;
	case LSP_IOS_TRANSITION_LOGGING_OUT:
		LspIoSession->SessionState = NT_SUCCESS(IoStatus->Status) ? 
			LSP_IOS_STATE_CONNECTED : LSP_IOS_STATE_CONNECTED;
		break;
	case LSP_IOS_TRANSITION_HANDSHAKING:
		LspIoSession->SessionState = NT_SUCCESS(IoStatus->Status) ? 
			LSP_IOS_STATE_IO_READY : LSP_IOS_STATE_LOGGED_IN;
		break;
	case LSP_IOS_TRANSITION_EXECUTING_IO:
		LspIoSession->SessionState = NT_SUCCESS(IoStatus->Status) ? 
			LSP_IOS_STATE_IO_READY : LSP_IOS_STATE_IO_READY;
		break;
	default:
		ASSERTMSG("LspIopLspIoCompletion SessionState is invalid", FALSE);
	}

	LspIopCompleteRequest(LspIoSession, LspIoRequest);
}

VOID
LSPIOCALL
LspIopSynchronousRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

NTSTATUS
LSPIOCALL
LspIoRequestIoSynchronous(
	__in PLSP_IO_SESSION IoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;
	KEVENT completionEvent;

	PLSP_IO_REQUEST_COMPLETION oldCompletionRoutine;
	PVOID oldCompletionContext;

	KeInitializeEvent(
		&completionEvent,
		NotificationEvent,
		FALSE);

	oldCompletionRoutine = IoRequest->CompletionRoutine;
	oldCompletionContext = IoRequest->CompletionContext;

	IoRequest->CompletionRoutine = LspIopSynchronousRequestCompletion;
	IoRequest->CompletionContext = &completionEvent;

	status = LspIoRequestIo(IoSession, IoRequest);

	KeWaitForSingleObject(
		&completionEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	IoRequest->CompletionRoutine = oldCompletionRoutine;
	IoRequest->CompletionContext = oldCompletionContext;

	return IoRequest->IoStatus.Status;
}

VOID
LSPIOCALL
LspIopSynchronousRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context)
{
	PKEVENT completionEvent;

	UNREFERENCED_PARAMETER(LspIoSession);
	UNREFERENCED_PARAMETER(LspIoRequest);

	completionEvent = (PKEVENT) Context;
	KeSetEvent(completionEvent, IO_NO_INCREMENT, FALSE);
}

NTSTATUS
LSPIOCALL
LspIopLogin(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopLogin: LspIoSession=%p, sessionState=%d\n", 
		LspIoSession, LspIoSession->SessionState);

	LspIoSession->SessionState = LSP_IOS_TRANSITION_LOGGING_IN;

	lsp_build_login(
		&IoRequest->LspRequestPacket,
		LspIoSession->LspIoLightSession.LspHandle,
		&LspIoSession->LspLoginInfo);

	status = LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&IoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		IoRequest);

	return status;
}

NTSTATUS
LSPIOCALL
LspIopLogout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopLogout: LspIoSession=%p, sessionState=%d\n", 
		LspIoSession, LspIoSession->SessionState);

	LspIoSession->SessionState = LSP_IOS_TRANSITION_LOGGING_OUT;

	lsp_build_logout(
		&IoRequest->LspRequestPacket,
		LspIoSession->LspIoLightSession.LspHandle);

	status = LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&IoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		IoRequest);

	return status;
}

NTSTATUS
LSPIOCALL
LspIopHandshake(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopHandshake: LspIoSession=%p, sessionState=%d\n", 
		LspIoSession, LspIoSession->SessionState);

	LspIoSession->SessionState = LSP_IOS_TRANSITION_HANDSHAKING;

	lsp_build_ata_handshake(
		&IoRequest->LspRequestPacket,
		LspIoSession->LspIoLightSession.LspHandle);

	status = LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&IoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		IoRequest);

	return status;
}

NTSTATUS
LSPIOCALL
LspIopReadWriteVerify(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	PLSP_IO_REQUEST InternalIoRequest;
	ULONG transferBlocks;
	lsp_status_t lspStatus;
	PVOID writeBuffer;
	const lsp_hardware_data_t* lspHardwareData;

	lspHardwareData = lsp_get_hardware_data(
		LspIoSession->LspIoLightSession.LspHandle);

	InternalIoRequest = LspIopAcquireInternalIoRequest(LspIoSession);
	RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
	InternalIoRequest->OriginalRequest = IoRequest;
	InternalIoRequest->RequestType = IoRequest->RequestType;
	InternalIoRequest->Context.ReadWriteVerify.LogicalBlockAddress = 
		IoRequest->Context.ReadWriteVerify.LogicalBlockAddress;
	InternalIoRequest->Context.ReadWriteVerify.Buffer =
		IoRequest->Context.ReadWriteVerify.Buffer;
	InternalIoRequest->Context.ReadWriteVerify.TransferBlocks =
		IoRequest->Context.ReadWriteVerify.TransferBlocks;
	InternalIoRequest->CompletionRoutine = LspIopReadWriteVerifyCompletion;

	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:
		transferBlocks = min(
			LspIoSession->MaximumLspWriteBytes / 512,
			InternalIoRequest->Context.ReadWriteVerify.TransferBlocks);
		break;
	case LSP_IO_READ:
		transferBlocks = min(
			LspIoSession->MaximumLspReadBytes / 512,
			InternalIoRequest->Context.ReadWriteVerify.TransferBlocks);
		break;
	case LSP_IO_VERIFY:
		transferBlocks = min(
			LspIoSession->MaximumLspTransferBytes / 512,
			InternalIoRequest->Context.ReadWriteVerify.TransferBlocks);
		break;
	}

	//
	// when data_encryption is enabled, we cannot use request buffer
	// directly, but we need to copy the buffer
	//
	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:
		if (lspHardwareData->data_encryption_algorithm)
		{
			size_t bufferedLength = transferBlocks * 512;
			ASSERT(LspIoSession->InternalBufferLength >= bufferedLength);
			RtlCopyMemory(
				LspIoSession->InternalBuffer,
				InternalIoRequest->Context.ReadWriteVerify.Buffer,
				bufferedLength);
			InternalIoRequest->Context.ReadWriteVerify.Buffer = 
				LspIoSession->InternalBuffer;
		}
	}

	InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks = transferBlocks;

	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_READ:
		lspStatus = lsp_build_ide_read(
			&InternalIoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			(lsp_large_integer_t*)&InternalIoRequest->
				Context.ReadWriteVerify.LogicalBlockAddress,
			(lsp_uint16_t) transferBlocks,
			InternalIoRequest->Context.ReadWriteVerify.Buffer,
			transferBlocks * 512);
		ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		break;
	case LSP_IO_WRITE:
		lspStatus = lsp_build_ide_write(
			&InternalIoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			(lsp_large_integer_t*)&InternalIoRequest->
				Context.ReadWriteVerify.LogicalBlockAddress,
			(lsp_uint16_t) transferBlocks,
			InternalIoRequest->Context.ReadWriteVerify.Buffer,
			transferBlocks * 512);
		ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		break;
	case LSP_IO_WRITE_FUA:
		if (lsp_get_ata_handshake_data(
			LspIoSession->LspIoLightSession.LspHandle)->support.write_fua)
		{
			lspStatus = lsp_build_ide_write_fua(
				&InternalIoRequest->LspRequestPacket,
				LspIoSession->LspIoLightSession.LspHandle,
				(lsp_large_integer_t*)&InternalIoRequest->
					Context.ReadWriteVerify.LogicalBlockAddress,
				(lsp_uint16_t) transferBlocks,
				InternalIoRequest->Context.ReadWriteVerify.Buffer,
				transferBlocks * 512);
			ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		}
		else
		{
			//
			// Start WRITE step now and WRITE completion calls
			// LspIopPseudoWriteFuaCompletion where VERIFY step
			// starts, whose completion calls LspIopReadWriteVerifyCompletion
			//
			InternalIoRequest->CompletionRoutine = LspIopPseudoWriteFuaCompletion;
			lspStatus = lsp_build_ide_write(
				&InternalIoRequest->LspRequestPacket,
				LspIoSession->LspIoLightSession.LspHandle,
				(lsp_large_integer_t*)&InternalIoRequest->
					Context.ReadWriteVerify.LogicalBlockAddress,
				(lsp_uint16_t) transferBlocks,
				InternalIoRequest->Context.ReadWriteVerify.Buffer,
				transferBlocks * 512);
			ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		}
		break;
	case LSP_IO_VERIFY:
		lspStatus = lsp_build_ide_verify(
			&InternalIoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			(lsp_large_integer_t*)&InternalIoRequest->
				Context.ReadWriteVerify.LogicalBlockAddress,
			(lsp_uint16_t) transferBlocks,
			NULL,
			0);
		ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		break;
	DEFAULT_UNREACHABLE;
	}

	++LspIoSession->LspIoCount;

	LspIoSession->SessionState = LSP_IOS_TRANSITION_EXECUTING_IO;

	return LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&InternalIoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		InternalIoRequest);
}

VOID
LSPIOCALL
LspIopPseudoWriteFuaCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalRequest;
	lsp_status_t lspStatus;

	ASSERT(LSP_IO_WRITE_FUA == InternalIoRequest->RequestType);

	//
	// Once the secondary request fails, it fails the primary request
	//
	if (!NT_SUCCESS(InternalIoRequest->IoStatus.Status))
	{
		OriginalRequest = InternalIoRequest->OriginalRequest;
		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, OriginalRequest);
		return;
	}

	//
	// Replace the completion routine with LspIopReadWriteVerifyCompletion
	// to complete the VERIFY step
	//
	InternalIoRequest->CompletionRoutine = LspIopReadWriteVerifyCompletion;

	lspStatus = lsp_build_ide_verify(
		&InternalIoRequest->LspRequestPacket,
		LspIoSession->LspIoLightSession.LspHandle,
		(lsp_large_integer_t*)&InternalIoRequest->
			Context.ReadWriteVerify.LogicalBlockAddress,
		(lsp_uint16_t) InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks,
		NULL,
		0);

	ASSERT(LSP_STATUS_SUCCESS == lspStatus);

	LspIoSession->SessionState = LSP_IOS_TRANSITION_EXECUTING_IO;

	++LspIoSession->LspIoCount;

	LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&InternalIoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		InternalIoRequest);
}

VOID
LSPIOCALL
LspIopReadWriteVerifyCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalRequest;
	ULONG transferBlocks;
	lsp_status_t lspStatus;
	const lsp_hardware_data_t* lspHardwareData;
	ULONG remainingTransferBlocks;

	if (InternalIoRequest->RequestType == LSP_IO_UPDATE_CONNECTION_STATS)
	{
		PLSP_IO_REQUEST FlowControlIoRequest = InternalIoRequest;
		InternalIoRequest = FlowControlIoRequest->OriginalRequest;
		LspIopReleaseInternalIoRequest(LspIoSession, FlowControlIoRequest);
	}
	else if (LSP_IO_READ == InternalIoRequest->RequestType ||
		LSP_IO_WRITE == InternalIoRequest->RequestType ||
		LSP_IO_WRITE_FUA == InternalIoRequest->RequestType)
	{
		//
		// Update the tx/rx statistics for flow-controls
		//
		PLSP_IO_REQUEST FlowControlIoRequest;
		FlowControlIoRequest = LspIopAcquireInternalIoRequest(LspIoSession);
		RtlZeroMemory(FlowControlIoRequest, sizeof(LSP_IO_REQUEST));
		FlowControlIoRequest->RequestType = LSP_IO_UPDATE_CONNECTION_STATS;
		FlowControlIoRequest->Context.UpdateConnectionStats.IoType = 
			InternalIoRequest->RequestType;
		FlowControlIoRequest->Context.UpdateConnectionStats.IoBytes = 
			InternalIoRequest->Context.ReadWriteVerify.TransferBlocks * 512;
		FlowControlIoRequest->OriginalRequest = InternalIoRequest;
		FlowControlIoRequest->CompletionRoutine = LspIopReadWriteVerifyCompletion;
		LspIoRequestIo(LspIoSession, FlowControlIoRequest);
		return;
	}
	else
	{
		ASSERT(LSP_IO_VERIFY == InternalIoRequest->RequestType);
	}

	ASSERT(InternalIoRequest->Context.ReadWriteVerify.TransferBlocks >=
		InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks);

	remainingTransferBlocks = 
		InternalIoRequest->Context.ReadWriteVerify.TransferBlocks -
		InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks;

	InternalIoRequest->Context.ReadWriteVerify.TransferBlocks = 
		remainingTransferBlocks;

	OriginalRequest = InternalIoRequest->OriginalRequest;

	//
	// Once the secondary request fails, it fails the primary request
	//
	if (!NT_SUCCESS(InternalIoRequest->IoStatus.Status))
	{
		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, OriginalRequest);
		return;
	}

	//
	// InternalIoRequest..TransferLength holds the remaining transfer
	// length. When it goes down to zero, complete the original request.
	//

	if (0 == remainingTransferBlocks)
	{
		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, OriginalRequest);
		return;
	}

	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:
		transferBlocks = min(
			LspIoSession->MaximumLspWriteBytes / 512,
			remainingTransferBlocks);
		break;
	case LSP_IO_READ:
		transferBlocks = min(
			LspIoSession->MaximumLspReadBytes / 512,
			remainingTransferBlocks);
		break;
	case LSP_IO_VERIFY:
		transferBlocks = min(
			LspIoSession->MaximumLspTransferBytes / 512,
			remainingTransferBlocks);
		break;
	}

	//
	// Move the buffer offset
	//
	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_READ:
		(UCHAR*) InternalIoRequest->Context.ReadWriteVerify.Buffer +=
			InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks * 512;
		break;
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:

		lspHardwareData = lsp_get_hardware_data(
			LspIoSession->LspIoLightSession.LspHandle);

		if (!lspHardwareData->data_encryption_algorithm)
		{
			(UCHAR*) InternalIoRequest->Context.ReadWriteVerify.Buffer +=
				InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks * 512;
		}
		else
		{
			size_t bufferedLength;
			PVOID originalBuffer;
			size_t nextOffsetBlocks;
			PVOID nextBuffer;

			bufferedLength = transferBlocks * 512;
			originalBuffer = OriginalRequest->Context.ReadWriteVerify.Buffer;
			nextOffsetBlocks = 
				OriginalRequest->Context.ReadWriteVerify.TransferBlocks -
				remainingTransferBlocks;
			nextBuffer = LspIopOffsetOf(originalBuffer, nextOffsetBlocks * 512);

			ASSERT(LspIoSession->InternalBufferLength >= bufferedLength);

			RtlCopyMemory(
				LspIoSession->InternalBuffer,
				nextBuffer,
				bufferedLength);

			InternalIoRequest->Context.ReadWriteVerify.Buffer = 
				LspIoSession->InternalBuffer;
		}
	}

	//
	// Move the LBA offset
	//
	InternalIoRequest->Context.ReadWriteVerify.LogicalBlockAddress.QuadPart +=
		InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks;

	InternalIoRequest->Internal.ReadWriteVerify.TransferBlocks = transferBlocks;

#if DBG
	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_READ:
	case LSP_IO_WRITE:
	case LSP_IO_WRITE_FUA:
		{
			ULONG originalLbaOffset;

			originalLbaOffset = 
				OriginalRequest->Context.ReadWriteVerify.TransferBlocks -
				remainingTransferBlocks;

			ASSERT(RtlEqualMemory(
				InternalIoRequest->Context.ReadWriteVerify.Buffer,
				LspIopOffsetOf(
					OriginalRequest->Context.ReadWriteVerify.Buffer, 
					originalLbaOffset * 512),
				transferBlocks * 512));
		}
	}
#endif

	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_READ:
		lspStatus = lsp_build_ide_read(
			&InternalIoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			(lsp_large_integer_t*)&InternalIoRequest->Context.ReadWriteVerify.LogicalBlockAddress,
			(lsp_uint16_t) transferBlocks,
			InternalIoRequest->Context.ReadWriteVerify.Buffer,
			transferBlocks * 512);
		ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		break;
	case LSP_IO_WRITE:
		lspStatus = lsp_build_ide_write(
			&InternalIoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			(lsp_large_integer_t*)&InternalIoRequest->
			Context.ReadWriteVerify.LogicalBlockAddress,
			(lsp_uint16_t) transferBlocks,
			InternalIoRequest->Context.ReadWriteVerify.Buffer,
			transferBlocks * 512);
		ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		break;
	case LSP_IO_WRITE_FUA:
		if (lsp_get_ata_handshake_data(
			LspIoSession->LspIoLightSession.LspHandle)->support.write_fua)
		{
			lspStatus = lsp_build_ide_write_fua(
				&InternalIoRequest->LspRequestPacket,
				LspIoSession->LspIoLightSession.LspHandle,
				(lsp_large_integer_t*)&InternalIoRequest->
				Context.ReadWriteVerify.LogicalBlockAddress,
				(lsp_uint16_t) transferBlocks,
				InternalIoRequest->Context.ReadWriteVerify.Buffer,
				transferBlocks * 512);
			ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		}
		else
		{
			//
			// Start WRITE step now and WRITE completion calls
			// LspIopPseudoWriteFuaCompletion where VERIFY step
			// starts, whose completion calls LspIopReadWriteVerifyCompletion
			//
			InternalIoRequest->CompletionRoutine = LspIopPseudoWriteFuaCompletion;
			lspStatus = lsp_build_ide_write(
				&InternalIoRequest->LspRequestPacket,
				LspIoSession->LspIoLightSession.LspHandle,
				(lsp_large_integer_t*)&InternalIoRequest->
				Context.ReadWriteVerify.LogicalBlockAddress,
				(lsp_uint16_t) transferBlocks,
				InternalIoRequest->Context.ReadWriteVerify.Buffer,
				transferBlocks * 512);
			ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		}
		break;
	case LSP_IO_VERIFY:
		lspStatus = lsp_build_ide_verify(
			&InternalIoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			(lsp_large_integer_t*)&InternalIoRequest->Context.ReadWriteVerify.LogicalBlockAddress,
			(lsp_uint16_t) transferBlocks,
			NULL,
			0);
		ASSERT(LSP_STATUS_SUCCESS == lspStatus);
		break;
	DEFAULT_UNREACHABLE;
	}

	++LspIoSession->LspIoCount;

	LspIoSession->SessionState = LSP_IOS_TRANSITION_EXECUTING_IO;

	LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&InternalIoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		InternalIoRequest);

	return;
}

NTSTATUS
LSPIOCALL
LspIopFlushCache(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	NTSTATUS status;
	lsp_ide_register_param_t idereg;
	const lsp_ide_identify_device_data_t* identifyData;

	identifyData = lsp_get_ide_identify_device_data(
		LspIoSession->LspIoLightSession.LspHandle);

	RtlZeroMemory(&idereg, sizeof(lsp_ide_register_param_t));
	idereg.device.s.dev = LspIoSession->LspLoginInfo.unit_no;

	if (identifyData->command_set_support.flush_cache_ext)
	{
		idereg.command.command = LSP_IDE_CMD_FLUSH_CACHE_EXT;

		lsp_build_ide_command(
			&IoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			&idereg,
			NULL,
			NULL);
	}
	else if (identifyData->command_set_support.flush_cache)
	{
		idereg.command.command = LSP_IDE_CMD_FLUSH_CACHE;

		lsp_build_ide_command(
			&IoRequest->LspRequestPacket,
			LspIoSession->LspIoLightSession.LspHandle,
			&idereg,
			NULL,
			NULL);
	}
	else
	{
		IoRequest->IoStatus.Status = STATUS_NOT_SUPPORTED;
		IoRequest->IoStatus.Information = 0;
		LspIopCompleteRequest(LspIoSession, IoRequest);
		return STATUS_NOT_SUPPORTED;
	}

	LspIoSession->SessionState = LSP_IOS_TRANSITION_EXECUTING_IO;

	status = LspIoLightSessionRequest(
		&LspIoSession->LspIoLightSession,
		&IoRequest->LspRequestPacket,
		LspIopLspIoLightCompletion,
		IoRequest);

	return status;
}

VOID
LSPIOCALL
LspIopStartSessionInternalRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

//
// INITIALIZED -> CONNECTING  ->
// CONNECTED   -> LOGGING_IN  ->
// LOGGED_IN   -> HANDSHAKING ->
// IO_READY
//

NTSTATUS
LSPIOCALL
LspIopStartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	PLSP_IO_REQUEST InternalIoRequest;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopStartSession, LspIoSession=%p, state=%d, IoRequest=%p\n",
		LspIoSession, LspIoSession->SessionState, IoRequest);

	InternalIoRequest = LspIopAcquireInternalIoRequest(LspIoSession);
	RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));

	switch (LspIoSession->SessionState)
	{
	case LSP_IOS_STATE_INITIALIZED:
		InternalIoRequest->RequestType = LSP_IO_CONNECT;
		InternalIoRequest->Context.Connect.Timeout = 
			IoRequest->Context.StartSession.ConnectTimeout;
		break;

	case LSP_IOS_STATE_CONNECTED:
		InternalIoRequest->RequestType = LSP_IO_LOGIN;
		break;

	case LSP_IOS_STATE_LOGGED_IN:
		InternalIoRequest->RequestType = LSP_IO_HANDSHAKE;
		break;

	case LSP_IOS_STATE_IO_READY:

		IoRequest->IoStatus.Status = STATUS_SUCCESS;
		IoRequest->IoStatus.Information = 0;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, IoRequest);
		return STATUS_SUCCESS;

	default:
		ASSERTMSG("Invalid state", FALSE);
		IoRequest->IoStatus.Status = STATUS_INTERNAL_ERROR;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, IoRequest);
		return STATUS_INTERNAL_ERROR;
	}

	InternalIoRequest->OriginalRequest = IoRequest;
	InternalIoRequest->CompletionRoutine = 
		LspIopStartSessionInternalRequestCompletion;
	InternalIoRequest->CompletionContext = NULL;

	return LspIoRequestIo(LspIoSession, InternalIoRequest);
}

VOID
LSPIOCALL
LspIopStartSessionStopSessionRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context);

VOID
LSPIOCALL
LspIopStartSessionInternalRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalRequest;

	UNREFERENCED_PARAMETER(Context);

	OriginalRequest = InternalIoRequest->OriginalRequest;

	ASSERT(NULL != OriginalRequest);
	ASSERT(LSP_IO_START_SESSION == OriginalRequest->RequestType);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopStartSession-InternalCompletion, LspIoSession=%p, "
		" InternalIoRequest=%p(%d), OriginalIoRequest=%p(%d)\n",
		LspIoSession,
		InternalIoRequest, InternalIoRequest->RequestType,
		OriginalRequest, OriginalRequest ? OriginalRequest->RequestType : 0);

	if (!NT_SUCCESS(InternalIoRequest->IoStatus.Status))
	{
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"StartSession failure, LspIoSession=%p, State=%d, Status=%X, ignored\n",
			LspIoSession,
			LspIoSession->SessionState,
			InternalIoRequest->IoStatus.Status);

		switch (LspIoSession->SessionState)
		{
		case LSP_IOS_STATE_INITIALIZED:
		case LSP_IOS_STATE_CONNECTED:
		case LSP_IOS_STATE_LOGGED_IN:
			break;
		default:
			ASSERTMSG("Invalid Session State", FALSE);
		}

		//
		// Save the StartSession failure IoStatus
		//
		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		
		RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
		InternalIoRequest->RequestType = LSP_IO_STOP_SESSION;
		InternalIoRequest->OriginalRequest = OriginalRequest;
		InternalIoRequest->CompletionRoutine = 
			LspIopStartSessionStopSessionRequestCompletion;
		InternalIoRequest->CompletionContext = NULL;

		LspIoRequestIo(LspIoSession, InternalIoRequest);
		return;
	}

	switch (LspIoSession->SessionState)
	{
	case LSP_IOS_STATE_CONNECTED:
	case LSP_IOS_STATE_LOGGED_IN:
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopStartSession(LspIoSession, OriginalRequest);
		break;
	case LSP_IOS_STATE_IO_READY:

		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, OriginalRequest);
		return;
	default:
		ASSERTMSG("Invalid Session State", FALSE);
	}
}

VOID
LSPIOCALL
LspIopStartSessionStopSessionRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalRequest;

	//
	// As StartSession error IoStatus is already saved 
	// in OriginalRequest->IoStatus, we do not copy 
	// InternalIoRequest->IoStatus to OriginalRequest->IoStatus
	// 
	OriginalRequest = InternalIoRequest->OriginalRequest;

	ASSERT(NULL != OriginalRequest);
	ASSERT(LSP_IO_START_SESSION == OriginalRequest->RequestType);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopStartSession-InternalStopCompletion, LspIoSession=%p, "
		" InternalIoRequest=%p(%d), OriginalIoRequest=%p(%d)\n",
		LspIoSession,
		InternalIoRequest, InternalIoRequest->RequestType,
		OriginalRequest, OriginalRequest ? OriginalRequest->RequestType : 0);

	LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
	LspIopCompleteRequest(LspIoSession, OriginalRequest);
}

VOID
LSPIOCALL
LspIopStopSessionInternalRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

//
// IO_READY -> LOGGING_OUT ->
// CONNECTED -> DISCONNECTING -> 
// INITIALIZED
//

NTSTATUS
LSPIOCALL
LspIopStopSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	PLSP_IO_REQUEST InternalIoRequest;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopStopSession, LspIoSession=%p, state=%d, IoRequest=%p\n",
		LspIoSession, LspIoSession->SessionState, IoRequest);

	InternalIoRequest = LspIopAcquireInternalIoRequest(LspIoSession);
	RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
	InternalIoRequest->OriginalRequest = IoRequest;

	switch (LspIoSession->SessionState)
	{
	case LSP_IOS_STATE_LOGGED_IN:
	case LSP_IOS_STATE_IO_READY:

		InternalIoRequest->RequestType = LSP_IO_LOGOUT;
		InternalIoRequest->Context.Logout.Reserved;
		InternalIoRequest->CompletionRoutine = 
			LspIopStopSessionInternalRequestCompletion;
		InternalIoRequest->CompletionContext = NULL;

		break;

	case LSP_IOS_STATE_CONNECTED:

		InternalIoRequest->RequestType = LSP_IO_DISCONNECT;
		InternalIoRequest->Context.Disconnect.DisconnectFlags;
		InternalIoRequest->Context.Disconnect.Timeout;
		InternalIoRequest->CompletionRoutine = 
			LspIopStopSessionInternalRequestCompletion;
		InternalIoRequest->CompletionContext = NULL;

		break;

	case LSP_IOS_STATE_INITIALIZED:

		IoRequest->IoStatus.Status = STATUS_SUCCESS;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, IoRequest);
		return STATUS_SUCCESS;

	default:
		ASSERTMSG("Invalid state", FALSE);
		IoRequest->IoStatus.Status = STATUS_INTERNAL_ERROR;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, IoRequest);
		return STATUS_INTERNAL_ERROR;
	}

	return LspIoRequestIo(LspIoSession, InternalIoRequest);
}

VOID
LSPIOCALL
LspIopStopSessionInternalRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalRequest;

	UNREFERENCED_PARAMETER(Context);

	OriginalRequest = InternalIoRequest->OriginalRequest;

	ASSERT(NULL != OriginalRequest);
	ASSERT(LSP_IO_STOP_SESSION == OriginalRequest->RequestType);

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopStopSession-InternalCompletion, LspIoSession=%p, "
		" InternalIoRequest=%p(%d), OriginalIoRequest=%p(%d)\n",
		LspIoSession,
		InternalIoRequest, InternalIoRequest->RequestType,
		OriginalRequest, OriginalRequest ? OriginalRequest->RequestType : 0);

	if (!NT_SUCCESS(InternalIoRequest->IoStatus.Status))
	{
		//
		// Unlike StartSession, stopping failures are ignored
		//
		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_ERROR,
			"StopSession failure, LspIoSession=%p, State=%d, Status=%X, ignored\n",
			LspIoSession,
			LspIoSession->SessionState,
			InternalIoRequest->IoStatus.Status);
	}

	switch (LspIoSession->SessionState)
	{
	case LSP_IOS_STATE_CONNECTED:
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopStopSession(LspIoSession, OriginalRequest);
		break;
	case LSP_IOS_STATE_INITIALIZED:
		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, OriginalRequest);
		return;
	default:
		ASSERTMSG("Invalid Session State in StopSessionCompletion", FALSE);
		return;
	}
}

VOID
LspIopPauseSessionTimerDpc(
	__in PRKDPC Dpc,
	__in PVOID DeferredContext,
	__in PVOID SystemArgument1,
	__in PVOID SystemArgument2);

NTSTATUS
LSPIOCALL
LspIopPauseSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	BOOLEAN alreadyQueued;

	ASSERT(IoRequest->RequestType == LSP_IO_PAUSE_SESSION);

	KeInitializeTimer(
		&IoRequest->Internal.PauseSession.PauseTimer);

	KeInitializeDpc(
		&IoRequest->Internal.PauseSession.PauseTimerDpc,
		LspIopPauseSessionTimerDpc,
		LspIoSession);

	alreadyQueued = KeSetTimer(
		&IoRequest->Internal.PauseSession.PauseTimer,
		IoRequest->Context.PauseSession.PauseTimeout,
		&IoRequest->Internal.PauseSession.PauseTimerDpc);

	ASSERT(!alreadyQueued);

	return STATUS_PENDING;
}

VOID
LspIopPauseSessionTimerDpc(
	__in PRKDPC Dpc,
	__in PVOID DeferredContext,
	__in PVOID SystemArgument1,
	__in PVOID SystemArgument2)
{
	PLSP_IO_SESSION LspIoSession;
	PLSP_IO_REQUEST IoRequest;

	// DeferredContext holds LspIoSession
	// Dpc is contained in LspIoRequest
	LspIoSession = (PLSP_IO_SESSION) DeferredContext;
	IoRequest = CONTAINING_RECORD(
		Dpc, 
		LSP_IO_REQUEST, 
		Internal.PauseSession.PauseTimerDpc);

	NdasPortTrace(NDASATA_INIT, TRACE_LEVEL_INFORMATION,
		"LspIopPauseSessionTimerDpc, LspIoSession=%p, IoRequest=%p\n", 
		LspIoSession, IoRequest);

	LspIopCompleteRequest(
		LspIoSession,
		IoRequest);
}

VOID
LSPIOCALL
LspIopRestartSessionInternalRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

NTSTATUS
LSPIOCALL
LspIopRestartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST IoRequest)
{
	PLSP_IO_REQUEST InternalIoRequest;

	NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
		"LspIopRestartSession, LspIoSession=%p, state=%d, IoRequest=%p\n",
		LspIoSession, LspIoSession->SessionState, IoRequest);

	InternalIoRequest = LspIopAcquireInternalIoRequest(LspIoSession);
	RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
	InternalIoRequest->OriginalRequest = IoRequest;
	InternalIoRequest->RequestType = LSP_IO_STOP_SESSION;
	InternalIoRequest->Context.StopSession.DisconnectTimeout = 
		IoRequest->Context.RestartSession.DisconnectTimeout;
	InternalIoRequest->CompletionRoutine = LspIopRestartSessionInternalRequestCompletion;
	InternalIoRequest->CompletionContext = NULL;

	return LspIoRequestIo(LspIoSession, InternalIoRequest);
}

VOID
LSPIOCALL
LspIopRestartSessionInternalRequestCompletion(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST InternalIoRequest,
	__in PVOID Context)
{
	PLSP_IO_REQUEST OriginalRequest;

	UNREFERENCED_PARAMETER(Context);

	OriginalRequest = InternalIoRequest->OriginalRequest;

	ASSERT(NULL != OriginalRequest);
	ASSERT(LSP_IO_RESTART_SESSION == OriginalRequest->RequestType);

	switch (InternalIoRequest->RequestType)
	{
	case LSP_IO_STOP_SESSION:

		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
			"RestartSession-StopSession completed, LspIoSession=%p, Status=%X\n",
			LspIoSession,
			InternalIoRequest->IoStatus.Status);

		if (0 != OriginalRequest->Context.RestartSession.RestartDelay.QuadPart)
		{
			RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
			InternalIoRequest->OriginalRequest = OriginalRequest;
			InternalIoRequest->RequestType = LSP_IO_PAUSE_SESSION;
			InternalIoRequest->Context.PauseSession.PauseTimeout = 
				OriginalRequest->Context.RestartSession.RestartDelay;
			InternalIoRequest->CompletionRoutine = LspIopRestartSessionInternalRequestCompletion;
			InternalIoRequest->CompletionContext = NULL;
			LspIoRequestIo(LspIoSession, InternalIoRequest);
			return;
		}
		//
		// Go directly to LSP_IO_PAUSE_SESSION if delay is zero.
		//
	case LSP_IO_PAUSE_SESSION:

		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
			"RestartSession-PauseSession completed, LspIoSession=%p, Status=%X\n",
			LspIoSession,
			InternalIoRequest->IoStatus.Status);

		RtlZeroMemory(InternalIoRequest, sizeof(LSP_IO_REQUEST));
		InternalIoRequest->OriginalRequest = OriginalRequest;
		InternalIoRequest->RequestType = LSP_IO_START_SESSION;
		InternalIoRequest->Context.StartSession.ConnectTimeout = 
			OriginalRequest->Context.RestartSession.ConnectTimeout;
		InternalIoRequest->CompletionRoutine = LspIopRestartSessionInternalRequestCompletion;
		InternalIoRequest->CompletionContext = NULL;
		LspIoRequestIo(LspIoSession, InternalIoRequest);

		return;

	case LSP_IO_START_SESSION:

		NdasPortTrace(NDASPORT_LSPIO, TRACE_LEVEL_WARNING,
			"RestartSession-StartSession completed, LspIoSession=%p, Status=%X\n",
			LspIoSession,
			InternalIoRequest->IoStatus.Status);

		OriginalRequest->IoStatus = InternalIoRequest->IoStatus;
		LspIopReleaseInternalIoRequest(LspIoSession, InternalIoRequest);
		LspIopCompleteRequest(LspIoSession, OriginalRequest);
		return;

	default:
		ASSERTMSG("Invalid Request State", FALSE);
		return;
	}
}

//
// Wrapper Functions without IoRequest
//

NTSTATUS
LSPIOCALL
LspIoStartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in_opt PLARGE_INTEGER ConnectionTimeout,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_START_SESSION;
	IoRequest->Context.StartSession.ConnectTimeout = 
		LspIopGetTimeoutNullAsZero(ConnectionTimeout);
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoStopSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_STOP_SESSION;
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoRestartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLARGE_INTEGER RestartDelay,
	__in PLARGE_INTEGER DisconnectTimeout,
	__in PLARGE_INTEGER ConnectTimeout,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_RESTART_SESSION;
	IoRequest->Context.RestartSession.RestartDelay = 
		LspIopGetTimeoutNullAsZero(RestartDelay);
	IoRequest->Context.RestartSession.DisconnectTimeout = 
		LspIopGetTimeoutNullAsZero(DisconnectTimeout);
	IoRequest->Context.RestartSession.ConnectTimeout = 
		LspIopGetTimeoutNullAsZero(ConnectTimeout);
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoConnect(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS DeviceAddress,
	__in_ecount(LocalAddressCount) PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_CONNECT;
	IoRequest->Context.Connect.Timeout = LspIopGetTimeoutNullAsZero(Timeout);
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoDisconnect(
	__in PLSP_IO_SESSION LspIoSession, 
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER Timeout,
	__in_opt PLSP_IO_REQUEST_COMPLETION CompletionRoutine, 
	__in_opt PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_DISCONNECT;
	IoRequest->Context.Disconnect.DisconnectFlags = DisconnectFlags;
	IoRequest->Context.Disconnect.Timeout = LspIopGetTimeoutNullAsZero(Timeout);
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoLogin(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_LOGIN;
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoLogout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_LOGOUT;
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoHandshake(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_HANDSHAKE;
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

NTSTATUS
LSPIOCALL
LspIoFlushCache(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext)
{
	PLSP_IO_REQUEST IoRequest;

	IoRequest = &LspIoSession->InternalFrontEndRequest;
	RtlZeroMemory(IoRequest, sizeof(LSP_IO_REQUEST));

	IoRequest->RequestType = LSP_IO_FLUSH_CACHE;
	IoRequest->CompletionRoutine = CompletionRoutine;
	IoRequest->CompletionContext = CompletionContext;

	if (NULL == CompletionRoutine)
	{
		return LspIoRequestIoSynchronous(LspIoSession, IoRequest);
	}
	else
	{
		return LspIoRequestIo(LspIoSession, IoRequest);
	}
}

