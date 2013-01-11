#ifndef LANSCSI_CCB_H
#define LANSCSI_CCB_H

#include <scsi.h>
#include <ntddscsi.h>

#define CCB_POOL_TAG			'ccSL'

//
// CCB Status
//
#define CCB_STATUS_SUCCESS					0x00	// CCB completed without error
#define CCB_STATUS_BUSY						0x14	// busy.
#define CCB_STATUS_STOP						0x23	// device stopped.
#define CCB_STATUS_UNKNOWN_STATUS			0x02
#define CCB_STATUS_NOT_EXIST				0x10    // No Target or Connection is disconnected.
#define CCB_STATUS_INVALID_COMMAND			0x11
#define CCB_STATUS_COMMAND_FAILED			0x12
#define	CCB_STATUS_RESET					0x13
#define	CCB_STATUS_COMMUNICATION_ERROR		0x21
#define CCB_STATUS_INVALID_REQUEST			0x22

//
//	CCB Opcode
//
#define CCB_OPCODE_FROM_SRBOPCODE(SRBOPCODE)	(SRBOPCODE)	
#define CCB_OPCODE_CONTROL_BIT					0x00010000

#define CCB_OPCODE_RESETBUS				CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_RESET_BUS)
#define CCB_OPCODE_ABORT_COMMAND		CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_ABORT_COMMAND)
#define CCB_OPCODE_EXECUTE				CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_EXECUTE_SCSI)
#define CCB_OPCODE_STOP					(CCB_OPCODE_CONTROL_BIT|0x1)
#define CCB_OPCODE_RESTART				(CCB_OPCODE_CONTROL_BIT|0x2)
#define CCB_OPCODE_QUERY				(CCB_OPCODE_CONTROL_BIT|0x3)
#define CCB_OPCODE_UPDATE				(CCB_OPCODE_CONTROL_BIT|0x4)

//
//	CCB Flags
//
#define CCB_FLAG_SYNCHRONOUS				0x00000001
#define CCB_FLAG_PENDING					0x00000002
#define CCB_FLAG_COMPLETED					0x00000004
#define CCB_FLAG_ASSOCIATE					0x00000008
#define CCB_FLAG_ALLOCATED					0x00000010
#define CCB_FLAG_DATABUF_ALLOCATED			0x00000020
#define CCB_FLAG_URGENT						0x00000040
#define CCB_FLAG_LOWER_LURN					0x00000080

#define CCBSTATUS_FLAG_TIMER_COMPLETE		0x00000001
#define CCBSTATUS_FLAG_RECONNECTING			0x00000002
#define CCBSTATUS_FLAG_BUSRESET_REQUIRED	0x00000004
#define CCBSTATUS_FLAG_LURN_IN_ERROR		0x00000008
#define CCBSTATUS_FLAG_LURN_STOP			0x00000010

#define NR_MAX_CCB_STACKLOCATION		4

#define LURID_LENGTH					8
#define LURID_PATHID					0
#define LURID_TARGETID					1
#define LURID_LUN						2

//
//	CCB stacklocation
//
typedef struct _CCB CCB ;
typedef NTSTATUS (*CCB_COMPLETION_ROUTINE)(PCCB, PVOID);

typedef struct _CCB_STACKLOCATION {
	//
	//	completion
	//
	PVOID	Lurn;
	CCB_COMPLETION_ROUTINE	CcbCompletionRoutine;
	PVOID	CcbCompletionContext;

} CCB_STACKLOCATION, *PCCB_STACKLOCATION ;

//
//
//	CCB structure
//
//	For SCSI commands:	
//				LurId[0] = PathId
//				LurId[1] = TargetId
//				LurId[2] = Lun
//
typedef struct _CCB CCB, *PCCB;

typedef struct _CCB {
	UINT16				Type;
	UINT16				Length;

	UINT32				OperationCode;
	ULONG				CcbStatusFlags;
	UCHAR				CdbLength;
	UCHAR				CcbStatus;
	UCHAR				LurId[LURID_LENGTH];
	ULONG				DataBufferLength;
	PVOID				DataBuffer;
	UCHAR				Cdb[MAXIMUM_CDB_SIZE];

	ULONG				Flags;
	LIST_ENTRY			ListEntry;
	PVOID				Srb;
	PVOID				AbortSrb;
	PCCB				AbortCcb;

	ULONG				SenseDataLength;
	PVOID				SenseBuffer;
	ULONG				ResidualDataLength;

	PVOID				HwDeviceExtension;

	PKEVENT				CompletionEvent ;

	//
	//	For associate CCB
	//
	KSPIN_LOCK			CcbSpinLock;	// protect CcbStatus, CcbStatusFlags when it has associate CCBs.
	LONG				AssociateCount;
	USHORT				AssociateID;

	//
	//	Stack locations
	//
	CHAR				CcbCurrentStackLocationIndex;
	PCCB_STACKLOCATION	CcbCurrentStackLocation;
	CCB_STACKLOCATION	CcbStackLocation[NR_MAX_CCB_STACKLOCATION];

} CCB, *PCCB;

