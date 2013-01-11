#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"
#include "XixFsRawDiskInteralApi.h"
#include "XixFsRawDiskAccessApi.h"

/*
 *	Function must be done within waitable thread context
 */


/*
 *	Lot Lock Info
 */
VOID
ChangeLockInfoLtoB(
	IN PXIDISK_LOCK					Lock
)
{
	// Lock Info
	XixChange32LittleToBig(&(Lock->LockState));
	XixChange64LittleToBig(&(Lock->LockAcquireTime));

	return;
}


VOID
ChangeLockInfoBtoL(
	IN PXIDISK_LOCK					Lock
)
{
	// Lock Info
	XixChange32BigToLittle(&(Lock->LockState));
	XixChange64BigToLittle(&(Lock->LockAcquireTime));

	return;
}


NTSTATUS
XixFsEndianSafeReadLotLockInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_LOCK	pLotHeader = NULL;
	pLotHeader = (PXIDISK_LOCK)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC ) 
		if(NT_SUCCESS(RC)){
			ChangeLockInfoLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteLotLockInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_LOCK	pLotHeader = NULL;
	pLotHeader = (PXIDISK_LOCK)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeLockInfoBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if  (_M_PPC || _M_MPPC) 
			ChangeLockInfoLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




/*
 *	Lot Header
 */
VOID
ChangeLotHeaderLtoB(
	IN PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	// Lock Info
	XixChange32LittleToBig(&(pLotHeader->Lock.LockState));
	XixChange64LittleToBig(&(pLotHeader->Lock.LockAcquireTime));


	// Lot Header
	XixChange32LittleToBig(&(pLotHeader->LotInfo.Type));
	XixChange32LittleToBig(&(pLotHeader->LotInfo.Flags));
	XixChange64LittleToBig(&(pLotHeader->LotInfo.LotIndex));
	XixChange64LittleToBig(&(pLotHeader->LotInfo.BeginningLotIndex));
	XixChange64LittleToBig(&(pLotHeader->LotInfo.PreviousLotIndex));
	XixChange64LittleToBig(&(pLotHeader->LotInfo.NextLotIndex));
	XixChange64LittleToBig(&(pLotHeader->LotInfo.LogicalStartOffset));
	XixChange32LittleToBig(&(pLotHeader->LotInfo.StartDataOffset));
	XixChange32LittleToBig(&(pLotHeader->LotInfo.LotTotalDataSize));
	XixChange32LittleToBig(&(pLotHeader->LotInfo.LotSignature));

	return;
}

VOID
ChangeLotHeaderBtoL(
	IN PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	// Lock Info
	XixChange32BigToLittle(&(pLotHeader->Lock.LockState));
	XixChange64BigToLittle(&(pLotHeader->Lock.LockAcquireTime));


	// Lot Header
	XixChange32BigToLittle(&(pLotHeader->LotInfo.Type));
	XixChange32BigToLittle(&(pLotHeader->LotInfo.Flags));
	XixChange64BigToLittle(&(pLotHeader->LotInfo.LotIndex));
	XixChange64BigToLittle(&(pLotHeader->LotInfo.BeginningLotIndex));
	XixChange64BigToLittle(&(pLotHeader->LotInfo.PreviousLotIndex));
	XixChange64BigToLittle(&(pLotHeader->LotInfo.NextLotIndex));
	XixChange64BigToLittle(&(pLotHeader->LotInfo.LogicalStartOffset));
	XixChange32BigToLittle(&(pLotHeader->LotInfo.StartDataOffset));
	XixChange32BigToLittle(&(pLotHeader->LotInfo.LotTotalDataSize));
	XixChange32BigToLittle(&(pLotHeader->LotInfo.LotSignature));
	return;
}


NTSTATUS
XixFsEndianSafeReadLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC ) 
		if(NT_SUCCESS(RC)){
			ChangeLotHeaderLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeLotHeaderBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if  (_M_PPC || _M_MPPC) 
			ChangeLotHeaderLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




/*
 *	Lot and File Header
 */

VOID
ChangeLotAndFileHeaderLtoB(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;

	// Lock Info
	XixChange32LittleToBig(&(pDirLotHeader->LotHeader.Lock.LockState));
	XixChange64LittleToBig(&(pDirLotHeader->LotHeader.Lock.LockAcquireTime));


	// Lot Header
	XixChange32LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.Type));
	XixChange32LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.Flags));
	XixChange64LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.LotIndex));
	XixChange64LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.BeginningLotIndex));
	XixChange64LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.PreviousLotIndex));
	XixChange64LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.NextLotIndex));
	XixChange64LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.LogicalStartOffset));
	XixChange32LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.StartDataOffset));
	XixChange32LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.LotTotalDataSize));
	XixChange32LittleToBig(&(pDirLotHeader->LotHeader.LotInfo.LotSignature));


	// FileHeader

	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.DirInfoSignature1));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.State));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.Type));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.ParentDirLotIndex));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.FileSize));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.AllocationSize));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.FileAttribute));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.LotIndex));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.AddressMapIndex));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.LinkCount));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.Create_time));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.Change_time));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.Modified_time));
	XixChange64LittleToBig(&(pDirLotHeader->DirInfo.Access_time));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.AccessFlags));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.ACLState));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.NameSize));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.childCount));
	XixChange32LittleToBig(&(pDirLotHeader->DirInfo.DirInfoSignature1));

	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pDirLotHeader->DirInfo.NameSize/2);
	Name = (uint16 *)pDirLotHeader->DirInfo.Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16LittleToBig(&Name[i]);
		}
	}
	
	return;
}

