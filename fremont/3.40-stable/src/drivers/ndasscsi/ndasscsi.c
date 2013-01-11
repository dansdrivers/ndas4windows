#include "port.h"


#if !__SCSIPORT__

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "NDASSCSI"

#endif

BOOLEAN	NdasTestBug = 1;

VOID
MiniUnload (
	IN PDRIVER_OBJECT DriverObject
	);

BOOLEAN
MiniHwInitialize (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
    );

ULONG
MiniHwFindAdapter (
    IN	PMINIPORT_DEVICE_EXTENSION		HwDeviceExtension,
    IN	PVOID							Context,
    IN	PVOID							BusInformation,
    IN	PCHAR							ArgumentString,
    IN	PPORT_CONFIGURATION_INFORMATION	ConfigInfo,
    OUT	PBOOLEAN						Again
    );

BOOLEAN
MiniHwStartIo (
	IN PVOID				HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK	Srb
	);

ULONG
EnumerateLURFromNDASBUS (
	IN	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN	BOOLEAN						Restart
	);

BOOLEAN
DetectShippingSRB (
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK			Srb
	);

SCSI_ADAPTER_CONTROL_STATUS
MiniHwAdapterControl (
    IN PVOID						HwDeviceExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE	ControlType,
    IN PVOID						Parameters
    );

VOID
MiniStopAdapter (
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN BOOLEAN						CallByPort
	);

VOID
MiniRestartAdapter (
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
	);

VOID
MiniQueueStopWorkItem (
	PNDAS_MINI_GLOBALS	NdscGlobals
);

VOID
MiniDriverThreadProc (
	PVOID StartContext
);

BOOLEAN
MiniHwResetBus (
	IN PVOID	HwDeviceExtension,
	IN ULONG	PathId
	);

VOID
MiniStopWorker (
		IN PDEVICE_OBJECT			DeviceObject,
		IN PNDSC_WORKITEM			NdscWorkItemCtx
	);

VOID
MiniRestartWorker (
		IN PDEVICE_OBJECT			DeviceObject,
		IN PNDSC_WORKITEM			NdscWorkItemCtx
	);

NTSTATUS
SendCcbToAllLURsSync (
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode
	);

NTSTATUS
SendCcbToAllLURs (
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode,
		CCB_COMPLETION_ROUTINE		CcbCompletionRoutine,
		PVOID						CompletionContext
	);

NTSTATUS
SendStopCcbToAllLURsSync (
	LONG		LURCount,
	PLURELATION	*LURs
	); 


//////////////////////////////////////////////////////////////////////////
//
//	Driver-wide variables
//
NDAS_MINI_GLOBALS NdasMiniGlobalData = {0};


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi Miniport Driver routines
//

#if !__SCSIPORT__

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT	DriverObject,
    IN PVOID			RegistryPath
    )

#else

NTSTATUS
MiniPortDriverEntry (
    IN PDRIVER_OBJECT	DriverObject,
    IN PUNICODE_STRING	RegistryPath
    )

#endif

{
	HW_INITIALIZATION_DATA	hwInitializationData;
	ULONG					isaStatus;
	NTSTATUS				status;

#if DBG
	
	DbgPrint( "\n************NdasScsi %s %s %s\n", __FUNCTION__, __DATE__, __TIME__ );

	NdasScsiDebugLevel |= NDASSCSI_DBG_MINIPORT_INFO;
	//NdasScsiDebugLevel |= NDASSCSI_DBG_LURN_IDE_INFO;
	//NdasScsiDebugLevel |= NDASSCSI_DBG_LUR_INFO;
	//NdasScsiDebugLevel |= NDASSCSI_DBG_TRANSPORT_INFO;
	
#endif

	// Get OS Version.

    NdasMiniGlobalData.CheckedVersion = 
		PsGetVersion( &NdasMiniGlobalData.MajorVersion, &NdasMiniGlobalData.MinorVersion, &NdasMiniGlobalData.BuildNumber, NULL );

    DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, 
				("Major Ver %d, Minor Ver %d, Build %d Checked:%d\n",
				NdasMiniGlobalData.MajorVersion, NdasMiniGlobalData.MinorVersion,
				NdasMiniGlobalData.BuildNumber, NdasMiniGlobalData.CheckedVersion) );

	KeInitializeSpinLock( &NdasMiniGlobalData.LurQSpinLock );
	InitializeListHead( &NdasMiniGlobalData.LurQueue );

	// Initialize the LUR system
	
	status = InitializeLurSystem (0);

	if (status != STATUS_SUCCESS) {

		return status;
	}

	// Initialize teh SCSI miniport global data.

	NdasMiniGlobalData.DriverObject = DriverObject;

	//	initialize Scsi miniport

	RtlZeroMemory( &hwInitializationData, sizeof(HW_INITIALIZATION_DATA) );

	hwInitializationData.HwInitializationDataSize = sizeof(hwInitializationData); 

	hwInitializationData.AdapterInterfaceType = Isa;

	hwInitializationData.HwInitialize		= MiniHwInitialize; 
	hwInitializationData.HwStartIo			= MiniHwStartIo; 
	hwInitializationData.HwInterrupt		= NULL; 
	hwInitializationData.HwFindAdapter		= MiniHwFindAdapter; 
	hwInitializationData.HwResetBus			= MiniHwResetBus; 
	hwInitializationData.HwDmaStarted		= NULL; 
	hwInitializationData.HwAdapterState		= NULL; 

	hwInitializationData.DeviceExtensionSize		= (INT32)sizeof(MINIPORT_DEVICE_EXTENSION); 
	hwInitializationData.SpecificLuExtensionSize	= sizeof(MINIPORT_LU_EXTENSION);
	hwInitializationData.SrbExtensionSize			= 0;
	hwInitializationData.NumberOfAccessRanges		= 0;
	hwInitializationData.Reserved					= 0;
	hwInitializationData.MapBuffers					= TRUE;
	hwInitializationData.NeedPhysicalAddresses		= FALSE;
	hwInitializationData.TaggedQueuing				= FALSE;

	// Not receive separate SCSIOP_REQUEST_SENSE from the SCSIPORT
	// when error reported by Miniport.
	// Instead, Miniport must fill sense information when reporting error.

	hwInitializationData.AutoRequestSense			= TRUE;

	hwInitializationData.MultipleRequestPerLu		= TRUE;

	hwInitializationData.ReceiveEvent				= FALSE;

	hwInitializationData.VendorIdLength				= 0;
	hwInitializationData.VendorId					= NULL;
	
	hwInitializationData.ReservedUshort				= 0;
	hwInitializationData.PortVersionFlags			= 0;
	
	hwInitializationData.DeviceIdLength				= 0;
	hwInitializationData.DeviceId					= NULL;

	hwInitializationData.HwAdapterControl	= MiniHwAdapterControl;

	isaStatus = ScsiPortInitialize( DriverObject, RegistryPath, &hwInitializationData, NULL );
	
	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("IsaBus type: NTSTATUS=%08lx.\n", isaStatus) );

	if (isaStatus != STATUS_SUCCESS) {

		NDAS_ASSERT(FALSE);

		DestroyLurSystem ();

		return isaStatus;
	}

	do {
		
		UNICODE_STRING		svcName;
		OBJECT_ATTRIBUTES	objectAttributes;

		//	Create Halt event for a driver-dedicated thread

		KeInitializeEvent( &NdasMiniGlobalData.WorkItemQueueEvent, NotificationEvent, FALSE );
		InitializeListHead( &NdasMiniGlobalData.WorkItemQueue );
		KeInitializeSpinLock( &NdasMiniGlobalData.WorkItemQueueSpinlock );

		//	Create a driver-dedicated thread
	
		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		status = PsCreateSystemThread( &NdasMiniGlobalData.DriverThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   MiniDriverThreadProc,
									   &NdasMiniGlobalData );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT(FALSE);

			NdasMiniGlobalData.DriverThreadHandle = NULL;
			NdasMiniGlobalData.DriverThreadObject = NULL;

			break;
		}

		status = ObReferenceObjectByHandle( NdasMiniGlobalData.DriverThreadHandle,
											FILE_READ_DATA,
											NULL,
											KernelMode,
											&NdasMiniGlobalData.DriverThreadObject,
											NULL );
		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT(FALSE);

			break;
		}

		//	Register TDI PnPPowerHandler

		RtlInitUnicodeString( &svcName, NDASSCSI_SVCNAME );

		status = LsuRegisterTdiPnPHandler( &svcName, 
										   LsuClientPnPBindingChange,
										   NULL,
										   NULL,
										   LsuClientPnPPowerChange, 
										   &NdasMiniGlobalData.TdiPnP );

		NDAS_ASSERT( status == STATUS_SUCCESS );
		break;
	
	} while (0);

	if (status == STATUS_SUCCESS) {

		//	Override driver unload routine.

		NdasMiniGlobalData.ScsiportUnload = DriverObject->DriverUnload;
		DriverObject->DriverUnload = MiniUnload;
	
	} else {

		MiniUnload( DriverObject );
	}

	return status;
}

VOID
MiniUnload (
	IN PDRIVER_OBJECT DriverObject
	)
{
	NTSTATUS	status;
	PLIST_ENTRY	listEntry;

#if DBG
	DbgPrint( "\n************NdasScsi %s %s %s\n", __FUNCTION__, __DATE__, __TIME__ );
#endif

	if (NdasMiniGlobalData.TdiPnP) {

		LsuDeregisterTdiPnPHandlers( NdasMiniGlobalData.TdiPnP );
	}

	if (NdasMiniGlobalData.DriverThreadHandle) {
			
		NDAS_ASSERT( NdasMiniGlobalData.DriverThreadObject );

		MiniQueueStopWorkItem( &NdasMiniGlobalData );

		if (NdasMiniGlobalData.DriverThreadObject) {

			status = KeWaitForSingleObject( NdasMiniGlobalData.DriverThreadObject,
											Executive,
											KernelMode,
											FALSE,
											NULL );
		
			NDAS_ASSERT( status == STATUS_SUCCESS );

			ObDereferenceObject( NdasMiniGlobalData.DriverThreadObject );
		}
	}

	while (listEntry = ExInterlockedRemoveHeadList(&NdasMiniGlobalData.LurQueue, &NdasMiniGlobalData.LurQSpinLock)) {

		PLURELATION	lur;

		
		lur = CONTAINING_RECORD( listEntry, LURELATION, ListEntry ); 

		status = SendStopCcbToAllLURsSync( 1, &lur );

		NDAS_ASSERT( NT_SUCCESS(status) );
		LurClose( lur );

		ObDereferenceObject( NdasMiniGlobalData.DriverObject );
	}

	DestroyLurSystem ();

	if (NdasMiniGlobalData.ScsiportUnload) {

		NdasMiniGlobalData.ScsiportUnload( DriverObject );
	}

#if __SCSIPORT__
	PortDriverUnload( DriverObject );
#endif

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("return2. DriverObject=%p\n", DriverObject) );

