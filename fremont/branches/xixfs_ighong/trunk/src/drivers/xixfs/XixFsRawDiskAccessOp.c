#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDiskForm.h"
#include "XixFsRawDiskInteralApi.h"
#include "XixFsRawDiskAccessApi.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsRawReadLotHeader)
#pragma alloc_text(PAGE, XixFsRawWriteLotHeader)
#pragma alloc_text(PAGE, XixFsRawReadLotLockInfo)
#pragma alloc_text(PAGE, XixFsRawWriteLotLockInfo)
#pragma alloc_text(PAGE, XixFsRawReadLotAndFileHeader)
#pragma alloc_text(PAGE, XixFsRawWriteLotAndFileHeader)
#pragma alloc_text(PAGE, XixFsRawReadFileHeader)
#pragma alloc_text(PAGE, XixFsRawWriteFileHeader)
#pragma alloc_text(PAGE, XixFsRawReadAddressOfFile)
#pragma alloc_text(PAGE, XixFsRawWriteAddressOfFile)
#pragma alloc_text(PAGE, XixFsRawReadVolumeLotHeader)
#pragma alloc_text(PAGE, XixFsRawWriteVolumeLotHeader)
#pragma alloc_text(PAGE, XixFsRawReadLotAndLotMapHeader)
#pragma alloc_text(PAGE, XixFsRawWriteLotAndLotMapHeader)
#pragma alloc_text(PAGE, XixFsRawReadRegisterHostInfo)
#pragma alloc_text(PAGE, XixFsRawWriteRegisterHostInfo)
#pragma alloc_text(PAGE, XixFsRawReadRegisterRecord)
#pragma alloc_text(PAGE, XixFsRawWriteRegisterRecord)
#endif


/*
 *	Function must be done within waitable thread context
 */

/*
 *	Read and Write Lot Header
 */

NTSTATUS
XixFsRawReadLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadLotHeader \n"));

	ASSERT(BufferSize >= XIDISK_COMMON_LOT_HEADER_SIZE);

	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeReadLotHeader(
			TargetDevice,
			&LotOffset,
			XIDISK_COMMON_LOT_HEADER_SIZE, 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadLotHeader \n"));
	return RC;
}


NTSTATUS
XixFsRawWriteLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteLotHeader \n"));
	
	ASSERT(BufferSize >= XIDISK_COMMON_LOT_HEADER_SIZE);

	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeWriteLotHeader(
			TargetDevice,
			&LotOffset,
			XIDISK_COMMON_LOT_HEADER_SIZE, 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteLotHeader \n"));
	return RC;
}


NTSTATUS
XixFsRawReadLotLockInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
	//IN	BOOLEAN			b4kBlock
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadLotLockInfo \n"));
	
	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeReadLotLockInfo(
			TargetDevice,
			&LotOffset,
			BufferSize, 
			Buffer,
			SectorSize
			);


	/*
	if(b4kBlock){
		
		ASSERT(BufferSize >=XIDISK_COMMON_LOT_HEADER_SIZE);

		RC = XixFsEndianSafeReadLotHeader(
				TargetDevice,
				&LotOffset,
				XIDISK_COMMON_LOT_HEADER_SIZE, 
				Buffer,
				SectorSize
				);

	}else{
		ASSERT(BufferSize >=XIDISK_LOCK_SIZE);

		RC = XixFsEndianSafeReadLotLockInfo(
				TargetDevice,
				&LotOffset,
				XIDISK_LOCK_SIZE,
				Buffer,
				SectorSize
				);
	}
	*/
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadLotLockInfo \n"));
	return RC;
}



NTSTATUS
XixFsRawWriteLotLockInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
	//IN	BOOLEAN			b4kBlock
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadLotLockInfo \n"));
	
	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeWriteLotLockInfo(
			TargetDevice,
			&LotOffset,
			BufferSize, 
			Buffer,
			SectorSize
			);

	/*
	if(b4kBlock){

		ASSERT(BufferSize >=XIDISK_COMMON_LOT_HEADER_SIZE);

		RC = XixFsEndianSafeWriteLotHeader(
				TargetDevice,
				&LotOffset,
				XIDISK_COMMON_LOT_HEADER_SIZE, 
				Buffer
				);

	}else{
		ASSERT(BufferSize >=XIDISK_LOCK_SIZE);

		RC = XixFsEndianSafeWriteLotLockInfo(
				TargetDevice,
				&LotOffset,
				XIDISK_LOCK_SIZE,
				Buffer
				);
	}
	*/

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadLotLockInfo \n"));
	return RC;
}





/*
 *	Read and Write Lot and File Header
 */


NTSTATUS
XixFsRawReadLotAndFileHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadLotAndFileHeader \n"));
	
	ASSERT(BufferSize >=XIDISK_FILE_HEADER_LOT_SIZE);

	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeReadLotAndFileHeader(
			TargetDevice,
			&LotOffset,
			(XIDISK_FILE_HEADER_LOT_SIZE), 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadLotAndFileHeader \n"));
	return RC;
}



