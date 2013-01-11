/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "eventlog.h"
#include <regstr.h>
#include <crtdbg.h>

namespace ximeta {

// const DWORD CEventLog::MAX_SOURCE_NAME = 255;
const LPCTSTR CEventLog::REGSTR_PATH_APPLICATION_EVENTLOG = 
	REGSTR_PATH_SERVICES _T("\\Eventlog\\Application");

const LPCTSTR CEventLog::REGSTR_PATH_SYSTEM_EVENTLOG = 
	REGSTR_PATH_SERVICES _T("\\Eventlog\\System");

class CEventLogSingleton : public CEventLog
{
	BOOL m_fRegistered;

public:
	CEventLogSingleton(LPCTSTR szSourceName) :
	  m_fRegistered(FALSE),
	  CEventLog(szSourceName)
	{
		m_fRegistered = CEventLog::RegisterEventSource();
	}

	virtual ~CEventLogSingleton()
	{
		if (m_fRegistered) {
			BOOL fSuccess = CEventLog::DeregisterEventSource();
			_ASSERTE(fSuccess);
		}
	}

	BOOL LogMessage(
		WORD wType, 
		DWORD dwEventID, 
		PSID lpUserSid, 
		WORD wNumStrings, 
		DWORD dwDataSize, 
		LPCTSTR *lpStrings, 
		LPVOID lpRawData)
	{
		if (m_fRegistered) {
			return CEventLog::LogMessage(
				wType, 
				dwEventID, 
				lpUserSid, 
				wNumStrings, 
				dwDataSize, 
				lpStrings, 
				lpRawData);
		} else {
			return FALSE;
		}
	}

};

CEventLog::CEventLog(LPCTSTR szSourceName) :
	m_hEventLog(NULL),
	m_wEventCategory(0),
	m_hMessageFile(NULL),
	m_bUseStdOut(FALSE),
	m_bUseEventLog(TRUE)
{
	HRESULT hr;
	hr = StringCchCopy(m_szSourceName, CEventLog::MAX_SOURCE_NAME, szSourceName);
	m_szMessageFilePath[0] = _T('\0');

	// TODO: Error handle InitCriticalSection
	::InitializeCriticalSectionAndSpinCount(&m_csEventWriter, 0x08000400);
}

CEventLog::~CEventLog()
{
	::DeleteCriticalSection(&m_csEventWriter);

	if (NULL != m_hMessageFile) {
		::FreeLibrary(m_hMessageFile);
	}
}

BOOL CEventLog::RegisterEventSource()
{
	m_hEventLog = ::RegisterEventSource(NULL, m_szSourceName);
	
	if (NULL == m_hEventLog)
		return FALSE;
	
	return TRUE;
}

BOOL CEventLog::DeregisterEventSource()
{
	return ::DeregisterEventSource(m_hEventLog);
}

BOOL CEventLog::LogMessageToStdOut(
	WORD wType, 
	DWORD dwEventID, 
	PSID lpUserSid, 
	WORD wNumStrings, 
	DWORD dwDataSize, 
	LPCTSTR *lpStrings, 
	LPVOID lpRawData)
{
	LPVOID lpBuffer;
	DWORD dwBufferLength, dwBytesWritten;
	BOOL fSuccess;

	if (NULL == m_hMessageFile) {
		m_hMessageFile = LoadLibraryEx(
			m_szMessageFilePath, 
			NULL, 
			LOAD_LIBRARY_AS_DATAFILE);

		if (NULL == m_hMessageFile) {
			return FALSE;
		}
	}

	//DWORD_PTR* pdwStrings = new DWORD_PTR[wNumStrings + 1];
	//for (WORD i = 0; i < wNumStrings; i++) {
	//	pdwStrings[i] = reinterpret_cast<DWORD_PTR>(lpStrings[i]);
	//}
	//pdwStrings[wNumStrings] = NULL;

	dwBufferLength = ::FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_HMODULE |
		FORMAT_MESSAGE_ARGUMENT_ARRAY,
		m_hMessageFile,
		dwEventID,
		0,
		(LPTSTR) &lpBuffer,
		0,
		(va_list*) lpStrings
		);

	if (dwBufferLength == 0) {
		return FALSE;
	}


	//
	// Prevents message mess
	// as several threads may call this log message simultaneously
	//
	::EnterCriticalSection(&m_csEventWriter);

