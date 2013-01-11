#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"

#include "xixcore/callback.h"
#include "xixcore/ondisk.h"
#include "xixcore/fileaddr.h"
#include "xixcore/lotaddr.h"

#define XIFS_DEFAULT_IO_RUN_COUNT			(8)

typedef struct _IO_RUN {
    LONGLONG	DiskOffset;
    ULONG		DiskByteCount;
    PVOID		UserBuffer;
    PVOID		TransferBuffer;
    ULONG		TransferByteCount;
    ULONG		TransferBufferOffset;
    PMDL 		TransferMdl;
    PVOID		TransferVirtualAddress;
    PIRP		SavedIrp;
} IO_RUN;
typedef IO_RUN *PIO_RUN;


NTSTATUS
xixfs_VolumePrepareBuffers (
    IN PXIXFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIXFS_FCB 			FCB,
    IN uint64 				ByteOffset,
    IN uint32 				ByteCount,
	IN uint32				PreviousProcessByteCount,
    IN PVOID				Buffer,
    IN uint32				LotSize,
    IN uint32				NumOfIoRun,
    IN PIO_RUN 				IoRun,
    OUT uint32				*pNumberRuns,
    OUT BOOLEAN				*bUnAligned,
    OUT uint32				*ProcessedByteCount,
	BOOLEAN					bReadIo,
	uint32					OrgBuffLenth
    );


NTSTATUS
xixfs_FilePrepareBuffers(
    IN PXIXFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIXFS_FCB 			FCB,
    IN uint64 				ByteOffset,
    IN uint32 				ByteCount,
	IN uint32				PreviousProcessByteCount,
    IN PVOID				Buffer,
    IN uint32				LotSize,
    IN uint32				NumOfIoRun,
    IN PIO_RUN 				IoRun,
    OUT uint32				*pNumberRuns,
    OUT BOOLEAN				*bUnAligned,
    OUT uint32				*ProcessedByteCount,
	BOOLEAN					bReadIo,
	uint32					OrgBuffLenth
    );

NTSTATUS
xixfs_IOPrepareBuffers (
    IN PXIXFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIXFS_FCB 			FCB,
	IN TYPE_OF_OPEN			TypeOfOpen,
    IN uint64 				ByteOffset,
    IN uint32 				ByteCount,
	IN uint32				PreviousProcessByteCount,
    IN PVOID				Buffer,
    IN uint32				LotSize,
    IN uint32				NumOfIoRun,
    IN PIO_RUN 				IoRun,
    OUT uint32				*pNumberRuns,
    OUT BOOLEAN				*bUnAligned,
    OUT uint32				*ProcessedByteCount,
	BOOLEAN					bReadIo,
	uint32					OrgBuffLenth
    );


NTSTATUS
xixfs_CheckFCBAccess(
	IN PXIXFS_FCB	pFCB,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN uint32	RequestDeposition,
	IN ACCESS_MASK	DesiredAccess
);





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_FileFinishIoEof)
#pragma alloc_text(PAGE, xixfs_FileCheckEofWrite)
#pragma alloc_text(PAGE, xixfs_CheckAndVerifyVolume)
#pragma alloc_text(PAGE, xixfs_VerifyFCBOperation)
#pragma alloc_text(PAGE, xixfs_FileNonCachedIo)
#pragma alloc_text(PAGE, xixfs_ProcessFileNonCachedIo)
#pragma alloc_text(PAGE, xixfs_VolumePrepareBuffers)
#pragma alloc_text(PAGE, xixfs_IOPrepareBuffers)
#pragma alloc_text(PAGE, xixfs_FilePrepareBuffers)
#pragma alloc_text(PAGE, xixfs_CheckFastIoPossible)
#pragma alloc_text(PAGE, xixfs_CheckFCBAccess)
#pragma alloc_text(PAGE, xixfs_OpenExistingFCB)
#pragma alloc_text(PAGE, xixfs_OpenByFileId)
#pragma alloc_text(PAGE, xixfs_OpenObjectFromDirContext) 
#pragma alloc_text(PAGE, xixfs_OpenNewFileObject)
#pragma alloc_text(PAGE, xixfs_InitializeFileContext)
#pragma alloc_text(PAGE, xixfs_SetFileContext)
#pragma alloc_text(PAGE, xixfs_ClearFileContext)
#pragma alloc_text(PAGE, xixfs_FileInfoFromContext)
#pragma alloc_text(PAGE, xixfs_ReadFileInfoFromFcb)
#pragma alloc_text(PAGE, xixfs_WriteFileInfoFromFcb)
#pragma alloc_text(PAGE, xixfs_ReLoadFileFromFcb)
#pragma alloc_text(PAGE, xixfs_InitializeFCBInfo)
#endif



VOID
xixfs_FileFinishIoEof(
	IN PXIXFS_FCB pFCB
)
{
	PEOF_WAIT_CTX	pEofWaitCtx;
	PLIST_ENTRY		pEntry;
	PAGED_CODE();

	if(!IsListEmpty(&(pFCB->EndOfFileLink)) )
	{
		pEntry = RemoveHeadList(&(pFCB->EndOfFileLink));
		pEofWaitCtx = (PEOF_WAIT_CTX)CONTAINING_RECORD(pEntry, EOF_WAIT_CTX, EofWaitLink);
		KeSetEvent(&(pEofWaitCtx->EofEvent), 0, FALSE);
	}else{
		XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
	}

}



BOOLEAN
xixfs_FileCheckEofWrite(
	IN PXIXFS_FCB		pFCB,
	IN PLARGE_INTEGER	FileOffset,
	IN ULONG			Length
)
{
	EOF_WAIT_CTX EofWaitCtx;
	PAGED_CODE();
	ASSERT_FCB(pFCB);
	
	ASSERT(pFCB->FileSize.QuadPart >= pFCB->ValidDataLength.QuadPart);

	InitializeListHead(&(EofWaitCtx.EofWaitLink));
	KeInitializeEvent(&(EofWaitCtx.EofEvent), NotificationEvent, FALSE);

	InsertTailList(&(pFCB->EndOfFileLink), &(EofWaitCtx.EofWaitLink));
	
	XifsdUnlockFcb(NULL, pFCB);

	KeWaitForSingleObject( &(EofWaitCtx.EofEvent), Executive, KernelMode, FALSE, NULL);

	XifsdLockFcb(NULL, pFCB);

	if((FileOffset->QuadPart >= 0) &&
		((FileOffset->QuadPart + Length) <= pFCB->ValidDataLength.QuadPart))
	{
		xixfs_FileFinishIoEof(pFCB);

		return FALSE;
	}

	return TRUE;
}




NTSTATUS
xixfs_CheckAndVerifyVolume(
	IN PXIXFS_VCB	pVCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	ASSERT(pVCB);

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XifsdCheckVerifyVolume .\n"));
	
	XifsdLockVcb(NULL, pVCB);
	
	if(pVCB->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED)
	{
		RC = STATUS_UNSUCCESSFUL;

	}else if (XIXCORE_TEST_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED))
	{
		
		RC = STATUS_ACCESS_DENIED;
	}else if(XIXCORE_TEST_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP)){
		
		RC = STATUS_UNSUCCESSFUL;
	}
	
	XifsdUnlockVcb(NULL, pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status (0x%x) XifsdCheckVerifyVolume .\n", RC));
	
	return RC;
}


BOOLEAN
xixfs_VerifyFCBOperation (
	IN PXIXFS_IRPCONTEXT IrpContext OPTIONAL,
	IN PXIXFS_FCB Fcb
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PXIXFS_VCB Vcb = Fcb->PtrVCB;
	PDEVICE_OBJECT RealDevice = Vcb->PtrVPB->RealDevice;
	PIRP Irp;

    PAGED_CODE();
    DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_VerifyFCBOperation \n"));
    //
    //  Check that the fileobject has not been cleaned up.
    //
    
	if ( ARGUMENT_PRESENT( IrpContext ))  {

		PFILE_OBJECT FileObject;

		Irp = IrpContext->Irp;
		FileObject = IoGetCurrentIrpStackLocation( Irp)->FileObject;

		if ( FileObject && XIXCORE_TEST_FLAGS( FileObject->Flags, FO_CLEANUP_COMPLETE))  {

			PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

			//
			//  Following FAT,  we allow certain operations even on cleaned up
			//  file objects.  Everything else,  we fail.
			//

			if ( (FlagOn(Irp->Flags, IRP_PAGING_IO)) ||
				 (IrpSp->MajorFunction == IRP_MJ_CLOSE ) ||
				 (IrpSp->MajorFunction == IRP_MJ_QUERY_INFORMATION) ||
				 ( (IrpSp->MajorFunction == IRP_MJ_READ) &&
				   FlagOn(IrpSp->MinorFunction, IRP_MN_COMPLETE) ) ) {

				NOTHING;

			} else {

				//XifsdRaiseStatus( IrpContext, STATUS_FILE_CLOSED );
				return FALSE;
			}
		}
	}

    //
    //  Fail immediately if the volume is in the progress of being dismounted
    //  or has been marked invalid.
    //

    if ((Vcb->VCBState == XIFSD_VCB_STATE_VOLUME_INVALID) ||
        (Vcb->VCBState == XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS)) {

        if (ARGUMENT_PRESENT( IrpContext )) {

            //XifsdRaiseStatus( IrpContext, STATUS_FILE_INVALID );
        }

        return FALSE;
    }

    //
    //  Always fail if the volume needs to be verified.
    //

    if (XIXCORE_TEST_FLAGS( RealDevice->Flags, DO_VERIFY_VOLUME )) {

        if (ARGUMENT_PRESENT( IrpContext )) {

            //IoSetHardErrorOrVerifyDevice( IrpContext->Irp,
           //                               RealDevice );

            //XifsdRaiseStatus( IrpContext, STATUS_VERIFY_REQUIRED );
        }

        return FALSE;

    //
    //  All operations are allowed on mounted volumes.
    //

    } else if (Vcb->VCBState == XIFSD_VCB_STATE_VOLUME_MOUNTED) {

        return TRUE;

    //
    //  Fail all requests for fast Io on other Vcb conditions.
    //

    } else if (!ARGUMENT_PRESENT( IrpContext )) {

        return FALSE;

    //
    //  The remaining case is VcbNotMounted.
    //  Mark the device to be verified and raise WRONG_VOLUME.
    //

    } else if (Vcb->VCBState != XIFSD_VCB_STATE_VOLUME_MOUNTED) {

        if (ARGUMENT_PRESENT( IrpContext )) {

            XIXCORE_SET_FLAGS(RealDevice->Flags, DO_VERIFY_VOLUME);

            //IoSetHardErrorOrVerifyDevice( IrpContext->Irp, RealDevice );
            //XifsdRaiseStatus( IrpContext, STATUS_WRONG_VOLUME );
        }

        return FALSE;
    }
    DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_VerifyFCBOperation \n"));
    return TRUE;
}



