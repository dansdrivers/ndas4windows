#include <windows.h>
#include <tchar.h>
#include <setupapi.h>
#include <winioctl.h>
#include <dbt.h>
#include <initguid.h>
#include "ndasbusctl.h"

#include <strsafe.h>
#ifdef NDASBUSCTL_DLL_IMPL
#define DBGPRN_USE_EXTERN_LEVEL
#endif
#include "xdbgprn.h"

//
// If succeeded, returns the device file of the LANSCSI Bus Enumerator
// If failed, return INVALID_HANDLE_VALUE
//
static HANDLE OpenBusInterface(VOID)
{
	BOOL fSuccess = FALSE;
	DWORD err = ERROR_SUCCESS;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
    HDEVINFO hDevInfoSet = INVALID_HANDLE_VALUE;
	SP_INTERFACE_DEVICE_DATA devIntfData = {0};
    PSP_INTERFACE_DEVICE_DETAIL_DATA devIntfDetailData = NULL;
    ULONG predictedLength = 0;
    ULONG requiredLength = 0;

	hDevInfoSet = SetupDiGetClassDevs (
		(LPGUID)&GUID_NDAS_BUS_ENUMERATOR_INTERFACE_CLASS,
		NULL, // Define no enumerator (global)
		NULL, // Define no
		(DIGCF_PRESENT | // Only Devices present
		DIGCF_INTERFACEDEVICE)); // Function class devices.

    if (INVALID_HANDLE_VALUE == hDevInfoSet)
    {
		DebugPrintErrEx(_T("OpenBusInterface: SetupDiGetClassDevs failed: "));
		goto cleanup;
    }

    devIntfData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	fSuccess = SetupDiEnumDeviceInterfaces (
		hDevInfoSet,
		0, // No care about specific PDOs
		(LPGUID)&GUID_NDAS_BUS_ENUMERATOR_INTERFACE_CLASS,
		0, //
		&devIntfData);

	if (!fSuccess) {
		DebugPrintErrEx(_T("OpenBusInterface: SetupDiEnumDeviceInterfaces failed: "));
		if (ERROR_NO_MORE_ITEMS == GetLastError()) {
			DebugPrint(1, _T("OpenBusInterface: Interface")
				_T(" GUID_NDAS_BUS_ENUMERATOR_INTERFACE_CLASS is not registered.\n"));
		}
		goto cleanup;
	}

	fSuccess = SetupDiGetInterfaceDeviceDetail (
            hDevInfoSet,
            &devIntfData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL // not interested in the specific dev-node
			);

	if (!fSuccess && ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
		DebugPrintErrEx(_T("OpenBusInterface: SetupDiGetInterfaceDeviceDetail failed: "));
		goto cleanup;
	}

    predictedLength = requiredLength;

	devIntfDetailData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, predictedLength);
	devIntfDetailData->cbSize = sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);
    
	fSuccess = SetupDiGetInterfaceDeviceDetail(
		hDevInfoSet,
		&devIntfData,
		devIntfDetailData,
		predictedLength,
		&requiredLength,
		NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("OpenBusInterface: SetupDiGetInterfaceDeviceDetail failed: "));
		goto cleanup;
	}

	DebugPrint(3, _T("OpenBusInterface: Opening %s\n"), devIntfDetailData->DevicePath);

    hDevice = CreateFile (
		devIntfDetailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		0, // FILE_SHARE_READ | FILE_SHARE_WRITE
		NULL, // no SECURITY_ATTRIBUTES structure
		OPEN_EXISTING, // No special create flags
		0, // No special attributes
		NULL); // No template file

    if (INVALID_HANDLE_VALUE == hDevice) {
		DebugPrintErrEx(_T("OpenBusInterface: Device not ready: "));
		goto cleanup;
    }
    
	DebugPrint(3, _T("OpenBusInterface: Device file %s opened successfully.\n"),
		devIntfDetailData->DevicePath);

