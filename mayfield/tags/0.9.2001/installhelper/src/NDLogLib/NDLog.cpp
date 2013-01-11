#define STRSAFE_LIB
#include "NDLog.h"

FILE *pfLogFile = NULL;
HSPFILELOG *hSpFileLog = NULL;

void FormatCurrentTime(OUT LPTSTR szBuffer, OUT DWORD cchBuffer);
void FormatDateTime(IN SYSTEMTIME st, OUT LPTSTR szBuffer, OUT DWORD cchBuffer);
int LogPrintString(BOOL bTimestamped, LPCTSTR szString);
int __stdcall LogNTPrintf(LPCTSTR szFormat, ...);

// #define LINK_TEST
#ifdef LINK_TEST
int __cdecl _tmain(int argc, LPTSTR *argv)
{
	LogStart();
	DebugPrintf(TEXT("NDSetupLog\n"));
	LogEnd();
	return -1;
}
#endif

VOID DebugPrintf(IN LPCTSTR szFormat, ...)
{
#ifdef DBG
	static TCHAR szBuffer[256] = {0};
    va_list ap;
    va_start(ap, szFormat);
	StringCchVPrintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szFormat, ap);	
	OutputDebugString(szBuffer);
    va_end(ap);
#endif
}

int __stdcall LogStart()
{
	TCHAR szWinDir[MAX_PATH + 1];
	TCHAR szLogFile[MAX_PATH + 1];

    SYSTEMTIME  ust;
    SYSTEMTIME  lst;
	TCHAR szLocalTime[256], szUTCTime[256];

	if (pfLogFile)
	{
		DebugPrintf(TEXT("NDSetupLog Already Started\n"));
		return 0;
	}

	GetWindowsDirectory(szWinDir, MAX_PATH);

	// hSpFileLog = SetupInitializeFileLog(szLogFile, 0);

	StringCchPrintf(szLogFile, sizeof(szLogFile), TEXT("%s\\%s"), szWinDir, NDSETUP_LOGFILE);

	if ((pfLogFile = _tfopen(szLogFile, TEXT("a")))== NULL)
	{
		DebugPrintf(TEXT("Failed to open the log file: %s\n"), szLogFile);
		return -1;
	}

	DebugPrintf(TEXT("NDSSetupLog started successfully.\n"));

	GetSystemTime(&ust);
	SystemTimeToTzSpecificLocalTime(NULL, &ust, &lst);
	FormatDateTime(ust, szUTCTime, sizeof(szUTCTime));
	FormatDateTime(lst, szLocalTime, sizeof(szLocalTime));
	
	LogNTPrintf(TEXT("\n"));
	LogNTPrintf(TEXT("------------------------------------------------------------------------------"));
	LogNTPrintf(TEXT("Log stared"));
	LogNTPrintf(TEXT("Timestamp: %s (Local), %s (UTC)"), szLocalTime, szUTCTime);
	LogNTPrintf(TEXT("------------------------------------------------------------------------------"));

	return 0;
}

int __stdcall LogEnd()
{
    SYSTEMTIME  ust;
    SYSTEMTIME  lst;
	TCHAR szLocalTime[256], szUTCTime[256];

	if (pfLogFile)
	{

		GetSystemTime(&ust);
		SystemTimeToTzSpecificLocalTime(NULL, &ust, &lst);
		FormatDateTime(ust, szUTCTime, sizeof(szUTCTime));
		FormatDateTime(lst, szLocalTime, sizeof(szLocalTime));
		
		LogNTPrintf(TEXT("------------------------------------------------------------------------------"));
		LogNTPrintf(TEXT("Log ended"));
		LogNTPrintf(TEXT("Timestamp: %s (Local), %s (UTC)"), szLocalTime, szUTCTime);
		LogNTPrintf(TEXT("------------------------------------------------------------------------------"));

		fclose(pfLogFile);
		DebugPrintf(TEXT("NDSSetupLog ended successfully.\n"));
		return 0;
	}
	else
	{
		DebugPrintf(TEXT("NDSSetupLog not started yet.\n"));
		return -1;
	}

}

