#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_debug.h"
#include "xixfs_global.h"
#include "xixfs_internal.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_AllocateFCB)
#pragma alloc_text(PAGE, xixfs_FreeFCB)
#pragma alloc_text(PAGE, xixfs_AllocateCCB)
#pragma alloc_text(PAGE, xixfs_FreeCCB)
#pragma alloc_text(PAGE, xixfs_AllocateLCB)
#pragma alloc_text(PAGE, xixfs_FreeLCB)
#pragma alloc_text(PAGE, xixfs_AllocateIrpContext)
#pragma alloc_text(PAGE, xixfs_ReleaseIrpContext)
#endif


PXIXFS_FCB 	
xixfs_AllocateFCB(VOID)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIXFS_FCB			FCB = NULL;
	uint32				i = 0;
	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Enter xixfs_AllocateFCB\n"));

	// allocate memory
	FCB = ExAllocateFromNPagedLookasideList(&(XifsFcbLookasideList));
	if(!FCB)
	{
		FCB = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIXFS_FCB),TAG_FCB);
		if(!FCB)
		{
			//debug message
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
				("ERROR XixFsAllocateFCB Can't allocate\n"));
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}
	
	RtlZeroMemory(FCB,sizeof(XIXFS_FCB));
	
	if (!IsFromLookasideList) {
		XIXCORE_SET_FLAGS(FCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_NOT_FROM_POOL);
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
			("xixfs_AllocateFCB FCBFlags(0x%x)\n",FCB->XixcoreFcb.FCBFlags));
	}
		
	FCB->NodeTypeCode = XIFS_NODE_FCB;
	FCB->NodeByteSize = sizeof(sizeof(XIXFS_FCB));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Exit xixfs_AllocateFCB(%p)\n",FCB));
	return FCB;	
}



void
xixfs_FreeFCB(
	PXIXFS_FCB pFCB
)
{
		
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Enter xixfs_FreeFCB (%p)\n",pFCB));

	// give back memory either to the zone or to the VMM
	if (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsFcbLookasideList),pFCB);
	} else {
		ExFreePool(pFCB);
	}

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Exit xixfs_FreeFCB\n"));

	return;

}


PXIXFS_CCB	
xixfs_AllocateCCB(VOID)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIXFS_CCB			CCB = NULL;

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Enter xixfs_AllocateCCB\n"));


	// allocate memory
	CCB = ExAllocateFromNPagedLookasideList(&(XifsCcbLookasideList));
	if(!CCB)
	{
		CCB = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIXFS_CCB),TAG_CCB);
		if(!CCB)
		{
			//debug message
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
				("ERROR xixfs_AllocateCCB Can't allocate\n"));
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}

	RtlZeroMemory(CCB,sizeof(XIXFS_CCB));

	CCB->NodeTypeCode = XIFS_NODE_CCB;
	CCB->NodeByteSize = sizeof(sizeof(XIXFS_CCB));
	
	InitializeListHead(&CCB->LinkToFCB);

	
	if (!IsFromLookasideList) {
		XIXCORE_SET_FLAGS(CCB->CCBFlags, XIXFSD_CCB_NOT_FROM_POOL);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
			("xixfs_AllocateCCB CCBFlags(0x%x)\n",CCB->CCBFlags));
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Exit xixfs_AllocateCCB(%p)\n",CCB));

	return CCB;	
}


void
xixfs_FreeCCB(
	PXIXFS_CCB pCCB
)
{	

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Enter xixfs_FreeCCB (%p)\n",pCCB));

	ASSERT(pCCB);
	
	if(pCCB->SearchExpression.Buffer != NULL){
		ExFreePool(pCCB->SearchExpression.Buffer);
	}

	/*
	if(pCCB->FullPath.Buffer != NULL){
		ExFreePool(pCCB->FullPath.Buffer);
	}
	*/
		
	// give back memory either to the zone or to the VMM
	if (!(pCCB->CCBFlags& XIXFSD_CCB_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsCcbLookasideList),pCCB);
	} else {
		ExFreePool(pCCB);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Exit xixfs_FreeCCB \n"));
	return;
}


