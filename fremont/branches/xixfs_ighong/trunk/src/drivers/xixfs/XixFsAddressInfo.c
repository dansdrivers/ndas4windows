#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDiskForm.h"
#include "XixFsDrv.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsGetAddressInfoForOffset)
#pragma alloc_text(PAGE, XixFsAddNewLot)
#pragma alloc_text(PAGE, XixFsCheckEOFAndAllocLotFileInfo)
#endif





#define MAX_NEW_LOT_COUNT 64

typedef struct _XI_ADDR_TEMP{
	uint64 tmpAddressBuffer[MAX_NEW_LOT_COUNT];
	uint64 PrevAddrBuffer[MAX_NEW_LOT_COUNT];
	uint32 Index[MAX_NEW_LOT_COUNT];
}XI_ADDR_TEMP, *PXI_ADDR_TEMP;


NTSTATUS
XixFsGetAddressInfoForOffset(
	IN BOOLEAN			Waitable,
	IN PXIFS_FCB		pFCB,
	IN uint64			Offset,
	IN uint32			LotSize,
	IN uint8			* Addr,
	IN uint32			AddrSize,
	IN uint32			* AddrStartSecIndex,
	IN uint32			* Reason,
	IN OUT PXIFS_IO_LOT_INFO	pAddress
)
{
	NTSTATUS		RC = 0;
	PXIFS_VCB		pVCB = NULL;
	uint32 			LotIndex = 0;
	uint32			RealLotIndex = 0;
	uint32			SecNumber = 0;

	uint32 			LotType = 0;
	uint32 			AllocatedAddrCount = 0;
	int32 			ReadAddrCount = 0;
	uint32			DefaultSecPerLotAddr = (uint32)(SECTORSIZE_512/sizeof(uint64));
	uint64			*AddrLot = NULL;



	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter XixFsGetAddressInfoForOffset\n"));
	
	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT(AddrSize >= pVCB->SectorSize);




	if(pFCB->FCBType == FCB_TYPE_FILE){
		if(Offset > (uint64)pFCB->FileSize.QuadPart) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Error XixFsGetAddressInfoForOffset Offset(%I64d) > pFCB->FileSize.QuadPart(%I64d)\n", 
					Offset, pFCB->FileSize.QuadPart));

			*Reason =  XI_CODE_ADDR_INVALID;
			return STATUS_UNSUCCESSFUL;
		}
	}else{
		if(Offset > (uint64)pFCB->RealAllocationSize) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Error XixFsGetAddressInfoForOffset Offset(%I64d) > pFCB->RealAllocationSize(%I64d)\n", 
					Offset, pFCB->RealAllocationSize));

			*Reason =  XI_CODE_ADDR_INVALID;
			return STATUS_UNSUCCESSFUL;
		}
	}


	
	ASSERT((pFCB->FCBType == FCB_TYPE_FILE)|| (pFCB->FCBType == FCB_TYPE_DIR));

	ExAcquireResourceExclusiveLite(&(pFCB->AddrResource), TRUE);
	try{
		LotIndex = GetIndexOfLogicalAddress( LotSize, Offset);
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
				("FCB(%I64d) LotIndex(%ld) Offset(%I64d)\n",
				pFCB->LotNumber, LotIndex, Offset));
		
		DefaultSecPerLotAddr = (uint32)(pVCB->SectorSize/sizeof(uint64));



		SecNumber = (uint32)(LotIndex / DefaultSecPerLotAddr);

		if(*AddrStartSecIndex != SecNumber){
			if(Waitable == TRUE){

				RC = XixFsRawReadAddressOfFile(pVCB->TargetDeviceObject,
												pVCB->LotSize,
												pFCB->LotNumber,
												pFCB->AddrLotNumber,
												Addr, 
												AddrSize ,
												AddrStartSecIndex,
												SecNumber,
												pVCB->SectorSize);
												
				
				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, 
						("Error XixFsGetAddressInfoForOffset:ReadAddLot Status(0x%x)\n",RC));

					try_return(RC);
				}
			}else{
				return STATUS_UNSUCCESSFUL;
			}
		}


		AddrLot = (uint64 *)Addr;
		
		RealLotIndex = LotIndex - (DefaultSecPerLotAddr* (uint32)SecNumber);

		
		pAddress->BeginningLotIndex = pFCB->LotNumber;
		pAddress->LogicalStartOffset = GetLogicalStartAddrOfFile(LotIndex,LotSize);
		
		if(LotIndex == 0){
			pAddress->LotTotalDataSize =  LotSize - XIDISK_FILE_HEADER_SIZE;
			pAddress->Flags = LOT_FLAG_BEGIN;
		}else{
			pAddress->LotTotalDataSize = LotSize- XIDISK_DATA_LOT_SIZE;
			pAddress->Flags = LOT_FLAG_BODY;
		}
		
		pAddress->LotIndex = AddrLot[RealLotIndex];
		if(RealLotIndex  > 0){
			pAddress->PreviousLotIndex = AddrLot[RealLotIndex -1];	
		}else {
			pAddress->PreviousLotIndex = 0;
		}

		if(RealLotIndex < (DefaultSecPerLotAddr -1)){
			pAddress->NextLotIndex = AddrLot[RealLotIndex + 1];
		}else{
			pAddress->NextLotIndex = 0;
		}

		if(pAddress->LotIndex == 0){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Error XixFsGetAddressInfoForOffset pAddress->LotIndex == 0 Req Offset(%I64d) Real Lot Index(%ld)\n"
					,Offset,RealLotIndex));
			//DbgBreakPoint();
			ASSERT(FALSE);
			*Reason =  XI_CODE_ADDR_INVALID;
			return STATUS_UNSUCCESSFUL;
		}

		pAddress->PhysicalAddress = GetAddressOfFileData(pAddress->Flags, LotSize, pAddress->LotIndex);

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
				("ADDR INFO[FCB(%I64d)] : Begin(%I64d) LogAddr(%I64d) PhyAddr(%I64d) TotDataSize(%ld)\n",
				pFCB->LotNumber,
				pAddress->BeginningLotIndex, 
				pAddress->LogicalStartOffset, 
				pAddress->PhysicalAddress,
				pAddress->LotTotalDataSize));

	}finally{
		ExReleaseResourceLite(&(pFCB->AddrResource));
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit XixFsGetAddressInfoForOffset Status(0x%x)\n", RC));	

	return RC;
	
}

