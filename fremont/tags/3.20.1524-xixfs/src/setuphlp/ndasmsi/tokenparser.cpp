#include "stdafx.h"
#include "tokenparser.hpp"

CTokenParser::
CTokenParser() :
	m_nTokens(0),
	m_lpBuffer(0)
{
}

CTokenParser::
~CTokenParser()
{
	if (m_lpBuffer)
	{
		HLOCAL hLocal = ::LocalFree((HLOCAL)m_lpBuffer);
		_ASSERTE(NULL == hLocal);
	}
}

DWORD
CTokenParser::
GetTokenCount()
{
	return m_nTokens;
}

template<>
void
CTokenParser::
AddToken<DWORD>(
	DWORD& dwTokenRef)
{
	_ASSERTE(m_nTokens < MAX_TOKENS);
	if (m_nTokens >= MAX_TOKENS) return;

	m_tokens[m_nTokens].Type = ttDWORD;
	m_tokens[m_nTokens].Output = (LPVOID) &dwTokenRef;
	++m_nTokens;
}

template<>
void
CTokenParser::
AddToken<INT>(
	INT& iTokenRef)
{
	_ASSERTE(m_nTokens < MAX_TOKENS);
	if (m_nTokens >= MAX_TOKENS) return;

	m_tokens[m_nTokens].Type = ttINT;
	m_tokens[m_nTokens].Output = (LPVOID) &iTokenRef;
	++m_nTokens;
}

template<>
void
CTokenParser::
AddToken<LPCTSTR>(
	LPCTSTR& lpszTokenRef)
{
	_ASSERTE(m_nTokens < MAX_TOKENS);
	if (m_nTokens >= MAX_TOKENS) return;

	m_tokens[m_nTokens].Type = ttString;
	m_tokens[m_nTokens].Output = (LPVOID) &lpszTokenRef;
	++m_nTokens;
}

DWORD
CTokenParser::
Parse(
	LPCTSTR szString,
	TCHAR chDelimiter)
{
	size_t cch;
	HRESULT hr = ::StringCchLength(szString, STRSAFE_MAX_CCH, &cch);
	if (FAILED(hr))
	{
		return 0;
	}

	if (m_lpBuffer)
	{
		HLOCAL hFree = ::LocalFree((HLOCAL)m_lpBuffer);
		_ASSERTE(NULL == hFree);
	}

	m_lpBuffer = (LPTSTR) ::LocalAlloc(LPTR, (cch + 1)* sizeof(TCHAR));

	if (NULL == m_lpBuffer)
	{
		return 0;
	}

	::CopyMemory(m_lpBuffer, szString, (cch+1) * sizeof(TCHAR));

	LPTSTR lpCur = m_lpBuffer;

	DWORD i = 0;
	for (; i < m_nTokens; ++i)
	{
		LPTSTR lpDelimiter = (LPTSTR) GetNextDelimiter(lpCur, chDelimiter);
		BOOL eos = (_T('\0') == lpDelimiter);
		*lpDelimiter = _T('\0');
		if (ttString == m_tokens[i].Type)
		{
			*(LPTSTR*)(m_tokens[i].Output) = lpCur;
		}
		else if (ttDWORD == m_tokens[i].Type)
		{
			int ivalue;
			BOOL converted = ::StrToIntEx(
				lpCur,
				STIF_SUPPORT_HEX, 
				&ivalue);
			if (!converted)
			{
				return i;
			}
			*(LPDWORD)(m_tokens[i].Output) = (DWORD) ivalue;
		}
		else if (ttINT == m_tokens[i].Type)
		{
			int ivalue;
			BOOL converted = ::StrToIntEx(
				lpCur,
				STIF_SUPPORT_HEX, 
				&ivalue);
			if (!converted)
			{
				return i;
			}
			*(LPINT)(m_tokens[i].Output) = ivalue;
		}
		else
		{
			_ASSERTE(FALSE);
		}
		if (eos)
		{
			break;
		}
		else
		{
			lpCur = lpDelimiter + 1;
		}
	}

	return i;
}

LPCTSTR 
CTokenParser::
GetNextDelimiter(
	LPCTSTR lpStart, 
	TCHAR chDelimiter)
{
	LPCTSTR pch = lpStart;
	while (chDelimiter != *pch && _T('\0') != *pch) 
	{
		++pch;
	}
	return pch;
}
