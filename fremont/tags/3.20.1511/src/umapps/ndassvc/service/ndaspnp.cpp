#include "stdafx.h"
#include <winioctl.h>
#include <setupapi.h>
#include <ntddscsi.h>
#include <rofiltctl.h>
#define NDASVOL_SLIB
#include <ndas/ndastypeex.h>
#include <ndas/ndasvol.h>
#include <ndas/ndasvolex.h>
#include <ndas/ndassvcparam.h>

// #include "ntddstor.h" // for StoragePortClassGuid, VolumeClassGuid
#include "ndaspnp.h"
#include "ndasdev.h"
#include "ndascfg.h"
#include "ndasobjs.h"
#include "ndaseventpub.h"
#include "xguid.h"
#include "sysutil.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaspnp.tmh"
#endif

namespace
{

bool 
IsWindows2000();

LPCSTR 
DeviceNotifyDataTypeStringA(
	DeviceNotifyData::DataType Type);

void
pSetDiskHotplugInfoByPolicy(
	HANDLE hDisk);


struct IsLogicalDeviceMounted : std::unary_function<CNdasLogicalDevicePtr,bool> {
	bool operator()(const CNdasLogicalDevicePtr& pLogDevice) const {
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status ||
			NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == status)
		{
			return true;
		}
		return false;
	}
};

struct EffectiveNdasAccess : std::unary_function<NDAS_LOCATION,void> {
	EffectiveNdasAccess() : 
		AllowedAccess(GENERIC_READ | GENERIC_WRITE) {
	}
	void operator()(const NDAS_LOCATION& location) {

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Volume on ndasLocation=%d\n", location);

		CNdasLogicalDevicePtr pLogDevice = pGetNdasLogicalDeviceByNdasLocation(location);
		if (CNdasLogicalDeviceNullPtr == pLogDevice)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, "Logical device is not available. Already unmounted?\n");
			return;
		}

		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, "Logical device is mounted or mount pending. Already unmounted?\n");
			return;
		}
		AllowedAccess &= pLogDevice->GetMountedAccess();
	}
	ACCESS_MASK AllowedAccess;
};

} // end of namespace

//////////////////////////////////////////////////////////////////////////

CNdasServiceDeviceEventHandler::
CNdasServiceDeviceEventHandler(
	CNdasService& service) :
	m_service(service),
	m_bInitialized(FALSE),
	m_bROFilterFilteringStarted(FALSE),
	m_hROFilter(INVALID_HANDLE_VALUE),
	m_bNoLfs(NdasServiceConfig::Get(nscDontUseWriteShare)),
	m_hRecipient(NULL),
	m_dwReceptionFlags(0),
	m_hStoragePortNotify(NULL),
	m_hVolumeNotify(NULL),
	m_hDiskNotify(NULL),
	m_hCdRomClassNotify(NULL)
{
}

CNdasServiceDeviceEventHandler::
~CNdasServiceDeviceEventHandler()
{
	(void) Uninitialize();
}

bool
CNdasServiceDeviceEventHandler::
Initialize(HANDLE hRecipient, DWORD dwReceptionFlag)
{
	m_hRecipient = hRecipient;
	m_dwReceptionFlags = dwReceptionFlag;
	//
	// Caution!
	//
	// DEVICE_NOFITY_WINDOW_HANDLE is 0x000, hence
	// XTLASSERT(DEVICE_NOFITY_WINDOW_HANDLE & m_dwReceptionFlags)
	// will always fail.
	//
	XTLASSERT(
		((DEVICE_NOTIFY_SERVICE_HANDLE & m_dwReceptionFlags) == DEVICE_NOTIFY_SERVICE_HANDLE) ||
		((DEVICE_NOTIFY_WINDOW_HANDLE & m_dwReceptionFlags) == DEVICE_NOTIFY_WINDOW_HANDLE));

	//
	// Do not initialize twice if successful.
	//
	XTLASSERT(!m_bInitialized);

	//
	// register Storage Port, Volume and Disk device notification
	//

	m_hStoragePortNotify = RegisterDeviceInterfaceNotification(&GUID_DEVINTERFACE_STORAGEPORT);
	if (m_hStoragePortNotify.IsInvalid()) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Registering Storage Port Device Notification failed, error=0x%X\n", GetLastError());
	}

	m_hVolumeNotify = RegisterDeviceInterfaceNotification(&GUID_DEVINTERFACE_VOLUME);
	if (m_hVolumeNotify.IsInvalid()) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Registering Volume Device Notification failed, error=0x%X\n", GetLastError());
	}

	m_hDiskNotify = RegisterDeviceInterfaceNotification(&GUID_DEVINTERFACE_DISK);

	if (m_hDiskNotify.IsInvalid()) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Registering Disk Device Notification failed, error=0x%X\n", GetLastError());
	}

	m_hCdRomClassNotify = RegisterDeviceInterfaceNotification(&GUID_DEVINTERFACE_CDROM);

	if (m_hCdRomClassNotify.IsInvalid()) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Registering CDROM Device Notification failed, error=0x%X\n", GetLastError());
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Storage Port Notify Handle: %p\n", m_hStoragePortNotify);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Volume Notify Handle      : %p\n", m_hVolumeNotify);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Disk Device Notify Handle : %p\n", m_hDiskNotify);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "CDROM Device Notify Handle : %p\n", m_hCdRomClassNotify);

	//
	// nothing more for Windows XP or later
	//

	//
	// If NoLfs is set and if the OS is Windows 2000, 
	// load ROFilter service
	//
	if (!(IsWindows2000() && m_bNoLfs)) 
	{
		m_bInitialized = TRUE;
		return TRUE;
	}

	XTLASSERT(m_hROFilter.IsInvalid());

	//
	// Even if the rofilter is not loaded
	// initialization returns TRUE
	// However, m_hROFilter has INVALID_HANDLE_VALUE
	//

	m_bInitialized = TRUE;

	SERVICE_STATUS serviceStatus;
	BOOL fSuccess = NdasRoFilterQueryServiceStatus(&serviceStatus);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"NdasRoFilterQueryServiceStatus failed, error=0x%X\n", GetLastError());
		return TRUE;
	}

	if (SERVICE_RUNNING != serviceStatus.dwCurrentState) 
	{
		fSuccess = NdasRoFilterStartService();
		if (!fSuccess) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
				"NdasRoFilterStartService failed, error=0x%X\n", GetLastError());
			return TRUE;
		}
	}

	HANDLE hROFilter = NdasRoFilterOpenDevice();
	if (INVALID_HANDLE_VALUE == hROFilter) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"NdasRoFilterCreate failed, error=0x%X\n", GetLastError());
		return TRUE;
	}

	m_hROFilter = hROFilter;

	return TRUE;
}

