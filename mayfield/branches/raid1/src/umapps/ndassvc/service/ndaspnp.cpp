#include "stdafx.h"
#include "ndaspnp.h"
#include <winioctl.h>
#include <setupapi.h>

// #include "ntddstor.h" // for StoragePortClassGuid, VolumeClassGuid
#include "drivematch.h"

#include "rofiltctl.h"

#include "ndasinstman.h"
#include "ndasdev.h"
#include "autores.h"
#include "ndascfg.h"
#include "ndasobjs.h"

#include "xguid.h"
#include "sysutil.h"
#include "ndassvcparam.h"

#include "xdbgflags.h"
#define XDBG_MODULE_FLAG XDF_NDASPNP
#include "xdebug.h"

static BOOL IsWindows2000();

static BOOL IsWindows2000()
{
	static BOOL bHandled = FALSE;
	static BOOL bWindows2000 = FALSE;

	if (!bHandled) {
		OSVERSIONINFOEX osvi;
		::ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		BOOL fSuccess = ::GetVersionEx((OSVERSIONINFO*) &osvi);
		_ASSERT(fSuccess);
		if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) {
			bWindows2000 = TRUE;
		}
		bHandled = TRUE;
	}

	return bWindows2000;
}

//////////////////////////////////////////////////////////////////////////

CNdasServiceDeviceEventHandler::CNdasServiceDeviceEventHandler(
	HANDLE hRecipient, 
	DWORD dwReceptionFlags) :
	m_bInitialized(FALSE),
	m_bROFilterFilteringStarted(FALSE),
	m_hROFilter(INVALID_HANDLE_VALUE),
	m_bNoLfs(FALSE),
	m_hRecipient(hRecipient),
	m_dwReceptionFlags(dwReceptionFlags),
	m_hStoragePortNotify(NULL),
	m_hVolumeNotify(NULL),
	m_hDiskNotify(NULL),
	m_hCdRomClassNotify(NULL)
{
	_ASSERTE(INVALID_HANDLE_VALUE != hRecipient);
	_ASSERTE(NULL != hRecipient);

	// as DEVICE_NOFITY_WINDOW_HANDLE is 0x000,
	// _ASSERTE(DEVICE_NOFITY_WINDOW_HANDLE & m_dwReceptionFlags)
	// will be always false!
	_ASSERTE(
		((DEVICE_NOTIFY_SERVICE_HANDLE & m_dwReceptionFlags) == DEVICE_NOTIFY_SERVICE_HANDLE) ||
		((DEVICE_NOTIFY_WINDOW_HANDLE & m_dwReceptionFlags) == DEVICE_NOTIFY_WINDOW_HANDLE));

	BOOL bNoLfs(FALSE);
	BOOL fSuccess = _NdasServiceCfg.GetValue(_T("NoLFS"), &bNoLfs);
	if (fSuccess) {
		m_bNoLfs = bNoLfs;
	}
}

CNdasServiceDeviceEventHandler::~CNdasServiceDeviceEventHandler()
{
	if (m_bROFilterFilteringStarted) {
		BOOL fSuccess = NdasRoFilterStopFilter(m_hROFilter);
		if (!fSuccess) {
			DPWarningEx(_FT("Failed to stop ROFilter session: "));
		}
	}

	if (m_hStoragePortNotify) {
		::UnregisterDeviceNotification(m_hStoragePortNotify);
	}

	if (m_hDiskNotify) {
		::UnregisterDeviceNotification(m_hDiskNotify);
	}

	if (m_hCdRomClassNotify) {
		::UnregisterDeviceNotification(m_hCdRomClassNotify);
	}

	if (m_hVolumeNotify) {
		::UnregisterDeviceNotification(m_hVolumeNotify);
	}
}

