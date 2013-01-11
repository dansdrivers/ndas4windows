#pragma once
#ifndef _NDAS_UPDATE_H_
#define _NDAS_UPDATE_H_

#ifndef __urlmon_h__
#error ndupdate.h requires urlmon.h to be included first
#endif

#if   defined NDUPDATE_DLL_EXPORTS
#define NDUPDATE_API	__declspec(dllexport)
#elif defined NDUPDATE_DLL_IMPORTS
#define NDUPDATE_API	__declspec(dllimport)
#else
#define NDUPDATE_API
#endif // NDUPDATE_DLL_{EXPORTS|IMPORTS}

#ifdef __cplusplus
extern "C" {
#endif

#define NDUPDATE_API_VERSION_MAJOR 1
#define NDUPDATE_API_VERSION_MINOR 0

#define NDUPDATE_MAX_URL	255
#define NDUPDATE_MAX_HOSTNAME 255
#define NDUPDATE_STRING_SIZE 255

#define NDAS_PLATFORM_UKNWN	0
#define NDAS_PLATFORM_WIN2K	1
#define NDAS_PLATFORM_WINXP	2
#define NDAS_PLATFORM_WIN98	3
#define NDAS_PLATFORM_WINME	4
#define NDAS_PLATFORM_W2003 5
#define NDAS_PLATFORM_LINUX	6
#define NDAS_PLATFORM_MACOS	7

typedef struct _NDUPDATE_SYSTEM_INFO {
	DWORD dwPlatform;
	struct {
		WORD wMajor;
		WORD wMinor;
		WORD wBuild;
		WORD wPrivate;
	} ProductVersion;
	DWORD dwVendor;
	DWORD dwLanguageSet;
} NDUPDATE_SYSTEM_INFO, *PNDUPDATE_SYSTEM_INFO;

typedef struct _NDUPDATE_UPDATE_INFO {
	BOOL fNeedUpdate;
	struct {
		WORD wMajor;
		WORD wMinor;
		WORD wBuild;
		WORD wPrivate;
	} ProductVersion;
	DWORD dwUpdateFileSize;
	TCHAR szUpdateFileName[MAX_PATH];
	TCHAR szUpdateFileURL[NDUPDATE_MAX_URL];
	TCHAR szRefURL[NDUPDATE_MAX_URL];
} NDUPDATE_UPDATE_INFO, *PNDUPDATE_UPDATE_INFO;

#define NDUPDATE_ERROR_NOT_IMPLEMENTED ERROR_CALL_NOT_IMPLEMENTED

#define NDUPDATE_ERROR_INVALID_UPDATE_VERSION_FROM_SERVER   0xFF000001
#define NDUPDATE_ERROR_INVALID_SERVER_RESPONSE				0xFF000002

NDUPDATE_API
DWORD
WINAPI
NdasUpdateGetAPIVersion();

NDUPDATE_API
BOOL
WINAPI
NdasUpdateGetUpdateInfo(
	IN IBindStatusCallback* pBSC,
	IN LPCTSTR szBaseURL,
	IN PNDUPDATE_SYSTEM_INFO pSystemInformation,
	OUT PNDUPDATE_UPDATE_INFO pUpdateInformation);

#ifdef __cplusplus
}
#endif

#endif // _NDAS_UPDATE_H_
