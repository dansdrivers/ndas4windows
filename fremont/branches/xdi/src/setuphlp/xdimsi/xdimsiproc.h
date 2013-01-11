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
pxDiMsiInitializeScheduledAction(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallFromInfSection(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallLegacyPnpDeviceRollback(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallLegacyPnpDevice(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallPnpDeviceInfRollback(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallPnpDeviceInf(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiUninstallPnpDevice(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallNetworkComponentRollback(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiInstallNetworkComponent(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiUninstallNetworkComponent(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiCleanupOEMInf(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

typedef enum _XDIMSI_CHECK_SERVICES_FLAGS {
	XDIMSI_CHECK_SERVICES_USE_SECTION = 0x0,
	XDIMSI_CHECK_SERVICES_USE_HARDWARE_ID = 0x1, /* use section as a hardware id */
	XDIMSI_CHECK_SERVICES_USE_FORCEREBOOT = 0x2  /* use ForceReboot instead of ScheduleReboot */
} XDIMSI_CHECK_SERVICES_FLAGS;

UINT
pxDiMsiCheckServicesInfSection(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

typedef enum _XDIMSI_START_SERVICE_FLAGS {
	XDIMSI_START_SERVICE_FLAGS_IGNORE_FAILURE = 0x00000001 /* ignore start failure */
} XDIMSI_START_SERVICE_FLAGS;

UINT
pxDiMsiStartService(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiStopService(
	__in MSIHANDLE hInstall, 
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

UINT
pxDiMsiQueueScheduleReboot(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);