NTSTATUS
xixfs_VolumePrepareBuffers (
    IN PXIXFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIXFS_FCB 			FCB,
    IN uint64 				ByteOffset,
    IN uint32 				ByteCount,
	IN uint32				PreviousProcessByteCount,
    IN PVOID				Buffer,
    IN uint32				LotSize,
    IN uint32				NumOfIoRun,
    IN PIO_RUN 				IoRun,
    OUT uint32				*pNumberRuns,
    OUT BOOLEAN				*bUnAligned,
    OUT uint32				*ProcessedByteCount,
	BOOLEAN					bReadIo,
	uint32					OrgBuffLenth
    )
{
	NTSTATUS 			RC = STATUS_UNSUCCESSFUL;
	PXIXFS_VCB			VCB = NULL;
	uint64				Margin = 0;
	uint64				PhyStartByteOffset = 0;

	uint32				NewByteCount = 0;
	uint32				OrgTransferSize = 0;
	uint32				ChangedOrgTransferSize = 0;
	uint32				TransferedByteCount = 0;
	uint32				RemainByteCount = ByteCount;
	
	PVOID				CurrentUserBuffer = Buffer;
	uint32				CurrentUserBufferOffset = 0;
	
	uint32				Offset = 0;
	uint32				NumSector = 0;
	
	uint32				IoRunIndex = 0;
	uint32				TruncateSize = 0;
	uint32				tempTranferSize = 0;
	uint32				Reason;
	uint32				i = 0;
	BOOLEAN				Waitable = FALSE;
	BOOLEAN				bUnalign = FALSE;
	uint32				RemainOrgByteCount = OrgBuffLenth;
	
	ASSERT_FCB(FCB);
	VCB = FCB->PtrVCB;
	ASSERT_VCB(VCB);

	

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_VolumePrepareBuffers\n"));



	*bUnAligned = FALSE;

	Waitable = XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	RtlZeroMemory(IoRun, sizeof(IO_RUN)*NumOfIoRun);
	while((RemainByteCount > 0) && (IoRunIndex < NumOfIoRun)) 
	{		
		Margin = 0;
		TruncateSize = 0;
		Offset = 0;
		NewByteCount= 0;
		OrgTransferSize = 0;
		ChangedOrgTransferSize = 0;
		bUnalign = FALSE;

		PhyStartByteOffset = ByteOffset;

		/*
			 added bytes in head
		*/

		Offset = ((ULONG)PhyStartByteOffset & (VCB->XixcoreVcb.SectorSize -1));
		PhyStartByteOffset = SECTOR_ALIGNED_ADDR(VCB->XixcoreVcb.SectorSizeBit, ByteOffset);
		NewByteCount = (uint32)SECTOR_ALIGNED_SIZE(VCB->XixcoreVcb.SectorSizeBit, RemainByteCount) ;
		

		
		if(Waitable){
			if(NewByteCount > XIFSD_MAX_BUFF_LIMIT){
				NewByteCount = XIFSD_MAX_BUFF_LIMIT;
			}


		}else{
			// Changed by ILGU HONG
			//if(Offset || (TruncateSize && (RemainOrgByteCount < NewByteCount))){
			if(Offset || (RemainOrgByteCount < NewByteCount)){
			// Changed by ILGU HONG END
				if(NewByteCount > XIFSD_MAX_BUFF_LIMIT){
					NewByteCount = XIFSD_MAX_BUFF_LIMIT;
				}
			}
		}


		

		if(ByteOffset + RemainByteCount >= PhyStartByteOffset + NewByteCount){
			TruncateSize = 0;
		}else{
			TruncateSize = (uint32)((PhyStartByteOffset + NewByteCount) - (ByteOffset + RemainByteCount));
		}

		OrgTransferSize = NewByteCount - Offset - TruncateSize;
		ASSERT(OrgTransferSize > 0);
	
		// Added by ILGU HONG
		ChangedOrgTransferSize = SECTOR_ALIGNED_SIZE(VCB->XixcoreVcb.SectorSizeBit, OrgTransferSize);
		if((ChangedOrgTransferSize != OrgTransferSize) && (ChangedOrgTransferSize > RemainByteCount)){
			bUnalign = TRUE;
		}
		// Added by ILGU HONG END


		/*
			added bytes in tail
		*/


		IoRun[IoRunIndex].DiskOffset = PhyStartByteOffset;
		//IoRun[IoRunIndex].DiskByteCount = NewByteCount;
		IoRun[IoRunIndex].UserBuffer = CurrentUserBuffer;

		// Changed by ILGU HONG
		//if(Offset || (TruncateSize && (RemainOrgByteCount < NewByteCount))){
		if(Offset || bUnalign){
			if(Waitable == FALSE) {
				DbgPrint("Can process!!\n");
				RC = STATUS_ACCESS_DENIED;
				goto error_out;
			}

			IoRun[IoRunIndex].DiskByteCount = NewByteCount;
		// Changed by ILGU HONG END
			IoRun[IoRunIndex].TransferBuffer = ExAllocatePoolWithTag(NonPagedPool, NewByteCount, TAG_BUFFER);	
			if(!IoRun[IoRunIndex].TransferBuffer){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				goto error_out;
			}

			if(bReadIo){
				RtlZeroMemory(IoRun[IoRunIndex].TransferBuffer, NewByteCount);
			}else{
				ASSERT(Offset == 0);
				RtlZeroMemory(IoRun[IoRunIndex].TransferBuffer, NewByteCount);
				RtlCopyMemory(IoRun[IoRunIndex].TransferBuffer, CurrentUserBuffer, (NewByteCount - TruncateSize));
			}	

			IoRun[IoRunIndex].TransferMdl = IoAllocateMdl(IoRun[IoRunIndex].TransferBuffer, IoRun[IoRunIndex].DiskByteCount, FALSE, FALSE, NULL);
			if(!IoRun[IoRunIndex].TransferMdl){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				goto error_out;
			}

			IoRun[IoRunIndex].TransferVirtualAddress = IoRun[IoRunIndex].TransferBuffer;
			MmBuildMdlForNonPagedPool(IoRun[IoRunIndex].TransferMdl);
			*bUnAligned = TRUE;
		}else{
		// Added by ILGU HONG
			IoRun[IoRunIndex].DiskByteCount = ChangedOrgTransferSize; 
		// Added by ILGU HONG END
			IoRun[IoRunIndex].TransferBuffer = CurrentUserBuffer;
			IoRun[IoRunIndex].TransferMdl = Irp->MdlAddress;
			IoRun[IoRunIndex].TransferVirtualAddress = Add2Ptr(Irp->UserBuffer ,(PreviousProcessByteCount + CurrentUserBufferOffset));
		}

		IoRun[IoRunIndex].TransferByteCount = OrgTransferSize ;
		IoRun[IoRunIndex].TransferBufferOffset = Offset;


		CurrentUserBufferOffset += OrgTransferSize;
		CurrentUserBuffer = (UCHAR *)CurrentUserBuffer + OrgTransferSize; //add2Ptr(Irp->UserBuffer, CurrentUserBufferOffset);

		RemainByteCount -= OrgTransferSize;
		RemainOrgByteCount -= OrgTransferSize;
		ByteOffset += OrgTransferSize;
		TransferedByteCount += OrgTransferSize;
		IoRunIndex++;		
	}

	*pNumberRuns = IoRunIndex;
	*ProcessedByteCount = TransferedByteCount ;
	return STATUS_SUCCESS;
	
error_out:
	if(IoRun){
		for(i = 0; i<IoRunIndex; i++){
			if(IoRun[i].TransferMdl != IrpContext->Irp->MdlAddress){
			            if (IoRun[i].TransferMdl != NULL) {
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free MDL!!!\n"));
			                IoFreeMdl( IoRun[i].TransferMdl );
			            }

			            //
			            //  Now free any buffer we may have allocated.  If the Mdl
			            //  doesn't match the original Mdl then free the buffer.
			            //

			            if (IoRun[i].TransferBuffer != NULL) {

			                ExFreePool( IoRun[i].TransferBuffer );
			            }						
			}
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_VolumePrepareBuffers \n"));
	return RC;
}



NTSTATUS
xixfs_FilePrepareBuffers(
    IN PXIXFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIXFS_FCB 			FCB,
    IN uint64 				ByteOffset,
    IN uint32 				ByteCount,
	IN uint32				PreviousProcessByteCount,
    IN PVOID				Buffer,
    IN uint32				LotSize,
    IN uint32				NumOfIoRun,
    IN PIO_RUN 				IoRun,
    OUT uint32				*pNumberRuns,
    OUT BOOLEAN				*bUnAligned,
    OUT uint32				*ProcessedByteCount,
	BOOLEAN					bReadIo,
	uint32					OrgBuffLenth
    )
{
	NTSTATUS 			RC = STATUS_UNSUCCESSFUL;
	
	XIXCORE_IO_LOT_INFO	AddressInfo;
	PXIXFS_VCB			VCB = NULL;
	uint64				Margin = 0;
	uint64				PhyStartByteOffset = 0;
	uint64				NewPhyStartByteOffset = 0;
	uint32				NewByteCount = 0;
	uint32				TransferedByteCount = 0;
	uint32				OrgTransferSize = 0;
	uint32				ChangedOrgTransferSize = 0;
	uint32				RemainByteCount = ByteCount;
	
	PVOID				CurrentUserBuffer = Buffer;
	uint32				CurrentUserBufferOffset = 0;
	
	uint32				Offset = 0;
	uint32				NumSector = 0;
	
	uint32				IoRunIndex = 0;
	uint32				TruncateSize = 0;
	
	uint32				Reason;
	uint32				i = 0;
	uint32				Waitable = 0;
	BOOLEAN				bUnalign = FALSE;
	uint32				RemainOrgByteCount = OrgBuffLenth;


	ASSERT_FCB(FCB);
	VCB = FCB->PtrVCB;
	ASSERT_VCB(VCB);


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB),
		("Enter xixfs_FilePrepareBuffers\n"));	

	//DbgPrint("xixfs_FilePrepareBuffers Request ByteOffset (%I64d) : File AllocationSize (%I64d)\n",ByteOffset, FCB->RealAllocationSize); 



	Waitable = XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);



	*bUnAligned = FALSE;
	
	RtlZeroMemory(IoRun, sizeof(IO_RUN)*NumOfIoRun);
	RtlZeroMemory(&AddressInfo, sizeof(XIXCORE_IO_LOT_INFO));
	while((RemainByteCount > 0) && (IoRunIndex < NumOfIoRun)) 
	{		
		Margin = 0;
		TruncateSize = 0;
		Offset = 0;
		NewByteCount= 0;
		OrgTransferSize = 0;
		ChangedOrgTransferSize = 0;
		bUnalign = FALSE;




		if(ByteOffset >= (xc_uint64)FCB->FileSize.QuadPart) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Error!! Offset(%lld) > pFCB->RealAllocationSize(%lld)\n", 
					Offset, FCB->FileSize.QuadPart));

			RC = STATUS_ACCESS_DENIED;
			goto error_out;
		}


		RC = xixcore_GetAddressInfoForOffset(
								&VCB->XixcoreVcb,
								&FCB->XixcoreFcb,
								ByteOffset,
								FCB->XixcoreFcb.AddrLot,
								FCB->XixcoreFcb.AddrLotSize,
								&FCB->XixcoreFcb.AddrStartSecIndex,
								&Reason, 
								&AddressInfo,
								Waitable
								);

		if(!NT_SUCCESS(RC)){
			if(Reason != 0){
				// print reason
			}
			RC = STATUS_ACCESS_DENIED;
			goto error_out;
			//Raise exception
		}
		

		Margin = ByteOffset - AddressInfo.LogicalStartOffset;
		PhyStartByteOffset = AddressInfo.PhysicalAddress + Margin;

		/*
			 added bytes in head
		*/
		
		Offset = ((ULONG)PhyStartByteOffset & (VCB->XixcoreVcb.SectorSize -1));
		NewPhyStartByteOffset = SECTOR_ALIGNED_ADDR(VCB->XixcoreVcb.SectorSizeBit, PhyStartByteOffset);	
		
		if(ByteOffset + RemainByteCount  >  AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)
		{
				NewByteCount = (uint32)(AddressInfo.LotTotalDataSize - (uint32)Margin + (uint32)Offset);
				
		}else{
				NewByteCount = RemainByteCount + (uint32)Offset;		
		}
		
		NewByteCount = SECTOR_ALIGNED_SIZE(VCB->XixcoreVcb.SectorSizeBit, NewByteCount);


		if(Waitable){
			if(NewByteCount > XIFSD_MAX_BUFF_LIMIT){
				NewByteCount = XIFSD_MAX_BUFF_LIMIT;
			}


		}else{
			// Changed by ILGU HONG
			if(Offset|| (RemainOrgByteCount < NewByteCount)){
			// Changed by ILGU HONG END
				if(NewByteCount > XIFSD_MAX_BUFF_LIMIT){
					NewByteCount = XIFSD_MAX_BUFF_LIMIT;
				}
			}
		}

		if(PhyStartByteOffset + RemainByteCount > NewPhyStartByteOffset + NewByteCount){
			TruncateSize = 0;
		}else{
			TruncateSize = (uint32)((NewPhyStartByteOffset + NewByteCount) - (PhyStartByteOffset + RemainByteCount));
		}

		OrgTransferSize = NewByteCount - Offset - TruncateSize;
		ASSERT(OrgTransferSize > 0);
		

		// Added by ILGU HONG
		ChangedOrgTransferSize = SECTOR_ALIGNED_SIZE(VCB->XixcoreVcb.SectorSizeBit, OrgTransferSize);
		if((ChangedOrgTransferSize != OrgTransferSize) && (ChangedOrgTransferSize > RemainByteCount)){
			bUnalign = TRUE;
		}
		// Added by ILGU HONG END


		
		IoRun[IoRunIndex].DiskOffset = NewPhyStartByteOffset;
		// Changed by ILGU HONG
		// Changed by ILGU HONG ND
		IoRun[IoRunIndex].UserBuffer = CurrentUserBuffer;
		
		// Changed by ILGU HONG
		if(Offset || bUnalign ){
			if(Waitable == FALSE) {
				DbgPrint("Can process!!\n");
				RC = STATUS_ACCESS_DENIED;
				goto error_out;
			}
			IoRun[IoRunIndex].DiskByteCount = NewByteCount;
		// Changed by ILGU HONG END
			DebugTrace(DEBUG_LEVEL_ALL, 
				(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB),
				("Allocation Size NumSector %ld\n", NewByteCount));

			IoRun[IoRunIndex].TransferBuffer = ExAllocatePoolWithTag(NonPagedPool, NewByteCount, TAG_BUFFER);
			if(!IoRun[IoRunIndex].TransferBuffer){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				goto error_out;
			}

			if(bReadIo){
				RtlZeroMemory(IoRun[IoRunIndex].TransferBuffer, NewByteCount);
			}else{
				ASSERT(Offset == 0);
				RtlZeroMemory(IoRun[IoRunIndex].TransferBuffer, NewByteCount);
				RtlCopyMemory(IoRun[IoRunIndex].TransferBuffer, CurrentUserBuffer, (NewByteCount - TruncateSize));
			}	
			

			IoRun[IoRunIndex].TransferMdl = IoAllocateMdl(IoRun[IoRunIndex].TransferBuffer, IoRun[IoRunIndex].DiskByteCount, FALSE, FALSE, NULL);
			if(!IoRun[IoRunIndex].TransferMdl){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				goto error_out;
			}

			IoRun[IoRunIndex].TransferVirtualAddress = IoRun[IoRunIndex].TransferBuffer;
			MmBuildMdlForNonPagedPool(IoRun[IoRunIndex].TransferMdl);
			*bUnAligned = TRUE;
			DebugTrace(DEBUG_LEVEL_ALL, 
			(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB|DEBUG_TARGET_ALL),
				("UnAligned !!! offset(%ld) TruncateSize(%ld) NewByteCount(%ld) OrgTransferSize(%ld) PhyStartByteOffset(%I64d) NewPhyStartByteOffset(%I64d\n", 
					Offset, TruncateSize, NewByteCount, OrgTransferSize, PhyStartByteOffset,NewPhyStartByteOffset));

			


		}else{
			// Added by ILGU HONG
			IoRun[IoRunIndex].DiskByteCount = ChangedOrgTransferSize;
			// Added by ILGU HONG END
			IoRun[IoRunIndex].TransferBuffer = CurrentUserBuffer;
			IoRun[IoRunIndex].TransferMdl = Irp->MdlAddress;
			IoRun[IoRunIndex].TransferVirtualAddress =Add2Ptr(Irp->UserBuffer ,(PreviousProcessByteCount + CurrentUserBufferOffset));
		}

		IoRun[IoRunIndex].TransferByteCount = OrgTransferSize ;
		IoRun[IoRunIndex].TransferBufferOffset = Offset;

		DebugTrace(DEBUG_LEVEL_ALL, 
			(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB|DEBUG_TARGET_ALL),
			("[INFO IoRun(%ld)]: DOff(%I64d) DBC(%ld) UBuff(%p) TBC(%ld) TBOff(%ld)\n",
			IoRunIndex,
			IoRun[IoRunIndex].DiskOffset,
			IoRun[IoRunIndex].DiskByteCount,
			IoRun[IoRunIndex].UserBuffer, 
			IoRun[IoRunIndex].TransferByteCount,
			IoRun[IoRunIndex].TransferBufferOffset));

		
		


		CurrentUserBufferOffset += OrgTransferSize;
		CurrentUserBuffer = (UCHAR *)CurrentUserBuffer + OrgTransferSize; //Add2Ptr(Irp->UserBuffer, CurrentUserBufferOffset);

		RemainByteCount -= OrgTransferSize;
		RemainOrgByteCount -= OrgTransferSize;
		ByteOffset += OrgTransferSize;
		TransferedByteCount += OrgTransferSize;
		DebugTrace(DEBUG_LEVEL_ALL, 
			(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB|DEBUG_TARGET_ALL),
			("[INFO After IoRun(%ld)]: Remain(%ld) BOffset(%I64d) TByte(%ld)\n",
				IoRunIndex,
				RemainByteCount,
				ByteOffset,
				TransferedByteCount));

		IoRunIndex++;
		

	}

	*pNumberRuns = IoRunIndex;
	*ProcessedByteCount = TransferedByteCount ;
	return STATUS_SUCCESS;
	
error_out:
	if(IoRun){
		for(i = 0; i<IoRunIndex; i++){
			if(IoRun[i].TransferMdl != IrpContext->Irp->MdlAddress){
			            if (IoRun[i].TransferMdl != NULL) {
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free MDL!!!\n"));
			                IoFreeMdl( IoRun[i].TransferMdl );
			            }

			            //
			            //  Now free any buffer we may have allocated.  If the Mdl
			            //  doesn't match the original Mdl then free the buffer.
			            //

			            if (IoRun[i].TransferBuffer != NULL) {
	
			                ExFreePool( IoRun[i].TransferBuffer );
			            }						
			}
		}
	}
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB),
		("Exit xixfs_FilePrepareBuffers\n"));	
	return RC;
}


NTSTATUS
xixfs_IOPrepareBuffers (
    IN PXIXFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIXFS_FCB 			FCB,
	IN TYPE_OF_OPEN			TypeOfOpen,
    IN uint64 				ByteOffset,
    IN uint32 				ByteCount,
	IN uint32				PreviousProcessByteCount,
    IN PVOID				Buffer,
    IN uint32				LotSize,
    IN uint32				NumOfIoRun,
    IN PIO_RUN 				IoRun,
    OUT uint32				*pNumberRuns,
    OUT BOOLEAN				*bUnAligned,
    OUT uint32				*ProcessedByteCount,
	BOOLEAN					bReadIo,
	uint32					OrgBuffLenth
    )
{
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_ALL, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB| DEBUG_TARGET_ALL),
		("Enter xixfs_IOPrepareBuffers\n"));

	if(TypeOfOpen <= UserVolumeOpen){
		return xixfs_VolumePrepareBuffers (
											IrpContext,
											Irp,
											FCB,
											ByteOffset,
											ByteCount,
											PreviousProcessByteCount,
											Buffer,
											LotSize,
											NumOfIoRun,
											IoRun,
											pNumberRuns,
											bUnAligned,
											ProcessedByteCount,
											bReadIo,
											OrgBuffLenth
											);	
	}else{

		return xixfs_FilePrepareBuffers (   
											IrpContext,
											Irp,
											FCB,
											ByteOffset,
											ByteCount,
											PreviousProcessByteCount,
											Buffer,
											LotSize,
											NumOfIoRun,
											IoRun,
											pNumberRuns,
											bUnAligned,
											ProcessedByteCount,
											bReadIo,
											OrgBuffLenth
											);	
	}
}


BOOLEAN
xixfs_IOFinishBuffers (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PIO_RUN IoRuns,
    IN uint32 RunCount,
    IN BOOLEAN	FinalCleanup,
	IN BOOLEAN	ReadOp
    )
{
    BOOLEAN FlushIoBuffers = FALSE;

    uint32 RemainingEntries = RunCount;
    PIO_RUN ThisIoRun = &IoRuns[RunCount - 1];

    //PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_IOFinishBuffers\n"));

    //
    //  Walk through each entry in the IoRun array.
    //

    while (RemainingEntries != 0) {

        //
        //  We only need to deal with the case of an unaligned transfer.
        //

        if (ThisIoRun->TransferByteCount != 0) {

            //
            //  If not the final cleanup then transfer the data to the
            //  user's buffer and remember that we will need to flush
            //  the user's buffer to memory.
            //

            if (!FinalCleanup) {

                //
                //  If we are shifting in the user's buffer then use
                //  MoveMemory.
                //
				if(ReadOp){

					if (ThisIoRun->TransferMdl != IrpContext->Irp->MdlAddress) {

						RtlCopyMemory( ThisIoRun->UserBuffer,
									   Add2Ptr( ThisIoRun->TransferBuffer,
												ThisIoRun->TransferBufferOffset),
									   ThisIoRun->TransferByteCount );
					}
				}

                FlushIoBuffers = TRUE;
            }

            //
            //  Free any Mdl we may have allocated.  If the Mdl isn't
            //  present then we must have failed during the allocation
            //  phase.
            //

            if (ThisIoRun->TransferMdl != IrpContext->Irp->MdlAddress) {

                if (ThisIoRun->TransferMdl != NULL) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free MDL!!!\n"));
                    IoFreeMdl( ThisIoRun->TransferMdl );
					ThisIoRun->TransferMdl = NULL;
                }

                //
                //  Now free any buffer we may have allocated.  If the Mdl
                //  doesn't match the original Mdl then free the buffer.
                //

                if (ThisIoRun->TransferBuffer != NULL) {

                    ExFreePool( ThisIoRun->TransferBuffer );
					ThisIoRun->TransferBuffer = NULL;
                }
            }
        }

        //
        //  Now handle the case where we failed in the process
        //  of allocating associated Irps and Mdls.
        //

        if (ThisIoRun->SavedIrp != NULL) {

            if (ThisIoRun->SavedIrp->MdlAddress != NULL) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free MDL!!!\n"));
                IoFreeMdl( ThisIoRun->SavedIrp->MdlAddress );
				
            }

            IoFreeIrp( ThisIoRun->SavedIrp );
        }

        //
        //  Move to the previous IoRun entry.
        //

        ThisIoRun -= 1;
        RemainingEntries -= 1;
    }

    //
    //  If we copied any data then flush the Io buffers.
    //

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_IOFinishBuffers\n"));
    return FlushIoBuffers;
}



VOID
xixfs_IOWaitSync (
    IN PXIXFS_IRPCONTEXT IrpContext
    )
{
   
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_IOWaitSync\n"));

    KeWaitForSingleObject( &(IrpContext->IoContext->SyncEvent),
                           Executive,
                           KernelMode,
                           FALSE,
                           NULL );

    KeClearEvent( &(IrpContext->IoContext->SyncEvent) );
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_IOWaitSync\n"));

    return;
}

