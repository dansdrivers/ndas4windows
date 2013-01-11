#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"
#include "xixcore/lotaddr.h"
#include "xixcore/fileaddr.h"



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_FastIoCheckIfPossible)
#pragma alloc_text(PAGE, xixfs_FastIoRead)
#pragma alloc_text(PAGE, xixfs_FastIoWrite)
#pragma alloc_text(PAGE, xixfs_FastIoQueryBasicInfo)
#pragma alloc_text(PAGE, xixfs_FastIoQueryStdInfo)
#pragma alloc_text(PAGE, xixfs_FastIoQueryNetInfo)
#pragma alloc_text(PAGE, xixfs_FastIoLock)
#pragma alloc_text(PAGE, xixfs_FastIoUnlockSingle)
#pragma alloc_text(PAGE, xixfs_FastIoUnlockAll)
#pragma alloc_text(PAGE, xixfs_FastIoUnlockAllByKey)
#pragma alloc_text(PAGE, xixfs_FastIoAcqCreateSec)
#pragma alloc_text(PAGE, xixfs_FastIoRelCreateSec)
#pragma alloc_text(PAGE, xixfs_FastIoAcqModWrite)
#pragma alloc_text(PAGE, xixfs_FastIoRelModWrite)
#pragma alloc_text(PAGE, xixfs_FastIoAcqCcFlush)
#pragma alloc_text(PAGE, xixfs_FastIoRelCcFlush)
#pragma alloc_text(PAGE, xixfs_FastIoMdlRead)
#pragma alloc_text(PAGE, xixfs_FastIoPrepareMdlWrite)
#pragma alloc_text(PAGE, xixfs_AcqLazyWrite)
#pragma alloc_text(PAGE, xixfs_RelLazyWrite)
#pragma alloc_text(PAGE, xixfs_AcqReadAhead)
#pragma alloc_text(PAGE, xixfs_RelReadAhead)
#pragma alloc_text(PAGE, xixfs_NopAcqLazyWrite)
#pragma alloc_text(PAGE, xixfs_NopRelLazyWrite)
#pragma alloc_text(PAGE, xixfs_NopAcqReadAhead)
#pragma alloc_text(PAGE, xixfs_NopRelReadAhead)
#endif







BOOLEAN 
xixfs_FastIoCheckIfPossible(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN BOOLEAN					Wait,
	IN ULONG					LockKey,
	IN BOOLEAN					CheckForReadOperation,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	NTSTATUS		Status = STATUS_SUCCESS;
	PXIXFS_FCB 		pFCB = NULL;
	PXIXFS_CCB		pCCB = NULL;
	LARGE_INTEGER	LargeLength;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoCheckIfPossible\n"));

	ASSERT(FileObject);
	
	pCCB = FileObject->FsContext2;
	pFCB = FileObject->FsContext;
	ASSERT_FCB(pFCB);
		
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));	
	

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Type is diff %lx .\n", pFCB));
		IoStatus->Status = STATUS_INVALID_PARAMETER;
		IoStatus->Information = 0;
		return FALSE;
	}


	LargeLength.QuadPart = Length;

	try{

		if(CheckForReadOperation){

			
			if( (pFCB->FCBFileLock == NULL) ||
				FsRtlFastCheckLockForRead(pFCB->FCBFileLock, 
											FileOffset, 
											&LargeLength, 
											LockKey, 
											FileObject, 
											PsGetCurrentProcess() ))
			{
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Past Io Allowed %lx .\n", pFCB));

				try_return(RC = TRUE);
			}
			
		}else{
			
			ULONG	Reason = 0;

			if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
				try_return (RC = FALSE);
			}

			// Added by ILGU HONG for readonly 09082006
			if((pFCB->FCBFileLock != NULL) && (pFCB->PtrVCB->XixcoreVcb.IsVolumeWriteProtected)){
				try_return(RC = FALSE);
			}
			// Added by ILGU HONG for readonly end

			if((pFCB->FCBFileLock == NULL) || 
				FsRtlFastCheckLockForWrite(pFCB->FCBFileLock, 
											FileOffset, 
											&LargeLength, 
											LockKey, 
											FileObject, 
											PsGetCurrentProcess() ))
				{
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Past Io Allowed %lx .\n", pFCB));
					
					try_return (RC = TRUE);
				}
			
		}
		

	}finally{
		;
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoCheckIfPossible %lx .\n", pFCB));	
	
	return RC;
	
}

BOOLEAN 
xixfs_FastIoRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN BOOLEAN					Wait,
	IN ULONG					LockKey,
	OUT PVOID					Buffer,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIXFS_FCB		pFCB = NULL;
	PXIXFS_CCB		pCCB = NULL;
	PDEVICE_OBJECT 	pTargetVDO = NULL;
	LARGE_INTEGER	LastBytes;
	BOOLEAN			IsOpDone = FALSE;
	uint32			LocalLength = 0;
	uint32			PrePocessData = 0;
	BOOLEAN			LockFCB = FALSE;
	BOOLEAN			IsEOF = FALSE;
	PAGED_CODE();


	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoRead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("[Param]FileOffset(%I64d):Length(%Id) [Fcb]FileSize(%I64d):ValidLength(%I64d):AllocSize(%I64d)[fileObj]COff(%I64d).\n", 
		FileOffset->QuadPart, Length, 
		pFCB->FileSize.QuadPart, pFCB->ValidDataLength.QuadPart , pFCB->AllocationSize.QuadPart,
		FileObject->CurrentByteOffset.QuadPart));



	// Check if is it possible cached operation
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("XifsdFastIoRead  Invalid Type%lx .\n", pFCB));
		return FALSE;
	}

	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED)){
		return FALSE;
	}



	if(!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("XifsdFastIoRead  Invalid State%lx .\n", pFCB));
		return FALSE;
	}

	
	
	if (IoGetTopLevelIrp() != NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Is Not Top Level Irp %lx .\n", pFCB));
        return FALSE;
    }

	if(Length == 0)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Request size is 0 %lx .\n", pFCB));
		
		IoStatus->Status = STATUS_SUCCESS;
		IoStatus->Information = 0;
		IsOpDone = TRUE;
		goto error_done; 
	}


	

	FsRtlEnterFileSystem();

	if(!ExAcquireResourceSharedLite(pFCB->PagingIoResource, Wait)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail Acquire PagingIoResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IsOpDone = FALSE;
		return FALSE; 
	}

	if((FileObject->PrivateCacheMap == NULL) ||
		(pFCB->IsFastIoPossible == FastIoIsNotPossible))
	{
		//FsRtlIncrementCcFastReadNotPossible();
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("Fast Io is not possible %lx .\n", pFCB));
		
		//DbgPrint("error read out 3\n");
		IoStatus->Status = STATUS_UNSUCCESSFUL;
		IoStatus->Information = 0;
		IsOpDone = FALSE;
		goto error_done; 
	}


	XifsdLockFcb(NULL, pFCB);	
	LockFCB = TRUE;
	
	// Check File Length
	LastBytes.QuadPart = FileOffset->QuadPart + (LONGLONG)Length;
	
	IoStatus->Information = 0;

	if(LastBytes.QuadPart > pFCB->ValidDataLength.QuadPart){
		IsEOF = (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO) 
						|| xixfs_FileCheckEofWrite(pFCB, FileOffset, Length));
	}

	XifsdUnlockFcb(NULL, pFCB);
	LockFCB = FALSE;


	if(IsEOF){
        XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
		
		if(FileOffset->QuadPart >= pFCB->ValidDataLength.QuadPart){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				("Fail Request Size is Too Big %I64d , %lx .\n", FileOffset->QuadPart, pFCB));
			
			//DbgPrint("error read out 1\n");

			IoStatus->Status = STATUS_END_OF_FILE;
			IoStatus->Information = 0;
			IsOpDone = FALSE;
			goto error_done; 
		}
		
		if((FileOffset->QuadPart + Length) > pFCB->ValidDataLength.QuadPart){
			Length = (uint32)(pFCB->ValidDataLength.QuadPart - FileOffset->QuadPart);
			LastBytes.QuadPart = FileOffset->QuadPart + Length;
		}
		
	}

	




	

	if(Length == 0)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Request size is 0 %lx .\n", pFCB));
		
		IoStatus->Status = STATUS_SUCCESS;
		IoStatus->Information = 0;
		IsOpDone = TRUE;
		goto error_done; 
	}


	// Check Operation status



	if(pFCB->IsFastIoPossible == FastIoIsQuestionable )
	{
		PFAST_IO_DISPATCH FastIoDispatch;
		
		pTargetVDO = IoGetRelatedDeviceObject(FileObject);
		ASSERT(pTargetVDO);
		
		FastIoDispatch = pTargetVDO->DriverObject->FastIoDispatch;
		ASSERT(FastIoDispatch);

		if(!FastIoDispatch->FastIoCheckIfPossible(FileObject,
											FileOffset,
											Length,
											Wait,
											LockKey,
											TRUE, 
											IoStatus,
											pTargetVDO ))
		{
			//FsRtlIncrementCcFastReadNotPossible();
			//DbgPrint("error read out 2\n");
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail chek FastIoCheckIfPossible %lx .\n", pFCB));		
			IoStatus->Status = STATUS_ABANDONED;
			IoStatus->Information = 0;
			IsOpDone = FALSE;
			goto error_done; 
		}
		
	}
	
	
	IoSetTopLevelIrp( (PIRP) FSRTL_FAST_IO_TOP_LEVEL_IRP );

	try{

		ULONG PageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES( FileOffset->QuadPart, Length );
		IsOpDone = TRUE;
		
		if(Wait && ((LastBytes.HighPart | pFCB->FileSize.HighPart ) == 0)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("LastBytes.HighPart %ld pFCB->FileSize.LowPart %ld FileOffset->LowPart (%ld), Length %ld\n",
				LastBytes.HighPart , pFCB->FileSize.LowPart,  FileOffset->LowPart, Length));

			CcFastCopyRead( FileObject,
					                FileOffset->LowPart,
					                Length,
					                PageCount,
					                Buffer,
					                IoStatus );	
			
			


			ASSERT((IoStatus->Status == STATUS_END_OF_FILE) ||
				((LONGLONG)(FileOffset->QuadPart + IoStatus->Information) <= ((pFCB->ValidDataLength.QuadPart > pFCB->FileSize.QuadPart)?pFCB->ValidDataLength.QuadPart:pFCB->FileSize.QuadPart)) );
		}else{
			IsOpDone= CcCopyRead( FileObject,
							             FileOffset,
							             Length,
							             Wait,
							             Buffer,
							             IoStatus );

			ASSERT((IoStatus->Status == STATUS_END_OF_FILE) ||
				((LONGLONG)(FileOffset->QuadPart + IoStatus->Information) <= ((pFCB->ValidDataLength.QuadPart > pFCB->FileSize.QuadPart)?pFCB->ValidDataLength.QuadPart:pFCB->FileSize.QuadPart)) );
		}


	}except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                        ? EXCEPTION_EXECUTE_HANDLER
                                        : EXCEPTION_CONTINUE_SEARCH ){
		IsOpDone = FALSE;
	}
	
	IoSetTopLevelIrp( NULL );