#if DBG
	DbgPrint( "\n************NdasScsi %s %s %s return\n", __FUNCTION__, __DATE__, __TIME__ );
#endif

	return;
}

// Initialize the miniport device extension
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.

BOOLEAN
MiniHwInitialize (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
    )
{
	KIRQL	oldIrql;
	UINT32	AdapterStatus;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("\n") );
	
	KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

	AdapterStatus = ADAPTER_SETSTATUS( HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_RUNNING );

	// Notify NdasBus to reset the adapter's status value.

	AdapterStatus |= NDASSCSI_ADAPTER_STATUSFLAG_RESETSTATUS;

	KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
				("AdapterMaxDataTransferLength:%x\n",
				 HwDeviceExtension->AdapterMaxDataTransferLength) );

	UpdatePdoInfoInLSBus( HwDeviceExtension, AdapterStatus );

	ScsiPortNotification( RequestTimerCall, HwDeviceExtension, MiniTimer, NDSC_TIMER_VALUE );

	return TRUE;
}

// find a scsi adapter on the bus.
//
// NOTE: This routine does NOT execute in the context of ScsiPort synchronized callback.

ULONG
MiniHwFindAdapter (
    IN	PMINIPORT_DEVICE_EXTENSION		HwDeviceExtension,
    IN	PVOID							Context,
    IN	PVOID							BusInformation,
    IN	PCHAR							ArgumentString,
    IN	PPORT_CONFIGURATION_INFORMATION	ConfigInfo,
    OUT	PBOOLEAN						Again
    )
{
	ULONG	result;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("in\n") );

	UNREFERENCED_PARAMETER( Context );
	UNREFERENCED_PARAMETER( BusInformation );
	UNREFERENCED_PARAMETER( ArgumentString );


	if (KeGetCurrentIrql() != PASSIVE_LEVEL) {

		NDAS_ASSERT(FALSE);		
		return SP_RETURN_NOT_FOUND;
	}

#if DBG
	NdasMiniLogError( HwDeviceExtension,
					  NULL,
					  0,
					  0,
					  0,
					  NDASSCSI_IO_FINDADAPTER_START,
					  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_START_FIND, 0) );
#endif

	//	Initialize MINIPORT_DEVICE_EXTENSION

	RtlZeroMemory( HwDeviceExtension, sizeof(MINIPORT_DEVICE_EXTENSION) );

	KeInitializeSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock );

	HwDeviceExtension->AdapterStatus				= NDASSCSI_ADAPTER_STATUS_INIT;

	HwDeviceExtension->AdapterStopInterval.QuadPart = NDASSCSI_DEFAULT_STOP_INTERVAL;

	HwDeviceExtension->NumberOfBuses				= 1;
	HwDeviceExtension->InitiatorId					= INITIATOR_ID;
	HwDeviceExtension->MaximumNumberOfTargets		= MAX_NR_LOGICAL_TARGETS + 1;	// add one for the initiator.
	HwDeviceExtension->MaximumNumberOfLogicalUnits	= MAX_NR_LOGICAL_LU;
	HwDeviceExtension->SlotNumber					= ConfigInfo->SystemIoBusNumber;
	HwDeviceExtension->ScsiportFdoObject			= FindScsiportFdo(HwDeviceExtension);
	HwDeviceExtension->AdapterMaxDataTransferLength	= NDAS_MAX_TRANSFER_LENGTH;

	HwDeviceExtension->EnabledTime.QuadPart = NdasCurrentTime().QuadPart;

	KeInitializeSpinLock( &HwDeviceExtension->CcbListSpinLock );

	InitializeListHead( &HwDeviceExtension->CcbList );
	InitializeListHead( &HwDeviceExtension->CcbCompletionList );

	LsuInitializeBlockAcl( &HwDeviceExtension->BackupUserBacl[0] );

	// Make NDASSCSI device PAGABLE.
	// NOTE: NDASBUS's PDO should also have PAGABLE flag.

	HwDeviceExtension->ScsiportFdoObject->Flags |= DO_POWER_PAGABLE;

	//	Get PDO and LUR information
	//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData

	NDAS_ASSERT( HwDeviceExtension->SlotNumber );

	result = EnumerateLURFromNDASBUS( HwDeviceExtension, FALSE );

	if (result != SP_RETURN_FOUND) {

		NDAS_ASSERT(FALSE);

		NdasMiniLogError( HwDeviceExtension,
						  NULL,
						  0,
						  0,
						  0,
						  NDASSCSI_IO_INITLUR_FAIL,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_ADD_LUR, 0) );

		UpdateStatusInLSBus( HwDeviceExtension->SlotNumber,
							 NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT | NDASSCSI_ADAPTER_STATUS_STOPPED );
		
		*Again = FALSE;
		
		return SP_RETURN_NOT_FOUND;
	}

	//	Set PORT_CONFIGURATION_INFORMATION

	ConfigInfo->Length;
	ConfigInfo->SystemIoBusNumber;
	ConfigInfo->AdapterInterfaceType;

	ConfigInfo->BusInterruptLevel;
	ConfigInfo->BusInterruptVector;
	ConfigInfo->InterruptMode;
	
	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
				("Default NumberOfPhysicalBreaks = 0x%x\n", ConfigInfo->NumberOfPhysicalBreaks) );

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
				("Default MaximumTranferLength = 0x%x\n", ConfigInfo->MaximumTransferLength) );

	NDAS_ASSERT( HwDeviceExtension->AdapterMaxDataTransferLength == NDAS_MAX_TRANSFER_LENGTH );

	//	If SCSI port does not specify the value,
	//	set from HwDeviceExtension
	
	if (ConfigInfo->MaximumTransferLength == SP_UNINITIALIZED_VALUE) {

		ConfigInfo->MaximumTransferLength = HwDeviceExtension->AdapterMaxDataTransferLength;

	} else {
		
		//	Choose smaller one
		
	if (HwDeviceExtension->AdapterMaxDataTransferLength > ConfigInfo->MaximumTransferLength) {

			HwDeviceExtension->AdapterMaxDataTransferLength = ConfigInfo->MaximumTransferLength;
		
		} else {

			ConfigInfo->MaximumTransferLength = HwDeviceExtension->AdapterMaxDataTransferLength;
		}
	}

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, 
				("Set MaximumTranferLength = 0x%x\n", ConfigInfo->MaximumTransferLength) );

	ConfigInfo->NumberOfPhysicalBreaks;

	ConfigInfo->DmaChannel;
	ConfigInfo->DmaPort;
	ConfigInfo->DmaWidth;
	ConfigInfo->DmaSpeed;

	ConfigInfo->AlignmentMask = 0x0;

	ConfigInfo->NumberOfAccessRanges;
	ConfigInfo->AccessRanges;
	ConfigInfo->Reserved;
	ConfigInfo->NumberOfBuses = HwDeviceExtension->NumberOfBuses;

	if ((ConfigInfo->InitiatorBusId[0]+1) == 0) {

		ConfigInfo->InitiatorBusId[0] = HwDeviceExtension->InitiatorId;
	}

	ConfigInfo->ScatterGather;

	// Windows XP/Server 2003 x64 does not allow non-master mini port device.
	// Additionally, NDAS bus driver should support the bus standard interface
	// to prevent 1 physical page limit.

	if (ConfigInfo->Dma64BitAddresses == SCSI_DMA64_SYSTEM_SUPPORTED) {
	
		ConfigInfo->Master = TRUE;
	}
	
	ConfigInfo->CachesData = TRUE;  // If CachesData is FALSE, SRB_FUNCTION_SHUTDOWN and SRB_FUNCTION_FLUSH is not passed to down to ndasscsi.
									// But we need this code for RAID because RAID stores meta data on disk and this meta data need to be updated when shutdown and flush
									// And RAID will use real caching when write-back cache is implemented.

	ConfigInfo->AdapterScansDown;
	ConfigInfo->AtdiskPrimaryClaimed;
	ConfigInfo->AtdiskSecondaryClaimed;
	ConfigInfo->Dma32BitAddresses;
	ConfigInfo->DemandMode;
	ConfigInfo->MapBuffers;
	ConfigInfo->NeedPhysicalAddresses;
	ConfigInfo->TaggedQueuing;
	
	ConfigInfo->AutoRequestSense;
	NDAS_ASSERT( ConfigInfo->AutoRequestSense );

	ConfigInfo->MultipleRequestPerLu;

	ConfigInfo->ReceiveEvent;
	ConfigInfo->RealModeInitialized;
	ConfigInfo->BufferAccessScsiPortControlled;
	ConfigInfo->MaximumNumberOfTargets = HwDeviceExtension->MaximumNumberOfTargets;
	ConfigInfo->ReservedUchars;
	ConfigInfo->SlotNumber;
	
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

	if (ConfigInfo->Dma64BitAddresses == SCSI_DMA64_SYSTEM_SUPPORTED) {

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("DMA64Bit on!\n") );

		ConfigInfo->Dma64BitAddresses = SCSI_DMA64_MINIPORT_SUPPORTED;
	}

	ConfigInfo->ResetTargetSupported;
	ConfigInfo->MaximumNumberOfLogicalUnits = HwDeviceExtension->MaximumNumberOfLogicalUnits;
	ConfigInfo->WmiDataProvider = FALSE;