VOID
ChangeLotAndFileHeaderBtoL(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;

	// Lock Info
	XixChange32BigToLittle(&(pDirLotHeader->LotHeader.Lock.LockState));
	XixChange64BigToLittle(&(pDirLotHeader->LotHeader.Lock.LockAcquireTime));


	// Lot Header
	XixChange32BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.Type));
	XixChange32BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.Flags));
	XixChange64BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.LotIndex));
	XixChange64BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.BeginningLotIndex));
	XixChange64BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.PreviousLotIndex));
	XixChange64BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.NextLotIndex));
	XixChange64BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.LogicalStartOffset));
	XixChange32BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.StartDataOffset));
	XixChange32BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.LotTotalDataSize));
	XixChange32BigToLittle(&(pDirLotHeader->LotHeader.LotInfo.LotSignature));


	// FileHeader

	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.DirInfoSignature1));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.State));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.Type));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.ParentDirLotIndex));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.FileSize));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.AllocationSize));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.FileAttribute));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.LotIndex));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.AddressMapIndex));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.LinkCount));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.Create_time));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.Change_time));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.Modified_time));
	XixChange64BigToLittle(&(pDirLotHeader->DirInfo.Access_time));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.AccessFlags));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.ACLState));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.NameSize));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.childCount));
	XixChange32BigToLittle(&(pDirLotHeader->DirInfo.DirInfoSignature1));


	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pDirLotHeader->DirInfo.NameSize/2);
	Name = (uint16 *)pDirLotHeader->DirInfo.Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16BigToLittle(&Name[i]);
		}
	}
	return;
}


NTSTATUS
XixFsEndianSafeReadLotAndFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_DIR_HEADER_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_DIR_HEADER_LOT)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeLotAndFileHeaderLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteLotAndFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_DIR_HEADER_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_DIR_HEADER_LOT)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeLotAndFileHeaderBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeLotAndFileHeaderLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}



// Added by ILGU HONG 12082006
/*
 *	Directory Entry Hash Value Table
 */



VOID
ChangeDirHashValTableLtoB(
	IN PXIDISK_HASH_VALUE_TABLE	pTable
)
{

	uint32 i = 0;
	for(i = 0; i< MAX_DIR_ENTRY; i++){
		XixChange16LittleToBig( &(pTable->EntryHashVal[i]) );
	}

	return;
}

VOID
ChangeDirHashValTableBtoL(
	IN PXIDISK_HASH_VALUE_TABLE	pTable
)
{
	
	uint32 i = 0;
	for(i = 0; i<MAX_DIR_ENTRY; i++){
		XixChange16BigToLittle( &(pTable->EntryHashVal[i]) );
	}

	return;
}



NTSTATUS
XixFsEndianSafeReadDirEntryHashValueTable(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	pTable = (PXIDISK_HASH_VALUE_TABLE)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error XixFsEndianSafeReadDirEntryHashValueTable:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeDirHashValTableLtoB(pTable);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}

