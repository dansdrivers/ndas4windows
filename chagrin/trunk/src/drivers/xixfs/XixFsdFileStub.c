#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"

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
XixFsdVolumePrepareBuffers (
    IN PXIFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIFS_FCB 			FCB,
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
XixFsdFilePrepareBuffers(
    IN PXIFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIFS_FCB 			FCB,
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
XixFsdPrepareBuffers (
    IN PXIFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIFS_FCB 			FCB,
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
XixFsdCheckFCBAccess(
	IN PXIFS_FCB	pFCB,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN uint32	RequestDeposition,
	IN ACCESS_MASK	DesiredAccess
);





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdCheckVerifyVolume)
#pragma alloc_text(PAGE, XixFsdVerifyFcbOperation)
#pragma alloc_text(PAGE, XixFsdNonCachedIo)
#pragma alloc_text(PAGE, XifsdProcessNonCachedIo)
#pragma alloc_text(PAGE, XixFsdVolumePrepareBuffers)
#pragma alloc_text(PAGE, XixFsdPrepareBuffers)
#pragma alloc_text(PAGE, XixFsdFilePrepareBuffers)
#pragma alloc_text(PAGE, XixFsdCheckFastIoPossible)
#pragma alloc_text(PAGE, XixFsdCheckFCBAccess)
#pragma alloc_text(PAGE, XixFsdOpenExistingFCB)
#pragma alloc_text(PAGE, XixFsdOpenByFileId)
#pragma alloc_text(PAGE, XixFsdOpenObjectFromDirContext) 
#pragma alloc_text(PAGE, XixFsdOpenNewFileObject)
#endif

NTSTATUS
XixFsdCheckVerifyVolume(
	IN PXIFS_VCB	pVCB
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

	}else if (XifsdCheckFlagBoolean(pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED))
	{
		
		RC = STATUS_ACCESS_DENIED;
	}else if(XifsdCheckFlagBoolean(pVCB->VCBFlags, XIFSD_VCB_FLAGS_PROCESSING_PNP)){
		
		RC = STATUS_UNSUCCESSFUL;
	}
	
	XifsdUnlockVcb(NULL, pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status (0x%x) XifsdCheckVerifyVolume .\n", RC));
	
	return RC;
}


BOOLEAN
XixFsdVerifyFcbOperation (
	IN PXIFS_IRPCONTEXT IrpContext OPTIONAL,
	IN PXIFS_FCB Fcb
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PXIFS_VCB Vcb = Fcb->PtrVCB;
	PDEVICE_OBJECT RealDevice = Vcb->PtrVPB->RealDevice;
	PIRP Irp;

    PAGED_CODE();
    DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdVerifyFcbOperation \n"));
    //
    //  Check that the fileobject has not been cleaned up.
    //
    
	if ( ARGUMENT_PRESENT( IrpContext ))  {

		PFILE_OBJECT FileObject;

		Irp = IrpContext->Irp;
		FileObject = IoGetCurrentIrpStackLocation( Irp)->FileObject;

		if ( FileObject && XifsdCheckFlagBoolean( FileObject->Flags, FO_CLEANUP_COMPLETE))  {

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

    if (XifsdCheckFlagBoolean( RealDevice->Flags, DO_VERIFY_VOLUME )) {

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

            XifsdSetFlag(RealDevice->Flags, DO_VERIFY_VOLUME);

            //IoSetHardErrorOrVerifyDevice( IrpContext->Irp, RealDevice );
            //XifsdRaiseStatus( IrpContext, STATUS_WRONG_VOLUME );
        }

        return FALSE;
    }
    DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdVerifyFcbOperation \n"));
    return TRUE;
}



NTSTATUS
XixFsdVolumePrepareBuffers (
    IN PXIFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIFS_FCB 			FCB,
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
	PXIFS_VCB			VCB = NULL;
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
		("Enter XixFsdVolumePrepareBuffers\n"));



	*bUnAligned = FALSE;

	Waitable = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

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

		Offset = ((ULONG)PhyStartByteOffset & (VCB->SectorSize -1));
		PhyStartByteOffset = SECTOR_ALIGNED_ADDR(VCB->SectorSize, ByteOffset);
		NewByteCount = (uint32)SECTOR_ALIGNED_SIZE(VCB->SectorSize, RemainByteCount) ;
		

		
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
		ChangedOrgTransferSize = SECTOR_ALIGNED_SIZE(VCB->SectorSize, OrgTransferSize);
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
		("Exit XixFsdVolumePrepareBuffers \n"));
	return RC;
}



NTSTATUS
XixFsdFilePrepareBuffers(
    IN PXIFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIFS_FCB 			FCB,
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
	
	XIFS_IO_LOT_INFO	AddressInfo;
	PXIFS_VCB			VCB = NULL;
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
	BOOLEAN				Waitable = FALSE;
	BOOLEAN				bUnalign = FALSE;
	uint32				RemainOrgByteCount = OrgBuffLenth;


	ASSERT_FCB(FCB);
	VCB = FCB->PtrVCB;
	ASSERT_VCB(VCB);

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB),
		("Enter XixFsdFilePrepareBuffers\n"));	

	//DbgPrint("XixFsdFilePrepareBuffers Request ByteOffset (%I64d) : File AllocationSize (%I64d)\n",ByteOffset, FCB->RealAllocationSize); 



	Waitable = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);



	*bUnAligned = FALSE;
	
	RtlZeroMemory(IoRun, sizeof(IO_RUN)*NumOfIoRun);
	RtlZeroMemory(&AddressInfo, sizeof(XIFS_IO_LOT_INFO));
	while((RemainByteCount > 0) && (IoRunIndex < NumOfIoRun)) 
	{		
		Margin = 0;
		TruncateSize = 0;
		Offset = 0;
		NewByteCount= 0;
		OrgTransferSize = 0;
		ChangedOrgTransferSize = 0;
		bUnalign = FALSE;

		RC = XixFsGetAddressInfoForOffset(
								Waitable, 
								FCB, 
								ByteOffset,
								LotSize, 
								(uint8 *)FCB->AddrLot,
								VCB->SectorSize, 
								&(FCB->AddrStartSecIndex),
								&Reason, 
								&AddressInfo);

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
		
		Offset = ((ULONG)PhyStartByteOffset & (VCB->SectorSize -1));
		NewPhyStartByteOffset = SECTOR_ALIGNED_ADDR(VCB->SectorSize, PhyStartByteOffset);	
		
		if(ByteOffset + RemainByteCount  >  AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)
		{
				NewByteCount = (uint32)(AddressInfo.LotTotalDataSize - (uint32)Margin + (uint32)Offset);
				
		}else{
				NewByteCount = RemainByteCount + (uint32)Offset;		
		}
		
		NewByteCount = SECTOR_ALIGNED_SIZE(VCB->SectorSize, NewByteCount);


		if(Waitable){
			if(NewByteCount > XIFSD_MAX_BUFF_LIMIT){
				NewByteCount = XIFSD_MAX_BUFF_LIMIT;
			}


		}else{
			// Changed by ILGU HONG
			//if(Offset || (TruncateSize && (RemainOrgByteCount < NewByteCount))){
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
		ChangedOrgTransferSize = SECTOR_ALIGNED_SIZE(VCB->SectorSize, OrgTransferSize);
		if((ChangedOrgTransferSize != OrgTransferSize) && (ChangedOrgTransferSize > RemainByteCount)){
			bUnalign = TRUE;
		}
		// Added by ILGU HONG END


		
		IoRun[IoRunIndex].DiskOffset = NewPhyStartByteOffset;
		// Changed by ILGU HONG
		//IoRun[IoRunIndex].DiskByteCount = NewByteCount;
		// Changed by ILGU HONG ND
		IoRun[IoRunIndex].UserBuffer = CurrentUserBuffer;
		
		// Changed by ILGU HONG
		//if(Offset || (TruncateSize && (RemainOrgByteCount < NewByteCount)) ){
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
			DbgPrint("UnAligned !!! offset(%ld) TruncateSize(%ld) NewByteCount(%ld) OrgTransferSize(%ld) PhyStartByteOffset(%I64d) NewPhyStartByteOffset(%I64d\n", 
					Offset, TruncateSize, NewByteCount, OrgTransferSize, PhyStartByteOffset,NewPhyStartByteOffset);

			


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
		("Exit XixFsdFilePrepareBuffers\n"));	
	return RC;
}


