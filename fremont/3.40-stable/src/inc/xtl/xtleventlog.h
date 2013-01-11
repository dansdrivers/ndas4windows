#pragma once
#include "xtldef.h"
#include "xtlautores.h"

namespace XTL
{

template <bool t_bManaged>
class CEventLogT
{
protected:
	
	AutoEventSourceHandle m_hEventLog;

public:

	CEventLogT(HANDLE hEventSource = NULL) throw() : m_hEventLog(hEventSource)
	{
	}

	CEventLogT& operator=(HANDLE hEventSource) throw()
	{
		m_hEventLog.Attach(hEventSource);
		return *this;
	}

	~CEventLogT()
	{
		if (!t_bManaged) m_hEventLog.Detach();
	}

	void Attach(HANDLE hNewEventSource) throw()
	{
		m_hEventLog.Attach(hNewEventSource);
	}

	HANDLE Detach() throw()
	{
		return m_hEventLog.Detach();
	}

	CEventLogT(LPCTSTR szSourceName)
	{
		XTLVERIFY(::RegisterEventSource(szSourceName));
	}

	BOOL RegisterEventSource(LPCTSTR szSourceName)
	{
		m_hEventLog = ::RegisterEventSource(NULL, szSourceName);
		return (m_hEventLog.IsInvalid());
	}

	BOOL DeregisterEventSource()
	{
		XTLASSERT(!m_hEventLog.IsInvalid());
		if (!::DeregisterEventSource(m_hEventLog))
		{
			return FALSE;
		}
		m_hEventLog.Detach();
		return TRUE;
	}

	BOOL LogMessageEx(WORD wType, WORD wCategory, DWORD dwEventID, PSID lpUserSid, WORD wNumStrings, DWORD dwDataSize, LPCTSTR* lpStrings, LPVOID lpRawData)
	{
		XTLENSURE_RETURN_BOOL(!m_hEventLog.IsInvalid());
		return ::ReportEvent(m_hEventLog, wType, wCategory, dwEventID, 
			lpUserSid, wNumStrings, dwDataSize, lpStrings, lpRawData);
	}

	BOOL LogMessage(WORD wType, DWORD dwEventID, PSID lpUserSid, WORD wNumStrings, DWORD dwDataSize, LPCTSTR* lpStrings, LPVOID lpRawData)
	{
		return LogMessageEx(wType, 0, dwEventID, lpUserSid, wNumStrings, dwDataSize, lpStrings, lpRawData);
	}

