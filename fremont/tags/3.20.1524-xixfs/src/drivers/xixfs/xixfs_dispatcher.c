#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"




NTSTATUS
xixfs_DispatchRequest(
	IN PXIXFS_IRPCONTEXT IrpContext
);




NTSTATUS 
xixfs_ExceptionHandler(
	PXIXFS_IRPCONTEXT				PtrIrpContext,
	PIRP								Irp
);




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_DispatchRequest)
#pragma alloc_text(PAGE, xixfs_Dispatch)
#pragma alloc_text(PAGE, xixfs_CommonDispatch)
#pragma alloc_text(PAGE, xixfs_MdlComplete)
#pragma alloc_text(PAGE, xixfs_CompleteRequest)
#pragma alloc_text(PAGE, xixfs_PostRequest)
#pragma alloc_text(PAGE, xixfs_DoDelayProcessing)
#pragma alloc_text(PAGE, xixfs_DoDelayNonCachedIo)
#endif


NTSTATUS
xixfs_DispatchRequest(
	IN PXIXFS_IRPCONTEXT IrpContext
){
	NTSTATUS RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_DispatchRequest\n"));


	
   	ASSERT_IRPCONTEXT(IrpContext);
	ASSERT(IrpContext->Irp);
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT), 
				 ("[MJ: 0x%02x] [MN: 0x%02x] \n",IrpContext->MajorFunction, IrpContext->MinorFunction));
	
   	switch(IrpContext->MajorFunction){
   	case IRP_MJ_CREATE:
		
   		RC= xixfs_CommonCreate(IrpContext);
   		break;
   	case  IRP_MJ_CLOSE:
		
   		RC = xixfs_CommonClose(IrpContext);
   		break;
   	case IRP_MJ_CLEANUP:
	
   		RC = xixfs_CommonCleanUp(IrpContext);
   		break;   		
   	case IRP_MJ_READ:
	
   		RC = xixfs_CommonRead(IrpContext);
   		break;
   	case IRP_MJ_WRITE:
		
   		RC = xixfs_CommonWrite(IrpContext);
   		break;
   	case IRP_MJ_QUERY_INFORMATION:
		
   		RC = xixfs_CommonQueryInformation(IrpContext);
   		break;
   	case IRP_MJ_SET_INFORMATION:
		
   		RC = xixfs_CommonSetInformation(IrpContext);
   		break;
   	case IRP_MJ_FLUSH_BUFFERS:
		
   		RC = xixfs_CommonFlushBuffers(IrpContext);
   		break;
   	case IRP_MJ_QUERY_VOLUME_INFORMATION:
		
   		RC = xixfs_CommonQueryVolumeInformation(IrpContext);
   		break;
   	case IRP_MJ_SET_VOLUME_INFORMATION:
		
   		RC = xixfs_CommonSetVolumeInformation(IrpContext);
   		break;
   	case IRP_MJ_DIRECTORY_CONTROL:
		
   		RC = xixfs_CommonDirectoryControl(IrpContext);
   		break;
   	case IRP_MJ_FILE_SYSTEM_CONTROL:
		
   		RC = xixfs_CommonFileSystemControl(IrpContext);
   		break;
   	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
   	case IRP_MJ_DEVICE_CONTROL:
		
   		RC = xixfs_CommonDeviceControl(IrpContext);
   		break;
   	case IRP_MJ_SHUTDOWN:
		
   		RC = xixfs_CommonShutdown(IrpContext);
   		break;
   	case IRP_MJ_LOCK_CONTROL:
		
   		RC = xixfs_CommonLockControl(IrpContext);
   		break;
   	case IRP_MJ_QUERY_SECURITY:
		
   		RC = xixfs_CommonQuerySecurity(IrpContext);
   		break;
   	case IRP_MJ_SET_SECURITY:
		
   		RC = xixfs_CommonSetSecurity(IrpContext);
   		break;
   	case IRP_MJ_QUERY_EA:
		
   		RC = xixfs_CommonQueryEA(IrpContext);
   		break;
   	case IRP_MJ_SET_EA:
		
   		RC = xixfs_CommonSetEA(IrpContext);
   		break;
   	case IRP_MJ_PNP:
		
   		RC = xixfs_CommonPNP(IrpContext);
		break;
   	default:
		DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!Enter Other \n"));
   		RC = STATUS_INVALID_DEVICE_REQUEST;
              xixfs_CompleteRequest( IrpContext, RC, 0 );
   		break;	
   	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_DispatchRequest\n"));
   	return RC;
}



