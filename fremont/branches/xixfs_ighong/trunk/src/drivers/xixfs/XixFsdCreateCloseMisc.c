#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdUpdateFCB)
#pragma alloc_text(PAGE, XixFsdLastUpdateFileFromFCB)
#pragma alloc_text(PAGE, XixFsdDeleteVCB)
#pragma alloc_text(PAGE, XixFsdDeleteFcb)
#pragma alloc_text(PAGE, XixFsdDeleteInternalStream)
#pragma alloc_text(PAGE, XixFsdTeardownStructures)
#pragma alloc_text(PAGE, XixFsdCloseFCB)
#pragma alloc_text(PAGE, XixFsdRealCloseFCB)
#pragma alloc_text(PAGE, XixFsdCreateNewFileObject)
#pragma alloc_text(PAGE, XixFsdCreateAndAllocFCB)
#pragma alloc_text(PAGE, XixFsdCreateInternalStream)
#pragma alloc_text(PAGE, XixFsdInitializeFCBInfo)
#pragma alloc_text(PAGE, XixFsdCreateCCB)
#pragma alloc_text(PAGE, XixFsdInitializeLcbFromDirContext)
#endif

/*
 *	Delete Structure
 */


NTSTATUS
XixFsdUpdateFCB( 
	IN	PXIFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	uint32				blockSize = 0;
	uint8				*buffer;
	PXIFS_VCB			pVCB = NULL;
	LARGE_INTEGER 		Offset;
	PXIDISK_LOT_INFO	pLotInfo = NULL;
	uint32				Reason = 0;
	uint32				NotifyFilter = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdUpdateFCB.\n"));	


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if((pFCB->FCBType != FCB_TYPE_DIR) && (pFCB->FCBType != FCB_TYPE_FILE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL XixFsdUpdateFCB STATUS_INVALID_PARAMETER.\n"));	
		
		return STATUS_INVALID_PARAMETER;
	}
	
	/*
	if((pFCB->FCBType == FCB_TYPE_FILE) && 
		(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE))
	)
	{
		return STATUS_SUCCESS;
	}
	*/

	if(!XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_MODIFIED_FILE) ){
		return STATUS_SUCCESS;
	}

	blockSize = XIDISK_DIR_HEADER_LOT_SIZE;
	
	buffer = ExAllocatePoolWithTag(NonPagedPool, blockSize, TAG_BUFFER);
	
	if(!buffer) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL XixFsdUpdateFCB STATUS_INSUFFICIENT_RESOURCES.\n"));	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		if(pFCB->FCBType == FCB_TYPE_DIR )
		{
			if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
				RC = XixFsLotLock(TRUE, pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock, TRUE, TRUE);
				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL XixFsdUpdateFCB DIR GET LOCK.\n"));	
					try_return(RC);
				}
			}

		}else{
			if (pFCB->HasLock != FCB_FILE_LOCK_HAS) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB FILE HAS NO LOCK .\n"));	

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}

		RC = XixFsReadFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer, 
						blockSize
						);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL  XixFsdUpdateFCB : XixFsReadFileInfoFromFcb .\n"));	

			try_return(RC);
		}


		if(pFCB->FCBType  == FCB_TYPE_DIR )
		{
			PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
			PXIDISK_DIR_INFO		DirInfo = NULL;
			uint8 	*Name = NULL;
			DirLotHeader = (PXIDISK_DIR_HEADER_LOT)buffer;
			DirInfo = &(DirLotHeader->DirInfo);
		

			if(DirInfo->State== XIFS_FD_STATE_DELETED){
				XifsdSetFlag(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : Deleted .\n"));	

				try_return(RC);
			}

			

			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);
				RtlZeroMemory(DirInfo->Name, pFCB->FCBName.MaximumLength);
				RtlCopyMemory(DirInfo->Name, pFCB->FCBName.Buffer, pFCB->FCBName.MaximumLength);
				DirInfo->NameSize = pFCB->FCBName.MaximumLength;
				DirInfo->ParentDirLotIndex = pFCB->ParentLotNumber;
				NotifyFilter |= FILE_NOTIFY_CHANGE_DIR_NAME;

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("XixFsdUpdateFCB : Set New Name (%wZ) .\n", &pFCB->FCBName));

			}

			
			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR);
				DirInfo->FileAttribute = pFCB->FileAttribute;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}


			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
				DirInfo->Create_time = pFCB->CreationTime;
				DirInfo->Change_time = pFCB->LastWriteTime;
				DirInfo->Access_time = pFCB->LastAccessTime;
				DirInfo->Modified_time = pFCB->LastWriteTime;

				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
			}


			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
				DirInfo->FileSize = pFCB->FileSize.QuadPart;
				DirInfo->AllocationSize = pFCB->RealAllocationSize;
				NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
			}



			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT);
				DirInfo->LinkCount = pFCB->LinkCount;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}


		}else{

			PXIDISK_FILE_HEADER_LOT	FileLotHeader = NULL;
			PXIDISK_FILE_INFO FileInfo = NULL;
			uint8 	*Name = NULL;
			FileLotHeader = (PXIDISK_FILE_HEADER_LOT)buffer;
			FileInfo = &(FileLotHeader->FileInfo);
			

			if(FileInfo->State == XIFS_FD_STATE_DELETED){
				XifsdSetFlag(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : Deleted .\n"));	

				try_return(RC);
			}


			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);
				RtlZeroMemory(FileInfo->Name, pFCB->FCBName.MaximumLength);
				RtlCopyMemory(FileInfo->Name, pFCB->FCBName.Buffer, pFCB->FCBName.MaximumLength);
				FileInfo->NameSize = pFCB->FCBName.MaximumLength;
				FileInfo->ParentDirLotIndex = pFCB->ParentLotNumber;
				NotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("XixFsdUpdateFCB : Set New Name (%wZ) .\n", &pFCB->FCBName));
			}

			
			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR);
				FileInfo->FileAttribute = pFCB->FileAttribute;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}


			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
				FileInfo->Create_time = pFCB->CreationTime;
				FileInfo->Change_time = pFCB->LastWriteTime;
				FileInfo->Access_time = pFCB->LastAccessTime;
				FileInfo->Modified_time = pFCB->LastWriteTime;
				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
				NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
			}


			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);
				FileInfo->FileSize = pFCB->FileSize.QuadPart;
				FileInfo->AllocationSize = pFCB->RealAllocationSize;
				NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
			}

			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT);
				FileInfo->LinkCount = pFCB->LinkCount;
				NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
			}

		}

		RC = XixFsWriteFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer, 
						blockSize
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : XixFsWriteFileInfoFromFcb .\n"));
			try_return(RC);
		}

		XixFsdNotifyReportChange(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);


	}finally{
		ExFreePool(buffer);

		if(pFCB->FCBType == FCB_TYPE_DIR)
		{
			RC = XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock);
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdUpdateFCB Status(0x%x).\n", RC));	
	return RC;
}



