#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"
#include "XixFsDrv.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"


VOID
XixFsGetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN OUT PXIFS_IO_LOT_INFO 	pAddressInfo
);


VOID
XixFsSetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN 	PXIFS_IO_LOT_INFO 	pAddressInfo
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsInitializeCommonLotHeader)
#pragma alloc_text(PAGE, XixFsCheckLotInfo)
#pragma alloc_text(PAGE, XixFsCheckOutLotHeader)
#pragma alloc_text(PAGE, XixFsDumpFileLot)
#pragma alloc_text(PAGE, XixFsGetLotInfo)
#pragma alloc_text(PAGE, XixFsSetLotInfo)
#endif


VOID 
XixFsInitializeCommonLotHeader(
	IN PXIDISK_COMMON_LOT_HEADER 	LotHeader,
	IN uint32						LotSignature,
	IN uint32						LotType,
	IN uint32						LotFlag,
	IN int64						LotNumber,
	IN int64						BeginLotNumber,
	IN int64						PreviousLotNumber,
	IN int64 						NextLotNumber,
	IN int64						LogicalStartOffset,
	IN int32						StartDataOffset,
	IN int32						TotalDataSize
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsInitializeCommonLotHeader \n"));

	RtlZeroMemory(LotHeader, sizeof(XIDISK_COMMON_LOT_HEADER));
	LotHeader->Lock.LockState = XIDISK_LOCK_RELEASED;
	LotHeader->LotInfo.Type = LotType;
	LotHeader->LotInfo.Flags = LotFlag;
	LotHeader->LotInfo.BeginningLotIndex = BeginLotNumber;
	LotHeader->LotInfo.LotIndex = LotNumber;
	LotHeader->LotInfo.PreviousLotIndex = PreviousLotNumber;
	LotHeader->LotInfo.NextLotIndex = NextLotNumber;
	LotHeader->LotInfo.LogicalStartOffset = LogicalStartOffset;
	LotHeader->LotInfo.StartDataOffset = StartDataOffset;
	LotHeader->LotInfo.LotTotalDataSize = TotalDataSize;
	LotHeader->LotInfo.LotSignature = LotSignature;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsInitializeCommonLotHeader \n"));
}


NTSTATUS
XixFsCheckLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN	uint32				VolLotSignature,
	IN 	int64				LotNumber,
	IN 	uint32				Type,
	IN 	uint32				Flags,
	IN OUT	uint32			*Reason
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsCheckLotInfo \n"));

	*Reason = 0;
	
	if(pLotInfo->LotIndex!= LotNumber)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail XixFsCheckLotInfo is not same index(%I64d) request(%I64d).\n",pLotInfo->LotIndex, LotNumber));			
		*Reason = XI_CODE_LOT_INDEX_WARN;
		return STATUS_UNSUCCESSFUL;		
	}

	if(pLotInfo->Type != Type) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsCheckLotInfo is not same Type(%ld) request(%ld).\n",pLotInfo->Type, Type));			
		*Reason = XI_CODE_LOT_TYPE_WARN;
		return STATUS_UNSUCCESSFUL;	
	}
	
	if(pLotInfo->Flags != Flags){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsCheckLotInfo is not same Flags(%ld) request(%ld).\n",pLotInfo->Flags, Flags));
		*Reason = XI_CODE_LOT_FLAG_WARN;
		return STATUS_UNSUCCESSFUL;
	}


	if(pLotInfo->LotSignature != VolLotSignature){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail XixFsCheckLotInfo is not same Signature(%ld) request(%ld).\n",pLotInfo->LotSignature, VolLotSignature));
		*Reason = XI_CODE_LOT_SIGNATURE_WARN;
		return STATUS_UNSUCCESSFUL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFSDCheckLotInfo \n"));
	return STATUS_SUCCESS;
}


NTSTATUS
XixFsCheckOutLotHeader(
	IN PDEVICE_OBJECT	TargetDevice,
	IN uint32			VolLotSignature,
	IN uint64			LotIndex,
	IN uint32			LotSize,
	IN uint32			LotType,
	IN uint32			LotFlag,
	IN uint32			SectorSize
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	PXIDISK_LOT_INFO			pLotInfo = NULL;
	uint32				size = 0;
	uint32				Reason = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFSDCheckOutLotHeader\n"));

	size = XIDISK_COMMON_LOT_HEADER_SIZE;
	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)ExAllocatePool(NonPagedPool, size);
	if(!pLotHeader){
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(pLotHeader, size);

	try{
		RC = XixFsRawReadLotHeader(TargetDevice,LotSize,LotIndex,(uint8 *)pLotInfo, size, SectorSize);
		
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}
		
		pLotInfo = &(pLotHeader->LotInfo);


		RC = XixFsCheckLotInfo(pLotInfo,VolLotSignature,LotIndex,LotType,LotFlag,&Reason);
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}
	}finally{
		ExFreePool(pLotHeader);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFSDCheckOutLotHeader\n"));

	return RC;
}


