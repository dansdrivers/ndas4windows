#ifndef _NDAS_DIB_H_
#define _NDAS_DIB_H_

#pragma once

//#include <ntintsafe.h>


//	disk information format


// Turn 1-byte structure alignment on 
// Use poppack.h to restore previous or default alignment 

#include <pshpack1.h>

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
 

static const UINT32 SECTOR_SIZE = 512;

typedef INT64 NDAS_BLOCK_LOCATION;

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V1		= -1;		// Disk Information Block V1
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V2		= -2;		// Disk Information Block V2
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ENCRYPT	= -3;		// Content encryption information
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_RMD		= -4;		// RAID Meta data
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_RMD_T		= -5;		// RAID Meta data(for transaction)

#define NDAS_BLOCK_LOCATION_META_DATA	NDAS_BLOCK_LOCATION_RMD_T			// RAID Meta data

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_OEM		= -0x0100;	// OEM Reserved
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ADD_BIND	= -0x0200;	// Additional bind informations
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BACL		= -0x0204;	// Block Access Control List
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BITMAP		= -0x0ff0;	// Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LWR_REV1	= -0x1000;	// Last written region(Optional). Used only by 3.10 RAID1 rev.1.

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_XAREA			= 0x1000;	// Total X Area Size
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP_REV1	= 0x0800;	// Corruption Bitmap(Optional)		

#if 0
#define NDAS_BLOCK_SIZE_BITMAP	0x0010							// Corruption Bitmap(Optional) As of 3.20, Bitmap size is max 8k(16 sector)
#else
#define NDAS_BLOCK_SIZE_BITMAP	0x0040							// Corruption Bitmap(Optional) As of 3.20, Bitmap size is max 32k(64 sector)
#endif

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_LWR_REV1		= 0x0001;	// Last written region. Used only by 3.10 RAID1 rev.1

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_MBR		= 0;		// Master Boot Record
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_USER		= 0x80;		// Partition 1 starts here
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LDM		= 0;		// depends on sizeUserSpace, last 1MB of user space

typedef UINT8 NDAS_DIB_DISK_TYPE;

#define NDAS_DIB_DISK_TYPE_SINGLE				0x00
#define NDAS_DIB_DISK_TYPE_MIRROR_MASTER		0x01
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST	0x02
#define NDAS_DIB_DISK_TYPE_MIRROR_SLAVE			0x0B
#define NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND	0x15
#define NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD	0x16
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH	0x17
//#define NDAS_DIB_DISK_TYPE_DVD				0x1F
#define NDAS_DIB_DISK_TYPE_VDVD					0x20
//#define NDAS_DIB_DISK_TYPE_MO					0x21
//#define NDAS_DIB_DISK_TYPE_FLASHCATD			0x22
#define NDAS_DIB_DISK_TYPE_INVALID				0x80
#define NDAS_DIB_DISK_TYPE_BIND					0xC0

// extended type information is stored in DIBEXT

#define NDAS_DIB_DISK_TYPE_EXTENDED				0x50


typedef UINT8 NDAS_DIBEXT_DISK_TYPE;

static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_SINGLE				= 0;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_MASTER		= 1;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FIRST	= 2;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_SLAVE		= 11;

typedef UINT8 NDAS_DIB_DISKTYPE;
typedef UINT8 NDAS_DIBEXT_DISKTYPE;

#define NDAS_DIBEXT_SIGNATURE 0x3F404A9FF4495F9F

// obsolete types (NDAS_DIB_USAGE_TYPE)

#define NDAS_DIB_USAGE_TYPE_HOME	0x00
#define NDAS_DIB_USAGE_TYPE_OFFICE	0x10

static const UINT32 NDAS_DIB_SIGNATURE		= 0xFE037A4E;
static const UINT8  NDAS_DIB_VERSION_MAJOR	= 0;
static const UINT8  NDAS_DIB_VERSION_MINOR	= 1;

