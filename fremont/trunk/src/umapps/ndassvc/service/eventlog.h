#pragma once
#include <windows.h>

namespace ximeta {

	class CEventLog
	{
	public:
		static const DWORD MAX_SOURCE_NAME = 255;
		static const LPCTSTR REGSTR_PATH_APPLICATION_EVENTLOG;
		static const LPCTSTR REGSTR_PATH_SYSTEM_EVENTLOG;

		static BOOL RegisterToApplicationLog(LPCTSTR szSourceName, LPCTSTR szFileName, DWORD dwTypes);
		static BOOL DeregisterFromApplicationLog(LPCTSTR szSourceName);
		static BOOL GetRegisteredMessageFilePath(LPCTSTR szSourceName, LPTSTR lpMessageFilePath, DWORD cchPath);
		static LPTSTR* CreateLpStrings(DWORD nStrings, ...);

	protected:
		HANDLE m_hEventLog;
		WORD m_wEventCategory;
		TCHAR m_szSourceName[MAX_SOURCE_NAME];
		
		HMODULE m_hMessageFile;
		TCHAR m_szMessageFilePath[MAX_PATH];

		BOOL m_bUseStdOut;
		BOOL m_bUseEventLog;

		CRITICAL_SECTION m_csEventWriter;

		BOOL RegisterEventSource();
		BOOL DeregisterEventSource();
		BOOL LogMessageToStdOut(WORD wType, DWORD dwEventID, PSID lpUserSid, WORD wNumStrings, DWORD dwDataSize, LPCTSTR* lpStrings, LPCVOID lpRawData);

	public:

		CEventLog(LPCTSTR szSourceName);
		virtual ~CEventLog();

		BOOL LogMessage(WORD wType, DWORD dwEventID, PSID lpUserSid, WORD wNumStrings, DWORD dwDataSize, LPCTSTR* lpStrings, LPCVOID lpRawData);