void
CNdasServiceDeviceEventHandler::OnShutdown()
{
	m_hStoragePortNotify.Release(); 
	m_hDiskNotify.Release();
	m_hDiskNotify.Release();
	m_hCdRomClassNotify.Release();
	m_hVolumeNotify.Release();
}

bool 
CNdasServiceDeviceEventHandler::Uninitialize()
{
	bool ret = true;
	
	//if (m_bROFilterFilteringStarted) 
	//{
	//	BOOL fSuccess = NdasRoFilterStopFilter(m_hROFilter);
	//	if (fSuccess) 
	//	{
	//		m_bROFilterFilteringStarted = FALSE;
	//	}
	//	else
	//	{
	//		XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_WARNING, "Failed to stop ROFilter session.\n");
	//		ret = false;
	//	}
	//}

	m_hStoragePortNotify.Release(); 
	m_hDiskNotify.Release();
	m_hDiskNotify.Release();
	m_hCdRomClassNotify.Release();
	m_hVolumeNotify.Release();

	return ret;
}

HDEVNOTIFY
CNdasServiceDeviceEventHandler::
RegisterDeviceInterfaceNotification(
	LPCGUID classGuid)
{
	DEV_BROADCAST_DEVICEINTERFACE dbtdi;
	::ZeroMemory(&dbtdi, sizeof(DEV_BROADCAST_DEVICEINTERFACE));
	dbtdi.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	dbtdi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	dbtdi.dbcc_classguid = *classGuid;

	return ::RegisterDeviceNotification(
		m_hRecipient, &dbtdi, m_dwReceptionFlags);
}

HDEVNOTIFY 
CNdasServiceDeviceEventHandler::
RegisterDeviceHandleNotification(
	HANDLE hDeviceFile)
{
	DEV_BROADCAST_HANDLE dbth;
	::ZeroMemory(&dbth, sizeof(DEV_BROADCAST_HANDLE));
	dbth.dbch_size = sizeof(DEV_BROADCAST_HANDLE ) ;
	dbth.dbch_devicetype = DBT_DEVTYP_HANDLE  ;
	dbth.dbch_handle = hDeviceFile ;

	return ::RegisterDeviceNotification(
		m_hRecipient, &dbth, m_dwReceptionFlags);
}


LRESULT 
CNdasServiceDeviceEventHandler::
OnStoragePortDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "%ls\n", pdbhdr->dbcc_name);

	HRESULT hr;
	BOOL fSuccess = FALSE;
	DWORD dwSlotNo = 0;

	const DWORD MaxWait = 10;
	DWORD retry = 0;
	do 
	{
		// Intentional wait for NDASSCSI miniport
		// Miniport will be available a little bit later than
		// the scsiport.
		//
		::Sleep(200);

		hr = pGetNdasSlotNumberFromDeviceNameW(
			pdbhdr->dbcc_name, &dwSlotNo);

		if (FAILED(hr) && HRESULT_FROM_WIN32(ERROR_DEV_NOT_EXIST) == hr)
		{
			// Wait for the instance of actual NDASSCSI miniport 
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"NDASSCSI miniport is not available yet. Wait (%d/%d)\n",
				retry + 1, MaxWait);
		} 
		else if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
				"pGetNdasSlotNumberForScsiPortDeviceName(%ls) failed, hr=0x%X\n",  
				(LPCWSTR) pdbhdr->dbcc_name, hr);
		}

		++retry;

	} while (FAILED(hr) && HRESULT_FROM_WIN32(ERROR_DEV_NOT_EXIST) == hr && retry < MaxWait);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"pGetNdasSlotNumberForScsiPortDeviceName(%ls) failed, hr=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, hr);
		return TRUE;
	}

	XTL::AutoFileHandle hStoragePort = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	if (hStoragePort.IsInvalid()) 
	{
		//
		// TODO: Set Logical Device Status to 
		// LDS_UNMOUNT_PENDING -> LDS_NOT_INITIALIZED (UNMOUNTED)
		// and Set LDS_ERROR? However, which one?
		//
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());
		return TRUE;
	}