NTSTATUS
XixFsdDeleteFileLotAddress(
	IN PXIFS_VCB pVCB,
	IN PXIFS_FCB pFCB
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	uint64 * pAddrmapInfo = NULL;
	uint32	i = 0;
	uint32	j = 0;
	uint32	maxCount = 	GetLotCountOfFile(pVCB->LotSize, (pFCB->RealAllocationSize -1));
	uint32	maxSec = 0; //(uint32)((maxCount + 63)/64);
	uint32	AddrCount = 0;
	uint32	DefaultSecPerLotAddr = 0;
	uint32	AddrStartSecIndex = 0;
	uint32	AddrBufferSize = 0;
	uint8	*AddrBuffer = NULL;
	BOOLEAN	bDone = FALSE;	


	ASSERT_VCB(pVCB);
	ASSERT_FCB(pFCB);
	
	AddrBufferSize = pVCB->SectorSize;
	DefaultSecPerLotAddr = (uint32)(pVCB->SectorSize/sizeof(uint64));



	maxSec = (uint32)((maxCount + DefaultSecPerLotAddr -1)/DefaultSecPerLotAddr);

	AddrBuffer = ExAllocatePoolWithTag(NonPagedPool, AddrBufferSize, TAG_BUFFER);

	if(NULL == AddrBuffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	
	try{
		for(i = 0; i<maxSec; i++){

			RC = XixFsRawReadAddressOfFile(pVCB->TargetDeviceObject,
											pVCB->LotSize,
											pFCB->LotNumber,
											pFCB->AddrLotNumber,
											AddrBuffer, 
											AddrBufferSize ,
											&AddrStartSecIndex,
											i,
											pVCB->SectorSize);
											

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, 
					("Error XixFsGetAddressInfoForOffset:ReadAddLot Status(0x%x)\n",RC));

				try_return(RC);
			}

			pAddrmapInfo = (uint64 *)AddrBuffer;
			for(j = 0; j< DefaultSecPerLotAddr; j++){
				
				if(AddrCount > maxCount){
					bDone = TRUE;
					break;
				}

				if(pAddrmapInfo[j] != 0){
					XixFsFreeVCBLot(pVCB, pAddrmapInfo[j]);
					AddrCount++;
				}else{
					bDone = TRUE;
					break;
				}

			}
			
			if(bDone == TRUE){
				break;
			}

		}

		if(pFCB->AddrLotNumber != 0){
			XixFsFreeVCBLot(pVCB, pFCB->AddrLotNumber);
		}



	}finally{
		
		if(AddrBuffer){
			ExFreePool(AddrBuffer);
		}
	}

	return RC;
	
}



NTSTATUS
XixFsdDeleteUpdateFCB(
	IN	PXIFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	uint32				blockSize = 0;
	uint8				*buffer;
	PXIFS_VCB			pVCB = NULL;
	LARGE_INTEGER 		Offset;
	PXIDISK_LOT_INFO	pLotInfo = NULL;
	uint32				Reason = 0;
	uint32				NotifyFilter = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdUpdateFCB.\n"));	


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if((pFCB->FCBType != FCB_TYPE_DIR) && (pFCB->FCBType != FCB_TYPE_FILE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL XixFsdUpdateFCB STATUS_INVALID_PARAMETER.\n"));	
		
		return STATUS_INVALID_PARAMETER;
	}
	
	/*
	if((pFCB->FCBType == FCB_TYPE_FILE) && 
		(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE))
	)
	{
		return STATUS_SUCCESS;
	}
	*/

	blockSize = XIDISK_DIR_HEADER_LOT_SIZE;
	
	buffer = ExAllocatePoolWithTag(NonPagedPool, blockSize, TAG_BUFFER);
	
	if(!buffer) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL XixFsdUpdateFCB STATUS_INSUFFICIENT_RESOURCES.\n"));	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		if(pFCB->FCBType == FCB_TYPE_DIR )
		{
			if(pFCB->HasLock != FCB_FILE_LOCK_HAS){ 


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
					&& (pFCB->FCBCleanup == 0))
				{
					RC = XixFsLotLock(TRUE, pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock, TRUE, TRUE);
					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("FAIL XixFsdUpdateFCB DIR GET LOCK.\n"));	
						try_return(RC);
					}
				}else{
					try_return(RC = STATUS_SUCCESS);
				}

	
			}
		}else{
			if (pFCB->HasLock != FCB_FILE_LOCK_HAS) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB FILE HAS NO LOCK .\n"));	

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}

		RC = XixFsReadFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer, 
						blockSize
						);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL  XixFsdUpdateFCB : XixFsReadFileInfoFromFcb .\n"));	

			try_return(RC);
		}


		if(pFCB->FCBType  == FCB_TYPE_DIR )
		{
			PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
			PXIDISK_DIR_INFO		DirInfo = NULL;
			uint8 	*Name = NULL;
			DirLotHeader = (PXIDISK_DIR_HEADER_LOT)buffer;
			DirInfo = &(DirLotHeader->DirInfo);
		

			if(DirInfo->State== XIFS_FD_STATE_DELETED){
				XifsdSetFlag(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : Deleted .\n"));	

				try_return(RC);
			}

			
			
			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
				&& (DirLotHeader->DirInfo.LinkCount == 1))
			{
				
				RC = XixFsdDeleteFileLotAddress(pVCB, pFCB);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL XixFsdUpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(DirLotHeader, blockSize);
				DirInfo->AddressMapIndex = 0;
				DirInfo->State = XIFS_FD_STATE_DELETED;
				DirInfo->Type =  XIFS_FD_TYPE_INVALID;

				DirLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				DirLotHeader->LotHeader.LotInfo.LotIndex = pFCB->LotNumber;
				DirLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				DirLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				DirLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_DELETED);
				pFCB->HasLock = FCB_FILE_LOCK_INVALID;
				//XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);	
				
			}else if (	XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (DirLotHeader->DirInfo.LinkCount > 1))
			{
				DirInfo->LinkCount--;
			}

			


		}else{

			PXIDISK_FILE_HEADER_LOT	FileLotHeader = NULL;
			PXIDISK_FILE_INFO FileInfo = NULL;
			uint8 	*Name = NULL;
			FileLotHeader = (PXIDISK_FILE_HEADER_LOT)buffer;
			FileInfo = &(FileLotHeader->FileInfo);
			

			if(FileInfo->State== XIFS_FD_STATE_DELETED){
				XifsdSetFlag(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : Deleted .\n"));	

				try_return(RC);
			}



			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
				&& (FileLotHeader->FileInfo.LinkCount == 1))
			{
				
				RC = XixFsdDeleteFileLotAddress(pVCB, pFCB);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL XixFsdUpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(FileLotHeader, blockSize);
				FileInfo->AddressMapIndex = 0;
				FileInfo->State = XIFS_FD_STATE_DELETED;
				FileInfo->Type =  XIFS_FD_TYPE_INVALID;

				FileLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				FileLotHeader->LotHeader.LotInfo.LotIndex = pFCB->LotNumber;
				FileLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				FileLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				FileLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_DELETED);
				pFCB->HasLock = FCB_FILE_LOCK_INVALID;
				//XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);	
				
			}else if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (FileLotHeader->FileInfo.LinkCount > 1))
			{
				FileInfo->LinkCount --;

			}else{
				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);
					RtlZeroMemory(FileInfo->Name, pFCB->FCBName.MaximumLength);
					RtlCopyMemory(FileInfo->Name, pFCB->FCBName.Buffer, pFCB->FCBName.MaximumLength);
					FileInfo->NameSize = pFCB->FCBName.MaximumLength;
					FileInfo->ParentDirLotIndex = pFCB->ParentLotNumber;
					NotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("XixFsdUpdateFCB : Set New Name (%wZ) .\n", &pFCB->FCBName));
				}

				
				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR);
					FileInfo->FileAttribute = pFCB->FileAttribute;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
					FileInfo->Create_time = pFCB->CreationTime;
					FileInfo->Change_time = pFCB->LastWriteTime;
					FileInfo->Access_time = pFCB->LastAccessTime;
					FileInfo->Modified_time = pFCB->LastWriteTime;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
					NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				}


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE)){
					FileInfo->FileSize = pFCB->FileSize.QuadPart;
					FileInfo->AllocationSize = pFCB->RealAllocationSize;
					NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
				}


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT);
					FileInfo->LinkCount = pFCB->LinkCount;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}
			}




		}

		RC = XixFsWriteFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer, 
						blockSize
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : XixFsWriteFileInfoFromFcb .\n"));
			try_return(RC);
		}

		XixFsdNotifyReportChange(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);


		XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_DELETED);


	}finally{
		ExFreePool(buffer);

		if(pFCB->FCBType == FCB_TYPE_DIR)
		{
			RC = XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock);
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdUpdateFCB Status(0x%x).\n", RC));	
	return RC;

}








