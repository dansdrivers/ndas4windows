#ifndef __LANSCSI_BUS_H__
#define __LANSCSI_BUS_H__

#include "oem.h"
#include "lsbusioctl.h"

#define NANO100_PER_SEC			(LONGLONG)(10 * 1000 * 1000)
#define LSBUS_LANSCSIMINIPORT_STOP_TIMEOUT		40

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

typedef struct _PDO_LANSCSI_DEVICE_DATA {

	//
	//	set by LanscsiMiniport
	//
	KSPIN_LOCK	LSDevDataSpinLock;
	ULONG		AdapterStatus;
    UINT32		DesiredAccess;
	UINT32		GrantedAccess;

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
	PLANSCSI_ADD_TARGET_DATA	AddTargetData;


} PDO_LANSCSI_DEVICE_DATA, *PPDO_LANSCSI_DEVICE_DATA;

#endif