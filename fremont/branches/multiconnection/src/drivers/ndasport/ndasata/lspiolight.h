#pragma once
#ifndef LSP_IO_LIGHT_H_INCLUDED
#define LSP_IO_LIGHT_H_INCLUDED

#include <xtdiios.h>
#include "lspio.h"
#include "lspiolight.h"

typedef struct _LSP_IO_LIGHT_SESSION *PLSP_IO_LIGHT_SESSION; 

typedef
VOID
LSPIOCALL
LSP_IO_LIGHT_COMPLETION(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

typedef LSP_IO_LIGHT_COMPLETION *PLSP_IO_LIGHT_COMPLETION;

typedef enum _LSP_IO_LIGHT_SESSION_FLAGS {
	LSP_IO_LIGHT_SESSION_OP_IN_PROGRESS = 0x1
} LSP_IO_LIGHT_SESSION_FLAGS, *PLSP_IO_LIGHT_SESSION_FLAGS;

typedef struct _LSP_IO_LIGHT_SESSION {

	ULONG SessionFlags;

	lsp_handle_t LspHandle;
	lsp_status_t LspStatus;

	PFILE_OBJECT ConnectionFileObject;
	PDEVICE_OBJECT ConnectionDeviceObject;

	LONG PendingTdiIo;
	XTDI_IO_SESSION TdiIoSession[4];

	PVOID LspSessionBuffer;

	LONG RequestSequence;

	lsp_request_packet_t* ActiveLspRequest;
	PLSP_IO_LIGHT_COMPLETION CompletionRoutine;
	PVOID CompletionContext;

} LSP_IO_LIGHT_SESSION, *PLSP_IO_LIGHT_SESSION;

//
// IRQL == PASSIVE_LEVEL
//
NTSTATUS
LSPIOCALL
LspIoLightSessionCreate(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject);

//
// IRQL == PASSIVE_LEVEL
//
VOID
LSPIOCALL
LspIoLightSessionDelete(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession);

//
// IRQL == PASSIVE_LEVEL
//
NTSTATUS
LSPIOCALL
LspIoLightSessionRecreate(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject);

//
// IRQL <= DISPATCH_LEVEL
//
NTSTATUS
LSPIOCALL
LspIoLightSessionRequest(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession,
	__in lsp_request_packet_t* LspRequestPacket,
	__in PLSP_IO_LIGHT_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

VOID
LSPIOCALL
LspIoLightSessionCancelPendingIrps(
	__in PLSP_IO_LIGHT_SESSION LspIoLightSession);

#endif /* LSP_IO_LIGHT_H_INCLUDED */