NTSTATUS
XixFsEndianSafeWriteDirEntryHashValueTable(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_DIR_HEADER_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_DIR_HEADER_LOT)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeDirHashValTableBtoL(pTable);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error XixFsEndianSafeWriteDirEntryHashValueTable:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeDirHashValTableLtoB(pTable);
#endif //#if (_M_PPC || _M_MPPC) 		
	}
	return RC;
}

// Added by ILGU HONG 12082006 END


/*
 *	File Header
 */

VOID
ChangeFileHeaderLtoB(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;



	// FileHeader

	XixChange32LittleToBig(&(pDirHeader->DirInfoSignature1));
	XixChange32LittleToBig(&(pDirHeader->State));
	XixChange32LittleToBig(&(pDirHeader->Type));
	XixChange64LittleToBig(&(pDirHeader->ParentDirLotIndex));
	XixChange64LittleToBig(&(pDirHeader->FileSize));
	XixChange64LittleToBig(&(pDirHeader->AllocationSize));
	XixChange32LittleToBig(&(pDirHeader->FileAttribute));
	XixChange64LittleToBig(&(pDirHeader->LotIndex));
	XixChange64LittleToBig(&(pDirHeader->AddressMapIndex));
	XixChange32LittleToBig(&(pDirHeader->LinkCount));
	XixChange64LittleToBig(&(pDirHeader->Create_time));
	XixChange64LittleToBig(&(pDirHeader->Change_time));
	XixChange64LittleToBig(&(pDirHeader->Modified_time));
	XixChange64LittleToBig(&(pDirHeader->Access_time));
	XixChange32LittleToBig(&(pDirHeader->AccessFlags));
	XixChange32LittleToBig(&(pDirHeader->ACLState));
	XixChange32LittleToBig(&(pDirHeader->NameSize));
	XixChange32LittleToBig(&(pDirHeader->childCount));
	XixChange32LittleToBig(&(pDirHeader->DirInfoSignature1));

	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pDirHeader->NameSize/2);
	Name = (uint16 *)pDirHeader->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16LittleToBig(&Name[i]);
		}
	}
	
	return;
}

VOID
ChangeFileHeaderBtoL(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;

	// FileHeader

	XixChange32BigToLittle(&(pDirHeader->DirInfoSignature1));
	XixChange32BigToLittle(&(pDirHeader->State));
	XixChange32BigToLittle(&(pDirHeader->Type));
	XixChange64BigToLittle(&(pDirHeader->ParentDirLotIndex));
	XixChange64BigToLittle(&(pDirHeader->FileSize));
	XixChange64BigToLittle(&(pDirHeader->AllocationSize));
	XixChange32BigToLittle(&(pDirHeader->FileAttribute));
	XixChange64BigToLittle(&(pDirHeader->LotIndex));
	XixChange64BigToLittle(&(pDirHeader->AddressMapIndex));
	XixChange32BigToLittle(&(pDirHeader->LinkCount));
	XixChange64BigToLittle(&(pDirHeader->Create_time));
	XixChange64BigToLittle(&(pDirHeader->Change_time));
	XixChange64BigToLittle(&(pDirHeader->Modified_time));
	XixChange64BigToLittle(&(pDirHeader->Access_time));
	XixChange32BigToLittle(&(pDirHeader->AccessFlags));
	XixChange32BigToLittle(&(pDirHeader->ACLState));
	XixChange32BigToLittle(&(pDirHeader->NameSize));
	XixChange32BigToLittle(&(pDirHeader->childCount));
	XixChange32BigToLittle(&(pDirHeader->DirInfoSignature1));

	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pDirHeader->NameSize/2);
	Name = (uint16 *)pDirHeader->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16BigToLittle(&Name[i]);
		}
	}
	return;
}


NTSTATUS
XixFsEndianSafeReadFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_DIR_INFO	pLotHeader = NULL;
	pLotHeader = (PXIDISK_DIR_INFO)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeFileHeaderLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_DIR_INFO	pLotHeader = NULL;
	pLotHeader = (PXIDISK_DIR_INFO)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeFileHeaderBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeFileHeaderLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}


