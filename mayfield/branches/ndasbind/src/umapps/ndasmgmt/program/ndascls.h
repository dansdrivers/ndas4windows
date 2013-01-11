#pragma once

#include <ndas/ndasuser.h>
#include "ndas/ndasmsg.h"
#include <vector>

namespace ndas {

	struct IExtensibleObject
	{
		virtual void AddRef(void) = 0;
		virtual void Release(void) = 0;
	};

	template <typename T>
	class CExtensibleObject :
		public IExtensibleObject
	{
		LONG m_cRef;
	protected:
		CExtensibleObject() : m_cRef(0) {}

	public:
		// virtual ~CExtensibleObject() {}

		virtual void AddRef(void) 
		{ 
			::InterlockedIncrement(&m_cRef); 
		}

		virtual void Release(void) 
		{
			::InterlockedDecrement(&m_cRef);
			if (0 == m_cRef) {
				T* pThis = reinterpret_cast<T*>(this);
				delete pThis;
			}
		}
	};

	class CUpdatableObject
	{
		DWORD m_dwMinInterval;
		DWORD m_dwLastUpdated;

	protected:

		CUpdatableObject(DWORD dwMinInterval = 1500) : 
			m_dwMinInterval(dwMinInterval),
			m_dwLastUpdated(0)
		{}

		VOID OnUpdate() 
		{ m_dwLastUpdated = ::GetTickCount(); }

		BOOL IsExpired() 
		{ return ((::GetTickCount() - m_dwLastUpdated) > m_dwMinInterval); }

	};

	class Device;
	class UnitDevice;
	class LogicalDevice;

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////
	
