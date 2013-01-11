#pragma once
#include <tdi.h>
#include <socketlpx.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include <specstrings.h>

#define LSPIOCALL FASTCALL

typedef lsp_status_t LSP_STATUS;
typedef lsp_ide_register_param_t LSP_IDE_REGISTER, *PLSP_IDE_REGISTER;
typedef lsp_login_info_t LSP_LOGIN_INFO, *PLSP_LOGIN_INFO;
typedef struct _LSP_IO_SESSION *PLSP_IO_SESSION;
typedef struct _LSP_CONNECT_CONTEXT *PLSP_CONNECT_CONTEXT;
typedef struct _LSP_FAST_CONNECT_CONTEXT *PLSP_FAST_CONNECT_CONTEXT;

typedef enum _LSP_TRANSPORT_ADDRESS_TYPE {
	LspTransportAddressNotSpecified,
	LspOverTcpLpx,
	LspOverTcpIp,
	MakeLspTransportAddressTypeAsULong = 0xFFFFFFFF
} LSP_TRANSPORT_ADDRESS_TYPE;

C_ASSERT(sizeof(LSP_TRANSPORT_ADDRESS_TYPE) == 4);

typedef struct _LSP_TRANSPORT_ADDRESS {
	LSP_TRANSPORT_ADDRESS_TYPE Type;
	ULONG Reserved;
	union {
		TDI_ADDRESS_LPX LpxAddress;
		TDI_ADDRESS_IP IPAddress;
		UCHAR Alignment[24];
	};
} LSP_TRANSPORT_ADDRESS, *PLSP_TRANSPORT_ADDRESS;

C_ASSERT(sizeof(LSP_TRANSPORT_ADDRESS) == 32);

#define IO_ERR_LSP_FAILURE ((STATUS_SEVERITY_ERROR << 30) | (1 << 29) | (0xECC  << 16) | (0x0001 << 0))
#define STATUS_LSP_ERROR IO_ERR_LSP_FAILURE

typedef struct _LSP_IO_REQUEST LSP_IO_REQUEST, *PLSP_IO_REQUEST;

typedef
VOID
LSPIOCALL
LSP_IO_REQUEST_COMPLETION(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST LspIoRequest,
	__in PVOID Context);

typedef LSP_IO_REQUEST_COMPLETION *PLSP_IO_REQUEST_COMPLETION;

typedef enum _LSP_IO_REQUEST_TYPE {
	LSP_IO_NONE,
	//
	// First-tier Requests
	//
	LSP_IO_LSP_COMMAND,
	LSP_IO_CONNECT,
	LSP_IO_DISCONNECT,
	LSP_IO_LOGIN,
	LSP_IO_LOGOUT,
	LSP_IO_SET_TDI_EVENT_HANDLER,
	LSP_IO_FLUSH_CACHE,
	LSP_IO_SMART,
	LSP_IO_IDENTIFY,
	LSP_IO_HANDSHAKE,
	LSP_IO_PAUSE_SESSION,
	LSP_IO_UPDATE_CONNECTION_STATS,
	//
	// Second-tier Requests
	//
	LSP_IO_READ,
	LSP_IO_WRITE,
	LSP_IO_WRITE_FUA,
	LSP_IO_VERIFY,
	//
	// Third-tier Requests
	//
	LSP_IO_START_SESSION,
	LSP_IO_STOP_SESSION,
	//
	// Fourth-tier Requests
	//
	LSP_IO_RESTART_SESSION,
	LSP_IO_RESERVED
} LSP_IO_REQUEST_TYPE;

typedef struct _LPX_TDI_PERF_COUNTER {
	ULONG RetransmitCounter; // TX
	ULONG PacketLossCounter; // RX
} LPX_TDI_PERF_COUNTER, *PLPX_TDI_PERF_COUNTER;

enum {
	LSP_IO_MINIMUM_READ_BYTES = 4096,
	LSP_IO_STABLE_RX_COUNT_THRESHOLD = 64,
	LSP_IO_STABLE_TX_COUNT_THRESHOLD = 64,
	LSP_IO_RX_INCREMENT_BYTES = 512
};

typedef struct _LSP_IO_REQUEST {
	LSP_IO_REQUEST_TYPE RequestType;
	union {

		/* Aggregated Request */
		struct {
			LARGE_INTEGER ConnectTimeout;
			lsp_status_t LspStatus;
		} StartSession;

		struct {
			LARGE_INTEGER DisconnectTimeout;
		} StopSession;

		struct {
			LARGE_INTEGER RestartDelay;
			LARGE_INTEGER ConnectTimeout;
			LARGE_INTEGER DisconnectTimeout;
		} RestartSession;

		struct {
			LARGE_INTEGER Timeout;
		} Connect;

		struct {
			ULONG DisconnectFlags;
			LARGE_INTEGER Timeout;
		} Disconnect;

		struct {
			PVOID Reserved;
		} Login;

		struct {
			PVOID Reserved;
		} Logout;

		struct {
			USHORT EventId;
			PVOID EventHandler;
			PVOID EventContext;
		} SetTdiEventHandler;

		struct {
			LARGE_INTEGER LogicalBlockAddress;
			PVOID Buffer;
			ULONG TransferBlocks;
		} ReadWriteVerify;

		struct {
			PMDL Mdl;
			LARGE_INTEGER LogicalBlockAddress;
			ULONG TransferLength;
		} ReadWriteMdl;

		struct {
			LARGE_INTEGER PauseTimeout;
		} PauseSession;

		struct {
			LSP_IO_REQUEST_TYPE IoType;
			ULONG IoBytes;
		} UpdateConnectionStats;
	} Context;
	lsp_request_packet_t LspRequestPacket;
	PLSP_IO_REQUEST_COMPLETION CompletionRoutine;
	PVOID CompletionContext;
	//
	// Following fields are used internally.
	// Do not use from the LSP IO client
	//
	union {
		PLSP_IO_SESSION LspIoSession;
		struct {
			PLSP_IO_SESSION LspIoSession;
			ULONG TransferBlocks;
		} ReadWriteVerify;
		struct {
			PLSP_IO_SESSION LspIoSession;
			KTIMER PauseTimer;
			KDPC PauseTimerDpc;
		} PauseSession;
		struct {
			PLSP_IO_SESSION LspIoSession;
			LPX_TDI_PERF_COUNTER PerfCounter;
		} UpdateConnectionStats;
	} Internal;
	PLSP_IO_REQUEST OriginalRequest;
	IO_STATUS_BLOCK IoStatus;
} LSP_IO_REQUEST, *PLSP_IO_REQUEST;

