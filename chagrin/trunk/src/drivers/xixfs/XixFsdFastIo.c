#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsdInternalApi.h"
#include "XixFsExportApi.h"




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdFinishIoEof)
#pragma alloc_text(PAGE, XixFsdCheckEofWrite)
#pragma alloc_text(PAGE, XixFsdFastIoCheckIfPossible)
#pragma alloc_text(PAGE, XixFsdFastIoRead)
#pragma alloc_text(PAGE, XixFsdFastIoWrite)
#pragma alloc_text(PAGE, XixFsdFastIoQueryBasicInfo)
#pragma alloc_text(PAGE, XixFsdFastIoQueryStdInfo)
#pragma alloc_text(PAGE, XixFsdFastIoQueryNetInfo)
#pragma alloc_text(PAGE, XixFsdFastIoLock)
#pragma alloc_text(PAGE, XixFsdFastIoUnlockSingle)
#pragma alloc_text(PAGE, XixFsdFastIoUnlockAll)
#pragma alloc_text(PAGE, XixFsdFastIoUnlockAllByKey)
#pragma alloc_text(PAGE, XixFsdFastIoAcqCreateSec)
#pragma alloc_text(PAGE, XixFsdFastIoRelCreateSec)
#pragma alloc_text(PAGE, XixFsdFastIoAcqModWrite)
#pragma alloc_text(PAGE, XixFsdFastIoRelModWrite)
#pragma alloc_text(PAGE, XixFsdFastIoAcqCcFlush)
#pragma alloc_text(PAGE, XixFsdFastIoRelCcFlush)
#pragma alloc_text(PAGE, XixFsdFastIoMdlRead)
#pragma alloc_text(PAGE, XixFsdFastIoPrepareMdlWrite)
#pragma alloc_text(PAGE, XixFsdAcqLazyWrite)
#pragma alloc_text(PAGE, XixFsdRelLazyWrite)
#pragma alloc_text(PAGE, XixFsdAcqReadAhead)
#pragma alloc_text(PAGE, XixFsdRelReadAhead)
#pragma alloc_text(PAGE, XixFsdNopAcqLazyWrite)
#pragma alloc_text(PAGE, XixFsdNopRelLazyWrite)
#pragma alloc_text(PAGE, XixFsdNopAcqReadAhead)
#pragma alloc_text(PAGE, XixFsdNopRelReadAhead)
#endif


typedef struct _EOF_WAIT_CTX{
	LIST_ENTRY	EofWaitLink;
	KEVENT		EofEvent;
}EOF_WAIT_CTX, *PEOF_WAIT_CTX;


VOID
XixFsdFinishIoEof(
	IN PXIFS_FCB pFCB
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
		XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
	}

}



BOOLEAN
XixFsdCheckEofWrite(
	IN PXIFS_FCB		pFCB,
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
		XixFsdFinishIoEof(pFCB);

		return FALSE;
	}

	return TRUE;
}





BOOLEAN 
XixFsdFastIoCheckIfPossible(
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
	PXIFS_FCB 		pFCB = NULL;
	PXIFS_CCB		pCCB = NULL;
	LARGE_INTEGER	LargeLength;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoCheckIfPossible\n"));

	ASSERT(FileObject);
	
	pCCB = FileObject->FsContext2;
	pFCB = FileObject->FsContext;
	ASSERT_FCB(pFCB);
		
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));	
	

	if(pFCB->FCBType != FCB_TYPE_FILE) 
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
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Past Io Allowed %lx .\n", pFCB));

				try_return(RC = TRUE);
			}
			
		}else{
			
			ULONG	Reason = 0;

			if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
				try_return (RC = FALSE);
			}

			// Added by ILGU HONG for readonly 09082006
			if((pFCB->FCBFileLock != NULL) && (pFCB->PtrVCB->IsVolumeWriteProctected)){
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
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Past Io Allowed %lx .\n", pFCB));
					
					try_return (RC = TRUE);
				}
			
		}
		

	}finally{
		;
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoCheckIfPossible %lx .\n", pFCB));	
	
	return RC;
	
}

