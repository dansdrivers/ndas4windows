#ifndef _NDAS_PNP_H_
#define _NDAS_PNP_H_
#pragma once
#include <map>
#include <set>
#include <xtl/xtlpnpevent.h>
#include <xtl/xtlautores.h>

class CNdasService;

class CNdasServicePowerEventHandler :
	public XTL::CPowerEventHandler<CNdasServicePowerEventHandler>
{
public:

	CNdasServicePowerEventHandler(CNdasService& service);
	bool Initialize();

	LRESULT OnQuerySuspend(DWORD dwFlags);
	void OnQuerySuspendFailed();
	void OnSuspend();
	void OnResumeAutomatic();
	void OnResumeSuspend();
	void OnResumeCritical();

protected:
	CNdasService& m_service;
private:
	CNdasServicePowerEventHandler(const CNdasServicePowerEventHandler&);
	const CNdasServicePowerEventHandler& operator=(const CNdasServicePowerEventHandler&);
};

typedef enum _DEVICE_HANDLE_TYPE 
{ 
	DNT_UNKNOWN = 0,
	DNT_STORAGE_PORT, 
	DNT_DISK, 
	DNT_CDROM, 
	DNT_VOLUME
} DEVICE_HANDLE_TYPE;

typedef struct _DEVICE_HANDLE_NOTIFY_DATA
{
	DEVICE_HANDLE_TYPE Type;
	NDAS_LOCATION NdasLocation;
	TCHAR DevicePath[MAX_PATH];
} DEVICE_HANDLE_NOTIFY_DATA, *PDEVICE_HANDLE_NOTIFY_DATA;

typedef std::map<HDEVNOTIFY,DEVICE_HANDLE_NOTIFY_DATA> DeviceNotifyHandleMap;

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

	XTL::AutoDeviceNotifyHandle m_hStoragePortNotify;
	XTL::AutoDeviceNotifyHandle m_hVolumeNotify;
	XTL::AutoDeviceNotifyHandle m_hDiskNotify;
	XTL::AutoDeviceNotifyHandle m_hCdRomClassNotify;

	typedef std::set<DWORD> FilterdDriveNumberSet;
	
	FilterdDriveNumberSet m_FilteredDriveNumbers;
	DeviceNotifyHandleMap m_DevNotifyMap;

protected:

	HDEVNOTIFY RegisterDeviceInterfaceNotification(LPCGUID classGuid);
	HDEVNOTIFY RegisterDeviceHandleNotification(HANDLE hDeviceFile);
	
	LRESULT OnStoragePortDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnStoragePortDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnVolumeDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnVolumeDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnDiskDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnDiskDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnCdRomDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnCdRomDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnDeviceHandleQueryRemove(PDEV_BROADCAST_HANDLE pdbch);
	LRESULT OnDeviceHandleQueryRemoveFailed(PDEV_BROADCAST_HANDLE pdbch);
	LRESULT OnDeviceHandleRemovePending(PDEV_BROADCAST_HANDLE pdbch);
	LRESULT OnDeviceHandleRemoveComplete(PDEV_BROADCAST_HANDLE pdbch);

public:

	CNdasServiceDeviceEventHandler(CNdasService& service);
	~CNdasServiceDeviceEventHandler();
	
	bool Initialize(HANDLE hRecipient, DWORD dwReceptionFlag); 
	bool Uninitialize();

	void OnDeviceArrival(PDEV_BROADCAST_HDR pdbhdr);
	LRESULT OnDeviceQueryRemove(PDEV_BROADCAST_HDR pdbhdr);
	void OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR pdbhdr);
	void OnDeviceRemovePending(PDEV_BROADCAST_HDR pdbhdr);
	void OnDeviceRemoveComplete(PDEV_BROADCAST_HDR pdbhdr);

	void OnShutdown();

public:

	bool AddDeviceNotificationHandle(HANDLE hDevice, const DEVICE_HANDLE_NOTIFY_DATA& Data);

protected:

	bool FindDeviceHandle(
		__in HDEVNOTIFY NotifyHandle, 
		__out DEVICE_HANDLE_NOTIFY_DATA* Data);
	HRESULT pEnumerateNdasStoragePorts();

};

#endif /* _NDAS_PNP_H_ */
