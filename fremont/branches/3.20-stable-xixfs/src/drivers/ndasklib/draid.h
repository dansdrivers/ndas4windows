#ifndef LANSCSI_DRAID_H
#define LANSCSI_DRAID_H

#include <ndas/ndasdib.h>
#include "lpxtdi.h"

//
//	Internal distributed RAID interface.
//
//
// 	To do: separate DRIX (protocol) part, DRAID implementation part, and export to ndasscsi part.

#include <pshpack1.h>
//
//  DRAID Information exchange protocol definitions
//
//  To do: move to common header. Bind tool need to use this protocol to support on-line bind information change.
//

//
// The following SIGNATURE is in big-endian. Convert to network order when sending
//
#define DRIX_SIGNATURE 0x44524958
#define DRIX_SIGNATURE_CHAR_ARRAY {'D','R','I','X'}

#define DRIX_CURRENT_VERSION 0x01

#define MAX_DRAID_MEMBER_DISK 16

#define DRIX_ARBITER_PORT_NUM_BASE  0x5000 // Even number is for notification channel, odd number is for request channel.


#define DRIX_IO_LOCK_CLEANUP_TIMEOUT	60	// in seconds

#define DRIX_MAX_REQUEST_SIZE	128

//
// Client to arbiter's draid listener
//
										// Request packet format - reply packet format
#define DRIX_CMD_REGISTER		0x01	// DRIX_REGISTER - None.
//#define DRIX_CMD_UNREGISTER	0x02	// None  - None.

//
// Client to arbiter request
//
#define DRIX_CMD_NODE_CHANGE 	0x03	// DRIX_NODE_CHANGE - None
#define DRIX_CMD_ACQUIRE_LOCK	0x04	// DRIX_ACQUIRE_LOCK - DRIX_ACQUIRE_LOCK_REPLY
#define DRIX_CMD_RELEASE_LOCK 0x06		// DRIX_RELEASE_LOCK - None

//
// Arbiter to client notification
//
#define DRIX_CMD_RETIRE		   	0x21	// None  - None
#define DRIX_CMD_CHANGE_STATUS	0x22 	// DRIX_CHANGE_STATUS - None
#define DRIX_CMD_STATUS_SYNCED	0x23 	// None - None
#define DRIX_CMD_GRANT_LOCK		0x24	// DRIX_GRANT_LOCK - None
#define DRIX_CMD_REQ_TO_YIELD_LOCK	0x25	// DRIX_REQ_TO_YIELD_LOCK - None

//
// Result code
//
#define DRIX_RESULT_SUCCESS  			0x00
#define DRIX_RESULT_FAIL				0x01
#define DRIX_RESULT_RAID_SET_NOT_FOUND	0x02
#define DRIX_RESULT_LOW_USN			0x03
#define DRIX_RESULT_TERMINATING		0x04	// Peer is in terminating state.
#define DRIX_RESULT_REQUIRE_SYNC		0x05	// RAID status needs to be synced. Hold IO until synced.
#define DRIX_RESULT_NO_CHANGE			0x06	// RAID status didn't change. 
#define DRIX_RESULT_GRANTED			0x07	// Lock is granted
#define DRIX_RESULT_PENDING			0x08	// Request is pending. Additional notificaion will be sent later.
#define DRIX_RESULT_UNSUPPORTED		0x09	// Unsupported command. This should not happen.
#define DRIX_RESULT_INVALID_LOCK_ID	0x0a

#if DBG
#define DrixGetCmdString(Cmd)  (Cmd == DRIX_CMD_REGISTER)?"REG":\
	(Cmd == DRIX_CMD_NODE_CHANGE)?"NODECHANGE":\
	(Cmd == DRIX_CMD_ACQUIRE_LOCK)?"ACQUIRE":\
	(Cmd == DRIX_CMD_RELEASE_LOCK)?"RELEASE":\
	(Cmd == DRIX_CMD_RETIRE)?"RETIRE":\
	(Cmd == DRIX_CMD_CHANGE_STATUS)?"CHANGESTATUS":\
	(Cmd == DRIX_CMD_STATUS_SYNCED)?"SYNCED":\
	(Cmd == DRIX_CMD_GRANT_LOCK)?"GRANT":\
	(Cmd == DRIX_CMD_REQ_TO_YIELD_LOCK)?"YIELD":\
	"Unknown"