NTSTATUS
XixFsAddNewLot(
	IN PXIFS_VCB pVCB, 
	IN PXIFS_FCB pFCB, 
	IN uint32	RequestStatIndex, 
	IN uint32	LotCount,
	IN uint32	*AddedLotCount,
	IN uint8	* Addr,
	IN uint32	AddrSize,
	IN uint32	* AddrStartSecIndex
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint32			SecNum = 0;
	uint32			RealLotIndex = 0;
	PXI_ADDR_TEMP	AddrInfo = NULL;
	uint32 i =0;
	BOOLEAN			bGetLot = FALSE;
	PUCHAR			Buff = NULL;
	uint64			LastValidLotNumber = 0;
	uint32			NewLotIndex = 0;
	uint32			DefaultSecPerLotAddr = 0;
	uint64			*AddrLot = NULL;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter XixFsAddNewLot\n"));


	ASSERT_VCB(pVCB);
	ASSERT_FCB(pFCB);
	

	Buff = ExAllocatePoolWithTag(NonPagedPool,XIDISK_COMMON_LOT_HEADER_SIZE, TAG_BUFFER );

	if(!Buff){
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	

	AddrInfo = (PXI_ADDR_TEMP)ExAllocatePoolWithTag(NonPagedPool, sizeof(XI_ADDR_TEMP), TAG_BUFFER);
	if(!AddrInfo){
		ExFreePool(Buff);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(AddrInfo, sizeof(XI_ADDR_TEMP));

	


	// Adjust Lot Count
	if(LotCount > MAX_NEW_LOT_COUNT){
		LotCount = MAX_NEW_LOT_COUNT; 
	}	
	

		
	AddrLot = (uint64 *)Addr;

	DefaultSecPerLotAddr = (uint32)(pVCB->SectorSize/sizeof(uint64));



	SecNum = (uint32)(RequestStatIndex / DefaultSecPerLotAddr);
	RealLotIndex = RequestStatIndex - (DefaultSecPerLotAddr*(uint32)SecNum);
	NewLotIndex = RequestStatIndex + 1;
 
	ExAcquireResourceExclusiveLite(&(pFCB->AddrResource), TRUE);

	try{
		// Check Current Request Index
		if (SecNum != *AddrStartSecIndex){
			RC = XixFsRawReadAddressOfFile(pVCB->TargetDeviceObject,
											pVCB->LotSize,
											pFCB->LotNumber,
											pFCB->AddrLotNumber,
											(uint8 *)AddrLot,
											AddrSize, 
											AddrStartSecIndex, 
											SecNum,
											pVCB->SectorSize);
										
			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
		}

		if(RealLotIndex == (DefaultSecPerLotAddr-1))
		{
			LastValidLotNumber = AddrLot[RealLotIndex];

			SecNum++;
			RC = XixFsRawReadAddressOfFile(pVCB->TargetDeviceObject,
											pVCB->LotSize,
											pFCB->LotNumber,
											pFCB->AddrLotNumber,
											(uint8 *)AddrLot,
											AddrSize, 
											AddrStartSecIndex, 
											SecNum,
											pVCB->SectorSize);
											
				
				
		
			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}

			RealLotIndex = 0;	

		}else{



			LastValidLotNumber = AddrLot[RealLotIndex];
			RealLotIndex ++;
		}
		
		for(i =0; i<LotCount; i++){
			AddrInfo->tmpAddressBuffer[i] = XixFsAllocVCBLot(pVCB);
			
			if(AddrInfo->tmpAddressBuffer[i] == -1){
				bGetLot = TRUE;

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("ERROR XixFsAddNewLot:XifsdAllocVCBLot Fail Insufficient resource\n"));

				RC =  STATUS_DISK_FULL;
				try_return(RC);
			}
			
			AddrInfo->Index[i] = NewLotIndex ++;
			
			if(i ==0){
				AddrInfo->PrevAddrBuffer[i] = LastValidLotNumber;
			}else{
				AddrInfo->PrevAddrBuffer[i] = AddrInfo->tmpAddressBuffer[i-1];
			}
		}

		bGetLot = TRUE;
		
		for(i = 0; i<LotCount; i++){
			AddrLot[RealLotIndex] = AddrInfo->tmpAddressBuffer[i];
			if(RealLotIndex == (DefaultSecPerLotAddr -1)){
				RC = XixFsRawWriteAddressOfFile(pVCB->TargetDeviceObject,
											pVCB->LotSize,
											pFCB->LotNumber,
											pFCB->AddrLotNumber,
											(uint8 *)AddrLot,
											AddrSize, 
											AddrStartSecIndex, 
											SecNum,
											pVCB->SectorSize);
											
					
				if(!NT_SUCCESS(RC)){
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("ERROR XixFsAddNewLot:WriteAddLot Status(0x%x)\n", RC));
					
					try_return(RC);
	
				}

				SecNum++;
				RealLotIndex = 0;
				RC = XixFsRawReadAddressOfFile(pVCB->TargetDeviceObject,
											pVCB->LotSize,
											pFCB->LotNumber,
											pFCB->AddrLotNumber,
											(uint8 *)AddrLot,
											AddrSize, 
											AddrStartSecIndex, 
											SecNum,
											pVCB->SectorSize);
											
					
					
				if(!NT_SUCCESS(RC)){
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("ERROR XixFsAddNewLot:ReadAddLot Status(0x%x)\n", RC));

					try_return(RC);
				}
			}else{
				RealLotIndex++;
			}

			
		}

		
		RC = XixFsRawWriteAddressOfFile(pVCB->TargetDeviceObject,
											pVCB->LotSize,
											pFCB->LotNumber,
											pFCB->AddrLotNumber,
											(uint8 *)AddrLot,
											AddrSize, 
											AddrStartSecIndex, 
											SecNum,
											pVCB->SectorSize);
											
			
		
		if(!NT_SUCCESS(RC)){
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR XixFsAddNewLot:WriteAddLot Status(0x%x)\n", RC));
			
			try_return(RC);

		}		


		for(i = 0; i<LotCount; i++){
			RtlZeroMemory(Buff, XIDISK_COMMON_LOT_HEADER_SIZE);

			XixFsInitializeCommonLotHeader((PXIDISK_COMMON_LOT_HEADER)Buff,
											pVCB->VolumeLotSignature,
											((pFCB->FCBType == FCB_TYPE_FILE)?LOT_INFO_TYPE_FILE:LOT_INFO_TYPE_DIRECTORY),
											LOT_FLAG_BODY,
											AddrInfo->tmpAddressBuffer[i],
											pFCB->LotNumber,
											AddrInfo->PrevAddrBuffer[i],
											((i == (LotCount-1))?0:AddrInfo->tmpAddressBuffer[i+1]),
											GetLogicalStartAddrOfFile(AddrInfo->Index[i], pVCB->LotSize),
											XIDISK_DATA_LOT_SIZE,
											pVCB->LotSize - XIDISK_DATA_LOT_SIZE
											);

	

			RC = XixFsRawWriteLotHeader(pVCB->TargetDeviceObject, 
									pVCB->LotSize, 
									AddrInfo->tmpAddressBuffer[i], 
									Buff,
									XIDISK_COMMON_LOT_HEADER_SIZE,
									pVCB->SectorSize);
			

			if(!NT_SUCCESS(RC)){
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR XixFsAddNewLot:RawWriteLotHeader Status(0x%x)\n", RC));
				
				try_return(RC);

			}		
		}
		


		*AddedLotCount = LotCount;
		XifsdLockFcb(NULL, pFCB);
		pFCB->RealAllocationSize += ((pVCB->LotSize- XIDISK_DATA_LOT_SIZE)*LotCount);
		if(pFCB->FCBType == FCB_TYPE_FILE){
			pFCB->AllocationSize.QuadPart = pFCB->RealAllocationSize;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_ALLOC_SIZE);
		}
		
		XifsdUnlockFcb(NULL, pFCB);