#if DBG
	NdasMiniLogError( HwDeviceExtension,
					  NULL,
					  0,
					  0,
					  0,
					  NDASSCSI_IO_FINDADAPTER_SUCC,
					  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_SUCCEED_FIND, 0) );
#endif

	return SP_RETURN_FOUND;
}

//	StartIO
//	Receives all SRBs toward this adapter.
//
//  NOTE: This routine executes in the context of ScsiPort synchronized callback.

BOOLEAN
_MiniHwStartIo (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK			Srb
    )
{
	BOOLEAN					result;
	KIRQL					oldIrql;
	NTSTATUS				status;
	PMINIPORT_LU_EXTENSION	luExtension;
	PCCB					ccb;
	
	//	check the adapter status

	KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

	if (Srb->QueueTag == SP_UNTAGGED) {

		NDAS_ASSERT( Srb->Function == SRB_FUNCTION_EXECUTE_SCSI && 
					 (Srb->Cdb[0] == SCSIOP_INQUIRY || Srb->Cdb[0] == SCSIOP_TEST_UNIT_READY || Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE) );
	
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
					("Srb->QueueTag == SP_UNTAGGED Srb->Function = %x, %s(0x%02x) Srb(%p) CdbLen=%d\n",
					 Srb->Function,
					 CdbOperationString(Srb->Cdb[0]),
					 (int)Srb->Cdb[0],
					 Srb,
					 (UCHAR)Srb->CdbLength) );

		NDAS_ASSERT( HwDeviceExtension->CurrentUntaggedRequest == NULL );

		HwDeviceExtension->CurrentUntaggedRequest = Srb;
	}

	if (ADAPTER_ISSTATUS(HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_STOPPING) ||
		ADAPTER_ISSTATUSFLAG(HwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_RESTARTING)) {

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		if (ADAPTER_ISSTATUS(HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_STOPPING)) {

			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Error! stopping in progress due to internal error.\n") );
		
		} else {
		
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Delay the request due to reinit.\n"));
		}

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
					("Func:%x %s(0x%02x) Srb(%p) CdbLen=%d\n",
					 Srb->Function,
					 CdbOperationString(Srb->Cdb[0]),
					 (int)Srb->Cdb[0],
					 Srb,
					 (UCHAR)Srb->CdbLength) );

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
					("TxLen:%d TO:%d SenseLen:%d SrbFlags:%08lx\n",
					 Srb->DataTransferLength,
					 Srb->TimeOutValue,
					 Srb->SenseInfoBufferLength,
					 Srb->SrbFlags) );

		Srb->ScsiStatus = SCSISTAT_GOOD;
		
		if (ADAPTER_ISSTATUS(HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_STOPPING)) {

			Srb->DataTransferLength = 0;
			Srb->SrbStatus = SRB_STATUS_BUS_RESET;
		
		} else {
		
			Srb->SrbStatus = SRB_STATUS_BUSY;
		}

		//	Complete the SRB.

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {
				
			NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;
		
		} else {

			NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
		}

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

		return TRUE;
	}

	KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

	if (Srb->Function != SRB_FUNCTION_EXECUTE_SCSI) {
	
		DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE,
					("--->0x%x, Srb->Function = %s, Srb->PathId = %d, Srb->TargetId = %d Srb->Lun = %d\n",
					  Srb->Function, SrbFunctionCodeString(Srb->Function), Srb->PathId, Srb->TargetId, Srb->Lun) );
	}

	//	Adapter dispatcher

	switch (Srb->Function) {

	case SRB_FUNCTION_ABORT_COMMAND:
		
		NDAS_ASSERT( Srb->NextSrb );
		NDAS_ASSERT(FALSE);

		NdasMiniLogError( HwDeviceExtension,
						  Srb,
						  Srb->PathId,
						  Srb->TargetId,
						  Srb->Lun,
						  NDASSCSI_IO_ABORT_SRB,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ABORT_SRB_ENTERED, 0) );

		Srb->ScsiStatus = SCSISTAT_GOOD;
		Srb->SrbStatus = SRB_STATUS_ERROR;

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

			NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
		}

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

		return FALSE;

	case SRB_FUNCTION_RESET_BUS:

		NDAS_ASSERT(FALSE);

		result = MiniHwResetBus( HwDeviceExtension, Srb->PathId );

		Srb->ScsiStatus = SCSISTAT_GOOD;
		Srb->SrbStatus = SRB_STATUS_ERROR;

		//	Complete the request.

		NDAS_ASSERT( Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE );

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

			NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
		}

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

		return result;

	case SRB_FUNCTION_SHUTDOWN:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, 
					("SRB_FUNCTION_SHUTDOWN: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", 
					 Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength) );

		break; // Pass to lower LUR

	case SRB_FUNCTION_FLUSH: 

		DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE,
					("SRB_FUNCTION_FLUSH: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", 
					 Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength) );

		break;

	case SRB_FUNCTION_IO_CONTROL: {

		luExtension = ScsiPortGetLogicalUnit( HwDeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun );

		DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE,
					("SRB_FUNCTION_IO_CONTROL: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", 
					 Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength) );

		status = MiniSrbControl( HwDeviceExtension, luExtension, Srb );

		if (status == STATUS_PENDING) {
		
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("SRB_FUNCTION_IO_CONTROL: Srb pending.\n") );

			return TRUE;

		} else if (status != STATUS_MORE_PROCESSING_REQUIRED) {


			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("SRB_FUNCTION_IO_CONTROL: Srb completed.\n") );

			KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

			if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

				NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
				HwDeviceExtension->CurrentUntaggedRequest = NULL;

			} else {

				NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
			}

			KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

			ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

			return TRUE;
		}

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("SRB_FUNCTION_IO_CONTROL: going to LUR\n") );

		break;
	}

	case SRB_FUNCTION_EXECUTE_SCSI: {

		LONG		idx_lur;
		NTSTATUS	status;
		PLURELATION	lur;
		KIRQL		oldIrql;

		if (Srb->Cdb[0] == SCSIOP_WRITE || Srb->Cdb[0] == SCSIOP_WRITE16) {

			UINT64	logicalBlockAddress;
			ULONG	transferBlocks;

			LsCcbGetAddressAndLength( (PCDB)Srb->Cdb, &logicalBlockAddress, &transferBlocks );

			HwDeviceExtension->WriteCount.QuadPart ++;
			HwDeviceExtension->WriteBlocks.QuadPart += transferBlocks;

			DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE,
						("%05I64d: %07I64d, %s(%X,%d): %I64X, %u\n",
						 HwDeviceExtension->WriteCount.QuadPart, HwDeviceExtension->WriteBlocks.QuadPart,
						 CdbOperationString(Srb->Cdb[0]), Srb->Cdb[0], 
						 Srb->CdbLength, logicalBlockAddress, transferBlocks) );
		}

		if (Srb->Cdb[0] == SCSIOP_READ || Srb->Cdb[0] == SCSIOP_WRITE || Srb->Cdb[0] == SCSIOP_READ16 || Srb->Cdb[0] == SCSIOP_WRITE16) {

			UINT64	logicalBlockAddress;
			ULONG	transferBlocks;

			LsCcbGetAddressAndLength( (PCDB)Srb->Cdb, &logicalBlockAddress, &transferBlocks );

			if (Srb->DataTransferLength > (64*1024)) {

				DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE,
							("%p %s(%X,%d):%u,%u,%u Tx:%d S:%d To:%d F:%x\n",
							Srb,
							CdbOperationString(Srb->Cdb[0]),
							(int)Srb->Cdb[0],
							(UCHAR)Srb->CdbLength,
							((PCDB)Srb->Cdb)->CDB10.ForceUnitAccess,
							logicalBlockAddress,
							transferBlocks,
							Srb->DataTransferLength,
							Srb->SenseInfoBufferLength,
							Srb->TimeOutValue,
							Srb->SrbFlags) );
			}
		}
		
		if (BooleanFlagOn( Srb->SrbFlags,
						   SRB_FLAGS_DISABLE_DISCONNECT | SRB_FLAGS_BYPASS_FROZEN_QUEUE | SRB_FLAGS_BYPASS_LOCKED_QUEUE)) {

			UINT64	logicalBlockAddress;
			ULONG	transferBlocks;

			LsCcbGetAddressAndLength( (PCDB)Srb->Cdb, &logicalBlockAddress, &transferBlocks );
			
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
						("%p %s(%X,%d):%u,%u,%u Tx:%d S:%d To:%d F:%x\n",
						 Srb,
						 CdbOperationString(Srb->Cdb[0]),
						 (int)Srb->Cdb[0],
						 (UCHAR)Srb->CdbLength,
						 ((PCDB)Srb->Cdb)->CDB10.ForceUnitAccess,
						 logicalBlockAddress,
						 transferBlocks,
						 Srb->DataTransferLength,
						 Srb->SenseInfoBufferLength,
						 Srb->TimeOutValue,
						 Srb->SrbFlags) );

			NDAS_ASSERT( Srb->NextSrb == NULL );
		}

		switch (Srb->Cdb[0]) {

		case SCSIOP_INQUIRY: {

			luExtension = ScsiPortGetLogicalUnit( HwDeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun );

			//	Block INQUIRY when NDASSCSI is stopping.
	
			KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );
	
			if (ADAPTER_ISSTATUS(HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_STOPPING)) {

				NDAS_ASSERT(FALSE);

				NdasMiniLogError( HwDeviceExtension,
								  Srb,
								  Srb->PathId,
								  Srb->TargetId,
								  Srb->Lun,
								  NDASSCSI_IO_INQUIRY_WHILE_STOPPING,
								  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_INQUIRY_DURING_STOPPING, 0) );

				Srb->SrbStatus = SRB_STATUS_BUS_RESET;
				Srb->ScsiStatus = SCSISTAT_GOOD;
				status = STATUS_SUCCESS;
			}

			KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

			//	look up Logical Unit Relations and set it to LuExtension
	
			luExtension->LUR = NULL;
			lur = NULL;

			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
						("LHwDeviceExtension->LURCount = %d\n", HwDeviceExtension->LURCount) );

			for (idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++) {
			 
				lur = HwDeviceExtension->LURs[idx_lur];

				if (lur && lur->LurId[0] == Srb->PathId && lur->LurId[1] == Srb->TargetId && lur->LurId[2] == Srb->Lun) {

					DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("lur = %p, idx_lur = %d\n", lur, idx_lur) );
					break;
				}

				lur = NULL;
			}

			if (lur == NULL) {

				DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
							("LUR == NULL LuExtension(%p) HwDeviceExtension(%p), HwDeviceExtension->LURCount(%d)\n",
							 luExtension, HwDeviceExtension, HwDeviceExtension->LURCount) );
