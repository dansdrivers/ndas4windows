#include "stdafx.h"
#include <winioctl.h>
#include <setupapi.h>
#include <ntddscsi.h>
#include <rofiltctl.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasvol.h>
#include <ndas/ndasvolex.h>
#include <ndas/ndassvcparam.h>
#include <ndas/ndasportctl.h>

#include "ndascfg.h"
#include "ndasobjs.h"
#include "ndaseventpub.h"
#include "ndaseventmon.h"
#include "xguid.h"
#include "sysutil.h"

#include "sdi.h"

#include <initguid.h>
#include <ioevent.h>
#include <diskguid.h>
#include <ndas/ndasportguid.h>
#include <ndas/ndasdluguid.h>

#include "ndaspnp.h"

#include "trace.h"
#ifdef RUN_WPP
#include "ndaspnp.tmh"
#endif

namespace
{

const struct {
	LPCGUID Guid;
	LPCSTR TypeName;
} LogicalUnitInterfaceGuids[] = {
	&GUID_DEVINTERFACE_DISK, "DISK",
	&GUID_DEVINTERFACE_CDROM, "CDROM",
	&GUID_DEVINTERFACE_TAPE, "TAPE",
	&GUID_DEVINTERFACE_WRITEONCEDISK, "WRITEONCEDISK",
	&GUID_DEVINTERFACE_MEDIUMCHANGER, "MEDIUMCHANGER",
	&GUID_DEVINTERFACE_CDCHANGER, "CDCHANGER",
};

typedef std::vector<NDAS_LOCATION> NdasLocationVector;

HRESULT CALLBACK 
OnNdasVolume(NDAS_LOCATION NdasLocation, LPVOID Context)
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

struct DevNotifyHandleCloser :
	std::unary_function<HDEVNOTIFY,void>
{
	void operator()(HDEVNOTIFY DevNotifyHandle) const
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
			"Closing DevNotifyHandle=%p\n", DevNotifyHandle);

		XTLVERIFY( UnregisterDeviceNotification(DevNotifyHandle) );
	}
};

bool 
IsWindows2000();

void
pSetDiskHotplugInfoByPolicy(
	HANDLE hDisk);

HRESULT
pRegisterDeviceInterfaceNotification(
	__in HANDLE RecipientHandle,
	__in DWORD ReceptionFlags,
	__in LPCGUID InterfaceGuid,
	__out HDEVNOTIFY* DevNotifyHandle);

HRESULT
pRegisterDeviceHandleNotification(
	__in HANDLE RecipientHandle,
	__in DWORD ReceptionFlags,
	__in HANDLE DeviceHandle,
	__out HDEVNOTIFY* DevNotifyHandle);

struct EffectiveNdasAccess : 
	std::unary_function<NDAS_LOCATION,void> 
{
	EffectiveNdasAccess() : 
		AllowedAccess(GENERIC_READ | GENERIC_WRITE),
		NdasPort(FALSE) 
	{
	}
	void operator()(NDAS_LOCATION location) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"Volume on ndasLocation=%d\n", location);

		// If NDAS port exists, treats NDAS_LOCATION as a NDAS logical unit address in form of SCSI_ADDRESS.
		// translate the NDAS logical unit address to the NDAS location
		if (NdasPort) {
			NDAS_LOGICALUNIT_ADDRESS	ndasLogicalUnitAddress;
			ndasLogicalUnitAddress.Address = location;
			location = NdasLogicalUnitAddressToLocation(ndasLogicalUnitAddress);
		}

		CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

		HRESULT hr = pGetNdasLogicalUnit(location, &pNdasLogicalUnit);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"Logical device is not available. Already unmounted?\n");
			return;
		}

		NDAS_LOGICALDEVICE_STATUS status;
		COMVERIFY(pNdasLogicalUnit->get_Status(&status));

		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != status &&
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != status) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"Logical device is not mounted nor mount pending. Already unmounted?\n");
			return;
		}

		ACCESS_MASK mountedAccess;
		COMVERIFY(pNdasLogicalUnit->get_MountedAccess(&mountedAccess));
		AllowedAccess &= mountedAccess;
	}
	ACCESS_MASK AllowedAccess;
	BOOL NdasPort;
};

HRESULT
pNdasPortGetLogicalUnitAddress(
	__in LPCWSTR DeviceName,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

HRESULT
pNdasPortGetLogicalUnitAddress(
	__in HANDLE DeviceHandle,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

HRESULT 
pInspectDiskAbnormalities(
	__in HANDLE DiskHandle, 
	__out NDAS_LOGICALUNIT_ABNORMALITIES * Abnormalities);

HRESULT 
xSetupDiGetDeviceInterfaceDetail(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
	__deref_out PSP_DEVICE_INTERFACE_DETAIL_DATA* DeviceInterfaceDetailData,
	__in_opt PSP_DEVINFO_DATA DeviceInfoData);

HRESULT
pGetDevNodeConfigFlag(
	__in LPCTSTR DevicePath,
	__out DWORD* ConfigFlags);

HRESULT 
pGetDevNodeStatus(
	__in LPCTSTR DevicePath,
	__out PULONG Status,
	__out PULONG ProblemNumber);

} // end of namespace

HRESULT
NdasVolumeExtentsNdasLogicalUnit(
	__in HANDLE VolumeHandle,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

HRESULT
NdasVolumeExtentsNdasLogicalUnitByName(
	__in LPCWSTR VolumeName,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

//////////////////////////////////////////////////////////////////////////

CNdasServiceDeviceEventHandler::CNdasServiceDeviceEventHandler() :
	m_bInitialized(FALSE),
	m_bROFilterFilteringStarted(FALSE),
	m_hROFilter(INVALID_HANDLE_VALUE),
	m_bNoLfs(NdasServiceConfig::Get(nscDontUseWriteShare)),
	m_hRecipient(NULL),
	m_dwReceptionFlags(0),
	m_WorkQueueCount(0)
{
	//
	// Fill the drive set with null pointers
	//
	m_NdasLogicalUnitDrives.resize('Z'-'A'+1, 0);
}

CNdasServiceDeviceEventHandler::~CNdasServiceDeviceEventHandler()
{
	Uninitialize();
}

HRESULT
CNdasServiceDeviceEventHandler::Initialize(
	HANDLE hRecipient, DWORD dwReceptionFlag)
{
	HRESULT hr;

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
	// Register interested device interfaces
	//
	
	RegisterDeviceInterfaceNotification(
		&GUID_DEVINTERFACE_STORAGEPORT, "StoragePort");

	for (DWORD i = 0; i < RTL_NUMBER_OF(LogicalUnitInterfaceGuids); ++i)
	{
		RegisterDeviceInterfaceNotification(
			LogicalUnitInterfaceGuids[i].Guid,
			LogicalUnitInterfaceGuids[i].TypeName);
	}
	
	//
	// Register all existing logical units
	//

	hr = pRegisterNdasScsiPorts();

	hr = pRegisterLogicalUnits();

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
		return S_OK;
	}

	XTLASSERT(m_hROFilter.IsInvalid());

	//
	// Even if the rofilter is not loaded
	// initialization returns TRUE
	// However, m_hROFilter has INVALID_HANDLE_VALUE
	//

	m_bInitialized = TRUE;

	SERVICE_STATUS serviceStatus;
	BOOL success = NdasRoFilterQueryServiceStatus(&serviceStatus);
	if (!success) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"NdasRoFilterQueryServiceStatus failed, error=0x%X\n", GetLastError());
		return S_OK;
	}

	if (SERVICE_RUNNING != serviceStatus.dwCurrentState) 
	{
		success = NdasRoFilterStartService();
		if (!success) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
				"NdasRoFilterStartService failed, error=0x%X\n", GetLastError());
			return S_OK;
		}
	}

	HANDLE hROFilter = NdasRoFilterOpenDevice();
	if (INVALID_HANDLE_VALUE == hROFilter) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"NdasRoFilterCreate failed, error=0x%X\n", GetLastError());
		return S_OK;
	}

	m_hROFilter = hROFilter;

	return S_OK;
}

void
CNdasServiceDeviceEventHandler::OnShutdown()
{
	// Uninitialize();
}

void
CNdasServiceDeviceEventHandler::Uninitialize()
{
	//if (m_bROFilterFilteringStarted) 
	//{
	//	BOOL success = NdasRoFilterStopFilter(m_hROFilter);
	//	if (success) 
	//	{
	//		m_bROFilterFilteringStarted = FALSE;
	//	}
	//	else
	//	{
	//		XTLTRACE2_ERR(NDASSVC_PNP, TRACE_LEVEL_WARNING, "Failed to stop ROFilter session.\n");
	//		ret = false;
	//	}
	//}

	while (TRUE)
	{
		LONG count = InterlockedCompareExchange(&m_WorkQueueCount, 0, 0);
		if (0 == count)
			break;
		Sleep(0);
	}


	CAutoCritSec devNotifyMapLock(m_DevNotifyMapLock);

	std::for_each(
		m_DevNotifyMap.begin(),
		m_DevNotifyMap.end(),
		compose1(
			DevNotifyHandleCloser(), 
			select1st<DevNotifyMap::value_type>()));

	m_DevNotifyMap.clear();

	std::for_each(
		m_DeviceInterfaceNotifyHandles.begin(),
		m_DeviceInterfaceNotifyHandles.end(),
		DevNotifyHandleCloser());

	m_DeviceInterfaceNotifyHandles.clear();

}

