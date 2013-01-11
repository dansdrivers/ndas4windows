#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"

#include "xixcore/callback.h"
#include "xixcore/fileaddr.h"
#include "xixcore/ondisk.h"
#include  "xcsystem/system.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_UpdateFCB)
#pragma alloc_text(PAGE, xixfs_LastUpdateFileFromFCB)
#pragma alloc_text(PAGE, xixfs_DeleteVCB)
#pragma alloc_text(PAGE, xixfs_DeleteFCB)
#pragma alloc_text(PAGE, xixfs_DeleteInternalStream)
#pragma alloc_text(PAGE, xixfs_TeardownStructures)
#pragma alloc_text(PAGE, xixfs_CloseFCB)
#pragma alloc_text(PAGE, xixfs_RealCloseFCB)
#pragma alloc_text(PAGE, xixfs_CreateAndAllocFCB)
#pragma alloc_text(PAGE, xixfs_CreateInternalStream)
#pragma alloc_text(PAGE, xixfs_InitializeFCB)
#pragma alloc_text(PAGE, xixfs_CreateCCB)
#pragma alloc_text(PAGE, xixfs_InitializeLcbFromDirContext)
#endif

/*
 *	Delete Structure
 */


NTSTATUS
xixfs_UpdateFCB( 
	IN	PXIXFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIXCORE_BUFFER		buffer = NULL;
	PXIXFS_VCB			pVCB = NULL;
	LARGE_INTEGER 		Offset;
	PXIDISK_LOT_INFO	pLotInfo = NULL;
	uint32				Reason = 0;
	uint32				NotifyFilter = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_UpdateFCB.\n"));	


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if((pFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) && (pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL xixfs_UpdateFCB STATUS_INVALID_PARAMETER.\n"));	
		
		return STATUS_INVALID_PARAMETER;
	}
	

	if(!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_FILE) ){
		return STATUS_SUCCESS;
	}

	
	
	buffer = xixcore_AllocateBuffer(XIDISK_DIR_HEADER_LOT_SIZE);
	
	if(!buffer) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL xixfs_UpdateFCB STATUS_INSUFFICIENT_RESOURCES.\n"));	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xixcore_GetDataBuffer(buffer),XIDISK_DIR_HEADER_LOT_SIZE);

	try{
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR )
		{
			if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
				//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
				RC = xixcore_LotLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock), 1, 1);
				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL xixfs_UpdateFCB DIR GET LOCK.\n"));	
					try_return(RC);
				}
			}

		}else{
			if (pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB FILE HAS NO LOCK .\n"));	

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}

		RC = xixfs_ReadFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer
						);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL  xixfs_UpdateFCB : xixfs_ReadFileInfoFromFcb .\n"));	

			try_return(RC);
		}


		if(pFCB->XixcoreFcb.FCBType  == FCB_TYPE_DIR )
		{
			PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
			PXIDISK_DIR_INFO		DirInfo = NULL;
			uint8 	*Name = NULL;
			DirLotHeader = (PXIDISK_DIR_HEADER_LOT)xixcore_GetDataBuffer(buffer);
			DirInfo = &(DirLotHeader->DirInfo);
		

			if(DirInfo->State== XIFS_FD_STATE_DELETED){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : Deleted .\n"));	

				try_return(RC);
			}

			

			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
				RtlZeroMemory(DirInfo->Name, pFCB->XixcoreFcb.FCBNameLength);
				RtlCopyMemory(DirInfo->Name, pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
				DirInfo->NameSize = pFCB->XixcoreFcb.FCBNameLength;
				DirInfo->ParentDirLotIndex = pFCB->XixcoreFcb.ParentLotNumber;
				NotifyFilter |= FILE_NOTIFY_CHANGE_DIR_NAME;

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("xixfs_UpdateFCB : Set New Name  .\n"));

			}

			
			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
				DirInfo->FileAttribute = pFCB->XixcoreFcb.FileAttribute;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}


			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
				DirInfo->Create_time = pFCB->XixcoreFcb.Create_time;
				DirInfo->Change_time = pFCB->XixcoreFcb.Modified_time;
				DirInfo->Access_time = pFCB->XixcoreFcb.Access_time;
				DirInfo->Modified_time = pFCB->XixcoreFcb.Modified_time;

				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
			}


			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
				DirInfo->FileSize = pFCB->FileSize.QuadPart;
				DirInfo->AllocationSize = pFCB->XixcoreFcb.RealAllocationSize;
				NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
			}



			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT);
				DirInfo->LinkCount = pFCB->XixcoreFcb.LinkCount;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}


		}else{

			PXIDISK_FILE_HEADER_LOT	FileLotHeader = NULL;
			PXIDISK_FILE_INFO FileInfo = NULL;
			uint8 	*Name = NULL;
			FileLotHeader = (PXIDISK_FILE_HEADER_LOT)xixcore_GetDataBuffer(buffer);
			FileInfo = &(FileLotHeader->FileInfo);
			


			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
				uint32		Reason = 0;
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK)){
					RC = xixcore_RawWriteAddressOfFile(
									pVCB->XixcoreVcb.XixcoreBlockDevice,
									pVCB->XixcoreVcb.LotSize,
									pVCB->XixcoreVcb.SectorSize,
									pVCB->XixcoreVcb.SectorSizeBit,
									pFCB->XixcoreFcb.LotNumber,
									pFCB->XixcoreFcb.AddrLotNumber,
									pFCB->XixcoreFcb.AddrLotSize,
									&(pFCB->XixcoreFcb.AddrStartSecIndex), 
									pFCB->XixcoreFcb.AddrStartSecIndex,
									pFCB->XixcoreFcb.AddrLot,
									&Reason
									);
					if (RC< 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail(0x%x) Reason(0x%x):xixfs_UpdateFCB:xixcore_RawWriteAddressOfFile\n",
								RC, Reason));
						
						try_return(RC);
					}
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);
				}
			}



			if(FileInfo->State == XIFS_FD_STATE_DELETED){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : Deleted .\n"));	

				try_return(RC);
			}


			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
				RtlZeroMemory(FileInfo->Name, pFCB->XixcoreFcb.FCBNameLength);
				RtlCopyMemory(FileInfo->Name, pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
				FileInfo->NameSize = pFCB->XixcoreFcb.FCBNameLength;
				FileInfo->ParentDirLotIndex = pFCB->XixcoreFcb.ParentLotNumber;
				NotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("xixfs_UpdateFCB : Set New Name \n"));
			}

			
			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
				FileInfo->FileAttribute = pFCB->XixcoreFcb.FileAttribute;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}


			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
				FileInfo->Create_time = pFCB->XixcoreFcb.Create_time;
				FileInfo->Change_time = pFCB->XixcoreFcb.Modified_time;
				FileInfo->Access_time = pFCB->XixcoreFcb.Access_time;
				FileInfo->Modified_time = pFCB->XixcoreFcb.Modified_time;
				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
			}


			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
				FileInfo->FileSize = pFCB->FileSize.QuadPart;
				FileInfo->AllocationSize = pFCB->XixcoreFcb.RealAllocationSize;
				NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
			}

			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT);
				FileInfo->LinkCount = pFCB->XixcoreFcb.LinkCount;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}

		}

		RC = xixfs_WriteFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : xixfs_WriteFileInfoFromFcb .\n"));
			try_return(RC);
		}

		xixfs_NotifyReportChangeToXixfs(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);


	}finally{
		xixcore_FreeBuffer(buffer);

		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR)
		{
			RC = xixcore_LotUnLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock));
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_UpdateFCB Status(0x%x).\n", RC));	
	return RC;
}




