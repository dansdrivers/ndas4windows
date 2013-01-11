#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <ndas/ndasuser.h>
#include "ndascmd.h"

#define NDAS_MSG_DLL	_T("ndasmsg.dll")

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(a) (sizeof(a)/sizeof(a[0]))
#endif

/* Error Message Resolution Functions */

/* Custom Error Messages (NDAS Error Messages) can be retrieved
from ndasmsg.dll using FormatMessage function */

void NC_PrintNdasErrMsg(DWORD dwError)
{
	BOOL fSuccess;
	HLOCAL hLocal;
	HMODULE hModule = NULL;
	LPTSTR lpszErrorMessage = NULL;

	hModule = LoadLibraryEx(
		NDAS_MSG_DLL,
		NULL,
		LOAD_LIBRARY_AS_DATAFILE);

	if (NULL == hModule) {
		_tprintf(_T("NDAS Error (0x%08X).\n"), dwError);
		return;
	}

	fSuccess = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_HMODULE,
		hModule,
		dwError,
		0,
		(LPTSTR) &lpszErrorMessage,
		0,
		NULL);

	if (!fSuccess) 
	{
		_tprintf(_T("NDAS Error (0x%08X).\n"), dwError);

		fSuccess = FreeLibrary(hModule);
		_ASSERTE(fSuccess);

		return;
	}

	_tprintf(_T("NDAS Error (0x%08X): %s"), dwError, lpszErrorMessage);

	hLocal = LocalFree(lpszErrorMessage);
	_ASSERTE(NULL == hLocal);

	fSuccess = FreeLibrary(hModule);
	_ASSERTE(fSuccess);

	return;
}

void NC_PrintSysErrMsg(DWORD dwError)
{
	BOOL fSuccess;
	HLOCAL hLocal;
	LPTSTR lpszErrorMessage = NULL;

	fSuccess = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, 
		dwError, 
		0, 
		(LPTSTR) &lpszErrorMessage, 
		0, 
		NULL);

	if (!fSuccess) 
	{
		_tprintf(_T("System Error (0x%08X)\n"), dwError);
		return;
	}

	_tprintf(_T("System Error (0x%08X): %s"), dwError, lpszErrorMessage);

	hLocal = LocalFree(lpszErrorMessage);
	_ASSERTE(NULL == hLocal);
	return;
}

void NC_PrintErrMsg(DWORD dwError)
{
	if (dwError & APPLICATION_ERROR_MASK) 
	{
		NC_PrintNdasErrMsg(dwError);
	}
	else
	{
		NC_PrintSysErrMsg(dwError);
	}
}

void NC_PrintLastErrMsg()
{
	NC_PrintErrMsg(GetLastError());
}

BOOL
NC_GuidToString(LPTSTR lpBuffer, DWORD cchBuffer, LPCGUID lpGuid)
{
	HRESULT hr = StringCchPrintf(
		lpBuffer,
		cchBuffer,
		_T("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"),
		lpGuid->Data1,
		lpGuid->Data2,
		lpGuid->Data3,
		lpGuid->Data4[0], lpGuid->Data4[1],
		lpGuid->Data4[2], lpGuid->Data4[3],
		lpGuid->Data4[4], lpGuid->Data4[5],
		lpGuid->Data4[6], lpGuid->Data4[7]);
	return SUCCEEDED(hr);
}

BOOL
NC_BlockSizeToString(LPTSTR lpBuffer, DWORD cchBuffer, DWORD dwBlocks)
{
	static TCHAR* szSuffixes[] = {
		_T("KB"), _T("MB"), _T("GB"),
		_T("TB"), _T("PB"), _T("EB"),
		NULL };

	HRESULT hr;
	DWORD dwKB = dwBlocks / 2; // 1 BLOCK = 512 Bytes = 1/2 KB
	DWORD dwBase = dwKB;
	DWORD dwSub = 0;
	TCHAR ** ppszSuffix = szSuffixes; // KB

	while (dwBase > 1024 && NULL != *(ppszSuffix + 1)) {
		//
		// e.g. 1536 bytes
		// 1536 bytes % 1024 = 512 Bytes
		// 512 bytes * 1000 / 1024 = 500
		// 500 / 100 = 5 -> 0.5
		dwSub = MulDiv(dwBase % 1024, 1000, 1024) / 100;
		dwBase = dwBase / 1024;
		++ppszSuffix;
	}

	if (dwSub == 0) {
		hr = StringCchPrintf(
			lpBuffer,
			cchBuffer,
			_T("%d %s"),
			dwBase,
			*ppszSuffix);
		_ASSERTE(SUCCEEDED(hr));
	} else {
		hr = StringCchPrintf(
			lpBuffer,
			cchBuffer,
			_T("%d.%d %s"),
			dwBase,
			dwSub,
			*ppszSuffix);
		_ASSERTE(SUCCEEDED(hr));
	}

	return SUCCEEDED(hr);
}