error_done:	

	if(IsOpDone){
		FileObject->CurrentByteOffset.QuadPart = LastBytes.QuadPart;
	}



	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoRead (%I64d) IsOpDone(%s) .\n", pFCB->XixcoreFcb.LotNumber, ((IsOpDone)?"TRUE":"FALSE")));
	
	if(LockFCB)
		XifsdUnlockFcb(NULL, pFCB);
	
	if(IsEOF){
		XifsdLockFcb(NULL, pFCB);
		xixfs_FileFinishIoEof(pFCB);
		XifsdUnlockFcb(NULL, pFCB);
	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsOpDone;	
}



BOOLEAN 
xixfs_FastIoWrite(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER				FileOffset,
	IN ULONG						Length,
	IN BOOLEAN						Wait,
	IN ULONG						LockKey,
	OUT PVOID						Buffer,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	NTSTATUS		Status = STATUS_SUCCESS;
	PXIXFS_FCB		pFCB;
	PXIXFS_CCB		pCCB;
	BOOLEAN			IsOpDone = FALSE;
	BOOLEAN			IsEOF =FALSE;
	BOOLEAN			LockFCB = FALSE;
    LARGE_INTEGER 	NewFileSize;
    LARGE_INTEGER 	OldFileSize;
	LARGE_INTEGER	Offset;	




	PAGED_CODE();
	
	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));


	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_ALL),
		("[Param]FileOffset(%I64d):Length(%Id) [Fcb]FileSize(%I64d):ValidLength(%I64d):AllocSize(%I64d)[fileObj]COff(%I64d).\n", 
		FileOffset->QuadPart, Length, 
		pFCB->FileSize.QuadPart, pFCB->ValidDataLength.QuadPart , pFCB->AllocationSize.QuadPart,
		FileObject->CurrentByteOffset.QuadPart));




	// Check if is it possible cached operation
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("xixfs_FastIoWrite Invalid Type%lx .\n", pFCB));
		return FALSE;
	}

	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED)){
		return FALSE;
	}

	if(!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("xixfs_FastIoWrite  Invalid State%lx .\n", pFCB));
		return FALSE;
	}



	if (IoGetTopLevelIrp() != NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Is Not Top Level Irp %lx .\n", pFCB));
        return FALSE;
    }


	
	if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Has not Host Lock %lx .\n", pFCB));
		
		IoStatus->Status = STATUS_UNSUCCESSFUL;
        IoStatus->Information = 0;
		IsOpDone = FALSE;	
		return FALSE;
	}


	if(Length == 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Request size is 0 %lx .\n", pFCB));
		
		IoStatus->Status = STATUS_SUCCESS;
        IoStatus->Information = 0;
		IsOpDone = FALSE;
		return FALSE;
	}


	
	
	if(!XIXCORE_TEST_FLAGS(FileObject->Flags, FO_WRITE_THROUGH)
		&& CcCanIWrite(FileObject, Length, Wait, FALSE)
		&& CcCopyWriteWontFlush(FileObject, FileOffset, Length)
		&& XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){

		FsRtlEnterFileSystem();
		
		if(!ExAcquireResourceSharedLite(pFCB->PagingIoResource, Wait)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Can't Acquire PagingIoResource %lx .\n", pFCB));
			FsRtlExitFileSystem();
			IoStatus->Status = STATUS_ABANDONED;
			IoStatus->Information = 0;
			IsOpDone = FALSE;
			return IsOpDone; 
		}

		
		XifsdLockFcb(NULL, pFCB);
		LockFCB = TRUE;

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Do xixfs_FastIoWrite %lx .\n", pFCB));

		IoStatus->Status = STATUS_SUCCESS;
		IoStatus->Information = Length;
		
		NewFileSize.QuadPart = FileOffset->QuadPart + Length;
		
		Offset = *FileOffset;
		

		// Changed by ILGU HONG
		//if(Wait && (pFCB->AllocationSize.HighPart == 0))
		if(Wait && ( ((FileOffset->HighPart == 0) && (NewFileSize.HighPart ==0))
				|| ((FileOffset->HighPart <0) && (pFCB->AllocationSize.HighPart == 0)) ) )
		// changed by ILGU HONG END
		{

			// Check End of File and Update File Size
			if((FileOffset->HighPart <0) 
				|| (NewFileSize.QuadPart > pFCB->ValidDataLength.QuadPart))
			{
				
				
					
				if(FileOffset->HighPart <0){
					Offset = pFCB->FileSize;
					NewFileSize.QuadPart = Offset.QuadPart + Length;
				}
					
				IsEOF = (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO) 
						|| xixfs_FileCheckEofWrite(pFCB, &Offset, Length));
					
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				("Change Info NewFileSize(%I64d) NewOffSet(%ld) Length(%ld).\n", 
					NewFileSize.QuadPart, Offset.LowPart, Length));
				
			
				if(IsEOF){
					XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
					OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

					if(NewFileSize.QuadPart > pFCB->FileSize.QuadPart){
						pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
						
					}
				}

				if((uint64)(NewFileSize.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize){
					
					uint64 LastOffset = 0;
					uint64 RequestStartOffset = 0;
					uint32 EndLotIndex = 0;
					uint32 CurentEndIndex = 0;
					uint32 RequestStatIndex = 0;
					uint32 LotCount = 0;
					uint32 AllocatedLotCount = 0;

					

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" FileSize ceck LotNumber(%I64d) FileSize(%I64d)\n", pFCB->XixcoreFcb.LotNumber, pFCB->FileSize));

					
					RequestStartOffset = Offset.QuadPart;
					LastOffset = NewFileSize.QuadPart;
					

					CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pFCB->PtrVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
					EndLotIndex = xixcore_GetIndexOfLogicalAddress(pFCB->PtrVCB->XixcoreVcb.LotSize, LastOffset);
					

					// Allocate New Lot
					if(EndLotIndex > CurentEndIndex){
						LotCount = EndLotIndex - CurentEndIndex;
						

						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
								pFCB->XixcoreFcb.LotNumber, CurentEndIndex, LotCount, pFCB->XixcoreFcb.RealAllocationSize));
						
						XifsdUnlockFcb(NULL, pFCB);
						LockFCB = FALSE;

						
						Status = xixcore_AddNewLot(
										&(pFCB->PtrVCB->XixcoreVcb), 
										&(pFCB->XixcoreFcb), 
										CurentEndIndex, 
										LotCount, 
										&AllocatedLotCount,
										pFCB->XixcoreFcb.AddrLot, 
										pFCB->XixcoreFcb.AddrLotSize, 
										&(pFCB->XixcoreFcb.AddrStartSecIndex)
										);
						

						XifsdLockFcb(NULL, pFCB);
						LockFCB = TRUE;
						if(!NT_SUCCESS(Status)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
									pFCB->XixcoreFcb.LotNumber, CurentEndIndex, LotCount, pFCB->XixcoreFcb.RealAllocationSize));


							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("FAIL AddNewLot FCB LotNumber(%I64d) .\n", pFCB->XixcoreFcb.LotNumber));

						
							IoStatus->Status = Status;
							IoStatus->Information = 0;
							IsOpDone = FALSE;	
							goto error_done; 
						}

						pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;

						if(LotCount != AllocatedLotCount){
							if(pFCB->XixcoreFcb.RealAllocationSize < RequestStartOffset){
								DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("FAIL AddNewLot FCB LotNumber(%I64d) Request Lot(%ld) Allocated Lot(%ld) .\n", 
									pFCB->XixcoreFcb.LotNumber, LotCount, AllocatedLotCount));
								

								IoStatus->Status = STATUS_UNSUCCESSFUL;
								IoStatus->Information = 0;
								IsOpDone = FALSE;	
								goto error_done; 
							}else if(LastOffset > pFCB->XixcoreFcb.RealAllocationSize){
								 Length = (uint32)(pFCB->XixcoreFcb.RealAllocationSize - RequestStartOffset);
							}else {
								IoStatus->Status = STATUS_UNSUCCESSFUL;
								IoStatus->Information = 0;
								IsOpDone = FALSE;	
								goto error_done; 
							}
						}

						CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
						//DbgPrint(" 1 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					}
				}


				

				
				
			}	

			XifsdUnlockFcb(NULL, pFCB);
			LockFCB = FALSE;

			if((FileObject->PrivateCacheMap == NULL) ||
				(pFCB->IsFastIoPossible == FastIoIsNotPossible) ||
				(Offset.QuadPart >= (pFCB->ValidDataLength.QuadPart + 0x2000)) )
			{
				//FsRtlIncrementCcFastReadNotPossible();
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("Fast Io is not possible %lx .\n", pFCB));
				
				IoStatus->Status = STATUS_ABANDONED;
				IoStatus->Information = 0;
				IsOpDone = FALSE;
				goto error_done; 
			}


			if(pFCB->IsFastIoPossible == FastIoIsQuestionable){
				PDEVICE_OBJECT	pTargetVDO = NULL;
				PFAST_IO_DISPATCH FastIoDispatch = NULL;
				pTargetVDO = IoGetRelatedDeviceObject(FileObject);
				FastIoDispatch = pTargetVDO->DriverObject->FastIoDispatch;

				ASSERT(FastIoDispatch);
				ASSERT(FastIoDispatch->FastIoCheckIfPossible);

				if(!FastIoDispatch->FastIoCheckIfPossible(FileObject,
												&Offset,
												Length,
												TRUE,
												LockKey,
												FALSE, 
												IoStatus,
												pTargetVDO )) 
				{
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Fail chek FastIoCheckIfPossible %lx .\n", pFCB));
					
					IoStatus->Status = STATUS_ABANDONED;
					IoStatus->Information = 0;
					IsOpDone = FALSE;
					goto error_done; 
				}


			}
			

			//Update File Object Size // Need Update Allocation Size
			if(IsEOF) {
				CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
				//DbgPrint(" 1-1 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

			}


			IoSetTopLevelIrp( (PIRP) FSRTL_FAST_IO_TOP_LEVEL_IRP );

			try{
					if(Offset.QuadPart > pFCB->ValidDataLength.QuadPart){
						CcZeroData(FileObject,
								&pFCB->ValidDataLength,
								&Offset,
								TRUE 
								);
							
					}

	
					IoStatus->Status = STATUS_SUCCESS;
					CcFastCopyWrite(FileObject, 
									Offset.LowPart, 
									Length, 
									Buffer);
					
					
					IsOpDone = TRUE;

					



					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Add File data to the end FilSize(%I64d) StartOffSet(%ld) Length(%ld).\n", 
						pFCB->FileSize.QuadPart, Offset.LowPart, Length));


		

			}except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                                ? EXCEPTION_EXECUTE_HANDLER
                                                : EXCEPTION_CONTINUE_SEARCH ){
				IsOpDone = FALSE;
							
			}


			IoSetTopLevelIrp( NULL );

	
			
		}else{



			//DbgPrint("Big File Info NewFileSize(%I64d)  ValisdDataLen(%i64d) NewOffSet(%I64d) Length(%ld).\n", 
			//		NewFileSize.QuadPart, pFCB->ValidDataLength.QuadPart, Offset.QuadPart, Length);


			// Check End of File and Update File Size
			// Changed by ILGU HONG
			//if((FileOffset->HighPart <0) 
			//	|| (NewFileSize.LowPart > pFCB->ValidDataLength.LowPart))
			if((FileOffset->HighPart <0) 
				|| (NewFileSize.QuadPart > pFCB->ValidDataLength.QuadPart))
			// Changed by ILGU HONG END
			{
				

				
				if(FileOffset->HighPart < 0){
					Offset = pFCB->FileSize;
					NewFileSize.QuadPart = Offset.QuadPart + Length;
				}
			
				IsEOF = (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO) 
						|| xixfs_FileCheckEofWrite(pFCB, &Offset, Length));
				
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				("Change Info NewFileSize(%I64d) NewOffSet(%I64d) Length(%ld).\n", 
					NewFileSize.QuadPart, Offset.QuadPart, Length));
			
			
				if(IsEOF){
					XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
					OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

					if(NewFileSize.QuadPart > pFCB->FileSize.QuadPart){
						pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
						
					}
				}



				if((uint64)(NewFileSize.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize){
					
					uint64 LastOffset = 0;
					uint64 RequestStartOffset = 0;
					uint32 EndLotIndex = 0;
					uint32 CurentEndIndex = 0;
					uint32 RequestStatIndex = 0;
					uint32 LotCount = 0;
					uint32 AllocatedLotCount = 0;

					

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" FileSize ceck LotNumber(%I64d) FileSize(%I64d)\n", pFCB->XixcoreFcb.LotNumber, pFCB->FileSize));

					
					RequestStartOffset = Offset.QuadPart;
					LastOffset = NewFileSize.QuadPart;
					

					CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pFCB->PtrVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
					EndLotIndex = xixcore_GetIndexOfLogicalAddress(pFCB->PtrVCB->XixcoreVcb.LotSize, LastOffset);
					

					// Allocate New Lot
					if(EndLotIndex > CurentEndIndex){
						LotCount = EndLotIndex - CurentEndIndex;
						

						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
								pFCB->XixcoreFcb.LotNumber, CurentEndIndex, LotCount, pFCB->XixcoreFcb.RealAllocationSize));
						
						XifsdUnlockFcb(NULL, pFCB);
						LockFCB = FALSE;
						
						
						Status = xixcore_AddNewLot(
										&(pFCB->PtrVCB->XixcoreVcb), 
										&(pFCB->XixcoreFcb), 
										CurentEndIndex, 
										LotCount, 
										&AllocatedLotCount,
										pFCB->XixcoreFcb.AddrLot, 
										pFCB->XixcoreFcb.AddrLotSize,
										&(pFCB->XixcoreFcb.AddrStartSecIndex)
										);
						

						XifsdLockFcb(NULL, pFCB);
						LockFCB = TRUE;

						if(!NT_SUCCESS(Status)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
									pFCB->XixcoreFcb.LotNumber, CurentEndIndex, LotCount, pFCB->XixcoreFcb.RealAllocationSize));


							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("FAIL AddNewLot FCB LotNumber(%I64d) .\n", pFCB->XixcoreFcb.LotNumber));

						
							IoStatus->Status = Status;
							IoStatus->Information = 0;
							IsOpDone = FALSE;	
							goto error_done; 
						}
						pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;

						if(LotCount != AllocatedLotCount){
							if(pFCB->XixcoreFcb.RealAllocationSize < RequestStartOffset){
								DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("FAIL AddNewLot FCB LotNumber(%I64d) Request Lot(%ld) Allocated Lot(%ld) .\n", 
									pFCB->XixcoreFcb.LotNumber, LotCount, AllocatedLotCount));
								

								IoStatus->Status = STATUS_UNSUCCESSFUL;
								IoStatus->Information = 0;
								IsOpDone = FALSE;	
								goto error_done; 
							}else if (LastOffset >pFCB->XixcoreFcb.RealAllocationSize) {
								 Length = (uint32)(pFCB->XixcoreFcb.RealAllocationSize -RequestStartOffset );
							}else{
								IoStatus->Status = STATUS_UNSUCCESSFUL;
								IoStatus->Information = 0;
								IsOpDone = FALSE;	
								goto error_done; 
							}
						}
					}
				}

				

				
				
			}

			XifsdUnlockFcb(NULL, pFCB);
			LockFCB = FALSE;

			if((FileObject->PrivateCacheMap == NULL) ||
				(pFCB->IsFastIoPossible == FastIoIsNotPossible) ||
				(Offset.QuadPart >= (pFCB->ValidDataLength.QuadPart + 0x2000)) )
			{
				//FsRtlIncrementCcFastReadNotPossible();
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("Fast Io is not possible %lx .\n", pFCB));
				
				IoStatus->Status = STATUS_ABANDONED;
				IoStatus->Information = 0;
				IsOpDone = FALSE;
				goto error_done; 
			}


			if(pFCB->IsFastIoPossible == FastIoIsQuestionable){
				PDEVICE_OBJECT	pTargetVDO = NULL;
				PFAST_IO_DISPATCH FastIoDispatch = NULL;
				pTargetVDO = IoGetRelatedDeviceObject(FileObject);
				FastIoDispatch = pTargetVDO->DriverObject->FastIoDispatch;

				ASSERT(FastIoDispatch);
				ASSERT(FastIoDispatch->FastIoCheckIfPossible);

				if(!FastIoDispatch->FastIoCheckIfPossible(FileObject,
												&Offset,
												Length,
												TRUE,
												LockKey,
												FALSE, 
												IoStatus,
												pTargetVDO )) 
				{
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Fail chek FastIoCheckIfPossible %lx .\n", pFCB));
					
					IoStatus->Status = STATUS_ABANDONED;
					IoStatus->Information = 0;
					IsOpDone = FALSE;
					goto error_done; 
				}


			}


			//Update File Object Size // Need Update Allocation Size
			
			if(IsEOF) {
				CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
				//DbgPrint(" 2 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

			}


			IoSetTopLevelIrp( (PIRP) FSRTL_FAST_IO_TOP_LEVEL_IRP );

			try{
					// Changed by ILGU HONG
					//if(Offset.LowPart > pFCB->ValidDataLength.LowPart){
					if(Offset.QuadPart > pFCB->ValidDataLength.QuadPart){
					// Changed by ILGU HONG END
						CcZeroData(FileObject,
								&pFCB->ValidDataLength,
								&Offset,
								TRUE 
								);
							
					}

					IoStatus->Status = STATUS_SUCCESS;
					IsOpDone = CcCopyWrite(FileObject, 
									&Offset, 
									Length,
									Wait,
									Buffer);
					
					
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Add data FilSize(%I64d) StartOffSet(%ld) Length(%ld).\n", 
						pFCB->FileSize.QuadPart, Offset.LowPart, Length));
					

			}except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                                ? EXCEPTION_EXECUTE_HANDLER
                                                : EXCEPTION_CONTINUE_SEARCH ){
				IsOpDone = FALSE;
			}
			
			IoSetTopLevelIrp( NULL);
		}

	
	}else{

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("FAIL xixfs_FastIoWrite FCB LotNumber(%I64d) .\n", pFCB->XixcoreFcb.LotNumber));

		return FALSE;
	}
	
	if(IsOpDone ){
		FileObject->Flags |= FO_FILE_MODIFIED;
		FileObject->CurrentByteOffset.QuadPart = NewFileSize.QuadPart;
		

		XifsdLockFcb(NULL, pFCB);
		LockFCB = TRUE;


		if(pFCB->XixcoreFcb.WriteStartOffset == -1){
			pFCB->XixcoreFcb.WriteStartOffset = Offset.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->XixcoreFcb.WriteStartOffset > Offset.QuadPart){
			pFCB->XixcoreFcb.WriteStartOffset = Offset.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}
		
		
		

		if(IsEOF) {
			FileObject->Flags |= FO_FILE_SIZE_CHANGED;
			pFCB->ValidDataLength.QuadPart = NewFileSize.QuadPart;
			//DbgPrint(" 33 Changed File (%wZ) ValidDataLength (%I64d) .\n", &pFCB->FCBFullPath, pFCB->ValidDataLength.QuadPart);
			if(pFCB->FileSize.QuadPart < NewFileSize.QuadPart){
				pFCB->FileSize.QuadPart = NewFileSize.QuadPart;

				//DbgPrint(" 33 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
				CcGetFileSizePointer(FileObject)->QuadPart = pFCB->FileSize.QuadPart;
			}
			
			xixfs_FileFinishIoEof(pFCB);
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		}



		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;
		
	}else{
		if(IsEOF) {
			XifsdLockFcb(NULL, pFCB)
			LockFCB = TRUE;
			
			pFCB->FileSize.QuadPart = OldFileSize.QuadPart;
			//CcGetFileSizePointer(FileObject)->QuadPart = OldFileSize.QuadPart;
			//CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
			//DbgPrint(" 2-3 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
			
			xixfs_FileFinishIoEof(pFCB);
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

			XifsdUnlockFcb(NULL, pFCB);
			LockFCB = FALSE;
		}
	}



			
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoWrite %lx .\n", pFCB));
	if(LockFCB)
		XifsdUnlockFcb(NULL, pFCB);
	
				
	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsOpDone;	
	

