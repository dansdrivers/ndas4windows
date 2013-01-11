#ifdef _XDEBUG_H_
#error XDEBUG should not be overlapped! Do not include xdebug.h from the header file
#else
#define _XDEBUG_H_

//
// You must define #define XDEBUG_MAIN_MODULE in the main.cpp 
// in executable file
// 

// #pragma once

// #define XDBG_USE_FILENAME

//#ifndef XDEBUG_MODULE_FLAG
// #define XDEBUG_MODULE_FLAG 0xFFFFFFFF
//#error XDEBUG_MODULE_FLAG is not defined.
//#endif

#include <wmistr.h>
#include <evntrace.h>

struct IXDbgOutput
{
	virtual VOID OutputDebugStringEx(
		LPCTSTR lpDebugInfo,
		LPCTSTR lpOutputString, 
		DWORD cchOutput = 0, 
		DWORD dwLevel = 0) = 0;

	virtual VOID OutputDebugString(
		LPCTSTR lpOutputString, 
		DWORD cchOutput = 0, 
		DWORD dwLevel = 0) = 0;
};

class XDbgSystemDebugOutput : public IXDbgOutput
{
public:

	virtual VOID OutputDebugStringEx(
		LPCTSTR lpDebugInfo,
		LPCTSTR lpOutputString, 
		DWORD cchOutput, 
		DWORD dwLevel)
	{
		UNREFERENCED_PARAMETER(cchOutput);
		UNREFERENCED_PARAMETER(dwLevel);
		TCHAR szBuffer[4096];
		HRESULT hr = ::StringCchPrintf(szBuffer, 4096, 
			_T("%s%s"), lpDebugInfo, lpOutputString);
		::OutputDebugString(szBuffer);
	}

	virtual VOID OutputDebugString(
		LPCTSTR lpOutputString, 
		DWORD cchOutput, 
		DWORD dwLevel)
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

class XDbgEventTrace : public IXDbgOutput
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

	XDbgEventTrace(LPCGUID lpControlGuid, LPCGUID lpTransactionGuid) :
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
			(WMIDPREQUEST)sControlCallback, // callback function
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

	static ULONG sControlCallback(
		IN WMIDPREQUESTCODE RequestCode,
		IN PVOID Context,
		IN OUT ULONG *InOutBufferSize,
		IN OUT PVOID Buffer)
	{
		return reinterpret_cast<XDbgEventTrace*>(Context)->
			ControlCallback(RequestCode, InOutBufferSize, Buffer);
	}

	virtual VOID OutputDebugStringEx(
		LPCTSTR lpDebugInfo,
		LPCTSTR lpOutputString, 
		DWORD cchOutputString = 0, 
		DWORD dwLevel = 0)
	{
		if (!m_hLoggerHandle) {
			return;
		}

		BYTE buffer[sizeof(XDEBUG_EVENT) + 4096];

		PXDEBUG_EVENT pXDebugEvent = 
			reinterpret_cast<PXDEBUG_EVENT>(&buffer[0]);

		DWORD ccbOutputString = 
			(4096 > (cchOutputString + 1) * sizeof(TCHAR)) ?
			(cchOutputString + 1) * sizeof(TCHAR) : 4096;

		DWORD ccbEvent = sizeof(XDEBUG_EVENT) + ccbOutputString;

		::ZeroMemory(pXDebugEvent, ccbEvent);
		pXDebugEvent->Header.Size = (USHORT)ccbEvent;
		pXDebugEvent->Header.Flags = WNODE_FLAG_TRACED_GUID;
		pXDebugEvent->Header.Guid = m_TransactionGuid;
		pXDebugEvent->Header.Class.Type = EVENT_TRACE_TYPE_INFO;
		pXDebugEvent->dwLevel = dwLevel;

		HRESULT hr = ::StringCchPrintf(
			pXDebugEvent->szMessage, 
			4096, 
			_T("%s%s"),
			lpDebugInfo,
			lpOutputString);

		_ASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);

		ULONG status = ::TraceEvent(
			m_hLoggerHandle, 
			(PEVENT_TRACE_HEADER) pXDebugEvent);

		_ASSERT(status == ERROR_SUCCESS);
	}

	virtual VOID OutputDebugString(
		LPCTSTR lpOutputString, 
		DWORD cchOutputString = 0, 
		DWORD dwLevel = 0)
	{
		OutputDebugStringEx(_T(""), lpOutputString, cchOutputString, dwLevel);
	}
};

