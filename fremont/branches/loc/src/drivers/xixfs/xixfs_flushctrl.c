#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"


VOID
xixfs_FlushFile(
	IN PXIXFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp, 
	IN PXIXFS_FCB			pFCB
);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_FlushFile)
#pragma alloc_text(PAGE, xixfs_CommonFlushBuffers)
#endif



NTSTATUS 
xixfs_FlushCompleteRoutine(
	PDEVICE_OBJECT	PtrDeviceObject,
	PIRP			PtrIrp,
	PVOID			Context
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Enter xixfs_FlushCompleteRoutine \n"));

	if (PtrIrp->PendingReturned) {
		IoMarkIrpPending(PtrIrp);
	}

	if (PtrIrp->IoStatus.Status == STATUS_INVALID_DEVICE_REQUEST) {
		PtrIrp->IoStatus.Status = STATUS_SUCCESS;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Exit xixfs_FlushCompleteRoutine \n"));
	return(STATUS_SUCCESS);
}


VOID
xixfs_FlushFile(
	IN PXIXFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp, 
	IN PXIXFS_FCB			pFCB
)
{
	PIO_STATUS_BLOCK		pIoStatus = NULL;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_FCB),
		("Enter xixfs_FlushFile \n"));


	ASSERT(pIrp);
	pIoStatus = &(pIrp->IoStatus);
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		// Added by ILGU HONG for readonly 09052006
		if(!pFCB->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
			CcFlushCache(&(pFCB->SectionObject), NULL, 0, pIoStatus);
		}
		// Added by ILGU HONG for readonly end
		//DbgPrint("CcFlush  5 File(%wZ)\n", &pFCB->FCBFullPath);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_FCB),
		("Exit xixfs_FlushFile \n"));
}





NTSTATUS
xixfs_CommonFlushBuffers(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP						pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_VCB				pVCB = NULL;

	PIO_STACK_LOCATION		pNextIrpSp = NULL;
	NTSTATUS				RC1 = STATUS_SUCCESS;

	BOOLEAN					AcquiredFCB = FALSE;
	BOOLEAN					PostRequest = FALSE;
	BOOLEAN					Wait = TRUE;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Enter xixfs_CommonFlushBuffers \n"));


	ASSERT(pIrpContext);
	ASSERT(pIrpContext->Irp);

	pIrp = pIrpContext->Irp;

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = xixfs_DecodeFileObject(pFileObject, &pFCB, &pCCB);

	ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!FlushBuffers pCCB(%p)\n",  pCCB));

	if(TypeOfOpen != UserFileOpen){
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	Wait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	if(!Wait){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
			("PostRequest IrpContext(%p) Irp(%p)\n",pIrpContext, pIrp ));
		
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}

	if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
		RC = STATUS_SUCCESS;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
	}

	try{

		
		
		//ExAcquireResourceExclusiveLite(&(pFCB->FCBResource), TRUE);
		ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
		xixfs_FlushFile(pIrpContext, pIrp, pFCB);
		ExReleaseResourceLite(pFCB->PagingIoResource);
		//ExReleaseResourceLite(&(pFCB->FCBResource));
			
		pIrpContext->Irp = NULL;
		xixfs_CompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
	
			
		pNextIrpSp = IoGetNextIrpStackLocation(pIrp);
		IoCopyCurrentIrpStackLocationToNext(pIrp );
		IoSetCompletionRoutine(pIrp, xixfs_FlushCompleteRoutine, NULL, TRUE, TRUE, TRUE);

		RC1 = IoCallDriver(pVCB->TargetDeviceObject, pIrp);
		RC = ((RC1 == STATUS_INVALID_DEVICE_REQUEST)?RC:RC1);	


		
	}finally{
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Exit xixfs_CommonFlushBuffers \n"));

	return RC;
}