#define DrixGetResultString(Code) (Code == DRIX_RESULT_SUCCESS)?"SUCCESS":\
	(Code == DRIX_RESULT_FAIL)?"FAIL":\
	(Code == DRIX_RESULT_RAID_SET_NOT_FOUND)?"RAID_SET_NOT_FOUND":\
	(Code == DRIX_RESULT_LOW_USN)?"LOW_USN":\
	(Code == DRIX_RESULT_TERMINATING)?"TERMINATING":\
	(Code == DRIX_RESULT_REQUIRE_SYNC)?"SYNC_REQUIRE":\
	(Code == DRIX_RESULT_NO_CHANGE)?"NO_CHANGE":\
	(Code == DRIX_RESULT_GRANTED)?"GRANTED":\
	(Code == DRIX_RESULT_PENDING)?"PENDING":\
	(Code == DRIX_RESULT_UNSUPPORTED)?"NOTSUPPORTED":\
	(Code == DRIX_RESULT_INVALID_LOCK_ID)?"INVALID_LOCK_ID":\
	"UNKNOWN"

#endif

//
// 16 byte header for DRIX packets.
// All data is in network byte order.
// 
typedef struct _DRIX_HEADER {

	union {
		UCHAR SignatureChars[4];
		ULONG Signature;
	};			// byte 4
	UCHAR Command; // DRIX_CMD_*
	UCHAR ReplyFlag : 1;	// 1 if this is reply. Useful for packet level debugging.
	UCHAR ReservedFlags:7;  	// byte 6
	USHORT Length; // byte 8. Length including this header. 
	USHORT	Sequence;	// byte 10. Sequence number. Incease by one for every command. 
						// Required if multiple request/notification can be queued without being replied.(but not now..)
	UCHAR 	Result;	// byte 11. Set for reply packet. DRIX_RESULT_SUCCESS, DRIX_RESULT_*
	UCHAR 	Reserved[5]; //16.
} DRIX_HEADER, *PDRIX_HEADER;

C_ASSERT(16 == sizeof(DRIX_HEADER));


#define DRIX_CONN_TYPE_INFO			0	// This client->arbiter connection is for getting RAID information
#define DRIX_CONN_TYPE_NOTIFICATION	1
#define DRIX_CONN_TYPE_REQUEST		2

//
// client -> arbiter. Used to check we are talk about same RAID set.
//	Send after connecting to arbiter via request channel
//	Request connection is established first and after registration is successful, notify connection will be established.
//
typedef struct _DRIX_REGISTER {
	DRIX_HEADER Header;
	UCHAR	ConnType;	// DRIX_CONN_TYPE_INFO, DRIX_CONN_TYPE_*
	UCHAR	Reserved[15];	//32
	GUID 	RaidSetId;		// 48
	GUID 	ConfigSetId;		// 64
	UINT32 	Usn;			// 68
} DRIX_REGISTER, *PDRIX_REGISTER;

C_ASSERT(68 == sizeof(DRIX_REGISTER));

#define DRIX_NODE_FLAG_UNKNOWN		0x01 	// Node is not initialize by master.No other flag is possible with unknown status
#define DRIX_NODE_FLAG_RUNNING 		0x02	// UNKNOWN/RUNNING/STOP/DEFECTIVE is mutual exclusive.
#define DRIX_NODE_FLAG_STOP			0x04
#define DRIX_NODE_FLAG_DEFECTIVE 		0x08	// Disk can be connected but ignore it 
												//because it is defective or not a raid member or replaced by spare.
