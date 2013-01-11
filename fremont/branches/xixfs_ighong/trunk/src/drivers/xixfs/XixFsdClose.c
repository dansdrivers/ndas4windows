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


typedef struct _DONE_CLOSE_CTX {
	PXIFS_FCB	pFCB;
}DONE_CLOSE_CTX, *PDONE_CLOSE_CTX;


NTSTATUS
XixFsdTryClose(
	IN BOOLEAN	CanWait,
	IN BOOLEAN	bbUserReference,
	IN PXIFS_FCB	pFCB
	);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdTryClose)
#pragma alloc_text(PAGE, XixFsdCommonClose)
#endif


NTSTATUS
XixFsdTryClose(
	IN BOOLEAN	CanWait,
	IN BOOLEAN	bUserReference,
	IN PXIFS_FCB	pFCB
	)
{
	PXIFS_VCB		pVCB = NULL;
	NTSTATUS		RC = STATUS_SUCCESS;
	uint32			UserReference = 0;


	PAGED_CODE();

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	if(bUserReference == TRUE){
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

			DbgPrint("dec CCB user ref Count CCB with  VCB (%d/%d) FCB:%wZ (%d/%d)\n",
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 &pFCB->FCBName,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference);	


			DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
						 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pFCB->FCBReference,
						 pFCB->FCBUserReference ));
			
			XifsdUnlockVcb(TRUE, pVCB);
		}
			
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			 ("XifsdCloseFCB Fail Insert delete queue FCB Lotnumber(%I64d)\n", pFCB->LotNumber));
		
		XixFsdInsertCloseQueue(pFCB);
		
		RC =  STATUS_UNSUCCESSFUL;
	}else{
		if(!XixFsdCloseFCB(CanWait, pVCB, pFCB, UserReference)){
			if(bUserReference){
				XifsdLockVcb(TRUE, pVCB);
				
				DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
							 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
							 pVCB->VCBReference,
							 pVCB->VCBUserReference,
							 pFCB->FCBReference,
							 pFCB->FCBUserReference ));

				
				XifsdDecRefCount(pFCB, 0, 1);

				DbgPrint("dec CCB user ref Count CCB with  VCB (%d/%d) FCB:%wZ (%d/%d)\n",
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 &pFCB->FCBName,
						 pFCB->FCBReference,
						 pFCB->FCBUserReference);	


				DebugTrace(DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT),
							 ("XifsdCloseFCB, Fcb %08x  Vcb %d/%d Fcb %d/%d\n", pFCB,
							 pVCB->VCBReference,
							 pVCB->VCBUserReference,
							 pFCB->FCBReference,
							 pFCB->FCBUserReference ));
				
				XifsdUnlockVcb(TRUE, pVCB);
			}
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
				 ("XifsdCloseFCB Fail Insert delete queue FCB Lotnumber(%I64d)\n", pFCB->LotNumber));
			
			XixFsdInsertCloseQueue(pFCB);
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
XixFsdCommonClose(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_VCB				pVCB = NULL;
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
					("Enter XixFsdCommonClose IrpContext(%p)\n", pIrpContext));



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
			XixFsdCompleteRequest(pIrpContext,RC,0);
			return(RC);
		}

	}
	
	if(pIrpContext->VCB == NULL){

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CLOSE, 
					("pIrpContext->VCB == NULL.\n"));
		RC = STATUS_SUCCESS;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);





	if(TypeOfOpen == UnopenedFileObject){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT),
					("TypeOfOpen <= StreamFileOpen.\n"));
		XixFsdCompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
		return STATUS_SUCCESS;
	}
	

	if(TypeOfOpen == UserVolumeOpen){
		ForceDismount = XifsdCheckFlagBoolean( pCCB->CCBFlags, XIFSD_CCB_DISMOUNT_ON_CLOSE);
		if(ForceDismount){
			if(!CanWait){
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT),
					("Force Dismount with Non Waitable Context.\n"));

				RC = XixFsdPostRequest(pIrpContext, pIrp);
				return RC;
			}
		}
	}


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Close (%wZ) pCCB(%p) FileObject(%p)\n", &pFCB->FCBName, pCCB, pFileObject));


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	if( (pVCB->VCBCleanup == 0)
		&& (pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED) )
	{
		if(!CanWait){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT),
				("Force Dismount with Non Waitable Context.\n"));

			RC = XixFsdPostRequest(pIrpContext, pIrp);
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

			XixFsdFreeCCB( pCCB );
		}




		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_REFCOUNT|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
					 ("XifsdCommonClose GenINFO Fcb LotNumber(%I64d)  TypeOfOpen (%ld) Fcb %d/%d  Vcb %d/%d \n", 
					 pFCB->LotNumber,
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
					

					if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_MODIFIED_FILE)){
						XixFsdLastUpdateFileFromFCB(pFCB);
						
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

			XixFsdTryClose(CanWait, bUserReference, pFCB);

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
				if(!XixFsdCheckForDismount(pIrpContext, pVCB, TRUE)){
					bVcbAcq = FALSE;
					try_return(RC = STATUS_SUCCESS);
				}
			}

			RC = XixFsdTryClose(CanWait, bUserReference, pFCB);
			
			if(NT_SUCCESS(RC)){
				if(PotentialVCBUnmount && bVcbAcq){
					if(!XixFsdCheckForDismount(pIrpContext, pVCB, FALSE)){
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
	
	XixFsdCompleteRequest(pIrpContext, RC, 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_IRPCONTEXT), 
					("Exit XixFsdCommonClose \n"));
	return RC;
}