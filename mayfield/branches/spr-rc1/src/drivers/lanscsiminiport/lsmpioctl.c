#include "port.h"
#include <ntddk.h>
#include "KDebug.h"
#include "lsminiportioctl.h"
#include "LanscsiMiniport.h"
#include "LSCcb.h"
#include "LSLurn.h"
#include "public\ndas\ndasiomsg.h"
#include "ndasscsi.ver"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSMP_Ioctl"

UCHAR
SrbIoctlRecoverRaid(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		PMINIPORT_LU_EXTENSION		LuExtension,
		PLANSCSI_RECOVER_TARGET_DATA	RecoverTargetData,
		PNTSTATUS					NtStatus,
		ULONG						CurSrbSequence
) {
	UCHAR				status;
	PCCB				Ccb;
	NTSTATUS			ntStatus;

	UNREFERENCED_PARAMETER(RecoverTargetData);

	status = SRB_STATUS_SUCCESS;

	//
	//	Query to the LUR
	//
	ntStatus = LSCcbAllocate(&Ccb);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("LSCcbAllocate() failed.\n"));
		*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
		return SRB_STATUS_INVALID_REQUEST;
	}

	LSCCB_INITIALIZE(Ccb, CurSrbSequence);
	Ccb->OperationCode = CCB_OPCODE_RECOVER;
	LSCcbSetFlag(Ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED);

	if(LuExtension) {
		KDPrint(3,("going to LuExtention %p.\n", LuExtension));
		ntStatus = LurRequest(
			LuExtension->LUR,
			Ccb
			);
	} else {
		KDPrint(3,("going to default LuExtention 0.\n"));
		ntStatus = LurRequest(
			HwDeviceExtension->LURs[0],	// default: 0.
			Ccb
			);
	}
	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("LurRequest() failed.\n"));
		*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
		return SRB_STATUS_INVALID_REQUEST;
	}

	return SRB_STATUS_SUCCESS;
}

