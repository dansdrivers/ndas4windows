#ifndef WXPFATLIB_H
#define WXPFATLIB_H


VOID
WxpFatFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		FSpyDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp
	);


VOID
WxpFatPurgeVolume (
    IN PDEVICE_OBJECT		SpyFsDeviceObject,
    IN PDEVICE_OBJECT		BaseDeviceObject
	);


#endif
