#ifndef _JB_M_DIB_H_
#define _JB_M_DIB_H_
#include <pshpack1.h>

typedef unsigned _int8 NDAS_DIBEXT_DISK_TYPE;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_SINGLE = 0;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_MASTER = 1;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FIRST = 2;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_SLAVE = 11;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_SECOND = 21;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_THIRD = 22;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FOURTH = 23;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_VDVD = 32;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MEDIAJUKE = 104;


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

}NDAS_DIB, *PNDAS_DIB;



static const unsigned _int64 NDAS_DIB_V2_SIGNATURE = 0x3F404A9FF4495F9F;
static const unsigned _int32 NDAS_DIB_VERSION_MAJOR_V2 = 1;
static const unsigned _int32 NDAS_DIB_VERSION_MINOR_V2 = 1;

/*
	Because prior NDAS service overwrites NDAS_BLOCK_LOCATION_DIB_V1 if Signature, version does not match,
	NDAS_DIB_V2 locates at NDAS_BLOCK_LOCATION_DIB_V2(-2).
*/

// Disk Information Block V2
// Disk Information Block V2
typedef struct _NDAS_DIB_V2 {
	union {
		struct {
			unsigned _int64	Signature;
			unsigned _int32	MajorVersion;
			unsigned _int32	MinorVersion;

			// sizeXArea + sizeLogicalSpace <= sizeTotalDisk
			unsigned _int64	sizeXArea; // in sectors, always 2 * 1024 * 2 (2 MB)

			unsigned _int64	sizeUserSpace; // in sectors

			unsigned _int32	iSectorsPerBit; // dirty bitmap . default : 128
#define NMT_INVALID			0		// operation purpose only : must NOT written on disk
#define NMT_SINGLE			1		// unbound
#define NMT_MIRROR			2		// 2 disks without repair information. need to be converted to RAID1
#define NMT_SAFE_RAID1		7		// operation purpose only : must NOT written on disk. used in bind operation only
#define NMT_AGGREGATE		3		// 2 or more
#define NMT_RAID0			6		// with repair
#define NMT_RAID1			4		// with repair
#define NMT_RAID4			5		// with repair
#define NMT_VDVD			100		// virtual DVD
#define NMT_CDROM			101		// packet device, CD / DVD
#define NMT_OPMEM			102		// packet device, Magnetic Optical
#define NMT_FLASH			103		// block device, flash card
#define NMT_MEDIAJUKE			104		// MEDIA JUKE BOX
#define NMT_MEDIA_DISK			105		// MEDIA DISK
			unsigned _int32	iMediaType; // NDAS Media Type
			unsigned _int32	nDiskCount; // 1 ~ . physical disk units
			unsigned _int32	iSequence; // 0 ~
			unsigned _int8	AutoRecover; // Recover RAID automatically
			unsigned _int8	Reserved0[3];
		};

		unsigned _int8 byte_248[248];
	};
	unsigned _int32 crc32; // 252
	unsigned _int32 crc32_unitdisks; // 256
	unsigned _int8  data[256];
} NDAS_DIB_V2, *PNDAS_DIB_V2;
#include <poppack.h>
#endif //#ifndef _JB_M_DIB_H_