#ifndef LANSCSI_NDASR_H
#define LANSCSI_NDASR_H


//	Internal distributed RAID interface.
//
//
// 	To do: separate DRIX (protocol) part, DRAID implementation part, and export to ndasscsi part.

#include <pshpack1.h>

//  DRAID Information exchange protocol definitions
//
//  To do: move to common header. Bind tool need to use this protocol to support on-line bind information change.


// The following SIGNATURE is in big-endian. Convert to network order when sending

#define NRIX_SIGNATURE 0x44524958
#define NRIX_SIGNATURE_CHAR_ARRAY {'D','R','I','X'}

#define NRIX_CURRENT_VERSION 0x01

#define MAX_NDASR_MEMBER_DISK 16

#define NRIX_ARBITER_PORT_NUM_BASE  0x5000 // Even number is for notification channel, odd number is for request channel.

#define NRIX_IO_LOCK_CLEANUP_TIMEOUT	60	// in seconds

#define NRIX_MAX_REQUEST_SIZE	128

// Client to arbiter's draid listener

#define NRIX_CMD_REGISTER		0x01	// NRIX_REGISTER - None.
//#define NRIX_CMD_UNREGISTER	0x02	// None  - None.

// Client to arbiter request

#define NRIX_CMD_NODE_CHANGE 	0x03	// NRIX_NODE_CHANGE - None
#define NRIX_CMD_ACQUIRE_LOCK	0x04	// NRIX_ACQUIRE_LOCK - NRIX_ACQUIRE_LOCK_REPLY
#define NRIX_CMD_RELEASE_LOCK	0x06	// NRIX_RELEASE_LOCK - None

// Arbiter to client notification

#define NRIX_CMD_RETIRE			   		0x21	// None  - None
#define NRIX_CMD_CHANGE_STATUS			0x22 	// NRIX_CHANGE_STATUS - None
#define NRIX_CMD_STATUS_SYNCED			0x23 	// None - None
//#define NRIX_CMD_GRANT_LOCK			0x24	// NRIX_GRANT_LOCK - None
//#define NRIX_CMD_REQ_TO_YIELD_LOCK	0x25	// NRIX_REQ_TO_YIELD_LOCK - None

// Result code

#define NRIX_RESULT_SUCCESS  			0x00
#define NRIX_RESULT_FAIL				0x01
#define NRIX_RESULT_RAID_SET_NOT_FOUND	0x02
#define NRIX_RESULT_LOWER_USN			0x03
#define NRIX_RESULT_TERMINATING			0x04	// Peer is in terminating state.
//#define NRIX_RESULT_REQUIRE_SYNC		0x05	// RAID status needs to be synced. Hold IO until synced.
#define NRIX_RESULT_NO_CHANGE			0x06	// RAID status didn't change. 
#define NRIX_RESULT_GRANTED				0x07	// Lock is granted
//#define NRIX_RESULT_PENDING			0x08	// Request is pending. Additional notificaion will be sent later.
#define NRIX_RESULT_UNSUPPORTED			0x09	// Unsupported command. This should not happen.
#define NRIX_RESULT_INVALID_LOCK_ID		0x0a
#define NRIX_RESULT_WAIT_SYNC_FOR_WRITE	0x0b	// RAID status needs to be synced. Hold IO until synced.

#if DBG

#define NdasRixGetCmdString(Cmd)  (Cmd == NRIX_CMD_REGISTER)?"REG":\
	(Cmd == NRIX_CMD_NODE_CHANGE)?"NODECHANGE":\
	(Cmd == NRIX_CMD_ACQUIRE_LOCK)?"ACQUIRE":\
	(Cmd == NRIX_CMD_RELEASE_LOCK)?"RELEASE":\
	(Cmd == NRIX_CMD_RETIRE)?"RETIRE":\
	(Cmd == NRIX_CMD_CHANGE_STATUS)?"CHANGESTATUS":\
	(Cmd == NRIX_CMD_STATUS_SYNCED)?"SYNCED":\
	/*(Cmd == NRIX_CMD_GRANT_LOCK)?"GRANT":*/\
	/*(Cmd == NRIX_CMD_REQ_TO_YIELD_LOCK)?"YIELD":*/\
	"Unknown"

