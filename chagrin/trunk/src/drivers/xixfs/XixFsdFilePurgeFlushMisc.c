#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdPurgeVolume)
#pragma alloc_text(PAGE, XixFsdCleanupFlushVCB)
#endif


NTSTATUS
XixFsdFlusVolume(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_VCB		pVCB
)
{
	PVOID RestartKey = NULL;
	PXIFS_FCB ThisFcb = NULL;
	PXIFS_FCB NextFcb = NULL;
	
	BOOLEAN RemovedFcb = FALSE;
	BOOLEAN	CanWait = FALSE;
	uint32	lockedVcbValue = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter  XixFsdFlusVolume\n"));

	ASSERT_EXCLUSIVE_VCB(pVCB);

	CanWait = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	XixFsdRealCloseFCB(pVCB);

	while (TRUE) {
		
		XifsdLockVcb( IrpContext, pVCB );
		NextFcb = XixFsdGetNextFcb(pVCB, &RestartKey );

		//
		//  Reference the NextFcb if present.
		//

		if (NextFcb != NULL) {

			NextFcb->FCBReference += 1;
		}

		//
		//  If the last Fcb is present then decrement reference count and call teardown
		//  to see if it should be removed.
		//

		if (ThisFcb != NULL) {

			ThisFcb->FCBReference -= 1;

			XifsdUnlockVcb( IrpContext, pVCB );

		} else {

			XifsdUnlockVcb( IrpContext, pVCB );
		}

		//
		//  Break out of the loop if no more Fcb's.
		//

		if (NextFcb == NULL) {

			break;
		}

		//
		//  Move to the next Fcb.
		//

		ThisFcb = NextFcb;

		//
		//  If there is a image section then see if that can be closed.
		//

		if(ThisFcb->FCBType == FCB_TYPE_FILE){
			
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("XifsdPurgeVolume FCB(%I64d) FCBName(%wZ)\n", 
						 ThisFcb->LotNumber, &ThisFcb->FCBName));
			
			XifsdAcquireFcbExclusive(TRUE, ThisFcb, FALSE);
			//ExAcquireResourceShared(ThisFcb->PagingIoResource, TRUE);


			if(XifsdCheckFlagBoolean(ThisFcb->FCBFlags, XIFSD_FCB_CACHED_FILE)){

				// Added by ILGU HONG for readonly 09052006
				if(!ThisFcb->PtrVCB->IsVolumeWriteProctected){
					CcFlushCache(&ThisFcb->SectionObject, NULL, 0, NULL);
				}
				// Added by ILGU HONG for readonly end
				
			}
			//ExReleaseResource(ThisFcb->PagingIoResource);
			XifsdReleaseFcb(TRUE, ThisFcb);
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit  XixFsdFlusVolume\n"));

	return STATUS_SUCCESS;
}



NTSTATUS
XixFsdPurgeVolume(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_VCB		pVCB,
	IN BOOLEAN			DismountForce
)
{
	NTSTATUS Status = STATUS_SUCCESS;

	PVOID RestartKey = NULL;
	PXIFS_FCB ThisFcb = NULL;
	PXIFS_FCB NextFcb = NULL;
	
	BOOLEAN RemovedFcb = FALSE;
	BOOLEAN	CanWait = FALSE;
	uint32	lockedVcbValue = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter  XixFsdPurgeVolume\n"));

	ASSERT_EXCLUSIVE_VCB(pVCB);

	CanWait = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);



	
	
	//
	//  Force any remaining Fcb's in the delayed close queue to be closed.
	//
	
	XixFsdRealCloseFCB(pVCB);

	//
	//  Acquire the global file resource.
	//

	//XifsdAcquireAllFiles( CanWait, pVCB );

	//
	//  Loop through each Fcb in the Fcb Table and perform the flush.
	//

	while (TRUE) {

		//
		//  Lock the Vcb to lookup the next Fcb.
		//

		XifsdLockVcb( IrpContext, pVCB );
		NextFcb = XixFsdGetNextFcb(pVCB, &RestartKey );

		//
		//  Reference the NextFcb if present.
		//

		if (NextFcb != NULL) {

			NextFcb->FCBReference += 1;
		}

		//
		//  If the last Fcb is present then decrement reference count and call teardown
		//  to see if it should be removed.
		//

		if (ThisFcb != NULL) {

			ThisFcb->FCBReference -= 1;

			XifsdUnlockVcb( IrpContext, pVCB );


			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_PNP|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("XifsdPurgeVolume FCB(%I64d) FCBName(%wZ), VCB %d/%d FCB %d/%d\n",
					 ThisFcb->LotNumber,
					 &ThisFcb->FCBName,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 ThisFcb->FCBReference,
					 ThisFcb->FCBUserReference ));


			DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_PNP|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB|DEBUG_TARGET_REFCOUNT,
				("XifsdPurgeVolume FCBLotNumber(%I64d) FCBCleanUp(%ld) VCBCleanup(%ld)\n", 
				ThisFcb->LotNumber, 
				ThisFcb->FCBCleanup, 
				pVCB->VCBCleanup));
			

			XixFsdTeardownStructures( CanWait, ThisFcb, FALSE, &RemovedFcb );

		} else {

			XifsdUnlockVcb( IrpContext, pVCB );
		}

		//
		//  Break out of the loop if no more Fcb's.
		//

		if (NextFcb == NULL) {

			break;
		}

		//
		//  Move to the next Fcb.
		//

		ThisFcb = NextFcb;

		//
		//  If there is a image section then see if that can be closed.
		//

		if(ThisFcb->FCBType == FCB_TYPE_FILE){
			
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("XifsdPurgeVolume FCB(%I64d) FCBName(%wZ)\n", 
						 ThisFcb->LotNumber, &ThisFcb->FCBName));
			
			XifsdAcquireFcbExclusive(TRUE, ThisFcb, FALSE);

			
			//	Added by ILGU HONG for readonly 09052006
			if(!ThisFcb->PtrVCB->IsVolumeWriteProctected){
				if (ThisFcb->SectionObject.ImageSectionObject != NULL) {

					MmFlushImageSection( &ThisFcb->SectionObject, MmFlushForWrite );
				}

				//
				//  If there is a data section then purge this.  If there is an image
				//  section then we won't be able to.  Remember this if it is our first
				//  error.
				//

				
				CcFlushCache(&ThisFcb->SectionObject, NULL, 0, NULL);
				//DbgPrint("CcFlush  3 File(%wZ)\n", &ThisFcb->FCBFullPath);
			}
			//	Added by ILGU HONG for readonly end


			
			ExAcquireResourceSharedLite(ThisFcb->PagingIoResource, TRUE);
			ExReleaseResourceLite( ThisFcb->PagingIoResource );

			
			if ((ThisFcb->SectionObject.DataSectionObject != NULL) &&
				!CcPurgeCacheSection( &ThisFcb->SectionObject,
									   NULL,
									   0,
									   FALSE ) &&
				(Status == STATUS_SUCCESS)) {

				Status = STATUS_UNABLE_TO_DELETE_SECTION;
			}
			
			

			XifsdReleaseFcb(TRUE, ThisFcb);
		}
		//
		//  Dereference the internal stream if dismounting.
		//

		if (DismountForce &&
			(ThisFcb->FCBType  != FCB_TYPE_FILE) &&
			(ThisFcb->FCBType != FCB_TYPE_DIR)	&&
			(ThisFcb->FileObject != NULL)) {

			XixFsdDeleteInternalStream( CanWait, ThisFcb );
		}
	}

	//
	//  Now look at the Root Index, Metadata, Volume Dasd and VAT Fcbs.
	//  Note that we usually hit the Root Index in the loop above, but
	//  it is possible miss it if it didn't get into the Fcb table in the
	//  first place!
	//


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("XixFsdPurgeVolume DismountFore (%s)\n", ((DismountForce)?"TRUE":"FALSE")));


	if (DismountForce) {




		if (pVCB->RootDirFCB != NULL) {
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
				("Deal with RootFCB \n"));


			ThisFcb = pVCB->RootDirFCB;
			InterlockedIncrement( &ThisFcb->FCBReference );

			if ((ThisFcb->SectionObject.DataSectionObject != NULL) &&
				!CcPurgeCacheSection( &ThisFcb->SectionObject,
									   NULL,
									   0,
									   FALSE ) &&
				(Status == STATUS_SUCCESS)) {

				Status = STATUS_UNABLE_TO_DELETE_SECTION;
			}


			InterlockedDecrement( &ThisFcb->FCBReference );
/*
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						 ("pVCB->RootDirFCB FCB(%I64d) FCBName(%wZ), VCB %d/%d FCB %d/%d\n",
						 ThisFcb->LotNumber,
						 &ThisFcb->FCBName,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ThisFcb->FCBReference,
						 ThisFcb->FCBUserReference ));
			
			XifsdLockVcb(IrpContext, pVCB);
			XifsdDecRefCount(ThisFcb, 1, 1);
			XifsdUnlockVcb(IrpContext, pVCB);
*/	
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("pVCB->RootDirFCB FCB(%I64d) FCBName(%wZ), VCB %d/%d FCB %d/%d\n",
						 ThisFcb->LotNumber,
						 &ThisFcb->FCBName,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ThisFcb->FCBReference,
						 ThisFcb->FCBUserReference ));

			XixFsdTeardownStructures( CanWait, ThisFcb, FALSE, &RemovedFcb );
			
		}
    
		if (pVCB->MetaFCB != NULL) {

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
				("Deal with pVCB->MetaFCB \n"));

			ThisFcb = pVCB->MetaFCB;
			InterlockedIncrement( &ThisFcb->FCBReference );

			if ((ThisFcb->SectionObject.DataSectionObject != NULL) &&
				!CcPurgeCacheSection( &ThisFcb->SectionObject,
									   NULL,
									   0,
									   FALSE ) &&
				(Status == STATUS_SUCCESS)) {

				Status = STATUS_UNABLE_TO_DELETE_SECTION;
			}

			
			XixFsdDeleteInternalStream( CanWait, ThisFcb );

			InterlockedDecrement( &ThisFcb->FCBReference );

