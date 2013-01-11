#ifndef _NDAS_PNP_H_
#define _NDAS_PNP_H_
#pragma once
#include <xtl/xtlpnpevent.h>
#include <xtl/xtlautores.h>

class CNdasService;

typedef enum _DEVICE_HANDLE_TYPE {
	PnpLogicalUnitHandle, 
	PnpVolumeHandle,
	PnpStoragePortHandle,
} DEVICE_HANDLE_TYPE;

typedef struct _DEVICE_HANDLE_NOTIFY_DATA {
	DEVICE_HANDLE_TYPE HandleType;
	NDAS_LOCATION NdasLocation;
} DEVICE_HANDLE_NOTIFY_DATA, *PDEVICE_HANDLE_NOTIFY_DATA;

typedef struct _NDAS_LOCATION_DATA {
	NDAS_LOCATION NdasLocation;
	TCHAR DevicePath[MAX_PATH];
} NDAS_LOCATION_DATA;

class CCritSec : private CRITICAL_SECTION
{
public:
	CCritSec()
	{
		InitializeCriticalSection(this);
	}
	~CCritSec()
	{
		DeleteCriticalSection(this);
	}
	void Enter()
	{
		EnterCriticalSection(this);
	}
	void Leave()
	{
		LeaveCriticalSection(this);
	}
	BOOL TryEnter()
	{
		return TryEnterCriticalSection(this);
	}
};

class CAutoCritSec
{
	CCritSec* m_cs;
public:
	explicit CAutoCritSec(CCritSec& cs) : m_cs(&cs) { m_cs->Enter(); }
	~CAutoCritSec() { if (m_cs) m_cs->Leave(); }
	void Release() { m_cs = NULL; }
};

class CNdasServiceDeviceEventHandler : 
	public XTL::CDeviceEventHandler<CNdasServiceDeviceEventHandler>
{
protected:

	BOOL m_bNoLfs;
	BOOL m_bInitialized;
	BOOL m_bROFilterFilteringStarted;
	XTL::AutoFileHandle m_hROFilter;

	HANDLE m_hRecipient;
	DWORD m_dwReceptionFlags;
	
	std::set<DWORD> m_FilteredDriveNumbers;

	CCritSec m_DevNotifyMapLock;
	std::map<HDEVNOTIFY,DEVICE_HANDLE_NOTIFY_DATA> m_DevNotifyMap;
	std::vector<NDAS_LOCATION_DATA> m_NdasLocationData;

	std::vector<HDEVNOTIFY> m_DeviceInterfaceNotifyHandles;

	typedef std::map<HDEVNOTIFY,DEVICE_HANDLE_NOTIFY_DATA> DevNotifyMap;

	CCritSec m_NdasLogicalUnitDrivesLock;
	std::vector<CComPtr<INdasLogicalUnit> > m_NdasLogicalUnitDrives;

	LONG m_WorkQueueCount;

public:

	CNdasServiceDeviceEventHandler();
	~CNdasServiceDeviceEventHandler();
	
	HRESULT Initialize(HANDLE hRecipient, DWORD dwReceptionFlag); 
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

	void OnVolumeArrival(PDEV_BROADCAST_VOLUME dbcv);
	void OnVolumeRemoveComplete(PDEV_BROADCAST_VOLUME dbcv);

	void OnShutdown();

	HRESULT GetLogicalUnitDevicePath(
		__in NDAS_LOCATION NdasLocation,
		__out BSTR* DevicePath);

	HRESULT QueueRescanDriveLetters();

protected:

	void RescanDriveLetters();

	static DWORD WINAPI RescanDriverLettersThreadStart(LPVOID Context)
	{
		CNdasServiceDeviceEventHandler* instance = 
			static_cast<CNdasServiceDeviceEventHandler*>(Context);
		instance->RescanDriveLetters();
		ATLVERIFY(InterlockedDecrement(&instance->m_WorkQueueCount) >= 0);
		return 0;
	}


	HRESULT RegisterDeviceInterfaceNotification(
		__in LPCGUID InterfaceGuid,
		__in LPCSTR TypeName);

	HRESULT RegisterVolumeHandleNotification(
		__in HANDLE DeviceHandle, 
		__in LPCTSTR DevicePath);

	HRESULT RegisterStoragePortHandleNotification(
		__in HANDLE DeviceHandle,
		__in NDAS_LOCATION NdasLocation,
		__in LPCTSTR DevicePath);

	HRESULT RegisterLogicalUnitHandleNotification(
		__in HANDLE DeviceHandle, 
		__in NDAS_LOCATION NdasLocation,
		__in LPCTSTR DevicePath);

	HRESULT RegisterDeviceHandleNotification(
		__in HANDLE DeviceHandle, 
		__in DEVICE_HANDLE_TYPE HandleType,
		__in NDAS_LOCATION NdasLocation,
		__in LPCTSTR DevicePath);

	HRESULT UnregisterDeviceHandleNotification(
		__in HDEVNOTIFY DevNotifyHandle);

	const DEVICE_HANDLE_NOTIFY_DATA* GetDeviceHandleNotificationData(
		__in HDEVNOTIFY NotifyHandle);

	HRESULT pRegisterNdasScsiPorts();

	HRESULT RegisterLogicalUnits();
	
	HRESULT RegisterLogicalUnit(
		__in HDEVINFO DevInfoSet, 
		__in LPCGUID InterfaceGuid);

	void pOnVolumeArrivalOrRemoval(PDEV_BROADCAST_VOLUME dbcv, BOOL Removal);
	void pOnVolumeArrivalOrRemoval(DWORD UnitMask, BOOL Removal);
};

#endif /* _NDAS_PNP_H_ */
