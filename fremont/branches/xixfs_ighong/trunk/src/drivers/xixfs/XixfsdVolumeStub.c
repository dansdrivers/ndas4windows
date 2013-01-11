#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"





NTSTATUS
XixFsdInitializeBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx,
	IN PXIFS_VCB	pVCB,
	IN uint64 UsedBitmapIndex,
	IN uint64 UnusedBitmapIndex,
	IN uint64 CheckOutBitmapIndex
);

NTSTATUS
XixFsdCleanupBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdReadDirmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdWriteDirmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdReadFreemapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdWriteFreemapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdReadCheckoutmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdWriteCheckoutmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdInvalidateDirtyBitMap(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdInvalidateFreeBitMap(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdInvalidateCheckOutBitMap(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
SetCheckOutLotMap(
	IN	PXIFS_LOT_MAP	FreeLotMap,
	IN	PXIFS_LOT_MAP	CheckOutLotMap,
	IN	int32 			HostCount
);

NTSTATUS
XixFsdInitializeRecordContext(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx,
	IN PXIFS_VCB pVCB,
	IN uint8 * HOSTSIGNATURE
);

NTSTATUS
XixFsdCleanupRecordContext(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
);

NTSTATUS
XixFsdLookUpInitializeRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
);

NTSTATUS
XixFsdGetNextRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
);

NTSTATUS
XixFsdSetNextRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
);

NTSTATUS
XixFsdSetHostRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
);

NTSTATUS
XixFsdCheckFileDirFromBitMapContext(
	IN PXIFS_VCB VCB,
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

NTSTATUS
XixFsdCheckAndInvalidateHost(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx,
	OUT PBOOLEAN				IsLockHead
);






#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdCheckVolume)
#pragma alloc_text(PAGE, XixFsdGetSuperBlockInformation)
#pragma alloc_text(PAGE, XixFsdInitializeBitmapContext)
#pragma alloc_text(PAGE, XixFsdCleanupBitmapContext)
#pragma alloc_text(PAGE, XixFsdReadDirmapFromBitmapContext)
#pragma alloc_text(PAGE, XixFsdWriteDirmapFromBitmapContext)
#pragma alloc_text(PAGE, XixFsdReadFreemapFromBitmapContext)
#pragma alloc_text(PAGE, XixFsdWriteFreemapFromBitmapContext)
#pragma alloc_text(PAGE, XixFsdReadCheckoutmapFromBitmapContext)
#pragma alloc_text(PAGE, XixFsdWriteCheckoutmapFromBitmapContext)
#pragma alloc_text(PAGE, XixFsdInvalidateDirtyBitMap)
#pragma alloc_text(PAGE, XixFsdInvalidateFreeBitMap)
#pragma alloc_text(PAGE, XixFsdInvalidateCheckOutBitMap)
#pragma alloc_text(PAGE, SetCheckOutLotMap)
#pragma alloc_text(PAGE, XixFsdInitializeRecordContext)
#pragma alloc_text(PAGE, XixFsdCleanupRecordContext)
#pragma alloc_text(PAGE, XixFsdLookUpInitializeRecord)
#pragma alloc_text(PAGE, XixFsdGetNextRecord)
#pragma alloc_text(PAGE, XixFsdSetNextRecord)
#pragma alloc_text(PAGE, XixFsdSetHostRecord)
#pragma alloc_text(PAGE, XixFsdCheckFileDirFromBitMapContext)
#pragma alloc_text(PAGE, XixFsdCheckAndInvalidateHost)
#pragma alloc_text(PAGE, XixFsdRegisterHost)
#pragma alloc_text(PAGE, XixFsdDeRegisterHost)
#pragma alloc_text(PAGE, XixFsdGetMoreCheckOutLotMap)
#pragma alloc_text(PAGE, XixFsdUpdateMetaData)
#endif








NTSTATUS
XixFsdCheckVolume(
	IN PDEVICE_OBJECT	TargetDevice,
	IN uint32			SectorSize,
	IN uint8			*DiskID
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PUCHAR					buffer = NULL;
	LARGE_INTEGER			offset;
	PXIDISK_VOLUME_LOT 		VolumeLot = NULL;
	uint32					blockSize = 0;
	
	PAGED_CODE();

	ASSERT(TargetDevice);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdCheckVolume .\n"));

	blockSize = XIDISK_VOLUME_LOT_SIZE;
	buffer = (PUCHAR) ExAllocatePool(NonPagedPool, blockSize);

	if(!buffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Not Alloc buffer .\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	try{
		RC = XixFsRawReadVolumeLotHeader(
						TargetDevice,
						buffer,
						blockSize,
						SectorSize
						);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  XixFsRawReadVolumeHeader .\n"));
			try_return(RC);
		}
		
		VolumeLot = (PXIDISK_VOLUME_LOT)buffer;
		if(VolumeLot->LotHeader.LotInfo.Type  != LOT_INFO_TYPE_VOLUME)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x)  Is Not LOT_INFO_TYPE_VOLUME .\n", 
				VolumeLot->LotHeader.LotInfo.Type));
			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);
		}

		if(VolumeLot->VolInfo.VolumeSignature != XIFS_VOLUME_SIGNATURE)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail (0x%x)  Is XIFS_VOLUME_SIGNATURE .\n", 
				VolumeLot->VolInfo.VolumeSignature));
			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);	
		}

		if((VolumeLot->VolInfo.XifsVesion > XIFS_CURRENT_VERSION))
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x)  Is XIFS_CURRENT_VERSION .\n",
				VolumeLot->VolInfo.XifsVesion ));
			RC = STATUS_UNRECOGNIZED_VOLUME;
			try_return(RC);		
		}

		// Changed by ILGU HONG
		//	RtlCopyMemory(DiskID, VolumeLot->VolInfo.DiskId, 6);
		RtlCopyMemory(DiskID, VolumeLot->VolInfo.DiskId, 16);
		RC = STATUS_SUCCESS;


	}finally{
		ExFreePool(buffer);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Status(0x%x) XixFsdCheckVolume .\n", RC));
	return RC;
}


