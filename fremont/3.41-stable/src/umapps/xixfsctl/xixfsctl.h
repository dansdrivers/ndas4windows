/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

Initial implementation:

2004-8-15 Chesong Lee <cslee@ximeta.com>

Revisons:

--*/
#ifndef _XIXFS_CTL_H_
#define _XIXFS_CTL_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #define XDF_XIXFSCTL 0x00000080

/*++

Create a device file handle for XIXFS

--*/

HRESULT
WINAPI
XixfsCtlOpenControlDevice(
	__deref_out HANDLE* ControlDeviceHandle);

/*++

Start XIXFS driver service

--*/

HRESULT
WINAPI
XixfsCtlStartService();

/*++

Query Xixfs service status 

--*/

HRESULT
WINAPI
XixfsCtlQueryServiceStatus(
	__out LPSERVICE_STATUS lpServiceStatus);

/*++

Get Xixfs Version

--*/

HRESULT
WINAPI
XixfsCtlGetVersionByHandle(
	HANDLE hXixfsControlDevice,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor);

HRESULT
WINAPI 
XixfsCtlGetVersion(
	LPWORD lpwVerMajor, 
	LPWORD lpwVerMinor,
	LPWORD lpwVerBuild,
	LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,
	LPWORD lpwNDFSVerMinor);

HRESULT
WINAPI
XixfsCtlShutdownByHandle(
	HANDLE ControlDeviceHandle);

HRESULT
WINAPI
XixfsCtlShutdown();

#ifdef __cplusplus
}
#endif

#endif /* _XixfsERCTL_H_ */