NTSTATUS
XixFsdPrepareBuffers (
    IN PXIFS_IRPCONTEXT 	IrpContext,
    IN PIRP 				Irp,
    IN PXIFS_FCB 			FCB,
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
		("Enter XixFsdPrepareBuffers\n"));

	if(TypeOfOpen <= UserVolumeOpen){
		return XixFsdVolumePrepareBuffers (
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

		return XixFsdFilePrepareBuffers (   
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
XixFsdFinishBuffers (
    IN PXIFS_IRPCONTEXT IrpContext,
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
		("Enter XixFsdFinishBuffers\n"));

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
		("Exit XixFsdFinishBuffers\n"));
    return FlushIoBuffers;
}



VOID
XixFsdWaitSync (
    IN PXIFS_IRPCONTEXT IrpContext
    )
{
   
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XixFsdWaitSync\n"));

    KeWaitForSingleObject( &(IrpContext->IoContext->SyncEvent),
                           Executive,
                           KernelMode,
                           FALSE,
                           NULL );

    KeClearEvent( &(IrpContext->IoContext->SyncEvent) );
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit XixFsdWaitSync\n"));

    return;
}

NTSTATUS
XixFsdMulipleAsyncIoCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
 {
	PXIFS_IO_CONTEXT IoContext = Context;


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Enter XixFsdMulipleAsyncIoCompletionRoutine  IrpCount(%ld) Status(%x) \n", 
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
				(" XixFsdMulipleAsyncIoCompletionRoutine Status (%x) Information (%ld)\n", 
				IoContext->Status, IoContext->Async.RequestedByteCount));



		if (NT_SUCCESS( IoContext->MasterIrp->IoStatus.Status )) {
			IoContext->MasterIrp->IoStatus.Information = IoContext->Async.RequestedByteCount;
			//DbgPrint("Async Write Result %ld\n", IoContext->Async.RequestedByteCount);
		}

		ExReleaseResourceForThreadLite( IoContext->Async.Resource, IoContext->Async.ResourceThreadId );

		ExFreePool(IoContext);
		
		DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit XixFsdMulipleAsyncIoCompletionRoutine  \n"));
		return STATUS_SUCCESS;

	}else{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("Free MDL!!!\n"));
		IoFreeMdl( Irp->MdlAddress );
		IoFreeIrp( Irp );
	}

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit XixFsdMulipleAsyncIoCompletionRoutine  \n"));
	return STATUS_MORE_PROCESSING_REQUIRED;
}



NTSTATUS
XixFsdMulipleSyncIoCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
 {
	PXIFS_IO_CONTEXT IoContext = Context;
	

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" call XixFsdMulipleSyncIoCompletionRoutine  IrpCount(%ld) Status(%x) \n", 
			IoContext->IrpCount ,Irp->IoStatus.Status));


	if (!NT_SUCCESS( Irp->IoStatus.Status )) {

		//ASSERT(FALSE);
		//DbgPrint("XixFsdMulipleSyncIoCompletionRoutine sub Status (%x) \n", Irp->IoStatus.Status);

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
				("XixFsdMulipleSyncIoCompletionRoutine Status (%x) \n", IoContext->Status));
		
		IoContext->MasterIrp->IoStatus.Status = IoContext->Status;
		KeSetEvent( &(IoContext->SyncEvent), 0, FALSE );
	}

	UNREFERENCED_PARAMETER( DeviceObject );

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		(" Exit XixFsdMulipleSyncIoCompletionRoutine  \n"));
	return STATUS_MORE_PROCESSING_REQUIRED;
}


VOID
XixFsdMultipleAsyncIO(
	IN PXIFS_IRPCONTEXT	IrpContext,
	IN PXIFS_VCB		pVCB,
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
		(" Enter XixFsdMultipleAsyncIO  \n"));	


	if(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		CompletionRoutine= XixFsdMulipleSyncIoCompletionRoutine;
	}else{
		CompletionRoutine = XixFsdMulipleAsyncIoCompletionRoutine;
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
		(" Exit XixFsdMultipleAsyncIO  \n"));	

	return;
	
}



NTSTATUS
XixFsdSingleSyncIoCompletionRoutine(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	PXIFS_IO_CONTEXT IoContext = NULL;
	IoContext = (PXIFS_IO_CONTEXT)Context;
	


	//
	//  Store the correct information field into the Irp.
	//
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XixFsdSingleSyncIoCompletionRoutine  \n"));

	if (!NT_SUCCESS( Irp->IoStatus.Status )) {
		//ASSERT(FALSE);
	    Irp->IoStatus.Information = 0;
	}

	DebugTrace(DEBUG_LEVEL_INFO, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("XixFsdSingleSyncIoCompletionRoutine IoContext (%p)\n", IoContext));

	KeSetEvent( &(IoContext->SyncEvent), 0, FALSE );

	UNREFERENCED_PARAMETER( DeviceObject );
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit XixFsdSingleSyncIoCompletionRoutine  \n"));

	return STATUS_MORE_PROCESSING_REQUIRED;


}


