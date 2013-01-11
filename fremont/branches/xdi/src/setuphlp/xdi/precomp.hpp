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
#include <strsafe.h>

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#ifndef TRACE_LEVEL_INFORMATION
#define TRACE_LEVEL_ERROR 1
#define TRACE_LEVEL_WARNING 2
#define TRACE_LEVEL_INFORMATION 3
#endif

#if 0
void XTLTRACE1(DWORD Level, LPCSTR Format, ...);
#endif

#ifndef  XTLTRACE1
#define XTLTRACE1 XtlTrace1

__declspec(selectany) DWORD XtlTraceLevel = TRACE_LEVEL_INFORMATION;

inline void OutputDebugStringAV(LPCSTR Format, va_list ap)
{
	CHAR buffer[256];
	StringCchVPrintfA(buffer, RTL_NUMBER_OF(buffer), Format, ap);
	OutputDebugStringA(buffer);
}

inline void XtlTrace1(DWORD Level, LPCSTR Format, ...)
{
	if (Level <= XtlTraceLevel)
	{
		va_list ap;
		va_start(ap, Format);
		OutputDebugStringAV(Format, ap);
		va_end(ap);
	}
}

#endif
