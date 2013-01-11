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
#pragma alloc_text(PAGE, XixFsdCommonDeviceControl)
#endif



#ifndef VOLSNAPCONTROLTYPE
#define VOLSNAPCONTROLTYPE	((ULONG) 'S')
#endif


#ifndef IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES
#define IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES	CTL_CODE(VOLSNAPCONTROLTYPE, 0, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS) 
#endif


NTSTATUS
XixFsdDevFlushAndHoldWriteCompletionRoutine
(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
)
{
	PKEVENT Event = (PKEVENT) Contxt;
	
	if(Event){
		KeSetEvent(Event, 0, FALSE);
		return STATUS_MORE_PROCESSING_REQUIRED;
	}

	return STATUS_SUCCESS;
}



NTSTATUS
XixFsdDevIoctlCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )
{

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DEVCTL, ("Enter XixFsdDevIoctlCompletion .\n"));

 	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DEVCTL, ("Exit XixFsdDevIoctlCompletion .\n"));
	return(STATUS_SUCCESS);
}


NTSTATUS
XixFsdCommonDeviceControl(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	PIRP						pIrp = NULL;
	PIO_STACK_LOCATION			pIrpSp= NULL;
	PIO_STACK_LOCATION			pNextIrpSp = NULL;
	PFILE_OBJECT				PtrFileObject = NULL;
	PXIFS_FCB					pFCB = NULL;
	PXIFS_CCB					pCCB = NULL;
	PXIFS_VCB					pVCB = NULL;
	BOOLEAN						CompleteIrp = FALSE;
	ULONG						IoControlCode = 0;
	void						*BufferPointer = NULL;
	BOOLEAN						Wait = FALSE;
	BOOLEAN						PostRequest = FALSE;
	TYPE_OF_OPEN				TypeOfOpen = UnopenedFileObject;
	KEVENT						WaitEvent;
	PVOID						CompletionContext = NULL;


	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DEVCTL, 
		("Enter XixFsdCommonDeviceControl .\n"));

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter XixFsdCommonDeviceControl \n"));


	ASSERT(pIrpContext);
	ASSERT(pIrpContext->Irp);
	pIrp = pIrpContext->Irp;
	
	


	try {	
		Wait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
		if(!Wait){
			PostRequest = TRUE;
			try_return(RC = STATUS_PENDING);
		}

		// First, get a pointer to the current I/O stack location
		pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
		ASSERT(pIrpSp);

		PtrFileObject = pIrpSp->FileObject;
		ASSERT(PtrFileObject);

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DEVCTL,
			(" Decode File Object\n"));
		
		TypeOfOpen = XixFsdDecodeFileObject( PtrFileObject, &pFCB, &pCCB );


		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_IRPCONTEXT),
			("pIrpSp->Parameters.DeviceIoControl.IoControlCode(0x%02x) .\n",
			pIrpSp->Parameters.DeviceIoControl.IoControlCode));

		if (TypeOfOpen == UserVolumeOpen) {
			pVCB = (PXIFS_VCB)(pFCB ->PtrVCB);
			ASSERT_VCB(pVCB);
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("User Volume Open Not supported!!.\n"));

			RC = STATUS_INVALID_PARAMETER;
			XixFsdCompleteRequest(pIrpContext, RC, 0);
			try_return(RC);
		}
		
		
		switch(pIrpSp->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES:

			//DbgPrint("!!! IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES\n");

			XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			XixFsdFlusVolume(pIrpContext, pVCB);
			

			KeInitializeEvent( &WaitEvent, NotificationEvent, FALSE );
			CompletionContext = &WaitEvent;
			IoCopyCurrentIrpStackLocationToNext(pIrp );
			IoSetCompletionRoutine(pIrp, 
								XixFsdDevFlushAndHoldWriteCompletionRoutine, 
								CompletionContext, 
								TRUE, 
								TRUE, 
								TRUE
								);



			break;
		default:
			 IoSkipCurrentIrpStackLocation( pIrp );
			 /*
			IoCopyCurrentIrpStackLocationToNext(pIrp );
			// Set a completion routine.
			IoSetCompletionRoutine(pIrp, XixFsdDevIoctlCompletion, NULL, TRUE, TRUE, TRUE);
			*/
			break;
		}
		

		// Send the request.
		RC = IoCallDriver(pVCB->TargetDeviceObject, pIrp);

		if(RC == STATUS_PENDING && CompletionContext){
			//DbgPrint("!!! IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES2\n");
			KeWaitForSingleObject(&WaitEvent, Executive, KernelMode, FALSE, NULL);
			RC = pIrp->IoStatus.Status;
		}


		if(CompletionContext)
		{
			/*
			DbgPrint("!!! IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES RC(0x%x) Status(0x%x) Information(%ld) \n",
				RC, pIrp->IoStatus.Status, pIrp->IoStatus.Information);
			*/
			XifsdReleaseVcb(TRUE, pVCB);
			
			XixFsdCompleteRequest(pIrpContext, RC, (uint32)pIrp->IoStatus.Information);
		}else{
			XixFsdReleaseIrpContext(pIrpContext);
		}

		

	} finally {
		if (!(pIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_EXCEPTION)) {
			;
		}

		if(PostRequest ){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_IRPCONTEXT),
					("PostRequest pIrpCotnext(0x%x) pIrp(0x%x)\n", pIrpContext, pIrp));
			RC = XixFsdPostRequest(pIrpContext, pIrp);
		}		

	}


	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DEVCTL, 
		("Exit XixFsdCommonDeviceControl .\n"));
	return(RC);
}