NTSTATUS
xixfs_DeleteUpdateFCB(
	IN	PXIXFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIXCORE_BUFFER		buffer = NULL;
	PXIXFS_VCB			pVCB = NULL;
	LARGE_INTEGER 		Offset;
	PXIDISK_LOT_INFO	pLotInfo = NULL;
	uint32				Reason = 0;
	uint32				NotifyFilter = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_UpdateFCB.\n"));	


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if((pFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) && (pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL xixfs_UpdateFCB STATUS_INVALID_PARAMETER.\n"));	
		
		return STATUS_INVALID_PARAMETER;
	}
	
	buffer = xixcore_AllocateBuffer(XIDISK_DIR_HEADER_LOT_SIZE);
	
	if(!buffer) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL xixfs_UpdateFCB STATUS_INSUFFICIENT_RESOURCES.\n"));	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xixcore_GetDataBuffer(buffer),XIDISK_DIR_HEADER_LOT_SIZE);
	

	try{
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR )
		{
			if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){ 


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
					&& (pFCB->FCBCleanup == 0))
				{
					//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
					RC = xixcore_LotLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock), 1, 1);
					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("FAIL xixfs_UpdateFCB DIR GET LOCK.\n"));	
						try_return(RC);
					}
				}else{
					try_return(RC = STATUS_SUCCESS);
				}

	
			}
		}else{
			if (pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB FILE HAS NO LOCK .\n"));	

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}

		RC = xixfs_ReadFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer
						);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL  xixfs_UpdateFCB : xixfs_ReadFileInfoFromFcb .\n"));	

			try_return(RC);
		}


		if(pFCB->XixcoreFcb.FCBType  == FCB_TYPE_DIR )
		{
			PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
			PXIDISK_DIR_INFO		DirInfo = NULL;
			uint8 	*Name = NULL;
			DirLotHeader = (PXIDISK_DIR_HEADER_LOT)xixcore_GetDataBuffer(buffer);
			DirInfo = &(DirLotHeader->DirInfo);
		

			if(DirInfo->State== XIFS_FD_STATE_DELETED){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : Deleted .\n"));	

				try_return(RC);
			}

			
			
			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
				&& (DirLotHeader->DirInfo.LinkCount == 1))
			{
				RC = xixcore_DeleteFileLotAddress(&pVCB->XixcoreVcb, &pFCB->XixcoreFcb);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL xixfs_UpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(xixcore_GetDataBuffer(buffer),XIDISK_DIR_HEADER_LOT_SIZE);
				DirInfo->AddressMapIndex = 0;
				DirInfo->State = XIFS_FD_STATE_DELETED;
				DirInfo->Type =  XIFS_FD_TYPE_INVALID;

				DirLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				DirLotHeader->LotHeader.LotInfo.LotIndex = pFCB->XixcoreFcb.LotNumber;
				DirLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				DirLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				DirLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
				pFCB->XixcoreFcb.HasLock = FCB_FILE_LOCK_INVALID;
				//XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_OPEN_WRITE);	
				
			}else if (	XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (DirLotHeader->DirInfo.LinkCount > 1))
			{
				DirInfo->LinkCount--;
			}

			


		}else{

			PXIDISK_FILE_HEADER_LOT	FileLotHeader = NULL;
			PXIDISK_FILE_INFO FileInfo = NULL;
			uint8 	*Name = NULL;
			FileLotHeader = (PXIDISK_FILE_HEADER_LOT)xixcore_GetDataBuffer(buffer);
			FileInfo = &(FileLotHeader->FileInfo);
			

			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
				uint32		Reason = 0;
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK)){
					RC = xixcore_RawWriteAddressOfFile(
									pVCB->XixcoreVcb.XixcoreBlockDevice,
									pVCB->XixcoreVcb.LotSize,
									pVCB->XixcoreVcb.SectorSize,
									pVCB->XixcoreVcb.SectorSizeBit,
									pFCB->XixcoreFcb.LotNumber,
									pFCB->XixcoreFcb.AddrLotNumber,
									pFCB->XixcoreFcb.AddrLotSize,
									&(pFCB->XixcoreFcb.AddrStartSecIndex), 
									pFCB->XixcoreFcb.AddrStartSecIndex,
									pFCB->XixcoreFcb.AddrLot,
									&Reason
									);
					if (RC< 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail(0x%x) Reason(0x%x):xixfs_UpdateFCB:xixcore_RawWriteAddressOfFile\n",
								RC, Reason));
						
						try_return(RC);
					}
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);
				}
			}



			if(FileInfo->State== XIFS_FD_STATE_DELETED){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : Deleted .\n"));	

				try_return(RC);
			}



			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
				&& (FileLotHeader->FileInfo.LinkCount == 1))
			{
				
				RC = xixcore_DeleteFileLotAddress(&pVCB->XixcoreVcb, &pFCB->XixcoreFcb);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL xixfs_UpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(xixcore_GetDataBuffer(buffer),XIDISK_DIR_HEADER_LOT_SIZE);
				FileInfo->AddressMapIndex = 0;
				FileInfo->State = XIFS_FD_STATE_DELETED;
				FileInfo->Type =  XIFS_FD_TYPE_INVALID;

				FileLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				FileLotHeader->LotHeader.LotInfo.LotIndex = pFCB->XixcoreFcb.LotNumber;
				FileLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				FileLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				FileLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
				pFCB->XixcoreFcb.HasLock = FCB_FILE_LOCK_INVALID;
				//XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_OPEN_WRITE);	
				
			}else if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (FileLotHeader->FileInfo.LinkCount > 1))
			{
				FileInfo->LinkCount --;

			}else{
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
					RtlZeroMemory(FileInfo->Name, pFCB->XixcoreFcb.FCBNameLength);
					RtlCopyMemory(FileInfo->Name, pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
					FileInfo->NameSize = pFCB->XixcoreFcb.FCBNameLength;
					FileInfo->ParentDirLotIndex = pFCB->XixcoreFcb.ParentLotNumber;
					NotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("xixfs_UpdateFCB : Set New Name .\n"));
				}

				
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
					FileInfo->FileAttribute = pFCB->XixcoreFcb.FileAttribute;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
					FileInfo->Create_time = pFCB->XixcoreFcb.Create_time;
					FileInfo->Change_time = pFCB->XixcoreFcb.Modified_time;
					FileInfo->Access_time = pFCB->XixcoreFcb.Access_time;
					FileInfo->Modified_time = pFCB->XixcoreFcb.Modified_time;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
					NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				}


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE)){
					FileInfo->FileSize = pFCB->FileSize.QuadPart;
					FileInfo->AllocationSize = pFCB->XixcoreFcb.RealAllocationSize;
					NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
				}


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT);
					FileInfo->LinkCount = pFCB->XixcoreFcb.LinkCount;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}
			}
		}

		RC = xixfs_WriteFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : xixfs_WriteFileInfoFromFcb .\n"));
			try_return(RC);
		}

		xixfs_NotifyReportChangeToXixfs(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);


		XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);


	}finally{
		xixcore_FreeBuffer(buffer);

		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR)
		{
			RC = xixcore_LotUnLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock));
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_UpdateFCB Status(0x%x).\n", RC));	
	return RC;

}








