#pragma once
#ifndef _NDASSTRINGID_H_
#define _NDASSTRINGID_H_

#ifdef __cplusplus
extern "C" {
#endif

BOOL 
ValidateStringIdKeyW(
	LPCWSTR lpszDeviceStringId, LPCWSTR lpszDeviceStringKey = NULL, PBOOL pbWritable = NULL);

BOOL 
ValidateStringIdKeyA(
	LPCSTR lpszDeviceStringId, LPCSTR lpszDeviceStringKey = NULL, PBOOL pbWritable = NULL);

#ifdef UNICODE
#define ValidateStringIdKey ValidateStringIdKeyW
#else
#define ValidateStringIdKey ValidateStringIdKeyA
#endif

BOOL 
ConvertStringIdToRealIdW(
	LPCWSTR lpszDeviceStringId, PNDAS_DEVICE_ID pDeviceId);

BOOL 
ConvertStringIdToRealIdA(
	LPCSTR lpszDeviceStringId, PNDAS_DEVICE_ID pDeviceId);

#ifdef UNICODE
#define ConvertStringIdToRealId ConvertStringIdToRealIdW
#else
#define ConvertStringIdToRealId ConvertStringIdToRealIdA
#endif

BOOL 
ConvertRealIdToStringIdW(
	const PNDAS_DEVICE_ID pDeviceId, LPWSTR lpszDeviceStringId);

BOOL 
ConvertRealIdToStringIdA(
	const PNDAS_DEVICE_ID pDeviceId, LPSTR lpszDeviceStringId);

#ifdef UNICODE
#define ConvertRealIdToStringId ConvertRealIdToStringIdW
#else
#define ConvertRealIdToStringId ConvertRealIdToStringIdA
#endif

#ifdef __cplusplus
}
#endif

#endif // _NDASSTRINGID_H_