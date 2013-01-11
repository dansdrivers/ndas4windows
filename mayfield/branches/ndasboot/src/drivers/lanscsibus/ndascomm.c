#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>

#include "lanscsibus.h"
#include "lsbusioctl.h"
#include "lsminiportioctl.h"
#include "ndas/ndasdib.h"
#include "lsutils.h"
#include "lurdesc.h"
#include "binparams.h"
#include "lslurn.h"

#include "busenum.h"
#include "stdio.h"
#include "LanscsiBusProc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NdasComm"

#define NCOMM_POOLTAG_LSS	'lnSL'

NTSTATUS
NCommGetDIBV1(
	PLURELATION_NODE_DESC	LurnDesc,
	UINT64					DIBAddress,
	PTA_LSTRANS_ADDRESS		NodeAddress,
	PTA_LSTRANS_ADDRESS		BindingAddress,
	PNDAS_DIB				DiskInformationBlock
) {
	PLANSCSI_SESSION	LSS;
	NTSTATUS			status;
	LSTRANS_TYPE		LstransType;
	LSSLOGIN_INFO		LoginInfo;

	LSS = (PLANSCSI_SESSION)ExAllocatePoolWithTag(PagedPool, sizeof(LANSCSI_SESSION), NCOMM_POOLTAG_LSS);
	if(!LSS) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Set login information.
	//
	LoginInfo.LoginType				= LOGIN_TYPE_NORMAL;
	RtlCopyMemory(&LoginInfo.UserID, &LurnDesc->LurnIde.UserID, LSPROTO_USERID_LENGTH);
	RtlCopyMemory(&LoginInfo.Password, &LurnDesc->LurnIde.Password, LSPROTO_PASSWORD_LENGTH);

	LoginInfo.MaxBlocksPerRequest	= LurnDesc->MaxBlocksPerRequest;
	LoginInfo.LanscsiTargetID		= LurnDesc->LurnIde.LanscsiTargetID;
	LoginInfo.LanscsiLU				= LurnDesc->LurnIde.LanscsiLU;
	LoginInfo.HWType				= LurnDesc->LurnIde.HWType;
	LoginInfo.HWVersion				= LurnDesc->LurnIde.HWVersion;
	LoginInfo.IsEncryptBuffer		= TRUE;
	LoginInfo.BlockInBytes			= BLOCK_SIZE;

	//
	//	Connect and log in.
	//
	status = LsuConnectLogin(LSS, NodeAddress, BindingAddress, &LoginInfo, &LstransType);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuConnectLogin() failed. NTSTATUS: %08lx.\n", status));
		ExFreePool(LSS);
		return status;
	}

	status = LsuGetDiskInfoBlockV1(
					LSS,
					DiskInformationBlock,
					DIBAddress,
					PDUDESC_FLAG_PIO|PDUDESC_FLAG_LBA
				);

	LspLogout(LSS);
	LspDisconnect(LSS);
	ExFreePool(LSS);
	return status;
}

NTSTATUS
NCommGetDIBV2(
	PLURELATION_NODE_DESC		LurnDesc,
	UINT64						DIBAddress,
	PTA_LSTRANS_ADDRESS			NodeAddress,
	PTA_LSTRANS_ADDRESS			BindingAddress,
	PNDAS_DIB_V2				DiskInformationBlock
) {
	PLANSCSI_SESSION	LSS;
	NTSTATUS			status;
	LSTRANS_TYPE		LstransType;
	LSSLOGIN_INFO		LoginInfo;

	LSS = (PLANSCSI_SESSION)ExAllocatePoolWithTag(PagedPool, sizeof(LANSCSI_SESSION), NCOMM_POOLTAG_LSS);
	if(!LSS) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Set login information.
	//
	LoginInfo.LoginType				= LOGIN_TYPE_NORMAL;
	RtlCopyMemory(&LoginInfo.UserID, &LurnDesc->LurnIde.UserID, LSPROTO_USERID_LENGTH);
	RtlCopyMemory(&LoginInfo.Password, &LurnDesc->LurnIde.Password, LSPROTO_PASSWORD_LENGTH);

	LoginInfo.MaxBlocksPerRequest	= LurnDesc->MaxBlocksPerRequest;
	LoginInfo.LanscsiTargetID		= LurnDesc->LurnIde.LanscsiTargetID;
	LoginInfo.LanscsiLU				= LurnDesc->LurnIde.LanscsiLU;
	LoginInfo.HWType				= LurnDesc->LurnIde.HWType;
	LoginInfo.HWVersion				= LurnDesc->LurnIde.HWVersion;
	LoginInfo.IsEncryptBuffer		= TRUE;
	LoginInfo.BlockInBytes			= BLOCK_SIZE;

	//
	//	Connect and log in.
	//
	status = LsuConnectLogin(LSS, NodeAddress, BindingAddress, &LoginInfo, &LstransType);
	if(!NT_SUCCESS(status)) {
		ExFreePool(LSS);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuConnectLogin() failed. NTSTATUS: %08lx.\n", status));
		return status;
	}

	status = LsuGetDiskInfoBlockV2(
					LSS,
					DiskInformationBlock,
					DIBAddress,
					PDUDESC_FLAG_PIO|PDUDESC_FLAG_LBA
				);

	LspLogout(LSS);
	LspDisconnect(LSS);
	ExFreePool(LSS);
	return status;
}


