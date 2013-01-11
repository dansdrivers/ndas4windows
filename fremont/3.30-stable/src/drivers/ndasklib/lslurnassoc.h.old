#ifndef LANSCSI_LURN_ASSOCIATE_H
#define LANSCSI_LURN_ASSOCIATE_H


#define LURNASSOC_COMPLETE_POOL_TAG			'clSL'

#define AGGR_MODEL_NAME			{'A','g','g','r','e','g','a','t','e','d',0,0,0,0,0,0}
#define MIRR_MODEL_NAME			{'M','i','r','r','o','r','e','d',0,0,0,0,0,0,0,0}
#define RAID0_MODEL_NAME		{'R','A','I','D',' ','0',0,0,0,0,0,0,0,0,0,0}
#define RAID1R_MODEL_NAME		{'R','A','I','D','1','R','3',0,0,0,0,0,0,0,0,0}
#define RAID4R_MODEL_NAME		{'R','A','I','D','4','R','3',0,0,0,0,0,0,0,0,0}

extern LURN_INTERFACE LurnAggrInterface;
//extern LURN_INTERFACE LurnMirrorInterface;
extern LURN_INTERFACE LurnRAID0Interface;
extern LURN_INTERFACE LurnRAID1RInterface;
extern LURN_INTERFACE LurnRAID4RInterface;


#define		RAID_INFO_POOL_TAG					'TPIR'
#define		RAID_DATA_BUFFER_POOL_TAG			'BDDR'

#define		DRAID_BITMAP_POOL_TAG				'PMTB'

#define 	DRAID_ARBITER_INFO_POOL_TAG			'IARD'
#define 	DRAID_CLIENT_INFO_POOL_TAG			'ICRD'
#define 	DRAID_CLIENT_CONTEXT_POOL_TAG		'CCRD'
#define 	DRAID_ARBITER_NOTIFY_MSG_POOL_TAG	'MNRD'
#define 	DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG	'RNRD'
#define 	DRAID_CLIENT_REQUEST_MSG_POOL_TAG	'MCRD'
#define 	DRAID_CLIENT_REQUEST_REPLY_POOL_TAG	'RCRD'

#define 	DRAID_MSG_LINK_POOL_TAG				'LMRD'
#define 	DRAID_CLIENT_LOCK_POOL_TAG			'LCRD'
#define 	DRAID_ARBITER_LOCK_POOL_TAG			'LARD'
#define		DRAID_REBUILD_BUFFER_POOL_TAG		'BRRD'
#define		DRAID_LISTEN_CONTEXT_POOL_TAG		'CLRD'
#define		DRAID_EVENT_ARRAY_POOL_TAG			'AERD'
#define		DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG 'CRRD'
#define		DRAID_LOCKED_RANGE_BITMAP_POOL_TAG	'RLRD'
#define		DRAID_SHUTDOWN_POOL_TAG				'DSRD'
#define 	DRAID_ARBITER_LOCK_ADDRLEN_PAIR_POOL_TAG 'PARD'

#define NDAS_MAX_RAID0_CHILD 8 // RAID 0 supports 8
#define NDAS_MAX_RAID1R_CHILD 2 // RAID 1R supports 2
#define NDAS_MAX_RAID4R_CHILD 9 // RAID 4R supports 9
#define NDAS_MAX_RAID_CHILD NDAS_MAX_RAID4R_CHILD


#ifndef HZ
#define HZ                (LONGLONG)(10 * 1000 * 1000)
#endif

#define NDAS_RAID_SPARE_HOLDING_TIMEOUT		(HZ * 3 * 60) // Do not use spare for this amount time. 
//#define NDAS_RAID_SPARE_HOLDING_TIMEOUT		(HZ * 30) // temp short time

typedef struct _DATA_BUFFER {
	PCHAR		DataBuffer;
	UINT32		DataBufferLenght;
} DATA_BUFFER, *PDATA_BUFFER;