NTSTATUS
XixFsdGetSuperBlockInformation(
	IN PXIFS_VCB VCB)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PUCHAR				buffer = NULL;
	LARGE_INTEGER		offset;
	PDEVICE_OBJECT 		TargetDevice = NULL;
	PXIDISK_VOLUME_LOT 	VolumeLot = NULL;
	PXDISK_VOLUME_INFO	VolInfo = NULL;
	uint32				blockSize = 0;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdGetSuperBlockInformation .\n"));

	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);

	blockSize = XIDISK_VOLUME_LOT_SIZE;
	
	buffer = (PUCHAR) ExAllocatePool(NonPagedPool, blockSize);
	
	if(!buffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Not Alloc buffer .\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(buffer, blockSize);


	try{
		RC = XixFsRawReadVolumeLotHeader(
						TargetDevice,
						buffer,
						blockSize,
						VCB->SectorSize
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  XixFsdGetSuperBlockInformation .\n"));
			try_return(RC);
		}

		VolumeLot = (PXIDISK_VOLUME_LOT)buffer;

		VolInfo = &(VolumeLot->VolInfo);
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("VolInfo HostRegLotMap %I64d:RootLotMap %I64d: LotSize: %ld : TotalLotNumber %I64d .\n",
				VolInfo->HostRegLotMapIndex, VolInfo->RootDirectoryLotIndex, VolInfo->LotSize, VolInfo->NumLots));

		VCB->HostRegLotMapIndex = VolInfo->HostRegLotMapIndex;
		VCB->RootDirectoryLotIndex = VolInfo->RootDirectoryLotIndex;
		VCB->NumLots = VolInfo->NumLots;
		VCB->LotSize = VolInfo->LotSize;
		VCB->VolumeLotSignature = VolInfo->LotSignature;

		// Changed by ILGU HONG
		//	RtlCopyMemory(VCB->DiskId, VolInfo->DiskId,6);
		RtlCopyMemory(VCB->DiskId, VolInfo->DiskId,16);

		VCB->VolCreationTime = VolInfo->VolCreationTime;
		VCB->VolSerialNumber = VolInfo->VolSerialNumber;
		VCB->VolLabel.Length = (uint16)VolInfo->VolLabelLength;
		VCB->VolLabel.MaximumLength = (uint16)SECTORALIGNSIZE_512(VolInfo->VolLabelLength);
		
		if(VolInfo->VolLabelLength != 0){
			VCB->VolLabel.Buffer = (PWCHAR) ExAllocatePool(NonPagedPool, SECTORALIGNSIZE_512(VolInfo->VolLabelLength));
			if(!VCB->VolLabel.Buffer){
				RC = STATUS_INSUFFICIENT_RESOURCES;
				try_return(RC);
			}
			RtlCopyMemory(VCB->VolLabel.Buffer, VolInfo->VolLabel, VCB->VolLabel.Length);
		}
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("VCB HostRegLotMap %I64d:RootLotMap %I64d: LotSize: %ld : TotalLotNumber %I64d .\n",
				VCB->HostRegLotMapIndex, VCB->RootDirectoryLotIndex, VCB->LotSize, VCB->NumLots));

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
			("VCB Volume Signature (0x%x).\n",
				VCB->VolumeLotSignature));


	}finally{
		ExFreePool(buffer);
		if(!NT_SUCCESS(RC)){
			if(VCB->VolLabel.Buffer) {
				ExFreePool(VCB->VolLabel.Buffer);
				VCB->VolLabel.Length = VCB->VolLabel.MaximumLength = 0;
			}
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit Statue(0x%x) XixFsdGetSuperBlockInformation .\n", RC));
	return RC;
}





NTSTATUS
XixFsdInitializeBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx,
	IN PXIFS_VCB	pVCB,
	IN uint64 UsedBitmapIndex,
	IN uint64 UnusedBitmapIndex,
	IN uint64 CheckOutBitmapIndex
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	uint32 size;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdInitializeBitmapContext.\n"));		

	try{
		RtlZeroMemory(BitmapEmulCtx, sizeof(XIFS_BITMAP_EMUL_CONTEXT));
		BitmapEmulCtx->VCB = pVCB;
		BitmapEmulCtx->UsedBitmapIndex = UsedBitmapIndex;
		BitmapEmulCtx->UnusedBitmapIndex = UnusedBitmapIndex;
		BitmapEmulCtx->CheckOutBitmapIndex = CheckOutBitmapIndex;	
		BitmapEmulCtx->BitMapBytes =(uint32) ((pVCB->NumLots + 7)/8);


		
		size = SECTOR_ALIGNED_SIZE(pVCB->SectorSize, sizeof(XIFS_LOT_MAP)) + (uint32) ((pVCB->NumLots + 7)/8);
		size = SECTOR_ALIGNED_SIZE(pVCB->SectorSize, size);


		BitmapEmulCtx->BitmapLotHeader = ExAllocatePoolWithTag(NonPagedPool,XIDISK_MAP_LOT_SIZE, TAG_BUFFER);
		if(!BitmapEmulCtx->BitmapLotHeader){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);	
		}


		RtlZeroMemory(BitmapEmulCtx->BitmapLotHeader, XIDISK_MAP_LOT_SIZE);

		BitmapEmulCtx->UsedBitmap = ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);
		if(!BitmapEmulCtx->UsedBitmap){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);	
		}

		RtlZeroMemory(BitmapEmulCtx->UsedBitmap, size);


		BitmapEmulCtx->UnusedBitmap = ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);
		if(!BitmapEmulCtx->UnusedBitmap){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);			
		}

		RtlZeroMemory(BitmapEmulCtx->UnusedBitmap, size);

		BitmapEmulCtx->CheckOutBitmap = ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);
		if(!BitmapEmulCtx->CheckOutBitmap){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);			
		}

		RtlZeroMemory(BitmapEmulCtx->CheckOutBitmap, size);


		
	}finally{

		if(!NT_SUCCESS(RC)){
			if(BitmapEmulCtx->BitmapLotHeader){
				ExFreePool(BitmapEmulCtx->BitmapLotHeader);
				BitmapEmulCtx->BitmapLotHeader = NULL;
			}

			if(BitmapEmulCtx->UsedBitmap){
				ExFreePool(BitmapEmulCtx->UsedBitmap);
				BitmapEmulCtx->UsedBitmap = NULL;
			}

			if(BitmapEmulCtx->UnusedBitmap){
				ExFreePool(BitmapEmulCtx->UnusedBitmap);
				BitmapEmulCtx->UnusedBitmap = NULL;
			}

			if(BitmapEmulCtx->CheckOutBitmap){
				ExFreePool(BitmapEmulCtx->CheckOutBitmap);
				BitmapEmulCtx->CheckOutBitmap = NULL;
			}
		}

	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdInitializeBitmapContext Statue(0x%x).\n", RC));		

	return STATUS_SUCCESS;
}


NTSTATUS
XixFsdCleanupBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdCleanupBitmapContext.\n"));
	
	try{

		if(BitmapEmulCtx->BitmapLotHeader){
			ExFreePool(BitmapEmulCtx->BitmapLotHeader);
			BitmapEmulCtx->BitmapLotHeader = NULL;
		}

		if(BitmapEmulCtx->UsedBitmap){
			ExFreePool(BitmapEmulCtx->UsedBitmap);
			BitmapEmulCtx->UsedBitmap = NULL;
		}

		if(BitmapEmulCtx->UnusedBitmap){
			ExFreePool(BitmapEmulCtx->UnusedBitmap);
			BitmapEmulCtx->UnusedBitmap = NULL;
		}

		if(BitmapEmulCtx->CheckOutBitmap){
			ExFreePool(BitmapEmulCtx->CheckOutBitmap);
			BitmapEmulCtx->CheckOutBitmap = NULL;
		}
	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdCleanupBitmapContext.\n"));

	return STATUS_SUCCESS;
}




NTSTATUS
XixFsdReadDirmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdReadDirmapFromBitmapContext.\n"));

	return XixFsReadBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->UsedBitmapIndex, 
			(PXIFS_LOT_MAP)BitmapEmulCtx->UsedBitmap, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader);
}


NTSTATUS
XixFsdWriteDirmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdWriteDirmapFromBitmapContext.\n"));

	return XixFsWriteBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->UsedBitmapIndex, 
			(PXIFS_LOT_MAP)BitmapEmulCtx->UsedBitmap, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader,
			FALSE);
}


NTSTATUS
XixFsdReadFreemapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdReadFreemapFromBitmapContext.\n"));

	return XixFsReadBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->UnusedBitmapIndex, 
			(PXIFS_LOT_MAP)BitmapEmulCtx->UnusedBitmap, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader);
}


NTSTATUS
XixFsdWriteFreemapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdWriteFreemapFromBitmapContext.\n"));


	return XixFsWriteBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->UnusedBitmapIndex, 
			(PXIFS_LOT_MAP)BitmapEmulCtx->UnusedBitmap, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader,
			FALSE);
}


NTSTATUS
XixFsdReadCheckoutmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdReadCheckoutmapFromBitmapContext.\n"));

	return XixFsReadBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->CheckOutBitmapIndex, 
			(PXIFS_LOT_MAP)BitmapEmulCtx->CheckOutBitmap, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader);
}


NTSTATUS
XixFsdWriteCheckoutmapFromBitmapContext(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdWriteCheckoutmapFromBitmapContext.\n"));

	return XixFsWriteBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->CheckOutBitmapIndex, 
			(PXIFS_LOT_MAP)BitmapEmulCtx->CheckOutBitmap, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader,
			FALSE);
}


NTSTATUS
XixFsdInvalidateDirtyBitMap(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdInvalidateDirtyBitMap.\n"));

	return XixFsInvalidateLotBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->UsedBitmapIndex, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader
			);
}


NTSTATUS
XixFsdInvalidateFreeBitMap(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdInvalidateFreeBitMap.\n"));

	return XixFsInvalidateLotBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->UnusedBitmapIndex, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader
			);
}


NTSTATUS
XixFsdInvalidateCheckOutBitMap(
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdInvalidateCheckOutBitMap.\n"));

	return XixFsInvalidateLotBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			BitmapEmulCtx->CheckOutBitmapIndex, 
			(PXIDISK_MAP_LOT)BitmapEmulCtx->BitmapLotHeader
			);
}



