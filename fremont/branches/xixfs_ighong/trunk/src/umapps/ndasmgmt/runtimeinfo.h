#pragma once
#include <windows.h>

DWORD 
GetDllVersion(LPCTSTR lpszDllName);

__forceinline
DWORD
GetShellVersion()
{
	return GetDllVersion(_T("shell32.dll"));
}

__forceinline 
DWORD
PackVersion(WORD Major, WORD Minor)
{
	return static_cast<DWORD>(MAKELONG(Major, Minor));
}