#if DBG
				NdasMiniLogError( HwDeviceExtension,
								  Srb,
								  Srb->PathId,
								  Srb->TargetId,
								  Srb->Lun,
								  NDASSCSI_IO_LUR_NOT_FOUND,
								  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_INQUIRY_LUR_NOT_FOUND, 0) );
#endif
				Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
				Srb->ScsiStatus = SCSISTAT_GOOD;

				DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("SRB_FUNCTION_EXECUTE_SCSI: Srb = %p completed.\n", Srb) );

				KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

				if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

					NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
					HwDeviceExtension->CurrentUntaggedRequest = NULL;

				} else {

					NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
				}

				KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

				ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

				return TRUE;
			}

			break;
		}

		default: 

			break;
		}

		break;
	}

	case SRB_FUNCTION_RESET_DEVICE:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("SRB_FUNCTION_RESET_DEVICE: CurrentSrb is Presented. Srb = %x\n", Srb) );

	default:

		NDAS_ASSERT(FALSE);

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
					("Invalid SRB Function:%d Srb = %x\n", Srb->Function,Srb) );

		Srb->SrbStatus  = SRB_STATUS_INVALID_REQUEST;
		Srb->ScsiStatus = SCSISTAT_GOOD;

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

			NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
		}

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

		return TRUE;
	}

	//	initialize Ccb in srb to call LUR dispatchers.

	status = LsCcbAllocate(&ccb);

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT(FALSE);

		NdasMiniLogError( HwDeviceExtension,
						  Srb,
						  Srb->PathId,
						  Srb->TargetId,
						  Srb->Lun,
						  NDASSCSI_IO_CCBALLOC_FAIL,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_CCB_ALLOCATION_FAIL, 0) );

		Srb->SrbStatus = SRB_STATUS_ERROR;
		Srb->ScsiStatus = SCSISTAT_GOOD;

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

			NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
		}

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

		return TRUE;
	}

	LsCcbInitialize( Srb, HwDeviceExtension, FALSE, ccb );


	LsuIncrementTdiClientInProgress();
	LsCcbSetCompletionRoutine( ccb, NdscAdapterCompletion, HwDeviceExtension );

	// Workaround for cache synchronization command failure while entering hibernation mode.

	if (Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE	|| Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE16 || Srb->Cdb[0] == SCSIOP_START_STOP_UNIT) {
		
	} else {

		NDAS_ASSERT( !FlagOn(Srb->SrbFlags, SRB_FLAGS_BYPASS_LOCKED_QUEUE) );
	}

	if (Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE	|| Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE16 || Srb->Cdb[0] == SCSIOP_START_STOP_UNIT) {

		// start/stop, and cache synchronization command while entering hibernation.

		if (FlagOn(Srb->SrbFlags, SRB_FLAGS_BYPASS_LOCKED_QUEUE)) {
		
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("SRB %p must succeedSrb->Cdb[0] = %s\n", Srb, CdbOperationString(Srb->Cdb[0])) );
			SetFlag( ccb->Flags, CCB_FLAG_MUST_SUCCEED );
		}
	}

	if (Srb->Cdb[0] == SCSIOP_START_STOP_UNIT) {

		PCDB cdb = (PCDB)(Srb->Cdb);

		if (cdb->START_STOP.Start == STOP_UNIT_CODE) {

			HwDeviceExtension->Stopping = TRUE;
		}

		if (cdb->START_STOP.Start == START_UNIT_CODE) {

			HwDeviceExtension->Stopping = FALSE;
		}
	}

	InitializeListHead( &ccb->ListEntryForMiniPort );
			
	ExInterlockedInsertTailList( &HwDeviceExtension->CcbList,
								 &ccb->ListEntryForMiniPort,
								 &HwDeviceExtension->CcbListSpinLock );
	
	status = LurRequest( HwDeviceExtension->LURs[0], ccb );

	//	If failure, SRB was not queued. Complete it with an error.

	if (status != STATUS_SUCCESS && status != STATUS_PENDING) {

		KIRQL	oldIrql;

		NDAS_ASSERT(FALSE);

		//	Free resources for the SRB.

		LsuDecrementTdiClientInProgress();

		KeAcquireSpinLock( &HwDeviceExtension->CcbListSpinLock, &oldIrql );
		RemoveEntryList( &ccb->ListEntryForMiniPort );
		KeReleaseSpinLock( &HwDeviceExtension->CcbListSpinLock, oldIrql );

		LsCcbFree(ccb);

		//	Complete the SRB with an error.

		Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
		Srb->ScsiStatus = SCSISTAT_GOOD;

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		if (HwDeviceExtension->CurrentUntaggedRequest == Srb) {

			NDAS_ASSERT( Srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( Srb->QueueTag != SP_UNTAGGED );
		}

		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );
	}

	return TRUE;
}

BOOLEAN
MiniHwStartIo (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK			Srb
    )
{
	BOOLEAN		result = FALSE;
	KIRQL		oldIrql;

	NDAS_ASSERT( !Srb->NextSrb );
	NDAS_ASSERT( Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE );
	NDAS_ASSERT( ScsiPortGetLogicalUnit(HwDeviceExtension, Srb->PathId, Srb->TargetId, Srb->Lun) );

	DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE, ("Srb = %p\n", Srb) );

	KeAcquireSpinLock( &HwDeviceExtension->CcbListSpinLock, &oldIrql );

	while (!IsListEmpty(&HwDeviceExtension->CcbCompletionList)) {

		PCCB				ccb;
		PSCSI_REQUEST_BLOCK	srb;
		PLIST_ENTRY			listEntry;

		listEntry = RemoveHeadList( &HwDeviceExtension->CcbCompletionList );

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntryForMiniPort );
		srb = ccb->Srb;
		
		NDAS_ASSERT( srb );
		NDAS_ASSERT( srb->SrbFlags & SRB_FLAGS_IS_ACTIVE );
		
		if (LsCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE)) {
			
			InsertHeadList( &HwDeviceExtension->CcbCompletionList, &ccb->ListEntryForMiniPort );
			break;
		}

		CcbStatusToSrbStatus( HwDeviceExtension, ccb, srb );
		LsCcbFree(ccb);

		LsuDecrementTdiClientInProgress();

		if (HwDeviceExtension->CurrentUntaggedRequest == srb) {

			NDAS_ASSERT( srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( srb->QueueTag != SP_UNTAGGED );
		}

		ScsiPortNotification( RequestComplete, HwDeviceExtension, srb );
	}

	KeReleaseSpinLock( &HwDeviceExtension->CcbListSpinLock, oldIrql );

	if (Srb->Function == SRB_FUNCTION_EXECUTE_SCSI	&& Srb->Cdb[0] == SCSIOP_COMPLETE &&
		Srb->DataBuffer && Srb->DataBuffer != HwDeviceExtension) {

		NDAS_ASSERT(FALSE);
	}

	if (Srb->Function == SRB_FUNCTION_EXECUTE_SCSI && Srb->Cdb[0] == SCSIOP_COMPLETE && Srb->DataBuffer == HwDeviceExtension) {

		NDAS_ASSERT( FlagOn(Srb->SrbFlags, SRB_FLAGS_BYPASS_FROZEN_QUEUE) );
		NDAS_ASSERT( FlagOn(Srb->SrbFlags, SRB_FLAGS_NO_QUEUE_FREEZE) );

		DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE, ("shippingSrbDetected\n") );

		Srb->SrbStatus = SRB_STATUS_SUCCESS;

		ScsiPortNotification( RequestComplete, HwDeviceExtension, Srb );

		result = TRUE;

		KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );
		HwDeviceExtension->CompletionIrpPosted = FALSE;
		KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

	} else {

		result = _MiniHwStartIo( HwDeviceExtension, Srb );
	}

	KeAcquireSpinLock( &HwDeviceExtension->CcbListSpinLock, &oldIrql );

	if (!IsListEmpty(&HwDeviceExtension->CcbList) || !IsListEmpty(&HwDeviceExtension->CcbCompletionList)) {

		ScsiPortNotification( RequestTimerCall, HwDeviceExtension, MiniTimer, NDSC_TIMER_VALUE );
	}

	ScsiPortNotification( NextRequest, HwDeviceExtension, NULL );

	if (HwDeviceExtension->CurrentUntaggedRequest == NULL) {

		if (HwDeviceExtension->LURs[0]) {

			ScsiPortNotification( NextLuRequest,
								  HwDeviceExtension, 
								  HwDeviceExtension->LURs[0]->LurId[0], 
								  HwDeviceExtension->LURs[0]->LurId[1], 
								  HwDeviceExtension->LURs[0]->LurId[2] );
		}
	}

	KeReleaseSpinLock( &HwDeviceExtension->CcbListSpinLock, oldIrql );

	return result;
}

// reset the bus by callback
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.

BOOLEAN
MiniHwResetBus (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN ULONG						PathId
    )
{
	NTSTATUS	status;


	UNREFERENCED_PARAMETER(PathId);

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,
				("PathId = %d KeGetCurrentIrql()= 0x%x\n", PathId, KeGetCurrentIrql()) );

	NDAS_ASSERT( PathId <= 0xFF );
	NDAS_ASSERT( SCSI_MAXIMUM_TARGETS_PER_BUS <= 0xFF );
	NDAS_ASSERT( SCSI_MAXIMUM_LOGICAL_UNITS <= 0xFF );

	NDAS_ASSERT( !(IsListEmpty(&HwDeviceExtension->CcbList) && IsListEmpty(&HwDeviceExtension->CcbCompletionList) &&
				   HwDeviceExtension->CompletionIrpPosted == FALSE) );

