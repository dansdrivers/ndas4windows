/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"

typedef struct _ATADISK_DEVCTX {
	ULONG		DeviceId;
	HANDLE		DiskFileHandle;


	//
	//	Disk characteristic
	//

	UINT64			Capacity;
	UINT16			BytesInBlock;
	UINT16			BytesInBlockBitShift;
	BOOL			LBA;
	BOOL			LBA48;
	unsigned short	PIO;
	unsigned short	MWDMA;
	unsigned short	UDMA;

} ATADISK_DEVCTX, *PATADISK_DEVCTX;

BOOL AtaDiskInitialize(IN PNDEMU_DEV EmuDev, IN PNDEMU_DEV LowerDev, IN NDEMU_DEV_INIT EmuDevInit);
BOOL AtaDiskDestroy(IN PNDEMU_DEV EmuDev);
BOOL AtaDiskRequest(IN PNDEMU_DEV EmuDev, IN OUT PATA_COMMAND AtaCommand);


NDEMU_INTERFACE NdemuAtaDiskInterface = {
						NDEMU_DEVTYPE_ATADISK,
						0,
						AtaDiskInitialize,
						AtaDiskDestroy,
						AtaDiskRequest};

//
//	ND emulator interface functions
//
#define ATADISK_FILENAME_FORMAT	_T("AtaDisk%u")

BOOL
AtaDiskInitialize(
	PNDEMU_DEV		EmuDev,
	PNDEMU_DEV		LowerDev,
	NDEMU_DEV_INIT	EmuDevInit
){
	PNDEMU_ATADISK_INIT	ataDiskInit = (PNDEMU_ATADISK_INIT)EmuDevInit;
	PATADISK_DEVCTX		ataDiskCtx;
	LONGLONG			loc;
	TCHAR				fileNameBuffer[64];
	BOOL				initDib;
	BOOL				bret;
	LARGE_INTEGER		distanceToMove;

	if(EmuDev == NULL)
		return FALSE;
	if(EmuDevInit == NULL)
		return FALSE;
	if(ataDiskInit->Capacity == 0)
		return FALSE;
	if(ataDiskInit->BytesInBlock == 0)
		return FALSE;

	//
	//	ATA disk only can stand alone.
	//

	if(LowerDev)
		return FALSE;

	EmuDev->LowerDevice = LowerDev;
	ataDiskCtx = (PATADISK_DEVCTX)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ATADISK_DEVCTX));
	if(ataDiskCtx == NULL)
		return FALSE;

	initDib = FALSE;

	EmuDev->DevContext = (NDEMU_DEVCTX)ataDiskCtx;
	ataDiskCtx->DeviceId = ataDiskInit->DeviceId;
	ataDiskCtx->Capacity = ataDiskInit->Capacity;
	ataDiskCtx->BytesInBlock = ataDiskInit->BytesInBlock;
	ataDiskCtx->BytesInBlockBitShift = ataDiskInit->BytesInBlockBitShift;
	ataDiskCtx->LBA = TRUE;
	ataDiskCtx->LBA48 = TRUE;
	ataDiskCtx->PIO = 0;
	ataDiskCtx->MWDMA = 0x0407;
	ataDiskCtx->UDMA = 0x003f;



	//
	//	Try to open the disk file for this unit disk.
	//
	_sntprintf(fileNameBuffer, 64, ATADISK_FILENAME_FORMAT, ataDiskCtx->DeviceId);
	ataDiskCtx->DiskFileHandle = CreateFile(
					fileNameBuffer, 
					GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL,
					NULL
					);
	if(ataDiskCtx->DiskFileHandle == INVALID_HANDLE_VALUE) {

		//
		//	Create a file if not exist.
		//

		ataDiskCtx->DiskFileHandle = CreateFile(
					fileNameBuffer, 
					GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL
					);
		if(ataDiskCtx->DiskFileHandle == INVALID_HANDLE_VALUE) {
			_tprintf(_T("Can not open file '%s'\n"), fileNameBuffer);
			return 1;
		}


		//
		//	Initialize DIB of newly created disk
		//

		initDib = TRUE;
	}
	if(ataDiskInit->UseSparseFile == TRUE) {
		bret = SetSparseFile(ataDiskCtx->DiskFileHandle);
		if(bret == FALSE) {
			_tprintf(_T("Can not create sparse file '%s'\n"), fileNameBuffer);
			return 1;
		}
	}

	//
	//	Set initial disk size
	//

	distanceToMove.QuadPart = ataDiskCtx->Capacity << ataDiskCtx->BytesInBlockBitShift;
	bret = SetFilePointerEx(
						ataDiskCtx->DiskFileHandle,
						distanceToMove,
						NULL,
						FILE_BEGIN);
	if(bret == FALSE) {
		return FALSE;
	}

	printf("Loc : %I64d\n", distanceToMove.QuadPart);
	bret = SetEndOfFile(ataDiskCtx->DiskFileHandle);
	if(bret == FALSE) {
		perror( "Can not write initial DIB" );
		return FALSE;
	}

	return TRUE;
}