/*			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("pVCB->MetaFCB FCB(%I64d)  VCB %d/%d FCB %d/%d\n",
						 ThisFcb->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ThisFcb->FCBReference,
						 ThisFcb->FCBUserReference ));

			XifsdLockVcb(IrpContext, pVCB);
			XifsdDecRefCount(ThisFcb, 1, 1);
			XifsdUnlockVcb(IrpContext, pVCB);
*/
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("pVCB->MetaFCB FCB(%I64d)  VCB %d/%d FCB %d/%d\n",
						 ThisFcb->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ThisFcb->FCBReference,
						 ThisFcb->FCBUserReference ));



			XixFsdTeardownStructures( CanWait, ThisFcb, FALSE, &RemovedFcb );
			
			
			
		}

		if (pVCB->VolumeDasdFCB != NULL) {
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
				("Deal with pVCB->VolumeDasdFCB \n"));

			ThisFcb = pVCB->VolumeDasdFCB;
			InterlockedIncrement( &ThisFcb->FCBReference );

			if ((ThisFcb->SectionObject.DataSectionObject != NULL) &&
				!CcPurgeCacheSection( &ThisFcb->SectionObject,
									   NULL,
									   0,
									   FALSE ) &&
				(Status == STATUS_SUCCESS)) {

				Status = STATUS_UNABLE_TO_DELETE_SECTION;
			}
			
			XixFsdDeleteInternalStream( CanWait, ThisFcb );

			InterlockedDecrement( &ThisFcb->FCBReference );