	BOOL LogMessage(WORD wType, DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
	{
		return LogMessage(wType, dwEventID, NULL, wNumStrings, 0, lpStrings, NULL); 
	}

	BOOL LogMessage(WORD wType, DWORD dwEventID, WORD wNumStrings, DWORD dwDataSize, LPCTSTR* lpStrings, LPVOID lpRawData)
	{
		return LogMessage(wType, dwEventID, NULL, wNumStrings, dwDataSize, lpStrings, lpRawData); 
	}

	BOOL LogMessage(WORD wType, DWORD dwEventID, LPCTSTR lpString = NULL)
	{
		if (lpString != NULL)
		{
			return LogMessage(wType, dwEventID, NULL, 1, 0, &lpString, NULL);
		}
		else
		{
			return LogMessage(wType, dwEventID, NULL, 0, 0, NULL, NULL);
		}
	}

	BOOL LogSuccess(DWORD dwEventID, LPCTSTR lpString = NULL)
	{
		return LogMessage(EVENTLOG_SUCCESS, dwEventID, lpString);	
	}
	
	BOOL LogSuccess(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
	{
		return LogMessage(EVENTLOG_SUCCESS, dwEventID, wNumStrings, lpStrings); 
	}

	BOOL LogError(DWORD dwEventID, LPCTSTR lpString = NULL)
	{
		return LogMessage(EVENTLOG_ERROR_TYPE, dwEventID, lpString);	
	}
	
	BOOL LogError(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
	{
		return LogMessage(EVENTLOG_ERROR_TYPE, dwEventID, wNumStrings, lpStrings); 
	}

	BOOL LogWarning(DWORD dwEventID, LPCTSTR lpString = NULL)
	{
		return LogMessage(EVENTLOG_WARNING_TYPE, dwEventID, lpString);	
	}
	
	BOOL LogWarning(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
	{
		return LogMessage(EVENTLOG_WARNING_TYPE, dwEventID, wNumStrings, lpStrings); 
	}

	BOOL LogInformation(DWORD dwEventID, LPCTSTR lpString = NULL)
	{
		return LogMessage(EVENTLOG_INFORMATION_TYPE, dwEventID, lpString);	
	}

	BOOL LogInformation(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
	{
		return LogMessage(EVENTLOG_INFORMATION_TYPE, dwEventID, wNumStrings, lpStrings); 
	}

	static BOOL InstallEventSource(
		LPCTSTR szSourceName, LPCTSTR szFileName, 
		DWORD dwTypes = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE);
	static BOOL RemoveEventSource(LPCTSTR szSourceName);
	static BOOL GetInstalledMessageModulePath(LPCTSTR szSourceName, LPTSTR lpMessageFilePath, DWORD cchPath);

	// static LPTSTR GetTranslatedString(HMODULE hMessageModule, DWORD dwEventID, WORD wNumStrings = 0, LPCTSTR* lpStrings = NULL);
	static LPTSTR* CreateLpStrings(DWORD nStrings, ...);
};

template <bool managed>
inline LPTSTR* 
CEventLogT<managed>::CreateLpStrings(DWORD nStrings, ...)
{
	va_list marker;

	DWORD cbhead = (nStrings + 1) * sizeof(LPTSTR);
	
	AutoProcessHeap lpBuffer = ::HeapAlloc(::GetProcessHeap(), 0, cbhead);
	XTLENSURE_RETURN_T(!lpBuffer.IsInvalid(), NULL);

	LPTSTR* lpHeader = static_cast<LPTSTR*>(static_cast<LPVOID>(lpBuffer));
	size_t* lpCch = static_cast<size_t*>(static_cast<LPVOID>(lpBuffer));

	size_t cchData = 0;
	va_start(marker, nStrings);
	for (DWORD i = 0; i < nStrings; i++) 
	{
		LPCTSTR lpStr = va_arg(marker,LPCTSTR);
		size_t cch;
		HRESULT hr;
		XTLVERIFY( SUCCEEDED(hr = ::StringCchLength(lpStr, 1024, &cch)) );
		cchData += cch + 1;
		lpCch[i] = cch;
	}
	lpCch[nStrings] = 0;
	va_end(marker);

	DWORD cball = cbhead + cchData * sizeof(TCHAR);

	lpBuffer = ::HeapReAlloc(::GetProcessHeap(), 0, lpBuffer.Detach(), cball);
	XTLENSURE_RETURN_T(!lpBuffer.IsInvalid(), NULL);

	lpHeader = static_cast<LPTSTR*>(static_cast<LPVOID>(lpBuffer));
	lpCch = static_cast<size_t*>(static_cast<LPVOID>(lpBuffer));

	LPTSTR lpNextStrBuffer = 
		reinterpret_cast<LPTSTR>(
			reinterpret_cast<LPBYTE>(static_cast<LPVOID>(lpBuffer)) + cbhead);

	va_start(marker, nStrings);
	for (DWORD i = 0; i < nStrings; i++) 
	{
		size_t cch = lpCch[i];
		lpHeader[i] = lpNextStrBuffer;
		LPCTSTR lpStr = va_arg(marker,PTCHAR);
		HRESULT hr;
		XTLVERIFY( SUCCEEDED(hr = ::StringCchCopy(lpNextStrBuffer, cch + 1, lpStr)) );
		if (!SUCCEEDED(hr)) 
		{
			va_end(marker);
			return NULL;
		}
		lpNextStrBuffer += cch + 1;
	}
	va_end(marker);

	return static_cast<LPTSTR*>(lpBuffer.Detach());
}

typedef CEventLogT<true> CEventLog;
typedef CEventLogT<false> CEventLogHandle;

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

template <bool m_bManaged>
inline BOOL 
CEventLogT<m_bManaged>::GetInstalledMessageModulePath(
	LPCTSTR szSourceName, 
	LPTSTR lpMessageFilePath, 
	DWORD cchPath)
{
	const LPCTSTR REGSTR_PATH_APPLICATION_EVENTLOG = REGSTR_PATH_SERVICES _T("\\Eventlog\\Application");
	const LPCTSTR REGSTR_PATH_SYSTEM_EVENTLOG = REGSTR_PATH_SERVICES _T("\\Eventlog\\System");

	TCHAR szKey[260];

	XTLENSURE_RETURN_BOOL(SUCCEEDED(
		::StringCchCopy(szKey, RTL_NUMBER_OF(szKey), REGSTR_PATH_APPLICATION_EVENTLOG)));
	XTLENSURE_RETURN_BOOL(SUCCEEDED(
		::StringCchCat(szKey, RTL_NUMBER_OF(szKey), _T("\\"))));
	XTLENSURE_RETURN_BOOL(SUCCEEDED(
		::StringCchCat(szKey, RTL_NUMBER_OF(szKey), szSourceName)));

	AutoKeyHandle hKey;
	LONG ret = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		szKey,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != ret) 
	{
		return FALSE;
	}

	DWORD cbData = cchPath * sizeof(TCHAR);

	ret = ::RegQueryValueEx(
		hKey,
		_T("EventMessageFile"),
		NULL,
		NULL,
		(LPBYTE) lpMessageFilePath,
		&cbData);

	return (ERROR_SUCCESS == ret);
}

template <bool t_bManaged>
inline BOOL 
CEventLogT<t_bManaged>::InstallEventSource(
	LPCTSTR szSourceName, 
	LPCTSTR szFileName, 
	DWORD dwTypes)
{
	const LPCTSTR REGSTR_PATH_APPLICATION_EVENTLOG = REGSTR_PATH_SERVICES _T("\\Eventlog\\Application");
	const LPCTSTR REGSTR_PATH_SYSTEM_EVENTLOG = REGSTR_PATH_SERVICES _T("\\Eventlog\\System");

	TCHAR szKey[260];
	LONG lRet = ERROR_SUCCESS;

	XTLENSURE_RETURN_BOOL(SUCCEEDED(
		::StringCchCopy(szKey, RTL_NUMBER_OF(szKey), REGSTR_PATH_APPLICATION_EVENTLOG)));
	XTLENSURE_RETURN_BOOL(SUCCEEDED(
		::StringCchCat(szKey, RTL_NUMBER_OF(szKey), _T("\\"))));
	XTLENSURE_RETURN_BOOL(SUCCEEDED(
		::StringCchCat(szKey, RTL_NUMBER_OF(szKey), szSourceName)));

	AutoKeyHandle hKey;
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
	}

	hKey.Release();

	// Add the service to the "Sources" value

	lRet =	::RegOpenKeyEx( 
		HKEY_LOCAL_MACHINE,	// handle of open key 
		REGSTR_PATH_APPLICATION_EVENTLOG, // address of name of subkey to open 
		0,					// reserved 
		KEY_ALL_ACCESS,		// security access mask 
		&hKey				// address of handle of open key 
		);
	if (lRet == ERROR_SUCCESS) 
	{
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

		if( lRet == ERROR_SUCCESS ) 
		{
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
			if( lRet == ERROR_SUCCESS ) 
			{
				_ASSERTE(dwType == REG_MULTI_SZ);

				// check whether this service is already a known source
				register LPTSTR p = LPTSTR(Buffer);
				for(; *p; p += _tcslen(p)+1 ) 
				{
					if( _tcscmp(p, szSourceName) == 0 )
						break;
				}
				if( ! * p ) 
				{
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
	}

	return TRUE;
}

template <bool t_bManaged>
inline BOOL 
CEventLogT<t_bManaged>::RemoveEventSource(LPCTSTR szSourceName)
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
	if( lRet == ERROR_SUCCESS ) 
	{
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

		if( lRet == ERROR_SUCCESS ) 
		{
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
			if( lRet == ERROR_SUCCESS ) 
			{
				_ASSERTE(dwType == REG_MULTI_SZ);

				// check whether this service is already a known source
				register LPTSTR p = LPTSTR(Buffer);
				register LPTSTR pNew = LPTSTR(NewBuffer);
				BOOL bNeedSave = FALSE;	// assume the value is already correct
				for(; *p; p += _tcslen(p)+1) 
				{
					// except ourself: copy the source string into the destination
					if( _tcscmp(p, szSourceName) != 0 ) 
					{
						StringCbCopy(pNew, dwSize, p);
						pNew += _tcslen(pNew)+1;
					}
					else 
					{
						bNeedSave = TRUE;		// *this* application found
						dwSize -= (DWORD) _tcslen(p)+1;	// new size of value
					}
				}
				if( bNeedSave ) 
				{
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
	}
	return TRUE;
}

} // namespace XTL
