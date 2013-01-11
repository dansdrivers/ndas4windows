#ifndef __NDAS_FS_H__
#define __NDAS_FS_H__

#define NDAS_FS_UNLOAD				CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 17, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define NDAS_FAT_CONTROL_DEVICE_NAME	L"\\Device\\NdasFatControl"
#define NDAS_FAT_CONTROL_LINK_NAME		L"\\DosDevices\\NdasFatControl"

#define NDAS_FAT_DEVICE_NAME			L"\\NdasFat"

#define NDAS_NTFS_CONTROL_DEVICE_NAME	L"\\Device\\NdasNtfsControl"
#define NDAS_NTFS_CONTROL_LINK_NAME		L"\\DosDevices\\NdasNtfsControl"

#define NDAS_NTFS_DEVICE_NAME			L"\\NdasNtfs"

#endif



