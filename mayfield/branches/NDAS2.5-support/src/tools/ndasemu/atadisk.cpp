/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"

typedef struct _ATADISK_DEVCTX {
	ULONG		DeviceId;
	INT			DiskFileHandle;


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

extern BOOL EiReadHang;
extern BOOL EiReadBadSector;
extern BOOL EiWriteBadSector;
extern BOOL EiVerifyBadSector;
extern UINT64 EiErrorLocation;
extern UINT32 EiErrorLength;
extern UINT32 OptVerbose;

NDEMU_INTERFACE NdemuAtaDiskInterface = {
						NDEMU_DEVTYPE_ATADISK,
						0,
						AtaDiskInitialize,
						AtaDiskDestroy,
						AtaDiskRequest};

//
//	ND emulator interface functions
//
#define ATADISK_FILENAME_FORMAT	"AtaDisk%u"


BOOL IsInErrorRange(UINT64 Start, UINT32 Length)
{
	if (Start+Length-1 < EiErrorLocation)
		return FALSE;
	if (EiErrorLocation+EiErrorLength-1<Start)
		return FALSE;
	return TRUE;
}

BOOL
AtaDiskInitialize(
	PNDEMU_DEV		EmuDev,
	PNDEMU_DEV		LowerDev,
	NDEMU_DEV_INIT	EmuDevInit
){
	PNDEMU_ATADISK_INIT	ataDiskInit = (PNDEMU_ATADISK_INIT)EmuDevInit;
	PATADISK_DEVCTX		ataDiskCtx;
	PCHAR				buffer;
	INT					iret;
	LONGLONG			loc;
	CHAR				fileNameBuffer[64];

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
	//	Open the disk file for this unit disk.
	//
	_snprintf(fileNameBuffer, 64, ATADISK_FILENAME_FORMAT, ataDiskCtx->DeviceId);

	ataDiskCtx->DiskFileHandle = _open(fileNameBuffer, _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
	if(ataDiskCtx->DiskFileHandle < 0) {
		char	buffer[512];
		_int64	loc;

		//
		//	Create a file.
		//

		ataDiskCtx->DiskFileHandle = _open(fileNameBuffer, _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
		if(ataDiskCtx->DiskFileHandle < 0) {
			printf("Can not open file '%s'\n", fileNameBuffer);
			return 1;
		}

	}

	//
	//	Write init DIB
	//
	buffer = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ataDiskCtx->BytesInBlock);
	loc = _lseeki64(	ataDiskCtx->DiskFileHandle,
						(ataDiskCtx->Capacity - 1) << ataDiskCtx->BytesInBlockBitShift,
						SEEK_SET);

	printf("Loc : %I64d\n", loc);
	iret = _write(ataDiskCtx->DiskFileHandle, buffer, ataDiskCtx->BytesInBlock);
	if(iret == -1) {
		perror( "Can not write ND" );
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
		_close(ataDiskCtx->DiskFileHandle);
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
	UINT64		location;
	unsigned	sectorCount;
	BOOL	bRet = TRUE;
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
		location = ((_int64)AtaCommand->LBAHigh_Prev << 40)
				+ ((_int64)AtaCommand->LBAMid_Prev << 32)
				+ ((_int64)AtaCommand->LBALow_Prev << 24)
				+ ((_int64)AtaCommand->LBAHigh_Curr << 16)
				+ ((_int64)AtaCommand->LBAMid_Curr << 8)
				+ ((_int64)AtaCommand->LBALow_Curr);
		sectorCount = ((unsigned)AtaCommand->SectorCount_Prev << 8)
				+ (AtaCommand->SectorCount_Curr);
	} else {
		location = ((_int64)AtaCommand->LBAHeadNR << 24)
			+ ((_int64)AtaCommand->LBAHigh_Curr << 16)
			+ ((_int64)AtaCommand->LBAMid_Curr << 8)
			+ ((_int64)AtaCommand->LBALow_Curr);

		sectorCount = AtaCommand->SectorCount_Curr;
	}



	switch(AtaCommand->Command) {
	case WIN_READDMA:
	case WIN_READDMA_EXT:
		{
			if (OptVerbose == 2) {
				fprintf(stderr, "R");
			} else if (OptVerbose > 2) {
				fprintf(stderr, "READ(0x%I64x,%u)\n", location, sectorCount);
			}

			//
			// Check Bound.
			//
			if(location + sectorCount > ataDiskCtx->Capacity) 
			{
				fprintf(stderr, "READ: location = 0x%I64x, Sector_Size = %u, DeviceId = %d, Out of bound\n",
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
				break;
			}


			if (EiReadHang && IsInErrorRange(location, sectorCount)) {
				fprintf(stderr, "Error Injection: Hanging when accessing sector %d, length %d\n", location, sectorCount);
				Sleep(60 * 1000);
				bRet = FALSE;
				break;
			}

			if (EiReadBadSector && IsInErrorRange(location, sectorCount)) {
				fprintf(stderr, "Error Injection: Returning bad sector when accessing sector %d, length %d\n", location, sectorCount);
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ECC_ERR;
				break;
			}

			//
			//	Read blocks
			//

			if(sectorCount != 0) {
				_lseeki64(ataDiskCtx->DiskFileHandle, location<<ataDiskCtx->BytesInBlockBitShift, SEEK_SET);
				_read(	ataDiskCtx->DiskFileHandle,
					AtaCommand->DataTransferBuffer,
					sectorCount<<ataDiskCtx->BytesInBlockBitShift);
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
			if (OptVerbose == 2) {
				fprintf(stderr, "W");
			} else if (OptVerbose >= 3) {
				fprintf(stderr, "WRITE(0x%I64x,%u)\n", location, sectorCount);
			}

			//
			// Check Bound.
			//
			if(location + sectorCount > ataDiskCtx->Capacity) 
			{
				fprintf(stderr, "WRITE: location = 0x%I64x, Sector_Size = %u, DeviceId = %d, Out of bound\n",
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
				break;
			}
			if (EiWriteBadSector && IsInErrorRange(location, sectorCount)) {
				fprintf(stderr, "Error Injection: Returning bad sector when writing sector %I64d, length %d\n", location, sectorCount);
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ECC_ERR;
				break;
			}

			//
			//	Write blocks
			//

			if(sectorCount != 0) {
				_lseeki64(ataDiskCtx->DiskFileHandle, location  << ataDiskCtx->BytesInBlockBitShift, SEEK_SET);
				_write(ataDiskCtx->DiskFileHandle, AtaCommand->DataTransferBuffer, sectorCount << ataDiskCtx->BytesInBlockBitShift);
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

			if (EiVerifyBadSector && IsInErrorRange(location, sectorCount)) {
				fprintf(stderr, "Error Injection: Returing bad sector for verifying sector %I64d, length %d\n", location, sectorCount);
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ECC_ERR;
				// Set location of error
				if (AtaCommand->Command == WIN_VERIFY) {
					AtaCommand->LBALow_Curr = (_int8)(EiErrorLocation);
					AtaCommand->LBAMid_Curr = (_int8)(EiErrorLocation >> 8);
					AtaCommand->LBAHigh_Curr = (_int8)(EiErrorLocation >> 16);
					AtaCommand->LBAHeadNR = (_int8)(EiErrorLocation >> 24);
					AtaCommand->SectorCount_Curr = (_int8)0; // reserved
				} else if (AtaCommand->Command == WIN_VERIFY_EXT){
					AtaCommand->LBALow_Curr = (_int8)(EiErrorLocation);
					AtaCommand->LBAMid_Curr = (_int8)(EiErrorLocation >> 8);
					AtaCommand->LBAHigh_Curr = (_int8)(EiErrorLocation >> 16);
					AtaCommand->LBALow_Prev = (_int8)(EiErrorLocation >> 24);
					AtaCommand->LBAMid_Prev = (_int8)(EiErrorLocation >> 32);
					AtaCommand->LBAHigh_Prev = (_int8)(EiErrorLocation >> 40);

					AtaCommand->SectorCount_Curr = (_int8)0; // reserved
					AtaCommand->SectorCount_Prev = (_int8)0; // reserved
				}
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

			fprintf(stderr, "set features ");
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

	return bRet;
}
