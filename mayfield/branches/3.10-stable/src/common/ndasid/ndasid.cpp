#include <windows.h>
#include <crtdbg.h>
#include <ndas/ndasmsg.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasidenc.h>
#include <ndas/ndasid.h>

//
// Compile-time assertion
//
#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif
#define C_ASSERT_SIZEOF(type, size) C_ASSERT(sizeof(type) == size)
#define C_ASSERT_EQUALSIZE(t1,t2) C_ASSERT(sizeof(t1) == sizeof(t2))

//
// Parameter validation macros
//

#define VALPARM_DEFAULT_ERR ERROR_INVALID_PARAMETER
#define VALPARMEX(pred, err) if (!(pred)) return ::SetLastError(err), FALSE;
#define VALPARM(pred) VALPARMEX(pred, VALPARM_DEFAULT_ERR)

//
// Inversion of IsBadXXXXPtrs for VALPARM
//

#define IsValidReadPtr(p,ucb) (!::IsBadReadPtr(p,ucb))
#define IsValidWritePtr(p,ucb) (!::IsBadWritePtr(p,ucb))
#define IsValidStringPtrA(lpsz,ucchMax) (!::IsBadStringPtrA(lpsz,ucchMax))
#define IsValidStringPtrW(lpsz,ucchMax) (!::IsBadStringPtrW(lpsz,ucchMax))
#define IsValidStringOutPtrA(lpsz, ucchMax) (!::IsBadWritePtr(lpsz, sizeof(CHAR) * (ucchMax)))
#define IsValidStringOutPtrW(lpsz, ucchMax) (!::IsBadWritePtr(lpsz, sizeof(WCHAR) * (ucchMax)))

//
// keys for XIMETA NetDisk ID V1
//

#define NDAS_DEFAULT_KEY1	{0x45,0x32,0x56,0x2f,0xec,0x4a,0x38,0x53}
#define NDAS_DEFAULT_KEY2	{0x1e,0x4e,0x0f,0xeb,0x33,0x27,0x50,0xc1}
#define NDAS_DEFAULT_VID	0x01
#define NDAS_DEFAULT_RANDOM 0xCD
#define NDAS_DEFUALT_RES1	0xFF
#define NDAS_DEFUALT_RES2	0xFF

//
// obfuscators
//

#define NDASID_EXT_KEY_DEFAULT	X_48804094944
#define NDASID_EXT_DATA_DEFAULT	X_00103040562

//
// default key definitions
//

static const NDASID_EXT_KEY 
	NDASID_EXT_KEY_DEFAULT = { NDAS_DEFAULT_KEY1, NDAS_DEFAULT_KEY2 };

static const NDASID_EXT_DATA
	NDASID_EXT_DATA_DEFAULT = { 
		NDAS_DEFAULT_RANDOM, 
		NDAS_DEFAULT_VID, 
		{ NDAS_DEFUALT_RES1, NDAS_DEFUALT_RES2 } };