void
CNdasServiceDeviceEventHandler::OnStoragePortDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"DeviceInterface Arrival: %ls\n", pdbhdr->dbcc_name);

	HRESULT hr;
	BOOL success = FALSE;
	DWORD slotNo = 0;

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
			pdbhdr->dbcc_name, &slotNo);

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
		return;
	}

	XTL::AutoFileHandle deviceHandle = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	if (deviceHandle.IsInvalid()) 
	{
		//
		// TODO: Set Logical Device Status to 
		// LDS_UNMOUNT_PENDING -> LDS_NOT_INITIALIZED (UNMOUNTED)
		// and Set LDS_ERROR? However, which one?
		//
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());
		return;
	}

	//
	// We got the LANSCSI miniport
	//

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"NDAS Logical Device Slot No: %d.\n", slotNo);

	//
	//	Register device handler notification to get remove-query and remove-failure.
	//  Windows system doesn't send a notification of remove-query and remove-failure
	//    of a device interface.
	//

	RegisterStoragePortHandleNotification(
		deviceHandle, 
		slotNo, 
		pdbhdr->dbcc_name);

	return;
}

HRESULT 
CNdasServiceDeviceEventHandler::RegisterDeviceInterfaceNotification(
	__in LPCGUID InterfaceGuid,
	__in LPCSTR TypeName)
{
	HDEVNOTIFY devNotifyHandle;

	HRESULT hr = pRegisterDeviceInterfaceNotification(
		m_hRecipient, 
		m_dwReceptionFlags,
		InterfaceGuid,
		&devNotifyHandle);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"RegisterDeviceInterfaceNotification failed, type=%hs, hr=0x%X\n",
			TypeName, hr);
		return hr;
	}

	try
	{
		m_DeviceInterfaceNotifyHandles.push_back(devNotifyHandle);
	}
	catch (...)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"RegisterDeviceInterfaceNotification failed, "
			"C++ exception, type=%hs\n",
			TypeName);			

		XTLVERIFY(UnregisterDeviceNotification(devNotifyHandle));

		return E_FAIL;
	}

	return S_OK;
}

HRESULT
CNdasServiceDeviceEventHandler::RegisterDeviceHandleNotification(
	__in HANDLE DeviceHandle, 
	__in DEVICE_HANDLE_TYPE HandleType,
	__in NDAS_LOCATION NdasLocation,
	__in LPCTSTR DevicePath)
{
	//
	// Register device handle first
	//

	HDEVNOTIFY devNotifyHandle;
	HRESULT hr = pRegisterDeviceHandleNotification(
		m_hRecipient, m_dwReceptionFlags, 
		DeviceHandle, &devNotifyHandle);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Registering device handle %p failed, hr=0x%X\n",
			DeviceHandle, hr);

		return hr;
	}

	//
	// Then, add the entry to the device notification map
	// to reference on its removal (OnRemoveComplete)
	//

	DEVICE_HANDLE_NOTIFY_DATA ndata;
	NDAS_LOCATION_DATA locationData;

	locationData.NdasLocation = NdasLocation;

	XTLVERIFY(SUCCEEDED(
		StringCchCopy(
			locationData.DevicePath,
			RTL_NUMBER_OF(locationData.DevicePath),
			DevicePath)));

	ndata.HandleType = HandleType;
	ndata.NdasLocation = NdasLocation;

	CAutoCritSec devNotifyMapLock(m_DevNotifyMapLock);

	std::pair<DevNotifyMap::iterator,bool> mi;

	try
	{
		mi = m_DevNotifyMap.insert(std::make_pair(devNotifyHandle, ndata));
	}
	catch (...)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"DevNotifyMap.insert() failed.\n");

		devNotifyMapLock.Release();

		XTLVERIFY( UnregisterDeviceNotification(devNotifyHandle) );

		return E_OUTOFMEMORY;
	}

	try
	{
		m_NdasLocationData.push_back(locationData);
	}
	catch (...)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"m_NdasLocationData.push_back() failed.\n");

		m_DevNotifyMap.erase(mi.first);

		devNotifyMapLock.Release();

		XTLVERIFY( UnregisterDeviceNotification(devNotifyHandle) );

		return E_OUTOFMEMORY;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"Device handle %p Notification Registered successfully.\n", 
		DeviceHandle);

	return S_OK;
}

HRESULT 
CNdasServiceDeviceEventHandler::RegisterVolumeHandleNotification(
	__in HANDLE DeviceHandle, 
	__in LPCTSTR DevicePath)
{
	return RegisterDeviceHandleNotification(
		DeviceHandle, PnpVolumeHandle, 0, DevicePath);
}

HRESULT 
CNdasServiceDeviceEventHandler::RegisterStoragePortHandleNotification(
	__in HANDLE DeviceHandle,
	__in NDAS_LOCATION NdasLocation,
	__in LPCTSTR DevicePath)
{
	return RegisterDeviceHandleNotification(
		DeviceHandle, PnpStoragePortHandle, NdasLocation, DevicePath);
}

HRESULT
CNdasServiceDeviceEventHandler::RegisterLogicalUnitHandleNotification(
	__in HANDLE DeviceHandle, 
	__in NDAS_LOCATION NdasLocation,
	__in LPCTSTR DevicePath)
{
	return RegisterDeviceHandleNotification(
		DeviceHandle, PnpLogicalUnitHandle, NdasLocation, DevicePath);
}

HRESULT 
CNdasServiceDeviceEventHandler::UnregisterDeviceHandleNotification(
	__in HDEVNOTIFY DevNotifyHandle)
{
	CAutoCritSec devNotifyMapLock(m_DevNotifyMapLock);

	DevNotifyMap::iterator itr = m_DevNotifyMap.find(DevNotifyHandle);
	if (m_DevNotifyMap.end() == itr)
	{
		return E_FAIL;
	}

	DEVICE_HANDLE_NOTIFY_DATA& ndata = itr->second;

	m_DevNotifyMap.erase(itr);

	devNotifyMapLock.Release();

	XTLVERIFY( UnregisterDeviceNotification(DevNotifyHandle) );

	return S_OK;
}

void
CNdasServiceDeviceEventHandler::OnVolumeDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	// Return TRUE to grant the request.
	// Return BROADCAST_QUERY_DENY to deny the request.

	BOOL success;

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"Volume=%ls\n", pdbhdr->dbcc_name);

	return;
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

	XTL::AutoFileHandle volumeHandle = ::CreateFile(
		(LPCTSTR)pdbhdr->dbcc_name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (volumeHandle.IsInvalid()) 
	{
		//
		// TODO: Set Logical Device Status to 
		// LDS_MOUNT_PENDING -> LDS_UNMOUNTED
		// and Set LDS_ERROR? However, which one?
		//
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());

		return;
	}

	RegisterVolumeHandleNotification(
		volumeHandle, reinterpret_cast<LPCTSTR>(pdbhdr->dbcc_name));

	return;

	HRESULT hr;

	NdasLocationVector ndasLocations;
	ndasLocations.reserve(1);
	hr = NdasEnumNdasLocationsForVolume(
		volumeHandle, 
		OnNdasVolume, 
		&ndasLocations);

	if (FAILED(hr)) 
	{    
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"NdasEnumNdasLocationsForVolume(hVolume %p) failed, error=0x%X\n", 
			volumeHandle, GetLastError());
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

		return;
	}

	//
	// For each NDAS Logical Devices, we should check granted-access
	//

	//
	// minimum access
	//

	EffectiveNdasAccess getEffectiveAccess;

	// Set NDAS port existence.
	getEffectiveAccess.NdasPort = IsNdasPortMode();

	std::for_each(
		ndasLocations.begin(),
		ndasLocations.end(),
		getEffectiveAccess);

	ACCESS_MASK effectiveAccess = getEffectiveAccess.AllowedAccess;

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"Volume has granted as 0x%08X (%hs %hs).\n", 
		effectiveAccess,
		(effectiveAccess & GENERIC_READ) ? "GENERIC_READ" : "",
		(effectiveAccess & GENERIC_WRITE) ?"GENERIC_WRITE" : "");
	
	//
	// Volume handle is no more required
	//
	volumeHandle.Release();

	if ((effectiveAccess & GENERIC_WRITE)) 
	{
		//
		// nothing to do for write-access
		//
	}
	else 
	{
	}

	return;
}

HRESULT 
CNdasServiceDeviceEventHandler::QueueRescanDriveLetters()
{
	ATLVERIFY(InterlockedIncrement(&m_WorkQueueCount) > 0);

	BOOL success = QueueUserWorkItem(
		RescanDriverLettersThreadStart, 
		this, 
		WT_EXECUTEDEFAULT);

	if (!success)
	{
		HRESULT hr = AtlHresultFromLastError();
		ATLVERIFY(InterlockedDecrement(&m_WorkQueueCount) >= 0);
		return hr;
	}

	return S_OK;
}

