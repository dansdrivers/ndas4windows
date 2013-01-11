#pragma once
#include <winver.h>

#define FVI_Comments         _T("Comments")
#define FVI_InternalName     _T("InternalName")
#define FVI_ProductName      _T("ProductName")
#define FVI_CompanyName      _T("CompanyName")
#define FVI_LegalCopyright   _T("LegalCopyright")
#define FVI_ProductVersion   _T("ProductVersion")
#define FVI_FileDescription  _T("FileDescription") 
#define FVI_LegalTrademarks  _T("LegalTrademarks") 
#define FVI_PrivateBuild     _T("PrivateBuild") 
#define FVI_FileVersion      _T("FileVersion") 
#define FVI_OriginalFilename _T("OriginalFilename") 
#define FVI_SpecialBuild     _T("SpecialBuild") 

typedef struct _LANGCODEPAGE {
	WORD Language;
	WORD CodePage;
} LANGCODEPAGE, *PLANGCODEPAGE;

HRESULT
WINAPI
FvGetFileVersionTranslations(
	__in LPCVOID FileVersionInfo,
	__out LPDWORD TranslationCount,
	__out const LANGCODEPAGE** TranslationArray);

HRESULT
WINAPI
FvGetFileVersionInformationString(
	__in LPCVOID FileVersionInfo,
	__in LPCTSTR InformationName,
	__out LPCTSTR* Information,
	__out PUINT InformationLength);

CONST VS_FIXEDFILEINFO* 
FvGetFixedFileInfo(
	__in LPCVOID FileVersionInfo);

HRESULT
FvReadFileVersionInfo(
	__in LPCTSTR FileName,
	__deref_out PVOID* FileVersionInfo);

HRESULT
FvReadFileVersionInfoByHandle(
	__in_opt HMODULE ModuleHandle,
	__deref_out PVOID* FileVersionInfo);

HRESULT
WINAPI
FvReadFileVersionFixedFileInfoByHandle(
	__in_opt HMODULE ModuleHandle,
	__out VS_FIXEDFILEINFO* FixedFileInfo);

HRESULT
WINAPI
FvReadFileVersionFixedFileInfo(
	__in LPCTSTR ModuleFileName,
	__out VS_FIXEDFILEINFO* FixedFileInfo);
