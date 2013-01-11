#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSLurnAssoc"


#define SAFE_FREE_POOL_WITH_TAG(MEM_POOL_PTR, POOL_TAG) \
	if(MEM_POOL_PTR) { \
		ExFreePoolWithTag(MEM_POOL_PTR, POOL_TAG); \
		MEM_POOL_PTR = NULL; \
	}


//////////////////////////////////////////////////////////////////////////
//
//	Associate LURN interfaces
//

//
//	aggregation interface
//
NTSTATUS
AggrLurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	);

NTSTATUS
AggrLurnRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

LURN_INTERFACE LurnAggrInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_AGGREGATION,
					0,
					{
						AggrLurnInitialize,
						LurnDestroyDefault,
						AggrLurnRequest
					}
		 };


//
//	RAID 0 (Spanning) interface
//
NTSTATUS
RAID0LurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
RAID0LurnDestroy(
		PLURELATION_NODE Lurn
	) ;

NTSTATUS
RAID0LurnRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

LURN_INTERFACE LurnRAID0Interface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_RAID0,
					0,
					{
						RAID0LurnInitialize,
						RAID0LurnDestroy,
						RAID0LurnRequest
					}
		 };


//
//	RAID1(mirroring V2) online-recovery interface
//
NTSTATUS
RAID1RLurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
RAID1RLurnRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

NTSTATUS
RAID1RLurnDestroy(
		PLURELATION_NODE Lurn
	) ;

#if 0

LURN_INTERFACE LurnRAID1RInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_RAID1R,
					0,
					{
						RAID1RLurnInitialize,
						RAID1RLurnDestroy,
						RAID1RLurnRequest
					}
		 };

#endif

//////////////////////////////////////////////////////////////////////////
//
//	common to LURN array
//	common to associate LURN
//

//
//  AssociateCascade flag is only handled correctly by UPDATE command only
//
NTSTATUS
LurnAssocSendCcbToChildrenArray(
		IN PLURELATION_NODE			*pLurnChildren,
		IN LONG						ChildrenCnt,
		IN PCCB						Ccb,
		IN CCB_COMPLETION_ROUTINE	CcbCompletion,
		IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
		IN PVOID					*apExtendedCmd, // NULL if no cmd
		IN LURN_CASCADE_OPTION		AssociateCascade
)
{
	LONG		idx_child;
	NTSTATUS	status;
	PCCB		NextCcb[LUR_MAX_LURNS_PER_LUR] = {NULL};
	PCMD_COMMON	pCmdTemp;

	ASSERT(!LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN));

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
			return status;
		}

		status = LSCcbInitializeByCcb(Ccb, pLurnChildren[idx_child], NextCcb[idx_child]);
		if(!NT_SUCCESS(status))
		{
			LONG	idx;

			KDPrintM(DBG_LURN_ERROR, ("LSCcbAllocate failed.\n"));
			
			for(idx = 0; idx <= idx_child; idx++) {
				LSCcbFree(NextCcb[idx]);
			}

			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			return status;
		}
	
		NextCcb[idx_child]->AssociateID = (USHORT)idx_child;
		LSCcbSetFlag(NextCcb[idx_child], CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
		LSCcbSetFlag(NextCcb[idx_child], Ccb->Flags&CCB_FLAG_SYNCHRONOUS);

		LSCcbSetCompletionRoutine(NextCcb[idx_child], CcbCompletion, Ccb);

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

	if(AssociateCascade != LURN_CASCADE_FORWARD)
	{
		Ccb->CascadeEventArray = ExAllocatePoolWithTag(
			NonPagedPool, sizeof(KEVENT) * ChildrenCnt, EVENT_ARRAY_TAG);
		if(!Ccb->CascadeEventArray)
		{
			ASSERT(FALSE);
			for(idx_child = 0; idx_child < ChildrenCnt; idx_child++)
			{
				if(NextCcb[idx_child])
					LSCcbFree(NextCcb[idx_child]);
			}

			status = STATUS_INSUFFICIENT_RESOURCES;
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			return status;
		}

		
		for (idx_child = 0; idx_child < ChildrenCnt; idx_child++) {

			KDPrintM(DBG_LURN_INFO, ("Init event(%p)\n", &Ccb->CascadeEventArray[idx_child]));
			
			KeInitializeEvent( &Ccb->CascadeEventArray[idx_child],
							   SynchronizationEvent, 
							   FALSE );

			NextCcb[idx_child]->CascadeEvent = &Ccb->CascadeEventArray[idx_child];

			if (AssociateCascade == LURN_CASCADE_FORWARD_CHAINING) {

				if (idx_child < ChildrenCnt-1) {
				
					NextCcb[idx_child]->CascadeNextCcb = NextCcb[idx_child+1];

				} else {

					NextCcb[idx_child]->CascadeNextCcb = NULL;
				}

				if (idx_child == 0) {
					
					NextCcb[idx_child]->CascadePrevCcb = NULL;
				
				} else {
					
					NextCcb[idx_child]->CascadePrevCcb = NextCcb[idx_child-1];
				}

			} else {

				if (idx_child < ChildrenCnt-1) {
				
					NextCcb[idx_child]->CascadePrevCcb = NextCcb[idx_child+1];

				} else {
					
					NextCcb[idx_child]->CascadePrevCcb = NULL;
				}

				if (idx_child == 0) {

					NextCcb[idx_child]->CascadeNextCcb = NULL;
				
				} else {
				
					NextCcb[idx_child]->CascadeNextCcb = NextCcb[idx_child-1];
				}
			}

			KDPrintM(DBG_LURN_INFO, ("Cascade #%d (%p).\n", idx_child, NextCcb[idx_child]->CascadeEvent));
		}
		
		Ccb->CascadeEventArrarySize = ChildrenCnt;
		
		// ignition code
//		Ccb->CascadeEventToWork = 0;

		if (AssociateCascade == LURN_CASCADE_FORWARD_CHAINING) {
			
			KeSetEvent(&Ccb->CascadeEventArray[0], IO_NO_INCREMENT, FALSE);
		
		} else {
		
			KeSetEvent(&Ccb->CascadeEventArray[ChildrenCnt-1], IO_NO_INCREMENT, FALSE);
		}
	}

	//
	//	Send CCBs to the child.
	//

	Ccb->AssociateCount = ChildrenCnt;
	Ccb->ChildReqCount = ChildrenCnt; 

	for(idx_child = 0; idx_child < ChildrenCnt; idx_child++) {
		int reqidx;
		if (AssociateCascade == LURN_CASCADE_BACKWARD || 
			AssociateCascade == LURN_CASCADE_BACKWARD_CHAINING) {
			// Send request in reverse order.
			reqidx = ChildrenCnt -1 - idx_child;
		} else {
			reqidx = idx_child;
		}

		status = LurnRequest(pLurnChildren[reqidx], NextCcb[reqidx]);

		if(!NT_SUCCESS(status)) {
			LONG	idx;
			KDPrintM(DBG_LURN_ERROR, ("LurnRequest to Child#%d failed.\n", reqidx));
			for (idx = idx_child; idx < ChildrenCnt; idx++) {
				if (AssociateCascade == LURN_CASCADE_BACKWARD || 
					AssociateCascade == LURN_CASCADE_BACKWARD_CHAINING) {
					// Send request in reverse order.
					reqidx = ChildrenCnt -1 - idx;
				} else {
					reqidx = idx;
				}
				
				LSCcbSetStatus(NextCcb[reqidx], CCB_STATUS_COMMAND_FAILED);
				LSCcbSetNextStackLocation(NextCcb[reqidx]);
				LSCcbCompleteCcb(NextCcb[reqidx]);
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
		IN LURN_CASCADE_OPTION		AssociateCascade
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
	if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
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
			LurnInfo->Length		= sizeof(LURN_INFORMATION);
			LurnInfo->LurnId		= Lurn->LurnId;
			LurnInfo->LurnType		= Lurn->LurnType;
			LurnInfo->UnitBlocks	= Lurn->UnitBlocks;
			LurnInfo->BlockBytes	= Lurn->BlockBytes;
			LurnInfo->AccessRight	= Lurn->AccessRight;
			LurnInfo->StatusFlags	= 0;

			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, CcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);

		}
		break;

	case LurRefreshLurn:
		{
			// only the leaf nodes will process this query
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, CcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);
		}
		break;

	case LurPrimaryLurnInformation:
		{
			KDPrintM(DBG_LURN_TRACE, ("LurPrimaryLurnInformation\n"));
			status = LurnAssocSendCcbToChildrenArray(
				&Lurn->LurnChildren[0],
				1,
				Ccb,
				CcbCompletion,
				NULL,
				NULL,
				LURN_CASCADE_FORWARD
				);
		}
		break;
	default:
		if(Lurn->LurnChildrenCnt > 0) {
			status = LurnRequest(Lurn->LurnChildren[0], Ccb);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_TRACE, ("LurnRequest to Child#0 failed.\n"));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				LSCcbCompleteCcb(Ccb);
			}
		} else {
			status = STATUS_ILLEGAL_FUNCTION;
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
	LSCCB_INITIALIZE(&Ccb);
	Ccb.OperationCode = CCB_OPCODE_QUERY;
	LSCcbSetFlag(&Ccb, CCB_FLAG_SYNCHRONOUS);

	RtlZeroMemory(LurBuffer, sizeof(LurBuffer));
	LurQuery = (PLUR_QUERY)LurBuffer;
	LurQuery->InfoClass = LurRefreshLurn;
	LurQuery->QueryDataLength = 0;

	LurnRefresh = (PLURN_REFRESH)LUR_QUERY_INFORMATION(LurQuery);
	LurnRefresh->Length = sizeof(LURN_REFRESH);

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
//static 
NTSTATUS
LurnExecuteSyncMulti(
				IN ULONG					NrLurns,
				IN PLURELATION_NODE			Lurns[],
				IN UCHAR					CDBOperationCode,
				IN PCHAR					DataBuffer[],
				IN UINT64					BlockAddress,		// Child block addr
				IN UINT16					BlockTransfer,		// Child block count
				IN PCMD_COMMON				ExtendedCommand)
{
	NTSTATUS				status;
	PCCB					Ccb;
	UINT32					DataBufferLength = BlockTransfer * Lurns[0]->BlockBytes; // taken from the first LURN.
	ULONG					i, Waits;
	PKEVENT					CompletionEvents = NULL, *CompletionWaitEvents = NULL;
	PKWAIT_BLOCK			WaitBlockArray = NULL;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	ASSERT(NrLurns < LUR_MAX_LURNS_PER_LUR);

	if ((SCSIOP_WRITE == CDBOperationCode || SCSIOP_WRITE16 == CDBOperationCode)&& 
		!(GENERIC_WRITE & Lurns[0]->AccessRight))
	{
		KDPrintM(DBG_LURN_INFO, ("SKIP(R/O)\n"));
		return STATUS_SUCCESS;
	}

	KDPrintM(DBG_LURN_NOISE, ("NrLurns : %d, Lurn : %08x, DataBuffer : %08x, ExtendedCommand : %08x\n",
		NrLurns, Lurns, DataBuffer, ExtendedCommand));

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
		LSCCB_INITIALIZE(&Ccb[i]);

		Ccb[i].OperationCode = CCB_OPCODE_EXECUTE;
		Ccb[i].DataBuffer = (DataBuffer) ? DataBuffer[i] : NULL;
		Ccb[i].DataBufferLength = DataBufferLength;		

		((PCDB)(Ccb[i].Cdb))->CDB10.OperationCode = CDBOperationCode; // OperationCode is in same place for CDB10 and CDB16
		LSCcbSetLogicalAddress((PCDB)(Ccb[i].Cdb), BlockAddress);
		LSCcbSetTransferLength((PCDB)(Ccb[i].Cdb), BlockTransfer);

		Ccb[i].CompletionEvent = &CompletionEvents[i];
		Ccb[i].pExtendedCommand = (ExtendedCommand) ? &ExtendedCommand[i] : NULL;
		// Set ccb flags
		if((CDBOperationCode == SCSIOP_WRITE ||
			CDBOperationCode == SCSIOP_WRITE16 ||
			(Ccb[i].pExtendedCommand && Ccb[i].pExtendedCommand->Operation == CCB_EXT_WRITE)) &&
			Lurns[i]->Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE){
			LSCcbSetFlag(&Ccb[i], CCB_FLAG_ACQUIRE_BUFLOCK);
		}

		//
		// Do not assume FUA operation for LurnExecuteSyncMulti
		// FUA will reduce write speed seriously, and this function is used by rebuild operation when writing.
		//
		// Currently all metadata write is done through LurnExecuteSyncWrite and it is handled as FUA
		// 
		// To do: handle remove CCB_EXT_WRITE, CCB_EXT_READ and add option for FUA to remove this mess..
		//
#if 0
		if (CDBOperationCode == SCSIOP_WRITE) {
			((PCDB)(Ccb[i].Cdb))->CDB10.ForceUnitAccess = TRUE;
		} else if (CDBOperationCode == SCSIOP_WRITE16) {
			((PCDBEXT)(Ccb[i].Cdb))->CDB16.ForceUnitAccess = TRUE;
		}
#endif

		if(Lurns[i]->Lur->LurFlags & LURFLAG_WRITE_CHECK_REQUIRED) {
			LSCcbSetFlag(&Ccb[i], CCB_FLAG_WRITE_CHECK);
		} else {
			LSCcbResetFlag(&Ccb[i], CCB_FLAG_WRITE_CHECK);
		}

#if 0	// Now LURN IDE detects power-cycled HDD and replaced HDD, so it is safe to allow retried operation.
		if (Lurns[i]->LurnParent &&
			LURN_REDUNDENT_TYPE(Lurns[i]->LurnParent->LurnType)) {
			LSCcbSetFlag(&Ccb[i], CCB_FLAG_RETRY_NOT_ALLOWED);
		}
#endif
		KeInitializeEvent(Ccb[i].CompletionEvent, SynchronizationEvent, FALSE);

		status = LurnRequest(Lurns[i], &Ccb[i]);
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
		else if(!LURN_IS_RUNNING(Lurns[i]->LurnStatus))
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
	SAFE_FREE_POOL_WITH_TAG(CompletionEvents, EXEC_SYNC_POOL_TAG);
	SAFE_FREE_POOL_WITH_TAG(CompletionWaitEvents, EXEC_SYNC_POOL_TAG);
	SAFE_FREE_POOL_WITH_TAG(WaitBlockArray, EXEC_SYNC_POOL_TAG);
	SAFE_FREE_POOL_WITH_TAG(Ccb, EXEC_SYNC_POOL_TAG);

	if(STATUS_SUCCESS != status)
	{
		KDPrintM(DBG_LURN_ERROR, ("Failed : Status %08lx\n", status));
	}

	return status;
}

/* static */
NTSTATUS
LurnExecuteSyncRead(
	IN PLURELATION_NODE	Lurn,
	OUT PUCHAR			DataBuffer,
	IN INT64			LogicalBlockAddress,	// child block address
	IN UINT32			TransferBlocks			// child block count
){
	PCMD_BYTE_OP ExtendedCommand = NULL;

	NTSTATUS status;
	CMD_BYTE_OP ext_cmd;

	RtlZeroMemory(&ext_cmd, sizeof(CMD_BYTE_OP));
	ext_cmd.Operation = CCB_EXT_READ;
	ext_cmd.CountBack = 
		(LogicalBlockAddress < 0) ? TRUE : FALSE;
	ext_cmd.logicalBlockAddress = 
		(UINT64)((LogicalBlockAddress < 0) ? -1 * LogicalBlockAddress : LogicalBlockAddress);
	ext_cmd.ByteOperation = EXT_BLOCK_OPERATION;
	ext_cmd.pByteData = DataBuffer;
	ext_cmd.LengthBlock = (UINT16)TransferBlocks;
	ext_cmd.pLurnCreated = Lurn;

	status = LurnExecuteSyncMulti(
		1, 
		&Lurn,
		SCSIOP_READ, // whatever to execute
		&DataBuffer, // not used
		0,
		0,
		(PCMD_COMMON)&ext_cmd);

	return status;
}

/* static */
NTSTATUS
LurnExecuteSyncWrite(
	IN PLURELATION_NODE	Lurn,
	IN OUT PUCHAR		DataBuffer,
	IN INT64			LogicalBlockAddress,	// child block address
	IN UINT32			TransferBlocks			// child block count
){
	NTSTATUS status;
	CMD_BYTE_OP ext_cmd;

	RtlZeroMemory(&ext_cmd, sizeof(CMD_BYTE_OP));
	ext_cmd.Operation = CCB_EXT_WRITE; // This assume write-through.
	ext_cmd.CountBack = 
		(LogicalBlockAddress < 0) ? TRUE : FALSE;
	ext_cmd.logicalBlockAddress = 
		(UINT64)((LogicalBlockAddress < 0) ? -1 * LogicalBlockAddress : LogicalBlockAddress);
	ext_cmd.ByteOperation = EXT_BLOCK_OPERATION;
	ext_cmd.pByteData = DataBuffer;
	ext_cmd.LengthBlock = (UINT16)TransferBlocks;
	ext_cmd.pLurnCreated = Lurn;

	status = LurnExecuteSyncMulti(
		1, 
		&Lurn,
		SCSIOP_READ, // whatever to execute
		&DataBuffer, // not used
		0,
		0,
		(PCMD_COMMON)&ext_cmd);

	return status;
}

//
// Set additional flag to pass to upper layer.
//
VOID 
LSAssocSetRedundentRaidStatusFlag(
	PLURELATION_NODE Lurn,
	PCCB Ccb
) {
	ULONG Flags = 0;
	UINT32 DraidStatus;
	KIRQL	oldIrql;
	if (Lurn->LurnRAIDInfo) {
		ACQUIRE_SPIN_LOCK(&Lurn->LurnRAIDInfo->SpinLock, &oldIrql);
		// Do not set RAID flag if RAID status is in transition.
		if (Lurn->LurnRAIDInfo->pDraidClient && !Lurn->LurnRAIDInfo->pDraidClient->InTransition) {
			DraidStatus = Lurn->LurnRAIDInfo->pDraidClient->DRaidStatus;
			if (DRIX_RAID_STATUS_REBUILDING == DraidStatus)
			{
				Flags |= CCBSTATUS_FLAG_RAID_RECOVERING;
			}
			else if (DRIX_RAID_STATUS_DEGRADED == DraidStatus)
			{
				Flags |= CCBSTATUS_FLAG_RAID_DEGRADED;
			} 
			else if (DRIX_RAID_STATUS_FAILED == DraidStatus)
			{
				Flags |= CCBSTATUS_FLAG_RAID_FAILURE;
			}
			else if (DRIX_RAID_STATUS_NORMAL== DraidStatus)
			{
				Flags |= CCBSTATUS_FLAG_RAID_NORMAL;
			}
			Flags |= CCBSTATUS_FLAG_RAID_FLAG_VALID;
		}
		RELEASE_SPIN_LOCK(&Lurn->LurnRAIDInfo->SpinLock, oldIrql);
		if (Flags) {
			ACQUIRE_SPIN_LOCK(&Ccb->CcbSpinLock, &oldIrql);
			LSCcbSetStatusFlag(Ccb, Flags);
			RELEASE_SPIN_LOCK(&Ccb->CcbSpinLock, oldIrql);
		}	
	}	
}

//
// If all mode sense reports were successul, summary them and complete ccb..
//
NTSTATUS
LurnAssocModeSenseCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
) {
	KIRQL	oldIrql;
	LONG	RemainingAssocCount;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	//
	// Ignore stopped node and fail at error.
	//
	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:	// prority 0
		break;
	case CCB_STATUS_BUSY:		// prority 1
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;
	case CCB_STATUS_STOP:
		// Ignore stopped node.
		break;
	default:	// prority 2
		if(OriginalCcb->CcbStatus != CCB_STATUS_STOP) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}
	LSCcbSetStatusFlag(	OriginalCcb,
					Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
			);

	if (CCB_STATUS_SUCCESS == Ccb->CcbStatus) {
		//
		// Update original CCB's MODE_SENSE data.
		//
		PMODE_CACHING_PAGE	RootCachingPage;
		PMODE_CACHING_PAGE	ChildCachingPage;
		UINT32	CachingPageOffset;

		CachingPageOffset = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_PARAMETER_BLOCK);
		
		RootCachingPage = (PMODE_CACHING_PAGE)(((PUCHAR)OriginalCcb->DataBuffer) + CachingPageOffset);
		ChildCachingPage = (PMODE_CACHING_PAGE)(((PUCHAR)Ccb->DataBuffer) + CachingPageOffset);
		if (ChildCachingPage->WriteCacheEnable == 0) 
			RootCachingPage->WriteCacheEnable = 0;
		if (ChildCachingPage->ReadDisableCache == 1)
			RootCachingPage->ReadDisableCache = 1;	
	}
	ExFreePoolWithTag(Ccb->DataBuffer, CCB_CUSTOM_BUFFER_POOL_TAG);

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//
	//	Complete the original CCB if this is last Ccb
	//

	RemainingAssocCount = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(RemainingAssocCount >= 0);
	if(RemainingAssocCount != 0) {
		return STATUS_SUCCESS;
	}

	LSAssocSetRedundentRaidStatusFlag(OriginalCcb->CcbCurrentStackLocation->Lurn, OriginalCcb);
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}