BOOL
CNdasServiceDeviceEventHandler::Initialize()
{
	//
	// Do not initialize twice if successful.
	//
	_ASSERTE(!m_bInitialized);

	//
	// register Storage Port, Volume and Disk device notification
	//

	m_hStoragePortNotify = RegisterDeviceInterfaceNotification(&StoragePortClassGuid);
	if (NULL == m_hStoragePortNotify) {
		DPErrorEx(TEXT("Registering Storage Port Device Notification failed: "));
	}

	m_hVolumeNotify = RegisterDeviceInterfaceNotification(&VolumeClassGuid);
	if (NULL == m_hVolumeNotify) {
		DPErrorEx(TEXT("Registering Volume Device Notification failed: "));
	}

	m_hDiskNotify = RegisterDeviceInterfaceNotification(&DiskClassGuid);

	if (NULL == m_hDiskNotify) {
		DPErrorEx(TEXT("Registering Disk Device Notification failed: "));
	}

	m_hCdRomClassNotify = RegisterDeviceInterfaceNotification(&CdRomClassGuid);

	if (NULL == m_hCdRomClassNotify) {
		DPErrorEx(TEXT("Registering CDROM Device Notification failed: "));
	}

	DPInfo(_FT("Storage Port Notify Handle: %p\n"), m_hStoragePortNotify);
	DPInfo(_FT("Volume Notify Handle      : %p\n"), m_hVolumeNotify);
	DPInfo(_FT("Disk Device Notify Handle : %p\n"), m_hDiskNotify);
	DPInfo(_FT("CDROM Device Notify Handle : %p\n"), m_hCdRomClassNotify);

	//
	// nothing more for Windows XP or later
	//

	//
	// If NoLfs is set and if the OS is Windows 2000, 
	// load ROFilter service
	//
	if (!(IsWindows2000() && m_bNoLfs)) {
		m_bInitialized = TRUE;
		return TRUE;
	}

	_ASSERTE(INVALID_HANDLE_VALUE == m_hROFilter);

	//
	// Even if the rofilter is not loaded
	// initialization returns TRUE
	// However, m_hROFilter has INVALID_HANDLE_VALUE
	//

	m_bInitialized = TRUE;

	SERVICE_STATUS serviceStatus;
	BOOL fSuccess = NdasRoFilterQueryServiceStatus(&serviceStatus);
	if (!fSuccess) {
		DPErrorEx(_FT("NdasRoFilterQueryServiceStatus failed: "));
		return TRUE;
	}

	if (SERVICE_RUNNING != serviceStatus.dwCurrentState) {
		fSuccess = NdasRoFilterStartService();
		if (!fSuccess) {
			DPErrorEx(_FT("NdasRoFilterStartService failed: "));
			return TRUE;
		}
	}

	HANDLE hROFilter = NdasRoFilterOpenDevice();
	if (INVALID_HANDLE_VALUE == hROFilter) {
		DPErrorEx(_FT("NdasRoFilterCreate failed: "));
		return TRUE;
	}

	m_hROFilter = hROFilter;

	return TRUE;
}



