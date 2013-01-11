#ifndef _NDAS_DIB_H_
#define _NDAS_DIB_H_

#pragma once

//
//	disk information format
//
//

/* Turn 1-byte structure alignment on */
/* Use poppack.h to restore previous or default alignment */
#include <pshpack1.h>

//
// cslee:
//
// Disk Information Block should be aligned to 512-byte (1 sector size)
//
// Checksum has not been used yet, so we can safely fix the size of
// the structure at this time without increasing the version of the block structure
//
// Note when you change members of the structure, you should increase the
// version of the structure and you SHOULD provide the migration
// path from the previous version.
// 

static const unsigned _int32 SECTOR_SIZE = 512;

typedef unsigned _int64 NDAS_BLOCK_LOCATION;
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V1		= -1;		// Disk Information Block V1
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V2		= -2;		// Disk Information Block V2
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ENCRYPT	= -3;		// Content encryption information
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_RMD		= -4;		// RAID Meta data
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_RMD_T		= -5;		// RAID Meta data(for transaction)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_OEM		= -0x0100;	// OEM Reserved
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ADD_BIND	= -0x0200;	// Additional bind informations
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BACL		= -0x0204;	// Block Access Control List
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BITMAP		= -0x0ff0;	// Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LWR_REV1	= -0x1000;	// Last written region(Optional). Used only by 3.10 RAID1 rev.1.

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_XAREA			= 0x1000;	// Total X Area Size
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP_REV1	= 0x0800;	// Corruption Bitmap(Optional)		
//#define NDAS_BLOCK_SIZE_BITMAP	0x0010							// Corruption Bitmap(Optional) As of 3.20, Bitmap size is max 8k(16 sector)
#define NDAS_BLOCK_SIZE_BITMAP	0x0040							// Corruption Bitmap(Optional) As of 3.20, Bitmap size is max 32k(64 sector)
//static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP		= 0x0010;	// Corruption Bitmap(Optional)

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_LWR_REV1		= 0x0001;	// Last written region. Used only by 3.10 RAID1 rev.1

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_MBR		= 0;		// Master Boot Record
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_USER		= 0x0;		// Partition 1 starts here
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LDM		= 0;		// depends on sizeUserSpace, last 1MB of user space

typedef unsigned _int8 NDAS_DIB_DISK_TYPE;

#define NDAS_DIB_DISK_TYPE_SINGLE				0
#define NDAS_DIB_DISK_TYPE_MIRROR_MASTER		1
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST	2
#define NDAS_DIB_DISK_TYPE_MIRROR_SLAVE			11
#define NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND	21
#define NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD	22
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH	23
//#define NDAS_DIB_DISK_TYPE_DVD		31
#define NDAS_DIB_DISK_TYPE_VDVD			32
//#define NDAS_DIB_DISK_TYPE_MO			33
//#define NDAS_DIB_DISK_TYPE_FLASHCATD	34
#define NDAS_DIB_DISK_TYPE_INVALID	0x80
#define NDAS_DIB_DISK_TYPE_BIND		0xC0

// extended type information is stored in DIBEXT
#define NDAS_DIB_DISK_TYPE_EXTENDED				80

typedef unsigned _int8 NDAS_DIBEXT_DISK_TYPE;

static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_SINGLE = 0;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_MASTER = 1;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FIRST	= 2;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_SLAVE = 11;

typedef unsigned char NDAS_DIB_DISKTYPE;
typedef unsigned char NDAS_DIBEXT_DISKTYPE;

#define NDAS_DIBEXT_SIGNATURE 0x3F404A9FF4495F9F

//
// obsolete types (NDAS_DIB_USAGE_TYPE)
//
#define NDAS_DIB_USAGE_TYPE_HOME	0x00
#define NDAS_DIB_USAGE_TYPE_OFFICE	0x10