class XDbgConsoleOutput : public IXDbgOutput
{
	HANDLE m_hStdOut;
//	HANDLE m_hStdErr;
//	HANDLE m_hStdIn;

public:
	XDbgConsoleOutput()
	{
		//
		// just try to alloc a console for a GUI application
		// which has no pre-allocated console.
		// For console applications, this call is subject to fail.
		// However, we can still get standard handles if there is a console
		// whichever.
		//
		(void) ::AllocConsole();
		m_hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
//		m_hStdErr = ::GetStdHandle(STD_ERROR_HANDLE);
//		m_hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
	}

	virtual VOID OutputDebugStringEx(
		LPCTSTR lpDebugInfo,
		LPCTSTR lpOutputString, 
		DWORD cchOutput, 
		DWORD dwLevel)
	{
		//
		// lpDebugInfo is ignored for console output
		//
		UNREFERENCED_PARAMETER(lpDebugInfo);
		UNREFERENCED_PARAMETER(dwLevel);

		if (INVALID_HANDLE_VALUE == m_hStdOut) return;
		DWORD cchWritten;
		(VOID) ::WriteConsole(
			m_hStdOut, 
			lpOutputString, 
			cchOutput, 
			&cchWritten, 
			NULL);
	}

	virtual VOID OutputDebugString(
		LPCTSTR lpOutputString, 
		DWORD cchOutput, 
		DWORD dwLevel)
	{
		UNREFERENCED_PARAMETER(dwLevel);
		if (INVALID_HANDLE_VALUE == m_hStdOut) return;
		DWORD cchWritten;
		(VOID) ::WriteConsole(
			m_hStdOut, 
			lpOutputString, 
			cchOutput, 
			&cchWritten, 
			NULL);
	}
};

#define XDEBUG_USE_CRITSEC

#define XDBG_OL_ALWAYS	0xFFFFFFFF
#define XDBG_OL_NOISE	0x50
#define XDBG_OL_TRACE	0x40
#define XDBG_OL_INFO	0x30
#define XDBG_OL_WARNING	0x20
#define XDBG_OL_ERROR	0x10
#define XDBG_OL_NONE	0x00

#define XDBG_INLINE inline

class XDebug {

	static const DWORD MAX_OUTPUTS = 5;
	static const DWORD DEFAULT_OUTPUT_LEVEL = 0;

	DWORD m_nOutput;
	IXDbgOutput* m_ppOutput[MAX_OUTPUTS];

	CRITICAL_SECTION m_cs;

public:

	DWORD dwEnabledModules;
	DWORD dwEnabledLibraryFlags;
	DWORD dwOutputLevel;
	LPCTSTR szModule;

	static const DWORD OL_ALWAYS = XDBG_OL_ALWAYS;
	static const DWORD OL_NOISE = XDBG_OL_NOISE;
	static const DWORD OL_TRACE = XDBG_OL_TRACE;
	static const DWORD OL_INFO = XDBG_OL_INFO;
	static const DWORD OL_WARNING = XDBG_OL_WARNING;
	static const DWORD OL_ERROR = XDBG_OL_ERROR;
	static const DWORD OL_NONE = XDBG_OL_NONE;

	XDebug(LPCTSTR szModuleName);
	XDebug();
	~XDebug();
	BOOL Attach(IXDbgOutput* pOutput);
	void CompactIXDbgOutput();
	BOOL Detach(IXDbgOutput* pOutput);
	void SendOutput(
		DWORD dwLevel, 
		LPCTSTR szOutputString, 
		DWORD cchOutputString);
	void Printf(DWORD dwLevel, LPCTSTR szFormat, ...);
	void VRawPrintf(DWORD dwLevel, LPCTSTR szFormat, va_list va);
	void VPrintf(DWORD dwLevel, LPCTSTR szFormat, va_list va);
	void VPrintfEx(LPCTSTR szDebugInfo, DWORD dwLevel, LPCTSTR szFormat, va_list va);
	void VPrintfErr(DWORD dwLevel, LPCTSTR szFormat, DWORD dwError, va_list ap);
};

XDBG_INLINE
XDebug::XDebug(
	LPCTSTR szModuleName) : 
	m_nOutput(0),
	dwOutputLevel(DEFAULT_OUTPUT_LEVEL),
	szModule(szModuleName),
	dwEnabledModules(0)
{
	::InitializeCriticalSection(&m_cs);
	for (DWORD i = 0; i < MAX_OUTPUTS; ++i) {
		m_ppOutput[i] = NULL;
	}
}

