#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

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
	ULONG				pduFlags;
	BOOLEAN				dma;

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

	status = LsuConfigureIdeDisk(LSS, &pduFlags, &dma);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IdeConfigure() failed. NTSTATUS: %08lx.\n", status));
		return status;
	}
	if(dma == FALSE) {
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
	ULONG				pduFlags;
	BOOLEAN				dma;

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

	status = LsuConfigureIdeDisk(LSS, &pduFlags, &dma);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IdeConfigure() failed. NTSTATUS: %08lx.\n", status));
		return status;
	}
	if(dma == FALSE) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Disk does not support DMA. DMA required.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Read information block
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
VerifyAddTargetData(
	IN PLANSCSI_ADD_TARGET_DATA	AddTargetData
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
	case NDASSCSI_TYPE_DISK_RAID1:
	case NDASSCSI_TYPE_DISK_RAID4:
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
	BOOLEAN				oriDibValid;
	UINT32				crcunit;
	BOOLEAN				exitImm;

	//
	//	Parameter check
	//
	if(SecondaryAddress) {
		if(SecondaryAddress->Address[0].AddressType != TDI_ADDRESS_TYPE_LPX) {
			return STATUS_NOT_SUPPORTED;
		}
	}

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
		LSTRANS_COPY_LPXADDRESS(&bindingAddr2, &SecondaryAddress->Address[0].Address);

		BUILD_LOGININFO(&loginInfo, unit, MaxBlocksPerRequest);


		//
		//	Query binding address
		//

		status = LsuQueryBindingAddress(&boundAddr, &targetAddr, &bindingAddr, &bindingAddr2, TRUE);
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
			union {
				NDAS_DIB	V1;
				NDAS_DIB_V2	V2;
			}	DIB;


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

		case NDASSCSI_TYPE_DISK_RAID1: {
			NDAS_DIB_V2	DIBV2;

			//
			//	Disk information block Version 2
			//

			status = NCommGetDIBV2(	&DIBV2,
									&loginInfo,
									unit->ulPhysicalBlocks-2,
									&targetAddr,
									&bindingAddr);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1: NCommGetDIBV2() failed. NTSTATUS:%08lx\n", status));
				exitImm = TRUE;
			}

			if(DIBV2.iMediaType != NdasDevType2DIBType(AddTargetData->ucTargetType)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1: DIBv2's disktype mismatch. MediaType:%x\n", DIBV2.iMediaType));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				exitImm = TRUE;
				break;
			}

			if(DIBV2.nDiskCount != AddTargetData->ulNumberOfUnitDiskList) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1: DIBv2's Diskcount mismatch."
					" numberofunitdisklist=%u ndiskcount=%u\n", AddTargetData->ulNumberOfUnitDiskList, DIBV2.nDiskCount));
				status = STATUS_OBJECT_TYPE_MISMATCH;
				exitImm = TRUE;
				break;
			}

			if(DIBV2.iSequence != idx_unit) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1: DIBv2's sequence number mismatch."
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
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1: DIBv2's unit crc mismatch."
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

				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RAID1: Set original CRC."
								" original=%08lx\n", crcunit));
			}

			status = STATUS_SUCCESS;
			break;
		}
		case NDASSCSI_TYPE_DISK_RAID0:
		case NDASSCSI_TYPE_DISK_RAID4: {
			NDAS_DIB_V2	DIBV2;


			status = NCommGetDIBV2(	&DIBV2,
									&loginInfo,
									unit->ulPhysicalBlocks-2,
									&targetAddr,
									&bindingAddr);
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


	//	Recoverable RAIDs.
	//	Even if only one is successfully detected,
	//	go ahead to the plugin process.
	//

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Fault count = %u\n", fault_cnt));
	if( AddTargetData->ucTargetType == NDASSCSI_TYPE_DISK_RAID0 ||
		AddTargetData->ucTargetType == NDASSCSI_TYPE_DISK_RAID4
		) {

		if(fault_cnt < AddTargetData->ulNumberOfUnitDiskList) {
			status = STATUS_SUCCESS;
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Let's try mount this recoverable device.\n"));
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