typedef struct _NDAS_DIB {

	UINT32	Signature;				// 4 (4)
	
	UINT8	MajorVersion;			// 1 (5)
	UINT8	MinorVersion;			// 1 (6)
	UINT8	reserved1[2];			// 1 * 2 (8)

	UINT32	Sequence;				// 4 (12)

	UINT8	EtherAddress[6];		// 1 * 6 (18)
	UINT8	UnitNumber;				// 1 (19)
	UINT8	reserved2;				// 1 (20)

	UINT8	DiskType;				// 1 (21)
	UINT8	PeerAddress[6];			// 1 * 6 (27)
	UINT8	PeerUnitNumber;			// 1 (28)
	UINT8	reserved3;				// 1 (29)

	UINT8	UsageType;				// 1 (30)
	UINT8	reserved4[3];			// 1 * 3 (33)

	UINT8	reserved5[512 - 37];	// should be 512 - ( 33 + 4 )

	UINT32	Checksum;				// 4 (512)

} NDAS_DIB, *PNDAS_DIB;

C_ASSERT( sizeof(NDAS_DIB) == 512 );

typedef struct _UNIT_DISK_INFO {

	UINT8 HwVersion:4;	// Used to determine capability of offline RAID member.
	UINT8 Reserved1:4;

	UINT8 Reserved2;

} UNIT_DISK_INFO, *PUNIT_DISK_INFO;

C_ASSERT( sizeof(UNIT_DISK_INFO) == 2 );

typedef struct _UNIT_DISK_LOCATION {

	UINT8 MACAddr[6];
	UINT8 UnitNoObsolete; // don't use as disk location, user can switch disks in 2 Bay
	UINT8 VID; // vendor ID

} UNIT_DISK_LOCATION, *PUNIT_DISK_LOCATION;

C_ASSERT( sizeof(UNIT_DISK_LOCATION) == 8 );

typedef struct _BLOCK_ACCESS_CONTROL_LIST_ELEMENT {

#define BACL_ACCESS_MASK_WRITE				0x01
#define BACL_ACCESS_MASK_READ				0x02
#define BACL_ACCESS_MASK_EMBEDDED_SYSTEM	0x80
#define BACL_ACCESS_MASK_PC_SYSTEM			0x40
	UINT8	AccessMask;
	
	UINT8	Reserved[3 + 4 + 8];
	UINT64	ui64StartSector;	
	UINT64	ui64SectorCount;	// ignored if 0 == ui64SectorCount

} BLOCK_ACCESS_CONTROL_LIST_ELEMENT, *PBLOCK_ACCESS_CONTROL_LIST_ELEMENT;

C_ASSERT( sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT) == 32 );

static const UINT32 BACL_SIGNATURE	= 0x3C1C49B2;
static const UINT32 BACL_VERSION	= 0x00000001;

typedef struct _BLOCK_ACCESS_CONTROL_LIST {

	UINT32	Signature;
	UINT32	Version;
	UINT32	crc; // crc of Elements
	UINT32	ElementCount;
	UINT8	Reserved[16];

	BLOCK_ACCESS_CONTROL_LIST_ELEMENT Elements[1];

} BLOCK_ACCESS_CONTROL_LIST, *PBLOCK_ACCESS_CONTROL_LIST;

C_ASSERT( sizeof(BLOCK_ACCESS_CONTROL_LIST) == 64 );

#define BACL_SIZE(ELEMENT_COUNT) (sizeof(BLOCK_ACCESS_CONTROL_LIST) + sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT) * ((ELEMENT_COUNT) -1))
#define BACL_SECTOR_SIZE(ELEMENT_COUNT) ((BACL_SIZE(ELEMENT_COUNT) +511)/ 512)

#define NDAS_MAX_UNITS_IN_BIND		16 // not system limit
#define NDAS_MAX_UNITS_IN_V2		32
#define NDAS_MAX_UNITS_IN_V2_1		 8
#define NDAS_MAX_UNITS_IN_SECTOR	64