XDBG_INLINE
XDebug::XDebug() 
{
	XDebug(TEXT("XDebug")); 
}

XDBG_INLINE
XDebug::~XDebug()
{
	::DeleteCriticalSection(&m_cs);
}

XDBG_INLINE
BOOL 
XDebug::Attach(
	IXDbgOutput* pOutput)
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

XDBG_INLINE
void
XDebug::CompactIXDbgOutput()
{
	DWORD emp = MAX_OUTPUTS;
	for (DWORD i = 0; i < MAX_OUTPUTS; ++i) {
		if (NULL == m_ppOutput[i] && emp == MAX_OUTPUTS) {
			emp = i;
		} else if (NULL != m_ppOutput[i] && emp < MAX_OUTPUTS) {
			m_ppOutput[emp] = m_ppOutput[i];
			m_ppOutput[i] = NULL;
			DWORD j = emp + 1;
			while (j < MAX_OUTPUTS && NULL != m_ppOutput[j]) {
				++j;
			}
			emp = j;
		}
	}
}

XDBG_INLINE
BOOL 
XDebug::Detach(
	IXDbgOutput* pOutput)
{
	::EnterCriticalSection(&m_cs);
	for (DWORD i = 0; i < m_nOutput; ++i) {
		if (m_ppOutput[i] == pOutput) {
			m_ppOutput[i] = NULL;
			CompactIXDbgOutput();
			::LeaveCriticalSection(&m_cs);
			return TRUE;
		}
	}
	::LeaveCriticalSection(&m_cs);
	return FALSE;
}

XDBG_INLINE
void
XDebug::SendOutput(
	DWORD dwLevel, 
	LPCTSTR szOutputString, 
	DWORD cchOutputString)
{
	if (0 == cchOutputString) {
		HRESULT hr = ::StringCchLength(
			szOutputString, 
			4096, 
			(size_t*) &cchOutputString);
		_ASSERTE(SUCCEEDED(hr));
	}

	for (DWORD i = 0; i < m_nOutput; ++i) {
		if (m_ppOutput[i]) {
			m_ppOutput[i]->OutputDebugString(
				szOutputString, 
				cchOutputString, 
				dwLevel);
		}
	}
}

XDBG_INLINE
void
XDebug::Printf(
	DWORD dwLevel, 
	LPCTSTR szFormat, 
	...)
{
	va_list ap;
	va_start(ap, szFormat);
	VPrintf(dwLevel, szFormat, ap);
	va_end(ap);
}

XDBG_INLINE
void
XDebug::VRawPrintf(
	DWORD dwLevel, 
	LPCTSTR szFormat, 
	va_list va)
{
	//
	// Prevent shadowing of the error
	//
	DWORD dwLastError = ::GetLastError();

	const static DWORD cchBuffer = 4096;
	TCHAR szBuffer[cchBuffer];
	size_t cchRemaining(cchBuffer);
	TCHAR *pszNext = szBuffer;
	HRESULT hr;

	hr = ::StringCchVPrintfEx(pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		szFormat, va);
	_ASSERT(SUCCEEDED(hr));
	SendOutput(dwLevel, szBuffer, 4096 - (DWORD) cchRemaining);

	//
	// Restore last error
	//
	::SetLastError(dwLastError);
}

XDBG_INLINE
void
XDebug::VPrintf(
	DWORD dwLevel, 
	LPCTSTR szFormat, 
	va_list va)
{
	//
	// Prevent shadowing of the error
	//
	DWORD dwLastError = ::GetLastError();

	const static DWORD cchBuffer = 4096;
	TCHAR szBuffer[cchBuffer];
	size_t cchRemaining(cchBuffer);
	TCHAR *pszNext = szBuffer;
	//		hr = ::StringCchPrintfEx(pszNext, cchRemaining, 
	//			&pszNext, &cchRemaining, 0,
	//			TEXT("%08s,0x%04X,"), szModule, ::GetCurrentThreadId());
	//		_ASSERT(SUCCEEDED(hr));

	HRESULT hr = ::StringCchVPrintfEx(pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		szFormat, va);
	_ASSERT(SUCCEEDED(hr));
	SendOutput(dwLevel, szBuffer, 4096 - (DWORD) cchRemaining);

	//
	// Restore last error
	//
	::SetLastError(dwLastError);
}

