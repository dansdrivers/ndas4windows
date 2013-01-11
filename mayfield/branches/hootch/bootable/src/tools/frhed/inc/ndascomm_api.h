/*++

  NDAS Communication Access API Header

  Copyright (C) 2002-2004 XIMETA, Inc.
  All rights reserved.

--*/

#ifndef _NDASCOMM_H_
#define _NDASCOMM_H_

#pragma once

#if defined(_WINDOWS_)

#ifndef _INC_WINDOWS
	#error ndascomm_api.h requires windows.h to be included first
#endif

#ifdef NDASCOMM_EXPORTS
#define NDASCOMM_API __declspec(dllexport)
#else
#define NDASCOMM_API __declspec(dllimport)
#endif

#else /* defined(WIN32) || defined(UNDER_CE) */
#define NDASCOMM_API 
#endif

#ifndef _NDAS_TYPE_H_
#include "ndastype.h"
#endif /* _NDAS_TYPE_H_ */

#ifdef __cplusplus
extern "C" {
#endif

/* 

Use the following definition for WinCE specifics
#if defined(_WIN32_WCE_)
#endif

*/

#define NDASCOMM_API_VERSION_MAJOR 0
#define NDASCOMM_API_VERSION_MINOR 1

/*++

NdasCommInitialize function must be called before call any other functions in ndascomm library

Return Values:

If the function succeeds, the return value is non-zero.

If the function fails, the return value is zero. To get extended error 
information, call GetLastError.

--*/

NDASCOMM_API
BOOL
__stdcall
NdasCommInitialize();

/*++

NdasCommGetAPIVersion function returns the current version information
of the loaded library

Return Values:

If the function succeeds, the return value is handle to operate NDAS Device.

Low word contains the major version number and high word the minor
version number.

--*/

typedef void* HNDAS;

NDASCOMM_API
DWORD
__stdcall
NdasCommGetAPIVersion();

/*++

NdasCommConnect function connects to NDAS Devices using given ID, KEY

Parameters:

pConnectionInfo
	[in] Pointer to a NDAS_CONNECTION_INFO structure that contains 
	connection informations of NDAS Device and unit number

Return Values:

	If the function succeeds, the return value is handle to operate NDAS Device.

	If the function fails, the return value is NULL. To get extended error 
	information, call GetLastError.

--*/

NDASCOMM_API
HNDAS
__stdcall
NdasCommConnect(
	PNDAS_CONNECTION_INFO pConnectionInfo);


/*++

NdasCommDisconnect function disconnects from NDAS Devices and release resources
You MUST call NdasCommDisconnect after the job for NDAS Device is complete.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/

NDASCOMM_API
BOOL
__stdcall
NdasCommDisconnect(
	HNDAS hNDASDevice);

/*++

The NdasCommBlockDeviceRead function reads up to usSectorCount of size sectors from the NDAS device and stores in pData.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

i64Location
	[in] NDAS Device position in sector to read from

usSectorCount
	[in] Number of sectors to read

pData
	[out] Storage location for data. The size of pData must be at least usSectorCount * LANSCSI_BLOCK_SIZE bytes.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/

NDASCOMM_API
BOOL
__stdcall
NdasCommBlockDeviceRead(
	HNDAS	hNDASDevice,
	INT64	i64Location,
	UINT16	usSectorCount,
	PCHAR	pData);

/*++

The NdasCommBlockDeviceWrite function writes up to usSectorCount of size sectors to the NDAS device.
If the connection to NDAS devices is using encryption, the contents in pData will be corrupted,
if you don't want so, use NdasCommBlockDeviceWriteSafeBuffer

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

i64Location
	[in] NDAS Device position in sector to read from

usSectorCount
	[in] Number of sectors to read

pData
	[in out] Storage location for data. The size of pData must be at least usSectorCount * LANSCSI_BLOCK_SIZE bytes.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/

NDASCOMM_API
BOOL
__stdcall
NdasCommBlockDeviceWrite(
	HNDAS	hNDASDevice,
	INT64	i64Location,
	UINT16	usSectorCount,
	PCHAR	pData);

/*++

The NdasCommBlockDeviceWriteSafeBuffer function acts as NdasCommBlockDeviceWrite if the connection does not use encryption.
If the connection uses data encryption, NdasCommBlockDeviceWriteSafeBuffer uses internal buffer as data buffer so that
the contents in pData should not be corrupted.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

i64Location
	[in] NDAS Device position in sector to write to

usSectorCount
	[in] Number of sectors to write

pData
	[in] Storage location for data. The size of pData must be at least usSectorCount * LANSCSI_BLOCK_SIZE bytes.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/

NDASCOMM_API
BOOL
__stdcall
NdasCommBlockDeviceWriteSafeBuffer(
	HNDAS	hNDASDevice,
	INT64	i64Location,
	UINT16	usSectorCount,
	PCHAR	pData);


/*++

The NdasCommBlockDeviceVerify function sends verify command to NDAS device and receives a result.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

i64Location
	[in] NDAS Device position in sector to verify

usSectorCount
	[in] Number of sectors to verify

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/
NDASCOMM_API
BOOL
__stdcall
NdasCommBlockDeviceVerify(
						  HNDAS	hNDASDevice,
						  INT64	i64Location,
						  UINT16	usSectorCount
						  );

/*++

The NdasCommBlockDeviceVerify function sends set feature command to NDAS device.
You must NOT call this function if you don't know what really this is.
No additional information.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

feature
	[in] feature

param
	[in] parameter for feature

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/
NDASCOMM_API
BOOL
__stdcall
NdasCommBlockDeviceSetFeature(
	HNDAS hNDASDevice,
	BYTE feature,
	UINT16 param
	);

/*++

The NdasCommGetDeviceInfo function get device informations which does not
change dynamically. You may call this function once after connected.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

pInfo
	[in] Pointer to PNDAS_DEVICE_INFO structure to retrieve information.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/
NDASCOMM_API
BOOL
__stdcall
NdasCommGetDeviceInfo(
	HNDAS hNDASDevice,
	PNDAS_DEVICE_INFO pInfo);

/*++

The NdasCommGetUnitDeviceInfo function get unit device informations which does not
change dynamically. You may call this function once after connected.

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

pUnitInfo
	[in] Pointer to NDAS_UNIT_DEVICE_INFO structure to retrieve information.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/
NDASCOMM_API
BOOL
__stdcall
NdasCommGetUnitDeviceInfo(
	HNDAS hNDASDevice,
	PNDAS_UNIT_DEVICE_INFO pUnitInfo);

/*++

The NdasCommGetUnitDeviceDynInfo function get device informations which
change dynamically. You need to call this function whenever you want the
CURRENT status.

Parameters:

pConnectionInfo
	[in] Pointer to a NDAS_CONNECTION_INFO structure that contains 
	connection informations of NDAS Device and unit number

pUnitDynInfo
	[in] Pointer to PNDAS_UNIT_DEVICE_DYN_INFO structure to retrieve information.

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/
NDASCOMM_API
BOOL
__stdcall
NdasCommGetUnitDeviceDynInfo(
	PNDAS_CONNECTION_INFO pConnectionInfo,
	PNDAS_UNIT_DEVICE_DYN_INFO pUnitDynInfo);

/*++

The NdasCommGetDeviceId function copies device id of hNdasDevice to pDeviceId

Parameters:

hNDASDevice
	[in] Handle to operate NDAS Device.

pDeviceId
	[out] Buffer to receive Device ID

Return Values:

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.

--*/
NDASCOMM_API
BOOL
__stdcall
NdasCommGetDeviceId(
	HNDAS hNDASDevice,
	BYTE *pDeviceId,
	BYTE *pUnitNo);


#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASCOMM_H_ */