/*
			= GetAddressOfFileData(LOT_FLAG_BODY, pVCB->LotSize, (RequestStatIndex + LotCount))
										+ (pVCB->LotSize- XIDISK_DATA_LOT_SIZE);
*/
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL, 
			("Changed Size pFCB(%I64d)  pFCB->RealAllocationSize(%I64d)\n", pFCB->LotNumber, pFCB->RealAllocationSize));		


		

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("Modified flag pFCB(%I64d) pFCB->FCBFlags(0x%x)\n", pFCB->LotNumber, pFCB->FCBFlags));


		bGetLot = FALSE;
	}finally{
		ExReleaseResourceLite(&(pFCB->AddrResource));

		if(AbnormalTermination() || !NT_SUCCESS(RC)){
			if(bGetLot = TRUE){
				for(i =0; i<LotCount; i++){
					
					if(AddrInfo->tmpAddressBuffer[i] != -1){
						
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Release Lot pFCB(%I64d) LotAddress(%I64d)\n", pFCB->LotNumber, AddrInfo->tmpAddressBuffer[i]));
						
						XixFsFreeVCBLot(pVCB, AddrInfo->tmpAddressBuffer[i]);


					}else{
						break;
					}
				}				
			}
		}

		ExFreePool(Buff);
		ExFreePool(AddrInfo);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit XixFsAddNewLot Status(0x%x)\n", RC));	
	return RC;
}


NTSTATUS
XixFsCheckEOFAndAllocLotFileInfo(
	IN PXIFS_FCB 	pFCB,
	IN uint64		RequestedEOF,
	IN BOOLEAN		Wait
	)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter XixFsCheckEOFAndAllocLotFileInfo\n"));

	pFCB->FileSize.QuadPart = RequestedEOF;
	XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);

	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit XixFsCheckEOFAndAllocLotFileInfo\n"));
	return STATUS_SUCCESS;
}



