#include "ndasklibproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LurnIde"


//////////////////////////////////////////////////////////////////////////
//
//	common to all IDE interface
//

void
ConvertString(
		PCHAR	result,
		PCHAR	source,
		int	size
	) {
	int	i;

	for(i = 0; i < size / 2; i++) {
		result[i * 2] = source[i * 2 + 1];
		result[i * 2 + 1] = source[i * 2];
	}
	result[size] = '\0';
	
}


BOOLEAN
Lba_capacity_is_ok (
	struct hd_driveid *id
	)
{
	unsigned _int32	lba_sects, chs_sects, head, tail;

	if ((id->command_set_2 & 0x0400) && (id->cfs_enable_2 & 0x0400)) {

		// 48 Bit Drive.
		return TRUE;
	}

	// The ATA spec tells large drivers to return
	// C/H/S = 16383/16/63 independent of their size.
	// Some drives can be jumpered to use 15 heads instead of 16.
	// Some drives can be jumpered to use 4092 cyls instead of 16383

	if ((id->cyls == 16383 || (id->cyls == 4092 && id->cur_cyls== 16383)) && 
		id->sectors == 63 && (id->heads == 15 || id->heads == 16) && id->lba_capacity >= (unsigned)(16383 * 63 * id->heads)) {

		return TRUE;
	}

	lba_sects = id->lba_capacity;
	chs_sects = id->cyls * id->heads * id->sectors;

	// Perform a rough sanity check on lba_sects: within 10% is OK

	if ((lba_sects - chs_sects) < chs_sects / 10) {

		return TRUE;
	}

	// Some drives have the word order reversed

	head = ((lba_sects >> 16) & 0xffff);
	tail = (lba_sects & 0xffff);
	lba_sects = (head | (tail << 16));
	
	if ((lba_sects - chs_sects) < chs_sects / 10) {

		id->lba_capacity = lba_sects;

		KDPrintM( DBG_LURN_ERROR, ("Lba_capacity_is_ok: Capacity reversed....\n") );

		return TRUE;
	}

	return FALSE;
}

//	Ide query

