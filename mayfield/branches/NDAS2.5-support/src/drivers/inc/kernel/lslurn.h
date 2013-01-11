#ifndef LANSCSI_LURN_H
#define LANSCSI_LURN_H

#include "lurdesc.h"
#include "ndasbusioctl.h"
#include "LSTransport.h"
#include "LSProto.h"
#include "LSCcb.h"

//
//	Disk information for SCSIOP_INQUIRY
//
#define NDAS_DISK_VENDOR_ID "NDAS"

#define	PRODUCT_REVISION_LEVEL "1.0"

//
//	Pool tags for LUR
//
#define LUR_POOL_TAG			'rlSL'
#define LURN_POOL_TAG			'ulSL'
#define LURNEXT_POOL_TAG		'elSL'
#define LURNEXT_ENCBUFF_TAG		'xlSL'

//
//	Defines for reconnecting
//	Do not increase RECONNECTION_MAX_TRY.
//	Windows XP allows busy return within 19 times.
//
#define MAX_RECONNECTION_INTERVAL	(NANO100_PER_SEC*4)
#define RECONNECTION_MAX_TRY		19

//
//
//
#define BLOCK_SIZE_BITS				9
#define BLOCK_SIZE					(1<<BLOCK_SIZE_BITS)

#define BITS_PER_BYTE				8
#define	BITS_PER_BLOCK				(BITS_PER_BYTE * BLOCK_SIZE)

#define	LUR_CONTENTENCRYPT_KEYLENGTH	64

//////////////////////////////////////////////////////////////////////////
//
//	LUR Ioctl
//

//
//	LUR Query
//
typedef enum _LUR_INFORMATION_CLASS {

	LurPrimaryLurnInformation=1,
	LurEnumerateLurn,
	LurRefreshLurn

} LUR_INFORMATION_CLASS, *PLUR_INFORMATION_CLASS;

typedef struct _LUR_QUERY {

	UINT32					Length;
	LUR_INFORMATION_CLASS	InfoClass;
	UINT32					QueryDataLength; // <- MUST be multiples of 4 for information to fit in align
	UCHAR					QueryData[1];

}	LUR_QUERY, * PLUR_QUERY;

#define SIZE_OF_LURQUERY(QUERY_DATA_LENGTH, RETURN_INFORMATION_SIZE) \
	(FIELD_OFFSET(LUR_QUERY, QueryData) + QUERY_DATA_LENGTH + RETURN_INFORMATION_SIZE)

#define LUR_QUERY_INITIALIZE(pLurQuery, INFO_CLASS, QUERY_DATA_LENGTH, RETURN_INFORMATION_SIZE) \
	(pLurQuery) = (PLUR_QUERY)ExAllocatePoolWithTag(NonPagedPool, SIZE_OF_LURQUERY(QUERY_DATA_LENGTH, RETURN_INFORMATION_SIZE), LSMP_PTAG_IOCTL); \
	if(pLurQuery)	\
	{	\
		RtlZeroMemory((pLurQuery), SIZE_OF_LURQUERY(QUERY_DATA_LENGTH, RETURN_INFORMATION_SIZE));	\
		(pLurQuery)->Length = SIZE_OF_LURQUERY(QUERY_DATA_LENGTH, RETURN_INFORMATION_SIZE);	\
		(pLurQuery)->InfoClass = INFO_CLASS;	\
		(pLurQuery)->QueryDataLength = QUERY_DATA_LENGTH;	\
	}

// address of the returned information
#define LUR_QUERY_INFORMATION(QUERY_PTR) \
	(((PBYTE)QUERY_PTR + FIELD_OFFSET(LUR_QUERY, QueryData) + (QUERY_PTR)->QueryDataLength))

//
//	Returned information
//
typedef struct _LURN_INFORMATION {

	UINT32					Length;
	UINT32					LurnId;
	UINT32					LurnType;
	TA_LSTRANS_ADDRESS		NetDiskAddress;
	TA_LSTRANS_ADDRESS		BindingAddress;
	UCHAR					UnitDiskId;
	UCHAR					Reserved;
	UCHAR					UserID[LSPROTO_USERID_LENGTH];
	UCHAR					Password[LSPROTO_PASSWORD_LENGTH];
	UCHAR					Password_v2[LSPROTO_PASSWORD_V2_LENGTH];	
	ACCESS_MASK				AccessRight;
	UINT64					UnitBlocks;
	UINT32					BlockUnit;
	UINT32					StatusFlags;

	UCHAR					Reserved2[16];

}	LURN_INFORMATION, *PLURN_INFORMATION;


