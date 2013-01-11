#ifndef WXPNTFSLIB_H
#define WXPNTFSLIB_H

BOOLEAN
WxpNtfsFlushMetaFile (
    IN PDEVICE_OBJECT		SpyFsDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN ULONG				BufferLen,
	IN PUCHAR				Buffer
	);

VOID
WxpNtfsFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		FSpyDeviceObject,
	IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp,
	IN ULONG				BufferLen,
	IN PUCHAR				Buffer
	) ;

#endif