UCHAR
SrbIoctlQueryInfo(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		PMINIPORT_LU_EXTENSION		LuExtension,
		PLSMPIOCTL_QUERYINFO		QueryInfo,
		ULONG						OutputBufferLength,
		PUCHAR						OutputBuffer,
		PNTSTATUS					NtStatus,
		ULONG						CurSrbSequence
) {
	UCHAR				status;
	PCCB				Ccb;
	NTSTATUS			ntStatus;
	KIRQL				oldIrql;

	status = SRB_STATUS_SUCCESS;
	switch(QueryInfo->InfoClass) {
	case LsmpAdapterInformation: {
		PLSMPIOCTL_ADAPTERINFO	adapter = (PLSMPIOCTL_ADAPTERINFO)OutputBuffer;

		KDPrint(1,("LsmpAdapterInformation\n"));

		if(OutputBufferLength < sizeof(LSMPIOCTL_ADAPTERINFO)) {
			KDPrint(1,("Too small output buffer. OutputBufferLength:%d\n", OutputBufferLength));
			*NtStatus = STATUS_BUFFER_TOO_SMALL;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		adapter->Length								= sizeof(LSMPIOCTL_ADAPTERINFO);
		adapter->Adapter.SlotNo						= HwDeviceExtension->SlotNumber;
		adapter->Adapter.Length						= sizeof(LSMP_ADAPTER);
		adapter->Adapter.InitiatorId				= HwDeviceExtension->InitiatorId;
		adapter->Adapter.NumberOfBuses				= HwDeviceExtension->NumberOfBuses;
		adapter->Adapter.MaximumNumberOfTargets		= HwDeviceExtension->MaximumNumberOfTargets;
		adapter->Adapter.MaximumNumberOfLogicalUnits= HwDeviceExtension->MaximumNumberOfLogicalUnits;
		adapter->Adapter.MaxBlocksPerRequest		= HwDeviceExtension->MaxBlocksPerRequest;

		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		adapter->Adapter.Status						= HwDeviceExtension->AdapterStatus;
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

		*NtStatus = STATUS_SUCCESS;
		status = SRB_STATUS_SUCCESS;
		break;
		}
	case LsmpPrimaryUnitDiskInformation: {
		PLSMPIOCTL_PRIMUNITDISKINFO	primUnitDisk = (PLSMPIOCTL_PRIMUNITDISKINFO)OutputBuffer;
		PLUR_QUERY					LurQuery;
		PLURN_PRIMARYINFORMATION	LurPrimaryInfo;
		BYTE						LurBuffer[SIZE_OF_LURQUERY(0, sizeof(LURN_PRIMARYINFORMATION))];
		ACCESS_MASK					DesiredAccess;

		KDPrint(1,("LsmpPrimaryUnitDiskInformation\n"));

		if(OutputBufferLength < sizeof(LSMPIOCTL_PRIMUNITDISKINFO)) {
			KDPrint(1,("Too small output buffer\n"));
			*NtStatus = STATUS_BUFFER_TOO_SMALL;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		//
		//	Query to the LUR
		//
		ntStatus = LSCcbAllocate(&Ccb);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("LSCcbAllocate() failed.\n"));
			*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		LSCCB_INITIALIZE(Ccb, CurSrbSequence);
		Ccb->OperationCode = CCB_OPCODE_QUERY;
		LSCcbSetFlag(Ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED);
		Ccb->DataBuffer = LurBuffer;
		Ccb->DataBufferLength = sizeof(LURN_PRIMARYINFORMATION);

		LurQuery = (PLUR_QUERY)LurBuffer;
		LurQuery->InfoClass			= LurPrimaryLurnInformation;
		LurQuery->Length			= SIZE_OF_LURQUERY(0, sizeof(LURN_PRIMARYINFORMATION));
		LurQuery->QueryDataLength	= 0;

		LurPrimaryInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION(LurQuery);

		if(LuExtension) {
			KDPrint(3,("going to LuExtention %p.\n", LuExtension));
			ntStatus = LurRequest(
								LuExtension->LUR,
								Ccb
							);
			DesiredAccess = LuExtension->LUR->DesiredAccess;
		} else {
			KDPrint(3,("going to default LuExtention 0.\n"));
			ntStatus = LurRequest(
								HwDeviceExtension->LURs[0],	// default: 0.
								Ccb
							);
			DesiredAccess = HwDeviceExtension->LURs[0]->DesiredAccess;
		}
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("LurRequest() failed.\n"));
			*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}
		
		//
		//	Set return values.
		//
		primUnitDisk->Length					= sizeof(LSMPIOCTL_PRIMUNITDISKINFO);
		primUnitDisk->UnitDisk.Length			= sizeof(LSMP_UNITDISK);
		
		// added by ktkim at 03/06/2004
		primUnitDisk->EnabledTime    = HwDeviceExtension->EnabledTime;

		primUnitDisk->UnitDisk.UnitDiskId		= LurPrimaryInfo->PrimaryLurn.UnitDiskId;
		primUnitDisk->UnitDisk.DesiredAccess	= DesiredAccess;
		primUnitDisk->UnitDisk.GrantedAccess	= LurPrimaryInfo->PrimaryLurn.AccessRight;
		RtlCopyMemory(
			primUnitDisk->UnitDisk.UserID,
			&LurPrimaryInfo->PrimaryLurn.UserID,
			sizeof(primUnitDisk->UnitDisk.UserID)
			);
		RtlCopyMemory(
			primUnitDisk->UnitDisk.Password,
			&LurPrimaryInfo->PrimaryLurn.Password,
			sizeof(primUnitDisk->UnitDisk.Password)
			);

		RtlCopyMemory(	&primUnitDisk->UnitDisk.NetDiskAddress,
						&LurPrimaryInfo->PrimaryLurn.NetDiskAddress,
						sizeof(TA_LSTRANS_ADDRESS)
			);
		RtlCopyMemory(	&primUnitDisk->UnitDisk.BindingAddress,
						&LurPrimaryInfo->PrimaryLurn.BindingAddress,
						sizeof(TA_LSTRANS_ADDRESS)
			);
		primUnitDisk->UnitDisk.UnitBlocks		= (UINT32)LurPrimaryInfo->PrimaryLurn.UnitBlocks;
		primUnitDisk->UnitDisk.SlotNo			= HwDeviceExtension->SlotNumber;
		break;
		}

	case LsmpAdapterLurInformation: {
		PLSMPIOCTL_ADAPTERLURINFO	info = (PLSMPIOCTL_ADAPTERLURINFO)OutputBuffer;
		PLSMP_LURN_FULL				unitDisk;
		PLUR_QUERY					LurQuery;
		PLURN_ENUM_INFORMATION		LurnEnumInfo;
		UINT32						LurnEnumInfoLen;
		PLURN_INFORMATION			lurnInformation;
		ACCESS_MASK					DesiredAccess;
		ACCESS_MASK					GrantedAccess;
		ULONG						idx_lurn;
		UINT32						returnLength;

		KDPrint(1,("LsmpAdapterLurInformation\n"));

		if(OutputBufferLength < FIELD_OFFSET(LSMPIOCTL_ADAPTERLURINFO, UnitDisks)) {
			KDPrint(1,("PDOSLOTLIST: Buffer size is less than required %d bytes\n", FIELD_OFFSET(LSMPIOCTL_ADAPTERLURINFO, UnitDisks)));
			*NtStatus = STATUS_INVALID_PARAMETER;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		returnLength = FIELD_OFFSET(LSMPIOCTL_ADAPTERLURINFO, UnitDisks);

		//
		//	Adapter information
		//
		info->EnabledTime.QuadPart					=	HwDeviceExtension->EnabledTime.QuadPart;

		info->Adapter.SlotNo						= HwDeviceExtension->SlotNumber;
		info->Adapter.Length						= sizeof(LSMP_ADAPTER);
		info->Adapter.InitiatorId					= HwDeviceExtension->InitiatorId;
		info->Adapter.NumberOfBuses					= HwDeviceExtension->NumberOfBuses;
		info->Adapter.MaximumNumberOfTargets		= HwDeviceExtension->MaximumNumberOfTargets;
		info->Adapter.MaximumNumberOfLogicalUnits	= HwDeviceExtension->MaximumNumberOfLogicalUnits;
		info->Adapter.MaxBlocksPerRequest			= HwDeviceExtension->MaxBlocksPerRequest;

		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		info->Adapter.Status						= HwDeviceExtension->AdapterStatus;
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		//
		// LUR information ( Scsi LU information )
		//
		if(!HwDeviceExtension->LURs[0]) {
			KDPrint(1,("No LUR exists.\n"));
			*NtStatus = STATUS_INVALID_PARAMETER;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}
		info->Lur.Length		= sizeof(LSMP_LUR);
		info->Lur.DevType		= HwDeviceExtension->LURs[0]->DevType;
		info->Lur.TargetId		= HwDeviceExtension->LURs[0]->LurId[1];
		info->Lur.Lun			= HwDeviceExtension->LURs[0]->LurId[2];
		info->Lur.LurnCnt		= HwDeviceExtension->LURs[0]->NodeCount;
		info->UnitDiskCnt		= info->Lur.LurnCnt;

		ASSERT(HwDeviceExtension->LURs[0]->NodeCount >= 1);
		info->Length			=	sizeof(LSMPIOCTL_ADAPTERLURINFO) +
									sizeof(LSMP_LURN_FULL) *
									(HwDeviceExtension->LURs[0]->NodeCount - 1);

		//
		//	Lurn information.
		//	Query to the LUR
		//
		unitDisk = info->UnitDisks;

		//
		//	allocate a CCB
		//
		ntStatus = LSCcbAllocate(&Ccb);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("LSCcbAllocate() failed.\n"));
			*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		//
		//	initialize query CCB
		//
		LSCCB_INITIALIZE(Ccb, CurSrbSequence);
		Ccb->OperationCode = CCB_OPCODE_QUERY;
		LSCcbSetFlag(Ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED);

		ASSERT(info->Lur.LurnCnt >= 1);

		LurnEnumInfoLen = sizeof(LURN_ENUM_INFORMATION) + sizeof(LURN_INFORMATION) * (info->Lur.LurnCnt-1);
		LUR_QUERY_INITIALIZE(LurQuery, LurEnumerateLurn, 0, LurnEnumInfoLen);
		if(!LurQuery) {
			KDPrint(1,("allocating DataBuffer failed.\n"));
			*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		Ccb->DataBuffer = LurQuery;
		Ccb->DataBufferLength = LurQuery->Length;

		LurnEnumInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION(LurQuery);

		//
		//	send the CCB down
		//
		if(LuExtension) {
			KDPrint(3,("going to LuExtention %p.\n", LuExtension));
			ntStatus = LurRequest(
								LuExtension->LUR,
								Ccb
							);
			DesiredAccess = LuExtension->LUR->DesiredAccess;
			GrantedAccess = LuExtension->LUR->GrantedAccess;
		} else {
			KDPrint(3,("going to default LuExtention 0.\n"));
			ntStatus = LurRequest(
								HwDeviceExtension->LURs[0],	// default: 0.
								Ccb
							);
			DesiredAccess = HwDeviceExtension->LURs[0]->DesiredAccess;
			GrantedAccess = HwDeviceExtension->LURs[0]->GrantedAccess;
		}
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("LurRequest() failed.\n"));
			ExFreePoolWithTag(LurQuery, LSMP_PTAG_IOCTL);
			*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		info->Lur.DesiredAccess = DesiredAccess;
		info->Lur.GrantedAccess = GrantedAccess;

		//
		//	Set return values for each LURN.
		//
		for(idx_lurn = 0; idx_lurn < info->UnitDiskCnt; idx_lurn++) {

			//
			//	Add one Unitdisk to return bytes and check the user buffer size.
			//
			returnLength += sizeof(LSMP_LURN_FULL);
			if(returnLength > OutputBufferLength) {
				continue;
			}

			unitDisk = info->UnitDisks + idx_lurn;
			lurnInformation = LurnEnumInfo->Lurns + idx_lurn;

			unitDisk->Length = sizeof(LSMP_LURN_FULL);
			unitDisk->UnitDiskId = lurnInformation->UnitDiskId;
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

			RtlCopyMemory(	&unitDisk->NetDiskAddress,
						&lurnInformation->NetDiskAddress,
						sizeof(TA_LSTRANS_ADDRESS)
					);
			RtlCopyMemory(	&unitDisk->BindingAddress,
						&lurnInformation->BindingAddress,
						sizeof(TA_LSTRANS_ADDRESS)
					);
		}

		if(returnLength > OutputBufferLength) {
			KDPrint(1,("Output buffer too small. outbuffer:%u required:%u\n", OutputBufferLength, returnLength));
			ExFreePoolWithTag(LurQuery, LSMP_PTAG_IOCTL);
			*NtStatus = STATUS_BUFFER_TOO_SMALL;
			status = SRB_STATUS_SUCCESS;
			break;
		}

		ExFreePoolWithTag(LurQuery, LSMP_PTAG_IOCTL);
		*NtStatus = STATUS_SUCCESS;
		status = SRB_STATUS_SUCCESS;
		break;
		}
	//
	//	User application can use this information instead of LANSCSIMINIPORT_IOCTL_GET_VERSION.
	// 
	case 	LsmpDriverVersion:				// 5
		{
			PLSMPIOCTL_DRVVER	version = (PLSMPIOCTL_DRVVER)OutputBuffer;

			version->VersionMajor = VER_FILEMAJORVERSION;
			version->VersionMinor = VER_FILEMINORVERSION;
			version->VersionBuild = VER_FILEBUILD;
			version->VersionPrivate = VER_FILEBUILD_QFE;

			*NtStatus = STATUS_SUCCESS;
			status = SRB_STATUS_SUCCESS;
		break;
		}

	default:
		KDPrint(1,("Invalid Information Class!!\n"));
		*NtStatus = STATUS_INVALID_PARAMETER;
		status = SRB_STATUS_INVALID_REQUEST;
	}

	return status;
}

