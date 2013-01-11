#include <windows.h>
#include <tchar.h>
#include <setupapi.h>
#include <winioctl.h>
#include <dbt.h>
#include <initguid.h>
#include "lsbusctl.h"

#include <strsafe.h>
#ifdef LSBUSCTL_DLL_IMPL
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
		(LPGUID)&GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS,
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
		(LPGUID)&GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS,
		0, //
		&devIntfData);

	if (!fSuccess) {
		DebugPrintErrEx(_T("OpenBusInterface: SetupDiEnumDeviceInterfaces failed: "));
		if (ERROR_NO_MORE_ITEMS == GetLastError()) {
			DebugPrint(1, _T("OpenBusInterface: Interface")
				_T(" GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS is not registered.\n"));
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

LSBUSCTLAPI BOOL WINAPI
LsBusCtlGetVersion(
	LPWORD lpVersionMajor, 
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	BUSENUM_GET_VERSION VersionData = {0};

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	fSuccess = DeviceIoControl (
		hDevice,
		IOCTL_LANSCSI_GETVERSION,
		NULL,
		0,
		&VersionData,
		sizeof(VersionData),
		&cbReturned, 
		NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlGetVersion failed :"));
	} else {

		if (NULL != lpVersionMajor) *lpVersionMajor = VersionData.VersionMajor;
		if (NULL != lpVersionMinor) *lpVersionMinor = VersionData.VersionMinor;
		if (NULL != lpVersionBuild) *lpVersionBuild = VersionData.VersionBuild;
		if (NULL != lpVersionPrivate) *lpVersionPrivate = VersionData.VersionPrivate;

		DebugPrint(3, _T("LsBusCtlGetVersion %d.%d.%d.%d completed successfully.\n"),
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

LSBUSCTLAPI BOOL WINAPI
LsBusCtlGetMiniportVersion(
	DWORD SlotNo, 
	LPWORD lpVersionMajor, 
	LPWORD lpVersionMinor,
	LPWORD lpVersionBuild,
	LPWORD lpVersionPrivate)
{
	LSMPIOCTL_QUERYINFO			query;
	LONG						queryLen;
	LSMPIOCTL_DRVVER			VersionData;
	BOOL						bret;

	queryLen				= sizeof(LSMPIOCTL_QUERYINFO) - sizeof(UCHAR);
	query.Length			= queryLen;
	query.InfoClass			= LsmpDriverVersion;
	query.SlotNo			= SlotNo;
	query.QueryDataLength	= 0;
	bret = LsBusCtlQueryMiniportInformation(
		&query,
		queryLen,
		&VersionData,
		sizeof(VersionData));
	if(bret == FALSE) {
		DebugPrint(1, _T("LsBusCtlGetMiniportVersion: Slot %d. ErrCode:%lu\n"), 
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

LSBUSCTLAPI BOOL WINAPI
LsBusCtlPlugInEx(
	ULONG	SlotNo,
	ULONG	MaxRequestBlocks,
	HANDLE	hEvent,
	HANDLE	hAlarmEvent)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	PBUSENUM_PLUGIN_HARDWARE_EX	pLanscsiPluginData;	
	DWORD cbLanscsiPluginData = 0;

	DebugPrint(1, _T("LsBusCtlPlugInEx: Slot %d, MaxRequestBlock %d, hEvent %p, hAlarmEvent %p\n"), 
		SlotNo, MaxRequestBlocks, hEvent, hAlarmEvent);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	cbLanscsiPluginData = sizeof (BUSENUM_PLUGIN_HARDWARE_EX) + 
		LSMINIPORT_HARDWARE_IDS_W_SIZE;

	pLanscsiPluginData = HeapAlloc(
		GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		cbLanscsiPluginData);
	//
	// The size field should be set to the sizeof the struct as declared
	// and *not* the size of the struct plus the multi_sz
	//
    pLanscsiPluginData->Size = sizeof(BUSENUM_PLUGIN_HARDWARE_EX);
	pLanscsiPluginData->SlotNo = SlotNo;
	pLanscsiPluginData->MaxRequestBlocks = MaxRequestBlocks;
	pLanscsiPluginData->phEvent = &hEvent;
	pLanscsiPluginData->phAlarmEvent = &hAlarmEvent;
	
	CopyMemory(
		pLanscsiPluginData->HardwareIDs,
		LSMINIPORT_HARDWARE_IDS_W,
        LSMINIPORT_HARDWARE_IDS_W_SIZE);

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_BUSENUM_PLUGIN_HARDWARE_EX,
            pLanscsiPluginData,
			cbLanscsiPluginData,
            pLanscsiPluginData,
			cbLanscsiPluginData,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlPlugInEx at slot %d failed: "), SlotNo);
	} else {
		DebugPrintErrEx(_T("LsBusCtlPlugInEx at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();

	HeapFree(GetProcessHeap(), 0, pLanscsiPluginData);
    CloseHandle(hDevice);

	SetLastError(err);

	return fSuccess;
}

LSBUSCTLAPI BOOL WINAPI
LsBusCtlPlugInEx2(
	ULONG	SlotNo,
	ULONG	MaxRequestBlocks,
	HANDLE	hEvent,
	HANDLE	hAlarmEvent,
	BOOL	NotRegister
	)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	PBUSENUM_PLUGIN_HARDWARE_EX2	pLanscsiPluginData;	
	DWORD cbLanscsiPluginData = 0;

	DebugPrint(1, _T("LsBusCtlPlugInEx2: Slot %d, MaxRequestBlock %d, hEvent %p, hAlarmEvent %p\n"), 
		SlotNo, MaxRequestBlocks, hEvent, hAlarmEvent);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	cbLanscsiPluginData = sizeof (BUSENUM_PLUGIN_HARDWARE_EX2) + 
		LSMINIPORT_HARDWARE_IDS_W_SIZE;

	pLanscsiPluginData = HeapAlloc(
		GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		cbLanscsiPluginData);
	//
	// The size field should be set to the sizeof the struct as declared
	// and *not* the size of the struct plus the multi_sz
	//
    pLanscsiPluginData->Size = sizeof(BUSENUM_PLUGIN_HARDWARE_EX2);
	pLanscsiPluginData->SlotNo = SlotNo;
	pLanscsiPluginData->MaxRequestBlocks = MaxRequestBlocks;
	pLanscsiPluginData->phEvent = &hEvent;
	pLanscsiPluginData->phAlarmEvent = &hAlarmEvent;

	pLanscsiPluginData->HardwareIDLen = LSMINIPORT_HARDWARE_IDS_W_SIZE / sizeof(WCHAR);
	CopyMemory(
		pLanscsiPluginData->HardwareIDs,
		LSMINIPORT_HARDWARE_IDS_W,
        LSMINIPORT_HARDWARE_IDS_W_SIZE);

	if(NotRegister) {
		pLanscsiPluginData->Flags |= PLUGINFLAG_NOT_REGISTER;
	}

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_BUSENUM_PLUGIN_HARDWARE_EX2,
            pLanscsiPluginData,
			cbLanscsiPluginData,
            pLanscsiPluginData,
			cbLanscsiPluginData,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlPlugInEx2 at slot %d failed: "), SlotNo);
	} else {
		DebugPrintErrEx(_T("LsBusCtlPlugInEx2 at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();

	HeapFree(GetProcessHeap(), 0, pLanscsiPluginData);
    CloseHandle(hDevice);

	SetLastError(err);

	return fSuccess;
}

LSBUSCTLAPI BOOL WINAPI
LsBusCtlEject(
	ULONG SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	BUSENUM_EJECT_HARDWARE  eject;

	DebugPrint(3, _T("LsBusCtlEject at slot %d.\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&eject, sizeof(BUSENUM_EJECT_HARDWARE));
	eject.SlotNo = SlotNo;
	eject.Size = sizeof (eject);

	fSuccess = DeviceIoControl (
		hDevice,
		IOCTL_BUSENUM_EJECT_HARDWARE,
		&eject,
		sizeof (eject),
		&eject,
		sizeof (eject),
		&cbReturned, 
		NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlEject at slot %d failed :"), SlotNo);
	} else {
		DebugPrint(3, _T("LsBusCtlEject at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


LSBUSCTLAPI BOOL WINAPI
LsBusCtlUnplug(
	ULONG SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	BUSENUM_UNPLUG_HARDWARE unplug;

	DebugPrint(3, _T("LsBusCtlUnplug at slot %d.\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&unplug, sizeof(BUSENUM_UNPLUG_HARDWARE));
	unplug.SlotNo = SlotNo;
    unplug.Size = sizeof (unplug);

    fSuccess = DeviceIoControl (
			hDevice,
			IOCTL_BUSENUM_UNPLUG_HARDWARE,
            &unplug,
			sizeof (unplug),
            &unplug,
			sizeof (unplug),
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlUnplug at slot %d failed :"), SlotNo);
	} else {
		DebugPrint(3, _T("LsBusCtlUnplug at slot %d completed successfully.\n"), SlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}

LSBUSCTLAPI BOOL WINAPI
LsBusCtlAddTarget(
	PLANSCSI_ADD_TARGET_DATA AddTargetData)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;
	
	DebugPrint(1, _T("LsBusCtlAddTarget: Slot %d.\n"), AddTargetData->ulSlotNo);

	//	Sleep(1000*5);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_LANSCSI_ADD_TARGET,
            AddTargetData,
			AddTargetData->ulSize,
            AddTargetData,
			AddTargetData->ulSize,
            &cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlAddTarget at slot %d failed :"), AddTargetData->ulSlotNo);
	} else {
		DebugPrint(3, _T("LsBusCtlAddTarget at slot %d completed successfully.\n"), AddTargetData->ulSlotNo);
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return fSuccess;
}


LSBUSCTLAPI BOOL WINAPI
LsBusCtlRemoveTarget(DWORD SlotNo)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	LANSCSI_REMOVE_TARGET_DATA RemoveTargetData;

	DebugPrint(3, _T("LsBusCtlRemoveTarget: Slot %d\n"), SlotNo);

	hDevice = OpenBusInterface();
	if(INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	ZeroMemory(&RemoveTargetData, sizeof(RemoveTargetData));

	RemoveTargetData.ulSlotNo = SlotNo;
//	RemoveTargetData->MasterUnitDisk;

    fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_LANSCSI_REMOVE_TARGET,
            &RemoveTargetData,
			sizeof(LANSCSI_REMOVE_TARGET_DATA),
            &RemoveTargetData,
			sizeof(LANSCSI_REMOVE_TARGET_DATA),
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

LSBUSCTLAPI BOOL WINAPI
LsBusCtlQueryStatus(
	ULONG SlotNo,
	PULONG pStatus)
{
	BOOL fSuccess = FALSE;

	BUSENUM_QUERY_INFORMATION Query;
	BUSENUM_INFORMATION	Information;

	//
	//	Query AccessRight to LanscsiBus
	//
	Query.InfoClass = INFORMATION_PDO;
	Query.Size		= sizeof(Query) ;
	Query.SlotNo	= SlotNo;

	fSuccess = LsBusCtlQueryInformation(
				&Query,
				sizeof(Query),
				&Information,
				sizeof(Information));

	if (!fSuccess) {
		return FALSE;
	}

	// lsminiportioctl.h
	// ADAPTERINFO_STATUS_*
	// ADAPTERINFO_STATUSFLAG_*
	*pStatus = Information.PdoInfo.AdapterStatus;

	return TRUE;
}

LSBUSCTLAPI BOOL WINAPI
LsBusCtlQueryDvdStatus(
	DWORD SlotNo,
	PULONG pDvdStatus)
{
	BOOL fSuccess = FALSE;
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	DWORD cbReturned = 0;
	DWORD err;

	BUSENUM_DVD_STATUS DvdStatusIn;
	BUSENUM_DVD_STATUS DvdStatusOut;
	
	hDevice = OpenBusInterface();
	if (INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}
	
	DvdStatusIn.SlotNo = SlotNo;
	DvdStatusIn.Size = sizeof(BUSENUM_DVD_STATUS);

	fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_DVD_GET_STATUS,
			&DvdStatusIn, 
			sizeof(BUSENUM_DVD_STATUS),
			&DvdStatusOut, 
			sizeof(BUSENUM_DVD_STATUS),
			&cbReturned, 
			NULL);
			
	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlQueryDvdStatus: BUSENUM_QUERY_NODE_ALIVE failed: "));
	} else {
		DebugPrint(3, _T("LsBusCtlQueryDvdStatus: Slot %d, Status %d\n"), 
			SlotNo, DvdStatusOut.Status);
		*pDvdStatus = DvdStatusOut.Status;
	}

	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);
	
	return fSuccess;
}


LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryNodeAlive(
	DWORD SlotNo,
	LPBOOL pbAlive,
	LPBOOL pbAdapterHasError)
{
	BOOL fSuccess = FALSE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    DWORD cbReturned = 0;
	DWORD err;

	BUSENUM_NODE_ALIVE_IN	nodeAliveIn;
	BUSENUM_NODE_ALIVE_OUT	nodeAliveOut;

	hDevice = OpenBusInterface();
	if (INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	nodeAliveIn.SlotNo = SlotNo;

	fSuccess = DeviceIoControl(
			hDevice,
			IOCTL_BUSENUM_QUERY_NODE_ALIVE,
			&nodeAliveIn, 
			sizeof(BUSENUM_NODE_ALIVE_IN),
			&nodeAliveOut, 
			sizeof(BUSENUM_NODE_ALIVE_OUT),
			&cbReturned, 
			NULL);

	if (!fSuccess) {
		DebugPrintErrEx(_T("LsBusCtlQueryNodeAlive: BUSENUM_QUERY_NODE_ALIVE failed: "));
	} else {
		*pbAlive = nodeAliveOut.bAlive;
		*pbAdapterHasError = nodeAliveOut.bHasError;
	}
		
	err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);
	
	return fSuccess;
}

LSBUSCTLAPI BOOL WINAPI
LsBusCtlQueryInformation(
	PBUSENUM_QUERY_INFORMATION Query,
	DWORD QueryLength,
	PBUSENUM_INFORMATION Information,
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
			IOCTL_BUSENUM_QUERY_INFORMATION,
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


LSBUSCTLAPI BOOL WINAPI
LsBusCtlQueryMiniportInformation(
	PLSMPIOCTL_QUERYINFO Query,
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
			IOCTL_LANSCSI_QUERY_LSMPINFORMATION,
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


LSBUSCTLAPI
BOOL 
WINAPI
LsBusCtlQueryMiniportFullInformation(
	ULONG						SlotNo,
	PLSMPIOCTL_ADAPTERLURINFO	*LurFullInfo) 
{
	LSMPIOCTL_QUERYINFO			query;
	LONG						queryLen;
	LSMPIOCTL_ADAPTERLURINFO	tmpLurFullInfo;
	PLSMPIOCTL_ADAPTERLURINFO	lurFullInfo;
	LONG						infoLen;
	BOOL						bret;

	*LurFullInfo = NULL;

	queryLen				= sizeof(LSMPIOCTL_QUERYINFO) - sizeof(UCHAR);
	query.Length			= queryLen;
	query.InfoClass			= LsmpAdapterLurInformation;
	query.SlotNo			= SlotNo;
	query.QueryDataLength	= 0;
	bret = LsBusCtlQueryMiniportInformation(
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

	bret = LsBusCtlQueryMiniportInformation(
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

LSBUSCTLAPI
BOOL 
WINAPI
LsBusCtlQueryPdoSlotList(
	PBUSENUM_INFORMATION *BusInfo
) {
	BOOL						bret;
	BUSENUM_QUERY_INFORMATION	query;
	BUSENUM_INFORMATION			tmpInfo;
	PBUSENUM_INFORMATION		info;
	LONG						infoLen;

	*BusInfo = NULL;

	query.Size		= sizeof(query);
	query.InfoClass = INFORMATION_PDOSLOTLIST;
	query.Flags		= 0;
	query.SlotNo	= 0;
	bret = LsBusCtlQueryInformation(
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

	bret = LsBusCtlQueryInformation(
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


LSBUSCTLAPI
BOOL 
WINAPI
LsBusCtlQueryPdoEvent(
	ULONG	SlotNo,
	PHANDLE	AlarmEvent,
	PHANDLE	DisconnectEvent
) {
	BOOL						bret;
	BUSENUM_QUERY_INFORMATION	query;
	BUSENUM_INFORMATION			tmpInfo;

	query.Size		= sizeof(query);
	query.InfoClass = INFORMATION_PDOEVENT;
	query.Flags		= LSBUS_QUERYFLAG_USERHANDLE;
	query.SlotNo	= SlotNo;
	bret = LsBusCtlQueryInformation(
					&query,
					sizeof(query),
					&tmpInfo,
					sizeof(tmpInfo)
				);
	if(bret == FALSE) {
		return FALSE;
	}

	if(!(tmpInfo.PdoEvents.Flags & LSBUS_QUERYFLAG_USERHANDLE)) {
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


LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlStartStopRegistrarEnum(
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
			IOCTL_LANSCSI_STARTSTOP_REGISTRARENUM,
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

LSBUSCTLAPI 
BOOL 
WINAPI
LsBusCtlQueryPdoFileHandle(
	ULONG	SlotNo,
	PHANDLE	PdoFileHandle)
{
	BOOL						bret;
	BUSENUM_QUERY_INFORMATION	query;
	BUSENUM_INFORMATION			tmpInfo;

	query.Size		= sizeof(query);
	query.InfoClass = INFORMATION_PDOFILEHANDLE;
	query.Flags		= LSBUS_QUERYFLAG_USERHANDLE;
	query.SlotNo	= SlotNo;
	bret = LsBusCtlQueryInformation(
					&query,
					sizeof(query),
					&tmpInfo,
					sizeof(tmpInfo)
				);
	if(bret == FALSE) {
		return FALSE;
	}

	if(!(tmpInfo.PdoEvents.Flags & LSBUS_QUERYFLAG_USERHANDLE)) {
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
