/*++

  NDAS USER API Header

  Copyright (C) 2002-2004 XIMETA, Inc.
  All rights reserved.

--*/

#ifndef _NDASUSER_H_
#define _NDASUSER_H_

#pragma once

#if defined(_WINDOWS_)

#ifndef _INC_WINDOWS
	#error ndasuser.h requires windows.h to be included first
#endif

#ifdef NDASUSER_EXPORTS
#define NDASUSER_API __declspec(dllexport)
#else
#define NDASUSER_API __declspec(dllimport)
#endif

#else /* defined(WIN32) || defined(UNDER_CE) */
#define NDASUSER_API 
#endif

#ifndef _NDAS_TYPE_H_
#include "ndastype.h"
#endif /* _NDAS_TYPE_H_ */

#ifndef _NDAS_EVENT_H_
#include "ndasevent.h"
#endif /* _NDAS_EVENT_H_ */

#ifndef _NDAS_SERVICE_PARAM_H_
#include "ndassvcparam.h"
#endif /* _NDAS_SERVICE_PARAM_H_ */

#ifdef __cplusplus
extern "C" {
#endif

/* 

Use the following definition for WinCE specifics
#if defined(_WIN32_WCE_)
#endif

*/

#define NDASUSER_API_VERSION_MAJOR 0
#define NDASUSER_API_VERSION_MINOR 8

/*++

NdasGetApiVersion function returns the current version information
of the loaded library
  
Return Values:

Low word contains the major version number and high word the minor
version number.
  
--*/

NDASUSER_API
DWORD
NdasGetAPIVersion();

/*++

NdasValidateStringIdKey function is used for validating a device string 
ID and write key.

Parameters:

lpszDeviceStringId
  [in] Pointer to a null-terminated string specifying the device id
       to validate.

lpszDeviceStringKey
  [in] Pointer to a null-terminated string specifying the device
       (write) key to validate. 

       If lpszDeviceStringKey is NULL, NdasValidateStringIdKey
       ignores this parameter and checks the validation of device
       ID only.

Return Values:

  If the function succeeds, the return value is non-zero.
 
  If the function fails, the return value is zero. To get extended error 
  information, call GetLastError.

--*/

NDASUSER_API
BOOL 
WINAPI
NdasValidateStringIdKeyW(
	LPCWSTR lpszDeviceStringId, 
	LPCWSTR lpszDeviceStringKey);

#ifdef UNICODE
#define NdasValidateStringIdKey NdasValidateStringIdKeyW
#else
#define NdasValidateStringIdKey NdasValidateStringIdKeyA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Register Device
//
//////////////////////////////////////////////////////////////////////////
//
// The NdasRegisterDevice function registers the new device entry to
// the Device Registrar.
//
// Parameters:
//
// lpszDeviceStringId
//		[in] Pointer to a null-terminated string specifying the device id
//           of the registering device which consists of 20 chars without
//           dashes. String ID should be 20 characters in length and
//           it should be a valid one.
// 
// lpszDeviceStringKey
//		[in] Pointer to a null-terminated string specifying the device
//           (write) key of the registering device which consists of
//           5 chars.
//
//           If lpszDeviceStringKey is NULL, a device is registered
//           with read-only access.
//
//           If lpszDeviceStringKey is non-NULL, and if it is valid,
//           a device is registered with read-write access.
//           
// lpszDeviceName
//		[in] Pointer to a null-terminated string specifying the name
//           of a registering device with MAX_NDAS_DEVICE_NAME_LEN
//           in its length excluding terminating NULL character.
//
//           The name is supplied for the user's conveniences,
//           and it is not validated for the duplicate name for
//           the existing devices.
//
// Return Values:
//
// If the function succeeds, the return value is the registerd slot number
// of the device in the Device Registrar. Valid slot number is
// starting from 1 (One). 0 (Zero) is not used and indicates an invalid
// slot number.
// 
// If the function fails, the return value is zero. To get extended error 
// information, call GetLastError.
//	
NDASUSER_API
DWORD
WINAPI
NdasRegisterDeviceW(
	LPCWSTR lpszDeviceStringId,
	LPCWSTR lpszDeviceStringKey,
	LPCWSTR lpszDeviceName);

NDASUSER_API
DWORD
WINAPI
NdasRegisterDeviceA(
	LPCSTR lpszDeviceStringId,
	LPCSTR lpszDeviceStringKey,
	LPCSTR lpszDeviceName);

#ifdef UNICODE
#define NdasRegisterDevice NdasRegisterDeviceW
#else
#define NdasRegisterDevice NdasRegisterDeviceA
#endif

/*++

Get Registration Data

CURRENTLY NOT IMPLEMENTED

Parameters:

lpRegistrationData
  [in] Vendor-specific registration information to be 
       stored in the device registrar

cbRegistrationData
  [in] Size of lpRegistrationData in bytes.
       Maximum allowed size is defined as MAX_NDAS_REGISTRATION_DATA

--*/

NDASUSER_API
BOOL
WINAPI
NdasGetRegistrationData(
	 IN	DWORD dwSlotNo,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

NDASUSER_API
BOOL
WINAPI
NdasGetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

NDASUSER_API
BOOL
WINAPI
NdasGetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

#ifdef UNICODE
#define NdasGetRegistrationDataById NdasGetRegistrationDataByIdW
#else
#define NdasGetRegistrationDataById NdasGetRegistrationDataByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Set Registration Data
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasSetRegistrationData(
	 IN	DWORD dwSlotNo,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

NDASUSER_API
BOOL
WINAPI
NdasSetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 IN LPVOID lpBuffer,
	 IN DWORD cbBuffer);

NDASUSER_API
BOOL
WINAPI
NdasSetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 OUT LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

#ifdef UNICODE
#define NdasSetRegistrationDataById NdasSetRegistrationDataByIdW
#else
#define NdasSetRegistrationDataById NdasSetRegistrationDataByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Enumerate Device
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDASUSER_DEVICE_ENUM_ENTRYW {
	WCHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	WCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	DWORD SlotNo;
} NDASUSER_DEVICE_ENUM_ENTRYW, *PNDASUSER_DEVICE_ENUM_ENTRYW;

typedef struct _NDASUSER_DEVICE_ENUM_ENTRYA {
	CHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	CHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	DWORD SlotNo;
} NDASUSER_DEVICE_ENUM_ENTRYA, *PNDASUSER_DEVICE_ENUM_ENTRYA;

#ifdef UNICODE
#define NDASUSER_DEVICE_ENUM_ENTRY NDASUSER_DEVICE_ENUM_ENTRYW
#define PNDASUSER_DEVICE_ENUM_ENTRY PNDASUSER_DEVICE_ENUM_ENTRYW
#else
#define NDASUSER_DEVICE_ENUM_ENTRY NDASUSER_DEVICE_ENUM_ENTRYA
#define PNDASUSER_DEVICE_ENUM_ENTRY PNDASUSER_DEVICE_ENUM_ENTRYA
#endif

//
// lpEnumEntry
//	[in] Pointer to an NDAS device enumerate entry
// lpContext
//	[in] Application-defined value specified in the NdasEnumDevices function.
//

typedef BOOL (CALLBACK* NDASDEVICEENUMPROCW)(
	PNDASUSER_DEVICE_ENUM_ENTRYW lpEnumEntry, LPVOID lpContext);
typedef BOOL (CALLBACK* NDASDEVICEENUMPROCA)(
	PNDASUSER_DEVICE_ENUM_ENTRYA lpEnumEntry, LPVOID lpContext);

#ifdef UNICODE
#define NDASDEVICEENUMPROC NDASDEVICEENUMPROCW
#else
#define NDASDEVICEENUMPROC NDASDEVICEENUMPROCA
#endif

//
// lpFnumFunc:
//	[in] Pointer to an application-defined EnumNdasDeviceProc callback function
// lpContext:
//  [in] Application-defined value to be passed to the callback function
//

NDASUSER_API 
BOOL 
WINAPI
NdasEnumDevicesW(NDASDEVICEENUMPROCW lpEnumFunc, LPVOID lpContext);

#ifdef UNICODE
#define NdasEnumDevices NdasEnumDevicesW
#else
#define NdasEnumDevices NdasEnumDevicesA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Enable Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasEnableDevice(DWORD dwSlotNo, BOOL bEnable);

NDASUSER_API
BOOL 
WINAPI
NdasEnableDeviceByIdW(LPCWSTR lpszDeviceStringId, BOOL bEnable);

NDASUSER_API
BOOL 
NdasEnableDeviceByIdA(LPCSTR lpszDeviceStringId, BOOL bEnable);

#ifdef UNICODE
#define NdasEnableDeviceById NdasEnableDeviceByIdW
#else
#define NdasEnableDeviceById NdasEnableDeviceByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Unregister Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasUnregisterDevice(DWORD dwSlotNo);

NDASUSER_API
BOOL 
WINAPI
NdasUnregisterDeviceByIdW(LPCWSTR lpszDeviceStringId);

NDASUSER_API
BOOL 
WINAPI
NdasUnregisterDeviceByIdA(LPCSTR lpszDeviceStringId);

#ifdef UNICODE
#define NdasUnregisterDeviceById NdasUnregisterDeviceByIdW
#else
#define NdasUnregisterDeviceById NdasUnregisterDeviceByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Query Device Status
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceStatus(
	DWORD dwSlotNo, 
	NDAS_DEVICE_STATUS* pStatus,
	NDAS_DEVICE_ERROR* pLastError);

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceStatusByIdW(
	LPCWSTR lpszDeviceStringId, 
	NDAS_DEVICE_STATUS* pStatus,
	NDAS_DEVICE_ERROR* pLastError);

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceStatusByIdA(
	LPCSTR lpszDeviceStringId, 
	NDAS_DEVICE_STATUS* pStatus,
	NDAS_DEVICE_ERROR* pLastError);

#ifdef UNICODE
#define NdasQueryDeviceStatusById NdasQueryDeviceStatusByIdW
#else
#define NdasQueryDeviceStatusById NdasQueryDeviceStatusByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Query Device Information
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDASUSER_DEVICE_INFORMATIONA {
	DWORD SlotNo;
	WCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
	CHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	NDAS_DEVICE_HW_INFORMATION HardwareInfo;
	NDAS_DEVICE_PARAMS DeviceParams;
} NDASUSER_DEVICE_INFORMATIONA, *PNDASUSER_DEVICE_INFORMATIONA;

typedef struct _NDASUSER_DEVICE_INFORMATIONW {
	DWORD SlotNo;
	WCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
	WCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	NDAS_DEVICE_HW_INFORMATION HardwareInfo;
	NDAS_DEVICE_PARAMS DeviceParams;
} NDASUSER_DEVICE_INFORMATIONW, *PNDASUSER_DEVICE_INFORMATIONW;

#ifdef UNICODE
#define NDASUSER_DEVICE_INFORMATION NDASUSER_DEVICE_INFORMATIONW
#define PNDASUSER_DEVICE_INFORMATION PNDASUSER_DEVICE_INFORMATIONW
#else
#define NDASUSER_DEVICE_INFORMATION NDASUSER_DEVICE_INFORMATIONA
#define PNDASUSER_DEVICE_INFORMATION PNDASUSER_DEVICE_INFORMATIONA
#endif

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceInformationW(
	DWORD dwSlotNo, 
	NDASUSER_DEVICE_INFORMATIONW* pDeviceInfo);

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceInformationA(
	DWORD dwSlotNo, 
	NDASUSER_DEVICE_INFORMATIONA* pDeviceInfo);

#ifdef UNICODE
#define NdasQueryDeviceInformation NdasQueryDeviceInformationW
#else
#define NdasQueryDeviceInformation NdasQueryDeviceInformationA
#endif

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceInformationByIdW(
	LPCWSTR lpszDeviceStringId, 
	NDASUSER_DEVICE_INFORMATIONW* pDeviceInfo);

NDASUSER_API
BOOL 
WINAPI
NdasQueryDeviceInformationByIdA(
	LPCSTR lpszDeviceStringId, 
	NDASUSER_DEVICE_INFORMATIONA* pDeviceInfo);

#ifdef UNICODE
#define NdasQueryDeviceInformationById NdasQueryDeviceInformationByIdW
#else
#define NdasQueryDeviceInformationById NdasQueryDeviceInformationByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Set Device Name
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasSetDeviceNameW(
	DWORD dwSlotNo, 
	LPCWSTR lpszDeviceName);

#ifdef UNICODE
#define NdasSetDeviceName NdasSetDeviceNameW
#else
#define NdasSetDeviceName NdasSetDeviceNameA
#endif

NDASUSER_API
BOOL 
WINAPI
NdasSetDeviceNameByIdW(
	LPCWSTR lpszDeviceStringId, 
	LPCWSTR lpszDeviceName);

#ifdef UNICODE
#define NdasSetDeviceNameById NdasSetDeviceNameByIdW
#else
#define NdasSetDeviceNameById NdasSetDeviceNameByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Change Device Access Rights
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasSetDeviceAccessByIdW(
	LPCWSTR lpszDeviceStringId, 
	BOOL bWriteAccess, 
	LPCWSTR lpszDeviceStringKey);

#ifdef UNICODE
#define NdasSetDeviceAccessById NdasSetDeviceAccessByIdW
#else
#define NdasSetDeviceAccessById NdasSetDeviceAccessByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Enumerate Unit Devices
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDASUSER_UNITDEVICE_ENUM_ENTRY {
	DWORD UnitNo;
	NDAS_UNITDEVICE_TYPE UnitDeviceType;
} NDASUSER_UNITDEVICE_ENUM_ENTRY, *PNDASUSER_UNITDEVICE_ENUM_ENTRY;

typedef BOOL (CALLBACK* NDASUNITDEVICEENUMPROC)(
	PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, LPVOID lpContext);

NDASUSER_API
BOOL 
WINAPI
NdasEnumUnitDevices(
	DWORD dwSlotNo, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

NDASUSER_API
BOOL 
WINAPI
NdasEnumUnitDevicesByIdW(
	LPCWSTR lpszDeviceStringId, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

NDASUSER_API
BOOL 
WINAPI
NdasEnumUnitDevicesByIdA(
	LPCSTR lpszDeviceStringId, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

#ifdef UNICODE
#define NdasEnumUnitDevicesById NdasEnumUnitDevicesByIdW
#else
#define NdasEnumUnitDevicesById NdasEnumUnitDevicesByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Query Unit Device Status
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasQueryUnitDeviceStatus(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

NDASUSER_API
BOOL 
WINAPI
NdasQueryUnitDeviceStatusByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

NDASUSER_API
BOOL 
WINAPI
NdasQueryUnitDeviceStatusByIdA(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

#ifdef UNICODE
#define NdasQueryUnitDeviceStatusById NdasQueryUnitDeviceStatusByIdW
#else
#define NdasQueryUnitDeviceStatusById NdasQueryUnitDeviceStatusByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Query Unit Device Information
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDASUSER_UNITDEVICE_INFORMATIONW {
	NDAS_UNITDEVICE_TYPE UnitDeviceType;
	NDAS_UNITDEVICE_SUBTYPE UnitDeviceSubType;
	NDAS_UNITDEVICE_HW_INFORMATIONW HardwareInfo;
	NDAS_UNITDEVICE_PARAMS UnitDeviceParams;
} NDASUSER_UNITDEVICE_INFORMATIONW, *PNDASUSER_UNITDEVICE_INFORMATIONW;

typedef struct _NDASUSER_UNITDEVICE_INFORMATIONA {
	NDAS_UNITDEVICE_TYPE UnitDeviceType;
	NDAS_UNITDEVICE_SUBTYPE UnitDeviceSubType;
	NDAS_UNITDEVICE_HW_INFORMATIONA HardwareInfo;
	NDAS_UNITDEVICE_PARAMS UnitDeviceParams;
} NDASUSER_UNITDEVICE_INFORMATIONA, *PNDASUSER_UNITDEVICE_INFORMATIONA;

#ifdef UNICODE
#define NDASUSER_UNITDEVICE_INFORMATION NDASUSER_UNITDEVICE_INFORMATIONW
#else
#define NDASUSER_UNITDEVICE_INFORMATION NDASUSER_UNITDEVICE_INFORMATIONA
#endif

NDASUSER_API
BOOL 
WINAPI
NdasQueryUnitDeviceInformationW(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo);

#ifdef UNICODE
#define NdasQueryUnitDeviceInformation NdasQueryUnitDeviceInformationW
#else
#define NdasQueryUnitDeviceInformation NdasQueryUnitDeviceInformationA
#endif

NDASUSER_API
BOOL 
WINAPI
NdasQueryUnitDeviceInformationByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo);

#ifdef UNICODE
#define NdasQueryUnitDeviceInformationById NdasQueryUnitDeviceInformationByIdW
#else
#define NdasQueryUnitDeviceInformationById NdasQueryUnitDeviceInformationByIdA
#endif

NDASUSER_API
BOOL
WINAPI
NdasQueryUnitDeviceHostStats(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	LPDWORD lpnROHosts, LPDWORD lpnRWHosts);

//////////////////////////////////////////////////////////////////////////
//
// Find Logical Device of the Unit Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL 
WINAPI
NdasFindLogicalDeviceOfUnitDevice(
	DWORD dwSlotNo, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

NDASUSER_API
BOOL 
WINAPI
NdasFindLogicalDeviceOfUnitDeviceByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

NDASUSER_API
BOOL 
WINAPI
NdasFindLogicalDeviceOfUnitDeviceByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo,
	NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

#ifdef UNICODE
#define NdasFindLogicalDeviceOfUnitDeviceById NdasFindLogicalDeviceOfUnitDeviceByIdW
#else
#define NdasFindLogicalDeviceOfUnitDeviceById NdasFindLogicalDeviceOfUnitDeviceByIdA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Plug In Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API 
BOOL 
WINAPI
NdasPlugInLogicalDevice(
	BOOL bWritable, 
	NDAS_LOGICALDEVICE_ID logicalDeviceId);

//////////////////////////////////////////////////////////////////////////
//
// Eject Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasEjectLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId);

//////////////////////////////////////////////////////////////////////////
//
// Recover Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasRecoverLogicalDevice(
					   NDAS_LOGICALDEVICE_ID logicalDeviceId);

//////////////////////////////////////////////////////////////////////////
//
// Unplug Logical Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasUnplugLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId);

//////////////////////////////////////////////////////////////////////////
//
// Enumerate Logical Devices
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDASUSER_LOGICALDEVICE_ENUM_ENTRY {
	NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	NDAS_LOGICALDEVICE_TYPE Type;
} NDASUSER_LOGICALDEVICE_ENUM_ENTRY, *PNDASUSER_LOGICALDEVICE_ENUM_ENTRY;

typedef BOOL (CALLBACK* NDASLOGICALDEVICEENUMPROC)(
	PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry, 
	LPVOID lpContext);

NDASUSER_API 
BOOL 
WINAPI
NdasEnumLogicalDevices(
	NDASLOGICALDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

//////////////////////////////////////////////////////////////////////////
//
// Query Logical Device Status
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasQueryLogicalDeviceStatus(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	NDAS_LOGICALDEVICE_STATUS* pStatus,
	NDAS_LOGICALDEVICE_ERROR* pLastError);

//////////////////////////////////////////////////////////////////////////
//
// Query Hosts for Logical Device
//
//////////////////////////////////////////////////////////////////////////

//
// lpHostInfo members and their data pointed by members are valid 
// only for the scope of the enumerator function.
// If any data is need persistent should be copied to another buffer
//
typedef BOOL (CALLBACK* NDASQUERYHOSTENUMPROC)(
	CONST GUID* lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext);

NDASUSER_API
BOOL
WINAPI
NdasQueryHostsForLogicalDevice(
	NDAS_LOGICALDEVICE_ID logicalDeviceId, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);


//////////////////////////////////////////////////////////////////////////
//
// Query Hosts for Unit Device
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasQueryHostsForUnitDevice(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

//////////////////////////////////////////////////////////////////////////
//
// Query Hosts with Host GUID
//
//////////////////////////////////////////////////////////////////////////

#include "ndashostinfo.h"

NDASUSER_API
BOOL
WINAPI
NdasQueryHostInfoW(
	IN LPCGUID lpHostGuid, 
	IN OUT NDAS_HOST_INFOW* pHostInfo);

NDASUSER_API
BOOL
WINAPI
NdasQueryHostInfoA(
	IN LPCGUID lpHostGuid, 
	IN OUT NDAS_HOST_INFOA* pHostInfo);

#ifdef UNICODE
#define NdasQueryHostInfo NdasQueryHostInfoW
#else
#define NdasQueryHostInfo NdasQueryHostInfoA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Request Surrender Access to the NDAS hosts
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasRequestSurrenderAccess(
	IN LPCGUID lpHostGuid,
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access);

//////////////////////////////////////////////////////////////////////////
//
// Query Logical Device Information
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW {
	DWORD Index;
	WCHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	DWORD UnitNo;
	DWORD Blocks;
} NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW, *PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYW;

typedef struct _NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA {
	DWORD Index;
	CHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	DWORD UnitNo;
	DWORD Blocks;
} NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA, *PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYA;

#ifdef UNICODE
#define NDASUSER_LOGICALDEVICE_MEMBER_ENTRY NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW
#define PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYW
#else
#define NDASUSER_LOGICALDEVICE_MEMBER_ENTRY NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA
#define PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYA
#endif

typedef struct _NDASUSER_LOGICALDEVICE_INFORMATIONW {

	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
	ACCESS_MASK GrantedAccess;
	ACCESS_MASK MountedAccess;

	union {
		struct {
			DWORD Blocks;
		} LogicalDiskInformation;

		struct {
			DWORD Reserved;
		} LogicalDVDInformation;

		BYTE Reserved[48];
	};

	DWORD nUnitDeviceEntries;
	NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW FirstUnitDevice;
	NDAS_LOGICALDEVICE_PARAMS LogicalDeviceParams;

} NDASUSER_LOGICALDEVICE_INFORMATIONW, *PNDASUSER_LOGICALDEVICE_INFORMATIONW;

typedef struct _NDASUSER_LOGICALDEVICE_INFORMATIONA {

	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
	ACCESS_MASK GrantedAccess;
	ACCESS_MASK MountedAccess;

	union {

		struct {
			DWORD Blocks;
		} LogicalDiskInformation;

		struct {
			DWORD Reserved;
		} LogicalDVDInformation;

		BYTE Reserved[48];
	};

	DWORD nUnitDeviceEntries;
	NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA FirstUnitDevice;
	NDAS_LOGICALDEVICE_PARAMS LogicalDeviceParams;

} NDASUSER_LOGICALDEVICE_INFORMATIONA, *PNDASUSER_LOGICALDEVICE_INFORMATIONA;

#ifdef UNICODE
#define NDASUSER_LOGICALDEVICE_INFORMATION NDASUSER_LOGICALDEVICE_INFORMATIONW
#define PNDASUSER_LOGICALDEVICE_INFORMATION PNDASUSER_LOGICALDEVICE_INFORMATIONW
#else
#define NDASUSER_LOGICALDEVICE_INFORMATION NDASUSER_LOGICALDEVICE_INFORMATIONA
#define PNDASUSER_LOGICALDEVICE_INFORMATION PNDASUSER_LOGICALDEVICE_INFORMATIONA
#endif

NDASUSER_API
BOOL
WINAPI
NdasQueryLogicalDeviceInformationW(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	PNDASUSER_LOGICALDEVICE_INFORMATIONW pLogDevInfo);

NDASUSER_API
BOOL
WINAPI
NdasQueryLogicalDeviceInformationA(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	PNDASUSER_LOGICALDEVICE_INFORMATIONA pLogDevInfo);

#ifdef UNICODE
#define NdasQueryLogicalDeviceInformation NdasQueryLogicalDeviceInformationW
#else
#define NdasQueryLogicalDeviceInformation NdasQueryLogicalDeviceInformationA
#endif

typedef BOOL (CALLBACK* NDASLOGICALDEVICEMEMBERENUMPROCW)(
	PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYW lpEntry, LPVOID lpContext);

typedef BOOL (CALLBACK* NDASLOGICALDEVICEMEMBERENUMPROCA)(
	PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYA lpEntry, LPVOID lpContext);

#ifdef UNICODE
#define NDASLOGICALDEVICEMEMBERENUMPROC NDASLOGICALDEVICEMEMBERENUMPROCW
#else
#define NDASLOGICALDEVICEMEMBERENUMPROC NDASLOGICALDEVICEMEMBERENUMPROCA
#endif

NDASUSER_API
BOOL
WINAPI
NdasEnumLogicalDeviceMembersW(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	NDASLOGICALDEVICEMEMBERENUMPROCW lpEnumProc,
	LPVOID lpContext);

NDASUSER_API
BOOL
WINAPI
NdasEnumLogicalDeviceMembersA(
	NDAS_LOGICALDEVICE_ID logicalDeviceId,
	NDASLOGICALDEVICEMEMBERENUMPROCA lpEnumProc,
	LPVOID lpContext);

#ifdef UNICODE
#define NdasEnumLogicalDeviceMembers NdasEnumLogicalDeviceMembersW
#else
#define NdasEnumLogicalDeviceMembers NdasEnumLogicalDeviceMembersA
#endif

//////////////////////////////////////////////////////////////////////////
//
// Register NDAS Event Callback function
//
// RegisterEventCallback function registers a user-defined callback
// function to the NDAS events from the underlying service.
// 
// The caller thread must be in an alertable waite state for the callback
// function to be called.
//
//////////////////////////////////////////////////////////////////////////

#define MAX_NDAS_EVENT_CALLBACK 5

typedef struct _NDAS_EVENT_INFO {
	NDAS_EVENT_TYPE EventType;
	union {
		NDAS_EVENT_LOGICALDEVICE_INFO LogicalDeviceInfo;
		NDAS_EVENT_DEVICE_INFO DeviceInfo;
		NDAS_EVENT_SURRENDER_REQUEST_INFO SurrenderRequestInfo;
		NDAS_EVENT_UNITDEVICE_INFO UnitDeviceInfo;
	};
} NDAS_EVENT_INFO, *PNDAS_EVENT_INFO;

typedef VOID (CALLBACK* NDASEVENTPROC)(
	DWORD dwError, PNDAS_EVENT_INFO pEventInfo, LPVOID lpContext);

typedef void *HNDASEVENTCALLBACK;

NDASUSER_API
HNDASEVENTCALLBACK
WINAPI
NdasRegisterEventCallback(
	IN NDASEVENTPROC lpEventProc, 
	IN LPVOID lpContext);

NDASUSER_API
BOOL
WINAPI
NdasUnregisterEventCallback(
	IN HNDASEVENTCALLBACK hEventCallback);

//////////////////////////////////////////////////////////////////////////
//
// Notify Change of Unit Device DIB
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasNotifyUnitDeviceChange(
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo);

//////////////////////////////////////////////////////////////////////////
//
// NDAS Service Parameter
//
//////////////////////////////////////////////////////////////////////////

NDASUSER_API
BOOL
WINAPI
NdasSetServiceParam(CONST NDAS_SERVICE_PARAM* pParam);

NDASUSER_API
BOOL
WINAPI
NdasGetServiceParam(DWORD ParamCode, NDAS_SERVICE_PARAM* pParam);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASUSER_H_ */