#define NdasRixGetResultString(Code) (Code == NRIX_RESULT_SUCCESS)?"SUCCESS":\
	(Code == NRIX_RESULT_FAIL)?"FAIL":\
	(Code == NRIX_RESULT_RAID_SET_NOT_FOUND)?"RAID_SET_NOT_FOUND":\
	(Code == NRIX_RESULT_LOWER_USN)?"LOW_USN":\
	(Code == NRIX_RESULT_TERMINATING)?"TERMINATING":\
	(Code == NRIX_RESULT_WAIT_SYNC_FOR_WRITE)?"NRIX_RESULT_WAIT_SYNC_FOR_WRITE":\
	(Code == NRIX_RESULT_NO_CHANGE)?"NO_CHANGE":\
	(Code == NRIX_RESULT_GRANTED)?"GRANTED":\
	/*(Code == NRIX_RESULT_PENDING)?"PENDING":*/\
	(Code == NRIX_RESULT_UNSUPPORTED)?"NOTSUPPORTED":\
	(Code == NRIX_RESULT_INVALID_LOCK_ID)?"INVALID_LOCK_ID":\
	"UNKNOWN"

#endif

//
// 16 byte header for DRIX packets.
// All data is in network byte order.
// 

typedef struct _NRIX_HEADER {

	union {

		UCHAR SignatureChars[4];
		ULONG Signature;
	};								// byte 4
	
	UCHAR	Command;				// NRIX_CMD_*
	UCHAR	ReplyFlag		: 1;	// 1 if this is reply. Useful for packet level debugging.
	UCHAR	RaidInformation : 1;
	UCHAR	ReservedFlags	: 6;	// byte 6
	USHORT	Length;					// byte 8. Length including this header. 
	USHORT	Sequence;				// byte 10. Sequence number. Incease by one for every command. 
									// Required if multiple request/notification can be queued without being replied.(but not now..)
	UCHAR 	Result;					// byte 11. Set for reply packet. NRIX_RESULT_SUCCESS, NRIX_RESULT_*
	UINT32 	Usn;					// 68
	UCHAR	Reserved[1];

} NRIX_HEADER, *PNRIX_HEADER;

C_ASSERT( 16 == sizeof(NRIX_HEADER) );

#define NRIX_CONN_TYPE_INFO			0	// This client->arbiter connection is for getting RAID information
#define NRIX_CONN_TYPE_NOTIFICATION	1
#define NRIX_CONN_TYPE_REQUEST		2

//
// client -> arbiter. Used to check we are talk about same RAID set.
//	Send after connecting to arbiter via request channel
//	Request connection is established first and after registration is successful, notify connection will be established.
//
typedef struct _NRIX_REGISTER {

	NRIX_HEADER Header;
	UCHAR		ConnType;		// NRIX_CONN_TYPE_INFO, NRIX_CONN_TYPE_*
	UCHAR		LocalClient : 1;
	UCHAR		Reserved1	: 7;
	UCHAR		Reserved2[14];	// 32
	GUID 		RaidSetId;		// 48
	GUID 		ConfigSetId;	// 64
	UINT32 		Usn;			// 68

} NRIX_REGISTER, *PNRIX_REGISTER;

C_ASSERT( 68 == sizeof(NRIX_REGISTER) );

#define NRIX_NODE_FLAG_UNKNOWN		0x01 	// Node is not initialize by master.No other flag is possible with unknown status
#define NRIX_NODE_FLAG_RUNNING 		0x02	// UNKNOWN/RUNNING/STOP/DEFECTIVE is mutual exclusive.
#define NRIX_NODE_FLAG_STOP			0x04
#define NRIX_NODE_FLAG_DEFECTIVE 	0x08	// Disk can be connected but ignore it 
											// because it is defective or not a raid member or replaced by spare.

//#define NRIX_NODE_FLAG_OUT_OF_SYNC 	0x10	// Replaced with Arbiter->OutOfSyncRoleIndex. Only used by client side.

