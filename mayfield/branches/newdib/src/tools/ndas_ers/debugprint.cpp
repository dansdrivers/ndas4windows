#include "StdAfx.h"
#include "DebugPrint.h"

ULONG	DebugPrintLevel = 1;

CHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];

VOID
DbgPrint(
		 IN PCHAR	DebugMessage,
		 ...
		 )
{
    va_list ap;
	
    va_start(ap, DebugMessage);
	
	StringCchVPrintfA(DebugBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	
	OutputDebugString(DebugBuffer);
    
    va_end(ap);
}

void PrintErrorCode(LPTSTR strPrefix, DWORD	ErrorCode)
{
	LPTSTR lpMsgBuf;
	CHAR	szTempBuffer[512] = { 0 };
	
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	StringCchPrintfA(szTempBuffer, 512, "%s%s", strPrefix, lpMsgBuf);
	
	OutputDebugString(szTempBuffer);

	// Free the buffer.
	LocalFree( lpMsgBuf );
}


void PrintLastError() {

#ifdef DBG
	
	LPVOID lpMsgBuf;
	
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION);
	// Free the buffer.
	LocalFree(lpMsgBuf);

#else

#endif
}
