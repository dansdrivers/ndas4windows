#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"
#include "XixFsComProto.h"
#include "XixFsGlobalData.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"


//#pragma alloc_text(PAGE, XixFsSetCheckOutLotMap)


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsfindSetBitFromMap)
#pragma alloc_text(PAGE, XixFsfindFreeBitFromMap)
#pragma alloc_text(PAGE, XixFsfindSetBitMapCount)
#pragma alloc_text(PAGE, XixFsAllocLotMapFromFreeLotMap)
#pragma alloc_text(PAGE, XixFsReadBitMapWithBuffer)
#pragma alloc_text(PAGE, XixFsWriteBitMapWithBuffer)
#pragma alloc_text(PAGE, XixFsInvalidateLotBitMapWithBuffer)
#pragma alloc_text(PAGE, XixFsReadAndAllocBitMap)
#pragma alloc_text(PAGE, XixFsWriteBitMap)
#pragma alloc_text(PAGE, XixFsORMap)
#pragma alloc_text(PAGE, XixFsEORMap)
#pragma alloc_text(PAGE, XixFsAllocVCBLot)
#pragma alloc_text(PAGE, XixFsFreeVCBLot)
#endif





int64 
XixFsfindSetBitFromMap(
	IN		int64 bitmap_count, 
	IN		int64 bitmap_hint, 
	IN OUT	volatile void * Mpa
)
{
	int64 i;
	int64 hint = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsfindSetBitFromMap \n"));

	bitmap_hint++;
	hint = bitmap_hint;

	ASSERT(hint >= 0);
	for(i = hint;  i< bitmap_count; i++)
	{
		if(test_bit(i, Mpa) )
		{
			return i;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsfindSetBitFromMap \n"));
	return i;
}

uint64 
XixFsfindFreeBitFromMap(
	IN		uint64 bitmap_count, 
	IN		uint64 bitmap_hint, 
	IN OUT	volatile void * Mpa
)
{
	uint64 i;
	uint64 hint = bitmap_hint;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsfindFreeBitFromMap \n"));

	if(hint >0){
		hint ++;
	}
	for(i = hint; i< bitmap_count; i++)
	{
		if(!test_bit(i, Mpa) )
		{
			return i;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsfindFreeBitFromMap \n"));
	return i;
}



uint64 
XixFsfindSetBitMapCount(
	IN		uint64 bitmap_count, 
	IN OUT	volatile void *LotMapData
)
{
	uint64 i;
	uint32 count = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsfindSetBitMapCount \n"));

	for(i = 0; i< bitmap_count; i++)
	{
		if(test_bit(i, LotMapData) )
		{
			count ++;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsfindSetBitMapCount \n"));
	return count;
}

uint64
XixFsAllocLotMapFromFreeLotMap(
	IN	uint64 bitmap_count, 
	IN	uint64 request,  
	IN	volatile void * FreeLotMapData, 
	IN	volatile void * CheckOutLotMapData,
	IN	uint64 * 	AllocatedCount
)
{
	uint64 i;
	uint64 alloc = 0;
	uint64 StartIndex = XIFS_RESERVED_LOT_SIZE;
	uint64 ReturnIndex = 0;
	BOOLEAN		InitSet =FALSE;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsAllocLotMapFromFreeLotMap \n"));

	for( i = StartIndex;  i < bitmap_count; i++)
	{
		if(alloc >= request) break;
		
		if(test_bit(i, FreeLotMapData))
		{
			if(InitSet == FALSE){
				ReturnIndex = i;
				InitSet = TRUE;
			}
			set_bit(i,CheckOutLotMapData);
			alloc++;
		}
	}


	if(alloc < request) {
		for( i = XIFS_RESERVED_LOT_SIZE;  i < bitmap_count; i++)
		{
			if(alloc >= request) break;
			
			if(test_bit(i, FreeLotMapData))
			{
				if(InitSet == FALSE){
					ReturnIndex = i;
					InitSet = TRUE;
				}
				set_bit(i,CheckOutLotMapData);
				alloc++;
			}
		}
	}

	*AllocatedCount= alloc;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsAllocLotMapFromFreeLotMap \n"));

	return ReturnIndex;
}




/*
 *	Function must be done within waitable thread context
 */

NTSTATUS
XixFsReadBitMapWithBuffer(
	IN PXIFS_VCB VCB,
	IN uint64 BitMapLotNumber,
	IN PXIFS_LOT_MAP	Bitmap,	
	IN PXIDISK_MAP_LOT BitmapLotHeader
)
{
	NTSTATUS				RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER			offset;
	PDEVICE_OBJECT			TargetDevice = NULL;
	LARGE_INTEGER			Offset;
	int32					size;
	PXIDISK_LOT_MAP_INFO    pMapInfo = NULL;

	uint32					Reason = 0;
	uint32					i = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsReadBitMap \n"));


	ASSERT(VCB);
	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);


	try{


		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, sizeof(XIFS_LOT_MAP)) + (uint32) ((VCB->NumLots + 7)/8);
		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, size);


		RtlZeroMemory((PCHAR)Bitmap, size);
		RtlZeroMemory((PCHAR)BitmapLotHeader, XIDISK_MAP_LOT_SIZE);


		RC = XixFsRawReadLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						BitMapLotNumber,
						(uint8 *)BitmapLotHeader,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}

		RC = XixFsCheckLotInfo(
				&BitmapLotHeader->LotHeader.LotInfo,
				VCB->VolumeLotSignature,
				BitMapLotNumber,
				LOT_INFO_TYPE_BITMAP,
				LOT_FLAG_BEGIN,
				&Reason
				);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Check LotInfo .\n", RC));	
			try_return(RC);
		}

		pMapInfo = &BitmapLotHeader->Map;
		Bitmap->BitMapBytes = pMapInfo->BitMapBytes;
		Bitmap->NumLots = pMapInfo->NumLots;
		Bitmap->MapType = pMapInfo->MapType;

		offset.QuadPart= GetAddressOfBitmapData(
					VCB->LotSize,
					BitMapLotNumber
					);	


		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, (uint32) ((VCB->NumLots + 7)/8));



		RC = XixFsRawAlignSafeReadBlockDevice(
				TargetDevice,
				VCB->SectorSize,
				&offset, 
				size, 
				(uint8 *)Bitmap->Data);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMap Data .\n", RC));	
			try_return(RC);
		}

		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
				("NumLots(%I64d) ,Num Bytes(%ld)\n",
				Bitmap->NumLots, Bitmap->BitMapBytes));

		/*
		for(i = 0; i< (uint32)(pLotMap->BitMapBytes/8); i++)
		{
			DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
				("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
				i*8,
				pLotMap->Data[i*8 + 0],pLotMap->Data[i*8 + 1],
				pLotMap->Data[i*8 + 2],pLotMap->Data[i*8 + 3],
				pLotMap->Data[i*8 + 4],pLotMap->Data[i*8 + 5],
				pLotMap->Data[i*8 + 6],pLotMap->Data[i*8 + 7]
				));
		}
		*/

	}finally{
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Exit XifsdReadBitMap  Status(0x%x)\n", RC));
	
	return RC;
}