XDBG_INLINE
void
XDebug::VPrintfEx(
	LPCTSTR szDebugInfo,
	DWORD dwLevel, 
	LPCTSTR szFormat, 
	va_list va)
{
	//
	// Prevent shadowing of the error
	//
	DWORD dwLastError = ::GetLastError();

	const static DWORD cchBuffer = 4096;
	TCHAR szBuffer[cchBuffer];
	size_t cchRemaining(cchBuffer);
	TCHAR *pszNext = szBuffer;
	HRESULT hr;

	hr = ::StringCchPrintfEx(pszNext, cchRemaining,
		&pszNext, &cchRemaining, 0,
		_T("%s"), szDebugInfo);

	hr = ::StringCchPrintfEx(pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		_T("%08s,0x%04X,"), szModule, ::GetCurrentThreadId());
	_ASSERT(SUCCEEDED(hr));

	hr = ::StringCchVPrintfEx(pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0,
		szFormat, va);
	_ASSERT(SUCCEEDED(hr));
	SendOutput(dwLevel, szBuffer, 4096 - (DWORD)cchRemaining);

	//
	// Restore last error
	//
	::SetLastError(dwLastError);
}

XDBG_INLINE
void
XDebug::VPrintfErr(
	DWORD dwLevel, 
	LPCTSTR szFormat, 
	DWORD dwError, 
	va_list ap)
{
	//
	// Prevent shadowing of the error
	//
	DWORD dwLastError = ::GetLastError();

	const static DWORD cchBuffer = 4096;
	TCHAR szBuffer[cchBuffer];
	size_t cchRemaining = cchBuffer;
	TCHAR *pszNext = szBuffer;
	HRESULT hr;

	//		hr = ::StringCchPrintfEx(pszNext, cchRemaining, 
	//			&pszNext, &cchRemaining, 0,
	//			TEXT("%08s,0x%04X,"), szModule, ::GetCurrentThreadId());
	//		_ASSERT(SUCCEEDED(hr));

	hr = ::StringCchVPrintfEx(pszNext, cchRemaining, 
		&pszNext, &cchRemaining, 0, szFormat, ap);
	_ASSERT(SUCCEEDED(hr));

	if (dwError & APPLICATION_ERROR_MASK) {
		hr = ::StringCchPrintfEx(pszNext, cchRemaining, 
			&pszNext, &cchRemaining, 0, 
			TEXT("Error from an application %u (0x%08X)\n"), dwError, dwError);
		_ASSERT(SUCCEEDED(hr));
	} else {
		hr = ::StringCchPrintfEx(pszNext, cchRemaining,
			&pszNext, &cchRemaining, 0, 
			TEXT("Error %u (0x%08X):"), dwError, dwError);
		_ASSERT(SUCCEEDED(hr));
		DWORD cchUsed = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, dwError, 0, pszNext, (DWORD)cchRemaining, NULL);
		if (cchUsed > 0) {
			pszNext += cchUsed;
			cchRemaining -= cchUsed;
		} else {
			hr = ::StringCchPrintfEx(pszNext, cchRemaining,
				&pszNext, &cchRemaining, 0,
				_T(" (no description available)\n"));
			_ASSERT(SUCCEEDED(hr));
		}
	}

	SendOutput(dwLevel, szBuffer, 4096 - (DWORD) cchRemaining);

	//
	// Restore last error
	//
	::SetLastError(dwLastError);
}

//-------------------------------------------------------------------------
//
// MemoryDumpToString
//
// Make a string of the byte array to the hexadecimal dump format
// For the output string buffer, you have to call ::LocalFree() 
//
//-------------------------------------------------------------------------

__inline 
LPCTSTR 
XDebugMemoryDump(
	OUT LPTSTR* lpOutBuffer, 
	IN const BYTE* lpBuffer, 
	IN DWORD cbBuffer)
{
	DWORD cbOutBuffer = (cbBuffer * 3 + 1) * sizeof(TCHAR);
	*lpOutBuffer = (LPTSTR) ::LocalAlloc(0, cbOutBuffer);
	_ASSERTE(NULL != *lpOutBuffer);
	for (DWORD i = 0; i < cbBuffer; ++i) {
		if (i > 0 && i % 20 == 0) {
			HRESULT hr = ::StringCchPrintf(
				*lpOutBuffer + i,
				cbOutBuffer - (i * 3) * sizeof(TCHAR),
				TEXT("%02X "), lpBuffer[i]);
			_ASSERT(SUCCEEDED(hr));
		} else {
			HRESULT hr = ::StringCchPrintf(
				*lpOutBuffer + i,
				cbOutBuffer - (i * 3) * sizeof(TCHAR),
				TEXT("%02X\n"), lpBuffer[i]);
			_ASSERT(SUCCEEDED(hr));
		}
	}
	return *lpOutBuffer;
}