NTSTATUS
XixFsdUpdateFCBLast( 
	IN	PXIFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	uint32				blockSize = 0;
	uint8				*buffer;
	PXIFS_VCB			pVCB = NULL;
	LARGE_INTEGER 		Offset;
	PXIDISK_LOT_INFO	pLotInfo = NULL;
	uint32				Reason = 0;
	uint32				NotifyFilter = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdUpdateFCB.\n"));	


	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);



	if((pFCB->FCBType != FCB_TYPE_DIR) && (pFCB->FCBType != FCB_TYPE_FILE)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL XixFsdUpdateFCB STATUS_INVALID_PARAMETER.\n"));	
		
		return STATUS_INVALID_PARAMETER;
	}
	
	/*
	if((pFCB->FCBType == FCB_TYPE_FILE) && 
		(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE))
	)
	{
		return STATUS_SUCCESS;
	}
	*/


	
	if(!XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_MODIFIED_FILE) ){
		return STATUS_SUCCESS;
	}
	

	blockSize = XIDISK_DIR_HEADER_LOT_SIZE;
	
	buffer = ExAllocatePoolWithTag(NonPagedPool, blockSize, TAG_BUFFER);
	
	if(!buffer) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL XixFsdUpdateFCB STATUS_INSUFFICIENT_RESOURCES.\n"));	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		if(pFCB->FCBType == FCB_TYPE_DIR )
		{
			if(pFCB->HasLock != FCB_FILE_LOCK_HAS){ 


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
					&& (pFCB->FCBCleanup == 0))
				{
					RC = XixFsLotLock(TRUE, pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock, TRUE, TRUE);
					if(!NT_SUCCESS(RC)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("FAIL XixFsdUpdateFCB DIR GET LOCK.\n"));	
						try_return(RC);
					}
				}else{
					try_return(RC = STATUS_SUCCESS);
				}

	
			}
		}else{
			if (pFCB->HasLock != FCB_FILE_LOCK_HAS) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB FILE HAS NO LOCK .\n"));	

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}
		}

		RC = XixFsReadFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer, 
						blockSize
						);


		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL  XixFsdUpdateFCB : XixFsReadFileInfoFromFcb .\n"));	

			try_return(RC);
		}


		if(pFCB->FCBType  == FCB_TYPE_DIR )
		{
			PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
			PXIDISK_DIR_INFO		DirInfo = NULL;
			uint8 	*Name = NULL;
			DirLotHeader = (PXIDISK_DIR_HEADER_LOT)buffer;
			DirInfo = &(DirLotHeader->DirInfo);
		

			if(DirInfo->State== XIFS_FD_STATE_DELETED){
				XifsdSetFlag(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : Deleted .\n"));	

				try_return(RC);
			}

			
			
			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
				&& (pFCB->FCBCleanup == 0)
				&& (DirLotHeader->DirInfo.LinkCount == 1))
			{
				
				RC = XixFsdDeleteFileLotAddress(pVCB, pFCB);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL XixFsdUpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(DirLotHeader, blockSize);
				DirInfo->AddressMapIndex = 0;
				DirInfo->State = XIFS_FD_STATE_DELETED;
				DirInfo->Type =  XIFS_FD_TYPE_INVALID;

				DirLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				DirLotHeader->LotHeader.LotInfo.LotIndex = pFCB->LotNumber;
				DirLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				DirLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				DirLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				DirLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_DELETED);
				pFCB->HasLock = FCB_FILE_LOCK_INVALID;
				//XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);	
				
			}else if (	XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (DirLotHeader->DirInfo.LinkCount > 1))
			{
				DirInfo->LinkCount--;
			}

			


		}else{

			PXIDISK_FILE_HEADER_LOT	FileLotHeader = NULL;
			PXIDISK_FILE_INFO FileInfo = NULL;
			uint8 	*Name = NULL;
			FileLotHeader = (PXIDISK_FILE_HEADER_LOT)buffer;
			FileInfo = &(FileLotHeader->FileInfo);
			

			if(FileInfo->State== XIFS_FD_STATE_DELETED){
				XifsdSetFlag(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED);

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : Deleted .\n"));	

				try_return(RC);
			}



			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
				&& (pFCB->FCBCleanup == 0)
				&& (FileLotHeader->FileInfo.LinkCount == 1))
			{
				
				RC = XixFsdDeleteFileLotAddress(pVCB, pFCB);

				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
						("FAIL XixFsdUpdateFCB : XixFsdDeleteFileLotAddress .\n"));	
					try_return(RC);
				}
				
				
				RtlZeroMemory(FileLotHeader, blockSize);
				FileInfo->AddressMapIndex = 0;
				FileInfo->State = XIFS_FD_STATE_DELETED;
				FileInfo->Type =  XIFS_FD_TYPE_INVALID;

				FileLotHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
				FileLotHeader->LotHeader.LotInfo.LotIndex = pFCB->LotNumber;
				FileLotHeader->LotHeader.LotInfo.NextLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
				FileLotHeader->LotHeader.LotInfo.StartDataOffset = 0;
				FileLotHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_INVALID;
				FileLotHeader->LotHeader.LotInfo.Flags = LOT_FLAG_INVALID;
				
			
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_CHANGE_DELETED);
				pFCB->HasLock = FCB_FILE_LOCK_INVALID;
				//XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE);	
				
			}else if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)
						&& (pFCB->FCBCleanup == 0)
						&& (FileLotHeader->FileInfo.LinkCount > 1))
			{
				FileInfo->LinkCount --;

			}else{
				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);
					RtlZeroMemory(FileInfo->Name, pFCB->FCBName.MaximumLength);
					RtlCopyMemory(FileInfo->Name, pFCB->FCBName.Buffer, pFCB->FCBName.MaximumLength);
					FileInfo->NameSize = pFCB->FCBName.MaximumLength;
					FileInfo->ParentDirLotIndex = pFCB->ParentLotNumber;
					NotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("XixFsdUpdateFCB : Set New Name (%wZ) .\n", &pFCB->FCBName));
				}

				
				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR);
					FileInfo->FileAttribute = pFCB->FileAttribute;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
					FileInfo->Create_time = pFCB->CreationTime;
					FileInfo->Change_time = pFCB->LastWriteTime;
					FileInfo->Access_time = pFCB->LastAccessTime;
					FileInfo->Modified_time = pFCB->LastWriteTime;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
					NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
					NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
				}


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE)){
					FileInfo->FileSize = pFCB->FileSize.QuadPart;
					FileInfo->AllocationSize = pFCB->RealAllocationSize;
					NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
				}


				if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT)){
					XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT);
					FileInfo->LinkCount = pFCB->LinkCount;
					NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
				}
			}




		}

		RC = XixFsWriteFileInfoFromFcb(
						pFCB,
						TRUE,
						buffer, 
						blockSize
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("FAIL XixFsdUpdateFCB : XixFsWriteFileInfoFromFcb .\n"));
			try_return(RC);
		}

		XixFsdNotifyReportChange(
						pFCB,
						NotifyFilter,
						FILE_ACTION_MODIFIED
						);


	}finally{
		ExFreePool(buffer);

//		if(pFCB->FCBType == FCB_TYPE_DIR)
//		{
//			RC = XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber);
//		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdUpdateFCB Status(0x%x).\n", RC));	
	return RC;
}





