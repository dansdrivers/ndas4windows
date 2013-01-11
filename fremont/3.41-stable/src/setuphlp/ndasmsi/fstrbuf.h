#pragma once
#include <strsafe.h>
#include <crtdbg.h>

template <size_t BUFFER_SIZE = 256>
class FixedStringBuffer
{
	TCHAR m_szBuffer[BUFFER_SIZE];
public:
	FixedStringBuffer()
	{
		m_szBuffer[0] = _T('\0');
	}
	FixedStringBuffer(LPCTSTR lpStr)
	{
		HRESULT hr = ::StringCchCopy(m_szBuffer, BUFFER_SIZE, lpStr);
		_ASSERTE(SUCCEEDED(hr));
	}
	FixedStringBuffer(LPCTSTR szFormat, ...)
	{
		va_list ap;
		va_start(ap, szFormat);
		HRESULT hr = ::StringCchVPrintfEx(
			m_szBuffer, BUFFER_SIZE, NULL, NULL, 0, szFormat, ap);
		_ASSERTE(SUCCEEDED(hr));
		va_end(ap);
	}
	~FixedStringBuffer()
	{
	}
	LPCTSTR Append(LPCTSTR szStr)
	{
		HRESULT hr = ::StringCchCat(m_szBuffer, BUFFER_SIZE, szStr);
		_ASSERTE(SUCCEEDED(hr));
	}
	LPCTSTR Format(LPCTSTR szFormat, ...)
	{
		va_list ap;
		va_start(ap, szFormat);
		HRESULT hr = ::StringCchVPrintfEx(
			m_szBuffer, BUFFER_SIZE, NULL, NULL, 0, szFormat, ap);
		_ASSERTE(SUCCEEDED(hr));
		va_end(ap);
		return m_szBuffer;
	}
	operator LPCTSTR()
	{
		return m_szBuffer;
	}
};

typedef FixedStringBuffer<256> FSB256;
typedef FixedStringBuffer<1024> FSB1024;