NTSTATUS
xixfs_UpdateFCBLast( 
	IN	PXIXFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIXCORE_BUFFER		buffer = NULL;
	PXIXFS_VCB			pVCB = NULL;
	LARGE_INTEGER 		Offset;
	PXIDISK_LOT_INFO	pLotInfo = NULL;
	uint32				Reason = 0;
	uint32				NotifyFilter = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_UpdateFCB.\n"));	


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if((pFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) && (pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL xixfs_UpdateFCB STATUS_INVALID_PARAMETER.\n"));	
		
		return STATUS_INVALID_PARAMETER;
	}
	

	if(!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_FILE) ){
		return STATUS_SUCCESS;
	}
	

	buffer = xixcore_AllocateBuffer(XIDISK_DIR_HEADER_LOT_SIZE);
	
	if(!buffer) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL xixfs_UpdateFCB STATUS_INSUFFICIENT_RESOURCES.\n"));	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xixcore_GetDataBuffer(buffer),XIDISK_DIR_HEADER_LOT_SIZE);


	try{
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR )
		{
			if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){ 


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
					&& (pFCB->FCBCleanup == 0))
				{
					//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
					RC = xixcore_LotLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock), 1, 1);
					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("FAIL xixfs_UpdateFCB DIR GET LOCK.\n"));	
						try_return(RC);
					}
				}else{
					try_return(RC = STATUS_SUCCESS);
				}

	
			}
		}else{
			if (pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB FILE HAS NO LOCK .\n"));	

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}

		RC = xixfs_ReadFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer
						);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL  xixfs_UpdateFCB : xixfs_ReadFileInfoFromFcb .\n"));	

			try_return(RC);
		}


		if(pFCB->XixcoreFcb.FCBType  == FCB_TYPE_DIR )
		{
			PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
			PXIDISK_DIR_INFO		DirInfo = NULL;
			uint8 	*Name = NULL;
			DirLotHeader = (PXIDISK_DIR_HEADER_LOT)xixcore_GetDataBuffer(buffer);
			DirInfo = &(DirLotHeader->DirInfo);
		

			if(DirInfo->State== XIFS_FD_STATE_DELETED){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : Deleted .\n"));	

				try_return(RC);
			}

			
			
			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
				&& (pFCB->FCBCleanup == 0)
				&& (DirLotHeader->DirInfo.LinkCount == 1))
			{
				
				RC = xixcore_DeleteFileLotAddress(&pVCB->XixcoreVcb, &pFCB->XixcoreFcb);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL xixfs_UpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(xixcore_GetDataBuffer(buffer),XIDISK_DIR_HEADER_LOT_SIZE);
				DirInfo->AddressMapIndex = 0;
				DirInfo->State = XIFS_FD_STATE_DELETED;
				DirInfo->Type =  XIFS_FD_TYPE_INVALID;

				DirLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				DirLotHeader->LotHeader.LotInfo.LotIndex = pFCB->XixcoreFcb.LotNumber;
				DirLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				DirLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				DirLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
				pFCB->XixcoreFcb.HasLock = FCB_FILE_LOCK_INVALID;
				//XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_OPEN_WRITE);	
				
			}else if (	XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (DirLotHeader->DirInfo.LinkCount > 1))
			{
				DirInfo->LinkCount--;
			}

			


		}else{

			PXIDISK_FILE_HEADER_LOT	FileLotHeader = NULL;
			PXIDISK_FILE_INFO FileInfo = NULL;
			uint8 	*Name = NULL;
			FileLotHeader = (PXIDISK_FILE_HEADER_LOT)xixcore_GetDataBuffer(buffer);
			FileInfo = &(FileLotHeader->FileInfo);
			

			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
				uint32		Reason = 0;
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK)){
					RC = xixcore_RawWriteAddressOfFile(
									pVCB->XixcoreVcb.XixcoreBlockDevice,
									pVCB->XixcoreVcb.LotSize,
									pVCB->XixcoreVcb.SectorSize,
									pVCB->XixcoreVcb.SectorSizeBit,
									pFCB->XixcoreFcb.LotNumber,
									pFCB->XixcoreFcb.AddrLotNumber,
									pFCB->XixcoreFcb.AddrLotSize,
									&(pFCB->XixcoreFcb.AddrStartSecIndex), 
									pFCB->XixcoreFcb.AddrStartSecIndex,
									pFCB->XixcoreFcb.AddrLot,
									&Reason
									);
					if (RC< 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail(0x%x) Reason(0x%x):xixfs_UpdateFCB:xixcore_RawWriteAddressOfFile\n",
								RC, Reason));
						
						try_return(RC);
					}
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);
				}
			}


			if(FileInfo->State== XIFS_FD_STATE_DELETED){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : Deleted .\n"));	

				try_return(RC);
			}



			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
				&& (pFCB->FCBCleanup == 0)
				&& (FileLotHeader->FileInfo.LinkCount == 1))
			{
				
				RC = xixcore_DeleteFileLotAddress(&pVCB->XixcoreVcb, &pFCB->XixcoreFcb);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL xixfs_UpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(FileLotHeader,XIDISK_DIR_HEADER_LOT_SIZE);
				FileInfo->AddressMapIndex = 0;
				FileInfo->State = XIFS_FD_STATE_DELETED;
				FileInfo->Type =  XIFS_FD_TYPE_INVALID;

				FileLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				FileLotHeader->LotHeader.LotInfo.LotIndex = pFCB->XixcoreFcb.LotNumber;
				FileLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				FileLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				FileLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
				pFCB->XixcoreFcb.HasLock = FCB_FILE_LOCK_INVALID;
				//XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_OPEN_WRITE);	
				
			}else if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (FileLotHeader->FileInfo.LinkCount > 1))
			{
				FileInfo->LinkCount --;

			}else{
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
					RtlZeroMemory(FileInfo->Name, pFCB->XixcoreFcb.FCBNameLength);
					RtlCopyMemory(FileInfo->Name, pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
					FileInfo->NameSize = pFCB->XixcoreFcb.FCBNameLength;
					FileInfo->ParentDirLotIndex = pFCB->XixcoreFcb.ParentLotNumber;
					NotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("xixfs_UpdateFCB : Set New Name  .\n"));
				}

				
				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
					FileInfo->FileAttribute = pFCB->XixcoreFcb.FileAttribute;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
					FileInfo->Create_time = pFCB->XixcoreFcb.Create_time;
					FileInfo->Change_time = pFCB->XixcoreFcb.Modified_time;
					FileInfo->Access_time = pFCB->XixcoreFcb.Access_time;
					FileInfo->Modified_time = pFCB->XixcoreFcb.Modified_time;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
					NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				}


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE)){
					FileInfo->FileSize = pFCB->FileSize.QuadPart;
					FileInfo->AllocationSize = pFCB->XixcoreFcb.RealAllocationSize;
					NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
				}


				if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT)){
					XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT);
					FileInfo->LinkCount = pFCB->XixcoreFcb.LinkCount;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}
			}




		}

		RC = xixfs_WriteFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL xixfs_UpdateFCB : xixfs_WriteFileInfoFromFcb .\n"));
			try_return(RC);
		}

		xixfs_NotifyReportChangeToXixfs(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);


	}finally{
		xixcore_FreeBuffer(buffer);

//		if(pFCB->Xixcore.FCBType == FCB_TYPE_DIR)
//		{
//			RC = xixcore_LotUnLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock));
//		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_UpdateFCB Status(0x%x).\n", RC));	
	return RC;
}





