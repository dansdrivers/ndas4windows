#include "stdafx.h"
#include "ndaspnp.h"
#include <winioctl.h>
#include "ntddstor.h" // for StoragePortClassGuid, VolumeClassGuid
#include "drivematch.h"

#include "rofiltctl.h"

#include "ndasinstman.h"
#include "ndasdev.h"
#include "autores.h"
#include "ndascfg.h"

#include "xdbgflags.h"
#define XDEBUG_MODULE_FLAG XDF_PNP
#include "xdebug.h"

LPCWSTR 
CNdasServiceDeviceEventHandler::
LANSCSIDEV_IFID_W = L"\\\\?\\LanscsiBus#NetDisk_V0";

// LANSCSIDEV_IFID_W = L"\\\\?\\LANSCSIBUS#LANSCSIPORT_V0";

static BOOL DisableDiskWriteCache(LPCTSTR szDiskPath);
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

BOOL DisableDiskWriteCache(LPCTSTR szDiskPath)
{
	DPInfo(_FT("CreateFile(%s).\n"), szDiskPath);

	HANDLE hDisk = ::CreateFile(
		szDiskPath, 
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == hDisk) {
		DPErrorEx(_FT("CreateFile(%s) failed: "), szDiskPath);
		return FALSE;
	}

	BOOL fSuccess(FALSE);
	DWORD cbReturned;
	DISK_CACHE_INFORMATION diskCacheInfo;
	::ZeroMemory(&diskCacheInfo, sizeof(DISK_CACHE_INFORMATION));

	fSuccess = ::DeviceIoControl(
		hDisk,
		IOCTL_DISK_GET_CACHE_INFORMATION,
		NULL,
		0,
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_DISK_GET_CACHE_INFORMATION) failed: "));
		::CloseHandle(hDisk);
		return FALSE;
	}

	DPInfo(_FT("Disk Write Cache Enabled: %d.\n"), diskCacheInfo.WriteCacheEnabled);
	if (!diskCacheInfo.WriteCacheEnabled) {
		DPInfo(_FT("Disk Write Cache Already Disabled. Ignoring.\n"));
		::CloseHandle(hDisk);
		return TRUE;
	}

	diskCacheInfo.WriteCacheEnabled = FALSE;
	fSuccess = ::DeviceIoControl(
		hDisk,
		IOCTL_DISK_SET_CACHE_INFORMATION,
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&diskCacheInfo,
		sizeof(DISK_CACHE_INFORMATION),
		&cbReturned,
		NULL);

	if (!fSuccess) {
		DPErrorEx(_FT("DeviceIoControl(IOCTL_DISK_SET_CACHE_INFORMATION) failed: "));
		::CloseHandle(hDisk);
		return FALSE;
	}

	::CloseHandle(hDisk);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

CNdasServiceDeviceEventHandler::
CNdasServiceDeviceEventHandler(HANDLE hRecipient, DWORD dwReceptionFlags) :
	m_bInitialized(FALSE),
	m_bROFilterFilteringStarted(FALSE),
	m_hROFilter(INVALID_HANDLE_VALUE),
	m_bNoLfs(FALSE),
	m_hRecipient(hRecipient),
	m_dwReceptionFlags(dwReceptionFlags),
	m_hStoragePortNotify(NULL),
	m_hVolumeNotify(NULL),
	m_hDiskNotify(NULL)
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

CNdasServiceDeviceEventHandler::
~CNdasServiceDeviceEventHandler()
{
	if (m_bROFilterFilteringStarted) {
		BOOL fSuccess = NdasRoFilterStopFilter(m_hROFilter);
		if (!fSuccess) {
			DPWarningEx(_FT("Failed to stop ROFilter session: "));
		}
	}

	if (INVALID_HANDLE_VALUE != m_hROFilter) {
		::CloseHandle(m_hROFilter);
	}
}

