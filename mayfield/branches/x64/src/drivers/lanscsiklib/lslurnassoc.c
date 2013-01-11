#include <ntddk.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnAssoc.h"
#include "lsminiportioctl.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSLurnAssoc"

NTSTATUS
LurnRAID1ReadCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	);

//////////////////////////////////////////////////////////////////////////
//
//	Associate LURN interfaces
//

//
//	aggregation interface
//
NTSTATUS
LurnAggrRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

LURN_INTERFACE LurnAggrInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_AGGREGATION,
					0,
					{
						LurnInitializeDefault,
						LurnDestroyDefault,
						LurnAggrRequest
					}
		 };

//
//	mirroring interface
//
NTSTATUS
LurnMirrRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);


LURN_INTERFACE LurnMirrorInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_MIRRORING,
					0,
					{
						LurnInitializeDefault,
						LurnDestroyDefault,
						LurnMirrRequest
					}
		 };

NTSTATUS
LurnRAID0Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
LurnRAID0Destroy(
		PLURELATION_NODE Lurn
	) ;

NTSTATUS
LurnRAID0Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

LURN_INTERFACE LurnRAID0Interface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_RAID0,
					0,
					{
						LurnRAID0Initialize,
						LurnRAID0Destroy,
						LurnRAID0Request
					}
		 };

//
//	mirroring V2 interface
//
NTSTATUS
LurnRAID1Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
LurnRAID1Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

NTSTATUS
LurnRAID1Destroy(
		PLURELATION_NODE Lurn
	) ;

LURN_INTERFACE LurnRAID1Interface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_RAID1,
					0,
					{
						LurnRAID1Initialize,
						LurnRAID1Destroy,
						LurnRAID1Request
					}
		 };

//
//	RAID4 interface
//
NTSTATUS
LurnRAID4Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
LurnRAID4Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

NTSTATUS
LurnRAID4Destroy(
		PLURELATION_NODE Lurn
	) ;

LURN_INTERFACE LurnRAID4Interface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_RAID4,
					0,
					{
						LurnRAID4Initialize,
						LurnRAID4Destroy,
						LurnRAID4Request
					}
		 };

//////////////////////////////////////////////////////////////////////////
//
//	common to LURN array
//	common to associate LURN
//

NTSTATUS
LurnAssocSendCcbToChildrenArray(
		IN PLURELATION_NODE			*pLurnChildren,
		IN LONG						ChildrenCnt,
		IN PCCB						Ccb,
		IN CCB_COMPLETION_ROUTINE	CcbCompletion,
		IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
		IN PVOID					*apExtendedCmd, // NULL if no cmd
		IN BOOLEAN					AssociateCascade
)
{
	LONG		idx_child;
	NTSTATUS	status;
	PCCB		NextCcb[LUR_MAX_LURNS_PER_LUR];
	PCMD_COMMON	pCmdTemp;

	ASSERT(!LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN));
	//
	//	Allocate new CCBs for the children
	//
	for(idx_child = 0; idx_child < ChildrenCnt; idx_child++)
	{
		status = LSCcbAllocate(&NextCcb[idx_child]);

		if(!NT_SUCCESS(status))
		{
			LONG	idx;

			KDPrintM(DBG_LURN_ERROR, ("LSCcbAllocate failed.\n"));
			for(idx = 0; idx < idx_child; idx++) {
				LSCcbFree(NextCcb[idx]);
			}
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			return STATUS_SUCCESS;
		}

		LSCcbInitializeByCcb(Ccb, pLurnChildren[idx_child], 0, NextCcb[idx_child]);
		NextCcb[idx_child]->AssociateID = (USHORT)idx_child;
		LSCcbSetFlag(NextCcb[idx_child], CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
		LSCcbSetFlag(NextCcb[idx_child], Ccb->Flags&CCB_FLAG_SYNCHRONOUS);
		LSCcbSetCompletionRoutine(NextCcb[idx_child], CcbCompletion, Ccb);


		// Cascade code
		if(AssociateCascade)
		{
			KDPrintM(DBG_LURN_ERROR, ("Cascade #%d.\n", idx_child));
			NextCcb[idx_child]->AssociateCascade = TRUE;
			if(NextCcb[idx_child]->AssociateID > 0)
			{
				KeInitializeEvent(&NextCcb[idx_child]->EventCascade, SynchronizationEvent, FALSE);
				NextCcb[idx_child]->EventCascadeNext = NULL;
				NextCcb[idx_child -1]->EventCascadeNext = &NextCcb[idx_child]->EventCascade;
				NextCcb[idx_child]->ForceFail = FALSE;
				NextCcb[idx_child]->ForceFailNext = NULL;
				NextCcb[idx_child -1]->ForceFailNext = &NextCcb[idx_child]->ForceFail;
			}
		}


		// attach the data buffers to each CCBs(optional)
		if(pcdbDataBuffer)
		{
			ASSERT(ChildrenCnt == pcdbDataBuffer->DataBufferCount);

			NextCcb[idx_child]->DataBuffer = pcdbDataBuffer->DataBuffer[idx_child];
			NextCcb[idx_child]->DataBufferLength = (ULONG)pcdbDataBuffer->DataBufferLength[idx_child];
		}

		// add extended cmd
		if(apExtendedCmd)
		{
			// iterate to last command in Ccb
			for(pCmdTemp = NextCcb[idx_child]->pExtendedCommand; NULL != pCmdTemp && NULL != pCmdTemp->pNextCmd; pCmdTemp = pCmdTemp->pNextCmd)
				;
			// attach
			if(NULL == pCmdTemp) // nothing in list
				NextCcb[idx_child]->pExtendedCommand = apExtendedCmd[idx_child];
			else
				pCmdTemp->pNextCmd = apExtendedCmd[idx_child];
		}
	}

	//
	//	Send CCBs to the child.
	//
	Ccb->AssociateCount = ChildrenCnt;
	for(idx_child = 0; idx_child < ChildrenCnt; idx_child++) {
		status = LurnRequest(pLurnChildren[idx_child], NextCcb[idx_child]);
		if(!NT_SUCCESS(status)) {
			LONG	idx;

			KDPrintM(DBG_LURN_ERROR, ("LurnRequest to Child#%d failed.\n", idx_child));
			for(idx = idx_child; idx < ChildrenCnt; idx++) {
					LSCcbSetStatus(NextCcb[idx], CCB_STATUS_COMMAND_FAILED);
					LSCcbSetNextStackLocation(NextCcb[idx]);
					LSCcbCompleteCcb(NextCcb[idx]);
			}
			break;
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnAssocSendCcbToAllChildren(
		IN PLURELATION_NODE			Lurn,
		IN PCCB						Ccb,
		IN CCB_COMPLETION_ROUTINE	CcbCompletion,
		IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
		IN PVOID					*apExtendedCmd,
		IN BOOLEAN					AssociateCascade
){
	return LurnAssocSendCcbToChildrenArray((PLURELATION_NODE *)Lurn->LurnChildren, Lurn->LurnChildrenCnt, Ccb, CcbCompletion, pcdbDataBuffer, apExtendedCmd, AssociateCascade);
}

NTSTATUS
LurnAssocQuery(
	IN PLURELATION_NODE			Lurn,
	IN CCB_COMPLETION_ROUTINE	CcbCompletion,
	IN OUT PCCB					Ccb
)
{
	NTSTATUS			status;
	PLUR_QUERY			query;

	if(CCB_OPCODE_QUERY != Ccb->OperationCode)
		return STATUS_INVALID_PARAMETER;

	KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_QUERY\n"));

	//
	//	Check to see if the CCB is coming for only this LURN.
	//
	if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
		LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	}

	query = (PLUR_QUERY)Ccb->DataBuffer;

	switch(query->InfoClass)
	{
	case LurEnumerateLurn:
		{
			PLURN_ENUM_INFORMATION	ReturnInfo;
			PLURN_INFORMATION		LurnInfo;

			ReturnInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION(query);
			LurnInfo = &ReturnInfo->Lurns[Lurn->LurnId];
			LurnInfo->Length = sizeof(LURN_INFORMATION);
			LurnInfo->LurnId = Lurn->LurnId;
			LurnInfo->LurnType = Lurn->LurnType;
			LurnInfo->UnitBlocks = Lurn->UnitBlocks;
			LurnInfo->BlockUnit	= BLOCK_SIZE;
			LurnInfo->AccessRight = Lurn->AccessRight;
			LurnInfo->StatusFlags = 0;

			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, CcbCompletion, NULL, NULL, FALSE);

		}
		break;

	case LurRefreshLurn:
		{
			// only the leaf nodes will process this query
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, CcbCompletion, NULL, NULL, FALSE);
		}
		break;

	case LurPrimaryLurnInformation:
	default:
		if(Lurn->LurnChildrenCnt > 0) {
			status = LurnRequest(Lurn->LurnChildren[0], Ccb);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_TRACE, ("LurnRequest to Child#0 failed.\n"));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				LSCcbCompleteCcb(Ccb);
			}
		} else {
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
		}
	}
	return status;
}

NTSTATUS
LurnAssocRefreshCcbStatusFlag(
	IN PLURELATION_NODE			pLurn,
	PULONG						CcbStatusFlags
)
{
	NTSTATUS					ntStatus;

	CCB							Ccb;

	PLUR_QUERY					LurQuery;
	BYTE						LurBuffer[SIZE_OF_LURQUERY(0, sizeof(LURN_REFRESH))];
	PLURN_REFRESH				LurnRefresh;

	//
	//	initialize query CCB
	//
	LSCCB_INITIALIZE(&Ccb, 0);
	Ccb.OperationCode = CCB_OPCODE_QUERY;
	LSCcbSetFlag(&Ccb, CCB_FLAG_SYNCHRONOUS);

	RtlZeroMemory(LurBuffer, sizeof(LurBuffer));
	LurQuery = (PLUR_QUERY)LurBuffer;
	LurQuery->InfoClass = LurRefreshLurn;
	LurQuery->QueryDataLength = 0;

	LurnRefresh = (PLURN_REFRESH)LUR_QUERY_INFORMATION(LurQuery);
	LurnRefresh->Length = sizeof(LurnRefresh);

	Ccb.DataBuffer = LurQuery;
	Ccb.DataBufferLength = LurQuery->Length;

	ntStatus = LurnRequest(pLurn, &Ccb);

	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("LurnRequest() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	*CcbStatusFlags |= LurnRefresh->CcbStatusFlags;

	return STATUS_SUCCESS;
}

#define EXEC_SYNC_POOL_TAG 'YSXE'
static 
NTSTATUS
LurnExecuteSync(
				IN ULONG					NrLurns,
				IN PLURELATION_NODE			Lurn[],
				IN UCHAR					CDBOperationCode,
				IN PCHAR					DataBuffer[],
				IN UINT64					BlockAddress,
				IN UINT16					BlockTransfer,
				IN PCMD_COMMON				ExtendedCommand)
{
	NTSTATUS				status;
	PCCB					Ccb;
	UINT32					DataBufferLength;
	ULONG					i, Waits;
	PKEVENT					CompletionEvents, *CompletionWaitEvents;
	PKWAIT_BLOCK			WaitBlockArray;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;

	ASSERT(NrLurns < LUR_MAX_LURNS_PER_LUR);

	DataBufferLength = BlockTransfer * BLOCK_SIZE;

	KDPrintM(DBG_LURN_NOISE, ("NrLurns : %d, Lurn : %08x, DataBuffer : %08x, ExtendedCommand : %08x\n",
		NrLurns, Lurn, DataBuffer, ExtendedCommand));

	Ccb = (PCCB)ExAllocatePoolWithTag(NonPagedPool, sizeof(CCB) * NrLurns,
		EXEC_SYNC_POOL_TAG);
	if(!Ccb)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	CompletionEvents = (PKEVENT)ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT) * NrLurns,
		EXEC_SYNC_POOL_TAG);
	if(!CompletionEvents)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	CompletionWaitEvents = (PKEVENT *)ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT *) * NrLurns,
		EXEC_SYNC_POOL_TAG);
	if(!CompletionWaitEvents)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	WaitBlockArray = (PKWAIT_BLOCK)ExAllocatePoolWithTag(NonPagedPool, sizeof(KWAIT_BLOCK) * NrLurns,
		EXEC_SYNC_POOL_TAG);
	if(!WaitBlockArray)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	Waits = 0;

	for(i = 0; i < NrLurns; i++)
	{
		LSCCB_INITIALIZE(&Ccb[i], 0);

		Ccb[i].OperationCode = CCB_OPCODE_EXECUTE;
		Ccb[i].DataBuffer = DataBuffer[i];
		Ccb[i].DataBufferLength = DataBufferLength;		
		((PCDB)(Ccb[i].Cdb))->CDB10.OperationCode = CDBOperationCode;
		CDB10_LOGICAL_BLOCK_BYTE_TO_BYTES(&(Ccb[i].Cdb), BlockAddress );
		CDB10_TRANSFER_BLOCKS_TO_BYTES(&(Ccb[i].Cdb), BlockTransfer);
		Ccb[i].CompletionEvent = &CompletionEvents[i];
		Ccb[i].pExtendedCommand = (ExtendedCommand) ? &ExtendedCommand[i] : NULL;

		KeInitializeEvent(Ccb[i].CompletionEvent, SynchronizationEvent, FALSE);

		status = LurnRequest(Lurn[i], &Ccb[i]);
		KDPrintM(DBG_LURN_NOISE, ("LurnRequest result : %08x\n", status));

		if(!NT_SUCCESS(status))
		{
			KDPrintM(DBG_LURN_ERROR, ("LurnRequest Failed Status : %08lx", status));

			ASSERT(FALSE);
			goto out;
		}

		if(STATUS_PENDING == status)
		{
			CompletionWaitEvents[Waits] = &CompletionEvents[i];
			Waits++;
		}
		else if(!LURN_IS_RUNNING(Lurn[i]->LurnStatus))
		{
			// can not proceed this job
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}
		else
		{
			// CCB_OPCODE_EXECUTE always pending
			ASSERT(STATUS_PENDING == status);
		}
	}

	if(Waits)
	{
		status = KeWaitForMultipleObjects(
			Waits,
			CompletionWaitEvents,
			WaitAll,
			Executive,
			KernelMode,
			FALSE,
			NULL,
			WaitBlockArray
			);

		KDPrintM(DBG_LURN_NOISE, ("KeWaitForMultipleObjects result : %08x, Waits : %d\n",
			status, Waits));

		switch(status)
		{
		case STATUS_SUCCESS:
			break;
		case STATUS_ALERTED:
		case STATUS_USER_APC:
		case STATUS_TIMEOUT:
		default: // includes STATUS_WAIT_0 ~ STATUS_WAIT_63
			ASSERT(FALSE);
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}

	}

	for(i = 0; i < NrLurns; i++)
	{
		if(CCB_STATUS_SUCCESS != Ccb[i].CcbStatus)
		{
			KDPrintM(DBG_LURN_ERROR, ("Failed CcbStatus : %08lx, CcbStatusFlags : %08lx\n",
				Ccb[i].CcbStatus, Ccb[i].CcbStatusFlags));
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}
	}

	status = STATUS_SUCCESS;
out:
	if(CompletionEvents)
	{
		ExFreePoolWithTag(CompletionEvents, EXEC_SYNC_POOL_TAG);
		CompletionEvents = NULL;
	}

	if(CompletionWaitEvents)
	{
		ExFreePoolWithTag(CompletionWaitEvents, EXEC_SYNC_POOL_TAG);
		CompletionWaitEvents = NULL;
	}

	if(WaitBlockArray)
	{
		ExFreePoolWithTag(WaitBlockArray, EXEC_SYNC_POOL_TAG);
		WaitBlockArray = NULL;
	}	

	if(Ccb)
	{
		ExFreePoolWithTag(Ccb, EXEC_SYNC_POOL_TAG);
		Ccb = NULL;
	}


	return status;
}