void
CNdasServiceDeviceEventHandler::RescanDriveLetters()
{
	//
	// Logical Drive Letters
	//
	DWORD unitMask = GetLogicalDrives();
	for (DWORD index = 'C' - 'A'; index < 'Z' - 'A' + 1; ++index)
	{
		if (unitMask & (1 << index))
		{
			TCHAR rootPath[32];
			COMVERIFY(StringCchPrintf(
				rootPath, RTL_NUMBER_OF(rootPath), 
				_T("%C:\\"), index + 'A'));
			UINT type = GetDriveType(rootPath);
			switch (type)
			{
			case DRIVE_REMOVABLE:
			case DRIVE_FIXED:
			case DRIVE_CDROM:
			case DRIVE_RAMDISK:
				// leave the drive letter
				break;
			default:
				unitMask &= ~(1 << index);
				// remove the drive letter
				break;
			}
		}
	}

	pOnVolumeArrivalOrRemoval(unitMask, FALSE);
}

void
CNdasServiceDeviceEventHandler::pOnVolumeArrivalOrRemoval(PDEV_BROADCAST_VOLUME dbcv, BOOL Removal)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
		"%s, UnitMask=%08X, Flags=%08X\n", 
		Removal ? "Removal" : "Arrival",
		dbcv->dbcv_unitmask, dbcv->dbcv_flags);

	//
	// We ignore the media or the network volume
	//
	if (0 != dbcv->dbcv_flags)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Ignored network volume or media\n");
		return;
	}

	pOnVolumeArrivalOrRemoval(dbcv->dbcv_unitmask, Removal);
}


void
CNdasServiceDeviceEventHandler::pOnVolumeArrivalOrRemoval(DWORD UnitMask, BOOL Removal)
{
	//
	// Guard against m_NdasLogicalUnitDrives
	//

	CAutoCritSec autolock(m_NdasLogicalUnitDrivesLock);

	for (int index = 0; index < ('Z' - 'A' + 1); ++index)
	{
		char driveLetter = 'A' + index;

		if (UnitMask & (1 << index))
		{
			if (Removal)
			{
				INdasLogicalUnit* pNdasLogicalUnit = m_NdasLogicalUnitDrives.at(index);
				if (!pNdasLogicalUnit)
				{
					XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
						"Not a registered NDAS drive letter(%C:).\n", driveLetter);
					continue;
				}

				NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;
				COMVERIFY(pNdasLogicalUnit->get_LogicalUnitAddress(&logicalUnitAddress));

				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
					"Dismounted is a volume of the NDAS logical unit, drive=%C:, address=%08X\n", 
					driveLetter, 
					logicalUnitAddress.Address);

				//
				// Remove the drive letter from the logical unit
				//
				CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
				pNdasLogicalUnitPnpSink->OnMountedDriveSetChanged(1 << index, 0);

				//
				// Remove from the letter mapping set
				//
				m_NdasLogicalUnitDrives.at(index) = 0;

				continue;
			}

			TCHAR volumeDeviceName[32];
			COMVERIFY(StringCchPrintf(
				volumeDeviceName, RTL_NUMBER_OF(volumeDeviceName), 
				_T("\\\\.\\%C:"), driveLetter));

			NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;
			HRESULT hr = NdasVolumeExtentsNdasLogicalUnitByName(
				volumeDeviceName, 
				&logicalUnitAddress);

			if (FAILED(hr))
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
					"NdasVolumeExtentsNdasLogicalUnitByName failed, volume=%ls, hr=0x%X\n", 
					volumeDeviceName, hr);
				continue;
			}

			NDAS_LOCATION loc = NdasLogicalUnitAddressToLocation(logicalUnitAddress);
			CComPtr<INdasLogicalUnit> pNdasLogicalUnit;
			hr = pGetNdasLogicalUnit(loc, &pNdasLogicalUnit);
			if (FAILED(hr))
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
					"pGetNdasLogicalDeviceByNdasLocation failed, address=%08X\n", 
					logicalUnitAddress.Address);
				continue;
			}

			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"Mounted is a volume of the NDAS logical unit, volume=%ls\n", 
				volumeDeviceName);

			//
			// Add the drive letter to the mounted drive set
			//
			CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
			pNdasLogicalUnitPnpSink->OnMountedDriveSetChanged(0, 1 << index);
			m_NdasLogicalUnitDrives.at(index) = pNdasLogicalUnit;
		}
	}
}

void 
CNdasServiceDeviceEventHandler::OnVolumeArrival(PDEV_BROADCAST_VOLUME dbcv)
{
	pOnVolumeArrivalOrRemoval(dbcv, FALSE);
}

void 
CNdasServiceDeviceEventHandler::OnVolumeRemoveComplete(PDEV_BROADCAST_VOLUME dbcv)
{
	pOnVolumeArrivalOrRemoval(dbcv, TRUE);
}

void
CNdasServiceDeviceEventHandler::OnLogicalUnitInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnLogicalUnitInterfaceArrival: %ls\n", pdbhdr->dbcc_name);

	HRESULT hr;

	//
	// As node config flags of the registry value may not be set 
	// by the PNP manager immediately, we cannot trust the value for now.
	//
#if 0

	DWORD nodeConfigFlags;

	hr = pGetDevNodeConfigFlag(pdbhdr->dbcc_name, &nodeConfigFlags);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"pGetDevNodeConfigFlag failed, hr=0x%X\n", hr);
	}
	else
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"pGetDevNodeConfigFlag: flags=0x%X\n", nodeConfigFlags);

		if ((CONFIGFLAG_REINSTALL | CONFIGFLAG_FINISH_INSTALL) & nodeConfigFlags)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"LogicalUnit device node will be reinstalled. Ignore arrival.\n");
			return;
		}
	}

#endif

	XTL::AutoFileHandle deviceHandle = ::CreateFile(
		reinterpret_cast<LPCTSTR>(pdbhdr->dbcc_name),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (deviceHandle.IsInvalid()) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, error=0x%X\n", 
			(LPCWSTR) pdbhdr->dbcc_name, GetLastError());
		return;
	}

	NDAS_LOCATION ndasLocation;

	if (IsNdasPortMode())
	{
		NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;

		hr = pNdasPortGetLogicalUnitAddress(
			deviceHandle, &logicalUnitAddress);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"NdasPortGetLogicalUnitAddress failed, hr=0x%X\n", hr);
			return;
		}

		ndasLocation = NdasLogicalUnitAddressToLocation(logicalUnitAddress);
	}
	else
	{
		hr = pGetNdasSlotNumberFromStorageDeviceHandle(
			deviceHandle, &ndasLocation);

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"pGetNdasSlotNumberFromStorageDeviceHandle failed, handle=%p, hr=0x%X\n", 
				deviceHandle, hr);
			return;
		}
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
		"NDAS Logical Device, ndasLocation=%08Xh.\n", 
		ndasLocation);

	//
	// we are to set the logical device status 
	// from MOUNT_PENDING -\> MOUNTED
	//

	//////////////////////////////////////////////////////////////////////
	// minimum access
	//////////////////////////////////////////////////////////////////////

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

	hr = pGetNdasLogicalUnit(ndasLocation, &pNdasLogicalUnit);
	
	if (FAILED(hr)) 
	{
		// unknown NDAS logical device
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"No Logical device found in LDM, ndasLocation=%d\n", 
			ndasLocation);
		return;
	}

	RegisterLogicalUnitHandleNotification(
		deviceHandle, 
		ndasLocation,
		pdbhdr->dbcc_name);

	if (IsEqualGUID(pdbhdr->dbcc_classguid, GUID_DEVINTERFACE_DISK))
	{
		NDAS_LOGICALUNIT_ABNORMALITIES abnormalities;
		hr = pInspectDiskAbnormalities(deviceHandle, &abnormalities);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"pInspectDiskAnormalities failed, hr=0x%X\n", hr);

			abnormalities = NDAS_LOGICALUNIT_ABNORM_NONE;
		}
		else if (abnormalities)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"Disk abnormalities detected, abf=%X\n", abnormalities);
		}

		//if (S_FALSE == hr)
		//{
		//	NDAS_LOGICALDEVICE_ID logicalDeviceId = pNdasLogicalUnit->GetLogicalDeviceId();
		//	CNdasEventPublisher& eventpub = pGetNdasEventPublisher();
		//	eventpub.RawDiskDetected(logicalDeviceId);
		//}

		CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
		pNdasLogicalUnitPnpSink->OnMounted(CComBSTR(pdbhdr->dbcc_name), abnormalities);
	}
	else
	{
		CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
		pNdasLogicalUnitPnpSink->OnMounted(CComBSTR(pdbhdr->dbcc_name), NDAS_LOGICALUNIT_ABNORM_NONE);
	}

	return;
}

