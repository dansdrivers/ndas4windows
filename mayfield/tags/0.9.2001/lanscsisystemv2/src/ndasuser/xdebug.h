#ifndef _XDEBUG_H_
#define _XDEBUG_H_
#pragma once

#include <wmistr.h>
#include <evntrace.h>

struct __declspec(novtable) IXDebugOutput
{
	virtual VOID OutputDebugString(LPCTSTR lpOutputString, DWORD cchOutput = 0, DWORD dwLevel = 0) = 0;
};

class XDebugSystemOutput : public IXDebugOutput
{
public:
	virtual VOID OutputDebugString(LPCTSTR lpOutputString, DWORD cchOutput, DWORD dwLevel)
	{
		UNREFERENCED_PARAMETER(cchOutput);
		UNREFERENCED_PARAMETER(dwLevel);
		::OutputDebugString(lpOutputString); 
	}
};

// {1106BB3E-BE2D-4c68-AFB2-2BD3D47149EC}
//DEFINE_GUID(XDebugEventTracer, 
//0x1106bb3e, 0xbe2d, 0x4c68, 0xaf, 0xb2, 0x2b, 0xd3, 0xd4, 0x71, 0x49, 0xec);

// Control GUID for this provider
// {9219E43F-D999-4387-BD8E-BFF19E7D3FE8}
//GUID ControlGuid =
//{ 0x9219e43f, 0xd999, 0x4387, { 0xbd, 0x8e, 0xbf, 0xf1, 0x9e, 0x7d, 0x3f, 0xe8 } };

// Only one transaction GUID will be registered for this provider.
//GUID TransactionGuid =
// {9FF69AC6-2562-4186-8DDE-E3B420FF4A3E}
//{ 0x9ff69ac6, 0x2562, 0x4186, { 0x8d, 0xde, 0xe3, 0xb4, 0x20, 0xff, 0x4a, 0x3e } };

class XDebugEventTrace : public IXDebugOutput
{
	// User Event Layout
#pragma warning(disable: 4200)
	typedef struct _XDEBUG_EVENT {
		EVENT_TRACE_HEADER Header;
		DWORD dwLevel;
		TCHAR szMessage[0];
	} XDEBUG_EVENT, *PXDEBUG_EVENT;
#pragma warning(default: 4200)

	TRACEHANDLE m_hLoggerHandle;
	TRACEHANDLE m_hRegistrationHandle;

	BOOL m_bTraceOnFlag;
	TRACE_GUID_REGISTRATION m_TraceGuidReg[1];

	GUID m_ControlGuid;
	GUID m_TransactionGuid;
	ULONG m_EnableLevel;
	ULONG m_EnableFlags;

public:

	XDebugEventTrace(LPCGUID lpControlGuid, LPCGUID lpTransactionGuid) :
		m_hLoggerHandle(NULL),
		m_hRegistrationHandle(NULL),
		m_bTraceOnFlag(FALSE)
	{
		m_ControlGuid = *lpControlGuid;
		m_TransactionGuid = *lpTransactionGuid;
		m_TraceGuidReg[0].Guid = &m_TransactionGuid;
		m_TraceGuidReg[0].RegHandle = NULL;
	}

	BOOL Register()
	{
		ULONG Status = RegisterTraceGuidsW(
			(WMIDPREQUEST)ControlCallback__, // callback function
			this,
			&m_ControlGuid,
			1,
			m_TraceGuidReg,
			NULL,
			NULL,
			&m_hRegistrationHandle);
		
		if (Status != ERROR_SUCCESS) {
			::SetLastError(Status);
			return FALSE;
		}
		return TRUE;
	}

	BOOL Unregister()
	{
		ULONG Status = UnregisterTraceGuids(m_hRegistrationHandle);
		if (Status != ERROR_SUCCESS) {
			::SetLastError(Status);
			return FALSE;
		}
		return TRUE;
	}

