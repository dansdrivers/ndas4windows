#include <basetyps.h>
#include <stdlib.h>
#include <wtypes.h>
#include <SetupAPI.h>
#include <stdio.h>
#include <string.h>
#include <winioctl.h>
#include <tchar.h>
#include <windows.h>
#include <initguid.h>
#include <dbt.h>
#include "lsbusctl.h"


#ifdef _DEBUG	

static ULONG DebugPrintLevel = 1;

#define DebugPrint(_l_, _x_)	\
	if(_l_ <= DebugPrintLevel) {	\
	DbgPrint _x_;				\
	}

#else
#define DebugPrint(_l_, _x_)
#endif

// DbgPrint
#define DEBUG_BUFFER_LENGTH 256

static CHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];

static VOID
DbgPrint(
		 IN PCHAR	DebugMessage,
		 ...
		 )
{
    va_list ap;
	
    va_start(ap, DebugMessage);
	
	_vsnprintf(DebugBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	
	OutputDebugString(DebugBuffer);
    
    va_end(ap);
}

static void PrintErrorCode(LPTSTR strPrefix, DWORD	ErrorCode)
{
	LPTSTR lpMsgBuf;
	
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	OutputDebugString(strPrefix);

	OutputDebugString(lpMsgBuf);
	
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

static HANDLE
OpenBusInterface(
	VOID
	)
{
    HDEVINFO							hardwareDeviceInfo;
    SP_INTERFACE_DEVICE_DATA			deviceInterfaceData;
    HANDLE                              file;
    PSP_INTERFACE_DEVICE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;

    hardwareDeviceInfo = SetupDiGetClassDevs (
							(LPGUID)&GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS,
							NULL, // Define no enumerator (global)
							NULL, // Define no
							(DIGCF_PRESENT | // Only Devices present
							DIGCF_INTERFACEDEVICE) // Function class devices.
							);

    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
		DebugPrint(1, ("[LanscsiLib]OpenBusInterface: SetupDiGetClassDevs failed: 0x%x\n", GetLastError()));
        return NULL;
    }

    deviceInterfaceData.cbSize = sizeof (SP_INTERFACE_DEVICE_DATA);

    if (SetupDiEnumDeviceInterfaces (
			hardwareDeviceInfo,
            0, // No care about specific PDOs
            (LPGUID)&GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS,
            0, //
            &deviceInterfaceData)) 
	{
    } else if (ERROR_NO_MORE_ITEMS == GetLastError()) 
	{
		DebugPrint(1, ("[LanscsiLib]OpenBusInterface: Interface GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS is not registered\n"));
		return NULL;
    }

    SetupDiGetInterfaceDeviceDetail (
            hardwareDeviceInfo,
            &deviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL // not interested in the specific dev-node
			);

    predictedLength = requiredLength;

	deviceInterfaceDetailData = 
		HeapAlloc(GetProcessHeap(), 0, predictedLength);

	deviceInterfaceDetailData->cbSize = 
                    sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);
    
    if (! SetupDiGetInterfaceDeviceDetail (
               hardwareDeviceInfo,
               &deviceInterfaceData,
               deviceInterfaceDetailData,
               predictedLength,
               &requiredLength,
               NULL)) 
	{
		DebugPrint(1, ("[LanscsiLib]OpenBusInterface: SetupDiGetInterfaceDeviceDetail Error.\n"));
		HeapFree(GetProcessHeap(), 0, deviceInterfaceDetailData);
	    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
        return NULL;
    }

	DebugPrint(3, ("[LanscsiLib]OpenBusInterface: Opening %s\n", deviceInterfaceDetailData->DevicePath));

    file = CreateFile (
		deviceInterfaceDetailData->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		0, // FILE_SHARE_READ | FILE_SHARE_WRITE
		NULL, // no SECURITY_ATTRIBUTES structure
		OPEN_EXISTING, // No special create flags
		0, // No special attributes
		NULL); // No template file

    if (INVALID_HANDLE_VALUE == file) {
		DebugPrint(1, ("[LanscsiLib]OpenBusInterface: Device not ready: 0x%x", GetLastError()));
		HeapFree(GetProcessHeap(), 0, deviceInterfaceDetailData);
	    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
        return NULL;
    }
    
	DebugPrint(3, ("[LanscsiLib]OpenBusInterface: Bus interface opened!!! %s\n", deviceInterfaceDetailData->DevicePath));	
	
	// Clean up.
	HeapFree(GetProcessHeap(), 0, deviceInterfaceDetailData);
	SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);

	return file;
}
/*
BOOLEAN
LanscsiPlugin(
			  ULONG		SlotNo,
			  ULONG		MaxRequestBlocks,
			  HANDLE	*phEvent
			  )
{
    HANDLE						file;
	BOOLEAN						result;
	ULONG						bytes;
    ULONG						bytesReturned;
	PBUSENUM_PLUGIN_HARDWARE	lanscsiPluginData;	

	printf("SlotNo. of the device to be enumerated: %d\n", SlotNo);

	DebugPrint(3, ("[LanscsiLib]LanscsiPlugin: SlotNo. of the device to be enumerated: %d\n", SlotNo));

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

    lanscsiPluginData = malloc (bytes = (sizeof (BUSENUM_PLUGIN_HARDWARE) 
											+ BUS_HARDWARE_IDS_LENGTH));

    lanscsiPluginData->Size = sizeof (BUSENUM_PLUGIN_HARDWARE);
	lanscsiPluginData->SlotNo = SlotNo;
	lanscsiPluginData->MaxRequestBlocks = MaxRequestBlocks;
	lanscsiPluginData->phEvent = phEvent;

	DebugPrint(1, ("[LanscsiLib]LanscsiPlugin: phEvent 0x%x, hEvent 0x%x\n", phEvent, *phEvent));

	memcpy(
		lanscsiPluginData->HardwareIDs,
		BUS_HARDWARE_IDS,
        BUS_HARDWARE_IDS_LENGTH
		);

    if (!DeviceIoControl (
			file,
			IOCTL_BUSENUM_PLUGIN_HARDWARE,
            lanscsiPluginData,
			bytes,
            lanscsiPluginData,
			bytes,
            &bytesReturned, 
			NULL)) 
	{
		printf("PlugIn failed:0x%x\n", GetLastError());

		PrintErrorCode("[LanscsiLib]LanscsiPlugin: PlugIn failed ", GetLastError());

		result = FALSE;
    } else
		result = TRUE;

	free(lanscsiPluginData);
    CloseHandle(file);

	return result;
}
*/