BOOLEAN 
XixFsdFastIoRead(
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
	PXIFS_FCB		pFCB = NULL;
	PXIFS_CCB		pCCB = NULL;
	PDEVICE_OBJECT 	pTargetVDO = NULL;
	LARGE_INTEGER	LastBytes;
	BOOLEAN			IsOpDone = FALSE;
	uint32			LocalLength = 0;
	uint32			PrePocessData = 0;
	BOOLEAN			LockFCB = FALSE;
	BOOLEAN			IsEOF = FALSE;
	PAGED_CODE();


	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoRead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("[Param]FileOffset(%I64d):Length(%Id) [Fcb]FileSize(%I64d):ValidLength(%I64d):AllocSize(%I64d)[fileObj]COff(%I64d).\n", 
		FileOffset->QuadPart, Length, 
		pFCB->FileSize.QuadPart, pFCB->ValidDataLength.QuadPart , pFCB->AllocationSize.QuadPart,
		FileObject->CurrentByteOffset.QuadPart));



	// Check if is it possible cached operation
	if(pFCB->FCBType != FCB_TYPE_FILE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("XifsdFastIoRead  Invalid Type%lx .\n", pFCB));
		return FALSE;
	}

	if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
		return FALSE;
	}



	if(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE))
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
		IsEOF = (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
						|| XixFsdCheckEofWrite(pFCB, FileOffset, Length));
	}

	XifsdUnlockFcb(NULL, pFCB);
	LockFCB = FALSE;


	if(IsEOF){
        XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
		
		if(FileOffset->QuadPart >= pFCB->ValidDataLength.QuadPart){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
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



	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoRead (%I64d) IsOpDone(%s) .\n", pFCB->LotNumber, ((IsOpDone)?"TRUE":"FALSE")));
	
	if(LockFCB)
		XifsdUnlockFcb(NULL, pFCB);
	
	if(IsEOF){
		XifsdLockFcb(NULL, pFCB);
		XixFsdFinishIoEof(pFCB);
		XifsdUnlockFcb(NULL, pFCB);
	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsOpDone;	
}



BOOLEAN 
XixFsdFastIoWrite(
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
	PXIFS_FCB		pFCB;
	PXIFS_CCB		pCCB;
	BOOLEAN			IsOpDone = FALSE;
	BOOLEAN			IsEOF =FALSE;
	BOOLEAN			LockFCB = FALSE;
    LARGE_INTEGER 	NewFileSize;
    LARGE_INTEGER 	OldFileSize;
	LARGE_INTEGER	Offset;	

	uint32			OldLevel = 0;
	uint32			OldTarget = 0;


	PAGED_CODE();
	
	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));


	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_ALL),
		("[Param]FileOffset(%I64d):Length(%Id) [Fcb]FileSize(%I64d):ValidLength(%I64d):AllocSize(%I64d)[fileObj]COff(%I64d).\n", 
		FileOffset->QuadPart, Length, 
		pFCB->FileSize.QuadPart, pFCB->ValidDataLength.QuadPart , pFCB->AllocationSize.QuadPart,
		FileObject->CurrentByteOffset.QuadPart));




	// Check if is it possible cached operation
	if(pFCB->FCBType != FCB_TYPE_FILE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("XixFsdFastIoWrite Invalid Type%lx .\n", pFCB));
		return FALSE;
	}

	if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
		return FALSE;
	}

	if(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("XixFsdFastIoWrite  Invalid State%lx .\n", pFCB));
		return FALSE;
	}



	if (IoGetTopLevelIrp() != NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Is Not Top Level Irp %lx .\n", pFCB));
        return FALSE;
    }


	
	if(pFCB->HasLock != FCB_FILE_LOCK_HAS)
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


	OldLevel = XifsdDebugLevel;
	OldTarget = XifsdDebugTarget;


	//XifsdDebugLevel = DEBUG_LEVEL_ALL;
	//XifsdDebugTarget = DEBUG_TARGET_ALL;

	
	if(!XifsdCheckFlagBoolean(FileObject->Flags, FO_WRITE_THROUGH)
		&& CcCanIWrite(FileObject, Length, Wait, FALSE)
		&& CcCopyWriteWontFlush(FileObject, FileOffset, Length)
		&& XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){

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

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Do XixFsdFastIoWrite %lx .\n", pFCB));

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
					
				IsEOF = (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
						|| XixFsdCheckEofWrite(pFCB, &Offset, Length));
					
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				("Change Info NewFileSize(%I64d) NewOffSet(%ld) Length(%ld).\n", 
					NewFileSize.QuadPart, Offset.LowPart, Length));
				
			
				if(IsEOF){
					XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
					OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

					if(NewFileSize.QuadPart > pFCB->FileSize.QuadPart){
						pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
						
					}
				}

				if((uint64)(NewFileSize.QuadPart) > pFCB->RealAllocationSize){
					
					uint64 LastOffset = 0;
					uint64 RequestStartOffset = 0;
					uint32 EndLotIndex = 0;
					uint32 CurentEndIndex = 0;
					uint32 RequestStatIndex = 0;
					uint32 LotCount = 0;
					uint32 AllocatedLotCount = 0;

					

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" FileSize ceck LotNumber(%I64d) FileSize(%I64d)\n", pFCB->LotNumber, pFCB->FileSize));

					
					RequestStartOffset = Offset.QuadPart;
					LastOffset = NewFileSize.QuadPart;
					

					CurentEndIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, (pFCB->RealAllocationSize-1));
					EndLotIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, LastOffset);
					

					// Allocate New Lot
					if(EndLotIndex > CurentEndIndex){
						LotCount = EndLotIndex - CurentEndIndex;
						

						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
								pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));
						
						XifsdUnlockFcb(NULL, pFCB);
						LockFCB = FALSE;

						
						Status = XixFsAddNewLot(
										pFCB->PtrVCB, 
										pFCB, 
										CurentEndIndex, 
										LotCount, 
										&AllocatedLotCount,
										(uint8 *)pFCB->AddrLot, 
										pFCB->AddrLotSize, 
										&pFCB->AddrStartSecIndex
										);
						

						XifsdLockFcb(NULL, pFCB);
						LockFCB = TRUE;
						if(!NT_SUCCESS(Status)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
									pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));


							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("FAIL AddNewLot FCB LotNumber(%I64d) .\n", pFCB->LotNumber));

						
							IoStatus->Status = Status;
							IoStatus->Information = 0;
							IsOpDone = FALSE;	
							goto error_done; 
						}

						if(LotCount != AllocatedLotCount){
							if(pFCB->RealAllocationSize < RequestStartOffset){
								DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("FAIL AddNewLot FCB LotNumber(%I64d) Request Lot(%ld) Allocated Lot(%ld) .\n", 
									pFCB->LotNumber, LotCount, AllocatedLotCount));
								

								IoStatus->Status = STATUS_UNSUCCESSFUL;
								IoStatus->Information = 0;
								IsOpDone = FALSE;	
								goto error_done; 
							}else if(LastOffset > pFCB->RealAllocationSize){
								 Length = (uint32)(pFCB->RealAllocationSize - RequestStartOffset);
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
					

					



					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
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
			
				IsEOF = (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
						|| XixFsdCheckEofWrite(pFCB, &Offset, Length));
				
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				("Change Info NewFileSize(%I64d) NewOffSet(%I64d) Length(%ld).\n", 
					NewFileSize.QuadPart, Offset.QuadPart, Length));
			
			
				if(IsEOF){
					XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
					OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

					if(NewFileSize.QuadPart > pFCB->FileSize.QuadPart){
						pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
						
					}
				}



				if((uint64)(NewFileSize.QuadPart) > pFCB->RealAllocationSize){
					
					uint64 LastOffset = 0;
					uint64 RequestStartOffset = 0;
					uint32 EndLotIndex = 0;
					uint32 CurentEndIndex = 0;
					uint32 RequestStatIndex = 0;
					uint32 LotCount = 0;
					uint32 AllocatedLotCount = 0;

					

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
					(" FileSize ceck LotNumber(%I64d) FileSize(%I64d)\n", pFCB->LotNumber, pFCB->FileSize));

					
					RequestStartOffset = Offset.QuadPart;
					LastOffset = NewFileSize.QuadPart;
					

					CurentEndIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, (pFCB->RealAllocationSize-1));
					EndLotIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, LastOffset);
					

					// Allocate New Lot
					if(EndLotIndex > CurentEndIndex){
						LotCount = EndLotIndex - CurentEndIndex;
						

						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
								("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
								pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));
						
						XifsdUnlockFcb(NULL, pFCB);
						LockFCB = FALSE;
						
						
						Status = XixFsAddNewLot(
										pFCB->PtrVCB, 
										pFCB, 
										CurentEndIndex, 
										LotCount, 
										&AllocatedLotCount,
										(uint8 *)pFCB->AddrLot, 
										pFCB->AddrLotSize,
										&pFCB->AddrStartSecIndex
										);
						

						XifsdLockFcb(NULL, pFCB);
						LockFCB = TRUE;

						if(!NT_SUCCESS(Status)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("AddNewLot FCB LotNumber(%I64d) CurrentEndIndex(%ld), Request Lot(%ld) AllocatLength(%I64d) .\n",
									pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));


							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("FAIL AddNewLot FCB LotNumber(%I64d) .\n", pFCB->LotNumber));

						
							IoStatus->Status = Status;
							IoStatus->Information = 0;
							IsOpDone = FALSE;	
							goto error_done; 
						}

						if(LotCount != AllocatedLotCount){
							if(pFCB->RealAllocationSize < RequestStartOffset){
								DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
									("FAIL AddNewLot FCB LotNumber(%I64d) Request Lot(%ld) Allocated Lot(%ld) .\n", 
									pFCB->LotNumber, LotCount, AllocatedLotCount));
								

								IoStatus->Status = STATUS_UNSUCCESSFUL;
								IoStatus->Information = 0;
								IsOpDone = FALSE;	
								goto error_done; 
							}else if (LastOffset >pFCB->RealAllocationSize) {
								 Length = (uint32)(pFCB->RealAllocationSize -RequestStartOffset );
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
					
					
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
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
		("FAIL XixFsdFastIoWrite FCB LotNumber(%I64d) .\n", pFCB->LotNumber));

		return FALSE;
	}
	
	if(IsOpDone ){
		FileObject->Flags |= FO_FILE_MODIFIED;
		FileObject->CurrentByteOffset.QuadPart = NewFileSize.QuadPart;
		

		XifsdLockFcb(NULL, pFCB);
		LockFCB = TRUE;


		if(pFCB->WriteStartOffset == -1){
			pFCB->WriteStartOffset = Offset.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->WriteStartOffset > Offset.QuadPart){
			pFCB->WriteStartOffset = Offset.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
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
			
			XixFsdFinishIoEof(pFCB);
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
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
			
			XixFsdFinishIoEof(pFCB);
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

			XifsdUnlockFcb(NULL, pFCB);
			LockFCB = FALSE;
		}
	}

	//XifsdDebugLevel = OldLevel;
	//XifsdDebugTarget = OldTarget;

			
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoWrite %lx .\n", pFCB));
	if(LockFCB)
		XifsdUnlockFcb(NULL, pFCB);
	
				
	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsOpDone;	
	

