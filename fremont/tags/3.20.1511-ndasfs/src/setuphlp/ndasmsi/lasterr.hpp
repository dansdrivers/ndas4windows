#pragma once
#include <windows.h>
//
// Last Error Reserver
//
// This class is usually used for clean up functions
// or functions requiring no side-effects on the last error,
// such as debug or logging functions.
//
// RESERVE_LAST_ERROR() macro is provided to use this class
// as an automatic variable.
// 
// June 30, 2005 Chesong Lee 
//
class CLastErrorReserver
{
	DWORD m_err;
public:
	CLastErrorReserver() : m_err(::GetLastError()) {}
	~CLastErrorReserver() { ::SetLastError(m_err); }
};

#define RESERVE_LAST_ERROR() CLastErrorReserver _last_error_holder

