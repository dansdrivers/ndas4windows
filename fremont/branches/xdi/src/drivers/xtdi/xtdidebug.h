#ifndef _XTDI_DEBUG_H_
#define _XTDI_DEBUG_H_
#pragma once

#ifdef RUN_WPP

#include <wpp/xtdi.wpp.h>

#define WPP_CONTROL_GUIDS \
	XTDI_WPP_CONTROL_GUIDS

#define WPP_LEVEL_FLAG_LOGGER(_Level,_Flags) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_LEVEL_FLAG_ENABLED(_Level,_Flags) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#define WPP_FLAG_LEVEL_LOGGER(_Flags,_Level) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_FLAG_LEVEL_ENABLED(_Flags,_Level) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#else /* RUN_WPP */

#ifndef TRACE_LEVEL_NONE
#define TRACE_LEVEL_NONE        0   // Tracing is not on
#define TRACE_LEVEL_FATAL       1   // Abnormal exit or termination
#define TRACE_LEVEL_ERROR       2   // Severe errors that need logging
#define TRACE_LEVEL_WARNING     3   // Warnings such as allocation failure
#define TRACE_LEVEL_INFORMATION 4   // Includes non-error cases(e.g.,Entry-Exit)
#define TRACE_LEVEL_VERBOSE     5   // Detailed traces from intermediate steps
#define TRACE_LEVEL_RESERVED6   6
#define TRACE_LEVEL_RESERVED7   7
#define TRACE_LEVEL_RESERVED8   8
#define TRACE_LEVEL_RESERVED9   9
#endif

#define XTDI_GENERAL 0x00000001

#define XTDI_DEFAULT_DEBUG_LEVEL 2
#define XTDI_DEFAULT_DEBUG_FLAGS 1

extern __declspec(selectany) ULONG xTdiDebugLevel = XTDI_DEFAULT_DEBUG_LEVEL;
extern __declspec(selectany) ULONG xTdiDebugFlags = XTDI_DEFAULT_DEBUG_FLAGS;

//
// This is not used when WPP is used. WPP is enabled by default
//
#if DBG
#include <stdarg.h>
#include <ntstrsafe.h>
FORCEINLINE
VOID xTdiTrace(ULONG Flag, ULONG Level, PCHAR Format, ...)
{
	UNREFERENCED_PARAMETER(Flag);
	if (Level <= xTdiDebugLevel)
	{
		CHAR buffer[512];
		va_list ap;
		va_start(ap, Format);
		RtlStringCchVPrintfA(buffer, 512, Format, ap);
		va_end(ap);
		DbgPrint(buffer);
	}
}
#else /* DBG */
#define xTdiTrace __noop
#endif /* DBG */

#endif /* RUN_WPP */
#endif /* _XTDI_DEBUG_H_ */