NTSTATUS
SetCheckOutLotMap(
	IN	PXIFS_LOT_MAP	FreeLotMap,
	IN	PXIFS_LOT_MAP	CheckOutLotMap,
	IN	int32 			HostCount
)
{
	NTSTATUS RC = STATUS_UNSUCCESSFUL;
	
	uint64 FreeLotCount = 0;
	uint64 RequestCount = 0;
	uint64 ModIndicator = 0;
	uint64 AllocatedCount = 0;
	uint32 i = 0;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter SetCheckOutLotMap \n"));

	if(HostCount > XIFS_DEFAULT_USER){
		ModIndicator = HostCount;
	}else {
		ModIndicator = XIFS_DEFAULT_USER;
	}



	FreeLotCount = XixFsfindSetBitMapCount(FreeLotMap->NumLots, FreeLotMap->Data);
	//RequestCount = ((FreeLotCount / ModIndicator) * 9) /10;  
	
	RequestCount = 10;

	CheckOutLotMap->StartIndex = XixFsAllocLotMapFromFreeLotMap(
									FreeLotMap->NumLots, 
									RequestCount, 
									FreeLotMap->Data, 
									CheckOutLotMap->Data,
									&AllocatedCount);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%I64d) ,Num Bytes(%ld)\n",
			FreeLotMap->NumLots, FreeLotMap->BitMapBytes));

	/*
	for(i = 0; i< (uint32)(FreeLotMap->BitMapBytes/8); i++)
	{
		DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
			("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
			i*8,
			FreeLotMap->Data[i*8 + 0],FreeLotMap->Data[i*8 + 1],
			FreeLotMap->Data[i*8 + 2],FreeLotMap->Data[i*8 + 3],
			FreeLotMap->Data[i*8 + 4],FreeLotMap->Data[i*8 + 5],
			FreeLotMap->Data[i*8 + 6],FreeLotMap->Data[i*8 + 7]
			));
	}
	*/

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%I64d) ,FreeLotMapCount(%I64d), RequestCount(%I64d), StartIndex(%I64d)\n",
			FreeLotMap->NumLots, FreeLotCount, RequestCount,CheckOutLotMap->StartIndex ));


	if(AllocatedCount < XIFS_DEFAULT_HOST_LOT_COUNT){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Exit Error SetCheckOutLotMap \n"));

		DbgPrint("NO AVAILABE DISK SPACE!!!!\n");
		return STATUS_UNSUCCESSFUL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit  SetCheckOutLotMap \n"));
	return STATUS_SUCCESS;
}



NTSTATUS
XixFsdInitializeRecordContext(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx,
	IN PXIFS_VCB pVCB,
	IN uint8 * HOSTSIGNATURE
)
{
	NTSTATUS RC = STATUS_SUCCESS;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdInitializeRecordContext .\n"));

	try{
		RtlZeroMemory(RecordEmulCtx, sizeof(XIFS_RECORD_EMUL_CONTEXT));	
		RtlCopyMemory(RecordEmulCtx->HostSignature, HOSTSIGNATURE, 16);
		
		RecordEmulCtx->RecordInfo = ExAllocatePoolWithTag(NonPagedPool, XIDISK_HOST_INFO_SIZE, TAG_BUFFER);
		
		if(!RecordEmulCtx->RecordInfo){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}

		RtlZeroMemory(RecordEmulCtx->RecordInfo, XIDISK_HOST_INFO_SIZE);
		
		RecordEmulCtx->RecordEntry = ExAllocatePoolWithTag(NonPagedPool, XIDISK_HOST_RECORD_SIZE, TAG_BUFFER);
		if(!RecordEmulCtx->RecordEntry){
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}	
		
		RtlZeroMemory(RecordEmulCtx->RecordEntry, XIDISK_HOST_RECORD_SIZE);

		RecordEmulCtx->VCB = pVCB;

	}finally{
		if(!NT_SUCCESS(RC)){
			if(RecordEmulCtx->RecordInfo){
				ExFreePool(RecordEmulCtx->RecordInfo);
			}

			if(RecordEmulCtx->RecordEntry){
				ExFreePool(RecordEmulCtx->RecordEntry);
			}
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit  XixFsdInitializeRecordContext Status(0x%x) .\n", RC));

	return RC;
	
}


NTSTATUS
XixFsdCleanupRecordContext(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdCleanupRecordContext .\n"));


	try{
		if(RecordEmulCtx->RecordInfo){
			ExFreePool(RecordEmulCtx->RecordInfo);
			RecordEmulCtx->RecordInfo = NULL;
		}

		if(RecordEmulCtx->RecordEntry){
			ExFreePool(RecordEmulCtx->RecordEntry);
			RecordEmulCtx->RecordEntry = NULL;
		}
	}finally{
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdCleanupRecordContext .\n"));

	return STATUS_SUCCESS;
}

NTSTATUS
XixFsdLookUpInitializeRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdLookUpInitializeRecord .\n"));
	
	ASSERT_VCB(RecordEmulCtx->VCB);

	RecordEmulCtx->RecordIndex = 0;
	RecordEmulCtx->RecordSearchIndex = -1;
	try{

		RtlZeroMemory(RecordEmulCtx->RecordEntry, XIDISK_HOST_INFO_SIZE);

		RC = XixFsRawReadRegisterHostInfo(
							RecordEmulCtx->VCB->TargetDeviceObject,
							RecordEmulCtx->VCB->LotSize,
							RecordEmulCtx->VCB->HostRegLotMapIndex,
							RecordEmulCtx->RecordInfo,
							XIDISK_HOST_INFO_SIZE,
							RecordEmulCtx->VCB->SectorSize
							);

	}finally{
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL , 
				("FAIL  XixFsdLookUpInitializeRecord Get:RecordInfo .\n"));			
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdLookUpInitializeRecord  Status(0x%x).\n", RC));
	
	return RC;
}




NTSTATUS
XixFsdGetNextRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PXIDISK_HOST_INFO 		RecordInfo = NULL;
	PXIDISK_HOST_RECORD		RecordEntry = NULL;
	LARGE_INTEGER	offset;
	uint32			blockSize = 0;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XifsdGetNextRecord .\n"));	


	RecordInfo = (PXIDISK_HOST_INFO)RecordEmulCtx->RecordInfo;



	try{
		
		RecordEmulCtx->RecordIndex = 
			RecordEmulCtx->RecordSearchIndex = (uint32)XixFsfindSetBitFromMap(
															XIFSD_MAX_HOST, 
															RecordEmulCtx->RecordSearchIndex, 
															RecordInfo->RegisterMap
															);
		
		if(RecordEmulCtx->RecordIndex == XIFSD_MAX_HOST) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Can't found record.\n"));
			RC = STATUS_NOT_FOUND;
			try_return(RC) ;
		}

		RtlZeroMemory(RecordEmulCtx->RecordEntry, XIDISK_HOST_RECORD_SIZE);

		RC = XixFsRawReadRegisterRecord(
						RecordEmulCtx->VCB->TargetDeviceObject,
						RecordEmulCtx->VCB->LotSize,
						RecordEmulCtx->RecordIndex,
						RecordEmulCtx->VCB->HostRegLotMapIndex,
						RecordEmulCtx->RecordEntry,
						XIDISK_HOST_RECORD_SIZE,
						RecordEmulCtx->VCB->SectorSize
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail XifsdRawReadBlockDevice Status(0x%x).\n", RC));
			RC = STATUS_NOT_FOUND;
			try_return(RC) ;
		}

	}finally{
			
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XifsdGetNextRecord Status(0x%x).\n", RC));	
	return RC;
}



NTSTATUS
XixFsdSetNextRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	int32 index = 0;
	PXIDISK_HOST_INFO 		RecordInfo = NULL;
	LARGE_INTEGER	offset;
	uint32			blockSize = 0;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdSetNextRecord .\n"));	


	RecordInfo = (PXIDISK_HOST_INFO)RecordEmulCtx->RecordInfo;


	try{
		

		RC = XixFsRawWriteRegisterRecord(
						RecordEmulCtx->VCB->TargetDeviceObject,
						RecordEmulCtx->VCB->LotSize,
						RecordEmulCtx->RecordIndex,
						RecordEmulCtx->VCB->HostRegLotMapIndex,
						RecordEmulCtx->RecordEntry,
						XIDISK_HOST_RECORD_SIZE,
						RecordEmulCtx->VCB->SectorSize
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail XixFsRawWriteRegisterRecord Status(0x%x).\n", RC));
			RC = STATUS_NOT_FOUND;
			try_return(RC) ;
		}


		RC = XixFsRawWriteRegisterHostInfo(
						RecordEmulCtx->VCB->TargetDeviceObject,
						RecordEmulCtx->VCB->LotSize,
						RecordEmulCtx->VCB->HostRegLotMapIndex,
						RecordEmulCtx->RecordInfo,
						XIDISK_HOST_INFO_SIZE,
						RecordEmulCtx->VCB->SectorSize
						);

	}finally{
			
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdSetNextRecord Status(0x%x).\n", RC));	
	return RC;
}



NTSTATUS
XixFsdSetHostRecord(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	XIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx;
	PXIFS_LOT_MAP		HostFreeMap = NULL;
	PXIFS_LOT_MAP		HostDirtyMap = NULL;
	// Added by ILGU HONG 2006 06 12
	PXIFS_LOT_MAP		VolumeFreeMap = NULL;
	// Added by ILGU HONG 2006 06 12 End
	PXIFS_LOT_MAP		ptempLotMap = NULL;
	PXIFS_LOT_MAP 		TempFreeLotMap = NULL;
	PXIDISK_HOST_INFO	HostInfo = NULL;
	PXIDISK_HOST_RECORD HostRecord = NULL; 
	PXIFS_VCB			VCB = NULL;
	uint32				size = 0;	
	uint32				RecodeIndex = 0;
	uint32				Step = 0;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdSetHostRecord .\n"));	


	VCB = RecordEmulCtx->VCB;
	ASSERT_VCB(VCB);

	size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, sizeof(XIFS_LOT_MAP)) + (uint32) ((VCB->NumLots + 7)/8);
	size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, size);


	try{

		// Zero Bitmap Context
		RtlZeroMemory(&BitmapEmulCtx, sizeof(XIFS_BITMAP_EMUL_CONTEXT));

		HostInfo = (PXIDISK_HOST_INFO)RecordEmulCtx->RecordInfo;

		RecodeIndex = (uint32)XixFsfindFreeBitFromMap(XIFSD_MAX_HOST, -1, HostInfo->RegisterMap);
		if(RecodeIndex == XIFSD_MAX_HOST){
			RC = STATUS_NOT_FOUND;
			try_return(RC);
		}
		
	


		HostDirtyMap = (PXIFS_LOT_MAP)ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);
		if(!HostDirtyMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Not Alloc buffer .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}

		RtlZeroMemory((char *)HostDirtyMap, size);
		
		HostFreeMap = (PXIFS_LOT_MAP)ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);
		if(!HostFreeMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Not Alloc buffer .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}
		
		RtlZeroMemory((char *)HostFreeMap, size);

	
		//	Added by ILGU HONG 2006 06 12
		VolumeFreeMap = (PXIFS_LOT_MAP)ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);
		if(!VolumeFreeMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Not Alloc buffer .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}
		
		RtlZeroMemory((char *)VolumeFreeMap, size);
		//	Added by ILGU HONG 2006 06 12 End


		
		TempFreeLotMap = (PXIFS_LOT_MAP)ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);

		if(!TempFreeLotMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Allocate TempFreeLotMap .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}


		RtlZeroMemory(TempFreeLotMap, size);




		RC = XixFsdInitializeBitmapContext(&BitmapEmulCtx,
										RecordEmulCtx->VCB,
										HostInfo->UsedLotMapIndex,
										HostInfo->UnusedLotMapIndex,
										HostInfo->CheckOutLotMapIndex);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
			try_return(RC);
		}


		RC = XixFsdReadFreemapFromBitmapContext(&BitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		RC = XixFsdReadCheckoutmapFromBitmapContext(&BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		
		// Get Real FreeMap without CheckOut
		XixFsEORMap((PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap, (PXIFS_LOT_MAP)BitmapEmulCtx.CheckOutBitmap);
		

		// Initialize HostFree/HostDirty
		ptempLotMap = (PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap;
		TempFreeLotMap->BitMapBytes = HostDirtyMap->BitMapBytes = HostFreeMap->BitMapBytes = ptempLotMap->BitMapBytes;
		TempFreeLotMap->MapType = HostDirtyMap->MapType = HostFreeMap->MapType = ptempLotMap->MapType;
		TempFreeLotMap->NumLots = HostDirtyMap->NumLots = HostFreeMap->NumLots = ptempLotMap->NumLots;	

		RC = SetCheckOutLotMap((PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap, TempFreeLotMap, RecodeIndex + 1);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  SetCheckOutLotMap Status (0x%x) .\n", RC));
			try_return(RC);
		}

		HostDirtyMap->StartIndex = HostFreeMap->StartIndex;


		//Update CheckOut LotMap --> include Host aligned bit map
		XixFsORMap((PXIFS_LOT_MAP)BitmapEmulCtx.CheckOutBitmap, TempFreeLotMap);
		
		RC = XixFsdWriteCheckoutmapFromBitmapContext(&BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		XixFsORMap(HostFreeMap, TempFreeLotMap);
		
		
		Step = 1;	
		
		VCB->HostFreeLotMap = HostFreeMap;
		VCB->HostDirtyLotMap = HostDirtyMap;

		//	Added by ILGU HONG	2006 06 12
		VolumeFreeMap->BitMapBytes = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->BitMapBytes;
		VolumeFreeMap->MapType = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->MapType;
		VolumeFreeMap->NumLots = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->NumLots;
		VolumeFreeMap->StartIndex = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->StartIndex;
		RtlCopyMemory(VolumeFreeMap->Data, ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->Data, VolumeFreeMap->BitMapBytes);
		VCB->VolumeFreeMap	= VolumeFreeMap;
		//	Added by ILGU HONG	2006 06 12 End
			

		HostFreeMap = NULL;
		HostDirtyMap = NULL;

		// Alloc Host specified map
		VCB->HostUsedLotMapIndex = XixFsAllocVCBLot(VCB);
		VCB->HostUnUsedLotMapIndex = XixFsAllocVCBLot(VCB);
		VCB->HostCheckOutLotMapIndex = XixFsAllocVCBLot(VCB);


		// Write Host Check Out
		RC = XixFsWriteBitMapWithBuffer(
							VCB,
							VCB->HostCheckOutLotMapIndex, 
							TempFreeLotMap, 
							(PXIDISK_MAP_LOT)BitmapEmulCtx.BitmapLotHeader,
							TRUE);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  WriteBitMap for Checkout (0x%x) .\n", RC));
			try_return(RC);			
		}		



		RC = XixFsWriteBitMapWithBuffer(VCB,
							VCB->HostUnUsedLotMapIndex, 
							VCB->HostFreeLotMap, 
							(PXIDISK_MAP_LOT)BitmapEmulCtx.BitmapLotHeader,
							TRUE);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  WriteBitMap for Free (0x%x) .\n", RC));
			try_return(RC);			
		}		


		RC = XixFsWriteBitMapWithBuffer(VCB,
							VCB->HostUsedLotMapIndex, 
							VCB->HostDirtyLotMap, 
							(PXIDISK_MAP_LOT)BitmapEmulCtx.BitmapLotHeader,
							TRUE);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  WriteBitMap for Free (0x%x) .\n", RC));
			try_return(RC);			
		}		


		Step =2;




		// Set Host Record
		VCB->HostRecordIndex = RecodeIndex;	
		
		setBitToMap(RecodeIndex, HostInfo->RegisterMap);
		HostInfo->NumHost ++;

		// UpdateHostInfo
		HostRecord = (PXIDISK_HOST_RECORD)RecordEmulCtx->RecordEntry;
		RtlZeroMemory(HostRecord, XIDISK_HOST_RECORD_SIZE);
		HostRecord->HostCheckOutLotMapIndex = VCB->HostCheckOutLotMapIndex;
		HostRecord->HostUnusedLotMapIndex = VCB->HostUnUsedLotMapIndex;
		HostRecord->HostUsedLotMapIndex = VCB->HostUsedLotMapIndex;
		HostRecord->HostMountTime = XixGetSystemTime().QuadPart;	
		RtlCopyMemory(HostRecord->HostSignature, RecordEmulCtx->HostSignature, 16);
		HostRecord->HostState = HOST_MOUNT;			

		
		RecordEmulCtx->RecordIndex = RecodeIndex;
		

		RC = XixFsdSetNextRecord(RecordEmulCtx);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x) XifsdSetNextRecord .\n", RC));
			try_return(RC);
		}		

	}finally{
		if(!NT_SUCCESS(RC)){
			if(HostDirtyMap){
				ExFreePool(HostDirtyMap);
			}

			if(HostFreeMap){
				ExFreePool(HostFreeMap);
			}
		}

		if(TempFreeLotMap) ExFreePool(TempFreeLotMap);

		XixFsdCleanupBitmapContext(&BitmapEmulCtx);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdSetHostRecord Status(0x%x).\n", RC));	

	return RC;
}



