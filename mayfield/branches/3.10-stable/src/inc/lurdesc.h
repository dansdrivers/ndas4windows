#ifndef LANSCSI_LURNDESC_H
#define LANSCSI_LURNDESC_H

#include "LanScsi.h"


//////////////////////////////////////////////////////////////////////////
//
//	Logical Unit Relation Node specific extension
//

//
//	Optional init data
//
#define LURNDESC_INITDATA_BUFFERLEN		80

//
//	LUR ID ( SCSI address )
//
#define LURID_LENGTH		8
#define LURID_PATHID		0
#define LURID_TARGETID		1
#define LURID_LUN			2

//
//	LUR device types
//	Even though an LUR has a Disk LURN,
//	It can be a DVD to Windows.
//
#define LUR_DEVTYPE_HDD		0x00000001
#define LUR_DEVTYPE_ODD		0x00000002
#define LUR_DEVTYPE_MOD		0x00000003

//
//	LUR Node type
//
#define 	LURN_AGGREGATION		0x0000
#define 	LURN_MIRRORING			0x0001
#define 	LURN_IDE_DISK			0x0002
#define 	LURN_IDE_ODD			0x0003
#define		LURN_IDE_MO				0x0004
#define 	LURN_RAID1				0x0005
#define 	LURN_RAID4				0x0006
#define 	LURN_RAID0				0x0007
#define 	LURN_AOD				0x0008
#define 	LURN_NULL				0xFFFF

typedef	UINT16 LURN_TYPE, *PLURN_TYPE;


#include <pshpack1.h>

//
//	IDE specific information
//
//  Used in _LURELATION_NODE_DESC
//
typedef struct _LURNDESC_IDE {

	UINT32					UserID;
	UINT64					Password;
	TA_LSTRANS_ADDRESS		TargetAddress;	// sizeof(TA_LSTRANS_ADDRESS) == 26 bytes
	TA_LSTRANS_ADDRESS		BindingAddress;
	UCHAR					HWType;
	UCHAR					HWVersion;
	UCHAR					LanscsiTargetID;
	UCHAR					LanscsiLU;
	UINT64					EndBlockAddrReserved;

} LURNDESC_IDE, *PLURNDESC_IDE;


//
//	MirrorV2 specific information
//
//  Used in _LURELATION_NODE_DESC
//  Negative sector means index from last sector(ex: -1 indicates last sector, -2 for 1 before last sector...)
//
typedef struct _INFO_RAID {
	UINT64					SectorInfo; // sector where DIB_V2 exists
	UINT32					OffsetFlagInfo; // dirty flag's byte offset in DIB_V2
	UINT64					SectorBitmapStart; // sector where bitmap starts
	UINT32					SectorsPerBit; // 2^7(~ 2^11)
	UINT64					SectorLastWrittenSector; // sector where to write last written sectors
} INFO_RAID, *PINFO_RAID;

#include <poppack.h>

//////////////////////////////////////////////////////////////////////////
//
//	Logical Unit Relation Node descriptor
//

//
//	LU relation node options.
//	These options override default setting.
//
#define	LURNOPTION_SET_RECONNECTION		0x00000001	// Override default reconnection setting.

//
//	LU relation node descriptor
//
typedef struct _LURELATION_NODE_DESC {
	UINT16					NextOffset;

	LURN_TYPE				LurnType;
	UINT32					LurnId;
	UINT64					StartBlockAddr;
	UINT64					EndBlockAddr;
	UINT64					UnitBlocks;
	UINT32					BytesInBlock;
	UINT32					MaxBlocksPerRequest;
	ACCESS_MASK				AccessRight;

	//
	//	Override default settings.
	//
	UINT32					LurnOptions;

	//
	//	Reconnection parameters
	//
	//	ReconnTrial: Number of reconnection trial
	//	ReconnInterval:	Reconnection interval in milliseconds.
	//
	//	NOTE: Valid with LURNOPTION_SET_RECONNECTION option.
	//
	UINT32					ReconnTrial;
	UINT32					ReconnInterval;

	//
	//	LURN-specific extension
	//
	union {
		BYTE				InitData[LURNDESC_INITDATA_BUFFERLEN];
		LURNDESC_IDE		LurnIde;
		INFO_RAID			LurnInfoRAID[9]; // 2 disks for each mirrored disks
	};

	UINT32					LurnParent;
	UINT32					LurnChildrenCnt;
	UINT32					LurnChildren[1];

} LURELATION_NODE_DESC, *PLURELATION_NODE_DESC;

//////////////////////////////////////////////////////////////////////////
//
//	Logical Unit Relation Descriptor
//

//
//	LU relation options.
//	These options override default setting.
//
//	Default setting:
//				Fake write is on when an LUR doesn't get granted write access.
//					and read-only mode on Windows 2000.
//				Write share is always on.
//
#define	LUROPTION_ON_FAKEWRITE		0x00000001	// Turn On the fake write function.
#define	LUROPTION_OFF_FAKEWRITE		0x00000002	// Turn Off the fake write function.
#define	LUROPTION_ON_WRITESHARE_PS	0x00000004	// Turn On the write-sharing in Primary-Secondary mode.
#define	LUROPTION_OFF_WRITESHARE_PS	0x00000008	// Turn Off the write-sharing in Primary-Secondary mode.
#define	LUROPTION_ON_UDMA_TO3		0x00000040	// Set highest UDMA level to 4.
#define	LUROPTION_OFF_UDMA_TO3		0x00000080	// Reset higest UDMA level.

//
//	LU relation descriptor which bears node descriptor.
//
typedef struct _PLURELATION_DESC {

	UINT16					Length;
	UINT16					Type;
	UINT16					DevType;
	UINT16					DevSubtype;
	UCHAR					LurId[LURID_LENGTH];
	UINT32					MaxBlocksPerRequest;
	UINT32					BytesInBlock;
	ACCESS_MASK				AccessRight;
	UINT32					LurOptions;
	UCHAR					Reserved[4];

	//
	//	Content encryption.
	//	Key length in bytes = CntEcrKeyLength.
	//	If CntEcrMethod is zero, there is no content encryption.
	//	If key length is zero, there is no content encryption.
	//
	IN	UCHAR				CntEcrMethod;
	IN	UCHAR				CntEcrKeyLength;
	IN	UCHAR				CntEcrKey[NDAS_CONTENTENCRYPT_KEY_LENGTH];

	UINT32					LurnDescCount;
	LURELATION_NODE_DESC	LurnDesc[1];

} LURELATION_DESC, *PLURELATION_DESC;


#define SIZE_OF_LURELATION_DESC() (sizeof(LURELATION_DESC) - sizeof(LURELATION_NODE_DESC))
#define SIZE_OF_LURELATION_NODE_DESC(NR_LEAVES) (sizeof(LURELATION_NODE_DESC) - sizeof(LONG) + sizeof(LONG) * (NR_LEAVES))


#endif