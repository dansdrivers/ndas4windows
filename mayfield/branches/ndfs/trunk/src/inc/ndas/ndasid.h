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
	BYTE VID;
	BYTE Reserved[2];
} NDASID_EXT_DATA, *PNDASID_EXT_DATA;

#include <poppack.h>

BOOL
WINAPI
NdasIdValidateA(
	IN LPCSTR szStringId,
	IN LPCSTR szWriteKey OPTIONAL);

BOOL
WINAPI
NdasIdValidateW(
	IN LPCWSTR szStringId,
	IN LPCWSTR szWriteKey OPTIONAL);

#ifdef UNICODE
#define NdasIdValidate NdasIdValidateW
#else
#define NdasIdValidate NdasIdValidateA
#endif

BOOL
WINAPI
NdasIdValidateExA(
	IN LPCSTR szStringId,
	IN LPCSTR szWriteKey OPTIONAL,
	IN const NDASID_EXT_KEY* pExtKey,
	IN const NDASID_EXT_DATA* pExtData);


BOOL
WINAPI
NdasIdValidateExW(
	IN LPCWSTR szStringId,
	IN LPCWSTR szWriteKey OPTIONAL,
	IN const NDASID_EXT_KEY* pExtKey,
	IN const NDASID_EXT_DATA* pExtData);

#ifdef UNICODE
#define NdasIdValidateEx NdasIdValidateExW
#else
#define NdasIdValidateEx NdasIdValidateExA
#endif

BOOL
WINAPI
NdasIdStringToDeviceA(
	IN LPCSTR szStringId,
	OUT NDAS_DEVICE_ID* pDeviceId);

BOOL
WINAPI
NdasIdStringToDeviceW(
	IN LPCWSTR szStringId,
	OUT NDAS_DEVICE_ID* pDeviceId);

#ifdef UNICODE
#define NdasIdStringToDevice NdasIdStringToDeviceW
#else
#define NdasIdStringToDevice NdasIdStringToDeviceA
#endif

BOOL
WINAPI
NdasIdStringToDeviceExA(
	IN LPCSTR szStringId,
	OUT NDAS_DEVICE_ID* pDeviceId,
	IN const NDASID_EXT_KEY* pExtKey,
	OUT NDASID_EXT_DATA* pExtData);

BOOL
WINAPI
NdasIdStringToDeviceExW(
	IN LPCWSTR szStringId,
	OUT NDAS_DEVICE_ID* pDeviceId,
	IN const NDASID_EXT_KEY* pExtKey,
	OUT NDASID_EXT_DATA* pExtData);

#ifdef UNICODE
#define NdasIdStringToDeviceEx NdasIdStringToDeviceExW
#else
#define NdasIdStringToDeviceEx NdasIdStringToDeviceExA
#endif

BOOL
WINAPI
NdasIdDeviceToStringA(
	IN const NDAS_DEVICE_ID* pDeviceId,
	OUT LPSTR lpszStringId OPTIONAL,
	OUT LPSTR lpszWriteKey OPTIONAL);

BOOL
WINAPI
NdasIdDeviceToStringW(
	IN const NDAS_DEVICE_ID* pDeviceId,
	OUT LPWSTR lpszStringId OPTIONAL,
	OUT LPWSTR lpszWriteKey OPTIONAL);

#ifdef UNICODE
#define NdasIdDeviceToString NdasIdDeviceToStringW
#else
#define NdasIdDeviceToString NdasIdDeviceToStringA
#endif

BOOL
WINAPI
NdasIdDeviceToStringExA(
	IN const NDAS_DEVICE_ID* pDeviceId,
	OUT LPSTR lpszStringId OPTIONAL,
	OUT LPSTR lpszWriteKey OPTIONAL,
	IN const NDASID_EXT_KEY* pExtKey,
	IN const NDASID_EXT_DATA* pExtData);

BOOL
WINAPI
NdasIdDeviceToStringExW(
	IN const NDAS_DEVICE_ID* pDeviceId,
	OUT LPWSTR lpszStringId OPTIONAL,
	OUT LPWSTR lpszWriteKey OPTIONAL,
	IN const NDASID_EXT_KEY* pExtKey,
	IN const NDASID_EXT_DATA* pExtData);

#ifdef UNICODE
#define NdasIdDeviceToStringEx NdasIdDeviceToStringExW
#else
#define NdasIdDeviceToStringEx NdasIdDeviceToStringExA
#endif

#ifdef __cplusplus
}
#endif
