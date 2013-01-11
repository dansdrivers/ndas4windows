#include <ntddk.h>
//#include "public.h"
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnAssoc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSLurnAssoc"

NTSTATUS
LurnMirrV2ReadCcbCompletion(
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

//
//	mirroring V2 interface
//
NTSTATUS
LurnInitializeMirrV2(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
LurnMirrV2Request(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

NTSTATUS
LurnDestroyMirrV2(
		PLURELATION_NODE Lurn
	) ;

LURN_INTERFACE LurnMirrorV2Interface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_MIRRORING_V2,
					0,
					{
						LurnInitializeMirrV2,
						LurnDestroyMirrV2,
						LurnMirrV2Request
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

		LSCcbInitializeByCcb(Ccb, pLurnChildren[idx_child], NextCcb[idx_child]);
		NextCcb[idx_child]->AssociateID = (USHORT)idx_child;
		LSCcbSetFlag(NextCcb[idx_child], CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
		LSCcbSetFlag(NextCcb[idx_child], Ccb->Flags&CCB_FLAG_SYNCHRONOUS);
		LSCcbSetCompletionRoutine(NextCcb[idx_child], CcbCompletion, Ccb);

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
		IN PVOID					*apExtendedCmd
){
	return LurnAssocSendCcbToChildrenArray((PLURELATION_NODE *)Lurn->LurnChildren, Lurn->LurnChildrenCnt, Ccb, CcbCompletion, apExtendedCmd);
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
		ULONG				startBlockAddress, endBlockAddress;
		USHORT				transferBlocks;
		ASSERT(Ccb->CdbLength <= MAXIMUM_CDB_SIZE);

		startBlockAddress = ((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte3 |
							((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte2 << 8 |
							((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte1 << 16 |
							((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte0 << 24;

		transferBlocks = (USHORT)(((PCDB)Ccb->Cdb)->CDB10.TransferBlocksMsb << 8 |
								((PCDB)Ccb->Cdb)->CDB10.TransferBlocksLsb );

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
			LSCcbInitializeByCcb(Ccb, Lurn->LurnChildren[idx_child], NextCcb);
			LSCcbSetFlag(NextCcb, CCB_FLAG_ASSOCIATE|CCB_FLAG_ALLOCATED);
			LSCcbSetFlag(NextCcb, Ccb->Flags&CCB_FLAG_SYNCHRONOUS);
			NextCcb->AssociateID = (USHORT)idx_child;
			LSCcbSetCompletionRoutine(NextCcb, LurnAggrCcbCompletion, Ccb);

			// start address
			startBlockAddress -= ChildLurn->StartBlockAddr;
			pCdb = (PCDB)&NextCcb->Cdb[0];
			pCdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&startBlockAddress)->Byte3;
			pCdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&startBlockAddress)->Byte2;
			pCdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&startBlockAddress)->Byte1;
			pCdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&startBlockAddress)->Byte0;

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
			ULONG		firstStartBlockAddress;
			USHORT		firstTransferBlocks;
			USHORT		secondTransferBlocks;
			PCDB		pCdb;
			LONG		idx_ccb;

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

			// start address
			firstStartBlockAddress = startBlockAddress - ChildLurn->StartBlockAddr;
			pCdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&firstStartBlockAddress)->Byte3;
			pCdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&firstStartBlockAddress)->Byte2;
			pCdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&firstStartBlockAddress)->Byte1;
			pCdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&firstStartBlockAddress)->Byte0;
			// transfer length
			firstTransferBlocks = (USHORT)(ChildLurn->EndBlockAddr - startBlockAddress + 1);
			pCdb->CDB10.TransferBlocksMsb = ((PTWO_BYTE)&firstTransferBlocks)->Byte1;
			pCdb->CDB10.TransferBlocksLsb = ((PTWO_BYTE)&firstTransferBlocks)->Byte0;
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
			pCdb->CDB10.LogicalBlockByte0 = 0;
			pCdb->CDB10.LogicalBlockByte1 = 0;
			pCdb->CDB10.LogicalBlockByte2 = 0;
			pCdb->CDB10.LogicalBlockByte3 = 0;
			// transfer length
			secondTransferBlocks = transferBlocks - firstTransferBlocks;
			ASSERT(secondTransferBlocks > 0);
			pCdb->CDB10.TransferBlocksMsb = ((PTWO_BYTE)&secondTransferBlocks)->Byte1;
			pCdb->CDB10.TransferBlocksLsb = ((PTWO_BYTE)&secondTransferBlocks)->Byte0;
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
			VENDOR_ID,
			(strlen(VENDOR_ID)+1) < 8 ? (strlen(VENDOR_ID)+1) : 8
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

				if(Lurn->Lur->LurFlags & LUR_FLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;

			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1;
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->NumberOfBlocks[0] = (BYTE)(BlockCount>>16);
			parameterBlock->NumberOfBlocks[1] = (BYTE)(BlockCount>>8);
			parameterBlock->NumberOfBlocks[2] = (BYTE)(BlockCount);

			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LSCcbCompleteCcb(Ccb);
		} else {

			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
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
		LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnAggrCcbCompletion, NULL);
		break;
	}
	case CCB_OPCODE_QUERY: {
		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_QUERY\n"));
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

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
		break;
	}

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
			VENDOR_ID,
			(strlen(VENDOR_ID)+1) < 8 ? (strlen(VENDOR_ID)+1) : 8
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
//		MpDebugPrint(1,("logicalBlockAddress = %x\n", logicalBlockAddress));
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
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
	
				if(Lurn->Lur->LurFlags & LUR_FLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1;
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(BLOCK_SIZE>>16);
			parameterBlock->BlockLength[1] = (BYTE)(BLOCK_SIZE>>8);
			parameterBlock->BlockLength[2] = (BYTE)(BLOCK_SIZE);

			Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		} else {
			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
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
		LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnFaultTolerantCcbCompletion, NULL);
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
		LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL);
		break;
	}

	case CCB_OPCODE_QUERY: {
		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_QUERY\n"));
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

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
		break;
	}

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
//	Mirroring V2 Lurn
//

NTSTATUS
LurnInitializeMirrV2(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{
	PRAID_01_INFO pRaidInfo = NULL;
	ULONG ulBitMapSize;
	NTSTATUS ntStatus;

	// initialize raid information
	Lurn->LurnExtension = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID_01_INFO), RAID_01_INFO_POOL_TAG);
	if(NULL == Lurn->LurnExtension)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnExtension;

	pRaidInfo->RaidType = 1;
	pRaidInfo->RaidStatus = RAID_STATUS_NORMAL;
	RtlCopyMemory(pRaidInfo->ChildrenInfo, LurnDesc->LurnInfoRAID_1, sizeof(INFO_RAID_1) * 2);

	// set bitmap

	ulBitMapSize = (UINT32)(sizeof(BYTE) * (Lurn->UnitBlocks / (pRaidInfo->ChildrenInfo[0].SectorsPerBit * 8))) +1;
	ulBitMapSize += sizeof(RTL_BITMAP);
	
	pRaidInfo->Bitmap = (PRTL_BITMAP)ExAllocatePoolWithTag(NonPagedPool, ulBitMapSize, RAID_BITMAP_POOL_TAG);
	if(NULL == pRaidInfo->Bitmap)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	RtlInitializeBitMap(
		pRaidInfo->Bitmap,
		(PULONG)(pRaidInfo->Bitmap +1),
		ulBitMapSize * 8);

    RtlClearAllBits(pRaidInfo->Bitmap);
	KeInitializeSpinLock(&pRaidInfo->BitmapSpinLock);

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnExtension)
		{
			if(pRaidInfo->Bitmap)
			{
				ExFreePoolWithTag(pRaidInfo->Bitmap, RAID_BITMAP_POOL_TAG);
				pRaidInfo->Bitmap = NULL;
			}
			ExFreePoolWithTag(Lurn->LurnExtension, RAID_01_INFO_POOL_TAG);
			Lurn->LurnExtension = NULL;
		}
	}
	

	return ntStatus;
}

