#ifndef _FILEDISK_IOCTL_H_
#define _FILEDISK_IOCTL_H_
#pragma once

#include <ndas/ndasportioctl.h>
#include <pshpack8.h>

#define FILEDISK_FLAG_USE_SPARSE_FILE 0x00000001

typedef struct _FILEDISK_DESCRIPTOR {
	NDAS_LOGICALUNIT_DESCRIPTOR Header;
	LARGE_INTEGER LogicalBlockAddress;
	ULONG BytesPerBlock;
	ULONG FileDiskFlags;
	WCHAR FilePath[1];
} FILEDISK_DESCRIPTOR, *PFILEDISK_DESCRIPTOR;

C_ASSERT(FIELD_OFFSET(FILEDISK_DESCRIPTOR, FilePath) == 16 + sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));
C_ASSERT(sizeof(FILEDISK_DESCRIPTOR) == 24 + sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));

#include <poppack.h>

#endif /* _FILEDISK_IOCTL_H_ */
