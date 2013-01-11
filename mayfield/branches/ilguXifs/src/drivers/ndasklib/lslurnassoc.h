#ifndef LANSCSI_LURN_ASSOCIATE_H
#define LANSCSI_LURN_ASSOCIATE_H

#include <ndas/ndasdib.h>

#define LURNASSOC_COMPLETE_POOL_TAG			'clSL'

#define AGGR_MODEL_NAME			{'A','g','g','r','e','g','a','t','e','d',0,0,0,0,0,0}
#define MIRR_MODEL_NAME			{'M','i','r','r','o','r','e','d',0,0,0,0,0,0,0,0}
#define RAID0_MODEL_NAME		{'R','A','I','D',' ','0',0,0,0,0,0,0,0,0,0,0}
#define RAID1R_MODEL_NAME		{'R','A','I','D',' ','1','R',0,0,0,0,0,0,0,0,0}
#define RAID4R_MODEL_NAME		{'R','A','I','D',' ','4','R',0,0,0,0,0,0,0,0,0}

extern LURN_INTERFACE LurnAggrInterface;
extern LURN_INTERFACE LurnMirrorInterface;
extern LURN_INTERFACE LurnRAID0Interface;
extern LURN_INTERFACE LurnRAID1RInterface;
extern LURN_INTERFACE LurnRAID4RInterface;

#define		RAID_INFO_POOL_TAG			'TPIR'
#define		RAID_RECOVER_POOL_TAG		'VCER'

#define		BITMAP_POOL_TAG				'PMTB'
#define		BYTE_SET_POOL_TAG			'TSTB'
#define		DATA_BUFFER_POOL_TAG		'ATAD'

#define		ONE_SECTOR_POOL_TAG			'TCES'

#define NDAS_MAX_RAID0_CHILD 8 // RAID 0 supports 8
#define NDAS_MAX_RAID1R_CHILD 2 // RAID 1R supports 2
#define NDAS_MAX_RAID4R_CHILD 9 // RAID 4R supports 9
#define NDAS_MAX_RAID_CHILD NDAS_MAX_RAID4R_CHILD

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

// records log of written sectors
// set on writing RAID_STATUS_EMERGENCY_READY
// set to bitmap on emergency_ready
// reset on init and emergency_ready
typedef struct _WRITE_LOG {
	UINT64	logicalBlockAddress;
	UINT32	transferBlocks;
	ULONG	timeStamp;
} WRITE_LOG, *PWRITE_LOG;

#define NDAS_RAID_WRITE_LOG_SIZE 32

typedef struct _RAID_INFO {
#define		RAID_STATUS_NORMAL			0x0000
#define		RAID_STATUS_EMERGENCY_READY	0x0007
#define		RAID_STATUS_EMERGENCY		0x0002
#define		RAID_STATUS_RECOVERING		0x0003
#define		RAID_STATUS_TERMINATING		0x0004
#define		RAID_STATUS_INITIAILIZING	0x0005
#define		RAID_STATUS_FAIL			0x0006
	UINT32			RaidStatus;

	NPAGED_LOOKASIDE_LIST	DataBufferLookaside;
	KSPIN_LOCK		LockInfo;				// Lock RaidStatus, BitmapIdxToRecover to protect recover thread

	UINT32			SectorsPerBit;			// 2^7(~ 2^11). From service
	UINT32			MaxBlocksPerRequest;	// From service. Normally 128
	HANDLE			ThreadRecoverHandle;	// Recover thread handle
	PVOID			ThreadRecoverObject;	// Receive termination notification
	KEVENT			RecoverThreadEvent;		// Used to terminate recover thread

	PRTL_BITMAP		Bitmap;					// Bitmap to record corruption
	ULONG			BitmapIdxToRecover;		// Used to protect sectors being recovered

	UINT32			iChildDefected;			// Disconnected device index in LURELATION_NODE.LurnChildren
	UINT32			nDiskCount;
	UINT32			nSpareDisk;

	NDAS_RAID_META_DATA rmd;				// ndasdib.h
	UINT32			iWriteLog;				// increase from 0
	WRITE_LOG		WriteLogs[NDAS_RAID_WRITE_LOG_SIZE];
	PLURELATION_NODE	MapLurnChildren[NDAS_MAX_RAID_CHILD];
} RAID_INFO, *PRAID_INFO;


//
//	in-line functions
//

#ifndef LSLURNASSOC_USE_INLINE_FUNCTIONS
#define LSLURNASSOC_USE_INLINE_FUNCTIONS 1
#endif

#if LSLURNASSOC_USE_INLINE_FUNCTIONS
#ifndef LSLURNASSOC_INLINE
#define LSLURNASSOC_INLINE __forceinline
#endif

LSLURNASSOC_INLINE
VOID 
LSAssocSetStatusFlag(
	PRAID_INFO RaidInfo, 
	PCCB Ccb
	)
{
	ULONG Flags = 0;
	if (RAID_STATUS_RECOVERING == RaidInfo->RaidStatus)
	{
		Flags |= CCBSTATUS_FLAG_RECOVERING;
	}
	else if (RAID_STATUS_EMERGENCY == RaidInfo->RaidStatus)
	{
		Flags |= CCBSTATUS_FLAG_LURN_STOP;
	}
	else if (RAID_STATUS_EMERGENCY_READY == RaidInfo->RaidStatus)
	{
		Flags |= CCBSTATUS_FLAG_LURN_STOP;
	}
	LSCcbSetStatusFlag(Ccb, Flags);
}

LSLURNASSOC_INLINE
BOOLEAN 
RAID_IS_RUNNING(
	PRAID_INFO RaidInfo
	)
{
	switch (RaidInfo->RaidStatus)
	{
	case RAID_STATUS_NORMAL:
	case RAID_STATUS_EMERGENCY:
	case RAID_STATUS_RECOVERING:
		return TRUE;
	}
	return FALSE;
}

#else

#define	RAID_IS_RUNNING(RI) (RAID_STATUS_NORMAL == (RI)->RaidStatus || RAID_STATUS_EMERGENCY == (RI)->RaidStatus || RAID_STATUS_RECOVERING == (RI)->RaidStatus)

#define LSAssocSetStatusFlag(RI, CCB_POINTER) LSCcbSetStatusFlag( (CCB_POINTER), \
	( (RAID_STATUS_RECOVERING == (RI)->RaidStatus) ? CCBSTATUS_FLAG_RECOVERING : (ULONG)0) | \
	( (RAID_STATUS_EMERGENCY == (RI)->RaidStatus) ? CCBSTATUS_FLAG_LURN_STOP : (ULONG)0) | \
	( (RAID_STATUS_EMERGENCY_READY == (RI)->RaidStatus) ? CCBSTATUS_FLAG_LURN_STOP : (ULONG)0) )

#endif
	

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