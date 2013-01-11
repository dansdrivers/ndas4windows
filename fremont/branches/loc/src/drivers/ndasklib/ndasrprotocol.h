#ifndef NDAS_RAID_PROTOCOL_H
#define NDAS_RAID_PROTOCOL_H


//  Ndas Raid Message eXchange(NRMX) protocol definitions

#include <pshpack1.h>


// The following SIGNATURE is in big-endian. Convert to network order when sending

#define NRMX_SIGNATURE 0x4E524D58
#define NRMX_SIGNATURE_CHAR_ARRAY {'N','R','M','X'}

#define NRMX_CURRENT_VERSION 0x02

#define	LPXRP_NRMX_ARBITRRATOR_PORT	((UINT16)0x0010)	// stream

#define MAX_NDASR_MEMBER_DISK			16

#define NRMX_IO_LOCK_CLEANUP_TIMEOUT	60					// in seconds

#define NRMX_MAX_REQUEST_SIZE	128

// Client to arbiter's ndas raid listener

#define NRMX_CMD_REGISTER		0x01	// NRMX_CMD_REGISTER - NRMX_HEADER.

//#define NRMX_CMD_UNREGISTER	0x02	// NRMX_HEADER - NRMX_HEADER.

// Client to arbiter request

#define NRMX_CMD_NODE_CHANGE 	0x03	// NRMX_NODE_CHANGE  - NRMX_HEADER
#define NRMX_CMD_ACQUIRE_LOCK	0x04	// NRMX_ACQUIRE_LOCK - NRMX_ACQUIRE_LOCK_REPLY
#define NRMX_CMD_RELEASE_LOCK	0x05	// NRMX_RELEASE_LOCK - NRMX_HEADER

// Arbitrator to client notification

#define NRMX_CMD_RETIRE			   		0x21	// NRMX_CMD_RETIRE		- None
#define NRMX_CMD_CHANGE_STATUS			0x22 	// NRMX_CHANGE_STATUS	- None

#define NRMX_CMD_STATUS_SYNCED			0x23 	// NRMX_HEADER			- None not currently used

//#define NRMX_CMD_GRANT_LOCK			0x24	// NRMX_GRANT_LOCK - None
//#define NRMX_CMD_REQ_TO_YIELD_LOCK	0x25	// NRMX_REQ_TO_YIELD_LOCK - None

// Result code

#define NRMX_RESULT_SUCCESS  			0x00
#define NRMX_RESULT_UNSUCCESSFUL		0x01
#define NRMX_RESULT_UNSUPPORTED			0x02	// Unsupported command. This should not happen.
#define NRMX_RESULT_RAID_SET_NOT_FOUND	0x03
#define NRMX_RESULT_LOWER_USN			0x04
//#define NRMX_RESULT_TERMINATING		0x05	// Peer is in terminating state.
#define NRMX_RESULT_REQUIRE_SYNC		0x06	// RAID status needs to be synced. Hold IO until synced.
#define NRMX_RESULT_NO_CHANGE			0x07	// RAID status didn't change. 
#define NRMX_RESULT_GRANTED				0x08	// Lock is granted
//#define NRMX_RESULT_PENDING			0x09	// Request is pending. Additional notificaion will be sent later.
#define NRMX_RESULT_INVALID_LOCK_ID		0x0a

//
// 16 byte header for DRIX packets.
// All data is in network byte order.
// 

typedef struct _NRMX_HEADER {

	union {

		UINT8 SignatureChars[4];
		UINT32 Signature;
	};								// byte 4
	
	UINT8	Command;				// NRMX_CMD_*

	UINT8	ReplyFlag		: 1;	// 1 if this is reply. Useful for packet level debugging.
	UINT8	RaidInformation : 1;
	UINT8	ReservedFlags	: 6;	// byte 6
	
	UINT16	Length;					// byte 8.   Length including this header. 
	UINT16	Sequence;				// byte 10.  Sequence number. Incease by one for every command. 
									// Required  if multiple request/notification can be queued without being replied.(but not now..)
	UINT8 	Result;					// byte 11.  Set for reply packet. NRMX_RESULT_SUCCESS, NRMX_RESULT_*
	
	UINT32 	Usn;					// Rmd Usn when send request
	
	UINT8	Reserved[1];

} NRMX_HEADER, *PNRMX_HEADER;

