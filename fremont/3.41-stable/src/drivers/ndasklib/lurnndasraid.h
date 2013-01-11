#ifndef NDAS_RAID_H
#define NDAS_RAID_H


#define NDAS_AGGREGATION_MODEL_NAME	"Aggregation"
#define NDAS_RAID0_MODEL_NAME		"Raid0"
#define NDAS_RAID1_MODEL_NAME		"Raid1"
#define NDAS_RAID4_MODEL_NAME		"Raid4"
#define NDAS_RAID5_MODEL_NAME		"Raid5"

extern LURN_INTERFACE LurnNdasAggregationInterface;
extern LURN_INTERFACE LurnNdasRaid0Interface;
extern LURN_INTERFACE LurnNdasRaid1Interface;
extern LURN_INTERFACE LurnNdasRaid4Interface;
extern LURN_INTERFACE LurnNdasRaid5Interface;


#define		NDASR_INFO_POOL_TAG						'TPIN'
#define		NDASR_DATA_BUFFER_POOL_TAG				'BDDN'

#define		NDASR_BITMAP_POOL_TAG					'PMTN'

#define 	NDASR_ARBITRATOR_POOL_TAG				'IARN'
#define 	NDASR_CLIENT_POOL_TAG					'ICRN'
#define 	NDASR_CLIENT_CONTEXT_POOL_TAG			'CCRN'
#define 	NDASR_ARBITRATOR_NOTIFY_MSG_POOL_TAG	'MNRN'
#define 	NDASR_ARBITRATOR_NOTIFY_REPLY_POOL_TAG	'RNRN'
#define 	NDASR_CLIENT_REQUEST_MSG_POOL_TAG		'MCRN'
#define 	NDASR_CLIENT_REQUEST_REPLY_POOL_TAG		'RCRN'

#define 	NDASR_MSG_LINK_POOL_TAG					'LMRN'
#define 	NDASR_CLIENT_LOCK_POOL_TAG				'LCRN'
#define 	NDASR_ARBITRATOR_LOCK_POOL_TAG			'LARN'
#define		NDASR_REBUILD_BUFFER_POOL_TAG			'BRRN'
#define		NDASR_LISTEN_CONTEXT_POOL_TAG			'CLRN'
#define		NDASR_EVENT_ARRAY_POOL_TAG				'AERN'
#define		NDASR_REMOTE_CLIENT_CHANNEL_POOL_TAG	'CRRN'
#define		NDASR_LOCKED_RANGE_BITMAP_POOL_TAG		'RLRN'
#define		NDASR_SHUTDOWN_POOL_TAG					'DSRN'
#define 	NDASR_ARBITRATOR_LOCK_ADDRLEN_PAIR_POOL_TAG 'PARN'
#define		NDASR_OVERLAP_DATA_POOL_TAG				'CLON'
#define		NDASR_LOCAL_MSG_POOL_TAG				'mLDN'

#define MAX_NDAS_AGGREATION_CHILD	8 // RAID 1 supports 2
#define MAX_NDAS_RAID0_CHILD		8 // RAID 1 supports 2
#define MAX_NDAS_RAID1_CHILD		2 // RAID 1 supports 2
#define MAX_NDAS_RAID4_CHILD		8 // RAID 1 supports 2
#define MAX_NDAS_RAID5_CHILD		8 // RAID 1 supports 2

#define MAX_NDAS_RAID_CHILD		NRMX_MAX_MEMBER_DISK

#define NDAS_RAID_SPARE_HOLDING_TIMEOUT	(30 * NANO100_PER_SEC) // Do not use spare for this amount time. 


typedef struct _NDASR_CUSTOM_DATA_BUFFER {

	BOOLEAN		DataBufferAllocated;
	UINT32		DataBufferCount;
	UINT64		DataBlockAddress[MAX_NDAS_RAID_CHILD];
	UINT32		DataBlockLength[MAX_NDAS_RAID_CHILD];
	PCHAR		DataBuffer[MAX_NDAS_RAID_CHILD];
	UINT32		DataBufferLength[MAX_NDAS_RAID_CHILD];

} NDASR_CUSTOM_DATA_BUFFER, *PNDASR_CUSTOM_DATA_BUFFER;


typedef enum _NDASR_CASCADE_OPTION {

	NDASR_CASCADE_FORWARD = 0,			// Send request from first children. If request is non-blocking, request will be sent to children at once
	NDASR_CASCADE_FORWARD_CHAINING,		// Process one request after another. Handle in child's order. Process Ccb without blocking requester.
	NDASR_CASCADE_BACKWARD,				// Send request from last children. 
	NDASR_CASCADE_BACKWARD_CHAINING,	// Process one request after another. Handle in child's reverse order. Currently not used.

} NDASR_CASCADE_OPTION;


typedef struct _LURN_NDASR_SHUT_DOWN_PARAM {

	PIO_WORKITEM 		IoWorkItem;
	PLURELATION_NODE	Lurn;
	PCCB				Ccb;

} LURN_NDASR_SHUT_DOWN_PARAM, *PLURN_NDASR_SHUT_DOWN_PARAM;

