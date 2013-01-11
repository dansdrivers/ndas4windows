#include "stdafx.h"

#include "ndas/ndastypeex.h"
#include "ndas/ndasdib.h"
#include "ndas/ndascntenc.h"
#include "ndas/ndassyscfg.h"

#include "xs/xautores.h"
#include "xs/xcrypt_encrypt.h"
#include "xs/xcrypt_hash.h"
#include "xs/xstrhelp.h"

#include "scrc32.h"


// {C9463038-6917-4758-8AFE-3738305A86A6}
// {D3D7BEEA-0BAB-4f66-A086-36F4688F3EEB}
// {1366BDAC-7A26-454a-B49A-FF6EC1BD4BA7}
// {6CE685B1-9CD9-4163-8555-0B6C90877624}
static CONST GUID NDAS_ENC_GUIDS[] = {
{ 0xc9463038, 0x6917, 0x4758, { 0x8a, 0xfe, 0x37, 0x38, 0x30, 0x5a, 0x86, 0xa6 } },
{ 0xd3d7beea, 0x0bab, 0x4f66, { 0xa0, 0x86, 0x36, 0xf4, 0x68, 0x8f, 0x3e, 0xeb } },
{ 0x1366bdac, 0x7a26, 0x454a, { 0xb4, 0x9a, 0xff, 0x6e, 0xc1, 0xbd, 0x4b, 0xa7 } },
{ 0x6ce685b1, 0x9cd9, 0x4163, { 0x85, 0x55, 0xb, 0x6c, 0x90, 0x87, 0x76, 0x24 } }
};


// {9DD7BB6B-2CFA-4957-B918-C687CAF96BCB}
static const GUID NDAS_SYSKEY_FILE_GUID = 
{ 0x9dd7bb6b, 0x2cfa, 0x4957, { 0xb9, 0x18, 0xc6, 0x87, 0xca, 0xf9, 0x6b, 0xcb } };

#include <pshpack1.h>
#pragma warning(disable: 4200)
typedef struct _NDAS_ENC_SYSKEY_FILE_HEADER {
	GUID FileGuid;
	DWORD KeyDataLength;
	DWORD KeyDecryptedDataCRC32;
	BYTE KeyData[];
} NDAS_ENC_SYSKEY_FILE_HEADER, *PNDAS_ENC_SYSKEY_FILE_HEADER;
#pragma warning(default: 4200)
#include <poppack.h>

//
// Create a 128 bit key
//
UINT
WINAPI
NdasEncCreateKey128(
	LPCTSTR szPassphrase, 
	CONST BYTE* lpEntropy,
	DWORD cbEntrypy,
	LPBYTE lpbKey128)
{
	BOOL fInvalidParam = ::IsBadStringPtr(szPassphrase, -1);
	_ASSERTE(!fInvalidParam);
	if (fInvalidParam) {
		return ERROR_INVALID_PARAMETER;
	}

	fInvalidParam = ::IsBadReadPtr(lpEntropy, cbEntrypy);
	_ASSERTE(!fInvalidParam);
	if (fInvalidParam) {
		return ERROR_INVALID_PARAMETER;
	}

	fInvalidParam = ::IsBadWritePtr(lpbKey128, 16);
	_ASSERTE(!fInvalidParam);
	if (fInvalidParam) {
		return ERROR_INVALID_PARAMETER;
	}

	size_t cbPassphrase = 0;

	HRESULT hr = ::StringCbLength(
		szPassphrase, 
		STRSAFE_MAX_CCH * sizeof(TCHAR), 
		&cbPassphrase);

	if (FAILED(hr)) {
		return ERROR_INVALID_PARAMETER;
	}

	//
	// Get MD5 hash of the passphrase
	//

	AutoResourceT<xcrypt::IHashing*> pHashing = 
		xcrypt::CreateCryptAPIMD5Hashing();

	BOOL fSuccess = pHashing->Initialize();

	if (!fSuccess) {
		return NDASENC_ERROR_HASH_PROVIDER_INIT_FAILURE;
	}

	fSuccess = pHashing->HashData(
		(CONST BYTE*) szPassphrase, 
		cbPassphrase);

	if (!fSuccess) {
		return NDASENC_ERROR_HASH_GEN_FAILURE;
	}

	fSuccess = pHashing->HashData(
		lpEntropy,
		cbEntrypy);

	if (!fSuccess) {
		return NDASENC_ERROR_HASH_GEN_FAILURE_2;
	}

	DWORD cbHash = 0;
	LPBYTE lpbHash = pHashing->GetHashValue(&cbHash);

	if (NULL == lpbHash) {
		return NDASENC_ERROR_HASH_GEN_FAILURE_3;
	}

#ifdef _DEBUG
	_tprintf(_T("Hash: %s\n"), 
		xs::CXSByteString(cbHash, lpbHash, _T(' ')).ToString());
#endif

	_ASSERTE(16 == cbHash);

	::CopyMemory(lpbKey128, lpbHash, cbHash);

	return ERROR_SUCCESS;

}