typedef struct _LURN_PRIMARYINFORMATION {

	UINT32					Length;
	LURN_INFORMATION		PrimaryLurn;

}	LURN_PRIMARYINFORMATION, *PLURN_PRIMARYINFORMATION;

typedef struct _LURN_ENUM_INFORMATION {

	UINT32					Length;
	LURN_INFORMATION		Lurns[1];

}	LURN_ENUM_INFORMATION, *PLURN_ENUM_INFORMATION;

typedef struct _LURN_DVD_STATUS {
	UINT32					Length;
	LARGE_INTEGER			Last_Access_Time;
	UINT32					Status;
} LURN_DVD_STATUS, *PLURN_DVD_STATUS;

#define REFRESH_POOL_TAG 'SHFR'
typedef struct _LURN_REFRESH {
	UINT32					Length;
	ULONG					CcbStatusFlags;
} LURN_REFRESH, *PLURN_REFRESH;

#define		LURN_UPDATECLASS_WRITEACCESS_USERID		0x0001
#define		LURN_UPDATECLASS_READONLYACCESS			0x0002

typedef struct _LURN_UPDATE {

	UINT16			UpdateClass;

}	LURN_UPDATE, *PLURN_UPDATE;

//////////////////////////////////////////////////////////////////////////
//
//	LURN event structure
//
typedef struct _LURELATION LURELATION, *PLURELATION;
typedef struct _LURN_EVENT  LURN_EVENT, *PLURN_EVENT;

typedef VOID (*LURN_EVENT_CALLBACK)(
				PLURELATION	Lur,
				PLURN_EVENT	LurnEvent
			);

typedef	enum _LURN_EVENT_CLASS {

	LURN_REQUEST_NOOP_EVENT

} LURN_EVENT_CLASS, *PLURN_EVENT_CLASS;

typedef struct _LURN_EVENT {

	UINT32				LurnId;
	LURN_EVENT_CLASS	LurnEventClass;

} LURN_EVENT, *PLURN_EVENT;


//////////////////////////////////////////////////////////////////////////
//
//	LURN interface
//
#define LUR_MAX_LURNS_PER_LUR		64

typedef struct _LURELATION_NODE LURELATION_NODE, *PLURELATION_NODE;
typedef struct _LURELATION_NODE_DESC LURELATION_NODE_DESC, *PLURELATION_NODE_DESC;

#define LUR_GETROOTNODE(LUR_POINTER)	((LUR_POINTER)->Nodes[0])

typedef NTSTATUS (*LURN_INITIALIZE)(PLURELATION_NODE Lurn, PLURELATION_NODE_DESC LurnDesc);
typedef NTSTATUS (*LURN_DESTROY)(PLURELATION_NODE Lurn);
typedef NTSTATUS (*LURN_REQUEST)(PLURELATION_NODE Lurn, PCCB Ccb);

typedef struct _LURN_FUNC {

	LURN_INITIALIZE	LurnInitialize;
	LURN_DESTROY	LurnDestroy;
	LURN_REQUEST	LurnRequest;
	PVOID			Reserved[12];
	PVOID			LurnExtension;

} LURN_FUNC, *PLURN_FUNC;


typedef	UINT16 LURN_TYPE, *PLURN_TYPE;

#define NR_LURN_LAST_INTERFACE		11
#define NR_LURN_MAX_INTERFACE		16

typedef struct _LURN_INTERFACE {

	UINT16			Type;
	UINT16			Length;
	LURN_TYPE		LurnType;
	UINT32			LurnCharacteristic;
	LURN_FUNC		LurnFunc;

} LURN_INTERFACE, *PLURN_INTERFACE;

extern PLURN_INTERFACE	LurnInterfaceList[NR_LURN_MAX_INTERFACE];

#define LURN_IS_RUNNING(LURNSTATUS)				(((LURNSTATUS) == LURN_STATUS_RUNNING) || ((LURNSTATUS) == LURN_STATUS_STALL))

#define LURN_STATUS_INIT							0x00000000
#define LURN_STATUS_RUNNING						0x00000001
#define LURN_STATUS_STALL							0x00000002
#define LURN_STATUS_STOP_PENDING					0x00000003
#define LURN_STATUS_STOP							0x00000004
#define LURN_STATUS_DESTROYING					0x00000005
#define LURN_STATUS_DEFECTIVE						0x00000006