//#define NRIX_NODE_FLAG_NEW_DISK	0x20	// Replacing disk online is not supported.
											// Disk is replaced with new disk. Always comes with NRIX_NODE_FLAG_OUT_OF_SYNC											// If new disk is small disk, set defective flag only.

#define NRIX_NODE_DEFECT_NONE				0x00
#define NRIX_NODE_DEFECT_ETC				0x01	// Defects that is no need to store or propagate
#define NRIX_NODE_DEFECT_BAD_SECTOR			0x02	// Disk has bad block. 
#define NRIX_NODE_DEFECT_BAD_DISK			0x04	// May be bad disk or bad NDAS HW
#define NRIX_NODE_DEFECT_REPLACED_BY_SPARE	0x08	// May be bad disk or bad NDAS HW


__forceinline 
UCHAR 
NdasRaidNodeDefectCodeToRmdUnitStatus (
	UCHAR DefectCode
	) 
{
	UCHAR status = 0;

	if (DefectCode & NRIX_NODE_DEFECT_BAD_SECTOR)  {

		status|= NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR;
	}

	if (DefectCode & NRIX_NODE_DEFECT_BAD_DISK)  {

		status|= NDAS_UNIT_META_BIND_STATUS_BAD_DISK;
	}

	if (DefectCode & NRIX_NODE_DEFECT_REPLACED_BY_SPARE)  {

		status|= NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE;
	}

	return status;
}

__forceinline 
UCHAR 
NdasRaidRmdUnitStatusToDefectCode (
	UCHAR UnitStatus
	) 
{
	UCHAR defectCode = 0;
	
	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)  {
	
		defectCode |= NRIX_NODE_DEFECT_BAD_SECTOR;
	}

	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK)  {

		defectCode |= NRIX_NODE_DEFECT_BAD_DISK;
	}

	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)  {

		defectCode |= NRIX_NODE_DEFECT_REPLACED_BY_SPARE;
	}

	return defectCode;
}
	
// client->arbiter. When request connection is initialized and lurn status has significantly changed.
// This client will wait until it gets NRIX_CMD_STATUS_SYNCED message.
// Currently always send all node.

typedef struct _NRIX_NODE_CHANGE {

	NRIX_HEADER Header;
	UCHAR		UpdateCount;
	UCHAR		Reserved[3];
	
	struct {
	
		UCHAR  	NodeNum;
		UCHAR	NodeFlags;	 // Ored value of NRIX_NODE_FLAGS_*
		UCHAR	DefectCode;  // Valid only with NRIX_NODE_STATUS_DEFECTIVE. NRIX_NODE_DEFECT_*
		UCHAR	Reserved[1];

	} Node[1];

} NRIX_NODE_CHANGE, *PNRIX_NODE_CHANGE;

#define SIZE_OF_NRIX_NODE_CHANGE(_NodeCount) \
	(sizeof(NRIX_NODE_CHANGE) + (_NodeCount-1) * sizeof(((PNRIX_NODE_CHANGE)NULL)->Node[0]))

C_ASSERT( NRIX_MAX_REQUEST_SIZE >= SIZE_OF_NRIX_NODE_CHANGE(MAX_NDASR_MEMBER_DISK) );

#define NRIX_RAID_STATE_INITIALIZING	0x00	
#define NRIX_RAID_STATE_NORMAL  		0x01
#define NRIX_RAID_STATE_DEGRADED		0x02
#define NRIX_RAID_STATE_OUT_OF_SYNC		0x03
#define NRIX_RAID_STATE_FAILED			0x04
#define NRIX_RAID_STATE_TERMINATED		0x05

// arbiter->client. When arbiter received NRIX_CMD_NODE_CHANGE message, arbiter will send this to all node. 
// client will stop IO until NRIX_CMD_APPLY_STATUS_CHANGE comes.

