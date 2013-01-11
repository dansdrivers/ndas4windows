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


NTSTATUS
XixFsdPnpQueryRemove (
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
);


NTSTATUS
XixFsdPnpSurpriseRemove (
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
);


NTSTATUS
XixFsdPnpRemove(
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
);

NTSTATUS
XixFsdPnpCancelRemove(
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdPnpQueryRemove)
#pragma alloc_text(PAGE, XixFsdPnpSurpriseRemove)
#pragma alloc_text(PAGE, XixFsdPnpRemove)
#pragma alloc_text(PAGE, XixFsdPnpCancelRemove)
#pragma alloc_text(PAGE, XixFsdCommonPNP)
#endif



NTSTATUS
XixFsdPnpCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )
{
    PKEVENT Event = (PKEVENT) Contxt;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdPnpCompletionRoutine \n"));

    KeSetEvent( Event, 0, FALSE );


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdPnpCompletionRoutine \n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
XixFsdPnpQueryRemove (
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	KEVENT		Event;
	BOOLEAN		IsPresentVCB = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdPnpQueryRemove  \n"));
	
	
	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT(pIrp);
	ASSERT_VCB(pVCB);

	

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("1 VCB->PtrVPB->ReferenceCount %d \n", 
						 pVCB->PtrVPB->ReferenceCount));



	XifsdAcquireGData(pIrpContext);
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);

	XifsdLockVcb(pIrpContext, pVCB);
	XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);


	IsPresentVCB = TRUE;
	RC = XixFsdLockVolumeInternal(pIrpContext, pVCB, NULL);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("2 VCB->PtrVPB->ReferenceCount %d \n", 
						 pVCB->PtrVPB->ReferenceCount));
	


	if(NT_SUCCESS(RC)){
		
		//
		//	Release System Resource
		//

		XixFsdCleanupFlushVCB(pIrpContext, pVCB, TRUE);

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("XixFsdPnpQueryRemove Forward IRP to low stack  .\n"));

		IoCopyCurrentIrpStackLocationToNext( pIrp );
		
		KeInitializeEvent( &Event, NotificationEvent, FALSE );
		IoSetCompletionRoutine( pIrp,
								XixFsdPnpCompletionRoutine,
								&Event,
								TRUE,
								TRUE,
								TRUE );

		//
		//  Send the request and wait.
		//

		RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("XixFsdPnpQueryRemove Forward IRP's result(0x%x)  .\n", RC));

		if (RC == STATUS_PENDING) {

			KeWaitForSingleObject( &Event,
								   Executive,
								   KernelMode,
								   FALSE,
								   NULL );

			RC = pIrp->IoStatus.Status;
		}

		if(NT_SUCCESS(RC)){

			IsPresentVCB = XixFsdCheckForDismount(pIrpContext, pVCB, TRUE);
			ASSERT(!IsPresentVCB || (pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS));
		}
	}

	
	ASSERT( !(NT_SUCCESS(RC) && IsPresentVCB && pVCB->VCBReference != 0) );

	if(IsPresentVCB){
		XifsdLockVcb(pIrpContext, pVCB);
		XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData(pIrpContext);
	XixFsdCompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XifsdPnpQueryRemove  \n"));

	return RC;
}

NTSTATUS
XixFsdPnpSurpriseRemove (
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	KEVENT		Event;
	BOOLEAN		IsPresentVCB = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdPnpSurpriseRemove  \n"));
	
	ASSERT(pIrpContext);
	ASSERT(pIrp);
	ASSERT(pVCB);

	XifsdAcquireGData(pIrpContext);
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	


	XifsdLockVcb(pIrpContext, pVCB);

	XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	
	if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
		pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
	}
	XifsdUnlockVcb(pIrpContext, pVCB);
	
	//
	//	Release System Resource
	//

	XixFsdCleanupFlushVCB(pIrpContext, pVCB, TRUE);


	IoCopyCurrentIrpStackLocationToNext( pIrp );
	
	KeInitializeEvent( &Event, NotificationEvent, FALSE );
	IoSetCompletionRoutine( pIrp,
							XixFsdPnpCompletionRoutine,
							&Event,
							TRUE,
							TRUE,
							TRUE );

	//
	//  Send the request and wait.
	//

	RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

	if (RC == STATUS_PENDING) {

		KeWaitForSingleObject( &Event,
							   Executive,
							   KernelMode,
							   FALSE,
							   NULL );

		RC = pIrp->IoStatus.Status;
	}


	IsPresentVCB = XixFsdCheckForDismount(pIrpContext, pVCB, TRUE);
	
	if(IsPresentVCB){
		XifsdLockVcb(pIrpContext, pVCB);
		XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData(pIrpContext);
	XixFsdCompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdPnpSurpriseRemove  \n"));
	return RC;

}