error_done:


	
	if(LockFCB)
		XifsdUnlockFcb(NULL, pFCB);

	if(IsEOF) {
		XifsdLockFcb(NULL, pFCB)
		LockFCB = TRUE;
		
		pFCB->FileSize.QuadPart = OldFileSize.QuadPart;
		//CcGetFileSizePointer(FileObject)->QuadPart = OldFileSize.QuadPart;
		//CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
		//DbgPrint(" 2-2 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

		xixfs_FileFinishIoEof(pFCB);
		DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			(" 3 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;

	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsOpDone;	
}

BOOLEAN 
xixfs_FastIoQueryBasicInfo(
	IN PFILE_OBJECT					FileObject,
	IN BOOLEAN						Wait,
	OUT PFILE_BASIC_INFORMATION		Buffer,
	OUT PIO_STATUS_BLOCK 			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;
	BOOLEAN		IOResult = FALSE;
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		Length = sizeof(FILE_BASIC_INFORMATION);
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
		
	ASSERT(FileObject);
	TypeOfOpen = xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoQueryBasicInfo FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	
	if(TypeOfOpen <= UserVolumeOpen){
		return FALSE;
	}


	
	
	FsRtlEnterFileSystem();
	/*
	if(!ExAcquireResourceSharedLite(pFCB->Resource, Wait))
	{
		DebugTrace(1, (DEBUG_TRACE_FASTCTL| DEBUG_TRACE_ERROR), ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IOResult = FALSE;
		return IOResult;
	}
	*/

	if(!XifsdAcquireFcbShared(Wait, pFCB, FALSE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IOResult = FALSE;
		return IOResult;	
	}

	try{
		RtlZeroMemory((uint8 *)Buffer, Length );
		RC = xixfs_QueryBasicInfo( pFCB, 
							pCCB, 
							Buffer, 
							&Length 
							);
		
		IoStatus->Status = RC;
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail XifsdQueryBasicInfo %lx .\n", pFCB));
			IoStatus->Information = 0;
			IOResult = FALSE;
		}else{
			IoStatus->Information = sizeof(FILE_BASIC_INFORMATION);
			IOResult = TRUE;
		}

		;
	}finally{
		XifsdReleaseFcb(Wait, pFCB);
	}


	//ExReleaseResourceLite(pFCB->Resource);
	
	FsRtlExitFileSystem();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoQueryBasicInfo %lx .\n", pFCB));

	return IOResult;	
}

BOOLEAN 
xixfs_FastIoQueryStdInfo(
	IN PFILE_OBJECT						FileObject,
	IN BOOLEAN							Wait,
	OUT PFILE_STANDARD_INFORMATION 	Buffer,
	OUT PIO_STATUS_BLOCK 				IoStatus,
	IN PDEVICE_OBJECT					DeviceObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;
	BOOLEAN		IOResult = FALSE;
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		Length = sizeof(FILE_STANDARD_INFORMATION);
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();

	ASSERT(FileObject);
	TypeOfOpen = xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_FastIoQueryStdInfo FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	
	if(TypeOfOpen <= UserVolumeOpen){
		return FALSE;
	}

	

	FsRtlEnterFileSystem();
	/*
	if(!ExAcquireResourceSharedLite(pFCB->Resource, Wait))
	{
		DebugTrace(1, (DEBUG_TRACE_FASTCTL| DEBUG_TRACE_ERROR), ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IOResult = FALSE;
		return IOResult;
	}
	*/
	if(!XifsdAcquireFcbShared(Wait, pFCB, FALSE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IOResult = FALSE;
		return IOResult;	
	}
	
	try{

		RtlZeroMemory((uint8 *)Buffer, Length );

		RC = xixfs_QueryStandardInfo( pFCB, 
							pCCB, 
							Buffer, 
							&Length 
							);
		
		IoStatus->Status = RC;
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail xixfs_FastIoQueryStdInfo %lx .\n", pFCB));
			IoStatus->Information = 0;
			IOResult = FALSE;
		}else{
			IoStatus->Information = sizeof(FILE_STANDARD_INFORMATION);
			IOResult = TRUE;
		}

		;
	}finally{
		XifsdReleaseFcb(Wait, pFCB);
		//ExReleaseResourceLite(pFCB->Resource);
	}



	FsRtlExitFileSystem();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Exit xixfs_FastIoQueryStdInfo %lx .\n", pFCB));
	return IOResult;	
}


BOOLEAN 
xixfs_FastIoQueryNetInfo(
	IN PFILE_OBJECT						FileObject,
	IN BOOLEAN							Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION 	Buffer,
	OUT PIO_STATUS_BLOCK 				IoStatus,
	IN PDEVICE_OBJECT					DeviceObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;
	BOOLEAN		IOResult = FALSE;
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		Length = sizeof(FILE_NETWORK_OPEN_INFORMATION);
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
	ASSERT(FileObject);
	TypeOfOpen = xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_FastIoQueryNetInfo FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));

	if(TypeOfOpen <= UserVolumeOpen){
		return FALSE;
	}
	
	
	FsRtlEnterFileSystem();
	/*
	if(!ExAcquireResourceSharedLite(pFCB->Resource, Wait))
	{
		DebugTrace(1, (DEBUG_TRACE_FASTCTL| DEBUG_TRACE_ERROR), ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IOResult = FALSE;
		return IOResult;
	}
	*/
	
	if(!XifsdAcquireFcbShared(Wait, pFCB, FALSE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		IOResult = FALSE;
		return IOResult;	
	}

	try{
		RtlZeroMemory((uint8 *)Buffer, Length );
		
		RC = xixfs_QueryNetworkInfo( pFCB, 
							pCCB, 
							Buffer, 
							&Length 
							);
		
		IoStatus->Status = RC;
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail XifsdFastIoQueryNetInfo %lx .\n", pFCB));
			IoStatus->Information = 0;
			IOResult = FALSE;
		}else{
			IoStatus->Information = sizeof(FILE_NETWORK_OPEN_INFORMATION);
			IOResult = TRUE;
		}

		;
	}finally{
		XifsdReleaseFcb(Wait, pFCB);
	}


	//ExReleaseResourceLite(pFCB->Resource);
	FsRtlExitFileSystem();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoQueryNetInfo %lx .\n", pFCB));
	return IOResult;	
}