static const unsigned _int32 NDAS_DIB_SIGNATURE = 0xFE037A4E;
static const unsigned _int8 NDAS_DIB_VERSION_MAJOR = 0;
static const unsigned _int8 NDAS_DIB_VERSION_MINOR = 1;

typedef struct _NDAS_DIB {

	unsigned _int32	Signature;		// 4 (4)
	
	unsigned _int8	MajorVersion;	// 1 (5)
	unsigned _int8	MinorVersion;	// 1 (6)
	unsigned _int8	reserved1[2];	// 1 * 2 (8)

	unsigned _int32	Sequence;		// 4 (12)

	unsigned _int8	EtherAddress[6];	// 1 * 6 (18)
	unsigned _int8	UnitNumber;		// 1 (19)
	unsigned _int8	reserved2;		// 1 (20)

	unsigned _int8	DiskType;		// 1 (21)
	unsigned _int8	PeerAddress[6];	// 1 * 6 (27)
	unsigned _int8	PeerUnitNumber; // 1 (28)
	unsigned _int8	reserved3;		// 1 (29)

	unsigned _int8	UsageType;		// 1 (30)
	unsigned _int8	reserved4[3];	// 1 * 3 (33)

	unsigned char reserved5[512 - 37]; // should be 512 - ( 33 + 4 )

	unsigned _int32	Checksum;		// 4 (512)

} NDAS_DIB, *PNDAS_DIB;

C_ASSERT(512 == sizeof(NDAS_DIB));

typedef struct _UNIT_DISK_INFO
{
	unsigned _int8 HwVersion:4;	// Used to determine capability of offline RAID member.
	unsigned _int8 Reserved1:4;
	unsigned _int8 Reserved2;
} UNIT_DISK_INFO, *PUNIT_DISK_INFO;

C_ASSERT(2 == sizeof(UNIT_DISK_INFO));


typedef struct _UNIT_DISK_LOCATION
{
	unsigned _int8 MACAddr[6];
	unsigned _int8 UnitNumber;
	unsigned _int8 VID; // vendor ID
} UNIT_DISK_LOCATION, *PUNIT_DISK_LOCATION;

C_ASSERT(8 == sizeof(UNIT_DISK_LOCATION));

typedef struct _BLOCK_ACCESS_CONTROL_LIST_ELEMENT {
#define BACL_ACCESS_MASK_WRITE				0x01
#define BACL_ACCESS_MASK_READ				0x02
#define BACL_ACCESS_MASK_EMBEDDED_SYSTEM	0x80
#define BACL_ACCESS_MASK_PC_SYSTEM			0x40
	unsigned _int8	AccessMask;
	unsigned _int8	Reserved[3 + 4 + 8];
	unsigned _int64	ui64StartSector;	
	unsigned _int64	ui64SectorCount;	// ignored if 0 == ui64SectorCount
} BLOCK_ACCESS_CONTROL_LIST_ELEMENT, *PBLOCK_ACCESS_CONTROL_LIST_ELEMENT;
C_ASSERT(32 == sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT));

static const unsigned _int32 BACL_SIGNATURE = 0x3C1C49B2;
static const unsigned _int32 BACL_VERSION = 0x00000001;

typedef struct _BLOCK_ACCESS_CONTROL_LIST {
	unsigned _int32	Signature;
	unsigned _int32	Version;
	unsigned _int32	crc; // crc of Elements
	unsigned _int32	ElementCount;
	unsigned _int8	Reserved[16];

	BLOCK_ACCESS_CONTROL_LIST_ELEMENT Elements[1];
} BLOCK_ACCESS_CONTROL_LIST, *PBLOCK_ACCESS_CONTROL_LIST;
C_ASSERT(64 == sizeof(BLOCK_ACCESS_CONTROL_LIST));

#define BACL_SIZE(ELEMENT_COUNT) (sizeof(BLOCK_ACCESS_CONTROL_LIST) + sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT) * ((ELEMENT_COUNT) -1))
#define BACL_SECTOR_SIZE(ELEMENT_COUNT) ((BACL_SIZE(ELEMENT_COUNT) +511)/ 512)