	// we do not need CR(\n) as the message has CR at the end.
	_ftprintf(stdout, 
		_T("%s:%08X:%s\n"),
		(wType == EVENTLOG_SUCCESS) ?          _T("SUCCESS") :
		(wType == EVENTLOG_ERROR_TYPE) ?       _T("ERROR  ") :
		(wType == EVENTLOG_WARNING_TYPE) ?     _T("WARNING") :
		(wType == EVENTLOG_INFORMATION_TYPE) ? _T("INFO   ") :
		_T("UNKNOWN"),
		dwEventID,
		lpBuffer);

	::LeaveCriticalSection(&m_csEventWriter);

	/*
	fSuccess = ::WriteFile(
		::GetStdHandle(STD_OUTPUT_HANDLE),
		lpBuffer,
		dwBufferLength,
		&dwBytesWritten,
		NULL);
	*/

//	delete[] pdwStrings;
	::LocalFree(lpBuffer);
		
	return TRUE;
}

BOOL CEventLog::LogMessage(
	WORD wType, 
	DWORD dwEventID, 
	PSID lpUserSid, 
	WORD wNumStrings, 
	DWORD dwDataSize, 
	LPCTSTR *lpStrings, 
	LPVOID lpRawData)
{
	BOOL fSuccess = FALSE;

	if (m_bUseStdOut) {
		fSuccess |= LogMessageToStdOut(
			wType,
			dwEventID,
			lpUserSid,
			wNumStrings,
			dwDataSize,
			lpStrings,
			lpRawData);
	}
	
	if (m_bUseEventLog) {
		fSuccess |= ::ReportEvent(
			m_hEventLog, 
			wType, 
			m_wEventCategory, 
			dwEventID, 
			lpUserSid, 
			wNumStrings, 
			dwDataSize, 
			lpStrings, 
			lpRawData);
	}

	if (!m_bUseEventLog && !m_bUseStdOut) {
		fSuccess = TRUE;
	}

	return fSuccess;
}

BOOL CEventLog::RegisterToApplicationLog(
	LPCTSTR szSourceName, 
	LPCTSTR szFileName, 
	DWORD dwTypes)
{
	TCHAR szKey[260];
	HKEY hKey = 0;
	LONG lRet = ERROR_SUCCESS;
	HRESULT hr;

	hr = ::StringCchPrintf(
		szKey, 
		260, 
		_T("%s\\%s"), 
		REGSTR_PATH_APPLICATION_EVENTLOG, 
		szSourceName);

	// Create a key for that application and insert values for
	// "EventMessageFile" and "TypesSupported"
	if( ::RegCreateKey(HKEY_LOCAL_MACHINE, szKey, &hKey) == ERROR_SUCCESS )
	{
		lRet = ::RegSetValueEx(
			hKey,						// handle of key to set value for
			TEXT("EventMessageFile"),	// address of value to set
			0,							// reserved
			REG_EXPAND_SZ,				// flag for value type
			(CONST BYTE*)szFileName,	// address of value data
			(DWORD) _tcslen(szFileName) + 1	// size of value data
			);

		// Set the supported types flags.
		lRet = ::RegSetValueEx(
			hKey,					// handle of key to set value for
			TEXT("TypesSupported"),	// address of value to set
			0,						// reserved
			REG_DWORD,				// flag for value type
			(CONST BYTE*)&dwTypes,	// address of value data
			sizeof(DWORD)			// size of value data
			);
		::RegCloseKey(hKey);
	}

	// Add the service to the "Sources" value

	lRet =	::RegOpenKeyEx( 
		HKEY_LOCAL_MACHINE,	// handle of open key 
		REGSTR_PATH_APPLICATION_EVENTLOG, // address of name of subkey to open 
		0,					// reserved 
		KEY_ALL_ACCESS,		// security access mask 
		&hKey				// address of handle of open key 
		);
	if( lRet == ERROR_SUCCESS ) {
		DWORD dwSize;

		// retrieve the size of the needed value
		lRet =	::RegQueryValueEx(
			hKey,			// handle of key to query 
			TEXT("Sources"),// address of name of value to query 
			0,				// reserved 
			0,				// address of buffer for value type 
			0,				// address of data buffer 
			&dwSize			// address of data buffer size 
			);

		if( lRet == ERROR_SUCCESS ) {
			DWORD dwType;
			DWORD dwNewSize = dwSize+_tcslen(szSourceName)+1;
			LPBYTE Buffer = LPBYTE(::GlobalAlloc(GPTR, dwNewSize));

			lRet =	::RegQueryValueEx(
				hKey,			// handle of key to query 
				TEXT("Sources"),// address of name of value to query 
				0,				// reserved 
				&dwType,		// address of buffer for value type 
				Buffer,			// address of data buffer 
				&dwSize			// address of data buffer size 
				);
			if( lRet == ERROR_SUCCESS ) {
				_ASSERTE(dwType == REG_MULTI_SZ);

				// check whether this service is already a known source
				register LPTSTR p = LPTSTR(Buffer);
				for(; *p; p += _tcslen(p)+1 ) {
					if( _tcscmp(p, szSourceName) == 0 )
						break;
				}
				if( ! * p ) {
					// We're standing at the end of the stringarray
					// and the service does still not exist in the "Sources".
					// Now insert it at this point.
					// Note that we have already enough memory allocated
					// (see GlobalAlloc() above). We also don't need to append
					// an additional '\0'. This is done in GlobalAlloc() above
					// too.
					StringCbCopy(p, dwNewSize, szSourceName);

					// OK - now store the modified value back into the
					// registry.
					lRet =	::RegSetValueEx(
						hKey,			// handle of key to set value for
						TEXT("Sources"),// address of value to set
						0,				// reserved
						dwType,			// flag for value type
						Buffer,			// address of value data
						dwNewSize		// size of value data
						);
				}
			}

			::GlobalFree(HGLOBAL(Buffer));
		}

		::RegCloseKey(hKey);
	}

	return TRUE;
}

