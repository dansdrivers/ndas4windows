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

#include "ndaspnp.h"
#include "ndasdev.h"
#include "ndascfg.h"
#include "ndasobjs.h"
#include "ndaseventpub.h"
#include "ndaseventmon.h"
#include "xguid.h"
#include "sysutil.h"

#include <initguid.h>
#include <ndas/ndasportguid.h>
#include <ndas/ndasdluguid.h>

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
		if(NdasPort) {
			NDAS_LOGICALUNIT_ADDRESS	ndasLogicalUnitAddress;
			ndasLogicalUnitAddress.Address = location;
			location = NdasLogicalUnitAddressToLocation(ndasLogicalUnitAddress);
		}

		CNdasLogicalDevicePtr pLogDevice = 
			pGetNdasLogicalDeviceByNdasLocation(location);

		if (CNdasLogicalDeviceNullPtr == pLogDevice)
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"Logical device is not available. Already unmounted?\n");
			return;
		}

		NDAS_LOGICALDEVICE_STATUS status = pLogDevice->GetStatus();

		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != status &&
			NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != status) 
		{
			XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
				"Logical device is not mounted nor mount pending. Already unmounted?\n");
			return;
		}

		AllowedAccess &= pLogDevice->GetMountedAccess();
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

} // end of namespace

//////////////////////////////////////////////////////////////////////////

CNdasServiceDeviceEventHandler::CNdasServiceDeviceEventHandler(
	CNdasService& service) :
	m_service(service),
	m_bInitialized(FALSE),
	m_bROFilterFilteringStarted(FALSE),
	m_hROFilter(INVALID_HANDLE_VALUE),
	m_bNoLfs(NdasServiceConfig::Get(nscDontUseWriteShare)),
	m_hRecipient(NULL),
	m_dwReceptionFlags(0)
{
	InitializeCriticalSection(&m_DevNotifyMapSection);
}

CNdasServiceDeviceEventHandler::~CNdasServiceDeviceEventHandler()
{
	Uninitialize();
	DeleteCriticalSection(&m_DevNotifyMapSection);
}

bool
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

	RegisterDeviceInterfaceNotification(
		&GUID_DEVINTERFACE_VOLUME, "Volume");

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
	Uninitialize();
}

void
CNdasServiceDeviceEventHandler::Uninitialize()
{
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

	EnterCriticalSection(&m_DevNotifyMapSection);

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

	LeaveCriticalSection(&m_DevNotifyMapSection);
}

void
CNdasServiceDeviceEventHandler::OnStoragePortDeviceInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"DeviceInterface Arrival: %ls\n", pdbhdr->dbcc_name);

	HRESULT hr;
	BOOL fSuccess = FALSE;
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

	RegisterDeviceHandleNotification(
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

	ndata.NdasLocation = NdasLocation;

	EnterCriticalSection(&m_DevNotifyMapSection);

	std::pair<DevNotifyMap::iterator,bool> mi;

	try
	{
		mi = m_DevNotifyMap.insert(std::make_pair(devNotifyHandle, ndata));
	}
	catch (...)
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_ERROR, 
			"DevNotifyMap.insert() failed.\n");

		LeaveCriticalSection(&m_DevNotifyMapSection);

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

		LeaveCriticalSection(&m_DevNotifyMapSection);

		XTLVERIFY( UnregisterDeviceNotification(devNotifyHandle) );

		return E_OUTOFMEMORY;
	}

	LeaveCriticalSection(&m_DevNotifyMapSection);

	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"Device handle %p Notification Registered successfully.\n", 
		DeviceHandle);

	return S_OK;
}

