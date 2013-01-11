#pragma once

#include <pshpack8.h>

typedef enum _XDIMSI_PROCESS_TYPE {
	_XDIMSI_PROCESS_INITIALIZE,
	XDIMSI_PROCESS_INSTALL_FROM_INF_SECTION,
	XDIMSI_PROCESS_INSTALL_LEGACY_PNP_DEVICE,
	XDIMSI_PROCESS_INSTALL_PNP_DEVICE_INF,
	XDIMSI_PROCESS_INSTALL_NETWORK_COMPONENT,
	XDIMSI_PROCESS_UNINSTALL_PNP_DEVICE,
	XDIMSI_PROCESS_UNINSTALL_NETWORK,
	XDIMSI_PROCESS_CLEANUP_OEM_INF,
	XDIMSI_PROCESS_START_SERVICE,
	XDIMSI_PROCESS_STOP_SERVICE,
	XDIMSI_PROCESS_QUEUE_SCHEDULE_REBOOT,
	_XDIMSI_PROCESS_CHECK_SERVICES_INF_SECTION,
	_XDIMSI_PROCESS_TYPE_INVALID
} XDIMSI_PROCESS_TYPE;

const LPCWSTR xMsiProcessTypes[] = {
	L"_ProcessInitialize",
	L"InstallFromInfSection",
	L"InstallLegacyPnpDevice",
	L"InstallPnpDeviceInf",
	L"InstallNetworkComponent",
	L"UninstallPnpDevice",
	L"UninstallNetworkComponent",
	L"CleanupOEMInf",
	L"StartService",
	L"StopService",
	L"QueueScheduleReboot",
	L"_CheckServicesInfSection"
};

C_ASSERT(_XDIMSI_PROCESS_TYPE_INVALID == RTL_NUMBER_OF(xMsiProcessTypes));

typedef struct _XDIMSI_PROCESS_RECORD {
	XDIMSI_PROCESS_TYPE ProcessType;
	LPCWSTR ActionName;
	union {
		LPWSTR HardwareId;
		LPWSTR InfSectionList;
		LPWSTR ServiceName;
	};
	LPWSTR InfPath;
	DWORD Flags;
	INT ErrorNumber;
	DWORD RegRoot;
	LPWSTR RegKey;
	LPWSTR RegName;
	DWORD ProgressTicks;
} XDIMSI_PROCESS_RECORD, *PXDIMSI_PROCESS_RECORD;

typedef struct _XDIMSI_PROCESS_DATA {
	ULONG Version; /* sizeof(XDIMSI_PROCESS_DATA) */
	ULONG Size;    /* total length of the data */
	XDIMSI_PROCESS_TYPE ProcessType;
	ULONG ErrorNumber;
	ULONG Flags;
	ULONG RegRoot;
	ULONG ProgressTicks;
	ULONG HardwareIdOffset;
	ULONG HardwareIdLength;
	ULONG InfPathOffset;
	ULONG InfPathLength;
	ULONG RegKeyOffset;
	ULONG RegKeyLength;
	ULONG RegNameOffset;
	ULONG RegNameLength;
	ULONG ActionOffset;
	ULONG ActionLength;
	UCHAR AdditionalData[1];
} XDIMSI_PROCESS_DATA, *PXDIMSI_PROCESS_DATA;

#include <poppack.h>

HRESULT
pxMsiProcessDataInit(
	__inout PXDIMSI_PROCESS_DATA* ProcessData);

HRESULT
pxDiMsiProcessDataCreate(
	__inout PXDIMSI_PROCESS_DATA* ProcessData,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord);

HRESULT
pxDiMsiProcessDataFree(
	__in PXDIMSI_PROCESS_DATA ProcessData);

HRESULT
pxDiMsiProcessDataDecode(
	__in LPCWSTR StringData,
	__out PXDIMSI_PROCESS_DATA* ProcessData,
	__out_opt LPDWORD OutputSize);

HRESULT
pxDiMsiProcessDataEncode(
	__in PXDIMSI_PROCESS_DATA ProcessData,
	__out LPWSTR* StringData,
	__out_opt LPDWORD StringDataLength);

inline PVOID pxOffsetOf(PVOID Pointer, ULONG Offset)
{
	return reinterpret_cast<UCHAR*>(Pointer) + Offset;
}