typedef struct _NRIX_CHANGE_STATUS {

	NRIX_HEADER Header;
	UCHAR		ConfigSetId[16];		// If ConfigSetId is changed, RAID member mapping is changed.
	UINT32		Usn;					// If Usn is updated, reload DIB and RMD.
	UCHAR		RaidState;				// NRIX_RAID_STATE_NORMAL, NRIX_RAID_STATE_*
	UCHAR		NodeCount;
	UCHAR		WaitForSync1	 : 1;	// Do not process any IO until RAID sync message is sent. Set when there is multiple client.
	UCHAR		WaitSyncForWrite : 1;
	UCHAR		Reserved1:6;
	UCHAR		OutOfSyncRoleIndex;
	
	struct {
	
		UCHAR NodeFlags; 			// Node flags. NRIX_NODE_FLAG_RUNNING, NRIX_NODE_FLAG_*
		UCHAR NodeRole;				// Node index to RAID role map.
		UCHAR DefectCode;		
		UCHAR Reserved[1];

	} Node[1];	// Variable size array. Indexed as in DIB.

} NRIX_CHANGE_STATUS, *PNRIX_CHANGE_STATUS;

#define SIZE_OF_NRIX_CHANGE_STATUS(_NodeCount) \
	(sizeof(NRIX_CHANGE_STATUS) + (_NodeCount-1) * sizeof(((PNRIX_CHANGE_STATUS)NULL)->Node[0]))

//
// arbiter->client. After arbiter recieved NRIX_CHANGE_STATUS reply from all clients, arbiter will send this to confirm change.
//
typedef NRIX_CHANGE_STATUS NRIX_APPLY_STATUS_CHANGE, *PNRIX_APPLY_STATUS_CHANGE;

//
// DRIX Lock type
//
#define NRIX_LOCK_TYPE_BLOCK_IO 0x0 // Currently only block IO lock type is used.

// Lock mode.

#define NRIX_LOCK_MODE_NL	0x0		// No lock. Internal use
#define NRIX_LOCK_MODE_EX	0x1		// Exclusive read/write.
#define NRIX_LOCK_MODE_PR	0x2		// Protected read. No other host can write.

// Other mode may be possible, but not used not now.

#define NRIX_LOCK_ID_ALL	((UINT64)-1)

#define NDASR_LOCK_GRANULARITY_LOW_THRES (64 * 2) // 64kbytes.

// Client -> Arbiter. Send requeust to acquire lock.

typedef struct _NRIX_ACQUIRE_LOCK {

	NRIX_HEADER Header;
	UCHAR		LockType;		// NRIX_LOCK_TYPE_BLOCK_IO
	UCHAR		LockMode;		// NRIX_LOCK_MODE_*
	UCHAR		Reserved[2];
	UINT64		BlockAddress;	// in sector for block IO lock
	UINT32		BlockLength;	// in sector.

} NRIX_ACQUIRE_LOCK, *PNRIX_ACQUIRE_LOCK;

// Reply to NRIX_CMD_ACQUIRE_LOCK. Result is in header.  

typedef struct _NRIX_ACQUIRE_LOCK_REPLY {

	NRIX_HEADER Header;
	UINT64		LockId;		// Valid for result Granted/waiting
	UCHAR		LockType;		// NRIX_LOCK_TYPE_BLOCK_IO
	UCHAR		LockMode;		// NRIX_LOCK_MODE_*
	UCHAR		Reserved[2];
	UINT64		BlockAddress;	// in sector for block IO lock
	UINT32		BlockLength;	// in sector.

} NRIX_ACQUIRE_LOCK_REPLY, *PNRIX_ACQUIRE_LOCK_REPLY;

// Client -> Arbiter. 
// Release all lock if LockId is NRIX_LOCK_ID_ALL

typedef struct _NRIX_RELEASE_LOCK {

	NRIX_HEADER Header;
	UINT64		LockId;

} NRIX_RELEASE_LOCK, *PNRIX_RELEASE_LOCK;


#include <poppack.h>

#define NDASR_RANGE_NO_OVERLAP 				0
#define NDASR_RANGE_SRC1_HEAD_OVERLAP 		1
#define NDASR_RANGE_SRC1_TAIL_OVERLAP 		2
#define NDASR_RANGE_SRC1_CONTAINS_SRC2		3
#define NDASR_RANGE_SRC2_CONTAINS_SRC1		4 // Return this if exact match


extern struct _DRAID_GLOBALS *DRaidGlobals;

#endif /* LANSCSI_NDASR_H */