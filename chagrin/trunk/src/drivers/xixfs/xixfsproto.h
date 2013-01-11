#ifndef __XIXFS_PROTO_H__
#define __XIXFS_PROTO_H__

#include "XixFsType.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"


#include "lanscsi.h"
#include "ndasscsiioctl.h"
#include "lurdesc.h"




/*
 *		XixFsLotInfo.c
 */
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
);

NTSTATUS
XixFsCheckLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN	uint32				VolLotSignature,
	IN 	int64				LotNumber,
	IN 	uint32				Type,
	IN 	uint32				Flags,
	IN OUT	uint32			*Reason
);


NTSTATUS
XixFsCheckOutLotHeader(
	IN PDEVICE_OBJECT	TargetDevice,
	IN uint32			VolLotSignature,
	IN uint64			LotIndex,
	IN uint32			LotSize,
	IN uint32			LotType,
	IN uint32			LotFlag,
	IN uint32			SectorSize
);

NTSTATUS
XixFsDumpFileLot(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32  VolLotSignature,
	IN	uint32	LotSize,
	IN	uint64	StartIndex,
	IN	uint32	Type,
	IN	uint32	SectorSize
);





/*
 *	XixFsRawDirStub.c
 */
NTSTATUS
XixFsInitializeDirContext(
	IN PXIFS_VCB		pVCB,
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_DIR_EMUL_CONTEXT DirContext
);


VOID
XixFsCleanupDirContext(
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_DIR_EMUL_CONTEXT DirContext
);


NTSTATUS
XixFsLookupInitialDirEntry(
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_FCB Fcb,
    IN PXIFS_DIR_EMUL_CONTEXT DirContext,
    IN uint32 InitIndex
);

NTSTATUS
XixFsUpdateDirNames(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext
);


NTSTATUS
XixFsFindDirEntryByLotNumber(
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_FCB Fcb,
    IN uint64	LotNumber,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	OUT uint64	*EntryIndex
);


NTSTATUS
XixFsLookupInitialFileIndex(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_FCB pFCB,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN uint64 InitialIndex
);

NTSTATUS
XixFsFindDirEntry (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_FCB Fcb,
    IN PUNICODE_STRING Name,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN				bIgnoreCase
    );



NTSTATUS
XixFsAddChildToDir(
	IN BOOLEAN					Wait,
	IN PXIFS_VCB				pVCB,
	IN PXIFS_FCB				pDir,
	IN uint64					ChildLotNumber,
	IN uint32					Type,
	IN PUNICODE_STRING 			ChildName,
	IN PXIFS_DIR_EMUL_CONTEXT	DirContext,
	OUT uint64 *				ChildIndex
);

NTSTATUS
XixFsDeleteChildFromDir(
	IN BOOLEAN				Wait,
	IN PXIFS_VCB 			pVCB,
	IN PXIFS_FCB			pDir,
	IN uint64				ChildIndex,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext
);



NTSTATUS
XixFsUpdateChildFromDir(
	IN BOOLEAN			Wait,
	IN PXIFS_VCB		pVCB,
	IN PXIFS_FCB		pDir,
	IN uint64			ChildLotNumber,
	IN uint32			Type,
	IN uint32			State,
	IN PUNICODE_STRING	ChildName,
	IN uint64			ChildIndex,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext
);


/*
 *	XixFsRawFileStub.c
 */
NTSTATUS
XixFsInitializeFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
);

NTSTATUS
XixFsSetFileContext(
	PXIFS_VCB	pVCB,
	uint64	LotNumber,
	uint32	FileType,
	PXIFS_FILE_EMUL_CONTEXT FileContext
);

NTSTATUS
XixFsClearFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
);

NTSTATUS
XixFsReadFileInfoFromContext(
	BOOLEAN					Waitable,
	PXIFS_FILE_EMUL_CONTEXT FileContext
);

NTSTATUS
XixFsReadFileInfoFromFcb(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN uint8 *	Buffer,
	IN uint32	BufferSize
);

NTSTATUS
XixFsWriteFileInfoFromFcb(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN uint8 *	Buffer,
	IN uint32	BufferSize
);


NTSTATUS
XixFsReLoadFileFromFcb(
	IN PXIFS_FCB pFCB
);


NTSTATUS
XixFsInitializeFCBInfo(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable
);







/*
 *	XixFsAddressInfo.c
 */

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
);


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
);


NTSTATUS
XixFsCheckEOFAndAllocLotFileInfo(
	IN PXIFS_FCB 	pFCB,
	IN uint64		RequestedEOF,
	IN BOOLEAN		Wait
	);



/*
 *	XixFsNdasDeviceControl.c
 */
BOOLEAN	
XixFsGetScsiportAdapter(
  	IN	PDEVICE_OBJECT				DiskDeviceObject,
  	IN	PDEVICE_OBJECT				*ScsiportAdapterDeviceObject
	);



NTSTATUS
XixFsAddUserBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PNDAS_BLOCK_ACE	NdasBace,
	OUT PBLOCKACE_ID	BlockAceId
	);



NTSTATUS
XixFsRemoveUserBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	BLOCKACE_ID		BlockAceId
	);


