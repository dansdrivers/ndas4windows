#include "stdafx.h"
#include "misc.h"

LPCTSTR 
pGetNextToken(
	LPCTSTR lpszStart, 
	CONST TCHAR chToken)
{
	LPCTSTR pch = lpszStart;
	while (chToken != *pch && _T('\0') != *pch) {
		++pch;
	}
	return pch;
}

LPTSTR 
pGetTokensV(
	LPCTSTR szData, 
	CONST TCHAR chToken,
	va_list ap)
{
	size_t cch = 0;
	HRESULT hr = ::StringCchLength(szData, STRSAFE_MAX_CCH, &cch);
	if (FAILED(hr)) {
		return NULL;
	}

	size_t cbOutput = (cch + 1) * sizeof(TCHAR);

	LPTSTR lpOutput = (LPTSTR) ::LocalAlloc(
		LPTR, cbOutput);

	if (NULL == lpOutput) {
		return NULL;
	}

	::CopyMemory(lpOutput, szData, cbOutput);

	LPTSTR lpStart = lpOutput;
	LPTSTR lpNext = lpStart;

	while (TRUE) {

		TCHAR** ppszToken = va_arg(ap, TCHAR**);

		if (NULL == ppszToken) {
			break;
		}

		if (_T('\0') != *lpStart) {

			lpNext = (LPTSTR) pGetNextToken(lpStart, chToken);

			if (_T('\0') == *lpNext) {
				*ppszToken = lpStart;
				lpStart = lpNext;
			} else {
				*lpNext = _T('\0');
				*ppszToken = lpStart;
				lpStart = lpNext + 1;
			}

		} else {
			*ppszToken = lpStart; 
		}

	}

	return lpOutput;
}

LPTSTR 
pGetTokens(
	LPCTSTR szData, 
	CONST TCHAR chToken, ...)
{
	va_list ap;
	va_start(ap, chToken);
	LPTSTR lp = pGetTokensV(szData, chToken, ap);
	va_end(ap);
	return lp;
}

NetClass 
pGetNetClass(LPCTSTR szNetClass)
{
	static const struct { LPCTSTR szClass; NetClass nc; } 
	nctable[] = {
		{_T("protocol"), NC_NetProtocol},
		{_T("adapter"), NC_NetAdapter},
		{_T("service"), NC_NetService},
		{_T("client"), NC_NetClient}
	};
	
	size_t nEntries = RTL_NUMBER_OF(nctable);

	for (size_t i = 0; i < nEntries; ++i) {
		if (0 == lstrcmpi(szNetClass, nctable[i].szClass)) {
			return nctable[i].nc;
		}
	}

	return NC_Unknown;
}
