#ifndef __NDAS_FS_H__
#define __NDAS_FS_H__

#define NDAS_FS_UNLOAD				CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 17, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define NDAS_FAT_CONTROL_DEVICE_NAME	L"\\Device\\NdFatControl"
#define NDAS_FAT_CONTROL_LINK_NAME		L"\\DosDevices\\NdFatControl"

#define NDAS_FAT_DEVICE_NAME			L"\\NdFat"

#define NDAS_NTFS_CONTROL_DEVICE_NAME	L"\\Device\\NdNtfsControl"
#define NDAS_NTFS_CONTROL_LINK_NAME		L"\\DosDevices\\NdNtfsControl"

#define NDAS_NTFS_DEVICE_NAME			L"\\NdNtfs"

#endif



