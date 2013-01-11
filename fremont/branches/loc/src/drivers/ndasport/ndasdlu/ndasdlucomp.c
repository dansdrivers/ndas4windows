#include "ndasport.h"
#include "trace.h"
#include "ndasdlu.h"
#include <ntdddisk.h>
#include "constants.h"

#include <initguid.h>
#include "ndasdluguid.h"
#include "lslur.h"

#ifdef RUN_WPP
#include "ndasdlu.tmh"
#endif

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DLUCOMP"




//
// Translate CCB status to SRB status
//

VOID
CcbStatusToSrbStatus(PCCB Ccb, PSCSI_REQUEST_BLOCK Srb) {

	Srb->DataTransferLength -= Ccb->ResidualDataLength;

	switch(Ccb->CcbStatus) {
		case CCB_STATUS_SUCCESS:

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_NOT_EXIST:

			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_INVALID_COMMAND:

			Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_LOST_LOCK:
		case CCB_STATUS_COMMAND_FAILED:

			Srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
			Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			break;

		case CCB_STATUS_BAD_SECTOR:

			Srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
			Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			break;

		case CCB_STATUS_COMMMAND_DONE_SENSE2:
			Srb->SrbStatus =  SRB_STATUS_BUSY  | SRB_STATUS_AUTOSENSE_VALID ;
			Srb->DataTransferLength = 0;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMMAND_DONE_SENSE:
			Srb->SrbStatus =  SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID ;
			Srb->DataTransferLength = 0;
			Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			break;

		case CCB_STATUS_RESET:

			Srb->SrbStatus = SRB_STATUS_BUS_RESET;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_DATA_OVERRUN:
			Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMUNICATION_ERROR:
			{
				PSENSE_DATA	senseData;

				Srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
				Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
				Srb->DataTransferLength = 0;

				senseData = Srb->SenseInfoBuffer;

				senseData->ErrorCode = 0x70;
				senseData->Valid = 1;
				//senseData->SegmentNumber = 0;
				senseData->SenseKey = SCSI_SENSE_HARDWARE_ERROR;	//SCSI_SENSE_MISCOMPARE;
				//senseData->IncorrectLength = 0;
				//senseData->EndOfMedia = 0;
				//senseData->FileMark = 0;

				senseData->AdditionalSenseLength = 0xb;
				senseData->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
				senseData->AdditionalSenseCodeQualifier = 0;
			}
			break;

		case CCB_STATUS_BUSY:
			Srb->SrbStatus = SRB_STATUS_BUSY;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			KDPrint(1,("CCB_STATUS_BUSY\n"));
			break;

			//
			//	Stop one LUR
			//
		case CCB_STATUS_STOP: {
			Srb->SrbStatus = SRB_STATUS_BUS_RESET;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			Srb->DataTransferLength = 0;
			KDPrint(1,("CCB_STATUS_STOP. Stopping!\n"));
			break;
		}
		default:
			// Error in Connection...
			// CCB_STATUS_UNKNOWN_STATUS, CCB_STATUS_RESET, and so on.
			Srb->SrbStatus = SRB_STATUS_ERROR;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			Srb->DataTransferLength = 0;
	}
}

//
// Copy query output to SRB
//