NTSTATUS
XixFsdCheckFileDirFromBitMapContext(
	IN PXIFS_VCB VCB,
	IN PXIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIDISK_COMMON_LOT_HEADER	pCommonLotHeader = NULL;
	PXIFS_LOT_MAP		HostFreeMap = NULL;
	PXIFS_LOT_MAP		HostDirtyMap = NULL;
	PXIFS_LOT_MAP		HostCheckOutMap = NULL;
	int64				SearchIndex = -1;
	PUCHAR				Buff = NULL;
	uint32				Size = 0;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdCheckFileDirFromBitMapContext .\n"));	

	

	HostFreeMap = (PXIFS_LOT_MAP)BitmapEmulCtx->UnusedBitmap;
	HostDirtyMap = (PXIFS_LOT_MAP)BitmapEmulCtx->UsedBitmap;
	HostCheckOutMap = (PXIFS_LOT_MAP)BitmapEmulCtx->CheckOutBitmap;

	if(VCB->SectorSize > XIDISK_FILE_HEADER_LOT_SIZE){
		Size = VCB->SectorSize;
	}else{
		Size = XIDISK_FILE_HEADER_LOT_SIZE;
	}

	Buff = ExAllocatePoolWithTag(NonPagedPool, Size, TAG_BUFFER);
	
	if(!Buff){
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
			("Fail Allocate Buff\n", SearchIndex));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(Buff, XIDISK_FILE_HEADER_LOT_SIZE);

	pCommonLotHeader = (PXIDISK_COMMON_LOT_HEADER)Buff;

	try{
		

		// Check Start with Host Dirty map
		while(1)
		{
			SearchIndex = XixFsfindSetBitFromMap(VCB->NumLots, SearchIndex, HostDirtyMap->Data);

			if((uint64)SearchIndex >= VCB->NumLots){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Fail findSetBitFromMap End of Lot.\n"));
				break;
			}
			
			if(SearchIndex < 24){
				continue;
			}


			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Searched Index (%I64d).\n", SearchIndex));

			// Read Lot map info
			RC = XixFsRawReadLotHeader(
						VCB->TargetDeviceObject, 
						VCB->LotSize, 
						SearchIndex, 
						(uint8 *)pCommonLotHeader,
						XIDISK_COMMON_LOT_HEADER_SIZE,
						VCB->SectorSize
						);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Fail(0x%x) RawReadLot .\n", RC));
				try_return(RC);
			}					
			// Check Lot map info
			


			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Lot (%I64d) Type(0x%x) Flags(0x%x).\n", 
					SearchIndex, pCommonLotHeader->LotInfo.Type, pCommonLotHeader->LotInfo.Flags));

			if( (pCommonLotHeader->LotInfo.Type & (LOT_INFO_TYPE_INVALID|LOT_INFO_TYPE_FILE|LOT_INFO_TYPE_DIRECTORY))
				&& (pCommonLotHeader->LotInfo.LotSignature == VCB->VolumeLotSignature))
			{

				


				if((pCommonLotHeader->LotInfo.Type == LOT_INFO_TYPE_INVALID) 
					|| (pCommonLotHeader->LotInfo.Flags == LOT_FLAG_INVALID ))
				{



						DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
							("!!!!! Find Null bitmap information Lot %I64d !!!!!!\n", SearchIndex));

						RtlZeroMemory(pCommonLotHeader, sizeof(XIDISK_COMMON_LOT_HEADER));

						XixFsInitializeCommonLotHeader(
								pCommonLotHeader,
								VCB->VolumeLotSignature,
								LOT_INFO_TYPE_INVALID,
								LOT_FLAG_INVALID,
								SearchIndex,
								SearchIndex,
								0,
								0,
								0,
								0,
								0
								);	


						RC = XixFsRawWriteLotHeader(
									VCB->TargetDeviceObject, 
									VCB->LotSize, 
									SearchIndex, 
									(uint8 *)pCommonLotHeader,
									XIDISK_COMMON_LOT_HEADER_SIZE,
									VCB->SectorSize);

						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
								("Fail(0x%x) RawReadLot .\n", RC));
							try_return(RC);
						}

						// Update Lot map
						clearBitToMap(SearchIndex, HostDirtyMap->Data);
						setBitToMap(SearchIndex, HostFreeMap->Data);						

						
				}else if((pCommonLotHeader->LotInfo.Type == LOT_INFO_TYPE_FILE) 
							&& (pCommonLotHeader->LotInfo.Flags == LOT_FLAG_BEGIN)){

						PXIDISK_FILE_INFO	pFileHeader;
						LARGE_INTEGER		Offset;
						uint32				SecNum = 0;
						uint32				i = 0;
						uint32				AddrStartSectorIndex = 0;
						uint64				AdditionalAddrLot = 0;
						BOOLEAN				bDelete = FALSE;
						BOOLEAN				bStop = FALSE;
						uint64				* Addr = NULL;

						DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
							("!!!!! Find a Valid Information Lot %I64d !!!!!!\n", SearchIndex));
						
					
						RC = XixFsAuxLotLock(
									TRUE,
									VCB,
									VCB->TargetDeviceObject,
									SearchIndex,
									FALSE,
									FALSE
									);
					

						if(!NT_SUCCESS(RC)){
							continue;
						}


						RC = XixFsRawReadLotAndFileHeader(
									VCB->TargetDeviceObject, 
									VCB->LotSize, 
									SearchIndex, 
									(uint8 *)pCommonLotHeader,
									XIDISK_FILE_HEADER_LOT_SIZE,
									VCB->SectorSize
									);

						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
								("Fail(0x%x) RawReadLot .\n", RC));
							try_return(RC);
						}					



						Offset.QuadPart = 0;
						SecNum = 0;
						i = 0;

						pFileHeader = (PXIDISK_FILE_INFO)((PUCHAR)pCommonLotHeader + XIDISK_LOT_INFO_SIZE);

						if(pFileHeader->State == XIFS_FD_STATE_DELETED){
							bDelete = TRUE;
						}

						AdditionalAddrLot = pFileHeader->AddressMapIndex;

						do{
								
							RtlZeroMemory(Buff, VCB->SectorSize);
							

							RC = XixFsRawReadAddressOfFile(
											VCB->TargetDeviceObject,
											VCB->LotSize,
											SearchIndex,
											AdditionalAddrLot,
											Buff,
											VCB->SectorSize, 
											&AddrStartSectorIndex,
											SecNum,
											VCB->SectorSize);
											
		
							if(!NT_SUCCESS(RC)){
								DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
									("Error Read Addr Lot Status(0x%x)\n", RC));
								RC = STATUS_UNSUCCESSFUL;
								bStop = TRUE;
								break;
							}

							Addr = (uint64 *)Buff;

							for(i = 0; i<64; i++){
								if((Addr[i] != 0) && (Addr[i] < VCB->NumLots)){
									

									if(IsSetBit(Addr[i] , HostCheckOutMap->Data)){

										DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
												("Update Addr Info Lo(%I64d)\n", Addr[i]));

										if(bDelete == TRUE){
											clearBitToMap(Addr[i], HostDirtyMap->Data);
											setBitToMap(Addr[i], HostFreeMap->Data);	
										}else{
											clearBitToMap(Addr[i], HostFreeMap->Data);
											setBitToMap(Addr[i], HostDirtyMap->Data);											
										}
									}



								}else{
									bStop = TRUE;
									break;
								}
							}
							
							SecNum ++;

						}while(bStop == FALSE);

						XixFsAuxLotUnLock(
									VCB,
									VCB->TargetDeviceObject,
									SearchIndex
									);
				}
			}else{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Failed Lot (%I64d) Type(0x%x) Flags(0x%x).\n", 
					SearchIndex, pCommonLotHeader->LotInfo.Type, pCommonLotHeader->LotInfo.Flags));


				if(pCommonLotHeader->LotInfo.LotSignature != VCB->VolumeLotSignature){

						DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
							("!!!!! Find a inaccurate bitmap information Lot %I64d !!!!!!\n", SearchIndex));

						RtlZeroMemory(pCommonLotHeader, sizeof(XIDISK_COMMON_LOT_HEADER));

						XixFsInitializeCommonLotHeader(
								pCommonLotHeader,
								VCB->VolumeLotSignature,
								LOT_INFO_TYPE_INVALID,
								LOT_FLAG_INVALID,
								SearchIndex,
								SearchIndex,
								0,
								0,
								0,
								0,
								0
								);	


						RC = XixFsRawWriteLotHeader(
								VCB->TargetDeviceObject, 
								VCB->LotSize, 
								SearchIndex, 
								(uint8 *)pCommonLotHeader,
								XIDISK_COMMON_LOT_HEADER_SIZE,
								VCB->SectorSize
								);
						
						if(!NT_SUCCESS(RC)){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
								("Fail(0x%x) RawReadLot .\n", RC));
							try_return(RC);
						}
						
						clearBitToMap(SearchIndex, HostDirtyMap->Data);
						setBitToMap(SearchIndex, HostFreeMap->Data);	

				}
			}

		}

		SearchIndex = -1;

		// Check Start with Host Free map
		while(1)
		{
			SearchIndex = XixFsfindSetBitFromMap(VCB->NumLots, SearchIndex, HostFreeMap->Data);

			if((uint64)SearchIndex >= VCB->NumLots){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Fail findSetBitFromMap End of Lot.\n"));
				break;
			}
			
			if(SearchIndex < 24){
				continue;
			}


			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Searched Index (%I64d).\n", SearchIndex));


			// Read Lot map info
			RC = XixFsRawReadLotHeader(
					VCB->TargetDeviceObject, 
					VCB->LotSize, 
					SearchIndex, 
					(uint8 *)pCommonLotHeader,
					XIDISK_COMMON_LOT_HEADER_SIZE,
					VCB->SectorSize
					);

			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
					("Fail(0x%x) RawReadLot .\n", RC));
				try_return(RC);
			}					
			// Check Lot map info
			
			if( pCommonLotHeader->LotInfo.Type & (LOT_INFO_TYPE_INVALID|LOT_INFO_TYPE_FILE|LOT_INFO_TYPE_DIRECTORY))
			{
				if(	(pCommonLotHeader->LotInfo.Type != LOT_INFO_TYPE_INVALID) 
					&& (pCommonLotHeader->LotInfo.Flags != LOT_FLAG_INVALID )
					&& (pCommonLotHeader->LotInfo.LotSignature == VCB->VolumeLotSignature)) 
				{

						DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
							("!!!!! Find a Used bitmap information Lot %I64d !!!!!!\n", SearchIndex));

						// Update Lot map
						clearBitToMap(SearchIndex, HostFreeMap->Data);
						setBitToMap(SearchIndex, HostDirtyMap->Data);						
				}
			}

		}

	}finally{
		
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XixFsdCheckFileDirFromBitMapContext Status(0x%x).\n", RC));	

	return STATUS_SUCCESS;
}




