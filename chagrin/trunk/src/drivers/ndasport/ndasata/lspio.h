#pragma once
#include <tdi.h>
#include <socketlpx.h>
#include <lspx/lsp.h>
#include <specstrings.h>

#define LSPIOCALL FASTCALL

typedef lsp_status_t LSP_STATUS;
typedef lsp_ide_register_param_t LSP_IDE_REGISTER, *PLSP_IDE_REGISTER;
typedef lsp_login_info_t LSP_LOGIN_INFO, *PLSP_LOGIN_INFO;
typedef struct _LSP_IO_SESSION LSP_IO_SESSION, *PLSP_IO_SESSION;
// typedef volatile LSP_IO_SESSION * PLSP_IO_SESSION;

typedef enum _LSP_TRANSPORT_ADDRESS_TYPE {
	LspOverLpxStream,
	LspOverTcp,
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

typedef enum _LSP_IO_FLAG {
	LSP_IOF_USE_DMA      = 0x00000001,
	LSP_IOF_USE_LBA      = 0x00000002,
	LSP_IOF_USE_LBA48    = 0x00000004,
	LSP_IOF_DIRECT_WRITE = 0x00000008,
	LSP_IOF_LOCKED_WRITE = 0x00000010,
	LSP_IOF_VERIFY_WRITE = 0x00000020
} LSP_IO_FLAG;

#define IO_ERR_LSP_FAILURE ((STATUS_SEVERITY_ERROR << 30) | (1 << 29) | (0xECC  << 16) | (0x0001 << 0))
#define STATUS_LSP_ERROR IO_ERR_LSP_FAILURE

typedef NTSTATUS (*PLSP_IO_COMPLETION)(
	__in PLSP_IO_SESSION LspIoSession,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

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
	__in PLSP_IO_SESSION LspIoSession);

VOID
LSPIOCALL
LspIoCleanupSession(
	__in PLSP_IO_SESSION LspIoSession);

NTSTATUS
LSPIOCALL
LspIoCreateConnectionFile(
	__in PLSP_IO_SESSION LspIoSession);

VOID
LSPIOCALL
LspIoCloseConnectionFile(
	__in PLSP_IO_SESSION LspIoSession);

NTSTATUS
LSPIOCALL
LspIoCreateAddressFile(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLSP_TRANSPORT_ADDRESS LocalAddress);

VOID
LSPIOCALL
LspIoCloseAddressFile(
	__in PLSP_IO_SESSION LspIoSession);

//
// Lsp Io Functions
//

ULONG
LSPIOCALL
LspIoGetIoFlags(
	__in PLSP_IO_SESSION LspIoSession);

VOID
LSPIOCALL
LspIoSetIoFlags(
	__in PLSP_IO_SESSION LspIoSession,
	__in ULONG Flags);

VOID
LSPIOCALL
LspIoAppendIoFlag(
	__in PLSP_IO_SESSION LspIoSession,
	__in ULONG Flag);

VOID
LSPIOCALL
LspIoClearIoFlag(
	__in PLSP_IO_SESSION LspIoSession,
	__in ULONG Flag);

/* Lower byte contains the hardware version,
   Higher byte contains the hardware revision */

typedef struct _NDAS_HARDWARE_VERSION_INFO {
	UCHAR  Version;
	UCHAR  Reserved;
	USHORT Revision;
	UCHAR  Reserved2;
} NDAS_HARDWARE_VERSION_INFO, *PNDAS_HARDWARE_VERSION_INFO;

NDAS_HARDWARE_VERSION_INFO
LSPIOCALL
LspIoGetHardwareVersionInfo(
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

VOID
LSPIOCALL
LspIoSetReceiveTimeout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLARGE_INTEGER DueTime);

VOID
LSPIOCALL
LspIoSetSendTimeout(
	__in PLSP_IO_SESSION LspIoSession,
	__in PLARGE_INTEGER DueTime);

LARGE_INTEGER
LSPIOCALL
LspIoGetReceiveTimeout(
	__in PLSP_IO_SESSION LspIoSession);

LARGE_INTEGER
LSPIOCALL
LspIoGetSendTimeout(
	__in PLSP_IO_SESSION LspIoSession);

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
	__in_opt PLARGE_INTEGER Timeout,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoDisconnect(
	__in PLSP_IO_SESSION LspIoSession, 
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER Timeout,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine, 
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoLogin(
	__in PLSP_IO_SESSION LspIoSession,
	__in CONST LSP_LOGIN_INFO* LspLoginInfo,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoLogout(
	__in PLSP_IO_SESSION LspIoSession,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

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
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoAtaIdentifyDevice(
	__in PLSP_IO_SESSION LspIoSession,
	__out_bcount(TransferLength) PVOID Buffer,
	__reserved PLARGE_INTEGER Reserved,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

typedef enum ATA_SETFEATURE_SUBCOMMAND {
	AtaSfcEnable8bitPIO    = 0x01,
	AtaSfcEnableWriteCache = 0x02,
	AtaSfcSetTransferMode  = 0x03,
	AtaSfcEnableAPM        = 0x05,
	AtaSfcEnablePowerUpInStandby = 0x06,
	AtaSfcPowerUpInStandbySetDeviceSpinup = 0x07,
	AtaSfcEnableCFAPowerMode1               = 0x0A,
	AtaSfcDisableMediaStatusNotification    = 0x31,
	AtaSfcEnableAutomaticAcousticManagement = 0x42,
	AtaSfcSetMaximumHostInterfaceSectorTimes = 0x43,
	AtaSfcDisableReadLookAhead = 0x55,
	AtaSfcEnableReleaseInterrupt = 0x5D,
	AtaSfcEnableServiceInterrupt = 0x5E,
	AtaSfcDisableRevertingToPowerOnDefaults = 0x66,
	AtaSfcDisable8bitPIO = 0x81,
	AtaSfcDisableWriteCache = 0x82,
	AtaSfcDisableAPM = 0x85,
	AtaSfcDisablePowerUpInStandby = 0x86,
	AtaSfcDisableCFAPowerMode1 = 0x8A,
	AtaSfcEnableMediaStatusNotification = 0x95,
	AtaSfcEnableReadLookAhead = 0xAA,
	AtaSfcDisableAutomaticAcousticManagement = 0xC2,
	AtaSfcEnableRevertingToPowerOnDefaults = 0xCC,
	AtaSfcDisableReleaseInterrupt = 0xDD,
	AtaSfcDisableServiceInterrupt = 0xDE
} ATA_SETFEATURE_SUBCOMMAND;

/* FCT = Flow Control Transfer */

typedef enum ATA_SETFEATURE_TRANSFER_MODE {
	ATA_TRANSFER_MODE_PIO_DEFAULT_MODE = 0x00,
	ATA_TRANSFER_MODE_PIO_DEFAULT_DISABLE_IORDY_MODE = 0x01,
	ATA_TRANSFER_MODE_PIO_FCT_MODE_0 = 0x08,
	ATA_TRANSFER_MODE_PIO_FCT_MODE_1 = 0x09,
	ATA_TRANSFER_MODE_PIO_FCT_MODE_2 = 0x0A,
	ATA_TRANSFER_MODE_PIO_FCT_MODE_3 = 0x0B,
	ATA_TRANSFER_MODE_PIO_FCT_MODE_4 = 0x0C,
	ATA_TRANSFER_MODE_MULTIWORD_DMA_0 = 0x20,
	ATA_TRANSFER_MODE_MULTIWORD_DMA_1 = 0x21,
	ATA_TRANSFER_MODE_MULTIWORD_DMA_2 = 0x22,
	ATA_TRANSFER_MODE_ULTRA_DMA_0 = 0x40,
	ATA_TRANSFER_MODE_ULTRA_DMA_1 = 0x41,
	ATA_TRANSFER_MODE_ULTRA_DMA_2 = 0x42,
	ATA_TRANSFER_MODE_ULTRA_DMA_3 = 0x43,
	ATA_TRANSFER_MODE_ULTRA_DMA_4 = 0x44,
	ATA_TRANSFER_MODE_ULTRA_DMA_5 = 0x45,
	ATA_TRANSFER_MODE_ULTRA_DMA_6 = 0x46,
	ATA_TRANSFER_MODE_ULTRA_DMA_7 = 0x47,
	ATA_TRANSFER_MODE_UNSPECIFIED = 0xFFFFFFFF
} ATA_SETFEATURE_TRANSFER_MODE;

NTSTATUS
LSPIOCALL
LspIoAtaSetFeature(
	__in PLSP_IO_SESSION LspIoSession,
	__in ATA_SETFEATURE_SUBCOMMAND Feature,
	__in UCHAR SectorCountRegister,
	__in UCHAR LBALowRegister,
	__in UCHAR LBAMidRegister,
	__in UCHAR LBAHighRegister,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoAtaIdentifyPacketDevice(
	__in PLSP_IO_SESSION LspIoSession,
	__out_bcount(TransferLength) PVOID Buffer,
	__reserved PLARGE_INTEGER Reserved,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoAtaFlushCache(
	__in PLSP_IO_SESSION LspIoSession,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoRead(
	__in PLSP_IO_SESSION LspIoSession,
	__out_bcount(TransferLength) PVOID Buffer,
	__in PLARGE_INTEGER LogicalBlockAddress,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoWrite(
	__in PLSP_IO_SESSION LspIoSession,
	__in_bcount(TransferLength) PVOID Buffer,
	__in PLARGE_INTEGER LogicalBlockAddress,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoVerify(
	__in PLSP_IO_SESSION LspIoSession,
	__reserved PVOID Reserved,
	__in PLARGE_INTEGER LogicalBlockAddress,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoSingleWrite(
	__in PLSP_IO_SESSION LspIoSession,
	__in_bcount(TransferLength) PVOID Buffer,
	__in PLARGE_INTEGER LogicalBlockAddress,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoSingleRead(
	__in PLSP_IO_SESSION LspIoSession,
	__in_bcount(TransferLength) PVOID Buffer,
	__in PLARGE_INTEGER LogicalBlockAddress,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

NTSTATUS
LSPIOCALL
LspIoSingleVerify(
	__in PLSP_IO_SESSION LspIoSession,
	__reserved PVOID Reserved,
	__in PLARGE_INTEGER LogicalBlockAddress,
	__in ULONG TransferLength,
	__in_opt PLSP_IO_COMPLETION CompletionRoutine,
	__in_opt PVOID CompletionContext);

typedef enum _LSP_IO_OPERATION_CODE {
	LspIoOpAcquireBufferLock,
	LspIoOpReleaseBufferLock,
	LspIoOpRead,
	LspIoOpWrite,
	LspIoOpLockedWrite,
	LspIoOpVerify,
	LspIoOpIdentify,
	LspIoOpFlushCache
} LSP_IO_OPERATION_CODE;

typedef struct _LSP_IO_REQUEST LSP_IO_REQUEST, *PLSP_IO_REQUEST;

typedef struct _LSP_IO_REQUEST {
	LSP_IO_OPERATION_CODE OperationCode;
	union {
		struct {
			PVOID Buffer;
			LARGE_INTEGER Reserved;
			ULONG TransferLength;
		} IDENTIFY, PIDENTIFY;
		struct {
			PVOID Buffer;
			LARGE_INTEGER LogicalBlockAddress;
			ULONG TransferLength;
		} READ, WRITE;
		struct {
			PVOID Reserved;
			LARGE_INTEGER LogicalBlockAddress;
			ULONG TransferLength;
		} VERIFY;
		struct {
			LSP_IDE_REGISTER IdeRegister;
			PVOID InBuffer;
			ULONG InBufferLength;
			PVOID OutBuffer;
			ULONG OutBufferLength;
		} IDE_COMMAND;
		struct {
			PVOID Reserved;
		} BUFFER_LOCK;
	};
	PLSP_IO_COMPLETION CompletionRoutine;
	PVOID CompletionContext;
	PVOID ContextData[4];
	PLSP_IO_REQUEST NextIoRequest;
} LSP_IO_REQUEST, *PLSP_IO_REQUEST;

NTSTATUS
LSPIOCALL
LspIoRequest(
	__in PLSP_IO_SESSION IoSession,
	__in PLSP_IO_REQUEST IoRequest);

// C_ASSERT(sizeof(LSP_IO_REQUEST) == 20);
#if defined(_X86_)
C_ASSERT(sizeof(LSP_IDE_REGISTER) == 14);
C_ASSERT(sizeof(LSP_IO_OPERATION_CODE) == 4);
C_ASSERT(FIELD_OFFSET(LSP_IO_REQUEST, VERIFY) == 8);
C_ASSERT(FIELD_OFFSET(LSP_IO_REQUEST, CompletionRoutine) == (8+16+16));
#elif defined(_AMD64_)
C_ASSERT(sizeof(LSP_IDE_REGISTER) == 14);
C_ASSERT(sizeof(LSP_IO_OPERATION_CODE) == 4);
C_ASSERT(FIELD_OFFSET(LSP_IO_REQUEST, VERIFY) == 8);
//C_ASSERT(FIELD_OFFSET(LSP_IO_REQUEST, CompletionRoutine) == (4+16+16));
//C_ASSERT(FIELD_OFFSET(LSP_IO_REQUEST, CompletionRoutine) == 36);
#else
#endif
