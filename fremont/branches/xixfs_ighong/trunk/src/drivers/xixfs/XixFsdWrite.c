#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsdInternalApi.h"


void XixFsdDeferredWriteCallBack (
		void		*Context1,			// Should be PtrIrpContext
		void		*Context2			// Should be PtrIrp
);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdDeferredWriteCallBack)
#pragma alloc_text(PAGE, XixFsdCommonWrite)
#endif


NTSTATUS
XixFsdCommonWrite(
	IN PXIFS_IRPCONTEXT PtrIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		PtrIoStackLocation = NULL;
	PIRP					PtrIrp;
	LARGE_INTEGER			ByteOffset;
	uint32					WriteLength = 0, TruncatedWriteLength = 0;
	uint32					NumberBytesWritten = 0;
	PFILE_OBJECT			PtrFileObject = NULL;
	PXIFS_FCB				PtrFCB = NULL;
	PXIFS_CCB				PtrCCB = NULL;
	PXIFS_VCB				PtrVCB = NULL;
	LARGE_INTEGER			OldFileSize;


	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;

	PERESOURCE				PtrResourceAcquired = NULL;
	void					*PtrSystemBuffer = NULL;

	
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					PagingIo = FALSE;
	BOOLEAN					NonBufferedIo = FALSE;
	BOOLEAN					SynchronousIo = FALSE;
	BOOLEAN					IsThisADeferredWrite = FALSE;
	BOOLEAN					WritingAtEndOfFile = FALSE;
	XIFS_IO_CONTEXT			LocalIoContext;
	BOOLEAN					DelayedOP = FALSE;
	BOOLEAN					IsEOFProcessing = FALSE;
	BOOLEAN					bLazyWrite = FALSE;
	BOOLEAN					bRecursiveCall = FALSE;
	BOOLEAN					bModWrite = FALSE;
	BOOLEAN					bExtendValidData = FALSE;
	BOOLEAN					bExtendFile = FALSE;
	BOOLEAN					btempExtend = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonWrite \n"));


	ASSERT_IRPCONTEXT(PtrIrpContext);

	ASSERT(PtrIrpContext->Irp);
	PtrIrp = PtrIrpContext->Irp;
	PtrIoStackLocation = IoGetCurrentIrpStackLocation(PtrIrp);
	ASSERT(PtrIoStackLocation);

	PtrFileObject = PtrIoStackLocation->FileObject;
	ASSERT(PtrFileObject);


	
	if(IoGetTopLevelIrp() == (PIRP) FSRTL_MOD_WRITE_TOP_LEVEL_IRP){
		//DbgPrint("Set New Top Level!!\n");
		bModWrite = TRUE;
		XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	}
	
  	
	if (PtrIoStackLocation->MinorFunction & IRP_MN_COMPLETE) {
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
			("XifsdMdlComplete IrpContext(%p) Irp(%p) \n", PtrIrpContext, PtrIrp));

		XixFsdMdlComplete(PtrIrpContext, PtrIrp, PtrIoStackLocation, FALSE);
		RC = STATUS_SUCCESS;
		return RC;
	}

	
	if (PtrIoStackLocation->MinorFunction & IRP_MN_DPC) {
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
			("PostRequest IrpContext(%p) Irp(%p) \n", PtrIrpContext, PtrIrp));
		
		RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);
		return RC;
	}

	
	PtrFileObject = PtrIoStackLocation->FileObject;
	ASSERT(PtrFileObject);

	TypeOfOpen = XixFsdDecodeFileObject( PtrFileObject, &PtrFCB, &PtrCCB );

	if((TypeOfOpen == UnopenedFileObject) || (TypeOfOpen == UserDirectoryOpen))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			(" Un supported type %ld\n", TypeOfOpen));
		
		if(TypeOfOpen == UserDirectoryOpen){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("WRITE Un supported by Dir Name(%wZ) CCB(%p) %ld\n",  &PtrFCB->FCBName, PtrCCB,TypeOfOpen));
		}

		RC = STATUS_INVALID_DEVICE_REQUEST;
		XixFsdCompleteRequest(PtrIrpContext, RC, 0);
		return RC;
	}







	ByteOffset = PtrIoStackLocation->Parameters.Write.ByteOffset;
	WriteLength = PtrIoStackLocation->Parameters.Write.Length;

	CanWait = ((PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_WAIT) ? TRUE : FALSE);
	PagingIo = ((PtrIrp->Flags & IRP_PAGING_IO) ? TRUE : FALSE);
	NonBufferedIo = ((PtrIrp->Flags & IRP_NOCACHE) ? TRUE : FALSE);
	SynchronousIo = ((PtrFileObject->Flags & FO_SYNCHRONOUS_IO) ? TRUE : FALSE);


	// Added by ILGU HONG for readonly 09052006
	if(PtrFCB->PtrVCB->IsVolumeWriteProctected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Is Write protected %lx .\n", PtrFCB));
		DbgPrint(" !!! Failed Write Has No Lock File (%wZ) FileSize (%I64d) Req Offset(%I64d) Size(%ld).\n", 
				&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, ByteOffset.QuadPart, WriteLength);
		RC = STATUS_MEDIA_WRITE_PROTECTED;
		XixFsdCompleteRequest(PtrIrpContext, RC, 0);
		return RC;
	}
	// Added by ILGU HONG end


	if(XifsdCheckFlagBoolean(PtrFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
		RC = STATUS_INVALID_DEVICE_REQUEST;
		return RC;
	}


	if(TypeOfOpen == UserVolumeOpen){
		if(!XifsdCheckFlagBoolean(PtrFCB->PtrVCB->VCBFlags, XIFSD_VCB_FLAGS_VOLUME_LOCKED)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Has not Host Lock %lx .\n", PtrFCB));
			DbgPrint(" !!! Failed Write Has No Lock Volume (%wZ) FileSize (%I64d) Req Offset(%I64d) Size(%ld).\n", 
					&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, ByteOffset.QuadPart, WriteLength);
			RC = STATUS_INVALID_DEVICE_REQUEST;
			XixFsdCompleteRequest(PtrIrpContext, RC, 0);
			return RC;		
		}

	}else{
		if(PtrFCB->HasLock != FCB_FILE_LOCK_HAS)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Has not Host Lock %lx .\n", PtrFCB));
			DbgPrint(" !!! Failed Write Has No Lock File (%wZ) FileSize (%I64d) Req Offset(%I64d) Size(%ld).\n", 
					&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, ByteOffset.QuadPart, WriteLength);
			RC = STATUS_INVALID_DEVICE_REQUEST;
			XixFsdCompleteRequest(PtrIrpContext, RC, 0);
			return RC;
		}
	}



	

	
	if(!CanWait && PagingIo && (!XifsdCheckFlagBoolean(PtrFCB->Type, XIFS_FD_TYPE_PAGE_FILE))){
		if(!XifsdCheckFlagBoolean(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT)
			&& (IoGetTopLevelIrp() != (PIRP) FSRTL_MOD_WRITE_TOP_LEVEL_IRP)){
			
			//DbgPrint("Set New Top Level 2!!\n");

			CanWait = TRUE;
			XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
		}
	}
	

	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("[WRITE PARM Lot(%I64d) Irp(%p)]: Wait(%s) PagingIo(%s) NonBuffIo(%s) SynchIo(%s) BOffset(%I64d) Length(%ld)\n",
				PtrFCB->LotNumber,
				PtrIrp,
				((CanWait == TRUE)?"TRUE":"FALSE"),
				((PagingIo == TRUE)?"TRUE":"FALSE"),
				((NonBufferedIo == TRUE)?"TRUE":"FALSE"),
				((SynchronousIo == TRUE)?"TRUE":"FALSE"),
				ByteOffset.QuadPart,
				WriteLength));




	if (WriteLength == 0) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			(" XifsdCommonWrite  Write Length == 0 FCB(%x) FileName(%wZ)\n", PtrFCB, &PtrFCB->FCBName));
		RC = STATUS_SUCCESS;
		XixFsdCompleteRequest(PtrIrpContext, RC, 0);
		return RC;

	}


	if(PtrCCB && XifsdCheckFlagBoolean( PtrCCB->CCBFlags, XIFSD_CCB_DISMOUNT_ON_CLOSE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			(" XixFsdCommonWrite  CCB->FLAGS set DisMountOnClose File FCB(%x) FileName(%wZ)\n", PtrFCB, &PtrFCB->FCBName));
		RC = STATUS_UNSUCCESSFUL;
		XixFsdCompleteRequest(PtrIrpContext, RC, 0);
		return RC;
	}