LRESULT 
CNdasServiceDeviceEventHandler::OnDeviceHandleQueryRemove(
	PDEV_BROADCAST_HANDLE pdbch)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleQueryRemove: Device Handle %p, Notify Handle %p\n", 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);

	return TRUE;
}

void
CNdasServiceDeviceEventHandler::OnDeviceHandleQueryRemoveFailed(
	PDEV_BROADCAST_HANDLE pdbch)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleQueryRemoveFailed: Device Handle %p, Notify Handle %p\n", 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);

	const DEVICE_HANDLE_NOTIFY_DATA* notifyData = 
		GetDeviceHandleNotificationData(pdbch->dbch_hdevnotify);

	if (NULL == notifyData)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"Notify Handle is not registered.\n");
		return;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"DeviceHandleQueryRemoveFailed, ndasLocation=%08X\n", 
		notifyData->NdasLocation);

	//
	// DEVNOTIFYINFO_TYPE_STORAGEPORT is the only notification we are 
	// interested in. However, when a CDROM device attached to the storage port,
	// the storage port device notification for QueryRemoveFailed is not
	// sent to us. So, we also need to handle CD-ROM.
	// 
	// Do not worry about redundant calls to pNdasLogicalUnit->OnUnmountFailed().
	// It only cares if NDAS_LOGICALDEVICE_STATUS is UNMOUNT_PENDING,
	// which means backup calls will be ignored.
	//

	CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

	HRESULT hr = pGetNdasLogicalUnit(notifyData->NdasLocation, &pNdasLogicalUnit);

	if (FAILED(hr)) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Logical Device not found, ndasLocation=%08X.\n", 
			notifyData->NdasLocation);
		return;
	}

	CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
	pNdasLogicalUnitPnpSink->OnDismountFailed();
}

void 
CNdasServiceDeviceEventHandler::OnDeviceHandleRemovePending(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// RemoveComplete will handle the completion of the removal
	//
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleRemovePending: Device Handle %p, Notify Handle %p\n", 
		pdbch->dbch_handle,
		pdbch->dbch_hdevnotify);
}


void 
CNdasServiceDeviceEventHandler::OnDeviceHandleRemoveComplete(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// Device Handle Remove Complete is called on Surprise Removal
	//

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleRemoveComplete: Device Handle %p, Notify Handle %p\n",
		pdbch->dbch_handle, pdbch->dbch_hdevnotify);

	const DEVICE_HANDLE_NOTIFY_DATA* notifyData =
		GetDeviceHandleNotificationData(pdbch->dbch_hdevnotify);

	if (NULL == notifyData)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"Notify Handle is not registered.\n");
		return;
	}

	//
	// Copy the ndasLocation as we cannot access 
	// notifyData after UnregisterDeviceHandleNotification
	//

	switch (notifyData->HandleType)
	{
	case PnpLogicalUnitHandle:
	case PnpStoragePortHandle:

		{
			NDAS_LOCATION ndasLocation = notifyData->NdasLocation;

			UnregisterDeviceHandleNotification(pdbch->dbch_hdevnotify);

			CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

			HRESULT hr = pGetNdasLogicalUnit(ndasLocation, &pNdasLogicalUnit);

			if (FAILED(hr))
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
					"Logical Device not found, ndasLocation=%08X.\n", 
					ndasLocation);
				return;
			}

			CComQIPtr<INdasLogicalUnitPnpSink> pNdasLogicalUnitPnpSink(pNdasLogicalUnit);
			pNdasLogicalUnitPnpSink->OnDismounted();

		}
		break;

	case PnpVolumeHandle:
		{
			UnregisterDeviceHandleNotification(pdbch->dbch_hdevnotify);

			// m_VolumePathDetector.QueueVolumePathDetection();
		}
		break;

	default:
		;
	}
}

void
CNdasServiceDeviceEventHandler::OnDeviceHandleCustomEvent(
	PDEV_BROADCAST_HANDLE pdbch)
{
	//
	// Device Handle Remove Complete is called on Surprise Removal
	//
	XTLASSERT(m_bInitialized);

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceHandleCustomEvent: Device Handle %p, Notify Handle %p\n",
		pdbch->dbch_handle, pdbch->dbch_hdevnotify);

	const DEVICE_HANDLE_NOTIFY_DATA* notifyData = 
		GetDeviceHandleNotificationData(pdbch->dbch_hdevnotify);

	if (NULL == notifyData)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"Notify Handle is not registered.\n");
		return;
	}

	if (IsEqualGUID(NDAS_DLU_EVENT_GUID, pdbch->dbch_eventguid))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
			"NDAS_DLU_EVENT arrived, ndasLocation=0x%08X\n", 
			notifyData->NdasLocation);

		CComPtr<INdasLogicalUnit> pNdasLogicalUnit;

		HRESULT hr = pGetNdasLogicalUnit(notifyData->NdasLocation, &pNdasLogicalUnit);

		if (FAILED(hr)) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"Logical Device not found, ndasLocation=%d.\n", 
				notifyData->NdasLocation);
			return;
		}

		PNDAS_DLU_EVENT dluEvent = reinterpret_cast<PNDAS_DLU_EVENT>(pdbch->dbch_data);
		//
		// Send the custom event to the event monitor.
		//
		CNdasEventMonitor& emon = pGetNdasEventMonitor();
		emon.OnLogicalDeviceAlarmedByPnP(pNdasLogicalUnit, dluEvent->DluInternalStatus);
	}
	else if (IsEqualGUID(GUID_IO_VOLUME_NAME_CHANGE, pdbch->dbch_eventguid))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"IoEvent - IO_VOLUME_NAME_CHANGE\n");

		// m_VolumePathDetector.QueueVolumePathDetection();
	}
	else if (IsEqualGUID(GUID_IO_VOLUME_MOUNT, pdbch->dbch_eventguid))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"IoEvent - IO_VOLUME_MOUNT, nameoffset=%d, data=%08X\n",
			pdbch->dbch_nameoffset,
			*(LPDWORD)pdbch->dbch_data);

		PDEVICE_EVENT_MOUNT mountData = 
			reinterpret_cast<PDEVICE_EVENT_MOUNT>(pdbch->dbch_data);

		// m_VolumePathDetector.QueueVolumePathDetection();
	}
	else
	{
		struct {
			const GUID& IoEventGuid;
			LPCTSTR EventName;
		} IoEventTable[] = {
			GUID_IO_VOLUME_CHANGE, _T("IO_VOLUME_CHANGE"),
			GUID_IO_VOLUME_DISMOUNT, _T("IO_VOLUME_DISMOUNT"),
			GUID_IO_VOLUME_DISMOUNT_FAILED, _T("VOLUME_DISMOUNT_FAILED"),
			GUID_IO_VOLUME_MOUNT, _T("VOLUME_MOUNT"),
			GUID_IO_VOLUME_LOCK, _T("VOLUME_LOCK"),
			GUID_IO_VOLUME_LOCK_FAILED, _T("VOLUME_LOCK_FAILED"),
			GUID_IO_VOLUME_UNLOCK, _T("VOLUME_UNLOCK"),
			GUID_IO_VOLUME_NAME_CHANGE, _T("VOLUME_NAME_CHANGE"),
			GUID_IO_VOLUME_NEED_CHKDSK, _T("VOLUME_NEED_CHKDSK"),
			GUID_IO_VOLUME_WORM_NEAR_FULL, _T("VOLUME_WORM_NEAR_FULL"),
			GUID_IO_VOLUME_WEARING_OUT, _T("VOLUME_WEARING_OUT"),
			GUID_IO_VOLUME_FORCE_CLOSED, _T("VOLUME_FORCE_CLOSED"),
			GUID_IO_VOLUME_INFO_MAKE_COMPAT, _T("VOLUME_INFO_MAKE_COMPAT"),
			GUID_IO_VOLUME_PREPARING_EJECT, _T("VOLUME_PREPARING_EJECT"),
			GUID_IO_VOLUME_PHYSICAL_CONFIGURATION_CHANGE, _T("VOLUME_PHYSICAL_CONFIGURATION_CHANGE"),
			GUID_IO_VOLUME_FVE_STATUS_CHANGE, _T("VOLUME_FVE_STATUS_CHANGE"),
			GUID_IO_VOLUME_DEVICE_INTERFACE, _T("VOLUME_DEVICE_INTERFACE"),
			GUID_IO_VOLUME_CHANGE_SIZE, _T("VOLUME_CHANGE_SIZE"),
			GUID_IO_MEDIA_ARRIVAL, _T("MEDIA_ARRIVAL"),
			GUID_IO_MEDIA_REMOVAL, _T("MEDIA_REMOVAL"),
			GUID_IO_CDROM_EXCLUSIVE_LOCK, _T("CDROM_EXCLUSIVE_LOCK"),
			GUID_IO_CDROM_EXCLUSIVE_UNLOCK, _T("CDROM_EXCLUSIVE_UNLOCK"),
			GUID_IO_DEVICE_BECOMING_READY, _T("DEVICE_BECOMING_READY"),
			GUID_IO_DEVICE_EXTERNAL_REQUEST, _T("DEVICE_EXTERNAL_REQUEST"),
			GUID_IO_MEDIA_EJECT_REQUEST, _T("MEDIA_EJECT_REQUEST"),
			GUID_IO_DRIVE_REQUIRES_CLEANING, _T("DRIVE_REQUIRES_CLEANING"),
			GUID_IO_TAPE_ERASE, _T("TAPE_ERASE"),
			GUID_DEVICE_EVENT_RBC, _T("DEVICE_EVENT_RBC"),
			GUID_IO_DISK_CLONE_ARRIVAL, _T("DISK_CLONE_ARRIVAL"),
			GUID_IO_DISK_LAYOUT_CHANGE, _T("DISK_LAYOUT_CHANGE"),
		};

		for (int i = 0; i < RTL_NUMBER_OF(IoEventTable); ++i)
		{
			if (IsEqualGUID(IoEventTable[i].IoEventGuid, pdbch->dbch_eventguid))
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
					"IoEvent - %ls\n", IoEventTable[i].EventName);
				break;
			}
		}
	}
}

