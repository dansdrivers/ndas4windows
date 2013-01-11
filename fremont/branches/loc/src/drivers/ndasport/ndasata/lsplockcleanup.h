#pragma once
#ifndef LSP_LOCK_CLEANUP_H_INCLUDED
#define LSP_LOCK_CLEANUP_H_INCLUDED

#ifndef LSPIOCALL
#define LSPIOCALL FASTCALL
#endif

typedef struct _LSP_LOCK_CLEANUP_CONTEXT *PLSP_LOCK_CLEANUP_CONTEXT;
typedef struct _lsp_login_info_t lsp_login_info_t;
typedef struct _LSP_CONNECT_CONTEXT *PLSP_CONNECT_CONTEXT;

typedef
VOID
LSPIOCALL
LSP_LOCK_CLEANUP_COMPLETION(
	__in PLSP_LOCK_CLEANUP_CONTEXT LspLockCleanupContext,
	__in PIO_STATUS_BLOCK IoStatus,
	__in PVOID Context);

typedef LSP_LOCK_CLEANUP_COMPLETION *PLSP_LOCK_CLEANUP_COMPLETION;

PLSP_LOCK_CLEANUP_CONTEXT
LSPIOCALL
LspLockCleanupAllocate(
	__in PDEVICE_OBJECT DeviceObject);

VOID
LSPIOCALL
LspLockCleanupFree(
	__in PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext);

NTSTATUS
LSPIOCALL
LspLockCleanup(
	__in PLSP_LOCK_CLEANUP_CONTEXT LockCleanupContext,
	__in const lsp_login_info_t* LspLoginInfo,
	__in PLSP_CONNECT_CONTEXT LspConnection,
	__in PLSP_LOCK_CLEANUP_COMPLETION CompletionRoutine,
	__in PVOID CompletionContext);

#endif /* LSP_LOCK_CLEANUP_H_INCLUDED */