BOOL
CNdasServiceDeviceEventHandler::
Initialize()
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

	DPInfo(_FT("Storage Port Notify Handle: %p\n"), m_hStoragePortNotify);
	DPInfo(_FT("Volume Notify Handle      : %p\n"), m_hVolumeNotify);
	DPInfo(_FT("Disk Device Notify Handle : %p\n"), m_hDiskNotify);

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

	HANDLE hROFilter = NdasRoFilterCreate();
	if (INVALID_HANDLE_VALUE == hROFilter) {
		DPErrorEx(_FT("NdasRoFilterCreate failed: "));
		return TRUE;
	}

	m_hROFilter = hROFilter;

	return TRUE;
}



HDEVNOTIFY
CNdasServiceDeviceEventHandler::
RegisterDeviceInterfaceNotification(LPCGUID classGuid)
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
RegisterDeviceHandleNotification(HANDLE hDeviceFile)
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
OnStoragePortDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	BOOL fSuccess(FALSE);
	DWORD dwSlotNo(0);

	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(pdbhdr->dbcc_name, &dwSlotNo);
	if (!fSuccess) {
		// non LANSCSI-port device, ignore
		return TRUE;
	}

	AutoHandle hStoragePort = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
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
	// this resource is not available at this time
	// fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(hStoragePort, &dwSlotNo);
	// if (!fSuccess) {
	//	DPWarningEx(_FT("NdasDmGetNdasLogDevSlotNoOfDisk(%p) failed.\n"), hStoragePort);
	//	DPWarningEx(_FT("May not be NDAS logical device. Ignoring.\n"));
	//	return TRUE;
	//}

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

	NdasLogicalDeviceNotifyMap::const_iterator itr =
		m_NdasLogicalDeviceStoragePortNotifyMap.find(dwSlotNo);
	if (m_NdasLogicalDeviceStoragePortNotifyMap.end() != itr) {
		DPWarningEx(
			_FT("NDAS logical device storage port notify already registered ")
			_T("at slot %d. Deleting old ones: "), dwSlotNo);
		::UnregisterDeviceNotification(itr->second);
	}

	//
	// register new one
	//

	HDEVNOTIFY hDevNotify = RegisterDeviceHandleNotification(hStoragePort);
	if (NULL != hDevNotify) {
		DPWarningEx(_FT("Registering device handle for NDAS logical device")
			_T("at slot %d failed: "), dwSlotNo);
	}

	DPInfo(_FT("StoragePort Handle Notification Registered successfully")
		_T(" of slot %d.\n"), dwSlotNo);

	m_NdasLogicalDeviceStoragePortNotifyMap[dwSlotNo] = hDevNotify;

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnStoragePortDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnStoragePortDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnStoragePortDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnStoragePortDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	BOOL fSuccess(FALSE);
	DWORD dwSlotNo(0);

	fSuccess = NdasDmGetNdasLogDevSlotNoOfScsiPort(pdbhdr->dbcc_name, &dwSlotNo);
	if (!fSuccess) {
		// non LANSCSI-port device, ignore
		return TRUE;
	}

	//
	// On remove complete ::CreateFile to the device file will fail!
	// Use only dbcc_name!
	//

	DPInfo(_FT("NDAS Logical Device Slot No: %d.\n"), dwSlotNo);

	//
	// unregister device notification
	// 
	NdasLogicalDeviceNotifyMap::const_iterator itr =
		m_NdasLogicalDeviceStoragePortNotifyMap.find(dwSlotNo);
	if (m_NdasLogicalDeviceStoragePortNotifyMap.end() == itr) {
		DPWarningEx(
			_FT("NDAS logical device storage port notify not found.")
			_T("at slot %d: "), dwSlotNo);
	} else {
		::UnregisterDeviceNotification(itr->second);
		m_NdasLogicalDeviceStoragePortNotifyMap.erase(dwSlotNo);

		DPInfo(_FT("StoragePort Handle Notification unregistered successfully")
			_T(" of slot %d.\n"), dwSlotNo);
	}


	//
	// we are to set the logical device status 
	// from UNMOUNT_PENDING -> UNMOUNTED
	//

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	PCNdasLogicalDeviceManager pLogDevMan = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLogDevMan);

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(dwSlotNo);
	if (NULL == pLogDevice) {
		// unhandled NDAS logical device
		DPWarning(_FT("Logical device on slot %d not found in LDM.\n"), dwSlotNo);
		return TRUE;
	}

	pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_UNMOUNTED);

	DPInfo(_FT("Logical device status is set to NOT_INIT on slot %d.\n"), dwSlotNo);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	// Return TRUE to grant the request.
	// Return BROADCAST_QUERY_DENY to deny the request.


	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	//
	// Nothing to do for other than Windows 2000
	//
	if (!IsWindows2000()) {
		return TRUE;
	}

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

	AutoHandle hVolume = ::CreateFile(
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


	//
	// For each NDAS Logical Devices, we should check granted-access
	//

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	PCNdasLogicalDeviceManager pLogDevMan = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLogDevMan);

	//
	// minimum access
	//

	ACCESS_MASK allowedAccess = GENERIC_READ | GENERIC_WRITE;
	for (DWORD i = 0; i < cSlots; ++i) {
		PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(lpdwSlotNo[i]);
		_ASSERTE(NULL != pLogDevice);
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		_ASSERTE(NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status);
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

	}

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	//
	// A device or piece of media is about to be removed. 
	// Cannot be denied.
	//

	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	//
	// Nothing to do for other than Windows 2000
	//
	if (!IsWindows2000()) {
		return TRUE;
	}

	//
	// if no ROFilter session is started,
	// we have nothing to do.
	//
	if (!m_bROFilterFilteringStarted) {
		return TRUE;
	}

	//
	// If rofilter is loaded on this volume, unload it!
	//

	AutoHandle hVolume = ::CreateFile(
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

		// TODO: Return FALSE?
		return FALSE;
	}

	DWORD dwDriveNumber;
	BOOL fSuccess = NdasDmGetDriveNumberOfVolume(hVolume, &dwDriveNumber);
	if (!fSuccess) {
		DPErrorEx(_FT("Failed to Get drive number of the volume: "));
		return TRUE;
	}

	FilterdDriveNumberSet::iterator itr =
		m_FilteredDriveNumbers.find(dwDriveNumber);
	if (itr != m_FilteredDriveNumbers.end()) {
		fSuccess = NdasRoFilterEnableFilter(m_hROFilter, dwDriveNumber, FALSE);
		if (!fSuccess) {
			DPWarningEx(_FT("Failed to disable ROFilter on drive %d.\n"), dwDriveNumber);
		}
		DPInfo(_FT("ROFilter disabled on drive %d.\n"), dwDriveNumber);
		m_FilteredDriveNumbers.erase(itr);
	}

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnVolumeDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);

	HANDLE hDisk = ::CreateFile(
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
		
		//
		// BUG: return FALSE?
		//
		return TRUE;
	}

	BOOL fSuccess(FALSE);
	DWORD dwSlotNo(0);

	fSuccess = NdasDmGetNdasLogDevSlotNoOfDisk(hDisk, &dwSlotNo);
	if (!fSuccess) {
		DPWarningEx(_FT("NdasDmGetNdasLogDevSlotNoOfDisk(%p) failed.\n"), hDisk);
		DPWarningEx(_FT("May not be NDAS logical device. Ignoring.\n"));
		::CloseHandle(hDisk);
		return TRUE;
	}

	DPInfo(_FT("NDAS Logical Device Slot No: %d.\n"), dwSlotNo);

	//
	// we are to set the logical device status 
	// from MOUNT_PENDING -> MOUNTED
	//

	PCNdasInstanceManager pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);
	PCNdasLogicalDeviceManager pLogDevMan = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLogDevMan);

	//
	// minimum access
	//

	PCNdasLogicalDevice pLogDevice = pLogDevMan->Find(dwSlotNo);
	if (NULL == pLogDevice) {
		// unhandled NDAS logical device
		DPWarning(_FT("Logical device on slot %d not found in LDM.\n"), dwSlotNo);
		::CloseHandle(hDisk);
		return TRUE;
	}

	pLogDevice->SetStatus(NDAS_LOGICALDEVICE_STATUS_MOUNTED);

	DPInfo(_FT("Logical device status is set to MOUNTED on slot %d.\n"), dwSlotNo);

	::CloseHandle(hDisk);