BOOL
NC_NdasDeviceIDToString(LPTSTR lpBuffer, DWORD cchBuffer, LPCTSTR szDeviceID)
{
	HRESULT hr = StringCchPrintf(
		lpBuffer,
		cchBuffer,
		_T("%C%C%C%C%C-%C%C%C%C%C-%C%C%C%C%C-*****"),
		szDeviceID[0], szDeviceID[1], szDeviceID[2], szDeviceID[3], szDeviceID[4], 
		szDeviceID[5], szDeviceID[6], szDeviceID[7], szDeviceID[8],	szDeviceID[9], 
		szDeviceID[10], szDeviceID[11], szDeviceID[12], szDeviceID[13], szDeviceID[14]);
	return SUCCEEDED(hr);
}

/* Helper Functions */

BOOL NC_ParseSlotUnitNo(LPCTSTR arg, DWORD* slot_no_ptr, DWORD* unit_no_ptr)
{
	HRESULT hr;
	DWORD slot_no, unit_no;
	TCHAR* d_ptr = NULL; /* ptr to delimiter(:) */
	TCHAR arg_copy[30];

	/* slot[:unit] should be less than 30 characters */
	if (lstrlen(arg) >= RTL_NUMBER_OF(arg_copy)) return FALSE;

	hr = StringCchCopy(arg_copy, RTL_NUMBER_OF(arg_copy), arg);
	_ASSERTE(SUCCEEDED(hr));

	d_ptr = _tcschr(arg, _T(':'));

	if (NULL == d_ptr)
	{
		/* when there is no delimiter, it contains only a slot number
		and the unit no is 0. */
		slot_no = _ttoi(arg);

		/* slot number 0 is invalid slot number,
		and _ttoi also returns 0 for a non-numeric token */
		if (0 == slot_no) return FALSE;

		unit_no = 0;
	}
	else
	{
		/* Replace : to \0 */
		*d_ptr = _T('\0');

		/* Each arg_copy and d_ptr + 1 contains tokens */
		slot_no = _ttoi(arg_copy);
		unit_no = _ttoi(d_ptr + 1);

		/* slot number 0 is invalid slot number,
		and _ttoi also returns 0 for a non-numeric token */
		if (0 == slot_no) return FALSE;

		/* invalid unit no maps to zero, so nothing to do here */
	}

	*slot_no_ptr = slot_no;
	*unit_no_ptr = unit_no;

	return TRUE;
}

BOOL NC_ResolveLDIDFromSlotUnit(LPCTSTR arg, NDAS_LOGICALDEVICE_ID* ld_id_ptr)
{
	DWORD slot_no, unit_no;
	BOOL fSuccess;

	fSuccess = NC_ParseSlotUnitNo(arg, &slot_no, &unit_no);
	if (!fSuccess)
	{
		_tprintf(_T("Invalid slot (or unit) number format.\n"));
		return FALSE;
	}

	fSuccess = NdasFindLogicalDeviceOfUnitDevice(slot_no, unit_no, ld_id_ptr);
	if (!fSuccess)
	{
		return NC_PrintLastErrMsg(), FALSE;
	}

	return TRUE;
}

LPCTSTR
NC_UnitDeviceTypeString(NDAS_UNITDEVICE_TYPE type)
{
	switch (type) {
	case NDAS_UNITDEVICE_TYPE_DISK: return _T("Disk Drive");
	case NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK: return _T("CF Card Reader");
	case NDAS_UNITDEVICE_TYPE_CDROM: return _T("CD/DVD Drive");
	case NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY: return _T("MO Drive");
	default: return _T("Unknown Type");
	}
}