BOOLEAN 
xixfs_FastIoLock(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN PLARGE_INTEGER			Length,
	PEPROCESS					ProcessId,
	ULONG						Key,
	BOOLEAN						FailImmediately,
	BOOLEAN						ExclusiveLock,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	PXIXFS_FCB 		pFCB = NULL;
	PXIXFS_CCB 		pCCB = NULL;
	

	PAGED_CODE();
	
	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoLock FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Type not support! %lx .\n", pFCB));
		IoStatus->Status = STATUS_INVALID_PARAMETER;
		IoStatus->Information = 0;
		return FALSE;
	}

/*
	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
			
		RC = XifsdLotLock(TRUE, pFCB->PtrVCB, pFCB->PtrVCB->TargetDeviceObject, pFCB->LotNumber, TRUE, FALSE);
		if( NT_SUCCESS(RC)){
			pFCB->HasLock = FCB_FILE_LOCK_HAS;
		}else{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("has not host lock! %lx .\n", pFCB));
			IoStatus->Status = STATUS_ACCESS_DENIED;
			IoStatus->Information = 0;
			return FALSE;
		}

	}
*/


	FsRtlEnterFileSystem();
	if(!ExAcquireResourceSharedLite(pFCB->Resource, TRUE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		RC = FALSE;
		return RC;
	}

	


	try{
		
		if(pFCB->FCBFileLock == NULL){
			pFCB->FCBFileLock = FsRtlAllocateFileLock(NULL, NULL);

			if(!pFCB->FCBFileLock){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail FsRtlAllocateFileLock! %lx .\n", pFCB));
				IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
				IoStatus->Information = 0;
				RC = FALSE;
				try_return(RC);
			}
		}

		if(RC = FsRtlFastLock(pFCB->FCBFileLock, 
						FileObject,
						FileOffset,
						Length,
						ProcessId,
						Key,
						FailImmediately,
						ExclusiveLock,
						IoStatus,
						NULL,
						FALSE 
						))
		{
			if(pFCB->IsFastIoPossible == FastIoIsPossible){
				XifsdLockFcb(TRUE,pFCB);
				pFCB->IsFastIoPossible =xixfs_CheckFastIoPossible(pFCB);
				XifsdUnlockFcb(TRUE,pFCB);
			}
		}


		


		;
	}finally{
		DebugUnwind(XifsdFastIoLock);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit xixfs_FastIoLock %lx .\n", pFCB));
	}
	
	return RC;
	
}


