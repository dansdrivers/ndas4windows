#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsdInternalApi.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdCommonRead)
#endif

#define	XIFSD_REQ_NOT_VIA_CACHE_MGR(ptr)	(!MmIsRecursiveIoFault() && ((ptr)->ImageSectionObject != NULL))


NTSTATUS
XixFsdCommonRead(
	IN PXIFS_IRPCONTEXT PtrIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIO_STACK_LOCATION		PtrIoStackLocation = NULL;
	PIRP					PtrIrp = NULL;

	LARGE_INTEGER			ByteOffset;
	uint32					ReadLength = 0;
	uint32					NumberBytesRead = 0;
	PCHAR					buffer = NULL;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;

	
	PFILE_OBJECT			PtrFileObject = NULL;
	PXIFS_FCB				PtrFCB = NULL;
	PXIFS_CCB				PtrCCB = NULL;
	PXIFS_VCB				PtrVCB = NULL;
	
	PERESOURCE				PtrResourceAcquired = NULL;
	void					*PtrSystemBuffer = NULL;


	BOOLEAN					CanWait = FALSE;
	BOOLEAN					PagingIo = FALSE;
	BOOLEAN					NonBufferedIo = FALSE;
	BOOLEAN					SynchronousIo = FALSE;
	XIFS_IO_CONTEXT			LocalIoContext;
	BOOLEAN					DelayedOP = FALSE;
	BOOLEAN					IsEOFProcessing = FALSE;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB), 
		("Enter XixFsdCommonRead\n"));

	ASSERT_IRPCONTEXT(PtrIrpContext);
	PtrIrp = PtrIrpContext->Irp;
	ASSERT(PtrIrp);
	PtrIoStackLocation = IoGetCurrentIrpStackLocation(PtrIrp);
	ASSERT(PtrIoStackLocation);



	if (PtrIoStackLocation->MinorFunction & IRP_MN_COMPLETE) {
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
			(" IRP_MN_COMPLETE\n"));
		XixFsdMdlComplete(PtrIrpContext, PtrIrp, PtrIoStackLocation, TRUE);
		RC = STATUS_SUCCESS;
		return RC;
	}


	if (PtrIoStackLocation->MinorFunction & IRP_MN_DPC) {
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					("IRP_MN_DPC PostRequest IrpContext(%p) Irp(%p)\n", PtrIrpContext, PtrIrp));
		RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);
		return RC;
	}

	PtrFileObject = PtrIoStackLocation->FileObject;
	ASSERT(PtrFileObject);

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
		(" Decode File Object\n"));
	
	TypeOfOpen = XixFsdDecodeFileObject( PtrFileObject, &PtrFCB, &PtrCCB );

	if((TypeOfOpen == UnopenedFileObject) || (TypeOfOpen == UserDirectoryOpen))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			(" Un supported type %ld\n", TypeOfOpen));
		
		if(TypeOfOpen == UserDirectoryOpen){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Read Un supported by Dir Name(%wZ) CCB(%p) %ld\n", &PtrFCB->FCBName, PtrCCB, TypeOfOpen));
		
		}
			
		RC = STATUS_INVALID_DEVICE_REQUEST;
		

	
		XixFsdCompleteRequest(PtrIrpContext, RC, 0);
		return RC;
	}


	PtrVCB = PtrFCB->PtrVCB;
	ASSERT_VCB(PtrVCB);


	if(XifsdCheckFlagBoolean(PtrFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
		RC = STATUS_INVALID_DEVICE_REQUEST;
		return RC;
	}

	// Get some of the parameters supplied to us
	ByteOffset = PtrIoStackLocation->Parameters.Read.ByteOffset;
	ReadLength = PtrIoStackLocation->Parameters.Read.Length;

	CanWait = ((PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_WAIT) ? TRUE : FALSE);
	PagingIo = ((PtrIrp->Flags & IRP_PAGING_IO) ? TRUE : FALSE);
	NonBufferedIo = ((PtrIrp->Flags & IRP_NOCACHE) ? TRUE : FALSE);
	SynchronousIo = ((PtrFileObject->Flags & FO_SYNCHRONOUS_IO) ? TRUE : FALSE);

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
				(" Param ByteOffset(%I64d) ReadLength(%ld)" 
					" PagingIO(%s) NonBufferedIo(%s)\n",
				ByteOffset,ReadLength,
				((PagingIo == TRUE)?"TRUE":"FALSE"),
				((NonBufferedIo == TRUE)?"TRUE":"FALSE")
				));






	if (ReadLength == 0) {
		RC=STATUS_SUCCESS;
		XixFsdCompleteRequest(PtrIrpContext, RC, 0);
		return RC;
	}

	if(TypeOfOpen == UserVolumeOpen){
		NonBufferedIo = TRUE;
	}

