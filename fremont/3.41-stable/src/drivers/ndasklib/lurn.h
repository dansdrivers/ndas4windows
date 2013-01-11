#ifndef _NDAS_LURN_H_
#define _NDAS_LURN_H_


//
//	Disk information for SCSIOP_INQUIRY
//
#define NDAS_DISK_VENDOR_ID "NDAS"

#define	PRODUCT_REVISION_LEVEL "1.0"

//
//	Pool tags for LURN
//
#define LUR_POOL_TAG							'rlSL'
#define LURN_POOL_TAG							'ulSL'
#define LURN_DESC_POOL_TAG						'dlSL'
#define LURNEXT_POOL_TAG						'elSL'
#define LURNEXT_ENCBUFF_TAG						'xlSL'
#define LURNEXT_WRITECHECK_BUFFER_POOLTAG		'weSL'



//////////////////////////////////////////////////////////////////////////
//
//	LURN interface
//
#define LUR_MAX_LURNS_PER_LUR		64

struct _LURELATION_NODE;

#define LUR_GETROOTNODE(LUR_POINTER)	((LUR_POINTER)->Nodes[0])

typedef 
struct _LURN_FUNC {

	NTSTATUS 
	(*LurnCreate) (struct _LURELATION_NODE	*Lurn,
				   PLURELATION_NODE_DESC	LurnDesc);

	VOID 
	(*LurnClose) (struct _LURELATION_NODE	*Lurn);

	VOID
	(*LurnStop) (struct _LURELATION_NODE	*Lurn);

	NTSTATUS 
	(*LurnRequest) (struct _LURELATION_NODE	*Lurn,
					PCCB					Ccb);

	PVOID			Reserved[12];
	PVOID			LurnExtension;

} LURN_FUNC, *PLURN_FUNC;


#define NR_LURN_MAX_INTERFACE	7

typedef struct _LURN_INTERFACE {

	UINT16		Type;
	UINT16		Length;
	LURN_TYPE	LurnType;
	UCHAR		DeviceInterfaceType;
	UINT32		LurnCharacteristic;
	LURN_FUNC	LurnFunc;

} LURN_INTERFACE, *PLURN_INTERFACE;

extern PLURN_INTERFACE	LurnInterfaceList[NR_LURN_MAX_INTERFACE];

#define LURN_IS_RUNNING(LURNSTATUS)				(FlagOn((LURNSTATUS), (LURN_STATUS_RUNNING | LURN_STATUS_STALL)))

#define LURN_STATUS_INIT							0x00000001
#define LURN_STATUS_RUNNING							0x00000002
#define LURN_STATUS_STALL							0x00000004
#define LURN_STATUS_STOP_PENDING					0x00000008
#define LURN_STATUS_STOP							0x00000010
#define LURN_STATUS_DESTROYING						0x00000020
#define LURN_STATUS_SHUTDOWN						0x00000080

typedef struct _RAID_INFO_LEGACY RAID_INFO_LEGACY, *PRAID_INFO_LEGACY;

typedef struct _NDASR_INFO NDASR_INFO, *PNDASR_INFO;

typedef enum _LURN_FAULT_TYPE {

	LURN_ERR_SUCCESS = 0,	// Count successful disk operation
	LURN_ERR_CONNECT,		// Connect failure. 
	LURN_ERR_LOGIN, 		// Login failure. to do: get different error code from each login step to differentiate various cases. Error code is LSS_LOGIN_*
	LURN_ERR_NDAS_OP,		// Operation on NDAS chip has failed such as discovery, vendor command, NOP
	LURN_ERR_FLUSH,			// Error occurred duing flush.
	LURN_ERR_DISK_OP, 		// Operation on disk has failed such as identify, disk error not related to sector ops.
	LURN_ERR_READ, 			// Error occured during disk read
							// Single read failure does not mean actuall IO failure. 
							// Single IO failure is recorded but only multiple read/write is considered IO fault.
	LURN_ERR_WRITE, 
	LURN_ERR_VERIFY, 
	LURN_ERR_DIGEST,		// CRC or checksum failure. Defective network equipment or SW bug.
	LURN_ERR_UNKNOWN,		// Unspecified error. Should not happen.
	LURN_ERR_LAST

} LURN_FAULT_TYPE;