	/*++

	Routine Description:

		Callback function when enabled.

	Arguments:

		RequestCode - Flag for either enable or disable.
		Context - User-defined context.
		InOutBufferSize - not used.
		Buffer - WNODE_HEADER for the logger session.

	Return Value:

		Error Status. ERROR_SUCCESS if successful.

	--*/
	ULONG ControlCallback(
		IN WMIDPREQUESTCODE RequestCode,
		IN OUT ULONG *InOutBufferSize,
		IN OUT PVOID Buffer)
	{
		ULONG Status;
		ULONG RetSize;

		Status = ERROR_SUCCESS;

		switch (RequestCode)
		{
			case WMI_ENABLE_EVENTS:
			{
				RetSize = 0;
				m_hLoggerHandle = GetTraceLoggerHandle( Buffer );
				m_EnableLevel = GetTraceEnableLevel(m_hLoggerHandle);
				m_EnableFlags = GetTraceEnableFlags(m_hLoggerHandle);
				_tprintf(_T("Logging enabled to 0x%016I64x(%d,%d,%d)\n"),
						m_hLoggerHandle, RequestCode, m_EnableLevel, m_EnableFlags);
				m_bTraceOnFlag = TRUE;
				break;
			}
			case WMI_DISABLE_EVENTS:
			{
				m_bTraceOnFlag = FALSE;
				RetSize = 0;
				m_hLoggerHandle = 0;
				_tprintf(_T("\nLogging Disabled\n"));
				break;
			}
			default:
			{
				RetSize = 0;
				Status = ERROR_INVALID_PARAMETER;
				break;
			}

		}

		*InOutBufferSize = RetSize;
		return(Status);
	}

	static ULONG ControlCallback__(
		IN WMIDPREQUESTCODE RequestCode,
		IN PVOID Context,
		IN OUT ULONG *InOutBufferSize,
		IN OUT PVOID Buffer)
	{
		return reinterpret_cast<XDebugEventTrace*>(Context)->
			ControlCallback(RequestCode, InOutBufferSize, Buffer);
	}

	virtual VOID OutputDebugString(LPCTSTR lpOutputString, DWORD cchOutputString = 0, DWORD dwLevel = 0)
	{
		if (!m_hLoggerHandle) {
			return;
		}

		BYTE buffer[sizeof(XDEBUG_EVENT) + 4096];
		PXDEBUG_EVENT pXDebugEvent = reinterpret_cast<PXDEBUG_EVENT>(&buffer[0]);

		DWORD ccbOutputString = min(4096, (cchOutputString + 1) * sizeof(TCHAR));
		DWORD ccbEvent = sizeof(XDEBUG_EVENT) + ccbOutputString;

		::ZeroMemory(pXDebugEvent, ccbEvent);
		pXDebugEvent->Header.Size = (USHORT)ccbEvent;
		pXDebugEvent->Header.Flags = WNODE_FLAG_TRACED_GUID;
		pXDebugEvent->Header.Guid = m_TransactionGuid;
		pXDebugEvent->Header.Class.Type = EVENT_TRACE_TYPE_INFO;
		pXDebugEvent->dwLevel = dwLevel;

		HRESULT hr = ::StringCchCopy( pXDebugEvent->szMessage, 4096, lpOutputString);
		_ASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);

		ULONG status = ::TraceEvent(m_hLoggerHandle, (PEVENT_TRACE_HEADER) pXDebugEvent);
		_ASSERT(status == ERROR_SUCCESS);
	}
};

class XDebugConsoleOutput : public IXDebugOutput
{
	HANDLE m_hStdOut;
	HANDLE m_hStdErr;
	HANDLE m_hStdIn;

public:
	XDebugConsoleOutput()
	{
		//
		// just try to alloc a console for a GUI application
		// which has no pre-allocated console.
		// For console applications, this call is subject to fail.
		// However, we can still get standard handles if there is a console
		// whichever.
		//
		(VOID) ::AllocConsole();
		m_hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
		m_hStdErr = ::GetStdHandle(STD_ERROR_HANDLE);
		m_hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
	}

	virtual VOID OutputDebugString(LPCTSTR lpOutputString, DWORD cchOutput, DWORD dwLevel)
	{
		UNREFERENCED_PARAMETER(dwLevel);
		if (m_hStdOut == INVALID_HANDLE_VALUE) return;
		DWORD cchWritten;
		::OutputDebugString(lpOutputString);
		(VOID) ::WriteConsole(m_hStdOut, lpOutputString, cchOutput, &cchWritten, NULL);
	}
};

#define XDEBUG_USE_CRITSEC

class XDebug {

	static const MAX_OUTPUTS = 5;
	static const DEFAULT_OUTPUT_LEVEL = 0;

	DWORD m_nOutput;
	IXDebugOutput* m_ppOutput[MAX_OUTPUTS];

	CRITICAL_SECTION m_cs;

public:

	DWORD dwOutputLevel;
	LPCTSTR szModule;

