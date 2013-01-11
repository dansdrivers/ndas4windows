#pragma once

extern LPCTSTR NDASSYS_KEYS_REGKEY;
extern LPCTSTR NDASSYS_SERVICE_REGKEY;
extern LPCTSTR NDASSYS_SERVICE_ARFLAGS_REGVAL;

BOOL 
WINAPI
NdasSysSetConfigValue(
	LPCTSTR szContainer,
	LPCTSTR szValueName,
	CONST VOID* lpValue,
	DWORD cbValue);

BOOL 
WINAPI
NdasSysGetConfigValue(
	LPCTSTR szContainer,
	LPCTSTR szValueName,
	LPVOID lpValue,
	DWORD cbValue,
	LPDWORD pcbUsed);

BOOL 
WINAPI
NdasSysDeleteConfigValue(
	LPCTSTR szContainer,
	LPCTSTR szValueName);