/*
 *		For Addr information of File function
 */
VOID
ChangeAddrBtoL(
	IN uint8 * Addr,
	IN uint32  BlockSize
)
{
	uint64 * tmpAddr = NULL;
	uint32 i = 0;
	uint32 maxLoop = 0;

	tmpAddr = (uint64 *)Addr;
	maxLoop = (uint32)( BlockSize/sizeof(uint64));	
	
	for(i = 0; i < maxLoop; i++){
		XixChange64BigToLittle(&tmpAddr[i]);
	}

	return;
}

VOID
ChangeAddrLtoB(
	IN uint8 * Addr,
	IN uint32  BlockSize
)
{
	uint64 * tmpAddr = NULL;
	uint32 i = 0;
	uint32 maxLoop = 0;

	tmpAddr = (uint64 *)Addr;
	maxLoop = (uint32)( BlockSize/sizeof(uint64));	
	
	for(i = 0; i < maxLoop; i++){
		XixChange64LittleToBig(&tmpAddr[i]);
	}

	return;
}


NTSTATUS
XixFsEndianSafeReadAddrInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeAddrLtoB(Buffer,Length);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteAddrInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;

#if (_M_PPC || _M_MPPC) 
	ChangeAddrBtoL(Buffer,Length);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeAddrLtoB(Buffer,Length);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}


/*
 *	Dir Entry
 */

VOID
ChangeDirEntryLtoB(
	IN PXIDISK_CHILD_INFORMATION	pDirEntry
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;



	// DirEntry

	XixChange32LittleToBig(&(pDirEntry->Childsignature));
	XixChange32LittleToBig(&(pDirEntry->Type));
	XixChange32LittleToBig(&(pDirEntry->State));
	XixChange64LittleToBig(&(pDirEntry->StartLotIndex));
	XixChange32LittleToBig(&(pDirEntry->NameSize));
	XixChange32LittleToBig(&(pDirEntry->ChildIndex));
	XixChange32LittleToBig(&(pDirEntry->Childsignature2));


	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pDirEntry->NameSize/2);
	Name = (uint16 *)pDirEntry->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16LittleToBig(&Name[i]);
		}
	}
	
	return;
}

VOID
ChangeDirEntryBtoL(
	IN PXIDISK_CHILD_INFORMATION	pDirEntry
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;



	// DirEntry

	XixChange32BigToLittle(&(pDirEntry->Childsignature));
	XixChange32BigToLittle(&(pDirEntry->Type));
	XixChange32BigToLittle(&(pDirEntry->State));
	XixChange64BigToLittle(&(pDirEntry->StartLotIndex));
	XixChange32BigToLittle(&(pDirEntry->NameSize));
	XixChange32BigToLittle(&(pDirEntry->ChildIndex));
	XixChange32BigToLittle(&(pDirEntry->Childsignature2));


	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pDirEntry->NameSize/2);
	Name = (uint16 *)pDirEntry->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16BigToLittle(&Name[i]);
		}
	}
	
	return;
}


/*
 *	Volume Header
 */