ULONG
UnitDiskType2AssocId(
	ULONG DiskType
) {
	switch(DiskType) {
	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
		return 0;
	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		return 1;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
		return 0;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
		return 1;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
		return 2;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
		return 3;
	default:
		return -1;
	}

}


NTSTATUS
NCommVerifyLurnWithDIB(
		PLURELATION_NODE_DESC	LurnDesc,
		LURN_TYPE				ParentLurnType,
		ULONG					AssocID
	) {
	NTSTATUS		status;

	switch(LurnDesc->LurnType) {
	case LURN_IDE_DISK:
		if(!LurnDesc->LurnIde.EndBlockAddrReserved) {
			status = STATUS_INVALID_PARAMETER;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PhysicalEndBlockAddr is not available.\n"));
			break;
		}

		switch(ParentLurnType) {
		//
		//	Old associate disk types.
		//
		case LURN_AGGREGATION:
		case LURN_MIRRORING: {
			NDAS_DIB DIB;

			status = NCommGetDIBV1(LurnDesc, LurnDesc->LurnIde.EndBlockAddrReserved, &LurnDesc->LurnIde.TargetAddress, &LurnDesc->LurnIde.BindingAddress, &DIB);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NCommGetDIBV1() failed. NTSTATUS:%08lx\n", status));
				break;
			}
			if(UnitDiskType2AssocId(DIB.DiskType) != AssocID) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIB's disktype mismatch. DiskType:%x\n", DIB.DiskType));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				break;
			}
			status = STATUS_SUCCESS;
			break;
		}
		//
		//	Old associate disk types.
		//
		case LURN_RAID1: {
			NDAS_DIB_V2	DIBV2;
			status = NCommGetDIBV2(LurnDesc, LurnDesc->LurnIde.EndBlockAddrReserved-1, &LurnDesc->LurnIde.TargetAddress, &LurnDesc->LurnIde.BindingAddress, &DIBV2);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIBV2 doesn't exist. NTSTATUS:%x\n", status));
				break;
			}
			//
			//	Disk information block Version 2
			//
			if(DIBV2.iMediaType != NMT_MIRROR ||
				DIBV2.iSequence != AssocID
				) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIB's disktype mismatch. MediaType:%x Sequence:%d\n", DIBV2.iMediaType, DIBV2.iSequence));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				break;
			}
			status = STATUS_SUCCESS;
			break;
		}
		case LURN_NULL: {
			union {
				NDAS_DIB	V1;
				NDAS_DIB_V2	V2;
			}	DIB;

			status = NCommGetDIBV2(LurnDesc, LurnDesc->LurnIde.EndBlockAddrReserved-1, &LurnDesc->LurnIde.TargetAddress, &LurnDesc->LurnIde.BindingAddress, &DIB.V2);
			if(NT_SUCCESS(status)) {
				//
				//	Disk information block Version 2
				//
				if(DIB.V2.iMediaType != NMT_SINGLE) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIB's disktype mismatch. MediaType:%x\n", DIB.V2.iMediaType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}
				status = STATUS_SUCCESS;
			} else if(status == STATUS_REVISION_MISMATCH) {
				//
				//	Disk information block Version 1
				//
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIBV2 doesn't exist. try DIBV1. NTSTATUS:%08lx\n", status));
				status = NCommGetDIBV1(LurnDesc, LurnDesc->LurnIde.EndBlockAddrReserved, &LurnDesc->LurnIde.TargetAddress, &LurnDesc->LurnIde.BindingAddress, &DIB.V1);

				if(status == STATUS_REVISION_MISMATCH) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIBV1 doesn't exist. Take it Single. NTSTATUS:%08lx\n", status));
					status = STATUS_SUCCESS;
				} else if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NCommGetDIBV1() failed. NTSTATUS:%08lx\n", status));
					break;
				}
				if(DIB.V1.DiskType != NDAS_DIB_DISK_TYPE_SINGLE) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DIB's disktype mismatch. DiskType:%x\n", DIB.V1.DiskType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}
				status = STATUS_SUCCESS;

			} else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NCommGetDIBV2() failed. NTSTATUS:%08lx\n", status));
				break;
			}
			break;
		}
		default:
			status = STATUS_OBJECT_TYPE_MISMATCH;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("This LURN is a child of non-associate node.\n"));
		}
		break;
	default:
		status = STATUS_SUCCESS;
	}

	return status;
}