NTSTATUS
XixFsRawWriteLotAndFileHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteLotAndFileHeader \n"));

	ASSERT(BufferSize >=XIDISK_FILE_HEADER_LOT_SIZE);

	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeWriteLotAndFileHeader(
			TargetDevice,
			&LotOffset,
			(XIDISK_FILE_HEADER_LOT_SIZE), 
			Buffer,
			SectorSize
			);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteLotAndFileHeader \n"));
	return RC;
}

/*
 *	Read and Write File Header
 */

NTSTATUS
XixFsRawReadFileHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadFileHeader \n"));
	
	ASSERT(BufferSize >=XIDISK_FILE_INFO_SIZE);

	LotOffset.QuadPart = GetAddressOfFileHeader(LotSize, LotIndex);
	
	RC = XixFsEndianSafeReadFileHeader(
			TargetDevice,
			&LotOffset,
			(XIDISK_FILE_INFO_SIZE), 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadFileHeader \n"));
	return RC;
}



NTSTATUS
XixFsRawWriteFileHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteFileHeader \n"));

	ASSERT(BufferSize >=XIDISK_FILE_INFO_SIZE);

	LotOffset.QuadPart = GetAddressOfFileHeader(LotSize, LotIndex);
	
	RC = XixFsEndianSafeWriteFileHeader(
			TargetDevice,
			&LotOffset,
			(XIDISK_FILE_INFO_SIZE), 
			Buffer,
			SectorSize);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteFileHeader \n"));
	return RC;
}


/*
 *		Read and Write Address of File
 */
NTSTATUS
XixFsRawReadAddressOfFile(
	IN 	PDEVICE_OBJECT	TargetDeviceObject,
	IN	uint32			LotSize,
	IN	uint64			LotNumber,
	IN	uint64			AdditionalLotNumber,
	IN OUT	uint8		* Addr,
	IN	uint32			BufferSize,
	IN OUT	uint32		* AddrStartSecIndex,
	IN	uint32			SecNum,
	IN	uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	LARGE_INTEGER	Offset;
	uint32			BlockSize = 0;

	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), ("Enter XixFsRawReadAddressOfFile\n"));
	

	ASSERT(Addr);
	ASSERT(AddrStartSecIndex);
	ASSERT(LotNumber > 0);

	ASSERT(BufferSize >= SectorSize);
	BlockSize = SectorSize;


	try{
		Offset.QuadPart = GetAddressOfFileAddrInfoFromSec(LotSize, LotNumber, (uint32)SecNum, AdditionalLotNumber, SectorSize);
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("XixFsRawReadAddressOfFile of FCB(%I64d) Sec(%ld) Lot Infor Loc(%I64d)\n", 
				LotNumber,SecNum, Offset.QuadPart));
		
		RC = XixFsEndianSafeReadAddrInfo(
			TargetDeviceObject,
			SectorSize,
			&Offset,
			BlockSize, 
			Addr);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error XixFsRawReadAddressOfFile:XixFsEndianSafeReadAddrInfo Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}

		*AddrStartSecIndex = SecNum;
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("After XixFsRawReadAddressOfFile of FCB(%I64d) Sec(%ld)\n", 
				LotNumber,SecNum));
	}finally{
		
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), ("Exit XixFsRawReadAddressOfFile Status(0x%x)\n", RC));	
	return RC;
}


NTSTATUS
XixFsRawWriteAddressOfFile(
	IN 	PDEVICE_OBJECT	TargetDeviceObject,
	IN	uint32			LotSize,
	IN	uint64			LotNumber,
	IN	uint64			AdditionalLotNumber,
	IN OUT	uint8			* Addr,
	IN	uint32			BufferSize,
	IN OUT	uint32			* AddrStartSecIndex,
	IN	uint32			SecNum,
	IN	uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;	
	LARGE_INTEGER	Offset;
	uint32			BlockSize = 0;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), ("Enter XixFsRawWriteAddressOfFile\n"));
	ASSERT(Addr);
	ASSERT(AddrStartSecIndex);

	ASSERT(SecNum == * AddrStartSecIndex);
	ASSERT(LotNumber > 0);

	ASSERT(BufferSize >= SectorSize);
	BlockSize = SectorSize;


	try{
		Offset.QuadPart = GetAddressOfFileAddrInfoFromSec(LotSize, LotNumber, (uint32)SecNum, AdditionalLotNumber, SectorSize);
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("XixFsRawWriteAddressOfFile of FCB(%I64d) Sec(%ld) Lot Infor Loc(%I64d)\n", 
				LotNumber,SecNum, Offset.QuadPart));


		RC = XixFsEndianSafeWriteAddrInfo(
			TargetDeviceObject,
			SectorSize,
			&Offset,
			BlockSize, 
			Addr);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error XixFsRawWriteAddressOfFile:XixFsEndianSafeWriteAddrInfo Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}
		*AddrStartSecIndex = SecNum;
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("After XixFsRawWriteAddressOfFile of FCB(%I64d) Sec(%ld)\n", 
				LotNumber,SecNum));
	
	}finally{
	
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), ("Exit XixFsRawWriteAddressOfFile Status(0x%x)\n", RC));	
	return RC;
}


