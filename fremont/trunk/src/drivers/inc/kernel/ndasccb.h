#ifndef LANSCSI_CCB_H
#define LANSCSI_CCB_H


#ifndef NDAS_ASSERT
#define NDAS_ASSERT(EXP)
#endif

C_ASSERT( sizeof(CDB) == 16 );

//#include "winscsiext.h"

#define CCB_POOL_TAG				'ccSL'
#define CCB_CUSTOM_BUFFER_POOL_TAG	'acSL'
#define CCB_SENSE_DATA_TAG			'scSL'

//
// CCB Status
//

#define CCB_STATUS_SUCCESS					0x00	// CCB completed without error
#define CCB_STATUS_BUSY						0x05	// busy.
#define CCB_STATUS_STOP						0x23	// device stopped.
#define CCB_STATUS_UNKNOWN_STATUS			0x02
#define CCB_STATUS_NOT_EXIST				0x10    // No Target or Connection is disconnected.
#define CCB_STATUS_INVALID_COMMAND			0x11	// Invalid command, out of range, unknown command
#define CCB_STATUS_DATA_OVERRUN				0x12
#define CCB_STATUS_COMMAND_FAILED			0x13
#define	CCB_STATUS_RESET					0x14
#define CCB_STATUS_BAD_SECTOR				0x15
#define CCB_STATUS_BAD_DISK					0x16
#define	CCB_STATUS_COMMUNICATION_ERROR		0x21
#define CCB_STATUS_COMMMAND_DONE_SENSE		0x22
#define CCB_STATUS_COMMMAND_DONE_SENSE2		0x24
#define CCB_STATUS_NO_ACCESS				0x25	// No access right to execute this ccb operation.
#define CCB_STATUS_LOST_LOCK				0x30
#define CCB_STATUS_LOCK_NOT_GRANTED			0x31

//
//	CCB status flags
//

#define CCBSTATUS_FLAG_ASSIGNMASK			0x0000FFFF

#define CCBSTATUS_FLAG_TIMER_COMPLETE		0x00000001
#define CCBSTATUS_FLAG_RECONNECTING			0x00000002
#define CCBSTATUS_FLAG_BUSCHANGE			0x00000004
#define CCBSTATUS_FLAG_LURN_IN_ERROR		0x00000008
#define CCBSTATUS_FLAG_LURN_STOP			0x00000010
#define CCBSTATUS_FLAG_LONG_COMM_TIMEOUT	0x00000020

#define CCBSTATUS_FLAG_RAID_FLAG_VALID 		0x00000080 // Only when this flag is on, CCBSTATUS_FLAG_RAID_* is valid.
														// This is used because CCB may be handled without sending request to RAID system
														// We need to use another method to notify RAID status change.
#define CCBSTATUS_FLAG_RAID_FAILURE			0x00000100
#define CCBSTATUS_FLAG_RAID_NORMAL			0x00000200
#define CCBSTATUS_FLAG_RAID_DEGRADED		0x00000400	// Member of fault-tolerant RAID is stopped.
#define CCBSTATUS_FLAG_RAID_RECOVERING		0x00000800

//
// Stop reason status flags
//

#define CCBSTATUS_FLAG_RECONNECT_REACH_MAX	0x00001000	// Reconnection trial reached maximum.
#define CCBSTATUS_FLAG_POWERRECYLE_OCCUR	0x00002000	// Power recycle occurs on the NDAS device
														// when write data might still stay in the disk cache.
#define CCBSTATUS_FLAG_DISK_REPLACED		0x00004000	// Disk is replaced.
#define CCBSTATUS_FLAG_RECONNECT_NOTALLOWED	0x00008000

//
//	Only set in LsCcbMarkCcbAsPending()
//	Do not set this flag in other functions
//
#define CCBSTATUS_FLAG_PENDING				0x00010000
#define CCBSTATUS_FLAG_COMPLETED			0x00080000


//
//	CCB Opcode
//

#define CCB_OPCODE_FROM_SRBOPCODE(SRBOPCODE)	(SRBOPCODE)	
#define CCB_OPCODE_CONTROL_BIT					0x00010000

