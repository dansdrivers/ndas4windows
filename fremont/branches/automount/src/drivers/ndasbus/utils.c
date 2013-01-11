#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "ndasbus.h"
#include "ndasbusioctl.h"
#include "ndasscsiioctl.h"
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

NTSTATUS
NCommGetDIBV1(
	OUT	PNDAS_DIB				DiskInformationBlock,
	IN	PLSSLOGIN_INFO			LoginInfo,
	IN	UINT64					DIBAddress,
	IN	PTA_LSTRANS_ADDRESS		NodeAddress,
	IN	PTA_LSTRANS_ADDRESS		BindingAddress,
	IN	UCHAR					UdmaRestrict
) {
	PLANSCSI_SESSION	LSS;
	NTSTATUS			status;
	LSTRANS_TYPE		LstransType;
	ULONG				pduFlags;
	BOOLEAN				dma;
	ULONG				bytesOfBlock;

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
		ExFreePoolWithTag(LSS, NCOMM_POOLTAG_LSS);
		return status;
	}

	status = LsuConfigureIdeDisk(LSS, UdmaRestrict, &pduFlags, &dma, &bytesOfBlock);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(LSS, NCOMM_POOLTAG_LSS);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IdeConfigure() failed. NTSTATUS: %08lx.\n", status));
		return status;
	}
	if(dma == FALSE) {
		ExFreePoolWithTag(LSS, NCOMM_POOLTAG_LSS);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Disk does not support DMA. DMA required.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Read informaition block
	//

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("DIB addr=%I64u\n", DIBAddress));
	status = LsuGetDiskInfoBlockV1(
					LSS,
					DiskInformationBlock,
					DIBAddress,
					bytesOfBlock,
					pduFlags
				);

	LspLogout(LSS, NULL);
	LspDisconnect(LSS);
	ExFreePoolWithTag(LSS, NCOMM_POOLTAG_LSS);

	return status;
}

NTSTATUS
NCommGetDIBV2(
	OUT	PNDAS_DIB_V2				DiskInformationBlock,
	OUT PBLOCK_ACCESS_CONTROL_LIST	*BlockACL,
	IN	PLSSLOGIN_INFO				LoginInfo,
	IN	UINT64						DIBAddress,
	IN	UINT64						EncryptInfoBlockAddr,
	IN	UINT64						BaclBlockAddr,
	IN	PTA_LSTRANS_ADDRESS			NodeAddress,
	IN	PTA_LSTRANS_ADDRESS			BindingAddress,
	IN	UCHAR						UdmaRestrict
) {
	PLANSCSI_SESSION	LSS;
	NTSTATUS			status;
	LSTRANS_TYPE		LstransType;
	ULONG				pduFlags;
	BOOLEAN				dma;
	ULONG				bytesOfBlock;

	UNREFERENCED_PARAMETER(EncryptInfoBlockAddr);

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

	do {


		status = LsuConfigureIdeDisk(LSS, UdmaRestrict, &pduFlags, &dma, &bytesOfBlock);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IdeConfigure() failed. NTSTATUS: %08lx.\n", status));
			break;
		}
		if(dma == FALSE) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Disk does not support DMA. DMA required.\n"));
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}


		//
		//	Read information block
		//

		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("DIB addr=%I64u\n", DIBAddress));
		status = LsuGetDiskInfoBlockV2(
						LSS,
						DiskInformationBlock,
						DIBAddress,
						bytesOfBlock,
						pduFlags
					);

		//
		//	Read Block ACL
		//

		if(NT_SUCCESS(status) && DiskInformationBlock->BACLSize) {
			PBLOCK_ACCESS_CONTROL_LIST	bacl;

			//
			//	Allocate memory for BACL. Caller take charge in freeing it.
			//

			bacl = ExAllocatePoolWithTag(
							NonPagedPool,
							DiskInformationBlock->BACLSize,
							NCOMM_POOLTAG_BACL);
			if(bacl == NULL) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			status = LsuGetBlockACL(
					LSS,
					bacl,
					DiskInformationBlock->BACLSize,
					BaclBlockAddr,
					bytesOfBlock,
					pduFlags);
			if(NT_SUCCESS(status)) {
				*BlockACL = bacl;
			} else {
				*BlockACL = NULL;
				ExFreePoolWithTag(bacl, NCOMM_POOLTAG_BACL);
			}
		}

	} while(0);

	LspLogout(LSS, NULL);
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

