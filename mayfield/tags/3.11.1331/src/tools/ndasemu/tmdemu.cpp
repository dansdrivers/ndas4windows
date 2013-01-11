/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/


#ifdef __TMD_SUPPORT__

#include "ndasemupriv.h"
#define __XPL_USE_NATIVE_PRIMITIVE_TYPES
#include "tmdcore\tmdlib.h"

typedef struct _TMDISK_DEVCTX {

	//
	//	Device ID
	//	Usually same as the unit disk number
	//

	ULONG			DeviceId;

	//
	//	Time machine disk
	//

	//	Time machine disk instance
	PTM_DISK		TmDisk;

	//	Time machine disk's client has 32 bit address limitation.
	BOOL			ClientAddr32;

	//
	//	Disk information
	//

	//	Support LBA ( original 28 bit address )
	BOOL			LBA;
	//	Support LBA48 ( 48 bit address )
	BOOL			LBA48;

	//	PIO mode register
	UINT16			PIO;
	//	Multi-word DMA mode register
	UINT16			MWDMA;
	//	Ultra DMA mode register
	UINT16			UDMA;

	//
	//	Lower device information
	//

	ULONG			LowerDiskBytesInBlockBitShift;
	UINT64			LowerDiskCapacity;
	BOOL			LowerDiskLBA48;
	ATADISK_INFO	LowerDiskInfo;

} TMDISK_DEVCTX, *PTMDISK_DEVCTX;


BOOL TMDiskInitialize(IN PNDEMU_DEV EmuDev, IN PNDEMU_DEV LowerDev, IN NDEMU_DEV_INIT EmuDevInit);
BOOL TMDiskDestroy(IN PNDEMU_DEV EmuDev);
BOOL TMDiskRequest(IN PNDEMU_DEV EmuDev, IN OUT PATA_COMMAND AtaCommand);

NDEMU_INTERFACE NdemuTMDiskInterface = {
	NDEMU_DEVTYPE_ATADISK,
	0,
	TMDiskInitialize,
	TMDiskDestroy,
	TMDiskRequest};

	
//////////////////////////////////////////////////////////////////////////
//
//	Translate between SCSI and ATA commands
//


