#include "ndasscsiproc.h"


#if _WIN32_WINNT <= 0x500
#define NTSTRSAFE_LIB
#endif
#include <ntstrsafe.h>


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSUtils"


#if __PASS_DIB_CRC32_CHK__
#error "DIB CRC check must be enabled."
#endif

#if __PASS_RMD_CRC32_CHK__
#error "RMD CRC check must be enabled."
#endif


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi Protocol
//
NTSTATUS
LsuConnectLogin(
	OUT PLANSCSI_SESSION		LanScsiSession,
	IN  PTA_LSTRANS_ADDRESS		TargetAddress,
	IN  PTA_LSTRANS_ADDRESS		BindingAddress,
	IN  PLSSLOGIN_INFO			LoginInfo,
	OUT PLSTRANS_TYPE			LstransType

) {
	NTSTATUS		status;
	LARGE_INTEGER	defaultTimeOut;
	LSPROTO_TYPE	LSProto;

	//
	//	Confirm address type.
	//	Connect to the Lanscsi Node.
	//

	status = LstransAddrTypeToTransType( BindingAddress->Address[0].AddressType, LstransType);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}


	//
	//	Set timeouts
	//	Extend generic timeout
	//

	RtlZeroMemory(LanScsiSession, sizeof(LANSCSI_SESSION));
	defaultTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT * 15 / 10;
	LspSetDefaultTimeOut(LanScsiSession, &defaultTimeOut);
	status = LspConnect(
					LanScsiSession,
					BindingAddress,
					TargetAddress,
					NULL,
					NULL
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}

	//
	//	Login to the Lanscsi Node.
	//
	status = LspLookupProtocol(LoginInfo->HWType, LoginInfo->HWVersion, &LSProto);
	if(!NT_SUCCESS(status)) {
		goto error_out;
	}

	status = LspLogin(
					LanScsiSession,
					LoginInfo,
					LSProto,
					NULL,
					TRUE
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspLogin(), Can't login to the LS node with UserID:%08lx. STATUS:0x%08x.\n", LoginInfo->UserID, status));
		status = STATUS_ACCESS_DENIED;
		goto error_out;
	}

error_out:
	return status;

}


