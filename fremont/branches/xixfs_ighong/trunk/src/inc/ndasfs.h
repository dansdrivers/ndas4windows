#ifndef __NDAS_FS_H__
#define __NDAS_FS_H__

#define NDAS_FS_UNLOAD				CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 17, METHOD_NEITHER, FILE_READ_ACCESS)

#define ND_FAT_CONTROL_DEVICE_NAME	L"\\Device\\NdFatControl"
#define ND_FAT_CONTROL_LINK_NAME	L"\\DosDevices\\NdFatControl"

#define ND_FAT_DEVICE_NAME			L"\\NdFat"

#endif



