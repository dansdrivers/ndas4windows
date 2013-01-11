#ifndef LANSCSI_LURN_ASSOCIATE_H
#define LANSCSI_LURN_ASSOCIATE_H

#include <ndas/ndasdib.h>

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

#define		RAID_INFO_POOL_TAG			'TPIR'
#define		RAID_RECOVER_POOL_TAG		'VCER'

#define		BITMAP_POOL_TAG				'PMTB'
#define		BYTE_SET_POOL_TAG			'TSTB'
#define		DATA_BUFFER_POOL_TAG		'ATAD'

#define		ONE_SECTOR_POOL_TAG			'TCES'

#define NDAS_MAX_RAID0_CHILD 8 // RAID 4 supports 9
#define NDAS_MAX_RAID1_CHILD 2 // RAID 4 supports 9
#define NDAS_MAX_RAID4_CHILD 9 // RAID 4 supports 9
#define NDAS_MAX_RAID_CHILD NDAS_MAX_RAID4_CHILD

typedef struct _DATA_BUFFER {
	PCHAR		DataBuffer;
	UINT32		DataBufferLenght;
} DATA_BUFFER, *PDATA_BUFFER;

typedef struct _CUSTOM_DATA_BUFFER {
	UINT32		DataBufferCount;
#define		RAID_DATA_BUFFER_POOL_TAG	'BDDR'
	PCHAR		DataBuffer[NDAS_MAX_RAID_CHILD];
	UINT32		DataBufferLength[NDAS_MAX_RAID_CHILD];
} CUSTOM_DATA_BUFFER, *PCUSTOM_DATA_BUFFER;

typedef struct _RAID_INFO {
#define		RAID_STATUS_NORMAL			0x0000
#define		RAID_STATUS_EMERGENCY_READY	0x0007
#define		RAID_STATUS_EMERGENCY		0x0002
#define		RAID_STATUS_RECOVERRING		0x0003
#define		RAID_STATUS_TERMINATING		0x0004
#define		RAID_STATUS_INITIAILIZING	0x0005
#define		RAID_STATUS_FAIL			0x0006
	UINT32			RaidStatus;

	ULONG			timeStamp; // increased per each write operation. LWSs[timeStamp %32]->timeStamp = timeStamp;

	PCHAR			DataBufferAllocated;	// Allocated data buffer used for shuffled data I/O
	PCHAR			DataBuffers[NDAS_MAX_RAID_CHILD];
	LONG			LockDataBuffer;			// Lock DataBufferAllocated using InterlockedDecrement, InterlockedIncrement
	KSPIN_LOCK		LockInfo;				// Lock RaidStatus, BitmapIdxToRecover to protect recover thread

	UINT32			SectorsPerBit;			// 2^7(~ 2^11). From service
	UINT32			MaxBlocksPerRequest;	// From service. Normally 128
	HANDLE			ThreadRecoverHandle;	// Recover thread handle
	PVOID			ThreadRecoverObject;	// Receive termination notification
	KEVENT			RecoverThreadEvent;		// Used to terminate recover thread

	CMD_BYTE_OP		ExtCmdBitmap;			// Extended command used to write bitmap to device
	CMD_BYTE_OP		ExtCmdLws;				// Extended command used to write LWS to device, points RAID_INFO.LWSs
	PRTL_BITMAP		Bitmap;					// Bitmap to record corruption
	ULONG			BitmapIdxToRecover;		// Used to protect sectors being recovered

	UINT32			iChildDefected;			// Disconnected device index in LURELATION_NODE.LurnChildren
	UINT32			iChildRecoverInfo;		// Device index which records bitmap in LURELATION_NODE.LurnChildren
	UINT32			nDiskCount;
	UINT32			nSpareDisk;

	LAST_WRITTEN_SECTORS	LWSs; // 1 sector size of Last Written Sectors
	NDAS_RAID_META_DATA rmd;				// ndasdib.h
	PLURELATION_NODE	MapLurnChildren[NDAS_MAX_RAID_CHILD];
} RAID_INFO, *PRAID_INFO;

#define	RAID_IS_RUNNING(RI) (RAID_STATUS_NORMAL == (RI)->RaidStatus || RAID_STATUS_EMERGENCY == (RI)->RaidStatus || RAID_STATUS_RECOVERRING == (RI)->RaidStatus)
#define NDAS_RAID_INFO_BUFFER(RI, IDX) ((NDAS_RAID_META_DATA_SIGNATURE == (RI)->rmd.Signature) ? (RI)->DataBuffers[(RI)->rmd.UnitMetaData[IDX].iUnitDeviceIdx] : (RI)->DataBuffers[IDX])

//////////////////////////////////////////////////////////////////////////
//
//	lslurnassoc.c
//

NTSTATUS
LurnAssocSendCcbToAllChildren(
		IN PLURELATION_NODE			Lurn,
		IN PCCB						Ccb,
		IN CCB_COMPLETION_ROUTINE	CcbCompletion,
		IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
		IN PVOID					*apExtendedCmd,
		IN BOOLEAN					AssociateCascade
);

NTSTATUS
LurnAssocRefreshCcbStatusFlag(
	IN PLURELATION_NODE			pLurn,
	PULONG						CcbStatusFlags
);

#endif