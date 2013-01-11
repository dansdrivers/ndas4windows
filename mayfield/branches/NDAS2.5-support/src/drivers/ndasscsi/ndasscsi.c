#include "ndasscsi.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NDASSCSI"

//#define DISABLE_DRAID	1

VOID
MiniUnload(
	IN PDRIVER_OBJECT DriverObject
);

VOID
MiniQueueStopWorkItem(
	PNDSC_GLOBALS	NdscGlobals
);

VOID
MiniDriverThreadProc(
	PVOID StartContext
);

BOOLEAN
MiniHwInitialize(
	IN PVOID	HwDeviceExtension
	);

ULONG
MiniFindAdapter(
	IN		PVOID							HwDeviceExtension,
	IN		PVOID							Context,
	IN		PVOID							BusInformation,
	IN		PCHAR							ArgumentString,
	IN OUT	PPORT_CONFIGURATION_INFORMATION	ConfigInfo,
	OUT		PBOOLEAN						Again
	);

BOOLEAN
MiniStartIo(
	IN PVOID				HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK	Srb
	);

BOOLEAN
MiniResetBus(
	IN PVOID	HwDeviceExtension,
	IN ULONG	PathId
	);

NTSTATUS
MiniResetBusBySrb(
	    IN PVOID	HwDeviceExtension,
		IN ULONG	PathId
	);

SCSI_ADAPTER_CONTROL_STATUS
MiniAdapterControl(
    IN PVOID						HwDeviceExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE	ControlType,
    IN PVOID						Parameters
    );

VOID
LSMP_StopWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PNDSC_WORKITEM			LSMPWorkItemCtx
	);
VOID
LSMP_RestartWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PNDSC_WORKITEM			LSMPWorkItemCtx
	);

NTSTATUS
SendCcbToAllLURsSync(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode
	);

NTSTATUS
SendCcbToAllLURs(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode,
		CCB_COMPLETION_ROUTINE		CcbCompletionRoutine,
		PVOID						CompletionContext

	);


//////////////////////////////////////////////////////////////////////////
//
//	Driver-wide variables
//
NDSC_GLOBALS
_NdscGlobals;


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi Miniport Driver routines
//
ULONG
DriverEntry(
    IN PDRIVER_OBJECT	DriverObject,
    IN PVOID			RegistryPath
    )
{
	HW_INITIALIZATION_DATA	hwInitializationData;
	ULONG					isaStatus;

	KDPrint(1,("%s %s Flags:%08lx\n", __DATE__, __TIME__, DriverObject->Flags));

	// Get OS Version.
    _NdscGlobals.CheckedVersion = PsGetVersion(
        &_NdscGlobals.MajorVersion,
        &_NdscGlobals.MinorVersion,
        &_NdscGlobals.BuildNumber,
        NULL
        );

    KDPrint(1,("Major Ver %d, Minor Ver %d, Build %d Checked:%d\n",
										_NdscGlobals.MajorVersion,
										_NdscGlobals.MinorVersion,
										_NdscGlobals.BuildNumber,
										_NdscGlobals.CheckedVersion
								));
	_NdscGlobals.DriverObject = DriverObject;


	//
	//	initialize Scsi miniport
	//
	RtlZeroMemory(&hwInitializationData, sizeof(HW_INITIALIZATION_DATA));

	hwInitializationData.HwInitializationDataSize = sizeof(hwInitializationData); 

	hwInitializationData.HwInitialize	= MiniHwInitialize; 
	hwInitializationData.HwStartIo		= MiniStartIo; 
	hwInitializationData.HwInterrupt	= NULL; 
	hwInitializationData.HwFindAdapter	= MiniFindAdapter; 
	hwInitializationData.HwResetBus		= MiniResetBus; 
	hwInitializationData.HwDmaStarted	= NULL; 
	hwInitializationData.HwAdapterState = NULL; 
	hwInitializationData.HwAdapterControl			= MiniAdapterControl;

	hwInitializationData.DeviceExtensionSize		= (INT32)sizeof(MINIPORT_DEVICE_EXTENSION); 
	hwInitializationData.SpecificLuExtensionSize	= sizeof(MINIPORT_LU_EXTENSION);
	hwInitializationData.SrbExtensionSize			= 0;
	hwInitializationData.NumberOfAccessRanges		= 1;
	hwInitializationData.Reserved					= 0;
	hwInitializationData.MapBuffers					= TRUE;
	hwInitializationData.NeedPhysicalAddresses		= FALSE;
	hwInitializationData.TaggedQueuing				= FALSE;
	hwInitializationData.AutoRequestSense			= TRUE;
	hwInitializationData.MultipleRequestPerLu		= FALSE;
	hwInitializationData.ReceiveEvent				= FALSE;

	hwInitializationData.VendorIdLength				= 0;
	hwInitializationData.VendorId					= NULL;
	hwInitializationData.ReservedUshort				= 0;
	hwInitializationData.DeviceIdLength				= 0;
	hwInitializationData.DeviceId					= NULL;


	hwInitializationData.AdapterInterfaceType = Isa;
	isaStatus = ScsiPortInitialize(DriverObject, RegistryPath, &hwInitializationData, NULL);
	KDPrint(1,("IsaBus type: NTSTATUS=%08lx.\n", isaStatus));


	while(NT_SUCCESS(isaStatus)) {
		NTSTATUS		status;
		UNICODE_STRING	svcName;
		OBJECT_ATTRIBUTES  objectAttributes;


		//
		//	Override driver unload routine.
		//

		_NdscGlobals.ScsiportUnload = DriverObject->DriverUnload;
		DriverObject->DriverUnload = MiniUnload;


		//
		//	Create Halt event for a driver-dedicated thread
		//

		KeInitializeEvent(&_NdscGlobals.WorkItemQueueEvent, NotificationEvent, FALSE);
		InitializeListHead(&_NdscGlobals.WorkItemQueue);
		KeInitializeSpinLock(&_NdscGlobals.WorkItemQueueSpinlock);


		//
		//	Create a driver-dedicated thread
		//

		InitializeObjectAttributes(
			&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);


		status = PsCreateSystemThread(
			&_NdscGlobals.DriverThreadHandle,
			THREAD_ALL_ACCESS,
			&objectAttributes,
			NULL,
			NULL,
			MiniDriverThreadProc,
			&_NdscGlobals
			);
		if(!NT_SUCCESS(status)) {
			ASSERT(FALSE);
			_NdscGlobals.DriverThreadHandle = NULL;
			_NdscGlobals.DriverThreadObject = NULL;
			break;
		}

		status = ObReferenceObjectByHandle(
				_NdscGlobals.DriverThreadHandle,
				FILE_READ_DATA,
				NULL,
				KernelMode,
				&_NdscGlobals.DriverThreadObject,
				NULL
			);
		if(!NT_SUCCESS(status)) {
			ASSERT(FALSE);

			MiniQueueStopWorkItem(&_NdscGlobals);

			_NdscGlobals.DriverThreadHandle = NULL;
			_NdscGlobals.DriverThreadObject = NULL;
			break;
		}
#ifndef DISABLE_DRAID
		//
		// Init DRAID related stuffs.
		//
		status = DraidStart(&_NdscGlobals.DraidGlobal);
		if (!NT_SUCCESS(status)) {
			ASSERT(FALSE);

			MiniQueueStopWorkItem(&_NdscGlobals);

			_NdscGlobals.DriverThreadHandle = NULL;
			_NdscGlobals.DriverThreadObject = NULL;
			break;
		}
#endif
		//
		//	Register TDI PnPPowerHandler
		//

		RtlInitUnicodeString(&svcName,NDASSCSI_SVCNAME);
#ifdef DISABLE_DRAID
		status = LsuRegisterTdiPnPHandler(&svcName, 
			ClientPnPBindingChange_Delay,
			NULL,
			NULL,
			ClientPnPPowerChange_Delay, &_NdscGlobals.TdiPnP);
#else
		status = LsuRegisterTdiPnPHandler(&svcName, 
			ClientPnPBindingChange_Delay,
			DraidPnpAddAddressHandler,	// Right now, only DRAID uses Add/Del Address handler.
			DraidPnpDelAddressHandler,
			ClientPnPPowerChange_Delay, &_NdscGlobals.TdiPnP);
#endif
		ASSERT(NT_SUCCESS(status));
		break;
	}

	return isaStatus;
}


VOID
MiniUnload(
	IN PDRIVER_OBJECT DriverObject
){
	NTSTATUS	status;
	KDPrint(1,("entered. DriverObject=%p\n", DriverObject));

	LsuDeregisterTdiPnPHandlers(_NdscGlobals.TdiPnP);

	if(_NdscGlobals.DriverThreadHandle && _NdscGlobals.DriverThreadObject) {
		MiniQueueStopWorkItem(&_NdscGlobals);

		status = KeWaitForSingleObject(
					_NdscGlobals.DriverThreadObject,
					Executive,
					KernelMode,
					FALSE,
					NULL);
		ASSERT(NT_SUCCESS(status));

		ObDereferenceObject(_NdscGlobals.DriverThreadObject);
	}

#ifndef DISABLE_DRAID
	DraidStop(&_NdscGlobals.DraidGlobal);
#endif
	_NdscGlobals.ScsiportUnload(DriverObject);
}

//////////////////////////////////////////////////////////////////////////
//
//	Driver-dedicated work item manipulation
//