//
// Get minimum specification among all children.
//
NTSTATUS
LurnAssocModeSense(
	IN PLURELATION_NODE Lurn,
	IN PCCB Ccb
) {
	CUSTOM_DATA_BUFFER customBuffer;
	ULONG i, j;
	NTSTATUS status;
	PCDB	Cdb;
	PMODE_PARAMETER_HEADER	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
	PMODE_PARAMETER_BLOCK	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)Ccb->DataBuffer + sizeof(MODE_PARAMETER_HEADER));
	ULONG	requiredLen;
	ULONG	BlockCount;

	//
	// Check Ccb sanity check and set default sense data.
	//		
	//
	// Buffer size should larger than MODE_PARAMETER_HEADER
	//
	requiredLen = sizeof(MODE_PARAMETER_HEADER);
	if(Ccb->DataBufferLength < requiredLen) {
		KDPrintM(DBG_LURN_ERROR, ("Buffer too small. %d.\n", Ccb->DataBufferLength));
		
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	}
	
	RtlZeroMemory(
		Ccb->DataBuffer,
		Ccb->DataBufferLength
		);
	Cdb = (PCDB)Ccb->Cdb;

	//
	// We only report current values.
	//

	if(Cdb->MODE_SENSE.Pc != (MODE_SENSE_CURRENT_VALUES>>6)) {
		KDPrintM(DBG_LURN_ERROR,
			("unsupported page control:%x\n", (ULONG)Cdb->MODE_SENSE.Pc));
		LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	} else {
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
	}

	//
	// Mode parameter header.
	//
	parameterHeader->ModeDataLength = 
		sizeof(MODE_PARAMETER_HEADER) - sizeof(parameterHeader->ModeDataLength);
	parameterHeader->MediumType = 00;	// Default medium type.

	// Fill device specific parameter
	if(!(Lurn->AccessRight & GENERIC_WRITE)) {
		KDPrintM(DBG_LURN_INFO,
		("SCSIOP_MODE_SENSE: MODE_DSP_WRITE_PROTECT\n"));
		parameterHeader->DeviceSpecificParameter |= MODE_DSP_WRITE_PROTECT;

		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_W2K_READONLY_PATCH) ||
			LSCcbIsFlagOn(Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS))
			parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	} else {
		parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	}

	//
	// Mode parameter block
	//
	requiredLen += sizeof(MODE_PARAMETER_BLOCK);
	if(Ccb->DataBufferLength < requiredLen) {
		Ccb->CcbStatus = CCB_STATUS_DATA_OVERRUN;
		KDPrintM(DBG_LURN_ERROR, ("Could not fill out parameter block. %d.\n", Ccb->DataBufferLength));
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	}
	
	// Set the length of mode parameter block descriptor to the parameter header.
	parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);
	parameterHeader->ModeDataLength += sizeof(MODE_PARAMETER_BLOCK);

	//
	// Make Block.
	//
	BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
	parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
	parameterBlock->NumberOfBlocks[0] = (BYTE)(BlockCount>>16);
	parameterBlock->NumberOfBlocks[1] = (BYTE)(BlockCount>>8);
	parameterBlock->NumberOfBlocks[2] = (BYTE)(BlockCount);

	if(Cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL ||
		Cdb->MODE_SENSE.PageCode == MODE_PAGE_CACHING) {	// all pages
		PMODE_CACHING_PAGE	cachingPage;

		requiredLen += sizeof(MODE_CACHING_PAGE);
		if(Ccb->DataBufferLength < requiredLen) {
			Ccb->CcbStatus = CCB_STATUS_DATA_OVERRUN;
			KDPrintM(DBG_LURN_ERROR, ("Could not fill out caching page. %d.\n", Ccb->DataBufferLength));
			LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
			LSCcbCompleteCcb(Ccb);
			return STATUS_SUCCESS;
		}

		parameterHeader->ModeDataLength += sizeof(MODE_CACHING_PAGE);
		cachingPage = (PMODE_CACHING_PAGE)((PUCHAR)parameterBlock + sizeof(MODE_PARAMETER_BLOCK));

		cachingPage->PageCode = MODE_PAGE_CACHING;
		cachingPage->PageLength = sizeof(MODE_CACHING_PAGE) -
			(FIELD_OFFSET(MODE_CACHING_PAGE, PageLength) + sizeof(cachingPage->PageLength));
		// Set default value.
		cachingPage->WriteCacheEnable = 1;
		cachingPage->ReadDisableCache = 0;
	}	else {
		KDPrintM(DBG_LURN_TRACE,
					("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
		LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	}
	
	Ccb->ResidualDataLength = Ccb->DataBufferLength - requiredLen;

	// Prepare buffer for each child's 
	customBuffer.DataBufferCount = 0;
	for(i = 0; i < Lurn->LurnChildrenCnt; i++) {
		customBuffer.DataBuffer[i] = ExAllocatePoolWithTag(
						NonPagedPool,
						Ccb->DataBufferLength,
						CCB_CUSTOM_BUFFER_POOL_TAG
					);
		if(!customBuffer.DataBuffer[i])
		{
			// free allocated buffers
			for(j = 0; j < i; j++)
			{
				ExFreePoolWithTag(
					customBuffer.DataBuffer[j], CCB_CUSTOM_BUFFER_POOL_TAG
					);
				customBuffer.DataBuffer[i] = NULL;
			}

			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
			status = STATUS_INSUFFICIENT_RESOURCES;
			KDPrintM(DBG_LURN_ERROR, ("ExAllocatePoolWithTag Failed[%d]\n", i));
			LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
			LSCcbCompleteCcb(Ccb);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		customBuffer.DataBufferLength[i] = Ccb->DataBufferLength;
		customBuffer.DataBufferCount++;
	}
	//
	// Send request to all childrens.
	//
	status = LurnAssocSendCcbToAllChildren(
											Lurn,
											Ccb,
											LurnAssocModeSenseCompletion,
											&customBuffer,
											NULL,
											LURN_CASCADE_FORWARD
							);

	if(!NT_SUCCESS(status))
	{
		for(i = 0; i < Lurn->LurnChildrenCnt; i++)
		{
			ExFreePoolWithTag(
				customBuffer.DataBuffer[i], CCB_CUSTOM_BUFFER_POOL_TAG);
		}
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);		
		return status;
	}
	return STATUS_SUCCESS;
}

NTSTATUS
LurnAssocModeSelectCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
) {
	KIRQL	oldIrql;
	LONG	RemainingAssocCount;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	//
	// Ignore stopped node and fail at error.
	//
	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:	// prority 0
		break;
	case CCB_STATUS_BUSY:		// prority 1
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;
	case CCB_STATUS_STOP:
		break;
	default:	// prority 2
		if(OriginalCcb->CcbStatus != CCB_STATUS_STOP) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}
	LSCcbSetStatusFlag(	OriginalCcb,
					Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
			);

	ExFreePoolWithTag(Ccb->DataBuffer, CCB_CUSTOM_BUFFER_POOL_TAG);

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//
	//	Complete the original CCB if this is last Ccb
	//

	RemainingAssocCount = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(RemainingAssocCount >= 0);
	if(RemainingAssocCount != 0) {
		return STATUS_SUCCESS;
	}
	LSAssocSetRedundentRaidStatusFlag(OriginalCcb->CcbCurrentStackLocation->Lurn, OriginalCcb);
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}
	
