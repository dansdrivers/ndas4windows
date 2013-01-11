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
#pragma alloc_text(PAGE, XixFsdCommonQuerySecurity)
#pragma alloc_text(PAGE, XixFsdCommonSetSecurity)
#endif


NTSTATUS
XixFsdCommonQuerySecurity(
	IN PXIFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonQuerySecurity \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Enter XixFsdCommonQuerySecurity \n"));


	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_INVALID_PARAMETER;
	XixFsdCompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Exit XixFsdCommonQuerySecurity \n"));

	return RC;
}

NTSTATUS
XixFsdCommonSetSecurity(
	IN PXIFS_IRPCONTEXT IrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonSetSecurity \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Enter XixFsdCommonSetSecurity \n"));

	ASSERT(IrpContext);
	ASSERT(IrpContext->Irp);
	RC = STATUS_INVALID_PARAMETER;
	XixFsdCompleteRequest(IrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_IRPCONTEXT,
		("Exit XixFsdCommonSetSecurity \n"));

	return RC;
}