HDEVNOTIFY
CNdasServiceDeviceEventHandler::RegisterDeviceInterfaceNotification(
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
CNdasServiceDeviceEventHandler::RegisterDeviceHandleNotification(
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
CNdasServiceDeviceEventHandler::OnStoragePortDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	BOOL fSuccess(FALSE);
	DWORD dwSlotNo(0);

	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(pdbhdr->dbcc_name, &dwSlotNo);
	if (!fSuccess) {
		// non LANSCSI-port device, ignore
		return TRUE;
	}

	AutoFileHandle hStoragePort = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	if (INVALID_HANDLE_VALUE == hStoragePort) {
		//
		// TODO: Set Logical Device Status to 
		// LDS_UNMOUNT_PENDING -> LDS_NOT_INITIALIZED (UNMOUNTED)
		// and Set LDS_ERROR? However, which one?
		//
		DPErrorEx(_FT("Failed to CreateFile(%s): "), (LPCTSTR) pdbhdr->dbcc_name);
		return TRUE;
	}

	//
	// Availability of this resource is subject to change due to unknown reason?
	// NdasDmGetNdasLogDevSlotNoOfScsiPort may not succeed.
	//
	DWORD dwSlotNo2;
	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(hStoragePort, &dwSlotNo2);
	if (!fSuccess) {
		DPWarningEx(_FT("NdasDmGetNdasLogDevSlotNoOfDisk(%p) failed: "), hStoragePort);
	//	DPWarningEx(_FT("May not be NDAS logical device. Ignoring.\n"));
	//	return TRUE;
	}
	DPInfo(_FT("NdasDmGetNdasLogDevSlotNoOfScsiPort returned %d.\n"), dwSlotNo2);

	//
	// We got the LANSCSI miniport
	//

	DPInfo(_FT("NDAS Logical Device Slot No: %d.\n"), dwSlotNo);

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
	//	DPWarningEx(
	//		_FT("NDAS logical device storage port notify already registered ")
	//		_T("at slot %d. Deleting old ones: "), dwSlotNo);
	//	::UnregisterDeviceNotification(itr->second);
	//}

	//
	// register new one
	//

	HDEVNOTIFY hDevNotify = RegisterDeviceHandleNotification(hStoragePort);
	if (NULL == hDevNotify) {
		DPWarningEx(_FT("Registering device handle for NDAS logical device")
			_T("at slot %d failed: "), dwSlotNo);
	} else {

		DPInfo(_FT("StoragePort Handle Notification Registered successfully")
			_T(" of slot %d.\n"), dwSlotNo);

		DevNotifyInfo dni = {0};
		dni.LogDevSlotNo = dwSlotNo;
		dni.Type = DEVNOTIFYINFO_TYPE_STORAGEPORT;

		m_DevNotifyMap.insert(
			DevNotifyMap::value_type(hDevNotify, dni));

	}

	return TRUE;
}

//LRESULT 
//CNdasServiceDeviceEventHandler:://OnStoragePortDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
//{
//	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
//	return TRUE;
//}
//
//LRESULT 
//CNdasServiceDeviceEventHandler:://OnStoragePortDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
//{
//	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
//
//	BOOL fSuccess(FALSE);
//	DWORD dwSlotNo(0);
//
//	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(pdbhdr->dbcc_name, &dwSlotNo);
//	if (!fSuccess) {
//		// non LANSCSI-port device, ignore
//		return TRUE;
//	}
//
//	//
//	// On remove complete ::CreateFile to the device file will fail!
//	// Use only dbcc_name!
//	//
//
//	DPInfo(_FT("NDAS Logical Device Slot No: %d.\n"), dwSlotNo);
//
//	//
//	// we are to set the logical device status 
//	// from UNMOUNT_PENDING -> MOUNTED
//	//
//
//	PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(dwSlotNo);
//	if (NULL == pLogDevice) {
//		// unhandled NDAS logical device
//		DPWarning(_FT("Logical device on slot %d not found in LDM.\n"), dwSlotNo);
//		return TRUE;
//	}
//
//	pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
//
//	DPInfo(_FT("Logical device status is set to NDAS_LOGICALDEVICE_STATUS_MOUNTED")
//		_T(" on slot %d due to QueryRemoveFailed.\n"), dwSlotNo);
//
//	return TRUE;
//}
//
//LRESULT 
//CNdasServiceDeviceEventHandler:://OnStoragePortDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
//{
//	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
//	return TRUE;
//}

LRESULT 
CNdasServiceDeviceEventHandler::OnStoragePortDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	BOOL fSuccess(FALSE);

	//
	// This is an another way to get the notification of the removal of
	// the PNP device
	//

	CNdasScsiLocation location(0);
	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(
		pdbhdr->dbcc_name, 
		&location.SlotNo);

	if (!fSuccess) {
		// non LANSCSI-port device, ignore
		return TRUE;
	}

	//
	// On remove complete ::CreateFile to the device file will fail!
	// Use only dbcc_name!
	//

	DPInfo(_FT("NDAS Logical Device at: %s.\n"), location.ToString());

	////
	//// unregister device notification
	//// 
	//LogDevSlot_DevNotify_Map::iterator itr =
	//	m_StoragePortNotifyMap.find(dwSlotNo);
	//if (m_StoragePortNotifyMap.end() == itr) {
	//	DPWarning(
	//		_FT("NDAS logical device storage port notify not found.")
	//		_T("at slot %d: "), dwSlotNo);
	//} else {

	//	HDEVNOTIFY hDevNotify = itr->second;
	//	m_StoragePortNotifyMap.erase(itr);

	//	DevNotify_LogDevSlot_Map::iterator itr2 = 
	//		m_LogDevSlotMap.find(hDevNotify);
	//	if (m_LogDevSlotMap.end() == itr2) {
	//		//
	//		// It should always exist!
	//		//
	//		_ASSERTE(FALSE);
	//		DPWarning(
	//			_FT("StoragePortNotify %p is not found in LogDevSlot Map.\n"),
	//			hDevNotify);
	//	} else {
	//		m_LogDevSlotMap.erase(itr2);
	//	}

	//	::UnregisterDeviceNotification(hDevNotify);

	//	DPInfo(_FT("StoragePort Handle Notification unregistered successfully")
	//		_T(" of slot %d.\n"), dwSlotNo);

	//}

	//
	// we are to set the logical device status 
	// from UNMOUNT_PENDING -> UNMOUNTED
	//

	CNdasLogicalDevice* pLogDevice = pGetNdasLogicalDevice(location);
	if (NULL == pLogDevice) {
		// unhandled NDAS logical device
		DPWarning(_FT("Logical device at %s not found in LDM.\n"), location.ToString());
		return TRUE;
	}

	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == pLogDevice->GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == pLogDevice->GetStatus() ||
		NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus())
	{
		pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);
		DPInfo(_FT("Logical device %s is changed to NDAS_LOGICALDEVICE_STATUS_UNMOUNTED.\n"), pLogDevice->ToString());
	}


	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnVolumeDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	// Return TRUE to grant the request.
	// Return BROADCAST_QUERY_DENY to deny the request.


	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

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

	AutoFileHandle hVolume = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_ALWAYS,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == hVolume) {
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		DPErrorEx(_FT("Failed to CreateFile(%s): "), (LPCTSTR) pdbhdr->dbcc_name);

		// TODO: Return BROADCAST_QUERY_DENY?
		return TRUE;
	}

	DWORD lpdwSlotNo[256];
	DWORD cSlots;
	BOOL fSuccess = NdasDmGetNdasLogDevSlotNoOfVolume(hVolume, lpdwSlotNo, 256, &cSlots);

	if (!fSuccess) {

		DPErrorEx(_FT("Failed to NdasDmGetNdasLogDevSlotNoOfVolume(hVolume %p): "),
			hVolume);
		DPError(_FT("May not span a NDAS logical device!!!\n"));
		
		//
		// TODO:
		//
		// Should exactly process here
		//
		// BUG: 
		// At this time, we cannot distinguish if the NdasDmGetNdasLogDevSlotNoOfVolume()
		// is failed because it's not a NDAS logical device or
		// because of the actual system error.
		//
		// We are assuming that it's not a NDAS logical device.
		//

		// TODO: Return BROADCAST_QUERY_DENY?
		return TRUE;
	}


	for (DWORD i = 0; i < cSlots; ++i) {
		DPInfo(_FT("Volume spans NDAS logical devices slot (%d/%d): %d\n"), 
			i, cSlots, lpdwSlotNo[i]);
	}

