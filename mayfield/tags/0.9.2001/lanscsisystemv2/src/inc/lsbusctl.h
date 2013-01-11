#ifndef _LANSCSILIB_H_
#define _LANSCSILIB_H_

#ifndef __LANSCSI_BUS__
#define __LANSCSI_BUS__
#endif

#include "lsbusioctl.h"
#include "lsminiportioctl.h"

#ifdef  __cplusplus
extern "C"
{
#endif 

BOOLEAN
LsBusCtlPlugInEx(
	ULONG SlotNo,
	ULONG MaxRequestBlocks,
	HANDLE hEvent,
	HANDLE hAlarmEvnet);

BOOLEAN
LsBusCtlEject(
	ULONG SlotNo);

BOOLEAN
LsBusCtlUnplug(
	ULONG SlotNo);

BOOLEAN
LsBusCtlAddTarget(
    PLANSCSI_ADD_TARGET_DATA	pAddTargetData);

BOOLEAN
LsBusCtlRemoveTarget(
    PLANSCSI_REMOVE_TARGET_DATA	pRemoveTargetData);

#define ALARM_STATUS_NORMAL				0x00000000
#define ALARM_STATUS_START_RECONNECT	0x00000001
#define ALARM_STATUS_FAIL_RECONNECT		0x00000002 // obsolete

BOOLEAN
LsBusCtlQueryAlarmStatus(
	ULONG SlotNo,
	PULONG AlarmStatus);

// inserted by ILGU end
BOOLEAN
LsBusCtlQueryDvdStatus(
					  ULONG	SlotNo,
					  PULONG pDvdStatus
					  );

BOOLEAN
LsBusCtlQueryNodeAlive(
	ULONG SlotNo,
	PBOOL pbAdapterHasError);

ULONG
LsBusCtlQuerySlotNoByHandle(
	HANDLE	hDevice);


BOOLEAN
LsBusCtlQueryInformation(
	PBUSENUM_QUERY_INFORMATION LsBusQuery,
	ULONG QueryLength,
	PBUSENUM_INFORMATION Information,
	ULONG InformationLength);

BOOLEAN
LsBusCtlQueryLsmpInformation(
	PLSMPIOCTL_QUERYINFO LsMiniportQuery,
	ULONG QueryLength,
	PVOID Information,
	ULONG InformationLength);


#if 0
//
//	start devive driver
//
BOOL LoadDeviceDriver( const TCHAR * Name, const TCHAR * Path, 
					  HANDLE * lphDevice, PDWORD Error ) ;

#endif

#ifdef  __cplusplus
}
#endif 

#endif // _LANSCSILIB_H_