#define DRIX_NODE_FLAG_OUT_OF_SYNC 	0x10	// Replaced with Arbiter->OutOfSyncRole. Only used by client side.
#if 0	// Replacing disk online is not supported.
#define DRIX_NODE_FLAG_NEW_DISK		0x20	// Disk is replaced with new disk. Always comes with DRIX_NODE_FLAG_OUT_OF_SYNC
												// If new disk is small disk, set defective flag only.
#endif

#define DRIX_NODE_DEFECT_NONE			0x00
#define DRIX_NODE_DEFECT_ETC			0x01	// Defects that is no need to store or propagate
#define DRIX_NODE_DEFECT_BAD_SECTOR	0x02	// Disk has bad block. 
#define DRIX_NODE_DEFECT_BAD_DISK	0x04	// May be bad disk or bad NDAS HW
#define DRIX_NODE_DEFECT_REPLACED_BY_SPARE	0x08	// May be bad disk or bad NDAS HW

__forceinline 
UCHAR DraidNodeDefectCodeToRmdUnitStatus(UCHAR DefectCode) 
{
	UCHAR status = 0;
	if (DefectCode & DRIX_NODE_DEFECT_BAD_SECTOR)  {
		status|= NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR;
	}
	if (DefectCode & DRIX_NODE_DEFECT_BAD_DISK)  {
		status|= NDAS_UNIT_META_BIND_STATUS_BAD_DISK;
	}
	if (DefectCode & DRIX_NODE_DEFECT_REPLACED_BY_SPARE)  {
		status|= NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE;
	}
	return status;
}

__forceinline 
UCHAR DraidRmdUnitStatusToDefectCode(UCHAR UnitStatus) 
{
	UCHAR defectCode = 0;
	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)  {
		defectCode |= DRIX_NODE_DEFECT_BAD_SECTOR;
	}
	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK)  {
		defectCode |= DRIX_NODE_DEFECT_BAD_DISK;
	}
	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)  {
		defectCode |= DRIX_NODE_DEFECT_REPLACED_BY_SPARE;
	}
	return defectCode;
}

	
//
// client->arbiter. When request connection is initialized and lurn status has significantly changed.
// This client will wait until it gets DRIX_CMD_STATUS_SYNCED message.
// Currently always send all node.
//
typedef struct _DRIX_NODE_CHANGE {
	DRIX_HEADER Header;
	UCHAR	UpdateCount;
	UCHAR	Reserved[3];
	struct {
		UCHAR  	NodeNum;
		UCHAR	NodeFlags;	// Ored value of DRIX_NODE_FLAGS_*
		UCHAR	DefectCode;  // Valid only with DRIX_NODE_STATUS_DEFECTIVE. DRIX_NODE_DEFECT_*
		UCHAR	Reserved[1];
	} Node[1];
} DRIX_NODE_CHANGE, *PDRIX_NODE_CHANGE;
#define SIZE_OF_DRIX_NODE_CHANGE(_NodeCount) \
	(sizeof(DRIX_NODE_CHANGE) + (_NodeCount-1) * sizeof(((PDRIX_NODE_CHANGE)NULL)->Node[0]))

#define DRIX_RAID_STATUS_INITIALIZING	0x00	
#define DRIX_RAID_STATUS_NORMAL  		0x01
#define DRIX_RAID_STATUS_DEGRADED  	0x02
#define DRIX_RAID_STATUS_REBUILDING	0x03
#define DRIX_RAID_STATUS_FAILED		0x04
#define DRIX_RAID_STATUS_TERMINATED	0x05

