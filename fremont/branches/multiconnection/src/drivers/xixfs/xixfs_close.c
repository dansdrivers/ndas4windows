#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"


typedef struct _DONE_CLOSE_CTX {
	PXIXFS_FCB	pFCB;
}DONE_CLOSE_CTX, *PDONE_CLOSE_CTX;


NTSTATUS
xixfs_TryClose(
	IN BOOLEAN	CanWait,
	IN BOOLEAN	bbUserReference,
	IN PXIXFS_FCB	pFCB
	);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_TryClose)
#pragma alloc_text(PAGE, xixfs_CommonClose)
#endif


NTSTATUS
xixfs_TryClose(
	IN BOOLEAN	CanWait,
	IN BOOLEAN	bUserReference,
	IN PXIXFS_FCB	pFCB
	)
{
	PXIXFS_VCB		pVCB = NULL;
	NTSTATUS		RC = STATUS_SUCCESS;
	uint32			UserReference = 0;


	PAGED_CODE();

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	if(bUserReference){
		UserReference = 1;
	}else{
		UserReference = 0;
	}


	
	XifsdDecCloseCount(pFCB);
	


	if(!CanWait){
		if(bUserReference){
			XifsdLockVcb(TRUE, pVCB);
			
			DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
						 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pFCB->FCBReference,
						 pFCB->FCBUserReference ));

			
			XifsdDecRefCount(pFCB, 0, 1);
			/*
			DbgPrint("dec CCB user ref Count CCB with  VCB (%d/%d)  (%d/%d)\n",
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference);	
			*/

			DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
						 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pFCB->FCBReference,
						 pFCB->FCBUserReference ));
			
			XifsdUnlockVcb(TRUE, pVCB);
		}
			
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			 ("XifsdCloseFCB Fail Insert delete queue FCB Lotnumber(%I64d)\n", pFCB->XixcoreFcb.LotNumber));
		
		xixfs_InsertCloseQueue(pFCB);
		
		RC =  STATUS_UNSUCCESSFUL;
	}else{
		if(!xixfs_CloseFCB(CanWait, pVCB, pFCB, UserReference)){
			if(bUserReference){
				XifsdLockVcb(TRUE, pVCB);
				
				DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
							 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
							 pVCB->VCBReference,
							 pVCB->VCBUserReference,
							 pFCB->FCBReference,
							 pFCB->FCBUserReference ));

				
				XifsdDecRefCount(pFCB, 0, 1);
				/*
				DbgPrint("dec CCB user ref Count CCB with  VCB (%d/%d)  (%d/%d)\n",
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pFCB->FCBReference,
						 pFCB->FCBUserReference);	
				*/

				DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
							 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
							 pVCB->VCBReference,
							 pVCB->VCBUserReference,
							 pFCB->FCBReference,
							 pFCB->FCBUserReference ));
				
				XifsdUnlockVcb(TRUE, pVCB);
			}
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
				 ("XifsdCloseFCB Fail Insert delete queue FCB Lotnumber(%I64d)\n", pFCB->XixcoreFcb.LotNumber));
			
			xixfs_InsertCloseQueue(pFCB);
			RC =  STATUS_UNSUCCESSFUL;
		}

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
				 ("XifsdCommonFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
				 pVCB->VCBReference,
				 pVCB->VCBUserReference,
				 pFCB->FCBReference,
				 pFCB->FCBUserReference ));

	}

	return RC;
}