NTSTATUS
XixFsdLastUpdateFileFromFCB( 
	IN	PXIFS_FCB	pFCB
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIFS_VCB			pVCB = NULL;

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);





	if(	(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_MODIFIED_FILE))
		|| (pFCB->HasLock == FCB_FILE_LOCK_HAS) ){
		
		try{
			RC = XixFsdUpdateFCBLast(pFCB);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
			
			if(pFCB->WriteStartOffset != -1){
				XixFsdSendFileChangeRC(
						TRUE,
						pVCB->HostMac, 
						pFCB->LotNumber, 
						pVCB->DiskId, 
						pVCB->PartitionId, 
						pFCB->FileSize.QuadPart, 
						pFCB->RealAllocationSize,
						pFCB->WriteStartOffset
				);
				
				pFCB->WriteStartOffset = -1;
			}
			
			/*
			RC = XixFsLotUnLock(pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock);	

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
XixFsdDeleteVCB(
    IN PXIFS_IRPCONTEXT pIrpContext,
    IN PXIFS_VCB pVCB
)
{
	LARGE_INTEGER	TimeOut;

    //PAGED_CODE();

    ASSERT_EXCLUSIVE_XIFS_GDATA;
    ASSERT_EXCLUSIVE_VCB( pVCB );

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
				("Enter XixFsdDeleteVCB (%p).\n", pVCB));



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
	if(pVCB->VolumeFreeMap){
		ExFreePool(pVCB->VolumeFreeMap);
		pVCB->VolumeFreeMap = NULL;
	}

	if(pVCB->HostFreeLotMap){
		ExFreePool(pVCB->HostFreeLotMap);
		pVCB->HostFreeLotMap = NULL;
	}

	if(pVCB->HostDirtyLotMap){
		ExFreePool(pVCB->HostDirtyLotMap);
		pVCB->HostDirtyLotMap = NULL;
	}

	/*
	//	Added by ILGU HONG for 08312006
	if(pVCB->NdasVolBacl_Id){
		XixFsRemoveUserBacl(pVCB->TargetDeviceObject, pVCB->NdasVolBacl_Id);
	}
	
	//	Added by ILGU HONG End
	*/
	//	Added by ILGU HONG 2006 06 12 End


    //
    //  Dereference our target if we haven't already done so.
    //

    if (pVCB->TargetDeviceObject != NULL) {

        ObDereferenceObject( pVCB->TargetDeviceObject );
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_REFCOUNT), 
						 ("XixFsdDeleteVCB TargetDeviceObject(%p)->ReferenceCount (%d) \n", 
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


	

//    IoDeleteDevice( (PDEVICE_OBJECT) CONTAINING_RECORD( pVCB, XI_VOLUME_DEVICE_OBJECT, VCB ));
	IoDeleteDevice(&(pVCB->PtrVDO->DeviceObject));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE| DEBUG_TARGET_VCB), 
				("Exit XixFsdDeleteVCB .\n"));

    return;
}



