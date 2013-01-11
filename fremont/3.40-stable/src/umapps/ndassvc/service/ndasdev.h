/*//////////////////////////////////////////////////////////////////////////
//
// Copyright (C)2002-2004 XIMETA, Inc.
// All rights reserved.
//
//////////////////////////////////////////////////////////////////////////*/

#pragma once

//////////////////////////////////////////////////////////////////////////
//
// Device Class
//
//////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE CNdasDevice :
	public CComObjectRootEx<CComMultiThreadModel>,
	public ILockImpl<INdasDeviceInternal>,
	public INdasHeartbeatSink,
	public INdasDevicePnpSink,
	public INdasTimerEventSink
{
public:

	BEGIN_COM_MAP(CNdasDevice)
		COM_INTERFACE_ENTRY(INdasDevice)
		COM_INTERFACE_ENTRY(INdasDevicePnpSink)
		COM_INTERFACE_ENTRY(INdasTimerEventSink)
	END_COM_MAP()

	enum { MAX_NDAS_UNITDEVICE_COUNT = 2 };

	typedef CAutoLock<CNdasDevice> CAutoInstanceLock;

protected:

	// Slot number
	DWORD m_SlotNo;
	
	// Device ID
	NDAS_DEVICE_ID m_NdasDeviceId;

	DWORD m_RegFlags;

	// Unit Devices
	CInterfaceArray<INdasUnit> m_NdasUnits;
	
	// Registry configuration container name
	TCHAR m_ConfigContainer[30];

	// Device status
	NDAS_DEVICE_STATUS m_Status;

	// Last error
	NDAS_DEVICE_ERROR m_NdasDeviceError;

	// Granted access, which is supplied by the user
	// based on the registration key
	ACCESS_MASK m_grantedAccess;

	typedef union _SOCKET_ADDRESS_BUFFER {
		SOCKADDR Address;
		SOCKADDR_LPX LpxAddress;
		SOCKADDR_IN Ipv4Address;
#ifdef _WS2IPDEF_
		SOCKADDR_IN6 Ipv6Address;
#endif
		UCHAR RawAddress[32];
	} SOCKET_ADDRESS_BUFFER, LPSOCKET_ADDRESS_BUFFER;

	SOCKET_ADDRESS m_LocalSocketAddress;
	SOCKET_ADDRESS_BUFFER m_LocalSocketAddressBuffer;
	TCHAR m_LocalSocketAddressString[32];

	SOCKET_ADDRESS m_RemoteSocketAddress;
	SOCKET_ADDRESS_BUFFER m_RemoteSocketAddressBuffer;
	TCHAR m_RemoteSocketAddressString[32];

	// Device Name
	CComBSTR m_Name;

	// Last Heartbeat Tick
	DWORD m_LastHeartbeatTick;

	// Initial communication failure count
	DWORD m_DiscoverErrors;

	// NDAS Device OEM Code
	NDAS_OEM_CODE m_OemCode;

	// NDAS Device Stats
	NDAS_DEVICE_STAT m_NdasDeviceStat;

	// NDAS device hardware information
	NDAS_DEVICE_HARDWARE_INFO m_HardwareInfo;

	// NDAS ID Extension Data
	NDASID_EXT_DATA m_NdasIdExtension;

public:

	CNdasDevice() {}

	HRESULT NdasDevInitialize(
		__in DWORD SlotNo, 
		__in const NDAS_DEVICE_ID& DeviceId,
		__in DWORD RegFlags,
		__in_opt const NDASID_EXT_DATA* NdasIdExtension);

	void FinalRelease();

	//
	// Enable or Disable the device
	// When disabled, no heartbeat packet is processed
	//
	STDMETHODIMP put_Enabled(__in BOOL Enabled);

	STDMETHODIMP get_NdasDeviceId(__out NDAS_DEVICE_ID* NdasDeviceId);
	STDMETHODIMP get_SlotNo(__out DWORD* SlotNo);
	STDMETHODIMP get_RegisterFlags(__out DWORD* Flags);

	STDMETHODIMP put_Name(__in BSTR Name);
	STDMETHODIMP get_Name(__out BSTR* Name);
	STDMETHODIMP get_Status(__out NDAS_DEVICE_STATUS* Status);
	STDMETHODIMP get_DeviceError(__out NDAS_DEVICE_ERROR* Error);

	STDMETHODIMP put_GrantedAccess(__in ACCESS_MASK access);
	STDMETHODIMP get_GrantedAccess(__out ACCESS_MASK* Access);

	STDMETHODIMP get_AllowedAccess(__out ACCESS_MASK* Access);

	STDMETHODIMP get_NdasUnits(__inout CInterfaceArray<INdasUnit> & NdasUnits);
	STDMETHODIMP get_NdasUnit(__in DWORD UnitNo, __deref_out INdasUnit** ppNdasUnit);

	STDMETHODIMP get_RemoteAddress(__inout SOCKET_ADDRESS * SocketAddress);
	STDMETHODIMP get_LocalAddress(__inout SOCKET_ADDRESS * SocketAddress);

	STDMETHODIMP get_HardwareType(__out DWORD* HardwareType);
	STDMETHODIMP get_HardwareVersion(__out DWORD* HardwareVersion);
	STDMETHODIMP get_HardwareRevision(__out DWORD* HardwareRevision);
	STDMETHODIMP get_HardwarePassword(__out UINT64* HardwarePassword);

	STDMETHODIMP get_OemCode(__out NDAS_OEM_CODE* OemCode);
	STDMETHODIMP put_OemCode(__in const NDAS_OEM_CODE* OemCode);

	STDMETHODIMP get_HardwareInfo(__out NDAS_DEVICE_HARDWARE_INFO* HardwareInfo);
	STDMETHODIMP get_DeviceStat(__out NDAS_DEVICE_STAT* DeviceStat);
	STDMETHODIMP get_NdasIdExtension(__out NDASID_EXT_DATA* IdExtension);
	STDMETHODIMP get_MaxTransferBlocks(__out LPDWORD MaxTransferBlocks);

	//
	// Update Device Stats
	//
	STDMETHODIMP UpdateStats();

	//
	// Invalidating Unit Device
	//
	STDMETHODIMP InvalidateNdasUnit(INdasUnit* pNdasUnit);

	//
	// Discover Event Subscription
	//
	STDMETHODIMP_(void) NdasHeartbeatReceived(const NDAS_DEVICE_HEARTBEAT_DATA* Data);

	//
	// Status Check Event Subscription
	//
	STDMETHODIMP_(void) OnTimer();

	//
	// INdasDevicePnpSink
	//
	STDMETHODIMP_(void) UnitDismountCompleted(__in INdasUnit* pNdasUnit);

protected:

	// Is this device is volatile (not persistent) ?
	// Auto-registered or OEM devices may be volatile,
	// which means the registration will not be retained after reboot.
	bool pIsVolatile();

	BOOL pChangeStatus(
		__in NDAS_DEVICE_STATUS newStatus,
		__in_opt const CInterfaceArray<INdasUnit>* NewNdasUnits = NULL);

	void pSetLastDeviceError(NDAS_DEVICE_ERROR deviceError);


	//
	// Retrieve device information
	//
	HRESULT pRetrieveHardwareInfo(
		__out NDAS_DEVICE_HARDWARE_INFO& HardwareInfo);
	
	HRESULT pUpdateStats(
		__out NDAS_DEVICE_STAT& DeviceStat);

	HRESULT pGetLocalSocketAddressList(
		__deref_out PSOCKET_ADDRESS_LIST* SocketAddressList);

	//
	// Reconcile Unit Device Instances
	//
	// Return value: 
	//  TRUE if status can be changed to DISCONNECTED,
	//  FALSE if there are any MOUNTED unit devices left.
	//
	void pDestroyAllUnitDevices();

	HRESULT pCreateNdasUnit(__in DWORD UnitNo, __deref_out INdasUnit** ppNdasUnit);

	//
	// Is any unit device is mounted?
	//
	bool pIsAnyUnitDevicesMounted();

	//
	// Registry Access Helper
	//
	template <typename T> BOOL pSetConfigValue(LPCTSTR szName, T value);
	template <typename T> BOOL pSetConfigValueSecure(LPCTSTR szName, T value);
	BOOL pSetConfigValueSecure(LPCTSTR szName, LPCVOID lpValue, DWORD cbValue);
	BOOL pDeleteConfigValue(LPCTSTR szName);

	struct NdasDeviceTaskItem
	{
		enum Type 
		{ 
			HALT_TASK,
			DISCOVER_TASK, 
			NDAS_UNIT_CREATE_TASK,
		};
		Type TaskType;
		DWORD UnitNo;
		NdasDeviceTaskItem(Type TaskType, DWORD UnitNo) : 
			TaskType(TaskType), UnitNo(UnitNo) {} 
	};

	HANDLE m_TaskThread;
	HANDLE m_TaskQueueSemaphore;
	CLock m_TaskQueueLock; 
	std::queue<NdasDeviceTaskItem> m_TaskQueue;

	HRESULT pQueueTask(NdasDeviceTaskItem::Type Type, DWORD UnitNo = 0);
	HRESULT pTaskThreadStart();

	HRESULT pQueueDiscoverTask();
	HRESULT pQueueNdasUnitCreationTask(DWORD UnitNo);

	HRESULT pDiscoverThreadStart();
	HRESULT pNdasUnitCreationThreadStart(DWORD UnitNo);

	static DWORD WINAPI pTaskThreadStart(CNdasDevice* pInstance)
	{
		HRESULT hr = pInstance->pTaskThreadStart();
		return SUCCEEDED(hr) ? 0 : hr;
	}

private:

	// Copy constructor is prohibited.
	CNdasDevice(const CNdasDevice &);
	// Assignment operation is prohibited.
	CNdasDevice& operator = (const CNdasDevice&);
};
