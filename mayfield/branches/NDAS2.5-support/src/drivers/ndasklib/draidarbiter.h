#ifndef _DRAID_ARBITER_H
#define  _DRAID_ARBITER_H



typedef enum _DRAID_ARBITER_STATUS {
	DRAID_ARBITER_STATUS_INITALIZING = 0,
	DRAID_ARBITER_STATUS_RUNNING,
	DRAID_ARBITER_STATUS_TERMINATING
} DRAID_ARBITER_STATUS;

typedef enum _DRAID_SYNC_STATUS {
	DRAID_SYNC_DONE = 0, 
	DRAID_SYNC_REQUIRED,
	DRAID_SYNC_IN_PROGRESS
} DRAID_SYNC_STATUS;

//
// Allocated when accepting connection. Once register message is received this structure passed to arbiter thread.
//
typedef struct _DRAID_REMOTE_CLIENT_CONNECTION {
	UCHAR	ConnType; // DRIX_CONN_TYPE_INFO, DRIX_CONN_TYPE_*
	HANDLE			ConnectionFileHandle;
	PFILE_OBJECT		ConnectionFileObject;
	LPX_ADDRESS	RemoteAddr;

	BOOLEAN			Receiving;	// Set to TRUE when receiving is in pending.
	UCHAR			ReceiveBuf[DRIX_MAX_REQUEST_SIZE];
	TDI_RECEIVE_CONTEXT TdiReceiveContext;
} DRAID_REMOTE_CLIENT_CONNECTION, *PDRAID_REMOTE_CLIENT_CONNECTION;

// 
//	Connection context about clients. Members are guarded by arbiter's spinlock 
//
typedef struct _DRAID_CLIENT_CONTEXT {
//	KSPIN_LOCK	SpinLock;
	LIST_ENTRY	Link;

	BOOLEAN		UnregisterRequest;	
	KEVENT		UnregisterDoneEvent;
	
	//
	// Node flags reported by client.(also initialized when arbiter set raid status)
	// Arbiter's node status is converted to running status only when every host reports the node is running.
	//
	UCHAR NodeFlags[MAX_DRAID_MEMBER_DISK]; 			// Ored value of DRIX_NODE_FLAG_*. Indexed by Node number
	UCHAR DefectCode[MAX_DRAID_MEMBER_DISK]; 			// Ored value of DRIX_NODE_FLAG_*. Indexed by Node number
	
	UINT32		NotifySequence;
	//
	//	Notification channel(Aribter -> client)
	//
#if 0	// not used yet
	LIST_ENTRY		SentNotificationQueue;	// List of DRIX_MSG_CONTEXT. List of sent but not replied notifications.
#endif
	//
	// Connection/address object for notification channle
	//
	
	// 
	//	Request channel(Client -> arbiter)
	//
#if 0	// not used yet
	LIST_ENTRY		RequestQueue;	// List of DRIX_MSG_CONTEXT. List of received but not handled requests.
#endif

	LIST_ENTRY		AcquiredLockList;		// List of locks that this client is currently holding. Guarded with arbiter lock. only used when removing?

	LIST_ENTRY		PendingLockList;
	
	//
	// Connection/address object for request channel
	//

	BOOLEAN		Initialized;		// Set to TRUE when arbiter has sent current raid status.
	PDRAID_CLIENT_INFO	LocalClient; // Not NULL if Local client.

	//
	// Following members are only used for local client.(Didn't use direct function call to emulate operation over network)
	//
	PDRIX_LOCAL_CHANNEL	NotificationChannel;	// Reference client's
	PDRIX_LOCAL_CHANNEL 	RequestReplyChannel;// Reference client's
	
	DRIX_LOCAL_CHANNEL RequestChannel;
	DRIX_LOCAL_CHANNEL NotificationReplyChannel; // Client will wait reply to notification through this.

	//
	// Following members are only used for remote client.
	//
	UCHAR				RemoteClientAddr[6];
	PDRAID_REMOTE_CLIENT_CONNECTION	NotificationConnection;
	PDRAID_REMOTE_CLIENT_CONNECTION	RequestConnection;
	
	//
	// to do: add reference count?
	//
} DRAID_CLIENT_CONTEXT, *PDRAID_CLIENT_CONTEXT;