PXIXFS_LCB	
xixfs_AllocateLCB(uint16 Length)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIXFS_LCB			LCB = NULL;

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Enter XixFsAllocateLCB \n"));

	// allocate memory
	LCB = ExAllocateFromNPagedLookasideList(&(XifsLcbLookasideList));
	if(!LCB)
	{
		LCB = ExAllocatePoolWithTag(PagedPool, sizeof(XIXFS_LCB),TAG_LCB);
		if(!LCB)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
				("ERROR xixfs_AllocateLCB Can't allocate\n"));
			//debug message
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}


	RtlZeroMemory(LCB,sizeof(XIXFS_LCB));

	LCB->FileName.Buffer = ExAllocatePoolWithTag(PagedPool, SECTORALIGNSIZE_512(Length), TAG_FILE_NAME);
	if(!LCB->FileName.Buffer){
		if(IsFromLookasideList){
			ExFreeToNPagedLookasideList(&(XifsLcbLookasideList),LCB);
		}else{
			ExFreePool(LCB);
		}
		return NULL;
	}

	
	LCB->FileName.Length = Length;
	LCB->FileName.MaximumLength = SECTORALIGNSIZE_512(Length);

	RtlZeroMemory(LCB->FileName.Buffer, LCB->FileName.MaximumLength);

	LCB->IgnoreCaseFileName.Buffer = ExAllocatePoolWithTag(PagedPool, SECTORALIGNSIZE_512(Length), TAG_FILE_NAME);
	if(!LCB->IgnoreCaseFileName.Buffer){
		
		ExFreePool(LCB->IgnoreCaseFileName.Buffer);

		if(IsFromLookasideList){
			ExFreeToNPagedLookasideList(&(XifsLcbLookasideList),LCB);
		}else{
			ExFreePool(LCB);
		}
		return NULL;
	}

	RtlZeroMemory(LCB->IgnoreCaseFileName.Buffer, LCB->IgnoreCaseFileName.MaximumLength);

	LCB->IgnoreCaseFileName.Length = Length;
	LCB->IgnoreCaseFileName.MaximumLength = SECTORALIGNSIZE_512(Length);



	LCB->NodeTypeCode = XIFS_NODE_LCB;
	LCB->NodeByteSize = sizeof(sizeof(XIXFS_LCB));
	//InterlockedExchange(&(CCB->NodeIdentifier.RefCount), 1);
	
	if (!IsFromLookasideList) {
		XIXCORE_SET_FLAGS(LCB->LCBFlags, XIFSD_LCB_NOT_FROM_POOL);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
			(" xixfs_AllocateLCB LCBFlags(0x%x)\n",LCB->LCBFlags));
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Exit xixfs_AllocateLCB (%p)\n", LCB));
	return LCB;	
}



void
xixfs_FreeLCB(
	PXIXFS_LCB pLCB
)
{	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Enter xixfs_FreeLCB \n"));


	ASSERT(pLCB);
	

	if(pLCB->FileName.Buffer){
		ExFreePool(pLCB->FileName.Buffer);
		pLCB->FileName.Buffer = NULL;
		pLCB->FileName.Length = 0;
		pLCB->FileName.MaximumLength = 0;
	}

	if(pLCB->IgnoreCaseFileName.Buffer){
		ExFreePool(pLCB->IgnoreCaseFileName.Buffer);
		pLCB->IgnoreCaseFileName.Buffer = NULL;
		pLCB->IgnoreCaseFileName.Length = 0;
		pLCB->IgnoreCaseFileName.MaximumLength = 0;
	}


	// give back memory either to the zone or to the VMM
	if (!(pLCB->LCBFlags& XIFSD_LCB_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsLcbLookasideList),pLCB);
	} else {
		ExFreePool(pLCB);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Exit xixfs_FreeLCB \n"));
	return;
}



