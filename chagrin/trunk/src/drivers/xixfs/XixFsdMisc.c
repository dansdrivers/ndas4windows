#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"

#define TYPE_OF_OPEN_MASK               (0x00000007)


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdAcquireResource)
#pragma alloc_text(PAGE, XixFsdSetFileObject)
#pragma alloc_text(PAGE, XixFsdDecodeFileObject)
#pragma alloc_text(PAGE, XixFsdGetCallersBuffer)
#pragma alloc_text(PAGE, XixFsdPinUserBuffer)
#pragma alloc_text(PAGE, XixFsdSearchLastComponetOffset)
#endif


BOOLEAN
XixFsdAcquireResource(
	IN BOOLEAN Waitable,
	IN PERESOURCE Resource,
	IN BOOLEAN IgnoreWait,
	IN TYPE_OF_ACQUIRE Type
)
{
	BOOLEAN		Wait = FALSE;
	BOOLEAN		Acquired = FALSE;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_RESOURCE,
		("Enter XixFsdAcquireResource \n"));


	if(!IgnoreWait && Waitable){
		Wait = TRUE;
	}

	switch(Type) {
	case AcquireExclusive:
		Acquired = ExAcquireResourceExclusiveLite( Resource, Wait );
		break;
	case AcquireShared:
		Acquired = ExAcquireResourceSharedLite( Resource, Wait );
		break;
	case AcquireSharedStarveExclusive:
		Acquired = ExAcquireSharedStarveExclusive( Resource, Wait );
	default:
		ASSERT(FALSE);
		Acquired = FALSE;
	}

//	if(!Acquired && !IgnoreWait){
//		ExRaiseStatus(STATUS_CANT_WAIT);
//	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_RESOURCE,
		("Exit XixFsdAcquireResource \n"));
	return Acquired;

}



VOID
XixFsdSetFileObject(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN PXIFS_FCB pFCB OPTIONAL,
	IN PXIFS_CCB pCCB OPTIONAL
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdSetFileObject\n"));

	ASSERT(!XifsdCheckFlag(((ULONG_PTR)pCCB), TYPE_OF_OPEN_MASK));

	if(TypeOfOpen == UnopenedFileObject){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("XixFsdSetFileObject UnopenedFileObject\n"));

		FileObject->FsContext = 
		FileObject->FsContext2 = NULL;
		return;
	}
	
	FileObject->FsContext = pFCB;
	FileObject->FsContext2 = pCCB;
	FileObject->CurrentByteOffset.QuadPart = 0;
	XifsdSetFlag(((ULONG_PTR)FileObject->FsContext2), TypeOfOpen);

	FileObject->Vpb = pFCB->PtrVCB->PtrVPB;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdSetFileObject\n"));

	return;
}


