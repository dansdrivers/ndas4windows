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

typedef unsigned _int64 NDAS_BLOCK_LOCATION;
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V1		= -1;		// Disk Information Block V1
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V2		= -2;		// Disk Information Block V2
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_OEM		= -0x0100;	// OEM Reserved
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ADD_BIND	= -0x0200;	// Additional bind informations
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BITMAP		= -0x0f00;	// Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_WRITE_LOG	= -0x1000;	// Last written sector(Optional)

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_MBR		= 0;		// Master Boot Record
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_USER		= 0x80;		// Partion 1 starts here
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LDM		= 0;		// depends on sizeUserSpace, last 1MB of user space

typedef unsigned _int8 NDAS_DIB_DISK_TYPE;

static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_SINGLE = 0;
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_MIRROR_MASTER = 1;
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST = 2;
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_MIRROR_SLAVE = 11;
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND = 21;
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD = 22;
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH = 23;

// extended type information is stored in DIBEXT
static const NDAS_DIB_DISK_TYPE NDAS_DIB_DISK_TYPE_EXTENDED = 80;

typedef unsigned _int8 NDAS_DIBEXT_DISK_TYPE;

static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_SINGLE = 0;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_MASTER = 1;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FIRST	= 2;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_SLAVE = 11;

typedef unsigned char NDAS_DIB_DISKTYPE;
typedef unsigned char NDAS_DIBEXT_DISKTYPE;

static const unsigned _int64 NDAS_DIBEXT_SIGNATURE = 0x3F404A9FF4495F9F;

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

typedef struct _UNIT_DISK_LOCATION
{
	unsigned _int8 MACAddr[6];
	unsigned _int8 UnitNumber;
	unsigned _int8 reserved;
} UNIT_DISK_LOCATION, *PUNIT_DISK_LOCATION;

#define MAX_UNITS_IN_V2 32
#define MAX_UNITS_IN_SECTOR 64

// Disk Information Block V2
typedef struct _NDAS_DIB_V2 {
	union{
		struct {
#define	DISK_INFORMATION_SIGNATURE_V2	0x3F404A9FF4495F9F
			unsigned _int64	Signature;

#define NDAS_DIB_VERSION_MAJOR_V2	1
			unsigned _int32	MajorVersion;
#define NDAS_DIB_VERSION_MINOR_V2	0
			unsigned _int32	MinorVersion;

			// sizeXArea + sizeLogicalSpace <= sizeTotalDisk
			unsigned _int64	sizeXArea; // in sectors, always 2 * 1024 * 2 (2 MB)

			unsigned _int64	sizeUserSpace; // in sectors

			unsigned _int32	iSectorsPerBit; // dirty bitmap . default : 128
#define NMT_SINGLE			1		// unbound
#define NMT_MIRROR			2		// 2 disks without repair information. need to be converted to RAID_01
#define NMT_AGGREGATE		3		// 2 or more
#define NMT_RAID_01			4		// with repair
#define NMT_VDVD			100		// virtual DVD
#define NMT_CDROM			101		// packet device, CD / DVD
#define NMT_OPMEM			102		// packet device, Magnetic Optical
#define NMT_FLASH			103		// block device, flash card
			unsigned _int32	iMediaType; // NDAS Media Type

			unsigned _int32	nUnitCount; // 1 ~ . physical disk units
			unsigned _int32	iSequence; // 0 ~

#define NDAS_DIRTY_MIRROR_DIRTY 0x00000001
#define NDAS_DIRTY_MIRROR_DEAD	0x00000002 // run time flag, so ignore data in disk
			unsigned _int32	FlagDirty;
			unsigned _int8	reserved2[4];
		};
		unsigned char bytes_256[256];
	};

	UNIT_DISK_LOCATION	UnitDisks[MAX_UNITS_IN_V2]; // 256 bytes
} NDAS_DIB_V2, *PNDAS_DIB_V2;

#define IS_NDAS_DIBV1_WRONG_VERSION(DIBV1) \
	((0 != (DIBV1).MajorVersion) || \
	(0 == (DIBV1).MajorVersion && 1 != (DIBV1).MinorVersion))

#define IS_HIGHER_VERSION_V2(DIBV2) \
	((NDAS_DIB_VERSION_MAJOR_V2 < (DIBV2).MajorVersion) || \
	(NDAS_DIB_VERSION_MAJOR_V2 == (DIBV2).MajorVersion && \
	NDAS_DIB_VERSION_MINOR_V2 < (DIBV2).MinorVersion))

#define GET_TRAIL_SECTOR_COUNT_V2(DISK_COUNT) \
	(((DISK_COUNT) > MAX_UNITS_IN_V2) ? \
	(((DISK_COUNT) - MAX_UNITS_IN_V2) / MAX_UNITS_IN_SECTOR) +1 : \
	0)


/* Turn 1-byte structure alignment off */
#include <poppack.h>

#endif /* _NDAS_DIB_H_ */