NTSTATUS 
LurnAssocModeSelect(
	IN PLURELATION_NODE Lurn,
	IN PCCB Ccb
) {
	CUSTOM_DATA_BUFFER customBuffer;
	ULONG i, j;
	NTSTATUS status;
	PCDB	Cdb;
	PMODE_PARAMETER_HEADER	parameterHeader;
	PMODE_PARAMETER_BLOCK	parameterBlock;
	ULONG	requiredLen;
	UCHAR	parameterLength;
	PUCHAR	modePages;
	
	// Check buffer is enough
	Cdb = (PCDB)Ccb->Cdb;
	parameterHeader = (PMODE_PARAMETER_HEADER)Ccb->DataBuffer;
	parameterBlock =  (PMODE_PARAMETER_BLOCK)((PUCHAR)parameterHeader + sizeof(MODE_PARAMETER_HEADER));
	parameterLength = Cdb->MODE_SELECT.ParameterListLength;

	//
	// Buffer size should larger than MODE_PARAMETER_HEADER
	//

	requiredLen = sizeof(MODE_PARAMETER_HEADER);
	if(Ccb->DataBufferLength < requiredLen ||
		parameterLength < requiredLen) {
		KDPrintM(DBG_LURN_ERROR, ("Buffer too small. %d.\n", Ccb->DataBufferLength));
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);		
		return STATUS_SUCCESS;
	}

	requiredLen += sizeof(MODE_PARAMETER_BLOCK);
	if(Ccb->DataBufferLength < requiredLen ||parameterLength < requiredLen) {
		KDPrintM(DBG_LURN_ERROR, ("Buffer too small. %d.\n", Ccb->DataBufferLength));
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);		
		return STATUS_SUCCESS;
	}

	//
	// We only handle mode pages and volatile settings.
	//

	if(Cdb->MODE_SELECT.PFBit == 0) {
		KDPrintM(DBG_LURN_ERROR,
			("unsupported page format:%x\n", (ULONG)Cdb->MODE_SELECT.PFBit));
		LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	} else if(Cdb->MODE_SELECT.SPBit != 0)	{
		KDPrintM(DBG_LURN_ERROR,
			("unsupported save page to non-volitile memory:%x.\n", (ULONG)Cdb->MODE_SELECT.SPBit));
		LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	} else {
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
	}

	//
	// Get the mode pages
	//

	modePages = (PUCHAR)parameterBlock + sizeof(MODE_PARAMETER_BLOCK);

	//
	// Caching mode page
	//

	if(	(*modePages & 0x3f) == MODE_PAGE_CACHING) {	// all pages
		KDPrintM(DBG_LURN_ERROR, ("Caching page\n"));

		requiredLen += sizeof(MODE_CACHING_PAGE);
		if(Ccb->DataBufferLength < requiredLen ||parameterLength < requiredLen) {
				KDPrintM(DBG_LURN_ERROR, ("Buffer too small. %d.\n", Ccb->DataBufferLength));
				Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
				LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
				LSCcbCompleteCcb(Ccb);
				return STATUS_SUCCESS;
		}
		//
		// Send to all children.
		//
		// Prepare buffer for each child's 
		customBuffer.DataBufferCount = 0;
		for(i = 0; i < Lurn->LurnChildrenCnt; i++) {
			customBuffer.DataBuffer[i] = ExAllocatePoolWithTag(
							NonPagedPool,
							Ccb->DataBufferLength,
							CCB_CUSTOM_BUFFER_POOL_TAG
						);
			if(!customBuffer.DataBuffer[i])
			{
				// free allocated buffers
				for(j = 0; j < i; j++)
				{
					ExFreePoolWithTag(
						customBuffer.DataBuffer[j], CCB_CUSTOM_BUFFER_POOL_TAG
						);
					customBuffer.DataBuffer[i] = NULL;
				}

				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				status = STATUS_INSUFFICIENT_RESOURCES;
				KDPrintM(DBG_LURN_ERROR, ("ExAllocatePoolWithTag Failed[%d]\n", i));
				LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
				LSCcbCompleteCcb(Ccb);
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			RtlCopyMemory(customBuffer.DataBuffer[i], Ccb->DataBuffer, Ccb->DataBufferLength);
			customBuffer.DataBufferLength[i] = Ccb->DataBufferLength;
			customBuffer.DataBufferCount++;
		}
		//
		// Send request to all childrens.
		//
		status = LurnAssocSendCcbToAllChildren(
												Lurn,
												Ccb,
												LurnAssocModeSelectCompletion,
												&customBuffer,
												NULL,
												LURN_CASCADE_FORWARD
								);

		if(!NT_SUCCESS(status))
		{
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				ExFreePoolWithTag(
					customBuffer.DataBuffer[i], CCB_CUSTOM_BUFFER_POOL_TAG);
			}
			LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
			LSCcbCompleteCcb(Ccb);		
			return status;
		}	
	} else {
		KDPrintM(DBG_LURN_ERROR,
			("unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
		LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;	//SRB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	}
	return STATUS_SUCCESS;
}


NTSTATUS
LurnRAIDUpdateCcbCompletion(
	IN PCCB Ccb,
	IN PCCB OriginalCcb)
{
	KIRQL	oldIrql;
	LONG	ass;
	LONG	i;
	BOOLEAN FailAll = FALSE;
		
	PLURELATION_NODE	Lurn;
	Lurn = OriginalCcb->CcbCurrentStackLocation->Lurn;
	ASSERT(Lurn);

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	ASSERT(
		OriginalCcb->CascadeEventArray && 
		Ccb->CascadeEvent && 
		Ccb->CascadeEvent == &OriginalCcb->CascadeEventArray[Ccb->AssociateID]);

	ASSERT(OriginalCcb->OperationCode == CCB_OPCODE_UPDATE);

	KDPrintM(DBG_LURN_INFO, ("Ccb update status %x\n", Ccb->CcbStatus));

	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:	// prority 0
		break;
	case CCB_STATUS_NO_ACCESS: // priority 2 for RAID1,4 , 1 for RAID0, aggegation
		if (OriginalCcb->CcbStatus == CCB_STATUS_BUSY) {
			KDPrintM(DBG_LURN_INFO, ("Previous node is reconnecting. Retry later\n"));
		} else if (OriginalCcb->CcbStatus == CCB_STATUS_STOP) {
			if(LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType) {
				KDPrintM(DBG_LURN_INFO, ("Failed to get access right. Fail next ccb too\n"));
				OriginalCcb->CcbStatus = CCB_STATUS_NO_ACCESS;
				FailAll = TRUE; // Make another cascading CCB to fail.				
			} else {
				// Any node failure is fail. Keep stop status.
				KDPrintM(DBG_LURN_INFO, ("Node is stopped\n"));
			}
		} else {
			KDPrintM(DBG_LURN_INFO, ("Failed to get access right. Fail next ccb too\n"));
			OriginalCcb->CcbStatus = CCB_STATUS_NO_ACCESS;
			FailAll = TRUE; // Make another cascading CCB to fail.
		}
		break;
	case CCB_STATUS_STOP:		// prority 3

		if(OriginalCcb->CcbStatus != CCB_STATUS_BUSY) {
			if(LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType)
			{
				// do not stop for fault-tolerant RAID
			}
			else
			{
				OriginalCcb->CcbStatus = CCB_STATUS_STOP;
			}
		}
		break;

	case CCB_STATUS_BUSY:		// prority 4
		//
		//	We allow CCB_STATUS_BUSY when SRB exists.
		//
		ASSERT(OriginalCcb->Srb);
		OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		break;
	default:					// prority 2
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}

	if (Ccb->CascadePrevCcb) {
		KDPrintM(DBG_LURN_INFO, ("Handling out-of-sync completed ccb(prev=%p)\n", Ccb->CascadePrevCcb));
		//
		// If CascadePrevCcb is not NULL, this ccb is completed in out-of-order. Don't call next ccb
		//
		// Rearrange ccb cascade without this link.
		Ccb->CascadePrevCcb->CascadeNextCcb = Ccb->CascadeNextCcb;
		if (Ccb->CascadeNextCcb) {
			Ccb->CascadeNextCcb->CascadePrevCcb = Ccb->CascadePrevCcb;
		}
	} else if (Ccb->CascadeNextCcb) {
		KDPrintM(DBG_LURN_INFO, ("Set event for next ccb(%p)\n", Ccb->CascadeNextCcb));
		if (FailAll) {
			Ccb->CascadeNextCcb->FailCascadedCcbCode = Ccb->CcbStatus;
		}
		Ccb->CascadeNextCcb->CascadePrevCcb = NULL;
		KeSetEvent(Ccb->CascadeNextCcb->CascadeEvent, IO_NO_INCREMENT, FALSE);
	}

	// set own cascade event (even if already set) as a completion mark
	KeSetEvent(Ccb->CascadeEvent, IO_NO_INCREMENT, FALSE);

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

	ExFreePoolWithTag(OriginalCcb->CascadeEventArray, EVENT_ARRAY_TAG);
	OriginalCcb->CascadeEventArray = NULL;

	//	post-process
	// set event to work as primary
	switch(Lurn->LurnType)
	{
	case LURN_RAID1R:
	case LURN_RAID4R:
	{
		//
		// Success if two CCBs are both successful or one success, another not exist.
		//
		if (OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS ||
			OriginalCcb->CcbStatus == CCB_STATUS_NOT_EXIST)
		{
			PLURN_UPDATE	LurnUpdate;
			LurnUpdate = (PLURN_UPDATE)OriginalCcb->DataBuffer;

			if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID)
			{
				KDPrintM(DBG_LURN_ERROR,("********** Lurn->LurnType(%d) : R/O ->R/W : Start to initialize **********\n",
					Lurn->LurnType));

				if(Lurn->LurnParent == 0 || Lurn->LurnParent == Lurn)
				{
					Lurn->Lur->EnabledNdasFeatures &= ~NDASFEATURE_SECONDARY;
					Lurn->AccessRight |= GENERIC_WRITE;
					KDPrintM(DBG_LURN_INFO,("Updated enabled feature: %08lx\n",Lurn->Lur->EnabledNdasFeatures));
					KDPrintM(DBG_LURN_INFO,("Starting DRAID arbiter from updated permission\n"));

					//
					// Update access write of stopped child too
					//
					for(i=0;i<(LONG)Lurn->LurnChildrenCnt;i++) {
						if (!(Lurn->LurnChildren[i]->AccessRight & GENERIC_WRITE)) {
							if (Lurn->LurnChildren[i]->LurnStatus == LURN_STATUS_RUNNING) {
								KDPrintM(DBG_LURN_INFO,("Node %d is running status.It should be stop or failure status\n", i));
								ASSERT(FALSE);
								// This should not happen. If it happens go to revert path.
								goto update_failed;
							}
							KDPrintM(DBG_LURN_INFO,("Node %d is not updated to have write access. Giving write access for revival.\n", i));
							
							Lurn->LurnChildren[i]->AccessRight |= GENERIC_WRITE;
							if (Lurn->LurnChildren[i]->SavedLurnDesc) {
								Lurn->LurnChildren[i]->SavedLurnDesc->AccessRight |= GENERIC_WRITE;
							}
						}
					}
					
					//
					// Cannot call DraidArbiterStart directly because we are on the completion routine.
					// Let DRAID client call DraidArbiterStart
					//
					// Local client is polling local arbiter if this host is primary.
				}
			} else {
				// Other case does not happen.
				ASSERT(FALSE);
			}

			OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;
		}else {
update_failed:
			//
			// If failed to update whole available node, revert access right.
			//
			// to do...
			//
			KDPrintM(DBG_LURN_INFO,("Failed to update access right of all nodes. To do: demote rights if already updated.\n"));
		}
	}
	break;
	case LURN_AGGREGATION:
	case LURN_RAID0:
	{
		// Success only when all Ccb success
		if (OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS)
		{
			PLURN_UPDATE	LurnUpdate;
			LurnUpdate = (PLURN_UPDATE)OriginalCcb->DataBuffer;

			if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID)
			{
				KDPrintM(DBG_LURN_ERROR,("********** Lurn->LurnType(%d) : R/O ->R/W : Start to initialize **********\n",
					Lurn->LurnType));

				if(Lurn->LurnParent == 0 || Lurn->LurnParent == Lurn)
				{
					Lurn->Lur->EnabledNdasFeatures &= ~NDASFEATURE_SECONDARY;
					Lurn->AccessRight |= GENERIC_WRITE;
					KDPrintM(DBG_LURN_INFO,("Updated enabled feature: %08lx\n",Lurn->Lur->EnabledNdasFeatures));
				}
			}
		} else {
			KDPrintM(DBG_LURN_ERROR,("Failed to update access right: ccbstatus = %x\n", 
				OriginalCcb->CcbStatus));
		}
	}
	break;
	default:
	ASSERT(FALSE);
	}