NTSTATUS
LurnIdeQuery (
	PLURELATION_NODE	Lurn,
	PLURNEXT_IDE_DEVICE	IdeDev,
	PCCB				Ccb
	) 
{
    PLUR_QUERY	LurQuery;
	NTSTATUS	status;

    // Start off being paranoid.

	if (IdeDev == NULL || Ccb->DataBuffer == NULL) {

		NDAS_ASSERT(FALSE);

        return STATUS_UNSUCCESSFUL;
    }

	status			= STATUS_SUCCESS;
	Ccb->CcbStatus  = CCB_STATUS_SUCCESS;

    // Extract the query

    LurQuery = (PLUR_QUERY)Ccb->DataBuffer;

    switch (LurQuery->InfoClass) {

	case LurPrimaryLurnInformation: {

		PLURN_PRIMARYINFORMATION	ReturnInfo;
		PLURN_INFORMATION			LurnInfo;

		KDPrintM( DBG_LURN_INFO, ("LurPrimaryLurnInformation\n") );

		ReturnInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION(LurQuery);
		ReturnInfo->Length = sizeof(LURN_PRIMARYINFORMATION);
		LurnInfo = &ReturnInfo->PrimaryLurn;

		RtlZeroMemory( LurnInfo, sizeof(LURN_INFORMATION) );

		LurnInfo->Length = sizeof(LURN_INFORMATION);
		LurnInfo->UnitBlocks = Lurn->UnitBlocks;
		LurnInfo->BlockBytes = Lurn->BlockBytes;
		LurnInfo->AccessRight = Lurn->AccessRight;

		LurnInfo->UnitDiskId = IdeDev->LuHwData.LanscsiTargetID;
		LurnInfo->UnitDiskId = IdeDev->LuHwData.LanscsiTargetID;

		if (Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {

			LurnInfo->Connections = 1;
			
		} else {

			LurnInfo->Connections = 2;
		}

		LurnInfo->NdasBindingAddress = IdeDev->LanScsiSession->NdasBindAddress;
		LurnInfo->NdasNetDiskAddress = IdeDev->LanScsiSession->NdasNodeAddress; 

		RtlCopyMemory( &LurnInfo->UserID, &IdeDev->LanScsiSession->UserID, LSPROTO_USERID_LENGTH );
		RtlCopyMemory( &LurnInfo->Password, &IdeDev->LanScsiSession->Password, LSPROTO_PASSWORD_LENGTH );

		RtlZeroMemory( LurnInfo->PrimaryId, LURN_PRIMARY_ID_LENGTH );

#if __NDAS_SCSI_REMAIN_QUERY_IDE_BUG__
		RtlCopyMemory( &LurnInfo->PrimaryId[0], LurnInfo->NdasNetDiskAddress.Address, 8 );
#else
		RtlCopyMemory( &LurnInfo->PrimaryId[0], &LurnInfo->NdasNetDiskAddress.Address[0].NdasAddress.Lpx.Port, 2 );
		RtlCopyMemory( &LurnInfo->PrimaryId[2], LurnInfo->NdasNetDiskAddress.Address[0].NdasAddress.Lpx.Node, 6 );
#endif

		LurnInfo->PrimaryId[8] = LurnInfo->UnitDiskId;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("NdscId: %x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x\n", 
					 LurnInfo->PrimaryId[0], LurnInfo->PrimaryId[1], LurnInfo->PrimaryId[2], LurnInfo->PrimaryId[3],
					 LurnInfo->PrimaryId[4], LurnInfo->PrimaryId[5], LurnInfo->PrimaryId[6], LurnInfo->PrimaryId[7],
					 LurnInfo->PrimaryId[8], LurnInfo->PrimaryId[9], LurnInfo->PrimaryId[10], LurnInfo->PrimaryId[11],
					 LurnInfo->PrimaryId[12], LurnInfo->PrimaryId[13], LurnInfo->PrimaryId[14], LurnInfo->PrimaryId[15]) );

		break;
	}

	case LurEnumerateLurn: {

		PLURN_ENUM_INFORMATION	ReturnInfo;
		PLURN_INFORMATION		LurnInfo;

		KDPrintM( DBG_LURN_ERROR, ("LurEnumerateLurn\n") );

		ReturnInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION(LurQuery);
		LurnInfo = &ReturnInfo->Lurns[Lurn->LurnId];

		LurnInfo->Length = sizeof(LURN_INFORMATION);

		LurnInfo->UnitBlocks  = Lurn->UnitBlocks;
		LurnInfo->BlockBytes  = Lurn->BlockBytes;
		LurnInfo->AccessRight = Lurn->AccessRight;

		if (IdeDev) {

			LurnInfo->UnitDiskId = IdeDev->LuHwData.LanscsiTargetID;
		
			if (Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {

				LurnInfo->Connections = 1;
			
			} else {

				LurnInfo->Connections = 2;
			}

			LurnInfo->NdasBindingAddress = IdeDev->LanScsiSession->NdasBindAddress;

			LurnInfo->NdasNetDiskAddress = IdeDev->LanScsiSession->NdasNodeAddress;

			RtlCopyMemory( &LurnInfo->UserID, &IdeDev->LanScsiSession->UserID, LSPROTO_USERID_LENGTH );
			RtlCopyMemory( &LurnInfo->Password, &IdeDev->LanScsiSession->Password, LSPROTO_PASSWORD_LENGTH );
			
		} else {
		
			LurnInfo->UnitDiskId  = 0;
			LurnInfo->Connections = 0;

			RtlZeroMemory( &LurnInfo->NdasBindingAddress, sizeof(TA_NDAS_ADDRESS) ); 

			LurnInfo->NdasBindingAddress.Address[0].AddressType = TDI_ADDRESS_TYPE_INVALID;

			RtlZeroMemory( &LurnInfo->NdasNetDiskAddress, sizeof(TA_NDAS_ADDRESS) ); 

			LurnInfo->NdasNetDiskAddress.Address[0].AddressType = TDI_ADDRESS_TYPE_INVALID;

			RtlZeroMemory( &LurnInfo->UserID, LSPROTO_USERID_LENGTH );
			RtlZeroMemory( &LurnInfo->Password, LSPROTO_PASSWORD_LENGTH );
		}

		LurnInfo->LurnId = Lurn->LurnId;
		LurnInfo->LurnType = Lurn->LurnType;
		LurnInfo->StatusFlags = Lurn->LurnStatus;

		break;
	}

	case LurRefreshLurn: {

		PLURN_REFRESH			ReturnInfo;

		KDPrintM( DBG_LURN_TRACE, ("LurRefreshLurn\n") );

		ReturnInfo = (PLURN_REFRESH)LUR_QUERY_INFORMATION(LurQuery);

		if (LURN_STATUS_STOP == Lurn->LurnStatus) {

			KDPrintM( DBG_LURN_ERROR, ("!!!!!!!! LURN_STATUS_STOP == Lurn->LurnStatus !!!!!!!!\n") );

			ReturnInfo->CcbStatusFlags |= CCBSTATUS_FLAG_LURN_STOP;
		}

		break;
	}

	default:

		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		break;
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
//	configure IDE disk.
//

NTSTATUS
LurnIdeConfigure (
	IN	PLURELATION_NODE	Lurn,
	OUT	PBYTE				PduResponse
	)
{
	NTSTATUS			status;

	PLURNEXT_IDE_DEVICE	ideDisk;

	LANSCSI_PDUDESC		pduDesc;
	struct hd_driveid	info;
	char				buffer[41];

	UINT32				oldLudataFlsgs;
	UINT32				oldPduDescFlags;

	UCHAR				pioMode;
	UCHAR				pioFeature;

	// Init

	NDAS_ASSERT( Lurn->LurnExtension );

	ideDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

	oldLudataFlsgs	= ideDisk->LuHwData.LudataFlags;
	oldPduDescFlags = ideDisk->LuHwData.PduDescFlags;

	ideDisk->LuHwData.LudataFlags   = 0;
	ideDisk->LuHwData.PduDescFlags &= ~(PDUDESC_FLAG_LBA|PDUDESC_FLAG_LBA48|PDUDESC_FLAG_PIO|PDUDESC_FLAG_DMA|PDUDESC_FLAG_UDMA);

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Target ID %d\n", ideDisk->LuHwData.LanscsiTargetID) );

	do {

		LONG retryRequest;

#if __USE_PRESET_PIO_MODE__

		// Default PIO mode setting.
		// Some device requires PIO setting before IDENTIFY command.
		// Ex) SunPlus SPIF223A

		pioMode		= NDAS_DEFAULT_PIO_MODE;
		pioFeature  = pioMode | 0x08;	

		LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL, NULL );

		pduDesc.Feature		= SETFEATURES_XFER;
		pduDesc.BlockCount  = pioFeature;

		status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );

		if (status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed(1)...\n") );
			break;

		} else if (*PduResponse != LANSCSI_RESPONSE_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed(2)...\n") );

			// Ignore the response. Proceed to Identify command.
		}
#endif

		// Issue identify command.
		//
		// Try identify command several times.
		// A hard disk with long spin-up time might make the NDAS chip non-responsive.
		// Hard disk spin-down -> NDASSCSI requests sector IO -> Hard disk spin-up ->
		// -> NDAS chip not response -> NDASSCSI enters reconnection ->
		// -> Identify in LurnIdeDiskConfigure() -> May be error return.

		pduDesc.Feature	   = 0;
		pduDesc.BlockCount = 0;

		for (retryRequest = 0; retryRequest < 2; retryRequest ++) {

			LARGE_INTEGER	interval;

			LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_IDENTIFY, 0, 1, sizeof(info), &info, NULL );

			status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );

			if (NT_SUCCESS(status) && *PduResponse == LANSCSI_RESPONSE_SUCCESS) {

				break;
			}

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Identify Failed.Retry..\n") );

			interval.QuadPart = (-5*NANO100_PER_SEC);      //delay 5 seconds
			KeDelayExecutionThread( KernelMode, FALSE, &interval );
		}

	} while (0);

	if (!NT_SUCCESS(status) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {

		NDAS_ASSERT(FALSE);

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Give up..\n") );
		
		// Recover the original flags.
		// Cause we did not get valid identify data.
		
		ideDisk->LuHwData.LudataFlags  = oldLudataFlsgs;
		ideDisk->LuHwData.PduDescFlags = oldPduDescFlags;

		LurnRecordFault( Lurn, LURN_ERR_DISK_OP, LURN_FAULT_IDENTIFY, NULL );
		
		return NT_SUCCESS(status) ? STATUS_UNSUCCESSFUL : status;
	}

	// DMA/PIO Mode.

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("Major 0x%x, Minor 0x%x, Capa 0x%x\n", info.major_rev_num, info.minor_rev_num, info.capability) );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("DMA 0x%x, U-DMA 0x%x\n", info.dma_mword, info.dma_ultra) );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("CFS1 0x%x, CFS1 enabled 0x%x\n", info.command_set_1, info.cfs_enable_1) );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("CFS2 0x%x, CFS2 enabled 0x%x\n", info.command_set_2, info.cfs_enable_2) );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("CFS ext 0x%x, CFS default 0x%x\n", info.cfsse, info.csf_default) );

	// Determine PIO Mode

	pioMode = 2;

	if (info.eide_pio_modes & 0x0001) {

		pioMode = 3;
	}

	if (info.eide_pio_modes & 0x0002) {

		pioMode = 4;
	}

	pioFeature = pioMode | 0x08;

	LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL, NULL );

	pduDesc.Feature = SETFEATURES_XFER;
	pduDesc.BlockCount = pioFeature;

	status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );

	if (status != STATUS_SUCCESS || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed...\n") );

		return status;
	}

	// Lurn->UDMARestrictValid = TRUE;
	// Lurn->UDMARestrict = 0xff;

	// determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions and disk capacity.

	do {

		UCHAR	DmaFeature;
		UCHAR	DmaMode;

		DmaFeature  = 0;
		DmaMode		= 0;

		// Ultra DMA if NDAS chip is 2.0 or higher.

		// We can't support UDMA for 2.0 rev 0 due to the bug.
		// The bug : Written data using UDMA will be corrupted

		if (info.dma_ultra & 0x00ff											&&
			!(Lurn->UDMARestrictValid && Lurn->UDMARestrict == 0xff)		&& // not disable UDMA
			ideDisk->LuHwData.HwVersion	>= LANSCSIIDE_VERSION_2_0	 		&&
			!(ideDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_2_0 && ideDisk->LuHwData.HwRevision == 0)) {

			// Find Fastest Mode.

			if (info.dma_ultra & 0x0001) {

				DmaMode = 0;
			}

			if (info.dma_ultra & 0x0002) {

				DmaMode = 1;
			}

			if (info.dma_ultra & 0x0004) {

				DmaMode = 2;
			}

			//	if Cable80, try higher Ultra Dma Mode.

#if __DETECT_CABLE80__
			if (info.hw_config & 0x2000) {
#endif
				if (info.dma_ultra & 0x0008) {

					DmaMode = 3;
				}

				if (info.dma_ultra & 0x0010) {

					DmaMode = 4;
				}

				if (info.dma_ultra & 0x0020) {

					DmaMode = 5;
				}

				if (info.dma_ultra & 0x0040) {

					DmaMode = 6;
				}

				if (info.dma_ultra & 0x0080) {

					DmaMode = 7;
				}

				// If the ndas device is version 2.0 revision 100Mbps,
				// Restrict UDMA to mode 2.

				if (ideDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_2_0 && 
					ideDisk->LuHwData.HwRevision == LANSCSIIDE_VER20_REV_100M) {

					if (DmaMode > 2) {
					
						DmaMode = 2;
					}
				}

				// Restrict UDMA mode when requested.

				if (Lurn->UDMARestrictValid) {

					if (DmaMode > Lurn->UDMARestrict) {

						DmaMode = Lurn->UDMARestrict;

						DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("UDMA restriction applied. UDMA=%c\n", DmaMode) );
					}
				}
#if __DETECT_CABLE80__
			}
#endif
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Ultra DMA %c detected.\n", DmaMode) );

			DmaFeature = DmaMode | 0x40;	// Ultra DMA mode.
			ideDisk->LuHwData.PduDescFlags |= (PDUDESC_FLAG_DMA | PDUDESC_FLAG_UDMA);

		} else if (info.dma_mword & 0x00ff) {

			if (info.dma_mword & 0x0001) {

				DmaMode = 0;
			}

			if (info.dma_mword & 0x0002) {

				DmaMode = 1;
			}

			if (info.dma_mword & 0x0004) {

				DmaMode = 2;
			}

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("DMA mode %d detected.\n", (int)DmaMode) );

			DmaFeature = DmaMode | 0x20;
			ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_DMA;
		}

		// Set DMA mode if needed.

		if (FlagOn(ideDisk->LuHwData.PduDescFlags, PDUDESC_FLAG_DMA)) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("DMA Feature = %x\n", DmaFeature) );

			LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL, NULL );

			pduDesc.Feature = SETFEATURES_XFER;
			pduDesc.BlockCount = DmaFeature;

			status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );
			
			if (!NT_SUCCESS(status) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {

				NDAS_ASSERT(FALSE);

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed...\n") );

				return status;
			}

			// identify

			LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_IDENTIFY, 0, 1, sizeof(info), &info, NULL );
			
			status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );
			
			if (!NT_SUCCESS(status) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
			
				NDAS_ASSERT(FALSE);			

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Identify Failed...\n") );

				LurnRecordFault( Lurn, LURN_ERR_DISK_OP, LURN_FAULT_IDENTIFY, NULL );

				return status;
			}

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("After Set Feature DMA 0x%x, U-DMA 0x%x\n", info.dma_mword, info.dma_ultra) );

			break;
		}

		//	PIO.

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("ATA device does not support DMA mode. Turn to PIO mode.\n") );

		ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_PIO;

	} while(0);

	//	Product strings.

	ConvertString( (PCHAR)buffer, (PCHAR)info.serial_no, 20 );
	RtlCopyMemory( ideDisk->LuHwData.Serial, buffer, 20 );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Serial No: %s\n", buffer) );

	ConvertString( (PCHAR)buffer, (PCHAR)info.fw_rev, 8 );
	RtlCopyMemory( ideDisk->LuHwData.FW_Rev, buffer, 8 );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Firmware rev: %s\n", buffer) );

	ConvertString( (PCHAR)buffer, (PCHAR)info.model, 40 );
	RtlCopyMemory( ideDisk->LuHwData.Model, buffer, 40 );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Model No: %s\n", buffer) );

	// Bytes of sector

	ideDisk->LuHwData.BlockBytes = (UINT16)AtaGetBytesPerBlock(&info);

	NDAS_ASSERT( ideDisk->LuHwData.BlockBytes );

	// Support LBA?

	if (!(info.capability & 0x02)) {

		NDAS_ASSERT(FALSE);
		ideDisk->LuHwData.PduDescFlags &= ~PDUDESC_FLAG_LBA;
	
	} else {

		ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_LBA;
	}

	// Support LBA48.
	// Calc Capacity.

	if ((info.command_set_2 & 0x0400) && (info.cfs_enable_2 & 0x0400)) {	// Support LBA48bit
	
		ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_LBA48;
		ideDisk->LuHwData.SectorCount = info.lba_capacity_2;

	} else {

		ideDisk->LuHwData.PduDescFlags &= ~PDUDESC_FLAG_LBA48;

		if ((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {

			ideDisk->LuHwData.SectorCount = info.lba_capacity;
		
		} else {

			ideDisk->LuHwData.SectorCount = info.cyls * info.heads * info.sectors;
		}
	}

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
				("LBA %d, LBA48 %d, Number of Sectors: %I64d\n",
				(ideDisk->LuHwData.PduDescFlags & PDUDESC_FLAG_LBA) != 0,
				(ideDisk->LuHwData.PduDescFlags & PDUDESC_FLAG_LBA48) != 0,
				ideDisk->LuHwData.SectorCount) );

	// Support smart?

	if (info.command_set_1 & 0x0001) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Smart supported.\n") );
		
		ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_SMART_SUPPORT;
		
		if (info.cfs_enable_1 & 0x0001) {
		
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Smart enabled.\n") );
			ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_SMART_ENABLED;
		}
	}

	// Power management feature set enabled?

	if (info.command_set_1 & 0x0008) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Power management supported\n") );
	
		ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_POWERMGMT_SUPPORT;
		
		if (info.cfs_enable_1 & 0x0008) {

			ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_POWERMGMT_ENABLED;
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Power management enabled\n") );
		}
	}

	// Write cache enabled?

	if (info.command_set_1 & 0x20) {
	
		ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_WCACHE_SUPPORT;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Write cache supported\n") );

		if (info.cfs_enable_1 & 0x20) {

			ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_WCACHE_ENABLED;
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Write cache enabled\n") );
		}
	}

	// FUA ( force unit access ) feature set enabled?

	if (info.cfsse & 0x0040) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Native force unit access support.\n") );
		ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_FUA_SUPPORT;

		if (info.csf_default & 0x0040) {

			ideDisk->LuHwData.LudataFlags |= LUDATA_FLAG_FUA_ENABLED;
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Native force unit access enabled.\n") );
		}
	}

	return STATUS_SUCCESS;
}