static const UINT64 NDAS_DIB_V2_SIGNATURE		= 0x3F404A9FF4495F9FLL;
static const UINT32 NDAS_DIB_VERSION_MAJOR_V2	= 1;
static const UINT32 NDAS_DIB_VERSION_MINOR_V2	= 2; 

#if 0
#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT	(1<<16)	// 32Mbytes. Changed to 32M since 3.20
#else
#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT_LOG	13	// 4Mbytes. Changed to 4M since 3.30
#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT		(1<<NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT_LOG)		
#endif

#define NDAS_USER_SPACE_ALIGN					(128)	// 3.11 version required 128 sector alignment of user addressable space.
														// Do not change the value to keep compatiblity with NDAS devices 
														// formatted under NDAS software version 3.11.

typedef enum _NDAS_MEDIA_TYPE {

	NMT_INVALID			=0x00,	// operation purpose only : must NOT written on disk
	NMT_SINGLE			=0x01,	// unbound
	NMT_MIRROR			=0x02,	// 2 disks without repair information. need to be converted to RAID1
	NMT_AGGREGATE		=0x03,	// 2 or more
	NMT_RAID1			=0x04,	// with repair
	NMT_RAID4			=0x05,	// with repair. Never released.
	NMT_RAID0			=0x06,	// with repair. RMD is not used. Since the block size of this raid set is 512 * n, Mac OS X don't support this type.
	NMT_SAFE_RAID1		=0x07,	// operation purpose only(add mirror) : must NOT written on disk. used in bind operation only
	NMT_AOD				=0x08,	// Append only disk
	NMT_RAID0R2			=0x09,	// Block size is 512B not 512 * n. RMD is used.
	NMT_RAID1R2			=0x0A,	// with repair, from 3.11
	NMT_RAID4R2			=0x0B,	// with repair, from 3.11. Never released
	NMT_RAID1R3			=0x0C,	// with DRAID & persistent OOS bitmap. Added as of 3.20
	NMT_RAID4R3			=0x0D,	// with DRAID & persistent OOS bitmap. Added as of 3.21. Not implemented yet.
	NMT_RAID5			=0x0E,	// with DRAID & persistent OOS bitmap. Added as of 3.21. Not implemented yet.
	NMT_SAFE_AGGREGATE	=0x0F,	// operation purpose only(append disk to single disk) : must NOT written on disk. used in bind operation only
	NMT_SAFE_RAID_ADD	=0x10,	// operation purpose only(add spare) : must NOT written on disk. used in bind operation only
	NMT_SAFE_RAID_REMOVE=0x11,	// operation purpose only(remove member) : must NOT written on disk. used in bind operation only
	NMT_SAFE_RAID_REPLACE=0x12,	// operation purpose only(Replace member) : must NOT written on disk. used in bind operation only
	NMT_SAFE_RAID_CLEAR_DEFECT=0x13,	// operation purpose only(cleare defective mark) : must NOT written on disk. used in bind operation only
	NMT_VDVD			=0x64,	// virtual DVD
	NMT_CDROM			=0x65,	// packet device, CD / DVD
	NMT_OPMEM			=0x66,	// packet device, Magnetic Optical
	NMT_FLASH			=0x67,	// block device, flash card
	NMT_CONFLICT		=0xff,	// DIB information is conflicting with actual NDAS information. Used for internal purpose. Must not be written to disk.

	// To do: reimplement DIB read functions without using this value..

} NDAS_MEDIA_TYPE, *PNDAS_MEDIA_TYPE;

//	Because prior NDAS service overwrites NDAS_BLOCK_LOCATION_DIB_V1 if Signature, version does not match,
//	NDAS_DIB_V2 locates at NDAS_BLOCK_LOCATION_DIB_V2(-2).

// Disk Information Block V2