/*	
	if(ReadLength > XIFSD_MAX_IO_LIMIT){
		ReadLength = XIFSD_MAX_IO_LIMIT;
	}
*/

/*
	if(NonBufferedIo){
		if(!CanWait){
		DebugTrace(0, (DEBUG_TRACE_TRACE| DEBUG_TRACE_FILE_RW), (" Post request \n"));
		RC = XifsdPostRequest(PtrIrpContext, PtrIrp);
		return RC;
		}
	}
*/
	if(PagingIo){
		if(!ExAcquireResourceSharedLite(PtrFCB->PagingIoResource, CanWait)){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					("PostRequest IrpContext(%p) Irp(%p)\n", PtrIrpContext, PtrIrp));
			RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);
			return RC;
		}
		PtrResourceAcquired = PtrFCB->PagingIoResource;
	}else{
		if(!ExAcquireResourceSharedLite(PtrFCB->Resource, CanWait)){
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					("PostRequest IrpContext(%p) Irp(%p)\n", PtrIrpContext, PtrIrp));
			RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);
			return RC;
		}
		PtrResourceAcquired = PtrFCB->Resource;
	}
	

	try {

		
		
        if ((TypeOfOpen != UserVolumeOpen) || (NULL == PtrCCB) ||
            !XifsdCheckFlagBoolean( PtrCCB->CCBFlags, XIFSD_CCB_DISMOUNT_ON_CLOSE))  {
            
			if(!XixFsdVerifyFcbOperation( PtrIrpContext, PtrFCB )){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					(" XifsdCommonRead  Fail VerifyFcbOperation FileName(%wZ)\n", &PtrFCB->FCBName));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);				
			}			
        }

	

	/*
			if (XifsdCheckFlag(PtrFCB->Type, XIFS_FD_TYPE_PAGE_FILE)) {
				IoMarkIrpPending(PtrIrp);
				XifsdPageFileIo(PtrFCB, PtrIrpContext, PtrIrp, ByteOffset.QuadPart, ReadLength, TRUE);
				try_return(RC = STATUS_PENDING);
			}
	*/


		
		if(TypeOfOpen == UserVolumeOpen){
			
			

			if(ByteOffset.QuadPart >= PtrFCB->FileSize.QuadPart){
				RC = STATUS_END_OF_FILE;
				PtrIrp->IoStatus.Status = RC;
				NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
				try_return(RC);	
			}


			if(ReadLength >(uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart)){
				ReadLength = (uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart);
			}

			if(ReadLength ==0){
				RC = STATUS_SUCCESS;
				PtrIrp->IoStatus.Status = RC;
				NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
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
					PtrIrpContext->IoContext->Async.RequestedByteCount = ReadLength;
				}
			}

			PtrIrpContext->IoContext->pIrpContext = PtrIrpContext;
			PtrIrpContext->IoContext->pIrp = PtrIrp;

			
			
			RC = XixFsdNonCachedIo (
			 	PtrFCB,
				PtrIrpContext,
				PtrIrp,
				ByteOffset.QuadPart,
				ReadLength,
				TRUE,
				TypeOfOpen,
				PtrResourceAcquired
				);


			if(RC == STATUS_PENDING){
				PtrIrpContext->Irp = NULL;
				PtrResourceAcquired = FALSE;

			}else{

			
				if (NT_SUCCESS( RC )) {				

					if (SynchronousIo && !PagingIo){
						PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + ReadLength;
					}

					NumberBytesRead = ReadLength;
					
				}
				

			}

			try_return(RC);
		}
		



		




        if (TypeOfOpen == UserFileOpen) {




			if (NonBufferedIo && !PagingIo && (PtrFCB->SectionObject.DataSectionObject != NULL)) {
				//if	(!PagingIo || (XIFSD_REQ_NOT_VIA_CACHE_MGR(&(PtrFCB->SectionObject)))) {


					if(!ExAcquireResourceSharedLite(PtrFCB->PagingIoResource, CanWait)){
						RC = STATUS_CANT_WAIT;
						try_return(RC);
					}

					CcFlushCache(&(PtrFCB->SectionObject), &ByteOffset, ReadLength, &(PtrIrp->IoStatus));

					ExReleaseResourceLite(PtrFCB->PagingIoResource);
					DbgPrint("CcFlush  6 File(%wZ)\n", &PtrFCB->FCBFullPath);
					if (!NT_SUCCESS(RC = PtrIrp->IoStatus.Status)) {
						try_return(RC);
					}
				//}
			}


			if(!PagingIo){
				//
				//  We check whether we can proceed
				//  based on the state of the file oplocks.
				//
				DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					(" UserFileOpen Oplock check\n"));
            
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

					DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					(" return PENDING UserFileOpen Oplock check\n"));

					PtrIrp = NULL;
					PtrIrpContext = NULL;

					try_return( RC );
				}	
				
				XifsdLockFcb(TRUE,PtrFCB);
				PtrFCB->IsFastIoPossible= FastIoIsPossible;
				XifsdUnlockFcb(TRUE,PtrFCB);

			
				if ((PtrFCB->FCBFileLock != NULL) &&
					!FsRtlCheckLockForReadAccess( PtrFCB->FCBFileLock, PtrIrp )) {

					try_return( RC = STATUS_FILE_LOCK_CONFLICT );
				}


			}


 	

			if(ByteOffset.QuadPart >= PtrFCB->FileSize.QuadPart){
				RC = STATUS_END_OF_FILE;
				PtrIrp->IoStatus.Status = RC;
				NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
				try_return(RC);	
			}


			if(ReadLength >(uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart)){
				ReadLength = (uint32)(PtrFCB->FileSize.QuadPart - ByteOffset.QuadPart);
			}



			if(ByteOffset.QuadPart >= PtrFCB->ValidDataLength.QuadPart){
				RC = STATUS_END_OF_FILE;
				PtrIrp->IoStatus.Status = RC;
				NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
				try_return(RC);	
			}


			if(ReadLength >(uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart)){
				ReadLength = (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
			}
			
		



			
			if (!NonBufferedIo) 
			{
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					(" Non Buffered Io\n"));

				if (PtrFileObject->PrivateCacheMap == NULL) {
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					(" Initialize Cache Map of PtrFileObject (%p) PtrFCB(%I64d)\n", PtrFileObject, PtrFCB->LotNumber));

					//DbgPrint("1 Initialize Cache Map of Name (%wZ) \n", &PtrFCB->FCBFullPath);

					CcInitializeCacheMap(PtrFileObject, (PCC_FILE_SIZES)&(PtrFCB->AllocationSize),
						FALSE,		// We will not utilize pin access for this file
						&(XiGlobalData.XifsCacheMgrCallBacks), // callbacks
						PtrFCB);// The context used in callbacks

					CcSetReadAheadGranularity( PtrFileObject, XIXFS_READ_AHEAD_GRANULARITY );

					XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_CACHED_FILE);
					RC = STATUS_SUCCESS;
				}


				if(
					(CanWait) &&
					((ByteOffset.QuadPart + ReadLength) > PtrFCB->ValidDataLength.QuadPart)
				){
					XifsdLockFcb(NULL, PtrFCB);
					IsEOFProcessing =  (!XifsdCheckFlagBoolean(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
											|| XixFsdCheckEofWrite(PtrFCB, &ByteOffset, ReadLength));

					XifsdUnlockFcb(NULL, PtrFCB);
					
					if(IsEOFProcessing){
						XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO);
					}

					
				}


				if(IsEOFProcessing){
					if(ByteOffset.QuadPart >= PtrFCB->ValidDataLength.QuadPart){
						RC = STATUS_END_OF_FILE;
						PtrIrp->IoStatus.Status = RC;
						NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
						try_return(RC);					
					}

					ReadLength = (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
				}

				if(ReadLength ==0){
					RC = STATUS_SUCCESS;
					PtrIrp->IoStatus.Status = RC;
					NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
					try_return(RC);					
				}

				// Check and see if this request requires a MDL returned to the caller
				if (PtrIoStackLocation->MinorFunction & IRP_MN_MDL) {
					CcMdlRead(PtrFileObject, 
								&ByteOffset, 
								ReadLength, 
								&(PtrIrp->MdlAddress), 
								&(PtrIrp->IoStatus)
								);

					NumberBytesRead = (uint32)PtrIrp->IoStatus.Information;
					RC = PtrIrp->IoStatus.Status;
				}else{
					PtrSystemBuffer = XixFsdGetCallersBuffer(PtrIrp);
					ASSERT(PtrSystemBuffer);
					if (!CcCopyRead(PtrFileObject, 
									&(ByteOffset), 
									ReadLength, 
									CanWait, 
									PtrSystemBuffer, 
									&(PtrIrp->IoStatus)
									)
					) {
						// The caller was not prepared to block and data is not immediately
						// available in the system cache
						try_return(RC = STATUS_CANT_WAIT);
					}

					// We have the data
					RC = PtrIrp->IoStatus.Status;
					NumberBytesRead = (uint32)PtrIrp->IoStatus.Information;
				}

				if (SynchronousIo && !PagingIo && NT_SUCCESS( RC )) {

					PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + ReadLength;
				}	


				try_return(RC);

			} else {

			

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
					(" Buffered Io Io\n"));


		
				
				


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
						PtrIrpContext->IoContext->Async.RequestedByteCount = ReadLength;
					}
				}


				if(	
					(CanWait) &&
					((ByteOffset.QuadPart + ReadLength) > PtrFCB->ValidDataLength.QuadPart)
				){
					XifsdLockFcb(NULL, PtrFCB);
					IsEOFProcessing =  (!XifsdCheckFlagBoolean(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO) 
											|| XixFsdCheckEofWrite(PtrFCB, &ByteOffset, ReadLength));

								


					XifsdUnlockFcb(NULL, PtrFCB);
					
					if(IsEOFProcessing){
						XifsdSetFlag(PtrFCB->FCBFlags, XIFSD_FCB_EOF_IO);
					}

					
				}


				if(IsEOFProcessing){				
					if(ByteOffset.QuadPart >= PtrFCB->ValidDataLength.QuadPart){
						RC = STATUS_END_OF_FILE;
						PtrIrp->IoStatus.Status = RC;
						NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
						try_return(RC);					
					}

					ReadLength = (uint32)(PtrFCB->ValidDataLength.QuadPart - ByteOffset.QuadPart);
				}

		
				if(ReadLength ==0){
					RC = STATUS_SUCCESS;
					PtrIrp->IoStatus.Status = RC;
					NumberBytesRead = (uint32)PtrIrp->IoStatus.Information = 0;
					try_return(RC);					
				}

			
				PtrIrpContext->IoContext->pIrpContext = PtrIrpContext;
				PtrIrpContext->IoContext->pIrp = PtrIrp;

				
				
				RC = XixFsdNonCachedIo (
			 		PtrFCB,
					PtrIrpContext,
					PtrIrp,
					ByteOffset.QuadPart,
					ReadLength,
					TRUE,
					TypeOfOpen,
					PtrResourceAcquired
					);
				
				if(RC == STATUS_PENDING){
					PtrIrpContext->Irp = NULL;
					PtrResourceAcquired = FALSE;

				}else{

					if((RC == STATUS_ACCESS_DENIED)
						&& (!CanWait) 
					)
					{
						DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
							(" XixFsdDoDelayProcessing\n"));
						RC = XixFsdDoDelayProcessing(PtrIrpContext, XixFsdDoDelayNonCachedIo);
						DelayedOP = TRUE;
						NumberBytesRead = 0;
					}else{
						if (NT_SUCCESS( RC )) {				

							if (SynchronousIo && !PagingIo){
								PtrIoStackLocation->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + ReadLength;
							}

							NumberBytesRead = ReadLength;
							
						}
					}

				}
			
			}
		}
	} 
	finally {

		DebugUnwind( "XifsdCommonRead" );

		if(IsEOFProcessing){

			XifsdLockFcb(NULL, PtrFCB);
			XixFsdFinishIoEof(PtrFCB);
			XifsdUnlockFcb(NULL, PtrFCB);
		}


		if(!DelayedOP){
			if(PtrResourceAcquired){
				ExReleaseResourceLite(PtrResourceAcquired);
			}
			// Post IRP if required
			if (RC == STATUS_CANT_WAIT) {

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
						("PostRequest IrpContext(%p) Irp(%p)\n", PtrIrpContext, PtrIrp));
				RC = XixFsdPostRequest(PtrIrpContext, PtrIrp);

			} else {
				XixFsdCompleteRequest(PtrIrpContext, RC, NumberBytesRead );
			}
		}
	} // end of "finally" processing


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_FCB),
			(" Exit XifsdCommonRead (0x%x) NumBytesRead(%ld)\n", RC, NumberBytesRead));
	return(RC);
}