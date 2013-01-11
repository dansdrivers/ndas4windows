#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsRawDiskAccessApi.h"


uint32		TestVal = 0;

PXIFS_IRPCONTEXT 
XifsdAllocateIrpContext(
	PIRP				Irp,
	PDEVICE_OBJECT		PtrTargetDeviceObject)
{
	BOOLEAN				IsFromLookasideList = FALSE;
	PXIFS_IRPCONTEXT 	IrpContext = NULL;
	PIO_STACK_LOCATION	PtrIoStackLocation = NULL;
	BOOLEAN				IsFsDo = FALSE;
	
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Enter XifsdAllocateIrpContext Irp(%p) TargetDevObj(%p)\n", Irp, PtrTargetDeviceObject));

	ASSERT(Irp);

	PtrIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
	
	if(PtrIoStackLocation->DeviceObject == XiGlobalData.XifsControlDeviceObject){
		IsFsDo = TRUE;
	}


	if(IsFsDo){
		if(PtrIoStackLocation->FileObject != NULL 
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_CREATE
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_CLEANUP
			&& PtrIoStackLocation->MajorFunction != IRP_MJ_CLOSE
		)
		{
			ExRaiseStatus(STATUS_INVALID_DEVICE_REQUEST);
		}
		
		ASSERT(PtrIoStackLocation->FileObject != NULL ||
			(PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->MinorFunction == IRP_MN_USER_FS_REQUEST &&
				PtrIoStackLocation->Parameters.FileSystemControl.FsControlCode == FSCTL_INVALIDATE_VOLUMES) 
			|| (PtrIoStackLocation->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
				PtrIoStackLocation->MinorFunction == IRP_MN_MOUNT_VOLUME ) 
			|| PtrIoStackLocation->MajorFunction == IRP_MJ_SHUTDOWN 
		);
	}





	// allocate memory
	IrpContext = (PXIFS_IRPCONTEXT)ExAllocateFromNPagedLookasideList(&(XifsIrpContextLookasideList));

	if(!IrpContext)
	{
		IrpContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(XIFS_IRPCONTEXT),TAG_IPCONTEXT);
		if(!IrpContext)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("ERROR XifsdAllocateIrpContext Can't allocate\n"));
			
			return NULL;
		}
		IsFromLookasideList = FALSE;
	} else {
		IsFromLookasideList = TRUE;
	}

	RtlZeroMemory(IrpContext,sizeof(XIFS_IRPCONTEXT));

	IrpContext->NodeTypeCode = XIFS_NODE_IRPCONTEXT;
	IrpContext->NodeByteSize = sizeof(XIFS_IRPCONTEXT);
	IrpContext->TargetDeviceObject = PtrTargetDeviceObject;



	IrpContext->Irp = Irp;

	if (IoGetTopLevelIrp() != Irp) {
		// We are not top-level. Note this fact in the context structure
		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL), 
			("XifsdAllocateIrpContext IrpContextFlags(0x%x)\n",
				IrpContext->IrpContextFlags));
	}
		


	IrpContext->MajorFunction = PtrIoStackLocation->MajorFunction;
	IrpContext->MinorFunction = PtrIoStackLocation->MinorFunction;
	IrpContext->SavedExceptionCode = STATUS_SUCCESS;	

	if(XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL)){

	}else if (PtrIoStackLocation->FileObject == NULL){
		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("XifsdAllocateIrpContext Set Watable form FileObject== NULL IrpContextFlags(0x%x)\n",
					IrpContext->IrpContextFlags));
	} else {
		if (IoIsOperationSynchronous(Irp)) {
			XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
			
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
				("XifsdAllocateIrpContext Set Watable form IoIsOperationSynchronous IrpContextFlags(0x%x)\n",
					IrpContext->IrpContextFlags));

		}
	}
		
	if(!IsFsDo){
		IrpContext->VCB = &((PXI_VOLUME_DEVICE_OBJECT)PtrIoStackLocation->DeviceObject)->VCB;
	}
	
	if (IsFromLookasideList == FALSE) {
		XifsdSetFlag(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_NOT_FROM_POOL);
		
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
			("XifsdAllocateIrpContext IrpContextFlags(0x%x)\n",
				IrpContext->IrpContextFlags));

	}


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT),
			("[IrpCxt(%p) INFO] Irp(%x):MJ(%x):MN(%x):Flags(%x)\n",
				IrpContext, Irp, IrpContext->MajorFunction, IrpContext->MinorFunction, IrpContext->IrpContextFlags));


	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Exit XifsdAllocateIrpContext IrpContext(%p) Irp(%p) TargetDevObj(%p)\n",
			IrpContext, Irp, PtrTargetDeviceObject));

	return IrpContext;
}



