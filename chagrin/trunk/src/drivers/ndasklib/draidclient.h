#ifndef _DRAID_CLIENT_H
#define _DRAID_CLIENT_H

typedef enum _DRAID_CLIENT_STATE {

	DRAID_CLIENT_STATE_INITIALIZING = 0x01,
	DRAID_CLIENT_STATE_NO_ARBITER,
	DRAID_CLIENT_STATE_ARBITER_CONNECTED, 
	DRAID_CLIENT_STATE_TERMINATING

} DRAID_CLIENT_STATE;

#define DRAID_NODE_CHANGE_FLAG_CHANGED	0x01	// Node status change is detected, but not propagated to other clients
#define DRAID_NODE_CHANGE_FLAG_UPDATING 0x02	// Node status change is being propagated.

#define DRAID_CLIENT_CON_INIT 			0x00
#define DRAID_CLIENT_CON_CONNECTING 	0x01
#define DRAID_CLIENT_CON_CONNECTED 		0x02
#define DRAID_CLIENT_CON_REGISTERED 	0x03

typedef struct _DRAID_CLIENT_CONNECTION {

	KSPIN_LOCK			SpinLock;
	
	UINT32				Status;		// DRAID_CLIENT_CON_*

	LPX_ADDRESS			LocalAddr;
	LPX_ADDRESS			RemoteAddr;

	HANDLE				AddressFileHandle;
	PFILE_OBJECT		AddressFileObject;
	HANDLE				ConnectionFileHandle;
	PFILE_OBJECT		ConnectionFileObject;

	ULONG				Sequence;

	LARGE_INTEGER		Timeout;
	LONG				Waiting;	// Need to call recv this number of time more.
	LONG				RxPendingCount;
	UCHAR				ReceiveBuf[DRIX_MAX_REQUEST_SIZE];
	TDI_RECEIVE_CONTEXT TdiReceiveContext;

} DRAID_CLIENT_CONNECTION, *PDRAID_CLIENT_CONNECTION;