LagacyMirrAggrType2TargetType(
	ULONG DiskType
) {
	switch(DiskType) {

	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
		return NDASSCSI_TYPE_DISK_MIRROR;

	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		return NDASSCSI_TYPE_DISK_MIRROR;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
		return NDASSCSI_TYPE_DISK_AGGREGATION;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
		return NDASSCSI_TYPE_DISK_AGGREGATION;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
		return NDASSCSI_TYPE_DISK_AGGREGATION;

	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
		return NDASSCSI_TYPE_DISK_AGGREGATION;

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
#if 0	// Kernel driver does not support these types anymore.
	case NDASSCSI_TYPE_DISK_RAID1R2:
		return NMT_RAID1R2;
	case NDASSCSI_TYPE_DISK_RAID4R2:
		return NMT_RAID4R2;
#endif		
	case NDASSCSI_TYPE_DISK_RAID1R3:
		return NMT_RAID1R3;
	case NDASSCSI_TYPE_DISK_RAID4R3:
		return NMT_RAID4R3;
	default:
		return NMT_INVALID;
	}
}




#define BUILD_LOGININFO(LOGININFO_POINTER, UNIT_POINTER) {												\
	(LOGININFO_POINTER)->LoginType				= LOGIN_TYPE_NORMAL;									\
	RtlCopyMemory(&(LOGININFO_POINTER)->UserID, &(UNIT_POINTER)->iUserID, LSPROTO_USERID_LENGTH);		\
	RtlCopyMemory(&(LOGININFO_POINTER)->Password, &(UNIT_POINTER)->iPassword, LSPROTO_PASSWORD_LENGTH);	\
																										\
	(LOGININFO_POINTER)->MaxDataTransferLength	= (UNIT_POINTER)->UnitMaxDataSendLength>(UNIT_POINTER)->UnitMaxDataRecvLength? \
						(UNIT_POINTER)->UnitMaxDataSendLength:(UNIT_POINTER)->UnitMaxDataRecvLength;	\
																										\
	(LOGININFO_POINTER)->LanscsiTargetID		= (UNIT_POINTER)->ucUnitNumber;							\
	(LOGININFO_POINTER)->LanscsiLU				= 0;													\
	(LOGININFO_POINTER)->HWType					= (UNIT_POINTER)->ucHWType;								\
	(LOGININFO_POINTER)->HWVersion				= (UNIT_POINTER)->ucHWVersion;							\
	(LOGININFO_POINTER)->IsEncryptBuffer		= TRUE; }



NTSTATUS
VerifyAddTargetData(
	IN PNDASBUS_ADD_TARGET_DATA	AddTargetData
){
	switch(AddTargetData->ucTargetType) {
	case NDASSCSI_TYPE_DISK_NORMAL:
	case NDASSCSI_TYPE_DVD:
	case NDASSCSI_TYPE_VDVD:
	case NDASSCSI_TYPE_MO: {
		if(AddTargetData->ulNumberOfUnitDiskList != 1) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL/ODD: Too many Unit devices\n"));
			return STATUS_TOO_MANY_NODES;
		}
		break;
	}
	case NDASSCSI_TYPE_DISK_RAID0:
	case NDASSCSI_TYPE_DISK_RAID1R2:
	case NDASSCSI_TYPE_DISK_RAID4R2:
	case NDASSCSI_TYPE_DISK_RAID1R3:
	case NDASSCSI_TYPE_DISK_RAID4R3:
	case NDASSCSI_TYPE_DISK_MIRROR:
	case NDASSCSI_TYPE_DISK_AGGREGATION: {
		if(AddTargetData->ulNumberOfUnitDiskList <= 1) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID: Too few Unit devices\n"));
			return STATUS_INVALID_PARAMETER;
		}
		break;
	}
	case NDASSCSI_TYPE_AOD: {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("AOD: Not supported\n"));
		return STATUS_NOT_SUPPORTED;
	}
	default:
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Invalid target type.\n"));
		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
