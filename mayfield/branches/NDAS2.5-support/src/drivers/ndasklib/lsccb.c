#include <ntddk.h>
#include <TdiKrnl.h>
#include "basetsdex.h"
#include "LSKLib.h"
#include "KDebug.h"
#include "LSCcb.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSCcb"

NTSTATUS
LSCcbAllocate(
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
LSCcbFree(
		PCCB Ccb
	) {
//	ASSERT(Ccb->Flags & CCB_FLAG_ALLOCATED);

	KDPrintM(DBG_CCB_TRACE, ("entered with Ccb:%p\n", Ccb));

	if(LSCcbIsFlagOn(Ccb, CCB_FLAG_SENSEBUF_ALLOCATED)) {
		LSCcbResetFlag(Ccb, CCB_FLAG_SENSEBUF_ALLOCATED);
		KDPrintM(DBG_CCB_TRACE, ("Freeing Ccb sense=%p.\n", Ccb->SenseBuffer));
		ExFreePool(Ccb->SenseBuffer);
	}

	ExFreePoolWithTag(Ccb, CCB_POOL_TAG);
}

//
//	Complete a Ccb
//
//	We can set the CCBSTATUS_FLAG_COMPLETED flag only in this function
//
VOID
LSCcbCompleteCcb(
		IN PCCB Ccb
	) {
	LONG		idx_sl;
	NTSTATUS	status;

	ASSERT(Ccb->Type == LSSTRUC_TYPE_CCB);
	ASSERT(Ccb->Length == sizeof(CCB));
	ASSERT(!LSCcbIsStatusFlagOn(Ccb,CCBSTATUS_FLAG_COMPLETED));
	ASSERT(Ccb->AssociateCount == 0);
	ASSERT(Ccb->CcbCurrentStackLocationIndex < NR_MAX_CCB_STACKLOCATION);
	ASSERT(Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex]);

	KDPrintM(DBG_CCB_TRACE, ("entered with Ccb:%p\n", Ccb));

	LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_COMPLETED);
	LSCcbResetStatusFlag(Ccb, CCBSTATUS_FLAG_PENDING);

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
	LSCcbPostCompleteCcb(Ccb);
}


//
//	Complete a Ccb
//
VOID
LSCcbPostCompleteCcb(
		IN PCCB Ccb
	) {
	PKEVENT	event;
	ASSERT(Ccb->Type == LSSTRUC_TYPE_CCB);
	ASSERT(Ccb->Length == sizeof(CCB));
	ASSERT(LSCcbIsStatusFlagOn(Ccb,CCBSTATUS_FLAG_COMPLETED));
	ASSERT(Ccb->AssociateCount == 0);
	ASSERT(Ccb->CcbCurrentStackLocationIndex <= NR_MAX_CCB_STACKLOCATION);
	ASSERT(Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex]);

	if(LSCcbIsStatusFlagOn(Ccb,CCBSTATUS_FLAG_POST_COMPLETED)) {
		ASSERT(FALSE);
		return;
	}

	LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_POST_COMPLETED);

	event = Ccb->CompletionEvent;

	if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DATABUF_ALLOCATED) && Ccb->DataBuffer && Ccb->DataBufferLength) {
		ASSERT(LSCcbIsFlagOn(Ccb, CCB_FLAG_ALLOCATED));
		KDPrintM(DBG_CCB_INFO, ("Freeing Ccb->DataBuffer=%p.\n", Ccb->DataBuffer));
		ExFreePool(Ccb->DataBuffer);
	}

	if(LSCcbIsFlagOn(Ccb, CCB_FLAG_ALLOCATED)) {
		KDPrintM(DBG_CCB_TRACE, ("Freeing Ccb=%p.\n", Ccb));
		LSCcbFree(Ccb);
	}

	//  You must not use any data in Ccb after KeSetEvent 
	// because caller will free Ccb after event received
	if(event) {
		KeSetEvent(event, IO_DISK_INCREMENT, FALSE);
	}
}

