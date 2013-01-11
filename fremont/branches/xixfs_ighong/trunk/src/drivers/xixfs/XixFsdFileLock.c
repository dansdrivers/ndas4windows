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
#pragma alloc_text(PAGE, XixFsdCommonLockControl)
#endif





NTSTATUS
XixFsdCommonLockControl(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PFILE_OBJECT		pFileObject = NULL;
	PXIFS_FCB			pFCB = NULL;
	PXIFS_CCB			pCCB = NULL;
	PXIFS_VCB			pVCB = NULL;
	TYPE_OF_OPEN		TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonLockControl \n"));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdCommonLockControl\n"));




	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);

	ASSERT_FCB(pFCB);


	if(TypeOfOpen != UserFileOpen){
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	if(pFCB->FCBType != FCB_TYPE_FILE) 
	{
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		RC = STATUS_ACCESS_DENIED;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("CALL FsRtlCheckOplock : XixFsdCommonLockControl\n"));

	RC = FsRtlCheckOplock(&pFCB->FCBOplock,
						pIrp,
						pIrpContext,
						XixFsdOplockComplete,
						NULL);

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
			("return PENDING FsRtlCheckOplock : XixFsdCommonLockControl\n"));
		return RC;
	}

	if(!XixFsdVerifyFcbOperation(pIrpContext, pFCB)){
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	try{


		if(pFCB->FCBFileLock == NULL){
			pFCB->FCBFileLock = FsRtlAllocateFileLock(NULL, NULL);

			if(!pFCB->FCBFileLock){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}
		}


		RC = FsRtlProcessFileLock(pFCB->FCBFileLock, pIrp, NULL);

		XifsdLockFcb(pIrpContext, pFCB);
		pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible(pFCB);
		XifsdUnlockFcb(pIrpContext, pFCB);
	
	}finally{
	}
	
	XixFsdCompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdCommonLockControl\n"));
	
	return RC;
	
}