typedef struct _NDAS_DIB_V2 {

	union {

		struct {

			UINT64	Signature;		// Byte 0
			UINT32	MajorVersion;	// Byte 8
			UINT32	MinorVersion;	// Byte 12

			// sizeXArea + sizeLogicalSpace <= sizeTotalDisk
		
			UINT64	sizeXArea;		// Byte 16. in sectors, always 2 * 1024 * 2 (2 MB)

			UINT64	sizeUserSpace;	// Byte 24. in sectors

			UINT32	iSectorsPerBit; // Byte 32. dirty bitmap bit unit. default : 128(2^7). Passed to RAID system through service.
								    // default 64 * 1024(2^16, 32Mbyte per bit) since 3.20
								    // default 8 * 1024(2^16, 32Mbyte per bit) since 3.30

			NDAS_MEDIA_TYPE	iMediaType; //  Byte 36. NDAS Media Type. NMT_*

			UINT32	nDiskCount;		// Byte 40. 1 ~ . physical disk units
			UINT32	iSequence;		// Byte 44. 0 ~. Sequence number of this unit in UnitDisks list.
			UINT8	Reserved0[4];	// Byte 48.
			UINT32	BACLSize;		// Byte 52.In byte. Do not access BACL if zero

#define		NDAS_RAID_DEFAULT_START_OFFSET	(0x80)

			UINT64	sizeStartOffset; // byte 56. sizeStartOffset in physical address is 0 in logical NDAS drive

			UINT8	Reserved1[8];	// Byte 64
			UINT32	nSpareCount;	// Byte 72.  0 ~ . used for fault tolerant RAID
			UINT8	Reserved2[52];	// Byte 76

			UNIT_DISK_INFO 	UnitDiskInfos[NDAS_MAX_UNITS_IN_V2]; // Byte 128. Length 64.
		};

		UINT8 bytes_248[248];

	}; // 248

	UINT32 crc32;			// 252
	UINT32 crc32_unitdisks; // 256

#define NDAS_DIB_SERIAL_LEN	20

#if 0
	UNIT_DISK_LOCATION	UnitDisks[NDAS_MAX_UNITS_IN_V2];			// 256 bytes
#else

	UNIT_DISK_LOCATION	UnitLocation[NDAS_MAX_UNITS_IN_V2_1];								//  64  bytes / 2
	CHAR				Reserved3[sizeof(UNIT_DISK_LOCATION)];
	CHAR				UnitSimpleSerialNo[NDAS_MAX_UNITS_IN_V2_1][NDAS_DIB_SERIAL_LEN];	// 160 bytes / 2
	CHAR				Reserved4[24];

#endif

} NDAS_DIB_V2, *PNDAS_DIB_V2;

C_ASSERT( sizeof(NDAS_DIB_V2) == 512 );


#define DIB_UNIT_CRC_FUNC(CRC_FUNC, DIB)	CRC_FUNC((UINT8 *)&(DIB).UnitLocation, \
		sizeof((DIB).UnitLocation)+sizeof((DIB).Reserved3) + sizeof((DIB).UnitSimpleSerialNo) + sizeof((DIB).Reserved4))

#define IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) ((DIB).crc32 == CRC_FUNC((UINT8 *)&(DIB), sizeof((DIB).bytes_248)))

#define IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB) \
	((DIB).crc32_unitdisks == DIB_UNIT_CRC_FUNC(CRC_FUNC, DIB))

#define IS_DIB_CRC_VALID(CRC_FUNC, DIB) (IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) && IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB))

#define SET_DIB_DATA_CRC(CRC_FUNC, DIB) ((DIB).crc32 = CRC_FUNC((UINT8 *)&(DIB), sizeof((DIB).bytes_248)))

#define SET_DIB_UNIT_CRC(CRC_FUNC, DIB) \
	((DIB).crc32_unitdisks = DIB_UNIT_CRC_FUNC(CRC_FUNC, DIB))