NTSTATUS
xixfs_MulipleAsyncIoCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
 {
	PXIXFS_IO_CONTEXT IoContext = Context;


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Enter xixfs_MulipleAsyncIoCompletionRoutine  IrpCount(%ld) Status(%x) \n", 
			IoContext->IrpCount ,Irp->IoStatus.Status));




	if (!NT_SUCCESS( Irp->IoStatus.Status )) {

		//ASSERT(FALSE);
		InterlockedExchange( &IoContext->Status, Irp->IoStatus.Status );
		IoContext->MasterIrp->IoStatus.Information = 0;
	}

	

	//
	//  We must do this here since IoCompleteRequest won't get a chance
	//  on this associated Irp.
	//



	if (InterlockedDecrement( &IoContext->IrpCount ) == 0) {

		IoMarkIrpPending( IoContext->MasterIrp );
		//
		//  Update the Master Irp with any error status from the associated Irps.
		//

		IoContext->MasterIrp->IoStatus.Status = IoContext->Status;
		IoContext->MasterIrp->IoStatus.Information = 0;

		DebugTrace(DEBUG_LEVEL_INFO, 
				(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
				(" xixfs_MulipleAsyncIoCompletionRoutine Status (%x) Information (%ld)\n", 
				IoContext->Status, IoContext->Async.RequestedByteCount));



		if (NT_SUCCESS( IoContext->MasterIrp->IoStatus.Status )) {
			IoContext->MasterIrp->IoStatus.Information = IoContext->Async.RequestedByteCount;
			//DbgPrint("Async Write Result %ld\n", IoContext->Async.RequestedByteCount);
		}

		ExReleaseResourceForThreadLite( IoContext->Async.Resource, IoContext->Async.ResourceThreadId );

		ExFreePool(IoContext);
		
		DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit xixfs_MulipleAsyncIoCompletionRoutine  \n"));
		return STATUS_SUCCESS;

	}else{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free MDL!!!\n"));
		IoFreeMdl( Irp->MdlAddress );
		IoFreeIrp( Irp );
	}

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit xixfs_MulipleAsyncIoCompletionRoutine  \n"));
	return STATUS_MORE_PROCESSING_REQUIRED;
}



NTSTATUS
xixfs_MulipleSyncIoCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
 {
	PXIXFS_IO_CONTEXT IoContext = Context;
	

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" call xixfs_MulipleSyncIoCompletionRoutine  IrpCount(%ld) Status(%x) \n", 
			IoContext->IrpCount ,Irp->IoStatus.Status));


	if (!NT_SUCCESS( Irp->IoStatus.Status )) {

		//ASSERT(FALSE);
		//DbgPrint("xixfs_MulipleSyncIoCompletionRoutine sub Status (%x) \n", Irp->IoStatus.Status);

		InterlockedExchange( &IoContext->Status, Irp->IoStatus.Status );
		IoContext->MasterIrp->IoStatus.Information = 0;
	}

	//
	//  We must do this here since IoCompleteRequest won't get a chance
	//  on this associated Irp.
	//
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free TEMP MDL!!!\n"));
	IoFreeMdl( Irp->MdlAddress );
	IoFreeIrp( Irp );

	if (InterlockedDecrement( &IoContext->IrpCount ) == 0) {

		//
		//  Update the Master Irp with any error status from the associated Irps.
		//
		DebugTrace(DEBUG_LEVEL_INFO, 
				(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
				("xixfs_MulipleSyncIoCompletionRoutine Status (%x) \n", IoContext->Status));
		
		IoContext->MasterIrp->IoStatus.Status = IoContext->Status;
		KeSetEvent( &(IoContext->SyncEvent), 0, FALSE );
	}

	UNREFERENCED_PARAMETER( DeviceObject );

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit xixfs_MulipleSyncIoCompletionRoutine  \n"));
	return STATUS_MORE_PROCESSING_REQUIRED;
}


VOID
xixfs_IOMultipleAsyncIO(
	IN PXIXFS_IRPCONTEXT	IrpContext,
	IN PXIXFS_VCB		pVCB,
	IN ULONG RunCount,
	IN PIO_RUN IoRuns
	)
{
	PIO_STACK_LOCATION			IrpSp;
	PIO_COMPLETION_ROUTINE		CompletionRoutine;
	PMDL						Mdl;
	PIRP						Irp;
	PIRP						MasterIrp;
	ULONG						UnwindRunCount;



	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Enter xixfs_IOMultipleAsyncIO  \n"));	


	if(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		CompletionRoutine= xixfs_MulipleSyncIoCompletionRoutine;
	}else{
		CompletionRoutine = xixfs_MulipleAsyncIoCompletionRoutine;
	}
	

	MasterIrp = IrpContext->Irp;

	for (UnwindRunCount = 0; UnwindRunCount < RunCount; UnwindRunCount += 1) 
	{

	    //
	    //  Create an associated IRP, making sure there is one stack entry for
	    //  us, as well.
	    //

	    IoRuns[UnwindRunCount].SavedIrp =
	    Irp = IoMakeAssociatedIrp( MasterIrp, (CCHAR)(IrpContext->VCB->TargetDeviceObject->StackSize + 1) );

	    if (Irp == NULL) {

	        IrpContext->Irp->IoStatus.Information = 0;
	        XifsdRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
	    }

	    //
	    // Allocate and build a partial Mdl for the request.
	    //

	    Mdl = IoAllocateMdl( IoRuns[UnwindRunCount].TransferVirtualAddress,
	                         IoRuns[UnwindRunCount].DiskByteCount,
	                         FALSE,
	                         FALSE,
	                         Irp );

	    if (Mdl == NULL) {

	        IrpContext->Irp->IoStatus.Information = 0;
	        XifsdRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
	    }

	    IoBuildPartialMdl( IoRuns[UnwindRunCount].TransferMdl,
	                       Mdl,
	                       IoRuns[UnwindRunCount].TransferVirtualAddress,
	                       IoRuns[UnwindRunCount].DiskByteCount );

	    //
	    //  Get the first IRP stack location in the associated Irp
	    //

	    IoSetNextIrpStackLocation( Irp );
	    IrpSp = IoGetCurrentIrpStackLocation( Irp );

	    //
	    //  Setup the Stack location to describe our read.
	    //

	    IrpSp->MajorFunction = IrpContext->MajorFunction;
	    IrpSp->Parameters.Read.Length = IoRuns[UnwindRunCount].DiskByteCount;
	    IrpSp->Parameters.Read.ByteOffset.QuadPart = IoRuns[UnwindRunCount].DiskOffset;

	    //
	    // Set up the completion routine address in our stack frame.
	    //

	    IoSetCompletionRoutine( Irp,
	                            CompletionRoutine,
	                            (PVOID)IrpContext->IoContext,
	                            TRUE,
	                            TRUE,
	                            TRUE );

	    //
	    //  Setup the next IRP stack location in the associated Irp for the disk
	    //  driver beneath us.
	    //

	    IrpSp = IoGetNextIrpStackLocation( Irp );

	    //
	    //  Setup the Stack location to do a read from the disk driver.
	    //

	    IrpSp->MajorFunction = IrpContext->MajorFunction;
	    IrpSp->Parameters.Read.Length = IoRuns[UnwindRunCount].DiskByteCount;
	    IrpSp->Parameters.Read.ByteOffset.QuadPart = IoRuns[UnwindRunCount].DiskOffset;
	}

	//
	//  We only need to set the associated IRP count in the master irp to
	//  make it a master IRP.  But we set the count to one more than our
	//  caller requested, because we do not want the I/O system to complete
	//  the I/O.  We also set our own count.
	//

	IrpContext->IoContext->IrpCount = RunCount;
	IrpContext->IoContext->MasterIrp = MasterIrp;

	//
	//  We set the count in the master Irp to 1 since typically we
	//  will clean up the associated irps ourselves.  Setting this to one
	//  means completing the last associated Irp with SUCCESS (in the async
	//  case) will complete the master irp.
	//

	MasterIrp->AssociatedIrp.IrpCount = 1;

	//
	//  Now that all the dangerous work is done, issue the Io requests
	//

	for (UnwindRunCount = 0; UnwindRunCount < RunCount; UnwindRunCount++) 
	{

	    Irp = IoRuns[UnwindRunCount].SavedIrp;
	    IoRuns[UnwindRunCount].SavedIrp = NULL;

	    //
	    //  If IoCallDriver returns an error, it has completed the Irp
	    //  and the error will be caught by our completion routines
	    //  and dealt with as a normal IO error.
	    //

	    (VOID) IoCallDriver( pVCB->TargetDeviceObject, Irp );
	}

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit xixfs_IOMultipleAsyncIO  \n"));	

	return;
	
}



NTSTATUS
xixfs_SingleSyncIoCompletionRoutine(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	PXIXFS_IO_CONTEXT IoContext = NULL;
	IoContext = (PXIXFS_IO_CONTEXT)Context;
	


	//
	//  Store the correct information field into the Irp.
	//
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_SingleSyncIoCompletionRoutine  \n"));

	if (!NT_SUCCESS( Irp->IoStatus.Status )) {
		//ASSERT(FALSE);
	    Irp->IoStatus.Information = 0;
	}

	DebugTrace(DEBUG_LEVEL_INFO, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("xixfs_SingleSyncIoCompletionRoutine IoContext (%p)\n", IoContext));

	KeSetEvent( &(IoContext->SyncEvent), 0, FALSE );

	UNREFERENCED_PARAMETER( DeviceObject );
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_SingleSyncIoCompletionRoutine  \n"));

	return STATUS_MORE_PROCESSING_REQUIRED;


}


NTSTATUS
xixfs_SingleAsyncIoCompletionRoutine(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	PXIXFS_IO_CONTEXT IoContext = NULL;
	IoContext = (PXIXFS_IO_CONTEXT)Context;

	
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_SingleAsyncIoCompletionRoutine \n"));


	Irp->IoStatus.Information = 0;

	if (NT_SUCCESS( Irp->IoStatus.Status )) {

	    Irp->IoStatus.Information = IoContext->Async.RequestedByteCount;
		//DbgPrint("Async Write Result %ld\n", IoContext->Async.RequestedByteCount);
	}



	if(!NT_SUCCESS(Irp->IoStatus.Status)){
		//ASSERT(FALSE);
	}

	DebugTrace(DEBUG_LEVEL_INFO, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
				(" call xixfs_SingleAsyncIoCompletionRoutine  \n"));

	IoMarkIrpPending( Irp );

	//
	//  Now release the resource
	//

	ExReleaseResourceForThreadLite( IoContext->Async.Resource,
	                            IoContext->Async.ResourceThreadId );

	//
	//  and finally, free the context record.
	//
	ExFreePool(IoContext);
	
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit XifsdSingleAsyncIoCompletionRoutine \n"));

	UNREFERENCED_PARAMETER( DeviceObject );

	return STATUS_SUCCESS;

	
}



VOID
xixfs_SingleSyncIO(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_VCB		pVCB,
	IN LONGLONG  byteoffset,
	IN ULONG byteCount
)
{
	PIO_STACK_LOCATION		IrpSp;
	PIO_COMPLETION_ROUTINE	CompletionRoutine;


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XifdSingleSyncIO \n"));
	
	CompletionRoutine = xixfs_SingleSyncIoCompletionRoutine;
	

	IoSetCompletionRoutine(IrpContext->Irp, 
							CompletionRoutine,
							(PVOID)IrpContext->IoContext,
							TRUE,
							TRUE,
							TRUE);

	IrpSp = IoGetNextIrpStackLocation(IrpContext->Irp);
	IrpSp->MajorFunction = IrpContext->MajorFunction;
	IrpSp->Parameters.Read.Length = byteCount;
	IrpSp->Parameters.Read.ByteOffset.QuadPart = byteoffset;

	IoCallDriver(pVCB->TargetDeviceObject, IrpContext->Irp);

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_SingleSyncIO \n"));
	return;
}



VOID
xixfs_SingleAsyncIO(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_VCB		pVCB,
	IN LONGLONG  byteoffset,
	IN ULONG byteCount
)
{
	PIO_STACK_LOCATION		IrpSp;
	PIO_COMPLETION_ROUTINE	CompletionRoutine;


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_SingleAsyncIO \n"));



	if(XIXCORE_TEST_FLAGS( IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		CompletionRoutine = xixfs_SingleSyncIoCompletionRoutine;
	}else{
		CompletionRoutine = xixfs_SingleAsyncIoCompletionRoutine;
	}



	IoSetCompletionRoutine(IrpContext->Irp, 
							CompletionRoutine,
							(PVOID)(IrpContext->IoContext),
							TRUE,
							TRUE,
							TRUE);

	DebugTrace(DEBUG_LEVEL_INFO, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
							(" xixfs_SingleAsyncIO IoContext (%p)\n", IrpContext->IoContext));

	IrpSp = IoGetNextIrpStackLocation(IrpContext->Irp);
	IrpSp->MajorFunction = IrpContext->MajorFunction;
	IrpSp->Parameters.Read.Length = byteCount;
	IrpSp->Parameters.Read.ByteOffset.QuadPart = byteoffset;

	IoCallDriver(pVCB->TargetDeviceObject, IrpContext->Irp);

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_SingleAsyncIO \n"));

	
	return;
}





NTSTATUS
xixfs_FileNonCachedIo(
 	IN PXIXFS_FCB		FCB,
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP				Irp,
	IN uint64 			ByteOffset,
	IN ULONG 			ByteCount,
	IN BOOLEAN			IsReadOp,
	IN TYPE_OF_OPEN		TypeOfOpen,
	IN PERESOURCE		PtrResourceAcquired
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	uint32 		NumberOfRuns = 0;
	uint64 		CurrentStartByteOffset = 0;
	uint32		LotSize = 0;
	int32		RemainByteCount = 0;
	int32		TotalProcessByteCount = 0;
	uint32		ProcessedByteCount = 0;
	IO_RUN   	IoRun[XIFS_DEFAULT_IO_RUN_COUNT];
	PVOID		Buffer = NULL;
	BOOLEAN		UnAligned = FALSE;
	uint32		CleanUpRunCount = 0;
	BOOLEAN		FlushBuffer = FALSE;
	PXIXFS_VCB	pVCB = NULL;
	BOOLEAN		FirstPass = TRUE;
	uint32		Waitable = 0;
	BOOLEAN		bChangeBuffer = FALSE;
	uint32		OrgBuffLenth = 0;


	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter xixfs_FileNonCachedIo \n"));

	ASSERT_FCB(FCB);
	
	pVCB = FCB->PtrVCB;
	ASSERT(pVCB);

	LotSize = pVCB->XixcoreVcb.LotSize;

	Waitable = XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	
	// Write function flag set
	
	if(IrpContext->MajorFunction == IRP_MJ_WRITE){
		IrpContext->IoContext->IrpSpFlags = (SL_FT_SEQUENTIAL_WRITE | SL_WRITE_THROUGH);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = ByteCount;
	
	ASSERT(IrpContext->IoContext != NULL);
	
	RtlZeroMemory(IoRun, sizeof(IO_RUN)*XIFS_DEFAULT_IO_RUN_COUNT);

	DebugTrace(DEBUG_LEVEL_ALL, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB|DEBUG_TARGET_ALL),
		(" ByteOffset(%I64d) ByteCount(%ld) \n", ByteOffset, ByteCount));


	if(Irp->MdlAddress == NULL){
		DebugTrace(DEBUG_LEVEL_ALL, 
			(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB|DEBUG_TARGET_ALL),
			(" Create IrpContext->Irp->MdlAddress == NULL \n"));
			//return STATUS_INVALID_PARAMETER;
			//XifsdCreateUserMdl(IrpContext, Irp, ByteCount, TRUE, IoWriteAccess);
	}


	RC = xixfs_PinCallersBuffer(
		Irp,
		(IrpContext->MajorFunction == IRP_MJ_READ) ? TRUE: FALSE, 
		ByteCount);



	if((Irp->UserBuffer == NULL)
		&& (Irp->MdlAddress != NULL)
		&& (Irp->MdlAddress->StartVa != NULL))
	{
		


		bChangeBuffer = TRUE;
		//Irp->UserBuffer = (Irp->MdlAddress->StartVa);	
		Irp->UserBuffer =MmGetMdlVirtualAddress(Irp->MdlAddress);
		

		DebugTrace(DEBUG_LEVEL_ALL, 
			(DEBUG_TARGET_READ| DEBUG_TARGET_WRITE),
			(" SET USER BUFFER TEMP Buffer(%p) New Irp->UserBuffer(%p)!!\n", Buffer, Irp->UserBuffer));

		//DbgPrint(" SET USER BUFFER TEMP Buffer(%p) New Irp->UserBuffer(%p)!!\n", Buffer, Irp->UserBuffer);
		
	}


	if(IsReadOp){
		if(TypeOfOpen == UserFileOpen){

			uint32 OrgReadLength = 0;
			LARGE_INTEGER OrgByteOffset = {0,0};
			
			OrgBuffLenth = OrgReadLength = (IoGetCurrentIrpStackLocation(Irp))->Parameters.Read.Length;	
			OrgByteOffset = (IoGetCurrentIrpStackLocation(Irp))->Parameters.Read.ByteOffset;
		
			if((OrgByteOffset.QuadPart + OrgReadLength ) >= FCB->FileSize.QuadPart){
					uint8 *ReadBuffer = xixfs_GetCallersBuffer(Irp);
					DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_READ| DEBUG_TARGET_WRITE),
						("SetZero 1 OrgByteOffSet %I64d req length%ld Current File Size %I64d\n",
							OrgByteOffset.QuadPart, OrgReadLength, FCB->FileSize.QuadPart));
					RtlZeroMemory(ReadBuffer, OrgReadLength);
			}
			

			if(ByteOffset + ByteCount > (uint64)FCB->AllocationSize.QuadPart){
				if(ByteOffset > (uint64)FCB->AllocationSize.QuadPart) {
					uint8 *ReadBuffer = xixfs_GetCallersBuffer(Irp);
					RtlZeroMemory(ReadBuffer, ByteCount);
					DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_READ| DEBUG_TARGET_WRITE),
						("SetZero 2 ByteOffset %I64d ByteCount%ld Current File Size %I64d\n",
						ByteOffset, ByteCount, FCB->AllocationSize.QuadPart));
					Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = ByteCount;
					return STATUS_SUCCESS;
					
				}else{
					uint32	Magin = 0;
					uint32	Size =0;
					uint8	*ReadBuffer = xixfs_GetCallersBuffer(Irp);
					Magin = (uint32)(ByteOffset + ByteCount  - FCB->AllocationSize.QuadPart);
					Size = ByteCount - Magin;
					ReadBuffer += Magin;
					DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_READ| DEBUG_TARGET_WRITE),
						("SetZero 3 ByteOffset %I64d ByteCount%ld Current File Size %I64d\n",
						ByteOffset, ByteCount, FCB->AllocationSize.QuadPart));
					RtlZeroMemory(ReadBuffer, Size);
					//ByteCount = Magin;
					
				}
			}

		}

	}else{

		OrgBuffLenth = (IoGetCurrentIrpStackLocation(Irp))->Parameters.Write.Length;
	}


	Buffer = (PVOID)xixfs_GetCallersBuffer(Irp);	

	CurrentStartByteOffset = ByteOffset;
	RemainByteCount = ByteCount;

	//OldBuffer = Irp->UserBuffer;
	//Irp->UserBuffer = Buffer;	



	try{	

		do{
			RtlZeroMemory(IoRun, sizeof(IO_RUN)*XIFS_DEFAULT_IO_RUN_COUNT);

			RC = xixfs_IOPrepareBuffers(
								IrpContext,
								Irp,
								FCB,
								TypeOfOpen,
								CurrentStartByteOffset,
								RemainByteCount,
								TotalProcessByteCount,
								Buffer,
								LotSize,
								XIFS_DEFAULT_IO_RUN_COUNT,
								IoRun,
								&NumberOfRuns,
								&UnAligned,
								&ProcessedByteCount,
								IsReadOp,
								OrgBuffLenth
								);
			
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					(" Fail XixFsdPrepareBuffer!! \n"));
				CleanUpRunCount = 0;
				try_return(RC);
			}
			

			CleanUpRunCount = NumberOfRuns;


			

			if((NumberOfRuns == 1) && !UnAligned && FirstPass && (ProcessedByteCount == ByteCount)){
				

				if(Waitable == FALSE){
					IrpContext->IoContext->Async.RequestedByteCount = ProcessedByteCount;
				}


				xixfs_SingleAsyncIO(
							IrpContext,
							pVCB,
							IoRun[0].DiskOffset,
							IoRun[0].DiskByteCount
							);
				
				CleanUpRunCount = 0;

				if(Waitable){
					xixfs_IOWaitSync(IrpContext);
					RC = IrpContext->Irp->IoStatus.Status;

					DebugTrace(DEBUG_LEVEL_INFO, 
						(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB| DEBUG_TARGET_ALL),
						(" xixfs_SingleAsyncIO Status (%x) \n", RC));

					//DbgPrint("Sync Write Result %ld\n", ProcessedByteCount);

					if (!NT_SUCCESS( RC )) {
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							(" FAIL xixfs_SingleAsyncIO Status (%x) \n", RC));
						try_return(RC);
					}
					try_return(RC);

				}else {
					XIXCORE_CLEAR_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT);
					RC = STATUS_PENDING;
				}
				
				try_return(RC);
				
			}else{

				if(Waitable == FALSE){
					IrpContext->IoContext->Async.RequestedByteCount = ProcessedByteCount;
				}


				xixfs_IOMultipleAsyncIO(
					IrpContext, 
					pVCB,
					NumberOfRuns, 
					IoRun
					);
			
				if(Waitable){
					xixfs_IOWaitSync(IrpContext);
					RC = IrpContext->Irp->IoStatus.Status;
					
					DebugTrace(DEBUG_LEVEL_INFO, 
						(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB | DEBUG_TARGET_ALL),
						(" xixfs_IOMultipleAsyncIO Status (%x) \n", RC));


					//DbgPrint("Sync Write Result %ld\n", ProcessedByteCount);
					if (!NT_SUCCESS( RC )) {
						try_return(RC);
					}

					if (UnAligned &&
						xixfs_IOFinishBuffers( IrpContext, IoRun, CleanUpRunCount, FALSE, IsReadOp )) {
						FlushBuffer = TRUE;
					}

					CleanUpRunCount = 0;
					RemainByteCount -= ProcessedByteCount;
					OrgBuffLenth -= ProcessedByteCount;
					CurrentStartByteOffset += ProcessedByteCount;
					Buffer =  (PCHAR)Buffer + ProcessedByteCount;
					TotalProcessByteCount += ProcessedByteCount;
				}else{
					XIXCORE_CLEAR_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT);
					CleanUpRunCount = 0;
					//IrpContext->IoContext->Async.RequestedByteCount = ProcessedByteCount;
					try_return(RC = STATUS_PENDING);
					break;
				}	
	
			}	

			FirstPass = FALSE;

		DebugTrace(DEBUG_LEVEL_ALL, 
			(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB|DEBUG_TARGET_ALL),
				(" RemainByteCount(%ld) CurrentStartByteOffset(%I64d) ProcessedByteCount (%ld)\n",  RemainByteCount, CurrentStartByteOffset, ProcessedByteCount));

		}while(RemainByteCount > 0);

		if(FlushBuffer){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						(" Flush Buffer!!!!!  \n"));	
			KeFlushIoBuffers(IrpContext->Irp->MdlAddress, IsReadOp, FALSE);
		}


	}finally{
	
		if (CleanUpRunCount != 0) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						(" Remove Buffer!!!!!  \n"));			
			xixfs_IOFinishBuffers( IrpContext, IoRun, CleanUpRunCount, TRUE, IsReadOp );
		}

	}
	
	if(bChangeBuffer && Waitable){
		Irp->UserBuffer = NULL;
	}
	
	DebugTrace(DEBUG_LEVEL_ALL, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit xixfs_FileNonCachedIo \n"));
	return RC;
}