VOID
ChangeVolumeHeaderLtoB(
	IN	PXIDISK_VOLUME_LOT	pVolHeader
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;


	// Lock Info
	XixChange32LittleToBig(&(pVolHeader->LotHeader.Lock.LockState));
	XixChange64LittleToBig(&(pVolHeader->LotHeader.Lock.LockAcquireTime));


	// Lot Header
	XixChange32LittleToBig(&(pVolHeader->LotHeader.LotInfo.Type));
	XixChange32LittleToBig(&(pVolHeader->LotHeader.LotInfo.Flags));
	XixChange64LittleToBig(&(pVolHeader->LotHeader.LotInfo.LotIndex));
	XixChange64LittleToBig(&(pVolHeader->LotHeader.LotInfo.BeginningLotIndex));
	XixChange64LittleToBig(&(pVolHeader->LotHeader.LotInfo.PreviousLotIndex));
	XixChange64LittleToBig(&(pVolHeader->LotHeader.LotInfo.NextLotIndex));
	XixChange64LittleToBig(&(pVolHeader->LotHeader.LotInfo.LogicalStartOffset));
	XixChange32LittleToBig(&(pVolHeader->LotHeader.LotInfo.StartDataOffset));
	XixChange32LittleToBig(&(pVolHeader->LotHeader.LotInfo.LotTotalDataSize));
	XixChange32LittleToBig(&(pVolHeader->LotHeader.LotInfo.LotSignature));


	// FileHeader

	XixChange64LittleToBig(&(pVolHeader->VolInfo.VolumeSignature));
	XixChange32LittleToBig(&(pVolHeader->VolInfo.XifsVesion));
	XixChange64LittleToBig(&(pVolHeader->VolInfo.NumLots));
	XixChange32LittleToBig(&(pVolHeader->VolInfo.LotSize));
	XixChange64LittleToBig(&(pVolHeader->VolInfo.HostRegLotMapIndex));
	XixChange64LittleToBig(&(pVolHeader->VolInfo.RootDirectoryLotIndex));
	XixChange64LittleToBig(&(pVolHeader->VolInfo.VolCreationTime));
	XixChange32LittleToBig(&(pVolHeader->VolInfo.VolSerialNumber));
	XixChange32LittleToBig(&(pVolHeader->VolInfo.VolLabelLength));
	XixChange32LittleToBig(&(pVolHeader->VolInfo.LotSignature));
	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pVolHeader->VolInfo.VolLabelLength/2);
	Name = (uint16 *)pVolHeader->VolInfo.VolLabel;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16LittleToBig(&Name[i]);
		}
	}
	
	return;
}

VOID
ChangeVolumeHeaderBtoL(
	IN PXIDISK_VOLUME_LOT	pVolHeader
)
{
	uint32 NameSize = 0;
	uint32 i = 0;
	uint16 * Name = NULL;



	// Lock Info
	XixChange32BigToLittle(&(pVolHeader->LotHeader.Lock.LockState));
	XixChange64BigToLittle(&(pVolHeader->LotHeader.Lock.LockAcquireTime));


	// Lot Header
	XixChange32BigToLittle(&(pVolHeader->LotHeader.LotInfo.Type));
	XixChange32BigToLittle(&(pVolHeader->LotHeader.LotInfo.Flags));
	XixChange64BigToLittle(&(pVolHeader->LotHeader.LotInfo.LotIndex));
	XixChange64BigToLittle(&(pVolHeader->LotHeader.LotInfo.BeginningLotIndex));
	XixChange64BigToLittle(&(pVolHeader->LotHeader.LotInfo.PreviousLotIndex));
	XixChange64BigToLittle(&(pVolHeader->LotHeader.LotInfo.NextLotIndex));
	XixChange64BigToLittle(&(pVolHeader->LotHeader.LotInfo.LogicalStartOffset));
	XixChange32BigToLittle(&(pVolHeader->LotHeader.LotInfo.StartDataOffset));
	XixChange32BigToLittle(&(pVolHeader->LotHeader.LotInfo.LotTotalDataSize));
	XixChange32BigToLittle(&(pVolHeader->LotHeader.LotInfo.LotSignature));


	// FileHeader

	XixChange64BigToLittle(&(pVolHeader->VolInfo.VolumeSignature));
	XixChange32BigToLittle(&(pVolHeader->VolInfo.XifsVesion));
	XixChange64BigToLittle(&(pVolHeader->VolInfo.NumLots));
	XixChange32BigToLittle(&(pVolHeader->VolInfo.LotSize));
	XixChange64BigToLittle(&(pVolHeader->VolInfo.HostRegLotMapIndex));
	XixChange64BigToLittle(&(pVolHeader->VolInfo.RootDirectoryLotIndex));
	XixChange64BigToLittle(&(pVolHeader->VolInfo.VolCreationTime));
	XixChange32BigToLittle(&(pVolHeader->VolInfo.VolSerialNumber));
	XixChange32BigToLittle(&(pVolHeader->VolInfo.VolLabelLength));
	XixChange32BigToLittle(&(pVolHeader->VolInfo.LotSignature));
	/*
	 *	 Change Unicode string order
	 */
	NameSize = (uint32)(pVolHeader->VolInfo.VolLabelLength/2);
	Name = (uint16 *)pVolHeader->VolInfo.VolLabel;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XixChange16BigToLittle(&Name[i]);
		}
	}
	
	return;
}


