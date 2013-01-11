#include <ntddk.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnAssoc.h"
#include "lsminiportioctl.h"
#include <scrc32.h>

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
			KDPrintM(DBG_LURN_INFO, ("Cascade #%d.\n", idx_child));
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
	
	if(NrLurns > THREAD_WAIT_OBJECTS)
	{
		WaitBlockArray = (PKWAIT_BLOCK)ExAllocatePoolWithTag(NonPagedPool, sizeof(KWAIT_BLOCK) * NrLurns,
			EXEC_SYNC_POOL_TAG);
		if(!WaitBlockArray)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto out;
		}
	}
	else
	{
		WaitBlockArray = NULL;
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

		if(STATUS_SUCCESS != status)
		{
			KDPrintM(DBG_LURN_TRACE, ("KeWaitForMultipleObjects result : %08x, Waits : %d\n",
				status, Waits));
			// Pass if NT_SUCCESS. On win2k, it can be other value than STATUS_SUCCESS(such as STATUS_WAIT1)
			if(!NT_SUCCESS(status))
			{
				KDPrintM(DBG_LURN_ERROR, ("KeWaitForMultipleObjects result : %08x, Waits : %d\n",
					status, Waits));
				status = STATUS_UNSUCCESSFUL;
				goto out;
			}
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

static
NTSTATUS
LurnExecuteSyncRead(
	IN PLURELATION_NODE Lurn,
	OUT PUCHAR data_buffer,
	IN INT64 logicalBlockAddress,
	IN UINT32 transferBlocks
)
{
	PCMD_BYTE_OP ExtendedCommand = NULL;

	NTSTATUS status;
	CMD_BYTE_OP ext_cmd;
	RtlZeroMemory(&ext_cmd, sizeof(CMD_BYTE_OP));
	ext_cmd.Operation = CCB_EXT_READ;
	ext_cmd.CountBack = 
		(logicalBlockAddress < 0) ? TRUE : FALSE;
	ext_cmd.logicalBlockAddress = 
		(ULONG)((logicalBlockAddress < 0) ? -1 * logicalBlockAddress : logicalBlockAddress);
	ext_cmd.ByteOperation = EXT_BLOCK_OPERATION;
	ext_cmd.pByteData = data_buffer;
	ext_cmd.LengthBlock = (UINT16)transferBlocks;
	ext_cmd.pLurnCreated = Lurn;

	status = LurnExecuteSync(
		1, 
		&Lurn,
		SCSIOP_WRITE,
		&data_buffer, // not used
		0,
		0,
		(PCMD_COMMON)&ext_cmd);

	return status;
}

static
NTSTATUS
LurnExecuteSyncWrite(
	IN PLURELATION_NODE Lurn,
	IN OUT PUCHAR data_buffer,
	IN INT64 logicalBlockAddress,
	IN UINT32 transferBlocks
)
{
	PCMD_BYTE_OP ExtendedCommand = NULL;

	NTSTATUS status;
	CMD_BYTE_OP ext_cmd;

	RtlZeroMemory(&ext_cmd, sizeof(CMD_BYTE_OP));
	ext_cmd.Operation = CCB_EXT_WRITE;
	ext_cmd.CountBack = 
		(logicalBlockAddress < 0) ? TRUE : FALSE;
	ext_cmd.logicalBlockAddress = 
		(ULONG)((logicalBlockAddress < 0) ? -1 * logicalBlockAddress : logicalBlockAddress);
	ext_cmd.ByteOperation = EXT_BLOCK_OPERATION;
	ext_cmd.pByteData = data_buffer;
	ext_cmd.LengthBlock = (UINT16)transferBlocks;
	ext_cmd.pLurnCreated = Lurn;

	status = LurnExecuteSync(
		1, 
		&Lurn,
		SCSIOP_WRITE,
		&data_buffer, // not used
		0,
		0,
		(PCMD_COMMON)&ext_cmd);

	if(!NT_SUCCESS(status))
	{
		return status;
	}

	return status;
}


static
NTSTATUS
LurnRMDRead(
	IN PLURELATION_NODE		Lurn)
{
	NTSTATUS status;
	ULONG i;
	PRAID_INFO pRaidInfo;
	NDAS_RAID_META_DATA rmd_tmp, *rmd;
	UINT32 uiUSNMax;

	KDPrintM(DBG_LURN_INFO, ("IN\n"));

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	rmd = &pRaidInfo->rmd;

	uiUSNMax = 0;

	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		if(LURN_STATUS_RUNNING != Lurn->LurnChildren[i]->LurnStatus)
			continue;

		status = LurnExecuteSyncRead(Lurn->LurnChildren[i], (PUCHAR)&rmd_tmp,
			NDAS_BLOCK_LOCATION_RMD, 1);

		if(!NT_SUCCESS(status))
			continue;

		if(
			NDAS_RAID_META_DATA_SIGNATURE != rmd_tmp.Signature || 
			!IS_RMD_CRC_VALID(crc32_calc, rmd_tmp))
		{
			// invalid rmd
			continue;
		}
		else
		{
			if(uiUSNMax < rmd_tmp.uiUSN)
			{
				uiUSNMax = rmd_tmp.uiUSN;
				// newer one
				RtlCopyMemory(rmd, &rmd_tmp, sizeof(NDAS_RAID_META_DATA));
			}
		}
	}

	if(0 == uiUSNMax)
	{
		// not found, init rmd here
		RtlZeroMemory(rmd, sizeof(NDAS_RAID_META_DATA));
		rmd->Signature = NDAS_RAID_META_DATA_SIGNATURE;
		rmd->uiUSN = 1;
		for(i = 0; i < Lurn->LurnChildrenCnt; i ++)
		{
			rmd->UnitMetaData[i].iUnitDeviceIdx = (unsigned _int16)i;
		}
		SET_RMD_CRC(crc32_calc, *rmd);
	}

	status = STATUS_SUCCESS;
	
	KDPrintM(DBG_LURN_INFO, ("OUT\n"));
	return status;
}

static
NTSTATUS
LurnRMDWrite(
			IN PLURELATION_NODE		Lurn)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	ULONG i, fail_count;
	PRAID_INFO pRaidInfo;
	NDAS_RAID_META_DATA rmd_tmp, *rmd;
	UINT32 uiUSNMax;

	KDPrintM(DBG_LURN_INFO, ("IN\n"));
	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	rmd = &pRaidInfo->rmd;

	// read NDAS_BLOCK_LOCATION_RMD_T to get highest USN
	uiUSNMax = rmd->uiUSN;

	for(fail_count = 0, i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		if(LURN_STATUS_RUNNING != Lurn->LurnChildren[i]->LurnStatus)
			continue;

		status = LurnExecuteSyncRead(Lurn->LurnChildren[i], (PUCHAR)&rmd_tmp,
			NDAS_BLOCK_LOCATION_RMD, 1);

		if(!NT_SUCCESS(status))
		{
			fail_count++;
			KDPrintM(DBG_LURN_ERROR, ("read failed on Lurn->LurnChildren[%d] = %p\n", i, Lurn->LurnChildren[i]));
			continue;
		}

		if(
			NDAS_RAID_META_DATA_SIGNATURE != rmd_tmp.Signature || 
			!IS_RMD_CRC_VALID(crc32_calc, rmd_tmp))
		{
			// invalid rmd
			fail_count++;
			KDPrintM(DBG_LURN_ERROR, ("bad RMD on Lurn->LurnChildren[%d] = %p\n", i, Lurn->LurnChildren[i]));
			continue;
		}
		else
		{
			if(uiUSNMax < rmd_tmp.uiUSN)
			{
				uiUSNMax = rmd_tmp.uiUSN;
			}
		}
	}

	if(i == fail_count)
	{
		goto fail;
	}
		

	// increase USN to highest
	rmd->uiUSN = uiUSNMax +1;
	SET_RMD_CRC(crc32_calc, *rmd);

	// write rmd to NDAS_BLOCK_LOCATION_RMD_T
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		if(LURN_STATUS_RUNNING != Lurn->LurnChildren[i]->LurnStatus)
			continue;

		status = LurnExecuteSyncWrite(Lurn->LurnChildren[i], (PUCHAR)rmd,
			NDAS_BLOCK_LOCATION_RMD_T, 1);

		if(!NT_SUCCESS(status))
		{
			fail_count++;
			KDPrintM(DBG_LURN_ERROR, ("write _T failed on Lurn->LurnChildren[%d] = %p\n", i, Lurn->LurnChildren[i]));
			continue;
		}
	}

	if(i == fail_count)
	{
		goto fail;
	}

	// write rmd to NDAS_BLOCK_LOCATION_RMD
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		if(LURN_STATUS_RUNNING != Lurn->LurnChildren[i]->LurnStatus)
			continue;

		status = LurnExecuteSyncWrite(Lurn->LurnChildren[i], (PUCHAR)rmd,
			NDAS_BLOCK_LOCATION_RMD, 1);

		if(!NT_SUCCESS(status))
		{
			fail_count++;
			KDPrintM(DBG_LURN_ERROR, ("write failed on Lurn->LurnChildren[%d] = %p\n", i, Lurn->LurnChildren[i]));
			continue;
		}
	}

	if(i == fail_count)
	{
		goto fail;
	}

	status = STATUS_SUCCESS;
fail:
	KDPrintM(DBG_LURN_INFO, ("OUT\n"));
	return status;
}

