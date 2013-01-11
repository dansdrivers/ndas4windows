#pragma once
#include <windows.h>
#include <tchar.h>
#include "ndas/ndasdib.h"

struct _NDASCOMM_CONNECTION_INFO;
typedef _NDASCOMM_CONNECTION_INFO NDASCOMM_CONNECTION_INFO, *PNDASCOMM_CONNECTION_INFO;

#define NDASENC_ERROR_HASH_PROVIDER_INIT_FAILURE	0x0001
#define NDASENC_ERROR_HASH_GEN_FAILURE				0x0002
#define NDASENC_ERROR_HASH_GEN_FAILURE_2			0x0003
#define NDASENC_ERROR_HASH_GEN_FAILURE_3			0x0004
#define NDASENC_ERROR_SET_CONFIG_VALUE_FAILURE		0x0011
#define NDASENC_ERROR_GET_CONFIG_VALUE_FAILURE		0x0012
#define NDASENC_ERROR_REMOVE_CONFIG_VALUE_FAILURE	0x0013
#define NDASENC_ERROR_INVALID_SYSKEY_FILE			0x0021

#define NDASENC_ERROR_CEB_INVALID_SIGNATURE			0x0031
#define NDASENC_ERROR_CEB_INVALID_CRC				0x0032
#define NDASENC_ERROR_CEB_UNSUPPORTED_REVISION		0x0033
#define NDASENC_ERROR_CEB_INVALID_KEY_LENGTH		0x0034
#define NDASENC_ERROR_CEB_INVALID_FINGERPRINT		0x0035

#define NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH 64

#include <pshpack1.h>
typedef struct _NDAS_CONTENT_ENCRYPT {
	unsigned _int16	Method;
	unsigned _int16	KeyLength;
	unsigned _int8	Key[NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH];
} NDAS_CONTENT_ENCRYPT, *PNDAS_CONTENT_ENCRYPT;
#include <poppack.h>

UINT
WINAPI
NdasEncCreateKey(
	LPCTSTR szPassphrase,
	DWORD cbKey,
	LPBYTE pbKey);

UINT
WINAPI
NdasEncVerifySysKey(
	LPCTSTR szPassphrase, 
	LPBOOL pbVerified);

UINT
WINAPI
NdasEncSetSysKeyPhrase(
	LPCTSTR szPassphrase);

UINT
WINAPI
NdasEncCreateSysKey(
	LPCTSTR szPassphrase, 
	LPBYTE pbKey128);

UINT
WINAPI
NdasEncGetSysKey(
	DWORD cbSysKey, 
	LPBYTE pbSysKey, 
	LPDWORD lpcbUsed);

UINT
WINAPI
NdasEncSetSysKey(
	DWORD cbSysKey, 
	CONST BYTE* pbSysKey);

UINT
WINAPI
NdasEncRemoveSysKey();

UINT
WINAPI
NdasEncCreateFingerprint(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* pDiskKey,
	IN DWORD cbDiskKey,
	IN OUT BYTE* pFingerprint,
	IN DWORD cbFingerprint,
	OUT LPDWORD pcbUsed);

UINT
WINAPI
NdasEncVerifyFingerprint(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* pDiskKey,
	IN DWORD cbDiskKey,
	IN CONST BYTE* pFingerprint,
	IN DWORD cbFingerprint);

UINT
WINAPI
NdasEncVerifyFingerprintCEB(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST NDAS_CONTENT_ENCRYPT_BLOCK* pCEB);

UINT
WINAPI
NdasEncCreateSysKeyFile(
	LPCTSTR szFileName,
	LPCTSTR szPassphrase);

UINT
WINAPI
NdasEncImportSysKeyFromFile(
	LPCTSTR szFileName);

UINT
WINAPI
NdasEncExportSysKeyToFile(
	LPCTSTR szFileName);

BOOL
WINAPI
NdasEncIsValidKeyLength(
	DWORD Encryption, 
	DWORD KeyLength);

UINT
WINAPI
NdasEncCreateContentEncryptBlock(
	IN CONST BYTE *lpSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* lpDiskKey,
	IN DWORD cbDiskKey,
	IN DWORD Encryption,
	OUT NDAS_CONTENT_ENCRYPT_BLOCK* pCEB);

UINT
WINAPI
NdasEncVerifyContentEncryptBlock(
	IN CONST NDAS_CONTENT_ENCRYPT_BLOCK* pCEB);

UINT
WINAPI
NdasEncCreateContentEncryptKey(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* pDiskKey,
	IN DWORD cbDiskKey,
	OUT BYTE* pKey,
	IN DWORD cbKey);