// #define EXP_VOLUME_NDAS_ICON
#ifdef EXP_VOLUME_NDAS_ICON
	if (!IsWindows2000()) {

		HDEVINFO hDevInfoSet = ::SetupDiCreateDeviceInfoList(NULL, NULL);
		if (INVALID_HANDLE_VALUE == hDevInfoSet) {
			_ASSERTE(FALSE);
		}

		SP_DEVICE_INTERFACE_DATA devIntfData = {0};
		devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		fSuccess = ::SetupDiOpenDeviceInterface(
			hDevInfoSet, 
			pdbhdr->dbcc_name,
			0,
			&devIntfData);

		_ASSERTE(fSuccess);

		HKEY hDevKey = ::SetupDiOpenDeviceInterfaceRegKey(
			hDevInfoSet,
			&devIntfData,
			0,
			KEY_READ | KEY_WRITE);

		DWORD dwError = ::GetLastError();

		if (INVALID_HANDLE_VALUE == hDevKey && 
			ERROR_FILE_NOT_FOUND == ::GetLastError()) 
		{
			hDevKey = ::SetupDiCreateDeviceInterfaceRegKey(
				hDevInfoSet,
				&devIntfData,
				0,
				KEY_READ | KEY_WRITE,
				NULL,
				NULL);
			_ASSERTE(INVALID_HANDLE_VALUE != hDevKey);
		} else if (INVALID_HANDLE_VALUE == hDevKey) {
			_ASSERTE(FALSE);
		}

		HKEY hDevParamKey = (HKEY)INVALID_HANDLE_VALUE;
		DWORD dwKeyDisp = 0;

		LONG lRes = ::RegCreateKeyEx(
			hDevKey,
			_T("Device Parameters"),
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_READ | KEY_WRITE,
			NULL,
			&hDevParamKey,
			&dwKeyDisp);

		_ASSERTE(ERROR_SUCCESS == lRes);

		LPCTSTR szDeviceGroup = _T("NDASDiskVolume");

		lRes = ::RegSetValueEx(
			hDevParamKey,
			_T("DeviceGroup"),
			0,
			REG_MULTI_SZ,
			(CONST BYTE*) szDeviceGroup,
			sizeof(szDeviceGroup));

		_ASSERTE(ERROR_SUCCESS == lRes);

		fSuccess = ::SetupDiDestroyDeviceInfoList(hDevInfoSet);
		_ASSERTE(fSuccess);

		lRes = ::RegCloseKey(hDevParamKey);
		_ASSERTE(ERROR_SUCCESS == lRes);

		lRes = ::RegCloseKey(hDevKey);
		_ASSERTE(ERROR_SUCCESS == lRes);
	}


#endif

	//
	// For each NDAS Logical Devices, we should check granted-access
	//

	//
	// minimum access
	//

	ACCESS_MASK allowedAccess = GENERIC_READ | GENERIC_WRITE;
	for (DWORD i = 0; i < cSlots; ++i) {
		NDAS_SCSI_LOCATION location = {lpdwSlotNo[i], 0, 0};
		PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(location);
		if (NULL == pLogDevice) {
			DBGPRT_WARN(_FT("Logical device is not available. Already unmounted?\n"));
			return TRUE;
		}
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status) 
		{
			DBGPRT_WARN(_FT("Logical device is mounted or mount pending. Already unmounted?\n"));
			return TRUE;
		}
		allowedAccess &= pLogDevice->GetMountedAccess();
	}

	DPInfo(_FT("Volume has granted as 0x%08X (%s %s).\n"), 
		allowedAccess,
		(allowedAccess & GENERIC_READ) ? TEXT("GENERIC_READ") : TEXT(""),
		(allowedAccess & GENERIC_WRITE) ? TEXT("GENERIC_WRITE") : TEXT(""));

	if ((allowedAccess & GENERIC_WRITE)) {
		//
		// nothing to do for write-access
		//

		//
		// Volume handle is no more required
		//
		::CloseHandle(hVolume.Detach());

	} else {

		DPInfo(_FT("Loading ROFilter.\n"));

		//
		// filter to the logical drives (DOS devices)
		// it's sufficient to filter the first drive letter
		// to filter other mount-point'ed locations
		// ROFilter does it all!
		//

		DWORD dwDriveLetterNumber;
		fSuccess = NdasDmGetDriveNumberOfVolume(hVolume, &dwDriveLetterNumber);
		if (!fSuccess) {
			DPErrorEx(_FT("Failed to NdasDmGetDriveNumberOfVolume(hVolume %p): "),
				hVolume);

			// TODO: Return BROADCAST_QUERY_DENY?
			return TRUE;
		}

		//
		// Volume handle is no more required
		//
		::CloseHandle(hVolume.Detach());

		DPInfo(_FT("NdasDmGetDriveNumberOfVolume returns (%d).\n"), 
			dwDriveLetterNumber);

#define NO_ROFILTER
#ifndef NO_ROFILTER
		//
		// Defer starting filtering until we need it!
		//
		if (!m_bROFilterFilteringStarted) {
			fSuccess = NdasRoFilterStartFilter(m_hROFilter);
			if (!fSuccess) {
				DPErrorEx(_FT("Failed to NdasRoFilterStartFilter: "));
				// TODO: Return BROADCAST_QUERY_DENY?
				return TRUE;
			}
			DPInfo(_FT("RoFilter filtering started a session.\n"));
		}

		fSuccess = NdasRoFilterEnableFilter(m_hROFilter, dwDriveLetterNumber, TRUE);
		if (!fSuccess) {
			DPErrorEx(_FT("Failed to enable filter on drive %c: "),
				TCHAR(dwDriveLetterNumber + 'A'));
			// TODO: Return BROADCAST_QUERY_DENY?
			return TRUE;
		}

		//
		// Save in the filtered drive letter set
		//
		m_FilteredDriveNumbers.insert(dwDriveLetterNumber);

		DPInfo(_FT("RoFilter filtering started on drive %c.\n"),
			TCHAR(dwDriveLetterNumber + 'A'));

#endif // NO_ROFILTER

	}


	return TRUE;
}

