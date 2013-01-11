#include "port.h"
#include <ntddk.h>
#include <scsi.h>
#include <scsiwmi.h>
#include <ntddscsi.h>
#include "KDebug.h"
#include "LSMPIoctl.h"
#include "LanscsiMiniport.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSMP"

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
		IN PLSMP_WORKITEM_CTX			LSMPWorkItemCtx
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
//	Lanscsi Miniport DriverEntry
//
ULONG
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID RegistryPath
    )
{
	HW_INITIALIZATION_DATA	hwInitializationData;
	ULONG					isaStatus;
	ULONG					pnpBusStatus;


	KDPrint(1,("%s %s Flags:%08lx\n", __DATE__, __TIME__, DriverObject->Flags));

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

	hwInitializationData.DeviceExtensionSize		= (INT32)sizeof(MINIPORT_DEVICE_EXTENSION) + ( -1); 
	hwInitializationData.SpecificLuExtensionSize	= sizeof(MINIPORT_LU_EXTENSION); 
#ifdef __ALLOCATE_CCB_FROM_POOL__
	hwInitializationData.SrbExtensionSize			= 0; 
#else
	hwInitializationData.SrbExtensionSize			= sizeof(CCB);
#endif
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

	hwInitializationData.AdapterInterfaceType = PNPBus; 
	pnpBusStatus = ScsiPortInitialize(DriverObject, RegistryPath, &hwInitializationData, NULL);

	return (isaStatus < pnpBusStatus ? isaStatus : pnpBusStatus);
}

//////////////////////////////////////////////////////////////////////////
//
//	Miniport callbacks
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

	UNREFERENCED_PARAMETER(AddTargetDataLength);
	//
	//	Allocate LUR Descriptor
	//
	ChildLurnCnt	=	AddTargetData->ulNumberOfUnitDiskList;

	KDPrint(1,("SIZE_OF_LURELATION_DESC() = %d\n", SIZE_OF_LURELATION_DESC()));
	KDPrint(1,("SIZE_OF_LURELATION_NODE_DESC(0) = %d\n", SIZE_OF_LURELATION_NODE_DESC(0)));
	
	switch(AddTargetData->ucTargetType)
	{
	case DISK_TYPE_MIRROR:
	case DISK_TYPE_AGGREGATION:
		//	Associate type
		LurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt) + // root
							SIZE_OF_LURELATION_NODE_DESC(0) * (ChildLurnCnt);	// leaves
