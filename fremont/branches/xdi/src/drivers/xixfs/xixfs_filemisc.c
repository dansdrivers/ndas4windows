#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"

#define TYPE_OF_OPEN_MASK               (0x00000007)


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_AcquireResource)
#pragma alloc_text(PAGE, xixfs_SetFileObject)
#pragma alloc_text(PAGE, xixfs_DecodeFileObject)
#pragma alloc_text(PAGE, xixfs_GetCallersBuffer)
#pragma alloc_text(PAGE, xixfs_PinCallersBuffer)
#pragma alloc_text(PAGE, xixfs_SearchLastComponetOffsetOfName)
#pragma alloc_text(PAGE, xixfs_RemoveLastSlashOfName)
#pragma alloc_text(PAGE, xixfs_NotifyReportChangeToXixfs)
#endif

uint64
xixfs_GetLcnFromLot(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize );

	ASSERT(IS_4096_SECTOR(PhyAddress));

	return (uint64)(PhyAddress/CLUSTER_SIZE);
}


BOOLEAN
xixfs_AcquireResource(
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
		("Enter xixfs_AcquireResource \n"));


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
		("Exit xixfs_AcquireResource \n"));
	return Acquired;

}



VOID
xixfs_SetFileObject(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN PXIXFS_FCB pFCB OPTIONAL,
	IN PXIXFS_CCB pCCB OPTIONAL
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_SetFileObject\n"));

	ASSERT(!XIXCORE_TEST_FLAGS(((ULONG_PTR)pCCB), TYPE_OF_OPEN_MASK));

	if(TypeOfOpen == UnopenedFileObject){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("xixfs_SetFileObject UnopenedFileObject\n"));

		FileObject->FsContext = 
		FileObject->FsContext2 = NULL;
		return;
	}
	
	FileObject->FsContext = pFCB;
	FileObject->FsContext2 = pCCB;
	FileObject->CurrentByteOffset.QuadPart = 0;
	XIXCORE_SET_FLAGS(((ULONG_PTR)FileObject->FsContext2), TypeOfOpen);

	FileObject->Vpb = pFCB->PtrVCB->PtrVPB;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_SetFileObject\n"));

	return;
}


TYPE_OF_OPEN
xixfs_DecodeFileObject(
	IN PFILE_OBJECT pFileObject, 
	OUT PXIXFS_FCB *ppFCB, 
	OUT PXIXFS_CCB *ppCCB 
)
{
	TYPE_OF_OPEN TypeOfOpen;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_DecodeFileObject\n"));

	TypeOfOpen = (TYPE_OF_OPEN)XIXCORE_MASK_FLAGS((ULONG_PTR)pFileObject->FsContext2, TYPE_OF_OPEN_MASK);

	if(TypeOfOpen == UnopenedFileObject){
		*ppFCB = NULL;
		*ppCCB = NULL;
	}else{
		*ppFCB = pFileObject->FsContext;
		*ppCCB = pFileObject->FsContext2;
		XIXCORE_CLEAR_FLAGS((ULONG_PTR)*ppCCB, TYPE_OF_OPEN_MASK);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Eixt xixfs_DecodeFileObject\n"));
	return TypeOfOpen;

}


PVOID
xixfs_GetCallersBuffer(
	PIRP PtrIrp
)
{

	PVOID ReturnedBuffer = NULL;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Enter xixfs_GetCallersBuffer\n"));

	
	// If an MDL is supplied, use it.
	if (PtrIrp->MdlAddress) {
		ReturnedBuffer = MmGetSystemAddressForMdlSafe(PtrIrp->MdlAddress, NormalPagePriority);
	} else {
		ReturnedBuffer = PtrIrp->UserBuffer;
	}

	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Exit xixfs_GetCallersBuffer\n"));
	return(ReturnedBuffer);
}




NTSTATUS 
xixfs_PinCallersBuffer(
	PIRP				PtrIrp,
	BOOLEAN			IsReadOperation,
	uint32			Length
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PMDL				PtrMdl = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_READ|DEBUG_TARGET_WRITE),
		("Enter xixfs_PinCallersBuffer \n"));

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
		("Exit xixfs_PinCallersBuffer \n"));
	return(RC);
}


uint32
xixfs_SearchLastComponetOffsetOfName(
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
xixfs_RemoveLastSlashOfName(
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
xixfs_NotifyReportChangeToXixfs(
	
	IN PXIXFS_FCB	pFCB,
	IN uint32		FilterMatch,
	IN uint32		Action
)
{
	PXIXFS_VCB	pVCB = NULL;

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