#if DBG
	if(OriginalCcb->OperationCode == CCB_OPCODE_UPDATE && OriginalCcb->CcbStatus == CCB_STATUS_BUSY) {
		KDPrintM(DBG_LURN_INFO,("CCB_OPCODE_UPDATE: return CCB_STATUS_BUSY\n"));
	}
#endif

	LSAssocSetRedundentRaidStatusFlag(Lurn, OriginalCcb);
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
//	Aggregation Lurn
//

//
// Get the children's block bytes.
//

ULONG
LurnAsscGetChildBlockBytes(
	PLURELATION_NODE ParentLurn
){
	ULONG	idx_child;
	ULONG	childBlockBytes;

	if(ParentLurn->LurnChildrenCnt == 0) {
		KDPrintM(DBG_LURN_ERROR,("A child node does not exist.\n"));
		return 0;
	}
	childBlockBytes = ParentLurn->LurnChildren[0]->BlockBytes;
	if(childBlockBytes == 0) {
		KDPrintM(DBG_LURN_ERROR,("First child does not have the same block bytes\n"));
		return 0;
	}

	//
	// Verify the block bytes of the other children.
	//
	// This test cannot check the validity of aggregated disk
	//
	for(idx_child = 1; idx_child < ParentLurn->LurnChildrenCnt; idx_child ++) {

		if(childBlockBytes != ParentLurn->LurnChildren[idx_child]->BlockBytes) {
			KDPrintM(DBG_LURN_ERROR,("Children do not have the same block bytes\n"));
			return 0;
		}
	}

	return childBlockBytes;
}

NTSTATUS
AggrLurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
){
	UNREFERENCED_PARAMETER(LurnDesc);

	//
	// Determine block bytes
	//

	Lurn->ChildBlockBytes = LurnAsscGetChildBlockBytes(Lurn);
	if(Lurn->ChildBlockBytes == 0)
		return STATUS_DEVICE_NOT_READY;

	Lurn->BlockBytes = Lurn->ChildBlockBytes;

	return STATUS_SUCCESS;
}


NTSTATUS
LurnAggrCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;

	KDPrintM(DBG_LURN_TRACE,("LurnAggrCcbCompletion\n"));
	
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
					Lurn->Lur->EnabledNdasFeatures &= ~NDASFEATURE_SECONDARY;
					KDPrintM(DBG_LURN_INFO,("Updated enabled feature: %08lx\n",Lurn->Lur->EnabledNdasFeatures));
				}
			}
	}

	if (CCB_OPCODE_QUERY  == OriginalCcb->OperationCode) {
		PLUR_QUERY			query;
		PLURELATION_NODE LurnOriginal;
		LurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
		
		query = (PLUR_QUERY)Ccb->DataBuffer;
		if (LurPrimaryLurnInformation == query->InfoClass && 
			LURN_REDUNDENT_TYPE(LurnOriginal->LurnType))  {
			//
			// Set RAID set ID as primary ID.
			//
			PLURN_PRIMARYINFORMATION	ReturnInfo;
			PLURN_INFORMATION			LurnInfo;

			KDPrintM(DBG_LURN_INFO,("Setting RAID set ID as LurPrimaryLurnInformation's primary id\n"));
			ReturnInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION(query);
			LurnInfo = &ReturnInfo->PrimaryLurn;
			RtlCopyMemory(LurnInfo->PrimaryId, &LurnOriginal->LurnRAIDInfo->RaidSetId, sizeof(LurnInfo->PrimaryId));
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
	PLURELATION_NODE	ChildLurn = NULL;

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE16:
	case SCSIOP_WRITE:
	case 0x3E:		// READ_LONG
	case SCSIOP_READ16:
	case SCSIOP_READ:
	case SCSIOP_VERIFY16:
	case SCSIOP_VERIFY:  {
		UINT64				startBlockAddress, endBlockAddress;
		UINT64				childStartBlockAddress;
		ULONG				transferBlocks;
		ASSERT(Ccb->CdbLength <= MAXIMUM_CDB_SIZE);

		LSCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &startBlockAddress, &transferBlocks);

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
		childStartBlockAddress = 0;
		for(idx_child = 0; idx_child < Lurn->LurnChildrenCnt; idx_child ++) {
			ChildLurn = Lurn->LurnChildren[idx_child];

			ASSERT(ChildLurn->StartBlockAddr == 0);
			if( startBlockAddress >= childStartBlockAddress &&
				startBlockAddress < childStartBlockAddress + ChildLurn->UnitBlocks) {
				break;
			}

			childStartBlockAddress += ChildLurn->UnitBlocks;
		}
		if(ChildLurn == NULL || idx_child >= Lurn->LurnChildrenCnt) {
			KDPrintM(DBG_LURN_ERROR,("SCSIOP_WRITE/READ/VERIFY: Could not found child LURN. Ccb's ending sector:%ld\n", startBlockAddress));
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			status = STATUS_SUCCESS;
			break;
		}

		//
		//	determine if need to split the CCB.
		//
		if(endBlockAddress < childStartBlockAddress + ChildLurn->UnitBlocks) {
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
			LSCcbInitializeByCcb(Ccb, Lurn->LurnChildren[idx_child], NextCcb);
			LSCcbSetFlag(NextCcb, CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
			LSCcbSetFlag(NextCcb, Ccb->Flags&CCB_FLAG_SYNCHRONOUS);
			NextCcb->AssociateID = (USHORT)idx_child;
			LSCcbSetCompletionRoutine(NextCcb, LurnAggrCcbCompletion, Ccb);

			// start address
			startBlockAddress -= childStartBlockAddress;
			pCdb = (PCDB)&NextCcb->Cdb[0];

			LSCcbSetLogicalAddress(pCdb, (UINT32)startBlockAddress);

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
			ULONG		firstTransferBlocks;
			ULONG		secondTransferBlocks;
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
				LSCcbInitializeByCcb(Ccb, Lurn->LurnChildren[idx_ccb], NextCcb[idx_ccb]);
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

			// start address in the child address space
			firstStartBlockAddress = startBlockAddress - childStartBlockAddress;

			LSCcbSetLogicalAddress(pCdb, firstStartBlockAddress);

			// transfer length
			firstTransferBlocks = (USHORT)(ChildLurn->UnitBlocks - firstStartBlockAddress);

			LSCcbSetTransferLength(pCdb, firstTransferBlocks);

			NextCcb[0]->DataBufferLength = firstTransferBlocks * Lurn->ChildBlockBytes;
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

			LSCcbSetLogicalAddress(pCdb, BlockAddress_0);
			
			// transfer length
			secondTransferBlocks = transferBlocks - firstTransferBlocks;
			ASSERT(secondTransferBlocks > 0);

			LSCcbSetTransferLength(pCdb, secondTransferBlocks);

			NextCcb[1]->DataBufferLength = secondTransferBlocks * Lurn->ChildBlockBytes;
			NextCcb[1]->DataBuffer = ((PUCHAR)Ccb->DataBuffer) + (firstTransferBlocks * Lurn->ChildBlockBytes);	// offset 18
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
		if(logicalBlockAddress < 0xffffffff) {
			REVERSE_BYTES(&readCapacityData->LogicalBlockAddress, &logicalBlockAddress);
		} else {
			readCapacityData->LogicalBlockAddress = 0xffffffff;
		}

		blockSize = Lurn->BlockBytes;
		REVERSE_BYTES(&readCapacityData->BytesPerBlock, &blockSize);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_READ_CAPACITY16:
	{
		PREAD_CAPACITY_DATA_EX		readCapacityDataEx;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityDataEx = (PREAD_CAPACITY_DATA_EX)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		REVERSE_BYTES_QUAD(&readCapacityDataEx->LogicalBlockAddress.QuadPart, &logicalBlockAddress);

		blockSize = Lurn->BlockBytes;
		REVERSE_BYTES(&readCapacityDataEx->BytesPerBlock, &blockSize);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_MODE_SENSE:
	{
		LurnAssocModeSense(Lurn, Ccb);
		break;
	}
	case SCSIOP_MODE_SELECT:
	{
		LurnAssocModeSelect(Lurn, Ccb);
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
												LURN_CASCADE_FORWARD
								);
		break;
		}
	}

	return STATUS_SUCCESS;
}



NTSTATUS
AggrLurnRequest(
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
	case CCB_OPCODE_FLUSH:
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_FLUSH!\n"));
		// Nothing to do for aggregated disks. This flush is sent to controller side cache.		
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	case CCB_OPCODE_SHUTDOWN:
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_SHUTDOWN!\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;

	//
	//	Send to all LURNs
	//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_STOP:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_NOOP:
		{

			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnAggrCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);
			break;
		}

	case CCB_OPCODE_UPDATE:
		{

			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAIDUpdateCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD_CHAINING);
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


#if 0
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
			LSCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_RAID_DEGRADED);
		} else {
			//
			//	Two children stopped!
			//
			OriginalCcb->CcbStatus = CCB_STATUS_STOP;	// priority 5
		}
		break;
	case CCB_STATUS_NOT_EXIST:
		if(LSCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_RAID_DEGRADED)) {
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
#endif
//////////////////////////////////////////////////////////////////////////
//
//	RAID0 Lurn
//

NTSTATUS
RAID0LurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_INFO pRaidInfo = NULL;
	ULONG ulDataBufferSize, ulDataBufferSizePerDisk;
	NTSTATUS ntStatus;

	UNREFERENCED_PARAMETER(LurnDesc);

	//
	// Determine block bytes.
	//

	Lurn->ChildBlockBytes = LurnAsscGetChildBlockBytes(Lurn);
	if(Lurn->ChildBlockBytes == 0)
		return STATUS_DEVICE_NOT_READY;
	Lurn->BlockBytes = Lurn->ChildBlockBytes * Lurn->LurnChildrenCnt;

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

	KeInitializeSpinLock(&pRaidInfo->SpinLock);

//	LurnSetRaidInfoStatus(pRaidInfo, RAID_STATUS_NORMAL);

//	Data buffer shuffled
	pRaidInfo->MaxDataSendLength = LurnDesc->MaxDataSendLength;
	pRaidInfo->MaxDataRecvLength = LurnDesc->MaxDataRecvLength;

	ASSERT(0 == (pRaidInfo->MaxDataSendLength/Lurn->ChildBlockBytes % Lurn->LurnChildrenCnt));
	ASSERT(0 == (pRaidInfo->MaxDataRecvLength/Lurn->ChildBlockBytes % Lurn->LurnChildrenCnt));

	// service sets max blocks per request as full devices size of 1 I/O
	// ex) 64KB per unit device and 2 devices -> MBR = 128KB;
	if(pRaidInfo->MaxDataSendLength > pRaidInfo->MaxDataRecvLength) {
		ulDataBufferSizePerDisk = (pRaidInfo->MaxDataSendLength / Lurn->LurnChildrenCnt);
	} else {
		ulDataBufferSizePerDisk = (pRaidInfo->MaxDataRecvLength / Lurn->LurnChildrenCnt);
	}

	ulDataBufferSize = ulDataBufferSizePerDisk * Lurn->LurnChildrenCnt;

	ASSERT(sizeof(pRaidInfo->DataBufferLookaside) >= sizeof(NPAGED_LOOKASIDE_LIST));
	ExInitializeNPagedLookasideList(
		&pRaidInfo->DataBufferLookaside,
		NULL, // PALLOCATE_FUNCTION  Allocate  OPTIONAL
		NULL, // PFREE_FUNCTION  Free  OPTIONAL
		0, // Flags Reserved. Must be zero
		ulDataBufferSizePerDisk,
		RAID_DATA_BUFFER_POOL_TAG,
		0 // Depth Reserved. Must be zero
		);

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAIDInfo)
		{
			SAFE_FREE_POOL_WITH_TAG(Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG);
		}
	}


	return ntStatus;
}