UINT
WINAPI
NdasEncCreateKey(
	LPCTSTR szPassphrase,
	DWORD cbKey,
	LPBYTE pbKey)
{
	//
	// Parameter Checking
	//
	BOOL fInvalidParam = (cbKey > 64) || (0 == cbKey);
	_ASSERTE(!fInvalidParam);
	if (fInvalidParam) {
		return ERROR_INVALID_PARAMETER;
	}

	fInvalidParam = ::IsBadWritePtr(pbKey, cbKey);
	_ASSERTE(!fInvalidParam);
	if (fInvalidParam) {
		return ERROR_INVALID_PARAMETER;
	}

	fInvalidParam = ::IsBadStringPtr(szPassphrase, -1);
	_ASSERTE(!fInvalidParam);
	if (fInvalidParam) {
		return ERROR_INVALID_PARAMETER;
	}


	DWORD dwRounds = (cbKey - 1) / 16 + 1;
	_ASSERTE(dwRounds <= 4);

	for (DWORD i = 0; i < dwRounds; ++i) {

		// MD5 hash returns always 128 bits (16 bytes)
		BYTE Key128[16] = {0};

		LPBYTE pbKeyBlock = pbKey + 16 * i;
		DWORD cbKeyBlock = cbKey - 16 * i;

		UINT lResult = ::NdasEncCreateKey128(
			szPassphrase,
			(CONST BYTE*) &NDAS_ENC_GUIDS[i],
			sizeof(GUID),
			Key128);
		if (ERROR_SUCCESS != lResult) {
			return lResult;
		}
		::CopyMemory(pbKeyBlock, Key128, cbKeyBlock);
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncCreateSysKey(
	LPCTSTR szPassphrase, 
	LPBYTE pbKey128)
{
	UINT lResult = ::NdasEncCreateKey128(
		szPassphrase,
		(CONST BYTE*) &NDAS_ENC_GUIDS[0],
		sizeof(NDAS_ENC_GUIDS[0]),
		pbKey128);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncSetSysKeyPhrase(LPCTSTR szPassphrase)
{
	BYTE pbSysKey[16] = {0};
	DWORD cbSysKey = sizeof(pbSysKey);

	UINT lResult = ::NdasEncCreateSysKey(
		szPassphrase, 
		pbSysKey);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	//
	// Set MD5 hash to the system key
	//

	lResult = ::NdasEncSetSysKey(cbSysKey, pbSysKey);

	return lResult;
}

UINT
WINAPI
NdasEncSetSysKey(DWORD cbSysKey, CONST BYTE* pbSysKey)
{
	BOOL fSuccess = ::NdasSysSetConfigValue(
		NDASSYS_KEYS_REGKEY,
		NULL,
		pbSysKey,
		cbSysKey);

	if (!fSuccess) {
		return NDASENC_ERROR_SET_CONFIG_VALUE_FAILURE;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncGetSysKey(DWORD cbSysKey, LPBYTE pbSysKey, LPDWORD lpcbUsed)
{
	BOOL fSuccess = ::NdasSysGetConfigValue(
		NDASSYS_KEYS_REGKEY,
		NULL,
		pbSysKey,
		cbSysKey,
		lpcbUsed);
	if (!fSuccess) {
		return NDASENC_ERROR_GET_CONFIG_VALUE_FAILURE;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncVerifySysKey(LPCTSTR szPassphrase, LPBOOL pbVerified)
{
	_ASSERTE(!::IsBadWritePtr(pbVerified, sizeof(BOOL)));
	if (::IsBadWritePtr(pbVerified, sizeof(BOOL))) {
		return ERROR_INVALID_PARAMETER;
	}

	_ASSERTE(!::IsBadStringPtr(szPassphrase, -1));
	if (::IsBadStringPtr(szPassphrase, -1)) {
		return ERROR_INVALID_PARAMETER;
	}

	BYTE userSysKey[16] = {0};
	DWORD cbUserSysKey = sizeof(userSysKey);

	UINT lResult = ::NdasEncCreateSysKey(
		szPassphrase, 
		userSysKey);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	BYTE storedSysKey[16] = {0};
	DWORD cbStoredSysKey = sizeof(storedSysKey);

	lResult = ::NdasEncGetSysKey(
		cbStoredSysKey, 
		storedSysKey, 
		&cbStoredSysKey);

	if (cbUserSysKey != cbStoredSysKey) {
		*pbVerified = FALSE;
		return ERROR_SUCCESS;
	}

	if (0 != ::memcmp(userSysKey, storedSysKey, cbUserSysKey)) {
		*pbVerified = FALSE;
		return ERROR_SUCCESS;
	}

	*pbVerified = TRUE;
	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncRemoveSysKey()
{
	BOOL fSuccess = ::NdasSysDeleteConfigValue(
		NDASSYS_KEYS_REGKEY,
		NULL);

	if (!fSuccess) {
		return NDASENC_ERROR_REMOVE_CONFIG_VALUE_FAILURE;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncCreateFingerprint(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* pDiskKey,
	IN DWORD cbDiskKey,
	IN OUT BYTE* pFingerprint,
	IN DWORD cbFingerprint,
	OUT LPDWORD pcbUsed)
{
	//pFingerprint = md5(pSysKey . pDiskKey);

	AutoResourceT<xcrypt::IHashing*> pHashing = xcrypt::CreateCryptAPIMD5Hashing();
	BOOL fSuccess = pHashing->Initialize();

	if (!fSuccess) {
		return NDASENC_ERROR_HASH_PROVIDER_INIT_FAILURE;
	}

	fSuccess = pHashing->HashData(pSysKey, cbSysKey);

	if (!fSuccess) {
		return NDASENC_ERROR_HASH_GEN_FAILURE;
	}

	fSuccess = pHashing->HashData(pDiskKey, cbDiskKey);

	if (!fSuccess) {
		return NDASENC_ERROR_HASH_GEN_FAILURE_2;
	}

	DWORD cbHash = 0;
	BYTE* pbHash = pHashing->GetHashValue(&cbHash);

	if (NULL == pbHash) {
		return NDASENC_ERROR_HASH_GEN_FAILURE_3;
	}

	if (NULL != pcbUsed) {
		*pcbUsed = cbHash;
	}

	if (cbFingerprint < cbHash) {
		return ERROR_INSUFFICIENT_BUFFER;
	}

	::CopyMemory(pFingerprint, pbHash, cbHash);

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncVerifyFingerprint(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* pDiskKey,
	IN DWORD cbDiskKey,
	IN CONST BYTE* pFingerprint,
	IN DWORD cbFingerprint)
{
	BYTE pActualFingerprint[16] = {0};
	DWORD cbActualFingerprint = sizeof(pActualFingerprint);

	UINT lResult = ::NdasEncCreateFingerprint(
		pSysKey, 
		cbSysKey, 
		pDiskKey, 
		cbDiskKey, 
		pActualFingerprint, 
		cbActualFingerprint, 
		&cbActualFingerprint);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	if (cbActualFingerprint != cbFingerprint) {
		return NDASENC_ERROR_CEB_INVALID_FINGERPRINT;
	}

	if (0 != ::memcmp(pActualFingerprint, pFingerprint, cbFingerprint)) {
		return NDASENC_ERROR_CEB_INVALID_FINGERPRINT;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncVerifyFingerprintCEB(
	IN CONST BYTE* lpSysKey,
	IN DWORD cbSysKey,
	IN CONST NDAS_CONTENT_ENCRYPT_BLOCK* pCEB)
{
	return NdasEncVerifyFingerprint(
		lpSysKey, 
		cbSysKey, 
		pCEB->Key,
		pCEB->KeyLength,
		pCEB->Fingerprint,
		sizeof(pCEB->Fingerprint));
}

UINT
WINAPI
NdasEncImportSysKeyFromFile(LPCTSTR szFileName)
{
	BYTE lpBuffer[1024] = {0}; // large enough to hold the entire data;
	PNDAS_ENC_SYSKEY_FILE_HEADER lpFileHeader = 
		reinterpret_cast<PNDAS_ENC_SYSKEY_FILE_HEADER>(lpBuffer);

	AutoFileHandle hFile = ::CreateFile(
		szFileName, 
		GENERIC_READ, 
		0, 
		NULL, 
		OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL, 
		NULL);

	if (INVALID_HANDLE_VALUE == (HANDLE) hFile) {
		return ::GetLastError();
	}

	GUID FileGuid = {0};
	DWORD cbRead = 0;
	BOOL fSuccess = ::ReadFile(
		hFile, 
		lpBuffer, 
		sizeof(lpBuffer),
		&cbRead,
		NULL);

	if (!fSuccess) {
		return ::GetLastError();
	}

	if (cbRead < sizeof(NDAS_ENC_SYSKEY_FILE_HEADER)) {
		return NDASENC_ERROR_INVALID_SYSKEY_FILE;
	}

	if (0 != ::memcmp(
		&lpFileHeader->FileGuid, 
		&NDAS_SYSKEY_FILE_GUID,
		sizeof(NDAS_SYSKEY_FILE_GUID)))
	{
		return NDASENC_ERROR_INVALID_SYSKEY_FILE;
	}

	if (cbRead != 
		lpFileHeader->KeyDataLength + sizeof(NDAS_ENC_SYSKEY_FILE_HEADER))
	{
		return NDASENC_ERROR_INVALID_SYSKEY_FILE;
	}

	lpFileHeader->KeyDecryptedDataCRC32;
	lpFileHeader->KeyData;

	//
	// Encrypt the key (DES)
	//
	xcrypt::IEncryption* pDesEnc = xcrypt::CreateCryptAPIDESEncryption();

	if (NULL == pDesEnc) {
		return ERROR_OUTOFMEMORY;
	}

	fSuccess = pDesEnc->Initialize();

	if (!fSuccess) {
		return ::GetLastError();
	}

	fSuccess = pDesEnc->SetKey(
		(CONST BYTE*) &NDAS_ENC_GUIDS[0],
		sizeof(NDAS_ENC_GUIDS[0]));

	if (!fSuccess) {
		return ::GetLastError();
	}

	fSuccess = pDesEnc->Decrypt(
		lpFileHeader->KeyData, 
		&lpFileHeader->KeyDataLength);

	if (!fSuccess) {
		return ::GetLastError();
	}

	//
	// Check CRC
	//
	DWORD dataCRC32 = crc32_calc(
		lpFileHeader->KeyData, 
		lpFileHeader->KeyDataLength);

	if (dataCRC32 != lpFileHeader->KeyDecryptedDataCRC32) {
		return NDASENC_ERROR_INVALID_SYSKEY_FILE;
	}

#ifdef _DEBUG
	_tprintf(_T("Importing System Key: %s\n"), 
		xs::CXSByteString(
			lpFileHeader->KeyDataLength, 
			lpFileHeader->KeyData, 
			_T(' ')).ToString());
#endif

	UINT uiResult = ::NdasEncSetSysKey(
		lpFileHeader->KeyDataLength, 
		lpFileHeader->KeyData);

	if (ERROR_SUCCESS != uiResult) {
		return uiResult;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncExportKeyToFile(LPCTSTR szFileName, DWORD cbKey, LPBYTE lpKey)
{
	// make this large enough to hold the entire data;
	BYTE lpBuffer[1024] = {0}; 
	PNDAS_ENC_SYSKEY_FILE_HEADER lpFileHeader = 
		reinterpret_cast<PNDAS_ENC_SYSKEY_FILE_HEADER>(lpBuffer);

	_ASSERTE(sizeof(lpBuffer) >= cbKey + sizeof(NDAS_ENC_SYSKEY_FILE_HEADER));

	::CopyMemory(lpFileHeader->KeyData, lpKey, cbKey);

	LPBYTE lpKeyData = lpFileHeader->KeyData;
	DWORD cbKeyData = cbKey;

	lpFileHeader->FileGuid = NDAS_SYSKEY_FILE_GUID;
	lpFileHeader->KeyDecryptedDataCRC32 = ::crc32_calc(lpKeyData, cbKeyData);

	//
	// Encrypt the key (DES)
	//
	xcrypt::IEncryption* pDesEnc = xcrypt::CreateCryptAPIDESEncryption();
	if (NULL == pDesEnc) {
		return ERROR_OUTOFMEMORY;
	}

	BOOL fSuccess = pDesEnc->Initialize();

	if (!fSuccess) {
		return ::GetLastError();
	}

	fSuccess = pDesEnc->SetKey(
		(CONST BYTE*) &NDAS_ENC_GUIDS[0],
		sizeof(NDAS_ENC_GUIDS[0]));

	if (!fSuccess) {
		return ::GetLastError();
	}

	fSuccess = pDesEnc->Encrypt(
		lpKeyData, 
		&cbKeyData, 
		sizeof(lpBuffer) - sizeof(NDAS_ENC_SYSKEY_FILE_HEADER));

	if (!fSuccess) {
		return ::GetLastError();
	}

	lpFileHeader->KeyDataLength = cbKeyData;

	DWORD cbFileSize = sizeof(NDAS_ENC_SYSKEY_FILE_HEADER) + cbKeyData;

	AutoFileHandle hFile = ::CreateFile(
		szFileName, 
		GENERIC_WRITE, 
		0, 
		NULL, 
		CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, 
		NULL);

	if (INVALID_HANDLE_VALUE == (HANDLE) hFile) {
		return ::GetLastError();
	}

	DWORD cbWritten = 0;
	fSuccess = ::WriteFile(
		hFile, 
		lpFileHeader, 
		cbFileSize,
		&cbWritten,
		NULL);

	if (!fSuccess) {
		return ::GetLastError();
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncExportSysKeyToFile(LPCTSTR szFileName)
{
	BYTE lpSysKey[16] = {0};
	DWORD cbSysKey = sizeof(lpSysKey);

	UINT uiResult = ::NdasEncGetSysKey(cbSysKey, lpSysKey, &cbSysKey);
	if (ERROR_SUCCESS != uiResult) {
		return uiResult;
	}

	uiResult = ::NdasEncExportKeyToFile(szFileName, cbSysKey, lpSysKey);

	if (ERROR_SUCCESS != uiResult) {
		return uiResult;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncCreateSysKeyFile(LPCTSTR szFileName, LPCTSTR szPassphrase)
{
	BYTE lpSysKey[16] = {0};
	DWORD cbSysKey = sizeof(lpSysKey);

	//
	// Create a sys key
	//
	UINT lResult = ::NdasEncCreateKey(szPassphrase, cbSysKey, lpSysKey);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	lResult = ::NdasEncExportKeyToFile(szFileName, cbSysKey, lpSysKey);

	if (ERROR_SUCCESS != lResult) {
		return lResult;
	}

	return ERROR_SUCCESS;
}

BOOL
WINAPI
NdasEncIsValidKeyLength(DWORD Encryption, DWORD KeyLength)
{
	switch (Encryption) {
	case NDAS_CONTENT_ENCRYPT_METHOD_NONE:
		return (0 == KeyLength);
	case NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE:
		// 32 bits
		return (4 == KeyLength);
	case NDAS_CONTENT_ENCRYPT_METHOD_AES:
		// 128, 192, 256 bites
		return (16 == KeyLength ) || (24 == KeyLength) || (32 == KeyLength);
	default:
		return FALSE;
	}
}

UINT
WINAPI
NdasEncCreateContentEncryptBlock(
	IN CONST BYTE *lpSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* lpDiskKey,
	IN DWORD cbDiskKey,
	IN DWORD Encryption,
	OUT NDAS_CONTENT_ENCRYPT_BLOCK* pCEB)
{
	if (::IsBadReadPtr(lpSysKey, cbSysKey)) {
		return ERROR_INVALID_PARAMETER;
	}
	if (::IsBadReadPtr(lpDiskKey, cbDiskKey)) {
		return ERROR_INVALID_PARAMETER;
	}
	if (::IsBadWritePtr(pCEB, sizeof(NDAS_CONTENT_ENCRYPT_BLOCK))) {
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!NdasEncIsValidKeyLength(Encryption, cbDiskKey)) {
		return NDASENC_ERROR_CEB_INVALID_KEY_LENGTH;
	}

	BYTE fprt[sizeof(pCEB->Fingerprint)];
	DWORD cbfprt = sizeof(fprt);

	UINT uiRet = ::NdasEncCreateFingerprint(
		lpSysKey, 
		cbSysKey, 
		lpDiskKey, 
		cbDiskKey, 
		fprt, 
		cbfprt, 
		&cbfprt);

	if (ERROR_SUCCESS != uiRet) {
		return uiRet;
	}

	pCEB->Signature = NDAS_CONTENT_ENCRYPT_BLOCK_SIGNATURE;
	pCEB->Revision = NDAS_CONTENT_ENCRYPT_REVISION;
	pCEB->Method = (UINT16) Encryption;
	pCEB->KeyLength = (UINT16) cbDiskKey;
	::CopyMemory(pCEB->Key, lpDiskKey, cbDiskKey);
	::CopyMemory(pCEB->Fingerprint, fprt, sizeof(pCEB->Fingerprint));

	pCEB->CRC32 = ::crc32_calc(pCEB->bytes_508, sizeof(pCEB->bytes_508));

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncVerifyContentEncryptBlock(IN CONST NDAS_CONTENT_ENCRYPT_BLOCK* pCEB)
{
	//
	// Check Signature
	//
	if (pCEB->Signature != NDAS_CONTENT_ENCRYPT_BLOCK_SIGNATURE) {
		return NDASENC_ERROR_CEB_INVALID_SIGNATURE;
	}

	//
	// Check CRC
	//
	UINT calculatedCRC = ::crc32_calc(
		(const unsigned char*) pCEB,
		sizeof(pCEB->bytes_508));

	UINT diskCRC = pCEB->CRC32;

	if (calculatedCRC != diskCRC) {
		return NDASENC_ERROR_CEB_INVALID_CRC;
	}

	//
	// Invariant if valid
	//
	// TODO: Handle modified revision later
	//
	if (pCEB->Revision != NDAS_CONTENT_ENCRYPT_REVISION) {
		return NDASENC_ERROR_CEB_UNSUPPORTED_REVISION;
	}

	if (!::NdasEncIsValidKeyLength(pCEB->Method, pCEB->KeyLength)) {
		return NDASENC_ERROR_CEB_INVALID_KEY_LENGTH;
	}

	return ERROR_SUCCESS;
}

UINT
WINAPI
NdasEncCreateContentEncryptKey(
	IN CONST BYTE* pSysKey,
	IN DWORD cbSysKey,
	IN CONST BYTE* pDiskKey,
	IN DWORD cbDiskKey,
	OUT BYTE* pKey,
	IN DWORD cbKey)
{
	if (::IsBadReadPtr(pSysKey, cbSysKey)) {
		_ASSERTE(FALSE);
		return ERROR_INVALID_PARAMETER;
	}

	if (::IsBadReadPtr(pDiskKey, cbDiskKey)) {
		_ASSERTE(FALSE);
		return ERROR_INVALID_PARAMETER;
	}

	if (::IsBadWritePtr(pKey, cbKey)) {
		_ASSERTE(FALSE);
		return ERROR_INVALID_PARAMETER;
	}

	for (DWORD i = 0; i < cbDiskKey && i < cbKey; ++i) {
		CONST BYTE* psk = &pSysKey[i % cbSysKey];
		CONST BYTE* pdk = &pDiskKey[i];
		BYTE* pk = &pKey[i];
		*pk = *psk ^ *pdk;
	}

	return ERROR_SUCCESS;
}