/*
LurnRAIDInitialize MUST be called only by LurnRAIDThreadProcRecover
*/
static
NTSTATUS
LurnRAIDInitialize(
	IN OUT PLURELATION_NODE		Lurn,
	IN OUT PCMD_BYTE_OP ExtendedCommand,
	IN OUT PUCHAR BufferRecover
	)
{
	NTSTATUS status;

	PRAID_INFO pRaidInfo;
	PRTL_BITMAP Bitmap;
	UINT32 SectorsPerBit;
	UINT16 transferBlocks;
	UINT32 RecoverLoopMax;
	register ULONG i, j;
	KIRQL oldIrql;

	ASSERT(Lurn);
	ASSERT(LURN_RAID1 == Lurn->LurnType || LURN_RAID4 == Lurn->LurnType);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	Bitmap = pRaidInfo->Bitmap;
	ASSERT(Bitmap);

	SectorsPerBit = pRaidInfo->SectorsPerBit;
	ASSERT(SectorsPerBit > 0);

	transferBlocks = (USHORT)pRaidInfo->MaxBlocksPerRequest;
	RecoverLoopMax = (SectorsPerBit >= transferBlocks) ?
		SectorsPerBit / transferBlocks : 1;

	// assert that read/write operation is blocked
	ASSERT(RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus);

	// wait until all children being running status
	{
		ULONG LurnStatus;

		do{
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				ACQUIRE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, &oldIrql);
				LurnStatus = Lurn->LurnChildren[i]->LurnStatus;
				RELEASE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, oldIrql);

				if(LURN_STATUS_RUNNING == LurnStatus)
					continue;

				if(LURN_STATUS_INIT == LurnStatus)
				{
					break;
				}

				// error
				KDPrintM(DBG_LURN_ERROR, ("LURN_STATUS Failed : %d\n", LurnStatus));
				//				ASSERT(FALSE);
				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				return STATUS_SUCCESS;
			}
		} while(i != Lurn->LurnChildrenCnt);
	}

	// all children are running, now read & test each bitmap

	KDPrintM(DBG_LURN_ERROR, ("All children are ready to read bitmap from\n"));

	if(Lurn->AccessRight & GENERIC_WRITE)
	{
		PULONG					pReadBitmap;
		ULONG					iChildRecoverInfo = 0xFFFFFFFF;

		// step 1 : bitmap
		for (i = 0; i < Lurn->LurnChildrenCnt; i++)
		{
			ASSERT(2048 % transferBlocks == 0);
			for(j = 0; j < 2048 / (ULONG)transferBlocks; j++) // 1MB
			{
				RtlZeroMemory(ExtendedCommand, sizeof(CMD_BYTE_OP));
				ExtendedCommand->Operation = CCB_EXT_READ;
				ExtendedCommand->logicalBlockAddress =
					(ULONG)pRaidInfo->Children[i].SectorBitmapStart + 
					j * transferBlocks;
				ExtendedCommand->ByteOperation = EXT_BLOCK_OPERATION;
				ExtendedCommand->pByteData = BufferRecover;
				ExtendedCommand->LengthBlock = transferBlocks;
				ExtendedCommand->pLurnCreated = Lurn;

				status = LurnExecuteSync(
					1, 
					&Lurn->LurnChildren[i],
					SCSIOP_WRITE,
					&BufferRecover,
					0,
					0,
					(PCMD_COMMON)ExtendedCommand);

				if(!NT_SUCCESS(status))
				{
					ASSERT(FALSE);
					pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
					return STATUS_SUCCESS;
				}

				// is bitmap clean?
				for(pReadBitmap = (PULONG)BufferRecover;
					pReadBitmap < (PULONG)(BufferRecover + transferBlocks * BLOCK_SIZE);
					pReadBitmap++)
				{
					if(*pReadBitmap) // corrupted
					{
						if(0xFFFFFFFF == iChildRecoverInfo)
						{
							KDPrintM(DBG_LURN_ERROR, ("Child %d has corruption bitmap\n", i));
							pRaidInfo->iChildRecoverInfo = iChildRecoverInfo = i;
						}
						else if(i == iChildRecoverInfo) // already detected this child
						{
							KDPrintM(DBG_LURN_NOISE, ("again %d - %08lx\n", i, pReadBitmap));
						}
						else // 2nd corrupted device. fail to mount
						{
							ASSERT(FALSE);
							KDPrintM(DBG_LURN_ERROR, ("More than 1 children have corruption data\n"));
							pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
							return STATUS_SUCCESS;
						}
					}
				}


				if(i == iChildRecoverInfo)
				{
					ASSERT(pRaidInfo->Bitmap->SizeOfBitMap % 8 == 0);

					if((j +1) * transferBlocks * BLOCK_SIZE <= pRaidInfo->Bitmap->SizeOfBitMap / 8)
					{
						// full copy
						KDPrintM(DBG_LURN_NOISE, ("FC : %d <= %d \n", (j +1) * transferBlocks * BLOCK_SIZE, pRaidInfo->Bitmap->SizeOfBitMap / 8));
						RtlCopyMemory((PCHAR)(pRaidInfo->Bitmap->Buffer) + j * transferBlocks * BLOCK_SIZE, 
							BufferRecover, transferBlocks * BLOCK_SIZE);
					}
					else if(j * transferBlocks * BLOCK_SIZE < pRaidInfo->Bitmap->SizeOfBitMap / 8)
					{
						// copy partly
						KDPrintM(DBG_LURN_NOISE, ("PC : %d < %d \n", j * transferBlocks * BLOCK_SIZE, pRaidInfo->Bitmap->SizeOfBitMap / 8));
						RtlCopyMemory((PCHAR)(pRaidInfo->Bitmap->Buffer) + j * transferBlocks * BLOCK_SIZE, 
							BufferRecover, pRaidInfo->Bitmap->SizeOfBitMap / 8 - j * transferBlocks * BLOCK_SIZE);
					}
					else
					{
						// pass
						KDPrintM(DBG_LURN_NOISE, ("NC : %d ... %d \n", j * transferBlocks * BLOCK_SIZE, pRaidInfo->Bitmap->SizeOfBitMap / 8));
					}
				}

			}
			KDPrintM(DBG_LURN_ERROR, ("Check %d complete\n", i));
		}

		// step 2 : LWS
		if(0xFFFFFFFF == iChildRecoverInfo)
		{
			// all bitmaps are clean
			CMD_BYTE_LAST_WRITTEN_SECTOR LWSsBase[32];
			ULONG iMismatch;

			KDPrintM(DBG_LURN_ERROR, ("Corruption bitmap NOT detected, Checking LWSs\n"));

			// step 2-1 : read all LWSs and compare
			iMismatch = 0;
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				RtlZeroMemory(ExtendedCommand, sizeof(CMD_BYTE_OP));
				ExtendedCommand->Operation = CCB_EXT_READ;
				ExtendedCommand->logicalBlockAddress =
					(ULONG)pRaidInfo->Children[i].SectorLastWrittenSector;
				ExtendedCommand->ByteOperation = EXT_BLOCK_OPERATION;
				ExtendedCommand->pByteData = (PBYTE)pRaidInfo->LWSs;
				ExtendedCommand->LengthBlock = 1;
				ExtendedCommand->pLurnCreated = Lurn;

				status = LurnExecuteSync(
					1, 
					&Lurn->LurnChildren[i],
					SCSIOP_WRITE,
					&BufferRecover,
					0,
					0,
					(PCMD_COMMON)ExtendedCommand);

				if(!NT_SUCCESS(status))
				{
					ASSERT(FALSE);
					pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
					return STATUS_SUCCESS;
				}

				if(0 == i)
				{
					RtlCopyMemory(LWSsBase, pRaidInfo->LWSs, sizeof(LWSsBase));
				}
				else
				{
					if(sizeof(LWSsBase) != RtlCompareMemory(LWSsBase, pRaidInfo->LWSs, sizeof(LWSsBase)))
					{
						KDPrintM(DBG_LURN_ERROR, ("LWS mismatch at %d\n", i));
						iMismatch++;
						iChildRecoverInfo = (i == Lurn->LurnChildrenCnt -1) ? 0 : i + 1;
					}
				}
				KDPrintM(DBG_LURN_ERROR, ("Check %d complete\n", i));
			}

			// step 2-2 : find out defected LWSs
			if(0 == iMismatch)
			{
				// status ok
				ASSERT(0xFFFFFFFF == iChildRecoverInfo);
			}
			else if(1 == iMismatch)
			{
				// iChildRecoverInfo is correct
				ASSERT(0xFFFFFFFF != iChildRecoverInfo);
			}
			else if(Lurn->LurnChildrenCnt -1 == iMismatch) // 1st is wrong. just suspecting though
			{
				// ...
				ASSERT(0xFFFFFFFF != iChildRecoverInfo);
				iChildRecoverInfo = 0 + 1;
			}
			else // totally failed
			{
				KDPrintM(DBG_LURN_ERROR, ("Status invalid by LWSs\n"));
				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				return STATUS_SUCCESS;
			}
		}

		// step 3 : set status
		if(0xFFFFFFFF != iChildRecoverInfo)
		{
			KDPrintM(DBG_LURN_ERROR, ("Corruption information detected, Reading LWSs\n"));

			RtlZeroMemory(ExtendedCommand, sizeof(CMD_BYTE_OP));
			ExtendedCommand->Operation = CCB_EXT_READ;
			ExtendedCommand->logicalBlockAddress =
				(ULONG)pRaidInfo->Children[iChildRecoverInfo].SectorLastWrittenSector;
			ExtendedCommand->ByteOperation = EXT_BLOCK_OPERATION;
			ExtendedCommand->pByteData = (PBYTE)pRaidInfo->LWSs;
			ExtendedCommand->LengthBlock = 1;
			ExtendedCommand->pLurnCreated = Lurn;

			status = LurnExecuteSync(
				1, 
				&Lurn->LurnChildren[iChildRecoverInfo],
				SCSIOP_WRITE,
				&BufferRecover,
				0,
				0,
				(PCMD_COMMON)ExtendedCommand);

			if(!NT_SUCCESS(status))
			{
				ASSERT(FALSE);
				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				return STATUS_SUCCESS;
			}

			KDPrintM(DBG_LURN_ERROR, ("Corruption detected, starting with RECOVERY mode\n"));

			pRaidInfo->iChildRecoverInfo = iChildRecoverInfo;
			pRaidInfo->iChildDefected = 
				(iChildRecoverInfo == 0) ? Lurn->LurnChildrenCnt -1 : iChildRecoverInfo -1;
			pRaidInfo->RaidStatus = RAID_STATUS_RECOVERRING;
		}
		else
		{
			KDPrintM(DBG_LURN_ERROR, ("Corruption information not found, starting with NORMAL mode\n"));
			pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;
		}
	}
	else // do not recover when running on read only mode
	{
		KDPrintM(DBG_LURN_ERROR, ("Read only mode : starting with NORMAL mode\n"));
		pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;
	}

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
// LurnRAIDThreadProcRecover
//
// LurnRAIDThreadProcRecover is thread function which used
// to recover the broken RAID(1, 4)
//
// Created in LurnRAID?Initialize
// Starts to recover when Recover Thread Event is called
//
// 1. Revive the dead IdeLurn
// 2. Initialize variables
// 3. Merge LWS to Bitmap
// 4. Lock - Read - Write - Unlock each sectors for bitmap
// 5. Clear bitmap in disk when each sectors in bitmap is cleared
// 6. Recover LWS
// 7. If recovery is done | status is back to emergency, Wait for Recover Thread Event
// 8. Terminate Thread in LurnRAID?Destroy