NCommVerifyNdasBlockACL(
	IN PBLOCK_ACCESS_CONTROL_LIST	NdasBACLBlocks,
	IN ULONG						NdasBACLBlockLen
){
	ULONG	expectedLength;

	if(NdasBACLBlocks->Signature != BACL_SIGNATURE) {
		return STATUS_UNSUCCESSFUL;
	}

	expectedLength = FIELD_OFFSET(BLOCK_ACCESS_CONTROL_LIST, Elements) + 
			NdasBACLBlocks->ElementCount * sizeof(BLOCK_ACCESS_CONTROL_LIST_ELEMENT);
	if(NdasBACLBlockLen < expectedLength) {
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
NCommCompareNdasBlockACL(
	IN PNDAS_BLOCK_ACL				NdasBACL,
	IN PBLOCK_ACCESS_CONTROL_LIST	NdasBACLBlocks
){
	ULONG	cnt;
	PNDAS_BLOCK_ACE						bace;
	PBLOCK_ACCESS_CONTROL_LIST_ELEMENT	bacle;

	if(NdasBACL->BlockACECnt != (NdasBACLBlocks->ElementCount)) {
		return STATUS_UNSUCCESSFUL;
	}

	for(cnt = 0; cnt < NdasBACL->BlockACECnt; cnt++) {

		bace = &NdasBACL->BlockACEs[cnt];
		bacle = &NdasBACLBlocks->Elements[cnt];

		if(bace->BlockStartAddr != bacle->ui64StartSector) {
			return STATUS_UNSUCCESSFUL;
		}
		if(bacle->ui64SectorCount == 0)
			return STATUS_UNSUCCESSFUL;
		if(bace->BlockEndAddr != bacle->ui64StartSector + bacle->ui64SectorCount - 1) {
			return STATUS_UNSUCCESSFUL;
		}
		if(	(bace->AccessMode & NBACE_ACCESS_READ) && !(bacle->AccessMask & BACL_ACCESS_MASK_READ) ||
			!(bace->AccessMode & NBACE_ACCESS_READ) && (bacle->AccessMask & BACL_ACCESS_MASK_READ)) {
			return STATUS_UNSUCCESSFUL;
		}
		if(	bace->AccessMode & NBACE_ACCESS_WRITE && !(bacle->AccessMask & BACL_ACCESS_MASK_WRITE) ||
			!(bace->AccessMode & NBACE_ACCESS_WRITE) && (bacle->AccessMask & BACL_ACCESS_MASK_WRITE)) {
			return STATUS_UNSUCCESSFUL;
		}
	}

	return STATUS_SUCCESS;
}


//
//	Verify and compare block access control list
//
static
NTSTATUS
NCommVerifyNdasBlockACLWithTargetData(
	IN PNDASBUS_ADD_TARGET_DATA		AddTargetData,
	IN ULONG						NdasBACLBlockLen,
	IN PBLOCK_ACCESS_CONTROL_LIST	NdasBACLBlocks
) {
	NTSTATUS	status;
	PNDAS_BLOCK_ACL	ndasBlockAcl;

	ndasBlockAcl = (PNDAS_BLOCK_ACL)((PUCHAR)AddTargetData + AddTargetData->BACLOffset);


	if(	AddTargetData->BACLOffset && !NdasBACLBlocks ||
	   !AddTargetData->BACLOffset && NdasBACLBlocks) {
		   return STATUS_UNSUCCESSFUL;
	}

	if(NdasBACLBlocks == NULL)
		return STATUS_SUCCESS;

	status = NCommVerifyNdasBlockACL(NdasBACLBlocks, NdasBACLBlockLen);
	if(!NT_SUCCESS(status))
		return status;

	status = NCommCompareNdasBlockACL(ndasBlockAcl, NdasBACLBlocks);
	if(!NT_SUCCESS(status))
		return status;

	return STATUS_SUCCESS;
}

//
//	Verify NDAS device DIBs
//

NTSTATUS
NCommVerifyNdasDevWithDIB(
		IN OUT PNDASBUS_ADD_TARGET_DATA	AddTargetData,
		IN PTA_LSTRANS_ADDRESS		SecondaryAddress
	) {
	NTSTATUS			status;
	LSSLOGIN_INFO		loginInfo;
	PNDASBUS_UNITDISK		unit;
	TA_LSTRANS_ADDRESS	targetAddr;
	TA_LSTRANS_ADDRESS	bindingAddr;
	TA_LSTRANS_ADDRESS	bindingAddr2;
	TA_LSTRANS_ADDRESS	boundAddr;
	ULONG				idx_unit;
	ULONG				fault_cnt;
	BOOLEAN				oriDibValid;
	UINT32				crcunit = 0;
	BOOLEAN				exitImm;


	//
	//	check AddTargetData sanity
	//
	status = VerifyAddTargetData(AddTargetData);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("VerifyAddTargetData() failed. STATUS=%08lx\n", status));
		return status;
	}


	//
	//	Start verifying
	//

	fault_cnt = 0;
	oriDibValid = FALSE;
	exitImm = FALSE;
	for(idx_unit = 0; idx_unit < AddTargetData->ulNumberOfUnitDiskList; idx_unit++) {
		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("== UNIT #%u ==\n", idx_unit));

		unit = AddTargetData->UnitDiskList + idx_unit;
		LSTRANS_COPY_LPXADDRESS(&targetAddr, &unit->Address);
		LSTRANS_COPY_LPXADDRESS(&bindingAddr, &unit->NICAddr);
		if(SecondaryAddress)
			LSTRANS_COPY_LPXADDRESS(&bindingAddr2, &SecondaryAddress->Address[0].Address);

		BUILD_LOGININFO(&loginInfo, unit);

		if(unit->ucHWVersion <= LANSCSIIDE_VERSION_2_0)
			loginInfo.UserID = CONVERT_TO_ROUSERID(loginInfo.UserID);


		//
		//	Query binding address
		//

		if(SecondaryAddress)
			status = LsuQueryBindingAddress(&boundAddr, &targetAddr, &bindingAddr, &bindingAddr2, TRUE);
		else
			status = LsuQueryBindingAddress(&boundAddr, &targetAddr, &bindingAddr, NULL, TRUE);

		if(!NT_SUCCESS(status)) {
			unit->LurnOptions |= LURNOPTION_MISSING;
			fault_cnt ++;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LsuQueryBindingAddress() failed. STATUS=%08lx\n", status));
			continue;
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
			PBLOCK_ACCESS_CONTROL_LIST	bacl = NULL;
			union {
				NDAS_DIB	V1;
				NDAS_DIB_V2	V2;
			}	DIB;


			status = NCommGetDIBV2(	&DIB.V2,
									&bacl,
									&loginInfo,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_DIB_V2,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_ENCRYPT,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_BACL,
									&targetAddr,
									&bindingAddr,
									unit->UDMARestrict);
			if(NT_SUCCESS(status)) {
				//
				//	Disk information block Version 2
				//
				if(DIB.V2.iMediaType != NMT_SINGLE) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: DIB's disktype mismatch. MediaType:%x\n", DIB.V2.iMediaType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					break;
				}

				//
				//	Verify block access control list
				//
				status = NCommVerifyNdasBlockACLWithTargetData(AddTargetData, DIB.V2.BACLSize, bacl);
				if(!NT_SUCCESS(status))
					break;

				status = STATUS_SUCCESS;
			} else if(status == STATUS_REVISION_MISMATCH) {
				//
				//	Disk information block Version 1
				//
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("NORMAL: DIBV2 doesn't exist. try DIBV1. NTSTATUS:%08lx\n", status));
				status = NCommGetDIBV1(	&DIB.V1,
									&loginInfo,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_DIB_V1,
									&targetAddr,
									&bindingAddr,
									unit->UDMARestrict);

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

		case NDASSCSI_TYPE_DISK_RAID1R2:	
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R2: Not supported.\n"));
			status = STATUS_NOT_SUPPORTED;
			break;
			
		case NDASSCSI_TYPE_DISK_RAID1R3:	{			
			PBLOCK_ACCESS_CONTROL_LIST	bacl = NULL;
			NDAS_DIB_V2	DIBV2;

			//
			//	Disk information block Version 2
			//

			//
			// ulPhysicalBlocks can be 0 if mounted in degraded mode.
			// Temporary fix: 	Just don't mount.
			//			to do later: read RMD and compare RAID set ID and config set ID.
			if (unit->ulPhysicalBlocks ==0) {
				exitImm = TRUE;
				status = STATUS_OBJECT_TYPE_MISMATCH;
				break;
			}
			status = NCommGetDIBV2(	&DIBV2,
									&bacl,
									&loginInfo,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_DIB_V2,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_ENCRYPT,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_BACL,
									&targetAddr,
									&bindingAddr,
									unit->UDMARestrict);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R3: NCommGetDIBV2() failed. NTSTATUS:%08lx\n", status));
				exitImm = TRUE;
			}

			if(DIBV2.iMediaType != NdasDevType2DIBType(AddTargetData->ucTargetType)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R3: DIBv2's disktype mismatch. MediaType:%x\n", DIBV2.iMediaType));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				exitImm = TRUE;
				break;
			}

			if(DIBV2.nDiskCount != AddTargetData->ulNumberOfUnitDiskList) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R3: DIBv2's Diskcount mismatch."
					" numberofunitdisklist=%u ndiskcount=%u\n", AddTargetData->ulNumberOfUnitDiskList, DIBV2.nDiskCount));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				exitImm = TRUE;
				break;
			}

			if(DIBV2.iSequence != idx_unit) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R3: DIBv2's sequence number mismatch."
											" idx_unit=%u Seq=%u\n", idx_unit, DIBV2.iSequence));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				exitImm = TRUE;
				break;
			}

			//
			//	Compare Unit crc with the other units.
			//

			if(oriDibValid == TRUE){
				if(DIBV2.crc32_unitdisks != crcunit) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R3: DIBv2's unit crc mismatch."
						" original=%08lx crc unit=%08lx\n", crcunit, DIBV2.crc32_unitdisks));

					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
					break;
				}
			} else {
				//
				//	Set original DIB information to compare with the rest of units.
				//

				crcunit = DIBV2.crc32_unitdisks;
				oriDibValid = TRUE;

				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1R3: Set original CRC."
								" original=%08lx\n", crcunit));
			}

			//
			//	Verify block access control list
			//
			status = NCommVerifyNdasBlockACLWithTargetData(AddTargetData, DIBV2.BACLSize, bacl);
			if(!NT_SUCCESS(status)) {
				exitImm = TRUE;
				break;
			}

			status = STATUS_SUCCESS;
			break;
		}
		case NDASSCSI_TYPE_DISK_RAID4R2:
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRROR: Not supported.\n"));
			status = STATUS_NOT_SUPPORTED;
			break;

		case NDASSCSI_TYPE_DISK_RAID0:			
		case NDASSCSI_TYPE_DISK_RAID4R3: {
			PBLOCK_ACCESS_CONTROL_LIST	bacl = NULL;
			NDAS_DIB_V2	DIBV2;


			status = NCommGetDIBV2(	&DIBV2,
									&bacl,
									&loginInfo,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_DIB_V2,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_ENCRYPT,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_BACL,
									&targetAddr,
									&bindingAddr,
									unit->UDMARestrict);
			if(status == STATUS_REVISION_MISMATCH) {
				exitImm = TRUE;
				break;
			}
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID0/4: could not read DIBV2. NTSTATUS:%x\n", status));
				unit->LurnOptions |= LURNOPTION_MISSING;
				fault_cnt ++;
				break;
			}


			//
			//	Disk information block Version 2
			//

			if(DIBV2.iMediaType != NdasDevType2DIBType(AddTargetData->ucTargetType) ||
				DIBV2.iSequence != idx_unit
				) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID0/4: DIBv2's disktype mismatch. MediaType:%x Sequence:%d\n",
														DIBV2.iMediaType,
														DIBV2.iSequence));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				exitImm = TRUE;
				break;
			}

			//
			//	Compare Unit crc with the other units.
			//

			if(oriDibValid == TRUE){
				if(DIBV2.crc32_unitdisks != crcunit) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID0/4: DIBv2's unit crc mismatch."
						" original=%08lx crc unit=%08lx\n", crcunit, DIBV2.crc32_unitdisks));

					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
					break;
				}
			} else {
				//
				//	Set original DIB information to compare with the rest of units.
				//

				crcunit = DIBV2.crc32_unitdisks;
				oriDibValid = TRUE;

				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID0/4: Set original CRC."
								" original=%08lx\n", crcunit));
			}

			//
			//	Verify block access control list
			//
			status = NCommVerifyNdasBlockACLWithTargetData(AddTargetData, DIBV2.BACLSize, bacl);
			if(!NT_SUCCESS(status)) {
				exitImm = TRUE;
				break;
			}

			status = STATUS_SUCCESS;
			break;
		}

		case NDASSCSI_TYPE_DISK_MIRROR:
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRROR: Not supported.\n"));
			status = STATUS_NOT_SUPPORTED;
			break;
			
		case NDASSCSI_TYPE_DISK_AGGREGATION: {
			PBLOCK_ACCESS_CONTROL_LIST	bacl = NULL;
			union {
				NDAS_DIB	V1;
				NDAS_DIB_V2	V2;
			}	DIB;

			//
			//	Disk information block Version 2
			//

			status = NCommGetDIBV2(	&DIB.V2,
									&bacl,
									&loginInfo,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_DIB_V2,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_ENCRYPT,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_BACL,
									&targetAddr,
									&bindingAddr,
									unit->UDMARestrict);
			if(NT_SUCCESS(status)) {
				if(DIB.V2.iMediaType != NdasDevType2DIBType(AddTargetData->ucTargetType)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv2's disktype mismatch. MediaType:%x\n", DIB.V2.iMediaType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
					break;
				}

				if(DIB.V2.nDiskCount != AddTargetData->ulNumberOfUnitDiskList) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv2's Diskcount mismatch."
						" numberofunitdisklist=%u ndiskcount=%u\n", AddTargetData->ulNumberOfUnitDiskList, DIB.V2.nDiskCount));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
					break;
				}

				if(DIB.V2.iSequence != idx_unit) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv2's sequence number mismatch."
											" idx_unit=%u Seq=%u\n", idx_unit, DIB.V2.iSequence));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
					break;
				}

				//
				//	Verify block access control list
				//
				status = NCommVerifyNdasBlockACLWithTargetData(AddTargetData, DIB.V2.BACLSize, bacl);
				if(!NT_SUCCESS(status)) {
					exitImm = TRUE;
					break;
				}

				status = STATUS_SUCCESS;

			} else if(status == STATUS_REVISION_MISMATCH) {

				//
				//	Disk information block Version 1
				//

				status = NCommGetDIBV1(	&DIB.V1,
									&loginInfo,
									unit->ulPhysicalBlocks + NDAS_BLOCK_LOCATION_DIB_V1,
									&targetAddr,
									&bindingAddr,
									unit->UDMARestrict);
				if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: NCommGetDIBV1() failed. NTSTATUS:%08lx\n", status));

					//
					//	Set fault_cnt to exit immediately from this loop.
					//

					fault_cnt = AddTargetData->ulNumberOfUnitDiskList;
					break;
				}
				if(LagacyMirrAggrType2TargetType(DIB.V1.DiskType) != AddTargetData->ucTargetType) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv1's disktype mismatch. Disktype:%x\n", DIB.V1.DiskType));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
				}

				if(LagacyMirrAggrType2SeqNo(DIB.V1.DiskType) != idx_unit) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: DIBv1's sequnece mismatch."
								" DiskType:%x expected seq:%u\n", DIB.V1.DiskType, idx_unit));
					status = STATUS_OBJECT_TYPE_MISMATCH;
					exitImm = TRUE;
					break;
				}
			} else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("MIRAGR: NCommGetDIBV2() failed. NTSTATUS:%08lx\n", status));
				exitImm = TRUE;
			}

			status = STATUS_SUCCESS;
		break;
		}

		case NDASSCSI_TYPE_DVD:
		case NDASSCSI_TYPE_VDVD:
		case NDASSCSI_TYPE_MO:
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

		if(exitImm) {
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Exit from this verification loop immediately.\n"));
			break;
		}
	}


	//	Online-recoverable RAIDs.
	//	Even if only one is successfully detected,
	//	go ahead to the plug-in process.
	//

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Fault count = %u\n", fault_cnt));
	if(AddTargetData->ucTargetType == NDASSCSI_TYPE_DISK_RAID1R3 ||
		AddTargetData->ucTargetType == NDASSCSI_TYPE_DISK_RAID4R3
		) {
#if 0
		//
		// Boot-up mount policy: Do not mount if any of the member is missing
		//
		if (fault_cnt !=0) {
			status = STATUS_UNSUCCESSFUL;
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Member is missing. Denying auto mount.\n"));
		}
#else
		//
		// Boot-up mount policy: Do not mount if any of the member is missing
		//
		if (fault_cnt !=0) {
			status = STATUS_UNSUCCESSFUL;
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Member is missing. Denying auto mount.\n"));
		}
#endif
	} else {

		//
		//	Check the fault counter because RAID0 may set status to SUCCESS
		//		even if a member is in fault.
		//

		if(NT_SUCCESS(status)) {
			if(fault_cnt) {
				status = STATUS_UNSUCCESSFUL;
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Fault count=%u.\n", fault_cnt));
			}
		}
	}

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Event log
//

