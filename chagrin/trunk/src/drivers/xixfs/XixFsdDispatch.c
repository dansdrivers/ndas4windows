#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"




NTSTATUS
XixFsdDispatchRequest(
	IN PXIFS_IRPCONTEXT IrpContext
);




NTSTATUS 
XixFsdExceptionHandler(
	PXIFS_IRPCONTEXT				PtrIrpContext,
	PIRP								Irp
);




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdDispatchRequest)
#pragma alloc_text(PAGE, XixFsdDispatch)
#pragma alloc_text(PAGE, XixFsdCommonDispatch)
#pragma alloc_text(PAGE, XixFsdMdlComplete)
#pragma alloc_text(PAGE, XixFsdCompleteRequest)
#pragma alloc_text(PAGE, XixFsdPostRequest)
#pragma alloc_text(PAGE, XixFsdDoDelayProcessing)
#pragma alloc_text(PAGE, XixFsdDoDelayNonCachedIo)
#endif



NTSTATUS
XixFsdDispatchRequest(
	IN PXIFS_IRPCONTEXT IrpContext
){
	NTSTATUS RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDispatchRequest\n"));


	
   	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT(IrpContext->Irp);
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT), 
				 ("[MJ: 0x%02x] [MN: 0x%02x] \n",IrpContext->MajorFunction, IrpContext->MinorFunction));
	
   	switch(IrpContext->MajorFunction){
   	case IRP_MJ_CREATE:
		
   		RC= XixFsdCommonCreate(IrpContext);
   		break;
   	case  IRP_MJ_CLOSE:
		
   		RC = XixFsdCommonClose(IrpContext);
   		break;
   	case IRP_MJ_CLEANUP:
	
   		RC = XixFsdCommonCleanUp(IrpContext);
   		break;   		
   	case IRP_MJ_READ:
	
   		RC = XixFsdCommonRead(IrpContext);
   		break;
   	case IRP_MJ_WRITE:
		
   		RC = XixFsdCommonWrite(IrpContext);
   		break;
   	case IRP_MJ_QUERY_INFORMATION:
		
   		RC = XixFsdCommonQueryInformation(IrpContext);
   		break;
   	case IRP_MJ_SET_INFORMATION:
		
   		RC = XixFsdCommonSetInformation(IrpContext);
   		break;
   	case IRP_MJ_FLUSH_BUFFERS:
		
   		RC = XixFsdCommonFlushBuffers(IrpContext);
   		break;
   	case IRP_MJ_QUERY_VOLUME_INFORMATION:
		
   		RC = XixFsdCommonQueryVolumeInformation(IrpContext);
   		break;
   	case IRP_MJ_SET_VOLUME_INFORMATION:
		
   		RC = XixFsdCommonSetVolumeInformation(IrpContext);
   		break;
   	case IRP_MJ_DIRECTORY_CONTROL:
		
   		RC = XixFsdCommonDirectoryControl(IrpContext);
   		break;
   	case IRP_MJ_FILE_SYSTEM_CONTROL:
		
   		RC = XixFsdCommonFileSystemControl(IrpContext);
   		break;
   	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
   	case IRP_MJ_DEVICE_CONTROL:
		
   		RC = XixFsdCommonDeviceControl(IrpContext);
   		break;
   	case IRP_MJ_SHUTDOWN:
		
   		RC = XixFsdCommonShutdown(IrpContext);
   		break;
   	case IRP_MJ_LOCK_CONTROL:
		
   		RC = XixFsdCommonLockControl(IrpContext);
   		break;
   	case IRP_MJ_QUERY_SECURITY:
		
   		RC = XixFsdCommonQuerySecurity(IrpContext);
   		break;
   	case IRP_MJ_SET_SECURITY:
		
   		RC = XixFsdCommonSetSecurity(IrpContext);
   		break;
   	case IRP_MJ_QUERY_EA:
		
   		RC = XixFsdCommonQueryEA(IrpContext);
   		break;
   	case IRP_MJ_SET_EA:
		
   		RC = XixFsdCommonSetEA(IrpContext);
   		break;
   	case IRP_MJ_PNP:
		
   		RC = XixFsdCommonPNP(IrpContext);
		break;
   	default:
		DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter Other \n"));
   		RC = STATUS_INVALID_DEVICE_REQUEST;
              XixFsdCompleteRequest( IrpContext, RC, 0 );
   		break;	
   	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdDispatchRequest\n"));
   	return RC;
}



