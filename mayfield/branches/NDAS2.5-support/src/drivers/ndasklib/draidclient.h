#ifndef _DRAID_CLIENT_H
#define _DRAID_CLIENT_H

typedef enum _DRAID_CLIENT_STATUS {
	DRAID_CLIENT_STATUS_INITIALIZING = 0x01,
	DRAID_CLIENT_STATUS_NO_ARBITER,
	DRAID_CLIENT_STATUS_ARBITER_CONNECTED, 
	DRAID_CLIENT_STATUS_TERMINATING
} DRAID_CLIENT_STATUS;

#define DRAID_NODE_CHANGE_FLAG_CHANGED	0x01	// Node status change is detected, but not propagated to other clients
#define DRAID_NODE_CHANGE_FLAG_UPDATING 0x02	// Node status change is being propagated.

#define DRAID_CLIENT_CON_INIT 				0x00
#define DRAID_CLIENT_CON_CONNECTING 		0x01
#define DRAID_CLIENT_CON_CONNECTED 		0x02
#define DRAID_CLIENT_CON_REGISTERED 		0x03

typedef struct _DRAID_CLIENT_CONNECTION {
	KSPIN_LOCK		SpinLock;
	
	UINT32	Status;		// DRAID_CLIENT_CON_*

	LPX_ADDRESS	LocalAddr;
	LPX_ADDRESS	RemoteAddr;

	HANDLE					AddressFileHandle;
	PFILE_OBJECT				AddressFileObject;
	HANDLE					ConnectionFileHandle;
	PFILE_OBJECT				ConnectionFileObject;

	ULONG					Sequence;

	UCHAR					 ReceiveBuf[DRIX_MAX_REQUEST_SIZE];
	TDI_RECEIVE_CONTEXT 	TdiReceiveContext;
} DRAID_CLIENT_CONNECTION, *PDRAID_CLIENT_CONNECTION;

typedef struct _DRAID_CLIENT_INFO {
	KSPIN_LOCK		SpinLock;
	DRAID_CLIENT_STATUS			ClientStatus; // DRAID_CLIENT_STATUS_*
	PLURELATION_NODE	Lurn;	// Link to parent Lurn node.
	GUID	RaidSetId;

	BOOLEAN			RequestToTerminate;

	LIST_ENTRY	CcbQueue;
	//
	// Local RAID information
	//
	BOOLEAN   InTransition; // TRUE if RAID/Node status is changing.
	UINT32 DRaidStatus;	// DRIX_RAID_STATUS_INITIALIZING, DRIX_RAID_STATUS_*
	UCHAR NodeFlagsRemote[MAX_DRAID_MEMBER_DISK];  	// DRIX_NODE_FLAG_UNKNOWN, DRIX_NODE_FLAG_*
								// Node status reported by arbitor. IO based on this flag
								// Indexed by node number
	UCHAR NodeFlagsLocal[MAX_DRAID_MEMBER_DISK];  	// DRIX_NODE_FLAG_UNKNOWN, DRIX_NODE_FLAG_*
								// Local node status. This status is reported to arbiter.
	UCHAR NodeDefectLocal[MAX_DRAID_MEMBER_DISK];  	// Defect code. Valid only when  NodeFlagsLocal contains defective flag.
														// NDAS_UNIT_DEFECT_UNKNOWN, NDAS_UNIT_DEFECT_*
	UCHAR RoleToNodeMap[MAX_DRAID_MEMBER_DISK];	// RAID role(array index) to unit number as in RMD 
	UCHAR NodeToRoleMap[MAX_DRAID_MEMBER_DISK];

	NDAS_RAID_META_DATA Rmd;
	UINT32 RequestSequence;

	UINT32 TotalDiskCount;
	UINT32 ActiveDiskCount;

	//
	//	Flag that node change is propagated or not. Queue is not necessary assuming any lurn operation is held until status update is propagated.
	// 	DRAID_NODE_CHANGE_FLAG_*. 	
	//	CHANGED flag is set by ide thread and cleared by client thread after receiving(or failed to receive) raid update message.
	//	UPDATING flag is set by client when client detect CHANGED flag. Cleared when CHANGED flag is cleared.
	UCHAR NodeChanged[MAX_DRAID_MEMBER_DISK];
	
	HANDLE			ClientThreadHandle;
	PVOID			ClientThreadObject;
	KEVENT			ClientThreadEvent;		// Wake up client thread to check any pending requests.

	HANDLE			MonitorThreadHandle;
	PVOID			MonitorThreadObject;
	KEVENT			MonitorThreadEvent;		// Wake up client thread to check any pending requests.

	LIST_ENTRY	LockList; // List of DRAID_CLIENT_LOCK.Link

	LIST_ENTRY	PendingRequestList; // List of DRIX_MSG_CONTEXT. Contains request that is sent but not replied.
	
	BOOLEAN		HasLocalArbiter;	// Arbiter is in local.
	BOOLEAN		IsReadonly;		// This client is connected in read-only mode
	
	//
	// Following members are only used for local arbiter
	//

	DRIX_LOCAL_CHANNEL NotificationChannel;
	DRIX_LOCAL_CHANNEL RequestReplyChannel;
	PDRIX_LOCAL_CHANNEL RequestChannel;	// reference to local arbiter's client context's member
	PDRIX_LOCAL_CHANNEL NotificationReplyChannel;// reference to local arbiter's client context's member

	//
	//	Following members are only used for remote aribter.
	//
	DRAID_CLIENT_CONNECTION NotificationConnection;
	DRAID_CLIENT_CONNECTION RequestConnection;

	//
	// Client tried to connect arbiter based on this USN number.
	//
	UINT32	LastTriedArbiterUsn;
	LARGE_INTEGER LastTriedArbiterTime;
} DRAID_CLIENT_INFO, *PDRAID_CLIENT_INFO;

NTSTATUS 
DraidClientStart(
	PLURELATION_NODE	Lurn
	);

NTSTATUS
DraidClientStop(
	PLURELATION_NODE	Lurn
	);

NTSTATUS
DraidClientIoWithLock(
	PDRAID_CLIENT_INFO pClientInfo,
	UCHAR LockMode,
	PCCB	Ccb
);

NTSTATUS
DraidReleaseBlockIoPermissionToClient(
	PDRAID_CLIENT_INFO pClientInfo,
	PCCB				Ccb
);

VOID DraidClientUpdateAllNodeFlags(
	PDRAID_CLIENT_INFO	pClientInfo
);

VOID
DraidClientUpdateNodeFlags(
	PDRAID_CLIENT_INFO	pClientInfo,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,	// Temp parameter.. set though lurn node info.
	UCHAR 				DefectCode
);

#endif /* _DRAID_CLIENT_H_ */