void
CNdasServiceDeviceEventHandler::OnDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE dbcc)
{
	XTLASSERT(m_bInitialized);

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnDeviceInterfaceArrival\n");

	if (IsEqualGUID(GUID_DEVINTERFACE_STORAGEPORT, dbcc->dbcc_classguid))
	{
		OnStoragePortDeviceInterfaceArrival(dbcc);
	}
	else if (IsEqualGUID(GUID_DEVINTERFACE_VOLUME, dbcc->dbcc_classguid))
	{
		OnVolumeDeviceInterfaceArrival(dbcc);
	}
	else
	{
		OnLogicalUnitInterfaceArrival(dbcc);
	}
}

const DEVICE_HANDLE_NOTIFY_DATA*
CNdasServiceDeviceEventHandler::GetDeviceHandleNotificationData(
	HDEVNOTIFY NotifyHandle)
{
	const DEVICE_HANDLE_NOTIFY_DATA* NotifyData;

	std::map<HDEVNOTIFY,DEVICE_HANDLE_NOTIFY_DATA>::const_iterator itr = 
		m_DevNotifyMap.find(NotifyHandle);
	
	if (m_DevNotifyMap.end() == itr) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Device notify handle %p is not registered.\n", 
			NotifyHandle);

		return NULL;
	}

	NotifyData = &itr->second;

	return NotifyData;
}

HRESULT 
CNdasServiceDeviceEventHandler::pRegisterNdasScsiPorts()
{
	HRESULT hr = S_OK;
	HDEVINFO hDevInfoSet = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_STORAGEPORT,
		NULL,
		NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (static_cast<HDEVINFO>(INVALID_HANDLE_VALUE) == hDevInfoSet)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"SetupDiCreateDeviceInfoList failed, hr=%x\n", hr);
		return hr;
	}

	for (DWORD index = 0; ; ++index)
	{
		SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { sizeof(SP_DEVICE_INTERFACE_DATA) };

		BOOL success = SetupDiEnumDeviceInterfaces(
			hDevInfoSet, 
			NULL,
			&GUID_DEVINTERFACE_STORAGEPORT,
			index,
			&deviceInterfaceData);

		if (!success)
		{
			if (ERROR_NO_MORE_ITEMS != GetLastError())
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"SetupDiEnumDeviceInterfaces failed, hr=%X\n", hr);
			}
			break;
		}

		DWORD requiredSize = 0;
		success = SetupDiGetDeviceInterfaceDetail(
			hDevInfoSet,
			&deviceInterfaceData,
			NULL,
			0,
			&requiredSize,
			NULL);

		if (success || ERROR_INSUFFICIENT_BUFFER != GetLastError())
		{
			if (success)
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"SetupDiGetDeviceInterfaceDetail failed, no interface details\n");
			}
			else
			{
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"SetupDiGetDeviceInterfaceDetail failed, error=0x%X\n", GetLastError());
			}
			continue;
		}

		PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = 
			static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(requiredSize));

		if (NULL == deviceInterfaceDetailData)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"malloc failed, bytes=%d\n", requiredSize);
			continue;
		}

		ZeroMemory(deviceInterfaceDetailData, requiredSize);
		deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		success = SetupDiGetDeviceInterfaceDetail(
			hDevInfoSet,
			&deviceInterfaceData,
			deviceInterfaceDetailData,
			requiredSize,
			NULL,
			NULL);

		if (!success)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"SetupDiGetDeviceInterfaceDetail failed, error=0x%X\n", GetLastError());

			free(deviceInterfaceDetailData);

			continue;
		}

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
			"StoragePort found, device=%ls\n", deviceInterfaceDetailData->DevicePath);

		HANDLE hDevice = CreateFile(
			deviceInterfaceDetailData->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_DEVICE,
			NULL);

		if (INVALID_HANDLE_VALUE == hDevice)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"CreateFile failed, device=%ls, error=0x%X\n",
				deviceInterfaceDetailData->DevicePath, GetLastError());

			free(deviceInterfaceDetailData);

			continue;
		}

		DWORD slotNo = 0;
		HRESULT hr2 = pGetNdasSlotNumberFromDeviceHandle(hDevice, &slotNo);

		if (S_OK != hr2)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"StoragePort is not an NDAS Storage Port, hr=%x\n", hr2);

			XTLVERIFY( CloseHandle(hDevice) );
			free(deviceInterfaceDetailData);

			continue;
		}

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
			"NdasSlotNumber=%d\n", slotNo);

		//
		// As the ownership of the handler goes to the vector,
		// do not close the device handle here
		//

		RegisterStoragePortHandleNotification(
			hDevice, 
			slotNo,
			deviceInterfaceDetailData->DevicePath);

		XTLVERIFY( CloseHandle(hDevice) );
		free(deviceInterfaceDetailData);
	}

	XTLVERIFY( SetupDiDestroyDeviceInfoList(hDevInfoSet) );

	return hr;
}


HRESULT
CNdasServiceDeviceEventHandler::pRegisterLogicalUnits()
{
	HRESULT hr;

	HDEVINFO devInfoSet = SetupDiGetClassDevsW(
		NULL,
		NULL,
		NULL,
		DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"SetupDiGetClassDevsW failed, hr=0x%X\n", hr);
		return hr;
	}

	SP_DEVINFO_DATA devInfoData;

	for (DWORD index = 0; ; ++index)
	{
		devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		BOOL success = SetupDiEnumDeviceInfo(devInfoSet, index, &devInfoData);
		if (!success)
		{
			if (ERROR_NO_MORE_ITEMS != GetLastError())
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"SetupDiEnumDeviceInfo failed, hr=0x%X\n", hr);
			}
			break;
		}
		devInfoData.ClassGuid;
	}

	const struct {
		LPCGUID Guid;
		LPCSTR TypeName;
	} LogicalUnitInterfaces[] = {
		&GUID_DEVINTERFACE_DISK, "DISK",
		&GUID_DEVINTERFACE_CDROM, "CDROM",
		&GUID_DEVINTERFACE_TAPE, "TAPE",
		&GUID_DEVINTERFACE_WRITEONCEDISK, "WRITEONCEDISK",
		&GUID_DEVINTERFACE_MEDIUMCHANGER, "MEDIUMCHANGER",
		&GUID_DEVINTERFACE_CDCHANGER, "CDCHANGER",
	};

	for (DWORD i = 0; i < RTL_NUMBER_OF(LogicalUnitInterfaces); ++i)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"Enumerating %hs...\n", LogicalUnitInterfaces[i].TypeName);

		pRegisterLogicalUnit(
			devInfoSet, 
			LogicalUnitInterfaces[i].Guid);
	}

	XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );

	return S_OK;
}