// TODO: by configuration!
	fSuccess = DisableDiskWriteCache(pdbhdr->dbcc_name);
	if (!fSuccess) {
		DPWarningEx(_FT("DisableDiskWriteCache failed: "));
	}

	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT 
CNdasServiceDeviceEventHandler::
OnDiskDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	DPInfo(_FT("%s\n"), pdbhdr->dbcc_name);
	return TRUE;
}

LRESULT
CNdasServiceDeviceEventHandler::
OnDeviceArrival(PDEV_BROADCAST_HDR pdbhdr)
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
		}
	}
	return CDeviceEventHandler::OnDeviceArrival(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::
OnDeviceRemoveComplete(PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
			return OnStoragePortDeviceInterfaceRemoveComplete(pdbcc);
		} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
			return OnVolumeDeviceInterfaceRemoveComplete(pdbcc);
		} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
			return OnDiskDeviceInterfaceRemoveComplete(pdbcc);
		}
	}
	// TODO: Set Logical Device Status (LDS_UNMOUNTED)
	return CDeviceEventHandler::OnDeviceRemoveComplete(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::
OnDeviceRemovePending(PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
			return OnStoragePortDeviceInterfaceRemovePending(pdbcc);
		} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
			return OnVolumeDeviceInterfaceRemovePending(pdbcc);
		} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
			return OnDiskDeviceInterfaceRemovePending(pdbcc);
		}
	}
	return CDeviceEventHandler::OnDeviceRemovePending(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::
OnDeviceQueryRemove(PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
			return OnStoragePortDeviceInterfaceQueryRemove(pdbcc);
		} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
			return OnVolumeDeviceInterfaceQueryRemove(pdbcc);
		} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
			return OnDiskDeviceInterfaceQueryRemove(pdbcc);
		}
	}
	return CDeviceEventHandler::OnDeviceQueryRemove(pdbhdr);
}