#ifdef NO_XDEBUG
XDebug* _PXDebug = NULL;
#else /* NO_XDEBUG */

//#ifdef XDBG_MAIN_MODULE
//XDebug* _PXDebug = NULL;
//#else /* XDEBUG_MAIN_MODULE */
//extern XDebug* _PXDebug;
//#endif /* XDEBUG_MAIN_MODULE */
__declspec(selectany) XDebug* _PXDebug = NULL;

#endif /* NO_XDEBUG */

__inline 
BOOL 
XDbgInit(LPCTSTR szModuleName)
{
	_PXDebug = new XDebug(szModuleName);
	if (NULL == _PXDebug) {
		return FALSE;
	}
	return TRUE;
}

__inline 
void
XDbgCleanup()
{
	if (NULL != _PXDebug) {
		delete _PXDebug;
		_PXDebug = NULL;
	}
}

__inline 
void
XDbgSetOutputLevel(DWORD dwOutputLevel)
{
	if (NULL != _PXDebug) {
		_PXDebug->dwOutputLevel = dwOutputLevel;
	}
}

__inline 
void
XDbgSetModuleFlags(DWORD dwFlags)
{
	if (NULL != _PXDebug) {
		_PXDebug->dwEnabledModules = dwFlags;
	}
}

__inline 
void
XDbgSetLibraryFlags(DWORD dwFlags)
{
	if (NULL != _PXDebug) {
		_PXDebug->dwEnabledLibraryFlags = dwFlags;
	}
}

//#ifdef XDBG_MAIN_MODULE
//XDbgSystemDebugOutput* _pxdbgSystemDebugOutput = NULL;
//XDbgConsoleOutput* _pxdbgConsoleOutput = NULL;
//XDbgEventTrace* _pxdbgEventTrace = NULL;
//#else
//extern XDbgSystemDebugOutput* _pxdbgSystemDebugOutput;
//extern XDbgConsoleOutput* _pxdbgConsoleOutput;
//extern XDbgEventTrace* _pxdbgEventTrace;
//#endif
__declspec(selectany) XDbgSystemDebugOutput* _pxdbgSystemDebugOutput = NULL;
__declspec(selectany) XDbgConsoleOutput* _pxdbgConsoleOutput = NULL;
__declspec(selectany) XDbgEventTrace* _pxdbgEventTrace = NULL;

__inline 
BOOL 
XDbgEnableSystemDebugOutput(BOOL Enable)
{
	if (NULL == _PXDebug) {
		return FALSE;
	}
	if (Enable) {
		if (NULL == _pxdbgSystemDebugOutput) { 
			_pxdbgSystemDebugOutput = new XDbgSystemDebugOutput(); 
			if (NULL == _pxdbgSystemDebugOutput) {
				return FALSE;
			}
		}
		return _PXDebug->Attach(_pxdbgSystemDebugOutput);
	} else {
		if (NULL == _pxdbgSystemDebugOutput) {
			return TRUE;
		} else {
			return _PXDebug->Detach(_pxdbgSystemDebugOutput);
		}
	}
}

__inline 
void
XDbgAttach(IXDbgOutput *pOutput)
{
	if (NULL != _PXDebug) {
		_PXDebug->Attach(pOutput);
	}
}

__inline 
void
XDbgDetach(IXDbgOutput *pOutput)
{
	if (NULL != _PXDebug) {
		_PXDebug->Detach(pOutput);
	}
}

