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
#pragma alloc_text(PAGE, XixFsdCommonCleanUp)
#endif


NTSTATUS
XixFsdCommonCleanUp(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIFS_FCB		pFCB = NULL;
	PXIFS_CCB		pCCB = NULL;
	PXIFS_VCB		pVCB = NULL;
	PFILE_OBJECT	pFileObject = NULL;	
	TYPE_OF_OPEN	TypeOfOpen = UnopenedFileObject;

	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	KIRQL					SavedIrql;

	PXIFS_LCB	pLCB = NULL;
	PXIFS_FCB	pParentFCB = NULL;

	BOOLEAN					Wait = FALSE;
	BOOLEAN					VCBAcquired = FALSE;
	BOOLEAN					ParentFCBAcquired = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					AttemptTeardown = FALSE;
	BOOLEAN					SendUnlockNotification = FALSE;

	PAGED_CODE();

	DebugTrace((DEBUG_LEVEL_TRACE), (DEBUG_TARGET_CLEANUP|DEBUG_TARGET_IRPCONTEXT), 
		("Enter XifsdCommonCleanUp pIrpContext(%p)\n", pIrpContext));


	ASSERT_IRPCONTEXT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	// check if open request is releated to file system CDO
	{
		PDEVICE_OBJECT	DeviceObject = pIrpContext->TargetDeviceObject;
		ASSERT(DeviceObject);
		
		if (DeviceObject == XiGlobalData.XifsControlDeviceObject) {
			RC = STATUS_SUCCESS;

			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLEANUP), 
					("CDO Device TargetDevide(%p).\n", DeviceObject));
			XixFsdCompleteRequest(pIrpContext,RC,0);
			return(RC);
		}

	}	



	if(pIrpContext->VCB == NULL){

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CLEANUP, 
					("pIrpContext->VCB == NULL.\n"));
		RC = STATUS_SUCCESS;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);

	if(TypeOfOpen <= StreamFileOpen){
		DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_CLEANUP, 
					("TypeOfOpen <= StreamFileOpen.\n"));
		XixFsdCompleteRequest(pIrpContext, STATUS_SUCCESS, 0);
		return STATUS_SUCCESS;
	}


	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	
	if(CanWait == FALSE){
		DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CLEANUP| DEBUG_TARGET_IRPCONTEXT), 
					("PostRequest IrpCxt(%p) Irp(%p)\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);




	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLEANUP| DEBUG_TARGET_RESOURCE|DEBUG_TARGET_VCB), 
				("Acquire exclusive pVCB(%p) VCBResource(%p).\n", pVCB, &pVCB->VCBResource));	

	if((TypeOfOpen == UserVolumeOpen)
		&& XifsdCheckFlagBoolean(pFileObject->Flags, FO_FILE_MODIFIED))
	{
		XifsdAcquireVcbExclusive(CanWait, pVCB, FALSE);
		VCBAcquired = TRUE;		
	}


	XifsdAcquireFcbExclusive(CanWait, pFCB, FALSE);		


	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLEANUP| DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB), 
					("Acquire exclusive FCB(%p) FCBResource(%p).\n", pFCB, pFCB->FCBResource));

	
	XifsdSetFlag(pFileObject->Flags, FO_CLEANUP_COMPLETE);
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLEANUP| DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB), 
					("Set File Object Flags(0x%x)\n", pFileObject->Flags));	



	//IoRemoveShareAccess( pFileObject, &pFCB->FCBShareAccess );

	try{
		switch(TypeOfOpen){
		case UserDirectoryOpen:
			
			if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_FLAG_NOFITY_SET))
			{
				DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("CompletionFilter Notify CleanUp (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));

				FsRtlNotifyCleanup(pVCB->NotifyIRPSync, &pVCB->NextNotifyIRP, pCCB);

				//DbgPrint("Notify CleanUp (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB);

			}


			//DbgPrint("CleanUp (%wZ) pCCB(%p) pCCB->CCBFlags(%x)\n", &pFCB->FCBName, pCCB, pCCB->CCBFlags);


			DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!CleanUp (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));


			IoRemoveShareAccess( pFileObject, &pFCB->FCBShareAccess );
			break;
			
		case UserFileOpen:
			//
			//  Coordinate the cleanup operation with the oplock state.
			//  Oplock cleanup operations can always cleanup immediately so no
			//  need to check for STATUS_PENDING.
			//

			FsRtlCheckOplock( &pFCB->FCBOplock,
							  pIrp,
							  pIrpContext,
							  NULL,
							  NULL );

			//
			//  Unlock all outstanding file locks.
			//

			if (pFCB->FCBFileLock != NULL) {

				FsRtlFastUnlockAll( pFCB->FCBFileLock,
									pFileObject,
									IoGetRequestorProcess( pIrp ),
									NULL );
			}



			//
			//  Check the fast io state.
			//

			XifsdLockFcb( pIrpContext, pFCB );
			pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible( pFCB );
			XifsdUnlockFcb( pIrpContext, pFCB );


			/*
			if((pFCB->HasLock != FCB_FILE_LOCK_HAS)
				&& (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE))
				&& XifsdCheckFlagBoolean(pFileObject->Flags, FO_CACHE_SUPPORTED) 
				&& (pFCB->FCBCleanup == 1)
			){
					if(pFCB->SectionObject.DataSectionObject != NULL) 
					{
						
						CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);

						ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
						ExReleaseResourceLite( pFCB->PagingIoResource );
						
						
						CcPurgeCacheSection( &(pFCB->SectionObject),
														NULL,
														0,
														FALSE 
														);	
														
					}			
			
			}
			*/


			
			if(
				XifsdCheckFlagBoolean(pFileObject->Flags, FO_CACHE_SUPPORTED) &&
				(pFCB->FcbNonCachedOpenCount > 1)	&&
				((pFCB->FcbNonCachedOpenCount + 1) ==  pFCB->FCBCleanup)
			)
			{
				if(pFCB->SectionObject.DataSectionObject != NULL) 
				{

					// changed by ILGU HONG for readonly 09052006
					if(!pFCB->PtrVCB->IsVolumeWriteProctected)
						CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);
					// changed by ILGU HONG for readonly end
					
					DbgPrint("CcFlush  1 File(%wZ)\n", &pFCB->FCBFullPath);
					ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
					ExReleaseResourceLite( pFCB->PagingIoResource );


					CcPurgeCacheSection( &(pFCB->SectionObject),
														NULL,
														0,
														FALSE 
														);					
					

				}	
			}
			
			/*
			else if(pFCB->FCBCleanup == 1 ){
		
				if(XifsdCheckFlagBoolean(pFileObject->Flags, FO_CACHE_SUPPORTED)){
					if(pFCB->SectionObject.DataSectionObject != NULL) 
					{
						CcFlushCache(&(pFCB->SectionObject), NULL, 0, NULL);

						ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
						ExReleaseResourceLite( pFCB->PagingIoResource );

						CcPurgeCacheSection( &(pFCB->SectionObject),
															NULL,
															0,
															FALSE 
															);					
						

					}
				}

				if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_MODIFIED_FILE)){
					XixFsdUpdateFCB(pFCB);
				}

			}
			*/

			IoRemoveShareAccess( pFileObject, &pFCB->FCBShareAccess );
			//
			//  Cleanup the cache map.
			//
			
			CcUninitializeCacheMap( pFileObject, NULL, NULL );
					
			

			break;
		case UserVolumeOpen:
			break;
		default:
			break;
		}

		


		if((TypeOfOpen == UserDirectoryOpen) || (TypeOfOpen == UserFileOpen)){

			if(pFCB->FCBCleanup == 1 ){
				if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_MODIFIED_FILE)){
					
					// changed by ILGU HONG for readonly 09052006
					if(!pFCB->PtrVCB->IsVolumeWriteProctected){

						XixFsdUpdateFCB(pFCB);

						if(pFCB->WriteStartOffset != -1){

							//DbgPrint("Set Update Information!!!\n");
							XixFsdSendFileChangeRC(
									TRUE,
									pVCB->HostMac, 
									pFCB->LotNumber, 
									pVCB->DiskId, 
									pVCB->PartitionId, 
									pFCB->FileSize.QuadPart, 
									pFCB->RealAllocationSize,
									pFCB->WriteStartOffset
							);
							
							pFCB->WriteStartOffset = -1;
						}
					}
					// changed by ILGU HONG for readonly end

				}


	


			}




			if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_FLAGS_DELETE_ON_CLOSE)){
				
				if(pFCB == pFCB->PtrVCB->RootDirFCB){
					XifsdClearFlag(pCCB->CCBFlags, XIFSD_CCB_FLAGS_DELETE_ON_CLOSE);
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				}else{
					XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				}
			}

	


			// changed by ILGU HONG for readonly 09082006
			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE) && (!pFCB->PtrVCB->IsVolumeWriteProctected) ){
			// changed by ILGU HONG for readonly end
				if(pFCB->FCBCleanup == 1){
					
					//DbgPrint(" !!!Delete Entry From table (%wZ)  .\n", &pFCB->FCBFullPath);

					ASSERT_CCB(pCCB);
					pLCB = pCCB->PtrLCB;

					ASSERT_LCB(pLCB);

					pParentFCB = pLCB->ParentFcb;
					ASSERT_FCB(pParentFCB);					



  					pFCB->FileSize.QuadPart = 0;
					pFCB->ValidDataLength.QuadPart = 0;

					if(pFCB->FCBType == FCB_TYPE_FILE){
						
						XifsdReleaseFcb(TRUE, pFCB);
						XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
						ParentFCBAcquired = TRUE;
						XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
						RC = DeleteParentChild(pIrpContext, pParentFCB, &pLCB->FileName);
						
						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail DeleteParentChild (%wZ)\n", &pLCB->FileName));
							
							//XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
							RC = STATUS_SUCCESS;
							goto pass_through;
						}
						
						XifsdClearFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
						XifsdSetFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_LINK_IS_GONE);
						
						XixFsdRemovePrefix(TRUE, pLCB);

						//
						//  Now Decrement the reference counts for the parent and drop the Vcb.
						//
						XifsdLockVcb(pIrpContext,pVCB);
						DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
									("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
									pParentFCB->LotNumber,
									 pVCB->VCBReference,
									 pVCB->VCBUserReference,
									 pParentFCB->FCBReference,
									 pParentFCB->FCBUserReference ));

						XifsdDecRefCount( pParentFCB, 1, 1 );

						DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
									("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
									pParentFCB->LotNumber,
									 pVCB->VCBReference,
									 pVCB->VCBUserReference,
									 pParentFCB->FCBReference,
									 pParentFCB->FCBUserReference ));

						XifsdUnlockVcb( pIrpContext, pVCB );
						


						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail DeleteParentChild (%wZ)\n", &pLCB->FileName));
							
							XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
							RC = STATUS_SUCCESS;
							goto pass_through;
						}
						
						

					}else {
						
						RC = XixFsReLoadFileFromFcb(pFCB);
						
						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail XixFsReLoadFileFromFcb (%wZ)\n", &pFCB->FCBName));
							
							XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
							RC = STATUS_SUCCESS;
							goto pass_through;
						}
						
						if(pFCB->ChildCount != 0){
							XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
							RC = STATUS_SUCCESS;
							goto pass_through;
						}

						XifsdReleaseFcb(TRUE, pFCB);
						XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
						ParentFCBAcquired = TRUE;
						
						XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);

						RC = DeleteParentChild(pIrpContext, pParentFCB, &pLCB->FileName);

						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail DeleteParentChild (%wZ)\n", &pLCB->FileName));
							
							//ifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
							RC = STATUS_SUCCESS;
							goto pass_through;
						}
						

						XifsdClearFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
						XifsdSetFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_LINK_IS_GONE);
						
						
						XixFsdRemovePrefix(TRUE, pLCB);

						//
						//  Now Decrement the reference counts for the parent and drop the Vcb.
						//
						XifsdLockVcb(pIrpContext,pVCB);
						DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
									("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
									pParentFCB->LotNumber,
									 pVCB->VCBReference,
									 pVCB->VCBUserReference,
									 pParentFCB->FCBReference,
									 pParentFCB->FCBUserReference ));

						XifsdDecRefCount( pParentFCB, 1, 1 );

						DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
									("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
									pParentFCB->LotNumber,
									 pVCB->VCBReference,
									 pVCB->VCBUserReference,
									 pParentFCB->FCBReference,
									 pParentFCB->FCBUserReference ));

						XifsdUnlockVcb( pIrpContext, pVCB );

						
						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Fail DeleteParentChild (%wZ)\n", &pLCB->FileName));
							
							XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
							RC = STATUS_SUCCESS;
							goto pass_through;
						}
						

						
					}


			
					//XixFsdDeleteUpdateFCB(pFCB);

					XixFsdSendRenameLinkBC(
						TRUE,
						XIFS_SUBTYPE_FILE_DEL,
						pVCB->HostMac,
						pFCB->LotNumber,
						pVCB->DiskId,
						pVCB->PartitionId,
						pFCB->ParentLotNumber,
						0
					);

				}
			}
			

		}