NTSTATUS
LsuQueryBindingAddress(
	OUT PTA_LSTRANS_ADDRESS		BoundAddr,
	IN  PTA_LSTRANS_ADDRESS		TargetAddr,
	IN  PTA_LSTRANS_ADDRESS		BindingAddr,
	IN  PTA_LSTRANS_ADDRESS		BindingAddr2,
	IN	BOOLEAN					BindAnyAddr
) {
	NTSTATUS			status;
	LARGE_INTEGER		defaultTimeOut;
	LSTRANS_TYPE		lstransType;
	PLANSCSI_SESSION	LSS;

	//
	//	Confirm address type.
	//	Connect to the Lanscsi Node.
	//

	status = LstransAddrTypeToTransType( TargetAddr->Address[0].AddressType, &lstransType);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		return STATUS_INVALID_ADDRESS;
	}


	//
	//	Allocate a Lanscsi session
	//

	LSS = (PLANSCSI_SESSION)ExAllocatePoolWithTag(NonPagedPool, sizeof(LANSCSI_SESSION), LSS_BUFFER_POOLTAG);
	if(!LSS) {
		KDPrintM(DBG_OTHER_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Set timeouts.
	//

	RtlZeroMemory(LSS, sizeof(LANSCSI_SESSION));
	defaultTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetDefaultTimeOut(LSS, &defaultTimeOut);
	status = LspConnectMultiBindAddr(
					LSS,
					BoundAddr,
					BindingAddr,
					BindingAddr2,
					TargetAddr,
					BindAnyAddr,
					NULL
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspConnectMultiBindAddr(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}


error_out:
	LspDisconnect(LSS);

	if(LSS)
		ExFreePool(LSS);

	return status;

}

//////////////////////////////////////////////////////////////////////////
//
//	IDE devices
//

VOID 
FORCEINLINE 
GetUlongFromArray(const UCHAR* UCharArray, PULONG ULongValue)
{
	*ULongValue = 0;
	*ULongValue |= UCharArray[0];
	*ULongValue <<= 8;
	*ULongValue |= UCharArray[1];
	*ULongValue <<= 8;
	*ULongValue |= UCharArray[2];
	*ULongValue <<= 8;
	*ULongValue |= UCharArray[3];
}

ULONG
AtaGetBytesPerBlock(struct hd_driveid *IdentifyData)
{
	/* word 106
	106  Physical sector size / Logical Sector Size
	F 15 Shall be cleared to zero
	F 14 Shall be set to one
	F 13 1 = Device has multiple logical sectors per physical sector.
	12 1 = Device Logical Sector Longer than 256 Words
	F 11-4 Reserved
	F 3-0 2^X logical sectors per physical sector
	*/

	typedef struct _ATA_SECTOR_SIZE_DATA {  // endian independent
		UCHAR Reserved1 : 4;             /* 8 - 11 */
		UCHAR LargeLogicalSector    : 1; /* 12 */
		UCHAR MultipleLogicalSector : 1; /* 13 */
		UCHAR IsOne  : 1;                /* 14 */
		UCHAR IsZero : 1;                /* 15 */
		UCHAR LogicalSectorsPerPhysicalSectorInPowerOfTwo : 4; /* 0 - 3 */
		UCHAR Reserved2 : 4;             /* 4 - 7 */
	} ATA_SECTOR_SIZE_DATA, *PATA_SECTOR_SIZE_DATA;

	C_ASSERT(sizeof(ATA_SECTOR_SIZE_DATA) == 2);
	//
	// If 15 is zero and 14 is one, this information is valid
	//
	PATA_SECTOR_SIZE_DATA sectorSizeData = (PATA_SECTOR_SIZE_DATA)
		&IdentifyData->sector_info;

	ULONG bytesPerBlock = 512;

	if (1 == sectorSizeData->IsOne && 0 == sectorSizeData->IsZero)
	{
		if (sectorSizeData->LargeLogicalSector)
		{
			// Words 117,118 indicate the size of device logical sectors in words. 
			// The value of words 117,118 shall be equal to or greater than 256. 
			// The value in words 117,118 shall be valid when word 106 bit 12 
			// is set to 1. All logical sectors on a device shall be 
			// 117,118 words long.
			GetUlongFromArray(
				(PUCHAR)&IdentifyData->logical_sector_size,
				&bytesPerBlock);
		}
	}

	return bytesPerBlock;
}

NTSTATUS
LsuConfigureIdeDisk(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			UdmaRestrict,
	OUT PULONG			PduFlags,
	OUT PBOOLEAN		Dma,
	OUT PULONG			BlockBytes
){
	NTSTATUS			status;
	struct hd_driveid	info;
	ULONG				pduFlags;
	BOOLEAN				setDmaMode;
	ULONG				blockBytes;
	LANSCSI_PDUDESC		pduDesc;
	UCHAR				pduResponse;
	LONG				retryRequest;

	//
	//	Get identify
	// Try identify command several times.
	// A hard disk with long spin-up time might make the NDAS chip non-responsive.
	// Hard disk spin-down -> NDASBUS requests sector IO -> Hard disk spin-up ->
	// -> NDAS chip not response -> NDASBUS enters connection with another NIC ->
	// -> Identify in LsuConfigureIdeDisk() -> May be error return.
	//
	//

	for(retryRequest = 0; retryRequest < 2; retryRequest ++) {
		status = LsuGetIdentify(LSS, &info);
		if(NT_SUCCESS(status)) {
			break;
		}
		KDPrint(1, ("LsuGetIdentify(1) failed. NTSTATUS: %08lx. retry\n", status));
	}
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("LsuGetIdentify(2) failed. NTSTATUS: %08lx\n", status));
		return status;
	}

	//
	// IO mode: DMA/PIO Mode.
	//
	KDPrintM(DBG_OTHER_INFO, ("Major 0x%x, Minor 0x%x, Capa 0x%x\n",
							info.major_rev_num,
							info.minor_rev_num,
							info.capability));
	KDPrintM(DBG_OTHER_INFO, ("DMA 0x%x, U-DMA 0x%x\n",
							info.dma_mword,
							info.dma_ultra));

	//
	//	determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions and disk capacity.
	//
	setDmaMode = FALSE;
	pduFlags = 0;
	blockBytes = 512;

	do {
		UCHAR	DmaFeature;
		UCHAR	DmaMode;

		DmaFeature = 0;
		DmaMode = 0;

		//
		// Ultra DMA if NDAS chip is 2.0 or higher.
		//
		/*
			We don't support UDMA for 2.0 rev 0 due to the bug.
			The bug : Written data using UDMA will be corrupted
		*/
		if (
			(info.dma_ultra & 0x00ff) &&
			(
				(LANSCSIIDE_VERSION_2_0 < LSS->HWVersion) ||
				((LANSCSIIDE_VERSION_2_0 == LSS->HWVersion) && (0 != LSS->HWRevision))
			))
		{
			// Find Fastest Mode.
			if(info.dma_ultra & 0x0001)
				DmaMode = 0;
			if(info.dma_ultra & 0x0002)
				DmaMode = 1;
			if(info.dma_ultra & 0x0004)
				DmaMode = 2;
			//	if Cable80, try higher Ultra Dma Mode.
#if __DETECT_CABLE80__
			if(info.hw_config & 0x2000) {
#endif
				if(info.dma_ultra & 0x0008)
					DmaMode = 3;
				if(info.dma_ultra & 0x0010)
					DmaMode = 4;
				if(info.dma_ultra & 0x0020)
					DmaMode = 5;
				if(info.dma_ultra & 0x0040)
					DmaMode = 6;
				if(info.dma_ultra & 0x0080)
					DmaMode = 7;

				//
				// If the ndas device is version 2.0 revision 100Mbps,
				// Restrict UDMA to mode 2.
				//

				if (LSS->HWVersion == LANSCSIIDE_VERSION_2_0 && 
					LSS->HWRevision == LANSCSIIDE_VER20_REV_100M) {

					if(DmaMode > 2)
						DmaMode = 2;
				}

				//
				// Restrict UDMA mode when requested.
				//

				if(UdmaRestrict != 0xff) {
					if(DmaMode > UdmaRestrict) {
						DmaMode = UdmaRestrict;
						KDPrintM(DBG_LURN_INFO, ("UDMA restriction applied. UDMA=%d\n", (ULONG)DmaMode));
					}
				}

#if __DETECT_CABLE80__
			}
#endif
			KDPrintM(DBG_OTHER_INFO, ("Ultra DMA %d detected.\n", (int)DmaMode));
			DmaFeature = DmaMode | 0x40;	// Ultra DMA mode.
			pduFlags |= PDUDESC_FLAG_DMA|PDUDESC_FLAG_UDMA;

			// Set Ultra DMA mode if needed
			if(!(info.dma_ultra & (0x0100 << DmaMode))) {
				setDmaMode = TRUE;
			}

		//
		//	detect DMA
		//	Lower 8 bits of dma_mword represent supported dma modes.
		//
		} else if(info.dma_mword & 0x00ff) {
			if(info.dma_mword & 0x0001)
				DmaMode = 0;
			if(info.dma_mword & 0x0002)
				DmaMode = 1;
			if(info.dma_mword & 0x0004)
				DmaMode = 2;

			KDPrintM(DBG_OTHER_INFO, ("DMA mode %d detected.\n", (int)DmaMode));
			DmaFeature = DmaMode | 0x20;
			pduFlags |= PDUDESC_FLAG_DMA;

			// Set DMA mode if needed
			if(!(info.dma_mword & (0x0100 << DmaMode))) {
				setDmaMode = TRUE;
			}

		}

		// Set DMA mode if needed.
		if(setDmaMode) {
			LSS_INITIALIZE_PDUDESC(LSS, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, 0, NULL, NULL);
			pduDesc.Feature = SETFEATURES_XFER;
			pduDesc.BlockCount = DmaFeature;
			status = LspRequest(LSS, &pduDesc, &pduResponse);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_OTHER_ERROR, ("Set Feature Failed...\n"));
				return status;
			}
			if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_OTHER_ERROR, ("SETFEATURES: PduResponse=%x\n", (ULONG)pduResponse));
				return STATUS_UNSUCCESSFUL;
			}

			// identify.
			status = LsuGetIdentify(LSS, &info);
			if(!NT_SUCCESS(status)) {
				KDPrint(1, ("LsuGetIdentify(3) failed. NTSTATUS: %08lx.\n", status));
				return status;
			}
			KDPrintM(DBG_OTHER_INFO, ("After Set Feature DMA 0x%x, U-DMA 0x%x\n",
							info.dma_mword,
							info.dma_ultra));
		}
		if(pduFlags & PDUDESC_FLAG_DMA) {
			break;
		}
		//
		//	PIO.
		//
		KDPrintM(DBG_OTHER_ERROR, ("NetDisk does not support DMA mode. Turn to PIO mode.\n"));
		pduFlags |= PDUDESC_FLAG_PIO;

	} while(0);


	//
	// Bytes of sector
	//

	blockBytes = AtaGetBytesPerBlock(&info);

	//
	// LBA support
	//

	if(!(info.capability & 0x02)) {
		pduFlags &= ~PDUDESC_FLAG_LBA;
		ASSERT(FALSE);
	} else {
		pduFlags |= PDUDESC_FLAG_LBA;
	}

	//
	// LBA48 support
	//

	if(info.command_set_2 & 0x0400 || info.cfs_enable_2 & 0x0400) {	// Support LBA48bit
		pduFlags |= PDUDESC_FLAG_LBA48;

		//
		//	If LBA48 is on, LBA is also on.
		//

		pduFlags |= PDUDESC_FLAG_LBA;
	}

	KDPrint(1, ("LBA support: LBA %d, LBA48 %d\n",
		(pduFlags & PDUDESC_FLAG_LBA) != 0,
		(pduFlags & PDUDESC_FLAG_LBA48) != 0
		));


	//
	//	Set return values
	//

	if(PduFlags) {
		*PduFlags = pduFlags;
	}
	if(Dma) {
		if(pduFlags & (PDUDESC_FLAG_DMA|PDUDESC_FLAG_UDMA)) {
			*Dma = TRUE;
		} else {
			*Dma = FALSE;
		}
	}
	if(BlockBytes) {
		*BlockBytes = blockBytes;
	}

	return status;
}