#define CCB_OPCODE_RESETBUS				CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_RESET_BUS)
#define CCB_OPCODE_ABORT_COMMAND		CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_ABORT_COMMAND)
#define CCB_OPCODE_EXECUTE				CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_EXECUTE_SCSI)
#define CCB_OPCODE_SHUTDOWN				CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_SHUTDOWN)
#define CCB_OPCODE_FLUSH				CCB_OPCODE_FROM_SRBOPCODE(SRB_FUNCTION_FLUSH)
#define CCB_OPCODE_STOP					(CCB_OPCODE_CONTROL_BIT|0x1)
#define CCB_OPCODE_RESTART				(CCB_OPCODE_CONTROL_BIT|0x2)
#define CCB_OPCODE_QUERY				(CCB_OPCODE_CONTROL_BIT|0x3)
#define CCB_OPCODE_UPDATE				(CCB_OPCODE_CONTROL_BIT|0x4)
#define CCB_OPCODE_NOOP					(CCB_OPCODE_CONTROL_BIT|0x5)
#define CCB_OPCODE_DVD_STATUS			(CCB_OPCODE_CONTROL_BIT|0x6)
#define CCB_OPCODE_SETBUSY				(CCB_OPCODE_CONTROL_BIT|0x7)
#define CCB_OPCODE_SMART				(CCB_OPCODE_CONTROL_BIT|0xa)
#define CCB_OPCODE_DEVLOCK				(CCB_OPCODE_CONTROL_BIT|0xb)


//	CCB Flags

#define CCB_FLAG_ALLOCATED					0x00000001 // don't set internel flag
#define CCB_FLAG_SENSEBUF_ALLOCATED			0x00000002 // don't set internel flag

#define CCB_FLAG_DATABUF_ALLOCATED			0x00000010

#define CCB_FLAG_SYNCHRONOUS				0x00000100
#define CCB_FLAG_MUST_SUCCEED				0x00000200
#define CCB_FLAG_URGENT						0x00000400
#define CCB_FLAG_DONOT_PASSDOWN				0x00000800
#define CCB_FLAG_RETRY_NOT_ALLOWED			0x00001000
#define CCB_FLAG_ACQUIRE_BUFLOCK			0x00002000
#define CCB_FLAG_WRITE_CHECK				0x00004000
#define CCB_FLAG_W2K_READONLY_PATCH			0x00008000
#define CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS	0x00010000

#define CCB_FLAG_BACKWARD_ADDRESS			0x01000000
#define CCB_FLAG_CALLED_INTERNEL			0x02000000

//
//	CCB structure
//
//	For SCSI commands:	
//				LurId[0] = PathId
//				LurId[1] = TargetId
//				LurId[2] = Lun
//

#if MAXIMUM_CDB_SIZE < 16
#undef MAXIMUM_CDB_SIZE
#define MAXIMUM_CDB_SIZE 16
#endif

C_ASSERT( sizeof(CDB) == 16 );

struct _CCB;

typedef NTSTATUS (*CCB_COMPLETION_ROUTINE)(struct _CCB *, PVOID);

typedef struct _CCB {

	UINT16		Type;					
	UINT16		Length;

	ULONG		Flags;

	KSPIN_LOCK	CcbSpinLock;

	UCHAR		LurId[LURID_LENGTH];	
	UINT32		OperationCode;

	UCHAR		CdbLength;

	union {

		struct {

			ULONG	StatusFlag0		 : 8;
			ULONG	NdasrStatusFlag8 : 4;
			ULONG	StatusFlag16	 : 20;
		};

		ULONG		CcbStatusFlags;			// Operation Result
	};

	UCHAR			CcbStatus;				// Operation Result

	union	{
	
		UCHAR	Cdb[sizeof(CDB)];	
		CDB		SrbCdb;
	};

	// Support for CD/DVD some 10byte command 
	// needs to change to 12byte command

	union {

		UCHAR	PKCMD[sizeof(CDB)];
		CDB		PacketCdb;
	};

	ULONG		DataBufferLength;		
	PVOID		DataBuffer;

	//	Volatile buffer

	PVOID		SecondaryBuffer;
	ULONG		SecondaryBufferLength;	

	LIST_ENTRY	ListEntry;	
	LIST_ENTRY	ListEntryForMiniPort;

	PVOID		Srb;					
	PVOID		AbortSrb;
	struct _CCB	*AbortCcb;				

	ULONG		SenseDataLength;
	PVOID		SenseBuffer;			
	ULONG		ResidualDataLength;

	PVOID		HwDeviceExtension;		
	ULONG		Reserved2;

	// For associate CCB
	// protect CcbStatus, CcbStatusFlags when it has associate CCBs.


	CCB_COMPLETION_ROUTINE	CcbCompletionRoutine;
	PVOID					CcbCompletionContext;		

} CCB, *PCCB;

