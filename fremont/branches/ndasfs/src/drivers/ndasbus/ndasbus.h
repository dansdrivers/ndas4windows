#ifndef __LANSCSI_BUS_H__
#define __LANSCSI_BUS_H__


#define NDASBUS_NDASSCSI_STOP_TIMEOUT		40

#define NDBUS_POOLTAG_UNPLUGWORKITEM	'iwSL'
#define NCOMM_POOLTAG_LSS				'lnSN'
#define NCOMM_POOLTAG_BACL				'baSN'
#define NDBUS_POOLTAG_PDOSTATUS			'spBN'

#define	LSDEVDATA_FLAG_LURDESC	0x00000001

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable


#define NDASPDO_EVENTQUEUE_THRESHOLD	32

typedef struct _PDO_LANSCSI_DEVICE_DATA {

	//
	//	set by NDAS SCSI driver
	//
	KSPIN_LOCK		LSDevDataSpinLock;
	ULONG			LastAdapterStatus;

	//
	// PDO event queue
	//

	LIST_ENTRY		NdasPdoEventQueue;
	LONG			NdasPdoEventEntryCount;

	UINT32			DeviceMode;
	NDAS_FEATURES	SupportedFeatures;
	NDAS_FEATURES	EnabledFeatures;

	//
	//	set by NDAS bus driver
	//
	ULONG			MaxRequestLength;
	PKEVENT			DisconEventToService;
	PKEVENT			AlarmEventToService;

	//
	//	NDAS bus private
	//
	KEVENT			AddTargetEvent;
	ULONG			Flags;
	LONG			AddDevInfoLength;
	PVOID			AddDevInfo;

} PDO_LANSCSI_DEVICE_DATA, *PPDO_LANSCSI_DEVICE_DATA;

#endif