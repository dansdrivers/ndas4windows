#include <ntddk.h>
//#include "public.h"
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
//

static
NTSTATUS
LurnAssocSendCcbToChildrenArray(
		IN PLURELATION_NODE			*pLurnChildren,
		IN LONG						ChildrenCnt,
		IN PCCB						Ccb,
		IN CCB_COMPLETION_ROUTINE	CcbCompletion,
		IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
		IN PVOID					*apExtendedCmd // NULL if no cmd
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

//////////////////////////////////////////////////////////////////////////
//
//	common to associate LURN
//

static
NTSTATUS
LurnAssocSendCcbToAllChildren(
		IN PLURELATION_NODE			Lurn,
		IN PCCB						Ccb,
		IN CCB_COMPLETION_ROUTINE	CcbCompletion,
		IN PCUSTOM_DATA_BUFFER		pcdbDataBuffer,
		IN PVOID					*apExtendedCmd
){
	return LurnAssocSendCcbToChildrenArray((PLURELATION_NODE *)Lurn->LurnChildren, Lurn->LurnChildrenCnt, Ccb, CcbCompletion, pcdbDataBuffer, apExtendedCmd);
}
static
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

			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, CcbCompletion, NULL, NULL);

		}
		break;

	case LurRefreshLurn:
		{
			// only the leaf nodes will process this query
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, CcbCompletion, NULL, NULL);
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

static
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
					Ccb->CcbStatusFlags
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
					KDPrintM(DBG_LURN_INFO,("Updated Lur->GrantedAccess: %08lx\n\n",Lurn->Lur->GrantedAccess));
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
	LONG				idx_child;
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
												NULL
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
	case CCB_OPCODE_UPDATE:
	case CCB_OPCODE_NOOP: {

		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnAggrCcbCompletion, NULL, NULL);
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
						Ccb->CcbStatusFlags
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
					Ccb->CcbStatusFlags
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

	LSCcbSetStatusFlag(OriginalCcb, Ccb->CcbStatusFlags);
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
					Ccb->CcbStatusFlags
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
					KDPrintM(DBG_LURN_INFO,("Updated Lur->GrantedAccess: %08lx\n\n",Lurn->Lur->GrantedAccess));
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
												NULL
								);
		break;
		}
	case 0x3E:		// READ_LONG
	case SCSIOP_READ: {
		LONG				idx_child;
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
												NULL
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
	case CCB_OPCODE_NOOP: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnFaultTolerantCcbCompletion, NULL, NULL);
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
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL);
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
	PRAID0_INFO pRaidInfo = NULL;
	ULONG ulDataBufferSize, ulDataBufferSizePerDisk;
	NTSTATUS ntStatus;
	UINT32 i;
	PRAID0_CHILD_INFO pChildInfo;

	UNREFERENCED_PARAMETER(LurnDesc);
//	Raid Information
	Lurn->LurnRAID0Info = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID0_INFO), 
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRAID0Info)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRAID0Info;

	RtlZeroMemory(pRaidInfo, sizeof(RAID0_INFO));

	pRaidInfo->RaidStatus = RAID0_STATUS_NORMAL;

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
	pRaidInfo->Children = ExAllocatePoolWithTag(NonPagedPool, 
		Lurn->LurnChildrenCnt * sizeof(RAID0_CHILD_INFO), RAID_INFO_POOL_TAG);
	if(NULL == pRaidInfo->Children)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}
	RtlZeroMemory(pRaidInfo->Children, Lurn->LurnChildrenCnt * sizeof(RAID0_CHILD_INFO));

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
		if(Lurn->LurnRAID0Info)
		{
			if(pRaidInfo->DataBufferAllocated)
			{
				ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG);
				pRaidInfo->DataBufferAllocated = NULL;
			}
			if(pRaidInfo->Children)
			{
				ExFreePoolWithTag(pRaidInfo->Children, RAID_INFO_POOL_TAG);
				pRaidInfo->Children = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnRAID0Info, RAID_INFO_POOL_TAG);
			Lurn->LurnRAID0Info = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnRAID0Destroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID0_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAID0Info);

	pRaidInfo = Lurn->LurnRAID0Info;

	ASSERT(pRaidInfo->DataBufferAllocated);
	ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG) ;
	pRaidInfo->DataBufferAllocated = NULL;

	ASSERT(pRaidInfo->Children);
	ExFreePoolWithTag(pRaidInfo->Children, RAID_INFO_POOL_TAG);
	pRaidInfo->Children = NULL;

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
	PRAID0_INFO pRaidInfo;
	PLURELATION_NODE pLurnOriginal;

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	pLurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	pRaidInfo = pLurnOriginal->LurnRAID0Info;

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
						Ccb->CcbStatusFlags
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
			int BlocksPerDisk;
			int i, j;

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
	PRAID0_INFO			pRaidInfo;

	pRaidInfo = Lurn->LurnRAID0Info;

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
			int BlocksPerDisk;
			register int i, j;
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
													NULL
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
				NULL
				);
		}
		break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
		{
			int DataBufferLengthPerDisk;
			int i;
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
				NULL
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
												NULL
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
	case CCB_OPCODE_NOOP: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID0CcbCompletion, NULL, NULL);
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
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL); // use same function as Mirror
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
	PRAID1_INFO pRaidInfo = NULL;
	ULONG ulBitMapSize;
	NTSTATUS ntStatus;
	PRAID1_CHILD_INFO pChildInfo;
	ULONG i;