#define NDAS_MAX_UNITS_IN_BIND		16 // not system limit
#define NDAS_MAX_UNITS_IN_V2		32
#define NDAS_MAX_UNITS_IN_SECTOR	64

static const unsigned _int64 NDAS_DIB_V2_SIGNATURE = 0x3F404A9FF4495F9F;
//#define	DISK_INFORMATION_SIGNATURE_V2	0x3F404A9FF4495F9F
static const unsigned _int32 NDAS_DIB_VERSION_MAJOR_V2 = 1;
static const unsigned _int32 NDAS_DIB_VERSION_MINOR_V2 = 2; 

//#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT	(1<<16)		// 32Mbytes. Changed to 32M since 3.20
#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT_LOG	13		// 4Mbytes. Changed to 4M since 3.30
#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT	(1<<NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT_LOG)		// 4Mbytes. Changed to 4M since 3.30
#define NDAS_USER_SPACE_ALIGN				(128)		// 3.11 version required 128 sector alignment of user addressable space.
														// Do not change the value to keep compatiblity with NDAS devices 
														// formatted under NDAS software version 3.11.

typedef enum _NDAS_MEDIA_TYPE {
	NMT_INVALID			=0x00,		// operation purpose only : must NOT written on disk
	NMT_SINGLE			=0x01,		// unbound
	NMT_MIRROR			=0x02,		// 2 disks without repair information. need to be converted to RAID1
	NMT_AGGREGATE		=0x03,		// 2 or more
	NMT_RAID1			=0x04,		// with repair
	NMT_RAID4			=0x05,		// with repair. Never released.
	NMT_RAID0			=0x06,		// with repair. RMD is not used. Since the block size of this raid set is 512 * n, Mac OS X don't support this type.
	NMT_SAFE_RAID1		=0x07,		// operation purpose only(add mirror) : must NOT written on disk. used in bind operation only
	NMT_AOD				=0x08,		// Append only disk
	NMT_RAID0R2			=0x09,		// Block size is 512B not 512 * n. RMD is used.
	NMT_RAID1R2			=0x0A,	// with repair, from 3.11
	NMT_RAID4R2			=0x0B,	// with repair, from 3.11. Never released
	NMT_RAID1R3			=0x0C,	// with DRAID & persistent OOS bitmap. Added as of 3.20
	NMT_RAID4R3			=0x0D,	// with DRAID & persistent OOS bitmap. Added as of 3.21. Not implemented yet.
	NMT_RAID5			=0x0E,	// with DRAID & persistent OOS bitmap. Added as of 3.21. Not implemented yet.
	NMT_SAFE_AGGREGATE	=0x0F,	// operation purpose only(append disk to single disk) : must NOT written on disk. used in bind operation only
	NMT_VDVD			=0x64,	// virtual DVD
	NMT_CDROM			=0x65,	// packet device, CD / DVD
	NMT_OPMEM			=0x66,	// packet device, Magnetic Optical
	NMT_FLASH			=0x67,	// block device, flash card
	NMT_CONFLICT		=0xff,	// DIB information is conflicting with actual NDAS information. Used for internal purpose. Must not be written to disk.
	// To do: reimplement DIB read functions without using this value..
} NDAS_MEDIA_TYPE, *PNDAS_MEDIA_TYPE;

/*
	Because prior NDAS service overwrites NDAS_BLOCK_LOCATION_DIB_V1 if Signature, version does not match,
	NDAS_DIB_V2 locates at NDAS_BLOCK_LOCATION_DIB_V2(-2).
*/

