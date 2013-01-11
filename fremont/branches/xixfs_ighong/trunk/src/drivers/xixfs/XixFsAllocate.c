#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"
#include "XixFsGlobalData.h"
#include "XixFsdInternalApi.h"



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdAllocateFCB)
#pragma alloc_text(PAGE, XixFsdFreeFCB)
#pragma alloc_text(PAGE, XixFsdAllocateCCB)
#pragma alloc_text(PAGE, XixFsdFreeCCB)
#pragma alloc_text(PAGE, XixFsdAllocateLCB)
#pragma alloc_text(PAGE, XixFsdFreeLCB)
#pragma alloc_text(PAGE, XixFsdAllocateIrpContext)
#pragma alloc_text(PAGE, XixFsdReleaseIrpContext)
#endif


PXIFS_FCB 	
XixFsdAllocateFCB(VOID)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIFS_FCB			FCB = NULL;
	uint32				i = 0;
	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Enter XixFsdAllocateFCB\n"));

	// allocate memory
	FCB = ExAllocateFromNPagedLookasideList(&(XifsFcbLookasideList));
	if(!FCB)
	{
		FCB = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIFS_FCB),TAG_FCB);
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
	
	RtlZeroMemory(FCB,sizeof(XIFS_FCB));
	
	if (!IsFromLookasideList) {
		XifsdSetFlag(FCB->FCBFlags, XIFSD_FCB_NOT_FROM_POOL);
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
			("XixFsdAllocateFCB FCBFlags(0x%x)\n",FCB->FCBFlags));
	}
		
	FCB->NodeTypeCode = XIFS_NODE_FCB;
	FCB->NodeByteSize = sizeof(sizeof(XIFS_FCB));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Exit XixFsdAllocateFCB(%p)\n",FCB));
	return FCB;	
}



void
XixFsdFreeFCB(
	PXIFS_FCB pFCB
)
{
		
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Enter XixFsdFreeFCB (%p)\n",pFCB));

	// give back memory either to the zone or to the VMM
	if (!(pFCB->FCBFlags& XIFSD_FCB_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsFcbLookasideList),pFCB);
	} else {
		ExFreePool(pFCB);
	}

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB), 
		("Exit XixFsdFreeFCB\n"));

	return;

}


PXIFS_CCB	
XixFsdAllocateCCB(VOID)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIFS_CCB			CCB = NULL;

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Enter XixFsdAllocateCCB\n"));


	// allocate memory
	CCB = ExAllocateFromNPagedLookasideList(&(XifsCcbLookasideList));
	if(!CCB)
	{
		CCB = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIFS_CCB),TAG_CCB);
		if(!CCB)
		{
			//debug message
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
				("ERROR XixFsdAllocateCCB Can't allocate\n"));
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}

	RtlZeroMemory(CCB,sizeof(XIFS_CCB));

	CCB->NodeTypeCode = XIFS_NODE_CCB;
	CCB->NodeByteSize = sizeof(sizeof(XIFS_CCB));
	
	InitializeListHead(&CCB->LinkToFCB);

	
	if (!IsFromLookasideList) {
		XifsdSetFlag(CCB->CCBFlags, XIFSD_CCB_NOT_FROM_POOL);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
			("XixFsdAllocateCCB CCBFlags(0x%x)\n",CCB->CCBFlags));
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Exit XixFsdAllocateCCB(%p)\n",CCB));

	return CCB;	
}


void
XixFsdFreeCCB(
	PXIFS_CCB pCCB
)
{	

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Enter XixFsdFreeCCB (%p)\n",pCCB));

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
	if (!(pCCB->CCBFlags& XIFSD_CCB_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsCcbLookasideList),pCCB);
	} else {
		ExFreePool(pCCB);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_CCB), 
		("Exit XixFsdFreeCCB \n"));
	return;
}


PXIFS_LCB	
XixFsdAllocateLCB(uint16 Length)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIFS_LCB			LCB = NULL;

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Enter XixFsAllocateLCB \n"));

	// allocate memory
	LCB = ExAllocateFromNPagedLookasideList(&(XifsLcbLookasideList));
	if(!LCB)
	{
		LCB = ExAllocatePoolWithTag(PagedPool, sizeof(XIFS_LCB),TAG_LCB);
		if(!LCB)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
				("ERROR XixFsdAllocateLCB Can't allocate\n"));
			//debug message
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}


	RtlZeroMemory(LCB,sizeof(XIFS_LCB));

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
	LCB->NodeByteSize = sizeof(sizeof(XIFS_LCB));
	//InterlockedExchange(&(CCB->NodeIdentifier.RefCount), 1);
	
	if (!IsFromLookasideList) {
		XifsdSetFlag(LCB->LCBFlags, XIFSD_LCB_NOT_FROM_POOL);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
			(" XixFsdAllocateLCB LCBFlags(0x%x)\n",LCB->LCBFlags));
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Exit XixFsdAllocateLCB (%p)\n", LCB));
	return LCB;	
}



void
XixFsdFreeLCB(
	PXIFS_LCB pLCB
)
{	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_LCB), 
		("Enter XixFsdFreeLCB \n"));


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
		("Exit XixFsdFreeLCB \n"));
	return;
}



