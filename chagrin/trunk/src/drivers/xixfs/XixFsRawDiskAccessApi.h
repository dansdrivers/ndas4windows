#ifndef __XIXFS_RAWDISKACCESS_API__H__
#define __XIXFS_RAWDISKACCESS_API__H__

#include "XixFsType.h"
#include "XixFsDiskForm.h"


/*
 *	XixFsRawDiskAccessOp.c		Endian Safe function
 */




NTSTATUS
XixFsRawReadLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);


NTSTATUS
XixFsRawWriteLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);


NTSTATUS
XixFsRawReadLotLockInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
	//IN	BOOLEAN			b4kBlock
);



NTSTATUS
XixFsRawWriteLotLockInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
	//IN	BOOLEAN			b4kBlock
);





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
);



NTSTATUS
XixFsRawWriteLotAndFileHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);


// Added by ILGU HONG 12082006
/*
 *  Read and Write Directory Entry Hash Value table
 */

NTSTATUS
XixFsRawReadDirEntryHashValueTable(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);

NTSTATUS
XixFsRawWriteDirEntryHashValueTable(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);
// Added by ILGU HONG 12082006 END



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
);



NTSTATUS
XixFsRawWriteFileHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize
);

/*
 *	Get and Set Dir Entry Info
 */

VOID
XixFsGetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
);


VOID
XixFsSetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
);

/*
 *		Read and Write Address of File
 */
NTSTATUS
XixFsRawReadAddressOfFile(
	IN 	PDEVICE_OBJECT	TargetDeviceObject,
	IN	uint32			LotSize,
	IN	uint64			LotNumber,
	IN	uint64			AdditionalLotNumber,
	IN OUT	uint8			* Addr,
	IN	uint32			BufferSize,
	IN OUT	uint32			* AddrStartSecIndex,
	IN	uint32			SecNum,
	IN	uint32			SectorSize
);


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
);







/*
 *	Read Volume Header
 */

NTSTATUS
XixFsRawReadVolumeLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize	
);


NTSTATUS
XixFsRawWriteVolumeLotHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize	
);


/*
 * Read and Write boot sector
 */
NTSTATUS
XixFsRawReadBootSector(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize	
);

NTSTATUS
XixFsRawWriteBootSector(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize	
);


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
);


NTSTATUS
XixFsRawWriteLotAndLotMapHeader(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);





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
);


NTSTATUS
XixFsRawWriteRegisterHostInfo(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN uint32			SectorSize
);



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
);


NTSTATUS
XixFsRawWriteRegisterRecord(
	IN 	PDEVICE_OBJECT	TargetDevice,
	IN	uint32			LotSize,
	IN  uint32			Index,
	IN	uint64			LotIndex,
	IN	uint8			*Buffer,
	IN	uint32			BufferSize,
	IN	uint32			SectorSize
);




/*
 *	XixFsRawDeviceOp.c	Endian Unsafe function
 */

NTSTATUS
XixFsRawDevIoCtrl (
    IN	PDEVICE_OBJECT Device,
    IN	uint32 IoControlCode,
    IN	uint8 * InputBuffer OPTIONAL,
    IN	uint32 InputBufferLength,
    OUT uint8 * OutputBuffer OPTIONAL,
    IN	uint32 OutputBufferLength,
    IN	BOOLEAN InternalDeviceIoControl,
    OUT PIO_STATUS_BLOCK Iosb OPTIONAL
    );


NTSTATUS
XixFsRawWriteBlockDevice (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	IN uint8			*Buffer
	);



NTSTATUS
XixFsRawReadBlockDevice (
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PLARGE_INTEGER	Offset,
	IN	uint32			Length,
	OUT uint8			*Buffer
	);


NTSTATUS
XixFsRawAlignSafeWriteBlockDevice (
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	IN uint8			*Buffer
	);


NTSTATUS
XixFsRawAlignSafeReadBlockDevice (
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	uint32			SectorSize,
	IN	PLARGE_INTEGER	Offset,
	IN	uint32			Length,
	OUT uint8			*Buffer
	);



#endif //#ifndef __XIXFS_RAWDISKACCESS_API__H__