BOOLEAN 
xixfs_FastIoUnlockSingle(
	IN PFILE_OBJECT			FileObject,
	IN PLARGE_INTEGER		FileOffset,
	IN PLARGE_INTEGER		Length,
	PEPROCESS				ProcessId,
	ULONG					Key,
	OUT PIO_STATUS_BLOCK	IoStatus,
	IN PDEVICE_OBJECT		DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	PXIXFS_FCB pFCB = NULL;
	PXIXFS_CCB pCCB = NULL;
		
	PAGED_CODE();

	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoUnlockSingle FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Type not support! %lx .\n", pFCB));
		IoStatus->Status = STATUS_INVALID_PARAMETER;
		IoStatus->Information = 0;
		return FALSE;
	}

/*
	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("has not host lock! %lx .\n", pFCB));
		IoStatus->Status = STATUS_ACCESS_DENIED;
		IoStatus->Information = 0;
		return FALSE;
	}
*/
	FsRtlEnterFileSystem();
	if(!ExAcquireResourceSharedLite(pFCB->Resource, TRUE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		RC = FALSE;
		return RC;
	}


	try{
		
		if(pFCB->FCBFileLock == NULL){
			pFCB->FCBFileLock = FsRtlAllocateFileLock(NULL, NULL);

			if(!pFCB->FCBFileLock){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail FsRtlAllocateFileLock! %lx .\n", pFCB));
				IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
				IoStatus->Information = 0;
				RC = FALSE;
				try_return(RC);
			}
		}

		RC = TRUE;
		 IoStatus->Status = FsRtlFastUnlockSingle( pFCB->FCBFileLock,
                                                  FileObject,
                                                  FileOffset,
                                                  Length,
                                                  ProcessId,
                                                  Key,
                                                  NULL,
                                                  FALSE );

		if (!FsRtlAreThereCurrentFileLocks(pFCB->FCBFileLock ) &&
			(pFCB->IsFastIoPossible != FastIoIsPossible)) 
		{
			XifsdLockFcb(TRUE,pFCB);
			pFCB->IsFastIoPossible = xixfs_CheckFastIoPossible(pFCB);
			XifsdUnlockFcb(TRUE,pFCB);
		}
		


		;
	}finally{
		DebugUnwind(XifsdFastIoUnlockSingle);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit xixfs_FastIoUnlockSingle %lx .\n", pFCB));
	}
	
	return RC;
}

BOOLEAN 
xixfs_FastIoUnlockAll(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS						ProcessId,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	PXIXFS_FCB pFCB = NULL;
	PXIXFS_CCB pCCB = NULL;

	PAGED_CODE();
	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoUnlockAll FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Type not support! %lx .\n", pFCB));
		IoStatus->Status = STATUS_INVALID_PARAMETER;
		IoStatus->Information = 0;
		return FALSE;
	}