error_done:


	//XifsdDebugLevel = OldLevel;
	//XifsdDebugTarget = OldTarget;

	if(LockFCB)
		XifsdUnlockFcb(NULL, pFCB);

	if(IsEOF) {
		XifsdLockFcb(NULL, pFCB)
		LockFCB = TRUE;
		
		pFCB->FileSize.QuadPart = OldFileSize.QuadPart;
		//CcGetFileSizePointer(FileObject)->QuadPart = OldFileSize.QuadPart;
		//CcSetFileSizes(FileObject,(PCC_FILE_SIZES) &pFCB->AllocationSize);
		//DbgPrint(" 2-2 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);

		XixFsdFinishIoEof(pFCB);
		DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			(" 3 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;

	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsOpDone;	
}

BOOLEAN 
XixFsdFastIoQueryBasicInfo(
	IN PFILE_OBJECT					FileObject,
	IN BOOLEAN						Wait,
	OUT PFILE_BASIC_INFORMATION		Buffer,
	OUT PIO_STATUS_BLOCK 			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;
	BOOLEAN		IOResult = FALSE;
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		Length = sizeof(FILE_BASIC_INFORMATION);
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
		
	ASSERT(FileObject);
	TypeOfOpen = XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoQueryBasicInfo FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	
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
		RC = XixFsdQueryBasicInfo( pFCB, 
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
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoQueryBasicInfo %lx .\n", pFCB));

	return IOResult;	
}

BOOLEAN 
XixFsdFastIoQueryStdInfo(
	IN PFILE_OBJECT						FileObject,
	IN BOOLEAN							Wait,
	OUT PFILE_STANDARD_INFORMATION 	Buffer,
	OUT PIO_STATUS_BLOCK 				IoStatus,
	IN PDEVICE_OBJECT					DeviceObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;
	BOOLEAN		IOResult = FALSE;
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		Length = sizeof(FILE_STANDARD_INFORMATION);
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();

	ASSERT(FileObject);
	TypeOfOpen = XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdFastIoQueryStdInfo FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	
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

		RC = XixFsdQueryStandardInfo( pFCB, 
							pCCB, 
							Buffer, 
							&Length 
							);
		
		IoStatus->Status = RC;
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, ("Fail XixFsdFastIoQueryStdInfo %lx .\n", pFCB));
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
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Exit XixFsdFastIoQueryStdInfo %lx .\n", pFCB));
	return IOResult;	
}


BOOLEAN 
XixFsdFastIoQueryNetInfo(
	IN PFILE_OBJECT						FileObject,
	IN BOOLEAN							Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION 	Buffer,
	OUT PIO_STATUS_BLOCK 				IoStatus,
	IN PDEVICE_OBJECT					DeviceObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;
	BOOLEAN		IOResult = FALSE;
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		Length = sizeof(FILE_NETWORK_OPEN_INFORMATION);
	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
	ASSERT(FileObject);
	TypeOfOpen = XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdFastIoQueryNetInfo FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));

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
		
		RC = XixFsdQueryNetworkInfo( pFCB, 
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
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoQueryNetInfo %lx .\n", pFCB));
	return IOResult;	
}