pass_through:

		XifsdLockVcb(pIrpContext, pVCB);
		
		if(XifsdCheckFlagBoolean(pFileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING))
		{
			ASSERT(pFCB->FcbNonCachedOpenCount > 0);
			pFCB->FcbNonCachedOpenCount --;
		}

		XifsdDecrementClenupCount(pFCB);


		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
			("Cleanup  Name(%wZ) FCBLotNumber(%I64d) FCBCleanUp(%ld) VCBCleanup(%ld) pCCB(%p) FileObject(%p)\n", 
			&pFCB->FCBName,
			pFCB->LotNumber, 
			pFCB->FCBCleanup, 
			pVCB->VCBCleanup,
			pCCB,
			pFileObject
			));
			
		XifsdUnlockVcb( pIrpContext, pVCB );

		AttemptTeardown = (pVCB->VCBCleanup == 0 && pVCB->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNTED );
		
		if(pFileObject == pVCB->LockVolumeFileObject){
			ASSERT(XifsdCheckFlagBoolean(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED));
			
			IoAcquireVpbSpinLock(&SavedIrql);
			XifsdClearFlag(pVCB->PtrVPB->Flags, VPB_LOCKED);
			IoReleaseVpbSpinLock( SavedIrql );
			
			XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED);
			pVCB->LockVolumeFileObject = NULL;
			SendUnlockNotification = TRUE;
			
		}

		

		/*
		if( (pFCB->FCBCleanup == 0) 
				&& (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)) ){

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLEANUP|DEBUG_TARGET_IRPCONTEXT| DEBUG_TARGET_ALL), 
				("CleanUp Release Lot Lock LotNumber(%ld)\n", pFCB->LotNumber));

			XifsdLotUnLock(pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber);
			pFCB->HasLock = FCB_FILE_LOCK_INVALID;
		}
		*/

		//
		//  We must clean up the share access at this time, since we may not
		//  get a Close call for awhile if the file was mapped through this
		//  File Object.
		//

		


	}finally{

		XifsdReleaseFcb(pIrpContext, pFCB);

		if(ParentFCBAcquired == TRUE) {
			XifsdReleaseFcb(TRUE,pParentFCB);
		}
		

        if (SendUnlockNotification) {

            FsRtlNotifyVolumeEvent( pFileObject, FSRTL_VOLUME_UNLOCK );
        }

  		if (VCBAcquired)  {

			XifsdReleaseVcb( pIrpContext, pVCB);
		}


	}



    if (AttemptTeardown) {
        XifsdAcquireVcbExclusive( CanWait, pVCB, FALSE );

        try {
            
            XixFsdPurgeVolume( pIrpContext, pVCB, FALSE );

        } finally {

            XifsdReleaseVcb( pIrpContext, pVCB );
        }
    }


    //
    //  If this is a normal termination then complete the request
    //

	DebugTrace((DEBUG_LEVEL_TRACE), (DEBUG_TARGET_CLEANUP|DEBUG_TARGET_IRPCONTEXT), 
		("Exit XifsdCommonCleanUp pIrpContext(%p)\n", pIrpContext));

    XixFsdCompleteRequest( pIrpContext, STATUS_SUCCESS, 0 );

    return STATUS_SUCCESS;
}