// Disk Information Block V2
typedef struct _NDAS_DIB_V2 {
	union{
		struct {
			unsigned _int64	Signature;		// Byte 0
			unsigned _int32	MajorVersion;	// Byte 8
			unsigned _int32	MinorVersion;	// Byte 12

			// sizeXArea + sizeLogicalSpace <= sizeTotalDisk
			unsigned _int64	sizeXArea; // Byte 16. in sectors, always 2 * 1024 * 2 (2 MB)

			unsigned _int64	sizeUserSpace; // Byte 24. in sectors

			unsigned _int32	iSectorsPerBit; // Byte 32. dirty bitmap bit unit. default : 128(2^7). Passed to RAID system through service.
										    // default 64 * 1024(2^16, 32Mbyte per bit) since 3.20
										    // default 8 * 1024(2^16, 32Mbyte per bit) since 3.30
			NDAS_MEDIA_TYPE	iMediaType; //  Byte 36. NDAS Media Type. NMT_*
			unsigned _int32	nDiskCount; // Byte 40. 1 ~ . physical disk units
			unsigned _int32	iSequence; // Byte 44. 0 ~. Sequence number of this unit in UnitDisks list.
//			unsigned _int8	AutoRecover; // Recover RAID automatically. Obsoleted
			unsigned _int8	Reserved0[4]; // Byte 48.
			unsigned _int32	BACLSize;	// Byte 52.In byte. Do not access BACL if zero

#define		NDAS_RAID_DEFAULT_START_OFFSET	(0x80)
			unsigned _int64 sizeStartOffset; // byte 56. sizeStartOffset in physical address is 0 in logical NDAS drive

			unsigned _int8	Reserved1[8]; // Byte 64
			unsigned _int32	nSpareCount; // Byte 72.  0 ~ . used for fault tolerant RAID
			unsigned _int8	Reserved2[52]; // Byte 76
			UNIT_DISK_INFO 	UnitDiskInfos[NDAS_MAX_UNITS_IN_V2]; // Byte 128. Length 64.
		};
		unsigned char bytes_248[248];
	}; // 248
	unsigned _int32 crc32; // 252
	unsigned _int32 crc32_unitdisks; // 256

	UNIT_DISK_LOCATION	UnitDisks[NDAS_MAX_UNITS_IN_V2]; // 256 bytes
} NDAS_DIB_V2, *PNDAS_DIB_V2;

C_ASSERT(512 == sizeof(NDAS_DIB_V2));

#ifdef __PASS_DIB_CRC32_CHK__
#define IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) TRUE
#define IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB) TRUE
#else
#define IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) ((DIB).crc32 == CRC_FUNC((unsigned char *)&(DIB), sizeof((DIB).bytes_248)))
#define IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB) ((DIB).crc32_unitdisks == CRC_FUNC((unsigned char *)&(DIB).UnitDisks, sizeof((DIB).UnitDisks)))
#endif

#define IS_DIB_CRC_VALID(CRC_FUNC, DIB) (IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) && IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB))

#define SET_DIB_DATA_CRC(CRC_FUNC, DIB) ((DIB).crc32 = CRC_FUNC((unsigned char *)&(DIB), sizeof((DIB).bytes_248)))
#define SET_DIB_UNIT_CRC(CRC_FUNC, DIB) ((DIB).crc32_unitdisks = CRC_FUNC((unsigned char *)&(DIB).UnitDisks, sizeof((DIB).UnitDisks)))
#define SET_DIB_CRC(CRC_FUNC, DIB) {SET_DIB_DATA_CRC(CRC_FUNC, DIB); SET_DIB_UNIT_CRC(CRC_FUNC, DIB);}
//
// Unless ContentEncryptCRC32 matches the CRC32,
// no ContentEncryptXXX fields are valid.
//
// CRC32 is Only for ContentEncrypt Fields (LITTLE ENDIAN)
// CRC32 is calculated based on the following criteria
//
// p = ContentEncryptMethod;
// crc32(p, sizeof(ContentEncryptMethod + KeyLength + Key)
//
//
// Key is encrypted with DES to the size of
// NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH
// Unused portion of the ContentEncryptKey must be filled with 0
// before encryption
//
// UCHAR key_value[8] = {0x0B,0xBC,0xAB,0x45,0x44,0x01,0x65,0xF0};
//