NTSTATUS
RAID0LurnDestroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAIDInfo);

	pRaidInfo = Lurn->LurnRAIDInfo;

	ExDeleteNPagedLookasideList(
		&pRaidInfo->DataBufferLookaside);

	ASSERT(pRaidInfo);
	SAFE_FREE_POOL_WITH_TAG(pRaidInfo, RAID_INFO_POOL_TAG);

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

	//
	//	Higher number of priority will overwrite CcbStatus.
	//
	//	
	//	RAID0 (striping) priority
	//
	//	CCB_STATUS_SUCCESS	: 0
	//	CCB_STATUS_BUSY		: 1
	//	Other error code	: 2
	//	CCB_STATUS_STOP		: 3
	switch(Ccb->CcbStatus) {

	case CCB_STATUS_SUCCESS:
		if (CCB_OPCODE_EXECUTE == OriginalCcb->OperationCode)
		{
			switch(OriginalCcb->Cdb[0])
			{
			case 0x3E:		// READ_LONG
			case SCSIOP_READ:
			case SCSIOP_READ16:				
				{
					ULONG i, j, BlocksPerDisk;

					KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));
					ASSERT(Ccb->DataBuffer);

					BlocksPerDisk = Ccb->DataBufferLength / pLurnOriginal->ChildBlockBytes;
					i = Ccb->AssociateID;
					for(j = 0; j < BlocksPerDisk; j++)
					{
						RtlCopyMemory( // Copy back
							(PCHAR)OriginalCcb->DataBuffer + (i + j * (pLurnOriginal->LurnChildrenCnt)) * pLurnOriginal->ChildBlockBytes,
							(PCHAR)Ccb->DataBuffer + (j * pLurnOriginal->ChildBlockBytes),
							pLurnOriginal->ChildBlockBytes);
					}
				}
				break;
			}
		}
		break;

	case CCB_STATUS_BUSY:
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
//			ASSERT(OriginalCcb->Srb);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
		break;
	default:
		KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d\n",
								(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID));

		OriginalCcb->CcbStatus = Ccb->CcbStatus;
		break;
	}

	if(CCB_OPCODE_EXECUTE == OriginalCcb->OperationCode)
	{
		switch(OriginalCcb->Cdb[0])
		{
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:			
		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:
			KDPrintM(DBG_LURN_NOISE,("Release data buffer look aside (%p)\n", Ccb->DataBuffer));
			ASSERT(Ccb->DataBuffer);
			ExFreeToNPagedLookasideList(
				&pRaidInfo->DataBufferLookaside,
				Ccb->DataBuffer);

			Ccb->DataBuffer = NULL;
		}
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
					Lurn->Lur->EnabledNdasFeatures &= ~NDASFEATURE_SECONDARY;
					Lurn->AccessRight |= GENERIC_WRITE;
					KDPrintM(DBG_LURN_INFO,("Updated enabled feature: %08lx\n",Lurn->Lur->EnabledNdasFeatures));
				}
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
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PRAID_INFO			pRaidInfo;

	pRaidInfo = Lurn->LurnRAIDInfo;

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:		
		{
			UINT64 logicalBlockAddress;
			UINT32 transferBlocks;

			int DataBufferLengthPerDisk;
			ULONG BlocksPerDisk;
			register ULONG i, j;
			CUSTOM_DATA_BUFFER cdb;

			LSCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &logicalBlockAddress, &transferBlocks);

			ASSERT(transferBlocks <= pRaidInfo->MaxDataSendLength/Lurn->BlockBytes);

			KDPrintM(DBG_LURN_NOISE,("RAID0 Write %I64x:%x, Buffer length %x\n", logicalBlockAddress, transferBlocks, Ccb->DataBufferLength));
			
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));
			
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			DataBufferLengthPerDisk = Ccb->DataBufferLength / Lurn->LurnChildrenCnt;
			BlocksPerDisk = DataBufferLengthPerDisk / Lurn->ChildBlockBytes;

			// create new data buffer and encrypt here.
			// new data buffer will be deleted at completion routine
			cdb.DataBufferCount = 0;
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				cdb.DataBuffer[i] = ExAllocateFromNPagedLookasideList(&pRaidInfo->DataBufferLookaside);
				ASSERT(cdb.DataBuffer[i]);
				if(!cdb.DataBuffer[i])
				{
					// free allocated buffers
					for(j = 0; j < i; j++)
					{
						ExFreeToNPagedLookasideList(
							&pRaidInfo->DataBufferLookaside,
							cdb.DataBuffer[i]);
						cdb.DataBuffer[i] = NULL;
					}

					Ccb->CcbStatus = CCB_STATUS_SUCCESS;
					status = STATUS_INSUFFICIENT_RESOURCES;

					KDPrintM(DBG_LURN_ERROR, ("ExAllocateFromNPagedLookasideList Failed[%d]\n", i));
					LSCcbCompleteCcb(Ccb);
					break;
				}
				
				cdb.DataBufferLength[i] = DataBufferLengthPerDisk;
				cdb.DataBufferCount++;
			}
			
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory(
						(PCHAR)cdb.DataBuffer[i] + j * Lurn->ChildBlockBytes,
						(PCHAR)Ccb->DataBuffer + (i + j * (Lurn->LurnChildrenCnt)) * Lurn->ChildBlockBytes,
						Lurn->ChildBlockBytes);
				}

			}

			//
			//	send to all child LURNs.
			//
			status = LurnAssocSendCcbToAllChildren(
													Lurn,
													Ccb,
													LurnRAID0CcbCompletion,
													&cdb,
													NULL,
													LURN_CASCADE_FORWARD
									);

			if(!NT_SUCCESS(status))
			{
				for(i = 0; i < Lurn->LurnChildrenCnt; i++)
				{
					ExFreeToNPagedLookasideList(
						&pRaidInfo->DataBufferLookaside,
						cdb.DataBuffer[i]);
				}
			}
		}
		break;
	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:
		status = LurnAssocSendCcbToAllChildren(
			Lurn,
			Ccb,
			LurnRAID0CcbCompletion,
			NULL,
			NULL,
			LURN_CASCADE_FORWARD
			);
		break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_READ16:
		{
			int DataBufferLengthPerDisk;
			ULONG i, j;
			CUSTOM_DATA_BUFFER cdb;

#if 0
			{
				UINT64 logicalBlockAddress;
				UINT32 transferBlocks;

				LSCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &logicalBlockAddress, &transferBlocks);

				KDPrintM(DBG_LURN_NOISE,("RAID0 Read %I64x:%x, Buffer length %x\n", logicalBlockAddress, transferBlocks, Ccb->DataBufferLength));
			}