NTSTATUS
xixfs_LastUpdateFileFromFCB( 
	IN	PXIXFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIXFS_VCB			pVCB = NULL;

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);





	if(	(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_FILE))
		|| (pFCB->XixcoreFcb.HasLock == FCB_FILE_LOCK_HAS) ){
		
		try{
			RC = xixfs_UpdateFCBLast(pFCB);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
			
			if(pFCB->XixcoreFcb.WriteStartOffset != -1){
				xixfs_SendFileChangeRC(
						TRUE,
						pVCB->XixcoreVcb.HostMac, 
						pFCB->XixcoreFcb.LotNumber, 
						pVCB->XixcoreVcb.VolumeId, 
						pFCB->FileSize.QuadPart, 
						pFCB->XixcoreFcb.RealAllocationSize,
						pFCB->XixcoreFcb.WriteStartOffset
				);
				
				pFCB->XixcoreFcb.WriteStartOffset = -1;
			}
			
			/*
			RC = xixcore_LotUnLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &(pFCB->XixcoreFcb.HasLock));
			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
			*/
			

			

		}finally{

		}


		
	}


	return RC;
}




VOID
xixfs_DeleteVCB(
    IN PXIXFS_IRPCONTEXT pIrpContext,
    IN PXIXFS_VCB pVCB
)
{
	LARGE_INTEGER	TimeOut;

    //PAGED_CODE();

    ASSERT_EXCLUSIVE_XIFS_GDATA;
    ASSERT_EXCLUSIVE_VCB( pVCB );

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
				("Enter xixfs_DeleteVCB (%p).\n", pVCB));



    //
    //  Now delete the volume device object.
    //

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
				("VDODeviceObject(%p) DriverObject(%p) NextDevice(%p) AttachedDevice(%p)\n", 
				&pVCB->PtrVDO->DeviceObject,
				pVCB->PtrVDO->DeviceObject.DriverObject,
				pVCB->PtrVDO->DeviceObject.NextDevice,
				pVCB->PtrVDO->DeviceObject.AttachedDevice));	


	//	Added by ILGU HONG 2006 06 12
	if(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap){
		xixcore_FreeBuffer(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap->Data);
		xixcore_FreeMem(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap,XCTAG_VOLFREEMAP);
		pVCB->XixcoreVcb.MetaContext.VolumeFreeMap = NULL;
	}

	if(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap) {
		xixcore_FreeBuffer(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap->Data);
		xixcore_FreeMem(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap, XCTAG_HOSTFREEMAP);
		pVCB->XixcoreVcb.MetaContext.HostFreeLotMap = NULL;
	}

	if(pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap) {
		xixcore_FreeBuffer(pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap->Data);
		xixcore_FreeMem(pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap, XCTAG_HOSTDIRTYMAP);
		pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap = NULL;
	}

	xixcore_CleanUpAuxLotLockInfo(&(pVCB->XixcoreVcb));
	if(pVCB->XixcoreVcb.VolumeName) {
		ExFreePoolWithTag(pVCB->XixcoreVcb.VolumeName, XCTAG_VOLNAME);
		pVCB->XixcoreVcb.VolumeName = NULL;
	}


    //
    //  Dereference our target if we haven't already done so.
    //

    if (pVCB->TargetDeviceObject != NULL) {

        ObDereferenceObject( pVCB->TargetDeviceObject );
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_REFCOUNT), 
						 ("xixfs_DeleteVCB TargetDeviceObject(%p)->ReferenceCount (%d) \n", 
						 pVCB->TargetDeviceObject,
						 pVCB->TargetDeviceObject->ReferenceCount));

		pVCB->TargetDeviceObject = NULL;
    }
    

	if(pVCB->SwapVpb){
		ExFreePool(pVCB->SwapVpb);
	}

	if(pVCB->PtrVPB){
		ExFreePool(pVCB->PtrVPB);
	}

    //
    //  Remove this entry from the global queue.
    //

    RemoveEntryList( &pVCB->VCBLink );

    //
    //  Delete resources.
    //

    ExDeleteResourceLite( &pVCB->VCBResource );
    ExDeleteResourceLite( &pVCB->FileResource );
    

    //
    //  Uninitialize the notify structures.
    //

    if (pVCB->NotifyIRPSync != NULL) {

        FsRtlNotifyUninitializeSync( &pVCB->NotifyIRPSync);
    }


	IoDeleteDevice(&(pVCB->PtrVDO->DeviceObject));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
				("Exit xixfs_DeleteVCB .\n"));

    return;
}



VOID
xixfs_DeleteFCB(
	IN BOOLEAN CanWait,
	IN PXIXFS_FCB		pFCB
)
{
	PXIXFS_VCB pVCB = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_DeleteFCB\n"));
	//
	//  Check inputs.
	//

	ASSERT_FCB( pFCB );

	//
	//  Sanity check the counts and Lcb lists.
	//
	
	ASSERT( (pFCB->FCBCleanup - pFCB->FcbNonCachedOpenCount) == 0 );
	ASSERT( pFCB->FCBReference == 0 );

	ASSERT( IsListEmpty( &pFCB->ChildLcbQueue ));
	ASSERT( IsListEmpty( &pFCB->ParentLcbQueue ));

	//
	//  Release any Filter Context structures associated with this FCB
	//

	//FsRtlTeardownPerStreamContexts( &pFCB->Header );


	//
	//  Now do the type specific structures.
	//

	switch (pFCB->XixcoreFcb.FCBType) {

	case FCB_TYPE_DIR:

		if (pFCB == pFCB->PtrVCB->RootDirFCB) {

			pVCB = pFCB->PtrVCB;
			pVCB->RootDirFCB = NULL;
    
		} 

		break;

	case FCB_TYPE_FILE :

		if (pFCB->FCBFileLock != NULL) {

			FsRtlFreeFileLock( pFCB->FCBFileLock );
		}

		FsRtlUninitializeOplock( &pFCB->FCBOplock );

		/*
		try{
			if(pFCB->XixcoreFcb.AddrLot)
				ExFreePool(pFCB->XixcoreFcb.AddrLot);
		}finally{
			if(AbnormalTermination()){
				ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
			}
		}
		*/


		break;
	

	case FCB_TYPE_VOLUME:

		if (pFCB == pFCB->PtrVCB->MetaFCB) {

			pVCB = pFCB->PtrVCB;
			pVCB->MetaFCB = NULL;

		}else if( pFCB == pFCB->PtrVCB->VolumeDasdFCB){
			
			pVCB = pFCB->PtrVCB;
			pVCB->MetaFCB = NULL;
		}
		break;
	default:
		break;
	}
	//
	//  Decrement the Vcb reference count if this is a system
	//  Fcb.
	//

	ExDeleteResourceLite(&(pFCB->FCBResource));
	ExDeleteResourceLite(&(pFCB->MainResource));
	ExDeleteResourceLite(&(pFCB->RealPagingIoResource));
	
	
	xixcore_DestroyFCB(&pFCB->XixcoreFcb);

	if(pFCB->FCBFullPath.Buffer){
		ExFreePool(pFCB->FCBFullPath.Buffer);
		pFCB->FCBFullPath.Buffer = NULL;
	}

	xixfs_FreeFCB(pFCB);

	
	if (pVCB != NULL) {

		InterlockedDecrement( &pVCB->VCBReference );
		InterlockedDecrement( &pVCB->VCBUserReference );
	}
	
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_DeleteFCB\n"));
	return;	
}