NTSTATUS
XixFsdCheckAndInvalidateHost(
	IN PXIFS_RECORD_EMUL_CONTEXT RecordEmulCtx,
	OUT PBOOLEAN				IsLockHead
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	XIFS_BITMAP_EMUL_CONTEXT BitmapEmulCtx;
	XIFS_BITMAP_EMUL_CONTEXT DiskBitmapEmulCtx;


	PXIDISK_HOST_INFO	HostInfo = NULL;
	PXIDISK_HOST_RECORD HostRecord = NULL; 
	PXIFS_VCB			VCB = NULL;
	uint32				size = 0;	
	uint32				RecordIndex = 0;
	uint32				Step = 0;
	BOOLEAN				bAcqLotLock = TRUE;



	PAGED_CODE();
	
	VCB = RecordEmulCtx->VCB;
	ASSERT_VCB(VCB);

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdCheckAndInvalidateHost .\n"));

	try{
		

		// Zero Bit map context;
		RtlZeroMemory(&BitmapEmulCtx, sizeof(XIFS_BITMAP_EMUL_CONTEXT));
		RtlZeroMemory(&DiskBitmapEmulCtx, sizeof(XIFS_BITMAP_EMUL_CONTEXT));


		// Save Record Index
		RecordIndex = RecordEmulCtx->RecordIndex;

		// Initialize Host Bitmap Context
		HostRecord = (PXIDISK_HOST_RECORD)RecordEmulCtx->RecordEntry;
		
		

		RC = XixFsdInitializeBitmapContext(&BitmapEmulCtx,
										RecordEmulCtx->VCB,
										HostRecord->HostUsedLotMapIndex,
										HostRecord->HostUnusedLotMapIndex,
										HostRecord->HostCheckOutLotMapIndex);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
			try_return(RC);
		}





		// Read Host Bitmap information

		RC = XixFsdReadDirmapFromBitmapContext(&BitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
	
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Before Validate Host Dirty Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}



		RC = XixFsdReadFreemapFromBitmapContext(&BitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Before Validate Host Free Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


		RC = XixFsdReadCheckoutmapFromBitmapContext(&BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(BitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Before Validate Host CheckOut Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}





		RC = XixFsAuxLotUnLock(
			VCB,
			VCB->TargetDeviceObject,
			VCB->HostRegLotMapIndex
			);	
	

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Reg Unlock (0x%x) .\n", RC));
			try_return(RC);
		}



		bAcqLotLock = FALSE;

		// Check Host Dirty map information and make new Host Free Bitmap 

		RC = XixFsdCheckFileDirFromBitMapContext(VCB, &BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdCheckFileDirFromBitMapContext (0x%x) .\n", RC));
			try_return(RC);
		}


		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Validate Host Dirty Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(BitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Validate Host Free Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		



		RC = XixFsAuxLotLock(
			TRUE,
			VCB,
			VCB->TargetDeviceObject,
			VCB->HostRegLotMapIndex, 
			TRUE,
			TRUE
			);

		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Reg Lock (0x%x) .\n", RC));
			try_return(RC);
		}
		
		

		bAcqLotLock = TRUE;

	
		RC = XixFsdLookUpInitializeRecord(RecordEmulCtx);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdLookUpInitializeRecord .\n", RC));
			try_return(RC);
		}
		


		// Initialize Disk Bitmap Context
		HostInfo = (PXIDISK_HOST_INFO)RecordEmulCtx->RecordInfo;
		RC = XixFsdInitializeBitmapContext(&DiskBitmapEmulCtx,
										RecordEmulCtx->VCB,
										HostInfo->UsedLotMapIndex,
										HostInfo->UnusedLotMapIndex,
										HostInfo->CheckOutLotMapIndex);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
			try_return(RC);
		}

		

		// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
		
		RC = XixFsdReadDirmapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
	
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Befor Validate Disk Dirty Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		




		RC = XixFsdReadFreemapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Befor Validate Disk Free Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		


		RC = XixFsdReadCheckoutmapFromBitmapContext(&DiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Befor Validate Disk CheckOut Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		



		clearBitToMap(HostRecord->HostUsedLotMapIndex, ((PXIFS_LOT_MAP)BitmapEmulCtx.UsedBitmap)->Data);
		setBitToMap(HostRecord->HostUsedLotMapIndex, ((PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap)->Data);
		clearBitToMap(HostRecord->HostUnusedLotMapIndex, ((PXIFS_LOT_MAP)BitmapEmulCtx.UsedBitmap)->Data);
		setBitToMap(HostRecord->HostUnusedLotMapIndex, ((PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap)->Data);
		clearBitToMap(HostRecord->HostCheckOutLotMapIndex, ((PXIFS_LOT_MAP)BitmapEmulCtx.UsedBitmap)->Data);
		setBitToMap(HostRecord->HostCheckOutLotMapIndex, ((PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap)->Data);

		

		XixFsEORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.CheckOutBitmap, (PXIFS_LOT_MAP)BitmapEmulCtx.CheckOutBitmap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Check Out bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


	
		XixFsORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UsedBitmap, (PXIFS_LOT_MAP)BitmapEmulCtx.UsedBitmap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Dirty bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}

		XixFsEORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UnusedBitmap, (PXIFS_LOT_MAP)BitmapEmulCtx.UsedBitmap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Free bitmap 1 \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}



		XixFsORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UnusedBitmap, (PXIFS_LOT_MAP)BitmapEmulCtx.UnusedBitmap);
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Free bitmap 2 \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}





		// Invalidate Host Bitmap

		XixFsdInvalidateDirtyBitMap(&BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInvalidateDirtyBitMap (0x%x) .\n", RC));
			try_return(RC);			
		}


		XixFsdInvalidateFreeBitMap(&BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInvalidateFreeBitMap (0x%x) .\n", RC));
			try_return(RC);			
		}


		XixFsdInvalidateCheckOutBitMap(&BitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInvalidateCheckOutBitMap (0x%x) .\n", RC));
			try_return(RC);			
		}


		// Update Information
		RC = XixFsdWriteDirmapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
		

		RC = XixFsdWriteFreemapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		RC = XixFsdWriteCheckoutmapFromBitmapContext(&DiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		
		//Update Record
		clearBitToMap(RecordIndex, HostInfo->RegisterMap);
		HostInfo->NumHost --;

		// UpdateHostInfo
		HostRecord = (PXIDISK_HOST_RECORD)RecordEmulCtx->RecordEntry;
		RtlZeroMemory(HostRecord, XIDISK_HOST_RECORD_SIZE);
		HostRecord->HostCheckOutLotMapIndex = 0;
		HostRecord->HostUnusedLotMapIndex = 0;
		HostRecord->HostUsedLotMapIndex = 0;
		HostRecord->HostMountTime = XixGetSystemTime().QuadPart;	
		RtlCopyMemory(HostRecord->HostSignature, RecordEmulCtx->HostSignature, 16);
		HostRecord->HostState = HOST_UMOUNT;			

		RecordEmulCtx->RecordIndex = RecordIndex;	

		RC = XixFsdSetNextRecord(RecordEmulCtx);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x) XifsdSetNextRecord .\n", RC));
			try_return(RC);
		}		

	}finally{
		XixFsdCleanupBitmapContext(&BitmapEmulCtx);
		XixFsdCleanupBitmapContext(&DiskBitmapEmulCtx);
	}


	*IsLockHead = bAcqLotLock;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XifsdCheckAndInvalidateHost .\n"));
	return STATUS_SUCCESS;
}








