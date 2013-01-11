#ifndef LANSCSI_LURN_ASSOCIATE_H
#define LANSCSI_LURN_ASSOCIATE_H

#define LURNASSOC_COMPLETE_POOL_TAG			'clSL'

#define AGGR_MODEL_NAME			{'A','g','g','r','e','g','a','t','e','d',0,0,0,0,0,0}
#define MIRR_MODEL_NAME			{'M','i','r','r','o','r','e','d',0,0,0,0,0,0,0,0}
#define RAID0_MODEL_NAME		{'R','A','I','D',' ','0',0,0,0,0,0,0,0,0,0,0}
#define RAID1_MODEL_NAME		{'R','A','I','D',' ','1',0,0,0,0,0,0,0,0,0,0}
#define RAID4_MODEL_NAME		{'R','A','I','D',' ','4',0,0,0,0,0,0,0,0,0,0}

extern LURN_INTERFACE LurnAggrInterface;
extern LURN_INTERFACE LurnMirrorInterface;
extern LURN_INTERFACE LurnRAID0Interface;
extern LURN_INTERFACE LurnRAID1Interface;
extern LURN_INTERFACE LurnRAID4Interface;

#define		RAID_INFO_POOL_TAG		'TPIR'

#define		BITMAP_POOL_TAG				'PMTB'
#define		BYTE_SET_POOL_TAG			'TSTB'
#define		DATA_BUFFER_POOL_TAG		'ATAD'

#define		ONE_SECTOR_POOL_TAG			'TCES'

typedef struct _DATA_BUFFER {
	PCHAR		DataBuffer;
	UINT32		DataBufferLenght;
} DATA_BUFFER, *PDATA_BUFFER;

typedef struct _CUSTOM_DATA_BUFFER {
#define CUSTOM_DATA_BUFFER_MAX 9
	UINT32		DataBufferCount;
#define		RAID_DATA_BUFFER_POOL_TAG	'BDDR'
	PCHAR		DataBuffer[CUSTOM_DATA_BUFFER_MAX];
	UINT32		DataBufferLength[CUSTOM_DATA_BUFFER_MAX];
} CUSTOM_DATA_BUFFER, *PCUSTOM_DATA_BUFFER;

typedef struct _RAID_INFO {
	UINT32	RaidStatus;
} RAID_INFO, *PRAID_INFO;

#define MAX_DISKS_IN_RAID0 8

typedef struct _RAID0_CHILD_INFO {
	PCHAR					DataBuffer;
} RAID0_CHILD_INFO, *PRAID0_CHILD_INFO;

typedef struct _RAID0_INFO {
#define		RAID0_STATUS_NORMAL			0x0000 // always NORMAL
	UINT32		RaidStatus;

	RAID0_CHILD_INFO	*Children;

	LONG		LockDataBuffer;
	PCHAR		DataBufferAllocated;
} RAID0_INFO, *PRAID0_INFO;

#define DISKS_IN_RAID1_PAIR 2

typedef struct _RAID1_CHILD_INFO {
	INFO_RAID1				InfoRaid1;

	CMD_BYTE_OP				ExtendedCommandForTag;
	CMD_BYTE_LAST_WRITTEN_SECTOR	ByteLastWrittenSector;
} RAID1_CHILD_INFO, *PRAID1_CHILD_INFO;

typedef struct _RAID1_INFO {
#define		RAID1_STATUS_NORMAL			0x0000
#define		RAID1_STATUS_FAIL_DETECTED		0x0001
#define		RAID1_STATUS_BITMAPPING		0x0002
	UINT32		RaidStatus;
	
	RAID1_CHILD_INFO	Children[DISKS_IN_RAID1_PAIR];

	LONG		LockDataBuffer;

	PRTL_BITMAP	Bitmap;

	UINT32		iDefectedChild;

	//	CMD_BYTE_OP	ExtendedCommandForFlag; // sizeof(CMD_COMMON) // 1 byte
	CMD_BYTE_OP	ExtendedComandForBitmap; // 
} RAID1_INFO, *PRAID1_INFO;

typedef struct _CMD_COMMON CMD_COMMON, *PCMD_COMMON;

#define MAX_DISKS_IN_RAID4 9

typedef struct _RAID4_CHILD_INFO {
	PCHAR					DataBuffer;
	INFO_RAID4				InfoRaid4;

	CMD_BYTE_OP				ExtendedCommandForTag;
	CMD_BYTE_LAST_WRITTEN_SECTOR	ByteLastWrittenSector;
} RAID4_CHILD_INFO, *PRAID4_CHILD_INFO;

typedef struct _RAID4_INFO {
#define		RAID4_STATUS_NORMAL			0x0000
#define		RAID4_STATUS_FAIL_DETECTED		0x0001
#define		RAID4_STATUS_BITMAPPING		0x0002
	UINT32		RaidStatus;

	RAID4_CHILD_INFO	*Children;

	LONG		LockDataBuffer;
	PCHAR		DataBufferAllocated;

	PRTL_BITMAP	Bitmap;

	UINT32		iDefectedChild;

//	CMD_BYTE_OP	ExtendedCommandForFlag; // sizeof(CMD_COMMON) // 1 byte
	CMD_BYTE_OP	ExtendedComandForBitmap; // 
} RAID4_INFO, *PRAID4_INFO;

#endif