#endif

			DataBufferLengthPerDisk = Ccb->DataBufferLength / (Lurn->LurnChildrenCnt);
			KDPrintM(DBG_LURN_NOISE,("R Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			cdb.DataBufferCount = 0;
			for(i = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				cdb.DataBuffer[i] = ExAllocateFromNPagedLookasideList(&pRaidInfo->DataBufferLookaside);
				ASSERT(cdb.DataBuffer[i]);
				if(!cdb.DataBuffer[i])
				{
					// free allocated buffers
					for(j = 0; j < i; j++)
					{
						ExFreeToNPagedLookasideList(
							&pRaidInfo->DataBufferLookaside,
							cdb.DataBuffer[i]);
						cdb.DataBuffer[i] = NULL;
					}

					Ccb->CcbStatus = CCB_STATUS_SUCCESS;
					status = STATUS_INSUFFICIENT_RESOURCES;

					KDPrintM(DBG_LURN_ERROR, ("ExAllocateFromNPagedLookasideList Failed[%d]\n", i));
					LSCcbCompleteCcb(Ccb);
					break;
				}

				cdb.DataBufferLength[i] = DataBufferLengthPerDisk;
				cdb.DataBufferCount++;
			}

			cdb.DataBufferCount = i;
			
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID0CcbCompletion,
				&cdb,
				NULL,
				LURN_CASCADE_FORWARD
				);

			if(!NT_SUCCESS(status))
			{
				for(i = 0; i < Lurn->LurnChildrenCnt; i++)
				{
					ExFreeToNPagedLookasideList(
						&pRaidInfo->DataBufferLookaside,
						cdb.DataBuffer[i]);
				}
			}
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
		if(logicalBlockAddress < 0xffffffff) {
			REVERSE_BYTES(&readCapacityData->LogicalBlockAddress, &logicalBlockAddress);
		} else {
			readCapacityData->LogicalBlockAddress = 0xffffffff;
		}

		blockSize = Lurn->BlockBytes;
		REVERSE_BYTES(&readCapacityData->BytesPerBlock, &blockSize);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}
	case SCSIOP_READ_CAPACITY16:
	{
		PREAD_CAPACITY_DATA_EX		readCapacityDataEx;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityDataEx = (PREAD_CAPACITY_DATA_EX)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		REVERSE_BYTES_QUAD(&readCapacityDataEx->LogicalBlockAddress.QuadPart, &logicalBlockAddress);

		blockSize = Lurn->BlockBytes;
		REVERSE_BYTES(&readCapacityDataEx->BytesPerBlock, &blockSize);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}
	
	case SCSIOP_MODE_SENSE:
	{
		LurnAssocModeSense(Lurn, Ccb);
		break;
	}
#if 0
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
	
				if(LSCcbIsFlagOn(Ccb, CCB_FLAG_W2K_READONLY_PATCH) ||
					LSCcbIsFlagOn(Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS))
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = (ULONG)(Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1);
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(Lurn->BlockBytes>>16);
			parameterBlock->BlockLength[1] = (BYTE)(Lurn->BlockBytes>>8);
			parameterBlock->BlockLength[2] = (BYTE)(Lurn->BlockBytes);

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
#endif		
	case SCSIOP_MODE_SELECT:
		LurnAssocModeSelect(Lurn, Ccb);
		break;
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
												LURN_CASCADE_FORWARD
								);
		break;

	}

	return status;
}

NTSTATUS
RAID0LurnRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS				status;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("RAID0LurnRequest!\n"));

	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnRAID0Execute(Lurn, Ccb);
		break;
	case CCB_OPCODE_FLUSH:
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_FLUSH!\n"));
		// Nothing to do for aggregated disks. This flush is sent to controller side cache.		
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	case CCB_OPCODE_SHUTDOWN:
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_SHUTDOWN!\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
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
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID0CcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);
			break;
		}

	case CCB_OPCODE_UPDATE:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				LSCcbCompleteCcb(Ccb);
				break;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAIDUpdateCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD_CHAINING);
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

#if 0

//////////////////////////////////////////////////////////////////////////
//
//	RAID1 Lurn
//

NTSTATUS
RAID1RLurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_INFO pRaidInfo = NULL;
	NTSTATUS ntStatus;

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);

	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnType = %d\n", LurnDesc->LurnType));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnId = %d\n", LurnDesc->LurnId));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->StartBlockAddr = 0x%I64x\n", LurnDesc->StartBlockAddr));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->EndBlockAddr = 0x%I64x\n", LurnDesc->EndBlockAddr));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->UnitBlocks = 0x%I64x\n", LurnDesc->UnitBlocks));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->MaxDataSendLength = %d bytes\n", LurnDesc->MaxDataSendLength));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->MaxDataRecvLength = %d bytes\n", LurnDesc->MaxDataRecvLength));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnOptions = %d\n", LurnDesc->LurnOptions));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnParent = %d\n", LurnDesc->LurnParent));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildrenCnt = %d\n", LurnDesc->LurnChildrenCnt));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildren = 0x%p\n", LurnDesc->LurnChildren));

	//
	// Determine block bytes.
	//

	Lurn->ChildBlockBytes = LurnAsscGetChildBlockBytes(Lurn);
	if(Lurn->ChildBlockBytes == 0)
		return STATUS_DEVICE_NOT_READY;

	Lurn->BlockBytes = Lurn->ChildBlockBytes;

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

	KeInitializeSpinLock(&pRaidInfo->SpinLock);

	// set spare disk count
	pRaidInfo->nDiskCount = LurnDesc->LurnChildrenCnt - LurnDesc->LurnInfoRAID.nSpareDisk;
	pRaidInfo->nSpareDisk = LurnDesc->LurnInfoRAID.nSpareDisk;
	pRaidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID.SectorsPerBit;
	pRaidInfo->RaidSetId = LurnDesc->LurnInfoRAID.RaidSetId;
	pRaidInfo->ConfigSetId = LurnDesc->LurnInfoRAID.ConfigSetId;
	
	if(!pRaidInfo->SectorsPerBit)
	{
		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;
	}

	pRaidInfo->MaxDataSendLength = LurnDesc->MaxDataSendLength;
	pRaidInfo->MaxDataRecvLength = LurnDesc->MaxDataRecvLength;

	// Always create draid client.
	ntStatus = DraidClientStart(Lurn); 
	if (!NT_SUCCESS(ntStatus)) {
		goto out;
	}

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAIDInfo)
		{
			SAFE_FREE_POOL_WITH_TAG(Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG);
		}
	}
	

	return ntStatus;
}

NTSTATUS
RAID1RLurnDestroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAIDInfo);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	SAFE_FREE_POOL_WITH_TAG(pRaidInfo, RAID_INFO_POOL_TAG);

	return STATUS_SUCCESS ;
}

//
// to do: restruct this function!
//
NTSTATUS
LurnRAID1RCcbCompletion(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	)
{
	KIRQL	oldIrql;
	LONG	AssocCount;
	NTSTATUS status;
	PRAID_INFO pRaidInfo;
	PLURELATION_NODE pLurnOriginal;
	PLURELATION_NODE pLurnCurrent;
	PDRAID_CLIENT_INFO pClient;
	UINT32 DraidStatus;

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	pLurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	pLurnCurrent = Ccb->CcbCurrentStackLocation->Lurn;
	ASSERT(pLurnOriginal);
	pRaidInfo = pLurnOriginal->LurnRAIDInfo;
	ACQUIRE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);
	pClient = pRaidInfo->pDraidClient;
	if (pClient == NULL) {
		// Client is alreay terminated.
		DraidStatus = DRIX_RAID_STATUS_TERMINATED;
	} else {
		DraidStatus = pClient->DRaidStatus;
		DraidClientUpdateNodeFlags(pClient, pLurnCurrent, 0, 0);
	}
	RELEASE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);


	// 
	// Find proper Ccbstatus based on OriginalCcb->CcbStatus(Empty or may contain first completed child Ccb's staus) and this Ccb.
	//
#if 0
	if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {
		KDPrintM(DBG_LURN_INFO, ("LurnRAID1RCcbCompletion: CcbStatus = %x\n", Ccb->CcbStatus));
	}
#endif	
	switch(Ccb->Cdb[0]) {
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_READ16:	
		//
		// Read request is sent to only one target.
		// If success, pass through to original ccb
		// If error and 
		// 	another target is running state, return busy to make upper layer to retry to another host.
		// 	another target is down, pass this Ccb status to original ccb
		//
		if (Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
			break;
		} else {
			if (pClient && pClient->DRaidStatus == DRIX_RAID_STATUS_NORMAL) {
				// Maybe another node is in running state. Hope that node can handle request and return busy
				ASSERT(OriginalCcb->Srb || OriginalCcb->OperationCode == CCB_OPCODE_RESETBUS);
				KDPrintM(DBG_LURN_INFO, ("Read on RAID1 failed. Enter emergency and return busy for retrial on redundent target\n"));

				OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
			} else {
				// Error in degraded mode. Pass error 
				KDPrintM(DBG_LURN_INFO, ("Read on RAID1 failed and redundent target is in defective. Returning error %x\n", Ccb->CcbStatus));
				// 
				// No other target is alive. Pass error including sense data
				//
				OriginalCcb->CcbStatus = Ccb->CcbStatus;

				if(OriginalCcb->SenseBuffer != NULL) {
					RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
				}
			}
		}
		break;
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:
	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:		 // fall through
	default:
			//
		// CCB Status priority(Higher level of error override another device's error) : In high to low order.
		//	Etc error(Invalid command, out of bound, unknown error)
		//	CCB_STATUS_COMMAND_FAILED(Errors that upper layer need to know about error - bad sector, etc)
		//	CCB_STATUS_BUSY: (lower this status to below success priority to improve responsiveness??)
		//	CCB_STATUS_SUCCES: 
		//	CCB_STATUS_STOP/CCB_STATUS_NOT_EXIST/CCB_STATUS_COMMUNICATION_ERROR: Enter emergency if in normal status.
		//														Return as success if another target is success, or 
			//
		switch(Ccb->CcbStatus) {
		case CCB_STATUS_SUCCESS:
			if (OriginalCcb->CcbStatus == CCB_STATUS_STOP ||
				OriginalCcb->CcbStatus == CCB_STATUS_NOT_EXIST ||
				OriginalCcb->CcbStatus == CCB_STATUS_COMMUNICATION_ERROR) {

				// Make upper layer retry.
				OriginalCcb->CcbStatus = CCB_STATUS_BUSY;

				ACQUIRE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);
				if (pRaidInfo->pDraidClient) {
					pClient = pRaidInfo->pDraidClient;
					ACQUIRE_DPC_SPIN_LOCK(&pClient->SpinLock);
					if (pClient->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
						if (!(pClient->NodeFlagsRemote[pLurnCurrent->LurnChildIdx] & 
							(DRIX_NODE_FLAG_STOP|DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_OUT_OF_SYNC))) {
							// Succeeded node is none-fault node. It is okay.
							OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;
						} else {
							// None-fault node has failed, this cause RAID fail. Need to stop
							KDPrintM(DBG_LURN_ERROR, ("Non-defective target %d failed. RAID failure\n", pLurnCurrent->LurnChildIdx));
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;
						}
					} else {
						// No target should fail in other cases. If so, RAID status will be changed. Wait for it returning busy.
						KDPrintM(DBG_LURN_ERROR, ("Ccb for target %d failed not in degraded mode. Returning busy.\n", pLurnCurrent->LurnChildIdx));
						OriginalCcb->CcbStatus = CCB_STATUS_BUSY;						
					}
					RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);
				} else {
					KDPrintM(DBG_LURN_ERROR, ("RAID client already terminated\n"));
					// RAID has terminated. Pass status
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
				}
				RELEASE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);

			} else if (OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {

				// Both target returned success or another target has not completed yet
				// Set success
				OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;
			} else {
				//
				// All other error including CCB_STATUS_BUSY & CCB_STATUS_COMMAND_FAILED
				// OriginalCcb is already filled with error info, so just keep them.
				// 
			}
			break;
		case CCB_STATUS_STOP:
		case CCB_STATUS_NOT_EXIST:
		case CCB_STATUS_COMMUNICATION_ERROR: 

			if (OriginalCcb->ChildReqCount == 1) {
				// Request sent to only one host. Pass error to upper layer.
				OriginalCcb->CcbStatus = Ccb->CcbStatus;
				if(OriginalCcb->SenseBuffer != NULL) {
					RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
					}
			} else if (OriginalCcb->AssociateCount == 2) {
				//
				// Another target host didn't return yet. Fill Ccb with current status
				// 
				OriginalCcb->CcbStatus = Ccb->CcbStatus;
				if(OriginalCcb->SenseBuffer != NULL) {
					RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
				}

				ACQUIRE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);
				if (pRaidInfo->pDraidClient) {
					pClient = pRaidInfo->pDraidClient;
					ACQUIRE_DPC_SPIN_LOCK(&pClient->SpinLock);
					if (pClient->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
						if (pClient->NodeFlagsRemote[pLurnCurrent->LurnChildIdx] & 
							(DRIX_NODE_FLAG_STOP|DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_OUT_OF_SYNC)) {
							// Fault target has failed. It is okay. Pass code.

						} else {
							// Normal target has failed, this cause RAID fail. Need to stop
							KDPrintM(DBG_LURN_ERROR, ("Non-defective target %d failed. RAID failure\n", pLurnCurrent->LurnChildIdx));
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					}
					} else {
						// One of target is stopped not in degraded mode.
						KDPrintM(DBG_LURN_ERROR, ("Ccb for target %d failed not in degraded mode.\n", pLurnCurrent->LurnChildIdx));
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;						
					}
					RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);						
				} else {
					// RAID has terminated. Pass status
				}
				RELEASE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);

			} else if (OriginalCcb->AssociateCount ==1) {
				//
				// Another target has completed already.  Check its status
				//
				switch (OriginalCcb->CcbStatus) {
				case CCB_STATUS_SUCCESS:
					ACQUIRE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);
					if (pRaidInfo->pDraidClient) {
						pClient = pRaidInfo->pDraidClient;
						ACQUIRE_DPC_SPIN_LOCK(&pClient->SpinLock);
						//
						// One target has succeeded, another target has failed. 
						// If RAID is degraded status and failed target is already recognized as fault unit, it is okay
						//	But if not return busy.
						if (pClient->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
							if (pClient->NodeFlagsRemote[pLurnCurrent->LurnChildIdx] & 
								(DRIX_NODE_FLAG_STOP|DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_OUT_OF_SYNC)) {
								// Failed target is already marked as failure. This is expected situation.
								OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;
							} else {
								// Failed target is not marked as failed. 
								KDPrintM(DBG_LURN_ERROR, ("Non-defective target %d failed. Target flag = %x, RAID failure\n", 
									pLurnCurrent->LurnChildIdx));
								OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
							}
						} else {
							// No target should fail in other cases. If so, RAID status will be changed. Wait for it returning busy.
							KDPrintM(DBG_LURN_ERROR, ("One of the target ccb failed not in degraded mode. Returning busy.\n"));
							OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
						}
						RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);						
					} else {
						// client stopped
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					}
					RELEASE_DPC_SPIN_LOCK(&pRaidInfo->SpinLock);
					break;
				case CCB_STATUS_STOP:
				case CCB_STATUS_NOT_EXIST:
				case CCB_STATUS_COMMUNICATION_ERROR: 
					KDPrintM(DBG_LURN_ERROR, ("Both target returned error\n"));
					// both target completed with error
					// pass error.
					break;
				case CCB_STATUS_BUSY:
				case CCB_STATUS_COMMAND_FAILED:					
				default:
					// CCB_STATUS_BUSY,CCB_STATUS_COMMAND_FAILED has higher priority.
					// Go with OriginalCcb's status.
							break;
						}
					}

			break;
		case CCB_STATUS_BUSY:
			if (OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS &&
				OriginalCcb->CcbStatus != CCB_STATUS_STOP &&
				OriginalCcb->CcbStatus != CCB_STATUS_NOT_EXIST &&
				OriginalCcb->CcbStatus != CCB_STATUS_COMMUNICATION_ERROR) {
				// FAIL and unknown error. Preserve previous error
			} else {
				// Overwrite low priority errors
				OriginalCcb->CcbStatus = Ccb->CcbStatus;
				if(OriginalCcb->SenseBuffer != NULL) {
					RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
				}
			}
			break;	
		case CCB_STATUS_COMMAND_FAILED:
			if (OriginalCcb->CcbStatus != CCB_STATUS_BUSY &&
				OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS &&
				OriginalCcb->CcbStatus != CCB_STATUS_STOP &&
				OriginalCcb->CcbStatus != CCB_STATUS_NOT_EXIST &&
				OriginalCcb->CcbStatus != CCB_STATUS_COMMUNICATION_ERROR) {
				// Unknown error. Preserve previous error
		} else {
				// Overwrite low priority errors
				OriginalCcb->CcbStatus = Ccb->CcbStatus;
				if(OriginalCcb->SenseBuffer != NULL) {
					RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
				}
		}
		break;

		default:
			//
			// Pass error to upper level regardless result of another child.
			//
			// To do: STATUS and error code from the target completed late overwrite previous status. Need to combine two error.
			//
			if (OriginalCcb->CcbStatus == CCB_STATUS_BUSY) {
				OriginalCcb->CcbStatus = Ccb->CcbStatus;
				if(OriginalCcb->SenseBuffer != NULL) {
					RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
				}
			}
			break;
		}
		break;
	}

	if(DRIX_RAID_STATUS_FAILED ==  DraidStatus ||
		DRIX_RAID_STATUS_TERMINATED == DraidStatus)
	{
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
		KDPrintM(DBG_LURN_INFO,("RAID status is %d. Ccb status stop\n", DraidStatus));
	}

	LSCcbSetStatusFlag(	OriginalCcb,
		Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
		);

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//
	//	Complete the original CCB
	//
	AssocCount = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(AssocCount >= 0);
	if(AssocCount != 0) {
		return status;
	}

	if (OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS) {
		KDPrintM(DBG_LURN_INFO,("Completing Ccb with status %x\n", OriginalCcb->CcbStatus));
	}

	if (pClient && (OriginalCcb->Cdb[0] == SCSIOP_WRITE ||OriginalCcb->Cdb[0] == SCSIOP_WRITE16)) {
		DraidReleaseBlockIoPermissionToClient(pClient, OriginalCcb);
	}

	LSAssocSetRedundentRaidStatusFlag(pLurnOriginal, OriginalCcb);
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}


