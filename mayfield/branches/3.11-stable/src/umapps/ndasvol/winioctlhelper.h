#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//#include <devioctl.h>
// #include <ntddstor.h>
// #include <ntddscsi.h>
// #include <ntdddisk.h>
// #include "extype.h"

//
// Caller should free the non-null returned buffer with
// HeapFree(GetProcessHeap(),...) 
//

PDRIVE_LAYOUT_INFORMATION_EX
pDiskGetDriveLayoutEx(
	HANDLE hDevice);

PSTORAGE_DESCRIPTOR_HEADER
pStorageQueryProperty(
	HANDLE hDevice,
	STORAGE_PROPERTY_ID PropertyId,
	STORAGE_QUERY_TYPE  QueryType);

PSTORAGE_DEVICE_DESCRIPTOR
pStorageQueryDeviceProperty(
	HANDLE hDevice);

PSTORAGE_ADAPTER_DESCRIPTOR
pStorageQueryAdapterProperty(
	HANDLE hDevice);

PVOLUME_DISK_EXTENTS
pVolumeGetVolumeDiskExtents(
	HANDLE hVolume);

BOOL
pScsiGetAddress(
	HANDLE hDevice, 
	PSCSI_ADDRESS ScsiAddress);

BOOL
pStorageGetDeviceNumber(
	HANDLE hDevice, 
	PSTORAGE_DEVICE_NUMBER StorDevNum);

/* obsolete function */
BOOL
pDiskGetPartitionInfo(
	HANDLE hDevice,
	PPARTITION_INFORMATION PartInfo);

#ifdef __cplusplus
}
#endif /* __cplusplus */