VOID
IdeLurnClose (
	PLURELATION_NODE Lurn
	) 
{
	PLURNEXT_IDE_DEVICE	IdeDev = Lurn->IdeDisk;

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (IdeDev) {

		NDAS_ASSERT( IdeDev->PrimaryLanScsiSession.LanscsiProtocol == NULL );
		NDAS_ASSERT( IdeDev->CommLanScsiSession.LanscsiProtocol == NULL );

		if (IdeDev->CntEcrBuffer && IdeDev->CntEcrBufferLength) {
		
			ExFreePoolWithTag( IdeDev->CntEcrBuffer, LURNEXT_ENCBUFF_TAG );
			IdeDev->CntEcrBuffer = NULL;
		}

		if (IdeDev->WriteCheckBuffer) {
		
			ExFreePoolWithTag( IdeDev->WriteCheckBuffer, LURNEXT_WRITECHECK_BUFFER_POOLTAG );
			IdeDev->WriteCheckBuffer = NULL;
		}

		if (IdeDev->CntEcrKey) {

			CloseCipherKey( IdeDev->CntEcrKey );
		}

		if (IdeDev->CntEcrCipher) {

			CloseCipher( IdeDev->CntEcrCipher );
		}
	}

	if (Lurn->IdeDisk) {

		ExFreePoolWithTag( Lurn->IdeDisk, LURNEXT_POOL_TAG );
		Lurn->IdeDisk =NULL;
	}

	return;
}