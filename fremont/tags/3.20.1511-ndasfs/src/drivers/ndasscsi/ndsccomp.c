#include <initguid.h>
#include "ndasscsi.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NDSCCOMP"


//////////////////////////////////////////////////////////////////////////
//
//	search scsiport FDO
//

typedef struct _ADAPTER_EXTENSION {

	PDEVICE_OBJECT DeviceObject;

} ADAPTER_EXTENSION, *PADAPTER_EXTENSION;

typedef struct _HW_DEVICE_EXTENSION2 {

    PADAPTER_EXTENSION FdoExtension;
    UCHAR HwDeviceExtension[0];

} HW_DEVICE_EXTENSION2, *PHW_DEVICE_EXTENSION2;

#define GET_FDO_EXTENSION(HwExt) ((CONTAINING_RECORD(HwExt, HW_DEVICE_EXTENSION2, HwDeviceExtension))->FdoExtension)


PDEVICE_OBJECT
FindScsiportFdo(
		IN OUT	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
){
	// DeviceObject which is one of argument is always NULL.
	PADAPTER_EXTENSION		deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);

	ASSERT(deviceExtension->DeviceObject);

	return deviceExtension->DeviceObject;
}


//////////////////////////////////////////////////////////////////////////
//
//	Event log
//

VOID
NDScsiLogError(
	IN PMINIPORT_DEVICE_EXTENSION HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb OPTIONAL,
	IN UCHAR PathId,
	IN UCHAR TargetId,
	IN UCHAR Lun,
	IN ULONG ErrorCode,
	IN ULONG UniqueId
){
	PDEVICE_OBJECT DeviceObject = HwDeviceExtension->ScsiportFdoObject;
	LSU_ERROR_LOG_ENTRY errorLogEntry;

	UNREFERENCED_PARAMETER(Srb);

	if(HwDeviceExtension == NULL) {
		KDPrint(1, ("HwDeviceExtension NULL!! \n"));
		return;
	}
	if(DeviceObject == NULL) {
		KDPrint(1, ("DeviceObject NULL!! \n"));
		return;
	}

	//
    // Save the error log data in the log entry.
    //

	errorLogEntry.ErrorCode = ErrorCode;
	errorLogEntry.MajorFunctionCode = IRP_MJ_SCSI;
	errorLogEntry.IoctlCode = 0;
    errorLogEntry.UniqueId = UniqueId;
	errorLogEntry.SequenceNumber = 0;
	errorLogEntry.ErrorLogRetryCount = 0;
	errorLogEntry.Parameter2 = HwDeviceExtension->SlotNumber;
	errorLogEntry.DumpDataEntry = 4;
	errorLogEntry.DumpData[0] = TargetId;
	errorLogEntry.DumpData[1] = Lun;
	errorLogEntry.DumpData[2] = PathId;
	errorLogEntry.DumpData[3] = ErrorCode;

	LsuWriteLogErrorEntry(DeviceObject, &errorLogEntry);

	return;
}


//////////////////////////////////////////////////////////////////////////
//
//	I/o control to NDASBUS
//

