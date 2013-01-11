#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>

#include "ndasbus.h"
#include "lsbusioctl.h"
#include "lsminiportioctl.h"
#include "ndas/ndasdib.h"
#include "lsutils.h"
#include "lurdesc.h"
#include "binparams.h"
#include "lslurn.h"

#include "busenum.h"
#include "stdio.h"
#include "hdreg.h"
#include "ndasbuspriv.h"



#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NdasComm"

#define NCOMM_POOLTAG_LSS	'lnSL'

NTSTATUS
NCommGetDIBV1(
	OUT	PNDAS_DIB				DiskInformationBlock,
	IN	PLSSLOGIN_INFO			LoginInfo,
	IN	UINT64					DIBAddress,
	IN	PTA_LSTRANS_ADDRESS		NodeAddress,
	IN	PTA_LSTRANS_ADDRESS		BindingAddress
) {
	PLANSCSI_SESSION	LSS;
	NTSTATUS			status;
	LSTRANS_TYPE		LstransType;
	struct hd_driveid	info;
	ULONG				pduFlags;

	LSS = (PLANSCSI_SESSION)ExAllocatePoolWithTag(NonPagedPool, sizeof(LANSCSI_SESSION), NCOMM_POOLTAG_LSS);
	if(!LSS) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Connect and log in.
	//
	status = LsuConnectLogin(	LSS,
								NodeAddress,
								BindingAddress,
								LoginInfo,
								&LstransType);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuConnectLogin() failed. NTSTATUS: %08lx.\n", status));
		ExFreePool(LSS);
		return status;
	}

	//
	//	Get identify
	//
	status = LsuGetIdentify(LSS, &info);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuGetIdentify() failed. NTSTATUS: %08lx.\n", status));
		ExFreePool(LSS);
		return status;
	}

	pduFlags = PDUDESC_FLAG_PIO;

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
	}

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("LBA support: LBA %d, LBA48 %d\n",
		(pduFlags & PDUDESC_FLAG_LBA) != 0,
		(pduFlags & PDUDESC_FLAG_LBA48) != 0
		));

	//
	//	Read informaition block
	//

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("DIB addr=%I64u\n", DIBAddress));
	status = LsuGetDiskInfoBlockV1(
					LSS,
					DiskInformationBlock,
					DIBAddress,
					pduFlags
				);

	LspLogout(LSS);
	LspDisconnect(LSS);
	ExFreePool(LSS);
	return status;
}

NTSTATUS
NCommGetDIBV2(
	OUT	PNDAS_DIB_V2			DiskInformationBlock,
	IN	PLSSLOGIN_INFO			LoginInfo,
	IN	UINT64					DIBAddress,
	IN	PTA_LSTRANS_ADDRESS		NodeAddress,
	IN	PTA_LSTRANS_ADDRESS		BindingAddress
) {
	PLANSCSI_SESSION	LSS;
	NTSTATUS			status;
	LSTRANS_TYPE		LstransType;
	struct hd_driveid	info;
	ULONG				pduFlags;

	LSS = (PLANSCSI_SESSION)ExAllocatePoolWithTag(NonPagedPool, sizeof(LANSCSI_SESSION), NCOMM_POOLTAG_LSS);
	if(!LSS) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Connect and log in.
	//
	status = LsuConnectLogin(	LSS,
								NodeAddress,
								BindingAddress,
								LoginInfo,
								&LstransType);
	if(!NT_SUCCESS(status)) {
		ExFreePool(LSS);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuConnectLogin() failed. NTSTATUS: %08lx.\n", status));
		return status;
	}

	//
	//	Get identify
	//
	status = LsuGetIdentify(LSS, &info);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuGetIdentify() failed. NTSTATUS: %08lx.\n", status));
		ExFreePool(LSS);
		return status;
	}

	pduFlags = PDUDESC_FLAG_PIO;

	//
	// LBA support
	//
	if(!(info.capability & 0x02)) {
		pduFlags &= ~PDUDESC_FLAG_LBA;
	} else {
		pduFlags |= PDUDESC_FLAG_LBA;
	}

	//
	// LBA48 support
	//
	if(info.command_set_2 & 0x0400 || info.cfs_enable_2 & 0x0400) {	// Support LBA48bit
		pduFlags |= PDUDESC_FLAG_LBA48;
	}

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("LBA support: LBA %d, LBA48 %d\n",
						(pduFlags & PDUDESC_FLAG_LBA) != 0,
						(pduFlags & PDUDESC_FLAG_LBA48) != 0
		));

	//
	//	Read informaition block
	//

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("DIB addr=%I64u\n", DIBAddress));
	status = LsuGetDiskInfoBlockV2(
					LSS,
					DiskInformationBlock,
					DIBAddress,
					pduFlags
				);

	LspLogout(LSS);
	LspDisconnect(LSS);
	ExFreePool(LSS);
	return status;
}