typedef struct _CUSTOM_DATA_BUFFER {

	BOOLEAN		DataBufferAllocated;
	UINT32		DataBufferCount;
	UINT64		DataBlockAddress[NDAS_MAX_RAID_CHILD];
	UINT32		DataBlockLength[NDAS_MAX_RAID_CHILD];
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

#define		RAID_STATUS_NORMAL			0x0000
#define		RAID_STATUS_EMERGENCY		0x0002
#define		RAID_STATUS_RECOVERING		0x0003
#define		RAID_STATUS_TERMINATING		0x0004
#define		RAID_STATUS_INITIAILIZING	0x0005
#define		RAID_STATUS_FAIL			0x0006
#define		RAID_STATUS_EMERGENCY_READY	0x0007

#define DRAID_NODE_CHANGE_FLAG_CHANGED	0x01	// Node status change is detected, but not propagated to other clients
#define DRAID_NODE_CHANGE_FLAG_UPDATING 0x02	// Node status change is being propagated.


typedef struct _RAID_INFO {

	KSPIN_LOCK				SpinLock;				// Lock RaidStatus, BitmapIdxToRecover to protect recover thread

	UINT32					MaxDataSendLength;		// From service. Normally 65536
	UINT32					MaxDataRecvLength;		// From service. Normally 65536
	NPAGED_LOOKASIDE_LIST	DataBufferLookaside;
	UINT32					SectorsPerBit;			// From service. Service read from DIBv2.iSectorsPerBit

	UINT32					ActiveDiskCount;		// Number of RAID member disk excluding spare.
	UINT32					SpareDiskCount;
	GUID					RaidSetId;
	GUID					ConfigSetId;
	
	// Index in this array is based on role(not unit device index)
	// i.e. in RAID1, index 0 and 1 are mirrored disks and index 2 is spare.
	// Will be removed. For RAID0, aggregation, lurn unit is work as role 
	// For RAID1~, NdasClient will have this mapping.
	// Entry will be NULL if disk is defective.

	PLURELATION_NODE		RoleIndexLurnChildren[MAX_NDASR_MEMBER_DISK];	

	// Local RAID information
	
	BOOLEAN				InTransition;						// TRUE if RAID/Node status is changing.
	UCHAR				DRaidState;							// DRIX_RAID_STATE_INITIALIZING, DRIX_RAID_STATE_*
	UINT32				Usn;
	BOOLEAN				WaitSyncForWrite;

	// Node status reported by arbitor. IO based on this flag
	// Indexed by node number

	UCHAR				NodeFlagsRemote[MAX_NDASR_MEMBER_DISK]; // DRIX_NODE_FLAG_UNKNOWN, DRIX_NODE_FLAG_*

	// Local node status. This status is reported to arbiter.

	UCHAR				NodeFlagsLocal[MAX_NDASR_MEMBER_DISK];  // DRIX_NODE_FLAG_UNKNOWN, DRIX_NODE_FLAG_*

	UCHAR				NodeDefectLocal[MAX_NDASR_MEMBER_DISK]; // Defect code. Valid only when  NodeFlagsLocal contains defective flag.

	// NDAS_UNIT_DEFECT_UNKNOWN, NDAS_UNIT_DEFECT_*
	
	UCHAR				RoleToNodeMap[MAX_NDASR_MEMBER_DISK];	// RAID role(array index) to unit number as in RMD 
	UCHAR				NodeToRoleMap[MAX_NDASR_MEMBER_DISK];

	//	Flag that node change is propagated or not. Queue is not necessary assuming any lurn operation is held until status update is propagated.
	// 	DRAID_NODE_CHANGE_FLAG_*. 	
	//	CHANGED flag is set by ide thread and cleared by client thread after receiving(or failed to receive) raid update message.
	//	UPDATING flag is set by client when client detect CHANGED flag. Cleared when CHANGED flag is cleared.

	UCHAR				NodeChanged[MAX_NDASR_MEMBER_DISK];

	PVOID				NdasArbiter;	// Not NULL only for primary host.
	PVOID				NdasClient;	// Not NULL only for non-dedundent RAID.

} RAID_INFO, *PRAID_INFO;


#define LUR_IS_PRIMARY(Lur) 	(((Lur)->DeviceMode ==  DEVMODE_SHARED_READWRITE && !((Lur)->EnabledNdasFeatures & NDASFEATURE_SECONDARY)) || \
									(Lur)->DeviceMode == DEVMODE_EXCLUSIVE_READWRITE)
