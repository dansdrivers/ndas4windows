/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

Initial implementation:

2004-6-10 Chesong Lee <cslee@ximeta.com>

Revisons:

--*/
#ifndef _ROFILTCTL_H_
#define _ROFILTCTL_H_
#pragma once

#define XDF_ROFILTCTL 0x00000040

//
// At this time, ROFilter (rofilt) has a session,
// which means, when you create an handle and close it,
// all filtering sessions will be gone.
// 
// To safely filter the volume,
//
// 1. Check the service status with NdasRofilterQueryServiceStatus
// 2. If the service is not started, start it with NdasRoFilterStartService
//
// 3. Create a handle with NdasRoFilter function and save a handle
//    Do not close the handle, until you stops all filters.
// 4. Start filtering with NdasRoFilterStartFilter
// 5. ... Enable/Disable/Query filtered drive(s)...
// 6. Stop filtering with NdasRoFilterStopFilter
// 7. Close the handle.
// 
// * Remark: 
// Once the service is started, you cannot stop the service,
// as this is a kernel filter driver.
// 
// Multiple sessions may not be supported at this time.
//
// BUGBUG:
// Currently, we can only support filtering by a DOS drive letter (A-Z),
// which restricts the use of volumes with mount-point without drive letters.
// This should be fixed later.
// To fix this, we should add features to rofilt.sys and
// extend these interfaces.
//

//
// Create a device file handle for ROFilt
//
HANDLE WINAPI NdasRoFilterOpenDevice();

//
// Start filtering on this session
//
// You can safely start filtering and enable the filter on
// volumes
//
BOOL WINAPI NdasRoFilterStartFilter(HANDLE hFilter);

//
// Stop filtering on this session
//
BOOL WINAPI NdasRoFilterStopFilter(HANDLE hFilter);

//
// Enable/Disable filtering on a selected driver letter (Drive Number)
// 


BOOL WINAPI NdasRoFilterEnableFilter(HANDLE hFilter, DWORD szDriveNumber, BOOL bEnable);

//
// Query filtered drives
//

BOOL WINAPI NdasRoFilterQueryFilteredDrives(HANDLE hFilter, LPDWORD lpdwDriveMask);

//
// Start ROFilt service
//
BOOL WINAPI NdasRoFilterStartService(SC_HANDLE hSCManager);

#ifdef __cplusplus
BOOL WINAPI NdasRoFilterStartService();
#endif

//
// Query ROFilt service status
//
BOOL WINAPI NdasRoFilterQueryServiceStatus(SC_HANDLE hSCManager, LPSERVICE_STATUS lpServiceStatus);

#ifdef __cplusplus
BOOL WINAPI NdasRoFilterQueryServiceStatus(LPSERVICE_STATUS lpServiceStatus);
#endif

#endif /* _ROFILTERCTL_H_ */