BOOL CEventLog::DeregisterFromApplicationLog(LPCTSTR szSourceName)
{
	TCHAR szKey[260];
	HKEY hKey = 0;
	LONG lRet = ERROR_SUCCESS;
	HRESULT hr;

	hr = StringCchPrintf(
		szKey, 
		260, 
		_T("%s\\%s"), 
		REGSTR_PATH_APPLICATION_EVENTLOG, 
		szSourceName);

	lRet = ::RegDeleteKey(HKEY_LOCAL_MACHINE, szKey);

	// now we have to delete the application from the "Sources" value too.
	lRet =	::RegOpenKeyEx( 
		HKEY_LOCAL_MACHINE,	// handle of open key 
		REGSTR_PATH_APPLICATION_EVENTLOG, // address of name of subkey to open 
		0,					// reserved 
		KEY_ALL_ACCESS,		// security access mask 
		&hKey				// address of handle of open key 
		);
	if( lRet == ERROR_SUCCESS ) {
		DWORD dwSize;

		// retrieve the size of the needed value
		lRet =	::RegQueryValueEx(
			hKey,			// handle of key to query 
			TEXT("Sources"),// address of name of value to query 
			0,				// reserved 
			0,				// address of buffer for value type 
			0,				// address of data buffer 
			&dwSize			// address of data buffer size 
			);

		if( lRet == ERROR_SUCCESS ) {
			DWORD dwType;
			LPBYTE Buffer = LPBYTE(::GlobalAlloc(GPTR, dwSize));
			LPBYTE NewBuffer = LPBYTE(::GlobalAlloc(GPTR, dwSize));

			lRet =	::RegQueryValueEx(
				hKey,			// handle of key to query 
				TEXT("Sources"),// address of name of value to query 
				0,				// reserved 
				&dwType,		// address of buffer for value type 
				Buffer,			// address of data buffer 
				&dwSize			// address of data buffer size 
				);
			if( lRet == ERROR_SUCCESS ) {
				_ASSERTE(dwType == REG_MULTI_SZ);

				// check whether this service is already a known source
				register LPTSTR p = LPTSTR(Buffer);
				register LPTSTR pNew = LPTSTR(NewBuffer);
				BOOL bNeedSave = FALSE;	// assume the value is already correct
				for(; *p; p += _tcslen(p)+1) {
					// except ourself: copy the source string into the destination
					if( _tcscmp(p, szSourceName) != 0 ) {
						StringCbCopy(pNew, dwSize, p);
						pNew += _tcslen(pNew)+1;
					} else {
						bNeedSave = TRUE;		// *this* application found
						dwSize -= (DWORD) _tcslen(p)+1;	// new size of value
					}
				}
				if( bNeedSave ) {
					// OK - now store the modified value back into the
					// registry.
					lRet =	::RegSetValueEx(
						hKey,			// handle of key to set value for
						TEXT("Sources"),// address of value to set
						0,				// reserved
						dwType,			// flag for value type
						NewBuffer,		// address of value data
						dwSize			// size of value data
						);
				}
			}

			::GlobalFree(HGLOBAL(Buffer));
			::GlobalFree(HGLOBAL(NewBuffer));
		}

		::RegCloseKey(hKey);
	}
	return TRUE;
}

