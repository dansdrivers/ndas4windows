#ifndef _NDASDISK_IOCTL_H_
#define _NDASDISK_IOCTL_H_
#pragma once
#include <ndas/ndasportioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <pshpack8.h>

#define NDAS_DEVICE_DEFAULT_LPX_STREAM_LISTEN_PORT 10000

//
// NDAS device identifier is a 16-byte wide structure
// containing MAC address of the NDAS device, 
// Unit Number and optionally port number.
// 

typedef struct _NDAS_DEVICE_IDENTIFIER {
	UCHAR Identifier[6];
	UCHAR UnitNumber;
	UCHAR Reserved;
	UCHAR Reserved2[8];
} NDAS_DEVICE_IDENTIFIER, *PNDAS_DEVICE_IDENTIFIER;

C_ASSERT(sizeof(NDAS_DEVICE_IDENTIFIER) == 16);

typedef struct _NDAS_GENERIC_ADDRESS {
	USHORT AddressLength;
	UCHAR Address[22];
} NDAS_GENERIC_ADDRESS, *PNDAS_GENERIC_ADDRESS;

C_ASSERT(sizeof(NDAS_GENERIC_ADDRESS) == 24);

typedef enum _NDAS_TRANSFER_MODE {
	NdasPIOMode,
	NdasMultiwordDMAMode0,
	NdasMultiwordDMAMode1,
	NdasMultiwordDMAMode2,
	NdasUltraDMAMode0,
	NdasUltraDMAMode1,
	NdasUltraDMAMode2,
	NdasUltraDMAMode3,
	NdasUltraDMAMode4,
	NdasUltraDMAMode5,
	NdasUltraDMAMode6,
	NdasUltraDMAMode7,
	NdasTransferModeAny = 0x0FFF,
	NdasTransferModeMaxMask = 0x8000
} NDAS_TRANSFER_MODE, *PNDAS_TRANSFER_MODE;

C_ASSERT(sizeof(NDAS_TRANSFER_MODE) == 4);

#define NDASLU_OPT_USE_FIXED_MAXIMUM_TRANSFER_LENGTH
#define NDASLU_OPT_USE_FIXED_TRANSFER_MODE

#define	NDAS_DISK_ENCRYPTION_MAX_KEY_LENGTH 64 // 64 bytes. 512bits.

// Must be same value in lanscsi.h
typedef enum _NDAS_DISK_ENCRYPTION_TYPE {
	NdasDiskEncryptionNone   = 0x00,
	NdasDiskEncryptionSimple = 0x01,
	NdasDiskEncryptionAES    = 0x02,
} NDAS_DISK_ENCRYPTION_TYPE;

C_ASSERT(sizeof(NDAS_DISK_ENCRYPTION_TYPE) == 4);

typedef struct _NDAS_DISK_ENCRYPTION_DESCRIPTOR {
	NDAS_DISK_ENCRYPTION_TYPE EncryptType;
	UCHAR EncryptKeyLength;
	UCHAR Reserved[3];
	UCHAR EncryptKey[NDAS_DISK_ENCRYPTION_MAX_KEY_LENGTH];
} NDAS_DISK_ENCRYPTION_DESCRIPTOR, *PNDAS_DISK_ENCRYPTION_DESCRIPTOR;

C_ASSERT(sizeof(NDAS_DISK_ENCRYPTION_DESCRIPTOR) == 72);

typedef enum _NDAS_DISK_FLAGS {
	NDAS_DISK_FLAG_LOCKED_WRITE     = 0x00000001,
	NDAS_DISK_FLAG_SHARED_WRITE     = 0x00000002,
	NDAS_DISK_FLAG_OOB_SHARED_WRITE = 0x00000004,
	NDAS_DISK_FLAG_LFS_SUPPORT      = 0x00000008,
	NDAS_DISK_FLAG_FAKE_WRITE       = 0x00000010,
	NDAS_DISK_FLAG_20_WRITE_VERIFY  = 0x00000020
} NDAS_DISK_FLAGS;

typedef enum _NDAS_DEVICE_ACCESS_MODE {
	NdasDeviceAccessUnspecified = 0x00,
	NdasDeviceAccessSharedRead = 0x01,
	NdasDeviceAccessSharedReadWrite = 0x02,
	NdasDeviceAccessExclusiveReadWrite = 0x03,
	NdasDeviceAccessSuperReadWrite = 0x04,
	_MakeNdasDeviceAccessModeAsULong = 0xFFFFFFFF
} NDAS_DEVICE_ACCESS_MODE;

