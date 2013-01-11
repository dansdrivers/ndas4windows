#if 0
//
//	RAID4 online-recovery interface
//
NTSTATUS
LurnRAID4RInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) ;

NTSTATUS
LurnRAID4RRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

NTSTATUS
LurnRAID4RDestroy(
		PLURELATION_NODE Lurn
	) ;

LURN_INTERFACE LurnRAID4RInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_RAID4R,
					0,
					{
						LurnRAID4RInitialize,
						LurnRAID4RDestroy,
						LurnRAID4RRequest
					}
		 };
#endif


#if 0	// RAID4 is not supported now.
//////////////////////////////////////////////////////////////////////////
//
//	RAID 4R
//

NTSTATUS
LurnRAID4RInitialize(
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
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->MaxDataSendLength = %d bytes\n", LurnDesc->MaxDataSendLength));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->MaxDataRecvLength = %d bytes\n", LurnDesc->MaxDataRecvLength));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnOptions = %d\n", LurnDesc->LurnOptions));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnParent = %d\n", LurnDesc->LurnParent));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildrenCnt = %d\n", LurnDesc->LurnChildrenCnt));
	KDPrintM(DBG_LURN_INFO, ("LurnDesc->LurnChildren = 0x%p\n", LurnDesc->LurnChildren));

//	Raid Information
	Lurn->LurnRaidInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(RAID_INFO), 
		RAID_INFO_POOL_TAG);

	if(NULL == Lurn->LurnRaidInfo)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto out;
	}

	pRaidInfo = Lurn->LurnRaidInfo;

	RtlZeroMemory(pRaidInfo, sizeof(RAID_INFO));

	LurnSetRaidInfoStatus(pRaidInfo, RAID_STATUS_INITIAILIZING);

	// set spare disk count
	pRaidInfo->ActiveDiskCount = LurnDesc->LurnChildrenCnt - LurnDesc->LurnInfoRAID.nSpareDisk;
	pRaidInfo->SpareDiskCount = LurnDesc->LurnInfoRAID.nSpareDisk;

	pRaidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID.SectorsPerBit;
	if(!pRaidInfo->SectorsPerBit)
	{
		KDPrintM(DBG_LURN_ERROR, ("SectorsPerBit is zero!\n"));

		ntStatus = STATUS_INVALID_PARAMETER;
		goto out;
	}

	//
	// Determine block bytes.
	//

	Lurn->ChildBlockBytes = LurnAsscGetChildBlockBytes(Lurn);
	if(Lurn->ChildBlockBytes == 0)
		return STATUS_DEVICE_NOT_READY;
	Lurn->BlockBytes = Lurn->ChildBlockBytes * (Lurn->LurnChildrenCnt - pRaidInfo->SpareDiskCount - 1); // exclude parity

	//
	// Maximum of data send/receive length from the upper LURN.
	//

	pRaidInfo->MaxDataSendLength = LurnDesc->MaxDataSendLength;
	pRaidInfo->MaxDataRecvLength = LurnDesc->MaxDataRecvLength;

	// allocate shuffle data buffer
	{
		ULONG ulDataBufferSizePerDisk;
		ULONG ulDataBufferSize;

		if(pRaidInfo->MaxDataSendLength > pRaidInfo->MaxDataRecvLength)
			ulDataBufferSizePerDisk = pRaidInfo->MaxDataSendLength / (pRaidInfo->ActiveDiskCount -1);
		else
			ulDataBufferSizePerDisk = pRaidInfo->MaxDataRecvLength / (pRaidInfo->ActiveDiskCount -1);

		ulDataBufferSize = pRaidInfo->ActiveDiskCount * ulDataBufferSizePerDisk;

		KDPrintM(DBG_LURN_INFO, 
			("allocating data buffer. ulDataBufferSizePerDisk = %d, ulDataBufferSize = %d\n",
			ulDataBufferSizePerDisk, ulDataBufferSize));

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
	}

	// Create & init BITMAP
	{
	//	Bitmap (1) * (bitmap structure size + bitmap size)
		ulBitMapSize = (ULONG)NDAS_BLOCK_SIZE_BITMAP * Lurn->ChildBlockBytes; // use full bytes 1MB(8Mb) of bitmap
		pRaidInfo->Bitmap = (PRTL_BITMAP)ExAllocatePoolWithTag(NonPagedPool, 
			ulBitMapSize + sizeof(RTL_BITMAP), DRAID_BITMAP_POOL_TAG);

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

	// Init write log
	pRaidInfo->iWriteLog = 0;
	RtlZeroMemory(pRaidInfo->WriteLogs, sizeof(pRaidInfo->WriteLogs));

	KeInitializeSpinLock(&pRaidInfo->LockInfo);

	// Create recover thread
	if(LUR_IS_PRIMARY(Lurn->Lur)) {
		DraidArbiterStart(Lurn);
	} else {
		KDPrintM(DBG_LURN_INFO, ("Not a primary node. Don't start Draid Arbiter\n"));
	}

	// Always create draid client.
	ntStatus = DraidClientStart(Lurn); 
	if (!NT_SUCCESS(ntStatus)) {
		goto out;
	}

	ntStatus = STATUS_SUCCESS;
out:
	if(!NT_SUCCESS(ntStatus))
	{
		if(Lurn->LurnRaidInfo)
		{
			if(pRaidInfo) {
				SAFE_FREE_POOL_WITH_TAG(pRaidInfo->Bitmap, DRAID_BITMAP_POOL_TAG);
			}
			SAFE_FREE_POOL_WITH_TAG(Lurn->LurnRaidInfo, RAID_INFO_POOL_TAG);
		}
	}


	return ntStatus;
}