	static const DWORD OL_NOISE = 0x50;
	static const DWORD OL_TRACE = 0x40;
	static const DWORD OL_INFO = 0x30;
	static const DWORD OL_WARNING = 0x20;
	static const DWORD OL_ERROR = 0x10;
	static const DWORD OL_ALWAYS = 0xFFFFFFFF;

	XDebug(LPCTSTR szModuleName) : 
		m_nOutput(0),
		dwOutputLevel(DEFAULT_OUTPUT_LEVEL),
		szModule(szModuleName)
	{
		::InitializeCriticalSection(&m_cs);
		for (DWORD i = 0; i < MAX_OUTPUTS; ++i) {
			m_ppOutput[i] = NULL;
		}
	}

	XDebug() 
	{	XDebug(TEXT("XDebug")); }

	~XDebug()
	{
		::DeleteCriticalSection(&m_cs);
	}

	BOOL Attach(IXDebugOutput* pOutput)
	{
		::EnterCriticalSection(&m_cs);
		if (m_nOutput < MAX_OUTPUTS) {
			for (DWORD i = 0; i < m_nOutput; ++i) {
				if (m_ppOutput[i] == pOutput) {
					::LeaveCriticalSection(&m_cs);
					return TRUE;
				}
			}
			m_ppOutput[m_nOutput] = pOutput;
			++m_nOutput;
			::LeaveCriticalSection(&m_cs);
			return TRUE;
		} else {
			for (DWORD i = 0; i < MAX_OUTPUTS; ++i) {
				if (m_ppOutput[i] == NULL) {
					m_ppOutput[i] = pOutput;
					::LeaveCriticalSection(&m_cs);
					return TRUE;
				}
			}
		}
		::LeaveCriticalSection(&m_cs);
		return FALSE;
	}

	BOOL Detach(IXDebugOutput* pOutput)
	{
		::EnterCriticalSection(&m_cs);
		for (DWORD i = 0; i < m_nOutput; ++i) {
			if (m_ppOutput[i] == pOutput) {
				m_ppOutput[i] = NULL;
			}
		}
		::LeaveCriticalSection(&m_cs);
		return TRUE;
	}

	inline VOID SendOutput(DWORD dwLevel, LPCTSTR szOutputString, DWORD cchOutputString)
	{
		if (cchOutputString == 0) {
			HRESULT hr = ::StringCchLength(szOutputString, 4096, (size_t*) &cchOutputString);
		}

		for (DWORD i = 0; i < m_nOutput; ++i)
			if (m_ppOutput[i]) 
				m_ppOutput[i]->OutputDebugString(szOutputString, cchOutputString, dwLevel);
	}

	inline VOID Printf(DWORD dwLevel, LPCTSTR szFormat, ...)
	{
		va_list ap;
		va_start(ap, szFormat);
		VPrintf(dwLevel, szFormat, ap);
		va_end(ap);
	}

	inline VOID VPrintf(DWORD dwLevel, LPCTSTR szFormat, va_list va)
	{
		const static DWORD cchBuffer = 4096;
		TCHAR szBuffer[cchBuffer];
		size_t cchRemaining(cchBuffer);
		TCHAR *pszNext = szBuffer;
		HRESULT hr;
		
		hr = ::StringCchPrintfEx(pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0,
			TEXT("%08s,%04X,"), szModule, ::GetCurrentThreadId());
		_ASSERT(SUCCEEDED(hr));
		hr = ::StringCchVPrintfEx(pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0,
			szFormat, va);
		_ASSERT(SUCCEEDED(hr));
		SendOutput(dwLevel, szBuffer, 4096 - cchRemaining);
	}

	inline VOID VPrintfErr(DWORD dwLevel, LPCTSTR szFormat, DWORD dwError, va_list ap)
	{
		const static DWORD cchBuffer = 4096;
		TCHAR szBuffer[cchBuffer];
		size_t cchRemaining = cchBuffer;
		TCHAR *pszNext = szBuffer;
		HRESULT hr;
		
		hr = ::StringCchPrintfEx(pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0,
			TEXT("%08s,%04X,"), szModule, ::GetCurrentThreadId());
		_ASSERT(SUCCEEDED(hr));
		hr = ::StringCchVPrintfEx(pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0, szFormat, ap);
		_ASSERT(SUCCEEDED(hr));

		if (dwError & APPLICATION_ERROR_MASK) {
			hr = ::StringCchPrintf(pszNext, cchRemaining, 
				TEXT("Error from an application %d (0x%08X)\n"), dwError, dwError);
			_ASSERT(SUCCEEDED(hr));
		} else {
			hr = ::StringCchPrintfEx(pszNext, cchRemaining,
				&pszNext, &cchRemaining, 0, TEXT("Error %d (0x%08X):"), dwError, dwError);
			_ASSERT(SUCCEEDED(hr));
			DWORD cchUsed = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, dwError, 0, pszNext, cchRemaining, NULL);
			pszNext += cchUsed;
			cchRemaining -= cchUsed;
		}

