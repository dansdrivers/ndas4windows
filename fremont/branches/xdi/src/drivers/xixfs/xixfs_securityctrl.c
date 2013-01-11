#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_CommonQuerySecurity)
#pragma alloc_text(PAGE, xixfs_CommonSetSecurity)
#endif


NTSTATUS
xixfs_CommonQuerySecurity(
	IN PXIXFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter xixfs_CommonQuerySecurity \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Enter xixfs_CommonQuerySecurity \n"));


	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_INVALID_PARAMETER;
	xixfs_CompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Exit xixfs_CommonQuerySecurity \n"));

	return RC;
}

NTSTATUS
xixfs_CommonSetSecurity(
	IN PXIXFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter xixfs_CommonSetSecurity \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Enter xixfs_CommonSetSecurity \n"));

	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_INVALID_PARAMETER;
	xixfs_CompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Exit xixfs_CommonSetSecurity \n"));

	return RC;
}