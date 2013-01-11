#include <ntddk.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "KDebug.h"
#include "LSKLib.h"
#include "basetsdex.h"
#include "cipher.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnIde.h"

#include "hdreg.h"
#include "binparams.h"
#include "ndas/ndasdib.h"
#include "scrc32.h"

#include "lsutils.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSUtils"


#ifdef __PASS_DIB_CRC32_CHK__
#error "DIB CRC check must be enabled."
#endif

#ifdef __PASS_RMD_CRC32_CHK__
#error "DIB CRC check must be enabled."
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
	LARGE_INTEGER	GenericTimeOut;
	LSPROTO_TYPE	LSProto;

	//
	//	Confirm address type.
	//	Connect to the Lanscsi Node.
	//

	status = LstransAddrTypeToTransType( BindingAddress->Address[0].AddressType, LstransType);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}


	//
	//	Set timeouts
	//	Extend generic timeout
	//

	RtlZeroMemory(LanScsiSession, sizeof(LANSCSI_SESSION));
	GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT * 15 / 10;
	LspSetTimeOut(LanScsiSession, &GenericTimeOut);

	status = LspConnect(
					LanScsiSession,
					BindingAddress,
					TargetAddress
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
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
					LSProto
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, ("LspLogin(), Can't login to the LS node with UserID:%08lx. STATUS:0x%08x.\n", LoginInfo->UserID, status));
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
	LARGE_INTEGER		genericTimeOut;
	LSTRANS_TYPE		lstransType;
	PLANSCSI_SESSION	LSS;

	//
	//	Confirm address type.
	//	Connect to the Lanscsi Node.
	//

	status = LstransAddrTypeToTransType( TargetAddr->Address[0].AddressType, &lstransType);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		return STATUS_INVALID_ADDRESS;
	}


	//
	//	Allocate a Lanscsi session
	//

	LSS = (PLANSCSI_SESSION)ExAllocatePoolWithTag(NonPagedPool, sizeof(LANSCSI_SESSION), LSS_BUFFER_POOLTAG);
	if(!LSS) {
		KDPrintM(DBG_PROTO_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Set timeouts.
	//

	RtlZeroMemory(LSS, sizeof(LANSCSI_SESSION));
	genericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetTimeOut(LSS, &genericTimeOut);

	status = LspConnectEx(
					LSS,
					BoundAddr,
					BindingAddr,
					BindingAddr2,
					TargetAddr,
					BindAnyAddr
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
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
NTSTATUS
LsuConfigureIdeDisk(
	PLANSCSI_SESSION	LSS,
	PULONG				PduFlags,
	PBOOLEAN			Dma
){
	NTSTATUS			status;
	struct hd_driveid	info;
	ULONG				pduFlags;
	BOOLEAN				setDmaMode;
	LANSCSI_PDUDESC		pduDesc;
	UCHAR				pduResponse;

	//
	//	Get identify
	//

	status = LsuGetIdentify(LSS, &info);
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("LsuGetIdentify(1) failed. NTSTATUS: %08lx.\n", status));
		return status;
	}

	//
	// IO mode: DMA/PIO Mode.
	//
	KDPrintM(DBG_LURN_INFO, ("Major 0x%x, Minor 0x%x, Capa 0x%x\n",
							info.major_rev_num,
							info.minor_rev_num,
							info.capability));
	KDPrintM(DBG_LURN_INFO, ("DMA 0x%x, U-DMA 0x%x\n",
							info.dma_mword,
							info.dma_ultra));

	//
	//	determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions and disk capacity.
	//
	setDmaMode = FALSE;
	pduFlags = 0;

	do {
		UCHAR	DmaFeature;
		UCHAR	DmaMode;

		DmaFeature = 0;
		DmaMode = 0;

		//
		// Ultra DMA if NDAS chip is 2.0 or higher.
		//	Lower 8 bits of dma_ultra represent supported ultra dma modes.
		//
		if(LSS->HWVersion >= LANSCSIIDE_VERSION_2_0 && (info.dma_ultra & 0x00ff)) {
			// Find Fastest Mode.
			if(info.dma_ultra & 0x0001)
				DmaMode = 0;
			if(info.dma_ultra & 0x0002)
				DmaMode = 1;
			if(info.dma_ultra & 0x0004)
				DmaMode = 2;
			//	if Cable80, try higher Ultra Dma Mode.
#ifdef __DETECT_CABLE80__
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
#ifdef __DETECT_CABLE80__
			}
#endif
			KDPrintM(DBG_LURN_INFO, ("Ultra DMA %d detected.\n", (int)DmaMode));
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

			KDPrintM(DBG_LURN_INFO, ("DMA mode %d detected.\n", (int)DmaMode));
			DmaFeature = DmaMode | 0x20;
			pduFlags |= PDUDESC_FLAG_DMA;

			// Set DMA mode if needed
			if(!(info.dma_mword & (0x0100 << DmaMode))) {
				setDmaMode = TRUE;
			}

		}

		// Set DMA mode if needed.
		if(setDmaMode) {
			LSS_INITIALIZE_PDUDESC(LSS, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL);
			pduDesc.Feature = SETFEATURES_XFER;
			pduDesc.Param8[0] = DmaFeature;
			status = LspRequest(LSS, &pduDesc, &pduResponse);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("Set Feature Failed...\n"));
				return status;
			}
			if(pduResponse != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, ("SETFEATURES: PduResponse=%x\n", (ULONG)pduResponse));
				return STATUS_UNSUCCESSFUL;
			}

			// identify.
			status = LsuGetIdentify(LSS, &info);
			if(!NT_SUCCESS(status)) {
				KDPrint(1, ("LsuGetIdentify(2) failed. NTSTATUS: %08lx.\n", status));
				return status;
			}
			KDPrintM(DBG_LURN_INFO, ("After Set Feature DMA 0x%x, U-DMA 0x%x\n",
							info.dma_mword,
							info.dma_ultra));
		}
		if(pduFlags & PDUDESC_FLAG_DMA) {
			break;
		}
		//
		//	PIO.
		//
		KDPrintM(DBG_LURN_ERROR, ("NetDisk does not support DMA mode. Turn to PIO mode.\n"));
		pduFlags |= PDUDESC_FLAG_PIO;

	} while(0);


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
	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_IDENTIFY, 0, 0, sizeof(struct hd_driveid), info);
	status = LspRequest(LSS, &PduDesc, &PduResponse);
	if(!NT_SUCCESS(status) || PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Identify Failed...\n"));
		return status;
	}

	return status;
}