static
void
LurnRAIDThreadProcRecover(
	IN	PVOID	Context
	)
{
	NTSTATUS				status;
	ULONG					i, j;
	PLURELATION_NODE		Lurn;
	PLURELATION_NODE		LurnDefected, LurnRecoverInfo, LurnsHealthy[NDAS_MAX_RAID_CHILD -1];
	PRAID_INFO				pRaidInfo;

	ULONG					BitmapIdxToRecover;
	ULONG					BitmapIdxToRecovered;
	PCMD_BYTE_OP			ExtendedCommand = NULL;

	PRTL_BITMAP				Bitmap;
	PUCHAR					BufferRecover = NULL; // Buffer to write to defected device
	PUCHAR					BufferRead = NULL; // Buffer to read from healthy device(s)
	PUCHAR					pBufferRead[NDAS_MAX_RAID_CHILD -1]; // Pointers to BufferRead
	PULONG					pDataBufferToRecover, pDataBufferSrc; // used to create parity buffer
	UINT32					SectorsPerBit;
	UCHAR					OneSector[512]; // used to clear bitmap and recover LWS with extended command

	UINT64					startBlockAddress;
	USHORT					transferBlocks, transferBlocksToRecover;
	UINT32					RecoverLoop, RecoverLoopMax;

	KIRQL					oldIrql;
	BOOLEAN					LurnRunning;

	LURN_EVENT				LurnEvent;

	KDPrintM(DBG_LURN_ERROR, ("Entered\n"));

	// initialize variables, buffers
	ASSERT(Context);

	Lurn = (PLURELATION_NODE)Context;
	ASSERT(LURN_RAID1 == Lurn->LurnType || LURN_RAID4 == Lurn->LurnType);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	Bitmap = pRaidInfo->Bitmap;
	ASSERT(Bitmap);

	SectorsPerBit = pRaidInfo->SectorsPerBit;
	ASSERT(SectorsPerBit > 0);

	transferBlocks = (USHORT)pRaidInfo->MaxBlocksPerRequest;
	RecoverLoopMax = (SectorsPerBit >= transferBlocks) ?
		SectorsPerBit / transferBlocks : 1;

	ASSERT(transferBlocks > 0);
	ASSERT(RecoverLoopMax > 0);

	// allocate buffers
	// extended command
	ExtendedCommand = (PCMD_BYTE_OP)ExAllocatePoolWithTag(NonPagedPool,
		sizeof(CMD_BYTE_OP), RAID_RECOVER_POOL_TAG);

	if(!ExtendedCommand)
	{
		KDPrintM(DBG_LURN_ERROR, ("Failed to allocate ExtendedCommand\n"));
		pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
		//			KeSetEvent(&pRaidInfo->ThreadReadyEvent, IO_NO_INCREMENT, FALSE);
		goto out;
	}

	// recover buffer
	BufferRecover = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 
		transferBlocks * BLOCK_SIZE, RAID_RECOVER_POOL_TAG);

	if(!BufferRecover)
	{
		KDPrintM(DBG_LURN_ERROR, ("Failed to allocate BufferRecover\n"));
		pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
		//			KeSetEvent(&pRaidInfo->ThreadReadyEvent, IO_NO_INCREMENT, FALSE);
		goto out;
	}

	// data buffer
	if(LURN_RAID4 == Lurn->LurnType)
	{
		if(BufferRead)
		{
			ExFreePoolWithTag(BufferRead, RAID_RECOVER_POOL_TAG);
			BufferRead = NULL;
		}

		BufferRead = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 
			transferBlocks * BLOCK_SIZE * Lurn->LurnChildrenCnt -1, RAID_RECOVER_POOL_TAG);

		if(!BufferRead)
		{
			KDPrintM(DBG_LURN_ERROR, ("Failed to allocate BufferRead\n"));
			pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
			//			KeSetEvent(&pRaidInfo->ThreadReadyEvent, IO_NO_INCREMENT, FALSE);
			goto out;
		}

		for(i = 0; i < Lurn->LurnChildrenCnt -1; i++)
		{
			pBufferRead[i] = BufferRead + transferBlocks * BLOCK_SIZE * i;
		}
	}
	else if(LURN_RAID1 == Lurn->LurnType)
	{
		// just copy
		pBufferRead[0] = BufferRead = BufferRecover;			
	}

	KDPrintM(DBG_LURN_ERROR, ("Variables, Buffers initialized. Read & testing bitmaps\n"));

	ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
	if(RAID_STATUS_INITIAILIZING != pRaidInfo->RaidStatus)
	{
		ASSERT(RAID_STATUS_TERMINATING == pRaidInfo->RaidStatus);
		RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
		goto out;
	}
	RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

	// assert that read/write operation is blocked
	ASSERT(RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus);

	status = LurnRAIDInitialize(Lurn, ExtendedCommand, BufferRecover);

	if(!NT_SUCCESS(status))
		goto out;

	if(RAID_STATUS_FAIL == pRaidInfo->RaidStatus)
		goto out;

	do{
		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
		LurnRunning = LURN_IS_RUNNING(Lurn->LurnStatus);
		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		if(!LurnRunning)
			break;

		if(RAID_STATUS_RECOVERRING != pRaidInfo->RaidStatus)
		{
			// Wait for recover start event
			KDPrintM(DBG_LURN_ERROR, ("KeWaitForSingleObject ...\n"));

			status = KeWaitForSingleObject(
				&pRaidInfo->RecoverThreadEvent,
				Executive,
				KernelMode,
				FALSE,
				NULL);

			KDPrintM(DBG_LURN_ERROR, ("KeWaitForSingleObject returns %08lx ...\n", status));

			if(STATUS_SUCCESS != status)
				break;

			KeClearEvent(&pRaidInfo->RecoverThreadEvent);

			if(RAID_STATUS_TERMINATING == pRaidInfo->RaidStatus)
			{
				KDPrintM(DBG_LURN_ERROR, ("Plug out : Terminating thread...\n"));
				break;
			}
			
			if(RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus)
			{
				// being a primary host
				ASSERT(Lurn->AccessRight & GENERIC_WRITE);

				status = LurnRAIDInitialize(Lurn, ExtendedCommand, BufferRecover);
				if(!NT_SUCCESS(status) || RAID_STATUS_FAIL == pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("Initialize failed : Terminating thread...\n"));
					break;
				}

				// bitmap, LWSs is clean
				if(RAID_STATUS_NORMAL == pRaidInfo->RaidStatus)
					continue;

				ASSERT(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus);
			}
			else
			{
				ASSERT(RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus);
				
				if(RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("Not emergency status, waiting...\n"));
					continue;
				}
			}

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_ERROR, ("Read only mode do not recover...\n"));
				continue;
			}
		}
		else
		{
			// recover on mount status
			// skip waiting event
			KDPrintM(DBG_LURN_ERROR, ("Skip waiting event & start to recover\n"));
			KeClearEvent(&pRaidInfo->RecoverThreadEvent);
		}

		// initialize variables
		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
		LurnRunning = LURN_IS_RUNNING(Lurn->LurnStatus);
		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		if(!LurnRunning || RAID_STATUS_TERMINATING == pRaidInfo->RaidStatus)
			break;

		LurnDefected = Lurn->LurnChildren[pRaidInfo->iChildDefected];
		LurnRecoverInfo = Lurn->LurnChildren[pRaidInfo->iChildRecoverInfo];

		for(i = 0; i < Lurn->LurnChildrenCnt -1; i++)
		{
			LurnsHealthy[i] = Lurn->LurnChildren[((UINT32)i >= pRaidInfo->iChildDefected) ? i +1 : i];
		}

		// revive corrupted LURN (IDE)
		status = LurnInitialize(LurnDefected, LurnDefected->Lur, NULL);

		KDPrintM(DBG_LURN_ERROR, ("LurnInitialize result. %x\n", status));

		if(NT_SUCCESS(status))
		{
			ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
			pRaidInfo->RaidStatus = RAID_STATUS_RECOVERRING;
			pRaidInfo->BitmapIdxToRecover = 0;
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
		}
		else
		{
			KDPrintM(DBG_LURN_ERROR, ("Failed to revive LURN. %x\n", status));
			continue;
		}

		// use NOOP to refresh adapter status
		LurnEvent.LurnId = Lurn->LurnId;
		LurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;
		LurCallBack(Lurn->Lur, &LurnEvent);

		// initialize variables

		// set bitmap where LWSs indicates
		for(i = 0; i < 32; i++)
		{
			BitmapIdxToRecover = (ULONG)pRaidInfo->LWSs[i].logicalBlockAddress;
			if(BitmapIdxToRecover >= Bitmap->SizeOfBitMap * SectorsPerBit)
			{
				ASSERT(FALSE);
				continue;
			}

			RtlSetBits(Bitmap, (ULONG)BitmapIdxToRecover / 128, 2);
		}

		// find first set bit
		ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
		BitmapIdxToRecover = RtlFindSetBits(Bitmap, 1, 0);
		pRaidInfo->BitmapIdxToRecover = BitmapIdxToRecover;
		RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
		BitmapIdxToRecovered = 0xFFFFFFFF;

		if(0xFFFFFFFF == BitmapIdxToRecover)
		{
			// do not recover if there is no set bit
			KDPrintM(DBG_LURN_ERROR, ("Nothing to recover\n"));
			pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;
			//			KeSetEvent(&pRaidInfo->ThreadReadyEvent, IO_NO_INCREMENT, FALSE);
			continue;
		}


		// now ready to start recovering

		KDPrintM(DBG_LURN_ERROR, ("STARTS RECOVERING -> %08lx\n",	BitmapIdxToRecover));
		do{
			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
			LurnRunning = LURN_IS_RUNNING(Lurn->LurnStatus);
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			if(!LurnRunning)
				break;

			ASSERT(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus);

			// AING_TO_DO : should be loop if SectorsPerBit > 128

			for(RecoverLoop = 0; RecoverLoop < RecoverLoopMax; RecoverLoop++)
			{
				startBlockAddress = BitmapIdxToRecover * SectorsPerBit + RecoverLoop * transferBlocks;

				if(startBlockAddress >= Lurn->UnitBlocks)
					continue;

				if(startBlockAddress + transferBlocks < (UINT32)Lurn->UnitBlocks)
					transferBlocksToRecover = transferBlocks;
				else 
					transferBlocksToRecover = transferBlocks - (UINT16)(startBlockAddress + transferBlocks - (UINT32)Lurn->UnitBlocks);


				// READ sectors from the healthy LURN
				status = LurnExecuteSync(
					Lurn->LurnChildrenCnt -1,
					LurnsHealthy,
					SCSIOP_READ,
					pBufferRead,
					startBlockAddress,
					transferBlocksToRecover,
					NULL);

				if(!NT_SUCCESS(status))
				{
					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY; // not sure
					break;
				}

				// create BufferRecover
				if(LURN_RAID4 == Lurn->LurnType)
				{
					// parity work
					RtlCopyMemory(BufferRecover, pBufferRead[0], transferBlocks * BLOCK_SIZE);
					for(i = 1; i < Lurn->LurnChildrenCnt -1; i++)
					{
						pDataBufferToRecover = (PULONG)BufferRecover;
						pDataBufferSrc = (PULONG)pBufferRead[i];

						j = (transferBlocksToRecover * BLOCK_SIZE) / sizeof(ULONG);
						while(j--)
						{
							*pDataBufferToRecover ^= *pDataBufferSrc;
							pDataBufferToRecover++;
							pDataBufferSrc++;
						}
					}
				}
				else if(LURN_RAID1 == Lurn->LurnType)
				{
					// just copy
				}

				// WRITE sectors to the defected LURN
				status = LurnExecuteSync(
					1,
					&LurnDefected,
					SCSIOP_WRITE,
					&BufferRecover,
					startBlockAddress,
					transferBlocksToRecover,
					NULL);

				if(!NT_SUCCESS(status))
				{
					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY; // not sure
					break;
				}

			}
			if( STATUS_SUCCESS != status  ||
				pRaidInfo->RaidStatus != RAID_STATUS_RECOVERRING)
				break;

			// prepare for next repair

			// seek the first corrupted sector
			BitmapIdxToRecovered = BitmapIdxToRecover;
			RtlClearBits(Bitmap, BitmapIdxToRecover, 1);
			ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
			BitmapIdxToRecover = RtlFindSetBits(Bitmap, 1, BitmapIdxToRecover);
			pRaidInfo->BitmapIdxToRecover = BitmapIdxToRecover;
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

			KDPrintM(DBG_LURN_ERROR, ("-> %08lx\n", BitmapIdxToRecover));

			if(BitmapIdxToRecover / (512 * 8) != BitmapIdxToRecovered / (512 * 8))
			{

				// 1 sector of bitmap in memory is cleared(or recovery complete), clears Bitmap in device
				KDPrintM(DBG_LURN_ERROR, ("<<< Clear a bitmap sector >>> : %08lx\n", BitmapIdxToRecovered));
				// add extended command to clear the cleared bitmap sector
				RtlZeroMemory(ExtendedCommand, sizeof(CMD_BYTE_OP));
				RtlZeroMemory(&OneSector, sizeof(OneSector));

				ExtendedCommand->Operation = CCB_EXT_WRITE;
				ExtendedCommand->logicalBlockAddress = 
					(ULONG)pRaidInfo->Children[pRaidInfo->iChildRecoverInfo].SectorBitmapStart + 
					(BitmapIdxToRecovered / (BLOCK_SIZE * 8));
				ExtendedCommand->ByteOperation = EXT_BLOCK_OPERATION;
				ExtendedCommand->pByteData = OneSector;
				ExtendedCommand->LengthBlock = 1;
				ExtendedCommand->pLurnCreated = Lurn;

				status = LurnExecuteSync(
					1,
					&LurnRecoverInfo,
					SCSIOP_WRITE,
					&BufferRecover,
					0,
					0,
					(PCMD_COMMON)ExtendedCommand);

				if(!NT_SUCCESS(status))
				{
					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY; // not sure
					break;
				}

			}

			// No more to recover
			if(0xFFFFFFFF == BitmapIdxToRecover)
			{
				// recover Last written sector information
				RtlZeroMemory(ExtendedCommand, sizeof(CMD_BYTE_OP));
				RtlZeroMemory(&OneSector, sizeof(OneSector));

				ExtendedCommand->Operation = CCB_EXT_READ;
				ExtendedCommand->logicalBlockAddress = 
					(ULONG)pRaidInfo->Children[pRaidInfo->iChildRecoverInfo].SectorLastWrittenSector;
				ExtendedCommand->ByteOperation = EXT_BLOCK_OPERATION;
				ExtendedCommand->pByteData = OneSector;
				ExtendedCommand->LengthBlock = 1;
				ExtendedCommand->pLurnCreated = Lurn;

				KDPrintM(DBG_LURN_ERROR, ("<<< Clear last written sector : READ >>> : %08lx\n", ExtendedCommand->logicalBlockAddress));

				status = LurnExecuteSync(
					1,
					&LurnRecoverInfo,
					SCSIOP_WRITE,
					&BufferRecover,
					0,
					0,
					(PCMD_COMMON)ExtendedCommand);

				if(!NT_SUCCESS(status))
				{
					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY; // not sure
					break;
				}

				RtlZeroMemory(ExtendedCommand, sizeof(CMD_BYTE_OP));
				ExtendedCommand->Operation = CCB_EXT_WRITE;
				ExtendedCommand->logicalBlockAddress = 
					(ULONG)pRaidInfo->Children[pRaidInfo->iChildDefected].SectorLastWrittenSector;
				ExtendedCommand->ByteOperation = EXT_BLOCK_OPERATION;
				ExtendedCommand->pByteData = OneSector;
				ExtendedCommand->LengthBlock = 1;
				ExtendedCommand->pLurnCreated = Lurn;

				KDPrintM(DBG_LURN_ERROR, ("<<< Clear last written sector : WRITE >>> : %08lx\n", ExtendedCommand->logicalBlockAddress));

				status = LurnExecuteSync(
					1,
					&LurnDefected,
					SCSIOP_WRITE,
					&BufferRecover,
					0,
					0,
					(PCMD_COMMON)ExtendedCommand);

				if(!NT_SUCCESS(status))
				{
					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY; // not sure
					break;
				}

				// failed to find set bits -> recovery complete
				KDPrintM(DBG_LURN_ERROR, ("!!! RECOVERY COMPLETE !!!\n"));

				pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;

				break;
			} // if(0xFFFFFFFF == BitmapIdxToRecover)
		}while(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus); // recover loop

		KDPrintM(DBG_LURN_ERROR, ("exit recover loop. RaidStatus : %08lx \n", pRaidInfo->RaidStatus));

		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
		LurnRunning = LURN_IS_RUNNING(Lurn->LurnStatus);
		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		// use NOOP to refresh adapter status
		LurnEvent.LurnId = Lurn->LurnId;
		LurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;
		LurCallBack(Lurn->Lur, &LurnEvent);
	}while(LurnRunning); // wait loop

