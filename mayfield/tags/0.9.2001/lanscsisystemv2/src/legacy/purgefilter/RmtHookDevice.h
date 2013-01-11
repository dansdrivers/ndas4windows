#ifndef RMTHOOKDEVICE_H
#define RMTHOOKDEVICE_H


typedef struct _NETDISK_VOLUME	
{
	LPX_ADDRESS		NetDiskAddress;
	USHORT			UnitDiskNo;
	LARGE_INTEGER	StartingOffset;

} NETDISK_VOLUME, *PNETDISK_VOLUME;


typedef struct _NETDISK_CONTEXT 
{
	NETDISK_VOLUME	NetdiskVolume;
	LPX_ADDRESS		ServerAddress;
	ULONG			SlotNo;

} NETDISK_CONTEXT, *PNETDISK_CONTEXT;
	

BOOLEAN
PrepareRemoteHookDevice(
	PDEVICE_OBJECT		DeviceObject, 
	PNETDISK_CONTEXT	NetdiskContext
	);

#endif