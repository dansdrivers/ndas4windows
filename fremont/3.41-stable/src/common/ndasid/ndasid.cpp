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
#define VALPARMEX(pred, err) if (!(pred)) return SetLastError(err), FALSE;
#define VALPARM(pred) VALPARMEX(pred, VALPARM_DEFAULT_ERR)

//
// Inversion of IsBadXXXXPtrs for VALPARM
//

#define IsValidReadPtr(p,ucb) (!IsBadReadPtr(p,ucb))
#define IsValidOptionalReadPtr(p,ucb) \
	((NULL == (p)) || IsValidReadPtr(p,ucb))
#define IsValidWritePtr(p,ucb) \
	(!IsBadWritePtr(p,ucb))
#define IsValidOptionalWritePtr(p,ucb) \
	((NULL == (p)) || IsValidWritePtr(p,ucb))
#define IsValidStringPtrA(lpsz,ucchMax) \
	(!IsBadStringPtrA(lpsz,ucchMax))
#define IsValidOptionalStringPtrA(lpsz,ucchMax) \
	((NULL == (lpsz)) || IsValidStringPtrA(lpsz,ucchMax))
#define IsValidStringPtrW(lpsz,ucchMax) \
	(!IsBadStringPtrW(lpsz,ucchMax))
#define IsValidOptionalStringPtrW(lpsz,ucchMax) \
	((NULL == (lpsz)) || IsValidStringPtrW(lpsz,ucchMax))
#define IsValidStringOutPtrA(lpsz, ucchMax) \
	(!IsBadWritePtr(lpsz, sizeof(CHAR) * (ucchMax)))
#define IsValidOptionalStringOutPtrA(lpsz, ucchMax) \
	((NULL == (lpsz)) || IsValidStringOutPtrA(lpsz,ucchMax))
#define IsValidStringOutPtrW(lpsz, ucchMax) \
	(!IsBadWritePtr(lpsz, sizeof(WCHAR) * (ucchMax)))
#define IsValidOptionalStringOutPtrW(lpsz, ucchMax) \
	((NULL == (lpsz)) || IsValidStringOutPtrW(lpsz,ucchMax))

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

static const NDASID_EXT_KEY NDASID_EXT_KEY_DEFAULT = {
	NDAS_DEFAULT_KEY1, NDAS_DEFAULT_KEY2 
};

static const NDASID_EXT_DATA NDASID_EXT_DATA_DEFAULT = { 
	NDAS_DEFAULT_RANDOM, NDAS_DEFAULT_VID, 
	NDAS_DEFUALT_RES1, NDAS_DEFUALT_RES2
};

