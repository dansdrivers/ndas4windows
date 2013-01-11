#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"

NTSTATUS
xixfs_PnpQueryRemove (
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
);


NTSTATUS
xixfs_PnpSurpriseRemove (
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
);


NTSTATUS
xixfs_PnpRemove(
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
);

NTSTATUS
xixfs_PnpCancelRemove(
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_PnpQueryRemove)
#pragma alloc_text(PAGE, xixfs_PnpSurpriseRemove)
#pragma alloc_text(PAGE, xixfs_PnpRemove)
#pragma alloc_text(PAGE, xixfs_PnpCancelRemove)
#pragma alloc_text(PAGE, xixfs_CommonPNP)
#endif



NTSTATUS
xixfs_PnpCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )
{
    PKEVENT Event = (PKEVENT) Contxt;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_PnpCompletionRoutine \n"));

    KeSetEvent( Event, 0, FALSE );


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_PnpCompletionRoutine \n"));
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
xixfs_PnpQueryRemove (
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	KEVENT		Event;
	BOOLEAN		IsPresentVCB = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_PnpQueryRemove  \n"));
	
	
	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT(pIrp);
	ASSERT_VCB(pVCB);

	

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("1 VCB->PtrVPB->ReferenceCount %d \n", 
						 pVCB->PtrVPB->ReferenceCount));



	XifsdAcquireGData(pIrpContext);
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);

	XifsdLockVcb(pIrpContext, pVCB);
	XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);


	IsPresentVCB = TRUE;
	RC = xixfs_LockVolumeInternal(pIrpContext, pVCB, NULL);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("2 VCB->PtrVPB->ReferenceCount %d \n", 
						 pVCB->PtrVPB->ReferenceCount));
	


	if(NT_SUCCESS(RC)){
		
		//
		//	Release System Resource
		//

		xixfs_CleanupFlushVCB(pIrpContext, pVCB, TRUE);

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("xixfs_PnpQueryRemove Forward IRP to low stack  .\n"));

		IoCopyCurrentIrpStackLocationToNext( pIrp );
		
		KeInitializeEvent( &Event, NotificationEvent, FALSE );
		IoSetCompletionRoutine( pIrp,
								xixfs_PnpCompletionRoutine,
								&Event,
								TRUE,
								TRUE,
								TRUE );

		//
		//  Send the request and wait.
		//

		RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("xixfs_PnpQueryRemove Forward IRP's result(0x%x)  .\n", RC));

		if (RC == STATUS_PENDING) {

			KeWaitForSingleObject( &Event,
								   Executive,
								   KernelMode,
								   FALSE,
								   NULL );

			RC = pIrp->IoStatus.Status;
		}

		if(NT_SUCCESS(RC)){

			IsPresentVCB = xixfs_CheckForDismount(pIrpContext, pVCB, TRUE);
			ASSERT(!IsPresentVCB || (pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS));
		}
	}

	
	ASSERT( !(NT_SUCCESS(RC) && IsPresentVCB && pVCB->VCBReference != 0) );

	if(IsPresentVCB){
		XifsdLockVcb(pIrpContext, pVCB);
		XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData(pIrpContext);
	xixfs_CompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XifsdPnpQueryRemove  \n"));

	return RC;
}