static VOID
XixFsGetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN OUT PXIFS_IO_LOT_INFO 	pAddressInfo
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter ReadLotInfo \n"));

	pAddressInfo->Type = pLotInfo->Type;
	pAddressInfo->Flags = pLotInfo->Flags;
	pAddressInfo->BeginningLotIndex = pLotInfo->BeginningLotIndex;
	pAddressInfo->LotIndex = pLotInfo->LotIndex;
	pAddressInfo->NextLotIndex = pLotInfo->NextLotIndex;
	pAddressInfo->PreviousLotIndex = pLotInfo->PreviousLotIndex;
	pAddressInfo->LogicalStartOffset = pLotInfo->LogicalStartOffset;
	pAddressInfo->StartDataOffset = pLotInfo->StartDataOffset;
	pAddressInfo->LotTotalDataSize = pLotInfo->LotTotalDataSize;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit ReadLotInfo \n"));
}


static VOID
XixFsSetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN 	PXIFS_IO_LOT_INFO 	pAddressInfo
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter WriteLotInfo \n"));

	pLotInfo->Type = pAddressInfo->Type ;
	pLotInfo->Flags = pAddressInfo->Flags ;
	pLotInfo->BeginningLotIndex = pAddressInfo->BeginningLotIndex ;
	pLotInfo->LotIndex = pAddressInfo->LotIndex ;
	pLotInfo->NextLotIndex = pAddressInfo->NextLotIndex ;
	pLotInfo->PreviousLotIndex = pAddressInfo->PreviousLotIndex ;
	pLotInfo->LogicalStartOffset = pAddressInfo->LogicalStartOffset ;
	pLotInfo->StartDataOffset = pAddressInfo->StartDataOffset ;
	pLotInfo->LotTotalDataSize = pAddressInfo->LotTotalDataSize ;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit WriteLotInfo \n"));

}



NTSTATUS
XixFsDumpFileLot(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32  VolLotSignature,
	IN	uint32	LotSize,
	IN	uint64	StartIndex,
	IN	uint32	Type,
	IN	uint32	SectorSize
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	XIFS_IO_LOT_INFO 	AddressInfo;
	PXIDISK_COMMON_LOT_HEADER  pLotHeader = NULL;
	uint32				Reason = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsDumpFileLot \n"));
	
	RtlZeroMemory(&AddressInfo, sizeof(XIFS_IO_LOT_INFO));
	
	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)ExAllocatePool(NonPagedPool, XIDISK_COMMON_LOT_HEADER_SIZE);
	if(!pLotHeader){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	
	try{
		RC = XixFsRawReadLotHeader(TargetDevice,LotSize,StartIndex,(uint8 *)pLotHeader, XIDISK_COMMON_LOT_HEADER_SIZE, SectorSize);
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}
		
		RC = XixFsCheckLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), VolLotSignature, StartIndex,Type,LOT_FLAG_BEGIN,&Reason);	
		if(!NT_SUCCESS(RC)){
			//Debug Print reason
			try_return(RC);
		}
		
		RtlZeroMemory(&AddressInfo, XIDISK_LOT_INFO_SIZE);
		XixFsGetLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), &AddressInfo);
			//Debug Print Lot Information

		while( AddressInfo.NextLotIndex != 0){
			
			RtlZeroMemory(&AddressInfo, sizeof(XIFS_IO_LOT_INFO));
			RC = XixFsRawReadLotHeader(TargetDevice,LotSize,AddressInfo.NextLotIndex,(uint8 *)pLotHeader, XIDISK_COMMON_LOT_HEADER_SIZE, SectorSize);
			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}

			
			RC = XixFsCheckLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), VolLotSignature, AddressInfo.NextLotIndex,Type,LOT_FLAG_BODY,&Reason);	
			if(!NT_SUCCESS(RC)){
				//Debug Print reason
				try_return(RC);
			}
			RtlZeroMemory(&AddressInfo, XIDISK_LOT_INFO_SIZE);
			XixFsGetLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), &AddressInfo);
				//Debug Print Lot Information		
		}
	}finally{
		if(pLotHeader) ExFreePool(pLotHeader);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsDumpFileLot \n"));
	return RC;
	
}