__inline 
BOOL 
XDbgLoadSettingsFromRegistry(
	LPCTSTR szRegPath, 
	HKEY hRootKey = HKEY_LOCAL_MACHINE)
{
	CONST DWORD SystemOutputFlags =     0x00000001;
	CONST DWORD ConsoleOutputFlags =    0x00000002;
	CONST DWORD EventTraceOutputFlags = 0x00000004;

	HKEY hKey = (HKEY)INVALID_HANDLE_VALUE;
	LONG lResult = ::RegOpenKeyEx(hRootKey, szRegPath, 0, KEY_READ, &hKey);
	if (ERROR_SUCCESS != lResult) {
		return FALSE;
	}

	DWORD cbValue;
	DWORD dwValueType;
	DWORD dwDebugLevel;

	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(hKey, 
		_T("DebugOutputLevel"), 
		NULL, 
		&dwValueType,
		(LPBYTE)&dwDebugLevel,
		&cbValue);

	if (ERROR_SUCCESS == lResult) {
		XDbgSetOutputLevel(dwDebugLevel);
	}

	DWORD dwDebugOutputFlags;
	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(
		hKey, 
		_T("DebugOutputFlags"), 
		NULL, 
		&dwValueType,
		(LPBYTE)&dwDebugOutputFlags,
		&cbValue);

	if (ERROR_SUCCESS == lResult) {
		if (dwDebugOutputFlags & SystemOutputFlags) {
			XDbgEnableSystemDebugOutput(TRUE);
		} else if (dwDebugOutputFlags & ConsoleOutputFlags) {
		} else if (dwDebugOutputFlags & EventTraceOutputFlags) {
		}
	}

	DWORD dwDebugModuleFlags;
	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(
		hKey,
		_T("DebugModuleFlags"),
		NULL,
		&dwValueType,
		(LPBYTE)&dwDebugModuleFlags,
		&cbValue);

	if (ERROR_SUCCESS == lResult) {
		XDbgSetModuleFlags(dwDebugModuleFlags);
	}

	DWORD dwDebugLibraryFlags;
	cbValue = sizeof(DWORD);
	lResult = ::RegQueryValueEx(
		hKey,
		_T("DebugLibraryFlags"),
		NULL,
		&dwValueType,
		(LPBYTE)&dwDebugLibraryFlags,
		&cbValue);

	if (ERROR_SUCCESS == lResult) {
		XDbgSetLibraryFlags(dwDebugLibraryFlags);
	}

	lResult = ::RegCloseKey(hKey);
	_ASSERTE(ERROR_SUCCESS == lResult);

	return TRUE;
}

#ifdef XDBG_LIBRARY_MODULE_FLAG

/* If LIBRARY_MODULE_FLAG is defined, redirect it */
static const DWORD _XDbgModuleFlag = 0x40000000;
static const DWORD _XDbgLibraryModuleFlag = XDBG_LIBRARY_MODULE_FLAG;

#else /* XDBG_LIBRARY_MODULE_FLAG */

#ifndef XDBG_MODULE_FLAG

static const DWORD _XDbgModuleFlag = 0x80000000;
#define XDBG_MODULE_FLAG 0x80000000

#else /* XDBG_MODULE_FLAG_DEFINED */

static const DWORD _XDbgModuleFlag = XDBG_MODULE_FLAG;

#endif /* XDBG_MODULE_FLAG_DEFINED */

#endif /* XDBG_LIBRARY_MODULE_FLAG */

#define XDBG_TO_STR2(x) #x
#define XDBG_TO_STR(x) XDBG_TO_STR2(x)
#define XDBG_MODULE_FLAG_T 0x0001
#define XDBG_MODULE_FLAG_STR  XDBG_TO_STR(XDBG_MODULE_FLAG)
#pragma message(XDBG_MODULE_FLAG_STR)

#define XDBG_USE_INLINE_FUNCTION
#ifdef XDBG_USE_INLINE_FUNCTION

static BOOL _XDbgIsEnabled(DWORD dwLevel)
{
	if (_PXDebug) {
		if (dwLevel <= _PXDebug->dwOutputLevel) {
#ifndef XDBG_LIBRARY_MODULE_FLAG
			if (_XDbgModuleFlag & _PXDebug->dwEnabledModules)
			{
				return TRUE;
			}
#else
			if (((_XDbgModuleFlag & _PXDebug->dwEnabledModules) == _XDbgModuleFlag) ||
				((_XDbgModuleFlag & 0x40000000) == _XDbgModuleFlag) && 
				((_XDbgLibraryModuleFlag & _PXDebug->dwEnabledLibraryFlags) == 
				_XDbgLibraryModuleFlag))
			{
				return TRUE;
			}
#endif
		}
	}
	return FALSE;
}