NTSTATUS
LurnRAID4RDestroy(
		PLURELATION_NODE Lurn
	) 
{
	PRAID_INFO pRaidInfo;

	KDPrintM(DBG_LURN_INFO, ("IN\n"));

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;
	ASSERT(Lurn->LurnRaidInfo);

	pRaidInfo = Lurn->LurnRaidInfo;
	ASSERT(pRaidInfo);

	ExDeleteNPagedLookasideList(
		&pRaidInfo->DataBufferLookaside);

	ASSERT(pRaidInfo->Bitmap);
	SAFE_FREE_POOL_WITH_TAG(pRaidInfo->Bitmap, DRAID_BITMAP_POOL_TAG);

	SAFE_FREE_POOL_WITH_TAG(pRaidInfo, RAID_INFO_POOL_TAG);

	KDPrintM(DBG_LURN_INFO, ("OUT\n"));
	return STATUS_SUCCESS ;
}

NTSTATUS
LurnRAID4RCcbCompletion(
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
		if (CCB_OPCODE_EXECUTE == OriginalCcb->OperationCode)
		{
			switch(OriginalCcb->Cdb[0])
			{
			case 0x3E:		// READ_LONG
			case SCSIOP_READ:
			case SCSIOP_READ16:
				{
					ULONG i, j, BlocksPerDisk;
					register int k;
					PULONG pDataBufferToRecover, pDataBufferSrc;

					KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));
					ASSERT(Ccb->DataBuffer);

					BlocksPerDisk = Ccb->DataBufferLength / pLurnOriginal->ChildBlockBytes;
					i = Ccb->AssociateID;

					// do not copy parity
					if (i < pRaidInfo->ActiveDiskCount -1)
					{
						for(j = 0; j < BlocksPerDisk; j++)
						{
							RtlCopyMemory( // Copy back
								(PCHAR)OriginalCcb->DataBuffer + (i + j * (pRaidInfo->ActiveDiskCount -1)) * pLurnOriginal->ChildBlockBytes,
								(PCHAR)Ccb->DataBuffer + (j * pLurnOriginal->ChildBlockBytes),
								pLurnOriginal->ChildBlockBytes);
						}
					}

					// if non-parity device is stop, do parity work on the blocks
					if ((RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
						RAID_STATUS_EMERGENCY_READY == pRaidInfo->RaidStatus ||
						RAID_STATUS_RECOVERING == pRaidInfo->RaidStatus) &&
						pRaidInfo->ActiveDiskCount -1 != pRaidInfo->iChildFault)
					{
						// parity work
						pDataBufferSrc = (PULONG)Ccb->DataBuffer;
						for(j = 0; j < BlocksPerDisk; j++)
						{
							pDataBufferToRecover = 
								(PULONG)(
								(PCHAR)OriginalCcb->DataBuffer + 
								(pRaidInfo->iChildDefected + j * (pRaidInfo->ActiveDiskCount -1)) * pLurnOriginal->ChildBlockBytes);

							k = pLurnOriginal->ChildBlockBytes / sizeof(ULONG);
							while(k--)
							{
								*pDataBufferToRecover ^= *pDataBufferSrc;
								pDataBufferToRecover++;
								pDataBufferSrc++;
							}
						}
					}
				}
				break;
			}
		}
		break;

	case CCB_STATUS_BUSY:		// priority 3
		if(OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {
			ASSERT(OriginalCcb->Srb || OriginalCcb->OperationCode == CCB_OPCODE_RESETBUS);
			OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
		}
		break;

	case CCB_STATUS_STOP:		// priority 1/5
		if(!LsCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP))
		{
			PLURELATION_NODE pLurnChildDefected;
			ULONG i;

			LsCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_STOP);	// priority 1
			LsCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_RAID_DEGRADED);

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
				ASSERT(LURN_RAID4R == pLurnOriginal->LurnType);

				if(!pLurnChildDefected || !pLurnOriginal)
				{
					ASSERT(FALSE);
					status = STATUS_ILLEGAL_INSTRUCTION;
					break;
				}

				ASSERT(pLurnChildDefected->LurnRAIDInfo);

				if(RAID_STATUS_RECOVERING == pRaidInfo->RaidStatus)
				{
					ASSERT(FALSE);
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
				}

				// 1 fail + 1 fail = broken
				if(RAID_STATUS_NORMAL != pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("RAID_STATUS_NORMAL != pRaidInfo->RaidStatus(%d)\n", pRaidInfo->RaidStatus));

					if(pLurnChildDefected != pRaidInfo->RoleIndexLurnChildren[pRaidInfo->iChildFault])
					{
						ASSERT(FALSE);
						//						status = STATUS_DATATYPE_MISALIGNMENT;
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
						break;
					}
				}

				// set pRaidInfo->iChildDefected
				for(i = 0; i < pRaidInfo->ActiveDiskCount + pRaidInfo->SpareDiskCount; i++)
				{
					if(pLurnChildDefected == pRaidInfo->RoleIndexLurnChildren[i])
					{
						pRaidInfo->iChildFault = i;
						break;
					}
				}

				if(i < pRaidInfo->ActiveDiskCount)
				{
					// ok
					KDPrintM(DBG_LURN_TRACE, ("pLurnChildDefected(%p) == pRaidInfo->RoleIndexLurnChildren[%d]\n", pLurnChildDefected, i));
				}
				else if(i < pRaidInfo->ActiveDiskCount + pRaidInfo->SpareDiskCount)
				{
					// ignore spare, just break
					KDPrintM(DBG_LURN_INFO, ("pLurnChildDefected(%p) == pRaidInfo->RoleIndexLurnChildren[%d](spare) opcode : %x, cdb[0] : %x\n", pLurnChildDefected, i, OriginalCcb->OperationCode, (ULONG)OriginalCcb->Cdb[0]));
					break;
				}
				else //if(i == pRaidInfo->ActiveDiskCount)
				{
					// failed to find a defected child
					KDPrintM(DBG_LURN_ERROR, ("pLurnChildDefected(%p) NOT found pRaidInfo->RoleIndexLurnChildren[%d] opcode : %x, cdb[0] : %x\n", pLurnChildDefected, i, OriginalCcb->OperationCode, (ULONG)OriginalCcb->Cdb[0]));
					ASSERT(FALSE);
					status = STATUS_DATATYPE_MISALIGNMENT;
					OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					break;
				}

				if (RAID_STATUS_EMERGENCY != pRaidInfo->RaidStatus &&
					RAID_STATUS_EMERGENCY_READY != pRaidInfo->RaidStatus)
				{
					KDPrintM(DBG_LURN_ERROR, ("Set from %d to RAID_STATUS_EMERGENCY_READY\n", pRaidInfo->RaidStatus));

					LurnSetRaidInfoStatus(pRaidInfo, RAID_STATUS_EMERGENCY_READY);
					pRaidInfo->rmd.UnitMetaData[pRaidInfo->iChildFault].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;

					for(i = 0; i < pRaidInfo->ActiveDiskCount; i++)
					{
						// 1 fail + 1 fail = broken
						if(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & pRaidInfo->rmd.UnitMetaData[i].UnitDeviceStatus &&
							i != pRaidInfo->iChildFault)
						{
							ASSERT(FALSE);
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;
							break;
						}
					}

					if(CCB_STATUS_STOP == OriginalCcb->CcbStatus)
						break;

					// access again, as we can't complete reading at this time
					KDPrintM(DBG_LURN_ERROR, ("NORMAL -> EMERGENCY_READY : Read again, OriginalCcb->CcbStatus = CCB_STATUS_BUSY\n"));
					OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
				}

				KDPrintM(DBG_LURN_ERROR, ("CCB_STATUS_STOP, pRaidInfo->iChildFault = %d\n", pRaidInfo->iChildFault));
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
		if(LsCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_RAID_DEGRADED)) {
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

		if(!LsCcbIsStatusFlagOn(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR)) {
			LsCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_LURN_IN_ERROR);	// priority 0
		} else {
			//
			//	at least two children have an error or do not exist! (2 not exist)
			//
			OriginalCcb->CcbStatus = Ccb->CcbStatus;	// 	// priority 4
		}
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

	if(RAID_STATUS_FAIL == pRaidInfo->RaidStatus)
	{
		ASSERT(FALSE);
		OriginalCcb->CcbStatus = CCB_STATUS_STOP;
	}

	LsCcbSetStatusFlag(	OriginalCcb,
		Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
		);

	if(RAID_STATUS_RECOVERING == pRaidInfo->RaidStatus)
		LsCcbSetStatusFlag(OriginalCcb, CCBSTATUS_FLAG_RAID_RECOVERING);

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

	LSAssocSetRedundentRaidStatusFlag(pRaidInfo, OriginalCcb);

	LsCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