VOID
xixfs_DeleteInternalStream( 
	IN BOOLEAN Waitable, 
	IN PXIXFS_FCB		pFCB 
)
{
	PFILE_OBJECT FileObject;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter  XifsdDeleteInternalStream\n"));

	ASSERT_FCB( pFCB );

	//
	//  Lock the Fcb.
	//

	XifsdLockFcb( Waitable, pFCB );

	//
	//  Capture the file object.
	//

	FileObject = pFCB->FileObject;
	pFCB->FileObject = NULL;

	//
	//  It is now safe to unlock the Fcb.
	//

	XifsdUnlockFcb( Waitable, pFCB );

	//
	//  Dereference the file object if present.
	//

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						 ("AA Current VCB->PtrVPB->ReferenceCount %d \n", pFCB->PtrVCB->PtrVPB->ReferenceCount));

	if (FileObject != NULL) {

		if (FileObject->PrivateCacheMap != NULL) {

			CcUninitializeCacheMap( FileObject, NULL, NULL );
		}

		ObDereferenceObject( FileObject );
	}

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
						 ("BB Current VCB->PtrVPB->ReferenceCount %d \n", pFCB->PtrVCB->PtrVPB->ReferenceCount));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit  XifsdDeleteInternalStream\n"));
	return;
}