__inline
BOOL
ConvertAtaCommand2ScsiCommand(
	IN PATA_COMMAND	AtaCommand,
	IN BOOL			Lba48,
	IN BOOL			ClientAddr32,
	OUT PXPL_SCSIIO	ScsiCommand
){
	BOOL	translateAddr = TRUE;

	//
	//	Zero out fields
	//

	ScsiCommand->SGBuffer = NULL;
	ScsiCommand->SenseData = NULL;
	ScsiCommand->SenseDataLen = 0;

	//
	//	Buffer
	//

	ScsiCommand->DataBuffer = AtaCommand->DataTransferBuffer;
	ScsiCommand->DataBufferLen = AtaCommand->BufferLength;


	//
	// Operation code
	// TMD modules treat 16 byte and 10 byte scsi operation code as the same.
	// We do not need to translate LBA48 command to 16 byte scsi operation code.
	//

	switch(AtaCommand->Command) {
		case WIN_READDMA:
		case WIN_READDMA_EXT:
				ScsiCommand->ScsiOp.OpCode = SCSIOP_READ;
			break;
		case WIN_WRITEDMA:
		case WIN_WRITEDMA_EXT:
				ScsiCommand->ScsiOp.OpCode = SCSIOP_WRITE;
			break;
		case WIN_VERIFY:
		case WIN_VERIFY_EXT:
				ScsiCommand->ScsiOp.OpCode = SCSIOP_VERIFY;
			break;
		case WIN_FLUSH_CACHE_EXT:
		case WIN_FLUSH_CACHE:
				ScsiCommand->ScsiOp.OpCode = SCSIOP_SYNCHRONIZE_CACHE;
			translateAddr = FALSE;
			break;
		default:
			return FALSE;
	}

	//
	//	Block address/length
	//

	if(translateAddr) {
		if(Lba48) {
			UINT64	tempAddr;

			tempAddr = ((UINT64)AtaCommand->LBAHigh_Prev << 40)
				+ ((UINT64)AtaCommand->LBAMid_Prev << 32)
				+ ((UINT64)AtaCommand->LBALow_Prev << 24)
				+ ((UINT64)AtaCommand->LBAHigh_Curr << 16)
				+ ((UINT64)AtaCommand->LBAMid_Curr << 8)
				+ ((UINT64)AtaCommand->LBALow_Curr);


			//
			//	If client software of TMD support only 32 bit,
			//	perform address range check and 32to64 translation
			//	

			if(ClientAddr32) {
				if(tempAddr >= 0xffffffff)
					return FALSE;
				ScsiCommand->ScsiOp.BlockAddress = TMDADDR_32TO64(tempAddr);
			} else {
				ScsiCommand->ScsiOp.BlockAddress = TMDADDR_48TO64(tempAddr);
			}

			ScsiCommand->ScsiOp.BlockLength = ((unsigned)AtaCommand->SectorCount_Prev << 8)
				+ (AtaCommand->SectorCount_Curr);


		} else {
			UINT32	tempAddr;

			 tempAddr = ((UINT32)AtaCommand->LBAHeadNR << 24)
				+ ((UINT32)AtaCommand->LBAHigh_Curr << 16)
				+ ((UINT32)AtaCommand->LBAMid_Curr << 8)
				+ ((UINT32)AtaCommand->LBALow_Curr);
			ScsiCommand->ScsiOp.BlockAddress = TMDADDR_28TO64(tempAddr);
			ScsiCommand->ScsiOp.BlockLength = AtaCommand->SectorCount_Curr;
		}
	}

	return TRUE;
}


__inline
BOOL
ConvertScsiCommand2AtaCommand(
	IN PXPL_SCSIIO		ScsiCommand,
	OUT PATA_COMMAND	AtaCommand,
	IN BOOL				Lba48
){
	BOOL	translateAddr = TRUE;

	//
	//	Buffer
	//

	AtaCommand->DataTransferBuffer = ScsiCommand->DataBuffer;
	AtaCommand->BufferLength = ScsiCommand->DataBufferLen;


	//
	// Operation code
	//

	switch(ScsiCommand->ScsiOp.OpCode) {
		case SCSIOP_READ:
		case SCSIOP_READ16:
			if(Lba48)
				AtaCommand->Command = WIN_READDMA_EXT;
			else
				AtaCommand->Command = WIN_READDMA;
			break;
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:
			if(Lba48)
				AtaCommand->Command = WIN_WRITEDMA_EXT;
			else
				AtaCommand->Command = WIN_WRITEDMA;
			break;
		case SCSIOP_VERIFY:
		case SCSIOP_VERIFY16:
			if(Lba48)
				AtaCommand->Command = WIN_VERIFY_EXT;
			else
				AtaCommand->Command = WIN_VERIFY;
			break;
		case SCSIOP_SYNCHRONIZE_CACHE:
		case SCSIOP_SYNCHRONIZE_CACHE16:
			AtaCommand->Command = WIN_FLUSH_CACHE;
			translateAddr = FALSE;
			break;
		default:
			return FALSE;
	}

	//
	//	Block address/length
	//

	if(translateAddr) {
		if(Lba48) {

			//
			//	Can not convert address over 48 bits
			//

			if(ScsiCommand->ScsiOp.BlockAddress + ScsiCommand->ScsiOp.BlockLength -1 >= 0xffffffffffff) {
				return FALSE;
			}

			AtaCommand->LBAHigh_Prev = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 40) & 0xff);
			AtaCommand->LBAMid_Prev = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 32) & 0xff);
			AtaCommand->LBALow_Prev = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 24) & 0xff);
			AtaCommand->LBAHigh_Curr = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 16) & 0xff);
			AtaCommand->LBAMid_Curr = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 8) & 0xff);
			AtaCommand->LBALow_Curr = (UINT8)(ScsiCommand->ScsiOp.BlockAddress & 0xff);

			AtaCommand->SectorCount_Prev = (UINT8)((ScsiCommand->ScsiOp.BlockLength >> 8) & 0xff);
			AtaCommand->SectorCount_Curr = (UINT8)(ScsiCommand->ScsiOp.BlockLength & 0xff);

		} else {

			//
			//	Can not convert address over 28 bits
			//

			if(ScsiCommand->ScsiOp.BlockAddress + ScsiCommand->ScsiOp.BlockLength -1 >= 0xfffffff) {
				return FALSE;
			}

			AtaCommand->LBAHeadNR = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 24) & 0xf);
			AtaCommand->LBAHigh_Prev = 0;
			AtaCommand->LBAMid_Prev = 0;
			AtaCommand->LBALow_Prev = 0;
			AtaCommand->LBAHigh_Curr = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 16) & 0xff);
			AtaCommand->LBAMid_Curr = (UINT8)((ScsiCommand->ScsiOp.BlockAddress >> 8) & 0xff);
			AtaCommand->LBALow_Curr = (UINT8)(ScsiCommand->ScsiOp.BlockAddress & 0xff);

			AtaCommand->SectorCount_Prev = 0;
			AtaCommand->SectorCount_Curr = (UINT8)(ScsiCommand->ScsiOp.BlockLength & 0xff);
		}
	}

	return TRUE;
}