/*
	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("has not host lock! %lx .\n", pFCB));
		IoStatus->Status = STATUS_ACCESS_DENIED;
		IoStatus->Information = 0;
		return FALSE;
	}
*/	
	
	FsRtlEnterFileSystem();
	if(!ExAcquireResourceSharedLite(pFCB->Resource, TRUE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		RC = FALSE;
		return RC;
	}

	try{
		
		if(pFCB->FCBFileLock == NULL){
			pFCB->FCBFileLock = FsRtlAllocateFileLock(NULL, NULL);

			if(!pFCB->FCBFileLock){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail FsRtlAllocateFileLock! %lx .\n", pFCB));
				IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
				IoStatus->Information = 0;
				RC = FALSE;
				try_return(RC);
			}
		}

		RC = TRUE;
		 IoStatus->Status = FsRtlFastUnlockAll( pFCB->FCBFileLock,
                                               FileObject,
                                               ProcessId,
                                               NULL );

		if (!FsRtlAreThereCurrentFileLocks(pFCB->FCBFileLock ) &&
			(pFCB->IsFastIoPossible != FastIoIsPossible)) 
		{
			XifsdLockFcb(TRUE,pFCB);
			pFCB->IsFastIoPossible = xixfs_CheckFastIoPossible(pFCB);
			XifsdUnlockFcb(TRUE,pFCB);
		}
		

		;
	}finally{
		DebugUnwind(XifsdFastIoUnlockAll);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit xixfs_FastIoUnlockAll %lx .\n", pFCB));
	}
	
	return RC;
}

BOOLEAN 
xixfs_FastIoUnlockAllByKey(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS					ProcessId,
	ULONG						Key,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	PXIXFS_FCB pFCB = NULL;
	PXIXFS_CCB pCCB = NULL;

	PAGED_CODE();
	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoUnlockAllByKey FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Type not support! %lx .\n", pFCB));
		IoStatus->Status = STATUS_INVALID_PARAMETER;
		IoStatus->Information = 0;
		return FALSE;
	}

	/*
	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("has not host lock! %lx .\n", pFCB));
		IoStatus->Status = STATUS_ACCESS_DENIED;
		IoStatus->Information = 0;
		return FALSE;
	}
	*/
	
	
	FsRtlEnterFileSystem();
	if(!ExAcquireResourceSharedLite(pFCB->Resource, TRUE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Can't Acquire FCBResource %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		RC = FALSE;
		return RC;
	}

	try{
		
		if(pFCB->FCBFileLock == NULL){
			pFCB->FCBFileLock = FsRtlAllocateFileLock(NULL, NULL);

			if(!pFCB->FCBFileLock){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail FsRtlAllocateFileLock! %lx .\n", pFCB));
				IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
				IoStatus->Information = 0;
				RC = FALSE;
				try_return(RC);
			}
		}

		RC = TRUE;
		 IoStatus->Status = FsRtlFastUnlockAllByKey( pFCB->FCBFileLock,
                                                    FileObject,
                                                    ProcessId,
                                                    Key,
                                                    NULL );

		if (!FsRtlAreThereCurrentFileLocks(pFCB->FCBFileLock ) &&
			(pFCB->IsFastIoPossible != FastIoIsPossible)) 
		{
			XifsdLockFcb(TRUE,pFCB);
			pFCB->IsFastIoPossible = xixfs_CheckFastIoPossible(pFCB);
			XifsdUnlockFcb(TRUE,pFCB);
		}
		


		;
	}finally{
		DebugUnwind(XifsdFastIoUnlockAllByKey);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit xixfs_FastIoUnlockAllByKey %lx .\n", pFCB));
	}
	
	return RC;
}

void 
xixfs_FastIoAcqCreateSec(
	IN PFILE_OBJECT			FileObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;

	PAGED_CODE();

	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoAcqCreateSec FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));

	
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, TRUE);
	}
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoAcqCreateSec %lx .\n", pFCB));
}


void 
xixfs_FastIoRelCreateSec(
	IN PFILE_OBJECT			FileObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;
	
	PAGED_CODE();
	
	ASSERT(FileObject);
	pCCB = FileObject->FsContext2;
	pFCB = FileObject->FsContext;
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoRelCreateSec FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		ExReleaseResourceLite(pFCB->PagingIoResource);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoRelCreateSec %lx .\n", pFCB));
}


NTSTATUS 
xixfs_FastIoAcqModWrite(
	IN PFILE_OBJECT						FileObject,
	IN PLARGE_INTEGER					EndingOffset,
	OUT PERESOURCE						*ResourceToRelease,
	IN PDEVICE_OBJECT					DeviceObject
)
{
	BOOLEAN		AcquireFile = FALSE;
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;

	PAGED_CODE();

	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);


	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return STATUS_UNSUCCESSFUL;
	//}
	// Added End

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoAcqModWrite .\n"));

	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("AcqModWrite Use PagingIoResource FCB(%x)\n", &pFCB));
		*ResourceToRelease = pFCB->PagingIoResource;
	}else {
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("AcqModWrite Use Resource FCB(%x) \n", &pFCB));

		*ResourceToRelease = pFCB->Resource;
	}

	//AcquireFile = ExAcquireResourceExclusiveLite(*ResourceToRelease , FALSE);
	AcquireFile = ExAcquireResourceSharedLite(*ResourceToRelease , FALSE);
	if(AcquireFile){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("xixfs_FastIoAcqModWrite SUCCESS \n"));
	}else{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("xixfs_FastIoAcqModWrite Fail \n"));
		*ResourceToRelease = NULL;
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoAcqModWrite Status(%s)\n", ((AcquireFile)?"SUCCESS":"FAIL")));
	
	return ((AcquireFile)?STATUS_SUCCESS:STATUS_CANT_WAIT);
}


NTSTATUS 
xixfs_FastIoRelModWrite(
	IN PFILE_OBJECT					FileObject,
	IN PERESOURCE					ResourceToRelease,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;

	PAGED_CODE();


	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return STATUS_UNSUCCESSFUL;
	//}
	// Added End

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoRelModWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	if(ResourceToRelease != NULL)
		ExReleaseResourceLite(ResourceToRelease);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoRelModWrite %lx .\n", pFCB));

	return TRUE;
}


NTSTATUS 
xixfs_FastIoAcqCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;

	PAGED_CODE();


	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_FastIoAcqCcFlush FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoAcqCcFlush %lx .\n", pFCB));
	return STATUS_SUCCESS;
}

NTSTATUS 
xixfs_FastIoRelCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;
	
	PAGED_CODE();

	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoRelCcFlush FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		ExReleaseResourceLite(pFCB->PagingIoResource);
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoRelCcFlush %lx .\n", pFCB));
	return STATUS_SUCCESS;
}



BOOLEAN 
xixfs_FastIoMdlRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIXFS_FCB		pFCB = NULL;
	PXIXFS_CCB		pCCB = NULL;
	PDEVICE_OBJECT 	pTargetVDO = NULL;
	LARGE_INTEGER	LastBytes;
	BOOLEAN			IsOpDone = FALSE;
	BOOLEAN			LockFCB = FALSE;
	BOOLEAN			IsEOF = FALSE;
	PAGED_CODE();

	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoMdlRead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		return FALSE;
	}

	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED)){
		return FALSE;
	}

	if(Length == 0)
	{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("File Size is 0 FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
		IoStatus->Status = STATUS_SUCCESS;
		IoStatus->Information = 0;
		return TRUE;
	}



	FsRtlEnterFileSystem();

	if(!ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Acquire Fail PagingIoResource 0 %lx .\n", pFCB));
		FsRtlExitFileSystem();
		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		return FALSE;
	}

	// Check Operation status
	if((FileObject->PrivateCacheMap == NULL) ||
		(pFCB->IsFastIoPossible == FastIoIsNotPossible))
	{
		//FsRtlIncrementCcFastReadNotPossible();
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Paging request is not possible %lx .\n", pFCB));

		IoStatus->Status = STATUS_ABANDONED;
		IoStatus->Information = 0;
		goto error_out;
	}

	XifsdLockFcb(NULL,pFCB);
	LockFCB = TRUE;

	LastBytes.QuadPart = FileOffset->QuadPart + (LONGLONG)Length;

	IoStatus->Information = 0;

	if(LastBytes.QuadPart > pFCB->ValidDataLength.QuadPart){
		IsEOF = (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO) 
						|| xixfs_FileCheckEofWrite(pFCB, FileOffset, Length));
	}

	XifsdUnlockFcb(NULL, pFCB);
	LockFCB = FALSE;


	if(IsEOF){
		
		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
		
		if(FileOffset->QuadPart >= pFCB->ValidDataLength.QuadPart){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				("Fail Request Size is Too Big %I64d , %lx .\n", FileOffset->QuadPart, pFCB));
			
			IoStatus->Status = STATUS_END_OF_FILE;
			IoStatus->Information = 0;
			IsOpDone = FALSE;
			goto error_out; 
		}

		if( (FileOffset->QuadPart + Length) > pFCB->ValidDataLength.QuadPart) {
			Length = (uint32)(pFCB->ValidDataLength.QuadPart - FileOffset->QuadPart);
			LastBytes.QuadPart = FileOffset->QuadPart + Length;
		}
		
	
	}


	if(Length == 0)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Request size is 0 %lx .\n", pFCB));
		
		IoStatus->Status = STATUS_SUCCESS;
		IoStatus->Information = 0;
		IsOpDone = TRUE;
		goto error_out; 
	}



	if(pFCB->IsFastIoPossible == FastIoIsQuestionable )
	{
		PFAST_IO_DISPATCH FastIoDispatch;
		
		pTargetVDO = IoGetRelatedDeviceObject(FileObject);
		ASSERT(pTargetVDO);
		
		FastIoDispatch = pTargetVDO->DriverObject->FastIoDispatch;
		ASSERT(FastIoDispatch);

		if(!FastIoDispatch->FastIoCheckIfPossible(FileObject,
											FileOffset,
											Length,
											TRUE,
											LockKey,
											TRUE, 
											IoStatus,
											pTargetVDO ))
		{
			//FsRtlIncrementCcFastReadNotPossible();
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Paging request is not possible %lx .\n", pFCB));
	
			IoStatus->Status = STATUS_ABANDONED;
			IoStatus->Information = 0;
			goto error_out;		
		}
		
	}

	IoSetTopLevelIrp( (PIRP) FSRTL_FAST_IO_TOP_LEVEL_IRP );
	try{
		
		IoStatus->Status = STATUS_SUCCESS;
		CcMdlRead( FileObject, FileOffset, Length, MdlChain, IoStatus );
		IsOpDone = TRUE;

		
	}except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                        ? EXCEPTION_EXECUTE_HANDLER
                                        : EXCEPTION_CONTINUE_SEARCH ){
		IsOpDone = FALSE;
	
	}

	IoSetTopLevelIrp(NULL);

	if(LockFCB) XifsdUnlockFcb(NULL, pFCB);

	if(IsEOF){
		XifsdLockFcb(NULL,pFCB);
		xixfs_FileFinishIoEof(pFCB);
		XifsdUnlockFcb(NULL, pFCB);
	}

	if(IsOpDone){
		FileObject->CurrentByteOffset.QuadPart = LastBytes.QuadPart;
	}


	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoMdlRead %lx .\n", pFCB));
	return IsOpDone;