// LURN_STATUS_DEFECTIVE:
// Disk/Chip is defective(on set in redundent RAID mode).
// If network or disk is down, or whole disk is inaccessible, it is stop status.
// In redundent RAID mode, defective disk is removed from active RAID role.
// Disk is set to defective only when reading fails.
//		because OS can handle write-failure and verify-failure case.
//

typedef struct _LURELATION LURELATION, *PLURELATION ;
typedef struct _RAID_INFO RAID_INFO, *PRAID_INFO;
typedef struct _RAID_INFO_LEGACY RAID_INFO_LEGACY, *PRAID_INFO_LEGACY;

typedef enum _LURN_FAULT_TYPE {
	LURN_ERR_SUCCESS = 0,	// Count successful disk operation
	LURN_ERR_CONNECT,	// Connect failure. 
	LURN_ERR_LOGIN, 	// Login failure. to do: get different error code from each login step to differentiate various cases. Error code is LSS_LOGIN_*
	LURN_ERR_NDAS_OP, // Operation on NDAS chip has failed such as discovery, vendor command, NOP
	LURN_ERR_FLUSH,	// Error occurred duing flush.
	LURN_ERR_DISK_OP, 	// Operation on disk has failed such as identify, disk error not related to sector ops.
	LURN_ERR_READ, 	// Error occured during disk read
						// Single read failure does not mean actuall IO failure. 
						// Single IO failure is recorded but only multiple read/write is considered IO fault.
	LURN_ERR_WRITE, 
	LURN_ERR_DIGEST,	// CRC or checksum failure. Defective network equipment or SW bug.
	LURN_ERR_UNKNOWN, // Unspecified error. Should not happen.
	LURN_ERR_LAST
} LURN_FAULT_TYPE;


// Fault code for LURN_ERR_NDAS_OP
#define LURN_FAULT_LOCK_CLEANUP  	0x0201

// Fault code for LURN_ERR_DISK_OP
#define LURN_FAULT_IDENTIFY		0x0301
#define LURN_FAULT_NOT_EXIST		0x0302	// Target does not exist. Currently not implemented by ~2.0 HW.

// Fault code for LURN_ERR_READ/LURN_ERR_WRITE
#define LURN_FAULT_COMMUNICATION	0x0401	// Failed to read/write packet during IO. 
											// This can be caused by network error but also by bad sector or long spin-up time.
#define LURN_FAULT_OUT_OF_BOUND_ACCESS 0x0403 // Accessed out of bound.
#define LURN_FAULT_TRANSFER_CRC		0x0404		// Data is corrupted during transfering over the cable
#define LURN_FAULT_BAD_SECTOR			0x0405
#define LURN_FAULT_NO_TARGET			0x0406

typedef struct _LURN_FAULT_IO {
	UINT64 Address;
	UINT32 Length;
} LURN_FAULT_IO, *PLURN_FAULT_IO;


// LURN Fault causes
#define LURN_FCAUSE_HOST_NETWORK 	(1<<0)	// Host network is down
#define LURN_FCAUSE_TARGET_DOWN 		(1<<1)	// Target network or disk is down
#define LURN_FCAUSE_BAD_DISK			(1<<3)	// Disk is reachable but all access to disk is unavaiable.
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


typedef struct _LURELATION_NODE {
	UINT16					Length;
	UINT16					Type; // LSSTRUC_TYPE_LURN

	//
	//	spinlock to protect LurnStatus
	//
	KSPIN_LOCK				LurnSpinLock;

	ULONG					LurnStatus;  // LURN_STATUS_*

	UINT32					LurnId;
	LURN_TYPE				LurnType; // LURN_RAID0, ...
	PLURN_INTERFACE			LurnInterface;

	UINT64					StartBlockAddr;	// Start address of RAID. 0 for RAID1, aggregated disks
	UINT64					EndBlockAddr;
	UINT64					UnitBlocks;

	ACCESS_MASK				AccessRight;

	LONG					NrOfStall;

	LURN_FAULT_INFO		FaultInfo;

	//
	//	set if UDMA restriction is valid
	//

	BOOLEAN				UDMARestrictValid;


	//
	//	Highest UDMA mode
	//	0xff to disable UDMA mode
	//

	BYTE				UDMARestrict;


	//
	//	Number of reconnection trial
	//	100-nanosecond time of reconnection interval
	//

	LONG					ReconnTrial;
	INT64					ReconnInterval;

	//
	// Disk Status information
	//
	UINT64				LastAccessedAddress; // Used as hint for optimizing read balancing.

	//
	//	extension for LU relation node
	//

	union{
		PVOID				LurnExtension; // LURNEXT_IDE_DEVICE for IDE devices.
		PRAID_INFO			LurnRAIDInfo;	// for RAID LURN. Child will have LurnExtension
	};

	PLURELATION_NODE_DESC	LurnDesc;

	//
	//	LU relation
	//
	ULONG					LurnChildIdx;	// This node is nth child.
	PLURELATION				Lur;
	PLURELATION_NODE		LurnParent;
	PLURELATION_NODE		LurnSibling;
	ULONG					LurnChildrenCnt;
	PLURELATION_NODE		LurnChildren[1];

} LURELATION_NODE, *PLURELATION_NODE;