BOOL
AtaDiskDestroy(
	PNDEMU_DEV EmuDev
){
	PATADISK_DEVCTX		ataDiskCtx = (PATADISK_DEVCTX)EmuDev->DevContext;

	if(EmuDev == NULL)
		return FALSE;
	if(ataDiskCtx->DiskFileHandle)
		CloseHandle(ataDiskCtx->DiskFileHandle);
	if(ataDiskCtx)
		HeapFree(GetProcessHeap(), 0, ataDiskCtx);

	return TRUE;
}

BOOL
AtaDiskRequest(
	IN PNDEMU_DEV		EmuDev,
	IN OUT PATA_COMMAND	AtaCommand
){
	PATADISK_DEVCTX		ataDiskCtx = (PATADISK_DEVCTX)EmuDev->DevContext;
	BOOL				bret;
	UINT64		location;
	unsigned	sectorCount;

	//
	//	Verify register values
	//

	// LBA48 command? 
	if((ataDiskCtx->LBA48 == FALSE) &&
		((AtaCommand->Command == WIN_READDMA_EXT)
		|| (AtaCommand->Command == WIN_WRITEDMA_EXT))) {
			fprintf(stderr, "AtaDiskRequest: Bad Command. LBA48 command to non-LBA48 device\n");
			AtaCommand->Command = ERR_STAT;
			AtaCommand->Feature_Curr = ABRT_ERR;
			return TRUE;
	}


	//
	// Convert location and Sector Count.
	//
	location = 0;
	sectorCount = 0;

	if(ataDiskCtx->LBA48 == TRUE) {
		location = ((UINT64)AtaCommand->LBAHigh_Prev << 40)
				+ ((UINT64)AtaCommand->LBAMid_Prev << 32)
				+ ((UINT64)AtaCommand->LBALow_Prev << 24)
				+ ((UINT64)AtaCommand->LBAHigh_Curr << 16)
				+ ((UINT64)AtaCommand->LBAMid_Curr << 8)
				+ ((UINT64)AtaCommand->LBALow_Curr);
		sectorCount = ((unsigned)AtaCommand->SectorCount_Prev << 8)
				+ (AtaCommand->SectorCount_Curr);
	} else {
		location = ((UINT64)AtaCommand->LBAHeadNR << 24)
			+ ((UINT64)AtaCommand->LBAHigh_Curr << 16)
			+ ((UINT64)AtaCommand->LBAMid_Curr << 8)
			+ ((UINT64)AtaCommand->LBALow_Curr);

		sectorCount = AtaCommand->SectorCount_Curr;
	}



	switch(AtaCommand->Command) {
	case WIN_READDMA:
	case WIN_READDMA_EXT:
		{
//			fprintf(stderr, "R");
//			fprintf(stderr, "READ: location 0x%I64x, Sector Count %u...\n", location, sectorCount);

			//
			// Check Bound.
			//
			if(location + sectorCount > ataDiskCtx->Capacity) 
			{
				fprintf(stderr, "ATADISK:READ: location = 0x%I64x, Sector_Size = %u, DeviceId = %d, Out of bound\n",
							location + sectorCount,
							ataDiskCtx->Capacity,
							ataDiskCtx->DeviceId);
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ID_ERR;
				break;
			}
			//
			//	Check buffer length
			//
			if((sectorCount << ataDiskCtx->BytesInBlockBitShift) > AtaCommand->BufferLength) {
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ABRT_ERR;
				fprintf(stderr, "ATADISK:READ: Buffer too small.\n");
				break;
			}


			//
			//	Read blocks
			//

			if(sectorCount != 0) {
				LARGE_INTEGER	distanceToMove;
				DWORD			readBytes;

				distanceToMove.QuadPart = location<<ataDiskCtx->BytesInBlockBitShift;
				bret = SetFilePointerEx(
									ataDiskCtx->DiskFileHandle,
									distanceToMove,
									NULL,
									FILE_BEGIN);
				if(bret == FALSE) {
					return FALSE;
				}

				bret = ReadFile(	ataDiskCtx->DiskFileHandle,
									AtaCommand->DataTransferBuffer,
									sectorCount<<ataDiskCtx->BytesInBlockBitShift,
									&readBytes,
									NULL);
				if(bret == FALSE) {
					fprintf(stderr, "ATADISK:READ: ReadFile() failed. Blk addr=%x len=%x\n",
									location, sectorCount);
					return FALSE;
				}
				if(readBytes != (sectorCount<<ataDiskCtx->BytesInBlockBitShift)) {
					fprintf(stderr, "ATADISK:READ: returned small data. readBytes=%x\n",
						readBytes);
					return FALSE;
				}
			}

			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_WRITEDMA:
	case WIN_WRITEDMA_EXT:
		{
//						fprintf(stderr, "W");
//						fprintf(stderr, "WRITE: location 0x%I64x, Sector Count %u...\n", location, sectorCount);

			//
			// Check Bound.
			//
			if(location + sectorCount > ataDiskCtx->Capacity) 
			{
				fprintf(stderr, "ATADISK:WRITE: location = 0x%I64x, Sector_Size = %u, DeviceId = %d, Out of bound\n",
							location + sectorCount,
							ataDiskCtx->Capacity,
							ataDiskCtx->DeviceId);
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ID_ERR;
				break;
			}
			//
			//	Check buffer length
			//
			if((sectorCount << ataDiskCtx->BytesInBlockBitShift) > AtaCommand->BufferLength) {
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ABRT_ERR;
				fprintf(stderr, "ATADISK:WRITE: Buffer too small.\n");
				break;
			}

			//
			//	Write blocks
			//

			if(sectorCount != 0) {
				LARGE_INTEGER	distanceToMove;
				DWORD			writtenBytes;

				distanceToMove.QuadPart = location<<ataDiskCtx->BytesInBlockBitShift;
				bret = SetFilePointerEx(
									ataDiskCtx->DiskFileHandle,
									distanceToMove,
									NULL,
									FILE_BEGIN);
				if(bret == FALSE) {
					return FALSE;
				}

				bret = WriteFile(	ataDiskCtx->DiskFileHandle,
									AtaCommand->DataTransferBuffer,
									sectorCount<<ataDiskCtx->BytesInBlockBitShift,
									&writtenBytes,
									NULL);
				if(bret == FALSE) {
					fprintf(stderr, "ATADISK:WRITE: WriteFile() failed. Blk addr=%x len=%x\n",
						location, sectorCount);
					return FALSE;
				}
				if(writtenBytes != (sectorCount<<ataDiskCtx->BytesInBlockBitShift)) {
					fprintf(stderr, "ATADISK:WRITE: returned small data. readBytes=%x\n",
						writtenBytes);
					return FALSE;
				}
			}

			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_VERIFY:
	case WIN_VERIFY_EXT:
		{
			fprintf(stderr, "Verify: location %I64d, Sector Count %d...\n", location, sectorCount);
			
			//
			// Check Bound.
			//
			if(location + sectorCount > ataDiskCtx->Capacity) 
			{
				fprintf(stderr, "VERIFY: location = %lld, Sector_Size = %lld, DeviceId = %d, Out of bound\n",
					location + sectorCount,
					ataDiskCtx->Capacity,
					ataDiskCtx->DeviceId);
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ID_ERR;
				break;
			}

			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_SETFEATURES:
		{
			struct	hd_driveid	*pInfo;
			int Feature, Mode;
			int targetId;

			fprintf(stderr, "SETFEATURES: ");
			pInfo = (struct hd_driveid *)AtaCommand->DataTransferBuffer;
			Feature = AtaCommand->Feature_Curr;
			Mode = AtaCommand->SectorCount_Curr;

			//
			//	Init return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
			switch(Feature) {
				case SETFEATURES_XFER:
					fprintf(stderr, "SETFEATURES_XFER %x\n", Mode);
					if((Mode & 0xf0) == 0x00) {			// PIO

						ataDiskCtx->PIO &= 0x00ff;
						ataDiskCtx->PIO |= (1 << ((Mode & 0xf) + 8));

					} else if((Mode & 0xf0) == 0x20) {	// Multi-word DMA

						ataDiskCtx->MWDMA &= 0x00ff;
						ataDiskCtx->MWDMA |= (1 << ((Mode & 0xf) + 8));

					} else if((Mode & 0xf0) == 0x40) {	// Ultra DMA

						ataDiskCtx->UDMA &= 0x00ff;
						ataDiskCtx->UDMA |= (1 << ((Mode & 0xf) + 8));

					} else {
						fprintf(stderr, "XFER unknown mode %x\n", Mode);
						AtaCommand->Command = ERR_STAT;
						AtaCommand->Feature_Curr = ABRT_ERR;
						break;
					}
					break;
				default:
					fprintf(stderr, "Unknown feature %d\n", Feature);
					AtaCommand->Command = ERR_STAT;
					AtaCommand->Feature_Curr = ABRT_ERR;
					break;
			}
		}
		break;

	case WIN_SETMULT:
		{
			fprintf(stderr, "set multiple mode\n");
			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_CHECKPOWERMODE1:
		{
			int Mode;

			Mode = AtaCommand->SectorCount_Curr;
			fprintf(stderr, "check power mode = 0x%02x\n", Mode);
			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_STANDBY:
		{
			fprintf(stderr, "standby\n");
			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_IDENTIFY:
	case WIN_PIDENTIFY:
		{
			struct	hd_driveid	*pInfo;
			char	serial_no[20] = { '0', '0', '0', '0', 0};
			char	firmware_rev[8] = {'.', '0', 0, '0', 0, };
			char	model[40] = { 'T', 'A', 'D', 'A', 's', 'i', 'E', 'k', 'u', 'm', 0, };

			fprintf(stderr, "IDENTIFY/PIDENTIFY: DeviceId=%d\n", (UINT)ataDiskCtx->DeviceId);

			pInfo = (struct hd_driveid *)AtaCommand->DataTransferBuffer;
			memset(pInfo, 0, sizeof(struct hd_driveid));
			pInfo->sector_bytes = ataDiskCtx->BytesInBlock;
			pInfo->lba_capacity = (unsigned int)ataDiskCtx->Capacity;
			pInfo->lba_capacity_2 = ataDiskCtx->Capacity;
			pInfo->heads = 255;
			pInfo->sectors = 63;
			pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
			pInfo->capability |= 0x0001;		// DMA
			if(ataDiskCtx->LBA)
				pInfo->capability |= 0x0002;	// LBA
			if(ataDiskCtx->LBA48) { // LBA48
				pInfo->cfs_enable_2 |= 0x0400;
				pInfo->command_set_2 |= 0x0400;
			}
			pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;	// ATAPI 5
			pInfo->dma_mword = ataDiskCtx->MWDMA;
			pInfo->dma_ultra = ataDiskCtx->UDMA;
			memcpy(pInfo->serial_no, serial_no, 20);
			memcpy(pInfo->fw_rev, firmware_rev, 8);
			memcpy(pInfo->model, model, 40);


			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	case WIN_FLUSH_CACHE_EXT:
	case WIN_FLUSH_CACHE: {
			fprintf(stderr, "FLUSH...\n");
			//
			//	Set return register values
			//

			AtaCommand->Command = READY_STAT;
			AtaCommand->Feature_Curr = 0;
		}
		break;
	default:
		fprintf(stderr, "Not Supported Command1 0x%x\n", AtaCommand->Command);
		AtaCommand->Command = ERR_STAT;
		AtaCommand->Feature_Curr = ABRT_ERR;
		break;
	}

	return TRUE;
}
