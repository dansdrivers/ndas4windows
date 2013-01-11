#include "stdafx.h"
#include "ndasctype.h"
#include "ndastype.h"
#include "ndasid.h"
#include "ndasidenc.h"

//////////////////////////////////////////////////////////////////////////
//
// keys for XIMETA NetDisk ID V1
//
//
// What're these macros if you ask? 
// These are obfuscators!!!, someone will answer!
//
//////////////////////////////////////////////////////////////////////////

#define NDIDV1Key1 xa08ef030a213
#define NDIDV1Key2 xc039401939fe
#define NDIDV1VID  xvi3019439492
#define NDIDV1Reserved  xrs30gj939294
#define NDIDV1Random  xd3d4499d0s01

const static BYTE NDIDV1Key1[8] = {0x45,0x32,0x56,0x2f,0xec,0x4a,0x38,0x53};
const static BYTE NDIDV1Key2[8] = {0x1e,0x4e,0x0f,0xeb,0x33,0x27,0x50,0xc1};
const static BYTE NDIDV1VID = 0x01;
const static BYTE NDIDV1Reserved[2] = { 0xff, 0xff };
const static BYTE NDIDV1Random = 0xcd;

BOOL ConvertStringIdToRealIdW(
	LPCWSTR lpszDeviceStringId,
	PNDAS_DEVICE_ID pDeviceId)
{
	_ASSERTE(!::IsBadStringPtrW(lpszDeviceStringId,NDAS_DEVICE_STRING_ID_LEN + 1));
	_ASSERTE(!::IsBadWritePtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));

	NDAS_ID_KEY_INFO info;
	::ZeroMemory(&info, sizeof(NDAS_ID_KEY_INFO));

	// check id length
	if (lpszDeviceStringId[NDAS_DEVICE_STRING_ID_LEN] != L'\0') {
		return FALSE;
	}

	// copy id
	for (DWORD i = 0; i < NDAS_DEVICE_STRING_ID_PARTS; ++i) {
		for (DWORD j = 0; j < NDAS_DEVICE_STRING_ID_PART_LEN; ++j) {
			info.serialNo[i][j] = static_cast<CHAR>(
				lpszDeviceStringId[i * NDAS_DEVICE_STRING_ID_PART_LEN + j]);
		}
	}

	// fill required keys
	::CopyMemory(info.key1, NDIDV1Key1, 8 * sizeof(CHAR));
	::CopyMemory(info.key2, NDIDV1Key2, 8 * sizeof(CHAR));

	// try decrypt
	if (!NdasIdKey_Decrypt(&info)) {
		return FALSE;
	}

	// fill the output
	::CopyMemory( pDeviceId->Node, info.address, sizeof(BYTE) * 6);

	return TRUE;
}

BOOL ConvertRealIdToStringIdW(
	const PNDAS_DEVICE_ID pDeviceId,
	LPWSTR lpszDeviceStringId)
{
	_ASSERTE(!::IsBadStringPtrW(lpszDeviceStringId,NDAS_DEVICE_STRING_ID_LEN + 1));
	_ASSERTE(!::IsBadWritePtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));

	NDAS_ID_KEY_INFO info;
	::ZeroMemory(&info, sizeof(NDAS_ID_KEY_INFO));

	info.random = NDIDV1Random;
	info.vid = NDIDV1VID;
	::CopyMemory(info.reserved, NDIDV1Reserved, sizeof(NDIDV1Reserved));
	::CopyMemory(info.address, pDeviceId, sizeof(BYTE) * 6);
	// fill required keys
	::CopyMemory(info.key1, NDIDV1Key1, 8 * sizeof(CHAR));
	::CopyMemory(info.key2, NDIDV1Key2, 8 * sizeof(CHAR));

	if (!NdasIdKey_Encrypt(&info)) {
		return FALSE;
	}

	// copy id
	for (DWORD i = 0; i < NDAS_DEVICE_STRING_ID_PARTS; ++i) {
		for (DWORD j = 0; j < NDAS_DEVICE_STRING_ID_PART_LEN; ++j) {
			lpszDeviceStringId[i * NDAS_DEVICE_STRING_ID_PART_LEN + j] =
				static_cast<WCHAR>(info.serialNo[i][j]);
		}
	}

	// terminate as null
	lpszDeviceStringId[NDAS_DEVICE_STRING_ID_LEN] = L'\0';

	return TRUE;
}