static
NTSTATUS
LurnRAID4RExecute(
		PLURELATION_NODE		Lurn,
		IN	PCCB				Ccb
	) 
{
	NTSTATUS			status = STATUS_SUCCESS;

	PRAID_INFO pRaidInfo;
	KIRQL	oldIrql;

	pRaidInfo = Lurn->LurnRaidInfo;
	// record a bitmap information to the next disk of the defected disk
	// pExtendedCommands itself will go into LurnAssocSendCcbToChildrenArray
	if(RAID_STATUS_INITIAILIZING != pRaidInfo->RaidStatus)
	{
		ASSERT(pRaidInfo->ActiveDiskCount > 0 && pRaidInfo->ActiveDiskCount <= NDAS_MAX_RAID4R_CHILD);
	}

	if(
		RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
		RAID_STATUS_RECOVERING == pRaidInfo->RaidStatus 
		)
	{
		ASSERT(pRaidInfo->iChildFault < pRaidInfo->ActiveDiskCount);
	}

	// recovery/data buffer protection code
	switch(Ccb->Cdb[0])
	{
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:			
		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:			
		case SCSIOP_VERIFY:
		case SCSIOP_VERIFY16:			
			{
				UINT64 logicalBlockAddress;
				UINT32 transferBlocks;

				LsCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &logicalBlockAddress, &transferBlocks);

				// Busy if this location is under recovering

				ACQUIRE_SPIN_LOCK(&pRaidInfo->LockInfo, &oldIrql);
				if(
					RAID_STATUS_INITIAILIZING == pRaidInfo->RaidStatus || // do not IO when reading bitmap
					RAID_STATUS_EMERGENCY_READY == pRaidInfo->RaidStatus || // emergency mode not ready
					(
						RAID_STATUS_RECOVERING == pRaidInfo->RaidStatus && // do not IO where being recovered
						(
							pRaidInfo->BitmapIdxToRecover == logicalBlockAddress / pRaidInfo->SectorsPerBit ||
							pRaidInfo->BitmapIdxToRecover == (logicalBlockAddress + transferBlocks -1) / pRaidInfo->SectorsPerBit
						)
					)
					)
				{
					RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);

					KDPrintM(DBG_LURN_INFO, ("!! RECOVER THREAD PROTECTION : %08lx, cmd : 0x%x, %x, %d\n", pRaidInfo->RaidStatus, (UINT32)Ccb->Cdb[0], logicalBlockAddress, transferBlocks));
					Ccb->CcbStatus = CCB_STATUS_BUSY;
					goto complete_here;
				}

				RELEASE_SPIN_LOCK(&pRaidInfo->LockInfo, oldIrql);
			}
	}

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:		
		{
			UINT64 logicalBlockAddress;
			UINT32 transferBlocks;
			register ULONG i, j;
			register int k;

			int DataBufferLengthPerDisk;
			ULONG BlocksPerDisk;
			PULONG pDataBufferParity, pDataBufferSrc;
			CUSTOM_DATA_BUFFER cdb;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			LsCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &logicalBlockAddress, &transferBlocks);

			ASSERT(transferBlocks <= pRaidInfo->MaxDataSendLength/Lurn->BlockBytes);
			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			switch(pRaidInfo->RaidStatus)
			{
			case RAID_STATUS_NORMAL:
			case RAID_STATUS_RECOVERING:
				// add write log
				{
					PWRITE_LOG write_log;
					pRaidInfo->iWriteLog++;
					write_log = &pRaidInfo->WriteLogs[pRaidInfo->iWriteLog % NDAS_RAID_WRITE_LOG_SIZE];
					write_log->logicalBlockAddress = logicalBlockAddress;
					write_log->transferBlocks = transferBlocks;
					write_log->timeStamp = pRaidInfo->iWriteLog;
				}
				break;
			case RAID_STATUS_EMERGENCY:
				{
					UINT32 uiBitmapStartInBits, uiBitmapEndInBits;

					KDPrintM(DBG_LURN_TRACE,("RAID_STATUS_EMERGENCY\n"));
					
					// seek first sector in bitmap
					uiBitmapStartInBits = (UINT32)(logicalBlockAddress / 
						pRaidInfo->SectorsPerBit);
					uiBitmapEndInBits = (UINT32)((logicalBlockAddress + transferBlocks -1) /
						pRaidInfo->SectorsPerBit);

					RtlSetBits(pRaidInfo->Bitmap, uiBitmapStartInBits, uiBitmapEndInBits - uiBitmapStartInBits +1);
				}
				break;
			default:
				// invalid status
				ASSERT(FALSE);
				break;
			}

			// create new data buffer and encrypt here.
			// new data buffer will be deleted at completion routine
			DataBufferLengthPerDisk = Ccb->DataBufferLength / (pRaidInfo->ActiveDiskCount -1);
			BlocksPerDisk = DataBufferLengthPerDisk / Lurn->ChildBlockBytes;

			cdb.DataBufferCount = 0;
			for(i = 0; i < pRaidInfo->ActiveDiskCount; i++)
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
					LsCcbCompleteCcb(Ccb);
					break;
				}

				cdb.DataBufferLength[i] = DataBufferLengthPerDisk;
				cdb.DataBufferCount++;
			}
			
			// split DataBuffer into each DataBuffers of children by block size
			for(i = 0; i < pRaidInfo->ActiveDiskCount -1; i++)
			{
				pDataBufferSrc = (PULONG)cdb.DataBuffer[i];

				for(j = 0; j < BlocksPerDisk; j++)
				{
					RtlCopyMemory(
						(PCHAR)pDataBufferSrc + j * Lurn->ChildBlockBytes,
						(PCHAR)Ccb->DataBuffer + (i + j * (pRaidInfo->ActiveDiskCount -1)) * Lurn->ChildBlockBytes,
						Lurn->ChildBlockBytes);
				}
			}

			// generate parity
			// initialize the parity buffer with the first buffer
			RtlCopyMemory(
				cdb.DataBuffer[pRaidInfo->ActiveDiskCount -1],
				cdb.DataBuffer[0],
				Lurn->ChildBlockBytes * BlocksPerDisk);

			// p' ^= p ^ d; from second buffer
			for(i = 1; i < pRaidInfo->ActiveDiskCount -1; i++)
			{
				pDataBufferSrc = (PULONG)cdb.DataBuffer[i];
				pDataBufferParity = (PULONG)cdb.DataBuffer[pRaidInfo->ActiveDiskCount -1];

				// parity work
				k = (BlocksPerDisk * Lurn->ChildBlockBytes) / sizeof(ULONG);
				while(k--)
				{
					*pDataBufferParity ^= *pDataBufferSrc;
					pDataBufferParity++;
					pDataBufferSrc++;
				}
			}

			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->RoleIndexLurnChildren,
				pRaidInfo->ActiveDiskCount,
				Ccb,
				LurnRAID4RCcbCompletion,
				&cdb,
				NULL,
				FALSE);

			if(!NT_SUCCESS(status))
			{
				for(i = 0; i < pRaidInfo->ActiveDiskCount; i++)
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
		{
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_VERIFY\n"));

			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->RoleIndexLurnChildren,
				pRaidInfo->ActiveDiskCount,
				Ccb,
				LurnRAID4RCcbCompletion,
				NULL,
				NULL,
				FALSE);
		}
		break;

	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_READ16:		
		{
			ULONG DataBufferLengthPerDisk;
			ULONG i, j;
			CUSTOM_DATA_BUFFER cdb;

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_READ\n"));

			DataBufferLengthPerDisk = Ccb->DataBufferLength / (pRaidInfo->ActiveDiskCount -1);
			KDPrintM(DBG_LURN_NOISE,("R Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));
			cdb.DataBufferCount = 0;
			for(i = 0; i < pRaidInfo->ActiveDiskCount; i++)
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
					LsCcbCompleteCcb(Ccb);
					break;
				}


				cdb.DataBufferLength[i] = DataBufferLengthPerDisk;
				cdb.DataBufferCount++;
			}

			// We should erase the buffer for defected child
			// We will fill this buffer using parity at completion routine
			if ((RAID_STATUS_EMERGENCY == pRaidInfo->RaidStatus ||
				RAID_STATUS_EMERGENCY_READY == pRaidInfo->RaidStatus ||
				RAID_STATUS_RECOVERING == pRaidInfo->RaidStatus))
			{
				RtlZeroMemory(Ccb->DataBuffer, Ccb->DataBufferLength);
			}

			cdb.DataBufferCount = pRaidInfo->ActiveDiskCount;
			status = LurnAssocSendCcbToChildrenArray(
				pRaidInfo->RoleIndexLurnChildren,
				pRaidInfo->ActiveDiskCount,
				Ccb,
				LurnRAID4RCcbCompletion,
				&cdb,
				NULL,
				FALSE
				);
			if(!NT_SUCCESS(status))
			{
				for(i = 0; i < pRaidInfo->ActiveDiskCount; i++)
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
		UCHAR				Model[16] = RAID4R_MODEL_NAME;


		KDPrintM(DBG_LURN_INFO,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]));
		//
		//	We don't support EVPD(enable vital product data).
		//
		if(Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			KDPrintM(DBG_LURN_ERROR,("SCSIOP_INQUIRY: got EVPD. Not supported.\n"));

			LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
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
		LsCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
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
			if(logicalBlockAddress < 0xffffffff) {
				REVERSE_BYTES(&readCapacityData->LogicalBlockAddress, &logicalBlockAddress);
			} else {
				readCapacityData->LogicalBlockAddress = 0xffffffff;
			}

			blockSize = Lurn->BlockBytes;
			REVERSE_BYTES(&readCapacityData->BytesPerBlock, &blockSize);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LsCcbCompleteCcb(Ccb);
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
			LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			LsCcbCompleteCcb(Ccb);
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

			blockSize = BLOCK_SIZE * (Lurn->LurnChildrenCnt - pRaidInfo->SpareDiskCount - 1);
			REVERSE_BYTES(&readCapacityDataEx->BytesPerBlock, &blockSize);

			LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;

			KDPrintM(DBG_LURN_TRACE, ("SCSIOP_READ_CAPACITY16: %08x : %04x\n", (UINT32)logicalBlockAddress, (UINT32)blockSize));
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
				parameterBlock->BlockLength[0] = (BYTE)(Lurn->BlockBytes>>16);
				parameterBlock->BlockLength[1] = (BYTE)(Lurn->BlockBytes>>8);
				parameterBlock->BlockLength[2] = (BYTE)(Lurn->BlockBytes);

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
				LurnRAID4RCcbCompletion,
				NULL,
				NULL,
				FALSE
				);
		}
		break;
	}

	return STATUS_SUCCESS;