/*
				sizeof(LURELATION_DESC) + (sizeof(LONG) * (ChildLurnCnt-1)) +		// LURELATION_DESC and root LURELATION_NODE_DESC
							(sizeof(LURELATION_NODE_DESC) - sizeof(LONG) ) * ChildLurnCnt;	// child LURELATION_NODE_DESC
*/
		break;
	case DISK_TYPE_BIND_RAID1:
		LurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt /2) + // root
							SIZE_OF_LURELATION_NODE_DESC(2) * (ChildLurnCnt /2) +// mirrored disks
							SIZE_OF_LURELATION_NODE_DESC(0) * (ChildLurnCnt);	// leaves
		break;
	case DISK_TYPE_NORMAL:
	case DISK_TYPE_DVD:
	case DISK_TYPE_VDVD:
	case DISK_TYPE_MO:
		//	Single type
		LurDescLength	=	sizeof(LURELATION_DESC) - sizeof(LONG);
		break;
	default:
		return STATUS_ILLEGAL_INSTRUCTION;
	}

	LurDesc = (PLURELATION_DESC)ExAllocatePoolWithTag(NonPagedPool,  LurDescLength, LSMP_PTAG_IOCTL);
	if(LurDesc == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Translate AddTargetData to LUR Desc
	//
	status = LurTranslateAddTargetDataToLURDesc(AddTargetData, HwDeviceExtension->MaxBlocksPerRequest, LurDescLength, LurDesc);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(LurDesc, LSMP_PTAG_IOCTL);
		return status;
	}

	//
	//	Create an LUR with LUR Desc
	//
	status = LurCreate(LurDesc, &Lur, HwDeviceExtension->ScsiportFdoObject, LsmpLurnCallback);
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


	return status;
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
	NTSTATUS					ntStatus;
	PLANSCSI_ADD_TARGET_DATA	AddTargetData;
	LONG						AddTargetDataLength;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(BusInformation);
	UNREFERENCED_PARAMETER(ArgumentString);

	KDPrint(2,("\n"));

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
	KeQuerySystemTime(&HwDeviceExtension->EnabledTime);

	InitializeListHead(&HwDeviceExtension->CcbCompletionList);
	KeInitializeSpinLock(&HwDeviceExtension->CcbCompletionListSpinLock);

	//
	//	Get PDO and LUR information
	//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData
	//
	result = GetScsiAdapterPdoEnumInfo(HwDeviceExtension, ConfigInfo->SystemIoBusNumber, &AddTargetDataLength, &AddTargetData);

	if(result != SP_RETURN_FOUND) {
		*Again = FALSE;
		return result;
	}

	ntStatus = AddTargetDataDuringFindAdapter(
								HwDeviceExtension,
								AddTargetDataLength,
								AddTargetData
							);
	ExFreePoolWithTag(AddTargetData, LSMP_PTAG_IOCTL);
	if(!NT_SUCCESS(ntStatus)) {

		if(HwDeviceExtension->DisconEventToService)
			KeSetEvent(HwDeviceExtension->DisconEventToService, IO_NO_INCREMENT, FALSE);

		return SP_RETURN_ERROR;
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
	
	KDPrint(1,("MaximumTranferLength = 0x%x\n", ConfigInfo->MaximumTransferLength));

	if(ConfigInfo->MaximumTransferLength == SP_UNINITIALIZED_VALUE
		|| ConfigInfo->MaximumTransferLength > HwDeviceExtension->MaxBlocksPerRequest * BLOCK_SIZE)
		
		ConfigInfo->MaximumTransferLength = HwDeviceExtension->MaxBlocksPerRequest * BLOCK_SIZE;
	//
	//	Add one more page ( 4KB ) to prevent read/write I/O split.
	//	NOTE: It works best with 64KB of MaximumTransferLength.
	//	hootch	05122004
	ConfigInfo->MaximumTransferLength += 4096;

	KDPrint(1,("Set MaximumTranferLength = 0x%x\n", ConfigInfo->MaximumTransferLength));
	

	if((ConfigInfo->NumberOfPhysicalBreaks+1) == 0)
		ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_DESCRIPTORS - 1;

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

	ConfigInfo->Master;
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

#ifdef __ALLOCATE_CCB_FROM_POOL__
	ConfigInfo->SrbExtensionSize = 0;
#else
	ConfigInfo->SrbExtensionSize = sizeof(CCB);
#endif

	ConfigInfo->Dma64BitAddresses;
	ConfigInfo->ResetTargetSupported;
	ConfigInfo->MaximumNumberOfLogicalUnits = HwDeviceExtension->MaximumNumberOfLogicalUnits;
	ConfigInfo->WmiDataProvider = FALSE;                     
  
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
	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	KDPrint(1,("MaxBlocksPerRequest:%d DisconEventToService:%p AlarmEventToService:%p\n",
						HwDeviceExtension->MaxBlocksPerRequest,
						HwDeviceExtension->DisconEventToService,
						HwDeviceExtension->AlarmEventToService
		));

	UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);

	ScsiPortNotification(
		RequestTimerCall,
		HwDeviceExtension,
		MiniTimer,
		1000
		);

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
    )
{
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

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

	KeQueryTickCount(&HwDeviceExtension->LastBusResetTime);
	ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_BUSRESET_PENDING);

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	KDPrint(1,("set LastBusResetTime:%I64u\n", 
						HwDeviceExtension->LastBusResetTime.QuadPart));

	return STATUS_MORE_PROCESSING_REQUIRED;

}


