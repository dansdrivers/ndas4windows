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

#ifndef EXTERN_C
#ifdef __cplusplus
    #define EXTERN_C    extern "C"
#else
    #define EXTERN_C    extern
#endif
#endif

#define XDF_XIXFSCTL 0x00000080


/*++

Create a device file handle for ROFilt

--*/
EXTERN_C
HANDLE 
WINAPI 
XixfsCtlOpenDevice();

/*++

Start LFS Filter service

--*/

EXTERN_C
BOOL 
WINAPI 
XixfsCtlStartService(SC_HANDLE hSCManager);

#ifdef __cplusplus
BOOL 
WINAPI 
XixfsCtlStartService();
#endif /* __cplusplus */

/*++

Query Xixfs service status 

--*/
EXTERN_C
BOOL 
WINAPI 
XixfsCtlQueryServiceStatus(
	SC_HANDLE hSCManager, 
	LPSERVICE_STATUS lpServiceStatus);

#ifdef __cplusplus
BOOL 
WINAPI 
XixfsCtlQueryServiceStatus(
	LPSERVICE_STATUS lpServiceStatus);
#endif /* __cplusplus */

/*++

Get Xixfs Version

--*/

EXTERN_C
BOOL 
WINAPI 
XixfsCtlGetVersion(
	HANDLE hFilter,
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor, LPWORD lpwNDFSVerMinor);

#ifdef __cplusplus
BOOL 
WINAPI 
XixfsCtlGetVersion(
	LPWORD lpwVerMajor, LPWORD lpwVerMinor,
	LPWORD lpwVerBuild, LPWORD lpwVerPrivate,
	LPWORD lpwNDFSVerMajor,	LPWORD lpwNDFSVerMinor);
#endif /* __cplusplus */

BOOL 
WINAPI 
XixfsCtlReadyForUnload(
	HANDLE hXixfsControlDevice);

#ifdef __cplusplus
BOOL 
WINAPI 
XixfsCtlReadyForUnload(VOID);
#endif /* __cplusplus */

#endif /* _XixfsERCTL_H_ */
