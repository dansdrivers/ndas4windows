#ifndef _NDASR_ARBITRATOR_H
#define _NDASR_ARBITRATOR_H


// Allocated when accepting connection. Once register message is received this structure passed to arbiter thread.

typedef struct _NDASR_ARBITRATOR_CONNECTION {

	UCHAR				ConnType; // NRMX_CONN_TYPE_INFO, NRMX_CONN_TYPE_*

	HANDLE						ConnectionFileHandle;
	PFILE_OBJECT				ConnectionFileObject;
	LPXTDI_OVERLAPPED_CONTEXT	ReceiveOverlapped;
	UCHAR						ReceiveBuf[NRMX_MAX_REQUEST_SIZE];
	
	LPX_ADDRESS			RemoteAddr;

	BOOLEAN				NeedReply;
	NRMX_REGISTER		RegisterMsg;

} NDASR_ARBITRATOR_CONNECTION, *PNDASR_ARBITRATOR_CONNECTION;

// 
//	Connection context about clients. Members are guarded by arbiter's spinlock 
//
typedef struct _NDASR_CLIENT_CONTEXT {

	LIST_ENTRY	Link;

	BOOLEAN		UnregisterRequest;
	PKEVENT		UnregisterDoneEvent;
	
	// Node flags reported by client.(also initialized when arbiter set raid status)
	// Arbitrator's node status is converted to running status only when every host reports the node is running.
	
	UINT32		Usn;
	UCHAR		NodeFlags[MAX_NDASR_MEMBER_DISK]; 			// Ored value of NRMX_NODE_FLAG_*. Indexed by Node number
	UCHAR		DefectCodes[MAX_NDASR_MEMBER_DISK]; 		// Ored value of NRMX_NODE_DEFECT_*. Indexed by Node number

	UINT32		NotifySequence;

	LIST_ENTRY	AcquiredLockList;			// List of locks that this client is currently holding. Guarded with arbiter lock. only used when removing?
	
	// Following members are only used for remote client.

	BOOLEAN						LocalClient;
	UCHAR						RemoteClientAddr[6];
	PNDASR_ARBITRATOR_CONNECTION	NotificationConnection;
	PNDASR_ARBITRATOR_CONNECTION	RequestConnection;
	
	// to do: add reference count?
	
} NDASR_CLIENT_CONTEXT, *PNDASR_CLIENT_CONTEXT;

#define NDASR_ARBITRATOR_LOCK_STATUS_NONE		0x0   // Not initialized yet
#define NDASR_ARBITRATOR_LOCK_STATUS_GRANTED	0x1
#define NDASR_ARBITRATOR_LOCK_STATUS_PENDING	0x2

typedef struct _NDASR_ARBITRATOR_LOCK_CONTEXT {

	// Members for lock management
	
	LIST_ENTRY	ArbitratorAcquiredLink;	// Link that iterates all locks in a arbiter. Guard with arbiter spinlock.
	LIST_ENTRY	ClientAcquiredLink;		// Link used by client conext and recovery process. Guard with NDASR_CLIENT_CONTEXT spinlock.

	UCHAR		Status;
	
	// Lock info
	
	UCHAR		Type;
	UCHAR		Mode;

	UINT64		BlockAddress;		// in sector
	UINT32		BlockLength;

	UINT32		Granularity;		
	
	UINT64		Id;					// Per arbiter unique ID.
	
	PNDASR_CLIENT_CONTEXT Owner;	// Client that has acquired or is waiting for this lock. Can be NULL for rebuild IO.

} NDASR_ARBITRATOR_LOCK_CONTEXT, *PNDASR_ARBITRATOR_LOCK_CONTEXT;