NTSTATUS
xixfs_CommonClose(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_VCB				pVCB = NULL;
	PFILE_OBJECT			pFileObject = NULL;	
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	BOOLEAN					OpenVCB = FALSE;
	BOOLEAN					PotentialVCBUnmount = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					ForceDismount = FALSE;
	BOOLEAN					bUserReference = FALSE;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;
	BOOLEAN					bVcbAcq = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT), 
					("Enter xixfs_CommonClose IrpContext(%p)\n", pIrpContext));



	ASSERT_IRPCONTEXT(pIrpContext);
	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);


	// check if open request is releated to file system CDO
	{
		PDEVICE_OBJECT	DeviceObject = pIrpContext->TargetDeviceObject;
		ASSERT(DeviceObject);
		
		if (DeviceObject == XiGlobalData.XifsControlDeviceObject) {
			RC = STATUS_SUCCESS;
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT), 
					("CDO Device Close DevObj(%p).\n", DeviceObject));
			xixfs_CompleteRequest(pIrpContext,RC,0);
			return(RC);
		}

	}
	
	if(pIrpContext->VCB == NULL){

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CLOSE, 
					("pIrpContext->VCB == NULL.\n"));
		RC = STATUS_SUCCESS;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	CanWait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = xixfs_DecodeFileObject(pFileObject, &pFCB, &pCCB);





	if(TypeOfOpen == UnopenedFileObject){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT),
					("TypeOfOpen <= StreamFileOpen.\n"));
		xixfs_CompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
		return STATUS_SUCCESS;
	}
	

	if(TypeOfOpen == UserVolumeOpen){
		ForceDismount = XIXCORE_TEST_FLAGS( pCCB->CCBFlags, XIXFSD_CCB_DISMOUNT_ON_CLOSE);
		if(ForceDismount){
			if(!CanWait){
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT),
					("Force Dismount with Non Waitable Context.\n"));

				RC = xixfs_PostRequest(pIrpContext, pIrp);
				return RC;
			}
		}
	}


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Close  pCCB(%p) FileObject(%p)\n", pCCB, pFileObject));


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	if( (pVCB->VCBCleanup == 0)
		&& (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED) )
	{
		if(!CanWait){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT),
				("Force Dismount with Non Waitable Context.\n"));

			RC = xixfs_PostRequest(pIrpContext, pIrp);
			return RC;
		}
	}




	//
    //  Clean up any CCB associated with this open.
    //

	try{

	


		if( ((TypeOfOpen == UserDirectoryOpen) || (TypeOfOpen == UserFileOpen) || (TypeOfOpen == UserVolumeOpen))
				&& (pCCB != NULL)
		) {
			bUserReference = TRUE;
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_CCB),
					 ("XifsdCommonClose Delete CCB (%x)\n", pCCB));
			
			// Test
			//XifsdLockFcb(NULL, pFCB);
			//RemoveEntryList(&pCCB->LinkToFCB);
			//XifsdUnlockFcb(NULL, pFCB);

			xixfs_FreeCCB( pCCB );
		}




		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
					 ("XifsdCommonClose GenINFO Fcb LotNumber(%I64d)  TypeOfOpen (%ld) Fcb %d/%d  Vcb %d/%d \n", 
					 pFCB->XixcoreFcb.LotNumber,
					 TypeOfOpen,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference));




		/*
		if(CanWait){
			if(TypeOfOpen == UserFileOpen){

				if(pFCB->FCBCleanup == 0){
					if(pFCB->SectionObject.DataSectionObject != NULL) 
					{
						CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);

						CcPurgeCacheSection( &(pFCB->SectionObject),
															NULL,
															0,
															FALSE 
															);					
					}
					

					if(XIXCORE_TEST_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_FILE)){
						xixfs_LastUpdateFileFromFCB(pFCB);
						
					}
				}
			}
		}
		*/


		if((pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED)
			&& ((TypeOfOpen == UserFileOpen) || (TypeOfOpen == UserDirectoryOpen))
			&& (pFCB != pVCB->RootDirFCB))
		{

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
					 ("XifsdCommonClose Destroy FCB (%x)\n", pFCB));

			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
					 ("XifsdCommonClose, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));

			xixfs_TryClose(CanWait, bUserReference, pFCB);

			try_return(RC = STATUS_SUCCESS);


		}else{
			
			if( ((pVCB->VCBCleanup == 0) || ForceDismount) 
				&& (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED))
			{


				if(ForceDismount){
					FsRtlNotifyVolumeEvent(pFileObject, FSRTL_VOLUME_DISMOUNT);
											
				}
	
				if( ((pVCB->VCBCleanup == 0) || ForceDismount)
					&& (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED))
				{
 
					PotentialVCBUnmount = TRUE;
					if(bVcbAcq)XifsdReleaseVcb(TRUE,pVCB);
					XifsdAcquireGData(pIrpContext);
					
					DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_RESOURCE),
						("XifsdCommonClose Acquire exclusive GData(%p)\n", &XiGlobalData.DataResource));
					
					bVcbAcq = XifsdAcquireVcbExclusive(TRUE,pVCB, FALSE);
					
					DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB|DEBUG_TARGET_RESOURCE),
						("XifsdCommonClose Acquire exclusive VCBResource(%p)\n", pVCB->VCBResource));
				}

			}
		


			if(PotentialVCBUnmount && bVcbAcq){
				if(!xixfs_CheckForDismount(pIrpContext, pVCB, TRUE)){
					bVcbAcq = FALSE;
					try_return(RC = STATUS_SUCCESS);
				}
			}

			RC = xixfs_TryClose(CanWait, bUserReference, pFCB);
			
			if(NT_SUCCESS(RC)){
				if(PotentialVCBUnmount && bVcbAcq){
					if(!xixfs_CheckForDismount(pIrpContext, pVCB, FALSE)){
						bVcbAcq = FALSE;
						try_return(RC = STATUS_SUCCESS);
					}
				}
			}

			RC = STATUS_SUCCESS;

		}
		
	}finally{
		if(bVcbAcq){
			XifsdReleaseVcb(TRUE, pVCB);
		}

		if(PotentialVCBUnmount){
			XifsdReleaseGData(pIrpContext);
			PotentialVCBUnmount = FALSE;
		}

	}
	
	xixfs_CompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT), 
					("Exit xixfs_CommonClose \n"));
	return RC;
}