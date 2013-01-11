#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LsCcb"


NTSTATUS
LsCcbAllocate(
		PCCB *Ccb
	) {

	*Ccb = ExAllocatePoolWithTag(
						NonPagedPool,
						sizeof(CCB),
						CCB_POOL_TAG
					);

	KDPrintM(DBG_CCB_TRACE, ("Allocated Ccb:%p\n", *Ccb));

	return (*Ccb == NULL)?STATUS_INSUFFICIENT_RESOURCES:STATUS_SUCCESS;
}


VOID
LsCcbFree(
		PCCB Ccb
	) {
//	ASSERT(Ccb->Flags & CCB_FLAG_ALLOCATED);

	KDPrintM(DBG_CCB_TRACE, ("entered with Ccb:%p\n", Ccb));

	if(LsCcbIsFlagOn(Ccb, CCB_FLAG_SENSEBUF_ALLOCATED)) {
		LsCcbResetFlag(Ccb, CCB_FLAG_SENSEBUF_ALLOCATED);
		KDPrintM(DBG_CCB_TRACE, ("Freeing Ccb sense=%p.\n", Ccb->SenseBuffer));
		ExFreePool(Ccb->SenseBuffer);
	}

	ExFreePoolWithTag(Ccb, CCB_POOL_TAG);
}

//
//	Complete a Ccb
//
//	We can set the CCBSTATUS_FLAG_PRE_COMPLETED flag only in this function
//
VOID
LsCcbCompleteCcb(
		IN PCCB Ccb
	) {
	LONG		idx_sl;
	NTSTATUS	status;

	ASSERT(Ccb->Type == LSSTRUC_TYPE_CCB);
	ASSERT(Ccb->Length == sizeof(CCB));
	ASSERT(!LsCcbIsStatusFlagOn(Ccb,CCBSTATUS_FLAG_PRE_COMPLETED));
	ASSERT(Ccb->AssociateCount == 0);
	ASSERT(Ccb->CcbCurrentStackLocationIndex < NR_MAX_CCB_STACKLOCATION);
	ASSERT(Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex]);

	KDPrintM(DBG_CCB_TRACE, ("entered with Ccb:%p\n", Ccb));

	LsCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_PRE_COMPLETED);
	LsCcbResetStatusFlag(Ccb, CCBSTATUS_FLAG_PENDING);

	Ccb->CcbCurrentStackLocationIndex++;
	Ccb->CcbCurrentStackLocation++;

	//
	//	Call completion routines in order.
	//
	for(	idx_sl = Ccb->CcbCurrentStackLocationIndex;
			idx_sl < NR_MAX_CCB_STACKLOCATION;
			idx_sl++, Ccb->CcbCurrentStackLocation++, Ccb->CcbCurrentStackLocationIndex++ ) {

		ASSERT(Ccb->CcbCurrentStackLocation <= &Ccb->CcbStackLocation[NR_MAX_CCB_STACKLOCATION - 1]);

		if(Ccb->CcbCurrentStackLocation->CcbCompletionRoutine) {
			status = Ccb->CcbCurrentStackLocation->CcbCompletionRoutine(Ccb, Ccb->CcbCurrentStackLocation->CcbCompletionContext);
			if(status == STATUS_MORE_PROCESSING_REQUIRED) {
				KDPrintM(DBG_CCB_TRACE, ("Ccb=%p, more processing required.\n", Ccb));
				return;
			}
			KDPrintM(DBG_CCB_TRACE, ("Ccb=%p, completion complete.\n", Ccb));
		}
	}

	//
	//	Do post operation
	//
	LsCcbPostCompleteCcb(Ccb);
}