//
// Description
//
// The GetRegisterdMessageFilePath returns the value of the
// registered message file from the following registry key:
//
// HKLM\System\CurrentControlSet\EventLog\<SourceName>\EventMessageFile
//
// Returns
//
// If the function succeeds, the return value is nonzero.
//
// If the function fails, the return value is zero. 
// To get extended error information, call GetLastError.
//

BOOL CEventLog::GetRegisteredMessageFilePath(
	LPCTSTR szSourceName, 
	LPTSTR lpMessageFilePath, 
	DWORD cchPath)
{
	TCHAR szKey[260];
	HKEY hKey = 0;
	LONG lRet = ERROR_SUCCESS;
	HRESULT hr;

	hr = ::StringCchPrintf(
		szKey, 
		260, 
		_T("%s\\%s"), 
		REGSTR_PATH_APPLICATION_EVENTLOG, 
		szSourceName);

	lRet = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		szKey,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lRet) {
		return FALSE;
	}

	DWORD cbData = cchPath * sizeof(TCHAR);

	lRet = ::RegQueryValueEx(
		hKey,
		_T("EventMessageFile"),
		NULL,
		NULL,
		(LPBYTE) lpMessageFilePath,
		&cbData);

	return (ERROR_SUCCESS == lRet);
}

//
// Description
//
// Create an array of strings to be used in event log inserts.
//
// Returns
//
// If the function succeeds, the return value is an allocated
// buffer with new operator. You have to deallocate the buffer
// after using returned buffer. 
//
// If the function fails, the return value is NULL. To get extended
// error information, call GetLastError.
//
// Example 
// 
// <CODE>
//
// LPTSTR* msgs = EvengLog::CreateLpStrings(2, _T("Hi"), _T("Second~~ "));
//
// ... LogMessage(..., 2, msgs);
//
// for (DWORD i = 0; i < 2; i++)
//   delete msgs[i];
// delete [] msgs;
//
// </CODE>
//
// FormatMessage arguments should be as below. This function
// is a utility one to make it easier.
//
// <CODE>
// TCHAR *args[3];
// TCHAR *txt;
//
// args[0] = new TCHAR[6];
// _tcscpy( args[0], _T("Hello") );
// args[1] = new TCHAR[6];
// _tcscpy( args[1], _T("World") );
// args[2] = NULL;
//
// FormatMessage(
//			  FORMAT_MESSAGE_ALLOCATE_BUFFER |
//			  FORMAT_MESSAGE_FROM_HMODULE |
//			  FORMAT_MESSAGE_ARGUMENT_ARRAY,
//			  hndSource, dwMessageID, 0, (LPTSTR)&txt, 0, (va_list *)args );
//
// </CODE>

LPTSTR* CEventLog::CreateLpStrings(IN DWORD nStrings, ...)
{
	LPTSTR* lpBuffer;
	PTCHAR lpStr;
	size_t cch;
	HRESULT hr;
	va_list marker;
	lpBuffer = new LPTSTR[nStrings + 1];
	if (NULL == lpBuffer)
		return NULL;
	va_start(marker, nStrings);
	__try {
		for (DWORD i = 0; i < nStrings; i++) {

			lpStr = va_arg(marker,PTCHAR);

			hr = StringCchLength(lpStr, 1024, &cch);
			if (!SUCCEEDED(hr)) {
				delete[] lpBuffer;
				return NULL;
			}

			lpBuffer[i] = new TCHAR[cch + 1];
			hr = StringCchCopy(lpBuffer[i], cch + 1, lpStr);
			if (!SUCCEEDED(hr)) {
				delete[] lpBuffer;
				return NULL;
			}
		}
		lpBuffer[nStrings] = NULL;
	}
	__finally {
		va_end(marker);
	}
	return lpBuffer;
}

}

BOOL
WINAPI
NdasLogEvent(
	WORD wType, 
	DWORD dwEventID, 
	PSID lpUserSid, 
	WORD wNumStrings, 
	DWORD dwDataSize, 
	LPCTSTR* lpStrings, 
	LPVOID lpRawData)
{
	static ximeta::CEventLogSingleton eventLog(_T("NDASSVC"));
	return eventLog.LogMessage(
		wType, 
		dwEventID, 
		lpUserSid, 
		wNumStrings, 
		dwDataSize, 
		lpStrings, 
		lpRawData);
}