NTSTATUS
xixfs_PnpSurpriseRemove (
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	KEVENT		Event;
	BOOLEAN		IsPresentVCB = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_PnpSurpriseRemove  \n"));
	
	ASSERT(pIrpContext);
	ASSERT(pIrp);
	ASSERT(pVCB);

	XifsdAcquireGData(pIrpContext);
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	


	XifsdLockVcb(pIrpContext, pVCB);

	XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	
	if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
		pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
	}
	XifsdUnlockVcb(pIrpContext, pVCB);
	
	//
	//	Release System Resource
	//

	xixfs_CleanupFlushVCB(pIrpContext, pVCB, TRUE);


	IoCopyCurrentIrpStackLocationToNext( pIrp );
	
	KeInitializeEvent( &Event, NotificationEvent, FALSE );
	IoSetCompletionRoutine( pIrp,
							xixfs_PnpCompletionRoutine,
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


	IsPresentVCB = xixfs_CheckForDismount(pIrpContext, pVCB, TRUE);
	
	if(IsPresentVCB){
		XifsdLockVcb(pIrpContext, pVCB);
		XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData(pIrpContext);
	xixfs_CompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_PnpSurpriseRemove  \n"));
	return RC;

}

NTSTATUS
xixfs_PnpRemove(
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	KEVENT		Event;
	BOOLEAN		IsPresentVCB = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_PnpRemove  \n"));
	
	ASSERT(pIrpContext);
	ASSERT(pIrp);
	ASSERT(pVCB);


	XifsdAcquireGData(pIrpContext);
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	
	XifsdLockVcb(pIrpContext, pVCB);
	XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);

	RC = xixfs_UnlockVolumeInternal(pIrpContext, pVCB, NULL);



	if(!NT_SUCCESS(RC)){
		

		XifsdLockVcb(pIrpContext, pVCB);
		if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS){
			pVCB->VCBState = XIFSD_VCB_STATE_VOLUME_INVALID;
		}
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
		XifsdReleaseGData(pIrpContext);
		
		RC = STATUS_SUCCESS;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	//
	//	Release System Resource
	//

	xixfs_CleanupFlushVCB(pIrpContext, pVCB, TRUE);


	IoCopyCurrentIrpStackLocationToNext( pIrp );
	
	KeInitializeEvent( &Event, NotificationEvent, FALSE );
	IoSetCompletionRoutine( pIrp,
							xixfs_PnpCompletionRoutine,
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



	IsPresentVCB = xixfs_CheckForDismount(pIrpContext, pVCB, TRUE);
	
	if(IsPresentVCB){
		XifsdLockVcb(pIrpContext, pVCB);
		XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
		XifsdUnlockVcb(pIrpContext, pVCB);

		XifsdReleaseVcb(TRUE, pVCB);
	}

	XifsdReleaseGData(pIrpContext);
	xixfs_CompleteRequest(pIrpContext, RC, 0);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_PnpRemove  \n"));
	return RC;

}

NTSTATUS
xixfs_PnpCancelRemove(
    PXIXFS_IRPCONTEXT	pIrpContext,
    PIRP				pIrp,
    PXIXFS_VCB			pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_PnpCancelRemove  \n"));



	ASSERT(pIrpContext);
	ASSERT(pIrp);
	ASSERT(pVCB);

	if(!XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE)){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
				 ("XifsdPostRequest  IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp ));
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}
	
	XifsdLockVcb(pIrpContext, pVCB);
	XIXCORE_SET_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);

	RC = xixfs_UnlockVolumeInternal(pIrpContext, pVCB, NULL);

	XifsdLockVcb(pIrpContext, pVCB);
	XIXCORE_CLEAR_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP);
	XifsdUnlockVcb(pIrpContext, pVCB);
	
	XifsdReleaseVcb(TRUE, pVCB);

	IoSkipCurrentIrpStackLocation( pIrp );

    RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);
	
	pIrpContext->Irp = NULL;
	xixfs_CompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_PnpCancelRemove  \n"));
	

	return RC;
}

NTSTATUS
xixfs_CommonPNP(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PIRP				pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PXI_VOLUME_DEVICE_OBJECT		pVolumeDeviceObject = NULL;
	PXIXFS_VCB						pVCB = NULL;
	uint32							PrevLevel = 0;
	uint32							PrevTarget = 0;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonPNP  \n"));

	ASSERT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);



	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	pVolumeDeviceObject = (PXI_VOLUME_DEVICE_OBJECT)pIrpSp->DeviceObject;
	
	if(pVolumeDeviceObject->DeviceObject.Size != sizeof(XI_VOLUME_DEVICE_OBJECT)
		|| (pVolumeDeviceObject->VCB.NodeTypeCode != XIFS_NODE_VCB))
	{
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				 ("xixfs_CommonPNP Invalid Parameter\n"));
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}

	if(!XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		RC = STATUS_PENDING;
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
				 ("XifsdPostRequest  IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp ));

		xixfs_PostRequest(pIrpContext, pIrp);
		
		return RC;
	}
	
	


	pVCB = &(pVolumeDeviceObject->VCB);
	ASSERT_VCB(pVCB);


	switch ( pIrpSp->MinorFunction ) 
	{
	case IRP_MN_QUERY_REMOVE_DEVICE:


		
		RC = xixfs_PnpQueryRemove( pIrpContext, pIrp, pVCB );
		
		break;

	case IRP_MN_SURPRISE_REMOVAL:

		
		RC = xixfs_PnpSurpriseRemove( pIrpContext, pIrp, pVCB );
		
		break;

	case IRP_MN_REMOVE_DEVICE:
	
		
		RC = xixfs_PnpRemove( pIrpContext, pIrp, pVCB );
		
		break;

	case IRP_MN_CANCEL_REMOVE_DEVICE:
		
		RC = xixfs_PnpCancelRemove( pIrpContext, pIrp, pVCB );
	
		break;

	default:

		IoSkipCurrentIrpStackLocation( pIrp );

		RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

		xixfs_ReleaseIrpContext(pIrpContext);

		break;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_CommonPNP  \n"));	


	

	return RC;
}