NTSTATUS
LsuGetIdentify(
	PLANSCSI_SESSION	LSS,
	struct hd_driveid	*info
){
	NTSTATUS		status;
	LANSCSI_PDUDESC	PduDesc;
	BYTE			PduResponse;

	status = STATUS_SUCCESS;

	// identify.
	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_IDENTIFY, 0, 0, 1, sizeof(struct hd_driveid), info, NULL);
	status = LspRequest(LSS, &PduDesc, &PduResponse);
	if(!NT_SUCCESS(status) || PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_OTHER_ERROR, ("Identify Failed...\n"));
		return NT_SUCCESS(status)?STATUS_UNSUCCESSFUL:status;
	}

	return status;
}

NTSTATUS
LsuReadBlocks(
	PLANSCSI_SESSION	LSS,
	PBYTE				Buffer,
	UINT64				LogicalBlockAddress,
	ULONG				TransferBlocks,
	ULONG				BlockBytes,
	ULONG				PduFlags
) {
	NTSTATUS		status;
	LANSCSI_PDUDESC	PduDesc;
	BYTE			PduResponse;

	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_READ, PduFlags, LogicalBlockAddress, TransferBlocks, TransferBlocks * BlockBytes, Buffer, NULL);
	status = LspRequest(
					LSS,
					&PduDesc,
					&PduResponse
				);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x\n", 
			LogicalBlockAddress, TransferBlocks));

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x PduResponse:%x\n", 
			LogicalBlockAddress, TransferBlocks, PduResponse));

		status = STATUS_REQUEST_NOT_ACCEPTED;
	}

	return status;
}


NTSTATUS
LsuWriteBlocks(
	IN PLANSCSI_SESSION	LSS,
	IN PBYTE			Buffer,
	IN UINT64			LogicalBlockAddress,
	IN ULONG			TransferBlocks,
	IN ULONG			BlockBytes,
	IN ULONG			PduFlags
) {
	NTSTATUS		status;
	LANSCSI_PDUDESC	PduDesc;
	BYTE			PduResponse;

	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_WRITE, PduFlags, LogicalBlockAddress, TransferBlocks, TransferBlocks * BlockBytes, Buffer, NULL);
	status = LspRequest(
					LSS,
					&PduDesc,
					&PduResponse
				);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x\n", 
			LogicalBlockAddress, TransferBlocks));

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x PduResponse:%x\n", 
			LogicalBlockAddress, TransferBlocks, PduResponse));

		status = STATUS_REQUEST_NOT_ACCEPTED;
	}

	return status;
}

NTSTATUS
LsuGetDiskInfoBlockV1(
	IN PLANSCSI_SESSION		LSS,
	OUT PNDAS_DIB			DiskInformationBlock,
	IN UINT64				LogicalBlockAddress,
	IN ULONG				BlockBytes,
	IN ULONG				PduFlags
) {
	NTSTATUS	status;

	status = LsuReadBlocks(LSS, (PBYTE)DiskInformationBlock, LogicalBlockAddress, 1, BlockBytes, PduFlags);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if( DiskInformationBlock->Signature != NDAS_DIB_SIGNATURE ||
		IS_NDAS_DIBV1_WRONG_VERSION(*DiskInformationBlock) ) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: Revision mismatch. Signature:0x%08lx, Revision:%d.%d\n",
							DiskInformationBlock->Signature,
							DiskInformationBlock->MajorVersion,
							DiskInformationBlock->MinorVersion
				));
	}

	return status;
}