/*
LurnReadyToRecover
returns STATUS_SUCCESS even if raid status is RAID_STATUS_FAIL
check raid status after function return
*/
static
NTSTATUS
LurnRefreshRaidStatus(
	PLURELATION_NODE Lurn,
	UINT32 *new_raid_status
	)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PRAID_INFO pRaidInfo;
	PNDAS_RAID_META_DATA rmd;
	LAST_WRITTEN_SECTORS LWSs;
	ULONG BitmapIdxToRecover;
	UINT32 i;
	UINT32 iChildDefected;
	BOOLEAN rmd_invalid = FALSE;

	KDPrintM(DBG_LURN_INFO, ("IN\n"));

	ASSERT(Lurn);
	
	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	rmd = &pRaidInfo->rmd;

	// lock outside
//	ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);

	if (RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus ||
		RAID_STATUS_TERMINATING == pRaidInfo->RaidStatus ||
		RAID_STATUS_FAIL == pRaidInfo->RaidStatus)
	{
		// nothing to do here
		// do not change raid status
		KDPrintM(DBG_LURN_INFO, ("RaidStatus : %d. nothing to do in this function\n", pRaidInfo->RaidStatus));
		status = STATUS_SUCCESS;
		goto out;
	}

	// mark current fault children
	for(i = 0; i < pRaidInfo->nDiskCount; i++)
	{
		if(LURN_IS_RUNNING(pRaidInfo->MapLurnChildren[i]->LurnStatus))
			continue;

		if(NDAS_UNIT_META_BIND_STATUS_FAULT & rmd->UnitMetaData[i].UnitDeviceStatus)
		{
			KDPrintM(DBG_LURN_INFO, ("Child not running, Mark as fault at %d\n", i));
			continue;
		}

		// new fault
		rmd->UnitMetaData[i].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_FAULT;
		rmd_invalid = TRUE;
	}

	// find fault device(s)
	iChildDefected = 0xFFFFFFFF;
	for(i = 0; i < pRaidInfo->nDiskCount; i++)
	{
		if(NDAS_UNIT_META_BIND_STATUS_FAULT & rmd->UnitMetaData[i].UnitDeviceStatus)
		{
			KDPrintM(DBG_LURN_INFO, ("Device %d(%d in DIB) is set as fault\n", i, rmd->UnitMetaData[i].iUnitDeviceIdx));
			if(0xFFFFFFFF != iChildDefected)
			{
				KDPrintM(DBG_LURN_ERROR, ("2 or more faults. we cannot proceed.\n"));
//				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				*new_raid_status = RAID_STATUS_FAIL;
				status = STATUS_UNSUCCESSFUL;
				goto out;
			}

			iChildDefected = i;
		}
	}

	if(0xFFFFFFFF == iChildDefected)
	{
		KDPrintM(DBG_LURN_INFO, ("fault device not found. I'm healthy\n"));
//		pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;
		*new_raid_status = RAID_STATUS_NORMAL;
		status = STATUS_SUCCESS;
		goto out;
	}

	// found fault device(one and only)
	pRaidInfo->iChildDefected = iChildDefected;
	pRaidInfo->iChildRecoverInfo = 
		(iChildDefected == pRaidInfo->nDiskCount -1) ? 0 : iChildDefected +1;
	
	// if read only | secondary. Just keep emergency
	if(!(GENERIC_WRITE & Lurn->AccessRight))
	{
//		pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY;
		*new_raid_status = RAID_STATUS_EMERGENCY;
		status = STATUS_SUCCESS;
		goto out;
	}

	// is the fault device alive now?
	if(LURN_IS_RUNNING(pRaidInfo->MapLurnChildren[iChildDefected]->LurnStatus))
	{
		KDPrintM(DBG_LURN_INFO, ("fault device alive, use the fault device(do not use spare device)\n"));
		if(RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus)
		{
			// it is mount time. we need to read bitmap, LWS

			// read bitmap
			status = LurnExecuteSyncRead(
				pRaidInfo->MapLurnChildren[pRaidInfo->iChildRecoverInfo],
				(PUCHAR)pRaidInfo->Bitmap->Buffer,
				NDAS_BLOCK_LOCATION_BITMAP,
				(UINT32)NDAS_BLOCK_SIZE_BITMAP);

			if(!NT_SUCCESS(status))
			{
//				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				*new_raid_status = RAID_STATUS_FAIL;
				status = STATUS_UNSUCCESSFUL;
				goto out;
			}

			// read LWS
			status = LurnExecuteSyncRead(
				pRaidInfo->MapLurnChildren[pRaidInfo->iChildRecoverInfo],
				(PUCHAR)LWSs.LWS,
				NDAS_BLOCK_LOCATION_WRITE_LOG,
				1);

			if(!NT_SUCCESS(status))
			{
//				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				*new_raid_status = RAID_STATUS_FAIL;
				status = STATUS_UNSUCCESSFUL;
				goto out;
			}

			// set bitmap where LWSs indicates
			for(i = 0; i < 32; i++)
			{
				BitmapIdxToRecover = (ULONG)pRaidInfo->LWSs.LWS[i].logicalBlockAddress;
				if(BitmapIdxToRecover >= pRaidInfo->Bitmap->SizeOfBitMap * pRaidInfo->SectorsPerBit)
				{
					ASSERT(FALSE);
					continue;
				}

				RtlSetBits(pRaidInfo->Bitmap, (ULONG)BitmapIdxToRecover / pRaidInfo->SectorsPerBit, 1 + 1/* set next bit also*/);
			}
		}
		else
		{
			// bitmap already has bitmap information. just go
		}

		// now bitmap ready. start to recover
//		pRaidInfo->RaidStatus = RAID_STATUS_RECOVERRING;
		*new_raid_status = RAID_STATUS_RECOVERRING;

		rmd->UnitMetaData[iChildDefected].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_FAULT;
		rmd_invalid = TRUE;

		// ok done.
		status = STATUS_SUCCESS;
		goto out;
	}

	KDPrintM(DBG_LURN_INFO, ("fault device not alive, we seek spare device to recover\n"));
	// find alive spare device
	for(i = pRaidInfo->nDiskCount; i < pRaidInfo->nDiskCount + pRaidInfo->nSpareDisk; i++)
	{
		if(LURN_STATUS_RUNNING == pRaidInfo->MapLurnChildren[i]->LurnStatus)
		{
			// alive spare device found. Hot spare time.
			PLURELATION_NODE node_tmp;
			NDAS_UNIT_META_DATA umd_tmp;

			// fill bitmaps first & write to recover info disk
			RtlSetAllBits(pRaidInfo->Bitmap);
			status = LurnExecuteSyncWrite(
				pRaidInfo->MapLurnChildren[pRaidInfo->iChildRecoverInfo],
				(PUCHAR)pRaidInfo->Bitmap->Buffer,
				NDAS_BLOCK_LOCATION_BITMAP,
				(UINT32)NDAS_BLOCK_SIZE_BITMAP);			

			if(!NT_SUCCESS(status))
			{
//				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				*new_raid_status = RAID_STATUS_FAIL;
				status = STATUS_UNSUCCESSFUL;
				goto out;
			}

			KDPrintM(DBG_LURN_INFO, ("Spare device alive. rmd->UnitMetaData[%d].iUnitDeviceIdx = %d\n", 
				i, rmd->UnitMetaData[i].iUnitDeviceIdx));

			// swap unit device information in rmd
			rmd->UnitMetaData[iChildDefected].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_SPARE;
			RtlCopyMemory(&umd_tmp, &rmd->UnitMetaData[iChildDefected], sizeof(NDAS_UNIT_META_DATA));
			RtlCopyMemory(&rmd->UnitMetaData[iChildDefected], &rmd->UnitMetaData[i], sizeof(NDAS_UNIT_META_DATA));
			RtlCopyMemory(&rmd->UnitMetaData[i], &umd_tmp, sizeof(NDAS_UNIT_META_DATA));
			// set spare flag to indicate this is spare recovery
			// when recovery complete, clear spare flag as well as fault flag.
			rmd->UnitMetaData[iChildDefected].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_FAULT | NDAS_UNIT_META_BIND_STATUS_SPARE;
			rmd_invalid = TRUE;


			// swap child in map
			node_tmp = pRaidInfo->MapLurnChildren[iChildDefected];
			pRaidInfo->MapLurnChildren[iChildDefected] = pRaidInfo->MapLurnChildren[i];
			pRaidInfo->MapLurnChildren[i] = node_tmp;

			if(!NT_SUCCESS(status))
			{
//				pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
				*new_raid_status = RAID_STATUS_FAIL;
				status = STATUS_UNSUCCESSFUL;
				goto out;
			}

//			pRaidInfo->RaidStatus = RAID_STATUS_RECOVERRING;
			*new_raid_status = RAID_STATUS_RECOVERRING;
			status = STATUS_SUCCESS;
			goto out;
		}
	}

	KDPrintM(DBG_LURN_INFO, ("we can not start to recover. keep emergency mode\n"));
	ASSERT(
		RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus ||
		RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus
		);
//	ASSERT(RAID_STATUS_EMERGENCY == *new_raid_status);
//	pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY;
	*new_raid_status = RAID_STATUS_EMERGENCY;

	status = STATUS_SUCCESS;