error_out:
	

	
	if(LockFCB) XifsdUnlockFcb(NULL, pFCB);

	if(IsEOF){
		XifsdLockFcb(NULL,pFCB);
		xixfs_FileFinishIoEof(pFCB);
		XifsdUnlockFcb(NULL, pFCB);
	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XifsdFastIoMdlRead %lx .\n", pFCB));
	return IsOpDone;
}


BOOLEAN 
xixfs_FastIoMdlReadComplete(
	IN PFILE_OBJECT				FileObject,
	OUT PMDL					MdlChain,
	IN PDEVICE_OBJECT			DeviceObject
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_FastIoMdlReadComplete .\n"));
	CcMdlReadComplete(FileObject, MdlChain);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoMdlReadComplete \n"));
	return TRUE;
}

BOOLEAN 
xixfs_FastIoPrepareMdlWrite(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	NTSTATUS		Status = STATUS_SUCCESS;
	PXIXFS_FCB		pFCB;
	PXIXFS_CCB		pCCB;
	BOOLEAN			IsDataWriten = FALSE;
	BOOLEAN			LockFCB = FALSE;
	BOOLEAN			IsEOF =FALSE;
    
	LARGE_INTEGER 	NewFileSize;
    LARGE_INTEGER 	OldFileSize;
	LARGE_INTEGER	Offset;	
	
	PAGED_CODE();

	
	ASSERT(FileObject);
	xixfs_DecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FastIoPrepareMdlWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));

	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED)){
		return FALSE;
	}

	
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		return FALSE;
	}
	
	if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Has not host lock FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
		IoStatus->Status = STATUS_ABANDONED;
        IoStatus->Information = 0;
		return FALSE;
	}

	if(Length == 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Length is 0 %lx .\n", pFCB));
		IoStatus->Status = STATUS_SUCCESS;
        IoStatus->Information = 0;
		return FALSE;
	}





	
	
	if(!XIXCORE_TEST_FLAGS(FileObject->Flags, FO_WRITE_THROUGH)
		&& CcCanIWrite(FileObject, Length, TRUE, FALSE)
		&& CcCopyWriteWontFlush(FileObject, FileOffset, Length)
		&& XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE))
	{



		IoStatus->Status = STATUS_SUCCESS;	



		FsRtlEnterFileSystem();

	
		if(!ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE))
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Acquire Fail PagingIoResource 0 %lx .\n", pFCB));
			FsRtlExitFileSystem();
			IoStatus->Status = STATUS_ABANDONED;
			IoStatus->Information = 0;
			return FALSE;
		}

		XifsdLockFcb(NULL, pFCB);
		LockFCB = TRUE;

		Offset.QuadPart = FileOffset->QuadPart;
		NewFileSize.QuadPart = FileOffset->QuadPart + Length;
		
		// Changed by ILGU HONG
		//if ((FileOffset->QuadPart < 0) 
		//	|| (FileOffset->QuadPart + Length > (int64)pFCB->ValidDataLength.LowPart)) 
		if ((FileOffset->HighPart < 0) 
			|| (FileOffset->QuadPart + Length > (int64)pFCB->ValidDataLength.QuadPart)) 
		// Changed by ILGU HONG END
		{	
			// Changed by ILGU HONG
			//if(FileOffset->QuadPart <0 ){
			if(FileOffset->HighPart <0 ){
			// Changed by ILGU HONG END
				Offset.QuadPart = pFCB->FileSize.QuadPart;
				NewFileSize.QuadPart = Offset.QuadPart + Length;
			}
			
			IsEOF = (!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO) 
							|| xixfs_FileCheckEofWrite(pFCB, &Offset, Length));
		}


			
		if(IsEOF){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_EOF_IO);
			OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

			if(NewFileSize.QuadPart > pFCB->FileSize.QuadPart){
				pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
				
			}
		}

		if((uint64)(NewFileSize.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize){
			
			uint64 LastOffset = 0;
			uint64 RequestStartOffset = 0;
			uint32 EndLotIndex = 0;
			uint32 CurentEndIndex = 0;
			uint32 RequestStatIndex = 0;
			uint32 LotCount = 0;
			uint32 AllocatedLotCount = 0;



			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
			(" FileSize ceck LotNumber(%I64d) FileSize(%I64d)\n", pFCB->XixcoreFcb.LotNumber, pFCB->FileSize));

			
			RequestStartOffset = Offset.QuadPart;
			LastOffset = NewFileSize.QuadPart;
			

			CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pFCB->PtrVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
			EndLotIndex = xixcore_GetIndexOfLogicalAddress(pFCB->PtrVCB->XixcoreVcb.LotSize, LastOffset);
			
			if(EndLotIndex > CurentEndIndex){
				LotCount = EndLotIndex - CurentEndIndex;
				
				XifsdUnlockFcb(NULL, pFCB);
				LockFCB = FALSE;

			
				Status = xixcore_AddNewLot(
								&(pFCB->PtrVCB->XixcoreVcb), 
								&(pFCB->XixcoreFcb), 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								pFCB->XixcoreFcb.AddrLot, 
								pFCB->XixcoreFcb.AddrLotSize,
								&(pFCB->XixcoreFcb.AddrStartSecIndex)
								);

				XifsdLockFcb(NULL, pFCB);
				LockFCB = TRUE;

				if(!NT_SUCCESS(Status)){

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("FAIL AddNewLot FCB LotNumber(%I64d) .\n", pFCB->XixcoreFcb.LotNumber));


					IoStatus->Status = Status;
					IoStatus->Information = 0;
					IsDataWriten = FALSE;	
					goto error_out;
					
				}
				pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;

				if(LotCount != AllocatedLotCount){
					if(pFCB->XixcoreFcb.RealAllocationSize < RequestStartOffset){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("FAIL AddNewLot FCB LotNumber(%I64d) Request Lot(%ld) Allocated Lot(%ld) .\n", 
							pFCB->XixcoreFcb.LotNumber, LotCount, AllocatedLotCount));
						
						IoStatus->Status = STATUS_UNSUCCESSFUL;
						IoStatus->Information = 0;
						IsDataWriten = FALSE;	
						goto error_out;
					}else if (LastOffset > pFCB->XixcoreFcb.RealAllocationSize){
						 Length = (uint32)(pFCB->XixcoreFcb.RealAllocationSize - RequestStartOffset);
					}else{
						IoStatus->Status = STATUS_UNSUCCESSFUL;
						IoStatus->Information = 0;
						IsDataWriten = FALSE;	
						goto error_out;
					}
				}

				CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
				//DbgPrint(" 3 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
			}
			
	
			
		}

		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;


		if((FileObject->PrivateCacheMap == NULL) ||
			(pFCB->IsFastIoPossible == FastIoIsNotPossible))
		{
			//FsRtlIncrementCcFastReadNotPossible();
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("Fast Io is not possible %lx .\n", pFCB));
			
			IoStatus->Status = STATUS_ABANDONED;
			IoStatus->Information = 0;
			IsDataWriten = FALSE;
			goto error_out; 
		}

		if(pFCB->IsFastIoPossible == FastIoIsQuestionable){
			PDEVICE_OBJECT	pTargetVDO = NULL;
			PFAST_IO_DISPATCH FastIoDispatch = NULL;
			pTargetVDO = IoGetRelatedDeviceObject(FileObject);
			FastIoDispatch = pTargetVDO->DriverObject->FastIoDispatch;

			ASSERT(FastIoDispatch);
			ASSERT(FastIoDispatch->FastIoCheckIfPossible);

			if(!FastIoDispatch->FastIoCheckIfPossible(FileObject,
											&Offset,
											Length,
											TRUE,
											LockKey,
											FALSE, 
											IoStatus,
											pTargetVDO )) 
			{
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("fast io is not possible 0 %lx .\n", pFCB));
				IoStatus->Status = STATUS_ABANDONED;
				IoStatus->Information = 0;
				IsDataWriten = FALSE;	
				goto error_out;
			}

		}
	

		
			
		if(IsEOF) {
			CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			//DbgPrint(" 5 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

		}
			


		IoSetTopLevelIrp( (PIRP) FSRTL_FAST_IO_TOP_LEVEL_IRP );
		
		try{
				// Changed by ILGU HONG
				//if(Offset.LowPart > pFCB->ValidDataLength.LowPart){
				if(Offset.QuadPart > pFCB->ValidDataLength.QuadPart){
				// Changed by ILGU HONG END
					CcZeroData(FileObject,
							&pFCB->ValidDataLength,
							&Offset,
							TRUE 
							);
						
				}

				IoStatus->Status = STATUS_SUCCESS;
				
				CcPrepareMdlWrite( FileObject, &Offset, Length, MdlChain, IoStatus );
				
				IsDataWriten = TRUE;
		}except( FsRtlIsNtstatusExpected(GetExceptionCode())
                                        ? EXCEPTION_EXECUTE_HANDLER
                                        : EXCEPTION_CONTINUE_SEARCH ){
			IsDataWriten = FALSE;


		}		
		IoSetTopLevelIrp(NULL);
	
	}else{

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("FAIL xixfs_FastIoPrepareMdlWrite FCB LotNumber(%I64d) .\n", pFCB->XixcoreFcb.LotNumber));

		return FALSE;
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoPrepareMdlWrite %lx .\n", pFCB));
	
	
	


	if(IsDataWriten){
		FileObject->Flags |= FO_FILE_MODIFIED;
		FileObject->CurrentByteOffset.QuadPart = NewFileSize.QuadPart;
		

		XifsdLockFcb(NULL, pFCB);
		LockFCB = TRUE;


		if(pFCB->XixcoreFcb.WriteStartOffset == -1){
			pFCB->XixcoreFcb.WriteStartOffset = Offset.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->XixcoreFcb.WriteStartOffset > Offset.QuadPart){
			pFCB->XixcoreFcb.WriteStartOffset = Offset.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}
		
		
		

		if(IsEOF) {
			FileObject->Flags |= FO_FILE_SIZE_CHANGED;
			pFCB->ValidDataLength.QuadPart = NewFileSize.QuadPart;
			if(pFCB->FileSize.QuadPart < NewFileSize.QuadPart){
				pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
			}
			CcGetFileSizePointer(FileObject)->QuadPart = pFCB->FileSize.QuadPart;
			xixfs_FileFinishIoEof(pFCB);
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		}

		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;

		
	}else{
		if(IsEOF) {
			XifsdLockFcb(NULL, pFCB);
			LockFCB = TRUE;
			
			pFCB->FileSize.QuadPart = OldFileSize.QuadPart;
			//CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
			//DbgPrint(" 4-4 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

			xixfs_FileFinishIoEof(pFCB);

			XifsdUnlockFcb(NULL, pFCB);
			LockFCB = FALSE;
		}
	}
		
	if(LockFCB) XifsdUnlockFcb(NULL, pFCB);
	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsDataWriten;


