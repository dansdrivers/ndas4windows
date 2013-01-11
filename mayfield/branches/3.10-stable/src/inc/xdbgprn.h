//
// Copyright (C) 2002-2004 XIMETA, Inc.
//
// Revision History:
//
// 08/22/2004 Chesong Lee <cslee@ximeta.com>
// Initial implementation
//
//

// If you do not want to use these functions inline 
// (and instead want to link w/ xdbgprn.lib), then
// #define DBGPRN_LIB before including this header file.
//
#if !defined(DBGPRN_LIB)
#ifndef _STRSAFE_H_INCLUDED_
#error inlined "xdbgprn.h" requires "strsafe.h" or use DBGPRN_LIB for library form
#endif
#endif // !DBGPRN_LIB

#ifndef _X_DBGPRINT_H_
#define _X_DBGPRINT_H_
#pragma once

#ifdef __cplusplus
#define _DBGPRN_EXTERN_C    extern "C"
#else
#define _DBGPRN_EXTERN_C    extern
#endif

#if defined(DBGPRN_LIB)
#define DBGPRNAPI  _DBGPRN_EXTERN_C void __stdcall
#pragma comment(lib, "xdbgprn.lib")
#elif defined(DBGPRN_LIB_IMPL)
#define DBGPRNAPI  _DBGPRN_EXTERN_C void __stdcall
#else
#define DBGPRNAPI  __inline void __stdcall
#define DBGPRN_INLINE
#endif

//
// Some functions always run inline 
//
#define DBGPRN_INLINE_API  __inline void __stdcall

// This should only be defined when we are building strsafe.lib
#ifdef DBGPRN_LIB_IMPL
#define DBGPRN_INLINE
#endif

#ifndef NO_DBGPRN

#define DebugPrintA			_DbgPrintLevelA
#define DebugPrintW			_DbgPrintLevelW
#define DebugPrintErrA		_DbgPrintErrA
#define DebugPrintErrW		_DbgPrintErrW
#define DebugPrintErrExA	_DbgPrintLastErrA
#define DebugPrintErrExW	_DbgPrintLastErrW
#define DebugPrintLastErrA	_DbgPrintLastErrA
#define DebugPrintLastErrW	_DbgPrintLastErrW

#else // NO_DBGPRN_

#define DebugPrintA			__noop
#define DebugPrintW			__noop
#define DebugPrintErrA		__noop
#define DebugPrintErrW		__noop
#define DebugPrintErrExA	__noop
#define DebugPrintErrExW	__noop
#define DebugPrintLastErrA	__noop
#define DebugPrintLastErrW	__noop

#endif // !NO_DBGPRN_

#ifdef UNICODE
#define DebugPrint DebugPrintW
#define DebugPrintErr DebugPrintErrW
#define DebugPrintErrEx DebugPrintErrExW
#define DebugPrintLastErr DebugPrintLastErrW
#else /* UNICODE */
#define DebugPrint DebugPrintA
#define DebugPrintErr DebugPrintErrA
#define DebugPrintErrEx DebugPrintErrExA
#define DebugPrintLastErr DebugPrintLastErrA
#endif /* UNICODE */

#ifndef DBGPRN_MAX_CHARS
#define DBGPRN_MAX_CHARS 256
#endif

#ifndef DBGPRN_DEFAULT_LEVEL
#ifdef _DEBUG
#define DBGPRN_DEFAULT_LEVEL 2
#else
#define DBGPRN_DEFAULT_LEVEL 0
#endif

#endif // DBGPRN_DEFAULT_LEVEL

#ifndef DBGPRN_ERROR_LEVEL
#define DBGPRN_ERROR_LEVEL 1
#endif // DBGPRN_ERROR_LEVEL

#if defined(DBGPRN_USE_EXTERN_LEVEL)
extern DWORD _DbgPrintLevel;
extern DWORD _DbgPrintErrorLevel;
#else // DBGPRN_EXTERN_LEVEL
static DWORD _DbgPrintLevel = DBGPRN_DEFAULT_LEVEL;
static DWORD _DbgPrintErrorLevel = DBGPRN_ERROR_LEVEL;
#endif // DBGPRN_EXTERN_LEVEL

#define DBGPRT_ERR(_l_,_expr_) \
	do { if ((_l_) <= _DbgPrintLevel) { \
		DebugPrintErr(_expr_); } } while(0)

#define DBGPRT(_l_,_expr_) \
	do { if ((_l_) <= _DbgPrintLevel) { \
		DebugPrint(_expr_); } } while(0)

#ifndef DBGPRN_INLINE
DBGPRNAPI _DbgPrintA(LPCSTR DebugMessage);
DBGPRNAPI _DbgPrintW(LPCWSTR DebugMessage);
DBGPRNAPI _DbgVPrintA(LPCSTR DebugMessage, va_list ap);
DBGPRNAPI _DbgVPrintW(LPCWSTR DebugMessage, va_list ap);
DBGPRNAPI _DbgVPrintErrA(DWORD ErrorCode, LPCSTR Format, va_list ap);
DBGPRNAPI _DbgVPrintErrW(DWORD ErrorCode, LPCWSTR Format, va_list ap);
#endif // DBGPRN_INLINE

