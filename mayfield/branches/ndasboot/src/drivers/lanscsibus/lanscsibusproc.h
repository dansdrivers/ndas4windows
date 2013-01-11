#ifndef __LANSCSIBUSPROC_H__
#define __LANSCSIBUSPROC_H__

//
//	lanscsibus.c
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
			PFDO_DEVICE_DATA	FdoData,
			ULONG				SlotNo,
			PWCHAR				HardwareIDs,
			LONG				HardwareIDLen,
			ULONG				MaxBlocksPerRequest
	);

NTSTATUS
LSBus_PlugOutLSBUSDevice(
			PFDO_DEVICE_DATA	FdoData,
			ULONG				SlotNo
	);

NTSTATUS
LSBus_AddNdasDeviceWithLurDesc(
		PFDO_DEVICE_DATA	FdoData,
		PLURELATION_DESC	LurDesc,
		ULONG				SlotNo
	);



//
//	register.c
//
NTSTATUS
Reg_LookupRegDeviceWithSlotNo(
	IN	HANDLE	NdasDeviceReg,
	IN	ULONG	SlotNo,
	OUT	HANDLE	*OneDevice
	);

NTSTATUS
Reg_OpenDeviceRegistry(
		PDEVICE_OBJECT	DeviceObject,
		HANDLE			*DeviceReg,
		ACCESS_MASK		AccessMask
	);

NTSTATUS
Reg_OpenNdasDeviceRegistry(
		HANDLE			*NdasDeviceReg,
		ACCESS_MASK		AccessMask,
		HANDLE			RootReg
	);

NTSTATUS
LSBUS_QueueWorker_PlugInByRegistry(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				PlugInTimeMask
	);

NTSTATUS
LSBus_RegisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PLANSCSI_REGISTER_NDASDEV	NdasInfo
	);

NTSTATUS
LSBus_UnregisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PLANSCSI_UNREGISTER_NDASDEV		NdasInfo
	);

NTSTATUS
LSBus_VerifyLurDescWithDIB(
		PLURELATION_DESC	LurDesc
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
//	NdasComm.c
//
NTSTATUS
NCommVerifyLurnWithDIB(
		PLURELATION_NODE_DESC	LurnDesc,
		LURN_TYPE				ParentLurnType,
		ULONG					AssocID
	);

#endif // __LANSCSIBUSPROC_H__
