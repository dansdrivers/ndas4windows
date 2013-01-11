#pragma once

#ifdef __cplusplus
extern "C" {
#endif

UINT __stdcall xDiMsiProcessDrivers(MSIHANDLE hInstall);
UINT __stdcall xDiMsiProcessDrivers1(MSIHANDLE hInstall);
UINT __stdcall xDiMsiProcessDrivers2(MSIHANDLE hInstall);
UINT __stdcall xDiMsiProcessDrivers3(MSIHANDLE hInstall);
UINT __stdcall xDiMsiProcessDrivers4(MSIHANDLE hInstall);
UINT __stdcall xDiMsiProcessDrivers5(MSIHANDLE hInstall);
UINT __stdcall xDiMsiProcessDriverScheduled(MSIHANDLE hInstall);
UINT __stdcall xDiMsiUpdateScheduledReboot(MSIHANDLE hInstall);

#ifdef __cplusplus
}
#endif