BOOLEAN 
XixFsdFastIoLock(
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
	PXIFS_FCB 		pFCB = NULL;
	PXIFS_CCB 		pCCB = NULL;
	

	PAGED_CODE();
	
	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoLock FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));

	if(pFCB->FCBType != FCB_TYPE_FILE) 
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
				try_return(RC = FALSE);
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
				pFCB->IsFastIoPossible =XixFsdCheckFastIoPossible(pFCB);
				XifsdUnlockFcb(TRUE,pFCB);
			}
		}


		


	}finally{
		DebugUnwind(XifsdFastIoLock);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit XixFsdFastIoLock %lx .\n", pFCB));
	}
	
	return RC;
	
}


BOOLEAN 
XixFsdFastIoUnlockSingle(
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
	PXIFS_FCB pFCB = NULL;
	PXIFS_CCB pCCB = NULL;
		
	PAGED_CODE();

	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoUnlockSingle FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	if(pFCB->FCBType != FCB_TYPE_FILE) 
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
				try_return(RC = FALSE);
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
			pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible(pFCB);
			XifsdUnlockFcb(TRUE,pFCB);
		}
		


	}finally{
		DebugUnwind(XifsdFastIoUnlockSingle);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit XixFsdFastIoUnlockSingle %lx .\n", pFCB));
	}
	
	return RC;
}