//	Raid Information
	Lurn->LurnRAID1Info = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID1_INFO),
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRAID1Info)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRAID1Info;

	RtlZeroMemory(pRaidInfo, sizeof(RAID1_INFO));

	pRaidInfo->RaidStatus = RAID1_STATUS_NORMAL;

	pRaidInfo->LockDataBuffer = 0;

	//	Children information
	RtlZeroMemory(pRaidInfo->Children, DISKS_IN_RAID1_PAIR * sizeof(RAID1_CHILD_INFO));

	for(i = 0; i < DISKS_IN_RAID1_PAIR; i++)
	{
		pChildInfo = &pRaidInfo->Children[i];

		RtlCopyMemory(&pChildInfo->InfoRaid1, &LurnDesc->LurnInfoRAID1[i], sizeof(INFO_RAID1));

		pChildInfo->ExtendedCommandForTag.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
		pChildInfo->ExtendedCommandForTag.Operation = CCB_EXT_WRITE;
		pChildInfo->ExtendedCommandForTag.logicalBlockAddress = (ULONG)pRaidInfo->Children[i].InfoRaid1.SectorLastWrittenInfo;
		pChildInfo->ExtendedCommandForTag.Offset = 0;
		pChildInfo->ExtendedCommandForTag.LengthByte = sizeof(CMD_BYTE_LAST_WRITTEN_SECTOR);
		pChildInfo->ExtendedCommandForTag.ByteOperation = BYTE_OPERATION_COPY;
		pChildInfo->ExtendedCommandForTag.pNextCmd = NULL; // varis
		pChildInfo->ExtendedCommandForTag.pByteData = (PBYTE)&pChildInfo->ByteLastWrittenSector;
		pChildInfo->ByteLastWrittenSector.timeStamp = 0;
	}

	// ExtendedComandForBitmap
	pRaidInfo->ExtendedComandForBitmap.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
	pRaidInfo->ExtendedComandForBitmap.Operation = CCB_EXT_WRITE;
//	pRaidInfo->ExtendedComandForBitmap.logicalBlockAddress = (ULONG)pRaidInfo->Children[i]->InfoRaid1.SectorLastWrittenInfo;
	pRaidInfo->ExtendedComandForBitmap.Offset = 0;
//	pRaidInfo->ExtendedComandForBitmap.LengthByte = 512; // 512 or 1024
	pRaidInfo->ExtendedComandForBitmap.ByteOperation = BLOCK_OPERATION_WRITE;
//	pRaidInfo->ExtendedComandForBitmap.pByteData = (BYTE *)&pRaidInfo->ExtendedComandForBitmap + sizeof(CMD_BYTE_OP);
	pRaidInfo->ExtendedComandForBitmap.pNextCmd = NULL; // varis

//	Bitmap (1) * (bitmap structure size + bitmap size)
	if(pRaidInfo->Children[0].InfoRaid1.SectorsPerBit) {
		ulBitMapSize = (UINT32)(sizeof(BYTE) * (Lurn->UnitBlocks / 
			(pRaidInfo->Children[0].InfoRaid1.SectorsPerBit * 8))) +1;

	} else {

		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;

	}

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

	ntStatus = STATUS_SUCCESS;