VOID
NDBusIoctlLogError(
	IN PDEVICE_OBJECT	DeviceObject,
	IN UINT32			ErrorCode,
	IN UINT32			IoctlCode,
	IN UINT32			Parameter2
){
	LSU_ERROR_LOG_ENTRY errorLogEntry;


	//
    // Save the error log data in the log entry.
    //

	errorLogEntry.ErrorCode = ErrorCode;
	errorLogEntry.MajorFunctionCode = IRP_MJ_DEVICE_CONTROL;
	errorLogEntry.IoctlCode = IoctlCode;
    errorLogEntry.UniqueId = 0;
	errorLogEntry.SequenceNumber = 0;
	errorLogEntry.ErrorLogRetryCount = 0;
	errorLogEntry.Parameter2 = Parameter2;
	errorLogEntry.DumpDataEntry = 0;

	LsuWriteLogErrorEntry(DeviceObject, &errorLogEntry);

	return;
}


//////////////////////////////////////////////////////////////////////////
//
//	LfsFilter control
//

#include "lfsfilterpublic.h"
#include "devreg.h"


//
//	LfsFilter device's names.
//	Use Dos device name which is common in all Windows NT versions.
//

const PWCHAR LFSFILT_CTLDEVICE_NAME = LFS_FILTER_DOSDEVICE_NAME;
const PWCHAR LFSFILT_DRIVER_NAME = LFS_FILTER_DRIVER_NAME;
const PWCHAR LFSFILT_SERVICE_NAME = LFS_FILTER_SERVICE_NAME;

