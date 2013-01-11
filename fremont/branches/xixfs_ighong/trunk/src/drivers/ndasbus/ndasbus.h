#ifndef __LANSCSI_BUS_H__
#define __LANSCSI_BUS_H__

#include "ndasbusioctl.h"

#define NANO100_PER_SEC			(LONGLONG)(10 * 1000 * 1000)
#define LSBUS_LANSCSIMINIPORT_STOP_TIMEOUT		40

#define	LSBUS_POOTAG_LURDESC		'ulBL'
#define	LSBUS_POOTAG_PLUGIN			'lpBL'
#define	LSBUS_POOTAG_WSTRBUFFER		'bwBL'

#define	LSDEVDATA_FLAG_LURDESC	0x00000001

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

typedef struct _PDO_LANSCSI_DEVICE_DATA {

	//
	//	set by LanscsiMiniport
	//
	KSPIN_LOCK			LSDevDataSpinLock;
	ULONG				AdapterStatus;
    UINT32				DeviceMode;
	NDAS_FEATURES		SupportedFeatures;
	NDAS_FEATURES		EnabledFeatures;

	//
	//	set by LDServ
	//
	ULONG			MaxBlocksPerRequest;
	PKEVENT			DisconEventToService;
	PKEVENT			AlarmEventToService;

	//
	//	Lanscsibus private
	//
	KEVENT						AddTargetEvent;
	ULONG						Flags;
	LONG						AddDevInfoLength;
	PVOID						AddDevInfo;


} PDO_LANSCSI_DEVICE_DATA, *PPDO_LANSCSI_DEVICE_DATA;

#endif