out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAID1Info)
		{
			if(pRaidInfo->Bitmap)
			{
				ExFreePoolWithTag(pRaidInfo->Bitmap, RAID_BITMAP_POOL_TAG);
				pRaidInfo->Bitmap = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnRAID1Info, RAID_INFO_POOL_TAG);
			Lurn->LurnRAID1Info = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnRAID1Destroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID1_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAID1Info);

	pRaidInfo = Lurn->LurnRAID1Info;
	ASSERT(pRaidInfo);

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
	PRAID1_INFO pRaidInfo;
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
	pRaidInfo = pLurnOriginal->LurnRAID1Info;

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
*	. Set raid status(OriginalCcb, pRaidInfo->RaidStatus) to RAID1_STATUS_FAIL_DETECTED
*	. Set IDE Lurn status(Ccb) to 'accept bitmapped' write only
*	. enumerate all write data & set busy if the data does not have bitmap
*	. release lock
*
*	Process after a child stop
*	. lock & check bitmap
*	. if RAID1_STATUS_FAIL_DETECTED, create extended command
*	. if bitmap changed, create extended command to write bitmap at non-user sector
*	. LurnIdeDiskExecute will process extended command
*	. release lock
*/
			PLURELATION_NODE pLurnChildDefected;
			int i;

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

				ASSERT(pLurnChildDefected->LurnRAID1Info);

				// 1 fail + 1 fail = broken
				if(RAID1_STATUS_NORMAL != pRaidInfo->RaidStatus)
				{
					if(pLurnChildDefected != pLurnOriginal->LurnChildren[pRaidInfo->iDefectedChild])
					{
						ASSERT(FALSE);
//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					}
					break;
				}
				
				pRaidInfo->RaidStatus = RAID1_STATUS_FAIL_DETECTED;

				for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
				{
					if(pLurnChildDefected == pLurnOriginal->LurnChildren[i])
					{
						pRaidInfo->iDefectedChild = i;
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

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iDefectedChild = %d\n", pRaidInfo->iDefectedChild));
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
		KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d, OriginalCcb->AssociateCount = %d\n",
			(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID, OriginalCcb->AssociateCount));

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
						Ccb->CcbStatusFlags
		);
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

	if(pRaidInfo->RaidStatus != RAID1_STATUS_NORMAL)
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

	PRAID1_INFO pRaidInfo;
	LONG iChildToRecordBitmap;
	PCMD_BYTE_OP	pExtendedCommands[DISKS_IN_RAID1_PAIR];

	// initialize extended commands
	RtlZeroMemory(pExtendedCommands, DISKS_IN_RAID1_PAIR * sizeof(PCMD_BYTE_OP));

	pRaidInfo = Lurn->LurnRAID1Info;

	// record a bitmap information to the opposite disk of the defected disk
	ASSERT(Lurn->LurnChildrenCnt == DISKS_IN_RAID1_PAIR);
	if(RAID1_STATUS_NORMAL != pRaidInfo->RaidStatus)
	{
		ASSERT(pRaidInfo->iDefectedChild == 0 || pRaidInfo->iDefectedChild == 1);
		iChildToRecordBitmap = (pRaidInfo->iDefectedChild == Lurn->LurnChildrenCnt -1) ? 0 : pRaidInfo->iDefectedChild +1;
	}


	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		// lock buffer : release at completion / do not forget to release when fail
		while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;
			register int i;

			KDPrintM(DBG_LURN_TRACE, ("SCSIOP_WRITE\n"));

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID1_STATUS_NORMAL: // write information where writing on.
				// record last written sector
				{
					// AING_TO_DO : use Ready made pool
					for(i = 0; i < Lurn->LurnChildrenCnt; i++)
					{
						pExtendedCommands[i] = &pRaidInfo->Children[i].ExtendedCommandForTag;
						pExtendedCommands[i]->pNextCmd = NULL;

						pRaidInfo->Children[i].ByteLastWrittenSector.logicalBlockAddress = (UINT64)logicalBlockAddress;
						pRaidInfo->Children[i].ByteLastWrittenSector.transferBlocks = (UINT32)transferBlocks;
						pRaidInfo->Children[i].ByteLastWrittenSector.timeStamp++;
					}
				}
				break;
			case RAID1_STATUS_FAIL_DETECTED: // dirty flag
/*
				{

					KDPrintM(DBG_LURN_ERROR,("RAID1_STATUS_FAIL_DETECTED\n"));

					pExtendedCommands[iHealthyChild] = (PCMD_BYTE_OP)ExAllocatePoolWithTag(NonPagedPool, 
						(sizeof(CMD_BYTE_OP)), EXTENDED_COMMAND_POOL_TAG);
					if(!pExtendedCommands[iHealthyChild])
					{
						return STATUS_INSUFFICIENT_RESOURCES;
					}

					pExtendedCommands[iHealthyChild]->pLurnCreated = Lurn;
					pExtendedCommands[iHealthyChild]->Operation = CCB_EXT_READ_OPERATE_WRITE;
					pExtendedCommands[iHealthyChild]->logicalBlockAddress = (UINT32)pRaidInfo->ChildrenInfo[iHealthyChild].SectorInfo;
					pExtendedCommands[iHealthyChild]->Offset = (UINT16)pRaidInfo->ChildrenInfo[iHealthyChild].OffsetFlagInfo;
					pExtendedCommands[iHealthyChild]->LengthByte = 1;
					pExtendedCommands[iHealthyChild]->ByteOperation = BYTE_OPERATION_COPY;
					pExtendedCommands[iHealthyChild]->pNextCmd = NULL;
					*pExtendedCommands[iHealthyChild]->pByteData = 1;

					pRaidInfo->RaidStatus = RAID1_STATUS_BITMAPPING;

					KDPrintM(DBG_LURN_ERROR,("RAID1_STATUS_FAIL_DETECTED : %x, %d, %d,\n",
						pExtendedCommands[iHealthyChild]->logicalBlockAddress,
						pExtendedCommands[iHealthyChild]->Offset,
						pExtendedCommands[iHealthyChild]->LengthByte));
				}
*/
				// fall through
				pRaidInfo->RaidStatus = RAID1_STATUS_BITMAPPING;					
			case RAID1_STATUS_BITMAPPING: // bitmap work
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID1_STATUS_BITMAPPING\n"));

					// use pRaidInfo->ExtendedComandForBitmap instead allocating
					// seek first sector in bitmap
					uiBitmapStartInBits = logicalBlockAddress / 
						pRaidInfo->Children[iChildToRecordBitmap].InfoRaid1.SectorsPerBit;
					uiBitmapEndInBits = (logicalBlockAddress + transferBlocks -1) /
						pRaidInfo->Children[iChildToRecordBitmap].InfoRaid1.SectorsPerBit;

					// check if any bits would be changed
					if(!RtlAreBitsSet(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits))
					{
						// bitmap work
						KDPrintM(DBG_LURN_TRACE,("RAID1_STATUS_BITMAPPING bitmap changed\n"));
						RtlSetBits(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits - uiBitmapStartInBits +1);
						pRaidInfo->ExtendedComandForBitmap.pByteData = 
							(PCHAR)pRaidInfo->Bitmap->Buffer + (uiBitmapStartInBits / BITS_PER_BLOCK) * BLOCK_SIZE;
						pRaidInfo->ExtendedComandForBitmap.LengthBlock = 
							(uiBitmapStartInBits / BITS_PER_BLOCK == uiBitmapEndInBits) ? 1 : 2;
						pRaidInfo->ExtendedComandForBitmap.logicalBlockAddress = 
							(ULONG)pRaidInfo->Children[iChildToRecordBitmap].InfoRaid1.SectorBitmapStart +
							(uiBitmapStartInBits / BITS_PER_BLOCK);
						pExtendedCommands[iChildToRecordBitmap] = &pRaidInfo->ExtendedComandForBitmap;
					}

					// split is super ultra hyper rare case
					if(pRaidInfo->ExtendedComandForBitmap.LengthByte == BLOCK_SIZE * 2)
					{
						KDPrintM(DBG_LURN_ERROR,("RAID1_STATUS_BITMAPPING bitmap changed DOUBLE : This is not error but ultra rare case\n"));
						ASSERT(FALSE);
					}
				}
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
			if(RAID1_STATUS_NORMAL != pRaidInfo->RaidStatus)
			{
				// send to the healthy child LURN
				status = LurnAssocSendCcbToChildrenArray(
														(PLURELATION_NODE *)&Lurn->LurnChildren[iChildToRecordBitmap],
														1,
														Ccb,
														LurnRAID1CcbCompletion,
														NULL,
														&pExtendedCommands[iChildToRecordBitmap]
										);
			}
			else
			{
				//
				//	send to all child LURNs.
				//
				status = LurnAssocSendCcbToAllChildren(
														Lurn,
														Ccb,
														LurnRAID1CcbCompletion,
														NULL,
														pExtendedCommands
										);
			}
		}
	break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ: {
		LONG				idx_child;
		PLURELATION_NODE	ChildLurn;
		KIRQL				oldIrql;
		//
		//	Find a child LURN to run.
		//
		// lock buffer : release at completion
		while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);

		idx_child = 0;
		ASSERT(Lurn->LurnChildrenCnt == DISKS_IN_RAID1_PAIR);
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
			NULL
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
												NULL
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
	case CCB_OPCODE_NOOP: {
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
			break;
		}
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID1CcbCompletion, NULL, NULL);
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
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL); // use same function as Mirror
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
//	RAID4 Lurn
//