out:
	// terminate thread
	if(ExtendedCommand)
	{
		ExFreePoolWithTag(ExtendedCommand, RAID_RECOVER_POOL_TAG);
		ExtendedCommand = NULL;
	}

	if(BufferRead)
	{
		if(LURN_RAID4 == Lurn->LurnType)
		{
			ExFreePoolWithTag(BufferRead, RAID_RECOVER_POOL_TAG);
		}
		else if(LURN_RAID1 == Lurn->LurnType)
		{
			ASSERT(BufferRead == BufferRecover);
		}

		BufferRead = NULL;
	}

	if(BufferRecover)
	{
		ExFreePoolWithTag(BufferRecover, RAID_RECOVER_POOL_TAG);
		BufferRecover = NULL;
	}

	KDPrintM(DBG_LURN_ERROR, ("Terminated\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
	return;
}

//////////////////////////////////////////////////////////////////////////
//
//	Aggregation Lurn
//
NTSTATUS
LurnAggrCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;

	//
	//	Higher number of priority will overwrite CcbStatus.
	//
	//	
	//	Aggregation priority
	//
	//	CCB_STATUS_SUCCESS	: 0
	//	CCB_STATUS_BUSY		: 1
	//	Other error code	: 2
	//	CCB_STATUS_STOP		: 3
	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:	// prority 0
		break;

	case CCB_STATUS_BUSY:		// prority 1
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;
	case CCB_STATUS_STOP:		// prority 3
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
		break;
	default:					// prority 2
		if(OriginalCcb->CcbStatus != CCB_STATUS_STOP) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}
	LSCcbSetStatusFlag(	OriginalCcb,
					Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
			);

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//
	//	Complete the original CCB
	//

	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		return STATUS_SUCCESS;
	}

	//
	//	post-process for CCB_OPCODE_UPDATE
	//
	if(OriginalCcb->OperationCode == CCB_OPCODE_UPDATE &&
		OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS
		) {
			PLURN_UPDATE	LurnUpdate;
			LurnUpdate = (PLURN_UPDATE)OriginalCcb->DataBuffer;

			if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID) {
				PLURELATION_NODE	Lurn;
				//
				//	If this is root LURN, update LUR access right.
				//
				Lurn = OriginalCcb->CcbCurrentStackLocation->Lurn;
				ASSERT(Lurn);

				if(Lurn->LurnParent == 0 || Lurn->LurnParent == Lurn) {
					Lurn->Lur->GrantedAccess |= GENERIC_WRITE;
					KDPrintM(DBG_LURN_INFO,("Updated Lur->GrantedAccess: %08lx\n",Lurn->Lur->GrantedAccess));
				}
			}
	}

	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnAggrExecute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) {
	ULONG				idx_child;
	NTSTATUS			status;
	PLURELATION_NODE	ChildLurn;

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_VERIFY:  {
		UINT64				startBlockAddress, endBlockAddress;
		USHORT				transferBlocks;
		ASSERT(Ccb->CdbLength <= MAXIMUM_CDB_SIZE);

		startBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);		
		transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

		endBlockAddress = startBlockAddress + transferBlocks - 1;

		if(transferBlocks == 0) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			return STATUS_SUCCESS;
		}
		if(endBlockAddress > Lurn->EndBlockAddr) {
			KDPrintM(DBG_LURN_ERROR,("SCSIOP_WRITE/READ/VERIFY: Ccb's ending sector:%ld, Lurn's ending sector:%ld\n", endBlockAddress, Lurn->EndBlockAddr));
			LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		//
		//	Find the target LURN.
		//
		for(idx_child = 0; idx_child < Lurn->LurnChildrenCnt; idx_child ++) {
			ChildLurn = Lurn->LurnChildren[idx_child];

			if( startBlockAddress >= ChildLurn->StartBlockAddr &&
				startBlockAddress <= ChildLurn->EndBlockAddr) {
				break;
			}
		}
		if(idx_child >= Lurn->LurnChildrenCnt) {
			KDPrintM(DBG_LURN_ERROR,("SCSIOP_WRITE/READ/VERIFY: Could not found child LURN. Ccb's ending sector:%ld\n", startBlockAddress));
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			status = STATUS_SUCCESS;
			break;
		}

		//
		//	determine if need to split the CCB.
		//
		if(endBlockAddress <= ChildLurn->EndBlockAddr) {
			PCCB		NextCcb;
			PCDB		pCdb;
			//
			//	One CCB
			//	Allocate one CCB for the children
			//
			KDPrintM(DBG_LURN_TRACE,("SCSIOP_WRITE/READ/VERIFY: found LURN#%d\n", idx_child));

			status = LSCcbAllocate(&NextCcb);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("SCSIOP_WRITE/READ/VERIFY: LSCcbAllocate failed.\n"));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			Ccb->AssociateCount = 1;
			LSCcbInitializeByCcb(Ccb, Lurn->LurnChildren[idx_child], 0, NextCcb);
			LSCcbSetFlag(NextCcb, CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
			LSCcbSetFlag(NextCcb, Ccb->Flags&CCB_FLAG_SYNCHRONOUS);
			NextCcb->AssociateID = (USHORT)idx_child;
			LSCcbSetCompletionRoutine(NextCcb, LurnAggrCcbCompletion, Ccb);

			// start address
			startBlockAddress -= ChildLurn->StartBlockAddr;
			pCdb = (PCDB)&NextCcb->Cdb[0];

			CDB10_LOGICAL_BLOCK_BYTE_TO_BYTES(pCdb, startBlockAddress);

			status = LurnRequest(ChildLurn, NextCcb);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("SCSIOP_WRITE/READ/VERIFY: LurnRequest to Child#%d failed.\n", idx_child));
				LSCcbFree(NextCcb);
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				LSCcbCompleteCcb(Ccb);
				break;
			}

		} else {
			PCCB		NextCcb[2];
			UINT64		firstStartBlockAddress;
			USHORT		firstTransferBlocks;
			USHORT		secondTransferBlocks;
			PCDB		pCdb;
			LONG		idx_ccb;
			UINT64		BlockAddress_0;

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_WRITE/READ/VERIFY: found LURN#%d, #%d\n", idx_child, idx_child+1));
			if(idx_child+1 >= Lurn->LurnChildrenCnt) {
				KDPrintM(DBG_LURN_ERROR,("SCSIOP_WRITE/READ/VERIFY: TWO CCB: no LURN#%d\n", idx_child+1));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			//
			//	Two CCB
			//	Allocate Two CCBs for the children
			//
			for(idx_ccb = 0; idx_ccb < 2; idx_ccb++) {
				status = LSCcbAllocate(&NextCcb[idx_ccb]);
				if(!NT_SUCCESS(status)) {
					LONG	idx;

					KDPrintM(DBG_LURN_ERROR, ("SCSIOP_WRITE/READ/VERIFY: LSCcbAllocate failed.\n"));
					for(idx = 0; idx < idx_ccb; idx++) {
						LSCcbFree(NextCcb[idx]);
					}

					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					LSCcbCompleteCcb(Ccb);
					return STATUS_SUCCESS;
				}
				LSCcbInitializeByCcb(Ccb, Lurn->LurnChildren[idx_ccb], 0, NextCcb[idx_ccb]);
				LSCcbSetFlag(NextCcb[idx_ccb], CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
				LSCcbSetFlag(NextCcb[idx_ccb], Ccb->Flags&CCB_FLAG_SYNCHRONOUS);
				NextCcb[idx_ccb]->AssociateID = (USHORT)(idx_child + idx_ccb);
				LSCcbSetCompletionRoutine(NextCcb[idx_ccb], LurnAggrCcbCompletion, Ccb);
			}

			//
			//	set associate counter
			//
			Ccb->AssociateCount = 2;
			//
			//	first LURN
			//
			ChildLurn = Lurn->LurnChildren[idx_child];
			pCdb = (PCDB)&NextCcb[0]->Cdb[0];

			// start address
			firstStartBlockAddress = startBlockAddress - ChildLurn->StartBlockAddr;
			CDB10_LOGICAL_BLOCK_BYTE_TO_BYTES(pCdb, firstStartBlockAddress);

			// transfer length
			firstTransferBlocks = (USHORT)(ChildLurn->EndBlockAddr - startBlockAddress + 1);
			CDB10_TRANSFER_BLOCKS_TO_BYTES(pCdb, firstTransferBlocks);

			NextCcb[0]->DataBufferLength = firstTransferBlocks * BLOCK_SIZE;
			NextCcb[0]->AssociateID = 0;

			status = LurnRequest(ChildLurn, NextCcb[0]);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("SCSIOP_WRITE/READ/VERIFY: LurnRequest to the first child#%d failed.\n", idx_child));

				LSCcbFree(NextCcb[0]);
				LSCcbFree(NextCcb[1]);

				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				LSCcbCompleteCcb(Ccb);
				break;
			}

			//
			//	second LURN
			//
			ChildLurn = Lurn->LurnChildren[idx_child + 1];
			pCdb = (PCDB)&NextCcb[1]->Cdb[0];

			// start address
			BlockAddress_0 = 0;
			CDB10_LOGICAL_BLOCK_BYTE_TO_BYTES(pCdb, BlockAddress_0);
			// transfer length
			secondTransferBlocks = transferBlocks - firstTransferBlocks;
			ASSERT(secondTransferBlocks > 0);
			CDB10_TRANSFER_BLOCKS_TO_BYTES(pCdb, secondTransferBlocks);
			NextCcb[1]->DataBufferLength = secondTransferBlocks * BLOCK_SIZE;
			NextCcb[1]->DataBuffer = ((PUCHAR)Ccb->DataBuffer) + (firstTransferBlocks * BLOCK_SIZE);	// offset 18
			NextCcb[1]->AssociateID = 1;

			status = LurnRequest(ChildLurn, NextCcb[1]);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("SCSIOP_WRITE/READ/VERIFY: LurnRequest to the child#%d failed.\n", idx_child));
				LSCcbSetStatus(NextCcb[1], CCB_STATUS_INVALID_COMMAND);
				LSCcbSetNextStackLocation(NextCcb[1]);
				LSCcbCompleteCcb(NextCcb[1]);
				status = STATUS_SUCCESS;
				break;
			}

		}
		break;
	}

	case SCSIOP_INQUIRY: {
		INQUIRYDATA			inquiryData;
		UCHAR				Model[16] = AGGR_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));

		RtlZeroMemory(Ccb->DataBuffer, Ccb->DataBufferLength);
		RtlZeroMemory(&inquiryData, sizeof(INQUIRYDATA));

		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
		inquiryData.DeviceTypeModifier;
		inquiryData.RemovableMedia = FALSE;
		inquiryData.Versions = 2;
        inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;
		inquiryData.NormACA;
//		inquiryData.TerminateTask;
		inquiryData.AERC;
		inquiryData.AdditionalLength = 31;	// including ProductRevisionLevel.
//		inquiryData.MediumChanger;
//		inquiryData.MultiPort;
//		inquiryData.EnclosureServices;
		inquiryData.SoftReset;
		inquiryData.CommandQueue;
		inquiryData.LinkedCommands;
		inquiryData.RelativeAddressing;

		RtlCopyMemory(
			inquiryData.VendorId,
			NDAS_DISK_VENDOR_ID,
			(strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8
			);
		
		RtlCopyMemory(
			inquiryData.ProductId,
			Model,
			16
			);

		RtlCopyMemory(
					inquiryData.ProductRevisionLevel,
					PRODUCT_REVISION_LEVEL,
					(strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  
							(strlen(PRODUCT_REVISION_LEVEL)+1) : 4
					);

		RtlMoveMemory (
					Ccb->DataBuffer,
					&inquiryData,
					Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? 
					sizeof (INQUIRYDATA) : 
					Ccb->DataBufferLength
					);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	case SCSIOP_READ_CAPACITY: 
	{
		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_MODE_SENSE:
	{
		PCDB	Cdb;
		PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
		PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));

		RtlZeroMemory(
			Ccb->DataBuffer,
			sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK)
			);
		Cdb = (PCDB)Ccb->Cdb;
		if(Cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) {	// all pages
			ULONG	BlockCount;
			//
			// Make Header.
			//
			parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK) - sizeof(parameterHeader->ModeDataLength);
			parameterHeader->MediumType = 00;	// Default medium type.
			
			if(!(Lurn->AccessRight & GENERIC_WRITE)) {
				KDPrintM(DBG_LURN_INFO,
				("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n"));
				parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;

				if(Lurn->Lur->LurFlags & LURFLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;

			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->NumberOfBlocks[0] = (BYTE)(BlockCount>>16);
			parameterBlock->NumberOfBlocks[1] = (BYTE)(BlockCount>>8);
			parameterBlock->NumberOfBlocks[2] = (BYTE)(BlockCount);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
		} else {

			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		}
		break;
	}

	default: {
		//
		//	send to all child LURNs.
		//
		status = LurnAssocSendCcbToAllChildren(
												Lurn,
												Ccb,
												LurnAggrCcbCompletion,
												NULL,
												NULL,
												FALSE
								);
		break;
		}
	}

	return STATUS_SUCCESS;
}



NTSTATUS
LurnAggrRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS				status;

	//
	//	dispatch a request
	//
	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnAggrExecute(Lurn, Ccb);
		break;

	//
	//	Send to all LURNs
	//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_STOP:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_RECOVER:
	case CCB_OPCODE_NOOP:
		{

			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnAggrCcbCompletion, NULL, NULL, FALSE);
			break;
		}

	case CCB_OPCODE_UPDATE:
		{

			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnAggrCcbCompletion, NULL, NULL, TRUE);
			break;
		}
	
	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	return STATUS_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
//	Mirroring Lurn
//
NTSTATUS
LurnFaultTolerantCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;

	//
	//	Higher number of priority will overwrite CcbStatus.
	//
	//	
	//	Mirroring priority
	//
	//	Other error codes when one child is in error							: 0
	//	CCB_STATUS_STOP when one child is in error								: 1
	//	CCB_STATUS_SUCCESS														: 2
	//	CCB_STATUS_BUSY															: 3
	//	Other error codes when both children are in error						: 4
	//	CCB_STATUS_STOP/CCB_STATUS_NOT_EXIST when both children are in error	: 5
	//
	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);
	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:	// priority 2
		break;

	case CCB_STATUS_BUSY:		// priority 3
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:		// priority 1/5

		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP)) {
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP);	// priority 1
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE);
		} else {
			//
			//	Two children stopped!
			//
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
		}
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE)) {
			//
			//	Two children stopped!
			//
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
			break;
		}
	default:					// priority 0/4
		KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d\n",
								(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID));

		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR)) {
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR);	// priority 0
		} else {
			//
			//	Two children have an error!
			//
			OriginalCcb->CcbStatus = Ccb->CcbStatus;	// 	// priority 4
		}
		break;
	}

	LSCcbSetStatusFlag(	OriginalCcb,
						Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
		);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);


	//
	//	Complete the original CCB
	//
	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		return STATUS_SUCCESS;
	}
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}


NTSTATUS
LurnMirrReadCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;

	ASSERT(OriginalCcb->AssociateCount == 0);

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);
	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:
		LSCcbSetStatusFlag(	OriginalCcb,
					Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
			);
		break;

	case CCB_STATUS_BUSY:
	case CCB_STATUS_STOP:
	default:
		//
		//	Retry again whatever error is occured.
		//	LurnMirrExecute() will stop retrying when no child LURN is working.
		//
		KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d\n", (int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID));

		LSCcbSetStatus(OriginalCcb, CCB_STATUS_BUSY);
		break;
	}

	LSCcbSetStatusFlag(OriginalCcb, Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	return STATUS_SUCCESS;
}


NTSTATUS
LurnMirrUpdateCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;

	//
	//	Higher number of priority will overwrite CcbStatus.
	//
	//	
	//	Mirroring Update status priority
	//
	//	CCB_STATUS_SUCCESS	: 0
	//	Other error code	: 1
	//	CCB_STATUS_STOP		: 2
	//	CCB_STATUS_BUSY		: 3
	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:	// prority 0
		break;
	case CCB_STATUS_STOP:		// prority 2

		if(OriginalCcb->CcbStatus != CCB_STATUS_BUSY) {
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;

		}
		break;

	case CCB_STATUS_BUSY:		// prority 3
			//
			//	We allow CCB_STATUS_BUSY when SRB exists.
			//
			ASSERT(OriginalCcb->Srb);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		break;
	default:					// prority 1
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}

	LSCcbSetStatusFlag(	OriginalCcb,
					Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
			);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//
	//	Complete the original CCB
	//
	KDPrintM(DBG_LURN_INFO,("OriginalCcb:%p. OrignalCcb->StatusFlags:%08lx\n",
									OriginalCcb, OriginalCcb->CcbStatusFlags));
	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		KDPrintM(DBG_LURN_INFO,("Ass:%d Ccb:%p Ccb->CcbStatus:%x Ccb->StatusFlags:%08lx Ccb->AssociateID#%d\n",
									ass, Ccb, Ccb->CcbStatus, Ccb->CcbStatusFlags, Ccb->AssociateID));
		return STATUS_SUCCESS;
	}
	KDPrintM(DBG_LURN_INFO,("OriginalCcb:%p Completed. Ccb->AssociateID#%d\n",
										OriginalCcb, Ccb->AssociateID));

	KDPrintM(DBG_LURN_INFO,("OriginalCcb->OperationCode = %08x, OriginalCcb->CcbStatus = %08x, LurnUpdate->UpdateClass = %08x\n",
		OriginalCcb->OperationCode, OriginalCcb->CcbStatus, ((PLURN_UPDATE)OriginalCcb->DataBuffer)->UpdateClass));

	//
	//	post-process for CCB_OPCODE_UPDATE
	//
	if(OriginalCcb->OperationCode == CCB_OPCODE_UPDATE &&
		OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS
		) {
			PLURN_UPDATE	LurnUpdate;
			LurnUpdate = (PLURN_UPDATE)OriginalCcb->DataBuffer;

			if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID) {
				PLURELATION_NODE	Lurn;
				//
				//	If this is root LURN, update LUR access right.
				//
				Lurn = OriginalCcb->CcbCurrentStackLocation->Lurn;
				ASSERT(Lurn);

				if(Lurn->LurnParent == 0 || Lurn->LurnParent == Lurn) {
					Lurn->Lur->GrantedAccess |= GENERIC_WRITE;
					Lurn->AccessRight |= GENERIC_WRITE;
					KDPrintM(DBG_LURN_INFO,("Updated Lur->GrantedAccess: %08lx\n",Lurn->Lur->GrantedAccess));
				}

				// set event to read bitmap & LWSs
				if(LURN_RAID1 == Lurn->LurnType || LURN_RAID4 == Lurn->LurnType)
				{
					KDPrintM(DBG_LURN_ERROR,("Set recover thread event to read bitmap & LWSs\n"));

					ACQUIRE_SPIN_LOCK(&Lurn->LurnRAIDInfo->LockInfo, &oldIrql);
					Lurn->LurnRAIDInfo->RaidStatus = RAID_STATUS_INITIAILIZING;
					KeSetEvent(&Lurn->LurnRAIDInfo->RecoverThreadEvent, IO_NO_INCREMENT, FALSE);
					RELEASE_SPIN_LOCK(&Lurn->LurnRAIDInfo->LockInfo, oldIrql);
				}
			}
	}

#if DBG
	if(OriginalCcb->OperationCode == CCB_OPCODE_UPDATE && OriginalCcb->CcbStatus == CCB_STATUS_BUSY) {
		KDPrintM(DBG_LURN_INFO,("CCB_OPCODE_UPDATE: return CCB_STATUS_BUSY\n"));
	}