/*
	if(WriteLength > XIFSD_MAX_IO_LIMIT){
		WriteLength = XIFSD_MAX_IO_LIMIT;
	}
*/

	ASSERT_FCB(PtrFCB);
	PtrVCB = PtrFCB->PtrVCB;
	ASSERT_VCB(PtrVCB);

	if(TypeOfOpen == UserVolumeOpen){
		NonBufferedIo = TRUE;
	}



	if(PagingIo){
		DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
			(" XixFsdCommonWrite  Try Acquire resouce PagingIoResource FCB(%x) FileName(%wZ)\n", PtrFCB, &PtrFCB->FCBName));
		
		if(!ExAcquireResourceSharedLite(PtrFCB->PagingIoResource, CanWait)){
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				("PostRequest IrpContext(%p) Irp(%p) \n", PtrIrpContext, PtrIrp));
			
			RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);
			return RC;
		}
		PtrResourceAcquired = PtrFCB->PagingIoResource;
	}else{
		DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
			(" XixFsdCommonWrite  Try Acquire resouce Resource FCB(%x) FileName(%wZ)\n", PtrFCB, &PtrFCB->FCBName));
		
		if(!ExAcquireResourceExclusiveLite(PtrFCB->Resource, CanWait)){
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				("PostRequest IrpContext(%p) Irp(%p) \n", PtrIrpContext, PtrIrp));
			
			RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);
			return RC;
		}
		PtrResourceAcquired = PtrFCB->Resource;
	}


	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
		("Do IrpContext(%p) Irp(%p) \n", PtrIrpContext, PtrIrp));


	try {
		
		if ((TypeOfOpen != UserVolumeOpen) || (NULL == PtrCCB) ||
			!XifsdCheckFlagBoolean( PtrCCB->CCBFlags, XIFSD_CCB_DISMOUNT_ON_CLOSE))  {

			if(!XixFsdVerifyFcbOperation( PtrIrpContext, PtrFCB )){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					(" XixFsdCommonWrite  Fail VerifyFcbOperation FileName(%wZ)\n", &PtrFCB->FCBName));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);				
			}
		}


		if ((ByteOffset.LowPart == FILE_WRITE_TO_END_OF_FILE) && (ByteOffset.HighPart == 0xFFFFFFFF)) {
         	WritingAtEndOfFile = TRUE;
         	ByteOffset.QuadPart = PtrFCB->FileSize.QuadPart;
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				(" Write End of File FCB(%x) FileSize(%I64d)\n", PtrFCB, PtrFCB->FileSize));
		}


		if(TypeOfOpen <= UserVolumeOpen){
			if((WritingAtEndOfFile) 
				|| (ByteOffset.QuadPart + WriteLength > PtrFCB->FileSize.QuadPart)){
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}


		if(TypeOfOpen == UserVolumeOpen){
			
			 
			if(ByteOffset.QuadPart >= PtrFCB->FileSize.QuadPart){
				
				 PtrIrp->IoStatus.Information = 0;
				 try_return( RC = STATUS_SUCCESS );
			}

			if(WriteLength > (PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart)){

				WriteLength = (uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart);
			}


			if(WriteLength ==0){
				RC = STATUS_SUCCESS;
				PtrIrp->IoStatus.Status = RC;
				NumberBytesWritten = (uint32)PtrIrp->IoStatus.Information = 0;
				try_return(RC);					
			}			


			if((PtrIrpContext->IoContext == NULL) 
				|| !XifsdCheckFlagBoolean(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT)
			)
			{
				if(CanWait){
					PtrIrpContext->IoContext = &LocalIoContext;
					RtlZeroMemory(&LocalIoContext, sizeof(XIFS_IO_CONTEXT));
					KeInitializeEvent(&(PtrIrpContext->IoContext->SyncEvent), NotificationEvent, FALSE);

				}else{
					PtrIrpContext->IoContext = ExAllocatePoolWithTag(NonPagedPool,
															sizeof(XIFS_IO_CONTEXT), 
															TAG_IOCONTEX);

					XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT);

					RtlZeroMemory(PtrIrpContext->IoContext, sizeof(XIFS_IO_CONTEXT));
					PtrIrpContext->IoContext->Async.Resource = PtrResourceAcquired;
					PtrIrpContext->IoContext->Async.ResourceThreadId = ExGetCurrentResourceThread();
					PtrIrpContext->IoContext->Async.RequestedByteCount = WriteLength;
				}
			}


			PtrIrpContext->IoContext->pIrpContext = PtrIrpContext;
			PtrIrpContext->IoContext->pIrp = PtrIrp;


			RC = XixFsdNonCachedIo (
			 		PtrFCB,
					PtrIrpContext,
					PtrIrp,
					ByteOffset.QuadPart,
					WriteLength,
					FALSE,
					TypeOfOpen,
					PtrResourceAcquired
					);



				if(RC == STATUS_PENDING){
					PtrIrpContext->Irp = NULL;
					PtrResourceAcquired = FALSE;
				}else{
					if (NT_SUCCESS( RC )) {				

						if (SynchronousIo && !PagingIo){
							PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + WriteLength;
						}

						NumberBytesWritten = WriteLength;
						
					}					
					
				}

				try_return(RC);

		}


		if (TypeOfOpen == UserFileOpen) {



			if(!PagingIo)
			{
				//
				//  We check whether we can proceed
				//  based on the state of the file oplocks.
				//
				DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					("CALL  FsRtlCheckOplock:XifsdCommonWrite \n"));
				RC = FsRtlCheckOplock( &PtrFCB->FCBOplock,
										   PtrIrp,
										   PtrIrpContext,
										   XixFsdOplockComplete,
										   XixFsdPrePostIrp );

				//
				//  If the result is not STATUS_SUCCESS then the Irp was completed
				//  elsewhere.
				//

				if (!NT_SUCCESS(RC)) {
					DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
						("return PENDING  FsRtlCheckOplock:XifsdCommonWrite \n"));

					PtrIrp = NULL;
					PtrIrpContext = NULL;

					try_return( RC );
				}

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" Check Oplock FCB(%x) LotNumber(%I64d)\n", PtrFCB, PtrFCB->LotNumber));



				if ((PtrFCB->FCBFileLock != NULL) &&
					!FsRtlCheckLockForWriteAccess( PtrFCB->FCBFileLock, PtrIrp )) {

					try_return( RC = STATUS_FILE_LOCK_CONFLICT );
				}

			}			


			IsThisADeferredWrite = ((PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_DEFERRED_WRITE) ? TRUE : FALSE);

			if (!NonBufferedIo
				&&!XifsdCheckFlagBoolean( PtrFileObject->Flags, FO_WRITE_THROUGH ) ) {

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" Set Defered Write\n"));

				if (!CcCanIWrite(PtrFileObject, WriteLength, CanWait, IsThisADeferredWrite)) {
					BOOLEAN Retry = FALSE;

					DbgPrint("!!! SET Defered IO\n");
					Retry = XifsdCheckFlagBoolean(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_DEFERRED_WRITE);
					XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_DEFERRED_WRITE);

					CcDeferWrite(PtrFileObject, XixFsdDeferredWriteCallBack, PtrIrpContext, PtrIrp, WriteLength, Retry);
					PtrIrp = NULL;
					PtrIrpContext = NULL;				
					try_return(RC = STATUS_PENDING);
				}
			}


			if (NonBufferedIo && !PagingIo && (PtrFCB->SectionObject.DataSectionObject != NULL)) {
				LARGE_INTEGER FileSize;
				FileSize.QuadPart = PtrFCB->FileSize.QuadPart;

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" Clear Cache(%x) FileSize(%I64d)\n", PtrFCB, PtrFCB->FileSize));


				if(!ExAcquireResourceSharedLite(PtrFCB->PagingIoResource, CanWait)){
					RC = STATUS_CANT_WAIT;
					try_return(RC);
				}

				CcFlushCache(&(PtrFCB->SectionObject), &ByteOffset, WriteLength, &(PtrIrp->IoStatus));

				ExReleaseResourceLite(PtrFCB->PagingIoResource);

		
				
				DbgPrint("CcFlush  7 File(%wZ)\n", &PtrFCB->FCBFullPath);
				if (!NT_SUCCESS(RC = PtrIrp->IoStatus.Status)) {
					try_return(RC);
				}
				
				if(!ExAcquireResourceExclusiveLite(PtrFCB->PagingIoResource, CanWait)){
					RC = STATUS_CANT_WAIT;
					try_return(RC);
				}

	
				CcPurgeCacheSection(&(PtrFCB->SectionObject), (WritingAtEndOfFile ? &(FileSize) : &(ByteOffset)),
											WriteLength, FALSE);

				ExReleaseResourceLite(PtrFCB->PagingIoResource);
			}

			
			if( PagingIo
				&& !NonBufferedIo){
				if(ByteOffset.QuadPart >= PtrFCB->ValidDataLength.QuadPart){
					
					 PtrIrp->IoStatus.Information = 0;
					 try_return( RC = STATUS_SUCCESS );
				}

				if(WriteLength > (PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart)){
	
					DbgPrint("!!!! !NonBufferedIo & PagingIo  (%wZ)Change FileSize(%I64x) ValidData(%I64x) ByteOffset(%I64x) WriteLength from(%ld) To(%ld)\n",
							&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, PtrFCB->ValidDataLength.QuadPart, ByteOffset.QuadPart,
							WriteLength, (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart) );


					WriteLength = (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
				}
				
			}
			


			if(PtrFCB->LazyWriteThread == PsGetCurrentThread())
			{
				bLazyWrite = TRUE;
			}

		
		
			if(bLazyWrite)
			{

				if(XifsdCheckFlagBoolean(PtrFCB->Flags, FSRTL_FLAG_USER_MAPPED_FILE)){
					if((ByteOffset.QuadPart > PtrFCB->ValidDataLength.QuadPart) &&
						(ByteOffset.QuadPart < PtrFCB->FileSize.QuadPart) ) 
					{
						if(ByteOffset.QuadPart > 
							((PtrFCB->ValidDataLength.QuadPart + PAGE_SIZE -1) & ~((LONGLONG)(PAGE_SIZE -1)) ) )
						{
							DbgPrint(" !! Lazy Write Conflict Offset(0x%I64X):ValidLen(0x%I64X):FileSize(0x%I64X):Length(%ld)\n",
								ByteOffset.QuadPart,
								PtrFCB->ValidDataLength.QuadPart,
								PtrFCB->FileSize.QuadPart,
								WriteLength);

							RC = STATUS_FILE_LOCK_CONFLICT;
							try_return(RC);
						}
					}
				}
			}


			if(XifsdCheckFlagBoolean(PtrIrp->Flags, IRP_SYNCHRONOUS_PAGING_IO)
				&& XifsdCheckFlagBoolean(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_RECURSIVE_CALL)
			){
				bRecursiveCall = TRUE;
			}



			if(!PagingIo && (ByteOffset.QuadPart + WriteLength > PtrFCB->FileSize.QuadPart)){
				bExtendFile = TRUE;
			}


			if(bExtendFile){
				// Check Allocation Size
				uint64 Offset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				(" Non Buffered File Io Check  FileSize(%x) FileSize(%I64d)\n", PtrFCB, PtrFCB->FileSize));

				if(WritingAtEndOfFile){
					RequestStartOffset = PtrFCB->FileSize.QuadPart;
					Offset = PtrFCB->FileSize.QuadPart + WriteLength;
				}else{
					RequestStartOffset = ByteOffset.QuadPart;
					Offset = ByteOffset.QuadPart + WriteLength;
				}

				CurentEndIndex = GetIndexOfLogicalAddress(PtrVCB->LotSize, (PtrFCB->RealAllocationSize-1));
				EndLotIndex = GetIndexOfLogicalAddress(PtrVCB->LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
				
					if(!CanWait){
						RC = STATUS_CANT_WAIT;
						try_return(RC);
					}

					LotCount = EndLotIndex - CurentEndIndex;
					
					
					RC = XixFsAddNewLot(
								PtrVCB, 
								PtrFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)PtrFCB->AddrLot,
								PtrFCB->AddrLotSize,
								&(PtrFCB->AddrStartSecIndex)
								);
					

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}

					if(LotCount != AllocatedLotCount){
						if(PtrFCB->RealAllocationSize < RequestStartOffset){
							RC = STATUS_INSUFFICIENT_RESOURCES;
							try_return(RC);
						}else{
							 WriteLength = (uint32)(Offset - PtrFCB->RealAllocationSize);
						}
					}

		
					
				}
				
				PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;


				if(CcIsFileCached(PtrFileObject))
				{
					CcSetFileSizes(PtrFileObject, (PCC_FILE_SIZES)&PtrFCB->AllocationSize);
					DbgPrint(" 12 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
				}

			}
			
		

			if(!bLazyWrite && !bRecursiveCall) 
			{
				
				if((ByteOffset.QuadPart + WriteLength) > PtrFCB->ValidDataLength.QuadPart)
				{
					
					

					if(PagingIo){

						DbgPrint("!!!! Toplevel Paging IO Change WriteLength from(%ld) To(%ld)\n", 
								WriteLength, 
								(uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart) );

						
						if(ByteOffset.QuadPart >= PtrFCB->ValidDataLength.QuadPart){
							 PtrIrp->IoStatus.Information = 0;
							 try_return( RC = STATUS_SUCCESS );
						}

						if(WriteLength > (PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart)){
							DbgPrint("!!!! Change WriteLength from(%ld) To(%ld)\n", 
								WriteLength, 
								(uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart) );

							WriteLength = (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
						}
						
						
					}else{
						bExtendValidData = TRUE;
					}
				}
				
			
			}


			

			if(!bExtendValidData){
				if(((ByteOffset.QuadPart + WriteLength) > PtrFCB->ValidDataLength.QuadPart)){
					if(ByteOffset.QuadPart >= PtrFCB->ValidDataLength.QuadPart){

						DbgPrint("!!!! !bExtendValidData (%wZ) End FileSize(%I64x) ValidData(%I64x) ByteOffset(%I64x) WriteLength from(%ld)\n",
							&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, PtrFCB->ValidDataLength.QuadPart, ByteOffset.QuadPart,
							WriteLength);


						 PtrIrp->IoStatus.Information = 0;
						 try_return( RC = STATUS_SUCCESS );
					}

					if(WriteLength > (PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart)){
						DbgPrint("!!!! !bExtendValidData (%wZ)Change FileSize(%I64x) ValidData(%I64x) ByteOffset(%I64x) WriteLength from(%ld) To(%ld)\n",
							&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, PtrFCB->ValidDataLength.QuadPart, ByteOffset.QuadPart,
							WriteLength, (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart) );

						WriteLength = (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
					}
				}
			}



			if (!NonBufferedIo) {
				BOOLEAN	bLocalOpDone = FALSE;

				if (PtrFileObject->PrivateCacheMap == NULL) {

					DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
					(" Create Cache file FileObject(%p) FileSize(%x) FileSize(%I64d)\n",PtrFileObject, PtrFCB, PtrFCB->FileSize));

					//DbgPrint("2 Initialize Cache Map of Name (%wZ) \n", &PtrFCB->FCBFullPath);
					
					CcInitializeCacheMap(PtrFileObject, (PCC_FILE_SIZES)(&(PtrFCB->AllocationSize)),
						FALSE,		// We will not utilize pin access for this file
						&(XiGlobalData.XifsCacheMgrCallBacks), // callbacks
						PtrFCB);		// The context used in callbacks

					CcSetReadAheadGranularity( PtrFileObject, XIXFS_READ_AHEAD_GRANULARITY );
					XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_CACHED_FILE);
					RC = STATUS_SUCCESS;
				}


				
				/*
				if( ((WritingAtEndOfFile) || ((uint64)(ByteOffset.QuadPart + WriteLength) > PtrFCB->RealAllocationSize))
				)
				{
					if(CanWait){
						
						uint64 Offset = 0;
						uint64 RequestStartOffset = 0;
						uint32 EndLotIndex = 0;
						uint32 CurentEndIndex = 0;
						uint32 RequestStatIndex = 0;
						uint32 LotCount = 0;
						uint32 AllocatedLotCount = 0;

						DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
						(" Non Buffered File Io Check  FileSize(%x) FileSize(%I64d)\n", PtrFCB, PtrFCB->FileSize));

						if(WritingAtEndOfFile){
							RequestStartOffset = PtrFCB->FileSize.QuadPart;
							Offset = PtrFCB->FileSize.QuadPart + WriteLength;
						}else{
							RequestStartOffset = ByteOffset.QuadPart;
							Offset = ByteOffset.QuadPart + WriteLength;
						}

						CurentEndIndex = GetIndexOfLogicalAddress(PtrVCB->LotSize, (PtrFCB->RealAllocationSize-1));
						EndLotIndex = GetIndexOfLogicalAddress(PtrVCB->LotSize, Offset);
						
						if(EndLotIndex > CurentEndIndex){
							
							LotCount = EndLotIndex - CurentEndIndex;
							
							
							RC = XixFsAddNewLot(
										PtrVCB, 
										PtrFCB, 
										CurentEndIndex, 
										LotCount, 
										&AllocatedLotCount,
										(uint8 *)PtrFCB->AddrLot,
										PtrFCB->AddrLotSize,
										&(PtrFCB->AddrStartSecIndex)
										);
							

							if(!NT_SUCCESS(RC)){
								RC = STATUS_INSUFFICIENT_RESOURCES;
								try_return(RC);
							}

							if(LotCount != AllocatedLotCount){
								if(PtrFCB->RealAllocationSize < RequestStartOffset){
									RC = STATUS_INSUFFICIENT_RESOURCES;
									try_return(RC);
								}else{
									 WriteLength = (uint32)(Offset - PtrFCB->RealAllocationSize);
								}
							}

							if(CcIsFileCached(PtrFileObject))
							{
								CcSetFileSizes(PtrFileObject, (PCC_FILE_SIZES)&PtrFCB->AllocationSize);
								DbgPrint(" 12 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
							}
							
						}
					}else{
						DbgPrint(" !!!BufferedIo Fail (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
						RC = STATUS_CANT_WAIT;
						try_return(RC);
					}

				}
				*/	
			
				if(	bExtendValidData &&
					(WritingAtEndOfFile || ((ByteOffset.QuadPart + WriteLength) > PtrFCB->ValidDataLength.QuadPart))
				){
					if(CanWait ){
						XifsdLockFcb(NULL, PtrFCB);
						IsEOFProcessing =  (!XifsdCheckFlagBoolean(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
												|| XixFsdCheckEofWrite(PtrFCB, &ByteOffset, WriteLength));

									
						if(IsEOFProcessing){
							OldFileSize.QuadPart = PtrFCB->FileSize.QuadPart;
							XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO);

							if((ByteOffset.QuadPart + WriteLength) > PtrFCB->FileSize.QuadPart){
								PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;
								CcSetFileSizes(PtrFileObject, (PCC_FILE_SIZES)&PtrFCB->AllocationSize);
								DbgPrint(" 16 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
							}
						}

						XifsdUnlockFcb(NULL, PtrFCB);
					}else{
						RC = STATUS_FILE_LOCK_CONFLICT;
						try_return(RC);
					}


				}else{
					if((ByteOffset.QuadPart + WriteLength) > PtrFCB->FileSize.QuadPart){
						
						

						
						if(ByteOffset.QuadPart >= PtrFCB->FileSize.QuadPart){
							 PtrIrp->IoStatus.Information = 0;
							 try_return( RC = STATUS_SUCCESS );
						}

						if(WriteLength > (PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart)){
							DbgPrint("!!!! Change WriteLength from(%ld) To(%ld)\n", 
								WriteLength, 
								(uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart) );

							WriteLength = (uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart);
						}
						
						
						/*
						XifsdLockFcb(NULL, PtrFCB);
						OldFileSize.QuadPart = PtrFCB->FileSize.QuadPart;
						PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;
						btempExtend = TRUE;
						XifsdUnlockFcb(NULL, PtrFCB);
						*/
					}
				}





				if( !bLazyWrite
					&& !bRecursiveCall
					&&(ByteOffset.QuadPart  > PtrFCB->ValidDataLength.QuadPart))
				{
					LARGE_INTEGER	ZeroStart = {0,0};
					LARGE_INTEGER	BeyondZeroEnd = {0,0};
					uint32			byteToZero = 0;

					byteToZero = (uint32)(ByteOffset.LowPart - PtrFCB->ValidDataLength.LowPart);
					ZeroStart.LowPart = (PtrFCB->ValidDataLength.LowPart + (PtrVCB->SectorSize -1)) & ~(PtrVCB->SectorSize -1);
					BeyondZeroEnd.QuadPart = ((ULONGLONG) PtrFCB->ValidDataLength.QuadPart + byteToZero + (PtrVCB->SectorSize -1)) & ~((LONGLONG)PtrVCB->SectorSize -1);
					
					if(ZeroStart.QuadPart != BeyondZeroEnd.QuadPart){
						CcZeroData(PtrFileObject, &ZeroStart, &BeyondZeroEnd, CanWait);
					}
				}


				if (PtrIoStackLocation->MinorFunction & IRP_MN_MDL) {
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
						(" Try Writ MDL ByteOffset(%I64d) WriteLength(%ld)\n", ByteOffset.QuadPart, WriteLength));

					CcPrepareMdlWrite(PtrFileObject, &ByteOffset, WriteLength, &(PtrIrp->MdlAddress), &(PtrIrp->IoStatus));
					NumberBytesWritten = (uint32)PtrIrp->IoStatus.Information;
					RC = PtrIrp->IoStatus.Status;
					bLocalOpDone = TRUE;
				}else{
					
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
						(" Try CcCopyWrite ByteOffset(%I64d) WriteLength(%ld)\n", ByteOffset.QuadPart, WriteLength));


					

					PtrSystemBuffer = XixFsdGetCallersBuffer(PtrIrp);

					ASSERT(PtrSystemBuffer);
					if (!CcCopyWrite(PtrFileObject, &(ByteOffset), WriteLength, CanWait, PtrSystemBuffer)) {

						try_return(RC = STATUS_CANT_WAIT);
					} else {
						PtrIrp->IoStatus.Status = RC;
						PtrIrp->IoStatus.Information = NumberBytesWritten = WriteLength;
						NumberBytesWritten = WriteLength;
						bLocalOpDone = TRUE;
					}
				}
				
				if(NT_SUCCESS(RC)){
					
					NumberBytesWritten = WriteLength;

					XifsdLockFcb(NULL, PtrFCB);

					if(PtrFCB->WriteStartOffset == -1){
						PtrFCB->WriteStartOffset = ByteOffset.QuadPart;
						XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
					}

					if(PtrFCB->WriteStartOffset > ByteOffset.QuadPart){
						PtrFCB->WriteStartOffset = ByteOffset.QuadPart;
						XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
					}

					
					if (SynchronousIo && !PagingIo && bLocalOpDone ) {

						PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + WriteLength;
		
					}
				
					if (!PagingIo){
						XifsdSetFlag(PtrFileObject->Flags, FO_FILE_MODIFIED);
					}



					if(IsEOFProcessing){
						
						if (!PagingIo){
							PtrFileObject->Flags |= FO_FILE_SIZE_CHANGED;
						}
						
						if(PtrFCB->FileSize.QuadPart < (ByteOffset.QuadPart + WriteLength)){
							PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;
						}
							
						

						if(bExtendValidData){

							PtrFCB->ValidDataLength.QuadPart = ByteOffset.QuadPart + WriteLength;

							if(CcIsFileCached(PtrFileObject))
							{
								CcGetFileSizePointer(PtrFileObject)->QuadPart = ByteOffset.QuadPart + WriteLength;
								DbgPrint(" 13 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
							}
						
						}	

						
					}




					XifsdUnlockFcb(NULL, PtrFCB);
				}else{
					if(IsEOFProcessing){
						XifsdLockFcb(NULL, PtrFCB);
						PtrFCB->FileSize.QuadPart = OldFileSize.QuadPart;
						XifsdUnlockFcb(NULL, PtrFCB);
					}



				}


				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
						(" PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart(%I64d) \n",
						PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart));

				try_return(RC);

			} else {

				

				

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" Do Non Buffered File Io Check  FCB(%x) LotNumber(%I64d) FileSize(%I64d)\n", 
							PtrFCB, PtrFCB->LotNumber, PtrFCB->FileSize));



				

				if((PtrIrpContext->IoContext == NULL) 
					|| !XifsdCheckFlagBoolean(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT)
				)
				{
					if(CanWait){
						PtrIrpContext->IoContext = &LocalIoContext;
						RtlZeroMemory(&LocalIoContext, sizeof(XIFS_IO_CONTEXT));
						KeInitializeEvent(&(PtrIrpContext->IoContext->SyncEvent), NotificationEvent, FALSE);

					}else{
						PtrIrpContext->IoContext = ExAllocatePoolWithTag(NonPagedPool,
																sizeof(XIFS_IO_CONTEXT), 
																TAG_IOCONTEX);

						XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT);

						RtlZeroMemory(PtrIrpContext->IoContext, sizeof(XIFS_IO_CONTEXT));
						PtrIrpContext->IoContext->Async.Resource = PtrResourceAcquired;
						PtrIrpContext->IoContext->Async.ResourceThreadId = ExGetCurrentResourceThread();
						PtrIrpContext->IoContext->Async.RequestedByteCount = WriteLength;
					}
				}

				/*
				if( ((WritingAtEndOfFile) || ((uint64)(ByteOffset.QuadPart + WriteLength) > PtrFCB->RealAllocationSize))
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
					(" Non Buffered File Io Check  FileSize(%x) FileSize(%I64d)\n", PtrFCB, PtrFCB->FileSize));

					if(WritingAtEndOfFile){
						RequestStartOffset = PtrFCB->FileSize.QuadPart;
						Offset = PtrFCB->FileSize.QuadPart + WriteLength;
					}else{
						RequestStartOffset = ByteOffset.QuadPart;
						Offset = ByteOffset.QuadPart + WriteLength;
					}

					CurentEndIndex = GetIndexOfLogicalAddress(PtrVCB->LotSize, (PtrFCB->RealAllocationSize-1));
					EndLotIndex = GetIndexOfLogicalAddress(PtrVCB->LotSize, Offset);
					
					if(EndLotIndex > CurentEndIndex){

						if(!CanWait){
							RC = XixFsdDoDelayProcessing(PtrIrpContext, XixFsdDoDelayNonCachedIo);
							DelayedOP = TRUE;
							try_return(RC);
						}
						
						LotCount = EndLotIndex - CurentEndIndex;
						
						
						RC = XixFsAddNewLot(
									PtrVCB, 
									PtrFCB, 
									CurentEndIndex, 
									LotCount, 
									&AllocatedLotCount,
									(uint8 *)PtrFCB->AddrLot,
									PtrFCB->AddrLotSize,
									&(PtrFCB->AddrStartSecIndex)
									);
						

						if(!NT_SUCCESS(RC)){
							RC = STATUS_INSUFFICIENT_RESOURCES;
							try_return(RC);
						}

						if(LotCount != AllocatedLotCount){
							if(PtrFCB->RealAllocationSize < RequestStartOffset){
								RC = STATUS_INSUFFICIENT_RESOURCES;
								try_return(RC);
							}else{
								 WriteLength = (uint32)(Offset - PtrFCB->RealAllocationSize);
							}
						}

						if(CcIsFileCached(PtrFileObject))
						{
							CcSetFileSizes(PtrFileObject, (PCC_FILE_SIZES)&PtrFCB->AllocationSize);
							DbgPrint(" 14 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
						}
						
					}else if(!CanWait){
					
						PtrIrpContext->IoContext->Async.RequestedByteCount = WriteLength;
					}

				}
				*/
				
				if((!bLazyWrite)
					&& !bRecursiveCall)
				{
					if(ByteOffset.QuadPart  > PtrFCB->ValidDataLength.QuadPart){
						LARGE_INTEGER	ZeroStart = {0,0};
						LARGE_INTEGER	BeyondZeroEnd = {0,0};
						uint32			byteToZero = 0;

						byteToZero = (uint32)(ByteOffset.LowPart - PtrFCB->ValidDataLength.LowPart);
						ZeroStart.LowPart = (PtrFCB->ValidDataLength.LowPart + (PtrVCB->SectorSize -1)) & ~(PtrVCB->SectorSize -1);
						BeyondZeroEnd.QuadPart = ((ULONGLONG) PtrFCB->ValidDataLength.QuadPart + byteToZero + (PtrVCB->SectorSize -1)) & ~((LONGLONG)PtrVCB->SectorSize -1);
						
						if(ZeroStart.QuadPart != BeyondZeroEnd.QuadPart){
							CcZeroData(PtrFileObject, &ZeroStart, &BeyondZeroEnd, CanWait);
						}
					}
				}
				


				if(
					bExtendValidData &&
					(WritingAtEndOfFile || ((ByteOffset.QuadPart + WriteLength) > PtrFCB->ValidDataLength.QuadPart))
				){

					if(CanWait){
						XifsdLockFcb(NULL, PtrFCB);
						IsEOFProcessing =  (!XifsdCheckFlagBoolean(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
												|| XixFsdCheckEofWrite(PtrFCB, &ByteOffset, WriteLength));



									
						if(IsEOFProcessing){
							OldFileSize.QuadPart = PtrFCB->FileSize.QuadPart;
							XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO);

							if((ByteOffset.QuadPart + WriteLength) > PtrFCB->FileSize.QuadPart){
								PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;
								
								
								if(CcIsFileCached(PtrFileObject) )
								{
									CcSetFileSizes(PtrFileObject, (PCC_FILE_SIZES)&PtrFCB->AllocationSize);
									DbgPrint(" 16 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
								}
								

							}
						}

						XifsdUnlockFcb(NULL, PtrFCB);
					}else{
						RC = STATUS_CANT_WAIT;
						try_return(RC);
					}

				}else{
					if((ByteOffset.QuadPart + WriteLength) > PtrFCB->FileSize.QuadPart){
						
	
						
						if(ByteOffset.QuadPart >= PtrFCB->FileSize.QuadPart){
							 PtrIrp->IoStatus.Information = 0;
							 try_return( RC = STATUS_SUCCESS );
						}

						if(WriteLength > (PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart)){
							DbgPrint("!!!!   111 Change WriteLength from(%ld) To(%ld)\n", 
								WriteLength, 
								(uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart) );

							WriteLength = (uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart);
						}
						
						/*
						XifsdLockFcb(NULL, PtrFCB);
						OldFileSize.QuadPart = PtrFCB->FileSize.QuadPart;
						PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;
						btempExtend = TRUE;
						XifsdUnlockFcb(NULL, PtrFCB);
						*/
					}
				}







				PtrIrpContext->IoContext->pIrpContext = PtrIrpContext;
				PtrIrpContext->IoContext->pIrp = PtrIrp;


				XifsdSetFlag(PtrFileObject->Flags, FO_FILE_MODIFIED);

				RC = XixFsdNonCachedIo (
			 			PtrFCB,
						PtrIrpContext,
						PtrIrp,
						ByteOffset.QuadPart,
						WriteLength,
						FALSE,
						TypeOfOpen,
						PtrResourceAcquired
						);

				if(RC == STATUS_PENDING){
					PtrIrpContext->Irp = NULL;
					PtrResourceAcquired = FALSE;
				}else{
					
					if(RC == STATUS_ACCESS_DENIED)
					{
						if(!CanWait){
							RC = XixFsdDoDelayProcessing(PtrIrpContext, XixFsdDoDelayNonCachedIo);
							DelayedOP = TRUE;
							try_return(RC);
						}else {
							DbgPrint(" !!! Failed Write File (%wZ) FileSize (%I64d) Req Offset(%I64d) Size(%ld).\n", 
								&PtrFCB->FCBName, PtrFCB->FileSize.QuadPart, ByteOffset.QuadPart, WriteLength);
							
						}

					}
					
					if (NT_SUCCESS( RC )) {

						NumberBytesWritten = WriteLength;
						XifsdLockFcb(NULL, PtrFCB);

						if(PtrFCB->WriteStartOffset == -1){
							PtrFCB->WriteStartOffset = ByteOffset.QuadPart;
							XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
						}

						if(PtrFCB->WriteStartOffset > ByteOffset.QuadPart){
							PtrFCB->WriteStartOffset = ByteOffset.QuadPart;
							XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
						}

						
						if (SynchronousIo && !PagingIo ) {

							PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + WriteLength;
			
						}
					
						if (!PagingIo){
							XifsdSetFlag(PtrFileObject->Flags, FO_FILE_MODIFIED);
						}



						if(IsEOFProcessing){
							if (!PagingIo){
								PtrFileObject->Flags |= FO_FILE_SIZE_CHANGED;
							}


							if((ByteOffset.QuadPart + WriteLength) > PtrFCB->FileSize.QuadPart){
									PtrFCB->FileSize.QuadPart = ByteOffset.QuadPart + WriteLength;
									//DbgPrint(" 44 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
							}
							
							if(bExtendValidData){
								
								PtrFCB->ValidDataLength.QuadPart = ByteOffset.QuadPart + WriteLength;
								//DbgPrint(" 44 Changed File (%wZ) ValidData (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->ValidDataLength.QuadPart);
							
								if(CcIsFileCached(PtrFileObject))
								{
									CcGetFileSizePointer(PtrFileObject)->QuadPart = ByteOffset.QuadPart + WriteLength;
									//DbgPrint(" 44 Changed File (%wZ) Size (%I64d) .\n", &PtrFCB->FCBFullPath, PtrFCB->FileSize.QuadPart);
								}		
							}
							
				
											
						}



						XifsdUnlockFcb(NULL, PtrFCB);
					}else{
						if(IsEOFProcessing){
							XifsdLockFcb(NULL, PtrFCB);
							PtrFCB->FileSize.QuadPart = OldFileSize.QuadPart;
							XifsdUnlockFcb(NULL, PtrFCB);
						}



					}
					
				}



				try_return(RC);
			}

		}	

	} finally {
		// Post IRP if required
		DebugUnwind( "XixFsdCommonWrite" );

		if(IsEOFProcessing){

			XifsdLockFcb(NULL, PtrFCB);
			XixFsdFinishIoEof(PtrFCB);
			XifsdUnlockFcb(NULL, PtrFCB);

			if(!NT_SUCCESS(RC)){
				PtrFCB->FileSize.QuadPart = OldFileSize.QuadPart;
			}
		}


		/*
		if(btempExtend){
			XifsdLockFcb(NULL, PtrFCB);
			PtrFCB->FileSize.QuadPart = OldFileSize.QuadPart;
			btempExtend = TRUE;
			XifsdUnlockFcb(NULL, PtrFCB);

		}
		*/		




		if(!DelayedOP){

			if(PtrResourceAcquired){
				ExReleaseResourceLite(PtrResourceAcquired);
			}
			

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Result  XifsdCommonWrite  PtrIrp(%p) RC(%x)\n", PtrIrp, RC));

			// Post IRP if required
			if (RC == STATUS_CANT_WAIT) {
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					("PostRequest IrpContext(%p) Irp(%p) \n", PtrIrpContext, PtrIrp));
				
				RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);

			} else {
				XixFsdCompleteRequest(PtrIrpContext, RC, NumberBytesWritten );
			}
		}
	} // end of "finally" processing

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCommonWrite  RC(%x)\n", RC));

	return(RC);	
}



void 
XixFsdDeferredWriteCallBack (
		void		*Context1,			// Should be PtrIrpContext
		void		*Context2			// Should be PtrIrp
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIFS_IRPCONTEXT PtrIrpContext = (PXIFS_IRPCONTEXT)Context1;
	PIRP				  PtrIrp = (PIRP)Context2;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDeferredWriteCallBack \n"));

	// mark the IRP pending
	RC = IoMarkIrpPending(PtrIrp);

	// queue up the request
	ExInitializeWorkItem(&(PtrIrpContext->WorkQueueItem), XixFsdCommonDispatch, PtrIrpContext);
	ExQueueWorkItem(&(PtrIrpContext->WorkQueueItem), CriticalWorkQueue);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdDeferredWriteCallBack \n"));
	// return status pending
	return;
}