//	
//	AING_TO_DO :
//	 Create LurnRAID4UpdateCcbCompletion so that miniport read & check bitmap status, disconnection status
//	 Remove unused commented sources
//	 Remove LanscsiQueryStatus call route
//	 RAID4 -> RAID0 duplicate codes
//	 CDB10_LOGICAL_BLOCK_BYTE, CDB10_TRANSFER_BLOCKS and correspond macros
//

NTSTATUS
LurnRAID4Initialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID4_INFO pRaidInfo = NULL;
	ULONG ulBitMapSize, ulDataBufferSize, ulDataBufferSizePerDisk;
	NTSTATUS ntStatus;
	ULONG i;
	PRAID4_CHILD_INFO pChildInfo;

	UNREFERENCED_PARAMETER(LurnDesc);

//	Raid Information
	Lurn->LurnRAID4Info = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID4_INFO), 
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRAID4Info)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRAID4Info;

	RtlZeroMemory(pRaidInfo, sizeof(RAID4_INFO));

	pRaidInfo->RaidStatus = RAID4_STATUS_NORMAL;

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
	pRaidInfo->Children = ExAllocatePoolWithTag(NonPagedPool, 
		Lurn->LurnChildrenCnt * sizeof(RAID4_CHILD_INFO), RAID_INFO_POOL_TAG);
	if(NULL == pRaidInfo->Children)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}
	RtlZeroMemory(pRaidInfo->Children, Lurn->LurnChildrenCnt * sizeof(RAID4_CHILD_INFO));

	for(i = 0; i < (ULONG)Lurn->LurnChildrenCnt; i++)
	{
		pChildInfo = &pRaidInfo->Children[i];

		// 3 disks : each 64 * 512B = RAID0_DATA_BUFFER_LENGTH / (3 - 1)
		// 5 disks : each 32 * 512B = RAID0_DATA_BUFFER_LENGTH / (5 - 1)
		// 9 disks : each 16 * 512B = RAID0_DATA_BUFFER_LENGTH / (9 - 1)
		pChildInfo->DataBuffer =
			(PCHAR)pRaidInfo->DataBufferAllocated + i * ulDataBufferSizePerDisk;

		KDPrintM(DBG_LURN_ERROR, ("pRaidInfo->Children[%d]->DataBuffer = %x\n", i, pRaidInfo->Children[i].DataBuffer));

		RtlCopyMemory(&pChildInfo->InfoRaid4, &LurnDesc->LurnInfoRAID4[i], sizeof(INFO_RAID4));

		pChildInfo->ExtendedCommandForTag.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
		pChildInfo->ExtendedCommandForTag.Operation = CCB_EXT_WRITE;
		pChildInfo->ExtendedCommandForTag.logicalBlockAddress = (ULONG)pRaidInfo->Children[i].InfoRaid4.SectorLastWrittenInfo;
		pChildInfo->ExtendedCommandForTag.Offset = 0;
		pChildInfo->ExtendedCommandForTag.LengthByte = sizeof(CMD_BYTE_LAST_WRITTEN_SECTOR);
		pChildInfo->ExtendedCommandForTag.ByteOperation = BYTE_OPERATION_COPY;
		pChildInfo->ExtendedCommandForTag.pNextCmd = NULL; // vari
		pChildInfo->ExtendedCommandForTag.pByteData = (PBYTE)&pChildInfo->ByteLastWrittenSector;
		pChildInfo->ByteLastWrittenSector.timeStamp = 0;
	}