#endif

	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnMirrExecute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) {
	NTSTATUS			status;

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
	case SCSIOP_VERIFY:
	{
		//
		//	send to all child LURNs.
		//
		status = LurnAssocSendCcbToAllChildren(
												Lurn,
												Ccb,
												LurnFaultTolerantCcbCompletion,
												NULL,
												NULL,
												FALSE
								);
		break;
		}
	case 0x3E:		// READ_LONG
	case SCSIOP_READ: {
		ULONG				idx_child;
		PLURELATION_NODE	ChildLurn;
		KIRQL				oldIrql;
		//
		//	Find a child LURN to run.
		//
		idx_child = 0;
		ASSERT(Lurn->LurnChildrenCnt);
		for(idx_child = 0; idx_child<Lurn->LurnChildrenCnt; idx_child ++) {
			ChildLurn = Lurn->LurnChildren[idx_child];

			ACQUIRE_SPIN_LOCK(&ChildLurn->LurnSpinLock, &oldIrql);
			if(LURN_IS_RUNNING(ChildLurn->LurnStatus)) {
				RELEASE_SPIN_LOCK(&ChildLurn->LurnSpinLock, oldIrql);
				break;
			}
			RELEASE_SPIN_LOCK(&ChildLurn->LurnSpinLock, oldIrql);
		}
		if(idx_child >= Lurn->LurnChildrenCnt) {
			// failed to find running child
			KDPrintM(DBG_LURN_ERROR, ("SCSIOP_READ: No available child to run.\n"));

			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

			if(LURN_IS_RUNNING(Lurn->LurnStatus)) {

				Lurn->LurnStatus = LURN_STATUS_STOP;
				RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

				LSCcbSetStatus(Ccb, CCB_STATUS_STOP);
				LSCcbCompleteCcb(Ccb);
				break;
			} else {

				RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

				LSCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
				LSCcbCompleteCcb(Ccb);
				break;
			}
		}

		// ChildLurn is the running child

		KDPrintM(DBG_LURN_TRACE,("SCSIOP_READ: decided LURN#%d\n", idx_child));
		//
		//	Set completion routine
		//
		LSCcbSetCompletionRoutine(Ccb, LurnMirrReadCcbCompletion, Ccb);

		status = LurnRequest(ChildLurn, Ccb);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("SCSIOP_READ: LurnRequest to Child#%d failed.\n", idx_child));
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		break;
	}
	
	case SCSIOP_INQUIRY: {
		INQUIRYDATA			inquiryData;
		UCHAR				Model[16] = MIRR_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));
		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		RtlZeroMemory(Ccb->DataBuffer, Ccb->DataBufferLength);
		RtlZeroMemory(&inquiryData, sizeof(INQUIRYDATA));

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
		inquiryData.DeviceTypeModifier;
		inquiryData.RemovableMedia = FALSE;
		inquiryData.Versions = 2;
        inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;
		inquiryData.NormACA;
//		inquiryData.TerminateTask;
		inquiryData.AERC;
		inquiryData.AdditionalLength = 31;	// including ProductRevisionLevel.
//		inquiryData.MediumChanger;
//		inquiryData.MultiPort;
//		inquiryData.EnclosureServices;
		inquiryData.SoftReset;
		inquiryData.CommandQueue;
		inquiryData.LinkedCommands;
		inquiryData.RelativeAddressing;

		RtlCopyMemory(
			inquiryData.VendorId,
			NDAS_DISK_VENDOR_ID,
			(strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8
			);
		
		RtlCopyMemory(
			inquiryData.ProductId,
			Model,
			16
			);

		RtlCopyMemory(
					inquiryData.ProductRevisionLevel,
					PRODUCT_REVISION_LEVEL,
					(strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  
							(strlen(PRODUCT_REVISION_LEVEL)+1) : 4
					);

		RtlMoveMemory (
					Ccb->DataBuffer,
					&inquiryData,
					Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? 
					sizeof (INQUIRYDATA) : 
					Ccb->DataBufferLength
					);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
		
		status = STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		break;
	}

	case SCSIOP_READ_CAPACITY: 
	{
		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_MODE_SENSE:
	{
		PCDB	Cdb;
		PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
		PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));

		RtlZeroMemory(
			Ccb->DataBuffer,
			sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK)
			);
		Cdb = (PCDB)Ccb->Cdb;
		if(	Cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) {	// all pages
			ULONG	BlockCount;
			//
			// Make Header.
			//
			parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK) - sizeof(parameterHeader->ModeDataLength);
			parameterHeader->MediumType = 00;	// Default medium type.
			
			if(!(Lurn->AccessRight & GENERIC_WRITE)) {
				KDPrintM(DBG_LURN_INFO, 
				("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n"));
				parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;
	
				if(Lurn->Lur->LurFlags & LURFLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(BLOCK_SIZE>>16);
			parameterBlock->BlockLength[1] = (BYTE)(BLOCK_SIZE>>8);
			parameterBlock->BlockLength[2] = (BYTE)(BLOCK_SIZE);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		} else {
			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		}
		break; 
	}

	default:
		//
		//	send to all child LURNs.
		//	Set WriteVerify completion routine to CCB
		//		it guarantees CCB_STATUS_SUCCESS when least one LURN is working fine.
		//
		status = LurnAssocSendCcbToAllChildren(
												Lurn,
												Ccb,
												LurnFaultTolerantCcbCompletion,
												NULL,
												NULL,
												FALSE
								);
		break;

	}

	return STATUS_SUCCESS;
}


NTSTATUS
LurnMirrRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS				status;

	//
	//	dispatch a request
	//
	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnMirrExecute(Lurn, Ccb);
		break;

	//
	//	Send to all LURNs
	//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_STOP:
	case CCB_OPCODE_RECOVER:
	case CCB_OPCODE_NOOP: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnFaultTolerantCcbCompletion, NULL, NULL, FALSE);
		break;
	}
	
	case CCB_OPCODE_UPDATE: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL, TRUE);
		break;
	}

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
//	RAID0 Lurn
//

NTSTATUS
LurnRAID0Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_INFO pRaidInfo = NULL;
	ULONG ulDataBufferSize, ulDataBufferSizePerDisk;
	NTSTATUS ntStatus;
	UINT32 i;
	PRAID_CHILD_INFO pChildInfo;

	UNREFERENCED_PARAMETER(LurnDesc);
//	Raid Information
	Lurn->LurnRAIDInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID_INFO), 
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRAIDInfo)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRAIDInfo;

	RtlZeroMemory(pRaidInfo, sizeof(RAID_INFO));

	pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;

//	Data buffer shuffled
	ulDataBufferSizePerDisk = 128 * BLOCK_SIZE / (Lurn->LurnChildrenCnt);
	ulDataBufferSize = ulDataBufferSizePerDisk * Lurn->LurnChildrenCnt;
	pRaidInfo->DataBufferAllocated = ExAllocatePoolWithTag(NonPagedPool, ulDataBufferSize, 
		RAID_DATA_BUFFER_POOL_TAG);

	if(NULL == pRaidInfo->DataBufferAllocated)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

//	Children information
	for(i = 0; i < (ULONG)Lurn->LurnChildrenCnt; i++)
	{
		pChildInfo = &pRaidInfo->Children[i];

		// 2 disks : each 64 * 512B = 128k / (3 - 1)
		// 4 disks : each 32 * 512B = 128k / (5 - 1)
		// 8 disks : each 16 * 512B = 128k / (9 - 1)
		pChildInfo->DataBuffer =
			(PCHAR)pRaidInfo->DataBufferAllocated + i * ulDataBufferSizePerDisk;

		KDPrintM(DBG_LURN_ERROR, ("pRaidInfo->Children[%d]->DataBuffer = %x\n", i, pRaidInfo->Children[i].DataBuffer));
	}

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAIDInfo)
		{
			if(pRaidInfo->DataBufferAllocated)
			{
				ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG);
				pRaidInfo->DataBufferAllocated = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG);
			Lurn->LurnRAIDInfo = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnRAID0Destroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAIDInfo);

	pRaidInfo = Lurn->LurnRAIDInfo;

	ASSERT(pRaidInfo->DataBufferAllocated);
	ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG) ;
	pRaidInfo->DataBufferAllocated = NULL;

	ASSERT(pRaidInfo);
	ExFreePoolWithTag(pRaidInfo, RAID_INFO_POOL_TAG) ;
	pRaidInfo = NULL;

	return STATUS_SUCCESS ;
}

NTSTATUS
LurnRAID0CcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;
	NTSTATUS status;
	PRAID_INFO pRaidInfo;
	PLURELATION_NODE pLurnOriginal;

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	pLurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	pRaidInfo = pLurnOriginal->LurnRAIDInfo;

	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:
		break;

	case CCB_STATUS_BUSY:
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE)) {
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;
			break;
		}
	default:
		KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d\n",
								(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID));

		OriginalCcb->CcbStatus = Ccb->CcbStatus;
		break;
	}

	LSCcbSetStatusFlag(	OriginalCcb,
						Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
		);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);


	//
	//	Complete the original CCB
	//

	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		return status;
	}

	// shuffle completion
	switch(OriginalCcb->Cdb[0])
	{
	case SCSIOP_WRITE: 
		// release buffer lock
		{
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		}
		break;
	case SCSIOP_VERIFY: 
		// release buffer lock
		{
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_VERIFY\n"));
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		}
		break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
		// deshuffle read buffer before release data buffer
		{
			ULONG BlocksPerDisk;
			ULONG i, j;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

			// create new data buffer and encrypt here.
			// new data buffer will be deleted at completion routine
			BlocksPerDisk = Ccb->DataBufferLength / BLOCK_SIZE;
			
			// deshuffle
			for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
			{
				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory( // Copy back
						(PCHAR)OriginalCcb->DataBuffer + (i + j * (pLurnOriginal->LurnChildrenCnt)) * BLOCK_SIZE,
						(PCHAR)pRaidInfo->Children[i].DataBuffer + j * BLOCK_SIZE,
						BLOCK_SIZE);
				}
			}

			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		}
	}
	
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnRAID0Execute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) {
	NTSTATUS			status;
	PRAID_INFO			pRaidInfo;

	pRaidInfo = Lurn->LurnRAIDInfo;

	// AING_TO_DO : ATM, fixed to 2 + 1.
	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		// lock buffer : release at completion / do not forget to release when fail
		while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;

			int DataBufferLengthPerDisk;
			ULONG BlocksPerDisk;
			register ULONG i, j;
			CUSTOM_DATA_BUFFER cdb;

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));
			
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			// create new data buffer and encrypt here.
			// new data buffer will be deleted at completion routine
			DataBufferLengthPerDisk = Ccb->DataBufferLength / Lurn->LurnChildrenCnt;
			BlocksPerDisk = DataBufferLengthPerDisk / BLOCK_SIZE;

			// shuffle
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory(
						(PCHAR)pRaidInfo->Children[i].DataBuffer + j * BLOCK_SIZE,
						(PCHAR)Ccb->DataBuffer + (i + j * (Lurn->LurnChildrenCnt)) * BLOCK_SIZE,
						BLOCK_SIZE);
				}

			}

			// initialize cdb, LurnChildren
			
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{			
				cdb.DataBuffer[i] = pRaidInfo->Children[i].DataBuffer;
				cdb.DataBufferLength[i] = (UINT32)DataBufferLengthPerDisk;
			}

			cdb.DataBufferCount = i;

			//
			//	send to all child LURNs.
			//
			status = LurnAssocSendCcbToAllChildren(
													Lurn,
													Ccb,
													LurnRAID0CcbCompletion,
													&cdb,
													NULL,
													FALSE
									);
		}
		break;
	case SCSIOP_VERIFY:
		{
			// lock buffer : release at completion
			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);

			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID0CcbCompletion,
				NULL,
				NULL,
				FALSE
				);
		}
		break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
		{
			int DataBufferLengthPerDisk;
			ULONG i;
			CUSTOM_DATA_BUFFER cdb;
			
			// lock buffer : release at completion
			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			
			DataBufferLengthPerDisk = Ccb->DataBufferLength / (Lurn->LurnChildrenCnt);
			KDPrintM(DBG_LURN_NOISE,("R Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));
			
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				cdb.DataBuffer[i] = pRaidInfo->Children[i].DataBuffer;
				cdb.DataBufferLength[i] = (UINT32)DataBufferLengthPerDisk;
			}
			
			cdb.DataBufferCount = i;
			
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID0CcbCompletion,
				&cdb,
				NULL,
				FALSE
				);
		}
		break;
	case SCSIOP_INQUIRY: {
		INQUIRYDATA			inquiryData;
		UCHAR				Model[16] = RAID0_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));
		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		RtlZeroMemory(Ccb->DataBuffer, Ccb->DataBufferLength);
		RtlZeroMemory(&inquiryData, sizeof(INQUIRYDATA));

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
		inquiryData.DeviceTypeModifier;
		inquiryData.RemovableMedia = FALSE;
		inquiryData.Versions = 2;
        inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;
		inquiryData.NormACA;
//		inquiryData.TerminateTask;
		inquiryData.AERC;
		inquiryData.AdditionalLength = 31;	// including ProductRevisionLevel.
//		inquiryData.MediumChanger;
//		inquiryData.MultiPort;
//		inquiryData.EnclosureServices;
		inquiryData.SoftReset;
		inquiryData.CommandQueue;
		inquiryData.LinkedCommands;
		inquiryData.RelativeAddressing;

		RtlCopyMemory(
			inquiryData.VendorId,
			NDAS_DISK_VENDOR_ID,
			(strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8
			);
		
		RtlCopyMemory(
			inquiryData.ProductId,
			Model,
			16
			);

		RtlCopyMemory(
					inquiryData.ProductRevisionLevel,
					PRODUCT_REVISION_LEVEL,
					(strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  
							(strlen(PRODUCT_REVISION_LEVEL)+1) : 4
					);

		RtlMoveMemory (
					Ccb->DataBuffer,
					&inquiryData,
					Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? 
					sizeof (INQUIRYDATA) : 
					Ccb->DataBufferLength
					);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
		
		status = STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		break;
	}

	case SCSIOP_READ_CAPACITY: 
	{
		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		blockSize *= Lurn->LurnChildrenCnt;

		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;

		KDPrintM(DBG_LURN_ERROR, ("SCSIOP_READ_CAPACITY: %08x : %04x\n", (UINT32)logicalBlockAddress, (UINT32)blockSize));
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_MODE_SENSE:
	{
		PCDB	Cdb;
		PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
		PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));

		RtlZeroMemory(
			Ccb->DataBuffer,
			sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK)
			);
		Cdb = (PCDB)Ccb->Cdb;
		if(	Cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) {	// all pages
			ULONG	BlockCount;
			//
			// Make Header.
			//
			parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK) - sizeof(parameterHeader->ModeDataLength);
			parameterHeader->MediumType = 00;	// Default medium type.
			
			if(!(Lurn->AccessRight & GENERIC_WRITE)) {
				KDPrintM(DBG_LURN_INFO, 
				("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n"));
				parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;
	
				if(Lurn->Lur->LurFlags & LURFLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(BLOCK_SIZE>>16);
			parameterBlock->BlockLength[1] = (BYTE)(BLOCK_SIZE>>8);
			parameterBlock->BlockLength[2] = (BYTE)(BLOCK_SIZE);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		} else {
			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		}
		break; 
	}

	default:
		//
		//	send to all child LURNs.
		//	Set WriteVerify completion routine to CCB
		//		it guarantees CCB_STATUS_SUCCESS when least one LURN is working fine.
		//

		status = LurnAssocSendCcbToAllChildren(
												Lurn,
												Ccb,
												LurnRAID0CcbCompletion,
												NULL,
												NULL,
												FALSE
								);
		break;

	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID0Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS				status;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("LurnRAID0Request!\n"));

	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnRAID0Execute(Lurn, Ccb);
		break;

	//
	//	Send to all LURNs
	//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_STOP:
	case CCB_OPCODE_RECOVER:
	case CCB_OPCODE_NOOP:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID0CcbCompletion, NULL, NULL, FALSE);
			break;
		}

	case CCB_OPCODE_UPDATE:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID0CcbCompletion, NULL, NULL, TRUE);
			break;
		}

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
//	RAID1 Lurn
//

NTSTATUS
LurnRAID1Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_INFO pRaidInfo = NULL;
	ULONG ulBitMapSize;
	NTSTATUS ntStatus;
	PRAID_CHILD_INFO pChildInfo;
	ULONG i;
	OBJECT_ATTRIBUTES objectAttributes;

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);

//	Raid Information
	Lurn->LurnRAIDInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID_INFO),
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRAIDInfo)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRAIDInfo;

	RtlZeroMemory(pRaidInfo, sizeof(RAID_INFO));

	pRaidInfo->RaidStatus = RAID_STATUS_INITIAILIZING;

	pRaidInfo->LockDataBuffer = 0;

	//	Children information
	RtlZeroMemory(pRaidInfo->Children, Lurn->LurnChildrenCnt * sizeof(RAID_CHILD_INFO));

	pRaidInfo->MaxBlocksPerRequest = LurnDesc->MaxBlocksPerRequest;

	pRaidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID[0].SectorsPerBit;
	if(pRaidInfo->SectorsPerBit) {
		ulBitMapSize = (UINT32)(sizeof(BYTE) * (Lurn->UnitBlocks / 
			(pRaidInfo->SectorsPerBit * 8))) +1;

	} else {
		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;
	}

	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		pChildInfo = &pRaidInfo->Children[i];

		pChildInfo->DataBuffer = NULL; // do not use

		pChildInfo->SectorBitmapStart = LurnDesc->LurnInfoRAID[i].SectorBitmapStart;
		pChildInfo->SectorLastWrittenSector = (ULONG)LurnDesc->LurnInfoRAID[i].SectorLastWrittenSector;
		if(pRaidInfo->SectorsPerBit != LurnDesc->LurnInfoRAID[i].SectorsPerBit)
		{
			ASSERT(pRaidInfo->SectorsPerBit != LurnDesc->LurnInfoRAID[i].SectorsPerBit);
			goto out;
		}

		pChildInfo->EC_LWS.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
		pChildInfo->EC_LWS.Operation = CCB_EXT_WRITE;
		pChildInfo->EC_LWS.logicalBlockAddress = (ULONG)pChildInfo->SectorLastWrittenSector;