static enum { LSP_MAX_ADDRESS_COUNT = 8 };

extern ULONG LSP_IO_DEFAULT_INTERAL_BUFFER_SIZE;

//
// Lsp Io Session Management Functions
//

//
// LspIoAllocateSession
// -> LspIoInitializeSession
//   -> LspIoCreateConnection <---------------------------------------+
//     -> LspIoCreateAddress                                          |
//       -> LspIoConnect --- failed ( CloseAddress, CloseConnection) -+
//         -> LspIoLogin
//           -> LspIoRead/Write ...
//         -> LspIoLogout
//       -> LspIoDisconnect
//     -> LspIoCloseAddress
//   -> LspIoCloseConnection
// -> LspIoCleanupSession
// LspIoFreeSession
//

NTSTATUS
LSPIOCALL
LspIoAllocateSession(
	__in PDEVICE_OBJECT DeviceObject,
	__in ULONG InternalBufferLength,
	__out PLSP_IO_SESSION* LspIoSession);

VOID
LSPIOCALL
LspIoFreeSession(
	__in PLSP_IO_SESSION LspIoSession);

NTSTATUS
LSPIOCALL
LspIoInitializeSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS DeviceAddress,
	__in_ecount(LocalAddressCount) PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount,
	__in const lsp_login_info_t* LspLoginInfo);

VOID
LSPIOCALL
LspIoUninitializeSession(
	__in PLSP_IO_SESSION LspIoSession);

LSP_STATUS
LSPIOCALL
LspIoGetLastLspStatus(
	__in PLSP_IO_SESSION LspIoSession);

NTSTATUS
LSPIOCALL
LspIoGetLastTdiError(
	__in PLSP_IO_SESSION LspIoSession);

NTSTATUS
LSPIOCALL
LspIoGetLastIdeOutputRegister(
	__in PLSP_IO_SESSION LspIoSession,
	__out_bcount(sizeof(LSP_IDE_REGISTER)) 
		PLSP_IDE_REGISTER IdeRegister);

NTSTATUS
LSPIOCALL
LspIoQueryLocalAddress(
	__in_opt PLSP_IO_SESSION LspIoSession,
	__inout_bcount(TransportAddressBufferLength) 
		PTRANSPORT_ADDRESS TransportAddressBuffer,
	__in ULONG TransportAddressBufferLength);

NTSTATUS
LSPIOCALL
LspIoConnect(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS DeviceAddress,
	__in_ecount(LocalAddressCount) PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount,
	__in_opt PLARGE_INTEGER Timeout,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoDisconnect(
	__in PLSP_IO_SESSION LspIoSession, 
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER Timeout,
	__in_opt PLSP_IO_REQUEST_COMPLETION CompletionRoutine, 
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoSetTdiEventHandler(
	__in PLSP_IO_SESSION LspIoSession,
	__in USHORT EventId,
	__in PVOID EventHandler,
	__in PVOID EventContext,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoLogin(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoLogout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoHandshake(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoFlushCache(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoRequestIo(
	__in PLSP_IO_SESSION IoSession,
	__in PLSP_IO_REQUEST IoRequest);

NTSTATUS
LSPIOCALL
LspIoRequestIoSynchronous(
	__in PLSP_IO_SESSION IoSession,
	__in PLSP_IO_REQUEST IoRequest);

lsp_handle_t
LSPIOCALL
LspIoGetLspHandle(
	__in PLSP_IO_SESSION IoSession);

VOID
LSPIOCALL
LspIoResetLoginInfo(
	__in PLSP_IO_SESSION IoSession,
	__in const lsp_login_info_t* LoginInfo);

VOID
LSPIOCALL
LspIoResetLocalAddressList(
	__in PLSP_IO_SESSION IoSession,
	__in PLSP_TRANSPORT_ADDRESS LocalAddressList,
	__in ULONG LocalAddressCount);

//
// Connect + Login + Handshake
//
NTSTATUS
LSPIOCALL
LspIoStartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in_opt PLARGE_INTEGER ConnectionTimeout,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

//
// Logout + Disconnect
//
NTSTATUS
LSPIOCALL
LspIoStopSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

//
// Logout + Disconnect + Connect + Login + Handshake
//
NTSTATUS
LSPIOCALL
LspIoRestartSession(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLARGE_INTEGER RestartDelay,
	__in PLARGE_INTEGER DisconnectTimeout,
	__in PLARGE_INTEGER ConnectTimeout,
	__in PLSP_IO_REQUEST_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);