NTSTATUS
XixFsdSingleAsyncIoCompletionRoutine(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	PXIFS_IO_CONTEXT IoContext = NULL;
	IoContext = (PXIFS_IO_CONTEXT)Context;

	
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XixFsdSingleAsyncIoCompletionRoutine \n"));


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
				(" call XixFsdSingleAsyncIoCompletionRoutine  \n"));

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
XixFsdSingleSyncIO(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_VCB		pVCB,
	IN LONGLONG  byteoffset,
	IN ULONG byteCount
)
{
	PIO_STACK_LOCATION		IrpSp;
	PIO_COMPLETION_ROUTINE	CompletionRoutine;


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XifdSingleSyncIO \n"));
	
	CompletionRoutine = XixFsdSingleSyncIoCompletionRoutine;
	

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
		("Exit XixFsdSingleSyncIO \n"));
	return;
}



VOID
XixFsdSingleAsyncIO(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_VCB		pVCB,
	IN LONGLONG  byteoffset,
	IN ULONG byteCount
)
{
	PIO_STACK_LOCATION		IrpSp;
	PIO_COMPLETION_ROUTINE	CompletionRoutine;


	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XixFsdSingleAsyncIO \n"));



	if(XifsdCheckFlagBoolean( IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)){
		CompletionRoutine = XixFsdSingleSyncIoCompletionRoutine;
	}else{
		CompletionRoutine = XixFsdSingleAsyncIoCompletionRoutine;
	}



	IoSetCompletionRoutine(IrpContext->Irp, 
							CompletionRoutine,
							(PVOID)(IrpContext->IoContext),
							TRUE,
							TRUE,
							TRUE);

	DebugTrace(DEBUG_LEVEL_INFO, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
							(" XixFsdSingleAsyncIO IoContext (%p)\n", IrpContext->IoContext));

	IrpSp = IoGetNextIrpStackLocation(IrpContext->Irp);
	IrpSp->MajorFunction = IrpContext->MajorFunction;
	IrpSp->Parameters.Read.Length = byteCount;
	IrpSp->Parameters.Read.ByteOffset.QuadPart = byteoffset;

	IoCallDriver(pVCB->TargetDeviceObject, IrpContext->Irp);

	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit XixFsdSingleAsyncIO \n"));

	
	return;
}





NTSTATUS
XixFsdNonCachedIo(
 	IN PXIFS_FCB		FCB,
	IN PXIFS_IRPCONTEXT IrpContext,
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
	PXIFS_VCB	pVCB = NULL;
	BOOLEAN		FirstPass = TRUE;
	BOOLEAN		Waitable = FALSE;
	BOOLEAN		bChangeBuffer = FALSE;
	uint32		OrgBuffLenth = 0;


	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Enter XixFsdNonCachedIo \n"));

	ASSERT_FCB(FCB);
	
	pVCB = FCB->PtrVCB;
	ASSERT(pVCB);

	LotSize = pVCB->LotSize;

	Waitable = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	
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


	RC = XixFsdPinUserBuffer(
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
					uint8 *ReadBuffer = XixFsdGetCallersBuffer(Irp);
					DbgPrint("SetZero 1 OrgByteOffSet %I64d req length%ld Current File Size %I64d\n",OrgByteOffset.QuadPart, OrgReadLength, FCB->FileSize.QuadPart);
					RtlZeroMemory(ReadBuffer, OrgReadLength);
			}
			

			if(ByteOffset + ByteCount > (uint64)FCB->AllocationSize.QuadPart){
				if(ByteOffset > (uint64)FCB->AllocationSize.QuadPart) {
					uint8 *ReadBuffer = XixFsdGetCallersBuffer(Irp);
					RtlZeroMemory(ReadBuffer, ByteCount);
					DbgPrint("SetZero 2 ByteOffset %I64d ByteCount%ld Current File Size %I64d\n",ByteOffset, ByteCount, FCB->AllocationSize.QuadPart);
					Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = ByteCount;
					return STATUS_SUCCESS;
					
				}else{
					uint32	Magin = 0;
					uint32	Size =0;
					uint8	*ReadBuffer = XixFsdGetCallersBuffer(Irp);
					Magin = (uint32)(ByteOffset + ByteCount  - FCB->AllocationSize.QuadPart);
					Size = ByteCount - Magin;
					ReadBuffer += Magin;
					DbgPrint("SetZero 3 ByteOffset %I64d ByteCount%ld Current File Size %I64d\n",ByteOffset, ByteCount, FCB->AllocationSize.QuadPart);;
					RtlZeroMemory(ReadBuffer, Size);
					//ByteCount = Magin;
					
				}
			}

		}

	}else{

		OrgBuffLenth = (IoGetCurrentIrpStackLocation(Irp))->Parameters.Write.Length;
	}


	Buffer = (PVOID)XixFsdGetCallersBuffer(Irp);	

	CurrentStartByteOffset = ByteOffset;
	RemainByteCount = ByteCount;

	//OldBuffer = Irp->UserBuffer;
	//Irp->UserBuffer = Buffer;	



	try{	

		do{
			RtlZeroMemory(IoRun, sizeof(IO_RUN)*XIFS_DEFAULT_IO_RUN_COUNT);

			RC = XixFsdPrepareBuffers(
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


				XixFsdSingleAsyncIO(
							IrpContext,
							pVCB,
							IoRun[0].DiskOffset,
							IoRun[0].DiskByteCount
							);
				
				CleanUpRunCount = 0;

				if(Waitable){
					XixFsdWaitSync(IrpContext);
					RC = IrpContext->Irp->IoStatus.Status;

					DebugTrace(DEBUG_LEVEL_INFO, 
						(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB| DEBUG_TARGET_ALL),
						(" XixFsdSingleAsyncIO Status (%x) \n", RC));

					//DbgPrint("Sync Write Result %ld\n", ProcessedByteCount);

					if (!NT_SUCCESS( RC )) {
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							(" FAIL XixFsdSingleAsyncIO Status (%x) \n", RC));
						try_return(RC);
					}
					try_return(RC);

				}else {
					XifsdClearFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT);
					RC = STATUS_PENDING;
				}
				
				try_return(RC);
				
			}else{

				if(Waitable == FALSE){
					IrpContext->IoContext->Async.RequestedByteCount = ProcessedByteCount;
				}


				XixFsdMultipleAsyncIO(
					IrpContext, 
					pVCB,
					NumberOfRuns, 
					IoRun
					);
			
				if(Waitable){
					XixFsdWaitSync(IrpContext);
					RC = IrpContext->Irp->IoStatus.Status;
					
					DebugTrace(DEBUG_LEVEL_INFO, 
						(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB | DEBUG_TARGET_ALL),
						(" XixFsdMultipleAsyncIO Status (%x) \n", RC));


					//DbgPrint("Sync Write Result %ld\n", ProcessedByteCount);
					if (!NT_SUCCESS( RC )) {
						try_return(RC);
					}

					if (UnAligned &&
						XixFsdFinishBuffers( IrpContext, IoRun, CleanUpRunCount, FALSE, IsReadOp )) {
						FlushBuffer = TRUE;
					}

					CleanUpRunCount = 0;
					RemainByteCount -= ProcessedByteCount;
					OrgBuffLenth -= ProcessedByteCount;
					CurrentStartByteOffset += ProcessedByteCount;
					Buffer =  (PCHAR)Buffer + ProcessedByteCount;
					TotalProcessByteCount += ProcessedByteCount;
				}else{
					XifsdClearFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT);
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
			XixFsdFinishBuffers( IrpContext, IoRun, CleanUpRunCount, TRUE, IsReadOp );
		}

	}
	
	if(bChangeBuffer && Waitable){
		Irp->UserBuffer = NULL;
	}
	
	DebugTrace(DEBUG_LEVEL_ALL, 
		(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_READ| DEBUG_TARGET_WRITE| DEBUG_TARGET_FCB| DEBUG_TARGET_VCB),
		("Exit XixFsdNonCachedIo \n"));
	return RC;
}



