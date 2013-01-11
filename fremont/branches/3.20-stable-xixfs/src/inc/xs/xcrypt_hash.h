#pragma once
#ifndef _XCRYPT_HASH_
#define _XCRYPT_HASH_

#include <windows.h>
#include <wincrypt.h>

class CMD5Hash
{
public:
	CMD5Hash();
	~CMD5Hash();

	HRESULT Initialize();
	HRESULT HashData(const BYTE* Data, DWORD DataLength);
	HRESULT GetHashValue(LPBYTE* HashValue, LPDWORD HashValueLength = NULL);

private:
	HCRYPTPROV m_providerHandle;
	HCRYPTHASH m_hashHandle;
	BYTE m_hash[16];
};

inline CMD5Hash::CMD5Hash() :
	m_providerHandle(NULL), 
	m_hashHandle(NULL)
{
}

inline CMD5Hash::~CMD5Hash()
{
	if (NULL != m_hashHandle)
	{
		CryptDestroyHash(m_hashHandle);
	}
	if (NULL != m_providerHandle)
	{
		CryptReleaseContext(m_providerHandle, 0);
	}
}

inline HRESULT CMD5Hash::Initialize()
{
	HRESULT hr;
	BOOL success = CryptAcquireContext(
		&m_providerHandle,
		NULL,
		NULL,
		PROV_RSA_FULL,
		CRYPT_VERIFYCONTEXT);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	success = CryptCreateHash(
		m_providerHandle,
		CALG_MD5,
		0,
		0,
		&m_hashHandle);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		CryptReleaseContext(m_providerHandle, 0);
		return hr;
	}

	return S_OK;
}

inline HRESULT CMD5Hash::HashData(const BYTE* Data, DWORD DataLength)
{
	HRESULT hr;
	BOOL success = CryptHashData(m_hashHandle, Data, DataLength, 0);
	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}
	return S_OK;
}

inline HRESULT CMD5Hash::GetHashValue(LPBYTE* HashValue, LPDWORD HashValueLength)
{
	HRESULT hr;
	DWORD l = sizeof(m_hash);

	if (NULL != HashValue)
	{
		*HashValue = NULL;
	}

	if (NULL != HashValueLength)
	{
		*HashValueLength = 0;
	}

	BOOL success = CryptGetHashParam(
		m_hashHandle,
		HP_HASHVAL,
		m_hash,
		&l,
		0);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	*HashValue = &m_hash[0];

	if (NULL != HashValueLength)
	{
		*HashValueLength = l;
	}

	return S_OK;
}

#endif