typedef struct _DRAID_ARBITER_LOCK_CONTEXT {
	// Lock info
	UCHAR LockType;
	UCHAR LockMode;

	UINT64 LockAddress;	// in sector
	UINT32 LockLength;

	UINT32 LockGranularity; // Lock Granularity is halfed if lock contention occurs.
	
	UINT64 LockId; // Per arbiter unique ID.

	// Members for lock management
	LIST_ENTRY	ClientAcquiredLink;	// Link used by client conext and recovery process. Guard with DRAID_CLIENT_CONTEXT spinlock.
	LIST_ENTRY	ArbiterAcquiredLink;		// Link that iterates all locks in a arbiter. Guard with arbiter spinlock.
	LIST_ENTRY	ToYieldLink;		// Link that iterates locks that requires yield. Guard with arbiter spinlock.
	LIST_ENTRY	PendingLink;	// Link that client->PendingLockList.Guard with arbiter spinlock.
	
	PDRAID_CLIENT_CONTEXT ClientAcquired; // Client that acquired this lock.
#if 0
	PFAST_MUTEX  InUse;	// This lock context is in use(i.e Communication which depends on this lock is in progress). This lock cannot be freed right now.
#endif
} DRAID_ARBITER_LOCK_CONTEXT, *PDRAID_ARBITER_LOCK_CONTEXT;


#define DRAID_REBUILD_STATUS_NONE 	0x0 	// No activity. Set by arbiter thread after acknowledge other status.
#define DRAID_REBUILD_STATUS_WORKING	0x1	// Copying
#define DRAID_REBUILD_STATUS_DONE	0x2	// Currently rebuild request is completed.
#define DRAID_REBUILD_STATUS_FAILED	0x3	// Currently rebuild request is completed.
#define DRAID_REBUILD_STATUS_CANCELLED 0x4	// Currently rebuild request is completed.

#define DRAID_REBUILD_BUFFER_SIZE		(32 * 1024)

#define DRAID_AGGRESSIVE_REBUILD_FRAGMENT 64

typedef struct _DRAID_REBUILD_INFO {
	HANDLE			ThreadHandle;
	PVOID			ThreadObject;
	KEVENT			ThreadEvent;

	UINT32		Status;  // DRAID_REBUILD_STATUS_NONE, DRAID_REBUILD_STATUS_*

	PDRAID_ARBITER_LOCK_CONTEXT RebuildLock;
	BOOLEAN		ExitRequested;	// Set to TRUE by arbiter thread when arbiter exits
	BOOLEAN		CancelRequested;	// Set to TRUE by arbiter thread when arbiter want to cancel recovery.
	BOOLEAN		RebuildRequested;		// Set to TRUE by arbiter when it want to rebuild 
	PUCHAR		RebuildBuffer[MAX_DRAID_MEMBER_DISK];
	UINT32		UnitRebuildSize;


	BOOLEAN		AggressiveRebuildMode;	// In Aggressive rebuild mode, rebuild is done in small range and cannot be cancelled
#if 0	// to do: add partial recovery
	ULONG		AggressiveRebuildBit;	// Bit that is being rebuilt by aggressive rebuild.
	RTL_BITMAP 	AggressiveRebuildMapHeader; // Out-of-sync bitmap
	ULONG		AggressiveRebuildMapBuffer[DRAID_AGGRESSIVE_REBUILD_FRAGMENT/(sizeof(ULONG)*8)];
#endif

	UINT64		Addr; 	// 	Address to rebuild
	UINT32		Length;	//	Length to rebuild
	
} DRAID_REBUILD_INFO, *PDRAID_REBUILD_INFO;

#define NO_OUT_OF_SYNC_ROLE ((UCHAR)-1)

