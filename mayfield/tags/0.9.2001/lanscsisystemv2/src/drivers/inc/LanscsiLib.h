#ifndef _LANSCSILIB_H_
#define _LANSCSILIB_H_

#ifndef __LANSCSI_BUS__
#define __LANSCSI_BUS__
#endif

#include "lsbusioctl.h"
#include "LSMPIoctl.h"

#ifdef  __cplusplus
extern "C"
{
#endif 
/*
BOOLEAN
LanscsiPlugin(
			  ULONG		SlotNo,
			  ULONG		MaxRequestBlocks,
			  HANDLE	*phEvent
			  );
*/
// inserted by ILGU
BOOLEAN
LanscsiPluginEx(
			  ULONG		SlotNo,
			  ULONG		MaxRequestBlocks,
			  HANDLE	*phEvent,
			  HANDLE    *phAlarmEvnet
			  );
// inserted by ILGU end
BOOLEAN
LanscsiEject(
	ULONG   SerialNo
	);

BOOLEAN
LanscsiUnplug(
	ULONG   SerialNo
	);

BOOLEAN
LanscsiAddTarget(
    PLANSCSI_ADD_TARGET_DATA	AddTargetData
	);

BOOLEAN
LanscsiRemoveTarget(
    PLANSCSI_REMOVE_TARGET_DATA	RemoveTargetData
	);
/*
BOOLEAN
LanscsiCopyTarget(
    PLANSCSI_COPY_TARGET_DATA	CopyTargetData
	);
*/

// inserted by ILGU
#define ALARM_STATUS_NORMAL				0x00000000
#define ALARM_STATUS_START_RECONNECT	0x00000001
#define ALARM_STATUS_FAIL_RECONNECT		0x00000002

BOOLEAN
LanscsiQueryAlarmStatus(
					  ULONG	SlotNo,
					  PULONG AlarmStatus
					  );
// inserted by ILGU end
BOOLEAN
LanscsiQueryDvdStatus(
					  ULONG	SlotNo,
					  PULONG pDvdStatus
					  );
BOOLEAN
LanscsiQueryNodeAlive(
					  ULONG		SlotNo,
					  PBOOL		pbAdapterHasError
					  );

ULONG
LanscsiQuerySlotNoByHandle(
						   HANDLE	hDevice
						   );


BOOLEAN
LanscsiQueryInformation(
		  PBUSENUM_QUERY_INFORMATION	Query,
		  ULONG							QueryLength,
		  PBUSENUM_INFORMATION			Information,
		  ULONG							InformationLength
	) ;

BOOLEAN
LanscsiQueryLsmpInformation(
		  PLSMPIOCTL_QUERYINFO			Query,
		  ULONG							QueryLength,
		  PVOID							Information,
		  ULONG							InformationLength
	  );


//////////////////////////////////////////////////////////////////////////////
//	@hootch@
//////////////////////////////////////////////////////////////////////////////
//
// register a window as device-notification listener
//
BOOL LSLib_RegisterDevNotification(SERVICE_STATUS_HANDLE hSS, HDEVNOTIFY *hSCSIIfNtf, HDEVNOTIFY *hVolNtf) ;

//
// unregister a service as device-notification listener
//
void LSLib_UnregisterDevNotification(HDEVNOTIFY hSCSIIfNtf, HDEVNOTIFY hVolNtf) ;

//
//	start devive driver
//
BOOL LoadDeviceDriver( const TCHAR * Name, const TCHAR * Path, 
					  HANDLE * lphDevice, PDWORD Error ) ;

#ifdef  __cplusplus
}
#endif 

#endif // _LANSCSILIB_H_