NTSTATUS
MiniQueueWorkItem(
		IN PNDSC_GLOBALS		NdscGlobals,
		IN PDEVICE_OBJECT		DeviceObject,
		IN PNDSC_WORKITEM_INIT	NdscWorkItemInit
	) {

	PNDSC_WORKITEM	workItem;

	KDPrint(2,("entered.\n"));


	//
	//	Allocate work item
	//

	workItem = (PNDSC_WORKITEM)ExAllocatePoolWithTag(NonPagedPool, sizeof(NDSC_WORKITEM), LSMP_PTAG_WORKITEM);
	if(!workItem) {
		ASSERT(FALSE);
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Init private fields
	//

	InitializeListHead(&workItem->WorkItemList);
	workItem->NdscGlobals = NdscGlobals;
	workItem->DeviceObject = DeviceObject;

	// Termination field only set by the driver itself
	workItem->TerminateWorkerThread = NdscWorkItemInit->TerminateWorkerThread;

	//
	//	Retrieve work item initializing values
	//

	workItem->WorkitemRoutine = NdscWorkItemInit->WorkitemRoutine;
	workItem->Ccb = NdscWorkItemInit->Ccb;
	workItem->Arg1 = NdscWorkItemInit->Arg1;
	workItem->Arg2 = NdscWorkItemInit->Arg2;
	workItem->Arg3 = NdscWorkItemInit->Arg3;


	//
	//	Insert the work item to the queue, and notify to the worker thread.
	//

	ExInterlockedInsertTailList(
		&NdscGlobals->WorkItemQueue,
		&workItem->WorkItemList,
		&NdscGlobals->WorkItemQueueSpinlock
		);
	InterlockedIncrement(&NdscGlobals->WorkItemCnt);
	KeSetEvent(&NdscGlobals->WorkItemQueueEvent, IO_DISK_INCREMENT, FALSE);

	KDPrint(2,("queued work item!!!!!!\n"));

	return STATUS_SUCCESS;
}


VOID
MiniQueueStopWorkItem(
	PNDSC_GLOBALS	NdscGlobals
){
	NDSC_WORKITEM_INIT	ndscWorkItemInit;

	NDSC_INIT_WORKITEM(&ndscWorkItemInit, NULL, NULL, NULL, NULL, NULL);
	ndscWorkItemInit.TerminateWorkerThread = TRUE;

	MiniQueueWorkItem(
			NdscGlobals,
			NULL,
			&ndscWorkItemInit
		);
}

PNDSC_WORKITEM
MiniDequeueWorkItem(
	PNDSC_GLOBALS	NdscGlobals
){
	PLIST_ENTRY		listEntry;
	PNDSC_WORKITEM	workItem;

	listEntry = ExInterlockedRemoveHeadList(
					&NdscGlobals->WorkItemQueue,
					&NdscGlobals->WorkItemQueueSpinlock);

	if(listEntry != NULL) {
		workItem = CONTAINING_RECORD(listEntry, NDSC_WORKITEM, WorkItemList);
		InterlockedDecrement(&NdscGlobals->WorkItemCnt);
	} else {
		workItem = NULL;
	}

	return workItem;
}

VOID
MiniCleanupWorkItemQueue(
	PNDSC_GLOBALS	NdscGlobals
){
	PNDSC_WORKITEM	workItem;

	while(TRUE)	{
		workItem = MiniDequeueWorkItem(NdscGlobals);
		if(workItem == NULL) {
			break;
		}
		ExFreePoolWithTag(workItem, LSMP_PTAG_WORKITEM);
		InterlockedDecrement(&NdscGlobals->WorkItemCnt);
		KDPrint(1, ("Canceled work item %p\n", workItem));
	}
}

//////////////////////////////////////////////////////////////////////////
//
//	Driver-dedicated thread
//

NTSTATUS
MiniExecuteWorkItem(
	PNDSC_GLOBALS	NdscGlobals,
	PBOOLEAN		LoopAgain
) {
	PNDSC_WORKITEM	workItem;
	BOOLEAN			termination;

	while(TRUE)	{
		workItem = MiniDequeueWorkItem(NdscGlobals);
		if(workItem == NULL) {
			break;
		}


		//
		// Call work item's function
		//

		if(workItem->WorkitemRoutine) {
			workItem->WorkitemRoutine(workItem->DeviceObject, workItem);
		}
		termination = workItem->TerminateWorkerThread;


		//
		// Free a work item
		//

		ExFreePoolWithTag(workItem, LSMP_PTAG_WORKITEM);


		//
		// Check to see if this work item indicates termination of the thread.
		//

		if(termination) {
			*LoopAgain = FALSE;
			break;
		}
	}

	return STATUS_SUCCESS;
}

VOID
MiniDriverThreadProc(
	PVOID StartContext
){
	PNDSC_GLOBALS	ndscGlobals = (PNDSC_GLOBALS)StartContext;
	NTSTATUS		status;
	BOOLEAN			loopAgain;


	loopAgain = TRUE;
	while(loopAgain) {
		status = KeWaitForSingleObject(
					&ndscGlobals->WorkItemQueueEvent,
					Executive,
					KernelMode,
					FALSE,
					NULL
			);
		switch(status) {
			case STATUS_SUCCESS:
				KeClearEvent(&ndscGlobals->WorkItemQueueEvent);
				status = MiniExecuteWorkItem(ndscGlobals, &loopAgain);
				ASSERT(status == STATUS_SUCCESS);
				break;
			default:
				loopAgain = FALSE;
		}
	}

	ASSERT(ndscGlobals->WorkItemCnt == 0);
	MiniCleanupWorkItemQueue(
			ndscGlobals
	);

	KDPrint(1,("Terminating the worker thread.\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
}

//////////////////////////////////////////////////////////////////////////
//
//	Miniport callbacks
//
VOID
GetDefaultLurFlags(
	   PLURELATION_DESC	LurDesc,
		PULONG	DefaultLurFlags
	) {

	*DefaultLurFlags = 0;
	if(_NdscGlobals.MajorVersion == NT_MAJOR_VERSION && _NdscGlobals.MinorVersion == W2K_MINOR_VERSION) {
		if(!(LurDesc->AccessRight & GENERIC_WRITE))
			*DefaultLurFlags |= LURFLAG_FAKEWRITE;
	}
	*DefaultLurFlags |= LURFLAG_WRITESHARE_PS;
#if 0
	KDPrint(1, ("Enabling LOCKED_WRITE as default option\n"));
	*DefaultLurFlags |= LURFLAG_LOCKEDWRITE;
#endif	
//	*DefaultLurFlags |= LURFLAG_OOB_SHAREDWRITE;
//	*DefaultLurFlags |= LURFLAG_NDAS_2_0_WRITE_CHECK;  // Read written data for 2.0 chip
	*DefaultLurFlags |= LURFLAG_DYNAMIC_REQUEST_SIZE;
}


//
//	Create LUR object with AddTargetData structure
//

static
NTSTATUS
AddTargetDataDuringFindAdapter(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN LONG							AddTargetDataLength,
	IN PLANSCSI_ADD_TARGET_DATA		AddTargetData
	) {
	LONG						LurDescLength;
	PLURELATION_DESC			LurDesc;
	PLURELATION					Lur;
	NTSTATUS					status;
	LONG						LURCount;
	LONG						ChildLurnCnt;
	UINT32						DefaultLurFlags;

	UNREFERENCED_PARAMETER(AddTargetDataLength);
	//
	//	Allocate LUR Descriptor
	//
	ChildLurnCnt	=	AddTargetData->ulNumberOfUnitDiskList;

	KDPrint(1,("SIZE_OF_LURELATION_DESC() = %d\n", SIZE_OF_LURELATION_DESC()));
	KDPrint(1,("SIZE_OF_LURELATION_NODE_DESC(0) = %d\n", SIZE_OF_LURELATION_NODE_DESC(0)));
	
	switch(AddTargetData->ucTargetType)
	{
	case NDASSCSI_TYPE_DISK_MIRROR:
	case NDASSCSI_TYPE_DISK_AGGREGATION:
	case NDASSCSI_TYPE_DISK_RAID0:
		//	Associate type
		LurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt) + // root
							SIZE_OF_LURELATION_NODE_DESC(0) * (ChildLurnCnt);	// leaves
		break;
	case NDASSCSI_TYPE_DISK_RAID1R:
		LurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt /2) + // root
							SIZE_OF_LURELATION_NODE_DESC(2) * (ChildLurnCnt /2) +// mirrored disks
							SIZE_OF_LURELATION_NODE_DESC(0) * (ChildLurnCnt);	// leaves
		break;
	case NDASSCSI_TYPE_DISK_RAID4R:
		//	Associate type
		LurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt) + // root
							SIZE_OF_LURELATION_NODE_DESC(0) * (ChildLurnCnt);	// leaves
		break;
	case NDASSCSI_TYPE_DISK_NORMAL:
	case NDASSCSI_TYPE_DVD:
	case NDASSCSI_TYPE_VDVD:
	case NDASSCSI_TYPE_MO:
		//	Single type
		LurDescLength	=	sizeof(LURELATION_DESC) - sizeof(LONG);
		break;
	default:
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_INVALID_TARGETTYPE,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_INVALID_TARGETTYPE, AddTargetData->ucTargetType)
			);
		return STATUS_ILLEGAL_INSTRUCTION;
	}

	LurDesc = (PLURELATION_DESC)ExAllocatePoolWithTag(NonPagedPool,  LurDescLength, LSMP_PTAG_IOCTL);
	if(LurDesc == NULL) {
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_MEMALLOC_FAIL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_ALLOC_LURDESC, 0)
			);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Translate AddTargetData to LUR Desc
	//	Pass Adapter max request block as LUR max request blocks 
	//
	status = LurTranslateAddTargetDataToLURDesc(
									AddTargetData,
									HwDeviceExtension->AdapterMaxBlocksPerRequest,
									LurDescLength,
									LurDesc
								);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(LurDesc, LSMP_PTAG_IOCTL);
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_INVAILD_TARGETDATA,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_TRANSLATE, 0)
			);
		return status;
	}

	//
	//	Get default LUR flags.
	//
	GetDefaultLurFlags(LurDesc,&DefaultLurFlags);

	//
	//	Create an LUR with LUR Descriptor
	//	We support only one LUR for now.
	//

	status = LurCreate(	LurDesc,
						DefaultLurFlags,
						&Lur,
						HwDeviceExtension->LURSavedGrantedAccess[0],
						HwDeviceExtension->ScsiportFdoObject,
						LsmpLurnCallback);
	if(NT_SUCCESS(status)) {
		LURCount = InterlockedIncrement(&HwDeviceExtension->LURCount);

		ASSERT(LURCount == 1);

		HwDeviceExtension->LURs[0] = Lur;
	}