// Fault code for LURN_ERR_NDAS_OP

#define LURN_FAULT_LOCK_CLEANUP  		0x0201

// Fault code for LURN_ERR_DISK_OP

#define LURN_FAULT_IDENTIFY				0x0301
#define LURN_FAULT_NOT_EXIST			0x0302	// Target does not exist. Currently not implemented by ~2.0 HW.

// Fault code for LURN_ERR_READ/LURN_ERR_WRITE

#define LURN_FAULT_COMMUNICATION		0x0401	// Failed to read/write packet during IO. 
												// This can be caused by network error but also by bad sector or long spin-up time.
#define LURN_FAULT_OUT_OF_BOUND_ACCESS	0x0403	// Accessed out of bound.
#define LURN_FAULT_TRANSFER_CRC			0x0404	// Data is corrupted during transfering over the cable
#define LURN_FAULT_BAD_SECTOR			0x0405
#define LURN_FAULT_NO_TARGET			0x0406

typedef struct _LURN_FAULT_IO {

	UINT64 Address;
	UINT32 Length;

} LURN_FAULT_IO, *PLURN_FAULT_IO;


// LURN Fault causes
#define LURN_FCAUSE_HOST_NETWORK 	(1<<0)	// Host network is down
#define LURN_FCAUSE_TARGET_DOWN 	(1<<1)	// Target network or disk is down
#define LURN_FCAUSE_BAD_DISK		(1<<3)	// Disk is reachable but all access to disk is unavaiable.
#define LURN_FCAUSE_BAD_SECTOR		(1<<4)	// Disk has bad sector. Single bad sector can make disk hang..


typedef struct _LURN_FAULT_INFO {

	LURN_FAULT_TYPE	LastFaultType;
	LARGE_INTEGER 	LastFaultTime;

	LONG	ErrorCount[LURN_ERR_LAST];
	LONG	LastErrorCode[LURN_ERR_LAST];

	// Store additional information for disk IO operation

	UINT32	LastIoOperation;
	UINT64	LastIoAddr;		// in sector
	UINT32	LastIoLength;	// in sector

	ULONG	FaultCause;		// Guessing of cause of fault.

} LURN_FAULT_INFO, *PLURN_FAULT_INFO;

//////////////////////////////////////////////////////////////////////////
//
//	Logical Unit Relation Node
//

typedef struct _LURNEXT_IDE_DEVICE LURNEXT_IDE_DEVICE, *PLURNEXT_IDE_DEVICE;

#define NDAS_LOCK_COUNT	4

#define NDAS_RW_LOCK		0	// wil convert unit 0 to 0, unit 1 to 1 
#define NDAS_CONTENT_LOCK	2	// used to content locking
#define NDAS_USER_LOCK		3 

C_ASSERT( LURNDEVLOCK_ID_XIFS == NDAS_USER_LOCK );

#define NDAS_MAX_LOCKING_DURATION	(150 * NANO100_PER_MSEC)
#define NDAS_MAX_LOCKING_DELAY		(50 * NANO100_PER_MSEC)

