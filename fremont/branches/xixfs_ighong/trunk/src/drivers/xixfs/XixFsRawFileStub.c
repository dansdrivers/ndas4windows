#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDiskForm.h"
#include "XixFsDrv.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"
#include "XixFsGlobalData.h"



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsInitializeFileContext)
#pragma alloc_text(PAGE, XixFsSetFileContext)
#pragma alloc_text(PAGE, XixFsClearFileContext)
#pragma alloc_text(PAGE, XixFsReadFileInfoFromContext)
#pragma alloc_text(PAGE, XixFsReadFileInfoFromFcb)
#pragma alloc_text(PAGE, XixFsWriteFileInfoFromFcb)
#pragma alloc_text(PAGE, XixFsReLoadFileFromFcb)
#pragma alloc_text(PAGE, XixFsInitializeFCBInfo)
#endif


/*
 *	Used for directory suffing. << FILE/DIRECOTRY >>
 */

NTSTATUS
XixFsInitializeFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsInitializeFileContext .\n"));

	RtlZeroMemory(FileContext, sizeof(XIFS_FILE_EMUL_CONTEXT));
	FileContext->Buffer = ExAllocatePoolWithTag(NonPagedPool,XIDISK_FILE_HEADER_LOT_SIZE, TAG_BUFFER);
	if(!FileContext->Buffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail XifsdInitializeFileContext.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsInitializeFileContext .\n"));

	return STATUS_SUCCESS;
}


