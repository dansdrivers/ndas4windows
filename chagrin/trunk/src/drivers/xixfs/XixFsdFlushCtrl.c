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
#pragma alloc_text(PAGE, XixFsdFlushFile)
#pragma alloc_text(PAGE, XixFsdCommonFlushBuffers)
#endif



NTSTATUS 
XifsdFlushCompleteRoutine(
	PDEVICE_OBJECT	PtrDeviceObject,
	PIRP			PtrIrp,
	PVOID			Context
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Enter XifsdFlushCompleteRoutine \n"));

	if (PtrIrp->PendingReturned) {
		IoMarkIrpPending(PtrIrp);
	}

	if (PtrIrp->IoStatus.Status == STATUS_INVALID_DEVICE_REQUEST) {
		PtrIrp->IoStatus.Status = STATUS_SUCCESS;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Exit XifsdFlushCompleteRoutine \n"));
	return(STATUS_SUCCESS);
}


VOID
XixFsdFlushFile(
	IN PXIFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp, 
	IN PXIFS_FCB			pFCB
)
{
	PIO_STATUS_BLOCK		pIoStatus = NULL;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_FCB),
		("Enter XixFsdFlushFile \n"));


	ASSERT(pIrp);
	pIoStatus = &(pIrp->IoStatus);
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		// Added by ILGU HONG for readonly 09052006
		if(!pFCB->PtrVCB->IsVolumeWriteProctected){
			CcFlushCache(&(pFCB->SectionObject), NULL, 0, pIoStatus);
		}
		// Added by ILGU HONG for readonly end
		//DbgPrint("CcFlush  5 File(%wZ)\n", &pFCB->FCBFullPath);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_FCB),
		("Exit XixFsdFlushFile \n"));
}





NTSTATUS
XixFsdCommonFlushBuffers(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP						pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_VCB				pVCB = NULL;

	PIO_STACK_LOCATION		pNextIrpSp = NULL;
	NTSTATUS				RC1 = STATUS_SUCCESS;

	BOOLEAN					AcquiredFCB = FALSE;
	BOOLEAN					PostRequest = FALSE;
	BOOLEAN					Wait = TRUE;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Enter XixFsdCommonFlushBuffers \n"));


	ASSERT(pIrpContext);
	ASSERT(pIrpContext->Irp);

	pIrp = pIrpContext->Irp;

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);

	ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!FlushBuffers (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));

	if(TypeOfOpen != UserFileOpen){
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	Wait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	if(!Wait){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
			("PostRequest IrpContext(%p) Irp(%p)\n",pIrpContext, pIrp ));
		
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		RC = STATUS_SUCCESS;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
	}

	try{

		
		
		//ExAcquireResourceExclusiveLite(&(pFCB->FCBResource), TRUE);
		ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
		XixFsdFlushFile(pIrpContext, pIrp, pFCB);
		ExReleaseResourceLite(pFCB->PagingIoResource);
		//ExReleaseResourceLite(&(pFCB->FCBResource));
			
		pIrpContext->Irp = NULL;
		XixFsdCompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
	
			
		pNextIrpSp = IoGetNextIrpStackLocation(pIrp);
		IoCopyCurrentIrpStackLocationToNext(pIrp );
		IoSetCompletionRoutine(pIrp, XifsdFlushCompleteRoutine, NULL, TRUE, TRUE, TRUE);

		RC1 = IoCallDriver(pVCB->TargetDeviceObject, pIrp);
		RC = ((RC1 == STATUS_INVALID_DEVICE_REQUEST)?RC:RC1);	


		
	}finally{
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FLUSH|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		("Exit XixFsdCommonFlushBuffers \n"));

	return RC;
}