//
//	control miniport
//
#define SUPPORT_TYPE_MAX 5

VOID
MiniStopAdapter(
	    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		IN	BOOLEAN						NotifyService
	) {
	LSMP_WORKITEM_CTX			WorkitemCtx;
	UINT32						AdapterStatus;
	KIRQL						oldIrql;

	KDPrint(1,("entered.\n"));

	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

	//
	//	Check to see if this is stopping for Power-saving.
	//
	if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING)) {

		ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING);
		AdapterStatus = ADAPTER_SETSTATUS(HwDeviceExtension, ADAPTER_STATUS_RUNNING);
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		KDPrint(1, ("Adapter waiting for power saving! Do not do the actual stop.\n"));
		UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);
		return;
	}

	if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING)) {

		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		KDPrint(1,("Error! stopping in progress.\n"));
		ASSERT(FALSE);
		return;
	}

	AdapterStatus = ADAPTER_SETSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING);

	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);

	//
	//	Call Stop routine by workitem.
	//	For now, we support one LUR for each miniport device.
	//
	if(NotifyService) {
		LSMP_INIT_WORKITEMCTX(
					&WorkitemCtx,
					LSMP_StopWorker,
					NULL,
					HwDeviceExtension->LURs[0],							// Arg1
					HwDeviceExtension->DisconEventToService,			// Arg2
					(PVOID)HwDeviceExtension->SlotNumber				// Arg3
				);
	} else {
		LSMP_INIT_WORKITEMCTX(
					&WorkitemCtx,
					LSMP_StopWorker,
					NULL,
					HwDeviceExtension->LURs[0],							// Arg1
					NULL,												// Arg2
					(PVOID)HwDeviceExtension->SlotNumber				// Arg3
				);
	}
	HwDeviceExtension->LURs[0] = NULL;
	LSMP_QueueMiniportWorker(HwDeviceExtension->ScsiportFdoObject,&WorkitemCtx);
}

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
	NTSTATUS							ntStatus;
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

        controlTypeList = Parameters;
        adjustedMaxControlType = (controlTypeList->MaxControlType < SUPPORT_TYPE_MAX) 
									? controlTypeList->MaxControlType : SUPPORT_TYPE_MAX;

        for (index = 0; index < adjustedMaxControlType; index++) {

            controlTypeList->SupportedTypeList[index] = supportedConrolTypes[index];

        }
		status = ScsiAdapterControlSuccess;
		KDPrint(1,("ScsiQuerySupportedControlTypes.\n"));
        break;

    case ScsiStopAdapter:
	{
		KDPrint(1,("ScsiStopAdapter.\n"));
		MiniStopAdapter(HwDeviceExtension, FALSE);
		break;
	}

    case ScsiRestartAdapter:

		KDPrint(1,("ScsiRestartAdapter.\n"));
		ntStatus = SendCcbToAllLURs(HwDeviceExtension, CCB_OPCODE_RESTART, NULL, NULL);
		if(!NT_SUCCESS(ntStatus)) {
			status = ScsiAdapterControlUnsuccessful;
		} else {
			status = ScsiAdapterControlSuccess;
		}
		break;

	case ScsiSetBootConfig:

		KDPrint(1,("ScsiSetBootConfig.\n"));
		status = ScsiAdapterControlUnsuccessful;
        break;

    case ScsiSetRunningConfig:

		KDPrint(1,("ScsiSetRunningConfig.\n"));
        status = ScsiAdapterControlUnsuccessful;
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

	switch(Srb->Cdb[0]) {
	case 0xd8:
//		ASSERT(FALSE);
		break;
	case SCSIOP_INQUIRY: {
		UINT32		AdapterStatus;

		//
		//	Clear ADAPTER_STATUSFLAG_POWERSAVING_PENDING if it is set.
		//
		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING)) {

			KDPrint(1,("default: Reset ADAPTER_STATUSFLAG_POWERSAVING_PENDING because a SCSI command is sent after STOP_UNIT_CODE\n"));
			AdapterStatus = ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING);
			RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

			UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);
		} else {
			RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		}

		//
		//	Block INQUIRY when LanscsiMiniport is stopping.
		//
		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING)) {
			KDPrint(1,("SCSIOP_INQUIRY: ADAPTER_STATUS_STOPPING. returned with error.\n"));
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
				LuExtension->LUR = LUR;
				KDPrint(1,("SCSIOP_INQUIRY: set LUR(%p) to LuExtension(%p)\n", LUR, LuExtension));

				ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
				UpdatePdoInfoInLSBus(HwDeviceExtension, HwDeviceExtension->AdapterStatus);
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
				break;
			}
		}

		if(LuExtension->LUR == NULL) {
			KDPrint(1,("LuExtension->LUR == NULL LuExtension(%p) HwDeviceExtension(%p), HwDeviceExtension->LURCount(%d)\n",
				LuExtension, HwDeviceExtension, HwDeviceExtension->LURCount));
			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			status = STATUS_SUCCESS;
		}
		break;
	}
	case SCSIOP_START_STOP_UNIT: {
		PCDB		cdb = (PCDB)(Srb->Cdb);
		UINT32		AdapterStatus;
			
		KDPrint(1,("SCSIOP_START_STOP_UNIT: Start:%d LoadEject:%d Immediate:%d LogicalUNitNumber:%d. PowerCond(MMC):%x\n",
								(int)cdb->START_STOP.Start,
								(int)cdb->START_STOP.LoadEject,
								(int)cdb->START_STOP.Immediate,
								(int)cdb->START_STOP.LogicalUnitNumber,
								(int)(Srb->Cdb[4]>>4)
								));

		if((int)cdb->START_STOP.LoadEject == 0) {

			ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
			if(cdb->START_STOP.Start == STOP_UNIT_CODE) {
				//
				//	Stop
				//
				KDPrint(1,("SCSIOP_START_STOP_UNIT: STOP_UNIT_CODE! set ADAPTER_STATUSFLAG_POWERSAVING_PENDING to the adapter.\n"));
				ASSERT(!ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING));
				AdapterStatus = ADAPTER_SETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING);
				RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

				UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);
			} else {
				//
				//	Restart
				//
				KDPrint(1,("SCSIOP_START_STOP_UNIT: START_UNIT_CODE! reset ADAPTER_STATUSFLAG_POWERSAVING_PENDING to the adapter.\n"));
				if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING)) {

					AdapterStatus = ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING);
					RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

					UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);
				} else {
					RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
					KDPrint(1,("SCSIOP_START_STOP_UNIT: START_UNIT_CODE! WARNING!!!!!: ADAPTER_STATUSFLAG_POWERSAVING_PENDING was not set.\n"));
				}

			}
		}

		
		break;
	}
	default: {
		UINT32		AdapterStatus;

		//
		//	Clear ADAPTER_STATUSFLAG_POWERSAVING_PENDING if it is set.
		//
		ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
		if(ADAPTER_ISSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING)) {

			KDPrint(1,("default: Reset ADAPTER_STATUSFLAG_POWERSAVING_PENDING because a SCSI command is sent after STOP_UNIT_CODE\n"));
			AdapterStatus = ADAPTER_RESETSTATUSFLAG(HwDeviceExtension, ADAPTER_STATUSFLAG_POWERSAVING_PENDING);
			RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

			UpdatePdoInfoInLSBus(HwDeviceExtension, AdapterStatus);
		} else {
			RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		}
		break;
		}
	}

	return status;
}

