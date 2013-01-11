#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <crtdbg.h>

/* Convert first hex character into unsigned char.
 * Returns 0xFF if failed.
 */
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

/* Parse the MAC Address String into Byte Array.
 * Returns number of bytes stored in Address if successful.
 * If AddressString contains past AddressLength bytes, the returning number 
 * of bytes is AddressLength + 1.
 * If failed, returns 0.
 */
DWORD
string_to_byte_addr(
	LPCTSTR AddressString,
	BYTE* Address,
	DWORD AddressLength)
{
	/* 000BD0AABBCC (without delimiter)          */
	/* 00:0B:D0:AA:BB:CC                         */
	/* 00-0B-D0-AA-BB-CC (alternative delimiter) */

	DWORD i;
	LPCTSTR paddr = AddressString;
	for (i = 0; i < AddressLength; ++i)
	{
		int val = double_hex_chars_to_int(paddr);
		if (-1 == val)
		{
			return i;
		}

		/* consumed 2 chars */
		paddr += 2;

		_ASSERTE(0x00 <= val && val <= 0xFF);
		Address[i] = (BYTE) val;

		while (_T('-') == *paddr || _T(':') == *paddr)
		{
			/* skip delimiter if any */
			paddr += 1;
		}
	}

	if (_T('\0') == *paddr)
	{
		return i;
	}
	else
	{
		return i + 1;
	}
}