//////////////////////////////////////////////////////////////////////////
//
//	Exported functions
//

#ifndef LSCCB_INLINE
#define LSCCB_INLINE __forceinline
#endif

static
LSCCB_INLINE 
VOID 
LsCcbMarkCcbAsPending(PCCB Ccb)
{
	NDAS_ASSERT( !FlagOn(Ccb->CcbStatusFlags, CCBSTATUS_FLAG_PENDING) );
	NDAS_ASSERT( !FlagOn(Ccb->Flags, CCB_FLAG_SYNCHRONOUS) );
	SetFlag( Ccb->CcbStatusFlags, CCBSTATUS_FLAG_PENDING );
}

static
LSCCB_INLINE
BOOLEAN 
LsCcbIsFlagOn(PCCB Ccb, ULONG Flags)
{ 
	return (Ccb->Flags & (Flags)) != 0; 
}

static
LSCCB_INLINE
VOID 
LsCcbSetFlag(PCCB Ccb, ULONG Flags) 
{ 
	Ccb->Flags |= Flags; 
}

static
LSCCB_INLINE
VOID 
LsCcbResetFlag(PCCB Ccb, ULONG Flags) 
{ 
	Ccb->Flags &= ~Flags; 
}

static
LSCCB_INLINE
BOOLEAN 
LsCcbIsStatusFlagOn(PCCB Ccb, ULONG Flags)
{ 
	return (Ccb->CcbStatusFlags & Flags) != 0; 
}

static
LSCCB_INLINE
VOID 
LsCcbSetStatusFlag(PCCB Ccb, ULONG Flags)
{ 
	Ccb->CcbStatusFlags |= Flags; 
}

static
LSCCB_INLINE
VOID 
LsCcbResetStatusFlag(PCCB Ccb, ULONG Flag)
{ 
	Ccb->CcbStatusFlags &= ~(Flag); 
}

static
LSCCB_INLINE
VOID
LsCcbSetCompletionRoutine (
	PCCB					Ccb, 
	CCB_COMPLETION_ROUTINE	CompletionRoutine,
	PVOID					CompletionContext
	)
{
	NDAS_ASSERT( Ccb->CcbCompletionRoutine == NULL );

	Ccb->CcbCompletionRoutine = CompletionRoutine;
	Ccb->CcbCompletionContext = CompletionContext;
}

static
LSCCB_INLINE
VOID
LsCcbSetStatus(
	PCCB Ccb,
	UCHAR CcbStatus
	)
{
	Ccb->CcbStatus = CcbStatus;
}

//	CDB utilities

static
LSCCB_INLINE 
VOID
LsCcbGetAddressAndLength (
	PCDB	Cdb, 
	PUINT64 LogicalBlockAddress, 
	PUINT32 TransferLength
	)
{
	if (Cdb->AsByte[0] == SCSIOP_READ16					||
		Cdb->AsByte[0] == SCSIOP_WRITE16				||
		Cdb->AsByte[0] == SCSIOP_VERIFY16				||
		Cdb->AsByte[0] == SCSIOP_SYNCHRONIZE_CACHE16	||
		Cdb->AsByte[0] == SCSIOP_READ_CAPACITY16) {

		if (LogicalBlockAddress) {

			*LogicalBlockAddress  = (UINT64)(Cdb->CDB16.LogicalBlock[0]) << 56;
			*LogicalBlockAddress |= (UINT64)(Cdb->CDB16.LogicalBlock[1]) << 48;
			*LogicalBlockAddress |= (UINT64)(Cdb->CDB16.LogicalBlock[2]) << 40;
			*LogicalBlockAddress |= (UINT64)(Cdb->CDB16.LogicalBlock[3]) << 32;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB16.LogicalBlock[4]	 << 24;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB16.LogicalBlock[5]	 << 16;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB16.LogicalBlock[6]	 << 8;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB16.LogicalBlock[7];
		}
	
		if (TransferLength) {

			*TransferLength  = (UINT32)Cdb->CDB16.TransferLength[0] << 24;
			*TransferLength |= (UINT32)Cdb->CDB16.TransferLength[1] << 16;
			*TransferLength |= (UINT32)Cdb->CDB16.TransferLength[2] << 8;
			*TransferLength |= (UINT32)Cdb->CDB16.TransferLength[3] << 0;
		}

	} else {

		if (LogicalBlockAddress) {

			*LogicalBlockAddress  = (UINT64)Cdb->CDB10.LogicalBlockByte0 << 24;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB10.LogicalBlockByte1 << 16;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB10.LogicalBlockByte2 << 8;
			*LogicalBlockAddress |= (UINT64)Cdb->CDB10.LogicalBlockByte3;
		}
	
		if (TransferLength) {

			*TransferLength  = (UINT32)Cdb->CDB10.TransferBlocksMsb << 8;
			*TransferLength |= (UINT32)Cdb->CDB10.TransferBlocksLsb;
		}
	}
}