C_ASSERT( 16 == sizeof(NRMX_HEADER) );
C_ASSERT( sizeof(NRMX_HEADER) % 4 == 0 );

//#define NRMX_CONN_TYPE_INFO			0	// This client->arbiter connection is for getting RAID information
#define NRMX_CONN_TYPE_NOTIFICATION	1
#define NRMX_CONN_TYPE_REQUEST		2

//  client -> arbiter. Used to check we are talk about same RAID set.
//	Send after connecting to arbiter via request channel
//	Request connection is established first and after registration is successful, notify connection will be established.

typedef struct _NRMX_REGISTER {

	NRMX_HEADER Header;
	UINT8		ConnType;		// NRMX_CONN_TYPE_*

	UINT8		LocalClient : 1;
	UINT8		Reserved1	: 7;

	UINT8		Reserved2[14];	// 32

	GUID 		RaidSetId;		// 48
	GUID 		ConfigSetId;	// 64
	
	UINT32 		Usn;			// 68

} NRMX_REGISTER, *PNRMX_REGISTER;

C_ASSERT( 68 == sizeof(NRMX_REGISTER) );
C_ASSERT( sizeof(NRMX_REGISTER) % 4 == 0 );

#define NRMX_NODE_FLAG_UNKNOWN		0x01 	// Node is not initialize by master.No other flag is possible with unknown status
#define NRMX_NODE_FLAG_RUNNING 		0x02	// UNKNOWN/RUNNING/STOP/DEFECTIVE is mutual exclusive.
#define NRMX_NODE_FLAG_STOP			0x04
#define NRMX_NODE_FLAG_DEFECTIVE 	0x08	// Disk can be connected but ignore it 
											// because it is defective or not a raid member or replaced by spare.

#define NRMX_NODE_FLAG_OFFLINE		0x10

//#define NRMX_NODE_FLAG_OUT_OF_SYNC 0x10	// Replaced with Arbitrator->OutOfSyncRoleIndex. Only used by client side.
//#define NRMX_NODE_FLAG_NEW_DISK	 0x20	// Replacing disk online is not supported.
											// Disk is replaced with new disk. Always comes with NRMX_NODE_FLAG_OUT_OF_SYNC											// If new disk is small disk, set defective flag only.

#define NRMX_NODE_DEFECT_NONE				0x00
#define NRMX_NODE_DEFECT_ETC				0x01	// Defects that is no need to store or propagate
#define NRMX_NODE_DEFECT_BAD_SECTOR			0x02	// Disk has bad block. 
#define NRMX_NODE_DEFECT_BAD_DISK			0x04	// May be bad disk or bad NDAS HW
#define NRMX_NODE_DEFECT_REPLACED_BY_SPARE	0x08	// May be bad disk or bad NDAS HW

	
// client->arbiter. When request connection is initialized and lurn status has significantly changed.
// This client will wait until it gets NRMX_CMD_STATUS_SYNCED message.
// Currently always send all node.

typedef struct _NRMX_NODE_CHANGE {

	NRMX_HEADER Header;

	UINT8		Reserved[3];

	UINT8		NodeCount;

	struct {
	
		UINT8  	NodeNum;
		UINT8	NodeFlags;	 // Ored value of NRMX_NODE_FLAGS_*
		UINT8	DefectCode;  // Valid only with NRMX_NODE_STATUS_DEFECTIVE. NRMX_NODE_DEFECT_*
		UINT8	Reserved[1];

	} Node[1];

} NRMX_NODE_CHANGE, *PNRMX_NODE_CHANGE;

#define SIZE_OF_NRMX_NODE_CHANGE(_UpdateNodeCount) \
	(sizeof(NRMX_NODE_CHANGE) + (_UpdateNodeCount-1) * sizeof(((PNRMX_NODE_CHANGE)NULL)->Node[0]))

C_ASSERT( NRMX_MAX_REQUEST_SIZE >= SIZE_OF_NRMX_NODE_CHANGE(MAX_NDASR_MEMBER_DISK) );
C_ASSERT( sizeof(NRMX_NODE_CHANGE) % 4 == 0 );