#define XDEBUG_IS_ENABLED _XDbgIsEnabled

#else /* _XDBG_USE_INLINE_FUCTION */

#define XDEBUG_IS_ENABLED(_level_) \
	(_PXDebug && \
	(_level_  <= _PXDebug->dwOutputLevel) && \
	(_xdbgModuleFlag & _PXDebug->dwEnabledModules))

#endif /* _XDBG_USE_INLINE_FUCTION */

#define XDBG_REMOVE_FILE_PATH
#ifdef XDBG_REMOVE_FILE_PATH

static 
DWORD_PTR 
_XDbgFilePathOffset = 0;

static 
__inline 
LPCTSTR 
XDbgRemoveFilePath(LPCTSTR szFormat)
{
	// ignore non _FT formats
	if (szFormat[0] == _T('\0') || (
		szFormat[1] != _T(':') && szFormat[1] != _T('\\'))) {
		return szFormat;
	}
	if (_XDbgFilePathOffset > 0) { 
		LPCTSTR pszFmt = szFormat + _XDbgFilePathOffset;
		return pszFmt; 
	}
	LPCTSTR p = szFormat + 2; // skip c: or \\ (unc path)
	while (*p != _T(':')) ++p;
	while (*p != _T('\\')) --p;
	++p; // remove backslash (\)
	_XDbgFilePathOffset = p - szFormat;
	LPCTSTR pszFmt = szFormat + _XDbgFilePathOffset;
	return pszFmt;
}
#else
#define XDbgRemoveFilePath(x) x
#endif // XDBG_REMOVE_FILE_PATH

static 
BOOL 
XDebugPrintf(DWORD dwLevel, LPCTSTR szFormat, ...)
{
	if (XDEBUG_IS_ENABLED(dwLevel)) {
		va_list ap;
		va_start(ap, szFormat);
		_PXDebug->VPrintf(dwLevel, XDbgRemoveFilePath(szFormat), ap);
		va_end(ap);
	}
	return TRUE;
}

#define XDEBUGV(_FuncName_,_l_) \
	static BOOL _FuncName_ (LPCTSTR szFormat, ...) \
{ \
	if (XDEBUG_IS_ENABLED(_l_)) { \
	va_list ap; \
	va_start(ap, szFormat); \
	_PXDebug->VPrintf(_l_, XDbgRemoveFilePath(szFormat), ap); \
	va_end(ap); \
	} \
	return TRUE; \
}

#define XDEBUGV_SYSERR(_FuncName_,_l_) \
	static BOOL _FuncName_ (LPCTSTR szFormat, ...) \
{ \
	if (XDEBUG_IS_ENABLED(_l_)) { \
	va_list ap; \
	va_start(ap, szFormat); \
	_PXDebug->VPrintfErr(_l_, XDbgRemoveFilePath(szFormat), ::GetLastError(), ap); \
	va_end(ap); \
	} \
	return TRUE; \
} \

#define XDEBUGV_USERERR(_FuncName_,_l_) \
	static BOOL _FuncName_ (DWORD dwError, LPCTSTR szFormat, ...) \
{ \
	if (XDEBUG_IS_ENABLED(_l_)) { \
	va_list ap; \
	va_start(ap, szFormat); \
	_PXDebug->VPrintfErr(_l_, XDbgRemoveFilePath(szFormat), dwError, ap); \
	va_end(ap); \
	} \
	return TRUE; \
}

#define XVDEBUGV(_FuncName_, _l_) \
	static BOOL _FuncName_ (LPCTSTR szFormat, va_list ap) \
{ \
	if (XDEBUG_IS_ENABLED(_l_)) { \
	_PXDebug->VPrintf(_l_, XDbgRemoveFilePath(szFormat), ap); \
	} \
	return TRUE; \
}

