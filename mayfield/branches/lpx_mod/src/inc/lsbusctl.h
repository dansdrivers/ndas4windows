#ifndef _LANSCSILIB_H_
#define _LANSCSILIB_H_

#ifndef __LANSCSI_BUS__
#define __LANSCSI_BUS__
#endif

#include "lsbusioctl.h"
#include "lsminiportioctl.h"

#ifdef __cplusplus
#define LSBUSCTL_LINKAGE extern "C" 
#else
#define LSBUSCTL_LINKAGE extern 
#endif 

#ifdef LSBUSCTL_DLL_IMPL
#define LSBUSCTLAPI __declspec(dllexport) LSBUSCTL_LINKAGE
#endif

#ifdef LSBUSCTL_USE_DLL
#pragma comment(lib, "lsbusctl_dyn.lib")
#define LSBUSCTLAPI __declspec(dllimport) LSBUSCTL_LINKAGE
#else
#pragma comment(lib, "lsbusctl.lib")
#define LSBUSCTLAPI LSBUSCTL_LINKAGE
#endif


LSBUSCTLAPI 
BOOL
WINAPI
LsBusCtlGetVersion(
	LPWORD lpVersionMajor, 
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlGetMiniportVersion(
	DWORD SlotNo,
	LPWORD lpVersionMajor,
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlPlugInEx(
	ULONG SlotNo,
	ULONG MaxRequestBlocks,
	HANDLE hEvent,
	HANDLE hAlarmEvnet);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlEject(DWORD SlotNo);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlUnplug(DWORD SlotNo);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlAddTarget(
    PLANSCSI_ADD_TARGET_DATA	pAddTargetData);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlRemoveTarget(DWORD SlotNo);

//#define LSBUSCTL_ALARM_STATUS_NORMAL          0x00000000
//#define LSBUSCTL_ALARM_STATUS_START_RECONNECT 0x00000001
//#define LSBUSCTL_ALARM_STATUS_FAIL_RECONNECT  0x00000002 // obsolete

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryStatus(
	DWORD SlotNo, 
	PULONG pStatus);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryDvdStatus(
	ULONG SlotNo,
	PULONG pDvdStatus);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryNodeAlive(
	ULONG SlotNo,
	LPBOOL pbAlive,
	LPBOOL pbAdapterHasError);

LSBUSCTLAPI 
ULONG 
WINAPI
LsBusCtlQuerySlotNoByHandle(
	HANDLE	hDevice);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryInformation(
	PBUSENUM_QUERY_INFORMATION LsBusQuery,
	ULONG QueryLength,
	PBUSENUM_INFORMATION Information,
	ULONG InformationLength);

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryMiniportInformation(
	PLSMPIOCTL_QUERYINFO LsMiniportQuery,
	ULONG QueryLength,
	PVOID Information,
	ULONG InformationLength);

LSBUSCTLAPI
BOOL 
WINAPI
LsBusCtlQueryMiniportFullInformation(
	ULONG						SlotNo,
	PLSMPIOCTL_ADAPTERLURINFO	*LurFullInfo);

LSBUSCTLAPI
BOOL 
WINAPI
LsBusCtlQueryPdoSlotList(
	PBUSENUM_INFORMATION *BusInfo
);

LSBUSCTLAPI
BOOL 
WINAPI
LsBusCtlQueryPdoEvent(
	ULONG	SlotNo,
	PHANDLE	AlarmEvent,
	PHANDLE	DisconnectEvent
);

#endif // _LANSCSILIB_H_