LRESULT
CNdasServiceDeviceEventHandler::
OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR pdbhdr)
{
	_ASSERTE(m_bInitialized);

	if (DBT_DEVTYP_DEVICEINTERFACE == pdbhdr->dbch_devicetype) {
		PDEV_BROADCAST_DEVICEINTERFACE pdbcc =
			reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pdbhdr);

		if (StoragePortClassGuid == pdbcc->dbcc_classguid ) {
			return OnStoragePortDeviceInterfaceQueryRemoveFailed(pdbcc);
		} else if (VolumeClassGuid == pdbcc->dbcc_classguid) {
			return OnVolumeDeviceInterfaceQueryRemoveFailed(pdbcc);
		} else if (DiskClassGuid == pdbcc->dbcc_classguid) {
			return OnDiskDeviceInterfaceQueryRemoveFailed(pdbcc);
		}
	}
	// TODO: Set Logical Device Status to Remove Failed (LDS_MOUNTED)
	return CDeviceEventHandler::OnDeviceQueryRemoveFailed(pdbhdr);
}

//
// Return TRUE to grant the request to suspend. 
// To deny the request, return BROADCAST_QUERY_DENY.
//
LRESULT 
CNdasServicePowerEventHandler::
OnQuerySuspend(DWORD dwFlags)
{
	// A DWORD value dwFlags specifies action flags. 
	// If bit 0 is 1, the application can prompt the user for directions 
	// on how to prepare for the suspension; otherwise, the application 
	// must prepare without user interaction. All other bit values are reserved. 

	CNdasInstanceManager* pInstMan = CNdasInstanceManager::Instance();
	_ASSERTE(NULL != pInstMan);

	CNdasLogicalDeviceManager* pLogDevMan = pInstMan->GetLogDevMan();
	_ASSERTE(NULL != pLogDevMan);

	CNdasLogicalDeviceManager::ConstIterator itr = pLogDevMan->begin();
	BOOL bMounted(FALSE);
	while (itr != pLogDevMan->end()) {
		CNdasLogicalDevice* pLogDevice = itr->second;
		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == status ||
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == status ||
			NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == status)
		{
			bMounted = TRUE;
			break;
		}
		++itr;
	}

	if (bMounted) {
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

void 
CNdasServicePowerEventHandler::
OnQuerySuspendFailed()
{
	return;
}

