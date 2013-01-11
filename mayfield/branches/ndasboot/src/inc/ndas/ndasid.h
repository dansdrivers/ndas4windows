#pragma once
#include <windows.h>
#include "ndas/ndasctype.h"

#ifdef __cplusplus 
extern "C" {
#endif

BOOL 
WINAPI
NdasIdValidateStringIdKeyW(
	LPCWSTR lpszDeviceStringId, 
	LPCWSTR lpszDeviceStringKey /* = NULL */, 
	PBOOL pbWritable /* = NULL */);

BOOL 
WINAPI
NdasIdValidateStringIdKeyA(
	LPCSTR lpszDeviceStringId, 
	LPCSTR lpszDeviceStringKey /* = NULL */, 
	PBOOL pbWritable /* = NULL */);

#ifdef UNICODE
#define NdasIdValidateStringIdKey NdasIdValidateStringIdKeyW
#else
#define NdasIdValidateStringIdKey NdasIdValidateStringIdKeyA
#endif

BOOL 
WINAPI
NdasIdConvertStringIdToRealIdW(
	LPCWSTR lpszDeviceStringId, 
	PNDAS_DEVICE_ID pDeviceId);

BOOL 
WINAPI
NdasIdConvertStringIdToRealIdA(
	LPCSTR lpszDeviceStringId, 
	PNDAS_DEVICE_ID pDeviceId);

#ifdef UNICODE
#define NdasIdConvertStringIdToRealId NdasIdConvertStringIdToRealIdW
#else
#define NdasIdConvertStringIdToRealId NdasIdConvertStringIdToRealIdA
#endif

BOOL 
WINAPI
NdasIdConvertRealIdToStringIdW(
	const NDAS_DEVICE_ID* pDeviceId, 
	LPWSTR lpszDeviceStringId);

BOOL 
WINAPI
NdasIdConvertRealIdToStringIdA(
	const NDAS_DEVICE_ID* pDeviceId, 
	LPSTR lpszDeviceStringId);

#ifdef UNICODE
#define NdasIdConvertRealIdToStringId NdasIdConvertRealIdToStringIdW
#else
#define NdasIdConvertRealIdToStringId NdasIdConvertRealIdToStringIdA
#endif

#ifdef __cplusplus
}
#endif