out:
	KDPrintM(DBG_LURN_INFO, ("status = %x, rmd_invalid = %d, *new_raid_status = %d\n", status, rmd_invalid, *new_raid_status));
	if(NT_SUCCESS(status) && rmd_invalid)
	{
		status = LurnRMDWrite(Lurn);
	}

//	RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

	KDPrintM(DBG_LURN_INFO, ("*new_raid_status = %d\n", *new_raid_status));

	KDPrintM(DBG_LURN_INFO, ("OUT\n"));
	return status;
}


/*
LurnRAIDInitialize MUST be called only by LurnRAIDThreadProcRecover
*/
static
NTSTATUS
LurnRAIDInitialize(
	IN OUT PLURELATION_NODE		Lurn
	)
{
	NTSTATUS status;

	PRAID_INFO pRaidInfo;
	PRTL_BITMAP Bitmap;
	UINT32 SectorsPerBit;
	UINT16 transferBlocks;
	ULONG i;
	KIRQL oldIrql;
	PNDAS_RAID_META_DATA rmd;
	UINT32 iChildDefected = 0xFFFFFFFF;
	UINT32 new_raid_status;

	KDPrintM(DBG_LURN_INFO, ("IN\n"));

	ASSERT(Lurn);
	ASSERT(LURN_RAID1 == Lurn->LurnType || LURN_RAID4 == Lurn->LurnType);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	Bitmap = pRaidInfo->Bitmap;
	ASSERT(Bitmap);

	SectorsPerBit = pRaidInfo->SectorsPerBit;
	ASSERT(SectorsPerBit > 0);

	transferBlocks = (USHORT)pRaidInfo->MaxBlocksPerRequest;

	// assert that read/write operation is blocked
	ASSERT(RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus);

	// We supports missing device launching. Do not ensure all children being running status
	if(0)
	{
		ULONG LurnStatus;

		for(i = 0; i < Lurn->LurnChildrenCnt; i++)
		{
			ACQUIRE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, &oldIrql);
			LurnStatus = Lurn->LurnChildren[i]->LurnStatus;
			RELEASE_SPIN_LOCK(&Lurn->LurnChildren[i]->LurnSpinLock, oldIrql);

			if(LURN_IS_RUNNING(LurnStatus))
				continue;

			// error
			KDPrintM(DBG_LURN_ERROR, ("LURN_STATUS Failed : %d\n", LurnStatus));
			//				ASSERT(FALSE);
			pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
			return STATUS_UNSUCCESSFUL;
		}
	}

	KDPrintM(DBG_LURN_INFO, ("all children are running, now read & test RMD\n"));
	status = LurnRMDRead(Lurn);
	rmd = &pRaidInfo->rmd;

	if(!NT_SUCCESS(status))
	{
		pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
		return STATUS_UNSUCCESSFUL;
	}

	// map MapChild
	for(i = 0; i < Lurn->LurnChildrenCnt; i++)
	{
		pRaidInfo->MapLurnChildren[i] = Lurn->LurnChildren[rmd->UnitMetaData[i].iUnitDeviceIdx];
	}

	status = LurnRefreshRaidStatus(Lurn, &new_raid_status);
	ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
	ASSERT(RAID_STATUS_INITIAILIZING != new_raid_status);
	pRaidInfo->RaidStatus = new_raid_status;
	RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

	KDPrintM(DBG_LURN_INFO, ("OUT\n"));

	return status;
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
	PNDAS_RAID_META_DATA	rmd;
	UINT32					nDiskCount;

	UINT32					SectorsPerBit;
	UCHAR					OneSector[512]; // used to clear bitmap and recover LWS with extended command

	KIRQL					oldIrql;
	LARGE_INTEGER			TimeOut;

	// recover variables
	PRTL_BITMAP				Bitmap;
	UINT32					buf_size_each_child, sectors_each_child; // in bytes
	PUCHAR					buf_for_recovery = NULL;
	PUCHAR					bufs_read_from_healthy[NDAS_MAX_RAID_CHILD -1];
	PUCHAR					buf_write_to_fault = NULL;
	ULONG					bit_to_recover, bit_recovered;
	UINT64					sector_to_recover_begin, sector_to_recover_now;
	USHORT					sectors_to_recover_now;
	BOOLEAN					success_on_recover;
	PULONG					parity_src_ptr, parity_tar_ptr;


	KDPrintM(DBG_LURN_INFO, ("Entered\n"));

	// initialize variables, buffers
	ASSERT(Context);

	Lurn = (PLURELATION_NODE)Context;
	KDPrintM(DBG_LURN_INFO, ("=================== BEFORE RAID LOOP ========================\n"));
	KDPrintM(DBG_LURN_INFO, ("==== Lurn : %p\n", Lurn));
	KDPrintM(DBG_LURN_INFO, ("==== Lurn->LurnType : %d\n", Lurn->LurnType));
	ASSERT(LURN_RAID1 == Lurn->LurnType || LURN_RAID4 == Lurn->LurnType);

	pRaidInfo = Lurn->LurnRAIDInfo;
	KDPrintM(DBG_LURN_INFO, ("==== pRaidInfo : %p\n", pRaidInfo));
	KDPrintM(DBG_LURN_INFO, ("==== pRaidInfo->RaidStatus : %d\n", pRaidInfo->RaidStatus));
	KDPrintM(DBG_LURN_INFO, ("==== pRaidInfo->nDiskCount : %d\n", pRaidInfo->nDiskCount));
	KDPrintM(DBG_LURN_INFO, ("==== pRaidInfo->nSpareDisk : %d\n", pRaidInfo->nSpareDisk));
	ASSERT(pRaidInfo);
	
	rmd = &pRaidInfo->rmd;

	Bitmap = pRaidInfo->Bitmap;
	KDPrintM(DBG_LURN_INFO, ("==== Bitmap->SizeOfBitMap : %d\n", Bitmap->SizeOfBitMap));
	ASSERT(Bitmap);

	SectorsPerBit = pRaidInfo->SectorsPerBit;
	ASSERT(SectorsPerBit > 0);

	nDiskCount = Lurn->LurnChildrenCnt - pRaidInfo->nSpareDisk;
	KDPrintM(DBG_LURN_INFO, ("==== nDiskCount : %d\n", nDiskCount));

	// allocate buffers for recovery
	sectors_each_child = pRaidInfo->MaxBlocksPerRequest / (nDiskCount -1);
	KDPrintM(DBG_LURN_INFO, ("==== sectors_each_child : %d\n", sectors_each_child));
	buf_size_each_child = SECTOR_SIZE * sectors_each_child;
	KDPrintM(DBG_LURN_INFO, ("==== buf_size_each_child : %d\n", buf_size_each_child));
	if(LURN_RAID4 == Lurn->LurnType)
	{
		buf_for_recovery = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 
			buf_size_each_child * nDiskCount, RAID_RECOVER_POOL_TAG);
		ASSERT(buf_for_recovery);
		for(i = 0; i < nDiskCount -1; i++)
		{
			bufs_read_from_healthy[i] = buf_for_recovery + i * buf_size_each_child;
		}
		
		// use last parts for write buffer
		buf_write_to_fault = buf_for_recovery + i * buf_size_each_child;
	}
	else if(LURN_RAID1 == Lurn->LurnType)
	{
		ASSERT(2 == nDiskCount);
		buf_for_recovery = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 
			buf_size_each_child, RAID_RECOVER_POOL_TAG);
		ASSERT(buf_for_recovery);

		// RAID 1 don't need parity. Just write with read
		buf_write_to_fault = bufs_read_from_healthy[0] = buf_for_recovery;		
	}
	KDPrintM(DBG_LURN_INFO, ("==== buf_for_recovery : %p\n", buf_for_recovery));
	KDPrintM(DBG_LURN_INFO, ("==== buf_write_to_fault : %p\n", buf_write_to_fault));
	KDPrintM(DBG_LURN_INFO, ("=============================================================\n"));

	if(!buf_for_recovery)
	{
		KDPrintM(DBG_LURN_ERROR, ("Failed to allocate buf_for_recovery\n"));
		pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
		goto fail;
	}

	KDPrintM(DBG_LURN_INFO, ("Variables, Buffers initialized. Read & testing bitmaps\n"));

	// assert that read/write operation is blocked
	ASSERT(RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus);

	status = LurnRAIDInitialize(Lurn);

	if(!NT_SUCCESS(status))
		goto fail;

	ASSERT(RAID_STATUS_INITIAILIZING != pRaidInfo->RaidStatus);

	if(!RAID_IS_RUNNING(pRaidInfo))
		goto fail;


	// OK. we are here. The RAID LOOP
	// The RAID is running anyway. normal/emergency/recovery
	// Here is the flow map
	// RAID LOOP :
	//   If Status recovery, go Recover LOOP
	//   Else, Wait for event(or just wait) for 10 sec(or whatever)
	//   Check status change(lock).
	//     Status normal : back to RAID LOOP
	//     Status emergency : try to revive devices & LurnRefreshRAIDStatus. back to RAID LOOP
	//     Status terminate, fail : goto fail, terminate
	//     Status recovery, init : no kidding! impossible
	// Recover LOOP :
	//   Wait for event (instant, or just skip)
	//   Check status change.
	//     Status recover : ok, proceed
	//     Status terminate, fail : terminate thread
	//     Status else : no kidding! impossible
	// Recover bit LOOP :
	//   Read-(Parity)-Write. Clear bit. Clear bitmap sector(for each clear full sector)
	//   If fully recovered, Status normal, goto RAID LOOP
	//   If fail, goto Fail.
	//   Back to Recover LOOP
	// fail :
	//   Wait for terminate event
	// terminate :
	//   ok. Let's die
	while(TRUE) // RAID LOOP
	{ 
		if(!LURN_IS_RUNNING(Lurn->LurnStatus))
			break;

		if(RAID_STATUS_RECOVERRING != pRaidInfo->RaidStatus)
		{
			// Wait for 5 sec
			KDPrintM(DBG_LURN_NOISE, ("KeWaitForSingleObject ...\n"));

			TimeOut.QuadPart = - NANO100_PER_SEC * 5;
			status = KeWaitForSingleObject(
				&pRaidInfo->RecoverThreadEvent,
				Executive,
				KernelMode,
				FALSE,
				&TimeOut);

			ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_INITIAILIZING: // impossible pass
			case RAID_STATUS_RECOVERRING: // impossible pass
				ASSERT(FALSE);
				break;
			case RAID_STATUS_NORMAL:
				// nothing to do
				break;
			case RAID_STATUS_EMERGENCY_READY: // set at completion
				KDPrintM(DBG_LURN_INFO, ("case RAID_STATUS_EMERGENCY_READY\n"));
				if(GENERIC_WRITE & Lurn->AccessRight)
				{
					RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
					status = LurnRMDWrite(Lurn);
					ASSERT(NT_SUCCESS(status));
					ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
				}
				pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY;
			case RAID_STATUS_EMERGENCY: // fall through
				KDPrintM(DBG_LURN_INFO, ("case RAID_STATUS_EMERGENCY\n"));
				if(!(GENERIC_WRITE & Lurn->AccessRight))
				{
					// keep emergency for read only, secondary
					// AING_TO_DO : we will add RMD check code later
					KDPrintM(DBG_LURN_INFO, ("keep emergency for read only, secondary\n"));
					break;
				}

				{
					BOOLEAN try_revive = FALSE;
					UINT32 new_raid_status;
					RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
					// try to revive all the dead children
					KDPrintM(DBG_LURN_INFO, ("try to revive all the dead children\n"));
					for(i = 0; i < Lurn->LurnChildrenCnt; i++)
					{
						if(LURN_IS_RUNNING(pRaidInfo->MapLurnChildren[i]->LurnStatus))
							continue;

						KDPrintM(DBG_LURN_TRACE, ("Reviving pRaidInfo->MapLurnChildren[%d] = %p\n", i, pRaidInfo->MapLurnChildren[i]));
						status = LurnInitialize(pRaidInfo->MapLurnChildren[i], pRaidInfo->MapLurnChildren[i]->Lur, pRaidInfo->MapLurnChildren[i]->LurnDesc);
						if(NT_SUCCESS(status))
						{
							KDPrintM(DBG_LURN_ERROR, ("!!! SUCCESS to revive pRaidInfo->MapLurnChildren[%d] : %p!!!\n", i, pRaidInfo->MapLurnChildren[i]));
							try_revive = TRUE;
							if(Lurn->LurnDesc) {
								//
								//	Assume that nobody acceses LurnDesc while this thread is running.
								//

								ExFreePool(Lurn->LurnDesc);
								Lurn->LurnDesc = NULL;
							}
						}
					}
					KDPrintM(DBG_LURN_INFO, ("try_revive = %d\n", try_revive));

					// find any alive spare
					KDPrintM(DBG_LURN_INFO, ("Finding any alive spare %d ~ %d -1\n", nDiskCount, Lurn->LurnChildrenCnt));
					for(i = nDiskCount; i < Lurn->LurnChildrenCnt; i++)
					{
						KDPrintM(DBG_LURN_TRACE, ("pRaidInfo->MapLurnChildren[%d]->LurnStatus == %d\n", i, pRaidInfo->MapLurnChildren[i]->LurnStatus));
						if(LURN_IS_RUNNING(pRaidInfo->MapLurnChildren[i]->LurnStatus))
						{
							KDPrintM(DBG_LURN_ERROR, ("!!! Spare alive pRaidInfo->MapLurnChildren[%d] : %p!!!\n", i, pRaidInfo->MapLurnChildren[i]));
							try_revive = TRUE;
						}
					}
					KDPrintM(DBG_LURN_INFO, ("try_revive = %d\n", try_revive));

					// ok, something changed
					if(try_revive)
					{
						status = LurnRefreshRaidStatus(Lurn, &new_raid_status);
						ASSERT(NT_SUCCESS(status));
						ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
						ASSERT(RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus);
						if(RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus)
						{
							pRaidInfo->RaidStatus = new_raid_status;
						}
					}
					else
					{
						ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
					}
				}

				break;
			case RAID_STATUS_TERMINATING: // set at destroy
				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
				goto terminate;
				break;
			case RAID_STATUS_FAIL:
				// usually impossible
				ASSERT(FALSE);
				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
				goto fail;
				break;
			}
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

			// back to RAID LOOP
			continue;
		}

		// prepare to recover
		for(i = 0, j = 0; i < nDiskCount; i++)
		{
			if(NDAS_UNIT_META_BIND_STATUS_FAULT & rmd->UnitMetaData[i].UnitDeviceStatus)
			{
				LurnDefected = pRaidInfo->MapLurnChildren[i];
				LurnRecoverInfo = 
					pRaidInfo->MapLurnChildren[(nDiskCount -1 == i) ? 0 : i +1];
				continue;
			}

			LurnsHealthy[j] = pRaidInfo->MapLurnChildren[i];
			j++;
		}
		ASSERT(i == j +1);
		
		bit_to_recover = RtlFindSetBits(Bitmap, 1, 0); // find first bit.
		bit_recovered = 0xFFFFFFFF; // not any recovered yet
		KDPrintM(DBG_LURN_INFO, ("STARTS RECOVERING -> %08lx\n",	bit_to_recover));

		while(TRUE) // Recover LOOP
		{
			ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_INITIAILIZING: // impossible pass
			case RAID_STATUS_NORMAL: // impossible pass
			case RAID_STATUS_EMERGENCY: // set at completion
			case RAID_STATUS_EMERGENCY_READY:
				ASSERT(FALSE);
				break;
			case RAID_STATUS_RECOVERRING: // ok, proceed
				break;
			case RAID_STATUS_TERMINATING: // set at destroy
				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
				goto terminate;
				break;
			case RAID_STATUS_FAIL:
				// usually impossible
				ASSERT(FALSE);
				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
				goto fail;
				break;
			}
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

			// prepare to recover one bit
			// we recover sector_to_recover_begin ~ sector_to_recover_begin + SectorsPerBit -1
			sector_to_recover_begin = bit_to_recover * SectorsPerBit;
			sector_to_recover_now = sector_to_recover_begin;
			sectors_to_recover_now = (USHORT)sectors_each_child;

			// Recover Bit LOOP
			success_on_recover = TRUE;
			while(
				sector_to_recover_now < sector_to_recover_begin + SectorsPerBit && // for 1 bit
				sector_to_recover_now < Lurn->UnitBlocks) // user space limit
			{
				if(sector_to_recover_now + sectors_to_recover_now > sector_to_recover_begin + SectorsPerBit)
					sectors_to_recover_now = (USHORT)(sector_to_recover_begin + SectorsPerBit - sector_to_recover_now);

				// READ sectors from the healthy LURN
				status = LurnExecuteSync(
					nDiskCount -1,
					LurnsHealthy,
					SCSIOP_READ,
					bufs_read_from_healthy,
					sector_to_recover_now,
					sectors_to_recover_now,
					NULL);

				if(!NT_SUCCESS(status))
				{
					success_on_recover = FALSE;
					break;
				}

				// create BufferRecover
				if(LURN_RAID4 == Lurn->LurnType)
				{
					// parity work
					RtlCopyMemory(buf_write_to_fault, bufs_read_from_healthy[0], buf_size_each_child);
					for(i = 1; i < nDiskCount -1; i++)
					{
						parity_tar_ptr = (PULONG)buf_write_to_fault;
						parity_src_ptr = (PULONG)bufs_read_from_healthy[i];

						j = (buf_size_each_child) / sizeof(ULONG);
						while(j--)
						{
							*parity_tar_ptr ^= *parity_src_ptr;
							parity_tar_ptr++;
							parity_src_ptr++;
						}
					}
				}
				else if(LURN_RAID1 == Lurn->LurnType)
				{
					ASSERT(buf_write_to_fault == bufs_read_from_healthy[0]);
				}

				// WRITE sectors to the defected LURN
				status = LurnExecuteSync(
					1,
					&LurnDefected,
					SCSIOP_WRITE,
					&buf_write_to_fault,
					sector_to_recover_now,
					sectors_to_recover_now,
					NULL);

				if(!NT_SUCCESS(status))
				{
					success_on_recover = FALSE;
					break;
				}

				sector_to_recover_now += sectors_to_recover_now;
			} // Recover Bit LOOP

			if(!success_on_recover)
				break;

			// clear the bit & find next set bit
			ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
			bit_recovered = bit_to_recover;
			RtlClearBits(Bitmap, bit_recovered, 1);
			bit_to_recover = RtlFindSetBits(Bitmap, 1, bit_recovered); // find next bit.
			pRaidInfo->BitmapIdxToRecover = bit_to_recover;
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
			if(sector_to_recover_now < Lurn->UnitBlocks)
			{
				// removing noisy trace
				if(bit_to_recover & 0xff && bit_recovered +1 == bit_to_recover)
				{
					KDPrintM(DBG_LURN_TRACE, ("-> %08lx\n", bit_to_recover));
				}
				else
				{
					KDPrintM(DBG_LURN_INFO, ("-> %08lx\n", bit_to_recover));
				}
			}
			else
			{
				KDPrintM(DBG_LURN_NOISE, ("-> %08lx\n", bit_to_recover));
			}

			if(bit_to_recover / (SECTOR_SIZE * 8) != bit_recovered / (SECTOR_SIZE * 8))
			{
				// one full sector of bitmap in memory is cleared(or recovery complete), clears Bitmap in disk
				KDPrintM(DBG_LURN_INFO, ("<<< Clear a bitmap sector >>> : %08lx\n", bit_recovered));
				RtlZeroMemory(&OneSector, sizeof(OneSector));
				status = LurnExecuteSyncWrite(
					LurnRecoverInfo,
					OneSector,
					NDAS_BLOCK_LOCATION_BITMAP + (bit_recovered / (SECTOR_SIZE * 8)),
					1);

				if(!NT_SUCCESS(status))
				{
					success_on_recover = FALSE;
					break;
				}				
			}

			if(0xFFFFFFFF == bit_to_recover)
			{
				// recovery complete
				success_on_recover = TRUE;
				break;
			}

		} // Recover LOOP


		ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
		switch(pRaidInfo->RaidStatus)
		{
		case RAID_STATUS_NORMAL:
		case RAID_STATUS_EMERGENCY:
		case RAID_STATUS_EMERGENCY_READY:
		case RAID_STATUS_INITIAILIZING:
			// impossible pass
			ASSERT(FALSE);
			break;
		case RAID_STATUS_RECOVERRING:
			if(!success_on_recover)
			{
				pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY;
				break;
			}

			// recover complete
			{
				// recover complete or nothing to recover
				// clear RMD flags
				for(i = 0; i < nDiskCount; i++)
				{
					rmd->UnitMetaData[i].UnitDeviceStatus = 0;
				}

				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
				status = LurnRMDWrite(Lurn);
				ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
				if(!NT_SUCCESS(status))
				{
					KDPrintM(DBG_LURN_ERROR, ("LurnRMDWrite Failed\n"));
				}

				KDPrintM(DBG_LURN_INFO, ("!!! RECOVERY COMPLETE !!!\n"));
				pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;
			}
			break;
		case RAID_STATUS_TERMINATING: // set at destroy
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
			goto terminate;
			break;
		case RAID_STATUS_FAIL:
			// usually impossible
			ASSERT(FALSE);
			RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
			goto fail;
			break;
		}
		RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

			// find next bit to recover
	} // RAID LOOP