#if DBG

	// Log bus-reset error.

	NdasMiniLogError( HwDeviceExtension,
					  NULL,
					  0,
					  0,
					  0,
					  NDASSCSI_IO_BUSRESET_OCCUR,
					  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_BUSRESET_OCCUR, 0) );

#endif

	//	Send a CCB to the root of LURelation.

	status = SendCcbToAllLURs( HwDeviceExtension, CCB_OPCODE_RESETBUS, NULL, NULL );

	NDAS_ASSERT( status == STATUS_SUCCESS );

	//	Initiate timer again.

	ScsiPortNotification( RequestTimerCall, HwDeviceExtension, MiniTimer, NDSC_TIMER_VALUE );

	return FALSE;
}


// SCSI Miniport callback function to control Scsi Adapter
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.


SCSI_ADAPTER_CONTROL_STATUS
MiniHwAdapterControl (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE	ControlType,
    IN PVOID						Parameters
    )
{
	PSCSI_SUPPORTED_CONTROL_TYPE_LIST	controlTypeList;
    ULONG								adjustedMaxControlType;
	SCSI_ADAPTER_CONTROL_STATUS			status;
    ULONG								index;

    BOOLEAN supportedConrolTypes[ScsiAdapterControlMax] = { 
		
		TRUE,	// ScsiQuerySupportedControlTypes
        TRUE,	// ScsiStopAdapter
        TRUE,	// ScsiRestartAdapter
        TRUE,	// ScsiSetBootConfig
        TRUE	// ScsiSetRunningConfig  
	};

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, 
				("ControlType = %d KeGetCurrentIrql()= 0x%x\n",
				 ControlType, KeGetCurrentIrql()) );

    switch (ControlType) {

    case ScsiQuerySupportedControlTypes:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("ScsiQuerySupportedControlTypes.\n") );

        controlTypeList = Parameters;

        adjustedMaxControlType = (controlTypeList->MaxControlType < ScsiAdapterControlMax) ? 
									controlTypeList->MaxControlType : ScsiAdapterControlMax;

        for (index = 0; index < adjustedMaxControlType; index++) {

            controlTypeList->SupportedTypeList[index] = supportedConrolTypes[index];

        }

		status = ScsiAdapterControlSuccess;
        break;

    case ScsiStopAdapter: {

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("ScsiStopAdapter.\n") );
		
		MiniStopAdapter( HwDeviceExtension, TRUE );

		NDAS_ASSERT( IsListEmpty(&HwDeviceExtension->CcbList) && IsListEmpty(&HwDeviceExtension->CcbCompletionList) &&
					 HwDeviceExtension->CompletionIrpPosted == FALSE );
		
		status = ScsiAdapterControlSuccess;
		break;
	}

	case ScsiSetBootConfig:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("ScsiSetBootConfig.\n") );

		status = ScsiAdapterControlSuccess;
		break;

	case ScsiSetRunningConfig:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("ScsiSetRunningConfig.\n") );
		
		status = ScsiAdapterControlSuccess;
		break;

	case ScsiRestartAdapter:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("ScsiRestartAdapter.\n") );

		MiniRestartAdapter( HwDeviceExtension );

		status = ScsiAdapterControlSuccess;
		break;

    case ScsiAdapterControlMax:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("ScsiAdapterControlMax.\n") );

		status = ScsiAdapterControlUnsuccessful;
        break;
				
	default:

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("default:%d\n", ControlType) );

		status = ScsiAdapterControlUnsuccessful;
		break;
    }

    return status;
}

// Timer routine called by ScsiPort after the specified interval.
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.

VOID
MiniTimer (
    IN PMINIPORT_DEVICE_EXTENSION HwDeviceExtension
    )
{
	BOOLEAN	busChanged = FALSE;
	KIRQL	oldIrql;

	KeAcquireSpinLock( &HwDeviceExtension->CcbListSpinLock, &oldIrql );

	while (!IsListEmpty(&HwDeviceExtension->CcbCompletionList)) {

		PCCB				ccb;
		PSCSI_REQUEST_BLOCK	srb;
		PLIST_ENTRY			listEntry;

		listEntry = RemoveHeadList(&HwDeviceExtension->CcbCompletionList);

		ccb = CONTAINING_RECORD(listEntry, CCB, ListEntryForMiniPort);
		srb = ccb->Srb;
		
		NDAS_ASSERT( srb );
		NDAS_ASSERT( srb->SrbFlags & SRB_FLAGS_IS_ACTIVE );

		if (LsCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_TIMER_COMPLETE)) {

			DebugTrace( NDASSCSI_DBG_MINIPORT_TRACE, ("completing CCB:%p SCSIOP:%x\n", ccb, ccb->Cdb[0]) );
		}

		if (LsCcbIsStatusFlagOn(ccb, CCBSTATUS_FLAG_BUSCHANGE)) {
		
			busChanged = TRUE;
		}

		if (ccb->CcbStatus == CCB_STATUS_STOP || ccb->CcbStatus == CCB_STATUS_NOT_EXIST || ccb->CcbStatus == CCB_STATUS_COMMUNICATION_ERROR) {

			DebugTrace( NDASSCSI_DBG_MINIPORT_ERROR, 
						("Adapter stopped abnormally, HwDeviceExtension->AdapterStopTimeout.QuadPart = %I64d, NdasCurrentTime().QuadPart = %I64d\n",
						 HwDeviceExtension->AdapterStopTimeout.QuadPart, NdasCurrentTime().QuadPart) );

			if (HwDeviceExtension->AdapterStopTimeout.QuadPart == 0) {

				HwDeviceExtension->AdapterStopTimeout.QuadPart = NdasCurrentTime().QuadPart + HwDeviceExtension->AdapterStopInterval.QuadPart;
			}
		}

		CcbStatusToSrbStatus( HwDeviceExtension, ccb, srb );
		LsCcbFree(ccb);

		LsuDecrementTdiClientInProgress();

		if (HwDeviceExtension->CurrentUntaggedRequest == srb) {

			NDAS_ASSERT( srb->QueueTag == SP_UNTAGGED );
			HwDeviceExtension->CurrentUntaggedRequest = NULL;

		} else {

			NDAS_ASSERT( srb->QueueTag != SP_UNTAGGED );
		}

		ScsiPortNotification( RequestComplete, HwDeviceExtension, srb );
	}

	if (busChanged) {

		ScsiPortNotification( BusChangeDetected, HwDeviceExtension, NULL );
	}

	if (HwDeviceExtension->AdapterStopTimeout.QuadPart) {
		
		if (HwDeviceExtension->AdapterStopTimeout.QuadPart > NdasCurrentTime().QuadPart) {

			ScsiPortNotification( RequestTimerCall, HwDeviceExtension, MiniTimer, NDSC_TIMER_VALUE );
		
		} else {

			MiniStopAdapter( HwDeviceExtension, FALSE );
		}
	}

	if (!IsListEmpty(&HwDeviceExtension->CcbList)) {

		ScsiPortNotification( RequestTimerCall, HwDeviceExtension, MiniTimer, NDSC_TIMER_VALUE );
	}

	ScsiPortNotification( NextRequest, HwDeviceExtension, NULL );

	if (NdasMiniGlobalData.MajorVersion == NT_MAJOR_VERSION && NdasMiniGlobalData.MinorVersion == W2K_MINOR_VERSION) {

	} else {

		if (HwDeviceExtension->CurrentUntaggedRequest == NULL) {

			if (HwDeviceExtension->LURs[0]) {

				ScsiPortNotification( NextLuRequest,
									  HwDeviceExtension, 
									  HwDeviceExtension->LURs[0]->LurId[0], 
									  HwDeviceExtension->LURs[0]->LurId[1], 
									  HwDeviceExtension->LURs[0]->LurId[2] );
			}
		}
	}

	KeReleaseSpinLock( &HwDeviceExtension->CcbListSpinLock, oldIrql );

	return;
}


//////////////////////////////////////////////////////////////////////////
//
//	Driver-dedicated work item manipulation
//