// inserted by ILGU for alarm Event
BOOLEAN
LsBusCtlPlugInEx(
	ULONG	SlotNo,
	ULONG	MaxRequestBlocks,
	HANDLE	hEvent,
	HANDLE	hAlarmEvent)
{
    HANDLE						file;
	BOOLEAN						result;
	ULONG						bytes;
    ULONG						bytesReturned;
	PBUSENUM_PLUGIN_HARDWARE_EX	lanscsiPluginData;	

	DebugPrint(3, ("[LanscsiLib]LanscsiPlugin: SlotNo. of the device to be enumerated: %d\n", SlotNo));

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

	bytes = sizeof (BUSENUM_PLUGIN_HARDWARE_EX) + BUS_HARDWARE_IDS_LENGTH;
	lanscsiPluginData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
    lanscsiPluginData->Size = sizeof (BUSENUM_PLUGIN_HARDWARE_EX);
	lanscsiPluginData->SlotNo = SlotNo;
	lanscsiPluginData->MaxRequestBlocks = MaxRequestBlocks;
	lanscsiPluginData->phEvent = &hEvent;
	lanscsiPluginData->phAlarmEvent = &hAlarmEvent;
	
	DebugPrint(1, ("[LanscsiLib]LanscsiPlugin: hEvent 0x%x\n", hEvent));
	DebugPrint(1, ("[LanscsiLib]LanscsiPlugin: hAlarmEvnet 0x%x\n", hAlarmEvent));
	CopyMemory(
		lanscsiPluginData->HardwareIDs,
		BUS_HARDWARE_IDS,
        BUS_HARDWARE_IDS_LENGTH);

    if (!DeviceIoControl (
			file,
			IOCTL_BUSENUM_PLUGIN_HARDWARE_EX,
            lanscsiPluginData,
			bytes,
            lanscsiPluginData,
			bytes,
            &bytesReturned, 
			NULL)) 
	{
		PrintErrorCode("[LanscsiLib]LanscsiPlugin: PlugIn failed ", GetLastError());
		result = FALSE;
    } else {
		DebugPrint(1, ("[LanscsiLib]LanscsiPlugin: Succeeded.\n"));
		result = TRUE;
    }

	HeapFree(GetProcessHeap(), 0, lanscsiPluginData);
    CloseHandle(file);

	return result;
}