fail:
	// wait for terminate thread event
	KDPrintM(DBG_LURN_INFO, ("KeWaitForSingleObject ...\n"));

	status = KeWaitForSingleObject(
		&pRaidInfo->RecoverThreadEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	ASSERT(RAID_STATUS_TERMINATING ==  pRaidInfo->RaidStatus);

terminate:
	// terminate thread

	if(buf_for_recovery)
	{
		ExFreePoolWithTag(buf_for_recovery, RAID_RECOVER_POOL_TAG);
		buf_for_recovery = NULL;
	}

	KDPrintM(DBG_LURN_INFO, ("Terminated\n"));
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

			KDPrintM(DBG_LURN_TRACE,
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
			KDPrintM(DBG_LURN_TRACE,
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
//	PRAID_CHILD_INFO pChildInfo;

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
	ulDataBufferSizePerDisk = 0xFFFFFFFF;
	for(i = 0; i < (ULONG)Lurn->LurnChildrenCnt; i++)
	{
		if(ulDataBufferSizePerDisk > LurnDesc->MaxBlocksPerRequest * BLOCK_SIZE)
			ulDataBufferSizePerDisk = LurnDesc->MaxBlocksPerRequest * BLOCK_SIZE;
	}

//	ulDataBufferSizePerDisk = 128 * BLOCK_SIZE / (Lurn->LurnChildrenCnt);
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
//		pChildInfo = &pRaidInfo->Children[i];

		pRaidInfo->DataBuffers[i] =
			(PCHAR)pRaidInfo->DataBufferAllocated + i * ulDataBufferSizePerDisk;

//		KDPrintM(DBG_LURN_ERROR, ("pRaidInfo->Children[%d]->DataBuffer = %x\n", i, pRaidInfo->Children[i].DataBuffer));
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
						(PCHAR)pRaidInfo->DataBuffers[i] + j * BLOCK_SIZE,
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
						(PCHAR)pRaidInfo->DataBuffers[i] + j * BLOCK_SIZE,
						(PCHAR)Ccb->DataBuffer + (i + j * (Lurn->LurnChildrenCnt)) * BLOCK_SIZE,
						BLOCK_SIZE);
				}

			}

			// initialize cdb, LurnChildren
			
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{			
				cdb.DataBuffer[i] = pRaidInfo->DataBuffers[i];
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
				cdb.DataBuffer[i] = pRaidInfo->DataBuffers[i];
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

		KDPrintM(DBG_LURN_TRACE, ("SCSIOP_READ_CAPACITY: %08x : %04x\n", (UINT32)logicalBlockAddress, (UINT32)blockSize));
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
			KDPrintM(DBG_LURN_TRACE,
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

	OBJECT_ATTRIBUTES objectAttributes;

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);

	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnType = %d\n", LurnDesc->LurnType));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnId = %d\n", LurnDesc->LurnId));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->StartBlockAddr = 0x%I64x\n", LurnDesc->StartBlockAddr));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->EndBlockAddr = 0x%I64x\n", LurnDesc->EndBlockAddr));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->UnitBlocks = 0x%I64x\n", LurnDesc->UnitBlocks));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->BytesInBlock = %d\n", LurnDesc->BytesInBlock));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->MaxBlocksPerRequest = %d\n", LurnDesc->MaxBlocksPerRequest));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnOptions = %d\n", LurnDesc->LurnOptions));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnParent = %d\n", LurnDesc->LurnParent));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildrenCnt = %d\n", LurnDesc->LurnChildrenCnt));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildren = 0x%p\n", LurnDesc->LurnChildren));

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

	// set spare disk count
	pRaidInfo->nDiskCount = LurnDesc->LurnChildrenCnt - LurnDesc->LurnInfoRAID.nSpareDisk;
	pRaidInfo->nSpareDisk = LurnDesc->LurnInfoRAID.nSpareDisk;

	pRaidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID.SectorsPerBit;
	if(!pRaidInfo->SectorsPerBit)
	{
		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;
	}

	pRaidInfo->MaxBlocksPerRequest = LurnDesc->MaxBlocksPerRequest;

	// RAID 1 does not need data buffer for shuffling
	{
		UINT32 i;

		pRaidInfo->DataBufferAllocated = NULL;
		for(i = 0; i < pRaidInfo->nDiskCount; i++)
		{
			pRaidInfo->DataBuffers[i] = NULL;
		}
	}

	// prepare extended commands
	// ExtCmdLws
	pRaidInfo->ExtCmdLws.pLurnCreated = NULL; // Lurn; // should not to be deleted at completion routine
	pRaidInfo->ExtCmdLws.Operation = CCB_EXT_WRITE;
	pRaidInfo->ExtCmdLws.CountBack = TRUE;
	pRaidInfo->ExtCmdLws.logicalBlockAddress = (ULONG)(-1 * NDAS_BLOCK_LOCATION_WRITE_LOG);
	pRaidInfo->ExtCmdLws.LengthBlock = 1;
	pRaidInfo->ExtCmdLws.ByteOperation = EXT_BLOCK_OPERATION;
	pRaidInfo->ExtCmdLws.pNextCmd = NULL;
	pRaidInfo->ExtCmdLws.pByteData = (PBYTE)&pRaidInfo->LWSs;

	// ExtCmdBitmap
	pRaidInfo->ExtCmdBitmap.pLurnCreated = NULL; // Lurn; // should not to be deleted at completion routine
	pRaidInfo->ExtCmdBitmap.Operation = CCB_EXT_WRITE;
	pRaidInfo->ExtCmdBitmap.CountBack = TRUE;
