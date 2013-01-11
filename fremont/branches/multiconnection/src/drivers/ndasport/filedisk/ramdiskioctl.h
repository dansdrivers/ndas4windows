#ifndef _RAMDISK_IOCTL_H_
#define _RAMDISK_IOCTL_H_
#pragma once

#include <ndas/ndasportioctl.h>
#include <pshpack8.h>

typedef struct _RAMDISK_DESCRIPTOR {
	NDAS_LOGICALUNIT_DESCRIPTOR Header;
	ULONG SectorSize;
	ULONG SectorCount;
	ULONG Flags;
	ULONG Reserved;
} RAMDISK_DESCRIPTOR, *PRAMDISK_DESCRIPTOR;

C_ASSERT(sizeof(RAMDISK_DESCRIPTOR) == 16 + sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));

#include <poppack.h>

#endif /* _RAMDISK_IOCTL_H_ */
