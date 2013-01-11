#pragma once

//
// Token Parser
//
class CTokenParser
{
	static const DWORD MAX_TOKENS = 32;
	enum TokenType
	{
		ttString,
		ttINT, /* or BOOL */
		ttDWORD,
	};
	struct Token
	{
		TokenType Type;
		LPVOID Output;
	};
	DWORD m_nTokens;
	LPTSTR m_lpBuffer;
	Token m_tokens[MAX_TOKENS];

	static LPCTSTR
	GetNextDelimiter(
		LPCTSTR lpStart, 
		TCHAR chDelimiter);

public:

	CTokenParser();
	~CTokenParser();

	template <typename T>
	void AddToken(T& tokenRef);

	DWORD GetTokenCount();

	//
	// returns number of tokens parsed, 0 if error
	//
	DWORD
	Parse(
		LPCTSTR szString,
		TCHAR chDelimiter);
};