//#if 0
//	//
//	// Availability of this resource is subject to change due to unknown reason?
//	// NdasDmGetNdasLogDevSlotNoOfScsiPort may not succeed.
//	//
//	DWORD dwSlotNo2;
//	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(hStoragePort, &dwSlotNo2);
//	if (!fSuccess) 
//	{
//		XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_WARNING, "NdasDmGetNdasLogDevSlotNoOfDisk(%p) failed.\n", hStoragePort);
//	//	XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_WARNING, "May not be NDAS logical device. Ignoring.\n");
//	//	return TRUE;
//	}
//	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "NdasDmGetNdasLogDevSlotNoOfScsiPort returned %d.\n", dwSlotNo2);
//#endif

	//
	// We got the LANSCSI miniport
	//

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "NDAS Logical Device Slot No: %d.\n", dwSlotNo);

	//
	//	Register device handler notification to get remove-query and remove-failure.
	//  Windows system doesn't send a notification of remove-query and remove-failure
	//    of a device interface.
	//

	//
	// remove duplicate if found
	//

	//LogDevSlot_DevNotify_Map::const_iterator itr =
	//	m_StoragePortNotifyMap.find(dwSlotNo);
	//if (m_StoragePortNotifyMap.end() != itr) {
	//	XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
	//		"NDAS logical device storage port notify already registered "
	//		_T("at slot %d. Deleting old ones: "), dwSlotNo);
	//	::UnregisterDeviceNotification(itr->second);
	//}

	//
	// register new one
	//

	bool result = AddDeviceNotificationHandle(
		hStoragePort,
		DeviceNotifyData(DeviceNotifyData::DNTStoragePort, dwSlotNo));

	if (!result)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Registering StoragePort device handle %p for notification failed, error=0x%X\n",
			hStoragePort, GetLastError());
	}
	else
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"StoragePort Device handle %p is registered for notification.\n", 
			hStoragePort);
	}


	return TRUE;
}

bool 
CNdasServiceDeviceEventHandler::
AddDeviceNotificationHandle(
	HANDLE hDevice, 
	const DeviceNotifyData& Data)
{
	//
	// Register device handle first
	//
	HDEVNOTIFY hDevNotify = RegisterDeviceHandleNotification(hDevice);
	if (NULL == hDevNotify) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Registering device handle %p failed, error=0x%X\n",
			hDevice, GetLastError());
		return false;
	}

	//
	// Then, add the entry to the device notification map
	// to reference on its removal (OnRemoveComplete)
	//

	m_DevNotifyMap.insert(std::make_pair(hDevNotify, Data));

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"Device handle %p Notification Registered successfully.\n", hDevice);

	return true;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnStoragePortDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	//
	// Removal of Device Interface does not mean the removal 
	// of the device. We need to wait until the actual device is being removed 
	// by holding it off until DeviceHandleOnRemoveComplete is called.
	//

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnStoragePortDeviceInterfaceRemoveComplete:%ls\n", pdbhdr->dbcc_name);

	//
	// Existing codes have been cleared
	//

	return TRUE;
}

namespace
{
	typedef std::vector<NDAS_LOCATION> NdasLocationVector;
	HRESULT CALLBACK 
	OnNdasVolume(
		NDAS_LOCATION NdasLocation,
		LPVOID Context)
	{
		NdasLocationVector* v = 
			static_cast<NdasLocationVector*>(Context);
		try
		{
			v->push_back(NdasLocation);
		}
		catch (...)
		{
			return E_OUTOFMEMORY;
		}
		return S_OK;
	}
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	// Return TRUE to grant the request.
	// Return BROADCAST_QUERY_DENY to deny the request.


	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnVolumeDeviceInterfaceArrival:%ls\n", pdbhdr->dbcc_name);

	//
	// Nothing to do for other than Windows 2000 or ROFILTER is not used
	//

//	if (!IsWindows2000()) {
//		return TRUE;
//	}

	//
	// If the volume contains any parts of the NDAS logical device,
	// it may be access control should be enforced to the entire volume
	// by ROFilter if applicable - e.g. Windows 2000
	//

	//
	// BUG:
	// At this time, we cannot apply ROFilter to a volume
	// without a drive letter due to the restrictions of ROFilter
	// This should be fixed!
	//

	XTL::AutoFileHandle hVolume = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_ALWAYS,
		0,
		NULL);

	if (hVolume.IsInvalid()) 
	{
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());

		// TODO: Return BROADCAST_QUERY_DENY?
		return TRUE;
	}

	NdasLocationVector ndasLocations;
	ndasLocations.reserve(1);
	HRESULT hr = NdasEnumNdasLocationsForVolume(
		hVolume, 
		OnNdasVolume, 
		&ndasLocations);

	if (FAILED(hr)) 
	{    
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"NdasEnumNdasLocationsForVolume(hVolume %p) failed, error=0x%X\n", 
			hVolume, GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, "May not span a NDAS logical device!!!\n");
		
		//
		// TODO:
		//
		// Should exactly process here
		//
		// BUG: 
		// At this time, we cannot distinguish if 
		// the NdasDmGetNdasLogDevSlotNoOfVolume() is failed because it is 
		// not a NDAS logical device or because of the actual system error.
		//
		// We are assuming that it is not a NDAS logical device.
		//

		// TODO: Return BROADCAST_QUERY_DENY?
		return TRUE;
	}

