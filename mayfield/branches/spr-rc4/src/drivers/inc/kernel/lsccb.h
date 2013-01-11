#ifndef LANSCSI_CCB_H
#define LANSCSI_CCB_H

#include <scsi.h>
#include <ntddscsi.h>
#include "lurdesc.h"

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
#define CCB_STATUS_COMMMAND_DONE_SENSE		0x22
#define CCB_STATUS_COMMMAND_DONE_SENSE2		0x24


//
//	CCB status flags
//

#define CCBSTATUS_FLAG_ASSIGNMASK			0x0000FFFF

#define CCBSTATUS_FLAG_TIMER_COMPLETE		0x00000001
#define CCBSTATUS_FLAG_RECONNECTING			0x00000002
#define CCBSTATUS_FLAG_BUSRESET_REQUIRED	0x00000004
#define CCBSTATUS_FLAG_LURN_IN_ERROR		0x00000008
#define CCBSTATUS_FLAG_LURN_STOP			0x00000010
#define CCBSTATUS_FLAG_LURN_STOP_INDICATE	0x00000020
#define CCBSTATUS_FLAG_RECOVERING			0x00000040

//
//	Only set in LSCcbMarkCcbAsPending()
//	Do not set this flag in other functions
//
#define CCBSTATUS_FLAG_PENDING				0x00010000

//
//	Only set in LSCcbCompleteCcb()
//	Do not set this flag in other functions
//
#define CCBSTATUS_FLAG_COMPLETED			0x00020000


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
#define CCB_OPCODE_NOOP					(CCB_OPCODE_CONTROL_BIT|0x5)
#define CCB_OPCODE_DVD_STATUS			(CCB_OPCODE_CONTROL_BIT|0x6)
#define CCB_OPCODE_SETBUSY				(CCB_OPCODE_CONTROL_BIT|0x7)


//
//	CCB Flags
//
#define CCB_FLAG_SYNCHRONOUS				0x00000001
#define CCB_FLAG_ASSOCIATE					0x00000008
#define CCB_FLAG_ALLOCATED					0x00000010
#define CCB_FLAG_DATABUF_ALLOCATED			0x00000020
#define CCB_FLAG_URGENT						0x00000040
#define CCB_FLAG_LOWER_LURN					0x00000080
#define CCB_FLAG_RETRY_NOT_ALLOWED			0x00000100
#define	CCB_FLAG_VOLATILE_PRIMARY_BUFFER	0x00000200
#define CCB_FLAG_VOLATILE_SECONDARY_BUFFER	0x00000400
#define CCB_FLAG_EXTENSION					0x00000800

#define NR_MAX_CCB_STACKLOCATION		8

//////////////////////////////////////////////////////////////////////////
//
//	extended commands
//
#define		CCB_EXT_READ					0x01
#define		CCB_EXT_WRITE					0x02
#define		CCB_EXT_READ_OPERATE_WRITE		0x03

#define		EXT_BYTE_OPERATION_COPY				0x01
#define		EXT_BYTE_OPERATION_AND				0x02
#define		EXT_BYTE_OPERATION_OR				0x03
#define		EXT_BLOCK_OPERATION					0x11

#define		EXTENDED_COMMAND_POOL_TAG			'ETYB'

typedef struct _LURELATION_NODE LURELATION_NODE, *PLURELATION_NODE;
typedef struct _CMD_COMMON CMD_COMMON, *PCMD_COMMON;

typedef struct _CMD_COMMON {
	UCHAR		Operation;
	PCMD_COMMON	pNextCmd;
	PLURELATION_NODE	pLurnCreated;
} CMD_COMMON, *PCMD_COMMON;

typedef struct _CMD_BYTE_OP {
	UCHAR			Operation;
	PCMD_COMMON			pNextCmd;
	PLURELATION_NODE	pLurnCreated;
	BOOLEAN				CountBack; // count address backward from end
	ULONG			logicalBlockAddress;
	UINT16			Offset;
	union {
		UINT16			LengthByte;
		UINT16			LengthBlock;
	};
	UINT16			ByteOperation;
	PBYTE			pByteData;
} CMD_BYTE_OP, *PCMD_BYTE_OP;

//
//	CCB stack location
//
typedef struct _CCB CCB;
typedef NTSTATUS (*CCB_COMPLETION_ROUTINE)(PCCB, PVOID);