// these functions are always inline
#ifndef DBGPRN_LIB_IMPL
DBGPRN_INLINE_API _DbgPrintLevelA(DWORD Level, LPCSTR Format, ...);
DBGPRN_INLINE_API _DbgPrintLevelW(DWORD Level, LPCWSTR Format, ...);
DBGPRN_INLINE_API _DbgPrintErrA(DWORD Error, LPCSTR Format, ...);
DBGPRN_INLINE_API _DbgPrintErrW(DWORD Error, LPCWSTR Format, ...);
DBGPRN_INLINE_API _DbgPrintLastErrA(LPCSTR Format, ...);
DBGPRN_INLINE_API _DbgPrintLastErrW(LPCWSTR Format, ...);
#endif 

//
// Inline implementations are exposed here.
//
#ifdef DBGPRN_INLINE
DBGPRNAPI _DbgPrintA(LPCSTR DebugMessage)
{
	OutputDebugStringA(DebugMessage);
}

DBGPRNAPI _DbgPrintW(LPCWSTR DebugMessage)
{
	OutputDebugStringW(DebugMessage);
}

DBGPRNAPI _DbgVPrintA(LPCSTR DebugMessage, va_list ap)
{
	DWORD err = GetLastError();
	CHAR szBuffer[DBGPRN_MAX_CHARS];
	HRESULT hr;
	hr = StringCchVPrintfA(szBuffer, DBGPRN_MAX_CHARS, DebugMessage, ap);
	if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		_DbgPrintA(szBuffer);
	}
	SetLastError(err);
}

DBGPRNAPI _DbgVPrintW(LPCWSTR DebugMessage, va_list ap)
{
	DWORD err = GetLastError();
	WCHAR szBuffer[DBGPRN_MAX_CHARS];
	HRESULT hr;
	hr = StringCchVPrintfW(szBuffer, DBGPRN_MAX_CHARS, DebugMessage, ap);
	if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		_DbgPrintW(szBuffer);
	}
	SetLastError(err);
}

DBGPRNAPI _DbgVPrintErrA(DWORD ErrorCode, LPCSTR Format, va_list ap)
{
	DWORD err = GetLastError();
	CHAR szBuffer[DBGPRN_MAX_CHARS];
	LPSTR pszNext = szBuffer;
	size_t cchRemaining = DBGPRN_MAX_CHARS;
	DWORD cch = 0;
	HRESULT hr;

	hr = StringCchVPrintfExA(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS, 
		Format, ap);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintA("FAILED_ON_DbgVPrintErr\n"); 
		return; 
	}

	hr = StringCchPrintfExA(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS,
		"Error %d (0x%08X) ", ErrorCode, ErrorCode);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintA("_DbgVPrintErr\n");
		return;
	}

	cch = FormatMessageA( 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pszNext,
		(DWORD)cchRemaining,
		NULL);

	if (0 == cch) {
		hr = StringCchCopyA(pszNext, cchRemaining, 
			"(no description available)\n");
		if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
			_DbgPrintA("_DbgVPrintErr\n");
			return;
		}
	} else {
		cchRemaining -= cch;
	}

	_DbgPrintA(szBuffer);
	SetLastError(err);
}

DBGPRNAPI _DbgVPrintErrW(DWORD ErrorCode, LPCWSTR Format, va_list ap)
{
	DWORD err = GetLastError();
	WCHAR szBuffer[DBGPRN_MAX_CHARS];
	LPWSTR pszNext = szBuffer;
	size_t cchRemaining = DBGPRN_MAX_CHARS;
	DWORD cch = 0;
	HRESULT hr;

	hr = StringCchVPrintfExW(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS, 
		Format, ap);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintW(L"FAILED_ON_DbgVPrintErr\n"); 
		return; 
	}

	hr = StringCchPrintfExW(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS,
		L"Error %d (0x%08X) ", ErrorCode, ErrorCode);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintW(L"_DbgVPrintErr\n");
		return;
	}

	cch = FormatMessageW( 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pszNext,
		(DWORD)cchRemaining,
		NULL);

	if (0 == cch) {
		hr = StringCchCopyW(pszNext, cchRemaining, 
			L"(no description available)\n");
		if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
			_DbgPrintW(L"_DbgVPrintErr\n");
			return;
		}
	} else {
		cchRemaining -= cch;
	}

	_DbgPrintW(szBuffer);
	SetLastError(err);
}
#endif // DBGPRN_INLINE

DBGPRN_INLINE_API _DbgPrintLevelA(ULONG Level, LPCSTR Format, ...)
{
	if(Level <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap,Format);
		_DbgVPrintA(Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintLevelW(ULONG Level, LPCWSTR Format, ...)
{
	if(Level <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap,Format);
		_DbgVPrintW(Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintErrA(DWORD Error, LPCSTR Format, ...)
{
	if (_DbgPrintErrorLevel <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap, Format);
		_DbgVPrintErrA(Error, Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintErrW(DWORD Error, LPCWSTR Format, ...)
{
	if (_DbgPrintErrorLevel <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap, Format);
		_DbgVPrintErrW(Error, Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintLastErrA(LPCSTR Format, ...)
{
	if (_DbgPrintErrorLevel <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap, Format);
		_DbgVPrintErrA(GetLastError(), Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintLastErrW(LPCWSTR Format, ...)
{
	if (_DbgPrintErrorLevel <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap, Format);
		_DbgVPrintErrW(GetLastError(), Format, ap);
		va_end(ap);
	}
}

#endif /* _X_DBGPRINT_H_ */