NTSTATUS
xixfs_ProcessFileNonCachedIo(
	IN PVOID			pContext
)
{

	NTSTATUS				RC = STATUS_SUCCESS;
	PXIXFS_IRPCONTEXT		pIrpContext = NULL;
	PIRP					pIrp = NULL;
	PXIXFS_IO_CONTEXT		pIoContext = NULL;
	XIXFS_IO_CONTEXT			LocalIoContext;

	PIO_STACK_LOCATION		pIrpSp = NULL;
	LARGE_INTEGER			ByteOffset;
	uint32					Length = 0;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;
	PFILE_OBJECT			pFileObject = NULL;
	PERESOURCE				pResourceAcquired = NULL;
	ERESOURCE_THREAD		ResourceThreadId = 0;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PXIXFS_VCB				pVCB = NULL;
	BOOLEAN					PagingIo = FALSE;
	BOOLEAN					NonBufferedIo = FALSE;
	BOOLEAN					SynchronousIo = FALSE;
	BOOLEAN					bEOF = FALSE;
	BOOLEAN					IsEOFProcessing = FALSE;
	LARGE_INTEGER			OldFileSize;

	pIrpContext = (PXIXFS_IRPCONTEXT)pContext;

	ASSERT_IRPCONTEXT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);


	pIoContext = pIrpContext->IoContext;
	ASSERT(pIoContext);

	pResourceAcquired = pIoContext->Async.Resource;
	ASSERT(pResourceAcquired);

	ResourceThreadId = pIoContext->Async.ResourceThreadId;
	
	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);


	TypeOfOpen = xixfs_DecodeFileObject( pFileObject, &pFCB, &pCCB );



	if((TypeOfOpen == UnopenedFileObject) || (TypeOfOpen == UserDirectoryOpen))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			(" Un supported type %ld\n", TypeOfOpen));
		
		RC = STATUS_INVALID_DEVICE_REQUEST;
		xixfs_CompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	ASSERT_FCB(pFCB);

	pVCB = pFCB->PtrVCB;

	ASSERT_VCB(pVCB);


	if(pIrpContext->MajorFunction == IRP_MJ_WRITE){
		
		if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Has not Host Lock %lx .\n", pFCB));
			
			RC = STATUS_INVALID_DEVICE_REQUEST;
			xixfs_CompleteRequest(pIrpContext, RC, 0);
			return RC;
		}

		ByteOffset = pIrpSp->Parameters.Write.ByteOffset;
		Length = pIrpSp->Parameters.Write.Length;
	}else {

		ByteOffset = pIrpSp->Parameters.Read.ByteOffset;
		Length = pIrpSp->Parameters.Read.Length;
	}


	if ((ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE) && (ByteOffset.HighPart == -1)) {
         	bEOF = TRUE;
         	ByteOffset.QuadPart = pFCB->FileSize.QuadPart;
			DebugTrace(DEBUG_LEVEL_INFO, 
				(DEBUG_TARGET_WRITE|DEBUG_TARGET_READ|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				(" End of File FCB(%x) FileSize(%I64d)\n", pFCB, pFCB->FileSize));
	}



	PagingIo = ((pIrp->Flags & IRP_PAGING_IO) ? TRUE : FALSE);
	NonBufferedIo = ((pIrp->Flags & IRP_NOCACHE) ? TRUE : FALSE);
	SynchronousIo = ((pFileObject->Flags & FO_SYNCHRONOUS_IO) ? TRUE : FALSE);




	try{


		if(pIrpContext->MajorFunction == IRP_MJ_WRITE)
		{
		
			if( (TypeOfOpen == UserFileOpen) &&
				((bEOF) || ((uint64)(ByteOffset.QuadPart + Length) > pFCB->XixcoreFcb.RealAllocationSize))
				)
			{
				uint64 Offset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;


				
				

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				(" Non Buffered File Io Check  FileSize(%x) FileSize(%I64d)\n", pFCB, pFCB->FileSize));

				if(bEOF){
					RequestStartOffset = pFCB->FileSize.QuadPart;
					Offset = pFCB->FileSize.QuadPart + Length;
				}else{
					RequestStartOffset = ByteOffset.QuadPart;
					Offset = ByteOffset.QuadPart + Length;
				}

				CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
							
					RC = xixcore_AddNewLot(
								&pVCB->XixcoreVcb, 
								&pFCB->XixcoreFcb, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								pFCB->XixcoreFcb.AddrLot,
								pFCB->XixcoreFcb.AddrLotSize,
								&pFCB->XixcoreFcb.AddrStartSecIndex
								);
					

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					XifsdLockFcb(NULL, pFCB);
					pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;
					XifsdUnlockFcb(NULL, pFCB);

					if(LotCount != AllocatedLotCount){
						if(pFCB->XixcoreFcb.RealAllocationSize < RequestStartOffset){
							RC = STATUS_INSUFFICIENT_RESOURCES;
							try_return(RC);
						}else if(Offset > pFCB->XixcoreFcb.RealAllocationSize){
							Length = (uint32)(pFCB->XixcoreFcb.RealAllocationSize - RequestStartOffset);
						}else{
							RC = STATUS_INSUFFICIENT_RESOURCES;
							try_return(RC);
						}
					}
					CcSetFileSizes(pFileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
					//DbgPrint(" 10 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					
				}

			}


		}


		if((ByteOffset.QuadPart + Length) > pFCB->ValidDataLength.QuadPart)
		{

			XifsdLockFcb(NULL, pFCB);
			IsEOFProcessing =  (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO) 
									|| xixfs_FileCheckEofWrite(pFCB, &ByteOffset, Length));

						
			if(IsEOFProcessing){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
			}
			XifsdUnlockFcb(NULL, pFCB);
		}

		if(pIrpContext->MajorFunction == IRP_MJ_READ)
		{
			if(IsEOFProcessing){
				if(ByteOffset.QuadPart >= pFCB->ValidDataLength.QuadPart){
					RC = STATUS_END_OF_FILE;
					pIrp->IoStatus.Status = RC;
					pIrp->IoStatus.Information = 0;
					try_return(RC);					
				}
				
				if((ByteOffset.QuadPart + Length) > pFCB->ValidDataLength.QuadPart){
					Length = (uint32)(pFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
				}
				
			}

		}else {
			if(IsEOFProcessing){
				OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

				if((ByteOffset.QuadPart + Length) > pFCB->FileSize.QuadPart){
					pFCB->FileSize.QuadPart = ByteOffset.QuadPart + Length;
					if(CcIsFileCached(pFileObject))
					{
						CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
						//DbgPrint(" 16 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					}

				}

			}
		}

		RtlZeroMemory(&LocalIoContext, sizeof(XIXFS_IO_CONTEXT));
		pIrpContext->IoContext = &LocalIoContext;
		KeInitializeEvent(&(pIrpContext->IoContext->SyncEvent), NotificationEvent, FALSE);

		pIrpContext->IoContext->pIrpContext = pIrpContext;
		pIrpContext->IoContext->pIrp = pIrp;


		RC = xixfs_FileNonCachedIo (
			 	pFCB,
				pIrpContext,
				pIrp,
				ByteOffset.QuadPart,
				Length,
				((pIrpContext->MajorFunction == IRP_MJ_WRITE)?FALSE:TRUE),
				TypeOfOpen,
				pResourceAcquired
				);

		if(RC == STATUS_PENDING){
			ASSERT(FALSE);
		}else{
			if(NT_SUCCESS(RC)){

				if(pIrpContext->MajorFunction == IRP_MJ_WRITE)
				{	
					

					BOOLEAN bUpdataLength = FALSE;
						
					XifsdLockFcb(NULL, pFCB);
					if (SynchronousIo && !PagingIo){
						pIrpSp->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + Length;
					}


					if(pFCB->ValidDataLength.QuadPart < (ByteOffset.QuadPart + Length)){
						pFCB->ValidDataLength.QuadPart = ByteOffset.QuadPart + Length;
						bUpdataLength = TRUE;

					}


					if( pFCB->FileSize.QuadPart < ByteOffset.QuadPart + Length){
						pFCB->FileSize.QuadPart =   ByteOffset.QuadPart + Length;
						bUpdataLength = TRUE;		

					}

					if(bUpdataLength){

						if(CcIsFileCached(pFileObject))
						{
							CcGetFileSizePointer(pFileObject)->QuadPart = ByteOffset.QuadPart + Length;
							//DbgPrint(" 15 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
						}
					}

					if( pFCB->RealFileSize < (uint64)(ByteOffset.QuadPart + Length)){
						pFCB->RealFileSize = ByteOffset.QuadPart + Length;
					}


					if(pFCB->XixcoreFcb.WriteStartOffset ==  -1){
						pFCB->XixcoreFcb.WriteStartOffset = ByteOffset.QuadPart;
						XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
					}

					if(pFCB->XixcoreFcb.WriteStartOffset > ByteOffset.QuadPart){
						pFCB->XixcoreFcb.WriteStartOffset = ByteOffset.QuadPart;
						XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
						
					}
					XifsdUnlockFcb(NULL, pFCB);
					

				}else{
					if (SynchronousIo && !PagingIo){
						pIrpSp->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + Length;
					}
				}

				
			}else{

				if(pIrpContext->MajorFunction == IRP_MJ_WRITE){
					if(IsEOFProcessing){
						XifsdLockFcb(NULL, pFCB);
						pFCB->FileSize.QuadPart = OldFileSize.QuadPart;
						XifsdUnlockFcb(NULL, pFCB);
					}
				}

			}
		}


	}finally{


			if(IsEOFProcessing){

				XifsdLockFcb(NULL, pFCB);
				xixfs_FileFinishIoEof(pFCB);
				XifsdUnlockFcb(NULL, pFCB);
			}

			if(pResourceAcquired){
					ExReleaseResourceForThreadLite( pResourceAcquired,ResourceThreadId );			
			}
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Result  XifsdCommonWrite  PtrIrp(%p) RC(%x)\n", pIrp, RC));

			xixfs_CompleteRequest(pIrpContext, RC, Length );

			ExFreePool(pIoContext);

	}

	return RC;
}


VOID
xixfs_AddToWorkque(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter  xixfs_AddToWorkque\n"));	
	xixfs_PostRequest(IrpContext, Irp);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit  xixfs_AddToWorkque\n"));	
}


VOID
xixfs_PrePostIrp(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
)
{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    BOOLEAN RemovedFcb = FALSE;
	BOOLEAN CanWait = FALSE;
    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter  xixfs_PrePostIrp\n"));

    ASSERT_IRPCONTEXT( IrpContext );
    ASSERT( Irp );

	CanWait = XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
    //
    //  Case on the type of the operation.
    //

    switch (IrpContext->MajorFunction) {

    case IRP_MJ_CREATE :

        //
        //  If called from the oplock package then there is an
        //  Fcb to possibly teardown.  We will call the teardown
        //  routine and release the Fcb if still present.  The cleanup
        //  code in create will know not to release this Fcb because
        //  we will clear the pointer.
        //

        if ((IrpContext->TeardownFcb != NULL) &&
            *(IrpContext->TeardownFcb) != NULL) {

            xixfs_TeardownStructures( CanWait, *(IrpContext->TeardownFcb), FALSE, &RemovedFcb );

            if (!RemovedFcb) {

                XifsdReleaseFcb( IrpContext, *(IrpContext->TeardownFcb) );
            }

            *(IrpContext->TeardownFcb) = NULL;
            IrpContext->TeardownFcb = NULL;
        }

        break;

    //
    //  We need to lock the user's buffer, unless this is an MDL-read,
    //  in which case there is no user buffer.
    //

    case IRP_MJ_READ :

        if (!FlagOn( IrpContext->MinorFunction, IRP_MN_MDL )) {

            xixfs_PinCallersBuffer( Irp, IoWriteAccess, IrpSp->Parameters.Read.Length );
        }

        break;
        
    case IRP_MJ_WRITE :

        xixfs_PinCallersBuffer( Irp, IoReadAccess, IrpSp->Parameters.Write.Length );
        break;

    //
    //  We also need to check whether this is a query file operation.
    //
    
    case IRP_MJ_DIRECTORY_CONTROL :

        if (IrpContext->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

            xixfs_PinCallersBuffer( Irp, IoWriteAccess, IrpSp->Parameters.QueryDirectory.Length );
        }

        break;
    }
    //
    //  Cleanup the IrpContext for the post.
    //
	/*
    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_MORE_PROCESSING );
    UdfCleanupIrpContext( IrpContext, TRUE );
	*/
    //
    //  Mark the Irp to show that we've already returned pending to the user.
    //

    IoMarkIrpPending( Irp );
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit  xixfs_PrePostIrp\n"));
    return;
}


