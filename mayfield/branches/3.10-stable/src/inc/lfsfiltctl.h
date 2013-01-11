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

#define XDF_LFSFILTCTL 0x00000080

/*++

Create a device file handle for ROFilt

--*/
HANDLE WINAPI LfsFiltCtlOpenDevice();

/*++

Start LFS Filter service

--*/
BOOL WINAPI LfsFiltCtlStartService();

#ifdef __cplusplus
BOOL WINAPI LfsFiltCtlStartService(SC_HANDLE hSCManager);
#endif

/*++

Query ROFilt service status 

--*/
BOOL WINAPI LfsFiltCtlQueryServiceStatus(SC_HANDLE hSCManager, LPSERVICE_STATUS lpServiceStatus);

#ifdef __cplusplus
BOOL WINAPI LfsFiltCtlQueryServiceStatus(LPSERVICE_STATUS lpServiceStatus);
#endif

/*++

Get LfsFilter Version

--*/

BOOL WINAPI LfsFiltCtlGetVersion(
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,	LPWORD lpwNDFSVerMinor);

#ifdef __cplusplus
BOOL WINAPI LfsFiltCtlGetVersion(
	HANDLE hFilter,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor);
#endif

/*++ 

Shutdown LfsFilter

Explicit call to shut down LfsFilter when shutting down the system. 

--*/

BOOL WINAPI LfsFiltCtlShutdown();

#ifdef __cplusplus
BOOL WINAPI LfsFiltCtlShutdown(HANDLE hFilter);
#endif


#endif /* _LFSFILTERCTL_H_ */