BOOLEAN 
XixFsdFastIoUnlockAll(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS						ProcessId,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	PXIFS_FCB pFCB = NULL;
	PXIFS_CCB pCCB = NULL;

	PAGED_CODE();
	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoUnlockAll FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	if(pFCB->FCBType != FCB_TYPE_FILE) 
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
				try_return(RC = FALSE);
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
			pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible(pFCB);
			XifsdUnlockFcb(TRUE,pFCB);
		}
		


	}finally{
		DebugUnwind(XifsdFastIoUnlockAll);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit XixFsdFastIoUnlockAll %lx .\n", pFCB));
	}
	
	return RC;
}

BOOLEAN 
XixFsdFastIoUnlockAllByKey(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS					ProcessId,
	ULONG						Key,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	BOOLEAN			RC = FALSE;
	PXIFS_FCB pFCB = NULL;
	PXIFS_CCB pCCB = NULL;

	PAGED_CODE();
	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoUnlockAllByKey FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	if(pFCB->FCBType != FCB_TYPE_FILE) 
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
				try_return(RC = FALSE);
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
			pFCB->IsFastIoPossible = XixFsdCheckFastIoPossible(pFCB);
			XifsdUnlockFcb(TRUE,pFCB);
		}
		


	}finally{
		DebugUnwind(XifsdFastIoUnlockAllByKey);
		ExReleaseResourceLite(pFCB->Resource);
		FsRtlExitFileSystem();
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Exit XixFsdFastIoUnlockAllByKey %lx .\n", pFCB));
	}
	
	return RC;
}

void 
XixFsdFastIoAcqCreateSec(
	IN PFILE_OBJECT			FileObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;

	PAGED_CODE();

	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoAcqCreateSec FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));

	
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, TRUE);
	}
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoAcqCreateSec %lx .\n", pFCB));
}


void 
XixFsdFastIoRelCreateSec(
	IN PFILE_OBJECT			FileObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;
	
	PAGED_CODE();
	
	ASSERT(FileObject);
	pCCB = FileObject->FsContext2;
	pFCB = FileObject->FsContext;
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoRelCreateSec FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		ExReleaseResourceLite(pFCB->PagingIoResource);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoRelCreateSec %lx .\n", pFCB));
}


NTSTATUS 
XixFsdFastIoAcqModWrite(
	IN PFILE_OBJECT						FileObject,
	IN PLARGE_INTEGER					EndingOffset,
	OUT PERESOURCE						*ResourceToRelease,
	IN PDEVICE_OBJECT					DeviceObject
)
{
	BOOLEAN		AcquireFile = FALSE;
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;

	PAGED_CODE();

	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);


	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return STATUS_UNSUCCESSFUL;
	//}
	// Added End

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoAcqModWrite .\n"));

	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("AcqModWrite Use PagingIoResource FCB(%x) FCBName(%wZ)\n", &pFCB, pFCB->FCBName));
		*ResourceToRelease = pFCB->PagingIoResource;
	}else {
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("AcqModWrite Use Resource FCB(%x) FCBName(%wZ)\n", &pFCB, pFCB->FCBName));

		*ResourceToRelease = pFCB->Resource;
	}

	//AcquireFile = ExAcquireResourceExclusiveLite(*ResourceToRelease , FALSE);
	AcquireFile = ExAcquireResourceSharedLite(*ResourceToRelease , FALSE);
	if(AcquireFile){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("XixFsdFastIoAcqModWrite SUCCESS \n"));
	}else{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("XixFsdFastIoAcqModWrite Fail \n"));
		*ResourceToRelease = NULL;
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoAcqModWrite Status(%s)\n", ((AcquireFile)?"SUCCESS":"FAIL")));
	
	return ((AcquireFile)?STATUS_SUCCESS:STATUS_CANT_WAIT);
}


