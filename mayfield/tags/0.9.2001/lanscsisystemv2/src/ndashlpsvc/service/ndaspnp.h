#ifndef _NDAS_PNP_H_
#define _NDAS_PNP_H_
#pragma once
#include "pnpevent.h"
#include <map>
#include <set>

class CNdasServicePowerEventHandler :
	public ximeta::CPowerEventHandler
{
public:
	virtual LRESULT OnQuerySuspend(DWORD dwFlags);
	virtual void OnQuerySuspendFailed();
};

class CNdasServiceDeviceEventHandler : 
	public ximeta::CDeviceEventHandler
{
	static LPCWSTR LANSCSIDEV_IFID_W;

	BOOL m_bNoLfs;
	BOOL m_bInitialized;
	HANDLE m_hROFilter;
	BOOL m_bROFilterFilteringStarted;

	const HANDLE m_hRecipient;
	const DWORD m_dwReceptionFlags;

	HDEVNOTIFY m_hStoragePortNotify;
	HDEVNOTIFY m_hVolumeNotify;
	HDEVNOTIFY m_hDiskNotify;

	typedef std::set<DWORD> FilterdDriveNumberSet;
	typedef std::map<DWORD,HDEVNOTIFY> NdasLogicalDeviceNotifyMap;
	FilterdDriveNumberSet m_FilteredDriveNumbers;
	NdasLogicalDeviceNotifyMap m_NdasLogicalDeviceStoragePortNotifyMap;

protected:

	HDEVNOTIFY RegisterDeviceInterfaceNotification(LPCGUID classGuid);
	HDEVNOTIFY RegisterDeviceHandleNotification(HANDLE hDeviceFile);
	
	LRESULT OnStoragePortDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnStoragePortDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnStoragePortDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnStoragePortDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnStoragePortDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnVolumeDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnVolumeDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnVolumeDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnVolumeDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnVolumeDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

	LRESULT OnDiskDeviceInterfaceArrival(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnDiskDeviceInterfaceQueryRemove(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnDiskDeviceInterfaceQueryRemoveFailed(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnDiskDeviceInterfaceRemovePending(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);
	LRESULT OnDiskDeviceInterfaceRemoveComplete(PDEV_BROADCAST_DEVICEINTERFACE pdbhdr);

public:

	CNdasServiceDeviceEventHandler(HANDLE hRecipient, DWORD dwReceptionFlag);
	~CNdasServiceDeviceEventHandler();
	BOOL Initialize();

	virtual LRESULT OnDeviceArrival(PDEV_BROADCAST_HDR pdbhdr);
	virtual LRESULT OnDeviceQueryRemove(PDEV_BROADCAST_HDR pdbhdr);
	virtual LRESULT OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR pdbhdr);
	virtual LRESULT OnDeviceRemovePending(PDEV_BROADCAST_HDR pdbhdr);
	virtual LRESULT OnDeviceRemoveComplete(PDEV_BROADCAST_HDR pdbhdr);
};

#endif /* _NDAS_PNP_H_ */