NTSTATUS
XixFsdPnpRemove(
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	KEVENT		Event;
	BOOLEAN		IsPresentVCB = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdPnpRemove  \n"));
	
	ASSERT(pIrpContext);
	ASSERT(pIrp);
	ASSERT(pVCB);


	XifsdAcquireGData(pIrpContext);
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	
	XifsdLockVcb(pIrpContext, pVCB);
	XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);

	RC = XixFsdUnlockVolumeInternal(pIrpContext, pVCB, NULL);



	if(!NT_SUCCESS(RC)){
		

		XifsdLockVcb(pIrpContext, pVCB);
		if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
			pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
		}
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
		XifsdReleaseGData(pIrpContext);
		
		RC = STATUS_SUCCESS;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	//
	//	Release System Resource
	//

	XixFsdCleanupFlushVCB(pIrpContext, pVCB, TRUE);


	IoCopyCurrentIrpStackLocationToNext( pIrp );
	
	KeInitializeEvent( &Event, NotificationEvent, FALSE );
	IoSetCompletionRoutine( pIrp,
							XixFsdPnpCompletionRoutine,
							&Event,
							TRUE,
							TRUE,
							TRUE );

	//
	//  Send the request and wait.
	//

	RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

	if (RC == STATUS_PENDING) {

		KeWaitForSingleObject( &Event,
							   Executive,
							   KernelMode,
							   FALSE,
							   NULL );

		RC = pIrp->IoStatus.Status;
	}



	IsPresentVCB = XixFsdCheckForDismount(pIrpContext, pVCB, TRUE);
	
	if(IsPresentVCB){
		XifsdLockVcb(pIrpContext, pVCB);
		XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData(pIrpContext);
	XixFsdCompleteRequest(pIrpContext, RC, 0);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdPnpRemove  \n"));
	return RC;

}

NTSTATUS
XixFsdPnpCancelRemove(
    PXIFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdPnpCancelRemove  \n"));



	ASSERT(pIrpContext);
	ASSERT(pIrp);
	ASSERT(pVCB);

	if(!XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE)){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
				 ("XifsdPostRequest  IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp ));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}
	
	XifsdLockVcb(pIrpContext, pVCB);
	XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);

	RC = XixFsdUnlockVolumeInternal(pIrpContext, pVCB, NULL);

	XifsdLockVcb(pIrpContext, pVCB);
	XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);
	
	XifsdReleaseVcb(TRUE, pVCB);

	IoSkipCurrentIrpStackLocation( pIrp );

    RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);
	
	pIrpContext->Irp = NULL;
	XixFsdCompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdPnpCancelRemove  \n"));
	

	return RC;
}

NTSTATUS
XixFsdCommonPNP(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PIRP				pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PXI_VOLUME_DEVICE_OBJECT		pVolumeDeviceObject = NULL;
	PXIFS_VCB						pVCB = NULL;
	uint32							PrevLevel = 0;
	uint32							PrevTarget = 0;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonPNP  \n"));

	ASSERT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);



	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	pVolumeDeviceObject = (PXI_VOLUME_DEVICE_OBJECT)pIrpSp->DeviceObject;
	
	if(pVolumeDeviceObject->DeviceObject.Size != sizeof(XI_VOLUME_DEVICE_OBJECT)
		|| (pVolumeDeviceObject->VCB.NodeTypeCode != XIFS_NODE_VCB))
	{
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				 ("XixFsdCommonPNP Invalid Parameter\n"));
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	if(!XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		RC = STATUS_PENDING;
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
				 ("XifsdPostRequest  IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp ));

		XixFsdPostRequest(pIrpContext, pIrp);
		
		return RC;
	}
	
	//PrevLevel = XifsdDebugLevel;
	//PrevTarget = XifsdDebugTarget;
	//XifsdDebugLevel = DEBUG_LEVEL_ALL;
	//XifsdDebugTarget = DEBUG_TARGET_REFCOUNT;


	pVCB = &(pVolumeDeviceObject->VCB);
	ASSERT_VCB(pVCB);


	switch ( pIrpSp->MinorFunction ) 
	{
	case IRP_MN_QUERY_REMOVE_DEVICE:


		
		RC = XixFsdPnpQueryRemove( pIrpContext, pIrp, pVCB );
		
		break;

	case IRP_MN_SURPRISE_REMOVAL:

		
		RC = XixFsdPnpSurpriseRemove( pIrpContext, pIrp, pVCB );
		
		break;

	case IRP_MN_REMOVE_DEVICE:
	
		
		RC = XixFsdPnpRemove( pIrpContext, pIrp, pVCB );
		
		break;

	case IRP_MN_CANCEL_REMOVE_DEVICE:
		
		RC = XixFsdPnpCancelRemove( pIrpContext, pIrp, pVCB );
	
		break;

	default:

		IoSkipCurrentIrpStackLocation( pIrp );

		RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

		XixFsdReleaseIrpContext(pIrpContext);

		break;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCommonPNP  \n"));	


	//XifsdDebugLevel = PrevLevel;
	//XifsdDebugTarget = PrevTarget;

	return RC;
}