PXIXFS_IRPCONTEXT 
xixfs_AllocateIrpContext(
	PIRP				Irp,
	PDEVICE_OBJECT		PtrTargetDeviceObject
)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIXFS_IRPCONTEXT 	IrpContext = NULL;
	PIO_STACK_LOCATION	PtrIoStackLocation = NULL;
	BOOLEAN				IsFsDo = FALSE;
	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Enter xixfs_AllocateIrpContext Irp(%p) TargetDevObj(%p)\n", Irp, PtrTargetDeviceObject));

	ASSERT(Irp);

	PtrIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
	
	if(PtrIoStackLocation->DeviceObject == XiGlobalData.XifsControlDeviceObject){
		IsFsDo = TRUE;
	}


	if(IsFsDo){
		if(PtrIoStackLocation->FileObject != NULL 
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_CREATE
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_CLEANUP
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_CLOSE
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_FILE_SYSTEM_CONTROL
		)
		{
			ExRaiseStatus(STATUS_INVALID_DEVICE_REQUEST);
		}
		
		ASSERT(PtrIoStackLocation->FileObject != NULL ||
			(PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->MinorFunction == IRP_MN_USER_FS_REQUEST &&
				PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == FSCTL_INVALIDATE_VOLUMES) 
			|| (PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == NDAS_XIXFS_UNLOAD) 
			|| (PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->MinorFunction == IRP_MN_MOUNT_VOLUME ) 
			|| PtrIoStackLocation->MajorFunction == IRP_MJ_SHUTDOWN 
		);
	}





	// allocate memory
	IrpContext = (PXIXFS_IRPCONTEXT)ExAllocateFromNPagedLookasideList(&(XifsIrpContextLookasideList));

	if(!IrpContext)
	{
		IrpContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIXFS_IRPCONTEXT),TAG_IPCONTEXT);
		if(!IrpContext)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("ERROR xixfs_AllocateIrpContext Can't allocate\n"));
			
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}

	RtlZeroMemory(IrpContext,sizeof(XIXFS_IRPCONTEXT));

	IrpContext->NodeTypeCode = XIFS_NODE_IRPCONTEXT;
	IrpContext->NodeByteSize = sizeof(XIXFS_IRPCONTEXT);
	IrpContext->TargetDeviceObject = PtrTargetDeviceObject;



	IrpContext->Irp = Irp;

	if (IoGetTopLevelIrp() != Irp) {
		// We are not top-level. Note this fact in the context structure
		XIXCORE_SET_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL), 
			("xixfs_AllocateIrpContext IrpContextFlags(0x%x)\n",
				IrpContext->IrpContextFlags));
	}
		


	IrpContext->MajorFunction = PtrIoStackLocation->MajorFunction;
	IrpContext->MinorFunction = PtrIoStackLocation->MinorFunction;
	IrpContext->SavedExceptionCode = STATUS_SUCCESS;		

	if(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){

	}else if (PtrIoStackLocation->FileObject == NULL){
		XIXCORE_SET_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("xixfs_AllocateIrpContext Set Watable form FileObject== NULL IrpContextFlags(0x%x)\n",
					IrpContext->IrpContextFlags));
	} else {
		if (IoIsOperationSynchronous(Irp)) {
			XIXCORE_SET_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("xixfs_AllocateIrpContext Set Watable form IoIsOperationSynchronous IrpContextFlags(0x%x)\n",
					IrpContext->IrpContextFlags));

		}
	}
		
	if(!IsFsDo){
		IrpContext->VCB = &((PXI_VOLUME_DEVICE_OBJECT)PtrIoStackLocation->DeviceObject)->VCB;
	}
	
	if (IsFromLookasideList == FALSE) {
		XIXCORE_SET_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_FROM_POOL);
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
			("xixfs_AllocateIrpContext IrpContextFlags(0x%x)\n",
				IrpContext->IrpContextFlags));

	}


	if ( IoGetTopLevelIrp() != Irp) {

		XIXCORE_SET_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_RECURSIVE_CALL);
	}


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT),
			("[IrpCxt(%p) INFO] Irp(%x):MJ(%x):MN(%x):Flags(%x)\n",
				IrpContext, Irp, IrpContext->MajorFunction, IrpContext->MinorFunction, IrpContext->IrpContextFlags));


	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Exit xixfs_AllocateIrpContext IrpContext(%p) Irp(%p) TargetDevObj(%p)\n",
			IrpContext, Irp, PtrTargetDeviceObject));

	return IrpContext;
}


void 
xixfs_ReleaseIrpContext(
	PXIXFS_IRPCONTEXT					PtrIrpContext)
{
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Enter xixfs_ReleaseIrpContext(%p)\n", PtrIrpContext));

	ASSERT(PtrIrpContext);

	// give back memory either to the zone or to the VMM
	if (!(PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsIrpContextLookasideList),PtrIrpContext);
	} else {
		ExFreePool(PtrIrpContext);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Exit xixfs_ReleaseIrpContext\n"));
	return;
}



PXIFS_CLOSE_FCB_CTX	
xixfs_AllocateCloseFcbCtx(VOID)
{
	BOOLEAN						IsFromLookasideList = FALSE;
	PXIFS_CLOSE_FCB_CTX			Ctx = NULL;

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Enter xixfs_AllocateCloseFcbCtx \n"));

	// allocate memory
	Ctx = ExAllocateFromNPagedLookasideList(&(XifsCloseFcbCtxList));
	if(!Ctx)
	{
		Ctx = ExAllocatePoolWithTag(PagedPool, sizeof(XIFS_CLOSE_FCB_CTX),TAG_CLOSEFCBCTX);
		if(!Ctx)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CLOSE), 
				("ERROR xixfs_AllocateCloseFcbCtx Can't allocate\n"));
			//debug message
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}


	RtlZeroMemory(Ctx,sizeof(XIFS_CLOSE_FCB_CTX));

	InitializeListHead(&(Ctx->DelayedCloseLink));
	
	if (!IsFromLookasideList) {
		XIXCORE_SET_FLAGS(Ctx->CtxFlags, XIFSD_CTX_FLAGS_NOT_FROM_POOL);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE), 
			(" xixfs_AllocateCloseFcbCtx CtxFlags(0x%x)\n",Ctx->CtxFlags));
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Exit xixfs_AllocateCloseFcbCtx (%p)\n", Ctx));
	return Ctx;	
}



void
xixfs_FreeCloseFcbCtx(
	PXIFS_CLOSE_FCB_CTX pCtx
)
{	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Enter xixfs_FreeCloseFcbCtx \n"));


	ASSERT(pCtx);
	
	// give back memory either to the zone or to the VMM
	if (!XIXCORE_TEST_FLAGS(pCtx->CtxFlags,  XIFSD_CTX_FLAGS_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsCloseFcbCtxList),pCtx);
	} else {
		ExFreePool(pCtx);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Exit xixfs_FreeCloseFcbCtx \n"));
	return;
}