typedef struct _NDASR_LOCAL_MSG {

	LIST_ENTRY		ListEntry;
	PNRMX_HEADER	NrmxHeader;

} NDASR_LOCAL_MSG, *PNDASR_LOCAL_MSG;

typedef struct _NDASR_LOCAL_CHANNEL {

	KSPIN_LOCK	SpinLock;
	
	KEVENT		RequestEvent;
	LIST_ENTRY	RequestQueue;

	KEVENT		ReplyEvent;
	LIST_ENTRY	ReplyQueue;

	BOOLEAN		Terminated;

} NDASR_LOCAL_CHANNEL, *PNDASR_LOCAL_CHANNEL;

__inline
NdasrLocalChannelInitialize ( 
	PNDASR_LOCAL_CHANNEL	NdasrLocalChannel
	)
{
	KeInitializeSpinLock( &NdasrLocalChannel->SpinLock );							

	KeInitializeEvent( &NdasrLocalChannel->RequestEvent, NotificationEvent, FALSE );	
	InitializeListHead( &NdasrLocalChannel->RequestQueue );	

	KeInitializeEvent( &NdasrLocalChannel->ReplyEvent, NotificationEvent, FALSE );	
	InitializeListHead( &NdasrLocalChannel->ReplyQueue );							
}


#define NDASR_NODE_CHANGE_FLAG_CHANGED	0x01	// Node status change is detected, but not propagated to other clients
#define NDASR_NODE_CHANGE_FLAG_UPDATING 0x02	// Node status change is being propagated.

typedef struct _NDASR_INFO {

	KSPIN_LOCK				SpinLock;				// Lock RaidStatus, BitmapIdxToRecover to protect recover thread
	ERESOURCE				BufLockResource;

	GUID					NdasRaidId;
	GUID					ConfigSetId;			// not used

	UINT32					BlocksPerBit;			// From service. Service read from DIBv2.iSectorsPerBit

	UCHAR					ActiveDiskCount;		// Number of RAID member disk excluding spare.
	UCHAR					ParityDiskCount;
	UCHAR					SpareDiskCount;

	BOOLEAN					Striping;
	BOOLEAN					DistributedParity;

	UINT32					MaxDataSendLength;		// From service. 
	UINT32					MaxDataRecvLength;		// From service. 

	// Local node status. This status is reported to arbiter.

	UCHAR					LocalNodeFlags[NRMX_MAX_MEMBER_DISK];  // NRMX_NODE_FLAG_RUNNING, DRIX_NODE_FLAG_*

	//	Flag that node change is propagated or not. Queue is not necessary assuming any lurn operation is held until status update is propagated.
	// 	DRAID_NODE_CHANGE_FLAG_*. 	
	//	CHANGED flag is set by ide thread and cleared by client thread after receiving(or failed to receive) raid update message.
	//	UPDATING flag is set by client when client detect CHANGED flag. Cleared when CHANGED flag is cleared.

	UCHAR					LocalNodeChanged[NRMX_MAX_MEMBER_DISK];

	NDAS_RAID_META_DATA		Rmd;
	BOOLEAN					NodeIsUptoDate[NRMX_MAX_MEMBER_DISK];
	UCHAR					UpToDateNode;

	UINT32						LocalClientRegisterUsn;
	struct _NDASR_ARBITRATOR	*NdasrArbitrator;	// Not NULL only for primary host.
	struct _NDASR_CLIENT		*NdasrClient;		// Not NULL only for non-dedundent RAID.

	NDASR_LOCAL_CHANNEL		RequestChannel;
	NDASR_LOCAL_CHANNEL		NotitifyChannel;

	NDASR_LOCAL_MSG	RequestChannelReply;

	union {

		UCHAR			RequestChannelReplyBuffer[NRMX_MAX_REQUEST_SIZE];
		NRMX_HEADER		NrmxHeader;
	};

} NDASR_INFO, *PNDASR_INFO;

#define NDASR_GLOBALS_FLAG_INITIALIZE	0x00000001

typedef struct _NDASR_GLOBALS {

	// DRAID listening thread.

	ULONG			Flags;
	LONG			ReferenceCount;

	KEVENT			StartEvent;

	KEVENT			DraidThreadReadyEvent;

	HANDLE			DraidThreadHandle;
	PVOID			DraidThreadObject;
	KEVENT			DraidExitEvent;

	LIST_ENTRY		IdeDiskQueue;
	KSPIN_LOCK		IdeDiskQSpinlock;

	LIST_ENTRY		ArbitratorQueue;
	KSPIN_LOCK		ArbitratorQSpinlock;

	LIST_ENTRY		ClientQueue;
	KSPIN_LOCK		ClientQSpinlock;

	KEVENT			NetChangedEvent;

	LIST_ENTRY		ListenContextList;	// List of DRAID_LISTEN_CONTEXT
	KSPIN_LOCK		ListenContextSpinlock;

	LONG			ReceptionThreadCount;	// Required to check all reception thread is finished.
	
	HANDLE			TdiClientBindingHandle;

	ULONG			OsMajorVersion;
	ULONG			OsMinorVersion;

} NDASR_GLOBALS, *PNDASR_GLOBALS;

#define NT_MAJOR_VERSION	5
#define W2K_MINOR_VERSION	0

extern NDASR_GLOBALS NdasrGlobalData;

#endif