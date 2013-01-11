#ifndef W2KFATLIB_H
#define W2KFATLIB_H


VOID
W2kFatFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		SpyFsDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp
	);


VOID
W2kFatPurgeVolume (
    IN PDEVICE_OBJECT	SpyFsDeviceObject,
    IN PDEVICE_OBJECT	BaseDeviceObject
	);


#endif
