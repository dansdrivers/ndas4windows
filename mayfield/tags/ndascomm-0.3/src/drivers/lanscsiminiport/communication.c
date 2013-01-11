#include "port.h"
#include <ntddk.h>
#include <scsi.h>
#include <stdio.h>
#include <stdarg.h>
#include <initguid.h>
#include "KDebug.h"
#include "LanscsiMiniport.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSMP_Comm"


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
	)
{
	/* DeviceObject which is one of argument is always NULL. */
	PADAPTER_EXTENSION		deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);

	ASSERT(deviceExtension->DeviceObject);

	return deviceExtension->DeviceObject;
}


//////////////////////////////////////////////////////////////////////////
//
//	I/o control to LanscsiBus 
//

//
//	gerneral ioctl to LanscsiBus
//
NTSTATUS
IoctlToLanscsiBus(
	IN ULONG  IoControlCode,
    IN PVOID  InputBuffer  OPTIONAL,
    IN ULONG  InputBufferLength,
    OUT PVOID  OutputBuffer  OPTIONAL,
    IN ULONG  OutputBufferLength,
	OUT PLONG	BufferNeeded
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
		&GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS,
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
		*BufferNeeded = ioStatus.Information;

	ExFreePool(symbolicLinkList);
	ObDereferenceObject(fileObject);
	
	KDPrint(1,("Done...ioStatus.Status = 0x%08x Information = %d\n", ioStatus.Status, ioStatus.Information));
	
	return ntStatus;
}

VOID
IoctlToLanscsiBus_Worker(
		IN PDEVICE_OBJECT		DeviceObject,
		IN PLSMP_WORKITEM_CTX	WorkitemCtx
	) {
	PKEVENT	AlarmEvent;

	AlarmEvent = (PKEVENT)WorkitemCtx->Arg3;

	UNREFERENCED_PARAMETER(DeviceObject);

	IoctlToLanscsiBus(
				(ULONG)WorkitemCtx->Ccb,
				WorkitemCtx->Arg1,
				(ULONG)WorkitemCtx->Arg2,
				NULL,
				0,
				NULL
			);

	if(AlarmEvent)
		KeSetEvent(AlarmEvent, IO_NO_INCREMENT, FALSE);

	if(WorkitemCtx->Arg2 && WorkitemCtx->Arg1)
		ExFreePoolWithTag(WorkitemCtx->Arg1, LSMP_PTAG_IOCTL);
}