//LRESULT 
//CNdasServiceDeviceEventHandler:://OnVolumeDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
//{
//	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
//	return TRUE;
//}
//
//LRESULT 
//CNdasServiceDeviceEventHandler:://OnVolumeDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
//{
//	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
//
//	return TRUE;
//}
//
//LRESULT 
//CNdasServiceDeviceEventHandler:://OnVolumeDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
//{
//	//
//	// A device or piece of media is about to be removed. 
//	// Cannot be denied.
//	//
//
//	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
//
//	//
//	// Nothing to do for other than Windows 2000 or ROFILTER is not used
//	//
//#ifndef NO_ROFILTER
//
//	if (!IsWindows2000()) {
//		return TRUE;
//	}
//
//	//
//	// if no ROFilter session is started,
//	// we have nothing to do.
//	//
//	if (!m_bROFilterFilteringStarted) {
//		return TRUE;
//	}
//
//	//
//	// If rofilter is loaded on this volume, unload it!
//	//
//
//	AutoHandle hVolume = ::CreateFile(
//		(LPCTSTR)pdbhdr->dbcc_name,
//		GENERIC_READ | GENERIC_WRITE,
//		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
//		NULL,
//		OPEN_ALWAYS,
//		0,
//		NULL);
//
//	if (INVALID_HANDLE_VALUE == hVolume) {
//		//
//		// TODO: Set Logical Device Status to 
//		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
//		// and Set LDS_ERROR? However, which one?
//		//
//		DPErrorEx(_FT("Failed to CreateFile(%s): "), (LPCTSTR) pdbhdr->dbcc_name);
//
//		// TODO: Return FALSE?
//		return FALSE;
//	}
//
//	DWORD dwDriveNumber;
//	BOOL fSuccess = NdasDmGetDriveNumberOfVolume(hVolume, &dwDriveNumber);
//	if (!fSuccess) {
//		DPErrorEx(_FT("Failed to Get drive number of the volume: "));
//		return TRUE;
//	}
//
//	FilterdDriveNumberSet::iterator itr =
//		m_FilteredDriveNumbers.find(dwDriveNumber);
//	if (itr != m_FilteredDriveNumbers.end()) {
//		fSuccess = NdasRoFilterEnableFilter(m_hROFilter, dwDriveNumber, FALSE);
//		if (!fSuccess) {
//			DPWarningEx(_FT("Failed to disable ROFilter on drive %d.\n"), dwDriveNumber);
//		}
//		DPInfo(_FT("ROFilter disabled on drive %d.\n"), dwDriveNumber);
//		m_FilteredDriveNumbers.erase(itr);
//	}
//
//#endif // NO_ROFILTER
//
//	return TRUE;
//}