VOID
xixfs_TeardownStructures(
	IN BOOLEAN Waitable,
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN Recursive,
	OUT PBOOLEAN RemovedFCB
)
{
	PXIXFS_VCB Vcb = pFCB->PtrVCB;
	PXIXFS_FCB CurrentFcb = pFCB;
	BOOLEAN AcquiredCurrentFcb = FALSE;
	PXIXFS_FCB ParentFcb = NULL;
	PXIXFS_LCB Lcb;

	PLIST_ENTRY ListLinks;
	BOOLEAN Abort = FALSE;
	BOOLEAN Removed;
	BOOLEAN	CanWait = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter  xixfs_TeardownStructures\n"));
	//
	//  Check input.
	//

	ASSERT_FCB( pFCB );

	CanWait = Waitable;

	ASSERT(CanWait);

	*RemovedFCB = FALSE;

	try {

		//
		//  Loop until we find an Fcb we can't remove.
		//

		do {

			//
			//  See if there is an internal stream we should delete.
			//  Only do this if it is the last reference on the Fcb.
			//

			if (( (CurrentFcb->XixcoreFcb.FCBType  != FCB_TYPE_FILE) &&
				(CurrentFcb->XixcoreFcb.FCBType  != FCB_TYPE_DIR) )&&
				(CurrentFcb->FCBUserReference == 0) &&
				(CurrentFcb->FileObject != NULL)) {

				//
				//  Go ahead and delete the stream file object.
				//

				xixfs_DeleteInternalStream( CanWait, CurrentFcb );
			}


			if(CanWait){

				
				if(CurrentFcb->FCBCloseCount == 0 ){
					if(XIXCORE_TEST_FLAGS(CurrentFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE)){
					
					/*
					DbgPrint(" !!!Last Update (%wZ) FileSize (%I64d) \n", 
								&CurrentFcb->FCBFullPath, CurrentFcb->FileSize.QuadPart);
					
					*/
						// changed by ILGU HONG for readonly 09082006
						if(!CurrentFcb->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
							xixfs_LastUpdateFileFromFCB(CurrentFcb);
						}
						// changed by ILGU HONG for readonly end
						
					}
				}else if(CurrentFcb->FCBCleanup == 0){
					if(XIXCORE_TEST_FLAGS(CurrentFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE)){
						// changed by ILGU HONG for readonly 09082006
						if(!CurrentFcb->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
							xixfs_UpdateFCB(CurrentFcb);
						}
						// changed by ILGU HONG for readonly end
						
					}
				}
			}


			//
			//  If the reference count is non-zero then break.
			//

			if (CurrentFcb->FCBReference != 0) {

				break;
			}

			//
			//  It looks like we have a candidate for removal here.  We
			//  will need to walk the list of prefixes and delete them
			//  from their parents.  If it turns out that we have multiple
			//  parents of this Fcb, we are going to recursively teardown
			//  on each of these.
			//

			for ( ListLinks = CurrentFcb->ParentLcbQueue.Flink;
				  ListLinks != &CurrentFcb->ParentLcbQueue; ) {

				Lcb = CONTAINING_RECORD( ListLinks, XIXFS_LCB, ChildFcbLinks );

				ASSERT_LCB( Lcb );

				//
				//  We advance the pointer now because we will be toasting this guy,
				//  invalidating whatever is here.
				//

				ListLinks = ListLinks->Flink;

				//
				//  We may have multiple parents through hard links.  If the previous parent we
				//  dealt with is not the parent of this new Lcb, lets do some work.
				//

				if (ParentFcb != Lcb->ParentFcb) {

					if (ParentFcb) {

						ASSERT( !Recursive );

						xixfs_TeardownStructures( CanWait, ParentFcb, TRUE, &Removed );

						if (!Removed) {

							XifsdReleaseFcb( CanWait, ParentFcb );
						}
					}

					//
					//  Get this new parent Fcb to work on.
					//

					ParentFcb = Lcb->ParentFcb;
					XifsdAcquireFcbExclusive( CanWait, ParentFcb, FALSE );
				}
    
				//
				//  Lock the Vcb so we can look at references.
				//

				XifsdLockVcb( CanWait, Vcb );

				//
				//  Now check that the reference counts on the Lcb are zero.
				//

				if ( Lcb->Reference != 0 ) {

					//
					//  A create is interested in getting in here, so we should
					//  stop right now.
					//

					XifsdUnlockVcb( CanWait, Vcb );
					XifsdReleaseFcb( CanWait, ParentFcb );
					Abort = TRUE;

					break;
				}

				//
				//  Now remove this prefix and drop the references to the parent.
				//

				ASSERT( Lcb->ChildFcb == CurrentFcb );
				ASSERT( Lcb->ParentFcb == ParentFcb );

				DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
							 ("XifsdTeardownStructures, Lcb %08x CLotNum (%I64d) <->  Vcb %d/%d PFcb (%I64d) %d/%d CFcb %d/%d\n",
							 Lcb,
							 CurrentFcb->XixcoreFcb.LotNumber,
							 Vcb->VCBReference,
							 Vcb->VCBUserReference,
							 ParentFcb->XixcoreFcb.LotNumber,
							 ParentFcb->FCBReference,
							 ParentFcb->FCBUserReference,
							 CurrentFcb->FCBReference,
							 CurrentFcb->FCBUserReference ));

				xixfs_FCBTLBRemovePrefix( CanWait, Lcb );
				XifsdDecRefCount( ParentFcb, 1, 1 );

				/*
				DbgPrint("dec Link Count CCB with  VCB (%d/%d) ParentFCB:(%d/%d) ChildPFB\n",
						 Vcb->VCBReference,
						 Vcb->VCBUserReference,
						 ParentFcb->FCBReference,
						 ParentFcb->FCBUserReference
						 );	
				*/

				DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
							 ("XifsdTeardownStructures, After remove link Lcb %08x CLotNum (%I64d) <->  Vcb %d/%d PFcb (%I64d) %d/%d CFcb %d/%d\n",
							 Lcb,
							 CurrentFcb->XixcoreFcb.LotNumber,
							 Vcb->VCBReference,
							 Vcb->VCBUserReference,
							 ParentFcb->XixcoreFcb.LotNumber,
							 ParentFcb->FCBReference,
							 ParentFcb->FCBUserReference,
							 CurrentFcb->FCBReference,
							 CurrentFcb->FCBUserReference ));

				XifsdUnlockVcb( CanWait, Vcb );
			}

			//
			//  Now really leave if we have to.
			//



			if (Abort) {

				break;
			}

			//
			//  Now that we have removed all of the prefixes of this Fcb we can make the final check.
			//  Lock the Vcb again so we can inspect the child's references.
			//

			XifsdLockVcb( CanWait, Vcb );

			if (CurrentFcb->FCBReference != 0) {

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
							 ("XifsdTeardownStructures, saving Fcb %08x %d/%d\n",
							 CurrentFcb,
							 CurrentFcb->FCBReference,
							 CurrentFcb->FCBUserReference ));

				//
				//  Nope, nothing more to do.  Stop right now.
				//

				XifsdUnlockVcb( CanWait, Vcb );

				if (ParentFcb != NULL) {

					XifsdReleaseFcb( CanWait, ParentFcb );
				}

				break;
			}

			//
			//  This Fcb is toast.  Remove it from the Fcb Table as appropriate and delete.
			//

			if (XIXCORE_TEST_FLAGS( CurrentFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_IN_TABLE )) {

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
                 ("delete FCB link form FCB Table Delete FCB (%x) LotNumber(%i64d)\n", 
					CurrentFcb, CurrentFcb->XixcoreFcb.LotNumber));

				xixfs_FCBTLBDeleteEntry(CurrentFcb );
				XIXCORE_CLEAR_FLAGS( CurrentFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_IN_TABLE );

			}

			//
			//  Unlock the Vcb but hold the parent in order to walk up
			//  the tree.
			//



			XifsdUnlockVcb( IrpContext, Vcb );
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
                 ("delete FCB  (%x) LotNumber (%ld)\n", CurrentFcb, CurrentFcb->XixcoreFcb.LotNumber));



			/*
			 Update FCB Context

			 */
			if((CurrentFcb->XixcoreFcb.FCBType == FCB_TYPE_FILE) || (CurrentFcb->XixcoreFcb.FCBType == FCB_TYPE_DIR))
			{

				if(XIXCORE_TEST_FLAGS(CurrentFcb->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE)){
					/*
						DbgPrint(" !!!Last Update (%wZ) FileSize (%I64d) \n", 
								&CurrentFcb->FCBFullPath, CurrentFcb->FileSize.QuadPart);
					*/	
					// changed by ILGU HONG for readonly 09082006
					if(!CurrentFcb->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
						xixfs_LastUpdateFileFromFCB(CurrentFcb);
					}
					// changed by ILGU HONG for readonly end
					
				}

				if(CurrentFcb->XixcoreFcb.HasLock == FCB_FILE_LOCK_HAS){
					xixcore_LotUnLock(&(CurrentFcb->PtrVCB->XixcoreVcb), CurrentFcb->XixcoreFcb.LotNumber, &(CurrentFcb->XixcoreFcb.HasLock));
				}
			}

			



			
			xixfs_DeleteFCB( CanWait, CurrentFcb );



			//
			//  Move to the parent Fcb.
			//

			CurrentFcb = ParentFcb;
			ParentFcb = NULL;
			AcquiredCurrentFcb = TRUE;

		} while (CurrentFcb != NULL);

	} finally {

		//
		//  Release the current Fcb if we have acquired it.
		//

		if (AcquiredCurrentFcb && (CurrentFcb != NULL)) {

			XifsdReleaseFcb( CanWait, CurrentFcb );
		}

	}

	*RemovedFCB = (CurrentFcb != pFCB);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit  xixfs_TeardownStructures\n"));
	return;


}





BOOLEAN
xixfs_CloseFCB(
	IN BOOLEAN		Waitable,
	IN PXIXFS_VCB	pVCB,
	IN PXIXFS_FCB	pFCB,
	IN uint32	UserReference
)
{
	BOOLEAN		CanWait = FALSE;
	BOOLEAN		RemovedFCB = FALSE;
	BOOLEAN		RT = FALSE;
	
	PAGED_CODE();


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Enter xixfs_CloseFCB FCB(%p)\n", pFCB));

	ASSERT_VCB(pVCB);
	ASSERT_FCB(pFCB);
	//ASSERT_EXCLUSIVE_VCB(pVCB);
      
	if(XifsdAcquireVcbShared(Waitable, pVCB, FALSE)){
		if(!XifsdAcquireFcbExclusive(Waitable, pFCB, FALSE)){
			XifsdReleaseVcb(Waitable, pVCB);
			return FALSE;
		}
	}else{
		return FALSE;
	}



	//XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("xixfs_CloseFCB Acquire exclusive FCBResource (%p)\n", &pFCB->FCBResource));
	


	XifsdLockVcb(TRUE, pVCB);
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
                 ("xixfs_CloseFCB, Fcb LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
				 pFCB->XixcoreFcb.LotNumber,
                 pVCB->VCBReference,
                 pVCB->VCBUserReference,
                 pFCB->FCBReference,
                 pFCB->FCBUserReference ));

	
	XifsdDecRefCount(pFCB, 1, UserReference);
    
	/*
	DbgPrint("dec Close with CCB Count CCB with  VCB (%d/%d) FCB (%d/%d)\n",
		 pVCB->VCBReference,
		 pVCB->VCBUserReference,
		 pFCB->FCBReference,
		 pFCB->FCBUserReference);	
	*/

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
                 ("xixfs_CloseFCB, Fcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
				 pFCB->XixcoreFcb.LotNumber,
                 pVCB->VCBReference,
                 pVCB->VCBUserReference,
                 pFCB->FCBReference,
                 pFCB->FCBUserReference ));
	
	XifsdUnlockVcb(TRUE, pVCB);
	
	xixfs_TeardownStructures(Waitable, pFCB, FALSE, &RemovedFCB);

	if(!RemovedFCB){
		DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
                 ("xixfs_CloseFCB is FALSE !\n"));

		/*
		if((pFCB->FCBCleanup == 0) 
			&& (pFCB->HasLock == FCB_FILE_LOCK_HAS)) 
		{
			XifsdLotUnLock(pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber);
			pFCB->HasLock = FCB_FILE_LOCK_INVALID;
		}
		*/
		XifsdReleaseFcb(TRUE, pFCB);
		RT = TRUE;
	}else{
		DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
                 ("xixfs_CloseFCB is TRUE !\n"));
		RT = TRUE;
	}
	

	XifsdReleaseVcb(Waitable, pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Exit xixfs_CloseFCB \n"));
	return RT;
}


