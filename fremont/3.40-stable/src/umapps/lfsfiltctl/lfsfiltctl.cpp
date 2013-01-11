/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

2006-9-26 Chesong Lee <cslee@ximeta.com>

Replaced xdebug to XTLTRACE1

2004-8-15 Chesong Lee <cslee@ximeta.com>

Initial implementation

--*/

#include <windows.h>
#include <tchar.h>
#include <winioctl.h>
#include <strsafe.h>
#include <crtdbg.h>

#include "lfsfiltctl.h"
#include "lfsfilterpublic.h"
#include <xtl/xtlautores.h>

#include "trace.h"
#ifdef RUN_WPP
#include "lfsfiltctl.tmh"
#endif

const LPCWSTR LFSFILT_DEVICE_NAME = L"\\\\.\\Global\\lfsfilt";
const LPCWSTR LFSFILT_DRIVER_NAME = L"lfsfilt.sys";
const LPCWSTR LFSFILT_SERVICE_NAME = L"lfsfilt";

/*++

LfsFiltCtlStartService with SCM handle

--*/
BOOL 
WINAPI 
LfsFiltCtlStartService(SC_HANDLE hSCManager)
{
	XTL::AutoSCHandle hService = ::OpenService(
		hSCManager,
		LFSFILT_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (hService.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening a service (%ls) failed, error=0x%X\n", 
			LFSFILT_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Starting a service (%ls) failed, error=0x%X\n",
			LFSFILT_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"LFS Filter service started successfully.\n");
	
	return TRUE;
}

/*++

LfsFiltCtlQueryServiceStatus without SCM handle

--*/
BOOL
WINAPI 
LfsFiltCtlStartService()
{
	XTL::AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (hSCManager.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"OpenSCManager failed, error=0x%X\n", 
			GetLastError());
		return FALSE;
	}

	return LfsFiltCtlStartService(hSCManager);
}

/*++

LfsFiltCtlQueryServiceStatus with SCM handle

--*/
BOOL 
WINAPI 
LfsFiltCtlQueryServiceStatus(
	SC_HANDLE hSCManager,
	LPSERVICE_STATUS lpServiceStatus)
{
	XTL::AutoSCHandle hService = ::OpenService(
		hSCManager,
		LFSFILT_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (hService.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"OpenService(%ls) failed, error=0x%X\n",
			LFSFILT_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	BOOL fSuccess = ::QueryServiceStatus(hService, lpServiceStatus);
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"QueryServiceStatus(%ls) failed, error=0x%X\n",
			LFSFILT_SERVICE_NAME,
			GetLastError());
		return FALSE;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"QueryServiceStatus(%ls) completed successfully\n",
		LFSFILT_SERVICE_NAME);

	return TRUE;
}


/*++

NdasRoFilterQueryServiceStatus without SCM handle

--*/
BOOL 
WINAPI 
LfsFiltCtlQueryServiceStatus(
	LPSERVICE_STATUS lpServiceStatus)
{
	XTL::AutoSCHandle hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (hSCManager.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening SCM Database failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	return LfsFiltCtlQueryServiceStatus(hSCManager, lpServiceStatus);
}

/*++

LfsFiltCtlOpenDevice

Create a handle for LFSFilt device object,
If failed, returns INVALID_HANDLE_VALUE

--*/
HANDLE 
WINAPI 
LfsFiltCtlOpenDevice()
{
	//
	// Returning handle must not be AutoFileHandle
	//
	HANDLE hDevice = ::CreateFile(
		LFSFILT_DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == hDevice) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME,
			GetLastError());

		return INVALID_HANDLE_VALUE;
	}

	return hDevice;
}

/*++

Get the version information of LFS Filter
This IOCTL supports since Rev 1853

--*/

BOOL 
WINAPI 
LfsFiltCtlGetVersion(
	HANDLE hFilter,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor)
{
	DWORD cbReturned = 0;
	LFSFILT_VER verInfo = {0};

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_GETVERSION,
		NULL, 0,
		&verInfo, sizeof(verInfo), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Getting LfsFilter Version failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	if (lpwVerMajor) *lpwVerMajor = verInfo.VersionMajor;
	if (lpwVerMinor) *lpwVerMinor = verInfo.VersionMinor;
	if (lpwVerBuild) *lpwVerBuild = verInfo.VersionBuild;
	if (lpwVerPrivate) *lpwVerPrivate = verInfo.VersionPrivate;
	if (lpwNDFSVerMajor) *lpwNDFSVerMajor = verInfo.VersionNDFSMajor;
	if (lpwNDFSVerMinor) *lpwNDFSVerMinor = verInfo.VersionNDFSMinor;

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"LfsFilter version is %d.%d.%d.%d, NDFS %x.%x\n",
		verInfo.VersionMajor, verInfo.VersionMinor,
		verInfo.VersionBuild, verInfo.VersionPrivate,
		verInfo.VersionNDFSMajor, verInfo.VersionNDFSMinor);

	return TRUE;
}