NTSTATUS
IoctlToLanscsiBusByWorker(
	IN PDEVICE_OBJECT	WorkerDeviceObject,
	IN ULONG			IoControlCode,
    IN PVOID			InputBuffer  OPTIONAL,
    IN ULONG			InputBufferLength,
	IN PKEVENT			AlarmEvent
) {
	LSMP_WORKITEM_CTX	WorkitemCtx;
	NTSTATUS			status;

	LSMP_INIT_WORKITEMCTX(&WorkitemCtx, IoctlToLanscsiBus_Worker, (PCCB)IoControlCode, InputBuffer, (PVOID)InputBufferLength, AlarmEvent);
	status = LSMP_QueueMiniportWorker(WorkerDeviceObject, &WorkitemCtx);
	if(!NT_SUCCESS(status)) {
		if(InputBufferLength && InputBuffer)
			ExFreePoolWithTag(InputBuffer, LSMP_PTAG_IOCTL);
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
	BUSENUM_SETPDOINFO	BusSet;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	//
	//	Update Status in LanscsiBus
	//
	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = AdapterStatus;
	BusSet.DesiredAccess = BusSet.GrantedAccess = 0;
	return IoctlToLanscsiBus(
			IOCTL_LANSCSI_SETPDOINFO,
			&BusSet,
			sizeof(BUSENUM_SETPDOINFO),
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
	BUSENUM_QUERY_INFORMATION		BusQuery;
	PBUSENUM_INFORMATION			BusInfo;
	LONG							BufferNeeded;

	KDPrint(1,("SystemIoBusNumber:%d\n", SystemIoBusNumber));
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	BusQuery.Size = sizeof(BUSENUM_QUERY_INFORMATION);
	BusQuery.InfoClass = INFORMATION_PDOENUM;
	BusQuery.SlotNo = SystemIoBusNumber;

	//
	//	Get a buffer length needed.
	//
	status = IoctlToLanscsiBus(
						IOCTL_BUSENUM_QUERY_INFORMATION,
						&BusQuery,
						sizeof(BUSENUM_QUERY_INFORMATION),
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
	BusInfo= (PBUSENUM_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, BufferNeeded, LSMP_PTAG_IOCTL);
	status = IoctlToLanscsiBus(
						IOCTL_BUSENUM_QUERY_INFORMATION,
						&BusQuery,
						sizeof(BUSENUM_QUERY_INFORMATION),
						BusInfo,
						BufferNeeded,
						&BufferNeeded
					);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(BusInfo, LSMP_PTAG_IOCTL);
		return SP_RETURN_NOT_FOUND;
	}

	ASSERT(BusInfo->PdoEnumInfo.DisconEventToService);
	ASSERT(BusInfo->PdoEnumInfo.AlarmEventToService);
	HwDeviceExtension->MaxBlocksPerRequest	= BusInfo->PdoEnumInfo.MaxBlocksPerRequest;
	HwDeviceExtension->DisconEventToService	= BusInfo->PdoEnumInfo.DisconEventToService;
	HwDeviceExtension->AlarmEventToService	= BusInfo->PdoEnumInfo.AlarmEventToService;
	HwDeviceExtension->EnumFlags			= BusInfo->PdoEnumInfo.Flags;
	*AddDevInfoFlags						= BusInfo->PdoEnumInfo.Flags;

	KDPrint(1,("MaxBlocksPerRequest:%d DisconEventToService:%p AlarmEventToService:%p\n",
						BusInfo->PdoEnumInfo.MaxBlocksPerRequest,
						BusInfo->PdoEnumInfo.DisconEventToService,
						BusInfo->PdoEnumInfo.AlarmEventToService
		));

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
		lurDesc = (PLURELATION_DESC)ExAllocatePoolWithTag(NonPagedPool, LurDescLen, LSMP_PTAG_ENUMINFO);
		if(lurDesc == NULL) {
			ExFreePoolWithTag(BusInfo,LSMP_PTAG_IOCTL);
			return SP_RETURN_NOT_FOUND;
		}

		RtlCopyMemory(lurDesc, &BusInfo->PdoEnumInfo.AddDevInfo, LurDescLen);
		*AddDevInfo = lurDesc;
		*AddDevInfoLength = LurDescLen;
	} else {
		LONG							addTargetDataLength;
		PLANSCSI_ADD_TARGET_DATA		addTargetData;
		PLANSCSI_ADD_TARGET_DATA		addTargetDataOrig;

		addTargetDataOrig = (PLANSCSI_ADD_TARGET_DATA)BusInfo->PdoEnumInfo.AddDevInfo;
		addTargetDataLength = sizeof(LANSCSI_ADD_TARGET_DATA) +
								( addTargetDataOrig->ulNumberOfUnitDiskList - 1) *
								sizeof(LSBUS_UNITDISK);
		//
		//	Verify sanity
		//
		if(addTargetDataOrig->CntEcrKeyLength > NDAS_CONTENTENCRYPT_KEY_LENGTH) {
			KDPrint(1,("AddTargetData has invalid key length: %d\n", addTargetDataOrig->CntEcrKeyLength));
			return SP_RETURN_NOT_FOUND;
		}

		//
		//	Allocate pool for the ADD_TARGET_DATA.
		//	copy LANSCSI_ADD_TARGET_DATA
		//
		addTargetData = (PLANSCSI_ADD_TARGET_DATA)ExAllocatePoolWithTag(NonPagedPool, addTargetDataLength, LSMP_PTAG_ENUMINFO);
		if(addTargetData == NULL) {
			ExFreePoolWithTag(BusInfo,LSMP_PTAG_IOCTL);
			return SP_RETURN_NOT_FOUND;
		}

		RtlCopyMemory(addTargetData, &BusInfo->PdoEnumInfo.AddDevInfo, addTargetDataLength);
		*AddDevInfo = addTargetData;
		*AddDevInfoLength = addTargetDataLength;
	}

	ExFreePoolWithTag(BusInfo,LSMP_PTAG_IOCTL);
	return SP_RETURN_FOUND;
}


VOID
UpdatePdoInfoInLSBus(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						AdapterStatus
	) {
	PBUSENUM_SETPDOINFO			BusSet;
	UINT32						DesiredAccess;
	UINT32						GrantedAccess;

	ASSERT(HwDeviceExtension);

	//
	//	Query to the LUR
	//
	if(HwDeviceExtension->LURs[0]) {
		KDPrint(3,("going to default LuExtention 0.\n"));
		DesiredAccess = HwDeviceExtension->LURs[0]->DesiredAccess;
		GrantedAccess = HwDeviceExtension->LURs[0]->GrantedAccess;
	} else {
		KDPrint(1,("No LUR available..\n"));
		DesiredAccess = GrantedAccess = 0;
		return;
	}

	//
	//	Send to LSBus
	//
	BusSet = (PBUSENUM_SETPDOINFO)ExAllocatePoolWithTag(NonPagedPool, sizeof(BUSENUM_SETPDOINFO), LSMP_PTAG_IOCTL);
	if(BusSet == NULL) {
		return;
	}

	BusSet->Size			= sizeof(BUSENUM_SETPDOINFO);
	BusSet->SlotNo			= HwDeviceExtension->SlotNumber;
	BusSet->AdapterStatus	= AdapterStatus;
	BusSet->DesiredAccess	= DesiredAccess;
	BusSet->GrantedAccess	= GrantedAccess;

	if(KeGetCurrentIrql() == PASSIVE_LEVEL) {
		IoctlToLanscsiBus(
						IOCTL_LANSCSI_SETPDOINFO,
						BusSet,
						sizeof(BUSENUM_SETPDOINFO),
						NULL,
						0,
						NULL);
		if(HwDeviceExtension->AlarmEventToService) {
			KeSetEvent(HwDeviceExtension->AlarmEventToService, IO_NO_INCREMENT, FALSE);
		}
		ExFreePoolWithTag(BusSet, LSMP_PTAG_IOCTL);

	} else {

		//
		//	IoctlToLanscsiBus_Worker() will free memory of BusSet.
		//
		IoctlToLanscsiBusByWorker(
						HwDeviceExtension->ScsiportFdoObject,
						IOCTL_LANSCSI_SETPDOINFO,
						BusSet,
						sizeof(BUSENUM_SETPDOINFO),
						HwDeviceExtension->AlarmEventToService
					);
	}
}




//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi miniport IO completion
//

static int MiniCounter;

VOID
MiniTimer(
    IN PMINIPORT_DEVICE_EXTENSION HwDeviceExtension
    )
{
	BOOLEAN	BusChanged;

	MiniCounter ++;
	BusChanged = FALSE;

	do {
		PLIST_ENTRY			listEntry;
		PCCB				ccb;
		PSCSI_REQUEST_BLOCK	srb;

		// Get Completed CCB
		listEntry = ExInterlockedRemoveHeadList(
							&HwDeviceExtension->CcbCompletionList,
							&HwDeviceExtension->CcbCompletionListSpinLock
						);

		if(listEntry == NULL)
			break;

		ccb = CONTAINING_RECORD(listEntry, CCB, ListEntry);
		srb = ccb->Srb;
		ASSERT(srb);
		ASSERT(LSCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE));
		KDPrint(1,("completed SRB:%p SCSIOP:%x\n", srb, srb->Cdb[0]));

		if(LSCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_BUSRESET_REQUIRED)) {
			BusChanged = TRUE;
		}

		if(	srb && IS_CCB_VAILD_SEQID(HwDeviceExtension, ccb)) {

			InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
			ASSERT(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
			ScsiPortNotification(
					RequestComplete,
					HwDeviceExtension,
					srb
				);
		}
#if DBG
		if(!srb)
			KDPrint(1,("CCB:%p doesn't have SRB.\n", srb));
		if(HwDeviceExtension->CcbSeqIdStamp != ccb->CcbSeqId)
			KDPrint(1,("CCB:%p has a old ID:%lu. CurrentID:%lu\n", ccb, ccb->CcbSeqId, HwDeviceExtension->CcbSeqIdStamp));
#endif

		//
		//	Free a CCB
		//
		LSCcbPostCompleteCcb(ccb);

	} while(1);


	//
	//	If any CCb has a BusChanged request,
	//	Notify it to ScsiPort.
	//
	if(BusChanged) {
		KDPrint(1,("Bus change detected. RequestExecuting = %d\n", HwDeviceExtension->RequestExecuting));
		ScsiPortNotification(
			BusChangeDetected,
			HwDeviceExtension,
			NULL
			);
	}

	//
	//	Reschedule the timer.
	//
	if((MiniCounter % 1000) == 0)
		KDPrint(4,("RequestExecuting = %d\n", HwDeviceExtension->RequestExecuting));

	if(HwDeviceExtension->RequestExecuting != 0) {
		ScsiPortNotification(
			RequestTimerCall,
			HwDeviceExtension,
			MiniTimer,
			1
			);
	} else {
		HwDeviceExtension->TimerOn = FALSE;

		if(ADAPTER_ISSTATUS(HwDeviceExtension,ADAPTER_STATUS_RUNNING))
			ScsiPortNotification(
				RequestTimerCall,
				HwDeviceExtension,
				MiniTimer,
				1 //1000
				);
		
	}

	ScsiPortNotification(
		NextRequest,
		HwDeviceExtension,
		NULL
		);

	return;
}



