#ifndef _NDASR_CLIENT_H
#define _NDASR_CLIENT_H


#define NDASR_CLIENT_CONNECTION_STATE_INIT			0x00
#define NDASR_CLIENT_CONNECTION_STATE_CONNECTING 	0x01
#define NDASR_CLIENT_CONNECTION_STATE_CONNECTED 	0x02
#define NDASR_CLIENT_CONNECTION_STATE_REGISTERED 	0x03
#define NDASR_CLIENT_CONNECTION_STATE_DISCONNECTED 	0x04

typedef struct _NDASR_CLIENT_CONNECTION {

	KSPIN_LOCK			SpinLock;
	
	UINT32				State;		// NDASR_CLIENT_CONNECTION_STATE_*

	LPX_ADDRESS			LocalAddr;
	LPX_ADDRESS			RemoteAddr;

	HANDLE				AddressFileHandle;
	PFILE_OBJECT		AddressFileObject;

	HANDLE				ConnectionFileHandle;
	PFILE_OBJECT		ConnectionFileObject;

	ULONG				Sequence;

	UCHAR				ReceiveBuf[NRMX_MAX_REQUEST_SIZE];

	LPXTDI_OVERLAPPED_CONTEXT	ReceiveOverlapped;

} NDASR_CLIENT_CONNECTION, *PNDASR_CLIENT_CONNECTION;


#define NDASR_CLIENT_STATUS_INITIALIZING				0x00000001
#define NDASR_CLIENT_STATUS_START						0x00000002
#define NDASR_CLIENT_STATUS_TERMINATING					0x00000004
#define NDASR_CLIENT_STATUS_TERMINATED					0x00000008

#define NDASR_CLIENT_STATUS_SHUTDOWN					0x00000100

#define NDASR_CLIENT_STATUS_EASTBLISH_ARBITRATOR_MODE	0x00001000
#define NDASR_CLIENT_STATUS_ARBITRATOR_MODE				0x00002000 
#define NDASR_CLIENT_STATUS_ARBITRATOR_RETIRE_MODE		0x00004000
#define NDASR_CLIENT_STATUS_NON_ARBITRATOR_MODE			0x00008000

#define NDASR_CLIENT_STATUS_ARBITRATOR_CONNECTED		0x00010000 
#define NDASR_CLIENT_STATUS_ARBITRATOR_DISCONNECTED		0x00020000 

#define NDASR_CLIENT_STATUS_CLUSTER_NODE_CHANGED		0x00100000

#define NDASR_CLIENT_STATUS_TEMPORAL_PRIMARY_MODE		0x01000000

#define NDASR_CLIENT_STATUS_ERROR						0x10000000

typedef struct _NDASR_CLIENT {

	LIST_ENTRY			AllListEntry;

	PLURELATION_NODE	Lurn;		

	KSPIN_LOCK			SpinLock;
	
	union {

		struct {

			ULONG		ClientState0		: 8;
			ULONG		Shutdown8			: 4;
			ULONG		ArbitratorMode12	: 4;
			ULONG		ConnectionState16	: 4;
			ULONG		ClusterState20	    : 4;
			ULONG		TemporalPrimay24	: 4;
			ULONG		ErrorStatus28		: 4;
		};	

		ULONG			Status;			
	};

	BOOLEAN				EmergencyMode;
	BOOLEAN				MonitorRevivedChild;
	HANDLE				MonitorThreadHandle;
	PLURELATION_NODE	MonitorChild;
	KEVENT				MonitorThreadReadyEvent;

	LARGE_INTEGER		TemporalPrimarySince;

	NTSTATUS			ThreadErrorStatus;

	BOOLEAN				RequestToTerminate;

#define NDASR_CLIENT_SHUTDOWN_WAIT_INTERVAL	(10*NANO100_PER_SEC)

	PCCB				ShutdownCcb;

	LIST_ENTRY			CcbQueue;
		
	LARGE_INTEGER		NextMonitoringTick;

	UINT16				RequestSequence;
	LIST_ENTRY			LockList;	

	UCHAR				NdasrState;								
	UCHAR				OutOfSyncRoleIndex;						

	UINT32				Usn;
	BOOLEAN				WaitForSync;

	UCHAR				NdasrNodeFlags[NRMX_MAX_MEMBER_DISK];	
	UCHAR				NdasrDefectCodes[NRMX_MAX_MEMBER_DISK]; 

	UCHAR				RoleToNodeMap[NRMX_MAX_MEMBER_DISK];	
	UCHAR				NodeToRoleMap[NRMX_MAX_MEMBER_DISK];

	NDASR_CLIENT_CONNECTION NotificationConnection;
	NDASR_CLIENT_CONNECTION RequestConnection;

	HANDLE				ThreadHandle;
	PVOID				ThreadObject;
	KEVENT				ThreadEvent;	
	KEVENT				ThreadReadyEvent;

	NTSTATUS			(*Flush)( struct _NDASR_CLIENT	*NdasClient );

	CCB						LockCcb[NRMX_MAX_MEMBER_DISK];
	LURN_DEVLOCK_CONTROL	LurnDeviceLock[NRMX_MAX_MEMBER_DISK];
	KEVENT					LockCompletionEvent[NRMX_MAX_MEMBER_DISK];

	PCHAR				BlockBuffer[NRMX_MAX_MEMBER_DISK];
	UCHAR				Buffer[0];

} NDASR_CLIENT, *PNDASR_CLIENT;


#define LOCK_STATUS_NONE 		(1<<0) // Lock is allocated. But not used yet
#define LOCK_STATUS_PENDING		(1<<3) // Sent message but lock is not available right now. waiting for grant 
#define LOCK_STATUS_GRANTED		(1<<4) // Lock is granted. 
#define LOCK_STATUS_HOLDING		(1<<5) // Lock is allocated. But not wait for release

// Guarded by Client's spinlock
// Lock info

typedef struct _NDASR_CLIENT_LOCK {

	LIST_ENTRY		Link;				// Link to ClientInfo.LockList. List of all locks including lock to acquire

	UINT32			Status;				// LOCK_STATUS_NONE, LOCK_STATUS_*

	UCHAR 			Type;
	UCHAR 			Mode;

	UINT64			BlockAddress;		// in sector for blo
	UINT32			BlockLength;

	UINT64			Id;					// ID reccieved from arbiter.
	
	LARGE_INTEGER	LastAccesseTick;
	LONG			InUseCount;			// Number of usage used by IO routine. Guarded by client's spinlock

} NDASR_CLIENT_LOCK, *PNDASR_CLIENT_LOCK;


#endif /* _NDASR_CLIENT_H_ */

