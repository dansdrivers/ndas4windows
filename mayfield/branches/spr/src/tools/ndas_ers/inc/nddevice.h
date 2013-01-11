#ifndef NDSDEVICE_H
#define NDSDEVICE_H

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#define STRSAFE_LIB
#include <strsafe.h>

#include "NDSetup.h"

#pragma comment(lib, "newdev.lib")

#ifdef __cplusplus
extern "C" {
#endif

int __stdcall NDInstallDevice(IN LPCTSTR szInfPath, IN LPCTSTR szHardwareId);
int __stdcall NDUpdateDriver(IN LPCTSTR szInfPath, IN LPCTSTR szHardwareId);
int __stdcall NDRemoveDevice(IN LPCTSTR szHardwareId);
int __stdcall NDCopyInf(IN TCHAR *szInfFullPath, OUT LPTSTR szOEMInfPath);

#ifdef __cplusplus
}
#endif

// typedef BOOL (WINAPI *SetupSetNonInteractiveModeProto)(IN BOOL NonInteractiveFlag);
//#define SETUPSETNONINTERACTIVEMODE "SetupSetNonInteractiveMode"

#endif