NTSTATUS
LsuReadBlocks(
	PLANSCSI_SESSION	LSS,
	PBYTE				Buffer,
	UINT64				LogicalBlockAddress,
	ULONG				TransferBlocks,
	ULONG				PduFlags
) {
	NTSTATUS		status;
	LANSCSI_PDUDESC	PduDesc;
	BYTE			PduResponse;

	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_READ, PduFlags, LogicalBlockAddress, TransferBlocks, Buffer);
	status = LspRequest(
					LSS,
					&PduDesc,
					&PduResponse
				);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x\n", 
			LogicalBlockAddress, TransferBlocks));

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x PduResponse:%x\n", 
			LogicalBlockAddress, TransferBlocks, PduResponse));

		status = STATUS_REQUEST_NOT_ACCEPTED;
	}

	return status;
}


NTSTATUS
LsuWriteBlocks(
	PLANSCSI_SESSION	LSS,
	PBYTE				Buffer,
	UINT64				LogicalBlockAddress,
	ULONG				TransferBlocks,
	ULONG				PduFlags
) {
	NTSTATUS		status;
	LANSCSI_PDUDESC	PduDesc;
	BYTE			PduResponse;

	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_WRITE, PduFlags, LogicalBlockAddress, TransferBlocks, Buffer);
	status = LspRequest(
					LSS,
					&PduDesc,
					&PduResponse
				);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_PROTO_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x\n", 
			LogicalBlockAddress, TransferBlocks));

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x PduResponse:%x\n", 
			LogicalBlockAddress, TransferBlocks, PduResponse));

		status = STATUS_REQUEST_NOT_ACCEPTED;
	}

	return status;
}

NTSTATUS
LsuGetDiskInfoBlockV1(
	PLANSCSI_SESSION		LSS,
	PNDAS_DIB				DiskInformationBlock,
	UINT64					LogicalBlockAddress,
	ULONG					PduFlags
) {
	NTSTATUS	status;

	status = LsuReadBlocks(LSS, (PBYTE)DiskInformationBlock, LogicalBlockAddress, 1, PduFlags);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if( DiskInformationBlock->Signature != NDAS_DIB_SIGNATURE ||
		IS_NDAS_DIBV1_WRONG_VERSION(*DiskInformationBlock) ) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_PROTO_ERROR, 
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
	PLANSCSI_SESSION			LSS,
	PNDAS_DIB_V2				DiskInformationBlock,
	UINT64						LogicalBlockAddress,
	ULONG						PduFlags
) {
	NTSTATUS	status;

	status = LsuReadBlocks(LSS, (PBYTE)DiskInformationBlock, LogicalBlockAddress, 1, PduFlags);
	if(!NT_SUCCESS(status)) {
		return status;
	}


	//
	//	Revision check
	//

	if( DiskInformationBlock->Signature != NDAS_DIB_V2_SIGNATURE ||
		IS_NDAS_DIB_V2_VERSION_HIGH(*DiskInformationBlock) ) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_PROTO_ERROR, 
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
			KDPrintM(DBG_PROTO_ERROR, ("DIBv2's crc is invalid.\n"));
			status = STATUS_REVISION_MISMATCH;
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