NTSTATUS
LsuGetDiskInfoBlockV2(
	IN PLANSCSI_SESSION		LSS,
	OUT PNDAS_DIB_V2		DiskInformationBlock,
	IN UINT64				LogicalBlockAddress,
	IN ULONG				BlockBytes,
	IN ULONG				PduFlags
) {
	NTSTATUS	status;

	status = LsuReadBlocks(LSS, (PBYTE)DiskInformationBlock, LogicalBlockAddress, 1, BlockBytes, PduFlags);
	if(!NT_SUCCESS(status)) {
		return status;
	}


	//
	//	Revision check
	//

	if( DiskInformationBlock->Signature != NDAS_DIB_V2_SIGNATURE ||
		IS_HIGHER_VERSION_V2(*DiskInformationBlock) ) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: Revision mismatch. Signature:0x%08lx, Revision:%u.%u\n",
							DiskInformationBlock->Signature,
							DiskInformationBlock->MajorVersion,
							DiskInformationBlock->MinorVersion
				));
	}


	//
	//	CRC check.
	//

	if(NT_SUCCESS(status)) {
		if(IS_DIB_CRC_VALID(crc32_calc, *DiskInformationBlock) != TRUE) {
			KDPrintM(DBG_OTHER_ERROR, ("DIBv2's crc is invalid.\n"));
			status = STATUS_REVISION_MISMATCH;
		}
	}

	return status;
}

NTSTATUS
LsuGetBlockACL(
	IN PLANSCSI_SESSION				LSS,
	OUT PBLOCK_ACCESS_CONTROL_LIST	BlockACL,
	IN ULONG						BlockACLLen,
	IN UINT64						LogicalBlockAddress,
	IN ULONG						BlockBytes,
	IN ULONG						PduFlags
) {
	NTSTATUS	status;
	ULONG		readBlock;
	PBYTE		buffer;

	ASSERT(BlockBytes);
	ASSERT(BlockACLLen);

	//
	//	allocate block buffer
	//

	readBlock = (BlockACLLen + BlockBytes) / BlockBytes;
	buffer = (PBYTE)ExAllocatePoolWithTag(NonPagedPool, readBlock * BlockBytes, LSU_POOLTAG_BLOCKBUFFER);
	if(buffer == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	status = LsuReadBlocks(LSS, buffer, LogicalBlockAddress, readBlock, BlockBytes, PduFlags);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(buffer, LSU_POOLTAG_BLOCKBUFFER);
		return status;
	}

	//
	//	Copy to the caller's buffer
	//

	RtlCopyMemory(BlockACL, buffer, BlockACLLen);

	//
	//	Free the buffer
	//

	ExFreePoolWithTag(buffer, LSU_POOLTAG_BLOCKBUFFER);


	//
	//	Revision check
	//

	if( BlockACL->Signature != BACL_SIGNATURE ||
		BlockACL->Version != BACL_VERSION) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_OTHER_ERROR, 
			("Error: Revision mismatch. Signature:0x%08lx, Version:%u\n",
							BlockACL->Signature,
							BlockACL->Version));
	}


	//
	//	CRC check.
	//

	if(NT_SUCCESS(status)) {
		UINT32 crc;
		
		crc = crc32_calc((PUCHAR)BlockACL->Elements,
			BlockACL->ElementCount * sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT));

		if(crc != BlockACL->crc) {
			KDPrintM(DBG_OTHER_ERROR, ("BACL's crc is invalid.\n"));
			return STATUS_REVISION_MISMATCH;
		}
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
//	Event log
//
static
VOID
_WriteLogErrorEntry(
	IN PDEVICE_OBJECT			DeviceObject,
	IN PLSU_ERROR_LOG_ENTRY		LogEntry
){
	PIO_ERROR_LOG_PACKET errorLogEntry;
	WCHAR					strBuff[16];
	NTSTATUS				status;
	ULONG					stringOffset;
	ULONG_PTR				stringLen;
	ULONG					idx_dump;

	//
	//	Parameter to unicode string
	//
	ASSERT(LogEntry->DumpDataEntry <= LSU_MAX_ERRLOG_DATA_ENTRIES);
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = RtlStringCchPrintfW(strBuff, 16, L"%u", LogEntry->Parameter2);
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("RtlStringCchVPrintfW() failed.\n"));
		return;
	}

	status = RtlStringCchLengthW(strBuff, 16, &stringLen);
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("RtlStringCchLengthW() failed.\n"));
		return;
	}

	//
	//	Translate unicode length into byte length including NULL termination.
	//

	stringLen = ( stringLen + 1 ) * sizeof(WCHAR);
	stringOffset = FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + LogEntry->DumpDataEntry * sizeof(ULONG);


	errorLogEntry = (PIO_ERROR_LOG_PACKET)
					IoAllocateErrorLogEntry(
									DeviceObject,
									(sizeof(IO_ERROR_LOG_PACKET) +
									(LogEntry->DumpDataEntry * sizeof(ULONG)) +
									(UCHAR)stringLen));
	if(errorLogEntry == NULL) {
		KDPrint(1, ("Could not allocate error log entry.\n"));
		ASSERT(FALSE);
		return ;
	}

	errorLogEntry->ErrorCode = LogEntry->ErrorCode;
	errorLogEntry->MajorFunctionCode = LogEntry->MajorFunctionCode;
	errorLogEntry->IoControlCode = LogEntry->IoctlCode;
	errorLogEntry->EventCategory;
	errorLogEntry->SequenceNumber = LogEntry->SequenceNumber;
	errorLogEntry->RetryCount = (UCHAR) LogEntry->ErrorLogRetryCount;
	errorLogEntry->UniqueErrorValue = LogEntry->UniqueId;
	errorLogEntry->FinalStatus = STATUS_SUCCESS;
	errorLogEntry->DumpDataSize = LogEntry->DumpDataEntry * sizeof(ULONG);
	for(idx_dump=0; idx_dump < LogEntry->DumpDataEntry; idx_dump++) {
		errorLogEntry->DumpData[idx_dump] = LogEntry->DumpData[idx_dump];
	}

	errorLogEntry->NumberOfStrings = 1;
	errorLogEntry->StringOffset = (USHORT)stringOffset;
	RtlCopyMemory((PUCHAR)errorLogEntry + stringOffset, strBuff, stringLen);

	IoWriteErrorLogEntry(errorLogEntry);

	return;
}