BOOL ValidateStringIdKeyW(
	LPCWSTR lpszDeviceStringId,
	LPCWSTR lpszDeviceStringKey,
	PBOOL pbWritable)
{
	NDAS_ID_KEY_INFO info;
	::ZeroMemory(&info, sizeof(NDAS_ID_KEY_INFO));

	// check id length
	if (lpszDeviceStringId[NDAS_DEVICE_STRING_ID_LEN] != L'\0') {
		return FALSE;
	}

	// copy id
	for (DWORD i = 0; i < NDAS_DEVICE_STRING_ID_PARTS; ++i) {
		for (DWORD j = 0; j < NDAS_DEVICE_STRING_ID_PART_LEN; ++j) {
			info.serialNo[i][j] = static_cast<CHAR>(lpszDeviceStringId[i * NDAS_DEVICE_STRING_ID_PART_LEN + j]);
		}
	}

	if (NULL != lpszDeviceStringKey) {

		// check key length
		if (lpszDeviceStringKey[NDAS_DEVICE_STRING_KEY_LEN] != L'\0') {
			return FALSE;
		}

		// copy key
		for (DWORD i = 0; i < NDAS_DEVICE_STRING_KEY_LEN; ++i) {
			info.writeKey[i] = static_cast<CHAR>(lpszDeviceStringKey[i]);
		}
	}

	::CopyMemory(info.key1, NDIDV1Key1, 8 * sizeof(CHAR));
	::CopyMemory(info.key2, NDIDV1Key2, 8 * sizeof(CHAR));

	//
	// Only string id will be validated on its return and
	// write key is validated by info.bWritable
	//
	BOOL fSuccess = NdasIdKey_Decrypt(&info);

	if (NULL == lpszDeviceStringKey) {
		if (pbWritable) *pbWritable = FALSE;
		return fSuccess;
	} else {
		if (fSuccess && info.writable) {
			if (pbWritable) *pbWritable = TRUE;
			return TRUE;
		} else {
			if (pbWritable) *pbWritable = FALSE;
			return FALSE;
		}
	}
}

BOOL ValidateStringIdKeyA(
	LPCSTR lpszDeviceStringId, LPCSTR lpszDeviceStringKey, PBOOL pbWritable)
{
	int iConverted(0);
	WCHAR wszDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	iConverted = ::MultiByteToWideChar(CP_ACP, 0, lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN + 1,
		wszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN + 1);
	if (0 == iConverted) {
		return FALSE;
	}

	if (NULL != lpszDeviceStringKey) {
		WCHAR wszDeviceStringKey[NDAS_DEVICE_STRING_KEY_LEN + 1];
		iConverted = ::MultiByteToWideChar(CP_ACP, 0, 
			lpszDeviceStringKey, NDAS_DEVICE_STRING_ID_LEN + 1,
			wszDeviceStringKey, NDAS_DEVICE_STRING_ID_LEN + 1);
		if (0 == iConverted) {
			return FALSE;
		}
		return ValidateStringIdKeyW(wszDeviceStringId, wszDeviceStringKey, pbWritable);
	} else {
		return ValidateStringIdKeyW(wszDeviceStringId, NULL, pbWritable);
	}

}

BOOL ConvertStringIdToRealIdA(
	LPCSTR lpszDeviceStringId,
	PNDAS_DEVICE_ID pDeviceId)
{
	WCHAR wszDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	int iConverted = ::MultiByteToWideChar(CP_ACP, 0, 
		lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN + 1,
		wszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN + 1);
	if (0 == iConverted) {
		return FALSE;
	}

	return ConvertStringIdToRealIdW(wszDeviceStringId, pDeviceId);
}

BOOL ConvertRealIdToStringIdA(
	const PNDAS_DEVICE_ID pDeviceId,
	LPSTR lpszDeviceStringId)
{
	WCHAR wszDeviceStringId[NDAS_DEVICE_STRING_ID_LEN + 1];
	BOOL fSuccess = ConvertRealIdToStringIdW(pDeviceId, wszDeviceStringId);
	if (!fSuccess) {
		return FALSE;
	}
	int iConverted = ::WideCharToMultiByte(
		CP_ACP, 0, wszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN + 1,
		lpszDeviceStringId, NDAS_DEVICE_STRING_ID_LEN + 1,
		NULL, NULL);
	if (0 == iConverted) {
		return FALSE;
	}
	return TRUE;
}