#ifndef NDSETUPLOG_H
#define NDSETUPLOG_H

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#pragma comment (lib, "strsafe.lib")
#include <setupapi.h>
#pragma comment (lib, "setupapi.lib")

#define NDSETUP_LOGFILE (TEXT("NDASDeviceSetup.log"))
#define NDSETUP_LOG_BUFFER_MAX	255

#ifdef __cplusplus
extern "C" {
#endif

VOID DebugPrintf(IN LPCTSTR szFormat, ...);

int __stdcall LogStart() ;
int __stdcall LogEnd() ;
int __stdcall LogPrintf(LPCTSTR szFormat, ...);
int __stdcall LogMPrintf(LPCTSTR szModule, LPCTSTR szFormat, ...);

int __stdcall  LogPrintfErr(LPCTSTR szFormat, ...);
void __stdcall LogLastError();
void __stdcall LogErrorCode(LPTSTR szPrefix, DWORD dwErrorCode);

#ifdef __cplusplus
};
#endif

#endif