//////////////////////////////////////////////////////////////////////////
//
//	Logical Unit Relation
//
//
//	For SCSI commands:	
//				LurId[0] = PathId
//				LurId[1] = TargetId
//				LurId[2] = Lun
//

//
//	LUR flags
//	Drivers set the default values,but users can override them with LurOptions in LurDesc.
//
#define LURFLAG_FAKEWRITE				0x00000001
#define	LURFLAG_WRITESHARE_PS			0x00000002	// Write-sharing in Primary-Secondary mode.
#define LURFLAG_NOT_REGISTER			0x00000004
#define LURFLAG_LOCKEDWRITE				0x00000008
#define LURFLAG_SHAREDWRITE				0x00000020
#define LURFLAG_OOB_SHAREDWRITE			0x00000040
#define LURFLAG_NDAS_2_0_WRITE_CHECK 	0x00000080
#define LURFLAG_DYNAMIC_REQUEST_SIZE 	0x00000100


typedef struct _LURELATION {

	UINT16					Length;
	UINT16					Type;
	UCHAR					LurId[LURID_LENGTH];
	UINT32					SlotNo;
	ULONG					MaxBlocksPerRequest;
	UINT32					LurFlags;
	ACCESS_MASK				DesiredAccess;
	ACCESS_MASK				GrantedAccess;
	PVOID					AdapterFdo;
	LURN_EVENT_CALLBACK		LurnEventCallback;
	UINT16					DevType;
	UINT16					DevSubtype;

	//
	//	Content encrypt
	//	CntEcrKeyLength is the key length in bytes.
	//

	UCHAR	CntEcrMethod;
	USHORT	CntEcrKeyLength;
	UCHAR	CntEcrKey[LUR_CONTENTENCRYPT_KEYLENGTH];


	//
	//	Children
	//

	UINT32					NodeCount;
	PLURELATION_NODE		Nodes[1];

} LURELATION, *PLURELATION;


//////////////////////////////////////////////////////////////////////////
//
//	exported variables
//
extern UINT32 LurnInterfaceCnt;

//////////////////////////////////////////////////////////////////////////
//
//	exported functions
//


//
//	LURELATION
//
NTSTATUS
LurCreate(
		IN PLURELATION_DESC		LurDesc,
		IN UINT32				DefaultLurFlags,
		OUT PLURELATION			*Lur,
		IN ACCESS_MASK			InitAccessMode,
		IN PVOID				AdapterFdo,
		IN LURN_EVENT_CALLBACK	LurnEventCallback
  );

VOID
LurClose(
		PLURELATION			Lur
	);

NTSTATUS
LurRequest(
		PLURELATION			Lur,
		PCCB				Ccb
	);

NTSTATUS
LurTranslateAddTargetDataToLURDesc(
	   PLANSCSI_ADD_TARGET_DATA	AddTargetData,
	   ULONG					LurMaxRequestBlocks,
	   LONG						LurDescLengh,
	   PLURELATION_DESC			LurDesc
	);

VOID
LurCallBack(
	PLURELATION	Lur,
	PLURN_EVENT	LurnEvent
	);


//
//	LURELATION NODE
//
NTSTATUS
LurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION				Lur,
		PLURELATION_NODE_DESC	LurnDesc
	);

NTSTATUS
LurnRequest(
		PLURELATION_NODE Lurn,
		PCCB Ccb
	);

NTSTATUS
LurnDestroy(
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

UINT32
LurnGetMaxRequestLength(
	 PLURELATION_NODE Lurn
);

NTSTATUS
LurnInitializeDefault(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	);

NTSTATUS
LurnDestroyDefault(
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
	
#endif // LANSCSI_LURN_H