NTSTATUS
LurnDestroyMirrV2(
		PLURELATION_NODE Lurn
	) 
{
	PRAID_01_INFO pRaidInfo;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnExtension);

	pRaidInfo = (PRAID_01_INFO)Lurn->LurnExtension;

	if(NULL != pRaidInfo->Bitmap)
		ExFreePoolWithTag(pRaidInfo->Bitmap, RAID_BITMAP_POOL_TAG) ;

	ExFreePoolWithTag(pRaidInfo, RAID_01_INFO_POOL_TAG) ;

	return STATUS_SUCCESS ;
}

NTSTATUS
LurnMirrV2ReadCcbCompletion(
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
LurnFaultTolerantCcbCompletionV2(
		IN PCCB	Ccb,
		IN PCCB	OriginalCcb
	) {
	KIRQL	oldIrql;
	LONG	ass;
	NTSTATUS status;

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
*	. Set raid status(OriginalCcb, pRaidInfo->RaidStatus) to RAID_STATUS_FAIL_DETECTED
*	. Set IDE Lurn status(Ccb) to 'accept bitmapped' write only
*	. enumerate all write data & set busy if the data does not have bitmap
*	. release lock
*
*	Process after a child stop
*	. lock & check bitmap
*	. if RAID_STATUS_FAIL_DETECTED, create extended command
*	. if bitmap changed, create extended command to write bitmap at non-user sector
*	. LurnIdeDiskExecute will process extended command
*	. release lock
*/
			PLURELATION_NODE pLurnDefected;
			PLURELATION_NODE pLurnHealthy;
			PLURELATION_NODE pLurnMirrV2;
			PRAID_01_INFO pRaidInfo;
			LONG iHealthyChild;

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
				pLurnDefected = Ccb->CcbCurrentStackLocation->Lurn;

				ASSERT(OriginalCcb->CcbCurrentStackLocation);
				ASSERT(OriginalCcb->CcbCurrentStackLocation->Lurn);
				pLurnMirrV2 = OriginalCcb->CcbCurrentStackLocation->Lurn;

				ASSERT(LURN_IDE_DISK == pLurnDefected->LurnType);
				ASSERT(LURN_MIRRORING_V2 == pLurnMirrV2->LurnType);

				if(!pLurnDefected || !pLurnMirrV2)
				{
					status = STATUS_ILLEGAL_INSTRUCTION;
					break;
				}

				ASSERT(pLurnDefected->LurnExtension);
				pRaidInfo = (PRAID_01_INFO)pLurnMirrV2->LurnExtension;

				if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
				{
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					break;
				}
				
				pRaidInfo->RaidStatus = RAID_STATUS_FAIL_DETECTED;

				if(pLurnDefected == pLurnMirrV2->LurnChildren[0])
				{
					iHealthyChild = 1;
					pRaidInfo->iDefectedChild = 0;
					
				}
				else if(pLurnDefected == pLurnMirrV2->LurnChildren[1])
				{
					iHealthyChild = 0;
					pRaidInfo->iDefectedChild = 1;
				}
				else
				{
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					break;
				}

				pLurnHealthy = pLurnMirrV2->LurnChildren[iHealthyChild];

			}

			// set CCB with CCB_OPCODE_SETBUSY
			{
				PCCB CcbForBusy;

				status = LSCcbAllocate(&CcbForBusy);
				if(!NT_SUCCESS(status))
					break;

				LSCcbInitializeByCcb(OriginalCcb, pLurnHealthy, CcbForBusy);
				LSCcbSetFlag(CcbForBusy, CCB_FLAG_ALLOCATED);
				CcbForBusy->OperationCode = CCB_OPCODE_SETBUSY;
				
				status = LurnRequest(pLurnHealthy, CcbForBusy);

				if(!NT_SUCCESS(status))
				{
					DbgBreakPoint();
				}
			}
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
			//	Both children stopped.!
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
		return status;
	}
	
	// clear extended command
	LSCcbRemoveExtendedCommandTailMatch(&(Ccb->pExtendedCommand), OriginalCcb->CcbCurrentStackLocation->Lurn);

	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnMirrV2Execute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) {
	NTSTATUS			status;

	PRAID_01_INFO pRaidInfo;
	LONG iHealthyChild;
	PCMD_BYTE_OP	pExtendedCommands[2];

	// initialize extended commands
	pExtendedCommands[0] = NULL;
	pExtendedCommands[1] = NULL;

	pRaidInfo = (PRAID_01_INFO)Lurn->LurnExtension;

	if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
	{
		if(0 == pRaidInfo->iDefectedChild)
			iHealthyChild = 1;
		else if(1 == pRaidInfo->iDefectedChild)
			iHealthyChild = 0;
		else
			return STATUS_ILLEGAL_INSTRUCTION;
	}


	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
		{
			UINT32 logicalBlockAddress;
			UINT16 transferBlocks;
			UINT32 timeStamp;

			KDPrintM(DBG_LURN_TRACE, ("SCSIOP_WRITE\n"));

			logicalBlockAddress = 
				((UINT32)(((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte0) << (8 * 3)) +
				((UINT32)(((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte1) << (8 * 2)) +
				((UINT32)(((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte2) << (8 * 1)) +
				((UINT32)(((PCDB)Ccb->Cdb)->CDB10.LogicalBlockByte3) << (8 * 0));

			transferBlocks =
				((UINT32)(((PCDB)Ccb->Cdb)->CDB10.TransferBlocksMsb) << (8 * 1)) +
				((UINT32)(((PCDB)Ccb->Cdb)->CDB10.TransferBlocksLsb) << (8 * 0));

			timeStamp = 0;

			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_NORMAL: // write information where writing on.
				{
					UINT16 bytelength = 8 /* start address*/ + 4 /* length */ + 4 /* stamp */;
					int i;
					
					pExtendedCommands[0] = (PCMD_BYTE_OP)ExAllocatePoolWithTag(NonPagedPool, 
						(sizeof(CMD_BYTE_OP) -1 + bytelength), EXTENDED_COMMAND_POOL_TAG);

					if(!pExtendedCommands[0])
						return STATUS_INSUFFICIENT_RESOURCES;

					pExtendedCommands[1] = (PCMD_BYTE_OP)ExAllocatePoolWithTag(NonPagedPool, 
						(sizeof(CMD_BYTE_OP) -1 + bytelength), EXTENDED_COMMAND_POOL_TAG);
					if(!pExtendedCommands[1])
					{
						ExFreePoolWithTag(pExtendedCommands[0], EXTENDED_COMMAND_POOL_TAG);
						return STATUS_INSUFFICIENT_RESOURCES;
					}
					
					for(i = 0; i < 2; i++)
					{
						pExtendedCommands[i]->pLurnCreated = Lurn;
						pExtendedCommands[i]->Operation = CCB_EXT_BYTE_WRITE;
						pExtendedCommands[i]->logicalBlockAddress = (ULONG)pRaidInfo->ChildrenInfo[i].SectorLastWrittenInfo;
						pExtendedCommands[i]->Offset = 0;
						pExtendedCommands[i]->LengthByte = bytelength;
						pExtendedCommands[i]->ByteOperation = BYTE_OPERATION_COPY;
						pExtendedCommands[i]->pNextCmd = NULL;

						*(PUINT64)&pExtendedCommands[i]->Byte[0] = (UINT64)logicalBlockAddress;
						*(PUINT32)&pExtendedCommands[i]->Byte[8] = (UINT32)transferBlocks;
						*(PUINT32)&pExtendedCommands[i]->Byte[12] = timeStamp;
					}

				}
				break;
			case RAID_STATUS_FAIL_DETECTED: // dirty flag
				{

					KDPrintM(DBG_LURN_ERROR,("RAID_STATUS_FAIL_DETECTED\n"));

					pExtendedCommands[iHealthyChild] = (PCMD_BYTE_OP)ExAllocatePoolWithTag(NonPagedPool, 
						(sizeof(CMD_BYTE_OP)), EXTENDED_COMMAND_POOL_TAG);
					if(!pExtendedCommands[iHealthyChild])
					{
						return STATUS_INSUFFICIENT_RESOURCES;
					}

					pExtendedCommands[iHealthyChild]->pLurnCreated = Lurn;
					pExtendedCommands[iHealthyChild]->Operation = CCB_EXT_BYTE_READ_OPERATE_WRITE;
					pExtendedCommands[iHealthyChild]->logicalBlockAddress = (UINT32)pRaidInfo->ChildrenInfo[iHealthyChild].SectorInfo;
					pExtendedCommands[iHealthyChild]->Offset = (UINT16)pRaidInfo->ChildrenInfo[iHealthyChild].OffsetFlagInfo;
					pExtendedCommands[iHealthyChild]->LengthByte = 1;
					pExtendedCommands[iHealthyChild]->ByteOperation = BYTE_OPERATION_COPY;
					pExtendedCommands[iHealthyChild]->pNextCmd = NULL;
					pExtendedCommands[iHealthyChild]->Byte[0] = 1;

					pRaidInfo->RaidStatus = RAID_STATUS_BITMAPPING;

					KDPrintM(DBG_LURN_ERROR,("RAID_STATUS_FAIL_DETECTED : %x, %d, %d,\n",
						pExtendedCommands[iHealthyChild]->logicalBlockAddress,
						pExtendedCommands[iHealthyChild]->Offset,
						pExtendedCommands[iHealthyChild]->LengthByte));
				}
				// fall through
			case RAID_STATUS_BITMAPPING: // bitmap work
				{
					UINT32 uiIterator;
					PCMD_BYTE_OP pCmdTemp, pNextCmd;
					UINT32 uiSectorOffsetInBitmap, uiSectorOffsetInBitmapOld = 0xFFFFFFFF;
					KIRQL oldIrql;

					KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_BITMAPPING\n"));
					
					ACQUIRE_SPIN_LOCK(&pRaidInfo->BitmapSpinLock, &oldIrql);
					pRaidInfo->RaidStatus = RAID_STATUS_BITMAPPING;
					for(uiIterator = logicalBlockAddress / pRaidInfo->ChildrenInfo[iHealthyChild].SectorsPerBit;
						uiIterator <= (logicalBlockAddress + transferBlocks -1) / pRaidInfo->ChildrenInfo[iHealthyChild].SectorsPerBit;
						uiIterator++)
					{
						KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_BITMAPPING Iterate %d -> %d\n", logicalBlockAddress, uiIterator));
						// operate if bitmap has not been checked
						if(!RtlCheckBit(pRaidInfo->Bitmap, uiIterator))
						{
							KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_BITMAPPING bitmap changed\n"));
							uiSectorOffsetInBitmap = uiIterator / BITS_PER_BLOCK; // 0 means 0 ~ 512 * 8 -1 bits in bitmap
							
							// if new sector, create new extended command and attach to bitmap
							if(0xFFFFFFFF == uiSectorOffsetInBitmapOld || // no command yet, create new one
								uiSectorOffsetInBitmapOld != uiSectorOffsetInBitmap // need another command
								)
							{
								KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_BITMAPPING Create new bitmap\n"));

								pCmdTemp = (PCMD_BYTE_OP)ExAllocatePoolWithTag(NonPagedPool, 
									(sizeof(CMD_BYTE_OP) -1 + BLOCK_SIZE), EXTENDED_COMMAND_POOL_TAG);
								if(!pCmdTemp)
								{
									// release all commands
									for(pCmdTemp = pExtendedCommands[iHealthyChild]; NULL != pCmdTemp; pCmdTemp = pNextCmd)
									{
										pNextCmd = (PCMD_BYTE_OP)pCmdTemp->pNextCmd;
										ExFreePoolWithTag(pCmdTemp, EXTENDED_COMMAND_POOL_TAG);
									}

									pExtendedCommands[iHealthyChild] = NULL;
									return STATUS_INSUFFICIENT_RESOURCES;
								}
								
								pCmdTemp->Operation = CCB_EXT_BYTE_WRITE;
								pCmdTemp->logicalBlockAddress = 
									(ULONG)pRaidInfo->ChildrenInfo[iHealthyChild].SectorBitmapStart + uiSectorOffsetInBitmap;
								pCmdTemp->Offset = 0;
								pCmdTemp->LengthByte = BLOCK_SIZE;
								pCmdTemp->ByteOperation = BYTE_OPERATION_COPY;

								pCmdTemp->pNextCmd = (PVOID)pExtendedCommands[iHealthyChild];
								pExtendedCommands[iHealthyChild] = pCmdTemp;
							}

							uiSectorOffsetInBitmapOld = uiSectorOffsetInBitmap;

							RtlSetBits(pRaidInfo->Bitmap, uiIterator, 1);
							RtlCopyMemory(
								pExtendedCommands[iHealthyChild]->Byte, 
								((PBYTE)(pRaidInfo->Bitmap->Buffer)) + uiSectorOffsetInBitmap * BLOCK_SIZE,
								BLOCK_SIZE); // full sector copy
						}
					}
					RELEASE_SPIN_LOCK(&pRaidInfo->BitmapSpinLock, oldIrql);

				}
				break;
			}
		}
		// fall down
	case SCSIOP_VERIFY:
		{
			if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
			{
				// send to the healthy child LURN
				status = LurnAssocSendCcbToChildrenArray(
														(PLURELATION_NODE *)&Lurn->LurnChildren[iHealthyChild],
														1,
														Ccb,
														LurnFaultTolerantCcbCompletionV2,
														&pExtendedCommands[iHealthyChild]
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
														LurnFaultTolerantCcbCompletionV2,
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
			VENDOR_ID,
			(strlen(VENDOR_ID)+1) < 8 ? (strlen(VENDOR_ID)+1) : 8
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
//		MpDebugPrint(1,("logicalBlockAddress = %x\n", logicalBlockAddress));
		readCapacityData->LogicalBlockAddress =	((PFOUR_BYTE)&logicalBlockAddress)->Byte3 
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte2 << 8
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte1 << 16
											| ((PFOUR_BYTE)&logicalBlockAddress)->Byte0 << 24;
				
		blockSize = BLOCK_SIZE;
		readCapacityData->BytesPerBlock = ((PFOUR_BYTE)&blockSize)->Byte3
											| ((PFOUR_BYTE)&blockSize)->Byte2 << 8
											| ((PFOUR_BYTE)&blockSize)->Byte1 << 16
											| ((PFOUR_BYTE)&blockSize)->Byte0 << 24;
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
	
				if(Lurn->Lur->LurFlags & LUR_FLAG_FAKEWRITE)
					parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
			} else
				parameterHeader->DeviceSpecificParameter &= ~MODE_DSP_WRITE_PROTECT;
	
			parameterHeader->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);

			//
			// Make Block.
			//
			BlockCount = Lurn->EndBlockAddr - Lurn->StartBlockAddr + 1;
			parameterBlock->DensityCode = 0;	// It is Reserved for direct access devices.
			parameterBlock->BlockLength[0] = (BYTE)(BLOCK_SIZE>>16);
			parameterBlock->BlockLength[1] = (BYTE)(BLOCK_SIZE>>8);
			parameterBlock->BlockLength[2] = (BYTE)(BLOCK_SIZE);

			Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
		} else {
			KDPrintM(DBG_LURN_ERROR,
						("SCSIOP_MODE_SENSE: unsupported pagecode:%x\n", (ULONG)Cdb->MODE_SENSE.PageCode));
			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
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
												LurnFaultTolerantCcbCompletionV2,
												NULL
								);
		break;

	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnMirrV2Request(
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
		LurnMirrV2Execute(Lurn, Ccb);
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
		LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnFaultTolerantCcbCompletionV2, NULL);
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
		LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnMirrUpdateCcbCompletion, NULL); // use same function as Mirror
		break;
	}

	case CCB_OPCODE_QUERY: {
		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_QUERY\n"));
		//
		//	Check to see if the CCB is coming for only this LURN.
		//
		if(LSCcbIsFlagOn(Ccb, CCB_FLAG_LOWER_LURN)) {
			LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
			LSCcbCompleteCcb(Ccb);
			break;
		}

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
		break;
	}

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	return STATUS_SUCCESS;
}