cleanup:

	err = GetLastError();

	if (INVALID_HANDLE_VALUE != hDevInfoSet) {
		SetupDiDestroyDeviceInfoList(hDevInfoSet);
	}

	if (NULL != devIntfDetailData) {
		HeapFree(GetProcessHeap(), 0, devIntfDetailData);
	}

	SetLastError(err);

	return hDevice;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlGetVersion(
	LPWORD lpVersionMajor, 
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_GET_VERSION VersionData = {0};

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	fSuccess = DeviceIoControl (
		hDevice,
		IOCTL_NDASBUS_GETVERSION,
		NULL,
		0,
		&VersionData,
		sizeof(VersionData),
		&cbReturned, 
		NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlGetVersion failed :"));
	} else {

		if (NULL != lpVersionMajor) *lpVersionMajor = VersionData.VersionMajor;
		if (NULL != lpVersionMinor) *lpVersionMinor = VersionData.VersionMinor;
		if (NULL != lpVersionBuild) *lpVersionBuild = VersionData.VersionBuild;
		if (NULL != lpVersionPrivate) *lpVersionPrivate = VersionData.VersionPrivate;

		DebugPrint(3, _T("NdasBusCtlGetVersion %d.%d.%d.%d completed successfully.\n"),
			VersionData.VersionMajor,
			VersionData.VersionMinor,
			VersionData.VersionBuild,
			VersionData.VersionPrivate);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}
#if 0
NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlGetMiniportVersion(
	DWORD SlotNo, 
	LPWORD lpVersionMajor, 
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate)
{
	NDASSCSI_QUERY_INFO_DATA			query;
	LONG						queryLen;
	NDSCIOCTL_DRVVER			VersionData;
	BOOL						bret;

	queryLen				= sizeof(NDASSCSI_QUERY_INFO_DATA) - sizeof(UCHAR);
	query.Length			= queryLen;
	query.InfoClass			= NdscDriverVersion;
	query.SlotNo			= SlotNo;
	query.QueryDataLength	= 0;
	bret = NdasBusCtlQueryMiniportInformation(
		&query,
		queryLen,
		&VersionData,
		sizeof(VersionData));
	if(bret == FALSE) {
		DebugPrint(1, _T("NdasBusCtlGetMiniportVersion: Slot %d. ErrCode:%lu\n"), 
			SlotNo, GetLastError() );
		return FALSE;
	} else {
		if (NULL != lpVersionMajor) *lpVersionMajor = VersionData.VersionMajor;
		if (NULL != lpVersionMinor) *lpVersionMinor = VersionData.VersionMinor;
		if (NULL != lpVersionBuild) *lpVersionBuild = VersionData.VersionBuild;
		if (NULL != lpVersionPrivate) *lpVersionPrivate = VersionData.VersionPrivate;
	}


	return TRUE;
}
#endif
NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlPlugInEx2(
	ULONG	SlotNo,
	ULONG	MaxOsDataTransferLength, // in bytes
	HANDLE	hEvent,
	HANDLE	hAlarmEvent,
	BOOL	NotRegister
	)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	PNDASBUS_PLUGIN_HARDWARE_EX2	pLanscsiPluginData;	
	DWORD cbLanscsiPluginData = 0;

	DebugPrint(1, _T("NdasBusCtlPlugInEx2: Slot %d, MaxRequestLength %d bytes, hEvent %p, hAlarmEvent %p\n"), 
		SlotNo, MaxOsDataTransferLength, hEvent, hAlarmEvent);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	cbLanscsiPluginData = sizeof (NDASBUS_PLUGIN_HARDWARE_EX2) + 
		NDASMINIPORT_HARDWARE_IDS_W_SIZE;

	pLanscsiPluginData = HeapAlloc(
		GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		cbLanscsiPluginData);
	//
	// The size field should be set to the sizeof the struct as declared
	// and *not* the size of the struct plus the multi_sz
	//
    pLanscsiPluginData->Size = sizeof(NDASBUS_PLUGIN_HARDWARE_EX2);
	pLanscsiPluginData->SerialNo = SlotNo;
	pLanscsiPluginData->MaxOsDataTransferLength = MaxOsDataTransferLength;
	pLanscsiPluginData->DisconEvent = hEvent;
	pLanscsiPluginData->AlarmEvent = hAlarmEvent;

	pLanscsiPluginData->HardwareIDLen = NDASMINIPORT_HARDWARE_IDS_W_SIZE / sizeof(WCHAR);
	CopyMemory(
		pLanscsiPluginData->HardwareIDs,
		NDASMINIPORT_HARDWARE_IDS_W,
        NDASMINIPORT_HARDWARE_IDS_W_SIZE);

	if(NotRegister) {
		pLanscsiPluginData->Flags |= PLUGINFLAG_NOT_REGISTER;
	}

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_NDASBUS_PLUGIN_HARDWARE_EX2,
            pLanscsiPluginData,
			cbLanscsiPluginData,
            pLanscsiPluginData,
			cbLanscsiPluginData,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlPlugInEx2 at slot %d failed: "), SlotNo);
	} else {
		DebugPrintErrEx(_T("NdasBusCtlPlugInEx2 at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();

	HeapFree(GetProcessHeap(), 0, pLanscsiPluginData);
    CloseHandle(hDevice);

	SetLastError(err);

	return fSuccess;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlEject(
	ULONG SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_EJECT_HARDWARE  eject;

	DebugPrint(3, _T("NdasBusCtlEject at slot %d.\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&eject, sizeof(NDASBUS_EJECT_HARDWARE));
	eject.SerialNo = SlotNo;
	eject.Size = sizeof (eject);

	fSuccess = DeviceIoControl (
		hDevice,
		IOCTL_NDASBUS_EJECT_HARDWARE,
		&eject,
		sizeof (eject),
		&eject,
		sizeof (eject),
		&cbReturned, 
		NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlEject at slot %d failed :"), SlotNo);
	} else {
		DebugPrint(3, _T("NdasBusCtlEject at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlUnplug(
	ULONG SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_UNPLUG_HARDWARE unplug;

	DebugPrint(3, _T("NdasBusCtlUnplug at slot %d.\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&unplug, sizeof(NDASBUS_UNPLUG_HARDWARE));
	unplug.SerialNo = SlotNo;
    unplug.Size = sizeof (unplug);

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_NDASBUS_UNPLUG_HARDWARE,
            &unplug,
			sizeof (unplug),
            &unplug,
			sizeof (unplug),
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlUnplug at slot %d failed :"), SlotNo);
	} else {
		DebugPrint(3, _T("NdasBusCtlUnplug at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlAddTarget(
	PNDASBUS_ADD_TARGET_DATA AddTargetData)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;
	
	DebugPrint(1, _T("NdasBusCtlAddTarget: Slot %d.\n"), AddTargetData->ulSlotNo);

	//	Sleep(1000*5);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_ADD_TARGET,
            AddTargetData,
			AddTargetData->ulSize,
            AddTargetData,
			AddTargetData->ulSize,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlAddTarget at slot %d failed :"), AddTargetData->ulSlotNo);
	} else {
		DebugPrint(3, _T("NdasBusCtlAddTarget at slot %d completed successfully.\n"), AddTargetData->ulSlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlRemoveTarget(DWORD SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_REMOVE_TARGET_DATA RemoveTargetData;

	DebugPrint(3, _T("NdasBusCtlRemoveTarget: Slot %d\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&RemoveTargetData, sizeof(RemoveTargetData));

	RemoveTargetData.ulSlotNo = SlotNo;
//	RemoveTargetData->MasterUnitDisk;

    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_REMOVE_TARGET,
            &RemoveTargetData,
			sizeof(NDASBUS_REMOVE_TARGET_DATA),
            &RemoveTargetData,
			sizeof(NDASBUS_REMOVE_TARGET_DATA),
            &cbReturned,
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LanscsiRemoveTarget at slot %d failed: "), SlotNo);
	} else {
		DebugPrint(3, _T("LanscsiRemoveTarget at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlQueryStatus(
	ULONG SlotNo,
	PULONG pStatus)
{
	BOOL fSuccess = FALSE;

	NDASBUS_QUERY_INFORMATION Query;
	NDASBUS_INFORMATION	Information;

	//
	//	Query AccessRight to LanscsiBus
	//
	Query.InfoClass = INFORMATION_PDO;
	Query.Size		= sizeof(Query) ;
	Query.SlotNo	= SlotNo;
	Query.Flags		= 0;

	fSuccess = NdasBusCtlQueryInformation(
				&Query,
				sizeof(Query),
				&Information,
				sizeof(Information));

	if (!fSuccess) {
		return FALSE;
	}

	// ndasscsiioctl.h
	// ADAPTERINFO_STATUS_*
	// ADAPTERINFO_STATUSFLAG_*
	*pStatus = Information.PdoInfo.AdapterStatus;

	return TRUE;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlQueryDeviceMode(
	ULONG SlotNo,
	PULONG pDeviceMode)
{
	BOOL fSuccess = FALSE;

	NDASBUS_QUERY_INFORMATION Query;
	NDASBUS_INFORMATION	Information;

	//
	//	Query AccessRight to LanscsiBus
	//
	Query.InfoClass = INFORMATION_PDO;
	Query.Size		= sizeof(Query);
	Query.SlotNo	= SlotNo;
	Query.Flags		= 0;

	fSuccess = NdasBusCtlQueryInformation(
		&Query,
		sizeof(Query),
		&Information,
		sizeof(Information));

	if (!fSuccess) {
		return FALSE;
	}

	// lurdesc.h
	// DEVMODE_*
	*pDeviceMode = Information.PdoInfo.DeviceMode;

	return TRUE;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlQueryEvent(
	ULONG SlotNo,
	PULONG pStatus)
{
	BOOL fSuccess = FALSE;

	NDASBUS_QUERY_INFORMATION Query;
	NDASBUS_INFORMATION	Information;

	//
	//	Query AccessRight to LanscsiBus
	//
	Query.InfoClass = INFORMATION_PDO;
	Query.Size		= sizeof(Query) ;
	Query.SlotNo	= SlotNo;
	Query.Flags		= NDASBUS_QUERYFLAG_EVENTQUEUE;

	fSuccess = NdasBusCtlQueryInformation(
				&Query,
				sizeof(Query),
				&Information,
				sizeof(Information));

	if (!fSuccess) {
		return FALSE;
	}

	// ndasscsiioctl.h
	// ADAPTERINFO_STATUS_*
	// ADAPTERINFO_STATUSFLAG_*
	*pStatus = Information.PdoInfo.AdapterStatus;

	return TRUE;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlQueryDvdStatus(
	DWORD SlotNo,
	PULONG pDvdStatus)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_DVD_STATUS DvdStatusIn;
	NDASBUS_DVD_STATUS DvdStatusOut;
	
	hDevice = OpenBusInterface();
	if (INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}
	
	DvdStatusIn.SlotNo = SlotNo;
	DvdStatusIn.Size = sizeof(NDASBUS_DVD_STATUS);

	fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_DVD_GET_STATUS,
			&DvdStatusIn, 
			sizeof(NDASBUS_DVD_STATUS),
			&DvdStatusOut, 
			sizeof(NDASBUS_DVD_STATUS),
			&cbReturned, 
			NULL);
			
	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlQueryDvdStatus: BUSENUM_QUERY_NODE_ALIVE failed: "));
	} else {
		DebugPrint(3, _T("NdasBusCtlQueryDvdStatus: Slot %d, Status %d\n"), 
			SlotNo, DvdStatusOut.Status);
		*pDvdStatus = DvdStatusOut.Status;
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);
	
	return fSuccess;
}


NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryNodeAlive(
	DWORD SlotNo,
	LPBOOL pbAlive,
	LPBOOL pbAdapterHasError)
{
	BOOL fSuccess = FALSE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_NODE_ALIVE_IN	nodeAliveIn;
	NDASBUS_NODE_ALIVE_OUT	nodeAliveOut;

	hDevice = OpenBusInterface();
	if (INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	nodeAliveIn.SlotNo = SlotNo;

	fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_QUERY_NODE_ALIVE,
			&nodeAliveIn, 
			sizeof(NDASBUS_NODE_ALIVE_IN),
			&nodeAliveOut, 
			sizeof(NDASBUS_NODE_ALIVE_OUT),
			&cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlQueryNodeAlive: BUSENUM_QUERY_NODE_ALIVE failed: "));
	} else {
		*pbAlive = nodeAliveOut.bAlive;
		*pbAdapterHasError = nodeAliveOut.bHasError;
	}
		
	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);
	
	return fSuccess;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlQueryInformation(
	PNDASBUS_QUERY_INFORMATION Query,
	DWORD QueryLength,
	PNDASBUS_INFORMATION Information,
	DWORD InformationLength)
{
	BOOL fSuccess = FALSE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    DWORD cbReturned = 0;
	DWORD err;

	hDevice = OpenBusInterface();

	if (INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}
	
    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_QUERY_INFORMATION,
			Query,
			QueryLength,
			Information, 
			InformationLength,
			&cbReturned, 
			NULL);

	err = GetLastError();
    CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlQueryMiniportInformation(
	PNDASSCSI_QUERY_INFO_DATA Query,
	DWORD QueryLength,
	PVOID Information,
	DWORD InformationLength)
{
	BOOL fSuccess = FALSE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    DWORD cbReturned = 0;
	DWORD err;

	hDevice = OpenBusInterface();

	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_QUERY_NDASSCSIINFO,
			Query,
			QueryLength,
			Information, 
			InformationLength,
			&cbReturned, 
			NULL);

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


NDASBUSCTLAPI
BOOL 
WINAPI
NdasBusCtlQueryMiniportFullInformation(
	ULONG						SlotNo,
	PNDSCIOCTL_LURINFO	*LurFullInfo) 
{
	NDASSCSI_QUERY_INFO_DATA			query;
	LONG						queryLen;
	NDSCIOCTL_LURINFO	tmpLurFullInfo;
	PNDSCIOCTL_LURINFO	lurFullInfo;
	LONG						infoLen;
	BOOL						bret;

	*LurFullInfo = NULL;

	queryLen				= sizeof(NDASSCSI_QUERY_INFO_DATA) - sizeof(UCHAR);
	query.Length			= queryLen;
	query.InfoClass			= NdscLurInformation;
	query.NdasScsiAddress.SlotNo = SlotNo;
	query.QueryDataLength	= 0;
	bret = NdasBusCtlQueryMiniportInformation(
					&query,
					queryLen,
					&tmpLurFullInfo,
					sizeof(tmpLurFullInfo));

	if(bret == FALSE) {
		if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return FALSE;
		}
	}

	infoLen = tmpLurFullInfo.Length;
	lurFullInfo = HeapAlloc(GetProcessHeap(), 0, infoLen);
	if (NULL == lurFullInfo) {
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}

	bret = NdasBusCtlQueryMiniportInformation(
					&query,
					queryLen,
					lurFullInfo,
					infoLen);

	if(bret == FALSE) {
		HeapFree(GetProcessHeap(), 0, lurFullInfo);
		return FALSE;
	}

	*LurFullInfo = lurFullInfo;

	return TRUE;
}

NDASBUSCTLAPI
BOOL 
WINAPI
NdasBusCtlQueryPdoSlotList(
	PNDASBUS_INFORMATION *BusInfo
) {
	BOOL						bret;
	NDASBUS_QUERY_INFORMATION	query;
	NDASBUS_INFORMATION			tmpInfo;
	PNDASBUS_INFORMATION		info;
	LONG						infoLen;

	*BusInfo = NULL;

	query.Size		= sizeof(query);
	query.InfoClass = INFORMATION_PDOSLOTLIST;
	query.Flags		= 0;
	query.SlotNo	= 0;
	bret = NdasBusCtlQueryInformation(
					&query,
					sizeof(query),
					&tmpInfo,
					sizeof(tmpInfo)
				);
	if(bret == FALSE) {
		if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return FALSE;
		}
	}

	infoLen = tmpInfo.Size;

	info = HeapAlloc(GetProcessHeap(), 0, infoLen);
	if (NULL == info) {
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}

	bret = NdasBusCtlQueryInformation(
					&query,
					sizeof(query),
					info,
					infoLen
				);
	if(bret == FALSE) {
		HeapFree(GetProcessHeap(), 0, info);
		return FALSE;
	}

	*BusInfo = info;

	return TRUE;
}


NDASBUSCTLAPI
BOOL 
WINAPI
NdasBusCtlQueryPdoEvent(
	ULONG	SlotNo,
	PHANDLE	AlarmEvent,
	PHANDLE	DisconnectEvent
) {
	BOOL						bret;
	NDASBUS_QUERY_INFORMATION	query;
	NDASBUS_INFORMATION			tmpInfo;

	query.Size		= sizeof(query);
	query.InfoClass = INFORMATION_PDOEVENT;
	query.Flags		= NDASBUS_QUERYFLAG_USERHANDLE;
	query.SlotNo	= SlotNo;
	bret = NdasBusCtlQueryInformation(
					&query,
					sizeof(query),
					&tmpInfo,
					sizeof(tmpInfo)
				);
	if(bret == FALSE) {
		return FALSE;
	}

	if(!(tmpInfo.PdoEvents.Flags & NDASBUS_QUERYFLAG_USERHANDLE)) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}
	if(tmpInfo.PdoEvents.SlotNo != SlotNo) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	*AlarmEvent = tmpInfo.PdoEvents.AlarmEvent;
	*DisconnectEvent = tmpInfo.PdoEvents.DisconEvent;

	return TRUE;
}


NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlStartStopRegistrarEnum(
	BOOL	bOnOff,
	LPBOOL	pbPrevState)
{
	BOOL fSuccess = FALSE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    DWORD cbReturned = 0;
	DWORD err;
	DWORD onOff;

	hDevice = OpenBusInterface();
	if (INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	onOff = bOnOff;

	fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_STARTSTOP_REGISTRARENUM,
			&onOff, 
			sizeof(DWORD),
			&onOff, 
			sizeof(DWORD),
			&cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlOnOffRegistrar: STOP_REGISTRAR failed: "));
	} else {
		if(pbPrevState)
			*pbPrevState = (onOff != 0);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);
	
	return fSuccess;
}

NDASBUSCTLAPI 
BOOL 
WINAPI
NdasBusCtlQueryPdoFileHandle(
	ULONG	SlotNo,
	PHANDLE	PdoFileHandle)
{
	BOOL						bret;
	NDASBUS_QUERY_INFORMATION	query;
	NDASBUS_INFORMATION			tmpInfo;

	query.Size		= sizeof(query);
	query.InfoClass = INFORMATION_PDOFILEHANDLE;
	query.Flags		= NDASBUS_QUERYFLAG_USERHANDLE;
	query.SlotNo	= SlotNo;
	bret = NdasBusCtlQueryInformation(
					&query,
					sizeof(query),
					&tmpInfo,
					sizeof(tmpInfo)
				);
	if(bret == FALSE) {
		return FALSE;
	}

	if(!(tmpInfo.PdoEvents.Flags & NDASBUS_QUERYFLAG_USERHANDLE)) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}
	if(tmpInfo.PdoEvents.SlotNo != SlotNo) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	*PdoFileHandle = tmpInfo.PdoFile.PdoFileHandle;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// NdasBus registrar control
//
//

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlRegisterDevice(
	ULONG	SlotNo,
	ULONG	MaxOsDataTransferLength
	)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	PNDASBUS_PLUGIN_HARDWARE_EX2	registerDev;	
	DWORD cbRegisterDev = 0;

	DebugPrint(1, _T("NdasBusCtlRegisterDevice: Slot %d, MaxRequestBlock %d\n"), 
		SlotNo, MaxOsDataTransferLength);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	cbReturned = sizeof (NDASBUS_PLUGIN_HARDWARE_EX2) + 
		NDASMINIPORT_HARDWARE_IDS_W_SIZE;

	registerDev = HeapAlloc(
		GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		cbReturned);
	//
	// The size field should be set to the sizeof the struct as declared
	// and *not* the size of the struct plus the multi_sz
	//
    registerDev->Size = sizeof(NDASBUS_PLUGIN_HARDWARE_EX2);
	registerDev->SerialNo = SlotNo;
	registerDev->MaxOsDataTransferLength = MaxOsDataTransferLength;

	registerDev->HardwareIDLen = NDASMINIPORT_HARDWARE_IDS_W_SIZE / sizeof(WCHAR);
	CopyMemory(
		registerDev->HardwareIDs,
		NDASMINIPORT_HARDWARE_IDS_W,
        NDASMINIPORT_HARDWARE_IDS_W_SIZE);

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_NDASBUS_REGISTER_DEVICE,
            registerDev,
			cbReturned,
            registerDev,
			cbReturned,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlRegisterDevice at slot %d failed: "), SlotNo);
	} else {
		DebugPrintErrEx(_T("NdasBusCtlRegisterDevice at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();

	HeapFree(GetProcessHeap(), 0, registerDev);
    CloseHandle(hDevice);

	SetLastError(err);

	return fSuccess;
}

NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlUnregisterDevice(
	ULONG SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_UNREGISTER_NDASDEV unregisterDev;

	DebugPrint(3, _T("NdasBusCtlUnplug at slot %d.\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&unregisterDev, sizeof(NDASBUS_UNREGISTER_NDASDEV));
	unregisterDev.SlotNo = SlotNo;

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_NDASBUS_UNREGISTER_DEVICE,
            &unregisterDev,
			sizeof (unregisterDev),
            &unregisterDev,
			sizeof (unregisterDev),
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlUnregisterDevice at slot %d failed :"), SlotNo);
	} else {
		DebugPrint(3, _T("NdasBusCtlUnregisterDevice at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlRegisterTarget(
	PNDASBUS_ADD_TARGET_DATA RegisterTargetData)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;
	
	DebugPrint(1, _T("NdasBusCtlAddTarget: Slot %d.\n"), RegisterTargetData->ulSlotNo);

	//	Sleep(1000*5);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_REGISTER_TARGET,
            RegisterTargetData,
			RegisterTargetData->ulSize,
            RegisterTargetData,
			RegisterTargetData->ulSize,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("NdasBusCtlAddTarget at slot %d failed :"), RegisterTargetData->ulSlotNo);
	} else {
		DebugPrint(3, _T("NdasBusCtlAddTarget at slot %d completed successfully.\n"), RegisterTargetData->ulSlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


NDASBUSCTLAPI BOOL WINAPI
NdasBusCtlUnregisterTarget(DWORD SlotNo, DWORD TargetId)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	NDASBUS_UNREGISTER_TARGET unregisterTargetData;

	DebugPrint(3, _T("NdasBusCtlRemoveTarget: Slot %d\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&unregisterTargetData, sizeof(unregisterTargetData));

	unregisterTargetData.SlotNo = SlotNo;
	unregisterTargetData.TargetId = 0;

    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_NDASBUS_UNREGISTER_TARGET,
            &unregisterTargetData,
			sizeof(NDASBUS_REMOVE_TARGET_DATA),
            &unregisterTargetData,
			sizeof(NDASBUS_REMOVE_TARGET_DATA),
            &cbReturned,
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LanscsiRemoveTarget at slot %d failed: "), SlotNo);
	} else {
		DebugPrint(3, _T("LanscsiRemoveTarget at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}