LONG
XixFsdExceptionFilter (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    )
{
    NTSTATUS ExceptionCode;
    BOOLEAN TestStatus = FALSE;
	long	ReturnCode = EXCEPTION_EXECUTE_HANDLER;

	DebugTrace(DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdExceptionFilter\n"));


	ASSERT(ExceptionPointer);

	ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionCode;
	

	DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
				 ("XixFsdExceptionFilter: %08x (exr %08x cxr %08x)\n",
				 ExceptionCode,
				 ExceptionPointer->ExceptionRecord,
				 ExceptionPointer->ContextRecord ));


	//
	// If the exception is STATUS_IN_PAGE_ERROR, get the I/O error code
	// from the exception record.
	//

	if ((ExceptionCode == STATUS_IN_PAGE_ERROR) &&
		(ExceptionPointer->ExceptionRecord->NumberParameters >= 3)) {

		ExceptionCode = (NTSTATUS) ExceptionPointer->ExceptionRecord->ExceptionInformation[2];
	}
	
    //
    //  If there is an Irp context then check which status code to use.
    //

    if ( IrpContext ) {

		DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
				("XixFsdExceptionFilter MAJOR(0x%x) MINOR(0x%x)\n", 
				IrpContext->MajorFunction, IrpContext->MinorFunction));


        if (IrpContext->SavedExceptionCode == STATUS_SUCCESS) {

			if(FsRtlIsNtstatusExpected( ExceptionCode )){

				IrpContext->SavedExceptionCode = ExceptionCode;
				TestStatus = TRUE;

				DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
						("XixFsdExceptionFilter Expected MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

				return EXCEPTION_EXECUTE_HANDLER;
			}else{
				TestStatus = FALSE;

				DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
						("XixFsdExceptionFilter Unexpected MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

				XifsdBugCheck( (ULONG_PTR) ExceptionPointer->ExceptionRecord,
						 (ULONG_PTR) ExceptionPointer->ContextRecord,
						 (ULONG_PTR) ExceptionPointer->ExceptionRecord->ExceptionAddress );
			}

  
        } else {

			DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
						("XixFsdExceptionFilter Double Exception MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

			ASSERT( IrpContext->SavedExceptionCode == ExceptionCode );
			ASSERT( FsRtlIsNtstatusExpected( ExceptionCode ) );         
		   
        }

		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_EXCEPTION);
    
	}else{
		if(!FsRtlIsNtstatusExpected( ExceptionCode )){
			 XifsdBugCheck( (ULONG_PTR) ExceptionPointer->ExceptionRecord,
							 (ULONG_PTR) ExceptionPointer->ContextRecord,
							 (ULONG_PTR) ExceptionPointer->ExceptionRecord->ExceptionAddress );			
		}

		return EXCEPTION_EXECUTE_HANDLER;
    }

	/*
    //
    //  Bug check if this status is not supported.
    //

    if (TestStatus && !FsRtlIsNtstatusExpected( ExceptionCode )) {

		if (IrpContext) {
			XifsdReleaseIrpContext(IrpContext);
		}
		
        XifsdBugCheck( (ULONG_PTR) ExceptionPointer->ExceptionRecord,
                     (ULONG_PTR) ExceptionPointer->ContextRecord,
                     (ULONG_PTR) ExceptionPointer->ExceptionRecord->ExceptionAddress );
		
		

    }

	DebugTrace(DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XifsdExceptionFilter\n"));

    return ReturnCode;
	*/
	return EXCEPTION_EXECUTE_HANDLER;
}





NTSTATUS 
XixFsdExceptionHandler(
	PXIFS_IRPCONTEXT				PtrIrpContext,
	PIRP								Irp)
{
	NTSTATUS						RC = STATUS_INSUFFICIENT_RESOURCES;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdExceptionHandler\n"));

	if (PtrIrpContext) {
		RC = PtrIrpContext->SavedExceptionCode;

	} else {
		// must be insufficient resources ...?
		RC = STATUS_INSUFFICIENT_RESOURCES;
	}


	if(RC != STATUS_CANT_WAIT){
		if(Irp){
			// set the error code in the IRP
			Irp->IoStatus.Status = RC;
			Irp->IoStatus.Information = 0;

			// complete the IRP
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
		}


		if(PtrIrpContext){
			if(!XifsdCheckFlagBoolean(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){
				IoSetTopLevelIrp(NULL);
			}


			// Free irp context here
			XixFsdReleaseIrpContext(PtrIrpContext);
		}

	}else{

		if(PtrIrpContext){
			PtrIrpContext->SavedExceptionCode = STATUS_SUCCESS;
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdExceptionHandler\n"));
	return(RC);
}


NTSTATUS
XixFsdDispatch (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP Irp)
{

	NTSTATUS			RC = STATUS_SUCCESS;
   	PXIFS_IRPCONTEXT	PtrIrpContext = NULL;
	BOOLEAN				TopLevel = FALSE;
	BOOLEAN				bCtolDeviceIo = FALSE;
	PIO_REMOVE_LOCK		pIoRemoveLock = NULL;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDispatch\n"));

	FsRtlEnterFileSystem();
	ASSERT(DeviceObject);
	ASSERT(Irp);
	
	if (DeviceObject == XiGlobalData.XifsControlDeviceObject) {
		PIO_STACK_LOCATION	PtrIoStackLocation = NULL;

		pIoRemoveLock = (PIO_REMOVE_LOCK)((PUCHAR)DeviceObject+sizeof(DEVICE_OBJECT));	
		ASSERT(pIoRemoveLock);

		PtrIoStackLocation = IoGetCurrentIrpStackLocation(Irp);

			
		if((PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL 
				&&	PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == NDAS_XIXFS_UNLOAD)
			|| PtrIoStackLocation->MajorFunction == IRP_MJ_CLEANUP 
			|| PtrIoStackLocation->MajorFunction == IRP_MJ_CLOSE
		)
		{
			bCtolDeviceIo = FALSE;
		}else{
			bCtolDeviceIo = TRUE;

			RC = IoAcquireRemoveLock(pIoRemoveLock, (PVOID)TAG_REMOVE_LOCK);
			
			if(!NT_SUCCESS(RC)){
				Irp->IoStatus.Status = RC;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				FsRtlExitFileSystem();
				return RC;
			}

		}
	}


	if (!IoGetTopLevelIrp())
	{
		TopLevel = TRUE;
	    IoSetTopLevelIrp(Irp);
	}

	do{
		try
		{
			if(PtrIrpContext == NULL){
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT), ("Allocate IrpContext\n"));
				PtrIrpContext = XixFsdAllocateIrpContext(Irp, DeviceObject);

			}

			if(PtrIrpContext == NULL){
				
				if(IoGetTopLevelIrp() == Irp){
					IoSetTopLevelIrp(NULL);	
				}

				RC = STATUS_INSUFFICIENT_RESOURCES;
				Irp->IoStatus.Status = RC;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				


			}else{
				
				ASSERT_IRPCONTEXT(PtrIrpContext);

				RC= XixFsdDispatchRequest(PtrIrpContext);
			}
			
		   
		}except (XixFsdExceptionFilter(PtrIrpContext, GetExceptionInformation()))
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
			RC = XixFsdExceptionHandler(PtrIrpContext, Irp);
		}

	}while( RC == STATUS_CANT_WAIT);
		
	if(TopLevel){
		IoSetTopLevelIrp(NULL);
	}
	
	if(bCtolDeviceIo){
		IoReleaseRemoveLock(pIoRemoveLock, (PVOID)TAG_REMOVE_LOCK);
	}

	FsRtlExitFileSystem();

 	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdDispatch\n"));   
    return RC;
}



NTSTATUS
XixFsdCommonDispatch(
	IN PVOID		Context
	)
{

	NTSTATUS					RC = STATUS_SUCCESS;
	PXIFS_IRPCONTEXT			PtrIrpContext = NULL;
	PIRP						PtrIrp = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonDispatch\n"));

	// The context must be a pointer to an IrpContext structure
	PtrIrpContext = (PXIFS_IRPCONTEXT)Context;
	ASSERT(PtrIrpContext);

	if ((PtrIrpContext->NodeTypeCode != XIFS_NODE_IRPCONTEXT) 
		|| (PtrIrpContext->NodeByteSize != sizeof(XIFS_IRPCONTEXT))) {
		// Panic
		//SFsdPanic(SFSD_ERROR_INTERNAL_ERROR, PtrIrpContext->NodeIdentifier.NodeType, PtrIrpContext->NodeIdentifier.NodeSize);
	}

	PtrIrp = PtrIrpContext->Irp;
	ASSERT(PtrIrp);


	if (PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL) {
		IoSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);
	}

	XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	
	try
	{
	    try
	    {
	       FsRtlEnterFileSystem();
		RC= XixFsdDispatchRequest(PtrIrpContext);
	       
	    }
	    except (XixFsdExceptionFilter(PtrIrpContext, GetExceptionInformation()))
	    {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
	        RC = XixFsdExceptionHandler(PtrIrpContext, PtrIrp);
	    }
	}
	finally
	{

	    IoSetTopLevelIrp(NULL);
	    FsRtlExitFileSystem();
	}
   
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonDispatch\n"));
	
    return RC;
}