TYPE_OF_OPEN
XixFsdDecodeFileObject(
	IN PFILE_OBJECT pFileObject, 
	OUT PXIFS_FCB *ppFCB, 
	OUT PXIFS_CCB *ppCCB 
)
{
	TYPE_OF_OPEN TypeOfOpen;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdDecodeFileObject\n"));

	TypeOfOpen = (TYPE_OF_OPEN)XifsdCheckFlag((ULONG_PTR)pFileObject->FsContext2, TYPE_OF_OPEN_MASK);

	if(TypeOfOpen == UnopenedFileObject){
		*ppFCB = NULL;
		*ppCCB = NULL;
	}else{
		*ppFCB = pFileObject->FsContext;
		*ppCCB = pFileObject->FsContext2;
		XifsdClearFlag((ULONG_PTR)*ppCCB, TYPE_OF_OPEN_MASK);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Eixt XixFsdDecodeFileObject\n"));
	return TypeOfOpen;

}


PVOID
XixFsdGetCallersBuffer(
	PIRP PtrIrp
)
{

	PVOID ReturnedBuffer = NULL;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Enter XixFsdGetCallersBuffer\n"));

	
	// If an MDL is supplied, use it.
	if (PtrIrp->MdlAddress) {
		ReturnedBuffer = MmGetSystemAddressForMdlSafe(PtrIrp->MdlAddress, NormalPagePriority);
	} else {
		ReturnedBuffer = PtrIrp->UserBuffer;
	}

	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Exit XixFsdGetCallersBuffer\n"));
	return(ReturnedBuffer);
}




NTSTATUS 
XixFsdPinUserBuffer(
	PIRP				PtrIrp,
	BOOLEAN			IsReadOperation,
	uint32			Length
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PMDL				PtrMdl = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Enter XixFsdPinUserBuffer \n"));

	ASSERT(PtrIrp);
	
	try {
		
		// Is a MDL already present in the IRP
		if (!(PtrIrp->MdlAddress)) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("PtrIrp->MdlAddress is NULL!!!\n"));
			// Allocate a MDL
			if (!(PtrMdl = IoAllocateMdl(PtrIrp->UserBuffer, Length, FALSE, FALSE, PtrIrp))) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail Allocate Mdl!!!\n"));
				
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}

			// Probe and lock the pages described by the MDL
			// We could encounter an exception doing so, swallow the exception
			// NOTE: The exception could be due to an unexpected (from our
			// perspective), invalidation of the virtual addresses that comprise
			// the passed in buffer
			try {
				MmProbeAndLockPages(PtrMdl, PtrIrp->RequestorMode, (IsReadOperation ? IoReadAccess : IoWriteAccess));
			} except(EXCEPTION_EXECUTE_HANDLER) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail Mdl Lock Page Fail!!!\n"));

				RC = STATUS_INVALID_USER_BUFFER;
			}


		}
		


	} finally {
		if (!NT_SUCCESS(RC) && PtrMdl) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail Free Mdl!!!\n"));

			IoFreeMdl(PtrMdl);
			PtrIrp->MdlAddress = NULL;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Exit XixFsdPinUserBuffer \n"));
	return(RC);
}


uint32
XixFsdSearchLastComponetOffset(
	IN PUNICODE_STRING pName
)
{
	int32 Length = 0;
	int32 Index = 0;


	ASSERT(pName->Length > 0);

	Length = pName->Length -1;
	
	// shitf null character
	Index = (int32)(Length/sizeof(WCHAR));

	ASSERT(Index > 0);

	while(pName->Buffer[Index] != L'\\')
	{
		Index --;
	}

	ASSERT(Index >= 0);
	Index++;
	Length = Index*sizeof(WCHAR);

	ASSERT(pName->Buffer[Length/sizeof(WCHAR) - 1] == L'\\');

	return Length;
}


VOID
XixFsdRemoveLastSlash(
	IN PUNICODE_STRING pName 
)
{
	int32 Length = 0;
	int32 Index = 0;

	ASSERT(pName->Length >= sizeof(WCHAR));

	
	if(pName->Length > sizeof(WCHAR)){
		Length = pName->Length-1;

		Index = (int32)(Length/sizeof(WCHAR));
		
		if(pName->Buffer[Index] == L'\\'){
			pName->Length = (uint16)(Index*sizeof(WCHAR));
		}
	}

	return;
}


VOID
XixFsdNotifyReportChange(
	
	IN PXIFS_FCB	pFCB,
	IN uint32		FilterMatch,
	IN uint32		Action
)
{
	PXIFS_VCB	pVCB = NULL;

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	if(pFCB->FCBFullPath.Buffer != NULL){
		FsRtlNotifyFullReportChange(
					pVCB->NotifyIRPSync,
					&pVCB->NextNotifyIRP,
					(PSTRING)&pFCB->FCBFullPath,
					(USHORT)pFCB->FCBTargetOffset,
					NULL,
					NULL,
					FilterMatch,
					Action,
					NULL
					);
	}
}


