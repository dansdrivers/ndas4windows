#pragma once

#if defined(_DEBUG)
#define _pxTraceExW OutputDebugStringW
#define pxTraceExW  _pxTraceExW
#define pxTraceW    _pxTraceW
#else /* _DEBUG */
#define pxTraceExW  __noop
#define pxTraceW    __noop
#endif /* _DEBUG */

inline void pxMsiTraceExW(MSIHANDLE hInstall, LPCWSTR Message);
inline void pxMsiTraceW(MSIHANDLE hInstall, LPCWSTR Format, ...);
inline void _pxTraceW(LPCWSTR Format, ...);
inline void _pxTraceExW(LPCWSTR Message);

inline void pxMsiTraceExW(MSIHANDLE hInstall, LPWSTR Message)
{
	pxTraceExW(Message);

	//
	// Strip \n
	//
	int len = lstrlen(Message);
	if (len > 0 && Message[len - 1] == L'\n')
	{
		Message[len - 1] = L'\0';
	}

	LPCWSTR msiFormat = L"xDiMsi[1]: [2]";

	PMSIHANDLE hRecord = MsiCreateRecord(3);
	if (NULL == hRecord) return;

	WCHAR prefix[32];
	StringCchPrintfW(prefix, RTL_NUMBER_OF(prefix), 
		L" (%c) (%02x:%02x)",
		MsiGetMode(hInstall, MSIRUNMODE_SCHEDULED) ? L's' : L'c',
		GetCurrentProcessId(),
		GetCurrentThreadId());

	MsiRecordSetStringW(hRecord, 0, msiFormat);
	MsiRecordSetStringW(hRecord, 1, prefix);
	MsiRecordSetStringW(hRecord, 2, Message);

	MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);
}

inline void pxMsiTraceW(MSIHANDLE hInstall, LPCWSTR Format, ...)
{
	WCHAR message[256];
	va_list ap;
	va_start(ap, Format);
	/* we deliberately ignore the truncation error */
	StringCchVPrintfW(message, RTL_NUMBER_OF(message), Format, ap);
	va_end(ap);

	pxMsiTraceExW(hInstall, message);
}

inline void _pxTraceW(LPCWSTR Format, ...)
{
	WCHAR message[256];
	va_list ap;
	va_start(ap, Format);
	StringCchVPrintfW(message, RTL_NUMBER_OF(message), Format, ap);
	va_end(ap);
	pxTraceExW(message);
}

