#ifndef LANSCSI_LURN_ASSOCIATE_H
#define LANSCSI_LURN_ASSOCIATE_H

#define LURNASSOC_COMPLETE_POOL_TAG			'clSL'

#define AGGR_MODEL_NAME			{'A','g','g','r','e','g','a','t','i','o','n',0,0,0,0,0}
#define MIRR_MODEL_NAME			{'M','i','r','r','o','r','i','n','g',0,0,0,0,0,0,0}
#define RAID01_MODEL_NAME		{'R','A','I','D',' ','1',0,0,0,0,0,0,0,0,0,0}

extern LURN_INTERFACE LurnAggrInterface;
extern LURN_INTERFACE LurnMirrorInterface;
extern LURN_INTERFACE LurnMirrorV2Interface;

#define		RAID_STATUS_NORMAL			0x0000
#define		RAID_STATUS_FAIL_DETECTED	0x0001
#define		RAID_STATUS_BITMAPPING		0x0002

#define		RAID_00_INFO_POOL_TAG		'00dR'
#define		RAID_01_INFO_POOL_TAG		'10dR'
#define		RAID_05_INFO_POOL_TAG		'50dR'

#define		RAID_BITMAP_POOL_TAG		'PMTB'
#define		RAID_BYTE_SET_POOL_TAG		'TSTB'

#define		ONE_SECTOR_POOL_TAG			'TCES'

typedef struct _RAID_INFO {
	UINT32	RaidType; // 0, 1, 5 ...
	UINT32	RaidStatus;
} RAID_INFO, *PRAID_INFO;

typedef struct _RAID_01_INFO {
	UINT32		RaidType;
	UINT32		RaidStatus;
	UINT32		iDefectedChild;
	INFO_RAID_1 ChildrenInfo[2];	

	KSPIN_LOCK	BitmapSpinLock;
	PRTL_BITMAP	Bitmap;
} RAID_01_INFO, *PRAID_01_INFO;

#endif