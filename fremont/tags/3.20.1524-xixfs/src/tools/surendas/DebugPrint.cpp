#include "StdAfx.h"

// 
// Debug
//
ULONG	DebugPrintLevel = 0;

//  
//  FUNCTION: GetLastErrorText  
//  
//  PURPOSE: copies error message text to string  
//  
//  PARAMETERS:  
//    lpszBuf - destination buffer  
//    dwSize - size of buffer  
//  
//  RETURN VALUE:  
//    destination buffer  
//  
//  COMMENTS:  
//  
LPTSTR GetLastErrorText( LPTSTR lpszBuf, DWORD dwSize )  
{  
    DWORD dwRet;  
    LPTSTR lpszTemp = NULL;  
	
    dwRet = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,  
		NULL,  
		GetLastError(),  
		LANG_NEUTRAL,  
		(LPTSTR)&lpszTemp,  
		0,  
		NULL 
		);  
  
    // supplied buffer is not long enough  
    if ( !dwRet || ( (long)dwSize < (long)dwRet+14 ) )  
        lpszBuf[0] = TEXT('\0');  
    else  
    {  
        lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  //remove cr and newline character  
        _stprintf( lpszBuf, TEXT("%s (0x%x)"), lpszTemp, GetLastError() );  
    }  
  
    if ( lpszTemp )  
        LocalFree((HLOCAL) lpszTemp );  
  
    return lpszBuf;  
} 


VOID
DbgPrintA(
		  IN PTCHAR	DebugMessage,
		  ...
		  )
{
	CHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];
    va_list ap;
	
    va_start(ap, DebugMessage);
	
	_vsnprintf(DebugBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	
	OutputDebugStringA(DebugBuffer);
    
    va_end(ap);
}

VOID
DbgPrintW(
		 IN PWCHAR	DebugMessage,
		 ...
		 )
{
	WCHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];
    va_list ap;
	
    va_start(ap, DebugMessage);
	
	_vsnwprintf(DebugBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	
	OutputDebugStringW(DebugBuffer);
    
    va_end(ap);
}

void PrintErrorCode(LPTSTR strPrefix, DWORD	ErrorCode)
{
	LPTSTR	lpMsgBuf;
	TCHAR	szTempBuffer[512] = { 0 };
	
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
	_stprintf(szTempBuffer, TEXT("%s ErrorCode: 0x%x %s"), strPrefix, ErrorCode, lpMsgBuf);

	OutputDebugString(szTempBuffer);
	
	// Free the buffer.
	LocalFree( lpMsgBuf );
}