NTSTATUS
MiniQueueWorkItem(
		IN PNDAS_MINI_GLOBALS	NdscGlobals,
		IN PDEVICE_OBJECT		DeviceObject,
		IN PNDSC_WORKITEM_INIT	NdscWorkItemInit
	) {

	PNDSC_WORKITEM	workItem;

	KDPrint(3,("entered.\n"));


	//
	//	Allocate work item
	//

	workItem = (PNDSC_WORKITEM)ExAllocatePoolWithTag(NonPagedPool, sizeof(NDSC_WORKITEM), NDSC_PTAG_WORKITEM);
	if(!workItem) {
		ASSERT(FALSE);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Increase device object reference.
	// It will be dereferenced by work routine or cleanup routine.
	//

	if(DeviceObject)
		ObReferenceObject(DeviceObject);


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

	KDPrint(3,("queued work item!!!!!!\n"));

	return STATUS_SUCCESS;
}


VOID
MiniQueueStopWorkItem(
	PNDAS_MINI_GLOBALS	NdscGlobals
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
	PNDAS_MINI_GLOBALS	NdscGlobals
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
	PNDAS_MINI_GLOBALS	NdscGlobals
){
	PNDSC_WORKITEM	workItem;

	while(TRUE)	{
		workItem = MiniDequeueWorkItem(NdscGlobals);
		if(workItem == NULL) {
			break;
		}

		//
		// Dereference the device object.
		//

		if(workItem->DeviceObject)
			ObDereferenceObject(workItem->DeviceObject);
		ExFreePoolWithTag(workItem, NDSC_PTAG_WORKITEM);
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("Canceled work item %p\n", workItem));
		ASSERT(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
//
//	Driver-dedicated thread
//

NTSTATUS
MiniExecuteWorkItem(
	PNDAS_MINI_GLOBALS	NdscGlobals,
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
		// Dereference the device object
		//

		if(workItem->DeviceObject)
			ObDereferenceObject(workItem->DeviceObject);

		//
		// Free a work item
		//

		ExFreePoolWithTag(workItem, NDSC_PTAG_WORKITEM);


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
	PNDAS_MINI_GLOBALS	ndscGlobals = (PNDAS_MINI_GLOBALS)StartContext;
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

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Terminating the worker thread.\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
}

//////////////////////////////////////////////////////////////////////////
//
//	Miniport callbacks
//

//
//	Create LUR object with LUR descriptor
//

static
NTSTATUS
AddLurDescDuringFindAdapter (
    IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN LONG							LurDescLength,
	IN PLURELATION_DESC				LurDesc
	) 
{
	PLURELATION	Lur;
	NTSTATUS	status;
	LONG		LURCount;
	BOOLEAN		w2kReadOnlyPatch;

	UNREFERENCED_PARAMETER( LurDescLength );

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("SIZE_OF_LURELATION_DESC() = %d\n", SIZE_OF_LURELATION_DESC()) );
	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("SIZE_OF_LURELATION_NODE_DESC(0) = %d\n", SIZE_OF_LURELATION_NODE_DESC(0)) );

	//	If the OS is Windows 2K, request read-only patch.

	if (LurDesc->DeviceMode == DEVMODE_SHARED_READONLY &&
		NdasMiniGlobalData.MajorVersion == NT_MAJOR_VERSION && NdasMiniGlobalData.MinorVersion == W2K_MINOR_VERSION) {

		w2kReadOnlyPatch = TRUE;

	} else {
		
		w2kReadOnlyPatch = FALSE;
	}

	//	Create an LUR with LUR Desc

	status = LurCreate( LurDesc,
					    HwDeviceExtension->LURSavedSecondaryFeature[0],
					    w2kReadOnlyPatch,
					    HwDeviceExtension->ScsiportFdoObject,
					    HwDeviceExtension,
					    MiniLurnCallback,
					    &Lur );

	if (NT_SUCCESS(status)) {

		ExInterlockedInsertTailList( &NdasMiniGlobalData.LurQueue, &Lur->ListEntry, &NdasMiniGlobalData.LurQSpinLock );
		ObReferenceObject( NdasMiniGlobalData.DriverObject );

		LURCount = InterlockedIncrement( &HwDeviceExtension->LURCount );

		//	We support only one LUR for now.

		NDAS_ASSERT( LURCount == 1 );

		HwDeviceExtension->LURs[0] = Lur;
	}

	return status;
}

//
//	Get PDO and LUR information
//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData
//
//	Parameter:	HwDeviceExtension - the adapter's miniport extension
//				DoNotAdjustAccessRight - Forcedly disable automatic access-mode change to NDAS device
//				FirstInstallation - Set if this is the first-time device installation
//

ULONG
EnumerateLURFromNDASBUS (
	IN	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN	BOOLEAN						Restart
	)
{
	NTSTATUS	ntStatus;
	ULONG		result;
	PVOID		AddDevInfo;
	LONG		AddDevInfoLength;
	ULONG		AddDevInfoFlag;

	if (HwDeviceExtension == NULL) {

		return SP_RETURN_NOT_FOUND;
	}

	//	Get PDO and LUR information
	//	GetScsiAdapterPdoEnumInfo() allocates a memory block for AddTargetData

	result = GetScsiAdapterPdoEnumInfo( HwDeviceExtension,
										HwDeviceExtension->SlotNumber,
										&AddDevInfoLength,
										&AddDevInfo,
										&AddDevInfoFlag );


	if (result != SP_RETURN_FOUND) {

		NDAS_ASSERT(FALSE);

		NdasMiniLogError( HwDeviceExtension,
						  NULL,
						  0,
						  0,
						  0,
						  NDASSCSI_IO_ADAPTERENUM_FAIL,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_GET_ENUM_INFO, 0) );

		return SP_RETURN_NOT_FOUND;
	}

	//	If this is the first enumeration,
	//	Return success, but do not enumerate any device.

	if (FlagOn(AddDevInfoFlag, PDOENUM_FLAG_DRV_NOT_INSTALLED)) {

#if DBG
		NdasMiniLogError( HwDeviceExtension,
						  NULL,
						  0,
						  0,
						  0,
						  NDASSCSI_IO_FIRSTINSTALL,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FIRST_TIME_INSTALL, 0) );
#endif

		ExFreePoolWithTag( AddDevInfo, NDSC_PTAG_ENUMINFO );
		
		return SP_RETURN_FOUND;
	}

	//	Create a LURN

	if (!FlagOn(AddDevInfoFlag, PDOENUM_FLAG_LURDESC)) {

		NDAS_ASSERT(FALSE);
		ExFreePoolWithTag( AddDevInfo, NDSC_PTAG_ENUMINFO );

		return SP_RETURN_NOT_FOUND;
	}

#if DBG
	NdasMiniLogError( HwDeviceExtension,
					  NULL,
					  0,
					  0,
					  0,
					  NDASSCSI_IO_RECV_LURDESC,
					  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_DETECT_LURDESC, 0) );
#endif

	if (Restart) {

		SetFlag( ((PLURELATION_DESC)AddDevInfo)->LurOptions, LUROPTION_DONOT_ADJUST_PRIM_SEC_ROLE );
		SetFlag( ((PLURELATION_DESC)AddDevInfo)->LurOptions, LUROPTION_REDUCE_TIMEOUT );
	}

	ntStatus = AddLurDescDuringFindAdapter( HwDeviceExtension, AddDevInfoLength, AddDevInfo );

	ExFreePoolWithTag( AddDevInfo, NDSC_PTAG_ENUMINFO );
	
	if (!NT_SUCCESS(ntStatus)) {
	
		NDAS_ASSERT( Restart == TRUE || FALSE );
		return SP_RETURN_NOT_FOUND;
	}

	// Increase TDI client count.
	// Decrease TDI client count when the device stops.

	LsuIncrementTdiClientDevice();

	return SP_RETURN_FOUND;
}







//////////////////////////////////////////////////////////////////////////
//
//	control miniport
//
//////////////////////////////////////////////////////////////////////////

//
// Clean up leftover SRBs in the timer completion queue.
// Only called by MiniStopAdapter() in the context of ScsiPort synchronized callback.
//

VOID
MiniCompleteLeftOverCcb (
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
	)
{
	PLIST_ENTRY			listEntry;
	PCCB				ccb;
	PSCSI_REQUEST_BLOCK	srb;

	do {

		listEntry = ExInterlockedRemoveHeadList( &HwDeviceExtension->CcbCompletionList, &HwDeviceExtension->CcbListSpinLock );

		if (listEntry == NULL) {

			break;
		}

		// Leftover should not exist.

		NDAS_ASSERT(FALSE);

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntryForMiniPort );
		srb = ccb->Srb;

		NDAS_ASSERT(srb);

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("completed SRB:%p SCSIOP:%x\n", srb, srb->Cdb[0]) );

		//	Free a CCB

		LsCcbFree(ccb);

		if (srb) {

			KIRQL oldIrql;

			LsuDecrementTdiClientInProgress();

			ASSERT(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);

			KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

			if (HwDeviceExtension->CurrentUntaggedRequest == srb) {

				NDAS_ASSERT( srb->QueueTag == SP_UNTAGGED );
				HwDeviceExtension->CurrentUntaggedRequest = NULL;
			}

			KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

			ScsiPortNotification( RequestComplete, HwDeviceExtension, srb );
		}

	} while (1);
}

//
// Stop the adapter
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback
//			when CallByPort set TRUE.
//       Also executes in the context of arbitrary system thread
//			when CallByPort set FALSE. We regard it as a abnormal stop.
//


VOID
MiniStopAdapter (
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	IN BOOLEAN						CallByPort
	) 
{
	NDSC_WORKITEM_INIT	WorkitemCtx;
	UINT32				AdapterStatus;
	KIRQL				oldIrql;
	PLURELATION			tempLUR;

#if DBG
	NdasMiniLogError( HwDeviceExtension,
					  NULL,
					  0,
					  0,
					  0,
					  NDASSCSI_IO_STOP_REQUESTED,
					  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ADAPTER_STOP, 0) );
#endif

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n") );

	KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

	if (ADAPTER_ISSTATUS(HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_STOPPING)) {

		KeReleaseSpinLock(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Already stopping in progress.\n"));

		NdasMiniLogError( HwDeviceExtension,
						  NULL,
						  0,
						  0,
						  0,
						  NDASSCSI_IO_STOPIOCTL_WHILESTOPPING,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_STOP_DURING_STOPPING, 0) );
		return;
	}

	AdapterStatus = ADAPTER_SETSTATUS( HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_STOPPING );

	// Set abnormal termination flag if the caller is not SCSI port.

	if (!CallByPort) {
	
		AdapterStatus = ADAPTER_SETSTATUSFLAG(HwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT);
	}

#if DBG
	if (HwDeviceExtension->LURs[0] == NULL) {

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("No LUR is available.\n") );

		NdasMiniLogError( HwDeviceExtension,
						  NULL,
						  0,
						  0,
						  0,
						  NDASSCSI_IO_STOPIOCTL_INVALIDLUR,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_STOP_WITH_INVALIDLUR, 0) );
	}
