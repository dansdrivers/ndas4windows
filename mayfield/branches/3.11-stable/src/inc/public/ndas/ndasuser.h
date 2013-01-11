/*++

  NDAS USER API Header

  Copyright (C) 2002-2005 XIMETA, Inc.
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
#define NDASUSER_LINKAGE
#else
#define NDASUSER_LINKAGE __declspec(dllimport)
#endif

#else /* defined(WIN32) || defined(UNDER_CE) */
#define NDASUSER_LINKAGE
#endif

#ifndef NDASUSERAPI
#define NDASUSERAPI  __stdcall
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

#include "ndashostinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NDASUSER_API_VERSION_MAJOR 0x0001
#define NDASUSER_API_VERSION_MINOR 0x0020

/* <TITLE NdasGetAPIVersion>

Declaration

DWORD
NdasGetAPIVersion();

Summary

	NdasGetApiVersion function returns the current version information
	of the loaded library

Returns

	Low word contains the major version number and high word the minor
	version number.

--*/

NDASUSER_LINKAGE
DWORD
NDASUSERAPI
NdasGetAPIVersion();

/*

<TITLE NdasValidateStringIdKey>

Declaration

BOOL 
NdasValidateStringIdKey(
	IN LPCTSTR lpszDeviceStringId, 
	IN LPCTSTR lpszDeviceStringKey);

Summary

	NdasValidateStringIdKey function is used for validating a
	device string ID and write key.
	
Parameters

	lpszDeviceStringId :   
		[in] Pointer to a null\-terminated string specifying
		the device id to validate.
	lpszDeviceStringKey :  
		[in] Pointer to a null\-terminated string specifying
		the device (write) key to validate.<P>
		If lpszDeviceStringKey is NULL, NdasValidateStringIdKey ignores 
		this parameter and checks the validation of device ID only.
   
Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                              
*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasValidateStringIdKeyW(
	IN LPCWSTR lpszDeviceStringId, 
	IN LPCWSTR lpszDeviceStringKey);

/* <COMBINE NdasValidateStringIdKeyW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasValidateStringIdKeyA(
	IN LPCSTR lpszDeviceStringId, 
	IN LPCSTR lpszDeviceStringKey);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasValidateStringIdKey NdasValidateStringIdKeyW
#else
#define NdasValidateStringIdKey NdasValidateStringIdKeyA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasRegisterDevice>

Declaration

DWORD
NdasRegisterDevice(
	IN LPCTSTR lpszDeviceStringId,
	IN LPCTSTR lpszDeviceStringKey,
	IN LPCTSTR lpszDeviceName,
	IN DWORD dwFlags);

Summary

	The NdasRegisterDevice function registers a new NDAS device to
	the Device Registrar.

Description

	A NDAS device should be registered to the device registrar before
	it can be used. A successful registration will return a slot number 
	other than zero, which can be used for other API calls.

	Both NdasRegisterDevice and NdasRegisterDeviceEx can be used
	to register NDAS devices to the system. However, NdasRegisterDeviceEx 
	should be used when OEM code is required.

Parameters

	lpszDeviceStringId:
		[in] Pointer to a null-terminated string specifying the device id
			of the registering device which consists of 20 chars without
			dashes. String ID should be 20 characters in length and
			it should be a valid one.

	lpszDeviceStringKey:
		[in] Pointer to a null-terminated string specifying the device
			(write) key of the registering device which consists of
			5 chars.
			If lpszDeviceStringKey is NULL, a device is registered
			with read-only access.
			If lpszDeviceStringKey is non-NULL, and if it is valid,
			a device is registered with read-write access.
          
	lpszDeviceName:
		[in] Pointer to a null-terminated string specifying the name
			of a registering device with MAX_NDAS_DEVICE_NAME_LEN
			in its length excluding terminating NULL character.
			The name is supplied for the user's conveniences,
			and it is not validated for the duplicate name for
			the existing devices.

	dwFlags:
		[in] This parameter can be one of the following values.


Returns

	If the function succeeds, the return value is the registered slot number
	of the device in the Device Registrar. Valid slot number is
	starting from 1 (One). 0 (Zero) is not used and indicates an invalid
	slot number.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.
*/

NDASUSER_LINKAGE
DWORD
NDASUSERAPI
NdasRegisterDeviceW(
	IN LPCWSTR lpszDeviceStringId,
	IN LPCWSTR lpszDeviceStringKey,
	IN LPCWSTR lpszDeviceName,
	IN DWORD dwFlags);

/* <COMBINE NdasRegisterDeviceW> */

NDASUSER_LINKAGE
DWORD
NDASUSERAPI
NdasRegisterDeviceA(
	IN LPCSTR lpszDeviceStringId,
	IN LPCSTR lpszDeviceStringKey,
	IN LPCSTR lpszDeviceName,
	IN DWORD dwFlags);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasRegisterDevice NdasRegisterDeviceW
#else
#define NdasRegisterDevice NdasRegisterDeviceA
#endif
/*DOM-IGNORE-END*/

/* 
<TITLE NdasRegisterDeviceEx>

Declaration

DWORD
NDASUSERAPI
NdasRegisterDeviceEx(
	IN CONST NDAS_DEVICE_REGISTRATION* Registration);

Summary

	The NdasRegisterDeviceEx function registers a new NDAS device to
	the Device Registrar.

Description

	A NDAS device should be registered to the device registrar before
	it can be used. A successful registration will return a slot number 
	other than zero, which can be used for other API calls.
	
	Both NdasRegisterDevice and NdasRegisterDeviceEx can be used
	to register NDAS devices to the system. However, NdasRegisterDeviceEx 
	should be used when OEM code is required.

Parameters

	Registration:
		[in] Pointer to a structure containing Registration information.

Returns

	If the function succeeds, the return value is the registered slot number
	of the device in the Device Registrar. Valid slot number is
	starting from 1 (One). 0 (Zero) is not used and indicates an invalid
	slot number.

	If the function fails, the return value is zero. To get extended error 
	information, call GetLastError.
*/

NDASUSER_LINKAGE
DWORD
NDASUSERAPI
NdasRegisterDeviceExW(
	IN CONST NDAS_DEVICE_REGISTRATIONW* Registration);

/* <COMBINE NdasRegisterDeviceExW> */

NDASUSER_LINKAGE
DWORD
NDASUSERAPI
NdasRegisterDeviceExA(
	IN CONST NDAS_DEVICE_REGISTRATIONA* Registration);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasRegisterDeviceEx NdasRegisterDeviceExW
#else
#define NdasRegisterDeviceEx NdasRegisterDeviceExA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasGetRegistrationData>

Declaration

BOOL
NdasGetRegistrationData(
	IN  DWORD dwSlotNo,
	OUT LPVOID lpBuffer,
	IN  DWORD cbBuffer,
	OUT LPDWORD cbBufferUsed);