BOOL 
WINAPI 
LfsFiltCtlGetVersion(
	LPWORD lpwVerMajor, 
	LPWORD lpwVerMinor,
	LPWORD lpwVerBuild,
	LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,
	LPWORD lpwNDFSVerMinor)
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltCtlGetVersion(
		hDevice, 
		lpwVerMajor, lpwVerMinor,
		lpwVerBuild, lpwVerPrivate,
		lpwNDFSVerMajor, lpwNDFSVerMinor);
}
/*++

Shutdown LfsFilter

Explicit call to shut down LfsFilter when shutting down the system.

--*/
BOOL WINAPI LfsFiltCtlShutdown(HANDLE hFilter)
{
	DWORD cbReturned = 0;
	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_SHUTDOWN,
		NULL, 0,
		NULL, 0, &cbReturned,
		(LPOVERLAPPED) NULL);
	
	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_SHUTDOWN failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"LFS_FILTER_SHUTDOWN completed successfully.\n");

	return TRUE;
}


/*++

Create an event queue in LfsFilt

--*/
BOOL 
WINAPI 
LfsFiltCtlCreateEventQueue(
	IN HANDLE hFilter,
	OUT PLFS_EVTQUEUE_HANDLE LfsEventQueueHandle,
	OUT HANDLE *LfsEventWait)
{
	DWORD cbReturned = 0;
	LFS_CREATEEVTQ_RETURN createEvetqReturn;

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_CREATE_EVTQ,
		NULL, 0,
		&createEvetqReturn, sizeof(createEvetqReturn), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_CREATE_EVTQ failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	if (LfsEventQueueHandle)
	{
		*LfsEventQueueHandle = (LFS_EVTQUEUE_HANDLE)
			createEvetqReturn.EventQueueHandle;
	}

	if (LfsEventWait)
	{
		*LfsEventWait = createEvetqReturn.EventWaitHandle;
	}

	XTLTRACE1(TRACE_LEVEL_INFORMATION,
		"LFS_FILTER_CREATE_EVTQ completed successfully, "
		"EventQueueHandle=%p, EventWaitHandle=%p\n", 
		createEvetqReturn.EventQueueHandle,
		createEvetqReturn.EventWaitHandle);

	return TRUE;
}

BOOL 
WINAPI 
LfsFiltCtlCreateEventQueue(
	OUT LFS_EVTQUEUE_HANDLE	*LfsEventQueueHandle,
	OUT HANDLE				*LfsEventWait)
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME,
			GetLastError());
		return FALSE;
	}

	return LfsFiltCtlCreateEventQueue(
		hDevice, 
		LfsEventQueueHandle,
		LfsEventWait);
}


/*++

Close an event queue in LfsFilt

--*/
BOOL WINAPI LfsFiltCtlCloseEventQueue(
	IN HANDLE				hFilter,
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle)
{
	DWORD cbReturned = 0;
	LFS_CLOSEVTQ_PARAM closeEvetqReturn;

	closeEvetqReturn.EventQueueHandle = LfsEventQueueHandle;

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_CLOSE_EVTQ,
		&closeEvetqReturn, sizeof(closeEvetqReturn),
		NULL, 0,
		&cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_CLOSE_EVTQ failed, EventQueueHandle=%p, error=0x%X\n",
			LfsEventQueueHandle, GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI LfsFiltCtlCloseEventQueue(
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle)
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltCtlCloseEventQueue(
		hDevice, 
		LfsEventQueueHandle);
}


/*++

Get an event header before retrieving the event.

--*/
BOOL WINAPI 
LfsFiltGetEventHeader(
	IN HANDLE				hFilter,
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT PUINT32				EventLength,
	OUT PUINT32				EventClass)
{
	DWORD cbReturned = 0;
	LFS_GET_EVTHEADER_PARAM		evtHeaderParam;
	LFS_GET_EVTHEADER_RETURN	evtHeaderReturn;

	evtHeaderParam.EventQueueHandle = LfsEventQueueHandle;

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_GET_EVTHEADER,
		&evtHeaderParam, sizeof(evtHeaderParam),
		&evtHeaderReturn, sizeof(evtHeaderReturn),
		&cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_GET_EVTHEADER failed, "
			"EventQueueHandle=%p, error=0x%X\n",
			LfsEventQueueHandle, GetLastError());
		return FALSE;
	}

	if(EventLength) *EventLength = evtHeaderReturn.EventLength;
	if(EventClass) *EventClass = evtHeaderReturn.EventClass;

	return TRUE;
}

BOOL 
WINAPI 
LfsFiltGetEventHeader(
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT PUINT32				EventLength,
	OUT PUINT32				EventClass)
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltGetEventHeader(
		hDevice, 
		LfsEventQueueHandle,
		EventLength,
		EventClass);
}

