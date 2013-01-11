#ifndef WNETFATLIB_H
#define WNETFATLIB_H


BOOLEAN
WnetFatFilteringIt (
    IN PDEVICE_OBJECT SpyFsDeviceObject,
    IN PDEVICE_OBJECT VolumeDeviceObject
	);

VOID
WnetFatFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		SpyFsDeviceObject,
    IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp
	);

VOID
WnetFatPurgeVolume (
    IN PDEVICE_OBJECT	SpyFsDeviceObject,
    IN PDEVICE_OBJECT	BaseDeviceObject
	);


#endif