#if DBG
	else
	{
		KDPrint(1,("LurCreate Failed %08lx\n", status));
		ASSERT(FALSE);
	}
#endif
	//
	//	Free LUR Descriptor
	//
	ExFreePoolWithTag(LurDesc, LSMP_PTAG_IOCTL);


	return status;
}


//
//	Create LUR object with LUR descriptor
//

static
NTSTATUS
AddLurDescDuringFindAdapter(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN LONG							LurDescLength,
	IN PLURELATION_DESC				LurDesc
	) {
	PLURELATION					Lur;
	NTSTATUS					status;
	LONG						LURCount;
	UINT32						DefaultLurFlags;

	UNREFERENCED_PARAMETER(LurDescLength);


	KDPrint(1,("SIZE_OF_LURELATION_DESC() = %d\n", SIZE_OF_LURELATION_DESC()));
	KDPrint(1,("SIZE_OF_LURELATION_NODE_DESC(0) = %d\n", SIZE_OF_LURELATION_NODE_DESC(0)));


	//
	//	Get default LUR flags.
	//

	GetDefaultLurFlags(LurDesc, &DefaultLurFlags);


	//
	//	Create an LUR with LUR Desc
	//

	status = LurCreate(
					LurDesc,
					DefaultLurFlags,
					&Lur,
					HwDeviceExtension->LURSavedGrantedAccess[0],
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

	return status;
}

//
//	Get PDO and LUR information
//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData
//
//	Parameter:	HwDeviceExtension - the adapter's miniport extension
//				DoNotAdjustAccessRight - Forcely disable automatic access-mode change to NDAS device
//				FirstInstallation - Set if this is the first-time device installation
//

static
ULONG
EnumerateLURFromNDASBUS(
	IN	PMINIPORT_DEVICE_EXTENSION		HwDeviceExtension,
	IN	BOOLEAN							DoNotAdjustAccessRight,
	OUT	PBOOLEAN						FirstInstallation
){
	NTSTATUS	ntStatus;
	ULONG		result;
	PVOID		AddDevInfo;
	LONG		AddDevInfoLength;
	ULONG		AddDevInfoFlag;

	if(HwDeviceExtension == NULL) {
		return SP_RETURN_NOT_FOUND;
	}

	//
	//	Get PDO and LUR information
	//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData
	//
	result = GetScsiAdapterPdoEnumInfo(
					HwDeviceExtension,
					HwDeviceExtension->SlotNumber,
					&AddDevInfoLength,
					&AddDevInfo,
					&AddDevInfoFlag);

	if(result != SP_RETURN_FOUND) {
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_ADAPTERENUM_FAIL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_GET_ENUM_INFO, 0)
			);
		return SP_RETURN_NOT_FOUND;
	}

	//
	//	If this is the first enumeration,
	//	Return success, but do not enumerate any device.
	//
	if(AddDevInfoFlag & PDOENUM_FLAG_DRV_NOT_INSTALLED) {
#if DBG
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_FIRSTINSTALL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FIRST_TIME_INSTALL, 0)
			);
#endif
		ExFreePoolWithTag(AddDevInfo, LSMP_PTAG_ENUMINFO);
		*FirstInstallation = TRUE;
		return SP_RETURN_FOUND;
	}


	//
	//	Create a LURN
	//
	if(AddDevInfoFlag & PDOENUM_FLAG_LURDESC) {
#if DBG
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_RECV_LURDESC,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_DETECT_LURDESC, 0)
			);
#endif
		if(DoNotAdjustAccessRight) {
			((PLURELATION_DESC)AddDevInfo)->LurOptions |= LUROPTION_DONOT_ADJUST_ACCESSMODE;
		}

		ntStatus = AddLurDescDuringFindAdapter(
			HwDeviceExtension,
			AddDevInfoLength,
			AddDevInfo
			);
	} else {
#if DBG
		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_RECV_ADDTARGETDATA,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_DETECT_ADDTARGET, 0)
			);
#endif
		if(DoNotAdjustAccessRight) {
			((PLANSCSI_ADD_TARGET_DATA)AddDevInfo)->LurOptions |= LUROPTION_DONOT_ADJUST_ACCESSMODE;
		}

		ntStatus = AddTargetDataDuringFindAdapter(
			HwDeviceExtension,
			AddDevInfoLength,
			AddDevInfo
			);
	}
	ExFreePoolWithTag(AddDevInfo, LSMP_PTAG_ENUMINFO);
	if(!NT_SUCCESS(ntStatus)) {
		return SP_RETURN_NOT_FOUND;
	}


	//
	//	Make a reference to driver object for each LUR creation
	//	to prevent from unloading unexpectedly.
	//	Must be decreased at each LUR deletion.
	//

	ObReferenceObject(_NdscGlobals.DriverObject);

	return SP_RETURN_FOUND;
}


//
//	find a scsi adapter on the bus.
//
ULONG
MiniFindAdapter(
    IN		PMINIPORT_DEVICE_EXTENSION		HwDeviceExtension,
    IN		PVOID							Context,
    IN		PVOID							BusInformation,
    IN		PCHAR							ArgumentString,
    IN OUT	PPORT_CONFIGURATION_INFORMATION	ConfigInfo,
    OUT		PBOOLEAN						Again
    )
{
	ULONG						result;
	BOOLEAN						firstInstallation;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(BusInformation);
	UNREFERENCED_PARAMETER(ArgumentString);

	KDPrint(2,("\n"));

#if DBG
	NDScsiLogError(
		HwDeviceExtension,
		NULL,
		0,
		0,
		0,
		NDASSCSI_IO_FINDADAPTER_START,
		EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_START_FIND, 0)
		);
#endif

	firstInstallation = FALSE;

	//
	//	Initialize MINIPORT_DEVICE_EXTENSION
	//
	RtlZeroMemory(HwDeviceExtension, sizeof(MINIPORT_DEVICE_EXTENSION));

	HwDeviceExtension->RequestExecuting				= 0;
	HwDeviceExtension->TimerOn						= FALSE;
	HwDeviceExtension->AdapterStatus				= ADAPTER_STATUS_INIT;
	HwDeviceExtension->NumberOfBuses				= 1;
	HwDeviceExtension->InitiatorId					= INITIATOR_ID;
	HwDeviceExtension->MaximumNumberOfTargets		= MAX_NR_LOGICAL_TARGETS + 1;	// add one for the initiator.
	HwDeviceExtension->MaximumNumberOfLogicalUnits	= MAX_NR_LOGICAL_LU;
	HwDeviceExtension->SlotNumber					= ConfigInfo->SystemIoBusNumber;
	HwDeviceExtension->ScsiportFdoObject			= FindScsiportFdo(HwDeviceExtension);
	HwDeviceExtension->AdapterMaxBlocksPerRequest	= 0;
	KeQuerySystemTime(&HwDeviceExtension->EnabledTime);

	InitializeListHead(&HwDeviceExtension->CcbTimerCompletionList);
	KeInitializeSpinLock(&HwDeviceExtension->CcbTimerCompletionListSpinLock);

	//
	//	Get PDO and LUR information
	//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData
	//
	result = EnumerateLURFromNDASBUS(HwDeviceExtension, FALSE, &firstInstallation);
	if(result != SP_RETURN_FOUND) {

		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_INITLUR_FAIL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_ADD_LUR, 0)
			);

		UpdateStatusInLSBus(HwDeviceExtension->SlotNumber,
					ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT | ADAPTER_STATUS_STOPPED);
		if(HwDeviceExtension->DisconEventToService)
			KeSetEvent(HwDeviceExtension->DisconEventToService, IO_NO_INCREMENT, FALSE);
		*Again = FALSE;
		return SP_RETURN_NOT_FOUND;
	}

	//
	//	If this is the first enumeration,
	//	Do not enumerate any device.
	//
	if(firstInstallation) {
		UpdateStatusInLSBus(HwDeviceExtension->SlotNumber, 
					ADAPTER_STATUS_STOPPED);
		KDPrint(1, ("First time installation. Do not enumerate a device.\n"));
	}

	//
	//	Set PORT_CONFIGURATION_INFORMATION
	//
	ConfigInfo->Length;
	ConfigInfo->SystemIoBusNumber;
	ConfigInfo->AdapterInterfaceType;

	ConfigInfo->BusInterruptLevel;
	ConfigInfo->BusInterruptVector;
	ConfigInfo->InterruptMode;
	
	KDPrint(1,("Adapter MaxBlocksPerRequest = 0x%x\n", HwDeviceExtension->AdapterMaxBlocksPerRequest));
	KDPrint(1,("Config MaximumTranferLength = 0x%x\n", ConfigInfo->MaximumTransferLength));

	//
	//	Determine the adapter's max request blocks.
	// If HwDeviceExtension->AdapterMaxBlocksPerRequest == 0,
	//	set default value
	//

	if(HwDeviceExtension->AdapterMaxBlocksPerRequest != 0) {

		//
		//	If SCSI port does not specify the value,
		//	set from HwDeviceExtension
		//

		if(ConfigInfo->MaximumTransferLength == SP_UNINITIALIZED_VALUE) {

			ConfigInfo->MaximumTransferLength = HwDeviceExtension->AdapterMaxBlocksPerRequest * BLOCK_SIZE;

		} else {
			//
			//	Choose smaller one
			//
			if(HwDeviceExtension->AdapterMaxBlocksPerRequest * BLOCK_SIZE > ConfigInfo->MaximumTransferLength)
				HwDeviceExtension->AdapterMaxBlocksPerRequest = ConfigInfo->MaximumTransferLength / BLOCK_SIZE;
			else
				ConfigInfo->MaximumTransferLength = HwDeviceExtension->AdapterMaxBlocksPerRequest * BLOCK_SIZE;
		}
	} else {

		//
		//	Set default value if SCSI port does not specify the value.
		//

		if(ConfigInfo->MaximumTransferLength == SP_UNINITIALIZED_VALUE)
			ConfigInfo->MaximumTransferLength = NDSC_DEFAULT_MAXREQUESTBLOCKS;
	}
	KDPrint(1,("Set MaximumTranferLength = 0x%x\n", ConfigInfo->MaximumTransferLength));