//
//	LfsFilter driver service
//

NTSTATUS
LfsFiltDriverServiceExist() {
	return DrDriverServiceExist(LFSFILT_SERVICE_NAME);
}


//
//	IOCTL primitives
//

static
NTSTATUS
LfsFiltOpenControl (
	OUT PHANDLE					ControlFileHandle, 
	OUT	PFILE_OBJECT			*ControlFileObject
	)
{
	HANDLE						controlFileHandle; 
	PFILE_OBJECT				controlFileObject;

	UNICODE_STRING	    		nameString;
	OBJECT_ATTRIBUTES	    	objectAttributes;
	IO_STATUS_BLOCK				ioStatusBlock;
	NTSTATUS	    			status;
	
	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered\n"));

	//
	// Init object attributes
	//

	RtlInitUnicodeString (&nameString, LFSFILT_CTLDEVICE_NAME);
	InitializeObjectAttributes (
		&objectAttributes,
		&nameString,
		0,
		NULL,
		NULL
		);

	status = ZwCreateFile(
				&controlFileHandle,
				GENERIC_READ,
				&objectAttributes,
				&ioStatusBlock,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				0,
				0,
				0,
				NULL,	// Open as control
				0		// 
				);

	if (!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FAILURE, ZwCreateFile returned status code=%x\n", status));
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}

	status = ioStatusBlock.Status;

	if (!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FAILURE, IoStatusBlock.Status contains status code=%x\n", status));
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}

	status = ObReferenceObjectByHandle (
		        controlFileHandle,
		        0L,
		        NULL,
		        KernelMode,
		        (PVOID *) &controlFileObject,
		        NULL
				);

	if (!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ObReferenceObjectByHandle() failed. STATUS=%08lx\n", status));
		ZwClose(controlFileHandle);
		*ControlFileHandle = NULL;
		*ControlFileObject = NULL;
		return status;
	}
	 
	*ControlFileHandle = controlFileHandle;
	*ControlFileObject = controlFileObject;

	return status;
}