// #define EXP_VOLUME_NDAS_ICON
//#ifdef EXP_VOLUME_NDAS_ICON
//	if (!IsWindows2000()) 
//	{
//
//		HDEVINFO hDevInfoSet = ::SetupDiCreateDeviceInfoList(NULL, NULL);
//		if (INVALID_HANDLE_VALUE == hDevInfoSet) 
//		{
//			XTLASSERT(FALSE);
//		}
//
//		SP_DEVICE_INTERFACE_DATA devIntfData = {0};
//		devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
//
//		fSuccess = ::SetupDiOpenDeviceInterface(
//			hDevInfoSet, 
//			pdbhdr->dbcc_name,
//			0,
//			&devIntfData);
//
//		XTLASSERT(fSuccess);
//
//		HKEY hDevKey = ::SetupDiOpenDeviceInterfaceRegKey(
//			hDevInfoSet,
//			&devIntfData,
//			0,
//			KEY_READ | KEY_WRITE);
//
//		DWORD dwError = ::GetLastError();
//
//		if (INVALID_HANDLE_VALUE == hDevKey && 
//			ERROR_FILE_NOT_FOUND == ::GetLastError()) 
//		{
//			hDevKey = ::SetupDiCreateDeviceInterfaceRegKey(
//				hDevInfoSet,
//				&devIntfData,
//				0,
//				KEY_READ | KEY_WRITE,
//				NULL,
//				NULL);
//			XTLASSERT(INVALID_HANDLE_VALUE != hDevKey);
//		} else if (INVALID_HANDLE_VALUE == hDevKey) 
//		{
//			XTLASSERT(FALSE);
//		}
//
//		HKEY hDevParamKey = (HKEY)INVALID_HANDLE_VALUE;
//		DWORD dwKeyDisp = 0;
//
//		LONG lRes = ::RegCreateKeyEx(
//			hDevKey,
//			_T("Device Parameters"),
//			0,
//			NULL,
//			REG_OPTION_NON_VOLATILE,
//			KEY_READ | KEY_WRITE,
//			NULL,
//			&hDevParamKey,
//			&dwKeyDisp);
//
//		XTLASSERT(ERROR_SUCCESS == lRes);
//
//		LPCTSTR szDeviceGroup = _T("NDASDiskVolume");
//
//		lRes = ::RegSetValueEx(
//			hDevParamKey,
//			_T("DeviceGroup"),
//			0,
//			REG_MULTI_SZ,
//			(CONST BYTE*) szDeviceGroup,
//			sizeof(szDeviceGroup));
//
//		XTLASSERT(ERROR_SUCCESS == lRes);
//
//		fSuccess = ::SetupDiDestroyDeviceInfoList(hDevInfoSet);
//		XTLASSERT(fSuccess);
//
//		lRes = ::RegCloseKey(hDevParamKey);
//		XTLASSERT(ERROR_SUCCESS == lRes);
//
//		lRes = ::RegCloseKey(hDevKey);
//		XTLASSERT(ERROR_SUCCESS == lRes);
//	}
//
//#endif

	//
	// For each NDAS Logical Devices, we should check granted-access
	//

	//
	// minimum access
	//

	EffectiveNdasAccess getEffectiveAccess;

	std::for_each(
		ndasLocations.begin(),
		ndasLocations.end(),
		getEffectiveAccess);

	ACCESS_MASK effectiveAccess = getEffectiveAccess.AllowedAccess;

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Volume has granted as 0x%08X (%s %s).\n", 
		effectiveAccess,
		(effectiveAccess & GENERIC_READ) ? "GENERIC_READ" : "",
		(effectiveAccess & GENERIC_WRITE) ?"GENERIC_WRITE" : "");

	
	//
	// Volume handle is no more required
	//
	hVolume.Release();

	if ((effectiveAccess & GENERIC_WRITE)) 
	{
		//
		// nothing to do for write-access
		//
	}
	else 
	{
//		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Loading ROFilter.\n");
//
//		//
//		// filter to the logical drives (DOS devices)
//		// it's sufficient to filter the first drive letter
//		// to filter other mount-point'ed locations
//		// ROFilter does it all!
//		//
//
//		DWORD dwDriveLetterNumber;
//		fSuccess = NdasDmGetDriveNumberOfVolume(hVolume, &dwDriveLetterNumber);
//		if (!fSuccess) 
//		{
//			XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
//				"NdasDmGetDriveNumberOfVolume(hVolume %p) failed.\n", 
//				hVolume);
//
//			// TODO: Return BROADCAST_QUERY_DENY 
//			return TRUE;
//		}
//
//		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
//			"NdasDmGetDriveNumberOfVolume returns (%d).\n", dwDriveLetterNumber);
//
//#define NO_ROFILTER
//#ifndef NO_ROFILTER
//		//
//		// Defer starting filtering until we need it!
//		//
//		if (!m_bROFilterFilteringStarted) 
//		{
//			fSuccess = NdasRoFilterStartFilter(m_hROFilter);
//			if (!fSuccess) 
//			{
//				XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_ERROR, "NdasRoFilterStartFilter failed.\n");
//				// TODO: Return BROADCAST_QUERY_DENY?
//				return TRUE;
//			}
//			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "RoFilter filtering started a session.\n");
//		}
//
//		fSuccess = NdasRoFilterEnableFilter(m_hROFilter, dwDriveLetterNumber, TRUE);
//		if (!fSuccess) 
//		{
//			XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
//				"Enable filter on drive %c failed.\n", (dwDriveLetterNumber + 'A'));
//			// TODO: Return BROADCAST_QUERY_DENY?
//			return TRUE;
//		}
//
//		//
//		// Save in the filtered drive letter set
//		//
//		m_FilteredDriveNumbers.insert(dwDriveLetterNumber);
//
//		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
//			"RoFilter filtering started on drive %c.\n", (dwDriveLetterNumber + 'A'));
//
//#endif // NO_ROFILTER

	}


	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Remove complete %ls\n", pdbhdr->dbcc_name);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Disk Device Interface Handlers
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDiskDeviceInterfaceArrival: %ls\n", pdbhdr->dbcc_name);

	XTL::AutoFileHandle hDisk = ::CreateFile(
		reinterpret_cast<LPCTSTR>(pdbhdr->dbcc_name),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hDisk.IsInvalid()) 
	{
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());
		return TRUE;
	}

	NDAS_LOCATION ndasLocation;
	HRESULT hr = pGetNdasSlotNumberFromStorageDeviceHandle(hDisk, &ndasLocation);
	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"pGetNdasSlotNumberFromStorageDeviceHandle failed, handle=%p, hr=0x%X\n", 
			hDisk, hr);
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"May not be NDAS logical device. Ignoring.\n");
		return TRUE;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
		"NDAS Logical Device, ndasLocation=%d.\n", 
		ndasLocation);

	//
	// we are to set the logical device status 
	// from MOUNT_PENDING -\> MOUNTED
	//

	//////////////////////////////////////////////////////////////////////
	// minimum access
	//////////////////////////////////////////////////////////////////////

	CNdasLogicalDevicePtr pLogDevice = pGetNdasLogicalDeviceByNdasLocation(ndasLocation);
	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	{
		// unknown NDAS logical device
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"No Logical device found in LDM, ndasLocation=%d\n", 
			ndasLocation);
		return TRUE;
	}

	pLogDevice->OnMounted();