__inline
VOID
ConvertAtaStatusToScsiStatus(
	IN PATA_COMMAND		AtaCommand,
	OUT UCHAR			SenseDataLen,
	OUT PXPLSENSE_DATA	SenseData,
	OUT PUCHAR			SrbStatus
){
	if(AtaCommand->Command & ERR_STAT) {
		*SrbStatus = XPLSRB_STATUS_ERROR;
	} else {
		*SrbStatus = XPLSRB_STATUS_SUCCESS;
	}

	UNREFERENCED_PARAMETER(SenseData);
	UNREFERENCED_PARAMETER(SenseDataLen);
}

//////////////////////////////////////////////////////////////////////////
//
//	TMD library callbacks
//	Exports to TMCore
//	Because TMCore utilizes SCSI command set, need to translate SCSI to ATA.
//


//
//	Flush a target object
//

XPLSTATUS
TmsFlushTargetObject(
	PTM_MEMBERDISK	TargetMem
){
	XPL_SCSIIO			scsiIo;
	XPLSENSE_DATA		senseData;

	XioInitializeScsiIo(&scsiIo, SCSIOP_SYNCHRONIZE_CACHE, 0, 0, 0, NULL, NULL,
						sizeof(XPLSENSE_DATA), &senseData);

	return TmsPerformBlockIoSync(
				TargetMem,
				TMD_NONAME_BLOCKFILE,
				&scsiIo,
				0,
				NULL,
				NULL);
}



