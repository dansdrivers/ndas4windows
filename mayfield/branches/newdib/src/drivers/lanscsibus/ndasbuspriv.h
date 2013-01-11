#ifndef __NDASBUS_PRIV_H__
#define __NDASBUS_PRIV_H__

#include <tdikrnl.h>

//
//	ndasbus.c
//
NTSTATUS
LSBus_OpenLanscsiAdapter(
	   IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter,
	   IN	ULONG				MaxBlocks,
	   IN	PKEVENT				DisconEventToService,
	   IN	PKEVENT				AlarmEventToService
   );

NTSTATUS
LSBus_CloseLanscsiAdapter(
		IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
	);

NTSTATUS
LSBus_WaitUntilLanscsiMiniportStop(
		IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
	);

NTSTATUS
LSBus_IoctlToLSMPDevice(
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
		PBUSENUM_QUERY_INFORMATION		Query,
		PBUSENUM_INFORMATION			Information,
		LONG							OutBufferLength,
		PLONG							OutBufferLenNeeded
	);

NTSTATUS
LSBus_PlugInLSBUSDevice(
			PFDO_DEVICE_DATA				FdoData,
			PBUSENUM_PLUGIN_HARDWARE_EX2	PlugIn
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
		PLANSCSI_ADD_TARGET_DATA	AddTargetData
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
		PBUSENUM_PLUGIN_HARDWARE_EX2	Plugin
);

NTSTATUS
LSBus_RegisterTarget(
	PFDO_DEVICE_DATA			FdoData,
	PLANSCSI_ADD_TARGET_DATA	AddTargetData
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
		IN PLANSCSI_ADD_TARGET_DATA	AddTargetData,
		IN PTA_LSTRANS_ADDRESS		SecondaryAddress,
		IN ULONG					MaxBlocksPerRequest
	);


VOID
NDBusIoctlLogError(
	IN PDEVICE_OBJECT	DeviceObject,
	IN UINT32			ErrorCode,
	IN UINT32			IoctlCode,
	IN UINT32			Parameter
);

#endif // __LANSCSIBUSPROC_H__