void XifsdReleaseIrpContext(
	PXIFS_IRPCONTEXT					PtrIrpContext)
{
	PAGED_CODE();
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Enter XifsdReleaseIrpContext(%p)\n", PtrIrpContext));

	ASSERT(PtrIrpContext);

	// give back memory either to the zone or to the VMM
	if (!(PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_NOT_FROM_POOL)) {
		ExFreeToNPagedLookasideList(&(XifsIrpContextLookasideList),PtrIrpContext);
	} else {
		ExFreePool(PtrIrpContext);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), (DEBUG_TARGET_CREATE|DEBUG_TARGET_IRPCONTEXT), 
		("Exit XifsdReleaseIrpContext\n"));
	return;
}





LONG
XifsdExceptionFilter (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    )
{
    NTSTATUS ExceptionCode;
    BOOLEAN TestStatus = FALSE;
	long	ReturnCode = EXCEPTION_EXECUTE_HANDLER;

	DebugTrace(DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XifsdExceptionFilter\n"));


	ASSERT(ExceptionPointer);

	ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionCode;
	

	DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
				 ("XifsdExceptionFilter: %08x (exr %08x cxr %08x)\n",
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
				("XifsdExceptionFilter MAJOR(0x%x) MINOR(0x%x)\n", 
				IrpContext->MajorFunction, IrpContext->MinorFunction));


        if (IrpContext->SavedExceptionCode == STATUS_SUCCESS) {

			if(FsRtlIsNtstatusExpected( ExceptionCode )){

				IrpContext->SavedExceptionCode = ExceptionCode;
				TestStatus = TRUE;

				DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
						("XifsdExceptionFilter Expected MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

				return EXCEPTION_EXECUTE_HANDLER;
			}else{
				TestStatus = FALSE;

				DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
						("XifsdExceptionFilter Unexpected MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
						IrpContext->MajorFunction, IrpContext->MinorFunction, ExceptionCode));

				XifsdBugCheck( (ULONG_PTR) ExceptionPointer->ExceptionRecord,
						 (ULONG_PTR) ExceptionPointer->ContextRecord,
						 (ULONG_PTR) ExceptionPointer->ExceptionRecord->ExceptionAddress );
			}

  
        } else {

			DebugTrace( DEBUG_LEVEL_EXCEPTIONS, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
						("XifsdExceptionFilter Double Exception MAJOR(0x%x) MINOR(0x%x) ExceptCode(0x%x)\n", 
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
XifsdExceptionHandler(
	PXIFS_IRPCONTEXT				PtrIrpContext,
	PIRP								Irp)
{
	NTSTATUS						RC = STATUS_INSUFFICIENT_RESOURCES;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XifsdExceptionHandler\n"));

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
			XifsdReleaseIrpContext(PtrIrpContext);
		}

	}else{

		if(PtrIrpContext){
			PtrIrpContext->SavedExceptionCode = STATUS_SUCCESS;
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XifsdExceptionHandler\n"));
	return(RC);
}

NTSTATUS
DoTestRequest(
	PXIFS_IRPCONTEXT	pIrpContext
)
{
	TestVal ++;
	if(TestVal == 10){
		uint32	temp = 0;
		TestVal = 0;
		ExRaiseStatus(STATUS_IN_PAGE_ERROR);
		return STATUS_SUCCESS;
	}

	
	//XifsdRaiseStatus(pIrpContext, STATUS_CANT_WAIT);
	ExRaiseStatus(STATUS_CANT_WAIT);
	return STATUS_SUCCESS;
}

NTSTATUS
XixFsDispatch (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP Irp)
{

	NTSTATUS			RC = STATUS_SUCCESS;
   	PXIFS_IRPCONTEXT	PtrIrpContext = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Enter XifsDispatch\n"));

	FsRtlEnterFileSystem();
	ASSERT(DeviceObject);
	ASSERT(Irp);
	
	if (!IoGetTopLevelIrp())
	{
	    IoSetTopLevelIrp(Irp);
	}

	do{
		try
		{
			if(PtrIrpContext == NULL){
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT), ("Allocate IrpContext\n"));
				PtrIrpContext = XifsdAllocateIrpContext(Irp, DeviceObject);

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

				RC= DoTestRequest(PtrIrpContext);
			}
			
		   
		}except (XifsdExceptionFilter(PtrIrpContext, GetExceptionInformation()))
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Exception!!\n"));
			RC = XifsdExceptionHandler(PtrIrpContext, Irp);
		}

	}while( RC == STATUS_CANT_WAIT);
		
	FsRtlExitFileSystem();

 	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DISPATCH| DEBUG_TARGET_IRPCONTEXT),
		("Exit XifsDispatch\n"));   
    return RC;


	return STATUS_SUCCESS;
}