		BOOL LogMessage(WORD wType, DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
		{ return LogMessage(wType, dwEventID, NULL, wNumStrings, 0, lpStrings, NULL); }

		BOOL LogMessage(WORD wType, DWORD dwEventID, WORD wNumStrings, DWORD dwDataSize, LPCTSTR* lpStrings, LPCVOID lpRawData)
		{ return LogMessage(wType, dwEventID, NULL, wNumStrings, dwDataSize, lpStrings, lpRawData); }

		BOOL LogMessage(WORD wType, DWORD dwEventID, LPCTSTR lpString = NULL)
		{
			if (lpString != NULL)
				return LogMessage(wType, dwEventID, NULL, 1, 0, &lpString, NULL);
			else
				return LogMessage(wType, dwEventID, NULL, 0, 0, NULL, NULL);
		}

		BOOL LogSuccess(DWORD dwEventID, LPCTSTR lpString = NULL)
		{ return LogMessage(EVENTLOG_SUCCESS, dwEventID, lpString);	}
		BOOL LogSuccess(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
		{ return LogMessage(EVENTLOG_SUCCESS, dwEventID, wNumStrings, lpStrings); }

		BOOL LogError(DWORD dwEventID, LPCTSTR lpString = NULL)
		{ return LogMessage(EVENTLOG_ERROR_TYPE, dwEventID, lpString);	}
		BOOL LogError(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
		{ return LogMessage(EVENTLOG_ERROR_TYPE, dwEventID, wNumStrings, lpStrings); }

		BOOL LogWarning(DWORD dwEventID, LPCTSTR lpString = NULL)
		{ return LogMessage(EVENTLOG_WARNING_TYPE, dwEventID, lpString);	}
		BOOL LogWarning(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
		{ return LogMessage(EVENTLOG_WARNING_TYPE, dwEventID, wNumStrings, lpStrings); }

		BOOL LogInformation(DWORD dwEventID, LPCTSTR lpString = NULL)
		{ return LogMessage(EVENTLOG_INFORMATION_TYPE, dwEventID, lpString);	}
		BOOL LogInformation(DWORD dwEventID, WORD wNumStrings, LPCTSTR* lpStrings)
		{ return LogMessage(EVENTLOG_INFORMATION_TYPE, dwEventID, wNumStrings, lpStrings); }

		BOOL EventLogEnabled()
		{ 
			return m_bUseEventLog;
		}

		BOOL StdOutEnabled()
		{
			return m_bUseStdOut;
		}

		BOOL EnableEventLog(BOOL bEnable = TRUE)
		{
			BOOL success;

			m_bUseEventLog = bEnable;
			
			if (m_bUseEventLog) {
				success = RegisterEventSource();
			} else {
				success = DeregisterEventSource();
			}

			return success;
		}

		BOOL EnableStdOut(BOOL bEnable = TRUE)
		{
			if (bEnable && _T('\0') == m_szMessageFilePath[0] ) {
				// TODO: SetLastError cannot enable stdout when message file path is not set
				return FALSE;
			}
			m_bUseStdOut = bEnable;
			return TRUE;
		}

		BOOL SetMessageModule(LPCTSTR szMessageFilePath)
		{
			HRESULT hr;
			hr = ::StringCchCopy(m_szMessageFilePath, MAX_PATH, szMessageFilePath);
			if (!SUCCEEDED(hr)) {
				return FALSE;
			}
			// initialize message file handle
			// to load when LogMessage is called
			if (NULL != m_hMessageFile) {
				(VOID) ::FreeLibrary(m_hMessageFile);
			}
			m_hMessageFile = NULL;
			return TRUE;
		}
	};

}

BOOL
WINAPI
NdasLogEvent(
	WORD wType, 
	DWORD dwEventID, 
	PSID lpUserSid = NULL, 
	WORD wNumStrings = 0, 
	DWORD dwDataSize = 0, 
	LPCTSTR* lpStrings = NULL, 
	CONST VOID* lpRawData = NULL);

__inline
BOOL
WINAPI
NdasLogEventSuccess(
	DWORD dwEventID, 
	PSID lpUserSid = NULL, 
	WORD wNumStrings = 0, 
	DWORD dwDataSize = 0, 
	LPCTSTR *lpString = NULL, 
	CONST VOID* lpRawData = NULL)
{ 
	return NdasLogEvent(EVENTLOG_SUCCESS, dwEventID, lpUserSid, wNumStrings, dwDataSize, lpString, lpRawData); 
}

__inline
BOOL
WINAPI
NdasLogEventError(
	DWORD dwEventID, 
	PSID lpUserSid = NULL, 
	WORD wNumStrings = 0, 
	DWORD dwDataSize = 0, 
	LPCTSTR *lpString = NULL, 
	CONST VOID* lpRawData = NULL)
{ 
	return NdasLogEvent(EVENTLOG_ERROR_TYPE, dwEventID, lpUserSid, wNumStrings, dwDataSize, lpString, lpRawData); 
}

__inline
BOOL
WINAPI
NdasLogEventError2(
	DWORD dwEventID,
	DWORD dwRetValue = 0,
	DWORD dwLastError = ::GetLastError())
{
	DWORD ErrorData[] = { dwRetValue, dwLastError };
	return NdasLogEvent(EVENTLOG_ERROR_TYPE, dwEventID, NULL, 0, sizeof(ErrorData), NULL, ErrorData);
}

__inline
BOOL
WINAPI
NdasLogEventWarning(
	DWORD dwEventID, 
	PSID lpUserSid = NULL, 
	WORD wNumStrings = 0, 
	DWORD dwDataSize = 0, 
	LPCTSTR *lpString = NULL, 
	CONST VOID* lpRawData = NULL)
{ return NdasLogEvent(EVENTLOG_WARNING_TYPE, dwEventID, lpUserSid, wNumStrings, dwDataSize, lpString, lpRawData); }

__inline
BOOL
WINAPI
NdasLogEventInformation(
	DWORD dwEventID, 
	PSID lpUserSid = NULL, 
	WORD wNumStrings = 0, 
	DWORD dwDataSize = 0, 
	LPCTSTR *lpString = NULL, 
	CONST VOID* lpRawData = NULL)
{ return NdasLogEvent(EVENTLOG_INFORMATION_TYPE, dwEventID, lpUserSid, wNumStrings, dwDataSize, lpString, lpRawData); }