// TODO: by configuration!
#ifdef NDAS_FEATURE_DISABLE_WRITE_CACHE
	fSuccess = sysutil::DisableDiskWriteCache(hDisk);
	if (!fSuccess) 
	{
		XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_WARNING, "DisableDiskWriteCache failed.\n");
	}
	else 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Disk Write Cache disabled successfully.\n");
	}
#endif

	//////////////////////////////////////////////////////////////////////
	// Set HotplugInfo
	//////////////////////////////////////////////////////////////////////

#ifdef NDAS_FEATURE_USE_HOTPLUG_POLICY
	pSetDiskHotplugInfoByPolicy(hDisk);
#endif

	//////////////////////////////////////////////////////////////////////
	// Registers Device Notification
	//////////////////////////////////////////////////////////////////////

	bool result = AddDeviceNotificationHandle(
		hDisk, 
		DeviceNotifyData(DeviceNotifyData::DNTDisk, ndasLocation));

	if (!result)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Registering disk device handle %p for notification failed, error=0x%X\n", 
			hDisk, GetLastError());
	}
	else
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"Disk device handle %p is registered for notification.\n", hDisk);
	}

	//////////////////////////////////////////////////////////////////////
	
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDiskDeviceInterfaceRemoveComplete: %ls\n", pdbhdr->dbcc_name);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// CdRom Device Interface Handlers
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CNdasServiceDeviceEventHandler::
OnCdRomDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnCdRomDeviceInterfaceArrival: %ls\n", pdbhdr->dbcc_name);

	XTL::AutoFileHandle hDevice = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hDevice.IsInvalid()) 
	{
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());

		return TRUE;
	}

	// Disks and CDROMs can be treated as same for querying scsi address 
	NDAS_LOCATION ndasLocation;
	HRESULT hr = pGetNdasSlotNumberFromStorageDeviceHandle(hDevice, &ndasLocation);
	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"pGetNdasLocationForCDROM failed, handle=%p, hr=0x%X\n", 
			hDevice, hr);
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"May not be NDAS logical device. Ignoring.\n");
		return TRUE;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
		"CDROM Device Interface Arrival, ndasLocation=%d.\n", 
		ndasLocation);

	//
	// we are to set the logical device status 
	// from MOUNT_PENDING -> MOUNTED
	//

	CNdasLogicalDevicePtr pLogDevice = pGetNdasLogicalDeviceByNdasLocation(ndasLocation);
	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	{
		// unknown NDAS logical device
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"No Logical device found in LDM, ndasLocation=%d\n", 
			ndasLocation);
		return TRUE;
	}

	pLogDevice->OnMounted();


	//
	// Device Notification
	//

	bool result = AddDeviceNotificationHandle(
		hDevice, 
		DeviceNotifyData(DeviceNotifyData::DNTCdRom, ndasLocation));

	if (!result)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Registering CDROM device handle %p for notification failed, error=0x%X\n",
			hDevice, GetLastError());
	}
	else
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"CDROM Device handle %p is registered for notification.\n", hDevice);
	}

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnCdRomDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnCdRomDeviceInterfaceRemoveComplete: %ls\n", pdbhdr->dbcc_name);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Device Handle Handlers
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CNdasServiceDeviceEventHandler::
OnDeviceHandleQueryRemove(
	PDEV_BROADCAST_HANDLE pdbch)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleQueryRemove: Device Handle %p, Notify Handle %p\n", 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDeviceHandleQueryRemoveFailed(
	PDEV_BROADCAST_HANDLE pdbch)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleQueryRemoveFailed: Device Handle %p, Notify Handle %p\n", 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);

	DeviceNotifyData notifyData;
	if (!FindDeviceHandle(pdbch->dbch_hdevnotify, notifyData))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Notify Handle is not registered.\n");
		return TRUE;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"DeviceHandleQueryRemoveFailed, ndasLocation=%d, Type=%s(0x%X)\n", 
		notifyData.NdasLocation, 
		DeviceNotifyDataTypeStringA(notifyData.Type),
		notifyData.Type);

	//
	// DEVNOTIFYINFO_TYPE_STORAGEPORT is the only notification we are 
	// interested in. However, when a CDROM device attached to the storage port,
	// the storage port device notification for QueryRemoveFailed is not
	// sent to us. So, we also need to handle CD-ROM.
	// 
	// Do not worry about redundant calls to pLogDevice->OnUnmountFailed().
	// It only cares if NDAS_LOGICALDEVICE_STATUS is UNMOUNT_PENDING,
	// which means backup calls will be ignored.
	//
	switch (notifyData.Type)
	{
	case DeviceNotifyData::DNTStoragePort:
	case DeviceNotifyData::DNTCdRom:
	case DeviceNotifyData::DNTDisk:
		{
			CNdasLogicalDevicePtr pLogDevice = 
				pGetNdasLogicalDeviceByNdasLocation(notifyData.NdasLocation);
			if (CNdasLogicalDeviceNullPtr == pLogDevice) 
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
					"Logical Device not found, ndasLocation=%d.\n", 
					notifyData.NdasLocation);
				return TRUE;
			}
			pLogDevice->OnUnmountFailed();
		}
		break;
	default:
		break;
	}
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDeviceHandleRemovePending(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// RemoveComplete will handle the completion of the removal
	//
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleRemovePending: Device Handle %p, Notify Handle %p\n", 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);

	//DEVNOTIFYINFO dni;
	//if (!FindDeviceHandle(dni, pdbch->dbch_hdevnotify))
	//{
	//	return TRUE;
	//}

	//switch (dni.Type)
	//{
	//case DEVNOTIFYINFO_TYPE_STORAGEPORT:
	//	{
	//		//
	//		// Set the logical device as unmount pending
	//		//
	//		NDAS_LOCATION location = {dni.LogDevSlotNo, 0, 0};
	//		CNdasLogicalDevicePtr pLogDevice = pGetNdasLogicalDeviceByNdasLocation(location);
	//		if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	//		{
	//			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, "Logical Device not found at slot %d.\n", dni.LogDevSlotNo);
	//		}
	//		else
	//		{
	//			pLogDevice->OnUnmountPending();
	//		}
	//	}
	//	break;
	//case DEVNOTIFYINFO_TYPE_CDROM:
	//case DEVNOTIFYINFO_TYPE_DISK:
	//default:
	//	// We do not care about other types at this time.
	//	break;
	//}

	return TRUE;
}