XPLAPILINKAGE
XPLSTATUS
XPLAPI
TmsPerformBlockIo(
	IN PTM_MEMBERDISK				TargetMem,
	IN TMD_BLOCKFILE_NAME			BlockFileName,
	IN PXPL_SCSIIO					XplScsiCmd,
	IN UINT32						IoSysFlags,
	IN PXPL_ASYNCH_IO				AsynchIo,		OPTIONAL
	OUT	PUINT32						DoneBlocks,		OPTIONAL
	IN PVOID						PreallocIoReq	OPTIONAL
){
	BOOL		bret;
	PNDEMU_DEV	targetDisk;
	ATA_COMMAND	ataCommand;
	UINT32		result;
	XPLSTATUS	returnStatus;
	UCHAR		srbStatus;


	UNREFERENCED_PARAMETER(IoSysFlags);
	UNREFERENCED_PARAMETER(BlockFileName);
	UNREFERENCED_PARAMETER(PreallocIoReq);

	if(!XplScsiCmd->SenseData || !XplScsiCmd->SenseDataLen) {
		return XPLSTATUS_INVALID_PARAMETER;
	}


	//
	//	Cast an object to an emulation disk
	//

	targetDisk = (PNDEMU_DEV)TargetMem->AmdTargetObject;
	returnStatus = XPLSTATUS_SUCCESS;
	srbStatus = XPLSRB_STATUS_SUCCESS;

	bret = ConvertScsiCommand2AtaCommand(
					XplScsiCmd,
					&ataCommand,
					TargetMem->AmdAddrBitLen == TM_ADDRBIT_48);
	if(bret == FALSE) {
		return XPLSTATUS_NOT_SUPPORTED;
	}

	//
	//	send the IO request to the target object.
	//

	NdemuDevRequest(targetDisk, &ataCommand);
	ConvertAtaStatusToScsiStatus(
				&ataCommand,
				XplScsiCmd->SenseDataLen,
				XplScsiCmd->SenseData,
				&srbStatus);

	if(DoneBlocks) {
		*DoneBlocks = XplScsiCmd->ScsiOp.BlockLength;
	}

	if(srbStatus != XPLSRB_STATUS_SUCCESS) {
		returnStatus = XplNativeStatusToStatus(GetLastError());
		result = 0;
	} else {
		result = XplScsiCmd->ScsiOp.BlockLength * TargetMem->AmdBytesOfBlock;
	}


	//
	//	Set result for asynchronous IOs
	//

	if(AsynchIo) {


		//
		//	Asynchronous IO
		//

		XPLASSERT(!DoneBlocks);
		XPLASSERT(AsynchIo->SenseData == (PUCHAR)XplScsiCmd->SenseData);
		XPLASSERT(AsynchIo->SenseDataLen == XplScsiCmd->SenseDataLen);

		//
		//	Set return values
		//

		AsynchIo->ReturnStatus = returnStatus;
		AsynchIo->IoSysStatus = srbStatus;
		AsynchIo->IoHwStatus = XPLSCSISTAT_GOOD;
		AsynchIo->IoDoneByteLen = result;


		//
		//	Call the requestor's completion routine.
		//	This completion routine will free the asynchronous context.
		//

		AsynchIo->CompletionRoutine(
						(PXIO_DEVOBJ)targetDisk,
						AsynchIo);
	}


	return returnStatus;
}

//////////////////////////////////////////////////////////////////////////
//
//	ND emulator interface functions
//