//		pChildInfo->EC_LWS.Offset = 0;
		pChildInfo->EC_LWS.LengthBlock = 1;
		pChildInfo->EC_LWS.ByteOperation = EXT_BLOCK_OPERATION;
		pChildInfo->EC_LWS.pNextCmd = NULL; // vary
		pChildInfo->EC_LWS.pByteData = (PBYTE)pRaidInfo->LWSs;
//		pChildInfo->ByteLastWrittenSector.timeStamp = 0;
	}

	// EC_Bitmap
	pRaidInfo->EC_Bitmap.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
	pRaidInfo->EC_Bitmap.Operation = CCB_EXT_WRITE;
//	pRaidInfo->EC_Bitmap.logicalBlockAddress = ;
	pRaidInfo->EC_Bitmap.Offset = 0;
	pRaidInfo->EC_Bitmap.ByteOperation = EXT_BLOCK_OPERATION;
//	pRaidInfo->EC_Bitmap.pByteData = ;
	pRaidInfo->EC_Bitmap.pNextCmd = NULL; // varis

//	Bitmap (1) * (bitmap structure size + bitmap size)

	pRaidInfo->Bitmap = (PRTL_BITMAP)ExAllocatePoolWithTag(NonPagedPool, 
		ulBitMapSize + sizeof(RTL_BITMAP), BITMAP_POOL_TAG);

	if(NULL == pRaidInfo->Bitmap)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	RtlInitializeBitMap(
		pRaidInfo->Bitmap,
		(PULONG)(pRaidInfo->Bitmap +1), // start address of bitmap data
		ulBitMapSize * 8);

    RtlClearAllBits(pRaidInfo->Bitmap);

	KeInitializeSpinLock(&pRaidInfo->LockInfo);

	// create recover thread
	KeInitializeEvent(&pRaidInfo->RecoverThreadEvent, SynchronizationEvent, FALSE);

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ntStatus = PsCreateSystemThread(
		&pRaidInfo->ThreadRecoverHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		LurnRAIDThreadProcRecover,
		Lurn
		);

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL : LurnRAIDThreadProcRecover !!!\n"));
		ntStatus = STATUS_THREAD_NOT_IN_PROCESS;
		goto out;
	}

	ntStatus = ObReferenceObjectByHandle(
		pRaidInfo->ThreadRecoverHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		&pRaidInfo->ThreadRecoverObject,
		NULL
		);

	ntStatus = STATUS_SUCCESS;

out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAIDInfo)
		{
			if(pRaidInfo->Bitmap)
			{
				ExFreePoolWithTag(pRaidInfo->Bitmap, BITMAP_POOL_TAG);
				pRaidInfo->Bitmap = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG);
			Lurn->LurnRAIDInfo = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnRAID1Destroy(
		PLURELATION_NODE Lurn
	) 
{
	NTSTATUS status;
	LARGE_INTEGER TimeOut;
	PRAID_INFO pRaidInfo;
	KIRQL oldIrql;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAIDInfo);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	if(pRaidInfo->ThreadRecoverHandle)
	{		
		ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
		pRaidInfo->RaidStatus = RAID_STATUS_TERMINATING;
		RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

		KeSetEvent(&pRaidInfo->RecoverThreadEvent,IO_NO_INCREMENT, FALSE);
	}

	TimeOut.QuadPart = - NANO100_PER_SEC * 120;

	status = KeWaitForSingleObject(
		pRaidInfo->ThreadRecoverObject,
		Executive,
		KernelMode,
		FALSE,
		&TimeOut
		);

	ASSERT(NT_SUCCESS(status));

	ObDereferenceObject(pRaidInfo->ThreadRecoverObject);

	ASSERT(pRaidInfo->Bitmap);
	ExFreePoolWithTag(pRaidInfo->Bitmap, BITMAP_POOL_TAG) ;
	pRaidInfo->Bitmap = NULL;

	ExFreePoolWithTag(pRaidInfo, RAID_INFO_POOL_TAG) ;
	pRaidInfo = NULL;

	return STATUS_SUCCESS ;
}

NTSTATUS
LurnRAID1CcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;
	NTSTATUS status;
	PRAID_INFO pRaidInfo;
	PLURELATION_NODE pLurnOriginal;

	status = STATUS_SUCCESS;

	//
	//	Higher number of priority will overwrite CcbStatus.
	//
	//	
	//	Mirroring priority
	//
	//	Other error codes when one child is in error							: 0
	//	CCB_STATUS_STOP when one child is in error								: 1
	//	CCB_STATUS_SUCCESS														: 2
	//	CCB_STATUS_BUSY															: 3
	//	Other error codes when both children are in error						: 4
	//	CCB_STATUS_STOP/CCB_STATUS_NOT_EXIST when both children are in error	: 5
	//
	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	pLurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	ASSERT(pLurnOriginal);
	pRaidInfo = pLurnOriginal->LurnRAIDInfo;

	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:	// priority 2
		break;

	case CCB_STATUS_BUSY:		// priority 3
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:		// priority 1/5
		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP))
		{
/*
*
*	Process when initializing
*	. Create & initialize bitmap, lock
*	. Initialize mirror information
*
*	Process when destroying
*	. Delete bitmap, lock
*	
*	Process when both children is ok
*	. Create extended command to record write log at non-user sector
*	. LurnIdeDiskExecute will process extended command
*
*	Process when a child stop. (completion routin, first time only)
*	. Set raid status(OriginalCcb, pRaidInfo->RaidStatus) to RAID_STATUS_EMERGENCY
*	. Set IDE Lurn status(Ccb) to 'accept bitmapped' write only
*	. enumerate all write data & set busy if the data does not have bitmap
*	. release lock
*
*	Process after a child stop
*	. lock & check bitmap
*	. if RAID_STATUS_EMERGENCY, create extended command
*	. if bitmap changed, create extended command to write bitmap at non-user sector
*	. LurnIdeDiskExecute will process extended command
*	. release lock
*/
			PLURELATION_NODE pLurnChildDefected;
			ULONG i;

			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP);	// priority 1
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE);

			KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, CCBSTATUS_FLAG_LURN_STOP not flagged\n"));

			//////////////////////////////////////////////
			//
			//	Initialize raid information
			//
			{
				// set backup information
				ASSERT(Ccb->CcbCurrentStackLocation);
				ASSERT(Ccb->CcbCurrentStackLocation->Lurn);
				pLurnChildDefected = Ccb->CcbCurrentStackLocation->Lurn;

				ASSERT(OriginalCcb->CcbCurrentStackLocation);
				ASSERT(OriginalCcb->CcbCurrentStackLocation->Lurn);

				ASSERT(LURN_IDE_DISK == pLurnChildDefected->LurnType);
				ASSERT(LURN_RAID1 == pLurnOriginal->LurnType);

				if(!pLurnChildDefected || !pLurnOriginal)
				{
					ASSERT(FALSE);
					status = STATUS_ILLEGAL_INSTRUCTION;
					break;
				}

				ASSERT(pLurnChildDefected->LurnRAIDInfo);

				if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
				{
					ASSERT(FALSE);
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
				}

				// 1 fail + 1 fail = broken
				if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
				{
					if(pLurnChildDefected != pLurnOriginal->LurnChildren[pRaidInfo->iChildDefected])
					{
						ASSERT(FALSE);
//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					}
					break;
				}
				
				pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY;

				for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
				{
					if(pLurnChildDefected == pLurnOriginal->LurnChildren[i])
					{
						pRaidInfo->iChildDefected = i;
						pRaidInfo->iChildRecoverInfo = i ^1;
						break;
					}
				}

				// failed to find a defected child
				if(i == pLurnOriginal->LurnChildrenCnt)
				{
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					break;
				}

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iChildDefected = %d\n", pRaidInfo->iChildDefected));
			}
		} else {
			//
			//	Two children stopped!
			//
			ASSERT(FALSE);
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
		}
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE)) {
			//
			//	Both children stopped.!
			//
			ASSERT(FALSE);
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
			break;
		}
	default:					// priority 0/4
		if(CCB_STATUS_NOT_EXIST != Ccb->CcbStatus)
		{
			KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d, OriginalCcb->AssociateCount = %d\n",
				(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID, OriginalCcb->AssociateCount));
		}

		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR)) {
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR);	// priority 0
		} else {
			//
			//	Two children have an error!
			//
			OriginalCcb->CcbStatus = Ccb->CcbStatus;	// 	// priority 4
		}
		break;
	}

	if(RAID_STATUS_FAIL == pRaidInfo->RaidStatus)
	{
		ASSERT(FALSE);
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
	}

	LSCcbSetStatusFlag(	OriginalCcb,
						Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
						);

	if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
		LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_RECOVERING);

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);


	// clear extended command
//	LSCcbRemoveExtendedCommandTailMatch(&(Ccb->pExtendedCommand), OriginalCcb->CcbCurrentStackLocation->Lurn);

	//
	//	Complete the original CCB
	//
	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		return status;
	}

	if(pRaidInfo->RaidStatus != RAID_STATUS_NORMAL)
	{
		KDPrintM(DBG_LURN_NOISE,("All Ccb complete in abnormal status : %d\n", (int)OriginalCcb->Cdb[0]));
	}
	
	if(CCB_OPCODE_EXECUTE == OriginalCcb->OperationCode)
	{
		switch(OriginalCcb->Cdb[0])
		{
		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_WRITE: 
		case SCSIOP_VERIFY: 
			// release buffer lock
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_ %d\n", (int)OriginalCcb->Cdb[0]));
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			break;
		}
	}

	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnRAID1Execute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) {
	NTSTATUS			status;
	KIRQL				oldIrql;

	PRAID_INFO pRaidInfo;
	PCMD_BYTE_OP	pExtendedCommands[NDAS_MAX_RAID1_CHILD];

	ASSERT(NDAS_MAX_RAID1_CHILD == Lurn->LurnChildrenCnt);
	// initialize extended commands
	RtlZeroMemory(pExtendedCommands, NDAS_MAX_RAID1_CHILD * sizeof(PCMD_BYTE_OP));

	pRaidInfo = Lurn->LurnRAIDInfo;

	// record a bitmap information to the opposite disk of the defected disk
	if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
	{
		ASSERT(pRaidInfo->iChildDefected == 0 || pRaidInfo->iChildDefected == 1);
	}

	// recovery/data buffer protection code
	switch(Ccb->Cdb[0])
	{
	case SCSIOP_WRITE:
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_VERIFY:
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);
			// Busy if this location is under recovering

			ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
			if(
				RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus ||
				(
				RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus &&
				(
				pRaidInfo->BitmapIdxToRecover == logicalBlockAddress / 128 ||
				pRaidInfo->BitmapIdxToRecover == (logicalBlockAddress + transferBlocks -1) / 128
				)
				)
				)
			{
				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

				KDPrintM(DBG_LURN_ERROR, ("!! RECOVER THREAD PROTECTION : %08lx, %d, %d\n", pRaidInfo->RaidStatus, logicalBlockAddress, transferBlocks));
				Ccb->CcbStatus = CCB_STATUS_BUSY;
				LSCcbCompleteCcb(Ccb);
				return STATUS_SUCCESS;
			}

			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

			// lock buffer : release at completion / do not forget to release when fail
			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		}
	}

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		// lock buffer : release at completion / do not forget to release when fail
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;
			register ULONG i;

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			KDPrintM(DBG_LURN_TRACE, ("SCSIOP_WRITE\n"));

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_NORMAL: // write information where writing on.
				// record last written sector
				{
					PCMD_BYTE_LAST_WRITTEN_SECTOR pLastWrittenSector;

					// AING_TO_DO : use Ready made pool
					for(i = 0; i < Lurn->LurnChildrenCnt; i++)
					{
						pExtendedCommands[i] = &pRaidInfo->Children[i].EC_LWS;
						pExtendedCommands[i]->pNextCmd = NULL;

						pLastWrittenSector = &pRaidInfo->LWSs[pRaidInfo->timeStamp % 32];

						pLastWrittenSector->logicalBlockAddress = (UINT64)logicalBlockAddress;
						pLastWrittenSector->transferBlocks = (UINT32)transferBlocks;
						pLastWrittenSector->timeStamp = pRaidInfo->timeStamp;
					}
					pRaidInfo->timeStamp++;
				}
				break;
			case RAID_STATUS_EMERGENCY: // bitmap work
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY\n"));

					// use pRaidInfo->EC_Bitmap instead allocating
					// seek first sector in bitmap
					uiBitmapStartInBits = logicalBlockAddress / 
						pRaidInfo->SectorsPerBit;
					uiBitmapEndInBits = (logicalBlockAddress + transferBlocks -1) /
						pRaidInfo->SectorsPerBit;

					// check if any bits would be changed
					if(!RtlAreBitsSet(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits))
					{
						// bitmap work
						KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY bitmap changed\n"));
						RtlSetBits(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits - uiBitmapStartInBits +1);
						pRaidInfo->EC_Bitmap.pByteData = 
							(PCHAR)(pRaidInfo->Bitmap->Buffer) + (uiBitmapStartInBits / BITS_PER_BLOCK) * BLOCK_SIZE;
						pRaidInfo->EC_Bitmap.LengthBlock = 
							(uiBitmapStartInBits / BITS_PER_BLOCK == uiBitmapEndInBits) ? 1 : 2;
						pRaidInfo->EC_Bitmap.logicalBlockAddress = 
							(ULONG)pRaidInfo->Children[pRaidInfo->iChildRecoverInfo].SectorBitmapStart +
							(uiBitmapStartInBits / BITS_PER_BLOCK);
						pExtendedCommands[pRaidInfo->iChildRecoverInfo] = &pRaidInfo->EC_Bitmap;
					}

					// split is super ultra hyper rare case
					if(pRaidInfo->EC_Bitmap.LengthByte == BLOCK_SIZE * 2)
					{
						KDPrintM(DBG_LURN_ERROR,("RAID_STATUS_EMERGENCY bitmap changed DOUBLE : This is not error but ultra rare case\n"));
						ASSERT(FALSE);
					}
				}
				break;
			case RAID_STATUS_RECOVERRING:
				break;
			default:
				// invalid status
				ASSERT(FALSE);
				break;
			}
		}
		// fall down
	case SCSIOP_VERIFY:
		{
			ASSERT(Ccb->Srb);
			//	send to all child LURNs.
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID1CcbCompletion,
				NULL,
				pExtendedCommands,
				FALSE
				);
				break;
		}
	break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ: {
		ULONG				idx_child;
		PLURELATION_NODE	ChildLurn;
		
		UINT32 logicalBlockAddress;
		UINT16 transferBlocks;

		// Busy if this location is under recovering
		logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
		transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

		//
		//	Find a child LURN to run.
		//

		idx_child = 0;
		ASSERT(Lurn->LurnChildrenCnt == Lurn->LurnChildrenCnt);
		for(idx_child = 0; idx_child<Lurn->LurnChildrenCnt; idx_child ++) {
			ChildLurn = Lurn->LurnChildren[idx_child];

			ACQUIRE_SPIN_LOCK(&ChildLurn->LurnSpinLock, &oldIrql);
			if(LURN_IS_RUNNING(ChildLurn->LurnStatus)) {
				RELEASE_SPIN_LOCK(&ChildLurn->LurnSpinLock, oldIrql);
				break;
			}
			RELEASE_SPIN_LOCK(&ChildLurn->LurnSpinLock, oldIrql);
		}
		if(idx_child >= Lurn->LurnChildrenCnt) {
			KDPrintM(DBG_LURN_ERROR, ("SCSIOP_READ: No available child to run.\n"));

			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

			if(LURN_IS_RUNNING(Lurn->LurnStatus)) {

				Lurn->LurnStatus = LURN_STATUS_STOP;
				RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

				LSCcbSetStatus(Ccb, CCB_STATUS_STOP);
				LSCcbCompleteCcb(Ccb);
				break;
			} else {

				RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

				LSCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
				LSCcbCompleteCcb(Ccb);
				break;
			}
		}

		KDPrintM(DBG_LURN_TRACE,("SCSIOP_READ: decided LURN#%d\n", idx_child));
		//
		//	Set completion routine
		//
		status = LurnAssocSendCcbToChildrenArray(
			&Lurn->LurnChildren[idx_child],
			1,
			Ccb,
			LurnRAID1CcbCompletion,
			NULL,
			NULL,
			FALSE
			);
		break;
	}
	
	case SCSIOP_INQUIRY: {
		INQUIRYDATA			inquiryData;
		UCHAR				Model[16] = RAID1_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));
		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		RtlZeroMemory(Ccb->DataBuffer, Ccb->DataBufferLength);
		RtlZeroMemory(&inquiryData, sizeof(INQUIRYDATA));

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
		inquiryData.DeviceTypeModifier;
		inquiryData.RemovableMedia = FALSE;
		inquiryData.Versions = 2;
        inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;
		inquiryData.NormACA;