NTSTATUS
XixFsEndianSafeReadVolumeLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize	
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_VOLUME_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_VOLUME_LOT)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeVolumeHeaderLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteVolumeLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_VOLUME_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_VOLUME_LOT)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeVolumeHeaderBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeVolumeHeaderLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




/*
*	Boot sector
*/




VOID
ChangeBootSectorLtoB(
	IN PPACKED_BOOT_SECTOR	pBootSector
)
{
	// Lock Info
	XixChange64LittleToBig(&(pBootSector->VolumeSignature));
	XixChange64LittleToBig(&(pBootSector->LotNumber));
	XixChange32LittleToBig(&(pBootSector->LotSignature ));
	XixChange32LittleToBig(&(pBootSector->XifsVesion ));
	return;
}

VOID
ChangeBootSectorBtoL(
	IN PPACKED_BOOT_SECTOR	pBootSector
)
{
	XixChange64BigToLittle(&(pBootSector->VolumeSignature));
	XixChange64BigToLittle(&(pBootSector->LotNumber));
	XixChange32BigToLittle(&(pBootSector->LotSignature ));
	XixChange32BigToLittle(&(pBootSector->XifsVesion ));
	
	return;
}

NTSTATUS
XixFsEndianSafeReadBootSector(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	OUT uint8			*Buffer,
	IN uint32			SectorSize	
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PPACKED_BOOT_SECTOR	pBootSector = NULL;
	pBootSector = (PPACKED_BOOT_SECTOR)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			SectorSize, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeBootSectorLtoB(pBootSector);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteBootSector(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PPACKED_BOOT_SECTOR	pBootSector = NULL;
	pBootSector = (PPACKED_BOOT_SECTOR)Buffer;		

#if (_M_PPC || _M_MPPC) 
	ChangeBootSectorBtoL(pBootSector);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			SectorSize, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeBootSector(pBootSector);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




/*
 *	Lot and Lot map Header
 */


VOID
ChangeLotAndLotMapHeaderLtoB(
	IN PXIDISK_MAP_LOT	pLotMapHeader
)
{

	// Lock Info
	XixChange32LittleToBig(&(pLotMapHeader->LotHeader.Lock.LockState));
	XixChange64LittleToBig(&(pLotMapHeader->LotHeader.Lock.LockAcquireTime));


	// Lot Header
	XixChange32LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.Type));
	XixChange32LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.Flags));
	XixChange64LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.LotIndex));
	XixChange64LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.BeginningLotIndex));
	XixChange64LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.PreviousLotIndex));
	XixChange64LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.NextLotIndex));
	XixChange64LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.LogicalStartOffset));
	XixChange32LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.StartDataOffset));
	XixChange32LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.LotTotalDataSize));
	XixChange32LittleToBig(&(pLotMapHeader->LotHeader.LotInfo.LotSignature));


	// LotMapHeader

	XixChange32LittleToBig(&(pLotMapHeader->Map.MapType));
	XixChange32LittleToBig(&(pLotMapHeader->Map.BitMapBytes));
	XixChange64LittleToBig(&(pLotMapHeader->Map.NumLots));
	
	return;
}

VOID
ChangeLotAndLotMapHeaderBtoL(
	IN PXIDISK_MAP_LOT	pLotMapHeader
)
{
	// Lock Info
	XixChange32BigToLittle(&(pLotMapHeader->LotHeader.Lock.LockState));
	XixChange64BigToLittle(&(pLotMapHeader->LotHeader.Lock.LockAcquireTime));


	// Lot Header
	XixChange32BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.Type));
	XixChange32BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.Flags));
	XixChange64BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.LotIndex));
	XixChange64BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.BeginningLotIndex));
	XixChange64BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.PreviousLotIndex));
	XixChange64BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.NextLotIndex));
	XixChange64BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.LogicalStartOffset));
	XixChange32BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.StartDataOffset));
	XixChange32BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.LotTotalDataSize));
	XixChange32BigToLittle(&(pLotMapHeader->LotHeader.LotInfo.LotSignature));


	// LotMapHeader

	XixChange32BigToLittle(&(pLotMapHeader->Map.MapType));
	XixChange32BigToLittle(&(pLotMapHeader->Map.BitMapBytes));
	XixChange64BigToLittle(&(pLotMapHeader->Map.NumLots));
	
	return;
}