NTSTATUS
XixFsWriteBitMapWithBuffer(
	IN PXIFS_VCB VCB,
	IN uint64 BitMapLotNumber,
	IN PXIFS_LOT_MAP Bitmap,
	IN PXIDISK_MAP_LOT BitmapLotHeader,
	IN BOOLEAN		Initialize
)
{
	NTSTATUS				RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER			offset;
	PDEVICE_OBJECT			TargetDevice = NULL;
	int32					size;
	PXIDISK_LOT_MAP_INFO    pMapInfo = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XifsdWriteBitMap \n"));

	ASSERT(VCB);
	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);

	try{


		RtlZeroMemory((PCHAR)BitmapLotHeader, XIDISK_MAP_LOT_SIZE);


		RC = XixFsRawReadLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						BitMapLotNumber,
						(uint8 *)BitmapLotHeader,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}

	

		if(Initialize){

			XixFsInitializeCommonLotHeader(
					(PXIDISK_COMMON_LOT_HEADER)BitmapLotHeader,
					VCB->VolumeLotSignature,
					LOT_INFO_TYPE_BITMAP,
					LOT_FLAG_BEGIN,
					BitMapLotNumber,
					BitMapLotNumber,
					0,
					0,
					0,
					sizeof(XIDISK_MAP_LOT),
					VCB->LotSize - sizeof(XIDISK_MAP_LOT)
					);	
			
		}
		
		pMapInfo = &BitmapLotHeader->Map;
		pMapInfo->BitMapBytes = Bitmap->BitMapBytes;
		pMapInfo->MapType = Bitmap->MapType;
		pMapInfo->NumLots = Bitmap->NumLots;
		


		RC = XixFsRawWriteLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						BitMapLotNumber,
						(uint8 *)BitmapLotHeader,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}


		offset.QuadPart = GetAddressOfBitmapData(
							VCB->LotSize,
							BitMapLotNumber
							);


		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, (uint32) ((VCB->NumLots + 7)/8));


		RC = XixFsRawAlignSafeWriteBlockDevice(
				TargetDevice, 
				VCB->SectorSize,
				&offset, 
				size, 
				(PCHAR)Bitmap->Data);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}
	
	}finally{


	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XifsdWriteBitMap Status(0x%x)\n", RC));
	return RC;
}