//
//	CCB IO Control header
//
typedef SRB_IO_CONTROL CCB_IO_CONTROL, *PCCB_IO_CONTROL;


//////////////////////////////////////////////////////////////////////////
//
//	Exported functions
//


#define LSCcbMarkCcbAsPending(CCB_TOBE_PENDING) {												\
			ASSERT(!((CCB_TOBE_PENDING)->Flags & CCB_FLAG_PENDING)) ;							\
			ASSERT(!((CCB_TOBE_PENDING)->Flags & CCB_FLAG_SYNCHRONOUS)) ;						\
			(CCB_TOBE_PENDING)->Flags |= CCB_FLAG_PENDING ; }

#define LSCcbIsFlagOn(CCB_POINTER, FLAG) ( ((CCB_POINTER)->Flags & (FLAG)) != 0)
#define LSCcbSetFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->Flags |= (FLAG))
#define LSCcbResetFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->Flags &= ~(FLAG))

#define LSCcbIsStatusFlagOn(CCB_POINTER, FLAG) ( ((CCB_POINTER)->CcbStatusFlags & (FLAG)) != 0)
#define LSCcbSetStatusFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->CcbStatusFlags |= (FLAG))
#define LSCcbResetStatusFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->CcbStatusFlags &= ~(FLAG))

#define LSCcbSetCompletionRoutine(CCB_POINTER, COMPLETIONROUTINE, COMPLETIONCONTEXT) {			\
		ASSERT(!((CCB_POINTER)->CcbCurrentStackLocation->CcbCompletionRoutine)) ;				\
		(CCB_POINTER)->CcbCurrentStackLocation->CcbCompletionRoutine =  COMPLETIONROUTINE ;		\
		(CCB_POINTER)->CcbCurrentStackLocation->CcbCompletionContext =	COMPLETIONCONTEXT ;		\
	}

#define LSCcbSetNextStackLocation(CCB_POINTER) {												\
		ASSERT((CCB_POINTER)->CcbCurrentStackLocationIndex >= 1) ;								\
		(CCB_POINTER)->CcbCurrentStackLocationIndex -- ;										\
		(CCB_POINTER)->CcbCurrentStackLocation -- ;												\
	}

#define LSCcbSetAssociateCount(CCB_POINTER, ASSOCIATECOUNT) {									\
		(CCB_POINTER)->AssociateCount = (ASSOCIATECOUNT); }

#define LSCcbSetStatusSynch(CCB_POINTER, CCBSTATUS) {											\
		KIRQL	oldIrql;																		\
		KeAcquireSpinLock(&(CCB_POINTER)->CcbSpinLock, &oldIrql);								\
		(CCB_POINTER)->CcbStatus = (CCBSTATUS);													\
		KeReleaseSpinLock(&(CCB_POINTER)->CcbSpinLock, oldIrql);								\
}

#define LSCcbSetStatus(CCB_POINTER, CCBSTATUS) {												\
		(CCB_POINTER)->CcbStatus = (CCBSTATUS);													\
}

NTSTATUS
LSCcbStartup();

VOID
LSCcbShutdown();

NTSTATUS
LSCcbAllocate(
		PCCB *Ccb
	);

VOID
LSCcbFree(
		PCCB Ccb
	);

VOID
LSCcbCompleteCcb(
		IN PCCB Ccb
	);

VOID
LSCcbPostCompleteCcb(
		IN PCCB Ccb
	);	
/*
NTSTATUS
LSCcbInitializeInSrb(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		OUT PCCB						*Ccb
	);
*/
VOID
LSCcbInitialize(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		OUT PCCB						Ccb
	);

VOID
LSCcbInitializeByCcb(
		IN PCCB							OriCcb,
		OUT PCCB						Ccb
	) ;

#define LSCCB_INITIALIZE(CCB_POINTER) {																		\
	RtlZeroMemory((CCB_POINTER), sizeof(CCB));																\
	(CCB_POINTER)->Type							= LSSTRUC_TYPE_CCB;											\
	(CCB_POINTER)->Length						= sizeof(CCB);												\
	(CCB_POINTER)->CcbCurrentStackLocationIndex = NR_MAX_CCB_STACKLOCATION - 1;								\
	(CCB_POINTER)->CcbCurrentStackLocation		= (CCB_POINTER)->CcbStackLocation + (NR_MAX_CCB_STACKLOCATION - 1);	\
	InitializeListHead(&(CCB_POINTER)->ListEntry);															\
	KeInitializeSpinLock(&(CCB_POINTER)->CcbSpinLock);														\
}

#endif