#define LUR_IS_SECONDARY(Lur) 	(((Lur)->DeviceMode ==  DEVMODE_SHARED_READWRITE && ((Lur)->EnabledNdasFeatures & NDASFEATURE_SECONDARY)))
#define LUR_IS_READONLY(Lur)	((Lur)->DeviceMode ==  DEVMODE_SHARED_READONLY)

typedef enum _LURN_ASSOC_CASCADE_OPTION {

	LURN_CASCADE_FORWARD = 0,		// Send request from first children. If request is non-blocking, request will be sent to children at once
	LURN_CASCADE_FORWARD_CHAINING,	// Process one request after another. Handle in child's order. Process Ccb without blocking requester.
	LURN_CASCADE_BACKWARD,			// Send request from last children. 
	LURN_CASCADE_BACKWARD_CHAINING,	// Process one request after another. Handle in child's reverse order. Currently not used.

} LURN_CASCADE_OPTION;

typedef struct _LURN_RAID_SHUT_DOWN_PARAM {

	PIO_WORKITEM 		IoWorkItem;
	PLURELATION_NODE	Lurn;
	PCCB				Ccb;

} LURN_RAID_SHUT_DOWN_PARAM, *PLURN_RAID_SHUT_DOWN_PARAM;


//////////////////////////////////////////////////////////////////////////
//
//	lslurnassoc.c
//

VOID 
LSAssocSetRedundentRaidStatusFlag (
	PLURELATION_NODE Lurn,
	PCCB Ccb
	);

NTSTATUS
LurnAssocSendCcbToAllChildren (
	IN PLURELATION_NODE			Lurn,
	IN PCCB						Ccb,
	IN CCB_COMPLETION_ROUTINE	CcbCompletion,
	IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
	IN PVOID					*apExtendedCmd,
	IN LURN_CASCADE_OPTION		AssociateCascade
	);

NTSTATUS
LurnAssocRefreshCcbStatusFlag (
	IN PLURELATION_NODE			pLurn,
	PULONG						CcbStatusFlags
	);

// Temporary export.
void
LurnSetRaidInfoStatus (
	PRAID_INFO	pRaidInfo,
	UINT32		RaidStatus
	);

// temp export
NTSTATUS
LurnExecuteSyncMulti (
	IN ULONG			NrLurns,
	IN PLURELATION_NODE	Lurn[],
	IN UCHAR			CDBOperationCode,
	IN PCHAR			DataBuffer[],
	IN UINT64			BlockAddress,
	IN UINT16			BlockTransfer,
	IN PCMD_COMMON		ExtendedCommand
	);


NTSTATUS
LurnExecuteSyncWrite (
	IN PLURELATION_NODE Lurn,
	IN OUT PUCHAR		data_buffer,
	IN INT64			logicalBlockAddress,
	IN UINT32			transferBlocks
	);

NTSTATUS
LurnExecuteSyncRead (
	IN PLURELATION_NODE	Lurn,
	OUT PUCHAR			data_buffer,
	IN INT64			logicalBlockAddress,
	IN UINT32			transferBlocks
	);

NTSTATUS
LurnRMDRead (
	IN PLURELATION_NODE			Lurn, 
	OUT PNDAS_RAID_META_DATA	rmd,
	OUT PUINT32					UpTodateNode
	);

void
LurnSetOutOfSyncBitmapAll (
	IN PLURELATION_NODE		Lurn
	);

NTSTATUS
LurnRefreshRaidStatus (
	PLURELATION_NODE	Lurn,
	UINT32				*new_raid_status
	);

NTSTATUS
LurnAssocSendCcbToChildrenArray (
	IN PLURELATION_NODE			*pLurnChildren,
	IN LONG						ChildrenCnt,
	IN PCCB						Ccb,
	IN CCB_COMPLETION_ROUTINE	CcbCompletion,
	IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
	IN PVOID					*apExtendedCmd, // NULL if no cmd
	IN LURN_CASCADE_OPTION		AssociateCascade
	);

NTSTATUS
LurnRAID1RCcbCompletion (
	IN PCCB	Ccb,
	IN PCCB	OriginalCcb
	);

#endif