#define	NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH		64		// 64 bytes. 512bits.
#define	NDAS_CONTENT_ENCRYPT_METHOD_NONE	0
#define	NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE	1
#define	NDAS_CONTENT_ENCRYPT_METHOD_AES		2

#define NDAS_CONTENT_ENCRYPT_REV_MAJOR	0x0010
#define NDAS_CONTENT_ENCRYPT_REV_MINOR	0x0010

#define NDAS_CONTENT_ENCRYPT_REVISION \
	((NDAS_CONTENT_ENCRYPT_REV_MAJOR << 16) + NDAS_CONTENT_ENCRYPT_REV_MINOR)

static const unsigned _int64 NDAS_CONTENT_ENCRYPT_BLOCK_SIGNATURE = 0x76381F4C2DF34D7D;

typedef struct _NDAS_CONTENT_ENCRYPT_BLOCK {
	union{
		struct {
			unsigned _int64	Signature;	// Little Endian
			// INVARIANT
			unsigned _int32 Revision;   // Little Endian
			// VARIANT BY REVISION (MAJOR)
			unsigned _int16	Method;		// Little Endian
			unsigned _int16	Reserved_0;	// Little Endian
			unsigned _int16	KeyLength;	// Little Endian
			unsigned _int16	Reserved_1;	// Little Endian
			union{
				unsigned _int8	Key32[32 /8];
				unsigned _int8	Key128[128 /8];
				unsigned _int8	Key192[192 /8];
				unsigned _int8	Key256[256 /8];
				unsigned _int8	Key[NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH];
			};
			unsigned _int8 Fingerprint[16]; // 128 bit, KeyPairMD5 = md5(ks . kd)
		};
		unsigned char bytes_508[508];
	};
	unsigned _int32 CRC32;		// Little Endian
} NDAS_CONTENT_ENCRYPT_BLOCK, *PNDAS_CONTENT_ENCRYPT_BLOCK;

C_ASSERT(512 == sizeof(NDAS_CONTENT_ENCRYPT_BLOCK));

#define IS_NDAS_DIBV1_WRONG_VERSION(DIBV1) \
	((0 != (DIBV1).MajorVersion) || \
	(0 == (DIBV1).MajorVersion && 1 != (DIBV1).MinorVersion))

#define IS_HIGHER_VERSION_V2(DIBV2) \
	((NDAS_DIB_VERSION_MAJOR_V2 < (DIBV2).MajorVersion) || \
	(NDAS_DIB_VERSION_MAJOR_V2 == (DIBV2).MajorVersion && \
	NDAS_DIB_VERSION_MINOR_V2 < (DIBV2).MinorVersion))

#define GET_TRAIL_SECTOR_COUNT_V2(DISK_COUNT) \
	(	((DISK_COUNT) > NDAS_MAX_UNITS_IN_V2) ? \
		(((DISK_COUNT) - NDAS_MAX_UNITS_IN_V2) / NDAS_MAX_UNITS_IN_SECTOR) +1 : 0)

// make older version should not access disks with NDAS_DIB_V2
#define NDAS_DIB_V1_INVALIDATE(DIB_V1)									\
	{	(DIB_V1).Signature		= NDAS_DIB_SIGNATURE;					\
		(DIB_V1).MajorVersion	= NDAS_DIB_VERSION_MAJOR;				\
		(DIB_V1).MinorVersion	= NDAS_DIB_VERSION_MINOR;				\
		(DIB_V1).DiskType		= NDAS_DIB_DISK_TYPE_INVALID;	}


#if 0 // LAST_WRITTEN_SECTOR, LAST_WRITTEN_SECTORS is not used anymore.
typedef struct _LAST_WRITTEN_SECTOR {
	UINT64	logicalBlockAddress;
	UINT32	transferBlocks;
	ULONG	timeStamp;
} LAST_WRITTEN_SECTOR, *PLAST_WRITTEN_SECTOR;