LRESULT 
CNdasServiceDeviceEventHandler::
OnDeviceHandleRemoveComplete(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// Device Handle Remove Complete is called on Surprise Removal
	//

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleRemoveComplete: Device Handle %p, Notify Handle %p\n",
		pdbch->dbch_handle, pdbch->dbch_hdevnotify);

	DeviceNotifyData notifyData;
	if (!FindDeviceHandle(pdbch->dbch_hdevnotify, notifyData))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "Notify Handle is not registered.\n");
		return TRUE;
	}

	// Erase from the map
	XTLVERIFY(1 == m_DevNotifyMap.erase(pdbch->dbch_hdevnotify));

	BOOL fSuccess = ::UnregisterDeviceNotification(pdbch->dbch_hdevnotify);
	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Unregistering a device notification to %p failed.\n",
			pdbch->dbch_hdevnotify);
	}

	switch (notifyData.Type)
	{
	case DeviceNotifyData::DNTStoragePort:
		{
			//
			// Remove removal is completed for NDAS SCSI Controller
			//
			CNdasLogicalDevicePtr pLogDevice = 
				pGetNdasLogicalDeviceByNdasLocation(notifyData.NdasLocation);
			if (CNdasLogicalDeviceNullPtr == pLogDevice) 
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
					"Logical Device not found, ndasLocation=%d.\n", 
					notifyData.NdasLocation);
			}
			else
			{
				pLogDevice->OnUnmounted();
			}
		}
		break;
	case DeviceNotifyData::DNTDisk:
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
				"Disk Remove Complete, ndasLocation=%d\n", 
				notifyData.NdasLocation);
		}
		break;
	case DeviceNotifyData::DNTCdRom:
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
				"CDROM Remove Complete, ndasLocation=%d\n", 
				notifyData.NdasLocation);
		}
		break;
	case DeviceNotifyData::DNTVolume:
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
				"Volume Remove Complete, ndasLocation=%d\n", 
				notifyData.NdasLocation);
		}
		break;
	default:
		// We do not care about other types at this time.
		break;
	}

	return TRUE;
}

void
CNdasServiceDeviceEventHandler::
OnDeviceArrival(
	PDEV_BROADCAST_HDR pdbhdr)
{
	XTLASSERT(m_bInitialized);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceArrival: Type %x\n", pdbhdr->dbch_devicetype);

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) 
	{
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);
		
		if (::IsEqualGUID(GUID_DEVINTERFACE_STORAGEPORT, pdbcc->dbcc_classguid))
		{
			OnStoragePortDeviceInterfaceArrival(pdbcc);
		}
		else if (::IsEqualGUID(GUID_DEVINTERFACE_VOLUME, pdbcc->dbcc_classguid))
		{
			OnVolumeDeviceInterfaceArrival(pdbcc);
		}
		else if (::IsEqualGUID(GUID_DEVINTERFACE_DISK, pdbcc->dbcc_classguid))
		{
			OnDiskDeviceInterfaceArrival(pdbcc);
		}
		else if (::IsEqualGUID(GUID_DEVINTERFACE_CDROM, pdbcc->dbcc_classguid))
		{
			OnCdRomDeviceInterfaceArrival(pdbcc);
		}
	}
}

