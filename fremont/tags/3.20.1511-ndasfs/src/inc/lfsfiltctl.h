/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

Initial implementation:

2004-8-15 Chesong Lee <cslee@ximeta.com>

Revisons:

--*/
#ifndef _LFSFILTCTL_H_
#define _LFSFILTCTL_H_
#pragma once

#ifndef EXTERN_C
#ifdef __cplusplus
    #define EXTERN_C    extern "C"
#else
    #define EXTERN_C    extern
#endif
#endif

#include "lfseventq.h"

#define XDF_LFSFILTCTL 0x00000080


/*++

Create a device file handle for ROFilt

--*/
EXTERN_C
HANDLE 
WINAPI 
LfsFiltCtlOpenDevice();

/*++

Start LFS Filter service

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltCtlStartService(SC_HANDLE hSCManager);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltCtlStartService();
#endif /* __cplusplus */

/*++

Query ROFilt service status 

--*/
EXTERN_C
BOOL 
WINAPI 
LfsFiltCtlQueryServiceStatus(
	SC_HANDLE hSCManager, 
	LPSERVICE_STATUS lpServiceStatus);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltCtlQueryServiceStatus(
	LPSERVICE_STATUS lpServiceStatus);
#endif /* __cplusplus */

/*++

Get LfsFilter Version

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltCtlGetVersion(
	HANDLE hFilter,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltCtlGetVersion(
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,	LPWORD lpwNDFSVerMinor);
#endif /* __cplusplus */

/*++

Create an event queue in LfsFilt

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltCtlCreateEventQueue(
	IN HANDLE				hFilter,
	OUT PLFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT HANDLE				*LfsEventWait);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltCtlCreateEventQueue(
	OUT PLFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT HANDLE				*LfsEventWait);
#endif /* __cplusplus */

/*++

Close an event queue in LfsFilt

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltCtlCloseEventQueue(
	IN HANDLE				hFilter,
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltCtlCloseEventQueue(
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle);
#endif /* __cplusplus */

/*++

Get an event header before retrieving the event.

--*/
EXTERN_C
BOOL 
WINAPI 
LfsFiltGetEventHeader(
	IN HANDLE				hFilter,
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT PUINT32				EventLength,
	OUT PUINT32				EventClass);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltGetEventHeader(
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT PUINT32				EventLength,
	OUT PUINT32				EventClass);
#endif /* __cplusplus */

/*++

retrieve an event

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltGetEvent(
	IN HANDLE				hFilter,
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT DWORD				EventLength,
	OUT PXEVENT_ITEM		LfsEvent);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltGetEvent(
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT DWORD				EventLength,
	OUT PXEVENT_ITEM		LfsEvent);
#endif /* __cplusplus */


/*++

Get NDAS usage stats

--*/

typedef struct _LFSCTL_NDAS_USAGE {
	BOOL				NoDiskVolume;
	BOOL				Attached;
	BOOL				ActPrimary;
	BOOL				ActSecondary;
	BOOL				ActReadOnly;
	BOOL				HasLockedVolume;
	DWORD				MountedFSVolumeCount;
} LFSCTL_NDAS_USAGE, *PLFSCTL_NDAS_USAGE;

EXTERN_C
BOOL 
WINAPI 
LfsFiltQueryNdasUsage(
	IN  HANDLE              hFilter,
	IN  DWORD               SlotNumber,
	OUT PLFSCTL_NDAS_USAGE	NdasUsage);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltQueryNdasUsage(
	IN  DWORD				SlotNumber,
	OUT PLFSCTL_NDAS_USAGE	NdasUsage);
#endif /* __cplusplus */

/*++

Stop a secondary volume

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltStopSecondaryVolume(
	IN HANDLE hFilter,
	IN DWORD PhysicalDriveNumber);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltStopSecondaryVolume(
	IN DWORD PhysicalDriveNumber);
#endif /* __cplusplus */

/*++ 

Shutdown LfsFilter

Explicit call to shut down LfsFilter when shutting down the system. 

--*/

EXTERN_C
BOOL 
WINAPI 
LfsFiltCtlShutdown(
	HANDLE hFilter);

#ifdef __cplusplus
BOOL 
WINAPI 
LfsFiltCtlShutdown();
#endif /* __cplusplus */

#endif /* _LFSFILTERCTL_H_ */