NTSTATUS
XifsdProcessNonCachedIo(
	IN PVOID			pContext
)
{

	NTSTATUS				RC = STATUS_SUCCESS;
	PXIFS_IRPCONTEXT		pIrpContext = NULL;
	PIRP					pIrp = NULL;
	PXIFS_IO_CONTEXT		pIoContext = NULL;
	XIFS_IO_CONTEXT			LocalIoContext;

	PIO_STACK_LOCATION		pIrpSp = NULL;
	LARGE_INTEGER			ByteOffset;
	uint32					Length = 0;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;
	PFILE_OBJECT			pFileObject = NULL;
	PERESOURCE				pResourceAcquired = NULL;
	ERESOURCE_THREAD		ResourceThreadId = 0;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;
	PXIFS_VCB				pVCB = NULL;
	BOOLEAN					PagingIo = FALSE;
	BOOLEAN					NonBufferedIo = FALSE;
	BOOLEAN					SynchronousIo = FALSE;
	BOOLEAN					bEOF = FALSE;
	BOOLEAN					IsEOFProcessing = FALSE;
	LARGE_INTEGER			OldFileSize;

	pIrpContext = (PXIFS_IRPCONTEXT)pContext;

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


	TypeOfOpen = XixFsdDecodeFileObject( pFileObject, &pFCB, &pCCB );



	if((TypeOfOpen == UnopenedFileObject) || (TypeOfOpen == UserDirectoryOpen))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			(" Un supported type %ld\n", TypeOfOpen));
		
		RC = STATUS_INVALID_DEVICE_REQUEST;
		XixFsdCompleteRequest(pIrpContext, RC, 0);
		return RC;
	}


	ASSERT_FCB(pFCB);

	pVCB = pFCB->PtrVCB;

	ASSERT_VCB(pVCB);


	if(pIrpContext->MajorFunction == IRP_MJ_WRITE){
		
		if(pFCB->HasLock != FCB_FILE_LOCK_HAS)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Has not Host Lock %lx .\n", pFCB));
			
			RC = STATUS_INVALID_DEVICE_REQUEST;
			XixFsdCompleteRequest(pIrpContext, RC, 0);
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
			DbgPrint("EndOfFile\n");
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
				((bEOF) || ((uint64)(ByteOffset.QuadPart + Length) > pFCB->RealAllocationSize))
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

				CurentEndIndex = GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
				EndLotIndex = GetIndexOfLogicalAddress(pVCB->LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
							
					RC = XixFsAddNewLot(
								pVCB, 
								pFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)pFCB->AddrLot,
								pFCB->AddrLotSize,
								&(pFCB->AddrStartSecIndex)
								);
					

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}


					if(LotCount != AllocatedLotCount){
						if(pFCB->RealAllocationSize < RequestStartOffset){
							RC = STATUS_INSUFFICIENT_RESOURCES;
							try_return(RC);
						}else if(Offset > pFCB->RealAllocationSize){
							Length = (uint32)(pFCB->RealAllocationSize - RequestStartOffset);
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
			IsEOFProcessing =  (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
									|| XixFsdCheckEofWrite(pFCB, &ByteOffset, Length));

						
			if(IsEOFProcessing){
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
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

		RtlZeroMemory(&LocalIoContext, sizeof(XIFS_IO_CONTEXT));
		pIrpContext->IoContext = &LocalIoContext;
		KeInitializeEvent(&(pIrpContext->IoContext->SyncEvent), NotificationEvent, FALSE);

		pIrpContext->IoContext->pIrpContext = pIrpContext;
		pIrpContext->IoContext->pIrp = pIrp;


		RC = XixFsdNonCachedIo (
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


					if(pFCB->WriteStartOffset ==  -1){
						pFCB->WriteStartOffset = ByteOffset.QuadPart;
						XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
					}

					if(pFCB->WriteStartOffset > ByteOffset.QuadPart){
						pFCB->WriteStartOffset = ByteOffset.QuadPart;
						XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
						
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
				XixFsdFinishIoEof(pFCB);
				XifsdUnlockFcb(NULL, pFCB);
			}

			if(pResourceAcquired){
					ExReleaseResourceForThreadLite( pResourceAcquired,ResourceThreadId );			
			}
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Result  XifsdCommonWrite  PtrIrp(%p) RC(%x)\n", pIrp, RC));

			XixFsdCompleteRequest(pIrpContext, RC, Length );

			ExFreePool(pIoContext);

	}

	return RC;
}


VOID
XixFsdAddToWorkque(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter  XixFsdAddToWorkque\n"));	
	XixFsdPostRequest(IrpContext, Irp);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit  XixFsdAddToWorkque\n"));	
}


VOID
XixFsdPrePostIrp(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
)
{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    BOOLEAN RemovedFcb = FALSE;
	BOOLEAN CanWait = FALSE;
    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter  XixFsdPrePostIrp\n"));

    ASSERT_IRPCONTEXT( IrpContext );
    ASSERT( Irp );

	CanWait = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
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

            XixFsdTeardownStructures( CanWait, *(IrpContext->TeardownFcb), FALSE, &RemovedFcb );

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

            XixFsdPinUserBuffer( Irp, IoWriteAccess, IrpSp->Parameters.Read.Length );
        }

        break;
        
    case IRP_MJ_WRITE :

        XixFsdPinUserBuffer( Irp, IoReadAccess, IrpSp->Parameters.Write.Length );
        break;

    //
    //  We also need to check whether this is a query file operation.
    //
    
    case IRP_MJ_DIRECTORY_CONTROL :

        if (IrpContext->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

            XixFsdPinUserBuffer( Irp, IoWriteAccess, IrpSp->Parameters.QueryDirectory.Length );
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
		("Exit  XixFsdPrePostIrp\n"));
    return;
}


VOID
XixFsdOplockComplete(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
)
{
	BOOLEAN RemovedFcb = FALSE;	
	BOOLEAN CanWait = FALSE;

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter  XixFsdOplockComplete\n"));

	CanWait = XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
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

					XixFsdTeardownStructures( CanWait, *(IrpContext->TeardownFcb), FALSE, &RemovedFcb );

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

		XixFsdAddToWorkque( IrpContext, Irp );

		//
		//  Otherwise complete the request.
		//

	} else {

		XixFsdCompleteRequest( IrpContext, Irp->IoStatus.Status, 0 );
	}
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit  XixFsdOplockComplete\n"));
	return;
}