ULONG
LagacyMirrAggrType2SeqNo(
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


ULONG
NdasDevType2DIBType(
	ULONG	NdasDevType
){
	switch(NdasDevType) {
	case NDASSCSI_TYPE_DISK_NORMAL:
		return NMT_SINGLE;
	case NDASSCSI_TYPE_DISK_MIRROR:
		return NMT_MIRROR;
	case NDASSCSI_TYPE_DISK_AGGREGATION:
		return NMT_AGGREGATE;
	case NDASSCSI_TYPE_DVD:
		return NMT_CDROM;
	case NDASSCSI_TYPE_VDVD:
		return NMT_VDVD;
	case NDASSCSI_TYPE_MO:
		return NMT_OPMEM;
	case NDASSCSI_TYPE_AOD:
		return NMT_AOD;
	case NDASSCSI_TYPE_DISK_RAID0:
		return NMT_RAID0;
	case NDASSCSI_TYPE_DISK_RAID1:
		return NMT_RAID1;
	case NDASSCSI_TYPE_DISK_RAID4:
		return NMT_RAID4;
	default:
		return NMT_INVALID;
	}
}




#define BUILD_LOGININFO(LOGININFO_POINTER, UNIT_POINTER, MAX_BPR) {										\
	(LOGININFO_POINTER)->LoginType				= LOGIN_TYPE_NORMAL;									\
	RtlCopyMemory(&(LOGININFO_POINTER)->UserID, &(UNIT_POINTER)->iUserID, LSPROTO_USERID_LENGTH);		\
	RtlCopyMemory(&(LOGININFO_POINTER)->Password, &(UNIT_POINTER)->iPassword, LSPROTO_PASSWORD_LENGTH);	\
	(LOGININFO_POINTER)->MaxBlocksPerRequest	= (MAX_BPR);											\
	(LOGININFO_POINTER)->LanscsiTargetID		= (UNIT_POINTER)->ucUnitNumber;							\
	(LOGININFO_POINTER)->LanscsiLU				= 0;													\
	(LOGININFO_POINTER)->HWType					= (UNIT_POINTER)->ucHWType;								\
	(LOGININFO_POINTER)->HWVersion				= (UNIT_POINTER)->ucHWVersion;							\
	(LOGININFO_POINTER)->IsEncryptBuffer		= TRUE;													\
	(LOGININFO_POINTER)->BlockInBytes			= BLOCK_SIZE;	}




