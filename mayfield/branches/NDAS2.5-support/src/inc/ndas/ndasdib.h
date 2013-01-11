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
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BITMAP		= -0x0ff0;	// Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LWR			= -0x1000;	// Last written region(Optional)

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_XAREA			= 0x1000;	// Total X Area Size
//static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP			= 0x0800;	// Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP			= 0x0010;	// Corruption Bitmap(Optional) As of DIBv2.1.2, Bitmap size is max 8k(16 sector)
#define NDAS_BLOCK_SIZE_LAST_WRITTEN_REGION 0x0010
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_LWR		= NDAS_BLOCK_SIZE_LAST_WRITTEN_REGION;	// Last written region(Optional). As of DIBv2.1.2, LWR is max 8k.

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_MBR		= 0;		// Master Boot Record
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_USER		= 0x80;		// Partion 1 starts here
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
	unsigned _int8 VID;
} UNIT_DISK_LOCATION, *PUNIT_DISK_LOCATION;

C_ASSERT(8 == sizeof(UNIT_DISK_LOCATION));

#define NDAS_MAX_UNITS_IN_BIND	16 // not system limit
#define NDAS_MAX_UNITS_IN_V2 32
#define NDAS_MAX_UNITS_IN_SECTOR 64

static const unsigned _int64 NDAS_DIB_V2_SIGNATURE = 0x3F404A9FF4495F9F;
//#define	DISK_INFORMATION_SIGNATURE_V2	0x3F404A9FF4495F9F
static const unsigned _int32 NDAS_DIB_VERSION_MAJOR_V2 = 1;
static const unsigned _int32 NDAS_DIB_VERSION_MINOR_V2 = 1; // increased to 1.2 to support out-of-sync bitmap, LWR, RAID5

#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT (1<<16)	// as of 2.1.2. Before that 128


#define NMT_INVALID			0		// operation purpose only : must NOT written on disk
#define NMT_SINGLE			1		// unbound
#define NMT_MIRROR			2		// 2 disks without repair information. need to be converted to RAID1
#define NMT_SAFE_RAID1		7		// operation purpose only(add mirror) : must NOT written on disk. used in bind operation only
#define NMT_AGGREGATE		3		// 2 or more
#define NMT_RAID0			6		// with repair
#define NMT_RAID1			4		// with repair
#define NMT_RAID4			5		// with repair
#define NMT_RAID1R2			10		// with repair, from 3.11
#define NMT_RAID4R2			11		// with repair, from 3.11. Never released
#define NMT_RAID1R3			12		// with DRAID & persistent OOS bitmap. Added as of 3.20
#define NMT_RAID4R3			13		// with DRAID & persistent OOS bitmap. Added as of 3.20. Not implemented yet.
#define NMT_AOD				8		// Append only disk
#define NMT_VDVD			100		// virtual DVD
#define NMT_CDROM			101		// packet device, CD / DVD
#define NMT_OPMEM			102		// packet device, Magnetic Optical
#define NMT_FLASH			103		// block device, flash card

/*
	Because prior NDAS service overwrites NDAS_BLOCK_LOCATION_DIB_V1 if Signature, version does not match,
	NDAS_DIB_V2 locates at NDAS_BLOCK_LOCATION_DIB_V2(-2).
*/