//
// arbiter->client. When arbiter received DRIX_CMD_NODE_CHANGE message, arbiter will send this to all node. 
//	client will stop IO until DRIX_CMD_APPLY_STATUS_CHANGE comes.
//
typedef struct _DRIX_CHANGE_STATUS {
	DRIX_HEADER Header;
	UCHAR	ConfigSetId[16];	// If ConfigSetId is changed, RAID member mapping is changed.
	UINT32	Usn;		// If Usn is updated, reload DIB and RMD.
	UCHAR	RaidStatus;	// DRIX_RAID_STATUS_NORMAL, DRIX_RAID_STATUS_*
	UCHAR	NodeCount;
	UCHAR	WaitForSync:1;	// Do not process any IO until RAID sync message is sent. Set when there is multiple client.
	UCHAR	Reserved1:7;
	UCHAR	Reserved2;
	struct {
		UCHAR NodeFlags; 		// Node flags. DRIX_NODE_FLAG_RUNNING, DRIX_NODE_FLAG_*
		UCHAR NodeRole;			// Node index to RAID role map.
		UCHAR DefectCode;		
		UCHAR Reserved[1];
	} Node[1];	// Variable size array. Indexed as in DIB.
} DRIX_CHANGE_STATUS, *PDRIX_CHANGE_STATUS;

#define SIZE_OF_DRIX_CHANGE_STATUS(_NodeCount) \
	(sizeof(DRIX_CHANGE_STATUS) + (_NodeCount-1) * sizeof(((PDRIX_CHANGE_STATUS)NULL)->Node[0]))


//
// arbiter->client. After arbiter recieved DRIX_CHANGE_STATUS reply from all clients, arbiter will send this to confirm change.
//
typedef DRIX_CHANGE_STATUS DRIX_APPLY_STATUS_CHANGE, *PDRIX_APPLY_STATUS_CHANGE;

//
// DRIX Lock type
//
#define DRIX_LOCK_TYPE_BLOCK_IO 0x0 // Currently only block IO lock type is used.

//
// Lock mode.
//
#define DRIX_LOCK_MODE_NL	0x0 // No lock. Internal use
#define DRIX_LOCK_MODE_EX	0x1 // Exclusive read/write.
#define DRIX_LOCK_MODE_PR	0x2 // Protected read. No other host can write.
// Other mode may be possible, but not used not now.

#define DRIX_LOCK_ID_ALL ((UINT64)-1)

#define DRAID_LOCK_GRANULARITY_LOW_THRES (64 * 2) // 64kbytes.

//
// Client -> Arbiter. Send requeust to acquire lock.
//
typedef struct _DRIX_ACQUIRE_LOCK {
	DRIX_HEADER Header;
	UCHAR LockType;	// DRIX_LOCK_TYPE_BLOCK_IO
	UCHAR LockMode;	// DRIX_LOCK_MODE_*
	UCHAR Reserved[2];
	UINT64 Addr;		// in sector for block IO lock
	UINT32 Length;	// in sector.
} DRIX_ACQUIRE_LOCK, *PDRIX_ACQUIRE_LOCK;

//
// Reply to DRIX_CMD_ACQUIRE_LOCK. Result is in header.  
//
typedef struct _DRIX_ACQUIRE_LOCK_REPLY {
	DRIX_HEADER Header;
	UINT64	LockId;		// Valid for result Granted/waiting
	UCHAR LockType;	// DRIX_LOCK_TYPE_BLOCK_IO
	UCHAR LockMode;	// DRIX_LOCK_MODE_*
	UCHAR Reserved[6];
	UINT64 Addr;		// in sector for block IO lock. Larger region will be locked to reduce lock operation. And this policy is determined by arbiter
	UINT32 Length;	// in sector.	
} DRIX_ACQUIRE_LOCK_REPLY, *PDRIX_ACQUIRE_LOCK_REPLY;

//
// Client -> Arbiter. 
//
// Release all lock if LockId is DRIX_LOCK_ID_ALL
typedef struct _DRIX_RELEASE_LOCK {
	DRIX_HEADER Header;
	UINT64	LockId;
} DRIX_RELEASE_LOCK, *PDRIX_RELEASE_LOCK;