error_out:



	if(LockFCB) XifsdUnlockFcb(NULL, pFCB);
	
	if(IsEOF) {
		XifsdLockFcb(NULL, pFCB)
		LockFCB = TRUE;

		pFCB->FileSize.QuadPart = OldFileSize.QuadPart;
		//CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
		//DbgPrint(" 4-3 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

		xixfs_FileFinishIoEof(pFCB);
		DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			(" 6 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;

	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsDataWriten;
}

BOOLEAN 
xixfs_FastIoMdlWriteComplete(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER				FileOffset,
	OUT PMDL						MdlChain,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Enter xixfs_FastIoMdlWriteComplete \n"));
	CcMdlWriteComplete(FileObject, FileOffset, MdlChain);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_FastIoMdlWriteComplete \n"));
	return TRUE;
}


//
//	Cache Mgr call for this routine
//		to avoid deadlock
//
BOOLEAN
xixfs_AcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PXIXFS_FCB pFCB = NULL;
	BOOLEAN		bRet = FALSE;
	PAGED_CODE();

	ASSERT(Context);
	pFCB = (PXIXFS_FCB)Context;
	ASSERT_FCB(pFCB);




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_AcqLazyWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));

 
	ASSERT(IoGetTopLevelIrp() == NULL);
 


	ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);
	ASSERT(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE));

	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return FALSE;
	//}
	// Added End
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("FCB(%x) xixfs_AcqLazyWrite Acq PagingIoResource .\n", pFCB));

	//bRet =  ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, Wait);
	bRet =  ExAcquireResourceSharedLite(pFCB->PagingIoResource, Wait);
	if(bRet){
		   IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

		   pFCB->LazyWriteThread = PsGetCurrentThread();
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("FCB(%x) xixfs_AcqLazyWrite Acq PagingIoResource SUCCESS .\n", pFCB));
	}else{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("FCB(%x) xixfs_AcqLazyWrite Acq PagingIoResource FAIL .\n", pFCB));
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_AcqLazyWrite %s .\n", ((bRet)?"TRUE":"FALSE")));
	return bRet;

}

VOID
xixfs_RelLazyWrite(
	IN PVOID Context
)
{
	PXIXFS_FCB pFCB = NULL;
	PAGED_CODE();

	ASSERT(Context);
	pFCB = (PXIXFS_FCB)Context;
	ASSERT(pFCB);


	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_RelLazyWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));


	ASSERT(IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
	IoSetTopLevelIrp(NULL);
	pFCB->LazyWriteThread = NULL;

	ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);
	ASSERT(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE));

	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return ;
	//}
	// Added End

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_RelLazyWrite %lx .\n", pFCB));
	ExReleaseResourceLite(pFCB->PagingIoResource);

	return;
}


BOOLEAN
xixfs_AcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PXIXFS_FCB pFCB = NULL;
	PAGED_CODE();

	ASSERT(Context);
	pFCB = (PXIXFS_FCB)Context;
	ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_AcqReadAhead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
 
	ASSERT(IoGetTopLevelIrp() == NULL);
    IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
	

	ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("Exit xixfs_AcqReadAhead %lx .\n", pFCB));
		return ExAcquireResourceSharedLite(pFCB->PagingIoResource, Wait);
		//return ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, Wait);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Exit xixfs_AcqReadAhead .\n"));

	return TRUE;
}

VOID
xixfs_RelReadAhead(
	IN PVOID Context
)
{
	PXIXFS_FCB pFCB = NULL;
	PAGED_CODE();


	ASSERT(Context);
	pFCB = (PXIXFS_FCB)Context;
	ASSERT_FCB(pFCB);

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_RelReadAhead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->XixcoreFcb.LotNumber));
	
	ASSERT(IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
	IoSetTopLevelIrp(NULL);

	ASSERT(pFCB->XixcoreFcb.FCBType ==  FCB_TYPE_FILE);
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CACHED_FILE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("Exit xixfs_RelReadAhead %lx .\n", pFCB));
		
		ExReleaseResourceLite(pFCB->PagingIoResource);
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Exit xixfs_RelReadAhead.\n"));

	return;
}

BOOLEAN
xixfs_NopAcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_NopAcqLazyWrite.\n"));
	return TRUE;
}

VOID
xixfs_NopRelLazyWrite(
	IN PVOID Context
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_NopRelLazyWrite.\n"));
	return;
}

BOOLEAN
xixfs_NopAcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_NopAcqReadAhead.\n"));
	return TRUE;
}

VOID
xixfs_NopRelReadAhead(
	IN PVOID Context
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixfs_NopRelReadAhead.\n"));
	return ;
}