// Disk Information Block V2
typedef struct _NDAS_DIB_V2 {
	union{
		struct {
			unsigned _int64	Signature;
			unsigned _int32	MajorVersion;
			unsigned _int32	MinorVersion;

			// sizeXArea + sizeLogicalSpace <= sizeTotalDisk
			unsigned _int64	sizeXArea; // in sectors, always 2 * 1024 * 2 (2 MB)

			unsigned _int64	sizeUserSpace; // in sectors

			unsigned _int32	iSectorsPerBit; // dirty bitmap bit unit. default : 128(2^7). Passed to RAID system through service.
										// default 64 * 1024(2^16, 32Mbyte per bit) since RAIDv2.1.2

			unsigned _int32	iMediaType; // NDAS Media Type
			unsigned _int32	nDiskCount; // 1 ~ . physical disk units
			unsigned _int32	iSequence; // 0 ~
			unsigned _int8	AutoRecover; // Recover RAID automatically. Not used
			unsigned _int8	Reserved0[3 + 8 + 8 + 4];
//			unsigned _int64	HVDiskLen; // Not used
//			unsigned _int64	AodSblockLoc; // Not used
//			unsigned _int32	AodSblockLen; // Not used
			unsigned _int32	nSpareCount; // 0 ~ . used for fault tolerant RAID
			unsigned _int8		Reserved2[52]; // Byte 76
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

#if 0
// LAST_WRITTEN_SECTOR, LAST_WRITTEN_SECTORS is not used anymore.
// This structures is provided for migration only
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


//
// All number is in little-endian
//
typedef struct _NDAS_LAST_WRITTEN_REGION_ENTRY {
	unsigned _int64 Address;
	unsigned _int32 Length;
	unsigned _int32 Reserved;
} NDAS_LAST_WRITTEN_REGION_ENTRY, *PNDAS_LAST_WRITTEN_REGION_ENTRY;
C_ASSERT((8 + 4 + 4) == sizeof(NDAS_LAST_WRITTEN_REGION_ENTRY)); // 16

// All number is in little-endian
#define NDAS_LWR_ENTRY_PER_BLOCK 30
typedef struct _NDAS_LAST_WRITTEN_REGION_BLOCK {
	unsigned _int64 SequenceNum; // Increased when LWR is rewritten. Used to check LWR integrity.
	unsigned _int16 LwrEntryCount; // Number of all LWR entries. Valid only for first LWR block
	unsigned _int8 LwrSectorCount; // Number of sectors that LWR resides. Max 16. Valid only for first LWR block
	unsigned _int8 Reserved1[5];	// 16
	NDAS_LAST_WRITTEN_REGION_ENTRY	Entry[NDAS_LWR_ENTRY_PER_BLOCK];
	unsigned _int8 Reserved2[8];	// 
	unsigned _int64 SequenceNumTail; // Used to check whether this block is partially updated.
} NDAS_LAST_WRITTEN_REGION_BLOCK, *PNDAS_LAST_WRITTEN_REGION_BLOCK;
C_ASSERT( 512 == sizeof(NDAS_LAST_WRITTEN_REGION_BLOCK));


// All number is in little-endian
typedef struct _NDAS_OOS_BITMAP_BLOCK {
	unsigned _int64 SequenceNumHead; // Increased when this block is updated.
	UCHAR Bits[512-16];
	unsigned _int64 SequenceNumTail; // Same value of SequenceNumHead. If two value is not matched, this block is corrupted. 
} NDAS_OOS_BITMAP_BLOCK, *PNDAS_OOS_BITMAP_BLOCK;
C_ASSERT( 512 == sizeof(NDAS_OOS_BITMAP_BLOCK));
#define NDAS_BYTE_PER_OOS_BITMAP_BLOCK  (512-16)
#define NDAS_BIT_PER_OOS_BITMAP_BLOCK  (NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8)

/*
member of the NDAS_RAID_META_DATA structure
8 bytes of size
*/
#define NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED	(1<<0) // Disk is not synced because of network down or new disk
#define NDAS_UNIT_META_BIND_STATUS_SPARE		(1<<1)
#define NDAS_UNIT_META_BIND_STATUS_BAD_DISK	(1<<2)
#define NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR	(1<<3)
#define NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE (1<<4)
#define NDAS_UNIT_META_BIND_STATUS_DEFECTIVE	(NDAS_UNIT_META_BIND_STATUS_BAD_DISK |\
													NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR|\
													NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)

typedef struct _NDAS_UNIT_META_DATA {
	unsigned _int16	iUnitDeviceIdx; // Index in NDAS_DIB_V2.UnitDisks
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


#define NDAS_RAID_META_DATA_SIGNATURE 0xA01B210C10CDC301

#define NDAS_RAID_META_DATA_STATE_MOUNTED 		(1<<0)
#define NDAS_RAID_META_DATA_STATE_UNMOUNTED (1<<1)
#define NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED (1<<2)

#define NDAS_DRAID_ARBITER_TYPE_NONE		0	// no arbiter available
#define NDAS_DRAID_ARBITER_TYPE_LPX		1	

#define NDAS_DRAID_ARBITER_ADDR_COUNT	8
typedef struct _NDAS_RAID_META_DATA {
	union {
		struct {
			unsigned _int64	Signature;	// Little Endian
			GUID			guid;
			unsigned _int32 uiUSN; // Universal sequence number increases 1 each writes.
			unsigned _int32 state; // Ored value of NDAS_RAID_META_DATA_STATE_*
			GUID			ConfigSetId;	// Added as of 3.20. 

			//
			// DRAID Arbiter node information. Updated when state is changed to mounted.
			//
			struct {
				unsigned char   Type; // Type of RAID master node. LPX/IP, running mode, etc.
								// Currently NDAS_DRAID_ARBITER_TYPE_LPX and NDAS_DRAID_ARBITER_TYPE_NONE only.
				unsigned char   Reserved[3];
				unsigned char	  Addr[8]; // MAC or IP. Need to be at least 16 byte to support IPv6?
			} ArbiterInfo[NDAS_DRAID_ARBITER_ADDR_COUNT];	// Assume max 8 network interface.
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
