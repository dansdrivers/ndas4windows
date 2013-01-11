#ifndef LANSCSI_LURN_H
#define LANSCSI_LURN_H

#include "oem.h"
#include "lsbusioctl.h"
#include "LSTransport.h"
#include "LSProto.h"
#include "LSCcb.h"

//
//	Disk information for SCSIOP_INQUIRY
//
// OEM SUPPORT
//
#ifdef OEM_VENDOR_ID
#define VENDOR_ID OEM_VENDOR_ID
#else
#define VENDOR_ID RETAIL_VENDOR_ID
#endif

#define	PRODUCT_REVISION_LEVEL "1.0"

//
//	Pool tags for LUR
//
#define LUR_POOL_TAG			'rlSL'
#define LURN_POOL_TAG			'ulSL'
#define LURNEXT_POOL_TAG		'elSL'

//
//
//
#define BLOCK_SIZE_BITS		9
#define BLOCK_SIZE			(1<<BLOCK_SIZE_BITS)


//////////////////////////////////////////////////////////////////////////
//
//	LUR Ioctl
//

//
//	LUR Query
//
typedef enum _LUR_INFORMATION_CLASS {

	LurPrimaryLurnInformation=1

} LUR_INFORMATION_CLASS, *PLUR_INFORMATION_CLASS;

typedef struct _LUR_QUERY {

	UINT32					Length;
	LUR_INFORMATION_CLASS	InfoClass;
	UINT32					QueryDataLength;
	UCHAR					QueryData[1];

}	LUR_QUERY, * PLUR_QUERY;

#define SIZE_OF_LURQUERY(QUERYDATA_LEN)	(sizeof(LUR_QUERY) + (QUERYDATA_LEN) - 1)

//
//	Returned information
//
typedef struct _LURN_INFORMATION {

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

}	LURN_INFORMATION, *PLURN_INFORMATION ;


typedef struct _LURN_PRIMARYINFORMATION {

	UINT32					Length ;
	LURN_INFORMATION		PrimaryLurn ;

}	LURN_PRIMARYINFORMATION, *PLURN_PRIMARYINFORMATION ;

#define		LURN_UPDATECLASS_WRITEACCESS_USERID		0x0001

typedef struct _LURN_UPDATE {

	UINT16			UpdateClass;

}	LURN_UPDATE, *PLURN_UPDATE ;

//////////////////////////////////////////////////////////////////////////
//
//	LURN interface
//
#define LUR_MAX_LURNS_PER_LUR		128

typedef struct _LURELATION_NODE LURELATION_NODE, *PLURELATION_NODE;
typedef struct _LURELATION_NODE_DESC LURELATION_NODE_DESC, *PLURELATION_NODE_DESC;

#define LUR_GETROOTNODE(LUR_POINTER)	((LUR_POINTER)->LurRoot)

typedef NTSTATUS (*LURN_INITIALIZE)(PLURELATION_NODE Lurn, PLURELATION_NODE_DESC LurnDesc);
typedef NTSTATUS (*LURN_DESTROY)(PLURELATION_NODE Lurn) ;
typedef NTSTATUS (*LURN_REQUEST)(PLURELATION_NODE Lurn, PCCB Ccb) ;

typedef struct _LURN_FUNC {

	LURN_INITIALIZE	LurnInitialize ;
	LURN_DESTROY	LurnDestroy ;
	LURN_REQUEST	LurnRequest ;
	PVOID			Reserved[12] ;
	PVOID			LurnExtension ;

} LURN_FUNC, *PLURN_FUNC ;

#define 	LURN_AGGREGATION		0x0000
#define 	LURN_MIRRORING			0x0001
#define 	LURN_IDE_DISK			0x0002
//#define 	LURN_IDE_ODD			0x0003
//#define 	LURN_IDE_VODD			0x0004

typedef	UINT16 LURN_TYPE, *PLURN_TYPE ;

#define NR_LURN_INTERFACE			3
#define NR_LURN_MAX_INTERFACE		16

typedef struct _LURN_INTERFACE {

	UINT16			Type ;
	UINT16			Length ;
	LURN_TYPE		LurnType ;
	UINT32			LurnCharacteristic ;
	LURN_FUNC		LurnFunc ;

} LURN_INTERFACE, *PLURN_INTERFACE ;

