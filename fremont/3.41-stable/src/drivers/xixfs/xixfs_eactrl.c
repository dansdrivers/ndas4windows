#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_CommonQueryEA)
#pragma alloc_text(PAGE, xixfs_CommonSetEA)
#endif

NTSTATUS
xixfs_CommonQueryEA(
	IN PXIXFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter xixfs_CommonQueryEA \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonQueryEA\n"));


	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_EAS_NOT_SUPPORTED;
	xixfs_CompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_CommonQueryEA\n"));

	return RC;
}

NTSTATUS
xixfs_CommonSetEA(
	IN PXIXFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter xixfs_CommonSetEA \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonSetEA\n"));

	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_EAS_NOT_SUPPORTED;
	xixfs_CompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_CommonSetEA\n"));
	return RC;
}