		SendOutput(dwLevel, szBuffer, 4096 - cchRemaining);
	}
};

class XDebugModule
{
	XDebug* m_pxdebug;
public:
	XDebugModule(LPCTSTR szModuleName)
	{
	}
};

//-------------------------------------------------------------------------
//
// MemoryDumpToString
//
// Make a string of the byte array to the hexadecimal dump format
// For the output string buffer, you have to call ::LocalFree() 
//
//-------------------------------------------------------------------------

inline LPCTSTR XDebugMemoryDump(OUT LPTSTR* lpOutBuffer, IN const BYTE* lpBuffer, IN DWORD cbBuffer)
{
	DWORD cbOutBuffer = (cbBuffer * 3 + 1) * sizeof(TCHAR);
	*lpOutBuffer = (LPTSTR) ::LocalAlloc(0, cbOutBuffer);
	_ASSERTE(NULL != *lpOutBuffer);
	for (DWORD i = 0; i < cbBuffer; ++i) {
		if (i > 0 && i % 20 == 0) {
			HRESULT hr = ::StringCchPrintf(
				*lpOutBuffer + i,
				cbOutBuffer - (i * 3) * sizeof(TCHAR),
				TEXT("%02X "));
			_ASSERT(SUCCEEDED(hr));
		} else {
			HRESULT hr = ::StringCchPrintf(
				*lpOutBuffer + i,
				cbOutBuffer - (i * 3) * sizeof(TCHAR),
				TEXT("%02X\n"));
			_ASSERT(SUCCEEDED(hr));
		}
	}
	return *lpOutBuffer;
}

#ifdef INIT_XDEBUG_MODULE

XDebug* _PXDebug = NULL;

__inline BOOL XDebugInit(LPCTSTR szAppName)
{
	if (_PXDebug == NULL) {
		_PXDebug = new XDebug(szAppName);
	}
	return (NULL != _PXDebug);
}

__inline VOID XDebugCleanup()
{
	if (_PXDebug != NULL) {
		delete _PXDebug;
		_PXDebug = NULL;
	}
}
#else

extern XDebug* _PXDebug;

#endif

__inline VOID XDebugPrintf(DWORD dwLevel, LPCTSTR szFormat, ...)
{
	if (_PXDebug && _PXDebug->dwOutputLevel >= dwLevel) {
		va_list ap;
		va_start(ap, szFormat);
		_PXDebug->VPrintf(dwLevel, szFormat, ap);
		va_end(ap);
	}
}

#define XDEBUGV(_FuncName_,_l_) \
	__inline VOID _FuncName_ (LPCTSTR szFormat, ...) \
	{ \
		if (_PXDebug && _PXDebug->dwOutputLevel < _l_) { \
			va_list ap; \
			va_start(ap, szFormat); \
			_PXDebug->VPrintf(_l_, szFormat, ap); \
			va_end(ap); \
		} \
	}

#define XDEBUGV_SYSERR(_FuncName_,_l_) \
	__inline VOID _FuncName_ (LPCTSTR szFormat, ...) \
	{ \
		if (_PXDebug && _PXDebug->dwOutputLevel < _l_) { \
			va_list ap; \
			va_start(ap, szFormat); \
			_PXDebug->VPrintfErr(_l_, szFormat, ::GetLastError(), ap); \
			va_end(ap); \
		} \
	} \

#ifdef _WINSOCKAPI_
#define XDEBUGV_WSAERR(_FuncName_,_l_) \
	__inline VOID _FuncName_ (LPCTSTR szFormat, ...) \
	{ \
		if (_PXDebug && _PXDebug->dwOutputLevel < _l_) { \
			va_list ap; \
			va_start(ap, szFormat); \
			_PXDebug->VPrintfErr(_l_, szFormat, ::WSAGetLastError(), ap); \
			va_end(ap); \
		} \
	}
#endif

#define XDEBUGV_USERERR(_FuncName_,_l_) \
	__inline VOID _FuncName_ (DWORD dwError, LPCTSTR szFormat, ...) \
	{ \
		if (_PXDebug && _PXDebug->dwOutputLevel < _l_) { \
			va_list ap; \
			va_start(ap, szFormat); \
			_PXDebug->VPrintfErr(_l_, szFormat, dwError, ap); \
			va_end(ap); \
		} \
	}