/*++

retrieve an event

--*/
BOOL 
WINAPI 
LfsFiltGetEvent(
	IN HANDLE				hFilter,
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT DWORD				EventLength,
	OUT PXEVENT_ITEM		LfsEvent)
{
	DWORD cbReturned = 0;
	LFS_DEQUEUE_EVT_PARAM		dequeueEvtParam;

	dequeueEvtParam.EventQueueHandle = LfsEventQueueHandle;

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_DEQUEUE_EVT,
		&dequeueEvtParam, sizeof(dequeueEvtParam),
		LfsEvent, EventLength,
		&cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_DEQUEUE_EVT failed, "
			"EventQueueHandle=%p, error=0x%X\n",
			LfsEventQueueHandle, GetLastError());
		return FALSE;
	}

	return TRUE;

}

BOOL 
WINAPI 
LfsFiltGetEvent(
	IN LFS_EVTQUEUE_HANDLE	LfsEventQueueHandle,
	OUT DWORD				EventLength,
	OUT PXEVENT_ITEM		LfsEvent)
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltGetEvent(
		hDevice, 
		LfsEventQueueHandle,
		EventLength,
		LfsEvent);
}

namespace
{
	struct FlaggedValue {
		const UINT32 Value;
		FlaggedValue(UINT32 Value) : Value(Value) {}
		BOOL IsFlagSet(UINT32 Flag) const { 
			return (Flag & Value) == Flag; 
		}
	};
}

/*++

Get NDAS usage stats

--*/
BOOL WINAPI 
LfsFiltQueryNdasUsage(
	IN HANDLE				hFilter,
	IN DWORD				SlotNumber,
	OUT PLFSCTL_NDAS_USAGE	NdasUsage)
{
	DWORD cbReturned = 0;
	LFS_QUERY_NDASUSAGE_PARAM	ndasUsageParam;
	LFS_QUERY_NDASUSAGE_RETURN	ndasUsageReturn;

	ndasUsageParam.SlotNumber = SlotNumber;
	ndasUsageParam.Reserved = 0;

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_QUERY_NDASUSAGE,
		&ndasUsageParam, sizeof(ndasUsageParam),
		&ndasUsageReturn, sizeof(ndasUsageReturn),
		&cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_QUERY_NDASUSAGE failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	if (NdasUsage) 
	{
		FlaggedValue fv(ndasUsageReturn.NdasUsageFlags);
		NdasUsage->NoDiskVolume = fv.IsFlagSet(LFS_NDASUSAGE_FLAG_NODISKVOL);
		NdasUsage->Attached     = fv.IsFlagSet(LFS_NDASUSAGE_FLAG_ATTACHED);
		NdasUsage->ActPrimary   = fv.IsFlagSet(LFS_NDASUSAGE_FLAG_PRIMARY);
		NdasUsage->ActSecondary = fv.IsFlagSet(LFS_NDASUSAGE_FLAG_SECONDARY);
		NdasUsage->ActReadOnly  = fv.IsFlagSet(LFS_NDASUSAGE_FLAG_READONLY);
		NdasUsage->HasLockedVolume  = fv.IsFlagSet(LFS_NDASUSAGE_FLAG_LOCKED);
		NdasUsage->MountedFSVolumeCount = ndasUsageReturn.MountedFSVolumeCount;
	}

	return TRUE;
}


BOOL WINAPI 
LfsFiltQueryNdasUsage(
	IN DWORD				SlotNumber,
	OUT PLFSCTL_NDAS_USAGE	NdasUsage)
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltQueryNdasUsage(
		hDevice, 
		SlotNumber,
		NdasUsage);
}

/*++

Stop a secondary volume

--*/
BOOL WINAPI LfsFiltStopSecondaryVolume(
		IN HANDLE	hFilter,
		IN DWORD	PhysicalDriveNumber
){
	DWORD cbReturned = 0;
	LFS_STOPSECVOLUME_PARAM	stopSecParam;

	stopSecParam.PhysicalDriveNumber = PhysicalDriveNumber;

	BOOL fSuccess = ::DeviceIoControl(
		hFilter,
		LFS_FILTER_STOP_SECVOLUME,
		&stopSecParam, sizeof(stopSecParam),
		NULL, 0,
		&cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"LFS_FILTER_STOP_SECVOLUME failed, error=0x%X\n",
			GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI LfsFiltStopSecondaryVolume(
		IN DWORD	PhysicalDriveNumber
){
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltStopSecondaryVolume(
		hDevice, 
		PhysicalDriveNumber);
}

/*++

Shutdown LfsFilter without a device file handle

--*/
BOOL WINAPI LfsFiltCtlShutdown()
{
	XTL::AutoFileHandle hDevice = LfsFiltCtlOpenDevice();

	if (hDevice.IsInvalid()) 
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Opening %ls failed, error=0x%X\n",
			LFSFILT_DEVICE_NAME, GetLastError());
		return FALSE;
	}

	return LfsFiltCtlShutdown(hDevice);
}