//	if((ConfigInfo->NumberOfPhysicalBreaks+1) == 0)
//		ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_DESCRIPTORS - 1;

	ConfigInfo->DmaChannel;
	ConfigInfo->DmaPort;
	ConfigInfo->DmaWidth;
	ConfigInfo->DmaSpeed;

	ConfigInfo->AlignmentMask = 0x0; //0x000001FF;
  
	ConfigInfo->NumberOfAccessRanges;
	ConfigInfo->AccessRanges;
	ConfigInfo->Reserved;
	ConfigInfo->NumberOfBuses = HwDeviceExtension->NumberOfBuses;
	if((ConfigInfo->InitiatorBusId[0]+1) == 0)
		ConfigInfo->InitiatorBusId[0] = HwDeviceExtension->InitiatorId;
/*
	if(!ConfigInfo->ScatterGather)
		ConfigInfo->ScatterGather = TRUE;
*/
	ConfigInfo->ScatterGather;

	//
	//	Windows XP/Server 2003 x64 does not allow non-master mini port device.
	//

#ifdef _WIN64
	ConfigInfo->Master = TRUE;
#else
	ConfigInfo->Master;
#endif

	ConfigInfo->CachesData = FALSE;
	ConfigInfo->AdapterScansDown;
	ConfigInfo->AtdiskPrimaryClaimed;
	ConfigInfo->AtdiskSecondaryClaimed;
	ConfigInfo->Dma32BitAddresses;
	ConfigInfo->DemandMode;
	ConfigInfo->MapBuffers;
	ConfigInfo->NeedPhysicalAddresses;
	ConfigInfo->TaggedQueuing;
	ConfigInfo->AutoRequestSense = TRUE;
	
	ConfigInfo->MultipleRequestPerLu = FALSE;
	
	ConfigInfo->ReceiveEvent;
	ConfigInfo->RealModeInitialized;
	ConfigInfo->BufferAccessScsiPortControlled;
	ConfigInfo->MaximumNumberOfTargets = HwDeviceExtension->MaximumNumberOfTargets;
	ConfigInfo->ReservedUchars;
	ConfigInfo->SlotNumber;
	
	KDPrint(1,("SlotNumber 0x%x, %d!!!!!\n", ConfigInfo->SlotNumber, ConfigInfo->SlotNumber >> 6));
	
	ConfigInfo->BusInterruptLevel2;
	ConfigInfo->BusInterruptVector2;
	ConfigInfo->InterruptMode2;
	ConfigInfo->DmaChannel2;
	ConfigInfo->DmaPort2;
	ConfigInfo->DmaWidth2;
	ConfigInfo->DmaSpeed2;
	ConfigInfo->DeviceExtensionSize;
	ConfigInfo->SpecificLuExtensionSize;
	ConfigInfo->SrbExtensionSize = 0;
	ConfigInfo->Dma64BitAddresses;
	ConfigInfo->ResetTargetSupported;
	ConfigInfo->MaximumNumberOfLogicalUnits = HwDeviceExtension->MaximumNumberOfLogicalUnits;
	ConfigInfo->WmiDataProvider = FALSE;

#if DBG
	NDScsiLogError(
		HwDeviceExtension,
		NULL,
		0,
		0,
		0,
		NDASSCSI_IO_FINDADAPTER_SUCC,
		EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_SUCCEED_FIND, 0)
		);
#endif

	return SP_RETURN_FOUND;
}


//
// Initialize the miniport device extension
//
BOOLEAN
MiniHwInitialize(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
    )
{
	KIRQL	oldIrql;
	UINT32	AdapterStatus;
	KDPrint(2,("\n"));
	
	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	AdapterStatus = ADAPTER_SETSTATUS(HwDeviceExtension,ADAPTER_STATUS_RUNNING);


	//
	// Notify NdasBus to reset the adapter's status value.
	//

	AdapterStatus |= ADAPTER_STATUSFLAG_RESETSTATUS;

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	KDPrint(1,("AdapterMaxBlocksPerRequest:%d DisconEventToService:%p AlarmEventToService:%p\n",
						HwDeviceExtension->AdapterMaxBlocksPerRequest,
						HwDeviceExtension->DisconEventToService,
						HwDeviceExtension->AlarmEventToService
		));

	UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);
/*
	ScsiPortNotification(
		RequestTimerCall,
		HwDeviceExtension,
		MiniTimer,
		1000 // micro second
		);
*/
	return TRUE;
}


//
//	reset the bus by callback
//
BOOLEAN
MiniResetBus(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN ULONG						PathId
    )
{
	NTSTATUS					status;

	KDPrint(2,("PathId = %d KeGetCurrentIrql()= 0x%x\n", 
						PathId, KeGetCurrentIrql()));

	ASSERT(PathId <= 0xFF);
	ASSERT(SCSI_MAXIMUM_TARGETS_PER_BUS <= 0xFF);
	ASSERT(SCSI_MAXIMUM_LOGICAL_UNITS <= 0xFF);

	status = MiniResetBusBySrb(HwDeviceExtension, PathId);
	if(status == STATUS_MORE_PROCESSING_REQUIRED) {

		//
		//	Send a CCB to the root of LURelation.
		//
		status = SendCcbToAllLURs(
						HwDeviceExtension,
						CCB_OPCODE_RESETBUS,
						NULL,
						NULL
					);
		if(!NT_SUCCESS(status)) {
			return NT_SUCCESS(status);
		}
	}

	return NT_SUCCESS(status);
}


//
//	reset bus by SRB
//
NTSTATUS
MiniResetBusBySrb(
	    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		IN ULONG						PathId
){
	KIRQL		oldIrql;

	KDPrint(1,("PathId = %d KeGetCurrentIrql()= 0x%x\n", 
						PathId, KeGetCurrentIrql()));
	UNREFERENCED_PARAMETER(HwDeviceExtension);
#if !DBG
	UNREFERENCED_PARAMETER(PathId);
#endif
	ASSERT(PathId <= 0xFF);
	ASSERT(SCSI_MAXIMUM_TARGETS_PER_BUS <= 0xFF);
	ASSERT(SCSI_MAXIMUM_LOGICAL_UNITS <= 0xFF);

	NDScsiLogError(
				HwDeviceExtension,
				NULL,
				0,
				0,
				0,
				NDASSCSI_IO_BUSRESET_OCCUR,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_BUSRESET_OCCUR, HwDeviceExtension->CcbSeqIdStamp)
	);

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

	ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING);

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	//
	//	Increment Stamp value to prevent old stamped CCBs to complete.
	//

	HwDeviceExtension->CcbSeqIdStamp++;

	//
	//	Make Scsiport complete all pending SRBs.
	//

	ScsiPortCompleteRequest(HwDeviceExtension, (UCHAR)PathId, SP_UNTAGGED, SP_UNTAGGED, SRB_STATUS_BUS_RESET);

	//
	//	Set Timer on to FALSE to refresh this adapter's timer.
	//

	HwDeviceExtension->TimerOn = FALSE;

	KDPrint(1,("set CcbSeqIdStamp:%lu\n", 
						HwDeviceExtension->CcbSeqIdStamp));

	return STATUS_MORE_PROCESSING_REQUIRED;

}