typedef struct _LURELATION_NODE {

	UINT16					Length;
	UINT16					Type; // LSSTRUC_TYPE_LURN

	//	spinlock to protect LurnStatus

	KSPIN_LOCK				SpinLock;

	ULONG					LurnStatus;  // LURN_STATUS_*

	BOOLEAN					NdasLocked[NDAS_LOCK_COUNT];
	LARGE_INTEGER			LockingDelay;
	LARGE_INTEGER			LockingTime;
	BOOLEAN					CascadeLock;

	UINT32					LurnId;
	LURN_TYPE				LurnType; // LURN_RAID0, ...
	PLURN_INTERFACE			LurnInterface;

	UINT64					UnitBlocks;
	UINT64					DataUnitBlocks;
	
	//
	// Block bytes of itself.
	// Report this to the upper LURN
	//

	UINT32					BlockBytes;

	UINT32					MaxDataSendLength;
	UINT32					MaxDataRecvLength;

	//
	// Children's block bytes
	// All children must have the same block bytes
	//

	UINT32					ChildBlockBytes;


	ACCESS_MASK				AccessRight;

	LONG					NrOfStall;

	LURN_FAULT_INFO			FaultInfo;

	//
	//	set if UDMA restriction is valid
	//

	BOOLEAN					UDMARestrictValid;


	//
	//	Highest UDMA mode
	//	0xff to disable UDMA mode
	//

	BYTE					UDMARestrict;


	//
	//	Number of reconnection trial
	//	100-nanosecond time of reconnection interval
	//

	LONG					ReconnTrial;
	INT64					ReconnInterval;

	//
	// Disk Status information
	//
	UINT64					LastAccessedAddress; // Used as hint for optimizing read balancing.

	//
	//	extension for LU relation node
	//

	union {

		PVOID				LurnExtension; // LURNEXT_IDE_DEVICE for IDE devices
		PLURNEXT_IDE_DEVICE	IdeDisk;
		PNDASR_INFO			NdasrInfo;
	};

	//
	// CCB status flag for stop reason.
	//

	USHORT					LurnStopReasonCcbStatusFlags;

	//
	//	LU relation
	//
	PLURELATION_NODE_DESC	LurnDesc;

	ULONG					LurnChildIdx;	// This node is nth child.
	PLURELATION				Lur;
	struct _LURELATION_NODE	*LurnParent;
	struct _LURELATION_NODE *LurnSibling;

	ULONG					LurnChildrenCnt;
	struct _LURELATION_NODE	*LurnChildren[1];	// Should be last member.

} LURELATION_NODE, *PLURELATION_NODE;


typedef struct _LURNDESC_TABLE {

	PLURELATION_NODE_DESC	LurnDesc;
	PLURELATION_NODE		Lurn;

} LURNDESC_ENTRY, *PLURNDESC_ENTRY;

extern NDAS_FEATURES	LurNdasFeatures[];

#if 0

//////////////////////////////////////////////////////////////////////////
//
// LUR global data
//

typedef struct _LURELATION_GLOBALS {
	KEVENT	LurSystemSynchEvent;
	LONG	LurCount;
} LURELATION_GLOBALS, *PLURELATION_GLOBALS;

extern LURELATION_GLOBALS LurGlobals;

#endif

//////////////////////////////////////////////////////////////////////////
//
//	exported functions
//	LURELATION NODE
//

NTSTATUS
LurnCreate (
	PLURELATION				Lur,
	PLURELATION_NODE		Lurn
	);

NTSTATUS
LurnRequest (
	PLURELATION_NODE Lurn,
	PCCB Ccb
	);

VOID
LurnStop (
	PLURELATION_NODE Lurn
	);

VOID
LurnClose (
	PLURELATION_NODE Lurn
	);

NTSTATUS
LurnAllocate(
		PLURELATION_NODE	*Lurn,
		LONG				ChildrenCnt
	);

VOID
LurnFree(
		PLURELATION_NODE Lurn
	);

NTSTATUS
LurnCreateDefault(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	);

VOID
LurnCloseDefault (
	PLURELATION_NODE Lurn
	);

VOID
LurnResetFaultInfo(
	PLURELATION_NODE Lurn
	);

NTSTATUS
LurnRecordFault(
	PLURELATION_NODE Lurn, LURN_FAULT_TYPE Type, UINT32 ErrorCode, PVOID Param
	);


ULONG
LurnGetCauseOfFault(
	PLURELATION_NODE Lurn
);

NTSTATUS
LurnSendStopCcb(
		PLURELATION_NODE	Lurn
	);

#endif