VOID
LSCcbInitializeInSrb(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		IN ULONG						CcbSeqId,
		OUT PCCB						*Ccb
	)
{
	PCCB	ccb;

	ccb = (PCCB)Srb->SrbExtension;
	ASSERT(ccb);
	RtlZeroMemory(
		ccb,
		sizeof(CCB)
		);

	ccb->Srb = Srb;

	ASSERT(Srb->CdbLength <= MAXIMUM_CDB_SIZE);
	ccb->Type						= LSSTRUC_TYPE_CCB;
	ccb->Length						= sizeof(CCB);
    ccb->CdbLength					= (UCHAR)Srb->CdbLength;
    RtlCopyMemory(ccb->Cdb, Srb->Cdb, ccb->CdbLength);
	ccb->OperationCode				= Srb->Function;
    ccb->LurId[0]					= Srb->PathId;                   // offset 5
    ccb->LurId[1]					= Srb->TargetId;                 // offset 6
    ccb->LurId[2]					= Srb->Lun;                      // offset 7
    ccb->DataBufferLength			= Srb->DataTransferLength;       // offset 10
    ccb->DataBuffer					= Srb->DataBuffer;               // offset 18
	ccb->AbortSrb					= Srb->NextSrb;
	ccb->HwDeviceExtension			= HwDeviceExtension;
	ccb->CcbCurrentStackLocationIndex = NR_MAX_CCB_STACKLOCATION - 1;
	ccb->CcbCurrentStackLocation	= ccb->CcbStackLocation + (NR_MAX_CCB_STACKLOCATION - 1);
	ccb->SenseBuffer				= Srb->SenseInfoBuffer;
	ccb->SenseDataLength			= Srb->SenseInfoBufferLength;
	ccb->Flags						= 0;
	KeInitializeSpinLock(&ccb->CcbSpinLock);
	InitializeListHead(&ccb->ListEntry);
	ccb->CcbSeqId					= CcbSeqId;
	ccb->AssociateCount				= 0;
	ccb->CompletionEvent			= NULL;

	*Ccb = ccb;
}


VOID
LSCcbInitialize(
	    IN PSCSI_REQUEST_BLOCK			Srb,
		IN PVOID						HwDeviceExtension,
		IN ULONG						CcbSeqId,
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
	Ccb->OperationCode				= Srb->Function;
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
	Ccb->Flags						= CCB_FLAG_ALLOCATED;
	KeInitializeSpinLock(&Ccb->CcbSpinLock);
	InitializeListHead(&Ccb->ListEntry);
	Ccb->CcbSeqId					= CcbSeqId;
	Ccb->AssociateCount				= 0;
	Ccb->CompletionEvent			= NULL;
}


NTSTATUS
LSCcbInitializeByCcb(
		IN PCCB							OriCcb,
		IN PVOID						pLurn,
		IN ULONG						CcbSeqId,
		OUT PCCB						Ccb
	)
{
	ASSERT(Ccb);

	RtlCopyMemory(
			Ccb,
			OriCcb,
			sizeof(CCB)
		);

	// Stack locations
	RtlZeroMemory(Ccb->CcbStackLocation, (NR_MAX_CCB_STACKLOCATION) * sizeof(CCB_STACKLOCATION));
	Ccb->CcbCurrentStackLocationIndex = NR_MAX_CCB_STACKLOCATION - 1;
	Ccb->CcbCurrentStackLocation	= Ccb->CcbStackLocation + (NR_MAX_CCB_STACKLOCATION - 1);
	Ccb->CcbCurrentStackLocation->Lurn = pLurn;
	InitializeListHead(&Ccb->ListEntry);
	KeInitializeSpinLock(&Ccb->CcbSpinLock);
	Ccb->CcbSeqId					= CcbSeqId;
	Ccb->AssociateCount				= 0;
	Ccb->CompletionEvent			= NULL;
	Ccb->Flags						= 0;
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
		LSCcbSetFlag(Ccb, CCB_FLAG_SENSEBUF_ALLOCATED);
	}
	return STATUS_SUCCESS;
}

NTSTATUS
LSCcbRemoveExtendedCommandTailMatch(
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
		ntStatus = LSCcbRemoveExtendedCommandTailMatch(&((*ppCmd)->pNextCmd), pLurnCreated);
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
		//	We do not allow to set CCBSTATUS_FLAG_PENDING and CCBSTATUS_FLAG_COMPLETED except for CcbCompletion function.
		//
		LSCcbSetStatusFlag(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE| (StatusFlags & CCBSTATUS_FLAG_ASSIGNMASK));
		LSCcbCompleteCcb(ccb);
		KDPrintM(DBG_LURN_ERROR, ("Completed Ccb:%p\n", ccb));
	}
}