NTSTATUS 
XixFsdFastIoRelModWrite(
	IN PFILE_OBJECT					FileObject,
	IN PERESOURCE					ResourceToRelease,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;

	PAGED_CODE();


	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return STATUS_UNSUCCESSFUL;
	//}
	// Added End

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoRelModWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	if(ResourceToRelease != NULL)
		ExReleaseResourceLite(ResourceToRelease);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoRelModWrite %lx .\n", pFCB));

	return TRUE;
}


NTSTATUS 
XixFsdFastIoAcqCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;

	PAGED_CODE();


	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdFastIoAcqCcFlush FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		ExAcquireResourceSharedLite(pFCB->PagingIoResource, TRUE);
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoAcqCcFlush %lx .\n", pFCB));
	return STATUS_SUCCESS;
}

NTSTATUS 
XixFsdFastIoRelCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;
	
	PAGED_CODE();

	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoRelCcFlush FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		ExReleaseResourceLite(pFCB->PagingIoResource);
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoRelCcFlush %lx .\n", pFCB));
	return STATUS_SUCCESS;
}



BOOLEAN 
XixFsdFastIoMdlRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
)
{
	PXIFS_FCB		pFCB = NULL;
	PXIFS_CCB		pCCB = NULL;
	PDEVICE_OBJECT 	pTargetVDO = NULL;
	LARGE_INTEGER	LastBytes;
	BOOLEAN			IsOpDone = FALSE;
	BOOLEAN			LockFCB = FALSE;
	BOOLEAN			IsEOF = FALSE;
	PAGED_CODE();

	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoMdlRead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));

	if(pFCB->FCBType != FCB_TYPE_FILE){
		return FALSE;
	}

	if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
		return FALSE;
	}

	if(Length == 0)
	{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("File Size is 0 FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
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
		IsEOF = (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
						|| XixFsdCheckEofWrite(pFCB, FileOffset, Length));
	}

	XifsdUnlockFcb(NULL, pFCB);
	LockFCB = FALSE;


	if(IsEOF){
		
		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
		
		if(FileOffset->QuadPart >= pFCB->ValidDataLength.QuadPart){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
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
		XixFsdFinishIoEof(pFCB);
		XifsdUnlockFcb(NULL, pFCB);
	}

	if(IsOpDone){
		FileObject->CurrentByteOffset.QuadPart = LastBytes.QuadPart;
	}


	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoMdlRead %lx .\n", pFCB));
	return IsOpDone;

error_out:
	

	
	if(LockFCB) XifsdUnlockFcb(NULL, pFCB);

	if(IsEOF){
		XifsdLockFcb(NULL,pFCB);
		XixFsdFinishIoEof(pFCB);
		XifsdUnlockFcb(NULL, pFCB);
	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XifsdFastIoMdlRead %lx .\n", pFCB));
	return IsOpDone;
}


BOOLEAN 
XixFsdFastIoMdlReadComplete(
	IN PFILE_OBJECT				FileObject,
	OUT PMDL					MdlChain,
	IN PDEVICE_OBJECT			DeviceObject
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdFastIoMdlReadComplete .\n"));
	CcMdlReadComplete(FileObject, MdlChain);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoMdlReadComplete \n"));
	return TRUE;
}

BOOLEAN 
XixFsdFastIoPrepareMdlWrite(
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
	PXIFS_FCB		pFCB;
	PXIFS_CCB		pCCB;
	BOOLEAN			IsDataWriten = FALSE;
	BOOLEAN			LockFCB = FALSE;
	BOOLEAN			IsEOF =FALSE;
    
	LARGE_INTEGER 	NewFileSize;
    LARGE_INTEGER 	OldFileSize;
	LARGE_INTEGER	Offset;	
	
	PAGED_CODE();

	
	ASSERT(FileObject);
	XixFsdDecodeFileObject( FileObject, &pFCB, &pCCB);
	ASSERT_FCB(pFCB);




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFastIoPrepareMdlWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));

	if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
		return FALSE;
	}

	
	if(pFCB->FCBType != FCB_TYPE_FILE){
		return FALSE;
	}
	
	if(pFCB->HasLock != FCB_FILE_LOCK_HAS)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Has not host lock FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
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





	
	
	if(!XifsdCheckFlagBoolean(FileObject->Flags, FO_WRITE_THROUGH)
		&& CcCanIWrite(FileObject, Length, TRUE, FALSE)
		&& CcCopyWriteWontFlush(FileObject, FileOffset, Length)
		&& XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE))
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
			
			IsEOF = (!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
							|| XixFsdCheckEofWrite(pFCB, &Offset, Length));
		}


			
		if(IsEOF){
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_EOF_IO);
			OldFileSize.QuadPart = pFCB->FileSize.QuadPart;

			if(NewFileSize.QuadPart > pFCB->FileSize.QuadPart){
				pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
				
			}
		}

		if((uint64)(NewFileSize.QuadPart) > pFCB->RealAllocationSize){
			
			uint64 LastOffset = 0;
			uint64 RequestStartOffset = 0;
			uint32 EndLotIndex = 0;
			uint32 CurentEndIndex = 0;
			uint32 RequestStatIndex = 0;
			uint32 LotCount = 0;
			uint32 AllocatedLotCount = 0;



			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
			(" FileSize ceck LotNumber(%I64d) FileSize(%I64d)\n", pFCB->LotNumber, pFCB->FileSize));

			
			RequestStartOffset = Offset.QuadPart;
			LastOffset = NewFileSize.QuadPart;
			

			CurentEndIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, (pFCB->RealAllocationSize-1));
			EndLotIndex = GetIndexOfLogicalAddress(pFCB->PtrVCB->LotSize, LastOffset);
			
			if(EndLotIndex > CurentEndIndex){
				LotCount = EndLotIndex - CurentEndIndex;
				
				XifsdUnlockFcb(NULL, pFCB);
				LockFCB = FALSE;

			
				Status = XixFsAddNewLot(
								pFCB->PtrVCB, 
								pFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)pFCB->AddrLot, 
								pFCB->AddrLotSize,
								&pFCB->AddrStartSecIndex
								);

				XifsdLockFcb(NULL, pFCB);
				LockFCB = TRUE;

				if(!NT_SUCCESS(Status)){

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("FAIL AddNewLot FCB LotNumber(%I64d) .\n", pFCB->LotNumber));


					IoStatus->Status = Status;
					IoStatus->Information = 0;
					IsDataWriten = FALSE;	
					goto error_out;
					
				}

				if(LotCount != AllocatedLotCount){
					if(pFCB->RealAllocationSize < RequestStartOffset){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("FAIL AddNewLot FCB LotNumber(%I64d) Request Lot(%ld) Allocated Lot(%ld) .\n", 
							pFCB->LotNumber, LotCount, AllocatedLotCount));
						
						IoStatus->Status = STATUS_UNSUCCESSFUL;
						IoStatus->Information = 0;
						IsDataWriten = FALSE;	
						goto error_out;
					}else if (LastOffset > pFCB->RealAllocationSize){
						 Length = (uint32)(pFCB->RealAllocationSize - RequestStartOffset);
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
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
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
		("FAIL XixFsdFastIoPrepareMdlWrite FCB LotNumber(%I64d) .\n", pFCB->LotNumber));

		return FALSE;
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoPrepareMdlWrite %lx .\n", pFCB));
	
	
	


	if(IsDataWriten){
		FileObject->Flags |= FO_FILE_MODIFIED;
		FileObject->CurrentByteOffset.QuadPart = NewFileSize.QuadPart;
		

		XifsdLockFcb(NULL, pFCB);
		LockFCB = TRUE;


		if(pFCB->WriteStartOffset == -1){
			pFCB->WriteStartOffset = Offset.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->WriteStartOffset > Offset.QuadPart){
			pFCB->WriteStartOffset = Offset.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
		}
		
		
		

		if(IsEOF) {
			FileObject->Flags |= FO_FILE_SIZE_CHANGED;
			pFCB->ValidDataLength.QuadPart = NewFileSize.QuadPart;
			if(pFCB->FileSize.QuadPart < NewFileSize.QuadPart){
				pFCB->FileSize.QuadPart = NewFileSize.QuadPart;
			}
			CcGetFileSizePointer(FileObject)->QuadPart = pFCB->FileSize.QuadPart;
			XixFsdFinishIoEof(pFCB);
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
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

			XixFsdFinishIoEof(pFCB);

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

		XixFsdFinishIoEof(pFCB);
		DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			(" 6 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		XifsdUnlockFcb(NULL, pFCB);
		LockFCB = FALSE;

	}

	ExReleaseResourceLite(pFCB->PagingIoResource);
	FsRtlExitFileSystem();
	return IsDataWriten;
}

BOOLEAN 
XixFsdFastIoMdlWriteComplete(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER				FileOffset,
	OUT PMDL						MdlChain,
	IN PDEVICE_OBJECT				DeviceObject
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("Enter XixFsdFastIoMdlWriteComplete \n"));
	CcMdlWriteComplete(FileObject, FileOffset, MdlChain);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFastIoMdlWriteComplete \n"));
	return TRUE;
}