VOID
XixFsdDeleteFcb(
	IN BOOLEAN CanWait,
	IN PXIFS_FCB		pFCB
)
{
	PXIFS_VCB pVCB = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdDeleteFcb\n"));
	//
	//  Check inputs.
	//

	ASSERT_FCB( pFCB );

	//
	//  Sanity check the counts and Lcb lists.
	//

	ASSERT( pFCB->FCBCleanup == 0 );
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

	switch (pFCB->FCBType) {

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

		try{
			if(pFCB->AddrLot)
				ExFreePool(pFCB->AddrLot);
		}finally{
			if(AbnormalTermination()){
				ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
			}
		}



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
	ExDeleteResourceLite(&(pFCB->AddrResource));

	if(pFCB->FCBName.Buffer){
		ExFreePool(pFCB->FCBName.Buffer);
		pFCB->FCBName.Buffer = NULL;
	}

	if(pFCB->FCBFullPath.Buffer){
		ExFreePool(pFCB->FCBFullPath.Buffer);
		pFCB->FCBFullPath.Buffer = NULL;
	}

	XixFsdFreeFCB(pFCB);

	
	if (pVCB != NULL) {

		InterlockedDecrement( &pVCB->VCBReference );
		InterlockedDecrement( &pVCB->VCBUserReference );
	}
	
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdDeleteFcb\n"));
	return;	
}



VOID
XixFsdDeleteInternalStream( 
	IN BOOLEAN Waitable, 
	IN PXIFS_FCB		pFCB 
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
XixFsdTeardownStructures(
	IN BOOLEAN Waitable,
	IN PXIFS_FCB pFCB,
	IN BOOLEAN Recursive,
	OUT PBOOLEAN RemovedFCB
)
{
	PXIFS_VCB Vcb = pFCB->PtrVCB;
	PXIFS_FCB CurrentFcb = pFCB;
	BOOLEAN AcquiredCurrentFcb = FALSE;
	PXIFS_FCB ParentFcb = NULL;
	PXIFS_LCB Lcb;

	PLIST_ENTRY ListLinks;
	BOOLEAN Abort = FALSE;
	BOOLEAN Removed;
	BOOLEAN	CanWait = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter  XixFsdTeardownStructures\n"));
	//
	//  Check input.
	//

	ASSERT_FCB( pFCB );

	CanWait = Waitable;

	ASSERT(CanWait == TRUE);

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

			if (( (CurrentFcb->FCBType  != FCB_TYPE_FILE) &&
				(CurrentFcb->FCBType  != FCB_TYPE_DIR) )&&
				(CurrentFcb->FCBUserReference == 0) &&
				(CurrentFcb->FileObject != NULL)) {

				//
				//  Go ahead and delete the stream file object.
				//

				XixFsdDeleteInternalStream( CanWait, CurrentFcb );
			}


			if(CanWait){

				
				if(CurrentFcb->FCBCloseCount == 0 ){
					if(XifsdCheckFlagBoolean(CurrentFcb->FCBFlags, XIFSD_FCB_MODIFIED_FILE)){
					
					/*
					DbgPrint(" !!!Last Update (%wZ) FileSize (%I64d) \n", 
								&CurrentFcb->FCBFullPath, CurrentFcb->FileSize.QuadPart);
					
					*/
						// changed by ILGU HONG for readonly 09082006
						if(!CurrentFcb->PtrVCB->IsVolumeWriteProctected){
							XixFsdLastUpdateFileFromFCB(CurrentFcb);
						}
						// changed by ILGU HONG for readonly end
						
					}
				}else if(CurrentFcb->FCBCleanup == 0){
					if(XifsdCheckFlagBoolean(CurrentFcb->FCBFlags, XIFSD_FCB_MODIFIED_FILE)){
						// changed by ILGU HONG for readonly 09082006
						if(!CurrentFcb->PtrVCB->IsVolumeWriteProctected){
							XixFsdUpdateFCB(CurrentFcb);
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

				Lcb = CONTAINING_RECORD( ListLinks, XIFS_LCB, ChildFcbLinks );

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

						XixFsdTeardownStructures( CanWait, ParentFcb, TRUE, &Removed );

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
							 ("XifsdTeardownStructures, Lcb %08x CLotNum(%wZ) (%I64d) <->  Vcb %d/%d PFcb(%wZ) (%I64d) %d/%d CFcb %d/%d\n",
							 Lcb,
							 &CurrentFcb->FCBName,
							 CurrentFcb->LotNumber,
							 Vcb->VCBReference,
							 Vcb->VCBUserReference,
							 &ParentFcb->FCBName,
							 ParentFcb->LotNumber,
							 ParentFcb->FCBReference,
							 ParentFcb->FCBUserReference,
							 CurrentFcb->FCBReference,
							 CurrentFcb->FCBUserReference ));

				XixFsdRemovePrefix( CanWait, Lcb );
				XifsdDecRefCount( ParentFcb, 1, 1 );

				/*
				DbgPrint("dec Link Count CCB with  VCB (%d/%d) ParentFCB:%wZ (%d/%d) ChildPFB:%wZ\n",
						 Vcb->VCBReference,
						 Vcb->VCBUserReference,
						 &ParentFcb->FCBName,
						 ParentFcb->FCBReference,
						 ParentFcb->FCBUserReference,
						 &CurrentFcb->FCBName);	
				*/

				DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_REFCOUNT),
							 ("XifsdTeardownStructures, After remove link Lcb %08x CLotNum(%wZ) (%I64d) <->  Vcb %d/%d PFcb(%wZ) (%I64d) %d/%d CFcb %d/%d\n",
							 Lcb,
							 &CurrentFcb->FCBName,
							 CurrentFcb->LotNumber,
							 Vcb->VCBReference,
							 Vcb->VCBUserReference,
							 &ParentFcb->FCBName,
							 ParentFcb->LotNumber,
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

			if (XifsdCheckFlagBoolean( CurrentFcb->FCBFlags, XIFSD_FCB_IN_TABLE )) {

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
                 ("delete FCB link form FCB Table Delete FCB (%x) LotNumber(%i64d)\n", 
					CurrentFcb, CurrentFcb->LotNumber));

				XixFsdFCBTableDeleteFCB(CurrentFcb );
				XifsdClearFlag( CurrentFcb->FCBFlags, XIFSD_FCB_IN_TABLE );

			}

			//
			//  Unlock the Vcb but hold the parent in order to walk up
			//  the tree.
			//



			XifsdUnlockVcb( IrpContext, Vcb );
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
                 ("delete FCB  (%x) LotNumber (%ld)\n", CurrentFcb, CurrentFcb->LotNumber));



			/*
			 Update FCB Context

			 */
			if((CurrentFcb->FCBType == FCB_TYPE_FILE) || (CurrentFcb->FCBType == FCB_TYPE_DIR))
			{

				if(XifsdCheckFlagBoolean(CurrentFcb->FCBFlags, XIFSD_FCB_MODIFIED_FILE)){
					/*
						DbgPrint(" !!!Last Update (%wZ) FileSize (%I64d) \n", 
								&CurrentFcb->FCBFullPath, CurrentFcb->FileSize.QuadPart);
					*/	
					// changed by ILGU HONG for readonly 09082006
					if(!CurrentFcb->PtrVCB->IsVolumeWriteProctected){
						XixFsdLastUpdateFileFromFCB(CurrentFcb);
					}
					// changed by ILGU HONG for readonly end
					
				}

				if(CurrentFcb->HasLock == FCB_FILE_LOCK_HAS){
					XixFsLotUnLock(CurrentFcb->PtrVCB, 
									CurrentFcb->PtrVCB->TargetDeviceObject, 
									CurrentFcb->LotNumber, 
									&CurrentFcb->HasLock);
				}
			}

			



			
			XixFsdDeleteFcb( CanWait, CurrentFcb );



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
		("Exit  XixFsdTeardownStructures\n"));
	return;


}





BOOLEAN
XixFsdCloseFCB(
	IN BOOLEAN		Waitable,
	IN PXIFS_VCB	pVCB,
	IN PXIFS_FCB	pFCB,
	IN uint32	UserReference
)
{
	BOOLEAN		CanWait = FALSE;
	BOOLEAN		RemovedFCB = FALSE;
	BOOLEAN		RT = FALSE;
	
	PAGED_CODE();


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Enter XixFsdCloseFCB FCB(%p)\n", pFCB));

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
			("XixFsdCloseFCB Acquire exclusive FCBResource (%p)\n", &pFCB->FCBResource));
	


	XifsdLockVcb(TRUE, pVCB);
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
                 ("XixFsdCloseFCB, Fcb LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
				 pFCB->LotNumber,
                 pVCB->VCBReference,
                 pVCB->VCBUserReference,
                 pFCB->FCBReference,
                 pFCB->FCBUserReference ));

	
	XifsdDecRefCount(pFCB, 1, UserReference);
    
	/*
	DbgPrint("dec Close with CCB Count CCB with  VCB (%d/%d) FCB:%wZ (%d/%d)\n",
		 pVCB->VCBReference,
		 pVCB->VCBUserReference,
		 &pFCB->FCBName,
		 pFCB->FCBReference,
		 pFCB->FCBUserReference);	
	*/

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
                 ("XixFsdCloseFCB, Fcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
				 pFCB->LotNumber,
                 pVCB->VCBReference,
                 pVCB->VCBUserReference,
                 pFCB->FCBReference,
                 pFCB->FCBUserReference ));
	
	XifsdUnlockVcb(TRUE, pVCB);
	
	XixFsdTeardownStructures(Waitable, pFCB, FALSE, &RemovedFCB);

	if(!RemovedFCB){
		DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
                 ("XixFsdCloseFCB is FALSE !\n"));

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
                 ("XixFsdCloseFCB is TRUE !\n"));
		RT = TRUE;
	}
	

	XifsdReleaseVcb(Waitable, pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Exit XixFsdCloseFCB \n"));
	return RT;
}