//	pRaidInfo->ExtCmdBitmap.logicalBlockAddress = ; // dynamic
	pRaidInfo->ExtCmdBitmap.Offset = 0;
	pRaidInfo->ExtCmdBitmap.ByteOperation = EXT_BLOCK_OPERATION;
//	pRaidInfo->ExtCmdBitmap.pByteData = ; // dynamic
	pRaidInfo->ExtCmdBitmap.pNextCmd = NULL;

	// Create & init BITMAP
	{
	//	Bitmap (1) * (bitmap structure size + bitmap size)
		ulBitMapSize = (ULONG)NDAS_BLOCK_SIZE_BITMAP * BLOCK_SIZE; // use full bytes 1MB(8Mb) of bitmap
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
	}

	KeInitializeSpinLock(&pRaidInfo->LockInfo);

	// Create recover thread
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

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL : LurnRAIDThreadProcRecover !!!\n"));
		ntStatus = STATUS_THREAD_NOT_IN_PROCESS;
		goto out;
	}

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAIDInfo)
		{
			if(pRaidInfo->Bitmap)
			{
				ExFreePoolWithTag(pRaidInfo->Bitmap, RAID_BITMAP_POOL_TAG);
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
	ASSERT(STATUS_TIMEOUT != status);

	ObDereferenceObject(pRaidInfo->ThreadRecoverObject);

	ASSERT(!pRaidInfo->DataBufferAllocated);

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
	)
{
	KIRQL	oldIrql, oldIrqlRaidInfo;
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

	ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrqlRaidInfo);

	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:	// priority 2
		break;

	case CCB_STATUS_BUSY:		// priority 3
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb || OriginalCcb->OperationCode == CCB_OPCODE_RESETBUS);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:		// priority 1/5
		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP))
		{
			PLURELATION_NODE pLurnChildDefected;
			ULONG i;

			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP);	// priority 1
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE);

			KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, CCBSTATUS_FLAG_LURN_STOP not flagged\n"));
			KDPrintM(DBG_LURN_INFO, ("pRaidInfo->RaidStatus : %d\n", pRaidInfo->RaidStatus));

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
					KDPrintM(DBG_LURN_ERROR, ("RAID_STATUS_NORMAL != pRaidInfo->RaidStatus(%d)\n", pRaidInfo->RaidStatus));

					if(pLurnChildDefected != pRaidInfo->MapLurnChildren[pRaidInfo->iChildDefected])
					{
						ASSERT(FALSE);
						//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
						break;
					}
				}

				// set pRaidInfo->iChildDefected
				for(i = 0; i < pRaidInfo->nDiskCount; i++)
				{
					if(pLurnChildDefected == pRaidInfo->MapLurnChildren[i])
					{
						pRaidInfo->iChildDefected = i;
						pRaidInfo->iChildRecoverInfo = 
							(pRaidInfo->nDiskCount -1 == pRaidInfo->iChildDefected) ? 0 : pRaidInfo->iChildDefected +1;
						break;
					}
				}

				// failed to find a defected child
				if(i == pRaidInfo->nDiskCount)
				{
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					break;
				}

				if (RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus ||
					RAID_STATUS_EMERGENCY_READY != pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("Set from %d to RAID_STATUS_EMERGENCY_READY\n", pRaidInfo->RaidStatus));

					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY_READY;
					pRaidInfo->rmd.UnitMetaData[pRaidInfo->iChildDefected].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_FAULT;

					for(i = 0; i < pRaidInfo->nDiskCount; i++)
					{
						// 1 fail + 1 fail = broken
						if(NDAS_UNIT_META_BIND_STATUS_FAULT & pRaidInfo->rmd.UnitMetaData[i].UnitDeviceStatus &&
							i != pRaidInfo->iChildDefected)
						{
							ASSERT(FALSE);
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;
							break;
						}
					}

					if(CCB_STATUS_STOP == OriginalCcb->CcbStatus)
						break;
				}

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iChildDefected = %d\n", pRaidInfo->iChildDefected));
			}
		} else {
			//
			//	at least two children stopped!
			//
			ASSERT(FALSE);
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
		}
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE)) {
			//
			//	at least two children problem.! (1 stop, 1 not exist)
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
			//	at least two children have an error or do not exist! (2 not exist)
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

	RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrqlRaidInfo);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

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

	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnRAID1Execute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) 
{
	NTSTATUS			status;

	PRAID_INFO pRaidInfo;
	PCMD_BYTE_OP	pExtendedCommands[NDAS_MAX_RAID1_CHILD];
	KIRQL				oldIrql;

	RtlZeroMemory(pExtendedCommands, NDAS_MAX_RAID1_CHILD * sizeof(PCMD_BYTE_OP));

	pRaidInfo = Lurn->LurnRAIDInfo;
	// record a bitmap information to the next disk of the defected disk
	// pExtendedCommands itself will go into LurnAssocSendCcbToChildrenArray
	if(RAID_STATUS_INITIAILIZING != pRaidInfo->RaidStatus)
	{
		ASSERT(pRaidInfo->nDiskCount > 0 && pRaidInfo->nDiskCount <= NDAS_MAX_RAID4_CHILD);
	}

	if(
		RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
		RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus 
		)
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
					RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus || // do not IO when reading bitmap
					RAID_STATUS_EMERGENCY_READY == pRaidInfo->RaidStatus || // emergency mode not ready
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

					KDPrintM(DBG_LURN_INFO, ("!! RECOVER THREAD PROTECTION : %08lx, cmd : 0x%x, %x, %d\n", pRaidInfo->RaidStatus, (UINT32)Ccb->Cdb[0], logicalBlockAddress, transferBlocks));
					Ccb->CcbStatus = CCB_STATUS_BUSY;
//					LSCcbCompleteCcb(Ccb);
					goto complete_here;
				}

				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
			}
	}

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;
			register ULONG i;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_NORMAL:
				// record last written sector
				{
					PLAST_WRITTEN_SECTOR pLWS;

					// AING_TO_DO : use Ready made pool
					for(i = 0; i < pRaidInfo->nDiskCount; i++)
					{
						pExtendedCommands[i] = &pRaidInfo->ExtCmdLws;
						pExtendedCommands[i]->pNextCmd = NULL;

					}

					pLWS = &pRaidInfo->LWSs.LWS[pRaidInfo->timeStamp % 32];

					pLWS->logicalBlockAddress = (UINT64)logicalBlockAddress;
					pLWS->transferBlocks = (UINT32)transferBlocks;
					pLWS->timeStamp = pRaidInfo->timeStamp;

					pRaidInfo->timeStamp++;
				}
				break;
			case RAID_STATUS_EMERGENCY:
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY\n"));

					// use pRaidInfo->ExtCmdBitmap instead allocating
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
						pRaidInfo->ExtCmdBitmap.pByteData = 
							(PCHAR)(pRaidInfo->Bitmap->Buffer) + (uiBitmapStartInBits / BITS_PER_BLOCK) * BLOCK_SIZE;
						pRaidInfo->ExtCmdBitmap.LengthBlock = 
							(uiBitmapStartInBits / BITS_PER_BLOCK == uiBitmapEndInBits) ? 1 : 2;
						pRaidInfo->ExtCmdBitmap.logicalBlockAddress = 
							(ULONG)(-1 * NDAS_BLOCK_LOCATION_BITMAP) - (uiBitmapStartInBits / BITS_PER_BLOCK);
						pExtendedCommands[pRaidInfo->iChildRecoverInfo] = &pRaidInfo->ExtCmdBitmap;
					}

					// split is super ultra hyper rare case
					if(pRaidInfo->ExtCmdBitmap.LengthByte == BLOCK_SIZE * 2)
					{
						KDPrintM(DBG_LURN_ERROR,("RAID_STATUS_EMERGENCY bitmap changed DOUBLE : This is not error but ultra rare case. Check values if stop here\n"));
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
			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->MapLurnChildren,
				pRaidInfo->nDiskCount,
				Ccb,
				LurnRAID1CcbCompletion,
				NULL,
				pExtendedCommands,
				FALSE
				);
		}
		break;
	case SCSIOP_VERIFY:
		{
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_VERIFY\n"));

			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->MapLurnChildren,
				pRaidInfo->nDiskCount,
				Ccb,
				LurnRAID1CcbCompletion,
				NULL,
				pExtendedCommands,
				FALSE
				);
		}
		break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
		{
		ULONG				idx_child;
		
		//
		//	Find a child LURN to run.
		//

		if (RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
			RAID_STATUS_EMERGENCY_READY == pRaidInfo->RaidStatus)
		{
			idx_child = pRaidInfo->iChildRecoverInfo;
		}
		else
		{
			// possibly apply balanced reading
			idx_child = 0;
		}

		KDPrintM(DBG_LURN_TRACE,("SCSIOP_READ: decided LURN#%d\n", idx_child));
		//
		//	Set completion routine
		//
		status = LurnAssocSendCcbToChildrenArray(
			&pRaidInfo->MapLurnChildren[idx_child],
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
			goto complete_here;
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
		goto complete_here;
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

			KDPrintM(DBG_LURN_TRACE, ("SCSIOP_READ_CAPACITY: %08x : %04x\n", (UINT32)logicalBlockAddress, (UINT32)blockSize));
			goto complete_here;
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
				goto complete_here;
			} else {
				KDPrintM(DBG_LURN_TRACE,
					("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
				LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
				Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
				goto complete_here;
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
				LurnRAID1CcbCompletion,
				NULL,
				NULL,
				FALSE
				);
		}
		break;

	}

	return STATUS_SUCCESS;
complete_here:
	if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_RECOVERING);
	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID1Request(
				 PLURELATION_NODE	Lurn,
				 PCCB				Ccb
				 )
{
	NTSTATUS				status;
	PRAID_INFO pRaidInfo;

	pRaidInfo = Lurn->LurnRAIDInfo;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("IN\n"));

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
				//			LSCcbCompleteCcb(Ccb);
				goto complete_here;
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
				//			LSCcbCompleteCcb(Ccb);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL, TRUE);
			break;
		}

	case CCB_OPCODE_RECOVER:
		{
			// we do nothing for this command anymore.
			// miniport driver(this) itself check recover status
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			//			LSCcbCompleteCcb(Ccb);
			goto complete_here;
		}
		break;

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		//		LSCcbCompleteCcb(Ccb);
		goto complete_here;
		break;
	}

	return STATUS_SUCCESS;