FAST_IO_POSSIBLE
XixFsdCheckFastIoPossible(
	IN PXIFS_FCB pFCB
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdCheckFastIoPossible\n"));
	
	if(pFCB->HasLock == FCB_FILE_LOCK_OTHER_HAS){
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
			("Exit XixFsdCheckFastIoPossible: FastIoIsQuestionable\n"));
		return FastIoIsQuestionable;
	}

	if((pFCB->FCBFileLock == NULL) || !FsRtlAreThereCurrentFileLocks(pFCB->FCBFileLock)) {
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
			("Exit XixFsdCheckFastIoPossible: FastIoIsPossible\n"));
		return FastIoIsPossible;
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdCheckFastIoPossible: FastIoIsQuestionable\n"));
	return FastIoIsQuestionable;
}


NTSTATUS
XixFsdCheckFCBAccess(
	IN PXIFS_FCB	pFCB,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN uint32	RequestDeposition,
	IN ACCESS_MASK	DesiredAccess
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdCheckFCBAccess\n"));



	if(TypeOfOpen == UserVolumeOpen){
		if(XifsdCheckFlagBoolean(DesiredAccess, (FILE_DELETE_CHILD | DELETE|WRITE_DAC)) ){
			return STATUS_ACCESS_DENIED;
		}	

		//	Added by ILGU HONG for readonly 09052006
		if(XifsdCheckFlagBoolean(DesiredAccess, 
			(FILE_WRITE_ATTRIBUTES| FILE_WRITE_DATA| FILE_WRITE_EA|FILE_APPEND_DATA)) ){

					
					if(pFCB->PtrVCB->IsVolumeWriteProctected){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("XixFsdCheckFCBAccess Fail Is read only volume .\n"));
						
						return STATUS_ACCESS_DENIED;
					}
					
		}
		//	Added by ILGU HONG for readonly end	

	}else {
		if(pFCB->FCBType == FCB_TYPE_FILE){

			if((RequestDeposition == FILE_OVERWRITE) || 
				(RequestDeposition == FILE_OVERWRITE_IF) ||
				(RequestDeposition == FILE_SUPERSEDE)){
				if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
					DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						(" XixFsdCheckFCBAccess FAIL OVERWRITE!!!\n"));

					//	Added by ILGU HONG for readonly 09052006
					if(pFCB->PtrVCB->IsVolumeWriteProctected){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("XixFsdCheckFCBAccess Fail Is read only volume .\n"));
						
						return STATUS_ACCESS_DENIED;
					}
					//	Added by ILGU HONG for readonly end	
					//DbgPrint("<%s:%d>:Get XixFsLotLock\n", __FILE__,__LINE__);
					RC = XixFsLotLock(
							TRUE,
							pFCB->PtrVCB, 
							pFCB->PtrVCB->TargetDeviceObject, 
							pFCB->LotNumber, 
							&pFCB->HasLock,
							TRUE,
							FALSE);

					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail has not host lock .\n"));
						
						return STATUS_ACCESS_DENIED;
					}

					XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);
				}
			}

			if(XifsdCheckFlagBoolean(DesiredAccess, 
				(FILE_WRITE_ATTRIBUTES| FILE_WRITE_DATA| FILE_WRITE_EA|FILE_APPEND_DATA)) 
				|| DeleteOnCloseSpecified ){
				

				//	Added by ILGU HONG for readonly 09052006
				if(pFCB->PtrVCB->IsVolumeWriteProctected){
					if(DeleteOnCloseSpecified){ //fake: write open support
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("XixFsdCheckFCBAccess Fail Is read only volume .\n"));
						
						return STATUS_ACCESS_DENIED;
					}

				}else {
				//	Added by ILGU HONG for readonly end	


					if(DeleteOnCloseSpecified || XifsdCheckFlagBoolean(DesiredAccess, FILE_WRITE_DATA) ){
						//XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);

						if(pFCB->FCBType == FCB_TYPE_FILE){

							if(pFCB->SectionObject.ImageSectionObject != NULL){

								if (!MmFlushImageSection( &pFCB->SectionObject, MmFlushForWrite )) {

									return (DeleteOnCloseSpecified ? STATUS_CANNOT_DELETE : STATUS_SHARING_VIOLATION);
								}
							}

						}

					}


					if(pFCB->HasLock == FCB_FILE_LOCK_HAS){
						XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);
						return STATUS_SUCCESS;
					}

					//DbgPrint("<%s:%d>:Get XixFsLotLock\n", __FILE__,__LINE__);
					RC = XixFsLotLock(
							TRUE,
							pFCB->PtrVCB, 
							pFCB->PtrVCB->TargetDeviceObject, 
							pFCB->LotNumber, 
							&pFCB->HasLock,
							TRUE,
							FALSE);
					
					// Added by 04112006 fake: write open support
					/*
					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail has not host lock .\n"));
						return STATUS_ACCESS_DENIED;
					}
					
					pFCB->HasLock = FCB_FILE_LOCK_HAS;
					XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);
					*/

					if(!NT_SUCCESS(RC) && DeleteOnCloseSpecified){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail has not host lock .\n"));
						return STATUS_ACCESS_DENIED;
					}


					if(NT_SUCCESS(RC)){
						XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);
					}
					// Added End
				}


			}
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdCheckFCBAccess\n"));
	return STATUS_SUCCESS;
}