BOOLEAN
MiniStartIo(
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK			Srb
    )
{
//    PMINIPORT_DEVICE_EXTENSION	hwDeviceExtension = HwDeviceExtension;
	PMINIPORT_LU_EXTENSION		luExtension;
	PCCB						ccb;
	NTSTATUS					status;
	KIRQL						oldIrql;


	luExtension = ScsiPortGetLogicalUnit(
			HwDeviceExtension,
			Srb->PathId,
			Srb->TargetId,
			Srb->Lun
		);
		
	ASSERT(luExtension);

	//
	//	Completion
	//
	if(Srb->Function == SRB_FUNCTION_EXECUTE_SCSI
		&& Srb->Cdb[0] == SCSIOP_COMPLETE)
	{
		PSCSI_REQUEST_BLOCK	shippedSrb = (PSCSI_REQUEST_BLOCK)(Srb->DataBuffer);

		KDPrint(3,("SCSIOP_COMPLETE: SrbSeq=%d\n", (LONG)Srb->Cdb[1]));

		InterlockedDecrement(&HwDeviceExtension->RequestExecuting);

		ScsiPortNotification(
			RequestComplete,
			HwDeviceExtension,
			(PSCSI_REQUEST_BLOCK)(Srb->DataBuffer)
			);

		Srb->DataBuffer = NULL;
		Srb->SrbStatus = SRB_STATUS_SUCCESS;
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

	if(Srb->Function != SRB_FUNCTION_EXECUTE_SCSI) {
		KDPrint(1,("--->\n0x%x, Srb->Function = %s, ",
						Srb->Function, SrbFunctionCodeString(Srb->Function)));
		KDPrint(1,("Srb->PathId = %d, Srb->TargetId = %d Srb->Lun = %d\n",
						Srb->PathId, Srb->TargetId, Srb->Lun));
	}

	//
	//	check the adapter status
	//
	ACQUIRE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(ADAPTER_ISSTATUS(HwDeviceExtension, ADAPTER_STATUS_STOPPING)) {
		RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

		KDPrint(1,("Error! stopping in progress.\n"));

		Srb->SrbStatus = SRB_STATUS_ERROR;

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
	RELEASE_SPIN_LOCK(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	//
	//	Adapter dispatcher
	//
	switch(Srb->Function) {

	case SRB_FUNCTION_ABORT_COMMAND:
		KDPrint(1,("SRB_FUNCTION_ABORT_COMMAND: Srb = %p Srb->NextSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));
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

		status = MiniSrbControl(HwDeviceExtension, luExtension, Srb);
		if(status == STATUS_PENDING) {
			KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb pending.\n" ));
			return TRUE;
		} else if(status != STATUS_MORE_PROCESSING_REQUIRED) {

			//
			//	Complete the request.
			//
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

			KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb completed.\n" ));
			return TRUE;
		}
		KDPrint(1,("SRB_FUNCTION_IO_CONTROL: going to LUR\n" ));

	case SRB_FUNCTION_EXECUTE_SCSI:
		if(	Srb->Cdb[0] != SCSIOP_READ &&
			Srb->Cdb[0] != SCSIOP_WRITE &&
			Srb->Cdb[0] != SCSIOP_VERIFY )
		{

			KDPrint(1,("SRB_FUNCTION_EXECUTE_SCSI: %s(0x%02x): Srb = %p Srb->NextSrb = %p, Srb->CdbLength = %d\n",
												CdbOperationString(Srb->Cdb[0]),
												(int)Srb->Cdb[0],
												Srb,
												Srb->NextSrb,
												(UCHAR)Srb->CdbLength ));
		}

		status = MiniExecuteScsi(HwDeviceExtension, luExtension, Srb);
		if(status != STATUS_MORE_PROCESSING_REQUIRED) {

			KDPrint(1,("SRB_FUNCTION_EXECUTE_SCSI: Srb = %p completed.\n", Srb));
				//
				//	Complete the request.
				//
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

#if 0
	if(hwDeviceExtension->BusChangeDetected == FALSE) {
		if(Srb->Cdb[0] != SCSIOP_READ 
			&& Srb->Cdb[0] != SCSIOP_WRITE 
			&& Srb->Cdb[0] != SCSIOP_READ_CAPACITY)
			&& Srb->Cdb[0] !=SCSIOP_VERIFY) 
		{
			KDPrint(2,("SCSIOP = 0x%x %s hwDeviceExtension->RequestExecuting = %x\n",
							Srb->Cdb[0], CdbOperationString(Srb->Cdb[0]), 
							hwDeviceExtension->RequestExecuting));
			KDPrint(2,("hwDeviceExtension->AdapterError = %d\n",
							hwDeviceExtension->AdapterError));
		}
	} else {
		KDPrint(2,("SCSIOP = 0x%x %s hwDeviceExtension->RequestExecuting = %x\n",
							Srb->Cdb[0], CdbOperationString(Srb->Cdb[0]), 
							hwDeviceExtension->RequestExecuting));
		KDPrint(2,("hwDeviceExtension->AdapterError = %d\n",
							hwDeviceExtension->AdapterError));
	}

#endif

#ifdef __ALLOCATE_CCB_FROM_POOL__
	//	
	//	initialize Ccb in srb to call LUR dispatchers.
	//
	status = LSCcbAllocate(&ccb);
	if(!NT_SUCCESS(status)) {
		Srb->SrbStatus = SRB_STATUS_ERROR;
		KDPrint(1,("LSCcbAllocate() failed.\n" ));
		return TRUE;
	}
	LSCcbInitialize(
			Srb,
			HwDeviceExtension,
			ccb
		);
#else
	LSCcbInitializeInSrb(
			Srb,
			HwDeviceExtension,
			&ccb
		);
#endif
	InterlockedIncrement(&HwDeviceExtension->RequestExecuting);
	LSCcbSetCompletionRoutine(ccb, LanscsiMiniportCompletion, HwDeviceExtension);

	if(HwDeviceExtension->TimerOn == FALSE)
	{
		ScsiPortNotification(
			RequestTimerCall,
			HwDeviceExtension,
			MiniTimer,
			1
			);
		HwDeviceExtension->TimerOn = TRUE;
	}

	//
	//	Send a CCB to LURelation.
	//
	status = LurRequest(
					luExtension->LUR,
					ccb
				);

	if( NT_SUCCESS(status) ) {

		ScsiPortNotification(
				NextRequest,
				HwDeviceExtension,
				NULL
			);

	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
//	Device worker thread routines
//


VOID
LSMP_WorkerRoutine(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PLSMP_WORKITEM_CTX			LSMPWorkItemCtx
  ) {
	KDPrint(1,("entered.\n"));

	if(LSMPWorkItemCtx->WorkitemRoutine)
		LSMPWorkItemCtx->WorkitemRoutine(DeviceObject, LSMPWorkItemCtx);
	else {
		KDPrint(1,("No WorkitemRoutine.\n"));
	}

	IoFreeWorkItem(LSMPWorkItemCtx->WorkItem);
	ExFreePoolWithTag(LSMPWorkItemCtx, LSMP_PTAG_WORKCTX);
}


NTSTATUS
LSMP_QueueMiniportWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PLSMP_WORKITEM_CTX			TmpWorkitemCtx
	) {

	NTSTATUS			status;
	PLSMP_WORKITEM_CTX	WorkItemCtx;

	KDPrint(1,("entered.\n"));

	WorkItemCtx = (PLSMP_WORKITEM_CTX)ExAllocatePoolWithTag(NonPagedPool, sizeof(LSMP_WORKITEM_CTX), LSMP_PTAG_WORKCTX);
	if(!WorkItemCtx) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	WorkItemCtx->WorkitemRoutine = TmpWorkitemCtx->WorkitemRoutine;
	WorkItemCtx->Ccb = TmpWorkitemCtx->Ccb;
	WorkItemCtx->Arg1 = TmpWorkitemCtx->Arg1;
	WorkItemCtx->Arg2 = TmpWorkitemCtx->Arg2;
	WorkItemCtx->Arg3 = TmpWorkitemCtx->Arg3;

	WorkItemCtx->WorkItem = IoAllocateWorkItem(DeviceObject);
	if(WorkItemCtx->WorkItem == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	IoQueueWorkItem(
			WorkItemCtx->WorkItem,
			LSMP_WorkerRoutine,
			DelayedWorkQueue,
			WorkItemCtx
		);

	KDPrint(1,("queued work item!!!!!!\n"));

	return STATUS_SUCCESS;

cleanup:
	if(WorkItemCtx) {
		ExFreePoolWithTag(WorkItemCtx, LSMP_PTAG_WORKCTX);
	}

	return status;
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

		LSCCB_INITIALIZE(ccb);
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

	LSCCB_INITIALIZE(ccb);
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

	KDPrint(1,("entered.\n"));

	for(idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++ ) {

		ntStatus = LSCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("allocation fail.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb);
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

	KDPrint(1,("exit.\n"));

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
		LurClose(
				LURs[idx_lur]
			);
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

		ntStatus = LSCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb);
		ccb->OperationCode				= CCB_OPCODE_STOP;
		ccb->LurId[0]					= LURs[idx_lur]->LurId[0];
		ccb->LurId[1]					= LURs[idx_lur]->LurId[1];
		ccb->LurId[2]					= LURs[idx_lur]->LurId[2];
		ccb->HwDeviceExtension			= NULL;
		LSCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED);
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
		IN PDEVICE_OBJECT				DeviceObject,
		IN PLSMP_WORKITEM_CTX			LSMPWorkItemCtx
	) {
	NTSTATUS			ntStatus;
	PLURELATION			LUR;
	PKEVENT				DisconEvent;
	ULONG				SlotNo;
	BUSENUM_SETPDOINFO	BusSet;
	NTSTATUS			status;

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
	SlotNo = (ULONG)LSMPWorkItemCtx->Arg3;
	if(LUR == NULL) {
		KDPrint(1,("no LUR. return.\n"));

		goto send_discon;
	}

	//
	//	CCB_OPCODE_STOP must succeed.
	//
	ntStatus = SendStopCcbToAllLURsSync(1, &LUR);
	if(!NT_SUCCESS(ntStatus)) {
		ASSERT(FALSE);
	}

	FreeAllLURs(1, &LUR);

	//
	//	Update Status in LanscsiBus
	//
	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = ADAPTER_STATUS_STOPPED;
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

send_discon:
	//
	//	Set Disconnecting event.
	//
	if(DisconEvent)
		KeSetEvent(DisconEvent, IO_NO_INCREMENT, FALSE);
}



//
//	NoOperation worker.
//	Send NoOperation Ioctl to Miniport itself.
//
VOID
LSMP_NoOperationWorker(
		IN PDEVICE_OBJECT				DeviceObject,
		IN PLSMP_WORKITEM_CTX			LSMPWorkItemCtx
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
		LSMP_WORKITEM_CTX			WorkitemCtx;

		LSMP_INIT_WORKITEMCTX(
					&WorkitemCtx,
					LSMP_NoOperationWorker,
					NULL,
					(PVOID)Lur->LurId[0],
					(PVOID)Lur->LurId[1],
					(PVOID)Lur->LurId[2]
				);
		LSMP_QueueMiniportWorker(Lur->AdapterFdo, &WorkitemCtx);

//		KDPrint(1,("LURN_REQUEST_NOOP_EVENT: AdapterStatus %x.\n",
//									AdapterStatus));
	break;
	}

	default:
		KDPrint(1,("Invalid event class:%x\n", LurnEvent->LurnEventClass));
	break;
	}

}