VOID
xixfs_OplockComplete(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
)
{
	BOOLEAN RemovedFcb = FALSE;	
	BOOLEAN CanWait = FALSE;

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter  xixfs_OplockComplete\n"));

	CanWait = XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	//
	//  Check on the return value in the Irp.  If success then we
	//  are to post this request.
	//

	if (Irp->IoStatus.Status == STATUS_SUCCESS) {

		//
		//  Check if there is any cleanup work to do.
		//

		switch (IrpContext->MajorFunction) {

			case IRP_MJ_CREATE :

			//
			//  If called from the oplock package then there is an
			//  Fcb to possibly teardown.  We will call the teardown
			//  routine and release the Fcb if still present.  The cleanup
			//  code in create will know not to release this Fcb because
			//  we will clear the pointer.
			//

				if (IrpContext->TeardownFcb != NULL) {

					xixfs_TeardownStructures( CanWait, *(IrpContext->TeardownFcb), FALSE, &RemovedFcb );

						if (!RemovedFcb) {

							XifsdReleaseFcb( IrpContext, *(IrpContext->TeardownFcb) );
						}

					*(IrpContext->TeardownFcb) = NULL;
					IrpContext->TeardownFcb = NULL;
				}

			break;
		}
		//
		//  Insert the Irp context in the workqueue.
		//

		xixfs_AddToWorkque( IrpContext, Irp );

		//
		//  Otherwise complete the request.
		//

	} else {

		xixfs_CompleteRequest( IrpContext, Irp->IoStatus.Status, 0 );
	}
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit  xixfs_OplockComplete\n"));
	return;
}


FAST_IO_POSSIBLE
xixfs_CheckFastIoPossible(
	IN PXIXFS_FCB pFCB
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_CheckFastIoPossible\n"));
	
	if(pFCB->XixcoreFcb.HasLock == FCB_FILE_LOCK_OTHER_HAS){
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
			("Exit xixfs_CheckFastIoPossible: FastIoIsQuestionable\n"));
		return FastIoIsQuestionable;
	}

	if((pFCB->FCBFileLock == NULL) || !FsRtlAreThereCurrentFileLocks(pFCB->FCBFileLock)) {
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
			("Exit xixfs_CheckFastIoPossible: FastIoIsPossible\n"));
		return FastIoIsPossible;
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_CheckFastIoPossible: FastIoIsQuestionable\n"));
	return FastIoIsQuestionable;
}


NTSTATUS
xixfs_CheckFCBAccess(
	IN PXIXFS_FCB	pFCB,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN uint32	RequestDeposition,
	IN ACCESS_MASK	DesiredAccess
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_CheckFCBAccess\n"));



	if(TypeOfOpen == UserVolumeOpen){
		if(XIXCORE_TEST_FLAGS(DesiredAccess, (FILE_DELETE_CHILD | DELETE|WRITE_DAC)) ){
			return STATUS_ACCESS_DENIED;
		}	

		//	Added by ILGU HONG for readonly 09052006
		if(XIXCORE_TEST_FLAGS(DesiredAccess, 
			(FILE_WRITE_ATTRIBUTES| FILE_WRITE_DATA| FILE_WRITE_EA|FILE_APPEND_DATA)) ){

					
				if(pFCB->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("xixfs_CheckFCBAccess Fail Is read only volume .\n"));
						
						return STATUS_ACCESS_DENIED;
					}
					
		}
		//	Added by ILGU HONG for readonly end	

	}else {
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){

			if((RequestDeposition == FILE_OVERWRITE) || 
				(RequestDeposition == FILE_OVERWRITE_IF) ||
				(RequestDeposition == FILE_SUPERSEDE)){
				if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
					DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						(" xixfs_CheckFCBAccess FAIL OVERWRITE!!!\n"));

					//	Added by ILGU HONG for readonly 09052006
					if(pFCB->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("xixfs_CheckFCBAccess Fail Is read only volume .\n"));
						
						return STATUS_ACCESS_DENIED;
					}
					//	Added by ILGU HONG for readonly end	
					//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
					RC = xixcore_LotLock(
							&(pFCB->PtrVCB->XixcoreVcb), 
							pFCB->XixcoreFcb.LotNumber, 
							&(pFCB->XixcoreFcb.HasLock),
							1,
							0);

					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail has not host lock .\n"));
						
						return STATUS_ACCESS_DENIED;
					}

					XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_WRITE);
				}
			}

			if(XIXCORE_TEST_FLAGS(DesiredAccess, 
				(FILE_WRITE_ATTRIBUTES| FILE_WRITE_DATA| FILE_WRITE_EA|FILE_APPEND_DATA)) 
				|| DeleteOnCloseSpecified ){
				

				//	Added by ILGU HONG for readonly 09052006
					if(pFCB->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
					if(DeleteOnCloseSpecified){ //fake: write open support
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("xixfs_CheckFCBAccess Fail Is read only volume .\n"));
						
						return STATUS_ACCESS_DENIED;
					}

				}else {
				//	Added by ILGU HONG for readonly end	


					if(DeleteOnCloseSpecified || XIXCORE_TEST_FLAGS(DesiredAccess, FILE_WRITE_DATA) ){
						//XIXCORE_SET_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);

						if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){

							if(pFCB->SectionObject.ImageSectionObject != NULL){

								if (!MmFlushImageSection( &pFCB->SectionObject, MmFlushForWrite )) {

									return (DeleteOnCloseSpecified ? STATUS_CANNOT_DELETE : STATUS_SHARING_VIOLATION);
								}
							}

						}

					}


					if(pFCB->XixcoreFcb.HasLock == FCB_FILE_LOCK_HAS){
						XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_WRITE);
						return STATUS_SUCCESS;
					}

					//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
					RC = xixcore_LotLock(
							&(pFCB->PtrVCB->XixcoreVcb), 
							pFCB->XixcoreFcb.LotNumber, 
							&(pFCB->XixcoreFcb.HasLock),
							1,
							0);

					if(!NT_SUCCESS(RC) && DeleteOnCloseSpecified){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail has not host lock .\n"));
						return STATUS_ACCESS_DENIED;
					}


					if(NT_SUCCESS(RC)){
						XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_WRITE);
					}
					// Added End
				}


			}
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_CheckFCBAccess\n"));
	return STATUS_SUCCESS;
}



NTSTATUS
xixfs_FileOverWrite(
	IN PXIXFS_VCB pVCB, 
	IN PXIXFS_FCB pFCB,
	IN PFILE_OBJECT pFileObject
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	LARGE_INTEGER Size = {0,0};
	uint32 NotifyFilter = 0;

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FileOverWrite\n"));

	ASSERT_VCB(pVCB);
	ASSERT_FCB(pFCB);


	if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Fail xixfs_FileOverWrite Has No Lock\n"));

		return STATUS_INVALID_PARAMETER;
	}

	InterlockedIncrement( &pFCB->FCBReference );
	try{
		
		
        NotifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE
                       | FILE_NOTIFY_CHANGE_ATTRIBUTES
                       | FILE_NOTIFY_CHANGE_SIZE;		

		pFileObject->SectionObjectPointer = &pFCB->SectionObject;
		
		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
			if(!MmCanFileBeTruncated(&pFCB->SectionObject, &Size)){
				RC = STATUS_USER_MAPPED_FILE;
				try_return(RC);
			}

			CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
		}



		
		ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, TRUE);
		

		pFCB->FileSize.QuadPart = 0;
		pFCB->ValidDataLength.QuadPart = 0;

		pFCB->XixcoreFcb.FileAttribute |= FILE_ATTRIBUTE_ARCHIVE;
	
		CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
		//DbgPrint(" 11 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
		
	
		ExReleaseResourceLite(pFCB->PagingIoResource);


		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		
		xixfs_NotifyReportChangeToXixfs(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);

	}finally{
		InterlockedDecrement( &pFCB->FCBReference );
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FileOverWrite %x\n", RC));
	return RC;
}




NTSTATUS
xixfs_ProcessOpenFCB(
	IN PXIXFS_IRPCONTEXT	IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIXFS_VCB	pVCB,
	IN OUT PXIXFS_FCB * ppCurrentFCB,
	IN PXIXFS_LCB	pLCB OPTIONAL,
	IN TYPE_OF_OPEN	TypeOfOpen,
	IN uint32		RequestDeposition,
	IN ACCESS_MASK	DesiredAccess,
	IN uint32		CCBFlags,
	IN BOOLEAN		DeleteOnClose,
	IN BOOLEAN		bIgnoreCase,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
		
)
{
    NTSTATUS RC  = STATUS_SUCCESS;
    NTSTATUS OplockStatus = STATUS_SUCCESS;
    ULONG Information = FILE_OPENED;
    BOOLEAN LockVolume = FALSE;
	PXIXFS_FCB			pFCB = *ppCurrentFCB;
	PXIXFS_CCB			pCCB = NULL;
	PIRP				Irp = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_ProcessOpenFCB\n"));
	
	ASSERT_IRPCONTEXT(IrpContext);
	Irp = IrpContext->Irp;
	ASSERT(Irp);
	ASSERT_VCB(pVCB);
	

	if((TypeOfOpen <= UserVolumeOpen)
		&& !XIXCORE_TEST_FLAGS(IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_READ ))
	{
		if(pVCB->VCBCleanup != 0){
			Irp->IoStatus.Information = 0;
			return STATUS_SHARING_VIOLATION;
		}
	

		ASSERT(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

		LockVolume = TRUE;
		xixfs_RealCloseFCB(pVCB);
		RC = xixfs_PurgeVolume(IrpContext, pVCB, FALSE);

		if(!NT_SUCCESS(RC)){
			Irp->IoStatus.Information = 0;
			return RC;
		}

		if(pVCB->VCBUserReference > pVCB->VCBResidualUserReference){
			Irp->IoStatus.Information = 0;
			return STATUS_SHARING_VIOLATION;
		}

	}

	try{
		if(pFCB->FCBCleanup != 0){
			if(TypeOfOpen == UserFileOpen){
					
				IrpContext->TeardownFcb = ppCurrentFCB;

				if(FsRtlCurrentBatchOplock(&pFCB->FCBOplock)){

					DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("CALL xixfs_ProcessOpenFCB : FsRtlCurrentBatchOplock\n"));
					OplockStatus = FsRtlCheckOplock( &pFCB->FCBOplock,
													 IrpContext->Irp,
													 IrpContext,
													 xixfs_OplockComplete,
													 xixfs_PrePostIrp );
					
					if (OplockStatus == STATUS_PENDING) {
						DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
							("return PENDING xixfs_ProcessOpenFCB : FsRtlCurrentBatchOplock\n"));
						Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
						try_return (RC = STATUS_PENDING);
					}		
				}
			
				
				if((RequestDeposition == FILE_OVERWRITE) || (RequestDeposition == FILE_OVERWRITE_IF))
				{
					XIXCORE_SET_FLAGS(DesiredAccess, FILE_WRITE_DATA);
				
				}else if(RequestDeposition == FILE_SUPERSEDE){

					XIXCORE_SET_FLAGS(DesiredAccess, DELETE);
				}


				RC = IoCheckShareAccess(DesiredAccess, 
										IrpSp->Parameters.Create.ShareAccess,
										IrpSp->FileObject,
										&pFCB->FCBShareAccess,
										FALSE );

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail xixfs_ProcessOpenFCB:IoCheckShareAccess Status(0x%x)\n", RC));

					Irp->IoStatus.Information = 0;
					try_return (RC);
				}

				DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("CALL xixfs_ProcessOpenFCB: FsRtlCheckOplock\n"));
				

				OplockStatus = FsRtlCheckOplock( &pFCB->FCBOplock,
												 IrpContext->Irp,
												 IrpContext,
												 xixfs_OplockComplete,
												 xixfs_PrePostIrp );
				
				if (OplockStatus == STATUS_PENDING) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("return PENDING xixfs_ProcessOpenFCB: FsRtlCheckOplock\n"));
					Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
					try_return (RC =  STATUS_PENDING);
				}		

				IrpContext->TeardownFcb = NULL;




				
				// Added by ILGU 04112006
				if( (XIXCORE_TEST_FLAGS(IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING))
					&& ( pFCB->FCBCleanup == pFCB->FcbNonCachedOpenCount )
					&& ( pFCB->SectionObject.DataSectionObject != NULL)
				) 	
				{

					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("FLUSH Previous cache\n"));

					//DbgPrint("FLUSH Previous cache\n");
					
					CcFlushCache( &(pFCB->SectionObject), NULL, 0, NULL);
					//DbgPrint("CcFlush  4 File(%wZ)\n", &pFCB->FCBFullPath);
					ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
					ExReleaseResourceLite( pFCB->PagingIoResource );
					CcPurgeCacheSection(&(pFCB->SectionObject), NULL, 0, FALSE);
				}
				// Added end
				
				/*	
				if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
					if((RequestDeposition == FILE_OPEN) || (RequestDeposition == FILE_OPEN_IF)){
						DbgPrint("Read Only FLUSH Previous cache\n");

						CcFlushCache( &(pFCB->SectionObject), NULL, 0, NULL);
						DbgPrint("CcFlush  4 File(%wZ)\n", &pFCB->FCBFullPath);
						ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
						ExReleaseResourceLite( pFCB->PagingIoResource );
						CcPurgeCacheSection(&(pFCB->SectionObject), NULL, 0, FALSE);						
					}
				}
				*/


			}else{
				RC = IoCheckShareAccess(DesiredAccess, 
										IrpSp->Parameters.Create.ShareAccess,
										IrpSp->FileObject,
										&pFCB->FCBShareAccess,
										FALSE );

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail xixfs_ProcessOpenFCB:IoCheckShareAccess Status(0x%x)\n", RC));

					Irp->IoStatus.Information = 0;
					try_return (RC);
				}
			}
		}


		/*
		// Added by ILGU HONG
		if( (pFCB->FCBType == FCB_TYPE_FILE)
			&& (pFCB->HasLock != FCB_FILE_LOCK_HAS)
			&& ( pFCB->FCBCleanup == 0 )
			&& ((RequestDeposition == FILE_OPEN) || (RequestDeposition == FILE_OPEN_IF))
			&& ( pFCB->SectionObject.DataSectionObject != NULL)
		) 		
		{

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("FLUSH Previous  Read Only File cache\n"));

			DbgPrint("PURGE Previous  Read Only File cache\n");
			CcPurgeCacheSection(&(pFCB->SectionObject), NULL, 0, FALSE);
		}
		// Added by ILGU HONG END
		*/



		if((RequestDeposition == FILE_OVERWRITE) 
			|| (RequestDeposition == FILE_OVERWRITE_IF)
			|| (RequestDeposition == FILE_SUPERSEDE) )
		{

			if(TypeOfOpen == UserFileOpen)
			{
				RC = xixfs_FileOverWrite(pVCB, pFCB, IrpSp->FileObject);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail xixfs_ProcessOpenFCB:xixfs_FileOverWrite Status(0x%x)\n", RC));

					Irp->IoStatus.Information = 0;
					try_return (RC);
				}
				if(RequestDeposition == FILE_SUPERSEDE){
					Information = FILE_SUPERSEDED;
				}else{
					Information = FILE_OVERWRITTEN;
				}
				
			}
		}



		pCCB = xixfs_CreateCCB(IrpContext,IrpSp->FileObject, pFCB, pLCB, CCBFlags);
		
		ASSERT_CCB(pCCB);


		if(DeleteOnClose){
			XIXCORE_SET_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_FLAGS_DELETE_ON_CLOSE);
		}


		if(bIgnoreCase){
			XIXCORE_SET_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_FLAGS_IGNORE_CASE);
		}


		if( !XIXCORE_TEST_FLAGS(CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID)
			&& (AbsolutePath != NULL))
		{
			if(pFCB->FCBFullPath.Buffer == NULL){
				uint32			FullPathSize = 0;
				FullPathSize = SECTORALIGNSIZE_512(AbsolutePath->Length);

				pFCB->FCBFullPath.Buffer = ExAllocatePoolWithTag(NonPagedPool,FullPathSize, TAG_FILE_NAME);
				ASSERT(pFCB->FCBFullPath.Buffer);
				pFCB->FCBFullPath.MaximumLength = (uint16)FullPathSize; 
				pFCB->FCBFullPath.Length = AbsolutePath->Length;
				RtlCopyMemory(pFCB->FCBFullPath.Buffer, AbsolutePath->Buffer, pFCB->FCBFullPath.Length);

				if(AbsolutePath->Length == 2){
					pFCB->FCBTargetOffset = 0;
				}else{
					pFCB->FCBTargetOffset = xixfs_SearchLastComponetOffsetOfName(&pFCB->FCBFullPath);
				}

			}
		}



		if (pFCB->FCBCleanup == 0) {

			IoSetShareAccess( DesiredAccess,
							  IrpSp->Parameters.Create.ShareAccess,
							  IrpSp->FileObject,
							  &pFCB->FCBShareAccess );

		} else {

			IoUpdateShareAccess( IrpSp->FileObject, &pFCB->FCBShareAccess );
		}


	}finally{
		/*
		if(!NT_SUCCESS(RC)){
			return RC;
		}
		*/
	}
	
	if(!NT_SUCCESS(RC)){
		return RC;
	}

	

	
	//
	//  Set the file object type.
	//

	xixfs_SetFileObject( IrpContext, IrpSp->FileObject, TypeOfOpen, pFCB, pCCB );
	




	//
	//  Set the appropriate cache flags for a user file object.
	//

	if (TypeOfOpen == UserFileOpen) {

		if (XIXCORE_TEST_FLAGS( IrpSp->Parameters.Create.Options, FILE_NO_INTERMEDIATE_BUFFERING )) {
			XIXCORE_CLEAR_FLAGS( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
			XIXCORE_SET_FLAGS( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING );		

		} else {

			XIXCORE_SET_FLAGS( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
		}
	}else if (TypeOfOpen == UserVolumeOpen)  {

		//
		//  DASD access is always noncached
		//

		XIXCORE_SET_FLAGS( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING );
	}	
	
	XifsdLockVcb(IrpContext, pVCB);
	
	if( XIXCORE_TEST_FLAGS(IrpSp->FileObject->Flags, FILE_NO_INTERMEDIATE_BUFFERING)	)
	{
		pFCB->FcbNonCachedOpenCount ++;
	}

	XifsdIncCleanUpCount(pFCB);
	XifsdIncCloseCount(pFCB);
	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
			("Create FCB LotNumber (%I64d) FCBCleanUp(%ld) VCBCleanup(%ld)\n", 
			pFCB->XixcoreFcb.LotNumber, pFCB->FCBCleanup, pVCB->VCBCleanup));

	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL, 
			("!!!!CREATE pCCB(%p) pFileObject(%p)\n", pCCB, IrpSp->FileObject));



	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("xixfs_ProcessOpenFCB LotNumber(%I64d) VCB (%d/%d) FCB (%d/%d)\n",
					 pFCB->XixcoreFcb.LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));	
	
	XifsdIncRefCount(pFCB, 1, 1);

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("xixfs_ProcessOpenFCB LotNumber(%I64d) VCB (%d/%d) FCB (%d/%d)\n",
					 pFCB->XixcoreFcb.LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));	

	/*
	DbgPrint("Open CCB with  VCB (%d/%d) FCB:(%I64d) (%d/%d) TypeOfOpen(%ld)\n",
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->XixcoreFcb.LotNumber,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference,
					 TypeOfOpen);	
	*/

   if (LockVolume) {

        pVCB->LockVolumeFileObject = IrpSp->FileObject;
        XIXCORE_SET_FLAGS( pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED );
    }

    XifsdUnlockVcb( IrpContext, pVCB );

    XifsdLockFcb(IrpContext, pFCB);

	try{
		if (TypeOfOpen == UserFileOpen) {

			pFCB->IsFastIoPossible = xixfs_CheckFastIoPossible(pFCB);

		} else {

			pFCB->IsFastIoPossible = FastIoIsNotPossible;
		}

		pFCB->XixcoreFcb.DesiredAccess = DesiredAccess;

	}finally{
		;
	}

  
    XifsdUnlockFcb( IrpContext, pFCB );
	
    //
    //  Show that we opened the file.
    //

    IrpContext->Irp->IoStatus.Information = Information;

    //
    //  Point to the section object pointer in the non-paged Fcb.
    //

    IrpSp->FileObject->SectionObjectPointer = &pFCB->SectionObject;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_ProcessOpenFCB\n"));
    return STATUS_SUCCESS;

}