NTSTATUS
XixFsdFileOverWrite(
	IN PXIFS_VCB pVCB, 
	IN PXIFS_FCB pFCB,
	IN PFILE_OBJECT pFileObject
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	LARGE_INTEGER Size = {0,0};
	uint32 NotifyFilter = 0;

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFileOverWrite\n"));

	ASSERT_VCB(pVCB);
	ASSERT_FCB(pFCB);


	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Fail XixFsdFileOverWrite Has No Lock\n"));

		return STATUS_INVALID_PARAMETER;
	}

	InterlockedIncrement( &pFCB->FCBReference );
	try{
		
		
        NotifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE
                       | FILE_NOTIFY_CHANGE_ATTRIBUTES
                       | FILE_NOTIFY_CHANGE_SIZE;		

		pFileObject->SectionObjectPointer = &pFCB->SectionObject;
		
		if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
			if(!MmCanFileBeTruncated(&pFCB->SectionObject, &Size)){
				RC = STATUS_USER_MAPPED_FILE;
				try_return(RC);
			}

			CcPurgeCacheSection(&pFCB->SectionObject, NULL, 0, FALSE);
		}



		
		ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, TRUE);
		

		pFCB->FileSize.QuadPart = 0;
		pFCB->ValidDataLength.QuadPart = 0;

		pFCB->FileAttribute |= FILE_ATTRIBUTE_ARCHIVE;
	
		CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
		//DbgPrint(" 11 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
		
	
		ExReleaseResourceLite(pFCB->PagingIoResource);


		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
		
		XixFsdNotifyReportChange(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);

	}finally{
		InterlockedDecrement( &pFCB->FCBReference );
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFileOverWrite %x\n", RC));
	return RC;
}




NTSTATUS
XixFsdProcessOpenFCB(
	IN PXIFS_IRPCONTEXT	IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIFS_VCB	pVCB,
	IN OUT PXIFS_FCB * ppCurrentFCB,
	IN PXIFS_LCB	pLCB OPTIONAL,
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
	PXIFS_FCB			pFCB = *ppCurrentFCB;
	PXIFS_CCB			pCCB = NULL;
	PIRP				Irp = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdProcessOpenFCB\n"));
	
	ASSERT_IRPCONTEXT(IrpContext);
	Irp = IrpContext->Irp;
	ASSERT(Irp);
	ASSERT_VCB(pVCB);
	

	if((TypeOfOpen <= UserVolumeOpen)
		&& !XifsdCheckFlagBoolean(IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_READ ))
	{
		if(pVCB->VCBCleanup != 0){
			Irp->IoStatus.Information = 0;
			return STATUS_SHARING_VIOLATION;
		}
	

		ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

		LockVolume = TRUE;
		XixFsdRealCloseFCB(pVCB);
		RC = XixFsdPurgeVolume(IrpContext, pVCB, FALSE);

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
						("CALL XixFsdProcessOpenFCB : FsRtlCurrentBatchOplock\n"));
					OplockStatus = FsRtlCheckOplock( &pFCB->FCBOplock,
													 IrpContext->Irp,
													 IrpContext,
													 XixFsdOplockComplete,
													 XixFsdPrePostIrp );
					
					if (OplockStatus == STATUS_PENDING) {
						DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
							("return PENDING XixFsdProcessOpenFCB : FsRtlCurrentBatchOplock\n"));
						Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
						try_return (RC = STATUS_PENDING);
					}		
				}
			
				
				if((RequestDeposition == FILE_OVERWRITE) || (RequestDeposition == FILE_OVERWRITE_IF))
				{
					XifsdSetFlag(DesiredAccess, FILE_WRITE_DATA);
				
				}else if(RequestDeposition == FILE_SUPERSEDE){

					XifsdSetFlag(DesiredAccess, DELETE);
				}


				RC = IoCheckShareAccess(DesiredAccess, 
										IrpSp->Parameters.Create.ShareAccess,
										IrpSp->FileObject,
										&pFCB->FCBShareAccess,
										FALSE );

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail XixFsdProcessOpenFCB:IoCheckShareAccess Status(0x%x)\n", RC));

					Irp->IoStatus.Information = 0;
					try_return (RC);
				}

				DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("CALL XixFsdProcessOpenFCB: FsRtlCheckOplock\n"));
				

				OplockStatus = FsRtlCheckOplock( &pFCB->FCBOplock,
												 IrpContext->Irp,
												 IrpContext,
												 XixFsdOplockComplete,
												 XixFsdPrePostIrp );
				
				if (OplockStatus == STATUS_PENDING) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("return PENDING XixFsdProcessOpenFCB: FsRtlCheckOplock\n"));
					Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
					try_return (RC =  STATUS_PENDING);
				}		

				IrpContext->TeardownFcb = NULL;




				
				// Added by ILGU 04112006
				if( (XifsdCheckFlagBoolean(IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING))
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
						("Fail XixFsdProcessOpenFCB:IoCheckShareAccess Status(0x%x)\n", RC));

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
				RC = XixFsdFileOverWrite(pVCB, pFCB, IrpSp->FileObject);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail XixFsdProcessOpenFCB:XixFsdFileOverWrite Status(0x%x)\n", RC));

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



		pCCB = XixFsdCreateCCB(IrpContext,IrpSp->FileObject, pFCB, pLCB, CCBFlags);
		
		ASSERT_CCB(pCCB);


		if(DeleteOnClose){
			XifsdSetFlag(pCCB->CCBFlags, XIFSD_CCB_FLAGS_DELETE_ON_CLOSE);
		}


		if(bIgnoreCase){
			XifsdSetFlag(pCCB->CCBFlags, XIFSD_CCB_FLAGS_IGNORE_CASE);
		}


		if( !XifsdCheckFlagBoolean(CCBFlags, XIFSD_CCB_OPENED_BY_FILEID)
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
					pFCB->FCBTargetOffset = XixFsdSearchLastComponetOffset(&pFCB->FCBFullPath);
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

	XixFsdSetFileObject( IrpContext, IrpSp->FileObject, TypeOfOpen, pFCB, pCCB );
	




	//
	//  Set the appropriate cache flags for a user file object.
	//

	if (TypeOfOpen == UserFileOpen) {

		if (XifsdCheckFlagBoolean( IrpSp->Parameters.Create.Options, FILE_NO_INTERMEDIATE_BUFFERING )) {
			XifsdClearFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
			XifsdSetFlag( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING );		

		} else {

			XifsdSetFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
		}
	}else if (TypeOfOpen == UserVolumeOpen)  {

		//
		//  DASD access is always noncached
		//

		XifsdSetFlag( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING );
	}	
	
	XifsdLockVcb(IrpContext, pVCB);
	
	if( XifsdCheckFlagBoolean(IrpSp->FileObject->Flags, FILE_NO_INTERMEDIATE_BUFFERING)	)
	{
		pFCB->FcbNonCachedOpenCount ++;
	}

	XifsdIncCleanUpCount(pFCB);
	XifsdIncCloseCount(pFCB);
	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
			("Create FCB LotNumber (%I64d) FCBCleanUp(%ld) VCBCleanup(%ld)\n", 
			pFCB->LotNumber, pFCB->FCBCleanup, pVCB->VCBCleanup));

	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL, 
			("!!!!CREATE (%wZ) pCCB(%p) pFileObject(%p)\n", &pFCB->FCBName, pCCB, IrpSp->FileObject));



	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("XixFsdProcessOpenFCB LotNumber(%I64d) VCB (%d/%d) FCB (%d/%d)\n",
					 pFCB->LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));	
	
	XifsdIncRefCount(pFCB, 1, 1);

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("XixFsdProcessOpenFCB LotNumber(%I64d) VCB (%d/%d) FCB (%d/%d)\n",
					 pFCB->LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));	

	/*
	DbgPrint("Open CCB with  VCB (%d/%d) FCB:(%I64d)%wZ (%d/%d) TypeOfOpen(%ld)\n",
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->LotNumber,
					 &pFCB->FCBName,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference,
					 TypeOfOpen);	
	*/

   if (LockVolume) {

        pVCB->LockVolumeFileObject = IrpSp->FileObject;
        XifsdSetFlag( pVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED );
    }

    XifsdUnlockVcb( IrpContext, pVCB );

    XifsdLockFcb(IrpContext, pFCB);

	try{
		if (TypeOfOpen == UserFileOpen) {

			pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible(pFCB);

		} else {

			pFCB->IsFastIoPossible = FastIoIsNotPossible;
		}

		pFCB->DesiredAccess = DesiredAccess;

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
		("Exit XixFsdProcessOpenFCB\n"));
    return STATUS_SUCCESS;

}




