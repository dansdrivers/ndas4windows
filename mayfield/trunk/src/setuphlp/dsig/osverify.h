#pragma once

#define IS_WINDOWS_XP() VerifyWindowsVersion(VER_EQUAL, 5,1, 0xFFFF)
#define IS_WINDOWS_2000() VerifyWindowsVersion(VER_EQUAL, 5,0, 0xFFFF)
#define IS_WINDOWS_XP_OR_LATER() VerifyWindowsVersion(VER_GREATER_EQUAL,5,1,0)
#define IS_WINDOWS_SERVER_2003_OR_LATER() VerifyWindowsVersion(VER_GREATER_EQUAL, 5,2,0)
#define IS_WINDOWS_2000_OR_LATER() VerifyWindowsVersion(VER_GREATER_EQUAL,5,0,0)

BOOL WINAPI VerifyWindowsVersion(
	BYTE dwCondMask, 
	DWORD dwMajorVersion, 
	DWORD dwMinorVersion, 
	WORD wServicePackMajor);

BOOL WINAPI IsUserAdmin(VOID);