static
NTSTATUS
LfsFiltCloseControl (
	IN	HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject
	)
{
	NTSTATUS status;

	if(ControlFileObject)
		ObDereferenceObject(ControlFileObject);

	if(!ControlFileHandle)
		return STATUS_SUCCESS;

	status = ZwClose (ControlFileHandle);

	if (!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FAILURE, NtClose returned status code=%x\n", status));
	} else {
		Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("SUCCESS.\n"));
	}

	return status;
}


static
NTSTATUS
LfsFiltIoControl(
	IN  HANDLE			ControlFileHandle,
	IN	PFILE_OBJECT	ControlFileObject,
	IN	ULONG    		IoControlCode,
	IN	PVOID    		InputBuffer OPTIONAL,
	IN	ULONG    		InputBufferLength,
	OUT PVOID	    	OutputBuffer OPTIONAL,
	IN	ULONG    		OutputBufferLength
	)
{
	NTSTATUS		ntStatus;
	PDEVICE_OBJECT	deviceObject;
	PIRP			irp;
	KEVENT	    	event;
	IO_STATUS_BLOCK	ioStatus;

	UNREFERENCED_PARAMETER(ControlFileHandle);

	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered\n"));

	deviceObject = IoGetRelatedDeviceObject(ControlFileObject);
	
	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp = IoBuildDeviceIoControlRequest(
			IoControlCode,
			deviceObject,
			InputBuffer,
			InputBufferLength,
			OutputBuffer,
			OutputBufferLength,
			FALSE,
			&event,
			&ioStatus
			);
	
	if (irp == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	if (ntStatus == STATUS_PENDING)
	{
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		ntStatus = ioStatus.Status;
	}

	return ntStatus;
}

//
//	IOCTLs
//

NTSTATUS
LfsCtlGetVersions(
	PUSHORT VerMajor,
	PUSHORT VerMinor,
	PUSHORT VerBuild,
	PUSHORT VerPrivate,
	PUSHORT NDFSVerMajor,
	PUSHORT NDFSVerMinor
){
	NTSTATUS		status;
	HANDLE			handle;
	PFILE_OBJECT	fileObject;
	LFSFILT_VER verInfo = {0};

	status = LfsFiltOpenControl(&handle, &fileObject);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	status = LfsFiltIoControl(	handle,
						fileObject,
						LFS_FILTER_GETVERSION,
						NULL,
						0,
						&verInfo,
						sizeof(LFSFILT_VER)
						);

	if(NT_SUCCESS(status)) {
		if(VerMajor) *VerMajor = verInfo.VersionMajor;
		if(VerMinor) *VerMinor = verInfo.VersionMinor;
		if(VerBuild) *VerBuild = verInfo.VersionBuild;
		if(VerPrivate) *VerPrivate = verInfo.VersionPrivate;
		if(NDFSVerMajor) *NDFSVerMajor = verInfo.VersionNDFSMajor;
		if(NDFSVerMinor) *NDFSVerMinor = verInfo.VersionNDFSMinor;
	}

	LfsFiltCloseControl(handle, fileObject);

	return status;
}


NTSTATUS
LfsCtlIsReady()
{
	NTSTATUS		status;
	HANDLE			handle;
	PFILE_OBJECT	fileObject;

	status = LfsFiltOpenControl(&handle, &fileObject);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	status = LfsFiltIoControl(	handle,
								fileObject,
								LFS_FILTER_READY,
								NULL,
								0,
								NULL,
								0
								);

	LfsFiltCloseControl(handle, fileObject);

	return status;
}