typedef struct _COMPLETION_DATA {

    PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension;
	PCCB						ShippedCcb;
	BOOLEAN						ShippedCcbAllocatedFromPool;
	PSCSI_REQUEST_BLOCK			CompletionSrb;

} COMPLETION_DATA, *PCOMPLETION_DATA;


//
//	Completion routine for IRP carrying SRB.
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


	KDPrint(3,("Entered\n"));
	UNREFERENCED_PARAMETER(DeviceObject);

	if(completionSrb->DataBuffer) 
	{
		PSCSI_REQUEST_BLOCK	shippedSrb = (PSCSI_REQUEST_BLOCK)(completionSrb->DataBuffer);

		KDPrint(1, ("Unexpected IRP completion!!!\n"));
		ASSERT(shippedCcb);

		InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
		InitializeListHead(&shippedCcb->ListEntry);
		LSCcbSetStatusFlag(shippedCcb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		ExInterlockedInsertTailList(
				&HwDeviceExtension->CcbCompletionList,
				&shippedCcb->ListEntry,
				&HwDeviceExtension->CcbCompletionListSpinLock
				);

		completionSrb->DataBuffer = NULL;
	} else {
		//
		//	Free the CCB if it is not going to the timer completion routine
		//	and allocated from the system pool by lanscsiminiport.
		//
		if(Context->ShippedCcbAllocatedFromPool)
			LSCcbPostCompleteCcb(shippedCcb);
	}

	// Free resources
	ExFreePoolWithTag(completionSrb, LSMP_PTAG_SRB);
	ExFreePoolWithTag(Context, LSMP_PTAG_CMPDATA);
	IoFreeIrp(Irp);

	return STATUS_MORE_PROCESSING_REQUIRED;
}