#define NDASR_REBUILD_STATUS_NONE 		0x0 // No activity. Set by arbiter thread after acknowledge other status.
#define NDASR_REBUILD_STATUS_WORKING	0x1	// Copying
#define NDASR_REBUILD_STATUS_SUCCESS	0x2	// Currently rebuild request is completed.
#define NDASR_REBUILD_STATUS_FAILED		0x3	// Currently rebuild request is completed.
#define NDASR_REBUILD_STATUS_CANCELLED	0x4	// Currently rebuild request is completed.

#define NDASR_REBUILD_BUFFER_SIZE		(32 * 1024)

#define NDASR_AGGRESSIVE_REBUILD_SIZE (1024 * 1024 / 512)	// In sector count. 1Mbytes

typedef struct _NDASR_REBUILDER {

	struct _NDASR_ARBITRATOR	*NdasrArbitrator;

	HANDLE					ThreadHandle;
	PVOID					ThreadObject;
	KEVENT					ThreadEvent;
	KEVENT					ThreadReadyEvent;

	UINT32					Status;				// NDASR_REBUILD_STATUS_NONE, NDASR_REBUILD_STATUS_*

	BOOLEAN					ExitRequested;		// Set to TRUE by arbiter thread when arbiter exits
	BOOLEAN					CancelRequested;	// Set to TRUE by arbiter thread when arbiter want to cancel recovery.
	
	BOOLEAN					RebuildRequested;	// Set to TRUE by arbiter when it want to rebuild 
	PUCHAR					RebuildBuffer[MAX_NDASR_MEMBER_DISK];
	UINT32					UnitRebuildSize;

	UINT64					BlockAddress; 	// 	Address to rebuild
	UINT32					BlockLength;	//	Length to rebuild

	NTSTATUS				RebuildStatus;

	PNDASR_ARBITRATOR_LOCK_CONTEXT RebuildLock;

} NDASR_REBUILDER, *PNDASR_REBUILDER;


#define NO_OUT_OF_SYNC_ROLE ((UCHAR)-1)

typedef enum _NDASR_SYNC_STATE {

	NDASR_SYNC_DONE = 0, 
	NDASR_SYNC_REQUIRED,
	NDASR_SYNC_IN_PROGRESS

} NDASR_SYNC_STATE;

#define NDASR_ARBITRATOR_STATUS_INITIALIZING				0x00000001
#define NDASR_ARBITRATOR_STATUS_START						0x00000002
#define NDASR_ARBITRATOR_STATUS_TERMINATING				0x00000004
#define NDASR_ARBITRATOR_STATUS_TERMINATED					0x00000008

#define NDASR_ARBITRATOR_STATUS_SHUTDOWN					0x00000100

#define NDASR_ARBITRATOR_STATUS_REBUILD					0x00001000

#define NDASR_ARBITRATOR_STATUS_CLUSTER_NODE_UNREACHABLE	0x00010000