#endif

	//	Save LURs and LURs's granted access mode and then 
	//	set NULL to the original.
	//	For now, we support one LUR for each miniport device.

	tempLUR = HwDeviceExtension->LURs[0];
	HwDeviceExtension->LURs[0] = NULL;
	InterlockedDecrement(&HwDeviceExtension->LURCount);

	if (tempLUR) {
	
		HwDeviceExtension->LURSavedSecondaryFeature[0] = (tempLUR->EnabledNdasFeatures & NDASFEATURE_SECONDARY) != 0;

		// Set NULL to the LUR's AdapterHardwareExtension to prevent callback.

		tempLUR->AdapterHardwareExtension = NULL;
	}

	KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

	// Call Stop routine by work-item.
	// Clean up non-completed CCBs
	// Get CCB to be completed.
	// If this gets called by other than the port driver,
	// we regard it as abnormal termination.

	if (CallByPort) {

		NDAS_ASSERT( IsListEmpty(&HwDeviceExtension->CcbList) && IsListEmpty(&HwDeviceExtension->CcbCompletionList) &&
					 HwDeviceExtension->CompletionIrpPosted == FALSE );

		MiniCompleteLeftOverCcb(HwDeviceExtension);
	}

	NDSC_INIT_WORKITEM( &WorkitemCtx,
						MiniStopWorker,
						NULL,
						tempLUR,									// Arg1: Target LUR
						(PVOID)(ULONG_PTR)(!CallByPort),			// Arg2: Abnormal stop
						UlongToPtr(HwDeviceExtension->SlotNumber) );// Arg3: SlotNumber for BUS IOCTL

	MiniQueueWorkItem( &NdasMiniGlobalData, HwDeviceExtension->ScsiportFdoObject, &WorkitemCtx );
}


//
// Restart an SCSI adapter
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.
//

VOID
MiniRestartAdapter (
	IN PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension
	) 
{
	NDSC_WORKITEM_INIT	WorkitemCtx;
	UINT32				AdapterStatus;
	KIRQL				oldIrql;

#if DBG
	NdasMiniLogError( HwDeviceExtension,
					  NULL,
					  0,
					  0,
					  0,
					  NDASSCSI_IO_STOP_REQUESTED,
					  EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ADAPTER_STOP, 0) );
#endif

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n") );

	KeAcquireSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

	if (ADAPTER_ISSTATUSFLAG(HwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT)) {

		KeReleaseSpinLock(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO, ("Abnormal stop in progress. Can not re-init the LUR.\n") );

		return;
	}

	if (ADAPTER_ISSTATUSFLAG(HwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_RESTARTING)) {

		KeReleaseSpinLock(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Already reinitialization in progress.\n") );

		return;
	}

	// Set adapter status to running with restarting flag.
	// Restarting flags will delay SRBs by returning busy status.

	ADAPTER_SETSTATUS(HwDeviceExtension, NDASSCSI_ADAPTER_STATUS_RUNNING);
	AdapterStatus = ADAPTER_SETSTATUSFLAG(HwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_RESTARTING);

	if (HwDeviceExtension->LURs[0] != NULL) {

		NDAS_ASSERT(FALSE);

		KeReleaseSpinLock(&HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("LUR[0] already exists.\n") );

		return;
	}

	//	Initiate timer again.

	ScsiPortNotification( RequestTimerCall, HwDeviceExtension, MiniTimer, NDSC_TIMER_VALUE );

	HwDeviceExtension->Stopping = FALSE;
	HwDeviceExtension->AdapterStopTimeout.QuadPart = 0;

	KeReleaseSpinLock( &HwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

	//	Call Restart routine by workitem.

	NDSC_INIT_WORKITEM( &WorkitemCtx,
						MiniRestartWorker,
						NULL,
						HwDeviceExtension,								// Arg1
						NULL,											// Arg2
						UlongToPtr(HwDeviceExtension->SlotNumber) );	// Arg3

	MiniQueueWorkItem( &NdasMiniGlobalData, HwDeviceExtension->ScsiportFdoObject, &WorkitemCtx );
}

//////////////////////////////////////////////////////////////////////////
//
//	LU Relation operation
//

#if 0

NTSTATUS
SendCcbToAllLURsSync(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		UINT32						CcbOpCode
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n"));

	for(idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++ ) {

		ntStatus = LsCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb);
		ccb->OperationCode				= CcbOpCode;
		ccb->LurId[0]					= HwDeviceExtension->LURs[idx_lur]->LurId[0];
		ccb->LurId[1]					= HwDeviceExtension->LURs[idx_lur]->LurId[1];
		ccb->LurId[2]					= HwDeviceExtension->LURs[idx_lur]->LurId[2];
		ccb->HwDeviceExtension			= HwDeviceExtension;

		LsCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
		LsCcbSetCompletionRoutine(ccb, NULL, NULL);

		//
		//	Send a CCB to the root of LURelation.
		//
		ntStatus = LurRequest(
				HwDeviceExtension->LURs[idx_lur],
				ccb
			);
		if(!NT_SUCCESS(ntStatus)) {
			LsCcbPostCompleteCcb(ccb);
			return ntStatus;
		}
	}

	return STATUS_SUCCESS;
}

#endif