VOID
XixFsdMdlComplete(
	PXIFS_IRPCONTEXT			PtrIrpContext,
	PIRP						PtrIrp,
	PIO_STACK_LOCATION			PtrIoStackLocation,
	BOOLEAN						ReadCompletion
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PFILE_OBJECT			PtrFileObject = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdMdlComplete\n"));

	PtrFileObject = PtrIoStackLocation->FileObject;
	ASSERT(PtrFileObject);

	// Not much to do here.
	if (ReadCompletion) {
		CcMdlReadComplete(PtrFileObject, PtrIrp->MdlAddress);
	} else {
		// The Cache Manager needs the byte offset in the I/O stack location.
      		CcMdlWriteComplete(PtrFileObject, &(PtrIoStackLocation->Parameters.Write.ByteOffset), PtrIrp->MdlAddress);
	}

	// Clear the MDL address field in the IRP so the IoCompleteRequest()
	// does not try to play around with the MDL.
	PtrIrp->MdlAddress = NULL;

	// Free up the Irp Context.
	XixFsdReleaseIrpContext(PtrIrpContext);

	// Complete the IRP.
	PtrIrp->IoStatus.Status = RC;
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE|DEBUG_TARGET_IRPCONTEXT), 
		(" XifsdMdlComplete Status(%x) Information(%ld)", RC, PtrIrp->IoStatus.Information));

	//PtrIrp->IoStatus.Information = 0;
	IoCompleteRequest(PtrIrp, IO_NO_INCREMENT);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdMdlComplete \n"));

	return;
}