HRESULT
CNdasServiceDeviceEventHandler::pRegisterLogicalUnit(
	__in HDEVINFO DevInfoSet,
	__in LPCGUID InterfaceGuid)
{
	HRESULT hr;
	BOOL success;
	DWORD index = 0;
	SP_DEVICE_INTERFACE_DATA devIntfData;

	DWORD devIntfDetailSize = 
		offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA, DevicePath) + 
		sizeof(TCHAR) * MAX_PATH;

	PSP_DEVICE_INTERFACE_DETAIL_DATA devIntfDetail = 
		static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(devIntfDetailSize));

	if (NULL == devIntfDetail)
	{
		hr = E_OUTOFMEMORY;
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Memory allocation failed, size=0x%X\n", devIntfDetailSize);
		return hr;
	}

	for (DWORD index = 0; ; ++index)
	{
		devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		
		success = SetupDiEnumDeviceInterfaces(
			DevInfoSet,
			NULL,
			InterfaceGuid,
			index,
			&devIntfData);

		if (!success)
		{
			if (ERROR_NO_MORE_ITEMS == GetLastError())
			{
				if (0 == index) 
				{
					XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
						"No devices\n");
				}
				hr = S_OK;
			}
			else
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"SetupDiEnumDeviceInterfaces failed, hr=0x%X\n", hr);
			}
			break;
		}

		devIntfDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		success = SetupDiGetDeviceInterfaceDetail(
			DevInfoSet,
			&devIntfData,
			devIntfDetail,
			devIntfDetailSize,
			&devIntfDetailSize,
			NULL);

		if (!success)
		{
			if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
			{
				hr = HRESULT_FROM_SETUPAPI(GetLastError());
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"SetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
				continue;
			}

			PVOID p = realloc(devIntfDetail, devIntfDetailSize);

			if (NULL == p)
			{
				hr = E_OUTOFMEMORY;
				XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
					"Memory allocation failed, size=0x%X\n", devIntfDetailSize);
				continue;
			}

			devIntfDetail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(p);

			success = SetupDiGetDeviceInterfaceDetail(
				DevInfoSet,
				&devIntfData,
				devIntfDetail,
				devIntfDetailSize,
				&devIntfDetailSize,
				NULL);
		}

		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"SetupDiGetDeviceInterfaceDetail2 failed, hr=0x%X\n", hr);
			continue;
		}

		//
		// Now we have devIntfDetail->DevicePath
		//
		HANDLE h = CreateFile(
			devIntfDetail->DevicePath,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_DEVICE,
			NULL);

		if (INVALID_HANDLE_VALUE == h)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"CreateFile(%ls) failed, hr=0x%X\n", 
				devIntfDetail->DevicePath, hr);
			continue;
		}

		NDAS_LOGICALUNIT_ADDRESS luAddress;

		hr = pNdasPortGetLogicalUnitAddress(h, &luAddress);

		if (FAILED(hr))
		{
			XTLVERIFY( CloseHandle(h) );
			continue;
		}

		NDAS_LOCATION ndasLocation = 
			NdasLogicalUnitAddressToLocation(luAddress);

		RegisterLogicalUnitHandleNotification(
			h, 
			ndasLocation,
			devIntfDetail->DevicePath);

		XTLVERIFY( CloseHandle(h) );
	}

	free(devIntfDetail);
	return S_OK;
}

struct NdasLocationDataFinder :
	std::unary_function<const NDAS_LOCATION_DATA&,bool>
{
	NdasLocationDataFinder(NDAS_LOCATION NdasLocation) :
		NdasLocation(NdasLocation)
	{
	}
	bool operator()(const NDAS_LOCATION_DATA& LocationData) const
	{
		return NdasLocation == LocationData.NdasLocation;
	}
	NDAS_LOCATION NdasLocation;
};

HRESULT
CNdasServiceDeviceEventHandler::GetLogicalUnitDevicePath(
	__in NDAS_LOCATION NdasLocation,
	__out BSTR* DevicePath)
{
	HRESULT hr;

	CAutoCritSec devNotifyMapLock(m_DevNotifyMapLock);

	std::vector<NDAS_LOCATION_DATA>::const_iterator itr = std::find_if(
		m_NdasLocationData.begin(),
		m_NdasLocationData.end(),
		NdasLocationDataFinder(NdasLocation));

	if (itr == m_NdasLocationData.end())
	{
		return HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS);
	}

	CComBSTR bstr(itr->DevicePath);
	hr = bstr.CopyTo(DevicePath);

	return hr;
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
		BOOL success = ::GetVersionEx((OSVERSIONINFO*) &osvi);
		XTLASSERT(success);
		if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0) 
		{
			bWindows2000 = true;
		}
		bHandled = true;
	}

	return bWindows2000;
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
	BOOL success = sysutil::GetStorageHotplugInfo(
		hDisk, 
		&bMediaRemovable, 
		&bMediaHotplug, 
		&bDeviceHotplug);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"GetStorageHotplugInfo failed, error=0x%X\n", GetLastError());
		return;
	} 

	bDeviceHotplug = TRUE;
	success = sysutil::SetStorageHotplugInfo(
		hDisk,
		bMediaRemovable,
		bMediaHotplug,
		bDeviceHotplug);

	if (!success) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"StorageHotplugInfo failed, error=0x%X\n", GetLastError());
	}

	return;
}

HRESULT
pRegisterDeviceInterfaceNotification(
	__in HANDLE RecipientHandle,
	__in DWORD ReceptionFlags,
	__in LPCGUID InterfaceGuid,
	__out HDEVNOTIFY* DevNotifyHandle)
{
	HRESULT hr;

	*DevNotifyHandle = NULL;

	DEV_BROADCAST_DEVICEINTERFACE filter = {0};
	filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	filter.dbcc_classguid = *InterfaceGuid;

	HDEVNOTIFY h = RegisterDeviceNotification(
		RecipientHandle, &filter, ReceptionFlags);

	if (NULL == h)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"RegisterDeviceNotification failed, INTERFACE, hr=0x%X\n", 
			hr);
		return hr;
	}

	*DevNotifyHandle = h;

	return S_OK;
}

HRESULT
pRegisterDeviceHandleNotification(
	__in HANDLE RecipientHandle,
	__in DWORD ReceptionFlags,
	__in HANDLE DeviceHandle,
	__out HDEVNOTIFY* DevNotifyHandle)
{
	HRESULT hr;

	*DevNotifyHandle = NULL;

	DEV_BROADCAST_HANDLE filter = {0};
	filter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
	filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
	filter.dbch_handle = DeviceHandle;

	HDEVNOTIFY h = RegisterDeviceNotification(
		RecipientHandle, &filter, ReceptionFlags);

	if (NULL == h)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"RegisterDeviceNotification failed, hDevice=%p, hr=0x%X\n", 
			DeviceHandle, hr);
		return hr;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION,
		"Registered DeviceHandle notification, hDevice=%p, hNotify=%p\n",
		DeviceHandle, h);

	*DevNotifyHandle = h;

	return S_OK;
}

HRESULT
pNdasPortGetLogicalUnitAddress(
	__in LPCWSTR DeviceName,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	HRESULT hr;

	HANDLE h = CreateFileW(
		DeviceName,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	if (INVALID_HANDLE_VALUE == h)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"CreateFile(%ls) failed, hr=0x%X\n", 
			DeviceName, hr);

		return hr;
	}

	hr = pNdasPortGetLogicalUnitAddress(h, LogicalUnitAddress);

	XTLVERIFY( CloseHandle(h) );

	return hr;
}

HRESULT
pNdasPortGetLogicalUnitAddress(
	__in HANDLE DeviceHandle,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	HRESULT hr;
	DWORD byteReturned;

	BOOL success = DeviceIoControl(
		DeviceHandle,
		IOCTL_NDASPORT_LOGICALUNIT_GET_ADDRESS,
		NULL, 0,
		LogicalUnitAddress, 
		sizeof(NDAS_LOGICALUNIT_ADDRESS),
		&byteReturned,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	return S_OK;
}

HRESULT
pGetNdasPortHandle(__out HANDLE* NdasportHandle)
{
	HRESULT hr;

	*NdasportHandle = INVALID_HANDLE_VALUE;

	HDEVINFO devInfoSet = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_NDASPORT,
		NULL,
		NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiGetClassDevs failed, hr=0x%X\n", hr);
		return hr;
	}

	SP_DEVICE_INTERFACE_DATA devIntfData;
	devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL success = SetupDiEnumDeviceInterfaces(
		devInfoSet,
		NULL,
		&GUID_DEVINTERFACE_NDASPORT,
		0,
		&devIntfData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiEnumDeviceInterfaces failed, hr=0x%X\n", hr);
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	DWORD devIntfDetailSize = offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA, DevicePath) + 
		sizeof(TCHAR) * MAX_PATH;
	
	PSP_DEVICE_INTERFACE_DETAIL_DATA devIntfDetail = 
		static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(devIntfDetailSize));

	if (NULL == devIntfDetail)
	{
		hr = E_OUTOFMEMORY;
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"Memory allocation failed, size=0x%X\n", devIntfDetailSize);
		SetupDiDestroyDeviceInfoList(devInfoSet);
		return hr;
	}

	devIntfDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	success = SetupDiGetDeviceInterfaceDetail(
		devInfoSet,
		&devIntfData,
		devIntfDetail,
		devIntfDetailSize,
		&devIntfDetailSize,
		NULL);

	if (!success)
	{
		if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
				"SetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
			free(devIntfDetail);
			XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );
			return hr;
		}

		PVOID p = realloc(devIntfDetail, devIntfDetailSize);

		if (NULL == p)
		{
			hr = E_OUTOFMEMORY;
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
				"Memory allocation failed, size=0x%X\n", devIntfDetailSize);
			free(devIntfDetail);
			XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );
			return hr;
		}

		devIntfDetail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(p);

		success = SetupDiGetDeviceInterfaceDetail(
			devInfoSet,
			&devIntfData,
			devIntfDetail,
			devIntfDetailSize,
			&devIntfDetailSize,
			NULL);
	}

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiGetDeviceInterfaceDetail2 failed, hr=0x%X\n", hr);
		free(devIntfDetail);
		XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );
		return hr;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		" NdasPort DevicePath=%ls\n", devIntfDetail->DevicePath);

	HANDLE h = CreateFile(
		devIntfDetail->DevicePath,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_DEVICE,
		NULL);

	free(devIntfDetail);
	XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );

	if (INVALID_HANDLE_VALUE == h)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CreateFile(%ls) failed, hr=0x%X\n", 
			devIntfDetail->DevicePath, hr);
		return hr;
	}

	*NdasportHandle = h;
	return S_OK;
}