PXIXFS_FCB
xixfs_RemoveCloseQueue(
	IN PXIXFS_VCB pVCB
)
{
	PLIST_ENTRY Entry = NULL;
	PXIFS_CLOSE_FCB_CTX	pCtx = NULL;
	PXIXFS_FCB pFCB = NULL;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Enter xixfs_RemoveCloseQueue \n"));

	ASSERT_VCB(pVCB);

	XifsdLockVcb(TRUE, pVCB);	

	if(pVCB->DelayedCloseCount > 0){	
		Entry = RemoveHeadList(&(pVCB->DelayedCloseList));
		
		if(Entry != &(pVCB->DelayedCloseList)){
			pCtx = CONTAINING_RECORD(Entry, XIFS_CLOSE_FCB_CTX, DelayedCloseLink);
			pVCB->DelayedCloseCount -= 1;
			XifsdUnlockVcb(TRUE, pVCB);
			pFCB = pCtx->TargetFCB;
			ASSERT_FCB(pFCB);

			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
				("Get Fcb (0x%p)  Id(%ld) xixfs_RemoveCloseQueue \n", pFCB, pFCB->XixcoreFcb.LotNumber));

			xixfs_FreeCloseFcbCtx(pCtx);
			pCtx = NULL;
			return pFCB;					
		}
	}
	XifsdUnlockVcb(TRUE, pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Exit xixfs_RemoveCloseQueue FCB(%p)\n", pFCB));
	return NULL;
}



VOID
xixfs_CallCloseFCB(
	IN PVOID Context
)
{
	PXIXFS_VCB pVCB = NULL;

	pVCB = (PXIXFS_VCB)Context;
	ASSERT_VCB(pVCB);
	
	PAGED_CODE();
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	xixfs_RealCloseFCB((PVOID)pVCB);
	XifsdReleaseVcb(TRUE, pVCB);
}



VOID
xixfs_RealCloseFCB(
	IN PVOID Context
)
{
	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_VCB	pVCB = (PXIXFS_VCB)Context;
	BOOLEAN		IsSetVCB = FALSE;
	uint32		lockedVcbValue = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE),
			("Enter xixfs_RealCloseFCB\n"));	

	ASSERT_VCB(pVCB);
	//ASSERT_EXCLUSIVE_VCB(pVCB);


	pFCB = xixfs_RemoveCloseQueue(pVCB);

	while(pFCB != NULL)
	{
		ASSERT_FCB(pFCB);
		pVCB = pFCB->PtrVCB;
		ASSERT_VCB(pVCB);
		xixfs_CloseFCB(TRUE, pVCB, pFCB, 0);
		pFCB = xixfs_RemoveCloseQueue(pVCB);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE),
			("Exit xixfs_RealCloseFCB\n"));	
}




VOID
xixfs_InsertCloseQueue(
	IN PXIXFS_FCB pFCB
)
{
	BOOLEAN CloseActive = FALSE;
	PXIXFS_VCB				pVCB = NULL;
	PXIFS_CLOSE_FCB_CTX		pCtx = NULL;


	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Enter xixfs_InsertCloseQueue FCB(%p)\n", pFCB));

	//DbgPrint("Enter xixfs_InsertCloseQueue FCB(%p)\n", pFCB);
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);
	//ASSERT_EXCLUSIVE_VCB(pVCB);


	pCtx = xixfs_AllocateCloseFcbCtx();
	
	if(pCtx == NULL)
	{
		XifsdLockVcb(TRUE, pVCB);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					 ("xixfs_CloseFCB, Fcb LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
					 pFCB->XixcoreFcb.LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));

		
		XifsdDecRefCount(pFCB, 1, 0);
		/*
		DbgPrint("dec Close  Count CCB with  VCB (%d/%d) FCB (%d/%d)\n",
			 pVCB->VCBReference,
			 pVCB->VCBUserReference,
			 pFCB->FCBReference,
			 pFCB->FCBUserReference);	
		*/
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					 ("xixfs_CloseFCB, Fcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
					 pFCB->XixcoreFcb.LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));	
		
		XifsdUnlockVcb(TRUE, pVCB);
	}else {

		pCtx->TargetFCB = pFCB;	

		XifsdLockVcb(TRUE, pVCB);

		InsertTailList(&(pVCB->DelayedCloseList), &pCtx->DelayedCloseLink);
		
		pVCB->DelayedCloseCount += 1;
		
		if(XIXCORE_TEST_FLAGS(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE) == FALSE){
			if(pVCB->DelayedCloseCount > 0){
				CloseActive = TRUE;
			}
		}

		
		XifsdUnlockVcb(TRUE, pVCB);

		if(CloseActive ){
			ExQueueWorkItem(&pVCB->WorkQueueItem, CriticalWorkQueue);
		}
		//DbgPrint("Exit xixfs_InsertCloseQueue FCB(%p) and CTX (%p)\n", pFCB, pCtx);
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
				("Exit xixfs_InsertCloseQueue FCB(%p) and CTX (%p)\n", pFCB, pCtx));
	}
	
	return;
}

/*
 *	Create
 */