NTSTATUS
XixFsInvalidateLotBitMapWithBuffer(
	IN PXIFS_VCB VCB,
	IN uint64 BitMapLotNumber,
	IN PXIDISK_MAP_LOT BitmapLotHeader
)
{

	NTSTATUS				RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER			offset;
	PDEVICE_OBJECT			TargetDevice = NULL;
	int32					size;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsInvalidateLotBitMapWithBuffer \n"));

	ASSERT(VCB);
	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);

	try{

		size = XIDISK_MAP_LOT_SIZE;


		RtlZeroMemory((PCHAR)BitmapLotHeader, XIDISK_MAP_LOT_SIZE);

		RC = XixFsRawReadLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						BitMapLotNumber,
						(uint8 *)BitmapLotHeader,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}

	
		XixFsInitializeCommonLotHeader(
				(PXIDISK_COMMON_LOT_HEADER)BitmapLotHeader,
				VCB->VolumeLotSignature,
				LOT_INFO_TYPE_INVALID,
				LOT_FLAG_INVALID,
				BitMapLotNumber,
				BitMapLotNumber,
				0,
				0,
				0,
				sizeof(XIDISK_MAP_LOT),
				VCB->LotSize - sizeof(XIDISK_MAP_LOT)
				);	
			
	

		
		
		RC = XixFsRawWriteLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						BitMapLotNumber,
						(uint8 *)BitmapLotHeader,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}

	}finally{

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsInvalidateLotBitMapWithBuffer Status(0x%x)\n", RC));
	return RC;
}