PXIFS_FCB
XixFsdRemoveCloseQueue(
	IN PXIFS_VCB pVCB
)
{
	PLIST_ENTRY Entry = NULL;
	PXIFS_CLOSE_FCB_CTX	pCtx = NULL;
	PXIFS_FCB pFCB = NULL;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Enter XixFsdRemoveCloseQueue \n"));

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
				("Get Fcb (0x%p)  Id(%ld) XixFsdRemoveCloseQueue \n", pFCB, pFCB->LotNumber));

			XixFsdFreeCloseFcbCtx(pCtx);
			pCtx = NULL;
			return pFCB;					
		}
	}
	XifsdUnlockVcb(TRUE, pVCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Exit XixFsdRemoveCloseQueue FCB(%p)\n", pFCB));
	return NULL;
}



VOID
XixFsdCallCloseFCB(
	IN PVOID Context
)
{
	PXIFS_VCB pVCB = NULL;

	pVCB = (PXIFS_VCB)Context;
	ASSERT_VCB(pVCB);
	
	PAGED_CODE();
	XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE);
	XixFsdRealCloseFCB((PVOID)pVCB);
	XifsdReleaseVcb(TRUE, pVCB);
}



VOID
XixFsdRealCloseFCB(
	IN PVOID Context
)
{
	PXIFS_FCB	pFCB = NULL;
	PXIFS_VCB	pVCB = (PXIFS_VCB)Context;
	BOOLEAN		IsSetVCB = FALSE;
	uint32		lockedVcbValue = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE),
			("Enter XixFsdRealCloseFCB\n"));	

	ASSERT_VCB(pVCB);
	//ASSERT_EXCLUSIVE_VCB(pVCB);


	pFCB = XixFsdRemoveCloseQueue(pVCB);

	while(pFCB != NULL)
	{
		ASSERT_FCB(pFCB);
		pVCB = pFCB->PtrVCB;
		ASSERT_VCB(pVCB);
		XixFsdCloseFCB(TRUE, pVCB, pFCB, 0);
		pFCB = XixFsdRemoveCloseQueue(pVCB);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE),
			("Exit XixFsdRealCloseFCB\n"));	
}




VOID
XixFsdInsertCloseQueue(
	IN PXIFS_FCB pFCB
)
{
	BOOLEAN CloseActive = FALSE;
	PXIFS_VCB				pVCB = NULL;
	PXIFS_CLOSE_FCB_CTX		pCtx = NULL;


	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
			("Enter XixFsdInsertCloseQueue FCB(%p)\n", pFCB));

	//DbgPrint("Enter XixFsdInsertCloseQueue FCB(%p)\n", pFCB);
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);
	//ASSERT_EXCLUSIVE_VCB(pVCB);


	pCtx = XixFsdAllocateCloseFcbCtx();
	
	if(pCtx == NULL)
	{
		XifsdLockVcb(TRUE, pVCB);

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					 ("XixFsdCloseFCB, Fcb LotNumber(%I64d) Vcb (%d/%d) Fcb (%d/%d)\n", 
					 pFCB->LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pFCB->FCBReference,
					 pFCB->FCBUserReference ));

		
		XifsdDecRefCount(pFCB, 1, 0);
    
		DbgPrint("dec Close  Count CCB with  VCB (%d/%d) FCB:%wZ (%d/%d)\n",
			 pVCB->VCBReference,
			 pVCB->VCBUserReference,
			 &pFCB->FCBName,
			 pFCB->FCBReference,
			 pFCB->FCBUserReference);	

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					 ("XixFsdCloseFCB, Fcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
					 pFCB->LotNumber,
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
		
		if(XifsdCheckFlagBoolean(pVCB->VCBFlags, XIFSD_VCB_FLAGS_DEFERED_CLOSE) == FALSE){
			if(pVCB->DelayedCloseCount > 0){
				CloseActive = TRUE;
			}
		}

		
		XifsdUnlockVcb(TRUE, pVCB);

		if(CloseActive == TRUE){
			ExQueueWorkItem(&pVCB->WorkQueueItem, CriticalWorkQueue);
		}
		//DbgPrint("Exit XixFsdInsertCloseQueue FCB(%p) and CTX (%p)\n", pFCB, pCtx);
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CLOSE|DEBUG_TARGET_FCB),
				("Exit XixFsdInsertCloseQueue FCB(%p) and CTX (%p)\n", pFCB, pCtx));
	}
	
	return;
}

/*
 *	Create
 */