// internal function
BOOL
WINAPI
pNdasIdStringToDeviceA(
	__in LPCSTR NdasId,
	__in_opt LPCSTR WriteKey,
	__out_opt NDAS_DEVICE_ID* DeviceID,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__out_opt NDASID_EXT_DATA* ExtData)
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

	if (NULL == ExtKey)
	{
		ExtKey = &NDASID_EXT_KEY_DEFAULT;
	}

	CopyMemory(ni.serialNo, NdasId, sizeof(ni.serialNo));

	if (NULL != WriteKey && 0 != WriteKey[0])
	{
		CopyMemory(ni.writeKey, WriteKey, sizeof(ni.writeKey));
	}

	C_ASSERT_EQUALSIZE(ni.key1, ExtKey->Key1);
	CopyMemory(ni.key1, ExtKey->Key1, sizeof(ni.key1));

	C_ASSERT_EQUALSIZE(ni.key2, ExtKey->Key2);
	CopyMemory(ni.key2, ExtKey->Key2, sizeof(ni.key2));

	//
	// Decryption
	//

	if (!NdasIdKey_Decrypt(&ni))
	{
		return SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

	//
	// Is Write Key valid?
	//

	if (WriteKey && WriteKey[0] && !ni.writable)
	{
		return SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
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
		CopyMemory(compDeviceID.Node, ni.address, sizeof(ni.address));

		compExtData.Seed = ni.random;
		compExtData.Vid = ni.vid;
		C_ASSERT_EQUALSIZE(compExtData.Reserved, ni.reserved);
		CopyMemory(compExtData.Reserved, ni.reserved, sizeof(ni.reserved));

		CHAR newNdasId[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
		CHAR newWriteKey[NDAS_DEVICE_STRING_KEY_LEN + 1] = {0};

		BOOL success = NdasIdDeviceToStringExA(
			&compDeviceID, 
			newNdasId, 
			newWriteKey, 
			ExtKey, // constant
			&compExtData);

		if (!success)
		{
			_ASSERTE(FALSE); // should not fail!
			return FALSE;
		}

		//
		// We should compare both case-insensitively!
		// NdasIdDeviceToStringExA always returns capital letters
		// but lpszStringId may have mixed cases.
		//
		if (0 != lstrcmpiA(newNdasId, NdasId))
		{
			return SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
		}

		// check write key also, if given
		if (WriteKey && 
			WriteKey [0] && 
			0 != lstrcmpiA(newWriteKey, WriteKey ))
		{
			return SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
		}
	}

#endif

	//
	// Copy outputs
	//

	if (NULL != DeviceID)
	{
		C_ASSERT_EQUALSIZE(DeviceID->Node, ni.address);
		CopyMemory(DeviceID->Node, ni.address, sizeof(ni.address));
	}

	if (NULL != ExtData)
	{
		ExtData->Seed = ni.random;
		ExtData->Vid = ni.vid;
		C_ASSERT_EQUALSIZE(ni.reserved, ExtData->Reserved);
		CopyMemory(ExtData->Reserved, ni.reserved, sizeof(ni.reserved));
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdValidateExA(
	__in LPCSTR NdasId,
	__in_opt LPCSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData)
{
	VALPARM(IsValidStringPtrA(NdasId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(NdasId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(IsValidOptionalStringPtrA(WriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(0 == WriteKey || WriteKey[NDAS_DEVICE_STRING_KEY_LEN] == '\0');
	VALPARM(IsValidOptionalReadPtr(ExtKey, sizeof(NDASID_EXT_KEY)));
	VALPARM(IsValidOptionalReadPtr(ExtData, sizeof(NDASID_EXT_DATA)));

	NDASID_EXT_DATA decryptedExtData;
	
	BOOL success = pNdasIdStringToDeviceA(
		NdasId, 
		WriteKey,
		NULL,
		ExtKey, 
		&decryptedExtData);

	if (!success)
	{
		return FALSE;
	}

	// VID, SEED, RANDOM check (only when ExtData is not null)
	//
	// Don't do this. This may affected by the alignment. 
	// (not this case though)
	//
	// memcmp(&extData, &NDASID_EXT_KEY_DEFAULT, sizeof(NDASID_EXT_DATA))
	// 

	if (NULL != ExtData)
	{
		if (decryptedExtData.Seed != ExtData->Seed ||
			decryptedExtData.Vid != ExtData->Vid ||
			decryptedExtData.Reserved[0] != ExtData->Reserved[0] ||
			decryptedExtData.Reserved[1] != ExtData->Reserved[1])
		{
			return SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
		}
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdStringToDeviceExA(
	__in LPCSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__out_opt NDASID_EXT_DATA* ExtData)
{
	VALPARM(IsValidStringPtrA(NdasId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(NdasId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(IsValidWritePtr(DeviceId, sizeof(NDAS_DEVICE_ID)));
	VALPARM(IsValidOptionalReadPtr(ExtKey, sizeof(NDASID_EXT_KEY)));
	VALPARM(IsValidOptionalWritePtr(ExtData, sizeof(NDASID_EXT_DATA)));

	if (!pNdasIdStringToDeviceA(NdasId, NULL, DeviceId, ExtKey, ExtData))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdDeviceToStringExA(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData)
{
	VALPARM(IsValidReadPtr(DeviceId, sizeof(NDAS_DEVICE_ID)));
	VALPARM(IsValidOptionalStringOutPtrA(NdasId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(IsValidOptionalStringOutPtrA(WriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(IsValidOptionalReadPtr(ExtKey, sizeof(NDASID_EXT_KEY)));
	VALPARM(IsValidOptionalReadPtr(ExtData, sizeof(NDASID_EXT_DATA)));

	//
	// Encryption parameters:
	//
	// in: address, key1, key2, random, vid, reserved
	// out: serialNo, writeKey
	//

	NDAS_ID_KEY_INFO ni = {0};

	if (NULL == ExtKey)
	{
		ExtKey = &NDASID_EXT_KEY_DEFAULT;
	}
	if (NULL == ExtData)
	{
		ExtData = &NDASID_EXT_DATA_DEFAULT;
	}

	//
	// input parameters
	//

	C_ASSERT_EQUALSIZE(ni.address, DeviceId->Node);
	CopyMemory(ni.address, DeviceId->Node, sizeof(ni.address));

	C_ASSERT_EQUALSIZE(ni.key1, ExtKey->Key1);
	CopyMemory(ni.key1, ExtKey->Key1, sizeof(ni.key1));

	C_ASSERT_EQUALSIZE(ni.key2, ExtKey->Key2);
	CopyMemory(ni.key2, ExtKey->Key2, sizeof(ni.key2));

	ni.random = ExtData->Seed;
	ni.vid = ExtData->Vid;

	C_ASSERT_EQUALSIZE(ni.reserved, ExtData->Reserved);
	CopyMemory(ni.reserved, ExtData->Reserved, sizeof(ni.reserved));

	if (!NdasIdKey_Encrypt(&ni))
	{
		return SetLastError(NDAS_ERROR_INVALID_ID_FORMAT), FALSE;
	}

	if (NdasId)
	{
		C_ASSERT_EQUALSIZE(ni.serialNo, CHAR[NDAS_DEVICE_STRING_ID_LEN]);
		CopyMemory(NdasId, ni.serialNo, sizeof(ni.serialNo));
		NdasId[NDAS_DEVICE_STRING_ID_LEN] = '\0';
	}

	if (WriteKey)
	{
		C_ASSERT_EQUALSIZE(ni.writeKey, CHAR[NDAS_DEVICE_STRING_KEY_LEN]);
		CopyMemory(WriteKey, ni.writeKey, sizeof(ni.writeKey));
		WriteKey[NDAS_DEVICE_STRING_KEY_LEN] = '\0';
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdValidateA(
	__in LPCSTR NdasId,
	__in_opt LPCSTR WriteKey)
{
	return NdasIdValidateExA(
		NdasId,
		WriteKey, 
		NULL,
		NULL);
}

BOOL
WINAPI
NdasIdStringToDeviceA(
	__in LPCSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId)
{
	return NdasIdStringToDeviceExA(
		NdasId, 
		DeviceId, 
		NULL, 
		NULL);
}

BOOL
WINAPI
NdasIdDeviceToStringA(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPSTR WriteKey)
{
	return NdasIdDeviceToStringExA(
		DeviceId, 
		NdasId, 
		WriteKey,
		NULL, 
		NULL);
}

//////////////////////////////////////////////////////////////////////////
//
// Unicode Versions
//
//////////////////////////////////////////////////////////////////////////

BOOL
WINAPI
pNdasIdToUnicode(
	__in LPCSTR AnsiNdasId, 
	__in LPCSTR AnsiWriteKey,
	__out_opt LPWSTR UnicodeStringId, 
	__out_opt LPWSTR UnicodeWriteKey)
{
	if (UnicodeStringId)
	{
		BOOL success = MultiByteToWideChar(
			CP_ACP, 0,
			AnsiNdasId, NDAS_DEVICE_STRING_ID_LEN,
			UnicodeStringId, NDAS_DEVICE_STRING_ID_LEN + 1);

		if (!success) return FALSE;

		UnicodeStringId[NDAS_DEVICE_STRING_ID_LEN] = L'\0';
	}

	if (UnicodeWriteKey)
	{
		BOOL success = MultiByteToWideChar(
			CP_ACP, 0,
			AnsiWriteKey, NDAS_DEVICE_STRING_KEY_LEN,
			UnicodeWriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1);

		if (!success) return FALSE;

		UnicodeWriteKey[NDAS_DEVICE_STRING_KEY_LEN] = L'\0';
	}

	return TRUE;
}

BOOL
WINAPI
NdasIdValidateW(
	__in LPCWSTR NdasId,
	__in_opt LPCWSTR WriteKey)
{
	return NdasIdValidateExW(NdasId, WriteKey, NULL, NULL);
}

BOOL
WINAPI
NdasIdValidateExW(
	__in LPCWSTR NdasId,
	__in_opt LPCWSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData)
{
	VALPARM(IsValidStringPtrW(NdasId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(NdasId[NDAS_DEVICE_STRING_ID_LEN] == '\0');
	VALPARM(IsValidOptionalStringPtrW(WriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));
	VALPARM(0 == WriteKey || WriteKey[NDAS_DEVICE_STRING_KEY_LEN] == '\0');

	CHAR ansiNdasId[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	for (size_t i = 0; i < NDAS_DEVICE_STRING_ID_LEN + 1; ++i)
	{
		ansiNdasId[i] = static_cast<CHAR>(NdasId[i]);
	}

	CHAR ansiWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};
	if (WriteKey)
	{
		for (size_t i = 0; i < NDAS_DEVICE_WRITE_KEY_LEN + 1; ++i)
		{
			ansiWriteKey[i] = static_cast<CHAR>(WriteKey[i]);
		}
	}

	return NdasIdValidateExA(
		ansiNdasId, 
		WriteKey ? ansiWriteKey : NULL,
		ExtKey, 
		ExtData);
}

BOOL
WINAPI
NdasIdStringToDeviceW(
	__in LPCWSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId)
{
	return NdasIdStringToDeviceExW(NdasId, DeviceId, NULL, NULL);
}

BOOL
WINAPI
NdasIdStringToDeviceExW(
	__in LPCWSTR NdasId,
	__out NDAS_DEVICE_ID* DeviceId,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__out_opt NDASID_EXT_DATA* ExtData)
{
	VALPARM(IsValidStringPtrW(NdasId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(NdasId[NDAS_DEVICE_STRING_ID_LEN] == '\0');

	CHAR ansiNdasId[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	for (size_t i = 0; i < NDAS_DEVICE_STRING_ID_LEN + 1; ++i)
	{
		ansiNdasId[i] = static_cast<CHAR>(NdasId[i]);
	}

	return NdasIdStringToDeviceExA(ansiNdasId, DeviceId, ExtKey, ExtData);
}

BOOL
WINAPI
NdasIdDeviceToStringW(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPWSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPWSTR WriteKey)
{
	return NdasIdDeviceToStringExW(DeviceId, NdasId, WriteKey, NULL, NULL);
}

BOOL
WINAPI
NdasIdDeviceToStringExW(
	__in const NDAS_DEVICE_ID* DeviceId,
	__out_ecount_opt(NDAS_DEVICE_STRING_ID_LEN+1) LPWSTR NdasId,
	__out_ecount_opt(NDAS_DEVICE_STRING_KEY_LEN+1) LPWSTR WriteKey,
	__in_opt const NDASID_EXT_KEY* ExtKey,
	__in_opt const NDASID_EXT_DATA* ExtData)
{
	VALPARM(IsValidOptionalStringOutPtrW(NdasId, NDAS_DEVICE_STRING_ID_LEN + 1));
	VALPARM(IsValidOptionalStringOutPtrW(WriteKey, NDAS_DEVICE_STRING_KEY_LEN + 1));

	CHAR ansiNdasId[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	CHAR ansiWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};

	BOOL success = NdasIdDeviceToStringExA(
		DeviceId, 
		NdasId ? ansiNdasId : NULL, 
		WriteKey ? ansiWriteKey : NULL,
		ExtKey, 
		ExtData);

	if (!success)
	{
		return FALSE;
	}

	return pNdasIdToUnicode(ansiNdasId, ansiWriteKey, NdasId, WriteKey);
}

