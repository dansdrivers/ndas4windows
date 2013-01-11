#pragma once

LPCTSTR 
pGetNextToken(
	LPCTSTR lpszStart, 
	CONST TCHAR chToken);

LPTSTR 
pGetTokensV(
	LPCTSTR szData, 
	CONST TCHAR chToken, 
	va_list ap);

LPTSTR 
pGetTokens(
	LPCTSTR szData,
	CONST TCHAR chToken, 
	...);

#include "netcomp.h"

NetClass 
pGetNetClass(LPCTSTR szNetClass);