NTSTATUS
XixFsdCreateNewFileObject(
	IN PXIFS_IRPCONTEXT	pIrpContext,
	IN PXIFS_VCB 		PtrVCB, 
	IN PXIFS_FCB 		ParentFCB,
	IN BOOLEAN			IsFileOnly,
	IN BOOLEAN			DeleteOnCloseSpecified,
	IN uint8 *			Name, 
	IN uint32 			NameLength, 
	IN uint32 			FileAttribute,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	OUT uint64			*NewFileId
)
{
	NTSTATUS 	RC = STATUS_UNSUCCESSFUL;
	int64 LotCount = 0;
	int64 i = 0,j=0; 
	uint32 tType =0;
	int64 BeginingLotIndex = 0;
	char * buffer = NULL;
	uint32 blockSize = 0;
	uint64 ChildIndex = 0;
	PXIFS_FCB FCB = NULL;
	PXIFS_CCB CCB = NULL;

	
	PXIDISK_FILE_HEADER 			pFileHeader = NULL;
	PXIDISK_DIR_HEADER 				pDirHeader = NULL;
	PXIDISK_COMMON_LOT_HEADER		pLotHeader = NULL;
	PXIDISK_FILE_INFO 				pFileInfo = NULL;
	PXIDISK_DIR_INFO 				pDirInfo = NULL;
 	PXIDISK_ADDR_MAP				pAddressMap = NULL;
	LARGE_INTEGER 					Offset;
	UNICODE_STRING					ChildName;
	uint32							ChildType;
	uint32							AddrStartSectorIndex = 0;

	BOOLEAN			Wait = FALSE;
	BOOLEAN			IsDirSet = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XifsdCreateNewFileObject \n"));
		

	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT_FCB(ParentFCB);
	ASSERT_VCB(PtrVCB);
	
	
	// Changed by ILGU HONG for readonly 09052006
	if(PtrVCB->IsVolumeWriteProctected){
		RC = STATUS_MEDIA_WRITE_PROTECTED;
		return RC;
	}
	// Changed by ILGU HONG for readonly end

	Wait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	

	ChildType = (IsFileOnly)?(XIFS_FD_TYPE_FILE):(XIFS_FD_TYPE_DIRECTORY);

	if(NameLength > XIFS_MAX_FILE_NAME_LENGTH){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("FileNameLen(%ld).\n", NameLength));	
		NameLength = XIFS_MAX_FILE_NAME_LENGTH;
	}


	blockSize = XIDISK_FILE_HEADER_LOT_SIZE;
	buffer = ExAllocatePool(PagedPool,blockSize);

	if(!buffer){
		return STATUS_INSUFFICIENT_RESOURCES;
	}



	try{

		BeginingLotIndex = XixFsAllocVCBLot(PtrVCB);
		if(BeginingLotIndex == -1){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}

		
		ChildName.Buffer = (PWSTR)Name;
		ChildName.Length = ChildName.MaximumLength = (uint16)NameLength;
		
		
		//XifsdDebugLevel = DEBUG_LEVEL_ALL;
		//XifsdDebugTarget = DEBUG_TARGET_ALL;



	
		RC = XixFsAddChildToDir(
							TRUE,
							PtrVCB,
							ParentFCB,
							BeginingLotIndex,
							ChildType,
							&ChildName,
							DirContext,
							&ChildIndex
							);
	

		//XifsdDebugLevel = DEBUG_LEVEL_TEST;
		//XifsdDebugTarget = DEBUG_TARGET_FASTIO| DEBUG_TARGET_READ|DEBUG_TARGET_FILEINFO;

		if(!NT_SUCCESS(RC)){
			XixFsFreeVCBLot(PtrVCB, BeginingLotIndex);
			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}	
		




		IsDirSet = TRUE;



		RtlZeroMemory(buffer, blockSize);

		
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
					("Make File Info.\n"));

		if(ChildType & XIFS_FD_TYPE_FILE){
			
			pFileHeader = (PXIDISK_FILE_HEADER)buffer;
			pLotHeader = (PXIDISK_COMMON_LOT_HEADER)buffer;

			XixFsInitializeCommonLotHeader(	
				pLotHeader,
				PtrVCB->VolumeLotSignature,
				LOT_INFO_TYPE_FILE,
				LOT_FLAG_BEGIN,
				BeginingLotIndex,
				BeginingLotIndex,
				0,
				0,
				0,
				sizeof(XIDISK_FILE_HEADER),
				PtrVCB->LotSize - sizeof(XIDISK_FILE_HEADER)
				);



			pLotHeader->Lock.LockState =  XIDISK_LOCK_RELEASED;
			pLotHeader->Lock.LockAcquireTime=0;
			RtlCopyMemory(pLotHeader->Lock.LockHostSignature, PtrVCB->HostId , 16);
			// Changed by ILGU HONG
			//	chesung suggest
			//RtlCopyMemory(pLotHeader->Lock.LockHostMacAddress, PtrVCB->HostMac, 6);
			RtlCopyMemory(pLotHeader->Lock.LockHostMacAddress, PtrVCB->HostMac, 32);
			
			
			pFileInfo = &pFileHeader->FileInfo;
			
			pFileInfo->Access_time = XixGetSystemTime().QuadPart;
			pFileInfo->Change_time = XixGetSystemTime().QuadPart;
			pFileInfo->Create_time = XixGetSystemTime().QuadPart;
			pFileInfo->Modified_time = XixGetSystemTime().QuadPart;
			RtlCopyMemory(pFileInfo->OwnHostId ,PtrVCB->HostId, 16);
			pFileInfo->ParentDirLotIndex = ParentFCB->LotNumber;
			pFileInfo->LotIndex = BeginingLotIndex;
			pFileInfo->State = XIFS_FD_STATE_CREATE;
			pFileInfo->ACLState = 0;
			pFileInfo->FileAttribute = (FileAttribute|FILE_ATTRIBUTE_ARCHIVE);
			pFileInfo->AccessFlags = FILE_SHARE_READ;
			pFileInfo->FileSize = 0;
			pFileInfo->AllocationSize = PtrVCB->LotSize - XIDISK_FILE_HEADER_SIZE;
			pFileInfo->AddressMapIndex = 0;
			pFileInfo->LinkCount = 1;
			pFileInfo->NameSize = NameLength;
			pFileInfo->AddressMapIndex = 0;
			XifsdSetFlag(pFileInfo->Type, XIFS_FD_TYPE_FILE);
			RtlCopyMemory(pFileInfo->Name, Name, NameLength);
			
			//pAddressMap = &pFileHeader->AddrInfo;
			//pAddressMap->LotNumber[0] = BeginingLotIndex;

			
		}else{
		
			pDirHeader = (PXIDISK_DIR_HEADER)buffer;
			pLotHeader = (PXIDISK_COMMON_LOT_HEADER)buffer;
		
			XixFsInitializeCommonLotHeader(	
				pLotHeader,
				PtrVCB->VolumeLotSignature,
				LOT_INFO_TYPE_DIRECTORY,
				LOT_FLAG_BEGIN,
				BeginingLotIndex,
				BeginingLotIndex,
				0,
				0,
				0,
				sizeof(XIDISK_FILE_HEADER_LOT),
				PtrVCB->LotSize - sizeof(XIDISK_DIR_HEADER_LOT)
				);

			pLotHeader->Lock.LockState=  XIDISK_LOCK_RELEASED;
			pLotHeader->Lock.LockAcquireTime= 0;;
			RtlCopyMemory(pLotHeader->Lock.LockHostSignature, PtrVCB->HostId , 16);
			// Changed by ILGU HONG
			//	chesung suggest
			//RtlCopyMemory(pLotHeader->Lock.LockHostMacAddress, PtrVCB->HostMac, 6);
			RtlCopyMemory(pLotHeader->Lock.LockHostMacAddress, PtrVCB->HostMac, 32);

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Make Lot Info.\n"));

			pDirInfo = &(pDirHeader->DirInfo);

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
					("Make FileInfo 0.\n"));
			pDirInfo->Access_time = XixGetSystemTime().QuadPart;
			pDirInfo->Change_time = XixGetSystemTime().QuadPart;
			pDirInfo->Create_time = XixGetSystemTime().QuadPart;
			pDirInfo->Modified_time = XixGetSystemTime().QuadPart;
			
			RtlCopyMemory(pDirInfo->OwnHostId ,PtrVCB->HostId, 16);
			pDirInfo->ParentDirLotIndex = ParentFCB->LotNumber;
			pDirInfo->LotIndex = BeginingLotIndex;
			pDirInfo->State = XIFS_FD_STATE_CREATE;
			pDirInfo->ACLState = 0;
			pDirInfo->FileAttribute = (FileAttribute | FILE_ATTRIBUTE_DIRECTORY);
			pDirInfo->AccessFlags = FILE_SHARE_READ;
			pDirInfo->FileSize = 0;
			pDirInfo->AllocationSize = PtrVCB->LotSize - XIDISK_DIR_HEADER_SIZE;
			pDirInfo->LinkCount = 1;
			XifsdSetFlag(pDirInfo->Type, XIFS_FD_TYPE_DIRECTORY);
			pDirInfo->NameSize = NameLength;

		
			RtlCopyMemory(pDirInfo->Name, Name, NameLength);	
			pDirInfo->AddressMapIndex = 0;

			
			//pAddressMap = &pDirHeader->AddrInfo;
			//pAddressMap->LotNumber[0] = BeginingLotIndex;
		}


		RC = XixFsRawWriteLotAndFileHeader(
						PtrVCB->TargetDeviceObject,
						PtrVCB->LotSize,
						BeginingLotIndex,
						buffer,
						blockSize,
						PtrVCB->SectorSize
						);

		if(!NT_SUCCESS(RC)){	
			try_return(RC);
		}

		
		RtlZeroMemory(buffer, blockSize);
		pAddressMap = (PXIDISK_ADDR_MAP)buffer;
		pAddressMap->LotNumber[0] = BeginingLotIndex;
			
		RC = XixFsRawWriteAddressOfFile(
						PtrVCB->TargetDeviceObject,
						PtrVCB->LotSize, 
						BeginingLotIndex, 
						0, 
						(uint8 *)pAddressMap, 
						PtrVCB->SectorSize,  
						&AddrStartSectorIndex, 
						0,
						PtrVCB->SectorSize
						);

		if(!NT_SUCCESS(RC)){	
			try_return(RC);
		}


		*NewFileId = BeginingLotIndex;


		KeSetEvent(&PtrVCB->VCBUpdateEvent, 0, FALSE);

	}finally{

		if(AbnormalTermination()){
			if(IsDirSet){



				XixFsDeleteChildFromDir(
							TRUE,
							PtrVCB, 
							ParentFCB, 
							ChildIndex,
							DirContext);




			}

			if(BeginingLotIndex != 0 && BeginingLotIndex != -1) XixFsFreeVCBLot(PtrVCB, BeginingLotIndex);
		}

		ExFreePool(buffer);
		
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XifsdCreateNewFileObject \n"));
	return RC;
}



