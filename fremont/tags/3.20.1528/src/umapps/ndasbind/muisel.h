#ifndef _UILANGSELECT_H_
#define _UILANGSELECT_H_

#include <windows.h>

typedef struct _SUPPORTINGLANGUAGES {
	DWORD	nLangs;
	LANGID* idLangs;
} SUPPORTINGLANGUAGES, *PSUPPORTINGLANGUAGES;

LANGID WINAPI
NuiGetCurrentUserUILanguage();

LANGID WINAPI
NuiFindMatchingLanguage(
	IN LANGID LangID, 
	IN SUPPORTINGLANGUAGES SupportingLangs);

LANGID WINAPI
NuiFindMatchingLanguage(
	IN LANGID UserLangID, 
	IN DWORD nSupportingLangIDs, 
	IN LANGID* SupportingLangIDs );

LANGID WINAPI
NuiGetResLangIDFromFile(LPCTSTR szFilename);

typedef struct _NUI_RESDLL_INFO {
	LANGID wLangID;
	WORD wReserved;
	LPTSTR lpszFilePath;
	_NUI_RESDLL_INFO* pNext;
} NUI_RESDLL_INFO, *PNUI_RESDLL_INFO;

DWORD_PTR WINAPI
NuiCreateAvailResourceDLLs(
	LPCTSTR szDirectory, 
	LPCTSTR szFindSpec,
	DWORD_PTR cbList,
	PNUI_RESDLL_INFO pRdiList);

DWORD_PTR WINAPI
NuiCreateAvailResourceDLLsInModuleDirectory(
	IN LPCTSTR szSubDirectory OPTIONAL, 
	IN LPCTSTR szFileSpec OPTIONAL,
	IN DWORD_PTR cbList,
	IN OUT PNUI_RESDLL_INFO pRdiList);

#endif // _UILANGSELECT_H_