NTSTATUS
AddNewDeviceToMiniport_Worker(
		IN PDEVICE_OBJECT		DeviceObject,
		IN PLSMP_WORKITEM_CTX	WorkitemCtx
	) {
	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension;
	PMINIPORT_LU_EXTENSION		LuExtension;
	PCCB						Ccb;
	PLURELATION_DESC			LurDesc;
	PLURELATION					Lur;
	NTSTATUS					status;
	LONG						LURCount;
	UINT32						DefaultLurFlags;

	ASSERT(WorkitemCtx);
	UNREFERENCED_PARAMETER(DeviceObject);

	LuExtension =(PMINIPORT_LU_EXTENSION) WorkitemCtx->Arg1;
	LurDesc = (PLURELATION_DESC)WorkitemCtx->Arg2;
	HwDeviceExtension = (PMINIPORT_DEVICE_EXTENSION)WorkitemCtx->Arg3;
	Ccb = WorkitemCtx->Ccb;

	//
	//	Get default LUR flags.
	//
	GetDefaultLurFlags(LurDesc, &DefaultLurFlags);

	status = LurCreate(LurDesc, DefaultLurFlags, &Lur, HwDeviceExtension->ScsiportFdoObject, LsmpLurnCallback);
	if(NT_SUCCESS(status)) {
		LURCount = InterlockedIncrement(&HwDeviceExtension->LURCount);
		//
		//	We support only one LUR for now.
		//
		ASSERT(LURCount == 1);

		HwDeviceExtension->LURs[0] = Lur;
		LuExtension->LUR = Lur;
	}

	//
	//	Free LUR Descriptor
	//
	ExFreePoolWithTag(LurDesc, LSMP_PTAG_IOCTL);

	//
	//	Notify a bus change.
	//
	LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_BUSRESET_REQUIRED);

	LSCcbCompleteCcb(Ccb);

	return status;
}


