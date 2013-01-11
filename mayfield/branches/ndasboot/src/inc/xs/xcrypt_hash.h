#pragma once
#ifndef _XCRYPT_HASH_
#define _XCRYPT_HASH_

#include <windows.h>

namespace xcrypt {

	struct IHashing
	{
		virtual BOOL Initialize() = 0;
		
		virtual BOOL HashData(
			CONST BYTE* pbData, 
			DWORD cbData) = 0;

		virtual LPBYTE GetHashValue(
			LPDWORD pcbHash = NULL) = 0;
	};

	IHashing* CreateCryptAPIMD5Hashing();

#ifdef XCRYPT_USE_GENERIC_MD5
	IHashing* CreateGenericMD5Hashing();
#endif

}

#endif

