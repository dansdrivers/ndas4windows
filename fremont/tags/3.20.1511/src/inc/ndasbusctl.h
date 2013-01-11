#ifndef _NDASBUS_LIB_H_
#define _NDASBUS_LIB_H_

#include "ndasbusioctl.h"
#include "ndasscsiioctl.h"

#ifdef __cplusplus
#define NDASBUSCTL_LINKAGE extern "C" 
#else
#define NDASBUSCTL_LINKAGE extern 
#endif 

#ifdef NDASBUSCTL_DLL_IMPL
#define NDASBUSCTLAPI __declspec(dllexport) NDASBUSCTL_LINKAGE
#endif

#ifdef NDASBUSCTL_USE_DLL
#pragma comment(lib, "ndasbusctl_dyn.lib")
#define NDASBUSCTLAPI __declspec(dllimport) NDASBUSCTL_LINKAGE
#else
#pragma comment(lib, "libndasbusctl.lib")
#define NDASBUSCTLAPI NDASBUSCTL_LINKAGE
#endif


NDASBUSCTLAPI 
BOOL
WINAPI
NdasBusCtlGetVersion(
	LPWORD lpVersionMajor, 
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlGetMiniportVersion(
	DWORD SlotNo,
	LPWORD lpVersionMajor,
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate);

NDASBUSCTLAPI
BOOL
WINAPI
NdasBusCtlPlugInEx2(
	ULONG	SlotNo,
	ULONG	MaxOsDataTransferLength, // in bytes
	HANDLE	hEvent,
	HANDLE	hAlarmEvent,
	BOOL	NotRegister);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlEject(DWORD SlotNo);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlUnplug(DWORD SlotNo);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlAddTarget(
    PNDASBUS_ADD_TARGET_DATA	pAddTargetData);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlRemoveTarget(DWORD SlotNo);

//#define LSBUSCTL_ALARM_STATUS_NORMAL          0x00000000
//#define LSBUSCTL_ALARM_STATUS_START_RECONNECT 0x00000001
//#define LSBUSCTL_ALARM_STATUS_FAIL_RECONNECT  0x00000002 // obsolete

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryStatus(
	DWORD SlotNo, 
	PULONG pStatus);

NDASBUSCTLAPI
BOOL
WINAPI
NdasBusCtlQueryDeviceMode(
	ULONG SlotNo,
	PULONG pDeviceMode);

NDASBUSCTLAPI
BOOL
WINAPI
NdasBusCtlQueryEvent(
	ULONG SlotNo,
	PULONG pStatus);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryDvdStatus(
	ULONG SlotNo,
	PULONG pDvdStatus);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryNodeAlive(
	ULONG SlotNo,
	LPBOOL pbAlive,
	LPBOOL pbAdapterHasError);

NDASBUSCTLAPI 
ULONG 
WINAPI
NdasBusCtlQuerySlotNoByHandle(
	HANDLE	hDevice);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryInformation(
	PNDASBUS_QUERY_INFORMATION NdasBusQuery,
	ULONG QueryLength,
	PNDASBUS_INFORMATION Information,
	ULONG InformationLength);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryMiniportInformation(
	PNDASSCSI_QUERY_INFO_DATA NdasMiniportQuery,
	ULONG QueryLength,
	PVOID Information,
	ULONG InformationLength);

NDASBUSCTLAPI
BOOL 
WINAPI
NdasBusCtlQueryMiniportFullInformation(
	ULONG						SlotNo,
	PNDSCIOCTL_ADAPTERLURINFO	*LurFullInfo);

NDASBUSCTLAPI
BOOL 
WINAPI
NdasBusCtlQueryPdoSlotList(
	PNDASBUS_INFORMATION *BusInfo
);

NDASBUSCTLAPI
BOOL 
WINAPI
NdasBusCtlQueryPdoEvent(
	ULONG	SlotNo,
	PHANDLE	AlarmEvent,
	PHANDLE	DisconnectEvent
);

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlStartStopRegistrarEnum(
	BOOL	bOnOff,
	LPBOOL	pbPrevState);


NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryPdoFileHandle(
	ULONG	SlotNo,
	PHANDLE	PdoFileHandle);

//////////////////////////////////////////////////////////////////////////
//
// NdasBus registrar control
//
//

//
// Register a SCSI adapter
//

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlRegisterDevice(
	ULONG	SlotNo,
	ULONG	MaxOsDataTransferLength);

//
// Unregister a NDAS adapter
//

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlUnregisterDevice(
	ULONG SlotNo);

//
// Register a target
//

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlUnregisterTarget(
	DWORD SlotNo,
	DWORD TargetId);


//
// Register a target
//

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlRegisterTarget(
	PNDASBUS_ADD_TARGET_DATA RegisterTargetData);


#endif // _NDASBUS_LIB_H_
