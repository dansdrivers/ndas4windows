#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, GetAddressOfLot)
#pragma alloc_text(PAGE, GetAddressOfLotLock)
#pragma alloc_text(PAGE, GetAddressOfFileHeader)
#pragma alloc_text(PAGE, GetAddressOfVolLotHeader)
#pragma alloc_text(PAGE, GetAddressOfBitMapHeader)
#pragma alloc_text(PAGE, GetAddressOfBitmapData)
#pragma alloc_text(PAGE, GetAddressOfFileAddrInfo)
#pragma alloc_text(PAGE, GetAddressOfFileData)
#pragma alloc_text(PAGE, GetAddressOfFileAddrInfoFromSec)
#pragma alloc_text(PAGE, GetLotCountOfFile)
#pragma alloc_text(PAGE, GetLogicalStartAddrOfFile)
#pragma alloc_text(PAGE, GetIndexOfLogicalAddress)
#endif


uint64
GetLcnFromLot(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize );

	ASSERT(IS_4096_SECTOR(PhyAddress));

	return (uint64)(PhyAddress/CLUSTER_SIZE);
}


/*

	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> lot's physical address
*/
uint64
GetAddressOfLot(
	IN uint32	LotSize,
	IN uint64	LotIndex
	)
{
	uint64 PhyAddress = 0;
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfLot\n"));

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize );

	ASSERT(IS_4096_SECTOR(PhyAddress));
	
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfLot phyAddr(%I64d)\n", PhyAddress));
	return PhyAddress;
}


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> lot lock's  physical address
 */
uint64
GetAddressOfLotLock(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfLot\n"));

	return GetAddressOfLot(LotSize, LotIndex);
}	




/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> file/dir header's  physical address
*/

uint64
GetAddressOfFileHeader(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfFileHeader\n"));
	
	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfFileHeader phyAddr(%I64d)\n", PhyAddress));
	return PhyAddress;
}


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Volume header's  physical address
*/
uint64
GetAddressOfVolLotHeader(void)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfVolLotHeader\n"));
	
	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC;
	
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfVolLottHeader phyAddr(%I64d)\n", PhyAddress));
	return PhyAddress;
}



/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Volume header's  physical address
*/
uint64
GetAddressOfBitMapHeader(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfBitMapHeader\n"));

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfBitMapHeader phyAddr(%I64d)\n", PhyAddress));
	return PhyAddress;	
}


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Host Record's  physical address
*/
uint64
GetIndexOfHostInfo(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetIndexOfHostInfo\n"));

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetIndexOfHostInfo phyAddr(%I64d)\n", PhyAddress));
	return PhyAddress;		
}



uint64
GetIndexOfHostRecord(
	IN uint32	LotSize,
	IN uint32	Index,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetIndexOfHostRecord\n"));

	PhyAddress = GetIndexOfHostInfo(LotSize, LotIndex) + Index * SECTORSIZE_4096;
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetIndexOfHostRecord Index(%ld) phyAddr(%I64d)\n", Index, PhyAddress));
	return PhyAddress;			
}

/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Bit map data's  physical address
*/
uint64
GetAddressOfBitmapData(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfBitmapData\n"));

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_MAP_LOT_SIZE;
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfBitmapData phyAddr(%I64d)\n", PhyAddress));

	return PhyAddress;	
}

/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> File Address info's address
*/
uint64
GetAddressOfFileAddrInfo(
	IN uint32	LotSize,
	IN uint64	LotIndex
)
{
	uint64 PhyAddress = 0;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfFileAddrInfo\n"));

	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_FILE_HEADER_LOT_SIZE;
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfFileAddrInfo phyAddr(%I64d)\n", PhyAddress));

	return PhyAddress;
}



/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> File Address info's address
*/
uint64
GetAddressOfFileData(
		IN uint32 Flag,
		IN uint32	LotSize,
		IN uint64	LotIndex
)
{
	int64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfFileData\n"));

	if(Flag == LOT_FLAG_BEGIN){
		PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_FILE_HEADER_SIZE;
	}else{
		PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	}
	
	ASSERT(IS_4096_SECTOR(PhyAddress));

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfFileData phyAddr(%I64d)\n", PhyAddress));

	return PhyAddress;
}




/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> File Address info's address
*/

uint64
GetAddressOfFileAddrInfoFromSec(
	IN uint32	LotSize,
	IN uint64	LotIndex,
	IN uint32	SecNum,
	IN uint64	AuxiAddrLot,
	IN uint32	SectorSize
)
{
	uint64 PhyAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetAddressOfFileAddrInfoFromSec\n"));

	if(SecNum < XIDISK_ADDR_MAP_SEC_COUNT(SectorSize)){
		PhyAddress = GetAddressOfFileAddrInfo(LotSize, LotIndex) + SecNum * SectorSize;
	
	}else{
		ASSERT(AuxiAddrLot > XIFS_RESERVED_LOT_SIZE);
		SecNum -= XIDISK_ADDR_MAP_SEC_COUNT(SectorSize);
		PhyAddress = GetAddressOfFileData(LOT_FLAG_BODY, LotSize, AuxiAddrLot) + SecNum* SectorSize;
	}

	IS_SECTOR_ALIGNED(SectorSize, PhyAddress);

	
	
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetAddressOfFileAddrInfoFromSec phyAddr(%I64d)\n", PhyAddress));

	return PhyAddress;
}


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> return LotCount which offset is included 
*/

uint32
GetLotCountOfFile(
	IN uint32		LotSize,
	IN uint64		Offset	
)
{
	uint32	Count = 0;
	uint64 	Size = Offset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetLotCountOfFile\n"));

	ASSERT(LotSize > XIDISK_FILE_HEADER_SIZE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ADDRTRANS, 
					("GetLotCountOfFile Size(%I64d).\n", Size));
	

	if(Size == 0) {
		DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
			("Exit GetLotCountOfFile Count 1\n"));
		return 1;
	}
	
	if(Size < (LotSize - XIDISK_FILE_HEADER_SIZE )){
		DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
			("Exit GetLotCountOfFile Count 1\n"));
		return 1;
	}

	Size -= (LotSize - XIDISK_FILE_HEADER_SIZE -1);
	Count =(uint32) ( ((Size + (LotSize - XIDISK_DATA_LOT_SIZE -1))/(LotSize - XIDISK_DATA_LOT_SIZE)) + 1);

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetLotCountOfFile Count(%ld)\n", Count));

	return Count;
}


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Logical start address of file from lot index from file
*/
uint64
GetLogicalStartAddrOfFile(
	IN uint32		Index,
	IN uint32		LotSize
)
{
	uint64		LogicalStartAddress = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetLogicalStartAddrOfFile\n"));

	if(Index == 0) {
		return 0;
	}else {
		LogicalStartAddress = ((LotSize - XIDISK_DATA_LOT_SIZE) * (Index -1)) + (LotSize - XIDISK_FILE_HEADER_SIZE);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetLogicalStartAddrOfFile LogAddr(%I64d)\n", LogicalStartAddress));

	return LogicalStartAddress;
}


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT -->Lot Index of File from logical address of file
*/
uint32
GetIndexOfLogicalAddress(
	IN uint32		LotSize,
	IN uint64		offset
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetIndexOfLogicalAddress\n"));

	return (GetLotCountOfFile(LotSize, offset) - 1);	
}