typedef struct _LSU_ERRORLOGCTX {

	PIO_WORKITEM		IoWorkItem;
	LSU_ERROR_LOG_ENTRY	ErrorLogEntry;

} LSU_ERRORLOGCTX, *PLSU_ERRORLOGCTX;


static
VOID
WriteErrorLogWorker(
 IN PDEVICE_OBJECT	DeviceObject,
 IN PVOID			Context
){
	PLSU_ERRORLOGCTX	errorLogCtx = (PLSU_ERRORLOGCTX)Context;

	UNREFERENCED_PARAMETER(DeviceObject);

	_WriteLogErrorEntry(DeviceObject, &errorLogCtx->ErrorLogEntry);

	IoFreeWorkItem(errorLogCtx->IoWorkItem);
	ExFreePoolWithTag(errorLogCtx, LSU_POOLTAG_ERRORLOGWORKER);

}

VOID
LsuWriteLogErrorEntry(
	IN PDEVICE_OBJECT		DeviceObject,
    IN PLSU_ERROR_LOG_ENTRY ErrorLogEntry
){


	if(KeGetCurrentIrql() > PASSIVE_LEVEL) {
		PIO_WORKITEM	workitem;
		PLSU_ERRORLOGCTX	context;

		context = ExAllocatePoolWithTag(NonPagedPool, sizeof(LSU_ERRORLOGCTX), LSU_POOLTAG_ERRORLOGWORKER);
		if(context == NULL) {
			KDPrintM(DBG_OTHER_ERROR, ("Allocating context failed.\n"));
			return;
		}

		RtlCopyMemory(&context->ErrorLogEntry, ErrorLogEntry, sizeof(LSU_ERROR_LOG_ENTRY));


		workitem = IoAllocateWorkItem(DeviceObject);
		if(workitem == NULL) {
			KDPrintM(DBG_OTHER_ERROR, ("IoAllocateWorkItem() failed.\n"));
			return;
		}

		context->IoWorkItem = workitem;

		IoQueueWorkItem(workitem, WriteErrorLogWorker, DelayedWorkQueue, context);

	} else {
		_WriteLogErrorEntry(DeviceObject, ErrorLogEntry);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//	TDI client
//
#define LSTRANS_LPX_POWERDOWN_TIMEOUT				(NANO100_PER_SEC*5)

//
//	Global variables
// TDI Client is created for device driver object.
//

struct _TDICLIENT_CONTEXT {
	HANDLE			BindingHandle;
	LONG			TdiPnPInit;

	KSPIN_LOCK		TdiPnPSpinLock;
	LONG			ClientDeviceCount;
	LONG			ClientInProgressIOCount;
	LARGE_INTEGER	LastOperationTime;
} TDICLIENT_CONTEXT;


VOID
LsuClientPnPBindingChange(
	IN TDI_PNP_OPCODE  PnPOpcode,
	IN PUNICODE_STRING  DeviceName,
	IN PWSTR  MultiSZBindList
);

//
//    get the current system clock
//

static
__inline
VOID
LsuCurrentTime(
	PLARGE_INTEGER	CurrentTime
){
	ULONG    	Tick;

	KeQueryTickCount(CurrentTime);
	Tick = KeQueryTimeIncrement();
	CurrentTime->QuadPart = CurrentTime->QuadPart * Tick;
}



/*++

Routine Description:

Register PnP handler routines with TDI

Arguments:

TdiClientName: set to the address of a buffered Unicode string
specifying the client's named key
in the registry HKLM\System\CCS\Services tree.

TdiClient: Handle for this PnP registration


Return Value:

NTSTATUS -- Indicates whether registration succeeded

--*/

NTSTATUS
LsuRegisterTdiPnPHandler(
	IN PUNICODE_STRING			TdiClientName,
	IN TDI_BINDING_HANDLER		TdiBindHandler,
	TDI_ADD_ADDRESS_HANDLER_V2 TdiAddAddrHandler,
	TDI_DEL_ADDRESS_HANDLER_V2 TdiDelAddrHandler,
	IN TDI_PNP_POWER_HANDLER	TdiPnPPowerHandler,
	OUT PHANDLE					TdiClient
){
    NTSTATUS                    status;
    TDI_CLIENT_INTERFACE_INFO   info;
	LONG						tdiPnp;

	//
	//	Do not allow more than one TdiPnP registration
	//

	tdiPnp = InterlockedExchange(&TDICLIENT_CONTEXT.TdiPnPInit, 1);
	if(tdiPnp) {
		ASSERT(FALSE);
		return STATUS_DEVICE_ALREADY_ATTACHED;
	}


	//
	//	Init spin lock protecting TdiPnP global variables.
	//

	KeInitializeSpinLock(&TDICLIENT_CONTEXT.TdiPnPSpinLock);
 
    //
    // Setup the TDI request structure
    //

    RtlZeroMemory (&info, sizeof (info));
#ifdef TDI_CURRENT_VERSION
    info.TdiVersion = TDI_CURRENT_VERSION;
#else
    info.MajorTdiVersion = 2;
    info.MinorTdiVersion = 0;
#endif
    info.Unused = 0;
    info.ClientName = TdiClientName;
    info.BindingHandler = TdiBindHandler;
    info.AddAddressHandlerV2 = TdiAddAddrHandler;
    info.DelAddressHandlerV2 = TdiDelAddrHandler;
    info.PnPPowerHandler = TdiPnPPowerHandler;


	//
    // Register handlers with TDI
    //

    status = TdiRegisterPnPHandlers (&info, sizeof(info), &TDICLIENT_CONTEXT.BindingHandle);
    if (!NT_SUCCESS (status)) {
		KDPrintM(DBG_OTHER_ERROR, (
					"Failed to register PnP handlers: %lx .\n",
					status));
        return status;
    }
	*TdiClient = TDICLIENT_CONTEXT.BindingHandle;
	KDPrintM(DBG_OTHER_INFO, ("TDI PnP registered.\n"));

    return STATUS_SUCCESS;
}

//
// Deregister Tdi client
//

VOID
LsuDeregisterTdiPnPHandlers(
	HANDLE			TdiClient
){
    if (TdiClient) {
		ASSERT(TdiClient == TDICLIENT_CONTEXT.BindingHandle);
		TdiDeregisterPnPHandlers(TdiClient);
		KDPrintM(DBG_OTHER_INFO, ("TDI PnP deregistered.\n"));
	}
}


//
//	Increment counter of TDI client device.
//

VOID
LsuIncrementTdiClientDevice() {
	KIRQL	oldIrql;

	ACQUIRE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, &oldIrql);

	ASSERT(TDICLIENT_CONTEXT.ClientDeviceCount >= 0);
	TDICLIENT_CONTEXT.ClientDeviceCount++;

	RELEASE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, oldIrql);

}