NTSTATUS
XixfsGetNdasBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	);




NTSTATUS
XixFsCheckXifsd(
	PDEVICE_OBJECT			DeviceObject,
	PPARTITION_INFORMATION	partitionInfo,
	PBLOCKACE_ID			BlockAceId,
	PBOOLEAN				IsWriteProtected
);


NTSTATUS
XixFsNdasLock(
	PDEVICE_OBJECT	DeviceObject
);

NTSTATUS
XixFsNdasUnLock(
	PDEVICE_OBJECT	DeviceObject
);


NTSTATUS
XixFsNdasQueryLock(
	PDEVICE_OBJECT	DeviceObject
);




/*
 *	XixFsLotLock.c
 */

VOID
XixFsRefAuxLotLock(
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo
);


VOID
XixFsDeRefAuxLotLock(
	PXIFS_AUXI_LOT_LOCK_INFO	AuxLotInfo
);

NTSTATUS
XixFsAuxLotLock(
	BOOLEAN  		Wait,
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	BOOLEAN			Check,
	BOOLEAN			Retry
);


NTSTATUS
XixFsAuxLotUnLock(
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber
);


VOID
XixFsCleanUpAuxLockLockInfo(
		PXIFS_VCB		VCB
);


NTSTATUS
XixFsLotLock(
	BOOLEAN  		Wait,
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	uint32			*Status,
	BOOLEAN			Check,
	BOOLEAN			Retry
);

NTSTATUS
XixFsLotUnLock(
	PXIFS_VCB		VCB,
	PDEVICE_OBJECT	DeviceObject,
	uint64			LotNumber,
	uint32			*Status
);

NTSTATUS
XixFsGetLockState(
	IN PXIFS_VCB VCB,
	IN PDEVICE_OBJECT DeviceObject,
	IN uint64 		LotNumber,
	OUT uint32 * 	State
);


NTSTATUS
XixFsCheckAndLock(
	IN BOOLEAN			Wait,
	IN PXIFS_VCB 		VCB,
	IN PDEVICE_OBJECT 	DeviceObject,
	IN uint64 				LotNumber,
	OUT uint32			*pIsAcuired
);




/*
 *	XixFsLotBitMapOp.c
 */
int64 
XixFsfindSetBitFromMap(
	IN		int64 bitmap_count, 
	IN		int64 bitmap_hint, 
	IN OUT	volatile void * Mpa
);

uint64 
XixFsfindFreeBitFromMap(
	IN		uint64 bitmap_count, 
	IN		uint64 bitmap_hint, 
	IN OUT	volatile void * Mpa
);

uint64 
XixFsfindSetBitMapCount(
	IN		uint64 bitmap_count, 
	IN OUT	volatile void *LotMapData
);

uint64
XixFsAllocLotMapFromFreeLotMap(
	IN	uint64 bitmap_count, 
	IN	uint64 request,  
	IN	volatile void * FreeLotMapData, 
	IN	volatile void * CheckOutLotMapData,
	IN	uint64 * 	AllocatedCount
);

NTSTATUS
XixFsReadBitMapWithBuffer(
	IN PXIFS_VCB VCB,
	IN uint64 BitMapLotNumber,
	IN PXIFS_LOT_MAP	Bitmap,	
	IN PXIDISK_MAP_LOT BitmapLotHeader
);

NTSTATUS
XixFsWriteBitMapWithBuffer(
	IN PXIFS_VCB VCB,
	IN uint64 BitMapLotNumber,
	IN PXIFS_LOT_MAP Bitmap,
	IN PXIDISK_MAP_LOT BitmapLotHeader,
	IN BOOLEAN		Initialize
);

NTSTATUS
XixFsInvalidateLotBitMapWithBuffer(
	IN PXIFS_VCB VCB,
	IN uint64 BitMapLotNumber,
	IN PXIDISK_MAP_LOT BitmapLotHeader
);

NTSTATUS
XixFsReadAndAllocBitMap(	
	IN PXIFS_VCB VCB,
	IN int64 LotMapIndex,
	PXIFS_LOT_MAP *ppLotMap
);

NTSTATUS
XixFsWriteBitMap(
	IN PXIFS_VCB VCB,
	IN int64 LotMapIndex,
	IN PXIFS_LOT_MAP pLotMap,
	IN BOOLEAN		Initialize
);

NTSTATUS
XixFsSetCheckOutLotMap(
	IN	PXIFS_LOT_MAP	FreeLotMap,
	IN	PXIFS_LOT_MAP	CheckOutLotMap,
	IN	int32 			HostCount
);

VOID
XixFsORMap(
	IN PXIFS_LOT_MAP		pDestMap,
	IN PXIFS_LOT_MAP		pSourceMap
	);

VOID
XixFsEORMap(
	IN PXIFS_LOT_MAP		pDestMap,
	IN PXIFS_LOT_MAP		pSourceMap
);

int64
XixFsAllocVCBLot(
	IN PXIFS_VCB	VCB
);

VOID
XixFsFreeVCBLot(
	IN PXIFS_VCB VCB,
	IN uint64 LotIndex
);





#endif //#ifndef __XIXFS_PROTO_H__