void
CNdasServiceDeviceEventHandler::
OnDeviceRemoveComplete(
	PDEV_BROADCAST_HDR pdbhdr)
{
	XTLASSERT(m_bInitialized);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceRemoveComplete: Type %x\n", pdbhdr->dbch_devicetype);

	//
	// RemoveComplete events are reported to 
	// both Device Interface and Device Handle
	//
	// We prefer to handling them in Device Handle
	//

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) 
	{
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"OnDeviceRemoveComplete : Interface %ls\n", 
			ximeta::CGuid(&pdbcc->dbcc_classguid).ToString());

		if (::IsEqualGUID(GUID_DEVINTERFACE_STORAGEPORT, pdbcc->dbcc_classguid))
		{
			OnStoragePortDeviceInterfaceRemoveComplete(pdbcc);
		}
		else if (::IsEqualGUID(GUID_DEVINTERFACE_VOLUME, pdbcc->dbcc_classguid))
		{
			OnVolumeDeviceInterfaceRemoveComplete(pdbcc);
		}
		else if (::IsEqualGUID(GUID_DEVINTERFACE_DISK, pdbcc->dbcc_classguid))
		{
			OnDiskDeviceInterfaceRemoveComplete(pdbcc);
		}
		else if (::IsEqualGUID(GUID_DEVINTERFACE_CDROM, pdbcc->dbcc_classguid))
		{
			OnCdRomDeviceInterfaceRemoveComplete(pdbcc);
		}
	} 
	else if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) 
	{

		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"OnDeviceRemoveComplete: Handle %p, Notify Handle %p\n",
			pdbch->dbch_handle, pdbch->dbch_hdevnotify);

		OnDeviceHandleRemoveComplete(pdbch);
	}

	// TODO: Set Logical Device Status (LDS_UNMOUNTED)
}

void
CNdasServiceDeviceEventHandler::
OnDeviceRemovePending(
	PDEV_BROADCAST_HDR pdbhdr)
{
	XTLASSERT(m_bInitialized);

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceRemovePending: Type %x\n", pdbhdr->dbch_devicetype);

	//
	// DeviceInterface never gets RemovePending events
	// DeviceHandle receives these events not DeviceInterface
	// only if we have registered a device handle notification
	//

	//if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
	//	PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
	//		reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

	//	if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
	//		return OnStoragePortDeviceInterfaceRemovePending(pdbcc);
	//	} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
	//		return OnVolumeDeviceInterfaceRemovePending(pdbcc);
	//	} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
	//		return OnDiskDeviceInterfaceRemovePending(pdbcc);
	//	} else if (CdRomClassGuid == pdbcc->dbcc_classguid) {
	//		return OnCdRomDeviceInterfaceRemovePending(pdbcc);
	//	}
	//}


	if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) 
	{
		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		OnDeviceHandleRemovePending(pdbch);
	}
}

LRESULT
CNdasServiceDeviceEventHandler::
OnDeviceQueryRemove(
	PDEV_BROADCAST_HDR pdbhdr)
{
	XTLASSERT(m_bInitialized);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceQueryRemove: Type %x\n", pdbhdr->dbch_devicetype);

	//
	// DeviceInterface never gets RemovePending events
	// DeviceHandle receives these events not DeviceInterface
	// only if we have registered a device handle notification
	//

	//if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
	//	PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
	//		reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);
	//	if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
	//		return OnStoragePortDeviceInterfaceQueryRemove(pdbcc);
	//	} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
	//		return OnVolumeDeviceInterfaceQueryRemove(pdbcc);
	//	} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
	//		return OnDiskDeviceInterfaceQueryRemove(pdbcc);
	//	} else if (CdRomClassGuid == pdbcc->dbcc_classguid) {
	//		return OnCdRomDeviceInterfaceQueryRemove(pdbcc);
	//	}
	//}
	
	if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) 
	{
		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		return OnDeviceHandleQueryRemove(pdbch);
	}

	return TRUE;
}

void
CNdasServiceDeviceEventHandler::
OnDeviceQueryRemoveFailed(
	PDEV_BROADCAST_HDR pdbhdr)
{
	XTLASSERT(m_bInitialized);
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceQueryRemoveFailed: Type %x\n", pdbhdr->dbch_devicetype);

	//
	// DeviceInterface never gets RemovePending events
	// DeviceHandle receives these events not DeviceInterface
	// only if we have registered a device handle notification
	//

	//if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
	//	PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
	//		reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

	//	if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
	//		return OnStoragePortDeviceInterfaceQueryRemoveFailed(pdbcc);
	//	} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
	//		return OnVolumeDeviceInterfaceQueryRemoveFailed(pdbcc);
	//	} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
	//		return OnDiskDeviceInterfaceQueryRemoveFailed(pdbcc);
	//	} else if (CdRomClassGuid == pdbcc->dbcc_classguid) {
	//		return OnCdRomDeviceInterfaceQueryRemoveFailed(pdbcc);
	//	}
	//}
	
	if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) 
	{
		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		OnDeviceHandleQueryRemoveFailed(pdbch);
	}

	// TODO: Set Logical Device Status to Remove Failed (LDS_MOUNTED)
}