typedef struct _NDASR_ARBITRATOR {

	LIST_ENTRY			AllListEntry;	// Link to DRAID_GLOBALS.ArbitratorQueue

	PLURELATION_NODE 	Lurn;	// Parent node.

	KSPIN_LOCK			SpinLock;

	union {

		struct {

			ULONG		ArbitratorState0		: 8;
			ULONG		Shutdown8			: 4;
			ULONG		RebuildState12		: 4;
			ULONG		ClusterState16		: 4;
			ULONG		Reserved			: 12;
		};	

		ULONG			Status;			// NDASR_ARBITRATOR_STATUS_*
	};

	BOOLEAN				RequestToTerminate;

	BOOLEAN				RequestToShutdown;
	KEVENT				FinishShutdownEvent;	

	LIST_ENTRY			NewClientQueue;
	LIST_ENTRY			ClientQueue;
	LIST_ENTRY			TerminatedClientQueue;

	LIST_ENTRY			AcquiredLockList;
	LONG				AcquiredLockCount;
	UINT64				NextLockId;

	UINT32				LockRangeGranularity; // Mininum locking range. Currently set as SectorsPerBit

	UCHAR				NdasrState;								// NRMX_RAID_STATE_INITIALIZING, NRMX_RAID_STATE_*
	UCHAR				OutOfSyncRoleIndex;	// NO_OUT_OF_SYNC_ROLE when there is no out-of-sync node.

	NDASR_SYNC_STATE	SyncState;
	LARGE_INTEGER		DegradedSince;
	GUID				ConfigSetId;

	UCHAR				NodeFlags[MAX_NDASR_MEMBER_DISK]; 		// Ored value of NRMX_NODE_FLAG_*. Indexed by Node number
	UCHAR				DefectCodes[MAX_NDASR_MEMBER_DISK]; 	// Ored value of NRMX_NODE_DEFECT_*. Indexed by Node number

	UCHAR				NodeToRoleMap[MAX_NDASR_MEMBER_DISK]; 	// Unit number to RAID role mapping.
	UCHAR				RoleToNodeMap[MAX_NDASR_MEMBER_DISK]; 	// RAID role index to unit number map
			
	RTL_BITMAP 			OosBmpHeader;		// Out-of-sync bitmap
	PULONG 				OosBmpBuffer;
	ULONG				OosBmpBitCount; // number of bit in bitmap
	ULONG				OosBmpByteCount; // number of byte used for bitmap

	RTL_BITMAP 			LwrBmpHeader; // Bitmap for currently locked for writing. Same size of OOS BMP
	PULONG 				LwrBmpBuffer;

	PNDAS_OOS_BITMAP_BLOCK	OosBmpOnDiskBuffer;
	ULONG					OosBmpOnDiskSectorCount; // number of on-disk blocks in bitmap
	//BOOLEAN					DirtyBmpSector[NDAS_BLOCK_SIZE_BITMAP]; // Set to TRUE when on-disk BMP needs update.
	PBOOLEAN			DirtyBmpSector; // Set to TRUE when on-disk BMP needs update.

	HANDLE				ThreadHandle;
	PVOID				ThreadObject;
	KEVENT				ThreadEvent;		// Used to terminate arbiter thread
	KEVENT				ThreadReadyEvent;

	NDASR_REBUILDER		NdasrRebuilder;

	NTSTATUS (*AcceptClient) (struct _NDASR_ARBITRATOR		*NdasArbitrator,
							  PNRMX_REGISTER			RegisterMsg,
							  PNDASR_ARBITRATOR_CONNECTION	*Connection );

} NDASR_ARBITRATOR, *PNDASR_ARBITRATOR;

#define NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(offset)  (offset/NDAS_BYTE_PER_OOS_BITMAP_BLOCK)
#define NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_BYTE_OFFSET(offset)  \
	((offset) - NDASR_INCORE_BMP_BYTE_OFFSET_TO_ONDISK_SECTOR_NUM(offset) * NDAS_BYTE_PER_OOS_BITMAP_BLOCK)

#define NDASR_ONDISK_BMP_OFFSET_TO_INCORE_OFFSET(sect, offset) ((sect * NDAS_BYTE_PER_OOS_BITMAP_BLOCK) + offset)


typedef struct _NDASR_LISTEN_CONTEXT {

	LIST_ENTRY			Link;
	BOOLEAN				Started;
	BOOLEAN				Destroy;	// Set to TRUE when initialized and set to FALSE if this context should be removed.

	LPX_ADDRESS			Addr;

	HANDLE				AddressFileHandle;
	PFILE_OBJECT		AddressFileObject;

	HANDLE						ListenFileHandle;
	PFILE_OBJECT				ListenFileObject;
	LPXTDI_OVERLAPPED_CONTEXT	ListenOverlapped;

	ULONG				RequestOptions;
	ULONG				ReturnOptions;
	ULONG				Flags;
	
} NDASR_LISTEN_CONTEXT, * PNDASR_LISTEN_CONTEXT;


#endif /*  _NDASR_ARBITRATOR_H */

