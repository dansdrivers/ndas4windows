////////////////////////////////////////////////////////////////////////////
//
// Implementation of exception classes 
//
// @file	ndasexception.cpp
// @author	Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasexception.h"

void DbgPrint(LPCTSTR DebugMessage, ... )
{
	va_list ap;
	WTL::CString strDebugMsg;
	va_start(ap, DebugMessage);

	strDebugMsg.FormatV( DebugMessage, ap );

	::OutputDebugString(strDebugMsg);

	va_end(ap);
}

WTL::CString CServiceException::FormatMessage(int nCode)
{
	WTL::CString strText;
	
	if ((nCode & APPLICATION_ERROR_MASK) == 0) {
		LPTSTR lpszErrorMessage;
		BOOL fSuccess = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, nCode, 0, (LPTSTR) &lpszErrorMessage, 0, NULL);
		if ( fSuccess )
		{
			strText.Format(TEXT("%s, Error Code: %d (0x%08X)"),
				nCode, nCode, lpszErrorMessage);
			::LocalFree(lpszErrorMessage);
		}
	}
	return strText;
}