#define NRMX_RAID_STATE_INITIALIZING	0x00	
#define NRMX_RAID_STATE_NORMAL  		0x01
#define NRMX_RAID_STATE_DEGRADED		0x02
#define NRMX_RAID_STATE_OUT_OF_SYNC		0x03
#define NRMX_RAID_STATE_FAILED			0x04
#define NRMX_RAID_STATE_TERMINATED		0x05

// arbiter->client. When arbiter received NRMX_CMD_NODE_CHANGE message, arbiter will send this to all node. 
// client will stop IO until NRMX_CMD_APPLY_STATUS_CHANGE comes.

typedef struct _NRMX_CHANGE_STATUS {

	NRMX_HEADER Header;

	UINT8		ConfigSetId[16];		// If ConfigSetId is changed, RAID member mapping is changed.
	UINT32		Usn;					// If Usn is updated, reload DIB and RMD.
	
	UINT8		RaidState;				// NRMX_RAID_STATE_NORMAL, NRMX_RAID_STATE_*
		
	UINT8		WaitForSync : 1;		// Do not process any IO until RAID sync message is sent. Set when there is multiple client.
	UINT8		Reserved1   : 7;

	UINT8		OutOfSyncRoleIndex;

	UINT8		NodeCount;

	struct {
	
		UINT8 NodeFlags; 			// Node flags. NRMX_NODE_FLAG_RUNNING, NRMX_NODE_FLAG_*
		UINT8 NodeRole;				// Node index to RAID role map.
		UINT8 DefectCode;		
		UINT8 Reserved[1];

	} Node[1];	// Variable size array. Indexed as in DIB.

} NRMX_CHANGE_STATUS, *PNRMX_CHANGE_STATUS;

C_ASSERT( sizeof(NRMX_CHANGE_STATUS) % 4 == 0 );

#define SIZE_OF_NRMX_CHANGE_STATUS(_NodeCount) \
	(sizeof(NRMX_CHANGE_STATUS) + (_NodeCount-1) * sizeof(((PNRMX_CHANGE_STATUS)NULL)->Node[0]))

//
// arbiter->client. After arbiter recieved NRMX_CHANGE_STATUS reply from all clients, arbiter will send this to confirm change.
//
typedef NRMX_CHANGE_STATUS NRMX_APPLY_STATUS_CHANGE, *PNRMX_APPLY_STATUS_CHANGE;

//
// DRIX Lock type
//
#define NRMX_LOCK_TYPE_BLOCK_IO 0x0 // Currently only block IO lock type is used.

// Lock mode.

//#define NRMX_LOCK_MODE_NL	0x0		// No lock. Internal use
//#define NRMX_LOCK_MODE_EX	0x1		// Exclusive read/write.
//#define NRMX_LOCK_MODE_PR	0x2		// Protected read. No other host can write.
#define NRMX_LOCK_MODE_SH	0x3		// Shared read/write.

// Other mode may be possible, but not used not now.

#define NRMX_LOCK_ID_ALL	((UINT64)-1)

// Client -> Arbitrator. Send requeust to acquire lock.

typedef struct _NRMX_ACQUIRE_LOCK {

	NRMX_HEADER Header;

	UINT8		LockType;		// NRMX_LOCK_TYPE_BLOCK_IO
	UINT8		LockMode;		// NRMX_LOCK_MODE_*

	UINT8		Reserved[2];

	UINT64		BlockAddress;	// in sector for block IO lock
	UINT32		BlockLength;	// in sector.

} NRMX_ACQUIRE_LOCK, *PNRMX_ACQUIRE_LOCK;

C_ASSERT( sizeof(NRMX_ACQUIRE_LOCK) % 4 == 0 );

// Reply to NRMX_CMD_ACQUIRE_LOCK. Result is in header.  

typedef struct _NRMX_ACQUIRE_LOCK_REPLY {

	NRMX_HEADER Header;

	UINT64		LockId;		// Valid for result Granted/waiting
	UINT8		LockType;		// NRMX_LOCK_TYPE_BLOCK_IO
	UINT8		LockMode;		// NRMX_LOCK_MODE_*

	UINT8		Reserved[2];

	UINT64		BlockAddress;	// in sector for block IO lock
	UINT32		BlockLength;	// in sector.

} NRMX_ACQUIRE_LOCK_REPLY, *PNRMX_ACQUIRE_LOCK_REPLY;

C_ASSERT( sizeof(NRMX_ACQUIRE_LOCK_REPLY) % 4 == 0 );