//
//	Complete a Ccb
//
VOID
LsCcbPostCompleteCcb(
		IN PCCB Ccb
	) {
	PKEVENT	event;
	ASSERT(Ccb->Type == LSSTRUC_TYPE_CCB);
	ASSERT(Ccb->Length == sizeof(CCB));
	ASSERT(LsCcbIsStatusFlagOn(Ccb,CCBSTATUS_FLAG_PRE_COMPLETED));
	ASSERT(Ccb->AssociateCount == 0);
	ASSERT(Ccb->CcbCurrentStackLocationIndex <= NR_MAX_CCB_STACKLOCATION);
	ASSERT(Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex]);

	KDPrintM(DBG_CCB_TRACE, ("Ccb=%p, post-completion.\n", Ccb));

	if(LsCcbIsStatusFlagOn(Ccb,CCBSTATUS_FLAG_POST_COMPLETED)) {
		ASSERT(FALSE);
		return;
	}

	LsCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_POST_COMPLETED);

	event = Ccb->CompletionEvent;

	if(LsCcbIsFlagOn(Ccb, CCB_FLAG_DATABUF_ALLOCATED) && Ccb->DataBuffer && Ccb->DataBufferLength) {
		ASSERT(LsCcbIsFlagOn(Ccb, CCB_FLAG_ALLOCATED));
		KDPrintM(DBG_CCB_INFO, ("Freeing Ccb->DataBuffer=%p.\n", Ccb->DataBuffer));
		ExFreePool(Ccb->DataBuffer);
	}
	if(LsCcbIsFlagOn(Ccb, CCB_FLAG_ALLOCATED)) {
		KDPrintM(DBG_CCB_TRACE, ("Freeing Ccb=%p.\n", Ccb));
		LsCcbFree(Ccb);
	}

	//  You must not use any data in Ccb after KeSetEvent 
	// because caller will free Ccb after event received
	if(event) {
		KeSetEvent(event, IO_DISK_INCREMENT, FALSE);
	}
}


VOID
LsCcbInitialize(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		OUT PCCB						Ccb
	)
{
	ASSERT(Ccb);

	RtlZeroMemory(
		Ccb,
		sizeof(CCB)
		);

	Ccb->Srb = Srb;

	ASSERT(Srb->CdbLength <= MAXIMUM_CDB_SIZE);
	Ccb->Type						= LSSTRUC_TYPE_CCB;
	Ccb->Length						= sizeof(CCB);
    Ccb->CdbLength					= (UCHAR)Srb->CdbLength;
    RtlCopyMemory(Ccb->Cdb, Srb->Cdb, Ccb->CdbLength);
    Ccb->OperationCode				= CCB_OPCODE_FROM_SRBOPCODE(Srb->Function);
    Ccb->LurId[0]					= Srb->PathId;                   // offset 5
    Ccb->LurId[1]					= Srb->TargetId;                 // offset 6
    Ccb->LurId[2]					= Srb->Lun;                      // offset 7
    Ccb->DataBufferLength			= Srb->DataTransferLength;       // offset 10
    Ccb->DataBuffer					= Srb->DataBuffer;               // offset 18
	Ccb->AbortSrb					= Srb->NextSrb;
	Ccb->HwDeviceExtension			= HwDeviceExtension;
	Ccb->CcbCurrentStackLocationIndex = NR_MAX_CCB_STACKLOCATION - 1;
	Ccb->CcbCurrentStackLocation	= Ccb->CcbStackLocation + (NR_MAX_CCB_STACKLOCATION - 1);
	Ccb->SenseBuffer				= Srb->SenseInfoBuffer;
	Ccb->SenseDataLength			= Srb->SenseInfoBufferLength;
	KeInitializeSpinLock(&Ccb->CcbSpinLock);
	InitializeListHead(&Ccb->ListEntry);
}


