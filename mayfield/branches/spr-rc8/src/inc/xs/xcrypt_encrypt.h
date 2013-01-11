#pragma once
#ifndef _XCRYPT_ENCRYPT_H_
#define _XCRYPT_ENCRYPT_H_

#include <windows.h>

#ifndef CRYPT_EXPORTABLE
#define CRYPT_EXPORTABLE        0x00000001
#endif

namespace xcrypt {

	struct IEncryption
	{
		virtual BOOL Initialize() = 0;

		virtual BOOL SetKey(
			CONST BYTE* pbData, 
			DWORD cbData, 
			DWORD dwFlags = CRYPT_EXPORTABLE) = 0;

		virtual BOOL Encrypt(
			BYTE* pbData,
			LPDWORD pcbData,
			DWORD cbDataBuf,
			BOOL bFinal = TRUE) = 0;

		virtual BOOL Decrypt(
			BYTE* pbData,
			LPDWORD pcbData,
			BOOL bFinal = TRUE) = 0;
	};

	IEncryption* CreateCryptAPIDESEncryption();
}

#endif
