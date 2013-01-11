#pragma once
#ifndef _XCRYPT_ENCRYPT_H_
#define _XCRYPT_ENCRYPT_H_

#include <windows.h>
#include <wincrypt.h>

#ifndef CRYPT_EXPORTABLE
#define CRYPT_EXPORTABLE        0x00000001
#endif

class CCryptEncrypt
{
public:

	CCryptEncrypt(
		ALG_ID Algorithm = CALG_DES, 
		LPCTSTR ProvName = MS_DEF_PROV, 
		DWORD ProvType = PROV_RSA_FULL);

	~CCryptEncrypt();

	HRESULT Initialize();
	HRESULT SetKey(const BYTE* Data, DWORD DataLength, DWORD Flags = CRYPT_EXPORTABLE);
	HRESULT Encrypt(BYTE* Data, LPDWORD DataLength, DWORD DataBufferLength, BOOL Final = TRUE);
	HRESULT Decrypt(BYTE* Data, LPDWORD DataLength, BOOL Final = TRUE);

private:

	const ALG_ID m_Algorithm;
	const LPCTSTR m_ProvName;
	const DWORD m_ProvType;

	HCRYPTPROV m_ProvHandle;
	HCRYPTHASH m_HashHandle;
	HCRYPTKEY m_KeyHandle;

};

inline CCryptEncrypt::CCryptEncrypt(ALG_ID Algorithm, LPCTSTR ProvName, DWORD ProvType) :
	m_Algorithm(Algorithm),
 	m_ProvName(ProvName),
 	m_ProvType(ProvType),
	m_ProvHandle(NULL),
 	m_HashHandle(NULL),
	m_KeyHandle(NULL)
{
}

inline CCryptEncrypt::~CCryptEncrypt()
{
	if (m_KeyHandle)
	{
		CryptDestroyKey(m_KeyHandle);
	}
	if (m_HashHandle)
	{
		CryptDestroyHash(m_HashHandle);
	}
	if (m_ProvHandle)
	{
		CryptReleaseContext(m_ProvHandle, 0);
	}
}

inline HRESULT CCryptEncrypt::Initialize()
{
	HRESULT hr;
	BOOL success = CryptAcquireContext(
		&m_ProvHandle, NULL, m_ProvName, m_ProvType, CRYPT_VERIFYCONTEXT);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	success = CryptCreateHash(
		m_ProvHandle, CALG_MD5, 0, 0, &m_HashHandle);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		CryptReleaseContext(m_ProvHandle, 0);
		return hr;
	}

	return S_OK;
}

inline HRESULT CCryptEncrypt::SetKey(const BYTE* Data, DWORD DataLength, DWORD Flags)
{
	HRESULT hr;
	BOOL success = CryptHashData(
		m_HashHandle, Data, DataLength, 0);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	success = CryptDeriveKey(
		m_ProvHandle, m_Algorithm, m_HashHandle, Flags, &m_KeyHandle);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		m_KeyHandle = NULL;
		return hr;
	}

	return S_OK;
}

inline HRESULT CCryptEncrypt::Encrypt(BYTE* Data, LPDWORD DataLength, DWORD DataBufferLength, BOOL Final)
{
	HRESULT hr;
	BOOL success = CryptEncrypt(
		m_KeyHandle, 0, Final, 0, Data, DataLength, DataBufferLength);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	return S_OK;
}

inline HRESULT CCryptEncrypt::Decrypt(BYTE* Data, LPDWORD DataLength, BOOL Final)
{
	HRESULT hr;
	BOOL success = CryptDecrypt(
		m_KeyHandle, 0, Final, 0, Data, DataLength);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	return S_OK;
}

#endif
