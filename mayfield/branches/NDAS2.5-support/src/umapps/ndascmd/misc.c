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

void print_ndas_error_message(DWORD dwError)
{
	BOOL success;
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

	success = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_HMODULE,
		hModule,
		dwError,
		0,
		(LPTSTR) &lpszErrorMessage,
		0,
		NULL);

	if (!success) 
	{
		_tprintf(_T("NDAS Error (0x%08X).\n"), dwError);

		success = FreeLibrary(hModule);
		_ASSERTE(success);

		return;
	}

	_tprintf(_T("NDAS Error (0x%08X): %s"), dwError, lpszErrorMessage);

	hLocal = LocalFree(lpszErrorMessage);
	_ASSERTE(NULL == hLocal);

	success = FreeLibrary(hModule);
	_ASSERTE(success);

	return;
}

void print_system_error_message(DWORD dwError)
{
	BOOL success;
	HLOCAL hLocal;
	LPTSTR lpszErrorMessage = NULL;

	success = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, 
		dwError, 
		0, 
		(LPTSTR) &lpszErrorMessage, 
		0, 
		NULL);

	if (!success) 
	{
		_tprintf(_T("System Error (0x%08X)\n"), dwError);
		return;
	}

	_tprintf(_T("System Error (0x%08X): %s"), dwError, lpszErrorMessage);

	hLocal = LocalFree(lpszErrorMessage);
	_ASSERTE(NULL == hLocal);
	return;
}

void print_error_message(DWORD dwError)
{
	if (dwError & APPLICATION_ERROR_MASK) 
	{
		print_ndas_error_message(dwError);
	}
	else
	{
		print_system_error_message(dwError);
	}
}

void print_last_error_message()
{
	print_error_message(GetLastError());
}

BOOL
convert_guid_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCGUID lpGuid)
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
convert_blocksize_to_string(LPTSTR lpBuffer, DWORD cchBuffer, DWORD dwBlocks)
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
conver_ndas_device_id_to_string(LPTSTR lpBuffer, DWORD cchBuffer, LPCTSTR szDeviceID)
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

BOOL parse_slot_unit_number(LPCTSTR arg, DWORD* slot_no_ptr, DWORD* unit_no_ptr)
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

BOOL resolve_logicaldevice_id_from_slot_unit_number(LPCTSTR arg, NDAS_LOGICALDEVICE_ID* ld_id_ptr)
{
	DWORD slot_no, unit_no;
	BOOL success;

	success = parse_slot_unit_number(arg, &slot_no, &unit_no);
	if (!success)
	{
		_tprintf(_T("Invalid slot (or unit) number format.\n"));
		return FALSE;
	}

	success = NdasFindLogicalDeviceOfUnitDevice(slot_no, unit_no, ld_id_ptr);
	if (!success)
	{
		return print_last_error_message(), FALSE;
	}

	return TRUE;
}

LPCTSTR get_unitdevice_type_string(NDAS_UNITDEVICE_TYPE type)
{
	switch (type) 
	{
	case NDAS_UNITDEVICE_TYPE_DISK: 
		return _T("Disk Drive");
	case NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK: 
		return _T("CF Card Reader");
	case NDAS_UNITDEVICE_TYPE_CDROM: 
		return _T("CD/DVD Drive");
	case NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY: 
		return _T("MO Drive");
	default: 
		return _T("Unknown Type");
	}
}

LPCTSTR get_veto_type_string(PNP_VETO_TYPE VetoType)
{
	LPCTSTR VetoTypeNames[] = {
		_T("Unknown"),
		_T("Legacy Device"),
		_T("Pending Close"),
		_T("Windows Application"),
		_T("Windows Service"),
		_T("Outstanding Open"),
		_T("Device"),
		_T("Driver"),
		_T("Illegal Device Request"),
		_T("Insufficient Power"),
		_T("Non Disableable"),
		_T("Legacy Driver"),
		_T("Insufficient Rights")
	};
	DWORD i = (DWORD) VetoType;
	if (i < RTL_NUMBER_OF(VetoTypeNames))
	{
		return VetoTypeNames[i];
	}
	else
	{
		return _T("Invalid Veto Type");
	}
}

