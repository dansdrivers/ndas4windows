#ifndef _NDAS_PNP_H_
#define _NDAS_PNP_H_
#pragma once
#include <map>
#include <set>
#include <xtl/xtlpnpevent.h>
#include <xtl/xtlautores.h>

class CNdasService;

typedef struct _DEVICE_HANDLE_NOTIFY_DATA {
	NDAS_LOCATION NdasLocation;
} DEVICE_HANDLE_NOTIFY_DATA, *PDEVICE_HANDLE_NOTIFY_DATA;

typedef struct _NDAS_LOCATION_DATA {
	NDAS_LOCATION NdasLocation;
	TCHAR DevicePath[MAX_PATH];
} NDAS_LOCATION_DATA;

class CNdasServiceDeviceEventHandler : 
	public XTL::CDeviceEventHandler<CNdasServiceDeviceEventHandler>
{
protected:

	CNdasService& m_service;

	BOOL m_bNoLfs;
	BOOL m_bInitialized;
	BOOL m_bROFilterFilteringStarted;
	XTL::AutoFileHandle m_hROFilter;

	HANDLE m_hRecipient;
	DWORD m_dwReceptionFlags;

	typedef std::set<DWORD> FilterdDriveNumberSet;
	
	FilterdDriveNumberSet m_FilteredDriveNumbers;

	CRITICAL_SECTION m_DevNotifyMapSection;
	std::map<HDEVNOTIFY,DEVICE_HANDLE_NOTIFY_DATA> m_DevNotifyMap;
	std::vector<NDAS_LOCATION_DATA> m_NdasLocationData;

	std::vector<HDEVNOTIFY> m_DeviceInterfaceNotifyHandles;

	typedef std::map<HDEVNOTIFY,DEVICE_HANDLE_NOTIFY_DATA> DevNotifyMap;

public:

	CNdasServiceDeviceEventHandler(CNdasService& service);
	~CNdasServiceDeviceEventHandler();
	
	bool Initialize(HANDLE hRecipient, DWORD dwReceptionFlag); 
	void Uninitialize();

	void OnDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE dbcc);

	void OnStoragePortDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	void OnVolumeDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	void OnLogicalUnitInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnDeviceHandleQueryRemove(PDEV_BROADCAST_HANDLE pdbch);
	void OnDeviceHandleQueryRemoveFailed(PDEV_BROADCAST_HANDLE pdbch);
	void OnDeviceHandleRemovePending(PDEV_BROADCAST_HANDLE pdbch);
	void OnDeviceHandleRemoveComplete(PDEV_BROADCAST_HANDLE pdbch);
	void OnDeviceHandleCustomEvent(PDEV_BROADCAST_HANDLE pdbch);

	void OnShutdown();

	HRESULT GetLogicalUnitDevicePath(
		__in NDAS_LOCATION NdasLocation,
		__out LPTSTR Buffer,
		__in DWORD BufferLength);

protected:

	HRESULT RegisterDeviceInterfaceNotification(
		__in LPCGUID InterfaceGuid,
		__in LPCSTR TypeName);

	HRESULT RegisterDeviceHandleNotification(
		__in HANDLE DeviceHandle, 
		__in NDAS_LOCATION NdasLocation,
		__in LPCTSTR DevicePath);

	HRESULT UnregisterDeviceHandleNotification(
		__in HDEVNOTIFY DevNotifyHandle);

	const DEVICE_HANDLE_NOTIFY_DATA* GetDeviceHandleNotificationData(
		__in HDEVNOTIFY NotifyHandle);

	HRESULT pRegisterNdasScsiPorts();
	HRESULT pRegisterLogicalUnits();
	HRESULT pRegisterLogicalUnit(
		__in HDEVINFO DevInfoSet, 
		__in LPCGUID InterfaceGuid);
};

#endif /* _NDAS_PNP_H_ */
