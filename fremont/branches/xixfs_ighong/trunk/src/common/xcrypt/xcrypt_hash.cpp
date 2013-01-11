#include "stdafx.h"
#include "xs/xcrypt_hash.h"
#include <wincrypt.h>
#include "md5.h"

namespace xcrypt {

	class CCryptAPIMD5Hashing : public IHashing
	{
		HCRYPTPROV m_hProv;
		HCRYPTHASH m_hHash;
		BYTE m_hash[16];

	public:

		CCryptAPIMD5Hashing() :
			m_hProv(NULL),
			m_hHash(NULL)
		{
		}
		
		~CCryptAPIMD5Hashing()
		{
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
			//Get handle to the crypto provider
			BOOL fSuccess = ::CryptAcquireContext(
				&(HCRYPTPROV)m_hProv,
				NULL,
				NULL,
				PROV_RSA_FULL,
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

		BOOL HashData(CONST BYTE* pbData, DWORD cbData)
		{
			BOOL fSuccess = ::CryptHashData(
				m_hHash,
				pbData, 
				cbData, 
				0);

			if (!fSuccess) {
				return FALSE;
			}

			return TRUE;
		}

		LPBYTE GetHashValue(LPDWORD pcbHash = NULL)
		{
			DWORD cbHash = sizeof(m_hash);
			BOOL fSuccess = ::CryptGetHashParam(
				m_hHash, 
				HP_HASHVAL, 
				m_hash,
				&cbHash, 
				0);

			if (!fSuccess) {
				return FALSE;
			}

			if (NULL != pcbHash) {
				*pcbHash = cbHash;
			}

			return m_hash;
		}
	};

	class CGenericMD5Hashing : public IHashing
	{
		md5_context m_ctx;
		BYTE m_hash[16];
	public:

		CGenericMD5Hashing()
		{
			::ZeroMemory(&m_ctx, sizeof(m_ctx));
			::ZeroMemory(m_hash, sizeof(m_hash));
		}

		~CGenericMD5Hashing()
		{
		}

		BOOL Initialize()
		{
			md5_starts(&m_ctx);
			return TRUE;
		}

		BOOL HashData(CONST BYTE* pbData, DWORD cbData)
		{
			md5_update(&m_ctx, const_cast<uint8*>(pbData), cbData);
			return TRUE;
		}

		LPBYTE GetHashValue(LPDWORD pcbHash = NULL)
		{
			md5_finish(&m_ctx, m_hash);
			if (NULL != pcbHash) {
				*pcbHash = sizeof(m_hash);
			}
			return m_hash;
		}
	};

	IHashing* CreateCryptAPIMD5Hashing()
	{
		return new CCryptAPIMD5Hashing();
	}

	IHashing* CreateGenericMD5Hashing()
	{
		return new CGenericMD5Hashing();
	}
}