NTSTATUS
XixFsdRegisterHost(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB VCB)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	XIFS_RECORD_EMUL_CONTEXT	RecordEmulCtx;
	PDEVICE_OBJECT				TargetDevice = NULL;
	PXIDISK_HOST_INFO 			HostInfo = NULL;
	PXIDISK_HOST_RECORD			HostRecord = NULL;	
	BOOLEAN						bLockHeld = FALSE;
	PAGED_CODE();
	
	ASSERT(VCB);
	ASSERT(pIrpContext);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdRegisterHost .\n"));

	TargetDevice = VCB->TargetDeviceObject;


	ASSERT(TargetDevice);
	RC = XixFsAuxLotLock(
		TRUE,
		VCB,
		TargetDevice,
		VCB->HostRegLotMapIndex,
		TRUE,
		TRUE
		);

	

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XixFsLotLock .\n", RC));
		return RC;
	}
	

	bLockHeld = TRUE;

	try{
		RC = XixFsdInitializeRecordContext(&RecordEmulCtx, VCB, VCB->HostId);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdInitializeRecordContext .\n", RC));
			try_return(RC);
		}
			
		RC = XixFsdLookUpInitializeRecord(&RecordEmulCtx);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdLookUpInitializeRecord .\n", RC));
			try_return(RC);
		}

		// Set BitMap Lot address information
		HostInfo = (PXIDISK_HOST_INFO)RecordEmulCtx.RecordInfo;
		VCB->AllocatedLotMapIndex = HostInfo->UsedLotMapIndex;
		VCB->FreeLotMapIndex = HostInfo->UnusedLotMapIndex;
		VCB->CheckOutLotMapIndex = HostInfo->CheckOutLotMapIndex;		
		

		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
		("HostInfo AllocIndex (%I64d) FreeIndex (%I64d) CheckIndex(%I64d) : NumHost(%I64d).\n", 
			HostInfo->UsedLotMapIndex,
			HostInfo->UnusedLotMapIndex,
			HostInfo->CheckOutLotMapIndex,
			HostInfo->NumHost
			));		


		//// Check Record for Error recovery
		if(HostInfo->NumHost !=0){			
			do{
				RC = XixFsdGetNextRecord(&RecordEmulCtx);
				if(NT_SUCCESS(RC)){

					HostRecord = (PXIDISK_HOST_RECORD)RecordEmulCtx.RecordEntry;
					
					if((RtlCompareMemory(HostRecord->HostSignature, VCB->HostId, 16) == 16)
						&& (HostRecord->HostState != HOST_UMOUNT ))
					{

						
						RC = XixFsdCheckAndInvalidateHost(&RecordEmulCtx, &bLockHeld);

						if(!NT_SUCCESS(RC)){
							//call check out routine
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
								("Fail(0x%x) XifsdCheckAndInvalidateHost .\n", RC));
							try_return(RC);
						}
						break;	
					}				
				}

			}while(NT_SUCCESS(RC));
		}
		
		RC = XixFsdSetHostRecord(&RecordEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdSetHostRecord .\n", RC));
			try_return(RC);
		}		
	}finally{
		
		XixFsdCleanupRecordContext(&RecordEmulCtx);
		if(bLockHeld == TRUE) {
			XixFsAuxLotUnLock(
				VCB,
				TargetDevice,
				VCB->HostRegLotMapIndex
				);	

			
		}

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XixFsdRegisterHost Status(0x%x).\n", RC));

	return RC;
}