HRESULT 
CNdasServiceDeviceEventHandler::UnregisterDeviceHandleNotification(
	__in HDEVNOTIFY DevNotifyHandle)
{
	EnterCriticalSection(&m_DevNotifyMapSection);

	DevNotifyMap::iterator itr = m_DevNotifyMap.find(DevNotifyHandle);
	if (m_DevNotifyMap.end() == itr)
	{
		LeaveCriticalSection(&m_DevNotifyMapSection);
		return E_FAIL;
	}

	DEVICE_HANDLE_NOTIFY_DATA& ndata = itr->second;

	m_DevNotifyMap.erase(itr);

	LeaveCriticalSection(&m_DevNotifyMapSection);

	XTLVERIFY( UnregisterDeviceNotification(DevNotifyHandle) );

	return S_OK;
}

void
CNdasServiceDeviceEventHandler::OnVolumeDeviceInterfaceArrival(
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
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
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

		return;
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
	getEffectiveAccess.NdasPort = m_service.NdasPortExists();

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
	hVolume.Release();

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

void
CNdasServiceDeviceEventHandler::OnLogicalUnitInterfaceArrival(
	PDEV_BROADCAST_DEVICEINTERFACE pdbhdr)
{
	XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_INFORMATION, 
		"OnLogicalUnitInterfaceArrival: %ls\n", pdbhdr->dbcc_name);

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

	HRESULT hr;

	NDAS_LOCATION ndasLocation;

	if (m_service.NdasPortExists())
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

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDeviceByNdasLocation(ndasLocation);
	
	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	{
		// unknown NDAS logical device
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"No Logical device found in LDM, ndasLocation=%d\n", 
			ndasLocation);
		return;
	}

	pLogDevice->OnMounted(pdbhdr->dbcc_name);

	RegisterDeviceHandleNotification(
		deviceHandle, 
		ndasLocation,
		pdbhdr->dbcc_name);
	
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
	// Do not worry about redundant calls to pLogDevice->OnUnmountFailed().
	// It only cares if NDAS_LOGICALDEVICE_STATUS is UNMOUNT_PENDING,
	// which means backup calls will be ignored.
	//

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDeviceByNdasLocation(notifyData->NdasLocation);

	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Logical Device not found, ndasLocation=%08X.\n", 
			notifyData->NdasLocation);
		return;
	}

	pLogDevice->OnUnmountFailed();
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

	NDAS_LOCATION ndasLocation = notifyData->NdasLocation;

	UnregisterDeviceHandleNotification(pdbch->dbch_hdevnotify);

	CNdasLogicalDevicePtr pLogDevice = 
		pGetNdasLogicalDeviceByNdasLocation(ndasLocation);

	if (CNdasLogicalDeviceNullPtr == pLogDevice) 
	{
		XTLTRACE2(NDASSVC_PNP, TRACE_LEVEL_WARNING, 
			"Logical Device not found, ndasLocation=%08X.\n", 
			ndasLocation);
		return;
	}

	pLogDevice->OnUnmounted();
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

		CNdasLogicalDevicePtr pLogDevice = 
			pGetNdasLogicalDeviceByNdasLocation(notifyData->NdasLocation);

		if (CNdasLogicalDeviceNullPtr == pLogDevice) 
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
		emon.OnLogicalDeviceAlarmedByPnP(pLogDevice, dluEvent->DluInternalStatus);
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

		RegisterDeviceHandleNotification(
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

		RegisterDeviceHandleNotification(
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
	__out LPTSTR Buffer,
	__in DWORD BufferLength)
{
	HRESULT hr;

	EnterCriticalSection(&m_DevNotifyMapSection);

	std::vector<NDAS_LOCATION_DATA>::const_iterator itr = std::find_if(
		m_NdasLocationData.begin(),
		m_NdasLocationData.end(),
		NdasLocationDataFinder(NdasLocation));

	if (itr == m_NdasLocationData.end())
	{
		LeaveCriticalSection(&m_DevNotifyMapSection);
		return HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS);
	}

	hr = StringCchCopy(Buffer, BufferLength, itr->DevicePath);

	LeaveCriticalSection(&m_DevNotifyMapSection);

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

} // end of namespace