//		inquiryData.TerminateTask;
		inquiryData.AERC;
		inquiryData.AdditionalLength = 31;	// including ProductRevisionLevel.
//		inquiryData.MediumChanger;
//		inquiryData.MultiPort;
//		inquiryData.EnclosureServices;
		inquiryData.SoftReset;
		inquiryData.CommandQueue;
		inquiryData.LinkedCommands;
		inquiryData.RelativeAddressing;

		RtlCopyMemory(
			inquiryData.VendorId,
			NDAS_DISK_VENDOR_ID,
			(strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8
			);
		
		RtlCopyMemory(
			inquiryData.ProductId,
			Model,
			16
			);

		RtlCopyMemory(
					inquiryData.ProductRevisionLevel,
					PRODUCT_REVISION_LEVEL,
					(strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  
							(strlen(PRODUCT_REVISION_LEVEL)+1) : 4
					);

		RtlMoveMemory (
					Ccb->DataBuffer,
					&inquiryData,
					Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? 
					sizeof (INQUIRYDATA) : 
					Ccb->DataBufferLength
					);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
		
		status = STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		break;
	}

	case SCSIOP_READ_CAPACITY: 
	{
		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_MODE_SENSE:
	{
		PCDB	Cdb;
		PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
		PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));

		RtlZeroMemory(
			Ccb->DataBuffer,
			sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK)
			);
		Cdb = (PCDB)Ccb->Cdb;
		if(	Cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) {	// all pages
			ULONG	BlockCount;
			//
			// Make Header.
			//
			parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK) - sizeof(parameterHeader->ModeDataLength);
			parameterHeader->MediumType = 00;	// Default medium type.
			
			if(!(Lurn->AccessRight & GENERIC_WRITE)) {
				KDPrintM(DBG_LURN_INFO, 
				("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n"));
				parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;
	
				if(Lurn->Lur->LurFlags & LURFLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(BLOCK_SIZE>>16);
			parameterBlock->BlockLength[1] = (BYTE)(BLOCK_SIZE>>8);
			parameterBlock->BlockLength[2] = (BYTE)(BLOCK_SIZE);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		} else {
			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		}
		break; 
	}

	default:
		//
		//	send to all child LURNs.
		//	Set WriteVerify completion routine to CCB
		//		it guarantees CCB_STATUS_SUCCESS when least one LURN is working fine.
		//
		status = LurnAssocSendCcbToAllChildren(
												Lurn,
												Ccb,
												LurnRAID1CcbCompletion,
												NULL,
												NULL,
												FALSE
								);
		break;

	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID1Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS				status;

	//
	//	dispatch a request
	//
	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnRAID1Execute(Lurn, Ccb);
		break;

	//
	//	Send to all LURNs
	//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_STOP:
	case CCB_OPCODE_NOOP:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID1CcbCompletion, NULL, NULL, FALSE);
			break;
		}

	case CCB_OPCODE_UPDATE:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL, TRUE);
			break;
		}

	case CCB_OPCODE_RECOVER:
		{
			PRAID_INFO pRaidInfo;

			pRaidInfo = Lurn->LurnRAIDInfo;

			KDPrintM(DBG_LURN_ERROR, ("CCB_OPCODE_RECOVER\n"));

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_ERROR, ("Read only => Do not start recovering\n"));
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}

			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);

			if(RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus)
			{
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);

				KDPrintM(DBG_LURN_ERROR, ("pRaidInfo->RaidStatus = %08lx => Do not start recovering\n", pRaidInfo->RaidStatus));
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}

			// start to recover
			KeSetEvent(&pRaidInfo->RecoverThreadEvent, IO_NO_INCREMENT, FALSE);

			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			
			KDPrintM(DBG_LURN_ERROR, ("Recovery initialize complete\n"));
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
		}
		break;

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID4Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_INFO pRaidInfo = NULL;
	ULONG ulBitMapSize, ulDataBufferSize, ulDataBufferSizePerDisk;
	NTSTATUS ntStatus;
	PRAID_CHILD_INFO pChildInfo;
	ULONG i;
	OBJECT_ATTRIBUTES objectAttributes;

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);

//	Raid Information
	Lurn->LurnRAIDInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID_INFO), 
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRAIDInfo)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRAIDInfo;

	RtlZeroMemory(pRaidInfo, sizeof(RAID_INFO));

	pRaidInfo->RaidStatus = RAID_STATUS_INITIAILIZING;

//	Data buffer shuffled
	ulDataBufferSizePerDisk = 128 * BLOCK_SIZE / (Lurn->LurnChildrenCnt -1);
	ulDataBufferSize = ulDataBufferSizePerDisk * Lurn->LurnChildrenCnt;
	pRaidInfo->DataBufferAllocated = ExAllocatePoolWithTag(NonPagedPool, ulDataBufferSize, 
		RAID_DATA_BUFFER_POOL_TAG);

	if(NULL == pRaidInfo->DataBufferAllocated)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo->LockDataBuffer = 0;

//	Children information
/*
	pRaidInfo->Children = ExAllocatePoolWithTag(NonPagedPool, 
		Lurn->LurnChildrenCnt * sizeof(RAID_CHILD_INFO), RAID_INFO_POOL_TAG);
	if(NULL == pRaidInfo->Children)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}
*/
	RtlZeroMemory(pRaidInfo->Children, Lurn->LurnChildrenCnt * sizeof(RAID_CHILD_INFO));

	pRaidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID[0].SectorsPerBit;
	if(pRaidInfo->SectorsPerBit) {
		ulBitMapSize = (UINT32)(sizeof(BYTE) * (Lurn->UnitBlocks / 
			(pRaidInfo->SectorsPerBit * 8))) +1;

	} else {
		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;
	}

	pRaidInfo->MaxBlocksPerRequest = LurnDesc->MaxBlocksPerRequest;

	for(i = 0; i < (ULONG)Lurn->LurnChildrenCnt; i++)
	{
		pChildInfo = &pRaidInfo->Children[i];

		// 3 disks : each 64 * 512B = RAID0_DATA_BUFFER_LENGTH / (3 - 1)
		// 5 disks : each 32 * 512B = RAID0_DATA_BUFFER_LENGTH / (5 - 1)
		// 9 disks : each 16 * 512B = RAID0_DATA_BUFFER_LENGTH / (9 - 1)
		pChildInfo->DataBuffer =
			(PCHAR)pRaidInfo->DataBufferAllocated + i * ulDataBufferSizePerDisk;

		KDPrintM(DBG_LURN_ERROR, ("pRaidInfo->Children[%d]->DataBuffer = %x\n", i, pRaidInfo->Children[i].DataBuffer));

		pChildInfo->SectorBitmapStart = LurnDesc->LurnInfoRAID[i].SectorBitmapStart;
		pChildInfo->SectorLastWrittenSector = (ULONG)LurnDesc->LurnInfoRAID[i].SectorLastWrittenSector;
		if(pRaidInfo->SectorsPerBit != LurnDesc->LurnInfoRAID[i].SectorsPerBit)
		{
			ASSERT(pRaidInfo->SectorsPerBit != LurnDesc->LurnInfoRAID[i].SectorsPerBit);
			goto out;
		}
		pChildInfo->EC_LWS.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
		pChildInfo->EC_LWS.Operation = CCB_EXT_WRITE;
		pChildInfo->EC_LWS.logicalBlockAddress = (ULONG)pChildInfo->SectorLastWrittenSector;
		pChildInfo->EC_LWS.LengthBlock = 1;
		pChildInfo->EC_LWS.ByteOperation = EXT_BLOCK_OPERATION;
		pChildInfo->EC_LWS.pNextCmd = NULL; // vary
		pChildInfo->EC_LWS.pByteData = (PBYTE)&pRaidInfo->LWSs;
	}

	// EC_Bitmap
	pRaidInfo->EC_Bitmap.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
	pRaidInfo->EC_Bitmap.Operation = CCB_EXT_WRITE;
//	pRaidInfo->EC_Bitmap.logicalBlockAddress = ;
	pRaidInfo->EC_Bitmap.Offset = 0;
	pRaidInfo->EC_Bitmap.ByteOperation = EXT_BLOCK_OPERATION;
//	pRaidInfo->EC_Bitmap.pByteData = ;
	pRaidInfo->EC_Bitmap.pNextCmd = NULL; // vari

//	Bitmap (1) * (bitmap structure size + bitmap size)
	
	pRaidInfo->Bitmap = (PRTL_BITMAP)ExAllocatePoolWithTag(NonPagedPool, 
		ulBitMapSize + sizeof(RTL_BITMAP), BITMAP_POOL_TAG);

	if(NULL == pRaidInfo->Bitmap)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	RtlInitializeBitMap(
		pRaidInfo->Bitmap,
		(PULONG)(pRaidInfo->Bitmap +1), // start address of bitmap data
		ulBitMapSize * 8);

    RtlClearAllBits(pRaidInfo->Bitmap);

	KeInitializeSpinLock(&pRaidInfo->LockInfo);

	// create recover thread
	KeInitializeEvent(&pRaidInfo->RecoverThreadEvent, SynchronizationEvent, FALSE);

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ntStatus = PsCreateSystemThread(
		&pRaidInfo->ThreadRecoverHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		LurnRAIDThreadProcRecover,
		Lurn
		);

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL : LurnRAIDThreadProcRecover !!!\n"));
		ntStatus = STATUS_THREAD_NOT_IN_PROCESS;
		goto out;
	}

	ntStatus = ObReferenceObjectByHandle(
		pRaidInfo->ThreadRecoverHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		&pRaidInfo->ThreadRecoverObject,
		NULL
		);

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAIDInfo)
		{
			if(pRaidInfo->DataBufferAllocated)
			{
				ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG);
				pRaidInfo->DataBufferAllocated = NULL;
			}
			if(pRaidInfo->Bitmap)
			{
				ExFreePoolWithTag(pRaidInfo->Bitmap, BITMAP_POOL_TAG);
				pRaidInfo->Bitmap = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG);
			Lurn->LurnRAIDInfo = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnRAID4Destroy(
		PLURELATION_NODE Lurn
	) 
{
	NTSTATUS status;
	LARGE_INTEGER TimeOut;
	PRAID_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAIDInfo);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	if(pRaidInfo->ThreadRecoverHandle)
	{
		pRaidInfo->RaidStatus = RAID_STATUS_TERMINATING;
		KeSetEvent(&pRaidInfo->RecoverThreadEvent,IO_NO_INCREMENT, FALSE);
	}

	TimeOut.QuadPart = - NANO100_PER_SEC * 120;

	status = KeWaitForSingleObject(
		pRaidInfo->ThreadRecoverObject,
		Executive,
		KernelMode,
		FALSE,
		&TimeOut
		);

	ASSERT(NT_SUCCESS(status));

	ObDereferenceObject(pRaidInfo->ThreadRecoverObject);

	ASSERT(pRaidInfo->DataBufferAllocated);
	ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG) ;
	pRaidInfo->DataBufferAllocated = NULL;
	
	ASSERT(pRaidInfo->Bitmap);
	ExFreePoolWithTag(pRaidInfo->Bitmap, BITMAP_POOL_TAG) ;
	pRaidInfo->Bitmap = NULL;

/*
	ASSERT(pRaidInfo->Children);
	ExFreePoolWithTag(pRaidInfo->Children, RAID_INFO_POOL_TAG);
	pRaidInfo->Children = NULL;
*/

	ExFreePoolWithTag(pRaidInfo, RAID_INFO_POOL_TAG) ;
	pRaidInfo = NULL;

	return STATUS_SUCCESS ;
}

NTSTATUS
LurnRAID4CcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;
	NTSTATUS status;
	PRAID_INFO pRaidInfo;
	PLURELATION_NODE pLurnOriginal;

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	pLurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	ASSERT(pLurnOriginal);
	pRaidInfo = pLurnOriginal->LurnRAIDInfo;

	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:
		break;

	case CCB_STATUS_BUSY:
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb || OriginalCcb->OperationCode == CCB_OPCODE_RESETBUS);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:
		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP))
		{
/*
*	see RAID1 for details
*	This method is not reliable without a lock to write/read command
*	Execute does not send Ccb to stopped IDE (and one by one), so...
*	If 2 or more Ccb fails, OriginalCcb->CcbStatus = CCB_STATUS_STOP
*	If RaidStatus != RAID_STATUS_NORMAL & 1 or more Ccb fails, OriginalCcb->CcbStatus = CCB_STATUS_STOP
*/
			PLURELATION_NODE pLurnChildDefected;
			ULONG i;

			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP);
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE);

			KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, CCBSTATUS_FLAG_LURN_STOP not flagged\n"));

			//////////////////////////////////////////////
			//
			//	Initialize raid information
			//
			{
				// set backup information
				ASSERT(Ccb->CcbCurrentStackLocation);
				ASSERT(Ccb->CcbCurrentStackLocation->Lurn);
				pLurnChildDefected = Ccb->CcbCurrentStackLocation->Lurn;

				ASSERT(OriginalCcb->CcbCurrentStackLocation);
				ASSERT(OriginalCcb->CcbCurrentStackLocation->Lurn);

				ASSERT(LURN_IDE_DISK == pLurnChildDefected->LurnType);
				ASSERT(LURN_RAID4 == pLurnOriginal->LurnType);

				if(!pLurnChildDefected || !pLurnOriginal)
				{
					ASSERT(FALSE);
					status = STATUS_ILLEGAL_INSTRUCTION;
					break;
				}

				ASSERT(pLurnChildDefected->LurnRAIDInfo);

				if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
				{
					ASSERT(FALSE);
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
				}

				// 1 fail + 1 fail = broken
				if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("RAID_STATUS_NORMAL != pRaidInfo->RaidStatus(%d)\n", pRaidInfo->RaidStatus));

					if(pLurnChildDefected != pLurnOriginal->LurnChildren[pRaidInfo->iChildDefected])
					{
						ASSERT(FALSE);
//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
						break;
					}
				}

				if(RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus)
					KDPrintM(DBG_LURN_ERROR, ("Set to emergency mode\n"));

				pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY;

				for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
				{
					if(pLurnChildDefected == pLurnOriginal->LurnChildren[i])
					{
						pRaidInfo->iChildDefected = i;
						pRaidInfo->iChildRecoverInfo = 
							(pLurnOriginal->LurnChildrenCnt -1 == pRaidInfo->iChildDefected) ? 0 : pRaidInfo->iChildDefected +1;
						break;
					}
				}

				// failed to find a defected child
				if(i == pLurnOriginal->LurnChildrenCnt)
				{
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					break;
				}

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iChildDefected = %d\n", pRaidInfo->iChildDefected));
			}
		} else {
			//
			//	at least two children stopped!
			//
			ASSERT(FALSE);
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;
		}
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE)) {
			//
			//	at least two children problem.! (1 stop, 1 not exist)
			//
			ASSERT(FALSE);
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;
			break;
		}
	default:					// priority 0/4
		if(CCB_STATUS_NOT_EXIST != Ccb->CcbStatus)
		{
			KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d, OriginalCcb->AssociateCount = %d\n",
				(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID, OriginalCcb->AssociateCount));
		}

		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR)) {
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR);
		} else {
			//
			//	at least two children have an error or do not exist! (2 not exist)
			//

			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}

	if(RAID_STATUS_FAIL == pRaidInfo->RaidStatus)
	{
		ASSERT(FALSE);
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
	}

	LSCcbSetStatusFlag(	OriginalCcb,
						Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
					);

	if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
		LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_RECOVERING);

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);


	//
	//	Complete the original CCB
	//

	// clear extended command