complete_here:
	if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_RECOVERING);
	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID4Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_INFO pRaidInfo = NULL;
	ULONG ulBitMapSize;
	NTSTATUS ntStatus;

	OBJECT_ATTRIBUTES objectAttributes;

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);

	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnType = %d\n", LurnDesc->LurnType));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnId = %d\n", LurnDesc->LurnId));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->StartBlockAddr = 0x%I64x\n", LurnDesc->StartBlockAddr));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->EndBlockAddr = 0x%I64x\n", LurnDesc->EndBlockAddr));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->UnitBlocks = 0x%I64x\n", LurnDesc->UnitBlocks));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->BytesInBlock = %d\n", LurnDesc->BytesInBlock));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->MaxBlocksPerRequest = %d\n", LurnDesc->MaxBlocksPerRequest));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnOptions = %d\n", LurnDesc->LurnOptions));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnParent = %d\n", LurnDesc->LurnParent));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildrenCnt = %d\n", LurnDesc->LurnChildrenCnt));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildren = 0x%p\n", LurnDesc->LurnChildren));

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

	// set spare disk count
	pRaidInfo->nDiskCount = LurnDesc->LurnChildrenCnt - LurnDesc->LurnInfoRAID.nSpareDisk;
	pRaidInfo->nSpareDisk = LurnDesc->LurnInfoRAID.nSpareDisk;

	pRaidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID.SectorsPerBit;
	if(!pRaidInfo->SectorsPerBit)
	{
		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;
	}

	pRaidInfo->MaxBlocksPerRequest = LurnDesc->MaxBlocksPerRequest;

	// allocate shuffle data buffer
	{
		ULONG ulDataBufferSizePerDisk;
		ULONG ulDataBufferSize;
		UINT32 i;
		ulDataBufferSizePerDisk = pRaidInfo->MaxBlocksPerRequest * BLOCK_SIZE / (pRaidInfo->nDiskCount -1);
		ulDataBufferSize = pRaidInfo->nDiskCount * ulDataBufferSizePerDisk;

		KDPrintM(DBG_LURN_INFO, 
			("allocating data buffer. ulDataBufferSizePerDisk = %d, ulDataBufferSize = %d\n",
			ulDataBufferSizePerDisk, ulDataBufferSize));
		ASSERT(NULL == pRaidInfo->DataBufferAllocated);
		pRaidInfo->DataBufferAllocated = ExAllocatePoolWithTag(NonPagedPool, ulDataBufferSize, 
			RAID_DATA_BUFFER_POOL_TAG);

		if(NULL == pRaidInfo->DataBufferAllocated)
		{
			pRaidInfo->RaidStatus = RAID_STATUS_FAIL;
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			goto out;
		}

		// map Data buffer
		for(i = 0; i < pRaidInfo->nDiskCount; i++)
		{
			pRaidInfo->DataBuffers[i] = pRaidInfo->DataBufferAllocated + i * ulDataBufferSizePerDisk;
			KDPrintM(DBG_LURN_INFO, 
				("pRaidInfo->DataBuffers[%d] = %p\n",
				i, pRaidInfo->DataBuffers[i]));
		}
	}

	// init raid information lock
	pRaidInfo->LockDataBuffer = 0;

	// prepare extended commands
	// ExtCmdLws
	pRaidInfo->ExtCmdLws.pLurnCreated = NULL; // Lurn; // should not to be deleted at completion routine
	pRaidInfo->ExtCmdLws.Operation = CCB_EXT_WRITE;
	pRaidInfo->ExtCmdLws.CountBack = TRUE;
	pRaidInfo->ExtCmdLws.logicalBlockAddress = (ULONG)(-1 * NDAS_BLOCK_LOCATION_WRITE_LOG);
	pRaidInfo->ExtCmdLws.LengthBlock = 1;
	pRaidInfo->ExtCmdLws.ByteOperation = EXT_BLOCK_OPERATION;
	pRaidInfo->ExtCmdLws.pNextCmd = NULL;
	pRaidInfo->ExtCmdLws.pByteData = (PBYTE)&pRaidInfo->LWSs;

	// ExtCmdBitmap
	pRaidInfo->ExtCmdBitmap.pLurnCreated = NULL; // Lurn; // should not to be deleted at completion routine
	pRaidInfo->ExtCmdBitmap.Operation = CCB_EXT_WRITE;
	pRaidInfo->ExtCmdBitmap.CountBack = TRUE;