/*
	// ExtendedCommandForFlag
	pRaidInfo->ExtendedCommandForFlag.pLurnCreated = Lurn;
	pRaidInfo->ExtendedCommandForFlag.Operation = CCB_EXT_READ_OPERATE_WRITE;
//	pRaidInfo->ExtendedCommandForFlag.logicalBlockAddress = (ULONG)pRaidInfo->Children[i]->InfoRaid4.SectorLastWrittenInfo;
	pRaidInfo->ExtendedCommandForFlag.Offset = (UINT16)pRaidInfo->Children[0].InfoRaid4.OffsetFlagInfo;
	pRaidInfo->ExtendedCommandForFlag.LengthByte = 1;
	pRaidInfo->ExtendedCommandForFlag.ByteOperation = BYTE_OPERATION_COPY;
	pRaidInfo->ExtendedCommandForFlag.Byte[0] = 1;
	pRaidInfo->ExtendedCommandForFlag.pNextCmd = NULL; // vari
*/

	// ExtendedComandForBitmap
	pRaidInfo->ExtendedComandForBitmap.pLurnCreated = NULL; // Lurn; // not to deleted at completion routine
	pRaidInfo->ExtendedComandForBitmap.Operation = CCB_EXT_WRITE;
//	pRaidInfo->ExtendedComandForBitmap.logicalBlockAddress = (ULONG)pRaidInfo->Children[i]->InfoRaid4.SectorLastWrittenInfo;
	pRaidInfo->ExtendedComandForBitmap.Offset = 0;
//	pRaidInfo->ExtendedComandForBitmap.LengthByte = 512; // 512 or 1024
	pRaidInfo->ExtendedComandForBitmap.ByteOperation = BLOCK_OPERATION_WRITE;
//	pRaidInfo->ExtendedComandForBitmap.pByteData = (BYTE *)&pRaidInfo->ExtendedComandForBitmap + sizeof(CMD_BYTE_OP);
	pRaidInfo->ExtendedComandForBitmap.pNextCmd = NULL; // vari