//	LSCcbRemoveExtendedCommandTailMatch(&(Ccb->pExtendedCommand), OriginalCcb->CcbCurrentStackLocation->Lurn);

	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		return status;
	}

	if(pRaidInfo->RaidStatus != RAID_STATUS_NORMAL)
	{
		KDPrintM(DBG_LURN_NOISE,("All Ccb complete in abnormal status : %d\n", (int)OriginalCcb->Cdb[0]));
	}
	
	if(CCB_OPCODE_EXECUTE == OriginalCcb->OperationCode)
	{
	// shuffle completion
		switch(OriginalCcb->Cdb[0])
		{
		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
			// merge read buffer before release buffer lock
			{
				ULONG BlocksPerDisk;
				ULONG i, j;
				register int k;
				PULONG pDataBufferToRecover, pDataBufferSrc;
				int	bDataBufferToRecoverInitialized;

				KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

				// create new data buffer and encrypt here.
				// new data buffer will be deleted at completion routine
				BlocksPerDisk = Ccb->DataBufferLength / BLOCK_SIZE;

				// if non-parity disk is corrupted, generate data with parity work
				if(pLurnOriginal->LurnRAIDInfo->RaidStatus != RAID_STATUS_NORMAL &&
					pLurnOriginal->LurnRAIDInfo->iChildDefected != pLurnOriginal->LurnChildrenCnt -1) // not parity
				{
					bDataBufferToRecoverInitialized = FALSE;
					
					for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
					{
						// skip defected
						if(pLurnOriginal->LurnRAIDInfo->iChildDefected == i)
							continue;

						pDataBufferSrc = (PULONG)pRaidInfo->Children[i].DataBuffer;
						pDataBufferToRecover = (PULONG)pRaidInfo->Children[pLurnOriginal->LurnRAIDInfo->iChildDefected].DataBuffer;

						// for performance, copy first disk to recover disk
						if(FALSE == bDataBufferToRecoverInitialized)
						{
							RtlCopyMemory(pDataBufferToRecover, pDataBufferSrc, BlocksPerDisk * BLOCK_SIZE);
							bDataBufferToRecoverInitialized = TRUE;
							continue;
						}
						
						k = (BlocksPerDisk * BLOCK_SIZE) / sizeof(ULONG);
						while(k--)
						{
							*pDataBufferToRecover ^= *pDataBufferSrc;
							pDataBufferToRecover++;
							pDataBufferSrc++;
						}
					}
				}

				// merge, exclude parity
				for(i = 0; i < pLurnOriginal->LurnChildrenCnt -1; i++)
				{
					for(j = 0; j < BlocksPerDisk; j++)
					{
						RtlCopyMemory( // Copy back
							(PCHAR)OriginalCcb->DataBuffer + (i + j * (pLurnOriginal->LurnChildrenCnt -1)) * BLOCK_SIZE,
							(PCHAR)pRaidInfo->Children[i].DataBuffer + j * BLOCK_SIZE,
							BLOCK_SIZE);
					}
				}

				InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			}
			break;
		case SCSIOP_WRITE: 
		case SCSIOP_VERIFY: 
	//	default:
			// release buffer lock
			{
				KDPrintM(DBG_LURN_NOISE,("SCSIOP_ %d\n", (int)OriginalCcb->Cdb[0]));
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			}
			break;
		}
	}
	
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnRAID4Execute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) {
	NTSTATUS			status;

	PRAID_INFO pRaidInfo;
	PCMD_BYTE_OP	pExtendedCommands[NDAS_MAX_RAID4_CHILD];
	PLURELATION_NODE	LurnChildren[NDAS_MAX_RAID4_CHILD];
	KIRQL	oldIrql;

	RtlZeroMemory(pExtendedCommands, NDAS_MAX_RAID4_CHILD * sizeof(PCMD_BYTE_OP));

	pRaidInfo = Lurn->LurnRAIDInfo;

	// record a bitmap information to the next disk of the defected disk
	// pExtendedCommands itself will go into LurnAssocSendCcbToChildrenArray
	ASSERT(Lurn->LurnChildrenCnt > 0 && Lurn->LurnChildrenCnt <= NDAS_MAX_RAID4_CHILD);

	if(
		RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
		RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus 
		)
	{
		ASSERT(pRaidInfo->iChildDefected < (UINT32)Lurn->LurnChildrenCnt);
	}

	// recovery/data buffer protection code
	switch(Ccb->Cdb[0])
	{
		case SCSIOP_WRITE:
		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_VERIFY:
			{
				UINT32 logicalBlockAddress;
				UINT16 transferBlocks;

				logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
				transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);
				// Busy if this location is under recovering

				ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
				if(
					RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus || // do not IO when reading bitmap
					(
						RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus && // do not IO where being recovered
						(
							pRaidInfo->BitmapIdxToRecover == logicalBlockAddress / 128 ||
							pRaidInfo->BitmapIdxToRecover == (logicalBlockAddress + transferBlocks -1) / 128
						)
					)
					)
				{
					RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

					KDPrintM(DBG_LURN_ERROR, ("!! RECOVER THREAD PROTECTION : %08lx, %d, %d\n", pRaidInfo->RaidStatus, logicalBlockAddress, transferBlocks));
					Ccb->CcbStatus = CCB_STATUS_BUSY;
					LSCcbCompleteCcb(Ccb);
					return STATUS_SUCCESS;
				}

				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

				// lock buffer : release at completion / NEVER forget to release when fail
				while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
					InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			}
	}

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;

			int DataBufferLengthPerDisk;
			ULONG BlocksPerDisk;
			register int k;
			register ULONG i, j;
			PULONG pDataBufferParity, pDataBufferSrc;
			CUSTOM_DATA_BUFFER cdb;
//			int	bDataBufferParityInitialized;
			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_NORMAL:
				// record last written sector
				{
					PCMD_BYTE_LAST_WRITTEN_SECTOR pLastWrittenSector;

					// AING_TO_DO : use Ready made pool
					for(i = 0; i < Lurn->LurnChildrenCnt; i++)
					{
						pExtendedCommands[i] = &pRaidInfo->Children[i].EC_LWS;
						pExtendedCommands[i]->pNextCmd = NULL;

						pLastWrittenSector = &pRaidInfo->LWSs[pRaidInfo->timeStamp % 32];

						pLastWrittenSector->logicalBlockAddress = (UINT64)logicalBlockAddress;
						pLastWrittenSector->transferBlocks = (UINT32)transferBlocks;
						pLastWrittenSector->timeStamp = pRaidInfo->timeStamp;
					}
					pRaidInfo->timeStamp++;
				}
				break;
			case RAID_STATUS_EMERGENCY:
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY\n"));
					
					// use pRaidInfo->EC_Bitmap instead allocating
					// seek first sector in bitmap
					uiBitmapStartInBits = logicalBlockAddress / 
						pRaidInfo->SectorsPerBit;
					uiBitmapEndInBits = (logicalBlockAddress + transferBlocks -1) /
						pRaidInfo->SectorsPerBit;

					// check if any bits would be changed
					if(!RtlAreBitsSet(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits))
					{
						// bitmap work
						KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY bitmap changed\n"));
						RtlSetBits(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits - uiBitmapStartInBits +1);
						pRaidInfo->EC_Bitmap.pByteData = 
							(PCHAR)(pRaidInfo->Bitmap->Buffer) + (uiBitmapStartInBits / BITS_PER_BLOCK) * BLOCK_SIZE;
						pRaidInfo->EC_Bitmap.LengthBlock = 
							(uiBitmapStartInBits / BITS_PER_BLOCK == uiBitmapEndInBits) ? 1 : 2;
						pRaidInfo->EC_Bitmap.logicalBlockAddress = 
							(ULONG)pRaidInfo->Children[pRaidInfo->iChildRecoverInfo].SectorBitmapStart +
							(uiBitmapStartInBits / BITS_PER_BLOCK);
						pExtendedCommands[pRaidInfo->iChildRecoverInfo] = &pRaidInfo->EC_Bitmap;
					}

					// split is super ultra hyper rare case
					if(pRaidInfo->EC_Bitmap.LengthByte == BLOCK_SIZE * 2)
					{
						KDPrintM(DBG_LURN_ERROR,("RAID_STATUS_EMERGENCY bitmap changed DOUBLE : This is not error but ultra rare case\n"));
						ASSERT(FALSE);
					}
				}
				break;
			case RAID_STATUS_RECOVERRING:
				break;
			default:
				// invalid status
				ASSERT(FALSE);
				break;
			}

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			// create new data buffer and encrypt here.
			// new data buffer will be deleted at completion routine
			DataBufferLengthPerDisk = Ccb->DataBufferLength / (Lurn->LurnChildrenCnt -1);
			BlocksPerDisk = DataBufferLengthPerDisk / BLOCK_SIZE;
			
			// split DataBuffer into each DataBuffers of children by block size
			for(i = 0; i < Lurn->LurnChildrenCnt -1; i++)
			{
				pDataBufferSrc = (PULONG)pRaidInfo->Children[i].DataBuffer;

				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory(
						(PCHAR)pDataBufferSrc + j * BLOCK_SIZE,
						(PCHAR)Ccb->DataBuffer + (i + j * (Lurn->LurnChildrenCnt -1)) * BLOCK_SIZE,
						BLOCK_SIZE);
				}
			}

			// generate parity
			// initialize the parity buffer with the first buffer
			RtlCopyMemory(
				pRaidInfo->Children[Lurn->LurnChildrenCnt -1].DataBuffer,
				pRaidInfo->Children[0].DataBuffer,
				BLOCK_SIZE * BlocksPerDisk);

			// p' ^= p ^ d;
			for(i = 1; i < Lurn->LurnChildrenCnt -1; i++)
			{
				pDataBufferSrc = (PULONG)pRaidInfo->Children[i].DataBuffer;
				pDataBufferParity = (PULONG)pRaidInfo->Children[Lurn->LurnChildrenCnt -1].DataBuffer;

				// parity work
				k = (BlocksPerDisk * BLOCK_SIZE) / sizeof(ULONG);
				while(k--)
				{
					*pDataBufferParity ^= *pDataBufferSrc;
					pDataBufferParity++;
					pDataBufferSrc++;
				}
			}

			// initialize cdb, LurnChildren
			for(i = 0, j= 0; i < Lurn->LurnChildrenCnt; i++)
			{
				cdb.DataBuffer[j] = pRaidInfo->Children[i].DataBuffer;
				cdb.DataBufferLength[j] = (UINT32)DataBufferLengthPerDisk;
				LurnChildren[j] = Lurn->LurnChildren[i];
				j++;
			}
			
			cdb.DataBufferCount = j;
			
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				&cdb,
				pExtendedCommands,
				FALSE);
		}
		break;
	case SCSIOP_VERIFY:
		{
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				NULL,
				NULL,
				FALSE);
		}
		break;

	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
		{
			ULONG DataBufferLengthPerDisk;
			ULONG i, j;
			CUSTOM_DATA_BUFFER cdb;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

			DataBufferLengthPerDisk = Ccb->DataBufferLength / (Lurn->LurnChildrenCnt -1);
			KDPrintM(DBG_LURN_NOISE,("R Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			for(i = 0, j = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				LurnChildren[j] = Lurn->LurnChildren[i];
				cdb.DataBuffer[j] = pRaidInfo->Children[i].DataBuffer;
				cdb.DataBufferLength[j] = (UINT32)DataBufferLengthPerDisk;
				j++;
			}

			cdb.DataBufferCount = j;
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				&cdb,
				NULL,
				FALSE
				);
		}
		break;

	case SCSIOP_INQUIRY: {
		INQUIRYDATA			inquiryData;
		UCHAR				Model[16] = RAID4_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));
		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

		RtlZeroMemory(Ccb->DataBuffer, Ccb->DataBufferLength);
		RtlZeroMemory(&inquiryData, sizeof(INQUIRYDATA));

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
		inquiryData.DeviceTypeModifier;
		inquiryData.RemovableMedia = FALSE;
		inquiryData.Versions = 2;
        inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;
		inquiryData.NormACA;
//		inquiryData.TerminateTask;
		inquiryData.AERC;
		inquiryData.AdditionalLength = 31;	// including ProductRevisionLevel.
//		inquiryData.MediumChanger;
//		inquiryData.MultiPort;
//		inquiryData.EnclosureServices;
		inquiryData.SoftReset;
		inquiryData.CommandQueue;
		inquiryData.LinkedCommands;
		inquiryData.RelativeAddressing;

		RtlCopyMemory(
			inquiryData.VendorId,
			NDAS_DISK_VENDOR_ID,
			(strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8
			);
		
		RtlCopyMemory(
			inquiryData.ProductId,
			Model,
			16
			);

		RtlCopyMemory(
					inquiryData.ProductRevisionLevel,
					PRODUCT_REVISION_LEVEL,
					(strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  
							(strlen(PRODUCT_REVISION_LEVEL)+1) : 4
					);

		RtlMoveMemory (
					Ccb->DataBuffer,
					&inquiryData,
					Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? 
					sizeof (INQUIRYDATA) : 
					Ccb->DataBufferLength
					);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
		
		status = STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		break;
	}

	case SCSIOP_READ_CAPACITY: 
	{
		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;
		
		logicalBlockAddress = sectorCount - 1;
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		blockSize *= (Lurn->LurnChildrenCnt - 1); // exclude parity

		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;

		KDPrintM(DBG_LURN_ERROR, ("SCSIOP_READ_CAPACITY: %08x : %04x\n", (UINT32)logicalBlockAddress, (UINT32)blockSize));
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_MODE_SENSE:
	{
		PCDB	Cdb;
		PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
		PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));

		RtlZeroMemory(
			Ccb->DataBuffer,
			sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK)
			);
		Cdb = (PCDB)Ccb->Cdb;
		if(	Cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) {	// all pages
			ULONG	BlockCount;
			//
			// Make Header.
			//
			parameterHeader->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK) - sizeof(parameterHeader->ModeDataLength);
			parameterHeader->MediumType = 00;	// Default medium type.
			
			if(!(Lurn->AccessRight & GENERIC_WRITE)) {
				KDPrintM(DBG_LURN_INFO, 
				("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n"));
				parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;
	
				if(Lurn->Lur->LurFlags & LURFLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(BLOCK_SIZE>>16);
			parameterBlock->BlockLength[1] = (BYTE)(BLOCK_SIZE>>8);
			parameterBlock->BlockLength[2] = (BYTE)(BLOCK_SIZE);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		} else {
			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		}
		break; 
	}

	default:
		//
		//	send to all child LURNs.
		//	Set WriteVerify completion routine to CCB
		//		it guarantees CCB_STATUS_SUCCESS when least one LURN is working fine.
		//

		{
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				NULL,
				NULL,
				FALSE
				);
		}
		break;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID4Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS				status;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("LurnRAID4Request!\n"));

	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnRAID4Execute(Lurn, Ccb);
		break;

	//
	//	Send to all LURNs
	//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_STOP:
	case CCB_OPCODE_NOOP: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID4CcbCompletion, NULL, NULL, FALSE);
		break;
	}

	case CCB_OPCODE_UPDATE: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL, TRUE); // use same function as Mirror
		break;
	}

	case CCB_OPCODE_RECOVER:
		{
			PRAID_INFO pRaidInfo;

			pRaidInfo = Lurn->LurnRAIDInfo;

			KDPrintM(DBG_LURN_ERROR, ("CCB_OPCODE_RECOVER\n"));

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_ERROR, ("Read only => Do not start recovering\n"));
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}

			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);

			if(RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus)
			{
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);

				KDPrintM(DBG_LURN_ERROR, ("pRaidInfo->RaidStatus = %08lx => Do not start recovering\n", pRaidInfo->RaidStatus));
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}

			// start to recover
			KeSetEvent(&pRaidInfo->RecoverThreadEvent, IO_NO_INCREMENT, FALSE);

			InterlockedDecrement(&pRaidInfo->LockDataBuffer);

			KDPrintM(DBG_LURN_ERROR, ("Recovery initialize complete\n"));
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
		}
		break;

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	return STATUS_SUCCESS;
}
