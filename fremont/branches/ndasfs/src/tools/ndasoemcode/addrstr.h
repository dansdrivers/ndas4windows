#pragma once
#include <windows.h>
#include <tchar.h>

DWORD
string_to_byte_addr(
	LPCTSTR AddressString,
	BYTE* Address,
	DWORD AddressLength);

