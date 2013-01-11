#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MSICALL __stdcall

UINT MSICALL xDiMsiProcessDrivers(MSIHANDLE hInstall);
UINT MSICALL xDiMsiProcessDrivers1(MSIHANDLE hInstall);
UINT MSICALL xDiMsiProcessDrivers2(MSIHANDLE hInstall);
UINT MSICALL xDiMsiProcessDrivers3(MSIHANDLE hInstall);
UINT MSICALL xDiMsiProcessDrivers4(MSIHANDLE hInstall);
UINT MSICALL xDiMsiProcessDrivers5(MSIHANDLE hInstall);
UINT MSICALL xDiMsiProcessDriverScheduled(MSIHANDLE hInstall);
UINT MSICALL xDiMsiUpdateScheduledReboot(MSIHANDLE hInstall);

#ifdef __cplusplus
}
#endif