NTSTATUS
xixfs_OpenExistingFCB(
	IN PXIXFS_IRPCONTEXT	IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN OUT PXIXFS_FCB * ppCurrentFCB,
	IN PXIXFS_LCB	pLCB OPTIONAL,
	IN TYPE_OF_OPEN	TypeOfOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIXFS_CCB	pRelatedCCB OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
)
{

	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	uint32	CCBFlags = 0;
	PIRP	Irp = NULL;
	

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_OpenExistingFCB\n"));

	ASSERT_IRPCONTEXT(IrpContext);
	Irp = IrpContext->Irp;
	ASSERT(Irp);
	ASSERT_EXCLUSIVE_FCB(*ppCurrentFCB);

	/*
	if( (TypeOfOpen == UserFileOpen)
		&& (IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING) )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Try to OPENT  FO_NO_INTERMEDIATE_BUFFERING.\n"));	
		RC = STATUS_INVALID_PARAMETER ;
		Irp->IoStatus.Information = 0;
		return RC;			
	}
	*/

	RC = xixfs_CheckFCBAccess(
			*ppCurrentFCB,
			TypeOfOpen,
			DeleteOnCloseSpecified,
			RequestDeposition,
			IrpSp->Parameters.Create.SecurityContext->DesiredAccess);

	if(!NT_SUCCESS(RC)){
		Irp->IoStatus.Information =0;
		return STATUS_ACCESS_DENIED;
	}
	
	if(ARGUMENT_PRESENT(pRelatedCCB)){
		if(XIXCORE_TEST_FLAGS(pRelatedCCB->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID) )
		{
			XIXCORE_SET_FLAGS(CCBFlags,XIXFSD_CCB_OPENED_BY_FILEID);
		}
	}



	


	RC = xixfs_ProcessOpenFCB(IrpContext,
							IrpSp,
							(*ppCurrentFCB)->PtrVCB,
							ppCurrentFCB,
							pLCB,
							TypeOfOpen,
							RequestDeposition,
							IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
							CCBFlags,
							DeleteOnCloseSpecified,
							bIgnoreCase,
							AbsolutePath
							);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_OpenExistingFCB\n"));
	return RC;
}



NTSTATUS
xixfs_OpenByFileId(
	IN PXIXFS_IRPCONTEXT		IrpContext,
	IN PIO_STACK_LOCATION	IrpSp,
	IN PXIXFS_VCB			pVCB,
	IN BOOLEAN				DeleteOnCloseSpecified,
	IN BOOLEAN				bIgnoreCase,
	IN BOOLEAN				DirectoryOnlyRequested,
	IN BOOLEAN				FileOnlyRequested,
	IN uint32				RequestDeposition,
	IN OUT PXIXFS_FCB		*ppFCB
)
{
	NTSTATUS RC = STATUS_ACCESS_DENIED;
	BOOLEAN UnlockVcb = FALSE;
	BOOLEAN Found = FALSE;
	BOOLEAN FcbExisted =FALSE;
	ACCESS_MASK DesiredAccess = 0;
	uint32	NodeTypeCode = 0;
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;
	uint64	FileId = 0;
	uint64	LotNumber = 0;
	uint32	FCB_TYPE_CODE = 0;
	PXIXFS_FCB NextFcb = NULL;
	PXIXFS_FCB ParentFcb = NULL;
	PXIXFS_LCB OpenLcb = NULL;

	BOOLEAN		bAcquireFcb = FALSE;
	BOOLEAN		bAcquireParentFcb = FALSE;

	PIRP		Irp = NULL;
	UNICODE_STRING		FileName;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XifsdOpenByFileId\n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	Irp = IrpContext->Irp;
	ASSERT(Irp);
	ASSERT_VCB( pVCB );


	//
	//  Extract the FileId from the FileObject.
	//

	RtlCopyMemory( &FileId, IrpSp->FileObject->FileName.Buffer, sizeof(uint64));


	DesiredAccess = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;

	if (XIFSD_TYPE_DIR( FileId )) {

		TypeOfOpen = UserDirectoryOpen;
		FCB_TYPE_CODE = FCB_TYPE_DIR;

	} else {

		TypeOfOpen = UserFileOpen;
		FCB_TYPE_CODE = FCB_TYPE_FILE;
	}
	
	// ADDED by ILGU 20060618
	if(TypeOfOpen == UserFileOpen){
		if(DirectoryOnlyRequested){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Invalid Parameter DirectoryOnlyRequested is  Set for File .\n"));

			RC = STATUS_INVALID_PARAMETER ;
			Irp->IoStatus.Information = 0;
			return RC;	
		}
	}


	if(TypeOfOpen == UserFileOpen)
	{
		// ADDED by ILGU 20060618
		if(FileOnlyRequested){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Invalid Parameter FileOnlyRequested is  Set for Directory .\n"));

			Irp->IoStatus.Information = 0;
			RC = STATUS_INVALID_PARAMETER ;
			return RC;
		}

		/*
		// ADDED by ILGU 20060618
		if(!DirectoryOnlyRequested){
			if(XIXCORE_TEST_FLAGS(DesiredAccess, FILE_LIST_DIRECTORY)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Invalid Parameter Access Directory for Read like file .\n"));

				Irp->IoStatus.Information = 0;
				RC = STATUS_ACCESS_DENIED ;
				return RC;
			}
			
		}
		*/
	}
	/*
	if( (TypeOfOpen == UserFileOpen)
		&& (IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING) )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Try to OPENT  FO_NO_INTERMEDIATE_BUFFERING.\n"));	
		RC = STATUS_INVALID_PARAMETER ;
		Irp->IoStatus.Information = 0;
		return RC;			
	}
	*/

	LotNumber = (FileId & FCB_ADDRESS_MASK);


	if( (LotNumber == pVCB->XixcoreVcb.RootDirectoryLotIndex)
			&& DeleteOnCloseSpecified)
	{

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Try to delete Root Dir (LotNumber %I64d) .\n", LotNumber));

		RC = STATUS_INVALID_PARAMETER ;
		Irp->IoStatus.Information = 0;
		return RC;	
	}

	//
	//  Use a try-finally to facilitate cleanup.
	//

	try {

		XifsdLockVcb( IrpContext, pVCB );
		UnlockVcb = TRUE;

		NextFcb = xixfs_CreateAndAllocFCB( pVCB, LotNumber, FCB_TYPE_CODE, &FcbExisted );

		//
		//  Now, if the Fcb was not already here we have some work to do.
		//
    
		if (!FcbExisted) 
		{

			//
			//  If we can't wait then post this request.
			//

			ASSERT(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

			//
			//  Use a try-finally to transform errors we get as a result of going
			//  off on a wild goose chase into a simple open failure.
			//

			try {

				NextFcb->FileId = FileId;

				RC = xixfs_InitializeFCB(NextFcb,TRUE);



			}except( xixfs_ExceptionFilter( IrpContext, GetExceptionInformation() )) {

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
				RC = STATUS_INVALID_PARAMETER;

			}

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XifsdInitializeFCBInfo .\n", RC));
				Irp->IoStatus.Information = 0;
				try_return(RC);				
			}


			//
			//  Do a little dance to leave the exception handler if we had problems.
			//

			if (RC == STATUS_INVALID_PARAMETER) {
				Irp->IoStatus.Information = 0;
				try_return( RC);
			}



			
		}
    
		//
		//  We have the Fcb.  Check that the type of the file is compatible with
		//  the desired type of file to open.
		//

		if (XIXCORE_TEST_FLAGS( NextFcb->XixcoreFcb.Type, FILE_ATTRIBUTE_DIRECTORY )) {

			if (XIXCORE_TEST_FLAGS( IrpSp->Parameters.Create.Options, FILE_NON_DIRECTORY_FILE )) {
				Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
				try_return( RC = STATUS_FILE_IS_A_DIRECTORY );
			}

		} else if (XIXCORE_TEST_FLAGS( IrpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE )) {
			Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
			try_return( RC = STATUS_NOT_A_DIRECTORY );
		}

		//
		//  We now know the Fcb and currently hold the Vcb lock.
		//  Try to acquire this Fcb without waiting.  Otherwise we
		//  need to reference it, drop the Vcb, acquire the Fcb, the
		//  Vcb and then dereference the Fcb.
		//

		XifsdUnlockVcb( IrpContext, pVCB );
		UnlockVcb = FALSE;

		XifsdAcquireFcbExclusive( TRUE, NextFcb, FALSE );
		//
		//  Move to this Fcb.
		//

		*ppFCB = NextFcb;



		// Added by ILGU HONG 05172006
		if (!FcbExisted)
		{
			ParentFcb = xixfs_FCBTLBLookupEntry(pVCB, NextFcb->XixcoreFcb.ParentLotNumber);

			if(ParentFcb == NULL){
				RC = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				try_return( RC);
			}

			XifsdReleaseFcb(TRUE, NextFcb);
			XifsdAcquireFcbExclusive(TRUE, ParentFcb, FALSE);
			bAcquireParentFcb = TRUE;
			XifsdAcquireFcbExclusive( TRUE, NextFcb, FALSE );
		
			XifsdLockVcb( IrpContext, pVCB );
			UnlockVcb = TRUE;

			FileName.Length = FileName.MaximumLength=(uint16)NextFcb->XixcoreFcb.FCBNameLength;
			FileName.Buffer = (PWSTR)NextFcb->XixcoreFcb.FCBName;

			OpenLcb = xixfs_FCBTLBInsertPrefix( IrpContext,
								   NextFcb,
								   &FileName,
								   ParentFcb );

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						("xixfs_OpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
							ParentFcb->XixcoreFcb.LotNumber,
							pVCB->VCBReference,
							pVCB->VCBUserReference,
							ParentFcb->FCBReference,
							ParentFcb->FCBUserReference ));

			XifsdIncRefCount( ParentFcb, 1, 1 );

			/*
			DbgPrint("Inc Link Count CCB with  VCB (%d/%d) ParentFCB (%d/%d) ChildPFB  xixfs_OpenByFileId\n",
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 ParentFcb->FCBReference,
					 ParentFcb->FCBUserReference
					 );	
			*/

			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						 ("xixfs_OpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d)  Vcb (%d/%d) Fcb (%d/%d)\n",
						 ParentFcb->XixcoreFcb.LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 ParentFcb->FCBReference,
						 ParentFcb->FCBUserReference ));

			XifsdUnlockVcb( IrpContext, pVCB );
			UnlockVcb = FALSE;
		}

		//
		//  Check the requested access on this Fcb.
		//

		RC = xixfs_CheckFCBAccess(
				NextFcb,
				TypeOfOpen,
				DeleteOnCloseSpecified,
				RequestDeposition,
				IrpSp->Parameters.Create.SecurityContext->DesiredAccess);

		if(!NT_SUCCESS(RC)){
			Irp->IoStatus.Information = 0;
			try_return( RC = STATUS_ACCESS_DENIED);
		}


	
		//
		//  Call our worker routine to complete the open.
		//

		RC = xixfs_ProcessOpenFCB( IrpContext,
									 IrpSp,
									 pVCB,
									 ppFCB,
									 NULL,
									 TypeOfOpen,
									 RequestDeposition,
									 IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
									 XIXFSD_CCB_OPENED_BY_FILEID,
									 DeleteOnCloseSpecified,
									 bIgnoreCase,
									 NULL);
		

	} finally {

		if (UnlockVcb) {

			XifsdUnlockVcb( IrpContext, pVCB );
		}

		
		if(bAcquireParentFcb){
			XifsdReleaseFcb(TRUE, ParentFcb);
		}


		//
		//  Destroy the new Fcb if it was not fully initialized.
		//

		if (NextFcb && !XIXCORE_TEST_FLAGS(NextFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_INIT)) {

//			DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));
			xixfs_DeleteFCB( TRUE , NextFcb );
		}

	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XifsdOpenByFileId\n"));
	return RC;
}