#define SET_DIB_CRC(CRC_FUNC, DIB) {SET_DIB_DATA_CRC(CRC_FUNC, DIB); SET_DIB_UNIT_CRC(CRC_FUNC, DIB);}


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


#define	NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH		64		// 64 bytes. 512bits.
#define	NDAS_CONTENT_ENCRYPT_METHOD_NONE		0
#define	NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE		1
#define	NDAS_CONTENT_ENCRYPT_METHOD_AES			2

#define NDAS_CONTENT_ENCRYPT_REV_MAJOR	0x0010
#define NDAS_CONTENT_ENCRYPT_REV_MINOR	0x0010

#define NDAS_CONTENT_ENCRYPT_REVISION \
	((NDAS_CONTENT_ENCRYPT_REV_MAJOR << 16) + NDAS_CONTENT_ENCRYPT_REV_MINOR)

static const UINT64 NDAS_CONTENT_ENCRYPT_BLOCK_SIGNATURE = 0x76381F4C2DF34D7DLL;

typedef struct _NDAS_CONTENT_ENCRYPT_BLOCK {

	union {

		struct {

			UINT64	Signature;	// Little Endian

			// INVARIANT

			UINT32	Revision;	// Little Endian
			
			// VARIANT BY REVISION (MAJOR)
			
			UINT16	Method;		// Little Endian
			UINT16	Reserved_0;	// Little Endian
			UINT16	KeyLength;	// Little Endian
			UINT16	Reserved_1;	// Little Endian

			union {

				UINT8	Key32[32 /8];
				UINT8	Key128[128 /8];
				UINT8	Key192[192 /8];
				UINT8	Key256[256 /8];
				UINT8	Key[NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH];
			};

			UINT8 Fingerprint[16]; // 128 bit, KeyPairMD5 = md5(ks . kd)
		};

		UINT8 bytes_508[508];
	};

	UINT32 CRC32;		// Little Endian

} NDAS_CONTENT_ENCRYPT_BLOCK, *PNDAS_CONTENT_ENCRYPT_BLOCK;

C_ASSERT(512 == sizeof(NDAS_CONTENT_ENCRYPT_BLOCK));

#define IS_NDAS_DIBV1_WRONG_VERSION(DIBV1)	\
	((0 != (DIBV1).MajorVersion) ||			\
	(0 == (DIBV1).MajorVersion && 1 != (DIBV1).MinorVersion))

#define IS_HIGHER_VERSION_V2(DIBV2)							\
	((NDAS_DIB_VERSION_MAJOR_V2 < (DIBV2).MajorVersion) ||	\
	(NDAS_DIB_VERSION_MAJOR_V2 == (DIBV2).MajorVersion &&	\
	NDAS_DIB_VERSION_MINOR_V2 < (DIBV2).MinorVersion))

#define GET_TRAIL_SECTOR_COUNT_V2(DISK_COUNT)	\
	( ((DISK_COUNT) > NDAS_MAX_UNITS_IN_V2) ?	\
	  (((DISK_COUNT) - NDAS_MAX_UNITS_IN_V2) / NDAS_MAX_UNITS_IN_SECTOR) +1 : 0 )

// make older version should not access disks with NDAS_DIB_V2

#define NDAS_DIB_V1_INVALIDATE(DIB_V1)							\
	{	(DIB_V1).Signature		= NDAS_DIB_SIGNATURE;			\
		(DIB_V1).MajorVersion	= NDAS_DIB_VERSION_MAJOR;		\
		(DIB_V1).MinorVersion	= NDAS_DIB_VERSION_MINOR;		\
		(DIB_V1).DiskType		= NDAS_DIB_DISK_TYPE_INVALID;	}


#if 0 // LAST_WRITTEN_SECTOR, LAST_WRITTEN_SECTORS is not used anymore.

typedef struct _LAST_WRITTEN_SECTOR {

	UINT64	logicalBlockAddress;
	UINT32	transferBlocks;
	ULONG	timeStamp;

} LAST_WRITTEN_SECTOR, *PLAST_WRITTEN_SECTOR;