typedef struct _CCB_STACKLOCATION {
	//
	//	completion
	//
	PVOID					Lurn;							// 0
	CCB_COMPLETION_ROUTINE	CcbCompletionRoutine;
	PVOID					CcbCompletionContext;			// 8
	PVOID					CcbCompletionContextExt;

} CCB_STACKLOCATION, *PCCB_STACKLOCATION;

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
	UINT16				Type;					// 0
	UINT16				Length;

	UINT32				OperationCode;
	ULONG				CcbStatusFlags;			// 8
	UCHAR				CcbStatus;
	UCHAR				CdbLength;
	UCHAR				Reserved[2];

	UCHAR				LurId[LURID_LENGTH];	// 16

	ULONG				DataBufferLength;		// 24. In byte
	PVOID				DataBuffer;

	UCHAR				Cdb[MAXIMUM_CDB_SIZE];	// 32
	PCMD_COMMON			pExtendedCommand;		// Extended command, used for RAID 1 etc.

	// Support for CD/DVD some 10byte command 
	//			needs to change to 12byte command
	UCHAR				PKCMD[MAXIMUM_CDB_SIZE];// 48

	//
	//	Volatile buffer
	//
	PVOID				SecondaryBuffer;
	ULONG				SecondaryBufferLength;	// 64
	ULONG				Flags;

	LIST_ENTRY			ListEntry;				// 72

	PVOID				Srb;					// 80
	PVOID				AbortSrb;
	PCCB				AbortCcb;				// 88

	ULONG				SenseDataLength;
	PVOID				SenseBuffer;			// 96
	ULONG				ResidualDataLength;

	PVOID				HwDeviceExtension;		// 104
	ULONG				CcbSeqId;

	PKEVENT				CompletionEvent;		// 112

	//
	//	For associate CCB
	// protect CcbStatus, CcbStatusFlags when it has associate CCBs.
	//
	KSPIN_LOCK			CcbSpinLock;

	LONG				AssociateCount;			// 120
	USHORT				AssociateID;

	//	For cascaded process
	BOOLEAN				AssociateCascade;
	BOOLEAN				ForceFail;
	PBOOLEAN			ForceFailNext;
	KEVENT				EventCascade;
	PKEVENT				EventCascadeNext;

	//
	//	Stack locations
	//
	CHAR				CcbCurrentStackLocationIndex;
	CHAR				Reserved2;
	PCCB_STACKLOCATION	CcbCurrentStackLocation;
	CHAR				Reserved3[4];
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
			ASSERT(!((CCB_TOBE_PENDING)->CcbStatusFlags & CCBSTATUS_FLAG_PENDING));				\
			ASSERT(!((CCB_TOBE_PENDING)->Flags & CCB_FLAG_SYNCHRONOUS));						\
			(CCB_TOBE_PENDING)->CcbStatusFlags |= CCBSTATUS_FLAG_PENDING; }

#define LSCcbIsFlagOn(CCB_POINTER, FLAG) ( ((CCB_POINTER)->Flags & (FLAG)) != 0)
#define LSCcbSetFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->Flags |= (FLAG))
#define LSCcbResetFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->Flags &= ~(FLAG))

#define LSCcbIsStatusFlagOn(CCB_POINTER, FLAG) ( ((CCB_POINTER)->CcbStatusFlags & (FLAG)) != 0)
#define LSCcbSetStatusFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->CcbStatusFlags |= (FLAG))
#define LSCcbResetStatusFlag(CCB_POINTER, FLAG) ((CCB_POINTER)->CcbStatusFlags &= ~(FLAG))

#define LSCcbSetCompletionRoutine(CCB_POINTER, COMPLETIONROUTINE, COMPLETIONCONTEXT) {			\
		ASSERT(!((CCB_POINTER)->CcbCurrentStackLocation->CcbCompletionRoutine));				\
		(CCB_POINTER)->CcbCurrentStackLocation->CcbCompletionRoutine =  COMPLETIONROUTINE;		\
		(CCB_POINTER)->CcbCurrentStackLocation->CcbCompletionContext =	COMPLETIONCONTEXT;		\
	}

#define LSCcbSetNextStackLocation(CCB_POINTER) {												\
		ASSERT((CCB_POINTER)->CcbCurrentStackLocationIndex >= 1);								\
		(CCB_POINTER)->CcbCurrentStackLocationIndex --;											\
		(CCB_POINTER)->CcbCurrentStackLocation --;												\
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

#define LSCcbSetExtendedCommand(CCB_POINTER, EXTENDED_COMMAND) {								\
		(CCB_POINTER)->pExtendedCommand = (EXTENDED_COMMAND);									\
}