NTSTATUS
xixfs_OpenObjectFromDirContext (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIXFS_VCB Vcb,
	IN OUT PXIXFS_FCB *CurrentFcb,
	IN PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN PerformUserOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIXFS_CCB RelatedCcb OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
)
{
	ULONG CcbFlags = 0;
	uint64 FileId = 0;
	BOOLEAN UnlockVcb = FALSE;
	BOOLEAN FcbExisted;
	PXIXFS_FCB NextFcb = NULL;
	PXIXFS_FCB ParentFcb = NULL;
	PXIDISK_CHILD_INFORMATION pChild = NULL;
	TYPE_OF_OPEN	TypeOfOpen;
	uint32			NodeTypeCode;
	PXIXFS_LCB OpenLcb = NULL;
	PIRP	Irp = NULL;
	NTSTATUS RC;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_OpenObjectFromDirContext \n"));

	ASSERT_IRPCONTEXT(IrpContext);
	Irp = IrpContext->Irp;
	ASSERT(Irp);

	//
	//  Figure out what kind of open we will be performing here.  The caller has already insured
	//  that the user is expecting us to do this.
	//

	pChild = (PXIDISK_CHILD_INFORMATION)DirContext->ChildEntry;

	if (pChild->Type == XIFS_FD_TYPE_DIRECTORY) {

		TypeOfOpen = UserDirectoryOpen;
		NodeTypeCode = FCB_TYPE_DIR;

	} else {

		TypeOfOpen = UserFileOpen;
		NodeTypeCode = FCB_TYPE_FILE;
	}
	
	/*
	if( (TypeOfOpen == UserFileOpen)
		&& (IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING) 
		&& (PerformUserOpen) 
	)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Try to OPENT  FO_NO_INTERMEDIATE_BUFFERING.\n"));	
		RC = STATUS_INVALID_PARAMETER ;
		Irp->IoStatus.Information = 0;
		return RC;			
	}
	*/

	try {

		//
		//  Check the related Ccb to see if this was an OpenByFileId.
		//

		if(ARGUMENT_PRESENT(RelatedCcb)){
			if(XIXCORE_TEST_FLAGS(RelatedCcb->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID) )
			{
				XIXCORE_SET_FLAGS(CcbFlags,XIXFSD_CCB_OPENED_BY_FILEID);
			}
		}


		FileId = pChild->StartLotIndex;


		//
		//  Lock the Vcb so we can examine the Fcb Table.
		//

		XifsdLockVcb( IrpContext, Vcb );
		UnlockVcb = TRUE;

		//
		//  Get the Fcb for this file.
		//

		NextFcb = xixfs_CreateAndAllocFCB(Vcb, FileId, NodeTypeCode, &FcbExisted );

		//
		//  If the Fcb was created here then initialize from the values in the
		//  dirent.  We have optimistically assumed that there isn't any corrupt
		//  information to this point - we're about to discover it if there is.
		//

		if (!FcbExisted) {

   			//
			//  If we can't wait then post this request.
			//

			ASSERT(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

			//
			//  Use a try-finally to transform errors we get as a result of going
			//  off on a wild goose chase into a simple open failure.
			//

			try {

				RC = xixfs_InitializeFCB(NextFcb,TRUE);



			}except( xixfs_ExceptionFilter( IrpContext, GetExceptionInformation() )) {

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
				RC = STATUS_INVALID_PARAMETER;

			}


			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) xixfs_InitializeFCB .\n", RC));
				Irp->IoStatus.Information = 0;
				try_return(RC);				
			}
/*
			if (PerformUserOpen){		
				RC = XifsdCheckFCBAccess(
						NextFcb,
						TypeOfOpen,
						IrpSp->Parameters.Create.SecurityContext->DesiredAccess);

				
				if(!NT_SUCCESS(RC)){
					Irp->IoStatus.Information = 0;
					RC = STATUS_ACCESS_DENIED;
					try_return( RC);
				}

			}
*/

			//
			//  Do a little dance to leave the exception handler if we had problems.
			//

			if (RC == STATUS_INVALID_PARAMETER) {
				Irp->IoStatus.Information = 0;
				try_return( RC);
			}

		}

		XifsdUnlockVcb( IrpContext, Vcb );


		//
		//  Now try to acquire the new Fcb without waiting.  We will reference
		//  the Fcb and retry with wait if unsuccessful.
		//

		if (!XifsdAcquireFcbExclusive( TRUE, NextFcb, FALSE )) {

			NextFcb->FCBReference += 1;

			

			XifsdReleaseFcb( IrpContext, *CurrentFcb );
			XifsdAcquireFcbExclusive( TRUE, NextFcb, FALSE );
			XifsdAcquireFcbExclusive( TRUE, *CurrentFcb, FALSE );

		
			NextFcb->FCBReference -= 1;
		}

		XifsdLockVcb( IrpContext, Vcb );
		//
		//  Move down to this new Fcb.  Remember that we still own the parent however.
		//

		ParentFcb = *CurrentFcb;
		*CurrentFcb = NextFcb;

		//
		//  Store this name into the prefix table for the parent.
		//



		//
		//  Now increment the reference counts for the parent and drop the Vcb.
		//

		if(!FcbExisted) 
		{
			UNICODE_STRING	ChildName;
			ChildName.Length = ChildName.MaximumLength = (uint16)DirContext->ChildNameLength;
			ChildName.Buffer = (PWSTR)DirContext->ChildName;

			OpenLcb = xixfs_FCBTLBInsertPrefix( IrpContext,
								   NextFcb,
								   &ChildName,
								   ParentFcb );

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						("xixfs_OpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
							ParentFcb->XixcoreFcb.LotNumber,
							Vcb->VCBReference,
							Vcb->VCBUserReference,
							ParentFcb->FCBReference,
							ParentFcb->FCBUserReference ));

			XifsdIncRefCount( ParentFcb, 1, 1 );

			/*
			DbgPrint("Inc Link Count CCB with  VCB (%d/%d) ParentFCB: (%d/%d) ChildPFB:  xixfs_OpenObjectFromDirContext\n",
					 Vcb->VCBReference,
					 Vcb->VCBUserReference,
					 ParentFcb->FCBReference,
					 ParentFcb->FCBUserReference
					 );	
			*/

			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						 ("xixfs_OpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d)  Vcb (%d/%d) Fcb (%d/%d)\n",
						 ParentFcb->XixcoreFcb.LotNumber,
						 Vcb->VCBReference,
						 Vcb->VCBUserReference,
						 ParentFcb->FCBReference,
						 ParentFcb->FCBUserReference ));
		}



		XifsdUnlockVcb( IrpContext, Vcb );
		UnlockVcb = FALSE;

		//
		//  Perform initialization associated with the directory context.
		//

		xixfs_InitializeLcbFromDirContext( IrpContext,
										OpenLcb);

		//
		//  If we just opened VIDEO_TS directory,  on a UDF1.02 file system,
		//  then mark the Fcb to allow the >=1Gb single AD workaround
		//  to be used on it's children (works around some corrupt DVD-Videos)
		//


		//
		//  Release the parent Fcb at this point.
		//

		XifsdReleaseFcb( IrpContext, ParentFcb );
		ParentFcb = NULL;

		RC = STATUS_SUCCESS;

		//
		//  Call our worker routine to complete the open.
		//

		if (PerformUserOpen) {

			//if((*CurrentFcb)->FCBCleanup == 0){
				RC = xixfs_CheckFCBAccess(
						NextFcb,
						TypeOfOpen,
						DeleteOnCloseSpecified,
						RequestDeposition,
						IrpSp->Parameters.Create.SecurityContext->DesiredAccess);

				
				if(!NT_SUCCESS(RC)){
					Irp->IoStatus.Information = 0;
					RC = STATUS_ACCESS_DENIED;
					try_return( RC);
				}
			//}

			RC = xixfs_ProcessOpenFCB(IrpContext,
										IrpSp,
										Vcb,
										CurrentFcb,
										OpenLcb,
										TypeOfOpen,
										RequestDeposition,
										IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
										CcbFlags,
										DeleteOnCloseSpecified,
										bIgnoreCase,
										AbsolutePath);

		}

	} finally {

		//
		//  Unlock the Vcb if held.
		//

		if (UnlockVcb) {

			XifsdUnlockVcb( IrpContext, Vcb );
		}

		//
		//  Release the parent if held.
		//

		if (ParentFcb != NULL) {

			XifsdReleaseFcb( IrpContext, ParentFcb );
		}

		//
		//  Destroy the new Fcb if it was not fully initialized.
		//

		if (NextFcb && !XIXCORE_TEST_FLAGS( NextFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_INIT )) {
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
								("NewFcb->FCBFlags (0x%x)\n", NextFcb->XixcoreFcb.FCBFlags));
			xixfs_DeleteFCB( TRUE, NextFcb );
		}

	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_OpenObjectFromDirContext \n"));
	return RC;
}




NTSTATUS
xixfs_OpenNewFileObject(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIXFS_VCB Vcb,
	IN PXIXFS_FCB *CurrentFCB, //ParentFcb,
	IN uint64	OpenFileId,
	IN uint32	OpenFileType,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN BOOLEAN	bIgnoreCase,
	IN PUNICODE_STRING OpenFileName,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
)
{
	ULONG CcbFlags = 0;
	uint64 FileId = 0;

	BOOLEAN UnlockVcb = FALSE;
	BOOLEAN FcbExisted;
	PXIXFS_FCB ParentFcb = NULL;
	PXIXFS_FCB NextFcb = NULL;

	TYPE_OF_OPEN	TypeOfOpen;
	uint32			NodeTypeCode;

	PXIXFS_LCB OpenLcb;

	NTSTATUS RC;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_OpenNewFileObject \n"));


	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT_VCB(Vcb);
	ASSERT_FCB(*CurrentFCB);

	if (OpenFileType == XIFS_FD_TYPE_DIRECTORY) {

		TypeOfOpen = UserDirectoryOpen;
		NodeTypeCode = FCB_TYPE_DIR;

	} else {

		TypeOfOpen = UserFileOpen;
		NodeTypeCode = FCB_TYPE_FILE;
	}
	


	try {

		//
		//  Check the related Ccb to see if this was an OpenByFileId.
		//

	
		XIXCORE_SET_FLAGS(CcbFlags,XIXFSD_CCB_OPENED_AS_FILE);
	

		FileId = OpenFileId;


		//
		//  Lock the Vcb so we can examine the Fcb Table.
		//

		XifsdLockVcb( IrpContext, Vcb );
		UnlockVcb = TRUE;

		//
		//  Get the Fcb for this file.
		//

		NextFcb = xixfs_CreateAndAllocFCB(Vcb, FileId, NodeTypeCode, &FcbExisted );
//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("1 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					 ("1 xixfs_OpenNewFileObject, Vcb %d/%d Fcb %d/%d\n",
					 Vcb->VCBReference,
					 Vcb->VCBUserReference,
					 NextFcb->FCBReference,
					 NextFcb->FCBUserReference ));

		//
		//  If the Fcb was created here then initialize from the values in the
		//  dirent.  We have optimistically assumed that there isn't any corrupt
		//  information to this point - we're about to discover it if there is.
		//

		if (!FcbExisted) {

   			//
			//  If we can't wait then post this request.
			//
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					 ("Create New FCB \n"));

			ASSERT(XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

			//
			//  Use a try-finally to transform errors we get as a result of going
			//  off on a wild goose chase into a simple open failure.
			//

			try {

				RC = xixfs_InitializeFCB(NextFcb,TRUE);
//				DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("2 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));

				

			}except( xixfs_ExceptionFilter( IrpContext, GetExceptionInformation() )) {

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Exception!!\n"));
					RC = STATUS_INVALID_PARAMETER;

			}

			if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) XifsdInitializeFCBInfo .\n", RC));
					try_return(RC);				
			}
			
			RC = xixfs_CheckFCBAccess(
					NextFcb,
					TypeOfOpen,
					DeleteOnCloseSpecified,
					FILE_CREATE,
					IrpSp->Parameters.Create.SecurityContext->DesiredAccess);

			
			
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) XifsdCheckFCBAccess .\n", RC));
				RC = STATUS_ACCESS_DENIED;
				try_return( RC);
			}


			//
			//  Do a little dance to leave the exception handler if we had problems.
			//

			if (RC == STATUS_INVALID_PARAMETER) {

				try_return( RC);
			}

		}
//			DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("3 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));
		//
		//  Now try to acquire the new Fcb without waiting.  We will reference
		//  the Fcb and retry with wait if unsuccessful.
		//

		XifsdUnlockVcb( IrpContext, Vcb );
		if (!XifsdAcquireFcbExclusive( TRUE, NextFcb, FALSE )) {

			NextFcb->FCBReference += 1;

			XifsdReleaseFcb( IrpContext, *CurrentFCB );
			XifsdAcquireFcbExclusive( TRUE, NextFcb, FALSE );
			XifsdAcquireFcbExclusive( TRUE, *CurrentFCB, FALSE );

		
			NextFcb->FCBReference -= 1;
		}

		XifsdLockVcb( IrpContext, Vcb );
		ParentFcb = *CurrentFCB;
		*CurrentFCB = NextFcb;
		//
		//  Store this name into the prefix table for the parent.
		//

		OpenLcb = xixfs_FCBTLBInsertPrefix( IrpContext,
								   NextFcb,
								   OpenFileName,
								   ParentFcb );

//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("4 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));
		//
		//  Now increment the reference counts for the parent and drop the Vcb.
		//

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					("xixfs_OpenNewFileObject IncFarentFCB LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
						ParentFcb->XixcoreFcb.LotNumber,
						Vcb->VCBReference,
						Vcb->VCBUserReference,
						ParentFcb->FCBReference,
						ParentFcb->FCBUserReference ));

		XifsdIncRefCount( ParentFcb, 1, 1 );

		/*
		DbgPrint("Inc Link Count CCB with  VCB (%d/%d) ParentFCB (%d/%d) ChildPFB  xixfs_OpenNewFileObject\n",
				 Vcb->VCBReference,
				 Vcb->VCBUserReference,
				 ParentFcb->FCBReference,
				 ParentFcb->FCBUserReference
				 );
		*/

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("xixfs_OpenNewFileObject,IncFarentFCB LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n",
						ParentFcb->XixcoreFcb.LotNumber,
						Vcb->VCBReference,
						Vcb->VCBUserReference,
						ParentFcb->FCBReference,
						ParentFcb->FCBUserReference ));

		XifsdUnlockVcb( IrpContext, Vcb );
		UnlockVcb = FALSE;




		//
		//  Perform initialization associated with the directory context.
		//

		xixfs_InitializeLcbFromDirContext( IrpContext,
										OpenLcb);

		//
		//  If we just opened VIDEO_TS directory,  on a UDF1.02 file system,
		//  then mark the Fcb to allow the >=1Gb single AD workaround
		//  to be used on it's children (works around some corrupt DVD-Videos)
		//

		//
		//  Release the parent Fcb at this point.
		//

		XifsdReleaseFcb( IrpContext, ParentFcb );
		ParentFcb = NULL;
	
		//
		//  Call our worker routine to complete the open.
		//

//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//					("Create File 6\n"));
//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));

		RC = xixfs_ProcessOpenFCB(IrpContext,
									IrpSp,
									Vcb,
									&NextFcb,
									OpenLcb,
									TypeOfOpen,
									FILE_CREATE,
									IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
									CcbFlags,
									DeleteOnCloseSpecified,
									bIgnoreCase,
									AbsolutePath);
//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//					("Create File 7\n"));
//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));


	} finally {

		//
		//  Unlock the Vcb if held.
		//

		if (UnlockVcb) {

			XifsdUnlockVcb( IrpContext, Vcb );
		}

		//
		//  Release the parent if held.
		//

		if (ParentFcb != NULL) {

			XifsdReleaseFcb( IrpContext, ParentFcb );
		}

		//
		//  Destroy the new Fcb if it was not fully initialized.
		//

		if (NextFcb && !XIXCORE_TEST_FLAGS(NextFcb->XixcoreFcb.FCBFlags,XIXCORE_FCB_INIT)) {

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("NewFcb->FCBFlags (0x%x)\n", NextFcb->XixcoreFcb.FCBFlags));

			xixfs_DeleteFCB( TRUE, NextFcb );
		}

	}
	
	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
		("Exit xixfs_OpenNewFileObject RC(0x%x) \n", RC));
	return RC;
}





/*
 *	Used for directory suffing. << FILE/DIRECOTRY >>
 */

NTSTATUS
xixfs_InitializeFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_InitializeFileContext .\n"));

	RtlZeroMemory(FileContext, sizeof(XIFS_FILE_EMUL_CONTEXT));
	FileContext->Buffer =xixcore_AllocateBuffer(XIDISK_FILE_HEADER_LOT_SIZE); 
	if(!FileContext->Buffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail XifsdInitializeFileContext.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_InitializeFileContext .\n"));

	return STATUS_SUCCESS;
}


