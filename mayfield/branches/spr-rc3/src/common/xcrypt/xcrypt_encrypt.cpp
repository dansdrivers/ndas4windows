#include "stdafx.h"
#include "xs/xcrypt_encrypt.h"
#include <wincrypt.h>

namespace xcrypt {

class CCryptAPIEncrypt : public IEncryption
{
	CONST ALG_ID m_Algorithm;
	CONST LPCTSTR m_szCryptProvName;
	CONST DWORD m_dwCryptProvType;

	HCRYPTPROV m_hProv;
	HCRYPTHASH m_hHash;
	HCRYPTKEY m_hCryptKey;

public:

	CCryptAPIEncrypt(
		ALG_ID algorithm,
		LPCTSTR szCryptProvName,
		DWORD dwCryptProvType) :
		m_Algorithm(algorithm),
		m_szCryptProvName(szCryptProvName),
		m_dwCryptProvType(dwCryptProvType),
		m_hProv(NULL),
		m_hHash(NULL),
		m_hCryptKey(NULL)
	{
	}

	~CCryptAPIEncrypt()
	{
		if (NULL != m_hCryptKey) {
			BOOL fSuccess = ::CryptDestroyKey(m_hCryptKey);
			_ASSERTE(fSuccess);
		}

		if (NULL != m_hHash) {
			BOOL fSuccess = ::CryptDestroyHash(m_hHash);
			_ASSERTE(fSuccess);
		}

		if (NULL != m_hProv) {
			BOOL fSuccess = ::CryptReleaseContext(m_hProv, 0);
			_ASSERTE(fSuccess);
		}
	}

	BOOL Initialize()
	{
		BOOL fSuccess = ::CryptAcquireContext(
			&(HCRYPTPROV)m_hProv,
			NULL,
			m_szCryptProvName,
			m_dwCryptProvType,
			CRYPT_VERIFYCONTEXT);

		if (!fSuccess) {
			return FALSE;
		}

		fSuccess = ::CryptCreateHash(
			m_hProv, 
			CALG_MD5,
			0, 
			0, 
			&(HCRYPTHASH)m_hHash);

		if (!fSuccess) {
			return FALSE;
		}

		return TRUE;
	}

	BOOL SetKey(
		CONST BYTE* pbData, 
		DWORD cbData,
		DWORD dwFlags)
	{
		BOOL fSuccess = ::CryptHashData(
			m_hHash, 
			pbData, 
			cbData, 
			0);
		
		if (!fSuccess) {
			return FALSE;
		}

		fSuccess = ::CryptDeriveKey(
			m_hProv, 
			m_Algorithm, 
			m_hHash, 
			dwFlags, 
			&m_hCryptKey);
		
		if (!fSuccess) {
			m_hCryptKey = NULL;
			return FALSE;
		}

		return TRUE;
	}

	BOOL Encrypt(
		BYTE* pbData,
		LPDWORD pcbData,
		DWORD cbDataBuf,
		BOOL bFinal = TRUE)
	{
		return ::CryptEncrypt(
			m_hCryptKey,
			0,
			bFinal,
			0,
			pbData,
			pcbData,
			cbDataBuf);
	}

	BOOL Decrypt(
		BYTE* pbData,
		LPDWORD pcbData,
		BOOL bFinal = TRUE)
	{
		return ::CryptDecrypt(
			m_hCryptKey,
			0,
			bFinal,
			0,
			pbData,
			pcbData);
	}
};

IEncryption* CreateCryptAPIDESEncryption()
{
	return new CCryptAPIEncrypt(CALG_DES, MS_DEF_PROV, PROV_RSA_FULL);
}

}