//
//	Cache Mgr call for this routine
//		to avoid deadlock
//
BOOLEAN
XixFsdAcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PXIFS_FCB pFCB = NULL;
	BOOLEAN		bRet = FALSE;
	PAGED_CODE();

	ASSERT(Context);
	pFCB = (PXIFS_FCB)Context;
	ASSERT_FCB(pFCB);




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdAcqLazyWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));

 
	ASSERT(IoGetTopLevelIrp() == NULL);
 


	ASSERT(pFCB->FCBType == FCB_TYPE_FILE);
	ASSERT(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE));

	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return FALSE;
	//}
	// Added End
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("FCB(%x) XixFsdAcqLazyWrite Acq PagingIoResource Name(%wZ) .\n", pFCB, &pFCB->FCBName));
	//bRet =  ExAcquireResourceExclusiveLite(pFCB->PagingIoResource, Wait);
	bRet =  ExAcquireResourceSharedLite(pFCB->PagingIoResource, Wait);
	if(bRet){
		   IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

		   pFCB->LazyWriteThread = PsGetCurrentThread();
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("FCB(%x) XixFsdAcqLazyWrite Acq PagingIoResource SUCCESS Name(%wZ) .\n", pFCB, &pFCB->FCBName));
	}else{
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("FCB(%x) XixFsdAcqLazyWrite Acq PagingIoResource FAIL Name(%wZ) .\n", pFCB, &pFCB->FCBName));
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdAcqLazyWrite %s .\n", ((bRet)?"TRUE":"FALSE")));
	return bRet;

}