LONG
xixfs_ExceptionFilter (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    )
{
    NTSTATUS ExceptionCode;
    BOOLEAN TestStatus = FALSE;
	long	ReturnCode = EXCEPTION_EXECUTE_HANDLER;

	DebugTrace(DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_ExceptionFilter\n"));


	ASSERT(ExceptionPointer);

	ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionCode;
	

	DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
				 ("xixfs_ExceptionFilter: %08x (exr %08x cxr %08x)\n",
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

		DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
				("xixfs_ExceptionFilter MAJOR(0x%x) MINOR(0x%x)\n", 
				IrpContext->MajorFunction, IrpContext->MinorFunction));


        if (IrpContext->SavedExceptionCode == STATUS_SUCCESS) {

			if(FsRtlIsNtstatusExpected( ExceptionCode )){

				IrpContext->SavedExceptionCode = ExceptionCode;
				TestStatus = TRUE;

				DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
						("xixfs_ExceptionFilter Expected MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

				return EXCEPTION_EXECUTE_HANDLER;
			}else{
				TestStatus = FALSE;

				DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
						("xixfs_ExceptionFilter Unexpected MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

				XifsdBugCheck( (ULONG_PTR) ExceptionPointer->ExceptionRecord,
						 (ULONG_PTR) ExceptionPointer->ContextRecord,
						 (ULONG_PTR) ExceptionPointer->ExceptionRecord->ExceptionAddress );
			}

  
        } else {

			DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
						("xixfs_ExceptionFilter Double Exception MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

			ASSERT( IrpContext->SavedExceptionCode == ExceptionCode );
			ASSERT( FsRtlIsNtstatusExpected( ExceptionCode ) );         
		   
        }

		XIXCORE_SET_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_EXCEPTION);
    
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

	DebugTrace(DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit XifsdExceptionFilter\n"));

    return ReturnCode;
	*/
	return EXCEPTION_EXECUTE_HANDLER;
}





NTSTATUS 
xixfs_ExceptionHandler(
	PXIXFS_IRPCONTEXT				PtrIrpContext,
	PIRP								Irp)
{
	NTSTATUS						RC = STATUS_INSUFFICIENT_RESOURCES;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_ExceptionHandler\n"));

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
			if(!XIXCORE_TEST_FLAGS(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){
				IoSetTopLevelIrp(NULL);
			}


			// Free irp context here
			xixfs_ReleaseIrpContext(PtrIrpContext);
		}

	}else{

		if(PtrIrpContext){
			PtrIrpContext->SavedExceptionCode = STATUS_SUCCESS;
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_ExceptionHandler\n"));
	return(RC);
}


NTSTATUS
xixfs_Dispatch (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP Irp)
{

	NTSTATUS			RC = STATUS_SUCCESS;
   	PXIXFS_IRPCONTEXT	PtrIrpContext = NULL;
	BOOLEAN				TopLevel = FALSE;
	BOOLEAN				bCtolDeviceIo = FALSE;
	PIO_REMOVE_LOCK		pIoRemoveLock = NULL;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_Dispatch\n"));

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
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT), ("Allocate IrpContext\n"));
				PtrIrpContext = xixfs_AllocateIrpContext(Irp, DeviceObject);

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

				RC= xixfs_DispatchRequest(PtrIrpContext);
			}
			
		   
		}except (xixfs_ExceptionFilter(PtrIrpContext, GetExceptionInformation()))
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
			RC = xixfs_ExceptionHandler(PtrIrpContext, Irp);
		}

	}while( RC == STATUS_CANT_WAIT);
		
	if(TopLevel){
		IoSetTopLevelIrp(NULL);
	}
	
	if(bCtolDeviceIo){
		IoReleaseRemoveLock(pIoRemoveLock, (PVOID)TAG_REMOVE_LOCK);
	}

	FsRtlExitFileSystem();

 	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_Dispatch\n"));   
    return RC;
}



NTSTATUS
xixfs_CommonDispatch(
	IN PVOID		Context
	)
{

	NTSTATUS					RC = STATUS_SUCCESS;
	PXIXFS_IRPCONTEXT			PtrIrpContext = NULL;
	PIRP						PtrIrp = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonDispatch\n"));

	// The context must be a pointer to an IrpContext structure
	PtrIrpContext = (PXIXFS_IRPCONTEXT)Context;
	ASSERT(PtrIrpContext);

	if ((PtrIrpContext->NodeTypeCode != XIFS_NODE_IRPCONTEXT) 
		|| (PtrIrpContext->NodeByteSize != sizeof(XIXFS_IRPCONTEXT))) {
		// Panic
		//SFsdPanic(SFSD_ERROR_INTERNAL_ERROR, PtrIrpContext->NodeIdentifier.NodeType, PtrIrpContext->NodeIdentifier.NodeSize);
	}

	PtrIrp = PtrIrpContext->Irp;
	ASSERT(PtrIrp);


	if (PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL) {
		IoSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);
	}

	XIXCORE_SET_FLAGS(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	
	try
	{
	    try
	    {
	       FsRtlEnterFileSystem();
		RC= xixfs_DispatchRequest(PtrIrpContext);
	       
	    }
	    except (xixfs_ExceptionFilter(PtrIrpContext, GetExceptionInformation()))
	    {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
	        RC = xixfs_ExceptionHandler(PtrIrpContext, PtrIrp);
	    }
	}
	finally
	{

	    IoSetTopLevelIrp(NULL);
	    FsRtlExitFileSystem();
	}
   
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonDispatch\n"));
	
    return RC;
}