HRESULT 
pInspectDiskAbnormalities(
	__in HANDLE DiskHandle, 
	__out NDAS_LOGICALUNIT_ABNORMALITIES * Abnormalities)
{
	BOOL diskIsInitialized = TRUE;
	PDRIVE_LAYOUT_INFORMATION_EX driveLayoutEx = NULL;
	HRESULT hr = SdiDiskGetDriveLayoutEx(DiskHandle, &driveLayoutEx);

	XTLASSERT(NULL != Abnormalities);
	*Abnormalities = NDAS_LOGICALUNIT_ABNORM_NONE;

	if (FAILED(hr))
	{
		//
		// Windows 2000 does not support SdiDiskGetDriveLayoutEx
		//

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"DiskGetDriveLayoutEx failed, hr=0x%X\n", hr);

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"Using DiskGetDriveLayout...\n");

		PDRIVE_LAYOUT_INFORMATION driveLayout = NULL;

		hr = SdiDiskGetDriveLayout(DiskHandle, &driveLayout);

		if (FAILED(hr))
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
				"DiskGetDriveLayout failed, hr=0x%X\n", hr);

			return hr;
		}

		//
		// MBR partition count is mostly 4.
		// Do not rely on this for finding uninitialized state.
		//

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"Partition Count=%d, Signature=%08X\n", 
			driveLayout->PartitionCount,
			driveLayout->Signature);

		if (0 == driveLayout->Signature)
		{
			*Abnormalities = NDAS_LOGICALUNIT_ABNORM_DISK_NOT_INITIALIZED;
			free(driveLayout);
			return S_OK;
		}

		diskIsInitialized = FALSE;

#ifdef _DEBUG
		for (DWORD i = 0; i < driveLayout->PartitionCount; ++i)
		{
			PPARTITION_INFORMATION partInfo = &driveLayout->PartitionEntry[i];

			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				" %d: StartingOffset=0x%I64X, Length=0x%I64X, "
				"HiddenSectors=0x%X, PartitionNumber=%d, Type=0x%02X, "
				"Boot=%d, Recognized=%d, Rewrite=%d\n", 
				i,
				partInfo->StartingOffset,
				partInfo->PartitionLength,
				partInfo->HiddenSectors,
				partInfo->PartitionNumber,
				partInfo->PartitionType,
				partInfo->BootIndicator,
				partInfo->RecognizedPartition,
				partInfo->RewritePartition);

			if (0 != partInfo->PartitionNumber && partInfo->RecognizedPartition)
			{
				diskIsInitialized = TRUE;
			}
		}
#endif

		for (DWORD i = 0; i < driveLayout->PartitionCount; ++i)
		{
			PPARTITION_INFORMATION partInfo = &driveLayout->PartitionEntry[i];
			//
			// Partition Number is 1-based index
			// Zero (0) is an invalid entry.
			//
			if (0 != partInfo->PartitionNumber && partInfo->RecognizedPartition)
			{
				//
				// Dynamic disk detection
				//
				if (PARTITION_LDM == partInfo->PartitionType)
				{
					free(driveLayout);
					*Abnormalities = NDAS_LOGICALUNIT_ABNORM_DYNAMIC_DISK;
					return S_OK;
				}
				diskIsInitialized = TRUE;
				break;
			}
		}

		free(driveLayout);

		if (!diskIsInitialized)
		{
			*Abnormalities = NDAS_LOGICALUNIT_ABNORM_DISK_WITH_NO_DATA_PARTITION;
		}

		return S_OK;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
		"Partition Style=%d, Count=%d\n", 
		driveLayoutEx->PartitionStyle,
		driveLayoutEx->PartitionCount);

	if (PARTITION_STYLE_MBR == driveLayoutEx->PartitionStyle &&
		0 == driveLayoutEx->PartitionCount)
	{
		*Abnormalities = NDAS_LOGICALUNIT_ABNORM_DISK_NOT_INITIALIZED;
		free(driveLayoutEx);
		return S_OK;
	}

#ifdef _DEBUG
	switch (driveLayoutEx->PartitionStyle)
	{
	case PARTITION_STYLE_MBR:
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			" MBR Signature: %08X\n", driveLayoutEx->Mbr.Signature);
		break;
	case PARTITION_STYLE_GPT:
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			" GPT DiskId={%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}, "
			"StartingUsableOffset=0x%I64X, UsableLength=0x%I64X, "
			"MaxPartitionCount=%d\n",
			driveLayoutEx->Gpt.DiskId.Data1,
			driveLayoutEx->Gpt.DiskId.Data2,
			driveLayoutEx->Gpt.DiskId.Data3,
			driveLayoutEx->Gpt.DiskId.Data4[0],
			driveLayoutEx->Gpt.DiskId.Data4[1],
			driveLayoutEx->Gpt.DiskId.Data4[2],
			driveLayoutEx->Gpt.DiskId.Data4[3],
			driveLayoutEx->Gpt.DiskId.Data4[4],
			driveLayoutEx->Gpt.DiskId.Data4[5],
			driveLayoutEx->Gpt.DiskId.Data4[6],
			driveLayoutEx->Gpt.DiskId.Data4[7],
			driveLayoutEx->Gpt.StartingUsableOffset,
			driveLayoutEx->Gpt.UsableLength,
			driveLayoutEx->Gpt.MaxPartitionCount);
		break;
	case PARTITION_STYLE_RAW:
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			" RAW Partition\n");
		break;
	default:
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			" Unrecognized partition style\n");
	}

	for (DWORD i = 0; i < driveLayoutEx->PartitionCount; ++i)
	{
		PPARTITION_INFORMATION_EX part = &driveLayoutEx->PartitionEntry[i];

		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			" %d: Style=%d, StartingOffset=0x%I64X, Length=0x%I64X, "
			"PartitionNumber=%d, Rewrite=%d\n", 
			i,
			part->PartitionStyle,
			part->StartingOffset,
			part->PartitionLength,
			part->PartitionNumber,
			part->RewritePartition);

		switch (part->PartitionStyle)
		{
		case PARTITION_STYLE_MBR:
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"    MBR Type=%02X, Boot=%d, "
				"Recognized=%d, HiddenSectors=0x%X\n",
				part->Mbr.PartitionType,
				part->Mbr.BootIndicator,
				part->Mbr.RecognizedPartition,
				part->Mbr.HiddenSectors);
			break;
		case PARTITION_STYLE_GPT:
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"    GPT Attrs=%I64X, Name=%ls\n",
				// part->Gpt.PartitionType,
				// part->Gpt.PartitionId,
				part->Gpt.Attributes,
				part->Gpt.Name);
			break;
		}
	}
#endif

	// Partition Style=0, Count=4
	//  MBR Signature: 31640DE2
	//  0: Style=0, StartingOffset=0x100000, Length=0x3A38700000, PartitionNumber=1, Rewrite=0
	//     MBR Type=06, Boot=0, Recognized=1, HiddenSectors=0x800
	//  1: Style=0, StartingOffset=0x0, Length=0x0, PartitionNumber=0, Rewrite=0
	//     MBR Type=00, Boot=0, Recognized=0, HiddenSectors=0x0
	//  2: Style=0, StartingOffset=0x0, Length=0x0, PartitionNumber=0, Rewrite=0
	//     MBR Type=00, Boot=0, Recognized=0, HiddenSectors=0x0
	//  3: Style=0, StartingOffset=0x0, Length=0x0, PartitionNumber=0, Rewrite=0
	//     MBR Type=00, Boot=0, Recognized=0, HiddenSectors=0x0

	//
	// Partition Style=0, Count=0
	// MBR Signature: 00000000
	//

	//
	// Partition Style=1, Count=1
	// GPT DiskId={A3047FE8-6132-4899-971B-1038F1215383}, StartingUsableOffset=0x4400, UsableLength=0x3A38917A00, MaxPartitionCount=128
	// 0: Style=1, StartingOffset=0x4400, Length=0x8000000, PartitionNumber=1, Rewrite=0
	// GPT Attrs=0, Name=Microsoft reserved partition
	//

	diskIsInitialized = FALSE;

	if (PARTITION_STYLE_MBR == driveLayoutEx->PartitionStyle)
	{
		for (DWORD i = 0; i < driveLayoutEx->PartitionCount; ++i)
		{
			PPARTITION_INFORMATION_EX part = &driveLayoutEx->PartitionEntry[i];
			if (0 != part->PartitionNumber && part->Mbr.RecognizedPartition)
			{
				if (PARTITION_LDM == part->Mbr.PartitionType)
				{
					free(driveLayoutEx);
					*Abnormalities = NDAS_LOGICALUNIT_ABNORM_DYNAMIC_DISK;
					return S_OK;
				}
				diskIsInitialized = TRUE;
				break;
			}
		}
	}
	else if (PARTITION_STYLE_GPT == driveLayoutEx->PartitionStyle)
	{
		//
		// Ignores:
		//
		// PARTITION_MSFT_RESERVED_GUID
		// PARTITION_SYSTEM_GUID
		//
		// Valid:
		//
		// PARTITION_BASIC_DATA_GUID
		// PARTITION_LDM_DATA_GUID
		// PARTITION_LDM_METADATA_GUID
		//

		for (DWORD i = 0; i < driveLayoutEx->PartitionCount; ++i)
		{
			PPARTITION_INFORMATION_EX part = &driveLayoutEx->PartitionEntry[i];
			if (0 != part->PartitionNumber && 
				!IsEqualGUID(PARTITION_SYSTEM_GUID, part->Gpt.PartitionType) &&
				!IsEqualGUID(PARTITION_MSFT_RESERVED_GUID, part->Gpt.PartitionType))
			{
				diskIsInitialized = TRUE;
				break;
			}
		}
	}

	free(driveLayoutEx);

	if (!diskIsInitialized)
	{
		*Abnormalities = NDAS_LOGICALUNIT_ABNORM_DISK_WITH_NO_DATA_PARTITION;
	}

	return S_OK;
}


