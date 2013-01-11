#ifndef LANSCSI_LURN_H
#define LANSCSI_LURN_H

#include "lurdesc.h"
#include "lsbusioctl.h"
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
//	UINT32					LurnCount;
	LURN_INFORMATION		Lurns[1];

}	LURN_ENUM_INFORMATION, *PLURN_ENUM_INFORMATION;

// Added by ILGU HONG 2004_07_05
typedef struct _LURN_DVD_STATUS {
	UINT32					Length;
	LARGE_INTEGER			Last_Access_Time;
	UINT32					Status;
} LURN_DVD_STATUS, *PLURN_DVD_STATUS;
// Added by ILGU HONG 2004_07_05 end

#define REFRESH_POOL_TAG 'SHFR'
typedef struct _LURN_REFRESH {
	UINT32					Length;
	ULONG					CcbStatusFlags;
} LURN_REFRESH, *PLURN_REFRESH;

#define		LURN_UPDATECLASS_WRITEACCESS_USERID		0x0001

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
#define LUR_MAX_LURNS_PER_LUR		128

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

#define NR_LURN_INTERFACE			8
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
typedef struct _RAID_INFO RAID_INFO, *PRAID_INFO ;

typedef struct _LURELATION_NODE {
	UINT16					Length;
	UINT16					Type;

	//
	//	spinlock to protect LurnStatus
	//
	KSPIN_LOCK				LurnSpinLock;

	ULONG					LurnStatus;

	UINT32					LurnId;
	LURN_TYPE				LurnType;
	PLURN_INTERFACE			LurnInterface;

	UINT64					StartBlockAddr;
	UINT64					EndBlockAddr;
	UINT64					UnitBlocks;

	ACCESS_MASK				AccessRight;

	LONG					NrOfStall;

	//
	//	Number of reconnection trial
	//	100-nanosecond time of reconnection interval
	//
	LONG					ReconnTrial;
	INT64					ReconnInterval;

	//
	//	extension for LU relation node
	//
	union{
		PVOID				LurnExtension;
		PRAID_INFO			LurnRAIDInfo;
	};

	//
	//	LU relation
	//
	PLURELATION				Lur;
	PLURELATION_NODE		LurnParent;
	PLURELATION_NODE		LurnSibling;
	ULONG					LurnChildrenCnt;
	PLURELATION_NODE		LurnChildren[1];

} LURELATION_NODE, *PLURELATION_NODE;



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
#define LURFLAG_FAKEWRITE		0x00000001
#define	LURFLAG_WRITESHARE_PS	0x00000002	// Write-sharing in Primary-Secondary mode.
#define LURFLAG_NOT_REGISTER	0x00000004


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
	  PLURELATION_DESC		LurDesc,
	  UINT32				DefaultLurFlags,
	  PLURELATION			*Lur,
	  PVOID					AdapterFdo,
	  LURN_EVENT_CALLBACK	LurnEventCallback
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
	   ULONG					MaxBlocksPerRequest,
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

#endif // LANSCSI_LURN_H