//
//	General ioctl to NDASBUS
//
NTSTATUS
IoctlToLanscsiBus(
	IN ULONG		IoControlCode,
    IN PVOID		InputBuffer  OPTIONAL,
    IN ULONG		InputBufferLength,
    OUT PVOID		OutputBuffer  OPTIONAL,
    IN ULONG		OutputBufferLength,
	OUT PULONG		BufferNeeded
)
{
	NTSTATUS			ntStatus;
	PWSTR				symbolicLinkList;
    UNICODE_STRING		objectName;
	PFILE_OBJECT		fileObject;
	PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
    KEVENT				event;
    IO_STATUS_BLOCK		ioStatus;


	KDPrint(2,("IoControlCode = %x\n", IoControlCode));

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	ntStatus = IoGetDeviceInterfaces(
		&GUID_NDAS_BUS_ENUMERATOR_INTERFACE_CLASS,
		NULL,
		0,
		&symbolicLinkList
		);
	
	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("IoGetDeviceInterfaces ntStatus = 0x%x\n", ntStatus));
		return ntStatus;
	}
	
	ASSERT(symbolicLinkList != NULL);

	KDPrint(1,("symbolicLinkList = %ws\n", symbolicLinkList));

    RtlInitUnicodeString(&objectName, symbolicLinkList);
	ntStatus = IoGetDeviceObjectPointer(
					&objectName,
					FILE_ALL_ACCESS,
					&fileObject,
					&deviceObject
					);

	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("ntStatus = 0x%x\n", ntStatus));
		ExFreePool(symbolicLinkList);
		return ntStatus;
	}
	
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
		KDPrint(1,("irp NULL\n"));
		ExFreePool(symbolicLinkList);
		ObDereferenceObject(fileObject); 
		return ntStatus;
	}
	KDPrint(2,("Before Done...ioStatus.Status = 0x%08x Information = %d\n", ioStatus.Status, ioStatus.Information));

	ntStatus = IoCallDriver(deviceObject, irp);
    if (ntStatus == STATUS_PENDING)
    {
		KDPrint(1,("IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        ntStatus = ioStatus.Status;
    } else if(NT_SUCCESS(ntStatus)) {
		ntStatus = ioStatus.Status;
    }

	if(BufferNeeded)
		*BufferNeeded = (ULONG)ioStatus.Information;

	ExFreePool(symbolicLinkList);
	ObDereferenceObject(fileObject);
	
	KDPrint(1,("Done...ioStatus.Status = 0x%08x Information = %d\n", ioStatus.Status, ioStatus.Information));
	
	return ntStatus;
}

VOID
IoctlToLanscsiBus_Worker(
		IN PDEVICE_OBJECT		DeviceObject,
		IN PNDSC_WORKITEM	WorkitemCtx
	) {

	UNREFERENCED_PARAMETER(DeviceObject);

	IoctlToLanscsiBus(
				PtrToUlong(WorkitemCtx->Ccb),
				WorkitemCtx->Arg1,
				PtrToUlong(WorkitemCtx->Arg2),
				NULL,
				0,
				NULL
			);


	if(WorkitemCtx->Arg2 && WorkitemCtx->Arg1)
		ExFreePoolWithTag(WorkitemCtx->Arg1, NDSC_PTAG_IOCTL);
}

NTSTATUS
IoctlToLanscsiBusByWorker(
	IN PDEVICE_OBJECT	WorkerDeviceObject,
	IN ULONG			IoControlCode,
    IN PVOID			InputBuffer  OPTIONAL,
    IN ULONG			InputBufferLength
) {
	NDSC_WORKITEM_INIT	WorkitemCtx;
	NTSTATUS			status;

	NDSC_INIT_WORKITEM(	&WorkitemCtx,
							IoctlToLanscsiBus_Worker,
							(PCCB)UlongToPtr(IoControlCode),
							InputBuffer,
							UlongToPtr(InputBufferLength),
							NULL);
	status = MiniQueueWorkItem(&_NdscGlobals, WorkerDeviceObject, &WorkitemCtx);
	if(!NT_SUCCESS(status)) {
		if(InputBufferLength && InputBuffer)
			ExFreePoolWithTag(InputBuffer, NDSC_PTAG_IOCTL);
	}

	return status;
}

//
//	Update Adapter status without Lur.
//
NTSTATUS
UpdateStatusInLSBus(
		ULONG	SlotNo,
		ULONG	AdapterStatus
) {
	NDASBUS_SETPDOINFO	BusSet;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	//
	//	Update Status in LanscsiBus
	//
	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = AdapterStatus;
	BusSet.SupportedFeatures = BusSet.EnabledFeatures = 0;
	return IoctlToLanscsiBus(
			IOCTL_NDASBUS_SETPDOINFO,
			&BusSet,
			sizeof(NDASBUS_SETPDOINFO),
			NULL,
			0,
			NULL);
}

//
// query scsiport PDO
//
ULONG
GetScsiAdapterPdoEnumInfo(
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN ULONG						SystemIoBusNumber,
	OUT PLONG						AddDevInfoLength,
	OUT PVOID						*AddDevInfo,
	OUT PULONG						AddDevInfoFlags
) {
	NTSTATUS						status;
	NDASBUS_QUERY_INFORMATION		BusQuery;
	PNDASBUS_INFORMATION			BusInfo;
	LONG							BufferNeeded;

	KDPrint(1,("SystemIoBusNumber:%d\n", SystemIoBusNumber));
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	BusQuery.Size = sizeof(NDASBUS_QUERY_INFORMATION);
	BusQuery.InfoClass = INFORMATION_PDOENUM;
	BusQuery.SlotNo = SystemIoBusNumber;

	//
	//	Get a buffer length needed.
	//
	status = IoctlToLanscsiBus(
						IOCTL_NDASBUS_QUERY_INFORMATION,
						&BusQuery,
						sizeof(NDASBUS_QUERY_INFORMATION),
						NULL,
						0,
						&BufferNeeded
					);
	if(status != STATUS_BUFFER_TOO_SMALL || BufferNeeded <= 0) {
		KDPrint(1,("IoctlToLanscsiBus() Failed.\n"));
		return SP_RETURN_NOT_FOUND;
	}

	//
	//
	//
	BusInfo= (PNDASBUS_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, BufferNeeded, NDSC_PTAG_IOCTL);
	status = IoctlToLanscsiBus(
						IOCTL_NDASBUS_QUERY_INFORMATION,
						&BusQuery,
						sizeof(NDASBUS_QUERY_INFORMATION),
						BusInfo,
						BufferNeeded,
						&BufferNeeded
					);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(BusInfo, NDSC_PTAG_IOCTL);
		return SP_RETURN_NOT_FOUND;
	}

	HwDeviceExtension->AdapterMaxDataTransferLength = BusInfo->PdoEnumInfo.MaxRequestLength;
	if(HwDeviceExtension->AdapterMaxDataTransferLength == 0) {
		HwDeviceExtension->AdapterMaxDataTransferLength = NDSC_DEFAULT_MAXDATATRANSFER;
	}

	HwDeviceExtension->EnumFlags			= BusInfo->PdoEnumInfo.Flags;
	*AddDevInfoFlags						= BusInfo->PdoEnumInfo.Flags;

	KDPrint(1,("MaxRequestLength:%d\n",
						BusInfo->PdoEnumInfo.MaxRequestLength));

	if(BusInfo->PdoEnumInfo.Flags & PDOENUM_FLAG_LURDESC) {
		ULONG				LurDescLen;
		PLURELATION_DESC	lurDesc;
		PLURELATION_DESC	lurDescOrig;

		lurDescOrig = (PLURELATION_DESC)BusInfo->PdoEnumInfo.AddDevInfo;
		LurDescLen = lurDescOrig->Length;
		//
		//	Verify sanity
		//
		if(lurDescOrig->CntEcrKeyLength > NDAS_CONTENTENCRYPT_KEY_LENGTH) {
			KDPrint(1,("LurDescOrig has invalid key length: %d\n", lurDescOrig->CntEcrKeyLength));
			return SP_RETURN_NOT_FOUND;
		}

		//
		//	Allocate pool for the LUREALTION descriptor
		//	copy LUREALTION descriptor
		//
		lurDesc = (PLURELATION_DESC)ExAllocatePoolWithTag(NonPagedPool, LurDescLen, NDSC_PTAG_ENUMINFO);
		if(lurDesc == NULL) {
			ExFreePoolWithTag(BusInfo,NDSC_PTAG_IOCTL);
			return SP_RETURN_NOT_FOUND;
		}

		RtlCopyMemory(lurDesc, &BusInfo->PdoEnumInfo.AddDevInfo, LurDescLen);
		*AddDevInfo = lurDesc;
		*AddDevInfoLength = LurDescLen;
	} else {
		LONG							addTargetDataLength;
		PNDASBUS_ADD_TARGET_DATA		addTargetData;
		PNDASBUS_ADD_TARGET_DATA		addTargetDataOrig;
		PNDAS_BLOCK_ACL					ndasBacl;

		addTargetDataOrig = (PNDASBUS_ADD_TARGET_DATA)BusInfo->PdoEnumInfo.AddDevInfo;
		addTargetDataLength =	FIELD_OFFSET(NDASBUS_ADD_TARGET_DATA, UnitDiskList) +
								addTargetDataOrig->ulNumberOfUnitDiskList * sizeof(NDASBUS_UNITDISK);
		// Ndas Bacl length
		if(addTargetDataOrig->BACLOffset) {
			ndasBacl = (PNDAS_BLOCK_ACL)((PUCHAR)addTargetDataOrig + addTargetDataOrig->BACLOffset);
			addTargetDataLength += ndasBacl->Length;
		} else
			ndasBacl = NULL;

		//
		//	Verify sanity
		//
		if(addTargetDataOrig->CntEcrKeyLength > NDAS_CONTENTENCRYPT_KEY_LENGTH) {
			KDPrint(1,("AddTargetData has invalid key length: %d\n", addTargetDataOrig->CntEcrKeyLength));
			return SP_RETURN_NOT_FOUND;
		}

		//
		//	Allocate pool for the ADD_TARGET_DATA.
		//	copy NDASBUS_ADD_TARGET_DATA
		//
		addTargetData = (PNDASBUS_ADD_TARGET_DATA)ExAllocatePoolWithTag(NonPagedPool, addTargetDataLength, NDSC_PTAG_ENUMINFO);
		if(addTargetData == NULL) {
			ExFreePoolWithTag(BusInfo,NDSC_PTAG_IOCTL);
			return SP_RETURN_NOT_FOUND;
		}

		RtlCopyMemory(addTargetData, &BusInfo->PdoEnumInfo.AddDevInfo, addTargetDataLength);
		*AddDevInfo = addTargetData;
		*AddDevInfoLength = addTargetDataLength;
	}

	ExFreePoolWithTag(BusInfo,NDSC_PTAG_IOCTL);
	return SP_RETURN_FOUND;
}


