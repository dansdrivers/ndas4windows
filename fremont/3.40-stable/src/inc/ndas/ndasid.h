#pragma once
#include <windows.h>
#include <ndas/ndastypeex.h>

#ifdef __cplusplus 
extern "C" {
#endif

#include <pshpack4.h>

typedef struct _NDASID_EXT_KEY {

	BYTE Key1[8];
	BYTE Key2[8];

} NDASID_EXT_KEY, *PNDASID_EXT_KEY;

typedef struct _NDASID_EXT_DATA {

	BYTE Seed;
	BYTE Vid;
	BYTE Reserved[2];

} NDASID_EXT_DATA, *PNDASID_EXT_DATA;

#include <poppack.h>

BOOL
WINAPI
NdasIdValidateA(
	__in LPCSTR NdasId,
	__in_opt LPCSTR WriteKey);

BOOL
WINAPI
NdasIdValidateW(
	__in LPCWSTR NdasId,
	__in_opt LPCWSTR WriteKey);

#ifdef UNICODE
#define NdasIdValidate NdasIdValidateW
#else
#define NdasIdValidate NdasIdValidateA
#endif

BOOL
WINAPI
NdasIdValidateExA(
	__in LPCSTR NdasId,
	__in_opt LPCSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData);

BOOL
WINAPI
NdasIdValidateExW(
	__in LPCWSTR NdasId,
	__in_opt LPCWSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData);

#ifdef UNICODE
#define NdasIdValidateEx NdasIdValidateExW
#else
#define NdasIdValidateEx NdasIdValidateExA
#endif

BOOL
WINAPI
NdasIdStringToDeviceA(
	__in LPCSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId);

BOOL
WINAPI
NdasIdStringToDeviceW(
	__in LPCWSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId);

#ifdef UNICODE
#define NdasIdStringToDevice NdasIdStringToDeviceW
#else
#define NdasIdStringToDevice NdasIdStringToDeviceA
#endif

BOOL
WINAPI
NdasIdStringToDeviceExA(
	__in LPCSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__out_opt NDASID_EXT_DATA* ExtData);

BOOL
WINAPI
NdasIdStringToDeviceExW(
	__in LPCWSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__out_opt NDASID_EXT_DATA* ExtData);

#ifdef UNICODE
#define NdasIdStringToDeviceEx NdasIdStringToDeviceExW
#else
#define NdasIdStringToDeviceEx NdasIdStringToDeviceExA
#endif

BOOL
WINAPI
NdasIdDeviceToStringA(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPSTR WriteKey);

BOOL
WINAPI
NdasIdDeviceToStringW(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPWSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPWSTR WriteKey);

#ifdef UNICODE
#define NdasIdDeviceToString NdasIdDeviceToStringW
#else
#define NdasIdDeviceToString NdasIdDeviceToStringA
#endif

BOOL
WINAPI
NdasIdDeviceToStringExA(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData);

BOOL
WINAPI
NdasIdDeviceToStringExW(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPWSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPWSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData);

#ifdef UNICODE
#define NdasIdDeviceToStringEx NdasIdDeviceToStringExW
#else
#define NdasIdDeviceToStringEx NdasIdDeviceToStringExA
#endif

#ifdef __cplusplus
}
#endif