//	pRaidInfo->ExtCmdBitmap.logicalBlockAddress = ; // dynamic
	pRaidInfo->ExtCmdBitmap.Offset = 0;
	pRaidInfo->ExtCmdBitmap.ByteOperation = EXT_BLOCK_OPERATION;
//	pRaidInfo->ExtCmdBitmap.pByteData = ; // dynamic
	pRaidInfo->ExtCmdBitmap.pNextCmd = NULL;

	// Create & init BITMAP
	{
	//	Bitmap (1) * (bitmap structure size + bitmap size)
		ulBitMapSize = (ULONG)NDAS_BLOCK_SIZE_BITMAP * BLOCK_SIZE; // use full bytes 1MB(8Mb) of bitmap
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
	}

	KeInitializeSpinLock(&pRaidInfo->LockInfo);

	// Create recover thread
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

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL : LurnRAIDThreadProcRecover !!!\n"));
		ntStatus = STATUS_THREAD_NOT_IN_PROCESS;
		goto out;
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
			if(pRaidInfo->Bitmap)
			{
				ExFreePoolWithTag(pRaidInfo->Bitmap, RAID_BITMAP_POOL_TAG);
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
	ASSERT(STATUS_TIMEOUT != status);

	ObDereferenceObject(pRaidInfo->ThreadRecoverObject);

	ASSERT(pRaidInfo->DataBufferAllocated);
	ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG) ;
	pRaidInfo->DataBufferAllocated = NULL;
	
	ASSERT(pRaidInfo->Bitmap);
	ExFreePoolWithTag(pRaidInfo->Bitmap, BITMAP_POOL_TAG) ;
	pRaidInfo->Bitmap = NULL;

	ExFreePoolWithTag(pRaidInfo, RAID_INFO_POOL_TAG) ;
	pRaidInfo = NULL;

	return STATUS_SUCCESS ;
}

