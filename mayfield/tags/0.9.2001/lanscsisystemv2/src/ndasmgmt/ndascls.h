#pragma once

#include "ndasuser.h"
#include "ndaserror.h"
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

	class Device;
	class UnitDevice;
	class LogicalDevice;

	///////////////////////////////////////////////////////////////////////
	//
	// API Exception Class
	//
	///////////////////////////////////////////////////////////////////////
#define NO_NDAS_API_EXCEPTION
#ifndef NO_NDAS_API_EXCEPTION
	class APIException
	{
		LPTSTR m_szMessage;
		const DWORD m_dwErrorCode;
	public:
		APIException(DWORD dwErrorCode = ::GetLastError());
		virtual ~APIException();

		DWORD GetErrorCode();
		bool IsSystemError();
		LPCTSTR ToString();
	};
#endif

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////
	
	class Device : 
		public CExtensibleObject<Device>
	{
		typedef std::vector<UnitDevice*> UnitDeviceVector;

		const DWORD m_slot;
		NDAS_DEVICE_STATUS m_status;
		NDAS_DEVICE_ERROR m_lastError;

		TCHAR m_id[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR m_name[MAX_NDAS_DEVICE_NAME_LEN + 1];
		ACCESS_MASK m_grantedAccess;
		UnitDeviceVector m_unitDevices;

		NDAS_DEVICE_HW_INFORMATION* m_hwinfo;

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
		LPCTSTR GetStringId();

		const NDAS_DEVICE_HW_INFORMATION& GetHwInfo();

		void Clear();

		BOOL Enable(bool bEnable = true);
		BOOL UpdateInfo();
		BOOL UpdateStatus();

		UnitDevice* FindUnitDevice(DWORD unitNo);
		UnitDevice* GetUnitDevice(DWORD index);
		DWORD GetUnitDeviceCount();

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
		public CExtensibleObject<UnitDevice>
	{
		Device* const m_pDevice;
		const DWORD m_unitNo;
		NDAS_UNITDEVICE_STATUS m_status;
		NDAS_UNITDEVICE_ERROR m_lastError;
		NDAS_UNITDEVICE_TYPE m_type;
		NDAS_UNITDEVICE_SUBTYPE m_subtype;
		NDAS_UNITDEVICE_HW_INFORMATION* m_hwinfo;
		NDAS_UNITDEVICE_USAGE_STATS m_stats;

		bool m_bStatusUpdated;
		bool m_bInfoUpdated;

	public:
		UnitDevice(
			Device* const pDevice,
			DWORD unitNo, 
			NDAS_UNITDEVICE_TYPE type);

		virtual ~UnitDevice();

		Device* GetParentDevice();
		DWORD GetSlotNo();
		DWORD GetUnitNo();
		const NDAS_UNITDEVICE_HW_INFORMATION& GetHWInfo();
		const NDAS_UNITDEVICE_USAGE_STATS& GetUsage();
		NDAS_UNITDEVICE_TYPE GetType();
		NDAS_UNITDEVICE_SUBTYPE GetSubType();
		NDAS_UNITDEVICE_STATUS GetStatus();

		NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

		BOOL UpdateInfo();
		BOOL UpdateStatus();
	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Device Collection Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class DeviceColl
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

	class LogicalDeviceColl
	{
		typedef std::vector<LogicalDevice*> LogicalDeviceVector;
		LogicalDeviceVector m_logicalDevices;

		static BOOL CALLBACK EnumProc(
			PNDASUSER_LOGICALDEVICE_ENUM_ENTRY lpEntry, 
			LPVOID lpContext);

	public:
		LogicalDeviceColl();
		virtual ~LogicalDeviceColl();

		LogicalDevice* FindLogicalDevice(
			const NDAS_LOGICALDEVICE_ID& logicalDeviceId);

		LogicalDevice* GetLogicalDevice(DWORD dwIndex);
		DWORD GetLogicalDeviceCount();

		void Clear(void);
		BOOL Update(void);

	};

	///////////////////////////////////////////////////////////////////////
	//
	// NDAS Logical Device Wrapper Class
	//
	///////////////////////////////////////////////////////////////////////

	class LogicalDevice :
		public CExtensibleObject<LogicalDevice>
	{
	public:

		typedef struct _UNITDEVICE_INFO {
			DWORD Index;
			TCHAR  DeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
			DWORD UnitNo;
			DWORD Blocks;
		} UNITDEVICE_INFO, *PUNITDEVICE_INFO;

	protected:

		const NDAS_LOGICALDEVICE_ID m_id;
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
			const NDAS_LOGICALDEVICE_ID& logicalDeviceId,
			NDAS_LOGICALDEVICE_TYPE type);

		NDAS_LOGICALDEVICE_ID GetLogicalDeviceId();

		DWORD GetSlotNo();
		DWORD GetTargetId();
		DWORD GetLUN();

		ACCESS_MASK GetGrantedAccess();
		ACCESS_MASK GetMountedAccess();

		NDAS_LOGICALDEVICE_STATUS GetStatus();
		NDAS_LOGICALDEVICE_ERROR GetLastError();

		NDAS_LOGICALDEVICE_TYPE GetType();

		const UNITDEVICE_INFO& GetUnitDeviceInfo(DWORD dwIndex);
		DWORD GetUnitDeviceInfoCount();

		BOOL UpdateInfo();
		BOOL UpdateStatus();

		BOOL PlugIn(bool bReadWrite = false);
		BOOL Eject();
		BOOL Unplug();

	};

}