NTSTATUS
XixFsdDeRegisterHost(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB VCB)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	XIFS_RECORD_EMUL_CONTEXT	RecordEmulCtx;
	XIFS_BITMAP_EMUL_CONTEXT	DiskBitmapEmulCtx;
	PDEVICE_OBJECT				TargetDevice = NULL;
	PXIDISK_HOST_INFO 			HostInfo = NULL;
	PXIDISK_HOST_RECORD			HostRecord = NULL;	
	PXIFS_LOT_MAP 				TempFreeLotMap = NULL;
	uint32						size = 0;
	//PAGED_CODE();
	
	ASSERT(VCB);
	ASSERT(pIrpContext);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdDeRegisterHost.\n"));

	TargetDevice = VCB->TargetDeviceObject;


	ASSERT(TargetDevice);
	RC = XixFsLotLock(
		TRUE,
		VCB,
		TargetDevice,
		VCB->HostRegLotMapIndex,
		&VCB->HostRegLotMapLockStatus,
		TRUE,
		TRUE
		);

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdLotLock .\n", RC));
		return RC;
	}

	
	try{


		// Zero Bit map context;
		RtlZeroMemory(&DiskBitmapEmulCtx, sizeof(XIFS_BITMAP_EMUL_CONTEXT));


		// Read Record Lot header Info
		RC = XixFsdInitializeRecordContext(&RecordEmulCtx, VCB, VCB->HostId);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdInitializeRecordContext .\n", RC));
			try_return(RC);
		}
		
		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, sizeof(XIFS_LOT_MAP)) + (uint32) ((VCB->NumLots + 7)/8);
		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, size);


		TempFreeLotMap = (PXIFS_LOT_MAP)ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);

		if(!TempFreeLotMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Allocate TempFreeLotMap .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}


		RtlZeroMemory(TempFreeLotMap, size);

		RC = XixFsdLookUpInitializeRecord(&RecordEmulCtx);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdLookUpInitializeRecord .\n", RC));
			try_return(RC);
		}

		// Set BitMap Lot address information
		HostInfo = (PXIDISK_HOST_INFO)RecordEmulCtx.RecordInfo;
		VCB->AllocatedLotMapIndex = HostInfo->UsedLotMapIndex;
		VCB->FreeLotMapIndex = HostInfo->UnusedLotMapIndex;
		VCB->CheckOutLotMapIndex = HostInfo->CheckOutLotMapIndex;



		// Read Disk Bitmap information
		RC = XixFsdInitializeBitmapContext(&DiskBitmapEmulCtx,
										VCB,
										HostInfo->UsedLotMapIndex,
										HostInfo->UnusedLotMapIndex,
										HostInfo->CheckOutLotMapIndex);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
			try_return(RC);
		}

		

		// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
		
		RC = XixFsdReadDirmapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Dirty Bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


		RC = XixFsdReadFreemapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Free bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


		RC = XixFsdReadCheckoutmapFromBitmapContext(&DiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Check Out bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}

		RC = XixFsReadBitMapWithBuffer(
						VCB, 
						VCB->HostCheckOutLotMapIndex, 
						TempFreeLotMap, 
						(PXIDISK_MAP_LOT)DiskBitmapEmulCtx.BitmapLotHeader
						);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Read Host Check Out Lotmap (0x%x) .\n", RC));
			try_return(RC);			
		}

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(TempFreeLotMap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Host Check Out bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}

	



		clearBitToMap(VCB->HostUnUsedLotMapIndex, VCB->HostDirtyLotMap->Data);
		setBitToMap(VCB->HostUnUsedLotMapIndex, VCB->HostFreeLotMap->Data);
		clearBitToMap(VCB->HostUsedLotMapIndex, VCB->HostDirtyLotMap->Data);
		setBitToMap(VCB->HostUsedLotMapIndex, VCB->HostFreeLotMap->Data);
		clearBitToMap(VCB->HostCheckOutLotMapIndex, VCB->HostDirtyLotMap->Data);
		setBitToMap(VCB->HostCheckOutLotMapIndex, VCB->HostFreeLotMap->Data);

		
		XixFsEORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.CheckOutBitmap, (PXIFS_LOT_MAP)TempFreeLotMap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Check Out bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


	
		XixFsORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UsedBitmap, (PXIFS_LOT_MAP)VCB->HostDirtyLotMap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Dirty bitmap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}

		XixFsEORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UnusedBitmap, (PXIFS_LOT_MAP)VCB->HostDirtyLotMap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Free bitmap 1 \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}



		XixFsORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UnusedBitmap, (PXIFS_LOT_MAP)VCB->HostFreeLotMap);
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Disk Free bitmap 2 \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}





		// Update Disk Information
		RC = XixFsdWriteDirmapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
		

		RC = XixFsdWriteFreemapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		RC = XixFsdWriteCheckoutmapFromBitmapContext(&DiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}



		// Invalidate Host Bitmap

		RC = XixFsInvalidateLotBitMapWithBuffer(
			VCB,
			VCB->HostUnUsedLotMapIndex, 
			(PXIDISK_MAP_LOT)DiskBitmapEmulCtx.BitmapLotHeader
			);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInvalidateBitMap HostFreeBitmap (0x%x) .\n", RC));
			try_return(RC);			
		}


		RC = XixFsInvalidateLotBitMapWithBuffer(
			VCB,
			VCB->HostUnUsedLotMapIndex, 
			(PXIDISK_MAP_LOT)DiskBitmapEmulCtx.BitmapLotHeader
			);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInvalidateBitMap HostDirtyBitmap (0x%x) .\n", RC));
			try_return(RC);			
		}



		RC = XixFsInvalidateLotBitMapWithBuffer(
			VCB,
			VCB->HostCheckOutLotMapIndex, 
			(PXIDISK_MAP_LOT)DiskBitmapEmulCtx.BitmapLotHeader
			);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInvalidateBitMap HostCheckOutLotMapIndex (0x%x) .\n", RC));
			try_return(RC);			
		}


		
		//Update Record
		clearBitToMap(VCB->HostRecordIndex, HostInfo->RegisterMap);
		HostInfo->NumHost --;

		// UpdateHostInfo
		HostRecord = (PXIDISK_HOST_RECORD)RecordEmulCtx.RecordEntry;
		RtlZeroMemory(HostRecord, XIDISK_HOST_RECORD_SIZE);
		HostRecord->HostCheckOutLotMapIndex = 0;
		HostRecord->HostUnusedLotMapIndex = 0;
		HostRecord->HostUsedLotMapIndex = 0;
		HostRecord->HostMountTime = XixGetSystemTime().QuadPart;	
		RtlCopyMemory(HostRecord->HostSignature, RecordEmulCtx.HostSignature, 16);
		HostRecord->HostState = HOST_UMOUNT;			

			

		RC = XixFsdSetNextRecord(&RecordEmulCtx);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x) XifsdSetNextRecord .\n", RC));
			try_return(RC);
		}		




	}finally{
		XixFsLotUnLock(
			VCB,
			TargetDevice,
			VCB->HostRegLotMapIndex,
			&VCB->HostRegLotMapLockStatus
			);	
		

		if(TempFreeLotMap) ExFreePool(TempFreeLotMap);
		
		XixFsdCleanupBitmapContext(&DiskBitmapEmulCtx);
		XixFsdCleanupRecordContext(&RecordEmulCtx);
		

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdDeRegisterHost Status (0x%x).\n", RC));

	return RC;
}