VOID
UpdatePdoInfoInLSBus(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						AdapterStatus
	) {
	PNDASBUS_SETPDOINFO		busSet;
	NDAS_FEATURES			supportedFeatures, enabledFeatures;

	ASSERT(HwDeviceExtension);

	//
	//	Query to the LUR
	//
	if(HwDeviceExtension->LURs[0]) {
		KDPrint(3,("going to default LuExtention 0.\n"));
		supportedFeatures = HwDeviceExtension->LURs[0]->SupportedNdasFeatures;
		enabledFeatures = HwDeviceExtension->LURs[0]->EnabledNdasFeatures;
	} else {
		KDPrint(1,("No LUR available..\n"));
		supportedFeatures = enabledFeatures = 0;
		return;
	}

	KDPrint(1,("Set AdapterStatus=%x Supp=%x Enab=%x\n",
					AdapterStatus,
					supportedFeatures,
					enabledFeatures));

	//
	//	Send to LSBus
	//
	busSet = (PNDASBUS_SETPDOINFO)ExAllocatePoolWithTag(NonPagedPool, sizeof(NDASBUS_SETPDOINFO), NDSC_PTAG_IOCTL);
	if(busSet == NULL) {
		return;
	}

	busSet->Size			= sizeof(NDASBUS_SETPDOINFO);
	busSet->SlotNo			= HwDeviceExtension->SlotNumber;
	busSet->AdapterStatus	= AdapterStatus;
	busSet->SupportedFeatures	= supportedFeatures;
	busSet->EnabledFeatures	= enabledFeatures;

	if(KeGetCurrentIrql() == PASSIVE_LEVEL) {
		IoctlToLanscsiBus(
						IOCTL_NDASBUS_SETPDOINFO,
						busSet,
						sizeof(NDASBUS_SETPDOINFO),
						NULL,
						0,
						NULL);
		ExFreePoolWithTag(busSet, NDSC_PTAG_IOCTL);

	} else {

		//
		//	IoctlToLanscsiBus_Worker() will free memory of BusSet.
		//
		IoctlToLanscsiBusByWorker(
						HwDeviceExtension->ScsiportFdoObject,
						IOCTL_NDASBUS_SETPDOINFO,
						busSet,
						sizeof(NDASBUS_SETPDOINFO)
					);
	}
}