BOOLEAN
LsBusCtlEject(
	ULONG SlotNo)
{
    HANDLE					file;
	BOOLEAN					result;
    ULONG					bytesReturned;
    BUSENUM_EJECT_HARDWARE  eject;

	DebugPrint(3, ("[LanscsiLib]LanscsiEject: SlotNo. of the device to be ejected: %d\n", SlotNo));

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

	ZeroMemory(&eject, sizeof(BUSENUM_EJECT_HARDWARE));
	eject.SlotNo = SlotNo;
    eject.Size = sizeof (eject);

    if (!DeviceIoControl (
			file,
			IOCTL_BUSENUM_EJECT_HARDWARE,
            &eject,
			sizeof (eject),
            &eject,
			sizeof (eject),
            &bytesReturned, 
			NULL)) 
	{
		PrintErrorCode("[LanscsiLib]LanscsiEject: LanscsiEject failed ", GetLastError());
		result = FALSE;
    } else
		result = TRUE;

    CloseHandle(file);

	return result;
}


BOOLEAN
LsBusCtlUnplug(
	ULONG SlotNo)
{
    HANDLE					file;
	BOOLEAN					result;
    ULONG					bytesReturned;
    BUSENUM_UNPLUG_HARDWARE unplug;

	DebugPrint(3, ("[LanscsiLib]LanscsiUnplug: SlotNo. of the device to be unpluged: %d\n", SlotNo));

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

	ZeroMemory(&unplug, sizeof(BUSENUM_UNPLUG_HARDWARE));
	unplug.SlotNo = SlotNo;
    unplug.Size = sizeof (unplug);

    if (!DeviceIoControl (
			file,
			IOCTL_BUSENUM_UNPLUG_HARDWARE,
            &unplug,
			sizeof (unplug),
            &unplug,
			sizeof (unplug),
            &bytesReturned, 
			NULL)) 
	{
		PrintErrorCode("[LanscsiLib]LanscsiUnplug: LanscsiUnplug failed ", GetLastError());
		result = FALSE;
    } else
		result = TRUE;

    CloseHandle(file);

	return result;
}

BOOLEAN
LsBusCtlAddTarget(
	PLANSCSI_ADD_TARGET_DATA AddTargetData)
{
    HANDLE					file;
	BOOLEAN					result;
    ULONG					bytesReturned;
	
	DebugPrint(1, ("[LanscsiLib]LanscsiAddTarget: SlotNo. of the device to add target: %d\n", AddTargetData->ulSlotNo));
//	Sleep(1000*5);

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

    if (!DeviceIoControl (
			file,
			IOCTL_LANSCSI_ADD_TARGET,
            AddTargetData,
			AddTargetData->ulSize,
            AddTargetData,
			AddTargetData->ulSize,
            &bytesReturned, 
			NULL)) 
	{
		PrintErrorCode("[LanscsiLib]LanscsiAddTarget: Add Target failed ", GetLastError());
		result = FALSE;
	} else {
		result = TRUE;
	}

    CloseHandle(file);

	return result;
}


BOOLEAN
LsBusCtlRemoveTarget(
	PLANSCSI_REMOVE_TARGET_DATA	RemoveTargetData)
{
    HANDLE					file;
	BOOLEAN					result;
    ULONG					bytesReturned;


	DebugPrint(3, ("SlotNo. of the device to remove target: %d\n", RemoveTargetData->ulSlotNo));

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

    if (!DeviceIoControl (
			file,
			IOCTL_LANSCSI_REMOVE_TARGET,
            RemoveTargetData,
			sizeof (*RemoveTargetData),
            RemoveTargetData,
			sizeof (*RemoveTargetData),
            &bytesReturned, 
			NULL)) 
	{
		DebugPrint(1, ("LanscsiRemoveTarget failed:0x%x\n", GetLastError()));
		result = FALSE;
	} else {
		result = TRUE;
	}

    CloseHandle(file);
	return result;
}