	class Device : 
		public CExtensibleObject<Device>,
		public CUpdatableObject
	{
		typedef std::vector<UnitDevice*> UnitDeviceVector;

		const DWORD m_slot;
		NDAS_DEVICE_STATUS m_status;
		NDAS_DEVICE_ERROR m_lastError;

		TCHAR m_id[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR m_name[MAX_NDAS_DEVICE_NAME_LEN + 1];
		ACCESS_MASK m_grantedAccess;
		UnitDeviceVector m_unitDevices;

		NDAS_DEVICE_HW_INFORMATION* m_pHWInfo;

		DWORD m_lastUpdateTick;

	public:

		Device(
			DWORD slot, 
			LPCTSTR szId,
			LPCTSTR szName, 
			ACCESS_MASK grantedAccess);

		virtual ~Device();

		DWORD GetSlotNo();
		ACCESS_MASK GetGrantedAccess();
		BOOL SetAsReadOnly();
		BOOL SetAsReadWrite(LPCTSTR szKey);
		LPCTSTR GetName();
		BOOL SetName(LPCTSTR szName);
		NDAS_DEVICE_STATUS GetStatus();
		NDAS_DEVICE_ERROR GetLastError();
		LPCTSTR GetStringId();

		const NDAS_DEVICE_HW_INFORMATION* GetHwInfo();

		void Clear();

		BOOL Enable(BOOL bEnable = TRUE);
		BOOL UpdateInfo();
		BOOL UpdateStatus();

		UnitDevice* FindUnitDevice(DWORD unitNo);
		UnitDevice* GetUnitDevice(DWORD index);
		DWORD GetUnitDeviceCount();

		BOOL IsAnyUnitDeviceMounted();

	protected:

		static BOOL CALLBACK 
		UnitDeviceEnumProc(
			PNDASUSER_UNITDEVICE_ENUM_ENTRY lpEntry, 
			LPVOID lpContext);
	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Unit Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class UnitDevice :
		public CExtensibleObject<UnitDevice>,
		public CUpdatableObject
	{
		Device* const m_pDevice;
		const DWORD m_unitNo;
		NDAS_UNITDEVICE_STATUS m_status;
		NDAS_UNITDEVICE_ERROR m_lastError;
		NDAS_UNITDEVICE_TYPE m_type;
		NDAS_UNITDEVICE_SUBTYPE m_subtype;
		NDAS_UNITDEVICE_HW_INFORMATION* m_pHWInfo;

		DWORD m_nROHosts;
		DWORD m_nRWHosts;

		BOOL m_bStatusUpdated;
		BOOL m_bInfoUpdated;

		DWORD m_lastUpdateTick;

		typedef struct _UNITDEVICE_HOST_ENTRY {
			GUID HostGuid;
			ACCESS_MASK Access;
			NDAS_HOST_INFO HostInfo;
		} UNITDEVICE_HOST_ENTRY, *PUNITDEVICE_HOST_ENTRY;

		typedef std::vector<UNITDEVICE_HOST_ENTRY> HostInfoEntryVector;
		HostInfoEntryVector m_hostInfoEntries;

		BOOL QueryHostEnumProc(
			LPCGUID lpHostGuid, 
			ACCESS_MASK access);

		static BOOL spQueryHostEnumProc(
			LPCGUID lpHostGuid,
			ACCESS_MASK access,
			LPVOID lpInstance)
		{
			UnitDevice* pThis = reinterpret_cast<UnitDevice*>(lpInstance);
			return pThis->QueryHostEnumProc(lpHostGuid, access);
		}

	public:
		UnitDevice(
			Device* const pDevice,
			DWORD unitNo, 
			NDAS_UNITDEVICE_TYPE type);

		virtual ~UnitDevice();

		Device* GetParentDevice();
		DWORD GetSlotNo();
		DWORD GetUnitNo();
		CONST NDAS_UNITDEVICE_HW_INFORMATION* GetHWInfo();
		NDAS_UNITDEVICE_TYPE GetType();
		NDAS_UNITDEVICE_SUBTYPE GetSubType();
		NDAS_UNITDEVICE_STATUS GetStatus();

		NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

		BOOL UpdateHostInfo();
		DWORD GetHostInfoCount();
		CONST NDAS_HOST_INFO* GetHostInfo(
			DWORD dwIndex, 
			ACCESS_MASK* lpAccess = NULL,
			LPGUID lpHostGuid = NULL);

		DWORD GetROHostCount() { return m_nROHosts; }
		DWORD GetRWHostCount() { return m_nRWHosts; }

		BOOL UpdateInfo();
		BOOL UpdateStatus();
		BOOL UpdateHostStats();
	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Device Collection Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class DeviceColl :
		public CUpdatableObject
	{
		typedef std::vector<Device*> DeviceVector;

		DeviceVector m_devices;

	public:

		virtual ~DeviceColl();

		void Clear();
		BOOL Update();

		Device* FindDevice(DWORD slot);
		Device* FindDevice(LPCTSTR szStringId);
		Device* GetDevice(DWORD index);
		DWORD GetDeviceCount();

		BOOL Register();
		BOOL Unregister(DWORD slot);

	protected:
		static BOOL CALLBACK 
		EnumProc(
			PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, 
			LPVOID lpContext);

	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Logical Device Collection Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class LogicalDeviceColl :
		public CUpdatableObject
	{
		typedef std::vector<LogicalDevice*> LogicalDeviceVector;
		LogicalDeviceVector m_logicalDevices;

		static BOOL CALLBACK EnumProc(
			PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry, 
			LPVOID lpContext);

		DWORD m_lastUpdateTick;

	public:
		LogicalDeviceColl();
		virtual ~LogicalDeviceColl();

		LogicalDevice* FindLogicalDevice(
			NDAS_LOGICALDEVICE_ID logicalDeviceId);

		LogicalDevice* GetLogicalDevice(DWORD dwIndex);
		DWORD GetLogicalDeviceCount();

		VOID Clear(void);
		BOOL Update(void);

	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Logical Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class LogicalDevice :
		public CExtensibleObject<LogicalDevice>,
		public CUpdatableObject
	{
	public:

		typedef struct _UNITDEVICE_INFO {
			DWORD Index;
			TCHAR DeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
			DWORD UnitNo;
			DWORD Blocks;
		} UNITDEVICE_INFO, *PUNITDEVICE_INFO;

		DWORD m_lastUpdateTick;

	protected:

		CONST NDAS_LOGICALDEVICE_ID m_id;
		NDAS_LOGICALDEVICE_STATUS m_status;
		NDAS_LOGICALDEVICE_ERROR m_lastError;
		NDAS_LOGICALDEVICE_TYPE m_type;
		ACCESS_MASK m_grantedAccess;
		ACCESS_MASK m_mountedAccess;

		typedef std::vector<UNITDEVICE_INFO> UnitDeviceInfoVector;
		UnitDeviceInfoVector m_unitDevices;

		static BOOL CALLBACK MemberEnumProc(
			PNDASUSER_LOGICALDEVICE_MEMBER_ENTRY lpEntry, 
			LPVOID lpContext);

	public:

		LogicalDevice(
			NDAS_LOGICALDEVICE_ID logicalDeviceId,
			NDAS_LOGICALDEVICE_TYPE type);

		NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

		ACCESS_MASK GetGrantedAccess();
		ACCESS_MASK GetMountedAccess();

		NDAS_LOGICALDEVICE_STATUS GetStatus();
		NDAS_LOGICALDEVICE_ERROR GetLastError();

		NDAS_LOGICALDEVICE_TYPE GetType();

		const UNITDEVICE_INFO& GetUnitDeviceInfo(DWORD dwIndex);
		DWORD GetUnitDeviceInfoCount();

		BOOL UpdateInfo();
		BOOL UpdateStatus();

		BOOL PlugIn(BOOL bReadWrite = false);
		BOOL Eject();
		BOOL Unplug();

	};

}
