#ifndef W2KNTFSLIB_H
#define W2KNTFSLIB_H

BOOLEAN
W2kNtfsFlushMetaFile (
    IN PDEVICE_OBJECT		FSpyDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN ULONG				BufferLen,
	IN PCHAR				Buffer
	);


VOID
W2kNtfsFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		SpyFsDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp,
	IN ULONG				BufferLen,
	IN PCHAR				Buffer
	);

#endif