NTSTATUS
AddNewDeviceToMiniport(
		IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		IN PMINIPORT_LU_EXTENSION		LuExtension,
		IN PSCSI_REQUEST_BLOCK			Srb,
		IN ULONG						CurSrbSequence,
		IN PLURELATION_DESC				LurDesc
	) {
	LSMP_WORKITEM_CTX			WorkitemCtx;
	NTSTATUS					status;
	PCCB						Ccb;

	status = LSCcbAllocate(&Ccb);
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("failed.\n"));
		return status;
	}
	LSCcbInitialize(
					Srb,
					HwDeviceExtension,
					CurSrbSequence,
					Ccb
				);
	InterlockedIncrement(&HwDeviceExtension->RequestExecuting);
	LSCcbSetCompletionRoutine(Ccb, LanscsiMiniportCompletion, HwDeviceExtension);
	LSCcbSetNextStackLocation(Ccb);


	//
	//	Queue a workitem
	//
	LSMP_INIT_WORKITEMCTX(&WorkitemCtx, AddNewDeviceToMiniport_Worker, Ccb, LuExtension, LurDesc, HwDeviceExtension);
	status = LSMP_QueueMiniportWorker(HwDeviceExtension->ScsiportFdoObject,&WorkitemCtx);
	if(NT_SUCCESS(status)) {
		status = STATUS_PENDING;
	}

	return status;
}

NTSTATUS
RemoveDeviceFromMiniport_Worker(
		IN PDEVICE_OBJECT		DeviceObject,
		IN PLSMP_WORKITEM_CTX	WorkitemCtx
	) {
	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension;
	PMINIPORT_LU_EXTENSION		LuExtension;
	LONG						LURCount;
	KIRQL						oldIrql;
	PLURELATION					Lur;
	PCCB						Ccb;
	UINT32						AdapterStatus;
	NTSTATUS					status;

	ASSERT(WorkitemCtx);
	UNREFERENCED_PARAMETER(DeviceObject);

	KDPrint(1,("Entered.\n"));

	LuExtension			= (PMINIPORT_LU_EXTENSION)WorkitemCtx->Arg1;
	HwDeviceExtension	= (PMINIPORT_DEVICE_EXTENSION)WorkitemCtx->Arg3;
	Ccb					= WorkitemCtx->Ccb;

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(ADAPTER_ISSTATUS(HwDeviceExtension,ADAPTER_STATUS_STOPPING)) {
		KDPrint(1,("Error! stopping in progress.\n"));
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		NDScsiLogError(
				HwDeviceExtension,
				NULL,
				0,
				0,
				0,
				NDASSCSI_IO_STOPIOCTL_WHILESTOPPING,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_IOCTL, EVTLOG_FAIL_WHILESTOPPING, 0)
			);
		return STATUS_SUCCESS;
	}
	AdapterStatus = ADAPTER_SETSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING);
	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);

	if(LuExtension) {
		Lur = LuExtension->LUR;
		LuExtension->LUR = NULL;
	} else {
		//
		//	We support only one LUR for now.
		//
		Lur = HwDeviceExtension->LURs[0];
		HwDeviceExtension->LURs[0] = NULL;
	}

	if(!Lur) {
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_STOPIOCTL_NO_LUR,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_IOCTL, EVTLOG_FAIL_LURNULL, 0)
			);
		KDPrint(1,("Error! LUR is NULL!\n"));
		return STATUS_SUCCESS;
	}

	status = SendCcbToLURSync(HwDeviceExtension, Lur, CCB_OPCODE_STOP);
	if(NT_SUCCESS(status)){
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_STOPIOCTL_LUR_ERROR,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_IOCTL, EVTLOG_FAIL_SENDSTOPCCB, 0xffff & status)
			);
		KDPrint(1,("Failed to send stop ccb.\n"));
	}
	LurClose(Lur);
	LURCount = InterlockedDecrement(&HwDeviceExtension->LURCount);

	//
	//	stop CCB
	//
	ASSERT(LURCount == 0);

	//
	//	Notify a bus change.
	//
	LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_BUSRESET_REQUIRED);

	LSCcbCompleteCcb(Ccb);

	return STATUS_SUCCESS;
}