C_ASSERT( sizeof(LAST_WRITTEN_SECTOR) == 16 );

typedef struct _LAST_WRITTEN_SECTORS {

	LAST_WRITTEN_SECTOR LWS[32];

} LAST_WRITTEN_SECTORS, *PLAST_WRITTEN_SECTORS;

C_ASSERT( sizeof(LAST_WRITTEN_SECTORS) == 512 );

#endif

// All number is in little-endian

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

#define NDAS_BYTE_PER_OOS_BITMAP_BLOCK  (512-16)
#define NDAS_BIT_PER_OOS_BITMAP_BLOCK  (NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8)

typedef struct _NDAS_OOS_BITMAP_BLOCK {

	UINT64	SequenceNumHead; // Increased when this block is updated.
	UCHAR	Bits[NDAS_BYTE_PER_OOS_BITMAP_BLOCK];
	UINT64	SequenceNumTail; // Same value of SequenceNumHead. If two value is not matched, this block is corrupted. 

} NDAS_OOS_BITMAP_BLOCK, *PNDAS_OOS_BITMAP_BLOCK;

C_ASSERT( sizeof(NDAS_OOS_BITMAP_BLOCK) == 512 );

//member of the NDAS_RAID_META_DATA structure
//8 bytes of size

typedef enum _NDAS_UNIT_MEDIA_BIND_STATUS_FLAGS {

	// Disk is not synced because of network down or new disk
	// NDAS_UNIT_META_BIND_STATUS_FAULT at RAID1R2

	NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED			= 0x00000001, 
	NDAS_UNIT_META_BIND_STATUS_SPARE				= 0x00000002,
	NDAS_UNIT_META_BIND_STATUS_BAD_DISK				= 0x00000004,
	NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR			= 0x00000008,
	NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE	= 0x00000010,
	NDAS_UNIT_META_BIND_STATUS_OFFLINE				= 0x00000020,

	NDAS_UNIT_META_BIND_STATUS_DEFECTIVE = NDAS_UNIT_META_BIND_STATUS_BAD_DISK		|
										   NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR	|
										   NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE,

} NDAS_UNIT_MEDIA_BIND_STATUS_FLAGS;

typedef struct _NDAS_UNIT_META_DATA {

	union {

		UINT16	iUnitDeviceIdx;	 	// Index in NDAS_DIB_V2.UnitDisks
	
		struct {
			
			UINT8 Nidx;
			UINT8 Reserved1;
		};
	};

	UINT8	UnitDeviceStatus; 	// NDAS_UNIT_META_BIND_STATUS_*
	UINT8	Reserved[5];

} NDAS_UNIT_META_DATA, *PNDAS_UNIT_META_DATA;

C_ASSERT( sizeof(NDAS_UNIT_META_DATA) == 8 );


// NDAS_RAID_META_DATA structure contains status which changes by the change
// of status of the RAID. For stable status, see NDAS_DIB_V2
// All data is in littlen endian.

// NDAS_RAID_META_DATA has 512 bytes of size

