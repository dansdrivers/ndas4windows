#pragma once
#include <strsafe.h>

struct IStructFormatter
{
	virtual void Append(LPCTSTR szString) = 0;
	virtual void AppendFormat(LPCTSTR szFormat, ...) = 0;
	virtual void AppendFormatV(LPCTSTR szFormat, va_list ap) = 0;
	virtual void Reset() = 0;
	virtual LPCTSTR operator()() = 0;
	virtual LPCTSTR ToString() = 0;
};

struct IStructFormatterImpl :
	public IStructFormatter
{
	IStructFormatterImpl() :
		lpBuffer(&szBuffer[0]),
		cchBufferUsed(0)
	{
		szBuffer[0] = _T('\0');
	}

	~IStructFormatterImpl()
	{
	}

	static const SIZE_T MAX_BUFFER_LEN = 1023;
	TCHAR szBuffer[MAX_BUFFER_LEN + 1];
	SIZE_T cchBufferUsed;
	PTCHAR lpBuffer;

	virtual void Append(LPCTSTR szString)
	{
		LPCTSTR lp = szString;
		while (lp && cchBufferUsed < MAX_BUFFER_LEN) {
			*lpBuffer = *lp;
			++lpBuffer;
			++cchBufferUsed;
			if (cchBufferUsed == MAX_BUFFER_LEN) {
				// truncation on buffer full
				break;
			}
		}
		*lpBuffer = _T('\0');
		return;
	}

	virtual void AppendFormat(LPCTSTR szFormat, ...)
	{
		va_list ap;
		va_start(ap, szFormat);
		AppendFormatV(szFormat, ap);
		va_end(ap);
	}

	virtual void AppendFormatV(LPCTSTR szFormat, va_list ap)
	{
		size_t cchAvailBuf = MAX_BUFFER_LEN - cchBufferUsed;
		size_t cchRemaining;
		LPTSTR lpDestEnd;
		HRESULT hr = ::StringCchVPrintfEx(
			lpBuffer, cchAvailBuf, &lpDestEnd,
			&cchRemaining, 0,	szFormat, ap);
		if (STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
			// truncation on buffer full
		}
		lpBuffer = lpDestEnd;
		*lpBuffer = _T('\0');
	}


	virtual void Reset()
	{
		lpBuffer = &szBuffer[0];
		*lpBuffer = TEXT('\0');
		cchBufferUsed = 0;
	}

	virtual LPCTSTR ToString()
	{		
		return szBuffer;
	}

	virtual LPCTSTR operator()()
	{
		return ToString();
	}
};

template<typename T>
VOID StructString(IStructFormatter* psf, T* st);

class CStructFormat
{
	IStructFormatterImpl m_sf;
public:
	CStructFormat()
	{
	}

	template<typename T>
	LPCTSTR CreateString(T* pData)
	{
		StructString(&m_sf, pData);
		return m_sf.ToString();
	}
};