NTSTATUS
NCommVerifyNdasDevWithDIB(
		IN OUT PLANSCSI_ADD_TARGET_DATA	AddTargetData,
		IN PTA_LSTRANS_ADDRESS		SecondaryAddress,
		IN ULONG					MaxBlocksPerRequest
	) {
	NTSTATUS			status;
	LSSLOGIN_INFO		loginInfo;
	PLSBUS_UNITDISK		unit;
	TA_LSTRANS_ADDRESS	targetAddr;
	TA_LSTRANS_ADDRESS	bindingAddr;
	TA_LSTRANS_ADDRESS	bindingAddr2;
	TA_LSTRANS_ADDRESS	boundAddr;
	ULONG				idx_unit;
	ULONG				fault_cnt;

	//
	//	Parameter check
	//
	if(SecondaryAddress) {
		if(SecondaryAddress->Address[0].AddressType != TDI_ADDRESS_TYPE_LPX) {
			return STATUS_NOT_SUPPORTED;
		}
	}


	//
	//	Start verifying
	//

	fault_cnt = 0;
	for(idx_unit = 0; idx_unit < AddTargetData->ulNumberOfUnitDiskList; idx_unit++) {
		unit = AddTargetData->UnitDiskList + idx_unit;
		LSTRANS_COPY_LPXADDRESS(&targetAddr, &unit->Address);
		LSTRANS_COPY_LPXADDRESS(&bindingAddr, &unit->NICAddr);
		LSTRANS_COPY_LPXADDRESS(&bindingAddr2, &SecondaryAddress->Address[0].Address);

		BUILD_LOGININFO(&loginInfo, unit, MaxBlocksPerRequest);

		//
		//	Query binding address
		//
		status = LsuQueryBindingAddress(&boundAddr, &targetAddr, &bindingAddr, &bindingAddr2, TRUE);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuQueryBindingAddress() failed. STATUS=%08lx\n\n", status));
			break;
		}

		//
		//	Set the actual binding address.
		//
		LSTRANS_COPY_TO_LPXADDRESS(&unit->NICAddr, &boundAddr);
		RtlCopyMemory(&bindingAddr, &boundAddr, sizeof(TA_LSTRANS_ADDRESS));

		//
		//	Read and verify DIB
		//

		switch(AddTargetData->ucTargetType) {
		case NDASSCSI_TYPE_DISK_NORMAL: {
			union {
				NDAS_DIB	V1;
				NDAS_DIB_V2	V2;
			}	DIB;

			if(idx_unit > 0) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: Too many Unit devices\n"));
				status = STATUS_TOO_MANY_NODES;
				break;
			}

			status = NCommGetDIBV2(	&DIB.V2,
									&loginInfo,
									unit->ulPhysicalBlocks-2,
									&targetAddr,
									&bindingAddr);
			if(NT_SUCCESS(status)) {
				//
				//	Disk information block Version 2
				//
				if(DIB.V2.iMediaType != NMT_SINGLE) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: DIB's disktype mismatch. MediaType:%x\n", DIB.V2.iMediaType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}
				status = STATUS_SUCCESS;
			} else if(status == STATUS_REVISION_MISMATCH) {
				//
				//	Disk information block Version 1
				//
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: DIBV2 doesn't exist. try DIBV1. NTSTATUS:%08lx\n", status));
				status = NCommGetDIBV1(	&DIB.V1,
									&loginInfo,
									unit->ulPhysicalBlocks-1,
									&targetAddr,
									&bindingAddr);

				if(status == STATUS_REVISION_MISMATCH) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: DIBV1 doesn't exist. Take it Single. NTSTATUS:%08lx\n", status));
					status = STATUS_SUCCESS;
					break;
				} else if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: NCommGetDIBV1() failed. NTSTATUS:%08lx\n", status));
					break;
				}
				if(DIB.V1.DiskType != NDAS_DIB_DISK_TYPE_SINGLE) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: DIB's disktype mismatch. DiskType:%x\n", DIB.V1.DiskType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}
				status = STATUS_SUCCESS;

			} else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: NCommGetDIBV2() failed. NTSTATUS:%08lx\n", status));
			}
			break;
		}

		case NDASSCSI_TYPE_DISK_RAID0:
		case NDASSCSI_TYPE_DISK_RAID1:
		case NDASSCSI_TYPE_DISK_RAID4: {
			NDAS_DIB_V2	DIBV2;


			status = NCommGetDIBV2(	&DIBV2,
									&loginInfo,
									unit->ulPhysicalBlocks-2,
									&targetAddr,
									&bindingAddr);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID: DIBV2 doesn't exist. NTSTATUS:%x\n", status));
				break;
			}
			//
			//	Disk information block Version 2
			//
			if(DIBV2.iMediaType != NdasDevType2DIBType(AddTargetData->ucTargetType) ||
				DIBV2.iSequence != idx_unit
				) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID: DIB's disktype mismatch. MediaType:%x Sequence:%d\n",
														DIBV2.iMediaType,
														DIBV2.iSequence));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}
				status = STATUS_SUCCESS;
				break;
		}

		case NDASSCSI_TYPE_DISK_MIRROR:
		case NDASSCSI_TYPE_DISK_AGGREGATION: {
			union {
				NDAS_DIB	V1;
				NDAS_DIB_V2	V2;
			}	DIB;

			//
			//	Disk information block Version 2
			//

			status = NCommGetDIBV2(	&DIB.V2,
									&loginInfo,
									unit->ulPhysicalBlocks-2,
									&targetAddr,
									&bindingAddr);
			if(NT_SUCCESS(status)) {
				if(DIB.V2.iMediaType != NMT_MIRROR &&
					DIB.V2.iMediaType != NMT_AGGREGATE
					) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv2's disktype mismatch. MediaType:%x\n", DIB.V2.iMediaType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}

				if(DIB.V2.nDiskCount != AddTargetData->ulNumberOfUnitDiskList) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv2's Diskcount mismatch."
						" numberofunitdisklist=%u ndiskcount=%u\n", AddTargetData->ulNumberOfUnitDiskList, DIB.V2.nDiskCount));
				}

				if(DIB.V2.iSequence != idx_unit) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv2's sequence number mismatch."
											" idx_unit=%u Seq=%u\n", idx_unit, DIB.V2.iSequence));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}

				status = STATUS_SUCCESS;

			} else if(status == STATUS_REVISION_MISMATCH) {

				//
				//	Disk information block Version 1
				//

				status = NCommGetDIBV1(	&DIB.V1,
									&loginInfo,
									unit->ulPhysicalBlocks-1,
									&targetAddr,
									&bindingAddr);
				if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: NCommGetDIBV1() failed. NTSTATUS:%08lx\n", status));
					break;
				}
				if(LagacyMirrAggrType2SeqNo(DIB.V1.DiskType) != idx_unit) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIB's disktype mismatch. DiskType:%x\n", DIB.V1.DiskType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}
			} else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: NCommGetDIBV2() failed. NTSTATUS:%08lx\n", status));
			}

			status = STATUS_SUCCESS;
		break;
		}

		case NDASSCSI_TYPE_DVD:
		case NDASSCSI_TYPE_VDVD:
		case NDASSCSI_TYPE_MO:
			if(idx_unit > 0) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ODD: Too many Unit devices\n"));
				status = STATUS_TOO_MANY_NODES;
				break;
			}

			status = STATUS_SUCCESS;
			break;
		case NDASSCSI_TYPE_AOD:
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("AOD: Not supported.\n"));
			status = STATUS_NOT_SUPPORTED;
			break;
		default:
			status = STATUS_OBJECT_TYPE_MISMATCH;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Invalid target type.\n"));
			break;
		}


		if(!NT_SUCCESS(status)) {
			break;
		}
	}

	return status;
}

