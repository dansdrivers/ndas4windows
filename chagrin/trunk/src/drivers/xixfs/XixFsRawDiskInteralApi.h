#ifndef __XIXFS_RAWDISKINTERNAL_API__H__
#define __XIXFS_RAWDISKINTERNAL_API__H__

#include "XixFsType.h"

VOID
XixChange16LittleToBig(
	IN uint16	* Addr
);

VOID
XixChange16BigToLittle(
	IN uint16	* Addr
);


VOID
XixChange32LittleToBig(
	IN uint32	* Addr
);


VOID
XixChange32BigToLittle(
	IN uint32	* Addr
);



VOID
XixChange64LittleToBig(
	IN uint64	* Addr
);


VOID
XixChange64BigToLittle(
	IN uint64	* Addr
);





/*
 *	All Operation Done Waitable Context
 */

/*
 *	Lot Lock Info
 */


NTSTATUS
XixFsEndianSafeReadLotLockInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);





NTSTATUS
XixFsEndianSafeWriteLotLockInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);




/*
 *	Lot Header
 */

NTSTATUS
XixFsEndianSafeReadLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);




NTSTATUS
XixFsEndianSafeWriteLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);




/*
 *	Lot and File Header
 */


NTSTATUS
XixFsEndianSafeReadLotAndFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);



NTSTATUS
XixFsEndianSafeWriteLotAndFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);

// Added by ILGU HONG 12082006
/*
 *	Directory Entry Hash Value Table
 */

NTSTATUS
XixFsEndianSafeReadDirEntryHashValueTable(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);

NTSTATUS
XixFsEndianSafeWriteDirEntryHashValueTable(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);

// Added by ILGU HONG 12082006 END

/*
 *	File Header
 */


NTSTATUS
XixFsEndianSafeReadFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);



NTSTATUS
XixFsEndianSafeWriteFileHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);


/*
 *		For Addr information of File function
 */


NTSTATUS
XixFsEndianSafeReadAddrInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
);




NTSTATUS
XixFsEndianSafeWriteAddrInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
);



/*
 *	Volume Header
 */


NTSTATUS
XixFsEndianSafeReadVolumeLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize	
);




NTSTATUS
XixFsEndianSafeWriteVolumeLotHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize	
);

/*
 *	bootsector
 */
NTSTATUS
XixFsEndianSafeReadBootSector(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	OUT uint8			*Buffer,
	IN uint32			SectorSize	
);

NTSTATUS
XixFsEndianSafeWriteBootSector(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	OUT uint8			*Buffer,
	IN uint32			SectorSize	
);


/*
 *	Lot and Lot map Header
 */



NTSTATUS
XixFsEndianSafeReadLotAndLotMapHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
);




NTSTATUS
XixFsEndianSafeWriteLotAndLotMapHeader(
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
);


/*
 *	Read and Write Register Host Info	
 */



NTSTATUS
XixFsEndianSafeReadHostInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);




NTSTATUS
XixFsEndianSafeWriteHostInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);



/*
 *	Read and Write Register Host Record
 */


NTSTATUS
XixFsEndianSafeReadHostRecord(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);




NTSTATUS
XixFsEndianSafeWriteHostRecord(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer,
	IN uint32			SectorSize
);





#endif //#ifndef __XIXFS_RAWDISKINTERNAL_API__H__