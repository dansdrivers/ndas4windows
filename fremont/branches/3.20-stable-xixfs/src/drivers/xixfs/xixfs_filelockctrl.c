#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_CommonLockControl)
#endif





NTSTATUS
xixfs_CommonLockControl(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PFILE_OBJECT		pFileObject = NULL;
	PXIXFS_FCB			pFCB = NULL;
	PXIXFS_CCB			pCCB = NULL;
	PXIXFS_VCB			pVCB = NULL;
	TYPE_OF_OPEN		TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter xixfs_CommonLockControl \n"));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_CommonLockControl\n"));




	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = xixfs_DecodeFileObject(pFileObject, &pFCB, &pCCB);

	ASSERT_FCB(pFCB);


	if(TypeOfOpen != UserFileOpen){
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) 
	{
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
		RC = STATUS_ACCESS_DENIED;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("CALL FsRtlCheckOplock : xixfs_CommonLockControl\n"));

	RC = FsRtlCheckOplock(&pFCB->FCBOplock,
						pIrp,
						pIrpContext,
						xixfs_OplockComplete,
						NULL);

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
			("return PENDING FsRtlCheckOplock : xixfs_CommonLockControl\n"));
		return RC;
	}

	if(!xixfs_VerifyFCBOperation(pIrpContext, pFCB)){
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
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
		pFCB->IsFastIoPossible = xixfs_CheckFastIoPossible(pFCB);
		XifsdUnlockFcb(pIrpContext, pFCB);
	
	}finally{
	}
	
	xixfs_CompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_CommonLockControl\n"));
	
	return RC;
	
}