bool
CNdasServiceDeviceEventHandler::
FindDeviceHandle(
	HDEVNOTIFY NotifyHandle, 
	DeviceNotifyData& Data)
{
	DeviceNotifyHandleMap::iterator itr = m_DevNotifyMap.find(NotifyHandle);
	if (m_DevNotifyMap.end() == itr) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Device notify handle %p is not registered.\n", 
			NotifyHandle);
		return false;
	}

	Data = itr->second;
	return true;
}

bool
CNdasServicePowerEventHandler::Initialize()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_VERBOSE, "CNdasServicePowerEventHandler::Initialize.\n");
	return true;
}

//
// Return TRUE to grant the request to suspend. 
// To deny the request, return BROADCAST_QUERY_DENY.
//
LRESULT 
CNdasServicePowerEventHandler::
OnQuerySuspend(DWORD dwFlags)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnQuerySuspend.\n");

	// A DWORD value dwFlags specifies action flags. 
	// If bit 0 is 1, the application can prompt the user for directions 
	// on how to prepare for the suspension; otherwise, the application 
	// must prepare without user interaction. All other bit values are reserved. 

	DWORD dwValue = NdasServiceConfig::Get(nscSuspendOptions);

	if (NDASSVC_SUSPEND_ALLOW == dwValue) 
	{
		return TRUE;
	}

	CNdasLogicalDeviceManager& manager = m_service.GetLogicalDeviceManager();

	CNdasLogicalDeviceVector logDevices;

	// do not unlock the logical device collection here
	// we want to make sure that there will be no registration 
	// during mount status check (until the end of this function)

	manager.Lock();
	manager.GetItems(logDevices);
	manager.Unlock();

	bool fMounted = (
		logDevices.end() != 
		std::find_if(
			logDevices.begin(), logDevices.end(), 
			IsLogicalDeviceMounted()));

	//
	// Service won't interact with the user
	// If you want to make this function interactive
	// You should set the NDASSVC_SUSPEND_ALLOW
	// and the UI application should process NDASSVC_SUSPEND by itself
	//
	if (fMounted) 
	{
		if (0x01 == (dwFlags & 0x01)) 
		{
			//
			// Possible to interact with the user
			//
			(void) m_service.GetEventPublisher().SuspendRejected();
			return BROADCAST_QUERY_DENY;
		}
		else 
		{
			//
			// No User interface is available
			//
			(void) m_service.GetEventPublisher().SuspendRejected();
			return BROADCAST_QUERY_DENY;
		}
	}

	return TRUE;
}

CNdasServicePowerEventHandler::
CNdasServicePowerEventHandler(CNdasService& service) :
	m_service(service)
{
}

void
CNdasServicePowerEventHandler::
OnQuerySuspendFailed()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnQuerySuspendFailed.\n");
	return;
}

void
CNdasServicePowerEventHandler::
OnSuspend()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnSuspend.\n");
	return;
}

void
CNdasServicePowerEventHandler::
OnResumeAutomatic()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnResumeAutomatic.\n");
	return;
}

void
CNdasServicePowerEventHandler::
OnResumeCritical()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnResumeCritical.\n");
	return;
}

void
CNdasServicePowerEventHandler::
OnResumeSuspend()
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, "OnResumeSuspend.\n");
	return;
}

//
// Local utility functions
//
namespace
{

bool
IsWindows2000()
{
	static bool bHandled = false;
	static bool bWindows2000 = false;

	if (!bHandled) 
	{
		OSVERSIONINFOEX osvi;
		::ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		BOOL fSuccess = ::GetVersionEx((OSVERSIONINFO*) &osvi);
		XTLASSERT(fSuccess);
		if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) 
		{
			bWindows2000 = true;
		}
		bHandled = true;
	}

	return bWindows2000;
}

LPCSTR 
DeviceNotifyDataTypeStringA(
	DeviceNotifyData::DataType Type)
{
	switch (Type) 
	{
	case DeviceNotifyData::DNTStoragePort:
		return "STORAGEPORT";
	case DeviceNotifyData::DNTDisk: 
		return "DISK";
	case DeviceNotifyData::DNTCdRom: 
		return "CDROM";
	case DeviceNotifyData::DNTVolume: 
		return "VOLUME";
	default:
		return "DNT_???";
	}
}

void
pSetDiskHotplugInfoByPolicy(
	HANDLE hDisk)
{
	BOOL fNoForceSafeRemoval = NdasServiceConfig::Get(nscDontForceSafeRemoval);

	if (fNoForceSafeRemoval)
	{
		// do not force safe hot plug
		return;
	}

	BOOL bMediaRemovable, bMediaHotplug, bDeviceHotplug;
	BOOL fSuccess = sysutil::GetStorageHotplugInfo(
		hDisk, 
		&bMediaRemovable, 
		&bMediaHotplug, 
		&bDeviceHotplug);

	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"GetStorageHotplugInfo failed, error=0x%X\n", GetLastError());
		return;
	} 

	bDeviceHotplug = TRUE;
	fSuccess = sysutil::SetStorageHotplugInfo(
		hDisk,
		bMediaRemovable,
		bMediaHotplug,
		bDeviceHotplug);

	if (!fSuccess) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"StorageHotplugInfo failed, error=0x%X\n", GetLastError());
	}

	return;
}

} // end of namespace

