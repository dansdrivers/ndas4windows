#pragma once

typedef struct _XDIMSI_PROCESS_RECORD XDIMSI_PROCESS_RECORD;

typedef UINT (*XDIMSI_PROC_FUNC)(
	__in MSIHANDLE hInstall,
	__in const struct _XDIMSI_PROCESS_RECORD* ProcessRecord);

typedef enum _XDIMSI_INSTALL_INF_FLAGS {
	XDIMSI_INSTALL_INF_FLAGS_NONE                  = 0x0,
	XDIMSI_INSTALL_INF_FLAGS_IGNORE_FILE_REBOOT    = 0x1,
	XDIMSI_INSTALL_INF_FLAGS_IGNORE_SERVICE_REBOOT = 0x2
} XDIMSI_INSTALL_INF_FLAGS;

UINT
xDiMsipInitializeScheduledAction(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallFromInfSection(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallLegacyPnpDeviceRollback(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallLegacyPnpDevice(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallPnpDeviceInfRollback(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallPnpDeviceInf(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipUninstallPnpDevice(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallNetworkComponentRollback(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipInstallNetworkComponent(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipUninstallNetworkComponent(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipCleanupOEMInf(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

typedef enum _XDIMSI_CHECK_SERVICES_FLAGS {
	XDIMSI_CHECK_SERVICES_USE_SECTION = 0x0,
	XDIMSI_CHECK_SERVICES_USE_HARDWARE_ID = 0x1, /* use section as a hardware id */
	XDIMSI_CHECK_SERVICES_USE_FORCEREBOOT = 0x2  /* use ForceReboot instead of ScheduleReboot */
} XDIMSI_CHECK_SERVICES_FLAGS;

UINT
xDiMsipCheckServicesInfSection(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

typedef enum _XDIMSI_START_SERVICE_FLAGS {
	XDIMSI_START_SERVICE_FLAGS_IGNORE_FAILURE = 0x00000001 /* ignore start failure */
} XDIMSI_START_SERVICE_FLAGS;

UINT
xDiMsipStartService(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipStopService(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
xDiMsipQueueScheduleReboot(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);
