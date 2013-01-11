#ifndef __LANSCSIBUSPROC_H__
#define __LANSCSIBUSPROC_H__


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


#endif // __LANSCSIBUSPROC_H__