LRESULT 
CNdasServiceDeviceEventHandler::OnVolumeDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("Remove complete %s\n"), pdbhdr->dbcc_name);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Disk Device Interface Handlers
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CNdasServiceDeviceEventHandler::OnDiskDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	AutoFileHandle hDisk = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == hDisk) {
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		DPErrorEx(_FT("Failed to CreateFile(%s): "), (LPCTSTR) pdbhdr->dbcc_name);
		return TRUE;
	}

	BOOL fSuccess(FALSE);
	DWORD dwSlotNo(0);

	fSuccess = NdasDmGetNdasLogDevSlotNoOfDisk(hDisk, &dwSlotNo);
	if (!fSuccess) {
		DPWarningEx(_FT("NdasDmGetNdasLogDevSlotNoOfDisk(%p) failed.\n"), hDisk);
		DPWarningEx(_FT("May not be NDAS logical device. Ignoring.\n"));
		return TRUE;
	}

	DPInfo(_FT("NDAS Logical Device Slot No: %d.\n"), dwSlotNo);

	//
	// we are to set the logical device status 
	// from MOUNT_PENDING -> MOUNTED
	//

	//
	// minimum access
	//
	NDAS_SCSI_LOCATION location = {dwSlotNo, 0, 0};
	PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(location);
	if (NULL == pLogDevice) {
		// unhandled NDAS logical device
		DPWarning(_FT("Logical device on slot %d not found in LDM.\n"), dwSlotNo);
		return TRUE;
	}

	pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	DPInfo(_FT("Logical device status is set to MOUNTED on slot %d.\n"), dwSlotNo);

// TODO: by configuration!
#ifdef NDAS_FEATURE_DISABLE_WRITE_CACHE
	fSuccess = sysutil::DisableDiskWriteCache(hDisk);
	if (!fSuccess) {
		DPWarningEx(_FT("DisableDiskWriteCache failed: "));
	} else {
		DPInfo(_FT("Disk Write Cache disabled successfully.\n"));
	}