// internal function
BOOL
WINAPI
pNdasIdStringToDeviceA(
	LPCSTR lpszStringId,
	LPCSTR lpszWriteKey,
	NDAS_DEVICE_ID& deviceID,
	const NDASID_EXT_KEY& extKey,
	NDASID_EXT_DATA& extData)
{
	// internal function validate parameters minimally.

	//
	// Decryption parameters:
	//
	// in: key1, key2, serialNo, writeKey
	// out: address, vid, random, reserved, writable
	//

	NDAS_ID_KEY_INFO ni = {0};

	//
	// input parameters
	//

	::CopyMemory(ni.serialNo, lpszStringId, sizeof(ni.serialNo));

	if (lpszWriteKey)
	{
		::CopyMemory(ni.writeKey, lpszWriteKey, sizeof(ni.writeKey));
	}

	C_ASSERT_EQUALSIZE(ni.key1, extKey.Key1);
	::CopyMemory(ni.key1, extKey.Key1, sizeof(ni.key1));

	C_ASSERT_EQUALSIZE(ni.key2, extKey.Key2);
	::CopyMemory(ni.key2, extKey.Key2, sizeof(ni.key2));

	//
	// Decryption
	//

	if (!NdasIdKey_Decrypt(&ni))
	{
		return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

	//
	// Is Write Key valid?
	//

	if (lpszWriteKey && !ni.writable)
	{
		return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

#define NDASID_USE_SINGLE_MAPPING
#ifdef NDASID_USE_SINGLE_MAPPING

	//
	// NDAS ID Encryption - Decryption Mapping Problem
	//
	// For a given, AAAA1-BBBB2-CCCC3-DDDD4,
	// other IDs which has incremented values of last digit of any part
	// are treated as same, e.g. AAAA1-BBBB3-CCCC4-DDDD4 generates the
	// same value as above. So is the write key.
	//
	// To prevent such anomalies, we should regenerate a string ID with
	// a generated device ID and re-compare to test a validity of the
	// given string ID.
	//
	// This means, even though there can be more than one string IDs
	// which map to a device ID, we ratify only a single ID,
	// that is generated from the device ID.
	//

	{

		NDAS_DEVICE_ID compDeviceID;
		NDASID_EXT_DATA compExtData;

		C_ASSERT_EQUALSIZE(compDeviceID.Node, ni.address);
		::CopyMemory(compDeviceID.Node, ni.address, sizeof(ni.address));

		compExtData.Seed = ni.random;
		compExtData.VID = ni.vid;
		C_ASSERT_EQUALSIZE(compExtData.Reserved, ni.reserved);
		::CopyMemory(compExtData.Reserved, ni.reserved, sizeof(ni.reserved));

		CHAR szNewStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
		CHAR szNewWriteKey[NDAS_DEVICE_STRING_KEY_LEN + 1] = {0};

		if (!NdasIdDeviceToStringExA(
			&compDeviceID, 
			szNewStringID, 
			szNewWriteKey, 
			&extKey, // constant
			&compExtData))
		{
			_ASSERTE(FALSE); // should not fail!
			return FALSE;
		}

		//
		// We should compare both case-insensitively!
		// NdasIdDeviceToStringExA always returns capital letters
		// but lpszStringId may have mixed cases.
		//
		if (0 != ::lstrcmpiA(szNewStringID, lpszStringId))
		{
			return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
		}

		// check write key also, if given
		if (lpszWriteKey && 0 != ::lstrcmpiA(szNewWriteKey, lpszWriteKey))
		{
			return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
		}
	}

#endif

	//
	// Copy outputs
	//

	C_ASSERT_EQUALSIZE(deviceID.Node, ni.address);
	::CopyMemory(deviceID.Node, ni.address, sizeof(ni.address));

	extData.Seed = ni.random;
	extData.VID = ni.vid;
	C_ASSERT_EQUALSIZE(ni.reserved, extData.Reserved);
	::CopyMemory(extData.Reserved, ni.reserved, sizeof(ni.reserved));

	return TRUE;
}

BOOL
WINAPI
NdasIdValidateExA(
	/* [in] */ LPCSTR lpszStringId,
	/* [in] */ LPCSTR lpszWriteKey,
	/* [in] */ const NDASID_EXT_KEY* pExtKey,
	/* [in] */ const NDASID_EXT_DATA* pExtData)
{
	VALPARM(IsValidStringPtrA(lpszStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(lpszStringId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(0 == lpszWriteKey || IsValidStringPtrA(lpszWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(0 == lpszWriteKey || lpszWriteKey[NDAS_DEVICE_STRING_KEY_LEN] == '\0');
	VALPARM(IsValidReadPtr(pExtKey, sizeof(NDASID_EXT_KEY)));
	VALPARM(IsValidReadPtr(pExtData, sizeof(NDASID_EXT_DATA)));

	NDAS_DEVICE_ID deviceID;
	NDASID_EXT_DATA decryptedExtData;
	
	if (!pNdasIdStringToDeviceA(lpszStringId, lpszWriteKey, deviceID, 
		*pExtKey, decryptedExtData))
	{
		return FALSE;
	}

	// VID, SEED, RANDOM check
	//
	// Don't do this. This may affected by the alignment. 
	// (not this case though)
	//
	// ::memcmp(&extData, &NDASID_EXT_KEY_DEFAULT, sizeof(NDASID_EXT_DATA))
	// 

	if (decryptedExtData.Seed != pExtData->Seed ||
		decryptedExtData.VID != pExtData->VID ||
		decryptedExtData.Reserved[0] != pExtData->Reserved[0] ||
		decryptedExtData.Reserved[1] != pExtData->Reserved[1])
	{
		return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdStringToDeviceExA(
	/* [in]  */ LPCSTR lpszStringId,
	/* [out] */ NDAS_DEVICE_ID* pDeviceId,
	/* [in]  */ const NDASID_EXT_KEY* pExtKey,
	/* [out] */ NDASID_EXT_DATA* pExtData)
{
	VALPARM(IsValidStringPtrA(lpszStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(lpszStringId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(IsValidWritePtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));
	VALPARM(IsValidReadPtr(pExtKey, sizeof(NDASID_EXT_KEY)));
	VALPARM(0 == pExtData || IsValidWritePtr(pExtData, sizeof(NDASID_EXT_DATA)));

	NDAS_DEVICE_ID deviceID;
	NDASID_EXT_DATA decryptedExtData;

	if (!pNdasIdStringToDeviceA(lpszStringId, NULL, deviceID,
		*pExtKey, decryptedExtData))
	{
		return FALSE;
	}

	//
	// Fill output data
	//

	::CopyMemory(pDeviceId, &deviceID, sizeof(NDAS_DEVICE_ID));

	if (NULL != pExtData) // optional
	{
		::CopyMemory(pExtData, &decryptedExtData, sizeof(NDASID_EXT_DATA));
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdDeviceToStringExA(
    /* [in]  */ const NDAS_DEVICE_ID* pDeviceId,
	/* [out] */ LPSTR lpszStringId,
	/* [out] */ LPSTR lpszWriteKey,
	/* [in]  */ const NDASID_EXT_KEY* pExtKey,
	/* [in]  */ const NDASID_EXT_DATA* pExtData)
{
	VALPARM(IsValidReadPtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));
	VALPARM(0 == lpszStringId || IsValidStringOutPtrA(lpszStringId, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(0 == lpszWriteKey || IsValidStringOutPtrA(lpszWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(IsValidReadPtr(pExtKey, sizeof(NDASID_EXT_KEY)));
	VALPARM(IsValidReadPtr(pExtData, sizeof(NDASID_EXT_DATA)));

	//
	// Encryption parameters:
	//
	// in: address, key1, key2, random, vid, reserved
	// out: serialNo, writeKey
	//

	NDAS_ID_KEY_INFO ni = {0};

	//
	// input parameters
	//

	C_ASSERT_EQUALSIZE(ni.address, pDeviceId->Node);
	::CopyMemory(ni.address, pDeviceId->Node, sizeof(ni.address));

	C_ASSERT_EQUALSIZE(ni.key1, pExtKey->Key1);
	::CopyMemory(ni.key1, pExtKey->Key1, sizeof(ni.key1));

	C_ASSERT_EQUALSIZE(ni.key2, pExtKey->Key2);
	::CopyMemory(ni.key2, pExtKey->Key2, sizeof(ni.key2));

	ni.random = pExtData->Seed;
	ni.vid = pExtData->VID;

	C_ASSERT_EQUALSIZE(ni.reserved, pExtData->Reserved);
	::CopyMemory(ni.reserved, pExtData->Reserved, sizeof(ni.reserved));

	if (!::NdasIdKey_Encrypt(&ni))
	{
		return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

	if (lpszStringId)
	{
		C_ASSERT_EQUALSIZE(ni.serialNo, CHAR[NDAS_DEVICE_STRING_ID_LEN]);
		::CopyMemory(lpszStringId, ni.serialNo, sizeof(ni.serialNo));
		lpszStringId[NDAS_DEVICE_STRING_ID_LEN] = '\0';
	}

	if (lpszWriteKey)
	{
		C_ASSERT_EQUALSIZE(ni.writeKey, CHAR[NDAS_DEVICE_STRING_KEY_LEN]);
		::CopyMemory(lpszWriteKey, ni.writeKey, sizeof(ni.writeKey));
		lpszWriteKey[NDAS_DEVICE_STRING_KEY_LEN] = '\0';
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdValidateA(
	/* [in]  */ LPCSTR lpszStringId,
	/* [in]  */ LPCSTR lpszWriteKey)
{
	return NdasIdValidateExA(lpszStringId, lpszWriteKey, 
		&NDASID_EXT_KEY_DEFAULT, &NDASID_EXT_DATA_DEFAULT);
}

BOOL
WINAPI
NdasIdStringToDeviceA(
	/* [in]  */ LPCSTR lpszStringId,
	/* [out] */ NDAS_DEVICE_ID* pDeviceId)
{
	//
	// parameter check is done by NdasIdStringToDeviceExA
	//

	NDASID_EXT_DATA extData;

	//
	// Conversion from string ID to device ID is not enough.
	// We should also check if the string ID is generated
	// from the same EXT_DATA
	//

	BOOL fValid = NdasIdStringToDeviceExA(
		lpszStringId, pDeviceId,
		&NDASID_EXT_KEY_DEFAULT, &extData);

	if (!fValid) return FALSE;

	//
	// VID, SEED, RANDOM check
	//
	// Don't do this. This may affected by the alignment. 
	// (not this case though)
	//
	// ::memcmp(&extData, &NDASID_EXT_KEY_DEFAULT, sizeof(NDASID_EXT_DATA))
	// 

	if (extData.Seed != NDASID_EXT_DATA_DEFAULT.Seed ||
		extData.VID != NDASID_EXT_DATA_DEFAULT.VID ||
		extData.Reserved[0] != NDASID_EXT_DATA_DEFAULT.Reserved[0] ||
		extData.Reserved[1] != NDASID_EXT_DATA_DEFAULT.Reserved[1])
	{
		return ::SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdDeviceToStringA(
    /* [in]  */ const NDAS_DEVICE_ID* pDeviceId,
	/* [out] */ LPSTR lpszStringId,
	/* [out] */ LPSTR lpszWriteKey)
{
	return NdasIdDeviceToStringExA(
		pDeviceId, lpszStringId, lpszWriteKey,
		&NDASID_EXT_KEY_DEFAULT, &NDASID_EXT_DATA_DEFAULT);
}

//////////////////////////////////////////////////////////////////////////
//
// Unicode Versions
//
//////////////////////////////////////////////////////////////////////////

BOOL
WINAPI
pNdasIdToUnicode(
	LPCSTR szStringId, 
	LPCSTR szWriteKey,
	LPWSTR lpwszStringId /* nullable */, 
	LPWSTR lpwszWriteKey /* nullable */)
{
	BOOL fSuccess;

	if (lpwszStringId)
	{
		fSuccess = ::MultiByteToWideChar(
			CP_ACP, 0,
			szStringId, NDAS_DEVICE_STRING_ID_LEN,
			lpwszStringId, NDAS_DEVICE_STRING_ID_LEN + 1);

		if (!fSuccess) return FALSE;

		lpwszStringId[NDAS_DEVICE_STRING_ID_LEN] = L'\0';
	}

	if (lpwszWriteKey)
	{
		fSuccess = ::MultiByteToWideChar(
			CP_ACP, 0,
			szWriteKey, NDAS_DEVICE_STRING_KEY_LEN,
			lpwszWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1);

		if (!fSuccess) return FALSE;

		lpwszWriteKey[NDAS_DEVICE_STRING_KEY_LEN] = L'\0';
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdValidateW(
	/* [in]  */ LPCWSTR szStringId,
	/* [in]  */ LPCWSTR szWriteKey)
{
	VALPARM(IsValidStringPtrW(szStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(szStringId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(0 == szWriteKey || IsValidStringPtrW(szWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(0 == szWriteKey || szWriteKey[NDAS_DEVICE_STRING_KEY_LEN] == '\0');

	CHAR mbszStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	for (size_t i = 0; i < NDAS_DEVICE_STRING_ID_LEN + 1; ++i)
	{
		mbszStringID[i] = static_cast<CHAR>(szStringId[i]);
	}

	CHAR mbszWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};
	if (szWriteKey)
	{
		for (size_t i = 0; i < NDAS_DEVICE_WRITE_KEY_LEN + 1; ++i)
		{
			mbszWriteKey[i] = static_cast<CHAR>(szWriteKey[i]);
		}
	}

	return NdasIdValidateA(mbszStringID, szWriteKey ? mbszWriteKey : NULL);
}

BOOL
WINAPI
NdasIdValidateExW(
	/* [in]  */ LPCWSTR szStringId,
	/* [in]  */ LPCWSTR szWriteKey,
	/* [in]  */ const NDASID_EXT_KEY* pExtKey,
	/* [in]  */ const NDASID_EXT_DATA* pExtData)
{
	VALPARM(IsValidStringPtrW(szStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(szStringId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(0 == szWriteKey || IsValidStringPtrW(szWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(0 == szWriteKey || szWriteKey[NDAS_DEVICE_STRING_KEY_LEN] == '\0');

	CHAR mbszStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	for (size_t i = 0; i < NDAS_DEVICE_STRING_ID_LEN + 1; ++i)
	{
		mbszStringID[i] = static_cast<CHAR>(szStringId[i]);
	}

	CHAR mbszWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};
	if (szWriteKey)
	{
		for (size_t i = 0; i < NDAS_DEVICE_WRITE_KEY_LEN + 1; ++i)
		{
			mbszWriteKey[i] = static_cast<CHAR>(szWriteKey[i]);
		}
	}

	return NdasIdValidateExA(
		mbszStringID, 
		szWriteKey ? mbszWriteKey : NULL,
		pExtKey, 
		pExtData);
}

BOOL
WINAPI
NdasIdStringToDeviceW(
	/* [in]  */ LPCWSTR szStringId,
	/* [out] */ NDAS_DEVICE_ID* pDeviceId)
{
	VALPARM(IsValidStringPtrW(szStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(szStringId[NDAS_DEVICE_STRING_ID_LEN] == '\0');

	CHAR mbszStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	for (size_t i = 0; i < NDAS_DEVICE_STRING_ID_LEN + 1; ++i)
	{
		mbszStringID[i] = static_cast<CHAR>(szStringId[i]);
	}

	return NdasIdStringToDeviceA(mbszStringID, pDeviceId);
}

BOOL
WINAPI
NdasIdStringToDeviceExW(
	/* [in]  */ LPCWSTR szStringId,
	/* [out] */ NDAS_DEVICE_ID* pDeviceId,
	/* [in]  */ const NDASID_EXT_KEY* pExtKey,
	/* [out] */ NDASID_EXT_DATA* pExtData)
{
	VALPARM(IsValidStringPtrW(szStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(szStringId[NDAS_DEVICE_STRING_ID_LEN] == '\0');

	CHAR mbszStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	for (size_t i = 0; i < NDAS_DEVICE_STRING_ID_LEN + 1; ++i)
	{
		mbszStringID[i] = static_cast<CHAR>(szStringId[i]);
	}

	return NdasIdStringToDeviceExA(mbszStringID, pDeviceId, pExtKey, pExtData);
}

BOOL
WINAPI
NdasIdDeviceToStringW(
    /* [in]  */ const NDAS_DEVICE_ID* pDeviceId,
	/* [out] */ LPWSTR lpszStringId,
	/* [out] */ LPWSTR lpszWriteKey)
{
	VALPARM(0 == lpszStringId || IsValidStringOutPtrW(lpszStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(0 == lpszWriteKey || IsValidStringOutPtrW(lpszWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));

	CHAR mbszStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	CHAR mbszWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};

	if (!NdasIdDeviceToStringA(
			pDeviceId, 
			lpszStringId ? mbszStringID : NULL, 
			lpszWriteKey ? mbszWriteKey : NULL))
	{
		return FALSE;
	}

	return pNdasIdToUnicode(mbszStringID, mbszWriteKey, lpszStringId, lpszWriteKey);
}

BOOL
WINAPI
NdasIdDeviceToStringExW(
    /* [in]  */ const NDAS_DEVICE_ID* pDeviceId,
	/* [out] */ LPWSTR lpszStringId,
	/* [out] */ LPWSTR lpszWriteKey,
	/* [in]  */ const NDASID_EXT_KEY* pExtKey,
	/* [in]  */ const NDASID_EXT_DATA* pExtData)
{
	VALPARM(0 == lpszStringId || IsValidStringOutPtrW(lpszStringId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(0 == lpszWriteKey || IsValidStringOutPtrW(lpszWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));

	CHAR mbszStringID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	CHAR mbszWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};

	if (!NdasIdDeviceToStringExA(
		pDeviceId, 
		lpszStringId ? mbszStringID : NULL, 
		lpszWriteKey ? mbszWriteKey : NULL,
		pExtKey, pExtData))
	{
		return FALSE;
	}

	return pNdasIdToUnicode(mbszStringID, mbszWriteKey, lpszStringId, lpszWriteKey);
}

