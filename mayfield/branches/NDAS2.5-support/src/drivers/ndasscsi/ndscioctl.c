#include "ndasscsi.h"
#include "ndasscsi.ver"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NDSCIOCTL"

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

	UNREFERENCED_PARAMETER(LuExtension);

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
		adapter->Adapter.MaxBlocksPerRequest		= HwDeviceExtension->AdapterMaxBlocksPerRequest;

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

		KDPrint(3,("going to default LuExtention 0.\n"));
		if(HwDeviceExtension->LURs[0] == NULL || HwDeviceExtension->LURCount == 0) {
			LSCcbFree(Ccb);

			*NtStatus = STATUS_INVALID_PARAMETER;
			status = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		ntStatus = LurRequest(
							HwDeviceExtension->LURs[0],	// default: 0.
							Ccb
						);
		DesiredAccess = HwDeviceExtension->LURs[0]->DesiredAccess;
		if(!NT_SUCCESS(ntStatus)) {
			LSCcbFree(Ccb);

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
		//
		//	Adapter information
		//
		primUnitDisk->EnabledTime.QuadPart					=	HwDeviceExtension->EnabledTime.QuadPart;

		primUnitDisk->Adapter.SlotNo						= HwDeviceExtension->SlotNumber;
		primUnitDisk->Adapter.Length						= sizeof(LSMP_ADAPTER);
		primUnitDisk->Adapter.InitiatorId					= HwDeviceExtension->InitiatorId;
		primUnitDisk->Adapter.NumberOfBuses					= HwDeviceExtension->NumberOfBuses;
		primUnitDisk->Adapter.MaximumNumberOfTargets		= HwDeviceExtension->MaximumNumberOfTargets;
		primUnitDisk->Adapter.MaximumNumberOfLogicalUnits	= HwDeviceExtension->MaximumNumberOfLogicalUnits;
		primUnitDisk->Adapter.MaxBlocksPerRequest			= HwDeviceExtension->AdapterMaxBlocksPerRequest;

		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		primUnitDisk->Adapter.Status						= HwDeviceExtension->AdapterStatus;
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		//
		// LUR information ( Scsi LU information )
		//
		primUnitDisk->Lur.Length		= sizeof(LSMP_LUR);
		primUnitDisk->Lur.DevType		= HwDeviceExtension->LURs[0]->DevType;
		primUnitDisk->Lur.TargetId		= HwDeviceExtension->LURs[0]->LurId[1];
		primUnitDisk->Lur.Lun			= HwDeviceExtension->LURs[0]->LurId[2];
		primUnitDisk->Lur.LurnCnt		= HwDeviceExtension->LURs[0]->NodeCount;
		primUnitDisk->Lur.DesiredAccess = HwDeviceExtension->LURs[0]->DesiredAccess;
		primUnitDisk->Lur.GrantedAccess = HwDeviceExtension->LURs[0]->GrantedAccess;

		//
		//	Unit device
		//

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
		RtlCopyMemory(
			primUnitDisk->UnitDisk.Password_v2,
			&LurPrimaryInfo->PrimaryLurn.Password_v2,
			sizeof(primUnitDisk->UnitDisk.Password_v2)
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
		info->Adapter.MaxBlocksPerRequest			= HwDeviceExtension->AdapterMaxBlocksPerRequest;

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
			LSCcbFree(Ccb);

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
		KDPrint(3,("going to default LuExtention 0.\n"));
		ntStatus = LurRequest(
							HwDeviceExtension->LURs[0],	// default: 0.
							Ccb
						);
		DesiredAccess = HwDeviceExtension->LURs[0]->DesiredAccess;
		GrantedAccess = HwDeviceExtension->LURs[0]->GrantedAccess;
		if(!NT_SUCCESS(ntStatus)) {
			LSCcbFree(Ccb);

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

			RtlCopyMemory(
					unitDisk->Password_v2,
					&lurnInformation->Password_v2,
					sizeof(unitDisk->Password_v2)
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
		IN PNDSC_WORKITEM	WorkitemCtx
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

	status = LurCreate(
				LurDesc,
				DefaultLurFlags,
				&Lur,
				0,
				HwDeviceExtension->ScsiportFdoObject,
				LsmpLurnCallback);
	if(NT_SUCCESS(status)) {
		LURCount = InterlockedIncrement(&HwDeviceExtension->LURCount);
		//
		//	We support only one LUR for now.
		//
		ASSERT(LURCount == 1);

		HwDeviceExtension->LURs[0] = Lur;
	}

	//
	//	Free LUR Descriptor
	//
	ExFreePoolWithTag(LurDesc, LSMP_PTAG_IOCTL);

	//
	//	Make a reference to driver object for each LUR creation
	//	to prevent from unloading unexpectedly.
	//	Must be decreased at each LUR deletion.
	//

	ObReferenceObject(_NdscGlobals.DriverObject);

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
	NDSC_WORKITEM_INIT			WorkitemCtx;
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
	LsuIncrementTdiClientInProgress();
	LSCcbSetCompletionRoutine(Ccb, NdscAdapterCompletion, HwDeviceExtension);
	LSCcbSetNextStackLocation(Ccb);


	//
	//	Queue a workitem
	//
	NDSC_INIT_WORKITEM(&WorkitemCtx, AddNewDeviceToMiniport_Worker, Ccb, LuExtension, LurDesc, HwDeviceExtension);
	status = MiniQueueWorkItem(&_NdscGlobals, HwDeviceExtension->ScsiportFdoObject,&WorkitemCtx);
	if(NT_SUCCESS(status)) {
		status = STATUS_PENDING;
	}

	return status;
}

NTSTATUS
RemoveDeviceFromMiniport_Worker(
		IN PDEVICE_OBJECT		DeviceObject,
		IN PNDSC_WORKITEM	WorkitemCtx
	) {
	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension;
	PMINIPORT_LU_EXTENSION		LuExtension;
	LONG						LURCount;
	KIRQL						oldIrql;
	PLURELATION					Lur;
	PCCB						Ccb;
	NTSTATUS					status;

	ASSERT(WorkitemCtx);
	UNREFERENCED_PARAMETER(DeviceObject);

	KDPrint(1,("Entered.\n"));

	LuExtension			= (PMINIPORT_LU_EXTENSION)WorkitemCtx->Arg1;
	HwDeviceExtension	= (PMINIPORT_DEVICE_EXTENSION)WorkitemCtx->Arg3;
	Ccb					= WorkitemCtx->Ccb;

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

	//
	//	We support only one LUR for now.
	//
	Lur = HwDeviceExtension->LURs[0];
	HwDeviceExtension->LURs[0] = NULL;

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	if(!Lur) {
		if(Ccb) {
			LSCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
			LSCcbCompleteCcb(Ccb);
		}

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

	//
	//	send stop CCB
	//

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
	ASSERT(LURCount == 0);


	//
	//	Dereference the driver object for this LUR
	//

	if(Lur != NULL)
		ObDereferenceObject(_NdscGlobals.DriverObject);

	//
	//	Notify a bus change.
	//
	LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_BUSRESET_REQUIRED);
	LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
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
	NDSC_WORKITEM_INIT			WorkitemCtx;
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
	LsuIncrementTdiClientInProgress();
	LSCcbSetCompletionRoutine(Ccb, NdscAdapterCompletion, HwDeviceExtension);
	LSCcbSetNextStackLocation(Ccb);

	//
	//	Queue a workitem
	//
	NDSC_INIT_WORKITEM(&WorkitemCtx, RemoveDeviceFromMiniport_Worker, Ccb, LuExtension, NULL, HwDeviceExtension);
	status = MiniQueueWorkItem(&_NdscGlobals, HwDeviceExtension->ScsiportFdoObject, &WorkitemCtx);
	if(NT_SUCCESS(status)) {
		status = STATUS_PENDING;
	}

	return status;
}


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


	UNREFERENCED_PARAMETER(LuExtension);

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

	KDPrint(3,("going to default LUR 0.\n"));
	ntStatus = LurRequest(
						HwDeviceExtension->LURs[0],	// default: 0.
						Ccb
					);

	if(!NT_SUCCESS(ntStatus)) {
		LSCcbFree(Ccb);

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
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        return STATUS_INVALID_PARAMETER;
    }
	status = STATUS_MORE_PROCESSING_REQUIRED;
	srbStatus = SRB_STATUS_SUCCESS;

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
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				KDPrint(1,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE: LSCcbAllocate() failed.\n"));
				break;
			}
			LurnUpdate = (PLURN_UPDATE)ExAllocatePoolWithTag(NonPagedPool, sizeof(LURN_UPDATE), LSMP_PTAG_IOCTL);
			if(!LurnUpdate) {
				srbStatus = SRB_STATUS_INVALID_REQUEST;
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
			LsuIncrementTdiClientInProgress();
			LSCcbSetCompletionRoutine(Ccb, NdscAdapterCompletion, HwDeviceExtension);

			LurnUpdate->UpdateClass = LURN_UPDATECLASS_WRITEACCESS_USERID;

			KDPrint(3,("LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE:  going to default LuExtention 0.\n"));
			status = LurRequest(
								HwDeviceExtension->LURs[0],	// default: 0.
								Ccb
							);
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
				srbStatus = SRB_STATUS_INVALID_REQUEST;
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
				srbStatus = SRB_STATUS_ERROR;
				status = STATUS_SUCCESS;
				break;
			}

			//	Pass Adapter max request block as LUR max request blocks 
			status = LurTranslateAddTargetDataToLURDesc(
									AddTargetData,
									HwDeviceExtension->AdapterMaxBlocksPerRequest,
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
			status = RemoveDeviceFromMiniport(HwDeviceExtension, LuExtension, Srb, CurSrbSequence);
			break;
		}

		case LANSCSIMINIPORT_IOCTL_REMOVE_TARGET: {
			PLANSCSI_REMOVE_TARGET_DATA	RemoveTargetData;

			RemoveTargetData = (PLANSCSI_REMOVE_TARGET_DATA)srbIoctlBuffer;
			status = RemoveDeviceFromMiniport(HwDeviceExtension, NULL, Srb, CurSrbSequence);
			break;
		}
		case LANSCSIMINIPORT_IOCTL_NOOP: {
			PCCB	Ccb;

			//
			//	Query to the LUR
			//
			status = LSCcbAllocate(&Ccb);
			if(!NT_SUCCESS(status)) {
				srbStatus = SRB_STATUS_ERROR;
				status = STATUS_SUCCESS;
				KDPrint(1,("LSCcbAllocate() failed.\n"));
				break;
			}

			LSCCB_INITIALIZE(Ccb, CurSrbSequence);
			Ccb->OperationCode = CCB_OPCODE_NOOP;
			LSCcbSetFlag(Ccb, CCB_FLAG_ALLOCATED);
			Ccb->Srb = Srb;
			LSCcbSetCompletionRoutine(Ccb, NdscAdapterCompletion, HwDeviceExtension);

			KDPrint(3,("going to default LuExtention 0.\n"));
			InterlockedIncrement(&HwDeviceExtension->RequestExecuting);
			LsuIncrementTdiClientInProgress();
			status = LurRequest(
								HwDeviceExtension->LURs[0],	// default: 0.
								Ccb
							);
			if(!NT_SUCCESS(status)) {
				srbStatus = SRB_STATUS_ERROR;
				status = STATUS_SUCCESS;
				KDPrint(1,("LurRequest() failed.\n"));
				break;
			}

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_PENDING;
		break;
		}
		case LANSCSIMINIPORT_IOCTL_GET_VERSION: {
			PLSMPIOCTL_DRVVER	version = (PLSMPIOCTL_DRVVER)srbIoctlBuffer;

			version->VersionMajor = VER_FILEMAJORVERSION;
			version->VersionMinor = VER_FILEMINORVERSION;
			version->VersionBuild = VER_FILEBUILD;
			version->VersionPrivate = VER_FILEBUILD_QFE;

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;

		break;
		}
        default:

            KDPrint(2,("Control Code (%x)\n", controlCode));
            srbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_MORE_PROCESSING_REQUIRED;
    }

	Srb->SrbStatus = srbStatus;

    return status;
}



NTSTATUS
LSMPSendIoctlSrb(
		IN PDEVICE_OBJECT	DeviceObject,
		IN ULONG			IoctlCode,
		IN PVOID			InputBuffer,
		IN LONG				InputBufferLength,
		OUT PVOID			OutputBuffer,
		IN LONG				OutputBufferLength
	) {

    PIRP				irp;
    KEVENT				event;
	PSRB_IO_CONTROL		psrbIoctl;
	LONG				srbIoctlLength;
	PVOID				srbIoctlBuffer;
	LONG				srbIoctlBufferLength;
    NTSTATUS			status;
    PIO_STACK_LOCATION	irpStack;
    SCSI_REQUEST_BLOCK	srb;
    LARGE_INTEGER		startingOffset;
    IO_STATUS_BLOCK		ioStatusBlock;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	psrbIoctl	= NULL;
	irp = NULL;

	//
	//	build an SRB for the miniport
	//
	srbIoctlBufferLength = (InputBufferLength>OutputBufferLength)?InputBufferLength:OutputBufferLength;
	srbIoctlLength = sizeof(SRB_IO_CONTROL) +  srbIoctlBufferLength;

	psrbIoctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , srbIoctlLength, LSMP_PTAG_SRB);
	if(psrbIoctl == NULL) {
		KDPrint(1, ("STATUS_INSUFFICIENT_RESOURCES\n"));
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	RtlZeroMemory(psrbIoctl, srbIoctlLength);
	psrbIoctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(psrbIoctl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
	psrbIoctl->Timeout = 10;
	psrbIoctl->ControlCode = IoctlCode;
	psrbIoctl->Length = srbIoctlBufferLength;

	srbIoctlBuffer = (PUCHAR)psrbIoctl + sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(srbIoctlBuffer, InputBuffer, InputBufferLength);

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                        NotificationEvent,
                        FALSE);
	startingOffset.QuadPart = 1;

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchonously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    irp = IoBuildSynchronousFsdRequest(
                IRP_MJ_SCSI,
                DeviceObject,
                psrbIoctl,
                srbIoctlLength,
                &startingOffset,
                &event,
                &ioStatusBlock);

    irpStack = IoGetNextIrpStackLocation(irp);

    if (irp == NULL) {
        KDPrint(1,("STATUS_INSUFFICIENT_RESOURCES\n"));

		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
    }

    //
    // Set major and minor codes.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = 1;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    srb.PathId = 0;
    srb.TargetId = 0;
    srb.Lun = 0;

    srb.Function = SRB_FUNCTION_IO_CONTROL;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;

	srb.QueueAction = SRB_SIMPLE_TAG_REQUEST;
	srb.QueueTag = SP_UNTAGGED;

    srb.OriginalRequest = irp;

    //
    // Set timeout to requested value.
    //

    srb.TimeOutValue = psrbIoctl->Timeout;

    //
    // Set the data buffer.
    //

    srb.DataBuffer = psrbIoctl;
    srb.DataTransferLength = srbIoctlLength;

    //
    // Flush the data buffer for output. This will insure that the data is
    // written back to memory.  Since the data-in flag is the the port driver
    // will flush the data again for input which will ensure the data is not
    // in the cache.
    //
/*
    KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
*/
    status = IoCallDriver( DeviceObject, irp );

    //
    // Wait for request to complete.
    //
    if (status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( 
									&event,
                                     Executive,
                                     KernelMode,
                                     FALSE,
                                     (PLARGE_INTEGER)NULL 
									 );

        status = ioStatusBlock.Status;
    }

	//
	//	get the result
	//
	if(status == STATUS_SUCCESS) {
		if(OutputBuffer && OutputBufferLength)
			RtlCopyMemory(OutputBuffer, srbIoctlBuffer, OutputBufferLength);
			KDPrint(1,("Ioctl(%08lx) succeeded!\n", IoctlCode));
	}

cleanup:
	if(psrbIoctl)
		ExFreePool(psrbIoctl);

    return status;
}