NTSTATUS
XixFsEndianSafeReadLotAndLotMapHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_MAP_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_MAP_LOT)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeLotAndLotMapHeaderLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteLotAndLotMapHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_MAP_LOT	pLotHeader = NULL;
	pLotHeader = (PXIDISK_MAP_LOT)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeLotAndLotMapHeaderBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeLotAndLotMapHeaderLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}


/*
 *	Read and Write Register Host Info	
 */

VOID
ChangeHostInfoLtoB(
	IN PXIDISK_HOST_INFO	pHostInfo
)
{

	// HostInfo
	XixChange32LittleToBig(&(pHostInfo->NumHost));
	XixChange64LittleToBig(&(pHostInfo->UsedLotMapIndex));
	XixChange64LittleToBig(&(pHostInfo->UnusedLotMapIndex));
	XixChange64LittleToBig(&(pHostInfo->CheckOutLotMapIndex));
	XixChange64LittleToBig(&(pHostInfo->LogFileIndex));
	return;
}

VOID
ChangeHostInfoBtoL(
	IN PXIDISK_HOST_INFO	pHostInfo
)
{
	// Lock Info
	XixChange32BigToLittle(&(pHostInfo->NumHost));
	XixChange64BigToLittle(&(pHostInfo->UsedLotMapIndex));
	XixChange64BigToLittle(&(pHostInfo->UnusedLotMapIndex));
	XixChange64BigToLittle(&(pHostInfo->CheckOutLotMapIndex));
	XixChange64BigToLittle(&(pHostInfo->LogFileIndex));
	return;
}


NTSTATUS
XixFsEndianSafeReadHostInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_HOST_INFO	pLotHeader = NULL;
	pLotHeader = (PXIDISK_HOST_INFO)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeHostInfoLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteHostInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_HOST_INFO	pLotHeader = NULL;
	pLotHeader = (PXIDISK_HOST_INFO)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeHostInfoBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeHostInfoLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




/*
 *	Read and Write Register Host Record
 */
VOID
ChangeHostRecordLtoB(
	IN PXIDISK_HOST_RECORD	pHostRecord
)
{

	// Host Rcord
	XixChange32LittleToBig(&(pHostRecord->HostState));
	XixChange64LittleToBig(&(pHostRecord->HostMountTime));
	XixChange64LittleToBig(&(pHostRecord->HostCheckOutLotMapIndex));
	XixChange64LittleToBig(&(pHostRecord->HostUsedLotMapIndex));
	XixChange64LittleToBig(&(pHostRecord->HostUnusedLotMapIndex));
	XixChange64LittleToBig(&(pHostRecord->LogFileIndex));
	return;
}

VOID
ChangeHostRecordBtoL(
	IN PXIDISK_HOST_RECORD	pHostRecord
)
{
	// Host Rcord
	XixChange32BigToLittle(&(pHostRecord->HostState));
	XixChange64BigToLittle(&(pHostRecord->HostMountTime));
	XixChange64BigToLittle(&(pHostRecord->HostCheckOutLotMapIndex));
	XixChange64BigToLittle(&(pHostRecord->HostUsedLotMapIndex));
	XixChange64BigToLittle(&(pHostRecord->HostUnusedLotMapIndex));
	XixChange64BigToLittle(&(pHostRecord->LogFileIndex));
	return;
}


NTSTATUS
XixFsEndianSafeReadHostRecord(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_HOST_RECORD	pLotHeader = NULL;
	pLotHeader = (PXIDISK_HOST_RECORD)Buffer;	

	try{
		RC = XixFsRawAlignSafeReadBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeHostRecordLtoB(pLotHeader);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XixFsEndianSafeWriteHostRecord(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PXIDISK_HOST_RECORD	pLotHeader = NULL;
	pLotHeader = (PXIDISK_HOST_RECORD)Buffer;	

#if (_M_PPC || _M_MPPC) 
	ChangeHostRecordBtoL(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XixFsRawAlignSafeWriteBlockDevice(
			DeviceObject,
			SectorSize,
			Offset,
			Length, 
			Buffer
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XixFsRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeHostRecordLtoB(pLotHeader);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}