PXIXFS_FCB
xixfs_CreateAndAllocFCB(
	IN PXIXFS_VCB		pVCB,
	IN uint64			FileId,
	IN uint32			FCB_TYPE_CODE,
	OUT PBOOLEAN		Exist OPTIONAL
)
{
	PXIXFS_FCB	pFCB = NULL;
	BOOLEAN		LocalExist = FALSE;
	PAGED_CODE();


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XifsdCreateAndAllocFCB FileId (%I64d)\n", FileId));

	if(!ARGUMENT_PRESENT(Exist)){
		Exist = &LocalExist;
	}

	pFCB = xixfs_FCBTLBLookupEntry(pVCB, FileId);
	


	if(pFCB == NULL){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("XifsdCreateAndAllocFCB Create New FCB\n"));


		

		try{
			
			pFCB = xixfs_AllocateFCB();

			ExInitializeResourceLite(&(pFCB->FCBResource));
			ExInitializeFastMutex(&(pFCB->FCBMutex));
			ExInitializeResourceLite(&(pFCB->MainResource));
			ExInitializeResourceLite(&(pFCB->RealPagingIoResource));
		

			InitializeListHead(&(pFCB->ParentLcbQueue));
			InitializeListHead(&(pFCB->ChildLcbQueue));
			//InitializeListHead(&(pFCB->DelayedCloseLink));
			InitializeListHead(&(pFCB->EndOfFileLink));
			InitializeListHead(&(pFCB->CCBListQueue));

			xixcore_InitializeFCB(&(pFCB->XixcoreFcb), 
								&(pVCB->XixcoreVcb),
								(PXIXCORE_SEMAPHORE)&pFCB->AddrResource
								);



			pFCB->XixcoreFcb.FCBType = FCB_TYPE_CODE; 
			pFCB->PtrVCB = pVCB;
			pFCB->XixcoreFcb.LotNumber = FileId;
			
			pFCB->PagingIoResource = &pFCB->RealPagingIoResource;
			pFCB->Resource = &pFCB->MainResource;




		}finally{
			DebugUnwind("XifsdCreateAndAllocFCB");
			if(AbnormalTermination()){
				xixfs_FreeFCB(pFCB);
			}

		}
		
		if(Exist){
			*Exist = FALSE;
		}

	}else{
		if(Exist){
			*Exist = TRUE;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XifsdCreateAndAllocFCB FileId (%I64d)\n", FileId));
	
	return pFCB;
}



VOID
xixfs_CreateInternalStream (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PXIXFS_VCB pVCB,
    IN PXIXFS_FCB pFCB,
	IN BOOLEAN	ReadOnly
)
{
	PFILE_OBJECT StreamFile = NULL;
	BOOLEAN DecRef = FALSE;
	BOOLEAN	UnlockFcb = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_CreateInternalStream\n"));

	if(pFCB->FileObject != NULL){
		return;
	}


	XifsdLockFcb(IrpContext, pFCB);
	try{



		StreamFile = IoCreateStreamFileObject(NULL, pVCB->PtrVPB->RealDevice);



		if(StreamFile == NULL){
			XifsdRaiseStatus(IrpContext, STATUS_INSUFFICIENT_RESOURCES);
		}

		if(ReadOnly){
			StreamFile->ReadAccess = TRUE;
			StreamFile->WriteAccess = FALSE;
			StreamFile->DeleteAccess = FALSE;
		}else{
			StreamFile->ReadAccess = TRUE;
			StreamFile->WriteAccess = TRUE;
			StreamFile->DeleteAccess = FALSE;
		}
		


		StreamFile->SectionObjectPointer = &pFCB->SectionObject;
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("XifsdSetFileObject %l", StreamFile));
		
		xixfs_SetFileObject(IrpContext, StreamFile, StreamFileOpen, pFCB, NULL);



		CcInitializeCacheMap(StreamFile, 
				(PCC_FILE_SIZES)&pFCB->AllocationSize, 
				TRUE, 
				&XiGlobalData.XifsCacheMgrCallBacks,
				pFCB);


		pFCB->FileObject = StreamFile;
		StreamFile = NULL;
		
		

		XifsdLockVcb(IrpContext, pVCB);

		XifsdIncRefCount(pFCB, 2, 0);
		DecRef = TRUE;
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
						 ("XifsdCreateInternalStream  VCB %d/%d FCB %d/%d\n",
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pFCB->FCBReference,
						 pFCB->FCBUserReference ));
		
		XifsdUnlockVcb(IrpContext, pVCB);


	}finally{

		DebugUnwind("xixfs_CreateInternalStream");
        
		if (StreamFile != NULL) {

            ObDereferenceObject( StreamFile );
            pFCB->FileObject = NULL;
        }
		
        if (DecRef) {

            XifsdLockVcb( IrpContext, pVCB );
            XifsdDecRefCount(pFCB, 1, 0 );
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
							 ("XifsdCreateInternalStream, VCB %d/%d FCB %d/%d\n",
							 pVCB->VCBReference,
							 pVCB->VCBUserReference,
							 pFCB->FCBReference,
							 pFCB->FCBUserReference ));
		 
            XifsdUnlockVcb( IrpContext, pVCB );
        }
		
		XifsdUnlockFcb( IrpContext, pFCB );
    }
		
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_CreateInternalStream\n"));
	return;
	

}


NTSTATUS
xixfs_InitializeFCB(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable
)
{
	NTSTATUS RC = STATUS_SUCCESS;

	ASSERT_FCB(pFCB);
	ASSERT(Waitable );
	try{
		RC = xixfs_InitializeFCBInfo(pFCB, Waitable);
	}finally{

	}


	if(NT_SUCCESS(RC)){
		xixfs_FCBTLBInsertEntry(pFCB);

		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_IN_TABLE);
		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_INIT);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("FCBFlags (0x%x)\n", pFCB->XixcoreFcb.FCBFlags));	
		
	}

	return RC;
}


PXIXFS_CCB
xixfs_CreateCCB(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN PXIXFS_FCB pFCB, 
	IN PXIXFS_LCB pLCB,
	IN uint32	CCBFlags
)
{
	PXIXFS_CCB NewCcb;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_CCB|DEBUG_TARGET_LCB),
		("Enter  xixfs_CreateCCB\n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_FCB( pFCB );
	ASSERT_OPTIONAL_LCB( pLCB );

	//
	//  Allocate and initialize the structure.
	//

	NewCcb = xixfs_AllocateCCB();

	XIXCORE_SET_FLAGS(NewCcb->CCBFlags, CCBFlags);
	//
	//  Set the initial value for the flags and Fcb/Lcb
	//

	NewCcb->PtrFCB = pFCB;
	NewCcb->PtrLCB = pLCB;

	//
	//  Initialize the directory enumeration context
	//

	NewCcb->currentFileIndex = 0;
	NewCcb->highestFileIndex = 0;

	NewCcb->SearchExpression.Length = 
	NewCcb->SearchExpression.MaximumLength = 0;
	NewCcb->SearchExpression.Buffer = NULL;

	// Test ILGU LOVE
	//XifsdLockFcb(NULL, pFCB);
	//InsertTailList(&pFCB->CCBListQueue, &NewCcb->LinkToFCB);
	//XifsdUnlockFcb(NULL, pFCB);

	NewCcb->FileObject = FileObject;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_CCB|DEBUG_TARGET_LCB),
		("Exit  xixfs_CreateCCB\n"));
	return NewCcb;
}


VOID
xixfs_InitializeLcbFromDirContext(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_LCB Lcb
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_InitializeLcbFromDirContext \n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
//	if(Lcb=){
//		ASSERT_LCB( Lcb );
//	}
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_InitializeLcbFromDirContext \n"));
}