#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <ndas/ndasid.h>

static BOOL CharToHex(TCHAR ch, LPBYTE pv);
static BOOL StringToHex(LPCTSTR pch, LPBYTE pv);
static BOOL StrToNdasDeviceID(LPCTSTR szString, NDAS_DEVICE_ID* deviceID);
static LPCTSTR NdasDeviceIDToString(const NDAS_DEVICE_ID* deviceID);
static LPCTSTR FormattedStringID(LPCTSTR sz);
static LPCTSTR UnformattedStringID(LPCTSTR sz);

int usage()
{
	_tprintf(_T("usage: ndasid [-addr <mac-address> | -id <id> [<writekey>] ]\n"));
	return 0;
}

int id_to_addr(int argc, TCHAR** argv)
{
	BOOL fSuccess;
	NDAS_DEVICE_ID deviceID;
	LPCTSTR lpszStringID = NULL;
	LPCTSTR lpszWriteKey = NULL;

	if (argc != 1 && argc != 2) return usage();

	lpszStringID = UnformattedStringID(argv[0]);
	lpszWriteKey = (argc == 2) ? argv[1] : NULL;

	if (!NdasIdValidate(lpszStringID, lpszWriteKey))
	{
		_tprintf(_T("Invalid NDAS ID (%u).\n"), GetLastError());
		return 2;
	}

	if (!NdasIdStringToDevice(lpszStringID, &deviceID))
	{
		_tprintf(_T("Conversion failed (%u).\n"), GetLastError());
		return 3;
	}

	_tprintf(_T("%s\n"), NdasDeviceIDToString(&deviceID));

	return 0;
}

int addr_to_id(int argc, TCHAR** argv)
{
	BOOL fSuccess;
	NDAS_DEVICE_ID deviceID;
	TCHAR szStrID[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	TCHAR szWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};

	if (argc != 1) return usage();
    	
	fSuccess = StrToNdasDeviceID(argv[0], &deviceID);
	if (!fSuccess)
	{
		_tprintf(_T("Invalid Address Format.\n"));
		return 2;
	}

	fSuccess = NdasIdDeviceToString(&deviceID, szStrID, szWriteKey);
	if (!fSuccess)
	{
		_tprintf(_T("Conversion failed (%u)\n"), GetLastError());
		return 3;
	}

	_tprintf(_T("%s %s\n"), FormattedStringID(szStrID), szWriteKey);

	return 0;
}


int __cdecl _tmain(int argc, TCHAR** argv)
{
	if (argc < 2) return usage();

	if (0 == lstrcmpi(_T("-id"), argv[1]))
		return id_to_addr(argc - 2, argv + 2);
	else if (0 == lstrcmpi(_T("-addr"), argv[1]))
		return addr_to_id(argc - 2, argv + 2);

	return usage();
}

BOOL 
CharToHex(TCHAR ch, LPBYTE pv)
{
	if (ch >= _T('0') && ch <= _T('9')) return *pv = ch - _T('0'), TRUE;
	else if (ch >= _T('a') && ch <= _T('f')) return *pv = ch - _T('a') + 0x0A, TRUE;
	else if (ch >= _T('A') && ch <= _T('F')) return *pv = ch - _T('A') + 0x0A, TRUE;
	else return FALSE;
}

LPCTSTR 
FormattedStringID(LPCTSTR sz)
{
	static TCHAR szBuf[NDAS_DEVICE_STRING_ID_LEN + NDAS_DEVICE_STRING_ID_PARTS + 1] = {0};
	StringCchPrintf(
		szBuf, 
		NDAS_DEVICE_STRING_ID_LEN + NDAS_DEVICE_STRING_ID_PARTS + 1,
		_T("%c%c%c%c%c-%c%c%c%c%c-%c%c%c%c%c-%c%c%c%c%c"),
		sz[0], sz[1], sz[2], sz[3], sz[4],
		sz[5], sz[6], sz[7], sz[8], sz[9],
		sz[10], sz[11], sz[12], sz[13], sz[14],
		sz[15], sz[16], sz[17], sz[18], sz[19]);
	return szBuf;
}

LPCTSTR 
UnformattedStringID(LPCTSTR sz)
{
	size_t i;
	static TCHAR szBuf[NDAS_DEVICE_STRING_ID_LEN + 1] = {0};
	LPCTSTR lpCur = sz;

	ZeroMemory(szBuf, sizeof(szBuf));

	for (i = 0; _T('\0') != lpCur && i < NDAS_DEVICE_STRING_ID_LEN; )
	{
		if (_T('-') == *lpCur || _T(':') == *lpCur) 
		{
			++lpCur;
		}
		else
		{
			szBuf[i++] = *lpCur++;
		}
	}
	return szBuf;
}

BOOL
StringToHex(LPCTSTR pch, LPBYTE pv)
{
	BYTE x, y;
	if (IsBadReadPtr(pch, sizeof(TCHAR) * 2)) return FALSE;
	if (CharToHex(*pch, &x) && CharToHex(*(pch+1), &y))
		return *pv = (x << 4) + y, TRUE;
	else
		return FALSE;
}

BOOL 
StrToNdasDeviceID(LPCTSTR szString, NDAS_DEVICE_ID* pDeviceID)
{
	BOOL fSuccess;
	size_t i = 0;
	NDAS_DEVICE_ID deviceID;
	LPCTSTR pch;

	for (pch = szString; *pch != _T('\0'); )
	{
		if (!StringToHex(pch, &deviceID.Node[i++])) return FALSE;
		pch += 2;
		if (_T(':') == *pch || _T('-') == *pch) ++pch;
	}
	*pDeviceID = deviceID;
	return TRUE;
}

LPCTSTR 
NdasDeviceIDToString(const NDAS_DEVICE_ID* pDeviceID)
{
	static TCHAR szBuf[30] = {0};

	StringCchPrintf(
		szBuf, 
		sizeof(szBuf) / sizeof(TCHAR),
		_T("%02X:%02X:%02X:%02X:%02X:%02X"),
		pDeviceID->Node[0], pDeviceID->Node[1], pDeviceID->Node[2], 
		pDeviceID->Node[3], pDeviceID->Node[4], pDeviceID->Node[5]);

	return szBuf;
}