static
LSCCB_INLINE 
VOID 
LsCcbSetLogicalAddress ( 
	PCDB	Cdb, 
	UINT64	LogicalBlockAddress
	) 
{
	if (Cdb->AsByte[0] == SCSIOP_READ16					||
		Cdb->AsByte[0] == SCSIOP_WRITE16				||
		Cdb->AsByte[0] == SCSIOP_VERIFY16				||
		Cdb->AsByte[0] == SCSIOP_SYNCHRONIZE_CACHE16	||
		Cdb->AsByte[0] == SCSIOP_READ_CAPACITY16) {

		Cdb->CDB16.LogicalBlock[0] = (UCHAR)(LogicalBlockAddress >> 56);
		Cdb->CDB16.LogicalBlock[1] = (UCHAR)(LogicalBlockAddress >> 48);
		Cdb->CDB16.LogicalBlock[2] = (UCHAR)(LogicalBlockAddress >> 40);
		Cdb->CDB16.LogicalBlock[3] = (UCHAR)(LogicalBlockAddress >> 32);
		Cdb->CDB16.LogicalBlock[4] = (UCHAR)(LogicalBlockAddress >> 24);
		Cdb->CDB16.LogicalBlock[5] = (UCHAR)(LogicalBlockAddress >> 16);
		Cdb->CDB16.LogicalBlock[6] = (UCHAR)(LogicalBlockAddress >> 8);
		Cdb->CDB16.LogicalBlock[7] = (UCHAR)(LogicalBlockAddress);
	
	} else {

		Cdb->CDB10.LogicalBlockByte0 = (UCHAR)(LogicalBlockAddress >> 24);
		Cdb->CDB10.LogicalBlockByte1 = (UCHAR)(LogicalBlockAddress >> 16);
		Cdb->CDB10.LogicalBlockByte2 = (UCHAR)(LogicalBlockAddress >> 8);
		Cdb->CDB10.LogicalBlockByte3 = (UCHAR)(LogicalBlockAddress);
	}

	return;
}

static
LSCCB_INLINE 
VOID 
LsCcbSetTransferLength (
	PCDB   Cdb, 
	UINT32 TransferLength
	) 
{
	if (Cdb->AsByte[0] == SCSIOP_READ16					||
		Cdb->AsByte[0] == SCSIOP_WRITE16				||
		Cdb->AsByte[0] == SCSIOP_VERIFY16				||
		Cdb->AsByte[0] == SCSIOP_SYNCHRONIZE_CACHE16	||
		Cdb->AsByte[0] == SCSIOP_READ_CAPACITY16) {

		Cdb->CDB16.TransferLength[0] = (UCHAR)(TransferLength >> 24);
		Cdb->CDB16.TransferLength[1] = (UCHAR)(TransferLength >> 16);
		Cdb->CDB16.TransferLength[2] = (UCHAR)(TransferLength >> 8);
		Cdb->CDB16.TransferLength[3] = (UCHAR)(TransferLength);
	
	} else {

		Cdb->CDB10.TransferBlocksMsb = (UCHAR)(TransferLength >> 8);
		Cdb->CDB10.TransferBlocksLsb = (UCHAR)(TransferLength);
	}

	return;
}