//////////////////////////////////////////////////////////////////////////
//
//	NDASSCSI adapter IO completion
//


//
// Timer routine called by ScsiPort after the specified interval.
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.
//

VOID
MiniTimer(
    IN PMINIPORT_DEVICE_EXTENSION HwDeviceExtension
    )
{
	BOOLEAN	busChanged;

	busChanged = FALSE;

	do {
		PLIST_ENTRY			listEntry;
		PCCB				ccb;
		PSCSI_REQUEST_BLOCK	srb;

		// Get Completed CCB
		listEntry = ExInterlockedRemoveHeadList(
							&HwDeviceExtension->CcbTimerCompletionList,
							&HwDeviceExtension->CcbTimerCompletionListSpinLock
						);

		if(listEntry == NULL)
			break;

		ccb = CONTAINING_RECORD(listEntry, CCB, ListEntry);
		srb = ccb->Srb;
		ASSERT(srb);
		ASSERT(LSCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE));
		KDPrint(1,("completing CCB:%p SCSIOP:%x\n", ccb, ccb->Cdb[0]));

		if(LSCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_BUSCHANGE)) {
			busChanged = TRUE;
		}

#if DBG
		if(!srb)
			KDPrint(1,("CCB:%p doesn't have SRB.\n", srb));
#endif

		//
		//	Perform CCB post-completion.
		//
		LSCcbPostCompleteCcb(ccb);

		InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		ASSERT(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		ScsiPortNotification(
				RequestComplete,
				HwDeviceExtension,
				srb
			);

	} while(1);


	//
	//	If any CCb has a BusChanged request,
	//	Notify it to ScsiPort.
	//
	if(busChanged) {
		KDPrint(1,("Bus change detected. RequestExecuting = %d\n", HwDeviceExtension->RequestExecuting));
		ScsiPortNotification(
			BusChangeDetected,
			HwDeviceExtension,
			NULL
			);
	}

	//
	//	If a SRB is still pending, reschedule the timer to complete
	//	the SRB returning with timer completion.
	//

	if(HwDeviceExtension->RequestExecuting != 0) {
		ScsiPortNotification(
			RequestTimerCall,
			HwDeviceExtension,
			MiniTimer,
			500 * NDSC_TIMER_VALUE // micro second
			);
		HwDeviceExtension->TimerOn = TRUE;
	} else {


		//
		//	No pending SRB.
		//	Indicate timer is off.
		//

		HwDeviceExtension->TimerOn = FALSE;
	}

	ScsiPortNotification(
		NextRequest,
		HwDeviceExtension,
		NULL
		);

	return;
}