NTSTATUS
XixFsdOpenExistingFCB(
	IN PXIFS_IRPCONTEXT	IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN OUT PXIFS_FCB * ppCurrentFCB,
	IN PXIFS_LCB	pLCB OPTIONAL,
	IN TYPE_OF_OPEN	TypeOfOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIFS_CCB	pRelatedCCB OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
)
{

	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	uint32	CCBFlags = 0;
	PIRP	Irp = NULL;
	

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdOpenExistingFCB\n"));

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

	RC = XixFsdCheckFCBAccess(
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
		if(XifsdCheckFlagBoolean(pRelatedCCB->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID) )
		{
			XifsdSetFlag(CCBFlags,XIFSD_CCB_OPENED_BY_FILEID);
		}
	}



	


	RC = XixFsdProcessOpenFCB(IrpContext,
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
		("Exit XixFsdOpenExistingFCB\n"));
	return RC;
}



NTSTATUS
XixFsdOpenByFileId(
	IN PXIFS_IRPCONTEXT		IrpContext,
	IN PIO_STACK_LOCATION	IrpSp,
	IN PXIFS_VCB			pVCB,
	IN BOOLEAN				DeleteOnCloseSpecified,
	IN BOOLEAN				bIgnoreCase,
	IN BOOLEAN				DirectoryOnlyRequested,
	IN BOOLEAN				FileOnlyRequested,
	IN uint32				RequestDeposition,
	IN OUT PXIFS_FCB		*ppFCB
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
	PXIFS_FCB NextFcb = NULL;
	PXIFS_FCB ParentFcb = NULL;
	PXIFS_LCB OpenLcb = NULL;

	BOOLEAN		bAcquireFcb = FALSE;
	BOOLEAN		bAcquireParentFcb = FALSE;

	PIRP		Irp = NULL;
	
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
			if(XifsdCheckFlagBoolean(DesiredAccess, FILE_LIST_DIRECTORY)){
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


	if( (LotNumber == pVCB->RootDirectoryLotIndex)
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

		NextFcb = XixFsdCreateAndAllocFCB( pVCB, LotNumber, FCB_TYPE_CODE, &FcbExisted );

		//
		//  Now, if the Fcb was not already here we have some work to do.
		//
    
		if (!FcbExisted) 
		{

			//
			//  If we can't wait then post this request.
			//

			ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

			//
			//  Use a try-finally to transform errors we get as a result of going
			//  off on a wild goose chase into a simple open failure.
			//

			try {

				NextFcb->FileId = FileId;

				RC = XixFsdInitializeFCBInfo(NextFcb,TRUE);



			}except( XixFsdExceptionFilter( IrpContext, GetExceptionInformation() )) {

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

		if (XifsdCheckFlagBoolean( NextFcb->Type, FILE_ATTRIBUTE_DIRECTORY )) {

			if (XifsdCheckFlagBoolean( IrpSp->Parameters.Create.Options, FILE_NON_DIRECTORY_FILE )) {
				Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
				try_return( RC = STATUS_FILE_IS_A_DIRECTORY );
			}

		} else if (XifsdCheckFlagBoolean( IrpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE )) {
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
			ParentFcb = XixFsdLookupFCBTable(pVCB, NextFcb->ParentLotNumber);

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

			OpenLcb = XixFsdInsertPrefix( IrpContext,
								   NextFcb,
								   &NextFcb->FCBName,
								   ParentFcb );

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						("XixFsdOpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
							ParentFcb->LotNumber,
							pVCB->VCBReference,
							pVCB->VCBUserReference,
							ParentFcb->FCBReference,
							ParentFcb->FCBUserReference ));

			XifsdIncRefCount( ParentFcb, 1, 1 );

			/*
			DbgPrint("Inc Link Count CCB with  VCB (%d/%d) ParentFCB:%wZ (%d/%d) ChildPFB:%wZ  XixFsdOpenByFileId\n",
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 &ParentFcb->FCBName,
					 ParentFcb->FCBReference,
					 ParentFcb->FCBUserReference,
					 &NextFcb->FCBName);	
			*/

			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						 ("XixFsdOpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d)  Vcb (%d/%d) Fcb (%d/%d)\n",
						 ParentFcb->LotNumber,
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

		RC = XixFsdCheckFCBAccess(
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

		RC = XixFsdProcessOpenFCB( IrpContext,
									 IrpSp,
									 pVCB,
									 ppFCB,
									 NULL,
									 TypeOfOpen,
									 RequestDeposition,
									 IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
									 XIFSD_CCB_OPENED_BY_FILEID,
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

		if (NextFcb && !XifsdCheckFlagBoolean(NextFcb->FCBFlags, XIFSD_FCB_INIT)) {

//			DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));
			XixFsdDeleteFcb( TRUE , NextFcb );
		}

	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XifsdOpenByFileId\n"));
	return RC;
}



NTSTATUS
XixFsdOpenObjectFromDirContext (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIFS_VCB Vcb,
	IN OUT PXIFS_FCB *CurrentFcb,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN PerformUserOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIFS_CCB RelatedCcb OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
)
{
	ULONG CcbFlags = 0;
	uint64 FileId = 0;
	BOOLEAN UnlockVcb = FALSE;
	BOOLEAN FcbExisted;
	PXIFS_FCB NextFcb = NULL;
	PXIFS_FCB ParentFcb = NULL;
	PXIDISK_CHILD_INFORMATION pChild = NULL;
	TYPE_OF_OPEN	TypeOfOpen;
	uint32			NodeTypeCode;
	PXIFS_LCB OpenLcb = NULL;
	PIRP	Irp = NULL;
	NTSTATUS RC;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdOpenObjectFromDirContext \n"));

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
			if(XifsdCheckFlagBoolean(RelatedCcb->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID) )
			{
				XifsdSetFlag(CcbFlags,XIFSD_CCB_OPENED_BY_FILEID);
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

		NextFcb = XixFsdCreateAndAllocFCB(Vcb, FileId, NodeTypeCode, &FcbExisted );

		//
		//  If the Fcb was created here then initialize from the values in the
		//  dirent.  We have optimistically assumed that there isn't any corrupt
		//  information to this point - we're about to discover it if there is.
		//

		if (!FcbExisted) {

   			//
			//  If we can't wait then post this request.
			//

			ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

			//
			//  Use a try-finally to transform errors we get as a result of going
			//  off on a wild goose chase into a simple open failure.
			//

			try {

				RC = XixFsdInitializeFCBInfo(NextFcb,TRUE);



			}except( XixFsdExceptionFilter( IrpContext, GetExceptionInformation() )) {

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
				RC = STATUS_INVALID_PARAMETER;

			}


			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) XixFsdInitializeFCBInfo .\n", RC));
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
			OpenLcb = XixFsdInsertPrefix( IrpContext,
								   NextFcb,
								   &DirContext->ChildName,
								   ParentFcb );

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						("XixFsdOpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
							ParentFcb->LotNumber,
							Vcb->VCBReference,
							Vcb->VCBUserReference,
							ParentFcb->FCBReference,
							ParentFcb->FCBUserReference ));

			XifsdIncRefCount( ParentFcb, 1, 1 );

			/*
			DbgPrint("Inc Link Count CCB with  VCB (%d/%d) ParentFCB:%wZ (%d/%d) ChildPFB:%wZ  XixFsdOpenObjectFromDirContext\n",
					 Vcb->VCBReference,
					 Vcb->VCBUserReference,
					 &ParentFcb->FCBName,
					 ParentFcb->FCBReference,
					 ParentFcb->FCBUserReference,
					 &NextFcb->FCBName);	
			*/

			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						 ("XixFsdOpenObjectFromDirContext, IncParentFCB Lotnumber(%I64d)  Vcb (%d/%d) Fcb (%d/%d)\n",
						 ParentFcb->LotNumber,
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

		XixFsdInitializeLcbFromDirContext( IrpContext,
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
				RC = XixFsdCheckFCBAccess(
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

			RC = XixFsdProcessOpenFCB(IrpContext,
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

		if (NextFcb && !XifsdCheckFlagBoolean( NextFcb->FCBFlags, XIFSD_FCB_INIT )) {
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
								("NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));
			XixFsdDeleteFcb( TRUE, NextFcb );
		}

	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdOpenObjectFromDirContext \n"));
	return RC;
}




NTSTATUS
XixFsdOpenNewFileObject(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIFS_VCB Vcb,
	IN PXIFS_FCB *CurrentFCB, //ParentFcb,
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
	PXIFS_FCB ParentFcb = NULL;
	PXIFS_FCB NextFcb = NULL;

	TYPE_OF_OPEN	TypeOfOpen;
	uint32			NodeTypeCode;

	PXIFS_LCB OpenLcb;

	NTSTATUS RC;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdOpenNewFileObject \n"));


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

	
		XifsdSetFlag(CcbFlags,XIFSD_CCB_OPENED_AS_FILE);
	

		FileId = OpenFileId;


		//
		//  Lock the Vcb so we can examine the Fcb Table.
		//

		XifsdLockVcb( IrpContext, Vcb );
		UnlockVcb = TRUE;

		//
		//  Get the Fcb for this file.
		//

		NextFcb = XixFsdCreateAndAllocFCB(Vcb, FileId, NodeTypeCode, &FcbExisted );
//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("1 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					 ("1 XixFsdOpenNewFileObject, Vcb %d/%d Fcb %d/%d\n",
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

			ASSERT(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

			//
			//  Use a try-finally to transform errors we get as a result of going
			//  off on a wild goose chase into a simple open failure.
			//

			try {

				RC = XixFsdInitializeFCBInfo(NextFcb,TRUE);
//				DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("2 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));

				

			}except( XixFsdExceptionFilter( IrpContext, GetExceptionInformation() )) {

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Exception!!\n"));
					RC = STATUS_INVALID_PARAMETER;

			}

			if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) XifsdInitializeFCBInfo .\n", RC));
					try_return(RC);				
			}
			
			RC = XixFsdCheckFCBAccess(
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

		OpenLcb = XixFsdInsertPrefix( IrpContext,
								   NextFcb,
								   OpenFileName,
								   ParentFcb );

//		DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_TRACE), 
//								("4 NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));
		//
		//  Now increment the reference counts for the parent and drop the Vcb.
		//

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					("XixFsdOpenNewFileObject IncFarentFCB LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
						ParentFcb->LotNumber,
						Vcb->VCBReference,
						Vcb->VCBUserReference,
						ParentFcb->FCBReference,
						ParentFcb->FCBUserReference ));

		XifsdIncRefCount( ParentFcb, 1, 1 );

		/*
		DbgPrint("Inc Link Count CCB with  VCB (%d/%d) ParentFCB:%wZ (%d/%d) ChildPFB:%wZ  XixFsdOpenNewFileObject\n",
				 Vcb->VCBReference,
				 Vcb->VCBUserReference,
				 &ParentFcb->FCBName,
				 ParentFcb->FCBReference,
				 ParentFcb->FCBUserReference,
				 &NextFcb->FCBName);
		*/

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
					 ("XixFsdOpenNewFileObject,IncFarentFCB LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n",
						ParentFcb->LotNumber,
						Vcb->VCBReference,
						Vcb->VCBUserReference,
						ParentFcb->FCBReference,
						ParentFcb->FCBUserReference ));

		XifsdUnlockVcb( IrpContext, Vcb );
		UnlockVcb = FALSE;




		//
		//  Perform initialization associated with the directory context.
		//

		XixFsdInitializeLcbFromDirContext( IrpContext,
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

		RC = XixFsdProcessOpenFCB(IrpContext,
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

		if (NextFcb && !XifsdCheckFlagBoolean(NextFcb->FCBFlags,XIFSD_FCB_INIT)) {

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("NewFcb->FCBFlags (0x%x)\n", NextFcb->FCBFlags));

			XixFsdDeleteFcb( TRUE, NextFcb );
		}

	}
	
	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
		("Exit XixFsdOpenNewFileObject RC(0x%x) \n", RC));
	return RC;
}