PXIFS_IRPCONTEXT 
XixFsdAllocateIrpContext(
	PIRP				Irp,
	PDEVICE_OBJECT		PtrTargetDeviceObject
)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIFS_IRPCONTEXT 	IrpContext = NULL;
	PIO_STACK_LOCATION	PtrIoStackLocation = NULL;
	BOOLEAN				IsFsDo = FALSE;
	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Enter XixFsdAllocateIrpContext Irp(%p) TargetDevObj(%p)\n", Irp, PtrTargetDeviceObject));

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
		)
		{
			ExRaiseStatus(STATUS_INVALID_DEVICE_REQUEST);
		}
		
		ASSERT(PtrIoStackLocation->FileObject != NULL ||
			(PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->MinorFunction == IRP_MN_USER_FS_REQUEST &&
				PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == FSCTL_INVALIDATE_VOLUMES) 
			|| (PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->MinorFunction == IRP_MN_MOUNT_VOLUME ) 
			|| PtrIoStackLocation->MajorFunction == IRP_MJ_SHUTDOWN 
		);
	}





	// allocate memory
	IrpContext = (PXIFS_IRPCONTEXT)ExAllocateFromNPagedLookasideList(&(XifsIrpContextLookasideList));

	if(!IrpContext)
	{
		IrpContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIFS_IRPCONTEXT),TAG_IPCONTEXT);
		if(!IrpContext)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("ERROR XixFsdAllocateIrpContext Can't allocate\n"));
			
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}

	RtlZeroMemory(IrpContext,sizeof(XIFS_IRPCONTEXT));

	IrpContext->NodeTypeCode = XIFS_NODE_IRPCONTEXT;
	IrpContext->NodeByteSize = sizeof(XIFS_IRPCONTEXT);
	IrpContext->TargetDeviceObject = PtrTargetDeviceObject;



	IrpContext->Irp = Irp;

	if (IoGetTopLevelIrp() != Irp) {
		// We are not top-level. Note this fact in the context structure
		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL), 
			("XixFsdAllocateIrpContext IrpContextFlags(0x%x)\n",
				IrpContext->IrpContextFlags));
	}
		


	IrpContext->MajorFunction = PtrIoStackLocation->MajorFunction;
	IrpContext->MinorFunction = PtrIoStackLocation->MinorFunction;
		

	if(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){

	}else if (PtrIoStackLocation->FileObject == NULL){
		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("XixFsdAllocateIrpContext Set Watable form FileObject== NULL IrpContextFlags(0x%x)\n",
					IrpContext->IrpContextFlags));
	} else {
		if (IoIsOperationSynchronous(Irp)) {
			XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("XixFsdAllocateIrpContext Set Watable form IoIsOperationSynchronous IrpContextFlags(0x%x)\n",
					IrpContext->IrpContextFlags));

		}
	}
		
	if(!IsFsDo){
		IrpContext->VCB = &((PXI_VOLUME_DEVICE_OBJECT)PtrIoStackLocation->DeviceObject)->VCB;
	}
	
	if (IsFromLookasideList == FALSE) {
		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_FROM_POOL);
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
			("XixFsdAllocateIrpContext IrpContextFlags(0x%x)\n",
				IrpContext->IrpContextFlags));

	}


	if ( IoGetTopLevelIrp() != Irp) {

		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_RECURSIVE_CALL);
	}


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT),
			("[IrpCxt(%p) INFO] Irp(%x):MJ(%x):MN(%x):Flags(%x)\n",
				IrpContext, Irp, IrpContext->MajorFunction, IrpContext->MinorFunction, IrpContext->IrpContextFlags));


	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Exit XixFsdAllocateIrpContext IrpContext(%p) Irp(%p) TargetDevObj(%p)\n",
			IrpContext, Irp, PtrTargetDeviceObject));

	return IrpContext;
}


void 
XixFsdReleaseIrpContext(
	PXIFS_IRPCONTEXT					PtrIrpContext)
{
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Enter XixFsdReleaseIrpContext(%p)\n", PtrIrpContext));

	ASSERT(PtrIrpContext);

	// give back memory either to the zone or to the VMM
	if (!(PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsIrpContextLookasideList),PtrIrpContext);
	} else {
		ExFreePool(PtrIrpContext);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Exit XixFsdReleaseIrpContext\n"));
	return;
}



PXIFS_CLOSE_FCB_CTX	
XixFsdAllocateCloseFcbCtx(VOID)
{
	BOOLEAN						IsFromLookasideList = FALSE;
	PXIFS_CLOSE_FCB_CTX			Ctx = NULL;

	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Enter XixFsdAllocateCloseFcbCtx \n"));

	// allocate memory
	Ctx = ExAllocateFromNPagedLookasideList(&(XifsCloseFcbCtxList));
	if(!Ctx)
	{
		Ctx = ExAllocatePoolWithTag(PagedPool, sizeof(XIFS_CLOSE_FCB_CTX),TAG_CLOSEFCBCTX);
		if(!Ctx)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CLOSE), 
				("ERROR XixFsdAllocateCloseFcbCtx Can't allocate\n"));
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
		XifsdSetFlag(Ctx->CtxFlags, XIFSD_CTX_FLAGS_NOT_FROM_POOL);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE), 
			(" XixFsdAllocateCloseFcbCtx CtxFlags(0x%x)\n",Ctx->CtxFlags));
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Exit XixFsdAllocateCloseFcbCtx (%p)\n", Ctx));
	return Ctx;	
}



void
XixFsdFreeCloseFcbCtx(
	PXIFS_CLOSE_FCB_CTX pCtx
)
{	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Enter XixFsdFreeCloseFcbCtx \n"));


	ASSERT(pCtx);
	
	// give back memory either to the zone or to the VMM
	if (!XifsdCheckFlagBoolean(pCtx->CtxFlags,  XIFSD_CTX_FLAGS_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsCloseFcbCtxList),pCtx);
	} else {
		ExFreePool(pCtx);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLOSE), 
		("Exit XixFsdFreeCloseFcbCtx \n"));
	return;
}