#define BUSRESET_EFFECT_TICKCOUNT		(400)

#define SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVT_ID)	\
	ScsiPortLogError(							\
	HwDeviceExtension,						\
	srb,									\
	srb->PathId,							\
	srb->TargetId,							\
	srb->Lun,								\
	SP_INTERNAL_ADAPTER_ERROR,				\
	EVTLOG_UNIQUEID(EVTLOG_MODULE_COMPLETION, EVT_ID, 0)	\
	)

NTSTATUS
LanscsiMiniportCompletion(
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

	KDPrint(3,("RequestExecuting = %d\n", HwDeviceExtension->RequestExecuting));

	srb = Ccb->Srb;
	if(!srb) {
		KDPrint(1,("Ccb:%p CcbStatus %d. No srb assigned.\n", Ccb, Ccb->CcbStatus));
		ASSERT(srb);
		return STATUS_SUCCESS;
	}
 
	//
	//	LanscsiMiniport completion routine will do post operation to complete CCBs.
	//
	return_status = STATUS_MORE_PROCESSING_REQUIRED;
	srbSeqIncremented = InterlockedIncrement(&SrbSeq);

	//
	// Update Adapter status flag
	//

	NeedToUpdatePdoInfoInLSBus = FALSE;

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	AdapterStatusBefore = HwDeviceExtension->AdapterStatus;

	//	Check to see if the associate member is in error.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_LURN_STOP))
	{
		ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_MEMBER_FAULT);
	}
	else
	{
		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_MEMBER_FAULT);
	}

	//	Check reconnecting process.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RECONNECTING))
	{
		ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECONNECT_PENDING);
	}
	else
	{
		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECONNECT_PENDING);
	}

	//	Check recovering process.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RECOVERING))
	{
		ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECOVERING);
	}
	else
	{
		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RECOVERING);
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
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_MEMBER_IN_ERROR);
			KDPrint(1,("Ccb:%p CcbStatus %d. Set member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & ADAPTER_STATUSFLAG_MEMBER_FAULT) &&
			!(AdapterStatus & ADAPTER_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// ADAPTER_STATUSFLAG_MEMBER_FAULT off
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_MEMBER_RECOVERED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Reset member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECONNECT_PENDING) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// ADAPTER_STATUSFLAG_RECONNECT_PENDING on
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_START_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Start reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECONNECT_PENDING) &&
			!(AdapterStatus & ADAPTER_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// ADAPTER_STATUSFLAG_RECONNECT_PENDING off
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_END_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Finish reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECOVERING) &&
			(AdapterStatus & ADAPTER_STATUSFLAG_RECOVERING)
			)
		{
			// ADAPTER_STATUSFLAG_RECONNECT_PENDING on
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_START_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & ADAPTER_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & ADAPTER_STATUSFLAG_RECOVERING)
			)
		{
			// ADAPTER_STATUSFLAG_RECONNECT_PENDING off
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_END_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Ended recovering\n", Ccb, Ccb->CcbStatus));
		}
		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	//	If CCB_OPCODE_UPDATE is successful, update adapter status in LanscsiBus
	//
	if(Ccb->OperationCode == CCB_OPCODE_UPDATE) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_SUCCEED_UPGRADE);
		} else {
			SCSI_PORT_LOG_ERROR_MODULE_COMPLETION(EVTLOG_FAIL_UPGRADE);
		}
		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	if(NeedToUpdatePdoInfoInLSBus)
	{
		KDPrint(0,("<<<<<<<<<<<<<<<< %08lx -> %08lx ADAPTER STATUS CHANGED >>>>>>>>>>>>>>>>\n", AdapterStatusBefore, AdapterStatus));	\
		UpdatePdoInfoInLSBus(HwDeviceExtension, HwDeviceExtension->AdapterStatus);
	}

	KDPrint(3,("CcbStatus %d\n", Ccb->CcbStatus));

	//
	//	Translate CcbStatus to SrbStatus
	//
	switch(Ccb->CcbStatus) {
		case CCB_STATUS_SUCCESS:

			srb->SrbStatus = SRB_STATUS_SUCCESS;
			srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_NOT_EXIST:

			srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_INVALID_COMMAND:

			srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMAND_FAILED:

			srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
			srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			srb->DataTransferLength -= Ccb->ResidualDataLength;
			break;
//	Added by ILGU HONG 2004_07_05

		case CCB_STATUS_COMMMAND_DONE_SENSE2:
			srb->SrbStatus =  SRB_STATUS_BUSY  | SRB_STATUS_AUTOSENSE_VALID ;
			srb->DataTransferLength = 0;
			//srb->DataTransferLength -= Ccb->ResidualDataLength;
			srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMMAND_DONE_SENSE:
			srb->SrbStatus =  SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID ;
			srb->DataTransferLength = 0;
			//srb->DataTransferLength -= Ccb->ResidualDataLength;
			srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			break;
//	Added by ILGU HONG 2004_07_05 end
		case CCB_STATUS_RESET:

			srb->SrbStatus = SRB_STATUS_BUS_RESET;
			srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMUNICATION_ERROR:
			{
				PSENSE_DATA	senseData;

				srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
				srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
				srb->DataTransferLength = 0;
				
				senseData = srb->SenseInfoBuffer;
				
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
			srb->SrbStatus = SRB_STATUS_BUSY;
			srb->ScsiStatus = SCSISTAT_GOOD;
			KDPrint(1,("CCB_STATUS_BUSY\n"));			
			break;

			//
			//	Stop one LUR
			//
		case CCB_STATUS_STOP: {
			KDPrint(1,("CCB_STATUS_STOP. Stopping!\n"));

			srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			srb->ScsiStatus = SCSISTAT_GOOD;

			MiniStopAdapter(HwDeviceExtension, TRUE);

			break;
		}
		default:
			// Error in Connection...
			// CCB_STATUS_UNKNOWN_STATUS, CCB_STATUS_RESET, and so on.
			srb->SrbStatus = SRB_STATUS_ERROR;
			srb->ScsiStatus = SCSISTAT_GOOD;
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
				&HwDeviceExtension->CcbCompletionList,
				&Ccb->ListEntry,
				&HwDeviceExtension->CcbCompletionListSpinLock
			);

		((PSCSI_REQUEST_BLOCK)abortCcb->Srb)->SrbStatus = SRB_STATUS_ABORTED;
		LSCcbSetStatusFlag(abortCcb, CCBSTATUS_FLAG_TIMER_COMPLETE);
		InitializeListHead(&abortCcb->ListEntry);
		ExInterlockedInsertTailList(
				&HwDeviceExtension->CcbCompletionList,
				&abortCcb->ListEntry,
				&HwDeviceExtension->CcbCompletionListSpinLock
			);

	} else {

		//
		// Make Complete IRP and Send it.
		//
		//
		//	In case of HostStatus == CCB_STATUS_SUCCESS_TIMER, CCB will go to the timer to complete.
		//
		if(		(Ccb->CcbStatus == CCB_STATUS_SUCCESS || Ccb->CcbStatus == CCB_STATUS_INVALID_COMMAND) &&
				!LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE) &&
				!LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_BUSRESET_REQUIRED) 
			)
		{
			PDEVICE_OBJECT		pDeviceObject = HwDeviceExtension->ScsiportFdoObject;
			PIRP				pCompletionIrp = NULL;
			PIO_STACK_LOCATION	pIoStack;
			NTSTATUS			ntStatus;
			PSCSI_REQUEST_BLOCK	completionSrb = NULL;
			PCOMPLETION_DATA	completionData = NULL;

			completionSrb = ExAllocatePoolWithTag(NonPagedPool, sizeof(SCSI_REQUEST_BLOCK), LSMP_PTAG_SRB);
			if(completionSrb == NULL)
				goto Out;

			RtlZeroMemory(
					completionSrb,
					sizeof(SCSI_REQUEST_BLOCK)
				);

			// Build New IRP.
			pCompletionIrp = IoAllocateIrp((CCHAR)(pDeviceObject->StackSize + 1), FALSE);
			if(pCompletionIrp == NULL) {
				ExFreePoolWithTag(completionSrb, LSMP_PTAG_SRB);
				goto Out;
			}

			completionData = ExAllocatePoolWithTag(NonPagedPool, sizeof(COMPLETION_DATA), LSMP_PTAG_CMPDATA);
			if(completionData == NULL) {
				ExFreePoolWithTag(completionSrb, LSMP_PTAG_SRB);
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

			if(pCompletionIrp) {

				IoSetCompletionRoutine(pCompletionIrp, CompletionIrpCompletionRoutine, completionData, TRUE, TRUE, TRUE);
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
					ScsiPortLogError(
						HwDeviceExtension,
						srb,
						srb->PathId,
						srb->TargetId,
						srb->Lun,
						SP_INTERNAL_ADAPTER_ERROR,
						EVTLOG_UNIQUEID(EVTLOG_MODULE_COMPLETION, EVTLOG_FAIL_COMPLIRP, 0)
						);

					KDPrint(1,("IoCallDriver() error. CCB(%p) and SRB(%p) is going to the timer. CcbStatus:%x CcbFlag:%x\n", Ccb, Ccb->Srb, Ccb->CcbStatus, Ccb->Flags));

					InitializeListHead(&Ccb->ListEntry);
					LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
					ExInterlockedInsertTailList(
								&HwDeviceExtension->CcbCompletionList,
								&Ccb->ListEntry,
								&HwDeviceExtension->CcbCompletionListSpinLock
							);
				}
			}else {
				KDPrint(1,("CompletionIRP NULL. CCB(%p) and SRB(%p) is going to the timer. CcbStatus:%x CcbFlag:%x\n", Ccb, Ccb->Srb, Ccb->CcbStatus, Ccb->Flags));

				InitializeListHead(&Ccb->ListEntry);
				LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
				ExInterlockedInsertTailList(
					&HwDeviceExtension->CcbCompletionList,
					&Ccb->ListEntry,
					&HwDeviceExtension->CcbCompletionListSpinLock
				);

			}
		} else {
			KDPrint(1,("CCB(%p) and SRB(%p) is going to the timer. CcbStatus:%x CcbFlag:%x\n", Ccb, Ccb->Srb, Ccb->CcbStatus, Ccb->Flags));

			InitializeListHead(&Ccb->ListEntry);
			LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
			ExInterlockedInsertTailList(
					&HwDeviceExtension->CcbCompletionList,
					&Ccb->ListEntry,
					&HwDeviceExtension->CcbCompletionListSpinLock
				);
		}
	}

	return return_status;
}