NTSTATUS
LurnRAID1RFlushCompletionForStopUnit(
		IN PCCB	Ccb,	
		IN PVOID Param // Not used.
) {
	NTSTATUS status;
	UNREFERENCED_PARAMETER(Param);

	// Ccb is not for this completion. Send to all children.
	status = LurnAssocSendCcbToAllChildren(
		Ccb->CcbCurrentStackLocation->Lurn,
		Ccb,
		LurnRAID1RCcbCompletion,
		NULL,
		NULL,
		FALSE
		);
	return STATUS_SUCCESS;
}


static
NTSTATUS
LurnRAID1RExecute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) 
{
	NTSTATUS			status;

	PRAID_INFO pRaidInfo;
	KIRQL				oldIrql;
	PDRAID_CLIENT_INFO pClient;

	pRaidInfo = Lurn->LurnRAIDInfo;

	//
	// Forward disk IO related request to Client thread
	//
	ACQUIRE_SPIN_LOCK(&pRaidInfo->SpinLock, &oldIrql);	
	pClient = pRaidInfo->pDraidClient;
	if (pClient) {
		ACQUIRE_DPC_SPIN_LOCK(&pClient->SpinLock);
		if (pClient->DRaidStatus != DRIX_RAID_STATUS_TERMINATED) {
			if (pClient->ClientState == DRAID_CLIENT_STATE_NO_ARBITER) {
				if (Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16) {
					// We cannot handle write in no arbiter mode.
					RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);
					RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
					KDPrintM(DBG_LURN_INFO, ("Cannot handle write in no arbiter mode. Returning busy\n"));
					Ccb->CcbStatus = CCB_STATUS_BUSY;
					goto complete_here;
				}
				if (pClient->InTransition)  {

					KDPrintM(DBG_LURN_INFO, ("Allowing operation in transition status.\n"));
				}
				// Allow read when no arbiter exist.
			} else if (pClient->InTransition)  {
				//
				// RAID can handle some operations even in transtion
				//
				switch(Ccb->Cdb[0]) {
					case SCSIOP_WRITE:
					case SCSIOP_WRITE16:		
					case 0x3E:		// READ_LONG
					case SCSIOP_READ:
					case SCSIOP_READ16:		
					case SCSIOP_VERIFY:
					case SCSIOP_VERIFY16:	
						RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);
						RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);						
						KDPrintM(DBG_LURN_INFO, ("IO(opcode=%x) cannot be process in transition. Returning busy\n", Ccb->Cdb[0]));
						Ccb->CcbStatus = CCB_STATUS_BUSY;
						goto complete_here;
					default:
						KDPrintM(DBG_LURN_INFO, ("Allowing operation in transition status: %x.\n", Ccb->Cdb[0]));
						break;
				}
			}
			switch(Ccb->Cdb[0]) {
				case SCSIOP_WRITE:
				case SCSIOP_WRITE16:		
				case 0x3E:		// READ_LONG
				case SCSIOP_READ:
				case SCSIOP_READ16:		
				case SCSIOP_VERIFY:
				case SCSIOP_VERIFY16:		
					// These command will be handled by client thread
					InsertTailList(&pClient->CcbQueue, &Ccb->ListEntry);
					KeSetEvent(&pClient->ClientThreadEvent, IO_NO_INCREMENT, FALSE);
					LSCcbMarkCcbAsPending(Ccb);
					RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);
					RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
					return STATUS_PENDING; // no meaning..
				default:
					break;
			}
		} else {
			// Terminated 
			KDPrintM(DBG_LURN_INFO, ("RAID client is terminating. Returning stop status\n"));			
			RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);
			RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
			Ccb->CcbStatus = CCB_STATUS_STOP;
			goto complete_here;
		}
		RELEASE_DPC_SPIN_LOCK(&pClient->SpinLock);
	}
	RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
	
	switch(Ccb->Cdb[0]) {
	case SCSIOP_INQUIRY: {
		INQUIRYDATA			inquiryData;
		UCHAR				Model[16] = RAID1R_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));
		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			goto complete_here;
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
			if(logicalBlockAddress < 0xffffffff) {
				REVERSE_BYTES(&readCapacityData->LogicalBlockAddress, &logicalBlockAddress);
			} else {
				readCapacityData->LogicalBlockAddress = 0xffffffff;
			}

			blockSize = Lurn->BlockBytes;
			REVERSE_BYTES(&readCapacityData->BytesPerBlock, &blockSize);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
	case SCSIOP_START_STOP_UNIT:
		{
			PCDB		cdb = (PCDB)(Ccb->Cdb);

			if(cdb->START_STOP.Start == START_UNIT_CODE) {
				//
				// Start. Nothing to do. Pass down Ccb.
				//
				status = LurnAssocSendCcbToAllChildren(
					Lurn,
					Ccb,
					LurnRAID1RCcbCompletion,
					NULL,
					NULL,
					FALSE
					);
				break;
			} else if(cdb->START_STOP.Start == STOP_UNIT_CODE) {
				//
				// In rebuilding state, don't send stop to child.
				// To do: check another host is accessing the disk.
				//
				// STOP is sent to spin-down HDD. It may be safe to ignore.
				//
				if (pClient->DRaidStatus == DRIX_RAID_STATUS_REBUILDING) {
					KDPrintM(DBG_LURN_ERROR,
						("RAID is in rebuilding status: Don't stop unit\n"));
					LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
					DraidClientFlush(pClient, NULL, NULL);
					goto complete_here;
				}

				//
				// Flush to reset dirty bitmaps.
				// LurnRAID1RFlushCompletionForStopUnit will send stop to child.
				//
				status = DraidClientFlush(pClient, Ccb, LurnRAID1RFlushCompletionForStopUnit);

				if (status == STATUS_PENDING) {
					LSCcbMarkCcbAsPending(Ccb);
				} else {
					// Assume success
					LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
					goto complete_here;
				}
				
			} else {
				KDPrintM(DBG_LURN_ERROR,
					("SCSIOP_START_STOP_UNIT: Invaild operation!!! %d %d.\n", Ccb->LurId[1], cdb->START_STOP.LogicalUnitNumber));
				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;	//SRB_STATUS_INVALID_REQUEST;
				goto complete_here;
			}
			break;
		}

		
	case SCSIOP_READ_CAPACITY16:
		{
			PREAD_CAPACITY_DATA_EX		readCapacityDataEx;
			ULONG				blockSize;
			UINT64				sectorCount;
			UINT64				logicalBlockAddress;

			sectorCount = Lurn->UnitBlocks;

			readCapacityDataEx = (PREAD_CAPACITY_DATA_EX)Ccb->DataBuffer;

			logicalBlockAddress = sectorCount - 1;
			REVERSE_BYTES_QUAD(&readCapacityDataEx->LogicalBlockAddress.QuadPart, &logicalBlockAddress);

			blockSize = Lurn->BlockBytes;
			REVERSE_BYTES(&readCapacityDataEx->BytesPerBlock, &blockSize);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
	
	case SCSIOP_MODE_SENSE:
		LurnAssocModeSense(Lurn, Ccb);
		break;
	case SCSIOP_MODE_SELECT:
		LurnAssocModeSelect(Lurn, Ccb);
		break;

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
				LurnRAID1RCcbCompletion,
				NULL,
				NULL,
				FALSE
				);
		}
		break;

	}

	return STATUS_SUCCESS;