NTSTATUS
SendCcbToLURSync(
		PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
		PLURELATION		LUR,
		UINT32			CcbOpCode
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n"));

	ntStatus = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(ntStatus)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LsCcbInitialize( NULL, NULL, FALSE, ccb );
	ccb->OperationCode				= CcbOpCode;
	ccb->LurId[0]					= LUR->LurId[0];
	ccb->LurId[1]					= LUR->LurId[1];
	ccb->LurId[2]					= LUR->LurId[2];
	ccb->HwDeviceExtension			= HwDeviceExtension;
	LsCcbSetFlag( ccb, CCB_FLAG_SYNCHRONOUS );
	LsCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	//	Send a CCB to the root of LURelation.
	//
	ntStatus = LurRequest(
			LUR,
			ccb
		);
	if (!NT_SUCCESS(ntStatus)) {

		LsCcbFree(ccb);
		return ntStatus;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
SendCcbToAllLURs (
	PMINIPORT_DEVICE_EXTENSION	HwDeviceExtension,
	UINT32						CcbOpCode,
	CCB_COMPLETION_ROUTINE		CcbCompletionRoutine,
	PVOID						CompletionContext
	) 
{
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	KDPrint( 4, ("entered.\n") );

	for (idx_lur = 0; idx_lur < HwDeviceExtension->LURCount; idx_lur ++ ) {

		ntStatus = LsCcbAllocate(&ccb);
		
		if (!NT_SUCCESS(ntStatus)) {

			NDAS_ASSERT(FALSE);
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("allocation fail.\n") );

			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LsCcbInitialize( NULL, NULL, FALSE, ccb );

		ccb->OperationCode		= CcbOpCode;
		ccb->LurId[0]			= HwDeviceExtension->InitiatorId;
		ccb->LurId[1]			= 0;
		ccb->LurId[2]			= 0;
		ccb->HwDeviceExtension	= HwDeviceExtension;

		LsCcbSetCompletionRoutine( ccb, CcbCompletionRoutine, CompletionContext );

		//	Send a CCB to the LUR.

		ntStatus = LurRequest( HwDeviceExtension->LURs[idx_lur], ccb );

		if (!NT_SUCCESS(ntStatus)) {
		
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("request fail.\n") );
			LsCcbFree(ccb);

			return ntStatus;
		}
	}

	KDPrint( 4,("exit.\n") );

	return STATUS_SUCCESS;
}

#if 0

NTSTATUS
FreeAllLURs(
		LONG			LURCount,
		PLURELATION		*LURs
	) {
	LONG		idx_lur;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n"));

	for(idx_lur = 0; idx_lur < LURCount; idx_lur ++ ) {

		//
		//	call destroy routines for LURNs
		//
		if(LURs[idx_lur])
			LurClose(LURs[idx_lur]);
	}

	return STATUS_SUCCESS;
}

#endif

//
//	Send Stop Ccb to all LURs.
//
NTSTATUS
SendStopCcbToAllLURsSync (
	LONG		LURCount,
	PLURELATION	*LURs
	) 
{
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n") );

	for(idx_lur = 0; idx_lur < LURCount; idx_lur ++ ) {

		if(LURs[idx_lur] == NULL) {
			continue;
		}

		ntStatus = LsCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LsCcbInitialize( NULL, NULL, FALSE, ccb );

		ccb->OperationCode				= CCB_OPCODE_STOP;
		ccb->LurId[0]					= LURs[idx_lur]->LurId[0];
		ccb->LurId[1]					= LURs[idx_lur]->LurId[1];
		ccb->LurId[2]					= LURs[idx_lur]->LurId[2];
		ccb->HwDeviceExtension			= NULL;
		LsCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_RETRY_NOT_ALLOWED);
		LsCcbSetCompletionRoutine(ccb, NULL, NULL);

		//
		//	Send a CCB to the root of LURelation.
		//
		ntStatus = LurRequest(
				LURs[idx_lur],
				ccb
			);
		//
		// Must not return pending for this synchronous request
		//
		ASSERT(ntStatus != STATUS_PENDING);
		if(!NT_SUCCESS(ntStatus)) {
			LsCcbFree(ccb);
			DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("LurRequest() failed\n"));
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
MiniStopWorker (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PNDSC_WORKITEM	NdscWorkItemCtx
	) 
{
	NTSTATUS			ntStatus;
	PLURELATION			LUR;
	BOOLEAN				abnormalTermination;
	ULONG				SlotNo;
	NDASBUS_SETPDOINFO	BusSet;
	NTSTATUS			status;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n"));
	UNREFERENCED_PARAMETER(DeviceObject);

#if 0 // DBG
	{
		LARGE_INTEGER	interval;

		interval.QuadPart = - 10000000 * 10;
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
#endif

	LUR = (PLURELATION)NdscWorkItemCtx->Arg1;
	abnormalTermination = (BOOLEAN)NdscWorkItemCtx->Arg2;
	SlotNo = PtrToUlong(NdscWorkItemCtx->Arg3);
	if(LUR) {
		//
		//	CCB_OPCODE_STOP must succeed.
		//	NOTE: If LURs are more than one, do not forget dereference
		//			the driver object for each LUR
		//

		ntStatus = SendStopCcbToAllLURsSync(1, &LUR);
		ASSERT(NT_SUCCESS(ntStatus));

		RemoveEntryList( &LUR->ListEntry );
		LurClose(LUR);

		ObDereferenceObject( NdasMiniGlobalData.DriverObject );
	}
	else {
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("======> no LUR\n"));
		return;
	}

	//
	// Decrease TDI client device count.
	//

	LsuDecrementTdiClientDevice();

	//
	//	Update Status in LanscsiBus
	//	This code does not use HwDeviceExtension to be independent.
	//	If disconnection event is available, we regard it as a abnormal unplug.
	//
	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = NDASSCSI_ADAPTER_STATUS_STOPPED;
	BusSet.SupportedFeatures = BusSet.EnabledFeatures = 0;

	if (abnormalTermination) {

		DebugTrace( NDASSCSI_DBG_MINIPORT_ERROR, ("Abnormal stop.\n") );
		BusSet.AdapterStatus |= NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT;
	}

	status = IoctlToLanscsiBus( IOCTL_NDASBUS_SETPDOINFO,
								&BusSet,
								sizeof(NDASBUS_SETPDOINFO),
								NULL,
								0,
								NULL );

	if (!NT_SUCCESS(status)) {
	
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Failed to Update AdapterStatus in NDASSCSI.\n"));

	} else {
	
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Updated AdapterStatus in NDASSCSI.\n"));
	}

	//	Dereference the driver object for this LUR.
	//	Raise IRQL to defer driver object dereferencing.
}


//
//	Stop worker from MinportControl.
//	NOTE: It must not reference HwDeviceExtenstion while stopping.
//

VOID
MiniRestartWorker (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PNDSC_WORKITEM	NdscWorkItemCtx
	) 
{
	ULONG						SlotNo;
	NDASBUS_SETPDOINFO			BusSet;
	NTSTATUS					status;
	ULONG						result;
	PMINIPORT_DEVICE_EXTENSION	hwDeviceExtension;
	ULONG						adapterStatus;
	KIRQL						oldIrql;
	LONG						loop;
	LARGE_INTEGER				interval;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n") );

	UNREFERENCED_PARAMETER(DeviceObject);

	hwDeviceExtension = (PMINIPORT_DEVICE_EXTENSION)NdscWorkItemCtx->Arg1;
	SlotNo = PtrToUlong(NdscWorkItemCtx->Arg3);

	//	Restart the LUR
	//	Retry if failure.
	
	interval.QuadPart = -NANO100_PER_SEC * 5;

	for (loop = 0; loop < 10; loop++) {

		result = EnumerateLURFromNDASBUS( hwDeviceExtension, TRUE );
		
		if (result == SP_RETURN_FOUND) {

			break;
		}

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Loop #%u. try creating LUR again.\n", loop) );

		KeDelayExecutionThread( KernelMode, FALSE, &interval );
	};

	if (result == SP_RETURN_FOUND) {

		//	Set AdapterStatus to RUNNUNG.

		KeAcquireSpinLock( &hwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql );

		NDAS_ASSERT( ADAPTER_ISSTATUS(hwDeviceExtension, NDASSCSI_ADAPTER_STATUS_RUNNING) );
		NDAS_ASSERT( ADAPTER_ISSTATUSFLAG(hwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_RESTARTING) );

		//	Set RUNNING status.
		//	clear all other flags.

		hwDeviceExtension->AdapterStatus = NDASSCSI_ADAPTER_STATUS_RUNNING;

		// Set hardware device extension again becuase we clears this field
		// at the stop adapter routine.

		if (hwDeviceExtension->LURs[0]) {
		
			hwDeviceExtension->LURs[0]->AdapterHardwareExtension = hwDeviceExtension;
		}

		KeReleaseSpinLock(&hwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

		adapterStatus = NDASSCSI_ADAPTER_STATUS_RUNNING |
						NDASSCSI_ADAPTER_STATUSFLAG_RESETSTATUS;

		//	Do not alert to the service.

	} else {

		// Set AdapterStatus to STOPPING to return error to all request from now.

		DebugTrace( NDASSCSI_DBG_MINIPORT_ERROR, ("Abnormal stop.\n") );

		NdasMiniLogError( hwDeviceExtension,
						  NULL,
						  0,
						  0,
						  0,
						  NDASSCSI_IO_INITLUR_FAIL,
						  EVTLOG_UNIQUEID(EVTLOG_MODULE_FINDADAPTER, EVTLOG_FAIL_TO_ADD_LUR, 0) );

		KeAcquireSpinLock(&hwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);

		NDAS_ASSERT( ADAPTER_ISSTATUS(hwDeviceExtension, NDASSCSI_ADAPTER_STATUS_RUNNING) );
		NDAS_ASSERT( ADAPTER_ISSTATUSFLAG(hwDeviceExtension, NDASSCSI_ADAPTER_STATUSFLAG_RESTARTING) );

		hwDeviceExtension->AdapterStatus = NDASSCSI_ADAPTER_STATUS_STOPPING;

		KeReleaseSpinLock( &hwDeviceExtension->LanscsiAdapterSpinLock, oldIrql );

		//	Alert abnormal status of the SCSI adapter to the service.

		adapterStatus = NDASSCSI_ADAPTER_STATUS_STOPPED | NDASSCSI_ADAPTER_STATUSFLAG_RESETSTATUS | NDASSCSI_ADAPTER_STATUSFLAG_ABNORMAL_TERMINAT;
	}

	if (result == SP_RETURN_FOUND) {

		PLIST_ENTRY		listEntry;
		PLSU_BLOCK_ACE  lsuBlockAce;
		PLSU_BLOCK_ACE  lsuBace;

		for (listEntry  = hwDeviceExtension->BackupUserBacl[0].BlockAclHead.Flink; 
			 listEntry != &hwDeviceExtension->BackupUserBacl[0].BlockAclHead; 
			 listEntry  = listEntry->Flink) {

			lsuBlockAce = CONTAINING_RECORD( listEntry, LSU_BLOCK_ACE, BlockAclList );

			lsuBace = LsuCreateBlockAce( lsuBlockAce->AccessMode, 
										 lsuBlockAce->BlockStartAddr * lsuBlockAce->BlockBytes,
										 lsuBlockAce->BlockLength * lsuBlockAce->BlockBytes, 
										 lsuBlockAce->BlockBytes,
										 lsuBlockAce->BlockAceId );

			if (lsuBace == NULL) {

				NDAS_ASSERT(FALSE);
				continue;
			}

			LsuInsertAce( &hwDeviceExtension->LURs[0]->UserBacl, lsuBace );
		}

	} else {

		LsuFreeAllBlockAce( &hwDeviceExtension->BackupUserBacl[0].BlockAclHead );
	}

	//	Update Status in LanscsiBus
	//	This code does not use HwDeviceExtension to be independent.
	//	If disconnection event is available, we regard it as a abnormal unplug.

	BusSet.Size = sizeof(BusSet);
	BusSet.SlotNo = SlotNo;
	BusSet.AdapterStatus = adapterStatus;
	BusSet.SupportedFeatures = BusSet.EnabledFeatures = 0;

	status = IoctlToLanscsiBus( IOCTL_NDASBUS_SETPDOINFO,
								&BusSet,
								sizeof(NDASBUS_SETPDOINFO),
								NULL,
								0,
								NULL );

	if (!NT_SUCCESS(status)) {
	
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Failed to Update AdapterStatus in NDASSCSI.\n") );

	} else {

		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Updated AdapterStatus in NDASSCSI.\n") );
	}
}

//	NoOperation worker.
//	Send NoOperation Ioctl to Miniport itself.

VOID
MiniNoOperationWorker(
		IN PDEVICE_OBJECT			DeviceObject,
		IN PNDSC_WORKITEM			NdscWorkItemCtx
	) {
	NDSCIOCTL_NOOP	NoopData;

	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("entered.\n"));
	NoopData.NdasScsiAddress.PortId		= 0;
	NoopData.NdasScsiAddress.PathId		= (UINT8)NdscWorkItemCtx->Arg1;
	NoopData.NdasScsiAddress.TargetId	= (UINT8)NdscWorkItemCtx->Arg2;
	NoopData.NdasScsiAddress.Lun		= (UINT8)NdscWorkItemCtx->Arg3;

	MiniSendIoctlSrbAsynch(
			DeviceObject,
			NDASSCSI_IOCTL_NOOP,
			&NoopData,
			sizeof(NDSCIOCTL_NOOP),
			NULL,
			0
		);

}

//////////////////////////////////////////////////////////////////////////
//
//	LurnCallback routine.
//
VOID
MiniLurnCallback(
	PLURELATION	Lur,
	PLURN_EVENT	LurnEvent
) {
	PMINIPORT_DEVICE_EXTENSION	hwDeviceExtension = Lur->AdapterHardwareExtension;
	KIRQL						oldIrql;

	ASSERT(LurnEvent);
	DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Lur:%p Event class:%x\n",
									Lur, LurnEvent->LurnEventClass));

	if(hwDeviceExtension == NULL) {
		DebugTrace( NDASSCSI_DBG_MINIPORT_ERROR,("Adapter hw extension does not exist.\n"));
		return;
	}

	KeAcquireSpinLock(&hwDeviceExtension->LanscsiAdapterSpinLock, &oldIrql);
	if(!ADAPTER_ISSTATUS(hwDeviceExtension, NDASSCSI_ADAPTER_STATUS_RUNNING)) {
		KeReleaseSpinLock(&hwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Adapter %p not running. AdapterStatus %x.\n",
			hwDeviceExtension, hwDeviceExtension->AdapterStatus));
		return;
	}
	KeReleaseSpinLock(&hwDeviceExtension->LanscsiAdapterSpinLock, oldIrql);

	switch(LurnEvent->LurnEventClass) {

	case LURN_REQUEST_NOOP_EVENT: {
		NDSC_WORKITEM_INIT			WorkitemCtx;

		NDSC_INIT_WORKITEM(
					&WorkitemCtx,
					MiniNoOperationWorker,
					NULL,
					(PVOID)Lur->LurId[0],
					(PVOID)Lur->LurId[1],
					(PVOID)Lur->LurId[2]
				);
		MiniQueueWorkItem(&NdasMiniGlobalData, Lur->AdapterFunctionDeviceObject, &WorkitemCtx);

//		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("LURN_REQUEST_NOOP_EVENT: AdapterStatus %x.\n",
//									AdapterStatus));
	break;
	}

	default:
		DebugTrace( NDASSCSI_DBG_MINIPORT_INFO,("Invalid event class:%x\n", LurnEvent->LurnEventClass));
	break;
	}

}