static
LSCCB_INLINE
NTSTATUS
LsCcbAllocate (
	PCCB *Ccb
	) 
{
	*Ccb = ExAllocatePoolWithTag( NonPagedPool, sizeof(CCB), CCB_POOL_TAG );

	if (*Ccb == NULL) {
		
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( *Ccb, sizeof(CCB) );

	SetFlag( (*Ccb)->Flags, CCB_FLAG_ALLOCATED );

	return STATUS_SUCCESS;
}

static
LSCCB_INLINE
VOID
LsCcbCleanUp (
	PCCB Ccb
	) 
{
	DebugTrace( NDASSCSI_DBG_CCB_TRACE, ("entered with Ccb:%p\n", Ccb) );

	if (FlagOn(Ccb->Flags, CCB_FLAG_DATABUF_ALLOCATED)) {

		if (Ccb->DataBuffer) {

			ExFreePool( Ccb->DataBuffer );
		
		} else {

			NDAS_ASSERT(FALSE);
		}	

		ClearFlag( Ccb->Flags, CCB_FLAG_DATABUF_ALLOCATED );
	}

	if (FlagOn(Ccb->Flags, CCB_FLAG_SENSEBUF_ALLOCATED)) {

		if (Ccb->SenseBuffer) {

			ExFreePool( Ccb->SenseBuffer );

		} else {

			NDAS_ASSERT(FALSE);
		}

		ClearFlag( Ccb->Flags, CCB_FLAG_SENSEBUF_ALLOCATED );
	}
}

static
LSCCB_INLINE
VOID
LsCcbFree (
	PCCB Ccb
	) 
{
	NDAS_ASSERT( FlagOn(Ccb->Flags, CCB_FLAG_ALLOCATED) );

	DebugTrace( NDASSCSI_DBG_CCB_TRACE, ("entered with Ccb:%p\n", Ccb) );

	LsCcbCleanUp(Ccb); 

	ExFreePoolWithTag( Ccb, CCB_POOL_TAG );
}

static
LSCCB_INLINE
VOID
LsCcbInitialize (
	IN  PSCSI_REQUEST_BLOCK	Srb,
	IN  PVOID				HwDeviceExtension,
	IN  BOOLEAN				SetZero,
	OUT PCCB				Ccb
	)
{
	NDAS_ASSERT( Ccb != NULL );

	if (SetZero) {

		RtlZeroMemory( Ccb, sizeof(CCB) );
	}

	Ccb->Type	= LSSTRUC_TYPE_CCB;
	Ccb->Length	= sizeof(CCB);
	Ccb->Srb	= Srb;

	KeInitializeSpinLock( &Ccb->CcbSpinLock );
	InitializeListHead( &Ccb->ListEntry );

	if (Srb) {

		NDAS_ASSERT( Srb->CdbLength <= MAXIMUM_CDB_SIZE );

	    Ccb->CdbLength					= (UCHAR)Srb->CdbLength;

		RtlCopyMemory(Ccb->Cdb, Srb->Cdb, Ccb->CdbLength);

		Ccb->OperationCode		= CCB_OPCODE_FROM_SRBOPCODE(Srb->Function);

	    Ccb->LurId[0]			= Srb->PathId;                   // offset 5
		Ccb->LurId[1]			= Srb->TargetId;                 // offset 6
	    Ccb->LurId[2]			= Srb->Lun;                      // offset 7
		Ccb->DataBufferLength	= Srb->DataTransferLength;       // offset 10
	    Ccb->DataBuffer			= Srb->DataBuffer;               // offset 18
		Ccb->AbortSrb			= Srb->NextSrb;
		Ccb->SenseBuffer		= Srb->SenseInfoBuffer;
		Ccb->SenseDataLength	= Srb->SenseInfoBufferLength;
	}

	Ccb->HwDeviceExtension			= HwDeviceExtension;
}

static
LSCCB_INLINE
NTSTATUS
LsCcbInitializeByCcb (
	IN  PCCB	OriCcb,
	OUT PCCB	Ccb
	)
{
	Ccb->Type	= OriCcb->Type;
	Ccb->Length = OriCcb->Length;

	SetFlag( Ccb->Flags, 
			 FlagOn( OriCcb->Flags, (CCB_FLAG_MUST_SUCCEED | CCB_FLAG_URGENT | CCB_FLAG_RETRY_NOT_ALLOWED |
									 CCB_FLAG_ACQUIRE_BUFLOCK | CCB_FLAG_WRITE_CHECK | 
									 CCB_FLAG_W2K_READONLY_PATCH | CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS)) );

	RtlCopyMemory( Ccb->LurId, OriCcb->LurId, sizeof(Ccb->LurId) );

	Ccb->OperationCode  = OriCcb->OperationCode;
	Ccb->CdbLength		= OriCcb->CdbLength;

	Ccb->SrbCdb		= OriCcb->SrbCdb;
	Ccb->PacketCdb	= OriCcb->PacketCdb;

	Ccb->DataBuffer			= OriCcb->DataBuffer;
	Ccb->DataBufferLength	= OriCcb->DataBufferLength;

	Ccb->SecondaryBuffer		= OriCcb->SecondaryBuffer;
	Ccb->SecondaryBufferLength	= OriCcb->SecondaryBufferLength;
	
	if (OriCcb->SenseBuffer) {

		Ccb->SenseBuffer = ExAllocatePoolWithTag( NonPagedPool,
												  OriCcb->SenseDataLength,
												  CCB_POOL_TAG );

		if (Ccb->SenseBuffer == NULL) {

			DebugTrace(NDASSCSI_DBG_LURN_IDE_ERROR, ("Sense buffer allocation failed.\n"));			
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory( Ccb->SenseBuffer, OriCcb->SenseDataLength );
		Ccb->SenseDataLength = OriCcb->SenseDataLength;
		SetFlag( Ccb->Flags, CCB_FLAG_SENSEBUF_ALLOCATED );
	}

	return STATUS_SUCCESS;
}

//	Complete a Ccb
//	We can set the CCBSTATUS_FLAG_PRE_COMPLETED flag only in this function

static
LSCCB_INLINE
VOID
LsCcbCompleteCcb (
	IN PCCB Ccb
	) 
{
	ASSERT( Ccb->Type == LSSTRUC_TYPE_CCB );
	ASSERT( Ccb->Length == sizeof(CCB) );
	
	NDAS_ASSERT( !FlagOn(Ccb->CcbStatusFlags, CCBSTATUS_FLAG_COMPLETED) );

	SetFlag( Ccb->CcbStatusFlags, CCBSTATUS_FLAG_COMPLETED );
	ClearFlag( Ccb->CcbStatusFlags, CCBSTATUS_FLAG_PENDING );

	if (Ccb->CcbCompletionRoutine) {

		NTSTATUS	status;

		status = Ccb->CcbCompletionRoutine( Ccb, Ccb->CcbCompletionContext );
		
		if (status == STATUS_MORE_PROCESSING_REQUIRED) {

			return;
		}
	}

	LsCcbFree(Ccb);
}

//	Complete all Ccb in a list.

static
LSCCB_INLINE
VOID
CcbCompleteList (
	PLIST_ENTRY	Head,
	CHAR		CcbStatus,
	USHORT		StatusFlags
) 
{
	PLIST_ENTRY	listEntry;
	PCCB		ccb;

	while (!IsListEmpty(Head)) {

		listEntry = RemoveHeadList(Head);
		
		ccb = CONTAINING_RECORD(listEntry, CCB, ListEntry);
		
		ccb->CcbStatus = CcbStatus;

		SetFlag( ccb->CcbStatusFlags, StatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

		LsCcbCompleteCcb(ccb);

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Completed Ccb:%p\n", ccb) );
	}
}

static
LSCCB_INLINE
VOID
LSCCB_SETSENSE (
	PCCB	Ccb,
	UCHAR	SenseKey,
	UCHAR	AdditionalSenseCode,
	UCHAR	AdditionalSenseCodeQualifier
	)
{
	if (Ccb->SenseBuffer) {

		PSENSE_DATA SenseBuffer = (PSENSE_DATA) Ccb->SenseBuffer;
		
		SenseBuffer->ErrorCode = 0x70;
		SenseBuffer->SenseKey = SenseKey;
		SenseBuffer->AdditionalSenseLength = 0x0a;
		SenseBuffer->AdditionalSenseCode = AdditionalSenseCode;
		SenseBuffer->AdditionalSenseCodeQualifier = AdditionalSenseCodeQualifier;
	}
}

#endif

