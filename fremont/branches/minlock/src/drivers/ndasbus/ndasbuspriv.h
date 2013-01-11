#ifndef __NDASBUS_PRIV_H__
#define __NDASBUS_PRIV_H__


//
//	ndasbus.c
//
NTSTATUS
LSBus_OpenLanscsiAdapter(
	   IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter,
	   IN	ULONG						MaxRequestLength,
	   IN	PKEVENT						DisconEventToService,
	   IN	PKEVENT						AlarmEventToService
   );

NTSTATUS
LSBus_CloseLanscsiAdapter(
		IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
	);

NTSTATUS
LSBus_WaitUntilNdasScsiStop(
		IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
	);

NTSTATUS
LSBus_IoctlToNdasScsiDevice(
		PPDO_DEVICE_DATA	PdoData,
		ULONG				IoctlCode,
		PVOID				InputBuffer,
		LONG				InputBufferLength,
		PVOID				OutputBuffer,
		LONG				OutputBufferLength
	);

NTSTATUS
LSBus_QueryInformation(
		PFDO_DEVICE_DATA				FdoData,
		BOOLEAN							Request32,
		PNDASBUS_QUERY_INFORMATION		Query,
		PNDASBUS_INFORMATION			Information,
		LONG							OutBufferLength,
		PLONG							OutBufferLenNeeded
	);

NTSTATUS
LSBus_PlugInLSBUSDevice(
			PFDO_DEVICE_DATA				FdoData,
			PNDASBUS_PLUGIN_HARDWARE_EX2	PlugIn
	);

NTSTATUS
LSBus_PlugOutLSBUSDevice(
			PFDO_DEVICE_DATA	FdoData,
			ULONG				SlotNo,
			BOOLEAN				Persistency
	);

NTSTATUS
LSBus_AddTargetWithLurDesc(
		PFDO_DEVICE_DATA	FdoData,
		PLURELATION_DESC	LurDesc,
		ULONG				SlotNo
	);

NTSTATUS
LSBus_AddTarget(
		PFDO_DEVICE_DATA			FdoData,
		PNDASBUS_ADD_TARGET_DATA	AddTargetData
);

//
// PDO status queue ( Adapter status queue ) entry
//

typedef struct _NDASBUS_PDOEVENT_ENTRY {
	ULONG	AdapterStatus;
	LIST_ENTRY	AdapterStatusQueueEntry;
} NDASBUS_PDOEVENT_ENTRY, *PNDASBUS_PDOEVENT_ENTRY;

PNDASBUS_PDOEVENT_ENTRY
NdasBusCreatePdoStatusItem(
	 ULONG	AdapterStatus
);

VOID
NdasBusDestroyPdoStatusItem(
	 PNDASBUS_PDOEVENT_ENTRY PdoEventEntry
);

VOID
NdasBusQueuePdoStatusItem(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData,
	PNDASBUS_PDOEVENT_ENTRY PdoEventEntry
);

PNDASBUS_PDOEVENT_ENTRY
NdasBusDequeuePdoStatusItem(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData
);

__inline
BOOLEAN
NdasBusIsEmpty(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData
){
	return IsListEmpty(&NdasPdoData->NdasPdoEventQueue);
}

VOID
NdasBusCleanupPdoStatusQueue(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData
);


//
//	register.c
//
NTSTATUS
LSBus_RegInitialize(
	PFDO_DEVICE_DATA	FdoData
);

VOID
LSBus_RegDestroy(
	PFDO_DEVICE_DATA	FdoData
);

NTSTATUS
LSBus_RegisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PNDASBUS_PLUGIN_HARDWARE_EX2	Plugin
);

NTSTATUS
LSBus_RegisterTarget(
	PFDO_DEVICE_DATA			FdoData,
	PNDASBUS_ADD_TARGET_DATA	AddTargetData
);

NTSTATUS
LSBus_UnregisterDevice(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo
);

NTSTATUS
LSBus_UnregisterTarget(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo,
		ULONG				TargetId
);

NTSTATUS
LSBus_IsRegistered(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo
);

NTSTATUS
LSBus_CleanupNDASDeviceRegistryUnsafe(
		PFDO_DEVICE_DATA	FdoData
);

//
//	busenum.c
//
PPDO_DEVICE_DATA
LookupPdoData(
	PFDO_DEVICE_DATA	FdoData,
	ULONG				SystemIoBusNumber
	);

//
//	utils.c
//

NTSTATUS
NCommVerifyNdasDevWithDIB(
		IN PNDASBUS_ADD_TARGET_DATA	AddTargetData,
		IN PTA_LSTRANS_ADDRESS		SecondaryAddress
	);


VOID
NDBusIoctlLogError(
	IN PDEVICE_OBJECT	DeviceObject,
	IN UINT32			ErrorCode,
	IN UINT32			IoctlCode,
	IN UINT32			Parameter
);

NTSTATUS
LfsFiltDriverServiceExist();

NTSTATUS
LfsCtlGetVersions(
	PUSHORT VerMajor,
	PUSHORT VerMinor,
	PUSHORT VerBuild,
	PUSHORT VerPrivate,
	PUSHORT NDFSVerMajor,
	PUSHORT NDFSVerMinor
);

NTSTATUS
LfsCtlIsReady();

#endif // __LANSCSIBUSPROC_H__