int __stdcall LogNTPrintf(LPCTSTR szFormat, ...)
{
	TCHAR szBuffer[NDSETUP_LOG_BUFFER_MAX];
	int iRet;
	va_list ap;
	va_start(ap, szFormat);
	StringCchVPrintf(szBuffer, sizeof(szBuffer), szFormat, ap);
	iRet = LogPrintString(FALSE, szBuffer);
	va_end(ap);
	return iRet;
}

int __stdcall LogPrintf(LPCTSTR szFormat, ...)
{
	TCHAR szBuffer[NDSETUP_LOG_BUFFER_MAX];
	int iRet;
	va_list ap;
	va_start(ap, szFormat);
	StringCchVPrintf(szBuffer, sizeof(szBuffer), szFormat, ap);
	iRet = LogPrintString(TRUE, szBuffer);
	va_end(ap);
	return iRet;
}

int __stdcall  LogPrintfErr(LPCTSTR szFormat, ...)
{
	TCHAR szBuffer[NDSETUP_LOG_BUFFER_MAX];
	int iRet;
	va_list ap;
	va_start(ap, szFormat);
	StringCchVPrintf(szBuffer, sizeof(szBuffer), szFormat, ap);
	iRet = LogPrintString(TRUE, szBuffer);
	va_end(ap);
	LogLastError();
	return iRet;
}

int __stdcall LogMPrintf(LPCTSTR szModule, LPCTSTR szFormat, ...)
{
	return 0;
}

void FormatDateTime(IN SYSTEMTIME st, OUT LPTSTR szBuffer, OUT DWORD cchBuffer)
{
    TCHAR       szTimeFormat[256] = _T("%02d.%02d.%02d, %02d:%02d:%02d");
    TCHAR       szFinalFormat[256] = _T("%d\t%s\t%s\t%d\t%d\t%s");
    TCHAR       szTime[256];

	StringCchPrintf(szBuffer, cchBuffer, szTimeFormat, 
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);
}

void FormatCurrentTime(OUT LPTSTR szBuffer, OUT DWORD cchBuffer)
{
    SYSTEMTIME  ust;
    SYSTEMTIME  lst;
    TCHAR       szTimeFormat[256] = _T("%02d:%02d:%02d.%04d");
    TCHAR       szTime[256];

	GetSystemTime(&ust);
	SystemTimeToTzSpecificLocalTime(NULL, &ust, &lst);
	StringCchPrintf(szBuffer, cchBuffer, szTimeFormat, lst.wHour, lst.wMinute, lst.wSecond, lst.wMilliseconds);
}

int LogPrintString(BOOL bTimestamped, LPCTSTR szText)
{
	static TCHAR szBuffer[NDSETUP_LOG_BUFFER_MAX + 1];
	TCHAR szTime[256];
	size_t cchText;
	int ret;

#ifdef DBG
	DebugPrintf(TEXT("%s\n"), szText);
#endif

	if (pfLogFile)
	{
		if (bTimestamped)
		{
			FormatCurrentTime(szTime, 255);
			
			ret = _ftprintf(pfLogFile, TEXT("%s %s\n"), szTime, szText);
			fflush(pfLogFile);
			return ret;
		}
		else
		{
			ret = _ftprintf(pfLogFile, TEXT("%s\n"), szText);
			fflush(pfLogFile);
			return ret;
		}
	}
	return -1;
}

void __stdcall LogLastError()
{
	LPVOID	lpMsgBuf;
	DWORD	dwError;
	
	dwError = GetLastError();
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	
	// Process any inserts in lpMsgBuf.
	// ...
	
	LogPrintf(TEXT("Error %x: %s"), dwError, lpMsgBuf);
	
	// Free the buffer.
	LocalFree(lpMsgBuf);
}

void __stdcall LogErrorCode(LPTSTR szPrefix, DWORD dwErrorCode)
{
	LPTSTR lpMsgBuf;
	TCHAR	szBuffer[NDSETUP_LOG_BUFFER_MAX + 1] = { 0 };	
	
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...

	// append to the log
	LogPrintf(TEXT("%s%s"), szPrefix, lpMsgBuf);

	// Free the buffer.
	LocalFree( lpMsgBuf );
}
