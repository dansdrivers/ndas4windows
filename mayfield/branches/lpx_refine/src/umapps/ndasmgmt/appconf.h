#pragma once
#include "regconf.h"

#define NACFG_USER 0x01
#define NACFG_SYSTEM 0x02
#define NACFG_ALL 0x03

BOOL InitAppConfig();

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPDWORD lpdwValue, DWORD dwFlags = NACFG_ALL);

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPBOOL lpbValue, DWORD dwFlags = NACFG_ALL);

BOOL pGetAppConfigValue(
	LPCTSTR szConfigName, LPTSTR lpszValue, DWORD cchMaxValue, DWORD dwFlags = NACFG_ALL);

BOOL pSetAppConfigValue(
	LPCTSTR szConfigName, LPCTSTR szValue, DWORD dwFlags = NACFG_USER);

BOOL pSetAppConfigValue(
	LPCTSTR szConfigName, DWORD dwValue, DWORD dwFlags = NACFG_USER);

BOOL pSetAppConfigValueBOOL(
	LPCTSTR szConfigName, BOOL fValue, DWORD dwFlags = NACFG_USER);
