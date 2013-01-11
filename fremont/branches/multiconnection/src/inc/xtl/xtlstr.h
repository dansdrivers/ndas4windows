#pragma once
#include <strsafe.h>
#include "xtldef.h"

namespace XTL
{

#if defined(UNICODE) || defined(_UNICODE)
#define CStaticStringBuffer CStaticStringBufferW
#else
#define CStaticStringBuffer CStaticStringBufferA
#endif

template <size_t tBufferSize = 256> class CStaticStringBufferW;
template <size_t tBufferSize = 256> class CStaticStringBufferA;

template <size_t tBufferSize>
class CStaticStringBufferA
{
	CHAR m_szBuffer[tBufferSize];
	size_t m_len;
protected:
	LPSTR GetNextPtr() throw()
	{
		return m_szBuffer + m_len;
	}
	size_t GetRemaining() const throw()
	{
		return tBufferSize - m_len;
	}
	bool IsBufferFull() const throw()
	{
		return (m_len >= tBufferSize);
	}
	void SetRemaining(size_t cch) throw()
	{
		m_len = tBufferSize - cch;
	}
public:
	CStaticStringBufferA() throw() : m_len(0)
	{
		m_szBuffer[0] = 0;
	}
	CStaticStringBufferA(LPCSTR Format, ...) throw() : m_len(0) 
	{
		va_list ap;
		va_start(ap, Format);
		AppendFormatV(Format, ap);
		va_end(ap);
	}
	CStaticStringBufferA& Append(LPCSTR Text) throw()
	{
		if (IsBufferFull()) return *this; // Buffer Full, IGNORE
		size_t cchRemaining = GetRemaining();
		HRESULT hr = ::StringCchCopyExA(
			GetNextPtr(), cchRemaining, Text, NULL, 
			&cchRemaining, STRSAFE_IGNORE_NULLS);
		XTLASSERT(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr); hr;
		SetRemaining(cchRemaining);
		return *this;
	}
	CStaticStringBufferA& Append(const GUID& Guid) throw()
	{
		AppendFormat(
			"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			Guid.Data1, Guid.Data2, Guid.Data3, 
			Guid.Data4[0], Guid.Data4[1],
			Guid.Data4[2], Guid.Data4[3], Guid.Data4[4], 
			Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
		return *this;
	}
	CStaticStringBufferA& AppendFormat(LPCSTR Format, ...) throw()
	{
		va_list ap;
		va_start(ap, Format);
		AppendFormatV(Format, ap);
		va_end(ap);
		return *this;
	}
	CStaticStringBufferA& AppendFormatV(LPCSTR Format, va_list ap) throw()
	{
		if (IsBufferFull()) return *this; // Buffer Full, IGNORE
		size_t cchRemaining = GetRemaining();
		HRESULT hr = ::StringCchVPrintfExA(
			GetNextPtr(), cchRemaining, 
			NULL, &cchRemaining, STRSAFE_IGNORE_NULLS,
			Format, ap);
		XTLASSERT(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr); hr;
		SetRemaining(cchRemaining);
		return *this;
	}
	size_t GetLength() const throw()
	{
		return m_len;
	}
	LPCSTR ToString() const throw()
	{
		return m_szBuffer;
	}
	operator LPCSTR() const throw()
	{
		return ToString();
	}
	void Reset()
	{
		m_len = 0;
		m_szBuffer[0] = 0;
	}
};

template <size_t tBufferSize>
class CStaticStringBufferW
{
	WCHAR m_szBuffer[tBufferSize];
	size_t m_len;
protected:
	LPWSTR GetNextPtr() throw()
	{
		return m_szBuffer + m_len;
	}
	size_t GetRemaining() const throw()
	{
		return tBufferSize - m_len;
	}
	bool IsBufferFull() const throw()
	{
		return (m_len >= tBufferSize);
	}
	void SetRemaining(size_t cch) throw()
	{
		m_len = tBufferSize - cch;
	}
public:
	CStaticStringBufferW() throw() : m_len(0)
	{
		m_szBuffer[0] = 0;
	}
	CStaticStringBufferW(LPCWSTR Format, ...) throw() : m_len(0) 
	{
		va_list ap;
		va_start(ap, Format);
		AppendFormatV(Format, ap);
		va_end(ap);
	}
	CStaticStringBufferW& Append(LPCWSTR Text) throw()
	{
		if (IsBufferFull()) return; // Buffer Full, IGNORE
		size_t cchRemaining = GetRemaining();
		HRESULT hr = ::StringCchCopyExW(
			GetNextPtr(), cchRemaining, Text, NULL, 
			&cchRemaining, STRSAFE_IGNORE_NULLS);
		XTLASSERT(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr); hr;
		SetRemaining(cchRemaining);
		return *this;
	}
	CStaticStringBufferW& Append(const GUID& Guid) throw()
	{
		AppendFormat(
			L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			Guid.Data1, Guid.Data2, Guid.Data3, 
			Guid.Data4[0], Guid.Data4[1],
			Guid.Data4[2], Guid.Data4[3], Guid.Data4[4], 
			Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
		return *this;
	}
	CStaticStringBufferW& AppendFormat(LPCWSTR Format, ...) throw()
	{
		va_list ap;
		va_start(ap, Format);
		AppendFormatV(Format, ap);
		va_end(ap);
		return *this;
	}
	CStaticStringBufferW& AppendFormatV(LPCWSTR Format, va_list ap) throw()
	{
		if (IsBufferFull()) return *this; // Buffer Full, IGNORE
		size_t cchRemaining = GetRemaining();
		HRESULT hr = ::StringCchVPrintfExW(
			GetNextPtr(), cchRemaining, 
			NULL, &cchRemaining, STRSAFE_IGNORE_NULLS,
			Format, ap);
		XTLASSERT(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr); hr;
		SetRemaining(cchRemaining);
		return *this;
	}
	size_t GetLength() const throw()
	{
		return m_len;
	}
	LPCWSTR ToString() const throw()
	{
		return m_szBuffer;
	}
	operator LPCWSTR() const throw()
	{
		return ToString();
	}
	void Reset()
	{
		m_len = 0;
		m_szBuffer[0] = 0;
	}
};

} // namespace XTL
