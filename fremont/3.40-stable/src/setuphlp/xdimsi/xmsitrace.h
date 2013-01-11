#pragma once
#include <xtl/xtltrace.h>

inline void pxMsiTraceW(MSIHANDLE hInstall, LPCWSTR Format, ...);
inline void pxMsiTraceExW(MSIHANDLE hInstall, LPCWSTR Message);

inline void pxMsiTrace1W(MSIHANDLE hInstall, LPWSTR Message)
{
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

inline void pxMsiTraceVW(MSIHANDLE hInstall, LPCWSTR Format, va_list ap)
{
	WCHAR message[256];
	/* we deliberately ignore the truncation error */
	StringCchVPrintfW(message, RTL_NUMBER_OF(message), Format, ap);
	pxMsiTrace1W(hInstall, message);
}

inline void pxMsiTraceW(MSIHANDLE hInstall, LPCWSTR Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	pxMsiTraceVW(hInstall, Format, ap);
	va_end(ap);
}

class CxMsiTraceHandle 
{
#if _DEBUG
	XTL::CXtlTraceBufferA m_Trace;
#endif
public:
#if _DEBUG
	CxMsiTraceHandle(LPCSTR Function, LPCSTR File, DWORD Line) : 
	  m_Trace(Function, File, Line)
	{
	}
#else
	CxMsiTraceHandle()
	{
	}
#endif
	void TraceHandleVW(MSIHANDLE hInstall, LPCWSTR Format, va_list ap)
	{
#if _DEBUG
		m_Trace.SimpleTraceV(Format, ap);
#endif
		pxMsiTraceVW(hInstall, Format, ap);
	}
	void TraceHandleW(MSIHANDLE hInstall, LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		TraceHandleVW(hInstall, Format, ap);
		va_end(ap);
	}
};

void XMSITRACE(LPCWSTR Format, ...);
void XMSITRACEH(MSIHANDLE hInstall, LPCWSTR Format, ...);

#ifdef _DEBUG
#define XMSITRACE XTL::CXtlTraceBufferA(__FUNCTION__,__FILE__,__LINE__).SimpleTrace
#define XMSITRACEH CxMsiTraceHandle(__FUNCTION__, __FILE__, __LINE__).TraceHandleW
#else
#define XMSITRACE __noop
#define XMSITRACEH CxMsiTraceHandle().TraceHandleW
#endif

#pragma deprecated(pxMsiTrace1W)
#pragma deprecated(pxMsiTraceVW)
#pragma deprecated(pxMsiTraceW)

