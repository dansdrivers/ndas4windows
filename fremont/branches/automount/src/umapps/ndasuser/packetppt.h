#pragma once
#include <strsafe.h>

struct IPacketPrinter
{
	virtual void Append(LPCTSTR szString) = 0;
	virtual void AppendFormat(LPCTSTR szFormat, ...) = 0;
	virtual void AppendFormatV(LPCTSTR szFormat, va_list ap) = 0;
	virtual void Flush() = 0;
	virtual void operator()(LPCTSTR szFormat,...) = 0;
};

struct ConsolePacketPrinter : public IPacketPrinter
{
	ConsolePacketPrinter() :
		lpBuffer(&szBuffer[0]),
		cchBufferUsed(0)
	{}

	~ConsolePacketPrinter()
	{
		Flush();
	}

	static const SIZE_T MAX_BUFFER_LEN = 1024;
	TCHAR szBuffer[MAX_BUFFER_LEN];
	SIZE_T cchBufferUsed;
	PTCHAR lpBuffer;

	virtual void Append(LPCTSTR szString)
	{
		LPTSTR lp = const_cast<LPTSTR>(szString);
		while (lp && cchBufferUsed < MAX_BUFFER_LEN) {
			*lpBuffer = *lp;
			++lpBuffer;
			++cchBufferUsed;
			if (cchBufferUsed == MAX_BUFFER_LEN)
				Flush();
		}
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
			Flush();
		}
		lpBuffer = lpDestEnd;
	}

	virtual void Flush()
	{
		*lpBuffer = TEXT('\0');
		_tprintf(szBuffer);
		lpBuffer = &szBuffer[0];
		cchBufferUsed = 0;
	}

	virtual void operator()(LPCTSTR szFormat, ...)
	{
		va_list ap;
		va_start(ap, szFormat);
		_vtprintf(szFormat, ap);
		va_end(ap);
	}
};

struct XDebugPacketPrinter : public ConsolePacketPrinter
{
	XDebugPacketPrinter() :
		ConsolePacketPrinter()
	{}

	~XDebugPacketPrinter()
	{
	}

	virtual void Flush()
	{
		*lpBuffer = TEXT('\0');
		DPInfo(szBuffer);
		lpBuffer = &szBuffer[0];
		cchBufferUsed = 0;
	}

	virtual void operator()(LPCTSTR szFormat, ...)
	{
		va_list ap;
		va_start(ap, szFormat);
		XVDebugInfo(szFormat, ap);
		va_end(ap);
	}

};