//	Bitmap (1) * (bitmap structure size + bitmap size)
	ulBitMapSize = (UINT32)(sizeof(BYTE) * (Lurn->UnitBlocks / 
		(pRaidInfo->Children[0].InfoRaid4.SectorsPerBit * 8))) +1;
	
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

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRAID4Info)
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
			if(pRaidInfo->Children)
			{
				ExFreePoolWithTag(pRaidInfo->Children, RAID_INFO_POOL_TAG);
				pRaidInfo->Children = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnRAID4Info, RAID_INFO_POOL_TAG);
			Lurn->LurnRAID4Info = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnRAID4Destroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID4_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRAID4Info);

	pRaidInfo = Lurn->LurnRAID4Info;
	ASSERT(pRaidInfo);

	ASSERT(pRaidInfo->DataBufferAllocated);
	ExFreePoolWithTag(pRaidInfo->DataBufferAllocated, RAID_DATA_BUFFER_POOL_TAG) ;
	pRaidInfo->DataBufferAllocated = NULL;
	
	ASSERT(pRaidInfo->Bitmap);
	ExFreePoolWithTag(pRaidInfo->Bitmap, BITMAP_POOL_TAG) ;
	pRaidInfo->Bitmap = NULL;

	ASSERT(pRaidInfo->Children);
	ExFreePoolWithTag(pRaidInfo->Children, RAID_INFO_POOL_TAG);
	pRaidInfo->Children = NULL;

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
	PRAID4_INFO pRaidInfo;
	PLURELATION_NODE pLurnOriginal;

	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	pLurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	ASSERT(pLurnOriginal);
	pRaidInfo = pLurnOriginal->LurnRAID4Info;

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
*	If RaidStatus != RAID4_STATUS_NORMAL & 1 or more Ccb fails, OriginalCcb->CcbStatus = CCB_STATUS_STOP
*/
			PLURELATION_NODE pLurnChildDefected;
			int i;

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

				ASSERT(pLurnChildDefected->LurnRAID4Info);

				// 1 fail + 1 fail = broken
				if(RAID4_STATUS_NORMAL != pRaidInfo->RaidStatus)
				{
					if(pLurnChildDefected != pLurnOriginal->LurnChildren[pRaidInfo->iDefectedChild])
					{
						ASSERT(FALSE);
//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					}
					break;
				}
				
				pRaidInfo->RaidStatus = RAID4_STATUS_FAIL_DETECTED;

				for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
				{
					if(pLurnChildDefected == pLurnOriginal->LurnChildren[i])
					{
						pRaidInfo->iDefectedChild = i;
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

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iDefectedChild = %d\n", pRaidInfo->iDefectedChild));
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
		KDPrintM(DBG_LURN_ERROR, ("ERROR: Ccb->CcbStatus= %x, AssociateID = %d, OriginalCcb->AssociateCount = %d\n",
								(int)Ccb->CcbStatus, (unsigned int)Ccb->AssociateID, OriginalCcb->AssociateCount));

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

	LSCcbSetStatusFlag(	OriginalCcb,
						Ccb->CcbStatusFlags
		);
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

	if(pRaidInfo->RaidStatus != RAID4_STATUS_NORMAL)
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
			// deshuffle read buffer before release buffer lock
			{
				int BlocksPerDisk;
				int i, j;
				register int k;
				PULONG pDataBufferToRecover, pDataBufferSrc;
				int	bDataBufferToRecoverInitialized;

				KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

				// create new data buffer and encrypt here.
				// new data buffer will be deleted at completion routine
				BlocksPerDisk = Ccb->DataBufferLength / BLOCK_SIZE;

				// if non-parity disk is corrupted, generate data with parity work
				if(pLurnOriginal->LurnRAID4Info->RaidStatus != RAID4_STATUS_NORMAL &&
					pLurnOriginal->LurnRAID4Info->iDefectedChild != pLurnOriginal->LurnChildrenCnt -1) // not parity
				{
					bDataBufferToRecoverInitialized = FALSE;
					
					for(i = 0; i < pLurnOriginal->LurnChildrenCnt; i++)
					{
						// skip defected
						if(pLurnOriginal->LurnRAID4Info->iDefectedChild == i)
							continue;

						pDataBufferSrc = (PULONG)pRaidInfo->Children[i].DataBuffer;
						pDataBufferToRecover = (PULONG)pRaidInfo->Children[pLurnOriginal->LurnRAID4Info->iDefectedChild].DataBuffer;

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

				// deshuffle, exclude parity
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

	PRAID4_INFO pRaidInfo;
	LONG iChildToRecordBitmap;
	PCMD_BYTE_OP	pExtendedCommands[MAX_DISKS_IN_RAID4];
	PLURELATION_NODE	LurnChildren[MAX_DISKS_IN_RAID4];

	RtlZeroMemory(pExtendedCommands, MAX_DISKS_IN_RAID4 * sizeof(PCMD_BYTE_OP));

	pRaidInfo = Lurn->LurnRAID4Info;

	// record a bitmap information to the next disk of the defected disk
	// pExtendedCommands itself will go into LurnAssocSendCcbToChildrenArray
	ASSERT(Lurn->LurnChildrenCnt > 0 && Lurn->LurnChildrenCnt <= MAX_DISKS_IN_RAID4);
	if(RAID4_STATUS_NORMAL != pRaidInfo->RaidStatus)
	{
		ASSERT(pRaidInfo->iDefectedChild < (UINT32)Lurn->LurnChildrenCnt);
		iChildToRecordBitmap = (pRaidInfo->iDefectedChild == Lurn->LurnChildrenCnt -1) ? 0 : pRaidInfo->iDefectedChild +1;
	}

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		// lock buffer : release at completion / do not forget to release when fail
		while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		// non defection
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;

			int DataBufferLengthPerDisk;
			int BlocksPerDisk;
			register int k;
			register int i, j;
			PULONG pDataBufferParity, pDataBufferSrc;
			CUSTOM_DATA_BUFFER cdb;
			int	bDataBufferParityInitialized;

			logicalBlockAddress = CDB10_LOGICAL_BLOCK_BYTE(Ccb->Cdb);
			transferBlocks = CDB10_TRANSFER_BLOCKS(Ccb->Cdb);

			ASSERT(transferBlocks <= 128 * 64);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID4_STATUS_NORMAL:
				// record last written sector
				{
					// AING_TO_DO : use Ready made pool
					for(i = 0; i < Lurn->LurnChildrenCnt; i++)
					{
						pExtendedCommands[i] = &pRaidInfo->Children[i].ExtendedCommandForTag;
						pExtendedCommands[i]->pNextCmd = NULL;

						pRaidInfo->Children[i].ByteLastWrittenSector.logicalBlockAddress = (UINT64)logicalBlockAddress;
						pRaidInfo->Children[i].ByteLastWrittenSector.transferBlocks = (UINT32)transferBlocks;
						pRaidInfo->Children[i].ByteLastWrittenSector.timeStamp++;
					}
				}
				break;
			case RAID4_STATUS_FAIL_DETECTED: // dirty flag
/*
				{
					// AING_TO_DO : use Ready made pool
					pExtendedCommands[iChildToRecordBitmap] = &pRaidInfo->ExtendedCommandForFlag;

					pExtendedCommands[iChildToRecordBitmap]->pLurnCreated = Lurn;
					pExtendedCommands[iChildToRecordBitmap]->Operation = CCB_EXT_READ_OPERATE_WRITE;
					pExtendedCommands[iChildToRecordBitmap]->logicalBlockAddress = (UINT32)pRaidInfo->Children[iChildToRecordBitmap].InfoRaid4.SectorInfo;
					pExtendedCommands[iChildToRecordBitmap]->Offset = (UINT16)pRaidInfo->Children[iChildToRecordBitmap].InfoRaid4.OffsetFlagInfo;
					pExtendedCommands[iChildToRecordBitmap]->LengthByte = 1;
					pExtendedCommands[iChildToRecordBitmap]->ByteOperation = BYTE_OPERATION_COPY;
					pExtendedCommands[iChildToRecordBitmap]->pNextCmd = NULL;
					*pExtendedCommands[iChildToRecordBitmap]->pByteData = 1;

				}
				// fall through
*/
				pRaidInfo->RaidStatus = RAID4_STATUS_BITMAPPING;					
			case RAID4_STATUS_BITMAPPING:
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID4_STATUS_BITMAPPING\n"));
					
					// use pRaidInfo->ExtendedComandForBitmap instead allocating
					// seek first sector in bitmap
					uiBitmapStartInBits = logicalBlockAddress / 
						pRaidInfo->Children[iChildToRecordBitmap].InfoRaid4.SectorsPerBit;
					uiBitmapEndInBits = (logicalBlockAddress + transferBlocks -1) /
						pRaidInfo->Children[iChildToRecordBitmap].InfoRaid4.SectorsPerBit;

					// check if any bits would be changed
					if(!RtlAreBitsSet(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits))
					{
						// bitmap work
						KDPrintM(DBG_LURN_TRACE,("RAID4_STATUS_BITMAPPING bitmap changed\n"));
						RtlSetBits(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits - uiBitmapStartInBits +1);
						pRaidInfo->ExtendedComandForBitmap.pByteData = 
							(PCHAR)pRaidInfo->Bitmap->Buffer + (uiBitmapStartInBits / BITS_PER_BLOCK) * BLOCK_SIZE;
						pRaidInfo->ExtendedComandForBitmap.LengthBlock = 
							(uiBitmapStartInBits / BITS_PER_BLOCK == uiBitmapEndInBits) ? 1 : 2;
						pRaidInfo->ExtendedComandForBitmap.logicalBlockAddress = 
							(ULONG)pRaidInfo->Children[iChildToRecordBitmap].InfoRaid4.SectorBitmapStart +
							(uiBitmapStartInBits / BITS_PER_BLOCK);
						pExtendedCommands[iChildToRecordBitmap] = &pRaidInfo->ExtendedComandForBitmap;
					}

					// split is super ultra hyper rare case
					if(pRaidInfo->ExtendedComandForBitmap.LengthByte == BLOCK_SIZE * 2)
					{
						KDPrintM(DBG_LURN_ERROR,("RAID4_STATUS_BITMAPPING bitmap changed DOUBLE : This is not error but ultra rare case\n"));
						ASSERT(FALSE);
					}
				}
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
			
//					int	bDataBufferParityInitialized;
			// initialize parity
//			RtlZeroMemory(pRaidInfo->Children[Lurn->LurnChildrenCnt -1].DataBuffer, BlocksPerDisk * BLOCK_SIZE);
			// shuffle
			bDataBufferParityInitialized = FALSE;
			for(i = 0; i < Lurn->LurnChildrenCnt -1; i++)
			{
				// even if defected, copy to build parity
//				if(pRaidInfo->iDefectedChild == i)
//					continue;

				pDataBufferSrc = (PULONG)pRaidInfo->Children[i].DataBuffer;

				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory(
						(PCHAR)pDataBufferSrc + j * BLOCK_SIZE,
						(PCHAR)Ccb->DataBuffer + (i + j * (Lurn->LurnChildrenCnt -1)) * BLOCK_SIZE,
						BLOCK_SIZE);
				}

				// check if parity disk is broken
				if(RAID4_STATUS_NORMAL != pRaidInfo->RaidStatus &&
					pRaidInfo->iDefectedChild == Lurn->LurnChildrenCnt -1)
					continue; // do not make parity

				pDataBufferParity = (PULONG)pRaidInfo->Children[Lurn->LurnChildrenCnt -1].DataBuffer;

				// for performance, copy first disk to parity disk
				if(FALSE == bDataBufferParityInitialized)
				{
					RtlCopyMemory(
						pDataBufferParity,
						pDataBufferSrc,
						BlocksPerDisk * BLOCK_SIZE);
					bDataBufferParityInitialized = TRUE;
					continue;
				}

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
//				if(RAID4_STATUS_NORMAL != pRaidInfo->RaidStatus &&
//					pRaidInfo->iDefectedChild == i)
//					continue;
				
				cdb.DataBuffer[j] = pRaidInfo->Children[i].DataBuffer;
				cdb.DataBufferLength[j] = (UINT32)DataBufferLengthPerDisk;
				LurnChildren[j] = Lurn->LurnChildren[i];
				j++;
			}
			
			cdb.DataBufferCount = j;
			
//			status = LurnAssocSendCcbToChildrenArray(
//				LurnChildren,
//				cdb.DataBufferCount,
//				Ccb,
//				LurnRAID4CcbCompletion,
//				&cdb,
//				pExtendedCommands
//				);
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				&cdb,
				pExtendedCommands
				);
		}
		break;

	case SCSIOP_VERIFY:
		// non defection
		while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
			InterlockedDecrement(&pRaidInfo->LockDataBuffer);
		{
/*
			int i, j;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_VERIFY\n"));
			// lock buffer : release at completion
			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);
			for(i = 0, j = 0; i < Lurn->LurnChildrenCnt; i++)
			{
				if(RAID4_STATUS_NORMAL != pRaidInfo->RaidStatus &&
					i == pRaidInfo->iDefectedChild)
					continue;
				LurnChildren[j] = Lurn->LurnChildren[i];
				j++;
			}

			status = LurnAssocSendCcbToChildrenArray(
				LurnChildren,
				j,
				Ccb,
				LurnRAID4CcbCompletion,
				NULL,
				NULL
				);
*/
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				NULL,
				NULL
				);
		}
		break;

	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
		{
			int DataBufferLengthPerDisk;
			int i, j;
			CUSTOM_DATA_BUFFER cdb;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

			// lock buffer : release at completion
			while(1 != InterlockedIncrement(&pRaidInfo->LockDataBuffer))
				InterlockedDecrement(&pRaidInfo->LockDataBuffer);

			DataBufferLengthPerDisk = Ccb->DataBufferLength / (Lurn->LurnChildrenCnt -1);
			KDPrintM(DBG_LURN_NOISE,("R Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			for(i = 0, j = 0; i < Lurn->LurnChildrenCnt; i++)
			{
//				if(RAID4_STATUS_NORMAL != pRaidInfo->RaidStatus &&
//					i == pRaidInfo->iDefectedChild)
//					continue;
				LurnChildren[j] = Lurn->LurnChildren[i];
				cdb.DataBuffer[j] = pRaidInfo->Children[i].DataBuffer;
				cdb.DataBufferLength[j] = (UINT32)DataBufferLengthPerDisk;
				j++;
			}

			cdb.DataBufferCount = j;
/*
			status = LurnAssocSendCcbToChildrenArray(
				LurnChildren,
				cdb.DataBufferCount,
				Ccb,
				LurnRAID4CcbCompletion,
				&cdb,
				NULL
				);
*/
			status = LurnAssocSendCcbToAllChildren(
				Lurn,
				Ccb,
				LurnRAID4CcbCompletion,
				&cdb,
				NULL
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
				NULL
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

	if(Lurn->LurnRAID4Info->RaidStatus != RAID4_STATUS_NORMAL)
	{
		if(Ccb->OperationCode == CCB_OPCODE_EXECUTE)
		{
			KDPrintM(DBG_LURN_ERROR, ("Execute : %X\n", (int)Ccb->Cdb[0]));
		}
		else
			KDPrintM(DBG_LURN_ERROR, ("Ccb->OperationCode = %X\n", Ccb->OperationCode));

	}

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
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID4CcbCompletion, NULL, NULL);
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
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL, NULL); // use same function as Mirror
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