/*
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("pVCB->VolumeDasdFCB FCB(%I64d)  VCB %d/%d FCB %d/%d\n",
						 ThisFcb->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ThisFcb->FCBReference,
						 ThisFcb->FCBUserReference ));

			
			XifsdLockVcb(IrpContext, pVCB);
			XifsdDecRefCount(ThisFcb, 1, 1);
			XifsdUnlockVcb(IrpContext, pVCB);
*/			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						 ("pVCB->VolumeDasdFCB FCB(%I64d)  VCB %d/%d FCB %d/%d\n",
						 ThisFcb->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ThisFcb->FCBReference,
						 ThisFcb->FCBUserReference ));	
			

			
			XixFsdTeardownStructures( CanWait, ThisFcb, FALSE, &RemovedFcb );
			
		}
	}

	//
	//  Release all of the files.
	//

	//XifsdReleaseAllFiles( CanWait, pVCB );
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit  XixFsdPurgeVolume\n"));
	return Status;
}



VOID
XixFsdCleanupFlushVCB(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB		pVCB,
	IN BOOLEAN			DisMountVCB
)
{

	

	//PAGED_CODE();

	ASSERT_IRPCONTEXT( pIrpContext );
	ASSERT_VCB( pVCB );

	ASSERT_EXCLUSIVE_XIFS_GDATA;
	ASSERT_EXCLUSIVE_VCB(pVCB);

	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
			("XixFsdCleanupFlushVCB Status(%ld).\n", pVCB->VCBState));




	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT), 
						 ("11 Current VCB->PtrVPB->ReferenceCount %d \n", pVCB->PtrVPB->ReferenceCount));


	XifsdLockVcb( pIrpContext, pVCB );

	XifsdSetFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);

	if(DisMountVCB){
		if(pVCB->VolumeDasdFCB != NULL){
			pVCB->VolumeDasdFCB->FCBReference -= 1;
			pVCB->VolumeDasdFCB->FCBUserReference -= 1;
		}

		
		if(pVCB->MetaFCB != NULL){
			pVCB->MetaFCB->FCBReference -=1;
			pVCB->MetaFCB->FCBUserReference -= 1;
		}


		if(pVCB->RootDirFCB != NULL){
			pVCB->RootDirFCB->FCBReference -=1;
			pVCB->RootDirFCB->FCBUserReference -= 1;
		}
	}

	XifsdUnlockVcb(pIrpContext, pVCB);

	XixFsdPurgeVolume(pIrpContext, pVCB, DisMountVCB);

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_PNP| DEBUG_TARGET_VCB| DEBUG_TARGET_IRPCONTEXT),
						 ("22 Current VCB->PtrVPB->ReferenceCount %d \n", pVCB->PtrVPB->ReferenceCount));


	if(DisMountVCB){
		// Added by ILGU HONG
		XifsdReleaseVcb(TRUE, pVCB);
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
					 ("VCB %d/%d \n", pVCB->VCBReference, pVCB->VCBUserReference));		


		CcWaitForCurrentLazyWriterActivity();

		XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
			
		XixFsdRealCloseFCB((PVOID)pVCB);
	}

	XifsdLockVcb( pIrpContext, pVCB );
	XifsdClearFlag(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE);
	XifsdUnlockVcb(pIrpContext, pVCB);


	// Added by ILGU HONG END
	if(DisMountVCB){

		// changed by ILGU HONG for readonly 09052006
		if(!pVCB->IsVolumeWriteProctected){

			LARGE_INTEGER	TimeOut;
			
			//
			//	Stop Meta Update process
			//

			KeSetEvent(&pVCB->VCBUmountEvent, 0, FALSE);

			TimeOut.QuadPart = - DEFAULT_XIFS_UMOUNTWAIT;
			KeWaitForSingleObject(&pVCB->VCBStopOkEvent, Executive, KernelMode, FALSE, &TimeOut);		
			


			XixFsdDeRegisterHost(pIrpContext, pVCB);
			
			//	Added by ILGU HONG for 08312006
			if(pVCB->NdasVolBacl_Id){
				XixFsRemoveUserBacl(pVCB->TargetDeviceObject, pVCB->NdasVolBacl_Id);
			}
			
			//	Added by ILGU HONG End	
		}
		// changed by ILGU HONG for readonly end
	}

	return;
}
