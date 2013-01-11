#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

// Single Parameter only
class CDdeCommandParser 
{
public:
	CDdeCommandParser();
	bool Parse(LPCTSTR lpCommand, LPCTSTR* lpNextCommand);
	LPCTSTR GetOpCode();
	LPCTSTR GetParam();

private:
	TCHAR m_szOpCode[MAX_PATH];
	TCHAR m_szParam[MAX_PATH];
	TCHAR* m_lpCurParam;
	bool _AppendToParam(TCHAR ch);
};

inline
CDdeCommandParser::CDdeCommandParser()
{
}

inline
LPCTSTR 
CDdeCommandParser::GetOpCode()
{
	return m_szOpCode;
}

inline
LPCTSTR 
CDdeCommandParser::GetParam()
{
	return m_szParam;
}

inline
bool 
CDdeCommandParser::_AppendToParam(TCHAR ch)
{
	if (m_lpCurParam < &m_szParam[MAX_PATH])
	{
		*m_lpCurParam = ch;
		++m_lpCurParam;
		return true;
	}
	return false;
}

inline
bool
CDdeCommandParser::Parse(
	LPCTSTR lpCommand,
	LPCTSTR* ppNextCommand)
{
	m_szOpCode[0] = 0;
	m_szParam[0] = 0;

	LPCTSTR lp = lpCommand;
	if (*lp != _T('[')) return false;
	++lp;
	LPCTSTR lpOpStart = lp;
	// get op code
	while (_T('(') != *lp && _T(']') != *lp && 0 != *lp) ++lp;
	if (0 == *lp) return false;
	HRESULT hr = ::StringCbCopyN(
		m_szOpCode, sizeof(m_szOpCode), 
		lpOpStart, (lp - lpOpStart) * sizeof(TCHAR));
	if (FAILED(hr)) return false;
	// get parameter
	if (_T('(') != *lp)
	{
		// null parameter
		if (_T(']') == *lp)
		{ 
			m_szParam[0] = 0;
			++lp; *ppNextCommand = lp;
			return true;
		}
		else
		{
			return false;
		}
	}
	++lp; // consume (
	m_lpCurParam = m_szParam;
	// with parameter
	while (_T(')') != *lp && 0 != *lp)
	{
		if (_T('"') == *lp)
		{
			_tprintf(_T("InQuote\n"));
			++lp;
			while (_T('"') != *lp && 0 != *lp)
			{
				_AppendToParam(*lp);
				++lp;
				// double quote "abcd "" efg"
				if (_T('"') == *lp && _T('"') == *(lp+1))
				{
					_AppendToParam(*lp);
					++lp; ++lp;
				}
			}
			if (0 == *lp) return false;
			++lp;
			_tprintf(_T("OutQuote\n"));
		}
		else
		{
			_AppendToParam(*lp);
			++lp;
		}
	}
	_AppendToParam(0);
	if (_T(')') != *lp) return false;
	++lp;
	if (_T(']') != *lp) return false;
	++lp;
	*ppNextCommand = lp;
	return true;
}