/*
BOOLEAN
LanscsiCopyTarget(
    PLANSCSI_COPY_TARGET_DATA	CopyTargetData
	)
{
    HANDLE					file;
	BOOLEAN					result;
    ULONG					bytesReturned;

	printf("LanscsiCopyTarget\n");

	DebugPrint(3, ("[LanscsiLib]LanscsiCopyTarget: Entered.\n"));

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}

    if (!DeviceIoControl (
			file,
			IOCTL_LANSCSI_COPY_TARGET,
            CopyTargetData,
			sizeof (*CopyTargetData),
            CopyTargetData,
			sizeof (*CopyTargetData),
            &bytesReturned, 
			NULL)) 
	{
		PrintErrorCode("[LanscsiLib]LanscsiCopyTarget: LanscsiCopyTarget failed ", GetLastError());
	
		printf("LanscsiCopyTarget failed:0x%x\n", GetLastError());
		
		result = FALSE;
    } else
		result = TRUE;

    CloseHandle(file);

	return result;
}
*/


BOOLEAN
LsBusCtlQueryAlarmStatus(
	ULONG SlotNo,
	PULONG AlarmStatus)
{
	BOOLEAN						BRet ;
	BUSENUM_QUERY_INFORMATION	Query;
	BUSENUM_INFORMATION	Information;

	//
	//	Query AccessRight to LanscsiBus
	//
	Query.InfoClass = INFORMATION_PDO;
	Query.Size		= sizeof(Query) ;
	Query.SlotNo	= SlotNo;

	BRet = LsBusCtlQueryInformation(
				&Query,
				sizeof(Query),
				&Information,
				sizeof(Information));
	if(BRet == FALSE) {
		return FALSE;
	}

//	if(Information.UnitDisk.StatusFlags & ADAPTERINFO_STATUS_RECONNECTFAILED) {
//		*AlarmStatus = ALARM_STATUS_FAIL_RECONNECT;
//	} else 
	if(ADAPTERINFO_ISSTATUSFLAG(Information.PdoInfo.AdapterStatus, ADAPTERINFO_STATUSFLAG_RECONNECTPENDING)) {
		*AlarmStatus = ALARM_STATUS_START_RECONNECT;
	} else {
		*AlarmStatus = ALARM_STATUS_NORMAL;
	}

	return BRet;
}

BOOLEAN
LsBusCtlQueryDvdStatus(
	ULONG	SlotNo,
	PULONG pDvdStatus)
{
    HANDLE					file;
	BOOLEAN					bResult;
    ULONG					bytesReturned;
	BUSENUM_DVD_STATUS		DVDSTATUS_IN;
	

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}
	
	DVDSTATUS_IN.SlotNo = SlotNo;
	DVDSTATUS_IN.Size = sizeof(BUSENUM_DVD_STATUS);
	
    if (!DeviceIoControl(
			file,
			IOCTL_DVD_GET_STATUS,
			&DVDSTATUS_IN, 
			sizeof(BUSENUM_DVD_STATUS),
			&DVDSTATUS_IN, 
			sizeof(BUSENUM_DVD_STATUS),
			&bytesReturned, 
			NULL)) {

		PrintErrorCode("[LanscsiLib]LanscsiQueryDvdStatus: failed ", GetLastError());
	
		bResult = FALSE;
    } else {
		bResult = TRUE;
		*pDvdStatus = DVDSTATUS_IN.Status;
	}
		
    CloseHandle(file);
	
	return bResult;
}


BOOLEAN
LsBusCtlQueryNodeAlive(
	ULONG	SlotNo,
	PBOOL	pbAdapterHasError)
{
    HANDLE					file;
	BOOLEAN					bResult;
    ULONG					bytesReturned;
	BUSENUM_NODE_ALIVE_IN	nodeAliveIn;
	BUSENUM_NODE_ALIVE_OUT	nodeAliveOut;

	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}
	
	nodeAliveIn.SlotNo = SlotNo;

    if (!DeviceIoControl(
			file,
			IOCTL_BUSENUM_QUERY_NODE_ALIVE,
			&nodeAliveIn, 
			sizeof(BUSENUM_NODE_ALIVE_IN),
			&nodeAliveOut, 
			sizeof(BUSENUM_NODE_ALIVE_OUT),
			&bytesReturned, 
			NULL)) {

		PrintErrorCode("[LanscsiLib]LanscsiQueryNodeAlive: failed ", GetLastError());
	
		bResult = FALSE;
    } else {
		bResult = nodeAliveOut.bAlive;
		*pbAdapterHasError = nodeAliveOut.bHasError;
	}
		
    CloseHandle(file);
	
	return bResult;
}