typedef struct _NDAS_ATA_DEVICE_DESCRIPTOR {

	NDAS_LOGICALUNIT_DESCRIPTOR Header;
	NDAS_DEVICE_IDENTIFIER NdasDeviceId;

	/* Flag bit is effective only when the mask bit is set 
	   Otherwise, default values will be used */

	ULONG NdasDeviceFlagMask;
	ULONG NdasDeviceFlags;

	/* offset of the pointer to the struct LSP_TRANSPORT_ADDRESS 
	   from the start of the structure NDAS_ATA_DEVICE_DESCRIPTOR */
	ULONG DeviceAddressOffset; 
	
	/* count of the LocalAddresses of the pointer set by LocalAddressOffset,
	   Count should be less than or equal to 8. More than 8 entries 
	   will be ignored.
	   */
	ULONG LocalAddressCount;

	/* offset of the pointer to the struct LSP_TRANSPORT_ADDRESS 
	   from the start of the structure NDAS_ATA_DEVICE_DESCRIPTOR */
	ULONG LocalAddressOffset;

	/* GENERIC_READ | GENERIC_WRITE */
	ULONG AccessMode;

	/* Device Login Password */
	UCHAR DevicePassword[8];

	/* User-specific password, Only for NDAS 2.5 or later*/
	UCHAR UserPassword[16];

	/* Set 0 to use the actual hardware data */
	ULONG VirtualBytesPerBlock;
	
	/* Set 0 to use the actual hardware data */
	/* Negative values will be actual LBA less VLBA */
	LARGE_INTEGER VirtualLogicalBlockAddress;

	/* Logical partitioning support */
	LARGE_INTEGER StartLogicalBlockAddress;

	/* Disk Encryption Data Offset */
	ULONG NdasDiskEncryptionDescriptorOffset;

} NDAS_ATA_DEVICE_DESCRIPTOR, *PNDAS_ATA_DEVICE_DESCRIPTOR;

//
// SCSI MINIPORT IOCTL's for NDAS Logical Unit
//

typedef struct _NDAS_ATA_DEVICE_RUNNING_CONFIG {
	ULONG Size;
	ULONG CurrentFlags;
	ULONG CurrentAccessMode;
	/* LSP_TRANSPORT_ADDRESS */
	UCHAR DeviceAddress[32];
	/* LSP_TRANSPORT_ADDRESS */
	UCHAR LocalAddress[32];
} NDAS_ATA_DEVICE_RUNNING_CONFIG, *PNDAS_ATA_DEVICE_RUNNING_CONFIG;

#define IOCTL_SCSI_MINIPORT_NDAS_SIGNATURE "NDASDU40"
#define IOCTL_SCSI_MINIPORT_NDAS_GET_RUNNING_CONFIG ((FILE_DEVICE_SCSI << 16) + 0x05F0)

//
// Supporting down-level clients
//

#define NDASSCSI_DEVICE_TYPE 0x89e9
#define NDASSCSI_CTL_CODE(x) \
	CTL_CODE(NDASSCSI_DEVICE_TYPE, x, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define NDASSCSI_IOCTL_SIGNATURE "XIMETA_1"
#define NDASSCSI_IOCTL_GET_SLOT_NO			NDASSCSI_CTL_CODE(0x001)
#define NDASSCSI_IOCTL_UPGRADETOWRITE		NDASSCSI_CTL_CODE(0x009)

//
// NDAS_ATA_LINK_EVENT_GUID
//

typedef enum _NDAS_ATA_LINK_EVENT_TYPE {
	NDAS_ATA_LINK_UP,
	NDAS_ATA_LINK_DOWN,
	NDAS_ATA_LINK_CONNECTING,
} NDAS_ATA_LINK_EVENT_TYPE;

typedef struct _NDAS_ATA_LINK_EVENT {
	ULONG EventType;
	ULONG LogicalUnitAddress;
} NDAS_ATA_LINK_EVENT, *PNDAS_ATA_LINK_EVENT;

#include <poppack.h>

#ifdef __cplusplus
}
#endif

#endif /* _NDASDISK_IOCTL_H_ */