VOID
MiniCompleteLeftoverCcb(
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
){
	PLIST_ENTRY			listEntry;
	PCCB				ccb;
	PSCSI_REQUEST_BLOCK	srb;

	do {

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
		KDPrint(1,("completed SRB:%p SCSIOP:%x\n", srb, srb->Cdb[0]));

		if(	srb && IS_CCB_VAILD_SEQID(HwDeviceExtension, ccb)) {

			InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
			LsuDecrementTdiClientInProgress();

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

}

//
//	control miniport
//
#define SUPPORT_TYPE_MAX 5

VOID
MiniStopAdapter(
	    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		IN BOOLEAN						SetDisconEvent,
		IN BOOLEAN						CallByPort
	) {
	NDSC_WORKITEM_INIT			WorkitemCtx;
	UINT32						AdapterStatus;
	KIRQL						oldIrql;
	PLURELATION					tempLUR;

#if DBG
	NDScsiLogError(
		HwDeviceExtension,
		NULL,
		0,
		0,
		0,
		NDASSCSI_IO_STOP_REQUESTED,
		EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ADAPTER_STOP, 0)
		);
#endif

	KDPrint(1,("entered.\n"));

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING)) {

		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		KDPrint(1,("Already stopping in progress.\n"));

		NDScsiLogError(
				HwDeviceExtension,
				NULL,
				0,
				0,
				0,
				NDASSCSI_IO_STOPIOCTL_WHILESTOPPING,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_STOP_DURING_STOPPING, 0)
			);
		return;
	}

	AdapterStatus = ADAPTER_SETSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING);

#if DBG
	if(HwDeviceExtension->LURs[0] == NULL) {

		KDPrint(1,("No LUR is available.\n"));

		NDScsiLogError(
			HwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_STOPIOCTL_INVALIDLUR,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_STOP_WITH_INVALIDLUR, 0)
			);
	}
#endif
	//
	//	Save LURs and LURs's granted access mode and then 
	//	set NULL to the original.
	//	For now, we support one LUR for each miniport device.
	//

	tempLUR = HwDeviceExtension->LURs[0];
	HwDeviceExtension->LURs[0] = NULL;
	InterlockedDecrement(&HwDeviceExtension->LURCount);

	if(tempLUR) {
		HwDeviceExtension->LURSavedGrantedAccess[0] = tempLUR->GrantedAccess;
	}

	KDPrint(1,("HwDeviceExtension->RequestExecuting:%u\n",
				HwDeviceExtension->RequestExecuting));

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);


	//
	//	Clean up non-completed CCBs
	//	Get CCB to be completed
	//

	if(CallByPort) {
		MiniCompleteLeftoverCcb(HwDeviceExtension);
	}


	//
	//	Call Stop routine by workitem.
	//

	if(SetDisconEvent) {
		NDSC_INIT_WORKITEM(
					&WorkitemCtx,
					LSMP_StopWorker,
					NULL,
					tempLUR,									// Arg1
					HwDeviceExtension->DisconEventToService,	// Arg2
					UlongToPtr(HwDeviceExtension->SlotNumber)	// Arg3
				);
	} else {
		NDSC_INIT_WORKITEM(
					&WorkitemCtx,
					LSMP_StopWorker,
					NULL,
					tempLUR,									// Arg1
					NULL,										// Arg2
					UlongToPtr(HwDeviceExtension->SlotNumber)	// Arg3
				);
	}
	MiniQueueWorkItem(&_NdscGlobals, HwDeviceExtension->ScsiportFdoObject,&WorkitemCtx);
}


//
// Restart an SCSI adapter
//

VOID
MiniRestartAdapter(
	    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
	) {
	NDSC_WORKITEM_INIT			WorkitemCtx;
	UINT32						AdapterStatus;
	KIRQL						oldIrql;

#if DBG
	NDScsiLogError(
		HwDeviceExtension,
		NULL,
		0,
		0,
		0,
		NDASSCSI_IO_STOP_REQUESTED,
		EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ADAPTER_STOP, 0)
		);
#endif

	KDPrint(1,("entered.\n"));

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RESTARTING)) {

		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		KDPrint(1,("Already reinitialization in progress.\n"));
		return;
	}

	ADAPTER_SETSTATUS(HwDeviceExtension, ADAPTER_STATUS_RUNNING);
	AdapterStatus = ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RESTARTING);

	if(HwDeviceExtension->LURs[0] != NULL) {
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

		KDPrint(1,("LUR[0] already exists.\n"));
		ASSERT(FALSE);
		return;
	}

	//
	//	Set TimerOn to FALSE to refresh this adapter's timer.
	//

	HwDeviceExtension->TimerOn = FALSE;

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);


	//
	//	Call Restart routine by workitem.
	//

	NDSC_INIT_WORKITEM(
					&WorkitemCtx,
					LSMP_RestartWorker,
					NULL,
					HwDeviceExtension,							// Arg1
					HwDeviceExtension->DisconEventToService,	// Arg2
					UlongToPtr(HwDeviceExtension->SlotNumber)	// Arg3
				);
	MiniQueueWorkItem(&_NdscGlobals, HwDeviceExtension->ScsiportFdoObject,
							&WorkitemCtx);
}


//
//	SCSI Miniport callback function to control Scsi Adapter
//

SCSI_ADAPTER_CONTROL_STATUS
MiniAdapterControl(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE	ControlType,
    IN PVOID						Parameters
    )
{
	PSCSI_SUPPORTED_CONTROL_TYPE_LIST	controlTypeList;
    ULONG								adjustedMaxControlType;
	SCSI_ADAPTER_CONTROL_STATUS			status;
    ULONG								index;

    BOOLEAN supportedConrolTypes[SUPPORT_TYPE_MAX] = {
        TRUE,	// ScsiQuerySupportedControlTypes
        TRUE,	// ScsiStopAdapter
        TRUE,	// ScsiRestartAdapter
        TRUE,	// ScsiSetBootConfig
        TRUE	// ScsiSetRunningConfig
        };

	KDPrint(1,("ControlType = %d KeGetCurrentIrql()= 0x%x\n",
						ControlType, KeGetCurrentIrql()));

    switch (ControlType) {

    case ScsiQuerySupportedControlTypes:

		KDPrint(1,("ScsiQuerySupportedControlTypes.\n"));
        controlTypeList = Parameters;
        adjustedMaxControlType = (controlTypeList->MaxControlType < SUPPORT_TYPE_MAX) 
									? controlTypeList->MaxControlType : SUPPORT_TYPE_MAX;

        for (index = 0; index < adjustedMaxControlType; index++) {

            controlTypeList->SupportedTypeList[index] = supportedConrolTypes[index];

        }
		status = ScsiAdapterControlSuccess;
        break;

    case ScsiStopAdapter:
	{
		KDPrint(1,("ScsiStopAdapter.\n"));
		MiniStopAdapter(HwDeviceExtension, FALSE, TRUE);
		status = ScsiAdapterControlSuccess;
		break;
	}

	case ScsiSetBootConfig:

		KDPrint(1,("ScsiSetBootConfig.\n"));
		status = ScsiAdapterControlSuccess;
		break;

	case ScsiSetRunningConfig:

		KDPrint(1,("ScsiSetRunningConfig.\n"));
		status = ScsiAdapterControlSuccess;
		break;

	case ScsiRestartAdapter:

		KDPrint(1,("ScsiRestartAdapter.\n"));
		MiniRestartAdapter(HwDeviceExtension);
		//
		//
		//
		status = ScsiAdapterControlSuccess;
		break;

    case ScsiAdapterControlMax:

		KDPrint(1,("ScsiAdapterControlMax.\n"));
        status = ScsiAdapterControlUnsuccessful;
        break;
				
	default:

		KDPrint(1,("default:%d\n", ControlType));
		status = ScsiAdapterControlUnsuccessful;
		break;
    }

    return status;
}


NTSTATUS
MiniExecuteScsi(
				IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
				IN PMINIPORT_LU_EXTENSION		LuExtension,
				IN PSCSI_REQUEST_BLOCK			Srb
			   )
{
	LONG		idx_lur;
    NTSTATUS	status;
	PLURELATION	LUR;
	KIRQL		oldIrql;


	status = STATUS_MORE_PROCESSING_REQUIRED;
	LUR = NULL;

	switch(Srb->Cdb[0]) {
	case 0xd8:
//		ASSERT(FALSE);
		break;
	case SCSIOP_INQUIRY: {

		//
		//	Block INQUIRY when LanscsiMiniport is stopping.
		//
		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING)) {
			KDPrint(1,("SCSIOP_INQUIRY: ADAPTER_STATUS_STOPPING. returned with error.\n"));

			NDScsiLogError(
				HwDeviceExtension,
				Srb,
				Srb->PathId,
				Srb->TargetId,
				Srb->Lun,
				NDASSCSI_IO_INQUIRY_WHILE_STOPPING,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_INQUIRY_DURING_STOPPING, 0)
				);

			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			status = STATUS_SUCCESS;
		}
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);


		//
		//	look up Logical Unit Relations and set it to LuExtension
		//

		LuExtension->LUR = NULL;
		for(idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++) {
			LUR = HwDeviceExtension->LURs[idx_lur];

			if(	LUR &&
				LUR->LurId[0] == Srb->PathId &&
				LUR->LurId[1] == Srb->TargetId &&
				LUR->LurId[2] == Srb->Lun
				) {
				KDPrint(1,("SCSIOP_INQUIRY: set LUR(%p) to LuExtension(%p)\n", LUR, LuExtension));

				ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
				UpdatePdoInfoInLSBus(HwDeviceExtension, HwDeviceExtension->AdapterStatus);
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
				break;
			}

			LUR = NULL;
		}

		if(LUR == NULL) {
			KDPrint(1,("LUR == NULL LuExtension(%p) HwDeviceExtension(%p), HwDeviceExtension->LURCount(%d)\n",
				LuExtension, HwDeviceExtension, HwDeviceExtension->LURCount));
#if DBG
			NDScsiLogError(
				HwDeviceExtension,
				Srb,
				Srb->PathId,
				Srb->TargetId,
				Srb->Lun,
				NDASSCSI_IO_LUR_NOT_FOUND,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_INQUIRY_LUR_NOT_FOUND, 0)
				);