//
// Increment counter of TDI client device.
//

VOID
LsuDecrementTdiClientDevice() {
	KIRQL	oldIrql;

	ACQUIRE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, &oldIrql);

	TDICLIENT_CONTEXT.ClientDeviceCount--;
	ASSERT(TDICLIENT_CONTEXT.ClientDeviceCount >= 0);

	RELEASE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, oldIrql);
}

//
//	Increment In-progress operation counter of TDI client.
//

VOID
LsuIncrementTdiClientInProgress() {
	KIRQL	oldIrql;

	ACQUIRE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, &oldIrql);

	ASSERT(TDICLIENT_CONTEXT.ClientInProgressIOCount >= 0);
	TDICLIENT_CONTEXT.ClientInProgressIOCount++;

	RELEASE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, oldIrql);

}


//
//	Increment In-progress operation counter of TDI client.
//

VOID
LsuDecrementTdiClientInProgress() {
	KIRQL	oldIrql;

	ACQUIRE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, &oldIrql);

	TDICLIENT_CONTEXT.ClientInProgressIOCount--;
	ASSERT(TDICLIENT_CONTEXT.ClientInProgressIOCount >= 0);

	LsuCurrentTime(&TDICLIENT_CONTEXT.LastOperationTime);

	RELEASE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, oldIrql);
}

VOID
LsuClientPnPBindingChange(
	IN TDI_PNP_OPCODE  PnPOpcode,
	IN PUNICODE_STRING  DeviceName,
	IN PWSTR  MultiSZBindList
){
	UNREFERENCED_PARAMETER(DeviceName);
	UNREFERENCED_PARAMETER(MultiSZBindList);

#if DBG
	if(DeviceName && DeviceName->Buffer) {
		KDPrintM(DBG_OTHER_ERROR, ("DeviceName=%ws PnpOpcode=%x\n", DeviceName->Buffer, PnPOpcode));
	} else {
		KDPrintM(DBG_OTHER_ERROR, ("DeviceName=NULL PnpOpcode=%x\n", PnPOpcode));
	}
#endif

	switch(PnPOpcode) {
	case TDI_PNP_OP_ADD:
		KDPrintM(DBG_OTHER_INFO, ("TDI_PNP_OP_ADD\n"));
	break;
	case TDI_PNP_OP_DEL:
		KDPrintM(DBG_OTHER_INFO, ("TDI_PNP_OP_DEL\n"));
	break;
	case TDI_PNP_OP_PROVIDERREADY:
		KDPrintM(DBG_OTHER_INFO, ("TDI_PNP_OP_PROVIDERREADY\n"));
	break;
	case TDI_PNP_OP_NETREADY:
		KDPrintM(DBG_OTHER_INFO, ("TDI_PNP_OP_NETREADY\n"));
	break;
	default:
		KDPrintM(DBG_OTHER_ERROR, ("Unknown PnP code. %x\n", PnPOpcode));
	}
}


//
//	PnP power event handler for use of delaying NIC power-down.
//

NTSTATUS
LsuClientPnPPowerChange(
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
){
	NTSTATUS				status;
	UNICODE_STRING			lpxPrefix;
	NET_DEVICE_POWER_STATE	powerState;

	UNREFERENCED_PARAMETER(Context1);
	UNREFERENCED_PARAMETER(Context2);

	if (DeviceName==NULL) {
		KDPrintM(DBG_OTHER_ERROR, (
			"NO DEVICE NAME SUPPLIED when power event of type %x.\n",
			PowerEvent->NetEvent));
		return STATUS_SUCCESS;
	}

	if(PowerEvent == NULL) {
		return STATUS_SUCCESS;
	}

	if(PowerEvent->Buffer == NULL ||
		PowerEvent->BufferLength == 0) {

				powerState = NetDeviceStateUnspecified;
	} else {
		powerState = *((PNET_DEVICE_POWER_STATE) PowerEvent->Buffer);
	}

#if DBG
	if(DeviceName->Buffer) {
		KDPrintM(DBG_OTHER_ERROR, ("DeviceName=%ws PowerEvent=%x BufferLen=%u PowerStat=%x\n",
			DeviceName->Buffer,
			PowerEvent->NetEvent,
			PowerEvent->BufferLength,
			powerState));
	} else {
		KDPrintM(DBG_OTHER_ERROR, ("DeviceName=NULL PowerEvent=%x BufferLen=%u PowerStat=%x\n",
			PowerEvent->NetEvent,
			PowerEvent->BufferLength,
			powerState));
	}
#endif
	//
	//	We support LPX for now.
	//	We need to support TCP/IP for the future.
	//

	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);
	if(	DeviceName == NULL || RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) == FALSE) {

		KDPrintM(DBG_OTHER_ERROR, (
			"Not LPX binding device.\n"));

		return STATUS_SUCCESS;
	}

	status = STATUS_SUCCESS;
	switch(PowerEvent->NetEvent) {
		case NetEventSetPower:
			KDPrintM(DBG_OTHER_INFO, ("SetPower\n"));
#if 0
			if(powerState != NetDeviceStateD0) {
				LARGE_INTEGER	interval;
				LONG			loopCnt;
				KIRQL			oldIrql;

				//
				//	Delay power event if Clients' operations are in progress.
				//

				KDPrintM(DBG_OTHER_INFO, ("Start holding power down\n"));

				interval.QuadPart = - NANO100_PER_SEC;
				loopCnt = 0;
				while(TRUE) {

					ACQUIRE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, &oldIrql);

					//
					//	If 5 seconds passes after last operation, exit.
					//

					if(TDICLIENT_CONTEXT.ClientInProgressIOCount == 0) {
						LARGE_INTEGER	currentTime;

						LsuCurrentTime(&currentTime);
						if(currentTime.QuadPart - TDICLIENT_CONTEXT.LastOperationTime.QuadPart
							> LSTRANS_LPX_POWERDOWN_TIMEOUT) {

							RELEASE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, oldIrql);

							break;
						}
					}

					RELEASE_SPIN_LOCK(&TDICLIENT_CONTEXT.TdiPnPSpinLock, oldIrql);

					KeDelayExecutionThread(KernelMode, FALSE, &interval);

					loopCnt ++;
					if(loopCnt >= TDIPNPCLIENT_MAX_HOLDING_LOOP) {
						KDPrintM(DBG_OTHER_ERROR, ("Too long holding.\n"));
						break;
					}
					KDPrintM(DBG_OTHER_INFO, ("Loop count:%u\n", loopCnt));
				}


				//
				//	Hold power down by returning PENDING.
				//

				KDPrintM(DBG_OTHER_INFO, ("Finished holding power down\n"));
			}
