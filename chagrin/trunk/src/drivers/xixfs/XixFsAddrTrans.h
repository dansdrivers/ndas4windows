#ifndef __XIXFS_ADDR_TRANS_H__
#define __XIXFS_ADDR_TRANS_H__
/*
 *	XixFsTransAddr.c
 */



uint64
GetLcnFromLot(
	IN uint32	LotSize,
	IN uint64	LotIndex
);

/*

	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> lot's physical address
*/
uint64
GetAddressOfLot(
	IN uint32	LotSize,
	IN uint64	LotIndex
	);


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> lot lock's  physical address
 */
uint64
GetAddressOfLotLock(
	IN uint32	LotSize,
	IN uint64	LotIndex
);




/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> file/dir header's  physical address
*/

uint64
GetAddressOfFileHeader(
	IN uint32	LotSize,
	IN uint64	LotIndex
);


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Volume header's  physical address
*/
uint64
GetAddressOfVolLotHeader(void);


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Volume header's  physical address
*/
uint64
GetAddressOfBitMapHeader(
	IN uint32	LotSize,
	IN uint64	LotIndex
);


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Host Record's  physical address
*/
uint64
GetIndexOfHostInfo(
	IN uint32	LotSize,
	IN uint64	LotIndex
);


uint64
GetIndexOfHostRecord(
	IN uint32	LotSize,
	IN uint32	Index,
	IN uint64	LotIndex
);

/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Bit map data's  physical address
*/
uint64
GetAddressOfBitmapData(
	IN uint32	LotSize,
	IN uint64	LotIndex
);

/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> File Address info's address
*/
uint64
GetAddressOfFileAddrInfo(
	IN uint32	LotSize,
	IN uint64	LotIndex
);



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
);


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
);

/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> return LotCount which offset is included 
*/

uint32
GetLotCountOfFile(
	IN uint32		LotSize,
	IN uint64		Offset	
);

/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT --> Logical start address of file from lot index from file
*/
uint64
GetLogicalStartAddrOfFile(
	IN uint64		Index,
	IN uint32		LotSize
);


/*
 	IN-->	LotSize		(size of lot >> 1M)
			LotIndex	(lot index)

	OUT -->Lot Index of File from logical address of file
*/
uint32
GetIndexOfLogicalAddress(
	IN uint32		LotSize,
	IN uint64		offset
);

#endif //#ifndef __XIXFS_ADDR_TRANS_H__