/*
 *	Get and Set Dir Entry Info
 */

VOID
XixFsGetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
)
{
#if (_M_PPC || _M_MPPC) 
		ChangeDirEntryLtoB(Buffer);
#endif //#if (_M_PPC || _M_MPPC) 
	return;
}


VOID
XixFsSetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
)
{
#if (_M_PPC || _M_MPPC) 
		ChangeDirEntryBtoL((PXIDISK_CHILD_INFORMATION)Buffer);
#endif //#if (_M_PPC || _M_MPPC) 
	return;
}



/*
 *	Read Volume Header
 */

NTSTATUS
XixFsRawReadVolumeLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize	
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadVolumeHeader \n"));
	
	ASSERT(BufferSize>=XIDISK_VOLUME_LOT_SIZE);

	LotOffset.QuadPart = GetAddressOfVolLotHeader();
	
	RC = XixFsEndianSafeReadVolumeLotHeader(
			TargetDevice,
			&LotOffset,
			(XIDISK_VOLUME_LOT_SIZE), 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadVolumeHeader \n"));
	return RC;
}


NTSTATUS
XixFsRawWriteVolumeLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize	
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteVolumeHeader \n"));

	ASSERT(BufferSize>=XIDISK_VOLUME_LOT_SIZE);

	LotOffset.QuadPart = GetAddressOfVolLotHeader();
	
	RC = XixFsEndianSafeWriteVolumeLotHeader(
			TargetDevice,
			&LotOffset,
			(XIDISK_VOLUME_LOT_SIZE), 
			Buffer,
			SectorSize
			);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteVolumeHeader \n"));
	return RC;
}







/*
 *	Read and Write Lot Map Header
 */
NTSTATUS
XixFsRawReadLotAndLotMapHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadLotAndLotMapHeader \n"));
	
	ASSERT(BufferSize>=XIDISK_MAP_LOT_SIZE);

	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeReadLotAndLotMapHeader(
			TargetDevice,
			SectorSize,
			&LotOffset,
			(XIDISK_MAP_LOT_SIZE), 
			Buffer
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadLotAndLotMapHeader \n"));
	return RC;
}


NTSTATUS
XixFsRawWriteLotAndLotMapHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteLotAndLotMapHeader \n"));
	
	ASSERT(BufferSize>=XIDISK_MAP_LOT_SIZE);
	
	LotOffset.QuadPart = GetAddressOfLot(LotSize, LotIndex);
	
	RC = XixFsEndianSafeWriteLotAndLotMapHeader(
			TargetDevice,
			SectorSize,
			&LotOffset,
			(XIDISK_MAP_LOT_SIZE), 
			Buffer);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteLotAndLotMapHeader \n"));
	return RC;
}





/*
 *	Read and Write Register Host Info
 */

NTSTATUS
XixFsRawReadRegisterHostInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadRegisterHostInfo \n"));
	
	ASSERT(BufferSize>=XIDISK_HOST_INFO_SIZE);

	LotOffset.QuadPart = GetIndexOfHostInfo(LotSize, LotIndex);
	
	RC = XixFsEndianSafeReadHostInfo(
			TargetDevice,
			&LotOffset,
			(XIDISK_HOST_INFO_SIZE), 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadRegisterHostInfo \n"));
	return RC;
}


NTSTATUS
XixFsRawWriteRegisterHostInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteRegisterHostInfo \n"));
	
	ASSERT(BufferSize>=XIDISK_HOST_INFO_SIZE);

	LotOffset.QuadPart = GetIndexOfHostInfo(LotSize, LotIndex);
	
	RC = XixFsEndianSafeWriteHostInfo(
			TargetDevice,
			&LotOffset,
			(XIDISK_HOST_INFO_SIZE), 
			Buffer,
			SectorSize
			);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteRegisterHostInfo \n"));
	return RC;
}



/*
 *	Read and Write Register Record Info
 */
NTSTATUS
XixFsRawReadRegisterRecord(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint32			Index,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawReadRegisterRecord \n"));
	
	ASSERT(BufferSize>=XIDISK_HOST_RECORD_SIZE);

	LotOffset.QuadPart = GetIndexOfHostRecord(LotSize, Index, LotIndex);
	
	RC = XixFsEndianSafeReadHostRecord(
			TargetDevice,
			&LotOffset,
			(XIDISK_HOST_RECORD_SIZE), 
			Buffer,
			SectorSize
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawReadRegisterRecord \n"));
	return RC;
}


NTSTATUS
XixFsRawWriteRegisterRecord(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN  uint32			Index,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LARGE_INTEGER	LotOffset;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsRawWriteRegisterRecord \n"));

	ASSERT(BufferSize>=XIDISK_HOST_RECORD_SIZE);

	LotOffset.QuadPart = GetIndexOfHostRecord(LotSize, Index, LotIndex);
	
	RC = XixFsEndianSafeWriteHostRecord(
			TargetDevice,
			&LotOffset,
			(XIDISK_HOST_RECORD_SIZE), 
			Buffer,
			SectorSize);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit XixFsRawWriteRegisterRecord \n"));
	return RC;
}