NTSTATUS
XixFsSetFileContext(
	PXIFS_VCB	pVCB,
	uint64	LotNumber,
	uint32	FileType,
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{
	NTSTATUS	RC = STATUS_SUCCESS;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsSetFileContext .\n"));


	FileContext->pVCB = pVCB;
	FileContext->LotNumber = LotNumber;
	FileContext->FileType = FileType;
	FileContext->pSearchedFCB = NULL;
	try{
		if(FileContext->Buffer == NULL){
			FileContext->Buffer = 
				ExAllocatePoolWithTag(NonPagedPool,XIDISK_FILE_HEADER_LOT_SIZE, TAG_BUFFER);
		}
	}finally{
		if(AbnormalTermination()){
			RC = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsSetFileContext .\n"));

	return RC;
}


NTSTATUS
XixFsClearFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{

	NTSTATUS	RC = STATUS_SUCCESS;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsClearFileContext .\n"));

	try{
		if(FileContext->Buffer){
			ExFreePool(FileContext->Buffer);
			FileContext->Buffer = NULL;
		}
	}finally{
		if(AbnormalTermination()){
			RC = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsClearFileContext .\n"));
	return RC;
}



NTSTATUS
XixFsReadFileInfoFromContext(
	BOOLEAN					Waitable,
	PXIFS_FILE_EMUL_CONTEXT FileContext
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_FILE_HEADER_LOT pFileHeader = NULL;
	PXIFS_VCB	pVCB = NULL;
	LARGE_INTEGER	Offset;
	uint32	BlockSize;
	uint32	LotType = LOT_INFO_TYPE_INVALID;
	uint32	Reason = 0;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsReadFileInfoFromContext.\n"));

	ASSERT(Waitable == TRUE);
	ASSERT(FileContext);
	pVCB = FileContext->pVCB;
	ASSERT_VCB(pVCB);


	DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					 ("FileContext Searched LotNumber(%I64d) FileType(%ld)\n", 
					 FileContext->LotNumber,
					 FileContext->FileType
					 ));
	
	FileContext->pSearchedFCB = NULL;	


		
	try{
		if(FileContext->Buffer == NULL){
			FileContext->Buffer = 
				ExAllocatePoolWithTag(NonPagedPool,XIDISK_FILE_HEADER_LOT_SIZE, TAG_BUFFER);
		}
	}finally{
		if(AbnormalTermination()){
			RC = STATUS_INSUFFICIENT_RESOURCES;
		}

	}

	if(!NT_SUCCESS(RC)){
		return RC;
	}


	try{
		RC = XixFsRawReadLotAndFileHeader(
				pVCB->TargetDeviceObject,
				pVCB->LotSize,
				FileContext->LotNumber,
				FileContext->Buffer,
				XIDISK_FILE_HEADER_LOT_SIZE,
				pVCB->SectorSize
				);
				
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Fail Read File Data(%I64d).\n", FileContext->LotNumber));
			try_return (RC);
		}	
		
		pFileHeader = (PXIDISK_FILE_HEADER_LOT)FileContext->Buffer;

		// Check Header
		if(FileContext->FileType ==  FCB_TYPE_DIR){
			LotType = LOT_INFO_TYPE_DIRECTORY;
		}else{
			LotType = LOT_INFO_TYPE_FILE;
		}

		RC = XixFsCheckLotInfo(
				&pFileHeader->LotHeader.LotInfo,
				pVCB->VolumeLotSignature,
				FileContext->LotNumber,
				LotType,
				LOT_FLAG_BEGIN,
				&Reason
				);

		if(!NT_SUCCESS(RC)){
			RC = STATUS_FILE_CORRUPT_ERROR;
			try_return(RC);
		}
		
	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsReadFileInfoFromContext Status (0x%x).\n", RC));

	return RC;
}


NTSTATUS
XixFsReadFileInfoFromFcb(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN uint8 *	Buffer,
	IN uint32	BufferSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint64			FileId = 0;
	PXIFS_VCB					pVCB = NULL;
	PXIDISK_FILE_HEADER_LOT		pFileHeader = NULL;
	uint32			LotType = LOT_INFO_TYPE_INVALID;
	uint32			Reason = 0;

	PAGED_CODE();

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(Waitable == TRUE);
	ASSERT(BufferSize >= XIDISK_FILE_HEADER_LOT_SIZE);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsReadFileInfoFromFcb\n"));

	try{
		RC = XixFsRawReadLotAndFileHeader(
				pVCB->TargetDeviceObject,
				pVCB->LotSize,
				pFCB->LotNumber,
				Buffer,
				XIDISK_FILE_HEADER_LOT_SIZE,
				pVCB->SectorSize
				);
				
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						("Fail Read File Data(%I64d).\n", pFCB->LotNumber));
			try_return (RC);
		}	
		
		pFileHeader = (PXIDISK_FILE_HEADER_LOT)Buffer;

		// Check Header
		if(pFCB->FCBType ==  FCB_TYPE_DIR){
			LotType = LOT_INFO_TYPE_DIRECTORY;
		}else{
			LotType = LOT_INFO_TYPE_FILE;
		}

		RC = XixFsCheckLotInfo(
				&pFileHeader->LotHeader.LotInfo,
				pVCB->VolumeLotSignature,
				pFCB->LotNumber,
				LotType,
				LOT_FLAG_BEGIN,
				&Reason
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("XixFsReadFileInfoFromFcb : XixFsCheckLotInfo (%I64d).\n", pFCB->LotNumber));
			RC = STATUS_FILE_CORRUPT_ERROR;
			try_return(RC);
		}
	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsReadFileInfoFromFcb Status(0x%x)\n", RC));

	return RC;
}



NTSTATUS
XixFsWriteFileInfoFromFcb(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN uint8 *	Buffer,
	IN uint32	BufferSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint64			FileId = 0;
	PXIFS_VCB					pVCB = NULL;

	PAGED_CODE();

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(Waitable == TRUE);
	ASSERT(BufferSize >= XIDISK_FILE_HEADER_LOT_SIZE);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsWriteFileInfoFromFcb\n"));

	try{
		RC = XixFsRawWriteLotAndFileHeader(
				pVCB->TargetDeviceObject,
				pVCB->LotSize,
				pFCB->LotNumber,
				Buffer,
				XIDISK_FILE_HEADER_LOT_SIZE,
				pVCB->SectorSize
				);
				
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail Write File Data(%I64d).\n", pFCB->LotNumber));
			try_return (RC);
		}	
		

	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsWriteFileInfoFromFcb Status(0x%x)\n", RC));

	return RC;
}



NTSTATUS
XixFsReLoadFileFromFcb(
	IN PXIFS_FCB pFCB
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	PXIFS_VCB					pVCB = NULL;
	PXIDISK_FILE_HEADER			pFileHeader = NULL;
	PXIDISK_DIR_HEADER			pDirHeader = NULL;
	LARGE_INTEGER	Offset;
	uint32			LotCount;

	uint32			BufferSize = 0;
	uint8			*Buffer = NULL;
	uint64			FileId = 0;
	uint32			LotType = LOT_INFO_TYPE_INVALID;
	uint8			*NameBuffer = NULL;
	uint8			*ChildBuffer = NULL;
	uint32			Reason;
	uint32			AddLotSize = 0;
	
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsReLoadFileFromFcb\n"));

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	
	FileId = pFCB->LotNumber;

	BufferSize = XIDISK_FILE_HEADER_LOT_SIZE; 

	Buffer = ExAllocatePoolWithTag(NonPagedPool, BufferSize, TAG_BUFFER );
	
	if(!Buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RC = XixFsReadFileInfoFromFcb(
						pFCB,
						TRUE,
						Buffer,
						BufferSize
						);
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		pFileHeader = (PXIDISK_FILE_HEADER)Buffer;

		if(pFCB->FCBType == FCB_TYPE_DIR){
			pDirHeader = (PXIDISK_DIR_HEADER)Buffer;
			pFCB->LastAccessTime = pDirHeader->DirInfo.Access_time;
			pFCB->LastWriteTime = pDirHeader->DirInfo.Modified_time;
			pFCB->CreationTime = pDirHeader->DirInfo.Create_time;
			
			if(pDirHeader->DirInfo.NameSize != 0){
				uint16 Size = (uint16)pDirHeader->DirInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->FCBName.Buffer){
					ExFreePool(pFCB->FCBName.Buffer);
					pFCB->FCBName.Buffer = NULL;

				}


				pFCB->FCBName.MaximumLength = SECTORALIGNSIZE_512(Size);
				pFCB->FCBName.Length = Size;
				NameBuffer =ExAllocatePoolWithTag(PagedPool, SECTORALIGNSIZE_512(Size),TAG_FILE_NAME);
				pFCB->FCBName.Buffer = (PWSTR)NameBuffer;
				if(!pFCB->FCBName.Buffer){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->FCBName.Buffer, 
					pDirHeader->DirInfo.Name, 
					pFCB->FCBName.Length);		
			}
			pFCB->RealFileSize = pDirHeader->DirInfo.FileSize;
			pFCB->RealAllocationSize = pDirHeader->DirInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart = pDirHeader->DirInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pDirHeader->DirInfo.AllocationSize; 
			
			pFCB->FileAttribute = pDirHeader->DirInfo.FileAttribute;
			pFCB->LinkCount = pDirHeader->DirInfo.LinkCount;
			pFCB->Type = pDirHeader->DirInfo.Type;
			if(pVCB->RootDirectoryLotIndex == FileId) {
				XifsdSetFlag(pFCB->Type, (XIFS_FD_TYPE_ROOT_DIRECTORY|XIFS_FD_TYPE_DIRECTORY));
			}
			pFCB->LotNumber = pDirHeader->DirInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_DIR_INDICATOR|(FCB_ADDRESS_MASK & pFCB->LotNumber));
			pFCB->ParentLotNumber = pDirHeader->DirInfo.ParentDirLotIndex;
			pFCB->AddrLotNumber = pDirHeader->DirInfo.AddressMapIndex;
			
			pFCB->ChildCount= pDirHeader->DirInfo.childCount;		
			pFCB->ChildCount = (uint32)XixFsfindSetBitMapCount(1024, pDirHeader->DirInfo.ChildMap);
			pFCB->AddrStartSecIndex = 0;

		}else{
			pFCB->LastAccessTime = pFileHeader->FileInfo.Access_time;
			
			pFCB->LastWriteTime = pFileHeader->FileInfo.Modified_time;
			pFCB->CreationTime = pFileHeader->FileInfo.Create_time;
			if(pFileHeader->FileInfo.NameSize != 0){
				uint16 Size = (uint16)pFileHeader->FileInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->FCBName.Buffer){
					ExFreePool(pFCB->FCBName.Buffer);
					pFCB->FCBName.Buffer = NULL;

				}

				pFCB->FCBName.MaximumLength = SECTORALIGNSIZE_512(Size);
				pFCB->FCBName.Length = Size;
				NameBuffer = ExAllocatePool(NonPagedPool, SECTORALIGNSIZE_512(Size));
				pFCB->FCBName.Buffer = (PWSTR)NameBuffer;
				if(!pFCB->FCBName.Buffer){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->FCBName.Buffer, 
					pFileHeader->FileInfo.Name, 
					pFCB->FCBName.Length);
			}
			pFCB->RealFileSize = pFileHeader->FileInfo.FileSize;
			pFCB->RealAllocationSize = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart =  pFileHeader->FileInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->FileAttribute = pFileHeader->FileInfo.FileAttribute;
			pFCB->LinkCount = pFileHeader->FileInfo.LinkCount;
			pFCB->Type = pFileHeader->FileInfo.Type;
			
			pFCB->LotNumber = pFileHeader->FileInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_FILE_INDICATOR|(FCB_ADDRESS_MASK & pFCB->LotNumber));
			pFCB->ParentLotNumber = pFileHeader->FileInfo.ParentDirLotIndex;
			pFCB->AddrLotNumber = pFileHeader->FileInfo.AddressMapIndex;
			
			pFCB->AddrStartSecIndex = 0;
		}

		if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CHANGE_RELOAD)){
			XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_RELOAD);
		}else if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CHANGE_RENAME)){
			XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_RENAME);	
		}else if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_CHANGE_LINK)){
			XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_LINK);
		}


		if(pFCB->LastAccessTime == 0){
			pFCB->LastAccessTime = XixGetSystemTime().QuadPart;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->LastAccessTime(%I64d).\n", pFCB->LastAccessTime));
		}
		if(pFCB->LastWriteTime == 0)
		{
			pFCB->LastWriteTime = XixGetSystemTime().QuadPart;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Initialize pFCB->LastWriteTime(%I64d).\n",pFCB->LastWriteTime));
		}
		
		if(pFCB->CreationTime == 0)
		{
			pFCB->CreationTime = XixGetSystemTime().QuadPart;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->CreationTime(%I64d).\n", pFCB->CreationTime));
		}


		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						("Add to FCB File LotNumber(%I64d).\n", FileId));

	}finally{
		if(Buffer) {
			ExFreePool(Buffer);
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsReLoadFileFromFcb\n"));
	return RC;
}






NTSTATUS
XixFsInitializeFCBInfo(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	PXIFS_VCB					pVCB = NULL;
	PXIDISK_FILE_HEADER			pFileHeader = NULL;
	PXIDISK_DIR_HEADER			pDirHeader = NULL;
	LARGE_INTEGER	Offset;
	uint32			LotCount;

	uint32			BufferSize = 0;
	uint8			*Buffer = NULL;
	uint64			FileId = 0;
	uint32			LotType = LOT_INFO_TYPE_INVALID;
	uint8			*NameBuffer = NULL;
	uint8			*ChildBuffer = NULL;
	uint32			Reason;
	uint32			AddLotSize = 0;
	
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsInitializeFCBInfo\n"));

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(Waitable == TRUE);

	
	FileId = pFCB->LotNumber;

	BufferSize = XIDISK_FILE_HEADER_LOT_SIZE; 

	Buffer = ExAllocatePoolWithTag(NonPagedPool, BufferSize, TAG_BUFFER );
	
	if(!Buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RC = XixFsReadFileInfoFromFcb(
						pFCB,
						Waitable,
						Buffer,
						BufferSize
						);
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		pFileHeader = (PXIDISK_FILE_HEADER)Buffer;

		if(pFCB->FCBType == FCB_TYPE_DIR){
			pDirHeader = (PXIDISK_DIR_HEADER)Buffer;
			pFCB->LastAccessTime = pDirHeader->DirInfo.Access_time;
			pFCB->LastWriteTime = pDirHeader->DirInfo.Modified_time;
			pFCB->CreationTime = pDirHeader->DirInfo.Create_time;
			
			if(pDirHeader->DirInfo.NameSize != 0){
				uint16 Size = (uint16)pDirHeader->DirInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->FCBName.Buffer){
					ExFreePool(pFCB->FCBName.Buffer);
					pFCB->FCBName.Buffer = NULL;

				}


				pFCB->FCBName.MaximumLength = SECTORALIGNSIZE_512(Size);
				pFCB->FCBName.Length = Size;
				NameBuffer =ExAllocatePoolWithTag(PagedPool, SECTORALIGNSIZE_512(Size),TAG_FILE_NAME);
				pFCB->FCBName.Buffer = (PWSTR)NameBuffer;
				if(!pFCB->FCBName.Buffer){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->FCBName.Buffer, 
					pDirHeader->DirInfo.Name, 
					pFCB->FCBName.Length);		
			}
			pFCB->RealFileSize = pDirHeader->DirInfo.FileSize;
			pFCB->RealAllocationSize = pDirHeader->DirInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart = pDirHeader->DirInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pDirHeader->DirInfo.AllocationSize; 
			
			pFCB->FileAttribute = pDirHeader->DirInfo.FileAttribute;
			
			pFCB->LinkCount = pDirHeader->DirInfo.LinkCount;
			pFCB->Type = pDirHeader->DirInfo.Type;
			if(pVCB->RootDirectoryLotIndex == FileId) {
				XifsdSetFlag(pFCB->Type, (XIFS_FD_TYPE_ROOT_DIRECTORY|XIFS_FD_TYPE_DIRECTORY));
			}
			pFCB->LotNumber = pDirHeader->DirInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_DIR_INDICATOR|(FCB_ADDRESS_MASK & pFCB->LotNumber));
			pFCB->ParentLotNumber = pDirHeader->DirInfo.ParentDirLotIndex;
			pFCB->AddrLotNumber = pDirHeader->DirInfo.AddressMapIndex;
			
			pFCB->ChildCount= pDirHeader->DirInfo.childCount;		
			pFCB->ChildCount = (uint32)XixFsfindSetBitMapCount(1024, pDirHeader->DirInfo.ChildMap);
			pFCB->AddrStartSecIndex = 0;

		}else{
			pFCB->LastAccessTime = pFileHeader->FileInfo.Access_time;
			
			pFCB->LastWriteTime = pFileHeader->FileInfo.Modified_time;
			pFCB->CreationTime = pFileHeader->FileInfo.Create_time;
			if(pFileHeader->FileInfo.NameSize != 0){
				uint16 Size = (uint16)pFileHeader->FileInfo.NameSize;
				if(Size > XIFS_MAX_NAME_LEN){
					Size = XIFS_MAX_NAME_LEN;
				}

				if(pFCB->FCBName.Buffer){
					ExFreePool(pFCB->FCBName.Buffer);
					pFCB->FCBName.Buffer = NULL;

				}

				pFCB->FCBName.MaximumLength = SECTORALIGNSIZE_512(Size);
				pFCB->FCBName.Length = Size;
				NameBuffer = ExAllocatePool(NonPagedPool, SECTORALIGNSIZE_512(Size));
				pFCB->FCBName.Buffer = (PWSTR)NameBuffer;
				if(!pFCB->FCBName.Buffer){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
				}
				RtlCopyMemory(
					pFCB->FCBName.Buffer, 
					pFileHeader->FileInfo.Name, 
					pFCB->FCBName.Length);
			}
			pFCB->RealFileSize = pFileHeader->FileInfo.FileSize;
			pFCB->RealAllocationSize = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->ValidDataLength.QuadPart =
			pFCB->FileSize.QuadPart =  pFileHeader->FileInfo.FileSize;
			pFCB->AllocationSize.QuadPart = pFileHeader->FileInfo.AllocationSize;
			
			pFCB->FileAttribute = pFileHeader->FileInfo.FileAttribute;
			pFCB->LinkCount = pFileHeader->FileInfo.LinkCount;
			pFCB->Type = pFileHeader->FileInfo.Type;
			
			pFCB->LotNumber = pFileHeader->FileInfo.LotIndex;
			pFCB->FileId = (FCB_TYPE_FILE_INDICATOR|(FCB_ADDRESS_MASK & pFCB->LotNumber));
			pFCB->ParentLotNumber = pFileHeader->FileInfo.ParentDirLotIndex;
			pFCB->AddrLotNumber = pFileHeader->FileInfo.AddressMapIndex;
			
			pFCB->AddrStartSecIndex = 0;

			AddLotSize = pVCB->SectorSize;

			

			pFCB->AddrLot = ExAllocatePoolWithTag(NonPagedPool, AddLotSize, TAG_BUFFER);
	
			if(!pFCB->AddrLot){
					RC = STATUS_INSUFFICIENT_RESOURCES;
					try_return(RC);
			}
			
			pFCB->AddrLotSize = AddLotSize;
			
			
			RC = XixFsRawReadAddressOfFile(
						pVCB->TargetDeviceObject,
						pVCB->LotSize,
						pFCB->LotNumber,
						pFCB->AddrLotNumber,
						(uint8 *)pFCB->AddrLot,
						pFCB->AddrLotSize,
						&(pFCB->AddrStartSecIndex),
						0,
						pVCB->SectorSize);
						


			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
		}


		if(pFCB->LastAccessTime == 0){
			pFCB->LastAccessTime = XixGetSystemTime().QuadPart;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->LastAccessTime(%I64d).\n", pFCB->LastAccessTime));
		}
		if(pFCB->LastWriteTime == 0)
		{
			pFCB->LastWriteTime = XixGetSystemTime().QuadPart;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Initialize pFCB->LastWriteTime(%I64d).\n",pFCB->LastWriteTime));
		}
		
		if(pFCB->CreationTime == 0)
		{
			pFCB->CreationTime = XixGetSystemTime().QuadPart;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Initialize pFCB->CreationTime(%I64d).\n", pFCB->CreationTime));
		}


		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
						("Add to FCB File LotNumber(%I64d).\n", FileId));

	}finally{
		if(Buffer) {
			ExFreePool(Buffer);
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsInitializeFCBInfo\n"));
	return RC;
}