#endif

	BOOL bMediaRemovable, bMediaHotplug, bDeviceHotplug;
	fSuccess = sysutil::GetStorageHotplugInfo(
		hDisk, 
		&bMediaRemovable, 
		&bMediaHotplug, 
		&bDeviceHotplug);

	if (!fSuccess) {
		DPWarningEx(_FT("Unable to get StorageHotplugInfo: "));
	} else {

		bDeviceHotplug = TRUE;

		fSuccess = sysutil::SetStorageHotplugInfo(
			hDisk,
			bMediaRemovable,
			bMediaHotplug,
			bDeviceHotplug);

		if (!fSuccess) {
			DPWarningEx(_FT("Unable to set StorageHotplugInfo: "));
		}
	}
	//
	// Device Notification
	//
	HDEVNOTIFY hDevNotify = RegisterDeviceHandleNotification(hDisk);
	if (NULL == hDevNotify) {
		DPWarningEx(_FT("Registering device handle notification failed: "));
	} else {
		DevNotifyInfo dni = {0};
		dni.LogDevSlotNo = dwSlotNo;
		dni.Type = DEVNOTIFYINFO_TYPE_DISK;
		m_DevNotifyMap.insert(DevNotifyMap::value_type(hDevNotify, dni));
		DPInfo(_FT("Device handle notification registered successfully (%p).\n"), hDevNotify);
	}

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnDiskDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// CdRom Device Interface Handlers
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CNdasServiceDeviceEventHandler::OnCdRomDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	AutoFileHandle hDevice = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == hDevice) {
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		DPErrorEx(_FT("Failed to CreateFile(%s): "), (LPCTSTR) pdbhdr->dbcc_name);

		return TRUE;
	}

	BOOL fSuccess(FALSE);
	DWORD dwSlotNo(0);

	fSuccess = NdasDmGetNdasLogDevSlotNoOfDisk(hDevice, &dwSlotNo);
	if (!fSuccess) {
		DPWarningEx(_FT("NdasDmGetNdasLogDevSlotNoOfDisk(%p) failed.\n"), hDevice);
		DPWarningEx(_FT("May not be NDAS logical device. Ignoring.\n"));
		return TRUE;
	}

	DPInfo(_FT("NDAS Logical Device Slot No: %d.\n"), dwSlotNo);

	//
	// we are to set the logical device status 
	// from MOUNT_PENDING -> MOUNTED
	//

	NDAS_SCSI_LOCATION location = {dwSlotNo, 0, 0};
	PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(location);
	if (NULL == pLogDevice) {
		// unhandled NDAS logical device
		DPWarning(_FT("Logical device on slot %d not found in LDM.\n"), dwSlotNo);
		return TRUE;
	}

	pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	DPInfo(_FT("Logical device status is set to MOUNTED on slot %d.\n"), dwSlotNo);

	//
	// Device Notification
	//
	HDEVNOTIFY hDevNotify = RegisterDeviceHandleNotification(hDevice);
	if (NULL == hDevNotify) {
		DPWarningEx(_FT("Registering device handle notification failed: "));
	} else {
		DevNotifyInfo dni = {0};
		dni.LogDevSlotNo = dwSlotNo;
		dni.Type = DEVNOTIFYINFO_TYPE_CDROM;
		m_DevNotifyMap.insert(DevNotifyMap::value_type(hDevNotify, dni));
		DPInfo(_FT("Device handle notification registered successfully (%p).\n"), hDevNotify);
	}

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnCdRomDeviceInterfaceRemoveComplete(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Device Handle Handlers
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CNdasServiceDeviceEventHandler::OnDeviceHandleQueryRemove(
	PDEV_BROADCAST_HANDLE pdbch)
{
	DPInfo(_FT("Device Handle %p, Notify Handle %p\n"), 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnDeviceHandleQueryRemoveFailed(
	PDEV_BROADCAST_HANDLE pdbch)
{
	DPInfo(_FT("Device Handle %p, Notify Handle %p\n"), 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);

	DevNotifyMap::iterator itr = m_DevNotifyMap.find(pdbch->dbch_hdevnotify);
	if (m_DevNotifyMap.end() == itr) {
		//
		// Non-handled device???
		//
		DPWarning(_FT("Unregistered device notify handle %p.\n"), 
			pdbch->dbch_hdevnotify);

		return TRUE;
	}

	DevNotifyInfo dni = itr->second;

	DPInfo(_FT("LogDevSlotNo %d, Type %s\n"), 
		dni.LogDevSlotNo,
		DevNotifyInfoTypeString(dni.Type));

	if (DEVNOTIFYINFO_TYPE_CDROM == dni.Type ||
		DEVNOTIFYINFO_TYPE_DISK == dni.Type)
	{
		NDAS_SCSI_LOCATION location = {dni.LogDevSlotNo, 0, 0};
		PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(location);
		if (NULL == pLogDevice) {
			DPWarning(_FT("Logical Device not found at slot %d.\n"), dni.LogDevSlotNo);
			return TRUE;
		}

		pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);
	} 

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnDeviceHandleRemovePending(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// The system broadcasts the DBT_DEVICEREMOVEPENDING device event when 
	// a device or piece of media is being removed and 
	// is no longer available for use.
	//
	// However, this event is not raised on Surprise Removal
	// Surprise Removal is reported as OnDeviceHandleRemoveComplete
	//

	DPInfo(_FT("Device Handle %p, Notify Handle %p\n"), 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);


	DevNotifyMap::iterator itr = m_DevNotifyMap.find(pdbch->dbch_hdevnotify);
	if (m_DevNotifyMap.end() == itr) {
		//
		// Non-handled device???
		//
		DPWarning(_FT("Unregistered device notify handle %p.\n"), 
			pdbch->dbch_hdevnotify);

		return TRUE;
	}

	DevNotifyInfo dni = itr->second;

	m_DevNotifyMap.erase(itr);

	DPInfo(_FT("LogDevSlotNo %d, Type %s\n"), 
		dni.LogDevSlotNo,
		DevNotifyInfoTypeString(dni.Type));


	BOOL fSuccess = ::UnregisterDeviceNotification(pdbch->dbch_hdevnotify);
	if (!fSuccess) {
		DPWarning(_FT("Unregistering a device notification to %p failed: "),
			pdbch->dbch_hdevnotify);
	}

	//
	// Logical Device Status is changed only for Disk or CdRom device
	//
	if (DEVNOTIFYINFO_TYPE_CDROM == dni.Type ||
		DEVNOTIFYINFO_TYPE_DISK == dni.Type)
	{
		NDAS_SCSI_LOCATION location = {dni.LogDevSlotNo, 0, 0};
		PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(location);
		if (NULL == pLogDevice) {
			DPWarning(_FT("Logical Device not found at slot %d.\n"), dni.LogDevSlotNo);
			return TRUE;
		}

		pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	} 
	
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnDeviceHandleRemoveComplete(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// Device Handle Remove Complete is called on Surprise Removal
	//

	DPInfo(_FT("Device Handle %p, Notify Handle %p\n"), 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);

	DevNotifyMap::iterator itr = m_DevNotifyMap.find(pdbch->dbch_hdevnotify);
	if (m_DevNotifyMap.end() == itr) {
		//
		// Non-handled device???
		//
		DPWarning(_FT("Unregistered device notify handle %p.\n"), 
			pdbch->dbch_hdevnotify);

		return TRUE;
	}

	DevNotifyInfo dni = itr->second;

	m_DevNotifyMap.erase(itr);

	BOOL fSuccess = ::UnregisterDeviceNotification(pdbch->dbch_hdevnotify);
	if (!fSuccess) {
		DPWarning(_FT("Unregistering a device notification to %p failed: "),
			pdbch->dbch_hdevnotify);
	}

	if (DEVNOTIFYINFO_TYPE_CDROM == dni.Type ||
		DEVNOTIFYINFO_TYPE_DISK == dni.Type)
	{
		NDAS_SCSI_LOCATION location = {dni.LogDevSlotNo, 0, 0};
		PCNdasLogicalDevice pLogDevice = pGetNdasLogicalDevice(location);
		if (NULL == pLogDevice) {
			DPWarning(_FT("Logical Device not found at slot %d.\n"), dni.LogDevSlotNo);
			return TRUE;
		}

		pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	} 

	return TRUE;
}

LRESULT
CNdasServiceDeviceEventHandler::OnDeviceArrival(
	PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
			return OnStoragePortDeviceInterfaceArrival(pdbcc);
		} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
			return OnVolumeDeviceInterfaceArrival(pdbcc);
		} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
			return OnDiskDeviceInterfaceArrival(pdbcc);
		} else if (CdRomClassGuid == pdbcc->dbcc_classguid) {
			return OnCdRomDeviceInterfaceArrival(pdbcc);
		}
	}
	return CDeviceEventHandler::OnDeviceArrival(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::OnDeviceRemoveComplete(
	PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

	//
	// RemoveComplete events are reported to 
	// both Device Interface and Device Handle
	//
	// We prefer to handling them in Device Handle
	//

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		DPInfo(_FT("OnDeviceRemoveComplete : Interface %s\n"), 
			ximeta::CGuid(&pdbcc->dbcc_classguid).ToString());

		if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
			return OnStoragePortDeviceInterfaceRemoveComplete(pdbcc);
		} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
			return OnVolumeDeviceInterfaceRemoveComplete(pdbcc);
		} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
			return OnDiskDeviceInterfaceRemoveComplete(pdbcc);
		} else if (CdRomClassGuid == pdbcc->dbcc_classguid) {
			return OnCdRomDeviceInterfaceRemoveComplete(pdbcc);
		}
	} else if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) {

		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);

		DPInfo(_FT("OnDeviceRemoveComplete: Handle %p, Notify Handle %p\n"),
			pdbch->dbch_handle, pdbch->dbch_hdevnotify);

		return OnDeviceHandleRemoveComplete(pdbch);
	}

	// TODO: Set Logical Device Status (LDS_UNMOUNTED)
	return CDeviceEventHandler::OnDeviceRemoveComplete(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::OnDeviceRemovePending(
	PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);


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


	if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		return OnDeviceHandleRemovePending(pdbch);
	}

	return CDeviceEventHandler::OnDeviceRemovePending(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::OnDeviceQueryRemove(
	PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

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
	
	if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		return OnDeviceHandleQueryRemove(pdbch);
	}

	return CDeviceEventHandler::OnDeviceQueryRemove(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::OnDeviceQueryRemoveFailed(
	PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

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
	
	if (DBT_DEVTYP_HANDLE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_HANDLE pdbch =
			reinterpret_cast<PDEV_BROADCAST_HANDLE>(pdbhdr);
		return OnDeviceHandleQueryRemoveFailed(pdbch);
	}

	// TODO: Set Logical Device Status to Remove Failed (LDS_MOUNTED)
	return CDeviceEventHandler::OnDeviceQueryRemoveFailed(pdbhdr);
}

//
// Return TRUE to grant the request to suspend. 
// To deny the request, return BROADCAST_QUERY_DENY.
//
LRESULT 
CNdasServicePowerEventHandler::OnQuerySuspend(DWORD dwFlags)
{
	// A DWORD value dwFlags specifies action flags. 
	// If bit 0 is 1, the application can prompt the user for directions 
	// on how to prepare for the suspension; otherwise, the application 
	// must prepare without user interaction. All other bit values are reserved. 

	DWORD dwValue = 0;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		_T("ndassvc"),
		_T("Suspend"), 
		&dwValue);

	if (!fSuccess &&
		NDASSVC_SUSPEND_DENY != dwValue &&
		NDASSVC_SUSPEND_ALLOW != dwValue) // &&
		// NDASSVC_SUSPEND_EJECT_ALL != dwValue &&
		// NDASSVC_SUSPEND_UNPLUG_ALL != dwValue)
	{
		dwValue = NDASSVC_SUSPEND_DENY;
	}

	if (NDASSVC_SUSPEND_ALLOW == dwValue) {
		return TRUE;
	}

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasLogicalDeviceManager* pLogDevMan = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLogDevMan);

	CNdasLogicalDeviceManager::ConstIterator itr = pLogDevMan->begin();
	BOOL fMounted(FALSE);
	while (itr != pLogDevMan->end()) {
		CNdasLogicalDevice* pLogDevice = itr->second;
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status ||
			NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == status)
		{
			fMounted = TRUE;
			break;
		}
		++itr;
	}

	//
	// Service won't interact with the user
	// If you want to make this function interactive
	// You should set the NDASSVC_SUSPEND_ALLOW
	// and the UI application should process NDASSVC_SUSPEND by itself
	//
	if (fMounted) {
		if (0x01 == (dwFlags & 0x01)) {
			//
			// Possible to interact with the user
			//
			return BROADCAST_QUERY_DENY;
		} else {
			//
			// No User interface is available
			//
			return BROADCAST_QUERY_DENY;
		}
	}

	return TRUE;
}

VOID
CNdasServicePowerEventHandler::OnQuerySuspendFailed()
{
	return;
}

VOID
CNdasServicePowerEventHandler::OnSuspend()
{
	return;
}

VOID
CNdasServicePowerEventHandler::OnResumeAutomatic()
{
	return;
}

VOID
CNdasServicePowerEventHandler::OnResumeCritical()
{
	return;
}

VOID
CNdasServicePowerEventHandler::OnResumeSuspend()
{
	return;
}