typedef struct _DRAID_ARBITER_INFO {
	KSPIN_LOCK		SpinLock;

	PLURELATION_NODE 	Lurn;	// Parent node.
	DRAID_ARBITER_STATUS	Status;  // DRAID_ARBITER_STATUS_*

	BOOLEAN	ReadOnlyMode;
	UINT64	NextLockId;
	
	UINT32	TotalDiskCount;
	UINT32	ActiveDiskCount;

	UINT32 RaidStatus;								// DRIX_RAID_STATUS_INITIALIZING, DRIX_RAID_STATUS_*
	UCHAR NodeFlags[MAX_DRAID_MEMBER_DISK]; 			// Ored value of DRIX_NODE_FLAG_*. Indexed by Node number
	UCHAR DefectCodes[MAX_DRAID_MEMBER_DISK]; 			// Ored value of DRIX_NODE_FLAG_*. Indexed by Node number
	UCHAR NodeToRoleMap[MAX_DRAID_MEMBER_DISK]; 	// Unit number to RAID role mapping.
	UCHAR RoleToNodeMap[MAX_DRAID_MEMBER_DISK]; 	// RAID role index to unit number map

	UCHAR OutOfSyncRole;	// NO_OUT_OF_SYNC_ROLE when there is no out-of-sync node.
	
	HANDLE			ThreadArbiterHandle;
	PVOID			ThreadArbiterObject;
	KEVENT			ArbiterThreadEvent;		// Used to terminate arbiter thread
	
	LARGE_INTEGER	DegradedSince;

	NDAS_RAID_META_DATA	Rmd;		// Same as on-disk RMD format.


	UINT32	LockRangeGranularity; // Mininum locking range. Currently set as SectorsPerBit
	LIST_ENTRY		ClientList;
	PDRAID_CLIENT_CONTEXT LocalClient;

	LIST_ENTRY		AcquiredLockList;
	LONG			AcquiredLockCount;

	LIST_ENTRY		ToYieldLockList;

	DRAID_SYNC_STATUS	SyncStatus;
	
	RTL_BITMAP 	OosBmpHeader; // Out-of-sync bitmap
	UINT32		SectorsPerOosBmpBit;
	PUCHAR 		OosBmpInCoreBuffer;
	ULONG		OosBmpBitCount; // number of bit in bitmap
	ULONG		OosBmpByteCount; // number of byte used for bitmap
	PNDAS_OOS_BITMAP_BLOCK OosBmpOnDiskBuffer;
	ULONG		OosBmpSectorCount; // number of on-disk blocks in bitmap
	BOOLEAN		DirtyBmpSector[NDAS_BLOCK_SIZE_LAST_WRITTEN_REGION]; // Set to TRUE when on-disk BMP needs update.
	
	PNDAS_LAST_WRITTEN_REGION_BLOCK LwrBlocks; // Last/Locked write region.Length NDAS_BLOCK_SIZE_LWR * block size,  8k

	DRAID_REBUILD_INFO	RebuildInfo;

	LIST_ENTRY		AllArbiterList;	// Link to DRAID_GLOBALS.ArbiterList	
} DRAID_ARBITER_INFO, *PDRAID_ARBITER_INFO;

#define DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(offset)  (offset/NDAS_BYTE_PER_OOS_BITMAP_BLOCK)
#define DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(offset)  \
	((offset) - DRAID_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(offset) * NDAS_BYTE_PER_OOS_BITMAP_BLOCK)

#define DRAID_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(sect, offset) ((sect * NDAS_BYTE_PER_OOS_BITMAP_BLOCK) + offset)




typedef struct _DRAID_LISTEN_CONTEXT {
	LIST_ENTRY	Link;
	BOOLEAN		Started;
	BOOLEAN		Destroy;	// Set to TRUE when initialized and set to FALSE if this context should be removed.

	LPX_ADDRESS Addr;

	HANDLE				AddressFileHandle;
	PFILE_OBJECT			AddressFileObject;
	HANDLE				ListenFileHandle;
	PFILE_OBJECT			ListenFileObject;
	ULONG				Flags;

	TDI_LISTEN_CONTEXT	TdiListenContext;
	
} DRAID_LISTEN_CONTEXT, * PDRAID_LISTEN_CONTEXT;


NTSTATUS 
DraidArbiterStart(
	PLURELATION_NODE	Lurn
	);

NTSTATUS
DraidArbiterStop(
	PLURELATION_NODE	Lurn
	);

NTSTATUS
DraidRegisterLocalClientToArbiter(
	PLURELATION_NODE	Lurn,
	PDRAID_CLIENT_INFO  pClient
);

NTSTATUS
DraidUnregisterLocalClient(
	PLURELATION_NODE	Lurn
);

NTSTATUS
DraidArbiterAcceptClient(
	PDRAID_ARBITER_INFO Arbiter,
	UCHAR ConnType,
	PDRAID_REMOTE_CLIENT_CONNECTION Channel
);

#endif /*  _DRAID_ARBITER_H */