complete_here:
	LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);

	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID1RFlushCompletion(
		IN PCCB	Ccb,	
		IN PVOID Param // Not used.
) {
	UNREFERENCED_PARAMETER(Param);
	LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
	LSAssocSetRedundentRaidStatusFlag(Ccb->CcbCurrentStackLocation->Lurn, Ccb);
	LSCcbCompleteCcb(Ccb);
	return STATUS_SUCCESS;
}

typedef struct _LURN_RAID_SHUT_DOWN_PARAM {
	PIO_WORKITEM 	IoWorkItem;
	PLURELATION_NODE	Lurn;
	PCCB				Ccb;
} LURN_RAID_SHUT_DOWN_PARAM, *PLURN_RAID_SHUT_DOWN_PARAM;

VOID
LurnRAID1ShutDownWorker(
	IN PDEVICE_OBJECT  DeviceObject,
	IN PVOID Parameter
) {
	PLURN_RAID_SHUT_DOWN_PARAM Params = (PLURN_RAID_SHUT_DOWN_PARAM) Parameter;
	PRAID_INFO pRaidInfo;

	UNREFERENCED_PARAMETER(DeviceObject);

	//
	// Is it possible that LURN is destroyed already?
	// 

	KDPrintM(DBG_LURN_INFO, ("Shutdowning DRAID\n"));
	
	pRaidInfo = Params->Lurn->LurnRAIDInfo;

	DraidClientStop(Params->Lurn);
	if (pRaidInfo->pDraidArbiter)
		DraidArbiterStop(Params->Lurn);

	//
	// Don't need to pass to child. lslurnide does nothing about CCB_OPCODE_SHUTDOWN 
	// Or we can set synchronous flag to ccb
//	status = LurnAssocSendCcbToAllChildren(Params->Lurn, Params->Ccb, LurnRAID1RCcbCompletion, NULL, NULL, FALSE);

	LSCcbSetStatus(Params->Ccb, CCB_STATUS_SUCCESS);
	LSCcbCompleteCcb(Params->Ccb);
	
	IoFreeWorkItem(Params->IoWorkItem);
	ExFreePoolWithTag(Params, DRAID_SHUTDOWN_POOL_TAG);
}


NTSTATUS
RAID1RLurnRequest(
				 PLURELATION_NODE	Lurn,
				 PCCB				Ccb
				 )
{
	NTSTATUS				status;
	PRAID_INFO pRaidInfo;
	KIRQL	oldIrql;
	pRaidInfo = Lurn->LurnRAIDInfo;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("IN\n"));

	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnRAID1RExecute(Lurn, Ccb);
		break;

		//
		//	Send to all LURNs
		//
	case CCB_OPCODE_ABORT_COMMAND:
	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_RESTART:
	case CCB_OPCODE_NOOP:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				//			LSCcbCompleteCcb(Ccb);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID1RCcbCompletion, NULL, NULL, FALSE);
			break;
		}
	case CCB_OPCODE_FLUSH:
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_FLUSH\n"));
		//
		// This code may be running at DPC level.
		// Flush operation should not block
		//

		status = DraidClientFlush(pRaidInfo->pDraidClient, Ccb, LurnRAID1RFlushCompletion);
		if (status == STATUS_PENDING) {
			LSCcbMarkCcbAsPending(Ccb);
		} else {
			// Assume success
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			goto complete_here;
		}
		break;

	case CCB_OPCODE_SHUTDOWN:
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_SHUTDOWN\n"));
		//
		// This code may be running at DPC level.
		// Run stop operation asynchrously.
		//
		// Alloc work item and call LurnRAID1ShutDownWorker
		{
			PLURN_RAID_SHUT_DOWN_PARAM Param;

			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

			if(Lurn->LurnStatus == LURN_STATUS_STOP_PENDING || Lurn->LurnStatus == LURN_STATUS_STOP)
			{
				RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);		
				// Already stopping
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;				
			} else {
				Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
			}
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			Param = ExAllocatePoolWithTag(
				NonPagedPool, sizeof(LURN_RAID_SHUT_DOWN_PARAM), DRAID_SHUTDOWN_POOL_TAG);
			if (Param==NULL) {
				KDPrintM(DBG_LURN_INFO, ("Failed to alloc shutdown worker\n"));
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;				
			}
			if (Lurn && Lurn->Lur && Lurn->Lur->AdapterFunctionDeviceObject) {
				Param->IoWorkItem = IoAllocateWorkItem(Lurn->Lur->AdapterFunctionDeviceObject);
				if (!Param->IoWorkItem) {
					KDPrintM(DBG_LURN_INFO, ("Failed to alloc shutdown worker\n"));
					LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
					ExFreePoolWithTag(Param, DRAID_SHUTDOWN_POOL_TAG);
					goto complete_here;					
				}
				Param->Lurn = Lurn;
				Param->Ccb = Ccb;
				KDPrintM(DBG_LURN_INFO, ("Queuing shutdown work to IoWorkItem\n"));
				IoQueueWorkItem(Param->IoWorkItem, LurnRAID1ShutDownWorker, DelayedWorkQueue, Param);
			} else {
				ASSERT(FALSE);
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				ExFreePoolWithTag(Param, DRAID_SHUTDOWN_POOL_TAG);
				goto complete_here;				
			}
		}
		break;
	case CCB_OPCODE_STOP:
		{
			KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_STOP\n"));
			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
			if(Lurn->LurnStatus == LURN_STATUS_STOP_PENDING || Lurn->LurnStatus == LURN_STATUS_STOP)
			{
				RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);		
				// Already stopping
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;				
			} else {
				Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
			}
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			DraidClientStop(Lurn);
			if (pRaidInfo->pDraidArbiter)
				DraidArbiterStop(Lurn);

			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID1RCcbCompletion, NULL, NULL, FALSE);
			break;
		}
	case CCB_OPCODE_UPDATE:
		{
			KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_UPDATE requested to RAID1\n"));	
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				//			LSCcbCompleteCcb(Ccb);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAIDUpdateCcbCompletion, NULL, NULL, TRUE);
			break;
		}

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		//		LSCcbCompleteCcb(Ccb);
		goto complete_here;
	}

	return STATUS_SUCCESS;

complete_here:

	LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);

	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}



//
// Currently we do not support hot-swap and RAID with conflict configuration.
// Return error if any of the member does not have expected value.
// Make user to resolve the problem using bindtool.
//
NTSTATUS
LurnRMDRead(
	IN PLURELATION_NODE		Lurn, 
	OUT PNDAS_RAID_META_DATA rmd,
	OUT PUINT32 UpTodateNode
)
{
	NTSTATUS status;
	ULONG i, j;
	NDAS_RAID_META_DATA rmd_tmp;
	UINT32 uiUSNMax;
	UINT32 FreshestNode = 0;
	BOOLEAN		UsedInDegraded[MAX_DRAID_MEMBER_DISK] = {0};
	
	// Update NodeFlags if it's RMD is missing or invalid.
	
	KDPrintM(DBG_LURN_INFO, ("IN\n"));

	uiUSNMax = 0;

	for(i = 0; i < Lurn->LurnChildrenCnt; i++)	// i is node flag
	{
		if(!LURN_IS_RUNNING(Lurn->LurnChildren[i]->LurnStatus)) {
			KDPrintM(DBG_LURN_INFO, ("Lurn is not running. Skip reading node %d.\n", i));
			continue;
		}
		status = LurnExecuteSyncRead(Lurn->LurnChildren[i], (PUCHAR)&rmd_tmp,
			NDAS_BLOCK_LOCATION_RMD, 1);

		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Failed to read from node %d\n", i));
			uiUSNMax = 0;
			break;
		}
		if(NDAS_RAID_META_DATA_SIGNATURE != rmd_tmp.Signature || 
			!IS_RMD_CRC_VALID(crc32_calc, rmd_tmp))
		{
			KDPrintM(DBG_LURN_INFO, ("Node %d has invalid RMD. All disk must have RMD\n", i));
			uiUSNMax = 0;
			break;
		} else if (RtlCompareMemory(&Lurn->LurnRAIDInfo->RaidSetId, &rmd_tmp.RaidSetId, sizeof(GUID)) != sizeof(GUID)) {
			KDPrintM(DBG_LURN_INFO, ("Node %d is not member of this RAID set\n", i));
			uiUSNMax = 0;
			break;
		} else if (RtlCompareMemory(&Lurn->LurnRAIDInfo->ConfigSetId, &rmd_tmp.ConfigSetId, sizeof(GUID)) != sizeof(GUID)) {
			KDPrintM(DBG_LURN_INFO, ("Node %d has different configuration set.\n", i));
			//
			// To do: mark this node as defective and continue.
			//
			uiUSNMax = 0;
			break;			
		} else {
			if(uiUSNMax < rmd_tmp.uiUSN)
			{
				BOOLEAN SpareDisk = FALSE;
				BOOLEAN OosDisk = FALSE;
				for(j=0;j<Lurn->LurnChildrenCnt;j++) { // Role index
					if (rmd_tmp.UnitMetaData[j].iUnitDeviceIdx == i && 
						(rmd_tmp.UnitMetaData[j].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_SPARE)) {
						SpareDisk = TRUE;
					}
					if (rmd_tmp.UnitMetaData[j].iUnitDeviceIdx == i && 
						(rmd_tmp.UnitMetaData[j].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {
						OosDisk = TRUE;
					}
				}
				if (SpareDisk) {
					//
					// Ignore spare disk's RMD. Because this disk's OOS bitmap information may not be up-to-date
					//
					KDPrintM(DBG_LURN_INFO, ("Spare disk has newer RMD USN %x but ignore it\n", uiUSNMax));
				} else if (OosDisk) {
					//
					// Ignore OOS disk's RMD. Because this disk's OOS bitmap information may not be up-to-date
					//
					KDPrintM(DBG_LURN_INFO, ("Disk has newer RMD USN %x but ignore it\n", uiUSNMax));
				} else {
					uiUSNMax = rmd_tmp.uiUSN;
					KDPrintM(DBG_LURN_INFO, ("Found newer RMD USN %x from node %d\n", uiUSNMax, i));

					// newer one
					RtlCopyMemory(rmd, &rmd_tmp, sizeof(NDAS_RAID_META_DATA));
					FreshestNode = i;
				}
			}
			if (rmd_tmp.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
				UsedInDegraded[i] = TRUE;
			}
		}
	}

	if(0 == uiUSNMax)
	{
		// This can happen if information that svc given is different from actual RMD.
		KDPrintM(DBG_LURN_INFO, ("Cannot find valid RMD or some LURN does not have valid RMD.\n"));
		RtlZeroMemory(rmd, sizeof(NDAS_RAID_META_DATA));
		status = STATUS_UNSUCCESSFUL;
		ASSERT(FALSE); // You can ignore this. Simply RAID will be unmounted.
	} else {
		status = STATUS_SUCCESS;
	}

	if (Lurn->LurnType == LURN_RAID1R) {
		//
		// Check UsedInDegraded flag is conflicted
		//	(We can assume RAID map is same if ConfigurationSetId matches)
		//		Check active member is all marked as used in degraded mode.
		//
		if (UsedInDegraded[rmd->UnitMetaData[0].iUnitDeviceIdx] == TRUE &&
			UsedInDegraded[rmd->UnitMetaData[1].iUnitDeviceIdx] == TRUE) {
			// Both disk is used in degraded mode. User need to solve this problem.
			// fail ReadRmd
			KDPrintM(DBG_LURN_INFO, ("All active members had been independently mounted in degraded mode. Conflict RAID situation. Cannot continue\n"));
			RtlZeroMemory(rmd, sizeof(NDAS_RAID_META_DATA));
			status = STATUS_UNSUCCESSFUL;
			uiUSNMax = 0;
		}
	}

	if (UpTodateNode)
		*UpTodateNode = FreshestNode;

	KDPrintM(DBG_LURN_INFO, ("OUT\n"));
	return status;
}

#endif