HRESULT 
xSetupDiGetDeviceInterfaceDetail(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
	__deref_out PSP_DEVICE_INTERFACE_DETAIL_DATA* DeviceInterfaceDetailData,
	__in_opt PSP_DEVINFO_DATA DeviceInfoData)
{
	HRESULT hr;

	DWORD detailSize = 0; 

	*DeviceInterfaceDetailData = NULL;

	BOOL success = SetupDiGetDeviceInterfaceDetail(
		DeviceInfoSet,
		DeviceInterfaceData,
		NULL,
		0,
		&detailSize,
		NULL);

	_ASSERTE(!success);
	if (!success && ERROR_INSUFFICIENT_BUFFER != GetLastError())
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
		return hr;
	}

	PSP_DEVICE_INTERFACE_DETAIL_DATA detail = 
		reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(detailSize));

	if (NULL == detail)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"malloc(%d) failed\n", detailSize);
		return E_OUTOFMEMORY;
	}

	detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	success = SetupDiGetDeviceInterfaceDetail(
		DeviceInfoSet,
		DeviceInterfaceData,
		detail, 
		detailSize,
		NULL,
		DeviceInfoData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);

		free(detail);

		return hr;
	}

	*DeviceInterfaceDetailData = detail;

	return S_OK;
}

HRESULT
pGetDevNodeConfigFlag(
	__in LPCTSTR DevicePath,
	__out DWORD* ConfigFlags)
{
	HRESULT hr;

	*ConfigFlags = 0;

	HDEVINFO devInfoSet = SetupDiCreateDeviceInfoList(NULL, NULL);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiCreateDeviceInfoList failed, hr=0x%X\n", hr);
		return hr;
	}

	SP_DEVICE_INTERFACE_DATA devIntfData;
	devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL success = SetupDiOpenDeviceInterface(
		devInfoSet, DevicePath, 0, &devIntfData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiCreateDeviceInfoList failed, hr=0x%X\n", hr);
		XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));
		return hr;
	}

	PSP_DEVICE_INTERFACE_DETAIL_DATA devIntfDetail;

	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	hr = xSetupDiGetDeviceInterfaceDetail(
		devInfoSet, &devIntfData, &devIntfDetail, &devInfoData);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"xSetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
		XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));
		return hr;
	}

	DWORD configFlags;

	success = SetupDiGetDeviceRegistryProperty(
		devInfoSet, 
		&devInfoData,
		SPDRP_CONFIGFLAGS,
		NULL, 
		reinterpret_cast<PBYTE>(&configFlags),
		sizeof(DWORD),
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiGetDeviceRegistryProperty failed, hr=0x%X\n", hr);
		free(devIntfDetail);
		XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));
		return hr;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"ConfigFlags=0x%X\n", configFlags);

	free(devIntfDetail);
	XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));

	*ConfigFlags = configFlags;

	return S_OK;
}

HRESULT 
pGetDevNodeStatus(
	__in LPCTSTR DevicePath,
	__out PULONG Status,
	__out PULONG ProblemNumber)
{
	HRESULT hr;
	HDEVINFO devInfoSet = SetupDiCreateDeviceInfoList(NULL, NULL);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiCreateDeviceInfoList failed, hr=0x%X\n", hr);
		return hr;
	}

	SP_DEVICE_INTERFACE_DATA devIntfData;
	devIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL success = SetupDiOpenDeviceInterface(
		devInfoSet, DevicePath, 0, &devIntfData);

	if (!success)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"SetupDiCreateDeviceInfoList failed, hr=0x%X\n", hr);
		XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));
		return hr;
	}

	PSP_DEVICE_INTERFACE_DETAIL_DATA devIntfDetail;
	
	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	hr = xSetupDiGetDeviceInterfaceDetail(
		devInfoSet, &devIntfData, &devIntfDetail, &devInfoData);

	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"xSetupDiGetDeviceInterfaceDetail failed, hr=0x%X\n", hr);
		XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));
		return hr;
	}

	CONFIGRET cret = CM_Get_DevNode_Status(Status, ProblemNumber, devInfoData.DevInst, 0);
	if (CR_SUCCESS != cret)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"CM_Get_DevNode_Status failed, cret=0x%X\n", cret);

		free(devIntfDetail);
		XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));

		return E_FAIL;
	}

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"Status=0x%X, Problem=0x%X\n", *Status, *ProblemNumber);

	free(devIntfDetail);
	XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));

	return S_OK;
}

} // end of namespace


HRESULT
NdasVolumeExtentsNdasLogicalUnitByName(
	__in LPCWSTR VolumeName,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	HRESULT hr;

	WCHAR volumeDeviceName[MAX_PATH+1];

	hr = StringCchCopy(volumeDeviceName, MAX_PATH+1, VolumeName);
	XTLASSERT(SUCCEEDED(hr));

	//
	// PathRemoveBackslashW(volumeDeviceName);
	//
	// PathRemoveBaclslash does not remove the backslash in C:\
	//

	LPWSTR p = volumeDeviceName;
	while (*p) ++p;
	if (p > volumeDeviceName && *(p-1) == L'\\')
	{
		*(p-1) = L'\0';
	}

	HANDLE volumeHandle = CreateFileW(
		volumeDeviceName,
		GENERIC_READ, 
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == volumeHandle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR,
			"CreateFile(%ls) failed, hr=0x%X\n", volumeDeviceName, hr);
		return hr;
	}

	hr = NdasVolumeExtentsNdasLogicalUnit(volumeHandle, LogicalUnitAddress);

	XTLVERIFY( CloseHandle(volumeHandle) );

	return hr;
}

HRESULT
NdasVolumeExtentsNdasLogicalUnit(
	__in HANDLE VolumeHandle,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;

	//
	// Get NDAS Logical Unit Address from the volume directly.
	// Applicable to non-ATA devices
	//
	HRESULT hr = NdasPortCtlGetLogicalUnitAddress(VolumeHandle, &logicalUnitAddress);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"GetLogicalUnitAddressFromVolume(%p) failed, hr=0x%X\n", VolumeHandle, hr);
	}

	PVOLUME_DISK_EXTENTS diskExtents;
	hr = SdiVolumeGetVolumeDiskExtents(VolumeHandle, &diskExtents, NULL);
	if (FAILED(hr))
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
			"GetVolumeDiskExtents(%p) failed, hr=0x%X\n", VolumeHandle, hr);
		return hr;
	}

	for (DWORD i = 0; i < diskExtents->NumberOfDiskExtents; ++i)
	{
		DWORD diskNumber = diskExtents->Extents[i].DiskNumber;
		TCHAR diskDeviceName[24];

		hr = StringCchPrintf(
			diskDeviceName, RTL_NUMBER_OF(diskDeviceName), 
			_T("\\\\.\\PhysicalDrive%d"), diskNumber);
		XTLASSERT(SUCCEEDED(hr));

		HANDLE diskHandle = CreateFile(
			diskDeviceName,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (INVALID_HANDLE_VALUE == diskHandle)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"CreateFile failed, name=%ls, error=0x%X\n", 
				diskDeviceName, GetLastError());
			continue;
		}

		hr = NdasPortCtlGetLogicalUnitAddress(diskHandle, &logicalUnitAddress);

		if (SUCCEEDED(hr))
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING,
				"Disk(%ls) extents LogicalUnit=%08X.\n",
				diskDeviceName, logicalUnitAddress.Address);

			XTLVERIFY( CloseHandle (diskHandle) );
			free(diskExtents);

			*LogicalUnitAddress = logicalUnitAddress;

			return S_OK;
		}

		XTLVERIFY( CloseHandle (diskHandle) );
	}

	free(diskExtents);

	return E_FAIL;
}
