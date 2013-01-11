#include <ntddk.h>

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

#include "lsutils.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSUtils"


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
		KDPrintM(DBG_LURN_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}

	//
	//	Set timeouts.
	//
	RtlZeroMemory(LanScsiSession, sizeof(LANSCSI_SESSION));
	GenericTimeOut.QuadPart = 8 * NANO100_PER_SEC;
	LspSetTimeOut(LanScsiSession, &GenericTimeOut);

	status = LspConnect(
					LanScsiSession,
					BindingAddress,
					TargetAddress
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
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
		KDPrintM(DBG_LURN_ERROR, ("LspLogin(), Can't login to the LS node with UserID:%08lx. STATUS:0x%08x.\n", LoginInfo->UserID, status));
		status = STATUS_ACCESS_DENIED;
		goto error_out;
	}

error_out:
	return status;

}

//////////////////////////////////////////////////////////////////////////
//
//	IDE devices
//
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
	LSS_INITIALIZE_PDUDESC(&LSS, &PduDesc, IDE_COMMAND, WIN_IDENTIFY, PDUDESC_FLAG_PIO, 0, sizeof(struct hd_driveid), &info);
	status = LspRequest(LSS, &PduDesc, &PduResponse);
	if(!NT_SUCCESS(status) || PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, ("Identify Failed...\n"));
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

	LSS_INITIALIZE_PDUDESC(&LSS, &PduDesc, IDE_COMMAND, WIN_READ, PduFlags, LogicalBlockAddress, TransferBlocks, Buffer);
	status = LspRequest(
					LSS,
					&PduDesc,
					&PduResponse
				);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x\n", 
			LogicalBlockAddress, TransferBlocks));

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
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

	LSS_INITIALIZE_PDUDESC(&LSS, &PduDesc, IDE_COMMAND, WIN_WRITE, PduFlags, LogicalBlockAddress, TransferBlocks, Buffer);
	status = LspRequest(
					LSS,
					&PduDesc,
					&PduResponse
				);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x\n", 
			LogicalBlockAddress, TransferBlocks));

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
			("Error: logicalBlockAddress = %I64x, transferBlocks = %x PduResponse:%x\n", 
			LogicalBlockAddress, TransferBlocks, PduResponse));

		status = STATUS_REQUEST_NOT_ACCEPTED;
	}

	return status;
}

NTSTATUS
LsuGetDiskInfoBlockV1(
	PLANSCSI_SESSION		LSS,
	PNDAS_DIB				DiskInfomationBlock,
	UINT64					LogicalBlockAddress,
	ULONG					PduFlags
) {
	NTSTATUS	status;

	status = LsuReadBlocks(LSS, (PBYTE)DiskInfomationBlock, LogicalBlockAddress, 1, PduFlags);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if( DiskInfomationBlock->Signature != NDAS_DIB_SIGNATURE ||
		IS_NDAS_DIBV1_WRONG_VERSION(*DiskInfomationBlock) ) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_LURN_ERROR, 
			("Error: Revision mismatch. Signature:0x%08lx, Revision:%d.%d\n",
							DiskInfomationBlock->Signature,
							DiskInfomationBlock->MajorVersion,
							DiskInfomationBlock->MinorVersion
				));
	}

	return status;
}


NTSTATUS
LsuGetDiskInfoBlockV2(
	PLANSCSI_SESSION			LSS,
	PNDAS_DIB_V2				DiskInfomationBlock,
	UINT64						LogicalBlockAddress,
	ULONG						PduFlags
) {
	NTSTATUS	status;

	status = LsuReadBlocks(LSS, (PBYTE)DiskInfomationBlock, LogicalBlockAddress, 1, PduFlags);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if( DiskInfomationBlock->Signature != NDAS_DIB_V2_SIGNATURE ||
		IS_HIGHER_VERSION_V2(*DiskInfomationBlock) ) {

		status = STATUS_REVISION_MISMATCH;
		KDPrintM(DBG_LURN_ERROR, 
			("Error: Revision mismatch. Signature:0x%08lx, Revision:%d.%d\n",
							DiskInfomationBlock->Signature,
							DiskInfomationBlock->MajorVersion,
							DiskInfomationBlock->MinorVersion
				));
	}

	return status;
}