static
NTSTATUS
NdscCopyQueryOutputToSrb(
	PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	ULONG						CcbBufferLength,
	PUCHAR						CcbBuffer,
	ULONG						SrbIoctlBufferLength,
	PUCHAR						SrbIoctlBuffer
){
	NTSTATUS		status;
	PLUR_QUERY		lurQuery;
	PNDAS_DLU_EXTENSION			ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	lurQuery = (PLUR_QUERY)CcbBuffer;
	if(CcbBufferLength < FIELD_OFFSET(LUR_QUERY, QueryDataLength)) {
		return STATUS_INVALID_PARAMETER;
	}

	status = STATUS_SUCCESS;
	switch(lurQuery->InfoClass) {
		case LurPrimaryLurnInformation: { // NdscPrimaryUnitDiskInformation
			PLURN_PRIMARYINFORMATION	lurPrimaryInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION(lurQuery);
			PNDSCIOCTL_PRIMUNITDISKINFO	primUnitDisk = (PNDSCIOCTL_PRIMUNITDISKINFO)SrbIoctlBuffer;

			if(CcbBufferLength < sizeof(LURN_PRIMARYINFORMATION)) {
				return STATUS_INVALID_PARAMETER;
			}
			if(SrbIoctlBufferLength < sizeof(NDSCIOCTL_PRIMUNITDISKINFO)) {
				return STATUS_INVALID_PARAMETER;
			}

			//
			//	Set length.
			//
			primUnitDisk->Length					= sizeof(NDSCIOCTL_PRIMUNITDISKINFO);
			primUnitDisk->UnitDisk.Length			= sizeof(NDSC_UNITDISK);
			//
			//	Adapter information
			//
			primUnitDisk->EnabledTime.QuadPart		=	ndasDluExtension->EnabledTime.QuadPart;

			//
			// LUR information ( Scsi LU information )
			//
			primUnitDisk->Lur.Length		= sizeof(NDSC_LUR);
			primUnitDisk->Lur.TargetId		= ndasDluExtension->LUR->LurId[1];
			primUnitDisk->Lur.Lun			= ndasDluExtension->LUR->LurId[2];
			primUnitDisk->Lur.LurnCnt		= ndasDluExtension->LUR->NodeCount;
			primUnitDisk->Lur.DeviceMode	= ndasDluExtension->LUR->DeviceMode;
			primUnitDisk->Lur.SupportedFeatures	= ndasDluExtension->LUR->SupportedNdasFeatures;
			primUnitDisk->Lur.EnabledFeatures	= ndasDluExtension->LUR->EnabledNdasFeatures;

			//
			//	Unit device
			//

			primUnitDisk->UnitDisk.UnitDiskId		= lurPrimaryInfo->PrimaryLurn.UnitDiskId;
			primUnitDisk->UnitDisk.Connections		= lurPrimaryInfo->PrimaryLurn.Connections;
			primUnitDisk->UnitDisk.GrantedAccess	= lurPrimaryInfo->PrimaryLurn.AccessRight;
			RtlCopyMemory(
				primUnitDisk->UnitDisk.UserID,
				&lurPrimaryInfo->PrimaryLurn.UserID,
				sizeof(primUnitDisk->UnitDisk.UserID)
				);
			RtlCopyMemory(
				primUnitDisk->UnitDisk.Password,
				&lurPrimaryInfo->PrimaryLurn.Password,
				sizeof(primUnitDisk->UnitDisk.Password)
				);
#if 0
			RtlCopyMemory(	&primUnitDisk->UnitDisk.NetDiskAddress,
				&lurPrimaryInfo->PrimaryLurn.NetDiskAddress,
				sizeof(TA_LSTRANS_ADDRESS)
				);
			RtlCopyMemory(	&primUnitDisk->UnitDisk.BindingAddress,
				&lurPrimaryInfo->PrimaryLurn.BindingAddress,
				sizeof(TA_LSTRANS_ADDRESS)
				);
#else
			primUnitDisk->UnitDisk.NetDiskAddress.TAAddressCount = 1;
			primUnitDisk->UnitDisk.NetDiskAddress.Address[0].AddressLength 
				= lurPrimaryInfo->PrimaryLurn.NdasNetDiskAddress.AddressLength;
			primUnitDisk->UnitDisk.NetDiskAddress.Address[0].AddressType = 
				lurPrimaryInfo->PrimaryLurn.NdasNetDiskAddress.AddressType;

			RtlCopyMemory( &primUnitDisk->UnitDisk.NetDiskAddress.Address[0].Address,
				&lurPrimaryInfo->PrimaryLurn.NdasNetDiskAddress.Address[0], 
				lurPrimaryInfo->PrimaryLurn.NdasNetDiskAddress.AddressLength );

			primUnitDisk->UnitDisk.BindingAddress.TAAddressCount = 1;
			primUnitDisk->UnitDisk.BindingAddress.Address[0].AddressLength 
				= lurPrimaryInfo->PrimaryLurn.NdasBindingAddress.AddressLength;
			primUnitDisk->UnitDisk.BindingAddress.Address[0].AddressType = 
				lurPrimaryInfo->PrimaryLurn.NdasBindingAddress.AddressType;

			RtlCopyMemory( &primUnitDisk->UnitDisk.BindingAddress.Address[0].Address,
				&lurPrimaryInfo->PrimaryLurn.NdasBindingAddress.Address[0], 
				lurPrimaryInfo->PrimaryLurn.NdasBindingAddress.AddressLength );


#endif
			primUnitDisk->UnitDisk.UnitBlocks		= (UINT32)lurPrimaryInfo->PrimaryLurn.UnitBlocks;
			primUnitDisk->UnitDisk.SlotNo			= ndasDluExtension->LogicalUnitAddress;
			RtlCopyMemory(primUnitDisk->NDSC_ID, lurPrimaryInfo->PrimaryLurn.PrimaryId, LURN_PRIMARY_ID_LENGTH);

			KDPrint(2,("NDSC_ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
				primUnitDisk->NDSC_ID[0], primUnitDisk->NDSC_ID[1], primUnitDisk->NDSC_ID[2], primUnitDisk->NDSC_ID[3], 
				primUnitDisk->NDSC_ID[4], primUnitDisk->NDSC_ID[5], primUnitDisk->NDSC_ID[6], primUnitDisk->NDSC_ID[7], 
				primUnitDisk->NDSC_ID[8], primUnitDisk->NDSC_ID[9], primUnitDisk->NDSC_ID[10], primUnitDisk->NDSC_ID[11], 
				primUnitDisk->NDSC_ID[12], primUnitDisk->NDSC_ID[13], primUnitDisk->NDSC_ID[14], primUnitDisk->NDSC_ID[15])
				);
		break;
		}
		case LurEnumerateLurn: { // NdscLurInformation
			PLURN_ENUM_INFORMATION	lurnEnumInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION(lurQuery);
			PNDSCIOCTL_LURINFO		info = (PNDSCIOCTL_LURINFO)SrbIoctlBuffer;
			NDAS_DEV_ACCESSMODE		deviceMode;
			NDAS_FEATURES			supportedNdasFeatures, enabledNdasFeatures;
			PNDSC_LURN_FULL			unitDisk;
			ULONG					idx_lurn;
			PLURN_INFORMATION		lurnInformation;
			ULONG					returnLength;
			UINT32				nodeCount;

			if(CcbBufferLength < sizeof(LURN_PRIMARYINFORMATION)) {
				return STATUS_INVALID_PARAMETER;
			}
			if(SrbIoctlBufferLength < FIELD_OFFSET(NDSCIOCTL_LURINFO, Reserved1)) {
				return STATUS_INVALID_PARAMETER;
			}

			if (ndasDluExtension->LUR) {
				ASSERT(ndasDluExtension->LUR->NodeCount >= 1);
				deviceMode = ndasDluExtension->LUR->DeviceMode;
				supportedNdasFeatures = ndasDluExtension->LUR->SupportedNdasFeatures;
				enabledNdasFeatures = ndasDluExtension->LUR->EnabledNdasFeatures;
				nodeCount = ndasDluExtension->LUR->NodeCount;
			} else {
				deviceMode = 0;
				supportedNdasFeatures = 0;
				enabledNdasFeatures = 0;
				nodeCount = 0;
			}

			//
			//	Adapter information
			//

			info->Length			=	FIELD_OFFSET(NDSCIOCTL_LURINFO, Lurns) +
										sizeof(NDSC_LURN_FULL) *
										nodeCount;

			// return length check
			returnLength = FIELD_OFFSET(NDSCIOCTL_LURINFO, Lurns);
			if(SrbIoctlBufferLength < returnLength) {
				return STATUS_BUFFER_TOO_SMALL;
			}

			info->EnabledTime.QuadPart					=	ndasDluExtension->EnabledTime.QuadPart;
			info->Lur.Length		= sizeof(NDSC_LUR);
			info->Lur.TargetId		= ndasDluExtension->LUR->LurId[1];
			info->Lur.Lun			= ndasDluExtension->LUR->LurId[2];
			info->Lur.LurnCnt		= ndasDluExtension->LUR->NodeCount;
			info->LurnCnt		= info->Lur.LurnCnt;
			info->Lur.DeviceMode = deviceMode;
			info->Lur.SupportedFeatures = supportedNdasFeatures;
			info->Lur.EnabledFeatures = enabledNdasFeatures;

			//
			//	Set return values for each LURN.
			//
			for(idx_lurn = 0; idx_lurn < info->LurnCnt; idx_lurn++) {

				//
				//	Add one LURN to return bytes and check the user buffer size.
				//
				returnLength += sizeof(NDSC_LURN_FULL);
				if(SrbIoctlBufferLength < returnLength) {
					status = STATUS_BUFFER_TOO_SMALL;
					// Do not exit. Must return full length.
					continue;
				}

				unitDisk = info->Lurns + idx_lurn;
				lurnInformation = lurnEnumInfo->Lurns + idx_lurn;

				unitDisk->Length = sizeof(NDSC_LURN_FULL);
				unitDisk->UnitDiskId = lurnInformation->UnitDiskId;
				unitDisk->Connections = lurnInformation->Connections;
				unitDisk->AccessRight = lurnInformation->AccessRight;
				unitDisk->UnitBlocks = lurnInformation->UnitBlocks;
				unitDisk->StatusFlags = lurnInformation->StatusFlags;
				unitDisk->LurnId = lurnInformation->LurnId;
				unitDisk->LurnType = lurnInformation->LurnType;
				unitDisk->StatusFlags = lurnInformation->StatusFlags;

				RtlCopyMemory(
					unitDisk->UserID,
					&lurnInformation->UserID,
					sizeof(unitDisk->UserID)
					);

				RtlCopyMemory(
					unitDisk->Password,
					&lurnInformation->Password,
					sizeof(unitDisk->Password)
					);
#if 0
				RtlCopyMemory(	&unitDisk->NetDiskAddress,
					&lurnInformation->NetDiskAddress,
					sizeof(TA_LSTRANS_ADDRESS)
					);
				RtlCopyMemory(	&unitDisk->BindingAddress,
					&lurnInformation->BindingAddress,
					sizeof(TA_LSTRANS_ADDRESS)
					);
#else

				unitDisk->NetDiskAddress.TAAddressCount = 1;
				unitDisk->NetDiskAddress.Address[0].AddressLength 
					= lurnInformation->NdasNetDiskAddress.AddressLength;
				unitDisk->NetDiskAddress.Address[0].AddressType = 
					lurnInformation->NdasNetDiskAddress.AddressType;

				RtlCopyMemory( &unitDisk->NetDiskAddress.Address[0].Address,
					&lurnInformation->NdasNetDiskAddress.Address[0], 
					lurnInformation->NdasNetDiskAddress.AddressLength );

				unitDisk->BindingAddress.TAAddressCount = 1;
				unitDisk->BindingAddress.Address[0].AddressLength 
					= lurnInformation->NdasBindingAddress.AddressLength;
				unitDisk->BindingAddress.Address[0].AddressType = 
					lurnInformation->NdasBindingAddress.AddressType;

				RtlCopyMemory( &unitDisk->BindingAddress.Address[0].Address,
					&lurnInformation->NdasBindingAddress.Address[0], 
					lurnInformation->NdasBindingAddress.AddressLength );

#endif
			}
			break;
		 }

		default:
			status = STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}
//
//	MACRO for the convenience
//

#define NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(ERRORCODE, EVT_ID)	\
	NdasDluLogError(												\
							LogicalUnitExtension,					\
							srb,									\
							srb->PathId,							\
							srb->TargetId,							\
							srb->Lun,								\
				(ERRORCODE),										\
				EVTLOG_UNIQUEID(EVTLOG_MODULE_COMPLETION,			\
								EVT_ID,								\
								0))

//
//	NDASSCSI adapter device object's completion routine
//

NTSTATUS
NdasDluCcbCompletion(
		  IN PCCB							Ccb,
		  IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension
	  )
{
	KIRQL						oldIrql;
	static	LONG				SrbSeq;
	LONG						srbSeqIncremented;
	PSCSI_REQUEST_BLOCK			srb;
	PCCB						abortCcb;
	NTSTATUS					return_status;
	UINT32						AdapterStatus, AdapterStatusBefore;
	UINT32						NeedToUpdatePdoInfoInLSBus;
	BOOLEAN						busResetOccured;
	PNDAS_DLU_EXTENSION			ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);
	LONG						requestCount;

	KDPrint(3,("RequestExecuting = %d\n", ndasDluExtension->RequestExecuting));

	srb = Ccb->Srb;
	if(!srb) {
		KDPrint(1,("Ccb:%p CcbStatus %d. No srb assigned.\n", Ccb, Ccb->CcbStatus));
		ASSERT(srb);
		return STATUS_SUCCESS;
	}
 
	//
	//	NDASSCSI completion routine will do post operation to complete CCBs.
	//
	return_status = STATUS_SUCCESS;

	//
	// Set SRB completion sequence for debugging
	//

	srbSeqIncremented = InterlockedIncrement(&SrbSeq);

#if 0
	if(KeGetCurrentIrql() == PASSIVE_LEVEL) {
		if((srbSeqIncremented%100) == 0) {
			LARGE_INTEGER	interval;

			KDPrint(1,("Interval for debugging.\n"));

			interval.QuadPart = - 11 * 10000000; // 10 seconds
			KeDelayExecutionThread(KernelMode, FALSE, &interval);
		}
	}
#endif

	//
	// Update Adapter status flag
	//

	NeedToUpdatePdoInfoInLSBus = FALSE;

	ACQUIRE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, &oldIrql);

	// Save the bus-reset flag
	if(DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_BUSRESET_PENDING)) {
		busResetOccured = TRUE;
	} else {
		busResetOccured = FALSE;
	}

	// Save the current flag
	AdapterStatusBefore = ndasDluExtension->DluInternalStatus;


	//	Check reconnecting process.

	if(LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RECONNECTING))
	{
		DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING);
	} else {
		DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING);
	}

	if (!LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FLAG_VALID)) {

		ASSERT( Ccb->NdasrStatusFlag8 == 0 );

	} else {

		ASSERT( Ccb->NdasrStatusFlag8 == CCBSTATUS_FLAG_RAID_DEGRADED	 >> 8	||
			Ccb->NdasrStatusFlag8 == CCBSTATUS_FLAG_RAID_RECOVERING >> 8	||
			Ccb->NdasrStatusFlag8 == CCBSTATUS_FLAG_RAID_FAILURE    >> 8	||
			Ccb->NdasrStatusFlag8 == CCBSTATUS_FLAG_RAID_NORMAL     >> 8 );
	}


	// Update adapter status only when CCBSTATUS_FLAG_RAID_FLAG_VALID is on.
	// In other case, Ccb has no chance to get flag information from RAID.

	if (LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FLAG_VALID)) {

		//	Check to see if the associate member is in error.

		if (LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_DEGRADED)) {

			if (!DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)) {

				KDPrint(2, ("DLUINTERNAL_STATUSFLAG_MEMBER_FAULT is Set\n") );
			}

			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_MEMBER_FAULT);

		} else {

			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_MEMBER_FAULT);
		}

		//	Check recovering process.

		if(LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_RECOVERING))	{

			if (!DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECOVERING)) {

				KDPrint(2, ("DLUINTERNAL_STATUSFLAG_RECOVERING is Set\n") );
			}

			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECOVERING);

		} else {

			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECOVERING);
		}

		// Check RAID failure

		if(LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FAILURE))	{

			if (!DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_FAILURE)) {

				KDPrint(2, ("DLUINTERNAL_STATUSFLAG_RAID_FAILURE is Set\n") );
			}

			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_FAILURE);

		} else {

			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_FAILURE);
		}

		// Set RAID normal status

		if(LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_NORMAL))	{
		
			if (!DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_NORMAL)) {

				KDPrint(2, ("DLUINTERNAL_STATUSFLAG_RAID_NORMAL is Set\n") );
			}

			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_NORMAL);

		} else {

			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_NORMAL);
		}
	}

	// power-recycle occurred.
	if(LsCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_POWERRECYLE_OCCUR)) {

		DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_POWERRECYCLED);
	} else {
		DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_POWERRECYCLED);
	}

	if (DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_POWERRECYCLED)) {

		//ASSERT( FALSE );
	}

	AdapterStatus = ndasDluExtension->DluInternalStatus;
	RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);

	if(AdapterStatus != AdapterStatusBefore)
	{
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// DLUINTERNAL_STATUSFLAG_MEMBER_FAULT on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_MEMBER_FAULT, EVTLOG_MEMBER_IN_ERROR);
			KDPrint(1,("Ccb:%p CcbStatus %d. Set member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT) &&
			!(AdapterStatus & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// DLUINTERNAL_STATUSFLAG_MEMBER_FAULT off
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_MEMBER_FAULT_RECOVERED, EVTLOG_MEMBER_RECOVERED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Reset member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECONNECT_START, EVTLOG_START_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Start reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING) &&
			!(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING off
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECONNECTED, EVTLOG_END_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Finish reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECOVERING) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECOVERING)
			)
		{
			// DLUINTERNAL_STATUSFLAG_RECOVERING on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECOVERY_START, EVTLOG_START_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & (DLUINTERNAL_STATUSFLAG_RAID_FAILURE|DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RAID_NORMAL))
		{
			// DLUINTERNAL_STATUSFLAG_RECOVERING off
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECOVERED, EVTLOG_END_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Ended recovering\n", Ccb, Ccb->CcbStatus));
		}
		if (
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RAID_FAILURE)	 &&
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RAID_FAILURE))
		{
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RAID_FAILURE, EVTLOG_RAID_FAILURE);
			KDPrint(1,("Ccb:%p CcbStatus %d. RAID failure\n", Ccb, Ccb->CcbStatus));
		}
		
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_POWERRECYCLED) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_POWERRECYCLED)
			)
		{
			// DLUINTERNAL_STATUSFLAG_POWERRECYCLED on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_DISK_POWERRECYCLE, EVTLOG_DISK_POWERRECYCLED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}


		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	//	If CCB_OPCODE_UPDATE is successful, update adapter status in LanscsiBus
	//
	if(Ccb->OperationCode == CCB_OPCODE_UPDATE) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_UPGRADE_SUCC, EVTLOG_SUCCEED_UPGRADE);
		} else {
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_UPGRADE_FAIL, EVTLOG_FAIL_UPGRADE);
		}
		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	// Copy IO control results to the SRB buffer.
	//
	// If device lock CCB is successful, copy the result to the SRB.
	//

	if(Ccb->OperationCode == CCB_OPCODE_DEVLOCK) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			PSRB_IO_CONTROL			srbIoctlHeader;
			PUCHAR					lockIoctlBuffer;
			PNDSCIOCTL_DEVICELOCK	ioCtlAcReDeviceLock;
			PLURN_DEVLOCK_CONTROL	lurnAcReDeviceLock;

			//
			// Get the Ioctl buffer.
			//
			srbIoctlHeader = (PSRB_IO_CONTROL)srb->DataBuffer;
			srbIoctlHeader->ReturnCode = SRB_STATUS_SUCCESS;
			lockIoctlBuffer = (PUCHAR)(srbIoctlHeader + 1);
			ioCtlAcReDeviceLock = (PNDSCIOCTL_DEVICELOCK)lockIoctlBuffer;
			lurnAcReDeviceLock = (PLURN_DEVLOCK_CONTROL)Ccb->DataBuffer;

			// Copy the result
			RtlCopyMemory(	ioCtlAcReDeviceLock->LockData,
							lurnAcReDeviceLock->LockData,
							NDSCLOCK_LOCKDATA_LENGTH);
		}
	} else if(Ccb->OperationCode == CCB_OPCODE_QUERY) {

		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			PSRB_IO_CONTROL		srbIoctlHeader;
			NTSTATUS			copyStatus;

			srbIoctlHeader = (PSRB_IO_CONTROL)srb->DataBuffer;

			copyStatus = NdscCopyQueryOutputToSrb(
				LogicalUnitExtension,
				Ccb->DataBufferLength,
				Ccb->DataBuffer,
				srbIoctlHeader->Length,
				(PUCHAR)(srbIoctlHeader + 1)
			);
			if(copyStatus == STATUS_BUFFER_TOO_SMALL) {
				srbIoctlHeader->ReturnCode = SRB_STATUS_DATA_OVERRUN;
			}else if(NT_SUCCESS(copyStatus)) {
				srbIoctlHeader->ReturnCode = SRB_STATUS_SUCCESS;
			} else {
				srbIoctlHeader->ReturnCode = SRB_STATUS_ERROR;
			}
		}
	}

	KDPrint(4,("CcbStatus %d\n", Ccb->CcbStatus));

	//
	//	Translate CcbStatus to SrbStatus
	//

	CcbStatusToSrbStatus(Ccb, srb);

	//
	// Perform stop process when we get stop status.
	//

	if(Ccb->CcbStatus == CCB_STATUS_STOP) {
		//
		// Notify the abnormal removal of the LU.
		//
		KDPrint(1, ("LUR initiate stop procedure.\n"));

		//
		// Notify the abnormal stop event to the LU's event listeners.
		//

		DLUINTERNAL_SETSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_STOPPED);
		DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_ABNORMAL_TERMINAT);
		NdasDluFireEvent(
			LogicalUnitExtension, 
			ndasDluExtension->DluInternalStatus);

		//
		// Notify the bus chage detection to the NDAS port.
		//

		NdasPortNotification(
			BusChangeDetected,
			LogicalUnitExtension, 
			ndasDluExtension->PathId);

		//
		// Complete the SRB.
		// We have to notify next lu request even though we will complete
		// those next requests with errors.
		// This is NDASPort specific patch.
		//
		InterlockedDecrement(&ndasDluExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		NdasPortNotification(
			RequestComplete,
			LogicalUnitExtension,
			srb);

		NdasPortNotification(
			NextLuRequest,
			LogicalUnitExtension,
			NULL
			);


		return return_status;
	} else {
		//
		// Update PDO information on the NDAS bus.
		//
		if(NeedToUpdatePdoInfoInLSBus)
		{
			KDPrint(1, ("<<<<<<<<<<<<<<<< %08lx -> %08lx ADAPTER STATUS CHANGED"
				" >>>>>>>>>>>>>>>>\n", AdapterStatusBefore, AdapterStatus));
			NdasDluFireEvent(LogicalUnitExtension, ndasDluExtension->DluInternalStatus);
		}

		if(Ccb->CcbStatus == CCB_STATUS_BUSY) {
			NDASDLU_WORKITEM_INIT			workitemCtx;

			KDPrint(1, ("LUR initiate reconnect. Retry...\n"));

			// Clear the request count.
			InterlockedDecrement(&ndasDluExtension->RequestExecuting);
			LsuDecrementTdiClientInProgress();

			// Queue the retry worker.
			NDASDLU_INIT_WORKITEM(
				&workitemCtx,
				NdasDluRetryWorker,
				NULL,
				(PVOID)srb,
				(PVOID)NULL,
				NULL
			);
			NdasDluQueueWorkItem(LogicalUnitExtension, &workitemCtx);

			return return_status;
		}
	}

	//
	// Process Abort CCB.
	//
	abortCcb = Ccb->AbortCcb;

	if(abortCcb != NULL) {

		KDPrint(1,("abortSrb\n"));
		ASSERT(FALSE);

		((PSCSI_REQUEST_BLOCK)abortCcb->Srb)->SrbStatus = SRB_STATUS_ABORTED;

		InterlockedDecrement(&ndasDluExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		NdasPortNotification(
			RequestComplete,
			LogicalUnitExtension,
			(PSCSI_REQUEST_BLOCK)abortCcb->Srb);

	}

	//
	// Complete the current SRB
	//
	requestCount = InterlockedDecrement(&ndasDluExtension->RequestExecuting);
	LsuDecrementTdiClientInProgress();

	// Clear srb field.
	Ccb->Srb = NULL;

	//
	// NOTE: You can not access after this completion request
	//   if the CCB is allocated in SRB by NDASPort.
	// __NDASDLU_USE_POOL_FOR_CCBALLOCATION__ must be defined with current
	// implementation.
	//
	NdasPortNotification(
		RequestComplete,
		LogicalUnitExtension,
		srb);
	if(requestCount <= ndasDluExtension->MaximumRequests) {
		NdasPortNotification(
			NextLuRequest,
			LogicalUnitExtension,
			NULL
			);
	}


	return return_status;
}