NTSTATUS
XixFsReadAndAllocBitMap(	
	IN PXIFS_VCB VCB,
	IN int64 LotMapIndex,
	PXIFS_LOT_MAP *ppLotMap
)
{
	NTSTATUS				RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER			offset;
	PDEVICE_OBJECT			TargetDevice = NULL;
	LARGE_INTEGER			Offset;
	int32					size;
	PXIDISK_MAP_LOT 		pDiskLotMap = NULL;
	PXIDISK_LOT_MAP_INFO    pMapInfo = NULL;
	PXIFS_LOT_MAP			pLotMap = NULL;
	uint32					Reason = 0;
	uint32					i = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsReadAndAllocBitMap \n"));


	ASSERT(VCB);
	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Read Lot Map Index(%I64d) \n", LotMapIndex));

	size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, sizeof(XIFS_LOT_MAP)) + (uint32) ((VCB->NumLots + 7)/8);
	size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, size);


	pLotMap = (PXIFS_LOT_MAP) ExAllocatePool(NonPagedPool, size);
	if(!pLotMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail Allocation pLotMap \n"));
		*ppLotMap = NULL;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory((PCHAR)pLotMap, size);


	size = XIDISK_MAP_LOT_SIZE;

	pDiskLotMap = (PXIDISK_MAP_LOT) ExAllocatePool(NonPagedPool, size);
	if(!pDiskLotMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail Allocation pDiskLotMap .\n"));
		
		ExFreePool(pLotMap);
		*ppLotMap = NULL;
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	RtlZeroMemory((PCHAR)pDiskLotMap, size);


	try{

		RC = XixFsRawReadLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						LotMapIndex,
						(uint8 *)pDiskLotMap,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}


		RC = XixFsCheckLotInfo(
				&pDiskLotMap->LotHeader.LotInfo,
				VCB->VolumeLotSignature,
				LotMapIndex,
				LOT_INFO_TYPE_BITMAP,
				LOT_FLAG_BEGIN,
				&Reason
				);
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Check LotInfo .\n", RC));	
			try_return(RC);
		}


		pMapInfo = &pDiskLotMap->Map;
		pLotMap->BitMapBytes = pMapInfo->BitMapBytes;
		pLotMap->NumLots = pMapInfo->NumLots;
		pLotMap->MapType = pMapInfo->MapType;


		offset.QuadPart= GetAddressOfBitmapData(
					VCB->LotSize,
					LotMapIndex
					);	

		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, (uint32) ((VCB->NumLots + 7)/8));
		


		RC = XixFsRawAlignSafeReadBlockDevice(
				TargetDevice, 
				VCB->SectorSize,
				&offset, 
				size, 
				(PCHAR)pLotMap->Data);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMap Data .\n", RC));	
			try_return(RC);
		}


		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
				("NumLots(%I64d) ,Num Bytes(%ld)\n",
				pLotMap->NumLots, pLotMap->BitMapBytes));

		/*
		for(i = 0; i< (uint32)(pLotMap->BitMapBytes/8); i++)
		{
			DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
				("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
				i*8,
				pLotMap->Data[i*8 + 0],pLotMap->Data[i*8 + 1],
				pLotMap->Data[i*8 + 2],pLotMap->Data[i*8 + 3],
				pLotMap->Data[i*8 + 4],pLotMap->Data[i*8 + 5],
				pLotMap->Data[i*8 + 6],pLotMap->Data[i*8 + 7]
				));
		}
		*/
		*ppLotMap = pLotMap;
		RC = STATUS_SUCCESS;

	}finally{
		ExFreePool(pDiskLotMap);

		if(!NT_SUCCESS(RC)){
			*ppLotMap = NULL;
			if(pLotMap) ExFreePool(pLotMap);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Fail Exit (0x%x)  XixFsReadAndAllocBitMap \n", RC));
	return RC;
}


NTSTATUS
XixFsWriteBitMap(
	IN PXIFS_VCB VCB,
	IN int64 LotMapIndex,
	IN PXIFS_LOT_MAP pLotMap,
	IN BOOLEAN		Initialize
)
{
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	LARGE_INTEGER		offset;
	PDEVICE_OBJECT		TargetDevice = NULL;
	LARGE_INTEGER		Offset;
	int32				size;
	PXIDISK_MAP_LOT 		pDiskLotMap = NULL;
	PXIDISK_LOT_MAP_INFO      pMapInfo = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsWriteBitMap \n"));

	ASSERT(VCB);
	TargetDevice = VCB->TargetDeviceObject;
	ASSERT(TargetDevice);

	size = XIDISK_MAP_LOT_SIZE;

	pDiskLotMap = (PXIDISK_MAP_LOT) ExAllocatePool(NonPagedPool, size);
	if(!pDiskLotMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail Allocation pDiskLotMap \n"));
		return  STATUS_INSUFFICIENT_RESOURCES;
		
	}
	
	RtlZeroMemory((PCHAR)pDiskLotMap, size);


	try{
		RC = XixFsRawReadLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						LotMapIndex,
						(uint8 *)pDiskLotMap,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}
		
		if(Initialize){
			XixFsInitializeCommonLotHeader(
					(PXIDISK_COMMON_LOT_HEADER)pDiskLotMap,
					VCB->VolumeLotSignature,
					LOT_INFO_TYPE_BITMAP,
					LOT_FLAG_BEGIN,
					LotMapIndex,
					LotMapIndex,
					0,
					0,
					0,
					sizeof(XIDISK_MAP_LOT),
					VCB->LotSize - sizeof(XIDISK_MAP_LOT)
					);
						
		}

		pMapInfo = &pDiskLotMap->Map;
		pMapInfo->BitMapBytes = pLotMap->BitMapBytes;
		pMapInfo->MapType = pLotMap->MapType;
		pMapInfo->NumLots = pLotMap->NumLots;
		
		RC = XixFsRawWriteLotAndLotMapHeader(
						TargetDevice,
						VCB->LotSize,
						LotMapIndex,
						(uint8 *)pDiskLotMap,
						XIDISK_MAP_LOT_SIZE,
						VCB->SectorSize
						);
		

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMapInfo .\n", RC));	
			try_return(RC);
		}


		offset.QuadPart= GetAddressOfBitmapData(
					VCB->LotSize,
					LotMapIndex
					);	
		size = SECTOR_ALIGNED_SIZE(VCB->SectorSize, (uint32) ((VCB->NumLots + 7)/8));

		
		RC = XixFsRawAlignSafeWriteBlockDevice(
				TargetDevice, 
				VCB->SectorSize,
				&offset, 
				size, 
				(PCHAR)pLotMap->Data);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) read LotMap Data .\n", RC));	
			try_return(RC);
		}
	
		RC = STATUS_SUCCESS;
	}finally{
		ExFreePool(pDiskLotMap);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsWriteBitMap \n"));
	return RC;
	
}



