#include "precomp.h"
#include "ndascmd.h"

#define NDAS_MSG_DLL	_T("ndasmsg.dll")

/* Parse the Hex String into Byte Array.
 * Returns number of bytes stored in Value.
 */

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

/* Helper Functions */

BOOL
parse_slot_unit_number(LPCTSTR arg, DWORD* slot_no_ptr, DWORD* unit_no_ptr)
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

BOOL 
resolve_logicaldevice_id_from_slot_unit_number(LPCTSTR arg, NDAS_LOGICALDEVICE_ID* ld_id_ptr)
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


/* Convert first hex character into unsigned char.
 * Returns 0xFF if failed.
 */
__inline
UCHAR
single_hex_char_to_int(TCHAR ch)
{
	if (_T('0') <= ch && ch <= _T('9'))
	{
		return (UCHAR)(ch - _T('0')) + 0;
	}
	else if (_T('a') <= ch && ch <= _T('z'))
	{
		return (UCHAR)(ch - _T('a')) + 0xA;
	}
	else if (_T('A') <= ch && ch <= _T('Z'))
	{
		return (UCHAR)(ch - _T('A')) + 0xA;
	}
	else
	{
		return 0xFF;
	}
}

/* Convert the first two chars representing hex numbers into unsigned char.
 * Returns 0-255 if successful. Otherwise returns -1.
 */
__inline
INT
double_hex_chars_to_int(LPCTSTR lpstr)
{
	UCHAR h, l;
	h = single_hex_char_to_int(*lpstr);
	if (0xFF == h)
	{
		return -1;
	}
	l = single_hex_char_to_int(*(lpstr+1));
	if (0xFF == l)
	{
		return -1;
	}
	return ((h << 4) | l);
}

/* Convert the first two digit hex number into unsigned char.
 * Returns 0-65535 if successful. Otherwise returns -1.
 */
__inline
INT
ushort_chars_to_int(LPCTSTR lpstr)
{
	int v = 0;
	int i;
	for (i = 0; i < 5; ++i)
	{
		TCHAR ch = *(lpstr + i);
		if (_T('0') <= ch && ch <= _T('9'))
		{
			v = v * 10 + ((int)(ch - _T('0')) + 0);
		}
		else
		{
			return -1;
		}
		if (_T('\0') == *(lpstr + i + 1))
		{
			if (v > 0xFFFF)
			{
				/* overflow */
				return -1;
			}
			return v;
		}
	}
	/* overflow? */
	return -1;
}

/* Parse the Hex String into Byte Array.
 * Returns number of bytes stored in Value.
 */
DWORD
string_to_hex(
	LPCTSTR HexString,
	BYTE* Value,
	DWORD ValueLength)
{
	/* 000BD0AABBCC (without delimiter)          */
	/* 00:0B:D0:AA:BB:CC                         */
	/* 00-0B-D0-AA-BB-CC (alternative delimiter) */

	DWORD i;
	LPCTSTR pstr = HexString;
	for (i = 0; i < ValueLength; ++i)
	{
		int val = double_hex_chars_to_int(pstr);
		if (-1 == val)
		{
			return i;
		}

		/* consumed 2 chars */
		pstr += 2;

		_ASSERTE(0x00 <= val && val <= 0xFF);
		Value[i] = (BYTE) val;

		while (_T('-') == *pstr || _T(':') == *pstr)
		{
			/* skip delimiter if any */
			pstr += 1;
		}
	}

	return i;
}

/* Normalize device string ID by removing delimiters, up to
 * cchDeviceStringId characters. 
 *
 * This function returns the number of Device String ID characters put
 * in lpDeviceStringId.
 */

DWORD
normalize_device_string_id(
	LPTSTR lpDeviceStringId,
	DWORD cchDeviceStringId,
	LPCTSTR lpString)
{
	DWORD i = 0;
	LPCTSTR lp = lpString;

	/* we need 'cchDeviceStringId' characters excluding delimiters */
	for (i = 0; *lp != _T('\0') && i < cchDeviceStringId; ++i, ++lp)
	{
		if (i > 0 && i % 5 == 0)
		{
			/* '-' or ' ' are accepted as a delimiter */
			if (_T('-') == *lp ||
				_T(' ') == *lp)
			{
				++lp;
			}
		}

		if ((*lp >= _T('0') && *lp <= _T('9')) ||
			(*lp >= _T('a') && *lp <= _T('z')) ||
			(*lp >= _T('A') && *lp <= _T('Z')))
		{
			lpDeviceStringId[i] = *lp;
		}
		else
		{
			/* return as error */
			return i;
		}
	}

	return i;
}
