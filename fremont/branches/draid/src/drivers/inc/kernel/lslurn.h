#ifndef LANSCSI_LURN_H
#define LANSCSI_LURN_H

#include "lurdesc.h"
#include "ndasbusioctl.h"
#include "LSTransport.h"
#include "LSProto.h"
#include "LSCcb.h"
#include "lsutils.h"

#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif
#ifndef C_ASSERT_SIZEOF
#define C_ASSERT_SIZEOF(type, size) C_ASSERT(sizeof(type) == size)
#endif

//
//	Disk information for SCSIOP_INQUIRY
//
#define NDAS_DISK_VENDOR_ID "NDAS"

#define	PRODUCT_REVISION_LEVEL "1.0"

//
//	Pool tags for LUR
//
#define LUR_POOL_TAG							'rlSL'
#define LURN_POOL_TAG							'ulSL'
#define LURN_DESC_POOL_TAG					'dlSL'
#define LURNEXT_POOL_TAG						'elSL'
#define LURNEXT_ENCBUFF_TAG						'xlSL'
#define LURNEXT_WRITECHECK_BUFFER_POOLTAG		'weSL'

//
//	Defines for reconnecting
//	Do not increase RECONNECTION_MAX_TRY.
//	Windows XP allows busy return within 19 times.
//
#define MAX_RECONNECTION_INTERVAL	(NANO100_PER_SEC*4)
#define RECONNECTION_MAX_TRY		19

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
	(pLurQuery) = (PLUR_QUERY)ExAllocatePoolWithTag(NonPagedPool, SIZE_OF_LURQUERY(QUERY_DATA_LENGTH, RETURN_INFORMATION_SIZE), NDSC_PTAG_IOCTL); \
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
	ACCESS_MASK				AccessRight;
	UINT64					UnitBlocks;
	UINT32					BlockBytes;
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

//
// Device lock index
// NOTE: The number of lock index are irrelevant to NDAS chip lock index.
//

#define LURNDEVLOCK_ID_NONE		0
#define LURNDEVLOCK_ID_XIFS		1

//
// Device lock numbers
//

#define NDAS_NR_GPLOCK		(1 + 2)	// NDAS chip 1.1, 2.0 icluding lock-none
#define NDAS_NR_ADV_GPLOCK	(1 + 8)	// NDAS chip 2.5

#define NDAS_NR_MAX_GPLOCK	((NDAS_NR_ADV_GPLOCK>NDAS_NR_GPLOCK)? \
							NDAS_NR_ADV_GPLOCK:NDAS_NR_GPLOCK)

//
// Lock data length
//

#define LURNDEVLOCK_LOCKDATA_LENGTH		64

#define LURNLOCK_OPCODE_ACQUIRE		0x01	// Available in NDAS chip 1.1, 2.0.
#define LURNLOCK_OPCODE_RELEASE		0x02	// Available in NDAS chip 1.1, 2.0.
#define LURNLOCK_OPCODE_QUERY_OWNER	0x03	// Available in NDAS chip 1.1, 2.0.
#define LURNLOCK_OPCODE_GETDATA		0x04	// Available in NDAS chip 1.1, 2.0.
#define LURNLOCK_OPCODE_SETDATA		0x05	// Available in NDAS chip 2.5 and later.
#define LURNLOCK_OPCODE_BREAK		0x06	// Available in NDAS chip 2.5 and later.

typedef struct _LURN_DEVLOCK_CONTROL {
	UINT32	LockId;
	BYTE	LockOpCode;
	BYTE	AdvancedLock:1;				// advanced GP lock operation. ADV_GPLOCK feature required.
	BYTE	AddressRangeValid:1;
	BYTE	RequireLockAcquisition:1;
	BYTE	Reserved1:5;
	BYTE	Reserved2[2];
	UINT64	StartingAddress;
	UINT64	EndingAddress;
	UINT64	ContentionTimeOut;

	//
	// Lock data
	//

	BYTE	LockData[LURNDEVLOCK_LOCKDATA_LENGTH];
} LURN_DEVLOCK_CONTROL, *PLURN_DEVLOCK_CONTROL;

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


typedef	UINT16 LURN_TYPE, *PLURN_TYPE; // LURN_AGGREGATION, LURN_, LURN_IDE_*

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
#define LURN_STATUS_RUNNING							0x00000001
#define LURN_STATUS_STALL							0x00000002
#define LURN_STATUS_STOP_PENDING					0x00000003
#define LURN_STATUS_STOP							0x00000004
#define LURN_STATUS_DESTROYING						0x00000005

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

//////////////////////////////////////////////////////////////////////////
//
//	Logical Unit Relation Node
//

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
	
	//
	// Block bytes of itself.
	// Report this to the upper LURN
	//

	UINT32					BlockBytes;

	//
	// Children's block bytes
	// All children must have the same block bytes
	//

	UINT32					ChildBlockBytes;


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

	PLURELATION_NODE_DESC	SavedLurnDesc;

	//
	//	LU relation
	//
	ULONG					LurnChildIdx;	// This node is nth child.
	PLURELATION				Lur;
	PLURELATION_NODE		LurnParent;
	PLURELATION_NODE		LurnSibling;
	ULONG					LurnChildrenCnt;
	PLURELATION_NODE		LurnChildren[1];	// Should be last member.
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

//
//	LURFLAG_W2K_READONLY_PATCH	:
//		Windows 2000 NTFS does not recognize read-only disk.
//		Emulate read-only NTFS by reporting read/write disk, but not actually writable.
//		And, NDAS file system filter should deny write file operation.
//

#define LURFLAG_W2K_READONLY_PATCH		0x00000001
#define LURFLAG_WRITE_CHECK_REQUIRED		0x00000002

typedef struct _LURELATION {

	UINT16					Length;
	UINT16					Type;
	UCHAR					LurId[LURID_LENGTH];
	UINT32					SlotNo;
	UINT32					LurFlags;
	PVOID					AdapterFdo;
	LURN_EVENT_CALLBACK		LurnEventCallback;
	UINT16					DevType;
	UINT16					DevSubtype;

	//
	// Disk ending block address
	//

	UINT64					EndingBlockAddr;

	//
	//	Lowest version number of NDAS hardware counterparts.
	//

	UINT16					LowestHwVer;

	//
	//	Content encrypt
	//	CntEcrKeyLength is the key length in bytes.
	//

	UCHAR	CntEcrMethod;
	USHORT	CntEcrKeyLength;
	UCHAR	CntEcrKey[LUR_CONTENTENCRYPT_KEYLENGTH];

	//
	//	Device mode
	//

	NDAS_DEV_ACCESSMODE	DeviceMode;

	//
	//	NDAS features
	//

	NDAS_FEATURES		SupportedNdasFeatures;
	NDAS_FEATURES		EnabledNdasFeatures;
	
	//
	//	Block access control list
	//

	LSU_BLOCK_ACL			SystemBacl;
	LSU_BLOCK_ACL			UserBacl;

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
		OUT PLURELATION			*Lur,
		IN BOOLEAN				EnableSecondary,
		IN BOOLEAN				EnableW2kReadOnlyPacth,
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
	   ULONG					LurMaxOsRequestLength, // in bytes
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

NTSTATUS
LurnSendStopCcb(
		PLURELATION_NODE	Lurn
	);

#endif // LANSCSI_LURN_H