NTSTATUS
XixFsdGetMoreCheckOutLotMap(
	IN PXIFS_VCB VCB
)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	XIFS_BITMAP_EMUL_CONTEXT	DiskBitmapEmulCtx;
	PDEVICE_OBJECT				TargetDevice = NULL;
	PXIFS_LOT_MAP 				TempFreeLotMap = NULL;
	uint32						size = 0;

	//PAGED_CODE();
	
	ASSERT(VCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XifsdGetMoreCheckOutLotMap.\n"));

	TargetDevice = VCB->TargetDeviceObject;


	ASSERT(TargetDevice);
	RC = XixFsLotLock(
		TRUE,
		VCB,
		TargetDevice,
		VCB->HostRegLotMapIndex,
		&VCB->HostRegLotMapLockStatus,
		TRUE,
		TRUE
		);
		

	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) XifsdLotLock .\n", RC));
		return RC;
	}


	try{

		// Zero Bit map context;
		RtlZeroMemory(&DiskBitmapEmulCtx, sizeof(XIFS_BITMAP_EMUL_CONTEXT));


		// Read Disk Bitmap information
		RC = XixFsdInitializeBitmapContext(&DiskBitmapEmulCtx,
										VCB,
										VCB->HostCheckOutLotMapIndex,
										VCB->FreeLotMapIndex,
										VCB->CheckOutLotMapIndex);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
			try_return(RC);
		}
		
		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, sizeof(XIFS_LOT_MAP)) + (uint32) ((VCB->NumLots + 7)/8);
		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, size);



		size = SECTORALIGNSIZE_512(size);
		TempFreeLotMap = (PXIFS_LOT_MAP)ExAllocatePoolWithTag(NonPagedPool, size, TAG_BUFFER);

		if(!TempFreeLotMap){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  Allocate TempFreeLotMap .\n"));
			RC = STATUS_INSUFFICIENT_RESOURCES;
			try_return(RC);
		}

		RtlZeroMemory(TempFreeLotMap, size);

		// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
		
		RC = XixFsdReadDirmapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}
	
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Host CheckOut BitMap \n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		

		RC = XixFsdReadFreemapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		

		RC = XixFsdReadCheckoutmapFromBitmapContext(&DiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}

		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk CheckOut Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}	
		

		// Get Real FreeMap without CheckOut
		XixFsEORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UnusedBitmap, (PXIFS_LOT_MAP)DiskBitmapEmulCtx.CheckOutBitmap);
			
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Real free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		


		RC = SetCheckOutLotMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UnusedBitmap, TempFreeLotMap, VCB->HostRecordIndex + 1);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  SetCheckOutLotMap Status (0x%x) .\n", RC));
			try_return(RC);
		}
		
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(TempFreeLotMap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Allocated Bit Map\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		





		//Update Host CheckOutLotMap
		XixFsORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.UsedBitmap, TempFreeLotMap);

		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UsedBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Allocate Host Checkout Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}


		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(VCB->HostFreeLotMap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Before Allocate Host free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}
		

		//Update Host FreeLotMap
		XixFsORMap((PXIFS_LOT_MAP)VCB->HostFreeLotMap, TempFreeLotMap);


		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(VCB->HostFreeLotMap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Allocate Host free Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		
		

		//Update Disk CheckOut BitMap
		XixFsORMap((PXIFS_LOT_MAP)DiskBitmapEmulCtx.CheckOutBitmap, TempFreeLotMap);	
		
		
		{
			uint32 i = 0;
			UCHAR *	Data;
			Data = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.CheckOutBitmap))->Data;
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("After Allocate Disk CheckOut Bitmap\n"));
			for(i = 0; i<2; i++){
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
					i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
					Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
			}
		}		
		

		//	Added by ILGU HONG	2006 06 12
		VCB->VolumeFreeMap->BitMapBytes = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->BitMapBytes;
		VCB->VolumeFreeMap->MapType = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->MapType;
		VCB->VolumeFreeMap->NumLots = ((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->NumLots;
		RtlCopyMemory(VCB->VolumeFreeMap->Data, 
					((PXIFS_LOT_MAP)(DiskBitmapEmulCtx.UnusedBitmap))->Data, 
					VCB->VolumeFreeMap->BitMapBytes
					);
		//	Added by ILGU HONG	2006 06 12 End




		// Update Disk Information

		RC = XixFsdWriteCheckoutmapFromBitmapContext(&DiskBitmapEmulCtx);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}



		// Update Host Record Information
		RC = XixFsdWriteDirmapFromBitmapContext(&DiskBitmapEmulCtx);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL  XifsdReadDirmapFromBitmapContext (0x%x) .\n", RC));
			try_return(RC);			
		}


		RC = XixFsWriteBitMapWithBuffer(
				VCB, 
				VCB->HostUsedLotMapIndex, 
				VCB->HostDirtyLotMap, 
				(PXIDISK_MAP_LOT)DiskBitmapEmulCtx.BitmapLotHeader, 
				FALSE
				);

		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XifsdWriteBitMap Host Dirty map .\n", RC));
			try_return(RC);
		}


		RC = XixFsWriteBitMapWithBuffer(
				VCB, 
				VCB->HostUnUsedLotMapIndex, 
				VCB->HostFreeLotMap, 
				(PXIDISK_MAP_LOT)DiskBitmapEmulCtx.BitmapLotHeader, 
				FALSE
				);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XifsdWriteBitMap  Host Free map.\n", RC));
			try_return(RC);
		}

	}finally{
		XixFsLotUnLock(
			VCB,
			TargetDevice,
			VCB->HostRegLotMapIndex,
			&VCB->HostRegLotMapLockStatus
			);	


		XixFsdCleanupBitmapContext(&DiskBitmapEmulCtx);
		if(TempFreeLotMap){
			ExFreePool(TempFreeLotMap);
		}
		

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdGetMoreCheckOutLotMap Status (0x%x).\n", RC));

	return RC;
}



NTSTATUS
XixFsdUpdateMetaData(
	IN PXIFS_VCB VCB
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_MAP_LOT	BitmapLotHeader = NULL;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdUpdateMetaData .\n"));

	BitmapLotHeader = ExAllocatePoolWithTag(NonPagedPool,XIDISK_MAP_LOT_SIZE, TAG_BUFFER);
	if(!BitmapLotHeader){
		RC = STATUS_INSUFFICIENT_RESOURCES;
		return RC;	
	}

	RtlZeroMemory(BitmapLotHeader, XIDISK_MAP_LOT_SIZE);
	
	try{

		RC = XixFsWriteBitMapWithBuffer(VCB, VCB->HostUsedLotMapIndex, VCB->HostDirtyLotMap, BitmapLotHeader, FALSE);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XixFsWriteBitMapWithBuffer Host Dirty map .\n", RC));
			try_return(RC);
		}




		RC = XixFsWriteBitMapWithBuffer(VCB, VCB->HostUnUsedLotMapIndex, VCB->HostFreeLotMap, BitmapLotHeader, FALSE);
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail Status(0x%x) XixFsWriteBitMapWithBuffer  Host Free map.\n", RC));
			try_return(RC);
		}

	}finally{

		ExFreePool(BitmapLotHeader);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit Status(0x%x) XixFsdUpdateMetaData .\n", RC));

	return RC;

}