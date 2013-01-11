#pragma once
#include <windows.h>
#include <crtdbg.h>
#include <setupapi.h> 
#include <netcfgx.h>
#include <netcfgn.h>
#include <devguid.h>
#include <regstr.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlconv.h>
#include <strsafe.h>

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#include <xtl/xtltrace.h>

#if 0
#ifndef TRACE_LEVEL_INFORMATION
#define TRACE_LEVEL_ERROR 1
#define TRACE_LEVEL_WARNING 2
#define TRACE_LEVEL_INFORMATION 3
#endif

#if 0
void XTLTRACE1(LPCSTR Prefix, DWORD Level, LPCSTR Format, ...);
#endif

#ifndef  XTLTRACE1

__declspec(selectany) DWORD XtlTraceLevel = TRACE_LEVEL_INFORMATION;

inline void OutputDebugStringWithPrefixAV(LPCSTR Prefix, LPCSTR Format, va_list ap)
{
	CHAR buffer[256];
	PCHAR p = buffer;
	size_t r = RTL_NUMBER_OF(buffer);
	if (NULL != Prefix) 
	{
		StringCchCopyExA(p, r, Prefix, &p, &r, STRSAFE_IGNORE_NULLS);
	}
	StringCchVPrintfExA(p, r, &p, &r, STRSAFE_IGNORE_NULLS, Format, ap);
	OutputDebugStringA(buffer);
}

class CXtlTrace
{
	LPCSTR m_Prefix;
public:
	CXtlTrace(LPCSTR Prefix) : m_Prefix(Prefix)
	{
	}
	void TraceA(DWORD Level, LPCSTR Format, ...)
	{
		if (Level <= XtlTraceLevel)
		{
			va_list ap;
			va_start(ap, Format);
			OutputDebugStringWithPrefixAV(m_Prefix, Format, ap);
			va_end(ap);
		}
	}
};

#define XTLTRACE1 CXtlTrace("[xdi!" __FUNCTION__ "] ").TraceA

#endif
#endif // if 0