//
// Arbiter -> Client. Send when lock requested by client is pended and the lock is granted later.
//	Same as DRIX_ACQUIRE_LOCK_REPLY
//
typedef struct _DRIX_GRANT_LOCK {
	DRIX_HEADER Header;
	UINT64	LockId;
	UCHAR LockType;	// DRIX_LOCK_TYPE_BLOCK_IO
	UCHAR LockMode;	// DRIX_LOCK_MODE_*
	UCHAR Reserved[6];	
	UINT64 Addr;		// in sector for block IO lock. Larger region will be locked to reduce lock operation. And this policy is determined by arbiter
	UINT32 Length;	// in sector.
} DRIX_GRANT_LOCK, *PDRIX_GRANT_LOCK;

//
// Arbiter -> Client. Send when client need to give up lock.
//
typedef struct _DRIX_REQ_TO_YIELD_LOCK {
	DRIX_HEADER Header;
	UINT64	LockId;
	UINT32	Reason;	
} DRIX_REQ_TO_YIELD_LOCK, *PDRIX_REQ_TO_YIELD_LOCK;

#include <poppack.h>

typedef VOID (*DRAID_MSG_CALLBACK)(PDRAID_CLIENT_INFO, PDRIX_HEADER, PVOID);

//
// Used to store sent or queued DRIX Message data in linked list and message context
// 
typedef struct _DRIX_MSG_CONTEXT {
	LIST_ENTRY		Link;
	PDRIX_HEADER	Message;
	BOOLEAN			HaveTimeout;
	LARGE_INTEGER	ReqTime;	// Time that this msg context is created.
	LARGE_INTEGER	Timeout;	// Timeout value. Exact timeout value will be recalculated with ReqTime & Timeout
	DRAID_MSG_CALLBACK	CallbackFunc; // Called when message is timed out or replied or client is terminating.
	PVOID			CallbackParam1;
} DRIX_MSG_CONTEXT, *PDRIX_MSG_CONTEXT;

//
//	Collection of references for communicating between local arbiter and client
//
typedef struct _DRIX_LOCAL_CHANNEL {
	KSPIN_LOCK	Lock;
	KEVENT		Event;
	LIST_ENTRY	Queue;	// List of DRIX_MSG_CONTEXT
} DRIX_LOCAL_CHANNEL, *PDRIX_LOCAL_CHANNEL;

#define DRIX_INITIALIZE_LOCAL_CHANNEL(channel) { \
	KeInitializeSpinLock(&(channel)->Lock);	\
	KeInitializeEvent(&(channel)->Event, NotificationEvent, FALSE);	\
	InitializeListHead(&(channel)->Queue);	\
	}

#include "draidclient.h"
#include "draidarbiter.h"

//
// draid.c
//
NTSTATUS 
DraidRegisterArbiter(
	PDRAID_ARBITER_INFO Arbiter
);

NTSTATUS
DraidUnregisterArbiter(
	PDRAID_ARBITER_INFO Arbiter
);

NTSTATUS
DraidRegisterClient(
	PDRAID_CLIENT_INFO Client
);

NTSTATUS
DraidUnregisterClient(
	PDRAID_CLIENT_INFO Client
);

#define DRAID_RANGE_NO_OVERLAP 				0
#define DRAID_RANGE_SRC1_HEAD_OVERLAP 		1
#define DRAID_RANGE_SRC1_TAIL_OVERLAP 		2
#define DRAID_RANGE_SRC1_CONTAINS_SRC2		3
#define DRAID_RANGE_SRC2_CONTAINS_SRC1		4 // Return this if exact match

// Returns DRAID_RANGE_*
UINT32 DraidGetOverlappedRange(
	UINT64 Start1, UINT64 Length1,
	UINT64 Start2, UINT64 Length2,
	UINT64* OverlapStart, UINT64* OverlapEnd
);

// Returns new event count
INT32 
DraidReallocEventArray(
	PKEVENT** Events,
	PKWAIT_BLOCK* WaitBlocks,
	INT32 CurrentCount
);

VOID
DraidFreeEventArray(
	PKEVENT* Events,
	PKWAIT_BLOCK WaitBlocks
);

#endif /* LANSCSI_DRAID_H */