#define XVDEBUGV(_FuncName_, _l_) \
	__inline VOID _FuncName_ (LPCTSTR szFormat, va_list ap) \
	{ \
		if (_PXDebug && _PXDebug->dwOutputLevel < _l_) { \
			_PXDebug->VPrintf(_l_, szFormat, ap); \
		} \
	}

XDEBUGV(XDebugAlways,  XDebug::OL_ALWAYS)
XDEBUGV(XDebugError,   XDebug::OL_ERROR)
XDEBUGV_SYSERR(XDebugErrorEx,   XDebug::OL_ERROR)
XDEBUGV_USERERR(XDebugErrorExUser,	XDebug::OL_ERROR)
XDEBUGV(XDebugWarning, XDebug::OL_WARNING)
XDEBUGV_SYSERR(XDebugWarningEx, XDebug::OL_WARNING)
XDEBUGV_USERERR(XDebugWarningExUser, XDebug::OL_WARNING)
XDEBUGV(XDebugInfo,    XDebug::OL_INFO)
XDEBUGV(XDebugTrace,   XDebug::OL_TRACE)
XDEBUGV(XDebugNoise,   XDebug::OL_NOISE)

XVDEBUGV(XVDebugInfo, XDebug::OL_INFO)
XVDEBUGV(XVDebugWarning, XDebug::OL_WARNING)
XVDEBUGV(XVDebugError, XDebug::OL_ERROR)
XVDEBUGV(XVDebugTrace, XDebug::OL_TRACE)
XVDEBUGV(XVDebugNoise, XDebug::OL_NOISE)

#ifdef _WINSOCKAPI_
XDEBUGV_WSAERR(XDebugErrorExWsa,	XDebug::OL_ERROR)
XDEBUGV_WSAERR(XDebugWarningExWsa,	XDebug::OL_ERROR)
#endif

#ifdef NO_XDEBUG
#define DebugPrintf __noop
#define DPAlways    __noop
#define DPError     __noop
#define DPErrorEx   __noop
#define DPErrorEx2  __noop
#define DPWarning   __noop
#define DPWarningEx __noop
#define DPWarningEx2 __noop
#define DPInfo      __noop
#define DPTrace     __noop
#define DPNoise     __noop

#ifdef _WINSOCKAPI_
#define DPErrorExWsa  __noop
#define DPWarningExWsa  __noop
#endif

#else
#define DebugPrintf XDebugPrintf
#define DPAlways    XDebugAlways
#define DPError     XDebugError
#define DPErrorEx   XDebugErrorEx
#define DPErrorEx2  XDebugErrorExUser
#define DPWarning   XDebugWarning
#define DPWarningEx XDebugWarningEx
#define DPWarningEx2 XDebugWarningExUser
#define DPInfo      XDebugInfo
#define DPTrace     XDebugTrace
#define DPNoise     XDebugNoise

#ifdef _WINSOCKAPI_
#define DPErrorExWsa  XDebugErrorExWsa
#define DPWarningExWsa  XDebugWarningExWsa
#endif

#endif

#define CHARTOHEX(dest, ch) \
	if ((ch) <= 9) { dest = TCHAR(CHAR(ch + '0')); } \
	else           { dest = TCHAR(CHAR(ch - 0xA + 'A')); }

template <typename T>
VOID DPType(DWORD dwLevel, const T* lpTypedData, DWORD cbSize)
{
	_ASSERTE(!IsBadReadPtr(lpTypedData, cbSize));
	// default is a byte dump
	TCHAR szBuffer[4096];
	TCHAR* psz = szBuffer;
	*psz = TEXT('\n');
	++psz;
	const BYTE* lpb = reinterpret_cast<const BYTE*>(lpTypedData);
	for (DWORD i = 0; i < cbSize; ++i) {
		CHARTOHEX(*psz, (lpb[i] >> 4) )
		++psz;
		CHARTOHEX(*psz, (lpb[i] & 0x0F))
		++psz;
		*psz = (i > 0 && (((i + 1)% 20) == 0)) ? TEXT('\n') : TEXT(' ');
		++psz;
	}
	*psz = TEXT('\0');

	XDebugPrintf(dwLevel, szBuffer);
}

#undef CHARTOHEX

#define _FT(x) _T(__FUNCTION__) _T(",") _T(x)

#endif /* _XDEBUG_H_ */
