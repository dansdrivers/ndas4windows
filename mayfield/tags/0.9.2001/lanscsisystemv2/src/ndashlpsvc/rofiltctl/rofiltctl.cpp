/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

Initial implementation:

2004-6-10 Chesong Lee <cslee@ximeta.com>

Revisons:

--*/

#include "stdafx.h"
#include "rofiltctl.h"

#include <winioctl.h>
#pragma warning(disable: 4200) // disable zero-sized array warning
#include "rofilterioctl.h"
#pragma warning(default: 4200)

#include "ndaserror.h"
#include "xdebug.h"

static LPCTSTR ROFILTER_DEVICE_NAME = _T("\\\\.\\Global\\ROFilt");
static LPCTSTR ROFILTER_DRIVER_NAME = _T("rofilt.sys");
static LPCTSTR ROFILTER_SERVICE_NAME = _T("rofilt");

//-------------------------------------------------------------------------
//
// NdasRoFilterStartService with SCM handle
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterStartService(SC_HANDLE hSCManager)
{
	SC_HANDLE hService = ::OpenService(
		hSCManager,
		ROFILTER_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (NULL == hService) {
		DPErrorEx(TEXT("Failed to open rofilt service.\n"));
		return FALSE;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess) {
		DPErrorEx(TEXT("Failed to start rofilt service.\n"));
		::CloseServiceHandle(hService);
		return FALSE;
	}

	DPInfo(TEXT("rofilt service started successfully.\n"));
	::CloseServiceHandle(hService);
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterStartService without SCM handle
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterStartService()
{
	SC_HANDLE hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (NULL == hSCManager) {
		DPErrorEx(TEXT("Failed to open SCM Database.\n"));
		return FALSE;
	}

	BOOL fSuccess = NdasRoFilterStartService(hSCManager);

	::CloseServiceHandle(hSCManager);

	return fSuccess;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterQueryServiceStatus with SCM handle
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterQueryServiceStatus(SC_HANDLE hSCManager, LPSERVICE_STATUS lpServiceStatus)
{
	SC_HANDLE hService = ::OpenService(
		hSCManager,
		ROFILTER_SERVICE_NAME,
		GENERIC_READ | GENERIC_EXECUTE);

	if (NULL == hService) {
		DPErrorEx(TEXT("Failed to open rofilt service.\n"));
		return FALSE;
	}

	BOOL fSuccess = ::QueryServiceStatus(hService, lpServiceStatus);
	if (!fSuccess) {
		DPErrorEx(TEXT("Failed to start rofilt service.\n"));
		::CloseHandle(hService);
		return FALSE;
	}

	DPInfo(TEXT("rofilt service started successfully.\n"));
	::CloseHandle(hService);
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterQueryServiceStatus without SCM handle
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterQueryServiceStatus(LPSERVICE_STATUS lpServiceStatus)
{
	SC_HANDLE hSCManager = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ);

	if (NULL == hSCManager) {
		DPErrorEx(TEXT("Failed to open SCM Database.\n"));
		return FALSE;
	}

	BOOL fSuccess = NdasRoFilterQueryServiceStatus(hSCManager, lpServiceStatus);

	::CloseServiceHandle(hSCManager);

	return fSuccess;
}

//-------------------------------------------------------------------------
//
// CreateROFilterHandle
//
// Create a handle for ROFilter device object,
// If failed, returns INVALID_HANDLE_VALUE
//
//-------------------------------------------------------------------------

HANDLE NdasRoFilterCreate()
{
	HANDLE hFilter = ::CreateFile(
		ROFILTER_DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		(LPSECURITY_ATTRIBUTES) NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		(HANDLE) NULL);

	if (INVALID_HANDLE_VALUE == hFilter) {
		DPErrorEx(_FT("Failed to open ROFilter interface (%s).\n"), 
			ROFILTER_DEVICE_NAME);
		return INVALID_HANDLE_VALUE;
	}

	return hFilter;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterStart
//
// Start the ROFilter
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterStart(HANDLE hFilter)
{
	BOOL fSuccess(FALSE);

	DWORD dwVersion;
	DWORD cbReturned;
	
	fSuccess = ::DeviceIoControl(
		hFilter,
		IOCTL_FILEMON_VERSION,
		NULL, 0,
		&dwVersion, sizeof(DWORD), &cbReturned, 
		(LPOVERLAPPED) NULL);

	if (FILEMONVERSION != dwVersion) {
		DPError(_FT("ROFilter driver version mismatch (%d), expected %d.\n"),
			dwVersion, FILEMONVERSION);
		::SetLastError(NDASHLPSVC_ERROR_ROFILTER_VERSION_MISMATCH);
		return FALSE;
	}

	fSuccess = ::DeviceIoControl(
		hFilter,
		IOCTL_FILEMON_STARTFILTER,
		NULL, 0,
		NULL, 0, &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("Starting ROFilter failed.\n"));
		return FALSE;
	}

	DPInfo(_FT("ROFilter started successfully.\n"));
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterStop
//
// Stop the ROFilter
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterStop(HANDLE hFilter)
{
	BOOL fSuccess(FALSE);

	DWORD cbReturned;
	fSuccess = ::DeviceIoControl(
		hFilter,
		IOCTL_FILEMON_STOPFILTER,
		NULL, 0,
		NULL, 0, &cbReturned,
		(LPOVERLAPPED) NULL);
	
	if (!fSuccess) {
		DPErrorEx(_FT("Stopping ROFilter failed.\n"));
		return FALSE;
	}

	DPInfo(_FT("ROFilter stopped successfully.\n"));
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterEnable
//
// Enable or Disable filtering on the select drive
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterEnable(HANDLE hFilter, DWORD dwDriveNumber, BOOL bEnable /* = TRUE */)
{
	BOOL fSuccess(FALSE);

	DPInfo(_FT("%s on drive number %d\n"), 
		bEnable ? TEXT("Enable") : TEXT("Disable"),
		dwDriveNumber);

	//
	// Get current ROFilter-enabled drives
	//

	ULONG ulDriveMask(0), ulNewDriveMask(0);
	DWORD cbReturned;
	fSuccess = ::DeviceIoControl(
		hFilter,
		IOCTL_FILEMON_GETDRIVES,
		NULL, 0,
		&ulDriveMask, sizeof(ULONG), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("ROFILTER GETDRIVES failed.\n"));
		return FALSE;
	}

	DPInfo(_FT("Current Drive Mask: 0x%08X\n"), ulDriveMask);

	TCHAR szBuffer[26 * 2 + 1] = {0};
	TCHAR* pszBuffer = szBuffer;
	for (DWORD i = 0; i < 'Z' - 'A' + 1; ++i) {
		if (ulDriveMask & (1 << i)) {
			pszBuffer[0] = i + TEXT('A');
			pszBuffer[1] = TEXT(' ');
			pszBuffer[2] = TEXT('\0');
			pszBuffer += 2;
		}
	}
	_ASSERTE(pszBuffer <= &szBuffer[26 * 2 + 1]);

	DPInfo(_FT("ROFilter is enabled on drive: %s.\n"), 
		(szBuffer[0]) ? szBuffer : TEXT("(none)"));

	//
	// If the current drive is filtered now,
	// unhook the current drive (and hook the drive again.)
	//
	if (bEnable && (ulDriveMask & (1 << dwDriveNumber))) {
		ulDriveMask &= ~(1 << dwDriveNumber);
		fSuccess = ::DeviceIoControl(
			hFilter,
			IOCTL_FILEMON_SETDRIVES,
			&ulDriveMask, sizeof(ULONG),
			&ulNewDriveMask, sizeof(ULONG), &cbReturned,
			(LPOVERLAPPED) NULL);
		if (!fSuccess) {
			DPErrorEx(_FT("ROFILTER SETDRIVES to disable to renew failed.\n"));
			return FALSE;
		}
		DPInfo(_FT("Unhooked Drive Mask: 0x%08X\n"), ulNewDriveMask);

		ulDriveMask = ulNewDriveMask;
	}


	//
	// Rehook the drive
	//
	if (bEnable) {
        ulDriveMask |= (1 << dwDriveNumber);
	} else {
		ulDriveMask &= ~(1 << dwDriveNumber);
	}

	DPInfo(_FT("Drive Mask: 0x%08X\n"), ulDriveMask);

	fSuccess = ::DeviceIoControl(
		hFilter,
		IOCTL_FILEMON_SETDRIVES,
		&ulDriveMask, sizeof(ULONG),
		&ulNewDriveMask, sizeof(ULONG), &cbReturned,
		(LPOVERLAPPED) NULL);
	if (!fSuccess) {
		DPErrorEx(_FT("ROFILTER SETDRIVES to failed.\n"));
		return FALSE;
	}

	DPInfo(_FT("New Drive Mask: 0x%08X\n"), ulNewDriveMask);

	DPInfo(_FT("ROFilter %s successfully on drive %c.\n"), 
		(bEnable) ? TEXT("enabled") : TEXT("disabled"),
		dwDriveNumber + TEXT('A'));
	return TRUE;
}

//-------------------------------------------------------------------------
//
// NdasRoFilterQueryFilteredDrives
//
// Query filtered drives. 
//
//-------------------------------------------------------------------------

BOOL NdasRoFilterQueryFilteredDrives(HANDLE hFilter, LPDWORD lpdwDriveMask)
{
	_ASSERTE(!IsBadWritePtr(lpdwDriveMask, sizeof(DWORD)));

	BOOL fSuccess(FALSE);

	//
	// Get current ROFilter-enabled drives
	//

	ULONG ulDriveMask;
	DWORD cbReturned;
	fSuccess = ::DeviceIoControl(
		hFilter,
		IOCTL_FILEMON_GETDRIVES,
		NULL, 0,
		&ulDriveMask, sizeof(ULONG), &cbReturned,
		(LPOVERLAPPED) NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("ROFILTER GETDRIVES failed.\n"));
		return FALSE;
	}

	TCHAR szBuffer[26 * 2 + 1] = {0};
	TCHAR* pszBuffer = szBuffer;
	for (DWORD i = 0; i < 'Z' - 'A' + 1; ++i) {
		if (ulDriveMask & (1 << i)) {
			pszBuffer[0] = i + TEXT('A');
			pszBuffer[1] = TEXT(' ');
			pszBuffer[2] = TEXT('\0');
			pszBuffer += 2;
		}
	}
	_ASSERTE(pszBuffer <= &szBuffer[26 * 2 + 1]);

	DPInfo(_FT("ROFilter is enabled on drive: (0x%08X) %s.\n"), 
		ulDriveMask, 
		(szBuffer[0]) ? szBuffer : TEXT("(none)"));
	
	*lpdwDriveMask = ulDriveMask;

	return TRUE;
}