#endif
		break;
		case NetEventQueryPower:
			KDPrintM(DBG_OTHER_INFO, ("NetEventQueryPower\n"));
		break;
		case NetEventQueryRemoveDevice:
			KDPrintM(DBG_OTHER_INFO, ("NetEventQueryRemoveDevice\n"));
		break;
		case NetEventCancelRemoveDevice:
			KDPrintM(DBG_OTHER_INFO, ("NetEventCancelRemoveDevice\n"));
		break;
		case NetEventReconfigure:
			KDPrintM(DBG_OTHER_INFO, ("NetEventReconfigure\n"));
		break;
		case NetEventBindList:
			KDPrintM(DBG_OTHER_INFO, ("NetEventBindList\n"));
		break;
		case NetEventBindsComplete:
			KDPrintM(DBG_OTHER_INFO, ("NetEventBindsComplete\n"));
		break;
		case NetEventPnPCapabilities:
			KDPrintM(DBG_OTHER_INFO, ("NetEventPnPCapabilities\n"));
		break;
		default:
			KDPrintM(DBG_OTHER_ERROR, ("Unknown PnP code. %x\n", PowerEvent->NetEvent));
	}

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Bock access control list ( BACL )
//


PLSU_BLOCK_ACE
LsuCreateBlockAce(
	IN UCHAR		AccessMode,
	IN UINT64		BlockStartAddr,
	IN UINT64		BlockEndAddr
){
	PLSU_BLOCK_ACE	ace;
	
	ASSERT(BlockStartAddr <= BlockEndAddr);

	ace = ExAllocatePoolWithTag(NonPagedPool, sizeof(LSU_BLOCK_ACE), LSU_POOLTAG_BLOCKACE);
	if(ace == NULL)
		return NULL;

	InitializeListHead(&ace->BlockAclList);
	ace->AccessMode = AccessMode;
	ace->BlockStartAddr = BlockStartAddr;
	ace->BlockEndAddr = BlockEndAddr;
	ace->BlockAceId = (BLOCKACE_ID)(ULONG_PTR)ace;

	return ace;
}

VOID
LsuFreeBlockAce(
	IN PLSU_BLOCK_ACE LsuBlockAce
){
	if(LsuBlockAce == NULL)
		return;
	ExFreePoolWithTag(LsuBlockAce, LSU_POOLTAG_BLOCKACE);
}

VOID
LsuFreeAllBlockAce(
	IN PLIST_ENTRY	AceHead
){
	PLIST_ENTRY	cur, next;
	PLSU_BLOCK_ACE lsuBlockAce;

	for(cur = AceHead->Flink; cur != AceHead; cur = next) {
		next = cur->Flink;

		lsuBlockAce = CONTAINING_RECORD(cur, LSU_BLOCK_ACE, BlockAclList);
		LsuFreeBlockAce(lsuBlockAce);
	}
}

VOID
LsuInitializeBlockAcl(
	OUT PLSU_BLOCK_ACL	LsuBacl
){
	RtlZeroMemory(LsuBacl, sizeof(LSU_BLOCK_ACL));

	InitializeListHead(&LsuBacl->BlockAclHead);
	KeInitializeSpinLock(&LsuBacl->BlcokAclSpinlock);
}

NTSTATUS
LsuConvertNdasBaclToLsuBacl(
	OUT PLSU_BLOCK_ACL	LsuBacl,
	IN PNDAS_BLOCK_ACL	NdasBacl
){
	NTSTATUS		status;
	KIRQL			oldIrql;
	ULONG			cnt;
	PNDAS_BLOCK_ACE	ndasBace;
	PLSU_BLOCK_ACE	lsuBace;
	UCHAR			accessMode;

	status = STATUS_SUCCESS;

	KeAcquireSpinLock(&LsuBacl->BlcokAclSpinlock, &oldIrql);

	for(cnt = 0; cnt < NdasBacl->BlockACECnt; cnt++) {
		ndasBace = &NdasBacl->BlockACEs[cnt];

		accessMode = 0;
		if(ndasBace->AccessMode & NBACE_ACCESS_READ)
			accessMode |= LSUBACE_ACCESS_READ;
		if(ndasBace->AccessMode & NBACE_ACCESS_WRITE)
			accessMode |= LSUBACE_ACCESS_WRITE;

		lsuBace = LsuCreateBlockAce(accessMode, ndasBace->BlockStartAddr, ndasBace->BlockEndAddr);
		if(lsuBace == NULL) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//
		//	Insert to the list
		//

		InsertTailList(&LsuBacl->BlockAclHead, &lsuBace->BlockAclList);
		InterlockedIncrement(&LsuBacl->BlockACECnt);
	}

	KeReleaseSpinLock(&LsuBacl->BlcokAclSpinlock, oldIrql);

	return status;
}

