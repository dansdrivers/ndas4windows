#pragma once

#ifndef _XTLTRACE_H_
#define _XTLTRACE_H_

#include "xtldef.h"
#include "xtlstr.h"
#include <strsafe.h>

namespace XTL
{

#if defined(_DEBUG) || defined(DBG)
#ifndef XTL_TRACE_CATEGORY
#define XTL_TRACE_CATEGORY 0xFFFFFFFF
#endif
#ifndef XTL_TRACE_LEVEL
#define XTL_TRACE_LEVEL 3
#endif
#else
#ifndef XTL_TRACE_CATEGORY
#define XTL_TRACE_CATEGORY 0x00000000
#endif
#ifndef XTL_TRACE_LEVEL
#define XTL_TRACE_LEVEL 0
#endif
#endif

#ifndef XTLTRACE_REMOVE_FILE_PATH
#define XTLTRACE_REMOVE_FILE_PATH 1
#endif

#ifndef XTLTRACE_CACHE_FILENAME
#define XTLTRACE_CACHE_FILENAME 0
#endif

#ifndef _T
#ifdef UNICODE
#define __T(x) L#x
#define _T(x) __T(x)
#else
#define __T(x) x
#define _T(x) __T(x)
#endif
#endif

struct IXtlTrace
{
	virtual void OutputDebugStringW(LPCWSTR lpOutputString, DWORD cchOutput = 0) = 0;
	virtual void OutputDebugStringA(LPCSTR lpOutputString, DWORD cchOutput = 0) = 0;
};

class CXtlDebuggerTrace : public IXtlTrace
{
public:
	CXtlDebuggerTrace() {}
	virtual VOID OutputDebugStringA(LPCSTR lpOutputString, DWORD cchOutput) throw()
	{
		UNREFERENCED_PARAMETER(cchOutput);
		::OutputDebugStringA(lpOutputString);
	}
	virtual VOID OutputDebugStringW(LPCWSTR lpOutputString, DWORD cchOutput) throw()
	{
		UNREFERENCED_PARAMETER(cchOutput);
		::OutputDebugStringW(lpOutputString);
	}
};

class CXtlFileTrace : public IXtlTrace
{
	HANDLE m_hFile;
public:
	CXtlFileTrace() throw() : m_hFile(INVALID_HANDLE_VALUE)
	{}
	CXtlFileTrace(HANDLE hFile) throw() : m_hFile(hFile)
	{}
	void SetFileHandle(HANDLE hFile) throw()
	{
		m_hFile = hFile;
	}
	HANDLE GetFileHandle() throw()
	{
		return m_hFile;
	}
	virtual void OutputDebugStringW(LPCWSTR lpOutputString, DWORD cchOutput) throw()
	{
		DWORD cbWritten;
		(void)::WriteFile(m_hFile, lpOutputString, cchOutput, &cbWritten, NULL);
	}
	virtual void OutputDebugStringA(LPCSTR lpOutputString, DWORD cchOutput) throw()
	{
		DWORD cbWritten;
		(void)::WriteFile(m_hFile, lpOutputString, cchOutput, &cbWritten, NULL);
	}
};

class CXtlConsoleTrace : public IXtlTrace
{
	bool m_enabled;
public:
	CXtlConsoleTrace() throw() : m_enabled(false)
	{
	}
	CXtlConsoleTrace& EnabledInstance()
	{
		//
		// just try to alloc a console for a GUI application
		// which has no pre-allocated console.
		// For console applications, this call is subject to fail.
		// However, we can still get standard handles if there is a console
		// whichever.
		//
		if (!m_enabled)
		{
			(void) ::AllocConsole();
			m_enabled = true;
		}
		return *this;
	}
	virtual void OutputDebugStringW(LPCWSTR lpOutputString, DWORD cchOutput) throw()
	{
		DWORD cchWritten;
		(void) ::WriteConsoleW(
			::GetStdHandle(STD_OUTPUT_HANDLE),
			lpOutputString, cchOutput, &cchWritten, NULL);
	}
	virtual void OutputDebugStringA(LPCSTR lpOutputString, DWORD cchOutput) throw()
	{
		DWORD cchWritten;
		(void) ::WriteConsoleA(
			::GetStdHandle(STD_OUTPUT_HANDLE),
			lpOutputString, cchOutput, &cchWritten, NULL);
	}
};

class CXtlTrace 
{
	static const DWORD MAX_CHARS = 512;
	static const DWORD MAX_OUTPUTS = 5;

public:

	DWORD TraceCategory;
	DWORD TraceLevel;

	CXtlTrace();
	~CXtlTrace();
	bool Attach(IXtlTrace* pOutput);
	bool Detach(IXtlTrace* pOutput);
	void CompactIXDbgOutput();
	void SendOutput(LPCSTR szOutputString, DWORD cchOutputString = -1);
	void SendOutput(LPCWSTR szOutputString, DWORD cchOutputString = -1);
	void Printf(DWORD dwLevel, LPCTSTR szFormat, ...);
	void VPrintf(DWORD Level, LPCSTR Format, va_list va);
	void VPrintf(DWORD dwLevel, LPCWSTR szFormat, va_list va);
	void VPrintfEx(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, LPCSTR Format, va_list va);
	void VPrintfEx(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, LPCWSTR Format, va_list va);
	void VPrintfExWithError(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, DWORD Error, LPCSTR Format, va_list va);
	void VPrintfExWithError(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, DWORD Error, LPCWSTR Format, va_list va);

private:

	DWORD m_nOutput;
	IXtlTrace* m_ppOutput[MAX_OUTPUTS];
	CRITICAL_SECTION m_cs;

};

inline
CXtlTrace::CXtlTrace() :
	TraceCategory(XTL_TRACE_CATEGORY),
	TraceLevel(XTL_TRACE_LEVEL),
	m_nOutput(0)
{
	::InitializeCriticalSection(&m_cs);
}

inline
CXtlTrace::~CXtlTrace()
{
	::DeleteCriticalSection(&m_cs);
}

inline
bool 
CXtlTrace::Attach(IXtlTrace* pOutput)
{
	::EnterCriticalSection(&m_cs);
	if (m_nOutput < MAX_OUTPUTS) 
	{
		for (DWORD i = 0; i < m_nOutput; ++i) 
		{
			if (m_ppOutput[i] == pOutput) 
			{
				::LeaveCriticalSection(&m_cs);
				return TRUE;
			}
		}
		m_ppOutput[m_nOutput] = pOutput;
		++m_nOutput;
		::LeaveCriticalSection(&m_cs);
		return TRUE;
	} 
	else 
	{
		for (DWORD i = 0; i < MAX_OUTPUTS; ++i) 
		{
			if (m_ppOutput[i] == NULL) 
			{
				m_ppOutput[i] = pOutput;
				::LeaveCriticalSection(&m_cs);
				return TRUE;
			}
		}
	}
	::LeaveCriticalSection(&m_cs);
	return FALSE;
}

inline
bool 
CXtlTrace::Detach(IXtlTrace* pOutput)
{
	::EnterCriticalSection(&m_cs);
	for (DWORD i = 0; i < m_nOutput; ++i) 
	{
		if (m_ppOutput[i] == pOutput) 
		{
			m_ppOutput[i] = NULL;
			CompactIXDbgOutput();
			::LeaveCriticalSection(&m_cs);
			return TRUE;
		}
	}
	::LeaveCriticalSection(&m_cs);
	return FALSE;
}

inline
void
CXtlTrace::CompactIXDbgOutput()
{
	DWORD emp = MAX_OUTPUTS;
	for (DWORD i = 0; i < MAX_OUTPUTS; ++i) 
	{
		if (NULL == m_ppOutput[i] && emp == MAX_OUTPUTS) 
		{
			emp = i;
		}
		else if (NULL != m_ppOutput[i] && emp < MAX_OUTPUTS) 
		{
			m_ppOutput[emp] = m_ppOutput[i];
			m_ppOutput[i] = NULL;
			DWORD j = emp + 1;
			while (j < MAX_OUTPUTS && NULL != m_ppOutput[j]) 
			{
				++j;
			}
			emp = j;
		}
	}
}

inline
void
CXtlTrace::SendOutput(LPCSTR szOutputString, DWORD cchOutputString)
{
	if (-1 == cchOutputString) 
	{
		size_t cch;
		XTLVERIFY(SUCCEEDED(::StringCchLengthA(szOutputString, MAX_CHARS, &cch)));
	}

	for (DWORD i = 0; i < m_nOutput; ++i) 
	{
		if (m_ppOutput[i]) 
		{
			m_ppOutput[i]->OutputDebugStringA(szOutputString, cchOutputString);
		}
	}
}

inline
void
CXtlTrace::SendOutput(LPCWSTR szOutputString, DWORD cchOutputString)
{
	if (-1 == cchOutputString) 
	{
		size_t cch;
		XTLVERIFY(SUCCEEDED(::StringCchLengthW(szOutputString, MAX_CHARS, &cch)));
	}

	for (DWORD i = 0; i < m_nOutput; ++i) 
	{
		if (m_ppOutput[i]) 
		{
			m_ppOutput[i]->OutputDebugStringW(szOutputString, cchOutputString);
		}
	}
}

inline
void
CXtlTrace::VPrintfEx(
	LPCSTR File, DWORD Line, DWORD /*Category*/, DWORD Level, 
	LPCSTR Format, va_list va)
{
	XTL_SAVE_LAST_ERROR();

	CHAR szBuffer[MAX_CHARS];
	size_t cchRemaining = MAX_CHARS;
	CHAR *pszNext = szBuffer;

	if (0 == File || 0 == Line)
	{
		HRESULT hr = ::StringCchPrintfExA(pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			"%X.%X %d:", 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}
	else
	{
		HRESULT hr = ::StringCchPrintfExA(pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			"%hs(%d) : %X.%X %d:", 
			File, Line, 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}

	HRESULT hr = ::StringCchVPrintfExA(pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		Format, va);
	XTLASSERT(SUCCEEDED(hr)); hr;
	
	SendOutput(szBuffer, MAX_CHARS - (DWORD)cchRemaining);
}

inline
void
CXtlTrace::VPrintfEx(
	LPCSTR File, DWORD Line, DWORD /*Category*/, DWORD Level, 
	LPCWSTR Format, va_list va)
{
	XTL_SAVE_LAST_ERROR();

	WCHAR szBuffer[MAX_CHARS];
	size_t cchRemaining = MAX_CHARS;
	WCHAR *pszNext = szBuffer;

	if (0 == File || 0 == Line)
	{
		HRESULT hr = ::StringCchPrintfExW(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			L"%X.%X %d:", 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}
	else
	{
		HRESULT hr = ::StringCchPrintfExW(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			L"%hs(%d) : %X.%X %d:", 
			File, Line, 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}

	HRESULT hr = ::StringCchVPrintfExW(
		pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		Format, va);
	XTLASSERT(SUCCEEDED(hr)); hr;
	
	SendOutput(szBuffer, MAX_CHARS - (DWORD)cchRemaining);
}

inline
void
CXtlTrace::VPrintfExWithError(
	LPCSTR File, DWORD Line, DWORD /*Category*/, DWORD Level, DWORD Error, 
	LPCSTR Format, va_list ap)
{
	XTL_SAVE_LAST_ERROR();

	CHAR szBuffer[MAX_CHARS];
	size_t cchRemaining = MAX_CHARS;
	CHAR *pszNext = szBuffer;

	if (0 == File || 0 == Line)
	{
		HRESULT hr = ::StringCchPrintfExA(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			"%X.%X %d:", 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}
	else
	{
		HRESULT hr = ::StringCchPrintfExA(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			"%hs(%d) : %X.%X %d:", 
			File, Line, 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}

	HRESULT hr = ::StringCchVPrintfExA(
		pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		Format, ap);
	XTLASSERT(SUCCEEDED(hr));

	if (Error & APPLICATION_ERROR_MASK) 
	{
		hr = ::StringCchPrintfExA(
			pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0, 
			"Error %u (0x%08X): (application error)\n", 
			Error, Error);
		XTLASSERT(SUCCEEDED(hr));
	}
	else 
	{
		hr = ::StringCchPrintfExA(pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0, 
			"Error %u (0x%08X):", Error, Error);
		XTLASSERT(SUCCEEDED(hr));
		DWORD cchUsed = ::FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, Error, 0, pszNext, (DWORD)cchRemaining, NULL);
		if (cchUsed > 0) 
		{
			pszNext += cchUsed;
			cchRemaining -= cchUsed;
		}
		else 
		{
			hr = ::StringCchPrintfExA(pszNext, cchRemaining,
				&pszNext, &cchRemaining, 0,
				"(n/a)\n");
			XTLASSERT(SUCCEEDED(hr));
		}
	}

	SendOutput(szBuffer, MAX_CHARS - (DWORD) cchRemaining);
}

inline
void
CXtlTrace::VPrintfExWithError(
	LPCSTR File, DWORD Line, DWORD /*Category*/, DWORD Level, DWORD Error, 
	LPCWSTR Format, va_list ap)
{
	XTL_SAVE_LAST_ERROR();

	WCHAR szBuffer[MAX_CHARS];
	size_t cchRemaining = MAX_CHARS;
	WCHAR *pszNext = szBuffer;

	if (0 == File || 0 == Line)
	{
		HRESULT hr = ::StringCchPrintfExW(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			L"%X.%X %d:", 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}
	else
	{
		HRESULT hr = ::StringCchPrintfExW(
			pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0,
			L"%hs(%d) : %X.%X %d:", 
			File, Line, 
			::GetCurrentProcessId(), 
			::GetCurrentThreadId(), 
			Level);
		XTLASSERT(SUCCEEDED(hr)); hr;
	}

	HRESULT hr = ::StringCchVPrintfExW(
		pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		Format, ap);
	XTLASSERT(SUCCEEDED(hr));

	if (Error & APPLICATION_ERROR_MASK) 
	{
		hr = ::StringCchPrintfExW(
			pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0, 
			L"Error %u (0x%08X): (application error)\n", 
			Error, Error);
		XTLASSERT(SUCCEEDED(hr));
	}
	else 
	{
		hr = ::StringCchPrintfExW(pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0, 
			L"Error %u (0x%08X):", Error, Error);
		XTLASSERT(SUCCEEDED(hr));
		DWORD cchUsed = ::FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, Error, 0, pszNext, (DWORD)cchRemaining, NULL);
		if (cchUsed > 0) 
		{
			pszNext += cchUsed;
			cchRemaining -= cchUsed;
		}
		else 
		{
			hr = ::StringCchPrintfExW(pszNext, cchRemaining,
				&pszNext, &cchRemaining, 0,
				L"(n/a)\n");
			XTLASSERT(SUCCEEDED(hr));
		}
	}

	SendOutput(szBuffer, MAX_CHARS - (DWORD) cchRemaining);
}

//-------------------------------------------------------------------------
//
// MemoryDumpToString
//
// Make a string of the byte array to the hexadecimal dump format
// For the output string buffer, you have to call ::LocalFree() 
//
//-------------------------------------------------------------------------

inline 
LPCTSTR 
XtlTraceMemoryDump(
	OUT LPTSTR* lpOutBuffer, 
	IN const BYTE* lpBuffer, 
	IN DWORD cbBuffer)
{
	DWORD cbOutBuffer = (cbBuffer * 3 + 1) * sizeof(TCHAR);
	*lpOutBuffer = (LPTSTR) ::LocalAlloc(0, cbOutBuffer);
	XTLASSERT(NULL != *lpOutBuffer);
	for (DWORD i = 0; i < cbBuffer; ++i) 
	{
		if (i > 0 && i % 20 == 0) 
		{
			HRESULT hr = ::StringCchPrintf(
				*lpOutBuffer + i,
				cbOutBuffer - (i * 3) * sizeof(TCHAR),
				TEXT("%02X "), lpBuffer[i]);
			XTLASSERT(SUCCEEDED(hr)); hr;
		}
		else 
		{
			HRESULT hr = ::StringCchPrintf(
				*lpOutBuffer + i,
				cbOutBuffer - (i * 3) * sizeof(TCHAR),
				TEXT("%02X\n"), lpBuffer[i]);
			XTLASSERT(SUCCEEDED(hr)); hr;
		}
	}
	return *lpOutBuffer;
}

extern __declspec(selectany) CXtlTrace _xtlTrace = CXtlTrace();
extern __declspec(selectany) CXtlConsoleTrace _xtlConsoleTrace = CXtlConsoleTrace();
extern __declspec(selectany) CXtlDebuggerTrace _xtlDebuggerTrace = CXtlDebuggerTrace();

inline
CXtlTrace&
XtlTraceGetInstance()
{
	return _xtlTrace;
}

inline
CXtlConsoleTrace&
XtlTraceGetConsoleTrace()
{
	return _xtlConsoleTrace.EnabledInstance();
}

inline
CXtlDebuggerTrace&
XtlTraceGetDebuggerTrace()
{
	return _xtlDebuggerTrace;
}

inline 
void
XtlTraceSetTraceLevel(DWORD TraceLevel)
{
	XtlTraceGetInstance().TraceLevel = TraceLevel;
}

inline 
void
XtlTraceSetTraceCategory(DWORD TraceCategory )
{
	XtlTraceGetInstance().TraceCategory = TraceCategory ;
}

inline 
bool
XtlTraceEnableConsoleTrace(bool Enable = true)
{
	if (Enable)
	{
		return XtlTraceGetInstance().Attach(&XtlTraceGetConsoleTrace());
	} 
	else 
	{	
		return XtlTraceGetInstance().Detach(&XtlTraceGetConsoleTrace());
	}
}

inline 
bool
XtlTraceEnableDebuggerTrace(BOOL Enable = true)
{
	if (Enable)
	{
		return XtlTraceGetInstance().Attach(&XtlTraceGetDebuggerTrace());
	} 
	else 
	{	
		return XtlTraceGetInstance().Detach(&XtlTraceGetDebuggerTrace());
	}
}

inline 
BOOL 
XtlTraceLoadSettingsFromRegistry(
	LPCTSTR szRegPath, 
	HKEY hRootKey = HKEY_LOCAL_MACHINE)
{
	const DWORD SystemOutputFlags =     0x00000001;
	const DWORD ConsoleOutputFlags =    0x00000002;
	const DWORD EventTraceOutputFlags = 0x00000004;

	HKEY hKey = (HKEY)INVALID_HANDLE_VALUE;
	LONG lResult = ::RegOpenKeyEx(hRootKey, szRegPath, 0, KEY_READ, &hKey);
	if (ERROR_SUCCESS != lResult) 
	{
		return FALSE;
	}

	DWORD cbValue;
	DWORD dwValueType;
	DWORD dwDebugLevel;

	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(hKey, 
		_T("TraceLevel"), 
		NULL, 
		&dwValueType,
		(LPBYTE)&dwDebugLevel,
		&cbValue);

	if (ERROR_SUCCESS == lResult) 
	{
		XtlTraceSetTraceLevel(dwDebugLevel);
	}

	DWORD dwDebugOutputFlags;
	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(
		hKey, 
		_T("TraceOutput"),
		NULL, 
		&dwValueType,
		(LPBYTE)&dwDebugOutputFlags,
		&cbValue);

	if (ERROR_SUCCESS == lResult)
	{
		if (dwDebugOutputFlags & SystemOutputFlags) 
		{
			XtlTraceEnableDebuggerTrace(TRUE);
		}
		else if (dwDebugOutputFlags & ConsoleOutputFlags) 
		{
			XtlTraceEnableConsoleTrace(TRUE);
		}
		else if (dwDebugOutputFlags & EventTraceOutputFlags) 
		{
		}
	}

	DWORD traceCategory;
	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(
		hKey,
		_T("TraceCategory"),
		NULL,
		&dwValueType,
		(LPBYTE)&traceCategory,
		&cbValue);

	if (ERROR_SUCCESS == lResult) 
	{
		XtlTraceSetTraceCategory(traceCategory);
	}

	XTLVERIFY(0 == ::RegCloseKey(hKey));

	return TRUE;
}

inline 
bool 
XtlTraceIsEnabled(DWORD Category, DWORD Level)
{
	CXtlTrace& instance = XtlTraceGetInstance();
    if (Category & instance.TraceCategory && Level <= instance.TraceLevel)
	{
		return true;
	}
	return false;
}


#if XTLTRACE_REMOVE_FILE_PATH

static DWORD_PTR _XtlTraceFilePathOffset = 0;

inline 
LPCSTR 
XtlTraceGetFileName(LPCSTR FilePath)
{
	// ignore non _FT formats
	//if (FilePath[0] == '\0' || 
	//	(FilePath[1] != ':' && FilePath[1] != '\\')) 
	//{
	//	return FilePath;
	//}
	if (FilePath[0] == 0) return FilePath;
#if XTLTRACE_CACHE_FILENAME
	if (_XtlTraceFilePathOffset > 0) 
	{ 
		LPCSTR FileName = FilePath + _XtlTraceFilePathOffset;
		return FileName; 
	}
#endif
	LPCSTR p = FilePath;
	while (*p != 0) ++p;
	while (*p != '\\' && *p != '/' && *p != ':' && p != FilePath ) --p;
	if (FilePath != p) ++p;
	DWORD_PTR offset = p - FilePath;
#if XTLTRACE_CACHE_FILENAME
	_XtlTraceFilePathOffset = offset;
#endif
	LPCSTR FileName = FilePath + offset;
	return FileName;
}
#else // XTLTRACE_REMOVE_FILE_PATH
inline LPCSTR XtlTraceGetFileName(LPCSTR x) { return x; }
#endif // XTLTRACE_REMOVE_FILE_PATH

inline
void __cdecl
XtlTrace2(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, LPCSTR Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	XtlTraceGetInstance().VPrintfEx(File,Line,Category,Level,Format,ap);
	va_end(ap);
}

inline
void __cdecl
XtlTrace2(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, LPWSTR Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	XtlTraceGetInstance().VPrintfEx(File,Line,Category,Level,Format,ap);
	va_end(ap);
}

inline
void __cdecl
XtlTraceV2(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, LPCSTR Format, va_list ap)
{
	XtlTraceGetInstance().VPrintfEx(File,Line,Category,Level,Format,ap);
}

inline
void __cdecl
XtlTraceV2WithError(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, DWORD Error, LPCSTR Format, va_list ap)
{
	XtlTraceGetInstance().VPrintfExWithError(File,Line,Category,Level,Error,Format,ap);
}

inline
void __cdecl
XtlTraceV2(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, LPCWSTR Format, va_list ap)
{
	XtlTraceGetInstance().VPrintfEx(File,Line,Category,Level,Format,ap);
}

inline
void __cdecl
XtlTraceV2WithError(LPCSTR File, DWORD Line, DWORD Category, DWORD Level, DWORD Error, LPCWSTR Format, va_list ap)
{
	XtlTraceGetInstance().VPrintfExWithError(File,Line,Category,Level,Error,Format,ap);
}

#ifndef XTLTRACE_DEFAULT_CATEGORY
#define XTLTRACE_DEFAULT_CATEGORY 0x1000000
#endif
#ifndef XTLTRACE_DEFAULT_LEVEL
#define XTLTRACE_DEFAULT_LEVEL 0
#endif

class CXtlTraceBufferA : public CStaticStringBufferA<256>
{
	LPCSTR m_lpFileName;
	DWORD m_dwLine;
public:
	CXtlTraceBufferA(LPCSTR File, DWORD Line) : 
	  m_lpFileName(File), m_dwLine(Line)
	{
	}
	//
	// DoTrace stuff
	//
	void DoTraceIfEnabled(LPCSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabled(XTLTRACE_DEFAULT_CATEGORY, XTLTRACE_DEFAULT_LEVEL, Format, ap);
		va_end(ap);
	}
	void DoTraceIfEnabled(LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabled(XTLTRACE_DEFAULT_CATEGORY, XTLTRACE_DEFAULT_LEVEL, Format, ap);
		va_end(ap);
	}
	void DoTraceIfEnabledWithLastError(LPCSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(XTLTRACE_DEFAULT_CATEGORY, XTLTRACE_DEFAULT_LEVEL, ::GetLastError(), Format, ap);
		va_end(ap);
	}
	void DoTraceIfEnabledWithLastError(LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(XTLTRACE_DEFAULT_CATEGORY, XTLTRACE_DEFAULT_LEVEL, ::GetLastError(), Format, ap);
		va_end(ap);
	}
	void DoTraceIfEnabledWithError(DWORD Error, LPCSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(XTLTRACE_DEFAULT_CATEGORY, XTLTRACE_DEFAULT_LEVEL, Error, Format, ap);
		va_end(ap);
	}
	void DoTraceIfEnabledWithError(DWORD Error, LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(XTLTRACE_DEFAULT_CATEGORY, XTLTRACE_DEFAULT_LEVEL, Error, Format, ap);
		va_end(ap);
	}
	//
	// DoTraceEx stuff
	//
	void DoTraceExIfEnabled(DWORD Category, DWORD Level, LPCSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabled(Category, Level, Format, ap);
		va_end(ap);
	}
	void DoTraceExIfEnabled(DWORD Category, DWORD Level, LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabled(Category, Level, Format, ap);
		va_end(ap);
	}
	void DoTraceExIfEnabledWithLastError(DWORD Category, DWORD Level, LPCSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(Category, Level, ::GetLastError(), Format, ap);
		va_end(ap);
	}
	void DoTraceExIfEnabledWithLastError(DWORD Category, DWORD Level, LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(Category, Level, ::GetLastError(), Format, ap);
		va_end(ap);
	}
	void DoTraceExIfEnabledWithError(DWORD Category, DWORD Level, DWORD Error, LPCSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(Category, Level, Error, Format, ap);
		va_end(ap);
	}
	void DoTraceExIfEnabledWithError(DWORD Category, DWORD Level, DWORD Error, LPCWSTR Format, ...)
	{
		va_list ap;
		va_start(ap, Format);
		DoTraceExVIfEnabledWithError(Category, Level, Error, Format, ap);
		va_end(ap);
	}
	//
	// DoTraceExV stuff
	//
	void DoTraceExVIfEnabled(DWORD Category, DWORD Level, LPCSTR Format, va_list ap)
	{
		if (!XtlTraceIsEnabled(Category, Level)) return;
		XtlTraceV2(XtlTraceGetFileName(m_lpFileName), m_dwLine, Category, Level, Format, ap);
	}
	void DoTraceExVIfEnabled(DWORD Category, DWORD Level, LPCWSTR Format, va_list ap)
	{
		if (!XtlTraceIsEnabled(Category, Level)) return;
		XtlTraceV2(XtlTraceGetFileName(m_lpFileName), m_dwLine, Category, Level, Format, ap);
	}
	void DoTraceExVIfEnabledWithError(DWORD Category, DWORD Level, DWORD Error, LPCSTR Format, va_list ap)
	{
		if (!XtlTraceIsEnabled(Category, Level)) return;
		XtlTraceV2WithError(XtlTraceGetFileName(m_lpFileName), m_dwLine, Category, Level, Error, Format, ap);
	}
	void DoTraceExVIfEnabledWithError(DWORD Category, DWORD Level, DWORD Error, LPCWSTR Format, va_list ap)
	{
		if (!XtlTraceIsEnabled(Category, Level)) return;
		XtlTraceV2WithError(XtlTraceGetFileName(m_lpFileName), m_dwLine, Category, Level, Error, Format, ap);
	}
};

#ifdef NO_XTLTRACE
#define XTLTRACE __noop
#define XTLTRACE2 __noop
#else
#define XTLTRACE XTL::CXtlTraceBufferA(__FILE__,__LINE__).DoTraceIfEnabled
#define XTLTRACE_ERR XTL::CXtlTraceBufferA(__FILE__,__LINE__).DoTraceIfEnabledWithLastError
#define XTLTRACE_ERR_EX XTL::CXtlTraceBufferA(__FILE__,__LINE__).DoTraceIfEnabledWithError
#define XTLTRACE2 XTL::CXtlTraceBufferA(__FILE__,__LINE__).DoTraceExIfEnabled
#define XTLTRACE2_ERR XTL::CXtlTraceBufferA(__FILE__,__LINE__).DoTraceExIfEnabledWithLastError
#define XTLTRACE2_ERR_EX XTL::CXtlTraceBufferA(__FILE__,__LINE__).DoTraceExIfEnabledWithError
#endif

const DWORD xtlTraceCall = 1;
const DWORD xtlTLCall = 4;

class CXtlCallTrace
{
	LPCSTR m_file;
	DWORD m_line;
	DWORD m_cat;
	LPCSTR m_func;
public:
	CXtlCallTrace(LPCSTR File, DWORD Line, DWORD Category, LPCSTR FunName) throw() : 
		m_file(File), m_line(Line), m_cat(Category), m_func(FunName)
	{
		XTL::CXtlTraceBufferA(m_file,m_line).DoTraceExIfEnabled(m_cat, xtlTLCall, "ENTER>%s\n", m_func);
	}
	~CXtlCallTrace()
	{
		XTL::CXtlTraceBufferA(m_file,m_line).DoTraceExIfEnabled(m_cat, xtlTLCall, "LEAVE>%s\n", m_func);
	}
};

template <typename RetT>
class CXtlCallTraceEx
{
	LPCSTR m_file;
	DWORD m_line;
	DWORD m_cat;
	LPCSTR m_func;
	const RetT& m_ret;
public:
	CXtlCallTraceEx(LPCSTR File, DWORD Line, DWORD Category, LPCSTR FunName, const RetT& Ret) throw() : 
		m_file(File), m_line(Line), m_cat(Category), m_func(FunName), m_ret(Ret)
	{
		XTL::CXtlTraceBufferA(m_file,m_line).DoTraceExIfEnabled(m_cat, xtlTLCall, "ENTER>%s\n", m_func);
	}
	~CXtlCallTraceEx()
	{
		XTL::CXtlTraceBufferA(m_file,m_line).DoTraceExIfEnabled(m_cat, xtlTLCall, "LEAVE>%s (ret=%x)\n", m_func, m_ret);
	}
private:
	CXtlCallTraceEx(const CXtlCallTraceEx&);
	const CXtlCallTraceEx& operator=(const CXtlCallTraceEx&);
};

#define XTLCALLTRACE() XTL::CXtlCallTrace _call_tracer(__FILE__,__LINE__,XTL::xtlTraceCall,__FUNCTION__)
#define XTLCALLTRACE2(cat) XTL::CXtlCallTrace _call_tracer(__FILE__,__LINE__,cat,__FUNCTION__)
#define XTLCALLTRACE_RET(_type,_ret) XTL::CXtlCallTraceEx<_type> _call_tracer(__FILE__, __LINE__, XTL::xtlTraceCall,__FUNCTION__,_ret)
#define XTLCALLTRACE_RET2(cat,_type,_ret) XTL::CXtlCallTraceEx<_type> _call_tracer(__FILE__, __LINE__, cat,__FUNCTION__,_ret)

} // namespace XTL

#endif /* _XTLTRACE_H_ */

