#pragma once
#include <windows.h>
#include <tchar.h>

#ifdef NO_XS_NAMESPACE
#define BEGIN_XS_NAMESPACE
#define END_XS_NAMESPACE
#else
#define BEGIN_XS_NAMESPACE namespace xs {
#define END_XS_NAMESPACE }
#endif

BEGIN_XS_NAMESPACE

struct IXSStringCreator
{
	virtual LPCTSTR ToString() = 0;
};

class CXSStringCreatorBase : public IXSStringCreator
{
protected:

	TCHAR m_szBufferStatic[256];
	TCHAR* m_lpBuffer;
	SIZE_T m_cchBuffer;

	CXSStringCreatorBase(DWORD cchMax) :
		m_lpBuffer(m_szBufferStatic),
		m_cchBuffer(sizeof(m_szBufferStatic) / sizeof(TCHAR))
	{
		if (cchMax > m_cchBuffer) {
			m_lpBuffer = new TCHAR[cchMax];
			if (NULL == m_lpBuffer) {
				m_lpBuffer = m_szBufferStatic;
			}
			m_cchBuffer = cchMax;
		}
	}

public:

	virtual ~CXSStringCreatorBase()
	{
		if (m_szBufferStatic != m_lpBuffer) {
			delete m_lpBuffer;
		}
	}
};

class CXSByteString : public CXSStringCreatorBase
{
	SIZE_T m_cbData;
	CONST BYTE* m_pbData;

	const BOOL m_fDelimited;
	const TCHAR m_chDelimiter;
	const BOOL m_fUpperCaseHexMode;

public:
	CXSByteString(DWORD cbData, CONST BYTE* pbData, BOOL fUppercaseHexMode = TRUE) :
		m_cbData(cbData),
		m_pbData(pbData),
		m_fDelimited(FALSE),
		m_chDelimiter(_T('\0')),
		m_fUpperCaseHexMode(fUppercaseHexMode),
		CXSStringCreatorBase(cbData * 2 + 1)
	{
	}

	CXSByteString(DWORD cbData, CONST BYTE* pbData, TCHAR chDelimiter, BOOL fUppercaseHexMode = TRUE) :
		m_cbData(cbData),
		m_pbData(pbData),
		m_fDelimited(TRUE),
		m_chDelimiter(chDelimiter),
		m_fUpperCaseHexMode(fUppercaseHexMode),
		CXSStringCreatorBase(cbData * 3 + 1)
	{
	}

	virtual ~CXSByteString()
	{
	}

	LPCTSTR ToString()
	{
		static CONST TCHAR HEX_DIGITS_LOWER[] = _T("0123456789abcdef");
		static CONST TCHAR HEX_DIGITS_UPPER[] = _T("0123456789ABCDEF");
		static CONST TCHAR* HEX_DIGITS = m_fUpperCaseHexMode ? 
			HEX_DIGITS_UPPER : HEX_DIGITS_LOWER;

		SIZE_T i = 0;
		SIZE_T chByte = m_fDelimited ? 3 : 2;

		for (; i < m_cbData; ++i)
		{
			m_lpBuffer[i*chByte] = HEX_DIGITS[m_pbData[i] >> 4];
			m_lpBuffer[i*chByte+1] = HEX_DIGITS[m_pbData[i] & 0xF];
			if (m_fDelimited) {
				m_lpBuffer[i*chByte+2] = (i + 1) < m_cbData ? m_chDelimiter : _T('\0');
			}
		}
		m_lpBuffer[i*chByte] = _T('\0');

		return m_lpBuffer;
	}	
};

END_XS_NAMESPACE