BOOL
NdasGetRegistrationDataById(
	 IN  LPCTSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN  DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

Summary
	
	Get Registration Data

Description
	
	CURRENTLY NOT IMPLEMENTED

Parameters
	
	lpRegistrationData:
	[in] Vendor-specific registration information to be 
		stored in the device registrar

	cbRegistrationData:
	[in] Size of lpRegistrationData in bytes.
		Maximum allowed size is defined as MAX_NDAS_REGISTRATION_DATA

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                             
*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetRegistrationData(
	 IN	DWORD dwSlotNo,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

/* <COMBINE NdasGetRegistrationData> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

/* <COMBINE NdasGetRegistrationData> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 OUT LPVOID lpBuffer,
	 IN DWORD cbBuffer,
	 OUT LPDWORD cbBufferUsed);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasGetRegistrationDataById NdasGetRegistrationDataByIdW
#else
#define NdasGetRegistrationDataById NdasGetRegistrationDataByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasSetRegistrationData>

Declaration

BOOL
NdasSetRegistrationData(
	 IN	DWORD dwSlotNo,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

BOOL
NdasSetRegistrationDataById(
	 IN LPCTSTR lpszDeviceStringId,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

Summary

	Set vendor-specific registration data of the NDAS device in the system.

Description

	Registration data is valid to the current system only.
	It persists between system boots, however, this data is not
	available to other NDAS hosts.

	CURRENTLY NOT IMPLEMENTED

Parameters

	dwSlotNo:
		[in] Slot number of the NDAS device in which the registration data will be set.

	lpBuffer:
		[in] Vendor-specific registration information to be 
			stored in the device registrar.

	cbBuffer:
		[in] Size of lpBuffer in bytes.
			Maximum allowed size is defined as MAX_NDAS_REGISTRATION_DATA

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                             
*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationData(
	 IN	DWORD dwSlotNo,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

/* <COMBINE NdasSetRegistrationData> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationDataByIdW(
	 IN LPCWSTR lpszDeviceStringId,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

/* <COMBINE NdasSetRegistrationData> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetRegistrationDataByIdA(
	 IN LPCSTR lpszDeviceStringId,
	 IN LPCVOID lpBuffer,
	 IN DWORD cbBuffer);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasSetRegistrationDataById NdasSetRegistrationDataByIdW
#else
#define NdasSetRegistrationDataById NdasSetRegistrationDataByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NDASUSER_DEVICE_ENUM_ENTRY>

Summary

	NDASUSER_DEVICE_ENUM_ENTRY structure contains basic data of 
	the NDAS device	which is enumerated.

*/

typedef struct _NDASUSER_DEVICE_ENUM_ENTRYW {
	WCHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	/* NDAS device string ID */
	WCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	/* NDAS device name */
	ACCESS_MASK GrantedAccess;
	/* Granted (possible) access to the NDAS device.
	Combinations of GENERIC_READ and GENERIC_WRITE are used at this time.*/
	DWORD SlotNo;
	/* Slot number of the NDAS device */
} NDASUSER_DEVICE_ENUM_ENTRYW, *PNDASUSER_DEVICE_ENUM_ENTRYW;

/* <COMBINE NDASUSER_DEVICE_ENUM_ENTRYW> */

typedef struct _NDASUSER_DEVICE_ENUM_ENTRYA {
	CHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	CHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	DWORD SlotNo;
} NDASUSER_DEVICE_ENUM_ENTRYA, *PNDASUSER_DEVICE_ENUM_ENTRYA;

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASUSER_DEVICE_ENUM_ENTRY NDASUSER_DEVICE_ENUM_ENTRYW
#define PNDASUSER_DEVICE_ENUM_ENTRY PNDASUSER_DEVICE_ENUM_ENTRYW
#else
#define NDASUSER_DEVICE_ENUM_ENTRY NDASUSER_DEVICE_ENUM_ENTRYA
#define PNDASUSER_DEVICE_ENUM_ENTRY PNDASUSER_DEVICE_ENUM_ENTRYA
#endif
/*DOM-IGNORE-END*/

/* @@NdasDeviceEnumProc

<TITLE NdasDeviceEnumProc>

Declaration

BOOL
NdasDeviceEnumProc(
	PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry,
	LPVOID lpContext);

Summary

	The NdasDeviceEnumProc function is an application-defined callback function
	used with the NdasEnumDevices function.
	It receives pointers to the NDASUSER_DEVICE_ENUM_ENTRY struct for
	each registered NDAS devices. NDASDEVICEENUMPROC type defines a pointer
	to this callback function. NdasDeviceEnumProc is a placeholder for the
	application-defined function name.

Parameters

	lpEnumEntry:
		[in] Pointer to an NDAS device enumerate entry (NDASUSER_DEVICE_ENUM_ENTRY)

	lpContext:
		[in] Application-defined value specified in the NdasEnumDevices function.

Returns

	To continue enumeration, the callback function must return TRUE; 
	to stop enumeration, it must return FALSE. 

	Description

	An application must register this callback function by passing its address 
	to NdasEnumDevices. 

*/

/* <COMBINE NdasDeviceEnumProc> */
typedef BOOL (CALLBACK* NDASDEVICEENUMPROCW)(
	PNDASUSER_DEVICE_ENUM_ENTRYW lpEnumEntry, LPVOID lpContext);

/* <COMBINE NdasDeviceEnumProc> */
typedef BOOL (CALLBACK* NDASDEVICEENUMPROCA)(
	PNDASUSER_DEVICE_ENUM_ENTRYA lpEnumEntry, LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASDEVICEENUMPROC NDASDEVICEENUMPROCW
#else
#define NDASDEVICEENUMPROC NDASDEVICEENUMPROCA
#endif
/*DOM-IGNORE-END*/

/* 

<TITLE NdasEnumDevices>

Declaration

BOOL 
NdasEnumDevices(
	NDASDEVICEENUMPROC lpEnumFunc, 
	LPVOID lpContext);

Summary

	NdasEnumDevices function enumerates all registered devices by passing
	a pointer to NDASUSER_DEVICE_ENUM_ENTRY struct for each registered 
	NDAS devices, in turn, to an application-defined callback function. 
	NdasEnumDevices continues until the last NDAS device is enumerated or 
	the callback function returns FALSE.

Parameters

	lpFnumFunc:
		[in] Pointer to an application-defined EnumNdasDeviceProc callback function
	lpContext:
		[in] Application-defined value to be passed to the callback function

Returns
	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                             
*/

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasEnumDevicesW(NDASDEVICEENUMPROCW lpEnumFunc, LPVOID lpContext);

/* <COMBINE NdasEnumDevicesW> */

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasEnumDevicesA(NDASDEVICEENUMPROCA lpEnumFunc, LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasEnumDevices NdasEnumDevicesW
#else
#define NdasEnumDevices NdasEnumDevicesA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasEnableDevice>

Declaration

BOOL 
NdasEnableDevice(
	IN DWORD dwSlotNo, 
	IN BOOL bEnable);

BOOL 
NdasEnableDeviceById(
	IN LPCTSTR lpszDeviceStringId, 
	IN BOOL bEnable);

Summary

	Enable or disable a NDAS device. 

Description

	When the NDAS device is disabled, the NDAS host does not communicate
	with it and any associated NDAS unit devices or logical devices are 
	invalidated and removed.

	Also, a NDAS device cannot be disabled if any associated NDAS logical devices 
	are mounted.

	"Enable" and "Disable" are also known as "Activate" and "Deactivate" 
	respectively.

Parameters

	dwSlotNo:
		[in] Slot number of the NDAS device.

	bEnable:
		[in] TRUE to enable, FALSE to disable this device

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                             

*/
NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnableDevice(
	IN DWORD dwSlotNo, 
	IN BOOL bEnable);

/* <COMBINE NdasEnableDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnableDeviceByIdW(
	IN LPCWSTR lpszDeviceStringId, 
	IN BOOL bEnable);

/* <COMBINE NdasEnableDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnableDeviceByIdA(
	IN LPCSTR lpszDeviceStringId, 
	IN BOOL bEnable);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasEnableDeviceById NdasEnableDeviceByIdW
#else
#define NdasEnableDeviceById NdasEnableDeviceByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasUnregisterDevice>

Declaration

BOOL
NdasUnregisterDevice(
	DWORD dwSlotNo);

Summary

	Unregister a NDAS device from the device registrar.

Description

	To unregister a NDAS device, it should be disabled first.
	Otherwise, it returns FALSE and the GetLastError will return
	NDASSVC_ERROR_CANNOT_UNREGISTER_ENABLED_DEVICE.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device.

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.

*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasUnregisterDevice(DWORD dwSlotNo);

/* <COMBINE NdasUnregisterDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasUnregisterDeviceByIdW(LPCWSTR lpszDeviceStringId);

/* <COMBINE NdasUnregisterDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasUnregisterDeviceByIdA(LPCSTR lpszDeviceStringId);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasUnregisterDeviceById NdasUnregisterDeviceByIdW
#else
#define NdasUnregisterDeviceById NdasUnregisterDeviceByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryDeviceStatus>

Declaration

BOOL 
NdasQueryDeviceStatus(
	IN  DWORD dwSlotNo, 
	OUT NDAS_DEVICE_STATUS* pStatus,
	OUT NDAS_DEVICE_ERROR* pLastError);

BOOL 
NdasQueryDeviceStatusById(
	IN  LPCTSTR lpszDeviceStringId, 
	OUT NDAS_DEVICE_STATUS* pStatus,
	OUT NDAS_DEVICE_ERROR* pLastError);

Summary

	NdasQueryDeviceStatus return the status and the last error of the
	NDAS Device.

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.

*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceStatus(
	IN DWORD dwSlotNo, 
	OUT NDAS_DEVICE_STATUS* pStatus,
	OUT NDAS_DEVICE_ERROR* pLastError);

/* <COMBINE NdasQueryDeviceStatus> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceStatusByIdW(
	IN LPCWSTR lpszDeviceStringId, 
	OUT NDAS_DEVICE_STATUS* pStatus,
	OUT NDAS_DEVICE_ERROR* pLastError);

/* <COMBINE NdasQueryDeviceStatus> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceStatusByIdA(
	IN LPCSTR lpszDeviceStringId, 
	OUT NDAS_DEVICE_STATUS* pStatus,
	OUT NDAS_DEVICE_ERROR* pLastError);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryDeviceStatusById NdasQueryDeviceStatusByIdW
#else
#define NdasQueryDeviceStatusById NdasQueryDeviceStatusByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NDASUSER_DEVICE_INFORMATION>
*/

typedef struct _NDASUSER_DEVICE_INFORMATIONA {
	DWORD SlotNo;
	WCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
	CHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	NDAS_DEVICE_HARDWARE_INFO HardwareInfo;
	NDAS_DEVICE_PARAMS DeviceParams;
} NDASUSER_DEVICE_INFORMATIONA, *PNDASUSER_DEVICE_INFORMATIONA;

/* <COMBINE NDASUSER_DEVICE_INFORMATIONA> */

typedef struct _NDASUSER_DEVICE_INFORMATIONW {
	DWORD SlotNo;
	WCHAR szDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
	WCHAR szDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	ACCESS_MASK GrantedAccess;
	NDAS_DEVICE_HARDWARE_INFO HardwareInfo;
	NDAS_DEVICE_PARAMS DeviceParams;
} NDASUSER_DEVICE_INFORMATIONW, *PNDASUSER_DEVICE_INFORMATIONW;

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASUSER_DEVICE_INFORMATION NDASUSER_DEVICE_INFORMATIONW
#define PNDASUSER_DEVICE_INFORMATION PNDASUSER_DEVICE_INFORMATIONW
#else
#define NDASUSER_DEVICE_INFORMATION NDASUSER_DEVICE_INFORMATIONA
#define PNDASUSER_DEVICE_INFORMATION PNDASUSER_DEVICE_INFORMATIONA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryDeviceInformation> 

Declaration

BOOL 
NdasQueryDeviceInformation(
	IN  DWORD dwSlotNo, 
	OUT NDASUSER_DEVICE_INFORMATION* pDeviceInfo);

BOOL 
NdasQueryDeviceInformationById(
	IN  LPCTSTR lpszDeviceStringId, 
	OUT NDASUSER_DEVICE_INFORMATION* pDeviceInfo);

Summary

	NdasQueryDeviceInformation returns the information of the NDAS device
	specified by the slot number of device string ID.

*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceInformationW(
	IN DWORD dwSlotNo, 
	OUT NDASUSER_DEVICE_INFORMATIONW* pDeviceInfo);

/* <COMBINE NdasQueryDeviceInformationW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceInformationA(
	IN DWORD dwSlotNo, 
	OUT NDASUSER_DEVICE_INFORMATIONA* pDeviceInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryDeviceInformation NdasQueryDeviceInformationW
#else
#define NdasQueryDeviceInformation NdasQueryDeviceInformationA
#endif
/*DOM-IGNORE-END*/

/* <COMBINE NdasQueryDeviceInformationW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceInformationByIdW(
	IN LPCWSTR lpszDeviceStringId, 
	OUT NDASUSER_DEVICE_INFORMATIONW* pDeviceInfo);

/* <COMBINE NdasQueryDeviceInformationW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryDeviceInformationByIdA(
	IN LPCSTR lpszDeviceStringId, 
	OUT NDASUSER_DEVICE_INFORMATIONA* pDeviceInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryDeviceInformationById NdasQueryDeviceInformationByIdW
#else
#define NdasQueryDeviceInformationById NdasQueryDeviceInformationByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasSetDeviceName> 

Declaration

BOOL
NdasSetDeviceName(
	IN DWORD dwSlotNo, 
	IN LPCTSTR lpszDeviceName);

BOOL 
NdasSetDeviceNameById(
	IN LPCTSTR lpszDeviceStringId, 
	IN LPCTSTR lpszDeviceName);

Summary

	NdasSetDeviceName sets the name of the NDAS device specified
	by the slot number or the device string ID.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device

	lpszDeviceName:
	[in] Pointer to a null-terminated string of maximum length MAX_NDAS_DEVICE_NAME_LEN 
	that contains the name of the NDAS device

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.
*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameW(
	IN DWORD dwSlotNo, 
	IN LPCWSTR lpszDeviceName);

/* <COMBINE NdasSetDeviceNameW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameA(
	IN DWORD dwSlotNo, 
	IN LPCSTR lpszDeviceName);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasSetDeviceName NdasSetDeviceNameW
#else
#define NdasSetDeviceName NdasSetDeviceNameA
#endif
/*DOM-IGNORE-END*/

/* <COMBINE NdasSetDeviceNameW>  */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameByIdW(
	IN LPCWSTR lpszDeviceStringId, 
	IN LPCWSTR lpszDeviceName);

/* <COMBINE NdasSetDeviceNameW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceNameByIdA(
	IN LPCSTR lpszDeviceStringId, 
	IN LPCSTR lpszDeviceName);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasSetDeviceNameById NdasSetDeviceNameByIdW
#else
#define NdasSetDeviceNameById NdasSetDeviceNameByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasSetDeviceAccessById> 

Declaration

BOOL 
NdasSetDeviceAccessById(
	IN LPCTSTR lpszDeviceStringId, 
	IN BOOL bWriteAccess, 
	IN LPCTSTR lpszDeviceStringKey);

Summary

	NdasSetDeviceAccessById function modifies the granted access
	of the NDAS device specified by the device string ID.
	If bWriteAccess is set as TRUE, the valid device string key of 
	the NDAS device should be provided. Otherwise, device string key
	is not used.

Parameters

	lpszDeviceStringId:
	[in] Device String ID of the NDAS device

	lpszDeviceStringKey:
	[in] Device String Key of the NDAS device (aka Write Key)

	bWriteAccess:
	[in] Set as TRUE to grant write access, otherwise set as FALSE.

Description

	Unlike other functions, there does not exist a function 
	NdasSetDeviceAccess which may use a slot number as the parameter
	instead of the device string ID.

*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceAccessByIdW(
	IN LPCWSTR lpszDeviceStringId, 
	IN BOOL bWriteAccess, 
	IN LPCWSTR lpszDeviceStringKey);

/* <COMBINE NdasSetDeviceAccessByIdW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasSetDeviceAccessByIdA(
	IN LPCSTR lpszDeviceStringId, 
	IN BOOL bWriteAccess, 
	IN LPCSTR lpszDeviceStringKey);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasSetDeviceAccessById NdasSetDeviceAccessByIdW
#else
#define NdasSetDeviceAccessById NdasSetDeviceAccessByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NDASUSER_UNITDEVICE_ENUM_ENTRY>
*/

typedef struct _NDASUSER_UNITDEVICE_ENUM_ENTRY {
	DWORD UnitNo;
	NDAS_UNITDEVICE_TYPE UnitDeviceType;
} NDASUSER_UNITDEVICE_ENUM_ENTRY, *PNDASUSER_UNITDEVICE_ENUM_ENTRY;

/* @@NdasUnitDeviceEnumProc
<TITLE NdasUnitDeviceEnumProc>

Declaration

BOOL
NdasUnitDeviceEnumProc(
	PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, 
	LPVOID lpContext);

*/

/* <COMBINE NdasUnitDeviceEnumProc> */

typedef BOOL (CALLBACK* NDASUNITDEVICEENUMPROC)(
	PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, LPVOID lpContext);

/* <TITLE NdasEnumUnitDevices>

Declaration

BOOL 
NdasEnumUnitDevices(
	DWORD dwSlotNo, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

BOOL 
NdasEnumUnitDevicesById(
	LPCTSTR lpszDeviceStringId, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

Summary

	NdasEnumUnitDevices enumerates unit devices of the NDAS device specified
	by the slot number or the device string ID by calling
	an application-defined callback function.

*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnumUnitDevices(
	DWORD dwSlotNo, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

/* <COMBINE NdasEnumUnitDevices>
*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnumUnitDevicesByIdW(
	LPCWSTR lpszDeviceStringId, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

/* <COMBINE NdasEnumUnitDevices> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasEnumUnitDevicesByIdA(
	LPCSTR lpszDeviceStringId, 
	NDASUNITDEVICEENUMPROC lpEnumProc, 
	LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasEnumUnitDevicesById NdasEnumUnitDevicesByIdW
#else
#define NdasEnumUnitDevicesById NdasEnumUnitDevicesByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryUnitDeviceStatus>

Declaration

BOOL 
NdasQueryUnitDeviceStatus(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

BOOL 
NdasQueryUnitDeviceStatusById(
	LPCTSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

Summary

	NdasQueryUnitDeviceStatus function queries the status and the last
	error code of the NDAS unit device specified by the slot number 
	(or the device string ID) and the unit number.

*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceStatus(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

/* <COMBINE NdasQueryUnitDeviceStatus> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceStatusByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

/* <COMBINE NdasQueryUnitDeviceStatus> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceStatusByIdA(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	NDAS_UNITDEVICE_STATUS* pStatus,
	NDAS_UNITDEVICE_ERROR* pLastError);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryUnitDeviceStatusById NdasQueryUnitDeviceStatusByIdW
#else
#define NdasQueryUnitDeviceStatusById NdasQueryUnitDeviceStatusByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NDASUSER_UNITDEVICE_INFORMATION>
*/

typedef struct _NDASUSER_UNITDEVICE_INFORMATIONW {
	NDAS_UNITDEVICE_TYPE UnitDeviceType;
	NDAS_UNITDEVICE_SUBTYPE UnitDeviceSubType;
	NDAS_UNITDEVICE_HARDWARE_INFOW HardwareInfo;
	NDAS_UNITDEVICE_PARAMS UnitDeviceParams;
} NDASUSER_UNITDEVICE_INFORMATIONW, *PNDASUSER_UNITDEVICE_INFORMATIONW;

/* <COMBINE NDASUSER_UNITDEVICE_INFORMATIONW> */

typedef struct _NDASUSER_UNITDEVICE_INFORMATIONA {
	NDAS_UNITDEVICE_TYPE UnitDeviceType;
	NDAS_UNITDEVICE_SUBTYPE UnitDeviceSubType;
	NDAS_UNITDEVICE_HARDWARE_INFOA HardwareInfo;
	NDAS_UNITDEVICE_PARAMS UnitDeviceParams;
} NDASUSER_UNITDEVICE_INFORMATIONA, *PNDASUSER_UNITDEVICE_INFORMATIONA;

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASUSER_UNITDEVICE_INFORMATION NDASUSER_UNITDEVICE_INFORMATIONW
#else
#define NDASUSER_UNITDEVICE_INFORMATION NDASUSER_UNITDEVICE_INFORMATIONA
#endif
/*DOM-IGNORE-END*/

/*
<TITLE NdasQueryUnitDeviceInformation>

Declaration

BOOL
NdasQueryUnitDeviceInformation(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATION pDevInfo);

BOOL
NdasQueryUnitDeviceInformationById(
	LPCTSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATION pDevInfo);

Summary

	NdasQueryUnitDeviceInformation queries the information of
	the unit device given by the slot number and the unit device number.

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.
*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationW(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo);

/* <COMBINE NdasQueryUnitDeviceInformationW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationA(
	DWORD dwSlotNo, DWORD dwUnitNo,
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryUnitDeviceInformation NdasQueryUnitDeviceInformationW
#else
#define NdasQueryUnitDeviceInformation NdasQueryUnitDeviceInformationA
#endif
/*DOM-IGNORE-END*/

/* <COMBINE NdasQueryUnitDeviceInformationW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONW pDevInfo);

/* <COMBINE NdasQueryUnitDeviceInformationW> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasQueryUnitDeviceInformationByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo, 
	PNDASUSER_UNITDEVICE_INFORMATIONA pDevInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryUnitDeviceInformationById NdasQueryUnitDeviceInformationByIdW
#else
#define NdasQueryUnitDeviceInformationById NdasQueryUnitDeviceInformationByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryUnitDeviceHostStats>

Declaration

BOOL
NdasQueryUnitDeviceHostStats(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts);

BOOL
NdasQueryUnitDeviceHostStatsById(
	LPCTSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	LPDWORD lpnROHosts, 
	LPDWORD lpnRWHosts);

Remarks:

	This function is deprecated.
	Use NdasQueryDeviceStats or NdasQueryUnitDeviceStats instead.

Summary

	Query the host statistics of the NDAS unit device.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device

	dwUnitNo:
	[in] Unit number of the NDAS unit device of the NDAS device
	specified by dwSlotNo

	lpnROHosts:
	[out] Pointer to the variable where the number of hosts using the 
	NDAS unit device as Read-only access.
	
	lpnRWHosts:
	[out] Pointer to the variable where the number of hosts using the 
	NDAS unit device as Read/Write access.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceHostStats(
	DWORD dwSlotNo, DWORD dwUnitNo, 
	LPDWORD lpnROHosts, LPDWORD lpnRWHosts);

/* <COMBINE NdasQueryUnitDeviceHostStats> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceHostStatsByIdW(
	LPCWSTR lpszDeviceStringId, DWORD dwUnitNo, 
	LPDWORD lpnROHosts, LPDWORD lpnRWHosts);

/* <COMBINE NdasQueryUnitDeviceHostStats> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceHostStatsByIdA(
	LPCSTR lpszDeviceStringId, DWORD dwUnitNo, 
	LPDWORD lpnROHosts, LPDWORD lpnRWHosts);

/* <TITLE NdasQueryDeviceStats>

Declaration

BOOL
NdasQueryDeviceStats(
	DWORD dwSlotNo,
	PNDAS_DEVICE_STAT pDeviceStats);

BOOL
NdasQueryDeviceStatsById(
	LPCTSTR lpszNdasId,
	PNDAS_DEVICE_STAT pDeviceStats);

Summary

	Query the statistics of the NDAS device.
	This contains statistics for all unit devices.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device

	pDeviceStats:
	[in,out] A pointer to NDAS_DEVICE_STAT structure,
	         containing device statistics.
			 Size field should be set as sizeof(NDAS_DEVICE_STAT)
			 when calling the function.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStats(
	IN DWORD dwSlotNo,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats);

/* <COMBINE NdasQueryDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStatsByIdW(
	IN LPCWSTR lpszNdasId,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats);

/* <COMBINE NdasQueryDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStatsByIdA(
	IN LPCSTR lpszNdasId,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryDeviceStatsById NdasQueryDeviceStatsByIdW
#else
#define NdasQueryDeviceStatsById NdasQueryDeviceStatsByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryDeviceStats>

Declaration

BOOL
NdasQueryUnitDeviceStats(
	DWORD dwSlotNo, DWORD dwUnitNo,
	PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

BOOL
NdasQueryUnitDeviceStatsById(
	LPCTSTR lpszNdasId, DWORD dwUnitNo,
	PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

Summary

	Query statistics of the NDAS unit device.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device of the unit device

	dwUnitNo:
	[in] Unit number of the NDAS unit device

	pUnitDeviceStats:
	[in,out] A pointer to NDAS_UNITDEVICE_STAT structure,
	         containing device statistics.
			 Size field should be set as sizeof(NDAS_UNITDEVICE_STAT)
			 when calling the function.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStats(
	IN DWORD dwSlotNo, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

/* <COMBINE NdasQueryUnitDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStatsByIdW(
	IN LPCWSTR lpszNdasId, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

/* <COMBINE NdasQueryUnitDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStatsByIdA(
	IN LPCSTR lpszNdasId, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryUnitDeviceStatsById NdasQueryUnitDeviceStatsByIdW
#else
#define NdasQueryUnitDeviceStatsById NdasQueryUnitDeviceStatsByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasFindLogicalDeviceOfUnitDevice>

Declaration

BOOL 
NdasFindLogicalDeviceOfUnitDevice(
	IN  DWORD dwSlotNo, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

BOOL 
NdasFindLogicalDeviceOfUnitDeviceById(
	IN  LPCTSTR lpszDeviceStringId, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

Summary

	Find the NDAS logical device associated with the NDAS unit device.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device

	dwUnitNo:
	[in] Unit number of the NDAS unit device of the NDAS device
	specified by dwSlotNo

	pLogicalDeviceId:
	[out] Pointer to a NDAS_LOGICALDEVICE_ID that receives the 
	logical device ID. This can be used in subsequent calls to 
	functions related to NDAS logical devices.

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.
	
*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDevice(
	IN  DWORD dwSlotNo, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/* <COMBINE NdasFindLogicalDeviceOfUnitDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdW(
	IN  LPCWSTR lpszDeviceStringId, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/* <COMBINE NdasFindLogicalDeviceOfUnitDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdA(
	IN  LPCSTR lpszDeviceStringId, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasFindLogicalDeviceOfUnitDeviceById NdasFindLogicalDeviceOfUnitDeviceByIdW
#else
#define NdasFindLogicalDeviceOfUnitDeviceById NdasFindLogicalDeviceOfUnitDeviceByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasPlugInLogicalDevice>

Declaration:

BOOL 
NdasPlugInLogicalDevice(
	IN BOOL bWritable, 
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

Summary:

	Plug in a NDAS logical device to the system.

Parameters:

	logicalDeviceId:
	[in] Logical Device ID of the NDAS logical device to be plugged in.

	bWritable:
	[in] Set as TRUE to enable write access, otherwise set as FALSE.

Returns:

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.

*/
NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasPlugInLogicalDevice(
	IN BOOL bWritable, 
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

/* <TITLE NdasPlugInLogicalDeviceEx>

Declaration

BOOL 
NdasPlugInLogicalDeviceEx(
	IN CONST NDAS_LOGICALDEVICE_PLUGIN_PARAM* Param);

Summary

	Plug in a NDAS logical device to the system.
	NdasPlugInLogicalDeviceEx provides more options
	over NdasPlugInLogicalDevice. 
	See NDAS_LOGICALDEVICE_PLUGIN_PARAM	for more information.

Parameters

	Param:
	[in] Pointer to a NDAS_LOGICALDEVICE_PLUGIN_PARAM structure.

*/

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasPlugInLogicalDeviceEx(
	IN CONST NDAS_LOGICALDEVICE_PLUGIN_PARAM* Param);

/* <TITLE NdasEjectLogicalDevice>

Declaration

BOOL
NdasEjectLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

Summary   

	Eject a NDAS logical device from the system.

Description
	
	NdasEjectLogicalDevice queues a request to eject a NDAS logical device
	and returns immediately. If the request is successfully accepted,
	the status of the logical device will be changed to 
	NDAS_LOGICAL_DEVICE_STATUS_UNMOUNT_PENDING.

	On successful ejection of the device, this status will be changed
	to NDAS_LOGICAL_DEVICE_STATUS_UNMOUNTED.

	On failure, the status will be changed to NDAS_LOGICAL_DEVICE_STATUS_MOUNTED.
	Failure is expected if any application is using an associated device 
	(e.g. Disk Volume).

Parameters
	logicalDeviceId:
		[in] NDAS logical device ID

Returns
	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                             

	This function returns the status of the acceptance of the eject request,
	which mean it returns immediately not waiting for the completion
	of the ejection. 
*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEjectLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

/* <TITLE NdasUnplugLogicalDevice>

Declaration

BOOL
NdasUnplugLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

Summary

	Unplug a NDAS logical device from the system.

Description

	NdasUnplugLogicalDevice queues a request to unplug a NDAS logical device
	and returns immediately. Unlike NdasEjectLogicalDevice, this function
	forcibly remove the device from the system and the data in the cache
	or uncommitted file system data may not be flushed, which may result
	to the loss of the data. Hence, this function should be used
	carefully as a last resort to removing the logical device from the system.

Parameters
	logicalDeviceId:
		[in] NDAS logical device ID

Returns
	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                             
*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasUnplugLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

/* <TITLE NDASUSER_LOGICALDEVICE_ENUM_ENTRY> */

typedef struct _NDASUSER_LOGICALDEVICE_ENUM_ENTRY {
	NDAS_LOGICALDEVICE_ID LogicalDeviceId;
	/* Logical Device ID */
	NDAS_LOGICALDEVICE_TYPE Type;
	/* Logical Device Type */
} NDASUSER_LOGICALDEVICE_ENUM_ENTRY, *PNDASUSER_LOGICALDEVICE_ENUM_ENTRY;

/*@@NdasLogicalDeviceEnumProc
  <TITLE NdasLogicalDeviceEnumProc>
  
Declaration

BOOL
NdasLogicalDeviceEnumProc(
	PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry,
	LPVOID lpContext);

Summary

  NdasLogicalDeviceEnumProc is an application-defined callback
  function                                                    

*/

/* <COMBINE NdasLogicalDeviceEnumProc> */

typedef BOOL (CALLBACK* NDASLOGICALDEVICEENUMPROC)(
	PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry, 
	LPVOID lpContext);

/* <TITLE NdasEnumLogicalDevices>

Declaration

BOOL 
NdasEnumLogicalDevices(
	IN NDASLOGICALDEVICEENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

Summary

	NdasEnumLogicalDevices enumerates all NDAS logical device instances
	in the system by calling application-defined callback functions.

*/

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasEnumLogicalDevices(
	IN NDASLOGICALDEVICEENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

/* <TITLE NdasQueryLogicalDeviceStatus>

Declaration

BOOL
NdasQueryLogicalDeviceStatus(
	IN  NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT NDAS_LOGICALDEVICE_STATUS* pStatus,
	OUT NDAS_LOGICALDEVICE_ERROR* pLastError);

Summary

	NdasQueryLogicalDeviceStatus queries the status of the NDAS logical
	device specified by the NDAS logical device ID.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryLogicalDeviceStatus(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT NDAS_LOGICALDEVICE_STATUS* pStatus,
	OUT NDAS_LOGICALDEVICE_ERROR* pLastError);


/* @@NdasQueryHostEnumProc

<TITLE NdasQueryHostEnumProc>

Declaration

BOOL
NdasQueryHostEnumProc(
	LPCGUID lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext);

Summary

	lpHostInfo members and their data pointed by members are valid 
	only for the scope of the enumerator function.
	If any data is need persistent should be copied to another buffer

*/

/* <COMBINE NdasQueryHostEnumProc> */

typedef BOOL (CALLBACK* NDASQUERYHOSTENUMPROC)(
	LPCGUID lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext);

/* <TITLE NdasQueryHostsForLogicalDevice>

Declaration

BOOL
NdasQueryHostsForLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId, 
	IN NDASQUERYHOSTENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

Summary

	NdasQueryHostsForLogicalDevice searches NDAS hosts which
	is using the logical device.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId, 
	IN NDASQUERYHOSTENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

/* <TITLE NdasQueryHostsForUnitDevice>

Declaration

BOOL
NdasQueryHostsForUnitDevice(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

BOOL
NdasQueryHostsForUnitDeviceById(
	LPCTSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

Summary

	NdasQueryHostInfo queries the information of the NDAS host
	specified by the NDAS host GUID.

	NDAS host GUIDs can be obtained by calling NdasQueryHostsForUnitDevice.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDevice(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

/* <COMBINE NdasQueryHostsForUnitDevice> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDeviceByIdW(
	LPCWSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

/* <COMBINE NdasQueryHostsForUnitDevice> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDeviceByIdA(
	LPCSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryHostsForUnitDeviceById NdasQueryHostsForUnitDeviceByIdW
#else
#define NdasQueryHostsForUnitDeviceById NdasQueryHostsForUnitDeviceByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryHostInfo>

Declaration

BOOL
NdasQueryHostInfo(
	IN LPCGUID lpHostGuid, 
	OUT NDAS_HOST_INFO* pHostInfo);

Summary

	NdasQueryHostInfo queries the information of the NDAS host
	specified by the NDAS host GUID.

	NDAS host GUIDs can be obtained by calling NdasQueryHostsForUnitDevice.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoW(
	IN LPCGUID lpHostGuid, 
	OUT NDAS_HOST_INFOW* pHostInfo);

/* <COMBINE NdasQueryHostInfoW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoA(
	IN LPCGUID lpHostGuid, 
	OUT NDAS_HOST_INFOA* pHostInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryHostInfo NdasQueryHostInfoW
#else
#define NdasQueryHostInfo NdasQueryHostInfoA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasRequestSurrenderAccess>

Declaration

BOOL
NdasRequestSurrenderAccess(
	IN LPCGUID lpHostGuid,
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access);

BOOL
NdasRequestSurrenderAccessById(
	IN LPCGUID lpHostGuid,
	IN LPCTSTR lpszDeviceStringId,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access);

Summary

	NdasRequestSurrenderAccess function sends a request to surrender 
	an access to the specified NDAS unit device to the NDAS host,
	specified by the host GUID.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasRequestSurrenderAccess(
	IN LPCGUID lpHostGuid,
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access);

/* <COMBINE NdasRequestSurrenderAccess> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasRequestSurrenderAccessByIdW(
	IN LPCGUID lpHostGuid,
	IN LPCWSTR lpszDeviceStringId,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access);

/* <COMBINE NdasRequestSurrenderAccess> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasRequestSurrenderAccessByIdA(
	IN LPCGUID lpHostGuid,
	IN LPCSTR lpszDeviceStringId,
	IN DWORD dwUnitNo,
	IN ACCESS_MASK access);

/* <TITLE NDASUSER_LOGICALDEVICE_MEMBER_ENTRY> */

typedef struct _NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW {
	DWORD Index;
	WCHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	DWORD UnitNo;
	DWORD Blocks;
} NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW, *PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYW;

/* <COMBINE NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW> */

typedef struct _NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA {
	DWORD Index;
	CHAR szDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	DWORD UnitNo;
	DWORD Blocks;
} NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA, *PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYA;

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASUSER_LOGICALDEVICE_MEMBER_ENTRY NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW
#define PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYW
#else
#define NDASUSER_LOGICALDEVICE_MEMBER_ENTRY NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA
#define PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYA
#endif
/*DOM-IGNORE-END*/

/*DOM-IGNORE-BEGIN*/
#define	NDAS_CONTENT_ENCRYPT_TYPE_NONE	 0
#define	NDAS_CONTENT_ENCRYPT_TYPE_SIMPLE 1
#define	NDAS_CONTENT_ENCRYPT_TYPE_AES    2
/*DOM-IGNORE-END*/

/* <TITLE NDASUSER_LOGICALDEVICE_INFORMATION> */

typedef struct _NDASUSER_LOGICALDEVICE_INFORMATIONW {

	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
	ACCESS_MASK GrantedAccess;
	ACCESS_MASK MountedAccess;

	union {
		struct {
			DWORD Blocks;
			DWORD Flags;
			struct {
				DWORD Revision;
				WORD Type;
				WORD KeyLength;
			} ContentEncrypt;
		} LogicalDiskInformation;

		struct {
			DWORD Reserved;
		} LogicalDVDInformation;

		BYTE Reserved[48];
	} SubType;

	DWORD nUnitDeviceEntries;
	NDASUSER_LOGICALDEVICE_MEMBER_ENTRYW FirstUnitDevice;
	NDAS_LOGICALDEVICE_PARAMS LogicalDeviceParams;

} NDASUSER_LOGICALDEVICE_INFORMATIONW, *PNDASUSER_LOGICALDEVICE_INFORMATIONW;

/* <COMBINE NDASUSER_LOGICALDEVICE_INFORMATIONW> */

typedef struct _NDASUSER_LOGICALDEVICE_INFORMATIONA {

	NDAS_LOGICALDEVICE_TYPE LogicalDeviceType;
	ACCESS_MASK GrantedAccess;
	ACCESS_MASK MountedAccess;

	union {

		struct {
			DWORD Blocks;
			DWORD Flags;
			struct {
				DWORD Revision;
				WORD Type;
				WORD KeyLength;
			} ContentEncrypt;
		} LogicalDiskInformation;

		struct {
			DWORD Reserved;
		} LogicalDVDInformation;

		BYTE Reserved[48];
	} SubType;

	DWORD nUnitDeviceEntries;
	NDASUSER_LOGICALDEVICE_MEMBER_ENTRYA FirstUnitDevice;
	NDAS_LOGICALDEVICE_PARAMS LogicalDeviceParams;

} NDASUSER_LOGICALDEVICE_INFORMATIONA, *PNDASUSER_LOGICALDEVICE_INFORMATIONA;

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASUSER_LOGICALDEVICE_INFORMATION NDASUSER_LOGICALDEVICE_INFORMATIONW
#define PNDASUSER_LOGICALDEVICE_INFORMATION PNDASUSER_LOGICALDEVICE_INFORMATIONW
#else
#define NDASUSER_LOGICALDEVICE_INFORMATION NDASUSER_LOGICALDEVICE_INFORMATIONA
#define PNDASUSER_LOGICALDEVICE_INFORMATION PNDASUSER_LOGICALDEVICE_INFORMATIONA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryLogicalDeviceInformation> 

Declaration

BOOL
NdasQueryLogicalDeviceInformation(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT PNDASUSER_LOGICALDEVICE_INFORMATION pLogDevInfo);

Summary

	Query the information of the NDAS logical device

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryLogicalDeviceInformationW(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT PNDASUSER_LOGICALDEVICE_INFORMATIONW pLogDevInfo);

/* <COMBINE NdasQueryLogicalDeviceInformationW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryLogicalDeviceInformationA(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT PNDASUSER_LOGICALDEVICE_INFORMATIONA pLogDevInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryLogicalDeviceInformation NdasQueryLogicalDeviceInformationW
#else
#define NdasQueryLogicalDeviceInformation NdasQueryLogicalDeviceInformationA
#endif
/*DOM-IGNORE-END*/

/* @@NdasLogicalDeviceMemberEnumProc
<TITLE NdasLogicalDeviceMemberEnumProc> 

Declaration

BOOL
NdasLogicalDeviceMemberEnumProc(
	PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry, 
	LPVOID lpContext);

Summary

	Application-defined callback function used for the parameter of
	NdasEnumLogicalDeviceMembers.

*/

/* <COMBINE NdasLogicalDeviceMemberEnumProc> */

typedef BOOL (CALLBACK* NDASLOGICALDEVICEMEMBERENUMPROCW)(
	PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYW lpEntry, LPVOID lpContext);

/* <COMBINE NdasLogicalDeviceMemberEnumProc> */

typedef BOOL (CALLBACK* NDASLOGICALDEVICEMEMBERENUMPROCA)(
	PNDASUSER_LOGICALDEVICE_MEMBER_ENTRYA lpEntry, LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NDASLOGICALDEVICEMEMBERENUMPROC NDASLOGICALDEVICEMEMBERENUMPROCW
#else
#define NDASLOGICALDEVICEMEMBERENUMPROC NDASLOGICALDEVICEMEMBERENUMPROCA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasEnumLogicalDeviceMembers> 

Declaration

BOOL
NdasEnumLogicalDeviceMembers(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	IN NDASLOGICALDEVICEMEMBERENUMPROC lpEnumProc,
	IN LPVOID lpContext);

Summary

	NdasEnumLogicalDeviceMembers function enumerates all unit devices
	of the NDAS logical device specified by the logical device ID
	by passing a pointer to NDASUSER_LOGICALDEVICE_MEMBER_ENTRY structure 
	for each NDAS unit devices, in turn, to an application-defined callback function. 
	NdasEnumLogicalDeviceMembers continues until the last unit device is 
	enumerated or the callback function returns FALSE.
*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEnumLogicalDeviceMembersW(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	IN NDASLOGICALDEVICEMEMBERENUMPROCW lpEnumProc,
	IN LPVOID lpContext);

/* <COMBINE NdasEnumLogicalDeviceMembersW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEnumLogicalDeviceMembersA(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	IN NDASLOGICALDEVICEMEMBERENUMPROCA lpEnumProc,
	IN LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasEnumLogicalDeviceMembers NdasEnumLogicalDeviceMembersW
#else
#define NdasEnumLogicalDeviceMembers NdasEnumLogicalDeviceMembersA
#endif
/*DOM-IGNORE-END*/

#define MAX_NDAS_EVENT_CALLBACK 5

/* <TITLE NDAS_EVENT_INFO>
*/

typedef struct _NDAS_EVENT_INFO {
	NDAS_EVENT_TYPE EventType;
	union {
		NDAS_EVENT_LOGICALDEVICE_INFO LogicalDeviceInfo;
		NDAS_EVENT_DEVICE_INFO DeviceInfo;
		NDAS_EVENT_SURRENDER_REQUEST_INFO SurrenderRequestInfo;
		NDAS_EVENT_UNITDEVICE_INFO UnitDeviceInfo;
	} EventInfo;
} NDAS_EVENT_INFO, *PNDAS_EVENT_INFO;

/* @@NdasEventProc
<TITLE NdasEventProc> 

Declaration

VOID
NdasEventProc(
	DWORD dwError,
	PNDAS_EVENT_INFO pEventInfo, 
	LPVOID lpContext);

Summary

	An application-defined callback function.

*/

/* <COMBINE NdasEventProc> */
typedef VOID (CALLBACK* NDASEVENTPROC)(
	DWORD dwError, PNDAS_EVENT_INFO pEventInfo, LPVOID lpContext);

/* <TITLE HNDASEVENTCALLBACK>
	Handle to an event callback object.
*/

#ifdef STRICT
struct HNDASEVENTCALLBACK__ { int unused; };
typedef struct HNDASEVENTCALLBACK__ *HNDASEVENTCALLBACK;
#else
typedef PVOID HNDASEVENTCALLBACK;
#endif

/* <TITLE NdasRegisterEventCallback>

Declaration

HNDASEVENTCALLBACK
NdasRegisterEventCallback(
	IN NDASEVENTPROC lpEventProc, 
	IN LPVOID lpContext);

Summary

	Registers an application-defined NDAS callback function,
	which will be called whenever an event occurs.

*/

NDASUSER_LINKAGE
HNDASEVENTCALLBACK
NDASUSERAPI
NdasRegisterEventCallback(
	IN NDASEVENTPROC lpEventProc, 
	IN LPVOID lpContext);

/* <TITLE NdasUnregisterEventCallback>

Declaration

BOOL
NdasUnregisterEventCallback(
	IN HNDASEVENTCALLBACK hEventCallback);

Summary

	Unregisters the specified NDAS event callback handle.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasUnregisterEventCallback(
	IN HNDASEVENTCALLBACK hEventCallback);

/* <TITLE NdasNotifyUnitDeviceChange>

Declaration

BOOL
NdasNotifyUnitDeviceChange(
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo);

BOOL
NdasNotifyUnitDeviceChangeById(
	IN LPCTSTR lpszDeviceStringId,
	IN DWORD dwUnitNo);

Summary

	NdasNotifyUnitDeviceChange notifies the changes of the unit device
	information by the application. Typically, when the application
	changes the the bind information stored in the NDAS unit disk,
	the application should call this function to notify other hosts
	in the network.

Parameters

Returns

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyUnitDeviceChange(
	IN DWORD dwSlotNo,
	IN DWORD dwUnitNo);

/* <COMBINE NdasNotifyUnitDeviceChange> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyUnitDeviceChangeByIdW(
	IN LPCWSTR lpszDeviceStringId,
	IN DWORD dwUnitNo);

/* <COMBINE NdasNotifyUnitDeviceChange> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasNotifyUnitDeviceChangeByIdA(
	IN LPCSTR lpszDeviceStringId,
	IN DWORD dwUnitNo);

/* <TITLE NdasSetServiceParam>

Declaration

BOOL
NdasSetServiceParam(
	CONST NDAS_SERVICE_PARAM* pParam);

Summary

	NdasSetServiceParam changes the parameters of the NDAS service.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasSetServiceParam(CONST NDAS_SERVICE_PARAM* pParam);

/* <TITLE NdasGetServiceParam>

Declaration

BOOL
NdasGetServiceParam(
	DWORD ParamCode, 
	NDAS_SERVICE_PARAM* pParam);

Summary

	NdasGetServiceParam retrieve the current parameters from the NDAS
	service.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetServiceParam(DWORD ParamCode, NDAS_SERVICE_PARAM* pParam);

/* <TITLE NdasIsNdasVolumePath>

Declaration:

BOOL
NdasIsNdasVolumePath(
	IN LPCTSTR szVolumePath, 
	OUT LPBOOL lpbNdasPath);

Summary:

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasIsNdasVolumePathW(
	IN LPCWSTR szVolumePath,
	OUT LPBOOL lpbNdasPath);

/* <COMBINE NdasIsNdasVolumePathW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasIsNdasVolumePathA(
	IN LPCSTR szVolumePath, 
	OUT LPBOOL lpbNdasPath);

/* <TITLE NdasGetLogicalDeviceForPath>

Declaration:

BOOL
NdasGetLogicalDeviceForPathW(
	IN LPCWSTR szPath, 
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

Summary:

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetLogicalDeviceForPathW(
	IN LPCWSTR szPath, 
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/* <COMBINE NdasGetLogicalDeviceForPathW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasGetLogicalDeviceForPathA(
	IN LPCSTR szPath, 
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

#ifdef UNICODE
#define NdasGetLogicalDeviceForPath NdasGetLogicalDeviceForPathW
#else
#define NdasGetLogicalDeviceForPath NdasGetLogicalDeviceForPathA
#endif

/* <TITLE NdasEjectLogicalDeviceEx>

Declaration

BOOL
NdasEjectLogicalDeviceEx(
	IN OUT PNDAS_LOGICALDEVICE_EJECT_PARAM EjectParam);

Summary

	NdasEjectLogicalDeviceEx is a function to request a ejection of
	the NDAS logical device. Unlike NdasEjectLogicalDevice, this function
	is a synchronous function. When this function returns, the requested
	logical device is ejected or intact if vetoed.

Parameters

	The caller should set Size and LogicalDeviceId fields when
	calling this function. When the function succeeded, 
	the ConfigRet value indicates if the ejection is done or vetoed.

Returns

	If the function succeeds, the return value is non-zero.
	The success of the function does not necessary mean the ejection is
	done. The caller should check the value of ConfigRet on successful
	return. 
	
	When ConfigRet is CR_SUCCESS, ejection is completed. 
	When it is CR_REMOVE_VETOED, eject request is vetoed by 
	the OS component specified by VetoType and VetoName.

	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                              

	On Windows 2000, this request is processed from caller's process
	token and the caller should have enough privileges to
	eject the device and the caller should be interactive logon user.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEjectLogicalDeviceExW(
	IN OUT PNDAS_LOGICALDEVICE_EJECT_PARAMW EjectParam);

/* <COMBINE NdasEjectLogicalDeviceExW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasEjectLogicalDeviceExA(
	IN OUT PNDAS_LOGICALDEVICE_EJECT_PARAMA EjectParam);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasEjectLogicalDeviceEx NdasEjectLogicalDeviceExW
#else
#define NdasEjectLogicalDeviceEx NdasEjectLogicalDeviceExA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasEjectLogicalDeviceEx>

Declaration

BOOL
NdasQueryNdasScsiLocation(
	NDAS_LOGICALDEVICE_ID LogicalDeviceId,
	PNDAS_SCSI_LOCATION pNdasScsiLocation);

Summary

	NdasQueryNdasScsiLocation queries the NDAS SCSI location of 
	the NDAS logical device.

Returns

	If the function succeeds, the return value is non-zero.

	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.                              

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryNdasScsiLocation(
	IN NDAS_LOGICALDEVICE_ID LogicalDeviceId,
	OUT PNDAS_SCSI_LOCATION pNdasScsiLocation);


#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASUSER_H_ */