#endif
			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			status = STATUS_SUCCESS;
		}
		break;
	}
	default: {
		break;
		}
	}

	return status;
}


//
//	Detect shipping SRB of SRB that carry an SRB to be completed.
//
//	return: TRUE - shipping SRB detected and completed.
//			FALSE - not a shipping SRB
//

BOOLEAN
DetectShippingSRB
(
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK			Srb
){
	if(Srb->Function == SRB_FUNCTION_EXECUTE_SCSI
		&& Srb->Cdb[0] == SCSIOP_COMPLETE) {

		PCCB				shippedCcb = (PCCB)Srb->DataBuffer;
		PSCSI_REQUEST_BLOCK	shippedSrb;

		KDPrint(3,("SCSIOP_COMPLETE: SrbSeq=%d\n", (LONG)Srb->Cdb[1]));
		shippedSrb = shippedCcb->Srb;

		//
		//	Complete the 'shipped' SRB that sender wants to make completed
		//

		if(	shippedSrb ) {

			Srb->DataBuffer = NULL;
			if(IS_CCB_VAILD_SEQID(HwDeviceExtension, shippedCcb)) {

				InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
				LsuDecrementTdiClientInProgress();

				ASSERT(shippedSrb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
				ScsiPortNotification(
						RequestComplete,
						HwDeviceExtension,
						shippedSrb
					);
			} else {
				//
				// Leave error log.
				//
				NDScsiLogError(
					HwDeviceExtension,
					Srb,
					Srb->PathId,
					Srb->TargetId,
					Srb->Lun,
					NDASSCSI_IO_SRB_DISCARDED,
					EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_DISCARD_SRB, shippedCcb->CcbSeqId)
					);
				KDPrint(1,("CCB:%p has a old ID:%lu. CurrentID:%lu\n",
							shippedCcb,
							shippedCcb->CcbSeqId,
							HwDeviceExtension->CcbSeqIdStamp));
			}
		} else {

			//
			// Leave error log.
			//

			NDScsiLogError(
				HwDeviceExtension,
				Srb,
				Srb->PathId,
				Srb->TargetId,
				Srb->Lun,
				NDASSCSI_IO_NO_SHIPPED_SRB,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_NO_SHIPPED_SRB, 0)
				);
			KDPrint(1,("CCB:%p doesn't have SRB.\n", shippedCcb));
		}


		//
		// Complete the shipping SRB
		//

		Srb->SrbStatus = SRB_STATUS_SUCCESS;
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		ScsiPortNotification(
			RequestComplete,
			HwDeviceExtension,
			Srb
			);

		ScsiPortNotification(
			NextRequest,
			HwDeviceExtension,
			NULL
			);
		return TRUE;
	}


	return FALSE;
}


//
//	StartIO
//	Receives all SRBs toward this adapter.
//

BOOLEAN
MiniStartIo(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK			Srb
    )
{
	PMINIPORT_LU_EXTENSION		luExtension;
	PCCB						ccb;
	NTSTATUS					status;
	KIRQL						oldIrql;
	BOOLEAN						shippingSrbDetected;
	BOOLEAN						busReset;

	luExtension = ScsiPortGetLogicalUnit(
			HwDeviceExtension,
			Srb->PathId,
			Srb->TargetId,
			Srb->Lun
		);
		
	ASSERT(luExtension);

	shippingSrbDetected = DetectShippingSRB(HwDeviceExtension, Srb);
	if(shippingSrbDetected == TRUE) {
		return TRUE;
	}

	//
	//	Clear BusReset flags because a normal request is entered.
	//
	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING)) {
		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING);
		busReset = TRUE;

		KDPrint(1,("Bus Reset detected.\n"));
	} else {
		busReset = FALSE;
	}
	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	//
	//	Set Timer in case of the timer completion of the SRB.
	//

	if(HwDeviceExtension->TimerOn == FALSE)
	{
		ScsiPortNotification(
			RequestTimerCall,
			HwDeviceExtension,
			MiniTimer,
			1000 // micro second
			);
		HwDeviceExtension->TimerOn = TRUE;
	}


	//
	//	check the adapter status
	//
	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING) ||
		ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_RESTARTING)
		) {
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

#if DBG
		if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING)) {
			KDPrint(1,("Error! stopping in progress due to internal error.\n"));
		} else {
			KDPrint(1,("Delay the request due to reinit.\n"));
		}
#endif
		KDPrint(1,("Func:%x %s(0x%02x) Srb(%p) CdbLen=%d\n",
			Srb->Function,
			CdbOperationString(Srb->Cdb[0]),
			(int)Srb->Cdb[0],
			Srb,
			(UCHAR)Srb->CdbLength
			));
		KDPrint(1,(" TxLen:%d TO:%d SenseLen:%d SrbFlags:%08lx\n",
			Srb->DataTransferLength,
			Srb->TimeOutValue,
			Srb->SenseInfoBufferLength,
			Srb->SrbFlags));
		ASSERT(!Srb->NextSrb);

		if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING))
			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
		else
			Srb->SrbStatus = SRB_STATUS_BUSY;

		Srb->ScsiStatus = SCSISTAT_GOOD;

		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING)) {
			RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
			//
			//	Do not complete the SRB.
			//	MiniResetBus() routine has been completed all active SRBs.
			//
			NDScsiLogError(
				HwDeviceExtension,
				Srb,
				Srb->PathId,
				Srb->TargetId,
				Srb->Lun,
				NDASSCSI_IO_SRB_DISCARDED,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_DISCARD_SRB, 0)
				);
		} else {
			RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
			ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
			ScsiPortNotification(
					RequestComplete,
					HwDeviceExtension,
					Srb
				);
		}

		ScsiPortNotification(
			NextRequest,
			HwDeviceExtension,
			NULL
			);
		return TRUE;
	}
	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	if(Srb->Function != SRB_FUNCTION_EXECUTE_SCSI) {
		KDPrint(1,("--->0x%x, Srb->Function = %s, Srb->PathId = %d, Srb->TargetId = %d Srb->Lun = %d\n",
			Srb->Function, SrbFunctionCodeString(Srb->Function), Srb->PathId, Srb->TargetId, Srb->Lun));
	}

	//
	//	Adapter dispatcher
	//
	switch(Srb->Function) {

	case SRB_FUNCTION_ABORT_COMMAND:
		KDPrint(1,("SRB_FUNCTION_ABORT_COMMAND: Srb = %p Srb->NextSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));
		NDScsiLogError(
			HwDeviceExtension,
			Srb,
			Srb->PathId,
			Srb->TargetId,
			Srb->Lun,
			NDASSCSI_IO_ABORT_SRB,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ABORT_SRB_ENTERED, 0)
			);
		ASSERT(Srb->NextSrb);
		ASSERT(FALSE);
		break;

	case SRB_FUNCTION_RESET_BUS:
		KDPrint(1,("SRB_FUNCTION_RESET_BUS: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));

		status = MiniResetBusBySrb(HwDeviceExtension, Srb->PathId);
		if(status == STATUS_MORE_PROCESSING_REQUIRED) {

			break;

		} else if(NT_SUCCESS(status)) {
			Srb->SrbStatus = SRB_STATUS_SUCCESS;
		} else {
			Srb->SrbStatus = SRB_STATUS_ERROR;
		}

		//
		//	Complete the request.
		//
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		ScsiPortNotification(
				RequestComplete,
				HwDeviceExtension,
				Srb
			);

		ScsiPortNotification(
				NextRequest,
				HwDeviceExtension,
				NULL
			);

		return TRUE;

	case SRB_FUNCTION_IO_CONTROL:
		KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));

		status = MiniSrbControl(HwDeviceExtension, luExtension, Srb, HwDeviceExtension->CcbSeqIdStamp);
		if(status == STATUS_PENDING) {
			KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb pending.\n" ));
			return TRUE;
		} else if(status != STATUS_MORE_PROCESSING_REQUIRED) {

			//
			//	Complete the request.
			//
			ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
			if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING)) {
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
				//
				//	Do not complete the SRB.
				//	MiniResetBus() routine has been completed all active SRBs.
				//
				NDScsiLogError(
					HwDeviceExtension,
					Srb,
					Srb->PathId,
					Srb->TargetId,
					Srb->Lun,
					NDASSCSI_IO_ABORT_SRB,
					EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ABORT_SRB_ENTERED, 0)
					);
			} else {
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
				ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
				ScsiPortNotification(
						RequestComplete,
						HwDeviceExtension,
						Srb
					);
			}

			ScsiPortNotification(
					NextRequest,
					HwDeviceExtension,
					NULL
				);

			KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb completed.\n" ));
			return TRUE;
		}
		KDPrint(1,("SRB_FUNCTION_IO_CONTROL: going to LUR\n" ));

	case SRB_FUNCTION_EXECUTE_SCSI:

#if DBG
		if((	Srb->Cdb[0] != SCSIOP_READ &&
			Srb->Cdb[0] != SCSIOP_READ16 &&
			Srb->Cdb[0] != SCSIOP_WRITE &&
			Srb->Cdb[0] != SCSIOP_WRITE16 &&
			Srb->Cdb[0] != SCSIOP_VERIFY &&
			Srb->Cdb[0] != SCSIOP_VERIFY16 &&
			Srb->Cdb[0] != SCSIOP_READ_CAPACITY &&
			Srb->Cdb[0] != SCSIOP_READ_CAPACITY16) ||
			busReset)
		{
			KDPrint(1,("EXECUTE: %s(0x%02x) Srb(%p) CdbLen=%d\n",
												CdbOperationString(Srb->Cdb[0]),
												(int)Srb->Cdb[0],
												Srb,
												(UCHAR)Srb->CdbLength
												));
			KDPrint(1,(" TxLen:%d TO:%d SenseLen:%d SrbFlags:%08lx\n",
												Srb->DataTransferLength,
												Srb->TimeOutValue,
												Srb->SenseInfoBufferLength,
												Srb->SrbFlags));
			ASSERT(!Srb->NextSrb);

		}
#endif

		status = MiniExecuteScsi(HwDeviceExtension, luExtension, Srb);
		if(status != STATUS_MORE_PROCESSING_REQUIRED) {

			KDPrint(1,("SRB_FUNCTION_EXECUTE_SCSI: Srb = %p completed.\n", Srb));
			//
			//	Complete the request.
			//
			ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
			if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING)) {

				ASSERT(!(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE));
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);


				//
				//	Do not complete the SRB.
				//	MiniResetBus() routine has been completed all active SRBs.
				//

				KDPrint(1,("SRB_FUNCTION_EXECUTE_SCSI: Srb = %p will not be completed due to BUS RESET", Srb));

				NDScsiLogError(
					HwDeviceExtension,
					Srb,
					Srb->PathId,
					Srb->TargetId,
					Srb->Lun,
					NDASSCSI_IO_ABORT_SRB,
					EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ABORT_SRB_ENTERED, 0)
					);
			} else {

				ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

				ScsiPortNotification(
						RequestComplete,
						HwDeviceExtension,
						Srb
					);
			}

			ScsiPortNotification(
					NextRequest,
					HwDeviceExtension,
					NULL
				);

			return TRUE;
		}

		break;

	case SRB_FUNCTION_RESET_DEVICE:
			KDPrint(1,("SRB_FUNCTION_RESET_DEVICE: CurrentSrb is Presented. Srb = %x\n", Srb));

	default:
			KDPrint(1,("Invalid SRB Function:%d Srb = %x\n", 
									Srb->Function,Srb));

		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		//
		//	Complete the request.
		//
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		ScsiPortNotification(
				RequestComplete,
				HwDeviceExtension,
				Srb
			);

		ScsiPortNotification(
				NextRequest,
				HwDeviceExtension,
				NULL
			);
		return TRUE;
	}


	//	
	//	initialize Ccb in srb to call LUR dispatchers.
	//

	status = LSCcbAllocate(&ccb);
	if(!NT_SUCCESS(status)) {
		NDScsiLogError(
			HwDeviceExtension,
			Srb,
			Srb->PathId,
			Srb->TargetId,
			Srb->Lun,
			NDASSCSI_IO_CCBALLOC_FAIL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_CCB_ALLOCATION_FAIL, 0)
			);

		Srb->SrbStatus = SRB_STATUS_ERROR;
		KDPrint(1,("LSCcbAllocate() failed.\n" ));
		return TRUE;
	}
	LSCcbInitialize(
			Srb,
			HwDeviceExtension,
			HwDeviceExtension->CcbSeqIdStamp,
			ccb
		);

	InterlockedIncrement(&HwDeviceExtension->RequestExecuting);
	LsuIncrementTdiClientInProgress();

	LSCcbSetCompletionRoutine(ccb, NdscAdapterCompletion, HwDeviceExtension);

	if(busReset) {
		KDPrint(1,("Set timer completion due to Bus Reset.\n"));
		LSCcbSetStatusFlag(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
	}

	//
	//	Send a CCB to LURelation.
	//

	status = LurRequest(
					HwDeviceExtension->LURs[0],
					ccb
				);

	//
	//	If faliure, SRB was not queued. Complete it with an error.
	//

	if( !NT_SUCCESS(status) ) {

		ASSERT(FALSE);

		//
		//	Free resources for the SRB.
		//

		InterlockedDecrement(&HwDeviceExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		LSCcbFree(ccb);

		//
		//	Complete the SRB with an error.
		//

		Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
		Srb->ScsiStatus = SCSISTAT_GOOD;
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		ScsiPortNotification(
				RequestComplete,
				HwDeviceExtension,
				Srb
			);
	}
	ScsiPortNotification(
			NextRequest,
			HwDeviceExtension,
			NULL
		);

	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
//
//	LU Relation operation
//


NTSTATUS
SendCcbToAllLURsSync(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	KDPrint(1,("entered.\n"));

	for(idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++ ) {

		ntStatus = LSCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb, 0);
		ccb->OperationCode				= CcbOpCode;
		ccb->LurId[0]					= HwDeviceExtension->LURs[idx_lur]->LurId[0];
		ccb->LurId[1]					= HwDeviceExtension->LURs[idx_lur]->LurId[1];
		ccb->LurId[2]					= HwDeviceExtension->LURs[idx_lur]->LurId[2];
		ccb->HwDeviceExtension			= HwDeviceExtension;

		LSCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
		LSCcbSetCompletionRoutine(ccb, NULL, NULL);

		//
		//	Send a CCB to the root of LURelation.
		//
		ntStatus = LurRequest(
				HwDeviceExtension->LURs[idx_lur],
				ccb
			);
		if(!NT_SUCCESS(ntStatus)) {
			LSCcbPostCompleteCcb(ccb);
			return ntStatus;
		}
	}

	return STATUS_SUCCESS;
}


NTSTATUS
SendCcbToLURSync(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		PLURELATION		LUR,
		UINT32			CcbOpCode
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;

	KDPrint(1,("entered.\n"));

	ntStatus = LSCcbAllocate(&ccb);
	if(!NT_SUCCESS(ntStatus)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LSCCB_INITIALIZE(ccb, 0);
	ccb->OperationCode				= CcbOpCode;
	ccb->LurId[0]					= LUR->LurId[0];
	ccb->LurId[1]					= LUR->LurId[1];
	ccb->LurId[2]					= LUR->LurId[2];
	ccb->HwDeviceExtension			= HwDeviceExtension;
	LSCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
	LSCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	//	Send a CCB to the root of LURelation.
	//
	ntStatus = LurRequest(
			LUR,
			ccb
		);
	if(!NT_SUCCESS(ntStatus)) {
		LSCcbPostCompleteCcb(ccb);
		return ntStatus;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
SendCcbToAllLURs(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode,
		CCB_COMPLETION_ROUTINE		CcbCompletionRoutine,
		PVOID						CompletionContext

	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	KDPrint(3,("entered.\n"));

	for(idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++ ) {

		ntStatus = LSCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("allocation fail.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb, 0);
		ccb->OperationCode				= CcbOpCode;
		ccb->LurId[0]					= HwDeviceExtension->InitiatorId;
		ccb->LurId[1]					= 0;
		ccb->LurId[2]					= 0;
		ccb->HwDeviceExtension			= HwDeviceExtension;
		LSCcbSetFlag(ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
		LSCcbSetCompletionRoutine(ccb, CcbCompletionRoutine, CompletionContext);

		//
		//	Send a CCB to the root of LURelation.
		//
		ntStatus = LurRequest(
				HwDeviceExtension->LURs[idx_lur],
				ccb
			);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("request fail.\n"));
			LSCcbPostCompleteCcb(ccb);
			return ntStatus;
		}
	}

	KDPrint(3,("exit.\n"));

	return STATUS_SUCCESS;
}


NTSTATUS
FreeAllLURs(
		LONG			LURCount,
		PLURELATION		*LURs
	) {
	LONG		idx_lur;

	KDPrint(1,("entered.\n"));

	for(idx_lur = 0; idx_lur < LURCount; idx_lur ++ ) {

		//
		//	call destroy routines for LURNs
		//
		if(LURs[idx_lur])
			LurClose(LURs[idx_lur]);
	}

	return STATUS_SUCCESS;
}

//
//	Send Stop Ccb to all LURs.
//
NTSTATUS
SendStopCcbToAllLURsSync(
		LONG			LURCount,
		PLURELATION		*LURs
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	KDPrint(1,("entered.\n"));

	for(idx_lur = 0; idx_lur < LURCount; idx_lur ++ ) {

		if(LURs[idx_lur] == NULL) {
			continue;
		}

		ntStatus = LSCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb, 0);
		ccb->OperationCode				= CCB_OPCODE_STOP;
		ccb->LurId[0]					= LURs[idx_lur]->LurId[0];
		ccb->LurId[1]					= LURs[idx_lur]->LurId[1];
		ccb->LurId[2]					= LURs[idx_lur]->LurId[2];
		ccb->HwDeviceExtension			= NULL;
		LSCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_RETRY_NOT_ALLOWED);
		LSCcbSetCompletionRoutine(ccb, NULL, NULL);

		//
		//	Send a CCB to the root of LURelation.
		//
		ntStatus = LurRequest(
				LURs[idx_lur],
				ccb
			);
		if(!NT_SUCCESS(ntStatus)) {
			LSCcbPostCompleteCcb(ccb);
			KDPrint(1,("LurRequest() failed\n"));
			return ntStatus;
		}
	}

	return STATUS_SUCCESS;
}

//
//	Stop worker from MinportControl.
//	NOTE: It must not reference HwDeviceExtenstion while stopping.
//
VOID
LSMP_StopWorker(
		IN PDEVICE_OBJECT			DeviceObject,
		IN PNDSC_WORKITEM			LSMPWorkItemCtx
	) {
	NTSTATUS			ntStatus;
	PLURELATION			LUR;
	PKEVENT				DisconEvent;
	ULONG				SlotNo;
	BUSENUM_SETPDOINFO	BusSet;
	NTSTATUS			status;
	KIRQL				oldIrql;

	KDPrint(1,("entered.\n"));
	UNREFERENCED_PARAMETER(DeviceObject);

#if 0 // DBG
	{
		LARGE_INTEGER	interval;

		interval.QuadPart = - 10000000 * 10;
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
#endif

	LUR = (PLURELATION)LSMPWorkItemCtx->Arg1;
	DisconEvent = (PKEVENT)LSMPWorkItemCtx->Arg2;
	SlotNo = PtrToUlong(LSMPWorkItemCtx->Arg3);
	if(LUR == NULL) {
		KDPrint(1,("======> no LUR\n"));

		goto send_discon;
	}


	//
	//	CCB_OPCODE_STOP must succeed.
	//	NOTE: If LURs are more than one, do forget dereference
	//			the driver object for each LUR
	//

	ntStatus = SendStopCcbToAllLURsSync(1, &LUR);
#if DBG
	if(!NT_SUCCESS(ntStatus)) {
		ASSERT(FALSE);
	}
#endif

	FreeAllLURs(1, &LUR);

send_discon:

	//
	//	Update Status in LanscsiBus
	//	This code does not use HwDeviceExtension to be independent.
	//	If disconnection event is available, we regard it as a abnormal unplug.
	//
	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = ADAPTER_STATUS_STOPPED;
	BusSet.DesiredAccess = BusSet.GrantedAccess = 0;

	if(DisconEvent) {
		KDPrint(1,("Abnormal stop.\n"));
		BusSet.AdapterStatus |= ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT;
	}

	status = IoctlToLanscsiBus(
				IOCTL_LANSCSI_SETPDOINFO,
				&BusSet,
				sizeof(BUSENUM_SETPDOINFO),
				NULL,
				0,
				NULL);
	if(!NT_SUCCESS(status)) {
		KDPrint(1,("Failed to Update AdapterStatus in LanscsiMiniport.\n"));
	} else {
		KDPrint(1,("Updated AdapterStatus in LanscsiMiniport.\n"));
	}

	//
	//	Set Disconnecting event.
	//
	if(DisconEvent) {
		KeSetEvent(DisconEvent, IO_NO_INCREMENT, FALSE);
		KDPrint(1,("Set Disconnection event.\n"));
	}


	oldIrql = KeRaiseIrqlToDpcLevel();

	//
	//	Dereference the driver object for this LUR.
	//	Raise IRQL to defer driver object dereferencing.
	//

	if(LUR != NULL)
		ObDereferenceObject(_NdscGlobals.DriverObject);

	KeLowerIrql(oldIrql);
}


//
//	Stop worker from MinportControl.
//	NOTE: It must not reference HwDeviceExtenstion while stopping.
//

VOID
LSMP_RestartWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PNDSC_WORKITEM			LSMPWorkItemCtx
	) {
	PKEVENT				DisconEvent;
	ULONG				SlotNo;
	BUSENUM_SETPDOINFO	BusSet;
	NTSTATUS			status;
	ULONG				result;
	BOOLEAN						firstInstallation;
	PMINIPORT_DEVICE_EXTENSION	hwDeviceExtension;
	ULONG				adapterStatus;
	KIRQL				oldIrql;
	LONG				loop;
	LARGE_INTEGER		interval;

	KDPrint(1,("entered.\n"));
	UNREFERENCED_PARAMETER(DeviceObject);

	hwDeviceExtension = (PMINIPORT_DEVICE_EXTENSION)LSMPWorkItemCtx->Arg1;
	DisconEvent = (PKEVENT)LSMPWorkItemCtx->Arg2;
	SlotNo = PtrToUlong(LSMPWorkItemCtx->Arg3);


	//
	//	Restart the LUR
	//	Retry if failure.
	//

	interval.QuadPart = - NANO100_PER_SEC * 2;
	for(loop=0; loop < 10; loop++) {
		result = EnumerateLURFromNDASBUS(hwDeviceExtension, TRUE, &firstInstallation);
		if(result == SP_RETURN_FOUND) {
			break;
		}

		KDPrint(1,("Loop #%u. try creating LUR again.\n", loop));
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	};

	if(result == SP_RETURN_FOUND) {


		//
		//	Set AdapterStatus to RUNNUNG.
		//

		ACQUIRE_SPIN_LOCK(&hwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

		ASSERT(ADAPTER_ISSTATUS(hwDeviceExtension, ADAPTER_STATUS_RUNNING));
		ASSERT(ADAPTER_ISSTATUSFLAG(hwDeviceExtension, ADAPTER_STATUSFLAG_RESTARTING));

		//
		//	Set RUNNING status.
		//	clear all other flags.
		//

		hwDeviceExtension->AdapterStatus = ADAPTER_STATUS_RUNNING;

		RELEASE_SPIN_LOCK(&hwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

		adapterStatus = ADAPTER_STATUS_RUNNING |
						ADAPTER_STATUSFLAG_RESETSTATUS;
		//
		//	Do not alert to the service.
		//

		DisconEvent = NULL;
	} else {

		//
		// Set AdapterStatus to STOPPING to return error to all request from now.
		//

		KDPrint(1,("Abnormal stop.\n"));

		NDScsiLogError(
			hwDeviceExtension,
			NULL,
			0,
			0,
			0,
			NDASSCSI_IO_INITLUR_FAIL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_ADD_LUR, 0)
			);

		ACQUIRE_SPIN_LOCK(&hwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

		ASSERT(ADAPTER_ISSTATUS(hwDeviceExtension, ADAPTER_STATUS_RUNNING));
		ASSERT(ADAPTER_ISSTATUSFLAG(hwDeviceExtension, ADAPTER_STATUSFLAG_RESTARTING));

		hwDeviceExtension->AdapterStatus = ADAPTER_STATUS_STOPPING;

		RELEASE_SPIN_LOCK(&hwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);


		//
		//	Alert abnormal status of the SCSI adapter to the service.
		//

		adapterStatus = ADAPTER_STATUS_STOPPED |
						ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT;
	}


	//
	//	Update Status in LanscsiBus
	//	This code does not use HwDeviceExtension to be independent.
	//	If disconnection event is available, we regard it as a abnormal unplug.
	//
	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = adapterStatus;
	BusSet.DesiredAccess = BusSet.GrantedAccess = 0;

	status = IoctlToLanscsiBus(
		IOCTL_LANSCSI_SETPDOINFO,
		&BusSet,
		sizeof(BUSENUM_SETPDOINFO),
		NULL,
		0,
		NULL);
	if(!NT_SUCCESS(status)) {
		KDPrint(1,("Failed to Update AdapterStatus in LanscsiMiniport.\n"));
	} else {
		KDPrint(1,("Updated AdapterStatus in LanscsiMiniport.\n"));
	}

	//
	//	Set Disconnecting event.
	//
	if(DisconEvent) {
		KeSetEvent(DisconEvent, IO_NO_INCREMENT, FALSE);
		KDPrint(1,("Set Disconnection event.\n"));
	}
}


//
//	NoOperation worker.
//	Send NoOperation Ioctl to Miniport itself.
//
VOID
LSMP_NoOperationWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PNDSC_WORKITEM			LSMPWorkItemCtx
	) {
	LSMPIOCTL_NOOP	NoopData;

	KDPrint(1,("entered.\n"));
	NoopData.PathId		= (BYTE)LSMPWorkItemCtx->Arg1;
	NoopData.TargetId	= (BYTE)LSMPWorkItemCtx->Arg2;
	NoopData.Lun		= (BYTE)LSMPWorkItemCtx->Arg3;

	LSMPSendIoctlSrb(
			DeviceObject,
			LANSCSIMINIPORT_IOCTL_NOOP,
			&NoopData,
			sizeof(LSMPIOCTL_NOOP),
			NULL,
			0
		);

}

//////////////////////////////////////////////////////////////////////////
//
//	LurnCallback routine.
//
VOID
LsmpLurnCallback(
	PLURELATION	Lur,
	PLURN_EVENT	LurnEvent
) {

	ASSERT(LurnEvent);
	KDPrint(1,("Lur:%p Event class:%x\n",
									Lur, LurnEvent->LurnEventClass));

	switch(LurnEvent->LurnEventClass) {

	case LURN_REQUEST_NOOP_EVENT: {
		NDSC_WORKITEM_INIT			WorkitemCtx;

		NDSC_INIT_WORKITEM(
					&WorkitemCtx,
					LSMP_NoOperationWorker,
					NULL,
					(PVOID)Lur->LurId[0],
					(PVOID)Lur->LurId[1],
					(PVOID)Lur->LurId[2]
				);
		MiniQueueWorkItem(&_NdscGlobals, Lur->AdapterFdo, &WorkitemCtx);

//		KDPrint(1,("LURN_REQUEST_NOOP_EVENT: AdapterStatus %x.\n",
//									AdapterStatus));
	break;
	}

	default:
		KDPrint(1,("Invalid event class:%x\n", LurnEvent->LurnEventClass));
	break;
	}

}