#define CDB10_LOGICAL_BLOCK_BYTE(pCdb)	\
			(	((UINT32)(((PCDB)(pCdb))->CDB10.LogicalBlockByte0) << (8 * 3)) +	\
				((UINT32)(((PCDB)(pCdb))->CDB10.LogicalBlockByte1) << (8 * 2)) +	\
				((UINT32)(((PCDB)(pCdb))->CDB10.LogicalBlockByte2) << (8 * 1)) +	\
				((UINT32)(((PCDB)(pCdb))->CDB10.LogicalBlockByte3) << (8 * 0))	)

#define CDB10_TRANSFER_BLOCKS(pCdb)	\
			(	((UINT32)(((PCDB)(pCdb))->CDB10.TransferBlocksMsb) << (8 * 1)) +	\
				((UINT32)(((PCDB)(pCdb))->CDB10.TransferBlocksLsb) << (8 * 0))	)

#define CDB10_LOGICAL_BLOCK_BYTE_TO_BYTES(pCdb, LogicalBlockAddress)	\
			{	(((PCDB)(pCdb))->CDB10.LogicalBlockByte0) = ((PFOUR_BYTE)&(LogicalBlockAddress))->Byte3;	\
				(((PCDB)(pCdb))->CDB10.LogicalBlockByte1) = ((PFOUR_BYTE)&(LogicalBlockAddress))->Byte2;	\
				(((PCDB)(pCdb))->CDB10.LogicalBlockByte2) = ((PFOUR_BYTE)&(LogicalBlockAddress))->Byte1;	\
				(((PCDB)(pCdb))->CDB10.LogicalBlockByte3) = ((PFOUR_BYTE)&(LogicalBlockAddress))->Byte0;	}	

#define CDB10_TRANSFER_BLOCKS_TO_BYTES(pCdb, TransferBlocks)	\
			{	(((PCDB)(pCdb))->CDB10.TransferBlocksMsb) = ((PTWO_BYTE)&(TransferBlocks))->Byte1;	\
				(((PCDB)(pCdb))->CDB10.TransferBlocksLsb) = ((PTWO_BYTE)&(TransferBlocks))->Byte0;	}

NTSTATUS
LSCcbRemoveExtendedCommandTailMatch(
								PCMD_COMMON *ppCmd, 
								PLURELATION_NODE pLurnCreated
								);

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

VOID
CcbCompleteList(
		PLIST_ENTRY	Head,
		CHAR		CcbStatus,
		USHORT		StatusFlags
	);

VOID
LSCcbInitializeInSrb(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		IN ULONG						CcbSeqId,
		OUT PCCB						*Ccb
	);

VOID
LSCcbInitialize(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		IN ULONG						CcbSeqId,
		OUT PCCB						Ccb
	);

VOID
LSCcbInitializeByCcb(
		IN PCCB							OriCcb,
		IN PVOID						pLurn,
		IN ULONG						CcbSeqId,
		OUT PCCB						Ccb
	);

#define LSCCB_INITIALIZE(CCB_POINTER, CCBSEQID) {															\
	RtlZeroMemory((CCB_POINTER), sizeof(CCB));																\
	(CCB_POINTER)->Type							= LSSTRUC_TYPE_CCB;											\
	(CCB_POINTER)->Length						= sizeof(CCB);												\
	(CCB_POINTER)->CcbCurrentStackLocationIndex = NR_MAX_CCB_STACKLOCATION - 1;								\
	(CCB_POINTER)->CcbCurrentStackLocation		= (CCB_POINTER)->CcbStackLocation + (NR_MAX_CCB_STACKLOCATION - 1);	\
	InitializeListHead(&(CCB_POINTER)->ListEntry);															\
	KeInitializeSpinLock(&(CCB_POINTER)->CcbSpinLock);														\
	(CCB_POINTER)->CcbSeqId = (CCBSEQID);																	\
}

#define LSCCB_SETSENSE(CCB_POINTER, SENSE_KEY, ADDI_SENSE, ADDI_SENSE_QUAL) {						\
			if(Ccb->SenseBuffer) {																	\
				((PSENSE_DATA)Ccb->SenseBuffer)->SenseKey = SENSE_KEY;								\
				((PSENSE_DATA)Ccb->SenseBuffer)->AdditionalSenseCode = ADDI_SENSE;					\
				((PSENSE_DATA)Ccb->SenseBuffer)->AdditionalSenseCodeQualifier = ADDI_SENSE_QUAL;	\
			} }


#endif