extern PLURN_INTERFACE	LurnInterfaceList[NR_LURN_MAX_INTERFACE] ;

#define LURN_IS_RUNNING(LURNSTATUS)				(((LURNSTATUS) == LURN_STATUS_RUNNING) || ((LURNSTATUS) == LURN_STATUS_STALL))

#define LURN_STATUS_INIT							0x00000000
#define LURN_STATUS_RUNNING							0x00000001
#define LURN_STATUS_STALL							0x00000002
#define LURN_STATUS_STOP_PENDING					0x00000003
#define LURN_STATUS_STOP							0x00000004

typedef struct _PLURELATION LURELATION, *PLURELATION ;

typedef struct _LURELATION_NODE {
	UINT16					Length;
	UINT16					Type;

	//
	//	spinlock to protect LurnStatus
	//
	KSPIN_LOCK				LurnSpinLock;

	ULONG					LurnStatus;

	LURN_TYPE				LurnType ;
	PLURN_INTERFACE			LurnInteface;

	ULONG					StartBlockAddr;
	ULONG					EndBlockAddr;
	UINT64					UnitBlocks;

	ACCESS_MASK				AccessRight;

	LONG					NrOfStall;

	//
	//	extension for LU relation node
	//
	PVOID					LurnExtension;

	//
	//	LU relation
	//
	PLURELATION				Lur;
	PLURELATION_NODE		LurnParent;
	PLURELATION_NODE		LurnSibling;
	LONG					LurnChildrenCnt;
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
#define LUR_FLAG_FAKEWRITE	0x00000001	// Fake if OS is W2k...

typedef struct _PLURELATION {

	UINT16					Length;
	UINT16					Type;
	UCHAR					LurId[LURID_LENGTH];
	UINT32					SlotNo;
	ULONG					MaxBlocksPerRequest;
	UINT32					LurFlags;
	ACCESS_MASK				DesiredAccess;
	ACCESS_MASK				GrantedAccess;
	PLURELATION_NODE		LurRoot;

} LURELATION, *PLURELATION ;


//////////////////////////////////////////////////////////////////////////
//
//	LUR descriptor
//
#define LURNDESC_INITDATA_BUFFERLEN		80

typedef struct _LURNDESC_IDE {

	UINT32					UserID;
	UINT64					Password;
	TA_LSTRANS_ADDRESS		TargetAddress;	// sizeof(TA_LSTRANS_ADDRESS) == 26 bytes
	TA_LSTRANS_ADDRESS		BindingAddress;
	UCHAR					HWType;
	UCHAR					HWVersion;
	UCHAR					LanscsiTargetID;
	UCHAR					LanscsiLU;

} LURNDESC_IDE, *PLURNDESC_IDE;

typedef struct _LURELATION_NODE_DESC {
	UINT16					NextOffset;

	LURN_TYPE				LurnType;
	ULONG					StartBlockAddr;
	ULONG					EndBlockAddr;
	UINT64					UnitBlocks;
	ULONG					MaxBlocksPerRequest;
	ACCESS_MASK				AccessRight;
	union {
		BYTE				InitData[LURNDESC_INITDATA_BUFFERLEN];
		LURNDESC_IDE		LurnIde;
	};

	LONG					LurnParent;
	LONG					LurnChildrenCnt;
	LONG					LurnChildren[1];

} LURELATION_NODE_DESC, *PLURELATION_NODE_DESC;


typedef struct _PLURELATION_DESC {

	UINT16					Length;
	UINT16					Type;
	UCHAR					LurId[LURID_LENGTH];
	UINT32					SlotNo;
	ULONG					MaxBlocksPerRequest;
	UINT32					LurFlags;
	ACCESS_MASK				AccessRight;
	LONG					LurnDescCount;
	LURELATION_NODE_DESC	LurnDesc[1];

} LURELATION_DESC, *PLURELATION_DESC ;

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
		PLURELATION_DESC	LurDesc,
		PLURELATION			*Lur
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

#endif // LANSCSI_LURN_H