/*
NTSTATUS
XixFsSetCheckOutLotMap(
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
		("Enter XixFsSetCheckOutLotMap \n"));

	if(HostCount > XIFS_DEFAULT_USER){
		ModIndicator = HostCount;
	}else {
		ModIndicator = XIFS_DEFAULT_USER;
	}



	FreeLotCount = XixFsfindSetBitMapCount(FreeLotMap->NumLots, FreeLotMap->Data);
	//RequestCount = ((FreeLotCount / ModIndicator) * 9) /10;  
	
	RequestCount = 100;

	CheckOutLotMap->StartIndex = XixFsAllocLotMapFromFreeLotMap(
									FreeLotMap->NumLots, 
									RequestCount, 
									FreeLotMap->Data, 
									CheckOutLotMap->Data,
									&AllocatedCount);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%I64d) ,Num Bytes(%ld)\n",
			FreeLotMap->NumLots, FreeLotMap->BitMapBytes));

	
	//for(i = 0; i< (uint32)(FreeLotMap->BitMapBytes/8); i++)
	//{
	//	DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
	//		("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
	//		i*8,
	//		FreeLotMap->Data[i*8 + 0],FreeLotMap->Data[i*8 + 1],
	//		FreeLotMap->Data[i*8 + 2],FreeLotMap->Data[i*8 + 3],
	//		FreeLotMap->Data[i*8 + 4],FreeLotMap->Data[i*8 + 5],
	//		FreeLotMap->Data[i*8 + 6],FreeLotMap->Data[i*8 + 7]
	//		));
	//}
	

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%I64d) ,FreeLotMapCount(%I64d), RequestCount(%I64d), StartIndex(%I64d)\n",
			FreeLotMap->NumLots, FreeLotCount, RequestCount,CheckOutLotMap->StartIndex ));


	if(AllocatedCount < XIFS_DEFAULT_HOST_LOT_COUNT){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Exit Error XixFsSetCheckOutLotMap \n"));
		return STATUS_UNSUCCESSFUL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit  XixFsSetCheckOutLotMap \n"));
	return STATUS_SUCCESS;
}
*/

