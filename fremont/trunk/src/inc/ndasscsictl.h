#ifndef _NDASSCSI_CONTROL_LIB_H_
#define _NDASSCSI_CONTROL_LIB_H_

#include "ndasscsi.h"

#ifdef __cplusplus
#define NDSCCTL_LINKAGE extern "C" 
#else
#define NDSCCTL_LINKAGE extern 
#endif 

#define NDSCCTLAPI NDSCCTL_LINKAGE


NDSCCTLAPI
PSTORAGE_DESCRIPTOR_HEADER
WINAPI
NdscCtlGetAdapterStoragePropertyFromDisk(
	IN DWORD			PhysicalDiskNumber,
	IN STORAGE_PROPERTY_ID PropertyId,
	IN STORAGE_QUERY_TYPE  QueryType
);

NDSCCTLAPI
PSTORAGE_DESCRIPTOR_HEADER
WINAPI
NdscCtlGetAdapterStoragePropertyFromScsiPort(
	IN DWORD			ScsiPortNumber,
	IN STORAGE_PROPERTY_ID PropertyId,
	IN STORAGE_QUERY_TYPE  QueryType
);

NDSCCTLAPI
BOOL
WINAPI
NdscCtlGetScsiAddressOfDisk(
	IN DWORD			PhysicalDiskNumber,
	OUT PSCSI_ADDRESS	ScsiAddress
);

NDSCCTLAPI
BOOL
WINAPI
NdscCtlGetScsiAddressOfDevice(
	IN DWORD			DeviceNumber,
	IN TCHAR			DeviceNamePrefix[],
	OUT PSCSI_ADDRESS	ScsiAddress
);

NDSCCTLAPI
BOOL
WINAPI
NdscCtlDeviceLockControl(
	IN DWORD					PhysicalDiskNumber,
	IN OUT PNDSCIOCTL_DEVICELOCK	NdasDevLockControl
);


#endif // _NDASSCSI_CONTROL_LIB_H_