// NDAS_RAID_META_DATA is for Fault tolerant bind only. ATM, RAID 1 and 4.


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

			UINT64	Signature;		// Little Endian
			GUID	RaidSetId;		// Prior RaidSetId
									// RaidSetId is created when RAID is created 
									// and not changed until RAID configuration is changed by bindtool.
									// Both RaidSetId and ConfigSetId should be matched to work as RAID member

			UINT32	uiUSN;			// Universal sequence number increases 1 each writes.
			UINT32	state;			// Ored value of NDAS_RAID_META_DATA_STATE_*

			GUID	ConfigSetId;	// Added as of 3.20. For previous version this is 0.
									// Set when RAID is created and changed 
									// when RAID is reconfigured online with missing member.
									// Reconfiguration occurs when spare member is changed to active member to rule out missing member
									// or on-line reconfiguration occurs.(Currently not implemented)
									// Meaningful only for RAID1 and RAID4/5 with 2+1 configuration with spare.
			
			// DRAID Arbiter node information. Updated when state is changed to mounted.
			// Used only for redundant RAID.
			// Added as of 3.20
			
			struct {

				UINT8   Type;	// Type of RAID master node. LPX/IP, running mode, etc.
								// Currently NDAS_RAID_ARBITRATOR_TYPE_LPX and NDAS_RAID_ARBITRATOR_TYPE_NONE only.
				
				UINT8   Reserved[3];
				UINT8	Addr[8]; // MAC or IP. Need to be at least 16 byte to support IPv6?
			
			} ArbitratorInfo[NDAS_RAID_ARBITRATOR_ADDR_COUNT];	// Assume max 8 network interface.
			
			GUID	RaidSetIdBackup;
		};

		UINT8 bytes_248[248];
	
	}; // 248
	
	UINT32 crc32;			// 252
	UINT32 crc32_unitdisks; // 256

	// This array is always sorted by its member role in RAID.

	NDAS_UNIT_META_DATA UnitMetaData[NDAS_MAX_UNITS_IN_V2]; // 256 bytes

} NDAS_RAID_META_DATA, *PNDAS_RAID_META_DATA;

C_ASSERT( sizeof(NDAS_RAID_META_DATA) == 512 );

#ifdef __PASS_RMD_CRC32_CHK__
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) TRUE
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) TRUE
#else
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32 == CRC_FUNC((UINT8 *)&(RMD), sizeof((RMD).bytes_248)))
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32_unitdisks == CRC_FUNC((UINT8 *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData)))
#endif

#define IS_RMD_CRC_VALID(CRC_FUNC, RMD) (IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) && IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD))

#define SET_RMD_DATA_CRC(CRC_FUNC, RMD) ((RMD).crc32 = CRC_FUNC((UINT8 *)&(RMD), sizeof((RMD).bytes_248)))
#define SET_RMD_UNIT_CRC(CRC_FUNC, RMD) ((RMD).crc32_unitdisks = CRC_FUNC((UINT8 *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData)))
#define SET_RMD_CRC(CRC_FUNC, RMD) {SET_RMD_DATA_CRC(CRC_FUNC, RMD); SET_RMD_UNIT_CRC(CRC_FUNC, RMD);}

#if 0
#define RMD_IDX_RECOVER_INFO(DISK_COUNT, IDX_DEFECTED) (((DISK_COUNT) -1 == (IDX_DEFECTED)) ? 0 : (IDX_DEFECTED) +1)
#define RMD_IDX_DEFEDTED(DISK_COUNT, IDX_RECOVER_INFO) ((0 == (IDX_RECOVER_INFO)) ? (DISK_COUNT) -1 : (IDX_RECOVER_INFO) -1)
#else
#define RMD_IDX_RECOVER_INFO(DISK_COUNT, IDX_DEFECTED) (((IDX_DEFECTED) +1) % (DISK_COUNT))
#define RMD_IDX_DEFEDTED(DISK_COUNT, IDX_RECOVER_INFO) (((IDX_RECOVER_INFO) + (DISK_COUNT) -1) % (DISK_COUNT))
#endif


typedef struct _NDAS_MATA_DATA {

	NDAS_RAID_META_DATA			RMD_T;
	NDAS_RAID_META_DATA			RMD;

	NDAS_CONTENT_ENCRYPT_BLOCK	EncryptBlock;

	NDAS_DIB_V2					DIB_V2;
	NDAS_DIB					DIB_V1;

} NDAS_META_DATA, *PNDAS_META_DATA;

C_ASSERT( sizeof(NDAS_META_DATA) == 512*5 );


// Turn 1-byte structure alignment off

#include <poppack.h>

#endif /* _NDAS_DIB_H_ */