VOID
XixFsdRelLazyWrite(
	IN PVOID Context
)
{
	PXIFS_FCB pFCB = NULL;
	PAGED_CODE();

	ASSERT(Context);
	pFCB = (PXIFS_FCB)Context;
	ASSERT(pFCB);


	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdRelLazyWrite FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));


	ASSERT(IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
	IoSetTopLevelIrp(NULL);
	pFCB->LazyWriteThread = NULL;

	ASSERT(pFCB->FCBType == FCB_TYPE_FILE);
	ASSERT(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE));

	// Added by 04112006
	//if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
	//	return ;
	//}
	// Added End

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdRelLazyWrite %lx .\n", pFCB));
	ExReleaseResourceLite(pFCB->PagingIoResource);

	return;
}


BOOLEAN
XixFsdAcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PXIFS_FCB pFCB = NULL;
	PAGED_CODE();

	ASSERT(Context);
	pFCB = (PXIFS_FCB)Context;
	ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdAcqReadAhead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
 

	ASSERT(IoGetTopLevelIrp() == NULL);
    IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
	

	ASSERT(pFCB->FCBType == FCB_TYPE_FILE);
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("Exit XixFsdAcqReadAhead %lx .\n", pFCB));
		return ExAcquireResourceSharedLite(pFCB->PagingIoResource, Wait);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Exit XixFsdAcqReadAhead .\n"));

	return TRUE;
}

VOID
XixFsdRelReadAhead(
	IN PVOID Context
)
{
	PXIFS_FCB pFCB = NULL;
	PAGED_CODE();


	ASSERT(Context);
	pFCB = (PXIFS_FCB)Context;
	ASSERT_FCB(pFCB);

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdRelReadAhead FCB(%x) LotNumber(%I64d) .\n", pFCB, pFCB->LotNumber));
	
	ASSERT(IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
	IoSetTopLevelIrp(NULL);

	ASSERT(pFCB->FCBType ==  FCB_TYPE_FILE);
	if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CACHED_FILE)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("Exit XixFsdRelReadAhead %lx .\n", pFCB));
		
		ExReleaseResourceLite(pFCB->PagingIoResource);
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Exit XixFsdRelReadAhead.\n"));

	return;
}

BOOLEAN
XixFsdNopAcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdNopAcqLazyWrite.\n"));
	return TRUE;
}

VOID
XixFsdNopRelLazyWrite(
	IN PVOID Context
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdNopRelLazyWrite.\n"));
	return;
}

BOOLEAN
XixFsdNopAcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdNopAcqReadAhead.\n"));
	return TRUE;
}

VOID
XixFsdNopRelReadAhead(
	IN PVOID Context
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter XixFsdNopRelReadAhead.\n"));
	return ;
}