// Client -> Arbitrator. 
// Release all lock if LockId is NRMX_LOCK_ID_ALL

typedef struct _NRMX_RELEASE_LOCK {

	NRMX_HEADER Header;
	UINT64		LockId;

} NRMX_RELEASE_LOCK, *PNRMX_RELEASE_LOCK;

C_ASSERT( sizeof(NRMX_RELEASE_LOCK) % 4 == 0 );

#include <poppack.h>


#if DBG

#define NdasRixGetCmdString(Cmd)  (Cmd == NRMX_CMD_REGISTER)?"REG":\
	(Cmd == NRMX_CMD_NODE_CHANGE)?"NODECHANGE":\
	(Cmd == NRMX_CMD_ACQUIRE_LOCK)?"ACQUIRE":\
	(Cmd == NRMX_CMD_RELEASE_LOCK)?"RELEASE":\
	(Cmd == NRMX_CMD_RETIRE)?"RETIRE":\
	(Cmd == NRMX_CMD_CHANGE_STATUS)?"CHANGESTATUS":\
	(Cmd == NRMX_CMD_STATUS_SYNCED)?"SYNCED":\
	/*(Cmd == NRMX_CMD_GRANT_LOCK)?"GRANT":*/\
	/*(Cmd == NRMX_CMD_REQ_TO_YIELD_LOCK)?"YIELD":*/\
	"Unknown"

#define NdasRixGetResultString(Code) (Code == NRMX_RESULT_SUCCESS)?"SUCCESS":\
	(Code == NRMX_RESULT_UNSUCCESSFUL)?"FAIL":\
	(Code == NRMX_RESULT_RAID_SET_NOT_FOUND)?"RAID_SET_NOT_FOUND":\
	(Code == NRMX_RESULT_LOWER_USN)?"LOW_USN":\
	/*(Code == NRMX_RESULT_TERMINATING)?"TERMINATING":*/\
	(Code == NRMX_RESULT_REQUIRE_SYNC)?"NRMX_RESULT_REQUIRE_SYNC":\
	(Code == NRMX_RESULT_NO_CHANGE)?"NO_CHANGE":\
	(Code == NRMX_RESULT_GRANTED)?"GRANTED":\
	/*(Code == NRMX_RESULT_PENDING)?"PENDING":*/\
	(Code == NRMX_RESULT_UNSUPPORTED)?"NOTSUPPORTED":\
	(Code == NRMX_RESULT_INVALID_LOCK_ID)?"INVALID_LOCK_ID":\
	"UNKNOWN"

#endif

__forceinline 
UINT8 
NdasRaidNodeDefectCodeToRmdUnitStatus (
	UINT8 DefectCode
	) 
{
	UINT8 status = 0;

	if (DefectCode & NRMX_NODE_DEFECT_BAD_SECTOR)  {

		status|= NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR;
	}

	if (DefectCode & NRMX_NODE_DEFECT_BAD_DISK)  {

		status|= NDAS_UNIT_META_BIND_STATUS_BAD_DISK;
	}

	if (DefectCode & NRMX_NODE_DEFECT_REPLACED_BY_SPARE)  {

		status|= NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE;
	}

	return status;
}

__forceinline 
UINT8 
NdasRaidRmdUnitStatusToDefectCode (
	UINT8 UnitStatus
	) 
{
	UINT8 defectCode = 0;
	
	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)  {
	
		defectCode |= NRMX_NODE_DEFECT_BAD_SECTOR;
	}

	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK)  {

		defectCode |= NRMX_NODE_DEFECT_BAD_DISK;
	}

	if (UnitStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)  {

		defectCode |= NRMX_NODE_DEFECT_REPLACED_BY_SPARE;
	}

	return defectCode;
}

#define NDASR_RANGE_NO_OVERLAP 				0
#define NDASR_RANGE_SRC1_HEAD_OVERLAP 		1
#define NDASR_RANGE_SRC1_TAIL_OVERLAP 		2
#define NDASR_RANGE_SRC1_CONTAINS_SRC2		3
#define NDASR_RANGE_SRC2_CONTAINS_SRC1		4 // Return this if exact match


extern struct _DRAID_GLOBALS *DRaidGlobals;

#endif /* LANSCSI_NDASR_H */