NTSTATUS
RemoveDeviceFromMiniport(
		IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		IN PMINIPORT_LU_EXTENSION		LuExtension,
		IN PSCSI_REQUEST_BLOCK			Srb,
		IN ULONG						CurSrbSequence
	) {
	LSMP_WORKITEM_CTX			WorkitemCtx;
	NTSTATUS					status;
	PCCB						Ccb;

	//
	//	initilize Ccb in srb.
	//
	KDPrint(1, ("Entered.\n"));
	status = LSCcbAllocate(&Ccb);
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("failed.\n"));
		return status;
	}
	LSCcbInitialize(
					Srb,
					HwDeviceExtension,
					CurSrbSequence,
					Ccb
				);
	InterlockedIncrement(&HwDeviceExtension->RequestExecuting);
	LSCcbSetCompletionRoutine(Ccb, LanscsiMiniportCompletion, HwDeviceExtension);
	LSCcbSetNextStackLocation(Ccb);

	//
	//	Queue a workitem
	//
	LSMP_INIT_WORKITEMCTX(&WorkitemCtx, RemoveDeviceFromMiniport_Worker, Ccb, LuExtension, NULL, HwDeviceExtension);
	status = LSMP_QueueMiniportWorker(HwDeviceExtension->ScsiportFdoObject, &WorkitemCtx);
	if(NT_SUCCESS(status)) {
		status = STATUS_PENDING;
	}

	return status;
}


// Added by ILGU HONG 2004_07_05
UCHAR
SrbIoctlGetDVDSTatus(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		PMINIPORT_LU_EXTENSION		LuExtension,
		ULONG						OutputBufferLength,
		PUCHAR						OutputBuffer,
		PNTSTATUS					NtStatus,
		ULONG						CurSrbSequence
) {
	UCHAR				status;
	PCCB				Ccb;
	NTSTATUS			ntStatus;


	PBUSENUM_DVD_STATUS DvdStatusInfo = (PBUSENUM_DVD_STATUS)OutputBuffer; 
	BYTE				LurBuffer[sizeof(LURN_DVD_STATUS)];
	PLURN_DVD_STATUS	pDvdStatusHeader;



	KDPrint(1,("\n"));

	if(OutputBufferLength < sizeof(BUSENUM_DVD_STATUS)) {
		KDPrint(1,("Too small output buffer\n"));
		*NtStatus = STATUS_BUFFER_TOO_SMALL;
		status = SRB_STATUS_INVALID_REQUEST;
		return status;
	}

	//
	//	Query to the LUR
	//
	ntStatus = LSCcbAllocate(&Ccb);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("LSCcbAllocate() failed.\n"));
		*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
		status = SRB_STATUS_INVALID_REQUEST;
		return status;
	}

	LSCCB_INITIALIZE(Ccb, CurSrbSequence);
	Ccb->OperationCode = CCB_OPCODE_DVD_STATUS;
	LSCcbSetFlag(Ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED);
	Ccb->DataBuffer = LurBuffer;
	Ccb->DataBufferLength = sizeof(LURN_DVD_STATUS);
	
	pDvdStatusHeader = (PLURN_DVD_STATUS)LurBuffer;
	pDvdStatusHeader->Length = sizeof(LURN_DVD_STATUS);

	if(LuExtension) {
		KDPrint(3,("going to LuExtention %p.\n", LuExtension));
		ntStatus = LurRequest(
							LuExtension->LUR,
							Ccb
						);
	} else {
		KDPrint(3,("going to default LuExtention 0.\n"));
		ntStatus = LurRequest(
							HwDeviceExtension->LURs[0],	// default: 0.
							Ccb
						);

	}
	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("LurnRequest() failed.\n"));
		*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
		status = SRB_STATUS_INVALID_REQUEST;
		return status;
	}
		
	//
	//	Set return values.
	//
	KDPrint(1,("Last Access Time :%I64d\n",pDvdStatusHeader->Last_Access_Time.QuadPart));
	KDPrint(1,("Result  %d\n",pDvdStatusHeader->Status));
	DvdStatusInfo->Status = pDvdStatusHeader->Status;
	*NtStatus = STATUS_SUCCESS;
	return SRB_STATUS_SUCCESS;
}
// Added by ILGU HONG 2004_07_05 end