VOID
XixFsORMap(
	IN PXIFS_LOT_MAP		pDestMap,
	IN PXIFS_LOT_MAP		pSourceMap
	)
{
	uint64	LotMapBytes;
	uint64	i = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsORMap \n"));

	LotMapBytes = pDestMap->BitMapBytes;
	for(i = 0; i < LotMapBytes; i++){
		pDestMap->Data[i] = (pDestMap->Data[i] | pSourceMap->Data[i]);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsORMap \n"));
}

VOID
XixFsEORMap(
	IN PXIFS_LOT_MAP		pDestMap,
	IN PXIFS_LOT_MAP		pSourceMap
)
{
	uint64	LotMapBytes;
	uint64	i = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsEORMap \n"));

	LotMapBytes = pDestMap->BitMapBytes;
	for(i = 0; i < LotMapBytes; i++){
		pDestMap->Data[i] = (pDestMap->Data[i] & ~(pSourceMap->Data[i]));
	}	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsEORMap \n"));
}



int64
XixFsAllocVCBLot(
	IN PXIFS_VCB	VCB
)
{

	uint64 i = 0;
	uint64 BitmapCount = 0;
	BOOLEAN bRetry = FALSE;
	NTSTATUS			RC = STATUS_SUCCESS;
	LARGE_INTEGER		TimeOut;
	uint32	retry_count = 0;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsAllocVCBLot \n"));


	ASSERT(VCB->HostFreeLotMap);
	ASSERT(VCB->HostDirtyLotMap);
	
	// Added by ILGU HONG for readonly 09052006
	if(VCB->IsVolumeWriteProctected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsAllocVCBLot : is Readonly device \n"));
		return -1;
	}
	// Added by ILGU HONG for readonly 09052006


retry:

	XifsdLockVcb(TRUE, VCB);

	if(XifsdCheckFlagBoolean(VCB->VCBFlags, XIFSD_VCB_FLAGS_RECHECK_RESOURCES)){
		XifsdUnlockVcb(TRUE, VCB);
		
		TimeOut.QuadPart = - DEFAULT_XIFS_RECHECKRESOURCE;
		RC = KeWaitForSingleObject(&VCB->ResourceEvent,
							Executive,
							KernelMode,
							FALSE,
							&TimeOut);
		
		if(RC == STATUS_TIMEOUT){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail KeWaitForSingleObject for ResourceEvent \n"));
			return -1;
		}

		XifsdLockVcb(TRUE,VCB);
	}


	if(retry_count == 0) {
		if(XifsdCheckFlagBoolean(VCB->VCBFlags, XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES)){
			XifsdClearFlag(VCB->VCBFlags, XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES);
		}
	}

	if(XifsdCheckFlagBoolean(VCB->VCBFlags, XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES)){
		XifsdUnlockVcb(TRUE, VCB);
		return -1;
	}



	BitmapCount = VCB->HostFreeLotMap->NumLots;
	
	for( i = VCB->HostFreeLotMap->StartIndex; i < BitmapCount; i++)
	{
		
		if(test_bit(i, VCB->HostFreeLotMap->Data))
		{
			clear_bit(i,VCB->HostFreeLotMap->Data);
			set_bit(i,VCB->HostDirtyLotMap->Data);
			VCB->HostFreeLotMap->StartIndex = i;
			VCB->HostDirtyLotMap->StartIndex = i;
			XifsdUnlockVcb(TRUE, VCB);
			XifsdSetFlag(VCB->ResourceFlag,XIFSD_VCB_RESOURCE_UPDATE_FALAG); 
			//DbgPrint("ALLOC LOT NUM(%I64d)\n",i);
			return (int64)i;			
		}
	}
	
	VCB->HostFreeLotMap->StartIndex = 0;
	VCB->HostDirtyLotMap->StartIndex = 0;

	for( i = VCB->HostFreeLotMap->StartIndex; i < BitmapCount; i++)
	{
		
		if(test_bit(i, VCB->HostFreeLotMap->Data))
		{
			clear_bit(i,VCB->HostFreeLotMap->Data);
			set_bit(i,VCB->HostDirtyLotMap->Data);
			VCB->HostFreeLotMap->StartIndex = i;
			VCB->HostDirtyLotMap->StartIndex = i;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
				("Exit Success XixFsAllocVCBLot \n"));
			XifsdUnlockVcb(TRUE, VCB);
			XifsdSetFlag(VCB->ResourceFlag,XIFSD_VCB_RESOURCE_UPDATE_FALAG); 
			//DbgPrint("ALLOC LOT NUM(%I64d)\n",i);
			return (int64)i;			
		}
	}
	
	if(bRetry == FALSE)
		XifsdSetFlag(VCB->VCBFlags, XIFSD_VCB_FLAGS_RECHECK_RESOURCES);
	
	XifsdUnlockVcb(TRUE, VCB);


	if(bRetry == FALSE){
		bRetry = TRUE;
		KeClearEvent(&VCB->ResourceEvent);
		KeSetEvent(&VCB->VCBGetMoreLotEvent, 0, FALSE);

		TimeOut.QuadPart = - DEFAULT_XIFS_RECHECKRESOURCE;
		RC = KeWaitForSingleObject(&VCB->ResourceEvent,
							Executive,
							KernelMode,
							FALSE,
							&TimeOut);
		
		if(NT_SUCCESS(RC)){
			retry_count ++;
			goto retry;
		}
		
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit fail XixFsAllocVCBLot \n"));
	return -1;
}

VOID
XixFsFreeVCBLot(
	IN PXIFS_VCB VCB,
	IN uint64 LotIndex
)
{
	uint64 BitmapCount = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsFreeVCBLot \n"));


	ASSERT(VCB->HostFreeLotMap);
	ASSERT(VCB->HostDirtyLotMap);

	XifsdLockVcb(TRUE, VCB);

	BitmapCount = VCB->HostFreeLotMap->NumLots;
			
	if(BitmapCount < LotIndex){
		XifsdUnlockVcb(TRUE, VCB);
		return;
	}

	//DbgPrint("FREE LOT NUM(%I64d)\n",LotIndex);
	if(test_bit(LotIndex, VCB->HostDirtyLotMap->Data))
	{
		clear_bit(LotIndex,VCB->HostDirtyLotMap->Data);
		set_bit(LotIndex,VCB->HostFreeLotMap->Data);
	}

	if(LotIndex > VCB->HostFreeLotMap->StartIndex){
		VCB->HostFreeLotMap->StartIndex = LotIndex;
		VCB->HostDirtyLotMap->StartIndex = LotIndex;
	}

	//DbgPrint("Dealloc %I64d\n", LotIndex);

	if(XifsdCheckFlagBoolean(VCB->VCBFlags, XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES)){
		XifsdClearFlag(VCB->VCBFlags, XIFSD_VCB_FLAGS_INSUFFICIENT_RESOURCES);
		
	}
	XifsdSetFlag(VCB->ResourceFlag,XIFSD_VCB_RESOURCE_UPDATE_FALAG); 

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsFreeVCBLot \n"));

	XifsdUnlockVcb(TRUE, VCB);
	return;
}
