#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "XixFsComProto.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdCommonQueryEA)
#pragma alloc_text(PAGE, XixFsdCommonSetEA)
#endif

NTSTATUS
XixFsdCommonQueryEA(
	IN PXIFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonQueryEA \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonQueryEA\n"));


	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_INVALID_PARAMETER;
	XixFsdCompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCommonQueryEA\n"));

	return RC;
}

NTSTATUS
XixFsdCommonSetEA(
	IN PXIFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonSetEA \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonSetEA\n"));

	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_INVALID_PARAMETER;
	XixFsdCompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCommonSetEA\n"));
	return RC;
}