C_ASSERT(16 == sizeof(LAST_WRITTEN_SECTOR));

typedef struct _LAST_WRITTEN_SECTORS {
	LAST_WRITTEN_SECTOR LWS[32];
} LAST_WRITTEN_SECTORS, *PLAST_WRITTEN_SECTORS;

C_ASSERT(512 == sizeof(LAST_WRITTEN_SECTORS));
#endif

// All number is in little-endian


#define NDAS_BYTE_PER_OOS_BITMAP_BLOCK  (512-16)
#define NDAS_BIT_PER_OOS_BITMAP_BLOCK  (NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8)

typedef struct _NDAS_OOS_BITMAP_BLOCK {

	unsigned _int64 SequenceNumHead; // Increased when this block is updated.
	UCHAR			Bits[NDAS_BYTE_PER_OOS_BITMAP_BLOCK];
	unsigned _int64 SequenceNumTail; // Same value of SequenceNumHead. If two value is not matched, this block is corrupted. 

} NDAS_OOS_BITMAP_BLOCK, *PNDAS_OOS_BITMAP_BLOCK;

C_ASSERT( 512 == sizeof(NDAS_OOS_BITMAP_BLOCK));

/*
member of the NDAS_RAID_META_DATA structure
8 bytes of size
*/

typedef enum _NDAS_UNIT_MEDIA_BIND_STATUS_FLAGS {
	// Disk is not synced because of network down or new disk
	// NDAS_UNIT_META_BIND_STATUS_FAULT at RAID1R2
	NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED = 0x00000001, 
	NDAS_UNIT_META_BIND_STATUS_SPARE = 0x00000002,
	NDAS_UNIT_META_BIND_STATUS_BAD_DISK = 0x00000004,
	NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR = 0x00000008,
	NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE = 0x00000010,
	NDAS_UNIT_META_BIND_STATUS_OFFLINE = 0x00000020,
	NDAS_UNIT_META_BIND_STATUS_DEFECTIVE = 
		NDAS_UNIT_META_BIND_STATUS_BAD_DISK |
		NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR |
		NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE,
} NDAS_UNIT_MEDIA_BIND_STATUS_FLAGS;

typedef struct _NDAS_UNIT_META_DATA {
	unsigned _int16		iUnitDeviceIdx;	 	// Index in NDAS_DIB_V2.UnitDisks
	unsigned _int8		UnitDeviceStatus; 	// NDAS_UNIT_META_BIND_STATUS_*
	unsigned _int8		Reserved[5];
} NDAS_UNIT_META_DATA, *PNDAS_UNIT_META_DATA;

C_ASSERT(8 == sizeof(NDAS_UNIT_META_DATA));

/*
NDAS_RAID_META_DATA structure contains status which changes by the change
of status of the RAID. For stable status, see NDAS_DIB_V2
All data is in littlen endian.

NDAS_RAID_META_DATA has 512 bytes of size

NDAS_RAID_META_DATA is for Fault tolerant bind only. ATM, RAID 1 and 4.
*/


#define NDAS_RAID_META_DATA_SIGNATURE 0xA01B210C10CDC301UL

#define NDAS_RAID_META_DATA_STATE_MOUNTED 			(1<<0)
#define NDAS_RAID_META_DATA_STATE_UNMOUNTED 		(1<<1)
#define NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED	(1<<2)	// This unit has ever been used in degraded mode.
															// Used to check independently updated RAID members
															// Not valid if disk is spare disk
#define NDAS_RAID_ARBITRATOR_TYPE_NONE	0					// no arbiter available
#define NDAS_RAID_ARBITRATOR_TYPE_LPX	1	

#define NDAS_RAID_ARBITRATOR_ADDR_COUNT	8

