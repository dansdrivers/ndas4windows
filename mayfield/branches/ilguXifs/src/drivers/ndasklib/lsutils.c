#include <ntddk.h>
#include <TdiKrnl.h>

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
#error "RMD CRC check must be enabled."
#endif


//
//	Global variables
//

LONG			TdiPnPInit = 0;
KSPIN_LOCK		TdiPnPSpinLock;
LONG			ClientInProgress = 0;
LARGE_INTEGER	LastOperationTime;


VOID
ClientPnPBindingChange_Delay(
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
		KDPrintM(DBG_OTHER_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
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
	LARGE_INTEGER		genericTimeOut;
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
		KDPrintM(DBG_OTHER_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
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
				if (LSS->HWVersion != LANSCSIIDE_VERSION_2_0) {
					if(info.dma_ultra & 0x0010)
						DmaMode = 4;
					if(info.dma_ultra & 0x0020)
						DmaMode = 5;
					if(info.dma_ultra & 0x0040)
						DmaMode = 6;
					if(info.dma_ultra & 0x0080)
						DmaMode = 7;
				}
#ifdef __DETECT_CABLE80__
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
			LSS_INITIALIZE_PDUDESC(LSS, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL);
			pduDesc.Feature = SETFEATURES_XFER;
			pduDesc.Param8[0] = DmaFeature;
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
				KDPrint(1, ("LsuGetIdentify(2) failed. NTSTATUS: %08lx.\n", status));
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
	LSS_INITIALIZE_PDUDESC(LSS, &PduDesc, IDE_COMMAND, WIN_IDENTIFY, 0, 0, 1, info);
	status = LspRequest(LSS, &PduDesc, &PduResponse);
	if(!NT_SUCCESS(status) || PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_OTHER_ERROR, ("Identify Failed...\n"));
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
LsuRegisterTdiPnPPowerHandler(
	IN PUNICODE_STRING			TdiClientName,
	IN TDI_PNP_POWER_HANDLER	TdiPnPPowerHandler,
	OUT PHANDLE					TdiClient
){
    NTSTATUS                    status;
    TDI_CLIENT_INTERFACE_INFO   info;
	LONG						tdiPnp;

	//
	//	Do not allow more than two TdiPnP registration
	//

	tdiPnp = InterlockedExchange(&TdiPnPInit, 1);
	if(tdiPnp) {
		ASSERT(FALSE);
		return STATUS_DEVICE_ALREADY_ATTACHED;
	}


	//
	//	Init spin lock protecting TdiPnP global variables.
	//

	KeInitializeSpinLock(&TdiPnPSpinLock);
 
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
    info.BindingHandler = ClientPnPBindingChange_Delay;
    info.AddAddressHandlerV2 = NULL;
    info.DelAddressHandlerV2 = NULL;
    info.PnPPowerHandler = TdiPnPPowerHandler;


	//
    // Register handlers with TDI
    //

    status = TdiRegisterPnPHandlers (&info, sizeof(info), TdiClient);
    if (!NT_SUCCESS (status)) {
		KDPrintM(DBG_OTHER_ERROR, (
					"Failed to register PnP handlers: %lx .\n",
					status));
        return status;
    }
	KDPrintM(DBG_OTHER_INFO, ("TDI PnP registered.\n"));

    return STATUS_SUCCESS;
}


VOID
LsuDeregisterTdiPnPHandlers(
	HANDLE			TdiClient
){
    if (TdiClient) {
        TdiDeregisterPnPHandlers(TdiClient);
		KDPrintM(DBG_OTHER_INFO, ("TDI PnP deregistered.\n"));
	}
}


//
//	Increment In-progress operation counter of TDI client.
//

VOID
LsuIncrementTdiClientInProgress() {
	KIRQL	oldIrql;

	ACQUIRE_SPIN_LOCK(&TdiPnPSpinLock, &oldIrql);

	ASSERT(ClientInProgress >= 0);
	ClientInProgress++;

	RELEASE_SPIN_LOCK(&TdiPnPSpinLock, oldIrql);

}


//
//	Increment In-progress operation counter of TDI client.
//

VOID
LsuDecrementTdiClientInProgress() {
	KIRQL	oldIrql;

	ACQUIRE_SPIN_LOCK(&TdiPnPSpinLock, &oldIrql);

	ClientInProgress--;
	ASSERT(ClientInProgress >= 0);

	LsuCurrentTime(&LastOperationTime);

	RELEASE_SPIN_LOCK(&TdiPnPSpinLock, oldIrql);
}

VOID
ClientPnPBindingChange_Delay(
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
ClientPnPPowerChange_Delay(
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
){
	NTSTATUS				status;
	UNICODE_STRING			lpxPrefix;
	NET_DEVICE_POWER_STATE	PowerState;

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

				PowerState = NetDeviceStateUnspecified;
	} else {
		PowerState = *((PNET_DEVICE_POWER_STATE) PowerEvent->Buffer);
	}

#if DBG
	if(DeviceName->Buffer) {
		KDPrintM(DBG_OTHER_ERROR, ("DeviceName=%ws PowerEvent=%x BufferLen=%u PowerStat=%x\n",
			DeviceName->Buffer,
			PowerEvent->NetEvent,
			PowerEvent->BufferLength,
			PowerState));
	} else {
		KDPrintM(DBG_OTHER_ERROR, ("DeviceName=NULL PowerEvent=%x BufferLen=%u PowerStat=%x\n",
			PowerEvent->NetEvent,
			PowerEvent->BufferLength,
			PowerState));
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
			if(PowerState != NetDeviceStateD0) {
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

					ACQUIRE_SPIN_LOCK(&TdiPnPSpinLock, &oldIrql);

					//
					//	If 3 seconds passes after last operation, exit.
					//

					if(ClientInProgress == 0) {
						LARGE_INTEGER	currentTime;

						LsuCurrentTime(&currentTime);
						if(currentTime.QuadPart - LastOperationTime.QuadPart
							> NANO100_PER_SEC * 3) {

							RELEASE_SPIN_LOCK(&TdiPnPSpinLock, oldIrql);

							break;
						}
					}

					RELEASE_SPIN_LOCK(&TdiPnPSpinLock, oldIrql);

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