VOID
XixFsdCompleteRequest(
	IN PXIFS_IRPCONTEXT		IrpContext,
	IN NTSTATUS				Status,
	IN uint32				ReturnByte)
{
	PIRP		Irp;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCompleteRequest\n"));


	if(ARGUMENT_PRESENT(IrpContext)){
	
		Irp = IrpContext->Irp;
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT), 
				("XixFsdCompleteRequest MJ(0x%x) MN(0x%x) .\n", IrpContext->MajorFunction, IrpContext->MinorFunction));

		
		if(Irp){
			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT), 
				("Complete Status(0x%x) Information(0x%x) .\n", Status, ReturnByte));
			
			Irp->IoStatus.Status = Status;
			Irp->IoStatus.Information = ReturnByte;
			

				



			IoCompleteRequest(Irp, IO_NO_INCREMENT);
		}

		if(!XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){
			IoSetTopLevelIrp(NULL);
		}

		// Free irp context here
		XixFsdReleaseIrpContext(IrpContext);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCompleteRequest\n"));
}




NTSTATUS
XixFsdPostRequest(
	IN PXIFS_IRPCONTEXT PtrIrpContext,
	IN PIRP				Irp
)
{
	NTSTATUS			RC = STATUS_PENDING;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdPostRequest\n"));

	IoMarkIrpPending(Irp);
	

	ExInitializeWorkItem(&(PtrIrpContext->WorkQueueItem), XixFsdCommonDispatch, PtrIrpContext);

	ExQueueWorkItem(&(PtrIrpContext->WorkQueueItem), CriticalWorkQueue);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdPostRequest\n"));

	return(RC);
}


NTSTATUS
XixFsdDoDelayProcessing(
	IN PXIFS_IRPCONTEXT PtrIrpContext,
	IN XIFS_DELAYEDPROCESSING Func
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDoDelayProcessing\n"));

	IoMarkIrpPending(PtrIrpContext->Irp);
	

	ExInitializeWorkItem(&(PtrIrpContext->WorkQueueItem), Func, PtrIrpContext);

	ExQueueWorkItem(&(PtrIrpContext->WorkQueueItem), CriticalWorkQueue);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdDoDelayProcessing\n"));

	return STATUS_PENDING;
}


NTSTATUS
XixFsdDoDelayNonCachedIo(
	IN PVOID pContext 
)
{

	NTSTATUS					RC = STATUS_SUCCESS;
	PXIFS_IRPCONTEXT			PtrIrpContext = NULL;
	PIRP						PtrIrp = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDoDelayNonCachedIo\n"));

	// The context must be a pointer to an IrpContext structure
	PtrIrpContext = (PXIFS_IRPCONTEXT)pContext;
	ASSERT_IRPCONTEXT(PtrIrpContext);



	PtrIrp = PtrIrpContext->Irp;
	ASSERT(PtrIrp);


	if (PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL) {
		IoSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);
	}

	XifsdSetFlag(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	
	try
	{
	    try
	    {
			FsRtlEnterFileSystem();
			RC = XifsdProcessNonCachedIo(pContext);
	       
	    }
	    except (XixFsdExceptionFilter(PtrIrpContext, GetExceptionInformation()))
	    {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
			RC = XixFsdExceptionHandler(PtrIrpContext, PtrIrp);
	    }
	}
	finally
	{

	    IoSetTopLevelIrp(NULL);
	    FsRtlExitFileSystem();
	}
   
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdDoDelayNonCachedIo\n"));
	
    return RC;

}


