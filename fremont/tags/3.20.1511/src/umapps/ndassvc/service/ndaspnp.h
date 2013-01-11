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

struct DeviceNotifyData
{
	// inner data type
	enum DataType 
	{ 
		DNTUnknown = 0,
		DNTStoragePort, 
		DNTDisk, 
		DNTCdRom, 
		DNTVolume 
	};

	// fields
	DataType Type;
	NDAS_LOCATION NdasLocation;

	// constructors

	DeviceNotifyData(DataType Type, DWORD SlotNumber) :
		Type(Type), NdasLocation(SlotNumber)
	{
	}

	DeviceNotifyData()
	{
		DeviceNotifyData(DNTUnknown, 0);
	}
};

typedef std::map<HDEVNOTIFY,DeviceNotifyData> DeviceNotifyHandleMap;

class CNdasServiceDeviceEventHandler : 
	public XTL::CDeviceEventHandler<CNdasServiceDeviceEventHandler>
{
public:
	
	//typedef enum _DEVNOTIFYINFO_TYPE 
	//{
	//	DEVNOTIFYINFO_TYPE_STORAGEPORT,
	//	DEVNOTIFYINFO_TYPE_VOLUME,
	//	DEVNOTIFYINFO_TYPE_DISK,
	//	DEVNOTIFYINFO_TYPE_CDROM
	//} DEVNOTIFYINFO_TYPE;

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
//	LRESULT OnStoragePortDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnStoragePortDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnStoragePortDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnStoragePortDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnVolumeDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnVolumeDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnVolumeDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnVolumeDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnVolumeDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnDiskDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnDiskDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnDiskDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnDiskDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnDiskDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnCdRomDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnCdRomDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnCdRomDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
//	LRESULT OnCdRomDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
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

	bool AddDeviceNotificationHandle(HANDLE hDevice, const DeviceNotifyData& Data);

protected:

	bool FindDeviceHandle(HDEVNOTIFY NotifyHandle, DeviceNotifyData& Data);

};

#endif /* _NDAS_PNP_H_ */