#define BEGIN_DBGPRT_BLOCK(_l_) \
	{ if (XDEBUG_IS_ENABLED(_l_)) {

#define END_DBGPRT_BLOCK() }}

#define BEGIN_DBGPRT_BLOCK_ALWAYS() BEGIN_DBGPRT_BLOCK(XDebug::OL_ALWAYS)
#define BEGIN_DBGPRT_BLOCK_ERR() BEGIN_DBGPRT_BLOCK(XDebug::OL_ERR)
#define BEGIN_DBGPRT_BLOCK_WARN() BEGIN_DBGPRT_BLOCK(XDebug::OL_WARNING)
#define BEGIN_DBGPRT_BLOCK_INFO() BEGIN_DBGPRT_BLOCK(XDebug::OL_INFO)
#define BEGIN_DBGPRT_BLOCK_TRACE() BEGIN_DBGPRT_BLOCK(XDebug::OL_TRACE)
#define BEGIN_DBGPRT_BLOCK_NOISE() BEGIN_DBGPRT_BLOCK(XDebug::OL_NOISE)

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

#ifdef NO_XDEBUG
#define DebugPrintf __noop
#define DPAny		__noop
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

#else /* NO_XDEBUG */
#define DebugPrintf XDebugPrintf
#define DPAny		XDebugPrintf
#define DPAlways    XDEBUG_IS_ENABLED(XDebug::OL_ALWAYS) && XDebugAlways
#define DPError     XDEBUG_IS_ENABLED(XDebug::OL_ERROR) && XDebugError
#define DPErrorEx   XDEBUG_IS_ENABLED(XDebug::OL_ERROR) && XDebugErrorEx
#define DPErrorEx2  XDEBUG_IS_ENABLED(XDebug::OL_ERROR) && XDebugErrorExUser
#define DPWarning   XDEBUG_IS_ENABLED(XDebug::OL_WARNING) && XDebugWarning
#define DPWarningEx XDEBUG_IS_ENABLED(XDebug::OL_WARNING) && XDebugWarningEx
#define DPWarningEx2 XDEBUG_IS_ENABLED(XDebug::OL_WARNING) && XDebugWarningExUser
#define DPInfo      XDEBUG_IS_ENABLED(XDebug::OL_INFO) && XDebugInfo
#define DPTrace     XDEBUG_IS_ENABLED(XDebug::OL_TRACE) && XDebugTrace
#define DPNoise     XDEBUG_IS_ENABLED(XDebug::OL_NOISE) && XDebugNoise

#endif /* NO_XDEBUG */

#define DBGPRT_ERR DPError
#define DBGPRT_WARN DPWarning
#define DBGPRT_INFO DPInfo
#define DBGPRT_NOISE DPNoise
#define DBGPRT_TRACE DPTrace
#define DBGPRT_ERR_EX DPErrorEx
#define DBGPRT_WARN_EX DPWarningEx
#define DBGPRT_LEVEL DPAny

#define CHARTOHEX(dest, ch) \
	if ((ch) <= 9) { dest = TCHAR(CHAR(ch + '0')); } \
	else           { dest = TCHAR(CHAR(ch - 0xA + 'A')); }

template <typename T>
void
DPType(
	DWORD dwLevel, 
	const T* lpTypedData, 
	DWORD cbSize)
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
	*psz = TEXT('\n');
	++psz;
	*psz = TEXT('\0');

	XDebugPrintf(dwLevel, szBuffer);
}

#undef CHARTOHEX

#define _LINE_STR3_(x) #x
#define _LINE_STR2_(x) _LINE_STR3_(x)
#define _LINE_STR_ _LINE_STR2_(__LINE__)

#ifndef XDBG_USE_FILENAME
	#ifdef _DEBUG
		#define XDBG_USE_FILENAME
	#endif
#endif
#ifndef XDBG_USE_FILENAME
	#ifdef DBG
		#define XDBG_USE_FILENAME
	#endif
#endif

// XDBG_USE_FUNC is used by default
// define XDBG_NO_FUNC to disable it
#ifdef XDBG_NO_FUNC
	#undef XDBG_USE_FUNC
#elif !defined(XDBG_USE_FUNC)
	#define XDBG_USE_FUNC
#endif

#ifdef XDBG_USE_FILENAME
#ifndef XDBG_FILENAME
#define XDBG_FILENAME_PREFIX _T(__FILE__) _T("(") _T(_LINE_STR_) _T("): ") 
#else
#define XDBG_FILENAME_PREFIX _T(XDBG_FILENAME) _T("(") _T(_LINE_STR_) _T("): ") 
#endif
#else
#define XDBG_FILENAME_PREFIX 
#endif

#ifdef XDBG_USE_FUNC
#define XDBG_FUNC_PREFIX _T(__FUNCTION__) _T(": ")
#else
#define XDBG_FUNC_PREFIX
#endif

#define _FT(x) XDBG_FILENAME_PREFIX XDBG_FUNC_PREFIX _T(x)

#endif /* _XDEBUG_H_ */