complete_here:
	LSAssocSetRedundentRaidStatusFlag(pRaidInfo, Ccb);

	LsCcbCompleteCcb(Ccb);

	return status;
}

NTSTATUS
LurnRAID4RRequest(
				 PLURELATION_NODE	Lurn,
				 PCCB				Ccb
				 )
{
	NTSTATUS				status;
	PRAID_INFO pRaidInfo;

	pRaidInfo = Lurn->LurnRaidInfo;

	//
	//	dispatch a request
	//
	KDPrintM(DBG_LURN_TRACE, ("IN\n"));

	switch(Ccb->OperationCode) {
	case CCB_OPCODE_EXECUTE:

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
		LurnRAID4RExecute(Lurn, Ccb);
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
			if(LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID4RCcbCompletion, NULL, NULL, FALSE);
			break;
		}
	case CCB_OPCODE_STOP:
		{
			LARGE_INTEGER TimeOut;
			KIRQL oldIrql;

			KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_STOP\n"));

			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
			if(pRaidInfo->ThreadRecoverHandle)
			{
				Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
				LurnSetRaidInfoStatus(pRaidInfo, RAID_STATUS_TERMINATING);
				KDPrintM(DBG_LURN_INFO, ("KeSetEvent\n"));
				KeSetEvent(&pRaidInfo->RecoverThreadEvent,IO_NO_INCREMENT, FALSE);
			}
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			TimeOut.QuadPart = - NANO100_PER_SEC * 120;

			KDPrintM(DBG_LURN_INFO, ("KeWaitForSingleObject IN\n"));
			status = KeWaitForSingleObject(
				pRaidInfo->ThreadRecoverObject,
				Executive,
				KernelMode,
				FALSE,
				&TimeOut
				);

			KDPrintM(DBG_LURN_INFO, ("KeWaitForSingleObject OUT\n"));

			ASSERT(status == STATUS_SUCCESS);

			//
			//	Dereference the thread object.
			//

			ObDereferenceObject(pRaidInfo->ThreadRecoverObject);
			
			ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

			pRaidInfo->ThreadRecoverObject = NULL;
			pRaidInfo->ThreadRecoverHandle = NULL;

			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID4RCcbCompletion, NULL, NULL, FALSE);
			break;
		}
	case CCB_OPCODE_UPDATE:
		{
			//
			//	Check to see if the CCB is coming for only this LURN.
			//
			if(LsCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
				LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;
			}
			status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAIDUpdateCcbCompletion, NULL, NULL, TRUE); // use same function as Mirror
			break;
		}

	case CCB_OPCODE_QUERY:
		status = LurnAssocQuery(Lurn, LurnAggrCcbCompletion, Ccb);
		break;

	default:
		KDPrintM(DBG_LURN_TRACE, ("INVALID COMMAND\n"));
		LsCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
		goto complete_here;
		break;
	}

	return STATUS_SUCCESS;

complete_here:
	LSAssocSetRedundentRaidStatusFlag(pRaidInfo, Ccb);

	LsCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}
#endif