NTSTATUS
LsuConvertLsuBaclToNdasBacl(
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN ULONG			NdasBaclLen,
	OUT	PULONG			RequiredBufLen,
	IN PLSU_BLOCK_ACL	LsuBacl
){
	NTSTATUS		status;
	KIRQL			oldIrql;
	PLSU_BLOCK_ACE	lsuBlockAce;
	PNDAS_BLOCK_ACE	ndasBace;
	PLIST_ENTRY		cur;
	ULONG			cnt;
	ULONG			requiredBufLen;

	KeAcquireSpinLock(&LsuBacl->BlcokAclSpinlock, &oldIrql);

	requiredBufLen = FIELD_OFFSET(NDAS_BLOCK_ACL, BlockACEs) +
		sizeof(NDAS_BLOCK_ACE) * LsuBacl->BlockACECnt;

	if(RequiredBufLen)
		*RequiredBufLen = requiredBufLen;

	if(NdasBaclLen < requiredBufLen) {
		KeReleaseSpinLock(&LsuBacl->BlcokAclSpinlock, oldIrql);
		return STATUS_BUFFER_TOO_SMALL;
	}

	NdasBacl->Length = requiredBufLen;
	NdasBacl->BlockACECnt = LsuBacl->BlockACECnt;
	status = STATUS_SUCCESS;
	cnt = 0;
	for(cur = LsuBacl->BlockAclHead.Flink;
		cur != &LsuBacl->BlockAclHead;
		cur = cur->Flink) {

		lsuBlockAce = CONTAINING_RECORD(cur, LSU_BLOCK_ACE, BlockAclList);
		ndasBace = &NdasBacl->BlockACEs[cnt];

		ndasBace->BlockAceId = lsuBlockAce->BlockAceId;

		ndasBace->AccessMode = 0;
		if(lsuBlockAce->AccessMode & LSUBACE_ACCESS_READ)
			ndasBace->AccessMode |= NBACE_ACCESS_READ;
		if(lsuBlockAce->AccessMode & LSUBACE_ACCESS_WRITE)
			ndasBace->AccessMode |= NBACE_ACCESS_WRITE;

		ndasBace->BlockStartAddr = lsuBlockAce->BlockStartAddr;
		ndasBace->BlockEndAddr = lsuBlockAce->BlockEndAddr;

		cnt ++;
	}

	KeReleaseSpinLock(&LsuBacl->BlcokAclSpinlock, oldIrql);

	return status;
}

PLSU_BLOCK_ACE
LsuGetAce(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN UINT64			StartAddr,
	IN UINT64			EndAddr
){
	KIRQL	oldIrql;
	PLIST_ENTRY	cur;
	PLSU_BLOCK_ACE lsuBlockAce;

	lsuBlockAce = NULL;
	KeAcquireSpinLock(&BlockAcl->BlcokAclSpinlock, &oldIrql);

	for(cur = BlockAcl->BlockAclHead.Flink; cur != &BlockAcl->BlockAclHead; cur = cur->Flink) {

		lsuBlockAce = CONTAINING_RECORD(cur, LSU_BLOCK_ACE, BlockAclList);

		if(	StartAddr >= lsuBlockAce->BlockStartAddr &&
			StartAddr <= lsuBlockAce->BlockEndAddr) {
			break;
		}
		if(	EndAddr >= lsuBlockAce->BlockStartAddr &&
			EndAddr <= lsuBlockAce->BlockEndAddr) {
			break;
		}

		lsuBlockAce = NULL;
	}

	KeReleaseSpinLock(&BlockAcl->BlcokAclSpinlock, oldIrql);

	return lsuBlockAce;
}

PLSU_BLOCK_ACE
LsuGetAceById(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN BLOCKACE_ID		BlockAceId
){
	KIRQL	oldIrql;
	PLIST_ENTRY	cur;
	PLSU_BLOCK_ACE lsuBlockAce;

	lsuBlockAce = NULL;
	KeAcquireSpinLock(&BlockAcl->BlcokAclSpinlock, &oldIrql);

	for(cur = BlockAcl->BlockAclHead.Flink; cur != &BlockAcl->BlockAclHead; cur = cur->Flink) {

		lsuBlockAce = CONTAINING_RECORD(cur, LSU_BLOCK_ACE, BlockAclList);

		if(	BlockAceId == lsuBlockAce->BlockAceId) {
			break;
		}

		lsuBlockAce = NULL;
	}

	KeReleaseSpinLock(&BlockAcl->BlcokAclSpinlock, oldIrql);

	return lsuBlockAce;
}

VOID
LsuInsertAce(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN PLSU_BLOCK_ACE	BlockAce
){
	InterlockedIncrement(&BlockAcl->BlockACECnt);
	ExInterlockedInsertHeadList(
			&BlockAcl->BlockAclHead,
			&BlockAce->BlockAclList,
			&BlockAcl->BlcokAclSpinlock);
}

VOID
LsuRemoveAce(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN PLSU_BLOCK_ACE	BlockAce
){
	KIRQL	oldIrql;

	InterlockedDecrement(&BlockAcl->BlockACECnt);

	KeAcquireSpinLock(&BlockAcl->BlcokAclSpinlock, &oldIrql);
	RemoveEntryList(&BlockAce->BlockAclList);
	KeReleaseSpinLock(&BlockAcl->BlcokAclSpinlock, oldIrql);
}

PLSU_BLOCK_ACE
LsuRemoveAceById(
	IN PLSU_BLOCK_ACL	BlockAcl,
	IN BLOCKACE_ID		BlockAceId
){
	KIRQL	oldIrql;
	PLIST_ENTRY	cur;
	PLSU_BLOCK_ACE lsuBlockAce;

	lsuBlockAce = NULL;
	KeAcquireSpinLock(&BlockAcl->BlcokAclSpinlock, &oldIrql);

	for(cur = BlockAcl->BlockAclHead.Flink; cur != &BlockAcl->BlockAclHead; cur = cur->Flink) {

		lsuBlockAce = CONTAINING_RECORD(cur, LSU_BLOCK_ACE, BlockAclList);

		if(	BlockAceId == lsuBlockAce->BlockAceId) {
			InterlockedDecrement(&BlockAcl->BlockACECnt);
			RemoveEntryList(&lsuBlockAce->BlockAclList);
			break;
		}

		lsuBlockAce = NULL;
	}

	KeReleaseSpinLock(&BlockAcl->BlcokAclSpinlock, oldIrql);

	return lsuBlockAce;
}