VOID
xixfs_MdlComplete(
	PXIXFS_IRPCONTEXT			PtrIrpContext,
	PIRP						PtrIrp,
	PIO_STACK_LOCATION			PtrIoStackLocation,
	BOOLEAN						ReadCompletion
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PFILE_OBJECT			PtrFileObject = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_MdlComplete\n"));

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
	xixfs_ReleaseIrpContext(PtrIrpContext);

	// Complete the IRP.
	PtrIrp->IoStatus.Status = RC;
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE|DEBUG_TARGET_IRPCONTEXT), 
		(" XifsdMdlComplete Status(%x) Information(%ld)", RC, PtrIrp->IoStatus.Information));

	//PtrIrp->IoStatus.Information = 0;
	IoCompleteRequest(PtrIrp, IO_NO_INCREMENT);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE|DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_MdlComplete \n"));

	return;
}



VOID
xixfs_CompleteRequest(
	IN PXIXFS_IRPCONTEXT		IrpContext,
	IN NTSTATUS				Status,
	IN uint32				ReturnByte)
{
	PIRP		Irp;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CompleteRequest\n"));


	if(ARGUMENT_PRESENT(IrpContext)){
	
		Irp = IrpContext->Irp;
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT), 
				("xixfs_CompleteRequest MJ(0x%x) MN(0x%x) .\n", IrpContext->MajorFunction, IrpContext->MinorFunction));

		
		if(Irp){
			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT), 
				("Complete Status(0x%x) Information(0x%x) .\n", Status, ReturnByte));
			
			Irp->IoStatus.Status = Status;
			Irp->IoStatus.Information = ReturnByte;
			

				



			IoCompleteRequest(Irp, IO_NO_INCREMENT);
		}

		if(!XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){
			IoSetTopLevelIrp(NULL);
		}

		// Free irp context here
		xixfs_ReleaseIrpContext(IrpContext);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_CompleteRequest\n"));
}




NTSTATUS
xixfs_PostRequest(
	IN PXIXFS_IRPCONTEXT PtrIrpContext,
	IN PIRP				Irp
)
{
	NTSTATUS			RC = STATUS_PENDING;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_PostRequest\n"));

	IoMarkIrpPending(Irp);
	

	ExInitializeWorkItem(&(PtrIrpContext->WorkQueueItem), xixfs_CommonDispatch, PtrIrpContext);

	ExQueueWorkItem(&(PtrIrpContext->WorkQueueItem), CriticalWorkQueue);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_PostRequest\n"));

	return(RC);
}


NTSTATUS
xixfs_DoDelayProcessing(
	IN PXIXFS_IRPCONTEXT PtrIrpContext,
	IN XIFS_DELAYEDPROCESSING Func
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_DoDelayProcessing\n"));

	IoMarkIrpPending(PtrIrpContext->Irp);
	

	ExInitializeWorkItem(&(PtrIrpContext->WorkQueueItem), Func, PtrIrpContext);

	ExQueueWorkItem(&(PtrIrpContext->WorkQueueItem), CriticalWorkQueue);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_DoDelayProcessing\n"));

	return STATUS_PENDING;
}


NTSTATUS
xixfs_DoDelayNonCachedIo(
	IN PVOID pContext 
)
{

	NTSTATUS					RC = STATUS_SUCCESS;
	PXIXFS_IRPCONTEXT			PtrIrpContext = NULL;
	PIRP						PtrIrp = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_DoDelayNonCachedIo\n"));

	// The context must be a pointer to an IrpContext structure
	PtrIrpContext = (PXIXFS_IRPCONTEXT)pContext;
	ASSERT_IRPCONTEXT(PtrIrpContext);



	PtrIrp = PtrIrpContext->Irp;
	ASSERT(PtrIrp);


	if (PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL) {
		IoSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);
	}

	XIXCORE_SET_FLAGS(PtrIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	
	try
	{
	    try
	    {
			FsRtlEnterFileSystem();
			RC = xixfs_ProcessFileNonCachedIo(pContext);
	       
	    }
	    except (xixfs_ExceptionFilter(PtrIrpContext, GetExceptionInformation()))
	    {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
			RC = xixfs_ExceptionHandler(PtrIrpContext, PtrIrp);
	    }
	}
	finally
	{

	    IoSetTopLevelIrp(NULL);
	    FsRtlExitFileSystem();
	}
   
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT| DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_DoDelayNonCachedIo\n"));
	
    return RC;

}