PXIFS_FCB
XixFsdCreateAndAllocFCB(
	IN PXIFS_VCB		pVCB,
	IN uint64			FileId,
	IN uint32			FCB_TYPE_CODE,
	OUT PBOOLEAN		Exist OPTIONAL
)
{
	PXIFS_FCB	pFCB = NULL;
	BOOLEAN		LocalExist = FALSE;
	PAGED_CODE();


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XifsdCreateAndAllocFCB FileId (%I64d)\n", FileId));

	if(!ARGUMENT_PRESENT(Exist)){
		Exist = &LocalExist;
	}

	pFCB = XixFsdLookupFCBTable(pVCB, FileId);
	


	if(pFCB == NULL){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("XifsdCreateAndAllocFCB Create New FCB\n"));

		try{
			pFCB = XixFsdAllocateFCB();

			ExInitializeResourceLite(&(pFCB->FCBResource));
			ExInitializeFastMutex(&(pFCB->FCBMutex));
			ExInitializeResourceLite(&(pFCB->MainResource));
			ExInitializeResourceLite(&(pFCB->RealPagingIoResource));
			ExInitializeResourceLite(&(pFCB->AddrResource));

			InitializeListHead(&(pFCB->ParentLcbQueue));
			InitializeListHead(&(pFCB->ChildLcbQueue));
			//InitializeListHead(&(pFCB->DelayedCloseLink));
			InitializeListHead(&(pFCB->EndOfFileLink));
			InitializeListHead(&(pFCB->CCBListQueue));


			pFCB->FCBType = FCB_TYPE_CODE; 
			pFCB->PtrVCB = pVCB;
			pFCB->LotNumber = FileId;
			
			pFCB->PagingIoResource = &pFCB->RealPagingIoResource;
			pFCB->Resource = &pFCB->MainResource;
			pFCB->WriteStartOffset = -1;


		}finally{
			DebugUnwind("XifsdCreateAndAllocFCB");
			if(AbnormalTermination()){
				XixFsdFreeFCB(pFCB);
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
XixFsdCreateInternalStream (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_VCB pVCB,
    IN PXIFS_FCB pFCB,
	IN BOOLEAN	ReadOnly
)
{
	PFILE_OBJECT StreamFile = NULL;
	BOOLEAN DecRef = FALSE;
	BOOLEAN	UnlockFcb = FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdCreateInternalStream\n"));

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
		
		XixFsdSetFileObject(IrpContext, StreamFile, StreamFileOpen, pFCB, NULL);



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

		DebugUnwind("XixFsdCreateInternalStream");
        
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
		("Exit XixFsdCreateInternalStream\n"));
	return;
	

}


NTSTATUS
XixFsdInitializeFCBInfo(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable
)
{
	NTSTATUS RC = STATUS_SUCCESS;

	ASSERT_FCB(pFCB);
	ASSERT(Waitable == TRUE);
	try{
		RC = XixFsInitializeFCBInfo(pFCB, Waitable);
	}finally{

	}


	if(NT_SUCCESS(RC)){
		XixFsdFCBTableInsertFCB(pFCB);

		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_IN_TABLE);
		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_INIT);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
			("FCBFlags (0x%x)\n", pFCB->FCBFlags));	
		
	}

	return RC;
}


PXIFS_CCB
XixFsdCreateCCB(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN PXIFS_FCB pFCB, 
	IN PXIFS_LCB pLCB,
	IN uint32	CCBFlags
)
{
	PXIFS_CCB NewCcb;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_CCB|DEBUG_TARGET_LCB),
		("Enter  XixFsdCreateCCB\n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_FCB( pFCB );
	ASSERT_OPTIONAL_LCB( pLCB );

	//
	//  Allocate and initialize the structure.
	//

	NewCcb = XixFsdAllocateCCB();

	XifsdSetFlag(NewCcb->CCBFlags, CCBFlags);
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
		("Exit  XixFsdCreateCCB\n"));
	return NewCcb;
}


VOID
XixFsdInitializeLcbFromDirContext(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_LCB Lcb
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdInitializeLcbFromDirContext \n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
//	if(Lcb=){
//		ASSERT_LCB( Lcb );
//	}
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdInitializeLcbFromDirContext \n"));
}