NTSTATUS
xixfs_SetFileContext(
	PXIXFS_VCB	pVCB,
	uint64	LotNumber,
	uint32	FileType,
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{
	NTSTATUS	RC = STATUS_SUCCESS;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_SetFileContext .\n"));


	FileContext->pVCB = pVCB;
	FileContext->LotNumber = LotNumber;
	FileContext->FileType = FileType;
	FileContext->pSearchedFCB = NULL;
	try{
		if(FileContext->Buffer == NULL){
			FileContext->Buffer = 
				xixcore_AllocateBuffer(XIDISK_FILE_HEADER_LOT_SIZE); 
		}
	}finally{
		if(AbnormalTermination()){
			RC = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_SetFileContext .\n"));

	return RC;
}


NTSTATUS
xixfs_ClearFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{

	NTSTATUS	RC = STATUS_SUCCESS;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_ClearFileContext .\n"));

	try{
		if(FileContext->Buffer){
			xixcore_FreeBuffer(FileContext->Buffer);
			FileContext->Buffer = NULL;
		}
	}finally{
		if(AbnormalTermination()){
			RC = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_ClearFileContext .\n"));
	return RC;
}



NTSTATUS
xixfs_FileInfoFromContext(
	BOOLEAN					Waitable,
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_FILE_HEADER_LOT pFileHeader = NULL;
	PXIXFS_VCB	pVCB = NULL;
	LARGE_INTEGER	Offset;
	uint32	BlockSize;
	uint32	LotType = LOT_INFO_TYPE_INVALID;
	uint32	Reason = 0;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FileInfoFromContext.\n"));

	ASSERT(Waitable);
	ASSERT(FileContext);
	pVCB = FileContext->pVCB;
	ASSERT_VCB(pVCB);


	DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					 ("FileContext Searched LotNumber(%I64d) FileType(%ld)\n", 
					 FileContext->LotNumber,
					 FileContext->FileType
					 ));
	
	FileContext->pSearchedFCB = NULL;	


		
	try{
		if(FileContext->Buffer == NULL){
			FileContext->Buffer = 
				xixcore_AllocateBuffer(XIDISK_FILE_HEADER_LOT_SIZE);
		}
	}finally{
		if(AbnormalTermination()){
			RC = STATUS_INSUFFICIENT_RESOURCES;
		}

	}

	if(!NT_SUCCESS(RC)){
		return RC;
	}


	try{

		RC = xixcore_RawReadLotAndFileHeader(
				(PXIXCORE_BLOCK_DEVICE)pVCB->XixcoreVcb.XixcoreBlockDevice,
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize,
				pVCB->XixcoreVcb.SectorSizeBit,
				FileContext->LotNumber,
				FileContext->Buffer,
				&Reason
				);
				
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Fail Read File Data(%I64d).\n", FileContext->LotNumber));
			try_return (RC);
		}	
		
		pFileHeader = (PXIDISK_FILE_HEADER_LOT)xixcore_GetDataBuffer(FileContext->Buffer);

		// Check Header
		if(FileContext->FileType ==  FCB_TYPE_DIR){
			LotType = LOT_INFO_TYPE_DIRECTORY;
		}else{
			LotType = LOT_INFO_TYPE_FILE;
		}

		RC = xixcore_CheckLotInfo(
				&pFileHeader->LotHeader.LotInfo,
				pVCB->XixcoreVcb.VolumeLotSignature,
				FileContext->LotNumber,
				LotType,
				LOT_FLAG_BEGIN,
				&Reason
				);


		if(!NT_SUCCESS(RC)){
			RC = STATUS_FILE_CORRUPT_ERROR;
			try_return(RC);
		}
		
	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FileInfoFromContext Status (0x%x).\n", RC));

	return RC;
}


NTSTATUS
xixfs_ReadFileInfoFromFcb(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN PXIXCORE_BUFFER Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint64			FileId = 0;
	PXIXFS_VCB					pVCB = NULL;
	PXIDISK_FILE_HEADER_LOT		pFileHeader = NULL;
	uint32			LotType = LOT_INFO_TYPE_INVALID;
	uint32			Reason = 0;

	PAGED_CODE();

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(Waitable);
	ASSERT(Buffer);
	ASSERT((Buffer->xcb_size - Buffer->xcb_offset) >= XIDISK_FILE_HEADER_LOT_SIZE);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_ReadFileInfoFromFcb\n"));

	try{

		RC = xixcore_RawReadLotAndFileHeader(
				pVCB->XixcoreVcb.XixcoreBlockDevice,
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize,
				pVCB->XixcoreVcb.SectorSizeBit,
				pFCB->XixcoreFcb.LotNumber,
				Buffer,
				&Reason
				);

				
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Fail Read File Data(%I64d).\n", pFCB->XixcoreFcb.LotNumber));
			try_return (RC);
		}	
		
		pFileHeader = (PXIDISK_FILE_HEADER_LOT)xixcore_GetDataBuffer(Buffer);

		// Check Header
		if(pFCB->XixcoreFcb.FCBType ==  FCB_TYPE_DIR){
			LotType = LOT_INFO_TYPE_DIRECTORY;
		}else{
			LotType = LOT_INFO_TYPE_FILE;
		}

		RC = xixcore_CheckLotInfo(
				&pFileHeader->LotHeader.LotInfo,
				pVCB->XixcoreVcb.VolumeLotSignature,
				pFCB->XixcoreFcb.LotNumber,
				LotType,
				LOT_FLAG_BEGIN,
				&Reason
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("xixfs_ReadFileInfoFromFcb : XixFsCheckLotInfo (%I64d).\n", pFCB->XixcoreFcb.LotNumber));
			RC = STATUS_FILE_CORRUPT_ERROR;
			try_return(RC);
		}
	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_ReadFileInfoFromFcb Status(0x%x)\n", RC));

	return RC;
}



NTSTATUS
xixfs_WriteFileInfoFromFcb(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN PXIXCORE_BUFFER Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint64			FileId = 0;
	PXIXFS_VCB		pVCB = NULL;
	uint32			Reason = 0;

	PAGED_CODE();

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(Waitable);
	ASSERT(Buffer);
	ASSERT((Buffer->xcb_size - Buffer->xcb_offset) >= XIDISK_FILE_HEADER_LOT_SIZE);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_WriteFileInfoFromFcb\n"));

	try{

		RC = xixcore_RawWriteLotAndFileHeader(
				pVCB->XixcoreVcb.XixcoreBlockDevice,
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize,
				pVCB->XixcoreVcb.SectorSizeBit,
				pFCB->XixcoreFcb.LotNumber,
				Buffer,
				&Reason
				);
				
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail Write File Data(%I64d).\n", pFCB->XixcoreFcb.LotNumber));
			try_return (RC);
		}	
		

	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_WriteFileInfoFromFcb Status(0x%x)\n", RC));

	return RC;
}



NTSTATUS
xixfs_ReLoadFileFromFcb(
	IN PXIXFS_FCB pFCB
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	PXIXFS_VCB					pVCB = NULL;
	PXIDISK_FILE_HEADER			pFileHeader = NULL;
	PXIDISK_DIR_HEADER			pDirHeader = NULL;
	LARGE_INTEGER	Offset;
	uint32			LotCount;

	PXIXCORE_BUFFER Buffer = NULL;
	uint64			FileId = 0;
	uint32			LotType = LOT_INFO_TYPE_INVALID;
	uint8			*NameBuffer = NULL;
	uint8			*ChildBuffer = NULL;
	uint32			Reason;
	uint32			AddLotSize = 0;
	
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_ReLoadFileFromFcb\n"));

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	
	FileId = pFCB->XixcoreFcb.LotNumber;
	Buffer = xixcore_AllocateBuffer(XIDISK_FILE_HEADER_LOT_SIZE);

	
	if(!Buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xixcore_GetDataBuffer(Buffer),XIDISK_FILE_HEADER_LOT_SIZE);

	try{
		RC = xixfs_ReadFileInfoFromFcb(
						pFCB,
						TRUE,
						Buffer
						);

		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		pFileHeader = (PXIDISK_FILE_HEADER)xixcore_GetDataBuffer(Buffer);

		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
			pDirHeader = (PXIDISK_DIR_HEADER)xixcore_GetDataBuffer(Buffer);
			pFCB->XixcoreFcb.Access_time = pDirHeader->DirInfo.Access_time;
			pFCB->XixcoreFcb.Modified_time = pDirHeader->DirInfo.Modified_time;
			pFCB->XixcoreFcb.Create_time = pDirHeader->DirInfo.Create_time;
			
			if(pDirHeader->DirInfo.NameSize != 0){
				uint16 Size = (uint16)pDirHeader->DirInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->XixcoreFcb.FCBName){
					ExFreePoolWithTag(pFCB->XixcoreFcb.FCBName, XCTAG_FCBNAME);
					pFCB->XixcoreFcb.FCBName = NULL;

				}
				pFCB->XixcoreFcb.FCBNameLength = Size;
				pFCB->XixcoreFcb.FCBName =ExAllocatePoolWithTag(PagedPool, SECTORALIGNSIZE_512(Size), XCTAG_FCBNAME);

				if(!pFCB->XixcoreFcb.FCBName){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				
				RtlCopyMemory(
					pFCB->XixcoreFcb.FCBName, 
					pDirHeader->DirInfo.Name, 
					pFCB->XixcoreFcb.FCBNameLength);		
			}
			pFCB->RealFileSize = pDirHeader->DirInfo.FileSize;
			pFCB->XixcoreFcb.RealAllocationSize = pDirHeader->DirInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart = pDirHeader->DirInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pDirHeader->DirInfo.AllocationSize; 
			
			pFCB->XixcoreFcb.FileAttribute = pDirHeader->DirInfo.FileAttribute;
			pFCB->XixcoreFcb.LinkCount = pDirHeader->DirInfo.LinkCount;
			pFCB->XixcoreFcb.Type = pDirHeader->DirInfo.Type;
			if(pVCB->XixcoreVcb.RootDirectoryLotIndex == FileId) {
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.Type, (XIFS_FD_TYPE_ROOT_DIRECTORY|XIFS_FD_TYPE_DIRECTORY));
			}
			pFCB->XixcoreFcb.LotNumber = pDirHeader->DirInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_DIR_INDICATOR|(FCB_ADDRESS_MASK & pFCB->XixcoreFcb.LotNumber));
			pFCB->XixcoreFcb.ParentLotNumber = pDirHeader->DirInfo.ParentDirLotIndex;
			pFCB->XixcoreFcb.AddrLotNumber = pDirHeader->DirInfo.AddressMapIndex;
			
			pFCB->XixcoreFcb.ChildCount= pDirHeader->DirInfo.childCount;		
			pFCB->XixcoreFcb.ChildCount = (uint32)xixcore_FindSetBitCount(1024, pDirHeader->DirInfo.ChildMap);
			pFCB->XixcoreFcb.AddrStartSecIndex = 0;

		}else{
			pFCB->XixcoreFcb.Access_time = pFileHeader->FileInfo.Access_time;
			
			pFCB->XixcoreFcb.Modified_time = pFileHeader->FileInfo.Modified_time;
			pFCB->XixcoreFcb.Create_time = pFileHeader->FileInfo.Create_time;
			if(pFileHeader->FileInfo.NameSize != 0){
				uint16 Size = (uint16)pFileHeader->FileInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->XixcoreFcb.FCBName){
					ExFreePoolWithTag(pFCB->XixcoreFcb.FCBName, XCTAG_FCBNAME);
					pFCB->XixcoreFcb.FCBName = NULL;

				}

				pFCB->XixcoreFcb.FCBNameLength =  Size;
				pFCB->XixcoreFcb.FCBName = ExAllocatePoolWithTag(NonPagedPool, SECTORALIGNSIZE_512(Size), XCTAG_FCBNAME);

				if(!pFCB->XixcoreFcb.FCBName){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->XixcoreFcb.FCBName, 
					pFileHeader->FileInfo.Name, 
					pFCB->XixcoreFcb.FCBNameLength);
			}
			pFCB->RealFileSize = pFileHeader->FileInfo.FileSize;
			pFCB->XixcoreFcb.RealAllocationSize = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart =  pFileHeader->FileInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->XixcoreFcb.FileAttribute = pFileHeader->FileInfo.FileAttribute;
			pFCB->XixcoreFcb.LinkCount = pFileHeader->FileInfo.LinkCount;
			pFCB->XixcoreFcb.Type = pFileHeader->FileInfo.Type;
			
			pFCB->XixcoreFcb.LotNumber = pFileHeader->FileInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_FILE_INDICATOR|(FCB_ADDRESS_MASK & pFCB->XixcoreFcb.LotNumber));
			pFCB->XixcoreFcb.ParentLotNumber = pFileHeader->FileInfo.ParentDirLotIndex;
			pFCB->XixcoreFcb.AddrLotNumber = pFileHeader->FileInfo.AddressMapIndex;
			
			pFCB->XixcoreFcb.AddrStartSecIndex = 0;
		}

		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RELOAD)){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RELOAD);
		}else if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RENAME)){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RENAME);	
		}else if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_LINK)){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_LINK);
		}


		if(pFCB->XixcoreFcb.Access_time == 0){
			pFCB->XixcoreFcb.Access_time = xixcore_GetCurrentTime64();
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->Access_time(%I64d).\n", pFCB->XixcoreFcb.Access_time));
		}
		if(pFCB->XixcoreFcb.Modified_time == 0)
		{
			pFCB->XixcoreFcb.Modified_time = xixcore_GetCurrentTime64();
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Initialize pFCB->Modified_time(%I64d).\n",pFCB->XixcoreFcb.Modified_time));
		}
		
		if(pFCB->XixcoreFcb.Create_time == 0)
		{
			pFCB->XixcoreFcb.Create_time = xixcore_GetCurrentTime64();
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->Create_time(%I64d).\n", pFCB->XixcoreFcb.Create_time));
		}


		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						("Add to FCB File LotNumber(%I64d).\n", FileId));

	}finally{
		if(Buffer) {
			xixcore_FreeBuffer(Buffer);
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_ReLoadFileFromFcb\n"));
	return RC;
}






NTSTATUS
xixfs_InitializeFCBInfo(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	PXIXFS_VCB					pVCB = NULL;
	PXIDISK_FILE_HEADER			pFileHeader = NULL;
	PXIDISK_DIR_HEADER			pDirHeader = NULL;
	LARGE_INTEGER	Offset;
	uint32			LotCount;

	PXIXCORE_BUFFER Buffer = NULL;
	uint64			FileId = 0;
	uint32			LotType = LOT_INFO_TYPE_INVALID;
	uint8			*NameBuffer = NULL;
	uint8			*ChildBuffer = NULL;
	uint32			Reason;
	uint32			AddLotSize = 0;
	
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_InitializeFCBInfo\n"));

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(Waitable);

	
	FileId = pFCB->XixcoreFcb.LotNumber;


	Buffer = xixcore_AllocateBuffer(XIDISK_FILE_HEADER_LOT_SIZE);

	
	if(!Buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xixcore_GetDataBuffer(Buffer),XIDISK_FILE_HEADER_LOT_SIZE);

	try{
		RC = xixfs_ReadFileInfoFromFcb(
						pFCB,
						Waitable,
						Buffer
						);

		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		pFileHeader = (PXIDISK_FILE_HEADER)xixcore_GetDataBuffer(Buffer);

		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
			pDirHeader = (PXIDISK_DIR_HEADER)xixcore_GetDataBuffer(Buffer);;
			pFCB->XixcoreFcb.Access_time = pDirHeader->DirInfo.Access_time;
			pFCB->XixcoreFcb.Modified_time = pDirHeader->DirInfo.Modified_time;
			pFCB->XixcoreFcb.Create_time = pDirHeader->DirInfo.Create_time;
			
			if(pDirHeader->DirInfo.NameSize != 0){
				uint16 Size = (uint16)pDirHeader->DirInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->XixcoreFcb.FCBName){
					ExFreePoolWithTag(pFCB->XixcoreFcb.FCBName, XCTAG_FCBNAME);
					pFCB->XixcoreFcb.FCBName = NULL;

				}


				pFCB->XixcoreFcb.FCBNameLength = Size;
				pFCB->XixcoreFcb.FCBName =ExAllocatePoolWithTag(PagedPool, SECTORALIGNSIZE_512(Size),XCTAG_FCBNAME);
				
				if(!pFCB->XixcoreFcb.FCBName){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->XixcoreFcb.FCBName, 
					pDirHeader->DirInfo.Name, 
					pFCB->XixcoreFcb.FCBNameLength);		
			}
			pFCB->RealFileSize = pDirHeader->DirInfo.FileSize;
			pFCB->XixcoreFcb.RealAllocationSize = pDirHeader->DirInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart = pDirHeader->DirInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pDirHeader->DirInfo.AllocationSize; 
			
			pFCB->XixcoreFcb.FileAttribute = pDirHeader->DirInfo.FileAttribute;
			
			pFCB->XixcoreFcb.LinkCount = pDirHeader->DirInfo.LinkCount;
			pFCB->XixcoreFcb.Type = pDirHeader->DirInfo.Type;
			if(pVCB->XixcoreVcb.RootDirectoryLotIndex == FileId) {
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.Type, (XIFS_FD_TYPE_ROOT_DIRECTORY|XIFS_FD_TYPE_DIRECTORY));
			}
			pFCB->XixcoreFcb.LotNumber = pDirHeader->DirInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_DIR_INDICATOR|(FCB_ADDRESS_MASK & pFCB->XixcoreFcb.LotNumber));
			pFCB->XixcoreFcb.ParentLotNumber = pDirHeader->DirInfo.ParentDirLotIndex;
			pFCB->XixcoreFcb.AddrLotNumber = pDirHeader->DirInfo.AddressMapIndex;
			
			pFCB->XixcoreFcb.ChildCount= pDirHeader->DirInfo.childCount;		
			pFCB->XixcoreFcb.ChildCount = (uint32)xixcore_FindSetBitCount(1024, pDirHeader->DirInfo.ChildMap);
			pFCB->XixcoreFcb.AddrStartSecIndex = 0;
			pFCB->XixcoreFcb.AddrLotSize = pVCB->XixcoreVcb.AddrLotSize;

		}else{
			pFCB->XixcoreFcb.Access_time = pFileHeader->FileInfo.Access_time;
			
			pFCB->XixcoreFcb.Modified_time = pFileHeader->FileInfo.Modified_time;
			pFCB->XixcoreFcb.Create_time = pFileHeader->FileInfo.Create_time;
			if(pFileHeader->FileInfo.NameSize != 0){
				uint16 Size = (uint16)pFileHeader->FileInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->XixcoreFcb.FCBName){
					ExFreePoolWithTag(pFCB->XixcoreFcb.FCBName,XCTAG_FCBNAME );
					pFCB->XixcoreFcb.FCBName = NULL;

				}

				pFCB->XixcoreFcb.FCBNameLength = Size;
				pFCB->XixcoreFcb.FCBName = ExAllocatePoolWithTag(NonPagedPool, SECTORALIGNSIZE_512(Size), XCTAG_FCBNAME);

				if(!pFCB->XixcoreFcb.FCBName){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->XixcoreFcb.FCBName, 
					pFileHeader->FileInfo.Name, 
					pFCB->XixcoreFcb.FCBNameLength);
			}
			pFCB->RealFileSize = pFileHeader->FileInfo.FileSize;
			pFCB->XixcoreFcb.RealAllocationSize = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart =  pFileHeader->FileInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->XixcoreFcb.FileAttribute = pFileHeader->FileInfo.FileAttribute;
			pFCB->XixcoreFcb.LinkCount = pFileHeader->FileInfo.LinkCount;
			pFCB->XixcoreFcb.Type = pFileHeader->FileInfo.Type;
			
			pFCB->XixcoreFcb.LotNumber = pFileHeader->FileInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_FILE_INDICATOR|(FCB_ADDRESS_MASK & pFCB->XixcoreFcb.LotNumber));
			pFCB->XixcoreFcb.ParentLotNumber = pFileHeader->FileInfo.ParentDirLotIndex;
			pFCB->XixcoreFcb.AddrLotNumber = pFileHeader->FileInfo.AddressMapIndex;
			
			pFCB->XixcoreFcb.AddrStartSecIndex = 0;
			pFCB->XixcoreFcb.AddrLotSize = pVCB->XixcoreVcb.AddrLotSize;
			
		
			
			if(!pFCB->XixcoreFcb.AddrLot){
				pFCB->XixcoreFcb.AddrLot = xixcore_AllocateBuffer(pFCB->XixcoreFcb.AddrLotSize);
				if(!pFCB->XixcoreFcb.AddrLot){
						RC = STATUS_INSUFFICIENT_RESOURCES;
						try_return(RC);
				}
			}


			RC = xixcore_RawReadAddressOfFile(
						pVCB->XixcoreVcb.XixcoreBlockDevice, 
						pVCB->XixcoreVcb.LotSize,
						pVCB->XixcoreVcb.SectorSize,
						pVCB->XixcoreVcb.SectorSizeBit,
						pFCB->XixcoreFcb.LotNumber,
						pFCB->XixcoreFcb.AddrLotNumber,
						pFCB->XixcoreFcb.AddrLotSize,
						&(pFCB->XixcoreFcb.AddrStartSecIndex),
						0,
						pFCB->XixcoreFcb.AddrLot,
						&Reason
						);						

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
		
		}

		if(pFCB->XixcoreFcb.Access_time == 0){
			pFCB->XixcoreFcb.Access_time = xixcore_GetCurrentTime64();
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->Access_time(%I64d).\n", pFCB->XixcoreFcb.Access_time));
		}
		if(pFCB->XixcoreFcb.Modified_time == 0)
		{
			pFCB->XixcoreFcb.Modified_time = xixcore_GetCurrentTime64();
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Initialize pFCB->Modified_time(%I64d).\n",pFCB->XixcoreFcb.Modified_time));
		}
		
		if(pFCB->XixcoreFcb.Create_time == 0)
		{
			pFCB->XixcoreFcb.Create_time = xixcore_GetCurrentTime64();
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->Create_time(%I64d).\n", pFCB->XixcoreFcb.Create_time));
		}


		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						("Add to FCB File LotNumber(%I64d).\n", FileId));

	}finally{
		if(Buffer) {
			xixcore_FreeBuffer(Buffer);
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_InitializeFCBInfo\n"));
	return RC;
}