//
//	User-context for completion IRPs
//

typedef struct _COMPLETION_DATA {

    PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension;
	PCCB						ShippedCcb;
	BOOLEAN						ShippedCcbAllocatedFromPool;
	PSCSI_REQUEST_BLOCK			CompletionSrb;

} COMPLETION_DATA, *PCOMPLETION_DATA;


//
//	Completion routine for IRPs carrying SRB.
//
// NOTE: This routine executes in the context of arbitrary system thread.
//

NTSTATUS
CompletionIrpCompletionRoutine(
		IN PDEVICE_OBJECT		DeviceObject,
		IN PIRP					Irp,
		IN PCOMPLETION_DATA		Context
	)
{
    PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension = Context->HwDeviceExtension;

	PSCSI_REQUEST_BLOCK			completionSrb = Context->CompletionSrb;
	PCCB						shippedCcb = Context->ShippedCcb;


	KDPrint(3, ("Entered\n"));
	UNREFERENCED_PARAMETER(DeviceObject);

	if(completionSrb->DataBuffer) 
	{
		KDPrint(1, ("Unexpected IRP completion!!! "
			"Maybe completion SRB did not reach NDASSCSI's StartIo routine.\n"));
		ASSERT(completionSrb->DataBuffer);

		InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		InitializeListHead(&shippedCcb->ListEntry);
		LSCcbSetStatusFlag(shippedCcb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		ExInterlockedInsertTailList(
				&HwDeviceExtension->CcbTimerCompletionList,
				&shippedCcb->ListEntry,
				&HwDeviceExtension->CcbTimerCompletionListSpinLock
				);

		completionSrb->DataBuffer = NULL;
	} else {
		//
		//	Free the CCB if it is not going to the timer completion routine
		//	and allocated from the system pool by NDASSCSI.
		//
		if(Context->ShippedCcbAllocatedFromPool)
			LSCcbPostCompleteCcb(shippedCcb);
	}

	// Free resources
	ExFreePoolWithTag(completionSrb, NDSC_PTAG_SRB);
	ExFreePoolWithTag(Context, NDSC_PTAG_CMPDATA);
	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

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
//	MACRO for the convenience
//

#define SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(ERRORCODE, EVT_ID)	\
	NDScsiLogError(													\
							HwDeviceExtension,						\
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
NdscAdapterCompletion(
		  IN PCCB							Ccb,
		  IN PMINIPORT_DEVICE_EXTENSION		HwDeviceExtension
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

	KDPrint(3,("RequestExecuting = %d\n", HwDeviceExtension->RequestExecuting));

	srb = Ccb->Srb;
	if(!srb) {
		KDPrint(1,("Ccb:%p CcbStatus %d. No srb assigned.\n", Ccb, Ccb->CcbStatus));
		ASSERT(srb);
		return STATUS_SUCCESS;
	}
 
	//
	//	NDASSCSI completion routine will do post operation to complete CCBs.
	//
	return_status = STATUS_MORE_PROCESSING_REQUIRED;

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

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

	// Save the bus-reset flag
	if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING)) {
		busResetOccured = TRUE;
	} else {
		busResetOccured = FALSE;
	}

	// Save the current flag
	AdapterStatusBefore = HwDeviceExtension->AdapterStatus;


	//	Check reconnecting process.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RECONNECTING))
	{
		ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECONNECT_PENDING);
	} else {
		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECONNECT_PENDING);
	}

	//
	// Update adapter status only when CCBSTATUS_FLAG_RAID_FLAG_VALID is on.
	// In other case, Ccb has no chance to get flag information from RAID.
	//
	if (LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FLAG_VALID)) {
		//	Check to see if the associate member is in error.
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_DEGRADED))
		{
			ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_MEMBER_FAULT);
		} else {
			ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_MEMBER_FAULT);
		}

		//	Check recovering process.
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_RECOVERING))	{
			ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECOVERING);
		} else {
			ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECOVERING);
		}

		// Check RAID failure
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FAILURE))	{
			ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RAID_FAILURE);
		} else {
			ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RAID_FAILURE);
		}

		// Set RAID normal status
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_NORMAL))	{
			ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RAID_NORMAL);
		} else {
			ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RAID_NORMAL);
		}
	}	
	// power-recycle occurred.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_POWERRECYLE_OCCUR)) {
		ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERRECYCLED);
	} else {
		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERRECYCLED);
	}


	AdapterStatus = HwDeviceExtension->AdapterStatus;
	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	if(AdapterStatus != AdapterStatusBefore)
	{
		if(
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_MEMBER_FAULT) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// ADAPTER_STATUSFLAG_MEMBER_FAULT on
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_MEMBER_FAULT, EVTLOG_MEMBER_IN_ERROR);
			KDPrint(1,("Ccb:%p CcbStatus %d. Set member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & ADAPTER_STATUSFLAG_MEMBER_FAULT) &&
			!(AdapterStatus & ADAPTER_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// ADAPTER_STATUSFLAG_MEMBER_FAULT off
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_MEMBER_FAULT_RECOVERED, EVTLOG_MEMBER_RECOVERED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Reset member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECONNECT_PENDING) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// ADAPTER_STATUSFLAG_RECONNECT_PENDING on
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECONNECT_START, EVTLOG_START_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Start reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECONNECT_PENDING) &&
			!(AdapterStatus & ADAPTER_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// ADAPTER_STATUSFLAG_RECONNECT_PENDING off
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECONNECTED, EVTLOG_END_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Finish reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECOVERING) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_RECOVERING)
			)
		{
			// ADAPTER_STATUSFLAG_RECOVERING on
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECOVERY_START, EVTLOG_START_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & ADAPTER_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & (ADAPTER_STATUSFLAG_RAID_FAILURE|ADAPTER_STATUSFLAG_MEMBER_FAULT)) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_RAID_NORMAL))
		{
			// ADAPTER_STATUSFLAG_RECOVERING off
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECOVERED, EVTLOG_END_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Ended recovering\n", Ccb, Ccb->CcbStatus));
		}
		if (
			(AdapterStatus & ADAPTER_STATUSFLAG_RAID_FAILURE)	 &&
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_RAID_FAILURE))
		{
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RAID_FAILURE, EVTLOG_RAID_FAILURE);
			KDPrint(1,("Ccb:%p CcbStatus %d. RAID failure\n", Ccb, Ccb->CcbStatus));
		}
		
		if(
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_POWERRECYCLED) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_POWERRECYCLED)
			)
		{
			// ADAPTER_STATUSFLAG_POWERRECYCLED on
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_DISK_POWERRECYCLE, EVTLOG_DISK_POWERRECYCLED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}


		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	//	If CCB_OPCODE_UPDATE is successful, update adapter status in LanscsiBus
	//
	if(Ccb->OperationCode == CCB_OPCODE_UPDATE) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_UPGRADE_SUCC, EVTLOG_SUCCEED_UPGRADE);
		} else {
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_UPGRADE_FAIL, EVTLOG_FAIL_UPGRADE);
		}
		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	// If device lock CCB is successful, copy the result to the SRB.
	//

	if(Ccb->OperationCode == CCB_OPCODE_DEVLOCK) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			PSRB_IO_CONTROL			srbIoctlBuffer;
			PUCHAR					lockIoctlBuffer;
			PNDSCIOCTL_DEVICELOCK	ioCtlAcReDeviceLock;
			PLURN_DEVLOCK_CONTROL	lurnAcReDeviceLock;

			//
			// Get the Ioctl buffer.
			//
			srbIoctlBuffer = (PSRB_IO_CONTROL)srb->DataBuffer;
			srbIoctlBuffer->ReturnCode = 0;
			lockIoctlBuffer = (PUCHAR)(srbIoctlBuffer + 1);
			ioCtlAcReDeviceLock = (PNDSCIOCTL_DEVICELOCK)lockIoctlBuffer;
			lurnAcReDeviceLock = (PLURN_DEVLOCK_CONTROL)Ccb->DataBuffer;

			// Copy the result
			RtlCopyMemory(	ioCtlAcReDeviceLock->LockData,
							lurnAcReDeviceLock->LockData,
							NDSCLOCK_LOCKDATA_LENGTH);
		}
	}

	KDPrint(3,("CcbStatus %d\n", Ccb->CcbStatus));

	//
	//	Translate CcbStatus to SrbStatus
	//

	CcbStatusToSrbStatus(Ccb, srb);

	//
	// Perform stop process when we get stop status.
	//

	if(Ccb->CcbStatus == CCB_STATUS_STOP) {
		MiniStopAdapter(HwDeviceExtension, FALSE);
	} else {
		//
		// Update PDO information on the NDAS bus.
		//

		if(NeedToUpdatePdoInfoInLSBus)
		{
			KDPrint(1, ("<<<<<<<<<<<<<<<< %08lx -> %08lx ADAPTER STATUS CHANGED"
				" >>>>>>>>>>>>>>>>\n", AdapterStatusBefore, AdapterStatus));
			UpdatePdoInfoInLSBus(HwDeviceExtension, HwDeviceExtension->AdapterStatus);
		}
	}

	//
	// Process Abort CCB.
	//
	abortCcb = Ccb->AbortCcb;

	if(abortCcb != NULL) {

		KDPrint(1,("abortSrb\n"));
		ASSERT(FALSE);

		srb->SrbStatus = SRB_STATUS_SUCCESS;
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		InitializeListHead(&Ccb->ListEntry);
		ExInterlockedInsertTailList(
				&HwDeviceExtension->CcbTimerCompletionList,
				&Ccb->ListEntry,
				&HwDeviceExtension->CcbTimerCompletionListSpinLock
			);

		((PSCSI_REQUEST_BLOCK)abortCcb->Srb)->SrbStatus = SRB_STATUS_ABORTED;
		LSCcbSetStatusFlag(abortCcb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		InitializeListHead(&abortCcb->ListEntry);
		ExInterlockedInsertTailList(
				&HwDeviceExtension->CcbTimerCompletionList,
				&abortCcb->ListEntry,
				&HwDeviceExtension->CcbTimerCompletionListSpinLock
			);

	} else {
		BOOLEAN	criticalSrb;

		//
		// We should not use completion IRP method with disable-disconnect flag.
		// A SRB with DISABLE_DISCONNECT flag prevents the next SRB to start.
		//

		criticalSrb =	(srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) != 0 ||
						(srb->SrbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE) != 0 ||
						(srb->SrbFlags & SRB_FLAGS_BYPASS_LOCKED_QUEUE) != 0;
#if DBG
		if(criticalSrb) {
			KDPrint(1, ("Critical Srb:%p\n", srb));
		}
#endif
		//
		// Make Complete IRP and Send it.
		//
		//
		//	In case of HostStatus == CCB_STATUS_SUCCESS_TIMER, CCB will go to the timer to complete.
		//
		if(		(Ccb->CcbStatus == CCB_STATUS_SUCCESS || Ccb->CcbStatus == CCB_STATUS_DATA_OVERRUN) &&
				!LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE) &&
				!LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_BUSCHANGE) &&
				!busResetOccured &&
				!criticalSrb
			)
		{
			PDEVICE_OBJECT		pDeviceObject = HwDeviceExtension->ScsiportFdoObject;
			PIRP				pCompletionIrp = NULL;
			PIO_STACK_LOCATION	pIoStack;
			NTSTATUS			ntStatus;
			PSCSI_REQUEST_BLOCK	completionSrb = NULL;
			PCOMPLETION_DATA	completionData = NULL;

			completionSrb = ExAllocatePoolWithTag(NonPagedPool, sizeof(SCSI_REQUEST_BLOCK), NDSC_PTAG_SRB);
			if(completionSrb == NULL)
				goto Out;

			RtlZeroMemory(
					completionSrb,
					sizeof(SCSI_REQUEST_BLOCK)
				);

			// Build New IRP.
			pCompletionIrp = IoAllocateIrp((CCHAR)(pDeviceObject->StackSize + 1), FALSE);
			if(pCompletionIrp == NULL) {
				ExFreePoolWithTag(completionSrb, NDSC_PTAG_SRB);
				goto Out;
			}

			completionData = ExAllocatePoolWithTag(NonPagedPool, sizeof(COMPLETION_DATA), NDSC_PTAG_CMPDATA);
			if(completionData == NULL) {
				ExFreePoolWithTag(completionSrb, NDSC_PTAG_SRB);
				IoFreeIrp(pCompletionIrp);
				pCompletionIrp = NULL;

				goto Out;
			}

			pCompletionIrp->MdlAddress = NULL;

			// Set IRP stack location.
			pIoStack = IoGetNextIrpStackLocation(pCompletionIrp);
			pIoStack->DeviceObject = pDeviceObject;
			pIoStack->MajorFunction = IRP_MJ_SCSI;
			pIoStack->Parameters.DeviceIoControl.InputBufferLength = 0;
			pIoStack->Parameters.DeviceIoControl.OutputBufferLength = 0; 
			pIoStack->Parameters.Scsi.Srb = completionSrb;

			// Set SRB.
			completionSrb->Length = sizeof(SCSI_REQUEST_BLOCK);
			completionSrb->Function = SRB_FUNCTION_EXECUTE_SCSI;
			completionSrb->PathId = srb->PathId;
			completionSrb->TargetId = srb->TargetId;
			completionSrb->Lun = srb->Lun;
			completionSrb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
			completionSrb->DataBuffer = Ccb;

			completionSrb->SrbFlags |= SRB_FLAGS_BYPASS_FROZEN_QUEUE | SRB_FLAGS_NO_QUEUE_FREEZE;
			completionSrb->OriginalRequest = pCompletionIrp;
			completionSrb->CdbLength = MAXIMUM_CDB_SIZE;
			completionSrb->Cdb[0] = SCSIOP_COMPLETE;
			completionSrb->Cdb[1] = (UCHAR)srbSeqIncremented;
			completionSrb->TimeOutValue = 20;
			completionSrb->SrbStatus = SRB_STATUS_SUCCESS;

			//
			//	Set completion data for the completion IRP.
			//
			completionData->HwDeviceExtension = HwDeviceExtension;
			completionData->CompletionSrb = completionSrb;
			completionData->ShippedCcb = Ccb;
			completionData->ShippedCcbAllocatedFromPool = LSCcbIsFlagOn(Ccb, CCB_FLAG_ALLOCATED);

Out:
			KDPrint(4,("Before Completion\n"));
			IoSetCompletionRoutine(	pCompletionIrp,
									CompletionIrpCompletionRoutine,
									completionData,
									TRUE, TRUE, TRUE);
			ASSERT(HwDeviceExtension->RequestExecuting != 0);

#if 0
			{
				LARGE_INTEGER	interval;
				ULONG			SrbTimeout;
				static			DebugCount = 0;

				DebugCount ++;
				SrbTimeout = ((PSCSI_REQUEST_BLOCK)(Ccb->Srb))->TimeOutValue;
				if(	SrbTimeout>9 &&
					(DebugCount%1000) == 0
					) {
					KDPrint(1,("Experiment!!!!!!! Delay completion. SrbTimeout:%d\n", SrbTimeout));
					interval.QuadPart = - (INT64)SrbTimeout * 11 * 1000000;
					KeDelayExecutionThread(KernelMode, FALSE, &interval);
				}
			}
#endif
			//
			//	call Scsiport FDO.
			//
			ntStatus = IoCallDriver(pDeviceObject, pCompletionIrp);
			ASSERT(NT_SUCCESS(ntStatus));
			if(ntStatus!= STATUS_SUCCESS && ntStatus!= STATUS_PENDING) {
				KDPrint(1,("ntStatus = 0x%x\n", ntStatus));

				SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_COMPIRP_FAIL, EVTLOG_FAIL_COMPLIRP);

				KDPrint(1,("IoCallDriver() error. CCB(%p) and SRB(%p) is going to the timer."
					" CcbStatus:%x CcbFlag:%x\n", Ccb, Ccb->Srb, Ccb->CcbStatus, Ccb->Flags));

				InitializeListHead(&Ccb->ListEntry);
				LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
				ExInterlockedInsertTailList(
							&HwDeviceExtension->CcbTimerCompletionList,
							&Ccb->ListEntry,
							&HwDeviceExtension->CcbTimerCompletionListSpinLock
						);
			}
		} else {
			KDPrint(1,("CCB(%p) and SRB(%p) is going to the timer."
				" CcbStatus:%x CcbFlag:%x\n", Ccb, Ccb->Srb, Ccb->CcbStatus, Ccb->Flags));

			InitializeListHead(&Ccb->ListEntry);
			LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
			ExInterlockedInsertTailList(
					&HwDeviceExtension->CcbTimerCompletionList,
					&Ccb->ListEntry,
					&HwDeviceExtension->CcbTimerCompletionListSpinLock
				);
		}
	}

	return return_status;
}