NTSTATUS
LsCcbInitializeByCcb(
		IN PCCB							OriCcb,
		IN PVOID						pLurn,
		OUT PCCB						Ccb
	)
{
#if 0
	PUCHAR cdbChar;
	PCDB cdb10;
	PCDBEXT cdb16;
#endif
	ASSERT(Ccb);

	RtlCopyMemory(Ccb,	OriCcb,	sizeof(CCB));

	// Stack locations
	RtlZeroMemory(Ccb->CcbStackLocation, (NR_MAX_CCB_STACKLOCATION) * sizeof(CCB_STACKLOCATION));
	Ccb->CcbCurrentStackLocationIndex = NR_MAX_CCB_STACKLOCATION - 1;
	Ccb->CcbCurrentStackLocation	= Ccb->CcbStackLocation + (NR_MAX_CCB_STACKLOCATION - 1);
	Ccb->CcbCurrentStackLocation->Lurn = pLurn;
	InitializeListHead(&Ccb->ListEntry);
	KeInitializeSpinLock(&Ccb->CcbSpinLock);
	Ccb->AssociateCount				= 0;
	Ccb->CompletionEvent			= NULL;
	// Pass some flags to child ccb 
	Ccb->Flags = OriCcb->Flags & 
					(CCB_FLAG_URGENT | CCB_FLAG_ACQUIRE_BUFLOCK | CCB_FLAG_WRITE_CHECK| 
					CCB_FLAG_W2K_READONLY_PATCH | CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS |
					CCB_FLAG_MUST_SUCCEED);
	Ccb->CcbStatusFlags				= 0;
	
	if (OriCcb->SenseBuffer) {
		Ccb->SenseBuffer = ExAllocatePoolWithTag(
						NonPagedPool,
						OriCcb->SenseDataLength,
						CCB_POOL_TAG
		);
		if (Ccb->SenseBuffer == NULL) {
			KDPrintM(DBG_LURN_ERROR, ("Sense buffer allocation failed.\n"));			
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		RtlZeroMemory(Ccb->SenseBuffer, OriCcb->SenseDataLength);
		Ccb->SenseDataLength = OriCcb->SenseDataLength;
		LsCcbSetFlag(Ccb, CCB_FLAG_SENSEBUF_ALLOCATED);
	}
#if 0	// Contents of Cdb is already copied by RtlCopyMemory(Ccb,OriCcb,sizeof(CCB));
	//
	// Pass down some Cdb flags. lurnide will handle this flags.
	//
	cdbChar = ((PUCHAR)Ccb->Cdb;
	cdb10 = (PCDB)Ccb->Cdb;
	cdb16 = (PCDBEXT)Ccb->Cdb;
	
	if ( cdbChar[0] == SCSIOP_READ16 ||				
		cdbChar[0] == SCSIOP_WRITE16 ||
		cdbChar[0] == SCSIOP_VERIFY16 ||
		cdbChar[0] == SCSIOP_SYNCHRONIZE_CACHE16 ||
		cdbChar[0] == SCSIOP_READ_CAPACITY16) {
		((PCDBEXT)OriCcb->Cdb)->CDB16.ForceUnitAccess = cdb16->CDB16.ForceUnitAccess;
	} else {
		((PCDB)OriCcb->Cdb)->CDB10.ForceUnitAccess = cdb10->CDB10.ForceUnitAccess;
	}
#endif

	return STATUS_SUCCESS;
}

NTSTATUS
LsCcbRemoveExtendedCommandTailMatch(
								PCMD_COMMON *ppCmd, 
								PLURELATION_NODE pLurnCreated
								)
{
	PCMD_COMMON pNextCmd;
	NTSTATUS ntStatus;

	if(NULL == ppCmd)
		return STATUS_INVALID_PARAMETER;

	if(NULL == *ppCmd) // nothing to do
	{
		return STATUS_SUCCESS;
	}

	if(NULL != (*ppCmd)->pNextCmd) // not tail
	{
		ntStatus = LsCcbRemoveExtendedCommandTailMatch(&((*ppCmd)->pNextCmd), pLurnCreated);
		if(!NT_SUCCESS(ntStatus))
			return ntStatus;
	}

	// tail
	if((*ppCmd)->pLurnCreated == pLurnCreated)
	{
		// remove this
		pNextCmd = (*ppCmd)->pNextCmd;
		ExFreePoolWithTag(*ppCmd, EXTENDED_COMMAND_POOL_TAG);
		*ppCmd = pNextCmd;
	}

	return STATUS_SUCCESS;
}


//
//	Complete all Ccb in a list.
//
VOID
CcbCompleteList(
		PLIST_ENTRY	Head,
		CHAR		CcbStatus,
		USHORT		StatusFlags
	) {
	PLIST_ENTRY	listEntry;
	PCCB		ccb;

	while(1) {
		listEntry = RemoveHeadList(Head);
		if(listEntry==Head)
			break;

		ccb = CONTAINING_RECORD(listEntry, CCB, ListEntry);
		ccb->CcbStatus=CcbStatus;

		//
		//	We do not allow to set CCBSTATUS_FLAG_PENDING and CCBSTATUS_FLAG_PRE_COMPLETED except for CcbCompletion function.
		//
		LsCcbSetStatusFlag(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE| (StatusFlags & CCBSTATUS_FLAG_ASSIGNMASK));
		LsCcbCompleteCcb(ccb);
		KDPrintM(DBG_LURN_ERROR, ("Completed Ccb:%p\n", ccb));
	}
}