BOOL
TMDiskInitialize(
	PNDEMU_DEV EmuDev,
	PNDEMU_DEV LowerDev,
	NDEMU_DEV_INIT EmuDevInit
){
	PNDEMU_TMDISK_INIT tmDiskInit = (PNDEMU_TMDISK_INIT)EmuDevInit;
	PTMDISK_DEVCTX tmDiskCtx;
	TM_MEMBERDISK	tmdMem[1];
	BOOL			bret;
	XPLSTATUS		status;


	//
	//	TM Disk can not stand alone.
	//

	if(LowerDev == NULL)
		return FALSE;

	EmuDev->LowerDevice = LowerDev;
	tmDiskCtx = (PTMDISK_DEVCTX)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TMDISK_DEVCTX));
	if(tmDiskCtx == NULL)
		return FALSE;

	//
	//	Initialize TMD variables
	//

	EmuDev->DevContext = (NDEMU_DEVCTX)tmDiskCtx;
	tmDiskCtx->DeviceId = tmDiskInit->DeviceId;
	tmDiskCtx->ClientAddr32 = TRUE;
	tmDiskCtx->LBA = TRUE;
	tmDiskCtx->LBA48 = TRUE;
	tmDiskCtx->PIO = 0;
	tmDiskCtx->MWDMA = 0x0007;
	tmDiskCtx->UDMA = 0x003f;


	//
	//	Retrieve lower device's information
	//

	bret = RetrieveAtaDiskInfo(
					EmuDev->LowerDevice,
					&tmDiskCtx->LowerDiskInfo,
					tmDiskCtx->LowerDiskInfo.sector_bytes);
	if(bret == FALSE) {
		HeapFree(GetProcessHeap(), 0, tmDiskCtx);
		return FALSE;
	}


	//
	//	Require the disk to support LBA and DMA
	//

	if(	(tmDiskCtx->LowerDiskInfo.capability & 0x1) &&
		(tmDiskCtx->LowerDiskInfo.capability & 0x2)) {

		//
		//	Require the disk to support 512, 1024, or 2048 sector size.
		//

		switch(tmDiskCtx->LowerDiskInfo.sector_bytes) {
			case 0:
			case 512:
				tmDiskCtx->LowerDiskBytesInBlockBitShift = 9;
				break;
			case 1024:
				tmDiskCtx->LowerDiskBytesInBlockBitShift = 10;
				break;
			case 2048:
				tmDiskCtx->LowerDiskBytesInBlockBitShift = 11;
				break;
			default:
				HeapFree(GetProcessHeap(), 0, tmDiskCtx);
				return FALSE;
		}

		//
		//	Determine the lower disk supports LBA48
		//

		if((tmDiskCtx->LowerDiskInfo.command_set_2 & 0x0400) == 0 ||
			(tmDiskCtx->LowerDiskInfo.cfs_enable_2 & 0x0400) == 0
			) {
			HeapFree(GetProcessHeap(), 0, tmDiskCtx);
			return FALSE;
		}
		tmDiskCtx->LowerDiskLBA48 = TRUE;

		//
		//	Determine disk capacity
		//

		if(tmDiskCtx->LowerDiskInfo.lba_capacity_2) {
			tmDiskCtx->LowerDiskCapacity = tmDiskCtx->LowerDiskInfo.lba_capacity_2;
		} else {
			tmDiskCtx->LowerDiskCapacity = tmDiskCtx->LowerDiskInfo.lba_capacity;
		}

		fprintf(stderr, "TMDiskInitialize: Lower disk capacity=%I64u blocks\n",
							tmDiskCtx->LowerDiskCapacity);

	} else {
		fprintf(stderr, "TMDiskInitialize: Lower disk does not support DMA and LBA\n");

		HeapFree(GetProcessHeap(), 0, tmDiskCtx);
		return FALSE;
	}

	//
	//	Create a TMD instance
	//

	tmdMem[0].AmdMemberId = 0;
	tmdMem[0].AmdTargetObject = EmuDev->LowerDevice;
	tmdMem[0].AmdRawDiskLen	= tmDiskCtx->LowerDiskCapacity;
	tmdMem[0].AmdBytesOfBlock = 1 << tmDiskCtx->LowerDiskBytesInBlockBitShift;
	tmdMem[0].AmdMaxReqBlock = 128;
	tmdMem[0].AmdSrbFlags = 0;
	if(tmDiskCtx->LowerDiskLBA48)
		tmdMem[0].AmdAddrBitLen = TM_ADDRBIT_48;
	else
		tmdMem[0].AmdAddrBitLen = TM_ADDRBIT_32;

	status = TmdCreate(
						&tmDiskCtx->TmDisk,
						0,
						0,
						0,
						TMDCREATE_MODE_SERVER,
						1,
						tmdMem);
	if(!XPL_SUCCESS(status)) {
		HeapFree(GetProcessHeap(), 0, tmDiskCtx);
		bret = FALSE;
	}

	//
	//	Try to enable TMD
	//
	status = TmdStart(tmDiskCtx->TmDisk);
	if(!XPL_SUCCESS(status)) {
		fprintf(stderr, "Failed initial start. Try later. STATUS=%08lx\n", status);
	} else {
		fprintf(stderr, "TMD module started.\n");
	}

	return bret;
}