typedef struct _DRAID_CLIENT_INFO {

	KSPIN_LOCK			SpinLock;
	
	LIST_ENTRY			AllClientList;
	
	DRAID_CLIENT_STATE	ClientState;	// DRAID_CLIENT_STATE_*
	
	PLURELATION_NODE	Lurn;			// Link to parent Lurn node.

	GUID				RaidSetId;
	GUID				ConfigSetId;
	
	LIST_ENTRY			CcbQueue;
	
	// Local RAID information
	
	BOOLEAN				InTransition;								// TRUE if RAID/Node status is changing.
	UINT32				DRaidStatus;								// DRIX_RAID_STATUS_INITIALIZING, DRIX_RAID_STATUS_*
	UCHAR				NodeFlagsRemote[MAX_DRAID_MEMBER_DISK];  	// DRIX_NODE_FLAG_UNKNOWN, DRIX_NODE_FLAG_*

	// Node status reported by arbitor. IO based on this flag
	// Indexed by node number
	
	UCHAR				NodeFlagsLocal[MAX_DRAID_MEMBER_DISK];  	// DRIX_NODE_FLAG_UNKNOWN, DRIX_NODE_FLAG_*

	// Local node status. This status is reported to arbiter.

	UCHAR				NodeDefectLocal[MAX_DRAID_MEMBER_DISK];  	// Defect code. Valid only when  NodeFlagsLocal contains defective flag.

	// NDAS_UNIT_DEFECT_UNKNOWN, NDAS_UNIT_DEFECT_*
	
	UCHAR				RoleToNodeMap[MAX_DRAID_MEMBER_DISK];	// RAID role(array index) to unit number as in RMD 
	UCHAR				NodeToRoleMap[MAX_DRAID_MEMBER_DISK];

	NDAS_RAID_META_DATA	Rmd;
	UINT32				RequestSequence;

	UINT32				TotalDiskCount;
	UINT32				ActiveDiskCount;

	//	Flag that node change is propagated or not. Queue is not necessary assuming any lurn operation is held until status update is propagated.
	// 	DRAID_NODE_CHANGE_FLAG_*. 	
	//	CHANGED flag is set by ide thread and cleared by client thread after receiving(or failed to receive) raid update message.
	//	UPDATING flag is set by client when client detect CHANGED flag. Cleared when CHANGED flag is cleared.
	
	UCHAR				NodeChanged[MAX_DRAID_MEMBER_DISK];
	
	HANDLE				ClientThreadHandle;
	PVOID				ClientThreadObject;
	KEVENT				ClientThreadEvent;		// Wake up client thread to check any pending requests.
	KEVENT				ClientThreadReadyEvent;

	HANDLE				MonitorThreadHandle;
	PVOID				MonitorThreadObject;
	KEVENT				MonitorThreadEvent;		// Wake up client thread to check any pending requests.
	KEVENT				MonitorThreadReadyEvent;

	LIST_ENTRY			LockList; // List of DRAID_CLIENT_LOCK.Link

	LIST_ENTRY			PendingRequestList; // List of DRIX_MSG_CONTEXT. Contains request that is sent but not replied.
	
	BOOLEAN				HasLocalArbiter;	// Arbiter is in local.
	BOOLEAN				IsReadonly;			// This client is connected in read-only mode

	BOOLEAN					RequestToTerminate;

	// For handling CCB_OPCODE_FLUSH

	BOOLEAN					RequestForFlush;
	PCCB					FlushCcb;
	CCB_COMPLETION_ROUTINE	FlushCompletionRoutine;
	
	// Following members are only used for local arbiter
	
	DRIX_LOCAL_CHANNEL		NotificationChannel;
	DRIX_LOCAL_CHANNEL		RequestReplyChannel;
	PDRIX_LOCAL_CHANNEL		RequestChannel;	// reference to local arbiter's client context's member
	PDRIX_LOCAL_CHANNEL		NotificationReplyChannel;// reference to local arbiter's client context's member

	KEVENT					LocalClientTerminating;

	//	Following members are only used for remote arbiter.
	
	DRAID_CLIENT_CONNECTION NotificationConnection;
	DRAID_CLIENT_CONNECTION RequestConnection;

	// Client tried to connect arbiter based on this USN number.
	
	UINT32			LastTriedArbiterUsn;
	LARGE_INTEGER	LastTriedArbiterTime;

} DRAID_CLIENT_INFO, *PDRAID_CLIENT_INFO;


#define LOCK_STATUS_NONE 		(1<<0) // Lock is allocated. But not used yet
#define LOCK_STATUS_PENDING		(1<<3) // Sent message but lock is not available right now. waiting for grant 
#define LOCK_STATUS_GRANTED		(1<<4) // Lock is granted. 


// Guarded by Client's spinlock
// Lock info

typedef struct _DRAID_CLIENT_LOCK {

	UCHAR 			LockType;
	UCHAR 			LockMode;

	UINT64			LockAddress;		// in sector for blo
	UINT32			LockLength;

	UINT64			LockId;				// ID reccieved from arbiter.

	LIST_ENTRY		Link;				// Link to ClientInfo.LockList. List of all locks including lock to acquire
	
	UINT32			LockStatus;			// LOCK_STATUS_NONE, LOCK_STATUS_*

	LARGE_INTEGER	LastAccessedTime;
	LONG			InUseCount;			// Number of usage used by IO routine. Guarded by client's spinlock

	BOOLEAN			Contented;			// Lock is contented by other lock. It is preferred to release this lock as fast as possible.
	BOOLEAN			YieldRequested;		// Set to TRUE when arbiter sent req-to-yield message.

} DRAID_CLIENT_LOCK, *PDRAID_CLIENT_LOCK;


#endif /* _DRAID_CLIENT_H_ */

