#pragma once
#include <strsafe.h>

template <size_t tMaxBuffer = 64> class CStringizerA;
template <size_t tMaxBuffer = 64> class CStringizerW;

template <size_t tMaxBuffer>
class CStringizerA
{
public:
	CStringizerA() { 
		m_strbuf[0] = 0; 
	}
	CStringizerA(LPCSTR format, ...) {
		va_list ap;
		va_start(ap,format);
		XTLVERIFY(SUCCEEDED(
			StringCchVPrintfA(m_strbuf, tMaxBuffer, format, ap)));
		va_end(ap);
	}
	~CStringizerA() {}
	LPCSTR ToString() {
		return m_strbuf;
	}
	operator LPCSTR () {
		return ToString();
	}
private:
	CHAR m_strbuf[tMaxBuffer];
};

template <size_t tMaxBuffer>
class CStringizerW
{
public:
	CStringizerW() { 
		m_strbuf[0] = 0; 
	}
	CStringizerW(LPCWSTR format, ...) {
		va_list ap;
		va_start(ap,format);
		XTLVERIFY(SUCCEEDED(
			StringCchVPrintfW(m_strbuf, tMaxBuffer, format, ap)));
		va_end(ap);
	}
	~CStringizerW() {}
	LPCWSTR ToString() {
		return m_strbuf;
	}
	operator LPCWSTR ()	{
		return ToString();
	}
private:
	WCHAR m_strbuf[tMaxBuffer];
};


#if defined(UNICODE) || defined(_UNICODE)
typedef CStringizerW<32> CStringizer;
#else
typedef CStringizerA<32> CStringizer;
#endif