BOOLEAN
LsBusCtlQueryInformation(
	PBUSENUM_QUERY_INFORMATION	Query,
	ULONG						QueryLength,
	PBUSENUM_INFORMATION		Information,
	ULONG						InformationLength)
{
    HANDLE					file;
	BOOLEAN					bResult;
    ULONG					bytesReturned;


	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}
	
    if (!DeviceIoControl(
			file,
			IOCTL_BUSENUM_QUERY_INFORMATION,
			Query,
			QueryLength,
			Information, 
			InformationLength,
			&bytesReturned, 
			NULL)) {

//		PrintErrorCode("[LanscsiLib]LanscsiQueryInformation: failed ", GetLastError());
	
		bResult = FALSE;
    } else {
//		PrintErrorCode("[LanscsiLib]LanscsiQueryInformation: succeeded.", GetLastError());

		bResult = TRUE;
	}

    CloseHandle(file);

	return bResult;
}


BOOLEAN
LsBusCtlQueryLsmpInformation(
	PLSMPIOCTL_QUERYINFO	Query,
	ULONG					QueryLength,
	PVOID					Information,
	ULONG					InformationLength)
{
    HANDLE					file;
	BOOLEAN					bResult;
    ULONG					bytesReturned;


	file = OpenBusInterface();
	if(file == NULL) {
		return FALSE;
	}
	
    if (!DeviceIoControl(
			file,
			IOCTL_LANSCSI_QUERY_LSMPINFORMATION,
			Query,
			QueryLength,
			Information, 
			InformationLength,
			&bytesReturned, 
			NULL)) {

//		PrintErrorCode("[LanscsiLib]LanscsiQueryLsmpInformation: failed ", GetLastError());
	
		bResult = FALSE;
    } else {
//		PrintErrorCode("[LanscsiLib]LanscsiQueryLsmpInformation: succeeded.", GetLastError());

		bResult = TRUE;
	}

    CloseHandle(file);

	return bResult;
}

#if 0
//////////////////////////////////////////////////////////////////////////////
//	@hootch@
//////////////////////////////////////////////////////////////////////////////

//
// register a window as device-notification listener
//
BOOL LSLib_RegisterDevNotification(SERVICE_STATUS_HANDLE hSS, HDEVNOTIFY *hSCSIIfNtf, HDEVNOTIFY *hVolNtf) {
	DEV_BROADCAST_DEVICEINTERFACE	SCSIAdapterNtfFilter;
	DEV_BROADCAST_DEVICEINTERFACE	VolNtfFilter;

	//
	//	register SCSI adapter interface event listener
	//
    ZeroMemory(&SCSIAdapterNtfFilter, sizeof(SCSIAdapterNtfFilter) );
    SCSIAdapterNtfFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    SCSIAdapterNtfFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	SCSIAdapterNtfFilter.dbcc_classguid = StoragePortClassGuid ;

	*hSCSIIfNtf = RegisterDeviceNotification(
		hSS,
		&SCSIAdapterNtfFilter,
		DEVICE_NOTIFY_SERVICE_HANDLE
		);
    if(*hSCSIIfNtf == NULL) {
        DebugPrint(1, (TEXT("[LanscsiBus] RegisterDeviceNotification SCSIAdapter Interface failed\n")));
		return FALSE ;
    }

	//
	// register Volume event listener
	//
    ZeroMemory(&VolNtfFilter, sizeof(VolNtfFilter) );
    VolNtfFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    VolNtfFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE ;
    VolNtfFilter.dbcc_classguid = VolumeClassGuid ;

	*hVolNtf = RegisterDeviceNotification(
		hSS,
		&VolNtfFilter,
		DEVICE_NOTIFY_SERVICE_HANDLE
		);
    if(*hVolNtf == NULL) {
        DebugPrint(1, (TEXT("[LanscsiBus] RegisterDeviceNotification Logical Volume failed ErrCode:%d\n"), GetLastError()));
		return FALSE ;
    }

	return TRUE ;
}

//
// unregister a service as device-notification listener
//
void LSLib_UnregisterDevNotification(HDEVNOTIFY hSCSIIfNtf, HDEVNOTIFY hVolNtf) {
	if(UnregisterDeviceNotification(hSCSIIfNtf) == FALSE) {
		PrintErrorCode("[LDServ] Unregister SCSI Adapter Interface Notification fail... ", GetLastError());
	}
	if(UnregisterDeviceNotification(hVolNtf) == FALSE) {
		PrintErrorCode("[LDServ] Unregister Logical Volume Notification fail... ", GetLastError());
	}
}

#endif