NTSTATUS
LurnRAID4CcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	)
{
	KIRQL	oldIrql, oldIrqlRaidInfo;
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

	ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrqlRaidInfo);

	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:	// priority 2
		break;

	case CCB_STATUS_BUSY:		// priority 3
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb || OriginalCcb->OperationCode == CCB_OPCODE_RESETBUS);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:		// priority 1/5
		if(!LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP))
		{
			PLURELATION_NODE pLurnChildDefected;
			ULONG i;

			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP);	// priority 1
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE);

			KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, CCBSTATUS_FLAG_LURN_STOP not flagged\n"));
			KDPrintM(DBG_LURN_INFO, ("pRaidInfo->RaidStatus : %d\n", pRaidInfo->RaidStatus));

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

					if(pLurnChildDefected != pRaidInfo->MapLurnChildren[pRaidInfo->iChildDefected])
					{
						ASSERT(FALSE);
						//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
						break;
					}
				}

				// set pRaidInfo->iChildDefected
				for(i = 0; i < pRaidInfo->nDiskCount; i++)
				{
					if(pLurnChildDefected == pRaidInfo->MapLurnChildren[i])
					{
						pRaidInfo->iChildDefected = i;
						pRaidInfo->iChildRecoverInfo = 
							(pRaidInfo->nDiskCount -1 == pRaidInfo->iChildDefected) ? 0 : pRaidInfo->iChildDefected +1;
						break;
					}
				}

				// failed to find a defected child
				if(i == pRaidInfo->nDiskCount)
				{
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					break;
				}

				if (RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus ||
					RAID_STATUS_EMERGENCY_READY != pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("Set from %d to RAID_STATUS_EMERGENCY_READY\n", pRaidInfo->RaidStatus));

					pRaidInfo->RaidStatus = RAID_STATUS_EMERGENCY_READY;
					pRaidInfo->rmd.UnitMetaData[pRaidInfo->iChildDefected].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_FAULT;

					for(i = 0; i < pRaidInfo->nDiskCount; i++)
					{
						// 1 fail + 1 fail = broken
						if(NDAS_UNIT_META_BIND_STATUS_FAULT & pRaidInfo->rmd.UnitMetaData[i].UnitDeviceStatus &&
							i != pRaidInfo->iChildDefected)
						{
							ASSERT(FALSE);
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;
							break;
						}
					}

					if(CCB_STATUS_STOP == OriginalCcb->CcbStatus)
						break;
				}

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iChildDefected = %d\n", pRaidInfo->iChildDefected));
			}
		} else {
			//
			//	at least two children stopped!
			//
			ASSERT(FALSE);
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
		}
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP_INDICATE)) {
			//
			//	at least two children problem.! (1 stop, 1 not exist)
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
			//	at least two children have an error or do not exist! (2 not exist)
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

	RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrqlRaidInfo);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

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
					pLurnOriginal->LurnRAIDInfo->iChildDefected != pRaidInfo->nDiskCount -1) // not parity
				{
					bDataBufferToRecoverInitialized = FALSE;

					for(i = 0; i < pRaidInfo->nDiskCount; i++)
					{
						// skip defected
						if(pLurnOriginal->LurnRAIDInfo->iChildDefected == i)
							continue;

						pDataBufferSrc = (PULONG)pRaidInfo->DataBuffers[i];
						pDataBufferToRecover = (PULONG)pRaidInfo->DataBuffers[pLurnOriginal->LurnRAIDInfo->iChildDefected];

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
				for(i = 0; i < pRaidInfo->nDiskCount -1; i++)
				{
					for(j = 0; j < BlocksPerDisk; j++)
					{
						RtlCopyMemory( // Copy back
							(PCHAR)OriginalCcb->DataBuffer + (i + j * (pRaidInfo->nDiskCount -1)) * BLOCK_SIZE,
							(PCHAR)pRaidInfo->DataBuffers[i] + j * BLOCK_SIZE,
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
	) 
{
	NTSTATUS			status;

	PRAID_INFO pRaidInfo;
	PCMD_BYTE_OP	pExtendedCommands[NDAS_MAX_RAID4_CHILD];
	KIRQL	oldIrql;

	RtlZeroMemory(pExtendedCommands, NDAS_MAX_RAID4_CHILD * sizeof(PCMD_BYTE_OP));

	pRaidInfo = Lurn->LurnRAIDInfo;
	// record a bitmap information to the next disk of the defected disk
	// pExtendedCommands itself will go into LurnAssocSendCcbToChildrenArray
	if(RAID_STATUS_INITIAILIZING != pRaidInfo->RaidStatus)
	{
		ASSERT(pRaidInfo->nDiskCount > 0 && pRaidInfo->nDiskCount <= NDAS_MAX_RAID4_CHILD);
	}

	if(
		RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
		RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus 
		)
	{
		ASSERT(pRaidInfo->iChildDefected < pRaidInfo->nDiskCount);
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
					RAID_STATUS_EMERGENCY_READY == pRaidInfo->RaidStatus || // emergency mode not ready
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

					KDPrintM(DBG_LURN_INFO, ("!! RECOVER THREAD PROTECTION : %08lx, cmd : 0x%x, %x, %d\n", pRaidInfo->RaidStatus, (UINT32)Ccb->Cdb[0], logicalBlockAddress, transferBlocks));
					Ccb->CcbStatus = CCB_STATUS_BUSY;
//					LSCcbCompleteCcb(Ccb);
					goto complete_here;
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
			register ULONG i, j;
			register int k;

			int DataBufferLengthPerDisk;
			ULONG BlocksPerDisk;
			PULONG pDataBufferParity, pDataBufferSrc;
			CUSTOM_DATA_BUFFER cdb;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_NORMAL:
				// record last written sector
				{
					PLAST_WRITTEN_SECTOR pLWS;

					// AING_TO_DO : use Ready made pool
					for(i = 0; i < pRaidInfo->nDiskCount; i++)
					{
						pExtendedCommands[i] = &pRaidInfo->ExtCmdLws;
						pExtendedCommands[i]->pNextCmd = NULL;

					}

					pLWS = &pRaidInfo->LWSs.LWS[pRaidInfo->timeStamp % 32];

					pLWS->logicalBlockAddress = (UINT64)logicalBlockAddress;
					pLWS->transferBlocks = (UINT32)transferBlocks;
					pLWS->timeStamp = pRaidInfo->timeStamp;

					pRaidInfo->timeStamp++;
				}
				break;
			case RAID_STATUS_EMERGENCY:
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY\n"));
					
					// use pRaidInfo->ExtCmdBitmap instead allocating
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
						pRaidInfo->ExtCmdBitmap.pByteData = 
							(PCHAR)(pRaidInfo->Bitmap->Buffer) + (uiBitmapStartInBits / BITS_PER_BLOCK) * BLOCK_SIZE;
						pRaidInfo->ExtCmdBitmap.LengthBlock = 
							(uiBitmapStartInBits / BITS_PER_BLOCK == uiBitmapEndInBits) ? 1 : 2;
						pRaidInfo->ExtCmdBitmap.logicalBlockAddress = 
							(ULONG)(-1 * NDAS_BLOCK_LOCATION_BITMAP) - (uiBitmapStartInBits / BITS_PER_BLOCK);						
						pExtendedCommands[pRaidInfo->iChildRecoverInfo] = &pRaidInfo->ExtCmdBitmap;
					}

					// split is super ultra hyper rare case
					if(pRaidInfo->ExtCmdBitmap.LengthByte == BLOCK_SIZE * 2)
					{
						KDPrintM(DBG_LURN_ERROR,("RAID_STATUS_EMERGENCY bitmap changed DOUBLE : This is not error but ultra rare case. Check values if stop here\n"));
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

			// create new data buffer and encrypt here.
			// new data buffer will be deleted at completion routine
			DataBufferLengthPerDisk = Ccb->DataBufferLength / (pRaidInfo->nDiskCount -1);
			BlocksPerDisk = DataBufferLengthPerDisk / BLOCK_SIZE;
			
			// split DataBuffer into each DataBuffers of children by block size
			for(i = 0; i < pRaidInfo->nDiskCount -1; i++)
			{
				pDataBufferSrc = (PULONG)pRaidInfo->DataBuffers[i];

				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory(
						(PCHAR)pDataBufferSrc + j * BLOCK_SIZE,
						(PCHAR)Ccb->DataBuffer + (i + j * (pRaidInfo->nDiskCount -1)) * BLOCK_SIZE,
						BLOCK_SIZE);
				}
			}

			// generate parity
			// initialize the parity buffer with the first buffer
			RtlCopyMemory(
				pRaidInfo->DataBuffers[pRaidInfo->nDiskCount -1],
				pRaidInfo->DataBuffers[0],
				BLOCK_SIZE * BlocksPerDisk);

			// p' ^= p ^ d;
			for(i = 1; i < pRaidInfo->nDiskCount -1; i++)
			{
				pDataBufferSrc = (PULONG)pRaidInfo->DataBuffers[i];
				pDataBufferParity = (PULONG)pRaidInfo->DataBuffers[pRaidInfo->nDiskCount -1];

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
			for(i = 0; i < pRaidInfo->nDiskCount; i++)
			{
				cdb.DataBuffer[i] = pRaidInfo->DataBuffers[i];
				cdb.DataBufferLength[i] = (UINT32)DataBufferLengthPerDisk;
			}
			
			cdb.DataBufferCount = pRaidInfo->nDiskCount;
			
			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->MapLurnChildren,
				pRaidInfo->nDiskCount,
				Ccb,
				LurnRAID4CcbCompletion,
				&cdb,
				pExtendedCommands,
				FALSE);
		}
		break;
	case SCSIOP_VERIFY:
		{
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_VERIFY\n"));

			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->MapLurnChildren,
				pRaidInfo->nDiskCount,
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
			ULONG i;
			CUSTOM_DATA_BUFFER cdb;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

			DataBufferLengthPerDisk = Ccb->DataBufferLength / (pRaidInfo->nDiskCount -1);
			KDPrintM(DBG_LURN_NOISE,("R Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			for(i = 0; i < pRaidInfo->nDiskCount; i++)
			{
				cdb.DataBuffer[i] = pRaidInfo->DataBuffers[i];
				cdb.DataBufferLength[i] = (UINT32)DataBufferLengthPerDisk;
			}

			cdb.DataBufferCount = pRaidInfo->nDiskCount;
			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->MapLurnChildren,
				pRaidInfo->nDiskCount,
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
			goto complete_here;
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
		goto complete_here;
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
			blockSize *= (Lurn->LurnChildrenCnt - pRaidInfo->nSpareDisk - 1); // exclude parity

			readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
				| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
				| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
				| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;

			KDPrintM(DBG_LURN_TRACE, ("SCSIOP_READ_CAPACITY: %08x : %04x\n", (UINT32)logicalBlockAddress, (UINT32)blockSize));
			goto complete_here;
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
				goto complete_here;
			} else {
				KDPrintM(DBG_LURN_TRACE,
					("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
				LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
				Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
				goto complete_here;
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
complete_here:
	if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_RECOVERING);
	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID4Request(
				 PLURELATION_NODE	Lurn,
				 PCCB				Ccb
				 )
{
	NTSTATUS				status;
	PRAID_INFO pRaidInfo;

	pRaidInfo = Lurn->LurnRAIDInfo;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("IN\n"));

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
	case CCB_OPCODE_NOOP:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				//			LSCcbCompleteCcb(Ccb);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID4CcbCompletion, NULL, NULL, FALSE);
			break;
		}

	case CCB_OPCODE_UPDATE:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				//			LSCcbCompleteCcb(Ccb);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL, TRUE); // use same function as Mirror
			break;
		}

	case CCB_OPCODE_RECOVER:
		{
			// we do nothing for this command anymore.
			// miniport driver(this) itself check recover status
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			//			LSCcbCompleteCcb(Ccb);
			goto complete_here;
		}
		break;

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		//		LSCcbCompleteCcb(Ccb);
		goto complete_here;
		break;
	}

	return STATUS_SUCCESS;

complete_here:
	if(RAID_STATUS_RECOVERRING == pRaidInfo->RaidStatus)
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_RECOVERING);
	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}