BOOL
TMDiskDestroy(
	PNDEMU_DEV EmuDev
){
	PTMDISK_DEVCTX tmDiskCtx;

	if(EmuDev == NULL)
		return FALSE;
	if(EmuDev->DevContext == NULL)
		return FALSE;

	tmDiskCtx = (PTMDISK_DEVCTX)EmuDev->DevContext;

	TmdStop(tmDiskCtx->TmDisk);
	TmdDestroy(tmDiskCtx->TmDisk);
	HeapFree(GetProcessHeap(), 0, tmDiskCtx);

	return TRUE;
}


BOOL
TMDiskRequest(
	PNDEMU_DEV EmuDev,
	PATA_COMMAND AtaCommand
){
	PTMDISK_DEVCTX		tmDiskCtx = (PTMDISK_DEVCTX)EmuDev->DevContext;
	BOOL				bret;
	XPL_SCSIIO			ScsiCommand;
	TMD_IOREQUEST		ioReq;
	XPLSTATUS			status;
	ULONG				ioMode;


	switch(AtaCommand->Command) {
		case WIN_READDMA:
		case WIN_READDMA_EXT:
		case WIN_WRITEDMA:
		case WIN_WRITEDMA_EXT:
		case WIN_VERIFY:
		case WIN_VERIFY_EXT:
		case WIN_FLUSH_CACHE:
		case WIN_FLUSH_CACHE_EXT:
			bret = ConvertAtaCommand2ScsiCommand(
						AtaCommand,
						tmDiskCtx->LBA48,
						tmDiskCtx->ClientAddr32,
						&ScsiCommand
				);
			if(bret == FALSE) {
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ABRT_ERR;
				break;
			}

			//
			//	Detect addressing mode.
			//

			ioMode = TMDADDR_MODE(ScsiCommand.ScsiOp.BlockAddress);
			ScsiCommand.ScsiOp.BlockAddress = TMDADDR_STRIP(ScsiCommand.ScsiOp.BlockAddress);

			TcuInitializeIoRequest(
				&ioReq,
				(PXIO_REQ)AtaCommand,
				NULL,
				NULL,
				ScsiCommand.ScsiOp.BlockAddress,
				ScsiCommand.ScsiOp.BlockLength,
				ScsiCommand.DataBuffer,
				ScsiCommand.DataBufferLen,
				ScsiCommand.ScsiOp.OpCode,
				0,
				ioMode);

			status = TmdExecuteIoSvr(tmDiskCtx->TmDisk, &ioReq);
			if(!XPL_SUCCESS(status) || !XPL_SUCCESS(ioReq.AiResult.ReturnStatus)) {
				AtaCommand->Command = ERR_STAT;
				AtaCommand->Feature_Curr = ABRT_ERR;
				break;
			}

			AtaCommand->Command = 0;
			AtaCommand->Feature_Curr = 0;
		break;
		case WIN_SETFEATURES:
		{
			struct	hd_driveid	*pInfo;
			int Feature, Mode;

			fprintf(stderr, "set features ");
			pInfo = (struct hd_driveid *)AtaCommand->DataTransferBuffer;
			Feature = AtaCommand->Feature_Curr;
			Mode = AtaCommand->SectorCount_Curr;

			switch(Feature) {
				case SETFEATURES_XFER:
					fprintf(stderr, "SETFEATURES_XFER %x\n", Mode);
					if((Mode & 0xf0) == 0x00) {			// PIO

						tmDiskCtx->PIO &= 0x00ff;
						tmDiskCtx->PIO |= (1 << ((Mode & 0xf) + 8));

					} else if((Mode & 0xf0) == 0x20) {	// Multi-word DMA

						tmDiskCtx->MWDMA &= 0x00ff;
						tmDiskCtx->MWDMA |= (1 << ((Mode & 0xf) + 8));

					} else if((Mode & 0xf0) == 0x40) {	// Ultra DMA

						tmDiskCtx->UDMA &= 0x00ff;
						tmDiskCtx->UDMA |= (1 << ((Mode & 0xf) + 8));

					} else {
						fprintf(stderr, "XFER unknown mode %x\n", Mode);
						AtaCommand->Command = ERR_STAT;
						AtaCommand->Feature_Curr = ABRT_ERR;
						break;
					}

					AtaCommand->Command = 0;
					AtaCommand->Feature_Curr = 0;
					break;
				default:
					fprintf(stderr, "Unknown feature %d\n", Feature);
					AtaCommand->Command = ERR_STAT;
					AtaCommand->Feature_Curr = ABRT_ERR;
					break;
			}
		}
		break;
		case WIN_IDENTIFY:
		case WIN_PIDENTIFY:	{
			struct	hd_driveid	*pInfo;
			char	serial_no[20] = { '0', '0', '0', '0', 0};
			char	firmware_rev[8] = {'.', '0', 0, '0', 0, };
			char	model[40] = { 'M', 'T', 'i', 'D', 'k', 's', 'm', 'E', ' ', 'u', 0, };

			fprintf(stderr, "IDENTIFY/PIDENTIFY: DeviceId=%d\n", (UINT)tmDiskCtx->DeviceId);

			pInfo = (struct hd_driveid *)AtaCommand->DataTransferBuffer;

			if(TmdIsStarted(tmDiskCtx->TmDisk)) {
				pInfo->lba_capacity = (unsigned int)TmdGetHVDiskLen(tmDiskCtx->TmDisk);
				pInfo->lba_capacity_2 = TmdGetHVDiskLen(tmDiskCtx->TmDisk);
			} else {
				pInfo->lba_capacity = (unsigned int)tmDiskCtx->LowerDiskCapacity;
				pInfo->lba_capacity_2 = tmDiskCtx->LowerDiskCapacity;
			}

			pInfo->heads = 255;
			pInfo->sectors = 63;
			pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
			if(tmDiskCtx->LBA)	// LBA
				pInfo->capability |= 0x0002;
			if(tmDiskCtx->LBA48) { // LBA48
				pInfo->cfs_enable_2 |= 0x0400;
				pInfo->command_set_2 |= 0x0400;
			}
			pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;	// ATAPI 5
			pInfo->dma_mword = tmDiskCtx->MWDMA;
			pInfo->dma_ultra = tmDiskCtx->UDMA;
			memcpy(pInfo->serial_no, serial_no, 20);
			memcpy(pInfo->fw_rev, firmware_rev, 8);
			memcpy(pInfo->model, model, 40);


			//
			//	Set return register values
			//

			AtaCommand->Command = 0;
			AtaCommand->Feature_Curr = 0;
		}
		break;
		case WIN_SETMULT:
			fprintf(stderr, "set multiple mode\n");
			AtaCommand->Command = 0;
			AtaCommand->Feature_Curr = 0;
		break;
		case WIN_CHECKPOWERMODE1: {
			int Mode;

			Mode = AtaCommand->SectorCount_Curr;
			fprintf(stderr, "check power mode = 0x%02x\n", Mode);
			AtaCommand->Command = 0;
			AtaCommand->Feature_Curr = 0;
		}
		break;
		case WIN_STANDBY:
			fprintf(stderr, "standby\n");
			AtaCommand->Command = 0;
			AtaCommand->Feature_Curr = 0;
		break;
		default:
			//
			//	Simply relay all other commands
			//
			fprintf(stderr, "COMMAND:%x\n", AtaCommand->Command);
			NdemuDevRequest(EmuDev->LowerDevice, AtaCommand);
	}


	return TRUE;
}


#endif