typedef struct _NDAS_RAID_META_DATA {
	union {
		struct {
			unsigned _int64	Signature;	// Little Endian
			GUID			RaidSetId;	// Prior RaidSetId
								// RaidSetId is created when RAID is created 
								//		and not changed until RAID configuration is changed by bindtool.
								// Both RaidSetId and ConfigSetId should be matched to work as RAID member
			unsigned _int32 uiUSN; // Universal sequence number increases 1 each writes.
			unsigned _int32 state; // Ored value of NDAS_RAID_META_DATA_STATE_*
			GUID			ConfigSetId;	// Added as of 3.20. For previous version this is 0.
					// Set when RAID is created and changed 
					// when RAID is reconfigured online with missing member.
					// Reconfiguration occurs when spare member is changed to active member to rule out missing member
					//	or on-line reconfiguration occurs.(Currently not implemented)
					// Meaningful only for RAID1 and RAID4/5 with 2+1 configuration with spare.

			//
			// DRAID Arbiter node information. Updated when state is changed to mounted.
			// Used only for redundent RAID.
			// Added as of 3.20
			//
			struct {
				unsigned char   Type; // Type of RAID master node. LPX/IP, running mode, etc.
								// Currently NDAS_RAID_ARBITRATOR_TYPE_LPX and NDAS_RAID_ARBITRATOR_TYPE_NONE only.
				unsigned char   Reserved[3];
				unsigned char	  Addr[8]; // MAC or IP. Need to be at least 16 byte to support IPv6?
			} ArbitratorInfo[NDAS_RAID_ARBITRATOR_ADDR_COUNT];	// Assume max 8 network interface.
			GUID			RaidSetIdBackup;
		};
		unsigned char bytes_248[248];
	}; // 248
	unsigned _int32 crc32; // 252
	unsigned _int32 crc32_unitdisks; // 256

	NDAS_UNIT_META_DATA UnitMetaData[NDAS_MAX_UNITS_IN_V2]; // 256 bytes
					// This array is always sorted by its member role in RAID.
} NDAS_RAID_META_DATA, *PNDAS_RAID_META_DATA;

C_ASSERT(512 == sizeof(NDAS_RAID_META_DATA));

#ifdef __PASS_RMD_CRC32_CHK__
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) TRUE
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) TRUE
#else
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32 == CRC_FUNC((unsigned char *)&(RMD), sizeof((RMD).bytes_248)))
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32_unitdisks == CRC_FUNC((unsigned char *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData)))
#endif

#define IS_RMD_CRC_VALID(CRC_FUNC, RMD) (IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) && IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD))

#define SET_RMD_DATA_CRC(CRC_FUNC, RMD) ((RMD).crc32 = CRC_FUNC((unsigned char *)&(RMD), sizeof((RMD).bytes_248)))
#define SET_RMD_UNIT_CRC(CRC_FUNC, RMD) ((RMD).crc32_unitdisks = CRC_FUNC((unsigned char *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData)))
#define SET_RMD_CRC(CRC_FUNC, RMD) {SET_RMD_DATA_CRC(CRC_FUNC, RMD); SET_RMD_UNIT_CRC(CRC_FUNC, RMD);}

//#define RMD_IDX_RECOVER_INFO(DISK_COUNT, IDX_DEFECTED) (((DISK_COUNT) -1 == (IDX_DEFECTED)) ? 0 : (IDX_DEFECTED) +1)
//#define RMD_IDX_DEFEDTED(DISK_COUNT, IDX_RECOVER_INFO) ((0 == (IDX_RECOVER_INFO)) ? (DISK_COUNT) -1 : (IDX_RECOVER_INFO) -1)
#define RMD_IDX_RECOVER_INFO(DISK_COUNT, IDX_DEFECTED) (((IDX_DEFECTED) +1) % (DISK_COUNT))
#define RMD_IDX_DEFEDTED(DISK_COUNT, IDX_RECOVER_INFO) (((IDX_RECOVER_INFO) + (DISK_COUNT) -1) % (DISK_COUNT))


/* Turn 1-byte structure alignment off */
#include <poppack.h>

#endif /* _NDAS_DIB_H_ */