NTSTATUS
MiniSrbControl(
	   IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	   IN PMINIPORT_LU_EXTENSION		LuExtension,
	   IN PSCSI_REQUEST_BLOCK			Srb,
	   IN ULONG							CurSrbSequence
	)
/*++

Routine Description:

    This is the SRB_IO_CONTROL handler for this driver. These requests come from
    the management driver.
    
Arguments:

    DeviceExtension - Context
    Srb - The request to process.
    
Return Value:

    Value from the helper routines, or if handled in-line, SUCCESS or INVALID_REQUEST. 
    
--*/
{
    PSRB_IO_CONTROL	srbIoControl;
    PUCHAR			srbIoctlBuffer;
	LONG			srbIoctlBufferLength;
    ULONG			controlCode;
    ULONG			transferPages = 0;
    NTSTATUS		status;
    UCHAR			srbStatus;

    //
    // Start off being paranoid.
    //
    if (Srb->DataBuffer == NULL) {
		KDPrint(1,("DataBuffer is NULL\n"));
        return SRB_STATUS_INVALID_REQUEST;
    }
	status = STATUS_MORE_PROCESSING_REQUIRED;

	//
    // Extract the io_control
    //
    srbIoControl = (PSRB_IO_CONTROL)Srb->DataBuffer;

    //
    // Ensure the signature is correct.
    //
    if (strncmp(srbIoControl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8) != 0) {

		KDPrint(1,("Signature mismatch %8s, %8s\n", srbIoControl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE));
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Get the control code.
    // 
    controlCode = srbIoControl->ControlCode;

    //
    // Get the Ioctl buffer. If this is a send message request, it gets
    // fixed up to be an I2O message later.
    //
    srbIoctlBuffer = ((PUCHAR)Srb->DataBuffer) + sizeof(SRB_IO_CONTROL);
	srbIoctlBufferLength = srbIoControl->Length;

    //
    // Based on the control code, figure out what to do.
    //
    switch (controlCode) {
		case LANSCSIMINIPORT_IOCTL_GET_DVD_STATUS:
			{
				srbStatus = SrbIoctlGetDVDSTatus(
								HwDeviceExtension,
								LuExtension,
								srbIoctlBufferLength,
								srbIoctlBuffer,
								&srbIoControl->ReturnCode,
								CurSrbSequence
								);
				if(srbStatus == SRB_STATUS_SUCCESS) {
					KDPrint(4,("LANSCSIMINIPORT_IOCTL_QUERYINFO_EX: Successful.\n"));
					status = STATUS_SUCCESS;
				} else {
					status = STATUS_UNSUCCESSFUL;
				}								
			}
			break;
		case LANSCSIMINIPORT_IOCTL_GET_SLOT_NO:
            KDPrint(2,("Get Slot No. Slot number is %d\n", HwDeviceExtension->SlotNumber));

			*(PULONG)srbIoctlBuffer = HwDeviceExtension->SlotNumber;
			srbIoControl->ReturnCode = 0;

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;
            break;

		case LANSCSIMINIPORT_IOCTL_QUERYINFO_EX: {
			PLSMPIOCTL_QUERYINFO	QueryInfo;
			PUCHAR tmpBuffer;
            KDPrint(5, ("Query information EX.\n"));

			QueryInfo = (PLSMPIOCTL_QUERYINFO)srbIoctlBuffer;

			tmpBuffer = ExAllocatePoolWithTag(NonPagedPool, srbIoctlBufferLength, LSMP_PTAG_IOCTL);
			if(tmpBuffer == NULL) {
				ASSERT(FALSE);
	            KDPrint(1,("LANSCSIMINIPORT_IOCTL_QUERYINFO_EX: SRB_STATUS_DATA_OVERRUN. BufferLength:%d\n", srbIoctlBufferLength));
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				break;
			}

			srbStatus = SrbIoctlQueryInfo(
							HwDeviceExtension,
							LuExtension,
							QueryInfo,
							srbIoctlBufferLength,
							tmpBuffer,
							&srbIoControl->ReturnCode,
							CurSrbSequence
						);
			if(srbStatus == SRB_STATUS_SUCCESS) {
	            KDPrint(4,("LANSCSIMINIPORT_IOCTL_QUERYINFO_EX: Successful.\n"));
				RtlCopyMemory(srbIoctlBuffer, tmpBuffer, srbIoctlBufferLength);
				status = STATUS_SUCCESS;
			} else {
				status = STATUS_UNSUCCESSFUL;
			}

			ExFreePoolWithTag(tmpBuffer, LSMP_PTAG_IOCTL);
			break;
		}
		case LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE: {
			PLURN_UPDATE		LurnUpdate;
			PCCB				Ccb;

			//
			//	Set a CCB
			//
			status = LSCcbAllocate(&Ccb);
			if(!NT_SUCCESS(status)) {
				KDPrint(1,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE: LSCcbAllocate() failed.\n"));
				break;
			}
			LurnUpdate = (PLURN_UPDATE)ExAllocatePoolWithTag(NonPagedPool, sizeof(LURN_UPDATE), LSMP_PTAG_IOCTL);
			if(!LurnUpdate) {
				KDPrint(1,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE: ExAllocatePoolWithTag() failed.\n"));
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			LSCCB_INITIALIZE(Ccb, CurSrbSequence);
			Ccb->OperationCode = CCB_OPCODE_UPDATE;
			Ccb->DataBuffer = LurnUpdate;
			Ccb->DataBufferLength = sizeof(LURN_UPDATE);
			LSCcbSetFlag(Ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);

			//	Ioctl Srb will complete asynchronously.
			Ccb->Srb = Srb;
			InterlockedIncrement(&HwDeviceExtension->RequestExecuting);
			LSCcbSetCompletionRoutine(Ccb, LanscsiMiniportCompletion, HwDeviceExtension);

			LurnUpdate->UpdateClass = LURN_UPDATECLASS_WRITEACCESS_USERID;

			if(LuExtension) {
				KDPrint(3,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE:  going to LuExtention %p.\n", LuExtension));
				status = LurRequest(
									LuExtension->LUR,
									Ccb
								);
			} else {
				KDPrint(3,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE:  going to default LuExtention 0.\n"));
				status = LurRequest(
									HwDeviceExtension->LURs[0],	// default: 0.
									Ccb
								);
			}
			if(!NT_SUCCESS(status)) {
				KDPrint(1,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE:  LurnRequest() failed.\n"));

				NDScsiLogError(
						HwDeviceExtension,
						Srb,
						Srb->PathId,
						Srb->TargetId,
						Srb->Lun,
						NDASSCSI_IO_UPGRADEIOCTL_FAIL,
						EVTLOG_UNIQUEID(EVTLOG_MODULE_IOCTL, EVTLOG_FAIL_UPGRADEIOCTL, 0)
					);
				LSCcbFree(Ccb);
				ExFreePoolWithTag(LurnUpdate, LSMP_PTAG_IOCTL);
				status = STATUS_SUCCESS;
			} else {
				status = STATUS_PENDING;
			}
			break;
		}

		case LANSCSIMINIPORT_IOCTL_ADD_TARGET: {
			PLANSCSI_ADD_TARGET_DATA	AddTargetData;
			PLURELATION_DESC			LurDesc;
			LONG						LurDescLength;
			LONG						LurnCnt;

			AddTargetData	= (PLANSCSI_ADD_TARGET_DATA)srbIoctlBuffer;
			LurnCnt			=	AddTargetData->ulNumberOfUnitDiskList;
			LurDescLength	=	sizeof(LURELATION_DESC) +											// LURELATION_DESC
								(sizeof(LURELATION_NODE_DESC) + sizeof(LONG) * (LurnCnt-1)) *		// LURELATION_NODE_DESC
								(LurnCnt - 1);
			//
			//	Allocate LUR Descriptor
			//	It will be freed in the worker routine.
			//
			LurDesc = (PLURELATION_DESC)ExAllocatePoolWithTag(NonPagedPool,  LurDescLength, LSMP_PTAG_IOCTL);
			if(LurDesc == NULL) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			status = LurTranslateAddTargetDataToLURDesc(
									AddTargetData,
									HwDeviceExtension->MaxBlocksPerRequest,
									LurDescLength,
									LurDesc
								);
			if(NT_SUCCESS(status)) {
				ASSERT(LurDescLength>0);
				status = AddNewDeviceToMiniport(HwDeviceExtension, LuExtension, Srb, CurSrbSequence, LurDesc);
			}
		}
		case LANSCSIMINIPORT_IOCTL_ADD_DEVICE: {
			PLURELATION_DESC			LurDesc;

			LurDesc = (PLURELATION_DESC)srbIoctlBuffer;

			status = AddNewDeviceToMiniport(HwDeviceExtension, LuExtension, Srb, CurSrbSequence, LurDesc);
			break;
		}
		case LANSCSIMINIPORT_IOCTL_REMOVE_DEVICE: {
			status = RemoveDeviceFro
        // Initialize value to zero.  It will be incremented once pnp is aware
        // of its existance.
        //

        commonExtension->RemoveLock = 0;
#if DBG
        KeInitializeSpinLock(&commonExtension->RemoveTrackingSpinlock);
        commonExtension->RemoveTrackingList = NULL;

        ExInitializeNPagedLookasideList(
            &(commonExtension->RemoveTrackingLookasideList),
            NULL,
            NULL,
            0,
            sizeof(REMOVE_TRACKING_BLOCK),
            SCSIPORT_TAG_LOCK_TRACKING,
            64);

        commonExtension->RemoveTrackingLookasideListInitialized = TRUE;
#else
        commonExtension->RemoveTrackingSpinlock = (ULONG) -1L;
        commonExtension->RemoveTrackingList = (PVOID) -1L;
#endif

        commonExtension->CurrentPnpState = 0xff;
        commonExtension->PreviousPnpState = 0xff;

        //
        // Initialize the remove lock event.
        //

        KeInitializeEvent(
            &(logicalUnitExtension->CommonExtension.RemoveEvent),
            SynchronizationEvent,
            FALSE);

        logicalUnitExtension->LuFlags |= LU_RESCAN_ACTIVE;

        logicalUnitExtension->PortNumber = deviceExtension->PortNumber;

        logicalUnitExtension->PathId = PathId;
        logicalUnitExtension->TargetId = TargetId;
        logicalUnitExtension->Lun = Lun;

        logicalUnitExtension->HwLogicalUnitExtension =
            (PVOID) (logicalUnitExtension + 1);

        logicalUnitExtension->AdapterExtension = deviceExtension;

        //
        // Give the caller the benefit of the doubt.
        //

        logicalUnitExtension->IsMissing = FALSE;

        //
        // The device cannot have been enumerated yet.
        //

        logicalUnitExtension->IsEnumerated = FALSE;

        //
        // Set timer counters to -1 to inidicate that there are no outstanding
        // requests.
        //

        logicalUnitExtension->RequestTimeoutCounter = -1;

        //
        // Initialize the maximum queue depth size.
        //

        logicalUnitExtension->MaxQueueDepth = 0xFF;

        //
        // Initialize the request list.
        //

        InitializeListHead(&logicalUnitExtension->RequestList);

        //
        // Initialize the push/pop list of SRB_DATA blocks for use with bypass
        // requests.
        //

        KeInitializeSpinLock(&(logicalUnitExtension->BypassSrbDataSpinLock));
        ExInitializeSListHead(&(logicalUnitExtension->BypassSrbDataList));
        for(i = 0; i < NUMBER_BYPASS_SRB_DATA_BLOCKS; i++) {
            ExInterlockedPushEntrySList(
                &(logicalUnitExtension->BypassSrbDataList),
                &(logicalUnitExtension->BypassSrbDataBlocks[i].Reserved),
                &(logicalUnitExtension->BypassSrbDataSpinLock));
        }

        //
        // Assume devices are powered on by default.
        //

        commonExtension->CurrentDeviceState = PowerDeviceD0;
        commonExtension->DesiredDeviceState = PowerDeviceUnspecified;

        //
        // Assume that we're being initialized in a working system.
        //

        commonExtension->CurrentSystemState = PowerSystemWorking;

        //
        // Set the pnp state to unknown.
        //

        commonExtension->CurrentPnpState = 0xff;
        commonExtension->PreviousPnpState = 0xff;

        //
        // Setup the request sense resources.
        //

        logicalUnitExtension->RequestSenseIrp = senseIrp;
        KeInitializeSpinLock(&(logicalUnitExtension->RequestSenseLock));

        //
        // Link the device into the list of physical device objects
        //

        SpAddLogicalUnitToBin(deviceExtension, logicalUnitExtension);

        //
        // I guess this is as ready to be opened as it ever will be.
        //

        pdo->Flags &= ~DO_DEVICE_INITIALIZING;

    } else {

        DebugPrint((1, "ScsiBusCreatePdo: Error %#08lx creating device object\n",
                       status));
        pdo = NULL;
    }

    *NewPdo = pdo;

    return status;
}

NTSTATUS
ScsiPortStartDevice(
    IN PDEVICE_OBJECT LogicalUnit
    )

/*++

Routine Description:

    This routine will attempt to start the specified device object.

    Currently this involves clearing the INITIALIZING flag if it was set,
    and running through to the device node and marking itself as started.  This
    last is a kludge

Arguments:

    LogicalUnit - a pointer to the PDO being started

Return Value:

    status

--*/

{
    PLOGICAL_UNIT_EXTENSION logicalUnitExtension = LogicalUnit->DeviceExtension;
    PADAPTER_EXTENSION adapterExtension =
        logicalUnitExtension->AdapterExtension;

    HANDLE instanceHandle;

    NTSTATUS status;

    PAGED_CODE();

    //
    // Open the devnode for this PDO and see if anyone's given us some
    // default SRB flags.
    //

    status = IoOpenDeviceRegistryKey(LogicalUnit,
                                     PLUGPLAY_REGKEY_DEVICE,
                                     KEY_READ,
                                     &instanceHandle);

    if(NT_SUCCESS(status)) {

        RTL_QUERY_REGISTRY_TABLE queryTable[2];
        ULONG zero = 0;

        RtlZeroMemory(queryTable, sizeof(queryTable));

        queryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        queryTable[0].Name = L"DefaultRequestFlags";
        queryTable[0].EntryContext =
            &logicalUnitExtension->CommonExtension.SrbFlags;
        queryTable[0].DefaultType = REG_DWORD;
        queryTable[0].DefaultData = &zero;
        queryTable[0].DefaultLength = sizeof(ULONG);

        status = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE | RTL_REGISTRY_OPTIONAL,
                                        (PWSTR) instanceHandle,
                                        queryTable,
                                        NULL,
                                        NULL);

        //
        // BUGBUG - need a way to turn off tagged queuing and caching.  Ie.
        // keep track of negative flags as well.
        //

        logicalUnitExtension->CommonExtension.SrbFlags &=
            ( SRB_FLAGS_DISABLE_DISCONNECT |
              SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
              SRB_FLAGS_DISABLE_SYNCH_TRANSFER);

        DebugPrint((1, "SpStartDevice: Default SRB flags for (%d,%d,%d) are "
                       "%#08lx\n",
                    logicalUnitExtension->PathId,
                    logicalUnitExtension->TargetId,
                    logicalUnitExtension->Lun,
                    logicalUnitExtension->CommonExtension.SrbFlags));

        ZwClose(instanceHandle);

    } else {

        DebugPrint((1, "SpStartDevice: Error opening instance key for pdo "
                       "[%#08lx]\n",
                    status));
    }

    //
    // If the queue 